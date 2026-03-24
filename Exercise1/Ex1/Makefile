all: count_chars

CC = gcc
CFLAGS = -Wall -O2
#CFLAGS =  -O2  

count_chars: count_chars.o utils.o
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<
    
clean:
	rm -f *.o count_chars
