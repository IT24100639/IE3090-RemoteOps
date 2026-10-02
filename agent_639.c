#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include "remoteops_config.h"
#include "remoteops_io.h"

/* Every response ends with the personalised SID and newline. */
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

/* Return real Linux load average, used RAM and uptime. */
static int send_sysinfo(int fd)
{
    struct sysinfo information;

    if (sysinfo(&information) == -1) {
        return reply(fd, "ERR 007 SYSINFO_FAILED");
    }

    double cpu_load =
        (double)information.loads[0] / 65536.0;

    /* Used RAM includes memory used for caching. */
    double used_memory_mb =
        ((double)information.totalram -
         (double)information.freeram) *
        (double)information.mem_unit / (1024.0 * 1024.0);

    char message[256];
    int length = snprintf(message, sizeof(message),
                          "OK SYSINFO %.2f %.2f %ld",
                          cpu_load, used_memory_mb,
                          information.uptime);

    if (length < 0 || (size_t)length >= sizeof(message)) {
        return reply(fd, "ERR 007 SYSINFO_FAILED");
    }

    return reply(fd, message);
}

/* Numeric directory names in /proc represent process IDs. */
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

/* Return a bounded snapshot of up to 20 readable processes. */
static int send_process_list(int fd)
{
    DIR *directory = opendir("/proc");

    if (directory == NULL) {
        return reply(fd, "ERR 008 LISTPROC_FAILED");
    }

    /* Leave room for the SID and newline added by reply(). */
    char message[MAX_LINE - 64] = "OK PROCS ";
    size_t used = strlen(message);
    unsigned count = 0;
    struct dirent *entry;

    while (count < 20 &&
           (entry = readdir(directory)) != NULL) {
        if (!is_pid_directory(entry->d_name)) {
            continue;
        }

        char path[128];
        int path_length = snprintf(path, sizeof(path),
                                   "/proc/%.20s/comm",
                                   entry->d_name);

        if (path_length < 0 ||
            (size_t)path_length >= sizeof(path)) {
            continue;
        }

        FILE *process_file = fopen(path, "r");

        /* A process may exit before its file is opened. */
        if (process_file == NULL) {
            continue;
        }

        char name[64];
        char *result = fgets(name, sizeof(name), process_file);
        fclose(process_file);

        if (result == NULL) {
            continue;
        }

        name[strcspn(name, "\r\n")] = '\0';

        if (name[0] == '\0') {
            continue;
        }

        /* Keep names safe for a comma-separated protocol line. */
        for (size_t i = 0; name[i] != '\0'; ++i) {
            unsigned char character = (unsigned char)name[i];

            if (!(isalnum(character) ||
                  character == '_' ||
                  character == '-' ||
                  character == '.')) {
                name[i] = '_';
            }
        }

        char item[128];
        int item_length = snprintf(item, sizeof(item),
                                   "%s%s/%.20s",
                                   count == 0 ? "" : ",",
                                   name, entry->d_name);

        if (item_length < 0 ||
            (size_t)item_length >= sizeof(item)) {
            continue;
        }

        if ((size_t)item_length >= sizeof(message) - used) {
            break;
        }

        memcpy(message + used, item, (size_t)item_length);
        used += (size_t)item_length;
        message[used] = '\0';
        ++count;
    }

    closedir(directory);

    if (count == 0) {
        return reply(fd, "ERR 008 LISTPROC_FAILED");
    }

    return reply(fd, message);
}

/* Each client gets its own thread and authentication state. */
static void *handle_client(void *argument)
{
    int fd = *(int *)argument;
    free(argument);

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
                response = "ERR 001 AUTH_FAILED";
            }
        } else if (!authenticated) {
            response = "ERR 003 AUTH_REQUIRED";
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
        } else if (strcmp(line, "QUIT") == 0) {
            reply(fd, "OK BYE");
            break;
        } else {
            /* Remaining command handlers will be added later. */
            response = "ERR 002 COMMAND_NOT_ALLOWED";
        }

        if (reply(fd, response) == -1) {
            break;
        }
    }

    close(fd);
    return NULL;
}

int main(void)
{
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
        fprintf(stderr, "pthread_attr_init: %s\n",
                strerror(error));
        close(server_fd);
        return EXIT_FAILURE;
    }

    error = pthread_attr_setdetachstate(
        &attributes, PTHREAD_CREATE_DETACHED);

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
            fprintf(stderr, "pthread_create: %s\n",
                    strerror(error));
            free(argument);
            close(client_fd);
        }
    }

    pthread_attr_destroy(&attributes);
    close(server_fd);
    return EXIT_FAILURE;
}
