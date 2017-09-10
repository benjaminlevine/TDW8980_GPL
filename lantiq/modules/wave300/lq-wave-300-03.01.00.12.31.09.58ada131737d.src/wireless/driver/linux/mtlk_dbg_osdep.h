/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __MTLK_DBG_OSDEP_H__
#define __MTLK_DBG_OSDEP_H__

#ifndef MTCFG_BENCHMARK_TOOLS
#error This file could be only used for benchmarks!
#endif

#if defined (CONFIG_ARCH_STR9100) /* Star boards */

#include <linux/time.h>

extern u32 APB_clock;

/************************************************************
 * These definition are taken from STAR 9100 kernel:
 * arch/arm/mach-str9100/str9100_timer.c
 ************************************************************/
#define uSECS_PER_TICK       (1000000 / APB_clock)
#define TICKS_PER_uSEC       (APB_clock / 1000000)
/************************************************************/

void str9100_counter_on_init( void );
void str9100_counter_on_cleanup( void );

typedef uint32 mtlk_dbg_hres_ts_t;

static __INLINE int
mtlk_dbg_init (void)
{
  str9100_counter_on_init();
  return MTLK_ERR_OK;
}

static __INLINE void
mtlk_dbg_cleanup (void)
{
  str9100_counter_on_cleanup();
}

#define MTLK_DBG_HRES_TS_WRAPAROUND_PERIOD_US() \
  (((mtlk_dbg_hres_ts_t)-1)/((mtlk_dbg_hres_ts_t)TICKS_PER_uSEC))

#define MTLK_DBG_HRES_TS(ts)                        \
  do {                                              \
    (ts) = (mtlk_dbg_hres_ts_t)TIMER2_COUNTER_REG;  \
  } while (0)

#define MTLK_DBG_HRES_DIFF_TO_US(ts_later, ts_earlier) \
  ( ((ts_later) - (ts_earlier))/(TICKS_PER_uSEC) )

#else /* Using Linux default - do_gettimeofday() - precision is NOT GUARANTEED! */

#include <linux/time.h>

static __INLINE int
mtlk_dbg_init (void)
{
  return MTLK_ERR_OK;
}

static __INLINE void
mtlk_dbg_cleanup (void)
{

}

typedef struct timeval mtlk_dbg_hres_ts_t;

#define MTLK_DBG_HRES_TS_WRAPAROUND_PERIOD_US() \
  (-1) /* wraparound is handled by do_gettimeofday() API internally */

#define MTLK_DBG_HRES_TS(ts)                    \
  do_gettimeofday(&(ts))

#define __MTLK_DBG_HRES_TS_TO_US(ts)            \
  ((((uint64)(ts).tv_sec) * USEC_PER_SEC) + (ts).tv_usec)

#define MTLK_DBG_HRES_DIFF_TO_US(ts_later, ts_earlier) \
  (__MTLK_DBG_HRES_TS_TO_US(ts_later) - __MTLK_DBG_HRES_TS_TO_US(ts_earlier))

#endif 

#endif /* __MTLK_DBG_OSDEP_H__ */

