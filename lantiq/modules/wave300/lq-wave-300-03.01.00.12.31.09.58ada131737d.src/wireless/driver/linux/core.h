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
 * Core module definitions
 *
 */
#ifndef __CORE_H__
#define __CORE_H__

#define _mtlk_core_t nic
struct nic;

#include <linux/sysctl.h>
#include <linux/wireless.h>

#include "mtlkhal.h"
#include "mtlkmib.h"

#include "addba.h"
#include "debug.h"
//#include "mib_osdep.h" TODO: find the right place of this include.
#include "mcast.h"
#include "mtlkqos.h"
#include "eeprom.h"

#include "mtlkflctrl.h"
#include "aocs.h"
#include "dfs.h"
#include "rod.h"
#include "stadb.h"
#include "dataex.h"
#ifdef MTCFG_RF_MANAGEMENT_MTLK
#include "mtlkasel.h"
#endif
#include "mtlk_eq.h"

#include "mtlk_coc.h"

#include "mtlk_core_iface.h"

#ifdef MTCFG_IRB_DEBUG
#include "mtlk_irb_pinger.h"
#endif

#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
#include <net/ifx_ppa_api.h>
#include <net/ifx_ppa_hook.h>
#endif

// the sane amount of time dedicated to MAC to perform
// connection or BSS activation
#define CONNECT_TIMEOUT 10000 /* msec */
#define ASSOCIATE_FAILURE_TIMEOUT 3000 /* msec */

enum ts_priorities {
  TS_PRIORITY_BE,
  TS_PRIORITY_BG,
  TS_PRIORITY_VIDEO,
  TS_PRIORITY_VOICE,
  TS_PRIORITY_LAST
};

static const IEEE_ADDR EMPTY_MAC_ADDR = { {0x00, 0x00, 0x00, 0x00, 0x00, 0x00} };
static const IEEE_ADDR EMPTY_MAC_MASK = { {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF} };

/***************************************************/

typedef struct _wme_class_cfg_t
{
  uint32 cwmin;
  uint32 cwmax;
  uint32 aifsn;
  uint32 txop;
} wme_class_cfg_t;

typedef struct _wme_cfg_t
{
  wme_class_cfg_t wme_class[NTS_PRIORITIES];
} wme_cfg_t;

typedef struct _mtlk_core_cfg_t
{
  mtlk_addba_cfg_t              addba;
  wme_cfg_t                     wme_bss;
  wme_cfg_t                     wme_ap;
  mtlk_dot11h_debug_params_t    dot11h_debug_params;
  uint8                         country_code;
  uint8                         dot11d;
  uint8                         basic_rate_set;
  uint16                        ht_forced_rate;
  uint16                        legacy_forced_rate;
  BOOL                          is_background_scan;
} mtlk_core_cfg_t;

#include "mib_osdep.h"

// private statistic counters
struct priv_stats {
  // TX consecutive dropped packets counter
  uint32 tx_cons_drop_cnt;

  // Maximum number of packets dropped consecutively
  uint32 tx_max_cons_drop;

  // Applicable only to STA:
  uint32 sta_session_rx_packets; // Packets received in this session
  uint32 sta_session_tx_packets; // Packets transmitted in this session

  // Dropped Tx packets counters per priority queue
  uint32 ac_dropped_counter[NTS_PRIORITIES];
  uint32 ac_rx_counter[NTS_PRIORITIES];
  uint32 ac_tx_counter[NTS_PRIORITIES];

  // AP forwarding statistics
  uint32 fwd_tx_packets;
  uint32 fwd_rx_packets;
  uint32 fwd_tx_bytes;
  uint32 fwd_rx_bytes;
  uint32 fwd_dropped;

  // Reliable Multicast statistics
  uint32 rmcast_dropped;

  // Used Tx packets per priority queue
  uint32 ac_used_counter[NTS_PRIORITIES];

  // Counter for replayed packets (802.11i)
  uint32 replays_cnt;

  // Received BAR frames
  uint32 bars_cnt;

  // Received data, control and management 802.11 frames from MAC
  uint32 rx_dat_frames;
  uint32 rx_ctl_frames;
  uint32 rx_man_frames;

  //trasmitted broadcast packets
  uint32 tx_bcast;
  //trasmitted non-reliable multicast packets
  uint32 tx_nrmcast;
  //received broadcast packets
  uint32 rx_bcast;
  //received non-reliable multicast packets
  uint32 rx_nrmcast;
  
  //number of disconnections
  uint32 num_disconnects;

#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
  uint32 ppa_tx_processed;
  uint32 ppa_rx_accepted;
  uint32 ppa_rx_rejected;
#endif
};

#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
extern uint32 mtlk_ppa_tx_sent_up;
extern uint32 mtlk_ppa_tx_dropped;
#endif


#define MTLK_PROCFS_UID         0
#define MTLK_PROCFS_GID         0
#define MTLK_MAX_PROCFS_ENTRIES 50

typedef struct _mtlk_procfs_t
{
  struct proc_dir_entry *dir;
  struct proc_dir_entry *entry[MTLK_MAX_PROCFS_ENTRIES];
  unsigned               entry_num;
} mtlk_procfs_t;

#define ADDBA_TIMER_INTERVAL         (1000)

/* driver is halted - in case if was not initialized yet
   or after critical error */
#define NET_STATE_HALTED         (1 << 0)
/* driver is initializing */
#define NET_STATE_IDLE           (1 << 1)
/* driver has been initialized */
#define NET_STATE_READY          (1 << 2)
/* activation request was sent - waiting for CFM */
#define NET_STATE_ACTIVATING     (1 << 3)
/* got connection event */
#define NET_STATE_CONNECTED      (1 << 4)
/* disconnect started */
#define NET_STATE_DISCONNECTING  (1 << 5)

#define WMM_ACI_DEFAULT_CLASS 0

#include "scan.h"
#include "channels.h"

struct nic;

#define NIC_POWER_LIMIT_VAL    MTLK_BFIELD_INFO(0, 8)  /*  8 bits starting bit0 */
#define NIC_POWER_LIMIT_IS_SET MTLK_BFIELD_INFO(8, 1)  /*  1 bit  starting bit8 */

struct nic_slow_ctx {
  struct nic *nic;
  sta_db stadb;

  uint8 net_mode_cfg;
  uint8 net_mode_cur;
  
  mtlk_core_hw_cfg_t hw_cfg;

  uint16 channel;

  // Debug API
  uint32 *dbg_mac_stats;
  char **dbg_general_pkt_text;
  uint32 *dbg_general_pkt_cnts;
  uint32 dbg_general_pkt_cnts_num;
  char **dbg_rr_text;
  uint32 *dbg_rr_cnts;
  uint32 dbg_rr_cnts_num;
  // data for packet capture

  mib_act *mib;         // default mib values
  char **mib_value;
  struct proc_dir_entry *procfs_dir;
  struct proc_dir_entry *procfs_entry[MTLK_MAX_PROCFS_ENTRIES];
  unsigned pentry_num;
  struct proc_dir_entry *net_procfs_entry[MTLK_MAX_PROCFS_ENTRIES];
  unsigned net_pentry_num;

  //MAC reset control: automatically on MAC assert/exception
  int mac_soft_reset_enable;

  char *mib_sysfs_strings;

  /* list of structs with seq_ops
   * needed for some proc entries.
   */
  struct list_head seq_ops_list;

  ctl_table *nlm_sysctls;
  uint32 mac_stat[STAT_TOTAL_NUMBER];

  // configuration
  mtlk_core_cfg_t cfg;

  tx_limit_t  tx_limits;
  int power_selection;

  /* user configured bonding - used for manual channel selection */
  uint8 bonding;
  // EEPROM data
  mtlk_eeprom_data_t ee_data;

  int connected;

  struct mtlk_scan   scan;
  scan_cache_t       cache; 
  /* spectrum of the last loaded programming model */
  uint8              last_pm_spectrum;
  /* frequency of the last loaded programming model */
  uint8              last_pm_freq;

  // ADDBA-related
  mtlk_addba_t      addba;
  struct timer_list addba_timer;

  /*AP - always 11h only
    STA - always 11h, if dot11d_active is set, use 11d table
  */
  //11h-related
  mtlk_dot11h_t dot11h;

  //aocs-related
  mtlk_aocs_t aocs;
  mtlk_procfs_t procfs;

  // 802.11i (security) stuff
  UMI_RSN_IE rsnie;
  uint8 default_key;        /* WPA key index */
  uint8 default_wep_key;    /* WEP key index for regular STAs */
  uint8 wep_enabled;        /* WEP enabled for regular STAs */
  uint8 peerAPs_key_idx;    /* WEP key index for Peer APs (0 - disabled) */
  MIB_WEP_DEF_KEYS wep_keys;
  uint8 wps_in_progress;

  mtlk_aux_pm_related_params_t pm_params;
  // features
  uint8 is_ht_cfg;
  uint8 is_ht_cur;
  uint8 is_tkip;

  /* Define HALT FW or not in case of unconfirmed disconnect req.
   * Default - FALSE (Not HALT) */
  BOOL	is_halt_fw_on_disc_timeout;

  uint8 frequency_band_cfg;
  uint8 frequency_band_cur;
  uint8 spectrum_mode;
  uint8 sta_force_spectrum_mode;

  // watchdog
  uint16 mac_watchdog_timeout_ms;
  uint32 mac_watchdog_period_ms;

#ifdef MTLK_DEBUG_CHARIOT_OOO
  uint16 seq_prev_sent[NTS_PRIORITIES];
#endif

  struct timer_list mtlk_timer;
  struct iw_statistics iw_stats;

  char nickname[IW_ESSID_MAX_SIZE+1];

  int is_stat_poll;
  rwlock_t stat_lock;
  struct timer_list stat_poll_timer;
  mtlk_osal_timer_t mac_watchdog_timer;
  uint8 channel_load;
  uint8 noise;
  unsigned char bssid[ETH_ALEN];
  char essid[IW_ESSID_MAX_SIZE+1];
  uint8 disable_sm_channels;
  /* ACL white/black list */
  IEEE_ADDR acl[MAX_ADDRESSES_IN_ACL];
  IEEE_ADDR acl_mask[MAX_ADDRESSES_IN_ACL];

  // This event arises when MAC sends either UMI_CONNECTED (STA)
  // or UMI_BSS_CREATED (AP)
  // Thread, that performs connection/bss_creation, waits for this event before returning.
  // If no such event arises - connect/bss_create process has failed and error
  // is reported to the caller.
  mtlk_osal_event_t connect_event;

  struct mtlk_eq eq;

  int mac_stuck_detected_by_sw;
#ifdef MTCFG_IRB_DEBUG
  mtlk_irb_pinger_t pinger;
#endif

  mtlk_coc_t  *coc_mngmt;

  uint16       power_limit; /* see NIC_POWER_LIMIT_... */
};

enum nic_txmm_async_msg_e
{
  MTLK_NIC_TXMMA_GET_CONN_STATS,
  MTLK_NIC_TXMMA_LAST
};

enum nic_txdm_async_msg_e
{
  MTLK_NIC_TXDMA_GET_MAC_STATS,
  MTLK_NIC_TXDMA_LAST
};

struct nic {
  struct nic_slow_ctx *slow_ctx;
  uint8 ap;

  struct net_device *ndev;
  struct net_device_stats stats;
  struct priv_stats pstats;
  mtlk_hw_t *hw;

  // reliable multicast context
  mcast_ctx mcast;

  int net_state;
  spinlock_t net_state_lock;
  BOOL  is_stopping;
  BOOL  is_iface_stopping;

  // entity's MAC address
  unsigned char mac_addr[ETH_ALEN];

  // 802.11i (security) stuff
  u8 group_cipher;
  u8 group_rsc[4][6]; // Replay Sequence Counters per key index

  //flctrl-related
  mtlk_flctrl_t flctrl;
  mtlk_handle_t flctrl_id;

  uint8 reliable_mcast;
  uint8 ap_forwarding;

  /* L2NAT fields */
  uint8 bridge_mode;
  uint8 l2nat_flags;
#define L2NAT_NEED_ARP_INFO    0x1
#define L2NAT_GOT_ARP_INFO     0x2
#define L2NAT_DEF_SET_BY_USER  0x4
  uint8  l2nat_default_host[ETH_ALEN];

  spinlock_t                 l2nat_lock;

  struct list_head           l2nat_free_entries;
  struct list_head           l2nat_active_entries;

  /* real entries (ip, mac, stats, list) */
  struct l2nat_hash_entry   *l2nat_hash_entries;

  /* entries indexes, used to enlarge number of possible hash values,
   * to decrease chances of collisions
   */
  l2nat_bslot_t             *l2nat_hash;

  uint32                    l2nat_aging_timeout;

  uint8                     l2nat_mac_for_arp[ETH_ALEN];
  struct in_addr            l2nat_ip_for_arp;
  unsigned long             l2nat_last_arp_sent_timestamp;

  /* send queue struct for shared packet scheduler */
  struct _mtlk_sq_t         *sq;

  /* "don't tx" flag used when internal packet scheduler is used */
  uint8                     tx_prohibited;

  /* internal packet scheduler enabled */
  uint8                     pack_sched_enabled;

  /* tasklet for "flushing" shared SendQueue on wake */
  struct tasklet_struct     *sq_flush_tasklet;

  /* bands which have already been CB-scanned */
  uint8                     cb_scanned_bands;
#define CB_SCANNED_2_4  0x1
#define CB_SCANNED_5_2  0x2

  /* Number of used REQ BD descriptors */
  uint16                    tx_data_nof_used_bds[MAX_USER_PRIORITIES];

  struct mtlk_qos           qos;

#ifdef MTCFG_RF_MANAGEMENT_MTLK
  mtlk_rf_mgmt_t            rf_mgmt;
#endif
  mtlk_txmm_msg_t           txmm_async_msgs[MTLK_NIC_TXMMA_LAST];
  mtlk_txmm_msg_t           txdm_async_msgs[MTLK_NIC_TXDMA_LAST];
  mtlk_txmm_msg_t           txmm_async_eeprom_msgs[MAX_NUM_TX_ANTENNAS]; /* must be moved to EEPROM module ASAP */
#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
  PPA_DIRECTPATH_CB         ppa_clb;
  uint32                    ppa_if_id;
#endif
  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_INIT_LOOP(TIMER_INIT);
};

#define MTLK_MM_BLOCKED_SEND_TIMEOUT 10000    /* ms */
#define MTLK_TIMER_PERIOD            (HZ/10) /* 100 ms */
#define WAIT_QUEUE_WAIT_TIMEOUT      5000    /* ms */

#define STAT_POLLING_TIMER_PERIOD    HZ      /* 1000 ms */
#define CON_STATUS_CFM_TIMEOUT       5000    /* ms */

typedef enum
  {
    CFG_INFRA_STATION,
    CFG_ADHOC_STATION,
    CFG_ACCESS_POINT,
    CFG_TEST_MAC,
    CFG_NUM_NET_TYPES
  } CFG_NETWORK_TYPE;

// FIXME: add packing
// Do not change this structure (synchronized with BCLSockServer)
typedef struct _BCL_DRV_DATA_EX_REQUEST
{
    uint32         mode;
    uint32         category;
    uint32         index;
    uint32         datalen;
} BCL_DRV_DATA_EX_REQUEST;

// Do not change these numbers (synchronized with BCLSockServer)
typedef enum
{
    BclDrvModeCatInit  = 1,
    BclDrvModeCatFree  = 2,
    BclDrvModeNameGet  = 3,
    BclDrvModeValGet   = 4,
    BclDrvModeValSet   = 5
} BCL_DRV_DATA_EX_MODE;

enum bridge_mode {
  BR_MODE_NONE        = 0,
  BR_MODE_WDS         = 1,
  BR_MODE_L2NAT       = 2,
  BR_MODE_MAC_CLONING = 3,
  __BR_MODE_LAST
};

#define BR_MODE_LAST (__BR_MODE_LAST - 1)

void mac_reset_stats (mtlk_core_t *nic);
UMI_GET_STATISTICS *mac_get_stats(mtlk_core_t *nic, mtlk_txmm_msg_t *man_msg);
int mtlk_activate_card (mtlk_core_t *nic);
int mtlk_xmit (struct sk_buff *skb, struct net_device *dev);
void mtlk_xmit_err (struct nic *nic, struct sk_buff *skb);
int mtlk_send_activate (struct nic *nic);
int mtlk_disconnect_me(struct nic *nic);
int mtlk_disconnect_sta(struct nic *nic, unsigned char *mac);
void mtlk_send_null_packet (struct nic *nic, sta_entry *sta);
int mtlk_detect_replay_or_sendup(struct sk_buff *skb, u8 *rsn);
char *mtlk_net_state_to_string(uint32 state);
void mtlk_init_iw_qual(struct iw_quality *pqual, uint8 mac_rssi, uint8 mac_noise);
int mtlk_core_get_net_state(mtlk_core_t *core);
int mtlk_core_set_net_state(mtlk_core_t *core, uint32 new_state);
uint32 mtlk_core_get_available_bitrates (struct nic *nic);
int mtlk_core_gen_dataex_send_mac_leds(struct nic *nic, WE_GEN_DATAEX_REQUEST *req, WE_GEN_DATAEX_RESPONSE *resp, void *usp_data);
int mtlk_core_gen_dataex_get_status (struct nic *nic, WE_GEN_DATAEX_REQUEST *preq, WE_GEN_DATAEX_RESPONSE *presp, void *usp_data);
int mtlk_core_gen_dataex_get_connection_stats (struct nic *nic, WE_GEN_DATAEX_REQUEST *preq, WE_GEN_DATAEX_RESPONSE *presp, void *usp_data);


int mtlk_core_set_gen_ie (struct nic *nic, u8 *ie, u16 ie_len, u8 ie_type);
int mtlk_core_validate_wep_key (uint8 *key, size_t length);
void mtlk_core_sw_reset_enable_set (int32 value, struct nic *nic);
uint16 mtlk_core_get_rate_for_addba (struct nic *nic);
mtlk_hw_state_e mtlk_core_get_hw_state (mtlk_core_t *nic);
int mtlk_core_send_mac_addr_tohw (struct nic *nic, const char * mac);
int __MTLK_IFUNC
mtlk_core_set_network_mode(mtlk_core_t* nic, uint8 net_mode);
int mtlk_core_sw_reset_enable_get (struct nic *nic);

int mtlk_set_hw_state(mtlk_core_t *nic, mtlk_hw_state_e st);

uint16 mtlk_core_get_rate_for_addba (struct nic *nic);

void mtlk_core_schedule_disconnect(struct nic *nic);

int __MTLK_IFUNC mtlk_core_set_acl(struct nic *nic, IEEE_ADDR *mac, IEEE_ADDR *mac_mask);
int __MTLK_IFUNC mtlk_core_del_acl(struct nic *nic, IEEE_ADDR *mac);

uint8 __MTLK_IFUNC mtlk_core_calc_signal_strength (int8 RSSI);
BOOL __MTLK_IFUNC mtlk_core_is_sm_channels_disabled(struct nic *nic);
mtlk_handle_t __MTLK_IFUNC mtlk_core_get_tx_limits_handle(mtlk_handle_t nic);
void __MTLK_IFUNC mtlk_core_handle_event(struct nic* nic, EVENT_ID id, const void *payload);

void __MTLK_IFUNC
mtlk_core_configuration_dump(mtlk_core_t *core);

static __INLINE uint8 
mtlk_core_get_spectrum(struct nic *nic)
{
  return nic->slow_ctx->spectrum_mode;
}

static __INLINE uint8
mtlk_core_get_band(struct nic *nic)
{
  return nic->slow_ctx->frequency_band_cur;
}

static __INLINE uint8 
mtlk_core_get_last_pm_spectrum(struct nic *nic)
{
  return nic->slow_ctx->last_pm_spectrum;
}

static __INLINE uint8
mtlk_core_get_last_pm_freq(struct nic *nic)
{
  return nic->slow_ctx->last_pm_freq;
}

static __INLINE const uint8 * 
mtlk_core_get_bssid(struct nic *nic)
{
  return nic->slow_ctx->bssid;
}

static __INLINE uint8
mtlk_core_get_network_mode(struct nic *nic)
{
  return nic->slow_ctx->net_mode_cur;
}

static __INLINE BOOL
mtlk_core_scan_is_running(struct nic *nic)
{
  return mtlk_scan_is_running(&nic->slow_ctx->scan);
}

BOOL __MTLK_IFUNC
mtlk_core_is_stopping(mtlk_core_t *core);

int __MTLK_IFUNC
mtlk_core_get_tx_power_limit(mtlk_core_t *core, uint8 *power_limit);

int __MTLK_IFUNC
mtlk_core_change_tx_power_limit(mtlk_core_t *core, uint8 power_limit);

int __MTLK_IFUNC
mtlk_core_set_bonding(mtlk_core_t *core, uint8 bonding);

#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
BOOL __MTLK_IFUNC
mtlk_core_ppa_is_available(struct nic* nic);
BOOL __MTLK_IFUNC
mtlk_core_ppa_is_registered(struct nic* nic);
int __MTLK_IFUNC
mtlk_core_ppa_register(struct nic* nic);
void __MTLK_IFUNC
mtlk_core_ppa_unregister(struct nic* nic);
#endif /*CONFIG_IFX_PPA_API_DIRECTPATH*/

#endif
