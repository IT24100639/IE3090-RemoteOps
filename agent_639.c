#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
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
        } else if (strcmp(line, "QUIT") == 0) {
            reply(fd, "OK BYE");
            break;
        } else {
            /* Other command handlers will be added next. */
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
