
#include "storage_task.h"
#include "../../fifo/fifo.h"

#include <stdio.h>      /* Standard input/output definitions */
#include <stdint.h>     /* Data types */
#include <time.h>       /* For timestamp */
#include <string.h>     /* For memcpy, strlen */



/* LOCALS *********************************************************************/

/* Fifo for data storage */
static str_fifo_t fifo = {
	0,
	0,
	STORAGE_FIFO_BUF_SIZE,
	STORAGE_FIFO_STR_SIZE,
	NULL
};

static char filename[FILENAME_STRING_LEN];

/* temporary string used for storing one 'line' of data on fifo read/write */
char data_save_str[STORAGE_FIFO_STR_SIZE];

/* file currently in use */
FILE *ofp;


/* FUNCTIONS (GLOBAL) *********************************************************/

/*  Point outer pointer to local fifo struct and init storage for fifo
 *   p1: pointer to pointer, pointing to fifo struct
 *  return: 0 on success, -1 on error
 */
int8_t storage_task_init_fifo (str_fifo_t **_fifo) {
    /* Set outer pointer to point to fifo local struct */
    *_fifo = &fifo;
    /* Set up memory for fifo struct and return success/error */
    return setup_str_fifo(
        &fifo, STORAGE_FIFO_BUF_SIZE, STORAGE_FIFO_STR_SIZE);
}


int8_t storage_task_init_file (char *_filename) {
    /* Check length */
    if (strlen(_filename) > FILENAME_STRING_LEN-1) {
        return -1;
    }
	/* Copy to local string (including '/0') */
    memcpy(filename, _filename, strlen(_filename)+1);
    return 0;
}


int8_t storage_task_run (void) {
	//printf("STORAGE TASK\n");
	if (str_fifo_read_auto_inc(&fifo, data_save_str) == 0) {
		ofp = fopen(filename, "a");
		// -- move to output buffer and flush immediately
		fprintf(ofp, "%s\n", data_save_str);
		fflush(ofp);
		fclose(ofp);
	}

	return 0;
}
