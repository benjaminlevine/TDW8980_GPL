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
 * Debug API.
 *
 * Written by Andrey Fidrya
 *
 */
#include "mtlkinc.h"

#include <linux/module.h>
#include <linux/sysctl.h>
#include <linux/netdevice.h>
#include <asm/uaccess.h>

#include "compat.h"
#include "utils.h"
#include "debug.h"
#include "mhi_statistics.h"
#include "core.h"
#include "proc_aux.h"
#include "l2nat.h"
#include "aocs.h"
#include "aocs_osdep.h"
#include "aocshistory_osdep.h"
#include "sq.h"

#define MAX_STAT_NAME_LENGTH 256

#define DEFAULT_PKTCAP_QUEUE_MAX_SIZE 65536

struct stat_print_info_t
{
  int idx;
  const char *name;
};

struct stat_print_info_t stat_info[] =
{
  { STAT_RX_UNICAST_DATA,    "Unicast data frames received" },
  { STAT_RX_DUPLICATE,       "Duplicate frames received" },
  { STAT_RX_MULTICAST_DATA,  "Multicast frames received" },
  { STAT_RX_DISCARD,         "Frames Discarded" },
  { STAT_RX_UNKNOWN,         "Unknown RX" },
  { STAT_RX_UNAUTH,          "Reception From Unauthenticated STA" },
  { STAT_RX_UNASSOC,         "AP: Frames RX from Unassociated STA" },
  { STAT_RX_INTS,            "RX Interrupts" },
  { STAT_RX_CONTROL,         "RX Control type frames" },

  { STAT_RX_TOTAL_MANAGMENT_PACKETS,  "Total management packets received" },
  { STAT_TX_TOTAL_MANAGMENT_PACKETS,  "Total management packets transmitted" },
  { STAT_RX_TOTAL_DATA_PACKETS,       "Total packets received" },

  { STAT_BEACON_TX,          "Beacons Sent" },
  { STAT_BEACON_RX,          "Beacons Received" },
  { STAT_AUTHENTICATE_TX,    "Authentication Requests Sent" },
  { STAT_AUTHENTICATE_RX,    "Authentication Requests Received" },

  { STAT_ASSOC_REQ_TX,       "Association Requests Sent" },
  { STAT_ASSOC_REQ_RX,       "Association Requests Received" },
  { STAT_ASSOC_RES_TX,       "Association Replies Sent" },
  { STAT_ASSOC_RES_RX,       "Association Replies Received" },

  { STAT_REASSOC_REQ_TX,     "ReAssociation Requests Sent" },
  { STAT_REASSOC_REQ_RX,     "ReAssociation Requests Received" },
  { STAT_REASSOC_RES_TX,     "ReAssociation Replies Sent" },
  { STAT_REASSOC_RES_RX,     "ReAssociation Replies Received" },

  { STAT_DEAUTH_TX,          "Deauthentication Notifications Sent" },
  { STAT_DEAUTH_RX,          "Deauthentication Notifications Received" },

  { STAT_DISASSOC_TX,        "Disassociation Notifications Sent" },
  { STAT_DISASSOC_RX,        "Disassociation Notifications Received" },

  { STAT_PROBE_REQ_TX,       "Probe Requests sent" },
  { STAT_PROBE_REQ_RX,       "Probe Requests received" },
  { STAT_PROBE_RES_TX,       "Probe Responses sent" },
  { STAT_PROBE_RES_RX,       "Probe Responses received" },

  { STAT_ATIM_TX,            "ATIMs Transmitted successfully" },
  { STAT_ATIM_RX,            "ATIMs Received" },
  { STAT_ATIM_TX_FAIL,       "ATIMs Failing transmission" },

  { STAT_TX_MSDU,            "TX MSDUs that have been sent" },

  { STAT_TX_FAIL,            "TX frames that have failed" },
  { STAT_TX_RETRY,           "TX retries to date" },
  { STAT_TX_DEFER_PS,        "Transmits deferred due to Power Mgmnt" },
  { STAT_TX_DEFER_UNAUTH,    "Transmit deferred pending authentication" },

  { STAT_BEACON_TIMEOUT,     "Beacon Timeouts" },
  { STAT_AUTH_TIMEOUT,       "Authentication Timeouts" },
  { STAT_ASSOC_TIMEOUT,      "Association Timeouts" },
  { STAT_ROAM_SCAN_TIMEOUT,  "Roam Scan timeout" },

  { STAT_WEP_TOTAL_PACKETS,  "Total number of packets passed through WEP" },
  { STAT_WEP_EXCLUDED,       "Unencrypted packets received when WEP is active" },
  { STAT_WEP_UNDECRYPTABLE,  "Packets with no valid keys for decryption " },
  { STAT_WEP_ICV_ERROR,      "Packets with incorrect WEP ICV" },
  { STAT_TX_PS_POLL,         "TX PS POLL" },
  { STAT_RX_PS_POLL,         "RX PS POLL" },

  { STAT_MAN_ACTION_TX,      "Management Actions sent" },
  { STAT_MAN_ACTION_RX,      "Management Actions received" },

  { STAT_OUT_OF_RX_MSDUS,    "Out of RX MSDUs" },

  { STAT_HOST_TX_REQ,        "Requests from PC to TX data - UM_DAT_TXDATA_REQ" },
  { STAT_HOST_TX_CFM,        "Confirm to PC by MAC of TX data - MC_DAT_TXDATA_CFM" },
  { STAT_BSS_DISCONNECT,     "Station remove from database due to beacon/data timeout" },

  { STAT_RX_DUPLICATE_WITH_RETRY_BIT_0, "Duplicate frames received with retry bit set to 0" },

  { STAT_RX_NULL_DATA,       "Total number of received NULL DATA packets" },
  { STAT_TX_NULL_DATA,       "Total number of sent NULL DATA packets" },

  { STAT_RX_BAR,             "BAR received" },
  { STAT_TX_BAR,             "BAR sent" },
  { STAT_TX_BAR_FAIL,        "BAR fail" },

  { STAT_RX_FAIL_NO_DECRYPTION_KEY, "RX Failures due to no key loaded" },
  { STAT_RX_DECRYPTION_SUCCESSFUL,  "RX decryption successful" },

  { STAT_NUM_UNI_PS_INACTIVE,       "Unicast packets in PS-Inactive queue" },
  { STAT_NUM_MULTI_PS_INACTIVE,     "Multicast packets in PS-Inactive queue" },
  { STAT_TOT_PKS_PS_INACTIVE,       "Total number of packets in PS-Inactive queue" },
  { STAT_NUM_MULTI_PS_ACTIVE,       "Multicast packets in PS-Active queue" },
  { STAT_NUM_TIME_IN_PS,            "Time in power-save" },

  { STAT_WDS_TX_UNICAST,            "Unicast WDS frames transmitted" },
  { STAT_WDS_TX_MULTICAST,          "Multicast WDS frames transmitted" },
  { STAT_WDS_RX_UNICAST,            "Unicast WDS frames received" },
  { STAT_WDS_RX_MULTICAST,          "Multicast WDS frames received" },

  { STAT_CTS2SELF_TX,       "CTS2SELF packets that have been sent" },
  { STAT_CTS2SELF_TX_FAIL,  "CTS2SELF packets that have failed" },
  { STAT_DECRYPTION_FAILED, "frames with decryption failed" },
  { STAT_FRAGMENT_FAILED,   "frames with wrong fragment number" },
  { STAT_TX_MAX_RETRY,      "TX dropped packets with retry limit exceeded" },

  { STAT_TX_RTS_SUCCESS,    "RTS succeeded" },
  { STAT_TX_RTS_FAIL,       "RTS failed" },
  { STAT_TX_MULTICAST_DATA, "transmitted multicast frames" },
  { STAT_FCS_ERROR,         "FCS errors" },

  { STAT_RX_ADDBA_REQ,      "Received ADDBA Request frames" },
  { STAT_RX_ADDBA_RES,      "Received ADDBA Response frames" },
  { STAT_RX_DELBA_PKT,      "Deceived DELBA frames" },

};

#if 0
struct print_stat_params
{
  void *usp_buf;
  int usp_at;
  size_t usp_space_left;
};
#endif

struct bufferize_stat_params
{
  char *pbuffer;
  int at;
  size_t space_left;
};

struct count_stat_params
{
  int num_stats;
};

#define FETCH_NAME 1
#define FETCH_VAL 2

struct fetch_stat_params
{
  int index_search;
  int index_cur;
  int what;
  char name[MAX_STAT_NAME_LENGTH];
  unsigned long val;
};

#define DLT_IEEE802_11 105

/*****************************************************************************
**
** NAME         mtlk_debug_reset_counters
**
** PARAMETERS   nic           Card context
**
** RETURNS      none
**
** DESCRIPTION  Resets debug counters to their default values. This function
**              does not reset the MAC counters. To reset them, use
**              mac_reset_stats.
**
******************************************************************************/
void
mtlk_debug_reset_counters (struct nic *nic)
{
  if (nic->slow_ctx->hw_cfg.ap != 0) {
    // AP
    sta_entry *sta;
    int i;

    for (i = 0; i < STA_MAX_STATIONS; i++)
    {
      sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, i);
      if (sta->state == PEER_UNUSED)
        continue;
  
      memset(&sta->stats, 0, sizeof(sta_stats));
    }
  }

  // For both AP & STA:
  ASSERT(sizeof(nic->stats) == sizeof(struct net_device_stats));
  memset(&nic->stats, 0, sizeof(nic->stats));

  ASSERT(sizeof(nic->pstats) == sizeof(struct priv_stats));
  memset(&nic->pstats, 0, sizeof(nic->pstats));

#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
  mtlk_ppa_tx_sent_up = 0;
  mtlk_ppa_tx_dropped = 0;
#endif
}

static void debug_reset_all_counters (struct nic *nic)
{
  if (mtlk_core_get_net_state(nic) != NET_STATE_HALTED)
    return;
  mtlk_debug_reset_counters(nic);
  mac_reset_stats(nic);
}

#if 0
static int print_stat(void *params, unsigned long val, const char *str)
{
  char buf[MAX_STAT_NAME_LENGTH];
  const int max_buf_len = ARRAY_SIZE(buf);
  struct print_stat_params *ppsp = (struct print_stat_params *) params;

  if (0 != copy_zstr_to_userspace_fmt(buf, max_buf_len,
      ppsp->usp_buf, &ppsp->usp_at, &ppsp->usp_space_left, "%10lu %s\n",
      val, str))
  {
    return 1;
  }
  return 0;
}
#endif

static int bufferize_stat(void *params, unsigned long val, const char *str)
{
  struct seq_file *s = params;

  seq_printf(s, "%10lu %s\n", val, str);
  return 0;
}

static int count_stat(void *params, unsigned long val, const char* str)
{
  struct count_stat_params *pcsp = (struct count_stat_params *) params;
  ++pcsp->num_stats;
  return 0;
}

static int fetch_stat(void *params, unsigned long val, const char *str)
{
  struct fetch_stat_params *pfsp = (struct fetch_stat_params *) params;
  int rslt = 0;
  
  if (pfsp->index_cur == pfsp->index_search) {
    if (pfsp->what == FETCH_VAL)
      pfsp->val = val;

    else if (pfsp->what == FETCH_NAME) {
      int rslt = snprintf(pfsp->name, MAX_STAT_NAME_LENGTH, "%s", str);
      if (rslt < 0 || rslt >= MAX_STAT_NAME_LENGTH)
        rslt = 1;

    } else {
      rslt = 1;
    }
  }
  ++pfsp->index_cur;
  return 0;
}

static int iterate_driver_stats(struct nic *nic,
    int (* fn)(void *params, unsigned long val, const char* str),
    void *params)
{
  int i;
  char buf[MAX_STAT_NAME_LENGTH];
  uint32 bist;

  if (0 != fn(params, nic->pstats.rx_dat_frames,
        "data frames received"))
    return 1;
  if (0 != fn(params, nic->pstats.rx_ctl_frames,
        "control frames received"))
    return 1;
  if (0 != fn(params, nic->pstats.rx_man_frames,
        "management frames received"))
    return 1;
  if (0 != fn(params, nic->stats.tx_dropped,
        "TX packets dropped"))
    return 1;
  if (0 != fn(params, nic->pstats.tx_max_cons_drop,
        "TX maximum consecutive dropped packets"))
    return 1;

  for (i = 0; i < NTS_PRIORITIES; i++) {
    sprintf(buf, "MSDUs received, QoS priority %d", i);
    if (0 != fn(params, nic->pstats.ac_rx_counter[i], buf))
      return 1;
  }

  for (i = 0; i < NTS_PRIORITIES; i++) {
    sprintf(buf, "MSDUs transmitted, QoS priority %d", i);
    if (0 != fn(params, nic->pstats.ac_tx_counter[i], buf))
      return 1;
  }

  for (i = 0; i < NTS_PRIORITIES; i++) {
    sprintf(buf, "MSDUs dropped, QoS priority %d", i);
    if (0 != fn(params, nic->pstats.ac_dropped_counter[i], buf))
      return 1;
  }

  for (i = 0; i < NTS_PRIORITIES; i++) {
    sprintf(buf, "MSDUs used, QoS priority %d", i);
    if (0 != fn(params, nic->pstats.ac_used_counter[i], buf))
      return 1;
  }

  {
    uint32 n = 0;
    mtlk_hw_get_prop(nic->hw, MTLK_HW_FREE_TX_MSGS, (void *)&n, sizeof(n));
    if (0 != fn(params, n, "TX MSDUs free"))
      return 1;
  }
  {
    uint32 n = 0;
    mtlk_hw_get_prop(nic->hw, MTLK_HW_TX_MSGS_USED_PEAK, (void *)&n, sizeof(n));
    if (0 != fn(params, n, "TX MSDUs usage peak"))
      return 1;
  }

  if (0 != fn(params, nic->pstats.fwd_rx_packets,
        "packets received that should be forwarded to one or more STAs"))
    return 1;
  if (0 != fn(params, nic->pstats.fwd_rx_bytes,
        "bytes received that should be forwarded to one or more STAs"))
    return 1;
  if (0 != fn(params, nic->pstats.fwd_tx_packets,
        "packets transmitted for forwarded data"))
    return 1;
  if (0 != fn(params, nic->pstats.fwd_tx_bytes,
        "bytes transmitted for forwarded data"))
    return 1;
  if (0 != fn(params, nic->pstats.fwd_dropped,
        "forwarding (transmission) failures"))
    return 1;
  if (0 != fn(params, nic->pstats.rmcast_dropped,
        "reliable multicast (transmission) failures"))
    return 1;
  if (0 != fn(params, nic->pstats.replays_cnt,
        "packets replayed"))
    return 1;
  if (0 != fn(params, nic->pstats.bars_cnt,
        "BAR frames received"))
    return 1;

  mtlk_hw_get_prop(nic->hw, MTLK_HW_BIST, (void *)&bist, sizeof(bist));
  if (0 != fn(params, bist, "BIST check passed"))
    return 1;

#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
  if (0 != fn(params, nic->pstats.ppa_tx_processed,
        "TX Frames processed from PPA"))
    return 1;
  if (0 != fn(params, mtlk_ppa_tx_sent_up,
        "TX Frames sent up from PPA"))
    return 1;
  if (0 != fn(params, mtlk_ppa_tx_dropped,
        "TX Frames dropped from PPA"))
    return 1;
  if (0 != fn(params, nic->pstats.ppa_rx_accepted,
        "RX Frames accepted by PPA"))
    return 1;
  if (0 != fn(params, nic->pstats.ppa_rx_rejected,
        "RX Frames rejected by PPA"))
    return 1;
#endif

  return 0;
}

void debug_show_general(struct seq_file *s, void *v, mtlk_core_t *nic)
{
  unsigned long total_rx_packets;
  unsigned long total_tx_packets;
  unsigned long total_rx_dropped;
  int i, j;
  sta_entry *sta;
  host_entry *host;

  seq_printf(s, "\n"
      "Driver Statistics\n"
      "\n"
      "------------------+------------------+--------------------------------------\n"
      "MAC               | Packets received | Packets sent     | Rx packets dropped\n"
      "------------------+------------------+--------------------------------------\n");

  total_rx_packets = 0;
  total_tx_packets = 0;
  total_rx_dropped = 0;

  for (i = 0; i < STA_MAX_STATIONS; i++) {
    sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, i);
    if (sta->state == PEER_UNUSED)
      continue;

    total_rx_packets += sta->stats.rx_packets;
    total_tx_packets += sta->stats.tx_packets;
    total_rx_dropped += sta->stats.rx_dropped;
    seq_printf(s, MAC_FMT " | %-16u | %-16u | %-16u\n", MAC_ARG(sta->mac),
      sta->stats.rx_packets, sta->stats.tx_packets, sta->stats.rx_dropped);

    if (nic->slow_ctx->hw_cfg.ap && (nic->bridge_mode == BR_MODE_WDS)) { // Print per-STA WDS hosts on AP
      seq_printf(s, "   STA's WDS hosts:\n");

      for (j = 0; j < STA_MAX_HOSTS; j++) {
        host = mtlk_stadb_get_host_by_id(&nic->slow_ctx->stadb, j);
        if (host->state == PEER_UNUSED)
          continue;
        if (host->sta_id != i)
          continue;
        seq_printf(s, MAC_FMT "\n", MAC_ARG(host[j].mac));
      }
    }

    seq_printf(s,"------------------+------------------+--------------------------------------\n");
  }

  if (nic->slow_ctx->hw_cfg.ap == 0 && (nic->bridge_mode == BR_MODE_WDS)) { // Print all WDS hosts on STA
    seq_printf(s, "   All WDS hosts connected to this STA\n");
     
    for (j = 0; j < STA_MAX_HOSTS; j++) {
      host = mtlk_stadb_get_host_by_id(&nic->slow_ctx->stadb, j);
      if (host->state == PEER_UNUSED)
        continue;
      seq_printf(s, MAC_FMT "\n", MAC_ARG(host[j].mac));
    }

    seq_printf(s,"------------------+------------------+--------------------------------------\n");
  }

  seq_printf(s,
      "Total             | %-16lu | %-16lu | %lu\n"
      "------------------+------------------+--------------------------------------\n"
      "Unicast           | %-16lu | %-16lu |\n"
      "Broadcast         | %-16lu | %-16lu |\n"
      "Multicast         | %-16lu | %-16lu |\n"
      "------------------+------------------+--------------------------------------\n"
      "\n"
      , total_rx_packets, total_tx_packets, total_rx_dropped,
      total_rx_packets - nic->pstats.rx_bcast - nic->pstats.rx_nrmcast,
      total_tx_packets,
      (unsigned long)nic->pstats.rx_bcast,
      (unsigned long)nic->pstats.tx_bcast,
      (unsigned long)nic->pstats.rx_nrmcast,
      (unsigned long)nic->pstats.tx_nrmcast);

  if (0 != iterate_driver_stats(nic, &bufferize_stat, s)) {
    ERROR_DRV_STATS_ITER();
  }
  return;
}

void debug_show_rr_stats (struct seq_file *s, void *v, mtlk_core_t *nic)
{
  int stn, tid;

  seq_printf(s, "\n"
      "\n"
      "Reordering Statistics\n"
      "\n"
      "------------------+----+------------+------------+------------+------------+------------\n"
      "MAC               | ID | Too old    | Duplicate  | Queued     | Overflows  | Lost       \n"
      "------------------+----+------------+------------+------------+------------+------------\n");

  for (stn = 0; stn < STA_MAX_STATIONS; stn++) {
    sta_entry *sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, stn);

    if (sta->state == PEER_UNUSED)
      continue;

    //FIXME: No lock taken during operations with reordering stats.
    //       Is it a problem?
    for (tid = 0; tid < NTS_TIDS; tid++) {
      const reordering_stats *pstats = mtlk_get_rod_stats(&sta->rod_queue[tid]);

      if(NULL == pstats)
          continue;

      seq_printf(s, MAC_FMT " | %-2d | %-10u | %-10u | %-10u | %-10u | %-10u\n",
          MAC_ARG(sta->mac),
          tid,
          pstats->too_old,
          pstats->duplicate,
          pstats->queued,
          pstats->overflows,
          pstats->lost);
    }
  }

  seq_printf(s,
      "------------------+----+------------+------------+------------+------------+------------\n");
  return;
}

void debug_show_mac_stats (struct seq_file *s, void *v, mtlk_core_t *nic)
{
  mtlk_txmm_msg_t     man_msg;
  UMI_GET_STATISTICS *pstats;
  int                 i;

  if (mtlk_core_get_net_state(nic) == NET_STATE_HALTED)
  {
    seq_printf(s, "Driver not activated - cannot read MAC statistics.\n");
    goto end;
  }

  if (mtlk_txmm_msg_init(&man_msg) != MTLK_ERR_OK) {
    ELOG("Can't init MM");
    goto end;
  }

  pstats = mac_get_stats(nic, &man_msg);
  if (!pstats) {
    goto get_failed;
  }

  seq_printf(s, "\nMAC Statistics\n\n");

  for (i = 0; i < sizeof(stat_info) / sizeof(stat_info[0]); ++i) {
    seq_printf(s, "%10u %s\n"
        , le32_to_cpu(pstats->sStats.au32Statistics[stat_info[i].idx])
        , stat_info[i].name);
  }
  
get_failed:
  mtlk_txmm_msg_cleanup(&man_msg);
end:
  return;
}

#ifdef MTCFG_CPU_STAT

static void cpu_stats_show(struct seq_file *s, void *v, mtlk_core_t *nic)
{
  int i;

  seq_printf(s, 
             "\n"
             "CPU Utilization Statistics (measurement unit is 'us')\n"
             "\n"
             "-------------+-------------+-------------+-------------+-------------+\n"
             " Count       | Average     | Peek        | Peek SN     | Name\n"
             "-------------+-------------+-------------+-------------+-------------+\n");

  _CPU_STAT_FOREACH_TRACK_IDX(i) {
    cpu_stat_node_t node;
    char name[32];
    uint64 avg_time_us;

    _CPU_STAT_GET_DATA(i,
                       &node);

    if (!node.count)
      continue;

    _CPU_STAT_GET_NAME_EX(i, name, sizeof(name));

    /* NOTE: 64-bit division is not supported by default in Linux kernel space =>
     *       we should use the do_div() ASM macro here.
     */
    avg_time_us = node.total;
    do_div(avg_time_us, node.count); /* the result is stored in avg_delay_us */

    seq_printf(s, 
               " %-12u| %-12u| %-12u| %-12u| %s %s\n",
               (uint32)node.count,
               (uint32)avg_time_us,
               (uint32)node.peak,
               (uint32)node.peak_sn,
               name,
               _CPU_STAT_IS_ENABLED(i)?"[*]":"");
  }

  seq_printf(s, 
             "-------------+-------------+-------------+-------------+-------------+\n");
}

#endif /* MTCFG_CPU_STAT */

/* here goes .seq_show wrappers. they are basically get the
 * pointer to mtlk_core_t structure and call function which 
 * generates the output of the file.
 */
#ifdef MTCFG_ENABLE_OBJPOOL
struct debug_mem_alloc_dump_ctx
{
  struct seq_file *s;
  char             buf[512];
};

static int __MTLK_IFUNC
_debug_mem_alloc_printf (mtlk_handle_t printf_ctx,
                         const char   *format,
                         ...)
{
  int                              res;
  va_list                          valst;
  struct debug_mem_alloc_dump_ctx *ctx = 
    HANDLE_T_PTR(struct debug_mem_alloc_dump_ctx, printf_ctx);

  va_start(valst, format);
  res = vsnprintf(ctx->buf, sizeof(ctx->buf), format, valst);
  va_end(valst);

  seq_printf(ctx->s, "%s\n", ctx->buf);

  return res;
}

static int debug_mem_alloc_dump(struct seq_file *s, void *v)
{
  struct debug_mem_alloc_dump_ctx *ctx = 
    (struct debug_mem_alloc_dump_ctx *)vmalloc_tag(sizeof(*ctx),
                                                   MTLK_MEM_TAG_DEBUG_DATA);

  if (ctx) {
    ctx->s = s;

    mem_leak_dbg_print_allocators_info(_debug_mem_alloc_printf,
                                       HANDLE_T(ctx));
    vfree_tag(ctx);
    return 0;
  }

  return -EAGAIN;
}
#endif

static int debug_general_seq_show(struct seq_file *s, void *v)
{
  mtlk_core_t *nic = (container_of(s->op, struct mtlk_seq_ops, seq_ops))->nic;

  debug_show_general(s, v, nic);

  return 0;
}

static int mac_stats_seq_show(struct seq_file *s, void *v)
{
  mtlk_core_t *nic = (container_of(s->op, struct mtlk_seq_ops, seq_ops))->nic;

  debug_show_mac_stats(s, v, nic);

  return 0;
}

static int rr_stats_seq_show(struct seq_file *s, void *v)
{
  mtlk_core_t *nic = (container_of(s->op, struct mtlk_seq_ops, seq_ops))->nic;

  debug_show_rr_stats(s, v, nic);
  return 0;
}

static int l2nat_stats_seq_show(struct seq_file *s, void *v)
{
  mtlk_core_t *nic = (container_of(s->op, struct mtlk_seq_ops, seq_ops))->nic;

  mtlk_l2nat_show_stats(s, v, nic);
  return 0;
}

static int sq_stats_seq_show(struct seq_file *s, void *v)
{
  mtlk_core_t *nic = (container_of(s->op, struct mtlk_seq_ops, seq_ops))->nic;

  mtlk_sq_show_stats(s, v, nic);
  return 0;
}

static int debug_aocs_table(struct seq_file *s, void *v)
{
  mtlk_core_t *nic = (container_of(s->op, struct mtlk_seq_ops, seq_ops))->nic;
  mtlk_aocs_print_table(&nic->slow_ctx->aocs, s); 
  return 0;
}

static int debug_aocs_channels(struct seq_file *s, void *v)
{
  mtlk_core_t *nic = (container_of(s->op, struct mtlk_seq_ops, seq_ops))->nic;
  mtlk_aocs_print_channels(&nic->slow_ctx->aocs, s);
  return 0;
}

static int debug_aocs_penalties(struct seq_file *s, void *v)
{
  mtlk_core_t *nic = (container_of(s->op, struct mtlk_seq_ops, seq_ops))->nic;
  mtlk_aocs_print_penalties(&nic->slow_ctx->aocs, s);
  return 0;
}

static int debug_aocs_history(struct seq_file *s, void *v)
{
    mtlk_core_t *nic = (container_of(s->op, struct mtlk_seq_ops, seq_ops))->nic;
    mtlk_aocs_history_print(&nic->slow_ctx->aocs.aocs_history, s);
    return 0;
}

static int debug_hw_limits(struct seq_file *s, void *v)
{
  mtlk_core_t *nic = (container_of(s->op, struct mtlk_seq_ops, seq_ops))->nic;
  mtlk_print_tx_limit_table(&nic->slow_ctx->tx_limits, s, HW_LIMITS);
  return 0;
}

static int debug_reg_limits(struct seq_file *s, void *v)
{
  mtlk_core_t *nic = (container_of(s->op, struct mtlk_seq_ops, seq_ops))->nic;
  mtlk_print_tx_limit_table(&nic->slow_ctx->tx_limits, s, REG_LIMITS);
  return 0;
}

static int debug_ant_gain(struct seq_file *s, void *v)
{
  mtlk_core_t *nic = (container_of(s->op, struct mtlk_seq_ops, seq_ops))->nic;
  mtlk_print_tx_limit_table(&nic->slow_ctx->tx_limits, s, ANTENNA_GAIN);
  return 0;
}

/* this function is registered as .write_proc
 * for per-card "reset" file PDE
 */
static int mtlk_debug_do_reset(struct file *file, const char *buffer,
                                         unsigned long count, void *data)
{
  struct nic *nic = data;

  debug_reset_all_counters(nic);
  /* let it looks as all data has
   * been written succesfully
   */
  return count;
}

static int l2nat_clear_table(struct file *file, const char *buffer,
                                         unsigned long count, void *data)
{
  struct nic *nic = data;

  mtlk_l2nat_clear_table(nic);
  /* let it looks as all data has
   * been written succesfully
   */
  return count;
}

enum BCL_UNIT_TYPE {
  BCL_UNIT_INT_RAM = 0,
  BCL_UNIT_AFE_RAM,
  BCL_UNIT_RFIC_RAM,
  BCL_UNIT_EEPROM,
  BCL_UNIT_MAX = 10
};

static int __MTLK_IFUNC
proc_bcl_read (char *page, char **start, off_t off, int count, int *eof,
  void *data, int io_base, int io_size)
{
  struct nic *nic = (struct nic*)data;
  UMI_BCL_REQUEST req;

  if (off >= io_size) {
    *eof = 1;
    return 0;
  }

  if ((off & (sizeof(req.Data) - 1)) || (count < sizeof(req.Data)))
    return -EIO;

  count = sizeof(req.Data);

  req.Unit = BCL_UNIT_INT_RAM;
  req.Size = count;
  req.Address = io_base + off;
  memset(req.Data, 0x5c, sizeof(req.Data)); /* poison */
  mtlk_debug_bswap_bcl_request(&req, TRUE);

  ILOG1(GID_DEBUG, "BCL read %04x@%08lx", count, io_base + off);

  if (mtlk_hw_get_prop(nic->hw, MTLK_HW_BCL_ON_EXCEPTION, &req, sizeof(req)) != MTLK_ERR_OK)
    return -EIO;

  memcpy(page, req.Data, count);
  *start = (char*)sizeof(req.Data);

  return count;
}

#define UM_DATA_BASE    0x80000000
#define UM_DATA_SIZE    0x00037d00
#define LM_DATA_BASE    0x80080000
#define LM_DATA_SIZE    0x0001fd00
#define SHRAM_DATA_BASE 0xa6000000
#define SHRAM_DATA_SIZE 0x00020000

static int __MTLK_IFUNC
proc_bcl_read_um (char *page, char **start, off_t off, int count, int *eof, void *data)
{
  return proc_bcl_read(page, start, off, count, eof, data, UM_DATA_BASE, UM_DATA_SIZE);
}

static int __MTLK_IFUNC
proc_bcl_read_lm (char *page, char **start, off_t off, int count, int *eof, void *data)
{
  return proc_bcl_read(page, start, off, count, eof, data, LM_DATA_BASE, LM_DATA_SIZE);
}

static int __MTLK_IFUNC
proc_bcl_read_shram (char *page, char **start, off_t off, int count, int *eof, void *data)
{
  return proc_bcl_read(page, start, off, count, eof, data, SHRAM_DATA_BASE, SHRAM_DATA_SIZE);
}


static int dbg_num_connected(struct nic *nic)
{
  int i;
  int num_connected = 0;
  if (nic->slow_ctx->hw_cfg.ap != 0) {
    for (i = 0; i < STA_MAX_STATIONS; i++)
    {
      if (mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, i)->state == PEER_UNUSED)
        continue;
      ++num_connected;
    }
  } else {
    if (nic->slow_ctx->connected != 0)
      ++num_connected;
  }
  return num_connected;
}

static int bcl_prepare_pkt_counters(struct nic *nic, uint32 *pcnt)
{
  const char hdr[] = "MAC|Packets received|Packets sent";
  const int num_cnts = str_count(hdr, '|');
  
  const char ftr[] = "Total";
  char **dbg_general_pkt_text = NULL;
  uint32 *dbg_general_pkt_cnts = NULL;
  uint32 total_rx_packets;
  uint32 total_tx_packets;
  char buf[128];
  int num_connected;
  int text_num_entries, text_cur_entry;
  int cnt_num_entries, cnts_cur_entry;
  int i;
  
  num_connected = dbg_num_connected(nic);

  text_num_entries = 1 // Header
      + num_connected // Entries
      + 1 // Footer
      + 1; // NULL terminator
  dbg_general_pkt_text = kmalloc_tag(text_num_entries * sizeof(char *),
      GFP_KERNEL, MTLK_MEM_TAG_DEBUG_DATA);
  if (!dbg_general_pkt_text) {
    ERROR_OUT_OF_MEMORY();
    goto err_clean;
  }
  memset(dbg_general_pkt_text, 0, text_num_entries * sizeof(char *));
  
  cnt_num_entries = num_cnts * (num_connected + 1); // +1 for Totals
  dbg_general_pkt_cnts = kmalloc_tag(
      cnt_num_entries*sizeof(uint32),
      GFP_KERNEL, MTLK_MEM_TAG_DEBUG_DATA);
  if (!dbg_general_pkt_cnts)
  {
    ERROR_OUT_OF_MEMORY();
    goto err_clean;
  }

  text_cur_entry = 0;

  dbg_general_pkt_text[text_cur_entry] = kmalloc_tag(sizeof(hdr),
    GFP_KERNEL, MTLK_MEM_TAG_DEBUG_DATA);
  if (!dbg_general_pkt_text[text_cur_entry])
  {
    ERROR_OUT_OF_MEMORY();
    goto err_clean;
  }
  memcpy(dbg_general_pkt_text[text_cur_entry], hdr, sizeof(hdr));
  ++text_cur_entry;
  
  cnts_cur_entry = 0;
  total_rx_packets = 0;
  total_tx_packets = 0;
 
  if (nic->slow_ctx->hw_cfg.ap != 0) {
    // AP
    sta_entry *sta;
    
    for (i = 0; i < STA_MAX_STATIONS; i++)
    {
      sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, i);

      if (sta->state == PEER_UNUSED)
        continue;
      total_rx_packets += sta->stats.rx_packets;
      total_tx_packets += sta->stats.tx_packets;
      
      sprintf(buf, MAC_FMT, MAC_ARG(sta->mac));
      dbg_general_pkt_text[text_cur_entry] = kmalloc_tag(strlen(buf) + 1,
          GFP_KERNEL, MTLK_MEM_TAG_DEBUG_DATA);
      if (!dbg_general_pkt_text[text_cur_entry]) {
        ERROR_OUT_OF_MEMORY();
        goto err_clean;
      }
      strcpy(dbg_general_pkt_text[text_cur_entry], buf);
      ++text_cur_entry;
      
      ASSERT(cnts_cur_entry < cnt_num_entries);
      dbg_general_pkt_cnts[cnts_cur_entry++] =
          sta->stats.rx_packets;

      ASSERT(cnts_cur_entry < cnt_num_entries);
      dbg_general_pkt_cnts[cnts_cur_entry++] =
          sta->stats.tx_packets;
    }
  } else {
    // STA
    if (nic->slow_ctx->connected != 0)
    {
      total_rx_packets = nic->pstats.sta_session_rx_packets;
      total_tx_packets = nic->pstats.sta_session_tx_packets;

      sprintf(buf, MAC_FMT, MAC_ARG(nic->slow_ctx->bssid));
      dbg_general_pkt_text[text_cur_entry] = kmalloc_tag(strlen(buf) + 1,
          GFP_KERNEL, MTLK_MEM_TAG_DEBUG_DATA);
      if (!dbg_general_pkt_text[text_cur_entry]) {
        ERROR_OUT_OF_MEMORY();
        goto err_clean;
      }
      strcpy(dbg_general_pkt_text[text_cur_entry], buf);
      ++text_cur_entry;
      
      ASSERT(cnts_cur_entry < cnt_num_entries);
      dbg_general_pkt_cnts[cnts_cur_entry++] =
          nic->pstats.sta_session_rx_packets;

      ASSERT(cnts_cur_entry < cnt_num_entries);
      dbg_general_pkt_cnts[cnts_cur_entry++] =
          nic->pstats.sta_session_tx_packets;
    }
  }

  dbg_general_pkt_text[text_cur_entry] = kmalloc_tag(sizeof(hdr),
    GFP_KERNEL, MTLK_MEM_TAG_DEBUG_DATA);
  if (!dbg_general_pkt_text[text_cur_entry])
  {
    ERROR_OUT_OF_MEMORY();
    goto err_clean;
  }
  memcpy(dbg_general_pkt_text[text_cur_entry], ftr, sizeof(ftr));
  ++text_cur_entry;

  dbg_general_pkt_text[text_cur_entry] = NULL; // terminate stringlist
  ++text_cur_entry;

  ASSERT(text_cur_entry == text_num_entries);

  ASSERT(cnts_cur_entry < cnt_num_entries);
  dbg_general_pkt_cnts[cnts_cur_entry++] = total_rx_packets;

  ASSERT(cnts_cur_entry < cnt_num_entries);
  dbg_general_pkt_cnts[cnts_cur_entry++] = total_tx_packets;

  ASSERT(cnts_cur_entry == cnt_num_entries);
  
  ASSERT(nic->slow_ctx->dbg_general_pkt_text == NULL);
  ASSERT(nic->slow_ctx->dbg_general_pkt_cnts == NULL);
  nic->slow_ctx->dbg_general_pkt_text = dbg_general_pkt_text;
  nic->slow_ctx->dbg_general_pkt_cnts = dbg_general_pkt_cnts;
  nic->slow_ctx->dbg_general_pkt_cnts_num = cnt_num_entries;

  *pcnt = num_connected + 1;

  return 0;

err_clean:
  if (dbg_general_pkt_text) {
    int i;
    for (i = 0; dbg_general_pkt_text[i] != NULL; ++i)
      kfree_tag(dbg_general_pkt_text[i]);
    kfree_tag(dbg_general_pkt_text);
  }
  if (dbg_general_pkt_cnts)
    kfree_tag(dbg_general_pkt_cnts);
  return 1;
}

static int bcl_prepare_rr_counters(struct nic *nic, uint32 *pcnt)
{
  const char hdr[] = "MAC|ID|Too old|Duplicate|Queued|Overflows";
  const int num_cnts = str_count(hdr, '|');
  
  int stn, tid, num_tids;

  char **dbg_rr_text = NULL;
  uint32 *dbg_rr_cnts = NULL;
  char buf[128];
  int num_connected;
  int text_num_entries, text_cur_entry;
  int cnt_num_entries, cnts_cur_entry;
  
  num_connected = dbg_num_connected(nic);
  num_tids = 0;
  for (stn = 0; stn < STA_MAX_STATIONS; stn++) {
    sta_entry *sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, stn);

    if (sta->state == PEER_UNUSED)
      continue;
    for (tid = 0; tid < NTS_TIDS; tid++) {
      if(mtlk_is_used_rod_queue(&sta->rod_queue[tid]))
          num_tids++;
    }
  }

  text_num_entries = 1 // Header
      + num_connected * num_tids // Entries
      + 1; // NULL terminator
  dbg_rr_text = kmalloc_tag(text_num_entries * sizeof(char *),
      GFP_KERNEL, MTLK_MEM_TAG_DEBUG_DATA);
  if (!dbg_rr_text) {
    ERROR_OUT_OF_MEMORY();
    goto err_clean;
  }
  memset(dbg_rr_text, 0, text_num_entries * sizeof(char *));

  cnt_num_entries = num_cnts * num_connected * num_tids;
  dbg_rr_cnts = kmalloc_tag(
      cnt_num_entries*sizeof(uint32),
      GFP_KERNEL, MTLK_MEM_TAG_DEBUG_DATA);
  if (!dbg_rr_cnts)
  {
    ERROR_OUT_OF_MEMORY();
    goto err_clean;
  }

  text_cur_entry = 0;

  dbg_rr_text[text_cur_entry] = kmalloc_tag(sizeof(hdr),
    GFP_KERNEL, MTLK_MEM_TAG_DEBUG_DATA);
  if (!dbg_rr_text[text_cur_entry])
  {
    ERROR_OUT_OF_MEMORY();
    goto err_clean;
  }
  memcpy(dbg_rr_text[text_cur_entry], hdr, sizeof(hdr));
  ++text_cur_entry;
  
  cnts_cur_entry = 0;
 
  for (stn = 0; stn < STA_MAX_STATIONS; stn++) {
    sta_entry *sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, stn);

    if (sta->state == PEER_UNUSED)
      continue;

    //FIXME: No lock taken during operations with reordering stats.
    //       Is it a problem?

    for (tid = 0; tid < NTS_TIDS; tid++) {
      reordering_stats *pstats = mtlk_get_rod_stats(&sta->rod_queue[tid]);

      if (pstats == NULL)
        continue;

      sprintf(buf, MAC_FMT, MAC_ARG(sta->mac));
      dbg_rr_text[text_cur_entry] = kmalloc_tag(strlen(buf) + 1,
        GFP_KERNEL, MTLK_MEM_TAG_DEBUG_DATA);
      if (!dbg_rr_text[text_cur_entry]) {
        ERROR_OUT_OF_MEMORY();
        goto err_clean;
      }
      strcpy(dbg_rr_text[text_cur_entry], buf);
      ++text_cur_entry;

      ASSERT(cnts_cur_entry < cnt_num_entries);
      dbg_rr_cnts[cnts_cur_entry++] = tid;

      ASSERT(cnts_cur_entry < cnt_num_entries);
      dbg_rr_cnts[cnts_cur_entry++] = pstats->too_old;

      ASSERT(cnts_cur_entry < cnt_num_entries);
      dbg_rr_cnts[cnts_cur_entry++] = pstats->duplicate;

      ASSERT(cnts_cur_entry < cnt_num_entries);
      dbg_rr_cnts[cnts_cur_entry++] = pstats->queued;

      ASSERT(cnts_cur_entry < cnt_num_entries);
      dbg_rr_cnts[cnts_cur_entry++] = pstats->overflows;
    }
  }
      
  dbg_rr_text[text_cur_entry] = NULL; // terminate stringlist
  ++text_cur_entry;

  ASSERT(text_cur_entry == text_num_entries);
  ASSERT(cnts_cur_entry == cnt_num_entries);
  
  ASSERT(nic->slow_ctx->dbg_rr_text == NULL);
  ASSERT(nic->slow_ctx->dbg_rr_cnts == NULL);
  nic->slow_ctx->dbg_rr_text = dbg_rr_text;
  nic->slow_ctx->dbg_rr_cnts = dbg_rr_cnts;
  nic->slow_ctx->dbg_rr_cnts_num = cnt_num_entries;

  *pcnt = num_connected * num_tids;

  return 0;

err_clean:
  if (dbg_rr_text) {
    int i;
    for (i = 0; dbg_rr_text[i] != NULL; ++i)
      kfree_tag(dbg_rr_text[i]);
    kfree_tag(dbg_rr_text);
  }
  if (dbg_rr_cnts)
    kfree_tag(dbg_rr_cnts);
  return 1;
}

int mtlk_debug_category_init(struct nic *nic, uint32 category,
    uint32 *pcnt)
{
  // Finalize previous session in case it wasn't finalized properly:
  mtlk_debug_category_free(nic, category);

  switch (category) {
  case DRVCAT_DBG_API_GENERAL:
    {
      struct count_stat_params csp;
      csp.num_stats = 0;
      if (0 != iterate_driver_stats(nic, &count_stat, &csp)) {
        ERROR_DRV_STATS_ITER();
        return 1;
      }
      *pcnt = csp.num_stats;
    }
    break;
  case DRVCAT_DBG_API_GENERAL_PKT:
    if (bcl_prepare_pkt_counters(nic, pcnt) != 0)
      return 1;
    break;
  case DRVCAT_DBG_API_MAC_STATS:
    {
      // Store MAC stats in a buffer so we don't have to read stats from
      // MAC on every single stat request from BCLSockServer:
      int i;
      uint32 cnt = 0;
      UMI_GET_STATISTICS *pstats;
      uint32 *dbg_mac_stats;
      mtlk_txmm_msg_t man_msg;
 
      if (mtlk_txmm_msg_init(&man_msg) != MTLK_ERR_OK) {
        return 1;
      }

      pstats = mac_get_stats(nic, &man_msg);
      if (!pstats) {
        mtlk_txmm_msg_cleanup(&man_msg);
        return 1;
      }

      cnt = sizeof(stat_info) / sizeof(stat_info[0]);

      dbg_mac_stats = kmalloc_tag(cnt * sizeof(uint32),
        GFP_KERNEL, MTLK_MEM_TAG_DEBUG_DATA);
      if (!dbg_mac_stats) {
        ERROR_OUT_OF_MEMORY();
        mtlk_txmm_msg_cleanup(&man_msg);
        return 1;
      }
      for (i = 0; i < cnt; ++i) {
        dbg_mac_stats[i] = le32_to_cpu(pstats->sStats.au32Statistics[
            stat_info[i].idx]);
      }
      ASSERT(nic->slow_ctx->dbg_mac_stats == NULL);
      nic->slow_ctx->dbg_mac_stats = dbg_mac_stats;

      mtlk_txmm_msg_cleanup(&man_msg);

      *pcnt = cnt;
    }
    break;
  case DRVCAT_DBG_API_RR_STATS:
    if (bcl_prepare_rr_counters(nic, pcnt) != 0)
      return 1;
    break;
  default:
    ERROR_CAT_UNSUPPORTED(category);
    return 1;
  }
  return 0;
}

int mtlk_debug_category_free(struct nic *nic, uint32 category)
{
  switch (category) {
  case DRVCAT_DBG_API_GENERAL:
    // Nothing to free - stats were taken directly from driver structures
    break;
  case DRVCAT_DBG_API_GENERAL_PKT:
    if (nic->slow_ctx->dbg_general_pkt_text) {
      int i;
      for (i = 0; nic->slow_ctx->dbg_general_pkt_text[i] != NULL; ++i)
        kfree_tag(nic->slow_ctx->dbg_general_pkt_text[i]);
      kfree_tag(nic->slow_ctx->dbg_general_pkt_text);
      nic->slow_ctx->dbg_general_pkt_text = NULL;
    }
    if (nic->slow_ctx->dbg_general_pkt_cnts) {
      kfree_tag(nic->slow_ctx->dbg_general_pkt_cnts);
      nic->slow_ctx->dbg_general_pkt_cnts = NULL;
    }
    nic->slow_ctx->dbg_general_pkt_cnts_num = 0;
    break;
  case DRVCAT_DBG_API_MAC_STATS:
    if (nic->slow_ctx->dbg_mac_stats) {
      kfree_tag(nic->slow_ctx->dbg_mac_stats);
      nic->slow_ctx->dbg_mac_stats = NULL;
    }
    break;
  case DRVCAT_DBG_API_RR_STATS:
    if (nic->slow_ctx->dbg_rr_text) {
      int i;
      for (i = 0; nic->slow_ctx->dbg_rr_text[i] != NULL; ++i)
        kfree_tag(nic->slow_ctx->dbg_rr_text[i]);
      kfree_tag(nic->slow_ctx->dbg_rr_text);
      nic->slow_ctx->dbg_rr_text = NULL;
    }
    if (nic->slow_ctx->dbg_rr_cnts) {
      kfree_tag(nic->slow_ctx->dbg_rr_cnts);
      nic->slow_ctx->dbg_rr_cnts = NULL;
    }
    nic->slow_ctx->dbg_rr_cnts_num = 0;
    break;
  default:
    ERROR_CAT_UNSUPPORTED(category);
    return 1;
  }
  return 0;
}

int mtlk_debug_name_get(struct nic *nic, uint32 category,
    uint32 index, char *pdata, uint32 datalen)
{
  int rslt;
  switch (category) {
  case DRVCAT_DBG_API_GENERAL:
    {
      struct fetch_stat_params fsp;
      fsp.index_cur = 0;
      fsp.index_search = index;
      fsp.what = FETCH_NAME;
      if (0 != iterate_driver_stats(nic, &fetch_stat, &fsp)) {
        ERROR_DRV_STATS_ITER();
        return 1;
      }
      rslt = snprintf(pdata, datalen, "%s", fsp.name);
      if (rslt < 0 || rslt >= datalen)
        WARN_STRING_TRUNCATED(datalen, category, index);
    }
    break;
  case DRVCAT_DBG_API_GENERAL_PKT:
    if (index >= nic->slow_ctx->dbg_general_pkt_cnts_num + 1) { // +1 for header
      ERROR_IDX_OUT_OF_BOUNDS(category, index);
      return 1;
    }
    ASSERT(nic->slow_ctx->dbg_general_pkt_text != NULL);
    rslt = snprintf(pdata, datalen, "%s",
        nic->slow_ctx->dbg_general_pkt_text[index]);
    if (rslt < 0 || rslt >= datalen)
      WARN_STRING_TRUNCATED(datalen, category, index);
    break;
  case DRVCAT_DBG_API_MAC_STATS:
    if (index >= sizeof(stat_info) / sizeof(stat_info[0])) {
      ERROR_IDX_OUT_OF_BOUNDS(category, index);
      return 1;
    }
    rslt = snprintf(pdata, datalen, "%s", stat_info[index].name);
    if (rslt < 0 || rslt >= datalen)
      WARN_STRING_TRUNCATED(datalen, category, index);
    break;
  case DRVCAT_DBG_API_RR_STATS:
    if (index >= nic->slow_ctx->dbg_rr_cnts_num + 1) { // +1 for header
      ERROR_IDX_OUT_OF_BOUNDS(category, index);
      return 1;
    }
    ASSERT(nic->slow_ctx->dbg_rr_text != NULL);
    rslt = snprintf(pdata, datalen, "%s",
        nic->slow_ctx->dbg_rr_text[index]);
    if (rslt < 0 || rslt >= datalen)
      WARN_STRING_TRUNCATED(datalen, category, index);
    break;
  default:
    ERROR_CAT_UNSUPPORTED(category);
    return 1;
  }
  return 0;
}

int mtlk_debug_val_get(struct nic *nic, uint32 category,
    uint32 index, uint32 *pval)
{
  switch (category) {
  case DRVCAT_DBG_API_GENERAL:
    {
      struct fetch_stat_params fsp;
      fsp.index_cur = 0;
      fsp.index_search = index;
      fsp.what = FETCH_VAL;
      if (0 != iterate_driver_stats(nic, &fetch_stat, &fsp)) {
        ERROR_DRV_STATS_ITER();
        return 1;
      }
      *pval = fsp.val;
    }
    break;
  case DRVCAT_DBG_API_GENERAL_PKT:
    if (index >= nic->slow_ctx->dbg_general_pkt_cnts_num) {
      ERROR_IDX_OUT_OF_BOUNDS(category, index);
      return 1;
    }
    *pval = nic->slow_ctx->dbg_general_pkt_cnts[index];
    break;
  case DRVCAT_DBG_API_MAC_STATS:
    if (index >= sizeof(stat_info) / sizeof(stat_info[0])) {
      ERROR_IDX_OUT_OF_BOUNDS(category, index);
      return 1;
    }
    *pval = nic->slow_ctx->dbg_mac_stats[index];
    break;
  case DRVCAT_DBG_API_RR_STATS:
    if (index >= nic->slow_ctx->dbg_rr_cnts_num) {
      ERROR_IDX_OUT_OF_BOUNDS(category, index);
      return 1;
    }
    *pval = nic->slow_ctx->dbg_rr_cnts[index];
    break;
  default:
    ERROR_CAT_UNSUPPORTED(category);
    return 1;
  }
  return 0;
}

int mtlk_debug_val_put(struct nic *nic, uint32 category,
    uint32 index, uint32 val)
{
  switch (category) {
  case DRVCAT_DBG_API_RESET:
    switch (index) {
    case IDX_DBG_API_RESET_ALL:
      debug_reset_all_counters(nic);
      break;
    default:
      ERROR_IDX_OUT_OF_BOUNDS(category, index);
      return 1;
    }
    break;
  default:
    ERROR_CAT_UNSUPPORTED(category);
    return 1;
  }
  return 0;
}

#if !defined MTCFG_SILENT
#if defined MTCFG_RT_LOGGER_OFF
static int
mtlk_debug_write (struct file *file, const char *buf,
                  unsigned long count, void *data)
{
    char debug_level[MAX_PROC_STR_LEN];
    int level;

    if (count > MAX_PROC_STR_LEN)
        return -EINVAL;

    memset(debug_level, 0, sizeof(debug_level));
    if (copy_from_user(debug_level, buf, count))
        return -EFAULT;

    level = simple_strtol(debug_level, NULL, 10);

    log_osdep_reset_levels(level);
    debug = level;

    return count;
}


static int
mtlk_debug_get_log_level (char *page, char **start, off_t off,
                          int count, int *eof, void *data)
{
    char *p   = page;

    if (off != 0)
    {
        *eof = 1;
        return 0;
    }

    p += sprintf(p, "%d\n", (int)log_osdep_get_level((long int)data));

    return(p - page);
}

static int
mtlk_debug_set_log_level (struct file *file, const char *buf,
                          unsigned long count, void *data)
{
    char debug_level[MAX_PROC_STR_LEN];
    int level;

    if (count > MAX_PROC_STR_LEN)
        return -EINVAL;

    memset(debug_level, 0, sizeof(debug_level));
    if (copy_from_user(debug_level, buf, count))
        return -EFAULT;

    level = simple_strtol(debug_level, NULL, 10);

    log_osdep_set_level((long int)data, level);

    return count;
}
#endif /* MTCFG_RT_LOGGER_OFF */
#endif /* !defined MTCFG_SILENT */

int
mtlk_debug_igmp_read (char *page, char **start, off_t off,
           int count, int *eof, void *data )
{
    char *buffer;
    int blen, len;
    struct nic *nic= (struct nic *) data;

    buffer = vmalloc_tag(4096 * 2, MTLK_MEM_TAG_DEBUG_DATA);
    if (buffer == NULL)
    {
        ELOG("Out of memory");
        return -ENOMEM;
    }

    blen = mtlk_mc_dump_groups(nic, buffer);

    len = blen - off > count ? count : blen - off;

    memcpy( page, buffer + off, len );
    *start = page;

    if (len == count)
        *eof = 0;
    else
        *eof = 1;

    vfree_tag( buffer );

    return len;
}

static int
ee_caps_read (char *page, char **start, off_t off,
           int count, int *eof, void *data)
{
    struct nic *nic= (struct nic *) data;

    *eof = 1;    
    return mtlk_eeprom_get_caps(&nic->slow_ctx->ee_data, page, count);
}

int 
mtlk_debug_bcl_category_init(struct nic *nic, uint32 category,
    uint32 *cnt)
{
  switch (category)
  {
  case DRVCAT_DBG_API_GENERAL:
  case DRVCAT_DBG_API_GENERAL_PKT:
  case DRVCAT_DBG_API_MAC_STATS:
  case DRVCAT_DBG_API_RR_STATS:
    return mtlk_debug_category_init(nic, category, cnt);
  default:
    ELOG("Unsupported data category (%u) requested", category);
    break;
  }
  return 1;
}

int
mtlk_debug_bcl_category_free(struct nic *nic, uint32 category)
{
  switch (category)
  {
  case DRVCAT_DBG_API_GENERAL:
  case DRVCAT_DBG_API_GENERAL_PKT:
  case DRVCAT_DBG_API_MAC_STATS:
  case DRVCAT_DBG_API_RR_STATS:
    return mtlk_debug_category_free(nic, category);
  default:
    ELOG("Unsupported data category (%u) requested", category);
    break;
  }
  return 1;
}

int  
mtlk_debug_bcl_name_get(struct nic *nic, uint32 category,
    uint32 index, char *pdata, uint32 datalen)
{
  switch (category)
  {
  case DRVCAT_DBG_API_GENERAL:
  case DRVCAT_DBG_API_GENERAL_PKT:
  case DRVCAT_DBG_API_MAC_STATS:
  case DRVCAT_DBG_API_RR_STATS:
    return mtlk_debug_name_get(nic, category, index, pdata, datalen);
  default:
    ELOG("Unsupported data category (%u) requested", category);
    break;
  }
  return 1;
}

int
mtlk_debug_bcl_val_get(struct nic *nic, uint32 category,
    uint32 index, uint32 *pval)
{
  switch (category)
  {
  case DRVCAT_DBG_API_GENERAL:
  case DRVCAT_DBG_API_GENERAL_PKT:
  case DRVCAT_DBG_API_MAC_STATS:
  case DRVCAT_DBG_API_RR_STATS:
    return mtlk_debug_val_get(nic, category, index, pval);
  default:
    ELOG("Unsupported data category (%u) requested", category);
    break;
  }
  return 1;
}

int
mtlk_debug_bcl_val_put(struct nic *nic, uint32 category,
    uint32 index, uint32 val)
{
  switch (category)
  {
  case DRVCAT_DBG_API_RESET:
    return mtlk_debug_val_put(nic, category, index, val);
  default:
    ELOG("Unsupported data category (%u) requested", category);
    break;
  }
  return 1;
}

void
mtlk_debug_bswap_bcl_request (UMI_BCL_REQUEST *req, BOOL hdr_only)
{
  int i;

  req->Size    = cpu_to_le32(req->Size);
  req->Address = cpu_to_le32(req->Address);
  req->Unit    = cpu_to_le32(req->Unit);

  if (!hdr_only) {
    for (i = 0; i < ARRAY_SIZE(req->Data); i++) {
      req->Data[i] = cpu_to_le32(req->Data[i]);
    }
  }
}

#ifdef MTCFG_CPU_STAT

static int cpu_stats_seq_show(struct seq_file *s, void *v)
{
  mtlk_core_t *nic = (container_of(s->op, struct mtlk_seq_ops, seq_ops))->nic;

  cpu_stats_show(s, v, nic);

  return 0;
}

static int cpu_stats_do_reset(struct file *file, const char *buffer,
                                         unsigned long count, void *data)
{
  mtlk_cpu_stat_reset();

  /* let it looks as all data has
   * been written successfully
   */
  return count;
}

static int cpu_stats_enable_read(char *page, char **start, off_t off,
                                 int count, int *eof, void *data)
{
  int res = 0;
  char name[32];
  int i = 0;

  /* no support for offset */
  if (off){
    *eof = 1;
    return 0;
  };

  /* scnprintf looks more preferable here,
   * but old kernels lack this function
   */

  res = snprintf(page, count, 
                 "%d\n",
                 _CPU_STAT_GET_ENABLED_ID());
  if (res < 0 || res >= count)
    return -ENOSPC;

  INFO("************************************************************");
  INFO("* CPU Statistics Available Indexes:");
  INFO("************************************************************");
  _CPU_STAT_FOREACH_TRACK_IDX(i) {

    _CPU_STAT_GET_NAME_EX(i, name, sizeof(name));
    
    INFO("* %03d - %-30s (delay %d usec) %s", 
         i, name, _CPU_STAT_GET_DELAY(i),
         _CPU_STAT_IS_ENABLED(i)?"[*]":"");
  }
  INFO("************************************************************");

  return res;
}

static int cpu_stats_enable_write(struct file *file, const char *buffer,
                                 unsigned long count, void *data)
{
  char buf[16];
  unsigned long len = min((unsigned long)sizeof(buf)-1, count);
  int val;

  /* if not all data copied return with error */
  if(copy_from_user(buf, buffer, len))
    return -EFAULT;

  /* put "\0" at the end of the read data */
  buf[len] = 0;

  /* sscanf allows to enter value in decimal, hex or octal */
  sscanf(buf, "%i", &val);
  mtlk_cpu_stat_enable(val);

  return strnlen(buf, len);
}

static int cpu_stats_delay_write(struct file *file, const char *buffer,
                                 unsigned long count, void *data)
{
  char buf[16];
  unsigned long len = min((unsigned long)sizeof(buf)-1, count);
  int track;
  uint32 delay;

  /* if not all data copied return with error */
  if(copy_from_user(buf, buffer, len))
    return -EFAULT;

  /* put "\0" at the end of the read data */
  buf[len] = 0;

  /* sscanf allows to enter value in decimal, hex or octal */
  sscanf(buf, "%i %u", &track, &delay);

  mtlk_cpu_stat_set_delay(track, delay);

  return strnlen(buf, len);
}

static int cpu_stats_max_id_read(char *page, char **start, off_t off,
                                 int count, int *eof, void *data)
{
  int res = 0;

  /* no support for offset */
  if (off){
    *eof = 1;
    return 0;
  };

  /* scnprintf looks more preferable here,
   * but old kernels lack this function
   */

  res = snprintf(page, count, "%d\n", CPU_STAT_ID_LAST - 1);
  if (res < 0 || res >= count)
    return -ENOSPC;

  return res;
}

#endif /* MTCFG_CPU_STAT */

void
mtlk_debug_procfs_cleanup(struct nic *nic, unsigned num_boards_alive)
{
  struct list_head *cur = NULL;

  while (nic->slow_ctx->pentry_num--)
    remove_proc_entry(nic->slow_ctx->procfs_entry[nic->slow_ctx->pentry_num]->name,
                      nic->slow_ctx->procfs_entry[nic->slow_ctx->pentry_num]->parent);
  /* freeing last card? */
  if (num_boards_alive == 0 && nic->slow_ctx->net_pentry_num) {
    while (nic->slow_ctx->net_pentry_num--)
      remove_proc_entry(nic->slow_ctx->net_procfs_entry[nic->slow_ctx->net_pentry_num]->name,
                        nic->slow_ctx->net_procfs_entry[nic->slow_ctx->net_pentry_num]->parent);
  }


  /* freeing all dynamically allocated mtlk_seq_ops
   * structures needed for some proc entries.
   */
  while(!list_empty(&nic->slow_ctx->seq_ops_list)){
    cur = nic->slow_ctx->seq_ops_list.next;
    list_del(cur);
    kfree_tag(list_entry(cur, struct mtlk_seq_ops, list));
  }
}

extern char *mtlk_version_string;

extern int
do_debug_assert_write (struct file *file, const char *buf,
					   unsigned long count, void *data);

/* this function is called on the "core" start. */
int mtlk_debug_register_proc_entries(mtlk_core_t *nic)
{
  struct proc_dir_entry *debug_dir;
  struct proc_dir_entry *pentry;

  // this directory is shared between NICs in multicard setups
  static struct proc_dir_entry *net_procfs_dir = NULL;

  if (!net_procfs_dir) {
    // /proc/net/mtlk contents are created upon first demand
    net_procfs_dir = proc_mkdir("mtlk", PROC_NET);
    ASSERT(net_procfs_dir != NULL);
    nic->slow_ctx->net_pentry_num = 0;
    net_procfs_dir->uid = MTLK_PROCFS_UID;
    net_procfs_dir->gid = MTLK_PROCFS_GID;
    nic->slow_ctx->net_procfs_entry[nic->slow_ctx->net_pentry_num++] = net_procfs_dir;

    pentry = create_proc_entry("version", S_IFREG|S_IRUSR|S_IRGRP|S_IROTH, net_procfs_dir);
    ASSERT(pentry != NULL);
    pentry->uid = MTLK_PROCFS_UID;
    pentry->gid = MTLK_PROCFS_GID;
    pentry->read_proc = mtlk_proc_aux_read_string;
    pentry->write_proc = NULL;
    pentry->data = &mtlk_version_string;
    nic->slow_ctx->net_procfs_entry[nic->slow_ctx->net_pentry_num++] = pentry;

#if !defined MTCFG_SILENT
#if defined MTCFG_RT_LOGGER_OFF
    pentry = create_proc_entry("debug", S_IFREG|S_IRUSR|S_IRGRP, net_procfs_dir);
    ASSERT(pentry != NULL);
    pentry->uid = MTLK_PROCFS_UID;
    pentry->gid = MTLK_PROCFS_GID;
    pentry->read_proc = mtlk_proc_aux_read;
    pentry->write_proc = mtlk_debug_write;
    pentry->data = &debug;
    nic->slow_ctx->net_procfs_entry[nic->slow_ctx->net_pentry_num++] = pentry;

    pentry = create_proc_entry("debug_addba", S_IFREG|S_IRUSR|S_IRGRP, net_procfs_dir);
    ASSERT(pentry != NULL);
    pentry->uid = MTLK_PROCFS_UID;
    pentry->gid = MTLK_PROCFS_GID;
    pentry->read_proc = mtlk_debug_get_log_level;
    pentry->write_proc = mtlk_debug_set_log_level;
    pentry->data = (void *)GID_ADDBA;
    nic->slow_ctx->net_procfs_entry[nic->slow_ctx->net_pentry_num++] = pentry;

    pentry = create_proc_entry("debug_disconnect", S_IFREG|S_IRUSR|S_IRGRP, net_procfs_dir);
    ASSERT(pentry != NULL);
    pentry->uid = MTLK_PROCFS_UID;
    pentry->gid = MTLK_PROCFS_GID;
    pentry->read_proc = mtlk_debug_get_log_level;
    pentry->write_proc = mtlk_debug_set_log_level;
    pentry->data = (void *)GID_DISCONNECT;
    nic->slow_ctx->net_procfs_entry[nic->slow_ctx->net_pentry_num++] = pentry;

    pentry = create_proc_entry("debug_frame", S_IFREG|S_IRUSR|S_IRGRP, net_procfs_dir);
    ASSERT(pentry != NULL);
    pentry->uid = MTLK_PROCFS_UID;
    pentry->gid = MTLK_PROCFS_GID;
    pentry->read_proc = mtlk_debug_get_log_level;
    pentry->write_proc = mtlk_debug_set_log_level;
    pentry->data = (void *)GID_FRAME;
    nic->slow_ctx->net_procfs_entry[nic->slow_ctx->net_pentry_num++] = pentry;

    pentry = create_proc_entry("debug_aocs", S_IFREG|S_IRUSR|S_IRGRP, net_procfs_dir);
    ASSERT(pentry != NULL);
    pentry->uid = MTLK_PROCFS_UID;
    pentry->gid = MTLK_PROCFS_GID;
    pentry->read_proc = mtlk_debug_get_log_level;
    pentry->write_proc = mtlk_debug_set_log_level;
    pentry->data = (void *)GID_AOCS;
    nic->slow_ctx->net_procfs_entry[nic->slow_ctx->net_pentry_num++] = pentry;

#ifdef MTCFG_RF_MANAGEMENT_MTLK
    pentry = create_proc_entry("debug_rf_mgmt", S_IFREG|S_IRUSR|S_IRGRP, net_procfs_dir);
    ASSERT(pentry != NULL);
    pentry->uid = MTLK_PROCFS_UID;
    pentry->gid = MTLK_PROCFS_GID;
    pentry->read_proc = mtlk_debug_get_log_level;
    pentry->write_proc = mtlk_debug_set_log_level;
    pentry->data = (void *)GID_ASEL;
    nic->slow_ctx->net_procfs_entry[nic->slow_ctx->net_pentry_num++] = pentry;
#endif /* MTCFG_RF_MANAGEMENT_MTLK */
#endif /* defined MTCFG_RT_LOGGER_OFF */
#endif /* !defined MTCFG_SILENT */

#ifdef MTCFG_ENABLE_OBJPOOL
    prepare_one_fn_seq_file(nic, debug_mem_alloc_dump, net_procfs_dir, "mem_alloc_dump");
#endif
  }

  nic->slow_ctx->procfs_dir = proc_mkdir(nic->ndev->name, net_procfs_dir);
  ASSERT(nic->slow_ctx->procfs_dir != NULL);
  nic->slow_ctx->procfs_dir->uid = MTLK_PROCFS_UID;
  nic->slow_ctx->procfs_dir->gid = MTLK_PROCFS_GID;
  nic->slow_ctx->procfs_entry[nic->slow_ctx->pentry_num++] = nic->slow_ctx->procfs_dir;

  pentry = create_proc_entry("do_debug_assert", S_IFREG|S_IRUSR|S_IRGRP, nic->slow_ctx->procfs_dir);
  ASSERT(pentry != NULL);
  pentry->uid = MTLK_PROCFS_UID;
  pentry->gid = MTLK_PROCFS_GID;
  pentry->read_proc = NULL;
  pentry->write_proc = do_debug_assert_write;
  pentry->data = nic;
  nic->slow_ctx->procfs_entry[nic->slow_ctx->pentry_num++] = pentry;

#if 0
  if (clr_init(&clr) == 1) {
    ELOG("Failed to initialize classifier");
    goto err_out;
  }
  pentry = create_proc_entry("classifier", S_IFREG|S_IRUSR|S_IRGRP, procfs_dir);
  ASSERT(pentry != NULL);
  pentry->uid = procfs_uid;
  pentry->gid = procfs_gid;
  pentry->proc_fops = &clr_pentry_fops;
  procfs_entry[pentry_num++] = pentry;
#endif

  /* make per-card debug dir in /proc/sys/mtlk/wlanN/
   * (which is pointed to by parent parameter)
   */

  pentry = create_proc_entry("igmp", S_IFREG|S_IRUSR|S_IRGRP, nic->slow_ctx->procfs_dir);
  ASSERT(pentry != NULL);
  pentry->uid = MTLK_PROCFS_UID;
  pentry->gid = MTLK_PROCFS_GID;
  pentry->data = nic;
  pentry->read_proc = mtlk_debug_igmp_read;
  pentry->write_proc = NULL;
  pentry->data = nic;
  nic->slow_ctx->procfs_entry[nic->slow_ctx->pentry_num++] = pentry;

  pentry = create_proc_entry("lm", S_IFREG|S_IRUSR|S_IRGRP, nic->slow_ctx->procfs_dir);
  ASSERT(pentry != NULL);
  pentry->uid = MTLK_PROCFS_UID;
  pentry->gid = MTLK_PROCFS_GID;
  pentry->read_proc = proc_bcl_read_lm;
  pentry->write_proc = NULL;
  pentry->data = nic;
  nic->slow_ctx->procfs_entry[nic->slow_ctx->pentry_num++] = pentry;

  pentry = create_proc_entry("um", S_IFREG|S_IRUSR|S_IRGRP, nic->slow_ctx->procfs_dir);
  ASSERT(pentry != NULL);
  pentry->uid = MTLK_PROCFS_UID;
  pentry->gid = MTLK_PROCFS_GID;
  pentry->read_proc = proc_bcl_read_um;
  pentry->write_proc = NULL;
  pentry->data = nic;
  nic->slow_ctx->procfs_entry[nic->slow_ctx->pentry_num++] = pentry;

  pentry = create_proc_entry("shram", S_IFREG|S_IRUSR|S_IRGRP, nic->slow_ctx->procfs_dir);
  ASSERT(pentry != NULL);
  pentry->uid = MTLK_PROCFS_UID;
  pentry->gid = MTLK_PROCFS_GID;
  pentry->read_proc = proc_bcl_read_shram;
  pentry->write_proc = NULL;
  pentry->data = nic;
  nic->slow_ctx->procfs_entry[nic->slow_ctx->pentry_num++] = pentry;

  pentry = create_proc_entry("EECaps", S_IFREG|S_IRUSR|S_IRGRP, nic->slow_ctx->procfs_dir);
  ASSERT(pentry != NULL);
  pentry->uid = MTLK_PROCFS_UID;
  pentry->gid = MTLK_PROCFS_GID;
  pentry->read_proc = ee_caps_read;
  pentry->write_proc = NULL;
  pentry->data = nic;
  nic->slow_ctx->procfs_entry[nic->slow_ctx->pentry_num++] = pentry;

  /* AOCS data */
  if (nic->slow_ctx->hw_cfg.ap) {
    prepare_one_fn_seq_file(nic, debug_aocs_table, nic->slow_ctx->procfs_dir, "aocs_table");
    prepare_one_fn_seq_file(nic, debug_aocs_channels, nic->slow_ctx->procfs_dir, "aocs_channels");
    prepare_one_fn_seq_file(nic, debug_aocs_penalties, nic->slow_ctx->procfs_dir, "aocs_penalties");
  }

  prepare_one_fn_seq_file(nic, debug_aocs_history, nic->slow_ctx->procfs_dir, "aocs_history");

  /* Tx limits and related */
  prepare_one_fn_seq_file(nic, debug_hw_limits, nic->slow_ctx->procfs_dir, "hw_limits");
  prepare_one_fn_seq_file(nic, debug_reg_limits, nic->slow_ctx->procfs_dir, "reg_limits");
  prepare_one_fn_seq_file(nic, debug_ant_gain, nic->slow_ctx->procfs_dir, "antenna_gain");

//-------------------------------

  debug_dir = proc_mkdir("Debug", nic->slow_ctx->procfs_dir);
  if(!debug_dir) {
    ELOG("Failed to create proc entry for debug.");
    return MTLK_ERR_UNKNOWN;
  }

  debug_dir->uid = MTLK_PROCFS_UID;
  debug_dir->gid = MTLK_PROCFS_GID;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
  debug_dir->owner = THIS_MODULE;
#endif /* LINUX_VERSION_CODE */
  nic->slow_ctx->procfs_entry[nic->slow_ctx->pentry_num++] = debug_dir;

  /* create files in the newly created "debug" directory */
  prepare_one_fn_seq_file(nic, debug_general_seq_show, debug_dir, "General");
  prepare_one_fn_seq_file(nic, mac_stats_seq_show, debug_dir, "MACStats");
  prepare_one_fn_seq_file(nic, rr_stats_seq_show, debug_dir, "ReorderingStats");
  prepare_one_fn_seq_file(nic, l2nat_stats_seq_show, debug_dir, "L2NAT");
  prepare_one_fn_seq_file(nic, sq_stats_seq_show, debug_dir, "SendQueue");

  /* create and init entry for "reset" file */
  register_one_proc_entry(nic, debug_dir, NULL, mtlk_debug_do_reset,
                                                 "ResetStats", S_IFREG|S_IWUSR);
  /* clear L2NAT table */
  register_one_proc_entry(nic, debug_dir, NULL, l2nat_clear_table,
                                                 "L2NAT_ClearTable", S_IFREG|S_IWUSR);

#ifdef MTCFG_CPU_STAT
  prepare_one_fn_seq_file(nic, cpu_stats_seq_show, net_procfs_dir, "cpu_stats");
  register_one_proc_entry(nic, net_procfs_dir, NULL, cpu_stats_do_reset,
                                                   "cpu_stats_reset", S_IFREG|S_IWUSR);
  register_one_proc_entry(nic, net_procfs_dir, cpu_stats_enable_read, 
                          cpu_stats_enable_write, "cpu_stats_enable" , 
                          S_IFREG|S_IWUSR|S_IRUGO);
  register_one_proc_entry(nic, net_procfs_dir, NULL, 
                          cpu_stats_delay_write, "cpu_stats_delay" , 
                          S_IFREG|S_IWUSR|S_IRUGO);
  register_one_proc_entry(nic, net_procfs_dir, cpu_stats_max_id_read, 
                          NULL, "cpu_stats_max_id" , 
                          S_IFREG|S_IRUGO);
#endif /* MTCFG_CPU_STAT */

#ifdef AOCS_DEBUG
  if (nic->slow_ctx->hw_cfg.ap)
    register_one_proc_entry(nic, debug_dir, NULL, aocs_proc_cl,
                                                 "aocs_cl", S_IFREG|S_IWUSR);
#endif
  return MTLK_ERR_OK;
}

