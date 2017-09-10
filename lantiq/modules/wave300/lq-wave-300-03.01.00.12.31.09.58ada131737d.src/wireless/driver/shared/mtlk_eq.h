/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
* $Id$
*
* Copyright (c) 2006-2008 Metalink Broadband (Israel)
*
* Queue of events
*
*/

#ifndef _MTLK_EQ_H_
#define _MTLK_EQ_H_

#include "mtlk_eq_osdep.h"
#include "mhi_ieee_address.h"
#include "mtlklist.h"

#define  MTLK_IDEFS_ON
#include "mtlkidefs.h"

typedef enum
{
  EVT_SCAN_CONFIRMED,
  EVT_SCAN_PAUSE_ELAPSED,
  EVT_MAC_WATCHDOG_TIMER,
  EVT_AGGR_REVIVE_TID,
  EVT_AGGR_REVIVE_ALL,
  EVT_DISCONNECT,
} EVENT_ID;

typedef struct mtlk_eq_entry
{
  EVENT_ID            id;
  void*               payload;
  mtlk_dlist_entry_t  link_entry;
} __MTLK_IDATA mtlk_eq_entry_t;

struct mtlk_eq
{
  BOOL                  initialized;
  BOOL                  started;
  mtlk_ldlist_t         events;
  struct nic            *nic;
  struct mtlk_eq_osdep  osdep;
} __MTLK_IDATA;

int __MTLK_IFUNC mtlk_eq_start (struct mtlk_eq *eq);
void __MTLK_IFUNC mtlk_eq_stop (struct mtlk_eq *eq);
int __MTLK_IFUNC mtlk_eq_init (struct mtlk_eq *eq, struct nic *nic);
void __MTLK_IFUNC mtlk_eq_cleanup (struct mtlk_eq *eq);
void __MTLK_IFUNC mtlk_eq_handle_events (struct mtlk_eq *eq);
// public interface for modules
int __MTLK_IFUNC mtlk_eq_notify (struct mtlk_eq *eq, EVENT_ID id, const void *payload, unsigned size);

#define  MTLK_IDEFS_OFF
#include "mtlkidefs.h"
#endif
