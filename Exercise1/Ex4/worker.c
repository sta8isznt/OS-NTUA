#define _XOPEN_SOURCE 700
#define _FILE_OFFSET_BITS 64

#include "protocol.h"
#include "utils.h"
#include "worker_helpers.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int file_fd;
    int read_fd;
    int write_fd;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <file_path> <read_fd> <write_fd>\n", argv[0]);
        return EXIT_FAILURE;
    }

    read_fd = parse_fd(argv[2]);
    write_fd = parse_fd(argv[3]);

    if (read_fd < 0 || write_fd < 0) {
        fprintf(stderr, "Invalid pipe file descriptor(s)\n");
        return EXIT_FAILURE;
    }

    if (install_worker_handlers() < 0) {
        perror("worker sigaction");
        return EXIT_FAILURE;
    }

    file_fd = open_worker_file(argv[1]);
    if (file_fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    while (!worker_should_stop()) {
        message_t msg;
        ssize_t r;

        memset(&msg, 0, sizeof(msg));
        r = read_all(read_fd, &msg, sizeof(msg));

        if (r == 0) {
            break;
        }

        if (r < 0) {
            if (errno == EINTR && worker_should_stop()) {
                break;
            }
            perror("read_all");
            break;
        }

        if ((size_t)r < sizeof(msg)) {
            break;
        }

        if (msg.type == CMD_ASSIGN_WORK) {
            int cnt = 0;
            int rc;
            message_t reply;

            rc = count_in_chunk(file_fd, msg.offset, msg.length,
                                (unsigned char)msg.target, &cnt);

            memset(&reply, 0, sizeof(reply));
            reply.type = CMD_WORK_RESULT;
            reply.pid = getpid();
            reply.job_id = msg.job_id;
            reply.offset = msg.offset;
            reply.length = msg.length;
            reply.target = msg.target;

            if (rc == 0) {
                reply.count = cnt;
                reply.value = 0;
            } else {
                reply.count = -1;
                reply.value = errno;
            }

            if (write_all(write_fd, &reply, sizeof(reply)) < 0) {
                if (errno == EINTR && worker_should_stop()) {
                    break;
                }
                perror("write_all");
                break;
            }

            if (rc != 0) {
                break;
            }
        }
        else if (msg.type == CMD_TERMINATE_WORKER || msg.type == CMD_SHUTDOWN) {
            break;
        }
    }

    close(file_fd);
    close(read_fd);
    close(write_fd);

    return EXIT_SUCCESS;
}