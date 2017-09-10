/*  Copyright(c) 2009-2011 Shenzhen TP-LINK Technologies Co.Ltd.
 *
 * file		dhcps_helper.h
 * brief	
 * details	
 *
 * author	Xu Chenglong
 * version	1.0.0
 * date		18Jul11
 *
 * history 	\arg 1.0.0, 18Jul11, Xu Chenglong, create file.	
 */
#include "dhcps.h"
#include "os_msg.h"

#define DHCPS_SHARED_MEM_SIZE   40960
#define DHCPS_SHARED_MEM_KEY    15200
#define DHCPS_SHARED_SEM_KEY    15211

/* 
 * fn           void dhcps_clear_up(CMSG_BUFF *pMsg)
 * brief	    clear dhcps daemon configuration
 *
 * param[in]	pMsg - pointer to the recieved msg
 * param[out]	N/A
 *
 * return		
 *
 * note		
 */
void dhcps_clear_up(CMSG_BUFF *pMsg);

/* 
 * fn           void dhcps_clear_up_index(int index)
 * brief	    clear up a pointed iface_configs[]
 *
 * param[in]	index - signed the index of iface_configs[] which to clear up
 * param[out]	N/A
 *
 * return		
 *
 * note		
 */
void dhcps_clear_up_index(int index);

/* 
 * fn           void dhcps_init(void)
 * brief	    initial dhcps daemon
 *
 * param[in]	N/A
 * param[out]	N/A
 *
 * return
 * retval       0 - success
 * retval       -1 - fail
 *
 * note		
 */
int dhcps_init(void);
void set_relay(void);

int read_dhcps_config(const char *file);
void write_leases(void);
void read_leases(void);
void write_leases_to_shm(void);
