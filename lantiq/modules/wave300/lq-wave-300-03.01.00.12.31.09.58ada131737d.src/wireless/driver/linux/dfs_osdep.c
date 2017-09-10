/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"

#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/io.h>

#include "core.h"
#include "mhi_umi.h"
#include "utils.h"
#include "compat.h"
#include "dfs_osdep.h"
#include "mtlkparams.h"
#include "mtlk_sq.h"

/**********************************************************************
 * definitions and database
***********************************************************************/

#define  DFS_EMUL_CHNL_AUTO -1

/**********************************************************************
 * function declaration
***********************************************************************/



/**********************************************************************
 * code
***********************************************************************/

//start data
#ifdef MT_FLCTRL_ENABLED
#if !MT_FLCTRL_ENABLED
void __MTLK_IFUNC dot11h_start_data(mtlk_handle_t usr_data)
{
  struct nic* nic = (struct nic*)usr_data;
  ILOG3(GID_DFS, "dot11h_start_data");

  mtlk_flctrl_start(nic->slow_ctx->dot11h.api.flctrl, nic->slow_ctx->dot11h.api.flctrl_id);
}

void __MTLK_IFUNC dot11h_wake_data(mtlk_handle_t usr_data)
{
  struct nic* nic = (struct nic*)usr_data;
  ILOG3(GID_DFS, "dot11h_wake_data");

  mtlk_flctrl_wake(nic->slow_ctx->dot11h.api.flctrl, nic->slow_ctx->dot11h.api.flctrl_id);
}

//stop data
void __MTLK_IFUNC dot11h_stop_data(mtlk_handle_t usr_data)
{
  struct nic* nic = (struct nic*)usr_data;
  
  ILOG3(GID_DFS, "dot11h_stop_data");
  mtlk_flctrl_stop(nic->slow_ctx->dot11h.api.flctrl, nic->slow_ctx->dot11h.api.flctrl_id);
}
#endif
#endif

//test implementaion of usr callback
int dot11h_on_channel_switch_clb(mtlk_handle_t clb_usr_data,
                                      mtlk_handle_t context,
                                      int change_res)
{
#if ((!defined MTCFG_SILENT) && (MAX_DLEVEL >= 2))
  struct nic* nic = (struct nic*)context;
#endif

  ILOG2(GID_DFS, "channel switch with res = %d nic->slow_ctx->dot11h.cfg.u16Channel = %d",
    change_res, nic->slow_ctx->dot11h.cfg.u16Channel);

  return 0;

}

void __MTLK_IFUNC dot11h_get_debug_params(mtlk_handle_t usr_data,
                                                 mtlk_dot11h_debug_params_t *dot11h_debug_params)
{
  struct nic* nic = (struct nic*)usr_data;

  *dot11h_debug_params = nic->slow_ctx->cfg.dot11h_debug_params;
}

int __MTLK_IFUNC dot11h_device_is_ap(unsigned long data)
{
  struct nic* nic = (struct nic*)data;

  ILOG3(GID_DFS, "nic->hw_cfg.ap = 0x%x \n",nic->slow_ctx->hw_cfg.ap);
  
  return nic->slow_ctx->hw_cfg.ap;
}

uint16 __MTLK_IFUNC dot11h_get_sq_size(unsigned long data, uint16 access_category)
{
  return mtlk_sq_get_qsize( ((struct nic*)data)->sq, access_category);
}

/* Setting the channel that will be switched to in case of radar detection*/
int
mtlk_dot11h_set_next_channel(struct net_device *dev, uint16 channel)
{
  struct nic *nic = netdev_priv(dev);

  nic->slow_ctx->cfg.dot11h_debug_params.debugNewChannel = channel;

  return 0;
}

int
mtlk_dot11h_debug_event (struct net_device *dev, uint8 event, uint16 channel)
{
  struct nic *nic = netdev_priv(dev);
  mtlk_aocs_evt_select_t switch_data;

  if (mtlk_core_get_net_state(nic) != NET_STATE_CONNECTED) {
    WLOG("%s: Can't switch channel - unappropriate state", dev->name);
    return -EAGAIN;
  }

  if (!mtlk_is_11h_radar_detection_enabled((mtlk_handle_t)nic)) {
    WLOG("Radar detection is disabled by user, iwpriv g11hRadarDetect = 0");
    return -EAGAIN;
  }

  if (MTLK_DOT11H_IN_PROCESS == nic->slow_ctx->dot11h.status) {
    WLOG("%s: Can't switch channel - already in progress", dev->name);
    return -EAGAIN;
  }

  ILOG0(GID_DFS, "Radar detected on current channel, switching...");

  nic->slow_ctx->dot11h.cfg.u16Channel         = channel? channel : nic->slow_ctx->channel;
  nic->slow_ctx->dot11h.cfg.u8IsHT             = nic->slow_ctx->is_ht_cur;
  nic->slow_ctx->dot11h.cfg.u8FrequencyBand    = nic->slow_ctx->frequency_band_cur;
  nic->slow_ctx->aocs.config.spectrum_mode     = /* thoughout */
  nic->slow_ctx->dot11h.cfg.u8SpectrumMode     = mtlk_aux_atol(mtlk_get_mib_value(PRM_SPECTRUM_MODE, nic));
  nic->slow_ctx->dot11h.cfg.u8Bonding          = nic->slow_ctx->bonding;
  nic->slow_ctx->dot11h.event                  = event;

  if (channel == (uint16)DFS_EMUL_CHNL_AUTO)
    switch_data.channel = 0;
  else
    switch_data.channel = channel;
  switch_data.reason = SWR_RADAR_DETECTED;
  switch_data.criteria = CHC_USERDEF;

  mtlk_dot11h_initiate_channel_switch(&nic->slow_ctx->dot11h, &switch_data, 
                                      FALSE, dot11h_on_channel_switch_clb);

  return 0;
}

const char *
mtlk_dot11h_status (struct net_device *dev)
{
  struct nic *nic = netdev_priv(dev);

  switch (nic->slow_ctx->dot11h.status) {
  case MTLK_DOT11H_IDLE:
    return "Not started";
  case MTLK_DOT11H_IN_PROCESS:
    return "In Process";
  case MTLK_DOT11H_IN_AVAIL_CHECK_TIME:
    return "In Availability check time";
  case MTLK_DOT11H_ERROR:
    return "Stop with error";
  case MTLK_DOT11H_FINISHED_OK:
    return "Finished OK";
  }

  return NULL; /* fake */
}

void dot11h_cleanup(struct nic *nic)
{
  ILOG3(GID_DFS, "dot11h_cleanup");

  mtlk_dot11h_cleanup(&nic->slow_ctx->dot11h);
}

int dot11h_init(struct nic *nic)
{
  int res = -1;
  mtlk_dot11h_wrap_api_t api;

  ILOG3(GID_DFS, "dot11h_init");

  memset(&api, 0, sizeof(api));
  mtlk_fill_default_dot11h_debug_params(&nic->slow_ctx->cfg.dot11h_debug_params);

  api.txmm                      = nic->slow_ctx->hw_cfg.txmm;
#if MT_FLCTRL_ENABLED
  api.flctrl                    = &nic->flctrl;
  api.flctrl_id                 = 0;
#else
  api.start_data                = dot11h_start_data;
  api.start_wake                = dot11h_wake_data;
  api.stop_data                 = dot11h_stop_data;
#endif
  api.aocs                      = &nic->slow_ctx->aocs;
  api.usr_data                  = HANDLE_T(nic);
  api.get_dot11h_debug_params   = dot11h_get_debug_params;
  api.device_is_ap              = dot11h_device_is_ap;
  api.get_sq_size               = dot11h_get_sq_size;

  res = mtlk_dot11h_init(&nic->slow_ctx->dot11h, NULL, &api, nic);
  if (res < 0)
  {
    ELOG("dot11h init (Err=%d)", res);
  }

  return res;
}

