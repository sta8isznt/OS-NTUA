#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <sys/types.h>

typedef enum {
    // Frontend -> Dispatcher
    CMD_ADD_WORKER,
    CMD_REMOVE_WORKER,
    CMD_INFO,
    CMD_PROGRESS,
    CMD_SHUTDOWN,

    // Dispatcher -> Worker
    CMD_ASSIGN_WORK,
    CMD_TERMINATE_WORKER,

    // Worker -> Dispatcher
    CMD_WORK_RESULT
} msg_type_t;

typedef struct {
    msg_type_t type;
    int value;      // For add, remove commands
    pid_t pid;
    off_t offset;   // For dispatcher -> worker commands
    size_t length;
    char target;
    int count;      // For results from workers
    int job_id;     // Identity of assigned chunk
} message_t;

#endif