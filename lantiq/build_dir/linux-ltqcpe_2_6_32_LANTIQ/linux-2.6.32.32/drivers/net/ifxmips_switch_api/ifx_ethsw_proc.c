/*  Copyright(c) 2009-2012 Shenzhen TP-LINK Technologies Co.Ltd.
 *
 * file		ifx_ethsw_proc.c
 * brief		
 * details	
 *
 * author	Wu Zhiqin
 * version	
 * date		25Apr12
 *
 * history 	\arg	
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <ifx_ethsw_kernel_api.h>
#include <ifx_ethsw_api.h>
#include <asm-generic/ioctl.h>


static int l_lan_port[] = {4, 2, 0, 5};
static int l_pseudo_port[] = {7, 8, 9};
static int l_lan_vid[] = {0x802, 0x803, 0x804, 0x805};

/* define in ifxmips_gpio.c */
extern struct proc_dir_entry *g_tplink;


static int strincmp(const char *, const char *, int);
static int vlan_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data);
static int vlan_proc_write(struct file *file, const char *buf, unsigned long count, void *data);
static int ethsw_sprintf(uint8_t * buf, const uint8_t *fmt, ...);
static int CfgGet(IFX_ETHSW_HANDLE handle, union ifx_sw_param *pParam);
static int CfgSet(IFX_ETHSW_HANDLE handle, 
				IFX_ETHSW_ageTimer_t eMAC_TableAgeTimer, 
				IFX_boolean_t bVLAN_Aware,
				IFX_uint16_t nMaxPacketLen, 
				IFX_boolean_t bPauseMAC_ModeSrc, 
				IFX_uint8_t *pPauseMAC_Src);
static int VLAN_IdCreate(IFX_ETHSW_HANDLE handle, 
						IFX_uint16_t nVId, 
						IFX_uint32_t nFId);
static int VLAN_IdDelete(IFX_ETHSW_HANDLE handle, 
						IFX_uint16_t nVId);
static int VLAN_PortMemberAdd(IFX_ETHSW_HANDLE handle, 
								IFX_uint16_t nVId, 
								IFX_uint8_t nPortId, 
								IFX_boolean_t bVLAN_TagEgress);
static int VLAN_PortMemberRemove(IFX_ETHSW_HANDLE handle, 
									IFX_uint16_t nVId, 
									IFX_uint8_t nPortId);
static int VLAN_PortCfgSet(IFX_ETHSW_HANDLE handle, 
							IFX_uint8_t nPortId, 
							IFX_uint16_t nPortVId, 
							IFX_boolean_t bVLAN_UnknownDrop, 
							IFX_boolean_t bVLAN_ReAssign, 
							IFX_ETHSW_VLAN_MemberViolation_t eVLAN_MemberViolation, 
							IFX_ETHSW_VLAN_Admit_t eAdmitMode, 
							IFX_boolean_t bTVM);
static int RegisterGet(IFX_ETHSW_HANDLE handle, union ifx_sw_param *pParam);
static int RegisterSet(IFX_ETHSW_HANDLE handle, 
					IFX_uint32_t nRegAddr, 
					IFX_uint32_t nData);
static int vlan_enable(IFX_ETHSW_HANDLE handle);
static int vlan_disable(IFX_ETHSW_HANDLE handle);
#if 0 /* Wu Zhiqin, 2012-07-06 */
static int ewan_enable(IFX_ETHSW_HANDLE handle);
static int ewan_disable(IFX_ETHSW_HANDLE handle);
#endif

static int strincmp(const char *p1, const char *p2, int n)
{
    int c1 = 0, c2;

    while ( n && *p1 && *p2 )
    {
        c1 = *p1 >= 'A' && *p1 <= 'Z' ? *p1 + 'a' - 'A' : *p1;
        c2 = *p2 >= 'A' && *p2 <= 'Z' ? *p2 + 'a' - 'A' : *p2;
        if ( (c1 -= c2) )
            return c1;
        p1++;
        p2++;
        n--;
    }

    return n ? *p1 - *p2 : c1;
}

static int ethsw_sprintf(uint8_t * buf, const uint8_t *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i=vsprintf(buf,fmt,args);
    va_end(args);
    return i;
}


static int CfgGet(IFX_ETHSW_HANDLE handle, union ifx_sw_param *pParam)
{
    int ret;

    ret = ifx_ethsw_kioctl(handle, IFX_ETHSW_CFG_GET, (unsigned int)&(pParam->cfg_Data));


    if(ret != IFX_SUCCESS)
    {
        printk("IFX_ETHSW_CFG_GET Error\n");
        return IFX_ERROR;
    }

    return IFX_SUCCESS;
}

static int CfgSet(IFX_ETHSW_HANDLE handle, 
				IFX_ETHSW_ageTimer_t eMAC_TableAgeTimer, 
				IFX_boolean_t bVLAN_Aware,
				IFX_uint16_t nMaxPacketLen, 
				IFX_boolean_t bPauseMAC_ModeSrc, 
				IFX_uint8_t *pPauseMAC_Src)
{
    int ret;
	union ifx_sw_param x;
    memset(&x.cfg_Data, 0x00, sizeof(x.cfg_Data));
	
    x.cfg_Data.eMAC_TableAgeTimer = eMAC_TableAgeTimer; // 10 seconds
    x.cfg_Data.bVLAN_Aware = bVLAN_Aware; // Disable
    x.cfg_Data.nMaxPacketLen = nMaxPacketLen; // 1518 bytes
    x.cfg_Data.bPauseMAC_ModeSrc = bPauseMAC_ModeSrc; // Enable;
    x.cfg_Data.nPauseMAC_Src[0] = pPauseMAC_Src[0];
    x.cfg_Data.nPauseMAC_Src[1] = pPauseMAC_Src[1];
    x.cfg_Data.nPauseMAC_Src[2] = pPauseMAC_Src[2];
    x.cfg_Data.nPauseMAC_Src[3] = pPauseMAC_Src[3];
    x.cfg_Data.nPauseMAC_Src[4] = pPauseMAC_Src[4];
    x.cfg_Data.nPauseMAC_Src[5] = pPauseMAC_Src[5];
	
    ret = ifx_ethsw_kioctl(handle, IFX_ETHSW_CFG_SET, (unsigned int)&x.cfg_Data);

    if(ret != IFX_SUCCESS)
    {
        printk("IFX_ETHSW_CFG_SET Error\n");
        return IFX_ERROR;
    }

    return IFX_SUCCESS;
}


int PortCfgGet(IFX_ETHSW_HANDLE handle, union ifx_sw_param *pParam)
{
    int ret;

    ret = ifx_ethsw_kioctl(handle, IFX_ETHSW_PORT_CFG_GET, (unsigned int)&(pParam->portcfg));


    if(ret != IFX_SUCCESS)
    {
        printk("IFX_ETHSW_PORT_CFG_GET Error\n");
        return IFX_ERROR;
    }

    return IFX_SUCCESS;
}

int PortCfgSet(IFX_ETHSW_HANDLE handle, 
				IFX_uint8_t nPortId, 
				IFX_ETHSW_portEnable_t eEnable, 
				IFX_boolean_t bUnicastUnknownDrop, 
				IFX_boolean_t bMulticastUnknownDrop, 
				IFX_boolean_t bReservedPacketDrop, 
				IFX_boolean_t bBroadcastDrop, 
				IFX_boolean_t bAging, 
				IFX_boolean_t bLearningMAC_PortLock, 
				IFX_uint16_t  nLearningLimit, 
				IFX_ETHSW_portFlow_t eFlowCtrl, 
				IFX_ETHSW_portMonitor_t ePortMonitor)
{
    int ret;
	union ifx_sw_param x;
    memset(&x.portcfg, 0x00, sizeof(x.portcfg));
    x.portcfg.nPortId = nPortId;
    x.portcfg.eEnable = eEnable;
    x.portcfg.bUnicastUnknownDrop = bUnicastUnknownDrop;
    x.portcfg.bMulticastUnknownDrop = bMulticastUnknownDrop;
    x.portcfg.bReservedPacketDrop = bReservedPacketDrop;
    x.portcfg.bBroadcastDrop = bBroadcastDrop;
    x.portcfg.bAging = bAging;
    x.portcfg.bLearningMAC_PortLock = bLearningMAC_PortLock;
    x.portcfg.nLearningLimit = nLearningLimit;
    x.portcfg.ePortMonitor = ePortMonitor;
    x.portcfg.eFlowCtrl = eFlowCtrl;
    ret = ifx_ethsw_kioctl(handle, IFX_ETHSW_PORT_CFG_SET, (unsigned int)&x.portcfg);

    if(ret != IFX_SUCCESS)
    {
        printk("IFX_ETHSW_PORT_CFG_SET Error\n");
        return IFX_ERROR;
    }

    return IFX_SUCCESS;
}


static int VLAN_IdCreate(IFX_ETHSW_HANDLE handle, 
						IFX_uint16_t nVId, 
						IFX_uint32_t nFId)
{
    int ret;
	union ifx_sw_param x;
    memset(&x.vlan_IdCreate, 0x00, sizeof(x.vlan_IdCreate));
    x.vlan_IdCreate.nVId = nVId;
    x.vlan_IdCreate.nFId = nFId;
    ret = ifx_ethsw_kioctl(handle, IFX_ETHSW_VLAN_ID_CREATE, (unsigned int)&x.vlan_IdCreate);
    if(ret != IFX_SUCCESS)
    {
        printk("IFX_ETHSW_VLAN_ID_CREATE Error\n");
        return IFX_ERROR;
    }

    return IFX_SUCCESS;
}


static int VLAN_IdDelete(IFX_ETHSW_HANDLE handle, 
						IFX_uint16_t nVId)
{
    int ret;
	union ifx_sw_param x;
    memset(&x.vlan_IdDelete, 0x00, sizeof(x.vlan_IdDelete));
    x.vlan_IdDelete.nVId = nVId;
    ret = ifx_ethsw_kioctl(handle, IFX_ETHSW_VLAN_ID_DELETE, (unsigned int)&x.vlan_IdDelete);
    if(ret != IFX_SUCCESS)
    {
        printk("IFX_ETHSW_VLAN_ID_DELETE Error\n");
        return IFX_ERROR;
    }

    return IFX_SUCCESS;
}


static int VLAN_PortMemberAdd(IFX_ETHSW_HANDLE handle, 
								IFX_uint16_t nVId, 
								IFX_uint8_t nPortId, 
								IFX_boolean_t bVLAN_TagEgress)
{
    int ret;
	union ifx_sw_param x;
    memset(&x.vlan_portMemberAdd, 0x00, sizeof(x.vlan_portMemberAdd));
    x.vlan_portMemberAdd.nVId = nVId;
    x.vlan_portMemberAdd.nPortId = nPortId;
    x.vlan_portMemberAdd.bVLAN_TagEgress = bVLAN_TagEgress;
    ret = ifx_ethsw_kioctl(handle, IFX_ETHSW_VLAN_PORT_MEMBER_ADD, (unsigned int)&x.vlan_portMemberAdd);
    if(ret != IFX_SUCCESS)
    {
        printk("IFX_ETHSW_VLAN_PORT_MEMBER_ADD Error\n");
        return IFX_ERROR;
    }

    return IFX_SUCCESS;
}

static int VLAN_PortMemberRemove(IFX_ETHSW_HANDLE handle, 
									IFX_uint16_t nVId, 
									IFX_uint8_t nPortId)
{
    int ret;
	union ifx_sw_param x;
    memset(&x.vlan_portMemberRemove, 0x00, sizeof(x.vlan_portMemberRemove));
    x.vlan_portMemberRemove.nVId = nVId;
    x.vlan_portMemberRemove.nPortId = nPortId;
    ret = ifx_ethsw_kioctl(handle, IFX_ETHSW_VLAN_PORT_MEMBER_REMOVE, (unsigned int)&x.vlan_portMemberRemove);
    if(ret != IFX_SUCCESS)
    {
        printk("IFX_ETHSW_VLAN_PORT_MEMBER_REMOVE Error\n");
        return IFX_ERROR;
    }

    return IFX_SUCCESS;
}


static int VLAN_PortCfgGet(IFX_ETHSW_HANDLE handle, union ifx_sw_param *pParam)
{
	int ret;
	if (pParam == NULL)
	{
		return IFX_ERROR;
	}
    
    ret = ifx_ethsw_kioctl(handle, IFX_ETHSW_VLAN_PORT_CFG_GET, (unsigned int)&(pParam->vlan_portcfg));
    if(ret != IFX_SUCCESS)
    {
        printk("IFX_ETHSW_VLAN_PORT_CFG_GET Error\n");
        return IFX_ERROR;
    }

    return IFX_SUCCESS;
}



static int VLAN_PortCfgSet(IFX_ETHSW_HANDLE handle, 
							IFX_uint8_t nPortId, 
							IFX_uint16_t nPortVId, 
							IFX_boolean_t bVLAN_UnknownDrop, 
							IFX_boolean_t bVLAN_ReAssign, 
							IFX_ETHSW_VLAN_MemberViolation_t eVLAN_MemberViolation, 
							IFX_ETHSW_VLAN_Admit_t eAdmitMode, 
							IFX_boolean_t bTVM)
{
    int ret;
	union ifx_sw_param x;
    memset(&x.vlan_portcfg, 0x00, sizeof(x.vlan_portcfg));
    x.vlan_portcfg.nPortId = nPortId;
    x.vlan_portcfg.nPortVId = nPortVId;
    x.vlan_portcfg.bVLAN_UnknownDrop = bVLAN_UnknownDrop;
    x.vlan_portcfg.bVLAN_ReAssign = bVLAN_ReAssign;
    x.vlan_portcfg.eVLAN_MemberViolation = eVLAN_MemberViolation;
    x.vlan_portcfg.eAdmitMode = eAdmitMode;
    x.vlan_portcfg.bTVM = bTVM;
    ret = ifx_ethsw_kioctl(handle, IFX_ETHSW_VLAN_PORT_CFG_SET, (unsigned int)&x.vlan_portcfg);
    if(ret != IFX_SUCCESS)
    {
        printk("IFX_ETHSW_VLAN_PORT_CFG_SET Error\n");
        return IFX_ERROR;
    }

    return IFX_SUCCESS;
}


static int RegisterGet(IFX_ETHSW_HANDLE handle, union ifx_sw_param *pParam)
{
    int ret;

    ret = ifx_ethsw_kioctl(handle, IFX_FLOW_REGISTER_GET, (unsigned int)&(pParam->register_access));
    if(ret != IFX_SUCCESS)
    {
        printk("IFX_FLOW_REGISTER_GET Error\n");
        return IFX_ERROR;
    }

    return IFX_SUCCESS;
}

static int RegisterSet(IFX_ETHSW_HANDLE handle, 
					IFX_uint32_t nRegAddr, 
					IFX_uint32_t nData)
{
    int ret;
	union ifx_sw_param x;
    memset(&x.register_access, 0x00, sizeof(x.register_access));
    x.register_access.nRegAddr = nRegAddr;
    x.register_access.nData = nData;
    ret = ifx_ethsw_kioctl(handle, IFX_FLOW_REGISTER_SET, (unsigned int)&x.register_access);

    if(ret != IFX_SUCCESS)
    {
        printk("IFX_FLOW_REGISTER_SET Error\n");
        return IFX_ERROR;
    }

    return IFX_SUCCESS;
}

static int vlan_enable(IFX_ETHSW_HANDLE handle)
{
	union ifx_sw_param param;
	int index = 1;
	
	memset(&param.register_access, 0x00, sizeof(param.register_access));
	param.register_access.nRegAddr = 0xccd;
	RegisterGet(handle, &param);

	if (param.register_access.nData == 0x0)
	{
		VLAN_PortCfgSet(handle, 4, l_lan_vid[0], 1, 0, 3, 0, 1);
	}

	memset(&(param.vlan_portcfg), 0x00, sizeof(param.vlan_portcfg));
	param.vlan_portcfg.nPortId = 6;
	VLAN_PortCfgGet(handle, &param);
	param.vlan_portcfg.bTVM = 0;
	VLAN_PortCfgSet(handle, 
					param.vlan_portcfg.nPortId, 
					param.vlan_portcfg.nPortVId,
					param.vlan_portcfg.bVLAN_UnknownDrop,
					param.vlan_portcfg.bVLAN_ReAssign,
					param.vlan_portcfg.eVLAN_MemberViolation, 
					param.vlan_portcfg.eAdmitMode,
					param.vlan_portcfg.bTVM);

	for (index = 1; index < sizeof(l_lan_port)/sizeof(int); index++)
	{
		VLAN_PortCfgSet(handle, l_lan_port[index], l_lan_vid[index], 1, 0, 3, 0, 1);
	}

	for (index = 0; index < sizeof(l_pseudo_port)/sizeof(int); index++)
	{
		VLAN_PortCfgSet(handle, l_pseudo_port[index], 0, 0, 0, 0, 0, 0);
	}

#if 0 /* Wu Zhiqin, 2012-07-05 */
	if (CfgSet(handle, 3, 1, 1536, 0, nPauseMAC_Src) != IFX_SUCCESS)
	{
		printk("CfgSet error.\n");
		return IFX_ERROR;
	}

	if (param.register_access.nData == 0x0)
	{
		if (VLAN_IdCreate(handle, 2, 2) != IFX_SUCCESS)
		{
			printk("VLAN_IdCreate 2, 2 error.\n");
			return IFX_ERROR;
		}
		if (VLAN_PortMemberAdd(handle, 2, 4, 0) != IFX_SUCCESS)
		{
			printk("VLAN_PortMemberAdd 2, 4, 0 error.\n");
			return IFX_ERROR;
		}
		if (VLAN_PortMemberAdd(handle, 2, 6, 1) != IFX_SUCCESS)
		{
			printk("VLAN_PortMemberAdd 2, 6, 1 error.\n");
			return IFX_ERROR;
		}
		if (VLAN_PortCfgSet(handle, 4, 2, 1, 0, 3, 0, 1) != IFX_SUCCESS)
		{
			printk("VLAN_PortCfgSet 4, 2, 1, 0, 3, 0, 1 error.\n");
			return IFX_ERROR;
		}
	}
	
	if (VLAN_IdCreate(handle, 3, 3) != IFX_SUCCESS)
	{
		printk("VLAN_IdCreate 3, 0 error.\n");
		return IFX_ERROR;
	}
	if (VLAN_PortMemberAdd(handle, 3, 2, 0) != IFX_SUCCESS)
	{
		printk("VLAN_PortMemberAdd 3, 0, 0 error.\n");
		return IFX_ERROR;
	}
	if (VLAN_PortMemberAdd(handle, 3, 6, 1) != IFX_SUCCESS)
	{
		printk("VLAN_PortMemberAdd 3, 6, 1 error.\n");
		return IFX_ERROR;
	}
	if (VLAN_PortCfgSet(handle, 2, 3, 1, 0, 3, 0, 1) != IFX_SUCCESS)
	{
		printk("VLAN_PortCfgSet 2, 3, 1, 0, 3, 0, 1 error.\n");
		return IFX_ERROR;
	}

	if (VLAN_IdCreate(handle, 4, 4) != IFX_SUCCESS)
	{
		printk("VLAN_IdCreate 4, 2 error.\n");
		return IFX_ERROR;
	}
	if (VLAN_PortMemberAdd(handle, 4, 0, 0) != IFX_SUCCESS)
	{
		printk("VLAN_PortMemberAdd 4, 2, 0 error.\n");
		return IFX_ERROR;
	}
	if (VLAN_PortMemberAdd(handle, 4, 6, 1) != IFX_SUCCESS)
	{
		printk("VLAN_PortMemberAdd 4, 6, 1 error.\n");
		return IFX_ERROR;
	}
	if (VLAN_PortCfgSet(handle, 0, 4, 1, 0, 3, 0, 1) != IFX_SUCCESS)
	{
		printk("VLAN_PortCfgSet 0, 0, 1, 0, 3, 0, 1 error.\n");
		return IFX_ERROR;
	}

	if (VLAN_IdCreate(handle, 5, 5) != IFX_SUCCESS)
	{
		printk("VLAN_IdCreate 5, 4 error.\n");
		return IFX_ERROR;
	}
	if (VLAN_PortMemberAdd(handle, 5, 5, 0) != IFX_SUCCESS)
	{
		printk("VLAN_PortMemberAdd 5, 5, 0 error.\n");
		return IFX_ERROR;
	}
	if (VLAN_PortMemberAdd(handle, 5, 6, 1) != IFX_SUCCESS)
	{
		printk("VLAN_PortMemberAdd 5, 6, 1 error.\n");
		return IFX_ERROR;
	}
	if (VLAN_PortCfgSet(handle, 5, 5, 1, 0, 3, 0, 1) != IFX_SUCCESS)
	{
		printk("VLAN_PortCfgSet 5, 5, 1, 0, 3, 0, 1 error.\n");
		return IFX_ERROR;
	}

	if (VLAN_PortCfgSet(handle, 7, 7, 1, 0, 3, 0, 1) != IFX_SUCCESS)
	{
		printk("VLAN_PortCfgSet 7, 7, 1, 0, 3, 0, 1 error.\n");
		return IFX_ERROR;
	}

	
	if (VLAN_PortCfgSet(handle, 8, 8, 1, 0, 3, 0, 1) != IFX_SUCCESS)
	{
		printk("VLAN_PortCfgSet 8, 8, 1, 0, 3, 0, 1 error.\n");
		return IFX_ERROR;
	}
	
	if (VLAN_PortCfgSet(handle, 9, 9, 1, 0, 3, 0, 1) != IFX_SUCCESS)
	{
		printk("VLAN_PortCfgSet 9, 9, 1, 0, 3, 0, 1 error.\n");
		return IFX_ERROR;
	}
#endif
	
	return IFX_SUCCESS;
}

static int vlan_disable(IFX_ETHSW_HANDLE handle)
{
	union ifx_sw_param param;
	int index = 1;

	memset(&param.register_access, 0x00, sizeof(param.register_access));
	param.register_access.nRegAddr = 0xccd;
	RegisterGet(handle, &param);

	if (param.register_access.nData == 0x0)
	{
		VLAN_PortCfgSet(handle, 4, 30, 0, 0, 3, 0, 1);
	}

	memset(&(param.vlan_portcfg), 0x00, sizeof(param.vlan_portcfg));
	param.vlan_portcfg.nPortId = 6;
	VLAN_PortCfgGet(handle, &param);
	param.vlan_portcfg.bTVM = 1;
	VLAN_PortCfgSet(handle, 
					param.vlan_portcfg.nPortId, 
					param.vlan_portcfg.nPortVId,
					param.vlan_portcfg.bVLAN_UnknownDrop,
					param.vlan_portcfg.bVLAN_ReAssign,
					param.vlan_portcfg.eVLAN_MemberViolation, 
					param.vlan_portcfg.eAdmitMode,
					param.vlan_portcfg.bTVM);

	for (index = 1; index < sizeof(l_lan_port)/sizeof(int); index++)
	{
		VLAN_PortCfgSet(handle, l_lan_port[index], 30, 0, 0, 3, 0, 1);
	}

	for (index = 0; index < sizeof(l_pseudo_port)/sizeof(int); index++)
	{
		VLAN_PortCfgSet(handle, l_pseudo_port[index], 30, 0, 0, 3, 0, 1);
	}

#if 0 /* Wu Zhiqin, 2012-07-05 */
	if (param.register_access.nData == 0x0)
	{
		VLAN_PortCfgSet(handle, 4, 0, 0, 0, 0, 0, 0);
		VLAN_PortMemberRemove(handle, 2, 4);
		VLAN_PortMemberRemove(handle, 2, 6);
		VLAN_IdDelete(handle, 2);
	}

	VLAN_PortCfgSet(handle, 5, 0, 0, 0, 0, 0, 0);
	VLAN_PortCfgSet(handle, 0, 0, 0, 0, 0, 0, 0);
	VLAN_PortCfgSet(handle, 2, 0, 0, 0, 0, 0, 0);

	VLAN_PortMemberRemove(handle, 3, 0);
	VLAN_PortMemberRemove(handle, 3, 6);

	VLAN_PortMemberRemove(handle, 4, 2);
	VLAN_PortMemberRemove(handle, 4, 6);

	VLAN_PortMemberRemove(handle, 5, 5);
	VLAN_PortMemberRemove(handle, 5, 6);
	
	VLAN_IdDelete(handle, 3);
	VLAN_IdDelete(handle, 4);
	VLAN_IdDelete(handle, 5);

	CfgSet(handle, 3, 0, 1536, 0, nPauseMAC_Src);
#endif

	return IFX_SUCCESS;
}

#if 0 /* Wu Zhiqin, 2012-07-06 */
static int ewan_enable(IFX_ETHSW_HANDLE handle)
{
#if 0 /* Wu Zhiqin, 2012-07-05 */
	union ifx_sw_param param;

	memset(&(param.vlan_portcfg), 0x00, sizeof(param.vlan_portcfg));
	param.vlan_portcfg.nPortId = 4;
	VLAN_PortCfgGet(handle, &param);

	if (VLAN_PortCfgSet(handle, 4, 0, 0, 0, 0, 0, 0) != IFX_SUCCESS)
	{
		printk("VLAN_PortCfgSet 4, 0, 0, 0, 0, 0, 0 error.\n");
		return IFX_ERROR;
	}
	
    if (param.vlan_portcfg.nPortVId == 30)
    {
		if (VLAN_PortMemberRemove(handle, 30, 4) != IFX_SUCCESS)
		{
			printk("VLAN_PortMemberRemove 2, 5 error.\n");
			return IFX_ERROR;
		}
    }
	
	if (RegisterSet(handle, 0xCCD, 0x10) != IFX_SUCCESS)
	{
		printk("RegisterSet 0xCCD, 0x10 error.\n");
		return IFX_ERROR;
	}

	return IFX_SUCCESS;
#endif
#if 0 /* Wu Zhiqin, 2012-07-05 */
	union ifx_sw_param param;

	memset(&param.cfg_Data, 0x00, sizeof(param.cfg_Data));
	CfgGet(handle, &param);
	
    if (param.cfg_Data.bVLAN_Aware == 1)
    {
		if (VLAN_PortCfgSet(handle, 4, 0, 0, 0, 0, 0, 0) != IFX_SUCCESS)
		{
			printk("VLAN_PortCfgSet 4, 0, 0, 0, 0, 0, 0 error.\n");
			return IFX_ERROR;
		}
		if (VLAN_PortMemberRemove(handle, 2, 4) != IFX_SUCCESS)
		{
			printk("VLAN_PortMemberRemove 2, 5 error.\n");
			return IFX_ERROR;
		}
		if (VLAN_PortMemberRemove(handle, 2, 6)  != IFX_SUCCESS)
		{
			printk("VLAN_PortMemberRemove 2, 6 error.\n");
			return IFX_ERROR;
		}
		if (VLAN_IdDelete(handle, 2) != IFX_SUCCESS)
		{
			printk("VLAN_IdDelete 2,  error.\n");
			return IFX_ERROR;
		}
    }
	
	if (RegisterSet(handle, 0xCCD, 0x10) != IFX_SUCCESS)
	{
		printk("RegisterSet 0xCCD, 0x10 error.\n");
		return IFX_ERROR;
	}
#endif

	return IFX_SUCCESS;
}
#endif

#if 0 /* Wu Zhiqin, 2012-07-05 */

static int ewan_disable(IFX_ETHSW_HANDLE handle)
{
	union ifx_sw_param param;

	memset(&param.cfg_Data, 0x00, sizeof(param.cfg_Data));
	CfgGet(handle, &param);
	
    if (param.cfg_Data.bVLAN_Aware == 1)
    {
		if (VLAN_IdCreate(handle, 2, 4) != IFX_SUCCESS)
		{
			printk("VLAN_IdCreate 2, 4 error.\n");
			return IFX_ERROR;
		}
		if (VLAN_PortMemberAdd(handle, 2, 4, 0) != IFX_SUCCESS)
		{
			printk("VLAN_PortMemberAdd(handle, 2, 4, 0) error.\n");
			return IFX_ERROR;
		}
		if (VLAN_PortMemberAdd(handle, 2, 6, 1)  != IFX_SUCCESS)
		{
			printk("VLAN_PortMemberAdd(handle, 2, 6, 1) error.\n");
			return IFX_ERROR;
		}
		if (VLAN_PortCfgSet(handle, 4, 2, 1, 0, 3, 0, 1) != IFX_SUCCESS)
		{
			printk("VLAN_PortCfgSet(handle, 4, 2, 1, 0, 3, 0, 1),  error.\n");
			return IFX_ERROR;
		}
    }
	
	RegisterSet(handle, 0xCCD, 0x0);

	return IFX_SUCCESS;
}
#endif


static int vlan_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
	IFX_ETHSW_HANDLE handle;
	union ifx_sw_param param;
	int index = 1;
	
    if ( off )
    {
        return off;
    }

	handle = ifx_ethsw_kopen("/dev/switch_api/0");

	memset(&(param.vlan_portcfg), 0x00, sizeof(param.vlan_portcfg));
	param.vlan_portcfg.nPortId = l_lan_port[index];
	VLAN_PortCfgGet(handle, &param);
	
    if (param.vlan_portcfg.nPortVId == l_lan_vid[index])
    {
        len += ethsw_sprintf(page + off + len, "vlan enable\n");
    }
    else
    {
		len += ethsw_sprintf(page + off + len, "vlan disable\n");
    }

#if 0 /* Wu Zhiqin, 2012-07-05 */
	memset(&param.register_access, 0x00, sizeof(param.register_access));
	param.register_access.nRegAddr = 0xccd;
	RegisterGet(handle, &param);
	if (param.register_access.nData != 0x0)
	{
		len += ethsw_sprintf(page + off + len, "ewan enable\n");
	}
	else
	{
		len += ethsw_sprintf(page + off + len, "ewan disable\n");
	}
#endif

    *eof = 1;

	ifx_ethsw_kclose(handle);
    return off + len;
}

static int vlan_proc_write(struct file *file, const char *buf, unsigned long count, void *data)
{
	IFX_ETHSW_HANDLE handle;
	int len;
	char str[64];
	char *p;

	int f_enable = 0;
	handle = ifx_ethsw_kopen("/dev/switch_api/0");
	
	len = min(count, (unsigned long)sizeof(str) - 1);
	len -= copy_from_user(str, buf, len);
	while ( len && str[len - 1] <= ' ' )
	{
		len--;
	}
	str[len] = 0;
	for ( p = str; *p && *p <= ' '; p++, len-- );
	if ( !*p )
	{
		ifx_ethsw_kclose(handle);
		return count;
	}

	if (*p == '1')	/* enable */
	{
		p += 1;  
		f_enable = 1;
	}
	else if (*p == '0')	/* disable */
	{
		p += 1; 
		f_enable = -1;
	}

	if ( f_enable )
	{
		if (*p == 0) /* no other op */
		{
			if ( f_enable > 0 )	/* enable */
			{
				vlan_enable(handle);
			}
			else	/* disable */
			{
				vlan_disable(handle);
			}
		}
#if 0 /* Wu Zhiqin, 2012-07-05 */
		else if (strincmp(p, " ewan", 5) == 0)
		{
			if ( f_enable > 0 )
			{
				ewan_enable(handle);
			}
			else
			{
				ewan_disable(handle);
			}
		}
#endif
	}
	
	ifx_ethsw_kclose(handle);
	return count;
}


static int forward_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int i = 0;
	int len = 0;
	IFX_ETHSW_HANDLE handle;
	union ifx_sw_param param;
	int enable = 1;
	
    if ( off )
    {
        return off;
    }

	handle = ifx_ethsw_kopen("/dev/switch_api/0");
	for (i = 0; i < 7; i++)
	{
		memset(&param.cfg_Data, 0x00, sizeof(param.portcfg));
		param.portcfg.nPortId = 6;
		PortCfgGet(handle, &param);
		if (!param.portcfg.eEnable)
		{
			enable = 0;
			break;
		}
	}
	len += ethsw_sprintf(page + off + len, "%d\n", enable);

	*eof = 1;

	ifx_ethsw_kclose(handle);
    return off + len;
}

static int forward_proc_write(struct file *file, const char *buf, unsigned long count, void *data)
{
	IFX_ETHSW_HANDLE handle;
	int len;
	char str[64];
	char *p;

	int f_enable = 0;
	handle = ifx_ethsw_kopen("/dev/switch_api/0");
	
	len = min(count, (unsigned long)sizeof(str) - 1);
	len -= copy_from_user(str, buf, len);

	while ( len && str[len - 1] <= ' ' )
	{
		len--;
	}
	str[len] = 0;
	for ( p = str; *p && *p <= ' '; p++, len-- );
	if ( !*p )
	{
		ifx_ethsw_kclose(handle);
		return count;
	}

	if (*p == '1')	/* enable */
	{
		printk("Enable forwarding\n");
		f_enable = 1;
	}
	else if (*p == '0')	/* disable */
	{
		f_enable = -1;
	}

	if (f_enable)
	{
		union ifx_sw_param param;
		int i = 0;
		IFX_ETHSW_portEnable_t enable = (f_enable == 1) ? 1: 0;

		for (i = 0; i < 7; i++)
		{
			memset(&param.portcfg, 0x00, sizeof(param.portcfg));
			param.portcfg.nPortId = i;
			PortCfgGet(handle, &param);
			param.portcfg.eEnable = enable;
			PortCfgSet(handle, 
					   param.portcfg.nPortId, 
					   param.portcfg.eEnable, 
					   param.portcfg.bUnicastUnknownDrop, 
					   param.portcfg.bMulticastUnknownDrop, 
					   param.portcfg.bReservedPacketDrop, 
					   param.portcfg.bBroadcastDrop, 
					   param.portcfg.bAging,
					   param.portcfg.bLearningMAC_PortLock, 
					   param.portcfg.nLearningLimit, 
					   param.portcfg.eFlowCtrl, 
					   param.portcfg.ePortMonitor);
		}
	}
	ifx_ethsw_kclose(handle);
	
	return count;
}

static int __init IFX_ETHSW_Switch_API_procModule_Init(void)
{
	struct proc_dir_entry *pEthVlan = NULL;
	struct proc_dir_entry *pEthForward = NULL;
	
    printk("Init IFX_ETHSW_Switch_API_procModule successfully.\n");

	if (g_tplink)
	{
		pEthVlan = create_proc_entry("eth_vlan",
	                            0,
	                            g_tplink);
	    if ( pEthVlan )
	    {
	        pEthVlan->read_proc  = vlan_proc_read;
	        pEthVlan->write_proc = vlan_proc_write;
	    }

		pEthForward = create_proc_entry("eth_forward",
	                            0,
	                            g_tplink);

		if ( pEthForward )
	    {
	        pEthForward->read_proc  = forward_proc_read;
	        pEthForward->write_proc = forward_proc_write;
	    }
	}

    return IFX_SUCCESS;
}


static void __exit IFX_ETHSW_Switch_API_procModule_Exit(void)
{
    printk("Exit from IFX_ETHSW_Switch_API_procModule successfully\n");
}

module_init(IFX_ETHSW_Switch_API_procModule_Init);
module_exit(IFX_ETHSW_Switch_API_procModule_Exit);

MODULE_AUTHOR("Lantiq");
MODULE_DESCRIPTION("IFX ethsw kernel api proc module");
MODULE_LICENSE("GPL");
