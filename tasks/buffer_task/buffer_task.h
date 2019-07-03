#ifndef BUFFER_TASK
#define BUFFER_TASK

#include "../../fifo/fifo.h"
//#include "../../serial/serial.h"

#include <stdint.h>


#define JSON_INCOMING_IDLE_MASK             (0x00)
#define JSON_INCOMING_COPY_MASK             (0x01)
#define JSON_INCOMING_STOP_COPY_MASK        (0x02)

#define EXPECTED_JSON_DEPTH                 (2)


/* JSON string buffer */
struct Json_str_buffer {
    char buffer[FIFO_STRING_SIZE] ;
    uint16_t current_write_idx;
    //uint16_t start_write_idx;
};

/* JSON incoming data */
struct Json_incoming {
    struct Json_str_buffer str_buffer;
    uint8_t num_of_nested_obj;
    uint8_t status;
};


/*  Get latest row of raw serial data, look for JSON and if present, copy to
 *  local storage and requests buffer.
 *   p1: pointer to array of fifo struct pointers
 *  return: 0 on success, -1 on error
 */
int8_t buffer_task_init (str_fifo_t *_fifo_buffers[3]);

/*  Get latest row of raw serial data, look for JSON and if present, copy to
 *  local storage and requests buffer.
 *  return: 0 on success, -1 on error
 */
int8_t buffer_task_run (void);


#endif
