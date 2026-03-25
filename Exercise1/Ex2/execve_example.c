#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

extern char ** environ;

int main(int argc, char * argv[]){
    if (argc != 5){
        perror("Invalid number of arguments!");
        exit(1);
    }

    char * newargv[] = {argv[1], argv[2], argv[3], argv[4], NULL};
    pid_t p;
    
    p = fork();
    if (p < 0){
        perror("fork");
        exit(1);
    }
    if (p == 0){
        execve(argv[1], newargv, environ);
        /* execve returns only on error */
        perror("execve");
        exit(1);
    }
    else{
        wait(NULL);
    }
}
