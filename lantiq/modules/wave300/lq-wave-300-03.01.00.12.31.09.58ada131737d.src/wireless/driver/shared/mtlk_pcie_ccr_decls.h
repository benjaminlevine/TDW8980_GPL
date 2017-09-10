/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/* $Id$ */

#if !defined(SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DECLS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DECLS */
#undef SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DECLS

#include "g3shram_ex.h"

typedef struct
{
  struct g3_pas_map         *pas;
  mtlk_hw_bus_t             *bus;
  uint8                      current_ucpu_state;
  uint8                      current_lcpu_state;
  uint8                      next_boot_mode;

  volatile BOOL              irqs_enabled;
  volatile BOOL              irq_pending;

  MTLK_DECLARE_INIT_STATUS;
} _mtlk_pcie_ccr_t;

#define G3PCIE_CPU_Control_BIST_Passed    ((1 << 31) | (1 << 15)) 
