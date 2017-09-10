/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"
#include "mtlk_osal.h"

static void
__mtlk_osal_timer_clb (unsigned long data)
{
  mtlk_osal_timer_t *timer = (mtlk_osal_timer_t *)data;
  uint32             msec  = 0;

  if (__LIKELY(!timer->stop)) {
    ASSERT(timer->clb != NULL);

    msec = timer->clb(timer, timer->clb_usr_data);
  }

  if (!msec) {
    /* Last hop or timer should stop =>
     * don't re-arm it and release the RM Lock .
     */
    mtlk_rmlock_release(&timer->rmlock);
  }
  else {
    /* Another hop required => 
     * re-arm the timer, RM Lock still acquired. 
     */
    mod_timer(&timer->os_timer, jiffies + msecs_to_jiffies(msec));
  }
}

int __MTLK_IFUNC
_mtlk_osal_timer_init (mtlk_osal_timer_t *timer,
                       mtlk_osal_timer_f  clb,
                       mtlk_handle_t      clb_usr_data)
{
  ASSERT(clb != NULL);

  memset(timer, 0, sizeof(*timer));
  init_timer(&timer->os_timer);
  mtlk_rmlock_init(&timer->rmlock);
  mtlk_rmlock_acquire(&timer->rmlock); /* Acquire RM Lock initially */

  timer->os_timer.data     = (unsigned long)timer;
  timer->os_timer.function = __mtlk_osal_timer_clb;
  timer->clb               = clb;
  timer->clb_usr_data      = clb_usr_data;

  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
_mtlk_osal_timer_set (mtlk_osal_timer_t *timer,
                      uint32             msec)
{
  _mtlk_osal_timer_cancel_sync(timer);

  timer->os_timer.expires  = jiffies + msecs_to_jiffies(msec);

  timer->stop = FALSE;                 /* Mark timer active         */
  mtlk_rmlock_acquire(&timer->rmlock); /* Acquire RM Lock on arming */
  add_timer(&timer->os_timer);

  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
_mtlk_osal_timer_cancel (mtlk_osal_timer_t *timer)
{
  timer->stop = TRUE; /* Mark timer stopped */
  if (del_timer(&timer->os_timer)) {
    /* del_timer_sync (and del_timer) returns whether it has 
     * deactivated a pending timer or not. (ie. del_timer() 
     * of an inactive timer returns 0, del_timer() of an
     * active timer returns 1.)
     * So, we're releasing RMLock for active (set) timers only.
     */
    mtlk_rmlock_release(&timer->rmlock);
  }
  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
_mtlk_osal_timer_cancel_sync (mtlk_osal_timer_t *timer)
{
  timer->stop = TRUE; /* Mark timer stopped */
  if (del_timer_sync(&timer->os_timer)) {
    /* del_timer_sync (and del_timer) returns whether it has 
     * deactivated a pending timer or not. (ie. del_timer() 
     * of an inactive timer returns 0, del_timer() of an
     * active timer returns 1.)
     * So, we're releasing RMLock for active (set) timers only.
     */
    mtlk_rmlock_release(&timer->rmlock);
  }
  return MTLK_ERR_OK;
}

void __MTLK_IFUNC
_mtlk_osal_timer_cleanup (mtlk_osal_timer_t *timer)
{
  _mtlk_osal_timer_cancel_sync(timer); /* Cancel Timer           */
  mtlk_rmlock_release(&timer->rmlock); /* Release last reference */
  mtlk_rmlock_wait(&timer->rmlock);    /* Wait for completion    */
  mtlk_rmlock_cleanup(&timer->rmlock); /* Cleanup the RM Lock    */
}

/**************************************************************************/
/*                                                                        */
/*                     *** NON_INLINE RATIONALE ***                       */
/*                                                                        */
/* There is a bug in GCC compiler. kmalloc sometimes produces link-time   */
/* error referencing the undefined symbol __you_cannot_kmalloc_that_much. */
/* This is a known problem. For example, see:                             */
/*   http://gcc.gnu.org/ml/gcc-patches/1998-09/msg00632.html              */
/*   http://lkml.indiana.edu/hypermail/linux/kernel/0901.3/01060.html     */
/* The root cause is in __builtin_constant_p which is GCC built-in that   */
/* provides information whether specified value is compile-time constant. */
/* Sometimes it may claim non-constant values as constants and break      */
/* kmalloc logic.                                                         */
/* Robust work around for this problem is non-inline wrapper for kmalloc. */
/* For this reason mtlk_osal_mem* functions made NON-INLINE.              */
/*                                                                        */
/**************************************************************************/

void *
__mtlk_kmalloc (size_t size, int flags)
{
  return kmalloc(size, flags);
}

void
__mtlk_kfree (void *p)
{
  kfree(p);
}

