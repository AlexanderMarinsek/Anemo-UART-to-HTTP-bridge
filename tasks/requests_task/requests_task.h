#ifndef REQUESTS_TASK_H
#define REQUESTS_TASK_H

#include "../../fifo/fifo.h"

#include <stdio.h> /* printf, sprintf */
#include <stdlib.h> /* exit */
#include <unistd.h> /* read, write, close */
#include <string.h> /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h> /* struct hostent, gethostbyname */
#include <time.h>


#define REQUESTS_FIFO_BUF_SIZE              (4096)
#define REQUESTS_FIFO_STR_SIZE              (FIFO_STRING_SIZE)



// -- debug
#ifndef DEBUG_REQUESTS
#define DEBUG_REQUESTS 1
#endif

// -- socket states
#define SOCKET_UNKNOWN_HOST -1
#define SOCKET_CLOSED 0
#define SOCKET_CREATED 1
#define SOCKET_CONNECTED 2
#define SOCKET_WRITE_FINISHED 3
#define SOCKET_READ_FINISHED 4
#define SOCKET_CLOSE_PENDING 4

// -- request buffer (actual size is number of entries + 1)
// -- 4096 R, 1 R = 1/4 kB -> 68 h of measurements, 1Mb total space
#define REQUEST_FIFO_BUF_SIZE 4096	//possible number of kept strings in fifo
#define REQUEST_FIFO_STR_SIZE 256	//size of string to be kept in fifo

// -- socket timing and time intervals
#define MAX_SOCKET_TIME 15    // -- max seconds in individual socket state
#define SOCKET_READ_WRITE_INTERVAL 10  // -- seconds between writing and reading

// -- requset, response and other array lengths
#define REQUEST_SIZE 1024
#define RESPONSE_SIZE 1024




// -- FUNS ---------------------------------------------------------------------

#define HOST_ADDR_BUF_LEN       64
#define REQUEST_PATH_LEN        128
#define REQUEST_BUF_LEN         1024
#define DATA_BUF_LEN            256

typedef struct {
    struct sockaddr_in serv_addr;
    char host_addr[HOST_ADDR_BUF_LEN];
    uint16_t portno;
    char request_path[REQUEST_PATH_LEN];
    char request[REQUEST_BUF_LEN];
    char data_buffer[DATA_BUF_LEN];
} request_link_t;

/*  Point outer pointer to local fifo struct and init storage for fifo
 *   p1: pointer to pointer, pointing to fifo struct
 *  return: 0 on success, -1 on error
 */
int8_t requests_task_init_fifo (str_fifo_t **_fifo);

/*  Create socket. Will not try connecting, returns OK, even if host is down.
 *   p1: hostname string
 *   p2: port number
 *  return: 0 on success, -1 on error
 */
int8_t requests_task_init_socket (char *_host, int16_t portno);

int8_t requests_task_run (void);


/* int32_t setup_socket (void)
 *  validate host address, set up server structure
 *  returns:
 *   1 - setup successful
 *   0 - error setting up
 */
int8_t setup_socket(void);

/* int32_t create_socket(void)
 *  select socket protocol and stream, create socket
 *  reset socket state elapsed time timer
 *  returns:
 *   1 - successfully connected
 *   0 - error connecting
 */
int32_t create_socket(void);

/* void connect_socket(void)
 *  connect socket
 *  reset socket state elapsed time timer
 *  exit if maximum socket state elapsed time is reached (close socket)
 */
void connect_socket(void);

/* void write_socket(void)
 *  write request to socket
 *  enable reentry, if writing process gets interrupted
 *  stop, when everything is written using single function call, or
 *   nothing new was written to the socket (no data left to write)
 *  reset socket state elapsed time timer
 *  exit if maximum socket state elapsed time is reached (close socket)
 */
void write_socket(void);

/* void read_socket(void)
 *  read response from socket
 *  minimum time must pass between writing and reading, to accomodate for
 *   slower connections/servers and prevent non-deterministic behaviour
 *  enable reentry, if reading process gets interrupted
 *  stop, when everything is received using single function call, or
 *   nothing new was received on the socket (no data left to read)
 *  reset socket state elapsed time timer
 *  exit if maximum socket state elapsed time is reached (close socket)
 */
int8_t read_socket(void);

/* void close_socket(void)
 *  close socket
 */
void close_socket(void);

/* int setup_request_buffer(void)
 *  set up request data buffer (only for data, not full request!)
 *  returns:
 *   1 - buffer setup successful
 *   0 - error
 */
int32_t setup_request_buffer(void);

/* void dummy_request(void)
 *  Set string for validating responses - in this case the device hash name.
 *  Both arrays have the same length, so null termination is not an issue.
 *   p1: Desired validation string.
 */
void set_write_validation (char *_tmp_validation);

/* void dummy_request(void)
 *  generate dummy request data -> fill buffer
 *  ThingSpeak channel located at: https://thingspeak.com/channels/488607
 *  find fresh IP through the terminal: "nslookup api.thingspeak.com"
 */
void dummy_request(void);

#endif //_REQUESTS_H_
