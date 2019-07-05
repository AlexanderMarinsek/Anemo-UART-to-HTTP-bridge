
#include "requests_task.h"
#include "../../fifo/fifo.h"
#include "../../timestamp/timestamp.h"

#include <stdio.h> /* printf, sprintf */
#include <stdlib.h> /* exit */
#include <unistd.h> /* read, write, close */
#include <string.h> /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h> /* struct hostent, gethostbyname */
#include <time.h>

#include <fcntl.h>

#include <errno.h>


// -- GLOBALS ------------------------------------------------------------------

int32_t socket_state;



// -- LOCALS -------------------------------------------------------------------


static int32_t sockfd;
static struct sockaddr_in serv_addr;



/* Fifo for data storage */
static str_fifo_t requests_fifo = {
	0,
	0,
	REQUESTS_FIFO_BUF_SIZE,
	REQUESTS_FIFO_STR_SIZE,
	NULL
};

static char timestamp[TIMESTAMP_RAW_STRING_SIZE];

//static request_link_t request_link;

static char host[HOST_ADDR_BUF_LEN];
//static int16_t portno;

char  request_buf[REQUEST_BUF_SIZE],
      response_buf[RESPONSE_BUF_SIZE],
      request_data_buf[REQUEST_FIFO_STR_SIZE+1];

int8_t _idle_socket(void);
int8_t _create_socket(void);
int8_t _connect_socket(void);
int8_t _add_request_data(void);
int8_t _write_socket(void);
int8_t _read_socket(void);
int8_t _evaluate_socket(void);
int8_t _close_socket(void);
int8_t _clear_requests_buffers(void);

int8_t _check_fifo (void);
void _report_socket_errno(void);

// -- FUNS ---------------------------------------------------------------------


/*  Point outer pointer to local fifo struct and init storage for fifo
 */
int8_t requests_task_init_fifo (str_fifo_t **_fifo) {
    /* Set outer pointer to point to fifo local struct */
    *_fifo = &requests_fifo;
    /* Set up memory for fifo struct and return success/error */
    return setup_str_fifo(
        &requests_fifo, REQUESTS_FIFO_BUF_SIZE, REQUESTS_FIFO_STR_SIZE);
}


int8_t requests_task_init_socket(char *_host, int16_t portno) {
    /* Check length */
    if (strlen(_host) > HOST_ADDR_BUF_LEN-1) {
        return -1;
    }
	/* Copy to local string */
    memcpy(host, _host, strlen(_host)+1);
    /* Save port number */
    //portno = _portno;


    struct hostent *server;
    // -- lookup host
    server = gethostbyname(host);
    if (server != NULL) {
    socket_state = SOCKET_STATE_IDLE;
    #if(DEBUG_REQUESTS==1)
    printf("SOCKET CREATED\n");
    #endif
    }
    else {
    socket_state = SOCKET_STATE_UNKNOWN_HOST;
    get_timestamp_raw(timestamp);
    printf("SOCKET: UNKNOWN HOST | %s\n", timestamp);
    return -1;
    }

    // -- initialize server
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    memcpy(&serv_addr.sin_addr.s_addr,server->h_addr,server->h_length);

    _clear_requests_buffers();

    return 0;
}


int8_t requests_task_run(void) {
	int8_t status = 1;
    int8_t (*state_fun_ptr) (void);

    printf("State: %d\n", socket_state);

    switch (socket_state) {
    // -- create
    case SOCKET_STATE_IDLE:
    	//status = _idle_socket();
    	state_fun_ptr = &_idle_socket;
        break;
    case SOCKET_STATE_CREATE:
    	//status = _create_socket();
    	state_fun_ptr = &_create_socket;
    	break;
	// -- connect
	case SOCKET_STATE_CONNECT:
		//status = _connect_socket();
    	state_fun_ptr = &_connect_socket;
		break;
	// -- connect
	case SOCKET_STATE_ADD_DATA:
		//status = _add_request_data();
    	state_fun_ptr = &_add_request_data;
		break;
    // -- write
    case SOCKET_STATE_WRITE:
    	//status = _write_socket();
    	state_fun_ptr = &_write_socket;
        break;
	/* Read from socket */
    case SOCKET_STATE_READ:
    	//status = _read_socket ();
    	state_fun_ptr = &_read_socket;
        break;
    // -- read
    case SOCKET_STATE_EVAL_RESPONSE:
    	//status = _evaluate_socket();
    	state_fun_ptr = &_evaluate_socket;
        break;
    // -- close
    case SOCKET_STATE_CLOSE:
    	//status = _close_socket();
    	state_fun_ptr = &_close_socket;
        break;
    default:
    	//status = _close_socket();
    	state_fun_ptr = &_close_socket;
        // -- do nothing
        break;
    }

    status = state_fun_ptr();

    if (status == -1) {
    	return SOCKET_STATUS_ERROR;
    }
    if (status == 0) {
    	return SOCKET_STATUS_BUSY;
    }
    if (status == 1) {
    	return SOCKET_STATUS_BUSY;
    }
    if (status == 2) {
    	return SOCKET_STATUS_IDLE;
    }
    else {
    	return SOCKET_STATUS_ERROR;
    }
}


int8_t _check_fifo (void) {

	/* Check for pending data */
    if (str_fifo_read(&requests_fifo, request_data_buf) == 0) {
        return 0;
    }

	return 1;
}


/* 	Check, if fifo contains new request data.
 *
 *  Next state:
 *  	SOCKET_STATE_CREATE
 *
 *  returns:
 *  	-1: error
 *		 0: new data available
 *		 2: idle
 */
int8_t _idle_socket(void) {
	if (_check_fifo() == 0) {
#if(DEBUG_REQUESTS==1)
		printf("\tSOCKET FIFO DATA DETECTED\n");
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

#if(DEBUG_REQUESTS==1)
	printf("sockfd: %d\n", sockfd);
    printf("errno: %d | %s\n", errno, strerror(errno));
#endif

    /* Catch error, which is not EINPROGRESS.
     * Normal: EINPROGRESS is thrown in non blocking operations
     */
    if (sockfd == -1 && errno != EINPROGRESS) {
		printf("***** ERROR *****\n");
		_report_socket_errno();
        socket_state = SOCKET_STATE_CLOSE;
        return -1;
    }

    /* Catch refused error, because socket() returns positive on refused. */
    if (errno == ECONNREFUSED) {
		printf("***** ERROR *****\n");
		_report_socket_errno();
        socket_state = SOCKET_STATE_CLOSE;
        return -1;
    }

    /* Set to non blocking */
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    /* Set socket state variable */
    socket_state = SOCKET_STATE_CONNECT;

#if(DEBUG_REQUESTS==1)
	printf("\tSOCKET CREATED\n");
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

#if(DEBUG_REQUESTS==1)
	printf("connected: %d\n", connected);
    printf("errno: %d | %s\n", errno, strerror(errno));
#endif

    /* Catch error, which is not EINPROGRESS. */
    if (sockfd == -1 && errno != EINPROGRESS) {
		printf("***** ERROR *****\n");
		_report_socket_errno();
        socket_state = SOCKET_STATE_CLOSE;
        return -1;
    }

    /* Set socket state variable */
	socket_state = SOCKET_STATE_ADD_DATA;

#if(DEBUG_REQUESTS==1)
	printf("\tSOCKET CONNECTED\n");
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
	/* Clear all buffers */
    _clear_requests_buffers();
    /* Get row of data from fifo buffer */
    str_fifo_read(&requests_fifo, request_data_buf);
    /* Add request data to request buffer */
    sprintf(request_buf, REQUEST_FMT,
		host, strlen(request_data_buf), request_data_buf);
    /* Set socket state variable */
    socket_state = SOCKET_STATE_WRITE;

#if(DEBUG_REQUESTS==1)
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
	printf("state 4\n");
    static size_t bytes_sent = 0;
    /* Only write the bytes containing data (string) */
    size_t request_len = strlen(request_buf);

    /* Write and get amount of bytes, that were written
     * 	-1: can't write
     * 	 0: nothing to write
     * 	>0: number of bytes written
     */
    size_t result =
		write(sockfd, request_buf + bytes_sent, request_len - bytes_sent);

#if(DEBUG_REQUESTS==1)
	printf("result: %ld, %ld, %ld\n", result, bytes_sent, request_len);
    printf("errno: %d | %s\n", errno, strerror(errno));
#endif

	/* Normal: EINPROGRESS is thrown in non blocking operations */
	if (result == -1 && errno != EINPROGRESS) {
		printf("***** ERROR *****\n");
		_report_socket_errno();
        socket_state = SOCKET_STATE_CLOSE;
		return -1;
	}

    /* Increment bytes_read ('request_buf' idx pointer) */
	bytes_sent += result;

    /* Finished writing (writen everything, nonthing else left) */
    if (request_len == bytes_sent || result == 0) {
#if(DEBUG_REQUESTS==1)
    printf("\tREQUEST WRITTEN: \n%s\n", request_buf);
#endif
		/* Reset static vars */
    	bytes_sent = 0;
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
    static ssize_t bytes_read = 0;
    /* Previous amount of bytes, that were read */
    static ssize_t prev_result = 0;

    /* Read and get amount of bytes, that were read
     * 	-1: nothing new
     * 	 0: can't access read data
     * 	>0: number of bytes read
     */
    ssize_t result =
    		read(sockfd, response_buf + bytes_read, RESPONSE_BUF_SIZE - bytes_read);

#if(DEBUG_REQUESTS==1)
	printf("result: %ld, %ld, %d\n", result, bytes_read, RESPONSE_BUF_SIZE);
    printf("errno: %d | %s\n", errno, strerror(errno));
#endif

    /* Check for socket error */
    //if (errno == ENOTCONN || errno == EBADF)
    if (result == 0) {
		printf("***** ERROR *****\n");
		_report_socket_errno();
        socket_state = SOCKET_STATE_CLOSE;
        return -1;
    }

    /* If read successfull, increment bytes_read ('response_buf' idx pointer) */
    if (result > 0) {
        bytes_read += result;
    }

    /* Reached end of buffer */
    if (bytes_read == RESPONSE_BUF_SIZE-1) {
    	/* Serious error, hard to catch */
	    printf("Response buffer too small\n");
        socket_state = SOCKET_STATE_CLOSE;
	    return -1;
	}

#if(DEBUG_REQUESTS==1)
    printf("Results: %ld, %ld, %ld\n", prev_result, result, bytes_read);
    printf("%ld | %d | %s\n", result, errno, strerror(errno));
	printf("%d | %d | %d | %d | %d | %d | %d | %d \n",
		EAGAIN, EWOULDBLOCK, EBADF, EFAULT, EINTR, EINVAL, EIO, EISDIR);
#endif

	/* Check for end of response */
    if (prev_result > 0) {		/* Previously read something */
    	if (result == -1) {		/* Nothing new was read */
#if(DEBUG_REQUESTS==1)
			printf("\tRESPONSE RECEIVED (%ld):\n%s\n",
				bytes_read, response_buf);
#endif
    		/* Reset static vars */
    		bytes_read = 0;
    		prev_result = 0;
            socket_state = SOCKET_STATE_EVAL_RESPONSE;
    		return 0;
    	}
    }

    /* Set for next function call */
	prev_result = result;

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

    /* Check, if match in string comparison exists */
	if (request_ok != NULL){
#if(DEBUG_REQUESTS==1)
		printf("\tRESPONSE OK\n");
#endif
		/* Increment read pointer, means next data row can be sent */
		if (fifo_increment_read_idx(&requests_fifo) != 0) {
			/* Hard to catch, is error possible (?) */
			printf("Tried to increment empty fifo\n");
		}
		if (_check_fifo() == 0) {
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
 */
int8_t _close_socket(void) {
    if (close(sockfd) != 0) {
		printf("***** ERROR *****\n");
		_report_socket_errno();
        socket_state = SOCKET_STATE_CLOSE;
        return -1;
    }
    socket_state = SOCKET_STATE_IDLE;
    return 0;
}


/*	Prints socket state, error #, verbose and timestamp.
 */
void _report_socket_errno(void) {
    get_timestamp_raw(timestamp);
    printf("Socket error: \n"
		"\t State: %d \n"
		"\t Error: (%d) %s \n"
		"\t Time: %s\n",
		socket_state, errno, strerror(errno), timestamp);
    return;
}


int8_t _clear_requests_buffers(void) {
    memset(request_buf, 0, REQUEST_BUF_SIZE);
    memset(request_data_buf, 0, REQUEST_FIFO_STR_SIZE);
    memset(response_buf, 0, RESPONSE_BUF_SIZE);
    return 0;
}

