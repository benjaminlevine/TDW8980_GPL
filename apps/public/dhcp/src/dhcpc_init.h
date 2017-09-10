/*  Copyright(c) 2009-2011 Shenzhen TP-LINK Technologies Co.Ltd.
 *
 * file		dhcpc_init.h
 * brief		
 * details	
 *
 * author		Xu Chenglong
 * version	
 * date		09Aug11
 *
 * history 	\arg	
 */
 
/**************************************************************************************************/
/*                                           DEFINES                                              */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           TYPES                                                */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           VARIABLES                                            */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           FUNCTIONS                                            */
/**************************************************************************************************/
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
int init_dhcpc();


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
void delete_client();



/* 
 * fn		int chkIpInAnyLanSubnet(uint32_t testIp)
 * brief	if the test ip is in the same subnet as any any 
 *
 * param[in]	testIP - The ip address in network order to test
 *
 * return	int
 * retval	1	- In the same subnet.
 * retval	0	- Not in the same subnet.
 */
int chkIpInAnyLanSubnet(uint32_t testIp);

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
int getMacFromIfName(char *pIfName, uint8_t *pMac);

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
int getIfIndexFormifName(const char *pIfName, int *pIfIndex);
