#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "protocol.h"
#include "utils.h"

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#define BUF_SIZE 4096

/*
 Worker logic:
 open the file once
 repeatedly wait for work from dispatcher
 count target characters in assigned chunk
 send one result back
 exit on CMD_TERMINATE_WORKER
 */

static int count_in_chunk(int fd, off_t offset, size_t length, char target){
    char buf[BUF_SIZE];
    off_t pos = offset;
    off_t end = offset + (off_t)length;
    int total = 0;

    while (pos < end) {
        size_t to_read;
        ssize_t n;
        ssize_t i;

        if ((off_t)BUF_SIZE <= end - pos) {
            to_read = BUF_SIZE;
        } else {
            to_read = (size_t)(end - pos);
        }

        n = pread(fd, buf, to_read, pos);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (n == 0) {
            break;
        }

        usleep(1000000);

        for (i = 0; i < n; i++) {
            if (buf[i] == target) {
                total++;
            }
        }

        pos += n;
    }

    return total;
}

int main(int argc, char *argv[]){
    const char *filename;
    int read_fd;
    int write_fd;
    int file_fd;

    if (argc != 4) {
        write_message(2, "Usage: ./worker <file> <read_fd> <write_fd>\n");
        _exit(1);
    }

    filename = argv[1];
    read_fd = atoi(argv[2]);
    write_fd = atoi(argv[3]);

    file_fd = open(filename, O_RDONLY);
    if (file_fd < 0) {
        write_message(2, "worker open failed\n");
        _exit(1);
    }

    while (1) {
        message_t msg;
        ssize_t r;

        r = read_all(read_fd, &msg, sizeof(msg));
        if (r == 0) {
            // Dspatcher closed the pipe so exit
            break;
        }

        if (r != (ssize_t)sizeof(msg)) {
            break;
        }

        if (msg.type == CMD_TERMINATE_WORKER) {
            break;
        }

        if (msg.type == CMD_ASSIGN_WORK) {
            message_t reply;
            int cnt;

            cnt = count_in_chunk(file_fd, msg.offset, msg.length, msg.target);

            reply.type = CMD_WORK_RESULT;
            reply.value = 0;
            reply.pid = getpid();
            reply.offset = msg.offset;
            reply.length = msg.length;
            reply.target = msg.target;
            reply.job_id = msg.job_id;

            if (cnt < 0) {
                reply.count = -1;
                reply.value = errno;
            } else {
                reply.count = cnt;
            }

            if (write_all(write_fd, &reply, sizeof(reply)) != (ssize_t)sizeof(reply)) {
                break;
            }
        }
    }

    close(file_fd);
    close(read_fd);
    close(write_fd);

    return 0;
}
