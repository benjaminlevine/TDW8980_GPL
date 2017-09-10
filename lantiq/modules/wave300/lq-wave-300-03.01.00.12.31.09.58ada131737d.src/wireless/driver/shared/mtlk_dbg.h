/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __MTLK_DBG_H__
#define __MTLK_DBG_H__

/************************************************************************
 * WARNING: this file contains CP debug related definitions.
 * WARNING: this file is not a part of release driver.
 * WARNING: These definitions may or may not exist on each 
 *          particular platform - so DO NOT rely on them.
 ************************************************************************/

#ifndef MTCFG_BENCHMARK_TOOLS
#error This file could be only used for benchmarks!
#endif

#include "mtlk_dbg_osdep.h"

static __INLINE int    mtlk_dbg_init(void);
static __INLINE void   mtlk_dbg_cleanup(void);

/* NOTE: All the MTLK_DBG_HRES_TS...US macros return UINT64 values */

#define mtlk_dbg_hres_ts_wraparound_period_us() \
  ((uint64)MTLK_DBG_HRES_TS_WRAPAROUND_PERIOD_US())

#define mtlk_dbg_hres_ts(var)                   \
  MTLK_DBG_HRES_TS((mtlk_dbg_hres_ts_t)(var))

#define mtlk_dbg_hres_diff_to_us(ts_later, ts_earlier) \
  ((uint64)MTLK_DBG_HRES_DIFF_TO_US(ts_later, ts_earlier))

#endif /* __MTLK_DBG_H__ */
