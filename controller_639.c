#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "remoteops_config.h"
#include "remoteops_io.h"
#include "remoteops_files.h"

/* Return 1 after sending, 0 for a local error, -1 for a fatal error. */
static int upload_file(int fd, const char *name)
{
    if (!valid_filename(name)) {
        fprintf(stderr, "Use a simple filename: PUT sample.txt\n");
        return 0;
    }

    FILE *file = fopen(name, "rb");

    if (file == NULL) {
        perror("Open upload file");
        return 0;
    }

    struct stat information;

    if (fstat(fileno(file), &information) == -1 ||
        !S_ISREG(information.st_mode) ||
        information.st_size < 0) {
        fprintf(stderr, "Upload must be a regular file\n");
        fclose(file);
        return 0;
    }

    unsigned long long size =
        (unsigned long long)information.st_size;

    if (size > MAX_FILE_SIZE) {
        fprintf(stderr, "Upload exceeds the 100 MiB limit\n");
        fclose(file);
        return 0;
    }

    char header[256];
    int length = snprintf(header, sizeof(header),
                          "PUT %s %llu\n", name, size);

    if (length < 0 || (size_t)length >= sizeof(header)) {
        fclose(file);
        return 0;
    }

    if (send_all(fd, header, (size_t)length) == -1) {
        perror("Send PUT header");
        fclose(file);
        return -1;
    }

    unsigned char buffer[FILE_CHUNK_SIZE];
    unsigned long long remaining = size;

    while (remaining > 0) {
        size_t chunk = remaining > sizeof(buffer)
                     ? sizeof(buffer) : (size_t)remaining;

        if (fread(buffer, 1, chunk, file) != chunk) {
            fprintf(stderr, "Could not read complete upload\n");
            fclose(file);
            return -1;
        }

        if (send_all(fd, buffer, chunk) == -1) {
            perror("Send file data");
            fclose(file);
            return -1;
        }

        remaining -= chunk;
    }

    fclose(file);
    return 1;
}

static int download_file(int fd, const char *response,
                         const char *requested_name)
{
    char name[129];
    char size_text[32];
    char sid[32];
    char extra;

    if (sscanf(response, "OK FILE_SEND %128s %31s %31s %c",
               name, size_text, sid, &extra) != 3 ||
        !valid_filename(name) ||
        strcmp(name, requested_name) != 0 ||
        strcmp(sid, SID_TAG) != 0) {
        fprintf(stderr, "Invalid file response header\n");
        return -1;
    }

    unsigned long long size;

    if (parse_file_size(size_text, &size) == -1 ||
        size > MAX_FILE_SIZE) {
        fprintf(stderr, "Invalid download size\n");
        return -1;
    }

    char destination[256];
    char temporary[] = "downloads/.download-XXXXXX";

    int length = snprintf(destination, sizeof(destination),
                          "downloads/%s", name);

    if (length < 0 || (size_t)length >= sizeof(destination)) {
        return -1;
    }

    int temporary_fd = mkstemp(temporary);

    if (temporary_fd == -1) {
        perror("Create download file");
        return -1;
    }

    FILE *file = fdopen(temporary_fd, "wb");

    if (file == NULL) {
        perror("Open download stream");
        close(temporary_fd);
        unlink(temporary);
        return -1;
    }

    unsigned char buffer[FILE_CHUNK_SIZE];
    unsigned long long remaining = size;

    while (remaining > 0) {
        size_t chunk = remaining > sizeof(buffer)
                     ? sizeof(buffer) : (size_t)remaining;

        if (recv_exact(fd, buffer, chunk) == -1) {
            fprintf(stderr, "Download interrupted\n");
            fclose(file);
            unlink(temporary);
            return -1;
        }

        if (fwrite(buffer, 1, chunk, file) != chunk) {
            fprintf(stderr, "Could not write download data\n");
            fclose(file);
            unlink(temporary);
            return -1;
        }

        remaining -= chunk;
    }

    if (fclose(file) != 0) {
        perror("Finish download");
        unlink(temporary);
        return -1;
    }

    if (rename(temporary, destination) == -1) {
        perror("Save download");
        unlink(temporary);
        return -1;
    }

    printf("Saved %llu bytes to %s\n", size, destination);
    return 0;
}

/* Bind before sending MONITOR START so the first datagram can arrive. */
static int bind_monitor_socket(unsigned short port)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (fd == -1) {
        perror("UDP socket");
        return -1;
    }

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)&address,
             sizeof(address)) == -1) {
        perror("Bind UDP port");
        close(fd);
        return -1;
    }

    return fd;
}

/* Display only valid SYSINFO datagrams from the Agent's IP. */
static void display_datagram(int fd, struct in_addr agent_address)
{
    char datagram[512];
    struct sockaddr_in sender = {0};
    socklen_t sender_length = sizeof(sender);

    ssize_t length = recvfrom(
        fd, datagram, sizeof(datagram) - 1, MSG_DONTWAIT,
        (struct sockaddr *)&sender, &sender_length);

    if (length <= 0 ||
        sender.sin_family != AF_INET ||
        sender.sin_addr.s_addr != agent_address.s_addr) {
        return;
    }

    datagram[length] = '\0';

    double cpu_load;
    double memory_used;
    long uptime;
    char sid[32];
    char extra;

    if (sscanf(datagram, "SYSINFO %lf %lf %ld %31s %c",
               &cpu_load, &memory_used, &uptime,
               sid, &extra) != 4 ||
        strncmp(datagram, "SYSINFO ", 8) != 0 ||
        strcmp(sid, SID_TAG) != 0) {
        return;
    }

    printf("\n[UDP] SYSINFO %.2f %.2f %ld %s\n",
           cpu_load, memory_used, uptime, sid);
    printf("remoteops> ");
    fflush(stdout);
}

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

    /* Avoid stdin read-ahead when polling for keyboard input. */
    setvbuf(stdin, NULL, _IONBF, 0);

    if (mkdir("downloads", 0700) == -1 && errno != EEXIST) {
        perror("Create downloads directory");
        return EXIT_FAILURE;
    }

    struct stat directory_information;

    if (stat("downloads", &directory_information) == -1 ||
        !S_ISDIR(directory_information.st_mode)) {
        fprintf(stderr, "downloads must be a directory\n");
        return EXIT_FAILURE;
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
    printf("Authenticate: AUTH %s\n", AUTH_TOKEN);
    printf("Upload: PUT filename\n");
    printf("Download: GET filename\n");
    printf("Monitoring: MONITOR START 9639\n");
    printf("Stop monitoring: MONITOR STOP\n");

    char command[MAX_LINE];
    char response[MAX_LINE];
    int authenticated = 0;
    int status = EXIT_SUCCESS;
    int udp_fd = -1;
    unsigned short current_udp_port = 0;

    printf("remoteops> ");
    fflush(stdout);

    for (;;) {
        struct pollfd watched[3] = {
            { .fd = STDIN_FILENO, .events = POLLIN },
            { .fd = udp_fd, .events = POLLIN },
            { .fd = fd, .events = POLLIN }
        };

        int ready = poll(watched, 3, -1);

        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("poll");
            status = EXIT_FAILURE;
            break;
        }

        if (watched[2].revents &
            (POLLIN | POLLHUP | POLLERR | POLLNVAL)) {
            fprintf(stderr, "\nAgent connection closed or unexpected data\n");
            status = EXIT_FAILURE;
            break;
        }

        if (udp_fd != -1 && (watched[1].revents & POLLIN)) {
            display_datagram(udp_fd, address.sin_addr);
        }

        if (!(watched[0].revents & (POLLIN | POLLHUP))) {
            continue;
        }

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
            printf("remoteops> ");
            fflush(stdout);
            continue;
        }

        command[--length] = '\0';

        if (length > 0 && command[length - 1] == '\r') {
            command[--length] = '\0';
        }

        int is_get = strncmp(command, "GET ", 4) == 0;
        int is_start = strncmp(command, "MONITOR START ", 14) == 0;
        int pending_udp_fd = -1;
        unsigned short requested_port = 0;

        if (is_start) {
            unsigned long long port_value;

            /* Invalid ports are sent to the Agent for its error reply. */
            if (parse_file_size(command + 14, &port_value) == 0 &&
                port_value >= 1 && port_value <= 65535) {
                requested_port = (unsigned short)port_value;

                if (udp_fd == -1 ||
                    requested_port != current_udp_port) {
                    pending_udp_fd =
                        bind_monitor_socket(requested_port);

                    if (pending_udp_fd == -1) {
                        printf("remoteops> ");
                        fflush(stdout);
                        continue;
                    }
                }
            }
        }

        if (strncmp(command, "PUT ", 4) == 0) {
            if (!authenticated) {
                fprintf(stderr,
                        "Authenticate before uploading: AUTH %s\n",
                        AUTH_TOKEN);
                printf("remoteops> ");
                fflush(stdout);
                continue;
            }

            int result = upload_file(fd, command + 4);

            if (result == 0) {
                printf("remoteops> ");
                fflush(stdout);
                continue;
            }

            if (result == -1) {
                status = EXIT_FAILURE;
                break;
            }
        } else {
            if (send_all(fd, command, length) == -1 ||
                send_all(fd, "\n", 1) == -1) {
                perror("send");

                if (pending_udp_fd != -1) {
                    close(pending_udp_fd);
                }

                status = EXIT_FAILURE;
                break;
            }
        }

        if (recv_line(fd, response, sizeof(response)) != 1) {
            fprintf(stderr, "Connection closed or response failed\n");

            if (pending_udp_fd != -1) {
                close(pending_udp_fd);
            }

            status = EXIT_FAILURE;
            break;
        }

        printf("%s\n", response);

        if (is_start &&
            strcmp(response, "OK MONITOR_STARTED " SID_TAG) == 0) {
            if (pending_udp_fd != -1) {
                if (udp_fd != -1) {
                    close(udp_fd);
                }

                udp_fd = pending_udp_fd;
                pending_udp_fd = -1;
            }

            current_udp_port = requested_port;
        }

        if (pending_udp_fd != -1) {
            close(pending_udp_fd);
        }

        if (strncmp(command, "AUTH ", 5) == 0) {
            authenticated =
                strcmp(response, "OK AUTHENTICATED " SID_TAG) == 0;

            if (!authenticated && udp_fd != -1) {
                close(udp_fd);
                udp_fd = -1;
                current_udp_port = 0;
            }
        }

        if (strcmp(command, "MONITOR STOP") == 0 &&
            strcmp(response, "OK MONITOR_STOPPED " SID_TAG) == 0) {
            if (udp_fd != -1) {
                close(udp_fd);
                udp_fd = -1;
            }

            current_udp_port = 0;
        }

        if (is_get &&
            strncmp(response, "OK FILE_SEND ", 13) == 0) {
            if (download_file(fd, response, command + 4) == -1) {
                status = EXIT_FAILURE;
                break;
            }
        }

        if (strcmp(response, "OK BYE " SID_TAG) == 0) {
            break;
        }

        if (strncmp(command, "PUT ", 4) == 0 &&
            strncmp(response, "ERR ", 4) == 0) {
            fprintf(stderr, "Upload rejected; reconnect to continue\n");
            status = EXIT_FAILURE;
            break;
        }

        printf("remoteops> ");
        fflush(stdout);
    }

    if (udp_fd != -1) {
        close(udp_fd);
    }

    close(fd);
    return status;
}
