/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id:$
 *
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *
 */
#include "mtlkinc.h"

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/seq_file.h>

#include "mtlk_sq.h"
#include "mcast.h"
#include "stadb.h"

/* called from shared code send queue implementation */
int _mtlk_sq_send_to_hw(mtlk_sq_t *pq, struct sk_buff *skb, uint16 prio, int *status)
{
  int res;

  /* check the flow control "stopped" flag */
  if(unlikely(pq->pcore->tx_prohibited))
    return MTLK_ERR_PROHIB;


  CPU_STAT_BEGIN_TRACK(CPU_STAT_ID_TX_FLUSH);
  res = mtlk_xmit(skb, skb->dev);
  CPU_STAT_END_TRACK(CPU_STAT_ID_TX_FLUSH);

  return res;
}

/* this function is called when the queue needs to be awaken */
void mtlk_sq_schedule_flush(struct nic *nic)
{
  tasklet_schedule(nic->sq_flush_tasklet);
}

/* tasklet to flush awaken queue */
static void mtlk_sq_flush_tasklet(unsigned long data)
{
  struct nic *nic = (struct nic *)data;

  mtlk_sq_flush(nic->sq);
}

/* called during initialization from core_start */
int sq_init (struct nic *nic)
{
  struct _mtlk_sq_t *sq;
  struct tasklet_struct *ft;
  int i;
  
  int res = MTLK_ERR_UNKNOWN;
  /* these limits are taken from tc script on dongle.
   * maybe it's better to make them settable from userspace.
   */
  struct _mtlk_sq_limits limits = {
    .global_queue_limit = {800, 50, 1100, 100},
    .peer_queue_limit =   {600, 38, 825 , 75 } /* 75% */
  };

  /* allocate send queue struct */
  sq = kmalloc_tag(sizeof(*sq), GFP_KERNEL, MTLK_MEM_TAG_SEND_QUEUE);
  if (!sq) {
    ELOG("unable to allocate memory for send queue.");
    res = MTLK_ERR_NO_MEM;
    goto out;
  }
  memset(sq, 0, sizeof(*sq));

  /* allocate flush tasklet struct */
  ft = kmalloc_tag(sizeof(*ft), GFP_KERNEL, MTLK_MEM_TAG_SEND_QUEUE);
  if (!ft) {
    ELOG("unable to allocate memory for send queue flush tasklet");
    res = MTLK_ERR_NO_MEM;
    goto err_tasklet;
  }
  tasklet_init(ft, mtlk_sq_flush_tasklet, (unsigned long)nic);

  /* connect allocated memory to "nic" structure */
  nic->sq = sq;
  nic->sq_flush_tasklet = ft;

  /* "create" send queue */
  mtlk_sq_init(nic->sq, nic);

  /* set the limits for queues */
  for(i = 0; i < NTS_PRIORITIES; i++) {
    sq->limits.global_queue_limit[i] = limits.global_queue_limit[i];
    sq->limits.peer_queue_limit[i] = limits.peer_queue_limit[i];
  }
  
  return MTLK_ERR_OK;

err_tasklet:
  kfree_tag(sq);
out:
  return res;
}

/* called when exiting from core_delete */
int sq_cleanup (struct nic *nic)
{
  int res = MTLK_ERR_UNKNOWN;

  if (!nic->sq || !nic->sq_flush_tasklet)
    goto out;

  /* "release" the send queue */
  mtlk_sq_cleanup(nic->sq);

  /* synchronously disable tasklet */
  tasklet_disable(nic->sq_flush_tasklet);
  
  /* deallocate memory */
  kfree_tag(nic->sq);
  kfree_tag(nic->sq_flush_tasklet);

  res = MTLK_ERR_OK;

out:  
  return res;
}


static int
mtlk_xmit_sq_enqueue (struct sk_buff *skb, struct net_device *dev, uint16 access_category, BOOL front)
{
  struct skb_private *skb_priv = (struct skb_private *)(skb->cb);
  struct nic *nic = netdev_priv(dev);
  mtlk_sq_peer_ctx_t *sq_peer_ctx;
  int res;

  if (skb_priv->dst_sta != NULL) {
    sq_peer_ctx = &skb_priv->dst_sta->sq_peer_ctx;
  } else {
    sq_peer_ctx = NULL;
  }

  /* try to enqueue packet into the "send queue" */
  res = mtlk_sq_enqueue_pkt(nic->sq, sq_peer_ctx, skb, access_category, front);

  /* if error - drop it */
  if (unlikely(res != MTLK_ERR_OK))
    mtlk_xmit_err(nic, skb);
  
  return res;   
}

void mtlk_xmit_sq_flush (struct nic *nic)
{
  mtlk_sq_flush(nic->sq);
}


int sq_get_limits(struct nic *nic, int32 *limits, int *written_num)
{
  int i;

  /* copy limits to destination buffer */
  for (i = 0; i < NTS_PRIORITIES; i++)
    limits[i] = mtlk_sq_get_limit(nic->sq, i);

  *written_num = NTS_PRIORITIES;

  return MTLK_ERR_OK;
}

int sq_get_peer_limits(struct nic *nic, int32 *ratio, int *written_num)
{
  mtlk_sq_t *sq = nic->sq;
  int i;

  mtlk_osal_lock_acquire(&sq->queue_lock);

  for (i = 0; i < NTS_PRIORITIES; i++)
    ratio[i] = (100*sq->limits.peer_queue_limit[i])/sq->limits.global_queue_limit[i];

  mtlk_osal_lock_release(&sq->queue_lock);

  *written_num = NTS_PRIORITIES;

  return MTLK_ERR_OK;
}

int sq_set_limits(struct nic *nic, int32 *global_queue_limit, int num)
{
  mtlk_sq_t *sq = nic->sq;
  int i;

  /* accept only the exact number */
  if (num != NTS_PRIORITIES)
    return MTLK_ERR_UNKNOWN;

  for (i = 0; i < NTS_PRIORITIES; i++)
    if (global_queue_limit[i] <= 0) return MTLK_ERR_UNKNOWN;

  mtlk_osal_lock_acquire(&sq->queue_lock);

  for (i = 0; i < NTS_PRIORITIES; i++) {
    int ratio = (100*sq->limits.peer_queue_limit[i])/sq->limits.global_queue_limit[i];
    sq->limits.global_queue_limit[i] = global_queue_limit[i];
    sq->limits.peer_queue_limit[i] = (ratio*global_queue_limit[i])/100;
  }

  mtlk_osal_lock_release(&sq->queue_lock);

  return MTLK_ERR_OK;
}

int sq_set_peer_limits(struct nic *nic, int32 *ratio, int num)
{
  mtlk_sq_t *sq = nic->sq;
  const int min_peer_queue_size_ratio = 5;
  const int max_peer_queue_size_ratio = 100;
  int i;

  /* accept only the exact number */
  if (num != NTS_PRIORITIES)
    return MTLK_ERR_UNKNOWN;

  for (i = 0; i < NTS_PRIORITIES; i++) {
    if (ratio[i] < min_peer_queue_size_ratio) return MTLK_ERR_UNKNOWN;
    if (ratio[i] > max_peer_queue_size_ratio) return MTLK_ERR_UNKNOWN;
  }

  mtlk_osal_lock_acquire(&sq->queue_lock);

  for (i = 0; i < NTS_PRIORITIES; i++)
    sq->limits.peer_queue_limit[i] = MAX(1, (ratio[i]*sq->limits.global_queue_limit[i])/100);

  mtlk_osal_lock_release(&sq->queue_lock);

  return MTLK_ERR_OK;
}

void mtlk_sq_show_stats (struct seq_file *s, void *v, mtlk_core_t *nic)
{
  struct _mtlk_sq_stats stats;
  struct _mtlk_sq_qsizes sizes;
  mtlk_sq_t *sq = nic->sq;
  mtlk_dlist_entry_t *entry;
  mtlk_dlist_entry_t *head;
  int i;
  uint8 mac_addr[ETH_ALEN];
  
  /* get statistics and current limits */
  mtlk_sq_get_stats(nic->sq, &stats);

  for(i = 0; i < NTS_PRIORITIES; i++)
      sizes.qsize[i] = mtlk_sq_get_qsize(nic->sq, i);

  if (!nic->pack_sched_enabled)
      return;
        
/* iterate with format string */
#define ITF(head_str, ...)           \
  seq_printf(s, head_str);            \
  for (i = 0; i < NTS_PRIORITIES; i++) \
    seq_printf(s,  __VA_ARGS__ );       \
  seq_printf(s,"\n");

  ITF("--------------------------", "%s", "-------------");
  ITF("Name                     |", "    %s   |", mtlk_qos_get_ac_name(i));
  ITF("--------------------------", "%s", "-------------");

/* iterating through numbers array */
#define ITA(val_str, arr)            \
  seq_printf(s, val_str);             \
  for (i = 0; i < NTS_PRIORITIES; i++) \
    seq_printf(s, " %10d |", (arr)[i]); \
  seq_printf(s,"\n");

  mtlk_osal_lock_acquire(&sq->queue_lock);

  ITA("packets pushed           |", stats.pkts_pushed);
  ITA("packets sent to UM       |", stats.pkts_sent_to_um);
  ITF("global queue limits      |", " %10d |", sq->limits.global_queue_limit[i]);
  ITF("peer queue limits        |", " %10d |", sq->limits.peer_queue_limit[i]);
  ITA("current sizes            |", sizes.qsize);
  ITA("packets dropped (limit)  |", stats.pkts_limit_dropped);

  mtlk_osal_lock_release(&sq->queue_lock);

  ITF("--------------------------", "%s", "-------------");

  if (mtlk_core_get_net_state(nic) != NET_STATE_CONNECTED)
    goto out;

  mtlk_osal_lock_acquire(&sq->queue_lock);

  mtlk_dlist_foreach(&sq->peer_queue[AC_BE].list, entry, head) {
    mtlk_peer_queue_entry_t *ppeer_queue_entry =
      MTLK_CONTAINER_OF(entry, mtlk_peer_queue_entry_t, list_entry);
    mtlk_sq_peer_ctx_t *ppeer =
      MTLK_CONTAINER_OF(ppeer_queue_entry, mtlk_sq_peer_ctx_t, peer_queue_entry[AC_BE]);

    if (&sq->broadcast != ppeer)  {
      sta_entry *sta =
        MTLK_CONTAINER_OF(ppeer, sta_entry, sq_peer_ctx);
      memcpy(mac_addr, sta->mac, ETH_ALEN);
    }
    else
      memset (&mac_addr, 0xFF, ETH_ALEN);


    seq_printf(s, "\nMAC:" MAC_FMT "\nLimit: %10u\nUsed: %11u\n", 
               MAC_ARG(mac_addr), mtlk_osal_atomic_get(&ppeer->limit), mtlk_osal_atomic_get(&ppeer->used));
    ITF("    --------------------------", "%s", "-------------");
    ITF("    Name                     |", "    %s   |", mtlk_qos_get_ac_name(i));
    ITF("    --------------------------", "%s", "-------------");
    ITA("    packets pushed           |", ppeer->stats.pkts_pushed);
    ITA("    packets sent to UM       |", ppeer->stats.pkts_sent_to_um);
    ITF("    current sizes            |", " %10d |", mtlk_buflist_size(&ppeer->peer_queue_entry[i].buflist));
    ITA("    packets dropped (limit)  |", ppeer->stats.pkts_limit_dropped);
  }

  ITF("    --------------------------", "%s", "-------------");

  mtlk_osal_lock_release(&sq->queue_lock);

#undef ITA
#undef ITF

out:
  return;
}

int mtlk_sq_enqueue_clone_begin (mtlk_sq_t *sq, uint16 ac, BOOL front, struct sk_buff *skb)
{
  struct skb_private *skb_priv = (struct skb_private *)(skb->cb);

  skb_priv->extra = mtlk_osal_mem_alloc(sizeof(skb_ext_t),
    MTLK_MEM_TAG_SEND_QUEUE_CLONE);
  if (skb_priv->extra == NULL)
    return MTLK_ERR_NO_MEM;
  mtlk_osal_atomic_set(&skb_priv->extra->ref_cnt, 1);

  _mtlk_sq_switch_if_urgent(skb, &ac, &front);
  skb_priv->extra->ac = ac;
  skb_priv->extra->front = front;

  mtlk_osal_atomic_inc(&sq->peer_queue[ac].size);

  return MTLK_ERR_OK;
}

int mtlk_sq_enqueue_clone(mtlk_sq_t *sq, struct sk_buff *clone_skb)
{
  struct skb_private *skb_priv = (struct skb_private *)(clone_skb->cb);
  int res;
  skb_ext_t *extra = skb_priv->extra;

  mtlk_osal_atomic_inc(&extra->ref_cnt);
  res = mtlk_xmit_sq_enqueue(clone_skb, clone_skb->dev, extra->ac, extra->front);
  if (res != MTLK_ERR_OK)
    mtlk_osal_atomic_dec(&extra->ref_cnt);

  return res;
}

void mtlk_sq_enqueue_clone_end(mtlk_sq_t *sq, struct sk_buff *skb)
{
  struct skb_private *skb_priv = (struct skb_private *)(skb->cb);
  skb_ext_t *extra = skb_priv->extra;

  if (mtlk_osal_atomic_dec(&extra->ref_cnt) == 0) {
    mtlk_osal_atomic_dec(&sq->peer_queue[extra->ac].size);
    mtlk_osal_mem_free(extra);
    skb_priv->extra = NULL;
  }
}

/* Be aware! This function is for SQ internal usage only. 
 * You will never know from the outside the exact queue 
 * to which SQ has placed the packet (due to the "urgent" packets).
 */ 
void _mtlk_sq_release_packet(mtlk_sq_t *sq, uint16 ac, struct sk_buff *skb)
{
  struct skb_private *skb_priv = (struct skb_private *)(skb->cb);

  if (skb_priv->flags & SKB_RMCAST) {
    ASSERT(skb_priv->extra != NULL);
    mtlk_sq_enqueue_clone_end(sq, skb);
  } else {
    mtlk_osal_atomic_dec(&sq->peer_queue[ac].size);
  }
}

int mtlk_sq_enqueue(mtlk_sq_t *sq, uint16 ac, BOOL front, struct sk_buff *skb)
{
  int res;

  _mtlk_sq_switch_if_urgent(skb, &ac, &front);

  mtlk_osal_atomic_inc(&sq->peer_queue[ac].size);
  res = mtlk_xmit_sq_enqueue(skb, skb->dev, ac, front);
  if (res != MTLK_ERR_OK) {
    mtlk_osal_atomic_dec(&sq->peer_queue[ac].size);
  }

  return res;
}

