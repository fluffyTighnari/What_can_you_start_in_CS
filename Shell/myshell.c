//Shell.c
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<signal.h>
#include<errno.h>
#include<termios.h>
#include<limits.h>
#include<fcntl.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64
#define MAX_BG_JOBS 64

static int bg_count = 0;
static pid_t bg_pids[MAX_BG_JOBS] = {0};
static struct termios g_old_tio;
static int g_tio_saved = 0;

typedef struct Command{
    char* split_command[MAX_ARGS];
    int size_of_command;
    int pipe;
    int background;
}Command;

void print_prompt(void);
void clean_up_processes(void);
void handle_child(int signal);
void handle_int(int signal);
Command deal_with_input(char* input);
void execute_command(Command cmd);
void execute_pipe_command(Command cmd1, Command cmd2, int background);
char* getinput(void);
void restore_terminal(void);
void safe_exit(int code);
int waitpid_retry(pid_t pid, int *status, int options);

int main(void){
    struct sigaction sa_chld, sa_int, sa_ign;

    /* 注册 SIGCHLD */
    memset(&sa_chld, 0, sizeof(sa_chld));
    sa_chld.sa_handler = handle_child;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa_chld, NULL) == -1) {
        perror("sigaction SIGCHLD");
        exit(1);
    }

    /* 注册 SIGINT —— shell 本身忽略，交由前台子进程处理 */
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_int;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("sigaction SIGINT");
        exit(1);
    }

    /* 忽略 SIGTSTP / SIGQUIT */
    memset(&sa_ign, 0, sizeof(sa_ign));
    sa_ign.sa_handler = SIG_IGN;
    sigemptyset(&sa_ign.sa_mask);
    sa_ign.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_ign, NULL);
    sigaction(SIGQUIT, &sa_ign, NULL);

    atexit(restore_terminal);

    Command cmd;
    char* input = NULL;

    while(1){
        clean_up_processes();
        print_prompt();

        input = getinput();
        if(input == NULL || strlen(input) == 0){
            free(input);
            continue;
        }

        cmd = deal_with_input(input);

        if(cmd.size_of_command > 0 && strcmp(cmd.split_command[0], "exit") == 0){
            if (bg_count > 0) {
                printf("There are %d background job(s) running.\n", bg_count);
                /* 第二次 exit 才真正退出；此处简单处理：发送告警并等待确认 */
                for (int i = 0; i < bg_count; i++) {
                    if (bg_pids[i] > 0) {
                        kill(bg_pids[i], SIGHUP);
                    }
                }
            }
            free(input);
            safe_exit(0);
        }

        if(cmd.size_of_command > 0 && strcmp(cmd.split_command[0], "cd") == 0){
            const char *target;
            if(cmd.size_of_command < 2){
                target = getenv("HOME");
                if (target == NULL) {
                    fprintf(stderr, "cd: HOME not set\n");
                    free(input);
                    continue;
                }
            } else if (strcmp(cmd.split_command[1], "-") == 0) {
                target = getenv("OLDPWD");
                if (target == NULL) {
                    fprintf(stderr, "cd: OLDPWD not set\n");
                    free(input);
                    continue;
                }
                printf("%s\n", target);
            } else {
                target = cmd.split_command[1];
            }

            char cwd_before[PATH_MAX];
            int have_old = (getcwd(cwd_before, sizeof(cwd_before)) != NULL);

            if(chdir(target) != 0){
                perror("cd");
            } else if (have_old) {
                setenv("OLDPWD", cwd_before, 1);
            }

            free(input);
            continue;
        }

        if(cmd.pipe == 0){
            execute_command(cmd);
        } else {
            int index_pipe = -1;
            for(int i = 0; i < cmd.size_of_command; i++){
                if(strcmp(cmd.split_command[i], "|") == 0){
                    index_pipe = i;
                    break;
                }
            }
            if(index_pipe <= 0 || index_pipe >= cmd.size_of_command - 1){
                fprintf(stderr, "管道语法错误\n");
                free(input);
                continue;
            }

            Command cmd1;
            cmd1.size_of_command = index_pipe;
            cmd1.pipe = 0;
            cmd1.background = 0;
            int index = 0;
            for(int i = 0; i < cmd1.size_of_command; i++){
                cmd1.split_command[i] = cmd.split_command[index++];
            }
            cmd1.split_command[cmd1.size_of_command] = NULL;

            Command cmd2;
            cmd2.size_of_command = cmd.size_of_command - index_pipe - 1;
            cmd2.pipe = 0;
            cmd2.background = 0;
            index++;
            for(int i = 0; i < cmd2.size_of_command; i++){
                cmd2.split_command[i] = cmd.split_command[index++];
            }
            cmd2.split_command[cmd2.size_of_command] = NULL;

            execute_pipe_command(cmd1, cmd2, cmd.background);
        }
        free(input);
    }
    return 0;
}

void handle_child(int signal){
    (void)signal;
    int saved_errno = errno;
    int status;
    while(waitpid(-1, &status, WNOHANG) > 0)
        ;
    errno = saved_errno;
}

void handle_int(int signal){
    (void)signal;
    /* shell 本身不响应 SIGINT，交由前台子进程处理 */
}

void clean_up_processes(void){
    int new_count = 0;
    for (int i = 0; i < bg_count; i++) {
        if (bg_pids[i] > 0) {
            int status;
            pid_t ret = waitpid(bg_pids[i], &status, WNOHANG);
            if (ret == 0) {
                bg_pids[new_count++] = bg_pids[i];
            }
            /* ret > 0: 已退出，不加入新数组 */
            /* ret == -1: 出错（如 ECHILD），也移除 */
        }
    }
    bg_count = new_count;
}

void print_prompt(void){
    char *cwd = getcwd(NULL, 0);
    if(cwd != NULL){
        printf("\033[1;32m%s\033[0m $ ", cwd);
        free(cwd);
    } else {
        printf("myshell $ ");
    }
    fflush(stdout);
}

char* getinput(void){
    char* input = (char*)malloc(MAX_INPUT * sizeof(char));
    if (input == NULL) {
        perror("malloc");
        safe_exit(1);
    }

    struct termios old_tio, new_tio;
    if (tcgetattr(STDIN_FILENO, &old_tio) == -1) {
        perror("tcgetattr");
        free(input);
        return NULL;
    }
    g_old_tio = old_tio;
    g_tio_saved = 1;

    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);
    new_tio.c_cc[VMIN] = 1;
    new_tio.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_tio) == -1) {
        perror("tcsetattr");
        free(input);
        return NULL;
    }

    int i = 0;
    int truncated = 0;
    int c;
    while (i < MAX_INPUT - 1) {
        c = getchar();
        if (c == EOF) {
            if (feof(stdin)) {
                tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
                g_tio_saved = 0;
                printf("\n");
                free(input);
                safe_exit(0);
            }
            continue;
        }
        if (c == '\n') {
            break;
        } else if (c == 8 || c == 127) {
            if (i > 0) {
                i--;
                printf("\b \b");
                fflush(stdout);
            }
        } else {
            input[i++] = (char)c;
            putchar(c);
            fflush(stdout);
        }
    }

    /* 如果输入已满，丢弃剩余字符直到换行 */
    if (i == MAX_INPUT - 1) {
        int peek = getchar();
        if (peek != '\n' && peek != EOF) {
            truncated = 1;
            while (peek != '\n' && peek != EOF) {
                peek = getchar();
            }
        }
    }

    input[i] = '\0';

    if (tcsetattr(STDIN_FILENO, TCSANOW, &old_tio) == -1) {
        perror("tcsetattr restore");
    }
    g_tio_saved = 0;
    printf("\n");

    if (truncated) {
        fprintf(stderr, "Warning: input truncated (max %d characters)\n", MAX_INPUT - 1);
    }

    return input;
}

Command deal_with_input(char* input){
    Command cmd;
    cmd.size_of_command = 0;
    cmd.pipe = 0;
    cmd.background = 0;

    char* token = strtok(input, " \t");
    char* prev_token = NULL;
    while(token != NULL && cmd.size_of_command < MAX_ARGS - 1){
        /* 先把上一个 token 存入 */
        if (prev_token != NULL) {
            if (strcmp(prev_token, "|") == 0) {
                cmd.pipe = 1;
            }
            cmd.split_command[cmd.size_of_command++] = prev_token;
        }
        prev_token = token;
        token = strtok(NULL, " \t");
    }

    /* 处理最后一个 token：如果是 & 则标记后台，不存入参数 */
    if (prev_token != NULL) {
        if (strcmp(prev_token, "&") == 0) {
            cmd.background = 1;
        } else {
            if (strcmp(prev_token, "|") == 0) {
                cmd.pipe = 1;
            }
            cmd.split_command[cmd.size_of_command++] = prev_token;
        }
    }

    cmd.split_command[cmd.size_of_command] = NULL;
    return cmd;
}

int waitpid_retry(pid_t pid, int *status, int options){
    int ret;
    do {
        ret = waitpid(pid, status, options);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

void execute_command(Command cmd){
    /* 阻塞 SIGCHLD 防止竞态：fork 后、记录 bg_pids 前子进程已退出 */
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    if (cmd.background) {
        sigprocmask(SIG_BLOCK, &mask, &oldmask);
    }

    pid_t pid = fork();
    if(pid == 0){
        /* 子进程：恢复默认信号处理 */
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);

        execvp(cmd.split_command[0], cmd.split_command);
        perror("execvp");
        int exit_code;
        if (errno == ENOENT) {
            exit_code = 127;
        } else if (errno == EACCES) {
            exit_code = 126;
        } else {
            exit_code = 1;
        }
        fprintf(stderr, "执行失败: %s\n", cmd.split_command[0]);
        _exit(exit_code);
    } else if(pid < 0){
        perror("fork");
        if (cmd.background) {
            sigprocmask(SIG_SETMASK, &oldmask, NULL);
        }
        return;
    } else {
        if(cmd.background == 1){
            if(bg_count < MAX_BG_JOBS){
                bg_pids[bg_count] = pid;
                printf("[%d] %d\n", bg_count + 1, pid);
                bg_count++;
            } else {
                fprintf(stderr, "后台进程数量超出限制，改为前台执行\n");
                sigprocmask(SIG_SETMASK, &oldmask, NULL);
                waitpid_retry(pid, NULL, 0);
                return;
            }
            sigprocmask(SIG_SETMASK, &oldmask, NULL);
        } else {
            int status;
            waitpid_retry(pid, &status, 0);
        }
    }
}

void execute_pipe_command(Command cmd1, Command cmd2, int background){
    int pipefd[2];
    if(pipe(pipefd) == -1){
        perror("pipe");
        return;
    }

    /* 后台模式下阻塞 SIGCHLD */
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    if (background) {
        sigprocmask(SIG_BLOCK, &mask, &oldmask);
    }

    pid_t pid1 = fork();
    if(pid1 == 0){
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);

        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("dup2 stdout");
            _exit(1);
        }
        close(pipefd[1]);
        execvp(cmd1.split_command[0], cmd1.split_command);
        perror("execvp");
        _exit(errno == ENOENT ? 127 : (errno == EACCES ? 126 : 1));
    }

    if (pid1 < 0) {
        perror("fork pid1");
        close(pipefd[0]);
        close(pipefd[1]);
        if (background) sigprocmask(SIG_SETMASK, &oldmask, NULL);
        return;
    }

    pid_t pid2 = fork();
    if(pid2 == 0){
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);

        close(pipefd[1]);
        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            perror("dup2 stdin");
            _exit(1);
        }
        close(pipefd[0]);
        execvp(cmd2.split_command[0], cmd2.split_command);
        perror("execvp");
        _exit(errno == ENOENT ? 127 : (errno == EACCES ? 126 : 1));
    }

    if (pid2 < 0) {
        perror("fork pid2");
        kill(pid1, SIGKILL);
        close(pipefd[0]);
        close(pipefd[1]);
        waitpid_retry(pid1, NULL, 0);
        if (background) sigprocmask(SIG_SETMASK, &oldmask, NULL);
        return;
    }

    close(pipefd[0]);
    close(pipefd[1]);

    if (background) {
        if (bg_count + 1 < MAX_BG_JOBS) {
            bg_pids[bg_count++] = pid1;
            bg_pids[bg_count++] = pid2;
            printf("[%d] %d | %d\n", bg_count / 2, pid1, pid2);
        } else {
            fprintf(stderr, "后台进程数量超出限制，改为前台执行\n");
            sigprocmask(SIG_SETMASK, &oldmask, NULL);
            waitpid_retry(pid1, NULL, 0);
            waitpid_retry(pid2, NULL, 0);
            return;
        }
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
    } else {
        int status1, status2;
        waitpid_retry(pid1, &status1, 0);
        waitpid_retry(pid2, &status2, 0);
    }
}

void restore_terminal(void){
    if (g_tio_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_old_tio);
        g_tio_saved = 0;
    }
}

void safe_exit(int code){
    restore_terminal();
    exit(code);
}
