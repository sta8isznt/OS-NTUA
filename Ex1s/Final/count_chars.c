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

    int fdr, fdw;
    char errmessr[] = "Problem opening file to read\n";
    char errmessw[] = "Problem opening file to write\n";
    char errmessrw[] = "Problem reading from file\n";
    char c2c = 'a';
    int cnt = 0;
    char buff[BUFF_SIZE];
    ssize_t n;
    
    /* open file for reading */
    fdr = open(argv[1], O_RDONLY);
    if (fdr == -1){
        write_message(2, errmessr);
        exit(1);
    }

    /* open file for writeing the result */
    fdw = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fdw == -1){
        write_message(2, errmessw);
        close(fdr);
        exit(1);
    }
    
    /* character to search for */
    c2c = argv[3][0];

    /* count the occurrences of the given character */
    while ((n = read(fdr, buff, BUFF_SIZE)) != 0){
        if (n == -1){
            write_message(2, errmessrw);
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
