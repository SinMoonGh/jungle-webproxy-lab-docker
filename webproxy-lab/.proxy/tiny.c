/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
void serve_static_headers(int fd, char *filename, int filesize);

int main(int argc, char **argv)
{
  int listenfd, connfd; 
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}


void doit(int fd) {
  int is_static;  // 요청한 URI가 정적 콘텐츠인지 동적 콘텐츠인지 구분
  struct stat sbuf; // 요청한 파일의 상태(존재 여부, 권한 등)를 저장하는 구조체

  // 클라이언트 요청 줄과 메서드, URI, HTTP 버전 등을 저장할 버퍼
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];

  // 정적 콘텐츠일 경우 파일 이름, 동적 콘텐츠일 경우 CGI 인자들이 저장될 버퍼
  char filename[MAXLINE], cgiargs[MAXLINE];

  rio_t rio;  // robust I/O를 위한 구조체

  // 요청 줄 읽기
  Rio_readinitb(&rio, fd);                 // rio 구조체를 fd로 초기화
  if (!Rio_readlineb(&rio, buf, MAXLINE)) return;       // 요청 줄 한 줄 읽음
  printf("Request headers:\n");
  printf("%s", buf);                       // 요청 줄 출력 (예: GET /index.html HTTP/1.0)

  // 요청 줄을 파싱하여 메서드, URI, 버전 추출
  sscanf(buf, "%s %s %s", method, uri, version);

  // Tiny는 GET 메서드만 지원하므로, 다른 메서드는 에러 응답
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  // 요청 헤더는 무시하고 읽기 처리만 진행 (다음 요청 처리를 위한 준비)
  read_requesthdrs(&rio);

  /* URI를 분석해 파일 이름과 CGI 인자 추출 */
  is_static = parse_uri(uri, filename, cgiargs);  // 정적이면 1, 동적이면 0 반환

  // 요청한 파일이 존재하는지 확인. 실패하면 404 응답
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  /* 파일 존재 시, 정적/동적 콘텐츠에 따라 분기 처리 */
  if (is_static) {
    // 정적 콘텐츠일 경우: 일반 파일이며 읽기 권한이 있어야 함
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    // 만약 HEAD 메서드라면, 본문 없이 헤더만 전송
    if (strcasecmp(method, "HEAD") == 0) {
      serve_static_headers(fd, filename, sbuf.st_size);
    } else {
      // GET 메서드일 경우 본문도 함께 전송
      serve_static(fd, filename, sbuf.st_size);
    }
  } else {
    // 동적 콘텐츠일 경우: 일반 파일이며 실행 권한이 있어야 함
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    // 동적 콘텐츠 제공
    serve_dynamic(fd, filename, cgiargs);
  }
}


void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];  // 응답 헤더와 바디를 저장할 버퍼

  /* HTML 형식의 에러 메시지 바디 생성 */
  sprintf(body, "<html><title>Tiny Error</title>");  // HTML 타이틀
  sprintf(body, "%s<body bgcolor=\"ffffff\">\r\n", body);  // 배경색 흰색으로 설정
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);   // ex)"404: Not found"
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);  // ex)"Tiny couldn't find this file: /bad/path"
  sprintf(body, "%s<hr><em>The Tiny Web Server</em>\r\n", body);  // 서버 서명

  /* HTTP 응답 헤더 생성 및 전송 */
  // 상태 줄 전송: 예) HTTP/1.0 404 Not found
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));

  // 콘텐츠 타입: HTML
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // 콘텐츠 길이 명시
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));

  /* HTML 본문 전송 */
  Rio_writen(fd, body, strlen(body));  // 위에서 만든 에러 HTML 바디 전송
}


void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];  // 요청 헤더 한 줄씩 저장할 버퍼

  // 첫 번째 요청 헤더 줄 읽기
  Rio_readlineb(rp, buf, MAXLINE);

  // 빈 줄(\r\n)이 나올 때까지 헤더를 계속 읽음
  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);  // 다음 줄 읽기
    printf("%s", buf);  // 헤더 내용 출력 (디버깅 목적)
  }

  return;  // 헤더는 따로 처리하지 않고 무시
}


int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;	// 동적 콘텐츠 인자 분리에 쓰이는 포인터

  /* 정적 콘텐츠: URI에 "cgi-bin"이 포함되지 않은 경우 */
  if (!strstr(uri, "cgi-bin")) {
    strcpy(cgiargs, "");         // CGI 인자는 없음 (빈 문자열로 설정)
    strcpy(filename, ".");       // 상대 경로 시작 (서버 루트 기준)
    strcat(filename, uri);       // URI를 이어 붙여 실제 파일 경로 생성

    // URI가 '/'로 끝나면 기본 파일인 "home.html"로 설정
    if (uri[strlen(uri) - 1] == '/') {
      strcat(filename, "home.html");
    }

    return 1;  // 정적 콘텐츠임을 반환
  }

  /* 동적 콘텐츠: URI에 "cgi-bin"이 포함된 경우 */
  else {
    ptr = index(uri, '?');       // '?' 문자를 기준으로 CGI 인자 분리 시도

    if (ptr) {
      // '?' 뒤의 문자열을 cgiargs에 저장
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';               // '?' 자리에 널 문자 삽입 → URI 문자열 자르기
    } else {
      strcpy(cgiargs, "");       // 인자가 없는 경우
    }

    strcpy(filename, ".");       // 상대 경로 시작
    strcat(filename, uri);       // 나머지 URI로 파일 이름 구성 (스크립트 경로)

    return 0;  // 동적 콘텐츠임을 반환
  }
}


void serve_static(int fd, char *filename, int filesize) {
  int srcfd;                           // 디스크에서 파일을 열 때 사용할 파일 디스크립터
  char *srcp;                          // 파일을 메모리로 매핑할 포인터
  char filetype[MAXLINE], buf[MAXBUF];  // MIME 타입, 응답 헤더를 저장할 버퍼
  char *buf_body = malloc(filesize);

  /* 응답 헤더 생성 및 전송 */
  get_filetype(filename, filetype);   // 파일 확장자 기반으로 MIME 타입 결정

  // 상태 줄, 헤더 정보 구성
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);

  // 헤더 전송
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);  // 터미널에도 출력 (디버깅용)

  /* 응답 바디 전송 (파일 내용 전송) */
  srcfd = Open(filename, O_RDONLY, 0);  // 파일 열기 (읽기 전용)

  read(srcfd, buf_body, filesize);

  Close(srcfd);                         // 매핑 완료 후 파일 디스크립터는 닫음

  Rio_writen(fd, buf_body, filesize);       // 매핑된 파일 내용을 클라이언트로 전송
  free(buf_body);
}


void get_filetype(char *filename, char *filetype) {
  if (strstr(filename, ".html")) {
    strcpy(filetype, "text/html");
  } else if (strstr(filename, ".gif")) {
    strcpy(filetype, "image/gif");
  } else if (strstr(filename, ".png")) {
    strcpy(filetype, "image/png");
  } else if (strstr(filename, ".jpg")) {
    strcpy(filetype, "image/jpeg");
  } else if (strstr(filename, ".mpg") || strstr(filename, ".mpeg")) {
    strcpy(filetype, "video/mpeg");  // MPEG 비디오 파일 대응
  } else if (strstr(filename, ".mp4")) {
    strcpy(filetype, "video/mp4");   // MP4도 함께 대응 가능
  } else {
    strcpy(filetype, "text/plain");
  }
}


void serve_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE];               // 응답 헤더를 담을 버퍼
  char *emptylist[] = {NULL};      // CGI 프로그램에 넘길 인자 리스트 (빈 리스트)

  /* 클라이언트에게 기본적인 HTTP 응답 헤더 전송 */
  // printf("")
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));  // 상태 줄 전송

  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));  // 서버 이름 헤더 전송

  /* 자식 프로세스를 생성하여 CGI 프로그램 실행 */
  if (Fork() == 0) {  // 자식 프로세스
    // CGI 환경 변수 설정 (쿼리 문자열 전달)
    setenv("QUERY_STRING", cgiargs, 1);  // 예: QUERY_STRING="1&2"

    // 표준 출력을 클라이언트 소켓으로 변경 (CGI 출력이 클라이언트로 직접 전송됨)
    Dup2(fd, STDOUT_FILENO);  // STDOUT_FILENO == 1

    // CGI 프로그램 실행 (인자 없음, 환경 변수는 부모와 동일)
    Execve(filename, emptylist, environ);  // 실패 시 return하지 않고 종료됨
  }

  /* 부모 프로세스는 자식이 끝날 때까지 기다림 */
  Wait(NULL);  // 좀비 프로세스 방지
}

void serve_static_headers(int fd, char *filename, int filesize) {
  char filetype[MAXLINE], buf[MAXBUF];  // MIME 타입, 응답 헤더를 저장할 버퍼

  // 파일 타입 결정
  get_filetype(filename, filetype);

  // 응답 헤더 생성
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);

  // 헤더 전송
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);  // 터미널에도 출력 (디버깅용)
}