/*
 * arpping.c
 *
 * Mostly stolen from: dhcpcd - DHCP client daemon
 * by Yoichi Hariguchi <yoichi@fore.com>
 */
#include <sys/time.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/if_ether.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "dhcps.h"
#include "arpping.h"
#include "common.h"


/* args:	yiaddr - what IP to ping
 *		ip - our ip
 *		mac - our arp address
 *		interface - interface to use
 * retn: 	1 addr free
 *		0 addr used
 *		-1 error
 */

/* FIXME: match response against chaddr */
int arpping(uint32_t yiaddr, uint8_t *chaddr, uint32_t ip, uint8_t *mac, char *interface)
{
    int	timeout = 2;
	int 	optval = 1;
	int	s;			/* socket */
	int	rv = 1;			/* return value */
	struct sockaddr addr;		/* for interface name */
	struct arpMsg	arp;
	fd_set		fdset;
	struct timeval	tm;
	time_t  prevTime;
	/* Added by xcl, 27Feb12, 将timeout改为0.5s */
	struct timeval timePre;
	struct timeval timeNow;
	long timeout_usec = 500000; /* 0.5s */
	/* End added */

	if ((s = socket (PF_PACKET, SOCK_PACKET, htons(ETH_P_ARP))) == -1) {
		LOG_ERR("Could not open raw socket");
		return -1;
	}

	if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) == -1) {
		LOG_ERR("Could not setsocketopt on raw socket");
		close(s);
		return -1;
	}

	/* send arp request */
	memset(&arp, 0, sizeof(arp));
	memcpy(arp.h_dest, MAC_BCAST_ADDR, 6);		/* MAC DA */
	memcpy(arp.h_source, mac, 6);			/* MAC SA */
	arp.h_proto = htons(ETH_P_ARP);			/* protocol type (Ethernet) */
	arp.htype = htons(ARPHRD_ETHER);		/* hardware type */
	arp.ptype = htons(ETH_P_IP);			/* protocol type (ARP message) */
	arp.hlen = 6;					/* hardware address length */
	arp.plen = 4;					/* protocol address length */
	arp.operation = htons(ARPOP_REQUEST);		/* ARP op code */
	memcpy(arp.sInaddr, &ip, sizeof(ip));		/* source IP address */
	memcpy(arp.sHaddr, mac, 6);			/* source hardware address */
	memcpy(arp.tInaddr, &yiaddr, sizeof(yiaddr));	/* target IP address */

	memset(&addr, 0, sizeof(addr));
	strcpy(addr.sa_data, interface);
	if (sendto(s, &arp, sizeof(arp), 0, &addr, sizeof(addr)) < 0)
		rv = 0;

	/* wait arp reply, and check it */
	/* Modified by xcl, 27Feb12。
	 * 将arp查询时长由2秒修改为0.5秒，尽可能地缓解DUT DHCP Server分配OFFER比WAN端Server慢问题。 
	 */
#if 0 /* xcl */	
	tm.tv_usec = 0;
	prevTime = time(0);
	while (timeout > 0) {
		FD_ZERO(&fdset);
		FD_SET(s, &fdset);
		tm.tv_sec = timeout;
		if (select(s + 1, &fdset, (fd_set *) NULL, (fd_set *) NULL, &tm) < 0) {
			LOG_ERR("Error on ARPING request: %m");
			if (errno != EINTR) rv = 0;
		} else if (FD_ISSET(s, &fdset)) {
			if (recv(s, &arp, sizeof(arp), 0) < 0 ) rv = 0;
			if (arp.operation == htons(ARPOP_REPLY) &&
                /* don't check it: Linux doesn't return proper tHaddr (fixed in 2.6.24?) and vista windows7 don't return tHaddr */
			    /* bcmp(arp.tHaddr, mac, 6) == 0 && */
			    *((uint32_t *) arp.sInaddr) == yiaddr &&
			    (chaddr == NULL || 0 != memcmp(chaddr, arp.sHaddr, 6))) {
				
				LOG_ERR("Valid arp reply receved for this address\n");
				rv = 0;
				break;
			}
		}
		timeout -= time(0) - prevTime;
		prevTime = time(0);
	}
#else /* xcl */
    tm.tv_sec = 0;
	gettimeofday(&timePre, NULL); /* 使用gettimeofday比time函数分辨率更高 */
	while (timeout_usec > 0) {
		FD_ZERO(&fdset);
		FD_SET(s, &fdset);
		tm.tv_usec = timeout_usec;
		if (select(s + 1, &fdset, (fd_set *) NULL, (fd_set *) NULL, &tm) < 0) {
			LOG_ERR("Error on ARPING request: %m");
			if (errno != EINTR) rv = 0;
		} else if (FD_ISSET(s, &fdset)) {
			if (recv(s, &arp, sizeof(arp), 0) < 0 ) rv = 0;
			if (arp.operation == htons(ARPOP_REPLY) &&
                /* don't check it: Linux doesn't return proper tHaddr (fixed in 2.6.24?) and vista windows7 don't return tHaddr */
			    /* bcmp(arp.tHaddr, mac, 6) == 0 && */
			    *((uint32_t *) arp.sInaddr) == yiaddr &&
			    (chaddr == NULL || 0 != memcmp(chaddr, arp.sHaddr, 6))) {
				
				LOG_ERR("Valid arp reply receved for this address\n");
				rv = 0;
				break;
			}
		}
		else
		{
		    close(s);
		    return rv;
        }
        
		gettimeofday(&timeNow, NULL);
		timeout_usec -= ((timeNow.tv_sec - timePre.tv_sec) * 1000000 + timeNow.tv_usec - timePre.tv_usec);
		gettimeofday(&timePre, NULL);
	}
#endif /* xcl */
	close(s);
	LOG_INFO("%salid arp replies for this address", rv ? "No v" : "V");	
	return rv;
}
