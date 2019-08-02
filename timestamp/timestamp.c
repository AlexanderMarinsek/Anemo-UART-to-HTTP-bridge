
#include "timestamp.h"

#include <stdint.h>         /* Data types */
#include <time.h>           /* For timestamp */
#include <string.h>         /* For memory operations */
#include <stdio.h>          /* Standard input/output definitions */


/* LOCALS *********************************************************************/

/* Calendar time (Epoch time) - usually signed int */
static time_t time_epoch;
/* Pointer to time structure, for specifications view 'ctime' at 'man7' */
//static struct tm *time_human;
/* Space for temporary timestamp */
//static char timestamp [TIMESTAMP_STRING_SIZE];


/* PROTOTYPES *****************************************************************/
static int8_t _refresh_timestamp (void);


/* FUNCTIONS (GLOBALS) ********************************************************/

/*  Get latest raw formated timestamp.
 */
int8_t get_timestamp_raw (char *_timestamp) {

    if (strlen(_timestamp) > TIMESTAMP_RAW_STRING_SIZE-1) {
        return -1;
    }

    _refresh_timestamp ();

    // -- convert to standardised timestamp
    /*sprintf(_timestamp, TIMESTAMP_RAW_FORMAT,
        time_human->tm_year+1900, time_human->tm_mon+1, time_human->tm_mday,
        time_human->tm_hour, time_human->tm_min);*/

    /* Convert to UTC and format ISO 8601 */
    strftime(_timestamp, TIMESTAMP_JSON_STRING_SIZE,
		TIMESTAMP_UTC_RAW_FORMAT, gmtime(&time_epoch));

    return 0;
}


/*  Get latest JSON formated timestamp with succeeding comma symbol.
 */
int8_t get_timestamp_json_w_comma (char *_timestamp) {

    if (strlen(_timestamp) > TIMESTAMP_JSON_STRING_SIZE-1) {
        return -1;
    }

    _refresh_timestamp ();

    // -- convert to standardised timestamp
    /*sprintf(_timestamp, TIMESTAMP_JSON_FORMAT_W_COMMA,
        time_human->tm_year+1900, time_human->tm_mon+1, time_human->tm_mday,
        time_human->tm_hour, time_human->tm_min);*/

    /* Convert to UTC and format ISO 8601 */
    strftime(_timestamp, TIMESTAMP_JSON_STRING_SIZE,
		TIMESTAMP_UTC_JSON_FORMAT_W_COMMA, gmtime(&time_epoch));


    return 0;
}

int8_t get_timestamp_epoch(long int *_time_epoch) {
	_refresh_timestamp();
//	/* Make sure 'time_t' is equal to 'long int' */
	*_time_epoch = (long int) time_epoch;
	return 0;
}


/* FUNCTIONS (LOCAL) **********************************************************/

/*  Get latest timestamp from the system and store in timestamp buffer.
 */
static int8_t _refresh_timestamp (void) {
    /* Save current time since Unix Epoch in seconds */
    //time_epoch = time(NULL);
	time(&time_epoch);
    /* Save current broken down time to time structure */
    //time_human = localtime(&time_epoch);

    return 0;
}
