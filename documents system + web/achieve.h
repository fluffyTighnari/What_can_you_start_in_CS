//achieve.h
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stddef.h>
#define PORT 8080
#define buffer_size 4096
#define root "./www"
#define backlog 128

struct message_of_client{
int client_fd;
struct sockaddr_in client_addr;
};

int socket_new_serverfd();

void* execute_connect(void* arguments);

char* get_username_from_cookie(const char* cookie);
void handle_login(int client_fd, const char* body);
void check_login_status(int client_fd, const char* cookie);
void handle_logout(int client_fd, const char* cookie);
void handle_logout(int client_fd, const char* cookie);</parameter=replace_all=false

void deal_with_request(char* buffer,char* method,char* path, char* body);

void send_error(int client_fd,int eerrno,const char* status);

void send_file_response(int client_fd,char* file_path);

void send_response(int client_fd, int status, const char* message);
