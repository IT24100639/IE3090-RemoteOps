#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
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
        fprintf(stderr,
                "Use a simple filename, for example: PUT sample.txt\n");
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
            fprintf(stderr, "Could not read the complete upload file\n");
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

/* Receive exactly the payload size advertised in the GET response. */
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
    printf("Authenticate first: AUTH %s\n", AUTH_TOKEN);
    printf("Upload: PUT filename\n");
    printf("Download: GET filename\n");

    char command[MAX_LINE];
    char response[MAX_LINE];
    int authenticated = 0;
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

        command[--length] = '\0';

        if (length > 0 && command[length - 1] == '\r') {
            command[--length] = '\0';
        }

        int is_get = strncmp(command, "GET ", 4) == 0;

        if (strncmp(command, "PUT ", 4) == 0) {
            if (!authenticated) {
                fprintf(stderr,
                        "Authenticate before uploading: AUTH %s\n",
                        AUTH_TOKEN);
                continue;
            }

            int result = upload_file(fd, command + 4);

            if (result == 0) {
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
                status = EXIT_FAILURE;
                break;
            }
        }

        if (recv_line(fd, response, sizeof(response)) != 1) {
            fprintf(stderr, "Connection closed or response failed\n");
            status = EXIT_FAILURE;
            break;
        }

        printf("%s\n", response);

        if (strncmp(command, "AUTH ", 5) == 0) {
            authenticated =
                strcmp(response, "OK AUTHENTICATED " SID_TAG) == 0;
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
    }

    close(fd);
    return status;
}
