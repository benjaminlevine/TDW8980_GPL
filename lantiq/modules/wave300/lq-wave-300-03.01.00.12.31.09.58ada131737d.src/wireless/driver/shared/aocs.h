/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __MTLK_AOCS_H__
#define __MTLK_AOCS_H__

#include "mtlk_osal.h"
#include "mtlkaux.h"

#include "mhi_umi.h"
#include "mtlklist.h"

#include "txmm.h"
#include "frame.h"

#include "mtlkqos.h"
#include "aocshistory.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"


#define MTLK_AOCS_PENALTIES_BUFSIZE 2

typedef enum _mtlk_aocs_weight_e {
  AOCS_WEIGHT_IDX_CL,
  AOCS_WEIGHT_IDX_TX,
  AOCS_WEIGHT_IDX_BSS,
  AOCS_WEIGHT_IDX_SM,
  AOCS_WEIGHT_IDX_LAST
} mtlk_aocs_weight_e;

/* channel's list entry */
typedef struct _mtlk_aocs_channel_data_t {
  uint16 channel;
  uint8 nof_bss;
  uint8 channel_load;
  mtlk_osal_timestamp_t time_cl;
  BOOL sm_required;
  uint8 num_20mhz_bss;
  uint8 num_40mhz_bss;
  /* affected by 40MHz intolerance */
  BOOL forty_mhz_int_affected;
  BOOL forty_mhz_intolerant;
#ifdef AOCS_DEBUG_40MHZ_INT
  BOOL emulate_forty_mhz_int;
#endif
  /* linked list */
  mtlk_slist_entry_t link_entry;
} __MTLK_IDATA mtlk_aocs_channel_data_t;

/* AOCS table entry */
typedef struct _mtlk_aocs_table_entry_t {
  /* CB - primary channel, nCB - 20 MHz wide channel */
  mtlk_aocs_channel_data_t *chnl_primary;
  /* CB - secondary channel, nCB - NULL */
  mtlk_aocs_channel_data_t *chnl_secondary;
  /* TxPower limit: for CB mode TxPower limit is not equal
     to min(TxPower_prim, TxPower_sec), because of HW limits
     influence. Thus, we don't store this value in 20MHz channel's
     data */
  uint16 max_tx_power;
  uint16 tx_power_penalty;
  /* exclude channel from selection: 2.4GHz nCB, 5.2GHz CB while CL exclude */
  BOOL exclude;
  /* was this channel forcibly added to the table - possible on 2.4 GHz band */
  BOOL forcibly_added;

  /* scan rank (metric based on scan results) of the channel */
  uint8 scan_rank;

  /* confirm rank (metric based on actual throughput) of the channel */
  uint8 confirm_rank;
  mtlk_osal_msec_t confirm_rank_update_time;

  /* don't use this channel - exclude from AOCS - debug, fine-tune */
  BOOL dont_use;

  /* TODO: entries below should be moved to Channel Mananger */
  /* channel's last clear check time - 11h, ms */
  mtlk_osal_msec_t time_ms_last_clear_check;
  /* non occupied period, ms */
  mtlk_osal_msec_t time_ms_non_occupied_period;
  /* was radar detected */
  BOOL radar_detected;

  /* linked list */
  mtlk_slist_entry_t link_entry;
} __MTLK_IDATA mtlk_aocs_table_entry_t;

/* AOCS supported events */
typedef enum _mtlk_aocs_event_e {
  /* on radar detected */
  MTLK_AOCS_EVENT_RADAR_DETECTED,
  /* request to select a new channel */
  MTLK_AOCS_EVENT_SELECT_CHANNEL,
  /* channel switch started */
  MTLK_AOCS_EVENT_SWITCH_STARTED,
  /* channel switch done */
  MTLK_AOCS_EVENT_SWITCH_DONE,
  /* initial channel selected */
  MTLK_AOCS_EVENT_INITIAL_SELECTED,
  /* MAC TCP AOCS indication received */
  MTLK_AOCS_EVENT_TCP_IND,
  MTLK_AOCS_EVENT_LAST
} mtlk_aocs_event_e;

/* on channel select event data */
typedef struct _mtlk_aocs_evt_select_t {
  uint16             channel;
  uint8              bonding;
  switch_reasons_t   reason;
  channel_criteria_t criteria;
  channel_criteria_details_t criteria_details;
} mtlk_aocs_evt_select_t;

/* on channel switch status */
typedef struct _mtlk_aocs_evt_switch_t {
  int status;
  uint16 sq_used[NTS_PRIORITIES];
} mtlk_aocs_evt_switch_t;

/* TxPowerPenalty list entry */
typedef struct _mtlk_aocs_tx_penalty_t {
  uint16 freq;
  uint16 penalty;
  /* linked list */
  mtlk_slist_entry_t link_entry;
} __MTLK_IDATA mtlk_aocs_tx_penalty_t;

/* restricted channel list entry */
typedef struct _mtlk_aocs_restricted_chnl_t {
  uint8 channel;
  /* linked list */
  mtlk_slist_entry_t link_entry;
} __MTLK_IDATA mtlk_aocs_restricted_chnl_t;

#ifdef AOCS_DEBUG_40MHZ_INT
/* 40MHz intolerant channel list entry */
typedef struct _mtlk_aocs_debug_40mhz_int_chnl_t {
  uint8 channel;
  /* linked list */
  mtlk_slist_entry_t link_entry;
} __MTLK_IDATA mtlk_aocs_debug_40mhz_int_chnl_t;
#endif

/* API */
typedef struct _mtlk_aocs_wrap_api_t {
  /* card's context */
  mtlk_core_t *core;
  void (__MTLK_IDATA *on_channel_change)(mtlk_core_t *core, int channel);
  void (__MTLK_IDATA *on_bonding_change)(mtlk_core_t *core, uint8 bonding);
  void (__MTLK_IDATA *on_spectrum_change)(mtlk_core_t *core, int spectrum);
} __MTLK_IDATA mtlk_aocs_wrap_api_t;

struct mtlk_scan;
struct _mtlk_dot11h_t;
struct _scan_cache_t;

typedef struct _mtlk_aocs_udp_config_t {
  uint16 msdu_threshold_aocs;
  uint16 lower_threshold;
  uint16 threshold_window;
  /* MSDU timeout */
  uint32 aocs_window_time_ms;
  /* number of packets that were acknowledged - switch threshold */
  uint32 msdu_per_window_threshold;
  /* is MSDU debug enabled */
  BOOL msdu_debug_enabled;
  /* access categories for which AOCS shall be enabled */
  BOOL aocs_enabled_tx_ac[NTS_PRIORITIES];
  /* access categories for which AOCS shall count RX packets */
  BOOL aocs_enabled_rx_ac[NTS_PRIORITIES];
} __MTLK_IDATA mtlk_aocs_udp_config_t;

typedef struct _mtlk_aocs_tcp_config_t {
    uint16 measurement_window;
    uint32 throughput_threshold;
} __MTLK_IDATA mtlk_aocs_tcp_config_t;

typedef enum {
  MTLK_AOCST_NONE,
  MTLK_AOCST_UDP,
  MTLK_AOCST_TCP,
  MTLK_AOCST_LAST
} mtlk_aocs_type_e;

/* AOCS configuration */
typedef struct _mtlk_aocs_config_t {
  /* is AOCS enabled */
  mtlk_aocs_type_e type;

  BOOL is_ht;
  uint8 frequency_band;
  uint8 spectrum_mode;
  BOOL is_auto_spectrum;
  mtlk_aocs_wrap_api_t api;
  struct mtlk_scan *scan_data;
  struct _scan_cache_t *cache;
  struct _mtlk_dot11h_t *dot11h;
  mtlk_txmm_t *txmm;
  /* rank switch threshold */
  uint8 cfm_rank_sw_threshold;
  /* scan cache aging */
  mtlk_osal_msec_t scan_aging_ms;
  /* confirm rank aging */
  mtlk_osal_msec_t confirm_rank_aging_ms;
  /* alpha filter coefficient, % */
  uint8 alpha_filter_coefficient;
  /* TRUE if TxPowerPenalties are used */
  BOOL use_tx_penalties;
  BOOL disable_sm_channels;
  mtlk_aocs_udp_config_t udp;
  mtlk_aocs_tcp_config_t tcp;
} __MTLK_IDATA mtlk_aocs_config_t;

/* AOCS context */
typedef struct _mtlk_aocs_t {
  BOOL initialized;
  mtlk_osal_mutex_t watchdog_mutex;
  BOOL              watchdog_started;
  uint16 cur_channel;
  /* configuration of the module */
  mtlk_aocs_config_t config;
  /* current bonding: upper/lower for CB */
  uint8 bonding;
  /* AOCS table */
  mtlk_slist_t table;
  /* AOCS channel's list */
  mtlk_slist_t channel_list;
  mtlk_osal_spinlock_t lock;
  uint8 weight_ch_load;
  uint8 weight_nof_bss;
  uint8 weight_tx_power;
  uint8 weight_sm_required;
  /* maximum of the tx power for all channels in the OCS table */
  uint16 max_tx_power;
  /* maximum number of BSS on a channel found */
  uint8 max_nof_bss;
  /* is channel switch in progress */
  BOOL ch_sw_in_progress;
  
  /* whether SM required channels should be considered during channel selection */
  BOOL disable_sm_required;
  /* for the current channel it is a time to wait after radar was detected */
  uint16 channel_availability_check_time;

  /* MSDU threshold tracking timer */
  mtlk_osal_timer_t msdu_timer;
  /* Flag whether timer is scheduled */
  BOOL msdu_timer_running;
  mtlk_atomic_t msdu_counter;

  /* debug variables */
  int8 dbg_non_occupied_period;
  int8 dbg_radar_detection_validity_time;
  /* tx_power_penalty list of channels */
  mtlk_slist_t tx_penalty_list;
  /* restricted channels list */
  mtlk_slist_t restricted_chnl_list;
  /* switch history */
  mtlk_aocs_history_t aocs_history;
  /* effective access categories for which AOCS shall be enabled */
  BOOL aocs_effective_tx_ac[NTS_PRIORITIES];
  /* effective access categories for which AOCS shall count RX packets */
  BOOL aocs_effective_rx_ac[NTS_PRIORITIES];

  /* Minimum and maximum thresholds for AOCS algorithm */
  uint16 lower_threshold[NTS_PRIORITIES];
  uint16 higher_threshold[NTS_PRIORITIES];
  mtlk_osal_timestamp_t lower_threshold_crossed_time[NTS_PRIORITIES];
#ifdef AOCS_DEBUG_40MHZ_INT
  /* 40MHz intolerance debug */
  mtlk_slist_t debug_40mhz_int_list;
#endif
  mtlk_txmm_msg_t msdu_debug_man_msg;
} __MTLK_IDATA mtlk_aocs_t;

typedef struct _mtlk_aocs_init_t {
  mtlk_aocs_wrap_api_t *api;
  struct mtlk_scan *scan_data;
  struct _scan_cache_t *cache;
  struct _mtlk_dot11h_t *dot11h;
  mtlk_txmm_t *txmm;
  BOOL is_ap;
  BOOL disable_sm_channels;
} mtlk_aocs_init_t;

int __MTLK_IFUNC mtlk_aocs_init (mtlk_aocs_t *aocs, mtlk_aocs_init_t *ini_data);
int __MTLK_IFUNC mtlk_aocs_cleanup (mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_start (mtlk_aocs_t *aocs, BOOL keep_chnl_info);
void __MTLK_IFUNC mtlk_aocs_stop (mtlk_aocs_t *aocs);

int __MTLK_IFUNC mtlk_aocs_start_watchdog (mtlk_aocs_t *aocs);
void __MTLK_IFUNC mtlk_aocs_stop_watchdog (mtlk_aocs_t *aocs);

int __MTLK_IFUNC mtlk_aocs_indicate_event (mtlk_aocs_t *aocs,
  mtlk_aocs_event_e event, void *data);
int __MTLK_IFUNC mtlk_aocs_channel_in_validity_time(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_update_cl_on_scan_cfm (mtlk_aocs_t *aocs,
  FREQUENCY_ELEMENT* freq_element);
void __MTLK_IFUNC mtlk_aocs_update_cl (mtlk_aocs_t *aocs, uint16 channel, uint8 channel_load);
void __MTLK_IFUNC aocs_optimize_channel(mtlk_aocs_t *aocs, switch_reasons_t reason);

void __MTLK_IFUNC mtlk_aocs_print_table (mtlk_aocs_t *aocs, mtlk_seq_printf_t *s);
void __MTLK_IFUNC mtlk_aocs_print_channels (mtlk_aocs_t *aocs, mtlk_seq_printf_t *s);
void __MTLK_IFUNC mtlk_aocs_print_penalties (mtlk_aocs_t *aocs, mtlk_seq_printf_t *s);

int __MTLK_IFUNC mtlk_aocs_get_weight(mtlk_aocs_t *aocs, mtlk_aocs_weight_e index);
int __MTLK_IFUNC mtlk_aocs_set_weight(mtlk_aocs_t *aocs, mtlk_aocs_weight_e index, int32 weight);

int __MTLK_IFUNC mtlk_aocs_get_cfm_rank_sw_threshold(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_cfm_rank_sw_threshold(mtlk_aocs_t *aocs, uint8 value);
int __MTLK_IFUNC mtlk_aocs_get_scan_aging(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_scan_aging(mtlk_aocs_t *aocs, int value);
int __MTLK_IFUNC mtlk_aocs_get_confirm_rank_aging(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_confirm_rank_aging(mtlk_aocs_t *aocs, int value);
int __MTLK_IFUNC mtlk_aocs_get_afilter(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_afilter(mtlk_aocs_t *aocs, uint8 value);
int __MTLK_IFUNC mtlk_aocs_get_penalty_enabled(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_penalty_enabled(mtlk_aocs_t *aocs, BOOL value);

static int __INLINE
mtlk_aocs_get_msdu_threshold(mtlk_aocs_t *aocs)
{
    return aocs->config.udp.msdu_threshold_aocs;
}

static int __INLINE
mtlk_aocs_set_msdu_threshold(mtlk_aocs_t *aocs, uint32 value)
{
  aocs->config.udp.msdu_threshold_aocs = value;
  return MTLK_ERR_OK;
}

static int __INLINE
mtlk_aocs_get_lower_threshold(mtlk_aocs_t *aocs)
{
    return aocs->config.udp.lower_threshold;
}

static int __INLINE
mtlk_aocs_set_lower_threshold(mtlk_aocs_t *aocs, uint32 value)
{
    aocs->config.udp.lower_threshold = value;
    return MTLK_ERR_OK;
}

static int __INLINE
mtlk_aocs_get_threshold_window(mtlk_aocs_t *aocs)
{
    return aocs->config.udp.threshold_window;
}

static int __INLINE
mtlk_aocs_set_threshold_window(mtlk_aocs_t *aocs, uint32 value)
{
    aocs->config.udp.threshold_window = value;
    return MTLK_ERR_OK;
}

int __MTLK_IFUNC mtlk_aocs_get_win_time(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_win_time(mtlk_aocs_t *aocs, uint32 value);
int __MTLK_IFUNC mtlk_aocs_get_msdu_win_thr(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_msdu_win_thr(mtlk_aocs_t *aocs, uint32 value);
int __MTLK_IFUNC mtlk_aocs_get_msdu_debug_enabled(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_msdu_debug_enabled(mtlk_aocs_t *aocs, uint32 value);
int __MTLK_IFUNC mtlk_aocs_get_type(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_type(mtlk_aocs_t *aocs, uint32 value);

int __MTLK_IFUNC mtlk_aocs_get_restricted_ch(mtlk_aocs_t *aocs, char *buffer, size_t len);
void __MTLK_IFUNC mtlk_aocs_set_restricted_ch(mtlk_aocs_t *aocs, char *buffer, size_t len);
size_t __MTLK_IFUNC mtlk_aocs_get_tx_penalty(mtlk_handle_t handle, char *buffer);
int __MTLK_IFUNC mtlk_aocs_set_tx_penalty (mtlk_aocs_t *aocs, int32 *v, int nof_ints);
int __MTLK_IFUNC mtlk_aocs_get_msdu_tx_ac(mtlk_aocs_t *aocs, char *buffer, size_t len);
int __MTLK_IFUNC mtlk_aocs_set_msdu_tx_ac(mtlk_aocs_t *aocs, char *buffer, size_t len);
int __MTLK_IFUNC mtlk_aocs_get_msdu_rx_ac(mtlk_aocs_t *aocs, char *buffer, size_t len);
int __MTLK_IFUNC mtlk_aocs_set_msdu_rx_ac(mtlk_aocs_t *aocs, char *buffer, size_t len);
void __MTLK_IFUNC mtlk_aocs_on_bss_data_update(mtlk_aocs_t *aocs, bss_data_t *bss_data);
BOOL __MTLK_IFUNC mtlk_aocs_set_auto_spectrum(mtlk_aocs_t *aocs, uint8 spectrum);

#ifdef AOCS_DEBUG_40MHZ_INT
int __MTLK_IFUNC mtlk_aocs_debug_get_40mhz_int(mtlk_aocs_t *aocs, char *buffer, size_t len);
void __MTLK_IFUNC mtlk_aocs_debug_set_40mhz_int(mtlk_aocs_t *aocs, char *buffer, size_t len);
#endif

int __MTLK_IFUNC
aocs_notify_mac(mtlk_aocs_t *aocs, uint32 req_type, uint32 data);
uint32 __MTLK_IFUNC
aocs_msdu_tmr(mtlk_osal_timer_t *timer, mtlk_handle_t clb_usr_data);

static void __INLINE
aocs_notify_mac_timer(mtlk_aocs_t *aocs, int timer_event)
{
  if (aocs->config.udp.msdu_debug_enabled)
      aocs_notify_mac(aocs, timer_event, 
        mtlk_osal_atomic_get(&aocs->msdu_counter));
}

static void __INLINE /* HOTPATH */
aocs_start_msdu_timer (mtlk_aocs_t *aocs)
{
  if(!aocs->msdu_timer_running)
  {
    aocs->msdu_timer_running = TRUE;
    mtlk_osal_timer_set(&aocs->msdu_timer, aocs->config.udp.aocs_window_time_ms);
    aocs_notify_mac_timer(aocs, MAC_OCS_TIMER_START);
  }
}

static void __INLINE /*HOTPATH*/
aocs_stop_msdu_timer (mtlk_aocs_t *aocs)
{
  if(aocs->msdu_timer_running)
  {
    aocs_notify_mac_timer(aocs, MAC_OCS_TIMER_STOP);
    mtlk_osal_timer_cancel(&aocs->msdu_timer);
    aocs->msdu_timer_running = FALSE;
  }
}

static void __INLINE /*HOTPATH*/
mtlk_aocs_chk_msdu_threshold(mtlk_aocs_t *aocs, const uint16* nof_msdu_used_by_ac)
{
  int i;
  uint32 nof_msdu_used = 0;

  for(i = 0; i < NTS_PRIORITIES; i++)
  {
    MTLK_ASSERT( (0 == aocs->aocs_effective_tx_ac[i]) ||
                 (1 == aocs->aocs_effective_tx_ac[i]) );
    nof_msdu_used += nof_msdu_used_by_ac[i] * aocs->aocs_effective_tx_ac[i];
  }

  if (nof_msdu_used < aocs->config.udp.msdu_threshold_aocs) {
    aocs_stop_msdu_timer(aocs);
  } else if (!aocs->ch_sw_in_progress) {
    /* If channel switch is in progress we don't need to start the timer */
    /* If timer is already running we don't need to restart it */
    aocs_start_msdu_timer(aocs);
  }
}

static void __INLINE
aocs_move_thresholds(mtlk_aocs_t *aocs, uint8 ac, uint16 lower_threshold)
{
  aocs->lower_threshold[ac] = lower_threshold;
  aocs->higher_threshold[ac] =
    lower_threshold + aocs->config.udp.threshold_window;
}

static void __INLINE
aocs_lower_threshold_crossed(mtlk_aocs_t *aocs, uint8 ac)
{
  aocs->lower_threshold_crossed_time[ac] = mtlk_osal_timestamp();
  mtlk_osal_atomic_set(&aocs->msdu_counter, 0);
}

static uint8 __INLINE
aocs_calc_confirm_rank(int msdu_confirm_threshold, int msdu_confirmed)
{
  return ((msdu_confirm_threshold - msdu_confirmed)*100) 
    / (msdu_confirm_threshold);
};

void __MTLK_IFUNC
aocs_set_confirm_rank(mtlk_aocs_t *aocs, uint16 channel, uint8 rank);

static void __INLINE /*HOTPATH*/
__aocs_chk_sqsize_threshold(mtlk_aocs_t *aocs, uint16 sq_used, 
                               uint8 ac)
{
  if(sq_used == aocs->lower_threshold[ac])
  {
    aocs_lower_threshold_crossed(aocs, ac);
  }
  else if(sq_used == aocs->higher_threshold[ac])
  {
    mtlk_osal_ms_diff_t time_diff =
        mtlk_osal_ms_time_diff(mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp()),
                               mtlk_osal_timestamp_to_ms(aocs->lower_threshold_crossed_time[ac]));
    uint16 msdu_per_window = (time_diff != 0) 
        ? (mtlk_osal_atomic_get(&aocs->msdu_counter) * aocs->config.udp.aocs_window_time_ms) / time_diff
        : 0;

    aocs_move_thresholds(aocs, ac, aocs->higher_threshold[ac]);
    aocs_lower_threshold_crossed(aocs, ac);

    if(msdu_per_window < aocs->config.udp.msdu_per_window_threshold)
    {
      /* update confirm rank */
      aocs_set_confirm_rank(aocs, aocs->cur_channel,
          aocs_calc_confirm_rank(aocs->config.udp.msdu_per_window_threshold, msdu_per_window));

      /* initiate channel switch */
      aocs_optimize_channel(aocs, SWR_HIGH_SQ_LOAD);
      return;
    }
  }
}

/* NOTE: aocs_effective_tx_ac[ac] and aocs_effective_rx_ac[ac] become 0 when
 * some of the following conditions are true:
 *  - AOCS is disabled 
 *  - a non-UDP AOCS algorithm is enabled (for example, TCP)
 *  - UDP AOCS algotrithm for this AC is disabled by user
 */

static void __INLINE /*HOTPATH*/
mtlk_aocs_on_tx_msdu_sent(mtlk_aocs_t *aocs, const uint16* nof_msdu_used_by_ac, 
                          uint8 ac, uint16 sq_size_limit, uint16 sq_used)
{
  int new_lower_threshold;

  MTLK_ASSERT(ac < NTS_PRIORITIES);

  if (!aocs->aocs_effective_tx_ac[ac])
    return;

  mtlk_osal_lock_acquire(&aocs->lock);

  /* If upper threshold is above the send queue capacity,
  AOCS uses timer-based algorithm */
  if( aocs->higher_threshold[ac] > sq_size_limit )
    mtlk_aocs_chk_msdu_threshold(aocs, nof_msdu_used_by_ac);

  new_lower_threshold = 
    MAX((int) aocs->lower_threshold[ac] - (int) aocs->config.udp.threshold_window, 
        (int) aocs->config.udp.lower_threshold);

  if( ((int) sq_used < new_lower_threshold) && 
      (aocs->lower_threshold[ac] > new_lower_threshold) )
    aocs_move_thresholds(aocs, ac, new_lower_threshold);

  mtlk_osal_lock_release(&aocs->lock);
}

static void __INLINE /*HOTPATH*/
mtlk_aocs_on_tx_msdu_enqued(mtlk_aocs_t *aocs, uint16 ac, 
                            uint16 sq_size, uint16 sq_size_limit)
{
  MTLK_ASSERT(ac < NTS_PRIORITIES);

  if (!aocs->aocs_effective_tx_ac[ac])
    return;

   /* If upper threshold is below the send queue capacity,
      AOCS uses SendQueue-based algorithm */
   mtlk_osal_lock_acquire(&aocs->lock);

   if( aocs->higher_threshold[ac] <= sq_size_limit )
     __aocs_chk_sqsize_threshold(aocs, sq_size, ac);

   mtlk_osal_lock_release(&aocs->lock);
}

static void __INLINE /*HOTPATH*/
mtlk_aocs_on_tx_msdu_returned (mtlk_aocs_t *aocs, uint8 ac)
{
  MTLK_ASSERT(ac < NTS_PRIORITIES);
  if(aocs->aocs_effective_tx_ac[ac])
    mtlk_osal_atomic_inc(&aocs->msdu_counter);
}

static void __INLINE /*HOTPATH*/
mtlk_aocs_on_rx_msdu (mtlk_aocs_t *aocs, uint8 ac)
{
  MTLK_ASSERT(ac < NTS_PRIORITIES);
  if(aocs->aocs_effective_rx_ac[ac])
    mtlk_osal_atomic_inc(&aocs->msdu_counter);
}

static void __INLINE 
mtlk_aocs_enable_smrequired(mtlk_aocs_t *aocs)
{
  aocs->disable_sm_required = FALSE;
}

static void __INLINE 
mtlk_aocs_disable_smrequired(mtlk_aocs_t *aocs)
{
  aocs->disable_sm_required = TRUE;
}

static BOOL __INLINE
mtlk_aocs_is_smrequired_disabled(mtlk_aocs_t *aocs)
{     
  return aocs->disable_sm_required;
}     

/****************************************************************************
 * TCP AOCS algorithm related stuff
 ****************************************************************************/
#define MTLK_TCP_AOCS_MIN_THROUGHPUT_THRESHOLD 75000
#define MTLK_TCP_AOCS_MAX_THROUGHPUT_THRESHOLD 3500000

static int __INLINE
mtlk_aocs_set_measurement_window (mtlk_aocs_t *aocs, uint16 val)
{
  if (aocs->config.tcp.measurement_window != val &&
      mtlk_aocs_get_type(aocs) == MTLK_AOCST_TCP) {
    return MTLK_ERR_NOT_READY;
  }

  aocs->config.tcp.measurement_window = val;
  return MTLK_ERR_OK;
}

static uint16 __INLINE
mtlk_aocs_get_measurement_window (mtlk_aocs_t *aocs)
{
  return aocs->config.tcp.measurement_window;
}

static int __INLINE
mtlk_aocs_set_troughput_threshold (mtlk_aocs_t *aocs, uint32 val)
{
  if (val < MTLK_TCP_AOCS_MIN_THROUGHPUT_THRESHOLD ||
      val > MTLK_TCP_AOCS_MAX_THROUGHPUT_THRESHOLD) {
    return MTLK_ERR_PARAMS;
  }
  
  if (aocs->config.tcp.throughput_threshold != val &&
      mtlk_aocs_get_type(aocs) == MTLK_AOCST_TCP) {
    return MTLK_ERR_NOT_READY; 
  }

  aocs->config.tcp.throughput_threshold = val;
  return MTLK_ERR_OK;
}

static uint32 __INLINE
mtlk_aocs_get_troughput_threshold (mtlk_aocs_t *aocs)
{
  return aocs->config.tcp.throughput_threshold;
}

uint16 __MTLK_IFUNC
mtlk_aocs_get_cur_channel(mtlk_aocs_t *aocs);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif



