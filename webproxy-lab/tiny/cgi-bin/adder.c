/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void)
{
  char *buf, *p;                       // buf: 전체 쿼리 문자열, p: '&'를 찾기 위한 포인터
  char arg1[MAXLINE], arg2[MAXLINE];  // 첫 번째/두 번째 인자를 담을 문자열 버퍼
  char content[MAXLINE];              // 최종 출력할 HTML 콘텐츠
  int n1 = 0, n2 = 0;                  // 파싱한 두 정수

  /* 쿼리 문자열에서 두 인자를 추출 */
  if ((buf = getenv("QUERY_STRING")) != NULL)
  {
    // QUERY_STRING 예: "num1=3&num2=5"
    p = strchr(buf, '&');     // '&' 위치 찾기 → "num2=5" 시작 위치
    *p = '\0';                // '&'를 널 문자로 바꿔 "num1=3"과 "num2=5"를 분리

    strcpy(arg1, buf);        // arg1 = "num1=3"
    strcpy(arg2, p + 1);      // arg2 = "num2=5"

    // '=' 다음 문자열을 정수로 변환
    n1 = atoi(strchr(arg1, '=') + 1);  // n1 = 3
    n2 = atoi(strchr(arg2, '=') + 1);  // n2 = 5
  }

  /* 결과 HTML 콘텐츠 구성 */
  sprintf(content, "QUERY_STRING=%s\r\n<p>", buf);  // 쿼리 문자열 출력
  sprintf(content + strlen(content), "Welcome to add.com: ");
  sprintf(content + strlen(content), "THE Internet addition portal.\r\n<p>");
  sprintf(content + strlen(content), "The answer is: %d + %d = %d\r\n<p>",
          n1, n2, n1 + n2);                         // 덧셈 결과 출력
  sprintf(content + strlen(content), "Thanks for visiting!\r\n");

  /* HTTP 응답 헤더 및 바디 출력 (stdout → 클라이언트로 전달됨) */
  printf("Content-type: text/html\r\n");           // MIME 타입
  printf("Content-length: %d\r\n", (int)strlen(content));  // 길이 헤더
  printf("\r\n");                                   // 헤더와 바디 구분
  printf("%s", content);                            // HTML 본문 출력
  fflush(stdout);                                   // 출력 즉시 전송

  exit(0);  // 정상 종료
}
/* $end adder */
