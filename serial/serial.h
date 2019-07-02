#ifndef SERIAL_H
#define SERIAL_H

#include "../fifo/fifo.h"
#include <stdint.h>                 /* Data types */


#define PORT_PATHNAME                       "/dev/ttyACM0"

#define SERIAL_FIFO_BUFFER_SIZE             (64)

#define TIMESTAMP_STRING_SIZE               (64)
#define FIFO_STRING_SIZE                    (512)
#define RAW_FIFO_STRING_SIZE                \
    (FIFO_STRING_SIZE - TIMESTAMP_STRING_SIZE)

#define TIMESTAMP_KEY_FORMAT                \
    "\"timestamp\":\"%04d-%02d-%02dT%02d:%02d\","


/*class SerialImp : public Serial {
public:
    SerialImp(void);
    int8_t init_port (void);
    int8_t init_fifo(str_fifo_t *fifo);
protected:
    int8_t open_port(void);
    int8_t set_up_port (void);
    int8_t add_signal_handler_IO (void);

    void signal_handler_IO (int status);

    str_fifo_t *fifo;
};*/


/*  Init serial port.
 *  return: 0 on success, -1 on error
 */
int8_t serial_init_port (void);

/*  Init raw serial data fifo.
 *   p1: pointer to fifo struct pointer
 *  return: 0 on success, -1 on error
 */
int8_t serial_init_fifo(str_fifo_t **fifo);


#endif
