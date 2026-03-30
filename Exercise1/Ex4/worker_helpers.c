#define _XOPEN_SOURCE 700
#define _FILE_OFFSET_BITS 64

#include "worker_helpers.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 4096

static volatile sig_atomic_t stop_worker = 0;

static void term_handler(int sig)
{
    (void)sig;
    stop_worker = 1;
}

int worker_should_stop(void)
{
    return stop_worker ? 1 : 0;
}

int parse_fd(const char *s)
{
    char *end = NULL;
    long v;

    if (s == NULL || *s == '\0') {
        return -1;
    }

    errno = 0;
    v = strtol(s, &end, 10);

    if (errno != 0 || end == s || *end != '\0') {
        return -1;
    }

    if (v < 0 || v > INT_MAX) {
        return -1;
    }

    return (int)v;
}

int install_worker_handlers(void)
{
    struct sigaction sa;
    struct sigaction ign;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = term_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) < 0) {
        return -1;
    }

    if (sigaction(SIGTERM, &sa, NULL) < 0) {
        return -1;
    }

    memset(&ign, 0, sizeof(ign));
    ign.sa_handler = SIG_IGN;
    sigemptyset(&ign.sa_mask);
    ign.sa_flags = 0;

    if (sigaction(SIGPIPE, &ign, NULL) < 0) {
        return -1;
    }

    return 0;
}

int open_worker_file(const char *file_path)
{
    return open(file_path, O_RDONLY);
}

int count_in_chunk(int fd, off_t offset, size_t length, unsigned char target, int *out_count)
{
    unsigned char buffer[BUF_SIZE];
    size_t remaining = length;
    int total_count = 0;

    while (remaining > 0) {
        size_t to_read = remaining < BUF_SIZE ? remaining : BUF_SIZE;
        ssize_t n;

        if (stop_worker) {
            errno = EINTR;
            return -1;
        }

        n = pread(fd, buffer, to_read, offset);
        if (n < 0) {
            if (errno == EINTR) {
                if (stop_worker) {
                    errno = EINTR;
                    return -1;
                }
                continue;
            }
            return -1;
        }

        if (n == 0) {
            break;
        }

        for (ssize_t i = 0; i < n; i++) {
            if (buffer[i] == target) {
                if (total_count == INT_MAX) {
                    errno = ERANGE;
                    return -1;
                }
                total_count++;
            }
        }

        offset += n;
        remaining -= (size_t)n;
    }

    *out_count = total_count;
    return 0;
}