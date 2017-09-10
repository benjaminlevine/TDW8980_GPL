/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id: core.c 5881 2009-01-26 11:50:32Z andriya $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Linux dependant event queue part 
 *
 */

#include "mtlkinc.h"
#include "mtlk_eq.h"

#define AUTO_WAKE_TIMEOUT 5000

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,4)

static int
thread(void *data)
{
  struct mtlk_eq_osdep *osdep = (struct mtlk_eq_osdep*)data;
  struct mtlk_eq *eq = MTLK_CONTAINER_OF(osdep, struct mtlk_eq, osdep); 

  /* in order to be able to stop the thread fast we have 2 phases:
    - first while-loop waits for an event with predetermined time-out
      of AUTO_WAKE_TIMEOUT
    - second waits for kthread_stop call actually

    so, the steps to stop are:
      1.1. set eq->started to FALSE
      1.2. send event osdep->new_event
      2. call kthread_stop
    
    this approach allows us to auto wake the thread at run-time
    with long time-out of 5 sec and at the other hand to quit the thread
    in number of milliseconds on rmmod */

  while (eq->started) {
    mtlk_osal_event_wait(&osdep->new_event, AUTO_WAKE_TIMEOUT);
    mtlk_osal_event_reset(&osdep->new_event);

    mtlk_eq_handle_events(eq);
  }

  while (!kthread_should_stop()) {
    schedule();
  }

  return 0;
}

int __MTLK_IFUNC
_mtlk_eq_osdep_start(struct mtlk_eq_osdep* osdep)
{
  osdep->thread = kthread_run(thread, (void*)osdep, "mtlk");
  if (osdep->thread == NULL) {
    ELOG("Failed to start kernel thread");
    return MTLK_ERR_UNKNOWN;
  }

  return MTLK_ERR_OK; 
}

void __MTLK_IFUNC
_mtlk_eq_osdep_stop(struct mtlk_eq_osdep *osdep)
{
  mtlk_osal_event_set(&osdep->new_event);
  ILOG2(GID_EQ, "waiting for thread to stop");
  kthread_stop(osdep->thread);
}

void __MTLK_IFUNC
_mtlk_eq_osdep_cleanup (struct mtlk_eq_osdep *osdep)
{
  mtlk_osal_event_cleanup(&osdep->new_event);
}

int __MTLK_IFUNC
_mtlk_eq_osdep_init (struct mtlk_eq_osdep *osdep)
{
  int res;
  
  res = mtlk_osal_event_init(&osdep->new_event);
  if (res != MTLK_ERR_OK) {
    ELOG("Failed to initialize event with err code %i", res);
    goto err;
  }

err:
  return res;
}

void __MTLK_IFUNC
_mtlk_eq_osdep_notify (struct mtlk_eq_osdep *osdep)
{
  mtlk_osal_event_set(&osdep->new_event);
}

#else

static uint32
timer_handler (mtlk_osal_timer_t *timer, mtlk_handle_t data)
{
  struct mtlk_eq_osdep *osdep = (struct mtlk_eq_osdep*)data;
  _mtlk_eq_osdep_notify(osdep);
  return AUTO_WAKE_TIMEOUT;
}

static void 
work_handler(void *data)
{
  struct mtlk_eq_osdep *osdep = (struct mtlk_eq_osdep*)data;
  struct mtlk_eq *eq = MTLK_CONTAINER_OF(osdep, struct mtlk_eq, osdep);

  mtlk_eq_handle_events(eq);
}

int __MTLK_IFUNC
_mtlk_eq_osdep_init (struct mtlk_eq_osdep *osdep)
{
  int res;

  INIT_WORK(&osdep->work, work_handler, osdep);
 
  res = mtlk_osal_timer_init(&osdep->timer, timer_handler, HANDLE_T(osdep));
  if (res != MTLK_ERR_OK) {
    ELOG("Failed to initialize timer with err code %i", res);
    goto err;
  }

err:
  return res;
}

int __MTLK_IFUNC
_mtlk_eq_osdep_start(struct mtlk_eq_osdep* osdep)
{
  int res;

  res = mtlk_osal_timer_set(&osdep->timer, AUTO_WAKE_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG("Failed to set timer with err code %i", res);
    goto err;
  }

err:
  return res;
}

void __MTLK_IFUNC
_mtlk_eq_osdep_stop(struct mtlk_eq_osdep *osdep)
{
  mtlk_osal_timer_cancel_sync(&osdep->timer);
}

void __MTLK_IFUNC
_mtlk_eq_osdep_cleanup (struct mtlk_eq_osdep *osdep)
{
  mtlk_osal_timer_cleanup(&osdep->timer);
}

void __MTLK_IFUNC
_mtlk_eq_osdep_notify (struct mtlk_eq_osdep *osdep)
{
  // work will not be scheduled by kernel if it's already sheduled, 
  // so no extra checks are required
  schedule_work(&osdep->work);
}

#endif

