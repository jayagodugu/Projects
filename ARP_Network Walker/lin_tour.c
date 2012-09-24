#include "ass4version1.h"


int
main(int argc, char **argv)
{
	
	int n, i, size, prflag;
	int pos = 3;
	int maxfd;
	int rt_sockfd,recv_ping_sockfd;
	int send_mulcst_fd, recv_mulcst_fd;
	const int on = 1;
	
	char buff_recv[BUFFSIZE];
	char tour_array[TOURNUM][IPLEN];
	char *ptr;

	fd_set  rset;
	struct timeval tval, tval1, tval2, tval3, tval4;
	struct sockaddr_ll saddrll;

	Signal(SIGALRM, sig_alrm);
	//Signal(SIGCHLD, sig_chld);
	uname(&myname);
	
	if (argc == 1){
		fputs("This is NOT source node\n", stdout);
		//_is_src_node = 0;
		source_flag = 0;
	}else{
		fputs("I'm source node, start to send tour infomation \n", stdout);
		//_is_src_node = 1;
		source_flag = 1;
	}

	
	//if (uname(&myname) < 0)
		//err_sys("uname error");

	for ( hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {

		if(strcmp(hwa->if_name,"eth0")==0){
		
			/* in eth0 we create pfsocket and bind it to hw addr, this pf socket is for sned ping info */
			//pf_sockfd = socket(PF_PACKET, SOCK_RAW,htons(ETH_P_IP));
			send_ping_pf_sockfd = socket(PF_PACKET, SOCK_RAW,htons(ETH_P_IP));
			//send_ping_pf_sockfd use to send ping to source node
			saddrll.sll_family      = PF_PACKET;
			saddrll.sll_ifindex     = hwa->if_index;
			saddrll.sll_protocol    = htons(ETH_P_IP); 
			bind(send_ping_pf_sockfd, (struct sockaddr *) &saddrll, sizeof(saddrll));
			
			
			printf("%s :%s", hwa->if_name, ((hwa->ip_alias) == IP_ALIAS) ? " (alias)\n" : "\n");
		
			if ( (sa = hwa->ip_addr) != NULL)
				printf("IP addr = %s\n", Sock_ntop_host(sa, sizeof(*sa)));
				
			prflag = 0;
			i = 0;
			do {
				if (hwa->if_haddr[i] != '\0') {
					prflag = 1;
					break;
				}
			} while (++i < IF_HADDR);

			if (prflag) {
				printf("         HW addr = ");
				ptr = hwa->if_haddr;
				i = IF_HADDR;
				do {
					printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
				} while (--i > 0);
			}
			printf("\n         Interface Index = %d\n\n", hwa->if_index);	
			break;
		}
		
	}
	
	
	/* creat two raw sockets for route travesal and receive ping*/

	rt_sockfd = Socket(AF_INET, SOCK_RAW, 254);
	setsockopt(rt_sockfd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));
	
	recv_ping_sockfd = Socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	setuid(getuid());           /* don't need special permissions any more */
	size = 60 * 1024;           /* OK if setsockopt fails */
	setsockopt (recv_ping_sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof (size));
		
	if (source_flag == 1)
	{//this node is source 

		struct icmp *icmp;
		struct ip* ip;
		int node_num;
		
		node_num = argc + 3;
		strcpy(tour_array[0], Sock_ntop_host(sa, sizeof(*sa)));//first one is source code

		sprintf(tour_array[1],"%d",argc - 1);
		sprintf(tour_array[2],"%d",node_num);
		sprintf(tour_array[3],"%d",4);
		for (i = 4; i < node_num; i++)
			strcpy(tour_array[i], getipbyvm(argv[i-3]));

//////////////////////////before send we let the source node join the multicast

		send_mulcst_fd = Udp_client(MC_ADDR_LIN, MC_PORT_LIN, (void **) &sasend, &salen);
		recv_mulcst_fd = Socket(AF_INET, SOCK_DGRAM, 0);
		Setsockopt(recv_mulcst_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
		sarecv = Malloc(salen);
		memcpy(sarecv, sasend, salen);
	
		Bind(recv_mulcst_fd, sarecv, salen);
		Mcast_join(recv_mulcst_fd, sasend, salen, NULL, 0);
		Mcast_set_loop(send_mulcst_fd, 0);
///////////////////////////////////////////////	this is the send tour part
	
		rt_send(rt_sockfd, tour_array);
	
		for(;;){
			if(first_mulcst_flag == 1){
				
				printf("wait for first multicast info\n");
				recv_all(recv_mulcst_fd, salen);
				first_mulcst_flag = 0;
				char buf[MAXLINE];
				//sleep(1);
				snprintf(buf, sizeof(buf), "<<<<< Node: %s I am a member of the group.>>>>>\n", myname.nodename);
				send_all(send_mulcst_fd, sasend, salen, buf);
				
			}else{
				for(;;){
					recv_all(recv_mulcst_fd, salen);
					printf("Waiting for 5 seconds and exit\n");
					
				}
			}
		}
			
	
			
		
		
		
	}else{//not source node
		pthread_t tid, tid2;
		char source_name[IPLEN];
		
		for( ;; )
		{
			FD_ZERO(&rset);
			
			FD_SET(rt_sockfd, &rset);
			
			
			maxfd = rt_sockfd;
			if(rt_recved_flag == 1){
				FD_SET(recv_ping_sockfd, &rset);
				maxfd = max(recv_ping_sockfd, maxfd);
			}
			
			if (ns_first_flag == 0)
			{
				FD_SET(recv_mulcst_fd,&rset);
				//maxfd = (maxfd > recv_mulcst_fd) ? maxfd : recv_mulcst_fd;
				maxfd = max(maxfd, recv_mulcst_fd);
			}
			
			//printf("before select\n");
			
			select(maxfd + 1, &rset, NULL, NULL, NULL);
			
			if (FD_ISSET(rt_sockfd, &rset)) 
			{    
				printf("receive route travelsal paket\n");
				n = rt_recv(rt_sockfd, tour_array);
				memcpy(dest_ip_addr, tour_array[0], IPLEN);
				if (n < 0) {
					if (errno == EINTR)
						continue;
				else
					err_sys("receive tour packet error");
				}
				
			    get_vmname(tour_array[0], source_name);
				
				
				 if (ns_first_flag == 1)
				{
					ns_first_flag = 0;
					rt_recved_flag = 1;
					
					// join the multicast first
					send_mulcst_fd = Udp_client(MC_ADDR_LIN, MC_PORT_LIN, (void **) &sasend, &salen);
					recv_mulcst_fd = Socket(AF_INET, SOCK_DGRAM, 0);
					Setsockopt(recv_mulcst_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
					sarecv = Malloc(salen);
					memcpy(sarecv, sasend, salen);
					Bind(recv_mulcst_fd, sarecv, salen);
					Mcast_join(recv_mulcst_fd, sasend, salen, NULL, 0);
					Mcast_set_loop(send_mulcst_fd, 0);
					
					//create a thread for ping
					Pthread_create(&tid, NULL, &ping, NULL);
						
				
				} 
				if(last_node_flag == 0){
					rt_send(rt_sockfd, tour_array);
				}else{
					//create a thread to handle last operations
					Pthread_create(&tid2, NULL, &ls_send_mul, &send_mulcst_fd);
				}
			}
			
			if (FD_ISSET(recv_mulcst_fd, &rset)) 
			{//recv multicast info
				if (first_mulcst_flag == 1 )
				{
					first_mulcst_flag = 0;
					ping_over_flag = 1;//
					//printf("ping_over_flag is %d\n", ping_over_flag);
					recv_all(recv_mulcst_fd, salen);
					char buf[MAXLINE];
					snprintf(buf, sizeof(buf), "<<<<< Node: %s I am a member of the group.>>>>>\n", myname.nodename);
					send_all(send_mulcst_fd, sasend, salen, buf); 
					//printf("gonna go to alarm with pof changed\n");
					alarm(0);
				
				}else{
						
						for(;;){
							recv_all(recv_mulcst_fd, salen);
						}
						printf("Waiting for 5 seconds and exit\n");
					}
				
			}
			
			 if (FD_ISSET(recv_ping_sockfd, &rset)) 
			{//recv ping reply
				//printf("received ping reply\n");
				recvfrom(recv_ping_sockfd, buff_recv, MAXLINE, 0, NULL, NULL);
				Gettimeofday (&tval, NULL);
				proc_v4 (buff_recv, n, &tval);
				if (ping_over_flag == 1)
					alarm(0);
			} 	
				
		}
	}
	

}

