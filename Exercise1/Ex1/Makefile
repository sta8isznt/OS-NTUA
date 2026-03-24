all: count_chars

CC = gcc
CFLAGS = -Wall -I../ -O2
#CFLAGS =  -O2  

count_chars: count_chars.o utils.o
	$(CC) $(CFLAGS) $^ -o $@

count_chars.o: count_chars.c ../utils.h
	$(CC) $(CFLAGS) -c $<

utils.o: ../utils.c ../utils.h
	$(CC) $(CFLAGS) -c ../utils.c -o $@

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<
    
clean:
	rm -f *.o count_chars
