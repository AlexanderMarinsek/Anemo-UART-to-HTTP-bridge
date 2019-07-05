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

#define REQUEST_FMT                        \
    "POST /latest HTTP/1.1\r\n" \
    "Host: %s\r\n" \
    "Content-Type: application/json; charset=utf-8\r\n" \
    "Content-Length: %lu\r\n\r\n" \
    "%s\r\n\r\n"
    /*"POST /latest HTTP/1.1\r\n"
    "Host: %s\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Content-Length: %lu\r\n\r\n"
    "%s\r\n\r\n"*/


// -- debug
#ifndef DEBUG_REQUESTS
#define DEBUG_REQUESTS 1
#endif

// -- socket states
#define SOCKET_STATE_UNKNOWN_HOST 		-1
#define SOCKET_STATE_IDLE				0
#define SOCKET_STATE_CREATE				1
#define SOCKET_STATE_CONNECT			2
#define SOCKET_STATE_ADD_DATA			3
#define SOCKET_STATE_WRITE				4
#define SOCKET_STATE_READ				5
#define SOCKET_STATE_EVAL_RESPONSE		6
#define SOCKET_STATE_CLOSE				7



#define SOCKET_STATUS_ERROR			-1
#define SOCKET_STATUS_IDLE			0
#define SOCKET_STATUS_BUSY			1



// -- request buffer (actual size is number of entries + 1)
// -- 4096 R, 1 R = 1/4 kB -> 68 h of measurements, 1Mb total space
#define REQUEST_FIFO_BUF_SIZE 4096	//possible number of kept strings in fifo
#define REQUEST_FIFO_STR_SIZE 256	//size of string to be kept in fifo

// -- socket timing and time intervals
//#define MAX_SOCKET_TIME 15    // -- max seconds in individual socket state
//#define SOCKET_READ_WRITE_INTERVAL 10  // -- seconds between writing and reading

#define SOCKET_MAX_ALLOWED_STATE_TIME_S		15

// -- requset, response and other array lengths
#define REQUEST_BUF_SIZE 	1024
#define RESPONSE_BUF_SIZE 	1024
//#define REQUEST_SIZE 		REQUEST_BUF_SIZE
//#define RESPONSE_SIZE 		RESPONSE_BUF_SIZE




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

int8_t new_re (void);



#endif
