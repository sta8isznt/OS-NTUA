all: count_chars fork_example count_chars_with_child execve_example parallel_counting

CC = gcc
CFLAGS = -Wall -O2
#CFLAGS =  -O2  

parallel_counting: parallel_counting.o
	$(CC) $(CFLAGS) $^ -o $@

execve_example: execve_example.o
	$(CC) $(CFLAGS) $^ -o $@

count_chars_with_child: count_chars_with_child.o utils.o
	$(CC) $(CFLAGS) $^ -o $@

fork_example: fork_example.o utils.o
	$(CC) $(CFLAGS) $^ -o $@

count_chars: count_chars.o utils.o
	$(CC) $(CFLAGS) $^ -o $@

count_chars_with_child.o: count_chars_with_child.c utils.h
	$(CC) $(CFLAGS) -c $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<
    
clean:
	rm -f *.o count_chars fork_example count_chars_with_child execve_example parallel_counting
