#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>

#define BUFF_SIZE 1024
#define P 4

volatile sig_atomic_t current_children = 0;

void sighandler(int signum)
{
    /* We will manually convert the int to string in order not to use stdlib functions inside the handler */
    char buf[16];
    buf[0] = '\n';
    int n = current_children;
    int i = 1;

    // read the parameter signum and do nothing (for the compiler)
    (void)signum;

    if (n == 0) {
        buf[i++] = '0';
    } else {
        char tmp[16];
        int j = 0;

        while (n > 0) {
            tmp[j++] = '0' + (n % 10);
            n /= 10;
        }

        while (j > 0) {
            buf[i++] = tmp[--j];
        }
    }

    buf[i++] = '\n';
    write(2, buf, i);
}

void sigchld_handler(int signum)
{
    pid_t pid;
    int status;

    (void)signum;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        current_children--;
    }
}

int main(int argc, char *argv[])
{
    /* check for arguments faults */
    if (argc != 4)
    {
        char error[] = "Please pass the correct number of arguments!\n";
       write_message(2, error);
        _exit(1);
    }

    if (strlen(argv[3]) != 1)
    {
        char *error = "The third argument must be a single character\n";
        write_message(2, error);
        return 1;
    }

    /* Set the ctrl-c handler */
    struct sigaction sa;

    sa.sa_handler = sighandler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) < 0){
        char *error = "handler";
        write_message(2, error);
        return 1;
    }

    /* Set the children signal handler */
    struct sigaction sa_ch;
    sa_ch.sa_handler = sigchld_handler;
    sa_ch.sa_flags = SA_RESTART;
    sigemptyset(&sa_ch.sa_mask);
    if (sigaction(SIGCHLD, &sa_ch, NULL) < 0){
        char *error = "handler";
        write_message(2, error);
        return 1;
    }

    /* Get the file size */
    struct stat reading_file_stats;
    if (stat(argv[1], &reading_file_stats) < 0)
    {
        char *error = "Error getting stats for the file\n";
        write_message(2, error);
        return 1;
    }
    off_t size = reading_file_stats.st_size;

    /* Create P pipes */
    int pipes[P][2];

    for (int i = 0; i < P; i++)
    {
        if (pipe(pipes[i]) < 0)
        {
            char *error = "Error creating a pipe\n";
            write_message(2, error);
            return 1;
        }
    }

    /* Create P children with fork */
    for (int i = 0; i < P; i++)
    {
        pid_t p = fork();
        if (p < 0)
        {
            char *error = "Error in fork\n";
            write_message(2, error);
            return 1;
        }
        if (p == 0)
        {
            /* Set a handler in order to ignore the SIGINT signal */
            signal(SIGINT, SIG_IGN);

            /* Open the file */
            int fd = open(argv[1], O_RDONLY);
            if (fd < 0)
            {
                char *error = "Error opening the file\n";
                write_message(2, error);
                _exit(1);
            }

            /* Close the parents or other children's pipes */
            for (int j = 0; j < P; j++)
            {
                close(pipes[j][0]);
                if (j != i)
                    close(pipes[j][1]);
            }

            /* Compute the range of file corresponding to this child */
            off_t start = i * size / P;
            off_t end = (i + 1) * size / P;

            /* Move the file offset to start */
            if (lseek(fd, start, SEEK_SET) < 0)
            {
                char *error = "Error moving the file offset\n";
                write_message(2, error);
                _exit(1);
            }

            /* Read the child's chunk from the file */
            off_t remaining = end - start;
            char buffer[BUFF_SIZE];
            int count = 0;

            while (remaining > 0)
            {
                ssize_t chunk = (remaining > BUFF_SIZE) ? BUFF_SIZE : remaining;
                ssize_t n = read(fd, buffer, chunk);
                if (n < 0)
                {
                    char *error = "Error reading from file";
                    write_message(2, error);
                    _exit(1);
                }
                else if (n == 0)
                    break;

                for (int i = 0; i < n; i++)
                {
                    if (argv[3][0] == buffer[i])
                        count++;
                }
                remaining -= n;
            }

            sleep((i + 1) * 2);

            /* Write the count in the pipe */
            if (write(pipes[i][1], &count, sizeof(count)) != sizeof(count))
            {
                char *error = "Error writing to pipe\n";
                write_message(2, error);
                _exit(1);
            }

            close(fd);
            close(pipes[i][1]);


            _exit(0);
        }
        // Parent increases the current number of children after successful fork
        current_children++;
    }

    /* Parent closes write ends */
    for (int i = 0; i < P; i++)
    {
        close(pipes[i][1]);
    }

    /* Wait for all childs to write their result and then sum them up */
    int total = 0;
    int x;

    for (int i = 0; i < P; i++)
    {
        if (read(pipes[i][0], &x, sizeof(x)) != sizeof(x))
        {
            char *error = "Error reading from pipe\n";
            write_message(2, error);
            return 1;
        }
        total += x;
        close(pipes[i][0]);
    }      

    /* Open output file*/
    int fdw = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fdw < 0)
    {
        char *error = "Error opening the file\n";
        write_message(2, error);
        _exit(1);
    }

    /* write the result to the output file */
    char output[100];
    int idx = 0;
    size_t len;
    ssize_t wcnt;

    sprintf(output, "The character '%c' appears %d times in file %s.\n", argv[3][0], total, argv[1]);
    len = strlen(output);
    do
    {
        wcnt = write(fdw, output + idx, len - idx);
        if (wcnt == -1)
        {
            char * error = "Error writing to the output file\n";
            write_message(2, error);
            close(fdw);
            return 1;
        }
        idx += wcnt;
    } while (idx < len);

    /* Close the writing file */
    close(fdw);
    return 0;
}
