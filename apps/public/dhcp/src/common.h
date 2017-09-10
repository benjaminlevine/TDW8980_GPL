/* common.h
 *
 * Russ Dill <Russ.Dill@asu.edu> September 2001
 * Rewritten by Vladimir Oleynik <dzo@simtreas.ru> (C) 2003
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

#ifndef _COMMON_H_
#define _COMMON_H_

#include "os_log.h"

#define DHCP_COND_SRV_POOL
#define DHCP_RELAY
/* #define DHCP_DEBUG */

/* DHCP protocol -- see RFC 2131 */
#define SERVER_PORT		67
#define CLIENT_PORT		68

#define DHCP_MAGIC		0x63825363

/* DHCP option codes (partial list) */
#define DHCP_PADDING		0x00
#define DHCP_SUBNET		0x01
#define DHCP_TIME_OFFSET	0x02
#define DHCP_ROUTER		0x03
#define DHCP_TIME_SERVER	0x04
#define DHCP_NAME_SERVER	0x05
#define DHCP_DNS_SERVER		0x06
#define DHCP_LOG_SERVER		0x07
#define DHCP_COOKIE_SERVER	0x08
#define DHCP_LPR_SERVER		0x09
#define DHCP_HOST_NAME		0x0c
#define DHCP_BOOT_SIZE		0x0d
#define DHCP_DOMAIN_NAME	0x0f
#define DHCP_SWAP_SERVER	0x10
#define DHCP_ROOT_PATH		0x11
#define DHCP_IP_TTL		0x17
#define DHCP_MTU		0x1a
#define DHCP_BROADCAST		0x1c
#define DHCP_NTP_SERVER		0x2a
#define DHCP_WINS_SERVER	0x2c
#define DHCP_REQUESTED_IP	0x32
#define DHCP_LEASE_TIME		0x33
#define DHCP_OPTION_OVER	0x34
#define DHCP_MESSAGE_TYPE	0x35
#define DHCP_SERVER_ID		0x36
#define DHCP_PARAM_REQ		0x37
#define DHCP_MESSAGE		0x38
#define DHCP_MAX_SIZE		0x39
#define DHCP_T1			0x3a
#define DHCP_T2			0x3b
#define DHCP_VENDOR		0x3c
#define DHCP_CLIENT_ID		0x3d
#ifdef INCLUDE_IPV6	/* Add by HYY: support dynamic 6RD, 19Mar12 */
#define DHCP_6RD		0xd4
#endif /* INCLUDE_IPV6 */
#define DHCP_END		0xFF

#define BOOTREQUEST		1
#define BOOTREPLY		2

#define ETH_10MB		1
#define ETH_10MB_LEN		6

#define DHCPDISCOVER		1
#define DHCPOFFER		2
#define DHCPREQUEST		3
#define DHCPDECLINE		4
#define DHCPACK			5
#define DHCPNAK			6
#define DHCPRELEASE		7
#define DHCPINFORM		8
#define DHCPNULL		9	/* add by lsz 23Aug07 for those ips are token(but not by dhcp) */

#define BROADCAST_FLAG		0x8000

#define OPTION_FIELD		0
#define FILE_FIELD		1
#define SNAME_FIELD		2

/* miscellaneous defines */
#define MAC_BCAST_ADDR		(uint8_t *) "\xff\xff\xff\xff\xff\xff"
#define OPT_CODE 0
#define OPT_LEN 1
#define OPT_DATA 2

#define MAX_GROUP_NUM 8
#define MAX_WAN_NUM   8
#define MAX_INTF_NAME 16

#define DHCPC_BPF_DEV_NAME "/bpf/dhcpc"
#define DHCPS_BPF_DEV_NAME "/bpf/dhcps"
#define DHCPC_ARP_DEV_NAME "/bpf/dhcpc-arp"

typedef enum
{	
	DHCPC = 1,
	DHCPS,
	RELAY
}DHCP_APP_TYPE;


enum syslog_levels {
	EMERG = 0,
	ALERT,
	CRIT,
	WARNING,
	ERR,
	INFO,
	DEBUG
};

void udhcp_logging(int level, const char *call, int line, const char *fmt, ...);
int udhcp_addrNumToStr(unsigned int numAddr, char *pStrAddr);

#ifdef DHCP_DEBUG
#define LOG_ERR(args...) udhcp_logging(ERR, __FUNCTION__, __LINE__, args)
#define LOG_WARNING(args...) udhcp_logging(WARNING, __FUNCTION__, __LINE__, args)
#define LOG_INFO(args...) udhcp_logging(INFO, __FUNCTION__, __LINE__, args)
#define LOG_DEBUG(args...) udhcp_logging(DEBUG, __FUNCTION__, __LINE__, args)
#define cmmlog(severity, module, args...) udhcp_logging(INFO, __FUNCTION__, __LINE__, args)
#else
#define LOG_ERR(args...) udhcp_logging(ERR, __FUNCTION__, __LINE__, args)
#define LOG_WARNING(args...)
#define LOG_INFO(args...)
#define LOG_DEBUG(args...)
#endif /* DHCP_DEBUG */

#endif /*_COMMON_H_*/
