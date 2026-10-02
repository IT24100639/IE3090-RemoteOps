#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "remoteops_config.h"

int main(void)
{
    /* Create an IPv4 TCP socket. */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    /* Allow the server to restart using the same port. */
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

    /* Assign the personalised port to this socket. */
    if (bind(server_fd, (struct sockaddr *)&address,
             sizeof(address)) == -1) {
        perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    /* Start listening for incoming connections. */
    if (listen(server_fd, 16) == -1) {
        perror("listen");
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
            close(server_fd);
            return EXIT_FAILURE;
        }

        char peer_ip[INET_ADDRSTRLEN] = "unknown";
        inet_ntop(AF_INET, &peer.sin_addr,
                  peer_ip, sizeof(peer_ip));

        printf("Connection from %s:%u\n",
               peer_ip, (unsigned)ntohs(peer.sin_port));
        fflush(stdout);

        /* Initial stage: close after confirming connection. */
        close(client_fd);
    }
}
