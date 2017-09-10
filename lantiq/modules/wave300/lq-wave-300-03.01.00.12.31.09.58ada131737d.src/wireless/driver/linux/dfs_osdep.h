/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/

#ifndef __DOT11H_OSDEP_H__
#define __DOT11H_OSDEP_H__

#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/io.h>

/**********************************************************************
 * definitions and database
***********************************************************************/


/**********************************************************************
 * function declaration
***********************************************************************/

//start data
extern void __MTLK_IFUNC dot11h_start_data(mtlk_handle_t usr_data);

//wake data
extern void __MTLK_IFUNC dot11h_wake_data(mtlk_handle_t usr_data);

//stop data
extern void __MTLK_IFUNC dot11h_stop_data(mtlk_handle_t usr_data);

// get debug parameters
extern void __MTLK_IFUNC dot11h_get_debug_params(mtlk_handle_t usr_data,
                                                 mtlk_dot11h_debug_params_t *dot11h_debug_params);

//return device type
extern int __MTLK_IFUNC dot11h_device_is_ap(mtlk_handle_t usr_data);
extern uint16 __MTLK_IFUNC dot11h_get_sq_size(unsigned long data, uint16 access_category);

extern int dot11h_on_channel_switch_clb(mtlk_handle_t clb_usr_data,
                                             mtlk_handle_t context,
                                             int change_res);

int mtlk_dot11h_set_next_channel(struct net_device *dev, uint16 channel);
int mtlk_dot11h_debug_event (struct net_device *dev, uint8 event, uint16 channel);
const char *mtlk_dot11h_status (struct net_device *dev);

int dot11h_init(struct nic *nic);
void dot11h_cleanup(struct nic *nic);

#endif /* __DOT11H_OSDEP_H__ */
