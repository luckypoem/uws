#include "uws_header.h"
#include "uws_utils.h"
#include "uws_memory.h"
char* get_header_param(char* key, struct http_header *http_header){
    int i = 0;
    if(http_header->params == NULL)
        return NULL;
    while(i < http_header->used_len){
        if(strcmp(key, http_header->params[i].name) == 0) {
            return http_header->params[i].value;
        }
        i++;
    }
    return NULL;
}

void add_header_param(char *key, char *value, struct http_header *http_header){
    int i = 0;
    if(http_header->params == NULL) 
    {
        http_header->params = (Http_Param*) uws_malloc(INIT_PARAMS_NUM * sizeof(Http_Param));
        http_header->used_len = 0;
        http_header->max_len = INIT_PARAMS_NUM;
    }
    for(i = 0; i < http_header->used_len; i++) {
        if(strcmp(key, http_header->params[i].name) == 0) {
            uws_free(http_header->params[i].value);
            http_header->params[i].value = uws_strdup(value);
            return;
        }
    }
    if(http_header->used_len == http_header->max_len - 1)
    {
        http_header->max_len *= 2;
        Http_Param* tmp = http_header->params;
        http_header->params = (Http_Param*)uws_realloc(http_header->params, http_header->used_len * sizeof(Http_Param), http_header->max_len * sizeof(Http_Param));
    }
    Http_Param* new_param = &http_header->params[http_header->used_len];
    new_param->name = uws_strdup(key);
    new_param->value = uws_strdup(value);
    http_header->used_len++;
}
void push_header_param(char *key, char *value, struct http_header *http_header){
    if(http_header->params == NULL) 
    {
        http_header->params = (Http_Param*) uws_malloc(INIT_PARAMS_NUM * sizeof(Http_Param));
        http_header->used_len = 0;
        http_header->max_len = INIT_PARAMS_NUM;
    }
    if(http_header->used_len == http_header->max_len - 1)
    {
        http_header->max_len *= 2;
        Http_Param* tmp = http_header->params;
        http_header->params = (Http_Param*)uws_malloc(http_header->max_len * sizeof(Http_Param));
        memcpy(http_header->params, tmp, http_header->used_len);
        uws_free(tmp);
    }
    Http_Param* new_param = &http_header->params[http_header->used_len];
    new_param->name = uws_strdup(key);
    new_param->value = uws_strdup(value);
    http_header->used_len++;
}
void free_header_params(struct http_header *http_header)
{
    int i = 0;
    for(i = 0; i < http_header->used_len; i++) {
        uws_free(http_header->params[i].name);
        uws_free(http_header->params[i].value);
    }
    uws_free(http_header->params);
    http_header->params = NULL;
    http_header->used_len = 0;
}
char* str_response_header(struct http_header *header) {
    char *response_str = (char*)  uws_malloc(HEADER_LEN * sizeof(char));   
    int i;
    sprintf(response_str, "%s %d %s", header->http_ver, header->status_code, header->status);
    strcat(response_str, "\r\n");
    for(i = 0; i < header->used_len; i++) {
        strcat(response_str, header->params[i].name);
        strcat(response_str, ": ");
        strcat(response_str, header->params[i].value);
        strcat(response_str, "\r\n");
    }
    return response_str;
}
char* str_request_header(struct http_header *header) {
    char *request_str = (char*)  uws_malloc(HEADER_LEN * sizeof(char));   
    int i;
    sprintf(request_str, "%s %s %s", header->method, header->url, header->http_ver);
    strcat(request_str, "\r\n");
    for(i = 0; i < header->used_len; i++) {
        strcat(request_str, header->params[i].name);
        strcat(request_str, ": ");
        strcat(request_str, header->params[i].value);
        strcat(request_str, "\r\n");
    }
    return request_str;
}
