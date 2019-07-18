#include "request_task.h"
#include "../../fifo/fifo.h"
#include "../../timestamp/timestamp.h"

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


/* LOCALS *********************************************************************/

/* Current socket state */
static int8_t socket_state;

/* Socket file descriptor */
static int32_t sockfd;
/* Struct with addres and port */
static struct sockaddr_in serv_addr;

/* Hostname string  */
static char host[HOST_ADDR_BUF_SIZE];

/* Request including headers, body ... */
static char request_buf[REQUEST_BUF_SIZE];
/* Response including headers, body ... */
static char response_buf[RESPONSE_BUF_SIZE];

/* Read/write byte counters */
static ssize_t bytes_sent;
static ssize_t bytes_read;
static ssize_t prev_read_result;

/* GLOBALS ********************************************************************/

/* Bears only the JSON data (request body) */
char request_data_buf[REQUEST_FIFO_STR_SIZE];

/* Fifo for data storage */
str_fifo_t request_fifo = {
	0,
	0,
	REQUEST_FIFO_BUF_SIZE,
	REQUEST_FIFO_STR_SIZE,
	NULL
};

/* Gets written externally. Static to avoid linkage conflicts. */
static char timestamp[TIMESTAMP_RAW_STRING_SIZE];

/* Used to measure socket time in single state */
long int socket_timer_start = 0;


/* PROTOTYPES *****************************************************************/

int8_t (*_get_socket_state_funciton(void))(void) ;

int8_t _idle_socket(void);
int8_t _create_socket(void);
int8_t _connect_socket(void);
int8_t _add_request_data(void);
int8_t _write_socket(void);
int8_t _read_socket(void);
int8_t _evaluate_socket(void);
int8_t _close_socket(void);
int8_t _clear_request_buffers(void);

int8_t _check_fifo_for_new_data (void);

void _report_socket_errno(void);

int8_t _has_socket_timer_ended(void);
void _reset_socket_timer(void);

void _reset_static_vars(void);


/* FUNCTIONS ******************************************************************/

/*  Point outer pointer to local fifo struct and init storage for fifo.
 */
int8_t request_task_init_fifo (str_fifo_t **_fifo) {
    /* Set outer pointer to point to fifo local struct */
    *_fifo = &request_fifo;
    /* Set up memory for fifo struct and return success/error */
    int8_t fifo_status = setup_str_fifo(
        &request_fifo, REQUEST_FIFO_BUF_SIZE, REQUEST_FIFO_STR_SIZE);

#if(DEBUG_REQUEST==1)
	printf("*\tREQUEST FIFO INITIATED\n");
#endif

    return fifo_status;
}


/*  Init socket using host address and port.s
 */
int8_t request_task_init_socket(char *_host, int16_t portno) {

    /* Check hostname length */
    if (strlen(_host) > HOST_ADDR_BUF_SIZE-1) {
    	/* Refresh local timestamp variable and report error */
		get_timestamp_raw(timestamp);
		printf("SOCKET FATAL: HOST ADDRESS TOO LONG | %s\n", timestamp);
        return -1;
    }

	/* Copy hostname to local string */
    memcpy(host, _host, strlen(_host)+1);

	/* Server data */
    struct hostent *server;
    /* Get server data by domain name, or IPv4 */
    server = gethostbyname(host);

	if (server == NULL) {
		socket_state = SOCKET_STATE_UNKNOWN_HOST;
    	/* Refresh local timestamp variable and report error */
		get_timestamp_raw(timestamp);
		printf("SOCKET FATAL: UNKNOWN HOST | %s\n", timestamp);
		return -1;
	}

    /* Initialize server */
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    memcpy(&serv_addr.sin_addr.s_addr,server->h_addr,server->h_length);

    _reset_socket_timer();

    /* Set socket state variable */
	socket_state = SOCKET_STATE_IDLE;

#if(DEBUG_REQUEST==1)
	printf("*\tSOCKET INITIATED\n");
#endif

    return 0;
}


/*  Check for data, create and enable socket, write, read and evaluate.
 */
int8_t request_task_run(void) {
	//printf("REQUEST TASK\n");
	/* Declare function pointer, that is: int8_t myFun (void) {...} */
    int8_t (*state_fun_ptr) (void);
    /* Get pointer with regard to current socket state, accepts no arguments */
    state_fun_ptr = _get_socket_state_funciton();

    /* Call socket state function and save status output */
    int8_t status = state_fun_ptr();

    /* Close socket if timer has elepsed */
    if (_has_socket_timer_ended() == 0) {
    	printf("SOCKET TIME ELAPSED\n");
    	_report_socket_errno();
        socket_state = SOCKET_STATE_CLOSE;
        return SOCKET_STATUS_BUSY;
    }

    /* Fatal error, quit execution */
    if (status == -1) {
    	return SOCKET_STATUS_ERROR;
    }
    /* State finished, switching to new one */
    if (status == 0) {
    	_reset_socket_timer();		/* Reset timer on change */
    	return SOCKET_STATUS_BUSY;
    }
    /* State busy */
    if (status == 1) {
    	return SOCKET_STATUS_BUSY;
    }
    /* Idle operation, can go to sleep */
    if (status == 2) {
    	_reset_socket_timer();		/* Idle is not time bound */
    	return SOCKET_STATUS_IDLE;
    }
    /* Unknow status */
    else {
    	printf("Unknown socket status\n");
    	return SOCKET_STATUS_ERROR;
    }
}


/* 	Get socket state function pointer with regard to current socket state.
 *  Second '(void)' means it takes no arguments.
 *
 *	return:
 *		function pointer of type:	int8_t myFun (void) {...}
 */
int8_t (*_get_socket_state_funciton(void))(void) {
    int8_t (*state_fun_ptr) (void);

#if(DEBUG_REQUEST==1)
    printf("Socket state: %d\n", socket_state);
#endif

    switch (socket_state) {
    case SOCKET_STATE_IDLE:
    	state_fun_ptr = &_idle_socket;
        break;
    case SOCKET_STATE_CREATE:
    	state_fun_ptr = &_create_socket;
    	break;
	case SOCKET_STATE_CONNECT:
    	state_fun_ptr = &_connect_socket;
		break;
	case SOCKET_STATE_ADD_DATA:
    	state_fun_ptr = &_add_request_data;
		break;
    case SOCKET_STATE_WRITE:
    	state_fun_ptr = &_write_socket;
        break;
    case SOCKET_STATE_READ:
    	state_fun_ptr = &_read_socket;
        break;
    case SOCKET_STATE_EVAL_RESPONSE:
    	state_fun_ptr = &_evaluate_socket;
        break;
    case SOCKET_STATE_CLOSE:
    	state_fun_ptr = &_close_socket;
        break;
    default:
    	state_fun_ptr = &_close_socket;
        break;
    }

    return state_fun_ptr;
}


/* 	Check, if fifo contains new request data.
 *
 *  Next state:
 *  	SOCKET_STATE_CREATE
 *
 *  return:
 *  	-1: error
 *		 0: new data available
 *		 2: idle
 */
int8_t _idle_socket(void) {
	if (_check_fifo_for_new_data() == 0) {
#if(DEBUG_REQUEST==1)
		printf("*\tSOCKET FIFO DATA DETECTED\n");
#endif
        socket_state = SOCKET_STATE_CREATE;
        return 0;
	}
	return 2;
}


/* Select socket protocol and stream, create socket.
 *
 *  Next state:
 *  	SOCKET_STATE_CONNECT
 *
 *  returns:
 *  	-1: error creating
 *		 0: successfully creted
 */
int8_t _create_socket(void) {

    /* Like 'open()' for files
     *  AF_INET - IPv4
     *  SOCK_STREAM - Provides sequenced, reliable, two-way streams
     *  0 - default protocol selector
     */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

#if(DEBUG_REQUEST==1)
	printf("socket(): %d\n", sockfd);
    printf("errno: %d | %s\n", errno, strerror(errno));
#endif

    /* Catch error, which is not EINPROGRESS.
     * Normal: EINPROGRESS is thrown in non blocking operations
     */
    if (sockfd == -1 && errno != EINPROGRESS) {
		_report_socket_errno();
        socket_state = SOCKET_STATE_CLOSE;
        return 0;
    }

    /* Catch refused error, because socket() returns positive on refused. */
    if (errno == ECONNREFUSED) {
		_report_socket_errno();
        socket_state = SOCKET_STATE_CLOSE;
        return 0;
    }

    /* Set to non blocking */
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    /* Set socket state variable */
    socket_state = SOCKET_STATE_CONNECT;

#if(DEBUG_REQUEST==1)
	printf("*\tSOCKET CREATED\n");
#endif

    return 0;
}


/*	Connect socket to defined address.
 *
 *  Next state:
 *  	SOCKET_STATE_ADD_DATA
 *
 *  returns:
 *  	-1: error connecting
 *		 0: successfully connected
 */
int8_t _connect_socket(void){

    /* Connect the socket to the previously defined address */
	int connected =
		connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

#if(DEBUG_REQUEST==1)
	printf("connect(): %d\n", connected);
    printf("errno: %d | %s\n", errno, strerror(errno));
#endif

    /* Catch error, which is not EINPROGRESS. */
    if (connected == -1 && errno != EINPROGRESS) {
		_report_socket_errno();
        socket_state = SOCKET_STATE_CLOSE;
        return 0;
    }

    /* Set socket state variable */
	socket_state = SOCKET_STATE_ADD_DATA;

#if(DEBUG_REQUEST==1)
	printf("*\tSOCKET CONNECTED\n");
#endif

  return 0;
}


/*	Take data from fifo and copy to request buffer.
 *
 *  Next state:
 *  	SOCKET_STATE_WRITE
 *
 *  returns:
 *  	-1: error
 *		 0: success
 */
int8_t _add_request_data(void) {
	/* Reset read/write byte counters */
	_reset_static_vars();
	/* Clear all buffers */
    _clear_request_buffers();
    /* Get row of data from fifo buffer */
    str_fifo_read(&request_fifo, request_data_buf);
    /* Add request data to request buffer */
    sprintf(request_buf, REQUEST_FMT,
		host, (long unsigned int)strlen(request_data_buf), request_data_buf);
    /* Set socket state variable */
    socket_state = SOCKET_STATE_WRITE;

#if(DEBUG_REQUEST==1)
	printf("\tADDED REQUEST DATA (%lu):\n%s\n",
		strlen(request_buf), request_buf);
#endif
    return 0;
}


/*	Take request buffer and write it to the socket.
 *
 *  Next state:
 *  	SOCKET_STATE_READ
 *
 *  returns:
 *  	-1: error
 *		 0: finished
 *		 1: still writing
 */
int8_t _write_socket(void) {
    //static ssize_t bytes_sent = 0;
    /* Only write the bytes containing data (string) */
    ssize_t request_len = strlen(request_buf);

    /* Write and get amount of bytes, that were written
     * 	-1: can't write
     * 	 0: nothing to write
     * 	>0: number of bytes written
     */
    size_t result =
		write(sockfd, request_buf + bytes_sent, request_len - bytes_sent);

#if(DEBUG_REQUEST==1)
	printf("write(): %ld, %ld, %ld\n", result, bytes_sent, request_len);
    printf("errno: %d | %s\n", errno, strerror(errno));
#endif

	/* Normal: EINPROGRESS is thrown in non blocking operations */
	if (result == -1 && errno != EINPROGRESS) {
		_report_socket_errno();
        socket_state = SOCKET_STATE_CLOSE;
		return 0;
	}

    /* Increment bytes_read ('request_buf' idx pointer) */
	bytes_sent += result;

    /* Finished writing (writen everything, nonthing else left) */
    if (request_len == bytes_sent || result == 0) {
#if(DEBUG_REQUEST==1)
    	printf("*\tREQUEST WRITTEN (%lu): \n%s\n",
			request_len, request_buf);
#endif
		/* Reset static vars */
    	//bytes_sent = 0;
        /* Set socket state variable */
        socket_state = SOCKET_STATE_READ;
        return 0;
    }

    return 1;
}


/*	Read from socket and write to response buffer.
 * 	Wait for a full response.
 *
 *  Next state:
 *  	SOCKET_STATE_EVAL_RESPONSE
 *
 * 	return:
 *  	-1: error
 *		 0: finished
 *		 1: still reading
 */
int8_t _read_socket(void) {
	/* Read bytes amount, or 'response_buf' write ptr */
    //static ssize_t bytes_read = 0;
    /* Previous amount of bytes, that were read */
    //static ssize_t prev_read_result = 0;

    /* Read and get amount of bytes, that were read
     * 	-1: nothing new
     * 	 0: can't access read data
     * 	>0: number of bytes read
     */
    ssize_t result =
    		read(sockfd, response_buf + bytes_read, RESPONSE_BUF_SIZE - bytes_read);

#if(DEBUG_REQUEST==1)
	printf("read(): %ld, %ld, %d\n", result, bytes_read, RESPONSE_BUF_SIZE);
	printf("read(): \n%s\n", response_buf);
    printf("errno: %d | %s\n", errno, strerror(errno));
//	printf("%d | %d | %d | %d | %d | %d | %d | %d \n",
//		EAGAIN, EWOULDBLOCK, EBADF, EFAULT, EINTR, EINVAL, EIO, EISDIR);
#endif

    /* Check for socket error */
    //if (errno == ENOTCONN || errno == EBADF)
    //if (result == 0) {
    if (result == 0 && errno != EINPROGRESS) {
		_report_socket_errno();
		/* Reset static vars */
		//bytes_read = 0;
		//prev_read_result = 0;
        socket_state = SOCKET_STATE_CLOSE;
        return 0;
    }

    /* If read successfull, increment bytes_read ('response_buf' idx pointer) */
    if (result > 0) {
        bytes_read += result;
    }

    /* Reached end of buffer */
    if (bytes_read == RESPONSE_BUF_SIZE-1) {
    	/* Refresh local timestamp variable and report error */
		get_timestamp_raw(timestamp);
		printf("SOCKET FATAL: RESPONSE BUFFER TOO LONG | %s\n", timestamp);
        //socket_state = SOCKET_STATE_CLOSE;
	    return -1;
	}

#if(DEBUG_REQUEST==1)
    printf("Previous, current, total read(): %ld, %ld, %ld\n",
		prev_read_result, result, bytes_read);
#endif

	/* Check for end of response */
    if (prev_read_result > 0) {		/* Previously read something */
    	if (result == -1) {		/* Nothing new was read */
#if(DEBUG_REQUEST==1)
			printf("*\tRESPONSE RECEIVED (%ld):\n%s\n",
				bytes_read, response_buf);
#endif
    		/* Reset static vars */
    		bytes_read = 0;
    		prev_read_result = 0;
            socket_state = SOCKET_STATE_EVAL_RESPONSE;
    		return 0;
    	}
    }

    /* Set for next function call */
	prev_read_result = result;

    return 1;
}


/*	Read response buffer and compare with sent data.
 *  On successful evaluation, increment fifo read pointer.
 *
 *  Next state:
 *  	SOCKET_STATE_ADD_DATA - fifo contains more data
 *  	SOCKET_STATE_CLOSE - fifo empty
 *
 * 	return:
 *  	-1: error
 *		 0: data OK
 *		 1: still evaluating
 */
int8_t _evaluate_socket(void) {

    /* Pointer to 'request_data_buf' substring inside of 'response_buf' */
    char *request_ok = strstr(response_buf, request_data_buf);
    char *request_400 = strstr(response_buf, "400 Bad Request");

    /* Check if JSON syntax s correct.
     * The first request after starting the app may contain missing chars.
     * The missing chars are usually in the region 40-80 (hash-error) */
    if (request_400 != NULL) {
#if(DEBUG_REQUEST==1)
		printf("*\tRESPONSE 400\n");
#endif
		printf( "\tReceived response code 400, continue with next request.\n"
				"\tOriginal request:\n%s\n", request_buf);
    }

    /* Check, if match in string comparison exists */
	if (request_ok != NULL){
#if(DEBUG_REQUEST==1)
		printf("*\tRESPONSE OK\n");
#endif
	}

    /* Check for response */
	if (request_ok != NULL || request_400 != NULL){
		/* Increment read pointer, means next data row can be sent */
		if (fifo_increment_read_idx(&request_fifo) != 0) {
			/* Is this error possible (?) */
	    	/* Refresh local timestamp variable and report error */
			get_timestamp_raw(timestamp);
			printf("SOCKET FATAL: INCREMENT EMPTY FIFO | %s\n", timestamp);
			return 0;
		}
		if (_check_fifo_for_new_data() == 0) {
	        socket_state = SOCKET_STATE_ADD_DATA;
		} else {
	        socket_state = SOCKET_STATE_CLOSE;
		}
		return 0;
	}

    return 1;
}


/*	Close the socket and destroy file descriptor.
 *
 *  Next state:
 *  	SOCKET_STATE_IDLE
 *
 * 	return:
 *  	-1: error
 *		 0: success
 *		 1: retry
 */
int8_t _close_socket(void) {
    if (close(sockfd) != 0) {
		_report_socket_errno();
        /* Common error when trying to close unopened socket */
		if (errno == EBADF) {
			socket_state = SOCKET_STATE_IDLE;
	        return 0;
		}
        return 1;
    }
    socket_state = SOCKET_STATE_IDLE;
#if(DEBUG_REQUEST==1)
		printf("*\tSOCKET CLOSED\n");
#endif
    return 0;
}


/*	Check if fifo has any pending data.
 *
 *  return:
 *  	-1: error
 *		 0: new data available
 *		 1: nothing new
 */
int8_t _check_fifo_for_new_data (void) {

	/* Check for pending data */
    if (str_fifo_read(&request_fifo, request_data_buf) == 0) {
        return 0;
    }

	return 1;
}


/* Reset read/write byte counters (on error, or timer elapsed)
 */
void _reset_static_vars(void){
	 bytes_sent = 0;
	 bytes_read = 0;
	 prev_read_result = 0;
	 return;
}


/* 	Write zeroes to request, response and request data buffers.
 */
int8_t _clear_request_buffers(void) {
    memset(request_buf, 0, REQUEST_BUF_SIZE);
    memset(request_data_buf, 0, REQUEST_FIFO_STR_SIZE);
    memset(response_buf, 0, RESPONSE_BUF_SIZE);
    return 0;
}


/*	Prints socket state, error #, verbose and timestamp.
 */
void _report_socket_errno(void) {
    get_timestamp_raw(timestamp);
    printf("Socket internal error: \n"
		"\t State: %d \n"
		"\t Error: (%d) %s \n"
		"\t Time: %s\n",
		socket_state, errno, strerror(errno), timestamp);
    return;
}


/*	Check if max allowed time in socket state has ended and reset timer.
 *
 * 	return:
 * 		0: max allowed time reached
 * 		1: timer still running
 */
int8_t _has_socket_timer_ended(void) {
	long int timer_now;
	get_timestamp_epoch(&timer_now);
	if (timer_now - socket_timer_start > SOCKET_MAX_ALLOWED_STATE_TIME_S) {
//      get_timestamp_raw(timestamp);
//		printf("MAX ALLOWED SOCKET TIME REACHED: %d | %s\n",
//				socket_state, timestamp);
		/* Reset timer */
    	_reset_socket_timer();
		return 0;
	}
	return 1;
}


/*	Reset socket state timer.
 */
void _reset_socket_timer(void) {
	get_timestamp_epoch(&socket_timer_start);
	return;
}

