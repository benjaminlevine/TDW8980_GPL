/*  Copyright(c) 2009-2011 Shenzhen TP-LINK Technologies Co.Ltd.
 *
 * file		dhcps_helper.c
 * brief	common functions for linux and vxworks	
 * details	
 *
 * author	Xu Chenglong
 * version	1.0.0
 * date		18Jul11
 *
 * history 	\arg 1.0.0, 18Jul11, Xu Chenglong, Create file.	
 */
#include <stdio.h> 
#include <stdlib.h>

#include "dhcps.h"
#include "static_leases.h"
#include "leases.h"
#include "os_msg.h"

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
void dhcps_clear_up(CMSG_BUFF *pMsg)
{
    int loop = 0;
    struct option_set *cur, *next;
    DHCPS_RELOAD_MSG_BODY *pMsgBody = (DHCPS_RELOAD_MSG_BODY *)pMsg->content;

    if (pMsgBody->delLanIp != 0 && pMsgBody->delLanMask != 0)
    {
        clear_leases_of_lan(pMsgBody->delLanIp, pMsgBody->delLanMask);
    }
    
    for (loop = 0; iface_configs[loop].in_use ==1 && loop < MAX_GROUP_NUM; loop++)
    {
        /* close bpfDev and skt if is active.*/
        if (iface_configs[loop].fd >=0)
        {
            close(iface_configs[loop].fd);
        }

#ifdef DHCP_RELAY
        if (iface_configs[loop].relay_skt >= 0)
        {
            close(iface_configs[loop].relay_skt);
        }
#endif
        /* options */
	    cur = iface_configs[loop].options;
	    while(cur) 
	    {
		    next = cur->next;
		    if(cur->data)
			    free(cur->data);
		    free(cur);
		    cur = next;
	    }
    }

    memset(iface_configs, 0, sizeof(struct iface_config_t) * loop);
    memset(static_leases, 0, sizeof(struct static_lease) * MAX_STATIC_LEASES_NUM);

    for (loop = 0; loop < MAX_GROUP_NUM; loop++)
	{
	    iface_configs[loop].fd = -1; 
#ifdef DHCP_RELAY	    
	    iface_configs[loop].relay_skt = -1;
#endif	    
	}

#if 0 /* 为什么要清空? */
    memset(leases, 0, sizeof(struct dhcpOfferedAddr) * MAX_CLIENT_NUM);
#endif

    return;
}

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
void dhcps_clear_up_index(int index)
{
    struct option_set *cur, *next;

    /* close bpfDev and skt if is active.*/
    if (iface_configs[index].fd >=0)
    {
        close(iface_configs[index].fd);
    }

#ifdef DHCP_RELAY
    if (iface_configs[index].relay_skt >= 0)
    {
        close(iface_configs[index].relay_skt);
    }
#endif

    /* options */
    cur = iface_configs[index].options;
    while(cur) 
    {
	    next = cur->next;
	    if(cur->data)
		    free(cur->data);
	    free(cur);
	    cur = next;
    }

    memset(&iface_configs[index], 0, sizeof(struct iface_config_t));

    iface_configs[index].fd = -1; 
#ifdef DHCP_RELAY
    iface_configs[index].relay_skt = -1;
#endif

    return;
}

/**************************************************************************************************/
/*                                           PUBLIC_FUNCTIONS                                     */
/**************************************************************************************************/
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
int dhcps_init(void)
{
    int loop = 0; 

	memset(&server_config, 0, sizeof(struct server_config_t));
	memset(iface_configs, 0, sizeof(struct iface_config_t) * MAX_GROUP_NUM);
	memset(leases, 0, sizeof(struct dhcpOfferedAddr) * MAX_CLIENT_NUM);
	memset(static_leases, 0, sizeof(struct static_lease) * MAX_STATIC_LEASES_NUM);

    for (loop = 0; loop < MAX_GROUP_NUM; loop++)
	{
	    iface_configs[loop].fd = -1; 
#ifdef DHCP_RELAY
	    iface_configs[loop].relay_skt = -1;
#endif
	}

	server_config.decline_time = 3600;
	server_config.conflict_time = 3600;
	server_config.offer_time = 60;
	server_config.min_lease = 60;
	
#ifdef DHCP_RELAY
    memset(relay_skts, 0, sizeof(struct relay_skt_t) * MAX_WAN_NUM);
    for (loop = 0; loop < MAX_WAN_NUM; loop++)
    {
       relay_skts[loop].skt= -1;
    }
#endif
	

    strncpy(server_config.lease_file, LEASES_FILE, MAX_FILE_NAME_LEN);
	return 0;
}

void dhcps_dump()
{
    int loop = 0;
    struct iface_config_t *pIf = NULL;
    int dhcps_num = 0;

    for (loop = 0; loop < MAX_GROUP_NUM; loop++)
    {
        pIf = &iface_configs[loop];
        
        if (pIf->in_use == 1)
        {
            printf("%s: fd = %d, server = %0x, dhcps_remote = %0x, relay_ifName = %s.\n", 
                pIf->ifName, pIf->fd, pIf->server, pIf->dhcps_remote, pIf->relay_ifName);
            dhcps_num++;
        }
    }

    printf("DHCPS services for %d lan(s).\n", dhcps_num);
    return;
}

