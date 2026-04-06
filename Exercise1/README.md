# Exercise 1: System Calls, Processes and IPC

Below is a guide of how the exercise programs work and how to run them. The exercise is divided into four parts, each part is a separate directory.

## Exercise 1
The code for this exercise is located in the `Ex1` directory. There are the source code named `count_chars.c` and a `Makefile` to compile the code.

To run the file you should run a command with the following format:

```bash
./count_chars <input_file> <output_file> <character_to_count>
```

The program also uses a helper function `write_message` which is defined in the `utils.c` file. This function is used to write messages to the standard output or standard error.

## Exercise 2
The code for this exercise is located in the `Ex2` directory. There are three source files named `fork_example.c`, `count_chars_with_child.c`, and `execv_example.c`, and a `Makefile` to compile them.

The file `fork_example.c` corresponds to statements 1 and 2 of the problem statement of Exercise 2.

The file `count_chars_with_child.c` corresponds to statement 3 of the problem statement of Exercise 2.

The file `execv_example.c` corresponds to statement 4 of the problem statement of Exercise 2.

To run the `fork_example` program you should use:

```bash
./fork_example
```

To run the `count_chars_with_child` program you should use:

```bash
./count_chars_with_child <input_file> <output_file> <character_to_count>
```

To run the `execv_example` program you should use:

```bash
./execv_example <program_path> <arg1> <arg2> <arg3>
```

For example, if you want to execute `count_chars_with_child` through `execv_example`, you can run:

```bash
./execv_example ./count_chars_with_child <input_file> <output_file> <character_to_count>
```

The programs in this exercise also use helper functions from the `utils.c` file.

## Exercise 3
The code for this exercise is located in the `Ex3` directory. There is a source file named `parallel_counting.c` and a `Makefile` to compile it.

The source file `parallel_counting.c` corresponds to the problem statement of Exercise 3.

To run the program you should use:

```bash
./parallel_counting <input_file> <output_file> <character_to_count>
```

The program creates 4 child processes that each process a different chunk of the input file and communicate their partial counts to the parent through pipes. If you press `Ctrl+C` while it is running, it prints the number of currently active child processes.

The program also uses helper functions from the `utils.c` file.

## Exercise 4
The code for this exercise is located in the `Ex4` directory. There are three source files named `frontend.c`, `dispatcher.c`, and `worker.c`, and a `Makefile` to compile them.

The file `frontend.c` corresponds to the Front-end part of the problem statement of Exercise 4.

The file `dispatcher.c` corresponds to the Dispatcher part of the problem statement of Exercise 4.

The file `worker.c` corresponds to the Worker part of the problem statement of Exercise 4.

To start the application you should run:

```bash
./frontend <input_file> <character_to_count>
```

The `frontend` program creates the `dispatcher` process, and the `dispatcher` creates worker processes to count the occurrences of the target character in the input file.

After the program starts, you can use the following commands from the terminal:

```text
p      -> show the current progress
i      -> show information about active worker processes
a x    -> add x worker processes
r y    -> remove y worker processes
e      -> terminate the application
```

The programs in this exercise use the message format defined in `protocol.h` and helper functions from the `utils.c` file.
