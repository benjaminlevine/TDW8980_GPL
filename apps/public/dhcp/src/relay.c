#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "relay.h"
#include "socket.h"

#define BUFFER_SIZE   2048

struct dhcp {
	u_int8_t op;
	u_int8_t htype;
	u_int8_t hlen;
	u_int8_t hops;
	u_int32_t xid;
	u_int16_t secs;
	u_int16_t flags;
	u_int32_t ciaddr;
	u_int32_t yiaddr;
	u_int32_t siaddr;
	u_int32_t giaddr;
	u_int8_t chaddr[16];
	u_int8_t srvname[64];
	u_int8_t file[128];
	u_int32_t cookie;
	u_int8_t options[308];
};

static void toSock(int sock, char *buf, int size, struct sockaddr_in *to)
{
	if (sendto(sock, buf, size, 0, (struct sockaddr *) to, sizeof(*to)) == -1){
		LOG_WARNING("Sendto failed.");
	}
}

static void pktHandler(char *buffer, unsigned int ip)
{
	((struct dhcp *) buffer)->hops += 1;

	if (!((struct dhcp *) buffer)->giaddr)
		((struct dhcp *) buffer)->giaddr = ip;
}


void process_relay_request(struct iface_config_t *iface, char *pkt, int len)
{
    int i = 0;

	if ((((struct dhcp *) pkt)->hops & 0xff) > 6)
		return;
		
	pktHandler(pkt, iface->server);

	for (i = 0; i < MAX_WAN_NUM; i++)
    {
        if (relay_skts[i].skt >= 0 && !strcmp(relay_skts[i].ifName, iface->relay_ifName))
        {
            struct sockaddr_in remote_addr;
            
        	memset(&remote_addr, 0, sizeof(remote_addr));
        	remote_addr.sin_family = AF_INET;
        	remote_addr.sin_port = htons(SERVER_PORT);
        	remote_addr.sin_addr.s_addr = iface->dhcps_remote;
        	
        	cmmlog(LOG_NOTICE, LOG_DHCPD, "Relay DHCP REQUEST form(%x) to remote server(%x).", 
        	        relay_skts[i].ip, iface->dhcps_remote);
                
        	toSock(relay_skts[i].skt, pkt, len, &remote_addr);
        }
        break;
    }

    return;
}


void process_relay_response(struct iface_config_t *iface)
{
	int len;
	struct sockaddr_in from;
	unsigned int fromSize = sizeof(from);
	struct dhcpMessage dhcpMsg;
	struct sockaddr_in addr;

    len = recvfrom(iface->relay_skt, &dhcpMsg, sizeof(struct dhcpMessage), 
                    0, (struct sockaddr *) &from, &fromSize);
                    
    if (len == -1) {
		LOG_WARNING("Recvfrom failed.");
		return;
	}
	/* If is a request pkg, ignore it.*/
    if (dhcpMsg.op == BOOTREQUEST)
    {
        LOG_DEBUG("Socket on ip(%x) recieve a req pkg, ignore it.", iface->server);
        return;
    }

    cmmlog(LOG_NOTICE, LOG_DHCPD, "Relay DHCP REPLY to lan(%x).", iface->server);
    
    memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(CLIENT_PORT);
	addr.sin_addr.s_addr = INADDR_BROADCAST;

    toSock(iface->fd, (char *)&dhcpMsg, len, &addr);
    
	return;
}

