#include "unp.h"
#include "unpifiplus.h"
#include <stdlib.h>
#include <stdio.h>
#define SERVER_CONFIG "server.in"
#define MAX_LINE_LENGTH 256
#define MAX_IF 10
#define MAX_TO 5
#define LOCALHOST "127.0.0.1"
#define ACKNOWLEDGEMENT "ACK"
#include	"unprtt.h"
#include	<setjmp.h>
#define MAX_WIN_SIZE 4096
/*
  The function to read the server.in file and store the
  values in global vars
*/
int g_ctr=0;
int check=0;
typedef enum
{
    CONN_NOT_EXIST = 0,
    CONN_EXIST = 1,
    CONN_CREATED = 2
} ConnType;

unsigned int
alarm (unsigned int useconds)
{
    struct itimerval old, new;
    new.it_interval.tv_usec = 0;
    new.it_interval.tv_sec = 0;
    new.it_value.tv_usec = (long int) useconds;
    new.it_value.tv_sec = 0;
    if (setitimer (ITIMER_REAL, &new, &old) < 0)
        return 0;
    else
        return old.it_value.tv_sec;
}
static struct rtt_info   rttinfo;
static int	rttinit = 0;
static struct msghdr	msgsend, msgrecv;	/* assumed init to 0 */
static int jmp_flag = 0;
static void	sig_alrm(int signo);
static sigjmp_buf first_jmpbuf, second_jmpbuf;
 static int persist = 0;
int port;
int mssw;
int operatingWindow=0;
int if_counter;
pthread_mutex_t	tracker_lock = PTHREAD_MUTEX_INITIALIZER;
char IPServer[INET_ADDRSTRLEN];
char IPClient[INET_ADDRSTRLEN];
int   pipeFD[2];
//static int bufLen = 512 - (5*sizeof(int)) - 1;
typedef struct dgram
{
    int seqNum;
    int ackNum;
    int EOFFlag;
    int windowSize;
    int datasize;
    char buf[512 - (5*sizeof(int))];
} dgram;
#define BUFLEN 491
dgram recvDgram;
dgram sendDgram;
dgram ackDgram;
int dgramLen = sizeof(dgram);
struct dgram* window[MAX_WIN_SIZE];

typedef struct tracker
{
    int id;
    char track_ip[INET_ADDRSTRLEN];
    int track_port;
    struct tracker *next;
} tracker_st;

tracker_st *tracker_begin, *tracker_end;

typedef struct sock_array
{
    int sockfd;
    char ip_address[INET_ADDRSTRLEN]; //IP address
    char subnet_mask[INET_ADDRSTRLEN]; //network mask
    char host_network[INET_ADDRSTRLEN]; //bitwise and between the IP address and its network mask)
} sock_array_st;

/* The wrapper to read line */

int readLine(char *line,FILE *file)
{
    if(fgets(line, MAX_LINE_LENGTH, file) == NULL)
        return 0;
    else
        return 1;
}
/* Read the server config file */
void logger(char* info, char* data);
int delete_connection(int client_port);
int check_and_add_conn( int client_port);

int NegotiateWindows(int windowSize,int mssw)
{
    if(windowSize < mssw)
    {
        return windowSize;
    }
    else
    {
        return mssw;
    }

}
int fillWindowfromFile(FILE *filename)
{
    char Buffer[BUFLEN]= {0};
    int EOFFlag=0;
    int seq=0;
    int i=0;
    char cin;
    while(1)
    {
        memset(Buffer,0,BUFLEN);
        for(i = 0; i < BUFLEN; i++)
        {
            cin = fgetc(filename);
            if(cin == EOF)
            {
                EOFFlag = 1;
                break;
            }
            Buffer[i] = cin;
            //printf("---%c%d\n",cin,i);

        }
        Buffer[i] = '\0';
        struct dgram * blob = (struct dgram*)malloc(sizeof (struct dgram));
        memset(blob, 0,sizeof (struct dgram));
        strcpy(blob->buf,Buffer);
        // printf("---%s\n",Buffer);
        blob->seqNum=seq++;
        blob->windowSize=operatingWindow;
        blob->datasize=i;
        window[seq-1] = blob;
        if(EOFFlag)
        {
            blob->EOFFlag=1;
            break;
        }
        else
        {
            blob->EOFFlag=0;
        }
    }
    printf("Read File into Buffer. Total blocks = %d \n", seq);
    return seq;
}

int readConfig()
{
    char line[MAX_LINE_LENGTH] = {0};
    int linenum=0;
    FILE *file = fopen (SERVER_CONFIG, "r" );
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
            if(sscanf(line, "%d", &port) != 1)
            {
                printf("Syntax error, line %d\n", linenum);
                ret = 0;
                break;
            }
            printf("Line %d: Well-known port number for server is %d \n", linenum, port);
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
            if(sscanf(line, "%d", &mssw) != 1)
            {
                printf("Syntax error, line %d\n", linenum);
                ret = 0;
                break;
            }
            printf("Line %d: Maximum sending sliding-window size is %d \n", linenum, mssw);
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

void establishCommunication(int connection, char* filename, int flag, struct sockaddr_in* cli, int len)
{
    uint32_t ts, tr;
    FILE *fp;
    int length = len;
    int maxfail=0;


    // Open File
    fp = fopen(filename,"r");
    if(fp == NULL)
    {
        strcpy(sendDgram.buf,"File does not exist\n");
        if(send(connection, &sendDgram, dgramLen,flag)<0)
        {
            printf("ERROR >> %d",errno);
        }
        //Sendto(connection, &sendDgram, dgramLen, flag,(SA*)cli,length);
        //printf("Send >> %s",sendDgram.buf);
        memset(sendDgram.buf,0,MAXLINE);

        if(recvfrom(connection, &ackDgram, dgramLen,0,(SA*)cli,&length)<0)
        {
            if(maxfail > 10)
            {
                printf("Exiting, CLIENT DEAD !! \n");
                return;
            }
            maxfail++;
            printf("Error in recvfrom() in Server\n%d",errno);
        }
        if(strcmp(ackDgram.buf,ACKNOWLEDGEMENT)==0)
        {
            printf("Rec << ACK(%d), Window Size(%d)\n",ackDgram.ackNum,ackDgram.windowSize);
        }
    }
    else
    {
        int total = fillWindowfromFile(fp);
        int count = 0;
        int shift=0;
        int ii=0;
resend:
        while(count < total)
        {
            dgram* ptr = window[count];
            if(send(connection, ptr, dgramLen,flag)<0)
            {
                printf("ERROR >> %d\n",errno);
            }
            printf("\n----------------------PACKET------------------------\n");

            jmp_flag = 1;
            ts = rtt_ts(&rttinfo);
            if(persist)
            {
                printf("PROBE  >>\n");
            }
            else{
                 printf("Send (%d)/(%d), TO(%d) >>\n",ptr->seqNum,total,ts);
            }
            alarm(ts);
            while(1)
            {
                if (sigsetjmp(second_jmpbuf, 1) != 0)
                {
                    shift++;
                    printf(" retry %d \n",shift);
                    goto resend;
                }
                shift=0;
                if(recvfrom(connection, &ackDgram, dgramLen,0,(SA*)cli,&length)<0)
                {
                    if(maxfail > 10)
                    {
                        printf("Exiting, CLIENT DEAD !! \n");
                        return;
                    }
                    maxfail++;
                    printf("Error in recvfrom() in Server = %d\n",errno);
                }

                if(strcmp(ackDgram.buf,ACKNOWLEDGEMENT)==0)
                {
                    printf("Rec << ACK(%d), Recv Window Size(%d)\n",ackDgram.ackNum,ackDgram.windowSize);
                    count = ackDgram.ackNum;
                    if(ackDgram.windowSize == 0)
                    {
                        printf("---------------PERSIST MODE ENTERED--------------------\n");
                        persist++;
                    }
                    else
                    {
                        persist = 0;
                    }
                    break;
                }
                else
                {
                    printf("UNEXPECTED Data from Client\n");
                }
            }
            tr = rtt_ts(&rttinfo);
            rtt_stop(&rttinfo, (tr - ts));
        }
    }
    return;
}

void signal_handler(int signo)
{
    pid_t	pid;
    int		stat;
    int bytesRead=0;
    char  *buf = (char*)malloc(INET_ADDRSTRLEN);
    while(1)
    {
        memset(buf,0,sizeof(buf));
        if((bytesRead = read(pipeFD[0], buf, INET_ADDRSTRLEN)) < 0)
        {
            // Handle slow system calls that can be interrupted
            if (errno == EINTR)
                continue;
            else
                break;
        }
        else if( bytesRead == 0)
        {
            printf("CLIENT:: Closed connection\n");
            break;
        }
        break;
    }
    printf("Last Packet sent successfully: Need to delete %d ID from tracker\n",atoi(buf));
    delete_connection(atoi(buf));
    free(buf);
    while((pid=waitpid(-1,&stat,WNOHANG)>0))
    {
        printf("Server Child %d Terminated \n",pid);
    }
    return;
}


int main()
{
    logger("","");
    if(readConfig() != 1)
    {
        printf("Error in Config File\n");
        exit(0);
    }
    tracker_begin = NULL;
    tracker_end = NULL;
    struct ifi_info	*ifi, *ifihead;
    struct sockaddr_in	*sa_ip, *sa_mask, sa_subnet;
    u_char	*ptr;
    int	i, family, doaliases;
    sock_array_st sock_array[MAX_IF];

    family = AF_INET;
    doaliases = 1;

    if_counter=0;
    int on=1;

    printf("\n----------UNICAST ADDRESS-----------\n");
    for (ifihead = ifi = Get_ifi_info_plus(family, doaliases);
            (ifi != NULL) && (if_counter < MAX_IF) ; ifi = ifi->ifi_next)
    {
        // Create the socket
        int sockfd = Socket(AF_INET,SOCK_DGRAM,0);
        char ip_address[INET_ADDRSTRLEN] = {0};
        char subnet_mask[INET_ADDRSTRLEN] = {0};
        char host_network[INET_ADDRSTRLEN] = {0};

        Setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
        // Extract and fill information from the interfaces to the array
        // using internals of sock_ntop_host.c
        // As we are only using , ifi_addr, we take care of unicast
        sa_ip = (struct sockaddr_in*) (ifi->ifi_addr);
        if (sa_ip != NULL)
        {
            sa_ip->sin_family =AF_INET;
            sa_ip->sin_port = htons(port);
            // Bind the unicast IP
            Bind(sockfd, (SA*) sa_ip, sizeof(*sa_ip));
            Inet_ntop(AF_INET,&sa_ip->sin_addr,ip_address, sizeof(ip_address));
        }
        else
        {
            logger("ERROR:", "sa_ip is NULL");
            continue;
        }

        sa_mask = (struct sockaddr_in*) (ifi->ifi_ntmaddr);
        if (sa_mask != NULL)
        {
            Inet_ntop(AF_INET,&sa_mask->sin_addr,subnet_mask, sizeof(subnet_mask));
            //strcpy(subnet_mask),  Sock_ntop_host(sa_mask, sizeof(*sa_mask)));
        }
        else
        {
            logger("ERROR:", "sa_mask is NULL");
            continue;
        }
        unsigned long subnet_no = 0;
        if ((sa_ip != NULL) && (sa_mask != NULL))
        {
            subnet_no = (sa_ip->sin_addr.s_addr) & (sa_mask->sin_addr.s_addr);
        }
        else
        {
            logger("ERROR:", "sa_ip or sa_mask is NULL");
            continue;
        }

        if (sa_mask != NULL)
        {
            sa_subnet.sin_addr.s_addr = subnet_no;
            Inet_ntop(AF_INET,&(sa_subnet.sin_addr),host_network, sizeof(host_network));
        }
        else
        {
            logger("ERROR:", "sa_subnet is NULL");
            continue;
        }
        sock_array[if_counter].sockfd = sockfd;
        strcpy(sock_array[if_counter].ip_address,ip_address);
        strcpy(sock_array[if_counter].subnet_mask,subnet_mask);
        strcpy(sock_array[if_counter].host_network,host_network);

        printf("-------------%d------------\n",if_counter+1);
        logger("IP address:", sock_array[if_counter].ip_address);
        logger("Subnet Mast:",sock_array[if_counter].subnet_mask);
        logger("Host Network:",sock_array[if_counter].host_network);
        if_counter++;
    }
    printf("--------------------------\n\n");
    free_ifi_info_plus(ifihead);
    fd_set	allset;
    int max_fd=0, ret;
    int validID=0;
    // Get the max FD
#ifdef DBG
    printf("if_counter is %d\n", if_counter);
#endif
    Signal(SIGCHLD,signal_handler);
    if (rttinit == 0)
    {
        rtt_init(&rttinfo);		/* first time we're called */
        rttinit = 1;
    }
    Signal(SIGALRM, sig_alrm);

    rtt_newpack(&rttinfo);		/* initialize for this packet */
    printf("\n---------- Waiting for Clients -----------\n");
    for ( ; ; )
    {
        FD_ZERO(&allset);
        max_fd=0;
        for (i=0; i<if_counter; i++)
        {
            FD_SET(sock_array[i].sockfd, &allset);
            if (max_fd < sock_array[i].sockfd)
            {
                max_fd = sock_array[i].sockfd;
            }
        }
        if ( (ret = select(max_fd+1, &allset, NULL, NULL, NULL)) < 0)
        {
            if(errno==EINTR)
                continue;
            else
                break;
        }
        for (i=0; i<if_counter; i++)
        {
            if (FD_ISSET(sock_array[i].sockfd, &allset))
            {
                struct dgram l_recvDgram;
                // Check if the connection from same client already exists
#ifdef DBG
                printf("DBG Parent Request recieved on %d\n",sock_array[i].sockfd);
#endif
                char filename[MAXLINE + 1] = {0};
                struct sockaddr_in client_addr;
                int rec = 0;
                int client_port = 0;
                int length = sizeof(struct sockaddr_in);
                rec = recvfrom(sock_array[i].sockfd, &l_recvDgram, sizeof(l_recvDgram),0,(SA*)&client_addr,&length);
                if(rec<0)
                {
                    printf("Error in recv\n");
                    continue;
                }
                else
                {
                    strcpy(IPServer, sock_array[i].ip_address);
                    printf("\n\nRecieved Request:\n Server IP is %s \n Server Port is %d\n",IPServer,port);
                    Inet_ntop(AF_INET,&client_addr.sin_addr,IPClient, sizeof(IPClient));
                    client_port = ntohs(client_addr.sin_port);
                    printf(" Client IP is %s \n",IPClient);
                    printf(" Client Epth Port is %d \n",client_port);
                    strcpy(filename, l_recvDgram.buf);
                    printf(" File Requested is: \"%s\" \n",filename);
                    printf(" Reciever Window recvd is: \"%d\" \n",l_recvDgram.windowSize);

                    operatingWindow = NegotiateWindows(l_recvDgram.windowSize,mssw);
                    printf(" Current Operating Windwo Negotiate is : \"%d\" \n",operatingWindow);
                }

                // Call tracker to check the new connection
                if(check_and_add_conn(client_port) == 1)
                {
                    // Connection already exists
                    // Do nothing
                    printf(" A Connection with same IP/Port exists.  Drop Packet\n");
                    continue;
                }
                else
                {
                    //createChild(i,if_counter,&client_addr,sock_array);
                    int childpid=0;
                    int l_ctr = g_ctr;
                    char trackerMsg[INET_ADDRSTRLEN]= {0};
                    //printf("Data recieved on %d\n",sock_array[conn].sockfd);
                    //printf("Address of sock_array Parent is %u\n",&sock_array);
                    if (pipe(pipeFD)== -1)
                    {
                        printf("CLIENT:: Pipe creation failed\n");
                        exit(1);
                    }
                    if ((childpid = fork()) == 0)
                    {
                        //printf("Address of sock_array Child is %u\n",&sock_array
                        close(pipeFD[0]);
                        char writeFD[10];
                        int x=0;
                        int connection = 0;
                        int listening=0;
                        int flag = 0;
                        unsigned long netMask;
                        char network_mask_server[INET_ADDRSTRLEN];
                        char network_mask_client[INET_ADDRSTRLEN];
                        struct in_addr netMaskS;
                        struct in_addr netMaskC = client_addr.sin_addr;
                        struct sockaddr_in cin;
                        int length=sizeof(cin);
                        // Close all other sockets
                        // Step 6:
                        for(x=0; x<if_counter; x++)
                        {
                            if(x == i)
                            {
                                listening=sock_array[x].sockfd;
#ifdef DBG
                                printf("DBG Child NOT Close %d\n",sock_array[x].sockfd);
#endif
                                continue;
                            }
                            else
                            {
#ifdef DBG
                                printf("DBG Child Close %d\n",sock_array[x].sockfd);
#endif
                                close(sock_array[x].sockfd);
                            }
                        }

                        strcpy(network_mask_server, sock_array[i].subnet_mask);
                        Inet_pton(AF_INET, network_mask_server, &netMaskS);

                        netMask = (netMaskC.s_addr) & (netMaskS.s_addr);
                        Inet_ntop(AF_INET, (struct in_addr*)&netMask, network_mask_client, sizeof(network_mask_client));
                        printf(" Client NetMask = %s, \n Server NetMask = %s\n",network_mask_client,sock_array[i].host_network);

                        if((strcmp (IPServer, LOCALHOST) == 0))
                        {
                            printf(" \n #### Client is LOCAL - Loopback #### \n");
                            flag  = MSG_DONTROUTE;
                        }
                        else if (strcmp (network_mask_client, sock_array[i].host_network) == 0)
                        {
                            // Using the array of structures built
                            // together with the addresses IPserver and IPclient
                            // to determine if the client is ‘local’.
                            printf(" \n #### Client is LOCAL - Same Subnet #### \n");
                            flag  = MSG_DONTROUTE;
                        }
                        else
                        {
                            printf(" \n ####  Client is NOT LOCAL, use MSG_DONTROUTE. #### \n");
                        }

                        // Step 7:
                        // The server (child) creates a UDP socket to handle file
                        // transfer to the client. Call this socket the ‘connection’ socket.
                        // It binds the socket to IPserver, with port number 0 so that
                        // its kernel assigns an ephemeral port.
                        connection=Socket(AF_INET,SOCK_DGRAM,0);
                        //printf("Connection FD is %d",connection);
                        cin.sin_family=AF_INET;
                        cin.sin_port=htons(0);

                        Inet_pton(AF_INET, IPServer, &cin.sin_addr);
                        if(bind(connection, (SA*)&cin, sizeof(cin)) < 0)
                        {
                            printf("BIND ERROR in Child - %d\n",errno);
                            exit(1);
                        }
                        if(getsockname(connection, (SA*)&cin, &length) < 0)
                        {
                            printf("getsockname ERROR - %d", errno);
                        }
                        if(connect(connection, (SA*)&client_addr, length) < 0)
                        {
                            printf("CONNECT ERROR \n");
                        }
                        printf("\nServer-Client Connection established\n - Server IP Address is %s, \n - Server Epth Port is %d\n",
                               Inet_ntop(AF_INET,&(cin.sin_addr),IPServer,sizeof(IPServer)),
                               ntohs(cin.sin_port));
                        printf(" - Client IP Address is %s \n",IPClient);
                        printf(" - Client Epth Port is %d \n",client_port);

                        printf("\n\n",client_port);
                        char* port_string = (char*) malloc(8);
                        memset(port_string,0,8);
                        sprintf(port_string, "%d", ntohs(cin.sin_port));

                        // The server now sends the client a datagram, in which it
                        // passes it the ephemeral port number of its ‘connection’
                        // socket as the data payload of the datagram. This datagram
                        // is sent using the ‘listening’ socket inherited from its parent,
                        // otherwise the client (whose socket is connected to the
                        // server’s ‘listening’ socket at the latter’s well-known port
                        // number) will reject it.

                        // Use ARQ Api Here - TBD
                        //Sendto(listening, port_string, strlen(port_string),flag,(SA*)&client_addr,length);
                        send_port_to_client(port_string,flag,listening,connection,(SA*)&client_addr,length);
                        // Now we have to play with connection and listening sockets
                        // only. We will do a select loop on connection for incoming
                        // request from clients
                        int ret=0;
                        fd_set	rset;
                        struct timeval	tv;
                        char ack_buf[4]= {0};
                        FD_ZERO(&rset);
                        FD_SET(connection, &rset);
                        tv.tv_sec = MAX_TO;
                        tv.tv_usec = 0;
                        while(1)
                        {
                            FD_ZERO(&rset);
                            FD_SET(connection, &rset);
                            if (select(connection +1, &rset, NULL, NULL, &tv) < 0)
                            {
                                printf("SERVER:: Select in child encountered error\n");
                            }
                            //printf("Max FD is %d\n",connection +1);

                            if (FD_ISSET(connection, &rset))
                            {
                                // Recieve ACKNOWLEDGEMENT from client
                                int rec_len=0;
                                if((rec_len = recvfrom(connection, ack_buf, sizeof(ack_buf),0,(SA*)&client_addr,&length))<0)
                                {
                                    printf("SERVER:: No Data recieved()\n%d",errno);
                                }
                                else if(strcmp(ack_buf,ACKNOWLEDGEMENT) == 0)
                                {
                                    printf("SERVER:: Received Acknowledgement, Initiate Data transfer \n\n");
                                    // Close the listening Socket
                                    close(listening);
                                    establishCommunication(connection,filename,flag,&client_addr,length);
                                    break;
                                }
                            }
                            else
                            {
                                // Timeout
                                printf("SERVER:: Handshake - Select Timeout \n\n");
                                Sendto(listening, port_string, strlen(port_string),flag,(SA*)&client_addr,length);
                                Sendto(connection,port_string, strlen(port_string),flag,(SA*)&client_addr,length);
                            }
                        }
                        // Done, Now tell the parent to delete the entry from Tracker
                        // Done, Now tell the parent to delete the entry from Tracker

                        memset(trackerMsg,0,INET_ADDRSTRLEN);
                        sprintf(trackerMsg,"%d",l_ctr);
#ifdef DBG
                        printf("DEL %s\n",trackerMsg);
#endif
                        if(write(pipeFD[1],trackerMsg,INET_ADDRSTRLEN)<0)
                        {
                            printf("Error in writiing to pipe\n");
                        }
                        exit(0);
                    }
                    else
                    {
                        close(pipeFD[1]);
#ifdef DBG
                        printf("SERVER:: returned in parent\n");
#endif
                    }
                }
            }
        }
    }
    if(ret < 0)
    {
        printf("SERVER:: Select failure encountered, %d\n",errno);
    }
    return 0;
}

int check_and_add_conn( int client_port)
{
    Pthread_mutex_lock(&tracker_lock);
    tracker_st *tmp = NULL;
    if(tracker_begin == NULL)
    {
        // No connection exists
        // create a new and add
        tracker_st* entry = (tracker_st*)malloc(sizeof(tracker_st));
        entry->track_port = client_port;
        g_ctr++;
        entry->id=g_ctr;
#ifdef DBG
        printf("Added  ID %d\n",entry->id);
#endif
        strcpy(entry->track_ip,IPClient);
        entry->next = NULL;
        tracker_begin = entry;
        tracker_end = entry;
    }
    else
    {
        tmp = tracker_begin;
        int count=0;
        while(tmp != NULL)
        {
            if((tmp->track_port == client_port)
                    && (strcmp(tmp->track_ip, IPClient) == 0))
            {
                Pthread_mutex_unlock(&tracker_lock);
                return CONN_EXIST;
            }
            count++;
            tmp = tmp->next;
        }
        tracker_st* entry = (tracker_st*)malloc(sizeof(tracker_st));
        entry->track_port = client_port;
        g_ctr++;
        entry->id=g_ctr;
#ifdef DBG
        printf("Added ID %d at pos%d\n",entry->id,count);
#endif
        strcpy(entry->track_ip,IPClient);
        entry->next = NULL;
        tracker_end->next=entry;
        tracker_end=entry;
    }
    Pthread_mutex_unlock(&tracker_lock);
}

int delete_connection(int id)
{
    Pthread_mutex_lock(&tracker_lock);
    tracker_st *tmp = tracker_begin, *prev;
    int marker=0;
    //printf("%s++%d",IPC,cl_port);
#ifdef DBG
    printf("look for %d\n",id);
#endif
    while(tmp!=NULL)
    {
#ifdef DBG
        printf("Present %d\n",tmp->id);
#endif
        if(tmp->id == id)
        {
            marker=1;
#ifdef DBG
            printf("Found %d\n",id);
#endif
            break;
        }
        tmp = tmp->next;
    }

    if(tmp == NULL)
    {
        printf("Error in Linked list!\n");
    }
    else
    {
        tracker_st *tmp1 = tracker_begin;
        if(tmp1->next == NULL)
        {
            tracker_begin = NULL;
            printf("Entry deleted from Tracker - IP=%s, Port=%d, ID=%d\n",tmp->track_ip, tmp->track_port, tmp->id);
            free(tmp1);
        }
        else
        {
            while(tmp1!=tmp)
            {
                prev = tmp1;
                tmp1=tmp1->next;
            }
            prev->next = tmp->next;
            printf("Entry deleted from Tracker - IP=%s, Port=%d, ID=%d\n",tmp->track_ip, tmp->track_port, tmp->id);
            free(tmp);
        }

    }
    Pthread_mutex_unlock(&tracker_lock);
}

void logger(char* info, char* data)
{
    printf("%s %s\n",info, data);
}


int send_port_to_client(char* port,int flags, int listening, int connection,const SA *destaddr, socklen_t destlen)
{
    struct dgram l_sendDgram, l_recvDgram;
    strcpy(l_sendDgram.buf, port);
    Reliable_Server_Port_Send(listening,connection, (void*)(&l_sendDgram),sizeof(l_sendDgram),
                              (void*)(&l_recvDgram),sizeof(l_sendDgram),flags,
                              destaddr,destlen);
}

ssize_t
reliable_server_port_send(int listening, int connection, const void *outbuff, size_t outbytes,
                          void *inbuff, size_t inbytes,int flags,const SA *destaddr, socklen_t destlen)
{
    int tsend;
    ssize_t			n;
    int sock_size = destlen;

sendagain:
    tsend = rtt_ts(&rttinfo);//in ms
    Sendto(listening, outbuff, outbytes ,flags,destaddr,sock_size);
    if(errno == ETIMEDOUT)
    {
        printf("Sendto ETIMEDOUT\n");
        Sendto(connection, outbuff, outbytes, flags,destaddr,sock_size);
        printf("Sendto ETIMEDOUT - Done\n");
    }
    alarm(rtt_start(&rttinfo));	/* calc timeout value & start timer */

    jmp_flag = 0;
    if (sigsetjmp(first_jmpbuf, 1) != 0)
    {
        if (rtt_timeout(&rttinfo) < 0)
        {
            printf("reliable_server_port_send: no response from server\n");
            rttinit = 0;
            errno = ETIMEDOUT;
            return(-1);
        }

    }
    alarm(0);
    rtt_stop(&rttinfo, rtt_ts(&rttinfo) - tsend);
    return 1;
}


static void sig_alrm(int signo)
{
    switch(jmp_flag)
    {
    case 0:
        siglongjmp(first_jmpbuf,1);
        break;

    case 1:
        siglongjmp(second_jmpbuf,1);
        break;

    default:
        break;
    }
}

ssize_t
Reliable_Server_Port_Send(int listening, int connection, const void *outbuff, size_t outbytes,
                          void *inbuff, size_t inbytes,int flags,const SA *destaddr, socklen_t destlen)
{
    ssize_t	n;
    n = reliable_server_port_send(listening,connection, outbuff, outbytes, inbuff, inbytes,
                                  flags,destaddr,destlen);
    if (n < 0)
        err_quit("reliable_client_send error");

    return(n);
}
