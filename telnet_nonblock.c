#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <arpa/telnet.h>                
#include <netinet/in.h>
#include <netdb.h>

#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

int
recv_data(void);

void
sig_term_handler(int);


//void
//init_signal(void);



/* ソケット */
int g_soc = -1;
/* 終了フラグ */
volatile sig_atomic_t g_end = 0;
/* サーバにソケット接続 */
int
client_socket(const char *hostnm, const char *portnm)
{
     char nbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
     struct addrinfo hints, *res0;
     int soc, errcode;
    
     /* アドレス情報のヒントをゼロクリア */
     (void) memset(&hints, 0, sizeof(hints));
     hints.ai_family = AF_INET;
     hints.ai_socktype = SOCK_STREAM;
     /* アドレス情報の決定 */
     if ((errcode = getaddrinfo(hostnm, portnm, &hints, &res0)) != 0) {
          (void) fprintf(stderr, "getaddrinfo():%s\n", gai_strerror(errcode));
          return (-1);
     }
     if ((errcode = getnameinfo(res0->ai_addr, res0->ai_addrlen,
                                nbuf, sizeof(nbuf),
                                sbuf, sizeof(sbuf),
                                NI_NUMERICHOST | NI_NUMERICSERV)) != 0) {
          (void) fprintf(stderr, "getnameinfo():%s\n", gai_strerror(errcode));
          freeaddrinfo(res0);
          return (-1);
     }
     (void) fprintf(stderr, "addr=%s\n", nbuf);
     (void) fprintf(stderr, "port=%s\n", sbuf);
     /* ソケットの生成 */
     if ((soc = socket(res0->ai_family, res0->ai_socktype, res0->ai_protocol))
         == -1) {
          perror("socket");
          freeaddrinfo(res0);
          return (-1);
     }
     /* コネクト */
     if (connect(soc, res0->ai_addr, res0->ai_addrlen) == -1) {
          perror("connect");
          (void) close(soc);
          freeaddrinfo(res0);
          return (-1);
     }
     freeaddrinfo(res0);
     return (soc);
}

int
set_block(int fd, int flag)
{
     int flags;

     if ((flags = fcntl(fd, F_GETFL, 0)) == -1) {
          perror("fcntl");
          return (-1);
     }

     if (flags == 0) {
          (void) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
     }else if(flag == 1){
          (void) fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
     }
     return (0);
}
     


void
send_recv_loop(void)
{
     int data_flag;
     char c;
    
     /* エコーなし、RAWモード */
     (void) system("stty -echo raw");
     /* バッファリングOFF */
     (void) setbuf(stdin, NULL);
     (void) setbuf(stdout, NULL);


     for (;;) {
          data_flag = 0;
          if(recv_data() == -1){
               if (errno != EAGAIN) {
                    break;
               }
          }else{
               data_flag = 1;
          }
          c = getchar();
          if (c != EOF) {
               if (send(g_soc, &c, 1,0) == -1) {
                    if (errno != EAGAIN) {
                         break;
                    }
               }
               data_flag = 1;
          }

          if (data_flag == 0) {
               (void) usleep(10000);
          }
          
     }
     (void) set_block(0,1);
     (void) set_block(g_soc, 1);
     (void)system("stty echo cooked -istrip");

}


int
recv_data(void)
{
     char buf[8];
     char c;
     if (recv(g_soc, &c, 1, 0) <= 0) {
          return (-1);
     }
     if ((int) (c & 0xFF) == IAC) {
          /* コマンド */
          if (recv(g_soc, &c, 1, 0) == -1) {
               perror("recv");
               return (-1);
          }
          if (recv(g_soc, &c, 1, 0) == -1) {
               perror("recv");
               return (-1);
          }
          /* 否定で応答 */
          (void) snprintf(buf, sizeof(buf), "%c%c%c", IAC, WONT, c);
          if (send(g_soc, buf, 3, 0) == -1) {
               perror("send");
               return (-1);
          }
     } else {
          /* 画面へ */
          (void) fputc(c & 0xFF, stdout);
     }
     return (0);
}
/* シグナルハンドラ */
void
sig_term_handler(int sig)
{
     g_end = sig;
}
/* シグナルの設定 */
void
init_signal(void)
{
     /* 終了関連 */
     (void) signal(SIGINT, sig_term_handler);
     (void) signal(SIGTERM, sig_term_handler);
     (void) signal(SIGQUIT, sig_term_handler);
     (void) signal(SIGHUP, sig_term_handler);
}
int
main(int argc,char *argv[])
{
     char *port;
     if (argc <= 1) {
          (void) fprintf(stderr, "telnet1 hostname [port]\n");
          return (-1);
     } else if (argc <= 2) {
          port = "telnet";
     } else {
          port = argv[2];
     }
     /* ソケット接続 */
     if ((g_soc = client_socket(argv[1], port)) == -1) {
          return (-1);
     }
     /* シグナル設定 */
     init_signal();
     /* メインループ */
     send_recv_loop();
     /* プログラム終了 */
     /* ソケットクローズ */
     if (g_soc != -1) {
          (void) close(g_soc);
     }
     (void) fprintf(stderr, "Connection Closed.\n");
     return (0);
}


