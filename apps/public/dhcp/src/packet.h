#ifndef _PACKET_H
#define _PACKET_H

#include "netinet/in.h"
#include "netinet/if_ether.h"

#include <netinet/udp.h>
#include <netinet/ip.h>

#include "common.h"

/* recv buff definition added by tiger 20090922 */
#define DHCP_MAX_MSG_SIZE               1024
#define IPHL       sizeof(struct ip)
#define UDPHL      sizeof(struct udphdr)
#define WORD 		4 	/* Multiplier for IP header length field */
#define DFLTDHCPLEN   sizeof (struct dhcpMessage)   /* default DHCP message size */
#define DHCP_MSG_SIZE (DFLTDHCPLEN + UDPHL + IPHL)
#define DHCP_OPTIONS_BUFSIZE            308
#define UDHCPC_SLACK_FOR_BUGGY_SERVERS  448 /* 1024 - 236 - 312 - 20 - 8 */

struct dhcpMessage {
	uint8_t op;
	uint8_t htype;
	uint8_t hlen;
	uint8_t hops;
	uint32_t xid;
	uint16_t secs;
	uint16_t flags;
	uint32_t ciaddr;
	uint32_t yiaddr;
	uint32_t siaddr;
	uint32_t giaddr;
	uint8_t chaddr[16];
	uint8_t sname[64];
	uint8_t file[128];
	uint32_t cookie;
	uint8_t options[DHCP_OPTIONS_BUFSIZE + UDHCPC_SLACK_FOR_BUGGY_SERVERS]; /* 312 + 448 - cookie */
};

struct udp_dhcp_packet {
	struct iphdr ip;
	struct udphdr udp;
	struct dhcpMessage data;
};

struct ps_udph          /* pseudo udp header for checksum */
    {
    struct in_addr      srcip;
    struct in_addr      dstip;
    char                zero;
    char                prto;
    short               ulen;
    };

void init_header(struct dhcpMessage *packet, char type);
int get_packet(struct dhcpMessage **packet, int fd);

uint16_t checksum(void *addr, int count);
int raw_packet(struct dhcpMessage *payload, uint32_t source_ip, int source_port,
		   uint32_t dest_ip, int dest_port, uint8_t *dest_arp, int ifindex);

int kernel_packet(struct dhcpMessage *payload, uint32_t source_ip, int source_port,
		   uint32_t dest_ip, int dest_port);
#endif
