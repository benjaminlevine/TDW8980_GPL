/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id$
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Core functionality
 *
 */
#include "mtlkinc.h"

#if (defined MTLK_DEBUG_IPERF_PAYLOAD_RX) || (defined MTLK_DEBUG_IPERF_PAYLOAD_TX)
#include <linux/in.h>
#include "iperf_debug.h"
#endif
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <net/iw_handler.h>

#include "rod.h"
#include "stadb.h"
#include "core.h"

#include "debug.h"
#include "drvver.h"
#include "mib_osdep.h"
#include "mhi_mac_event.h"
#include "scan.h"
#include "scan_osdep.h"
#include "frame.h"
#include "mtlkaux.h"
#include "mtlk_packets.h"
#include "mtlkparams.h"
#include "dfs_osdep.h"
#include "proc_aux.h"
#include "l2nat.h"
#include "mtlkmib.h"
#include "nlmsgs.h"
#include "dataex.h"
#include "ioctl.h"
#include "addba_osdep.h"
#include "flctrl_osdep.h"
#include "aocs_osdep.h"
#include "bufmgr.h"
#include "mtlkirb.h"
#include "progmodel.h"
#include "sq.h"
#include "mtlk_sq.h"

#define DEFAULT_TX_POWER        "17"
#define SCAN_CACHE_AGEING (3600) /* 1 hour */

#define DEFAULT_NUM_TX_ANTENNAS NUM_TX_ANTENNAS_GEN2
#define DEFAULT_NUM_RX_ANTENNAS (3)

#ifndef MTCFG_SILENT
#undef CONNECT_REASON
#define CONNECT_REASON(defname, defval, comment) #defname ": " comment,
static char *cr_str[] = {
  CONNECT_REASONS
};
#endif

#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
static __INLINE int
_mtlk_core_ppa_send_up_packet (struct nic *nic, struct sk_buff *skb)
{
  if (ppa_hook_directpath_send_fn && nic->ppa_clb.rx_fn) {
    /* set raw pointer for proper work if directpath is disabled */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
    skb_reset_mac_header(skb);
#else
    skb->mac.raw = skb->data;
#endif

    /* send packets to the ppa */
    if (ppa_hook_directpath_send_fn(nic->ppa_if_id, skb, skb->len, 0) == IFX_SUCCESS) {
      ++nic->pstats.ppa_rx_accepted;
      return NET_RX_SUCCESS;
    }
    ++nic->pstats.ppa_rx_rejected;
    return NET_RX_DROP;
  }

  skb->protocol = eth_type_trans(skb, skb->dev);
  return netif_rx(skb);
}
#endif /* CONFIG_IFX_PPA_API_DIRECTPATH */

static void mtlk_timer_handler (unsigned long);

#define STRING_VERSION_SIZE 1024
char mtlk_version_string[STRING_VERSION_SIZE] = "Driver version: " DRV_VERSION "\nMAC/PHY versions:\n";

int mtlk_classifier_register (void * classify_fn);
void mtlk_classifier_unregister (void);
EXPORT_SYMBOL(mtlk_classifier_register);
EXPORT_SYMBOL(mtlk_classifier_unregister);

static int send_disconnect_req(struct nic *nic, unsigned char *mac, BOOL blocked);
static void cleanup_on_disconnect(struct nic *nic, char *mac);
static int find_acl_entry(IEEE_ADDR *list, IEEE_ADDR *mac, signed int *free_entry);

static const mtlk_guid_t IRBE_RMMOD = MTLK_IRB_GUID_RMMOD;
static const mtlk_guid_t IRBE_HANG  = MTLK_IRB_GUID_HANG;

static int 
mtlk_app_notify_rmmod (u32 rmmod_data)
{
  return mtlk_irb_notify_app(&IRBE_RMMOD, &rmmod_data, sizeof(rmmod_data));
}

static int 
mtlk_app_notify_hang (u32 sw_watchdog_data)
{
  return mtlk_irb_notify_app(&IRBE_HANG, &sw_watchdog_data, sizeof(sw_watchdog_data));
}

mtlk_eeprom_data_t* __MTLK_IFUNC
mtlk_core_get_eeprom(mtlk_core_t* core)
{
  return &core->slow_ctx->ee_data;
}

static __INLINE void
msdu_tx_inc_nof_used(mtlk_core_t *core, uint8 ac)
{
  core->tx_data_nof_used_bds[ac]++;
  ILOG4(GID_CORE, "-> core->tx_data_nof_used_bds[%d] = %d", ac, core->tx_data_nof_used_bds[ac]);
}

static __INLINE void
msdu_tx_dec_nof_used(mtlk_core_t *core, uint8 ac)
{
  core->tx_data_nof_used_bds[ac]--;
  ILOG4(GID_CORE, "<- core->tx_data_nof_used_bds[%d] = %d", ac, core->tx_data_nof_used_bds[ac]);
}

static __INLINE uint16
msdu_tx_get_nof_used(mtlk_core_t *core, uint8 ac)
{
  ILOG4(GID_CORE, "core->tx_data_nof_used_bds[%d] = %d", ac, core->tx_data_nof_used_bds[ac]);
  return core->tx_data_nof_used_bds[ac];
}

static __INLINE const uint16*
msdu_tx_get_nof_used_array(mtlk_core_t *core)
{
  return core->tx_data_nof_used_bds;
}

char *
mtlk_net_state_to_string(uint32 state)
{
  switch (state) {
  case NET_STATE_HALTED:
    return "NET_STATE_HALTED";
  case NET_STATE_IDLE:
    return "NET_STATE_IDLE";
  case NET_STATE_READY:
    return "NET_STATE_READY";
  case NET_STATE_ACTIVATING:
    return "NET_STATE_ACTIVATING";
  case NET_STATE_CONNECTED:
    return "NET_STATE_CONNECTED";
  case NET_STATE_DISCONNECTING:
    return "NET_STATE_DISCONNECTING";
  default:
    break;
  }
  ILOG1(GID_CORE, "Unknown state 0x%04X", state);
  return "NET_STATE_UNKNOWN";
}

mtlk_hw_state_e
mtlk_core_get_hw_state (mtlk_core_t *nic)
{
  mtlk_hw_state_e hw_state = MTLK_HW_STATE_LAST;

  mtlk_hw_get_prop(nic->hw, MTLK_HW_PROP_STATE, &hw_state, sizeof(hw_state));
  return hw_state;
}

int
mtlk_set_hw_state (mtlk_core_t *nic, mtlk_hw_state_e st)
{
  mtlk_hw_state_e ost;
  mtlk_hw_get_prop(nic->hw, MTLK_HW_PROP_STATE, &ost, sizeof(ost));
  ILOG1(GID_CORE, "%i -> %i", ost, st);
  return mtlk_hw_set_prop(nic->hw, MTLK_HW_PROP_STATE, &st, sizeof(st));
}

int __MTLK_IFUNC
mtlk_core_set_net_state(mtlk_core_t *core, uint32 new_state)
{
  uint32 allow_mask;
  mtlk_hw_state_e hw_state;
  int result = MTLK_ERR_OK;

  spin_lock_bh(&core->net_state_lock);
  if (new_state == NET_STATE_HALTED) {
    if (core->net_state != NET_STATE_HALTED) {
      mtlk_hw_state_e hw_state = mtlk_core_get_hw_state(core);
      ELOG("Going to net state HALTED (net_state=%d)", core->net_state);
      core->net_state = NET_STATE_HALTED;
      if (hw_state != MTLK_HW_STATE_EXCEPTION &&
          hw_state != MTLK_HW_STATE_APPFATAL) {
        ELOG("Asserting FW: hw_state=%d", hw_state);
        mtlk_hw_set_prop(core->hw, MTLK_HW_RESET, NULL, 0);
      }
    }
    goto FINISH;
  }
  /* allow transition from NET_STATE_HALTED to NET_STATE_IDLE
     while in hw state MTLK_HW_STATE_READY */
  hw_state = mtlk_core_get_hw_state(core);
  if ((hw_state != MTLK_HW_STATE_READY) && (hw_state != MTLK_HW_STATE_UNLOADING) &&
      (new_state != NET_STATE_IDLE)) {
    result = MTLK_ERR_HW;
    goto FINISH;
  }
  allow_mask = 0;
  switch (new_state) {
  case NET_STATE_IDLE:
    allow_mask = NET_STATE_HALTED; /* on core_start */
    break;
  case NET_STATE_READY:
    allow_mask = NET_STATE_IDLE | NET_STATE_ACTIVATING |
      NET_STATE_DISCONNECTING;
    break;
  case NET_STATE_ACTIVATING:
    allow_mask = NET_STATE_READY; 
    break;
  case NET_STATE_DISCONNECTING:
    allow_mask = NET_STATE_CONNECTED;
    break;
  case NET_STATE_CONNECTED:
    allow_mask = NET_STATE_ACTIVATING;
    break;
  default:
    break;
  }
  /* check mask */ 
  if (core->net_state & allow_mask) {
    ILOG1(GID_CORE, "Going from %s to %s",
      mtlk_net_state_to_string(core->net_state),
      mtlk_net_state_to_string(new_state));
    core->net_state = new_state;
  } else {
    ILOG1(GID_CORE, "Failed to change state from %s to %s",
      mtlk_net_state_to_string(core->net_state),
      mtlk_net_state_to_string(new_state));
    result = MTLK_ERR_WRONG_CONTEXT; 
  }
FINISH:
  spin_unlock_bh(&core->net_state_lock);
  return result;
}

int __MTLK_IFUNC
mtlk_core_get_net_state(mtlk_core_t *core)
{
  uint32 net_state;
  mtlk_hw_state_e hw_state;

  hw_state = mtlk_core_get_hw_state(core);
  if (hw_state != MTLK_HW_STATE_READY && hw_state != MTLK_HW_STATE_UNLOADING) {
    net_state = NET_STATE_HALTED;
    goto FINISH;
  }
  net_state = core->net_state;
FINISH:
  return net_state;
}

/* TODO: Empiric values, must be replaced somehow */
#define MAC_WATCHDOG_DEFAULT_TIMEOUT_MS 10000
#define MAC_WATCHDOG_DEFAULT_PERIOD_MS 30000

static uint32
mac_watchdog_timer_handler (mtlk_osal_timer_t *timer, mtlk_handle_t data)
{
  struct nic *nic = (struct nic *)data;
  mtlk_eq_notify(&nic->slow_ctx->eq, EVT_MAC_WATCHDOG_TIMER, NULL, 0);
  return 0;
}

static void
check_mac_watchdog (struct nic *nic)
{
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry;
  UMI_MAC_WATCHDOG *mac_watchdog;
  int res = MTLK_ERR_OK;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, nic->slow_ctx->hw_cfg.txdm, NULL);
  if (!man_entry) {
    res = MTLK_ERR_NO_RESOURCES;
    goto END;
  }

  man_entry->id = UM_DBG_MAC_WATCHDOG_REQ;
  man_entry->payload_size = sizeof(UMI_MAC_WATCHDOG);

  mac_watchdog = (UMI_MAC_WATCHDOG *)man_entry->payload;
  mac_watchdog->u16Timeout = HOST_TO_MAC16(nic->slow_ctx->mac_watchdog_timeout_ms);

  res = mtlk_txmm_msg_send_blocked(&man_msg, nic->slow_ctx->mac_watchdog_timeout_ms);
  if (res == MTLK_ERR_OK) {
    switch (mac_watchdog->u8Status) {
    case UMI_OK:
      break;
    case UMI_MC_BUSY:
      break;
    case UMI_TIMEOUT:
      res = MTLK_ERR_UMI;
      break;
    default:
      res = MTLK_ERR_UNKNOWN;
      break;
    }
  }
  mtlk_txmm_msg_cleanup(&man_msg);

END:
  if (res != MTLK_ERR_OK) {
    ELOG("MAC watchdog error %d, resetting", res);
    mtlk_hw_set_prop(nic->hw, MTLK_HW_RESET, NULL, 0);
  } else {
    if (mtlk_osal_timer_set(&nic->slow_ctx->mac_watchdog_timer,
                            nic->slow_ctx->mac_watchdog_period_ms) != MTLK_ERR_OK) {
      ELOG("Cannot schedule MAC watchdog timer, resetting");
      mtlk_hw_set_prop(nic->hw, MTLK_HW_RESET, NULL, 0);
    }
  }
}

#define STAT_POLLING_PERIOD HZ
#define STAT_POLLING_TIMEOUT (5*HZ)

static void __MTLK_IFUNC
try_to_start_stat_poll_timer (struct nic *nic)
{
  read_lock_bh(&nic->slow_ctx->stat_lock);
  if (nic->slow_ctx->is_stat_poll)
    mod_timer(&nic->slow_ctx->stat_poll_timer, jiffies + STAT_POLLING_PERIOD);
  read_unlock_bh(&nic->slow_ctx->stat_lock);
}

static void __MTLK_IFUNC
start_stat_poll_timer (struct nic *nic)
{
  nic->slow_ctx->is_stat_poll = 1; /* there are no stat_poll_timer - so spin_lock is useless */
  try_to_start_stat_poll_timer(nic);
}

/*
 * stop_stat_poll_timer() guaranees that stat_poll_timer isn't running
 * on another CPU nor waiting for execution,
 * but it can't guarantee that txm callbacks wouldn't be called.
 * In order to prevent re-scheduling of stat_poll_timer
 * after stop_stat_poll_timer() nic->slow_ctx->is_stat_poll is used.
 */
static void __MTLK_IFUNC
stop_stat_poll_timer (struct nic *nic)
{
  write_lock_bh(&nic->slow_ctx->stat_lock);
  nic->slow_ctx->is_stat_poll = 0;
  write_unlock_bh(&nic->slow_ctx->stat_lock);
  del_timer_sync(&nic->slow_ctx->stat_poll_timer);
}

static mtlk_txmm_clb_action_e __MTLK_IFUNC
conn_stat_poll_clb (mtlk_handle_t data, mtlk_txmm_data_t *mm, mtlk_txmm_clb_reason_e reason)
{
  struct net_device *dev = (struct net_device*)data;
  struct nic *nic = netdev_priv(dev);
  mtlk_txmm_clb_action_e res = MTLK_TXMM_CLBA_FREE;
  UMI_GET_CONNECTION_STATUS *stat = (UMI_GET_CONNECTION_STATUS*)mm->payload;
  uint8_t u8DeviceIndex;
  int i;

  if (reason != MTLK_TXMM_CLBR_CONFIRMED) {
    goto end;
  }

  write_lock(&nic->slow_ctx->stat_lock);
  nic->slow_ctx->noise = stat->u8GlobalNoise;
  nic->slow_ctx->channel_load = stat->u8ChannelLoad;
  write_unlock(&nic->slow_ctx->stat_lock);

  for (i = 0; i < stat->u8NumOfDeviceStatus; i++) {
    int id = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, (unsigned char*)&stat->sDeviceStatus[i].sMacAdd);
    if (id >= 0) {
      sta_entry *sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, id);
      mtlk_osal_lock_acquire(&sta->lock);
      sta->rssi[0] = stat->sDeviceStatus[i].au8RSSI[0];
      sta->rssi[1] = stat->sDeviceStatus[i].au8RSSI[1];
      sta->rssi[2] = stat->sDeviceStatus[i].au8RSSI[2];
      sta->net_mode = stat->sDeviceStatus[i].u8NetworkMode;
      sta->tx_rate = le16_to_cpu(stat->sDeviceStatus[i].u16TxRate);
      mtlk_osal_lock_release(&sta->lock);
    }
  }

  u8DeviceIndex = stat->u8DeviceIndex;

  if (u8DeviceIndex == 0) {
    goto end;
  }

  mm->id = UM_MAN_GET_CONNECTION_STATUS_REQ;
  mm->payload_size = sizeof(UMI_GET_CONNECTION_STATUS);

  memset(stat, 0, sizeof(UMI_GET_CONNECTION_STATUS));
  stat->u8DeviceIndex = u8DeviceIndex;

  res = MTLK_TXMM_CLBA_SEND;

end:
  if (res != MTLK_TXMM_CLBA_SEND) {
    try_to_start_stat_poll_timer(nic);
  }

  return res;
}

static mtlk_txmm_clb_action_e __MTLK_IFUNC
mac_stat_poll_clb (mtlk_handle_t data, mtlk_txmm_data_t *mm, mtlk_txmm_clb_reason_e reason)
{
  struct net_device *dev = (struct net_device*)data;
  struct nic *nic = netdev_priv(dev);
  UMI_GET_STATISTICS *response = (UMI_GET_STATISTICS*)mm->payload;
  UMI_STATISTICS *stat = &response->sStats;
  UMI_GET_CONNECTION_STATUS *request;
  mtlk_txmm_data_t *conn_mm;
  int sres;

  if ((reason == MTLK_TXMM_CLBR_CONFIRMED) && (le16_to_cpu(response->u16Status) == UMI_OK)) {
    int i;
    write_lock(&nic->slow_ctx->stat_lock);
    for (i = 0; i < STAT_TOTAL_NUMBER; i++)
      nic->slow_ctx->mac_stat[i] = le32_to_cpu(stat->au32Statistics[i]);
    write_unlock(&nic->slow_ctx->stat_lock);
  }

  conn_mm = mtlk_txmm_msg_get_empty_data(&nic->txmm_async_msgs[MTLK_NIC_TXMMA_GET_CONN_STATS],
                                         nic->slow_ctx->hw_cfg.txmm);
  if (conn_mm == NULL) {
    try_to_start_stat_poll_timer(nic);
    goto end;
  }

  conn_mm->id = UM_MAN_GET_CONNECTION_STATUS_REQ;
  conn_mm->payload_size = sizeof(UMI_GET_CONNECTION_STATUS);

  request = (UMI_GET_CONNECTION_STATUS *)conn_mm->payload;
  memset(request, 0, sizeof(UMI_GET_CONNECTION_STATUS));

  sres = mtlk_txmm_msg_send(&nic->txmm_async_msgs[MTLK_NIC_TXMMA_GET_CONN_STATS],
                            conn_stat_poll_clb, 
                            HANDLE_T(dev), 
                            jiffies_to_msecs(STAT_POLLING_TIMEOUT));

  if (sres != MTLK_ERR_OK) {
    ELOG("Can't request CONN stats due to TXMM err#%d", sres);
  }

end:
  return MTLK_TXMM_CLBA_FREE;
}

static void
stat_poll_timer_handler (unsigned long data)
{
  struct net_device *dev = (struct net_device*)data;
  struct nic *nic = netdev_priv(dev);
  mtlk_txmm_data_t *mm;
  UMI_GET_STATISTICS *stat;
  int sres;

  mm = mtlk_txmm_msg_get_empty_data(&nic->txdm_async_msgs[MTLK_NIC_TXDMA_GET_MAC_STATS],
                                    nic->slow_ctx->hw_cfg.txdm);
  if (mm == NULL) {
    try_to_start_stat_poll_timer(nic);
    return;
  }

  mm->id = UM_DBG_GET_STATISTICS_REQ;
  mm->payload_size = sizeof(UMI_GET_STATISTICS);
  stat = (UMI_GET_STATISTICS*)mm->payload;
  memset(stat, 0, sizeof(UMI_GET_STATISTICS));
  stat->u16Ident = cpu_to_le16(STAT_TOTAL_NUMBER);

  sres = mtlk_txmm_msg_send(&nic->txdm_async_msgs[MTLK_NIC_TXDMA_GET_MAC_STATS],
                            mac_stat_poll_clb, 
                            HANDLE_T(dev), 
                            jiffies_to_msecs(STAT_POLLING_TIMEOUT));
  if (sres != MTLK_ERR_OK) {
    ELOG("Can't request MAC stats due to TXMM err#%d", sres);
  }
}

static int
get_cipher (struct sk_buff *skb)
{
  struct nic *nic = netdev_priv(skb->dev);
  struct skb_private *skb_priv = (struct skb_private*)skb->cb;

  if (skb_priv->flags & SKB_DIRECTED) {
    MTLK_ASSERT(skb_priv->src_sta != NULL);
    return skb_priv->src_sta->cipher;
  } else {
    return nic->group_cipher;
  }
}

uint16
mtlk_core_get_rate_for_addba (struct nic *nic)
{
  /* Return fake rate,
   * because anyway BA shouldn't depend on it.
   * BA should depend only on connection type (HT or non-HT)
   */
  return MTLK_ADDBA_RATE_ADAPTIVE;
}

/* Get Received Security Counter buffer */
static int
get_rsc_buf(struct sk_buff *skb, int off)
{
  struct skb_private *skb_priv = (struct skb_private *)(skb->cb);
  u8 cipher = get_cipher(skb);
 
  if (cipher == IW_ENCODE_ALG_TKIP || cipher == IW_ENCODE_ALG_CCMP) {
    memcpy(skb_priv->rsc_buf, skb->data +off, sizeof(skb_priv->rsc_buf));
    return sizeof(skb_priv->rsc_buf);
  } else {
    return 0;
  }
}

void
mac_reset_stats (mtlk_core_t *nic)
{
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, nic->slow_ctx->hw_cfg.txdm, NULL);
  if (man_entry) {
    UMI_RESET_STATISTICS *pstats = (UMI_RESET_STATISTICS *)man_entry->payload;

    man_entry->id           = UM_DBG_RESET_STATISTICS_REQ;
    man_entry->payload_size = sizeof(*pstats);
    pstats->u16Status       = 0;
    if (mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
      ELOG("MAC Reset Stat sending timeout");
    }

    mtlk_txmm_msg_cleanup(&man_msg);
  }
  else {
    ELOG("Can't reset statistics due to the lack of MAN_MSG");
  }
}

UMI_GET_STATISTICS *
mac_get_stats (mtlk_core_t *nic, mtlk_txmm_msg_t *man_msg)
{
  UMI_GET_STATISTICS *pstats = NULL;
  mtlk_txmm_data_t   *man_entry;

  MTLK_ASSERT(nic != NULL);
  MTLK_ASSERT(man_msg != NULL);

  man_entry = mtlk_txmm_msg_get_empty_data(man_msg, nic->slow_ctx->hw_cfg.txdm);
  if (!man_entry) {
    ELOG("Can't get statistics due to the lack of MAN_MSG");
    goto end;
  }

  pstats = (UMI_GET_STATISTICS *)man_entry->payload;

  man_entry->id           = UM_DBG_GET_STATISTICS_REQ;
  man_entry->payload_size = sizeof(*pstats);
  pstats->u16Status       = 0;
  pstats->u16Ident        = HOST_TO_MAC16(STAT_TOTAL_NUMBER);

  if (mtlk_txmm_msg_send_blocked(man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
    ELOG("MAC Get Stat sending timeout");
    pstats = NULL;
  }

end:
  return pstats;
}

static void
send_assoc_event(struct nic *nic, char *mac)
{
  union iwreq_data r;
  r.data.length = 0;
  r.data.flags  = 0;
  r.ap_addr.sa_family = ARPHRD_ETHER;
  if (mac == NULL) {
    memset(r.ap_addr.sa_data, 0, ETH_ALEN);
  } else {
    memcpy(r.ap_addr.sa_data, mac, ETH_ALEN);
    r.ap_addr.sa_family = ARPHRD_ETHER;
  }
  ILOG2(GID_CORE, "%Y", r.ap_addr.sa_data);
  wireless_send_event(nic->ndev, SIOCGIWAP, &r, NULL);
}

static void
send_conn_sta_event(struct net_device *dev, int event, u8 *addr)
{
  union iwreq_data wrqu;
  memset(&wrqu, 0, sizeof(wrqu));
  memcpy(wrqu.addr.sa_data, addr, ETH_ALEN);
  wrqu.addr.sa_family = ARPHRD_ETHER;
  wireless_send_event(dev, event, &wrqu, NULL);
}

static void
clean_all_sta_on_disconnect (struct nic *nic)
{
  int i;
  sta_entry *sta;
  IEEE_ADDR staId;

  for (i = 0; i < STA_MAX_STATIONS; i++) {
    sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, i);

    if (sta->state != PEER_UNUSED) {
      mtlk_mc_drop_sta(nic, sta->mac);
      memcpy(staId.au8Addr, sta->mac, IEEE_ADDR_LEN);

      if (sta->dot11n_mode) {
        mtlk_addba_on_disconnect_ex(&nic->slow_ctx->addba,
                                    &staId,
                                    &sta->addba_peer,
                                    HANDLE_T(i));
      }
      ILOG1(GID_CORE, "Station %Y disconnected", sta->mac);
      send_conn_sta_event(nic->ndev, IWEVEXPIRED, sta->mac);
      mtlk_stadb_remove_sta(&nic->slow_ctx->stadb, sta);
    }
  }
  mtlk_mc_drop_querier(nic);
  nic->slow_ctx->connected = 0;
}

static mtlk_txmm_clb_action_e __MTLK_IFUNC 
mtlk_disconnect_cfm_clb(mtlk_handle_t clb_usr_data, mtlk_txmm_data_t* data, mtlk_txmm_clb_reason_e reason)
{
  struct nic *nic = (struct nic *)clb_usr_data;
  UMI_DISCONNECT *psUmiDisconnect = (UMI_DISCONNECT *)data->payload;
  char *mac = psUmiDisconnect->sStationID.au8Addr;

  ILOG1(GID_CORE, "%Y disconnect confirmed (reason=%d status=%u)",
        mac, reason, MAC_TO_HOST16(psUmiDisconnect->u16Status));

  if((MTLK_TXMM_CLBR_CONFIRMED != reason) && nic->slow_ctx->is_halt_fw_on_disc_timeout)
  {
    mtlk_core_set_net_state(nic, NET_STATE_HALTED);
    return MTLK_TXMM_CLBA_FREE;
  }

  cleanup_on_disconnect(nic, mac);

  if (nic->ap) {
    ILOG1(GID_CORE, "Station %Y disconnected", mac);
    send_conn_sta_event(nic->ndev, IWEVEXPIRED, mac);
  } else {
    ILOG1(GID_CORE, "Disconnected from AP %Y", mac);
    send_assoc_event(nic, NULL);
  }
  /* update disconnections statistics */
  nic->pstats.num_disconnects++;
  return MTLK_TXMM_CLBA_FREE;
}

static void
cleanup_on_disconnect (struct nic *nic, char *mac)
{
  sta_entry *sta;
  BOOL       disc_all = mtlk_osal_is_zero_address(mac);
  int        id       = disc_all?-1:mtlk_stadb_find_sta(&nic->slow_ctx->stadb, mac);
  ILOG3(GID_CORE, "connected %i", nic->slow_ctx->connected);

  if (!nic->ap) {
    // Drop BSSID persistency in BSS cache.
    // We've been disconnected from BSS, thus we don't know 
    // whether it's alive or not.
    mtlk_cache_set_persistent(&nic->slow_ctx->cache, 
                              nic->slow_ctx->bssid, 
                              FALSE);

    /* restore network mode after disconnect */
    nic->slow_ctx->net_mode_cur = nic->slow_ctx->net_mode_cfg;
  }

  if (disc_all) {
    ILOG1("Zero MAC: disconnecting all STAs");
    clean_all_sta_on_disconnect(nic);
  }
  else if (id >= 0) {
    sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, id);
    mtlk_stadb_remove_sta(&nic->slow_ctx->stadb, sta);
    mtlk_mc_drop_sta(nic, mac);
    if (nic->slow_ctx->connected) {
      nic->slow_ctx->connected--;
    }
  }
  else {
    WLOG("STA not found during cleanup: %Y", mac);
    return;
  }

  if (nic->slow_ctx->connected == 0 &&
      mtlk_core_get_net_state(nic) == NET_STATE_DISCONNECTING) {
    memset(nic->slow_ctx->bssid, 0, sizeof(nic->slow_ctx->bssid));
    mtlk_core_set_net_state(nic, NET_STATE_READY);
  }

  if (nic->slow_ctx->connected == 0) {
#if MT_FLCTRL_ENABLED 
    mtlk_flctrl_stop(&nic->flctrl, nic->flctrl_id);
#else

    if (nic->pack_sched_enabled)
      nic->tx_prohibited = 1;
    else netif_stop_queue(nic->ndev);
#endif
  }
}

static int
send_disconnect_req(struct nic *nic, unsigned char *mac, BOOL blocked)
{
  mtlk_txmm_msg_t   blocked_man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  mtlk_txmm_msg_t  *man_msg   = NULL;
  UMI_DISCONNECT   *psUmiDisconnect;
  sta_entry *sta = NULL;
  IEEE_ADDR addr;
  int net_state = mtlk_core_get_net_state(nic);
  int res = MTLK_ERR_OK;
  peer_state sta_state = PEER_UNUSED;

  if (((nic->ap && mac == NULL) || !nic->ap) && (net_state != NET_STATE_HALTED)) {
    /* check if we can disconnect in current net state */
    res = mtlk_core_set_net_state(nic, NET_STATE_DISCONNECTING);
    if (res != MTLK_ERR_OK) {
      goto FINISH;
    }
  }

  if (mac == NULL && !nic->ap)
    mac = nic->slow_ctx->bssid;

  if (mac == NULL)
    memset(addr.au8Addr, 0, ETH_ALEN);
  else
    memcpy(addr.au8Addr, mac, ETH_ALEN);

#ifdef PHASE_3
  if (!nic->ap) {
    /* stop cache updation on STA. */
    mtlk_stop_cache_update(nic);
  }
#endif
  if (nic->ap && mac == NULL) {
    clean_all_sta_on_disconnect(nic);
  } else {
    int id = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, addr.au8Addr);
    if (id < 0) {
      WLOG("STA not found during cleanup: %Y", addr.au8Addr);
      /* We have always to disconnect the STA to allow FW to remove it
       * from its STA DB, so we create a fake STA for this purpose.
       */
      id = mtlk_stadb_add_sta(&nic->slow_ctx->stadb, addr.au8Addr, 0);
      if (id == -1) {
        ELOG("Can not add a fake STA for disconnect! Asserting FW!");
        mtlk_hw_set_prop(nic->hw, MTLK_HW_RESET, NULL, 0);
        res = MTLK_ERR_WRONG_CONTEXT;
        goto FINISH;
      }
    }
    sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, id);
    mtlk_osal_lock_acquire(&sta->lock);
    sta_state = sta->state;
    if (sta_state == PEER_CONNECTED) {
      mtlk_stadb_prepare_disconnect_sta(&nic->slow_ctx->stadb, sta);
      if (sta->dot11n_mode) {
        mtlk_addba_on_disconnect_ex(&nic->slow_ctx->addba, &addr,
                                    &sta->addba_peer, HANDLE_T(id));
      }
    }
    mtlk_osal_lock_release(&sta->lock);
  }

  if (net_state == NET_STATE_HALTED || (sta && sta_state != PEER_CONNECTED)) {
    /* Do not send anything to halted MAC or if STA hasn't been connected */
    res = MTLK_ERR_OK;
    goto FINISH;
  }

  if (blocked) {
    res = mtlk_txmm_msg_init(&blocked_man_msg);
    if (res != MTLK_ERR_OK) {
      ELOG("Can't send(b) DISCONNECT request to MAC due to msg init error");
      goto FINISH;
    }  
    man_msg = &blocked_man_msg;
  }
  else if (sta) {
    man_msg = &sta->async_man_msgs[STAE_AMM_DISCONNECT];
  }
  else {
    ELOG("No message to send!");
    MTLK_ASSERT(0);
    goto FINISH;
  }

  ILOG0(GID_CORE,"STA %Y Disconnect request", addr.au8Addr);

  man_entry = mtlk_txmm_msg_get_empty_data(man_msg, nic->slow_ctx->hw_cfg.txmm);
  if (man_entry == NULL) {
    ELOG("Can't send DISCONNECT request to MAC due to the lack of MAN_MSG");
    return MTLK_ERR_UNKNOWN;
  }
  man_entry->id           = UM_MAN_DISCONNECT_REQ;
  man_entry->payload_size = sizeof(UMI_DISCONNECT);
  psUmiDisconnect = (UMI_DISCONNECT *)man_entry->payload;
  psUmiDisconnect->u16Status = UMI_OK;
  memcpy(psUmiDisconnect->sStationID.au8Addr, addr.au8Addr, ETH_ALEN);
  DUMP2(psUmiDisconnect, sizeof(UMI_DISCONNECT), "dump of UMI_DISCONNECT:");

  if (blocked) {
    res = mtlk_txmm_msg_send_blocked(man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);

    if ((res == MTLK_ERR_TIMEOUT) && nic->slow_ctx->is_halt_fw_on_disc_timeout) {
      mtlk_core_set_net_state(nic, NET_STATE_HALTED);
      goto FINISH;
    }

    if (res != MTLK_ERR_OK) {
        goto FINISH;
    }
    
    mtlk_disconnect_cfm_clb(HANDLE_T(nic), man_entry, MTLK_TXMM_CLBR_CONFIRMED);
  }
  else {
    res = mtlk_txmm_msg_send(man_msg, mtlk_disconnect_cfm_clb, HANDLE_T(nic), MTLK_MM_BLOCKED_SEND_TIMEOUT);
    if (res == MTLK_ERR_OK)
      goto FINISH;
  }

FINISH:
  if (blocked && man_msg) {
    mtlk_txmm_msg_cleanup(man_msg);
  }
  return res;
}


int
mtlk_disconnect_sta(struct nic *nic, unsigned char *mac)
{
  ASSERT(nic->ap);
  send_disconnect_req(nic, mac, FALSE);
  return 0;
}

static int
clear_group_key(struct nic *nic)
{
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_CLEAR_KEY    *umi_cl_key;
  int res;

  ILOG1(GID_CORE, "%s", nic->ndev->name);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, nic->slow_ctx->hw_cfg.txmm, NULL);
  if (!man_entry) {
    return -ENOMEM;
  }

  umi_cl_key = (UMI_CLEAR_KEY*) man_entry->payload;
  memset(umi_cl_key, 0, sizeof(*umi_cl_key));
  man_entry->id = UM_MAN_CLEAR_KEY_REQ;
  man_entry->payload_size = sizeof(*umi_cl_key);

  umi_cl_key->u16KeyType = cpu_to_le16(UMI_RSN_GROUP_KEY);

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) 
    ELOG("mtlk_mm_send_blocked failed: %i", res);

  mtlk_txmm_msg_cleanup(&man_msg);

  return (res == MTLK_ERR_OK ? 0 : -EFAULT);
}

static void
reset_security_stuff(struct nic *nic)
{
  memset(&nic->slow_ctx->rsnie, 0, sizeof(nic->slow_ctx->rsnie));
  if (nic->ap)
    clear_group_key(nic);
  if (mtlk_set_mib_rsn(nic->slow_ctx->hw_cfg.txmm, 0))
    ELOG("%s: Failed to reset RSN", nic->ndev->name);

  nic->slow_ctx->wep_enabled = FALSE;
}

BOOL
can_disconnect_now(struct nic *nic)
{
  return !mtlk_scan_is_running(&nic->slow_ctx->scan);
}

/* This interface can be used if we need to disconnect while in 
 * atomic context (for example, when disconnecting from a timer).
 * Disconnect process requires blocking function calls, so we
 * have to schedule a work.
 */
void 
mtlk_core_schedule_disconnect(struct nic *nic)
{
  mtlk_eq_notify(&nic->slow_ctx->eq, EVT_DISCONNECT, NULL, 0);
}

int
mtlk_disconnect_me(struct nic *nic)
{
  uint32 net_state;

  net_state = mtlk_core_get_net_state(nic);
  if ((net_state != NET_STATE_CONNECTED) &&
      (net_state != NET_STATE_HALTED)) { /* allow disconnect for clean up */
    ILOG0(GID_CORE, "%s: disconnect in invalid state %s", nic->ndev->name,
      mtlk_net_state_to_string(net_state));
    return -EINVAL;
  }
  if (!can_disconnect_now(nic)) {
    mtlk_core_schedule_disconnect(nic);
    return MTLK_ERR_OK;
  }
  reset_security_stuff(nic);
  return send_disconnect_req(nic, NULL, TRUE);
}

/*****************************************************************************
**
** NAME         mtlk_send_null_packet
**
** PARAMETERS   nic           Card context
**              sta                 Destination STA
**
** RETURNS      none
**
** DESCRIPTION  Function used to send NULL packets from STA to AP in order
**              to support AutoReconnect feature (NULL packets are treated as
**              keepalive data)
**
******************************************************************************/
void
mtlk_send_null_packet (struct nic *nic, sta_entry *sta)
{
  mtlk_hw_send_data_t data;
  int                 ires = MTLK_ERR_UNKNOWN;
  struct sk_buff      *skb;
  struct skb_private  *skb_priv;
  mtlk_sq_peer_ctx_t  *sq_ppeer;

  // Transmit only if connected
  if (mtlk_core_get_net_state(nic) != NET_STATE_CONNECTED)
    return;

  // STA only ! Transmit only if data was not stopped (by 11h)
  if (nic->slow_ctx->dot11h.data_stop == 1) {
    goto END;
  }

  MTLK_ASSERT (NULL != sta);
  sq_ppeer = &sta->sq_peer_ctx;

  /* If there is any unconfirmed packet in FW, do not send null data packet */
  if (mtlk_osal_atomic_get(&sq_ppeer->used) > 0) {
    goto END;
  }

  // XXX, klogg: revisit
  // Use NULL sk_buffer (really UGLY)
  skb = dev_alloc_skb(0);
  if (!skb) {
    ILOG1(GID_CORE, "there is no free sk_buff to send NULL packet");
    goto END;
  }
  skb_priv = (struct skb_private *)(skb->cb);
  skb_priv->dst_sta = sta;

  memset(&data, 0, sizeof(data));

  data.msg = mtlk_hw_get_msg_to_send(nic->hw, NULL);
  // Check free MSDUs
  if (!data.msg) {
    ILOG1(GID_CORE, "there is no free msg to send NULL packet");
    dev_kfree_skb(skb);
    return;
  }

  ILOG3(GID_CORE, "got from hw msg %p", data.msg);

  data.nbuf            = skb;
  data.size            = 0;
  data.rcv_addr        = (IEEE_ADDR *)sta->mac;
  data.wds             = 0;
  data.access_category = UMI_USE_DCF;
  data.encap_type      = ENCAP_TYPE_ILLEGAL;
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  data.rf_mgmt_data    = mtlk_stadb_get_sta_rf_mgmt_data(sta);
#endif

  mtlk_osal_atomic_inc(&sq_ppeer->used);
  ires = mtlk_hw_send_data(nic->hw, &data);
  if (__LIKELY(ires == MTLK_ERR_OK)) {
    msdu_tx_inc_nof_used(nic, AC_BE);
  }
  else {
    WLOG("hw_send (NULL) failed with Err#%d", ires);
    mtlk_osal_atomic_dec(&sq_ppeer->used);
    mtlk_hw_release_msg_to_send(nic->hw, data.msg);
  }

END:
  return;
}

static __INLINE BOOL
mtlk_core_is_mc_group_member(struct nic *nic, const uint8* group_addr)
{
  MTLK_ASSERT(NULL != nic);
  MTLK_ASSERT(NULL != group_addr);

  /* check if we subscribed to all multicast */
  if (nic->ndev->allmulti) {
    return TRUE;
  }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
  {
    struct dev_mc_list *mc_list;
    for (mc_list = nic->ndev->mc_list;
         mc_list != NULL;
         mc_list = mc_list->next) {
      if (!mtlk_osal_compare_eth_addresses(mc_list->dmi_addr, group_addr)) {
        return TRUE;
      }
    }
  }
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35) */
  {
    struct netdev_hw_addr *ha;
      netdev_for_each_mc_addr(ha, nic->ndev) {
        if (!mtlk_osal_compare_eth_addresses(ha->addr, group_addr)) {
          return TRUE;
        }
      }
  }
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35) */
  return FALSE;
}

/*****************************************************************************
**
** DESCRIPTION  Determines if packet should be passed to upper layer and/or
**              forwarded to BSS. Sets skb flags.
**
** 1. Unicast (to us)   -> pass to upper layer.
** 2. Broadcast packet  -> pass to upper layer AND forward to BSS.
** 3. Multicast packet  -> forward to BSS AND check if AP is in multicast group,
**                           if so - pass to upper layer.
** 4. Unicast (not to us) -> if STA found in connected list - forward to BSS.
**
******************************************************************************/
static struct skb_private *
analyze_rx_packet (struct sk_buff *skb)
{
  struct skb_private *skb_priv = (struct skb_private *)(skb->cb);
  struct nic *nic = netdev_priv(skb->dev);
  struct ethhdr *ether_header = (struct ethhdr *)skb->data;

  // check if device in PROMISCUOUS mode
  if (nic->ndev->promiscuity)
    skb_priv->flags |= SKB_CONSUME;
    
  // check if destination MAC is our address
  if (!memcmp(ether_header->h_dest, nic->mac_addr, ETH_ALEN))
    skb_priv->flags |= SKB_CONSUME;

  // check if destination MAC is broadcast address
  else if (mtlk_osal_eth_is_broadcast(ether_header->h_dest)) {
    skb_priv->flags |= SKB_BROADCAST + SKB_CONSUME;
    if ((nic->ap) && (nic->ap_forwarding))
      skb_priv->flags |= SKB_FORWARD;
  }
  // check if destination MAC is multicast address
  else if (mtlk_osal_eth_is_multicast(ether_header->h_dest)) {
    skb_priv->flags |= SKB_MULTICAST;

    if ((nic->ap) && (nic->ap_forwarding))
      skb_priv->flags |= SKB_FORWARD;

    if(mtlk_core_is_mc_group_member(nic, ether_header->h_dest)) {
      skb_priv->flags |= SKB_CONSUME;
    }
  }
  // check if destination MAC is unicast address of connected STA
  else {
    skb_priv->flags |= SKB_UNICAST;
    if ((nic->ap) && (nic->ap_forwarding)) {
      // Search of DESTINATION MAC ADDRESS of RECEIVED packet
      int dst_sta_id = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, ether_header->h_dest);
      if (dst_sta_id != -1) {
        skb_priv->dst_sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, dst_sta_id);
        skb_priv->flags |= SKB_FORWARD;
      }
    }
  }

  return skb_priv;
}



/*****************************************************************************
**
** NAME         mtlk_wrap_transmit
**
** PARAMETERS   skb                 Skbuff to transmit
**              dev                 Networking device context
**
** RETURNS      Transmit operation result
**
** DESCRIPTION  This small wrapper just perform sk_buff control block
**              initialisation
**
******************************************************************************/
int
mtlk_wrap_transmit (struct sk_buff *skb, struct net_device *dev)
{
  struct skb_private *skb_priv;
  struct nic *nic = netdev_priv(skb->dev);
  struct ethhdr *ether_header = (struct ethhdr *)skb->data;
  uint16 ac;

  CPU_STAT_BEGIN_TRACK(CPU_STAT_ID_TX_OS);

  /* Transmit only if connected */
  if (unlikely(mtlk_core_get_net_state(nic) != NET_STATE_CONNECTED)) {
    ILOG2(GID_CORE, "Tx in not connected state");
    goto ERR;
  }

  /* Transmit only if there are someone connected to us */
  if (unlikely(nic->slow_ctx->connected == 0)) {
    goto ERR;
  }

  /* get and initialize private fields */
  skb_priv = (struct skb_private *)(skb->cb);
  skb_priv->flags = 0;
  skb_priv->dst_sta = NULL;
  skb_priv->src_sta = NULL;
  skb_priv->extra = NULL;

  /* analyze (and probably adjust) packet's priority and AC */
  ac = mtlk_qos_get_ac(&nic->qos, skb);
  if (unlikely(ac == AC_INVALID)) {
    ILOG3(GID_CORE, "Packet dropped by ACM facility");
    goto ERR;
  }

  /* check frame urgency (currently for the 802.1X packets only) */
  if (mtlk_wlan_pkt_is_802_1X(ether_header->h_proto))
    skb_priv->flags |= SKB_URGENT;
  
  switch (nic->bridge_mode)
  {
  case BR_MODE_MAC_CLONING: /* MAC cloning stuff */
    /* these frames will generate MIC failures on AP and they have
     * no meaning in MAC Cloning mode - so we just drop them silently */
    if (!(skb_priv->flags & SKB_URGENT) && 
         (memcmp(nic->mac_addr, ether_header->h_source, ETH_ALEN))) {

      ILOG3(GID_CORE, "Packet will cause MIC failure, dropped");
      goto ERR;
    }
    break;

  case BR_MODE_L2NAT: /* L2NAT stuff */
    /* call the hook */
    skb = mtlk_l2nat_on_tx(nic, skb);

    /* update ethernet header & skb_priv pointers */
    ether_header = (struct ethhdr *)skb->data;
    skb_priv = (struct skb_private *)(skb->cb);

    /* fall through */
  case BR_MODE_WDS: /* WDS stuff */
    if (!nic->ap && memcmp(nic->mac_addr, ether_header->h_source, ETH_ALEN)) {

      /* update or add host in case 802.3 SA is not ours */
      mtlk_stadb_update_host(&nic->slow_ctx->stadb, ether_header->h_source, 0);
    }
    break;

  default: /* no bridging */
    break;
  }

  ILOG4(GID_CORE, "802.3 tx DA: %Y", ether_header->h_dest);
  ILOG4(GID_CORE, "802.3 tx SA: %Y", ether_header->h_source);

  /* check frame destination */
  if (mtlk_osal_eth_is_broadcast(ether_header->h_dest))
    skb_priv->flags |= SKB_BROADCAST;
  else if (mtlk_osal_eth_is_multicast(ether_header->h_dest))
    skb_priv->flags |= SKB_MULTICAST;
  else {
    /* On STA we have only AP connected at id 0, so we do not need
     * to perform search of destination id */
    if (!nic->ap) {
      skb_priv->dst_sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, 0);
    /* On AP we need to find destination STA id in database of peers
     * (both registered HOSTs and connected STAs) */
    } else {
      int dst_sta_id = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, ether_header->h_dest);
      if (dst_sta_id == -1) {
        ILOG3(GID_CORE, "Unknown destination STA");
        goto ERR;
      }
      skb_priv->dst_sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, dst_sta_id);
      if (skb_priv->dst_sta->state != PEER_CONNECTED) {
        skb_priv->dst_sta = NULL;
        ILOG3(GID_CORE, "Destination STA is not connected");
        goto ERR;
      }
    }
    skb_priv->flags |= SKB_UNICAST;
  }

  ILOG9(GID_CORE, "skb %p, netdev %p", skb, dev);

  mtlk_mc_transmit(skb);

  CPU_STAT_END_TRACK(CPU_STAT_ID_TX_OS);

  /* since we're freeing packet that failed to TX - always return NETDEV_TX_OK here */
  return NETDEV_TX_OK;

ERR:
  dev_kfree_skb(skb);

  CPU_STAT_END_TRACK(CPU_STAT_ID_TX_OS);

  return NETDEV_TX_OK;
}

static int
parse_and_get_replay_counter(struct sk_buff *skb, u8 *rc, u8 cipher)
{
  struct skb_private *skb_priv = (struct skb_private*)skb->cb;

  ASSERT(rc != NULL);

  if (cipher != IW_ENCODE_ALG_TKIP && cipher != IW_ENCODE_ALG_CCMP)
    return 1;

  rc[0] = skb_priv->rsc_buf[7];
  rc[1] = skb_priv->rsc_buf[6];
  rc[2] = skb_priv->rsc_buf[5];
  rc[3] = skb_priv->rsc_buf[4];

  if (cipher == IW_ENCODE_ALG_TKIP) {
    rc[4] = skb_priv->rsc_buf[0];
    rc[5] = skb_priv->rsc_buf[2];
  } else if (cipher == IW_ENCODE_ALG_CCMP) {
    rc[4] = skb_priv->rsc_buf[1];
    rc[5] = skb_priv->rsc_buf[0];
  }

  return 0;
}

static int
detect_replay(struct sk_buff *skb, u8 *last_rc)
{
  struct nic *nic = netdev_priv(skb->dev);
  struct skb_private *skb_priv = (struct skb_private*)skb->cb;
  sta_entry *sta;
  u8 cipher;
  int res;
  u8 rc[6]; // replay counter
  u8 rsn_bits = skb_priv->rsn_bits;
  u8 nfrags;  // number of fragments this MSDU consisted of

  sta = skb_priv->src_sta;
  ASSERT(sta != NULL);
  ASSERT(sta->state != PEER_UNUSED);

  cipher = get_cipher(skb);

  if (cipher != IW_ENCODE_ALG_TKIP && cipher != IW_ENCODE_ALG_CCMP)
    return 0;

  res = parse_and_get_replay_counter(skb, rc, cipher);
  ASSERT(res == 0);

  ILOG3(GID_CORE, "last RSC %02x%02x%02x%02x%02x%02x, got RSC %02x%02x%02x%02x%02x%02x",
   last_rc[0], last_rc[1], last_rc[2], last_rc[3], last_rc[4], last_rc[5],
   rc[0], rc[1], rc[2], rc[3], rc[4], rc[5]);

  res = memcmp(rc, last_rc, sizeof(rc));

  if (res <= 0) {
    ILOG2(GID_CORE, "replay detected from %Y last RSC %02x%02x%02x%02x%02x%02x, got RSC %02x%02x%02x%02x%02x%02x",
        sta->mac,
        last_rc[0], last_rc[1], last_rc[2], last_rc[3], last_rc[4], last_rc[5],
        rc[0], rc[1], rc[2], rc[3], rc[4], rc[5]);
    nic->pstats.replays_cnt++;
    return 1;
  }

  ILOG3(GID_CORE, "rsn bits 0x%02x", rsn_bits);
  if (rsn_bits & UMI_NOTIFICATION_MIC_FAILURE) {
    union iwreq_data wrqu;
    struct iw_michaelmicfailure mic;

    wrqu.data.length = sizeof(struct iw_michaelmicfailure);

    memset(&mic, 0, sizeof(mic));
    mic.src_addr.sa_family = ARPHRD_ETHER;
    memcpy(mic.src_addr.sa_data, sta->mac, ETH_ALEN);

    if (skb_priv->flags & SKB_DIRECTED) {
      mic.flags |= IW_MICFAILURE_PAIRWISE;
      WLOG("pairwise MIC failure from %Y", sta->mac);
    } else {
      mic.flags |= IW_MICFAILURE_GROUP;
      WLOG("group MIC failure from %Y", sta->mac);
    }

    if (cipher == IW_ENCODE_ALG_TKIP)
      wireless_send_event(nic->ndev, IWEVMICHAELMICFAILURE, &wrqu, (char*)&mic);

    return 2;
  }

  nfrags = (rsn_bits & 0xf0) >> 4;
  if (nfrags) {
    uint64 u64buf = 0;
    char *p = (char*)&u64buf + sizeof(u64buf)-sizeof(rc);
    memcpy(p, rc, sizeof(rc));
    u64buf = be64_to_cpu(u64buf);
    u64buf += nfrags;
    u64buf = cpu_to_be64(u64buf);
    memcpy(rc, p, sizeof(rc));
  }

  memcpy(last_rc, rc, sizeof(rc));

  return 0;
}

/*****************************************************************************
**
** NAME         video_classifier_register / video_classifier_unregister
**
** DESCRIPTION  This functions are used for registration of Metalink's
**              video classifier module
**
******************************************************************************/
int mtlk_classifier_register (void * classify_fn)
{
  mtlk_qos_classifier_register((mtlk_qos_do_classify_f)classify_fn);
  return 0; /*TODO: mtlk_err_to_linux_err(res)*/
}


void mtlk_classifier_unregister (void)
{
  mtlk_qos_classifier_unregister();
}

#define SEQUENCE_NUMBER_LIMIT                    (0x1000)
#define SEQ_DISTANCE(seq1, seq2) (((seq2) - (seq1) + SEQUENCE_NUMBER_LIMIT) \
                                    % SEQUENCE_NUMBER_LIMIT)

// Send Packet to the OS's protocol stack
// (or forward)
static void
send_up (struct sk_buff *skb)
{
  int res;
  struct skb_private *skb_priv = (struct skb_private *)(skb->cb);
  struct nic *nic = netdev_priv(skb->dev);
  sta_entry *sta = skb_priv->src_sta;

  if (skb_priv->flags & SKB_FORWARD) {

    /* Clone received packet for forwarding */
    struct sk_buff *nskb = NULL;
    uint16 ac;

    CPU_STAT_BEGIN_TRACK(CPU_STAT_ID_TX_FWD);

    /* Count rxed data bo be forwarded */
    nic->pstats.fwd_rx_packets++;
    nic->pstats.fwd_rx_bytes += skb->len;

    /* analyze (and probably adjust) packet's priority and AC */
    ac = mtlk_qos_get_ac(&nic->qos, skb);
    if (unlikely(ac == AC_INVALID)) {
      ILOG3(GID_CORE, "Forwarding prohibited by ACM facility");
    } else
      nskb = skb_clone(skb, GFP_ATOMIC);

    if (likely(nskb != NULL))
      res = mtlk_mc_transmit(nskb);
    else
      nic->pstats.fwd_dropped++;

    CPU_STAT_END_TRACK(CPU_STAT_ID_TX_FWD);

  }

  if (skb_priv->flags & SKB_CONSUME) {
    struct ethhdr *ether_header = (struct ethhdr *)skb->data;
#if defined MTLK_DEBUG_IPERF_PAYLOAD_RX
    //check if it is an iperf's packet we use to debug
    mtlk_iperf_payload_t *iperf = debug_ooo_is_iperf_pkt((uint8*) ether_header);
    if (iperf != NULL) {
      iperf->ts.tag_tx_to_os = htonl(debug_iperf_priv.tag_tx_to_os);
      debug_iperf_priv.tag_tx_to_os++;
    }
#endif

    ILOG3(GID_CORE, "802.3 rx DA: %Y", skb->data);
    ILOG3(GID_CORE, "802.3 rx SA: %Y", skb->data+ETH_ALEN);

    ILOG3(GID_CORE, "packet protocol %04x", ntohs(ether_header->h_proto));

    /* NOTE: Operations below can change packet payload, so it seems that we
     * may need to perform skb_unshare for the packte. But they are available
     * only for station, which does not perform packet forwarding on RX, so
     * packet cannot be cloned (flag SKB_FORWARD is unset). In future this may
     * change (everything changes...) so we will need to unshare here */
    if ((!nic->ap) && (nic->bridge_mode != BR_MODE_WDS))
      mtlk_mc_restore_mac(skb);
    switch (nic->bridge_mode)
    {
    case BR_MODE_MAC_CLONING:
      if (mtlk_wlan_pkt_is_802_1X(ether_header->h_proto)) {
        memcpy(skb->data, skb->dev->dev_addr, ETH_ALEN);
        ILOG2(GID_CORE, "MAC Cloning enabled, DA set to %Y", skb->data);
      }
      break;
    case BR_MODE_L2NAT:
      mtlk_l2nat_on_rx(nic, skb);
      break;
    default:
      break;
    }

    if (mtlk_core_get_net_state(nic) != NET_STATE_CONNECTED) {
      dev_kfree_skb(skb);
      ILOG1(GID_CORE, "Data rx in not connected state, dropped");
      return;
    }

    if (skb_priv->flags & SKB_BROADCAST)
      nic->pstats.rx_bcast++;
    else if (skb_priv->flags & SKB_MULTICAST)
      nic->pstats.rx_nrmcast++;
#ifndef DONT_PERFORM_PRIV_CLEANUP
    memset(skb->cb, 0, sizeof(struct skb_private));
#endif /* DONT_PERFORM_PRIV_CLEANUP */

#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
    res = _mtlk_core_ppa_send_up_packet(nic, skb);
#else
    skb->protocol = eth_type_trans(skb, skb->dev);
    res = netif_rx(skb);
#endif /* CONFIG_IFX_PPA_API_DIRECTPATH */

    if (res != NET_RX_SUCCESS)
      ILOG2(GID_CORE, "netif_rx failed: %d", res);
    nic->ndev->last_rx = jiffies;
#ifdef MTLK_DEBUG_CHARIOT_OOO
    /* check out-of-order */
    {
      int diff, seq_prev;

      seq_prev = nic->seq_prev_sent[skb_priv->seq_qos];
      //LOG2_("qos %d sn %u\n", skb_priv->seq_qos, skb_priv->seq_num);
      diff = SEQ_DISTANCE(seq_prev, skb_priv->seq_num);
      if (diff > SEQUENCE_NUMBER_LIMIT / 2)
        ILOG2(GID_CORE, "ooo: qos %u prev = %u, cur %u\n", skb_priv->seq_qos, seq_prev, skb_priv->seq_num);
      nic->seq_prev_sent[skb_priv->seq_qos] = skb_priv->seq_num;
    }
#endif
    // Count only packets sent to OS
    sta->stats.rx_packets++;
    nic->stats.rx_packets++;
    nic->stats.rx_bytes += skb->len;

    ILOG3(GID_CORE, "skb %p, rx_packets %lu", skb, nic->stats.rx_packets);
  } else {
    dev_kfree_skb(skb);
    ILOG3(GID_CORE, "skb %p dropped - consumption is disabled", skb);
  }
}

void
send_up_sub_frames (struct sk_buff *skb)
{
  struct sk_buff *skb_next;
  struct skb_private *skb_priv;
  do 
  {
    skb_priv = (struct skb_private*)skb->cb;
    skb_next = skb_priv->sub_frame;
    send_up(skb);
    skb = skb_next;
  } while(skb);
}

/* free skb buffer with all subframes for a-msdu */
void mtlk_df_skb_free_sub_frames(struct sk_buff *skb)
{
  struct sk_buff *skb_next;
  struct skb_private *skb_priv;

  do
  {
    skb_priv = (struct skb_private*)skb->cb;
    skb_next = skb_priv->sub_frame;
    dev_kfree_skb(skb);
    skb = skb_next;
  } while(skb);
}


int
mtlk_detect_replay_or_sendup(struct sk_buff *skb, u8 *rsn)
{
  int res=0;
  struct skb_private *skb_priv = (struct skb_private *)(skb->cb);

  if (skb_priv->src_sta != NULL)
    res = detect_replay(skb, rsn);

  if (res != 0) {
    mtlk_df_skb_free_sub_frames(skb);
  } else {
    send_up_sub_frames(skb);
  }

  return res;
}

int16 __MTLK_IFUNC
mtlk_calc_tx_power_lim_wrapper(mtlk_handle_t usr_data, int8 spectrum_mode, uint8 channel) 
{
    uint8                 num_antennas = 0;
    char*                 tx_antennas;
    struct nic* nic = (struct nic*)usr_data;
    
    tx_antennas = mtlk_get_mib_value(PRM_TX_ANTENNAS, nic);
    ILOG4(GID_CORE, "MIB 'TxAntennas' = '%s'", tx_antennas);

    while (*tx_antennas) {
      if (*tx_antennas++ - '0')
        num_antennas++;
    }

    return mtlk_calc_tx_power_lim(&nic->slow_ctx->tx_limits, 
                                  channel,
                                  country_code_to_domain(nic->slow_ctx->cfg.country_code),
                                  spectrum_mode,
                                  nic->slow_ctx->bonding,
                                  num_antennas);
}

int16 __MTLK_IFUNC
mtlk_scan_calc_tx_power_lim_wrapper(mtlk_handle_t usr_data, int8 spectrum_mode, uint8 reg_domain, uint8 channel) 
{
    uint8                 num_antennas = 0;
    char*                 tx_antennas;
    struct nic* nic = (struct nic*)usr_data;
    
    tx_antennas = mtlk_get_mib_value(PRM_TX_ANTENNAS, nic);
    ILOG3(GID_CORE, "MIB 'TxAntennas' = '%s'", tx_antennas);

    while (*tx_antennas) {
      if (*tx_antennas++ - '0')
        num_antennas++;
    }

    return mtlk_calc_tx_power_lim(&nic->slow_ctx->tx_limits, 
                                  channel,
                                  reg_domain,
                                  spectrum_mode,
                                  nic->slow_ctx->bonding,
                                  num_antennas);
}

int16 __MTLK_IFUNC
mtlk_get_antenna_gain_wrapper(mtlk_handle_t usr_data, uint8 channel) 
{
    struct nic* nic = (struct nic*)usr_data;
    
    return mtlk_get_antenna_gain(&nic->slow_ctx->tx_limits, channel);
}

int __MTLK_IFUNC
mtlk_reload_tpc_wrapper (uint8 channel, mtlk_handle_t usr_data)
{
    struct nic* nic = (struct nic*)usr_data;
    
    return mtlk_reload_tpc (nic->slow_ctx->spectrum_mode,
        nic->slow_ctx->bonding,
        channel,
        nic->slow_ctx->hw_cfg.txmm,
        nic->txmm_async_eeprom_msgs,
        ARRAY_SIZE(nic->txmm_async_eeprom_msgs),
        &nic->slow_ctx->ee_data);
}

void
mtlk_xmit_err(struct nic *nic, struct sk_buff *skb)
{
  struct skb_private *skb_priv = (struct skb_private *)(skb->cb);

  if (!(skb_priv->flags & SKB_FORWARD)) {
    nic->stats.tx_dropped++;
  } else {
    nic->pstats.fwd_dropped++;
  }

  dev_kfree_skb(skb);

  if (++nic->pstats.tx_cons_drop_cnt > nic->pstats.tx_max_cons_drop)
    nic->pstats.tx_max_cons_drop = nic->pstats.tx_cons_drop_cnt;
}

/*****************************************************************************
**
** NAME         mtlk_xmit
**
** PARAMETERS   skb                 Skbuff to transmit
**              dev                 Device context
**
** RETURNS      Skbuff transmission status
**
** DESCRIPTION  This function called to perform packet transmission.
**
******************************************************************************/
int
mtlk_xmit (struct sk_buff *skb, struct net_device *dev)
{
  struct nic *nic = netdev_priv(dev);
  struct skb_private *skb_priv = (struct skb_private *)(skb->cb);
  int res = MTLK_ERR_PKT_DROPPED; 
  unsigned short ac = mtlk_qos_get_ac_by_tid(skb->priority);
  sta_entry *sta = NULL;
  uint32 ntx_free = 0;
  mtlk_hw_send_data_t data;
  char *rcv_addr;
  struct ethhdr *ether_header = (struct ethhdr *)skb->data;

  memset(&data, 0, sizeof(data));

  DUMP5(skb->data, skb->len, "skb->data received from OS");

  /* In STA mode all packets are unicast -
   * so we always have peer entry in this mode */
  if (!nic->ap) {
    sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, 0);

  /* For AP's unicast and reliable multicast if destination STA
   * id is known - get sta entry, otherwise - drop packet */
  } else if (likely(skb_priv->flags & (SKB_UNICAST | SKB_RMCAST))) {
    if (unlikely(skb_priv->dst_sta == NULL)) {
      ELOG("Destination STA id is not known!");
      goto tx_skip;
    }
    sta = skb_priv->dst_sta;
  }

  if (NULL != sta) {
    mtlk_pckt_filter_e filter = mtlk_stadb_sta_get_filter(sta);
    if (MTLK_PCKT_FLTR_ALLOW_ALL == filter) {
      /* all packets are allowed */
      ;
    }
    else if ((MTLK_PCKT_FLTR_ALLOW_802_1X == filter) &&
              mtlk_wlan_pkt_is_802_1X(MTLK_ETH_GET_ETHER_TYPE(skb->data))) {
      /* only 802.1x packets are allowed */
      ;
    }
    else {
      ILOG3(GID_CORE, "packet filtered out (dropped): filter %d", filter);
      goto tx_skip;
    }
  }
  
  data.msg = mtlk_hw_get_msg_to_send(nic->hw, &ntx_free);
  if (ntx_free == 0) { // Do not accept packets from OS anymore
    ILOG2(GID_CORE, "mtlk_xmit 0, call mtlk_flctrl_stop");
#if MT_FLCTRL_ENABLED 
    mtlk_flctrl_stop (&nic->flctrl, nic->flctrl_id);
#else

    if (nic->pack_sched_enabled)
      nic->tx_prohibited = 1;
    else netif_stop_queue(dev);
#endif
  }

  if (!data.msg) {
    ILOG5(GID_CORE, "No free MSDUs available, outgoing packet for AC %d dropped", ac);
     ++nic->pstats.ac_dropped_counter[ac];
    nic->stats.tx_fifo_errors++;
    goto tx_skip;
  }

  ++nic->pstats.ac_used_counter[ac];

  ILOG4(GID_CORE, "got from hw_msg %p", data.msg);

  /* check if wds should be used:
   *  - WDS option must be enabled
   *  - destination STA must be known (i.e. unicast being sent)
   *  - 802.3 DA is not equal to destination STA's MAC address
   */
  if ((nic->bridge_mode == BR_MODE_WDS) &&
      (sta != NULL) &&
      memcmp(sta->mac, ether_header->h_dest, ETH_ALEN))
    data.wds = 1;

  if (!nic->ap) {
    /* always AP's MAC address on the STA */
    rcv_addr = nic->slow_ctx->bssid;
  } else if (data.wds || (skb_priv->flags & SKB_RMCAST)) {
    /* use RA on AP when WDS enabled or Reliable Multicast used */
    ASSERT(sta);
    rcv_addr = sta->mac;
  } else {
    /* 802.3 DA on AP when WDS not needed */
    rcv_addr = ether_header->h_dest;
  }
  ILOG3(GID_CORE, "RA: %Y", rcv_addr);

  data.nbuf            = skb;
  data.size            = skb->len;
  data.rcv_addr        = (IEEE_ADDR *)rcv_addr;
  data.access_category = (uint8)skb->priority;
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  data.rf_mgmt_data    = sta?mtlk_stadb_get_sta_rf_mgmt_data(sta):MTLK_RF_MGMT_DATA_DEFAULT;
#endif

  if (ntohs(ether_header->h_proto) <= ETH_DATA_LEN) {
    data.encap_type = ENCAP_TYPE_8022;
  } else {
    switch (ether_header->h_proto) {
    case __constant_htons(ETH_P_AARP):
    case __constant_htons(ETH_P_IPX):
      data.encap_type = ENCAP_TYPE_STT;
      break;
    default:
      data.encap_type = ENCAP_TYPE_RFC1042;
      break;
    }
  }

  res = mtlk_hw_send_data(nic->hw, &data);
  if (__LIKELY(res == MTLK_ERR_OK)) {
    msdu_tx_inc_nof_used(nic, ac);

    if (nic->ap)
      mtlk_aocs_on_tx_msdu_sent(&nic->slow_ctx->aocs, 
                                msdu_tx_get_nof_used_array(nic), ac,
                                mtlk_sq_get_limit(nic->sq, ac),
                                mtlk_sq_get_qsize(nic->sq, ac));
  }
  else {
    ELOG("hw_send failed with Err#%d", res);
    goto tx_skip; /* will also release msg */
  }

  nic->pstats.sta_session_tx_packets++;
  nic->pstats.ac_tx_counter[ac]++;

  if (!(skb_priv->flags & SKB_FORWARD)) {
    nic->stats.tx_packets++;
    nic->stats.tx_bytes += skb->len;
  } else {
    nic->pstats.fwd_tx_packets++;
    nic->pstats.fwd_tx_bytes += skb->len;
  }

  // count total packets to STA if unicast or reliable
  // multicast being transmitted
  if (sta != NULL) {
    sta->stats.tx_packets++;
  } else {
    /* this should be broadcast or non-reliable multicast packet */
    if (skb_priv->flags & SKB_BROADCAST)
      nic->pstats.tx_bcast++;
    else
      nic->pstats.tx_nrmcast++;
  }

  // reset consecutive drops counter
  nic->pstats.tx_cons_drop_cnt = 0;

  dev->trans_start = jiffies;

  return MTLK_ERR_OK;

tx_skip:
  if (data.msg) {
    mtlk_hw_release_msg_to_send(nic->hw, data.msg);
  }

  mtlk_xmit_err(nic, skb);

  if (sta)
    sta->stats.tx_dropped++;

  return res;
}

static void
mtlk_timer_handler (unsigned long data)
{
  struct net_device *dev = (struct net_device*)data;
  struct nic *nic = netdev_priv(dev);

  mtlk_stadb_flush_sta(&nic->slow_ctx->stadb);
  mod_timer(&nic->slow_ctx->mtlk_timer, jiffies + MTLK_TIMER_PERIOD);
}

static int
set_80211d_mibs (struct nic *nic, uint8 spectrum)
{
  uint8 mitigation = mtlk_get_channel_mitigation(country_code_to_domain(nic->slow_ctx->cfg.country_code),
      nic->slow_ctx->is_ht_cur,
      spectrum,
      nic->slow_ctx->aocs.cur_channel);
  
  ILOG3(GID_CORE, "Setting MIB_COUNTRY for 0x%x domain with HT %s", country_code_to_domain(nic->slow_ctx->cfg.country_code), nic->slow_ctx->is_ht_cur? "enabled" : "disabled");
  mtlk_set_mib_value_uint8(nic->slow_ctx->hw_cfg.txmm, MIB_SM_MITIGATION_FACTOR, mitigation);
  mtlk_set_mib_value_uint8(nic->slow_ctx->hw_cfg.txmm, MIB_USER_POWER_SELECTION, nic->slow_ctx->power_selection);
  return mtlk_set_country_mib(nic->slow_ctx->hw_cfg.txmm, 
                              country_code_to_domain(nic->slow_ctx->cfg.country_code), 
                              nic->slow_ctx->is_ht_cur, 
                              nic->slow_ctx->frequency_band_cur, 
                              nic->ap, 
                              country_code_to_country(nic->slow_ctx->cfg.country_code),
                              nic->slow_ctx->cfg.dot11d);
}

#ifdef DEBUG_WPS
static char test_wps_ie0[] = {0xdd, 7, 0x00, 0x50, 0xf2, 0x04, 1, 2, 3};
static char test_wps_ie1[] = {0xdd, 7, 0x00, 0x50, 0xf2, 0x04, 4, 5, 6};
static char test_wps_ie2[] = {0xdd, 7, 0x00, 0x50, 0xf2, 0x04, 7, 8, 9};
#endif

int
mtlk_send_activate (struct nic *nic)
{
  mtlk_txmm_data_t* man_entry=NULL;
  mtlk_txmm_msg_t activate_msg;
  int channel, bss_type, essid_len;
  char *bss_type_str;
  UMI_ACTIVATE *areq;
  int result = MTLK_ERR_OK;
  FREQUENCY_ELEMENT cs_cfg_s;
  uint8 u8SwitchMode, SpectrumMode;
  uint16 pl, ag;
  BOOL aocs_started = FALSE;

  if (nic->ap == 0) {
    bss_type = CFG_INFRA_STATION;
    bss_type_str = "sta";
  } else {
    bss_type = CFG_ACCESS_POINT;
    bss_type_str = "ap";
  }

#if 0
  int num_antennas = 0;
  char *tx_antennas;
#endif

#ifdef DEBUG_WPS
  mtlk_core_set_gen_ie(nic, test_wps_ie0, sizeof(test_wps_ie0), 0);
  mtlk_core_set_gen_ie(nic, test_wps_ie2, sizeof(test_wps_ie2), 2);
#endif
  // Start activation request
  ILOG2(GID_CORE, "%s", nic->ndev->name);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&activate_msg, nic->slow_ctx->hw_cfg.txmm, &result);
  if (man_entry == NULL)
  {
    ELOG("Can't send ACTIVATE request to MAC due to the lack of MAN_MSG");
    return result;
  }
  
  man_entry->id           = UM_MAN_ACTIVATE_REQ;
  man_entry->payload_size = sizeof(UMI_ACTIVATE);

  areq = (UMI_ACTIVATE *) man_entry->payload;
  memset(areq, 0, sizeof(UMI_ACTIVATE));

  channel = nic->slow_ctx->channel;
  /* for AP channel 0 means "use AOCS", but for STA channel must be set
     implicitly - we cannot send 0 to MAC in activation request */
  if ((channel == 0) && (nic->ap == 0)) {
    result = MTLK_ERR_NOT_READY;
    goto FINISH;
  }
  essid_len = strlen(nic->slow_ctx->essid);
  if (essid_len > MIB_ESSID_LENGTH) {
    essid_len = MIB_ESSID_LENGTH;
    nic->slow_ctx->essid[MIB_ESSID_LENGTH] = '\0';
    WLOG("ESSID overflow");
  }
  if (essid_len == 0) {
    ELOG("ESSID is not set");
    result = MTLK_ERR_NOT_READY;
    goto FINISH;
  }
  /* Do not allow to activate if BSSID isn't set for the STA. Probably it
   * is worth to not allow this on AP as well? */
  if (!nic->ap && is_zero_ether_addr(nic->slow_ctx->bssid)) {
    ELOG("BSSID is not set");
    result = MTLK_ERR_NOT_READY;
    goto FINISH;
  }

  if (nic->slow_ctx->cfg.country_code == 0) {
    // we must set up at least something
    nic->slow_ctx->cfg.country_code = mtlk_eeprom_get_country_code(&nic->slow_ctx->ee_data);
    if (nic->slow_ctx->cfg.country_code == 0)
      nic->slow_ctx->cfg.country_code = country_to_country_code("US");
  }

#if 0
  tx_antennas = mtlk_get_mib_value(PRM_TX_ANTENNAS, nic);
  ILOG2(GID_CORE, "MIB 'TxAntennas' = '%s'", tx_antennas);

  while (*tx_antennas) {
    if (*tx_antennas++ - '0')
      num_antennas++;
  }

  ILOG2(GID_CORE, "Found %d Tx antennas", num_antennas);
#endif

  memcpy(areq->sActivate.sBSSID.au8Addr, nic->slow_ctx->bssid, sizeof(IEEE_ADDR)); 
  strcpy(areq->sActivate.sSSID.acESSID, nic->slow_ctx->essid);
  areq->sActivate.sSSID.u8Length = essid_len;
  areq->sActivate.u16RestrictedChannel = cpu_to_le16(channel);
  areq->sActivate.u16BSStype = cpu_to_le16(bss_type);
  
  /*Upper/Lower is taken from beacon and the MIB is update*/
  ILOG2(GID_CORE, "bonding = %d", nic->slow_ctx->bonding);
  SpectrumMode = mtlk_aux_atol(mtlk_get_mib_value(PRM_SPECTRUM_MODE, nic));
  nic->slow_ctx->aocs.config.spectrum_mode = SpectrumMode;


  //nic->slow_ctx->dot11h.api.cfg->u8Bonding =  bonding;
  nic->slow_ctx->dot11h.event = MTLK_DFS_EVENT_CHANGE_CHANNEL_NORMAL;
  nic->slow_ctx->aocs.config.is_ht = nic->slow_ctx->is_ht_cur;
  nic->slow_ctx->aocs.config.frequency_band = nic->slow_ctx->frequency_band_cur;
  if (nic->ap) {
    mtlk_aocs_evt_select_t aocs_data;
    uint8 ap_scan_band_cfg;

    /* build the AOCS channel's list now */
    if (mtlk_aocs_start(&nic->slow_ctx->aocs, FALSE) != MTLK_ERR_OK) {
      ELOG("Failed to prepare AOCS for selection");
      result = MTLK_ERR_AOCS_FAILED;
      goto FINISH;
    }

    aocs_started = TRUE;

    /* now we have to perform an AP scan and update
     * the table after we have scan results. Do scan only in one band */
    ap_scan_band_cfg = nic->slow_ctx->frequency_band_cfg;
    nic->slow_ctx->frequency_band_cfg = (ap_scan_band_cfg == MTLK_HW_BAND_2_4_GHZ)
      ? MTLK_HW_BAND_2_4_GHZ : MTLK_HW_BAND_5_2_GHZ;

    if (channel == 0 || mtlk_aocs_get_type(&nic->slow_ctx->aocs) != MTLK_AOCST_NONE) {
      ILOG0("AOCS is ON (ch=%d type=%d): doing the Initial Scan",
            channel, mtlk_aocs_get_type(&nic->slow_ctx->aocs));
      if (nic->slow_ctx->aocs.config.spectrum_mode == SPECTRUM_40MHZ) {
      	ILOG1("Initial scan started SPECTRUM_40MHZ");
        /* perform CB scan to collect CB calibration data */
        if (mtlk_scan_sync(&nic->slow_ctx->scan, nic->slow_ctx->frequency_band_cfg, 1) != MTLK_ERR_OK) {
          ELOG("Initial scan failed SPECTRUM_40MHZ");
          result = MTLK_ERR_SCAN_FAILED;
          goto FINISH;
         }
      }
      ILOG1_V("Initial scan started SPECTRUM_20MHZ");
      if (mtlk_scan_sync(&nic->slow_ctx->scan, nic->slow_ctx->frequency_band_cfg, 0) != MTLK_ERR_OK) {
        ELOG("Initial scan failed SPECTRUM_20MHZ");
        result = MTLK_ERR_SCAN_FAILED;
        goto FINISH;
      }
    }
    else {
      ILOG0("AOCS is completely OFF (ch=%d type=%d): skipping the Initial Scan",
            channel, mtlk_aocs_get_type(&nic->slow_ctx->aocs));
    }

    /* now select or validate the channel */
    aocs_data.channel = channel;
    aocs_data.reason = SWR_INITIAL_SELECTION;
    aocs_data.criteria = CHC_USERDEF;
    /* On initial channel selection we may use SM required channels */
    mtlk_aocs_enable_smrequired(&nic->slow_ctx->aocs);
    result = mtlk_aocs_indicate_event(&nic->slow_ctx->aocs, MTLK_AOCS_EVENT_SELECT_CHANNEL,
      (void*)&aocs_data);
    /* After initial channel was selected we must never use sm required channels */
    mtlk_aocs_disable_smrequired(&nic->slow_ctx->aocs);
    if (result == MTLK_ERR_OK)
      mtlk_aocs_indicate_event(&nic->slow_ctx->aocs, MTLK_AOCS_EVENT_INITIAL_SELECTED,
        (void *)&aocs_data);
    /* restore all after AP scan */
    nic->slow_ctx->frequency_band_cfg = ap_scan_band_cfg;
    /* spectrum may change */
    SpectrumMode = nic->slow_ctx->spectrum_mode;
    /* after AOCS initial scan we must reload progmodel */
    if (mtlk_progmodel_load(nic->slow_ctx->hw_cfg.txmm, nic, nic->slow_ctx->frequency_band_cfg,
                                   1, nic->slow_ctx->spectrum_mode) != 0) {
      ELOG("Error while downloading progmodel files");
      result = MTLK_ERR_UNKNOWN;
      goto FINISH;
    }
    
    mtlk_mib_update_pm_related_mibs(nic, &nic->slow_ctx->pm_params);
    mtlk_set_pm_related_params(nic->slow_ctx->hw_cfg.txmm, &nic->slow_ctx->pm_params);
    /* ----------------------------------- */
    /* restore MIB values in the MAC as they were before the AP scanning */
    if (mtlk_set_mib_values(nic) != 0) {
      result = MTLK_ERR_UNKNOWN;
      goto FINISH;
    }
    /* now check AOCS result - here all state is already restored */
    if (result != MTLK_ERR_OK) {
      ELOG("aocs did not find available channel");
      result = MTLK_ERR_AOCS_FAILED;
      goto FINISH;
    }
    /* update channel now */
    channel = aocs_data.channel;
    nic->slow_ctx->scan.last_channel = channel;
    MTLK_ASSERT(mtlk_aocs_get_cur_channel(&nic->slow_ctx->aocs) == channel);
  }
  /* set channel */
  areq->sFrequencyElement.u16Channel = cpu_to_le16(channel);

  u8SwitchMode = mtlk_get_chnl_switch_mode(SpectrumMode, nic->slow_ctx->bonding, 0);
 
  /*get data from Regulatory table:
    Availability Check Time,
    Scan Type
  */
  ILOG2(GID_CORE, "SpectrumMode = %d", SpectrumMode);
  {
    mtlk_get_channel_data_t params;
  
    params.reg_domain = country_code_to_domain(nic->slow_ctx->cfg.country_code);
    params.is_ht = nic->slow_ctx->is_ht_cur;
    params.ap = nic->ap;
    params.spectrum_mode = SpectrumMode;
    params.bonding = nic->slow_ctx->bonding;
    params.channel = channel;
    mtlk_get_channel_data(&params, &cs_cfg_s, NULL, NULL);
  }
  areq->sFrequencyElement.u16ChannelAvailabilityCheckTime = cpu_to_le16(cs_cfg_s.u16ChannelAvailabilityCheckTime);
  areq->sFrequencyElement.u8ScanType = cs_cfg_s.u8ScanType;
  areq->sFrequencyElement.u8SwitchMode = u8SwitchMode;
  areq->sFrequencyElement.u8ChannelSwitchCount = 0; /*don't care*/
  areq->sFrequencyElement.u8SmRequired = cs_cfg_s.u8SmRequired;
  
  /*TODO- add SmRequired to 11d table !!*/
  if (nic->slow_ctx->dot11h.cfg._11h_radar_detect == 0)
    areq->sFrequencyElement.u8SmRequired = 0; /*no 11h support (by proc)*/

  ILOG1(GID_CORE, "activating (mode:%s, essid:\"%s\", chan:%d, bssid %Y)...",
     bss_type_str, nic->slow_ctx->essid, channel, nic->slow_ctx->bssid);

  ILOG2(GID_CORE, "send_activate:  channel=%d Type=%d SmReq=%d Mode=%d CheckTime=%d",
    (int)le16_to_cpu(areq->sFrequencyElement.u16Channel),
    (int)areq->sFrequencyElement.u8ScanType,
    areq->sFrequencyElement.u8SmRequired,
    (int)areq->sFrequencyElement.u8SwitchMode,
    (int)le16_to_cpu(areq->sFrequencyElement.u16ChannelAvailabilityCheckTime));
  
  /*********************** END NEW **********************************/
  
  /* warlock: Fill frequency element */
  //mtlk_fill_freq_element(&areq->sFrequencyElement, channel, nic->slow_ctx->scan_data.freq, (SpectrumMode == 1) ? 40 : 20, u8RegulatoryDomain);
  pl = mtlk_calc_tx_power_lim_wrapper(HANDLE_T(nic), 0, channel);
  areq->sFrequencyElement.i16nCbTransmitPowerLimit = HOST_TO_MAC16(pl);
  if (SpectrumMode)
    pl = mtlk_calc_tx_power_lim_wrapper(HANDLE_T(nic), 1, channel);
  areq->sFrequencyElement.i16CbTransmitPowerLimit = HOST_TO_MAC16(pl);
  ag = mtlk_get_antenna_gain(&nic->slow_ctx->tx_limits, channel);
  areq->sFrequencyElement.i16AntennaGain = HOST_TO_MAC16(ag);
  ILOG2(GID_CORE, "Power limits: cb=%d, ncb=%d, ag=%d", 
    MAC_TO_HOST16(areq->sFrequencyElement.i16CbTransmitPowerLimit),
    MAC_TO_HOST16(areq->sFrequencyElement.i16nCbTransmitPowerLimit),
    MAC_TO_HOST16(areq->sFrequencyElement.i16AntennaGain));
  
  set_80211d_mibs(nic, SpectrumMode);

  /* Set TPC calibration MIBs */
  mtlk_reload_tpc_wrapper(channel, HANDLE_T(nic));

  ASSERT(sizeof(areq->sActivate.sRSNie) == sizeof(nic->slow_ctx->rsnie));
  memcpy(&areq->sActivate.sRSNie, &nic->slow_ctx->rsnie, sizeof(areq->sActivate.sRSNie));

  if (mtlk_core_set_net_state(nic, NET_STATE_ACTIVATING) != MTLK_ERR_OK) {
    result = MTLK_ERR_NOT_READY;
    goto FINISH;
  }

  mtlk_osal_event_reset(&nic->slow_ctx->connect_event);

  DUMP3(areq, sizeof(UMI_ACTIVATE), "dump of UMI_ACTIVATE:");

  result = mtlk_txmm_msg_send_blocked(&activate_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (result != MTLK_ERR_OK) {
    ELOG("Cannot send activate request due to TXMM err#%d", result);
    goto FINISH;
  }

  if (areq->sActivate.u16Status != UMI_OK && areq->sActivate.u16Status != UMI_ALREADY_ENABLED)
  {
    WLOG_D("Activate request failed with code %d", areq->sActivate.u16Status);
    result = MTLK_ERR_UNKNOWN;
    goto FINISH;
  }

  /* now wait and handle connection event if any */
  result = mtlk_osal_event_wait(&nic->slow_ctx->connect_event, CONNECT_TIMEOUT);
  if (result == MTLK_ERR_OK) {
    if (mtlk_core_get_net_state(nic) != NET_STATE_CONNECTED) {
      WLOG("MAC failed to connect (net state=%d)", mtlk_core_get_net_state(nic));
      result = MTLK_ERR_UNKNOWN; /* MAC failed to connect */
    }
    else if (nic->ap) {
      result = mtlk_aocs_start_watchdog(&nic->slow_ctx->aocs);
      if (result != MTLK_ERR_OK) {
        ELOG("Can't start AOCS watchdog: %d", result);
        mtlk_core_set_net_state(nic, NET_STATE_HALTED);
        goto CLEANUP;
      }
    }
  } else if (result == MTLK_ERR_TIMEOUT) {
    /* MAC is dead? Either fix MAC or increase timeout */
    ELOG("Timeout reached while waiting for %s event",
          nic->ap ? "BSS creation" : "connection");
    mtlk_core_set_net_state(nic, NET_STATE_HALTED);
    goto CLEANUP;
  } else {
    /* make sure we cover all cases */
    ELOG("Unexpected result: %d", result);
    mtlk_core_set_net_state(nic, NET_STATE_HALTED);
    ASSERT(FALSE);
  }

FINISH:
  if (result != MTLK_ERR_OK &&
      mtlk_core_get_net_state(nic) != NET_STATE_READY)
      mtlk_core_set_net_state(nic, NET_STATE_READY);

CLEANUP:
  mtlk_txmm_msg_cleanup(&activate_msg);
  if (result != MTLK_ERR_OK && aocs_started)
    mtlk_aocs_stop(&nic->slow_ctx->aocs);

  return result;
}

static int
mtlk_iface_open (struct net_device *dev)
{
  struct nic *nic = netdev_priv(dev);

  ILOG1(GID_CORE, "%s: open interface", dev->name);

  if (   (mtlk_core_get_net_state(nic) != NET_STATE_READY)
      || mtlk_core_is_stopping(nic)) {
    ELOG("%s: Failed to open - inappropriate state (%s)", 
      nic->ndev->name, mtlk_net_state_to_string(mtlk_core_get_net_state(nic)));
    return -EAGAIN;
  }
  else if (mtlk_scan_is_running(&nic->slow_ctx->scan)) {
    ELOG("%s: Failed to open - scan is running", 
      nic->ndev->name);
    return -EAGAIN;
  }

  if (nic->ap) {
    mtlk_get_channel_data_t param;

    if (!nic->slow_ctx->cfg.country_code) {
      ELOG("%s: Failed to open - Country not specified", nic->ndev->name);
      return -EAGAIN;
    }

    param.reg_domain = country_code_to_domain(nic->slow_ctx->cfg.country_code);
    param.is_ht = nic->slow_ctx->is_ht_cfg;
    param.ap = nic->ap;
    param.bonding = nic->slow_ctx->bonding;
    param.spectrum_mode = nic->slow_ctx->spectrum_mode;
    param.frequency_band = nic->slow_ctx->frequency_band_cfg;
    param.disable_sm_channels = mtlk_core_is_sm_channels_disabled(nic);

    if (   (0 != nic->slow_ctx->channel)
        && (MTLK_ERR_OK != mtlk_check_channel(&param, nic->slow_ctx->channel)) ) {
      ELOG("%s: Channel (%i) is not supported in current configuration.", nic->ndev->name, nic->slow_ctx->channel);
      mtlk_core_configuration_dump(nic);
      return -EAGAIN;
    }

    if (   (nic->slow_ctx->cfg.basic_rate_set == CFG_BASIC_RATE_SET_LEGACY)
        && (nic->slow_ctx->frequency_band_cfg != MTLK_HW_BAND_2_4_GHZ)) {
      ILOG1(GID_CORE, "Forcing BasicRateSet to `default'");
      nic->slow_ctx->cfg.basic_rate_set = CFG_BASIC_RATE_SET_DEFAULT;
    }

  }

  mtlk_update_mib_sysfs(nic);
  mtlk_set_mib_values(nic);
  addba_reconfigure(nic);

  /* Disable OS TX queue until link establishment */
#if MT_FLCTRL_ENABLED 
  mtlk_flctrl_stop(&nic->flctrl, nic->flctrl_id);
#else

  if (nic->pack_sched_enabled)
    nic->tx_prohibited = 1;
  else netif_stop_queue(dev);
#endif

  if (mtlk_send_activate(nic) != MTLK_ERR_OK && nic->ap)
    return -EAGAIN;
  /* interface is up - start timers */
  mod_timer(&nic->slow_ctx->mtlk_timer, jiffies);
  start_stat_poll_timer(nic);
  
  return 0;
}

static int
mtlk_iface_stop (struct net_device *dev)
{
  struct nic *nic = netdev_priv(dev);
  int net_state = mtlk_core_get_net_state(nic);
  int res = -EAGAIN;

  ILOG1(GID_DISCONNECT, "%s: stop interface", dev->name);

  spin_lock_bh(&nic->net_state_lock);
  nic->is_iface_stopping = TRUE;
  spin_unlock_bh(&nic->net_state_lock);

  scan_terminate_and_wait_completion(&nic->slow_ctx->scan);

  if ((net_state == NET_STATE_CONNECTED) ||
      (net_state == NET_STATE_HALTED)) /* for cleanup after exception */ {

    /* disconnect STA(s) */
    reset_security_stuff(nic);
    res = send_disconnect_req(nic, NULL, TRUE);
  }

  if (nic->ap) {
    mtlk_aocs_stop_watchdog(&nic->slow_ctx->aocs);
    mtlk_aocs_stop(&nic->slow_ctx->aocs);
  }

  del_timer_sync(&nic->slow_ctx->mtlk_timer);

  stop_stat_poll_timer(nic);

  /* Clearing cache */
  mtlk_cache_clear(&nic->slow_ctx->cache);

  spin_lock_bh(&nic->net_state_lock);
  nic->is_iface_stopping = FALSE;
  spin_unlock_bh(&nic->net_state_lock);

  ILOG1("%s: interface is stopped", dev->name);

  return res;
}


int
mtlk_core_send_mac_addr_tohw (struct nic *nic, const char * mac)
{
  mtlk_txmm_t *txmm = nic->slow_ctx->hw_cfg.txmm;
  MIB_VALUE uValue;

  /* Try to send value to the MAC */
  memset(&uValue, 0, sizeof(uValue));
  memcpy(uValue.au8ListOfu8.au8Elements, mac, ETH_ALEN);
  if (mtlk_set_mib_value_raw(txmm, MIB_IEEE_ADDRESS, &uValue) != MTLK_ERR_OK) {
    ELOG("Can't set MIB_IEEE_ADDRESS");
    return -EFAULT;
  }

  /* Save new HW MAC address */
  memcpy(nic->mac_addr, mac, ETH_ALEN);

  return 0;
}


static int
mtlk_set_mac_addr (struct net_device *dev, void *p)
{
  struct nic *nic = netdev_priv(dev);
  struct sockaddr *addr = p;

  /* Allow to set MAC address only if !IFF_UP */
  if (dev->flags & IFF_UP) {
    ILOG2(GID_CORE, "%s: Can't set MAC address with IFF_UP set", dev->name);
    return -EBUSY;
  }
  
  if (!mtlk_osal_is_valid_ether_addr(addr->sa_data)) {
    WLOG("%s: Invalid MAC address (%Y)!", dev->name, addr->sa_data);
    return -EINVAL;
  }

  if (mtlk_core_send_mac_addr_tohw(nic, addr->sa_data)) {
    return -EFAULT;
  }

  /* Set interface's MAC address */
  memcpy(dev->dev_addr, addr->sa_data, ETH_ALEN);

  ILOG1(GID_CORE, "%s: New HW and device MAC address: %Y", dev->name,
     addr->sa_data);

  return 0;
}

void __MTLK_IFUNC
mtlk_init_iw_qual(struct iw_quality *pqual, uint8 mac_rssi, uint8 mac_noise)
{
  int rssi_dbm = (int)mac_rssi - 256;
  int noise_dbm = (int)mac_noise - 256;

  pqual->updated = IW_QUAL_ALL_UPDATED | IW_QUAL_DBM;
  pqual->qual = mtlk_core_calc_signal_strength(rssi_dbm);
  pqual->level = (uint8) (rssi_dbm + 256); // dBm + 256
  pqual->noise = (uint8) (noise_dbm + 256); // dBm + 256
}

static struct net_device_stats *
mtlk_linux_get_stats (struct net_device *dev)
{
  struct nic *nic = netdev_priv(dev);
  sta_entry *sta = nic->slow_ctx->stadb.connected_stations;
  unsigned long rx_dropped = 0;
  int i;

  for (i = 0; i < STA_MAX_STATIONS; ++i, ++sta)
    if (sta->state != PEER_UNUSED)
      rx_dropped += sta->stats.rx_dropped;

  read_lock_bh(&nic->slow_ctx->stat_lock);

  /* RX error counters */
  nic->stats.rx_dropped = rx_dropped;
  nic->stats.rx_fifo_errors = nic->slow_ctx->mac_stat[STAT_OUT_OF_RX_MSDUS];

  /* TX error counters */
  nic->stats.collisions = nic->slow_ctx->mac_stat[STAT_TX_RETRY];
  nic->stats.tx_dropped = nic->slow_ctx->mac_stat[STAT_TX_FAIL];

  read_unlock_bh(&nic->slow_ctx->stat_lock);

  return &nic->stats;
}

int
mtlk_core_gen_dataex_get_connection_stats (struct nic *nic,
    WE_GEN_DATAEX_REQUEST *preq, WE_GEN_DATAEX_RESPONSE *presp, void *usp_data)
{
  WE_GEN_DATAEX_CONNECTION_STATUS dataex_conn_status;
  int nof_connected, sta_id;
  void *out;

  presp->ver = WE_GEN_DATAEX_PROTO_VER;
  presp->status = WE_GEN_DATAEX_SUCCESS;
  presp->datalen = sizeof(WE_GEN_DATAEX_CONNECTION_STATUS);

  if (preq->datalen < presp->datalen)
    return -ENOMEM;

  memset(&dataex_conn_status, 0, sizeof(WE_GEN_DATAEX_CONNECTION_STATUS));
  dataex_conn_status.u8GlobalNoise = nic->slow_ctx->noise;
  dataex_conn_status.u8ChannelLoad = nic->slow_ctx->channel_load;
  dataex_conn_status.u16NumDisconnections = nic->pstats.num_disconnects;

  nof_connected = 0;
  out = usp_data + sizeof(WE_GEN_DATAEX_CONNECTION_STATUS);

  for (sta_id = 0; sta_id < STA_MAX_STATIONS; sta_id++) {
    WE_GEN_DATAEX_DEVICE_STATUS dataex_dev_status;
    sta_entry *sta;

    sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, sta_id);
    if (sta->state == PEER_UNUSED)
      continue;

    memcpy(&dataex_dev_status.sMacAdd, sta->mac, ETH_ALEN);

    dataex_dev_status.u8NetworkMode = sta->net_mode;
    dataex_dev_status.u16TxRate = sta->tx_rate;

    dataex_dev_status.au8RSSI[0] = sta->rssi[0];
    dataex_dev_status.au8RSSI[1] = sta->rssi[1];
    dataex_dev_status.au8RSSI[2] = sta->rssi[2];

    dataex_dev_status.u32RxCount = sta->stats.rx_packets;
    dataex_dev_status.u32TxCount = sta->stats.tx_packets;
    dataex_dev_status.u32TxDropped = sta->stats.tx_failed;

    presp->datalen += sizeof(WE_GEN_DATAEX_DEVICE_STATUS);
    if (preq->datalen < presp->datalen)
      return -ENOMEM;

    if (copy_to_user(out, &dataex_dev_status, sizeof(WE_GEN_DATAEX_DEVICE_STATUS)))
      return -EFAULT;
    out += sizeof(WE_GEN_DATAEX_DEVICE_STATUS);

    nof_connected++;
  }

  dataex_conn_status.u8NumOfConnections = nof_connected;
  if (copy_to_user(usp_data, &dataex_conn_status, sizeof(WE_GEN_DATAEX_CONNECTION_STATUS)))
    return -EFAULT;

  return 0;
}

int
mtlk_core_gen_dataex_get_status (struct nic *nic,
    WE_GEN_DATAEX_REQUEST *preq, WE_GEN_DATAEX_RESPONSE *presp, void *usp_data)
{
  int res = 0;
  int i;
  sta_entry *sta;
  WE_GEN_DATAEX_STATUS status;

  presp->ver = WE_GEN_DATAEX_PROTO_VER;

  if (preq->datalen < sizeof(status)) {
    presp->status = WE_GEN_DATAEX_DATABUF_TOO_SMALL;
    presp->datalen = sizeof(status);
    goto end;
  }

  presp->status = WE_GEN_DATAEX_SUCCESS;
  presp->datalen = sizeof(status);

  memset(&status, 0, sizeof(status));

  status.security_on = 0;
  status.wep_enabled = 0;
  sta = nic->slow_ctx->stadb.connected_stations;
  for (i = 0; i < STA_MAX_STATIONS; i++) {
    if (sta[i].state == PEER_UNUSED)
      continue;
    // Check global WEP enabled flag only if some STA connected
    if ((sta[i].cipher != IW_ENCODE_ALG_NONE) || nic->slow_ctx->wep_enabled) {
      status.security_on = 1;
      if (nic->slow_ctx->wep_enabled)
        status.wep_enabled = 1;
      break;
    }
  }

  status.scan_started = mtlk_scan_is_running(&nic->slow_ctx->scan);
  if (nic->slow_ctx->scan.initialized) {
    status.frequency_band = nic->slow_ctx->frequency_band_cur;
  } else
    status.frequency_band = MTLK_HW_BAND_NONE;
  status.link_up = (mtlk_core_get_net_state(nic) == NET_STATE_CONNECTED) ? 1 : 0;

  if (copy_to_user(usp_data, &status, sizeof(status)) != 0) {
    res = -EINVAL;
    goto end;
  }

end:
  return res;
}

int
mtlk_core_gen_dataex_send_mac_leds (struct nic *nic,
  WE_GEN_DATAEX_REQUEST *req, WE_GEN_DATAEX_RESPONSE *resp, void *usp_data)
{
  int res = 0;
  WE_GEN_DATAEX_LED leds_status;

  resp->ver = WE_GEN_DATAEX_PROTO_VER;
  if (req->datalen < sizeof(leds_status)) {
    resp->status = WE_GEN_DATAEX_DATABUF_TOO_SMALL;
    resp->datalen = sizeof(leds_status);
    goto end;
  }

  memset(&leds_status, 0, sizeof(leds_status));

  if (copy_from_user(&leds_status, usp_data, sizeof(leds_status)) != 0) {
    ELOG("error copy from user, goto end");
    res = -EINVAL;
    goto end;
  }

  ILOG2(GID_CORE, "u8BasebLed = %d, u8LedStatus = %d", leds_status.u8BasebLed,
    leds_status.u8LedStatus);

  resp->status = WE_GEN_DATAEX_SUCCESS;
  resp->datalen = sizeof(leds_status);

  if (nic->slow_ctx->scan.initialized) {
    mtlk_txmm_msg_t   man_msg;
    mtlk_txmm_data_t *man_entry = NULL;
    UMI_SET_LED      *pdata;

    ILOG2(GID_CORE, "can write to MAC");
    //TODO send message
    man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, nic->slow_ctx->hw_cfg.txmm, NULL);
    if (!man_entry) {
      ELOG("No MM entry to set BCMC_RATE");
      res = -EINVAL;
      goto end;
    }
    pdata = (UMI_SET_LED *)man_entry->payload;
    man_entry->id           = UM_MAN_SET_LED_REQ;
    man_entry->payload_size = sizeof(leds_status);
    pdata->u8BasebLed = leds_status.u8BasebLed;
    pdata->u8LedStatus = leds_status.u8LedStatus;
    res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);

    mtlk_txmm_msg_cleanup(&man_msg);
  }

end:
  return res;
}

uint32
mtlk_core_get_available_bitrates (struct nic *nic)
{
  uint8 net_mode;
  uint32 mask = 0;

  /* Get all needed MIBs */
  if (nic->net_state == NET_STATE_CONNECTED)
    net_mode =  nic->slow_ctx->net_mode_cur;
  else
    net_mode = nic->slow_ctx->net_mode_cfg;
  mask = get_operate_rate_set(net_mode);
  ILOG3(GID_CORE, "Configuration mask: 0x%08x", mask);

  return mask;
}

int
mtlk_core_set_gen_ie (struct nic *nic, u8 *ie, u16 ie_len, u8 ie_type)
{
  int res = 0;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry;
  UMI_GENERIC_IE   *ie_req;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, 
                                                 nic->slow_ctx->hw_cfg.txmm,
                                                 NULL);
  if (!man_entry) {
    ELOG("No man entry available");
    res = -EAGAIN;
    goto end;
  }

  man_entry->id = UM_MAN_SET_IE_REQ;
  man_entry->payload_size = sizeof(UMI_GENERIC_IE);
  ie_req = (UMI_GENERIC_IE*) man_entry->payload;
  memset(ie_req, 0, sizeof(*ie_req));
  ie_req->u8Type = ie_type;
  if (ie_len > sizeof(ie_req->au8IE)) {
    ELOG("invalid IE length (%i > %i)",
        ie_len, (int)sizeof(ie_req->au8IE));
    res = -EINVAL;
    goto end;
  }
  if (ie && ie_len)
    memcpy(ie_req->au8IE, ie, ie_len);
  ie_req->u16Length = cpu_to_le16(ie_len);
  
  if (mtlk_txmm_msg_send_blocked(&man_msg, 
                                 MTLK_MM_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
    ELOG("cannot set IE to MAC");
    res = -EINVAL;
  }
  
end:
  if (man_entry)
    mtlk_txmm_msg_cleanup(&man_msg);
    
  return res;
} 

/* Driver support only 40 bit and 104 bit length WEP keys.
 * Also, according to IEEE standard packet transmission
 * with zero-filled WEP key is not supported.
 */ 
int
mtlk_core_validate_wep_key (uint8 *key, size_t length) {
  int i;
  ASSERT(key);
  
  /* Check key length */
  if ((length != MIB_WEP_KEY_WEP1_LENGTH) && /* 40 bit */
      (length != MIB_WEP_KEY_WEP2_LENGTH)) { /* 104 bit */
    return MTLK_ERR_PARAMS;
  }

  /* Validate key's value */
  for (i = 0; i < length; i++)
    if (key[i] == '\0')
      return MTLK_ERR_PARAMS;
    
  return MTLK_ERR_OK;
} 

void 
mtlk_core_sw_reset_enable_set (int32 value, struct nic *nic)
{
  if (value > 1 || value < 0)
    ILOG1(GID_CORE, "error setting SW Watchdog");
  else
    nic->slow_ctx->mac_soft_reset_enable = value;
}

int 
mtlk_core_sw_reset_enable_get (struct nic *nic)
{
  return nic->slow_ctx->mac_soft_reset_enable;
}

static int
mtlk_core_is_band_supported(const struct nic *nic, unsigned band)
{
  if (band == MTLK_HW_BAND_BOTH && nic->ap) // AP can't be dual-band
    return MTLK_ERR_UNKNOWN;

  return mtlk_eeprom_is_band_supported(&nic->slow_ctx->ee_data, band);
}

int __MTLK_IFUNC
mtlk_core_set_network_mode(mtlk_core_t* nic, uint8 net_mode)
{
  if (mtlk_core_is_band_supported(nic, net_mode_to_band(net_mode)) != MTLK_ERR_OK) {
    if (net_mode_to_band(net_mode) == MTLK_HW_BAND_BOTH) {
      /*
       * Just in case of single-band hardware
       * continue to use `default' frequency band,
       * which is de facto correct.
       */
      ELOG("dualband isn't supported");
      return MTLK_ERR_OK;
    } else {
      ELOG("%s band isn't supported",
              mtlk_eeprom_band_to_string(net_mode_to_band(net_mode)));
      return MTLK_ERR_NOT_SUPPORTED;
    }
  }
  if (is_ht_net_mode(net_mode) && nic->slow_ctx->wep_enabled) {
    if (nic->ap) {
      ELOG("AP: %s network mode isn't supported for WEP",
           net_mode_to_string(net_mode));
      return MTLK_ERR_NOT_SUPPORTED;
    }
    else if (!is_mixed_net_mode(net_mode)) {
      ELOG("STA: %s network mode isn't supported for WEP",
           net_mode_to_string(net_mode));
      return MTLK_ERR_NOT_SUPPORTED;
    }
  }
  ILOG1(GID_IOCTL, "Set Network Mode to %s", net_mode_to_string(net_mode));
  nic->slow_ctx->net_mode_cfg = /*walk through */
  nic->slow_ctx->net_mode_cur = net_mode;
  nic->slow_ctx->frequency_band_cfg = /*walk through */
  nic->slow_ctx->frequency_band_cur = net_mode_to_band(net_mode);
  nic->slow_ctx->is_ht_cfg = /*walk through */
  nic->slow_ctx->is_ht_cur = is_ht_net_mode(net_mode);
  if (!is_ht_net_mode(net_mode)) {
    WLOG("20MHz spectrum forced for %s", net_mode_to_string(net_mode));
    mtlk_set_dec_mib_value(PRM_SPECTRUM_MODE, SPECTRUM_20MHZ, nic);
    nic->slow_ctx->spectrum_mode = SPECTRUM_20MHZ;
  }

  /* The set of supported bands may be changed by this request.           */
  /* Scan cache to be cleared to throw out BSS from unsupported now bands */
  mtlk_cache_clear(&nic->slow_ctx->cache);
  return MTLK_ERR_OK;
}

int
mtlk_handle_bar (mtlk_handle_t context, uint8 *ta, uint8 tid, uint16 ssn)
{
  struct nic *nic = (struct nic *)context;
  int sta_id, res;

  nic->pstats.bars_cnt++;

  if (tid >= NTS_TIDS) {
    ELOG("Received BAR with wrong TID (%u)", tid);
    return -1;
  }

  sta_id = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, ta);
  if (sta_id == -1) {
    ELOG("Received BAR from unknown peer %Y", ta);
    return -1;
  }

  ILOG2(GID_CORE, "Received BAR from %Y TID %u", ta, tid);

  res = mtlk_rod_process_bar(&mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, sta_id)->rod_queue[tid], ssn);

  if(res != 0) {
    ELOG("Failed to process BAR (STA %Y TID %u)", ta, tid);
  }
  
  return res;
}

#ifdef MTCFG_RF_MANAGEMENT_MTLK
mtlk_rf_mgmt_t *
mtlk_get_rf_mgmt (mtlk_handle_t context)
{
  struct nic *nic = (struct nic *)context;

  return &nic->rf_mgmt;
}
#endif

static void
mtlk_update_prev_sub_skb(struct sk_buff * skb, struct sk_buff **sub_skb_stored, struct sk_buff **sub_skb_fisrt)
{
  struct sk_buff *sub_skb = *sub_skb_stored;
  struct skb_private *sub_skb_priv;

  if(sub_skb)
  {
    sub_skb_priv = (struct skb_private*)sub_skb->cb;
    sub_skb_priv->sub_frame = skb;
  }
  else
  {
    *sub_skb_fisrt = skb;
  }

  *sub_skb_stored = skb;
}

static struct sk_buff *
mtlk_parse_a_msdu(struct sk_buff *skb, int a_msdu_len)
{
  int msdu_len;
  int subpacket_len, pad;
  struct ethhdr *ether_header;
  struct sk_buff *sub_skb;
  struct sk_buff *sub_skb_stored = NULL;
  struct sk_buff *sub_skb_first = skb;
  const int AMSDU_MAX_MSDU_SIZE = 2304; /* See IEEE Std 802.11-2012, p. 417 */


  ILOG5(GID_CORE, "Parsing A-MSDU: length %d", a_msdu_len);

  while (a_msdu_len) {
    if(a_msdu_len < sizeof(struct ethhdr))
    {
      WLOG0("Invalid A-MSDU: subpacket header too short: %d", a_msdu_len);
      goto on_error;
    }

    ether_header = (struct ethhdr *)skb->data;
    msdu_len = ntohs(ether_header->h_proto);

    if(msdu_len > AMSDU_MAX_MSDU_SIZE)
    {
      WLOG0("Invalid A-MSDU: MSDU length > %d, a_msdu_len = %d",
            AMSDU_MAX_MSDU_SIZE, a_msdu_len);
      goto on_error;
    }

    subpacket_len = msdu_len + sizeof(struct ethhdr);

    ILOG5(GID_CORE, "A-MSDU subpacket: length = %d", subpacket_len);

    a_msdu_len -= subpacket_len;

    if (a_msdu_len < 0) {
      WLOG0("Invalid A-MSDU: subpacket too long (%d > %d)",
            subpacket_len, a_msdu_len + subpacket_len);
      goto on_error;
    } else if (a_msdu_len == 0) {
      sub_skb = skb;
      pad = 0;
    } else {
      sub_skb = skb_clone(skb, GFP_ATOMIC);
      if (!sub_skb) {
        ELOG0("Cannot clone A-MSDU (len=%d)", a_msdu_len);
        goto on_error;
      }
      /* skip padding */
      pad = (4 - (subpacket_len & 0x03)) & 0x03;
      a_msdu_len -= pad;
    }
    /* cut everything after data */
    skb_trim(sub_skb, subpacket_len);
    /* for A-MSDU case we need to skip LLC/SNAP header */
    memmove(sub_skb->data + sizeof(mtlk_snap_hdr_t) + sizeof(mtlk_llc_hdr_t),
      ether_header, ETH_ALEN * 2);
    skb_pull(sub_skb, sizeof(mtlk_snap_hdr_t) + sizeof(mtlk_llc_hdr_t));
    analyze_rx_packet(sub_skb);
    mtlk_mc_parse(sub_skb);
    mtlk_update_prev_sub_skb(sub_skb, &sub_skb_stored, &sub_skb_first);
    skb_pull(skb, subpacket_len + pad);
  }

  return sub_skb_first;

on_error:

  if(sub_skb_first != skb)
  {
    /* Free clones */
    mtlk_df_skb_free_sub_frames(sub_skb_first);
  }

  /*WLOG0("Dump of received packet: skb->len == %d", skb->len);
  mtlk_aux_print_hex(skb->data, skb->len + 256);*/

  if(sub_skb_stored != skb)
  {
    /* Free original SKB */
    dev_kfree_skb(skb);
  }

  return NULL;
}

/*S 
 * Definitions and macros below are used only for the packet's header transformation
 * For more information, please see following documents:
 *   - IEEE 802.1H standard
 *   - IETF RFC 1042
 *   - IEEE 802.11n standard draft 5 Annex M
 * */

#define _8021H_LLC_HI4BYTES             0xAAAA0300
#define _8021H_LLC_LO2BYTES_CONVERT     0x0000
#define RFC1042_LLC_LO2BYTES_TUNNEL     0x00F8

/* Default ISO/IEC conversion
 * we need to keep full LLC header and store packet length in the T/L subfield */
#define _8021H_CONVERT(ether_header, skb, data_offset) \
  data_offset -= sizeof(struct ethhdr); \
  ether_header = (struct ethhdr *)(skb->data + data_offset); \
  ether_header->h_proto = htons(skb->len - data_offset - sizeof(struct ethhdr))

/* 802.1H encapsulation
 * we need to remove LLC header except the 'type' field */
#define _8021H_DECAPSULATE(ether_header, skb, data_offset) \
  data_offset -= sizeof(struct ethhdr) - (sizeof(mtlk_snap_hdr_t) + sizeof(mtlk_llc_hdr_t)); \
  ether_header = (struct ethhdr *)(skb->data + data_offset)

static int
handle_rx_ind (mtlk_core_t *nic, struct sk_buff *skb, uint16 msdulen,
               const MAC_RX_ADDITIONAL_INFO_T *mac_rx_info)
{
  int res = MTLK_ERR_OK; /* Do not free skb */
  int off;
  struct skb_private *skb_priv;
  int sta_id;
  unsigned char fromDS, toDS;
  uint16 seq = 0, frame_ctl;
  uint16 frame_subtype;
  uint16 priority = 0, qos = 0;
  unsigned int a_msdu = 0;
  unsigned char *cp, *addr1, *addr2;
  reordering_queue *rod_queue = NULL;
  sta_entry *sta = NULL;
  uint8 key_idx;

  ILOG4(GID_CORE, "Rx indication");


  // Set the size of the skbuff data
  
  if (skb_tail_pointer(skb) + msdulen > skb_end_pointer(skb))
    ELOG("skb->tail + msdulen > skb->end ->> %p + %d > %p",
          skb_tail_pointer(skb),
          msdulen,
          skb_tail_pointer(skb));
  skb_put(skb, msdulen);

  // Get pointer to private area
  skb_priv = (struct skb_private*)skb->cb;
  memset(skb_priv, 0, sizeof(struct skb_private));

/*


802.11n data frame from AP:

        |----------------------------------------------------------------|
 Bytes  |  2   |  2    |  6  |  6  |  6  |  2  | 6?  | 2?  | 0..2312 | 4 |
        |------|-------|-----|-----|-----|-----|-----|-----|---------|---|
 Descr. | Ctl  |Dur/ID |Addr1|Addr2|Addr3| Seq |Addr4| QoS |  Frame  |fcs|
        |      |       |     |     |     | Ctl |     | Ctl |  data   |   |
        |----------------------------------------------------------------|
Total: 28-2346 bytes

Existance of Addr4 in frame is optional and depends on To_DS From_DS flags.
Existance of QoS_Ctl is also optional and depends on Ctl flags.
(802.11n-D1.0 describes also HT Control (0 or 4 bytes) field after QoS_Ctl
but we don't support this for now.)

Interpretation of Addr1/2/3/4 depends on To_DS From_DS flags:

To DS From DS   Addr1   Addr2   Addr3   Addr4
---------------------------------------------
0       0       DA      SA      BSSID   N/A
0       1       DA      BSSID   SA      N/A
1       0       BSSID   SA      DA      N/A
1       1       RA      TA      DA      SA


frame data begins with 8 bytes of LLC/SNAP:

        |-----------------------------------|
 Bytes  |  1   |   1  |  1   |    3   |  2  |
        |-----------------------------------|
 Descr. |        LLC         |     SNAP     |
        |-----------------------------------+
        | DSAP | SSAP | Ctrl |   OUI  |  T  |
        |-----------------------------------|
        |  AA  |  AA  |  03  | 000000 |     |
        |-----------------------------------|

From 802.11 data frame that we receive from MAC we are making
Ethernet DIX (II) frame.

Ethernet DIX (II) frame format:

        |------------------------------------------------------|
 Bytes  |  6  |  6  | 2 |         46 - 1500               |  4 |
        |------------------------------------------------------|
 Descr. | DA  | SA  | T |          Data                   | FCS|
        |------------------------------------------------------|

So we overwrite 6 bytes of LLC/SNAP with SA.

*/

  ILOG4(GID_CORE, "Munging IEEE 802.11 header to be Ethernet DIX (II), irrevesible!");
  cp = (unsigned char *) skb->data;

  DUMP4(cp, 64, "dump of recvd .11n packet");

  // Chop the last four bytes (FCS)
  skb_trim(skb, skb->len-4);

  frame_ctl = mtlk_wlan_pkt_get_frame_ctl(cp);
  addr1 = WLAN_GET_ADDR1(cp);
  addr2 = WLAN_GET_ADDR2(cp);

  ILOG4(GID_CORE, "frame control - %04x", frame_ctl);

  /*
  Excerpts from "IEEE P802.11e/D13.0, January 2005" p.p. 22-23
  Type          Subtype     Description
  -------------------------------------------------------------
  00 Management 0000        Association request
  00 Management 0001        Association response
  00 Management 0010        Reassociation request
  00 Management 0011        Reassociation response
  00 Management 0100        Probe request
  00 Management 0101        Probe response
  00 Management 0110-0111   Reserved
  00 Management 1000        Beacon
  00 Management 1001        Announcement traffic indication message (ATIM)
  00 Management 1010        Disassociation
  00 Management 1011        Authentication
  00 Management 1100        Deauthentication
  00 Management 1101        Action
  00 Management 1101-1111   Reserved
  01 Control    0000-0111   Reserved
  01 Control    1000        Block Acknowledgement Request (BlockAckReq)
  01 Control    1001        Block Acknowledgement (BlockAck)
  01 Control    1010        Power Save Poll (PS-Poll)
  01 Control    1011        Request To Send (RTS)
  01 Control    1100        Clear To Send (CTS)
  01 Control    1101        Acknowledgement (ACK)
  01 Control    1110        Contention-Free (CF)-End
  01 Control    1111        CF-End + CF-Ack
  10 Data       0000        Data
  10 Data       0001        Data + CF-Ack
  10 Data       0010        Data + CF-Poll
  10 Data       0011        Data + CF-Ack + CF-Poll
  10 Data       0100        Null function (no data)
  10 Data       0101        CF-Ack (no data)
  10 Data       0110        CF-Poll (no data)
  10 Data       0111        CF-Ack + CF-Poll (no data)
  10 Data       1000        QoS Data
  10 Data       1001        QoS Data + CF-Ack
  10 Data       1010        QoS Data + CF-Poll
  10 Data       1011        QoS Data + CF-Ack + CF-Poll
  10 Data       1100        QoS Null (no data)
  10 Data       1101        Reserved
  10 Data       1110        QoS CF-Poll (no data)
  10 Data       1111        QoS CF-Ack + CF-Poll (no data)
  11 Reserved   0000-1111   Reserved
  */

  // FIXME: ADD DEFINITIONS!!!!
  // XXX, klogg: see frame.h

  switch(WLAN_FC_GET_TYPE(frame_ctl))
  {
  case IEEE80211_FTYPE_DATA:
    nic->pstats.rx_dat_frames++;
    // Normal data
    break;
  case IEEE80211_FTYPE_MGMT:
    mtlk_process_man_frame(HANDLE_T(nic), &nic->slow_ctx->scan, &nic->slow_ctx->cache,
        &nic->slow_ctx->aocs, skb->data, skb->len, mac_rx_info);
    nic->pstats.rx_man_frames++;

    frame_subtype = (frame_ctl & FRAME_SUBTYPE_MASK) >> FRAME_SUBTYPE_SHIFT;
    ILOG4(GID_CORE, "Subtype is %d", frame_subtype);

    if ((frame_subtype == MAN_TYPE_BEACON && !nic->ap) ||
        frame_subtype == MAN_TYPE_PROBE_RES ||
        frame_subtype == MAN_TYPE_PROBE_REQ) {

      // Workaraund for WPS (wsc-1.7.0) - send channel instead of the Dur/ID field */
      *(uint16 *)(skb->data + 2) = mac_rx_info->u8Channel;

      mtlk_nl_send_brd_msg(skb->data, skb->len, GFP_ATOMIC,
                             NETLINK_SIMPLE_CONFIG_GROUP, NL_DRV_CMD_MAN_FRAME);
    }

    res = MTLK_ERR_NOT_IN_USE;
    goto end;
  case IEEE80211_FTYPE_CTL:
    mtlk_process_ctl_frame(HANDLE_T(nic), skb->data, skb->len);
    nic->pstats.rx_ctl_frames++;
    res = MTLK_ERR_NOT_IN_USE; /* Free skb */
    goto end;
  default:
    ILOG2(GID_CORE, "Unknown header type, frame_ctl %04x", frame_ctl);
    res = MTLK_ERR_NOT_IN_USE; /* Free skb */
    goto end;
  }

  ILOG4(GID_CORE, "802.11n rx TA: %Y", addr2);
  ILOG4(GID_CORE, "802.11n rx RA: %Y", addr1);

  /* Try to find source MAC of transmitter */
  sta_id = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, addr2);
  if (sta_id == -1) {
    ILOG2(GID_CORE, "SOURCE of RX packet not found!");
    res = MTLK_ERR_NOT_IN_USE; /* Free skb */
    goto end;
  }
  sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, sta_id);
  skb_priv->src_sta = sta;

  /* Peers are updated on any data packet including NULL */
  mtlk_stadb_update_sta(sta);

  seq = mtlk_wlan_pkt_get_seq(cp);
  ILOG3(GID_CORE, "seq %d", seq);

  if (WLAN_FC_IS_NULL_PKT(frame_ctl)) {
    ILOG3(GID_CORE, "Null data packet, frame ctl - 0x%04x", frame_ctl);
    res = MTLK_ERR_NOT_IN_USE; /* Free skb */
    goto end;
  }

  off = mtlk_wlan_get_hdrlen(frame_ctl);
  ILOG3(GID_CORE, "80211_hdrlen - %d", off);
  if (WLAN_FC_IS_QOS_PKT(frame_ctl)) {
    u16 qos_ctl = mtlk_wlan_pkt_get_qos_ctl(cp, off);
    priority = WLAN_QOS_GET_PRIORITY(qos_ctl);
    a_msdu = WLAN_QOS_GET_MSDU(qos_ctl);
  }

  qos = mtlk_qos_get_ac_by_tid(priority);
#ifdef MTLK_DEBUG_CHARIOT_OOO
  skb_priv->seq_qos = qos;
#endif
    
  if (nic->ap)
    mtlk_aocs_on_rx_msdu(&nic->slow_ctx->aocs, qos);

  fromDS = WLAN_FC_GET_FROMDS(frame_ctl);
  toDS   = WLAN_FC_GET_TODS(frame_ctl);
  ILOG3(GID_CORE, "FromDS %d, ToDS %d", fromDS, toDS);

  /* Check if packet was directed to us */
  if (!memcmp(addr1, nic->mac_addr, ETH_ALEN)) {
    skb_priv->flags |= SKB_DIRECTED;

    /* get reordering queue pointer and update timestamp */
    rod_queue = &mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, sta_id)->rod_queue[priority];
    mtlk_rod_queue_set_last_rx_time(rod_queue, mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp()));
  }
  
  nic->pstats.ac_rx_counter[qos]++;
  nic->pstats.sta_session_rx_packets++;

  skb_priv->rsn_bits = mac_rx_info->u8RSN;

  /* data offset should account also security info if it is there */
  off += get_rsc_buf(skb, off);
  key_idx = (skb_priv->rsc_buf[3] & 0300) >> 6;

  /* process regular MSDU */
  if (likely(!a_msdu)) {
    struct ethhdr *ether_header;
    /* Raw LLC header split into 3 parts to make processing more convenient */
    struct llc_hdr_raw_t {
      uint32 hi4bytes;
      uint16 lo2bytes;
      uint16 ether_type;
    } *llc_hdr_raw = (struct llc_hdr_raw_t *)(skb->data + off);

    if (llc_hdr_raw->hi4bytes == __constant_htonl(_8021H_LLC_HI4BYTES)) {
      switch (llc_hdr_raw->lo2bytes) {
      case __constant_htons(_8021H_LLC_LO2BYTES_CONVERT):
        switch (llc_hdr_raw->ether_type) {
        /* AppleTalk and IPX encapsulation - integration service STT (see table M.1) */
        case __constant_htons(ETH_P_AARP):
        case __constant_htons(ETH_P_IPX):
          _8021H_CONVERT(ether_header, skb, off);
          break;
        /* default encapsulation
         * TODO: make sure it will be the shortest path */
        default:
          _8021H_DECAPSULATE(ether_header, skb, off);
          break;
        }
        break;
      case __constant_htons(RFC1042_LLC_LO2BYTES_TUNNEL):
        _8021H_DECAPSULATE(ether_header, skb, off);
        break;
      default:
        _8021H_CONVERT(ether_header, skb, off);
        break;
      }
    } else {
      _8021H_CONVERT(ether_header, skb, off);
    }
    skb_pull(skb, off);
    ether_header = (struct ethhdr *)skb->data;

    /* save SRC/DST MAC adresses from 802.11 header to 802.3 header */
    mtlk_wlan_get_mac_addrs(cp, fromDS, toDS, ether_header->h_source, ether_header->h_dest);

    /* Check if packet received from WDS HOST
     * (HOST mac address is not equal to sender's MAC address) */
    if ((nic->bridge_mode == BR_MODE_WDS) &&
        memcmp(ether_header->h_source, sta->mac, ETH_ALEN) != 0) {

      /* On AP we need to update HOST's entry in database of registered
       * HOSTs behid connected STAs */
      if (nic->ap) {
        mtlk_stadb_update_host(&nic->slow_ctx->stadb, ether_header->h_source, sta_id);
      /* On STA we search if this HOST registered in STA's database of
       * connected HOSTs (that are behind this STA) */
      } else if (mtlk_stadb_find_sta(&nic->slow_ctx->stadb, ether_header->h_source) >= 0) {
          ILOG4(GID_CORE, "Packet from our own host received from AP");
          res = MTLK_ERR_NOT_IN_USE; /* Free skb */
          goto end;
      }
    }

#ifdef MTLK_DEBUG_IPERF_PAYLOAD_RX
    debug_ooo_analyze_packet(TRUE, skb, seq);
#endif

    analyze_rx_packet(skb);
    mtlk_mc_parse(skb);

    /* try to reorder packet */
    if (rod_queue &&
        (nic->ap || WLAN_FC_IS_QOS_PKT(frame_ctl))) {
      mtlk_reorder_packet(rod_queue, seq, skb);
    } else {
      mtlk_detect_replay_or_sendup(skb, nic->group_rsc[key_idx]);
    }

  /* process A-MSDU */
  } else {
    mtlk_nbuf_t *skb_first;

    skb_pull(skb, off);

    /* parse A-MSDU return first packet to send to OS */
    skb_first = mtlk_parse_a_msdu(skb, skb->len);

    if(skb_first)
    {
      /* try to reorder packet */
      if (rod_queue &&
        (nic->ap || WLAN_FC_IS_QOS_PKT(frame_ctl))) {
          mtlk_reorder_packet(rod_queue, seq, skb_first);
      } else {
        mtlk_detect_replay_or_sendup(skb_first, nic->group_rsc[key_idx]);
      }
    }
    else
    {
      /* packet is already dropped by mtlk_parse_a_msdu() */
    }

  }

end:
  return res;
}

/*****************************************************************************
**
** NAME         handleNetworkEventInd
**
** PARAMETERS   psMsg               Network Indication Message to process
**              nic           Card context
**
** RETURNS      none
**
** DESCRIPTION  This function is called from the Network thread to handle
**              Network Event indication messages received from the device
**              (called from only from msg_receive())
**
******************************************************************************/
static void
handleNetworkEventInd (struct nic *nic, void *payload)
{
  struct net_device *ndev = nic->ndev;
  mtlk_dot11h_cfg_t dot11h_cfg;
  UMI_NETWORK_EVENT *psNetwork;
  mtlk_aocs_evt_select_t switch_data;
  psNetwork = (UMI_NETWORK_EVENT *)payload;

  // Overkill to convert once and then back again. Should be fixed
  psNetwork->u16BSSstatus = le16_to_cpu(psNetwork->u16BSSstatus); 
  psNetwork->u16CFflag = le16_to_cpu(psNetwork->u16CFflag); 
  psNetwork->u16Reason = le16_to_cpu(psNetwork->u16Reason); 

  switch (psNetwork->u16BSSstatus)
    {
    case UMI_BSS_CREATED:
      ILOG1(GID_CORE, "Network created, BSSID %Y",
          psNetwork->sBSSID.au8Addr);
      //if (nic->ap)
      //	up(&nic->sema_connect);
      memcpy(nic->slow_ctx->bssid, psNetwork->sBSSID.au8Addr, ETH_ALEN);
      mtlk_core_set_net_state(nic, NET_STATE_CONNECTED);
      mtlk_osal_event_set(&nic->slow_ctx->connect_event);
      if (nic->pack_sched_enabled ? nic->tx_prohibited : 
                                    netif_queue_stopped(ndev)) {

        ILOG2(GID_CORE, "mtlk_flctrl_wake on UMI_BSS_CREATED");
#if MT_FLCTRL_ENABLED 
        mtlk_flctrl_wake (&nic->flctrl, nic->flctrl_id);
#else

        if (nic->pack_sched_enabled) {
          nic->tx_prohibited = 0;
          mtlk_sq_schedule_flush(nic);
        } else netif_wake_queue(ndev);

#endif
      }
      break;

    case UMI_BSS_CONNECTING:
      ILOG1(GID_CORE, "Connecting to network...");
      break;

    case UMI_BSS_CONNECTED:
      ILOG1(GID_CORE, "Found BSSID %Y", psNetwork->sBSSID.au8Addr);
      break;

    case UMI_BSS_FAILED:
      ILOG1(GID_CORE, "Failed to create/connect to network, reason %d (%s)",
            psNetwork->u16Reason, cr_str[psNetwork->u16Reason]);

      // AP is dead? Force user to rescan to see this BSS again
      if (psNetwork->u16Reason == UMI_BSS_JOIN_FAILED) {
        ASSERT(!nic->ap); 
        mtlk_cache_remove_bss_by_bssid(&nic->slow_ctx->cache, psNetwork->sBSSID.au8Addr);
      }

      mtlk_core_set_net_state(nic, NET_STATE_READY);
      mtlk_osal_event_set(&nic->slow_ctx->connect_event);
      break;

    case UMI_BSS_CHANNEL_SWITCH_DONE:
      ILOG1(GID_CORE, "got UMI_BSS_CHANNEL_SWITCH_DONE IND");
      nic->slow_ctx->channel = psNetwork->u16Reason >> 8;
      mtlk_dot11h_on_channel_switch_done_ind(&nic->slow_ctx->dot11h);
      break;

    case UMI_BSS_CHANNEL_PRE_SWITCH_DONE:
      ILOG1(GID_CORE, "got UM_SET_CHAN_CFM");
      mtlk_dot11h_cfm_clb(&nic->slow_ctx->dot11h);
      break;

    case UMI_BSS_CHANNEL_SWITCH_NORMAL:
      mtlk_dot11h_get_cfg(&nic->slow_ctx->dot11h, &dot11h_cfg);
      dot11h_cfg.u8SpectrumMode     =
          mtlk_aux_atol(mtlk_get_mib_value(PRM_SPECTRUM_MODE, nic));
      dot11h_cfg.u8Bonding = nic->slow_ctx->bonding;
      nic->slow_ctx->dot11h.event = MTLK_DFS_EVENT_CHANGE_CHANNEL_NORMAL; /*norm*/
      nic->slow_ctx->dot11h.set_channel = ((psNetwork->u16Reason)>>8);
      ILOG1(GID_CORE, "got UMI_BSS_CHANNEL_SWITCH_%x IND (announce), channel = %d",
        psNetwork->u16BSSstatus, nic->slow_ctx->dot11h.set_channel);
      mtlk_dot11h_set_cfg(&nic->slow_ctx->dot11h, &dot11h_cfg);
      mtlk_dot11h_on_channel_switch_announcement_ind(&nic->slow_ctx->dot11h);
      break;

    case UMI_BSS_CHANNEL_SWITCH_SILENT:
      mtlk_dot11h_get_cfg(&nic->slow_ctx->dot11h, &dot11h_cfg);
      dot11h_cfg.u8SpectrumMode     =
          mtlk_aux_atol(mtlk_get_mib_value(PRM_SPECTRUM_MODE, nic));
      dot11h_cfg.u8Bonding = nic->slow_ctx->bonding;
      nic->slow_ctx->dot11h.event = MTLK_DFS_EVENT_CHANGE_CHANNEL_SILENT;/*silent*/
      nic->slow_ctx->dot11h.set_channel = ((psNetwork->u16Reason)>>8);
      ILOG1(GID_CORE, "got UMI_BSS_CHANNEL_SWITCH_%x IND (announce), channel = %d",
        psNetwork->u16BSSstatus, nic->slow_ctx->dot11h.set_channel);
      mtlk_dot11h_set_cfg(&nic->slow_ctx->dot11h, &dot11h_cfg);
      mtlk_dot11h_on_channel_switch_announcement_ind(&nic->slow_ctx->dot11h);
      break;

    case UMI_BSS_RADAR_NORM:
    case UMI_BSS_RADAR_HOP:
      ILOG0(GID_CORE, "got UMI_BSS_RADAR_%x indication",psNetwork->u16BSSstatus);
      if (!mtlk_is_11h_radar_detection_enabled((mtlk_handle_t)nic)) {
        ELOG("Radar detection is disabled by user, iwpriv g11hRadarDetect = 0");
        break;
      }

      if (MTLK_DOT11H_IN_PROCESS == nic->slow_ctx->dot11h.status) {
        ELOG("Previous channel switch not finished yet");
        break;
      }
	   
      mtlk_dot11h_get_cfg(&nic->slow_ctx->dot11h, &dot11h_cfg);
      dot11h_cfg.u8IsHT = nic->slow_ctx->is_ht_cur;
      dot11h_cfg.u8FrequencyBand = nic->slow_ctx->frequency_band_cur;
      dot11h_cfg.u16Channel         = nic->slow_ctx->channel;
      dot11h_cfg.u8SpectrumMode     =
          mtlk_aux_atol(mtlk_get_mib_value(PRM_SPECTRUM_MODE, nic));
      dot11h_cfg.u8Bonding = nic->slow_ctx->bonding;

      nic->slow_ctx->dot11h.event = MTLK_DFS_EVENT_RADAR_DETECTED;
      mtlk_dot11h_set_cfg(&nic->slow_ctx->dot11h, &dot11h_cfg);
      switch_data.channel = 0;
      switch_data.reason = SWR_RADAR_DETECTED;
      switch_data.criteria = SWR_UNKNOWN;

      if (nic->slow_ctx->cfg.dot11h_debug_params.debugNewChannel != -1) {
        switch_data.channel = nic->slow_ctx->cfg.dot11h_debug_params.debugNewChannel;
        switch_data.criteria = CHC_USERDEF;
        ILOG0("The next channel explicitely set to %d by user", switch_data.channel);
      }

      mtlk_dot11h_initiate_channel_switch(&nic->slow_ctx->dot11h, &switch_data, FALSE,
        dot11h_on_channel_switch_clb);
      break;

    default:
      ELOG("Unrecognised network event %d",
	     psNetwork->u16BSSstatus);
      break;
    }

  psNetwork->u16BSSstatus = cpu_to_le16(psNetwork->u16BSSstatus); 
  psNetwork->u16CFflag = cpu_to_le16(psNetwork->u16CFflag); 
  psNetwork->u16Reason = cpu_to_le16(psNetwork->u16Reason); 
}

static mtlk_txmm_clb_action_e __MTLK_IFUNC
set_wep_key_for_peer_clb(mtlk_handle_t clb_usr_data, mtlk_txmm_data_t* data, mtlk_txmm_clb_reason_e reason)
{
  UMI_SET_KEY* msg = (UMI_SET_KEY *)data->payload;

  if (reason != MTLK_TXMM_CLBR_CONFIRMED)
    ELOG("Reason for TXMM callback is %d", reason);
  else if (msg->u16Status != UMI_OK)
    ELOG("Status is %d", msg->u16Status);

  return MTLK_TXMM_CLBA_FREE;
}

static int
set_wep_key_for_peer(struct nic *nic, sta_entry *sta)
{
  int res = -EINVAL;
  int16 i;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_SET_KEY *umi_key;

  ASSERT(sta);

  sta->cipher = IW_ENCODE_ALG_NONE;

  i = nic->slow_ctx->default_wep_key;

  if (sta->peer_ap) {
    if (nic->slow_ctx->peerAPs_key_idx) {
      i = nic->slow_ctx->peerAPs_key_idx -1;
      sta->cipher = IW_ENCODE_ALG_WEP;
    }
  } else {
    if (nic->slow_ctx->wep_enabled && !nic->slow_ctx->wps_in_progress)
      sta->cipher = IW_ENCODE_ALG_WEP;
  }

  if (sta->cipher == IW_ENCODE_ALG_WEP) {
    ASSERT(i >= 0 && i <= 3);
    if (nic->slow_ctx->wep_keys.sKey[i].u8KeyLength != MIB_WEP_KEY_WEP1_LENGTH &&
        nic->slow_ctx->wep_keys.sKey[i].u8KeyLength != MIB_WEP_KEY_WEP2_LENGTH) {
      ELOG("%s: Invalid length of WEP key[%d] - %d", nic->ndev->name,
          i+1, nic->slow_ctx->wep_keys.sKey[i].u8KeyLength);
      return -EINVAL;
    }
  }

  man_entry = mtlk_txmm_msg_get_empty_data(&sta->async_man_msgs[STAE_AMM_SET_KEY],
                                           nic->slow_ctx->hw_cfg.txmm);
  if (!man_entry) {
    ELOG("Can't set WEP key to MAC due to the lack of MAN_MSG");
    return -ENOMEM;
  }

  man_entry->id           = UM_MAN_SET_KEY_REQ;
  man_entry->payload_size = sizeof(*umi_key);
  umi_key = (UMI_SET_KEY*)man_entry->payload;
  memset(umi_key, 0, sizeof(*umi_key));

  memcpy(umi_key->sStationID.au8Addr, sta->mac, ETH_ALEN);
  umi_key->u16CipherSuite = cpu_to_le16(UMI_RSN_CIPHER_SUITE_NONE);
  if (sta->cipher == IW_ENCODE_ALG_WEP) {
    int klen = nic->slow_ctx->wep_keys.sKey[i].u8KeyLength;
    umi_key->u16CipherSuite = cpu_to_le16(UMI_RSN_CIPHER_SUITE_WEP40);
    if (klen == MIB_WEP_KEY_WEP2_LENGTH)
      umi_key->u16CipherSuite = cpu_to_le16(UMI_RSN_CIPHER_SUITE_WEP104);
    memcpy(umi_key->au8Tk1, nic->slow_ctx->wep_keys.sKey[i].au8KeyData, klen);
    umi_key->u16DefaultKeyIndex = cpu_to_le16(i);
  }
  DUMP2(umi_key, sizeof(*umi_key), "dump of UMI_SET_KEY");
  res = mtlk_txmm_msg_send(&sta->async_man_msgs[STAE_AMM_SET_KEY], 
                           set_wep_key_for_peer_clb, HANDLE_T(NULL), TXMM_DEFAULT_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG("mtlk_mm_send failed: %i", res);
    return -EFAULT;
  }

  return 0;
}


/*****************************************************************************
**
** NAME         handleNetworkConnectInd
**
** PARAMETERS   psMsg               Network Indication Message to process
**              nic           Card context
**
** RETURNS      none
**
** DESCRIPTION  This function is called from the Network thread to handle
**              Connection Event indication messages received from the device.
**              (called from only from msg_receive())
**
******************************************************************************/
static void
handleNetworkConnectInd (struct nic *nic, void *payload)
{
  UMI_CONNECTION_EVENT *psConnect = (UMI_CONNECTION_EVENT *)payload;
  struct net_device *ndev = nic->ndev;
  sta_entry *sta = NULL;
  int id, i;
  uint8  dot11n_mode = FALSE;
  uint16 event       = le16_to_cpu(psConnect->u16Event);
  uint16 reason      = le16_to_cpu(psConnect->u16Reason);
  u8     rsnie_id    = 0;

  if(psConnect->u8HTmode && (psConnect->u32SupportedRates & LM_PHY_11N_RATE_MSK))
  {
    dot11n_mode = TRUE;
  }
  else
  {
    dot11n_mode = FALSE;
  }

  switch (event) {
  case UMI_CONNECTED:
  case UMI_RECONNECTED:
    if (nic->ap && psConnect->u8PeerAP)
      id = mtlk_stadb_add_peer_ap(&nic->slow_ctx->stadb, psConnect->sStationID.au8Addr, dot11n_mode);
    else
      id = mtlk_stadb_add_sta(&nic->slow_ctx->stadb, psConnect->sStationID.au8Addr, dot11n_mode);
    if (id == -1)
      break;
    sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, id);
    if (nic->ap == 1)
    {
      u8 *addr = psConnect->sStationID.au8Addr;
      u8 *rsnie = psConnect->sRSNie.au8RsnIe;
      u8  rsnie_len = rsnie[1] +2;

      rsnie_id = rsnie[0];

      if (event == UMI_CONNECTED)
        ILOG1(GID_CORE, "%s %Y (%sN) has connected",
            psConnect->u8PeerAP ? "Peer AP" : "Station", addr,
            dot11n_mode ? "" : "non-");
      else
        ILOG1(GID_CORE, "Station %Y (%sN) has reconnected. Previous BSS was %Y",
            psConnect->sStationID.au8Addr, dot11n_mode ? "" : "non-",
            psConnect->sPrevBSSID.au8Addr);

      /*
       * WARNING! When there is RSN IE, we send address
       * in IWEVREGISTERED event not in wrqu.addr.sa_data as usual,
       * but in extra along with RSN IE data.
       * Why? Because iwreq_data is union and there is no other way
       * to send address and RSN IE in one event.
       * IWEVREGISTERED event is handled in hostAPd only
       * so there might be not any collessions with such non-standart
       * implementation of it.
       */
      DUMP3(psConnect, sizeof(UMI_CONNECTION_EVENT), "UMI_CONNECTION_EVENT:");
      DUMP2(rsnie, sizeof(psConnect->sRSNie.au8RsnIe), "dump of RSNIE:");
      if (rsnie_id) {
        union iwreq_data wrqu;
        u8 buf[IW_CUSTOM_MAX], *p=buf;
        p += sprintf(p, "NEWSTA " MAC_FMT ", RSNIE_LEN %i : ",
            MAC_ARG(addr), rsnie_len);
        for (i=0; i<rsnie_len; i++)
          p += sprintf(p, "%02x", rsnie[i]);
        ILOG2(GID_CORE, "sending event: %s", buf);
        memset(&wrqu, 0, sizeof(wrqu));
        wrqu.data.length = p - buf +1;
        ASSERT(wrqu.data.length < IW_CUSTOM_MAX);
        wireless_send_event(ndev, IWEVCUSTOM, &wrqu, buf);
      } else {
        set_wep_key_for_peer(nic, sta); 
        if (!sta->peer_ap)
          send_conn_sta_event(ndev, IWEVREGISTERED, addr);
      }
    }
    else  // STA
    {
      ASSERT(nic->slow_ctx->connected == 0);
      memcpy(nic->slow_ctx->bssid, psConnect->sStationID.au8Addr, 6);
      if (event == UMI_CONNECTED)
        ILOG1(GID_CORE, "connected to %Y (%sN)",
             psConnect->sStationID.au8Addr, 
             dot11n_mode ? "" : "non-");
      else
        ILOG1(GID_CORE, "connected to %Y, previous BSSID was %Y",
            psConnect->sStationID.au8Addr,
            psConnect->sPrevBSSID.au8Addr);
      send_assoc_event(nic, nic->slow_ctx->bssid);
      mtlk_core_set_net_state(nic, NET_STATE_CONNECTED);
      if (event == UMI_CONNECTED) {
        // reset counters only for new connections
        nic->pstats.sta_session_rx_packets = 0;
        nic->pstats.sta_session_tx_packets = 0;
      }

#ifdef PHASE_3
      if (!nic->ap) {
        mtlk_start_cache_update(nic);
      }
#endif

      // make BSS we've connected to persistent in cache until we're disconnected
      mtlk_cache_set_persistent(&nic->slow_ctx->cache, 
                                nic->slow_ctx->bssid, 
                                TRUE);

      mtlk_osal_event_set(&nic->slow_ctx->connect_event);
    }

    if (dot11n_mode) {
      mtlk_addba_on_connect_ex(&nic->slow_ctx->addba, 
                               &psConnect->sStationID,
                               &sta->addba_peer,
                               HANDLE_T(id));
    }

    if (nic->slow_ctx->rsnie.au8RsnIe[0] && !sta->peer_ap) {
      /* In WPA/WPA security start ADDBA after key is set */
      mtlk_stadb_sta_set_filter(sta, MTLK_PCKT_FLTR_ALLOW_802_1X);
      ILOG1(GID_CORE, "%Y: turn on 802.1x filtering due to RSN", sta->mac);
    } else if (!nic->ap && nic->slow_ctx->wps_in_progress) {
      mtlk_stadb_sta_set_filter(sta, MTLK_PCKT_FLTR_ALLOW_802_1X);
      ILOG1(GID_CORE, "%Y: turn on 802.1x filtering due to WPS", sta->mac);
    }

    if (nic->slow_ctx->connected == 0) {
      ILOG3(GID_CORE, "handleNetworkConnectInd, call mtlk_flctrl_start");
#if MT_FLCTRL_ENABLED 
      mtlk_flctrl_start (&nic->flctrl, nic->flctrl_id);
#else

      if (nic->pack_sched_enabled)
        nic->tx_prohibited = 0;
      else netif_start_queue(ndev);

#endif
    }
    nic->slow_ctx->connected++;
    ILOG3(GID_CORE, "connected counter - %i", nic->slow_ctx->connected);

    mtlk_qos_reset_acm_bits(&nic->qos);

    break;

  case UMI_DISCONNECTED:
    {
      uint32 net_state;

      net_state = mtlk_core_get_net_state(nic);
      if (net_state != NET_STATE_CONNECTED) {
        ILOG1(GID_DISCONNECT, "Failed to connect to %Y for reason %d (%s)",
          psConnect->sStationID.au8Addr,
          reason, cr_str[reason]);

        if (reason == UMI_BSS_JOIN_FAILED)
          /* AP is dead? Force user to rescan to see this BSS again */
          mtlk_cache_remove_bss_by_bssid(&nic->slow_ctx->cache, psConnect->sStationID.au8Addr);

        mtlk_osal_event_set(&nic->slow_ctx->connect_event);
      } else {
        if (nic->ap == 1) {
          ILOG1(GID_DISCONNECT, "STA %Y disconnected for reason %d (%s)",
            psConnect->sStationID.au8Addr,
            reason, cr_str[reason]);
        } else {
          ILOG1(GID_DISCONNECT, "Disconnected from BSS %Y for reason %d (%s)",
            psConnect->sStationID.au8Addr,
            reason, cr_str[reason]);

          /* We could have BG scan doing right now.
           * If we don't switch BG scan type off, it's likely
           * to bail out with error when trying to enable PS mode 
           * in not connected MAC.
           */
          mtlk_scan_set_background(&nic->slow_ctx->scan, FALSE);
        }
      }
      /* send disconnect request */
      send_disconnect_req(nic, psConnect->sStationID.au8Addr, FALSE);
    }
    return;
  default:
    ELOG("Unrecognised connection event %d", event);
    break;
  }
}

static void
handle_dynamic_param_ind (struct nic *nic, unsigned char *payload)
{
  int i;
  UMI_DYNAMIC_PARAM_TABLE *psDynamicParamTable;
  psDynamicParamTable = (UMI_DYNAMIC_PARAM_TABLE *)payload;

  for (i = 0; i < NTS_PRIORITIES; i++)
    ILOG5(GID_CORE, "Set ACM bit for priority %d: %d", i, psDynamicParamTable->ACM_StateTable[i]);

  mtlk_qos_set_acm_bits(&nic->qos, psDynamicParamTable->ACM_StateTable);
}

/*****************************************************************************
**
** NAME         alloc_mtlkdev
**
** PARAMETERS   none
**
** RETURNS      Allocated net_device structure
**
** DESCRIPTION  This function called to allocate and initialise net_device
**              structure
**
******************************************************************************/
extern const struct iw_handler_def mtlk_linux_handler_def;

static int
__mtlk_prevent_change_mtu (struct net_device *dev, int new_mtu)
{
  return -EFAULT;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
static const struct net_device_ops mtlk_netdev_ops = {
  .ndo_open            = mtlk_iface_open,
  .ndo_stop            = mtlk_iface_stop,
  .ndo_start_xmit      = mtlk_wrap_transmit,
  .ndo_set_mac_address = mtlk_set_mac_addr,
  .ndo_do_ioctl        = mtlk_ioctl_do_ioctl,
  .ndo_get_stats       = mtlk_linux_get_stats,
  .ndo_change_mtu      = __mtlk_prevent_change_mtu,
};
#endif

static struct net_device*
alloc_mtlkdev(void)
{
  struct net_device *dev;
  struct nic *nic;

  dev = alloc_etherdev(sizeof(struct nic));
  if (dev == NULL)
    return NULL;

  nic = netdev_priv(dev);
  nic->ndev = dev;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
  dev->hard_start_xmit = mtlk_wrap_transmit;
  dev->tx_timeout = NULL; /* netdev watchdog disabled */
  dev->get_stats = mtlk_linux_get_stats;
  dev->change_mtu = __mtlk_prevent_change_mtu;
  dev->open = mtlk_iface_open;
  dev->stop = mtlk_iface_stop;
  dev->set_mac_address = mtlk_set_mac_addr;
  dev->do_ioctl = mtlk_ioctl_do_ioctl;
#else
  dev->netdev_ops = &mtlk_netdev_ops;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
  dev->get_wireless_stats = mtlk_linux_get_iw_stats;
#endif
  dev->wireless_handlers = (struct iw_handler_def *)&mtlk_linux_handler_def;

  //set initial net state
  nic->net_state = NET_STATE_HALTED;
  return dev;
}

static void
handle_security_alert_ind(unsigned char *payload, struct nic *nic)
{
  UMI_SECURITY_ALERT *usa = (UMI_SECURITY_ALERT*)payload;
  union iwreq_data wrqu;
  struct iw_michaelmicfailure *mic = (struct iw_michaelmicfailure*)&wrqu.data;

  memset(mic, 0, sizeof(mic));

  if (usa->u16KeyType == UMI_RSN_PAIRWISE_KEY)
    mic->flags |= IW_MICFAILURE_PAIRWISE;
  else
    mic->flags |= IW_MICFAILURE_GROUP;

  mic->src_addr.sa_family = ARPHRD_ETHER;
  memcpy(mic->src_addr.sa_data, usa->sStationID.au8Addr, ETH_ALEN);

  switch (usa->u16EventCode) {
  case UMI_RSN_EVENT_TKIP_MIC_FAILURE:
    wireless_send_event(nic->ndev, IWEVMICHAELMICFAILURE, &wrqu, NULL);
    break;
  default:
    break;
  }
}

extern const char *mtlk_drv_info[];

static void 
mtlk_print_drv_info (void) {
#if (MAX_DLEVEL >= 1)
  int i = 0;
  ILOG1(GID_CORE, "*********************************************************");
  ILOG1(GID_CORE, "* Driver Compilation Details:");
  ILOG1(GID_CORE, "*********************************************************");
  while (mtlk_drv_info[i]) {
    ILOG1(GID_CORE, "* %s", mtlk_drv_info[i]);
    i++;
  }
  ILOG1(GID_CORE, "*********************************************************");
#endif
}

struct nic * __MTLK_IFUNC
mtlk_core_create (mtlk_persistent_device_data_t *persistent)
{
  struct nic *nic;
  struct net_device *dev;
  mib_act *mib;
  int mib_count;
  int txmm_cnt = 0;
  int txdm_cnt = 0;
  int txem_cnt = 0;

  mtlk_print_drv_info();
  
  /* sizeof(skb->cb) == 40 in 2.6.11 */
  ASSERT(sizeof(struct skb_private) <= 40);

  dev = alloc_mtlkdev();
  if (dev == NULL) {
    ELOG("netdev alloc failed");
    goto err_netdev_alloc;
  }    

  nic = netdev_priv(dev);

  nic->slow_ctx = kmalloc_tag(sizeof(struct nic_slow_ctx), GFP_KERNEL, MTLK_MEM_TAG_CORE);
  if (nic->slow_ctx == NULL) {
    ELOG("slow_ctx alloc failed");
    goto err_slow_ctx_alloc;
  }
  memset(nic->slow_ctx, 0, sizeof(struct nic_slow_ctx));
  nic->slow_ctx->nic = nic;

  nic->slow_ctx->mib = g2_mib_action;

  nic->slow_ctx->power_limit = 
    MTLK_BFIELD_VALUE(NIC_POWER_LIMIT_VAL, 0, uint16) | /* irrelevant while NIC_POWER_LIMIT_IS_SET == 0 */
    MTLK_BFIELD_VALUE(NIC_POWER_LIMIT_IS_SET, 0, uint16);

  mib = nic->slow_ctx->mib;
  mib_count = 0;
  while ((mib->mib_name != NULL) && strlen(mib->mib_name)) {
    mib->index = mib_count; /* link nic->slow_ctx->mib and nic->slow_ctx->mib_value */
    mib_count++;
    mib++;
  }

  nic->slow_ctx->mib_value = kmalloc_tag(mib_count*sizeof(char *), GFP_KERNEL, MTLK_MEM_TAG_MIB_VALUES);
  if (nic->slow_ctx->mib_value == NULL) {
    ELOG("mib_value alloc failed");
    goto err_mib_value_alloc;
  }
  memset(nic->slow_ctx->mib_value, 0, mib_count*sizeof(char *));

  mtlk_mib_set_nic_cfg(nic);

  // Initialize all system lists, semaphores, atomic variables, spin locks etc.
  spin_lock_init(&nic->net_state_lock); // spin lock for net state usage
  spin_lock_init(&nic->l2nat_lock); // lock for l2nat table&list
  rwlock_init(&nic->slow_ctx->stat_lock);
  
  /* init list of the mtlk_seq_ops structs */
  INIT_LIST_HEAD(&nic->slow_ctx->seq_ops_list);

  init_timer(&nic->slow_ctx->stat_poll_timer);
  nic->slow_ctx->stat_poll_timer.function = stat_poll_timer_handler;
  nic->slow_ctx->stat_poll_timer.data = (unsigned long)nic->ndev;

  if (mtlk_osal_timer_init(&nic->slow_ctx->mac_watchdog_timer,
                           mac_watchdog_timer_handler,
                           (mtlk_handle_t)nic) != MTLK_ERR_OK) {
    ELOG("Cannot initialize MAC watchdog timer");
    goto err_mac_watchdog_timer_init;
  }

  nic->slow_ctx->mac_soft_reset_enable = 1;
  nic->slow_ctx->last_pm_spectrum = -1;
  nic->slow_ctx->last_pm_freq = MTLK_HW_BAND_NONE;

  nic->slow_ctx->cfg.legacy_forced_rate = NO_RATE;
  nic->slow_ctx->cfg.ht_forced_rate = NO_RATE;

  nic->slow_ctx->mac_watchdog_timeout_ms = MAC_WATCHDOG_DEFAULT_TIMEOUT_MS;
  nic->slow_ctx->mac_watchdog_period_ms = MAC_WATCHDOG_DEFAULT_PERIOD_MS;

  /* Initialize WEP keys */
  nic->slow_ctx->wep_keys.sKey[0].u8KeyLength =
  nic->slow_ctx->wep_keys.sKey[1].u8KeyLength =
  nic->slow_ctx->wep_keys.sKey[2].u8KeyLength =
  nic->slow_ctx->wep_keys.sKey[3].u8KeyLength =
    MIB_WEP_KEY_WEP1_LENGTH;

  init_timer(&nic->slow_ctx->mtlk_timer);
  nic->slow_ctx->mtlk_timer.function = mtlk_timer_handler;
  nic->slow_ctx->mtlk_timer.data = (unsigned long)nic->ndev;

  mtlk_osal_event_init(&nic->slow_ctx->connect_event);

  nic->slow_ctx->tx_limits.num_tx_antennas = DEFAULT_NUM_TX_ANTENNAS;
  nic->slow_ctx->tx_limits.num_rx_antennas = DEFAULT_NUM_RX_ANTENNAS;

  if (mtlk_eq_init(&nic->slow_ctx->eq, nic) != MTLK_ERR_OK) {
    ELOG("EventQueue init failed");
    goto err_event_queue_init;
  }

  if (sq_init(nic) != MTLK_ERR_OK) {
    ELOG("SendQueue init failed");
    goto err_send_queue_init;
  }

#ifdef MTCFG_IRB_DEBUG
  if (mtlk_irb_pinger_init(&nic->slow_ctx->pinger) != MTLK_ERR_OK) {
    ELOG("IRB Pinger init failed");
    goto err_irb_pinger_init;
  }
#endif

  for (txmm_cnt = 0; txmm_cnt < ARRAY_SIZE(nic->txmm_async_msgs); txmm_cnt++) {
    if (mtlk_txmm_msg_init(&nic->txmm_async_msgs[txmm_cnt]) != MTLK_ERR_OK) {
      ELOG("TXMM msg#%d init failed", txmm_cnt);
      goto err_txmm_msg_init;
    }
  }

  for (txdm_cnt = 0; txdm_cnt < ARRAY_SIZE(nic->txdm_async_msgs); txdm_cnt++) {
    if (mtlk_txmm_msg_init(&nic->txdm_async_msgs[txdm_cnt]) != MTLK_ERR_OK) {
      ELOG("TXDM msg#%d init failed", txdm_cnt);
      goto err_txdm_msg_init;
    }
  }

  for (txem_cnt = 0; txem_cnt < ARRAY_SIZE(nic->txmm_async_eeprom_msgs); txem_cnt++) {
    if (mtlk_txmm_msg_init(&nic->txmm_async_eeprom_msgs[txem_cnt]) != MTLK_ERR_OK) {
      ELOG("EEPROM msg#%d init failed", txem_cnt);
      goto err_txem_msg_init;
    }
  }

  mtlk_stadb_init(&nic->slow_ctx->stadb, nic, nic->sq);

  return nic;

err_txem_msg_init:
  for (; txem_cnt > 0; txem_cnt--) {
    mtlk_txmm_msg_cleanup(&nic->txmm_async_eeprom_msgs[txem_cnt - 1]);
  }
err_txdm_msg_init:
  for (; txdm_cnt > 0; txdm_cnt--) {
    mtlk_txmm_msg_cleanup(&nic->txdm_async_msgs[txdm_cnt - 1]);
  }
err_txmm_msg_init:
  for (; txmm_cnt > 0; txmm_cnt--) {
    mtlk_txmm_msg_cleanup(&nic->txmm_async_msgs[txmm_cnt - 1]);
  }
#ifdef MTCFG_IRB_DEBUG
err_irb_pinger_init:
#endif
  sq_cleanup(nic);
err_send_queue_init:
  mtlk_eq_cleanup(&nic->slow_ctx->eq);
err_event_queue_init:
  mtlk_osal_event_cleanup(&nic->slow_ctx->connect_event);
  mtlk_osal_timer_cleanup(&nic->slow_ctx->mac_watchdog_timer);
err_mac_watchdog_timer_init:
  kfree_tag(nic->slow_ctx->mib_value);
err_mib_value_alloc:
  kfree_tag(nic->slow_ctx);
err_slow_ctx_alloc:
  free_netdev(dev);
err_netdev_alloc:
  return NULL;
}

static int
mtlk_core_set_default_band(struct nic *nic)
{
  if (mtlk_core_is_band_supported(nic, MTLK_HW_BAND_BOTH) == MTLK_ERR_OK) 
    nic->slow_ctx->frequency_band_cfg = MTLK_HW_BAND_BOTH;
  else if (mtlk_core_is_band_supported(nic, MTLK_HW_BAND_5_2_GHZ) == MTLK_ERR_OK)
    nic->slow_ctx->frequency_band_cfg = MTLK_HW_BAND_5_2_GHZ;
  else if (mtlk_core_is_band_supported(nic, MTLK_HW_BAND_2_4_GHZ) == MTLK_ERR_OK)
    nic->slow_ctx->frequency_band_cfg = MTLK_HW_BAND_2_4_GHZ;
  else {
    ELOG("None of the bands is supported");
    return MTLK_ERR_UNKNOWN;
  }

  return MTLK_ERR_OK;
}

static int
_mtlk_core_process_antennas_configuration(mtlk_core_t *nic)
{
  char *mib_tx_str;
  char *mib_rx_str;
  uint8 num_tx_antennas = nic->slow_ctx->tx_limits.num_tx_antennas;
  uint8 num_rx_antennas = nic->slow_ctx->tx_limits.num_rx_antennas;

#ifdef MTLK_DEBUG
  mtlk_hw_gen_e hw_generation = 0;
  int res = mtlk_hw_get_prop(nic->hw, MTLK_HW_GENERATION, 
                             (void *)&hw_generation, sizeof(hw_generation));

  if(MTLK_ERR_OK != res)
  {
    ELOG("Failed to determine HW generation, err: %d", res);
    return res;
  }

  if(MTLK_HW_GEN2 == hw_generation)
  {
    MTLK_ASSERT(2 == num_tx_antennas);
    MTLK_ASSERT(3 == num_rx_antennas);
  }
#endif

  /* determine number of TX antennas */
  if (2 == num_tx_antennas) {
    mib_tx_str = "120";
  }   
  else if (3 == num_tx_antennas) {
    mib_tx_str = "123";
  }
  else {
    MTLK_ASSERT(!"Wrong number of TX antennas");
    return MTLK_ERR_UNKNOWN;
  }

  /* determine number of RX antennas */
  if (2 == num_rx_antennas) {
    mib_rx_str = "120";
  }
  else if (3 == num_rx_antennas) {
    mib_rx_str = "123";
  }
  else {
    MTLK_ASSERT(!"Wrong number of RX antennas");
    return MTLK_ERR_UNKNOWN;
  }

  mtlk_set_mib_value(PRM_TX_ANTENNAS, mib_tx_str, nic);
  mtlk_set_mib_value(PRM_RX_ANTENNAS, mib_rx_str, nic);

  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_core_start (mtlk_core_t *nic, mtlk_hw_t *hw, const mtlk_core_hw_cfg_t *cfg, uint32 num_boards_alive)
{
  const char *eeprom_mac_addr = NULL;
  mtlk_coc_cfg_t coc_cfg;
  ILOG0(GID_CORE, "%s", mtlk_version_string);

  nic->hw = hw;
  nic->slow_ctx->hw_cfg = *cfg;
  nic->ap = cfg->ap;

  mtlk_core_set_net_state(nic, NET_STATE_IDLE);

  mtlk_eeprom_read_and_parse(mtlk_core_get_eeprom(nic), nic->slow_ctx->hw_cfg.txmm);

  eeprom_mac_addr = mtlk_eeprom_get_nic_mac_addr(&nic->slow_ctx->ee_data);
  /* Validate MAC address */
  if (!mtlk_osal_is_valid_ether_addr(eeprom_mac_addr)) {
    ELOG("Invalid EEPROM MAC address (%Y)!", eeprom_mac_addr);
    goto err_invalid_eeprom_mac_address;
  }

  /* Set MAC address into net_device, nic and send MIB to MAC */
  mtlk_core_send_mac_addr_tohw(nic, eeprom_mac_addr);
  memcpy(nic->ndev->dev_addr, nic->mac_addr, ETH_ALEN);

  if (nic->slow_ctx->ee_data.valid) {
    MIB_VALUE uValue;
    /* Check EEPROM options mask */
    ILOG0(GID_CORE, "Options mask is 0x%02x", nic->slow_ctx->ee_data.card_id.dev_opt_mask.d);
    if (nic->slow_ctx->ee_data.card_id.dev_opt_mask.s.ap_disabled && cfg->ap) {
      ELOG("AP functionality is not available on this device");
      goto err_ap_func_is_not_available;
    }
    nic->slow_ctx->disable_sm_channels =
      nic->slow_ctx->ee_data.card_id.dev_opt_mask.s.disable_sm_channels;
    if (nic->slow_ctx->disable_sm_channels)
      ILOG0(GID_CORE, "DFS (SM-required) channels will not be used");
    /* Send ee_version to MAC */
    memset(&uValue, 0, sizeof(MIB_VALUE));
    uValue.sEepromInfo.u16EEPROMVersion = cpu_to_le16(nic->slow_ctx->ee_data.eeprom_version);
    if (nic->slow_ctx->ee_data.tpc_52)
      uValue.sEepromInfo.u8NumberOfPoints5GHz = nic->slow_ctx->ee_data.tpc_52->num_points;
    if (nic->slow_ctx->ee_data.tpc_24)
      uValue.sEepromInfo.u8NumberOfPoints2GHz = nic->slow_ctx->ee_data.tpc_24->num_points;
    mtlk_set_mib_value_raw(nic->slow_ctx->hw_cfg.txmm, MIB_EEPROM_VERSION, &uValue);
  }

  nic->slow_ctx->cfg.dot11d = 1;
  if (nic->ap)
    nic->slow_ctx->cfg.country_code = mtlk_eeprom_get_country_code(&nic->slow_ctx->ee_data);
  else
    nic->slow_ctx->cfg.country_code = 0; /* according to IEEE 802.11d */

  if (nic->ap)
    mtlk_cache_init(&nic->slow_ctx->cache, 0);
  else
    mtlk_cache_init(&nic->slow_ctx->cache, SCAN_CACHE_AGEING);

  if (mtlk_create_mib_sysfs(nic) != MTLK_ERR_OK) {
    ELOG("MIB init failed");
    goto err_mib_init;
  }

  if (MTLK_ERR_OK != mtlk_init_tx_limit_tables(&nic->slow_ctx->tx_limits,
                       MAC_TO_HOST16(nic->slow_ctx->ee_data.vendor_id),
                       MAC_TO_HOST16(nic->slow_ctx->ee_data.device_id),
                       nic->slow_ctx->ee_data.card_id.type,
                       nic->slow_ctx->ee_data.card_id.revision)) {
    ELOG("tx_limit init failed");
    goto err_tx_limit_init;
  }

  /* Process RF usage mode and perform actions required */
  if (MTLK_ERR_OK != _mtlk_core_process_antennas_configuration(nic)) {
    ELOG("Antennas configuration failed");
    goto err_ant_config;
  }

  nic->slow_ctx->power_selection = 0;

  coc_cfg.hw_antenna_cfg.num_tx_antennas = nic->slow_ctx->tx_limits.num_tx_antennas;
  coc_cfg.hw_antenna_cfg.num_rx_antennas = nic->slow_ctx->tx_limits.num_rx_antennas;
  coc_cfg.txmm                           = nic->slow_ctx->hw_cfg.txmm;

  nic->slow_ctx->coc_mngmt = mtlk_coc_create(&coc_cfg);
  if (!nic->slow_ctx->coc_mngmt) {
    ELOG("CoC creation failed");
    goto err_coc_init;
  }

  mtlk_debug_reset_counters(nic);

  if (mtlk_l2nat_init(nic) != MTLK_ERR_OK) {
    ELOG("L2NAT init failed");
    goto err_l2nat_init;
  }
  
  if (flctrl_init(nic)) {
    ELOG("flctrl init failed");
    goto err_flctrl_init;
  }

  if (mtlk_scan_osdep_init(&nic->slow_ctx->scan, nic) != MTLK_ERR_OK) {
    ELOG("Scan init failed");
    goto err_scan_init;
  }

  if (dot11h_init(nic)) {
    ELOG("dot11h init failed");
    goto err_dot11h_init;
  }

  if (aocs_init(nic)) {
    ELOG("AOCS init failed");
    goto err_aocs_init;
  }

  if (addba_init(nic) != MTLK_ERR_OK) {
    ELOG("ADDBA init failed");
    goto err_addba_init;
  }

#ifdef MTCFG_RF_MANAGEMENT_MTLK
  {
    mtlk_rf_mgmt_cfg_t rf_mgmt_cfg = {0};

    rf_mgmt_cfg.txmm  = nic->slow_ctx->hw_cfg.txmm;
    rf_mgmt_cfg.stadb = &nic->slow_ctx->stadb;
    rf_mgmt_cfg.device_is_busy = mtlk_is_device_busy;

    if (mtlk_rf_mgmt_init(&nic->rf_mgmt, &rf_mgmt_cfg) != MTLK_ERR_OK) {
      ELOG("RF MGMT init failed");
      goto err_rf_mgmt_init;
    }
  }
#endif /* MTCFG_RF_MANAGEMENT_MTLK */

#if MT_FLCTRL_ENABLED 
  nic->flctrl_id = 0;
  mtlk_flctrl_register(&nic->flctrl, &nic->flctrl_id);
#endif
	
  mtlk_mc_init(nic);

  nic->slow_ctx->is_ht_cfg = /* walk through */
  nic->slow_ctx->is_ht_cur = 1;

  /* Not HALT FW in case of Unconfirmed Disconnect Req by default */
  nic->slow_ctx->is_halt_fw_on_disc_timeout = FALSE;

  if (mtlk_core_set_default_band(nic) != MTLK_ERR_OK) {
    ELOG("default band can't be selected");
    goto err_band_select;
  }
  nic->slow_ctx->frequency_band_cur = nic->slow_ctx->frequency_band_cfg;

  nic->slow_ctx->net_mode_cfg = /* walk through */
  nic->slow_ctx->net_mode_cur = get_net_mode(nic->slow_ctx->frequency_band_cfg,
                                             nic->slow_ctx->is_ht_cfg);

  nic->slow_ctx->sta_force_spectrum_mode = SPECTRUM_AUTO;

  if (mtlk_qos_init(&nic->qos)) {
    ELOG("QOS init failed");
    goto err_qos_init;
  }

  /* From time we've allocated device name till time we register netdevice
   * we should hold rtnl lock to prohibit someone else from registering same netdevice name.
   * We can't use register_netdev, because we've splitted netdevice registration into 2 phases:
   * 1) allocate name
   * ... driver internal initialization
   * 2) register.
   * We need this because:
   * 1) initialization (registration of proc entries) requires knowledge of netdevice name
   * 2) we can't register netdevice before we have initialized the driver (we might crash on
   * request from the OS)
   */
  rtnl_lock();

  if (dev_alloc_name(nic->ndev, NIC_NAME "%d") < 0) {
    ELOG("netdev name alloc failed");
    goto err_netdev_name_alloc;
  }

  if (register_netdevice(nic->ndev) != 0) {
    ELOG("netdev register failed");
    goto err_netdev_register;
  }

  if (mtlk_debug_register_proc_entries(nic) != MTLK_ERR_OK) {
    ELOG("debug iface init failed");
    goto err_debug_init;
  }

  if (mtlk_eq_start(&nic->slow_ctx->eq) != MTLK_ERR_OK) {
    ELOG("EventQueue start failed");
    goto err_event_queue_start;
  }

  if (mtlk_osal_timer_set(&nic->slow_ctx->mac_watchdog_timer,
                          nic->slow_ctx->mac_watchdog_period_ms) != MTLK_ERR_OK) {
    ELOG("MAC watchdog timer start failed");
    goto err_mac_watchdog_timer_set;
  }

  rtnl_unlock();

  mtlk_core_set_net_state(nic, NET_STATE_READY);

  return MTLK_ERR_OK;

err_mac_watchdog_timer_set:
  mtlk_eq_stop(&nic->slow_ctx->eq);
err_event_queue_start:
  mtlk_debug_procfs_cleanup(nic, num_boards_alive);
err_debug_init:
  unregister_netdevice(nic->ndev);
err_netdev_register:
  /* nothing to do ... */
err_netdev_name_alloc:
  mtlk_qos_cleanup(&nic->qos);
  rtnl_unlock();
err_qos_init:
  /* nothing to do ... */
err_band_select:
  mtlk_flctrl_unregister(&nic->flctrl, nic->flctrl_id);
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  mtlk_rf_mgmt_cleanup(&nic->rf_mgmt);
err_rf_mgmt_init:
#endif /* MTCFG_RF_MANAGEMENT_MTLK */
  addba_cleanup(nic);
err_addba_init:
  aocs_cleanup(nic);
err_aocs_init:
  dot11h_cleanup(nic);
err_dot11h_init:
  mtlk_scan_cleanup(&nic->slow_ctx->scan);
err_scan_init:
  flctrl_cleanup(nic);
err_flctrl_init:
  mtlk_l2nat_cleanup(nic);
err_l2nat_init:
  mtlk_coc_delete(nic->slow_ctx->coc_mngmt);
err_coc_init:
err_ant_config:
  mtlk_cleanup_tx_limit_tables(&nic->slow_ctx->tx_limits);
err_tx_limit_init:
  mtlk_unregister_mib_sysfs(nic);
err_mib_init:
  mtlk_cache_cleanup(&nic->slow_ctx->cache);
err_ap_func_is_not_available:
err_invalid_eeprom_mac_address:
  mtlk_clean_eeprom_data(&nic->slow_ctx->ee_data);
  return MTLK_ERR_UNKNOWN;
}

int __MTLK_IFUNC
mtlk_core_release_tx_data (mtlk_core_t *nic, const mtlk_core_release_tx_data_t *data)
{
  int res = MTLK_ERR_UNKNOWN;  
  struct sk_buff *skb = data->nbuf;
  unsigned short qos = 0;
  struct skb_private *skb_priv = (struct skb_private *)(skb->cb);
  sta_entry *sta = skb_priv->dst_sta;
  mtlk_sq_peer_ctx_t *sq_peer_ctx = NULL;

  // check if NULL packet confirmed
  if (data->size == 0) {
    ILOG9(GID_CORE, "Confirmation for NULL skb");

    // Release NULL skbuffer
    dev_kfree_skb(skb);
    goto FINISH;
  }

  qos = mtlk_qos_get_ac_by_tid(data->access_category);
  
  if ((qos != (uint16)-1) && (nic->pstats.ac_used_counter[qos] > 0))
    --nic->pstats.ac_used_counter[qos];

  if (unlikely(nic->pack_sched_enabled ? nic->tx_prohibited : 
               netif_queue_stopped(skb->dev))) {
    if (data->resources_free) {
      ILOG2(GID_CORE, "mtlk_flctrl_wake on OS TX queue wake");
#if MT_FLCTRL_ENABLED 
      mtlk_flctrl_wake (&nic->flctrl, nic->flctrl_id);
#else

      if (nic->pack_sched_enabled) {
        nic->tx_prohibited = 0;
      } else netif_wake_queue(skb->dev);
#endif
    }
  }

  if (__LIKELY(nic->pack_sched_enabled && !nic->tx_prohibited)) {
    mtlk_sq_schedule_flush(nic);
  }

  dev_kfree_skb(skb);

  res = MTLK_ERR_OK;

FINISH:
  // If confirmed (or failed) unicast packet to known STA
  if (sta != NULL) {

    sq_peer_ctx = &sta->sq_peer_ctx;
    if (likely(data->status == UMI_OK)) {
      /* Update STA's timestamp on successful (confirmed by ACK) TX */
      mtlk_stadb_update_sta(sta);
      if (nic->ap && (qos != (uint16)-1))
        mtlk_aocs_on_tx_msdu_returned(&nic->slow_ctx->aocs, qos);
    } else {
      sta->stats.tx_failed++;
    }
  }

  if (nic->pack_sched_enabled)
    mtlk_sq_on_tx_cfm(nic->sq, sq_peer_ctx);


  /* update used Tx MSDUs counter */
  if (qos != (uint16)-1)
    msdu_tx_dec_nof_used(nic, qos);

  return res;
}

int __MTLK_IFUNC
mtlk_core_handle_rx_data (mtlk_core_t *nic, mtlk_core_handle_rx_data_t *data)
{
  struct sk_buff *skb = data->nbuf;
  ASSERT(skb != 0);

  skb->dev = nic->ndev;
  
  skb_put(skb, data->offset);
  skb_pull(skb, data->offset);

  return handle_rx_ind(nic, skb, (uint16)data->size, data->info);
}

static void 
handle_pm_update_event(mtlk_core_t *nic, const void *payload)
{
  const UMI_PM_UPDATE *msg = (const UMI_PM_UPDATE *)payload;
  sta_entry *sta;
  int id;

  ILOG2(GID_CORE, "Power management mode changed to %d for %Y",
    msg->newPowerMode, msg->sStationID.au8Addr);

  id = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, msg->sStationID.au8Addr);
  if (id < 0) {
    ILOG2(GID_UNKNOWN, "PM update event received from STA %Y which is not known",
      msg->sStationID.au8Addr);
    return;
  }

  sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, id);
  mtlk_stadb_set_pm_enabled(sta, msg->newPowerMode == UMI_STATION_IN_PS);

  if (msg->newPowerMode == UMI_STATION_ACTIVE)
    mtlk_sq_schedule_flush(nic);
}

static int
_mtlk_core_check_app_fatal_event(APP_FATAL *app_fatal)
{
  uint32 i;
  uint32 buf_size = sizeof(app_fatal->FileName);

  /* check is buffer, received from MAC contains NULL terminated symbol */
  for (i=0; i<buf_size; i++) {
    if ('\0' == app_fatal->FileName[i]) {
      break;
    }
  }

  if (0 == i) {
    /* if index is equal to zero - it means that NULL symbol is in first position */
    WLOG("Wrong buffer from MAC, empty FileName received!");
    return MTLK_ERR_PARAMS;
  }

  if (buf_size == i) {
    /* if index is equal to buf_size - it means that NULL symbol isn't found in buffer */
    WLOG("Wrong buffer from MAC, NULL - terminated symbol in string isn't present!");
    app_fatal->FileName[buf_size-1] = '\0';
    return MTLK_ERR_PARAMS;
  }

  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_core_handle_rx_ctrl (mtlk_core_t         *nic,
                          const mtlk_hw_msg_t *msg,
                          uint32               id,
                          void                *payload)
{
  u32 macHangDetected = 0;
  int res = MTLK_ERR_OK; /* Respond on IND */

  switch (id) {
    case MC_MAN_DYNAMIC_PARAM_IND:
      handle_dynamic_param_ind(nic, payload);
      break;
    
    case MC_MAN_MAC_EVENT_IND:
      //MAC exception?
      {
        MAC_EVENT *psEvent = (MAC_EVENT *)payload;
        int mac;
#ifndef MTCFG_SILENT
        char *mac_ev_prefix = "MAC event"; // don't change this - used from CLI
#endif
        uint32 event_id = le32_to_cpu(psEvent->u32EventID);
     
        mac = event_id >> 24;

        switch (0xff & event_id)
        {
        case EVENT_EXCEPTION:
          WLOG("%s: From %s : MAC exception: Cause 0x%x, EPC 0x%x, Status 0x%x, TS 0x%x, %s",
            mac_ev_prefix,
            psEvent->u.sAppFatalEvent.uLmOrUm == 0 ? "lower" : "upper",
            le32_to_cpu(psEvent->u.sAppFatalEvent.uCauseRegOrLineNum),
            le32_to_cpu(psEvent->u.sAppFatalEvent.uEpcReg),
            le32_to_cpu(psEvent->u.sAppFatalEvent.uStatusReg),
            le32_to_cpu(psEvent->u.sAppFatalEvent.uTimeStamp),
            nic->ndev->name);
          mtlk_set_hw_state(nic, MTLK_HW_STATE_EXCEPTION);
          mtlk_core_set_net_state(nic, NET_STATE_HALTED);
          macHangDetected = MTLK_HW_STATE_EXCEPTION;
          break;
        case EVENT_EEPROM_FAILURE:
          WLOG("%s: From %s : EEPROM failure : Code %d, %s",
            mac_ev_prefix,
            mac == 0 ? "upper" : "lower", psEvent->u.sEepromEvent.u8ErrCode,
            nic->ndev->name);
          break;
        case EVENT_APP_FATAL:
          _mtlk_core_check_app_fatal_event(&psEvent->u.sAppFatalEvent);
          WLOG("%s: From %s : Application fatal error: [%s@%u], TS 0x%x, %s",
            mac_ev_prefix,
            psEvent->u.sAppFatalEvent.uLmOrUm == 0 ? "lower" : "upper",
            psEvent->u.sAppFatalEvent.FileName,
            le32_to_cpu(psEvent->u.sAppFatalEvent.uCauseRegOrLineNum),
            le32_to_cpu(psEvent->u.sAppFatalEvent.uTimeStamp),
            nic->ndev->name);
          mtlk_set_hw_state(nic, MTLK_HW_STATE_APPFATAL);
          mtlk_core_set_net_state(nic, NET_STATE_HALTED);
          macHangDetected = MTLK_HW_STATE_APPFATAL;
          break;
        case EVENT_GENERIC_EVENT:
          ILOG0(GID_CORE, "%s: From %s : Generic data: size %u, %s",
            mac_ev_prefix,
            mac == 0 ? "upper" : "lower",
            le32_to_cpu(psEvent->u.sGenericData.u32dataLength),
            nic->ndev->name);
          DUMP2(&psEvent->u.sGenericData.u8data, GENERIC_DATA_SIZE, "Generic MAC data");
          break;
        case EVENT_CALIBR_ALGO_FAILURE:
          if (le32_to_cpu(psEvent->u.sCalibrationEvent.u32calibrAlgoType) == mtlk_aux_atol(mtlk_get_mib_value(PRM_ALGO_CALIBR_MASK, nic)))
            WLOG("%s: From %s : Algo calibration failure: algo type %u, error code %u, %s",
                 mac_ev_prefix,
                 mac == 0 ? "upper" : "lower",
                 le32_to_cpu(psEvent->u.sCalibrationEvent.u32calibrAlgoType),
                 le32_to_cpu(psEvent->u.sCalibrationEvent.u32ErrCode),
                 nic->ndev->name);
          else {
            ILOG0(GID_CORE, "%s: From %s : Online calibration scheduler: algo type %u, state %u, %s",
                 mac_ev_prefix,
                 mac == 0 ? "upper" : "lower",
                 le32_to_cpu(psEvent->u.sCalibrationEvent.u32calibrAlgoType),
                 le32_to_cpu(psEvent->u.sCalibrationEvent.u32ErrCode),
                 nic->ndev->name);
            }
          break;
        case EVENT_DUMMY:
          ILOG0(GID_CORE, "%s: From %s : Dummy event : %u %u %u %u %u %u %u %u, %s",
            mac_ev_prefix,
            mac==0 ? "upper" : "lower",
            le32_to_cpu(psEvent->u.sDummyEvent.u32Dummy1),
            le32_to_cpu(psEvent->u.sDummyEvent.u32Dummy2),
            le32_to_cpu(psEvent->u.sDummyEvent.u32Dummy3),
            le32_to_cpu(psEvent->u.sDummyEvent.u32Dummy4),
            le32_to_cpu(psEvent->u.sDummyEvent.u32Dummy5),
            le32_to_cpu(psEvent->u.sDummyEvent.u32Dummy6),
            le32_to_cpu(psEvent->u.sDummyEvent.u32Dummy7),
            le32_to_cpu(psEvent->u.sDummyEvent.u32Dummy8),
            nic->ndev->name);
          break;
        default:
          ILOG0(GID_CORE, "%s: From %s : other MAC event id %u, %s",
            mac_ev_prefix,
            mac==0 ? "upper" : "lower", event_id,
            nic->ndev->name);
          break;
        }
        /*if exception/fatal-event send MAC Hang event to application*/
        if (macHangDetected) {
          nic->slow_ctx->mac_stuck_detected_by_sw = 0;
          ILOG2(GID_CORE, "MAC Hung detected, event = %d",macHangDetected);
          mtlk_app_notify_hang(macHangDetected);
        }
      }
      break;
      
    case MC_MAN_NETWORK_EVENT_IND:
      handleNetworkEventInd(nic, payload);
      break;

    case MC_MAN_CONNECTION_EVENT_IND:
      handleNetworkConnectInd(nic, payload);
      break;

    case MC_MAN_SECURITY_ALERT_IND:
      handle_security_alert_ind(payload, nic);
      break;

    case MC_MAN_AOCS_IND:
      mtlk_aocs_indicate_event(&nic->slow_ctx->aocs, 
                               MTLK_AOCS_EVENT_TCP_IND, 
                               payload);
      break;
    case MC_MAN_PM_UPDATE_IND:
      handle_pm_update_event(nic, payload); 
      break;
    default:
      ELOG("Unknown indication type 0x%x" , (unsigned int)id);
      res = MTLK_ERR_NOT_SUPPORTED; /* Unknown IND: print error and respond on IND */
      break;
  }
 
  return res;
}

/*****************************************************************************
**
** NAME         get_firmware_version
**
** PARAMETERS   fname               Firmware file
**              data                Buffer for processing
**              size                Size of buffer
**
** RETURNS      none
**
** DESCRIPTION  Extract firmware version string of firmware file <fname> to
**              global variable <mtlk_version_string> from buffer <data> of
**              size <size>. If given <fname> already processed - skip parsing
**
******************************************************************************/
static void 
get_firmware_version (struct nic      *nic, 
                      const char      *fname, 
                      const char      *data, 
                      unsigned long    size)
{
  static const char MAC_VERSION_SIGNATURE[] = "@@@ VERSION INFO @@@";
  const char *border = data + size;

  if (strstr(mtlk_version_string, fname)) return;

  data = mtlk_utils_memchr(data, '@', border - data);
  while (data) {
    if (memcmp(data, MAC_VERSION_SIGNATURE, strlen(MAC_VERSION_SIGNATURE)) == 0) {
      char *v = mtlk_version_string + strlen(mtlk_version_string);
      sprintf(v, "%s: %s\n", fname, data);
      break;
    }
    data = mtlk_utils_memchr(data + 1, '@', border - data - 1);
  }
}

int __MTLK_IFUNC
mtlk_core_get_prop (mtlk_core_t *core, mtlk_core_prop_e prop_id, void* buffer, uint32 size)
{
  int res = MTLK_ERR_NOT_SUPPORTED;

  switch (prop_id) {
  case MTLK_CORE_PROP_MAC_SW_RESET_ENABLED:
    if (buffer && size == sizeof(uint32))
    {
      uint32 *mac_sw_reset_enabled = (uint32 *)buffer;

      *mac_sw_reset_enabled = core->slow_ctx->mac_soft_reset_enable ? 1 : 0;
      res = MTLK_ERR_OK;
    }
  break;
  default:
    break;
  }
  return res;
}

int __MTLK_IFUNC
mtlk_core_set_prop (mtlk_core_t      *nic, 
                    mtlk_core_prop_e  prop_id, 
                    void             *buffer, 
                    uint32            size)
{
  int res = MTLK_ERR_NOT_SUPPORTED;

  switch (prop_id)
  {
  case MTLK_CORE_PROP_FIRMWARE_BIN_BUFFER:
    if (buffer && size == sizeof(mtlk_core_firmware_file_t))
    {
      mtlk_core_firmware_file_t *fw_buffer = (mtlk_core_firmware_file_t *)buffer;

      get_firmware_version(nic, 
                           fw_buffer->fname, 
                           fw_buffer->content.buffer, 
                           fw_buffer->content.size);

      res = MTLK_ERR_OK;
    }
    break;
  case MTLK_CORE_PROP_MAC_STUCK_DETECTED:
    nic->slow_ctx->mac_stuck_detected_by_sw = 1;
    mtlk_core_set_net_state(nic, NET_STATE_HALTED);
    mtlk_set_hw_state(nic, MTLK_HW_STATE_APPFATAL);
    mtlk_app_notify_hang(MTLK_HW_STATE_APPFATAL);
    break;
  default:
    break;
  }

  return res;
}

int __MTLK_IFUNC
mtlk_core_stop (mtlk_core_t *nic, uint32 num_boards_alive)
{
  mtlk_hw_state_e hw_state = mtlk_core_get_hw_state(nic);
  int i;

  ILOG0(GID_CORE, "%s stop", nic->ndev->name);

#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
  if (mtlk_core_ppa_is_registered(nic)) {
    mtlk_core_ppa_unregister(nic);
  }
#endif

#ifdef MTCFG_IRB_DEBUG
  if (mtlk_irb_pinger_is_started(&nic->slow_ctx->pinger)) {
    mtlk_irb_pinger_stop(&nic->slow_ctx->pinger);
  }
#endif

  mtlk_scan_cleanup(&nic->slow_ctx->scan);

  mtlk_osal_timer_cancel_sync(&nic->slow_ctx->mac_watchdog_timer);

  mtlk_eq_stop(&nic->slow_ctx->eq);

  /*send RMMOD event to application*/
  if ((hw_state != MTLK_HW_STATE_EXCEPTION) && 
      (hw_state != MTLK_HW_STATE_APPFATAL)) {
    ILOG4(GID_CORE, "RMMOD send event");
    mtlk_app_notify_rmmod(1);
  }

  /* We must unregister device here to prevent IOCTLS
   * coming to DEAD HW
   */
  unregister_netdev(nic->ndev);

  flush_scheduled_work();

  mtlk_qos_cleanup(&nic->qos);
  aocs_cleanup(nic);

  nic->hw = NULL;

  mtlk_unregister_mib_sysfs(nic);
  mtlk_debug_procfs_cleanup(nic, num_boards_alive);
  mtlk_l2nat_cleanup(nic);
  mtlk_coc_delete(nic->slow_ctx->coc_mngmt);
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  mtlk_rf_mgmt_cleanup(&nic->rf_mgmt);
#endif
  addba_cleanup(nic);
  mtlk_mc_drop_querier(nic);
  mtlk_cache_cleanup(&nic->slow_ctx->cache);
  mtlk_cleanup_tx_limit_tables(&nic->slow_ctx->tx_limits);
  dot11h_cleanup(nic);
  flctrl_cleanup(nic);
  mtlk_clean_eeprom_data(&nic->slow_ctx->ee_data);

  for (i = 0; i < ARRAY_SIZE(nic->txmm_async_eeprom_msgs); i++) {
    mtlk_txmm_msg_cancel(&nic->txmm_async_eeprom_msgs[i]);
  }
  for (i = 0; i < ARRAY_SIZE(nic->txmm_async_msgs); i++) {
    mtlk_txmm_msg_cancel(&nic->txmm_async_msgs[i]);
  }
  for (i = 0; i < ARRAY_SIZE(nic->txdm_async_msgs); i++) {
    mtlk_txmm_msg_cancel(&nic->txdm_async_msgs[i]);
  }

  ILOG0(GID_CORE, "%s stoped", nic->ndev->name);
  return MTLK_ERR_OK;
}

void __MTLK_IFUNC
mtlk_core_delete (mtlk_core_t *nic)
{
  int i;
  mtlk_osal_timer_cleanup(&nic->slow_ctx->mac_watchdog_timer);
  mtlk_stadb_clear(&nic->slow_ctx->stadb);
#ifdef MTCFG_IRB_DEBUG
  mtlk_irb_pinger_cleanup(&nic->slow_ctx->pinger);
#endif
  for (i = 0; i < ARRAY_SIZE(nic->txmm_async_eeprom_msgs); i++) {
    mtlk_txmm_msg_cleanup(&nic->txmm_async_eeprom_msgs[i]);
  }
  for (i = 0; i < ARRAY_SIZE(nic->txmm_async_msgs); i++) {
    mtlk_txmm_msg_cleanup(&nic->txmm_async_msgs[i]);
  }
  for (i = 0; i < ARRAY_SIZE(nic->txdm_async_msgs); i++) {
    mtlk_txmm_msg_cleanup(&nic->txdm_async_msgs[i]);
  }
  sq_cleanup(nic);
  mtlk_eq_cleanup(&nic->slow_ctx->eq);
  mtlk_osal_event_cleanup(&nic->slow_ctx->connect_event);
  mtlk_free_mib_values(nic);
  kfree_tag(nic->slow_ctx->mib_value);
  kfree_tag(nic->slow_ctx);
  free_netdev(nic->ndev);
}

uint8 __MTLK_IFUNC mtlk_is_11h_radar_detection_enabled(mtlk_handle_t context)
{
    mtlk_core_t *nic = (mtlk_core_t*)(context);
    mtlk_dot11h_cfg_t s11hCfg;
    mtlk_dot11h_get_cfg(&nic->slow_ctx->dot11h, &s11hCfg);
    return s11hCfg._11h_radar_detect;
}

uint8 __MTLK_IFUNC mtlk_is_device_busy(mtlk_handle_t context)
{
    mtlk_core_t *nic = (mtlk_core_t*)(context);
    return (  mtlk_is_11h_radar_detection_enabled(context)
           && !mtlk_dot11h_can_switch_now(&nic->slow_ctx->dot11h));
}


mtlk_hw_t*__MTLK_IFUNC mtlk_core_get_hw (mtlk_core_t *nic)
{
  return nic->hw;
}

void
mtlk_find_and_update_ap(mtlk_handle_t context, uint8 *addr, bss_data_t *bss_data)
{
  uint8 channel, is_ht;
  struct nic *nic = (struct nic *)context;
  int lost_beacons;
  sta_entry *sta;
  int32 sta_id = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, addr);
  ILOG4(GID_CORE, "Trying to find AP %Y; id is %d", addr, sta_id);

  /* No updates in not connected state of for non-STA or during scan*/
  if (nic->ap ||
      (mtlk_core_get_net_state(nic) != NET_STATE_CONNECTED) ||
      mtlk_core_scan_is_running(nic))
    return;

  /* Check wrong AP */
  if (sta_id == -1) {
    nic->slow_ctx->iw_stats.discard.nwid++;
    return;
  }
  sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, sta_id);

  /* Read setings for checks */
  is_ht = nic->slow_ctx->is_ht_cur;
  channel = nic->slow_ctx->channel;

  /* Check channel change */
  if (bss_data->channel != channel) {
    ILOG0(GID_CORE, "AP %Y changed its channel! (%u -> %u)", addr, channel, bss_data->channel);
    goto DISCONNECT;
  }

  /* Check HT capabilities change (only if HT is allowed in configuration) */
  if (nic->slow_ctx->is_ht_cfg && !nic->slow_ctx->is_tkip && !nic->slow_ctx->wep_enabled &&
      (!!bss_data->is_ht != is_ht)) {
    ILOG0(GID_CORE, "AP %Y changed its HT capabilities! (%s)",
        addr, is_ht ? "HT -> non-HT" : "non-HT -> HT");
    goto DISCONNECT;
  }

  /* Update lost beacons */
  lost_beacons = mtlk_stadb_update_ap(sta, bss_data->beacon_interval);
  nic->slow_ctx->iw_stats.miss.beacon += lost_beacons;
  return;

DISCONNECT:
  ILOG1(GID_DISCONNECT, "Disconnecting AP %Y due to changed parameters", addr);
  mtlk_core_schedule_disconnect(nic);
}

uint8 mtlk_core_get_country_code (mtlk_core_t *core)
{
  return core->slow_ctx->cfg.country_code;
}

void mtlk_core_set_country_code (mtlk_core_t *core, uint8 country_code)
{
  core->slow_ctx->cfg.country_code = country_code;
}

uint8 mtlk_core_get_dot11d (mtlk_core_t *core)
{
  return core->slow_ctx->cfg.dot11d;
}

void mtlk_core_set_dot11d (mtlk_core_t *core, uint8 dot11d)
{
  core->slow_ctx->cfg.dot11d = dot11d;
}

static int
find_acl_entry (IEEE_ADDR *list, IEEE_ADDR *mac, signed int *free_entry)
{
  int i;
  signed int idx;

  idx = -1;
  for (i = 0; i < MAX_ADDRESSES_IN_ACL; i++) {
    if (mtlk_osal_compare_eth_addresses(mac->au8Addr, list[i].au8Addr) == 0) {
      idx = i;
      break;
    }
  }
  if (NULL == free_entry)
    return idx;
  /* find first free entry */
  *free_entry = -1;
  for (i = 0; i < MAX_ADDRESSES_IN_ACL; i++) {
    if (mtlk_osal_is_zero_address(list[i].au8Addr)) {
      *free_entry = i;
      break;
    }
  }
  return idx;
}

int __MTLK_IFUNC
mtlk_core_set_acl(struct nic *nic, IEEE_ADDR *mac, IEEE_ADDR *mac_mask)
{
  signed int idx, free_idx;
  IEEE_ADDR addr_tmp;

  if (mtlk_osal_is_zero_address(mac->au8Addr)) {
    ILOG2(GID_CORE, "Upload ACL list");
    return MTLK_ERR_OK;
  }

  /* Check pair MAC/MAC-mask consistency : MAC == (MAC & MAC-mask) */
  if (NULL != mac_mask) {
    mtlk_osal_eth_apply_mask(addr_tmp.au8Addr, mac->au8Addr, mac_mask->au8Addr);
    if (0 != mtlk_osal_compare_eth_addresses(addr_tmp.au8Addr, mac->au8Addr)) {
      WLOG("The ACL rule addition failed: "
           "The specified mask parameter is invalid. (MAC & MAC-Mask) != MAC.");
      return MTLK_ERR_PARAMS;
    }
  }

  idx = find_acl_entry(nic->slow_ctx->acl, mac, &free_idx);
  if (idx >= 0) {
    /* already on the list */
    ILOG2(GID_CORE, "MAC %Y is already on the ACL list at %d", mac->au8Addr, idx);
    return MTLK_ERR_OK;
  }
  if (free_idx < 0) {
    /* list is full */
    WLOG("ACL list is full");
    return MTLK_ERR_NO_RESOURCES;
  }
  /* add new entry */
  nic->slow_ctx->acl[free_idx] = *mac;
  if (NULL != mac_mask) {
    nic->slow_ctx->acl_mask[free_idx] = *mac_mask;
  } else {
    nic->slow_ctx->acl_mask[free_idx] = EMPTY_MAC_MASK;
  }

  ILOG2(GID_CORE, "Added %Y to the ACL list at %d", mac->au8Addr, free_idx);
  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_core_del_acl(struct nic *nic, IEEE_ADDR *mac)
{
  signed int idx;

  if (mtlk_osal_is_zero_address(mac->au8Addr)) {
    ILOG2(GID_CORE, "Delete ACL list");
    memset(nic->slow_ctx->acl, 0, sizeof(nic->slow_ctx->acl));
    return MTLK_ERR_OK;
  }
  idx = find_acl_entry(nic->slow_ctx->acl, mac, NULL);
  if (idx < 0) {
    /* not found on the list */
    ILOG2(GID_CORE, "MAC %Y is not on the ACL list", mac->au8Addr);
    return MTLK_ERR_PARAMS;
  }
  /* del new entry */
  nic->slow_ctx->acl[idx] = EMPTY_MAC_ADDR;
  nic->slow_ctx->acl_mask[idx] = EMPTY_MAC_MASK;

  ILOG2(GID_CORE, "Cleared %Y from the ACL list at %d", mac->au8Addr, idx);
  return MTLK_ERR_OK;
}

uint8 __MTLK_IFUNC
mtlk_core_calc_signal_strength (int8 RSSI)
{
  uint8 sig_strength = 1;

  if (RSSI > -65)
    sig_strength = 5;
  else if (RSSI > -71)
    sig_strength = 4;
  else if (RSSI > -77)
    sig_strength = 3;
  else if (RSSI > -83)
    sig_strength = 2;

  return sig_strength;
}

BOOL __MTLK_IFUNC 
mtlk_core_is_sm_channels_disabled(struct nic *nic)
{
  return nic->slow_ctx->disable_sm_channels;
}

mtlk_handle_t __MTLK_IFUNC
mtlk_core_get_tx_limits_handle(mtlk_handle_t nic)
{
  return HANDLE_T(&(((struct nic*)nic)->slow_ctx->tx_limits));
}

void __MTLK_IFUNC
mtlk_core_handle_event(struct nic* nic, EVENT_ID id, const void *payload)
{
  switch (id) {
  case EVT_SCAN_CONFIRMED:
    ILOG2(GID_CORE, "Handling scan confirm");
    mtlk_scan_handle_evt_scan_confirmed(&nic->slow_ctx->scan, payload);
    break;
  case EVT_SCAN_PAUSE_ELAPSED:
    ILOG2(GID_CORE, "Handling scan pause elapsed event");
    mtlk_scan_handle_evt_pause_elapsed(&nic->slow_ctx->scan);
    break;
  case EVT_MAC_WATCHDOG_TIMER:
    ILOG2(GID_CORE, "Handling MAC watchdog timer");
    check_mac_watchdog(nic);
    break;
  case EVT_AGGR_REVIVE_TID:
    mtlk_stadb_on_aggregation_revive_tid(payload);
    break;
  case EVT_AGGR_REVIVE_ALL:
    mtlk_stadb_on_aggregation_revive_all(payload);
    break;
  case EVT_DISCONNECT:
    ILOG2(GID_CORE, "Handling disconnect request");
    mtlk_disconnect_me(nic);
    break;
  default:
    break;
  }
}

#ifdef CONFIG_IFX_PPA_API_DIRECTPATH

uint32 mtlk_ppa_tx_sent_up = 0;
uint32 mtlk_ppa_tx_dropped = 0;

static int
_mtlk_core_ppa_start_xmit (struct net_device *rx_dev,
                           struct net_device *tx_dev,
                           struct sk_buff *skb,
                           int len)
                           
{
  if (tx_dev != NULL) {
    struct nic *nic = netdev_priv(tx_dev);
    ++nic->pstats.ppa_tx_processed;

    skb->dev = tx_dev;
    /* call wlan interface hard_start_xmit() */
    mtlk_wrap_transmit(skb, tx_dev);
  }
  else if (rx_dev != NULL)
  {
    /* as usual shift the eth header with skb->data */
    skb->protocol = eth_type_trans(skb, skb->dev);
    /* push up to protocol stacks */
    netif_rx(skb);
    ++mtlk_ppa_tx_sent_up;
  }
  else {
    dev_kfree_skb_any(skb);
    ++mtlk_ppa_tx_dropped;
  }

  return 0;
}

BOOL __MTLK_IFUNC
mtlk_core_ppa_is_available (struct nic* nic)
{
  return (ppa_hook_directpath_register_dev_fn != NULL);
}

BOOL __MTLK_IFUNC
mtlk_core_ppa_is_registered (struct nic* nic)
{
  MTLK_ASSERT(nic != NULL);
  MTLK_ASSERT(mtlk_core_ppa_is_available(nic));

  return (nic->ppa_clb.rx_fn != NULL);
}

int __MTLK_IFUNC
mtlk_core_ppa_register (struct nic* nic)
{
  int    res = MTLK_ERR_UNKNOWN;
  uint32 ppa_res;

  MTLK_ASSERT(nic != NULL);
  MTLK_ASSERT(mtlk_core_ppa_is_registered(nic) == FALSE);

  memset(&nic->ppa_clb, 0, sizeof(nic->ppa_clb));

  nic->ppa_clb.rx_fn = _mtlk_core_ppa_start_xmit;

  ppa_res = 
    ppa_hook_directpath_register_dev_fn(&nic->ppa_if_id, 
      nic->ndev, &nic->ppa_clb, 
      PPA_F_DIRECTPATH_REGISTER | PPA_F_DIRECTPATH_ETH_IF);

  if (ppa_res != IFX_SUCCESS)
  {
    nic->ppa_clb.rx_fn = NULL;
    ELOG("Can't register PPA device function (err=%d)", ppa_res);
    res = MTLK_ERR_UNKNOWN;
    goto end;
  }

  ILOG0("PPA device function is registered (id=%d)", nic->ppa_if_id);
  res = MTLK_ERR_OK;

end:
  return res;
}

void __MTLK_IFUNC
mtlk_core_ppa_unregister (struct nic* nic)
{
  uint32 ppa_res;

  MTLK_ASSERT(nic != NULL);
  MTLK_ASSERT(mtlk_core_ppa_is_registered(nic) == TRUE);

  ppa_res = ppa_hook_directpath_register_dev_fn(&nic->ppa_if_id, 
    nic->ndev, NULL, 0/*PPA_F_DIRECTPATH_DEREGISTER*/);


  if (ppa_res == IFX_SUCCESS) {
    ILOG0("PPA device function is unregistered (id=%d)", nic->ppa_if_id);
    nic->ppa_clb.rx_fn = NULL;
    nic->ppa_if_id     = 0;
  }
  else {
    ELOG("Can't unregister PPA device function (err=%d)", ppa_res);
  }
}

#endif

void __MTLK_IFUNC
mtlk_core_configuration_dump(mtlk_core_t *core)
{
  ILOG0(GID_CORE, "Country\t\t: %s", country_code_to_country(mtlk_core_get_country_code(core)));
  ILOG0(GID_CORE, "Domain\t\t: %u", country_code_to_domain(mtlk_core_get_country_code(core)));
  ILOG0(GID_CORE, "Network mode\t: %s", net_mode_to_string(core->slow_ctx->net_mode_cfg));
  ILOG0(GID_CORE, "Band\t\t: %s", mtlk_eeprom_band_to_string(net_mode_to_band(core->slow_ctx->net_mode_cfg)));
  ILOG0(GID_CORE, "Spectrum\t\t: %s MHz", core->slow_ctx->spectrum_mode ? "40": "20");
  ILOG0(GID_CORE, "Bonding\t\t: %u",  core->slow_ctx->bonding); /* "Lower"(1) : "Upper"(0)) */
  ILOG0(GID_CORE, "HT mode\t\t: %s",  core->slow_ctx->is_ht_cur ? "enabled" : "disabled");
  ILOG0(GID_CORE, "SM enabled\t: %s",  core->slow_ctx->disable_sm_channels ? "disabled" : "enabled");
}

void __MTLK_IFUNC
mtlk_core_prepare_stop(mtlk_core_t *core)
{
  ILOG1_V("Core prepare stopping....");
  spin_lock_bh(&core->net_state_lock);
  core->is_stopping = TRUE;
  spin_unlock_bh(&core->net_state_lock);

  if (NET_STATE_HALTED == mtlk_core_get_net_state(core)) {
    if (mtlk_scan_is_running(&core->slow_ctx->scan)) {
      scan_complete(&core->slow_ctx->scan);
    }
  }
  else {
    scan_terminate_and_wait_completion(&core->slow_ctx->scan);
  }
}

BOOL __MTLK_IFUNC
mtlk_core_is_stopping(mtlk_core_t *core)
{
  return (core->is_stopping || core->is_iface_stopping);
}

int __MTLK_IFUNC
mtlk_core_get_tx_power_limit(mtlk_core_t *core, uint8 *power_limit)
{
  MTLK_ASSERT(power_limit != NULL);

 if (MTLK_BFIELD_GET(core->slow_ctx->power_limit, NIC_POWER_LIMIT_IS_SET)) {
    *power_limit = (uint8)MTLK_BFIELD_GET(core->slow_ctx->power_limit, NIC_POWER_LIMIT_VAL);
    return MTLK_ERR_OK;
  }

  return MTLK_ERR_UNKNOWN;
}

int __MTLK_IFUNC
mtlk_core_change_tx_power_limit(mtlk_core_t *core, uint8 power_limit)
{
  mtlk_txmm_msg_t     man_msg;
  mtlk_txmm_data_t*   man_entry = NULL;
  UMI_TX_POWER_LIMIT *mac_msg;
  int                 res = MTLK_ERR_OK;

  ILOG4("UM_MAN_CHANGE_TX_POWER_LIMIT_REQ TxPowerLimitOption %u", power_limit);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, core->slow_ctx->hw_cfg.txmm, NULL);

  if (NULL == man_entry) {
    ELOG("Can't send UM_MAN_CHANGE_TX_POWER_LIMIT_REQ due to the lack of MAN_MSG");
    res = MTLK_ERR_NO_RESOURCES;
    goto FINISH;
  }

  man_entry->id           = UM_MAN_CHANGE_TX_POWER_LIMIT_REQ;
  man_entry->payload_size = sizeof(UMI_TX_POWER_LIMIT);
  mac_msg = (UMI_TX_POWER_LIMIT *)man_entry->payload;

  memset(mac_msg, 0, sizeof(*mac_msg));

  mac_msg->TxPowerLimitOption = power_limit;

  DUMP3(man_entry->payload, sizeof(UMI_TX_POWER_LIMIT), "UM_MAN_CHANGE_TX_POWER_LIMIT_REQ dump:");

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG("Error sending UM_MAN_CHANGE_TX_POWER_LIMIT_REQ (%d)", res);
    goto FINISH;
  }

  ILOG2("UM_MAN_CHANGE_TX_POWER_LIMIT_REQ: Limit(%u) Status(%u)", (int)power_limit, (int)mac_msg->Status);
  if (mac_msg->Status != UMI_OK) {
    ELOG("FW refused to set Power Limit (%u) with status (%u)", (int)power_limit, (int)mac_msg->Status);
    res = MTLK_ERR_FW;
    goto FINISH;
  }

  core->slow_ctx->power_limit = 
    MTLK_BFIELD_VALUE(NIC_POWER_LIMIT_VAL, power_limit, uint16) |
    MTLK_BFIELD_VALUE(NIC_POWER_LIMIT_IS_SET, 1, uint16);

FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return res;
}

int __MTLK_IFUNC
mtlk_core_set_bonding(mtlk_core_t *core, uint8 bonding)
{
  int result = MTLK_ERR_OK;

  if ((bonding == ALTERNATE_UPPER) || (bonding == ALTERNATE_LOWER))
  {
    ILOG1("Bonding changed to %s", bonding == ALTERNATE_UPPER ? "upper" : "lower");
    core->slow_ctx->bonding = core->slow_ctx->pm_params.u8UpperLowerChannel = bonding;
  } else {
    result = MTLK_ERR_PARAMS;
  }
  return result;
}

uint8 __MTLK_IFUNC
mtlk_core_get_bonding(mtlk_core_t *core)
{
  return core->slow_ctx->bonding;
}
