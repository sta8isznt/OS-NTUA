#
# Makefile
# 

CC = gcc

# CAUTION: Always use '-pthread' when compiling POSIX threads-based
# applications, instead of linking with "-lpthread" directly.
CFLAGS = -Wall -O2 -pthread
LIBS = 

all: mandel mandel-semaphores mandel-cond

## Mandel
mandel: mandel-lib.o mandel.o
	$(CC) $(CFLAGS) -o mandel mandel-lib.o mandel.o $(LIBS)

mandel-semaphores: mandel-lib.o mandel_semaphores.o
	$(CC) $(CFLAGS) -o mandel-semaphores mandel-lib.o mandel_semaphores.o $(LIBS)

mandel-cond: mandel-lib.o mandel_cond.o
	$(CC) $(CFLAGS) -o mandel-cond mandel-lib.o mandel_cond.o $(LIBS)

mandel-lib.o: mandel-lib.h mandel-lib.c
	$(CC) $(CFLAGS) -c -o mandel-lib.o mandel-lib.c $(LIBS)

mandel.o: mandel.c
	$(CC) $(CFLAGS) -c -o mandel.o mandel.c $(LIBS)

mandel_semaphores.o: mandel_semaphores.c
	$(CC) $(CFLAGS) -c -o mandel_semaphores.o mandel_semaphores.c $(LIBS)

mandel_cond.o: mandel_cond.c
	$(CC) $(CFLAGS) -c -o mandel_cond.o mandel_cond.c $(LIBS)

clean:
	rm -f *.s *.o pthread-test simplesync-atomic simplesync-mutex simplesync-withoutsync mandel mandel-semaphores mandel-cond
