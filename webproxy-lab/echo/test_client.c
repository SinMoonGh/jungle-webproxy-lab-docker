// test_client.c
#include <stdio.h>
#include "csapp.h"

int main() {
    int clientfd;
    rio_t rio;
    char *host = "127.0.0.1";
    char *port = "8888";
    char buf[MAXLINE];

    clientfd = open_clientfd(host, port);
    if (clientfd < 0) {
        printf("Connection failed.\n");
        return 1;
    }

    Rio_readinitb(&rio, clientfd);

    // 테스트용 메시지 7개 전송
    for (int i = 0; i < 7; i++) {
        sprintf(buf, "message %d\n", i + 1);
        Rio_writen(clientfd, buf, strlen(buf));
    }

    // 응답 7개 받아서 출력
    for (int i = 0; i < 7; i++) {
        Rio_readlineb(&rio, buf, MAXLINE);
        printf("received: %s", buf);
    }

    Close(clientfd);
    return 0;
}