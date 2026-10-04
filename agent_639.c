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
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#include "remoteops_config.h"
#include "remoteops_io.h"
#include "remoteops_files.h"
#include "remoteops_monitor.h"
#include "remoteops_log.h"

/* Every TCP response ends with the personalised SID and newline. */
static int reply(int fd, const char *message)
{
    char response[MAX_LINE];
    int length = snprintf(response, sizeof(response),
                          "%s %s\n", message, SID_TAG);

    if (length < 0 || (size_t)length >= sizeof(response)) {
        log_event(fd, "ERROR", "response exceeded buffer capacity");
        return -1;
    }

    int result = send_all(fd, response, (size_t)length);
    log_event(fd, "RESPONSE", "send=%s message=%s",
              result == 0 ? "ok" : "failed", message);
    return result;
}

static int send_sysinfo(int fd)
{
    struct sysinfo information;

    if (sysinfo(&information) == -1) {
        return reply(fd, "ERR 007 SYSINFO_FAILED");
    }

    double load = (double)information.loads[0] / 65536.0;
    double memory =
        ((double)information.totalram - (double)information.freeram) *
        (double)information.mem_unit / (1024.0 * 1024.0);

    char message[256];
    snprintf(message, sizeof(message),
             "OK SYSINFO %.2f %.2f %ld",
             load, memory, information.uptime);

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
    unsigned count = 0;
    struct dirent *entry;

    while (count < 20 && (entry = readdir(directory)) != NULL) {
        if (!is_pid_directory(entry->d_name)) {
            continue;
        }

        char path[128];
        snprintf(path, sizeof(path),
                 "/proc/%.20s/comm", entry->d_name);

        FILE *file = fopen(path, "r");
        if (file == NULL) {
            continue;
        }

        char name[64];
        if (fgets(name, sizeof(name), file) == NULL) {
            fclose(file);
            continue;
        }
        fclose(file);

        name[strcspn(name, "\r\n")] = '\0';

        for (size_t i = 0; name[i] != '\0'; ++i) {
            unsigned char character = (unsigned char)name[i];
            if (!isalnum(character) &&
                character != '_' &&
                character != '-' &&
                character != '.') {
                name[i] = '_';
            }
        }

        char item[128];
        int length = snprintf(item, sizeof(item),
                              "%s%s/%.20s",
                              count == 0 ? "" : ",",
                              name, entry->d_name);

        if (length < 0 ||
            (size_t)length >= sizeof(item) ||
            used + (size_t)length >= sizeof(message)) {
            break;
        }

        memcpy(message + used, item, (size_t)length + 1);
        used += (size_t)length;
        ++count;
    }

    closedir(directory);

    if (count == 0) {
        return reply(fd, "ERR 008 LISTPROC_FAILED");
    }
    return reply(fd, message);
}

/* Exact allow-list: no shell or arbitrary command execution. */
static int send_exec_result(int fd, const char *command)
{
    char output[512];
    int length = -1;

    if (strcmp(command, "DATE") == 0) {
        time_t now = time(NULL);
        struct tm local_time;

        if (now != (time_t)-1 &&
            localtime_r(&now, &local_time) != NULL) {
            size_t result = strftime(output, sizeof(output),
                                     "%Y-%m-%d %H:%M:%S %z",
                                     &local_time);
            if (result > 0) {
                length = (int)result;
            }
        }
    } else if (strcmp(command, "UPTIME") == 0) {
        struct sysinfo information;
        if (sysinfo(&information) == 0) {
            length = snprintf(output, sizeof(output),
                              "%ld seconds", information.uptime);
        }
    } else if (strcmp(command, "DISKFREE") == 0) {
        struct statvfs information;
        if (statvfs(".", &information) == 0) {
            double available =
                (double)information.f_bavail *
                (double)information.f_frsize /
                (1024.0 * 1024.0);

            length = snprintf(output, sizeof(output),
                              "%.2f MB available on project filesystem",
                              available);
        }
    } else if (strcmp(command, "HOSTNAME") == 0) {
        struct utsname information;
        if (uname(&information) == 0) {
            length = snprintf(output, sizeof(output),
                              "%s", information.nodename);
        }
    } else if (strcmp(command, "WHOAMI") == 0) {
        struct passwd user;
        struct passwd *result = NULL;
        char buffer[16384];

        int error = getpwuid_r(geteuid(), &user,
                              buffer, sizeof(buffer), &result);
        if (error == 0 && result != NULL) {
            length = snprintf(output, sizeof(output),
                              "%s", result->pw_name);
        }
    } else {
        return reply(fd, "ERR 002 COMMAND_NOT_ALLOWED");
    }

    if (length < 0 || (size_t)length >= sizeof(output)) {
        return reply(fd, "ERR 009 EXEC_FAILED");
    }

    for (size_t i = 0; output[i] != '\0'; ++i) {
        unsigned char character = (unsigned char)output[i];
        if (character < 32 || character == 127) {
            output[i] = ' ';
        }
    }

    char message[640];
    int result = snprintf(message, sizeof(message),
                          "OK EXEC_RESULT %s", output);

    if (result < 0 || (size_t)result >= sizeof(message)) {
        return reply(fd, "ERR 009 EXEC_FAILED");
    }
    return reply(fd, message);
}

/* Each client has independent authentication and monitoring state. */
static void *handle_client(void *argument)
{
    int fd = *(int *)argument;
    free(argument);

    struct monitor_state monitor;
    int error = monitor_init(&monitor);

    if (error != 0) {
        log_event(fd, "ERROR", "monitor initialisation failed: %s",
                  strerror(error));
        reply(fd, "ERR 012 MONITOR_FAILED");
        log_event(fd, "DISCONNECT", "initialisation failed");
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
            log_event(fd, "READ_END", "recv_line result=%d", result);
            break;
        }

        /* Never write the authentication token to the log. */
        if (strncmp(line, "AUTH ", 5) == 0 ||
            strcmp(line, "AUTH") == 0) {
            log_event(fd, "COMMAND", "AUTH [redacted]");
        } else {
            log_event(fd, "COMMAND", "%s", line);
        }

        const char *response;

        if (strncmp(line, "AUTH ", 5) == 0) {
            if (strcmp(line + 5, AUTH_TOKEN) == 0) {
                authenticated = 1;
                response = "OK AUTHENTICATED";
                log_event(fd, "AUTH", "result=success");
            } else {
                authenticated = 0;
                monitor_stop(&monitor);
                response = "ERR 001 AUTH_FAILED";
                log_event(fd, "AUTH", "result=failed monitoring=stopped");
            }
        } else if (!authenticated) {
            if (reply(fd, "ERR 003 AUTH_REQUIRED") == -1) {
                break;
            }

            /* A PUT payload cannot be treated as subsequent commands. */
            if (strncmp(line, "PUT ", 4) == 0) {
                log_event(fd, "PUT_REJECTED",
                          "authentication required; closing connection");
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
            log_event(fd, "PUT_BEGIN", "request=%s", line + 4);
            result = receive_file(fd, line + 4);
            log_event(fd, "PUT_HANDLER_END",
                      "request=%s result=%d", line + 4, result);
            if (result == -1) {
                break;
            }
            continue;
        } else if (strncmp(line, "GET ", 4) == 0) {
            log_event(fd, "GET_BEGIN", "filename=%s", line + 4);
            result = send_file(fd, line + 4);
            log_event(fd, "GET_HANDLER_END",
                      "filename=%s result=%d", line + 4, result);
            if (result == -1) {
                break;
            }
            continue;
        } else if (strncmp(line, "MONITOR START ", 14) == 0) {
            unsigned short port;

            if (parse_udp_port(line + 14, &port) != 0) {
                response = "ERR 013 INVALID_UDP_PORT";
            } else if (monitor_start(&monitor, fd, port) != 0) {
                response = "ERR 012 MONITOR_FAILED";
                log_event(fd, "MONITOR", "start failed port=%u",
                          (unsigned)port);
            } else {
                response = "OK MONITOR_STARTED";
                log_event(fd, "MONITOR",
                          "started port=%u interval_seconds=%d",
                          (unsigned)port, MONITOR_INTERVAL_SEC);
            }
        } else if (strcmp(line, "MONITOR STOP") == 0) {
            monitor_stop(&monitor);
            response = "OK MONITOR_STOPPED";
            log_event(fd, "MONITOR", "stopped by command");
        } else if (strcmp(line, "QUIT") == 0) {
            monitor_stop(&monitor);
            reply(fd, "OK BYE");
            break;
        } else {
            response = "ERR 002 COMMAND_NOT_ALLOWED";
        }

        if (reply(fd, response) == -1) {
            break;
        }
    }

    monitor_destroy(&monitor);
    log_event(fd, "DISCONNECT", "session ended; monitoring stopped");
    close(fd);
    return NULL;
}

int main(void)
{
    struct stat storage;
    if (stat(STORAGE_DIR, &storage) == -1 ||
        !S_ISDIR(storage.st_mode)) {
        fprintf(stderr, "Create storage directory first: %s\n",
                STORAGE_DIR);
        return EXIT_FAILURE;
    }

    if (log_check() == -1) {
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

    log_event(-1, "SERVER_START", "registration=%s tcp_port=%d",
              REG_NUMBER, DEFAULT_PORT);

    printf("RemoteOps Agent %s listening on TCP port %d\n",
           REG_NUMBER, DEFAULT_PORT);
    printf("Log file: %s\n", LOG_FILE);
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
            log_event(-1, "ERROR", "accept failed: %s",
                      strerror(errno));
            break;
        }

        char peer_ip[INET_ADDRSTRLEN] = "unknown";
        inet_ntop(AF_INET, &peer.sin_addr,
                  peer_ip, sizeof(peer_ip));

        printf("Connection from %s:%u\n",
               peer_ip, (unsigned)ntohs(peer.sin_port));
        fflush(stdout);

        log_event(client_fd, "CONNECT", "TCP connection accepted");

        int *argument = malloc(sizeof(*argument));
        if (argument == NULL) {
            perror("malloc");
            log_event(client_fd, "ERROR", "client allocation failed");
            close(client_fd);
            continue;
        }
        *argument = client_fd;

        pthread_t thread;
        error = pthread_create(&thread, &attributes,
                               handle_client, argument);
        if (error != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(error));
            log_event(client_fd, "ERROR",
                      "client thread creation failed: %s",
                      strerror(error));
            free(argument);
            close(client_fd);
        }
    }

    log_event(-1, "SERVER_END", "accept loop ended");
    pthread_attr_destroy(&attributes);
    close(server_fd);
    return EXIT_FAILURE;
}
