/*  Copyright(c) 2010-2011 Shenzhen TP-LINK Technologies Co.Ltd.
 *
 * file	os_msg.h
 * brief	The msg lib declarations. 
 *
 * author	Yang Xv
 * version	1.0.0
 * date	28Apr11
 *
 * history 	\arg 1.0.0, 28Apr11, Yang Xv, Create the file.
 */

#ifndef __OS_MSG_H__
#define __OS_MSG_H__

/* 注意这个头文件不要引用<cstd.h>头文件 */
#include <unistd.h>


#ifdef INCLUDE_IPV6
#include <netinet/in.h>
#endif	/* INCLUDE_IPV6 */

/**************************************************************************************************/
/*                                           DEFINES                                              */
/**************************************************************************************************/

/* 
 * brief Message content size	
 */
#define MSG_CONTNET_SIZE	512


/* 
 * brief Message size	
 */
#define MSG_SIZE			520


/* for all message type */

#define SNTP_DM_SERVER_NUM	5

/* 
 * brief status of DHCP6C_INFO_MSG_BODY
 */
#ifdef INCLUDE_IPV6
#define DHCP6C_ASSIGNED_DNS 	0x1
#define DHCP6C_ASSIGNED_ADDR 	0x2
#define DHCP6C_ASSIGNED_PREFIX 	0x4
#define DHCP6C_ASSIGNED_DSLITE_ADDR		0x8  /* Add by YuanRui: support DS-Lite, 21Mar12 */
#endif	/* INCLUDE_IPV6 */

/**************************************************************************************************/
/*                                           TYPES                                                */
/**************************************************************************************************/


#ifdef __LINUX_OS_FC__
#include <sys/un.h>

typedef struct
{
	int fd;
	struct sockaddr_un _localAddr;
	struct sockaddr_un _remoteAddr;
}CMSG_FD;
#endif	/* __LINUX_OS_FC__ */

#ifdef __VXWORKS_OS_FC__
typedef struct
{
	int fd;
}CMSG_FD;
#endif /* __VXWORKS_OS_FC__ */


/* 
 * brief 	Enumeration message type
 * 			Convention:
 *			System message types - 1000 ~ 1999
 *			Common application message types - 2000 ~ 2999
 */
typedef enum
/* 如果一个消息只有一个UINT32的数据，那么可以省略该message type对应的结构体，
 * 只使用CMSG_BUFF结构中的priv成员即可
 */
{
	CMSG_NULL = 0,
	CMSG_REPLY = 1,
	CMSG_LOG = 2000,
	CMSG_SNTP_CFG = 2001,
	CMSG_SNTP_STATUS = 2002,	/* only have one word value */
	CMSG_SNTP_START = 2003,

	CMSG_DNS_PROXY_CFG = 2004,
	CMSG_DNS_SERVER	= 2005,		/* only have one word value */
	CMSG_PPP_STATUS = 2006,

 	/* Added by Yang Caiyong, 11-Aug-29.
 	 * For auto PVC.
 	 */ 
	CMSG_AUTO_PVC_SEARCHED = 2007,
 	/* Ended by Yang Caiyong, 11-Aug-29. */
	
	/* Added by xcl, 2011-06-13.*/
	CMSG_DHCPS_RELOAD_CFG = 2010, 
	CMSG_DHCPS_WRITE_LEASE_TO_SHM = 2011,
	CMSG_DHCPS_WAN_STATUS_CHANGE = 2012,
	CMSG_DHCPC_STATUS = 2013,
	CMSG_DHCPC_START = 2014,
	CMSG_DHCPC_RENEW = 2015, 
	CMSG_DHCPC_RELEASE = 2016,
	CMSG_DHCPC_SHUTDOWN = 2017,
	CMSG_DHCPC_MAC_CLONE = 2018,
	/* End added by xcl, 2011-06-13.*/

	CMSG_DDNS_PH_RT_CHANGED = 2020,
	CMSG_DDNS_PH_CFG_CHANGED = 2021,
	CMSG_DDNS_PH_GET_RT	= 2022,
	/* Added by xcl, for dyndns, 24Nov11 */
	CMSG_DYNDNS_RT_CHANGED = 2023,
	CMSG_DYNDNS_CFG_CHANGED = 2024,
	CMSG_DYNDNS_STATE_CHANGED = 2025,
	/* Added by tpj, for noipdns, 17Jan12 */
	CMSG_NOIPDNS_RT_CHANGED = 2026,
	CMSG_NOIPDNS_CFG_CHANGED = 2027,
	CMSG_NOIPDNS_STATE_CHANGED = 2028,
	/*end (n & n)*/

	CMSG_HTTPD_SERVICE_CFG = 2030,
	CMSG_HTTPD_USER_CFG = 2031,
	CMSG_HTTPD_HOST_CFG = 2032,
	CMSG_HTTPD_CHALLENGE_CFG = 2033,

	CMSG_CLI_USRCFG_UPDATE	= 2040,

 	/* Added  by  Li Chenglong , 11-Jul-31.*/
	CMSG_UPNP_ENABLE = 2050,
	CMSG_DEFAULT_GW_CH = 2051,
 	/* Ended  by  Li Chenglong , 11-Jul-31.*/

	/* Add by chz, 2012-12-24 */
	CMSG_UPNP_DEL_ENTRY = 2052,
	/* end add */

	/* added by Yuan Shang for diagTool, 2011-08-18 */
	CMSG_DIAG_TOOL_COMMAND = 2060,

	/* added by wuzhiqin, 2011-09-26 */
	CMSG_CWMP_CFG_UPDATE = 2070, 
	CMSG_CWMP_PARAM_VAL_CHANGED = 2071,
	CMSG_CWMP_WAN_CONNECTION_UP = 2072,
	CMSG_CWMP_TIMER = 2073,
	/* ended by wuzhiqin, 2011-09-26 */

	/* added by Yuan Shang for usb storage hotplug event, 2011-12-09 */
	CMSG_USB_HOTPLUG_EVENT = 2080,

	/* added by Wang Wenhao for IGMPv3 Proxy, 2011-11-24 */
	CMSG_IGMPD_ADD_LAN_IF = 2100,
	CMSG_IGMPD_ADD_WAN_IF = 2101,
	CMSG_IGMPD_DEL_IF = 2102,

	/* Added by LI CHENGLONG , 2011-Dec-15.*/
	CMSG_DLNA_MEDIA_SERVER_INIT = 2110,
	CMSG_DLNA_MEDIA_SERVER_MANUAL_SCAN = 2111,
	CMSG_DLNA_MEDIA_SERVER_OP_FOLDER = 2112,
	CMSG_DLNA_MEDIA_SERVER_RELOAD = 2113,
	CMSG_DLNA_MEDIA_DEVICE_STATE_CHANGE = 2114,
	/* Ended by LI CHENGLONG , 2011-Dec-15.*/
#ifdef INCLUDE_LAN_WLAN
	CMSG_WPS_CFG = 2700,
	CMSG_WLAN_SWITCH = 2701,
	CMSG_WPS_PIN_SWITCH = 2702,
#endif /* INCLUDE_LAN_WLAN */

	/* 
	 * for voice process. added by zhonglianbo 2011-8-10
	 */
#ifdef INCLUDE_VOIP
	CMSG_VOIP_CMCNXCOUNT 			= 2800,
	CMSG_VOIP_WAN_STS_CHANGED		= 2801,			/* wan口状态改变.
												 * 消息的priv字段值为 RSL_VOIP_INTF_STSCODE
												 * 消息的content为当前的端口ip地址
												 * Added by zhonglianbo, 2011-11-24 
												 */
	CMSG_VOIP_CONFIG_CHANGED 		= 2802,
	CMSG_VOIP_RESTART_CALLMGR 		= 2803,
	CMSG_VOIP_CONFIG_WRITTEN 		= 2804,			/* 非语音进程不能写flash,以此消息通知
												 * 语音进程写flash
												 */
	CMSG_VOIP_FLASH_WRITTEN 		= 2805,			/* 语音进程可以写flash,以此消息通知语
												 * 音进程已经写了flash
												 */
	CMSG_VOIP_STATISTICS_RESET 		= 2806, 
#ifdef INCLUDE_USB_VOICEMAIL	
	CMSG_VOIP_UVM_USING_RECORDED_FILE	= 2807,		/* 获取USB线程是否正在使用recorded
												 * file.	Added by zhonglianbo, 2011-9-19.
												 */
	CMSG_VOIP_USB_MOUNT_NEW = 2808,				/* A new disk that mounted can be used by usbvm  */
	CMSG_VOIP_USB_UMOUNT_CHANGE = 2809,			/* Change to another disk's path that is effective*/
	CMSG_VOIP_USB_UMOUNT_NULL = 2810,			/* There is not any disk can be used by usbvm  */
#endif /* INCLUDE_USB_VOICEMAIL */												 

#endif /* INCLUDE_VOIP */
	/* end of voice process */

    /* Added by xcl, 21Sep11 */
    CMSG_SNMP_CFG_CHANGED 	= 2850,
    CMSG_SNMP_LINK_UP       = 2851,
    CMSG_SNMP_LINK_DOWN     = 2852,
    CMSG_SNMP_WAN_UP		= 2853,
    /* End added by xcl, 21Sep11 */

#ifdef INCLUDE_IPV6	/* Add by CM, 16Nov11 */
	CMSG_IPV6_PPP_STATUS	= 2900,
	CMSG_IPV6_DHCP6C_STATUS	= 2901,
#endif	/* INCLUDE_IPV6 */

#ifdef INCLUDE_IPSEC
	CMSG_IPSEC_CFG_CHANGED = 3000,
	CMSG_IPSEC_WAN_CHANGED = 3001,
#endif

/* Add by zjj, 20120703, for usb 3g handle card event */
#ifdef INCLUDE_USB_3G_DONGLE
	CMSG_USB_3G_HANDLE_EVENT = 3100,
#endif /* INCLUDE_USB_3G_DONGLE */

/*<< BosaZhong@20Sep2012, add, for SMP system. */
#ifdef SOCKET_LOCK 
	CMSG_SOCKET_LOCK_P                  = 4000,
	CMSG_SOCKET_LOCK_V                  = 4001,
	CMSG_SOCKET_LOCK_PROBE_DEAD_PROCESS = 4002,
#endif /* SOCKET_LOCK */
/*>> endof BosaZhong@20Sep2012, add, for SMP system. */
}CMSG_TYPE;


/* 
 * brief	Message struct
 */
typedef struct
{
	CMSG_TYPE type;		/* specifies what message this is */
	unsigned int priv;		/* private data, one word of user data etc. */
	unsigned char content[MSG_CONTNET_SIZE];
}CMSG_BUFF;


/* 
 * brief	Message type identification	
 */
typedef enum
{
	CMSG_ID_NULL = 5,	/* start from 5 */
	CMSG_ID_COS = 6,
	CMSG_ID_SNTP = 7,
	CMSG_ID_HTTP = 8,
	CMSG_ID_DNS_PROXY = 9,
	CMSG_ID_DHCPS = 10, 	/* Added by xcl, 2011-06-13.*/
	CMSG_ID_DDNS_PH = 11,  	/* addde by tyz, 2011-07-21 */
	CMSG_ID_PH_RT = 12,
	CMSG_ID_CLI = 13,
	CMSG_ID_DHCPC = 14, 
	CMSG_ID_UPNP =15, 	/* Added  by  Li Chenglong , 11-Jul-31.*/
	CMSG_ID_DIAGTOOL =16, /*Added by Yuan Shang, 2011-08-18 */
	CMSG_ID_CWMP = 17, /* add by wuzhiqin, 2011-09-26 */
	CMSG_ID_SNMP = 18, /* Added by xcl, 21Sep11 */
	CMSG_ID_IGMP = 19,	/* Added by Wang Wenhao, 2011-11-18 */

#ifdef INCLUDE_VOIP
	CMSG_ID_VOIP = 20,  /* for voice process, added by zhonglianbo 2011-8-10 */
#endif /* INCLUDE_VOIP */
	CMSG_ID_DYNDNS = 21, /* Added by xcl, 24Nov11 */
	
	/* Added by LI CHENGLONG , 2011-Dec-15.*/
	CMSG_ID_DLNA_MEDIA_SERVER = 22,
	/* Ended by LI CHENGLONG , 2011-Dec-15.*/

	CMSG_ID_NOIPDNS = 23, /*added by tpj, 2012-2-1*/
	
	CMSG_ID_IPSEC = 24,

	CMSG_ID_LOG = 25,	/* added by yangxv, 2012.12.25, log message no longer shared cos_passive */

#ifdef SOCKET_LOCK
	CMSG_ID_SOCKET_LOCK = 26, /* BosaZhong@20Sep2012, add, for SMP system. */
	CMSG_ID_SOCKET_LOCK_ACCEPT = 27, /* BosaZhong@21Sep2012, add, for SMP system. */
#endif /* SOCKET_LOCK */

	CMSG_ID_MAX,	
}CMSG_ID;


/* for all message type 
 * 注意不要使用UINT8等这种自定义的数据类型
 */

/* 
 * brief	CMSG_SNTP_CFG message type content
 */
typedef struct
{
	char   	ntpServers[SNTP_DM_SERVER_NUM][32];
	unsigned int primaryDns;
	unsigned int secondaryDns;
	unsigned int timeZone;
}SNTP_CFG_MSG;

/* Added by LI CHENGLONG , 2011-Dec-15.*/

/* 
 * brief: Added by LI CHENGLONG, 2011-Nov-21.
 *		 厂商相关的信息，在DLNA_MEDIA_SERVER进程启动时通过INIT消息发送给DLNA_MEDIA_SERVER进程，
*        DLNA_MEDIA_SERVER进程在发送ssdp 通告时将通告特定厂商的信息.
 */
typedef struct _MANUFACT_SPEC_INFO
{
	char 	devManufacturerURL[64];
	char 	manufacturer[64];
	char 	modelName[64];
	char 	devModelVersion[16];
	char 	description[256];
}MANUFACT_SPEC_INFO;


/* 
 * brief: Added by LI CHENGLONG, 2011-Dec-15.
 *		  描述一个共享目录的结构.
 */
typedef struct _DMS_FOLDER_INFO
{
	char	dispName[32];
	char 	path[128];
}DMS_FOLDER_INFO;


/* 
 * brief: Added by LI CHENGLONG, 2011-Dec-16.
 *		  对目录的操作类型.
 */
typedef enum _DMS_FOLDER_OP
{
	DMS_INIT_FOLDER = 0,
	DMS_DEL_FOLDER = 1,
	DMS_ADD_FOLDER = 2
}DMS_FOLDER_OP;

/* 
 * brief: Added by LI CHENGLONG, 2011-Dec-15.
 *		  启动DLNA_MEDIA_SERVER进程后立即发送,将初始化配置信息发送给DLNA_MEDIA_SERVER进程.
 */
typedef struct _DMS_INIT_INFO_MSG
{
	unsigned char		serverState;			/* ServerState */
	char				serverName[16];
	unsigned char		scanFlag;				/*scan*/
	unsigned int		scanInterval;		/*scan interval*/
	MANUFACT_SPEC_INFO	manuInfo;				/*oem等不同厂商的信息*/
	unsigned int		folderCnt;			/*how many folde is shared now*/
}DMS_INIT_INFO_MSG;

/* 
 * brief: Added by LI CHENGLONG, 2011-Dec-15.
 *		  上层UI更新了DLNA_MEDIA_MEDIA_SERVER配置后直接将整个配置传给DLNA_MEDIA_SERVER进程,
 *		  不再对各个操作进行分类.
 */
typedef struct _DMS_RELOAD_MSG
{
	unsigned char		serverState;			/* ServerState */
	char				serverName[16];
	unsigned char		scanFlag;				/*scan*/
	unsigned int		scanInterval;		/*scan interval*/	
}DMS_RELOAD_MSG;

/* 
 * brief: Added by LI CHENGLONG, 2011-Dec-16.
 *		  操作目录的消息.
 */
typedef struct _DMS_OP_FOLDER_MSG
{
	 DMS_FOLDER_OP			op;
	 DMS_FOLDER_INFO		folder;
}DMS_OP_FOLDER_MSG;

/* Ended by LI CHENGLONG , 2011-Dec-15.*/


/* 
 * brief	CMSG_ID_CLI message type content
 */
typedef struct _CLI_USR_CFG_MSG
{
	char   		rootName[16];	/* RootName */
	char   		rootPwd[16];	/* RootPwd */
	char   		adminName[16];	/* AdminName */
	char   		adminPwd[16];	/* AdminPwd */
	char   		userName[16];	/* UserName */
	char   		userPwd[16];	/* UserPwd */
	char		manufact[64];/* Added by Li Chenglong , 2011-Oct-12.*/
}CLI_USR_CFG_MSG;


/* 
 * brief: Added by Li Chenglong, 11-Jul-31.
 *		  UPnP enable message
 */
typedef struct _UPNP_ENABLE_MSG
{
	unsigned int enable; 
}UPNP_ENABLE_MSG;

/* Add by chz, 2012-12-24 */
typedef struct _UPNP_DEL_MSG
{
	unsigned int port;
	char protocol[16];
}UPNP_DEL_MSG;
/* end add */


/* 
 * brief: Added by Li Chenglong, 11-Jul-31.
 *		  默认网关状态改变的消息。
 */
typedef struct _UPNP_DEFAULT_GW_CH_MSG
{
	char gwName[16];
	unsigned char natEnabled;
	unsigned char upDown;
}UPNP_DEFAULT_GW_CH_MSG;


/* 
 * brief CMSG_DNS_PROXY_CFG	message type content
 */
typedef struct
{
	unsigned int primaryDns;
	unsigned int secondaryDns;
}DNS_PROXY_CFG_MSG;


/* Added by Zeng Yi. 2011-07-08 */
typedef struct
{
	unsigned char isError;
	char SSID[34];
	int authMode;
	int encryMode;
	char key[65];
	int band;  /* 0 stand for 2.4GHz. 1 stand for 5GHz. */
}WPS_CFG_MSG;
/* End added. Zeng Yi. */

/* Added by Yang Caiyong for PPP connection status changed, 2011-07-18 */
typedef struct _PPP_CFG_MSG
{
	unsigned int pppDevUnit;
	char connectionStatus[18];
	unsigned int pppLocalIp;
	unsigned int pppSvrIp;
	unsigned int uptime;
	char lastConnectionError[32];
	unsigned int dnsSvrs[2];
}PPP_CFG_MSG;
/* YCY adds end */

/* Added by xcl, 2011-07-25 */
typedef struct 
{
    unsigned int delLanIp;
    unsigned int delLanMask;
}DHCPS_RELOAD_MSG_BODY;

#ifdef INCLUDE_IPV6	/* Add by HYY: support dynamic 6RD, 20Mar12 */
typedef struct
{
	unsigned char ipv4MaskLen;
	unsigned char sit6rdPrefixLen;
	struct in6_addr sit6rdPrefix;
	unsigned int sit6rdBRIPv4Addr;
}DHCPC_6RD_INFO;
#endif /* INCLUDE_IPV6 */

typedef struct 
{
    unsigned char status; /* Have we been assigned an IP address ? */
    char ifName[16];  
    unsigned int ip;
    unsigned int mask;
    unsigned int gateway;
    unsigned int dns[2];
#ifdef INCLUDE_IPV6	/* Add by HYY: support dynamic 6RD, 20Mar12 */
	DHCPC_6RD_INFO sit6rdInfo;
#endif /* INCLUDE_IPV6 */
}DHCPC_INFO_MSG_BODY;

typedef struct 
{
    unsigned char unicast;
    char ifName[16];
    char hostName[64];
#ifdef INCLUDE_IPV6	/* Add by HYY: support dynamic 6RD, 19Mar12 */
	unsigned char sit6rdEnabled;
#endif /* INCLUDE_IPV6 */
}DHCPC_CFG_MSG_BODY;
/* End added by xcl, 2011-07-25 */

/* Added by tyz 2011-08-02 (n & n) */
/* the msg of the interface */
typedef struct
{
	int ifUp;
	unsigned int ip;
	unsigned int gateway;
	unsigned int mask;
	unsigned int dns[2];
	char ifName[16];
}DDNS_RT_CHAGED_MSG;

/*
the msg of the ph running time
*/
typedef struct 
{
	unsigned char state;
	unsigned char sevType;
	unsigned short isEnd;
}DDNS_RT_PRIV_MSG;
/*
the msg of the cfg 
*/
typedef struct
{	
	int enabled;
	int reg;
	int userLen;
	char phUserName[64];
	int pwdLen;
	char phPwd[64];	
	char domain[1296];
}DDNS_PH_CFG_MSG;

/* Added by xcl, 24Nov11 */
/* dynDns config msg struct */
typedef struct 
{
    unsigned char   enable;
    char            userName[64];
    char            password[64];
    char            domain[128];
    char            server[64];
    unsigned char   login;
}DYN_DNS_CFG_MSG;

typedef struct 
{
    unsigned int state;
}DYN_DNS_STATE_MSG;
/* End added by xcl */

/* Added by tpj, 17Jan12 */
/* noipDns config msg struct */
typedef struct 
{
    unsigned char   enable;
    char            userName[64];
    char            password[64];
    char            domain[128];
    char            server[64];
    unsigned char   login;
}NOIP_DNS_CFG_MSG;

typedef struct 
{
    unsigned int state;
}NOIP_DNS_STATE_MSG;
/* End added by tpj */

typedef struct
{
	unsigned int command;
	char host[16];
	unsigned int result;
}DIAG_COMMAND_MSG;

/* end (n & n) */
typedef struct
{
	unsigned char vpi;
	unsigned short vci;
	char connName[32];
}AUTO_PVC_MSG;

/* Added by xcl, 17Oct11, snmp msg struct */
typedef struct 
{
    unsigned short ifIndex;
}SNMP_LINK_STAUS_CHANGED_MSG;
/* End added */

#ifdef INCLUDE_IPV6	/* Add by HYY: IPv6 support, 16Nov11 */
typedef struct _PPP6_CFG_MSG
{
	unsigned int pppDevUnit;
	char connectionStatus[18];
	unsigned long long remoteID;
	unsigned long long localID;
	unsigned int uptime;
	char lastConnectionError[31];
}PPP6_CFG_MSG;

typedef struct 
{
	struct in6_addr addr;
	unsigned int pltime;
	unsigned int vltime;
	unsigned char plen;
}DHCP6C_IP_INFO;

typedef struct 
{
	unsigned char status;
	char ifName[16];  
	DHCP6C_IP_INFO ip;
	DHCP6C_IP_INFO prefix;
	struct in6_addr dns[2];
	struct in6_addr dsliteAddr;   /* Add by YuanRui: support DS-Lite, 21Mar12 */
}DHCP6C_INFO_MSG_BODY;
#endif /* INCLUDE_IPV6 */

#ifdef INCLUDE_IPSEC
typedef struct
{
	unsigned short currDepth;								
	unsigned short numInstance[6];	
}IPSEC_OLD_NUM_STACK;

typedef struct 
{
	int state;
    char default_gw_ip[16];
}IPSEC_WAN_STATE_CHANGED_MSG;

typedef struct
{
	char local_ip[16];
	char local_mask[16];
	unsigned int  local_ip_mode;
	char remote_ip[16];
	char remote_mask[16];
	unsigned int  remote_ip_mode;
	char real_remote_gw_ip[16];
	char spi[16];
	char second_spi[16];
	unsigned int entryID;
	unsigned int  op;
	unsigned char  enable;
	unsigned int key_ex_type; /*Added for vxWorks*/
	IPSEC_OLD_NUM_STACK stack;
}IPSEC_CFG_CHANGED_MSG;
#endif

/**************************************************************************************************/
/*                                           FUNCTIONS                                            */
/**************************************************************************************************/

/* 
 * fn		int msg_init(CMSG_FD *pMsgFd)
 * brief	Create an endpoint for msg
 *	
 * param[out]	pMsgFd - return msg descriptor that has been create	
 *
 * return	-1 is returned if an error occurs, otherwise is 0
 *
 * note 	Need call msg_cleanup() when you no longer use this msg which is created by msg_init()
 */
int msg_init(CMSG_FD *pMsgFd);


/* 
 * fn		int msg_srvInit(CMSG_ID msgId, CMSG_FD *pMsgFd)
 * brief	Init an endpoint as a server and bind a name to this endpoint msg	
 *
 * param[in]	msgId - server name	
 * param[in]	pMsgFd - server endpoint msg fd
 *
 * return	-1 is returned if an error occurs, otherwise is 0	
 */
int msg_srvInit(CMSG_ID msgId, CMSG_FD *pMsgFd);



/* 
 * fn		int msg_connSrv(CMSG_ID msgId, CMSG_FD *pMsgFd)
 * brief	Init an endpoint as a client and specify a server name	
 *
 * param[in]		msgId - server name that we want to connect	
 * param[in/out]	pMsgFd - client endpoint msg fd	
 *
 * return	-1 is returned if an error occurs, otherwise is 0
 */
int msg_connSrv(CMSG_ID msgId, CMSG_FD *pMsgFd);


/* 
 * fn		int msg_recv(const CMSG_FD *pMsgFd, CMSG_BUFF *pMsgBuff)
 * brief	Receive a message form a msg	
 *
 * param[in]	pMsgFd - msg fd that we want to receive message
 * param[out]	pMsgBuff - return recived message
 *
 * return	-1 is returned if an error occurs, otherwise is 0
 *
 * note		we will clear msg buffer before recv
 */
int msg_recv(const CMSG_FD *pMsgFd, CMSG_BUFF *pMsgBuff);


/* 
 * fn		int msg_send(const CMSG_FD *pMsgFd, const CMSG_BUFF *pMsgBuff)
 * brief	Send a message from a msg	
 *
 * param[in]	pMsgFd - msg fd that we want to send message	
 * param[in]	pMsgBuff - msg that we wnat to send
 *
 * return	-1 is returned if an error occurs, otherwise is 0
 *
 * note 	This function will while call sendto() if sendto() return ENOENT error
 */
int msg_send(const CMSG_FD *pMsgFd, const CMSG_BUFF *pMsgBuff);


/* 
 * fn		int msg_cleanup(CMSG_FD *pMsgFd)
 * brief	Close a message fd
 * details	
 *
 * param[in]	pMsgFd - message fd that we want to close		
 *
 * return	-1 is returned if an error occurs, otherwise is 0		
 */
int msg_cleanup(CMSG_FD *pMsgFd);


/* 
 * fn		int msg_connCliAndSend(CMSG_ID msgId, CMSG_FD *pMsgFd, CMSG_BUFF *pMsgBuff)
 * brief	init a client msg and send msg to server which is specified by msgId	
 *
 * param[in]	msgId -	server ID that we want to send
 * param[in]	pMsgFd - message fd that we want to send
 * param[in]	pMsgBuff - msg that we wnat to send
 *
 * return	-1 is returned if an error occurs, otherwise is 0	
 */
int msg_connCliAndSend(CMSG_ID msgId, CMSG_FD *pMsgFd, CMSG_BUFF *pMsgBuff);


/* 
 * fn		int msg_sendAndGetReply(CMSG_FD *pMsgFd, CMSG_BUFF *pMsgBuff)
 * brief	
 *
 * param[in]	pMsgFd - msg fd that we want to use
 * param[in/out]pMsgBuff - send msg and get reply
 * param[in]	timeSeconds - timeout in second
 *
 * return	-1 is returned if an error occurs, otherwise is 0	
 */
int msg_sendAndGetReplyWithTimeout(CMSG_FD *pMsgFd, CMSG_BUFF *pMsgBuff, int timeSeconds);


#endif /* __OS_MSG_H__ */

