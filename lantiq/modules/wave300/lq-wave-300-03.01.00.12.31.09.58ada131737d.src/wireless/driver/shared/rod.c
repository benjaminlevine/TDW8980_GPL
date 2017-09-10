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
#include "iperf_debug.h"

#define SEQUENCE_NUMBER_LIMIT           (0x1000)
#define SEQ_DISTANCE(seq1, seq2)       (((seq2) - (seq1) + SEQUENCE_NUMBER_LIMIT) \
                                         % SEQUENCE_NUMBER_LIMIT);

// Retire packet from the traffic stream in slot number
static int
retire_slot (reordering_queue *prod_queue, int slot)
{
  int res = -1;
  rod_buff *buff;

  ASSERT(prod_queue->used == 1);

  slot %= prod_queue->window_size;
  buff = prod_queue->buff[slot];
  ILOG4(GID_ROD, "slot, %d, skb %p", slot, buff);

  if (buff != NULL) {
    mtlk_debug_process_iperf_payload_rx(prod_queue,slot);
    mtlk_rod_detect_replay_or_sendup(buff, prod_queue->rsc);
    prod_queue->buff[slot] = NULL;
    prod_queue->last_sent = mtlk_osal_timestamp();
    prod_queue->count--;
    ASSERT(prod_queue->count <= prod_queue->window_size);

    res = slot;
  } else
    prod_queue->stats.lost++;

  return res;
}

void __MTLK_IFUNC
mtlk_create_rod_queue (reordering_queue *prod_queue, int win_size, int ssn)
{
  mtlk_osal_lock_acquire(&prod_queue->lock);

  if (prod_queue->used == 1)
  {
    ILOG1(GID_ROD, "TS already exist");
    mtlk_flush_rod_queue(prod_queue);
  }

  prod_queue->used = 1;
  prod_queue->count = 0;
  prod_queue->head = ssn;
  prod_queue->window_size = win_size;
  prod_queue->last_sent = mtlk_osal_timestamp();
  memset(&prod_queue->stats, 0, sizeof(reordering_stats));

  mtlk_osal_lock_release(&prod_queue->lock);
}

/* move reordering window on N positions retiring slots */
static void
rod_move_window(reordering_queue *prod_queue, int n)
{
  int i;

  ASSERT(prod_queue->used == 1);

  for (i = 0; i < n; i++) {
    retire_slot(prod_queue, prod_queue->head);
    prod_queue->head++; // move window
  }
  prod_queue->head %= SEQUENCE_NUMBER_LIMIT;
}

void __MTLK_IFUNC
mtlk_flush_rod_queue (reordering_queue *prod_queue)
{
    while(prod_queue->count)
    {
        rod_move_window(prod_queue, 1);
    }
}

void
__MTLK_IFUNC mtlk_handle_rod_queue_timer(reordering_queue *prod_queue)
{
  mtlk_osal_lock_acquire(&prod_queue->lock);

  if (prod_queue->used == 1) {
    if (prod_queue->count && mtlk_osal_time_after(mtlk_osal_timestamp(),
        prod_queue->last_sent + mtlk_osal_ms_to_timestamp(ROD_QUEUE_FLUSH_TIMEOUT_MS)))
      mtlk_flush_rod_queue(prod_queue);
  }

  mtlk_osal_lock_release(&prod_queue->lock);
}

void __MTLK_IFUNC
mtlk_clear_rod_queue (reordering_queue *prod_queue)
{
  mtlk_osal_lock_acquire(&prod_queue->lock);

  if (prod_queue->used != 1)
  {
    ILOG3(GID_ROD, "TS do not exist");
    goto end;
  }

  mtlk_flush_rod_queue(prod_queue);
  prod_queue->used = 0;

end:
  mtlk_osal_lock_release(&prod_queue->lock);
}

// Retire packets in order from head until an empty slot is reached
static int
retire_rod_queue(reordering_queue *rq)
{
  int n=0;
  int loc = rq->head % rq->window_size;
  while (rq->buff[loc] != NULL) {
    retire_slot(rq, loc);
    loc++;
    if (loc >= (int)rq->window_size)
        loc = 0;
    rq->head++;
    n++;
  }
  rq->head %= SEQUENCE_NUMBER_LIMIT;
  return n;
}

void __MTLK_IFUNC
mtlk_reorder_packet (reordering_queue *prod_queue, int seq, rod_buff *buff)
{
  uint32 diff;
  int loc;
    
  mtlk_osal_lock_acquire(&prod_queue->lock);

  if (prod_queue->used == 0)
  {
    // legacy packets (i.e. not aggregated)
    mtlk_rod_detect_replay_or_sendup(buff, prod_queue->rsc);
    goto end;
  }

  ASSERT(seq >= 0 && seq < SEQUENCE_NUMBER_LIMIT);

  diff = SEQ_DISTANCE(prod_queue->head, seq);
  ASSERT(diff < SEQUENCE_NUMBER_LIMIT);

  if (diff > SEQUENCE_NUMBER_LIMIT/2) {
    // This is a packet that has been already seen in the past(?)
    ILOG3(GID_ROD, "too old packet, seq %d, head %u, diff %d > %d",
        seq, prod_queue->head, diff, SEQUENCE_NUMBER_LIMIT/2);
    ++prod_queue->stats.too_old;
    mtlk_rod_drop_packet(buff);
    goto end;
  }

  if (diff >= prod_queue->window_size) {
    int delta;
    ILOG3(GID_ROD, "packet outside of the window, seq %d, head %u, diff %d >= %u",
        seq, prod_queue->head, diff, prod_queue->window_size);
    ++prod_queue->stats.overflows;
    delta = diff - prod_queue->window_size + 1;
    rod_move_window(prod_queue, delta);
    diff = SEQ_DISTANCE(prod_queue->head, seq);
    ASSERT(diff == prod_queue->window_size - 1);
  }

  // Insert the skb into the reorder array
  // at the location of the sequence number.
  loc= seq % prod_queue->window_size;
  if (prod_queue->buff[loc] == NULL) {
    prod_queue->buff[loc]= buff;
    prod_queue->count++;
    ASSERT(prod_queue->count <= prod_queue->window_size);
    ILOG3(GID_ROD, "packet %p queued to slot %d, total queued %u",
        buff, loc, prod_queue->count);
    ++prod_queue->stats.queued;
  } else {
    mtlk_rod_drop_packet(buff);
    ILOG3(GID_ROD, "duplicate packet %p dropped, slot %d", buff, loc);
    ++prod_queue->stats.duplicate;
    goto end;
  }

  retire_rod_queue(prod_queue);

end:
  mtlk_osal_lock_release(&prod_queue->lock);
}

int __MTLK_IFUNC
mtlk_rod_process_bar(reordering_queue* prod_queue, uint16 ssn)
{
  int diff, res = 0;
    
  mtlk_osal_lock_acquire(&prod_queue->lock);

  if (prod_queue->used == 0) {
    ELOG("Received BAR for wrong TID (no BA agreement)");
    res = -1;
    goto end;
  }
  ASSERT(ssn < SEQUENCE_NUMBER_LIMIT);

  diff = SEQ_DISTANCE(prod_queue->head, ssn);
  if (diff > SEQUENCE_NUMBER_LIMIT/2) {
    /* sequence number is in the past */
    ILOG3(GID_ROD, "Nothing to free - SSN is in the past: seq %d, head %u, diff %d > %d",
          ssn, prod_queue->head, diff, SEQUENCE_NUMBER_LIMIT/2);
    goto end;
  }

  // Move reordering window to new received SSN
  ILOG3(GID_ROD, "BAR: SSN %d, head %u", ssn, prod_queue->head);
  rod_move_window(prod_queue, diff);

  retire_rod_queue(prod_queue);

end:
  mtlk_osal_lock_release(&prod_queue->lock);

  return res;
}
