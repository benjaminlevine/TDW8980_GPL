/*
 * leases.c -- tools to manage DHCP leases
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 */
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "arpping.h"

#include "dhcps.h"
#include "options.h"
#include "leases.h"
#include "common.h"
#include "static_leases.h"

uint8_t blank_chaddr[16] = {0};
uint8_t blank_host_name[32] = {0};	/* add by Li Shaozhang 07Jun07 */

/* 删除指定lan段的租期 */
void clear_leases_of_lan(uint32_t ip, uint32_t mask)
{
    int loop = 0;
    uint32_t subnet = ip & mask;

    LOG_DEBUG("DHCPS: Del LEASE of lan(%0X, %0X).", ip, mask);
    
    if (0 == ip || 0 == mask)
    {
        return;
    }
    
    for (loop = 0; loop < MAX_CLIENT_NUM; loop++)
    {
        if ((leases[loop].yiaddr & mask) == subnet)
        {
            memset(&leases[loop], 0, sizeof(struct dhcpOfferedAddr));
        }
    }

    return;
}

/* clear every lease out that chaddr OR yiaddr matches and is nonzero */
void clear_lease(uint8_t *chaddr, uint32_t yiaddr)
{
	unsigned int i, j;

	for (j = 0; j < 16 && !chaddr[j]; j++);

	for (i = 0; i < MAX_CLIENT_NUM; i++)
		if ((j != 16 && !memcmp(leases[i].chaddr, chaddr, 16)) ||
		    (yiaddr && leases[i].yiaddr == yiaddr)) {
			memset(&(leases[i]), 0, sizeof(struct dhcpOfferedAddr));
		}
}


/* add a lease into the table, clearing out any old ones */
struct dhcpOfferedAddr *add_lease(uint32_t state, 
								uint8_t * host_name,
								uint8_t *chaddr,
								uint32_t yiaddr,
								unsigned long lease)
{
	struct dhcpOfferedAddr *oldest;

	/* clean out any old ones */
	clear_lease(chaddr, yiaddr);

	oldest = oldest_expired_lease();
	if (oldest) {
		oldest->state = state;
		memcpy(oldest->host_name, host_name, 64);	/* add by Li Shaozhang, 07Jun07 */
		memcpy(oldest->chaddr, chaddr, 16);
		oldest->yiaddr = yiaddr;

		/* lease + time(0) may overflow */
		if (lease > 0xFFFFFFFF - time(0))
			oldest->expires = 0xFFFFFFFF;
		else
			oldest->expires = time(0) + lease;
	}

	return oldest;
}


/* true if a lease has expired */
int lease_expired(struct dhcpOfferedAddr *lease)
{
	return (lease->expires < (unsigned long) time(0));
}


/* Find the oldest expired lease, NULL if there are no expired leases */
struct dhcpOfferedAddr *oldest_expired_lease(void)
{
	struct dhcpOfferedAddr *oldest = NULL;
	unsigned long oldest_lease = time(0);
	unsigned int i;


	for (i = 0; i < MAX_CLIENT_NUM; i++)
		if (oldest_lease > leases[i].expires) {
			oldest_lease = leases[i].expires;
			oldest = &(leases[i]);
		}
	return oldest;

}


/* 
 * Find the first lease that matches chaddr, NULL if no match.
 * Modified by xcl, if the lease ip is not in the same subnet, deleted it and return null.
 */
struct dhcpOfferedAddr *find_lease_by_chaddr(uint8_t *chaddr)
{
	unsigned int i;

	for (i = 0; i < MAX_CLIENT_NUM; i++)
		if (!memcmp(leases[i].chaddr, chaddr, 16)) 
        {   
            /* If not in the very subnet, delete this lease. Modified by xcl.*/
            if ((leases[i].yiaddr & cur_iface_config->netmask) != 
                (cur_iface_config->server & cur_iface_config->netmask))
            {
                memset(&leases[i], 0, sizeof(struct dhcpOfferedAddr));
            }
            else
            {
		        return &(leases[i]);
		    }
        }

	return NULL;
}

#if 0
/* Find the first lease that matches chaddr, NULL if no match */
struct dhcpOfferedAddr *find_lease_by_chaddr(uint8_t *chaddr)
{
	unsigned int i;

	for (i = 0; i < cur_iface_config->max_leases; i++)
		if (!memcmp(cur_iface_config->leases[i].chaddr, chaddr, 16)) 
		    return &(cur_iface_config->leases[i]);

	return NULL;
}
#endif

/* Find the first lease that matches yiaddr, NULL is no match */
struct dhcpOfferedAddr *find_lease_by_yiaddr(uint32_t yiaddr)
{
	unsigned int i;

	for (i = 0; i < MAX_CLIENT_NUM; i++)
		if (leases[i].yiaddr == yiaddr) return &(leases[i]);

	return NULL;
}


/* check is an IP is taken, if it is, add it to the lease table */
int check_ip(struct dhcpMessage *oldpacket, uint32_t addr)
{
	uint8_t* mac = NULL;
	char strIp[10] = {0};

    /* Added for bug: dut会将lan IP(若在地址池范围内)分配出去，若收到此IP的Client无
     * 免费arp检查功能，将导致IP冲突. 2011-12-27, xcl.
     */
	if (addr == cur_iface_config->server)
	{
		LOG_DEBUG("The IP distributed equals to server ip, ignore!!!");
	    return 1;
	}
	/* End added */

	mac = oldpacket->chaddr;

    if (arpping(addr, mac, cur_iface_config->server, 
	        cur_iface_config->arp, cur_iface_config->ifName) == 0)
	{
		udhcp_addrNumToStr(addr, strIp);
		LOG_DEBUG("%s belongs to someone, reserving it for %ld seconds\n",
			strIp, server_config.conflict_time);
		add_lease(DHCPNULL, blank_host_name, blank_chaddr, addr, server_config.conflict_time);
		return 1;
	} else return 0;
}

/* Check if an ip is in dhcp server pool or not. */
int ipInSrvPool(uint32_t ip)
{
    if (ip >= ntohl(cur_iface_config->start)
        && ip <= ntohl(cur_iface_config->end)
        /* Added for bug: client获取到ip 192.168.1.100，然后再设置dut lan ip192.168.1.100,
         * client重新获取ip，DUT为client分配的ip将仍为192.168.1.100，若client无免费arp检查功能，
         * 会导致IP冲突. 2011-12-27, xcl.
         */
        && ip != cur_iface_config->server
        /* End added */
        )
    {
        return 1;
    }

    LOG_DEBUG("IP(%0X) not in dhcp pool or equal to server ip, ignore!!!", ip);
    return 0;
}

#ifdef DHCP_COND_SRV_POOL
/* Judge the vendor id is a conditional vendor id or not.*/
struct cond_server_pool_t * isConditionalVendorId(char *vid)
{
    struct cond_server_pool_t *pCondPool = NULL;
    char classVendorStr[MAX_VENDOR_ID_LEN] = {0};
    unsigned char len = 0;
    int loop = 0;

    if (!vid) return NULL;
    len = *(unsigned char*)(vid - OPT_LEN);

    memcpy(classVendorStr, vid, len >= MAX_VENDOR_ID_LEN ? MAX_VENDOR_ID_LEN -1 : len );

    pCondPool = &(cur_iface_config->cond_server_pools[0]);
    for (loop = 0; loop < MAX_COND_SERVER_POOLS && 
         pCondPool->vendor_id[0] != 0; pCondPool++)
    {
        if (strstr(classVendorStr, pCondPool->vendor_id)) 
        {
			return pCondPool;
		}
    }

    return NULL;   
}

/* Check if an ip is in the specific conditional pool or not. */
int ipInCondPool(uint32_t ip, struct cond_server_pool_t *pCondPool)
{
    if (ip >= ntohl(pCondPool->start)
        && ip <= ntohl(pCondPool->end))
    {
        return 1;
    }

    return 0;
}

/* Judge if an ip address is in conditional pools or not. Parameter addr must be host order.*/
int ipInAnyCondPools(uint32_t ip)
{
    int index = 0;
    
    for (index = 0; index < MAX_COND_SERVER_POOLS 
         && cur_iface_config->cond_server_pools[index].vendor_id[0] != 0; index++)
    {
        if (ip >= ntohl(cur_iface_config->cond_server_pools[index].start)
            && ip <= ntohl(cur_iface_config->cond_server_pools[index].end))
        {
            return 1;
        }
    }

    return 0;
}
#endif

/* find an assignable address, it check_expired is true, we check all the expired leases as well.
 * Maybe this should try expired leases by age... */
uint32_t find_address(struct dhcpMessage *oldpacket, int check_expired)
{
	uint32_t addrStart, addrEnd, ret;
	struct dhcpOfferedAddr *lease = NULL;

#ifdef DHCP_COND_SRV_POOL
	struct cond_server_pool_t *pCondPool = NULL;
	uint8_t *vendor_id = NULL;

	vendor_id = get_option(oldpacket, DHCP_VENDOR);
	pCondPool = isConditionalVendorId((char *)vendor_id);

    /* Calculate thd address pool.*/
	if (pCondPool)
    {
        addrStart = ntohl(pCondPool->start);
        addrEnd = ntohl(pCondPool->end);
    }
    else
    {
        addrStart = ntohl(cur_iface_config->start);
        addrEnd = ntohl(cur_iface_config->end);
    }
#else
    addrStart = ntohl(cur_iface_config->start);
    addrEnd = ntohl(cur_iface_config->end);
#endif

    for (; addrStart <= addrEnd; addrStart++)/* Modified by xcl, 2011-06-01.*/
	{
		/* ie, 192.168.55.0 */
		if (!(addrStart & 0xFF)) continue;

		/* ie, 192.168.55.255 */
		if ((addrStart & 0xFF) == 0xFF) continue;

		/* lan ip, added by xcl, 04Jun12*/
		if (htonl(addrStart) == cur_iface_config->server) continue;
		
		/* Only do if it isn't an assigned as a static lease */
		if(!reservedIp(static_leases, htonl(addrStart))
#ifdef  DHCP_COND_SRV_POOL
		   && (pCondPool || (!pCondPool && !ipInAnyCondPools(addrStart)))
#endif		   
	    )
		{
			/* lease is not taken */
			ret = htonl(addrStart);

#if 0 /* xcl: 除了分配已过期ip, 发送offer之前不进行arp查询，而放到发送ACK前进行, 04May12 */			
			if ((!(lease = find_lease_by_yiaddr(ret)) ||

			     /* or it expired and we are checking for expired leases */
			     (check_expired  && lease_expired(lease))) &&

			     /* and it isn't on the network */
		    	     !check_ip(oldpacket, ret)) 
#else
            if (!(lease = find_lease_by_yiaddr(ret)) ||
			     /* or it expired and we are checking for expired leases */
			     (check_expired  && lease_expired(lease) && !check_ip(oldpacket, ret)))
#endif
		    {
				return ret;
				break;
			}
		}
	}
	return 0;
}
