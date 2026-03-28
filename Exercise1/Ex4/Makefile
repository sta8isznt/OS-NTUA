all: frontend dispatcher

CC = gcc
CFLAGS = -Wall -I../ -O2
UTILS_DIR = ..
UTILS_SRC = $(UTILS_DIR)/utils.c
UTILS_HDR = $(UTILS_DIR)/utils.h
#CFLAGS = -O2

frontend: frontend.o utils.o
	$(CC) $(CFLAGS) $^ -o $@

dispatcher: dispatcher.o utils.o
	$(CC) $(CFLAGS) $^ -o $@

utils.o: $(UTILS_SRC) $(UTILS_HDR)
	$(CC) $(CFLAGS) -c $(UTILS_SRC) -o $@

clean:
	rm -f *.o frontend
