#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <features.h>
#include <time.h>
#if __GLIBC__ >=2 && __GLIBC_MINOR >= 1
#include <netpacket/packet.h>
#include <net/ethernet.h>
#else /* __GLIBC__ >=2 && __GLIBC_MINOR >= 1 */
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#endif /* __GLIBC__ >=2 && __GLIBC_MINOR >= 1 */
#include <errno.h>
#include <stddef.h>

#include <stdlib.h>

#include "packet.h"
#include "dhcps.h"
#include "options.h"
#include "common.h"


void init_header(struct dhcpMessage *packet, char type)
{
	memset(packet, 0, sizeof(struct dhcpMessage));
	switch (type) {
	case DHCPDISCOVER:
	case DHCPREQUEST:
	case DHCPRELEASE:
	case DHCPINFORM:
	case DHCPDECLINE:
		packet->op = BOOTREQUEST;
		break;
	case DHCPOFFER:
	case DHCPACK:
	case DHCPNAK:
		packet->op = BOOTREPLY;
	}
	packet->htype = ETH_10MB;
	packet->hlen = ETH_10MB_LEN;
	packet->cookie = htonl(DHCP_MAGIC);
	packet->options[0] = DHCP_END;
	add_simple_option(packet->options, DHCP_MESSAGE_TYPE, type);
}


int get_packet(struct dhcpMessage **packet, int fd)
{
	int bytes;
	int i;
	const char broken_vendors[][8] = {
		"MSFT 98",
		""
	};
    uint8_t *vendor;

    memset(*packet, 0, sizeof(struct dhcpMessage));
	bytes = read(fd, *packet, sizeof(struct dhcpMessage));

	if (bytes < 0)
	{
	    LOG_WARNING("Invalid pkg.\n");
	    return -1;
	}
    /* 
     * this funciton is used on kernel socket when leasing
     * UDP packet will be truncated if it is too long 
     * at most time, ACK length will not be truncated because raw socket has checked its length in DISCOVER phrase
     * truncated option will be checked in get_option because it check the length of option 
     * so there do not check truncation
     * commented by tiger 20090922
     */

	if (ntohl((*packet)->cookie) != DHCP_MAGIC) {
		LOG_WARNING("Received bogus message, ignoring.");
		return -2;
	}

	if ((*packet)->op == BOOTREQUEST && (vendor = get_option((*packet), DHCP_VENDOR))) 
	{
		for (i = 0; broken_vendors[i][0]; i++) 
		{
			if (vendor[OPT_LEN - 2] == (uint8_t) strlen(broken_vendors[i]) &&
			    !strncmp((char *)vendor, broken_vendors[i], vendor[OPT_LEN - 2])) 
			    {
			    	LOG_DEBUG("Broken client (%s), forcing broadcast.",
			    		broken_vendors[i]);
			    	(*packet)->flags |= htons(BROADCAST_FLAG);
			}
		}
	}

	return bytes;
}

uint16_t checksum(void *addr, int count)
{
	/* Compute Internet Checksum for "count" bytes
	 *         beginning at location "addr".
	 */
	register int32_t sum = 0;
	uint16_t *source = (uint16_t *) addr;

	while (count > 1)  {
		/*  This is the inner loop */
		sum += *source++;
		count -= 2;
	}

	/*  Add left-over byte, if any */
	if (count > 0) {
		/* Make sure that the left-over byte is added correctly both
		 * with little and big endian hosts */
		uint16_t tmp = 0;
		*(uint8_t *) (&tmp) = * (uint8_t *) source;
		sum += tmp;
	}
	/*  Fold 32-bit sum to 16 bits */
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}

/* Construct a ip/udp header for a packet, and specify the source and dest hardware address */
int raw_packet(struct dhcpMessage *payload, uint32_t source_ip, int source_port,
		   uint32_t dest_ip, int dest_port, uint8_t *dest_arp, int ifindex)
{
	int fd;
	int result;
	struct sockaddr_ll dest;
	struct udp_dhcp_packet packet;
	int opt_padding_len = 0;//this original code, following is to fix FR5300 -- lsz 081126
							//sizeof(payload->options) - (end_option(payload->options) + 1);

    enum {
		IP_UPD_DHCP_SIZE = sizeof(struct udp_dhcp_packet) - UDHCPC_SLACK_FOR_BUGGY_SERVERS,
		UPD_DHCP_SIZE    = IP_UPD_DHCP_SIZE - offsetof(struct udp_dhcp_packet, udp),
	};
                            
	if ((fd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP))) < 0) {
		LOG_WARNING("socket call failed: %m");
		return -1;
	}

	memset(&dest, 0, sizeof(dest));
	memset(&packet, 0, sizeof(packet));

	dest.sll_family = AF_PACKET;
	dest.sll_protocol = htons(ETH_P_IP);
	dest.sll_ifindex = ifindex;
	dest.sll_halen = 6;
	memcpy(dest.sll_addr, dest_arp, 6);
	if (bind(fd, (struct sockaddr *)&dest, sizeof(struct sockaddr_ll)) < 0) {
		LOG_ERR("bind call failed: %m");
		close(fd);
		return -1;
	}

	packet.ip.protocol = IPPROTO_UDP;
	packet.ip.saddr = source_ip;
	packet.ip.daddr = dest_ip;
	packet.udp.source = htons(source_port);
	packet.udp.dest = htons(dest_port);
	packet.udp.len = htons(UPD_DHCP_SIZE - opt_padding_len); /* cheat on the psuedo-header */
	packet.ip.tot_len = packet.udp.len;
	memcpy(&(packet.data), payload, sizeof(struct dhcpMessage) - opt_padding_len);
	packet.udp.check = checksum(&packet, IP_UPD_DHCP_SIZE - opt_padding_len);

	packet.ip.tot_len = htons(IP_UPD_DHCP_SIZE - opt_padding_len);
	packet.ip.ihl = sizeof(packet.ip) >> 2;
	packet.ip.version = IPVERSION;
	packet.ip.ttl = IPDEFTTL;

	/* set ip identification, lsz 14Nov08 */
	srand(time(0));
	packet.ip.id = (uint16_t)rand();
	
	packet.ip.check = checksum(&(packet.ip), sizeof(packet.ip));

	result = sendto(fd, &packet, IP_UPD_DHCP_SIZE - opt_padding_len, 0, (struct sockaddr *) &dest, 
sizeof(dest));
	if (result <= 0) {
		LOG_ERR("write on socket failed: %m");
	}
	close(fd);
	return result;
}

/* Let the kernel do all the work for packet generation */
int kernel_packet(struct dhcpMessage *payload, uint32_t source_ip, int source_port,
		   uint32_t dest_ip, int dest_port)
{
	int n = 1;
	int fd, result;
	struct sockaddr_in client;
	int opt_padding_len = 0;/*this original code, following is to fix FR5300 -- lsz 081126*/
							/*sizeof(payload->options) - (end_option(payload->options) + 1);*/
    
    enum {
		DHCP_SIZE = sizeof(struct dhcpMessage) - UDHCPC_SLACK_FOR_BUGGY_SERVERS,
	};
                            
	if ((fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		return -1;

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &n, sizeof(n)) == -1)
	{
		close(fd);
		return -1;
	}

	memset(&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(source_port);
	client.sin_addr.s_addr = source_ip;

	if (bind(fd, (struct sockaddr *)&client, sizeof(struct sockaddr)) == -1)
	{
		close(fd);
		return -1;
	}	

	memset(&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(dest_port);
	client.sin_addr.s_addr = dest_ip;
	
	if (connect(fd, (struct sockaddr *)&client, sizeof(struct sockaddr)) == -1)
	{
		close(fd);
		return -1;
	}	

	result = write(fd, (char *)payload, DHCP_SIZE - opt_padding_len);

	close(fd);
	return result;
}

