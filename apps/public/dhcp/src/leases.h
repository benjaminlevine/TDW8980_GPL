/* leases.h */
#ifndef _LEASES_H
#define _LEASES_H

#include "packet.h"

struct dhcpOfferedAddr {
	uint32_t state;
	uint8_t  host_name[64];
	uint8_t  chaddr[16];	/* 16 bytes is the len of client mac addr in dhcp protocol  */
	uint32_t yiaddr;		/* network order */
	uint32_t expires;		/* host order */
};

extern uint8_t blank_chaddr[];

void clear_leases_of_lan(uint32_t ip, uint32_t mask);
void clear_lease(uint8_t *chaddr, uint32_t yiaddr);

struct dhcpOfferedAddr *add_lease(uint32_t state,
								uint8_t * host_name, 
								uint8_t *chaddr, 
								uint32_t yiaddr, 
								unsigned long lease);

int lease_expired(struct dhcpOfferedAddr *lease);
struct dhcpOfferedAddr *oldest_expired_lease(void);
struct dhcpOfferedAddr *find_lease_by_chaddr(uint8_t *chaddr);
struct dhcpOfferedAddr *find_lease_by_yiaddr(uint32_t yiaddr);
uint32_t find_address(struct dhcpMessage *oldpacket, int check_expired);
int check_ip(struct dhcpMessage *oldpacket, uint32_t addr);
/* Added by xcl, 2011-05-25.*/
struct cond_server_pool_t * isConditionalVendorId(char *vid);
int ipInSrvPool(uint32_t ip);
int ipInCondPool(uint32_t ip, struct cond_server_pool_t *pCondPool);
int ipInAnyCondPools(uint32_t ip);
/* End added by xcl.*/

#endif
