#ifndef STORAGE_TASK
#define STORAGE_TASK

#include "../fifo/fifo.h"
#include "../serial/serial.h"

#include <stdint.h>     /* Data types */


#define DATA_SAVE_FIFO_BUF_SIZE (10)
#define DATA_SAVE_FIFO_STR_SIZE (FIFO_STRING_SIZE)
#define FILENAME_STRING_LEN 64


/*  Init raw serial data fifo.
 *   p1: pointer to fifo struct pointer
 *  return: 0 on success, -1 on error
 */
int8_t storage_task_init_fifo (str_fifo_t **_fifo);

/*  Init filename.
 *   p1: pointer to fifo struct pointer
 *  return: 0 on success, -1 on error
 */
int8_t storage_task_init_file (char *filename);
int8_t storage_task_run (void);

#endif
