
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

int8_t _clear_response_memory(void);

// -- GLOBALS ------------------------------------------------------------------
int32_t socket_state, request_data_len;

// -- ThingSpeak
// -- find fresh IP through the terminal: "nslookup api.thingspeak.com"
/*char *host =  "52.54.134.167";
char request_fmt[1024] =  "POST /update HTTP/1.1\r\n"
                          "Host: %s\r\n"
                          "Content-Type: application/x-www-form-urlencoded\r\n"
                          "Content-Length: %d\r\n\r\n"
                          "%s\r\n\r\n";*/

// -- test.anemo.si
char request_fmt[REQUEST_SIZE] =
            "POST /latest HTTP/1.1\r\n"
            "Host: %s\r\n"
            //"Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Type: application/json; charset=utf-8\r\n"
            "Content-Length: %d\r\n\r\n"
            "%s\r\n\r\n";



/*
// -- LOCAL environment
//char *host = "127.1.0.0";
char *host = "10.0.0.4";
char request_fmt[REQUEST_SIZE] =
            "POST /anemo-web/anemo_general_api.php HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: %d\r\n\r\n"
            "%s\r\n\r\n";
*/
char  request[REQUEST_SIZE],
      response[RESPONSE_SIZE],
      request_data[REQUEST_FIFO_STR_SIZE+1];



// -- LOCALS -------------------------------------------------------------------
// -- for measuring time needed in individual socket state
static struct timespec socket_state_start={0,0}, socket_state_end={0,0};
static int32_t socket_elapsed_time = 0;

// -- reset to zero each time socket state moves on
static uint32_t socket_error_count = 0;

static int32_t sockfd;
static struct sockaddr_in serv_addr;

//static int32_t portno = 5761;

// -- string for checking successful write
#define DEVICE_HASH_LEN 32
static char write_validation [DEVICE_HASH_LEN];

// -- requests fifo buffer
static char **request_fifo_buf=NULL;
str_fifo_t request_fifo = {
	0,
	0,
	REQUEST_FIFO_BUF_SIZE+1,
	REQUEST_FIFO_STR_SIZE+1,
	NULL
};




/* Fifo for data storage */
static str_fifo_t fifo = {
	0,
	0,
	REQUESTS_FIFO_BUF_SIZE,
	REQUESTS_FIFO_STR_SIZE,
	NULL
};

static char timestamp[TIMESTAMP_RAW_STRING_SIZE];

static request_link_t request_link;

static char host[HOST_ADDR_BUF_LEN];
//static int16_t portno;


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
            if (create_socket()) {
                //sprintf(request_data, "{\"a\":1,\"b\":\"2\",\"c\":{\"d\":\"3\",\"e\":\"4\"}}");
                request_data_len = strlen(request_data);
                sprintf(request, request_fmt, host, request_data_len, request_data);
            }
            #if(DEBUG_REQUESTS==1)
            //printf("%d\n",socket_state);
            #endif
            return 1;
        }
        return 0;
        break;
    // -- connect
    case SOCKET_CREATED:
        connect_socket();
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


/* int32_t setup_socket (void)
 *  validate host address, set up server structure
 *  returns:
 *   1 - setup successful
 *   0 - error setting up
 */
int8_t setup_socket (void) {
  /*struct hostent *server;
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
*/
  return 0;
}


/* int32_t create_socket(void)
 *  select socket protocol and stream, create socket
 *  reset socket state elapsed time timer
 *  returns:
 *   1 - successfully connected
 *   0 - error connecting
 */
int32_t create_socket(void) {

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    /* Non blocking */
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    if (sockfd >= 0) {
        socket_state = SOCKET_CREATED;
        // -- reset state timer
        clock_gettime(CLOCK_MONOTONIC, &socket_state_start);
        return 1;
    }
return 0;
}


/* void connect_socket(void)
 *  connect socket
 *  reset socket state elapsed time timer
 *  exit if maximum socket state elapsed time is reached (close socket)
 */
void connect_socket(void) {
    // -- try connecting
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) >= 0) {
        socket_state = SOCKET_CONNECTED;
        // -- reset state timer
  	    clock_gettime(CLOCK_MONOTONIC, &socket_state_start);
        #if(DEBUG_REQUESTS==1)
        printf("SOCKET CONNECTED\n");
        #endif
        // -- reset consecutive error counter
        if (socket_error_count != 0) {
            get_timestamp_raw(timestamp);
            printf("(connect) SOCKET ERROR CURED | %d | %s\n",
                socket_error_count, timestamp);
            socket_error_count = 0;
        }
    }

    // -- get elapsed time in socket state
    clock_gettime(CLOCK_MONOTONIC, &socket_state_end);
    socket_elapsed_time = socket_state_end.tv_sec - socket_state_start.tv_sec;
    // -- max allowed time in socket state has passed
    if (socket_elapsed_time >= MAX_SOCKET_TIME) {
        // -- only show error the first time it occured in a given interval
        if (socket_error_count == 0) {
            get_timestamp_raw(timestamp);
            printf("(connect) MAX ALLOWED SOCKET TIME REACHED IN: %d | %s\n",
                  socket_state, timestamp);
        }
        socket_error_count++;
        socket_state = SOCKET_CLOSE_PENDING;
    }

  return;
}


/* void write_socket(void)
 *  write request to socket
 *  enable reentry, if writing process gets interrupted
 *  stop, when everything is written using single function call, or
 *   nothing new was written to the socket (no data left to write)
 *  reset socket state elapsed time timer
 *  exit if maximum socket state elapsed time is reached (close socket)
 */
void write_socket(void) {
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
        // -- reset state timer
        clock_gettime(CLOCK_MONOTONIC, &socket_state_start);
        // -- reset consecutive error counter
        if (socket_error_count != 0) {
            get_timestamp_raw(timestamp);
            printf("(write) SOCKET ERROR CURED | %d | %s\n",
                socket_error_count, timestamp);
            socket_error_count = 0;
        }
    }

    // -- get elapsed time in socket state
    clock_gettime(CLOCK_MONOTONIC, &socket_state_end);
    socket_elapsed_time = socket_state_end.tv_sec - socket_state_start.tv_sec;
    // -- max allowed time in socket state has passed
    if (socket_elapsed_time >= MAX_SOCKET_TIME) {
        // -- only sho error the first time it occured in a given interval
        if (socket_error_count == 0) {
            get_timestamp_raw(timestamp);
            printf("(write) MAX ALLOWED SOCKET TIME REACHED IN: %d | %s\n",
                  socket_state, timestamp);
            socket_error_count++;
        }
        socket_state = SOCKET_CLOSE_PENDING;
    }
    return;
}


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
int8_t read_socket(void) {
    /* Pointer to validation substring */
    char *request_ok;


    static size_t bytes_read = 0;
    size_t result, response_len;


    result = read(sockfd, response + bytes_read, RESPONSE_SIZE - bytes_read);
    if (result != -1) {
        printf("%d | %d | %s\n", (int)result, errno, strerror(errno));
        printf("%d | %d | %d | %d | %d | %d | %d | %d \n",
            EAGAIN, EWOULDBLOCK, EBADF, EFAULT, EINTR, EINVAL, EIO, EISDIR);
        bytes_read+=result;
    }

    if (RESPONSE_SIZE == bytes_read) {
        printf("Response buffer too small\n");
        return -1;
    }

    request_ok = strstr(response, "204");
    if (request_ok != NULL){
        #if(DEBUG_REQUESTS==1)
        printf("\tRESPONSE RECEIVED: \n%s\n", response);
        #endif
        // -- check for valid response
        // -- NULL if substring can not be found
        request_ok = strstr(response, "204");
        //printf("###%lu ### %s\n", 12, request_ok);

        fifo_increment_read_idx(&fifo);
        _clear_response_memory();

        bytes_read = 0;
        socket_state = SOCKET_READ_FINISHED;
        // -- reset state timer
        clock_gettime(CLOCK_MONOTONIC, &socket_state_start);
        // -- reset consecutive error counter
        if (socket_error_count != 0) {
            get_timestamp_raw(timestamp);
            printf("(read)SOCKET ERROR CURED | %d | %s\n", socket_error_count, timestamp);
            socket_error_count = 0;
        }
    }


    // -- get elapsed time in socket state
    clock_gettime(CLOCK_MONOTONIC, &socket_state_end);
    socket_elapsed_time = socket_state_end.tv_sec - socket_state_start.tv_sec;
    printf("+++%d\n", socket_elapsed_time);
    // -- max allowed time in socket state has passed
    if (socket_elapsed_time >= MAX_SOCKET_TIME) {
        // -- only sho error the first time it occured in a given interval
        if (socket_error_count == 0) {
            get_timestamp_raw(timestamp);
            printf("(read) MAX ALLOWED SOCKET TIME REACHED IN: %d | %s\n",
                   socket_state, timestamp);
            socket_error_count++;
        }
        socket_state = SOCKET_CLOSE_PENDING;
    }

    return 1;
}


/* void close_socket(void)
 *  close socket
 */
void close_socket(void) {
    close(sockfd);
    socket_state = SOCKET_CLOSED;
    return;
}


/* int setup_request_buffer(void)
 *  set up request data buffer (only for data, not full request!)
 *  returns:
 *   1 - buffer setup successful
 *   0 - error
 */
int32_t setup_request_buffer(void) {

	request_fifo_buf = (char **) malloc(sizeof(char *)*(REQUEST_FIFO_BUF_SIZE+1));
	int i = 0;

	for(i=0; i < REQUEST_FIFO_BUF_SIZE+1; i++){
		char *tmp_p = (char *) malloc(sizeof(char)*(REQUEST_FIFO_STR_SIZE+1));
		request_fifo_buf[i] = tmp_p;
	}

	request_fifo.buffer = request_fifo_buf;
	return 0;
}


/* void dummy_request(void)
 *  Set string for validating responses - in this case the device hash name.
 *  Both arrays have the same length, so null termination is not an issue.
 *   p1: Desired validation string.
 */
void set_write_validation (char *_tmp_validation) {
    memcpy(write_validation, _tmp_validation, DEVICE_HASH_LEN);
    return;
}


/* void dummy_request(void)
 *  generate dummy request data -> fill buffer
 *  ThingSpeak channel located at: https://thingspeak.com/channels/488607
 *  find fresh IP through the terminal: "nslookup api.thingspeak.com"
 */
void dummy_request(void) {
    srand ( time(NULL) );     // -- seed the random function
    float data1;
    int32_t data2;

    // -- fill buffer
    int32_t write_ok = 1;
    if (!setup_request_buffer()) {
        while (write_ok) {
            data1 = rand() % 200 * 0.1;
            data2 = rand() % 20;

            // -- ThingSpeak
            //sprintf(request_data, "api_key=%s&field1=%d", "7HN4EKKQ3ZKMOD92", data2);
            // -- Local
            sprintf(request_data, "w_speed=%.2f&w_direction=%d", data1, data2);

            write_ok=str_fifo_write(&request_fifo, request_data);
            #if(DEBUG_REQUESTS==1)
            printf("***REQUEST DATA ADDED: \n%s\n", request_data);
            #endif
        }
    }
    return;
}


int8_t _clear_response_memory(void) {
    memset(response, 0, RESPONSE_SIZE);
    return 0;
}
