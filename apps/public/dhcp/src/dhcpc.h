/* dhcpc.h */
#ifndef _DHCPC_H
#define _DHCPC_H

#include "common.h"
#include "clientpacket.h"

#define DHCPC_CONF_FILE     "/var/tmp/dconf/udhcpc.conf"

#define NO_USE  -1
#define INIT_SELECTING	0
#define REQUESTING	1
#define BOUND		2
#define RENEWING	3
#define REBINDING	4
#define INIT_REBOOT	5
#define RENEW_REQUESTED 6
#define RELEASED	7

#define LISTEN_NONE 0
#define LISTEN_KERNEL 1
#define LISTEN_RAW 2

#define MAX_HOST_NAME_LEN 64
#define MAX_CLIENT_ID_LEN 64

/*
 * addeb by tiger 20090304, for reply mode setting 
 */
typedef enum _dhcp_flags_t
{
    DHCP_FLAGS_BROADCAST = 0,
    DHCP_FLAGS_UNICAST
} dhcp_flags_t;

/* 
 * struct to store dhcp information of each wan interface. 
 */
typedef struct client_info_t{
    int skt;
    char ifName[MAX_INTF_NAME];
    int ifindex;
    uint8_t arp[6];
    int state;
    int listen_mode;
    dhcp_flags_t bootp_flags;
    char hostName[MAX_HOST_NAME_LEN + 2];
    char clientId[MAX_CLIENT_ID_LEN + 2];
    unsigned long xid;
    uint32_t requested_ip;
    uint32_t server_addr;
    unsigned long lease;
    unsigned long start;
    unsigned long t1;
    unsigned long t2;
    unsigned long timeout;
    int packet_num;
    int fail_times;
#ifdef INCLUDE_IPV6	/* Add by HYY: support dynamic 6RD, 19Mar12 */
	unsigned char sit6rdEnabled;
#endif /* INCLUDE_IPV6 */
}client_info;

extern struct client_info_t dhcp_clients[MAX_WAN_NUM];

int dhcpc_start(void);

#endif /* _DHCPC_H */
