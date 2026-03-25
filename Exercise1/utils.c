#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void write_message(int fd, const char *message){
    size_t len = strlen(message);
    ssize_t wcnt;
    size_t idx = 0;

    do {
        wcnt = write(fd, message + idx, len - idx);
        if (wcnt == -1) {
            return;
        }
        idx += wcnt;
    } while (idx < len);
}

ssize_t read_all(int fd, void *buf, size_t count){
    size_t total = 0;
    ssize_t n;

    while (total < count) {
        n = read(fd, (char *)buf + total, count - total);
        if (n == -1) {
            return -1;
        }
        if (n == 0) {
            break;
        }
        total += n;
    }

    return total;
}

ssize_t write_all(int fd, const void *buf, size_t count){
    size_t total = 0;
    ssize_t n;

    while (total < count) {
        n = write(fd, (char *)buf + total, count - total);
        if (n == -1) {
            return -1;
        }
        total += n;
    }

    return total;
}

void show_pstree(pid_t p){
    int ret;
    char cmd[1024];

    snprintf(cmd, sizeof(cmd), "echo; echo; pstree -a -G -c -p %ld; echo; echo",
    (long)p);
    cmd[sizeof(cmd)-1] = '\0';
    ret = system(cmd);
    if (ret < 0) {
        write_message(2, "system");
        exit(104);
    }
}
