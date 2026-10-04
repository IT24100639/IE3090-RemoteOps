#ifndef REMOTEOPS_LOG_H
#define REMOTEOPS_LOG_H

#include <arpa/inet.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>

#include "remoteops_config.h"

/* One mutex protects complete log entries from concurrent threads. */
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline int log_check(void)
{
    FILE *file = fopen(LOG_FILE, "a");
    if (file == NULL) {
        perror(LOG_FILE);
        return -1;
    }
    if (fclose(file) != 0) {
        perror("close log");
        return -1;
    }
    return 0;
}

static inline void log_event(int fd, const char *event,
                             const char *format, ...)
{
    char details[MAX_LINE];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(details, sizeof(details), format, arguments);
    va_end(arguments);

    /* Prevent control characters from creating misleading log lines. */
    for (size_t i = 0; details[i] != '\0'; ++i) {
        unsigned char character = (unsigned char)details[i];
        if (character < 32 || character == 127) {
            details[i] = ' ';
        }
    }

    char peer_ip[INET_ADDRSTRLEN] = "server";
    unsigned peer_port = 0;
    struct sockaddr_in peer = {0};
    socklen_t peer_length = sizeof(peer);

    if (fd >= 0 &&
        getpeername(fd, (struct sockaddr *)&peer, &peer_length) == 0 &&
        peer.sin_family == AF_INET) {
        if (inet_ntop(AF_INET, &peer.sin_addr,
                      peer_ip, sizeof(peer_ip)) == NULL) {
            snprintf(peer_ip, sizeof(peer_ip), "unknown");
        }
        peer_port = (unsigned)ntohs(peer.sin_port);
    }

    pthread_mutex_lock(&log_mutex);

    time_t now = time(NULL);
    struct tm local_time;
    char timestamp[64] = "time-unavailable";

    if (localtime_r(&now, &local_time) != NULL) {
        if (strftime(timestamp, sizeof(timestamp),
                     "%Y-%m-%dT%H:%M:%S%z", &local_time) == 0) {
            snprintf(timestamp, sizeof(timestamp), "time-unavailable");
        }
    }

    FILE *file = fopen(LOG_FILE, "a");
    if (file == NULL) {
        perror(LOG_FILE);
    } else {
        if (fprintf(file, "%s %s peer=%s:%u event=%s %s\n",
                    timestamp, SID_TAG, peer_ip, peer_port,
                    event, details) < 0) {
            perror("write log");
        }
        if (fclose(file) != 0) {
            perror("close log");
        }
    }

    pthread_mutex_unlock(&log_mutex);
}

#endif
