//web2

#include "achieve.h"
#include "interface.h"
#include <signal.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define THREAD_POOL_SIZE 4
#define MAX_QUEUE_SIZE 128
#define MAX_EVENTS 1024

int server_fd_global = -1;
volatile sig_atomic_t g_shutdown_requested = 0;

// ?????????????
typedef struct task_node {
    struct message_of_client* client;
    struct task_node* next;
} task_node;

// ???????
typedef struct thread_pool {
    task_node* queue_head;
    task_node* queue_tail;
    pthread_t threads[THREAD_POOL_SIZE];
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int queue_size;
    int shutdown;
} thread_pool;

thread_pool* g_thread_pool = NULL;

// ??????????
static void* worker_thread(void* arg) {
    thread_pool* pool = (thread_pool*)arg;

    while (1) {
        pthread_mutex_lock(&pool->mutex);

        while (pool->queue_size == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->cond, &pool->mutex);
        }

        if (pool->shutdown && pool->queue_size == 0) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        task_node* task = pool->queue_head;
        if (task) {
            pool->queue_head = task->next;
            if (pool->queue_tail == task) {
                pool->queue_tail = NULL;
            }
            pool->queue_size--;
        }

        pthread_mutex_unlock(&pool->mutex);

        if (task) {
            execute_connect(task->client);
            free(task);
        }
    }

    return NULL;
}

// ?????????
thread_pool* init_thread_pool() {
    thread_pool* pool = (thread_pool*)malloc(sizeof(thread_pool));
    if (!pool) {
        perror("malloc thread pool");
        return NULL;
    }
    
    // ?????????
    pool->queue_head = NULL;
    pool->queue_tail = NULL;
    pool->queue_size = 0;
    pool->shutdown = 0;
    
    // ?????????????????
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond, NULL);
    
    // ???????????
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            perror("pthread_create");
            for (int j = 0; j < i; j++) {
                pthread_cancel(pool->threads[j]);
            }
            free(pool);
            return NULL;
        }
    }
    
    return pool;
}

// ??????????????
int add_task(thread_pool* pool, struct message_of_client* client) {
    if (!pool || !client) {
        return -1;
    }
    
    // ??????????
    task_node* task = (task_node*)malloc(sizeof(task_node));
    if (!task) {
        perror("malloc task node");
        return -1;
    }
    task->client = client;
    task->next = NULL;
    
    pthread_mutex_lock(&pool->mutex);

    if (pool->queue_size >= MAX_QUEUE_SIZE) {
        pthread_mutex_unlock(&pool->mutex);
        free(task);
        return -1;
    }

    if (pool->queue_tail) {
        pool->queue_tail->next = task;
    } else {
        pool->queue_head = task;
    }
    pool->queue_tail = task;
    pool->queue_size++;
    
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

// ????????
void destroy_thread_pool(thread_pool* pool) {
    if (!pool) {
        return;
    }
    
    pthread_mutex_lock(&pool->mutex);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);
    
    // ?????????????
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    // ????????
    task_node* task = pool->queue_head;
    while (task) {
        task_node* next = task->next;
        free(task->client);
        free(task);
        task = next;
    }
    
    // ????????????????
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    
    free(pool);
}

static void signal_handler(int sig) {
    if (sig == SIGINT) {
        g_shutdown_requested = 1;
    }
}

int main(){
    int server_fd,client_fd;
    struct sockaddr_in client_addr;
    socklen_t addr_size = sizeof(client_addr);

    // ??????????
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    // ?????????????
    if(fs_mount("./disk.img") != 0) {
        printf("??????????????????????????\n");
        return 1;
    }
    printf("???????????\n");

    // ?????????
    g_thread_pool = init_thread_pool();
    if (!g_thread_pool) {
        printf("?????????????????????????\n");
        fs_unmount();
        return 1;
    }
    printf("??????????????????? %d ?????????\n", THREAD_POOL_SIZE);

    server_fd = socket_new_serverfd();
    server_fd_global = server_fd;
    
    // ??????Reactor??epoll???
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        destroy_thread_pool(g_thread_pool);
        fs_unmount();
        return 1;
    }
    
    // ????????socket?????epoll
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) < 0) {
        perror("epoll_ctl add server fd");
        close(epoll_fd);
        destroy_thread_pool(g_thread_pool);
        fs_unmount();
        return 1;
    }
    
    struct epoll_event events[MAX_EVENTS];
    
    while(!g_shutdown_requested){
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 500);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            continue;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_fd) {
                client_fd = accept(server_fd,(struct sockaddr*)&client_addr,&addr_size);
                if(client_fd < 0){
                    if (errno == EINTR || errno == EAGAIN) continue;
                    perror("accept");
                    continue;
                }

                struct message_of_client* client = (struct message_of_client*)malloc(sizeof(struct message_of_client));
                if(client == NULL){
                    perror("malloc");
                    close(client_fd);
                    continue;
                }
                client->client_addr = client_addr;
                client->client_fd = client_fd;

                if (add_task(g_thread_pool, client) != 0) {
                    perror("add_task");
                    free(client);
                    close(client_fd);
                    continue;
                }
                printf("新客户端连接已加入任务队列\n");
            }
        }
    }

    printf("\n收到 SIGINT，正在优雅退出...\n");
    close(server_fd);
    server_fd_global = -1;
    destroy_thread_pool(g_thread_pool);
    g_thread_pool = NULL;
    close(epoll_fd);
    fs_unmount();
    printf("已清理完毕，退出。\n");
    return 0;
}