/* 
 * common.c
 *
 * contains the helper functions used both by the client and server
 *
 *
 *
 */
#include "unp.h"
#include "common.h"
#include <time.h>
/* Richard Steven's helper functions */

char *
sock_ntop (const struct sockaddr *sa, socklen_t salen)
{
  char portstr[7];
  static char str[128];		/* Unix domain is largest */

  struct sockaddr_in *sin = (struct sockaddr_in *) sa;

  if (inet_ntop (AF_INET, &sin->sin_addr, str, sizeof (str)) == NULL)
    return (NULL);

  if (ntohs (sin->sin_port) != 0) {
    snprintf (portstr, sizeof (portstr), ".%d", ntohs (sin->sin_port));
    strcat (str, portstr);
  }
  return (str);
}





char *
sock_ntop_host(const struct sockaddr *sa, socklen_t salen)
{
  static char str[128];		/* Unix domain is largest */

  switch (sa->sa_family) {
  case AF_INET: {
    struct sockaddr_in	*sin = (struct sockaddr_in *) sa;

    if (inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str)) == NULL)
      return(NULL);
    return(str);
  }

#ifdef	IPV6
  case AF_INET6: {
    struct sockaddr_in6	*sin6 = (struct sockaddr_in6 *) sa;

    if (inet_ntop(AF_INET6, &sin6->sin6_addr, str, sizeof(str)) == NULL)
      return(NULL);
    return(str);
  }
#endif

#ifdef	AF_UNIX
  case AF_UNIX: {
    struct sockaddr_un	*unp = (struct sockaddr_un *) sa;

    /* OK to have no pathname bound to the socket: happens on
       every connect() unless client calls bind() first. */
    if (unp->sun_path[0] == 0)
      strcpy(str, "(no pathname bound)");
    else
      snprintf(str, sizeof(str), "%s", unp->sun_path);
    return(str);
  }
#endif

#ifdef	HAVE_SOCKADDR_DL_STRUCT
  case AF_LINK: {
    struct sockaddr_dl	*sdl = (struct sockaddr_dl *) sa;

    if (sdl->sdl_nlen > 0)
      snprintf(str, sizeof(str), "%*s",
	       sdl->sdl_nlen, &sdl->sdl_data[0]);
    else
      snprintf(str, sizeof(str), "AF_LINK, index=%d", sdl->sdl_index);
    return(str);
  }
#endif
  default:
    snprintf(str, sizeof(str), "sock_ntop_host: unknown AF_xxx: %d, len %d",
	     sa->sa_family, salen);
    return(str);
  }
  return (NULL);
}


struct hwa_info *
get_hw_addrs()
{
  struct hwa_info	*hwa, *hwahead, **hwapnext;
  int		sockfd, len, lastlen, alias;
  char		*ptr, *buf, lastname[IF_NAME], *cptr;
  struct ifconf	ifc;
  struct ifreq	*ifr, ifrcopy;
  struct sockaddr	*sinptr;

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  lastlen = 0;
  len = 100 * sizeof(struct ifreq);	/* initial buffer size guess */
  for ( ; ; ) {
    buf = (char*) malloc(len);
    ifc.ifc_len = len;
    ifc.ifc_buf = buf;
    if (ioctl(sockfd, SIOCGIFCONF, &ifc) < 0) {
      if (errno != EINVAL || lastlen != 0) {
	printf("ioctl error");
	exit(0);
      }
    } else {
      if (ifc.ifc_len == lastlen)
	break;		/* success, len has not changed */
      lastlen = ifc.ifc_len;
    }
    len += 10 * sizeof(struct ifreq);	/* increment */
    free(buf);
  }
  hwahead = NULL;
  hwapnext = &hwahead;
  lastname[0] = 0;
  for (ptr = buf; ptr < buf + ifc.ifc_len; ) {
    ifr = (struct ifreq *) ptr;
    len = sizeof(struct sockaddr);
    ptr += sizeof(ifr->ifr_name) + len;	/* for next one in buffer */
    alias = 0; 
    hwa = (struct hwa_info *) calloc(1, sizeof(struct hwa_info));
    memcpy(hwa->if_name, ifr->ifr_name, IF_NAME);		/* interface name */
    hwa->if_name[IF_NAME-1] = '\0';
    /* start to check if alias address */
    if ( (cptr = (char *) strchr(ifr->ifr_name, ':')) != NULL)
      *cptr = 0;		/* replace colon will null */
    if (strncmp(lastname, ifr->ifr_name, IF_NAME) == 0) {
      alias = IP_ALIAS;
    }
    memcpy(lastname, ifr->ifr_name, IF_NAME);
    ifrcopy = *ifr;
    *hwapnext = hwa;		/* prev points to this new one */
    hwapnext = &hwa->hwa_next;	/* pointer to next one goes here */

    hwa->ip_alias = alias;		/* alias IP address flag: 0 if no; 1 if yes */
    sinptr = &ifr->ifr_addr;
    hwa->ip_addr = (struct sockaddr *) calloc(1, sizeof(struct sockaddr));
    memcpy(hwa->ip_addr, sinptr, sizeof(struct sockaddr));	/* IP address */
    ioctl(sockfd, SIOCGIFHWADDR, &ifrcopy);	/* get hw address */
    memcpy(hwa->if_haddr, ifrcopy.ifr_hwaddr.sa_data, IF_HADDR);
    ioctl(sockfd, SIOCGIFINDEX, &ifrcopy);	/* get interface index */
    memcpy(&hwa->if_index, &ifrcopy.ifr_ifindex, sizeof(int));
  }
  free(buf);
  return(hwahead);	/* pointer to first structure in linked list */
}

void
free_hwa_info(struct hwa_info *hwahead)
{
  struct hwa_info	*hwa, *hwanext;

  for (hwa = hwahead; hwa != NULL; hwa = hwanext) {
    free(hwa->ip_addr);
    hwanext = hwa->hwa_next;	/* can't fetch hwa_next after free() */
    free(hwa);			/* the hwa_info{} itself */
  }
}
/* end free_hwa_info */

struct hwa_info *
Get_hw_addrs()
{
  struct hwa_info	*hwa;

  if ( (hwa = get_hw_addrs()) == NULL) {
    printf("get_hw_addrs error");
    exit(1);
  }
  return(hwa);
}


char * get_my_mac(int index)
{

  struct hwa_info *hwa, *hwahead;
  char   *ptr;
  char *mac=(char*)malloc(50);
  int    prflag;

  for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next)
    {

      prflag = 0;
      int i = 0;
      int k=0;
      do {
	if (hwa->if_haddr[i] != '\0') {
	  prflag = 1;
	  break;
	}
      } while (++i < IF_HADDR);

      if (prflag) {
	ptr = hwa->if_haddr;
	i = IF_HADDR;
	do {
	  sprintf(mac+(3*k++),"%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
	} while (--i > 0);
      }
      if(hwa->if_index==index)
	{
	  return mac;
	}
    }
  return NULL;
}


int
prhwaddrs()
{
  struct hwa_info	*hwa, *hwahead;
  struct sockaddr	*sa;
  char   *ptr;
  int    i, prflag;

  printf("\n");

  for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {
		
    printf("%s :%s", hwa->if_name, ((hwa->ip_alias) == IP_ALIAS) ? " (alias)\n" : "\n");
		
    if ( (sa = hwa->ip_addr) != NULL)
      printf("         IP addr = %s\n", sock_ntop_host(sa, sizeof(*sa)));
				
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

    printf("\n         interface index = %d\n\n", hwa->if_index);
  }

  free_hwa_info(hwahead);
  return 0;
  //	exit(0);
}
