#include    "ping.h"
#include	"unp.h"
#include	"hw_addrs.h"

#include 	<stdlib.h>
#include	<stdio.h>
#include    <linux/if_ether.h>
#include    <linux/if_arp.h>
#include    <linux/if_packet.h>
#include    <setjmp.h>
#include    <sys/utsname.h>



#define RT_PROT_LIN 609
#define TOURNUM 65
#define IPLEN 16


//#define DEBUG

#define TOURLEN 	 398
#define MC_ADDR_LIN   "228.3.2.1"
#define MC_PORT_LIN   "8831"
#define USUNPATH    "g27_lin" 
#define LIN_PORT     6931

struct hwa_info *hwa;
struct utsname myname;
struct sockaddr *sa;

socklen_t salen;

unsigned char dst_mac[ETH_ALEN];
char source_ipaddr_ping[IPLEN];
char dest_ipaddr_ping[IPLEN];
char dest_ip_addr[IPLEN];
struct sockaddr *sasend, *sarecv;

//int send_ping_pf_sockfd;
//ip->id == 531
int     datalen = 56;
static int send_ping_pf_sockfd;
static int source_flag;
//static int first_mulcast_flag = 1;
static int ns_first_flag = 1;
static int last_node_flag = 0;//change it in the recvpacket()
static int first_mulcst_flag = 1;
static int ping_flag = 1;//_is_for_ping
static int rt_recved_flag = 0;
 int ping_over_flag = 0;
static sigjmp_buf jmpbuf;


struct hwaddr {
		     int             sll_ifindex;	 /* Interface number */
		     unsigned short  sll_hatype;	 /* Hardware type */
		     unsigned char   sll_halen;		 /* Length of address */
		     unsigned char   sll_addr[8];	 /* Physical layer address */
};


void
sig_alrm (int signo)
{
	if(last_node_flag == 0&&first_mulcst_flag != 1){
		printf("After waiting 5 seconds, exiting\n");
		exit(0);
	}
	if(last_node_flag == 1&&first_mulcst_flag != 1){
		printf("After waiting 5 seconds, exiting\n");
		exit(0);
	}
again:	
	//printf("ping_over_flag is %d\n", ping_over_flag);
	if(ping_over_flag == 0&&source_flag != 1&&ping_flag != 0){
		//printf("in siglarm and send ping\n");
		send_v4();
		sleep(1);
		goto again;
		//alarm(1);
	}

	if(ping_over_flag == 1)
		return;
	
	if (ping_flag == 0)
	{
		siglongjmp(jmpbuf, 1);
		return;
	}

	if(last_node_flag == 0&&first_mulcst_flag == 1){
		return;
	}
		
	return;
}

void
tv_sub (struct timeval *out, struct timeval *in)
{ /* from book 28-7 */
	if ((out->tv_usec -= in->tv_usec) < 0) {     /* out -= in */
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

void
send_v4 (void)
{	/*Modified 28-14 send_v4()*/
	//strcpy(source_ipaddr_ping, Sock_ntop_host(sa, sizeof(*sa)));

	int     len;
	struct icmp *icmp;
	
	char sd_packet[BUFFSIZE];
	
	struct ip * ip_hdr;
	struct ethhdr *eth_hdr;
	struct sockaddr_ll saddrll;
	//////////////////////////comment it when arp is OK
	//unsigned char dst_mac[ETH_ALEN]
    ////////////////////////////////////////	
	
	/* fill the saddrll struct */
	 saddrll.sll_family 	= PF_PACKET;
	saddrll.sll_halen    	= ETH_ALEN;
	saddrll.sll_ifindex 	= hwa->if_index;
	//memcpy(saddrll.sll_addr, hwa->if_haddr, ETH_ALEN);
	
	//printf("\tMac = %02x:%02x:%02x:%02x:%02x:%02x\n", dst_mac[0],dst_mac[1], dst_mac[2],dst_mac[3], dst_mac[4],dst_mac[5]);
	
	memcpy(saddrll.sll_addr, (void*)dst_mac, ETH_ALEN);
	
	/* saddrll.sll_family   = PF_PACKET;
	saddrll.sll_protocol = htons(USID_PROTO);
	saddrll.sll_hatype   = ARPHRD_ETHER;
	saddrll.sll_ifindex  = 2;
	saddrll.sll_pkttype  = PACKET_OTHERHOST;
	saddrll.sll_halen    = ETH_ALEN;
	saddrll.sll_addr[0]  = dst_mac[0];
	saddrll.sll_addr[1]  = dst_mac[1];
	saddrll.sll_addr[2]  = dst_mac[2];
	saddrll.sll_addr[3]  = dst_mac[3];
	saddrll.sll_addr[4]  = dst_mac[4];
	saddrll.sll_addr[5]  = dst_mac[5];
	saddrll.sll_addr[6]  = 0x00;
	saddrll.sll_addr[7]  = 0x00; */
	
	/* fill the ethernet head struct */
	eth_hdr = (struct ethhdr *) sd_packet;//first part of send packet is ethernet head
	eth_hdr->h_proto = htons(ETH_P_IP);
	memcpy((void*) eth_hdr->h_source, (void*)(hwa->if_haddr), ETH_ALEN);
	memcpy((void*) eth_hdr->h_dest, (void*)dst_mac, ETH_ALEN);//dst_mac is a global variant store the destination mac for ping
	
	/* fill the IP struct */
	ip_hdr = (struct ip *) (sd_packet + ETH_HLEN);//second part of send packet is ip structure
	ip_hdr->ip_hl = 5;
	ip_hdr->ip_v = 4;
	ip_hdr->ip_tos = 0;	//tos should be?
	ip_hdr->ip_len = htons(28 + datalen);
	ip_hdr->ip_id = 0;//here should be 0, when we send a tour we use our id
	ip_hdr->ip_off = 0;
	ip_hdr->ip_ttl = 1;
	ip_hdr->ip_p = IPPROTO_ICMP; //when we send ping we use this, when we send tour packet we use our unique protocol
	ip_hdr->ip_sum = 0;
	ip_hdr->ip_sum = in_cksum ((u_short *) ip_hdr, 20);  
	
	
	//printf("ping source is %s\n", source_ipaddr_ping);
	inet_pton(AF_INET,source_ipaddr_ping, (void*)&(ip_hdr->ip_src));
 	inet_pton(AF_INET,dest_ip_addr, (void*)&(ip_hdr->ip_dst));
	
	/* fill icmp struct */
	icmp = (struct icmp *) (sd_packet + 20 + ETH_HLEN);//third part is the icmp part
	icmp->icmp_type = ICMP_ECHO;
	icmp->icmp_code = 0;
	
	icmp->icmp_id = pid;
	icmp->icmp_seq = nsent++;
	memset (icmp->icmp_data, 0xa5, datalen); /* fill with pattern */
	Gettimeofday ((struct timeval *) icmp->icmp_data, NULL);
	
	len = 8 + datalen;
	icmp->icmp_cksum = 0;
	icmp->icmp_cksum = in_cksum ((u_short *) icmp, len);
	
	//printf("ping source node\n");
	sendto(send_ping_pf_sockfd, sd_packet, 8+ETH_HLEN+20+datalen, 0,(SA *)&saddrll,sizeof(saddrll));
	return;
}

void
proc_v4 (char *ptr, ssize_t len, struct timeval *tvrecv)
{/* Modified the code from book 28-8 */

	int     hlenl, icmplen;
	double  rtt;
	struct ip *ip;
	struct icmp *icmp;
	struct timeval *tvsend;

	ip = (struct ip *) ptr;      /* start of IP header */
	hlenl = ip->ip_hl << 2;      /* length of IP header */
	if (ip->ip_p != IPPROTO_ICMP)
		return;                  /* not ICMP */

	icmp = (struct icmp *) (ptr + hlenl);   /* start of ICMP header */
	if ( (icmplen = len - hlenl) < 8)
		return;                  /* malformed packet */
	
	
	if (icmp->icmp_type == ICMP_ECHOREPLY) {
		
		if (icmp->icmp_id != pid)
			return;                /* not a response to our ECHO_REQUEST */
		if (icmplen < 16)
			return;                /* not enough data to use */
		tvsend = (struct  timeval  *) icmp->icmp_data;
		tv_sub (tvrecv, tvsend);
		rtt = tvrecv->tv_sec * 1000.0 + tvrecv->tv_usec / 1000.0;

		
		
		//////////////////
		//need to change ping dest ip to vm name??
		//////////////////
		
		////////////dest_ipaddr_ping need to change
		printf ("Received %d bytes from %s: seq=%u, ttl=%d, rtt=%.3f ms\n",
		icmplen, dest_ip_addr, icmp->icmp_seq, ip->ip_ttl, rtt);

	}
	//do not have the verbose part
}

static void *
ls_send_mul(void *arg)
{
	int smfd;
	smfd = *((int *)arg);
	Pthread_detach(pthread_self());
	printf("End of the tour wait for 5 secs\n");
	sleep(5);
	ping_over_flag = 1;
	//send multicast
	char buf[MAXLINE];
	char buf2[MAXLINE];
	first_mulcst_flag = 0;
	snprintf(buf, sizeof(buf), "<<<<< This is node %s. Tour has ended. Group members please identify yourselves. >>>>>\n", myname.nodename);
	send_all(smfd, sasend, salen, buf); 

	snprintf(buf2, sizeof(buf2), "<<<<< Node: %s. I am a member of the group>>>>>\n", myname.nodename);
	send_all(smfd, sasend, salen, buf2); 
	fputs("Waiting for 5 seconds and will exit\n", stdout);
	sleep(5);
	fputs("After 5 seconds, exit\n", stdout);
	exit(0);

}

static void *
ping(void *arg)
{
	//char *dest_ip_addr;
	Pthread_detach(pthread_self());
	//printf("in the ping function\n");
	
	int     c; 
	opterr = 0;                  /* don't want getopt() writing to stderr */
	
	struct hwaddr areq_hwa;
	struct sockaddr_in  temp_ipad;
	struct sockaddr *arp_ipad = (struct sockaddr*)&temp_ipad;
	

	pid = getpid() & 0xffff;     /* ICMP ID field is 16 bits */
	

	/* initialize  according to protocol */

	temp_ipad.sin_family= AF_INET;
	inet_pton(AF_INET,dest_ip_addr,(void *)&temp_ipad.sin_addr);
	
	///already move to the main function
	strcpy(source_ipaddr_ping, Sock_ntop_host(sa, sizeof(*sa)));
	/////////////////
	

	printf ("Start to ping %s: (from %s ) %d data bytes\n", dest_ip_addr, source_ipaddr_ping, datalen);

	
	//////////////////////////////////////
	////////recover it when arp is OK
	//////////////////////////////////////
	if(areq(arp_ipad, sizeof(struct sockaddr), &areq_hwa) != 0){
		ping_flag = 1;
		memcpy((void*)dst_mac, (void*)(areq_hwa.sll_addr), ETH_ALEN);	
	}else{
		fputs("Get HW address error, exiting\n", stdout);
		exit(0);
	} 

	sig_alrm (SIGALRM);  

	
}

int 
areq(struct sockaddr *IPaddr,socklen_t sockaddrlen, struct hwaddr *hw_addr){//communicate with arp application
	//printf("in the areq\n");
	printf("AREQ called\n");
	char ip_crp_hw[16];
	struct sockaddr_in *give_to_arp=(struct sockaddr_in *)IPaddr;
	
	int ud_tour_fd;
	ud_tour_fd=socket(AF_LOCAL,SOCK_STREAM,0);

	int hw_len;
	hw_len = sizeof(struct hwaddr);
	
	char get_mac_addr[58];
	struct hwaddr  * mac_ptr = (struct hwaddr *) (get_mac_addr);
	
	int i = IF_HADDR;
	ping_flag = 0;
	
	struct sockaddr_un dest_arp;
	bzero(&dest_arp,sizeof(dest_arp));
	dest_arp.sun_family=AF_LOCAL;
	strcpy(dest_arp.sun_path,USUNPATH);
	connect(ud_tour_fd,(SA *)&dest_arp,sizeof(dest_arp));
	writen(ud_tour_fd,IPaddr,sockaddrlen);
	alarm(3);
	if (sigsetjmp(jmpbuf, 1) != 0) 
	{
		fputs("Timeout for receive MAC address from ARP application, closing the socket\n", stdout);
		close(ud_tour_fd);
		return 0;
	}
	read(ud_tour_fd,get_mac_addr,MAXLINE);
	memcpy((void*)hw_addr, (void*)mac_ptr, hw_len);
	Inet_ntop(AF_INET,&(give_to_arp->sin_addr),ip_crp_hw,16);
	
	printf("Received mac address from ARP: ");
	printf("%02x:%02x:%02x:%02x:%02x:%02x ", mac_ptr->sll_addr[0],mac_ptr->sll_addr[1], mac_ptr->sll_addr[2],mac_ptr->sll_addr[3], mac_ptr->sll_addr[4],mac_ptr->sll_addr[5]);
	printf("  corresponding IP: %s\n",ip_crp_hw);
	alarm(0);
	return 1;
}

void get_vmname(char *source_addr, char *vm)
{
	int key;
	key = atoi(source_addr + 12);
	
	if(key == 1)
		strcpy(vm,"vm1");
	if(key == 2)
		strcpy(vm,"vm1");
	if(key == 3)
		strcpy(vm,"vm1");
	if(key == 4)
		strcpy(vm,"vm1");
	if(key == 5)
		strcpy(vm,"vm1");
	if(key == 6)
		strcpy(vm,"vm1");
	if(key == 7)
		strcpy(vm,"vm1");
	if(key == 8)
		strcpy(vm,"vm1");
	if(key == 9)
		strcpy(vm,"vm1");
	if(key == 10)
		strcpy(vm,"vm1");
}

char * getipbyvm(char *vmname){
	if(strcmp(vmname, "vm1")==0)
		return "192.168.1.101";
	if(strcmp(vmname, "vm2")==0)
		return "192.168.1.102";
	if(strcmp(vmname, "vm3")==0)
		return "192.168.1.103";
	if(strcmp(vmname, "vm4")==0)
		return "192.168.1.104";
	if(strcmp(vmname, "vm5")==0)
		return "192.168.1.105";
	if(strcmp(vmname, "vm6")==0)
		return "192.168.1.106";
	if(strcmp(vmname, "vm7")==0)
		return "192.168.1.107";
	if(strcmp(vmname, "vm8")==0)
		return "192.168.1.108";
	if(strcmp(vmname, "vm9")==0)
		return "192.168.1.109";
	if(strcmp(vmname, "vm10")==0)
		return "192.168.1.110";
}

/* multicast receive */
void
recv_all(int recvfd, socklen_t salen)
{/* modified 21-19 from book*/

	int     n;
	char    line[MAXLINE + 1];
	socklen_t len;
	struct sockaddr *safrom;

	safrom = Malloc(salen);

	len = salen;
	
	alarm(6);
	n = Recvfrom(recvfd, line, MAXLINE, 0, safrom, &len);
	
	line[n] = 0;            /* null terminate */
	printf("Node %s. From %s, Recieved: %s",myname.nodename, Sock_ntop(safrom, len), line);
	
}


/* multicast send */
void
send_all(int sendfd, SA *sadest, socklen_t salen, char line[MAXLINE])
{/* modified 21-18 from book */

	printf("Node %s. Sending:%s",myname.nodename, line);
	Sendto(sendfd, line, strlen(line), 0, sadest, salen);

}


/* route travesal socket send the tour information */
int rt_send(int rtfd, char   tour_array[][IPLEN])
{

	int i, pos;
	struct ip * ip;
	char rt_sd_info[BUFFSIZE];
	ip = (struct ip *) (rt_sd_info);
	
	char rt_source_ip[IPLEN], dst_ip[IPLEN];
	struct sockaddr_in dst_addr;
	
	
	
	bzero(&dst_addr, sizeof(dst_addr));
	dst_addr.sin_family = AF_INET;
	dst_addr.sin_port = htons(LIN_PORT);
	
	
	ip->ip_hl = 5;
	ip->ip_v = 4;
	ip->ip_tos = 0;
	//when we send tour we set the id is 531
	ip->ip_id = 531;
	ip->ip_off = 0;
	ip->ip_ttl = 1;
	ip->ip_len = htons(20 + TOURLEN);
	//when we send tour we set the protocol unique
	//ip->ip_p = RT_PROT_LIN ;
	ip->ip_p = 254 ;

	
	strcpy(rt_source_ip, Sock_ntop_host(sa, sizeof(*sa)));
	inet_pton(AF_INET,rt_source_ip, &(ip->ip_src));
	//memcpy((void*)ip->ip_src,(void*) sa,sizeof(*sa));

	pos = atoi(tour_array[3]);
	printf("next node ip is : %s\n", tour_array[pos]);
	
	inet_pton(AF_INET,tour_array[pos], &(ip->ip_dst));
	//printf("dst %s\n",tour_array[3]);
	inet_pton(AF_INET,tour_array[pos], &dst_addr.sin_addr);

	//printf("We are sending...\n");
	for (i = 0; i < atoi(tour_array[2]); i++)
	{
		//printf("\t%s\n", tour_array[i]);
		memcpy((void*)(rt_sd_info+20+IPLEN*i),(void*)tour_array[i],IPLEN);//skip the first one, cuz its this host's addr
	}
	//memcpy((void*)(rt_sd_info+20+IPLEN*(i-1)),(void*)tour_array[i],IPLEN);
	
	printf("sending tour packet to next node\n");
	Sendto (rtfd, rt_sd_info, 20+IPLEN*i, 0, (SA*)&dst_addr, sizeof(dst_addr));
	return 1;
}

/* route travesal socket receive the tour information */
int 
rt_recv(int rtfd, char tour_array[][IPLEN])
{

	time_t	ticks;
	int i;
	char rt_rv_info[BUFFSIZE];
	struct sockaddr_in recv_ad;
	int r, len;
	
	r = recvfrom(rtfd, rt_rv_info, MAXLINE, 0, NULL, NULL);
	memcpy((void*)tour_array[2], (void*)(rt_rv_info+20+2*IPLEN), IPLEN);//first get the node number
	
	
	//printf("tour number is %d\n", atoi(tour_array[2]));
	for(i = 0;i <atoi(tour_array[2]); i++){
		/* if(i == 2)
			continue; */
		memcpy((void*)tour_array[i], (void*)(rt_rv_info+20+i*IPLEN), IPLEN);
	}
	ticks=time(NULL);
	printf("\n<%.24s> recieved tour packet from <%s>\n",ctime(&ticks), tour_array[atoi(tour_array[3])-1]);
	
	sprintf(tour_array[1],"%d",atoi(tour_array[1])-1);//the number of the tour minus one
	
	if(atoi(tour_array[1]) == 0){//if the vm nodes number equal to 0 that means i am the last node
		last_node_flag = 1;
	}
	                                                     
	sprintf(tour_array[3],"%d",atoi(tour_array[3])+1);//position plus one
	return r;
	//printf("\nTHS: %s %s\n", (rt_rv_info+20),inet_ntoa(recv_ad.sin_addr));
}
