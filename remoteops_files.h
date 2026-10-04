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
#include "remoteops_log.h"

/* Only simple filenames are accepted; directory paths are forbidden. */
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

static inline int parse_file_size(const char *text,
                                  unsigned long long *size)
{
    unsigned long long value = 0;
    if (*text == '\0') {
        return -1;
    }

    for (size_t i = 0; text[i] != '\0'; ++i) {
        if (text[i] < '0' || text[i] > '9') {
            return -1;
        }
        unsigned digit = (unsigned)(text[i] - '0');
        if (value > (~0ULL - digit) / 10ULL) {
            return -1;
        }
        value = value * 10ULL + digit;
    }

    *size = value;
    return 0;
}

static inline int file_reply(int fd, const char *message)
{
    char response[MAX_LINE];
    int length = snprintf(response, sizeof(response),
                          "%s %s\n", message, SID_TAG);
    if (length < 0 || (size_t)length >= sizeof(response)) {
        return -1;
    }

    int result = send_all(fd, response, (size_t)length);
    log_event(fd, "FILE_RESPONSE", "send=%s message=%s",
              result == 0 ? "ok" : "failed", message);
    return result;
}

/* Close rejected uploads to avoid interpreting payload as commands. */
static inline int reject_put(int fd, const char *message)
{
    log_event(fd, "PUT_REJECTED", "%s", message);
    file_reply(fd, message);
    return -1;
}

static inline int receive_file(int fd, const char *arguments)
{
    char filename[129];
    char size_text[32];
    char extra;
    unsigned long long size;

    if (sscanf(arguments, "%128s %31s %c",
               filename, size_text, &extra) != 2 ||
        !valid_filename(filename) ||
        parse_file_size(size_text, &size) != 0) {
        return reject_put(fd, "ERR 010 INVALID_FILE_REQUEST");
    }

    if (size > MAX_FILE_SIZE) {
        return reject_put(fd, "ERR 004 FILE_TOO_LARGE");
    }

    char destination[512];
    int length = snprintf(destination, sizeof(destination),
                          "%s/%s", STORAGE_DIR, filename);
    if (length < 0 || (size_t)length >= sizeof(destination)) {
        return reject_put(fd, "ERR 010 INVALID_FILE_REQUEST");
    }

    char temporary[512];
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

    log_event(fd, "PUT_ACCEPTED",
              "filename=%s expected_bytes=%llu", filename, size);

    unsigned long long remaining = size;
    unsigned long long received = 0;
    unsigned char buffer[FILE_CHUNK_SIZE];

    while (remaining > 0) {
        size_t chunk = remaining > sizeof(buffer) ?
                       sizeof(buffer) : (size_t)remaining;

        if (recv_exact(fd, buffer, chunk) == -1) {
            log_event(fd, "PUT_FAILED",
                      "filename=%s completed_chunk_bytes=%llu expected_bytes=%llu reason=connection",
                      filename, received, size);
            fclose(file);
            unlink(temporary);
            return -1;
        }

        if (fwrite(buffer, 1, chunk, file) != chunk) {
            log_event(fd, "PUT_FAILED",
                      "filename=%s reason=write", filename);
            fclose(file);
            unlink(temporary);
            return reject_put(fd, "ERR 011 FILE_IO_FAILED");
        }

        remaining -= chunk;
        received += chunk;
    }

    if (fclose(file) != 0) {
        unlink(temporary);
        return reject_put(fd, "ERR 011 FILE_IO_FAILED");
    }

    if (rename(temporary, destination) == -1) {
        unlink(temporary);
        return reject_put(fd, "ERR 011 FILE_IO_FAILED");
    }

    log_event(fd, "PUT_COMPLETE",
              "filename=%s bytes=%llu storage=%s",
              filename, size, destination);

    char message[256];
    snprintf(message, sizeof(message),
             "OK FILE_RECEIVED %s", filename);
    return file_reply(fd, message);
}

static inline int send_file(int fd, const char *filename)
{
    if (!valid_filename(filename)) {
        return file_reply(fd, "ERR 010 INVALID_FILE_REQUEST");
    }

    char path[512];
    int length = snprintf(path, sizeof(path),
                          "%s/%s", STORAGE_DIR, filename);
    if (length < 0 || (size_t)length >= sizeof(path)) {
        return file_reply(fd, "ERR 010 INVALID_FILE_REQUEST");
    }

    int file_fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (file_fd == -1) {
        log_event(fd, "GET_REJECTED",
                  "filename=%s reason=unavailable", filename);
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
             "OK FILE_SEND %s %llu", filename, size);

    if (file_reply(fd, message) == -1) {
        fclose(file);
        return -1;
    }

    log_event(fd, "GET_ACCEPTED",
              "filename=%s expected_bytes=%llu", filename, size);

    unsigned long long remaining = size;
    unsigned long long sent = 0;
    unsigned char buffer[FILE_CHUNK_SIZE];

    while (remaining > 0) {
        size_t chunk = remaining > sizeof(buffer) ?
                       sizeof(buffer) : (size_t)remaining;

        if (fread(buffer, 1, chunk, file) != chunk) {
            log_event(fd, "GET_FAILED",
                      "filename=%s completed_chunk_bytes=%llu reason=read",
                      filename, sent);
            fclose(file);
            return -1;
        }

        if (send_all(fd, buffer, chunk) == -1) {
            log_event(fd, "GET_FAILED",
                      "filename=%s completed_chunk_bytes=%llu reason=connection",
                      filename, sent);
            fclose(file);
            return -1;
        }

        remaining -= chunk;
        sent += chunk;
    }

    fclose(file);
    log_event(fd, "GET_COMPLETE",
              "filename=%s bytes=%llu", filename, sent);
    return 0;
}

#endif
