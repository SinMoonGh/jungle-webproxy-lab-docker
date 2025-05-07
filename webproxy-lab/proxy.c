#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

void doit(int fd);
void read_requesthdrs(rio_t *rp);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void *thread(void *vargp);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv)
{
  int listenfd, *connfdp;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  
  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen=sizeof(struct sockaddr_storage);
    connfdp = Malloc(sizeof(int));
    *connfdp = Accept(listenfd, (SA *) &clientaddr, &clientlen);
    Pthread_create(&tid, NULL, thread, connfdp);
  }
}


/*
 * doit - handle one HTTP request/response transaction
 */
void doit(int fd)
{
  int clientfd;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], server_header[MAXLINE];
  char filename[MAXLINE], hostname[MAXLINE], port[MAXLINE];
  rio_t rio, server_rio;
  size_t n;

  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s", method, uri);

  parse_uri(uri, hostname, port, filename);

  make_header(server_header, hostname, filename, &rio, method);

  clientfd = Open_clientfd(hostname, port);
  Rio_readinitb(&server_rio, clientfd);
  Rio_writen(clientfd, server_header, strlen(server_header));

  while((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0)
  {
    printf("%s\n", buf);
    Rio_writen(fd, buf, n);
  }

  Close(clientfd);
}


void parse_uri(const char *uri, char *hostname, char *port, char *path) {
  const char *default_port = "80";
  char *host_start, *port_start, *path_start;

  // 1. "http://" prefix 제거
  if (strncmp(uri, "http://", 7) == 0) {
      host_start = (char *)(uri + 7);  // "http://" 이후부터 시작
  } else {
      host_start = (char *)uri;
  }

  // 2. path 찾기: '/' 이후가 경로
  path_start = strchr(host_start, '/');
  if (path_start != NULL) {
      strcpy(path, path_start);      // 경로 저장
      *path_start = '\0';            // host:port 부분만 남기기 위해 null로 자름
  } else {
      strcpy(path, "/");             // 경로 없으면 루트로 설정
  }

  // 3. 포트가 있는지 확인: ':' 있으면 분리
  port_start = strchr(host_start, ':');
  if (port_start != NULL) {
      *port_start = '\0';            // host 이름만 추출
      strcpy(hostname, host_start);  // hostname 저장
      strcpy(port, port_start + 1);  // ':' 이후부터 port 저장
  } else {
      strcpy(hostname, host_start);  // ':' 없으면 전부 host
      strcpy(port, default_port);    // 기본 포트 80 사용
  }
}


void make_header(char *header, char *hostname, char *path, rio_t *rio, char *method) {
  char buf[MAXLINE];
  char request_line[MAXLINE];
  char host_hdr[MAXLINE] = "";
  char other_hdrs[MAXLINE] = "";

  // 요청 라인 구성 (항상 HTTP/1.0으로 강제)
  sprintf(request_line, "%s %s HTTP/1.0\r\n", method, path);

  // 요청 헤더 읽기 및 필터링
  while (Rio_readlineb(rio, buf, MAXLINE) > 0) {
      if (strcmp(buf, "\r\n") == 0) break; // 헤더 끝

      // Host 헤더 추출
      if (!strncasecmp(buf, "Host:", 5)) {
          strcpy(host_hdr, buf);
          continue;
      }

      // 아래 헤더는 무시 (프록시가 직접 삽입)
      if (!strncasecmp(buf, "Connection:", 11) ||
          !strncasecmp(buf, "Proxy-Connection:", 17) ||
          !strncasecmp(buf, "User-Agent:", 11)) {
          continue;
      }

      // 그 외 헤더는 그대로 저장
      strcat(other_hdrs, buf);
  }

  // Host 헤더가 없으면 기본 생성
  if (strlen(host_hdr) == 0) {
      sprintf(host_hdr, "Host: %s\r\n", hostname);
  }

  // 고정된 헤더
  static const char *user_agent_hdr =
      "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
  static const char *conn_hdr = "Connection: close\r\n";
  static const char *proxy_conn_hdr = "Proxy-Connection: close\r\n";

  // 최종 헤더 조립
  sprintf(header, "%s%s%s%s%s%s\r\n",
          request_line, host_hdr, conn_hdr, proxy_conn_hdr, user_agent_hdr, other_hdrs);
}


void *thread(void *vargp){
  int connfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  doit(connfd);
  Close(connfd);
  return NULL;
}