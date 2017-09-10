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
* Written by: Dmitry Fleytman
*
*/

#ifndef _MTLK_STADB_H_
#define _MTLK_STADB_H_

#include "mtlklist.h"
#include "mtlk_osal.h"
#include "mtlkqos.h"
#include "addba.h"
#include "rod.h"
#include "mtlk_sq.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

#define STA_MAX_PEERS     (64)  // must be power of 2, defines N of hash entries
#define STA_MAX_STATIONS  (16)
#define STA_MAX_HOSTS     (32)

#define PEER_HASH(mac)    (mac[5] & (STA_MAX_PEERS - 1))

#define DEFAULT_BEACON_INTERVAL (100)

typedef enum _peer_type
{
    PEER_STA = 1,
    PEER_HOST = 2
} peer_type;

typedef struct _sta_stats
{
    uint32 rx_packets;   /* number of packets received */
    uint32 tx_packets;   /* number of packets transmitted */
    uint32 rx_dropped;   /* number of received packets that were dropped */
    uint32 tx_dropped;   /* number of packets dropped (no free tx MSDU) */
    uint32 tx_failed;    /* number of packets failed to transmit (MAC timeout reached) */
} __MTLK_IDATA sta_stats;

typedef enum {
    PEER_UNUSED,          // show that peer entry is unused
    PEER_CONNECTED,       // 
    PEER_DISCONNECTING    // disconnect initiated
} peer_state;

/* common part for HOST and STA */
#define PEER_HEADER                         \
    peer_state            state;            \
    uint8                 mac[ETH_ALEN];    \
    sta_stats             stats;            \
    mtlk_osal_spinlock_t  lock;             \
    peer_type             type;             \
    mtlk_osal_timer_t     idle_timer;       \
    mtlk_osal_timestamp_t timestamp;        \
    mtlk_slist_entry_t    next_peer

typedef struct _peer_entry {
    PEER_HEADER;
} __MTLK_IDATA peer_entry;

enum sta_entry_async_man_msgs
{
  STAE_AMM_DISCONNECT,
  STAE_AMM_SET_KEY,
  STAE_AMM_LAST
};

typedef enum _mtlk_pckt_filter_e {
  MTLK_PCKT_FLTR_ALLOW_ALL,
  MTLK_PCKT_FLTR_ALLOW_802_1X,
  MTLK_PCKT_FLTR_DISCARD_ALL
} mtlk_pckt_filter_e;

struct _sta_entry {
  PEER_HEADER;
  /* per-STA part */
  reordering_queue      rod_queue[NTS_TIDS];  // reordering structures per STA
  uint8                 dot11n_mode;          // .11n support mode?
  uint8                 peer_ap;              // Is peer AP?
  mtlk_addba_peer_t     addba_peer;           // addba context
  uint8                 cipher;               // TKIP or CCMP
  mtlk_atomic_t         filter;               // flag for packets filtering
  mtlk_osal_timer_t     keepalive_timer;      // Timer for sending NULL data packets
  mtlk_atomic_t         tx_packets[NTS_TIDS];  // TX packets counters per STA
  BOOL                  aggr_inited[NTS_TIDS];  // aggregation inited flag per STA
  uint8 net_mode;
  uint8 rssi[NUM_OF_RX_ANT];
  uint16 tx_rate;
  mtlk_sq_peer_ctx_t     sq_peer_ctx;
  /* Description of AP */
  uint16                beacon_interval;      // AP's beacon interval
  mtlk_osal_timestamp_t beacon_timestamp;     // AP's last beacon timestamp
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  /* RF Management related info */
  uint8                rf_mgmt_data;
#endif
  mtlk_txmm_msg_t       async_man_msgs[STAE_AMM_LAST];
} __MTLK_IDATA;

typedef struct _host_entry {
  PEER_HEADER;
  /* per-HOST part */
  int32             sta_id;                   // STA WDS id in connected_stations
} __MTLK_IDATA host_entry;

struct nic;

typedef struct _sta_db
{
  sta_entry     connected_stations[STA_MAX_STATIONS];
  host_entry    found_hosts[STA_MAX_HOSTS];
  mtlk_slist_t  peers[STA_MAX_PEERS];
  uint32        wds_host_timeout;
  uint32        sta_keepalive_timeout;
  uint32        keepalive_interval;
  uint32        aggr_open_threshold;
  mtlk_sq_t     *sq;
  // Port this structure to be CP
  struct nic    *nic;
  /* default host related */
  uint8  default_host[ETH_ALEN];
  uint8  local_mac[ETH_ALEN];
} __MTLK_IDATA sta_db;

int __MTLK_IFUNC
mtlk_stadb_add_sta (sta_db *stadb, unsigned char *mac,
                   uint8 dot11n_mode);
int __MTLK_IFUNC
mtlk_stadb_add_peer_ap (sta_db *stadb, unsigned char *mac,
                   uint8 dot11n_mode);
int __MTLK_IFUNC
mtlk_stadb_update_sta (sta_entry *sta);

void __MTLK_IFUNC
mtlk_stadb_update_sta_tx(sta_db *stadb, sta_entry *sta, uint16 tid);

int __MTLK_IFUNC
mtlk_stadb_update_ap (sta_entry *sta, uint16 beacon_interval);

int __MTLK_IFUNC
mtlk_stadb_update_host (sta_db *stadb, const unsigned char *mac, 
                        int sta_id);

int __MTLK_IFUNC
mtlk_stadb_add_host (sta_db *stadb, const unsigned char *mac,
                     int sta_id);

int __MTLK_IFUNC
mtlk_stadb_remove_host (sta_db *stadb, host_entry *host);

int __MTLK_IFUNC
mtlk_stadb_remove_sta (sta_db *stadb, sta_entry *sta);

int __MTLK_IFUNC
mtlk_stadb_find_sta (sta_db *stadb, const unsigned char *mac);

void __MTLK_IFUNC
mtlk_stadb_flush_sta (sta_db *stadb);

void __MTLK_IFUNC
mtlk_stadb_init (sta_db *stadb, struct nic *nic, mtlk_sq_t *sq);

void __MTLK_IFUNC
mtlk_stadb_clear (sta_db *stadb);

void __MTLK_IFUNC
mtlk_stadb_sta_set_filter(sta_entry *sta, mtlk_pckt_filter_e filter);

static __INLINE mtlk_pckt_filter_e
mtlk_stadb_sta_get_filter(sta_entry *sta)
{
  return (mtlk_pckt_filter_e)mtlk_osal_atomic_get(&sta->filter);
}

static __INLINE sta_entry *
mtlk_stadb_get_sta_by_id(sta_db *stadb, int id)
{
  ASSERT(id >= 0);
  return &stadb->connected_stations[id];
}

static __INLINE host_entry*
mtlk_stadb_get_host_by_id(sta_db *stadb, int id)
{
  ASSERT(id >= 0);
  return &stadb->found_hosts[id];
}

static __INLINE int
mtlk_stadb_get_host_id_by_host(sta_db *stadb, host_entry* host)
{
  ASSERT( host - stadb->found_hosts >= 0 );
  ASSERT( host - stadb->found_hosts < STA_MAX_HOSTS );
  return (int) (host - stadb->found_hosts);
}

static __INLINE void
mtlk_stadb_prepare_disconnect_sta(sta_db *stadb, sta_entry *sta)
{
  MTLK_UNREFERENCED_PARAM(stadb);
  // Stop timers
  mtlk_osal_timer_cancel_sync(&sta->idle_timer);
  mtlk_osal_timer_cancel_sync(&sta->keepalive_timer);
  sta->state = PEER_DISCONNECTING;
}

#ifdef MTCFG_RF_MANAGEMENT_MTLK
static __INLINE void
mtlk_stadb_set_sta_rf_mgmt_data (sta_entry *sta, uint8 rf_mgmt_data)
{
  sta->rf_mgmt_data = rf_mgmt_data;
}

static __INLINE uint8
mtlk_stadb_get_sta_rf_mgmt_data (sta_entry *sta)
{
  return sta->rf_mgmt_data;
}
#endif

static __INLINE uint8 *
mtlk_stadb_get_default_host (sta_db* stadb)
{
  if (mtlk_osal_is_valid_ether_addr(stadb->default_host))
    return stadb->default_host;
  return NULL;
}

int __MTLK_IFUNC mtlk_stadb_set_local_mac (mtlk_handle_t usr_data,
                                           const unsigned char *mac);
void __MTLK_IFUNC mtlk_stadb_get_local_mac (mtlk_handle_t usr_data,
                                            unsigned char *mac);

/*****************************************************************************
 * ADDBA module wrapper
******************************************************************************/
mtlk_addba_peer_t* __MTLK_IFUNC mtlk_stadb_get_addba_peer(mtlk_handle_t usr_data, 
                                                const IEEE_ADDR* addr, 
                                                mtlk_handle_t* container_handle);

uint32 __MTLK_IFUNC mtlk_stadb_get_addba_last_rx_timestamp(mtlk_handle_t usr_data, 
                                                 mtlk_handle_t container_handle, 
                                                 uint16 tid);

void __MTLK_IFUNC mtlk_stadb_addba_do_nothing(mtlk_handle_t usr_data,
                                              mtlk_handle_t container_handle, /* from the get_peer() - to avoid double searching by MAC */
                                              uint16        prior_class);

void __MTLK_IFUNC mtlk_stadb_addba_revive_aggregation_tid(mtlk_handle_t usr_data,
                                                          mtlk_handle_t container_handle, /* from the get_peer() - to avoid double searching by MAC */
                                                          uint16        prior_class);

void __MTLK_IFUNC mtlk_stadb_addba_restart_tx_count_tid(mtlk_handle_t usr_data,
                                                        mtlk_handle_t container_handle, /* from the get_peer() - to avoid double searching by MAC */
                                                        uint16        prior_class);

void __MTLK_IFUNC mtlk_stadb_addba_start_reordering(mtlk_handle_t usr_data,
                                                    mtlk_handle_t container_handle, /* from the get_peer() - to avoid double searching by MAC */
                                                    uint16        prior_class,
                                                    uint16        ssn,
                                                    uint8         win_size);

void __MTLK_IFUNC mtlk_stadb_addba_stop_reordering(mtlk_handle_t usr_data,
                                                   mtlk_handle_t container_handle, /* from the get_peer() - to avoid double searching by MAC */
                                                   uint16        prior_class);
void __MTLK_IFUNC mtlk_stadb_addba_revive_aggregation_all(mtlk_handle_t usr_data,
                                                          mtlk_handle_t container_handle, /* from the get_peer() - to avoid double searching by MAC */
                                                          uint16        prior_class);


void __MTLK_IFUNC mtlk_stadb_set_pm_enabled(sta_entry *sta, BOOL enabled);


void __MTLK_IFUNC mtlk_stadb_on_aggregation_revive_tid(const void *notification_payload);
void __MTLK_IFUNC mtlk_stadb_on_aggregation_revive_all(const void *notification_payload);


#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* !_MTLK_STADB_H_ */
