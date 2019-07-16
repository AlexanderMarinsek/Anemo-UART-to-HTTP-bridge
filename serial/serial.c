
#include "serial.h"
#include "../fifo/fifo.h"

#include <stdio.h>          /* Standard input/output definitions */
#include <unistd.h>         /* UNIX standard function definitions */
#include <fcntl.h>          /* File control definitions */
#include <termios.h>        /* POSIX terminal control definitions */
#include <stdint.h>         /* Data types */
#include <sys/signal.h>     /* UART interrupt */
#include <string.h>         /* For memory operations */
//#include <errno.h>          /* Error number definitions */


/* LOCALS *********************************************************************/

/* Fifo for raw serial data */
static str_fifo_t serial_raw_fifo = {
	0,
	0,
	SERIAL_FIFO_BUFFER_SIZE,
	RAW_FIFO_STRING_SIZE,
	NULL
};

/* File descriptor for the port */
int fd;
/* Absolute path to port */
static char portname[PORTNAME_STRING_LEN];

/* Length of received data */
int rx_length = 0;
/* Received serial data, copied on interrupt to raw buffer */
char rx_buffer[RAW_FIFO_STRING_SIZE];

/* Create signal handler structure */
struct sigaction saio;


/* PROTOTYPES *****************************************************************/

static int8_t _open_port(void);
static int8_t _set_up_port (void);
static int8_t _add_signal_handler_IO (void);
static int8_t _set_portname (char *_portname);
static void signal_handler_IO (int status);


/* FUNCTIONS (GLOBAL) *********************************************************/

/*  Init raw serial data serial_raw_fifo.
 */
int8_t serial_init_fifo(str_fifo_t **_fifo){
    *_fifo = &serial_raw_fifo;
    setup_str_fifo(&serial_raw_fifo, SERIAL_FIFO_BUFFER_SIZE, RAW_FIFO_STRING_SIZE);
    return 0;
}

/*  Init serial port.
 */
int8_t serial_init_port (char *_portname) {
	int8_t error_control = 0;
	error_control += _set_portname (_portname);
	error_control += _open_port();
	error_control += _set_up_port();
	error_control += _add_signal_handler_IO();
    return error_control;
}

/*  Open serial port.
 */
int8_t serial_open_port (void) {
	int8_t error_control = 0;
	error_control += _open_port();
	error_control += _set_up_port();
    return error_control;
}


/* SIGNAL HANDLER *************************************************************/
void signal_handler_IO (int status)
{
    /* Clear temporary string buffer */
    memset(rx_buffer, 0, RAW_FIFO_STRING_SIZE);
    /* Read incoming to temporary string buffer */
	rx_length = read(fd, (void*)rx_buffer, RAW_FIFO_STRING_SIZE-1);
    /* Write to buffer */
    str_fifo_write(&serial_raw_fifo, rx_buffer);
    /*printf("serial handler - serial_raw_fifo:\n"
    		"%u\n"
    		"%u\n",
			serial_raw_fifo.read_idx,
			serial_raw_fifo.write_idx);
    printf("%s\n", rx_buffer);*/

}


/* FUNCTIONS (LOCAL) **********************************************************/

/* Save local copy of portname */
static int8_t _set_portname (char *_portname) {
    /* Check length */
    if (strlen(_portname) > PORTNAME_STRING_LEN-1) {
        printf("Error: storage_task_init_file\n");
        return -1;
    }
	/* Copy to local string */
    memcpy(portname, _portname, strlen(_portname)+1);
	return 0;
}

/*  Set up port.
 */
int8_t _set_up_port (void) {
    /* Init options structure and get currently applied set of options */
    struct termios options;
    tcgetattr(fd, &options);

    /* CONTROL options */
    /* Set baudrate */
    options.c_cflag = B115200;
    /* Clears the mask for setting the data size */
    options.c_cflag &= ~CSIZE;
    /* CSTOPB would set 2 stop bits, here it is cleared so 1 stop bit */
    options.c_cflag &= ~CSTOPB;
    /* Disables the parity enable bit(PARENB), so no parity */
    options.c_cflag &= ~PARENB;
    /* No hardware flow Control */
    options.c_cflag &= ~CRTSCTS;
    /* Set the data bits to 8 */
    options.c_cflag |=  CS8;
    /* Do not change "owner" of port (ignore modem control lines) */
    options.c_cflag |= CLOCAL;
    /* Enable receiver */
    options.c_cflag |= CREAD;

	/* INPUT options */
    /* Disable XON/XOFF flow control both i/p and o/p */
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    /* LINE options */
    /* Non cannonical, non-echo mode */
    //options.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    /* OUTPUT options */
    /* No Output Processing */
    options.c_oflag &= ~OPOST;

    /* Set terminal options using the file descriptor */
    tcsetattr(fd, TCSANOW, &options);

    return 0;
}


/*  Open the desired port.
 */
static int8_t _open_port(void) {

    /*  Open serial port.
    *   O_RDONLY - Only receive data
    *   O_NOCTTY - Leave process control to other 'jobs' for better portability.
    *   O_NDELAY - Enable non-blocking read
    */
    fd = open(portname, O_RDONLY | O_NOCTTY | O_NDELAY);

    /* Catch FD error */
    if (fd == -1) {
        return -1;
    }

    /*  Use the following to switch between blocking/non-blocking:
    *   B: fcntl(fd, F_SETFL, 0);
    *   NB: fcntl(fd, F_SETFL, FNDELAY);
    */

    /* Set process ID, that will receive SIGIO signals for file desc. events */
    fcntl (fd, F_SETOWN, getpid());
    /* Enable generation of signals */
    fcntl (fd, F_SETFL, O_ASYNC);

    return 0;
}


/*  Add input UART signal handler.
 *  return: 0 on success, -1 on error
 */
int8_t _add_signal_handler_IO (void) {
    /* Add UART handler function */
    saio.sa_handler = signal_handler_IO;
    /* Non-zero used for calling sighandler on alternative stacks and so on */
    saio.sa_flags = 0;
    /* Not specified by POSIX -> not in use */
    saio.sa_restorer = NULL;
    /* Bind SIGIO (async I/O signals) to the defined structure */
    int status = sigaction(SIGIO, &saio, NULL);        /* returns 0 / -1 */

    return status;
}
