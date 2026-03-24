#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>

void write_message(int fd, const char *message);

ssize_t read_all(int fd, void *buf, size_t count);

ssize_t write_all(int fd, const void *buf, size_t count);

#endif
