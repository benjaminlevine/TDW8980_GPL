/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"
#include "mtlk_irb_pinger.h"
#include "mtlkirb.h"
#include "dataex.h"

#ifdef MTCFG_IRB_BENCHMARKING
#include "mtlk_dbg.h"

typedef mtlk_dbg_hres_ts_t    mtlk_irb_pinger_ts_t;

#define mtlk_irb_pinger_ts(var)                 \
  mtlk_dbg_hres_ts(var)

#define mtlk_irb_pinger_diff_to_us(ts_later, ts_earlier) \
  mtlk_dbg_hres_diff_to_us(ts_later, ts_earlier)

#else
typedef mtlk_osal_timestamp_t mtlk_irb_pinger_ts_t;

#define mtlk_irb_pinger_ts(var)                 \
  do {                                          \
    (var) = mtlk_osal_timestamp();              \
  } while (0)

#define mtlk_irb_pinger_diff_to_us(ts_later, ts_earlier) \
  ( (mtlk_osal_timestamp_to_ms(ts_later) -               \
         mtlk_osal_timestamp_to_ms(ts_earlier)) * 1000 )

#endif

struct mtlk_irb_ping_data
{
  uint32               id;
  mtlk_irb_pinger_ts_t ts;
};

MTLK_INIT_STEPS_LIST_BEGIN(irb_pinger)
  MTLK_INIT_STEPS_LIST_ENTRY(irb_pinger, PINGER_TIMER_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(irb_pinger, PINGER_LOCK_INIT)
MTLK_INIT_INNER_STEPS_BEGIN(irb_pinger)
MTLK_INIT_STEPS_LIST_END(irb_pinger);

MTLK_START_STEPS_LIST_BEGIN(irb_pinger)
  MTLK_START_STEPS_LIST_ENTRY(irb_pinger, PINGER_IRB_REGISTER)
  MTLK_START_STEPS_LIST_ENTRY(irb_pinger, PINGER_TIMER_START)
MTLK_START_INNER_STEPS_BEGIN(irb_pinger)
MTLK_START_STEPS_LIST_END(irb_pinger);

const static mtlk_guid_t IRBE_PING = MTLK_IRB_GUID_PING;
const static mtlk_guid_t IRBE_PONG = MTLK_IRB_GUID_PONG;

static uint32 __MTLK_IFUNC
_mtlk_irb_pinger_do_ping (mtlk_osal_timer_t *timer, 
                          mtlk_handle_t      clb_usr_data)
{
  mtlk_irb_pinger_t        *pinger = HANDLE_T_PTR(mtlk_irb_pinger_t, clb_usr_data);
  struct mtlk_irb_ping_data ping_data;

  MTLK_UNREFERENCED_PARAM(timer);

  mtlk_osal_lock_acquire(&pinger->lock);

  ping_data.id = pinger->id;
  mtlk_irb_pinger_ts(ping_data.ts);

  mtlk_irb_notify_app(&IRBE_PING, &ping_data, sizeof(ping_data));

  ++pinger->id;

  mtlk_osal_lock_release(&pinger->lock);

  ILOG1(GID_IRB, "IRB PING #%d sent", ping_data.id);

  return pinger->ping_period_ms;
}

static void __MTLK_IFUNC
_mtlk_irb_pinger_handle_pong (mtlk_handle_t      context,
                              const mtlk_guid_t *evt,
                              void              *buffer,
                              uint32            *size)
{
  mtlk_irb_pinger_t         *pinger    = HANDLE_T_PTR(mtlk_irb_pinger_t, context);
  struct mtlk_irb_ping_data *ping_data = (struct mtlk_irb_ping_data *)buffer;
  mtlk_irb_pinger_ts_t       now_ts;
  uint64                     delay_us;

  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_PONG) == 0);
  MTLK_ASSERT(sizeof(*ping_data) == *size);

  mtlk_irb_pinger_ts(now_ts);

  mtlk_osal_lock_acquire(&pinger->lock);

  /* This will add 0 if there are no holes in pong */
  pinger->stats.nof_missed_pongs += ping_data->id - (pinger->last_pong_id + 1);

  if (ping_data->id < pinger->last_pong_id) {
    ++pinger->stats.nof_ooo_pongs;
    WLOG("Out-of-order Pong#%u received (last=%u)", ping_data->id, pinger->last_pong_id);
  }

  delay_us = mtlk_irb_pinger_diff_to_us(now_ts, ping_data->ts);

  pinger->stats.all_delay_us += delay_us;
  ++pinger->stats.nof_recvd_pongs;
  if (pinger->stats.peak_delay_us < delay_us) {
    pinger->stats.peak_delay_us = delay_us;
  }

  pinger->last_pong_id = ping_data->id;

  mtlk_osal_lock_release(&pinger->lock);

  ILOG1(GID_IRB, "IRB PONG #%d received", ping_data->id);
}

mtlk_irb_pinger_t * __MTLK_IFUNC
mtlk_irb_pinger_alloc (void)
{
  return (mtlk_irb_pinger_t *)mtlk_osal_mem_alloc(sizeof(mtlk_irb_pinger_t),
                                                  MTLK_MEM_TAG_IRBPINGER);
}

int __MTLK_IFUNC
mtlk_irb_pinger_init (mtlk_irb_pinger_t *pinger)
{
  MTLK_ASSERT(pinger != NULL);

  memset(pinger, 0, sizeof(*pinger));

  MTLK_INIT_TRY(irb_pinger, MTLK_OBJ_PTR(pinger))
    MTLK_INIT_STEP(irb_pinger, PINGER_TIMER_INIT, MTLK_OBJ_PTR(pinger),
                   mtlk_osal_timer_init, (&pinger->timer, _mtlk_irb_pinger_do_ping, HANDLE_T(pinger)));
    MTLK_INIT_STEP(irb_pinger, PINGER_LOCK_INIT, MTLK_OBJ_PTR(pinger),
                   mtlk_osal_lock_init, (&pinger->lock));
  MTLK_INIT_FINALLY(irb_pinger, MTLK_OBJ_PTR(pinger))
  MTLK_INIT_RETURN(irb_pinger, MTLK_OBJ_PTR(pinger), mtlk_irb_pinger_cleanup, (pinger));
}

int __MTLK_IFUNC
mtlk_irb_pinger_start (mtlk_irb_pinger_t *pinger, uint32 ping_period_ms)
{
  BOOL started = FALSE;

  MTLK_ASSERT(pinger != NULL);
  MTLK_ASSERT(ping_period_ms != 0);

  pinger->ping_period_ms = ping_period_ms;
  pinger->id             = 1;
  pinger->last_pong_id   = 0;

  MTLK_START_TRY(irb_pinger, MTLK_OBJ_PTR(pinger))
    MTLK_START_STEP_EX(irb_pinger, PINGER_IRB_REGISTER, MTLK_OBJ_PTR(pinger),
                       mtlk_irb_register, (&IRBE_PONG, 1, _mtlk_irb_pinger_handle_pong, HANDLE_T(pinger)),
                       pinger->irb_handle, pinger->irb_handle != 0, MTLK_ERR_UNKNOWN);
    MTLK_START_STEP(irb_pinger, PINGER_TIMER_START, MTLK_OBJ_PTR(pinger),
                    mtlk_osal_timer_set, (&pinger->timer, pinger->ping_period_ms));
    started = TRUE;
  MTLK_START_FINALLY(irb_pinger, MTLK_OBJ_PTR(pinger))
    if (!started) {
      pinger->ping_period_ms = 0;
    }
  MTLK_START_RETURN(irb_pinger, MTLK_OBJ_PTR(pinger), mtlk_irb_pinger_stop, (pinger));
}

void __MTLK_IFUNC
mtlk_irb_pinger_stop (mtlk_irb_pinger_t *pinger)
{
  MTLK_ASSERT(pinger != NULL);
  MTLK_ASSERT(pinger->ping_period_ms != 0);

  pinger->ping_period_ms = 0;

  MTLK_STOP_BEGIN(irb_pinger, MTLK_OBJ_PTR(pinger))
    MTLK_STOP_STEP(irb_pinger, PINGER_TIMER_START, MTLK_OBJ_PTR(pinger),
                   mtlk_osal_timer_cancel_sync, (&pinger->timer));
    MTLK_STOP_STEP(irb_pinger, PINGER_IRB_REGISTER, MTLK_OBJ_PTR(pinger),
                   mtlk_irb_unregister, (pinger->irb_handle));
  MTLK_STOP_END(irb_pinger, MTLK_OBJ_PTR(pinger))
}

void __MTLK_IFUNC
mtlk_irb_pinger_cleanup (mtlk_irb_pinger_t *pinger)
{
  MTLK_ASSERT(pinger != NULL);

  MTLK_CLEANUP_BEGIN(irb_pinger, MTLK_OBJ_PTR(pinger))
    MTLK_CLEANUP_STEP(irb_pinger, PINGER_LOCK_INIT, MTLK_OBJ_PTR(pinger),
                      mtlk_osal_lock_cleanup, (&pinger->lock));
    MTLK_CLEANUP_STEP(irb_pinger, PINGER_TIMER_INIT, MTLK_OBJ_PTR(pinger),
                      mtlk_osal_timer_cleanup, (&pinger->timer));
  MTLK_CLEANUP_END(irb_pinger, MTLK_OBJ_PTR(pinger))
}

void __MTLK_IFUNC
mtlk_irb_pinger_free (mtlk_irb_pinger_t *pinger)
{
  mtlk_osal_mem_free(pinger);
}

BOOL __MTLK_IFUNC
mtlk_irb_pinger_is_started (mtlk_irb_pinger_t *pinger)
{
  return (pinger->ping_period_ms != 0);
}

uint32 __MTLK_IFUNC
mtlk_irb_pinger_get_ping_period_ms (mtlk_irb_pinger_t *pinger)
{
  return pinger->ping_period_ms;
}


void __MTLK_IFUNC
mtlk_irb_pinger_get_stats (mtlk_irb_pinger_t *pinger, struct mtlk_irb_pinger_stats *stats)
{
  MTLK_ASSERT(pinger != NULL);
  MTLK_ASSERT(stats != NULL);

  mtlk_osal_lock_acquire(&pinger->lock);
  memcpy(stats, &pinger->stats, sizeof(pinger->stats));
  mtlk_osal_lock_release(&pinger->lock);
}

void __MTLK_IFUNC
mtlk_irb_pinger_zero_stats (mtlk_irb_pinger_t *pinger)
{
  MTLK_ASSERT(pinger != NULL);

  mtlk_osal_lock_acquire(&pinger->lock);
  memset(&pinger->stats, 0, sizeof(pinger->stats));
  mtlk_osal_lock_release(&pinger->lock);
}
