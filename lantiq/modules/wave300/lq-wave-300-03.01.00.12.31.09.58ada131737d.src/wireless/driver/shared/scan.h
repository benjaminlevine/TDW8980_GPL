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
 * Written by: Roman Sikorskyy & Andriy Fidrya
 *
 */

#ifndef __SCAN_H__
#define __SCAN_H__

#include "mhi_umi.h"
#include "mhi_ieee_address.h"
#include "mtlkerr.h"
#include "mtlk_osal.h"
#include "eeprom.h"
#include "frame.h"
#include "rsn.h"
#include "cache.h"
#include "aocs.h"
#include "mtlkflctrl.h"
#include "progmodel.h"

#define  MTLK_IDEFS_ON
//#define  MTLK_IDEFS_PACKING 1
#include "mtlkidefs.h"

#define SCAN_TIMEOUT (10000)

struct mtlk_scan_vector
{
  uint16 count;
  uint16 used;
  FREQUENCY_ELEMENT *params;
} __MTLK_IDATA;

/**
 * scan_params
 **/
struct mtlk_scan_params
{
  char essid[MIB_ESSID_LENGTH + 1]; /* ssid to connect to */
  uint8 bssid[IEEE_ADDR_LEN]; /* mac address to find */
  int bss_type; /* bss type: infra = 0, ad-hoc = 2, all = 3*/
  uint16 min_scan_time; /* in milliseconds */
  uint16 max_scan_time; /* in milliseconds */
  uint8 probe_rate;
  uint8 num_probes;
  uint8 channels_per_chunk_limit;
  uint16 pause_between_chunks;
  BOOL is_background;
} __MTLK_IDATA;

struct mtlk_scan_config {
  mtlk_core_t *core;
  struct mtlk_eq *eq;
  mtlk_txmm_t *txmm;
  mtlk_aocs_t *aocs;
  mtlk_flctrl_t *flctrl;
  scan_cache_t *bss_cache; 
} __MTLK_IDATA;

struct mtlk_scan
{
  struct mtlk_scan_config config;
  BOOL initialized;
  BOOL rescan;
  struct mtlk_scan_params params;
  uint8 ch_offset;
  uint16 last_channel;
  uint8 spectrum;
  struct mtlk_scan_vector vector;
  mtlk_osal_msec_t last_timestamp;
  uint8 orig_band;
  uint8 cur_band;
  uint8 next_band;  // for dual-band scan, indicates next band that should be scanned
  mtlk_osal_event_t completed; // for sync scan
  mtlk_atomic_t is_running;
  mtlk_atomic_t treminate_scan;
  mtlk_osal_spinlock_t lock;
  mtlk_osal_timer_t pause_timer;
  mtlk_handle_t flctrl_id;
  mtlk_progmodel_t *progmodels[MTLK_HW_BAND_BOTH];
  mtlk_txmm_msg_t async_scan_msg;
} __MTLK_IDATA;

int __MTLK_IFUNC mtlk_scan_init (struct mtlk_scan *scan, struct mtlk_scan_config config);
void __MTLK_IFUNC mtlk_scan_cleanup (struct mtlk_scan *scan);

void __MTLK_IFUNC mtlk_scan_handle_evt_pause_elapsed (struct mtlk_scan *scan);
void __MTLK_IFUNC mtlk_scan_handle_evt_scan_confirmed (struct mtlk_scan *scan, const void *data);
void __MTLK_IFUNC mtlk_scan_handle_bss_found_ind (struct mtlk_scan *scan, uint16 channel);
int __MTLK_IFUNC mtlk_scan_sync (struct mtlk_scan *scan, uint8 band, uint8 is_cb_scan);
int __MTLK_IFUNC mtlk_scan_async (struct mtlk_scan *scan, uint8 band, const char* essid);
void __MTLK_IFUNC mtlk_scan_set_background(struct mtlk_scan *scan, BOOL is_background);

void __MTLK_IFUNC mtlk_scan_schedule_rescan (struct mtlk_scan *scan);
int __MTLK_IFUNC mtlk_scan_set_essid_pattern (struct mtlk_scan *scan, const char *pattern);

void __MTLK_IFUNC mtlk_scan_set_essid (struct mtlk_scan *scan, const char *buf);
size_t __MTLK_IFUNC mtlk_scan_get_essid (struct mtlk_scan *scan, char *buf);

static __INLINE BOOL mtlk_scan_is_running(struct mtlk_scan *scan)
{
  return mtlk_osal_atomic_get(&scan->is_running);
}

void __MTLK_IFUNC
scan_terminate_and_wait_completion(struct mtlk_scan *scan);

void __MTLK_IFUNC scan_complete(struct mtlk_scan *scan);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __SCAN_H__ */
