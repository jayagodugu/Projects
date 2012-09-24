#include "unp.h"
#include "unpthread.h"

int
main(int argc, char **argv)
{
  struct sockaddr_in cliaddr, servaddr1, servaddr2;
  void sig_chld(int);
  pthread_t tid;
  int    option_value;
  void echo(int listenfd_echo);
  void daytime(int confd_daytime);
  
  int listenfd_echo = Socket(AF_INET, SOCK_STREAM, 0);
  int listenfd_daytime = Socket(AF_INET, SOCK_STREAM, 0);
  

  bzero(&servaddr1, sizeof(servaddr1));
  servaddr1.sin_family      = AF_INET;
  servaddr1.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr1.sin_port        = htons(9999);
  
if (setsockopt(listenfd_echo, SOL_SOCKET, SO_REUSEADDR, (char *)&option_value, 
	sizeof(option_value)) < 0) {
	fprintf(stderr, "setsockopt failure\n");
	exit(1);
  }
  bzero(&servaddr2, sizeof(servaddr2));
  servaddr2.sin_family      = AF_INET;
  servaddr2.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr2.sin_port        = htons(8888);
  
if (setsockopt(listenfd_daytime, SOL_SOCKET, SO_REUSEADDR, (char *)&option_value, 
	sizeof(option_value)) < 0) {
	fprintf(stderr, "setsockopt failure\n");
	exit(1);
  }
  Bind(listenfd_echo, (SA *) &servaddr1, sizeof(servaddr1));
  Bind(listenfd_daytime, (SA *) &servaddr2, sizeof(servaddr2));
  
  Listen(listenfd_echo, LISTENQ);
  Listen(listenfd_daytime, LISTENQ);
  
  Signal(SIGCHLD, sig_chld);
  
  for ( ; ; ) {
    socklen_t clilen = sizeof(cliaddr);
	fd_set		rset;
    int maxfd;

	FD_ZERO(&rset);
    FD_SET(listenfd_echo, &rset);
    FD_SET(listenfd_daytime, &rset);
    maxfd = max(listenfd_echo, listenfd_daytime) + 1;
    Select(maxfd, &rset, NULL, NULL, NULL);
    
    if (FD_ISSET(listenfd_echo, &rset)) {	/* socket is readable */

    int connfd_echo = Accept(listenfd_echo, (SA *) &cliaddr, &clilen);
    Pthread_create(&tid, NULL, (void *)&echo, (void *) connfd_echo);
    }

    if (FD_ISSET(listenfd_daytime, &rset)) {	/* socket is readable */
    int connfd_daytime = Accept(listenfd_daytime, (SA *) &cliaddr, &clilen);
    Pthread_create(&tid, NULL, (void *)&daytime, (void *) connfd_daytime);
    }
  }
  exit(0);
}


void daytime(int connfd_daytime)
{
    struct timeval tv;
    fd_set rset;
    int rv;


    Pthread_detach(pthread_self());
    while(1){
        time_t ticks = time(NULL);
        char buff[MAXLINE+1];
        FD_ZERO(&rset);
        FD_SET(connfd_daytime, &rset);
        tv.tv_sec = 5;
        tv.tv_usec = 0; 
        rv=Select(connfd_daytime + 1, &rset, NULL, NULL, &tv);
        if (rv == 0) {
            snprintf(buff, sizeof(buff), "%.24s\r\n", ctime(&ticks));
            Writen(connfd_daytime, buff, strlen(buff));
        }
        else{
            if (FD_ISSET(connfd_daytime, &rset)) { /* read Ctrl+C from client */
	        printf("control+c returned from client \n");
		break;
            }
        }
    }  /* end while */  
    Close(connfd_daytime);
}

/* ****************** action function for echo thread ***************** */

void echo(int connfd_echo)
{
  ssize_t n;
  char    line[MAXLINE];
  
  for ( ; ; ) {
    if ( (n = Readline(connfd_echo, line, MAXLINE)) == 0)
      /* exit (0); */        
	return; /* connection closed by other end */
    
    n = strlen(line);
    Writen(connfd_echo, line, n);
  }
  
}

void sig_chld(int signo)
{
}
