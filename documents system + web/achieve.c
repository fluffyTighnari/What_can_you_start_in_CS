//achieve.c
#ifndef ACHIEVE_C
#define ACHIEVE_C
#include "achieve.h"
#include "interface.h"
#include <ctype.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <netinet/tcp.h>

#define CGI_DIR "exec"
#define MAX_SESSIONS 64
#define SESSION_TOKEN_LEN 32
#endif

typedef struct {
    char token[SESSION_TOKEN_LEN + 1];
    char username[name_length];
    int in_use;
    time_t expires;
} Session;

static Session sessions[MAX_SESSIONS];
static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;

static void generate_token(char* buf, size_t len) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        unsigned char rand_bytes[256];
        ssize_t n = read(fd, rand_bytes, sizeof(rand_bytes));
        close(fd);
        if (n > 0) {
            for (size_t i = 0; i < len; i++) {
                buf[i] = charset[rand_bytes[i % (size_t)n] % (sizeof(charset) - 1)];
            }
            buf[len] = '\0';
            return;
        }
    }
    for (size_t i = 0; i < len; i++) {
        buf[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    buf[len] = '\0';
}

static const char* session_create(const char* username) {
    pthread_mutex_lock(&session_mutex);
    time_t now = time(NULL);
    int slot = -1;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!sessions[i].in_use) {
            slot = i;
            break;
        }
        if (sessions[i].expires < now) {
            sessions[i].in_use = 0;
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        pthread_mutex_unlock(&session_mutex);
        return NULL;
    }
    generate_token(sessions[slot].token, SESSION_TOKEN_LEN);
    strncpy(sessions[slot].username, username, name_length - 1);
    sessions[slot].username[name_length - 1] = '\0';
    sessions[slot].expires = now + 24 * 3600;
    sessions[slot].in_use = 1;
    const char* token = sessions[slot].token;
    pthread_mutex_unlock(&session_mutex);
    return token;
}

static int session_lookup(const char* token, char* username_buf, size_t buf_size) {
    if (!token || !username_buf || buf_size == 0) return -1;
    pthread_mutex_lock(&session_mutex);
    time_t now = time(NULL);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].in_use && strcmp(sessions[i].token, token) == 0) {
            if (sessions[i].expires < now) {
                sessions[i].in_use = 0;
                pthread_mutex_unlock(&session_mutex);
                return -1;
            }
            strncpy(username_buf, sessions[i].username, buf_size - 1);
            username_buf[buf_size - 1] = '\0';
            pthread_mutex_unlock(&session_mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&session_mutex);
    return -1;
}

static void session_destroy(const char* token) {
    if (!token) return;
    pthread_mutex_lock(&session_mutex);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].in_use && strcmp(sessions[i].token, token) == 0) {
            sessions[i].in_use = 0;
            break;
        }
    }
    pthread_mutex_unlock(&session_mutex);
}

static size_t url_decode(char* s) {
    char* p = s;
    char* q = s;
    while (*p) {
        if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
            int c = 0;
            sscanf(p + 1, "%2x", &c);
            *q++ = (char)c;
            p += 3;
        } else if (*p == '+') {
            *q++ = ' ';
            p++;
        } else {
            *q++ = *p++;
        }
    }
    *q = '\0';
    return (size_t)(q - s);
}


static char* get_session_token_from_cookie(const char* cookie) {
    if (!cookie) return NULL;
    char* copy = strdup(cookie);
    if (!copy) return NULL;
    char* session_token = NULL;
    char* tok = strtok(copy, "; ");
    while (tok) {
        if (strncmp(tok, "session=", 8) == 0) {
            session_token = strdup(tok + 8);
            break;
        }
        tok = strtok(NULL, "; ");
    }
    free(copy);
    return session_token;
}

char* get_username_from_cookie(const char* cookie) {
    char* token = get_session_token_from_cookie(cookie);
    if (!token) return NULL;
    char username[name_length];
    if (session_lookup(token, username, sizeof(username)) != 0) {
        free(token);
        return NULL;
    }
    free(token);
    return strdup(username);
}

static int path_contains_traversal(const char* path) {
    const char* p = path;
    while (*p) {
        if (p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == '\\' || p[2] == '\0')) {
            return 1;
        }
        p++;
    }
    return 0;
}

void handle_cgi_request(int client_fd, const char* buffer, const char* method, const char* path, const char* body, const char* cookie) {
    const char* cgi_rel = path + 4;
    if (path_contains_traversal(cgi_rel) || *cgi_rel == '/' || *cgi_rel == '\\') {
        send_error(client_fd, 400, "Bad Request");
        return;
    }

    char cgi_path[512];
    int n = snprintf(cgi_path, sizeof(cgi_path), "%s/%s", CGI_DIR, cgi_rel);
    if (n < 0 || (size_t)n >= sizeof(cgi_path)) {
        send_error(client_fd, 414, "URI Too Long");
        return;
    }

    char resolved[512];
    if (!realpath(cgi_path, resolved)) {
        send_error(client_fd, 404, "CGI Program Not Found");
        return;
    }

    char cgi_dir_real[512];
    if (!realpath(CGI_DIR, cgi_dir_real)) {
        send_error(client_fd, 500, "Internal Server Error");
        return;
    }
    size_t dir_len = strlen(cgi_dir_real);
    if (strncmp(resolved, cgi_dir_real, dir_len) != 0 ||
        (resolved[dir_len] != '/' && resolved[dir_len] != '\0')) {
        send_error(client_fd, 403, "Forbidden");
        return;
    }

    struct stat st;
    if (stat(resolved, &st) != 0 || !S_ISREG(st.st_mode) || !(st.st_mode & S_IXUSR)) {
        send_error(client_fd, 404, "CGI Program Not Found or Not Executable");
        return;
    }
    
    int out_pipe[2];
    int in_pipe[2] = {-1, -1};
    if (pipe(out_pipe) == -1) {
        send_error(client_fd, 500, "Internal Server Error");
        return;
    }

    int is_post = (strcmp(method, "POST") == 0);
    if (is_post) {
        if (pipe(in_pipe) == -1) {
            close(out_pipe[0]);
            close(out_pipe[1]);
            send_error(client_fd, 500, "Internal Server Error");
            return;
        }
    }

    pid_t pid = fork();
    if (pid == -1) {
        send_error(client_fd, 500, "Internal Server Error");
        close(out_pipe[0]);
        close(out_pipe[1]);
        if (in_pipe[0] >= 0) { close(in_pipe[0]); close(in_pipe[1]); }
        return;
    }

    if (pid == 0) {
        close(out_pipe[0]);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(out_pipe[1]);

        if (is_post) {
            close(in_pipe[1]);
            dup2(in_pipe[0], STDIN_FILENO);
            close(in_pipe[0]);
        } else {
            int devnull = open("/dev/null", O_RDONLY);
            if (devnull >= 0) {
                dup2(devnull, STDIN_FILENO);
                close(devnull);
            }
        }

        long max_fd = sysconf(_SC_OPEN_MAX);
        if (max_fd < 0) max_fd = 1024;
        for (long fd = 3; fd < max_fd; fd++) {
            close((int)fd);
        }

        if (cookie) {
            setenv("HTTP_COOKIE", cookie, 1);
        }
        setenv("REQUEST_METHOD", method, 1);
        setenv("PATH_INFO", path, 1);
        setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
        setenv("SERVER_PROTOCOL", "HTTP/1.1", 1);
        if (is_post && body) {
            char cl[32];
            snprintf(cl, sizeof(cl), "%zu", strlen(body));
            setenv("CONTENT_LENGTH", cl, 1);
        }

        execl(resolved, resolved, NULL);

        _exit(1);
    } else {
        close(out_pipe[1]);
        if (is_post) {
            close(in_pipe[0]);
            if (body && strlen(body) > 0) {
                ssize_t w = write(in_pipe[1], body, strlen(body));
                (void)w;
            }
            close(in_pipe[1]);
        }

        char cgi_output[4096];
        ssize_t n;
        while ((n = read(out_pipe[0], cgi_output, sizeof(cgi_output))) > 0) {
            send(client_fd, cgi_output, n, 0);
        }

        close(out_pipe[0]);
        int status = 0;
        waitpid(pid, &status, 0);
        (void)status;
    }
}


static int is_valid_username(const char* name) {
    if (!name || name[0] == '\0' || strlen(name) >= name_length) return 0;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;
    for (const char* p = name; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c <= 0x1F || c == 0x7F) return 0;
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') return 0;
    }
    return 1;
}

void handle_login(int client_fd, const char* body) {
    char name[256] = {0};
    if (!body || sscanf(body, "name=%255[^&]", name) < 1 || strlen(name) == 0) {
        send_error(client_fd, 400, "Bad Request");
        return;
    }
    url_decode(name);

    if (!is_valid_username(name)) {
        send_error(client_fd, 400, "Invalid username");
        return;
    }

    fs_mkdir(name, NULL);

    const char* token = session_create(name);
    if (!token) {
        send_error(client_fd, 503, "Service Unavailable");
        return;
    }

    char expire[64];
    time_t t = time(NULL) + 24 * 3600;
    struct tm tm_buf;
    gmtime_r(&t, &tm_buf);
    strftime(expire, sizeof(expire), "%a, %d %b %Y %H:%M:%S GMT", &tm_buf);

    char response[buffer_size];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: application/json\r\n"
             "Set-Cookie: session=%s; Path=/; Expires=%s; HttpOnly; SameSite=Lax\r\n"
             "Connection: close\r\n"
             "\r\n"
             "{\"success\":true,\"message\":\"Login successful\"}",
             token, expire);
    send(client_fd, response, strlen(response), 0);
}


void handle_logout(int client_fd, const char* cookie) {
    char* token = get_session_token_from_cookie(cookie);
    if (token) {
        session_destroy(token);
        free(token);
    }
    char response[buffer_size];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: application/json\r\n"
             "Set-Cookie: session=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n"
             "Connection: close\r\n"
             "\r\n"
             "{\"success\":true,\"message\":\"Logout successful\"}");
    send(client_fd, response, strlen(response), 0);
}

static const char* get_content_type(const char* filename) {
    const char* ext = filename ? strrchr(filename, '.') : NULL;
    if (!ext || !ext[1]) return "application/octet-stream";
    ext++;
    if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) return "text/html; charset=utf-8";
    if (strcasecmp(ext, "css") == 0) return "text/css";
    if (strcasecmp(ext, "js") == 0) return "application/javascript";
    if (strcasecmp(ext, "json") == 0) return "application/json";
    if (strcasecmp(ext, "txt") == 0) return "text/plain; charset=utf-8";
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcasecmp(ext, "png") == 0) return "image/png";
    if (strcasecmp(ext, "gif") == 0) return "image/gif";
    if (strcasecmp(ext, "webp") == 0) return "image/webp";
    if (strcasecmp(ext, "svg") == 0) return "image/svg+xml";
    if (strcasecmp(ext, "ico") == 0) return "image/x-icon";
    if (strcasecmp(ext, "pdf") == 0) return "application/pdf";
    if (strcasecmp(ext, "zip") == 0) return "application/zip";
    if (strcasecmp(ext, "mp3") == 0) return "audio/mpeg";
    if (strcasecmp(ext, "mp4") == 0) return "video/mp4";
    if (strcasecmp(ext, "xml") == 0) return "application/xml";
    return "application/octet-stream";
}


static void send_file_content(int client_fd, int status, const char* filename, const void* data, size_t len) {
    char header[buffer_size];
    const char* ct = get_content_type(filename);
    int n = snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "Cache-Control: max-age=3600\r\n"
             "\r\n",
             status, status == 200 ? "OK" : "Created", ct, len);
    if (n <= 0 || (size_t)n >= sizeof(header)) return;
    write(client_fd, header, (size_t)n);
    if (len > 0 && data) write(client_fd, data, len);
}


void check_login_status(int client_fd, const char* cookie) {
    char* username = get_username_from_cookie(cookie);
    char response[buffer_size];
    if (username) {
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Connection: close\r\n"
                 "\r\n"
                 "{\"loggedIn\":true,\"user\":\"%s\"}",
                 username);
        free(username);
    } else {
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Connection: close\r\n"
                 "\r\n"
                 "{\"loggedIn\":false}");
    }
    send(client_fd, response, strlen(response), 0);
}

int socket_new_serverfd(){
    int server_fd = socket(AF_INET,SOCK_STREAM,0);
    if(server_fd < 0){
        perror("socket");
    }

    int opt = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        perror("设置 socket 选项失败");
    }

    // 设置 TCP_NODELAY 选项，禁用 Nagle 算法以降低延迟
    if(setsockopt(server_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0){
        perror("设置 TCP_NODELAY 选项失败");
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if(bind(server_fd,(struct sockaddr *)&server_addr,sizeof(server_addr)) < 0){
        perror("bind");
    }

    if(listen(server_fd,backlog) < 0){
        perror("listen");
    }

    printf("服务器启动成功，监听端口:%d\n等待客户端连接...", PORT);
    return server_fd;
}


void* execute_connect(void* arguments){
    struct message_of_client* client = (struct message_of_client*)arguments;
    int client_fd = client->client_fd;
    struct sockaddr_in client_addr = client->client_addr;

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client->client_addr.sin_addr, ip_str, sizeof(ip_str));
    printf("线程 %lu: 新的客户端连接 from %s:%d\n",
           (unsigned long)pthread_self(),
           ip_str,
           ntohs(client->client_addr.sin_port));
    
    // 为客户端连接设置 SO_KEEPALIVE 选项，检测死连接
    int opt = 1;
    if(setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0){
        perror("为客户端连接设置 SO_KEEPALIVE 选项失败");
    }
    
    // 为客户端连接设置 TCP_NODELAY 选项，禁用 Nagle 算法以降低延迟
    if(setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0){
        perror("为客户端连接设置 TCP_NODELAY 选项失败");
    }

    // 文件系统在main函数中已单次挂载，此处无需挂载

    char buffer[buffer_size];
    ssize_t bytes_received;

    // ������������
    memset(buffer, 0, sizeof(buffer));

    bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
    if(bytes_received <= 0){
        close(client_fd);
        free(client);
        return NULL;
    }
    buffer[bytes_received] = '\0';

    char method[10];
    char path[256];
    char body[buffer_size] = {0};

    char* body_start = strstr(buffer, "\r\n\r\n");
    size_t body_in_buf = 0;
    long content_length = -1;
    if(body_start) {
        body_start += 4;
        body_in_buf = (size_t)((buffer + bytes_received) - body_start);
        if (body_in_buf > sizeof(body)) body_in_buf = sizeof(body);
        strncpy(body, body_start, sizeof(body) - 1);
        body[sizeof(body)-1] = '\0';
    }
    char* cl = strstr(buffer, "Content-Length:");
    if (cl) sscanf(cl, "Content-Length: %ld", &content_length);

    char* cookie_start = strstr(buffer, "Cookie:");
    const char* cookie = NULL;
    if(cookie_start) {
        cookie_start += 7;
        while(*cookie_start == ' ') cookie_start++;
        char* end = strstr(cookie_start, "\r\n");
        if(end) *end = '\0';
        cookie = cookie_start;
    }

    method[0] = '\0';
    path[0] = '\0';
    deal_with_request(buffer, method, path, body);
    if (method[0] == '\0' || path[0] == '\0') {
        send_error(client_fd, 400, "Bad Request");
        close(client_fd);
        free(client);
        return NULL;
    }
    { char* q = strchr(path, '?'); if (q) *q = '\0'; }

    /* ��¼�����¼����Ҫ���ѹ��أ��Ҳ����ļ�·������ */
    if(strcmp(method, "POST") == 0 && strcmp(path, "/login") == 0) {
        handle_login(client_fd, body);
    } else if(strcmp(method, "GET") == 0 && strcmp(path, "/checklogin") == 0) {
        check_login_status(client_fd, cookie);
    } else if(strcmp(method, "GET") == 0 && strcmp(path, "/logout") == 0) {
        handle_logout(client_fd, cookie);
    } else if (strstr(path, "/cgi/") == path) {
        /* ���� CGI ���� */
        handle_cgi_request(client_fd, buffer, method, path, body, cookie);
    } else if(strcmp(method,"GET") == 0 && strcmp(path,"/") == 0){
        //��һ���������html�ļ�
        char file_path[512];
        snprintf(file_path,sizeof(file_path),"%s/index.html",root);
        send_file_response(client_fd,file_path);
    } else {
        /* ����Ϊ���¼�Ĳ������� cookie �е��û�����ΪĿ¼ */
        char* username = get_username_from_cookie(cookie);
        if(!username || strlen(username) == 0) {
            send_error(client_fd, 401, "Unauthorized");
            if(username) free(username);
        } else {
            char file_path[512];
            int error_sent = 0; /* �����Ƿ��ѷ��ʹ�����Ӧ���������Ժ� break �Ա�ر����� */

            if(strcmp(method,"GET") == 0){
                if(strcmp(path,"/list") == 0) {
                    char list_buffer[buffer_size];
                    int len = fs_list_director_to_buf(username, list_buffer, sizeof(list_buffer));
                    if (len < 0) len = 0;
                    list_buffer[len] = '\0';
                    send_response(client_fd, 200, list_buffer);
                } else if(strcmp(path,"/members") == 0) {
                    char list_buffer[buffer_size];
                    int len = fs_list_member_to_buf(list_buffer, sizeof(list_buffer));
                    if (len < 0) len = 0;
                    list_buffer[len] = '\0';
                    send_response(client_fd, 200, list_buffer);
                } else {
                    char* file_name = path + 1;
                    if(strlen(file_name) > 0) {
                        char file_content[buffer_size];
                        int result = fs_read_file(file_name, username, file_content, sizeof(file_content), 0);
                        if(result >= 0) {
                            send_file_content(client_fd, 200, file_name, file_content, (size_t)result);
                        } else {
                            send_error(client_fd, 404, "File Not Found");
                            error_sent = 1;
                        }
                    } else {
                        send_error(client_fd, 400, "Bad Request");
                        error_sent = 1;
                    }
                }
            } else if(strcmp(method,"POST") == 0) {
                char* file_name = path + 1;
                if(strlen(file_name) > 0) {
                    int result = fs_create_file(file_name, username);
                    if(result == 0) {
                        send_response(client_fd, 201, "File created successfully");
                    } else {
                        send_error(client_fd, 409, "File Already Exists");
                        error_sent = 1;
                    }
                } else {
                    send_error(client_fd, 400, "Bad Request");
                    error_sent = 1;
                }
            } else if(strcmp(method,"PUT") == 0) {
                char* file_name = path + 1;
                if(strlen(file_name) == 0) {
                    send_error(client_fd, 400, "Bad Request");
                    error_sent = 1;
                } else {
                    size_t put_len;
                    const void* put_body;
                    if (content_length >= 0) {
                        put_len = (size_t)content_length;
                        put_body = body_start;
                        if (put_len == 0) {
                            fs_create_file(file_name, username);
                            send_response(client_fd, 200, "File updated successfully");
                        } else if ((unsigned long)content_length > 4 * buffer_size) {
                            send_error(client_fd, 413, "Payload Too Large");
                            error_sent = 1;
                        } else {
                            if (put_len > body_in_buf) {
                                fs_create_file(file_name, username);
                                int wr = fs_write_file(file_name, username, put_body, body_in_buf, 0);
                                if (wr < 0) {
                                    send_error(client_fd, 500, "Write failed");
                                    error_sent = 1;
                                } else {
                                    size_t offset = body_in_buf;
                                    char chunk[buffer_size];
                                    while (offset < put_len) {
                                        size_t to_read = put_len - offset;
                                        if (to_read > sizeof(chunk)) to_read = sizeof(chunk);
                                        ssize_t n = recv(client_fd, chunk, to_read, 0);
                                        if (n <= 0) break;
                                        wr = fs_write_file(file_name, username, chunk, (size_t)n, offset);
                                        if (wr < 0) break;
                                        offset += (size_t)n;
                                    }
                                    if (offset == put_len)
                                        send_response(client_fd, 200, "File updated successfully");
                                    else {
                                        send_error(client_fd, 500, "Upload incomplete");
                                        error_sent = 1;
                                    }
                                }
                            } else {
                                fs_create_file(file_name, username);
                                int result = fs_write_file(file_name, username, put_body, put_len, 0);
                                if (result >= 0)
                                    send_response(client_fd, 200, "File updated successfully");
                                else {
                                    send_error(client_fd, 500, "Write failed");
                                    error_sent = 1;
                                }
                            }
                        }
                    } else {
                        put_len = strlen(body);
                        put_body = body;
                        if (put_len > 0) {
                            fs_create_file(file_name, username);
                            int result = fs_write_file(file_name, username, put_body, put_len, 0);
                            if (result >= 0)
                                send_response(client_fd, 200, "File updated successfully");
                            else {
                                send_error(client_fd, 500, "Write failed");
                                error_sent = 1;
                            }
                        } else {
                            send_error(client_fd, 400, "Bad Request");
                            error_sent = 1;
                        }
                    }
                }
            } else if(strcmp(method,"DELETE") == 0) {
                char* file_name = path + 1;
                if(strlen(file_name) > 0) {
                    int result = fs_delete_file(file_name, username);
                    if(result == 0) {
                        send_response(client_fd, 200, "File deleted successfully");
                    } else {
                        send_error(client_fd, 404, "File Not Found");
                        error_sent = 1;
                    }
                } else {
                    send_error(client_fd, 400, "Bad Request");
                    error_sent = 1;
                }
            } else {
                send_error(client_fd, 501, "Not Implemented");
                error_sent = 1;
            }

            free(username);
        }
    }

    close(client_fd);
    free(client);

    return NULL;
}

void deal_with_request(char* request,char* method,char* path, char* body){
    char* host_header = strstr(request, "Host:");
    if(host_header) {
        char host[256];
        sscanf(host_header, "Host: %s", host);
        printf("Host: %s\n", host);
    }

    char* end_of_line = strstr(request,"\r\n");
    if(end_of_line != NULL){
        *end_of_line = '\0';
    }

    sscanf(request,"%9s %255s",method,path);
    printf("请求方法：%s; 请求路径：%s\n", method, path);
    if(body && strlen(body) > 0) {
        printf("请求体：%s\n", body);
    }
}

void send_error(int client_fd,int eerrno,const char* status){
    char buffer[buffer_size];
    size_t body_len = strlen(status);
    size_t head_len = snprintf(buffer, sizeof(buffer),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: text/plain; charset=utf-8\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             eerrno, status, body_len, status);
    ssize_t n = write(client_fd, buffer, head_len);
    (void)n;
}

void send_file_response(int client_fd,char* file_path){
    struct stat file_stat;

    if (stat(file_path, &file_stat) < 0 || !S_ISREG(file_stat.st_mode)) {
        send_error(client_fd, 404, "Not Found");
        return;
    }

    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        send_error(client_fd, 404, "Not Found");
        return;
    }

    char header[buffer_size] = {0};
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html; charset=utf-8\r\n"
             "Transfer-Encoding: chunked\r\n"
             "Connection: close\r\n"
             "Cache-Control: max-age=86400\r\n"
             "\r\n");
    write(client_fd, header, strlen(header));

    char file_buf[buffer_size] = {0};
    size_t bytes;
    while ((bytes = fread(file_buf, 1, sizeof(file_buf), fp)) > 0) {
        char chunk_header[32];
        snprintf(chunk_header, sizeof(chunk_header), "%zx\r\n", bytes);
        write(client_fd, chunk_header, strlen(chunk_header));
        write(client_fd, file_buf, bytes);
        write(client_fd, "\r\n", 2);
    }
    write(client_fd, "0\r\n\r\n", 5);

    fclose(fp);
}

void send_response(int client_fd, int status, const char* message){
    char header[buffer_size] = {0};
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: text/plain; charset=utf-8\r\n"
             "Transfer-Encoding: chunked\r\n"
             "Connection: close\r\n"
             "\r\n",
             status, status == 200 ? "OK" : (status == 201 ? "Created" : "Internal Server Error"));
    write(client_fd, header, strlen(header));

    if(message && strlen(message) > 0) {
        size_t len = strlen(message);
        char chunk_header[32];
        snprintf(chunk_header, sizeof(chunk_header), "%zx\r\n", len);
        write(client_fd, chunk_header, strlen(chunk_header));
        write(client_fd, message, len);
        write(client_fd, "\r\n", 2);
    }
    write(client_fd, "0\r\n\r\n", 5);
}
