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
#include "storage_task/storage_task.h"

#include <stdio.h>      /* Standard input/output definitions */
#include <unistd.h>     /* Sleep */

/* Pointer to three fifo buffers
 *  1: Raw incoming UART data
 *  2: Data storage buffer
 *  3: Requests buffer
 */
str_fifo_t *fifo_buffers[3];

str_fifo_t *fifooo;

#define MEASUREMENTS_FILENAME               "./measurements/data.json"

int main () {

    /* Init serial device */
    //SerialImp serial;
    serial_init_fifo(&fifo_buffers[0]);
    serial_init_port();

    storage_task_init_fifo(&fifo_buffers[1]);
    storage_task_init_file(MEASUREMENTS_FILENAME);

    /* Last of all! */
    buffer_task_init(fifo_buffers);
    printf("Address: %p\n", (void *)fifo_buffers[0]);

    //char data[512];
    while (1) {
        buffer_task_run();
        //printf("%d\n", str_fifo_read_auto_inc(fifo_buffers[1], data));
        //printf("%d, %d\n", fifo_buffers[1]->write_idx, fifo_buffers[1]->read_idx);
        //printf ("***%s\n", data);
        storage_task_run();
        usleep(1000000);
    }

    return 0;
}
