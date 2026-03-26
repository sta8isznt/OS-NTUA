#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>

void write_message(int fd, const char *message);

ssize_t read_all(int fd, void *buf, size_t count);

ssize_t write_all(int fd, const void *buf, size_t count);

void show_pstree(pid_t t);

ssize_t read_until(int fd, char *buf, size_t max_count, char delim);

#endif
