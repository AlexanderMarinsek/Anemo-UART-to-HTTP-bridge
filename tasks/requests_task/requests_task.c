
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


int8_t _create_socket(void);
int8_t _connect_socket(void);
int8_t _write_socket(void);
int8_t _wait_for_response(void);
int8_t read_socket(void);
int8_t close_socket(void);
int8_t _clear_requests_buffers(void);


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
    socket_state = SOCKET_CLOSED;
    #if(DEBUG_REQUESTS==1)
    printf("SOCKET CREATED\n");
    #endif
    }
    else {
    socket_state = SOCKET_UNKNOWN_HOST;
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
    printf("State: %d\n", socket_state);
    switch (socket_state) {
    // -- create
    case SOCKET_CLOSED:
        /* Check for pending request in buffer */
        if (str_fifo_read(&requests_fifo, request_data_buf) == 0) {
        	/* Clear all buffers */
            _clear_requests_buffers();
            /* Get row of data from fifo buffer */
            str_fifo_read(&requests_fifo, request_data_buf);
            if (_create_socket() == 0) {
                sprintf(request_buf, REQUEST_FMT, host, strlen(request_data_buf), request_data_buf);
                printf("%lu, %s\n", strlen(request_buf), request_buf);
            }
            return 1;
        }
        return 0;
        break;
    // -- connect
    case SOCKET_CREATED:
        _connect_socket();
        #if(DEBUG_REQUESTS==1)
        //printf("%d\n",socket_state);
        #endif
        return 1;
        break;
    // -- write
    case SOCKET_CONNECTED:
        _write_socket();
        #if(DEBUG_REQUESTS==1)
        //printf("%d\n",socket_state);
        #endif
        return 1;
        break;
	/* Read from socket */
    case SOCKET_WRITE_FINISHED:
    	_wait_for_response ();
        return 1;
        break;
    // -- read
    case SOCKET_READ_RESPONSE:
        read_socket();
        return 1;
        break;
    // -- close
    case SOCKET_CLOSE_PENDING:
    case SOCKET_READ_FINISHED:
        close_socket();
        #if(DEBUG_REQUESTS==1)
        //printf("%d\n",socket_state);
        #endif
        return 1;
        break;
    default:
        // -- do nothing
        break;
    }

    return 0;
}



/* Select socket protocol and stream, create socket.
 *
 *  returns:
 *		 0: successfully connected
 *  	-1: error connecting
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

    /* Normal: EINPROGRESS is thrown in non blocking operations */
    if (sockfd == -1 && errno != EINPROGRESS) {
    	printf("***** ERROR *****\n");
        return -1;
    }

    /* Set to non blocking */
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    /* Set socket state variable */
    socket_state = SOCKET_CREATED;

#if(DEBUG_REQUESTS==1)
	printf("SOCKET CREATED\n");
#endif

    return 0;
}


/*	Connect socket to defined address.
 * 	Exit if maximum socket state elapsed time is reached (close socket).
 */
int8_t _connect_socket(void){

    /* Connect the socket to the previously defined address */
	int connected =
		connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

#if(DEBUG_REQUESTS==1)
	printf("connected: %d\n", connected);
    printf("errno: %d | %s\n", errno, strerror(errno));
#endif

    /* Normal: EINPROGRESS is thrown in non blocking operations */
    if (connected != 0 && errno != EINPROGRESS) {
    	printf("***** ERROR *****\n");
        return -1;
    }

    /* Set socket state variable */
	socket_state = SOCKET_CONNECTED;

#if(DEBUG_REQUESTS==1)
	printf("SOCKET CONNECTED\n");
#endif

  return 0;
}



int8_t _write_socket(void) {
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
	printf("result: %lu, %lu, %lu\n", result, bytes_sent, request_len);
    printf("errno: %d | %s\n", errno, strerror(errno));
#endif

	/* Normal: EINPROGRESS is thrown in non blocking operations */
	if (result == -1 && errno != EINPROGRESS) {
		printf("***** ERROR *****\n");
		//_report_socket_errno();
		return -1;
	}

    /* Increment bytes_read ('request_buf' idx pointer) */
	bytes_sent += result;

    /* Finished writing (writen everything, nonthing else left) */
    if (request_len == bytes_sent || result == 0) {
#if(DEBUG_REQUESTS==1)
    printf("\tREQUEST WRITTEN: \n%s\n", request_buf);
#endif
    	bytes_sent = 0;
        socket_state = SOCKET_WRITE_FINISHED;
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
 */
int8_t _wait_for_response(void) {
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
	printf("result: %lu, %lu, %d\n", result, bytes_read, RESPONSE_BUF_SIZE);
    printf("errno: %d | %s\n", errno, strerror(errno));
#endif

    /* Check for socket error */
    //if (errno == ENOTCONN || errno == EBADF)
    if (result == 0) {
		printf("***** ERROR *****\n");
		//_report_socket_errno();
        return -1;
    }

    /* If read successfull, increment bytes_read ('response_buf' idx pointer) */
    if (result > 0) {
        bytes_read += result;
    }

    /* Reached end of buffer */
    if (bytes_read == RESPONSE_BUF_SIZE-1) {
	    printf("Response buffer too small\n");
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
            socket_state = SOCKET_READ_RESPONSE;
    		return 0;
    	}
    }

    /* Set for next function call */
	prev_result = result;

    return 1;
}


int8_t read_socket(void) {

    /* Pointer to 'request_data_buf' substring inside of 'response_buf' */
    char *request_ok = strstr(response_buf, request_data_buf);

    /* Check, if match in string comparison exists */
	if (request_ok != NULL){
#if(DEBUG_REQUESTS==1)
		printf("\tRESPONSE OK\n");
#endif
		/* Increment read pointer, means next data row can be sent */
		if (fifo_increment_read_idx(&requests_fifo) != 0) {
			printf("Tried to increment empty fifo\n");
			return -1;
		}
        socket_state = SOCKET_READ_FINISHED;
		return 0;
	}

    return 1;



    /* Pointer to validation substring */
    //char *request_ok;


    static size_t bytes_read = 0;
    //size_t result, response_len;
    size_t result;


    result = read(sockfd, response_buf + bytes_read, RESPONSE_BUF_SIZE - bytes_read);

    /*printf("%d | %d | %s\n", (int)result, errno, strerror(errno));
    printf("%d | %d | %d | %d | %d | %d | %d | %d \n",
        EAGAIN, EWOULDBLOCK, EBADF, EFAULT, EINTR, EINVAL, EIO, EISDIR);*/

    /* Error is often reported on subsequent reads in non blocking mode */
    if (result == -1) {
        printf("\tRESPONSE RECEIVED: \n%s\n", response_buf);
        fifo_increment_read_idx(&requests_fifo);
        socket_state = SOCKET_READ_FINISHED;
        return 1;
    }

    bytes_read+=result;

    if (RESPONSE_BUF_SIZE == bytes_read) {
        printf("Response buffer too small\n");
        return -1;
    }

    //request_ok = strstr(response, "202");
    request_ok = strstr(response_buf, request_data_buf);
    if (request_ok != NULL){
        #if(DEBUG_REQUESTS==1)
        printf("\tRESPONSE RECEIVED: \n%s\n", response_buf);
        #endif

        fifo_increment_read_idx(&requests_fifo);
        _clear_requests_buffers();

        bytes_read = 0;
        socket_state = SOCKET_READ_FINISHED;
    }


    return 1;
}


int8_t close_socket(void) {
    if (close(sockfd) != 0) {
		printf("***** ERROR *****\n");
		//_report_socket_errno();
    	return -1;
    }
    socket_state = SOCKET_CLOSED;
    return 0;
}



int8_t _clear_requests_buffers(void) {
    memset(request_buf, 0, REQUEST_BUF_SIZE);
    memset(request_data_buf, 0, REQUEST_FIFO_STR_SIZE);
    memset(response_buf, 0, RESPONSE_BUF_SIZE);
    return 0;
}

