#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "remoteops_config.h"
#include "remoteops_io.h"

int main(int argc, char *argv[])
{
    const char *agent_ip = "127.0.0.1";

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [agent_ipv4]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc == 2) {
        agent_ip = argv[1];
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(DEFAULT_PORT);

    if (inet_pton(AF_INET, agent_ip, &address.sin_addr) != 1) {
        fprintf(stderr, "Invalid IPv4 address\n");
        close(fd);
        return EXIT_FAILURE;
    }

    if (connect(fd, (struct sockaddr *)&address,
                sizeof(address)) == -1) {
        perror("connect");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Connected to Agent at %s:%d\n",
           agent_ip, DEFAULT_PORT);
    printf("Enter a command, then press Enter.\n");

    char command[MAX_LINE];
    char response[MAX_LINE];
    int status = EXIT_SUCCESS;

    for (;;) {
        printf("remoteops> ");
        fflush(stdout);

        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }

        size_t length = strlen(command);
        if (length == 0 || command[length - 1] != '\n') {
            int character;
            while ((character = getchar()) != '\n' &&
                   character != EOF) {
            }
            fprintf(stderr, "Command too long or missing newline\n");
            continue;
        }

        if (send_all(fd, command, length) == -1) {
            perror("send");
            status = EXIT_FAILURE;
            break;
        }

        if (recv_line(fd, response, sizeof(response)) != 1) {
            fprintf(stderr, "Connection closed or response failed\n");
            status = EXIT_FAILURE;
            break;
        }

        printf("%s\n", response);

        if (strcmp(response, "OK BYE " SID_TAG) == 0) {
            break;
        }
    }

    close(fd);
    return status;
}
