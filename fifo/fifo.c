#include "fifo.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> /* exit */


/* int8_t str_fifo_read(fifo_t *fifo, char *data);
 *  function for reading from fifo buffer of strings
 *   fifo - address of fifo for reading
 *   data - address of where read data are stored
 *
 *   returns 0 if data was successfully read, else 1 (buffer empty)
 */
int8_t str_fifo_read(str_fifo_t *fifo, char *data){
	uint32_t i=0;

	if(fifo->write_idx != fifo->read_idx){
		for(i=0; i < fifo->str_size; i++){
			data[i] = fifo->buffer[fifo->read_idx][i];
		}
		//fifo->read_idx = (fifo->read_idx+1)%fifo->buf_size;
        return 0;
	}
	else {
		return 1;
	}
}

int8_t str_fifo_read_auto_inc(str_fifo_t *fifo, char *data){
	str_fifo_read(fifo, data);
	return fifo_increment_read_idx(fifo);
}



/* int8_t str_fifo_write(fifo_t *fifo, char *data);
 *  function for writing to fifo buffer of strings
 *   fifo - address of fifo for writing
 *   data - address of data to be written into fifo
 *
 *   returns 0 if data was successfully written, else 1
 */
int8_t str_fifo_write(str_fifo_t *fifo, char *data){
	uint32_t i=0;
	uint32_t tmp_write_idx = (fifo->write_idx+1)%fifo->buf_size;

    /* Allow circular overwrite.
     * Always keep read_idx at leats one in front write_ix.
     */
	if(tmp_write_idx == fifo->read_idx){
        fifo->read_idx = (fifo->read_idx+1)%fifo->buf_size;
		printf("Fifo: circular overwrite (address: %p)\n", (void *)fifo);
    }
    for(i=0; i < fifo->str_size; i++){
        fifo->buffer[fifo->write_idx][i] = data[i];
    }
    fifo->write_idx = tmp_write_idx;
    return 0;
}


/* int8_t fifo_increment_read(str_fifo_t *fifo)
 *  manually increment fifo read pointer, only turn fifo after incrementation
 *   fifo - address of fifo for writing
 *
 *  returns:
 *   0 - read pointer incremented (new data available)
 *   1 - fifo empty
 */
int8_t fifo_increment_read_idx(str_fifo_t *fifo){
    // -- check if fifo is empty
    if (fifo->read_idx == fifo->write_idx) return 1;
    // -- increment read pointer (point to fresh data)
    fifo->read_idx = (fifo->read_idx+1)%fifo->buf_size;

    return 0;
}


/* int8_t setup_request_buffer(void)
 *  allocate space for new fifo buffer containing strings
 *   request data buffer: only for data, not full request!
 *   data_save buffer: new line for output file
 *
 *  returns:
 *   0 - buffer setup successful
 *   -1 - error
 */
int8_t setup_str_fifo (str_fifo_t *fifo, int32_t buf_size, int32_t str_size) {
	// -- initialie seperately to avoid 'is not constant' error
    static char **tmp_fifo_buf=NULL;

    tmp_fifo_buf= (char **) malloc(sizeof(char *)*(buf_size+1));

    int32_t i = 0;

	for(i=0; i < buf_size+1; i++){
		char *tmp_p = (char *) malloc(sizeof(char)*(str_size+1));
		tmp_fifo_buf[i] = tmp_p;
	}

	fifo->buffer = tmp_fifo_buf;
	return 0;
}
