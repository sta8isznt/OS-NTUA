#include "protocol.h"
#include "utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define BUF 256
#define DISPATCHER_BUF 4096

int shutting_down = 0;

int main(int argc, char *argv[])
{
    char dispatcher_buf[DISPATCHER_BUF];
    size_t dispatcher_buf_len = 0;

    if (argc != 3) {
        char error[] = "Arguments format: ./frontend input_file target\n";
        write_message(2, error);
        exit(1);
    }

    if (strlen(argv[2]) != 1) {
        char error[] = "The second argument must be a single character\n";
        write_message(2, error);
        exit(1);
    }

    int pipe_fd_dis[2];
    int pipe_dis_fd[2];

    if (pipe(pipe_fd_dis) < 0) {
        char error[] = "Error creating pipe_fd_dis\n";
        write_message(2, error);
        exit(1);
    }

    if (pipe(pipe_dis_fd) < 0) {
        char error[] = "Error creating pipe_dis_fd\n";
        write_message(2, error);
        exit(1);
    }

    pid_t p = fork();
    if (p < 0) {
        char error[] = "Error in fork\n";
        write_message(2, error);
        exit(1);
    }

    if (p == 0) {
        close(pipe_fd_dis[1]);
        close(pipe_dis_fd[0]);

        // Store in buffers the fds of pipes in order to pass them in dispatcher
        char fd_dis[16];
        char dis_fd[16];

        snprintf(fd_dis, sizeof(fd_dis), "%d", pipe_fd_dis[0]);
        snprintf(dis_fd, sizeof(dis_fd), "%d", pipe_dis_fd[1]);

        char *newargv[] = {
            "./dispatcher",
            fd_dis,
            dis_fd,
            argv[1],
            argv[2],
            NULL
        };

        execv("./dispatcher", newargv);

        // If successful this code will never execute
        {
            char error[] = "execv dispatcher failed\n";
            write_message(2, error);
            _exit(1);
        }
    }

    close(pipe_fd_dis[0]);
    close(pipe_dis_fd[1]);

    {
        char *welcome_message =
            "Welcome to the app!\n"
            "To see the progress type <p>\n"
            "To add x workers type <a x>\n"
            "To remove y workers type <r y>\n"
            "To see process info type <i>\n"
            "To exit the app type <e>\n";
        write_message(1, welcome_message);
    }

    while (1) {
        // Setup a file descriptor set to monitor -> Stdin + Dispatcher Response pipe
        fd_set fds;
        char buf[BUF];
        int nfds;
        int rc;

        FD_ZERO(&fds);
        if (!shutting_down) FD_SET(0, &fds);
        FD_SET(pipe_dis_fd[0], &fds);

        nfds = pipe_dis_fd[0] + 1;

        rc = select(nfds, &fds, NULL, NULL, NULL);
        if (rc < 0) {
            write_message(2, "Error with select\n");
            break;
        }

        if (FD_ISSET(0, &fds)) {
            int n = read_until(0, buf, BUF, '\n');
            if (n < 0) {
                write_message(2, "Error reading from stdin\n");
                break;
            } else if (n == 0) {
                break;
            }

            message_t msg;
            // Initialize the message struct with zeros
            memset(&msg, 0, sizeof(msg));

            if (parse_user_command(buf, &msg) < 0) {
                write_message(1, "Please pass a correct command!\n");
                continue;
            }

            // For p ir i instruction send dignal to dispatcher
            if (msg.type == CMD_PROGRESS) {
                if (kill(p, SIGUSR1) < 0) {
                    write_message(2, "Error sending SIGUSR1 to dispatcher\n");
                }
            }
            else if (msg.type == CMD_INFO) {
                if (kill(p, SIGUSR2) < 0) {
                    write_message(2, "Error sending SIGUSR2 to dispatcher\n");
                }
            }
            else {
                if (write_all(pipe_fd_dis[1], &msg, sizeof(msg)) != (ssize_t)sizeof(msg)) {
                    write_message(2, "Error writing command to dispatcher\n");
                    break;
                }
                // IF SHUTDOWN occurs, set the corresponding flag, close the pipe to dispatcher and stop monitoring stdin
                // When the dispatcher closes his side of pipe EOF will occur and the loop will break
                if (msg.type == CMD_SHUTDOWN) {
                    shutting_down = 1;
                    close(pipe_fd_dis[1]);
                }
            }
        }

        if (FD_ISSET(pipe_dis_fd[0], &fds)) {
            ssize_t n;
            size_t start;
            size_t i;

            n = read(pipe_dis_fd[0],
                     dispatcher_buf + dispatcher_buf_len,
                     sizeof(dispatcher_buf) - dispatcher_buf_len);
            if (n < 0) {
                write_message(2, "Error reading from dispatcher pipe\n");
                break;
            }
            else if (n == 0) {
                if (dispatcher_buf_len > 0) {
                    if (write_all(STDOUT_FILENO, dispatcher_buf, dispatcher_buf_len) !=
                        (ssize_t)dispatcher_buf_len) {
                        write_message(2, "Error writing response to stdout\n");
                    }
                }
                break;
            }

            dispatcher_buf_len += (size_t)n;
            start = 0;

            for (i = 0; i < dispatcher_buf_len; i++) {
                if (dispatcher_buf[i] == '\n') {
                    size_t line_len = i - start + 1;

                    if (write_all(STDOUT_FILENO, dispatcher_buf + start, line_len) !=
                        (ssize_t)line_len) {
                        write_message(2, "Error writing response to stdout\n");
                        break;
                    }
                    start = i + 1;
                }
            }

            if (i < dispatcher_buf_len) {
                break;
            }

            if (start > 0) {
                memmove(dispatcher_buf,
                        dispatcher_buf + start,
                        dispatcher_buf_len - start);
                dispatcher_buf_len -= start;
            }

            if (dispatcher_buf_len == sizeof(dispatcher_buf)) {
                if (write_all(STDOUT_FILENO, dispatcher_buf, dispatcher_buf_len) !=
                    (ssize_t)dispatcher_buf_len) {
                    write_message(2, "Error writing response to stdout\n");
                    break;
                }
                dispatcher_buf_len = 0;
            }
        }
    }

    if(!shutting_down) close(pipe_fd_dis[1]);
    close(pipe_dis_fd[0]);

    // We dont care here about status termination info
    waitpid(p, NULL, 0);
    return 0;
}
