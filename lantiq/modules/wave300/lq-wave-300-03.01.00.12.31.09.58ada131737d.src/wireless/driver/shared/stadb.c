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
#include "mtlkinc.h"

#include "rod.h"
#include "core.h"
#include "stadb.h"
#include "mtlk_osal.h"
#include "core.h"
#include "sq.h"

struct eq_evt_aggr_data
{
  sta_db    *stadb;
  sta_entry *sta;
  int        sta_id;
  uint16     prior_class;
};

// Timeout change adoptation interval
#define DEFAULT_KEEPALIVE_TIMEOUT 1000 /* ms */
#define DEFAULT_AGGR_OPEN_THRESHOLD 5

// XXX, Revisit to make CP
int mtlk_disconnect_sta (struct nic *nic, unsigned char *mac);
void mtlk_core_schedule_disconnect (struct nic *nic);
void mtlk_send_null_packet (struct nic *nic, sta_entry *sta);
int flctrl_is_stopped(mtlk_handle_t usr_data);

static __INLINE int
_mtlk_stadb_get_sta_id_by_sta (sta_db *stadb, sta_entry* sta)
{
  ASSERT( sta - stadb->connected_stations >= 0 );
  ASSERT( sta - stadb->connected_stations < STA_MAX_STATIONS );
  return (int) (sta - stadb->connected_stations);
}

// Timers
static uint32 __MTLK_IFUNC
keepalive_tmr (mtlk_osal_timer_t *timer, 
               mtlk_handle_t      clb_usr_data)
{
  sta_db *stadb = (sta_db *)clb_usr_data;
  sta_entry *sta = MTLK_CONTAINER_OF(timer, sta_entry, keepalive_timer);

  mtlk_osal_timestamp_diff_t diff;
  mtlk_osal_msec_t msecs, timeout = stadb->keepalive_interval;

  /* send NULL packet to connected station only, e.g. do not send
     to station being disconnected */
  if (sta->state != PEER_CONNECTED)
    return 0; /* do not restart timer */

  if (timeout == 0)
    return DEFAULT_KEEPALIVE_TIMEOUT; /* to adopt possible timeout change */

  diff = mtlk_osal_timestamp_diff(mtlk_osal_timestamp(), sta->timestamp);
  msecs = mtlk_osal_timestamp_to_ms(diff);

  if (msecs >= timeout) {
    mtlk_send_null_packet(stadb->nic, sta);
    return timeout; /* restart with same timeout */
  } else {
    return (timeout - msecs); /* restart with adjusted timeout */
  }
}



static uint32 __MTLK_IFUNC
data_disconnect_tmr (mtlk_osal_timer_t *timer, 
                     mtlk_handle_t      clb_usr_data)
{
  sta_db *stadb = (sta_db *)clb_usr_data;
  sta_entry *sta = MTLK_CONTAINER_OF(timer, sta_entry, idle_timer);

  mtlk_osal_timestamp_diff_t diff;
  mtlk_osal_msec_t msecs, timeout = stadb->sta_keepalive_timeout;

  if (sta->state != PEER_CONNECTED)
    return 0; /* do not restart timer */

  if (timeout == 0)
    return DEFAULT_KEEPALIVE_TIMEOUT; /* to adopt possible timeout change */

  if (flctrl_is_stopped((mtlk_handle_t)stadb->nic)) {
    mtlk_stadb_update_sta(sta);
    return timeout; /* restart with same timeout */
  }

  diff = mtlk_osal_timestamp_diff(mtlk_osal_timestamp(), sta->timestamp);
  msecs = mtlk_osal_timestamp_to_ms(diff);

  if (msecs >= timeout) {
    ILOG1(GID_DISCONNECT, "Disconnecting %Y due to data timeout", sta->mac);
    if (stadb->nic->ap)
      mtlk_disconnect_sta(stadb->nic, sta->mac);
    else
      mtlk_core_schedule_disconnect(stadb->nic);
    return 0; /* do not restart timer */
  } else {
    return (timeout - msecs); /* restart with adjusted timeout */
  }
}



static uint32 __MTLK_IFUNC
wds_disconnect_tmr (mtlk_osal_timer_t *timer, 
                    mtlk_handle_t      clb_usr_data)
{
  sta_db *stadb = (sta_db *)clb_usr_data;
  host_entry *host = MTLK_CONTAINER_OF(timer, host_entry, idle_timer);

  mtlk_osal_timestamp_diff_t diff;
  mtlk_osal_msec_t msecs, timeout = stadb->wds_host_timeout * 1000;

  if (host->state != PEER_CONNECTED)
    return 0; /* do not restart timer */

  if (timeout == 0)
    return DEFAULT_KEEPALIVE_TIMEOUT; /* to adopt possible timeout change */

  diff = mtlk_osal_timestamp_diff(mtlk_osal_timestamp(), host->timestamp);
  msecs = mtlk_osal_timestamp_to_ms(diff);

  if (msecs >= timeout) {
    mtlk_stadb_remove_host(stadb, host);
    ILOG3(GID_STADB, "WDS host %Y lost, idle time %u msecs (>%u)",
         host->mac, msecs, timeout);
    return 0; /* do not restart timer */
  } else {
    return (timeout - msecs); /* restart with adjusted timeout */
  }
}



// Private functions
static void
destroy_rod_queue (sta_entry *sta, unsigned int prio_id)
{
  ASSERT(sta != NULL);
  if (sta == NULL)
    return;

  ASSERT(prio_id < NTS_TIDS);
  if (prio_id >= NTS_TIDS)
    return;

  ILOG3(GID_STADB, "Destroying TS for STA %Y priority %d",
      sta->mac, prio_id);

  if (mtlk_is_used_rod_queue(&sta->rod_queue[prio_id])) {
      mtlk_clear_rod_queue(&sta->rod_queue[prio_id]);
  } else {
      WLOG("TS not used.");
  }
}



static void
create_rod_queue (sta_entry *sta, int prio_id, int win_size, int ssn)
{
  ASSERT(sta != NULL);
  if (sta == NULL)
      return;

  ASSERT(prio_id < NTS_TIDS);
  if (prio_id >= NTS_TIDS)
      return;

  ILOG3(GID_STADB, "Creating TS for STA %Y priority %d: window = %d SSN = %d",
      sta->mac, prio_id, win_size, ssn);

  mtlk_create_rod_queue(&sta->rod_queue[prio_id], win_size, ssn);
}


static void
_sta_reset_tx_packets (sta_entry *sta)
{
  uint16 tid;
  for (tid = 0; tid < ARRAY_SIZE(sta->tx_packets); tid++) {
    mtlk_osal_atomic_set(&sta->tx_packets[tid], 0);
  }
}


static __INLINE void
stadb_init_sta (sta_db *stadb, sta_entry *sta, unsigned char *mac,
                uint8 dot11n_mode)
{
  int pri;

  for (pri = 0; pri < ARRAY_SIZE(sta->tx_packets); pri++)
  {
      mtlk_init_rod_queue(&sta->rod_queue[pri]);
  }

  _sta_reset_tx_packets(sta);

  for (pri = 0; pri < ARRAY_SIZE(sta->tx_packets); pri++)
  {
    sta->aggr_inited[pri] = FALSE;
  }

  mtlk_osal_copy_eth_addresses(sta->mac, mac);  
  sta->timestamp        = mtlk_osal_timestamp();
  sta->dot11n_mode      = dot11n_mode;
  sta->state            = PEER_CONNECTED;
  sta->beacon_interval  = DEFAULT_BEACON_INTERVAL;
  sta->type             = PEER_STA;
  sta->cipher           = 0;
  sta->net_mode         = 0;
  sta->tx_rate          = 0;
  sta->beacon_timestamp = 0;
  sta->peer_ap          = 0;
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  sta->rf_mgmt_data     = MTLK_RF_MGMT_DATA_DEFAULT;
#endif
  mtlk_osal_atomic_set(&sta->filter, MTLK_PCKT_FLTR_ALLOW_ALL);
  memset(sta->rssi, 0, sizeof(sta->rssi));
  memset(&sta->stats, 0, sizeof(sta->stats));
  memset(&sta->next_peer, 0, sizeof(sta->next_peer));

  mtlk_sq_peer_ctx_init(stadb->sq, &sta->sq_peer_ctx, MTLK_SQ_TX_LIMIT_DEFAULT);

  // Start timers
  mtlk_osal_timer_set(&sta->keepalive_timer,
          stadb->keepalive_interval ? 
          stadb->keepalive_interval : 
          DEFAULT_KEEPALIVE_TIMEOUT);
  mtlk_osal_timer_set(&sta->idle_timer,
          stadb->sta_keepalive_timeout ? 
          stadb->sta_keepalive_timeout : 
          DEFAULT_KEEPALIVE_TIMEOUT);
}



static __INLINE void
stadb_cleanup_sta (sta_db *stadb, sta_entry *sta)
{
  int prio_id, host_id, id = _mtlk_stadb_get_sta_id_by_sta(stadb, sta);
  host_entry *host;

  // Stop timers
  mtlk_osal_timer_cancel_sync(&sta->keepalive_timer);
  mtlk_osal_timer_cancel_sync(&sta->idle_timer);

  for (prio_id = 0; prio_id < NTS_TIDS; prio_id++) {
    if (mtlk_is_used_rod_queue(&sta->rod_queue[prio_id])) {
      destroy_rod_queue(sta, prio_id);
    }
    mtlk_release_rod_queue(&sta->rod_queue[prio_id]);
  }

  // Remove all STA's HOSTs
  for (host_id = 0; host_id < STA_MAX_HOSTS; host_id++) {
    host = mtlk_stadb_get_host_by_id(stadb, host_id);
    if ((host->state != PEER_UNUSED) && (host->sta_id == id))
      mtlk_stadb_remove_host(stadb, host);
  }

  sta->state = PEER_UNUSED;
}



static __INLINE void
stadb_init_host (sta_db *stadb, host_entry *host,
                 const unsigned char *mac, int sta_id)
{
  memcpy(host->mac, mac, 6);
  host->timestamp = mtlk_osal_timestamp();
  host->sta_id    = sta_id;
  host->state     = PEER_CONNECTED;
  host->type = PEER_HOST;
  memset(&host->stats, 0, sizeof(host->stats));
  memset(&host->next_peer, 0, sizeof(host->next_peer));

  // Start timers
  mtlk_osal_timer_set(&host->idle_timer,
          (stadb->wds_host_timeout * 1000) ? : DEFAULT_KEEPALIVE_TIMEOUT);
}



static __INLINE void
stadb_cleanup_host (host_entry *host)
{
  mtlk_osal_timer_cancel_sync(&host->idle_timer);
  host->state = PEER_UNUSED;
}



// Public functions
int __MTLK_IFUNC
mtlk_stadb_add_peer_ap (sta_db *stadb, unsigned char *mac,
                    uint8 dot11n_mode)
{
  sta_entry *sta;
  int sid = mtlk_stadb_add_sta(stadb, mac, dot11n_mode);
  if (sid < 0)
    return sid;
  sta = mtlk_stadb_get_sta_by_id(stadb, sid);
  sta->peer_ap = 1;

  return sid;
}


int __MTLK_IFUNC
mtlk_stadb_add_sta (sta_db *stadb, unsigned char *mac,
                    uint8 dot11n_mode)
{
  int id;
  sta_entry *sta = NULL;
  mtlk_slist_t *peer_bucket = &stadb->peers[PEER_HASH(mac)];
  mtlk_slist_entry_t *peer;

  MTLK_ASSERT(stadb->aggr_open_threshold != 0);

  // Check if already connected
  for (peer = mtlk_slist_begin(peer_bucket);
       peer != NULL;
       peer = mtlk_slist_next(peer))
  {
    sta = MTLK_LIST_GET_CONTAINING_RECORD(peer, sta_entry, next_peer);
    if (mtlk_osal_compare_eth_addresses(sta->mac, mac) == 0) {
      WLOG("%Y already connected", mac);
      return -1;
    }
  }

  // Try to find UNUSED STA peer slot
  for (id = 0; id < STA_MAX_STATIONS; id++) {
    sta = mtlk_stadb_get_sta_by_id(stadb, id);
    if (sta->state == PEER_UNUSED)
      goto ADD_STA;
  }

  WLOG("Too many STAs connected");
  return -1;

ADD_STA:

  // Initialize STA
  stadb_init_sta(stadb, sta, mac, dot11n_mode);

  // Add STA to hash bucket
  mtlk_slist_push(peer_bucket, &sta->next_peer);

  ILOG3(GID_STADB, "PEER %Y added, id %d(%p)", mac, id, sta);
  return id;
}



int __MTLK_IFUNC
mtlk_stadb_update_sta (sta_entry *sta)
{
  ASSERT(sta != NULL);
  sta->timestamp = mtlk_osal_timestamp();
  ILOG3(GID_STADB, "Update STA %Y - %lu", sta->mac, sta->timestamp);
  return 0;
}

void __MTLK_IFUNC
mtlk_stadb_update_sta_tx (sta_db *stadb, sta_entry *sta, uint16 tid)
{
  ASSERT(stadb != NULL);
  ASSERT(sta != NULL);
  ASSERT(tid < ARRAY_SIZE(sta->tx_packets));

  if (sta->dot11n_mode && 
      sta->aggr_inited[tid] == FALSE &&
      mtlk_stadb_sta_get_filter(sta) == MTLK_PCKT_FLTR_ALLOW_ALL) {
    uint32 nof_tx_packets = mtlk_osal_atomic_inc(&sta->tx_packets[tid]);
    if (nof_tx_packets == stadb->aggr_open_threshold) {
      ILOG1(GID_STADB, "STA:%Y TID=%d TX Cntr=%d: Initiating aggregation agreement", 
            sta->mac, tid, nof_tx_packets);
      mtlk_osal_atomic_set(&sta->tx_packets[tid], 0);
      sta->aggr_inited[tid] = TRUE;
      mtlk_stadb_addba_revive_aggregation_tid(HANDLE_T(stadb),
                                              _mtlk_stadb_get_sta_id_by_sta(stadb, sta),
                                              tid);
    }
  }
}

int __MTLK_IFUNC
mtlk_stadb_update_ap (sta_entry *sta, uint16 beacon_interval)
{
  int lost_beacons;

  if (beacon_interval) {
    sta->beacon_interval = beacon_interval;
  }

  /* if sta->beacon_timestamp is zero - we receive first beacon from AP */
  if (sta->beacon_timestamp) {
    mtlk_osal_timestamp_diff_t diff;
    diff = mtlk_osal_timestamp_diff(mtlk_osal_timestamp(), sta->beacon_timestamp);
    lost_beacons = mtlk_osal_timestamp_to_ms(diff) / sta->beacon_interval;
  } else {
    lost_beacons = 0;
  }

  sta->beacon_timestamp = mtlk_osal_timestamp();

  ILOG3(GID_STADB, "Update AP %Y - %lu (beacon interval %d)",
    sta->mac, sta->timestamp, beacon_interval);

  return lost_beacons;
}



int __MTLK_IFUNC
mtlk_stadb_update_host (sta_db* stadb, const unsigned char *mac, int sta_id)
{
  host_entry *host = NULL;
  mtlk_slist_t *peer_bucket = &stadb->peers[PEER_HASH(mac)];
  mtlk_slist_entry_t *peer;

  // Try to find peer
  for (peer = mtlk_slist_begin(peer_bucket);
       peer != NULL;
       peer = mtlk_slist_next(peer))
  {
    host = MTLK_LIST_GET_CONTAINING_RECORD(peer, host_entry, next_peer);
    if (mtlk_osal_compare_eth_addresses(host->mac, mac) == 0) 
      break;
  }

  if (peer == NULL) {
    ILOG3(GID_STADB, "Can't find peer (HOST), adding...");
    return mtlk_stadb_add_host(stadb, mac, sta_id);
  }

  host->timestamp = mtlk_osal_timestamp();
  host->sta_id = sta_id;
  ILOG3(GID_STADB, "Update HOST %Y - %lu", host->mac, host->timestamp);

  return 0;
}



int __MTLK_IFUNC
mtlk_stadb_add_host (sta_db* stadb, const unsigned char *mac,
                     int sta_id)
{
  int id, oldest_id = 0;
  uint32 diff, biggest_diff = 0;
  host_entry *host = NULL;
  mtlk_slist_t *peer_bucket = &stadb->peers[PEER_HASH(mac)];
  mtlk_slist_entry_t *peer;

  // find place for HOST pointer
  for (peer = mtlk_slist_begin(peer_bucket);
       peer != NULL;
       peer = mtlk_slist_next(peer))
  {
    host = MTLK_LIST_GET_CONTAINING_RECORD(peer, host_entry, next_peer);
    if (mtlk_osal_compare_eth_addresses(host->mac, mac) == 0) {
      WLOG("Already registered");
      return -1;
    }
  }

  // find free HOST slot
  for (id = 0; id < STA_MAX_HOSTS; id++) {
    host = mtlk_stadb_get_host_by_id(stadb, id);
    diff = mtlk_osal_timestamp_diff(mtlk_osal_timestamp(), host->timestamp); 
    if (host->state == PEER_UNUSED)
      goto ADD_HOST;
    if (diff > biggest_diff) {
      biggest_diff = diff;
      oldest_id = id;
    }
  }

  if (stadb->wds_host_timeout == 0) {
    ILOG3(GID_STADB, "Replacing LRU id %u, diff %u", oldest_id, biggest_diff);
    host = mtlk_stadb_get_host_by_id(stadb, oldest_id);
    mtlk_stadb_remove_host(stadb, host);
    id = oldest_id;
  } else {
    WLOG("Too many hosts registered");
    return -1;
  }

ADD_HOST:

  // Initialize HOST
  stadb_init_host(stadb, host, mac, sta_id);

  // Add HOST to hash bucket
  mtlk_slist_push(peer_bucket, &host->next_peer);

  if ((!mtlk_osal_is_valid_ether_addr(stadb->default_host)) ||
      (mtlk_osal_compare_eth_addresses(stadb->default_host, stadb->local_mac) == 0)) {
    mtlk_osal_copy_eth_addresses(stadb->default_host, mac);
    ILOG3(GID_STADB, "default_host %Y", stadb->default_host);
  }

  ILOG3(GID_STADB, "HOST %Y added, id %d(%p)", mac, id, host);
  return id;
}



int __MTLK_IFUNC
mtlk_stadb_remove_host (sta_db *stadb, host_entry *host)
{
  mtlk_slist_t *peer_bucket = &stadb->peers[PEER_HASH(host->mac)];
  mtlk_slist_entry_t *peer;

  // Remove HOST from hash chain of peers
  for (peer = mtlk_slist_head(peer_bucket);
       peer != NULL;
       peer = mtlk_slist_next(peer))
  {
    mtlk_slist_entry_t *next = mtlk_slist_next(peer);
    if (host == MTLK_LIST_GET_CONTAINING_RECORD(next, host_entry, next_peer)) {
      mtlk_slist_remove_next(peer_bucket, peer);
      break;
    }
  }

  /* are we deleting default host? */
  if (mtlk_osal_compare_eth_addresses(stadb->default_host, host->mac) == 0) {
    int id, newest_id = -1;
    uint32 diff, smallest_diff = (uint32)-1;
    host_entry *newest_host = NULL;

    for (id = 0; id < STA_MAX_HOSTS; id++) {
      newest_host = mtlk_stadb_get_host_by_id(stadb, id);
      if (newest_host->state == PEER_UNUSED)
        continue;
      diff = mtlk_osal_timestamp_diff(mtlk_osal_timestamp(), newest_host->timestamp);
      if (diff < smallest_diff) {
        smallest_diff = diff;
        newest_id = id;
      }
    }
    if (newest_id < 0) {
      memset(stadb->default_host, 0, sizeof(stadb->default_host));
    } else {
      newest_host = mtlk_stadb_get_host_by_id(stadb, newest_id);
      mtlk_osal_copy_eth_addresses(stadb->default_host, newest_host->mac);
    }
    ILOG3(GID_STADB, "default_host %Y", stadb->default_host);
  }

  // Release HOST entry
  stadb_cleanup_host(host);

  ILOG3(GID_STADB, "HOST %Y removed", host->mac);
  return 0;
}



int __MTLK_IFUNC
mtlk_stadb_remove_sta (sta_db* stadb, sta_entry *sta)
{
  mtlk_slist_t *peer_bucket = &stadb->peers[PEER_HASH(sta->mac)];
  mtlk_slist_entry_t *peer;

  // Remove from SendQueue
  mtlk_sq_peer_ctx_cleanup(stadb->sq, &sta->sq_peer_ctx);

  // Remove STA from hash bucket of peers
  for (peer = mtlk_slist_head(peer_bucket);
       peer != NULL;
       peer = mtlk_slist_next(peer))
  {
    mtlk_slist_entry_t *next = mtlk_slist_next(peer);
    if (sta == MTLK_LIST_GET_CONTAINING_RECORD(next, sta_entry, next_peer)) {
      mtlk_slist_remove_next(peer_bucket, peer);
      break;
    }
  }
 
  // Release STA data
  stadb_cleanup_sta(stadb, sta);

  ILOG3(GID_STADB, "PEER %Y removed", sta->mac);
  return 0;
}



int __MTLK_IFUNC
mtlk_stadb_find_sta (sta_db* stadb, const unsigned char *mac)
{
  int id = -1;
  mtlk_slist_t *peer_bucket = &stadb->peers[PEER_HASH(mac)];
  mtlk_slist_entry_t *peer;
  peer_entry *virtual_peer = NULL;

  // find peer
  for (peer = mtlk_slist_begin(peer_bucket);
       peer != NULL;
       peer = mtlk_slist_next(peer))
  {
    virtual_peer = MTLK_LIST_GET_CONTAINING_RECORD(peer, peer_entry, next_peer);
    if (mtlk_osal_compare_eth_addresses(virtual_peer->mac, mac) == 0) 
      break;
  }

  if (peer == NULL) {
    ILOG3(GID_STADB, "Can't find peer (STA)");
    return -1;
  }

  // return STA id
  if (virtual_peer->type == PEER_HOST) {
    host_entry *host = (host_entry *)virtual_peer;
    id = host->sta_id;
    ILOG3(GID_STADB, "Found host %Y, STA id %d(%p)",
        host->mac, id, host);
  } else if (virtual_peer->type == PEER_STA) {
    sta_entry *sta = (sta_entry *)virtual_peer;
    id = _mtlk_stadb_get_sta_id_by_sta(stadb, sta);
    ILOG3(GID_STADB, "Found station %Y, STA id %d(%p)",
        sta->mac, id, sta);
  }

  return id;
}



void __MTLK_IFUNC
mtlk_stadb_flush_sta (sta_db* stadb)
{
  int sta_id, prio_id;
  sta_entry *sta;

  for (sta_id = 0; sta_id < STA_MAX_STATIONS; sta_id++) {
    sta = mtlk_stadb_get_sta_by_id(stadb, sta_id);
    if (sta->state != PEER_UNUSED) {
      for (prio_id = 0; prio_id < NTS_TIDS; prio_id++)
        mtlk_handle_rod_queue_timer(&sta->rod_queue[prio_id]);
    }
  }
}



void __MTLK_IFUNC
mtlk_stadb_init (sta_db *stadb, struct nic *nic, mtlk_sq_t *sq)
{
  int id, i;

  stadb->sq = sq;

  // Register parent
  stadb->nic = nic;

  // Set default configuration
  stadb->sta_keepalive_timeout = 60000;
  stadb->keepalive_interval = 30000;
  stadb->wds_host_timeout = 0;
  stadb->aggr_open_threshold = DEFAULT_AGGR_OPEN_THRESHOLD;

  // Create hash buckets
  for (i = 0; i < STA_MAX_PEERS; i++) {
    mtlk_slist_init(&stadb->peers[i]);
  }

  // Clear STA peers array
  for (id = 0; id < STA_MAX_STATIONS; id++) {
    sta_entry *sta = mtlk_stadb_get_sta_by_id(stadb, id);

    memset(sta, 0, sizeof(sta_entry));

    mtlk_osal_lock_init(&sta->lock);
    mtlk_osal_timer_init(&sta->keepalive_timer,
                         keepalive_tmr,
                         HANDLE_T(stadb));
    mtlk_osal_timer_init(&sta->idle_timer,
                         data_disconnect_tmr,
                         HANDLE_T(stadb));
    for (i = 0; i < ARRAY_SIZE(sta->async_man_msgs); i++) {
      mtlk_txmm_msg_init(&sta->async_man_msgs[i]);
    }
    mtlk_addba_peer_init(&sta->addba_peer);
    sta->state = PEER_UNUSED;
  }

  // Clear HOST peers array
  for (id = 0; id < STA_MAX_HOSTS; id++) {
    host_entry *host = mtlk_stadb_get_host_by_id(stadb, id);

    memset(host, 0, sizeof(host_entry));

    mtlk_osal_lock_init(&host->lock);
    mtlk_osal_timer_init(&host->idle_timer,
                         wds_disconnect_tmr, 
                         HANDLE_T(stadb));

    host->state = PEER_UNUSED;
  }
}



void __MTLK_IFUNC
mtlk_stadb_clear (sta_db *stadb)
{
  int i, j;

  for (i = 0; i < STA_MAX_STATIONS; i++) {
    sta_entry *sta = mtlk_stadb_get_sta_by_id(stadb, i);
    if (sta->state != PEER_UNUSED)
      mtlk_stadb_remove_sta(stadb, sta);

    mtlk_addba_peer_cleanup(&sta->addba_peer);
    for (j = 0; j < ARRAY_SIZE(sta->async_man_msgs); j++) {
      mtlk_txmm_msg_cleanup(&sta->async_man_msgs[j]);
    }
    mtlk_osal_timer_cleanup(&sta->keepalive_timer);
    mtlk_osal_timer_cleanup(&sta->idle_timer);
    mtlk_osal_lock_cleanup(&sta->lock);
  }

  for (i = 0; i < STA_MAX_HOSTS; i++) {
    host_entry *host = mtlk_stadb_get_host_by_id(stadb, i);

    mtlk_osal_timer_cleanup(&host->idle_timer);
    mtlk_osal_lock_cleanup(&host->lock);
  }

  // Delete hash buckets
  for (i = 0; i < STA_MAX_PEERS; i++) {
    mtlk_slist_cleanup(&stadb->peers[i]);
  }
}

void __MTLK_IFUNC
mtlk_stadb_sta_set_filter(sta_entry *sta, mtlk_pckt_filter_e filter)
{
  mtlk_pckt_filter_e prev = 
    (mtlk_pckt_filter_e)mtlk_osal_atomic_xchg(&sta->filter, (uint32)filter);
  if (prev != filter) {
    ILOG1(GID_STADB, "STA (%Y) filter: %d => %d", sta->mac, prev, filter);
    _sta_reset_tx_packets(sta);
  }
}

/*****************************************************************************
 * ADDBA module wrapper
******************************************************************************/
mtlk_addba_peer_t* __MTLK_IFUNC mtlk_stadb_get_addba_peer(mtlk_handle_t usr_data, const IEEE_ADDR* addr, mtlk_handle_t* container_handle)
{
  mtlk_addba_peer_t* peer       = NULL;
  sta_db*            stadb     = (sta_db*)usr_data;
  int                sta_id    = mtlk_stadb_find_sta(stadb, addr->au8Addr);

  if (sta_id != -1) {
    peer = &mtlk_stadb_get_sta_by_id(stadb, sta_id)->addba_peer;
    if (container_handle)
      *container_handle = HANDLE_T(sta_id);
  }

  return peer;
}

uint32 __MTLK_IFUNC mtlk_stadb_get_addba_last_rx_timestamp(mtlk_handle_t usr_data, mtlk_handle_t container_handle, uint16 tid)
{
  sta_db*    stadb     = (sta_db*)usr_data;
  sta_entry* sta       = mtlk_stadb_get_sta_by_id(stadb, (int)container_handle);

  if (tid >= NTS_TIDS)
    ELOG("Invalid TID=%d", (int)tid);

  return mtlk_rod_queue_get_last_rx_time(&sta->rod_queue[tid]);
}

void __MTLK_IFUNC mtlk_stadb_addba_do_nothing(mtlk_handle_t usr_data,
                                              mtlk_handle_t container_handle, /* from the get_peer() - to avoid double searching by MAC */
                                              uint16        prior_class)
{
  sta_db*    stadb     = (sta_db*)usr_data;
  sta_entry* sta       = mtlk_stadb_get_sta_by_id(stadb, (int)container_handle);

  MTLK_UNREFERENCED_PARAM(prior_class);
  MTLK_UNREFERENCED_PARAM(sta);
}

void __MTLK_IFUNC mtlk_stadb_addba_revive_aggregation_tid(mtlk_handle_t usr_data,
                                                          mtlk_handle_t container_handle, /* from the get_peer() - to avoid double searching by MAC */
                                                          uint16        prior_class)
{
  int                     res;
  struct eq_evt_aggr_data data;

  memset(&data, 0, sizeof(data));

  data.stadb       = (sta_db*)usr_data;
  data.sta         = mtlk_stadb_get_sta_by_id(data.stadb, (int)container_handle);
  data.sta_id      = (int)container_handle;
  data.prior_class = prior_class;

  res = mtlk_eq_notify(&data.stadb->nic->slow_ctx->eq, EVT_AGGR_REVIVE_TID,
                       &data, sizeof(data));
  if (res != MTLK_ERR_OK) {
    WLOG("Can't notify EQ of Aggr closing (err=%d)", res);
  }
}

void __MTLK_IFUNC mtlk_stadb_addba_restart_tx_count_tid(mtlk_handle_t usr_data,
                                                        mtlk_handle_t container_handle, /* from the get_peer() - to avoid double searching by MAC */
                                                        uint16        prior_class)
{
  sta_entry *sta = mtlk_stadb_get_sta_by_id((sta_db*)usr_data, (int)container_handle);

  MTLK_ASSERT(sta->aggr_inited[prior_class] == TRUE);

  ILOG0(GID_STADB, "STA:%Y TID=%d: Re-starting TX counter", sta->mac, prior_class);

  mtlk_osal_atomic_set(&sta->tx_packets[prior_class], 0);

  sta->aggr_inited[prior_class] = FALSE;
}

void __MTLK_IFUNC mtlk_stadb_addba_start_reordering(mtlk_handle_t usr_data,
                                                    mtlk_handle_t container_handle, /* from the get_peer() - to avoid double searching by MAC */
                                                    uint16        prior_class,
                                                    uint16        ssn,
                                                    uint8         win_size)
{
  sta_db*    stadb     = (sta_db*)usr_data;
  sta_entry* sta       = mtlk_stadb_get_sta_by_id(stadb, (int)container_handle);

  ILOG2(GID_STADB, "Starting reordering");
  create_rod_queue(sta, prior_class, win_size, ssn);
}

void __MTLK_IFUNC mtlk_stadb_addba_stop_reordering(mtlk_handle_t usr_data,
                                                   mtlk_handle_t container_handle, /* from the get_peer() - to avoid double searching by MAC */
                                                   uint16        prior_class)
{
  sta_db*    stadb     = (sta_db*)usr_data;
  sta_entry* sta       = mtlk_stadb_get_sta_by_id(stadb, (int)container_handle);

  ILOG2(GID_STADB, "Finishing reordering");
  destroy_rod_queue(sta, prior_class);
}

void __MTLK_IFUNC mtlk_stadb_addba_revive_aggregation_all (mtlk_handle_t usr_data,
                                                           mtlk_handle_t container_handle, /* from the get_peer() - to avoid double searching by MAC */
                                                           uint16        prior_class)
{
  int                     res;
  struct eq_evt_aggr_data data;

  memset(&data, 0, sizeof(data));

  data.stadb       = (sta_db*)usr_data;
  data.sta         = mtlk_stadb_get_sta_by_id(data.stadb, (int)container_handle);
  data.sta_id      = (int)container_handle;
  data.prior_class = prior_class;

  if (!data.sta->peer_ap) {
    return;
  }

  res = mtlk_eq_notify(&data.stadb->nic->slow_ctx->eq, EVT_AGGR_REVIVE_ALL,
                       &data, sizeof(data));
  if (res != MTLK_ERR_OK) {
    WLOG("Can't notify EQ of Aggr REQ receiving (err=%d)", res);
  }
}

void __MTLK_IFUNC mtlk_stadb_on_aggregation_revive_tid (const void *notification_payload)
{
  const struct eq_evt_aggr_data *data = 
    (const struct eq_evt_aggr_data *)notification_payload;

  ILOG2(GID_STADB, "Peer AP (%Y) BA REQ (tid=%d): re-sending TID BA request if needed...",
        data->sta->mac, data->prior_class);

  mtlk_addba_start_aggr_negotiation_ex(&data->stadb->nic->slow_ctx->addba,
                                        (IEEE_ADDR *)data->sta->mac,
                                        &data->sta->addba_peer,
                                        HANDLE_T(data->sta_id),
                                        data->prior_class);
}

void __MTLK_IFUNC mtlk_stadb_on_aggregation_revive_all (const void *notification_payload)
{
  const struct eq_evt_aggr_data *data = 
    (const struct eq_evt_aggr_data *)notification_payload;

  MTLK_ASSERT(data->sta->peer_ap);

  ILOG2(GID_STADB, "Peer AP (%Y) BA REQ (tid=%d): re-sending all BA requests needed...",
    data->sta->mac, data->prior_class);

  mtlk_addba_start_negotiation_ex(&data->stadb->nic->slow_ctx->addba,
                                  (IEEE_ADDR *)data->sta->mac,
                                  &data->sta->addba_peer,
                                  HANDLE_T(data->sta_id),
                                  mtlk_core_get_rate_for_addba(data->stadb->nic));
}

int __MTLK_IFUNC mtlk_stadb_set_local_mac (mtlk_handle_t usr_data,
                                           const unsigned char *mac)
{
  sta_db*    stadb     = (sta_db*)usr_data;
  int res = MTLK_ERR_OK;

  if (mtlk_osal_is_valid_ether_addr(mac))
    mtlk_osal_copy_eth_addresses(stadb->local_mac, mac);
  else
    res = MTLK_ERR_PARAMS;
  return res;
}

void __MTLK_IFUNC mtlk_stadb_get_local_mac (mtlk_handle_t usr_data,
                                            unsigned char *mac)
{
  sta_db*    stadb     = (sta_db*)usr_data;

  mtlk_osal_copy_eth_addresses(mac, stadb->local_mac);
}

void __MTLK_IFUNC
mtlk_stadb_set_pm_enabled(sta_entry *sta, BOOL enabled)
{
  mtlk_sq_set_pm_enabled(&sta->sq_peer_ctx, enabled);
}

