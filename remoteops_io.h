#ifndef REMOTEOPS_IO_H
#define REMOTEOPS_IO_H

#include <errno.h>
#include <stddef.h>
#include <sys/socket.h>

/* Keep sending until every byte has been sent. */
static inline int send_all(int fd, const void *data, size_t length)
{
    const unsigned char *bytes = data;
    size_t sent = 0;

    while (sent < length) {
        ssize_t result = send(fd, bytes + sent,
                              length - sent, MSG_NOSIGNAL);

        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return -1;
        }

        sent += (size_t)result;
    }

    return 0;
}

/* Receive exactly length bytes.
   Return 0 on success or -1 on error/premature disconnect. */
static inline int recv_exact(int fd, void *data, size_t length)
{
    unsigned char *bytes = data;
    size_t received = 0;

    while (received < length) {
        ssize_t result = recv(fd, bytes + received,
                              length - received, 0);

        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result < 0) {
            return -1;
        }
        if (result == 0) {
            errno = ECONNRESET;
            return -1;
        }

        received += (size_t)result;
    }

    return 0;
}

/* Return 1 for a line, 0 for clean EOF, -1 for an error,
   or -2 if the line exceeds the buffer capacity.
   Reading one byte at a time leaves file payload bytes untouched. */
static inline int recv_line(int fd, char *line, size_t capacity)
{
    size_t used = 0;

    if (capacity == 0) {
        return -2;
    }

    for (;;) {
        char character;
        ssize_t result = recv(fd, &character, 1, 0);

        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result < 0) {
            return -1;
        }
        if (result == 0) {
            return used == 0 ? 0 : -1;
        }
        if (character == '\n') {
            line[used] = '\0';
            return 1;
        }
        if (used >= capacity - 1) {
            return -2;
        }

        line[used++] = character;
    }
}

#endif
