/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"

#include <linux/netdevice.h>
#include <net/iw_handler.h>

#include "scan.h"
#include "core.h"

int 
mtlk_scan_osdep_init(struct mtlk_scan* scan, struct nic* nic)
{
  struct mtlk_scan_config config;
  config.txmm = nic->slow_ctx->hw_cfg.txmm;
  config.core = nic;
  config.eq = &nic->slow_ctx->eq;
  config.aocs = &nic->slow_ctx->aocs;
  config.flctrl = &nic->flctrl;
  config.bss_cache = &nic->slow_ctx->cache;
  return mtlk_scan_init(scan, config);
}
  
void __MTLK_IFUNC
_mtlk_scan_osdep_send_completed_event(struct nic *nic)
{
  union iwreq_data wrqu;

  memset(&wrqu, 0, sizeof(wrqu));
  wireless_send_event(nic->ndev, SIOCGIWSCAN, &wrqu, NULL);
}

