#ifndef __UWS_H__
#define  __UWS_H__
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#define LINE_LEN    256
#define OPT_LEN     20
#define VLU_LEN     50
#define PATH_LEN    512


void exit_err(const char* str);

typedef struct nv_pair {
    char* name;
    char* value;
}Http_Param, Param_Value;

struct response {
    char    *header;
    int     header_len;
    char    *content;
    int     content_len;
};
struct http_header{
    char* method;
    char* url;
    char* http_ver;
    Http_Param* params;
};


#endif
