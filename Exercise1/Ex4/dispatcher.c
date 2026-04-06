#include "protocol.h"
#include "utils.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define CHUNK_SIZE 4096
#define MAX_WORKERS 128

typedef enum {
    JOB_PENDING,
    JOB_IN_PROGRESS,
    JOB_DONE
} job_state_t;

// Struct to model a chunk
typedef struct {
    int job_id;
    off_t offset;
    size_t length;
    job_state_t state;
    pid_t worker_pid;
    int result_count;
} job_t;

// Struct to model a worker
typedef struct {
    pid_t pid;
    int to_fd;              // Write end of the pipe
    int from_fd;            // Read end of the pipe
    int alive;
    int busy;
    int current_job_id;     // -1 when nothing is assigned
} worker_t;

// Declare Flags used by the signal handlers
static volatile sig_atomic_t progress_flag = 0;
static volatile sig_atomic_t info_flag = 0;
static volatile sig_atomic_t child_flag = 0;
static volatile sig_atomic_t stop_flag = 0;

// Declare a pointer for the next round robin free worker
static size_t rr_next = 0;

// We will implement a controlled shutdown for SIGINT
static void h_usr1(int s){(void)s;progress_flag=1;}
static void h_usr2(int s){(void)s;info_flag=1;}
static void h_chld(int s){(void)s;child_flag=1;}
static void h_term(int s){(void)s;stop_flag=1;}

/*
 Install all signal handlers used by the dispatcher.
 SIGUSR1: request progress information
 SIGUSR2: request worker information
 SIGCHLD: detect worker termination
 SIGINT/SIGTERM: stop dispatcher cleanly
 */

static void install_handlers(void){
    // We will reuse the same sigaction struct
    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags=0;
    sa.sa_handler=h_usr1; sigaction(SIGUSR1,&sa,NULL);
    sa.sa_handler=h_usr2; sigaction(SIGUSR2,&sa,NULL);
    sa.sa_handler=h_chld; sigaction(SIGCHLD,&sa,NULL);
    // Map both SIGINT, SIGTERM to the termination handler
    sa.sa_handler=h_term; sigaction(SIGINT,&sa,NULL);
    sigaction(SIGTERM,&sa,NULL);
    signal(SIGPIPE,SIG_IGN);
}

/*
  Parameters: FilePath, JobArray, NumberOfJobs
  Split the input file into fixed-size chunks and create the work pool.
  Each chunk becomes one job with:
  unique job id
  file offset
  chunk length
  initial state JOB_PENDING
 */
static void init_jobs(const char *file, job_t** jobs,size_t* njobs){
    struct stat st;
    size_t i,n;
    if(stat(file,&st)<0){
        write_message(2,"stat failed\n");
        exit(1);
    }
    if(st.st_size==0){
        *jobs=NULL;
        *njobs=0;
        return;
    }
    // Ceiling division
    n=(size_t)((st.st_size+CHUNK_SIZE-1)/CHUNK_SIZE);
    *jobs=malloc(n*sizeof(job_t));
    if(*jobs==NULL){
        write_message(2,"malloc failed\n");
        exit(1);
    }
    *njobs=n;
    for(i=0;i<n;i++){
        off_t off=(off_t)i*CHUNK_SIZE;
        size_t len=(off+CHUNK_SIZE<=st.st_size)?CHUNK_SIZE:(size_t)(st.st_size-off);
        (*jobs)[i].job_id=(int)i;
        (*jobs)[i].offset=off;
        (*jobs)[i].length=len;
        (*jobs)[i].state=JOB_PENDING;
        (*jobs)[i].worker_pid=-1;
        (*jobs)[i].result_count=0;
    }
}

static int active_workers(worker_t w[]){
    int i,c=0;
    for(i=0;i<MAX_WORKERS;i++) if(w[i].alive) c++;
    return c;
}

/*
  Return the index of the first free worker slot. Used to add new workers.
  Return -1 if no free slot exists.
 */
static int find_free_slot(worker_t w[]){
    int i;
    for(i=0;i<MAX_WORKERS;i++) if(!w[i].alive) return i;
    return -1;
}

// Used for handling dead workers.
static int find_worker_by_pid(worker_t w[],pid_t pid){
    int i;
    for(i=0;i<MAX_WORKERS;i++) if(w[i].alive && w[i].pid==pid) return i;
    return -1;
}

/*
 Return the id of the first pending job.
 If no pending job exists, return -1. First Pending, First Served
 */
static int find_pending_job(job_t *jobs,size_t njobs){
    size_t i;
    for(i=0;i<njobs;i++) if(jobs[i].state==JOB_PENDING) return (int)i;
    return -1;
}

/*
 Return the index of the next alive and jobless worker using round-robin and update the rr_next.
 If no free worker exists, return -1.
 */
static int next_free_worker(worker_t w[]){
    size_t k;
    for(k=0;k<MAX_WORKERS;k++){
        size_t i=(rr_next+k)%MAX_WORKERS;
        if(w[i].alive && !w[i].busy){ rr_next=(i+1)%MAX_WORKERS; return (int)i; }
    }
    return -1;
}

/*
  Check if there is a job left.
  Return 0 if there is left and 1 if all jobs are done
*/
static int all_jobs_done(job_t *jobs,size_t njobs){
    size_t i;
    for(i=0;i<njobs;i++) if(jobs[i].state!=JOB_DONE) return 0;
    return 1;
}

static int total_matches(job_t *jobs,size_t njobs){
    size_t i;
    int total=0;
    for(i=0;i<njobs;i++) total+=jobs[i].result_count;
    return total;
}

static size_t done_jobs(job_t *jobs,size_t njobs){
    size_t i,d=0;
    for(i=0;i<njobs;i++) if(jobs[i].state==JOB_DONE) d++;
    return d;
}

/*
  Removes cleanly the worker from the bookkeeping list\
*/
static void clear_worker(worker_t *w){
    if(w->alive){
        close(w->to_fd);
        close(w->from_fd);
    }
    w->pid=0;
    w->alive=0;
    w->busy=0;
    w->current_job_id=-1;
}

/*
 Create one new worker process.
 For each worker we create:
 one pipe from dispatcher to worker
 one pipe from worker to dispatcher
 Then we fork and execv the worker executable.
 The parent stores the worker information in the worker list.
 */
static void spawn_one_worker(worker_t w[],const char *file,int cmd_fd,int resp_fd){
    int slot=find_free_slot(w),to_p[2],from_p[2],i;
    pid_t pid;
    if(slot<0){
        write_message(resp_fd,"No free worker slot\n");
        return;
    }
    if(pipe(to_p)<0||pipe(from_p)<0){ 
        write_message(2,"pipe failed\n");
        exit(1);
    }
    pid=fork();
    if(pid<0){
        write_message(2,"fork failed\n");
        exit(1);
    }
    if(pid==0){
        char rfd[16],wfd[16];
        char *newargv[]={
            "./worker",
            (char *)file,
            rfd,
            wfd,
            NULL
        };

        // Close all pipes to other workers in the current worker
        for(i=0;i<MAX_WORKERS;i++) if(w[i].alive){
            close(w[i].to_fd);
            close(w[i].from_fd);
        }
        // Close pipes to and from frontend to the current worker
        close(cmd_fd);
        close(resp_fd);

        close(to_p[1]);
        close(from_p[0]);

        snprintf(rfd,sizeof(rfd),"%d",to_p[0]);
        snprintf(wfd,sizeof(wfd),"%d",from_p[1]);

        execv("./worker",newargv);

        write_message(2,"execv worker failed\n");
        _exit(1);
    }
    close(to_p[0]);
    close(from_p[1]);

    // Bookkeep this worker
    w[slot].pid=pid;
    w[slot].to_fd=to_p[1];
    w[slot].from_fd=from_p[0];
    w[slot].alive=1;
    w[slot].busy=0;
    w[slot].current_job_id=-1;
}

static void add_workers(worker_t w[],int n,const char *file,int cmd_fd,int resp_fd){
    char buf[128];
    int i;
    for(i=0;i<n;i++) spawn_one_worker(w,file,cmd_fd,resp_fd);

    // Respond positive to the frontend
    snprintf(buf,sizeof(buf),"Workers added, active workers = %d\n",active_workers(w));
    write_message(resp_fd,buf);
}

/*
  Send a Termination message to the workers. After finishing their jobs
  they will be reaped and removed from the worker list.
*/
static void remove_workers(worker_t w[],int n,int resp_fd){
    message_t msg;
    int i;
    memset(&msg,0,sizeof(msg));
    msg.type=CMD_TERMINATE_WORKER;

    // Start removing workers from the end
    for(i=MAX_WORKERS-1;i>=0&&n>0;i--){
        if(w[i].alive){
            write_all(w[i].to_fd,&msg,sizeof(msg));
            n--;
        }
    }
    write_message(resp_fd, "Requested removal of worker(s). Current alive count may update shortly\n");
}

static void assign_jobs(worker_t w[],job_t *jobs,size_t njobs,char target){
    while(1){
        int j=find_pending_job(jobs,njobs);
        int k=next_free_worker(w);
        message_t msg;
        if(j<0||k<0) break;     // If no more job left or no free worker left stop the assignment loop

        // Costruct the message to the worker
        memset(&msg,0,sizeof(msg));
        msg.type=CMD_ASSIGN_WORK;
        msg.offset=jobs[j].offset;
        msg.length=jobs[j].length;
        msg.target=target;
        msg.job_id=jobs[j].job_id;

        // If Dispatcher fails to write the message -> terminate this worker (not trustworthy)
        if(write_all(w[k].to_fd,&msg,sizeof(msg))!=(ssize_t)sizeof(msg)){
            clear_worker(&w[k]);
            continue;
        }

        // Update bookkeeping
        w[k].busy=1;
        w[k].current_job_id=j;
        jobs[j].state=JOB_IN_PROGRESS;
        jobs[j].worker_pid=w[k].pid;
    }
}

/*
  Function to process replies from workers and update the bookkeeping of jobs and workers.
  idx param tells which worker entry.
*/
static void handle_reply(worker_t w[], job_t *jobs, size_t njobs, int idx){
    message_t rep;
    ssize_t r = read_all(w[idx].from_fd, &rep, sizeof(rep));

    if (r == 0) {
        // Worker's pipe reached EOF
        return;
    }

    if (r < 0) {
        // Real reading error occured
        return;
    }

    // Do some sanity checks
    if ((size_t)r != sizeof(rep)) return;
    if (rep.type != CMD_WORK_RESULT) return;

    // Update bookkeeping
    w[idx].busy = 0;
    w[idx].current_job_id = -1;

    // Validate that the reply contains a valid job id
    if (rep.job_id < 0 || (size_t)rep.job_id >= njobs) return;

    if (rep.value == 0 && rep.count >= 0) {
        jobs[rep.job_id].state = JOB_DONE;
        jobs[rep.job_id].result_count = rep.count;
        jobs[rep.job_id].worker_pid = rep.pid;
    } else {
        jobs[rep.job_id].state = JOB_PENDING;
        jobs[rep.job_id].result_count = 0;
        jobs[rep.job_id].worker_pid = -1;
    }
}

static void reap_dead_workers(worker_t w[],job_t *jobs,size_t njobs){
    int status;
    pid_t pid;

    // Wait for a child (not blocking)
    while((pid=waitpid(-1,&status,WNOHANG))>0){
        int idx=find_worker_by_pid(w,pid);
        if(idx>=0){

            // Check whether the worker had a job in progress -> Worker died before sending a completion message
            int j=w[idx].current_job_id;
            if(j>=0&&(size_t)j<njobs&&jobs[j].state==JOB_IN_PROGRESS){
                // Reinitialize the job
                jobs[j].state=JOB_PENDING;
                jobs[j].worker_pid=-1;
                jobs[j].result_count=0;
            }
            clear_worker(&w[idx]);
        }
    }
}

static void send_progress(int resp_fd, job_t *jobs, size_t njobs){
    char buf[128];
    int percent;

    // Compute the percentage of jobs done
    if (njobs == 0) {
        percent = 100;
    } else {
        percent = (int)((done_jobs(jobs, njobs) * 100) / njobs);
    }

    snprintf(buf, sizeof(buf),
             "PROGRESS: %d%% complete, matches so far: %d\n",
             percent, total_matches(jobs, njobs));

    write_message(resp_fd, buf);
}

static void send_info(int resp_fd,worker_t w[]){
    char buf[128];
    int i;

    snprintf(buf,sizeof(buf),"INFO: active workers = %d\n",active_workers(w));

    write_message(resp_fd,buf);

    for(i=0;i<MAX_WORKERS;i++) if(w[i].alive){
        snprintf(buf,sizeof(buf),"  worker pid=%ld busy=%d current_job=%d\n",(long)w[i].pid,w[i].busy,w[i].current_job_id);
        write_message(resp_fd,buf);
    }
}

static void terminate_all(worker_t w[]){
    message_t msg;
    int i;

    memset(&msg,0,sizeof(msg));
    msg.type=CMD_TERMINATE_WORKER;

    for(i=0;i<MAX_WORKERS;i++) if(w[i].alive) write_all(w[i].to_fd,&msg,sizeof(msg));
    for(i=0;i<MAX_WORKERS;i++) if(w[i].alive){
        close(w[i].to_fd);
        close(w[i].from_fd);
    }
    while(waitpid(-1,NULL,0)>0){}
}

int main(int argc, char *argv[]){
    int cmd_fd;
    int resp_fd;
    int maxfd;
    int rc;
    int i;
    const char *file;
    char target;
    worker_t workers[MAX_WORKERS];
    job_t *jobs = NULL;
    size_t njobs = 0;

    // Dispatcher arguments sanity checks
    if (argc != 5) {
        write_message(2, "Usage: ./dispatcher <cmd_fd> <resp_fd> <file> <char>\n");
        exit(1);
    }

    cmd_fd = atoi(argv[1]);
    resp_fd = atoi(argv[2]);
    file = argv[3];

    if (argv[4][0] == '\0' || argv[4][1] != '\0') {
        write_message(2, "dispatcher needs one character\n");
        exit(1);
    }

    target = argv[4][0];

    // Initialize the workers array
    memset(workers, 0, sizeof(workers));

    install_handlers();
    init_jobs(file, &jobs, &njobs);

    // Start the app with 2 workers and assign them jobs
    add_workers(workers, 2, file, cmd_fd, resp_fd);
    assign_jobs(workers, jobs, njobs, target);

    // Case of empty file
    if (njobs == 0) {
        write_message(resp_fd, "FINAL RESULT: total matches = 0\n");
        terminate_all(workers);
        close(cmd_fd);
        close(resp_fd);
        return 0;
    }

    while (!stop_flag) {
        fd_set fds;

        FD_ZERO(&fds);

        // Monitor the requests from frontend
        FD_SET(cmd_fd, &fds);
        maxfd = cmd_fd;

        // Monitor the responses of the alive workers
        for (i = 0; i < MAX_WORKERS; i++) {
            if (workers[i].alive) {
                FD_SET(workers[i].from_fd, &fds);
                if (workers[i].from_fd > maxfd) {
                    maxfd = workers[i].from_fd;
                }
            }
        }

        rc = select(maxfd + 1, &fds, NULL, NULL, NULL);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            write_message(2, "dispatcher select failed\n");
            break;
        }

        if (progress_flag) {
            send_progress(resp_fd, jobs, njobs);
            progress_flag = 0;
        }

        if (info_flag) {
            send_info(resp_fd, workers);
            info_flag = 0;
        }

        if (child_flag) {
            reap_dead_workers(workers, jobs, njobs);
            child_flag = 0;
        }

        if (FD_ISSET(cmd_fd, &fds)) {
            message_t cmd;
            ssize_t r;

            r = read_all(cmd_fd, &cmd, sizeof(cmd));

            if (r == 0) {
                break;
            }

            if (r < 0) {
                write_message(2, "dispatcher read_all failed\n");
                break;
            }

            if ((size_t)r != sizeof(cmd)) {
                continue;
            }

            if (cmd.type == CMD_ADD_WORKER && cmd.value > 0) {
                add_workers(workers, cmd.value, file, cmd_fd, resp_fd);
            } else if (cmd.type == CMD_REMOVE_WORKER && cmd.value > 0) {
                remove_workers(workers, cmd.value, resp_fd);
            } else if (cmd.type == CMD_SHUTDOWN) {
                break;
            }
        }

        // Search for the worker who wants to send a message and handle it
        for (i = 0; i < MAX_WORKERS; i++) {
            if (workers[i].alive && FD_ISSET(workers[i].from_fd, &fds)) {
                handle_reply(workers, jobs, njobs, i);
            }
        }

        assign_jobs(workers, jobs, njobs, target);

        if (all_jobs_done(jobs, njobs)) {
            char buf[128];

            snprintf(buf, sizeof(buf),
                     "FINAL RESULT: total matches = %d\n",
                     total_matches(jobs, njobs));
            write_message(resp_fd, buf);
            break;
        }
    }

    terminate_all(workers);
    free(jobs);
    close(cmd_fd);
    close(resp_fd);

    return 0;
}
