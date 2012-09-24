#include "unp.h"
#include "unpifiplus.h"
#define CLIENT_CONFIG "client.in"
#define MAX_LINE_LENGTH 256
#define MAX_IF 10
#define LOCALHOST "127.0.0.1"
#include <stdio.h>
#include <math.h>
#include	"unprtt.h"
#include	<setjmp.h>
#define ACKNOWLEDGEMENT "ACK"
#define	RTT_DEBUG 1
#define MAX_WIN_SIZE 4096
#include <math.h>
#include <sys/types.h>

static struct rtt_info   rttinfo;
static int	rttinit = 0;
static struct msghdr	msgsend, msgrecv;	/* assumed init to 0 */
static int stopConsumer=0;
static void	sig_alrm(int signo);
static sigjmp_buf	jmpbuf;
static int expectedDgramSeq=0;
int server_port;
char file_name[MAX_LINE_LENGTH];
static int recv_sw_size,orecv_sw_size;
int rand_seed;
float prob_datagram_loss;
int rate_millisec;
int if_counter;
int same_host=0;
char IPServer[INET_ADDRSTRLEN];
char IPClient[INET_ADDRSTRLEN];
char ServerEpthPort[INET_ADDRSTRLEN];
pthread_mutex_t	tracker_lock = PTHREAD_MUTEX_INITIALIZER;
int fileReadWindowCounter = 0;
static int childFin=0;
struct sockaddr_in client, server;

pthread_t pid;
pthread_mutex_t buflock= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static int bufLen = 512 - (5*sizeof(int));
typedef struct dgram{
	int seqNum;
	int ackNum;
	int EOFFlag;
	int windowSize;
	int datasize;
	char buf[512 - (5*sizeof(int))];
}dgram;

dgram recvDgram;
dgram sendDgram;
dgram ackDgram;

struct dgram* window[MAX_WIN_SIZE];

int dgramLen = sizeof(dgram);

/* The wrapper to read line */
static void consumer();
void logger(char* info, char* data);
int send_filename_and_get_server_port(char* file_name,int flags, int sockfd);
int readLine(char *line,FILE *file)
{
    if(fgets(line, MAX_LINE_LENGTH, file) == NULL)
        return 0;
    else
        return 1;
}
/* Read the server config file */
/* This file contains the following
1.RNG for probablity & packet loss simulation
2.RNG to compute sleeptime for datagram recieve thread */
/* if the assigned probabilty is larger than the randum number
then the data gram can be received. Otherwise the recived packet
can be dropped by the application simulating the packet loss */
int can_receive_dtagram(float p, int seed){
    float rnum=0.0;
    //srand(seed);
    srand48(seed);
    printf("\n\n-------------------PACKET---------------------------\n");
    rnum=(double)rand()/(double)RAND_MAX;
    if(rnum < p){
        printf("--- R Randomizer (rnum %f) < (p %f) --- \n",rnum,p);
        return 0;}  /* yes - app shall go ahead and receive the datagram */
    else{
        printf("--- R Randomizer (rnum %f) > (p %f) --- \n",rnum,p);
        return 1;} /* No - application shall drop the datagram */
}

/* if the assigned probabilty is larger than the randum number
then the ack can be sent. Otherwise the ack will not be sent by the
application simulating the ack loss */

int can_send_dtagram(float p, int seed){
    float rnum=0.0;
    rnum=(double)rand()/(double)RAND_MAX;
    //printf("----------------------------------------------\n ");
    if(rnum < p){
        printf("--- S Randomizer (rnum %f) < (p %f) --- \n",rnum,p);
        return 0;} /* yes - app shall go ahead with sending of ack */
    else{
        printf("--- S Randomizer (rnum %f) > (p %f) --- \n",rnum,p);
        return 1;
        } /* No - app shall not send the ack */
}

/*
  The function to read the server.in file and store the
  values in global vars
*/

int readConfig()
{
    char line[MAX_LINE_LENGTH] = {0};
    int linenum=0;
    FILE *file = fopen (CLIENT_CONFIG, "r" );
    int ret = 1;
    while(1)
    {
        if(file == 0)
        {
            printf("Syntax error, line %d\n", linenum);
            ret = 0;
            break;
        }
        if(readLine(line,file))
        {
            linenum++;

            if(sscanf(line, "%s", IPServer) != 1)
            {
                printf("Syntax error, line %d\n", linenum);
                ret = 0;
                break;
            }
            printf("Line %d: Server IP is %s \n", linenum, IPServer);
        }
        else
        {
            printf("Readline returned failure at line %d\n", linenum);
            ret = 0;
            break;
        }

        if(readLine(line,file))
        {
            linenum++;
            if(sscanf(line, "%d", &server_port) != 1)
            {
                printf("Syntax error, line %d\n", linenum);
                ret = 0;
                break;
            }
            printf("Line %d: Server port is %d \n", linenum, server_port);

        }
        else
        {
            printf("Readline returned failure at line %d\n", linenum);
            ret = 0;
            break;
        }

        if(readLine(line,file))
        {
            linenum++;
            if(sscanf(line, "%s", file_name) != 1)
            {
                printf("Syntax error, line %d\n", linenum);
                ret = 0;
                break;
            }
            printf("Line %d: File name is %s \n", linenum, file_name);
        }
        else
        {
            printf("Readline returned failure at line %d\n", linenum);
            ret = 0;
            break;
        }


        if(readLine(line,file))
        {
            linenum++;
            if(sscanf(line, "%d", &recv_sw_size) != 1)
            {
                printf("Syntax error, line %d\n", linenum);
                ret = 0;
                break;
            }
            printf("Line %d: Reciever Sliding Window size is %d \n", linenum, recv_sw_size);
        }
        else
        {
            printf("Readline returned failure at line %d\n", linenum);
            ret = 0;
            break;
        }

        if(readLine(line,file))
        {
            linenum++;
            if(sscanf(line, "%d", &rand_seed) != 1)
            {
                printf("Syntax error, line %d\n", linenum);
                ret = 0;
                break;
            }
            printf("Line %d: Random seed is %d \n", linenum, rand_seed);
        }
        else
        {
            printf("Readline returned failure at line %d\n", linenum);
            ret = 0;
            break;
        }

        if(readLine(line,file))
        {
            linenum++;
            if(sscanf(line, "%f", &prob_datagram_loss) != 1)
            {
                printf("Syntax error, line %d\n", linenum);
                ret = 0;
                break;
            }
            printf("Line %d: Probability of datagram loss is %.1f \n", linenum, prob_datagram_loss);
            if(prob_datagram_loss < 0 || prob_datagram_loss > 1)
            {
                 printf("Line %d: ERROR: This should be a real number in the range [ 0.0 , 1.0 ]\n");
                 ret = 0;
                 break;
            }
        }
        else
        {
            printf("Readline returned failure at line %d\n", linenum);
            break;
        }

        if(readLine(line,file))
        {
            linenum++;
            if(sscanf(line, "%d", &rate_millisec) != 1)
            {
                printf("Syntax error, line %d\n", linenum);
                ret = 0;
                break;
            }
            printf("Line %d: Rate in milisec is %d \n", linenum, rate_millisec);
        }
        else
        {
            printf("Readline returned failure at line %d\n", linenum);
            ret = 0;
            break;
        }

        // Success if here
        break;
    }
    return ret;
}

int main()
{
    printf("Hi Group\n");
    logger("","");
    if(readConfig() != 1)
    {
        printf("Error in Config File\n");
        exit(0);
    }

    struct ifi_info	*ifi, *ifihead;
	struct sockaddr_in	*ca_ip, *ca_mask, *ca_host_address;
	u_char	*ptr;
	int	i, family, doaliases;
    family = AF_INET;
	doaliases = 1;
	if_counter=0;
	int on=1;
	int flags = 0;
	int scenario = 0;
    int sockfd = Socket(AF_INET,SOCK_DGRAM,0);
    char ip_address[INET_ADDRSTRLEN] = {0};
    char subnet_mask[INET_ADDRSTRLEN] = {0};
    char host_address[INET_ADDRSTRLEN] = {0};
    int socksize = sizeof(struct sockaddr_in);
    int newport = 0;
    printf("\n--------------------------\n");
	for (ifihead = ifi = Get_ifi_info_plus(family, doaliases);
        (ifi != NULL) && (if_counter < MAX_IF) ; ifi = ifi->ifi_next)
    {
        // Create the socket
        memset(ip_address,0,INET_ADDRSTRLEN);
        memset(subnet_mask,0,INET_ADDRSTRLEN);
        memset(host_address,0,INET_ADDRSTRLEN);

        // Extract and fill information from the interfaces to the array
        // using internals of sock_ntop_host.c
        ca_ip = (struct sockaddr_in*) (ifi->ifi_addr);
        if (ca_ip != NULL)
        {
            Inet_ntop(AF_INET,&ca_ip->sin_addr,ip_address, sizeof(ip_address));
        }
        else
        {
             logger("ERROR:", "ca_ip is NULL");
             continue;
        }

        ca_mask = (struct sockaddr_in*) (ifi->ifi_ntmaddr);
        if (ca_mask != NULL)
        {
            Inet_ntop(AF_INET,&ca_mask->sin_addr,subnet_mask, sizeof(subnet_mask));
            //strcpy(subnet_mask),  Sock_ntop_host(ca_mask, sizeof(*ca_mask)));
        }
        else
        {
             logger("ERROR:", "sa_mask is NULL");
             continue;
        }
        unsigned long ca_network_host = 0;
        if ((ca_ip != NULL) && (ca_mask != NULL))
        {
            ca_network_host = (ca_ip->sin_addr.s_addr) & (ca_mask->sin_addr.s_addr);
        }
        else
        {
             logger("ERROR:", "ca_ip or ca_mask is NULL");
             continue;
        }

        ca_host_address = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in));
        if (ca_host_address != NULL)
        {
            ca_host_address->sin_addr.s_addr = ca_network_host;
            Inet_ntop(AF_INET,&ca_host_address->sin_addr,host_address, sizeof(host_address));
            free(ca_host_address);
        }
        else
        {
             logger("ERROR:", "ca_host_address is NULL");
             continue;
        }
        printf("-------------%d------------\n",if_counter+1);
        logger("IP address:", ip_address);
		logger("Subnet Mask:",subnet_mask);
		logger("Host Address:",host_address);
        if_counter++;

        // Check if we see the same IP for both Client and server
        if(strcmp(ip_address,IPServer) == 0)
        {
            scenario = 1;
            // Do not break, as we need to print all Interfaces
        }

        unsigned long server_network_host = 0;
        bzero(&server, sizeof(struct sockaddr_in));
        Inet_pton(AF_INET, IPServer, &server.sin_addr);
        server.sin_family = AF_INET;
        server.sin_port = htons(server_port);
        server_network_host = (server.sin_addr.s_addr) & (ca_mask->sin_addr.s_addr);
        if(scenario == 0)
        {
            if(server_network_host == ca_network_host)
            {
                strcpy(IPClient, ip_address);
                scenario = 2;
            }
        }

	}
    printf("--------------------------\n\n");
    free_ifi_info_plus(ifihead);

    if (scenario == 0)
    {
        printf("#### Server is REMOTE, arbitrarily choose last Client IP ####\n");
        strcpy(IPClient, ip_address);
    }
	else if(scenario == 1)
	{
		printf("#### Server is LOCAL - Loopback ####\n");
        strcpy(IPClient, LOCALHOST);
        strcpy(IPServer, LOCALHOST);
	}
	else if(scenario == 2)
	{
        flags = flags & MSG_DONTROUTE;
		printf("#### Server is LOCAL - Same Subnet, Using MSG_DONTROUTE ####\n");
	}

	printf("   IPClient: %s\n", IPClient);
    bzero(&client, sizeof(struct sockaddr_in));
	client.sin_family = AF_INET;

	// The client now creates a UDP socket and calls bind on IPclient,
	// with 0 as the port number. This will cause the kernel
	// to bind an ephemeral port to the socket.
	client.sin_port = htons(0);
	Inet_pton(AF_INET,IPClient,&client.sin_addr);

	printf("   IPServer: %s\n", IPServer);
	bzero(&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(server_port);
    Inet_pton(AF_INET,IPServer,&server.sin_addr);

    // After the bind, use the getsockname function (Section 4.10)
    // to obtain IPclient and the ephemeral port number that has been
    // assigned to the socket, and print that information out on stdout,
    // with an appropriate message and appropriately formatted.
	sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
	Bind(sockfd, (SA*) &client, sizeof(struct sockaddr_in));
    Getsockname(sockfd, (SA*) &client, &socksize);
	if(connect(sockfd, (SA*)&server, sizeof(struct sockaddr_in)) < 0)
	{
		printf("Error : Server connect %d\n", errno);
		exit(0);
    }
	if(getpeername(sockfd,(SA*) &server, &socksize) < 0)
	{
		printf("Error : getpeername %d\n", errno);
		exit(0);
	}

	logger("","");

	printf("---- INFORMATION ---- \n");
	printf("   IPClient    - %s\n",inet_ntop(AF_INET, &(client.sin_addr), IPClient, INET_ADDRSTRLEN));
	printf("   Client Port	- %d\n",(int)ntohs(client.sin_port));
	printf("   IPServer    - %s\n",inet_ntop(AF_INET, &(server.sin_addr), IPServer, INET_ADDRSTRLEN));
	printf("   Server PORT - %d\n",(int)ntohs(server.sin_port));


	// Step 1:
	// Tell the server the filename, but reliable
	newport = send_filename_and_get_server_port(file_name, flags, sockfd);
    printf("   Server NEW PORT - %d\n",newport);
	bzero(&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(newport);
    Inet_pton(AF_INET,IPServer,&server.sin_addr);
	if(connect(sockfd, (SA*)&server, sizeof(struct sockaddr_in)) < 0)
	{
		printf("Error : Server connect %d\n", errno);
		exit(0);
    }
	if(send(sockfd,ACKNOWLEDGEMENT,strlen(ACKNOWLEDGEMENT),flags)<0)
	{
		printf("Error : send() %d\n", errno);
	}


	// Start the window mechanism here

	// First send the datagram to tell own seq no and window size
    Pthread_create(&pid,NULL,&consumer,NULL);
    int sz = sizeof(struct sockaddr_in);
    int len=0;
    orecv_sw_size = recv_sw_size;
    printf("\n################### Begin Data Trasnfer ####################\n\n ");
    while(1)
    {
        struct dgram * blob = NULL;

        // Here is where we will recieve and send data
        len = recvfrom(sockfd, &recvDgram, dgramLen, flags,(SA*)&server, &sz);
        if(len < 0)
        {
            printf("Error in recvfrom\n");
        }
        printf("expectedDgramSeq = %d, recvDgram.seqNum = %d\n", expectedDgramSeq,recvDgram.seqNum);

        if(expectedDgramSeq == recvDgram.seqNum)
        {
            struct dgram * blob = (struct dgram*)malloc(sizeof (struct dgram));
            memset(blob, 0,sizeof (struct dgram));
            memcpy(blob->buf,recvDgram.buf,recvDgram.datasize);

            blob->seqNum=recvDgram.seqNum;
            blob->windowSize=recvDgram.windowSize;
            blob->EOFFlag=recvDgram.EOFFlag;
            if(can_receive_dtagram(prob_datagram_loss ,rand_seed))
            {

                printf("----> ALLOW Receiving -- Data Seq no. %d\n", recvDgram.seqNum);
                Pthread_mutex_lock(&buflock);
                    //printf("---%s\n",blob->buf);
                    if(window[blob->seqNum%recv_sw_size] != NULL)
                    {
                         printf("\n^^^^^^^^^^^^^^^^ X DROP Receiving - Congestion on Reciever^^^^^^^^^^^^^^^^\n");
                         free(blob);
                    }
                    else
                    {
                        window[blob->seqNum%recv_sw_size] = blob;
                        orecv_sw_size--;
                        expectedDgramSeq++;
                        sendDgram.ackNum = expectedDgramSeq;
                        sendDgram.windowSize = orecv_sw_size;
                    }
                Pthread_cond_signal(&cond);
                Pthread_mutex_unlock(&buflock);
            }
            else
            {
                printf("----X DROP Receiving -- Data Seq %d\n", recvDgram.seqNum);
                free(blob);
                continue;
            }


            strcpy(sendDgram.buf,ACKNOWLEDGEMENT);
            printf("Current Window Size: %d\n", sendDgram.windowSize);
            if(can_send_dtagram(prob_datagram_loss ,rand_seed))
            {
                if(send(sockfd, &sendDgram, dgramLen,flags)<0)
                {
                        printf("ERROR sending data\n");
                        exit(0);
                }
                else
                {
                        printf("----> SEND ACK(%d)\n",sendDgram.ackNum);
                        if(recvDgram.EOFFlag)
                        {
                            printf("----> SEND LAST ACK(%d)\n",sendDgram.ackNum);
                            stopConsumer=1;
                            int i=10;
                            while(childFin!=1)
                            {
                                Pthread_cond_signal(&cond);
                                sleep(1);
                            }
                            break;
                        }
                }
            }
            else
            {
                printf("----X DROP ACK(%d)\n", sendDgram.ackNum);
                continue;
            }
        }
        else
        {
            sendDgram.ackNum = expectedDgramSeq;
            sendDgram.windowSize = recv_sw_size;
            strcpy(sendDgram.buf,ACKNOWLEDGEMENT);
            printf("Current Window Size: %d\n", sendDgram.windowSize);
            if(can_send_dtagram(prob_datagram_loss ,rand_seed))
            {
                if(send(sockfd, &sendDgram, dgramLen,flags)<0)
                {
                        printf("ERROR sending data\n");
                        exit(0);
                }
                else
                {
                        printf("----> SEND RESEND ACK(%d)\n",sendDgram.ackNum);
                }
            }
            else
            {
                printf("----X DROP RESEND ACK(%d)\n", sendDgram.ackNum);
            }
            if(recvDgram.EOFFlag)
            {
                stopConsumer=1;
                int i=10;
                while(childFin!=1)
                {
                    Pthread_cond_signal(&cond);
                    sleep(1);
                }
                break;
            }
        }

    }

    pthread_join(pid, NULL);
    return 0;

}

int send_filename_and_get_server_port(char* file_name,int flags, int sockfd)
{
    struct dgram l_sendDgram, l_recvDgram;
    strcpy(l_sendDgram.buf, file_name);
    l_sendDgram.windowSize = recv_sw_size;
    Reliable_Client_Send(sockfd,(void*)(&l_sendDgram),sizeof(l_sendDgram),
                         (void*)(&l_recvDgram),sizeof(l_recvDgram),flags);

    strcpy(ServerEpthPort,l_recvDgram.buf);
    return (atoi(ServerEpthPort));

}

void logger(char* info, char* data)
{
    printf("%s %s\n",info, data);
}

ssize_t
reliable_client_send(int fd, const void *outbuff, size_t outbytes,
			 void *inbuff, size_t inbytes,int flags)
{
    int tsend;
	ssize_t			n;
    int sock_size = sizeof(server);
	if (rttinit == 0) {
		rtt_init(&rttinfo);		/* first time we're called */
		rttinit = 1;
		rtt_d_flag = 1;
	}

	Signal(SIGALRM, sig_alrm);
	rtt_newpack(&rttinfo);		/* initialize for this packet */
	tsend = rtt_ts(&rttinfo);
sendagain:
    if(can_send_dtagram(prob_datagram_loss ,rand_seed))
    {
		if(send(fd, outbuff, outbytes,flags)<0) {
				printf("ERROR sending data\n");
				exit(0);
			}
		else{
				printf("Filename - Data Sent.\n");
		}
    }

    alarm((rtt_start(&rttinfo) /1000) + ((rtt_start(&rttinfo)%1000) /1000));


	if (sigsetjmp(jmpbuf, 1) != 0) {
		if (rtt_timeout(&rttinfo) < 0) {
			err_msg("reliable_client_send: no response from server, giving up");
			rttinit = 0;	/* reinit in case we're called again */
			errno = ETIMEDOUT;
			return(-1);
		}

		goto sendagain;
	}
   // if(can_receive_dtagram(prob_datagram_loss ,rand_seed))
   // {
        if(n=recvfrom(fd, inbuff ,inbytes, flags, (SA*)&server, &sock_size)<0)
        {
            printf("Error Recving Data\n");
            exit(1);
        }
        else
        {
            printf("Server Port - Data recieved\n");
        }
 //   }
	alarm(0);			/* stop SIGALRM timer */
		/* 4calculate & store new RTT estimator values */
	rtt_stop(&rttinfo, rtt_ts(&rttinfo) -  tsend);

	return 1;	/* return size of received datagram */
}

static void
sig_alrm(int signo)
{
	siglongjmp(jmpbuf, 1);
}
/* end dgsendrecv2 */

ssize_t
Reliable_Client_Send(int fd, const void *outbuff, size_t outbytes,
			 void *inbuff, size_t inbytes,int flags)
{
	ssize_t	n;
	n = reliable_client_send(fd, outbuff, outbytes, inbuff, inbytes,flags);
	if (n < 0)
		err_quit("reliable_client_send error");

	return(n);
}

//The client reads the file contents in its receive buffer and prints them
//out on stdout using a separate thread. This thread sits in a repetitive
//loop till all the file contents have been printed out, doing the following.
//
//It samples from an exponential distribution with mean µ milliseconds
//(read from the client.in file), sleeps for that number of milliseconds;
//wakes up to read and print all in-order file contents available in the
//receive buffer at that point; samples again from the exponential
//distribution; sleeps; and so on.
//
//The formula -1 × µ × ln( random( ) ) ,    where ln is the
//natural logarithm, yields variates from an exponential distribution
//with mean µ, based on the uniformly-distributed variates over
//(0,1)  returned by random().


static void consumer()
{
	FILE *fpo;
	char filename[50] = {};
	float rnum=0.0;
    srand(time(NULL));
    sprintf(filename,"%d",time(NULL));
	strcat(filename,"output.txt");
	fpo = fopen(filename,"wt");
	float sleepT=0;
	int index=0;
	printf("*************   Consumer: Started   *************\n");
	for(;;)
	{
        srand48(rand_seed);
        rnum=(double)rand()/(double)RAND_MAX;
        sleepT = (-1)*rate_millisec*(log(rnum));
        //sleepT = (int)(rand())%300;
		printf("\n************* consumer - Sleep (%f us)  *************\n",sleepT);
		usleep(sleepT);
		Pthread_mutex_lock(&buflock);
        Pthread_cond_wait (&cond, &buflock);
		printf("\n************* consumer - Woke up  *************\n");
        while(fileReadWindowCounter < expectedDgramSeq)
        {
            index = fileReadWindowCounter % recv_sw_size;
            printf("%s",window[fileReadWindowCounter % recv_sw_size]->buf);
            fprintf(fpo,"%s", window[fileReadWindowCounter % recv_sw_size]->buf);
            printf("Consumer Read Packet No: %d\n", fileReadWindowCounter);
            window[fileReadWindowCounter % recv_sw_size] = NULL;
            orecv_sw_size++;
            fileReadWindowCounter++;
        }
		Pthread_mutex_unlock(&buflock);

        if(stopConsumer && (fileReadWindowCounter == expectedDgramSeq))
        {
           printf("************* consumer - Finish  *************\n");
           printf("************* File Copied to  = \"%s\"  *************\n",filename);
           childFin = 1;
           break;
        }
	}
	fclose(fpo);
}
