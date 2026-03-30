#define _XOPEN_SOURCE 700
#define _FILE_OFFSET_BITS 64

#include "worker_manager.h"
#include "utils.h"

#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void send_text(int fd, const char *text)
{
    if (text == NULL) {
        return;
    }

    write_all(fd, text, strlen(text));
}

ssize_t find_worker_index(worker_info_t *workers, size_t num_workers, pid_t pid)
{
    size_t i;

    for (i = 0; i < num_workers; i++) {
        if (workers[i].alive && workers[i].pid == pid) {
            return (ssize_t)i;
        }
    }

    return -1;
}

void send_progress_info(int resp_fd, work_pool_t *pool)
{
    char buf[256];
    size_t done;
    int percent;
    int total;

    done = processed_jobs(pool);
    total = total_matches(pool);

    if (pool->num_jobs == 0) {
        percent = 100;
    } else {
        percent = (int)((done * 100) / pool->num_jobs);
    }

    snprintf(buf, sizeof(buf),
             "PROGRESS: %d%% complete, matches so far: %d\n",
             percent, total);

    send_text(resp_fd, buf);
}

void send_workers_info(int resp_fd, worker_info_t *workers, size_t num_workers)
{
    char buf[256];
    size_t i;
    int active = 0;

    for (i = 0; i < num_workers; i++) {
        if (workers[i].alive) {
            active++;
        }
    }

    snprintf(buf, sizeof(buf), "INFO: active workers: %d\n", active);
    send_text(resp_fd, buf);

    for (i = 0; i < num_workers; i++) {
        if (workers[i].alive) {
            snprintf(buf, sizeof(buf),
                     "  worker pid=%ld busy=%d current_job=%d\n",
                     (long)workers[i].pid,
                     workers[i].busy,
                     workers[i].current_job_id);
            send_text(resp_fd, buf);
        }
    }
}

void spawn_one_worker(worker_info_t *workers,
                      size_t *num_workers,
                      const char *file_path)
{
    int to_worker[2];
    int from_worker[2];
    pid_t pid;
    char read_fd_str[16];
    char write_fd_str[16];

    if (*num_workers >= MAX_WORKERS) {
        return;
    }

    if (pipe(to_worker) < 0) {
        write_message(2, "pipe to_worker failed\n");
        return;
    }

    if (pipe(from_worker) < 0) {
        close(to_worker[0]);
        close(to_worker[1]);
        write_message(2, "pipe from_worker failed\n");
        return;
    }

    pid = fork();
    if (pid < 0) {
        close(to_worker[0]);
        close(to_worker[1]);
        close(from_worker[0]);
        close(from_worker[1]);
        write_message(2, "fork worker failed\n");
        return;
    }

    if (pid == 0) {
        close(to_worker[1]);
        close(from_worker[0]);

        snprintf(read_fd_str, sizeof(read_fd_str), "%d", to_worker[0]);
        snprintf(write_fd_str, sizeof(write_fd_str), "%d", from_worker[1]);

        {
            char *newargv[] = {
                "./worker",
                (char *)file_path,
                read_fd_str,
                write_fd_str,
                NULL
            };

            execv("./worker", newargv);
        }

        _exit(1);
    }

    close(to_worker[0]);
    close(from_worker[1]);

    workers[*num_workers].pid = pid;
    workers[*num_workers].to_worker_fd = to_worker[1];
    workers[*num_workers].from_worker_fd = from_worker[0];
    workers[*num_workers].busy = 0;
    workers[*num_workers].alive = 1;
    workers[*num_workers].current_job_id = -1;

    (*num_workers)++;
}

void spawn_n_workers(worker_info_t *workers,
                     size_t *num_workers,
                     const char *file_path,
                     int n)
{
    int i;

    for (i = 0; i < n; i++) {
        spawn_one_worker(workers, num_workers, file_path);
    }
}

void terminate_last_workers(worker_info_t *workers,
                            size_t num_workers,
                            int n)
{
    ssize_t i;
    message_t msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = CMD_TERMINATE_WORKER;

    for (i = (ssize_t)num_workers - 1; i >= 0 && n > 0; i--) {
        if (workers[i].alive) {
            write_all(workers[i].to_worker_fd, &msg, sizeof(msg));
            n--;
        }
    }
}

void assign_jobs(worker_info_t *workers,
                 size_t num_workers,
                 work_pool_t *pool,
                 char target)
{
    size_t i;

    for (i = 0; i < num_workers; i++) {
        job_t *job;
        message_t msg;

        if (!workers[i].alive || workers[i].busy) {
            continue;
        }

        job = next_pending_job(pool);
        if (job == NULL) {
            return;
        }

        memset(&msg, 0, sizeof(msg));
        msg.type = CMD_ASSIGN_WORK;
        msg.job_id = job->job_id;
        msg.offset = job->offset;
        msg.length = job->length;
        msg.target = target;

        if (write_all(workers[i].to_worker_fd, &msg, sizeof(msg)) != (ssize_t)sizeof(msg)) {
            continue;
        }

        workers[i].busy = 1;
        workers[i].current_job_id = job->job_id;

        job->state = JOB_IN_PROGRESS;
        job->worker_pid = workers[i].pid;
    }
}

void handle_worker_result(worker_info_t *workers,
                          size_t num_workers,
                          work_pool_t *pool,
                          int worker_fd)
{
    message_t msg;
    ssize_t r;
    ssize_t idx;
    job_t *job;

    memset(&msg, 0, sizeof(msg));
    r = read_all(worker_fd, &msg, sizeof(msg));

    if (r <= 0) {
        return;
    }

    if (msg.type != CMD_WORK_RESULT) {
        return;
    }

    idx = find_worker_index(workers, num_workers, msg.pid);
    if (idx >= 0) {
        workers[idx].busy = 0;
        workers[idx].current_job_id = -1;
    }

    job = find_job_by_id(pool, msg.job_id);
    if (job == NULL) {
        return;
    }

    if (msg.count >= 0 && msg.value == 0) {
        job->state = JOB_DONE;
        job->result_count = msg.count;
        job->worker_pid = msg.pid;
    } else {
        job->state = JOB_PENDING;
        job->worker_pid = -1;
        job->result_count = 0;
    }
}

void reap_dead_workers(worker_info_t *workers,
                       size_t num_workers,
                       work_pool_t *pool)
{
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        ssize_t idx = find_worker_index(workers, num_workers, pid);

        if (idx >= 0) {
            int job_id = workers[idx].current_job_id;

            workers[idx].alive = 0;
            workers[idx].busy = 0;
            workers[idx].current_job_id = -1;

            close(workers[idx].to_worker_fd);
            close(workers[idx].from_worker_fd);

            if (job_id >= 0) {
                job_t *job = find_job_by_id(pool, job_id);

                if (job != NULL && job->state == JOB_IN_PROGRESS) {
                    job->state = JOB_PENDING;
                    job->worker_pid = -1;
                    job->result_count = 0;
                }
            }
        }
    }
}

void shutdown_all_workers(worker_info_t *workers, size_t num_workers)
{
    size_t i;
    message_t msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = CMD_TERMINATE_WORKER;

    for (i = 0; i < num_workers; i++) {
        if (workers[i].alive) {
            write_all(workers[i].to_worker_fd, &msg, sizeof(msg));
        }
    }

    for (i = 0; i < num_workers; i++) {
        if (workers[i].alive) {
            close(workers[i].to_worker_fd);
            close(workers[i].from_worker_fd);
        }
    }

    while (waitpid(-1, NULL, 0) > 0) {
    }
}