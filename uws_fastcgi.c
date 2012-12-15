#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "uws_memory.h"
#include "uws_fastcgi.h"
#include "uws_header.h"
#include "uws_config.h"
#include "uws_utils.h"
#include "uws_status.h"
#include "uws_router.h"
#include "uws_http.h"
#define PARAMS_BUFF_LEN     1024
#define MAX_BODY_LEN        2048

/*{{{*/
static char *
header_to_fcgi(const char *str) 
{
    int prefix_len = strlen("HTTP_");
    int len = strlen(str) + prefix_len + 2;
    char *newstr = (char*) uws_malloc(len * sizeof(char));
    int i = 0;
    strcpy(newstr, "HTTP_");
    while(str[i]) {
        if(str[i] == '-') {
            newstr[i + prefix_len] = '_';
        } else {
            newstr[i + prefix_len] = toupper(str[i]);
        }
        i++;
    }
    newstr[i+ prefix_len] = 0;
    return newstr;
}

static FCGI_Header
make_header(int type, int request_id, int content_len, int padding_len)
{
    FCGI_Header header;
    header.version          =           FCGI_VERSION_1;
    header.type             =           (unsigned char) type;
    header.requestIdB1      =           (unsigned char) ((request_id >> 8) & 0xff);
    header.requestIdB0      =           (unsigned char) (request_id & 0xff);
    header.contentLengthB1  =           (unsigned char) ((content_len >> 8) & 0xff);
    header.contentLengthB0  =           (unsigned char) (content_len & 0xff);
    header.paddingLength    =           (unsigned char) padding_len;
    header.reserved         =           0;
    return header;
}
static FCGI_BeginRequestBody
make_begin_request_body(int role, int keep_conn)
{
    FCGI_BeginRequestBody body;
    body.roleB1         =           (unsigned char) ((role >> 8) & 0xff);
    body.roleB0         =           (unsigned char) (role & 0xff);
    body.flags          =           (unsigned char) (keep_conn ? FCGI_KEEP_CONN : 0);
    bzero(body.reserved, sizeof(body.reserved));
    return body;
}

static void
build_name_value_body(char *name, int name_len, char *value, int value_len, unsigned char *body_buff, int *body_len)
{
    unsigned char *start_body_buff = body_buff;
    if( name_len < 0x80) {
        *body_buff++ = (unsigned char) name_len;
    } else {
        *body_buff++ = (unsigned char) ((name_len >> 24) | 0x80);
        *body_buff++ = (unsigned char) (name_len >> 16);
        *body_buff++ = (unsigned char) (name_len >> 8);
        *body_buff++ = (unsigned char) name_len;
    }

    if( value_len < 0x80) {
        *body_buff++ = (unsigned char) value_len;
    } else {
        *body_buff++ = (unsigned char) ((value_len >> 24) | 0x80);
        *body_buff++ = (unsigned char) (value_len >> 16);
        *body_buff++ = (unsigned char) (value_len >> 8);
        *body_buff++ = (unsigned char) value_len;
    }
    while(*name != '\0') *body_buff++ = *name++;
    while(*value != '\0') *body_buff++ = *value++;
    *body_len = body_buff - start_body_buff;
}

static void
add_fcgi_param(int request_id, char* name, char* value, memory_t *smem) {
    int name_len, value_len, body_len;
    unsigned char body_buff[PARAMS_BUFF_LEN];
    bzero(body_buff, PARAMS_BUFF_LEN);
    name_len        =           strlen(name);
    value_len       =           strlen(value);
    build_name_value_body(name, name_len, value, value_len, &body_buff[0], &body_len);
    
    FCGI_Header name_value_header;
    name_value_header = make_header(FCGI_PARAMS, request_id, body_len, 0);

    append_mem_t(smem, &name_value_header, FCGI_HEADER_LEN);
    append_mem_t(smem, body_buff, body_len);
}
static void
begin_build_request(int request_id, memory_t *smem) {
    FCGI_BeginRequestRecord begin_record;
    begin_record.header = make_header(FCGI_BEGIN_REQUEST, request_id, sizeof(begin_record.body), 0);
    begin_record.body = make_begin_request_body(FCGI_RESPONDER, 0);
    append_mem_t(smem, &begin_record, sizeof(begin_record));
}/*}}}*//*}}}*/

static int
prepare_request(pConnInfo conn_info, const char* host, int port) {
    int result, sockfd;
    struct sockaddr_in address;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(host);
    address.sin_port = htons(port);
    result = connect(sockfd, (struct sockaddr*)&address, sizeof(address));
    if(result == -1) {
        return 0;
    }
    return sockfd;
}

static void 
send_request(int sockfd, memory_t *smem) {
    writen(sockfd, smem->mem, smem->len);
}

static bool
read_response(int sockfd, memory_t *mem_file)
{
    int count;
    FCGI_Header response_header;

    //We defined max 1024*1024*2 byte per response
    int already_read = 1024 * 1024 * 2;

    unsigned char* content;
    int content_len;
    char tmp[8];
    bzero(mem_file, sizeof(mem_file));
    while(read(sockfd, &response_header, FCGI_HEADER_LEN) > 0) {

        if(mem_file->len >= already_read) return true;

        if(response_header.type == FCGI_STDOUT) {
            content_len = (response_header.contentLengthB1 << 8) + (response_header.contentLengthB0);
            content = (unsigned char*) uws_malloc(sizeof(char) * content_len);

            count = read(sockfd, content, content_len);

            append_mem_t(mem_file, content, content_len);

            uws_free(content);
            if(response_header.paddingLength > 0) {
                count = read(sockfd, tmp, response_header.paddingLength);
                if(count != response_header.paddingLength) perror("read response error");
            }
        }
        else if(response_header.type == FCGI_STDERR) {
            content_len = (response_header.contentLengthB1 << 8) + (response_header.contentLengthB0);
            content = (unsigned char*) uws_malloc(content_len * sizeof(char));
            count = read(sockfd, content, count);
            uws_free(content);

            if(response_header.paddingLength > 0) {
                count = read(sockfd, tmp, response_header.paddingLength);
                if(count != response_header.paddingLength) perror("read");
            }
        }
        else if(response_header.type == FCGI_END_REQUEST) {
            FCGI_EndRequestBody end_request;
            count = read(sockfd, &end_request, FCGI_HEADER_LEN);

            /*
            if(count != 8) perror("read");
fprintf(stdout,"\nend_request:appStatus:%d,protocolStatus:%d\n",(end_request.appStatusB3<<24)+(end_request.appStatusB2<<16) +(end_request.appStatusB1<<8)+(end_request.appStatusB0),end_request.protocolStatus);
*/

        }
    }
    close(sockfd);
    return false;
}

void
fastcgi_router(pConnInfo conn_info) 
{
    memory_t smem;
    int sockfd = conn_info->clientfd;
    char *port = itoa(conn_info->running_server->listen);
    int request_id = 1;
    Param_Value pv[] = {
        {"QUERY_STRING",conn_info->request_header->request_params},
        {"REQUEST_METHOD", conn_info->request_header->method},
        {"CONTENT_TYPE", nullstring(get_header_param("Content-Type", conn_info->request_header))},
        {"CONTENT_LENGTH", nullstring(get_header_param("Content-Length", conn_info->request_header))},
        {"SCRIPT_FILENAME", conn_info->request_header->path},
        {"SCRIPT_NAME", strrchr(conn_info->request_header->path, '/')},
        {"REQUEST_URI", conn_info->request_header->url},
        {"DOCUMENT_URI", conn_info->request_header->path + strlen(conn_info->running_server->root)},
        {"DOCUMENT_ROOT", conn_info->running_server->root},
        {"SERVER_PROTOCOL", conn_info->request_header->http_ver},
        {"GATEWAY_INTERFACE", "CGI/1.1"},
        {"SERVER_SOFTWARE", UWS_SERVER},
        {"REMOTE_ADDR", get_header_param("Client-IP", conn_info->request_header)},
        {"REMOTE_PORT", get_header_param("Client-Port", conn_info->request_header)},
        {"SERVER_ADDR", conn_info->server_ip},
        {"SERVER_PORT", port},
        {"SERVER_NAME", conn_info->running_server->server_name},
        {"HTTPS", ""},
        {"REDIRECT_STATUS", "200"},
        {NULL,NULL} 
    };


    char *fastcgi_pass = conn_info->running_server->fastcgi_pass;
    char fhost[20];
    char fport[10];
    sscanf(fastcgi_pass, "%[^:]:%s", fhost, fport);

    int fcgi_fd = prepare_request(conn_info, fhost, atoi(fport));
    if(fcgi_fd == 0) {
        conn_info->status_code =  502;
        apply_next_router(conn_info);
        return;
    }

    //start build request
    begin_build_request(request_id, &smem);

    Param_Value *tmp = pv;
    while(tmp->name != NULL){
        add_fcgi_param(request_id, tmp->name, tmp->value, &smem);
        tmp++;
    }

    Http_Param *params = conn_info->request_header->params;
    int count = 0;
    char *new_header;
    for(count = 0; count < conn_info->request_header->used_len; ++count) {
        new_header = header_to_fcgi(params->name);
        add_fcgi_param(request_id, new_header, params->value, &smem);
        uws_free(new_header);
        params++;
    }
    uws_free(port);


    //add more http headers

    //terminate params
    FCGI_Header end_params;
    end_params = make_header(FCGI_PARAMS, request_id, 0, 0);
    append_mem_t(&smem, &end_params, FCGI_HEADER_LEN);
    send_request(fcgi_fd, &smem);
    free_mem_t(&smem);

    if(strcmp(conn_info->request_header->method, "POST") == 0 && get_header_param("Content-Length", conn_info->request_header) != NULL) {
        char line[MAX_BODY_LEN];
        FCGI_Header content_header;

        for(; ;) {
            if(feof(conn_info->input_file) || ferror(conn_info->input_file)) break;
            size_t read_num = fread(line, sizeof(char), MAX_BODY_LEN, conn_info->input_file);
            if(read_num == 0) break;
            content_header = make_header(FCGI_STDIN, request_id, read_num, 0);
            append_mem_t(&smem, &content_header, FCGI_HEADER_LEN);
            append_mem_t(&smem, line, read_num);
            send_request(fcgi_fd, &smem);
            free_mem_t(&smem);
        }
        FCGI_Header end_body;
        end_body = make_header(FCGI_STDIN, request_id, 0, 0);
        append_mem_t(&smem, &end_body, FCGI_HEADER_LEN);
    }
    //send finish request symbol
    FCGI_Header end_header;
    end_header = make_header(FCGI_PARAMS, request_id, 0, 0);
    append_mem_t(&smem, &end_header, FCGI_HEADER_LEN);

    send_request(fcgi_fd, &smem);

    memory_t mem_file;
    // if we have more content from fastcgi
    bool more_content = read_response(fcgi_fd, &mem_file);

    if(mem_file.len == 0) {
        conn_info->status_code =  500;
        apply_next_router(conn_info);
        return;
    }

    char line[LINE_LEN] = {0};
    unsigned char *oldpos = mem_file.mem;
    unsigned char *pos;
    struct http_header fcgi_response_header;
    bzero(&fcgi_response_header, sizeof(fcgi_response_header));
    char key[LINE_LEN];
    char value[LINE_LEN];
    
    char *time_string = get_time_string(NULL);
    add_header_param("Server", UWS_SERVER, &fcgi_response_header);
    add_header_param("Date", time_string, &fcgi_response_header);
    uws_free(time_string);

    while((pos = strstr(oldpos, "\r\n"))) {
        if(oldpos == pos) break;
        bzero(line, LINE_LEN);
        strncpy(line, oldpos, pos - oldpos);
        sscanf(line, "%[^:]: %s", key, value);
        if(strcmp(key, "Status") == 0) {
            fcgi_response_header.status_code = atoi(value);
        } else {
            push_header_param(key, value, &fcgi_response_header);
        }
        oldpos = pos + strlen("\r\n");
    }
    int content_len = mem_file.len - (pos - mem_file.mem) - strlen("\r\n");

    char *str_len =  itoa(content_len);
    add_header_param("Content-Length", str_len, &fcgi_response_header);
    uws_free(str_len);

    fcgi_response_header.http_ver = "HTTP/1.1";
    if(fcgi_response_header.status_code == 0) fcgi_response_header.status_code = 200;
    fcgi_response_header.status = get_by_code(fcgi_response_header.status_code);

    char *header_str = str_response_header(&fcgi_response_header);
    writen(sockfd, header_str, strlen(header_str));
    uws_free(header_str);
    writen(sockfd, pos, content_len + strlen("\r\n"));
    
    while(more_content) {
        free_mem_t(&mem_file);
        more_content = read_response(fcgi_fd, &mem_file);
        writen(sockfd, mem_file.mem, mem_file.len);
    }

    free_header_params(&fcgi_response_header);
    free_mem_t(&smem);
    free_mem_t(&mem_file);

    apply_next_router(conn_info);
}

