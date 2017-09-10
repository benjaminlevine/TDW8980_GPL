/* script.c
 *
 * Functions to call the DHCP client notification scripts
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

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "os_msg.h"

#include "options.h"
#include "dhcps.h"
#include "dhcpc.h"
#include "common.h"

#define SUCCESS		0
#define PROCESS     1
#define FAIL		2
#define SUSPEND		3

DHCPC_INFO_MSG_BODY dhcpcCfgMsgBody;

int update_info(char *pIfName, struct dhcpMessage *packet, const char *name)
{
    CMSG_FD msgFd;
	CMSG_BUFF msg;
	
	uint8_t *mask, *gw, *dns;
	uint32_t dns_num = 0, i = 0;

#ifdef INCLUDE_IPV6	/* Add by HYY: support dynamic 6RD, 20Mar12 */
	uint8_t *sit6rd;
#endif /* INCLUDE_IPV6 */

    memset(&msgFd, 0 , sizeof(CMSG_FD));
	memset(&msg, 0 , sizeof(CMSG_BUFF));
	memset(&dhcpcCfgMsgBody, 0, sizeof(DHCPC_INFO_MSG_BODY));
	
	if (name == NULL)
	{
		return -1;
	}

	strncpy(dhcpcCfgMsgBody.ifName, pIfName, MAX_INTF_NAME);

    /* Message Type */
	if (strcmp(name, "renew") == 0 || strcmp(name, "bound") == 0)
	{
	    dhcpcCfgMsgBody.status = SUCCESS;
	}
	else if (strcmp(name, "deconfig") == 0 || strcmp(name, "nak") == 0)
	{
	    dhcpcCfgMsgBody.status = PROCESS;
	    return 0;
	}
	else if (strcmp(name, "release") == 0)
	{
	    dhcpcCfgMsgBody.status = SUSPEND;
	}
	else if (strcmp(name, "leasefail") == 0)	
	{
		dhcpcCfgMsgBody.status = FAIL;
	}
    	else
    	{
        	LOG_DEBUG("DHCPC: Not Support state name!");
        	return -1;
    	}

    /* Message Body */
	if (dhcpcCfgMsgBody.status == SUCCESS && packet != NULL)
	{
		dhcpcCfgMsgBody.ip = packet->yiaddr;
		
		if ((mask = get_option(packet, DHCP_SUBNET)) != NULL)
			memcpy(&(dhcpcCfgMsgBody.mask), mask, 4);
		
		if ((gw = get_option(packet, DHCP_ROUTER)) != NULL)
			memcpy(&(dhcpcCfgMsgBody.gateway), gw, 4);
		
		if ((dns = get_option(packet, DHCP_DNS_SERVER)) != NULL)
		{
			dns_num = (*(dns - 1))/4;
			dns_num = dns_num > 2 ? 2 : dns_num;
			
			for (i = 0; i < dns_num; i++)
			{
				memcpy(dhcpcCfgMsgBody.dns + i, dns + 4 * i, 4);
			}
		}
		
#ifdef INCLUDE_IPV6	/* Add by HYY: support dynamic 6RD, 20Mar12 */
		if ((sit6rd = get_option(packet, DHCP_6RD)) != NULL)
		{
			dhcpcCfgMsgBody.sit6rdInfo.ipv4MaskLen = *sit6rd;
			dhcpcCfgMsgBody.sit6rdInfo.sit6rdPrefixLen = *(sit6rd + 1);
			memcpy(&(dhcpcCfgMsgBody.sit6rdInfo.sit6rdPrefix), sit6rd + 2, 16);
			memcpy(&(dhcpcCfgMsgBody.sit6rdInfo.sit6rdBRIPv4Addr), sit6rd + 18, 4);
		}
#endif /* INCLUDE_IPV6 */

		/*config_interface(&info);*/
	}

    LOG_DEBUG("DHCPC: DNS = %x, %x", dhcpcCfgMsgBody.dns[0], dhcpcCfgMsgBody.dns[1]);
    msg.type = CMSG_DHCPC_STATUS;
    memcpy(msg.content, &dhcpcCfgMsgBody, sizeof(DHCPC_INFO_MSG_BODY));
    
    msg_connCliAndSend(CMSG_ID_COS, &msgFd, &msg);

	return 0;
}
/***********************************************************************/



/* Call a script with a par file and env vars */
void run_script(char *pIfName, struct dhcpMessage *packet, const char *name)
{
	update_info(pIfName, packet, name);
		
	#if 0	/* change by Li Shaozhang 19Jun97 for wr742n */
	int pid;
	char **envp, **curr;

	if (client_config.script == NULL)
		return;

	DEBUG(LOG_INFO, "vforking and execle'ing %s", client_config.script);

	envp = fill_envp(packet);
	/* call script */
	pid = vfork();
	if (pid)
	{
		waitpid(pid, NULL, 0);
		for (curr = envp; *curr; curr++) free(*curr);
		free(envp);
		return;
	}
	else if (pid == 0)
	{
		/* close fd's? */

		
		/* exec script */
		execle(client_config.script, client_config.script,
		       name, NULL, envp);
		LOG(LOG_ERR, "script %s failed: %m", client_config.script);		
		exit(1);
	}
	#endif
}
