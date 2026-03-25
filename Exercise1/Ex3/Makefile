all: parallel_counting

CC = gcc
CFLAGS = -Wall -I../ -O2
UTILS_DIR = ..
UTILS_SRC = $(UTILS_DIR)/utils.c
UTILS_HDR = $(UTILS_DIR)/utils.h
#CFLAGS = -O2

parallel_counting: parallel_counting.o utils.o
	$(CC) $(CFLAGS) $^ -o $@

parallel_counting.o: parallel_counting.c $(UTILS_HDR)
	$(CC) $(CFLAGS) -c $<

utils.o: $(UTILS_SRC) $(UTILS_HDR)
	$(CC) $(CFLAGS) -c $(UTILS_SRC) -o $@

clean:
	rm -f *.o parallel_counting
