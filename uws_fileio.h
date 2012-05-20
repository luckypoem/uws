#ifndef __FILEIO_H_
#define __FILEIO_H_
#define PATH_LEN    512
#define BUFF_LEN    4096

int comparestr(const void *p1, const void *p2);
void printdir(const char *fpath);
void printfile(const char *path);
//void pathrouter(const char* arg, FILE *stream);
//static char* get_mime(const char* path);

#endif