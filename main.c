/*
 *  Useful links:
 *   POSIX UART: https://www.cmrr.umn.edu/~strupp/serial.html
 *   File/port open: http://man7.org/linux/man-pages/man2/open.2.html
 *
 *
 *
 */

 #include "fifo/fifo.h"
#include "serial/serial.h"
#include "buffer_task/buffer_task.h"

#include <stdio.h>      /* Standard input/output definitions */
#include <unistd.h>     /* Sleep */

/* Pointer to three fifo buffers
 *  1: Raw incoming UART data
 *  2: Local storage buffer
 *  3: Requests buffer
 */
str_fifo_t *fifo_buffers[3];

str_fifo_t *fifooo;

int main () {

    /* Init serial device */
    //SerialImp serial;
    serial_init_fifo(&fifo_buffers[0]);
    serial_init_port();

    /* Last of all! */
    buffer_task_init(fifo_buffers);
    printf("Address: %p\n", (void *)fifo_buffers[0]);

    while (1) {
        buffer_task_run();
        usleep(1000000);
    }

    return 0;
}
