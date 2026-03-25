#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "utils.h"

int main(int argc, char * argv[]){
    if (argc != 5){
        char error[] = "Invalid number of arguments!\n";
        write_message(2, error);
        exit(1);
    }

    char * newargv[] = {argv[1], argv[2], argv[3], argv[4], NULL};
    pid_t p;
    
    p = fork();
    if (p < 0){
        char error[] = "Problem during fork\n";
        write_message(2, error);
        exit(1);
    }
    if (p == 0){
        execv(argv[1], newargv);
        /* execv returns only on error */
        char error[] = "Problem during execv\n";
        write_message(2, error);
        exit(1);
    }
    else{
        wait(NULL);
    }
}