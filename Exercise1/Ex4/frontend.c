#include "protocol.h"
#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <signal.h>

#define BUF 256

int main(int argc, char * argv[]){
    /*
    Check for arguments faults 
    The correct format should be ./frontend input_file target
    */
    if (argc != 3){
        char error[] = "Arguments fromat: ./frontend input_file target\n";
        write_message(2, error);
        exit(1);
    }

    if (strlen(argv[2]) != 1){
        char error[] = "The second argument must be a single character\n";
        write_message(2, error);
        exit(1);
    }

    /* Create a pipe to communicate with dispatcher*/
    int pipe_fd_dis[2];
    int pipe_dis_fd[2];
    if (pipe(pipe_fd_dis) < 0){
            char error[] = "Error creating pipe_fd_dis\n";
            write_message(2, error);
            exit(1);
    }
    if (pipe(pipe_dis_fd) < 0){
        char error[] = "Error creating pipe_dis_fd\n";
        write_message(2, error);
        exit(1);
    }

    /* Fork to create dispatcher */
    pid_t p;
    p = fork();
    if (p < 0){
        char error[] = "Error in fork\n";
        write_message(2, error);
        exit(1);
    }
    if (p == 0){
        /* Dispatcher reads commands from frontend and writes replies back. */
        close(pipe_fd_dis[1]);
        close(pipe_dis_fd[0]);

        /* Create the new argv and pass the pipes fds */
        char fd_dis[16];
        char dis_fd[16];

        snprintf(fd_dis, sizeof(fd_dis), "%d", pipe_fd_dis[0]);
        snprintf(dis_fd, sizeof(dis_fd), "%d", pipe_dis_fd[1]);

        char* newargv[] = {"./dispatcher",fd_dis, dis_fd, argv[1], argv[2], NULL};
        execv("./dispatcher", newargv);

        /* If this code executes an error at execv has occured */
        char error[] = "execv\n";
        write_message(2, error);
        exit(1);
    }

    /* Frontend writes commands to dispatcher and reads replies back. */
    close(pipe_fd_dis[0]);
    close(pipe_dis_fd[1]);

    char *welcome_message = "Welcome to the app!\nTo see the progress type <p>\nTo add x workers type <a x>\nTo remove y workers type <r y>\nTo see process info type <i>\nTo exit the app type <e>\n";
    write_message(1,welcome_message);

    // Create a buffer for reading
    char buf[BUF];

    /* Event Loop */
    while (1) {
        /* Define a file descriptor set to monitor */
        fd_set fds;
        FD_ZERO(&fds);

        /* Add stdin and dispatcher to the monitor list */
        FD_SET(0, &fds);
        FD_SET(pipe_dis_fd[0], &fds);

        /* Set the max fd for select */
        int nfds = (pipe_dis_fd[0] > 0 ? pipe_dis_fd[0] : 0) + 1;

        /* Wait until stdin or dispatcher write something */
        if (select(nfds, &fds, NULL, NULL, NULL) < 0){
            char error[] = "Error with select\n";
            write_message(2, error);
            exit(1);
        };

        // User input (stdin)
        if (FD_ISSET(0, &fds)){
            // Read until you see a new line
            int n = read_until(0, buf, BUF, '\n');
            if (n < 0){
                char error[] = "Error reading from stdin\n";
                write_message(2, error);
                exit(1);
            }
            else if (n == 0) break;

            // Parse the command from the user
            message_t msg;
            if (parse_user_command(buf, &msg) < 0){
                char error[] = "Please pass a correct command!\n";
                write_message(1, error);
                continue;
            }

            if (msg.type == CMD_PROGRESS){
                // If the command is progress send a signal
                if (kill(p, SIGUSR1) < 0) {
                    char error[] = "Error sending SIGUSR1 to dispatcher\n";
                    write_message(2, error);
                }
            }
            else if (msg.type == CMD_INFO){
                // If the command is info send a signal
                if (kill(p, SIGUSR2) < 0) {
                    char error[] = "Error sending SIGUSR2 to dispatcher\n";
                    write_message(2, error);
                }
            }
            else {
                // Send the command to dispatcher
                if (write_all(pipe_fd_dis[1], &msg, sizeof(msg)) != sizeof(msg)){
                    char error[] = "Error writing to dispatcher\n";
                    write_message(2, error);
                    exit(1);
                }
            }
        }

        // Dispatcher response
        if (FD_ISSET(pipe_dis_fd[0], &fds)){
            int n = read_until(pipe_dis_fd[0], buf, BUF, '\n');

            if (n < 0){
                char error[] = "Error reading from pipe_dis_fd\n";
                write_message(2, error);
                exit(1);
            }
            else if (n == 0) break;

            if (write_all(1, buf, n) != n){
                char error[] = "Error writing response from dispatcher to stdout\n";
                write_message(2, error);
                exit(1);
            }
        }
    }
}
