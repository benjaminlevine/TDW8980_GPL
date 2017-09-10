/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/**********************************************************************
 * MetaLink Driver OS Abstraction Layer implementation
 * 
 * This file:
 * [*] defines system-dependent implementation of OSAL data types 
 *     and interfaces.
 * [*] is included in mtlk_osal.h only. No other files must include it!
 *
 * NOTE (MUST READ!!!): 
 *  OSAL_... macros (if defined) are designed for OSAL internal 
 *  usage only (see mtlk_osal.h for more information). They can not 
 *  and must not be used anywhere else.
***********************************************************************/

#if !defined (SAFE_PLACE_TO_INCLUDE_OSAL_OSDEP_DEFS) /* definitions - functions etc. */
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_OSAL_OSDEP_... */

#undef SAFE_PLACE_TO_INCLUDE_OSAL_OSDEP_DEFS

#include <linux/slab.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <linux/hardirq.h> /* in_atomic() is here for 2.6 */
#else
#include <asm/hardirq.h> /* in_interrupt() is here for 2.4 */
#define in_atomic() in_interrupt()
#endif

#include "mem_leak.h"

MTLK_DECLARE_OBJPOOL(g_objpool);

static __INLINE int
mtlk_osal_init (void)
{
  return mtlk_objpool_init(&g_objpool);
}

static __INLINE void
mtlk_osal_cleanup (void)
{
  mtlk_objpool_cleanup(&g_objpool);
}

static __INLINE void* 
mtlk_osal_mem_dma_alloc (uint32 size, uint32 tag)
{
  return kmalloc_tag(size, GFP_ATOMIC | __GFP_DMA, tag);
}

static __INLINE void* 
mtlk_osal_mem_alloc (uint32 size, uint32 tag)
{
  return kmalloc_tag(size, GFP_ATOMIC, tag);
}

static __INLINE void 
mtlk_osal_mem_free (void* buffer)
{
  kfree_tag(buffer);
}

static __INLINE int
mtlk_osal_lock_init (mtlk_osal_spinlock_t* spinlock)
{
  spin_lock_init(&spinlock->lock); 
  return MTLK_ERR_OK;
}

static __INLINE void
mtlk_osal_lock_acquire (mtlk_osal_spinlock_t* spinlock)
{
  spin_lock_bh(&spinlock->lock);
}

static __INLINE mtlk_handle_t
mtlk_osal_lock_acquire_irq (mtlk_osal_spinlock_t* spinlock)
{
  unsigned long res = 0; 
  spin_lock_irqsave(&spinlock->lock, res); 
  return HANDLE_T(res);
}

static __INLINE void
mtlk_osal_lock_release (mtlk_osal_spinlock_t* spinlock)
{
  spin_unlock_bh(&spinlock->lock);
}

static __INLINE void
mtlk_osal_lock_release_irq (mtlk_osal_spinlock_t* spinlock, 
                            mtlk_handle_t         acquire_val)
{
  spin_unlock_irqrestore(&spinlock->lock, (unsigned long)acquire_val);
}

static __INLINE void
mtlk_osal_lock_cleanup (mtlk_osal_spinlock_t* spinlock)
{

}

static __INLINE int
mtlk_osal_event_init (mtlk_osal_event_t* event)
{
  event->wait_flag = 0;
  init_waitqueue_head(&event->wait_queue);
  return MTLK_ERR_OK;
}

static __INLINE int
mtlk_osal_event_wait (mtlk_osal_event_t* event, uint32 msec)
{
  int res = wait_event_timeout(event->wait_queue, 
                               event->wait_flag,
                               msecs_to_jiffies(msec));

  if (res == 0) 
    res = MTLK_ERR_TIMEOUT;
  else if (res > 0) 
    res = MTLK_ERR_OK;
  else {
    // make sure we cover all cases
    ERROR("wait_event_timeout returned %d", res);
    ASSERT(FALSE);
  }

  return res;
}

static __INLINE void 
mtlk_osal_event_set (mtlk_osal_event_t* event)
{
  event->wait_flag = 1;
  wake_up(&event->wait_queue);
}

static __INLINE void
mtlk_osal_event_reset (mtlk_osal_event_t* event)
{
  event->wait_flag = 0;
}

static __INLINE void
mtlk_osal_event_cleanup (mtlk_osal_event_t* event)
{

}

static __INLINE int
mtlk_osal_mutex_init (mtlk_osal_mutex_t* mutex)
{
  init_MUTEX(&mutex->sem); 
  return MTLK_ERR_OK;
}

static __INLINE void
mtlk_osal_mutex_acquire (mtlk_osal_mutex_t* mutex)
{
  down(&mutex->sem); 
}

static __INLINE void
mtlk_osal_mutex_release (mtlk_osal_mutex_t* mutex)
{
  up(&mutex->sem);
}

static __INLINE void
mtlk_osal_mutex_cleanup (mtlk_osal_mutex_t* mutex)
{

}

static __INLINE void
mtlk_osal_msleep (uint32 msec)
{
  msleep(msec);
}

int  __MTLK_IFUNC _mtlk_osal_timer_init(mtlk_osal_timer_t *timer,
                                        mtlk_osal_timer_f  clb,
                                        mtlk_handle_t      clb_usr_data);
int  __MTLK_IFUNC _mtlk_osal_timer_set(mtlk_osal_timer_t *timer,
                                       uint32             msec);
int  __MTLK_IFUNC _mtlk_osal_timer_cancel(mtlk_osal_timer_t *timer);
int  __MTLK_IFUNC _mtlk_osal_timer_cancel_sync(mtlk_osal_timer_t *timer);
void __MTLK_IFUNC _mtlk_osal_timer_cleanup(mtlk_osal_timer_t *timer);

static __INLINE int
mtlk_osal_timer_init (mtlk_osal_timer_t *timer,
                     mtlk_osal_timer_f  clb,
                     mtlk_handle_t      clb_usr_data)
{
  return _mtlk_osal_timer_init(timer, clb, clb_usr_data);
}

static __INLINE int
mtlk_osal_timer_set (mtlk_osal_timer_t *timer,
                     uint32             msec)
{
  return _mtlk_osal_timer_set(timer, msec);
}

static __INLINE int
mtlk_osal_timer_cancel (mtlk_osal_timer_t *timer)
{
  return _mtlk_osal_timer_cancel(timer);
}

static __INLINE int
mtlk_osal_timer_cancel_sync (mtlk_osal_timer_t *timer)
{
  return _mtlk_osal_timer_cancel_sync(timer);
}

static __INLINE void
mtlk_osal_timer_cleanup (mtlk_osal_timer_t *timer)
{
  _mtlk_osal_timer_cleanup(timer);
}

static __INLINE mtlk_osal_timestamp_t
mtlk_osal_timestamp (void)
{
  return jiffies;
}

static __INLINE mtlk_osal_msec_t
mtlk_osal_timestamp_to_ms (mtlk_osal_timestamp_t timestamp)
{
  return jiffies_to_msecs(timestamp);
}

static __INLINE mtlk_osal_timestamp_t
mtlk_osal_ms_to_timestamp (mtlk_osal_msec_t msecs)
{
  return msecs_to_jiffies(msecs);
}

static __INLINE int
mtlk_osal_time_after (mtlk_osal_timestamp_t tm1, mtlk_osal_timestamp_t tm2)
{
  return time_after(tm1, tm2);
}

static __INLINE mtlk_osal_ms_diff_t
mtlk_osal_ms_time_diff (mtlk_osal_msec_t tm1, mtlk_osal_msec_t tm2)
{
  return ( (mtlk_osal_ms_diff_t) ( (uint32)(tm1) - (uint32)(tm2) ) );
}

static __INLINE mtlk_osal_timestamp_diff_t
mtlk_osal_timestamp_diff (mtlk_osal_timestamp_t tm1, mtlk_osal_timestamp_t tm2)
{
  return ( (mtlk_osal_timestamp_diff_t) ( (uint32)(tm1) - (uint32)(tm2) ) );
}

#include <linux/etherdevice.h>
#include "compat.h"

static __INLINE void 
mtlk_osal_copy_eth_addresses (uint8* dst, 
                              const uint8* src)
{
  memcpy(dst, src, ETH_ALEN);
}

static __INLINE int 
mtlk_osal_compare_eth_addresses (const uint8* addr1, 
                                 const uint8* addr2)
{
  return compare_ether_addr(addr1, addr2);
}

static __INLINE int
mtlk_osal_is_zero_address (const uint8* addr)
{
  return is_zero_ether_addr(addr);
}

static __INLINE int 
mtlk_osal_eth_is_multicast (const uint8* addr)
{
  return is_multicast_ether_addr(addr);
}

static __INLINE int
mtlk_osal_eth_is_broadcast (const uint8* addr)
{
  return is_broadcast_ether_addr(addr);
}

static __INLINE int
mtlk_osal_is_valid_ether_addr(const uint8* addr)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,6)
  return is_valid_ether_addr((uint8*)addr);
#else
  return is_valid_ether_addr(addr);
#endif
}

static __INLINE void
mtlk_osal_eth_apply_mask(uint8* dst, uint8* src, const uint8* mask)
{
  dst[0] = src[0] & mask[0];
  dst[1] = src[1] & mask[1];
  dst[2] = src[2] & mask[2];
  dst[3] = src[3] & mask[3];
  dst[4] = src[4] & mask[4];
  dst[5] = src[5] & mask[5];
}

/* atomic counters */

static __INLINE uint32
mtlk_osal_atomic_add (mtlk_atomic_t* val, uint32 i)
{
  return atomic_add_return(i, val);
}

static __INLINE uint32
mtlk_osal_atomic_sub (mtlk_atomic_t* val, uint32 i)
{
  return atomic_sub_return(i, val);
}

static __INLINE uint32
mtlk_osal_atomic_inc (mtlk_atomic_t* val)
{
  return atomic_inc_return(val);
}

static __INLINE uint32
mtlk_osal_atomic_dec (mtlk_atomic_t* val)
{
  return atomic_dec_return(val);
}

static __INLINE void
mtlk_osal_atomic_set (mtlk_atomic_t* target, uint32 value)
{
  atomic_set(target, value);
}

static __INLINE uint32
mtlk_osal_atomic_get (mtlk_atomic_t* val)
{
  return atomic_read(val);
}

static __INLINE uint32
mtlk_osal_atomic_xchg (mtlk_atomic_t* target, uint32 value)
{
  return atomic_xchg(target, value);
}

#include "mtlkrmlock.h"

struct _mtlk_osal_timer_t
{
  MTLK_DECLARE_OBJECT_HEADER(obj_hdr);

  struct timer_list os_timer;
  mtlk_osal_timer_f clb;
  mtlk_handle_t     clb_usr_data;
  mtlk_rmlock_t     rmlock;
  volatile BOOL     stop;
};
