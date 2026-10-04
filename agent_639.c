#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#include "remoteops_config.h"
#include "remoteops_io.h"
#include "remoteops_files.h"
#include "remoteops_monitor.h"

/* Every TCP response ends with the personalised SID and newline. */
static int reply(int fd, const char *message)
{
    char response[MAX_LINE];
    int length = snprintf(response, sizeof(response),
                          "%s %s\n", message, SID_TAG);

    if (length < 0 || (size_t)length >= sizeof(response)) {
        return -1;
    }

    return send_all(fd, response, (size_t)length);
}

static int send_sysinfo(int fd)
{
    struct sysinfo information;

    if (sysinfo(&information) == -1) {
        return reply(fd, "ERR 007 SYSINFO_FAILED");
    }

    double cpu_load = (double)information.loads[0] / 65536.0;
    double memory_used =
        ((double)information.totalram -
         (double)information.freeram) *
        (double)information.mem_unit / (1024.0 * 1024.0);

    char message[256];
    int length = snprintf(message, sizeof(message),
                          "OK SYSINFO %.2f %.2f %ld",
                          cpu_load, memory_used,
                          information.uptime);

    if (length < 0 || (size_t)length >= sizeof(message)) {
        return reply(fd, "ERR 007 SYSINFO_FAILED");
    }

    return reply(fd, message);
}

static int is_pid_directory(const char *name)
{
    size_t length = strlen(name);

    if (length == 0 || length > 20) {
        return 0;
    }

    for (size_t i = 0; i < length; ++i) {
        if (!isdigit((unsigned char)name[i])) {
            return 0;
        }
    }

    return 1;
}

static int send_process_list(int fd)
{
    DIR *directory = opendir("/proc");

    if (directory == NULL) {
        return reply(fd, "ERR 008 LISTPROC_FAILED");
    }

    char message[MAX_LINE - 64] = "OK PROCS ";
    size_t used = strlen(message);
    int count = 0;
    struct dirent *entry;

    while (count < 20 && (entry = readdir(directory)) != NULL) {
        if (!is_pid_directory(entry->d_name)) {
            continue;
        }

        char path[128];
        int length = snprintf(path, sizeof(path),
                              "/proc/%.20s/comm", entry->d_name);

        if (length < 0 || (size_t)length >= sizeof(path)) {
            continue;
        }

        FILE *file = fopen(path, "r");

        if (file == NULL) {
            continue;
        }

        char name[64];
        char *result = fgets(name, sizeof(name), file);
        fclose(file);

        if (result == NULL) {
            continue;
        }

        name[strcspn(name, "\r\n")] = '\0';

        if (name[0] == '\0') {
            continue;
        }

        for (size_t i = 0; name[i] != '\0'; ++i) {
            unsigned char c = (unsigned char)name[i];

            if (!(isalnum(c) || c == '_' || c == '-' || c == '.')) {
                name[i] = '_';
            }
        }

        char item[128];
        length = snprintf(item, sizeof(item),
                          "%s%s/%.20s",
                          count == 0 ? "" : ",",
                          name, entry->d_name);

        if (length < 0 || (size_t)length >= sizeof(item)) {
            continue;
        }

        if ((size_t)length >= sizeof(message) - used) {
            break;
        }

        memcpy(message + used, item, (size_t)length);
        used += (size_t)length;
        message[used] = '\0';
        ++count;
    }

    closedir(directory);

    if (count == 0) {
        return reply(fd, "ERR 008 LISTPROC_FAILED");
    }

    return reply(fd, message);
}

/* Exact allowlist, implemented with system/library calls. */
static int send_exec_result(int fd, const char *command)
{
    char output[512];
    int length = -1;

    if (strcmp(command, "DATE") == 0) {
        time_t now = time(NULL);
        struct tm local_time;

        if (now == (time_t)-1 ||
            localtime_r(&now, &local_time) == NULL) {
            return reply(fd, "ERR 009 EXEC_FAILED");
        }

        size_t written = strftime(output, sizeof(output),
                                  "%Y-%m-%d %H:%M:%S %z",
                                  &local_time);

        if (written == 0) {
            return reply(fd, "ERR 009 EXEC_FAILED");
        }

        length = (int)written;
    } else if (strcmp(command, "UPTIME") == 0) {
        struct sysinfo information;

        if (sysinfo(&information) == -1) {
            return reply(fd, "ERR 009 EXEC_FAILED");
        }

        length = snprintf(output, sizeof(output),
                          "%ld seconds", information.uptime);
    } else if (strcmp(command, "DISKFREE") == 0) {
        struct statvfs information;

        if (statvfs(".", &information) == -1) {
            return reply(fd, "ERR 009 EXEC_FAILED");
        }

        double available_mb =
            (double)information.f_bavail *
            (double)information.f_frsize / (1024.0 * 1024.0);

        length = snprintf(output, sizeof(output),
                          "%.2f MB available on project filesystem",
                          available_mb);
    } else if (strcmp(command, "HOSTNAME") == 0) {
        struct utsname information;

        if (uname(&information) == -1) {
            return reply(fd, "ERR 009 EXEC_FAILED");
        }

        length = snprintf(output, sizeof(output),
                          "%s", information.nodename);
    } else if (strcmp(command, "WHOAMI") == 0) {
        struct passwd user;
        struct passwd *result = NULL;
        char user_buffer[16384];

        int error = getpwuid_r(geteuid(), &user,
                              user_buffer, sizeof(user_buffer),
                              &result);

        if (error != 0 || result == NULL) {
            return reply(fd, "ERR 009 EXEC_FAILED");
        }

        length = snprintf(output, sizeof(output),
                          "%s", result->pw_name);
    } else {
        return reply(fd, "ERR 002 COMMAND_NOT_ALLOWED");
    }

    if (length < 0 || (size_t)length >= sizeof(output)) {
        return reply(fd, "ERR 009 EXEC_FAILED");
    }

    for (size_t i = 0; output[i] != '\0'; ++i) {
        unsigned char c = (unsigned char)output[i];

        if (c < 32 || c == 127) {
            output[i] = ' ';
        }
    }

    char message[640];
    length = snprintf(message, sizeof(message),
                      "OK EXEC_RESULT %s", output);

    if (length < 0 || (size_t)length >= sizeof(message)) {
        return reply(fd, "ERR 009 EXEC_FAILED");
    }

    return reply(fd, message);
}

/* Each client has its own authentication and monitoring state. */
static void *handle_client(void *argument)
{
    int fd = *(int *)argument;
    free(argument);

    struct monitor_state monitoring;
    int error = monitor_init(&monitoring);

    if (error != 0) {
        reply(fd, "ERR 012 MONITOR_FAILED");
        close(fd);
        return NULL;
    }

    int authenticated = 0;
    char line[MAX_LINE];

    for (;;) {
        int result = recv_line(fd, line, sizeof(line));

        if (result != 1) {
            if (result == -2) {
                reply(fd, "ERR 006 LINE_TOO_LONG");
            }
            break;
        }

        const char *response;

        if (strncmp(line, "AUTH ", 5) == 0) {
            if (strcmp(line + 5, AUTH_TOKEN) == 0) {
                authenticated = 1;
                response = "OK AUTHENTICATED";
            } else {
                authenticated = 0;
                monitor_stop(&monitoring);
                response = "ERR 001 AUTH_FAILED";
            }
        } else if (!authenticated) {
            if (reply(fd, "ERR 003 AUTH_REQUIRED") == -1) {
                break;
            }

            if (strncmp(line, "PUT ", 4) == 0) {
                break;
            }

            continue;
        } else if (strcmp(line, "SYSINFO") == 0) {
            if (send_sysinfo(fd) == -1) {
                break;
            }
            continue;
        } else if (strcmp(line, "LISTPROC") == 0) {
            if (send_process_list(fd) == -1) {
                break;
            }
            continue;
        } else if (strncmp(line, "EXEC ", 5) == 0) {
            if (send_exec_result(fd, line + 5) == -1) {
                break;
            }
            continue;
        } else if (strncmp(line, "PUT ", 4) == 0) {
            if (receive_file(fd, line + 4) == -1) {
                break;
            }
            continue;
        } else if (strncmp(line, "GET ", 4) == 0) {
            if (send_file(fd, line + 4) == -1) {
                break;
            }
            continue;
        } else if (strncmp(line, "MONITOR START ", 14) == 0) {
            unsigned short udp_port;

            if (parse_udp_port(line + 14, &udp_port) == -1) {
                response = "ERR 013 INVALID_UDP_PORT";
            } else if (monitor_start(&monitoring,
                                     fd, udp_port) == -1) {
                response = "ERR 012 MONITOR_FAILED";
            } else {
                response = "OK MONITOR_STARTED";
            }
        } else if (strcmp(line, "MONITOR STOP") == 0) {
            monitor_stop(&monitoring);
            response = "OK MONITOR_STOPPED";
        } else if (strcmp(line, "QUIT") == 0) {
            monitor_stop(&monitoring);
            reply(fd, "OK BYE");
            break;
        } else {
            response = "ERR 002 COMMAND_NOT_ALLOWED";
        }

        if (reply(fd, response) == -1) {
            break;
        }
    }

    /* Also stops monitoring after an unexpected disconnect. */
    monitor_destroy(&monitoring);
    close(fd);
    return NULL;
}

int main(void)
{
    struct stat storage_information;

    if (stat(STORAGE_DIR, &storage_information) == -1 ||
        !S_ISDIR(storage_information.st_mode)) {
        fprintf(stderr, "Create storage directory first: %s\n",
                STORAGE_DIR);
        return EXIT_FAILURE;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int reuse = 1;

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &reuse, sizeof(reuse)) == -1) {
        perror("setsockopt");
        close(server_fd);
        return EXIT_FAILURE;
    }

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(DEFAULT_PORT);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *)&address,
             sizeof(address)) == -1) {
        perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 16) == -1) {
        perror("listen");
        close(server_fd);
        return EXIT_FAILURE;
    }

    pthread_attr_t attributes;
    int error = pthread_attr_init(&attributes);

    if (error != 0) {
        fprintf(stderr, "pthread_attr_init: %s\n", strerror(error));
        close(server_fd);
        return EXIT_FAILURE;
    }

    error = pthread_attr_setdetachstate(&attributes,
                                       PTHREAD_CREATE_DETACHED);

    if (error != 0) {
        fprintf(stderr, "pthread_attr_setdetachstate: %s\n",
                strerror(error));
        pthread_attr_destroy(&attributes);
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("RemoteOps Agent %s listening on TCP port %d\n",
           REG_NUMBER, DEFAULT_PORT);
    fflush(stdout);

    for (;;) {
        struct sockaddr_in peer = {0};
        socklen_t peer_length = sizeof(peer);

        int client_fd = accept(server_fd,
                               (struct sockaddr *)&peer,
                               &peer_length);

        if (client_fd == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            break;
        }

        char peer_ip[INET_ADDRSTRLEN] = "unknown";
        inet_ntop(AF_INET, &peer.sin_addr,
                  peer_ip, sizeof(peer_ip));

        printf("Connection from %s:%u\n",
               peer_ip, (unsigned)ntohs(peer.sin_port));
        fflush(stdout);

        int *argument = malloc(sizeof(*argument));

        if (argument == NULL) {
            perror("malloc");
            close(client_fd);
            continue;
        }

        *argument = client_fd;

        pthread_t thread;
        error = pthread_create(&thread, &attributes,
                               handle_client, argument);

        if (error != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(error));
            free(argument);
            close(client_fd);
        }
    }

    pthread_attr_destroy(&attributes);
    close(server_fd);
    return EXIT_FAILURE;
}
