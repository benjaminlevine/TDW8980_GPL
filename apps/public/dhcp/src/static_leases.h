/* static_leases.h */
#ifndef _STATIC_LEASES_H
#define _STATIC_LEASES_H

#include "dhcps.h"

#if 0
/* Config file will pass static lease info to this function which will add it
 * to a data structure that can be searched later */
int addStaticLease(struct static_lease **lease_struct, uint8_t *mac, uint32_t *ip);
#endif

/* Check to see if a mac has an associated static lease */
uint32_t getIpByMac(struct static_lease *lease_struct, void *arg, 
                    uint32_t lanIp, uint32_t netmask);

/* Check to see if an ip is reserved as a static ip */
uint32_t reservedIp(struct static_lease *lease_struct, uint32_t ip);

#if 0
/* Print out static leases just to check what's going on */
void printStaticLeases(struct static_lease **lease_struct);
#endif

#endif



