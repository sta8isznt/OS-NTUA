CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I..

TARGETS = frontend dispatcher worker

OBJS_FRONTEND = frontend.o ../utils.o
OBJS_DISPATCHER = dispatcher.o work_pool.o worker_manager.o ../utils.o
OBJS_WORKER = worker.o worker_helpers.o ../utils.o

.PHONY: all clean

all: $(TARGETS)

frontend: $(OBJS_FRONTEND)
	$(CC) $(CFLAGS) -o frontend $(OBJS_FRONTEND)

dispatcher: $(OBJS_DISPATCHER)
	$(CC) $(CFLAGS) -o dispatcher $(OBJS_DISPATCHER)

worker: $(OBJS_WORKER)
	$(CC) $(CFLAGS) -o worker $(OBJS_WORKER)

frontend.o: frontend.c protocol.h
	$(CC) $(CFLAGS) -c frontend.c

dispatcher.o: dispatcher.c protocol.h work_pool.h worker_manager.h
	$(CC) $(CFLAGS) -c dispatcher.c

work_pool.o: work_pool.c work_pool.h
	$(CC) $(CFLAGS) -c work_pool.c

worker_manager.o: worker_manager.c worker_manager.h protocol.h work_pool.h
	$(CC) $(CFLAGS) -c worker_manager.c

worker.o: worker.c protocol.h worker_helpers.h
	$(CC) $(CFLAGS) -c worker.c

worker_helpers.o: worker_helpers.c worker_helpers.h
	$(CC) $(CFLAGS) -c worker_helpers.c

../utils.o: ../utils.c ../utils.h
	$(CC) $(CFLAGS) -c ../utils.c -o ../utils.o

clean:
	rm -f *.o ../utils.o frontend dispatcher worker