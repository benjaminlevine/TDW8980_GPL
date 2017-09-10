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
 * flctrl (Flow Control) OS-dependent module implementation
 *
 */

#include "mtlkinc.h"

#include "core.h"
#include "sq.h"

int __MTLK_IFUNC flctrl_is_stopped(mtlk_handle_t usr_data)
{
  struct nic *nic = (struct nic*)usr_data;
  struct net_device *dev     = nic->ndev;

  if (nic->pack_sched_enabled)
    return nic->tx_prohibited;
  else
    return netif_queue_stopped(dev);
}


static int __MTLK_IFUNC flctrl_stop_data(mtlk_handle_t usr_data)
{
  struct nic *nic = (struct nic*)usr_data;
  struct net_device *dev     = nic->ndev;

  ILOG3(GID_FLCTRL, "flctrl_stop_data, stop_status = 0x%d",
        nic->pack_sched_enabled ? nic->tx_prohibited :
        netif_queue_stopped(dev));

  if (nic->pack_sched_enabled)
    nic->tx_prohibited = 1;
  else netif_stop_queue(dev);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC  flctrl_start_data(mtlk_handle_t usr_data)
{
  struct nic *nic = (struct nic*)usr_data;
  struct net_device *dev    = nic->ndev;

  ILOG3(GID_FLCTRL, "flctrl_start_data, stop_status = 0x%d",
        nic->pack_sched_enabled ? nic->tx_prohibited :
        netif_queue_stopped(dev));

  if (nic->pack_sched_enabled)
    nic->tx_prohibited = 0;
  else netif_start_queue(dev);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC  flctrl_wake_data(mtlk_handle_t usr_data)
{
  struct nic *nic = (struct nic*)usr_data;
  struct net_device *dev    = nic->ndev;

  ILOG3(GID_FLCTRL, "flctrl_wake_data, stop_status = 0x%d",
        nic->pack_sched_enabled ? nic->tx_prohibited :
        netif_queue_stopped(dev));

  if (nic->pack_sched_enabled) {
    nic->tx_prohibited = 0;
    mtlk_sq_schedule_flush(nic);
  } else netif_wake_queue(dev);

  return MTLK_ERR_OK;
}

void flctrl_cleanup(struct nic *nic)
{
  ILOG4(GID_FLCTRL, "flctrl_cleanup");

  mtlk_flctrl_cleanup(&nic->flctrl);
}

int flctrl_init(struct nic *nic)
{
  int res = -1;
  mtlk_flctrl_api_t api;

  ILOG4(GID_FLCTRL, "flctrl_init");

  memset(&api, 0, sizeof(api));

  api.usr_data              = HANDLE_T(nic);
  api.start                 = flctrl_start_data;
  api.wake                  = flctrl_wake_data;
  api.stop                  = flctrl_stop_data;


  res = mtlk_flctrl_init(&nic->flctrl, &api);
  if (res < 0) {
    ELOG("flctrl init (Err=%d)", res);
  }
  else {
    res = 0;
  }

  return res;
}

