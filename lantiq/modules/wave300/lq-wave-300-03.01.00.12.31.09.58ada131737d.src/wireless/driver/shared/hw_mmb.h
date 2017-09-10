/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __HW_MMB_H__
#define __HW_MMB_H__

#ifdef MTCFG_HW_PCI
#define SAFE_PLACE_TO_INCLUDE_HW_PCI_DECLS
#include "hw_pci_decls.h"
#else
#error Wrong platform!
#endif 

#include "shram_ex.h"

#include "mtlk_osal.h"
#include "mtlkhal.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

#define MTLK_FRMW_UPPER_AP_NAME   "ap_upper.bin"
#define MTLK_FRMW_UPPER_STA_NAME  "sta_upper.bin"
#define MTLK_FRMW_LOWER_NAME      "contr_lm.bin"

/**************************************************************
 * Bus abstraction - uint32 Bus RAW access functions
 * NOTE: must be defined within hw_<platfrorm>.h
 **************************************************************/
static __INLINE uint32 mtlk_hw_bus_readl(mtlk_hw_bus_t       *bus,
                                         const volatile void *addr);
static __INLINE void   mtlk_hw_bus_writel(mtlk_hw_bus_t *bus,
                                          uint32         val,
                                          volatile void *addr);
static __INLINE uint32 mtlk_hw_bus_raw_readl(mtlk_hw_bus_t       *bus,
                                             const volatile void *addr);
static __INLINE void   mtlk_hw_bus_raw_writel(mtlk_hw_bus_t *bus,
                                              uint32         val,
                                              volatile void *addr);
/**************************************************************/

/**************************************************************
 * Auxilliary OS/bus abstractions required for MMB
 * NOTE: must be defined within hw_<platfrorm>.h
 **************************************************************/
typedef struct
{
  const uint8  *buffer;
  uint32        size;
  mtlk_handle_t context; /* for mtlk_hw_bus_t's usage. MMB does not use it. */
} __MTLK_IDATA mtlk_mw_bus_file_buf_t;

static __INLINE int mtlk_hw_bus_get_file_buffer(mtlk_hw_bus_t          *bus,
                                                const char             *name,
                                                mtlk_mw_bus_file_buf_t *fb);
static __INLINE void mtlk_hw_bus_release_file_buffer(mtlk_hw_bus_t          *bus,
                                                     mtlk_mw_bus_file_buf_t *fb);

static __INLINE void mtlk_hw_bus_udelay(mtlk_hw_bus_t *bus,
                                        uint32         us);

static __INLINE const char* mtlk_hw_bus_dev_name(mtlk_hw_bus_t *bus);

typedef void (__MTLK_IFUNC *mtlk_hw_bus_sync_f)(void *context);

void mtlk_mmb_sync_isr(mtlk_hw_t         *hw, 
                       mtlk_hw_bus_sync_f func,
                       void              *context);
/**************************************************************/

/**************************************************************
 * MMB Inreface
 **************************************************************/

typedef struct
{
  mtlk_ccr_t            *ccr;
  unsigned char         *pas;
  mtlk_hw_bus_t         *bus;
  mtlk_df_t             *df;
  uint8                  ap;
  uint32                 max_no_tx_cfm_sec;
} __MTLK_IDATA mtlk_hw_mmb_card_cfg_t;

typedef struct
{
  uint8  bist_check_permitted;
  uint32 no_pll_write_delay_us;
} __MTLK_IDATA mtlk_hw_mmb_cfg_t;

typedef struct
{
  mtlk_hw_mmb_cfg_t    cfg;
  mtlk_hw_t           *cards[MTLK_HW_MAX_CARDS];
  uint32               nof_cards;
  mtlk_osal_spinlock_t lock;
  uint32               bist_passed;

  MTLK_DECLARE_INIT_STATUS;
} __MTLK_IDATA mtlk_hw_mmb_t;

/**************************************************************
 * Init/cleanup functions - must be called on driver's
 * loading/unloading
 **************************************************************/
int __MTLK_IFUNC 
mtlk_hw_mmb_init(mtlk_hw_mmb_t *mmb, const mtlk_hw_mmb_cfg_t *cfg);
void __MTLK_IFUNC
mtlk_hw_mmb_cleanup(mtlk_hw_mmb_t *mmb);
/**************************************************************/

/**************************************************************
 * Auxilliary MMB interface - for BUS module usage
 **************************************************************/
uint32 __MTLK_IFUNC
mtlk_hw_mmb_get_cards_no(mtlk_hw_mmb_t *mmb);
mtlk_txmm_t *__MTLK_IFUNC
mtlk_hw_mmb_get_txmm(mtlk_hw_t *card);
mtlk_txmm_t *__MTLK_IFUNC
mtlk_hw_mmb_get_txdm(mtlk_hw_t *card);

/* Stops all the MAC-initiated events (INDs), sending to MAC still working */
void __MTLK_IFUNC
mtlk_hw_mmb_stop_mac_events(mtlk_hw_t *card);
/**************************************************************/

/**************************************************************
 * Add/remove card - must be called on device addition/removal
 **************************************************************/
mtlk_hw_t * __MTLK_IFUNC 
mtlk_hw_mmb_add_card(mtlk_hw_mmb_t                *mmb,
                     const mtlk_hw_mmb_card_cfg_t *card_cfg);
void __MTLK_IFUNC 
mtlk_hw_mmb_remove_card(mtlk_hw_mmb_t *mmb,
                        mtlk_hw_t     *card);

/**************************************************************
 * Init/cleanup card - must be called on device init/cleanup
 **************************************************************/
int __MTLK_IFUNC 
mtlk_hw_mmb_init_card(mtlk_hw_t   *card,
                      mtlk_core_t *core,
                      mtlk_ccr_t *ccr);
void __MTLK_IFUNC 
mtlk_hw_mmb_cleanup_card(mtlk_hw_t *card);

int __MTLK_IFUNC 
mtlk_hw_mmb_start_card(mtlk_hw_t   *hw);

void __MTLK_IFUNC 
mtlk_hw_mmb_stop_card(mtlk_hw_t *card);
/**************************************************************/

/**************************************************************
 * Card's ISR - must be called on interrupt handler
 * Return values:
 *   MTLK_ERR_OK      - do nothing
 *   MTLK_ERR_UNKNOWN - not an our interrupt
 *   MTLK_ERR_PENDING - order bottom half routine (DPC, tasklet etc.)
 **************************************************************/
int __MTLK_IFUNC 
mtlk_hw_mmb_interrupt_handler(mtlk_hw_t *card);
/**************************************************************/

/**************************************************************
 * Card's bottom half of irq handling (DPC, tasklet etc.)
 **************************************************************/
void __MTLK_IFUNC 
mtlk_hw_mmb_deferred_handler(mtlk_hw_t *card);
/**************************************************************/
/**************************************************************/

#ifdef MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS
uint32 __MTLK_IFUNC
mtlk_hw_get_tsf_card_time_stamp(void);
#endif /* MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS */

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#ifdef MTCFG_HW_PCI
#define SAFE_PLACE_TO_INCLUDE_HW_PCI_DEFS
#include "hw_pci_defs.h"
#else
#error Wrong platform!
#endif 

#endif /* __HW_MMB_H__ */
