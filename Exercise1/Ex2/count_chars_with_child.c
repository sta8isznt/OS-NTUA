#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "utils.h"
#define BUFF_SIZE 1024

int main(int argc, char *argv[]){
    /* check for arguments faults */
    if (argc != 4){
        char error[] = "Please pass the correct number of arguments!\n";
        write_message(2, error);
        exit(1);
    }

    int fdr, fdw;
    int pipefd[2];
    char c2c = 'a';
    int cnt = 0;
    int child_cnt = 0;
    char buff[BUFF_SIZE];
    char output[100];
    ssize_t n;
    
    /* open file for reading */
    fdr = open(argv[1], O_RDONLY);
    if (fdr == -1){
        char error[] = "Problem opening file to read\n";
        write_message(2, error);
        exit(1);
    }

    /* open file for writeing the result */
    fdw = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fdw == -1){
        char error[] = "Problem opening file to write\n";
        write_message(2, error);
        close(fdr);
        exit(1);
    }

    if (pipe(pipefd) == -1){
        char error[] = "Problem creating pipe\n";
        write_message(2, error);
        close(fdr);
        close(fdw);
        exit(1);
    }
    
    /* character to search for */
    c2c = argv[3][0];

    /* new code in order the child process to search for character occurances */
    pid_t p;
    int status;

    p = fork();
    if (p < 0){
        char error[] = "Problem during fork\n";
        write_message(2, error);
        close(fdr);
        close(fdw);
        close(pipefd[0]);
        close(pipefd[1]);
        exit(1);
    }
    if (p == 0){
        close(pipefd[0]);
    /* count the occurances of the given character */
        while ((n = read(fdr, buff, BUFF_SIZE)) != 0){
            if (n == -1){
                char error[] = "Problem reading from file\n";
                write_message(2, error);
                close(fdr);
                close(fdw);
                close(pipefd[1]);
                exit(1);
            }
            for (int i=0; i < n; i++)
               if (c2c == buff[i])
                  cnt++; 
        }
        if (write_all(pipefd[1], &cnt, sizeof(cnt)) == -1){
            char error[] = "Problem writing to pipe\n";
            write_message(2, error);
            close(fdr);
            close(fdw);
            close(pipefd[1]);
            exit(1);
        }
        close(fdr);
        close(fdw);
        close(pipefd[1]);
        exit(0);
    }
    else {
        close(pipefd[1]);
        wait(&status);
        if (!WIFEXITED(status) || WIFEXITED(status) != 0){
           char error[] = "Child failed\n";
           write_message(2, error);
           close(fdr);
           close(fdw);
           close(pipefd[0]);
           exit(1);
        } 

        if (read_all(pipefd[0], &child_cnt, sizeof(child_cnt)) != sizeof(child_cnt)){
            char error[] = "Problem reading from pipe\n";
            write_message(2, error);
            close(fdr);
            close(fdw);
            close(pipefd[0]);
            exit(1);
        }
        close(pipefd[0]);

        /* close the file for reading */
        close(fdr);

        /* write the result in the output file */
        int written = snprintf(output,sizeof(output), "The character '%c' appears %d times in file %s.\n",c2c,child_cnt,
            argv[1]);

        if (written < 0 || written >= sizeof(output)) {
            char error[] = "Output message too long\n";
            write_message(2, error);
            close(fdw);
            exit(1);
        }
        
        /* close the output file */
        close(fdw);
     }
}
