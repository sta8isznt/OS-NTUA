#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

#include "mandel-lib.h"

#define MANDEL_MAX_ITERATION 100000

#define perror_pthread(ret, msg) \
    do { errno = ret; perror(msg); } while (0)

int y_chars = 50;
int x_chars = 90;

// Define a global variable that will hold all the lines stored sequencially
int *image;

// Define the array of semaphores
sem_t *sems;

// Define the boundaries of the complex plane to be drawn
double xmin = -1.8, xmax = 1.0;
double ymin = -1.0, ymax = 1.0;

// Define the step of x and y for each point to be drawn
double xstep;
double ystep;

// Define the thread info structure
struct thread_info_struct {
    pthread_t tid; /* POSIX thread id, as returned by the library */

    int thrid; /* Application-defined thread id */
    int thrcnt;
};

/*
 * This function computes a line of output
 * as an array of x_char color values.
 */
void compute_mandel_line(int line, int color_val[])
{
    // Compute the y value corresponding to this line
    double x, y;

    int n;
    int val;

    /* Find out the y value corresponding to this line */
    y = ymax - ystep * line;

    /* and iterate for all points on this line */
    for (x = xmin, n = 0; n < x_chars; x+= xstep, n++) {

        /* Compute the point's color value */
        val = mandel_iterations_at_point(x, y, MANDEL_MAX_ITERATION);
        if (val > 255)
            val = 255;

        /* And store it in the color_val[] array */
        val = xterm_color(val);
        color_val[n] = val;
    }
}

/*
 * This function outputs an array of x_char color values
 * to a 256-color xterm.
 */
void output_mandel_line(int fd, int color_val[])
{
    int i;

    char point ='@';
    char newline='\n';

    for (i = 0; i < x_chars; i++) {
        /* Set the current color, then output the point */
        set_xterm_color(fd, color_val[i]);
        if (write(fd, &point, 1) != 1) {
            perror("compute_and_output_mandel_line: write point");
            exit(1);
        }
    }

    /* Now that the line is done, output a newline character */
    if (write(fd, &newline, 1) != 1) {
        perror("compute_and_output_mandel_line: write newline");
        exit(1);
    }
}

int safe_atoi(char *s, int *val)
{
    long l;
    char *endp;

    l = strtol(s, &endp, 10);
    if (s != endp && *endp == '\0') {
        *val = l;
        return 0;
    } else
        return -1;
}

void *safe_malloc(size_t size)
{
    void *p;

    if ((p = malloc(size)) == NULL) {
        fprintf(stderr, "Out of memory, failed to allocate %zd bytes\n",
                size);
        exit(1);
    }

    return p;
}

void *thread_start_fn(void* arg){
    struct thread_info_struct *thr = arg;
    int line;

    fprintf(stderr, "Thread %d of %d. START.\n", thr->thrid, thr->thrcnt);

    for (line = thr->thrid; line < y_chars; line += thr->thrcnt){
        compute_mandel_line(line, &image[line * x_chars]);
    }

    for (line = thr->thrid; line < y_chars; line += thr->thrcnt){
        if (sem_wait(&sems[line]) != 0) {
            perror("sem_wait");
            exit(1);
        }
        output_mandel_line(1, &image[line * x_chars]);
        if(line + 1< y_chars){
            if (sem_post(&sems[line+1]) != 0) {
                perror("sem_post");
                exit(1);
            }
        }
    }

    fprintf(stderr, "Thread %d of %d. END.\n", thr->thrid, thr->thrcnt);
    return NULL;
}

int main(int argc, char * argv[]){
    int NTHREADS;
    struct thread_info_struct *thr;
    xstep = (xmax - xmin) / x_chars;
    ystep = (ymax - ymin) / y_chars;

    // Parse the arguments safely
    if (argc != 2){
        fprintf(stderr, "Usage: %s thread_count\n", argv[0]);
        exit(1);
    }

    if ((safe_atoi(argv[1], &NTHREADS) < 0) || NTHREADS <= 0){
        fprintf(stderr, "Please pass a correct number of threads\n");
        exit(1);
    }

    // Allocate and initialize the thread info structures
    thr = safe_malloc(NTHREADS * sizeof(*thr));
    memset(thr, 0, NTHREADS * sizeof(*thr));

    // Allocate and initialize the image
    image = safe_malloc(y_chars*x_chars *sizeof(*image));
    memset(image, 0, y_chars*x_chars *sizeof(*image));

    // Allocate and initialize the array of semaphores
    sems = safe_malloc(y_chars * sizeof(*sems));
    for (int i = 0; i < y_chars; i++) {
        if (i == 0){
            if (sem_init(&sems[i], 0, 1) != 0) {
                perror("sem_init");
                exit(1);
            }
        } else {
            if (sem_init(&sems[i], 0, 0) != 0) {
                perror("sem_init");
                exit(1);
            }
        }
    }

    // Spawn the threads
    for (int i=0; i < NTHREADS; i++){
        thr[i].thrid = i;
        thr[i].thrcnt = NTHREADS;

        int ret = pthread_create(&thr[i].tid, NULL, thread_start_fn, &thr[i]);
        if (ret){
            perror_pthread(ret, "pthread_create");
            exit(1);
        }
    }

    // Wait for all threads to terminate
    for( int i=0; i < NTHREADS; i++){
        int ret = pthread_join(thr[i].tid, NULL);
        if (ret != 0) {
        perror_pthread(ret, "pthread_join");
        exit(1);
        }
    }

    // Destroy the semaphores and free the allocated memory
    for( int i=0; i < y_chars; i++){
        if (sem_destroy(&sems[i]) != 0) {
            perror("sem_destroy");
            exit(1);
        }
    }
    free(sems);
    free(image);
    free(thr);
    

    reset_xterm_color(1);
    return 0;
}
