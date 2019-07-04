
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

// -- reset to zero each time socket state moves on
static uint32_t socket_error_count = 0;

static int32_t sockfd;
static struct sockaddr_in serv_addr;



/* Fifo for data storage */
static str_fifo_t fifo = {
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

char  request[REQUEST_BUF_SIZE],
      response[RESPONSE_BUF_SIZE],
      request_data[REQUEST_FIFO_STR_SIZE+1];


int8_t _create_socket(void);
int8_t write_socket(void);
int8_t _connect_socket(void);
int8_t read_socket(void);
int8_t close_socket(void);
int8_t _clear_response_memory(void);


// -- FUNS ---------------------------------------------------------------------


/*  Point outer pointer to local fifo struct and init storage for fifo
 */
int8_t requests_task_init_fifo (str_fifo_t **_fifo) {
    /* Set outer pointer to point to fifo local struct */
    *_fifo = &fifo;
    /* Set up memory for fifo struct and return success/error */
    return setup_str_fifo(
        &fifo, REQUESTS_FIFO_BUF_SIZE, REQUESTS_FIFO_STR_SIZE);
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

    _clear_response_memory();

    return 0;
}


int8_t requests_task_run(void) {
    printf("State: %d\n", socket_state);
    switch (socket_state) {
    // -- create
    case SOCKET_CLOSED:
        // -- check for pending request in buffer and load it
        if (str_fifo_read(&fifo, request_data) == 0) {
            if (_create_socket() == 0) {
                sprintf(request, REQUEST_FMT, host, strlen(request_data), request_data);
                printf("%lu, %s\n", strlen(request), request);
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
        write_socket();
        #if(DEBUG_REQUESTS==1)
        //printf("%d\n",socket_state);
        #endif
        return 1;
        break;
    // -- read
    case SOCKET_WRITE_FINISHED:
        read_socket();
        #if(DEBUG_REQUESTS==1)
        //printf("%d\n",socket_state);
        #endif
        return 1;
        break;
    // -- close
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



int8_t _create_socket(void) {

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    /* Non blocking */
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    if (sockfd >= 0) {
        socket_state = SOCKET_CREATED;
        return 0;
    }
    return 1;
}


/*	Connect socket to defined address.
 * 	Exit if maximum socket state elapsed time is reached (close socket)
 */
int8_t _connect_socket(void){

    /* Connect the socket to the previously defined address */
	int connected =
		connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

	printf("connected: %d\n", connected);
    printf("errno: %d | %s\n", errno, strerror(errno));

    /* Normal: EINPROGRESS is thrown in non blocking operations */
    if (connected != 0 && errno != EINPROGRESS) {
    	printf("Error\n");
        return -1;
    }

    /* Set socket state variable */
	socket_state = SOCKET_CONNECTED;

#if(DEBUG_REQUESTS==1)
	printf("SOCKET CONNECTED\n");
#endif

  return 0;
}



int8_t write_socket(void) {
    static size_t sent = 0;
    size_t bytes_write, request_len;

    request_len = strlen(request);
    // (socket, buffer pointer + offset, number of characters to send)
    bytes_write = write(sockfd, request + sent, request_len - sent);
    sent+=bytes_write;

    #if(DEBUG_REQUESTS==1)
    printf("\tREQUEST WRITTEN: \n%s\n", request);
    #endif

    // -- finished writing
    if (request_len == sent || bytes_write == 0) {
        sent = 0;
        socket_state = SOCKET_WRITE_FINISHED;
        // -- reset consecutive error counter
        if (socket_error_count != 0) {
            get_timestamp_raw(timestamp);
            printf("(write) SOCKET ERROR CURED | %d | %s\n",
                socket_error_count, timestamp);
            socket_error_count = 0;
        }
    }

    return 0;
}



int8_t read_socket(void) {
    /* Pointer to validation substring */
    char *request_ok;


    static size_t bytes_read = 0;
    //size_t result, response_len;
    size_t result;


    result = read(sockfd, response + bytes_read, RESPONSE_BUF_SIZE - bytes_read);

    /*printf("\tRESPONSE\n%s\n", response);
    printf("\tREQUEST\n%s\n", request);
    printf("\trequest_data\n%s\n", request_data);
    char *data_ok;
    data_ok = strstr(response, request_data);
    if (data_ok != NULL) {
        printf ("\tdata_ok:\n%s\n", data_ok);
    }*/

    /*printf("%d | %d | %s\n", (int)result, errno, strerror(errno));
    printf("%d | %d | %d | %d | %d | %d | %d | %d \n",
        EAGAIN, EWOULDBLOCK, EBADF, EFAULT, EINTR, EINVAL, EIO, EISDIR);*/

    /* Error is often reported on subsequent reads in non blocking mode */
    if (result == -1) {
        printf("\tRESPONSE RECEIVED: \n%s\n", response);
        fifo_increment_read_idx(&fifo);
        socket_state = SOCKET_READ_FINISHED;
        return 1;
    }

    bytes_read+=result;

    if (RESPONSE_BUF_SIZE == bytes_read) {
        printf("Response buffer too small\n");
        return -1;
    }

    //request_ok = strstr(response, "202");
    request_ok = strstr(response, request_data);
    if (request_ok != NULL){
        #if(DEBUG_REQUESTS==1)
        printf("\tRESPONSE RECEIVED: \n%s\n", response);
        #endif

        fifo_increment_read_idx(&fifo);
        _clear_response_memory();

        bytes_read = 0;
        socket_state = SOCKET_READ_FINISHED;
    }


    return 1;
}


int8_t close_socket(void) {
    close(sockfd);
    socket_state = SOCKET_CLOSED;
    return 0;
}



int8_t _clear_response_memory(void) {
    memset(response, 0, RESPONSE_BUF_SIZE);
    return 0;
}

