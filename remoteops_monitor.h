#ifndef REMOTEOPS_MONITOR_H
#define REMOTEOPS_MONITOR_H

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <unistd.h>

#include "remoteops_config.h"

/* One monitoring state belongs to one TCP client session. */
struct monitor_state {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    pthread_t thread;
    int active;
    int stop;
    int udp_fd;
    struct sockaddr_in destination;
};

/* Accept only a decimal UDP port from 1 to 65535. */
static inline int parse_udp_port(const char *text,
                                 unsigned short *port)
{
    unsigned int value = 0;

    if (*text == '\0') {
        return -1;
    }

    for (const char *p = text; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            return -1;
        }

        value = value * 10U + (unsigned int)(*p - '0');

        if (value > 65535U) {
            return -1;
        }
    }

    if (value == 0) {
        return -1;
    }

    *port = (unsigned short)value;
    return 0;
}

/* Initialise synchronisation before starting a monitoring thread. */
static inline int monitor_init(struct monitor_state *state)
{
    memset(state, 0, sizeof(*state));
    state->udp_fd = -1;

    int error = pthread_mutex_init(&state->mutex, NULL);

    if (error != 0) {
        return error;
    }

    error = pthread_cond_init(&state->condition, NULL);

    if (error != 0) {
        pthread_mutex_destroy(&state->mutex);
        return error;
    }

    return 0;
}

/* Send SYSINFO-style UDP datagrams at a regular interval. */
static inline void *monitor_worker(void *argument)
{
    struct monitor_state *state = argument;

    pthread_mutex_lock(&state->mutex);

    while (!state->stop) {
        struct sysinfo information;

        if (sysinfo(&information) == 0) {
            double cpu_load =
                (double)information.loads[0] / 65536.0;

            double memory_used =
                ((double)information.totalram -
                 (double)information.freeram) *
                (double)information.mem_unit /
                (1024.0 * 1024.0);

            char datagram[256];
            int length = snprintf(
                datagram, sizeof(datagram),
                "SYSINFO %.2f %.2f %ld %s\n",
                cpu_load, memory_used,
                information.uptime, SID_TAG);

            if (length > 0 &&
                (size_t)length < sizeof(datagram)) {
                ssize_t sent;

                do {
                    sent = sendto(
                        state->udp_fd,
                        datagram, (size_t)length,
                        MSG_DONTWAIT,
                        (struct sockaddr *)&state->destination,
                        sizeof(state->destination));
                } while (sent < 0 && errno == EINTR);
            }
        }

        struct timespec deadline;

        if (clock_gettime(CLOCK_REALTIME, &deadline) == -1) {
            break;
        }

        deadline.tv_sec += MONITOR_INTERVAL_SEC;

        /* STOP wakes this wait immediately. */
        while (!state->stop) {
            int error = pthread_cond_timedwait(
                &state->condition, &state->mutex, &deadline);

            if (error == ETIMEDOUT) {
                break;
            }

            if (error != 0) {
                state->stop = 1;
                break;
            }
        }
    }

    pthread_mutex_unlock(&state->mutex);
    return NULL;
}

/* Stop and join before closing the socket or freeing the state. */
static inline void monitor_stop(struct monitor_state *state)
{
    if (!state->active) {
        return;
    }

    pthread_mutex_lock(&state->mutex);
    state->stop = 1;
    pthread_cond_signal(&state->condition);
    pthread_mutex_unlock(&state->mutex);

    pthread_join(state->thread, NULL);

    close(state->udp_fd);
    state->udp_fd = -1;
    state->active = 0;
}

/* Use the TCP client's IP and the requested UDP destination port. */
static inline int monitor_start(struct monitor_state *state,
                                int tcp_fd,
                                unsigned short udp_port)
{
    struct sockaddr_in peer = {0};
    socklen_t peer_length = sizeof(peer);

    if (getpeername(tcp_fd, (struct sockaddr *)&peer,
                    &peer_length) == -1 ||
        peer.sin_family != AF_INET) {
        return -1;
    }

    int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (udp_fd == -1) {
        return -1;
    }

    /* A repeated START replaces this session's previous stream. */
    monitor_stop(state);

    peer.sin_port = htons(udp_port);
    state->destination = peer;
    state->udp_fd = udp_fd;
    state->stop = 0;

    int error = pthread_create(&state->thread, NULL,
                               monitor_worker, state);

    if (error != 0) {
        close(state->udp_fd);
        state->udp_fd = -1;
        errno = error;
        return -1;
    }

    state->active = 1;
    return 0;
}

/* Called on QUIT, disconnect or a fatal session error. */
static inline void monitor_destroy(struct monitor_state *state)
{
    monitor_stop(state);
    pthread_cond_destroy(&state->condition);
    pthread_mutex_destroy(&state->mutex);
}

#endif
