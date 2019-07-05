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
#include "tasks/buffer_task/buffer_task.h"
#include "tasks/storage_task/storage_task.h"
#include "tasks/requests_task/requests_task.h"

#include <stdio.h>      /* Standard input/output definitions */
#include <unistd.h>     /* Sleep */


#define MEASUREMENTS_FILENAME               "./measurements/data.json"
#define SERIAL_PORTNAME                     "/dev/ttyACM0"
#define SERVER_HOSTNAME                     "127.0.0.1"
#define SERVER_PORT                         (5761)

#define USLEEP_AMOUNT                       (3*1000*1000)


/* Pointer to three fifo buffers
 *  1: Raw incoming UART data
 *  2: Data storage buffer
 *  3: Requests buffer
 */
str_fifo_t *fifo_buffers[3];

int8_t system_idle = 0;


int main () {

    /* Init serial fifo */
    if (serial_init_fifo(&fifo_buffers[0]) != 0) {
        printf("Error: serial_init_fifo");
        return -1;
    }
    /* Init serial port */
    if (serial_init_port(SERIAL_PORTNAME) != 0) {
        printf("Error: serial_init_port");
        return -1;
    }

    /* Init data storage fifo */
    if (storage_task_init_fifo(&fifo_buffers[1]) != 0) {
        printf("Error: serial_init_fifo");
        return -1;
    }
    /* Init data storage file */
    if (storage_task_init_file(MEASUREMENTS_FILENAME) != 0) {
        printf("Error: serial_init_port");
        return -1;
    }

    /* Init requests fifo */
    if (requests_task_init_fifo(&fifo_buffers[2]) != 0) {
        printf("Error: requests_task_init_fifo");
        return -1;
    }
    /* Init requests socket */
    if (requests_task_init_socket(SERVER_HOSTNAME, SERVER_PORT) != 0) {
        printf("Error: requests_task_init_host_and_port");
        return -1;
    }

    /* Last of all! */
    buffer_task_init(fifo_buffers);
    printf("Address: %p\n", (void *)fifo_buffers[0]);

    int8_t is_sys_idle;
    int8_t task_status;

    //char data[512];
    while (1) {
    	is_sys_idle = 0;

        task_status = buffer_task_run();
        task_status = storage_task_run();

        task_status = requests_task_run();

        if (task_status == -1) {
        	printf("FATAL ERROR\n");
        	return -1;
        }
        is_sys_idle += task_status;

        printf("STATUS: %d | %d\n", task_status, is_sys_idle);

        if (is_sys_idle == 0) {
            printf("SI\n");
            usleep(1000000);
        }

        usleep(10000);
        //printf("%d\n", str_fifo_read_auto_inc(fifo_buffers[1], data));
        //printf("%d, %d\n", fifo_buffers[1]->write_idx, fifo_buffers[1]->read_idx);
        //printf ("***%s\n", data);
    }

    return 0;
}
