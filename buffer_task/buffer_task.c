#include "buffer_task.h"
#include "../fifo/fifo.h"
#include "../serial/serial.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>      /* Standard input/output definitions */
#include <time.h>


/* LOCALS *********************************************************************/

/* Local copy of pointer to three fifo buffers
 *  1: Raw incoming UART data
 *  2: Data storage buffer
 *  3: Requests buffer
 */
static str_fifo_t *fifo_buffers[3];

/* Store oldest entry from raw serial fifo buffer */
static char tmp_serial_buffer[FIFO_STRING_SIZE];

/* Save incoming JSON data to buffer */
static struct Json_incoming json_incoming;

/* Check that all levels (nested objects) of JSON object were noticed */
static int8_t json_depth_valid = -1;

/* Calendar time (Epoch time) - usually signed int */
static time_t time_epoch;
/* Pointer to time structure, for specifications view 'ctime' at 'man7' */
static struct tm *time_human;
/* Space for temporary timestamp */
static char timestamp [TIMESTAMP_STRING_SIZE];


/* PROTOTYPES *****************************************************************/

static int8_t _get_json_from_raw (void);
static int8_t _reset_json_str_buffer(void);

static void _set_json_incoming_status_to_copy (void);
static void _set_json_incoming_status_to_copy_stop (void);
static void _set_json_incoming_status_to_idle (void);

static int8_t _is_json_string_copy(void);
static int8_t _is_json_string_buffer_full(void);
static int8_t _is_json_string_copy_stop(void);

static int8_t _refresh_timestamp (void);
static int8_t _add_timestamp_to_json (void);


/* FUNCTIONS ******************************************************************/

/*  Get latest row of raw serial data, look for JSON and if present, copy to
 *  local storage and requests buffer.
 */
int8_t buffer_task_init (str_fifo_t *_fifo_buffers[3]) {
    /* Point to buffers */
    int i;
    for (i=0; i<3; i++) {
        fifo_buffers[i] = _fifo_buffers[i];
        printf("-Address: %p, %d\n", (void *)&_fifo_buffers[i], i);
        printf("-Address: %p, %d\n", (void *)_fifo_buffers[i], i);
        printf("-Address: %p, %d\n", (void *)&fifo_buffers[i], i);
        printf("-Address: %p, %d\n", (void *)fifo_buffers[i], i);
    }

    json_incoming.num_of_nested_obj = 0;
    _set_json_incoming_status_to_idle();

    return 0;
}

/*  Get latest row of raw serial data, look for JSON and if present, copy to
 *  local storage and requests buffer.
 */
int8_t buffer_task_run (void) {
    /* Clear serial buffer */
    memset(tmp_serial_buffer, 0, FIFO_STRING_SIZE);
    /* If available, read raw string from fifo buffer */
    if (str_fifo_read_auto_inc(fifo_buffers[0], tmp_serial_buffer) == 0) {
        /* Check for JSON format */
        if (_get_json_from_raw() == 0) {
            printf("FOUND\n");
            printf("%s\n", json_incoming.str_buffer.buffer);

            /* Get latest timestamp from system */
            _refresh_timestamp();
            /* Add timestamp to JSON string */
            _add_timestamp_to_json();

            str_fifo_write(fifo_buffers[1], json_incoming.str_buffer.buffer);

            //str_fifo_t *fifo_ = fifo_buffers[1];

            char data[512];
            str_fifo_read(fifo_buffers[1], data);
            printf ("###%s\n", data);
            str_fifo_read(fifo_buffers[1], data);
            printf ("###%s\n", data);
            str_fifo_read(fifo_buffers[1], data);
            printf ("###%s\n", data);

            printf ("%s\n", json_incoming.str_buffer.buffer);
            // Save to other buffers

            _reset_json_str_buffer();
        }
    }
    return 0;
}


/* JSON ***********************************************************************/

/*  Get JSON string by reading strings strored in raw serial fifo buffer.
 *  Copy oldest row from fifo serial buffer and look for JSON format. If JSON
 *  format is present, save it to JSON buffer for later use.
 */
static int8_t _get_json_from_raw (void) {

    /* Iterate string in raw serial fifo buffer */
    int i;
    for (i=0; i<FIFO_STRING_SIZE; i++) {
        /* Get JSON opening braces */
        if (tmp_serial_buffer[i] == '{') {
            /* Increment number of nested objects */
            json_incoming.num_of_nested_obj++;
            /* Whole JSON object was noticed */
            if (json_incoming.num_of_nested_obj == EXPECTED_JSON_DEPTH) {
                json_depth_valid = 0;
            }
            /* Outer JSON braces */
            if (json_incoming.num_of_nested_obj == 1) {
                json_depth_valid = -1;
                _set_json_incoming_status_to_copy();
                /* Reset JSON buffer */
                _reset_json_str_buffer();
            }
        }
        /* Get JSON closing braces */
        else if (tmp_serial_buffer[i] == '}') {
            /* Opening braces were not already found */
            if (json_incoming.num_of_nested_obj <= 0) {
                _set_json_incoming_status_to_idle();
                _reset_json_str_buffer();
                return 1;
            }
            /* Decrement number of nested objects */
            json_incoming.num_of_nested_obj--;
            /* Reached end of JSON */
            if (json_incoming.num_of_nested_obj == 0) {
                /* Only inner JSON object was copied */
                if (json_depth_valid != 0) {
                    _set_json_incoming_status_to_idle();
                    _reset_json_str_buffer();
                    return 1;
                }
                _set_json_incoming_status_to_copy_stop();
            }
        }
        /* Serial buffer is zero padded at the end */
        else if (tmp_serial_buffer[i] == 0) {
            return 1;
        }

        /* Copy char to JSON buffer */
        if (_is_json_string_copy() == 0) {
            json_incoming.str_buffer.buffer
                [json_incoming.str_buffer.current_write_idx] =
                tmp_serial_buffer[i];
            json_incoming.str_buffer.current_write_idx++;
        }

        /* Check, that space for null specifier is still available */
        if (_is_json_string_buffer_full() == 0) {
            printf("Error: incoming too long, json buffer full\n");
            _set_json_incoming_status_to_idle();
            _reset_json_str_buffer();
        }

        /* Return to idle mode */
        if (_is_json_string_copy_stop() == 0) {
            /* Add null at end */
            json_incoming.str_buffer.buffer
                [json_incoming.str_buffer.current_write_idx] = '\0';
            printf("---%s---\n", json_incoming.str_buffer.buffer);
            _set_json_incoming_status_to_idle();
            memset(tmp_serial_buffer, 0, FIFO_STRING_SIZE);
            return 0;
        }
    }
    return 1;
}


/* Reset JSON buffer
 *
 */
int8_t _reset_json_str_buffer (void) {
    memset(json_incoming.str_buffer.buffer, 0, FIFO_STRING_SIZE);
    json_incoming.str_buffer.current_write_idx = 0;
    json_incoming.str_buffer.start_write_idx = 0;
    return 0;
}

/* Set JSON status
 */
static void _set_json_incoming_status_to_copy (void) {
    json_incoming.status |= JSON_INCOMING_COPY_MASK;
}
static void _set_json_incoming_status_to_copy_stop (void) {
    json_incoming.status |= JSON_INCOMING_STOP_COPY_MASK;
}
static void _set_json_incoming_status_to_idle (void) {
    json_incoming.status = JSON_INCOMING_IDLE_MASK;
}

/* Check JSON status
 */
static int8_t _is_json_string_copy(void){
    return (!(
        json_incoming.status & JSON_INCOMING_COPY_MASK
    ));
}
static int8_t _is_json_string_buffer_full(void){
    return (!(
        json_incoming.str_buffer.current_write_idx >=
        FIFO_STRING_SIZE - 1
    ));
}
static int8_t _is_json_string_copy_stop(void){
    return (!(
        json_incoming.status & JSON_INCOMING_STOP_COPY_MASK
    ));
}


/* TIMESTAMP ******************************************************************/

/*  Add to system's timestamp to JSON format. Add it outside of 'data', so that
 *  Linux and possible measuring station timestamps are kept separate.
 */
static int8_t _add_timestamp_to_json (void) {
    char tmp_json[FIFO_STRING_SIZE] = {0};

    int tmp_write_idx = 0;
    int buf_write_idx = 0;

    tmp_json[tmp_write_idx] = json_incoming.str_buffer.buffer[buf_write_idx];
    tmp_write_idx++;
    buf_write_idx++;

    memcpy(&tmp_json[tmp_write_idx], timestamp, strlen(timestamp));
    tmp_write_idx += strlen(timestamp);

    memcpy(&tmp_json[tmp_write_idx],
        &json_incoming.str_buffer.buffer[buf_write_idx],
        strlen(json_incoming.str_buffer.buffer));

    memcpy(json_incoming.str_buffer.buffer, tmp_json, strlen(tmp_json) + 1);

    return 0;
}

/*  Get latest timestamp from the system and store in timestamp buffer.
 */
static int8_t _refresh_timestamp (void) {
    // -- save current time since Unix Epoch in seconds
    time_epoch = time(NULL);
    // -- save current broken down time to time structure
    time_human = localtime(&time_epoch);

    // -- convert to standardised timestamp
    sprintf(timestamp, TIMESTAMP_KEY_FORMAT,
        time_human->tm_year+1900, time_human->tm_mon+1, time_human->tm_mday,
        time_human->tm_hour, time_human->tm_min);

    printf("%s\n", timestamp);

    return 0;
}
