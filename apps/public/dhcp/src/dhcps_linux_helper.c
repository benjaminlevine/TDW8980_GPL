/*  Copyright(c) 2009-2011 Shenzhen TP-LINK Technologies Co.Ltd.
 *
 * file		dhcps_linux_helper.c
 * brief	functions just for linux os	
 * details	
 *
 * author	Xu Chenglong
 * version	1.0.0
 * date		18Jul11
 *
 * history 	\arg 1.0.0, 18Jul11, Xu Chenglong, create file.
 */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <netdb.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/ether.h>

#include "static_leases.h"
#include "dhcps.h"
#include "options.h"
#include "common.h"
#include "dhcps_helper.h"
#include "socket.h"

#ifdef DHCP_RELAY
#include "relay.h"
#endif

#include "os_lib.h"

/**************************************************************************************************/
/*                                           DEFINES                                              */
/**************************************************************************************************/
#define READ_CONFIG_BUF_SIZE    512
#define TP_RETRY_INTERVAL       1
#define TP_RETRY_COUNT          3

/**************************************************************************************************/
/*                                           TYPES                                                */
/**************************************************************************************************/
#ifdef DHCP_RELAY
typedef struct netiface {
	char nif_name[32];
	unsigned char nif_mac[6];
	unsigned int nif_index;
	in_addr_t nif_ip;
}netiface;
#endif
/**************************************************************************************************/
/*                                           EXTERN_PROTOTYPES                                    */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           LOCAL_PROTOTYPES                                     */
/**************************************************************************************************/
static int read_ip(char *line, void *arg);
static int read_mac(const char *line, void *arg);
static int read_qstr(char *line, char *arg, int max_len);
static int read_u32(const char *line, void *arg);
static int read_yn(const char *line, void *arg);
static int read_opt(const char *const_line, void *arg);
static int read_staticlease(const char *const_line, void *arg);

#ifdef DHCP_COND_SRV_POOL
static void read_dns_pair(const char* const_line, uint32_t *pDns);
static void read_dev_tpye(const char* const_line, DEVICE_TYPE *pDevType);
static void read_cond_pool(char *const_line, struct cond_server_pool_t *cond_pools);
static void read_reserved_option(char *const_line, struct cond_server_pool_t *pCondPool);
#endif

#ifdef DHCP_RELAY
static netiface *get_netifaces(int *count);
static void addIptablesRuleForDHCPRelay();
static void removeIptablesRuleForDHCPRelay();
#endif

static int set_iface_config_defaults_if_not_set(void);
static void *attachSharedBuff();
static int detachSharedBuff(const void * pShmAddr);

/**************************************************************************************************/
/*                                           VARIABLES                                            */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           LOCAL_FUNCTIONS                                      */
/**************************************************************************************************/
/* on these functions, make sure you datatype matches */
static int read_ip(char *line, void *arg)
{
	struct in_addr *addr = arg;
	struct hostent *host;
	int retval = 1;

	if (!inet_aton(line, addr)) {
		if ((host = gethostbyname(line)))
			addr->s_addr = *((unsigned long *) host->h_addr_list[0]);
		else retval = 0;
	}
	return retval;
}

static int read_mac(const char *line, void *arg)
{
	uint8_t *mac_bytes = arg;
	struct ether_addr *temp_ether_addr;
	int retval = 1;

	temp_ether_addr = ether_aton(line);

	if(temp_ether_addr == NULL)
		retval = 0;
	else
		memcpy(mac_bytes, temp_ether_addr, 6);

	return retval;
}

/* 
 * Read string from @*line to @*arg not more than @max_len bytes.
 * Added by xcl, 04/28/2011.
 */
static int read_qstr(char *line, char *arg, int max_len)
{
	char * p = line;
	int quoted = 0;
	int len;
	
	if (*p == '\"') {
		quoted = 1;
		line++;
		p++;
	}
	
	while (*p) {
		if (*p == '\"' && quoted)
			break;
		else if (isspace(*p)) {
			if (!isblank(*p) || !quoted)
				break;
		}
		p++;
	}

	len = p - line;
	if (len >= max_len)
		len = max_len - 1;
	memcpy(arg, line, len);
	arg[len] = 0;

    if (quoted) memset(line, 1, len);/* Added by xcl, set space as 1.*/
	
	return len;
}
/* End added by xcl, 04/26/2011.*/


static int read_u32(const char *line, void *arg)
{
	uint32_t *dest = arg;
	char *endptr;
	*dest = strtoul(line, &endptr, 0);
	return endptr[0] == '\0';
}


static int read_yn(const char *line, void *arg)
{
	char *dest = arg;
	int retval = 1;

	if (!strcasecmp("yes", line))
		*dest = 1;
	else if (!strcasecmp("no", line))
		*dest = 0;
	else retval = 0;

	return retval;
}


/* read a dhcp option and add it to opt_list */
static int read_opt(const char *const_line, void *arg)
{
	struct option_set **opt_list = arg;
	char *opt, *val, *endptr;
	struct dhcp_option *option;
	int retval = 0, length;
	char buffer[8];
	char *line;
	uint16_t *result_u16 = (uint16_t *) buffer;
	uint32_t *result_u32 = (uint32_t *) buffer;

	/* Cheat, the only const line we'll actually get is "" */
	line = (char *) const_line;
	if (!(opt = strtok(line, " \t="))) return 0;

	for (option = dhcp_options; option->code; option++)
		if (!strcasecmp(option->name, opt))
			break;

	if (!option->code) return 0;

	do {
		if (!(val = strtok(NULL, ", \t"))) break;
		length = option_lengths[option->flags & TYPE_MASK];
		retval = 0;
		opt = buffer; /* new meaning for variable opt */
		switch (option->flags & TYPE_MASK) {
		case OPTION_IP:
			retval = read_ip(val, buffer);
			break;
		case OPTION_IP_PAIR:
			retval = read_ip(val, buffer);
			if (!(val = strtok(NULL, ", \t/-"))) retval = 0;
			if (retval) retval = read_ip(val, buffer + 4);
			break;
		case OPTION_STRING:
			length = strlen(val);
			if (length > 0) {
				if (length > 254) length = 254;
				opt = val;
				retval = 1;
			}
			break;
		case OPTION_BOOLEAN:
			retval = read_yn(val, buffer);
			break;
		case OPTION_U8:
			buffer[0] = strtoul(val, &endptr, 0);
			retval = (endptr[0] == '\0');
			break;
		case OPTION_U16:
			*result_u16 = htons(strtoul(val, &endptr, 0));
			retval = (endptr[0] == '\0');
			break;
		case OPTION_S16:
			*result_u16 = htons(strtol(val, &endptr, 0));
			retval = (endptr[0] == '\0');
			break;
		case OPTION_U32:
			*result_u32 = htonl(strtoul(val, &endptr, 0));
			retval = (endptr[0] == '\0');
			break;
		case OPTION_S32:
			*result_u32 = htonl(strtol(val, &endptr, 0));
			retval = (endptr[0] == '\0');
			break;
		default:
			break;
		}
		if (retval)
			attach_option(opt_list, option, opt, length);
	} while (retval && option->flags & OPTION_LIST);
	return retval;
}

/* Modify by xcl, 2011-07-18 */
static int read_staticlease(const char *const_line, void *arg)
{
	char *line;
	char *mac_string, *ip_string;
	int loop = 0;

	for (loop = 0; loop < MAX_STATIC_LEASES_NUM; loop++)
	{
	    if (static_leases[loop].ip == 0)
	    {
	        break;
	    }
	}

	if (loop >= MAX_STATIC_LEASES_NUM)
	{
	    LOG_WARNING("StaticIp: Support %d items at most, ignore the rest.", 
                            MAX_STATIC_LEASES_NUM);
        return 1;
    }
    
    /* Read mac */
	line = (char *) const_line;
	mac_string = strtok(line, " \t");
	read_mac(mac_string, static_leases[loop].mac);

	/* Read ip */
	ip_string = strtok(NULL, " \t");
	read_ip(ip_string, &static_leases[loop].ip);
	
	return 1;
}


#ifdef DHCP_COND_SRV_POOL
static void read_dns_pair(const char* const_line, uint32_t *pDns)
{
    char *token, *line;

    line = (char *)const_line;

    if (NULL != (token = strtok(line, " \t=,")))
    {
        read_ip(token, pDns++);
    }
    else
    {
        return;
    }

    if (NULL != (token = strtok(NULL, "")))
    {
        read_ip(token, pDns);
    }

    return;
}

static void read_dev_tpye(const char* const_line, DEVICE_TYPE *pDevType)
{
	if (!strncasecmp("PC", const_line, sizeof("PC") - 1))
		*pDevType = PC;
	else if (!strncasecmp("CAMERA", const_line, sizeof("CAMERA") - 1))
		*pDevType = CAMERA;
	else if (!strncasecmp("HGW", const_line, sizeof("HGW") - 1))
		*pDevType = HGW;
	else if (!strncasecmp("STB", const_line, sizeof("STB") -1))
		*pDevType = STB;
	else if (!strncasecmp("PHONE", const_line, sizeof("PHONE") - 1))
		*pDevType = PHONE;	
	else if (!strncasecmp("UNKNOWN", const_line, sizeof("UNKNOWN") - 1))
		*pDevType = UNKNOWN;	
	else
	    LOG_DEBUG("DHCPS: Unknown device type: %s.", const_line);
	
	return;
}


/* Read conditional-pools configs. */
static void read_cond_pool(char *const_line, struct cond_server_pool_t *pCondPool)
{   
    char *token, *line;

    if (NULL == pCondPool)
    {
        return;
    }

    line = (char *)const_line;

    while(1)
    {
        if (!(token = strtok(line, " \t=")))
            break;
        if (!(line = strtok(NULL, "")))
            break;
        line = line + strspn(line, " \t=");

        if (!strncmp(token, "vendor_id", sizeof("vendor_id")))
        {
            read_qstr(line, (char *)&(pCondPool->vendor_id), MAX_VENDOR_ID_LEN);
        }
        else if (!strncmp(token, "start", sizeof("start")))
        {
            read_ip(line, &(pCondPool->start));
        }
        else if (!strncmp(token, "end", sizeof("end")))
        {
            read_ip(line, &(pCondPool->end));
        }
        else if (!strncmp(token, "router", sizeof("router")))
        {
            read_ip(line, &(pCondPool->gateway));
        }
        else if (!strncmp(token, "device_type", sizeof("device_type")))
        {
            read_dev_tpye(line, &pCondPool->deviceTpye);
        }
        else if (!strncmp(token, "dns", sizeof("dns")))
        {
            read_dns_pair(line, pCondPool->dns);
        }

        if (!strtok(line, " \t")) break;
        if (!(line = strtok(NULL, ""))) break;
    }

    if (!pCondPool->vendor_id[0] || !pCondPool->start || !pCondPool->end) 
    {
        memset(pCondPool, 0, sizeof(struct cond_server_pool_t));
    }

    return;
}


static void read_reserved_option(char *const_line, struct cond_server_pool_t *pCondPool)
{
    char *token, *line;

    if (NULL == pCondPool)
    {
        return;
    }
    
    line = const_line;
    
    while(1)
    {
        if (!(token = strtok(line, " \t=")))
            break;
        if (!(line = strtok(NULL, "")))
            break;
        line = line + strspn(line, " \t=");

        if (!strncmp(token, "tag", sizeof("tag")))
        {
            read_u32(line, &pCondPool->optionCode);
        }
        else if (!strncmp(token, "value", sizeof("value")))
        {
            /* Modified by xcl, 20Mar12, 修复option value含有空格、引号时读取配置错误问题 */
            /*read_qstr(line, (char *)&(pCondPool->optionStr), MAX_OPTION_VALUE_LEN);*/
            if (pCondPool->optionCode == 0)
            {
                LOG_ERR("Option code must be pointed before its value!");
            }
            else
            {
                strncpy(pCondPool->optionStr, line, MAX_OPTION_VALUE_LEN -1);
                pCondPool->optionStr[MAX_OPTION_VALUE_LEN - 1] = '\0';
            }
            /* 读完option value后，option信息读取完毕 */
            break;
        }
        else
        {
            LOG_DEBUG("DHCPS: Failure parsing keyword '%s'", token);
        }

        if (!strtok(line, " \t")) break;
        if (!(line = strtok(NULL, ""))) break;
    }

    if (pCondPool->optionCode == 0 || pCondPool->optionStr[0] == '\0') 
    {
        memset(pCondPool, 0, sizeof(struct cond_server_pool_t));
    }

    return;
}
#endif /* DHCP_COND_SRV_POOL */

#ifdef DHCP_RELAY
static netiface *get_netifaces(int *count)
{
	netiface * netifaces = NULL;
	struct ifconf ifc;
	char buf[1024];
	int skt;
	int i;

	/* Create socket for querying interfaces */
	if ((skt = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return NULL;

	/* Query available interfaces. */
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if(ioctl(skt, SIOCGIFCONF, &ifc) < 0)
		goto err;

	/* Allocate memory for netiface array */
	if (ifc.ifc_len < 1)
		goto err;
	*count = ifc.ifc_len / sizeof(struct ifreq);
	netifaces = calloc(*count, sizeof(netiface));
	if (netifaces == NULL)
		goto err;

	/* Iterate through the list of interfaces to retrieve info */
	for (i = 0; i < *count; i++) {
		struct ifreq ifr;
		ifr.ifr_addr.sa_family = AF_INET;
		strcpy(ifr.ifr_name, ifc.ifc_req[i].ifr_name);

		/* Interface name */
		strcpy(netifaces[i].nif_name, ifc.ifc_req[i].ifr_name);

		/* Interface index */
		if (ioctl(skt, SIOCGIFINDEX, &ifr))
			goto err;
		netifaces[i].nif_index = ifr.ifr_ifindex;

		/* IPv4 address */
		if (ioctl(skt, SIOCGIFADDR, &ifr))
			goto err;
		netifaces[i].nif_ip = ((struct sockaddr_in*)&ifr.ifr_addr)->
				sin_addr.s_addr;

		/* MAC address */
		if (ioctl(skt, SIOCGIFHWADDR, &ifr))
			goto err;
		memcpy(netifaces[i].nif_mac, ifr.ifr_hwaddr.sa_data, 6);

	}
	close(skt);
	return netifaces;
err:
	close(skt);
	if (netifaces)
		free(netifaces);
	return NULL;
}

static void addIptablesRuleForDHCPRelay()
{
    system("iptables -A INPUT -p udp --dport 67 -j ACCEPT");
    /*system("iptables -t nat -I PREROUTING 1 -p udp --dport 67 -j ACCEPT");
    system("iptables -t nat -I POSTROUTING 1 -p udp --dport 67 -j ACCEPT");*/
}

static void removeIptablesRuleForDHCPRelay()
{
    system("iptables -D INPUT -p udp --dport 67 -j ACCEPT");
}

#endif

static int set_iface_config_defaults_if_not_set(void)
{
	int fd;
	struct ifreq ifr;
	struct sockaddr_in *sin;
	struct option_set *option;
	struct iface_config_t *iface;
	int retry_count;
	int local_rc;
    int i = 0;

	/* Create fd to retrieve interface info */
	if((fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
		LOG_WARNING("socket failed!");
		return 0;
	}

	for(i = 0; i < MAX_GROUP_NUM && iface_configs[i].in_use == 1; i++) 
	{
	  iface = &iface_configs[i];  
      if (iface->ifName == NULL || strstr(iface->ifName, "br") == 0)
         continue;

		/* Initialize socket to invalid */
		iface->fd = -1;

		/* Retrieve IP of the interface */
		local_rc = -1;
		for (retry_count = 0; retry_count < TP_RETRY_COUNT; retry_count++) 
		{
			ifr.ifr_addr.sa_family = AF_INET;
			strcpy(ifr.ifr_name, iface->ifName);
			if ((local_rc = ioctl(fd, SIOCGIFADDR, &ifr)) == 0) {
				sin = (struct sockaddr_in *) &ifr.ifr_addr;
				iface->server = sin->sin_addr.s_addr;
				LOG_DEBUG("server_ip(%s) = %s", ifr.ifr_name, inet_ntoa(sin->sin_addr));
				break;
			}
			sleep(TP_RETRY_INTERVAL);
		}
		if (local_rc < 0) {
			LOG_ERR("SIOCGIFADDR failed on %s!", ifr.ifr_name);
			close(fd);
			return 0;
		}

        /* Retrieve netmask of the interface */
		if (ioctl(fd, SIOCGIFNETMASK, &ifr) == 0){
		    sin = (struct sockaddr_in *)&ifr.ifr_addr;
		    iface->netmask = sin->sin_addr.s_addr;
            LOG_DEBUG("netmask(%s) = %s", ifr.ifr_name, inet_ntoa(sin->sin_addr)); 
		}

		/* Retrieve ifindex of the interface */
		if (ioctl(fd, SIOCGIFINDEX, &ifr) == 0) {
			LOG_DEBUG("ifindex(%s)  = %d", ifr.ifr_name, ifr.ifr_ifindex);
			iface->ifindex = ifr.ifr_ifindex;
		} else {
			LOG_ERR("SIOCGIFINDEX failed on %s!", ifr.ifr_name);
			close(fd);
			return 0;
		}

        /* Retrieve MAC of the interface */
		if (ioctl(fd, SIOCGIFHWADDR, &ifr) == 0) {
			memcpy(iface->arp, ifr.ifr_hwaddr.sa_data, 6);
			LOG_DEBUG("mac(%s) = "
				"%02x:%02x:%02x:%02x:%02x:%02x", ifr.ifr_name,
				iface->arp[0], iface->arp[1], iface->arp[2], 
				iface->arp[3], iface->arp[4], iface->arp[5]);
		} else {
			LOG_ERR("SIOCGIFHWADDR failed on %s!",
				ifr.ifr_name);
			close(fd);
			return 0;
		}

		/* Set default start and end if missing */
		if (iface->start == 0) {
			iface->start = (iface->server & htonl(0xffffff00)) | htonl(20);
		}
		if (iface->end == 0) {
			iface->end = (iface->server & htonl(0xffffff00)) | htonl(254);
		}
		/* it is not impossible */
	    if (iface->end < iface->start)
	    {
		    uint32_t tmp_ip = iface->start;
		    iface->start = iface->end ;
		    iface->end = tmp_ip;
	    }
	
		/* set lease time from option or default */
		if ((option = find_option(iface->options, DHCP_LEASE_TIME))) {
			memcpy(&iface->lease, option->data + 2, 4);
			iface->lease = ntohl(iface->lease);
		}
		else
			iface->lease = LEASE_TIME;
	}

	close(fd);
    return 1;
}


/* 
 * fn		static void *attachSharedBuff()
 * brief	Attach dhcps shared buffer	
 *
 * return	Shared buffer address if success, NULL if error 
 *
 * note 	Call dhcps_attachSharedBuff() before you want to use shared buffer,
 *			must call cmem_detachSharedBuff() after used.
 */
static void *attachSharedBuff()
{
	int shmId = 0;
	int shmFlg = 0;
	
	void *pAddr = NULL;
	OS_V_SEM *pSemId = NULL;

	shmFlg = IPC_CREAT | 0666;

	LOG_DEBUG("DHCPS: Start attach dhcps shared buffer.");
	
	/* attach shared buffer */
	if ((shmId = os_shmGet(DHCPS_SHARED_MEM_KEY, 
	                       DHCPS_SHARED_MEM_SIZE + sizeof(OS_V_SEM), shmFlg)) < 0)
	{
		LOG_ERR("DHCPS: Get shared buffer error.");
		return NULL;
	}

	if (NULL == (pAddr = os_shmAt(shmId, NULL, shmFlg)))
	{
		LOG_ERR("DHCPS: Attach shared buffer error.");
		return NULL;		
	}

	memset((pAddr + sizeof(OS_V_SEM)), 0, DHCPS_SHARED_MEM_SIZE);

	/* lock */
	pSemId = (OS_V_SEM *)pAddr;

	if (os_semVTake(*pSemId) < 0)
	{
		LOG_ERR("DHCPS: Take shared semaphore error.");
		return NULL;		
	}

	LOG_DEBUG("DHCPS: Attach shared buffer success.");
	
	return (void *)((char *)pAddr + sizeof(OS_V_SEM));
}


/* 
 * fn		static int detachSharedBuff(const void *pShmAddr)
 * brief	Detach dhcps shared buffer
 *
 * param[in]	Shared buffer address that we want to detach
 *
 * return	 0 if success, -1 if error		
 */
static int detachSharedBuff(const void *pShmAddr)
{
	void *pAddr = NULL;
	OS_V_SEM *pSemId = NULL;

	if (NULL == pShmAddr)
	{
	    return -1;
	}
		
	pAddr = (void *)((char *)pShmAddr - sizeof(OS_V_SEM));

	LOG_DEBUG("DHCPS: Start detach shared buffer.");

	/* unlock */
	pSemId = (OS_V_SEM *)pAddr;

	if (os_semVGive(*pSemId) < 0)
	{
		LOG_ERR("DHCPS: Give shared semaphore error.");
		return -1;		
	}
	
	/* detach shared buffer */
	if (os_shmDt(pAddr) < 0)
	{
		LOG_ERR("DHCPS: Detach shared buffer error.");
		return -1;
	}

	LOG_DEBUG("DHCPS: Detach shared buffer success.");
	
	return 0;
}

/**************************************************************************************************/
/*                                           PUBLIC_FUNCTIONS                                     */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           GLOBAL_FUNCTIONS                                     */
/**************************************************************************************************/
int read_dhcps_config(const char *file)
{
    FILE *in;
	char buffer[READ_CONFIG_BUF_SIZE] = {0}; 
	char *token, *line;
	int lineNum = 0;
	int offset = 0;
	char tmpIfName[MAX_INTF_NAME] = {0};
	int cur_iface_index = 0;
	int i = 0;
	struct cond_server_pool_t *pCurCondPool = NULL;
#ifdef DHCP_RELAY	
	int relayEnabled = 0;
#endif /* DHCP_RELAY */

    /* Read config file.*/
	if (!(in = fopen(file, "r"))) 
	{
		LOG_ERR("unable to open config file: %s", file);
		return 0;
	}

	while (fgets(buffer, READ_CONFIG_BUF_SIZE, in)) 
	{
		lineNum++;
		if (strchr(buffer, '\n')) *(strchr(buffer, '\n')) = '\0';
		if (strchr(buffer, '#')) *(strchr(buffer, '#')) = '\0';

		if (!(token = strtok(buffer, " \t"))) continue;
		if (!(line = strtok(NULL, ""))) continue;

		/* eat leading whitespace */
		line = line + strspn(line, " \t=");
		/* eat trailing whitespace */
		for (offset = strlen(line); offset > 0 && isspace(line[offset - 1]); offset--);
		line[offset] = '\0';

		if (strcasecmp(token, "interface") == 0) 
		{
			/* Read interface name */
			strncpy(tmpIfName, line, MAX_INTF_NAME);
			if (!tmpIfName[0])
				continue;
				
			/* Lookup read interfaces. If this interface already read, ignore it */
			for (i = 0; i <= cur_iface_index; i++) 
			{
				if (iface_configs[i].ifName[0] != '\0' 
				        && strcmp(iface_configs[i].ifName, tmpIfName) == 0) 
				{
				    break;
				}
			}
			if (i <= cur_iface_index)
				continue;
    
			/* Assign the interface name to the first iface */
			if (iface_configs[cur_iface_index].ifName[0] == '\0')
			{
			    strncpy(iface_configs[cur_iface_index].ifName, tmpIfName, MAX_INTF_NAME);
			    iface_configs[cur_iface_index].in_use = 1;
			}
			/* Finish the current iface, start a new one */
			else {
				cur_iface_index++;
				strncpy(iface_configs[cur_iface_index].ifName, tmpIfName, MAX_INTF_NAME);
				iface_configs[cur_iface_index].in_use = 1;
				if (cur_iface_index >= MAX_GROUP_NUM)
				    return 0;
			}
		} else if (strcasecmp(token, "start") == 0)
			read_ip(line, &iface_configs[cur_iface_index].start);
		else if (strcasecmp(token, "end") == 0)
			read_ip(line, &iface_configs[cur_iface_index].end);
		else if (strcasecmp(token, "option") == 0 ||
			strcasecmp(token, "opt") == 0)
			read_opt(line, &iface_configs[cur_iface_index].options);
		else if (strcasecmp(token, "decline_time") == 0)
			read_u32(line, &server_config.decline_time);
		else if (strcasecmp(token, "conflict_time") == 0)
			read_u32(line, &server_config.conflict_time);
		else if (strcasecmp(token, "offer_time") == 0)
			read_u32(line, &server_config.offer_time);
		else if (strcasecmp(token, "min_lease") == 0)
			read_u32(line, &server_config.min_lease);
		else if (strcasecmp(token, "lease_file") == 0)
			strncpy(server_config.lease_file, line, MAX_FILE_NAME_LEN);
		else if (strcasecmp(token, "siaddr") == 0)
			read_ip(line, &iface_configs[cur_iface_index].siaddr);
		else if (strcasecmp(token, "sname") == 0)
			strncpy(iface_configs[cur_iface_index].sname, line, MAX_BOOTP_SRV_NAME_LEN);
		else if (strcasecmp(token, "boot_file") == 0)
			strncpy(iface_configs[cur_iface_index].boot_file, line, MAX_FILE_NAME_LEN);
		else if (strcasecmp(token, "static_lease") == 0)
			read_staticlease(line, NULL);
#ifdef DHCP_RELAY
	    else if (strcasecmp(token, "relay")== 0){
	        relayEnabled = 1;
	        read_ip(line, &iface_configs[cur_iface_index].dhcps_remote);
	    }
#endif /* DHCP_RELAY */

#ifdef DHCP_COND_SRV_POOL
        else if (strcasecmp(token, "cond_pool") == 0)
        {
            /* 在cur_iface中找到一个未用到cond_server_pools */
            for (i = 0; i < MAX_COND_SERVER_POOLS; i++)
            {
                if (iface_configs[cur_iface_index].cond_server_pools[i].start == 0)
                {
                    pCurCondPool = &(iface_configs[cur_iface_index].cond_server_pools[i]);
                    break;
                }
            }

            if (i < MAX_COND_SERVER_POOLS && pCurCondPool != NULL)
                read_cond_pool(line, pCurCondPool);
        }
        else if (strcasecmp(token, "reserved_options") == 0)
        {
            read_reserved_option(line, pCurCondPool);
        }
#endif /* DHCP_COND_SRV_POOL */        
	    else
			LOG_WARNING("Failure parsing line %d of keyword '%s'", lineNum, token);
	}

	fclose(in);	

	/* Finish interface config automatically */
	set_iface_config_defaults_if_not_set();

#ifdef DHCP_RELAY
    /* set iptables rule for dhcp relay */
    removeIptablesRuleForDHCPRelay();
    
	if (relayEnabled == 1)
	{
	    LOG_DEBUG("relayEnabled = %d", relayEnabled);
        addIptablesRuleForDHCPRelay();
	}

	set_relay();
#endif /* DHCP_RELAY */	

    LOG_DEBUG("====================conditional pools======================");
    LOG_DEBUG("vid = %s, sip = %0x, eip = %0x, gw = %0x, dns = %0x,%0x, dev_type = %d,"
             "opt_tag = %d, value = %s", 
             iface_configs[0].cond_server_pools[0].vendor_id, 
             iface_configs[0].cond_server_pools[0].start, 
             iface_configs[0].cond_server_pools[0].end,
             iface_configs[0].cond_server_pools[0].gateway, 
             iface_configs[0].cond_server_pools[0].dns[0],
             iface_configs[0].cond_server_pools[0].dns[1],
             iface_configs[0].cond_server_pools[0].deviceTpye,
             iface_configs[0].cond_server_pools[0].optionCode,
             iface_configs[0].cond_server_pools[0].optionStr);
    
	return 1;
}


void write_leases(void)
{
	FILE *fp;
	unsigned int j = 0;
	unsigned long tmp_time;

	if (!(fp = fopen(server_config.lease_file, "w"))) {
	    LOG_ERR("Unable to open %s for writing", server_config.lease_file);
		return;
	}

	for (j = 0; j < MAX_CLIENT_NUM; j++)
    {
		if (leases[j].yiaddr != 0) 
		{
            if (lease_expired(&leases[j]))
            {
                memset(&leases[j], 0, sizeof(struct dhcpOfferedAddr));
            }
            else
            {
                if (leases[j].host_name[0] == 0 || 
					(leases[j].chaddr[0] == 0 &&
					leases[j].chaddr[1] == 0 &&
					leases[j].chaddr[2] == 0 &&
					leases[j].chaddr[3] == 0 &&
					leases[j].chaddr[4] == 0 &&
					leases[j].chaddr[5] == 0) )
				{
					continue;
				}
				
				if (leases[j].state != DHCPACK)
				{
					continue;
				}
    			/* screw with the time in the struct, for easier writing */
    			tmp_time = leases[j].expires;

    			if (leases[j].expires != 0xFFFFFFFF) 
    			{
    				leases[j].expires -= time(0);
    			} /* else stick with the time we got */
    			leases[j].expires = htonl(leases[j].expires);

    			fwrite(&leases[j], sizeof(struct dhcpOfferedAddr), 1, fp);
    			/* Then restore it when done. */
    			leases[j].expires = tmp_time;
			}
		}
    }
	fclose(fp);
}

void read_leases(void)
{
    FILE *fp;
	struct dhcpOfferedAddr lease;
	int count = 0;
	
	if (!(fp = fopen(server_config.lease_file, "r"))) {
		LOG_ERR("Unable to open %s for reading", server_config.lease_file);
		return;
	}

	while ((fread(&lease, sizeof(struct dhcpOfferedAddr), 1, fp) == 1)) 
	{
	    lease.expires += time(0);
		memcpy(&leases[count++], &lease, sizeof(struct dhcpOfferedAddr));
	}
	
	fclose(fp);
}


void write_leases_to_shm(void)
{
	int i = 0;
	int index = 0;

    leases_shm = (struct dhcpOfferedAddr *)attachSharedBuff();

	for (i = 0; i < MAX_CLIENT_NUM; i++)
	{
		if (leases[i].yiaddr != 0)
		{
			/* only record when remaining is true */
			if (lease_expired(&leases[i]))
			{
				memset(&leases[i], 0, sizeof(struct dhcpOfferedAddr));
			}
			else	/* record those not expired */
			{
				if (leases[i].host_name[0] == 0 || 
					(leases[i].chaddr[0] == 0 &&
					leases[i].chaddr[1] == 0 &&
					leases[i].chaddr[2] == 0 &&
					leases[i].chaddr[3] == 0 &&
					leases[i].chaddr[4] == 0 &&
					leases[i].chaddr[5] == 0) )
				{
					continue;
				}
				
				if (leases[i].state != DHCPACK)
				{
					continue;
				}

				memcpy(&leases_shm[index], &leases[i], sizeof(struct dhcpOfferedAddr));
				
				if (leases_shm[index].expires != 0xFFFFFFFF)
					leases_shm[index].expires -= time(0);
                
				index++;    
			}
		}
	}
	
	LOG_DEBUG("DHCPS: %d clients.", index);
	
	memset(&leases_shm[index], 0, sizeof(struct dhcpOfferedAddr));	    

	detachSharedBuff(leases_shm);
}

#ifdef DHCP_RELAY
void set_relay(void)
{
	int skt;
	int socklen;
	struct sockaddr_in addr;
	struct iface_config_t *iface;

	netiface *nifs = NULL;
	int nif_count;
	int i, j;

	/* 1. Release all relays */
	for (i = 0; i < MAX_WAN_NUM; i++)
	{
	    if (relay_skts[i].skt >= 0)
        {
            close(relay_skts[i].skt);
            memset(&relay_skts[i], 0, sizeof(struct relay_skt_t));
	        relay_skts[i].skt = -1;
	    }
	}

	/* Get network interface array */
	for (i = 0; i < TP_RETRY_COUNT; i++) {
		if ((nifs = get_netifaces(&nif_count)))
			break;
		if (i < TP_RETRY_COUNT)
			sleep(TP_RETRY_INTERVAL);
	}
	if (nifs == NULL) {
		LOG_ERR("DHCPS: failed querying interfaces");
		return;
	}

	/* Create UDP for looking up routes */
	if ((skt = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		free(nifs);
		return;
	}

    /* 为每个有设置relay_remote的iface寻找正确的relay_interface(即正确的wan口) */
	for (i = 0; i < MAX_GROUP_NUM && iface_configs[i].in_use == 1; i++) {
	    iface = &iface_configs[i];
		/* Is this a relay interface? */
		if (iface->dhcps_remote == 0)
			continue;

		/* Connect UDP socket to relay to find out local IP address,即wan口ip */
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = SERVER_PORT;
		addr.sin_addr.s_addr = iface->dhcps_remote;
		if (connect(skt, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
			LOG_WARNING("DHCPS: no route to relay %u.%u.%u.%u",
			    ((unsigned char *)&addr.sin_addr.s_addr)[0],
			    ((unsigned char *)&addr.sin_addr.s_addr)[1],
			    ((unsigned char *)&addr.sin_addr.s_addr)[2],
			    ((unsigned char *)&addr.sin_addr.s_addr)[3]);
			continue;
		}
		socklen = sizeof(addr);
		if (getsockname(skt, (struct sockaddr *)&addr, (socklen_t *)&socklen) < 0)
			continue;

		/* Iterate through the list of interfaces to find the one that
		 * has route to remote DHCP server */
		for (i = 0; i < nif_count; i++) {
			if (nifs[i].nif_ip == addr.sin_addr.s_addr) {
				strcpy(iface->relay_ifName, nifs[i].nif_name);
				break;
			}
		}

		if (iface->relay_ifName[0] == 0)
		{
		    continue;
		}

		/* If the same relay (same relay interface) has been created,
		 * don't do it again */
		for (j = 0; j < MAX_WAN_NUM; j ++)
		{
		    if (relay_skts[j].ifName[0] != 0 && !strcmp(relay_skts[j].ifName, iface->relay_ifName))
				break;
		}

        if (j >= MAX_WAN_NUM) /* 不存在相同的relay */
        {
            /* 找到未用的relay_skts[] */
            for (i = 0; i < MAX_WAN_NUM && relay_skts[i].ifName[0] == 0; i++)
            {
                strcpy(relay_skts[i].ifName, iface->relay_ifName);
    		    relay_skts[i].ip = (uint32_t)(addr.sin_addr.s_addr);
    		    relay_skts[i].skt = listen_socket_on_ip(relay_skts[i].ip, SERVER_PORT);

    		    if (relay_skts[i].skt == -1)
    		    {
    		        LOG_ERR("couldn't create socket on ip %0X", relay_skts[i].ip);
    				memset(&relay_skts[i], 0, sizeof(struct relay_skt_t));    
    				relay_skts[i].skt = -1;
    		    }

    		    break;
            }
		}
	}
	
	close(skt);
	free(nifs);
}
#endif /* DHCP_RELAY */

