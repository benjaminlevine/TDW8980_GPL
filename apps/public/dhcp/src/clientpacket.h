#ifndef _CLIENTPACKET_H
#define _CLIENTPACKET_H

#include "dhcpc.h"
#include "packet.h"

unsigned long random_xid(void);

int send_discover(void *pClient, unsigned long xid, unsigned long requested);
int send_selecting(void *pClient, unsigned long xid, unsigned long server, unsigned long requested);
int send_renew(void *pClient, unsigned long xid, unsigned long server, unsigned long ciaddr);
int send_release(void *pClient, unsigned long server, unsigned long ciaddr, int sendFrame);
int send_decline(void *pClient,  uint32_t xid, uint32_t server, uint32_t requested);

int get_raw_packet(struct dhcpMessage *payload, int fd);

#endif

