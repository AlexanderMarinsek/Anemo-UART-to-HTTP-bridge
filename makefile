CC = gcc
CFLAGS = -g -Wall -I.

# -- list of dependencies -> header files
DEPS = 	fifo/fifo.h								\
		timestamp/timestamp.h					\
	    serial/serial.h							\
	    tasks/buffer_task/buffer_task.h			\
		tasks/storage_task/storage_task.h		\
		tasks/requests_task/requests_task.h

# -- list of objet files
OBJ = 	main.o									\
		fifo/fifo.o								\
		timestamp/timestamp.o					\
		serial/serial.o							\
		tasks/buffer_task/buffer_task.o			\
		tasks/storage_task/storage_task.o		\
		tasks/requests_task/requests_task.o

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
