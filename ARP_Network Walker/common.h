/* 
 * common.h
 *
 * Common Header file for both the client and the server
 *
 * 
 *
 */
#ifndef COMMON_C
#define COMMON_C


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <sys/select.h>
#include <pthread.h>

//#include <net/if.h>

#include <sys/ioctl.h>
#include <math.h>
#include <setjmp.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>

//#include <sys/sockio.h>

#define ARP_PATH "g27_lin"


//#define ETH_FRAME_LEN 1518
#define ETH_P_ARP_PROTO 0x4864
#define TIME_SERVER_PORT 7778
#define MAXLINE 4096
#define max(a,b) ((a) > (b) ? (a) : (b))

#endif

/* get_hw_addr requirements */

#define	IF_NAME		16	/* same as IFNAMSIZ    in <net/if.h> */
#define	IF_HADDR	 6	/* same as IFHWADDRLEN in <net/if.h> */
#define MGRP 		531

#define	IP_ALIAS  	 1	/* hwa_addr is an alias */

/* function prototypes */
struct hwa_info	*get_hw_addrs();
struct hwa_info	*Get_hw_addrs();
void	free_hwa_info(struct hwa_info *);
int prhwaddrs();
char *get_my_mac(int );
char *sock_ntop_host(const struct sockaddr *sa, socklen_t salen);


struct hwa_info {
  char    if_name[IF_NAME];	/* interface name, null terminated */
  char    if_haddr[IF_HADDR];	/* hardware address */
  int     if_index;		/* interface index */
  short   ip_alias;		/* 1 if hwa_addr is an alias IP address */
  struct  sockaddr  *ip_addr;	/* IP address */
  struct  hwa_info  *hwa_next;	/* next of these structures */
};

	struct arp_req_res {
   unsigned short id; 
	unsigned short hw_type;
    unsigned short prot_type;
    unsigned char hw_size;
    unsigned char prot_size;
    unsigned short op;
    unsigned char src_mac[6];
    in_addr_t src_ip;
    unsigned char dst_mac[6];
    in_addr_t  dst_ip;
  };    
  
	struct hwaddr {
		int sll_ifindex;
		unsigned short sll_hatype;
		unsigned char sll_halen;
		unsigned char sll_addr[8];	
	};

	struct cache {
	  in_addr_t ipaddr;
    unsigned char mac[8];
    unsigned int sll_ifindex;
    unsigned short sll_hatype;
    int confd;
    struct cache *next;
	};

  struct uxapi {
    struct sockaddr *ipaddr;
    socklen_t len;
    struct hwaddr *hwptr;
  };
	
  struct pending_reply{
    int flag;
	  in_addr_t srcnodeip;
    int confd;
    struct sockaddr cliaddr;
  };




