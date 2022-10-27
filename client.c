/*
Заметки:
Перенаправить поток ошибок в поток стандартного вывода (чтобы посмотреть ошибки на сервере): "2>&1"
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <memory.h>
#include <unistd.h>
#include <wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#define SRV_PORT 5001
#define CLNT_PORT 5002
#define BUF_SIZE 256

void int_handler (int signum);
int sock;

int main (int argc, char **argvs) {
    int from_len;
    char buf[BUF_SIZE];
    struct hostent *hp;
    struct sockaddr_in clnt_sin, srv_sin;

    char cmd[BUF_SIZE] = "";
    if (argc < 3) {
        printf("Needs at least 2 args: host and command\n");
        return 1;
    }

    for (int i = 2; i < argc; i++) {
        strncat(cmd, argvs[i], BUF_SIZE - 2);
        strncat(cmd, " ", BUF_SIZE - 2);
    }
    strcat(cmd, "\n");

    sock = socket(AF_INET, SOCK_STREAM, 0);
    memset((char *)&clnt_sin, '\0', sizeof(clnt_sin));
    clnt_sin.sin_family = AF_INET;
    clnt_sin.sin_addr.s_addr = INADDR_ANY;
    clnt_sin.sin_port = CLNT_PORT;
    bind(sock, (struct sockaddr *)&clnt_sin, sizeof(clnt_sin));

    memset ((char *)&srv_sin, '\0', sizeof(srv_sin));
    hp = gethostbyname(argvs[1]);
    srv_sin.sin_family = AF_INET;
    memcpy ((char *)&srv_sin.sin_addr,hp->h_addr,hp->h_length);
    srv_sin.sin_port = SRV_PORT;

    if (connect (sock, (struct sockaddr *)&srv_sin, sizeof(srv_sin)) == -1) {
        printf("Can not open connection with server!\n");
        return 2;
    }
    write (sock, cmd, strlen(cmd));

    int r_len;
    char ch[1];
    int r_pid = fork();
    if (r_pid == 0) {
        while ((r_len = read(STDIN_FILENO, buf, BUF_SIZE)) > 0) {
            if (write(sock, buf, r_len) < 0) {
                break;
            }
        }
        shutdown(sock, SHUT_WR);
        exit(0);
    }

    // set sigint handler
    struct sigaction sh;
    sh.sa_handler = int_handler;
    sigemptyset (&sh.sa_mask);
    sh.sa_flags = 0;
    sigaction (SIGINT, &sh, NULL);

    while ((r_len = read(sock, ch, 1)) > 0) {
        if (write(STDOUT_FILENO, ch, 1) < 0) {
            break;
        }
    }

    kill(r_pid, SIGINT);
    wait(NULL);
    close(sock);
    exit(0);
}

void int_handler(int signum) {
    shutdown(sock, SHUT_RD);
}