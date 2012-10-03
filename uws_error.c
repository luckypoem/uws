#include "uws_error.h"
#include "uws_utils.h"
#include "uws_header.h"
#include "uws_config.h"
void send_error_response(int client_fd, const int status_code, const bool with_page) {
    char** error_pages = running_server->error_page;
    int i;
    char *error_path = NULL;
    char *error_file_path;
    while(*error_pages != NULL) {
        for(i = 0; i < strlen(*error_pages); i++) {
            if((*error_pages)[i] == '=') {
                (*error_pages)[i] = '\0';
                if(atoi(*error_pages) == status_code) {
                    error_path = strdup(*error_pages + i + 1);
                    (*error_pages)[i] = '=';
                    break;
                }
                (*error_pages)[i] = '=';
            }
        }
        if(error_path != NULL) break;
        error_pages++;
    }
    int content_len;
    char *content;
    if(with_page) {
        error_file_path  = strdup("/dev/null"); //just for test
        if(error_path != NULL) {
            char *tmp_path = strlcat(running_server->root, error_path);
            if(access(tmp_path, F_OK) == 0) {
                uws_free(error_file_path);
                error_file_path = tmp_path;
                uws_free(error_path);
            } else {
                uws_free(tmp_path);
            }
        }
        FILE* file = fopen(error_file_path, "r");
        fseek(file, 0, SEEK_END);
        content_len = ftell(file);
        rewind(file);
        content = (char*) uws_calloc (content_len, sizeof(char));

        fread(content, sizeof(char), content_len, file);
        fclose(file);
    } else {
        content_len = 0;
        content = strdup("");
    }

    //go here
    char *time_string = get_time_string(NULL);

    response_header->http_ver = "HTTP/1.1";
    response_header->status_code = status_code;
    response_header->status = get_by_code(status_code);
    add_header_param("Cache-Control", "private", response_header);
    add_header_param("Connection", "Keep-Alive", response_header);
    add_header_param("Server", UWS_SERVER, response_header);
    add_header_param("Date", time_string, response_header);
    add_header_param("Content-Type", "text/html", response_header);


    if(with_page) {
        char *content_len_str = itoa(content_len);
        add_header_param("Content-Length", content_len_str, response_header);
        uws_free(content_len_str);
    }


    struct response header_body;

    header_body.header = response_header;
    header_body.content = content;
    header_body.content_len = content_len;

    uws_free(time_string);
    //
    write_response(client_fd, &header_body);
    free_header_params(header_body.header);
    uws_free(header_body.header);
    uws_free(header_body.content);
    longjmp(error_jmp_buf, 1);
}
char *get_by_code(int code) {
    int i = 0;
    while(http_status[i].code != -1) {
        if(http_status[i].code == code) return http_status[i].message;
        i++;
    }
    return http_status[i].message;
}
