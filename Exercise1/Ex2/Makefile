all: fork_example count_chars_with_child execv_example

CC = gcc
CFLAGS = -Wall -I../ -O2
#CFLAGS =  -O2  

fork_example: fork_example.o utils.o
	$(CC) $(CFLAGS) $^ -o $@

count_chars_with_child: count_chars_with_child.o utils.o
	$(CC) $(CFLAGS) $^ -o $@

execv_example: execv_example.o utils.o
	$(CC) $(CFLAGS) $^ -o $@

fork_example.o: fork_example.c ../utils.h
	$(CC) $(CFLAGS) -c $<

count_chars_with_child.o: count_chars_with_child.c ../utils.h
	$(CC) $(CFLAGS) -c $<

execv_example.o: execv_example.c ../utils.h
	$(CC) $(CFLAGS) -c $<

utils.o: ../utils.c ../utils.h
	$(CC) $(CFLAGS) -c ../utils.c -o $@

clean:
	rm -f *.o fork_example count_chars_with_child execv_example
