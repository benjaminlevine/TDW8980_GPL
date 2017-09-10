#include <stdio.h>
#include <stdlib.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <time.h>
#include <netinet/icmp6.h>
#include <linux/socket.h>
#include <errno.h>

#define MAX_PACKET_SIZE	32	//mac neighbor solicit or advertise packet size
#ifndef MAX_IF_LEN
#define MAX_IF_LEN		16
#endif

#ifndef TRUE
#define TRUE	1
#define FALSE 	0
#endif

#ifdef DEBUG
#define LOG_ERROR	
#define LOG_DHCP6C
#endif

/****************************************************/
/* brief: dad test, check the link addr conflict
 * in:	ifName	the interface to be checked
 * in: 	addr	the address to be checked
 * out: TRUE	DAD pass
 		FALSE	DAD Fail
 */
int dad_start(char *ifName, struct in6_addr addr);

/****************************************************/
/* brief: create the socket for dad test packet send and recv
 * in:	ifName	the interface name to send/recv packet 
 * out: >0 socket created.
 		other	error
 */
int createDadSk(char *ifName);

/****************************************************/
/* brief: send a neighbour solicite packet to the selected interface
 * in:	sock		the packet send socket
 * in:  targetAddr	the target address of the ns packet to be sent.
 * in:	ifName		the packet send interface name
 * out: 
 */
void sendNS(int sock, struct in6_addr targetAddr, char *ifName);

/****************************************************/
/* brief: wait and recv a neighbour advertisement packet, or the time out
 * in:	sock		the packet recv socket
 * in:  targetAddr	the target address of the na packet to be recv.
 * in:  timeout		the selecte wait timeout
 * in:  ifName		the recv interface name
 * out: FALSE		timeout
 		TRUE		recv the target na
 */
int recvNA(int sock, struct in6_addr targetAddr, struct timeval timeout, char *ifName);

/****************************************************/
/* brief: check the selected device active
 * in:	sock		the ioctl socket
 * in:  ifName		the check device interface name
 * out: 0			OK
 		-1			Can not be used
 */
int check_device(int sock, char *ifName);


void info(void)
{
	printf("usage:\n");
	printf("\tsendNS <ifName> <addr>\n");
	return;
}

/****************************************************/
/* brief: dad test, check the link addr conflict
 * in:	ifName	the interface to be checked
 * in: 	addr	the address to be checked
 * out: TRUE	DAD pass
 		FALSE	DAD Fail
 */
int dad_start(char *ifName, struct in6_addr addr)
{
	int dadSk = 0;
	int dadRetryTime = 3;
	int dadRetryInterval = 100;	//minisecond
	int dadCount = 0;		
	int ret = 0;

	struct timeval tm;

	tm.tv_sec = 0;
	tm.tv_usec = dadRetryInterval;
	
	dadSk = createDadSk(ifName);

	if(dadSk <= 0)
	{
		return TRUE;
	}

	while(dadCount < dadRetryTime)
	{
		sendNS(dadSk, addr, ifName);
		if (TRUE == recvNA(dadSk, addr, tm, ifName))
		{
			close(dadSk);
			return FALSE;
		}
		dadCount++;		
	}
	
	return TRUE;
}

/****************************************************/
/* brief: create the socket for dad test packet send and recv
 * in:	ifName	the interface name to send/recv packet 
 * out: >0 socket created.
 		other	error
 */
int createDadSk(char *ifName)
{
	int sock;
	struct icmp6_filter filter;
	int err, val;

    sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	if (sock < 0)
	{
		return (-1);
	}

	val = 1;
	err = setsockopt(sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &val, sizeof(val));
	if (err < 0)
	{
		return (-1);
	}

	val = 2;
#ifdef __linux__
	err = setsockopt(sock, IPPROTO_RAW, IPV6_CHECKSUM, &val, sizeof(val));
#else
	err = setsockopt(sock, IPPROTO_IPV6, IPV6_CHECKSUM, &val, sizeof(val));
#endif
	if (err < 0)
	{
		return (-1);
	}

	val = 255;
	err = setsockopt(sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &val, sizeof(val));
	if (err < 0)
	{
		return (-1);
	}

	val = 255;
	err = setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &val, sizeof(val));
	if (err < 0)
	{
		return (-1);
	}

#ifdef IPV6_RECVHOPLIMIT
	val = 1;
	err = setsockopt(sock, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &val, sizeof(val));
	if (err < 0)
	{
		return (-1);
	}
#endif

	/*
	 * setup ICMP filter
	 */
	
	ICMP6_FILTER_SETBLOCKALL(&filter);
	ICMP6_FILTER_SETPASS(ND_NEIGHBOR_SOLICIT, &filter);
	ICMP6_FILTER_SETPASS(ND_NEIGHBOR_ADVERT, &filter);

	err = setsockopt(sock, IPPROTO_ICMPV6, ICMP6_FILTER, &filter,
			 sizeof(filter));
	if (err < 0)
	{
		return (-1);
	}

	return sock;
}

/****************************************************/
/* brief: send a neighbour solicite packet to the selected interface
 * in:	sock		the packet send socket
 * in:  targetAddr	the target address of the ns packet to be sent.
 * out: 
 */
void sendNS(int sock, struct in6_addr targetAddr, char *ifName)
{
	char nsPacket[sizeof(struct nd_neighbor_solicit) + sizeof(char) * 8];	//8 bytes for llc addr option
	struct nd_neighbor_solicit *pNS;
	struct slladdr_op {
		unsigned char type;
		unsigned char len;
		unsigned char addr[6];
	} *pSll;
	struct in6_addr multAddr;
	struct sockaddr_in6 addr;
	char __attribute__((aligned(8))) chdr[CMSG_SPACE(sizeof(struct in6_pktinfo))];
	struct ifreq ifr;
	struct in6_pktinfo *pkt_info;
	struct msghdr mhdr;
	struct cmsghdr *cmsg;
	struct iovec iov;
	int ifIndex = 0;
	int err = 0;
		
	multAddr.s6_addr32[0] = htonl(0xFF020000);
	multAddr.s6_addr32[1] = 0;
	multAddr.s6_addr32[2] = htonl(0x1);
	multAddr.s6_addr32[3] = htonl(0xFF000000) | targetAddr.s6_addr32[3];

	check_device(sock, ifName);

	pNS = (struct nd_neighbor_solicit *)nsPacket;
	pSll = (struct slladdr_op *)(pNS + 1);

	strncpy(ifr.ifr_name, ifName, IFNAMSIZ-1);
	ifr.ifr_name[IFNAMSIZ-1] = '\0';

	if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0)
	{
		return;
	}
	ifIndex = ifr.ifr_ifindex;

	if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0)
	{
		return;
	}
	pSll->type = 1;
	pSll->len = 1;
	memcpy(pSll->addr, ifr.ifr_hwaddr.sa_data, 6);
	
	pNS->nd_ns_type = ND_NEIGHBOR_SOLICIT;
	pNS->nd_ns_code = 0;
	pNS->nd_ns_cksum = 0;
	pNS->nd_ns_reserved = 0;

	pNS->nd_ns_target = targetAddr;

	
	iov.iov_len  = sizeof(nsPacket);
	iov.iov_base = (caddr_t) nsPacket;
	
	memset(chdr, 0, sizeof(chdr));
	cmsg = (struct cmsghdr *) chdr;
	
	cmsg->cmsg_len	 = CMSG_LEN(sizeof(struct in6_pktinfo));
	cmsg->cmsg_level = IPPROTO_IPV6;
	cmsg->cmsg_type  = IPV6_PKTINFO;
	
	pkt_info = (struct in6_pktinfo *)CMSG_DATA(cmsg);
	pkt_info->ipi6_ifindex = ifIndex;

	memset((void *)&addr, 0, sizeof(addr));
	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(IPPROTO_ICMPV6);
	memcpy(&addr.sin6_addr, &multAddr, sizeof(struct in6_addr));
	
#ifdef HAVE_SIN6_SCOPE_ID
		if (IN6_IS_ADDR_LINKLOCAL(&addr.sin6_addr) ||
			IN6_IS_ADDR_MC_LINKLOCAL(&addr.sin6_addr))
				addr.sin6_scope_id = ifIndex;
#endif
	
	memset(&mhdr, 0, sizeof(mhdr));
	mhdr.msg_name = (caddr_t)&addr;
	mhdr.msg_namelen = sizeof(struct sockaddr_in6);
	mhdr.msg_iov = &iov;
	mhdr.msg_iovlen = 1;
	mhdr.msg_control = (void *) cmsg;
	mhdr.msg_controllen = sizeof(chdr);

	err = sendmsg(sock, &mhdr, 0);
	
	if (err < 0) {
		printf("sendmsg: %s", strerror(errno));
	}
	
}

/****************************************************/
/* brief: wait and recv a neighbour advertisement packet, or the time out
 * in:	sock		the packet recv socket
 * in:  targetAddr	the target address of the na packet to be recv.
 * in:  timeout		the selecte wait timeout
 * in:  ifName		the recv interface name
 * out: FALSE		timeout
 		TRUE		recv the target na
 */
int recvNA(int sock, struct in6_addr targetAddr, struct timeval timeout, char *ifName)
{
	struct msghdr mhdr;
	struct cmsghdr *cmsg = NULL;
	struct iovec iov;
	struct sockaddr_in6 addr;
	struct in6_pktinfo *pktInfo = NULL; 
	struct nd_neighbor_advert *pAdv;
	struct icmp6_hdr *icmph;
	unsigned char *chdr = NULL;
	unsigned int chdrlen = 0;
	fd_set rfds;
	int len; 
	int hoplimit;

	int ret = 0;

	char *msg = NULL;
	
	if( ! chdr )
	{
		chdrlen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
				CMSG_SPACE(sizeof(int));
		if ((chdr = malloc(chdrlen)) == NULL) {
			return -1;
		}
	}

	msg = malloc(MAX_PACKET_SIZE * sizeof(char ));

	if (msg == NULL)
	{
		free(chdr);
		return -1;
	}

	while(1)
	{
		pktInfo= NULL;
		
		FD_ZERO( &rfds );
		FD_SET( sock, &rfds );
		
		if( (ret = select( sock+1, &rfds, NULL, NULL, &timeout )) < 0 )
		{
			return -1;
		}

		if (ret == 0)
		{
			free(msg);
			free(chdr);
			return FALSE;
		}

		iov.iov_len = MAX_PACKET_SIZE;
		iov.iov_base = (caddr_t) msg;

		memset(&mhdr, 0, sizeof(mhdr));
		mhdr.msg_name = (caddr_t)&addr;
		mhdr.msg_namelen = sizeof(addr);
		mhdr.msg_iov = &iov;
		mhdr.msg_iovlen = 1;
		mhdr.msg_control = (void *)chdr;
		mhdr.msg_controllen = chdrlen;

		len = recvmsg(sock, &mhdr, 0);

		if (len < 0)
		{
			continue;
		}

		for (cmsg = CMSG_FIRSTHDR(&mhdr); cmsg != NULL; cmsg = CMSG_NXTHDR(&mhdr, cmsg))
		{
          	if (cmsg->cmsg_level != IPPROTO_IPV6)
          		continue;
          
          	switch(cmsg->cmsg_type)
         	{
#ifdef IPV6_HOPLIMIT
              case IPV6_HOPLIMIT:
                if ((cmsg->cmsg_len == CMSG_LEN(sizeof(int))) && 
                    (*(int *)CMSG_DATA(cmsg) >= 0) && 
                    (*(int *)CMSG_DATA(cmsg) < 256))
                {
                  hoplimit = *(int *)CMSG_DATA(cmsg);
                }
               
                break;
#endif /* IPV6_HOPLIMIT */
              case IPV6_PKTINFO:
                if ((cmsg->cmsg_len == CMSG_LEN(sizeof(struct in6_pktinfo))) &&
                    ((struct in6_pktinfo *)CMSG_DATA(cmsg))->ipi6_ifindex)
                {
                  pktInfo = (struct in6_pktinfo *)CMSG_DATA(cmsg);
                }
               
                break;
          	}
		}

		if (pktInfo == NULL)
		{
			continue;
		}

		icmph = (struct icmp6_hdr *) msg;

		if (icmph->icmp6_type != ND_NEIGHBOR_ADVERT)
		{
		/*
		 *	We just want to listen to NSs and NAs
		 */
			continue;
		}

		if (icmph->icmp6_code != 0)
		{			
			continue;
		}

		if (icmph->icmp6_type == ND_NEIGHBOR_ADVERT)
		{
			/*NS packet*/
			pAdv = (struct nd_neighbor_advert *)msg;
			if ( 0 == memcmp(&pAdv->nd_na_target, &targetAddr, sizeof(struct in6_addr)))
			{
				free(msg);
				free(chdr);
				return TRUE;
			}			
		}
	}
	free(msg);
	free(chdr);
	return FALSE;
}

int check_device(int sock, char *ifName)
{
	struct ifreq	ifr;
	
	strncpy(ifr.ifr_name, ifName, IFNAMSIZ-1);
	ifr.ifr_name[IFNAMSIZ-1] = '\0';

	if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0)
	{
		return (-1);
	}

	if (!(ifr.ifr_flags & IFF_UP))
	{
		return (-1);
	}
	if (!(ifr.ifr_flags & IFF_RUNNING))
	{
		return (-1);
	}

	if (!(ifr.ifr_flags & IFF_MULTICAST))
	{
		return (-1);
	}

	return 0;
}

