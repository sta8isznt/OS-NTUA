#
# Makefile
# 

CC = gcc

# CAUTION: Always use '-pthread' when compiling POSIX threads-based
# applications, instead of linking with "-lpthread" directly.
CFLAGS = -Wall -O2 -pthread
LIBS = 

all: simplesync-mutex simplesync-atomic simplesync-withoutsync mandel 

## Simple sync (two versions)
simplesync-mutex: simplesync-mutex.o
	$(CC) $(CFLAGS) -o simplesync-mutex simplesync-mutex.o $(LIBS)

simplesync-atomic: simplesync-atomic.o
	$(CC) $(CFLAGS) -o simplesync-atomic simplesync-atomic.o $(LIBS)

simplesync-withoutsync: simplesync-withoutsync.o
	$(CC) $(CFLAGS) -o simplesync-withoutsync simplesync-withoutsync.o $(LIBS)

simplesync-mutex.o: simplesync.c
	$(CC) $(CFLAGS) -DSYNC_MUTEX -c -o simplesync-mutex.o simplesync.c

simplesync-atomic.o: simplesync.c
	$(CC) $(CFLAGS) -DSYNC_ATOMIC -c -o simplesync-atomic.o simplesync.c

simplesync-withoutsync.o: simplesync-withoutsync.c
	$(CC) $(CFLAGS) -DSYNC_MUTEX -c -o simplesync-withoutsync.o simplesync-withoutsync.c


## Mandel
mandel: mandel-lib.o mandel.o
	$(CC) $(CFLAGS) -o mandel mandel-lib.o mandel.o $(LIBS)

mandel-lib.o: mandel-lib.h mandel-lib.c
	$(CC) $(CFLAGS) -c -o mandel-lib.o mandel-lib.c $(LIBS)

mandel.o: mandel.c
	$(CC) $(CFLAGS) -c -o mandel.o mandel.c $(LIBS)

clean:
	rm -f *.s *.o pthread-test simplesync-atomic simplesync-mutex simplesync-withoutsync mandel
