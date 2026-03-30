#ifndef WORKER_HELPERS_H
#define WORKER_HELPERS_H

#include <sys/types.h>

int parse_fd(const char *s);
int install_worker_handlers(void);
int open_worker_file(const char *file_path);
int count_in_chunk(int fd, off_t offset, size_t length, unsigned char target, int *out_count);
int worker_should_stop(void);

#endif