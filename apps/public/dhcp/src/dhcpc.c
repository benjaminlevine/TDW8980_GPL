/* dhcpc.c
 *
 * udhcp DHCP client
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
#include <sys/time.h>
#include <sys/file.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>

#include "arpping.h"

#include <stdio.h>

#include "os_msg.h"

#include "dhcpc_init.h"
#include "dhcps.h"
#include "dhcpc.h"
#include "options.h"
#include "clientpacket.h"
#include "clientsocket.h"
#include "script.h"
#include "socket.h"
#include "common.h"

static void perform_start(DHCPC_CFG_MSG_BODY *pMsgBody);
static void perform_renew(DHCPC_CFG_MSG_BODY *pMsgBody);
static void perform_release(DHCPC_CFG_MSG_BODY *pMsgBody, int sendFrame);
static void perform_shutdown(DHCPC_CFG_MSG_BODY *pMsgBody);
static void perform_macClone(DHCPC_CFG_MSG_BODY *pMsgBody);

struct client_info_t dhcp_clients[MAX_WAN_NUM];

static void change_mode(struct client_info_t *pCli, int new_mode)
{
	if (pCli->skt >= 0) 
	    close(pCli->skt);
	    
	pCli->skt = -1;
	pCli->listen_mode = new_mode;

    /* Added by xcl, 20Mar12. 修复RENEW时监听offer包过迟问题 */
	if (pCli->skt < 0 && pCli->listen_mode != LISTEN_NONE)
    {
        if (pCli->listen_mode == LISTEN_KERNEL)
        {
            pCli->skt = listen_socket(INADDR_ANY, CLIENT_PORT, pCli->ifName);
        }
        else if (pCli->listen_mode == LISTEN_RAW)
        {
	        pCli->skt = raw_socket(pCli->ifindex);
	    }   
        if (pCli->skt < 0)
        {
            LOG_ERR("DHCPC: Couldn't create listen_fd on interface %s", pCli->ifName);
            memset(pCli, 0, sizeof(struct client_info_t));
            pCli->state = NO_USE;
        }

        LOG_DEBUG("DHCPC: Create listend_fd(%d) on interface(%s)", 
                pCli->skt, pCli->ifName);
    }   
    /* end added */
}

static void perform_start(DHCPC_CFG_MSG_BODY *pMsgBody)
{
    int loop = 0;
    struct client_info_t *pClient = NULL;
    int len = 0;
    int exist = 0;

    LOG_DEBUG("Perform a DHCPC Start");
    
    /* 如果已经存在相应接口 */
    for (loop = 0; loop < MAX_WAN_NUM; loop++)
    {
        if (dhcp_clients[loop].state != NO_USE
            && !strcmp(dhcp_clients[loop].ifName, pMsgBody->ifName))
        {
            pClient = &dhcp_clients[loop];
            exist = 1;
            break;
        }
    }
    
    /* 如果不存在，找到可用的dhcp_clients[]结构 */
    if (NULL == pClient)
    {
        for (loop = 0; loop < MAX_WAN_NUM; loop++)
        {
            if (dhcp_clients[loop].state == NO_USE)
            {
                pClient = &dhcp_clients[loop];
                break;
            }    
        }
    }    

    /* 初始化结构 */
    if (pClient)
    {
        if (!exist)
        {
            /* ifName */
            strncpy(pClient->ifName, pMsgBody->ifName, MAX_INTF_NAME);
        
            /* get mac */
            getMacFromIfName(pClient->ifName, pClient->arp);
        }    

        /* state */
        pClient->state = INIT_SELECTING;

        /* bootp_flags */
        pClient->bootp_flags = (pMsgBody->unicast == 0 ? DHCP_FLAGS_BROADCAST : DHCP_FLAGS_UNICAST);

        /* hostname */
        pClient->hostName[OPT_CODE] = DHCP_HOST_NAME;
        len = strlen(pMsgBody->hostName); 
        pClient->hostName[OPT_LEN] = len > MAX_HOST_NAME_LEN ? MAX_HOST_NAME_LEN : len;
        strncpy(&(pClient->hostName[OPT_DATA]), pMsgBody->hostName, MAX_HOST_NAME_LEN);
        pClient->hostName[pClient->hostName[OPT_LEN] + 2] = '\0';

        /* clientId */
        pClient->clientId[OPT_CODE] = DHCP_CLIENT_ID;
		pClient->clientId[OPT_LEN] = 7;
		pClient->clientId[OPT_DATA] = 1;
		memcpy(&(pClient->clientId[OPT_DATA + 1]),  pClient->arp, 6);
		pClient->clientId[pClient->clientId[OPT_LEN] + 2] = '\0';

		getIfIndexFormifName(pClient->ifName, &(pClient->ifindex));

        /* listen_mode */
        change_mode(pClient, LISTEN_RAW);

		/* start things over */
	    pClient->packet_num = 0;

	    /* Kill any timeouts because the user wants this to hurry along */
	    pClient->timeout = 0;
#ifdef INCLUDE_IPV6	/* Add by HYY: support dynamic 6RD, 19Mar12 */
		pClient->sit6rdEnabled = pMsgBody->sit6rdEnabled;
#endif /* INCLUDE_IPV6 */
    }   
    else
    {
        LOG_DEBUG("DHCPC: Support %d wans at most.", MAX_WAN_NUM);
    }

    return;
}

/* perform a renew */
static void perform_renew(DHCPC_CFG_MSG_BODY *pMsgBody)
{
	int loop = 0;
	struct client_info_t *cur_client = NULL;
	int len = 0;

    LOG_DEBUG("Perform a DHCP Renew.");
    
    for (loop = 0; loop < MAX_WAN_NUM; loop++)
    {
        if (dhcp_clients[loop].state != NO_USE && !strcmp(dhcp_clients[loop].ifName, pMsgBody->ifName))
        {
            cur_client = &dhcp_clients[loop];
            break;
        }
    }

    if (NULL == cur_client)
    {
        LOG_DEBUG("DHCPC: (%s) Client is not running, perform start instead.", pMsgBody->ifName);
        perform_start(pMsgBody);
        return;
    }

    /* "释放"后再"更新"，要重新设置unicast和hostname */
	if (cur_client->state == RELEASED)
	{
        /* bootp_flags */
        cur_client->bootp_flags = (pMsgBody->unicast == 0 ? DHCP_FLAGS_BROADCAST : DHCP_FLAGS_UNICAST);

        /* hostname */
        cur_client->hostName[OPT_CODE] = DHCP_HOST_NAME;
        len = strlen(pMsgBody->hostName); 
        cur_client->hostName[OPT_LEN] = len > MAX_HOST_NAME_LEN ? MAX_HOST_NAME_LEN : len;
        strncpy(&(cur_client->hostName[OPT_DATA]), pMsgBody->hostName, MAX_HOST_NAME_LEN);
        cur_client->hostName[cur_client->hostName[OPT_LEN] + 2] = '\0';
	}

	switch (cur_client->state) {
	case BOUND:
	    change_mode(cur_client, LISTEN_KERNEL);
	case RENEWING:
	case REBINDING:
		cur_client->state = RENEW_REQUESTED;
		break;
	case RENEW_REQUESTED: /* impatient are we? fine, square 1 */
		run_script(cur_client->ifName, NULL, "deconfig");
	case REQUESTING:
	case RELEASED:
	    change_mode(cur_client, LISTEN_RAW);
		cur_client->state = INIT_SELECTING;
		break;
	case INIT_SELECTING:
		break;
	}

	/* start things over */
	cur_client->packet_num = 0;

	/* Kill any timeouts because the user wants this to hurry along */
	cur_client->timeout = 0;
}


/* perform a release */
static void perform_release(DHCPC_CFG_MSG_BODY *pMsgBody, int sendFrame)
{
    int loop = 0;
    char buffer1[16] = {0};
    char buffer2[16] = {0};
	uint32_t temp_addr = 0;
	struct client_info_t *cur_client = NULL;

	LOG_DEBUG("Perform a DHCP Release.");
    
    for (loop = 0; loop < MAX_WAN_NUM; loop++)
    {
        if (dhcp_clients[loop].state != NO_USE && !strcmp(dhcp_clients[loop].ifName, pMsgBody->ifName))
        {
            cur_client = &dhcp_clients[loop];
            break;
        }
    }

    if (NULL == cur_client)
    {
        LOG_DEBUG("DHCPC: (%s) Client is not running, do noting.", pMsgBody->ifName);
        return;
    }
    else
    {
    	/* send release packet */
    	if (cur_client->state == BOUND || cur_client->state == RENEWING 
    	    || cur_client->state == REBINDING) 
    	{
    		temp_addr = cur_client->server_addr;
    		udhcp_addrNumToStr(temp_addr, buffer1);
    		temp_addr = cur_client->requested_ip;
    		udhcp_addrNumToStr(temp_addr, buffer2);
    		
    		cmmlog(LOG_INFORM, LOG_DHCPC, "Unicasting a release of %s to %s", buffer2, buffer1);
    		
    		if (send_release(cur_client, cur_client->server_addr, cur_client->requested_ip, sendFrame) < 0) 
    		{
    			LOG_DEBUG("DHCPC: (%s) Send_release error", cur_client->ifName);
    		}

            /* 
             * Must wait 1 second before send msg informing CMM to down wan interface because
             * before send release pkg, wan interface should send arp request pkg to get the mac
             * of the server.
             */
            sleep(1);
    		run_script(cur_client->ifName, NULL, "release");
    	}
    	
        change_mode(cur_client, LISTEN_NONE);
    	cur_client->state = RELEASED;
    	cur_client->timeout = 0x7fffffff;
    }

    return;
}

static void perform_shutdown(DHCPC_CFG_MSG_BODY *pMsgBody)
{
    /* 可能同时删除多条连接，故用以下代码 */
#if 0
    int loop = 0;
    
    for (loop = 0; loop < MAX_WAN_NUM; loop++)
    {
        if (!strcmp(dhcp_clients[loop].ifName, pMsgBody->ifName))
        {
            break;
        }
    }

    /* 释放资源 */
    if (loop != MAX_WAN_NUM)
    {
        close(dhcp_clients[loop].skt);
        memset(&dhcp_clients[loop], 0, sizeof(struct client_info_t));
        dhcp_clients[loop].skt = -1;
        dhcp_clients[loop].state = NO_USE;
    }
#endif    

    LOG_DEBUG("Perform a DHCP Shutdown.");
    delete_client();
}

static void perform_macClone(DHCPC_CFG_MSG_BODY *pMsgBody)
{
    int loop = 0;
    struct client_info_t *pClient = NULL;

    LOG_DEBUG("Perform a DHCP MacClone.");

    /* release */
    perform_release(pMsgBody, 1);

    for (loop = 0; loop < MAX_WAN_NUM; loop++)
    {
        if (dhcp_clients[loop].state != NO_USE && !strcmp(dhcp_clients[loop].ifName, pMsgBody->ifName))
        {
            pClient = &dhcp_clients[loop];
            break;
        }
    }

    if (NULL == pClient)
    {
        LOG_DEBUG("DHCPC: Connection type of wan(%s) is not DHCP.", pMsgBody->ifName);
        return;
    }
    
    /* retrieve mac */
    getMacFromIfName(pClient->ifName, pClient->arp);

    /* update class id */
    pClient->clientId[OPT_CODE] = DHCP_CLIENT_ID;
	pClient->clientId[OPT_LEN] = 7;
	pClient->clientId[OPT_DATA] = 1;
	memcpy(&(pClient->clientId[OPT_DATA + 1]),  pClient->arp, 6);
	pClient->clientId[pClient->clientId[OPT_LEN] + 2] = '\0';

    pClient->state = INIT_SELECTING;
    change_mode(pClient, LISTEN_RAW);
	pClient->packet_num = 0;
	pClient->timeout = 0;

    return;
}


int initSelectRfds(fd_set *pRfds, int msgFd)
{
    int loop = 0;
    int maxFd = 0;
    struct client_info_t *pCur = NULL;
    
    FD_ZERO(pRfds);

    /* Set message fd */
    FD_SET(msgFd, pRfds);
    maxFd = msgFd;

    /* set socket fd */
    for (loop = 0; loop < MAX_WAN_NUM; loop++)
    {
        pCur = &dhcp_clients[loop];
        if (pCur->state != NO_USE && pCur->skt >= 0)
        {
            FD_SET(pCur->skt, pRfds);
		    if (pCur->skt > maxFd)
		    {
		        maxFd = pCur->skt;
            }
        }
    }

    return maxFd;
}


/* 070707, LiShaozhang add the unicast opt */
int dhcpc_start(void)
{
	uint8_t *temp, *message;
	fd_set rfds;
	int retval;
	struct timeval tv;
	struct timeval tp_timeout;
	int len;
    struct dhcpMessage dhcpPkg;
    struct dhcpMessage *pDhcpPkg;
	struct in_addr temp_addr;
	long now;
	int max_fd;
	int loop = 0; 
	unsigned long min_timeout = 0x7fffffff;
	int index_of_min_timeout = 0;
	struct client_info_t *cur_client = NULL;

	char strIp1[16] = {0};
	char strIp2[16] = {0};

	CMSG_FD msgFd;
   	CMSG_BUFF msg;

   	LOG_DEBUG("DHCP Client start.");

    msg_init(&msgFd);
    msg_srvInit(CMSG_ID_DHCPC, &msgFd);

    init_dhcpc();
    
	/* run_script(NULL, "deconfig"); */

	for(;;) 
	{
        max_fd = 0;
        FD_ZERO(&rfds);

        /* Set message fd */
        FD_SET(msgFd.fd, &rfds);
        max_fd = max_fd > msgFd.fd ? max_fd : msgFd.fd;
        
        for (loop = 0; loop < MAX_WAN_NUM; loop++)
        {
            cur_client = &dhcp_clients[loop];
            if (cur_client->state != NO_USE) 
            {
                if (cur_client->skt < 0 && cur_client->listen_mode != LISTEN_NONE)
                {
                    if (cur_client->listen_mode == LISTEN_KERNEL)
                    {
                        cur_client->skt = listen_socket(INADDR_ANY, CLIENT_PORT, cur_client->ifName);
                    }
                    else if (cur_client->listen_mode == LISTEN_RAW)
                    {
				        cur_client->skt = raw_socket(cur_client->ifindex);
				    }    
                    if (cur_client->skt < 0)
                    {
                        LOG_ERR("DHCPC: Couldn't create listen_fd on interface %s", cur_client->ifName);
                        memset(cur_client, 0, sizeof(struct client_info_t));
                        cur_client->state = NO_USE;
                    }

                    LOG_DEBUG("DHCPC: Create listend_fd(%d) on interface(%s)", 
                            cur_client->skt, cur_client->ifName);
		     
			    }
                if (cur_client->skt >= 0)
			    {
    			    FD_SET(cur_client->skt, &rfds);
    			    if (cur_client->skt > max_fd)
    			        max_fd = cur_client->skt;
    			}
    		}
    		
        }

        /* Find the min timeout.*/
        min_timeout = 0x7fffffff;
        for (loop = 0; loop < MAX_WAN_NUM; loop++)
        {
            if (dhcp_clients[loop].state != NO_USE && dhcp_clients[loop].timeout < min_timeout)
            {    
                min_timeout = dhcp_clients[loop].timeout;
                index_of_min_timeout = loop;
            }    
        }
        
        tv.tv_sec = min_timeout - time(0);
		tv.tv_usec = 0;
        
        /* 
         * select don't return when timeout value is larger than 1.5 hours
         * we just wait multiple times
         * added by tiger 090819, should fix later
         */
        #define TP_TIMEOUT_MAX  (30*60)
		if (tv.tv_sec > 0) 
		{
            do
            {
                max_fd = initSelectRfds(&rfds, msgFd.fd);
            
                tp_timeout.tv_sec = (tv.tv_sec > TP_TIMEOUT_MAX) ? TP_TIMEOUT_MAX : tv.tv_sec;
                tv.tv_sec -= tp_timeout.tv_sec;                                
                tp_timeout.tv_usec = 0;
                
                retval = select(max_fd + 1, &rfds, NULL, NULL, &tp_timeout);
            } while (tv.tv_sec > 0 && retval == 0);
		}
		else
		{
			retval = 0; /* If we already timed out, fall through */
		}
        /* End added.*/

		now = time(0);
		if (retval == 0) 
		{
		    /* dhcp_clients[index_of_min_timeout] has the min timeout, first treat it.*/
		    cur_client = &dhcp_clients[index_of_min_timeout];

		    /*if timeout, need to resend pkts, else do noghing.*/
	        if (cur_client->timeout < time(0))
		    {
    		    /* timeout dropped to zero */
    			switch (cur_client->state) 
    			{
    			case INIT_SELECTING:
                   
#define     DISCOVER_RETRY_TIMES    5
#define     DISCOVER_INVERT_TIMES   3

    				if (cur_client->packet_num < DISCOVER_RETRY_TIMES) 
    				{
    					if (cur_client->packet_num == 0)
    					{
    						cur_client->xid = random_xid();                         
    					}

                        /* change runtime dhcp flags when exceed DISCOVER_INVERT_TIMES added by tiger 20090819 apply 11G and XP's option */
                        if (DISCOVER_INVERT_TIMES == cur_client->packet_num)
                        { 
                            if (DHCP_FLAGS_UNICAST == cur_client->bootp_flags)
                                cur_client->bootp_flags = DHCP_FLAGS_BROADCAST;
                            else
                                cur_client->bootp_flags = DHCP_FLAGS_UNICAST;
                        }
                        
    					/* send discover packet */
    					udhcp_addrNumToStr(cur_client->requested_ip, strIp1);
    					cmmlog(LOG_NOTICE, LOG_DHCPC, "Send DISCOVER with request ip %s and unicast flag %d", 
    					    /*cur_client->ifName, */strIp1, cur_client->bootp_flags); 
    					send_discover(cur_client, cur_client->xid, cur_client->requested_ip);                   
                        
    					cur_client->timeout = time(0) + ((cur_client->packet_num == 2) ? 4 : 2);
    					cur_client->packet_num++;
    				} 
    				else 
    				{
    				    cmmlog(LOG_NOTICE, LOG_DHCPC, "Recv no OFFER, DHCP Service unavailable"/*, 
    					    cur_client->ifName*/);
    					run_script(cur_client->ifName, NULL, "leasefail");
    					    
    					/* wait to try again */
    					cur_client->packet_num = 0;
    					cur_client->timeout = time(0) + 10 + (cur_client->fail_times++) * 30;	
    				}
    				break;
    			case RENEW_REQUESTED:
    			case REQUESTING:
    				if (cur_client->packet_num < 3) 
    				{
    					/* send request packet */
    					udhcp_addrNumToStr(cur_client->server_addr, strIp1);
    					udhcp_addrNumToStr(cur_client->requested_ip, strIp2);
    					cmmlog(LOG_NOTICE, LOG_DHCPC, "Send REQUEST to server %s with request ip %s", 
                            /*cur_client->ifName, */strIp1, strIp2);
                            
    					if (cur_client->state == RENEW_REQUESTED)
    					{   
    						send_renew(cur_client, cur_client->xid, cur_client->server_addr, 
    						    cur_client->requested_ip); /* unicast */
    					}
    					else
    					{ 
    						send_selecting(cur_client, cur_client->xid, cur_client->server_addr, 
    						    cur_client->requested_ip); /* broadcast */
    					}

    					cur_client->timeout = time(0) + ((cur_client->packet_num == 2) ? 10 : 2);
    					cur_client->packet_num++;
    				} 
    				else 
    				{
    					/* timed out, go back to init state */
    					if (cur_client->state == RENEW_REQUESTED) 
    					    run_script(cur_client->ifName, NULL, "deconfig");
    					cur_client->state = INIT_SELECTING;
    					cur_client->timeout = time(0);
    					cur_client->packet_num = 0;
    					change_mode(cur_client, LISTEN_RAW);
    				}
    				break;
    			case BOUND:
    				/* Lease is starting to run out, time to enter renewing state */
    				cur_client->state = RENEWING; 
    				change_mode(cur_client, LISTEN_KERNEL);
    				LOG_DEBUG("DHCPC: (%s) Entering renew state", cur_client->ifName);
    				/* fall right through */
    			case RENEWING:
    				/* Either set a new T1, or enter REBINDING state */
    				if ((cur_client->t2 - cur_client->t1) <= (cur_client->lease / 14400 + 1)) 
    				{
    					/* timed out, enter rebinding state */
    					cur_client->state = REBINDING;
    					cur_client->timeout = time(0) + (cur_client->t2 - cur_client->t1);
    					LOG_DEBUG("DHCPC: (%s) Entering rebinding state", cur_client->ifName);
    				} 
    				else 
    				{
    					/* send a request packet */	
    					udhcp_addrNumToStr(cur_client->server_addr, strIp1);
    					udhcp_addrNumToStr(cur_client->requested_ip, strIp2);
    					cmmlog(LOG_NOTICE, LOG_DHCPC, "Send REQUEST to server %s with request ip %s", 
                            /*cur_client->ifName, */strIp1, strIp2);
                            
    					send_renew(cur_client, cur_client->xid, cur_client->server_addr, 
    					    cur_client->requested_ip); /* unicast */
                  
    					cur_client->t1 = (cur_client->t2 - cur_client->t1) / 2 + cur_client->t1;
    					cur_client->timeout = cur_client->t1 + cur_client->start;
    				}
    				break;
    			case REBINDING:
    				/* Either set a new T2, or enter INIT state */
    				if ((cur_client->lease - cur_client->t2) <= (cur_client->lease / 14400 + 1)) 
    				{
    				    LOG_DEBUG("DHCPC: (%s) Lease lost, entering init state", cur_client->ifName);

    					/* timed out, enter init state */
    					cur_client->state = INIT_SELECTING;
    					run_script(cur_client->ifName, NULL, "deconfig");
    					cur_client->timeout = time(0);
    					cur_client->packet_num = 0;
    					change_mode(cur_client, LISTEN_RAW);
    				} 
    				else 
    				{
    					/* send a request packet */
    					udhcp_addrNumToStr(cur_client->server_addr, strIp1);
    					udhcp_addrNumToStr(cur_client->requested_ip, strIp2);
    					cmmlog(LOG_NOTICE, LOG_DHCPC, "Broadcast REQUEST with request ip %s", 
                            /*cur_client->ifName, */strIp2);
                            
    					send_renew(cur_client, cur_client->xid, 0, cur_client->requested_ip); /* broadcast */
    					
    					cur_client->t2 = (cur_client->lease - cur_client->t2) / 2 + cur_client->t2;
    					cur_client->timeout = cur_client->t2 + cur_client->start;
    				}
    				break;
    			case RELEASED:
    				/* yah, I know, *you* say it would never happen */
    				cur_client->timeout = 0x7fffffff;
    				break;
    			}
		    }
		}
		else if (retval > 0 && !FD_ISSET(msgFd.fd, &rfds))
		{   
		    /* not signal */
            for (loop = 0; loop < MAX_WAN_NUM; loop++)
            {
                if (dhcp_clients[loop].state != NO_USE 
                    && dhcp_clients[loop].skt >= 0 
                    && dhcp_clients[loop].listen_mode != LISTEN_NONE
                    && FD_ISSET(dhcp_clients[loop].skt, &rfds))
                {    
                    break;
                }    
            }
            if (loop == MAX_WAN_NUM)
            {
                continue;
            }
            cur_client = &dhcp_clients[loop];
            
            pDhcpPkg = &dhcpPkg;
            if (cur_client->listen_mode == LISTEN_KERNEL)
				len = get_packet(&pDhcpPkg, cur_client->skt);
			else
			    len = get_raw_packet(pDhcpPkg, cur_client->skt);

			if (len == -1 && errno != EINTR)
			{
				LOG_DEBUG("DHCPC: Error on read, reopening socket");
				change_mode(cur_client, cur_client->listen_mode);/* just close and reopen */
			}
			if (len < 0) 
			{   
			    /* 这里不需要reopen socket。get_raw_packet收到非dhcp报文也会返回<0，若重新去打开socket 
			     * ,在线路比较复杂的环境下，可能会错过dhcp回复包，导致拨号失败。31Mar12, xcl.
			     */
			    /*change_mode(cur_client, cur_client->listen_mode); *//* just close and reopen */
			    continue;
			}    

			if (pDhcpPkg->xid != cur_client->xid) 
			{
				cmmlog(LOG_NOTICE, LOG_DHCPC, "Ignoring XID %lx (our xid is %lx)",
					/*cur_client->ifName, */(unsigned long) pDhcpPkg->xid, cur_client->xid);
				continue;
			}
			
			/* Ignore packets that aren't for us */
			if (memcmp(pDhcpPkg->chaddr, cur_client->arp, 6)) 
			{
				LOG_DEBUG("DHCPC: (%s) Packet does not have our chaddr -- ignoring", cur_client->ifName);
				continue;
			}

			if ((message = get_option(pDhcpPkg, DHCP_MESSAGE_TYPE)) == NULL) 
			{
				LOG_DEBUG("DHCPC: Couldnt get option from packet -- ignoring");
				continue;
			}

			switch (cur_client->state)
			{
			case INIT_SELECTING:
				/* Must be a DHCPOFFER to one of our xid's */
				if (*message == DHCPOFFER) 
				{
					if ((temp = get_option(pDhcpPkg, DHCP_SERVER_ID))) 
					{
						memcpy(&cur_client->server_addr, temp, 4);
						cur_client->xid = pDhcpPkg->xid;
						cur_client->requested_ip = pDhcpPkg->yiaddr;

                        udhcp_addrNumToStr(cur_client->server_addr, strIp1);
    					udhcp_addrNumToStr(cur_client->requested_ip, strIp2);
						cmmlog(LOG_NOTICE, LOG_DHCPC, "Recv OFFER from server %s with ip %s", 
						    /*cur_client->ifName, */strIp1, strIp2);

						/* 如果获取到的IP与lan口在同一网段，重新获取 */
                        if (1 == chkIpInAnyLanSubnet(cur_client->requested_ip))
                        {
                            cmmlog(LOG_NOTICE, LOG_DHCPC, "IP(%s) is in the same subnet with lan, ignore it.", 
                                strIp2);
                            cur_client->timeout = time(0) + 2;
                            break;
                        }    
                        
						/* enter requesting state */
						cur_client->state = REQUESTING;
						cur_client->timeout = time(0);
						cur_client->packet_num = 0;
					} 
					else 
					{
						LOG_DEBUG("DHCPC: No server ID in message");
					}
				}
				break;
			case RENEW_REQUESTED:
			case REQUESTING:
			case RENEWING:
			case REBINDING:
				if (*message == DHCPACK) 
				{
					if (!(temp = get_option(pDhcpPkg, DHCP_LEASE_TIME))) 
					{
						LOG_DEBUG("DHCPC: No lease time with ACK, using 1 hour lease");
						cur_client->lease = 60 * 60;
					} 
					else 
					{
						memcpy(&cur_client->lease, temp, 4);
						cur_client->lease = ntohl(cur_client->lease);
					}


/* RFC 2131 3.1 paragraph 5:
 * "The client receives the DHCPACK message with configuration
 * parameters. The client SHOULD perform a final check on the
 * parameters (e.g., ARP for allocated network address), and notes
 * the duration of the lease specified in the DHCPACK message. At this
 * point, the client is configured. If the client detects that the
 * address is already in use (e.g., through the use of ARP),
 * the client MUST send a DHCPDECLINE message to the server and restarts
 * the configuration process..." 
 * added by tiger 20090827
 */
					if (!arpping(pDhcpPkg->yiaddr, (uint32_t)0, 
					             pDhcpPkg->yiaddr, cur_client->arp, cur_client->ifName))
                    {
					
						cmmlog(LOG_NOTICE, LOG_DHCPC, "Offered address is in use, Send decline"/*,  
                                cur_client->ifName*/);
                                
						send_decline(cur_client,  cur_client->xid,  cur_client->server_addr, pDhcpPkg->yiaddr);

                        change_mode(cur_client, LISTEN_RAW);
						cur_client->state = INIT_SELECTING;
						cur_client->requested_ip = 0;
						cur_client->timeout = now + 12;
						cur_client->packet_num = 0;
						continue; /* back to main loop */
                    }

					/* enter bound state */
					cur_client->t1 = cur_client->lease / 2;

					/* little fixed point for n * .875 */
					cur_client->t2 = (cur_client->lease * 0x7) >> 3;
					temp_addr.s_addr = pDhcpPkg->yiaddr;
						
					cur_client->start = time(0);
					cur_client->timeout = cur_client->t1 + cur_client->start;
					cur_client->requested_ip =pDhcpPkg->yiaddr;

					if ((temp = get_option(pDhcpPkg, DHCP_SERVER_ID))) 
						memcpy(&cur_client->server_addr, temp, 4);

					udhcp_addrNumToStr(cur_client->server_addr, strIp1);
    				udhcp_addrNumToStr(cur_client->requested_ip, strIp2);	
					cmmlog(LOG_NOTICE, LOG_DHCPC, "Recv ACK from server %s with ip %s lease time %ld", 
						/*cur_client->ifName, */strIp1, strIp2, cur_client->lease);
        					
					run_script(cur_client->ifName, pDhcpPkg,
						   ((cur_client->state == RENEWING || cur_client->state == REBINDING) ? "renew" : "bound"));

					cur_client->fail_times = 0;		/* clear the retry counter */
					cur_client->state = BOUND;
					change_mode(cur_client, LISTEN_NONE);
				} 
				else if (*message == DHCPNAK) 
				{
					/* return to init state */
					if ((temp = get_option(pDhcpPkg, DHCP_SERVER_ID))) 
						memcpy(&cur_client->server_addr, temp, 4);

					udhcp_addrNumToStr(cur_client->server_addr, strIp1);
    				udhcp_addrNumToStr(cur_client->requested_ip, strIp2);	
					cmmlog(LOG_NOTICE, LOG_DHCPC, "Recv NAK from server %s with ip %s", 
					    /*cur_client->ifName, */strIp1, strIp2);

					run_script(cur_client->ifName, pDhcpPkg, "nak");
					if (cur_client->state != REQUESTING)
						run_script(cur_client->ifName, NULL, "deconfig");
						
					cur_client->state = INIT_SELECTING;
					cur_client->timeout = time(0) + 3;	/* change by lsz 080905, without this 3 seconds,
										 * the udhcpc will keep on trying and the release
										 * msg cant be recved by udhcpc, even if we are
										 * wan static ip now, the udhcpc is still sending 
										 * discover pkts. 
										 */
					cur_client->requested_ip = 0;
					cur_client->packet_num = 0;
					change_mode(cur_client, LISTEN_RAW);
					/*sleep(3); *//* avoid excessive network traffic */
				}
				break;
			}
		}
		else if (retval > 0 && FD_ISSET(msgFd.fd, &rfds))
		{   
		    DHCPC_CFG_MSG_BODY *pMsgBody;
		    
            msg_recv(&msgFd, &msg);
            pMsgBody = (DHCPC_CFG_MSG_BODY *)msg.content;
            
            switch (msg.type)
            {
            case CMSG_DHCPC_START:
                LOG_DEBUG("DHCPC: Recv START msg");
                perform_start(pMsgBody);   
                break;
                
            case CMSG_DHCPC_RENEW:
                LOG_DEBUG("DHCPC: Recv RENEW msg");
                perform_renew(pMsgBody);
                break;
                
            case CMSG_DHCPC_RELEASE:
                LOG_DEBUG("DHCPC: Recv RELEASE msg");
                perform_release(pMsgBody, 0);
                break;
                
            case CMSG_DHCPC_SHUTDOWN:
                LOG_DEBUG("DHCPC: Recv SHUTDOWN msg");
                perform_shutdown(pMsgBody);
                break;

            case CMSG_DHCPC_MAC_CLONE:
                LOG_DEBUG("DHCPC: Recv MAC_CLONE msg");
                perform_macClone(pMsgBody);
                break;
                
            default:
                LOG_DEBUG("DHCPC: Not-supported Msg Type.");
                break;
            }
		}
		else 
		{
			/* An error occured */
			LOG_DEBUG("Error on select");
		}
	}

	return 1;
}


int main()
{
    if (daemon(0, 1) < 0)
	{
		perror("daemon");
		exit(-1);
	}

	/* Wait for system up */
	sleep(2);

	/* Infinite loop */
    dhcpc_start();

	return 0;
}

