/* clientpacket.c
 *
 * Packet generation and dispatching functions for the DHCP client.
 *
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <string.h>
#include <sys/socket.h>
#include <features.h>
#if __GLIBC__ >=2 && __GLIBC_MINOR >= 1
#include <netpacket/packet.h>
#include <net/ethernet.h>
#else
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#endif
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "packet.h"
#include "dhcps.h"
#include "clientpacket.h"
#include "options.h"
#include "dhcpc.h"
#include "common.h"


/* Create a random xid */
unsigned long random_xid(void)
{
	static int initialized;
	if (!initialized) {
		/* int fd; */
		unsigned long seed;
#if 0
		fd = open("/dev/urandom", 0);
		if (fd < 0 || read(fd, &seed, sizeof(seed)) < 0) {
			LOG(LOG_WARNING, "Could not load seed from /dev/urandom: %m");
			seed = time(0);
		}
		if (fd >= 0) close(fd);
#endif  
        seed = time(0);
		srand(seed);
		initialized++;
	}
	return rand();
}


/* Multi-wans support: add parameter int i for all function. By xcl, 2011-05-04.*/
/* initialize a packet with the proper defaults */
static void init_packet(struct client_info_t *pClient, struct dhcpMessage *packet, char type)
{
	struct vendor  {
		char vendor, length;
		char str[sizeof("MSFT 5.0")];
	} vendor_id = { DHCP_VENDOR,  sizeof("MSFT 5.0") - 1, "MSFT 5.0"};
	/* Changed by lsz 080424, cheat server:"I am Windows XP" */
	/* vendor_id = { DHCP_VENDOR,  sizeof("udhcp "VERSION) - 1, "udhcp "VERSION};*/
	

	init_header(packet, type);	/* message type */
	memcpy(packet->chaddr, pClient->arp, 6);

	/* moved by tiger 20090304, from send_discover, flags should be setting for all packet in that mode */
	/* Modified by Li Shaozhang, 070707 */
	/* Multi-wans support.*/
	if (DHCP_FLAGS_UNICAST == pClient->bootp_flags/*get_runtime_dhcp_flags()*/)			/* server reply mode choose */
		packet->flags &= htons(0x7FFF);	/* set first bit to 0, just AND 0111 1111 1111 1111 */
	else						/* server reply in broadcast mode */
		packet->flags |= htons(0x8000);	/* set first bit to 1, just OR  1000 0000 0000 0000 */

    /* Edited by xcl, 13Feb12. 
     * According to rfc2131, release packet must include options 53¡¢54 and may 61, must not others.
     * decline packet is most the same but must include options 50.
     */
    if (DHCPRELEASE != type && DHCPDECLINE != type)
    {
    	/* 080501, add maximum size option --- lsz */
        /* Explicitly saying that we want RFC-compliant packets helps
    	 * some buggy DHCP servers to NOT send bigger packets */
    	add_simple_option(packet->options, DHCP_MAX_SIZE, htons(DHCP_MAX_MSG_SIZE));
    }	

	if (pClient->clientId[OPT_DATA])
	    add_option_string(packet->options, (uint8_t *)pClient->clientId);/* client id */
	    
	/* Edited by xcl, 13Feb12. 
     * According to rfc2131, release and decline packet must not include option 12.
     */
	if (DHCPRELEASE != type && DHCPDECLINE != type && pClient->hostName[OPT_DATA]) 
		add_option_string(packet->options, (uint8_t *)pClient->hostName);/* hostname */

    /* Edited by xcl, 13Feb12. 
     * According to rfc2131, release and decline packet must not include option 60.
     */
	if (DHCPRELEASE != type && DHCPDECLINE != type)	
	    add_option_string(packet->options, (uint8_t *) &vendor_id);	/* vendor id */
}


/* Add a parameter request list for stubborn DHCP servers. Pull the data
 * from the struct in options.c. Don't do bounds checking here because it
 * goes towards the head of the packet. */
static void add_requests(struct dhcpMessage *packet, struct client_info_t *pClient)
{
	int end = end_option(packet->options);
	int i, len = 0;

	packet->options[end + OPT_CODE] = DHCP_PARAM_REQ;
	for (i = 0; dhcp_options[i].code; i++)
		if (dhcp_options[i].flags & OPTION_REQ)
			packet->options[end + OPT_DATA + len++] = dhcp_options[i].code;
#ifdef INCLUDE_IPV6	/* Add by HYY: support dynamic 6RD, 19Mar12 */
	if (pClient->sit6rdEnabled)
	{
		packet->options[end + OPT_DATA + len++] = DHCP_6RD;
	}
#endif /* INCLUDE_IPV6 */
	packet->options[end + OPT_LEN] = len;
	packet->options[end + OPT_DATA + len] = DHCP_END;

}


/* Broadcast a DHCP discover packet to the network, with an optionally requested IP */
int send_discover(void *pClient, unsigned long xid, unsigned long requested)
{
	struct dhcpMessage packet;
	struct client_info_t *pCli = (struct client_info_t *)pClient;

	init_packet(pCli, &packet, DHCPDISCOVER);
	packet.xid = xid;

	/* 080424, del by lsz, cauz some server may ignore our request with an requested ip */
	#if 0
	if (requested)
		add_simple_option(packet.options, DHCP_REQUESTED_IP, requested);
	#endif
	
	add_requests(&packet, pCli);

	return raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST,
				SERVER_PORT, MAC_BCAST_ADDR, pCli->ifindex);
}


/* Broadcasts a DHCP request message */
int send_selecting(void *pClient, unsigned long xid, unsigned long server, unsigned long requested)
{
	struct dhcpMessage packet;
	struct in_addr addr;
	struct client_info_t *pCli = (struct client_info_t *)pClient;

	init_packet(pCli, &packet, DHCPREQUEST);
	packet.xid = xid;

	add_simple_option(packet.options, DHCP_REQUESTED_IP, requested);
	add_simple_option(packet.options, DHCP_SERVER_ID, server);

	add_requests(&packet, pCli);
	addr.s_addr = requested;

	return raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST,
				SERVER_PORT, MAC_BCAST_ADDR, pCli->ifindex);
}


/* Unicasts or broadcasts a DHCP renew message */
int send_renew(void *pClient, unsigned long xid, unsigned long server, unsigned long ciaddr)
{
	struct dhcpMessage packet;
	int ret = 0;
	struct client_info_t *pCli = (struct client_info_t *)pClient;

	init_packet(pCli, &packet, DHCPREQUEST);
	packet.xid = xid;
	packet.ciaddr = ciaddr;

	add_requests(&packet, pCli);
 
	if (server)
		ret = kernel_packet(&packet, ciaddr, CLIENT_PORT, server, SERVER_PORT);
	else ret =  raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST,
				SERVER_PORT, MAC_BCAST_ADDR, pCli->ifindex);
 
	return ret;
}


/* Unicasts a DHCP release message */
int send_release(void *pClient, unsigned long server, unsigned long ciaddr, int sendFrame)
{
	struct dhcpMessage packet;
	struct client_info_t *pCli = (struct client_info_t *)pClient;

	init_packet(pCli, &packet, DHCPRELEASE);
	packet.xid = random_xid();
	packet.ciaddr = ciaddr;

    /* Del by xcl, 13Feb12. According to rfc2131, release packet must not include option 50 */
	/*add_simple_option(packet.options, DHCP_REQUESTED_IP, ciaddr);*/
	add_simple_option(packet.options, DHCP_SERVER_ID, server);

    if (!sendFrame)
	    return kernel_packet(&packet, ciaddr, CLIENT_PORT, server, SERVER_PORT);
	else
        return raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST,
				SERVER_PORT, MAC_BCAST_ADDR, pCli->ifindex);
}

/* send broadcast when GARP checking failed added by tiger 20090825 */
int send_decline(void *pClient, uint32_t xid, uint32_t server, uint32_t requested)
{
	struct dhcpMessage packet;
	struct client_info_t *pCli = (struct client_info_t *)pClient;

	init_packet(pCli, &packet, DHCPDECLINE);
	packet.xid = xid;
	add_simple_option(packet.options, DHCP_REQUESTED_IP, requested);
	add_simple_option(packet.options, DHCP_SERVER_ID, server);

	return raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST,
				SERVER_PORT, MAC_BCAST_ADDR, pCli->ifindex);
}

/* return -1 on errors that are fatal for the socket, -2 for those that aren't */
int get_raw_packet(struct dhcpMessage *payload, int fd)
{
	int bytes;
	struct udp_dhcp_packet packet;
	uint32_t source, dest;
	uint16_t check;

	memset(&packet, 0, sizeof(struct udp_dhcp_packet));
	bytes = read(fd, &packet, sizeof(struct udp_dhcp_packet));
	if (bytes < 0) {
		LOG_DEBUG("couldn't read on raw listening socket -- ignoring");
		usleep(500000); /* possible down interface, looping condition */
		return -1;
	}

	if (bytes < (int) (sizeof(struct iphdr) + sizeof(struct udphdr))) {
		LOG_DEBUG("message too short, ignoring");
		return -2;
	}

	if (bytes < ntohs(packet.ip.tot_len)) {
		LOG_DEBUG("Truncated packet");
		return -2;
	}

	/* ignore any extra garbage bytes */
	bytes = ntohs(packet.ip.tot_len);

	/* Make sure its the right packet for us, and that it passes sanity checks */
	if (packet.ip.protocol != IPPROTO_UDP || packet.ip.version != IPVERSION ||
	    packet.ip.ihl != sizeof(packet.ip) >> 2 || packet.udp.dest != htons(CLIENT_PORT) ||
	    bytes > (int) sizeof(struct udp_dhcp_packet) ||
	    ntohs(packet.udp.len) != (uint16_t) (bytes - sizeof(packet.ip))) {
	    	LOG_DEBUG("unrelated/bogus packet");
	    	return -2;
	}

	/* check IP checksum */
	check = packet.ip.check;
	packet.ip.check = 0;
	if (check != checksum(&(packet.ip), sizeof(packet.ip))) {
		LOG_DEBUG("bad IP header checksum, ignoring");
		return -1;
	}

	/* verify the UDP checksum by replacing the header with a psuedo header */
	source = packet.ip.saddr;
	dest = packet.ip.daddr;
	check = packet.udp.check;
	packet.udp.check = 0;
	memset(&packet.ip, 0, sizeof(packet.ip));

	packet.ip.protocol = IPPROTO_UDP;
	packet.ip.saddr = source;
	packet.ip.daddr = dest;
	packet.ip.tot_len = packet.udp.len; /* cheat on the psuedo-header */
	if (check && check != checksum(&packet, bytes)) {
		LOG_DEBUG("packet with bad UDP checksum received, ignoring");
		return -2;
	}

	memcpy(payload, &(packet.data), bytes - (sizeof(packet.ip) + sizeof(packet.udp)));

	if (ntohl(payload->cookie) != DHCP_MAGIC) {
		LOG_ERR("DHCPC: received bogus message (bad magic) -- ignoring");
		return -2;
	}
	
	LOG_DEBUG("oooooh!!! got some!");
	return bytes - (sizeof(packet.ip) + sizeof(packet.udp));
}
