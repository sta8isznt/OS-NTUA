#define _XOPEN_SOURCE 700
#define _FILE_OFFSET_BITS 64

#include "work_pool.h"
#include "utils.h"

#include <sys/stat.h>
#include <stdlib.h>

void init_work_pool_from_file(const char *file_path, work_pool_t *pool)
{
    struct stat st;
    off_t size;
    size_t num_jobs;
    size_t i;

    if (pool == NULL) {
        write_message(2, "work pool pointer is NULL\n");
        exit(1);
    }

    pool->jobs = NULL;
    pool->num_jobs = 0;

    if (stat(file_path, &st) < 0) {
        write_message(2, "Error getting stats for the file\n");
        exit(1);
    }

    size = st.st_size;

    if (size == 0) {
        return;
    }

    num_jobs = (size_t)((size + CHUNK_SIZE - 1) / CHUNK_SIZE);

    pool->jobs = malloc(num_jobs * sizeof(job_t));
    if (pool->jobs == NULL) {
        write_message(2, "malloc failed\n");
        exit(1);
    }

    pool->num_jobs = num_jobs;

    for (i = 0; i < num_jobs; i++) {
        off_t offset = (off_t)i * CHUNK_SIZE;
        size_t len;

        if (offset + CHUNK_SIZE <= size) {
            len = CHUNK_SIZE;
        } else {
            len = (size_t)(size - offset);
        }

        pool->jobs[i].job_id = (int)i;
        pool->jobs[i].offset = offset;
        pool->jobs[i].length = len;
        pool->jobs[i].state = JOB_PENDING;
        pool->jobs[i].worker_pid = -1;
        pool->jobs[i].result_count = 0;
    }
}

void free_work_pool(work_pool_t *pool)
{
    if (pool == NULL) {
        return;
    }

    free(pool->jobs);
    pool->jobs = NULL;
    pool->num_jobs = 0;
}

job_t *find_job_by_id(work_pool_t *pool, int job_id)
{
    if (pool == NULL || job_id < 0 || (size_t)job_id >= pool->num_jobs) {
        return NULL;
    }

    return &pool->jobs[job_id];
}

job_t *next_pending_job(work_pool_t *pool)
{
    size_t i;

    if (pool == NULL) {
        return NULL;
    }

    for (i = 0; i < pool->num_jobs; i++) {
        if (pool->jobs[i].state == JOB_PENDING) {
            return &pool->jobs[i];
        }
    }

    return NULL;
}

int all_jobs_done(work_pool_t *pool)
{
    size_t i;

    if (pool == NULL) {
        return 0;
    }

    for (i = 0; i < pool->num_jobs; i++) {
        if (pool->jobs[i].state != JOB_DONE) {
            return 0;
        }
    }

    return 1;
}

int total_matches(work_pool_t *pool)
{
    size_t i;
    int total = 0;

    if (pool == NULL) {
        return 0;
    }

    for (i = 0; i < pool->num_jobs; i++) {
        total += pool->jobs[i].result_count;
    }

    return total;
}

size_t processed_jobs(work_pool_t *pool)
{
    size_t i;
    size_t done = 0;

    if (pool == NULL) {
        return 0;
    }

    for (i = 0; i < pool->num_jobs; i++) {
        if (pool->jobs[i].state == JOB_DONE) {
            done++;
        }
    }

    return done;
}