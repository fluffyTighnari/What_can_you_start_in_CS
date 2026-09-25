#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // ???HTTP????
    printf("Content-Type: text/plain; charset=utf-8\r\n");
    printf("\r\n");
    
    // ???CGI???????
    printf("CGI Test Program\n");
    printf("================\n");
    
    // ???????????
    printf("Environment Variables:\n");
    
    const char* cookie = getenv("HTTP_COOKIE");
    if (cookie) {
        printf("HTTP_COOKIE: %s\n", cookie);
    } else {
        printf("HTTP_COOKIE: (not set)\n");
    }
    
    const char* method = getenv("REQUEST_METHOD");
    if (method) {
        printf("REQUEST_METHOD: %s\n", method);
    } else {
        printf("REQUEST_METHOD: (not set)\n");
    }
    
    const char* path = getenv("PATH_INFO");
    if (path) {
        printf("PATH_INFO: %s\n", path);
    } else {
        printf("PATH_INFO: (not set)\n");
    }
    
    return 0;
}