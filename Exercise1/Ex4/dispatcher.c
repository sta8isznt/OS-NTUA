#define _XOPEN_SOURCE 700
#define _FILE_OFFSET_BITS 64

#include "protocol.h"
#include "utils.h"
#include "work_pool.h"
#include "worker_manager.h"

#include <sys/select.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

static volatile sig_atomic_t progress_requested = 0;
static volatile sig_atomic_t info_requested = 0;
static volatile sig_atomic_t child_changed = 0;
static volatile sig_atomic_t terminate_dispatcher = 0;

static void sigusr1_handler(int sig) { (void)sig; progress_requested = 1; }
static void sigusr2_handler(int sig) { (void)sig; info_requested = 1; }
static void sigchld_handler(int sig) { (void)sig; child_changed = 1; }
static void sigterm_handler(int sig) { (void)sig; terminate_dispatcher = 1; }

static int parse_fd(const char *s)
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

static int install_handlers(void)
{
    struct sigaction sa;
    struct sigaction ign;

    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sa.sa_handler = sigusr1_handler;
    if (sigaction(SIGUSR1, &sa, NULL) < 0) return -1;

    sa.sa_handler = sigusr2_handler;
    if (sigaction(SIGUSR2, &sa, NULL) < 0) return -1;

    sa.sa_handler = sigchld_handler;
    if (sigaction(SIGCHLD, &sa, NULL) < 0) return -1;

    sa.sa_handler = sigterm_handler;
    if (sigaction(SIGINT, &sa, NULL) < 0) return -1;
    if (sigaction(SIGTERM, &sa, NULL) < 0) return -1;

    memset(&ign, 0, sizeof(ign));
    ign.sa_handler = SIG_IGN;
    sigemptyset(&ign.sa_mask);
    ign.sa_flags = 0;

    if (sigaction(SIGPIPE, &ign, NULL) < 0) return -1;

    return 0;
}

static void build_fd_set(fd_set *fds,
                         int cmd_fd,
                         worker_info_t *workers,
                         size_t num_workers,
                         int *maxfd)
{
    size_t i;

    FD_ZERO(fds);
    FD_SET(cmd_fd, fds);
    *maxfd = cmd_fd;

    for (i = 0; i < num_workers; i++) {
        if (workers[i].alive) {
            FD_SET(workers[i].from_worker_fd, fds);
            if (workers[i].from_worker_fd > *maxfd) {
                *maxfd = workers[i].from_worker_fd;
            }
        }
    }
}

static void handle_signal_requests(int resp_fd,
                                   worker_info_t *workers,
                                   size_t num_workers,
                                   work_pool_t *pool)
{
    if (progress_requested) {
        send_progress_info(resp_fd, pool);
        progress_requested = 0;
    }

    if (info_requested) {
        send_workers_info(resp_fd, workers, num_workers);
        info_requested = 0;
    }

    if (child_changed) {
        reap_dead_workers(workers, num_workers, pool);
        child_changed = 0;
    }
}

static void handle_frontend_command(int cmd_fd,
                                    int resp_fd,
                                    const char *file_path,
                                    worker_info_t *workers,
                                    size_t *num_workers)
{
    message_t cmd;
    ssize_t r;

    memset(&cmd, 0, sizeof(cmd));
    r = read_all(cmd_fd, &cmd, sizeof(cmd));

    if (r == 0) {
        terminate_dispatcher = 1;
        return;
    }

    if (r < 0) {
        if (errno != EINTR) {
            write_message(2, "dispatcher read cmd failed\n");
            terminate_dispatcher = 1;
        }
        return;
    }

    if ((size_t)r != sizeof(cmd)) {
        return;
    }

    if (cmd.type == CMD_ADD_WORKER) {
        if (cmd.value > 0) {
            spawn_n_workers(workers, num_workers, file_path, cmd.value);
            send_text(resp_fd, "Workers added\n");
        }
    } else if (cmd.type == CMD_REMOVE_WORKER) {
        if (cmd.value > 0) {
            terminate_last_workers(workers, *num_workers, cmd.value);
            send_text(resp_fd, "Workers removal requested\n");
        }
    } else if (cmd.type == CMD_SHUTDOWN) {
        terminate_dispatcher = 1;
    }
}

static void handle_ready_worker_fds(fd_set *fds,
                                    worker_info_t *workers,
                                    size_t num_workers,
                                    work_pool_t *pool)
{
    size_t i;

    for (i = 0; i < num_workers; i++) {
        if (workers[i].alive && FD_ISSET(workers[i].from_worker_fd, fds)) {
            handle_worker_result(workers, num_workers, pool, workers[i].from_worker_fd);
        }
    }
}

static int maybe_finish(int resp_fd, work_pool_t *pool)
{
    char buf[128];

    if (pool->num_jobs == 0) {
        send_text(resp_fd, "FINAL RESULT: total matches = 0\n");
        return 1;
    }

    if (!all_jobs_done(pool)) {
        return 0;
    }

    snprintf(buf, sizeof(buf),
             "FINAL RESULT: total matches = %d\n",
             total_matches(pool));
    send_text(resp_fd, buf);

    return 1;
}

int main(int argc, char *argv[])
{
    int cmd_fd;
    int resp_fd;
    const char *file_path;
    char target;
    work_pool_t pool;
    worker_info_t workers[MAX_WORKERS];
    size_t num_workers = 0;

    if (argc != 5) {
        write_message(2, "Usage: ./dispatcher <cmd_fd> <resp_fd> <file> <target>\n");
        exit(1);
    }

    cmd_fd = parse_fd(argv[1]);
    resp_fd = parse_fd(argv[2]);

    if (cmd_fd < 0 || resp_fd < 0) {
        write_message(2, "Invalid dispatcher pipe descriptors\n");
        exit(1);
    }

    file_path = argv[3];

    if (argv[4] == NULL || argv[4][0] == '\0' || argv[4][1] != '\0') {
        write_message(2, "Dispatcher target must be a single character\n");
        exit(1);
    }

    target = argv[4][0];

    memset(workers, 0, sizeof(workers));
    init_work_pool_from_file(file_path, &pool);

    if (install_handlers() < 0) {
        write_message(2, "dispatcher sigaction failed\n");
        free_work_pool(&pool);
        exit(1);
    }

    if (pool.num_jobs > 0) {
        spawn_n_workers(workers, &num_workers, file_path, 2);
        assign_jobs(workers, num_workers, &pool, target);
    }

    while (!terminate_dispatcher) {
        fd_set fds;
        int maxfd;
        int rc;

        build_fd_set(&fds, cmd_fd, workers, num_workers, &maxfd);

        rc = select(maxfd + 1, &fds, NULL, NULL, NULL);
        if (rc < 0 && errno != EINTR) {
            write_message(2, "dispatcher select failed\n");
            break;
        }

        handle_signal_requests(resp_fd, workers, num_workers, &pool);

        if (FD_ISSET(cmd_fd, &fds)) {
            handle_frontend_command(cmd_fd, resp_fd, file_path, workers, &num_workers);
        }

        handle_ready_worker_fds(&fds, workers, num_workers, &pool);

        assign_jobs(workers, num_workers, &pool, target);

        if (maybe_finish(resp_fd, &pool)) {
            break;
        }
    }

    shutdown_all_workers(workers, num_workers);
    free_work_pool(&pool);

    close(cmd_fd);
    close(resp_fd);

    return 0;
}