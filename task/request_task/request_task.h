#ifndef REQUEST_TASK_H
#define REQUEST_TASK_H

/*
 *  Task for issuing HTTP requests. Non blocking, asynchronous pooling based.
 *  All states are handled internal, as is fifo and other buffer allocation.
 *
 *  Includes error reporting (printf - error code and verbose).
 *
 *	Useful links:
 *		http://www-numi.fnal.gov/offline_software/srt_public_context/WebDocs/Errors/unix_system_errors.html
 */


#include "../../fifo/fifo.h"

#include <stdio.h> 			/* printf, sprintf */
#include <stdint.h> 		/* data types */
#include <stdlib.h> 		/* size_t */
#include <sys/types.h>		/* ssize_t */
#include <unistd.h> 		/* read, write, close */
#include <string.h> 		/* memcpy, memset */
#include <sys/socket.h> 	/* socket, connect */
#include <netinet/in.h> 	/* struct sockaddr_in, struct sockaddr */
#include <netdb.h> 			/* struct hostent, gethostbyname */
#include <fcntl.h>			/* File (socket) control - used for setting async */
#include <errno.h>			/* Socket error reporting */


#ifndef DEBUG_REQUEST
#define DEBUG_REQUEST (0)
#endif

/* Socket state codes */
#define SOCKET_STATE_UNKNOWN_HOST 		-1
#define SOCKET_STATE_IDLE				0
#define SOCKET_STATE_CREATE				1
#define SOCKET_STATE_CONNECT			2
#define SOCKET_STATE_ADD_DATA			3
#define SOCKET_STATE_WRITE				4
#define SOCKET_STATE_READ				5
#define SOCKET_STATE_EVAL_RESPONSE		6
#define SOCKET_STATE_CLOSE				7

#define SOCKET_ERROR					-1
#define SOCKET_CHANGE_STATE				0
#define SOCKET_NO_CHANGE				1
#define SOCKET_IDLE						2

/* Max seconds in individual socket state */
//#define SOCKET_MAX_ALLOWED_STATE_TIME_S		15
#define SOCKET_MAX_STATE_TIME_S				15
#define SOCKET_RETRY_STATE_TIME_S			3


/* Request buffer (actual size is number of entries + 1)
 * 4096 R, 1 R = 1/2 kB -> 68 h of measurements, 2Mb total space
 */
/* Possible number of kept strings in fifo */
#define REQUEST_FIFO_BUF_SIZE              (4096)
/* Size of string to be kept in fifo */
#define REQUEST_FIFO_STR_SIZE              (FIFO_STRING_SIZE)


#define REQUEST_FMT                        					\
    "POST /measurement HTTP/1.1\r\n" 						\
    "Host: %s\r\n" 											\
    "Content-Type: application/json; charset=utf-8\r\n" 	\
    "Content-Length: %lu\r\n\r\n" 							\
    "%s\r\n\r\n"

/* Requset, request data and response buffer sizes */
#define REQUEST_BUF_SIZE 				1024
#define REQUEST_DATA_BUF_SIZE 			1024
#define RESPONSE_BUF_SIZE 				4096

/* Host addres buffer size */
#define HOST_ADDR_BUF_SIZE       		64



/*  Point outer pointer to local fifo struct and init storage for fifo
 *   p1: pointer to pointer, pointing to fifo struct
 *
 *  return:
 *  	-1: error
 *  	 0: success
 */
int8_t request_task_init_fifo (str_fifo_t **_fifo);

/*  Create socket. Will not try connecting, returns OK, even if host is down.
 *   p1: hostname string
 *   p2: port number
 *
 *  return:
 *  	-1: error
 *  	 0: success
 */
int8_t request_task_init_socket (char *_host, int16_t portno);

/*  Check for data, create and enable socket, write, read and evaluate.
 *
 *  return:
 *  	-1: fatal error
 *  	 0: idle
 *  	 1: busy
 */
int8_t request_task_run (void);



#endif
