/*! Copyright(c) 2008-2009 Shenzhen TP-LINK Technologies Co.Ltd.
 *
 *\file		wlanWarnIptables.c
 *\brief	header file of user space iptables dynamic library
 *
 *\author	Zou Boyin
 *\version	1.0.0
 *\date		27Dec09
 *
 *\history  \arg 1.0.0, 27Dec09, Zou Boyin, Create the file.
 */
 
/***************************************************************************/
/*						INCLUDE_FILES									   */
/***************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>

#include <linux/netfilter_ipv4/ip_tables.h>
#include "libipt_wlanwarn.h" 



/***************************************************************************/
/*						PUBLIC_FUNCTIONS					 			   */
/***************************************************************************/


/*!
 *\fn			static void init(struct ipt_entry_target *t, unsigned int *nfcache) 
 *
 *\brief		init function of this dynamic library
 *
 *\retval		0
 */
static void init(struct ipt_entry_target *t) 
{
}

/*!
 *\fn			static void help(void)  
 *
 *\brief		call by iptables when user input help command
 *
 *\retval		0
 */
static void help(void) 
{
	PDEBUG("this target catchs the ip header and tcp header of packet, and return to user space"
		"by user getsockopt() on a socket\n");
}

/*!
 *\fn			static int parse(int c, char **argv, int invert, unsigned int *flags,
								const struct ipt_entry *entry,
								struct ipt_entry_target **target)
 *
 *\brief		call by iptables when user input command target match the 
 *				name of this dynamic library registered
 *
 *\retval		1
 */
static int parse(int c, char **argv, int invert, unsigned int *flags,
		const void *entry,
		struct ipt_entry_target **target)
{
	return 1;
}

/*!
 *\fn			static void finalCheck(unsigned int flags)
 *
 *\brief		call by iptables to do final check 
 *
 *\retval		NONE
 */
static void finalCheck(unsigned int flags)
{
	PDEBUG("wlanWarn check\n");
}

/*!
 *\fn			static void finalCheck(unsigned int flags)
 *
 *\brief		call by iptables when user invoke iptables-save
 *
 *\retval		NONE
 */
static void save(const void *ip,
		const struct ipt_entry_target *target)
{
	PDEBUG("wlanWarn save\n");
}

/*!
 *\fn			static void finalCheck(unsigned int flags)
 *
 *\brief		call by iptables when user invoke iptables -L
 *
 *\retval		NONE
 */
static void print(const void *ip,
		const struct ipt_entry_target *target, int numeric)
{
	PDEBUG(" DROP and return warning page\n");
}

/*!
 *\struct 	option
 *\brief	the options that this target support, currently it's empty
 */
static struct option opts[] = {
	{ "", 0, 0, '1' },
	{ 0 }
};

/*!
 *\struct 	iptables_target
 *\brief	the sturcut to register to iptables
 */
static struct xtables_target wlanwarn = {
	.family 	= AF_INET,
	.name		= "wlanwarn",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_wlan_warn_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_wlan_warn_info)),
	.help		= help,
	.init		= init,
	.parse		= parse,
	.final_check	= finalCheck,
	.print		= print,
	.save		= save,
	.extra_opts	= opts 
};


/*!
 *\fn			void _init(void)
 *
 *\brief		automatically call by iptables when loading this dynamic library
 *
 *\retval		NONE
 */
void _init(void)
{
	xtables_register_target(&wlanwarn);
}
