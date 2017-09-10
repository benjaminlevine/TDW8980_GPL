/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/* $Id$ */

#if !defined (SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DEFS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DEFS */

#undef SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DEFS

#include "mtlk_card_selector.h"

#ifndef MTCFG_CCR_DEBUG
#  define DPR0    LOG2
#  define DPR1    LOG3
#  define DPR2    LOG4
#  define DPR3    LOG5
#  define DPR4    LOG6
#else
#  define DPR0    LOG0
#  define DPR1    LOG0
#  define DPR2    LOG0
#  define DPR3    LOG0
#  define DPR4    LOG0
#endif

#define DPR_PFX "CCR: "

/* Helper BUS read/write macros */
#define __ccr_writel(bus, val, addr) for(;;) { \
    DPR4("write shram: 0x%X to " #addr " (0x%p)", (val), (addr)); \
    mtlk_hw_bus_writel((bus), (val), (addr)); \
    break; \
  }

#define __ccr_readl(bus, addr, val) for(;;) { \
    (val) = mtlk_hw_bus_readl((bus), (addr)); \
    DPR4("read shram: 0x%X from " #addr " (0x%p)", (val), (addr)); \
    break; \
  }

#define __ccr_setl(bus, addr, flag) for(;;) { \
    uint32 val; \
    __ccr_readl((bus), (addr), val); \
    __ccr_writel((bus), val | (flag), (addr)); \
    break; \
  }

#define __ccr_resetl(bus, addr, flag) for(;;) { \
    uint32 val; \
    __ccr_readl((bus), (addr), val); \
    __ccr_writel(bus, (val) & ~(flag), (addr)); \
    break; \
  }

#define __ccr_issetl(bus, addr, flag, res) for(;;) { \
    uint32 val; \
    __ccr_readl((bus), (addr), val); \
    (res) = ( (val & (flag)) == (flag) ); \
    break; \
  }

#define SAFE_PLACE_TO_INCLUDE_MTLK_PCI_CCR_DEFS
#include "mtlk_pcig2_ccr_defs.h"

#define SAFE_PLACE_TO_INCLUDE_MTLK_G3_CCR_DEFS
#include "mtlk_g3_ccr_defs.h"

#define SAFE_PLACE_TO_INCLUDE_MTLK_PCIG3_CCR_DEFS
#include "mtlk_pcig3_ccr_defs.h"

#define SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DEFS
#include "mtlk_pcie_ccr_defs.h"

static __INLINE int
_mtlk_sub_ccr_init(mtlk_ccr_t *ccr, mtlk_card_type_t hw_type, 
                   mtlk_hw_bus_t *bus, void* bar0, void* bar1)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( return _mtlk_pcig2_ccr_init(&ccr->mem.pcig2, bus, bar0, bar1) );
    IF_CARD_PCIG3 ( return _mtlk_pcig3_ccr_init(&ccr->mem.pcig3, bus, bar0, bar1) );
    IF_CARD_PCIE  ( return _mtlk_pcie_ccr_init(&ccr->mem.pcie, bus, bar0, bar1)   );
  CARD_SELECTOR_END();

  return MTLK_ERR_PARAMS;
}

static __INLINE void
_mtlk_sub_ccr_cleanup(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_ccr_cleanup(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_pcig3_ccr_cleanup(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_ccr_cleanup(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();
}

static __INLINE int
mtlk_ccr_handle_interrupt(mtlk_ccr_t *ccr);

#ifdef MTCFG_USE_INTERRUPT_POLLING

#define _MTLK_CCR_INT_POLL_PERIOD_MS (100)

static uint32 __MTLK_IFUNC
_mtlk_ccr_poll_interrupt (mtlk_osal_timer_t *timer, 
                          mtlk_handle_t      clb_usr_data)
{
  MTLK_UNREFERENCED_PARAM(timer);
  mtlk_ccr_handle_interrupt((mtlk_ccr_t *) clb_usr_data);
  return _MTLK_CCR_INT_POLL_PERIOD_MS;
}

#endif

MTLK_INIT_STEPS_LIST_BEGIN(ccr)
#ifdef MTCFG_USE_INTERRUPT_POLLING
  MTLK_INIT_STEPS_LIST_ENTRY(ccr, INT_POLL_TIMER)
#endif
  MTLK_INIT_STEPS_LIST_ENTRY(ccr, SUB_CCR)
MTLK_INIT_INNER_STEPS_BEGIN(ccr)
MTLK_INIT_STEPS_LIST_END(ccr);

static __INLINE void
mtlk_ccr_cleanup(mtlk_ccr_t *ccr)
{
  MTLK_CLEANUP_BEGIN(ccr, MTLK_OBJ_PTR(ccr))
    MTLK_CLEANUP_STEP(ccr, SUB_CCR, MTLK_OBJ_PTR(ccr),
                      _mtlk_sub_ccr_cleanup, (ccr))
#ifdef MTCFG_USE_INTERRUPT_POLLING
    MTLK_CLEANUP_STEP(ccr, INT_POLL_TIMER, MTLK_OBJ_PTR(ccr), 
                      mtlk_osal_timer_cleanup, (&ccr->poll_interrupts))
#endif
  MTLK_CLEANUP_END(ccr, MTLK_OBJ_PTR(ccr));
}

static __INLINE int
mtlk_ccr_init(mtlk_ccr_t *ccr, mtlk_card_type_t hw_type, 
              mtlk_hw_bus_t *bus, void* bar0, void* bar1)
{
  MTLK_INIT_TRY(ccr, MTLK_OBJ_PTR(ccr))
    ccr->hw = bus->mmb;
    ccr->hw_type = hw_type;

#ifdef MTCFG_USE_INTERRUPT_POLLING
    MTLK_INIT_STEP(ccr, INT_POLL_TIMER, MTLK_OBJ_PTR(ccr),
                   mtlk_osal_timer_init, 
                   (&ccr->poll_interrupts, _mtlk_ccr_poll_interrupt, HANDLE_T(ccr)))
#endif
    MTLK_INIT_STEP(ccr, SUB_CCR, MTLK_OBJ_PTR(ccr),
                   _mtlk_sub_ccr_init, (ccr, hw_type, bus, bar0, bar1))
  MTLK_INIT_FINALLY(ccr, MTLK_OBJ_PTR(ccr))
  MTLK_INIT_RETURN(ccr, MTLK_OBJ_PTR(ccr), mtlk_ccr_cleanup, (ccr));

}

#ifdef MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS
static __INLINE void
mtlk_ccr_read_hw_timestamp(mtlk_ccr_t *ccr, uint32 *low, uint32 *high)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_read_hw_timestamp(&ccr->mem.pcig2, low, high)   );
    IF_CARD_PCIG3 ( _mtlk_pcig3_read_hw_timestamp(&ccr->mem.pcig3, low, high) );
    IF_CARD_PCIE  ( _mtlk_pcie_read_hw_timestamp(&ccr->mem.pcie, low, high)   );
  CARD_SELECTOR_END();
}
#endif /* MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS */


static __INLINE void
mtlk_ccr_enable_interrupts(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_enable_interrupts(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_pcig3_enable_interrupts(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_enable_interrupts(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();

#ifdef MTCFG_USE_INTERRUPT_POLLING
  mtlk_osal_timer_set(&ccr->poll_interrupts, 
                      _MTLK_CCR_INT_POLL_PERIOD_MS);
#endif
}

static __INLINE void
mtlk_ccr_disable_interrupts(mtlk_ccr_t *ccr)
{
#ifdef MTCFG_USE_INTERRUPT_POLLING
  mtlk_osal_timer_cancel(&ccr->poll_interrupts);
#endif

  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_disable_interrupts(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_pcig3_disable_interrupts(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_disable_interrupts(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();
}

static __INLINE BOOL
mtlk_ccr_clear_interrupts_if_pending(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( return _mtlk_pcig2_clear_interrupts_if_pending(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( return _mtlk_pcig3_clear_interrupts_if_pending(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( return _mtlk_pcie_clear_interrupts_if_pending(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();

  MTLK_ASSERT(!"Should never be here");
  return 0;
}

static __INLINE BOOL
mtlk_ccr_disable_interrupts_if_pending(mtlk_ccr_t *ccr)
{
  BOOL res = FALSE;

  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( res = _mtlk_pcig2_disable_interrupts_if_pending(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( res = _mtlk_pcig3_disable_interrupts_if_pending(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( res = _mtlk_pcie_disable_interrupts_if_pending(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();

#ifdef MTCFG_USE_INTERRUPT_POLLING
  if(res)
    mtlk_osal_timer_cancel(&ccr->poll_interrupts);
#endif

  return res;
}

typedef struct
{
  mtlk_ccr_t *ccr;
  int result;
} ___ccr_his_ctx;

static void __MTLK_IFUNC
_mtlk_ccr_handle_interrupt_sync(void* data)
{
  ___ccr_his_ctx* ctx = (___ccr_his_ctx*) data;

  CARD_SELECTOR_START(ctx->ccr->hw_type)
    IF_CARD_PCIG2 ( ctx->result = _mtlk_pcig2_handle_interrupt(&ctx->ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( ctx->result = _mtlk_pcig3_handle_interrupt(&ctx->ccr->mem.pcig3) );
    IF_CARD_PCIE  ( ctx->result = _mtlk_pcie_handle_interrupt(&ctx->ccr->mem.pcie)   );
  CARD_SELECTOR_END();
}

static __INLINE int
mtlk_ccr_handle_interrupt(mtlk_ccr_t *ccr)
{
  ___ccr_his_ctx ctx;
  ctx.ccr = ccr;

  mtlk_mmb_sync_isr(ccr->hw, _mtlk_ccr_handle_interrupt_sync, &ctx);
  return ctx.result;
}

static __INLINE void
mtlk_ccr_initiate_doorbell_inerrupt(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_initiate_doorbell_inerrupt(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_pcig3_initiate_doorbell_inerrupt(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_initiate_doorbell_inerrupt(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_enable_xo(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_enable_xo(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_pcig3_enable_xo(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_enable_xo(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_disable_xo(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_disable_xo(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_pcig3_disable_xo(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_disable_xo(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_release_ctl_from_reset(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_release_ctl_from_reset(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_pcig3_release_ctl_from_reset(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_release_ctl_from_reset(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_put_ctl_to_reset(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_put_ctl_to_reset(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_g3_put_ctl_to_reset(ccr->mem.pcig3.bus, 
                                              ccr->mem.pcig3.pas) );
    IF_CARD_PCIE  ( _mtlk_g3_put_ctl_to_reset(ccr->mem.pcie.bus, 
                                              ccr->mem.pcie.pas)   );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_boot_from_bus(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_boot_from_bus(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_pcig3_boot_from_bus(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_boot_from_bus(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_clear_boot_from_bus(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_clear_boot_from_bus(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_pcig3_clear_boot_from_bus(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_clear_boot_from_bus(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_switch_to_iram_boot(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_switch_to_iram_boot(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_pcig3_switch_to_iram_boot(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_switch_to_iram_boot(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_exit_debug_mode(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_exit_debug_mode(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_pcig3_exit_debug_mode(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_exit_debug_mode(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_switch_clock(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_switch_clock(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_pcig3_switch_clock(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_switch_clock(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_unswitch_clock(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_unswitch_clock(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_pcig3_unswitch_clock(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_unswitch_clock(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_put_cpus_to_reset(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_put_cpus_to_reset(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_pcig3_put_cpus_to_reset(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_put_cpus_to_reset(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_power_on_cpus(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_power_on_cpus(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_pcig3_power_on_cpus(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_power_on_cpus(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_release_ucpu_reset(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_release_ucpu_reset(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_pcig3_release_ucpu_reset(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_release_ucpu_reset(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_release_lcpu_reset(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_release_lcpu_reset(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_pcig3_release_lcpu_reset(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_release_lcpu_reset(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_release_cpus_reset(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( _mtlk_pcig2_release_cpus_reset(&ccr->mem.pcig2) );
    IF_CARD_PCIG3 ( _mtlk_pcig3_release_cpus_reset(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_release_cpus_reset(&ccr->mem.pcie)   );
  CARD_SELECTOR_END();
}

static __INLINE BOOL
mtlk_ccr_check_bist(mtlk_ccr_t *ccr, uint32 *bist_result)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG2 ( return _mtlk_pcig2_check_bist(&ccr->mem.pcig2, bist_result) );
    IF_CARD_PCIG3 ( return _mtlk_pcig3_check_bist(&ccr->mem.pcig3, bist_result) );
    IF_CARD_PCIE  ( return _mtlk_pcie_check_bist(&ccr->mem.pcie, bist_result)   );
  CARD_SELECTOR_END();

  MTLK_ASSERT(!"Should never be here");
  return MTLK_ERR_PARAMS;
}

#undef DPR0
#undef DPR1
#undef DPR2
#undef DPR3
#undef DPR4
#undef DPR_PFX
