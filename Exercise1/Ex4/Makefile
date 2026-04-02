CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I..

UTILS = ../utils.o

all: frontend dispatcher worker

frontend: frontend.o $(UTILS)
	$(CC) $(CFLAGS) -o frontend frontend.o $(UTILS)

dispatcher: dispatcher.o $(UTILS)
	$(CC) $(CFLAGS) -o dispatcher dispatcher.o $(UTILS)

worker: worker.o $(UTILS)
	$(CC) $(CFLAGS) -o worker worker.o $(UTILS)

frontend.o: frontend.c protocol.h ../utils.h
	$(CC) $(CFLAGS) -c frontend.c

dispatcher.o: dispatcher.c protocol.h ../utils.h
	$(CC) $(CFLAGS) -c dispatcher.c

worker.o: worker.c protocol.h ../utils.h
	$(CC) $(CFLAGS) -c worker.c

../utils.o: ../utils.c ../utils.h
	$(CC) $(CFLAGS) -c ../utils.c -o ../utils.o

clean:
	rm -f *.o frontend dispatcher worker
	rm -f ../utils.o