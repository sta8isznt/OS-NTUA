#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <sys/types.h>

/* List of all the possible signal types */
typedef enum {
    // Frontend -> Dispatcher
    CMD_ADD_WORKER,
    CMD_REMOVE_WORKER,
    CMD_INFO,
    CMD_PROGRESS,
    CMD_SHUTDOWN,

    // Dispatcher -> Workers
    CMD_ASSIGN_WORK,
    CMD_WORK_RESULT,
} msg_type_t;

/* struct for messages between modules */
typedef struct {
    msg_type_t type;
    int value;          /* general field (e.g. how many workers to add) */
    pid_t pid;
    off_t offset;       /* file offset */
    size_t length;      /* count of bytes */
    char target;        /* searching character */
    int count;          /* worker result */
    int job_id;         // chunk number
} message_t;

#endif