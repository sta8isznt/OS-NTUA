#ifndef WORK_POOL_H
#define WORK_POOL_H

#include <stddef.h>
#include <sys/types.h>

#define CHUNK_SIZE 4096

typedef enum {
    JOB_PENDING,
    JOB_IN_PROGRESS,
    JOB_DONE
} job_state_t;

typedef struct {
    int job_id;
    off_t offset;
    size_t length;
    job_state_t state;
    pid_t worker_pid;
    int result_count;
} job_t;

typedef struct {
    job_t *jobs;
    size_t num_jobs;
} work_pool_t;

void init_work_pool_from_file(const char *file_path, work_pool_t *pool);
void free_work_pool(work_pool_t *pool);

job_t *find_job_by_id(work_pool_t *pool, int job_id);
job_t *next_pending_job(work_pool_t *pool);

int all_jobs_done(work_pool_t *pool);
int total_matches(work_pool_t *pool);
size_t processed_jobs(work_pool_t *pool);

#endif