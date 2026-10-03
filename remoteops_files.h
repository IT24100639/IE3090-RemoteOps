#ifndef REMOTEOPS_FILES_H
#define REMOTEOPS_FILES_H

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "remoteops_config.h"
#include "remoteops_io.h"

/* Accept simple filenames, without paths or leading dots. */
static inline int valid_filename(const char *name)
{
    size_t length = strlen(name);

    if (length == 0 || length > 128 || name[0] == '.') {
        return 0;
    }

    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)name[i];

        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '-' || c == '.')) {
            return 0;
        }
    }

    return 1;
}

/* Parse a decimal size without signs, spaces or overflow. */
static inline int parse_file_size(const char *text,
                                  unsigned long long *size)
{
    unsigned long long value = 0;

    if (*text == '\0') {
        return -1;
    }

    for (const char *p = text; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            return -1;
        }

        unsigned int digit = (unsigned int)(*p - '0');

        if (value > (~0ULL - digit) / 10ULL) {
            return -1;
        }

        value = value * 10ULL + digit;
    }

    *size = value;
    return 0;
}

/* File command replies include the personalised SID. */
static inline int file_reply(int fd, const char *message)
{
    char response[MAX_LINE];
    int length = snprintf(response, sizeof(response),
                          "%s %s\n", message, SID_TAG);

    if (length < 0 || (size_t)length >= sizeof(response)) {
        return -1;
    }

    return send_all(fd, response, (size_t)length);
}

/* Reject PUT and close the session because payload bytes may follow. */
static inline int reject_put(int fd, const char *message)
{
    file_reply(fd, message);
    return -1;
}

/* Arguments: filename filesize
   Receive raw bytes immediately after the PUT command line. */
static inline int receive_file(int fd, const char *arguments)
{
    char name[129];
    char size_text[32];
    char extra;

    if (sscanf(arguments, "%128s %31s %c",
               name, size_text, &extra) != 2 ||
        !valid_filename(name)) {
        return reject_put(fd, "ERR 010 INVALID_FILE_REQUEST");
    }

    unsigned long long size;

    if (parse_file_size(size_text, &size) == -1) {
        return reject_put(fd, "ERR 010 INVALID_FILE_REQUEST");
    }

    if (size > MAX_FILE_SIZE) {
        return reject_put(fd, "ERR 004 FILE_TOO_LARGE");
    }

    char destination[512];
    char temporary[512];

    int length = snprintf(destination, sizeof(destination),
                          "%s/%s", STORAGE_DIR, name);

    if (length < 0 || (size_t)length >= sizeof(destination)) {
        return reject_put(fd, "ERR 010 INVALID_FILE_REQUEST");
    }

    length = snprintf(temporary, sizeof(temporary),
                      "%s/.upload-XXXXXX", STORAGE_DIR);

    if (length < 0 || (size_t)length >= sizeof(temporary)) {
        return reject_put(fd, "ERR 011 FILE_IO_FAILED");
    }

    int temporary_fd = mkstemp(temporary);

    if (temporary_fd == -1) {
        return reject_put(fd, "ERR 011 FILE_IO_FAILED");
    }

    FILE *file = fdopen(temporary_fd, "wb");

    if (file == NULL) {
        close(temporary_fd);
        unlink(temporary);
        return reject_put(fd, "ERR 011 FILE_IO_FAILED");
    }

    unsigned char buffer[FILE_CHUNK_SIZE];
    unsigned long long remaining = size;

    while (remaining > 0) {
        size_t chunk = remaining > sizeof(buffer)
                     ? sizeof(buffer) : (size_t)remaining;

        if (recv_exact(fd, buffer, chunk) == -1) {
            fclose(file);
            unlink(temporary);
            return -1;
        }

        if (fwrite(buffer, 1, chunk, file) != chunk) {
            fclose(file);
            unlink(temporary);
            return reject_put(fd, "ERR 011 FILE_IO_FAILED");
        }

        remaining -= chunk;
    }

    if (fclose(file) != 0) {
        unlink(temporary);
        return reject_put(fd, "ERR 011 FILE_IO_FAILED");
    }

    /* Publish only a complete upload. */
    if (rename(temporary, destination) == -1) {
        unlink(temporary);
        return reject_put(fd, "ERR 011 FILE_IO_FAILED");
    }

    char message[256];
    snprintf(message, sizeof(message),
             "OK FILE_RECEIVED %s", name);

    return file_reply(fd, message);
}

/* Send a response header followed by exactly filesize raw bytes. */
static inline int send_file(int fd, const char *name)
{
    if (!valid_filename(name)) {
        return file_reply(fd, "ERR 010 INVALID_FILE_REQUEST");
    }

    char path[512];
    int length = snprintf(path, sizeof(path),
                          "%s/%s", STORAGE_DIR, name);

    if (length < 0 || (size_t)length >= sizeof(path)) {
        return file_reply(fd, "ERR 010 INVALID_FILE_REQUEST");
    }

    int file_fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);

    if (file_fd == -1) {
        return file_reply(fd, "ERR 005 FILE_NOT_FOUND");
    }

    struct stat information;

    if (fstat(file_fd, &information) == -1 ||
        !S_ISREG(information.st_mode) ||
        information.st_size < 0) {
        close(file_fd);
        return file_reply(fd, "ERR 005 FILE_NOT_FOUND");
    }

    unsigned long long size =
        (unsigned long long)information.st_size;

    if (size > MAX_FILE_SIZE) {
        close(file_fd);
        return file_reply(fd, "ERR 004 FILE_TOO_LARGE");
    }

    FILE *file = fdopen(file_fd, "rb");

    if (file == NULL) {
        close(file_fd);
        return file_reply(fd, "ERR 011 FILE_IO_FAILED");
    }

    char message[256];
    snprintf(message, sizeof(message),
             "OK FILE_SEND %s %llu", name, size);

    if (file_reply(fd, message) == -1) {
        fclose(file);
        return -1;
    }

    unsigned char buffer[FILE_CHUNK_SIZE];
    unsigned long long remaining = size;

    while (remaining > 0) {
        size_t chunk = remaining > sizeof(buffer)
                     ? sizeof(buffer) : (size_t)remaining;

        if (fread(buffer, 1, chunk, file) != chunk ||
            send_all(fd, buffer, chunk) == -1) {
            fclose(file);
            return -1;
        }

        remaining -= chunk;
    }

    fclose(file);
    return 0;
}

#endif
