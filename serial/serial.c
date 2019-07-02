
#include "serial.h"
#include "../fifo/fifo.h"

#include <stdio.h>          /* Standard input/output definitions */
#include <unistd.h>         /* UNIX standard function definitions */
#include <fcntl.h>          /* File control definitions */
#include <termios.h>        /* POSIX terminal control definitions */
#include <stdint.h>         /* Data types */
#include <sys/signal.h>     /* UART interrupt */
#include <string.h>         /* For memset */
//#include <errno.h>          /* Error number definitions */


/* LOCALS *********************************************************************/

/* File descriptor for the port */
int fd;
/* Create signal handler structure */
struct sigaction saio;

/* Length of received data */
int rx_length = 0;
/* Received serial data, copied on interrupt to raw buffer */
char rx_buffer[RAW_FIFO_STRING_SIZE];

/* Fifo for raw serial data */
static str_fifo_t fifo = {
	0,
	0,
	SERIAL_FIFO_BUFFER_SIZE,
	RAW_FIFO_STRING_SIZE,
	NULL
};


/* PROTOTYPES *****************************************************************/

static int8_t _open_port(void);
static int8_t _set_up_port (void);
static int8_t _add_signal_handler_IO (void);
static void signal_handler_IO (int status);


/* FUNCTIONS ******************************************************************/

/*  Init raw serial data fifo.
 */
int8_t serial_init_fifo(str_fifo_t **_fifo){
    printf("Address: %p\n", (void *)_fifo);
    printf("Address: %p\n", (void *)*_fifo);
    printf("Address: %p\n", &fifo);
    *_fifo = &fifo;
    setup_str_fifo(&fifo, SERIAL_FIFO_BUFFER_SIZE, RAW_FIFO_STRING_SIZE);
    //printf("%d, %d\n", _fifo->write_idx, _fifo->read_idx);
    printf("Address: %p\n", (void *)_fifo);
    printf("Address: %p\n", (void *)*_fifo);
    printf("Address: %p\n", &fifo);
    return 0;
}

/*  Init serial port.
 */
int8_t serial_init_port (void) {
    _open_port();
    if (fd == -1) {
        return -1;
        printf("Error: _open_port");
    }
    _set_up_port();
    _add_signal_handler_IO();
    return 0;
}


/*  Set up port.
 */
int8_t _set_up_port (void) {
    /* Init options structure and get currently applied set of options */
    struct termios options;
    tcgetattr(fd, &options);

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

    /* Disable XON/XOFF flow control both i/p and o/p */
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    /* Non Cannonical mode */
    options.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG);
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
    fd = open(PORT_PATHNAME, O_RDONLY | O_NOCTTY | O_NDELAY);

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


/*  Signal handler.
 */
void signal_handler_IO (int status)
{
    /* Clear temporary string buffer */
    memset(rx_buffer, 0, RAW_FIFO_STRING_SIZE);
    /* Read incoming to temporary string buffer */
	rx_length = read(fd, (void*)rx_buffer, RAW_FIFO_STRING_SIZE-1);
    /* Write to buffer */
    str_fifo_write(&fifo, rx_buffer);
}
