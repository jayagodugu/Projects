Readme
	
	
	
Operation:
	(1). Make, then deploy  arp_lin  and  tour_lin  on vms.
	(2). Start all arp_lin(./arp_lin) on vms.
	(3). Start lin_tour(./lin_tour) which are not the source node, after turning on all other tour node, you can turn on the source node (./lin_tour vm? vm? vm? vm? ... < those vm? are the order of nodes in the tour > ). Then you can test.
	
	
Tour part:
	(1 )The source node receives the tour, and converts them into IP adresses and stores them in a tour packet array, the first element is source node IP address, the second is how many nodes left of our tour, the third is the size of the array. The fourth is the position of next node it will send.
	
	(2) The source node use rt_send() send the tour packet to next node of the tour, then join itself to the Multicast. The second node received the tour packet will let the second element of tour array minus 1, if that number is 0, that means I am the last node. If it is not the last node, it will let the fourth element of the tour array plus one, that will point to the IP address of next node in the tour, and it join to the multicast and start ping, and then send the packet to the next node.
	
	(3) A node receive the tour packet, if it is its first time, it will use areq(), to get the HW address of the source node from ARP, and start ping the source node once per second.
	
	(4) The last node receive the tour packet, if it is its first time receiving, it will start ping for 5 seconds, or it just waiting for 5 seconds, and send multicast to stop other node's ping. And every other nodes will send multicast info to each other, to let other know it is a member of the tour.
	
	
ARP part:
	The arp module is used to identify the source node's MAC address. When a request for the source MAC address is placed by the tour client the ARP module recieves this through the Unix domain socket and checks wether there is an entry in its cache table for the given IP address.

	If an entry is present the same is retrieved & return back to the tour client.If the entry for the corresponding adress is not present then a broadcast is done( i.e. an areq is sent) over the pf socket.On arrival of the response fom the coressponding node (i.e. areply) over the pf socket  this reply is stored in the cache ( i.e. the cache is updated) and is sent back to the tour client over the unix domain socket.

	The following data structes have been created and used as per assignment:

	hardware address structure:

	arp response/request:

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

	cache:

		struct cache {
			 
			in_addr_t ipaddr;
				
			unsigned char mac[8];
				
			unsigned int sll_ifindex;
				
			unsigned short sll_hatype;
				
			int confd;
				
			struct cache *next;
		}; 