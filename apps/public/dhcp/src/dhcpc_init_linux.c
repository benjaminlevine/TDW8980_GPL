/*  Copyright(c) 2009-2011 Shenzhen TP-LINK Technologies Co.Ltd.
 *
 * file		dhcpc_init_linux.c
 * brief	this file is only for linux os.	
 * details	
 *
 * author	Xu Chenglong
 * version	1.0.0
 * date		09Aug11
 *
 * history 	\arg 1.0.0, 09Aug11, Xu Chenglong, create file. 	
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
#include <unistd.h>

#include "common.h"
#include "dhcpc.h"
#include "dhcpc_init.h"

/**************************************************************************************************/
/*                                           DEFINES                                              */
/**************************************************************************************************/
#define LINE_BUF_SIZE    128

/**************************************************************************************************/
/*                                           TYPES                                                */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           EXTERN_PROTOTYPES                                    */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           LOCAL_PROTOTYPES                                     */
/**************************************************************************************************/
static int read_yn(const char *line, void *arg);
static int read_dhcpc_config(const char * file);
static void init_clients_cfg(void);

/**************************************************************************************************/
/*                                           VARIABLES                                            */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           LOCAL_FUNCTIONS                                      */
/**************************************************************************************************/
/* 
 * fn		static int read_yn(const char *line, void *arg)
 * brief	read bool type value
 * details	
 *
 * param[in]	line - input string
 * param[out]	arg - bool type output paramerter
 *
 * return	
 * retval	0 - error happen
 * retval   1 - succeed
 *
 * note		
 */
static int read_yn(const char *line, void *arg)
{
	int *dest = arg;
	int retval = 1;

	if (!strcasecmp("yes", line))
		*dest = 1;
	else if (!strcasecmp("no", line))
		*dest = 0;
	else retval = 0;

	return retval;
}

/* 
 * fn		static int read_dhcpc_config(const char *file)
 * brief	read dhcpc config file
 * details	
 *
 * param[in]	file - config file path
 * param[out]	N/A
 *
 * return	
 * retval	-1 - error happen
 * retval   0 - succeed
 *
 * note		
 */
static int read_dhcpc_config(const char *file)
{
    FILE *in;
	char buffer[LINE_BUF_SIZE] = {0}; 
	char *token, *line;
	int offset = 0;
	char tmpIfName[MAX_INTF_NAME] = {0};
	int cur_client_index = 0;
	int i = 0;

    /* Read config file.*/
	if (!(in = fopen(file, "r"))) 
	{
		LOG_ERR("unable to open config file: %s", file);
		return -1;
	}

	while (fgets(buffer, LINE_BUF_SIZE, in)) 
	{
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
			for (i = 0; i <= cur_client_index; i++) 
			{
				if (dhcp_clients[i].ifName[0] != '\0' 
				        && strcmp(dhcp_clients[i].ifName, tmpIfName) == 0) 
				{
				    break;
				}
			}
			if (i <= cur_client_index)
				continue;
    
			/* Assign the interface name to the first iface */
			if (dhcp_clients[cur_client_index].ifName[0] == '\0')
			{
			    strncpy(dhcp_clients[cur_client_index].ifName, tmpIfName, MAX_INTF_NAME);
			}
			/* Finish the current iface, start a new one */
			else {
				cur_client_index++;
				strncpy(dhcp_clients[cur_client_index].ifName, tmpIfName, MAX_INTF_NAME);
				if (cur_client_index >= MAX_GROUP_NUM)
				    return 0;
			}
		} 
		else if (strcasecmp(token, "hostname") == 0)
			strncpy(dhcp_clients[cur_client_index].hostName, line, MAX_HOST_NAME_LEN);
		else if (strcasecmp(token, "unicast") == 0)
			read_yn(line, &dhcp_clients[cur_client_index].bootp_flags);
	    else
			LOG_WARNING("Failure parsing keyword '%s'", token);
	}

	fclose(in);

	init_clients_cfg();

	return 0;
}


/* 
 * fn		static void init_clients_cfg(void)
 * brief	initialize dhcp clients configuration
 * details	
 *
 * param[in]	N/A
 * param[out]	N/A
 *
 * return	
 * retval	
 *
 * note		
 */
static void init_clients_cfg(void)
{   
    struct client_info_t *pClient = NULL;
    int clientIndex = 0;
    char tmpHostName[MAX_HOST_NAME_LEN] = {0};
    int hostNameLen = 0;

    for (clientIndex = 0; 
         clientIndex < MAX_WAN_NUM && dhcp_clients[clientIndex].ifName[0] != '\0'; 
         clientIndex++)
    {
        pClient = &dhcp_clients[clientIndex];

        /* mac address */
        getMacFromIfName(pClient->ifName, pClient->arp);

        /* state */
        pClient->state = INIT_SELECTING;

        /* listen_mode */
        pClient->listen_mode = LISTEN_RAW;

        /* hostname */
        memcpy(tmpHostName, pClient->hostName, MAX_HOST_NAME_LEN);
        memset(pClient->hostName, 0, MAX_HOST_NAME_LEN + 2);
        pClient->hostName[OPT_CODE] = DHCP_HOST_NAME;
        hostNameLen = strlen(tmpHostName); 
        pClient->hostName[OPT_LEN] = hostNameLen > MAX_HOST_NAME_LEN ? MAX_HOST_NAME_LEN : hostNameLen;
        strncpy(&(pClient->hostName[OPT_DATA]), tmpHostName, MAX_HOST_NAME_LEN);
        pClient->hostName[pClient->hostName[OPT_LEN] + 2] = '\0';

        /* clientId */
        pClient->clientId[OPT_CODE] = DHCP_CLIENT_ID;
        pClient->clientId[OPT_LEN] = 7;
        pClient->clientId[OPT_DATA] = 1;
        memcpy(&(pClient->clientId[OPT_DATA + 1]),  pClient->arp, 6);
        pClient->clientId[pClient->clientId[OPT_LEN] + 2] = '\0';

        /* ifIndex */
		getIfIndexFormifName(pClient->ifName, &(pClient->ifindex));
    }     

    return;
}


/**************************************************************************************************/
/*                                           PUBLIC_FUNCTIONS                                     */
/**************************************************************************************************/
/* 
 * fn		int getIfIndexFormifName(const char *pIfName, int *pIfIndex)
 * brief	get ifIndex by the given ifName
 * details	
 *
 * param[in]	pIfName - interface name
 * param[out]	pIfIndex - the ifIndex of the interface
 *
 * return	int
 * retval	-1 - error happened
 * retval   0 - succeed
 *
 * note		
 */
int getIfIndexFormifName(const char *pIfName, int *pIfIndex)
{
    int fd;
	struct ifreq ifr;

	/* Create fd to retrieve interface info */
	if((fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) 
	{
		LOG_WARNING("socket failed!");
		return -1;
	}

	ifr.ifr_addr.sa_family = AF_INET;
	strcpy(ifr.ifr_name, pIfName);

	/* Retrieve ifindex of the interface */
	if (ioctl(fd, SIOCGIFINDEX, &ifr) == 0) 
	{
		LOG_DEBUG("ifindex(%s)  = %d", ifr.ifr_name, ifr.ifr_ifindex);
		*pIfIndex = ifr.ifr_ifindex;
	} 
	else 
	{
		LOG_ERR("SIOCGIFINDEX failed on %s!", ifr.ifr_name);
		close(fd);
		return -1;
	}

	close(fd);

	return 0;
}

/* 
 * fn		int init_dhcpc()
 * brief	initialize memory and read config file
 * details	
 *
 * param[in]	N/A
 * param[out]	N/A
 *
 * return	
 * retval	
 *
 * note		
 */
int init_dhcpc()
{
    int loop = 0;
    
    memset(dhcp_clients, 0, sizeof(struct client_info_t) * MAX_WAN_NUM);
    
    for (loop = 0; loop < MAX_WAN_NUM; loop++)
    {
        dhcp_clients[loop].skt = -1;
        dhcp_clients[loop].state = NO_USE;
    }

    read_dhcpc_config(DHCPC_CONF_FILE);

    return 0;
}

/* 
 * fn		void delete_client()
 * brief	delete dhcp clients not in config file
 * details	
 *
 * param[in]	N/A
 * param[out]	N/A
 *
 * return	
 * retval	
 *
 * note		考虑到web端可能同时删除多条连接，但消息管道容量仅为2，故删除连接操作时，重读配置文件。
 */
void delete_client()
{
    FILE *in;
	char buffer[LINE_BUF_SIZE] = {0}; 
	char *token, *line;
	int offset = 0;
    char ifNames[MAX_INTF_NAME][MAX_WAN_NUM] = {{0}};
    int dhcpcNum = 0;
    int i = 0, j = 0;

    /* Read config file.*/
	if (!(in = fopen(DHCPC_CONF_FILE, "r"))) 
	{
		LOG_ERR("unable to open config file: %s", DHCPC_CONF_FILE);
		return;
	}

	while (fgets(buffer, LINE_BUF_SIZE, in)) 
	{
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
			strncpy(ifNames[dhcpcNum++], line, MAX_INTF_NAME);
	    else
			continue;
	}

	fclose(in);

    for (i = 0; i < MAX_WAN_NUM; i++)
    {
        if (NO_USE == dhcp_clients[i].state)
        {
            continue;
        }
        
        for (j = 0; j < dhcpcNum; j++)
        {
            if (0 == strcmp(dhcp_clients[i].ifName, ifNames[j]))
                break;
        }

        /* 未找到此接口，应释放资源 */
        if (j == dhcpcNum)
        {
            LOG_DEBUG("DHCPC: DEL DHCP CLIENT OF (%s).", dhcp_clients[i].ifName);
            if (dhcp_clients[i].skt >= 0)
                close(dhcp_clients[i].skt);
            memset(&dhcp_clients[i], 0, sizeof(struct client_info_t));
            dhcp_clients[i].skt = -1;
            dhcp_clients[i].state = NO_USE;
        }
    }

    return;
}


/* 
 * fn		int getMacFromIfName(char *pIfName, uint8_t *pMac)
 * brief	get mac by interface name
 * details	
 *
 * param[in]	pIfName - interface name
 * param[out]	pMac - mac of the interface
 *
 * return	int
 * retval	-1 - error happened
 * retval   0 - succeed
 *
 * note		
 */
int getMacFromIfName(char *pIfName, uint8_t *pMac)
{
    int fd;
	struct ifreq ifr;

	/* Create fd to retrieve interface info */
	if((fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) 
	{
		LOG_WARNING("socket failed!");
		return -1;
	}
	
	strcpy(ifr.ifr_name, pIfName);

    /* Retrieve MAC of the interface */
	if (ioctl(fd, SIOCGIFHWADDR, &ifr) != 0)
	{
	    LOG_ERR("SIOCGIFHWADDR failed on %s!",ifr.ifr_name);
		close(fd);
		return -1;
	} 

    memcpy(pMac, ifr.ifr_hwaddr.sa_data, 6);

    LOG_DEBUG("mac(%s) = %02x:%02x:%02x:%02x:%02x:%02x", ifr.ifr_name,
		      pMac[0], pMac[1], pMac[2], pMac[3], pMac[4], pMac[5]);

    close(fd);
		      
    return 0;
}

/* 
 * fn		int chkIpInAnyLanSubnet(uint32_t testIp)
 * brief	if the test ip is in the same subnet as any lan
 *
 * param[in]	testIP - The ip address in network order to test
 *
 * return	int
 * retval	1	- In the same subnet.
 * retval	0	- Not in the same subnet.
 */
int chkIpInAnyLanSubnet(uint32_t testIp)
{
	return 0;
}


/**************************************************************************************************/
/*                                           GLOBAL_FUNCTIONS                                     */
/**************************************************************************************************/

