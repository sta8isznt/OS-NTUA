#ifndef WORKER_MANAGER_H
#define WORKER_MANAGER_H

#include "protocol.h"
#include "work_pool.h"

#include <stddef.h>
#include <sys/types.h>

#define MAX_WORKERS 128

typedef struct {
    pid_t pid;
    int to_worker_fd;
    int from_worker_fd;
    int busy;
    int alive;
    int current_job_id;
} worker_info_t;

void send_text(int fd, const char *text);

ssize_t find_worker_index(worker_info_t *workers, size_t num_workers, pid_t pid);

void send_progress_info(int resp_fd, work_pool_t *pool);
void send_workers_info(int resp_fd, worker_info_t *workers, size_t num_workers);

void spawn_one_worker(worker_info_t *workers,
                      size_t *num_workers,
                      const char *file_path);

void spawn_n_workers(worker_info_t *workers,
                     size_t *num_workers,
                     const char *file_path,
                     int n);

void terminate_last_workers(worker_info_t *workers,
                            size_t num_workers,
                            int n);

void assign_jobs(worker_info_t *workers,
                 size_t num_workers,
                 work_pool_t *pool,
                 char target);

void handle_worker_result(worker_info_t *workers,
                          size_t num_workers,
                          work_pool_t *pool,
                          int worker_fd);

void reap_dead_workers(worker_info_t *workers,
                       size_t num_workers,
                       work_pool_t *pool);

void shutdown_all_workers(worker_info_t *workers, size_t num_workers);

#endif