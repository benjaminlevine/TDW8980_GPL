/*! Copyright(c) 2008-2009 Shenzhen TP-LINK Technologies Co.Ltd.
 *
 *\file		wlanWarnIptables.h
 *\brief	header file of user space iptables dynamic library
 *
 *\author	Zou Boyin
 *\version	1.0.0
 *\date		27Dec09
 *
 *\history  \arg 1.0.0, 27Dec09, Zou Boyin, Create the file.
 */

#ifndef __WLAN_WARN_KERNEL_H__
#define __WLAN_WARN_KERNEL_H__

/***************************************************************************/
/*						 CONFIGURATIONS							 		   */
/***************************************************************************/
#define WLAN_WARN_IPT_DEBUG 0

/***************************************************************************/
/*						DEFINES						 					   */
/***************************************************************************/
#undef PDEBUG
#if WLAN_WARN_IPT_DEBUG
	#ifdef __KERNEL__
		#define PDEBUG(fmt, args...) printk(KERN_DEBUG  fmt, ##args)
	#else
		#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ##args)
	#endif
#else
	#define PDEBUG(fmt, args...) 
#endif


/***************************************************************************/
/*								TYPES									   */
/***************************************************************************/
/*!
 *\struct 	ipt_wlan_warn_info
 *\brief 	use to communicate between iptables target and iptables user space program
 */
struct ipt_wlan_warn_info {
	char empty;
};


#endif /* #ifndef __WLAN_WARN_KERNEL_H__ */
