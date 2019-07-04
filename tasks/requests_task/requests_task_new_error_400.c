
#include "requests_task.h"
#include "../../fifo/fifo.h"
#include "../../timestamp/timestamp.h"

#include <stdio.h> 			/* printf, sprintf */
#include <stdlib.h> 		/* exit */
#include <unistd.h> 		/* read, write, close */
#include <string.h> 		/* memcpy, memset */
#include <sys/socket.h> 	/* socket, connect */
#include <netinet/in.h> 	/* struct sockaddr_in, struct sockaddr */
#include <netdb.h> 			/* struct hostent, gethostbyname */
#include <time.h>

#include <fcntl.h>

#include <errno.h>




/* LOCALS *********************************************************************/

/* Socket file descriptor */
static int sockfd;
/* Struct with addres and port */
static struct sockaddr_in serv_addr;


static int8_t socket_state;

/* Hostname string  */
static char host[HOST_ADDR_BUF_LEN];

/* Request including headers, body ... */
static char request_buf[REQUEST_BUF_SIZE];
/* Response including headers, body ... */
static char response_buf[RESPONSE_BUF_SIZE];




char request_fmt[1024] =
            "POST /latest HTTP/1.1\r\n"
            "Host: %s\r\n"
            //"Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Type: application/json; charset=utf-8\r\n"
            "Content-Length: %d\r\n\r\n"
            "%s\r\n\r\n";


/* GLOBALS ********************************************************************/

/* Bears only the JSON data (request body) */
char request_data_buf[REQUEST_FIFO_STR_SIZE];

/* Fifo for data storage */
 str_fifo_t requests_fifo = {
	0,
	0,
	REQUESTS_FIFO_BUF_SIZE,
	REQUESTS_FIFO_STR_SIZE,
	NULL
};

/* Gets written externally. Static to avoid linkage conflicts. */
static char timestamp[TIMESTAMP_RAW_STRING_SIZE];

/* Used to measure socket time in single state */
long int socket_timer_start = 0;

/* PROTOTYPES *****************************************************************/

int8_t _create_socket(void);
int8_t write_socket(void);
int8_t connect_socket(void);
int8_t _wait_for_response(void);
int8_t _read_socket(void);
int8_t close_socket(void);

void _clear_requests_buffers(void);
void _copy_data_to_request(void);
void _change_socket_state(int8_t new_socket_state);

int8_t _has_socket_timer_ended(void);
void _reset_socket_timer(void);

void _report_socket_errno(void);


/* FUNCTIONS ******************************************************************/

/*  Point outer pointer to local fifo struct and init storage for fifo
 */
int8_t requests_task_init_fifo (str_fifo_t **_fifo) {
    /* Set outer pointer to point to requests_fifo local struct */
    *_fifo = &requests_fifo;
    /* Set up memory for requests_fifo struct */
    int8_t tmp_status = setup_str_fifo(
        &requests_fifo, REQUESTS_FIFO_BUF_SIZE, REQUESTS_FIFO_STR_SIZE);
    return tmp_status;
}

int8_t requests_task_init_socket(char *_host, int16_t portno) {

    /* Check hostname length */
    if (strlen(_host) > HOST_ADDR_BUF_LEN-1) {
        return -1;
    }

	/* Copy hostname to local string */
    memcpy(host, _host, strlen(_host)+1);

	/* Server data */
    struct hostent *server;
    /* Get server data by domain name, or IPv4 */
    server = gethostbyname(host);

    if (server == NULL) {
        socket_state = SOCKET_UNKNOWN_HOST;
        get_timestamp_raw(timestamp);
        printf("SOCKET: UNKNOWN HOST | %s\n", timestamp);
        return -1;
    }

    socket_state = SOCKET_CLOSED;
    #if(DEBUG_REQUESTS==1)
    printf("SOCKET CREATED\n");
    #endif

    /* Initialize server struct */
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    memcpy(&serv_addr.sin_addr.s_addr,server->h_addr,server->h_length);

    return 0;
}

int8_t new_re (void) {

    struct hostent *server;
    server = gethostbyname(host);
    memset(&serv_addr,0,sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(5761);
        memcpy(&serv_addr.sin_addr.s_addr,
            server->h_addr,server->h_length);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

	int cc = connect(sockfd,(struct sockaddr *)&serv_addr, sizeof(serv_addr));

	printf("%d, %d\n", sockfd, cc);
	printf("%s\n", request_buf);

	write(sockfd, request_buf , 1024);

	char response_buf[1024] = {0};
	ssize_t bytes_read = 0;
	ssize_t result =
		read(sockfd, response_buf + bytes_read, 1024 - bytes_read);
	printf("Response:\n%s\n", response_buf);
	printf("Result: %ld\n", result);


    return 0;
}


int8_t requests_task_run(void) {
    printf("State: %d\n", socket_state);



    int8_t state_fun_return;
    int8_t new_state;

    int8_t (*state_fun_ptr) (void);

	uint64_t ii = 0;
    switch (socket_state) {
    /* Create socket */
    case SOCKET_CLOSED:
        /* Check for new request data in buffer */
        if (str_fifo_read(&requests_fifo, request_data_buf) == 0) {
            _clear_requests_buffers();
            str_fifo_read(&requests_fifo, request_data_buf);
            if (_create_socket() == 0) {
                socket_state = SOCKET_CREATED;
                _copy_data_to_request();
                _reset_socket_timer();
            	state_fun_ptr = &connect_socket;
            	socket_state = SOCKET_CONNECTED;
            	state_fun_return = state_fun_ptr();
                state_fun_return = 0;
            }
            state_fun_return = 1;
        } else {
            printf("C\n");
        	state_fun_return = 1;
        }
        break;
    /* Connect socket */
    case SOCKET_CREATED:
    	for (ii=0; ii<2000000000; ii++) {}
    	printf("%lu\n", ii);
    	state_fun_ptr = &connect_socket;
    	new_state = SOCKET_CONNECTED;
    	state_fun_return = state_fun_ptr();
        break;
    /* Write to socket */
    case SOCKET_CONNECTED:
    	state_fun_ptr = &write_socket;
    	new_state = SOCKET_WRITE_FINISHED;
    	state_fun_return = state_fun_ptr();
        break;
    /* Read from socket */
    case SOCKET_WRITE_FINISHED:
    	state_fun_ptr = &_wait_for_response;
    	new_state = SOCKET_READ_RESPONSE;
        //socket_state = SOCKET_READ_RESPONSE;
    	state_fun_return = state_fun_ptr();
    	printf("AAA %d\n", state_fun_return);
        break;
    case SOCKET_READ_RESPONSE:
    	state_fun_ptr = &_read_socket;
    	new_state = SOCKET_CLOSE_PENDING;
    	state_fun_return = state_fun_ptr();
    	break;
    /* Close socket */
    case SOCKET_CLOSE_PENDING:
    case SOCKET_READ_FINISHED:
    	state_fun_ptr = &close_socket;
        new_state = SOCKET_CLOSED;
        state_fun_return = state_fun_ptr();
        break;
    default:
        // -- do nothing
        //state_fun_return = 1;
        break;
    }

    /* Socket state function finished successfully */
	if (state_fun_return == 0) {
    	_change_socket_state(new_state);
		_reset_socket_timer();
	}

	/* On error, close socket */
	if (_has_socket_timer_ended() == 0 || state_fun_return == -1) {
    	_change_socket_state(SOCKET_CLOSE_PENDING);
    	_reset_socket_timer();
	}


    return state_fun_return;
}



/* Select socket protocol and stream, create socket.
 *  reset socket state elapsed time timer
 *  returns:
 *   0 - successfully connected
 *   1 - error connecting
 */
int8_t _create_socket(void) {

    /* Like 'open()' for files
     *  AF_INET - IPv4
     *  SOCK_STREAM - Provides sequenced, reliable, two-way streams
     *  0 - default protocol selector
     */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
    	_report_socket_errno();
        return -1;
    }

    /* Set to non blocking */
    //int flags = fcntl(sockfd, F_GETFL, 0);
    //fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    /* Set socket state variable */
    socket_state = SOCKET_CREATED;

#if(DEBUG_REQUESTS==1)
	printf("SOCKET CREATED\n");
#endif

    return 0;
}


/*	Connect socket to defined address.
 * 	Exit if maximum socket state elapsed time is reached (close socket)
 */
int8_t connect_socket(void){

    /* Connect the socket to the previously defined address */
	int connected =
		connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	/*printf("$$$ %d\n", connected);
	connected =
		connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	printf("$$$ %d\n", connected);*/
	//connected =
	//	connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	//printf("$$$ %d\n", connected);
    if (connected != 0) {
    	_report_socket_errno();
        return -1;
    }

    /* Set socket state variable */
	socket_state = SOCKET_CONNECTED;

#if(DEBUG_REQUESTS==1)
	printf("SOCKET CONNECTED\n");
#endif

  return 0;
}


/* void write_socket(void)
 *  write request to socket
 *  enable reentry, if writing process gets interrupted
 *  stop, when everything is written using single function call, or
 *   nothing new was written to the socket (no data left to write)
 *  reset socket state elapsed time timer
 *  exit if maximum socket state elapsed time is reached (close socket)
 */
int8_t write_socket(void) {
    static size_t bytes_sent = 0;
    size_t result;

    /* Write and get amount of bytes, that were written
     * 	-1: can't write
     * 	 0: nothing to write
     * 	>0: number of bytes written
     */
    result =
    	write(sockfd, request_buf + bytes_sent, REQUEST_BUF_SIZE - bytes_sent);

    /* Check for socket error */
    if (result == -1) {
    	_report_socket_errno();
    	return -1;
    }

    /* Increment bytes_read ('request_buf' idx pointer) */
    bytes_sent+=result;

#if(DEBUG_REQUESTS==1)
    printf("\tREQUEST WRITTEN: \n%s\n", request_buf);
#endif

    // -- finished writing
    if (REQUEST_BUF_SIZE == bytes_sent || result == 0) {
        bytes_sent = 0;
        return 0;
    }

    return 1;
}


/*	Wait for a full response.
 *
 * 	return:
 * 		-1: Reached response buffer end
 * 		 0: Full response received
 * 		 1: Waiting for full response
 *
 * 	note: no error catching, if socket gets disconnected
 */
int8_t _wait_for_response(void) {
	/* Read bytes amount, or 'response_buf' write ptr */
    static ssize_t bytes_read = 0;
    /* Previous amount of bytes, that were read */
    static ssize_t prev_result = 0;

    //close(sockfd);
    //shutdown(sockfd, SHUT_RDWR);

    /* Read and get amount of bytes, that were read
     * 	-1: nothing new
     * 	 0: can't access read data
     * 	>0: number of bytes read
     */
    ssize_t result =
    	read(sockfd, response_buf + bytes_read, RESPONSE_BUF_SIZE - bytes_read);

    printf("BBB %d, %s\n", result, response_buf);

    /* Check for socket error */
    //if (errno == ENOTCONN || errno == EBADF)
    if (result == 0) {
    	_report_socket_errno();
        return -1;
    }

    /* If read successfull, increment bytes_read ('response_buf' idx pointer) */
    if (result > 0) {
        bytes_read += result;
    }

    if (bytes_read == RESPONSE_BUF_SIZE) {
	    printf("Response buffer too small\n");
	    return -1;
	}

    printf("Results: %ld, %ld, %ld\n", prev_result, result, bytes_read);
    printf("%ld | %d | %s\n", result, errno, strerror(errno));
	printf("%d | %d | %d | %d | %d | %d | %d | %d \n",
		EAGAIN, EWOULDBLOCK, EBADF, EFAULT, EINTR, EINVAL, EIO, EISDIR);

	/* Check for end of response */
    if (prev_result > 0) {
    	if (result == -1) {
#if(DEBUG_REQUESTS==1)
			printf("\tRESPONSE RECEIVED (%ld): \n%s\n",
					bytes_read, response_buf);
#endif
    		/* Reset static vars */
    		bytes_read = 0;
    		prev_result = 0;
    		return 0;
    	}
    }

    /* Set for next function call */
	prev_result = result;

    return 1;
}


/* void _read_socket(void)
 *  read response from socket
 *  minimum time must pass between writing and reading, to accomodate for
 *   slower connections/servers and prevent non-deterministic behaviour
 *  enable reentry, if reading process gets interrupted
 *  stop, when everything is received using single function call, or
 *   nothing new was received on the socket (no data left to read)
 *  reset socket state elapsed time timer
 *  exit if maximum socket state elapsed time is reached (close socket)
 */
int8_t _read_socket(void) {

    /* Pointer to 'request_data_buf' substring inside of 'response_buf' */
    char *request_ok = strstr(response_buf, request_data_buf);

    /* Check, if match in string comparison exists */
	if (request_ok != NULL){
#if(DEBUG_REQUESTS==1)
		printf("\tRESPONSE RECEIVED: \n%s\n", response_buf);
#endif

		/* Increment read pointer, means next data row can be sent */
		if (fifo_increment_read_idx(&requests_fifo) != 0) {
			printf("Tried to increment empty fifo\n");
			return -1;
		}

		return 0;
	}

    return 1;
}


/*
 *
 */
int8_t close_socket(void) {
    if (close(sockfd) != 0) {
    	_report_socket_errno();
    	return -1;
    }
    return 0;
}


/*	Change socket state variable's value.
 */
void _change_socket_state(int8_t new_socket_state) {
	socket_state = new_socket_state;
	return;
}

/* 	Write zeroes to request, response and request data buffers.
 */
void _clear_requests_buffers(void) {
    memset(request_buf, 0, REQUEST_BUF_SIZE);
    memset(request_data_buf, 0, REQUEST_FIFO_STR_SIZE);
    memset(response_buf, 0, RESPONSE_BUF_SIZE);
    return;
}

/* 	Copy request data to request buffer.
 */
void _copy_data_to_request (void) {
    //sprintf(request_buf, REQUEST_FMT, host, strlen(request_data_buf), request_data_buf);
    printf("%s\n", request_fmt);
	sprintf(request_buf, request_fmt, host, strlen(request_data_buf), request_data_buf);
	//sprintf(request_buf, REQUEST_FMT, host, strlen(""), "");
    return;
}

/*	Prints socket error number, verbose and timestamp.
 */
void _report_socket_errno(void) {
    get_timestamp_raw(timestamp);
    printf("Socket error: %d | %d | %s | %s\n",
    		socket_state, errno, strerror(errno), timestamp);
    return;
}

/*	Check if max allowed time in socket state has ended.
 *
 * 	return:
 * 		0: max allowed time reached
 * 		1: timer still running
 */
int8_t _has_socket_timer_ended(void) {
	long int timer_now;
	get_timestamp_epoch(&timer_now);
	if (timer_now - socket_timer_start > SOCKET_MAX_ALLOWED_STATE_TIME_S) {
        get_timestamp_raw(timestamp);
		printf("MAX ALLOWED SOCKET TIME REACHED: %d | %s\n",
				socket_state, timestamp);
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

