/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/* $Id$ */
#if !defined (SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DECLS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DECLS */

#undef SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DECLS

#define SAFE_PLACE_TO_INCLUDE_MTLK_PCI_CCR_DECLS
#include "mtlk_pcig2_ccr_decls.h"

#define SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DECLS
#include "mtlk_pcie_ccr_decls.h"

#define SAFE_PLACE_TO_INCLUDE_MTLK_PCIG3_CCR_DECLS
#include "mtlk_pcig3_ccr_decls.h"

typedef struct _mtlk_ccr_t
{
  union 
  {
    _mtlk_pcig2_ccr_t  pcig2;
    _mtlk_pcig3_ccr_t  pcig3;
    _mtlk_pcie_ccr_t   pcie;
  } mem;

  mtlk_card_type_t     hw_type;

#ifdef MTCFG_USE_INTERRUPT_POLLING
  mtlk_osal_timer_t    poll_interrupts;
#endif

  mtlk_hw_t           *hw;

  MTLK_DECLARE_INIT_STATUS;
} mtlk_ccr_t;
