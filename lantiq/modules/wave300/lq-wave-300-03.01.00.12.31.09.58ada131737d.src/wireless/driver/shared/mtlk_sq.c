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
*/
#include "mtlkinc.h"

#include "mtlk_sq.h"
#include "mtlkhal.h"
#include "mtlkqos.h"
#include "mtlk_sq_osdep.h"
#include "mtlk_core_iface.h"


MTLK_INIT_STEPS_LIST_BEGIN(sq)
  MTLK_INIT_STEPS_LIST_ENTRY(sq, LOCK_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(sq, DLIST_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(sq, PEER_CTX_INIT)
MTLK_INIT_INNER_STEPS_BEGIN(sq)
MTLK_INIT_STEPS_LIST_END(sq);

int __MTLK_IFUNC
mtlk_sq_init(mtlk_sq_t      *pqueue,
               mtlk_core_t    *pcore)
{
  int i;

  MTLK_ASSERT( pqueue  != NULL );
  MTLK_ASSERT( pcore   != NULL );

  MTLK_INIT_TRY(sq, MTLK_OBJ_PTR(pqueue))
    MTLK_INIT_STEP(sq, LOCK_INIT, MTLK_OBJ_PTR(pqueue), 
                   mtlk_osal_lock_init, (&pqueue->queue_lock));
                   
    for(i = 0; i < NTS_PRIORITIES; i++)
    {
      mtlk_osal_atomic_set(&pqueue->peer_queue[i].size, 0);
      MTLK_INIT_STEP_VOID_LOOP(sq, DLIST_INIT, MTLK_OBJ_PTR(pqueue), 
                               mtlk_dlist_init, (&pqueue->peer_queue[i].list));
    }                 

    MTLK_INIT_STEP_VOID(sq, PEER_CTX_INIT, MTLK_OBJ_PTR(pqueue), 
                   mtlk_sq_peer_ctx_init, (pqueue, &pqueue->broadcast, MTLK_SQ_TX_LIMIT_DEFAULT));

    pqueue->pcore = pcore;
    pqueue->flush_in_progress = 0;

    mtlk_osal_atomic_set(&pqueue->flush_count, 0);

    memset(&pqueue->stats, 0, sizeof(pqueue->stats));
    /* set unlimited queue lengths */
    memset(&pqueue->limits, 0, sizeof(pqueue->limits));
    
  MTLK_INIT_FINALLY(sq, MTLK_OBJ_PTR(pqueue))
  MTLK_INIT_RETURN(sq, MTLK_OBJ_PTR(pqueue), mtlk_sq_cleanup, (pqueue));    
}

void __MTLK_IFUNC
mtlk_sq_cleanup(mtlk_sq_t *pqueue)
{
  int i;

  MTLK_ASSERT( pqueue != NULL );

  MTLK_CLEANUP_BEGIN(sq, MTLK_OBJ_PTR(pqueue))
    MTLK_CLEANUP_STEP(sq, PEER_CTX_INIT, MTLK_OBJ_PTR(pqueue),
                      mtlk_sq_peer_ctx_cleanup, (pqueue, &pqueue->broadcast));
    for (i = 0; MTLK_CLEANUP_ITERATONS_LEFT(MTLK_OBJ_PTR(pqueue), DLIST_INIT) > 0; i++) {
      MTLK_CLEANUP_STEP_LOOP(sq, DLIST_INIT, MTLK_OBJ_PTR(pqueue),
                             mtlk_dlist_cleanup, (&pqueue->peer_queue[i].list));
    }

    MTLK_CLEANUP_STEP(sq, LOCK_INIT, MTLK_OBJ_PTR(pqueue),
                      mtlk_osal_lock_cleanup, (&pqueue->queue_lock));
  MTLK_CLEANUP_END(sq, MTLK_OBJ_PTR(pqueue));

}

void __MTLK_IFUNC
mtlk_sq_peer_ctx_init(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer, uint32 limit)
{
    int i;

    ASSERT( pqueue != NULL );
    ASSERT( ppeer != NULL );

    memset(ppeer, 0, sizeof(mtlk_sq_peer_ctx_t));
    mtlk_osal_atomic_set(&ppeer->limit, limit);
    mtlk_osal_atomic_set(&ppeer->used, 0);
    ppeer->limit_cfg = limit; 

    mtlk_osal_lock_acquire(&pqueue->queue_lock);
    for (i = 0; i < NTS_PRIORITIES; i++)
    {
        mtlk_buflist_init(&ppeer->peer_queue_entry[i].buflist);
        mtlk_dlist_push_front(&pqueue->peer_queue[i].list, &ppeer->peer_queue_entry[i].list_entry);
    }
    mtlk_osal_lock_release(&pqueue->queue_lock);
}

void __MTLK_IFUNC
mtlk_sq_peer_ctx_cleanup(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer)
{
    mtlk_completion_ctx_t completion_ctx;
    mtlk_buflist_entry_t *pbuflist_entry;
    mtlk_buffer_t *ppacket;
    int i;

    ASSERT( pqueue != NULL );
    ASSERT( ppeer != NULL );

    _mtlk_sq_create_completion_ctx(&completion_ctx);

    mtlk_osal_lock_acquire(&pqueue->queue_lock);

    for (i = 0; i < NTS_PRIORITIES; i++)
    {
        mtlk_dlist_remove(&pqueue->peer_queue[i].list, &ppeer->peer_queue_entry[i].list_entry);

        while (!mtlk_buflist_is_empty(&ppeer->peer_queue_entry[i].buflist))
        {
            pbuflist_entry = mtlk_buflist_pop_front(&ppeer->peer_queue_entry[i].buflist);
            ppacket = mtlk_bufmgr_get_by_buflist_entry(pbuflist_entry);
            _mtlk_sq_release_packet(pqueue, i, ppacket);
            _mtlk_sq_cancel_packet(ppacket);
        }

        mtlk_buflist_cleanup(&ppeer->peer_queue_entry[i].buflist);
    }

    mtlk_osal_lock_release(&pqueue->queue_lock);

    _mtlk_sq_release_completion_ctx(pqueue, &completion_ctx);
}

void __MTLK_IFUNC
mtlk_sq_peer_ctx_cancel_all_packets(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer)
{
  mtlk_buflist_entry_t *pbuflist_entry;
  mtlk_buffer_t *ppacket;
  int i;

  MTLK_ASSERT(NULL != pqueue);
  MTLK_ASSERT(NULL != ppeer);

  mtlk_osal_lock_acquire(&pqueue->queue_lock);
  for (i = 0; i < NTS_PRIORITIES; i++) {
    while (!mtlk_buflist_is_empty(&ppeer->peer_queue_entry[i].buflist)) {
      pbuflist_entry = mtlk_buflist_pop_front(&ppeer->peer_queue_entry[i].buflist);
      ppacket = mtlk_bufmgr_get_by_buflist_entry(pbuflist_entry);
      _mtlk_sq_release_packet(pqueue, i, ppacket);
      _mtlk_sq_cancel_packet(ppacket);
    }
  }
  mtlk_osal_lock_release(&pqueue->queue_lock);
}

int __MTLK_IFUNC
mtlk_sq_enqueue_pkt(mtlk_sq_t          *pqueue,
                    mtlk_sq_peer_ctx_t *ppeer,
                    mtlk_buffer_t      *ppacket,
                    uint16             access_category,
                    BOOL               front)
{
    int res = MTLK_ERR_OK;

    ASSERT( pqueue != NULL );
    ASSERT( ppacket != NULL );
    ASSERT( access_category < NTS_PRIORITIES );

    // Use broadcast pseudo-STA if ppeer isn't specified
    if (ppeer == NULL)
    {
        ppeer = &pqueue->broadcast;
    }

    mtlk_osal_lock_acquire(&pqueue->queue_lock);

    // Queue size could exceed configured limit---
    // in case if packet is enqueue on the front of queue
    if (!front && 
        /* total nof packets limit reached or */
        (mtlk_osal_atomic_get(&pqueue->peer_queue[access_category].size) >= pqueue->limits.global_queue_limit[access_category] ||
        /* per-peer nof packets limit reached */
         mtlk_buflist_size(&ppeer->peer_queue_entry[access_category].buflist) >= pqueue->limits.peer_queue_limit[access_category])
        )
    {
        pqueue->stats.pkts_limit_dropped[access_category]++;
        ppeer->stats.pkts_limit_dropped[access_category]++;
        res = MTLK_ERR_NO_RESOURCES;
        goto out;
    }

    if (front)
        mtlk_buflist_push_front(&ppeer->peer_queue_entry[access_category].buflist,
            mtlk_bufmgr_get_buflist_entry(ppacket));
    else
        mtlk_buflist_push_back(&ppeer->peer_queue_entry[access_category].buflist,
            mtlk_bufmgr_get_buflist_entry(ppacket));

    pqueue->stats.pkts_pushed[access_category]++;
    ppeer->stats.pkts_pushed[access_category]++;

out:
    mtlk_osal_lock_release(&pqueue->queue_lock);
    return res;
}

/*! 
\fn      static int process_packets_by_ac(mtlk_sq_t *pqueue, uint16 access_category, mtlk_completion_ctx_t *pcompletion_ctx)
\brief   Internal function that sends as much packets of given AC as possible from send queue to HW.

\param   pqueue Pointer to send queue structure
\param   access_category Access category of packets
\param   pcompletion_ctx Completion context used to free sent packets

\return MTLK_ERR_OK if packet was sent successfully
\return MTLK_ERR_NO_RESOURCES if some packets were not sent due to lack of HW resources
*/

static int
process_packets_by_ac(mtlk_sq_t             *pqueue, 
                      uint16                 access_category, 
                      mtlk_completion_ctx_t *pcompletion_ctx)
{
    mtlk_dlist_entry_t *entry;
    mtlk_dlist_entry_t *head;

    mtlk_dlist_foreach(&pqueue->peer_queue[access_category].list, entry, head)
    {
        mtlk_peer_queue_entry_t *ppeer_queue_entry =
            MTLK_CONTAINER_OF(entry, mtlk_peer_queue_entry_t, list_entry);
        mtlk_sq_peer_ctx_t *ppeer =
            MTLK_CONTAINER_OF(ppeer_queue_entry, mtlk_sq_peer_ctx_t, peer_queue_entry[access_category]);

        while (!mtlk_buflist_is_empty(&ppeer_queue_entry->buflist))
        {
            mtlk_buffer_t *ppacket;
            mtlk_os_status_t os_status;

            if (mtlk_osal_atomic_get(&ppeer->used) >= mtlk_osal_atomic_get(&ppeer->limit)) {
                break;
            }

            ppacket = mtlk_bufmgr_get_by_buflist_entry(
                mtlk_buflist_pop_front(&ppeer_queue_entry->buflist));

            switch (_mtlk_sq_send_to_hw(pqueue, ppacket, mtlk_bufmgr_get_priority(ppacket), &os_status))
            {
            case MTLK_ERR_OK:
                _mtlk_sq_release_packet(pqueue, access_category, ppacket);
                mtlk_osal_atomic_inc(&ppeer->used);
                pqueue->stats.pkts_sent_to_um[access_category]++;
                ppeer->stats.pkts_sent_to_um[access_category]++;
                _mtlk_sq_complete_packet(pcompletion_ctx, ppacket, os_status);
                break;
            case MTLK_ERR_PROHIB:
                /* fall through */
            case MTLK_ERR_NO_RESOURCES:
                mtlk_buflist_push_front(&ppeer_queue_entry->buflist, mtlk_bufmgr_get_buflist_entry(ppacket));
                return MTLK_ERR_PROHIB;
            case MTLK_ERR_PKT_DROPPED:
                _mtlk_sq_release_packet(pqueue, access_category, ppacket);
                _mtlk_sq_complete_packet(pcompletion_ctx, ppacket, os_status);
                break;
            default:
                WLOG("Unknown failure upon HW TX request");
                _mtlk_sq_release_packet(pqueue, access_category, ppacket);
                _mtlk_sq_complete_packet(pcompletion_ctx, ppacket, os_status);
                return MTLK_ERR_UNKNOWN;
            }
        }
    }

    return MTLK_ERR_OK;
}

// Following is a rationale and algorithm description of mtlk_sq_flush function. 
// So long as queue processing may be initiated by two different threads 
// in parallel - thread used to send data packets and thread used to process 
// MAC confirmations - all data structures of '''QoS Traffic Shaper''' must 
// be protected by synchronization primitives.
// 
// This leads to serialization of above threads when they call '''QoS Traffic Shaper''' 
// and performance degradation, basically because of concurrent invocations of queues 
// processing logic that takes majority of algorithm time. Moreover, 
// when MAC confirmation thread is blocked on some synchronization primitive adapter 
// is unable to return packets and this leads to further serialization of driver and MAC threads.
// 
// In order to avoid this scenario following locking strategy was implemented: 	 	 
// 
// 1. Queues processing logic operates with two additional variables: 	 	 
// 	* Flag to indicate that currently queues being flushed in concurrent thread (''InProgress flag''), 	 	 
// 	* Counter of queues flush requests (''Requests counter''); 	 	 
// 2. Before acquiring '''QoS Traffic Shaper''' lock, queues processing logic checks value of ''InProgress flag''.
// 3. If flag is raised, processing logic increments ''Requests counter'' and quits. 	 	 
// 4. If flag not raised, queues processing logic acquires lock, raises ''InProgress flag'', remembers value of 
//    ''Requests counter'' and starts processing of queues;
// 5. After queues processing finished, queues processing logic lowers ''InProgress flag'' to avoid 
//    ''signal loss'' effect, checks whether ''Requests counter'' value equals to remembered one. 
//    If not equals - code raises ''InProgress flag'' and repeats queues processing, else releases lock and quits;

#define PACKETS_IN_MAC_DURING_PM 3

void __MTLK_IFUNC
mtlk_sq_set_pm_enabled(mtlk_sq_peer_ctx_t *ppeer, BOOL enabled)
{
  uint32 val = enabled?
                 MIN(PACKETS_IN_MAC_DURING_PM, ppeer->limit_cfg) : 
                 ppeer->limit_cfg;

  mtlk_osal_atomic_set(&ppeer->limit, val);
}

void __MTLK_IFUNC
mtlk_sq_flush(mtlk_sq_t *pqueue)
{
    mtlk_completion_ctx_t completion_ctx;
    uint32 initial_flush_count;

    ASSERT( pqueue != NULL );

    if(pqueue->flush_in_progress)
    {
        mtlk_osal_atomic_inc(&pqueue->flush_count);
        return;
    }

    pqueue->flush_in_progress = 1;

    _mtlk_sq_create_completion_ctx(&completion_ctx);

    mtlk_osal_lock_acquire(&pqueue->queue_lock);

    do 
    {
        uint16 i;
        initial_flush_count = mtlk_osal_atomic_get(&pqueue->flush_count);
        pqueue->flush_in_progress = 1;

        //Send packets to MAC, higher priorities go first
        for (i = NTS_PRIORITIES; i > 0; i--)
        {
            if(MTLK_ERR_OK != process_packets_by_ac(pqueue,
                                                    mtlk_get_ac_by_number(i - 1),
                                                    &completion_ctx))
                break;
        }

        pqueue->flush_in_progress = 0;
    } 
    while(initial_flush_count != mtlk_osal_atomic_get(&pqueue->flush_count));

    mtlk_osal_lock_release(&pqueue->queue_lock);
    _mtlk_sq_release_completion_ctx(pqueue, &completion_ctx);
}

void __MTLK_IFUNC
mtlk_sq_get_stats(mtlk_sq_t *pqueue, mtlk_sq_stats_t *pstats)
{
    ASSERT( pqueue != NULL );
    ASSERT( pstats != NULL );

    mtlk_osal_lock_acquire(&pqueue->queue_lock);
    memcpy(pstats, &pqueue->stats, sizeof(*pstats));
    mtlk_osal_lock_release(&pqueue->queue_lock);
}
