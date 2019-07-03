#ifndef FIFO_H_
#define FIFO_H_

#include <stdint.h>


#define FIFO_STRING_SIZE                    (512)


struct _str_fifo {
	uint32_t read_idx;
	uint32_t write_idx;
	uint32_t buf_size;
	uint32_t str_size;
	char **buffer;
};

typedef struct _str_fifo str_fifo_t;

/* int8_t str_fifo_read(fifo_t *fifo, char *data);
 *  function for reading from fifo buffer of strings
 *   fifo - address of fifo for reading
 *   data - address of where read data are stored
 *
 *   returns 0 if data was successfully read, else 1 (buffer empty)
 */
int8_t str_fifo_read(str_fifo_t *fifo, char *data);

int8_t str_fifo_read_auto_inc(str_fifo_t *fifo, char *data);

/* int8_t str_fifo_write(fifo_t *fifo, char *data);
 *  function for writing to fifo buffer of strings
 *   fifo - address of fifo for writing
 *   data - address of data to be written into fifo
 *
 *   returns 0 if data was successfully written, else 1
 */
int8_t str_fifo_write(str_fifo_t *fifo, char *data);

/* int str_increment_read(str_fifo_t *fifo)
 *  manually increment fifo read pointer, only turn fifo after incrementation
 *   fifo - address of fifo for writing
 *
 *  returns:
 *   0 - read pointer incremented (new data available)
 *   1 - fifo empty
 */
int8_t fifo_increment_read_idx(str_fifo_t *fifo);

/* int8_t setup_request_buffer(void)
 *  allocate space for new fifo buffer containing strings
 *   request data buffer: only for data, not full request!
 *   data_save buffer: new line for output file
 *
 *  returns:
 *   0 - buffer setup successful
 *   -1 - error
 */
int8_t setup_str_fifo (str_fifo_t *fifo, int32_t buf_size, int32_t str_size);

#endif //FIFO_H_
