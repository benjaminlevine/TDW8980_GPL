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
 * AOCS OS-dependent module implementation
 *
 */
#include "mtlkinc.h"
#include "core.h"
#include "mtlkparams.h"

void __MTLK_IFUNC
aocs_on_channel_change(mtlk_core_t *nic, int channel)
{
  if (nic->slow_ctx->channel != channel) {
    ILOG0(GID_AOCS, "Channel changed to %d", channel);
  }
  nic->slow_ctx->channel = channel;
}

void __MTLK_IFUNC
aocs_on_bonding_change(mtlk_core_t *nic, uint8 bonding)
{
  mtlk_core_set_bonding(nic, bonding);
}

void __MTLK_IFUNC
aocs_on_spectrum_change(mtlk_core_t *nic, int spectrum)
{
  if (nic->slow_ctx->spectrum_mode != spectrum) {
    ILOG0(GID_AOCS, "Spectrum changed to %s0MHz", spectrum == SPECTRUM_20MHZ ? "2" : "4");
  }
  mtlk_set_dec_mib_value(PRM_SPECTRUM_MODE, spectrum, nic);
  nic->slow_ctx->spectrum_mode = 
    nic->slow_ctx->pm_params.u8SpectrumMode = 
    nic->slow_ctx->dot11h.cfg.u8SpectrumMode = spectrum;

  /*
   * 40MHz spectrum should be selected only if HighThroughput is enabled.
   * Refer WLS-1602 for further information.
   */
  ASSERT((spectrum == 0) || nic->slow_ctx->is_ht_cfg);
}

void aocs_cleanup(struct nic *nic)
{
  mtlk_aocs_cleanup(&nic->slow_ctx->aocs);
}

int aocs_init(struct nic *nic)
{
  int res = -1;
  mtlk_aocs_wrap_api_t api;
  mtlk_aocs_init_t ini_data;

  api.core = nic;
  api.on_channel_change = aocs_on_channel_change;
  api.on_bonding_change = aocs_on_bonding_change;
  api.on_spectrum_change = aocs_on_spectrum_change;
  ini_data.api = &api;
  ini_data.scan_data = &nic->slow_ctx->scan;
  ini_data.cache = &nic->slow_ctx->cache;
  ini_data.dot11h = &nic->slow_ctx->dot11h;
  ini_data.txmm = nic->slow_ctx->hw_cfg.txmm;
  ini_data.is_ap = nic->slow_ctx->hw_cfg.ap;
  ini_data.disable_sm_channels = nic->slow_ctx->disable_sm_channels;

  res = mtlk_aocs_init(&nic->slow_ctx->aocs, &ini_data);
  if (res < 0)
    ELOG("aocs init (Err=%d)", res);
  else
    res = 0;
  return res;
}

#ifdef AOCS_DEBUG
int aocs_proc_cl(struct file *file, const char *buf, unsigned long count, void *data)
{
  struct nic *nic = (struct nic *)data;
  unsigned long cl;

  sscanf(buf, "%lu", &cl);
  if ((cl < 0) || (cl > 100))
    ELOG("Channel load must be in [0; 100] range");
  else
    mtlk_aocs_update_cl(&nic->slow_ctx->aocs, nic->slow_ctx->aocs.cur_channel, cl);
    
    mtlk_osal_lock_acquire(&nic->slow_ctx->aocs.lock);
    aocs_optimize_channel(&nic->slow_ctx->aocs, SWR_CHANNEL_LOAD_CHANGED);
    mtlk_osal_lock_release(&nic->slow_ctx->aocs.lock);

  return count;
}
#endif

