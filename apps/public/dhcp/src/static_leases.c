/*
 * static_leases.c -- Couple of functions to assist with storing and
 * retrieving data for static leases
 *
 * Wade Berrier <wberrier@myrealbox.com> September 2004
 *
 */
#include "static_leases.h"
#include "common.h"

/* 
 * Check to see if a mac has an associated static lease.
 * Modified by xcl, if the bind ip is in the same subnet, return the ip, else return 0.
 */
uint32_t getIpByMac(struct static_lease *lease_struct, void *arg, 
                    uint32_t lanIp, uint32_t netmask)
{
    uint32_t return_ip = 0;
    uint8_t *mac = arg;
    int loop = 0;
    
    for (loop = 0; loop < MAX_STATIC_LEASES_NUM && lease_struct[loop].ip != 0; loop++)
    {
        if(memcmp(lease_struct[loop].mac, mac, 6) == 0
		   && (lanIp & netmask) == (lease_struct[loop].ip & netmask))
		{
			return_ip = lease_struct[loop].ip;
			break;
		}

    }

    return return_ip;
}

#if 0
/* Check to see if a mac has an associated static lease */
uint32_t getIpByMac(struct static_lease *lease_struct, void *arg)
{
	uint32_t return_ip;
	struct static_lease *cur = lease_struct;
	uint8_t *mac = arg;

	return_ip = 0;

	while(cur != NULL)
	{
		/* If the client has the correct mac  */
		if(memcmp(cur->mac, mac, 6) == 0)
		{
			return_ip = *(cur->ip);
			break;
		}

		cur = cur->next;
	}

	return return_ip;

}
#endif 

/* Check to see if an ip is reserved as a static ip */
uint32_t reservedIp(struct static_lease *lease_struct, uint32_t ip)
{
	struct static_lease *cur = lease_struct;

	uint32_t return_val = 0;

	while(cur->ip != 0)
	{
		/* If the client has the correct ip  */
		if(cur->ip == ip)
			return_val = 1;

		cur++;
	}

	return return_val;

}

