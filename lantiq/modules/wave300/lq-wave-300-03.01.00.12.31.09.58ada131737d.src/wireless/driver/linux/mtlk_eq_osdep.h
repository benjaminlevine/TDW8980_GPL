/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id:$
 *
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *  
 * Linux dependant event queue part
 *
 */

#ifndef __MTLK_EQ_OSDEP_H__
#define __MTLK_EQ_OSDEP_H__

#include "mtlk_osal.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,4)
struct mtlk_eq_osdep
{
  mtlk_osal_event_t  new_event;
  struct task_struct *thread;
};
#else
struct mtlk_eq_osdep
{
  struct work_struct work;
  mtlk_osal_timer_t  timer;
};
#endif

int __MTLK_IFUNC _mtlk_eq_osdep_start(struct mtlk_eq_osdep* osdep);
void __MTLK_IFUNC _mtlk_eq_osdep_stop(struct mtlk_eq_osdep *osdep);
void __MTLK_IFUNC _mtlk_eq_osdep_cleanup (struct mtlk_eq_osdep *osdep);
int __MTLK_IFUNC _mtlk_eq_osdep_init (struct mtlk_eq_osdep *osdep);
void __MTLK_IFUNC _mtlk_eq_osdep_notify (struct mtlk_eq_osdep *osdep);

#endif

