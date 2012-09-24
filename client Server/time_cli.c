#include	"unp.h"


/* This is the Time_Client. Connets to the server on Port No 8888
   Reads the data from server's socket and writes it out to the 
   stdout. It also writes to parent the status msgs usin pipe */

int
main(int argc, char **argv)
{
	int	sockfd;
	struct 	sockaddr_in servaddr;
	int 	pdes[2];
	char    recvline[MAXLINE + 1];
	int    	n;	

	if (argc != 3)
                err_quit("usage: echocli01 <IPaddress> <write PIPE descriptor> \n\r");

        pdes[1] = *argv[2];


        n = write(pdes[1] , "Time Client\n", 12);
	if( n < 0) printf("\n\r Time_Client: Error in writing to pipe");

	sockfd = Socket(AF_INET, SOCK_STREAM, 0);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(8888);
	Inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

	Connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) ; 
	
        write(pdes[1] , "Connection to server established\n  ", 32);
	while ( (n = Read(sockfd, recvline, MAXLINE)) > 0) {
            recvline[n] = 0;         /* null terminate */
            Fputs(recvline, stdout);
	}

	exit(0);
}


