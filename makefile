CC = gcc
CFLAGS = -g -Wall -I.

# -- list of dependencies -> header files
DEPS = 	fifo/fifo.h						\
	    serial/serial.h					\
	    buffer_task/buffer_task.h

# -- list of objet files
OBJ = 	main.o							\
		fifo/fifo.o						\
		serial/serial.o					\
		buffer_task/buffer_task.o

# -- list of phony targets
.PHONY: clean

# -- default command
all: main

# -- make main module
main: $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(EXTRA_LIBS)

# -- object files assembly rule
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

# -- delete .o files in main directory and sub-directories
clean:
	rm *.o */*.o
