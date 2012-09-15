#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include "uws.h"
#include "uws_router.h"
#include "uws_config.h"
#include "uws_utils.h"
#include "uws_mime.h"
#include "uws_fastcgi.h"
#include "uws_header.h"
#define MAP_LEN 20


static Router
map[MAP_LEN] = {{NULL, NULL}};
//extern router handlers
extern int dir_router(int sockfd);
extern int http_router(int sockfd);
extern int fastcgi_router(int sockfd);
extern int rewrite_router(int sockfd);
//end extern router handler
void add_router(Router router) {
    int i = 0;
    while(map[i].preg != NULL) i++;
    map[i] = router;
}
void init_routers(){
    //--- FILO
    Router httprt;
    httprt.preg = ".*";
    httprt.func = http_router;
    add_router(httprt);
    //---

    Router fastcgirt;
    fastcgirt.preg = "/([^/]+/)*[^/]+\\.php";
    fastcgirt.func = fastcgi_router;
    add_router(fastcgirt);

    Router rewritert;
    rewritert.preg = ".*";
    rewritert.func = rewrite_router;
    add_router(rewritert);

    Router dirrt;
    dirrt.preg = ".*";
    dirrt.func = dir_router;
    add_router(dirrt);
}

void pathrouter(int sockfd) {
    int i = 0;
    while(map[i].preg != NULL) i++; //最先添加的最后执行
    i--;
    for(; i >= 0; i--) {
        if(preg_match(request_header->url, map[i].preg)) {
            if(!map[i].func(sockfd)) return;//返回值为0则停止冒泡
        }
    }
}
