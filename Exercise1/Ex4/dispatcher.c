#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "utils.h"
#include "protocol.h"
#include <stdlib.h>
#include <math.h>

#define CHUNK_SIZE 4096

typedef enum {
    JOB_PENDING,
    JOB_IN_PROGRESS,
    JOB_DONE
} job_state_t;

typedef struct {
    int job_id;     // chunk number
    off_t offset;   // x
    size_t length;  // usually CHUNK_SIZE except of the final one

    job_state_t state;

    pid_t worker_pid;
    int result_count;   // partial result for this chunk
} job_t;

typedef struct {
    job_t *jobs;
    size_t num_jobs;
} work_pool_t;

void chunk_file(const char * file, work_pool_t * work_pool);

int main(int argc, char * argv[]){
    // Check the argument number
    if (argc < 4) {
        write_message(2, "Incorrect number of arguments passed to dispatcher\n");
        exit(1);
    }
    // Convert the pipe fds from strings to integers
    int cmd_fd = atoi(argv[1]);
    int resp_dis = atoi(argv[2]);

    // Split the file in chunks and initialize work_pool
    work_pool_t work_pool;
    chunk_file(argv[3], &work_pool);


    // Event loop
    while (1){
        fd_set fds;
        FD_ZERO(&fds);

        // Add the fd_dis pipe to the set
        FD_SET(cmd_fd, &fds);
    }
}

void chunk_file(const char * file, work_pool_t * work_pool){
    struct stat fstats;
    if (stat(file, &fstats) < 0){
        char error[] = "Error getting stats for the file\n";
        write_message(2, error);
        exit(1);
    }

    off_t size = fstats.st_size;
    work_pool->num_jobs = ceil((double)size/CHUNK_SIZE);
    work_pool->jobs = malloc(work_pool->num_jobs * sizeof(job_t));
    if (work_pool->jobs == NULL) {
        write_message(2, "malloc failed\n");
        exit(1);
    }

    // Fill the work_pool
    for (size_t i = 0; i < work_pool->num_jobs; i++) {
        work_pool->jobs[i].job_id = (int)i;
        work_pool->jobs[i].offset = (off_t)i * CHUNK_SIZE;

        if (work_pool->jobs[i].offset + CHUNK_SIZE <= size)
            work_pool->jobs[i].length = CHUNK_SIZE;
        else
            work_pool->jobs[i].length = size - work_pool->jobs[i].offset;

        // Indicate that no worker assigned to the job yet
        work_pool->jobs[i].state = JOB_PENDING;
        work_pool->jobs[i].worker_pid = -1;
        work_pool->jobs[i].result_count = 0;
    }
}