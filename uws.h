#ifndef __UWS_H__
#define  __UWS_H__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define UWS_SERVER "uws/0.0.3"

#define LINE_LEN    256
#define OPT_LEN     20
#define VLU_LEN     50
#define PATH_LEN    512
#define bool        int
#define true        1
#define false       0

void exit_err(const char* str);

typedef struct nv_pair {
    char* name;
    char* value;
}Http_Param, Param_Value;

struct response {
    struct http_header    *header;
    size_t header_len;
    char    *content;
    size_t content_len;
};
char* find_value(char* key);

#endif

