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

#ifndef _MTLK_SENDQUEUE_H_
#define _MTLK_SENDQUEUE_H_

/** 
*\file mtlk_sq.h
*\brief QoS traffic shaper
*/

#include "mtlklist.h"
#include "bufmgr.h"
#include "mtlkqos.h"

#define SQ_PUT_FRONT  TRUE
#define SQ_PUT_BACK   FALSE

/* queue limits */
typedef struct _mtlk_sq_limits
{
    uint16 global_queue_limit[NTS_PRIORITIES];
    uint16 peer_queue_limit[NTS_PRIORITIES];
} mtlk_sq_limits_t;

/* queue current sizes */
typedef struct _mtlk_sq_qsizes
{
    uint32  qsize[NTS_PRIORITIES];
} mtlk_sq_qsizes_t;

/* send queue statistics */
typedef struct _mtlk_sq_stats
{
    uint32  pkts_pushed[NTS_PRIORITIES];
    uint32  pkts_sent_to_um[NTS_PRIORITIES];
    uint32  pkts_limit_dropped[NTS_PRIORITIES];
} mtlk_sq_stats_t;

typedef struct _mtlk_peer_queue_entry_t {
    mtlk_buflist_t buflist;
    mtlk_dlist_entry_t list_entry; // for mtlk_sq_t.peer_queue membership
} mtlk_peer_queue_entry_t;

typedef struct _mtlk_peer_queue_t {
    mtlk_dlist_t  list; // list of mtlk_peer_queue_entry_t
    mtlk_atomic_t size;       // total size = size(mtlk_peer_queue_entry_t[...].buflist)
} mtlk_peer_queue_t;

typedef struct _mtlk_sq_peer_ctx_t {
    mtlk_peer_queue_entry_t peer_queue_entry[NTS_PRIORITIES];
#define MTLK_SQ_TX_LIMIT_INFINITE -1
#define MTLK_SQ_TX_LIMIT_DEFAULT  64
    uint32          limit_cfg;
    mtlk_atomic_t   limit; // maximum allowed MSDU amount which STA can occupy
    mtlk_atomic_t   used;
    mtlk_sq_stats_t stats;
} mtlk_sq_peer_ctx_t;

/* Opaque structure for send queue. Should not be used directly */
typedef struct _mtlk_sq_t
{
    mtlk_peer_queue_t    peer_queue[NTS_PRIORITIES];              //Main TX queue

    mtlk_sq_peer_ctx_t   broadcast;                               //pseudo-STA for broadcast
                                                                  //and non-reliable multicast
    mtlk_osal_spinlock_t queue_lock;                              //Lock for this object (protects queues)

    volatile uint8       flush_in_progress;                       //Flag indicating whether flush
                                                                  //operation currently in progress
    mtlk_atomic_t        flush_count;                             //Counter of flush requests

    mtlk_core_t*         pcore;                                   //Pointer to the CORE context

    mtlk_sq_stats_t      stats;                                   //send queue statistics
                                                                  //protected by queue_lock
    mtlk_sq_limits_t     limits;                                  //queue limits, 0 - unlimited
                                                                  //protected by queue_lock
    MTLK_DECLARE_INIT_STATUS;
    MTLK_DECLARE_INIT_LOOP(DLIST_INIT);                                                                  
} mtlk_sq_t;

/*! 
\fn      int __MTLK_IFUNC mtlk_sq_init(mtlk_sq_t *pqueue, mtlk_core_t *pcore)
\brief   Initializes send queue. 

\param   pqueue Pointer to user-allocated memory for send queue structure
\param   mtlk_core_t pointer to CORE context
*/

int __MTLK_IFUNC
mtlk_sq_init(mtlk_sq_t      *pqueue, 
               mtlk_core_t    *pcore);

/*! 
\fn      void __MTLK_IFUNC mtlk_sq_cleanup(mtlk_sq_t *pqueue)
\brief   Frees resources used by send queue. 

\param   pqueue Pointer to send queue structure
*/

void __MTLK_IFUNC
mtlk_sq_cleanup(mtlk_sq_t *pqueue);

/*! 
\fn      int __MTLK_IFUNC mtlk_sq_enqueue_pkt(mtlk_sq_t *pqueue, mtlk_buffer_t *ppacket, uint16 *paccess_category, BOOL front)
\brief   Places packet to send queue. 

\param   pqueue           [IN] Pointer to send queue structure
\param   ppacket          [IN] Pointer to the packet to be enqueued
\param   paccess_category [IN] AC of the queue where packet was placed. If packet is urgent - ignored.
\param   front            [IN] Insert the packet into the front of the queue

\return  MTLK_ERR_UNKNOWN In case of error
\return  MTLK_ERR_OK When succeeded
*/

/*!
\fn      void __MTLK_IFUNC mtlk_sq_peer_ctx_init(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer, uint32 resource)
\brief   Init SendQueue peer ctx

\param   pqueue SendQueue ctx
\param   ppeer SendQueue peer ctx
\param   resource maximum allowed MSDU amount which peer can occupy
*/

void __MTLK_IFUNC
mtlk_sq_peer_ctx_init(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer, uint32 resource);

/*!
\fn      void __MTLK_IFUNC mtlk_sq_peer_ctx_cleanup(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer)
\brief   Cleanup SendQueue peer ctx

\param   pqueue SendQueue ctx
\param   ppeer SendQueue peer ctx
*/

void __MTLK_IFUNC
mtlk_sq_peer_ctx_cleanup(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer);

/*!
\fn      void __MTLK_IFUNC mtlk_sq_peer_ctx_cancel_all_packets(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer)
\brief   Releases packets peer ctx

\param   pqueue Pointer to send queue structure
\param   ppeer SendQueue peer ctx
*/
void __MTLK_IFUNC
mtlk_sq_peer_ctx_cancel_all_packets(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer);

int __MTLK_IFUNC
mtlk_sq_enqueue_pkt(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer, mtlk_buffer_t *ppacket, uint16 access_category, BOOL front);

/*! 
\fn      void __MTLK_IFUNC mtlk_sq_flush(mtlk_sq_t *pqueue)
\brief   Processes send queue contents and sends as much packets as possible to the HW.

\param   pqueue Pointer to send queue structure
*/

void __MTLK_IFUNC
mtlk_sq_flush(mtlk_sq_t *pqueue);

/*! 
\fn      void __MTLK_IFUNC mtlk_sq_get_stats(mtlk_sq_t *pqueue, mtlk_sq_stats_t *pstats)
\brief   Copies send queue statistics to the caller's buffer.

\param   pqueue Pointer to send queue structure
\param   pstats Pointer to the caller's buffer
*/

void __MTLK_IFUNC
mtlk_sq_get_stats(mtlk_sq_t *pqueue, mtlk_sq_stats_t *pstats);

/*! 
\fn      static uint16 __INLINE mtlk_sq_get_limit(mtlk_sq_t *pqueue, uint8 ac)
\brief   Returns queue limit for given access category.

\param   pqueue  Pointer to send queue structure
\param   ac      Access category for which limit is being queried
*/

static uint16 __INLINE
mtlk_sq_get_limit(mtlk_sq_t *pqueue, uint8 ac)
{
    MTLK_ASSERT(NULL != pqueue);
    MTLK_ASSERT(ac < NTS_PRIORITIES);

    return pqueue->limits.global_queue_limit[ac];
}

/*! 
\fn      static uint16 __INLINE mtlk_sq_get_qsize(mtlk_sq_t *pqueue, uint8 ac)
\brief   Returns queue size for given access category.

\param   pqueue  Pointer to send queue structure
\param   ac      Access category for which queue size is being queried
*/

static uint16 __INLINE
mtlk_sq_get_qsize(mtlk_sq_t *pqueue, uint8 ac)
{
    ASSERT(pqueue != NULL);
    ASSERT(ac < NTS_PRIORITIES);

    return (uint16)mtlk_osal_atomic_get(&pqueue->peer_queue[ac].size);
}

static void __INLINE
_mtlk_sq_switch_if_urgent(mtlk_buffer_t *ppacket, uint16 *ac, BOOL *front)
{
    // Process out-of-band packet
    if (mtlk_bufmgr_is_urgent(ppacket))
    {
        *ac = AC_HIGHEST;
        *front = TRUE;
    }
}

/*!
=fn      void __MTLK_IFUNC mtlk_sq_on_tx_cfm(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer)
\brief   Request to free TX resouce for ppeer

\param   pqueue Pointer to send queue structure
\param   ppeer SendQueue peer ctx
*/
static void __INLINE
mtlk_sq_on_tx_cfm(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer)
{
    ASSERT( pqueue != NULL );

    if (ppeer == NULL) {
      ppeer = &pqueue->broadcast;
    }

    mtlk_osal_atomic_dec(&ppeer->used);
}

/*!
=fn      BOOL __MTLK_IFUNC mtlk_sq_is_all_packets_confirmed(mtlk_sq_peer_ctx_t *ppeer)
\brief   Shows is all packets sent to MAC already confirmed 

\param   ppeer SendQueue peer ctx
*/
static BOOL __INLINE
mtlk_sq_is_all_packets_confirmed(mtlk_sq_peer_ctx_t *ppeer)
{
  return (BOOL)(0 == mtlk_osal_atomic_get(&ppeer->used));
}

/*!
=fn      void __MTLK_IFUNC mtlk_sq_is_empty(mtlk_sq_peer_ctx_t *ppeer)
\brief   Returns true if the queue is empty

\param   ppeer SendQueue peer ctx
*/
static int __INLINE
mtlk_sq_is_empty(mtlk_sq_peer_ctx_t *ppeer)
{
    ASSERT (ppeer != NULL);
    return mtlk_osal_atomic_get(&ppeer->used) == 0;
}

int mtlk_sq_enqueue_clone_begin (mtlk_sq_t *sq, uint16 ac, BOOL front, struct sk_buff *skb);
int mtlk_sq_enqueue_clone(mtlk_sq_t *sq, struct sk_buff *clone_skb);
void mtlk_sq_enqueue_clone_end(mtlk_sq_t *sq, struct sk_buff *skb);
void _mtlk_sq_release_packet(mtlk_sq_t *sq, uint16 ac, struct sk_buff *skb);
int mtlk_sq_enqueue(mtlk_sq_t *sq, uint16 ac, BOOL front, struct sk_buff *skb);
void __MTLK_IFUNC mtlk_sq_set_pm_enabled(mtlk_sq_peer_ctx_t *ppeer, BOOL pm_enabled);

#endif //_MTLK_SENDQUEUE_H_
