#ifndef TIMESTAMP_H
#define TIMESTAMP_H


#include <stdint.h>         /* Data types */


#define TIMESTAMP_RAW_STRING_SIZE               (32)
#define TIMESTAMP_JSON_STRING_SIZE              (64)

#define TIMESTAMP_RAW_FORMAT                "%04d-%02d-%02dT%02d:%02d"
#define TIMESTAMP_JSON_FORMAT_W_COMMA      \
    "\"timestamp\":\"%04d-%02d-%02dT%02d:%02d\","

#define TIMESTAMP_UTC_RAW_FORMAT                "%FT%TZ"
#define TIMESTAMP_UTC_JSON_FORMAT_W_COMMA      \
    "\"timestamp\":\"%FT%TZ\","


/*  Get latest raw formated timestamp.
 *   p1: pointer to where timestamp should be written
 *  return: 0 on success, -1 on error
 */
int8_t get_timestamp_raw (char *_timestamp);

/*  Get latest JSON formated timestamp with succeeding comma symbol.
 *   p1: pointer to where timestamp should be written
 *  return: 0 on success, -1 on error
 */
int8_t get_timestamp_json_w_comma (char *_timestamp);

int8_t get_timestamp_epoch(long int *_time_epoch);


#endif
