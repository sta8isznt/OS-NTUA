#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

int main(int argx, char *argv[]){
    pid_t p;
    int status;
    int x=20;
    char message[200];

    p = fork();
    if (p < 0){
        write_message(2, "fork\n");
        exit(1);
    }
    if (p == 0){
        pid_t my_pid = getpid(), ppid = getppid();
        sprintf(message, "Hello world! My PID is %ld and my parent's PID is %ld\n", (long)my_pid, (long)ppid);
        write_message(1, message);
        x = 10;
        sprintf(message, "The child's variable is %d\n", x);
        write_message(1, message);
        exit(1);
    }
    else{
        sprintf(message, "My child's PID is %ld\n", (long)p);
        write_message(1, message);
        wait(&status);
        sprintf(message, "My child did it's job. It's exit status is %ld.\n", sizeof(WEXITSTATUS(status)));
        write_message(1, message);
        sprintf(message, "The parent's variable is %d\n", x);
        write_message(1, message);
        return 0;
    }
}
