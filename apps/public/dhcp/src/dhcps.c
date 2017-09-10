/* dhcps.c
 *
 * udhcp Server
 * Copyright (C) 1999 Matthew Ramsay <matthewr@moreton.com.au>
 *			Chris Trew <ctrew@moreton.com.au>
 *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
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
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <time.h>

#include <stdio.h>
#include <stdlib.h>

#include "os_msg.h"
#include "dhcps.h"
#include "arpping.h"
#include "socket.h"
#include "options.h"
#include "serverpacket.h"
#include "common.h"
#include "static_leases.h"
#include "leases.h"
#include "dhcps_helper.h"
#include "br_port.h"
#include "relay.h"

#define DISABLE_PORTNAME_PREFIX     "nas"

/* global */
struct server_config_t server_config; 
struct iface_config_t iface_configs[MAX_GROUP_NUM]; 
struct dhcpOfferedAddr leases[MAX_CLIENT_NUM];
struct iface_config_t *cur_iface_config;
struct static_lease static_leases[MAX_STATIC_LEASES_NUM];
struct dhcpOfferedAddr *leases_shm;

#ifdef DHCP_RELAY
struct relay_skt_t relay_skts[MAX_WAN_NUM];
#endif

/* 
 * fn		int portDisabled(const char *ifName, struct dhcpMessage* packet)
 * brief	check if the dhcp request packet comes from wan interface or not 
 * details	
 *
 * param[in]	ifName - bridge name
 * param[in]    packet - the dhcp packeg
 * param[out]	N/A
 *
 * return	int
 * retval	0 - not from wan interface
 * retval   1 - from wan interface
 *
 * note		
 */
int portDisabled(const char *ifName, struct dhcpMessage* packet)
{	
	char portName[MAX_PORT_NAME_LEN]={0};
	
	if (getPortNameFromMacAddr(ifName, packet->chaddr, portName) != 0)
	{
		return 0;
	}

    LOG_DEBUG("DHCPS: DHCP package from interface:%s", portName);

    if (NULL != strstr(portName, DISABLE_PORTNAME_PREFIX))
    {
        return 1;
    }
	
	return 0;
}


int check_and_ack(struct dhcpMessage* packet, uint32_t ip)
{
	uint32_t static_ip = 0;
	
#ifdef DHCP_COND_SRV_POOL
    struct cond_server_pool_t *pCondPool = NULL;
	uint8_t *vendor_id;
#endif
	
	/* There is an Static IP for this guy, whether it is requested or not */
	if ((static_ip = getIpByMac(static_leases, packet->chaddr, 
	                            cur_iface_config->server, cur_iface_config->netmask)) != 0)
	{
		cmmlog(LOG_NOTICE, LOG_DHCPD, "REQUEST ip %x, static ip %x", ip, static_ip);
		return sendACK(packet, static_ip);
	}

	/* requested ip is reserved by a static lease -- if it is himself, situation above match */
	if (reservedIp(static_leases, ip))
	{
		cmmlog(LOG_NOTICE, LOG_DHCPD, "REQUEST ip %x is reserved as a static ip", ip);
		return sendNAK(packet);
	}
	
	/* if some one reserve it */
	if ( ip != packet->ciaddr && check_ip(packet, ip) )
	{
		cmmlog(LOG_NOTICE, LOG_DHCPD, "REQUEST ip %x already reserved by someone", ip);
		return sendNAK(packet);
	}

#ifdef DHCP_COND_SRV_POOL
    vendor_id = get_option(packet, DHCP_VENDOR);
    pCondPool = isConditionalVendorId((char *)vendor_id);
#endif

	/*if (ntohl(ip) < ntohl(cur_iface_config->start) || ntohl(ip) > ntohl(cur_iface_config->end))*/
#ifdef DHCP_COND_SRV_POOL
	if ( (pCondPool && !(ipInCondPool(ntohl(ip), pCondPool)))
	    || (!pCondPool && (!ipInSrvPool(ntohl(ip)) || ipInAnyCondPools(ntohl(ip)))))
#else
    if (!ipInSrvPool(ntohl(ip)))
#endif    
	{
		cmmlog(LOG_NOTICE, LOG_DHCPD, "REQUEST ip %x is not in the address pool", ip);
		return sendNAK(packet);
	}
	
	return sendACK(packet, ip);
}

int main(int argc, char *argv[])
{
	fd_set rfds;
	int bytes, retval;
    struct dhcpMessage dhcpPkg;
    struct dhcpMessage *pDhcpPkg;
	uint8_t *state;
	uint8_t *server_id, *requested;
	uint32_t server_id_align, requested_align;
	struct dhcpOfferedAddr *lease;
	struct dhcpOfferedAddr static_lease;
	int max_sock;
	uint32_t static_lease_ip;
	int loop = 0;
#ifdef DHCP_COND_SRV_POOL
    struct cond_server_pool_t *pCondPool = NULL;
	uint8_t *vendor_id = NULL;
#endif

	CMSG_FD msgFd;
   	CMSG_BUFF msg;

    /* run in backgoud */
    if (daemon(0, 1) < 0)
	{
		perror("daemon");
		exit(-1);
	}

    msg_init(&msgFd);
    msg_srvInit(CMSG_ID_DHCPS, &msgFd);

    LOG_DEBUG("DHCP Server start.");

	if (-1 == dhcps_init())
	{
	    return -1; 
	}
	
	read_dhcps_config(argc < 2 ? DHCPD_CONF_FILE : argv[1]);
    
	/* loop until universe collapses */	
	while(1) { 	
        max_sock = 0;
        FD_ZERO (&rfds);

        /* set message fd */
        FD_SET(msgFd.fd, &rfds);
        max_sock = max_sock > msgFd.fd ? max_sock : msgFd.fd;
        
        for (loop = 0; iface_configs[loop].in_use == 1 && loop < MAX_GROUP_NUM; loop++)
        {
            /* Create bpfDev listenning on each interface to catch request pkgs */
            if (iface_configs[loop].fd < 0)
            {
                 iface_configs[loop].fd = listen_socket(INADDR_ANY, SERVER_PORT, 
                                                       iface_configs[loop].ifName);
                
				if(iface_configs[loop].fd == -1) {
					LOG_WARNING("Couldn't create fd on %s.", iface_configs[loop].ifName);
					dhcps_clear_up_index(loop);
					continue;
				}
				LOG_DEBUG("Create LISTEN FD(%d) on interface(%s)", 
				        iface_configs[loop].fd, iface_configs[loop].ifName);
			}

            if (iface_configs[loop].fd >= 0)
            {
			    FD_SET(iface_configs[loop].fd, &rfds);
			    if (iface_configs[loop].fd > max_sock)
				    max_sock = iface_configs[loop].fd;
		    }
		    
#ifdef DHCP_RELAY
            /* Create listen_socket on lan-ip to catch reply pkgs from remote server */
            if (iface_configs[loop].dhcps_remote != 0 )
            {
                if (iface_configs[loop].relay_skt < 0)
                {
                    iface_configs[loop].relay_skt = 
                        listen_socket_on_ip(iface_configs[loop].server, SERVER_PORT);
                    if(iface_configs[loop].relay_skt == -1) 
                    {
    					LOG_WARNING("Couldn't create server socket on ip(%x)", 
    					        iface_configs[loop].server);
    					dhcps_clear_up_index(loop);
    				}
    				LOG_DEBUG("Opening RELAY LISTEN SOCKET(%d) on 0x%08x:%d\n", 
    				    iface_configs[loop].relay_skt, iface_configs[loop].server, SERVER_PORT);
				}

                if (iface_configs[loop].relay_skt >= 0)
                {
    				FD_SET(iface_configs[loop].relay_skt, &rfds);
        			if (iface_configs[loop].relay_skt > max_sock)
        				max_sock = iface_configs[loop].relay_skt;
        	    }
            }
#endif
        }

		retval = select(max_sock + 1, &rfds, NULL, NULL, NULL);

		if (retval == 0) {
			continue;
		} else if (retval < 0 && errno != EINTR) {
			LOG_WARNING("Error on select");
			continue;
		}

		/* Added by xcl, 2011-07-05. Treat msg.*/
        if (FD_ISSET(msgFd.fd, &rfds))
        {
            msg_recv(&msgFd, &msg);
            switch (msg.type)
            {
            case CMSG_DHCPS_RELOAD_CFG:
                LOG_DEBUG("Receive Relaod Cfg Msg.");
                /* TODO: */
                /* write_leases(); */
                dhcps_clear_up(&msg);
                read_dhcps_config(argc < 2 ? DHCPD_CONF_FILE : argv[1]);
                /* read_leases(); */
                break;
            case CMSG_DHCPS_WRITE_LEASE_TO_SHM:
                LOG_DEBUG("Receive Write Lease To SHM Msg.");
                
                /* write leases to share memory */
                write_leases_to_shm();
                /* send reply msg */
                msg.type = CMSG_REPLY;
                msg_send(&msgFd, &msg);

                break;
            case CMSG_DHCPS_WAN_STATUS_CHANGE:
                LOG_DEBUG("Receive WAN Status Change Msg.");
                set_relay();    
                break;
                
            default: continue;
            }
            continue;
        }
		/* End added by xcl, 2011-07-05.*/

#ifdef DHCP_RELAY
        for (loop = 0; iface_configs[loop].in_use == 1 && loop < MAX_GROUP_NUM; loop++){
			if (iface_configs[loop].relay_skt >= 0 && FD_ISSET(iface_configs[loop].relay_skt, &rfds))
				break;
		}
		/* get reply pkg from remote server, so relay it to lan */
		if (loop < MAX_GROUP_NUM && iface_configs[loop].in_use == 1)
		{
			process_relay_response(&iface_configs[loop]);
			continue;
        }
#endif

		/* Find out which interface is selected */
        for (loop = 0; iface_configs[loop].in_use == 1 && loop < MAX_GROUP_NUM; loop++){
			if (FD_ISSET(iface_configs[loop].fd, &rfds))
				break;
		}
		if (loop < MAX_GROUP_NUM && iface_configs[loop].in_use == 1)
		    cur_iface_config = &iface_configs[loop];
		else 
		    continue;
		/* End added by xcl, 2011-06-01.*/

        pDhcpPkg = &dhcpPkg;
		if ((bytes = get_packet(&pDhcpPkg, cur_iface_config->fd)) < 0) 
		{ 
		    /* this waits for a packet - idle */
			if (bytes == -1 && errno != EINTR) {
				LOG_WARNING("Error on read, %m, reopening socket");
				close(cur_iface_config->fd);
				cur_iface_config->fd = -1;
			}
			continue;
		}


#ifdef DHCP_RELAY
		/* If enable dhcp relay on this interface, relay the request pkgs */
		if (cur_iface_config->dhcps_remote != 0)
		{
		    if (pDhcpPkg->op == BOOTREQUEST && cur_iface_config->relay_ifName[0])
		    {
                /* Added by xcl, 09Apr12. 
                 * RFC3046 2.1小节中规定: relay agents不修改DHCP客户端向服务器发出的DHCP单播包.
                 * 故对于续租request和release报文不进行relay。
                 */
                if (pDhcpPkg->ciaddr != 0)
                {
                    LOG_DEBUG("Ignore relaying unicast bootp-request pkg.");
                    continue;
                }
                /* End added */
        		        
		        process_relay_request(cur_iface_config, (char*)pDhcpPkg, bytes);
		    }
		    else if (pDhcpPkg->op == BOOTREPLY)
		    {
		        LOG_DEBUG("BPF device(%s) recieve a reply pkg, ignore it.", 
		                cur_iface_config->server);
		    }
    		continue;
		}
#endif


		if ((state = get_option(pDhcpPkg, DHCP_MESSAGE_TYPE)) == NULL) {
			LOG_WARNING("Couldn't get option from packet, ignoring");
			continue;
		}
        
		lease = find_lease_by_chaddr(pDhcpPkg->chaddr);

		if (!lease)
		{
			/* Look for a static lease */
			static_lease_ip = getIpByMac(static_leases, &pDhcpPkg->chaddr, 
			                            cur_iface_config->server, cur_iface_config->netmask);

			/* found */
			if(static_lease_ip)
			{
				memcpy(&static_lease.chaddr, &pDhcpPkg->chaddr, 16);
				static_lease.yiaddr = static_lease_ip;
				static_lease.expires = 0;

				lease = &static_lease;
			}
			/*
			else
			{
				lease = find_lease_by_chaddr(packet.chaddr);
			}
			*/
		}
		
		
		switch (state[0]) {
		case DHCPDISCOVER:
			cmmlog(LOG_NOTICE, LOG_DHCPD, "Recv DISCOVER from %02X:%02X:%02X:%02X:%02X:%02X", 
			     pDhcpPkg->chaddr[0], pDhcpPkg->chaddr[1], pDhcpPkg->chaddr[2], 
			     pDhcpPkg->chaddr[3], pDhcpPkg->chaddr[4], pDhcpPkg->chaddr[5]);

            /* 忽略来自wan口的报文 */
            if (1 == portDisabled(cur_iface_config->ifName, pDhcpPkg))
			{
				cmmlog(LOG_NOTICE, LOG_DHCPD, "Ignore DHCP REQUEST form WAN interface.");
				break;
			}
               
			if (sendOffer(pDhcpPkg) < 0) {
				LOG_DEBUG("Send OFFER failed\n");
			}
		
			break;
 		case DHCPREQUEST:		
			cmmlog(LOG_NOTICE, LOG_DHCPD, "Recv REQUEST from %02X:%02X:%02X:%02X:%02X:%02X", 
			     pDhcpPkg->chaddr[0], pDhcpPkg->chaddr[1], pDhcpPkg->chaddr[2], 
			     pDhcpPkg->chaddr[3], pDhcpPkg->chaddr[4], pDhcpPkg->chaddr[5]);
							
             /* 忽略来自wan口的报文 */
            if (1 == portDisabled(cur_iface_config->ifName, pDhcpPkg))
			{
				cmmlog(LOG_NOTICE, LOG_DHCPD, "Ignore DHCP REQUEST form WAN interface.");
				break;
			}
                       
			requested = get_option(pDhcpPkg, DHCP_REQUESTED_IP);
			server_id = get_option(pDhcpPkg, DHCP_SERVER_ID);

			if (requested) memcpy(&requested_align, requested, 4);
			if (server_id) memcpy(&server_id_align, server_id, 4);

			if (lease) {
			    /* Added by xcl, 2011-06-01. For conditional pools support.*/
			    /* 非静态地址分配发求req时需判断是否在相应条件地址池内*/
                if (lease->yiaddr != getIpByMac(static_leases, pDhcpPkg->chaddr, 
	                            cur_iface_config->server, cur_iface_config->netmask))
			    {
    			    vendor_id = get_option(pDhcpPkg, DHCP_VENDOR);
                    pCondPool = isConditionalVendorId((char *)vendor_id);
                    if ( (pCondPool && !ipInCondPool(ntohl(lease->yiaddr), pCondPool) )
                        || (!pCondPool && (ipInAnyCondPools(ntohl(lease->yiaddr)) 
                            || !ipInSrvPool(ntohl(lease->yiaddr)))))
                    {
                        LOG_DEBUG("The request ip is in other conditional pools.");
                        sendNAK(pDhcpPkg);
                        break;
                    }
                }
                /* End added by xcl, 2011-06-01.*/
                
				if (server_id) {
					/* SELECTING State */
					if (server_id_align == cur_iface_config->server && requested &&
					    requested_align == lease->yiaddr
                        /* xcl: 在Client发送REQUEST之后DUT发送ACK之前进行arp查询，看此ip是否被占用， 
                         * 04May12 */    
					    && !check_ip(pDhcpPkg, lease->yiaddr)
					    /* end added by xcl */
					    ) {
						sendACK(pDhcpPkg, lease->yiaddr);
					}
					else 
					{
					    LOG_DEBUG("DHCPS: %0x, %0x", requested_align, lease->yiaddr);
						cmmlog(LOG_NOTICE, LOG_DHCPD, "Wrong Server id or request an invalid ip");
						sendNAK(pDhcpPkg);
					}
				} else {
					if (requested) {
						/* INIT-REBOOT State */
						if (lease->yiaddr == requested_align)
							sendACK(pDhcpPkg, lease->yiaddr);
						else 
						{
							cmmlog(LOG_NOTICE, LOG_DHCPD, "Server id not found and request an invalid ip");
							sendNAK(pDhcpPkg);
						}
					} else {
						/* RENEWING or REBINDING State */
						if (lease->yiaddr == pDhcpPkg->ciaddr)
							sendACK(pDhcpPkg, lease->yiaddr);
						else {
							/* don't know what to do!!!! */
							cmmlog(LOG_NOTICE, LOG_DHCPD, "Server id and requested ip not found");
							LOG_DEBUG("%x %x\n", lease->yiaddr, pDhcpPkg->ciaddr);
							sendNAK(pDhcpPkg);
						}
					}
				}

			/* what to do if we have no record of the client */
			} else if (server_id) {
				/* SELECTING State */

			}
			else if (requested) 
			{
				/* INIT-REBOOT State */
				if ((lease = find_lease_by_yiaddr(requested_align))) 
				{
					/* Requested IP already reserved by other one */
					
					if (lease_expired(lease))
					{
						/* probably best if we drop this lease */
						memset(lease->chaddr, 0, 16);

						check_and_ack(pDhcpPkg, requested_align);
					/* make some contention for this address */
					} 
					else	/* still reserved by someone */
					{
						cmmlog(LOG_NOTICE, LOG_DHCPD, 
						    "REQUEST ip %x already reserved by %02X:%02X:%02X:%02X:%02X:%02X",
							requested_align, lease->chaddr[0], lease->chaddr[1], lease->chaddr[2],
							lease->chaddr[3], lease->chaddr[4], lease->chaddr[5]);
						sendNAK(pDhcpPkg);
					}
				}
				else /*if (requested_align < server_config.start ||
					   requested_align > server_config.end)*/ 
				{
					check_and_ack(pDhcpPkg, requested_align);
				} /* else remain silent */
			} 
			else 
			{
                /* error state, just reply NAK modified by tiger 20090927 */
                 sendNAK(pDhcpPkg);
			}
			break;
		case DHCPDECLINE:
			cmmlog(LOG_NOTICE, LOG_DHCPD, 
			     "Recv DECLINE from %02X:%02X:%02X:%02X:%02X:%02X", 
			     pDhcpPkg->chaddr[0], pDhcpPkg->chaddr[1], pDhcpPkg->chaddr[2], 
			     pDhcpPkg->chaddr[3], pDhcpPkg->chaddr[4], pDhcpPkg->chaddr[5]);
			
			if (lease) {
				memset(lease->chaddr, 0, 16);
				lease->expires = time(0) + server_config.decline_time;
			}
			break;
		case DHCPRELEASE:		
			cmmlog(LOG_NOTICE, LOG_DHCPD, 
			     "Recv RELEASE from %02X:%02X:%02X:%02X:%02X:%02X", 
			     pDhcpPkg->chaddr[0], pDhcpPkg->chaddr[1], pDhcpPkg->chaddr[2], 
			     pDhcpPkg->chaddr[3], pDhcpPkg->chaddr[4], pDhcpPkg->chaddr[5]);
			if (lease)
			{
				/* Delete the lease, lsz 080221 */
				#if 1
				memset(lease, 0, sizeof(struct dhcpOfferedAddr));
				#else
				lease->expires = time(0);
				#endif
			}
			
			break;
		case DHCPINFORM:
			cmmlog(LOG_NOTICE, LOG_DHCPD, "Recv INFORM from %02X:%02X:%02X:%02X:%02X:%02X", 
			     pDhcpPkg->chaddr[0], pDhcpPkg->chaddr[1], pDhcpPkg->chaddr[2], 
			     pDhcpPkg->chaddr[3], pDhcpPkg->chaddr[4], pDhcpPkg->chaddr[5]);
			send_inform(pDhcpPkg);
			break;
		default:
			LOG_INFO("Unsupported DHCP message (%02x) -- ignoring", state[0]);
		}
	}

	return 0;
}
