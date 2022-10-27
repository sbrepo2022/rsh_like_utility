#define MAX_CONNECTIONS 8
#define SRV_PORT 5001
#define BUF_SIZE 256

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <memory.h>

struct conndata {
    int cl_sock;
};

pthread_attr_t pattr;

void* processing_connection(void *arg_p);
void *reader(void *arg_p);

int main() {
    pthread_t tid;
    int listen_sock, cl_sock;
    struct sockaddr_in sin, inc_sin;
    int inc_sin_len;

    pthread_attr_init (&pattr);
    pthread_attr_setscope (&pattr, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setdetachstate (&pattr, PTHREAD_CREATE_DETACHED);

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    memset ((char *)&sin, '\0', sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = SRV_PORT;
    bind (listen_sock, (struct sockaddr *)&sin, sizeof(sin));
    listen (listen_sock, MAX_CONNECTIONS);
    printf("Start listening tcp connections on port %d...\n", SRV_PORT);
    while (1) {
        inc_sin_len = sizeof(inc_sin);
        cl_sock = accept(listen_sock, (struct sockaddr *)&inc_sin, &inc_sin_len);

        struct conndata cd;
        cd.cl_sock = cl_sock;
        pthread_create(&tid, &pattr, processing_connection, &cd);
    }
    close(listen_sock);
    
    return 0;
}

void* processing_connection(void *arg_p) {
    char cmd[BUF_SIZE], buf[BUF_SIZE];
    int r_len;
    char ch[1];
    int cl_sock = ((struct conndata*)arg_p)->cl_sock;
    int r_tid;
    
    for (int i = 0; i < BUF_SIZE; i++) {
        if ((r_len = read(cl_sock, ch, 1)) < 1 || i == BUF_SIZE - 1)
            pthread_exit(0);

        cmd[i] = ch[0];
        if (ch[0] == '\n') {
            cmd[i] = '\0';
            break;
        }
    }

    printf("Command accepted: %s\n", cmd);

    int pipefd[2][2];
    pipe(pipefd[0]);
    pipe(pipefd[1]);

    int pid = fork();
    if (pid == 0) {
        close(pipefd[0][1]);
        close(pipefd[1][0]);
        dup2(pipefd[0][0], STDIN_FILENO);
        dup2(pipefd[1][1], STDOUT_FILENO);
        close(pipefd[0][0]);
        close(pipefd[1][1]);
        execlp("/bin/sh", "/bin/sh", "-c", cmd, NULL);
    }

    int r_pid = fork();
    if (r_pid == 0) {
        close(pipefd[0][0]);
        close(pipefd[1][1]);
        close(pipefd[0][1]);
        while ((r_len = read(pipefd[1][0], buf, BUF_SIZE)) > 0) {
            if (write(cl_sock, buf, r_len) < 0) {
                break;
            }
        }
        close(pipefd[1][0]);
        shutdown(cl_sock, SHUT_RD);
        exit(0);
    }

    close(pipefd[0][0]);
    close(pipefd[1][1]);
    close(pipefd[1][0]);
    while ((r_len = read(cl_sock, ch, 1)) > 0) {
        if (write(pipefd[0][1], ch, 1) < 0) {
            break;
        }
    }
    close(pipefd[0][1]);

    kill(pid, SIGINT);
    wait(NULL);
    wait(NULL);
    close(cl_sock);
    printf("Connection closed!\n");
    pthread_exit(0);
}