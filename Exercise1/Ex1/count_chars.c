#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#define BUFF_SIZE 1024

int main(int argc, char *argv[]){
    /* check for arguments faults */
    if (argc != 4){
        char error[] = "Please pass the correct number of arguments!\n";
        write_message(2, error);
        exit(1);
    }

    if (strlen(argv[3]) != 1){
        char error[] = "Please pass a single character to search for!\n";
        write_message(2, error);
        exit(1);
    }

    int fdr, fdw;
    char c2c = 'a';
    int cnt = 0;
    char buff[BUFF_SIZE];
    ssize_t n;
    
    /* open file for reading */
    fdr = open(argv[1], O_RDONLY);
    if (fdr == -1){
        char error[] = "Error opening file for reading";
        write_message(2, error);
        exit(1);
    }

    /* open file for writeing the result */
    fdw = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fdw == -1){
        char error[] = "Error opening file for writing";
        write_message(2, error);
        close(fdr);
        exit(1);
    }
    
    /* character to search for */
    c2c = argv[3][0];

    /* count the occurrences of the given character */
    while ((n = read(fdr, buff, BUFF_SIZE)) != 0){
        if (n == -1){
            char error[] = "Problem reading from file";
            write_message(2, error);
            close(fdr);
            close(fdw);
            exit(1);
        }
        for (int i=0; i < n; i++)
           if (c2c == buff[i])
              cnt++; 
    }
    
    /* close the file for reading */
    close(fdr);

    /* write the result in the output file */
    char output[100];

    sprintf(output, "The character '%c' appears %d times in file %s.\n", c2c, cnt, argv[1]);
    write_message(fdw, output);
    
    /* close the output file */
    close(fdw);
}
