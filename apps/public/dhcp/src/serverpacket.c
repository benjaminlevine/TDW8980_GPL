/* serverpacket.c
 *
 * Construct and send DHCP server packets
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
#include <time.h>
 
#include <netinet/in.h>

#include "serverpacket.h"
#include "dhcps.h"
#include "options.h"
#include "common.h"
#include "static_leases.h"

#define DHCPS_HOST_NAME_LEN 64 /*32 xcl: the max length of hostname is 63, 22May12 */

extern int check_ip(struct dhcpMessage *oldpacket, uint32_t addr);

/* send a packet to giaddr using the kernel ip stack */
static int send_packet_to_relay(struct dhcpMessage *payload)
{
	LOG_INFO("Forwarding packet to relay");
	return kernel_packet(payload, cur_iface_config->server, SERVER_PORT,
			payload->giaddr, SERVER_PORT);
}


/* send a packet to a specific arp address and ip address by creating our own ip packet */
static int send_packet_to_client(struct dhcpMessage *payload, int force_broadcast)
{
	uint8_t *chaddr;
	uint32_t ciaddr;

	if (force_broadcast) {
		LOG_DEBUG("Broadcasting packet to client (NAK)");
		ciaddr = INADDR_BROADCAST;
		chaddr = MAC_BCAST_ADDR;
	} else if (payload->ciaddr) {
		LOG_DEBUG("Unicasting packet to client ciaddr");
		ciaddr = payload->ciaddr;
		chaddr = payload->chaddr;
	} else if (ntohs(payload->flags) & BROADCAST_FLAG) {
		LOG_DEBUG("Broadcasting packet to client (requested)");
		ciaddr = INADDR_BROADCAST;
		chaddr = MAC_BCAST_ADDR;
	} else {
		ciaddr = payload->yiaddr;
		chaddr = payload->chaddr;
	}
	
    return raw_packet(payload, cur_iface_config->server, SERVER_PORT,
			ciaddr, CLIENT_PORT, chaddr, cur_iface_config->ifindex);
}


/* send a dhcp packet, if force broadcast is set, the packet will be broadcast to the client */
static int send_packet(struct dhcpMessage *payload, int force_broadcast)
{
	int ret;	
	if (payload->giaddr)
	{
		ret = send_packet_to_relay(payload);
	}
	else  
        ret = send_packet_to_client(payload, force_broadcast);
	return ret;
}


static void init_packet(struct dhcpMessage *packet, struct dhcpMessage *oldpacket, char type)
{
	init_header(packet, type);
	packet->xid = oldpacket->xid;
	memcpy(packet->chaddr, oldpacket->chaddr, 16);
	packet->flags = oldpacket->flags;
	packet->giaddr = oldpacket->giaddr;
	packet->ciaddr = oldpacket->ciaddr;
	add_simple_option(packet->options, DHCP_SERVER_ID, cur_iface_config->server);
}


/* add in the bootp options */
static void add_bootp_options(struct dhcpMessage *packet)
{
	packet->siaddr = cur_iface_config->siaddr;
	if (cur_iface_config->sname)
		strncpy((char *)packet->sname, cur_iface_config->sname, sizeof(packet->sname) - 1);
	if (cur_iface_config->boot_file)
		strncpy((char *)packet->file, cur_iface_config->boot_file, sizeof(packet->file) - 1);
}


/* add conditional optons */
static void add_conditional_options(struct dhcpMessage *pPkg, struct cond_server_pool_t *pCondPool)
{
    unsigned char option[MAX_OPTION_VALUE_LEN + 2] = {0};    

    /* add gateway option */
    option[OPT_CODE] = DHCP_ROUTER;
	option[OPT_LEN] = 4;

	memcpy(option + OPT_DATA, &pCondPool->gateway, 4);

	add_option_string(pPkg->options, option);

    /* unknown add none option */
    if (pCondPool->deviceTpye == UNKNOWN)
    {
        return;
    }
    
    /* STB add dns option */
    if (pCondPool->deviceTpye == STB)
    {
        memset(option, 0, MAX_OPTION_VALUE_LEN + 2);
        
    	option[OPT_CODE] = DHCP_DNS_SERVER;
    	option[OPT_LEN] = 4;
    	memcpy(option + OPT_DATA, &pCondPool->dns[0], 4);
    	
    	if (pCondPool->dns[1] != 0) 
	    {
    		memcpy(option + option[OPT_LEN] + 2, &pCondPool->dns[1], 4);
    		option[OPT_LEN] = 8;
    	}
    	
    	add_option_string(pPkg->options, option);
    }
    
    /* add additional option like 240 */
    if (pCondPool->deviceTpye != UNKNOWN)
    {
        memset(option, 0, MAX_OPTION_VALUE_LEN + 2);
        
        option[OPT_CODE] = pCondPool->optionCode;
    	option[OPT_LEN] = strlen(pCondPool->optionStr);
    	memcpy(option + OPT_DATA, pCondPool->optionStr, option[OPT_LEN]);

        add_option_string(pPkg->options, option);
    }
}

/* send a DHCP OFFER to a DHCP DISCOVER */
int sendOffer(struct dhcpMessage *oldpacket)
{
	struct dhcpMessage packet;
	struct dhcpOfferedAddr *lease = NULL;
	uint32_t req_align, lease_time_align = cur_iface_config->lease;
	uint8_t *req, *lease_time;
	struct option_set *curr;
	char blank_hostname[] = "Unknown";
	char strIp[16] = {0};
	int found = 0;
	
#ifdef DHCP_COND_SRV_POOL
	uint8_t *vendor_id = NULL; 
	struct cond_server_pool_t *pCondPool = NULL;
#endif

	uint8_t host_name[DHCPS_HOST_NAME_LEN];
	uint8_t *host_name_start, *host_name_len;

	uint32_t static_lease_ip;

	init_packet(&packet, oldpacket, DHCPOFFER);

	static_lease_ip = getIpByMac(static_leases, oldpacket->chaddr, 
	                             cur_iface_config->server, cur_iface_config->netmask);

#ifdef DHCP_COND_SRV_POOL
	vendor_id = get_option(oldpacket, DHCP_VENDOR);
	pCondPool = isConditionalVendorId((char *)vendor_id);
#endif

	/* ADDME: if static, short circuit */
	if(!static_lease_ip)
	{
		/* the client is in our lease/offered table */
		if ((lease = find_lease_by_chaddr(oldpacket->chaddr)))
		{
		    /* Added by xcl, 2011-06-01.*/
		    if (/* 修复修改了静态绑定mac之后还会将此ip分配给原先客户端bug */
		        !reservedIp(static_leases, lease->yiaddr) &&
#ifdef DHCP_COND_SRV_POOL		        
		        /* not conditional vid, lease->yiaddr must not in any conditional pools.*/ 
		        ((!pCondPool && !ipInAnyCondPools(ntohl(lease->yiaddr)) && ipInSrvPool(ntohl(lease->yiaddr)))
		        /* conditional vid, yiaddr must be in the conditional pool.*/
		        || (pCondPool && ipInCondPool(ntohl(lease->yiaddr), pCondPool)))
#else
                ipInSrvPool(ntohl(lease->yiaddr))
#endif
            )
		    {
	            if (!lease_expired(lease))
			        lease_time_align = lease->expires - time(0);
		        packet.yiaddr = lease->yiaddr;
		        found = 1;
		    }
		    else/* drop this lease.*/
		    {
		        memset(lease, 0, sizeof(struct dhcpOfferedAddr));
		    }
		    /* End added by xcl, 2011-06-01.*/
		} 
		
		
		if (!found)
		{
		    /* the client has a requested ip */
		    if ((req = get_option(oldpacket, DHCP_REQUESTED_IP)) &&
		    
			memcpy(&req_align, req, 4) &&
			
		   	/* and the ip is in the lease range */
#ifdef DHCP_COND_SRV_POOL
		   	(/*vid not matched, reqIP must in server pool and not in any conditional pools.*/ 
		   	(!pCondPool && !ipInAnyCondPools(ntohl(req_align)) && ipInSrvPool(ntohl(req_align))) 
		   	/*vid matched, reqIp must in the conditional pool.*/
		   	|| (pCondPool && ipInCondPool(ntohl(req_align), pCondPool))) 
		   	&&
#else
            ipInSrvPool(ntohl(req_align)) &&
#endif         

			/* And the requested ip is not reserved by static leases */
			!reservedIp(static_leases, req_align) &&
			
			/* and is not already taken/offered */
		   	((!(lease = find_lease_by_yiaddr(req_align)) ||
		
		   	/* or its taken, but expired */ /* ADDME: or maybe in here */
		   	lease_expired(lease)))) 
    		{
    			packet.yiaddr = req_align; /* FIXME: oh my, is there a host using this IP? */

#if 0 /* Del by xcl, 27Feb12. 解决DUT分配OFFER比WAN端慢问题 */
    			/* check if there is a host using this IP - by lsz 01Sep07 */
    			if (check_ip(oldpacket, req_align))
    				packet.yiaddr = find_address(oldpacket, 0);
    			
    			if (!packet.yiaddr)
    				packet.yiaddr = find_address(oldpacket, 1);
#endif
    			/* otherwise, find a free IP */
    		}
    		else
    		{
    			/* Is it a static lease? (No, because find_address skips static lease) */
    			packet.yiaddr = find_address(oldpacket, 0);
    			/* try for an expired lease */
    			if (!packet.yiaddr) packet.yiaddr = find_address(oldpacket, 1);
    		}
		}

		if(!packet.yiaddr) 
		{
			cmmlog(LOG_NOTICE, LOG_DHCPD, "No ip addresses to give, OFFER abandoned.");
			return -1;
		}

		if (!(host_name_start = get_option(oldpacket, DHCP_HOST_NAME)))
		{
			LOG_DEBUG("Lease host name not found.");
			/* host_name_start = blank_hostname;*/
			memset(host_name, 0, DHCPS_HOST_NAME_LEN);
			memcpy(host_name, blank_hostname, strlen(blank_hostname));
		}
		else
		{	
			host_name_len = host_name_start - OPT_LEN;
			memset(host_name, 0, DHCPS_HOST_NAME_LEN);
            /* fix host name length bug by tiger 20091208 */
			memcpy(host_name, host_name_start, *host_name_len >= DHCPS_HOST_NAME_LEN ? DHCPS_HOST_NAME_LEN -1 : *host_name_len );
		}
		
		if (!add_lease(DHCPOFFER, host_name, packet.chaddr, packet.yiaddr, server_config.offer_time))
		{
			cmmlog(LOG_NOTICE, LOG_DHCPD, "Lease pool is full, OFFER abandoned");
			return -1;
		}

		if ((lease_time = get_option(oldpacket, DHCP_LEASE_TIME))) {
			memcpy(&lease_time_align, lease_time, 4);
			lease_time_align = ntohl(lease_time_align);
			if (lease_time_align > cur_iface_config->lease)
				lease_time_align = cur_iface_config->lease;
		}

		/* Make sure we aren't just using the lease time from the previous offer */
		if (lease_time_align < server_config.min_lease)
			lease_time_align = cur_iface_config->lease;
	}

	/* ADDME: end of short circuit */
	else
	{   
	    /* Added for bug: 为已分配ip的pc添加静态地址分配，pc重新获取ip失败 */
	    if ((lease = find_lease_by_chaddr(oldpacket->chaddr)) != NULL)
	    {
	        memset(lease, 0, sizeof(struct dhcpOfferedAddr));
	    }
	    /* End added */
	
		/* static lease, but check it first, lsz 080220 */
		if (check_ip(oldpacket, static_lease_ip))	/* belongs to someone, we choose another ip */
		{
			packet.yiaddr = find_address(oldpacket, 0);
			/* try for an expired lease */
			if (!packet.yiaddr) packet.yiaddr = find_address(oldpacket, 1);

			if(!packet.yiaddr) 
			{
				cmmlog(LOG_NOTICE, LOG_DHCPD, "No ip addresses to give, OFFER abandoned.");
				return -1;
			}
			
			lease_time_align = cur_iface_config->lease;

			if (!(host_name_start = get_option(oldpacket, DHCP_HOST_NAME)))
			{
				LOG_DEBUG("Lease host name not found.");
				/*host_name_start = blank_hostname;*/
				memset(host_name, 0, DHCPS_HOST_NAME_LEN);
				memcpy(host_name, blank_hostname, strlen(blank_hostname));
			}
			else
			{	
				host_name_len = host_name_start - OPT_LEN;
				memset(host_name, 0, DHCPS_HOST_NAME_LEN);
                /* fix host name length bug by tiger 20091208 */
				memcpy(host_name, host_name_start, *host_name_len >= DHCPS_HOST_NAME_LEN ? DHCPS_HOST_NAME_LEN -1 : *host_name_len);
			}
			
			if (!add_lease(DHCPOFFER, host_name, packet.chaddr, packet.yiaddr, server_config.offer_time))
			{
				cmmlog(LOG_NOTICE, LOG_DHCPD, "Lease pool is full, OFFER abandoned.");
				return -1;
			}
		}
		else
		{
			/* It is a static lease... use it */
			packet.yiaddr = static_lease_ip;

			/* Set lease time to INFINITY for static lease -- by lsz 22Aug07. */
			lease_time_align = 0xFFFFFFFF;
		}
	}

	add_simple_option(packet.options, DHCP_LEASE_TIME, htonl(lease_time_align));

	curr = cur_iface_config->options;
	while (curr) 
	{
		if (curr->data[OPT_CODE] != DHCP_LEASE_TIME
#ifdef DHCP_COND_SRV_POOL
        /* 如果vendorId匹配，则不添加DHCP_ROUTER OPTION */
        && (!pCondPool || curr->data[OPT_CODE] != DHCP_ROUTER)
        /* 如果匹配且为STB则不添加DNS OPTION */
        && (!pCondPool || pCondPool->deviceTpye != STB || curr->data[OPT_CODE] != DHCP_DNS_SERVER)
#endif
		)
		{
		    add_option_string(packet.options, curr->data); 
        }
        
		curr = curr->next;
	}

#ifdef DHCP_COND_SRV_POOL
    /* 添加条件地址池中配置的option */
    if (pCondPool)
    {
        add_conditional_options(&packet, pCondPool);
    }
#endif

	add_bootp_options(&packet);

	udhcp_addrNumToStr(packet.yiaddr, strIp);
	cmmlog(LOG_NOTICE, LOG_DHCPD, "Send OFFER with ip %s", strIp);

	return send_packet(&packet, 0);
}


int sendNAK(struct dhcpMessage *oldpacket)
{
	struct dhcpMessage packet;

	init_packet(&packet, oldpacket, DHCPNAK);
	
	cmmlog(LOG_NOTICE, LOG_DHCPD, "Send NAK");
	return send_packet(&packet, 1);
}


int sendACK(struct dhcpMessage *oldpacket, uint32_t yiaddr)
{
	struct dhcpMessage packet;
	struct option_set *curr;
	uint8_t *lease_time;
	uint32_t lease_time_align = cur_iface_config->lease;
	char blank_hostname[] = "Unknown";
	char strIp[16] = {0};
	
#ifdef DHCP_COND_SRV_POOL
	uint8_t *vendor_id = NULL; 
	struct cond_server_pool_t *pCondPool = NULL;
#endif
	
	uint8_t host_name[DHCPS_HOST_NAME_LEN];
	uint8_t *host_name_start, *host_name_len;

	init_packet(&packet, oldpacket, DHCPACK);
	packet.yiaddr = yiaddr;

	/* Set lease time to INFINITY for static lease -- by lsz 22Aug07. */
	if (reservedIp(static_leases, yiaddr))
	{
		lease_time_align = 0xFFFFFFFF;
	}
	else if ((lease_time = get_option(oldpacket, DHCP_LEASE_TIME)))
	{
		memcpy(&lease_time_align, lease_time, 4);
		lease_time_align = ntohl(lease_time_align);
		
		if (lease_time_align < server_config.min_lease || 
			lease_time_align > cur_iface_config->lease)
			lease_time_align = cur_iface_config->lease;
	}

	
	
	add_simple_option(packet.options, DHCP_LEASE_TIME, htonl(lease_time_align));


#ifdef DHCP_COND_SRV_POOL
	vendor_id = get_option(oldpacket, DHCP_VENDOR);
	pCondPool = isConditionalVendorId((char *)vendor_id);
#endif

	curr = cur_iface_config->options;
	while (curr) 
	{
		if (curr->data[OPT_CODE] != DHCP_LEASE_TIME
#ifdef DHCP_COND_SRV_POOL
        /* 如果vendorId匹配，则不添加DHCP_ROUTER OPTION */
        && (!pCondPool || curr->data[OPT_CODE] != DHCP_ROUTER)
        /* 如果匹配且为STB则不添加DNS OPTION */
        && (!pCondPool || pCondPool->deviceTpye != STB || curr->data[OPT_CODE] != DHCP_DNS_SERVER)
#endif
		)
		{
			add_option_string(packet.options, curr->data);
        }
        
		curr = curr->next;
	}

#ifdef DHCP_COND_SRV_POOL
    /* 添加条件地址池中配置的option */
    if (pCondPool)
    {
        add_conditional_options(&packet, pCondPool);
    }
#endif

	add_bootp_options(&packet);

	udhcp_addrNumToStr(packet.yiaddr, strIp);
	cmmlog(LOG_NOTICE, LOG_DHCPD, "Send ACK to %s", strIp);

	if (send_packet(&packet, 0) < 0)
		return -1;

	if (!(host_name_start = get_option(oldpacket, DHCP_HOST_NAME)))
	{
		LOG_DEBUG("Lease host name not found");
		/* host_name_start = blank_hostname;*/
		memset(host_name, 0, DHCPS_HOST_NAME_LEN);
		memcpy(host_name, blank_hostname, strlen(blank_hostname));
	}
	else
	{
		host_name_len = host_name_start - OPT_LEN;
		memset(host_name, 0, DHCPS_HOST_NAME_LEN);
        /* fix host name length bug by tiger 20091208 */
		memcpy(host_name, host_name_start, *host_name_len >= DHCPS_HOST_NAME_LEN ? DHCPS_HOST_NAME_LEN - 1 : *host_name_len);
	}

	add_lease(DHCPACK, host_name, packet.chaddr, packet.yiaddr, lease_time_align);

	return 0;
}


int send_inform(struct dhcpMessage *oldpacket)
{
	struct dhcpMessage packet;
	struct option_set *curr;

	init_packet(&packet, oldpacket, DHCPACK);

	curr = cur_iface_config->options;
	while (curr) {
		if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
			add_option_string(packet.options, curr->data);
		curr = curr->next;
	}

	add_bootp_options(&packet);

	return send_packet(&packet, 0);
}
