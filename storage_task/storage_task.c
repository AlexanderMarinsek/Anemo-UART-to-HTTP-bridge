
#include "storage_task.h"
#include "../fifo/fifo.h"

#include <stdio.h>      /* Standard input/output definitions */
#include <stdint.h>     /* Data types */
#include <time.h>       /* For timestamp */
#include <string.h>     /* For memcpy, strlen */


// -- GLOBALS ------------------------------------------------------------------

// -- data save buffer
str_fifo_t data_save_fifo = {
    0,
    0,
	DATA_SAVE_FIFO_BUF_SIZE,
    DATA_SAVE_FIFO_STR_SIZE,
    NULL
};

// -- file state (status) indicator
int32_t file_state;

// -- file currently in use
FILE *ofp;


// -- LOCALS ------------------------------------------------------------------



/* Fifo for data storage */
static str_fifo_t fifo = {
	0,
	0,
	DATA_SAVE_FIFO_BUF_SIZE,
	DATA_SAVE_FIFO_STR_SIZE,
	NULL
};

static char filename[FILENAME_STRING_LEN];

// -- temporary string used for storing one 'line' of data on fifo read/write
char data_save_str[DATA_SAVE_FIFO_STR_SIZE];

/*  Point outer pointer to local fifo struct and init storage for fifo
 *   p1: pointer to pointer, pointing to fifo struct
 *
 */
int8_t storage_task_init_fifo (str_fifo_t **_fifo) {
    /* Set outer pointer to point to fifo local struct */
    *_fifo = &fifo;
    /* Set up memory for fifo struct and return success/error */
    return setup_str_fifo(
        &fifo, DATA_SAVE_FIFO_BUF_SIZE, DATA_SAVE_FIFO_STR_SIZE);
}


int8_t storage_task_init_file (char *_filename) {
    if (strlen(_filename) > FILENAME_STRING_LEN-1) {
        printf("Error: storage_task_init_file\n");
        return -1;
    }
    memcpy(filename, _filename, strlen(_filename)+1);
    return 0;
}


int8_t storage_task_run (void) {
   if (str_fifo_read_auto_inc(&fifo, data_save_str) == 0) {
        ofp = fopen(filename, "a");
        // -- move to output buffer and flush immediately
        fprintf(ofp, "%s\n", data_save_str);
        fflush(ofp);
        fclose(ofp);
   }

   return 0;
}
