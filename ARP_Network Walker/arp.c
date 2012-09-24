#include "unp.h"
#include "common.h"


struct cache *cache_head = NULL;
struct cache *cacheptr;


/* ---------------------------------------------------------------------
Function checks for an entry and returns a pointer to the entry if present 
OR returns NULL when entry is not present
.......................................................................*/

struct cache  *mac_lookup(in_addr_t ipaddr ){
  if (cache_head == NULL)
    return NULL;
  for(cacheptr = cache_head; cacheptr != NULL; cacheptr = cacheptr -> next){
    if(cacheptr -> ipaddr == ipaddr)
      return cacheptr;
  }
  return NULL;
}

/* ----------------------------------------------
Function to Print ARP Cache 
............................................... */
void print_cache(){
  struct cache *cptr;
  struct in_addr Ipad;
  printf("\tIP HW address cache\n");
  printf("\n   IPaddress             Macaddr            \n");
  printf("-----------------------------------------------\n");
  for(cptr = cache_head; cptr != NULL; cptr = cptr -> next){
    Ipad.s_addr = cptr -> ipaddr;
    printf("%s        ", inet_ntoa( Ipad));
    printf("%02x:%02x:%02x:%02x:%02x:%02x       \n", cptr-> mac[0], cptr-> mac[1], cptr-> mac[2], cptr-> mac[3], cptr-> mac[4], cptr-> mac[5]);
 
  printf("------------------------------------------------\n");
  }
}



/* ---------------------------------------------------------------------
Function creates an entry in chace if the IP Address has no previous entry
.......................................................................*/
void cache_update(in_addr_t ipaddr, int ifindex, unsigned char mac[], int confd){
  int i;
  cacheptr = mac_lookup(ipaddr );
  if( cacheptr == NULL){/* case of no entry in cache */
		cacheptr = malloc(sizeof(struct cache));
		bzero( cacheptr,sizeof( struct cache));
		cacheptr -> ipaddr = ipaddr;
    for(i=0; i < 6; i++)
      cacheptr -> mac[i] = mac[i];
    cacheptr -> sll_ifindex = ifindex;
    cacheptr -> sll_hatype = 1;  
    cacheptr -> confd = confd;
    cacheptr -> next = cache_head;
    cache_head = cacheptr;
  }
}




/* *********************************************************************** */

	

int main()
{

	in_addr_t myip, ip_addr,pendip;
	unsigned char mymac[6], src_mac[6], dst_mac[6], mac[6];
	unsigned int my_ifindex;


	struct hwa_info *hwa, *hwahead;
	struct hwaddr hwdetail, *hwptr1;
	struct hwaddr *hwptr = & hwdetail;

	struct sockaddr *srcip;
	struct sockaddr_in *recvptr;
	int pfsocket, listenfd, confd;
	struct sockaddr_ll pfaddr;
	struct sockaddr_ll *pfaddr_ptr = &pfaddr;
	struct sockaddr_un arpaddr, cliaddr;
	struct in_addr Ipad;
	struct in_addr in_addr_temp1, in_addr_temp2, in_addr_temp3;

	struct arp_req_res arpreq, arpres;
	struct arp_req_res *arpreq_ptr, *arpres_ptr, *arpptr;
	struct pending_reply *pendhead, *ptrpend;
	pendhead = ptrpend = NULL;

	fd_set rset;
	int maxfd, i, n,flag=0;
	socklen_t len;

	void *buffer=(void*)malloc(ETH_FRAME_LEN);
	char *recv_buf=(char*)malloc(ETH_FRAME_LEN);
	unsigned char *etherhead=buffer;
	struct ethhdr *eh=(struct ethhdr*)etherhead;
	char sendbuf[512], recvline[512];


	//printf("--------------myip = %d --------------\n", myip);

	pfsocket = Socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP_PROTO));

	for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {
		if(strncmp(hwa->if_name,"eth0",4) == 0){
			srcip = hwa->ip_addr;
			struct sockaddr_in *sin = (struct sockaddr_in *)srcip;
			myip = sin -> sin_addr.s_addr;
			my_ifindex = hwa -> if_index;
			for(i=0; i < 6; i++)
			mymac[i] = hwa -> if_haddr[i];


			pfaddr.sll_family = AF_PACKET;
			pfaddr.sll_protocol = htons(ETH_P_ARP_PROTO);
			pfaddr.sll_ifindex = hwa->if_index;
			  //printf("Name : %s\n",hwa->if_name);
			Bind(pfsocket, (struct sockaddr*)&pfaddr, sizeof(pfaddr));
			break;
			}
	}

	prhwaddrs();
	free_hwa_info(hwahead);

	/* create an entry for self in the cache and add it at the head*/
	cache_update(myip, my_ifindex, mymac, -1);
	print_cache();
	/* prepare ARP Request */
	arpreq_ptr = &arpreq;
	arpreq_ptr -> id = 531;
	arpreq_ptr -> hw_type = 1;
	arpreq_ptr -> prot_type = 1842;
	arpreq_ptr -> hw_size = 6;
	arpreq_ptr -> prot_size = 4;
	arpreq_ptr -> op = 1; /* 1 for REQ */
	for(i=0; i < 6; i++)
	  arpreq_ptr -> src_mac[i] = mymac[i];
	arpreq_ptr -> src_ip = myip;
	for(i=0; i < 6; i++)
	  arpreq_ptr -> dst_mac[i] = 0xff; /* dest mac is 0xffffffffffff for broadcast */


	/* prepare ARP Response */
	arpres_ptr = &arpres;
	arpres_ptr -> id = 531;
	arpres_ptr -> hw_type = 1;
	arpres_ptr -> prot_type = 1842;
	arpres_ptr -> hw_size = 6;
	arpres_ptr -> prot_size = 4;
	arpres_ptr -> op = 2; /* 2 for RESP */
	for(i=0; i < 6; i++)


	/* Create unix socket */
	listenfd = Socket(AF_LOCAL, SOCK_STREAM, 0);
	unlink(ARP_PATH);
	bzero(&arpaddr, sizeof(arpaddr));
	arpaddr.sun_family = AF_LOCAL;
	strcpy(arpaddr.sun_path, ARP_PATH);
	
	Bind(listenfd, (struct sockaddr*)&arpaddr, sizeof(arpaddr));
	Listen(listenfd, LISTENQ);

	/*************** debug code *************/
	//printf("--------------myip = %x --------------\n", myip);
	//printf("ntoh myip = %x \n", ntohl((uint32_t)myip));


	Ipad.s_addr = myip;
	printf("myip is %s \n", inet_ntoa( Ipad));
	printf("myIFindex = %d \n", my_ifindex);
	printf("mymac =  ");
	for(i = 0; i<6; i++)
	  printf("%02x:", mymac[i]);
	printf("\n");
	/*************** debug code *************/

	while(1){      
		FD_ZERO(&rset);
		FD_SET(listenfd, &rset);
		FD_SET(pfsocket, &rset);
		maxfd = max(listenfd, pfsocket);
      	//printf("Waiting in select\n");
		select(maxfd+1,&rset,NULL,NULL,NULL);

		if(FD_ISSET(listenfd, &rset)){/* unix socket read*/
		  
			confd = Accept(listenfd, (struct sockaddr *)&cliaddr, &len); 
			len = sizeof(cliaddr);
			bzero(recvline, 512);
			Recvfrom(confd, recvline, 512, 0,(struct sockaddr *)&cliaddr, &len);
			printf("\n---------------Hardware details request received from tour client-----------------\n");
			printf("confd = %d \n", confd);
			printf("H/W details requsted for ");
			recvptr = (struct sockaddr_in*) recvline;
			ip_addr = recvptr -> sin_addr.s_addr;			
			Ipad.s_addr = ip_addr;
			printf("IPaddres = %s \n", inet_ntoa( Ipad));
			printf(".....................................................................................\n\n"); 

			cacheptr = mac_lookup(ip_addr);

			if(cacheptr != NULL){/* cache has entry for this IPaddress. Send HW details on the UXsocket */
				struct uxapi *sndptr, *rptr;
				bzero(sendbuf,512);
		//        sndptr = (struct uxapi *)sendbuf;
		//        rptr = (struct uxapi *)recvline;
		//        memcpy(sndptr, rptr, sizeof( struct uxapi ));
				hwptr = &hwdetail;
				hwptr -> sll_ifindex = cacheptr -> sll_ifindex;
				hwptr -> sll_hatype = 1;
				hwptr -> sll_halen = 8;
				memcpy((void *)hwptr -> sll_addr, (void *)cacheptr -> mac, 8);
				memcpy((struct hwaddr *)sendbuf, hwptr, sizeof(hwdetail));
				hwptr1 = (struct hwaddr *)sendbuf;

				printf("\n------------------------Sending HW details to client-6  ---------------------\n");
				printf("ifindex = %d, hatype = %d, halen = %d, Mac = %02x:%02x:%02x:%02x:%02x:%02x\n",\
				  hwptr1 -> sll_ifindex, hwptr1 -> sll_hatype, hwptr1 -> sll_halen, hwptr1 -> sll_addr[0],\
				  hwptr1 -> sll_addr[1], hwptr1 -> sll_addr[2], hwptr1 -> sll_addr[3], hwptr1 -> sll_addr[4],\
				  hwptr1 -> sll_addr[5]);
				printf("------------------------------------------------------------------------------------\n");

				//sleep(4);
				 writen(confd,sendbuf,sizeof(struct hwaddr));
				//printf("sendout;;;;;;;;;;;");
	
				
			
				//Sendto(confd, sendbuf, 512, 0, (struct sockaddr*)&cliaddr, len);
				close(confd);
			}
			else{//no such entry in the cache
			
					flag =1;//if this flag is set that means we need to write back to tour client
					
					pendip = ip_addr;
					printf("\nNo entry for hardware address. Sending ARP Request \n");
			/* preserve the IP address to send H/W details to client later 
				  ptrpend = malloc(sizeof(struct pending_reply));
				  ptrpend -> srcnodeip
			*/

			/* Send ARP Request with dst_mac set to broadcast of 0xffffffffff */
				    for(i=0; i < 6; i++){
						pfaddr.sll_addr[i] = 0xff;
						dst_mac[i] = 0xff;
						src_mac[i] = mymac[i];
				    }
				    bzero(buffer, ETH_FRAME_LEN);
			/* copy ethernet packet header in to send buffer */
					memcpy((void*)buffer, (void*)dst_mac, ETH_ALEN);
					memcpy((void*)(buffer+ETH_ALEN), (void*)src_mac, ETH_ALEN);
					eh->h_proto=htons(ETH_P_ARP_PROTO);

			/* broadcast ARP REQUEST */
				  arpreq_ptr -> dst_ip = ip_addr; /* Ip address of the node whose HW dsertails are to be gathered */
				  for(i=0; i < 6; i++){
					arpreq_ptr -> dst_mac[i] = 0xff; /* dest mac is 0xffffffffffff for broadcast */
					arpreq_ptr -> src_mac[i] = mymac[i];
				  }
			/* copy arp Request packet in to send buffer */
				  memcpy((void *)(buffer+14), (void *)arpreq_ptr, sizeof( struct arp_req_res));
				  printf("\n------------------ Sending ARP REQUEST packet-2 ---------------------\n");
			//      struct arp_req_res *arpptr;
				  arpptr = (struct arp_req_res *)(buffer+14);

				  printf(" ID = %d hw_type = %d   prot_type = %x   hw_size = %d   prot_size = %d   OP = %d \n",arpptr -> id, arpptr -> hw_type, \
						  arpptr -> prot_type, arpptr -> hw_size, arpptr -> prot_size, arpptr -> op);
				  Ipad.s_addr = arpptr -> src_ip;
				  printf("SrcMac = %02x:%02x:%02x:%02x:%02x:%02x   SrcIp = %s \n", arpptr -> src_mac[0], arpptr -> src_mac[1], \
					arpptr -> src_mac[2], arpptr -> src_mac[3], arpptr -> src_mac[4], arpptr -> src_mac[5], inet_ntoa( Ipad));
				  Ipad.s_addr = arpptr -> dst_ip;
				  printf("DstMac = %02x:%02x:%02x:%02x:%02x:%02x   DstIp = %s \n", arpptr -> dst_mac[0], arpptr -> dst_mac[1], \
					arpptr -> dst_mac[2], arpptr -> dst_mac[3], arpptr -> dst_mac[4], arpptr -> dst_mac[5], inet_ntoa( Ipad) );
				 printf(".......................................................................\n\n");
			 

						Sendto(pfsocket, buffer, ETH_FRAME_LEN, 0, (struct sockaddr*)&pfaddr, sizeof(pfaddr));
				 // print_cache();

			/* The client requested MAC address was not found in the cache. So an ARP Request was sent. 
			  The respective node will respond with ARP response and the cache will be updated.
			  After the cache is updated send the MAC address to the client. The pending response 
			  is indicated by the list */				
	   
			}

		}/* End of unix recieve */	
				
	/***************************************************************************************************/

	/*PF socket recieve*/

		if(FD_ISSET(pfsocket, &rset)){

		bzero(recv_buf, ETH_FRAME_LEN);
			Recvfrom(pfsocket, recv_buf, ETH_FRAME_LEN, 0, NULL, NULL);
		arpptr = (struct arp_req_res *)(recv_buf +14);
		Ipad.s_addr = arpptr -> dst_ip;
		//printf("\tdest of the req is %s\n",inet_ntoa(Ipad));
		if(arpptr -> id != 531)
			continue;
	//    if(arp_ptr -> dst_ip != myip)
	//      continue; /* If the msg is not meant for you, ignore */
	//    if(arp_ptr -> dst_ip != arp_ptr -> src_ip)
	//      continue; /* If it is self generated msg & received on loopback, ignore */
		
			if(arpptr -> op == 2){ /* received ARP response */
					//printf("in the 2\n");
				  cache_update(arpptr -> src_ip, my_ifindex, arpptr -> src_mac, -1);
				  print_cache();

				  printf("\n------------------ Received ARP RESPONSE packet-3 ---------------------\n");
				  printf(" ID = %d hw_type = %d   prot_type = %x   hw_size = %d   prot_size = %d   OP = %d \n", arpptr -> id, arpptr -> hw_type, \
						  arpptr -> prot_type, arpptr -> hw_size, arpptr -> prot_size, arpptr -> op);
				  Ipad.s_addr = arpptr -> src_ip;
				  printf("SrcMac = %02x:%02x:%02x:%02x:%02x:%02x   SrcIp = %s \n", arpptr -> src_mac[0], arpptr -> src_mac[1], \
					arpptr -> src_mac[2], arpptr -> src_mac[3], arpptr -> src_mac[4], arpptr -> src_mac[5], inet_ntoa( Ipad));
				  Ipad.s_addr = arpptr -> dst_ip;
				  printf("DstMac = %02x:%02x:%02x:%02x:%02x:%02x   DstIp = %s \n", arpptr -> dst_mac[0], arpptr -> dst_mac[1], \
					arpptr -> dst_mac[2], arpptr -> dst_mac[3], arpptr -> dst_mac[4], arpptr -> dst_mac[5], inet_ntoa( Ipad) );
				  printf(".......................................................................\n\n");
			}
			
			if(arpptr -> op == 1){ /* received ARP request. Send ARP response */
					//if i am not the destination I will ignore it
					int lin1,lin2;
					//printf("in the 1\n");
					//Ipad.s_addr = arpptr -> dst_ip;
					in_addr_temp1.s_addr = myip;
					
					//in_addr_temp3.s_addr = arpptr -> src_ip;
					 /* If the msg is not meant for you, ignore */
					// printf("dest of req ip is %s\n", inet_ntoa(Ipad));
					// printf("my ip is %s\n",inet_ntoa(in_addr_temp1) );
					 lin1 = atoi(inet_ntoa(Ipad)+11);
					 lin2 = atoi(inet_ntoa(in_addr_temp1)+11);
					 //printf("lin1 is %d, lin2 is %d\n", lin1, lin2); 
					 
					if(lin1 != lin2){//i am not the destination
						if(mac_lookup(arpptr -> src_ip) != NULL){//check the cache,
							printf("\nReceived a broadcast request, I am not the destination, but sender's info is in my cache, updating cache...\n");
							print_cache();
						}
						else {
							printf("\nReceived a broadcast request, I am not the destination and the sender's information is not in my cache, ignore it\n ");
						}
						
					}else{//I am the destination, update cache, and send back
						  printf("\nReceived a broadcast request, I am the destination of req, response with HW addr\n");
						  cache_update(arpptr -> src_ip, my_ifindex, arpptr -> src_mac, -1);
						  print_cache();

						  printf("\n------------------ Received ARP REQUEST packet-4 ---------------------\n");
						  printf("ID =%d hw_type = %d   prot_type = %x   hw_size = %d   prot_size = %d   OP = %d \n",arpptr -> id , arpptr -> hw_type, \
								  arpptr -> prot_type, arpptr -> hw_size, arpptr -> prot_size, arpptr -> op);
						  Ipad.s_addr = arpptr -> src_ip;
						  printf("SrcMac = %02x:%02x:%02x:%02x:%02x:%02x   SrcIp = %s \n", arpptr -> src_mac[0], arpptr -> src_mac[1], \
							arpptr -> src_mac[2], arpptr -> src_mac[3], arpptr -> src_mac[4], arpptr -> src_mac[5], inet_ntoa( Ipad));
						  Ipad.s_addr = arpptr -> dst_ip;
						  printf("DstMac = %02x:%02x:%02x:%02x:%02x:%02x   DstIp = %s \n", arpptr -> dst_mac[0], arpptr -> dst_mac[1], \
							arpptr -> dst_mac[2], arpptr -> dst_mac[3], arpptr -> dst_mac[4], arpptr -> dst_mac[5], inet_ntoa( Ipad) );
						  printf(".......................................................................\n\n");
						
						//in_addr_temp1.s_addr = myip;
					//in_addr_temp2.s_addr = arpptr -> dst_ip;
					//in_addr_temp3.s_addr = arpptr -> src_ip;
					 /* If the msg is not meant for you, ignore */
					// printf("\tdest of the req is %s\n",inet_ntoa(Ipad));
						
						

						  ip_addr = arpptr -> src_ip;
						  for(i=0; i < 6; i++)
							mac[i] = arpptr -> src_mac[i];
						  arpres_ptr = &arpres;
						  arpres_ptr -> dst_ip = ip_addr;
						  arpres_ptr -> src_ip = myip;

						  for(i=0; i < 6; i++){
							arpres_ptr -> dst_mac[i] = mac[i];
							pfaddr.sll_addr[i] = mac[i];
							arpres_ptr -> src_mac[i] = mymac[i];
						  }
						  bzero(buffer, ETH_FRAME_LEN);
						  memcpy((void*)buffer, (void*)arpres_ptr -> dst_mac, ETH_ALEN);
						  memcpy((void*)(buffer+ETH_ALEN), (void*)arpres_ptr -> src_mac, ETH_ALEN);
						  eh->h_proto=htons(ETH_P_ARP_PROTO);
						  memcpy((void*)(buffer+14), (void*)arpres_ptr, sizeof(struct arp_req_res));/* copy the ARP response */

						  arpptr = (struct arp_req_res *)(buffer + 14);
						  printf("\n------------------ Sending ARP RESPONSE packet-5 ---------------------\n");
						  printf("ID = %d hw_type = %d   prot_type = %x   hw_size = %d   prot_size = %d   OP = %d \n",arpptr -> id, arpptr -> hw_type, \
								  arpptr -> prot_type, arpptr -> hw_size, arpptr -> prot_size, arpptr -> op);
						  Ipad.s_addr = arpptr -> src_ip;
						  printf("SrcMac = %02x:%02x:%02x:%02x:%02x:%02x   SrcIp = %s \n", arpptr -> src_mac[0], arpptr -> src_mac[1], \
							arpptr -> src_mac[2], arpptr -> src_mac[3], arpptr -> src_mac[4], arpptr -> src_mac[5], inet_ntoa( Ipad));
						  Ipad.s_addr = arpptr -> dst_ip;
						  printf("DstMac = %02x:%02x:%02x:%02x:%02x:%02x   DstIp = %s \n", arpptr -> dst_mac[0], arpptr -> dst_mac[1], \
							arpptr -> dst_mac[2], arpptr -> dst_mac[3], arpptr -> dst_mac[4], arpptr -> dst_mac[5], inet_ntoa( Ipad) );
						  printf(".......................................................................\n\n");
						  Sendto(pfsocket, buffer, ETH_FRAME_LEN, 0, (struct sockaddr*)&pfaddr, sizeof(pfaddr));
					}
			}/* end of ARP request received */

		  }/* End of PF socket recieve */   
		if(flag == 1){//the flag is 1, and this 
			if((cacheptr = mac_lookup(pendip))!= NULL){
				bzero(sendbuf,512);
				hwptr = &hwdetail;
				hwptr -> sll_ifindex = cacheptr -> sll_ifindex;
				hwptr -> sll_hatype = 1;
				hwptr -> sll_halen = 8;
				memcpy((void *)hwptr -> sll_addr, (void *)cacheptr -> mac, 8);
				memcpy((struct hwaddr *)sendbuf, hwptr, sizeof(hwdetail));
				hwptr1 = (struct hwaddr *)sendbuf;

				//memcpy( sendbuf,cacheptr -> mac, 8 );
				//Sendto(confd, sendbuf, 512, 0, (struct sockaddr*)&cliaddr, len);
				flag =0;	
			
				writen(confd,sendbuf,sizeof(struct hwaddr));
				}

		}
	}/* end of  while */
}/* end of main*/


	      
