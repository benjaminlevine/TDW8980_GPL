/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/* $Id$ */

#if !defined(SAFE_PLACE_TO_INCLUDE_MTLK_PCI_CCR_DECLS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_MTLK_PCI_CCR_DECLS */
#undef SAFE_PLACE_TO_INCLUDE_MTLK_PCI_CCR_DECLS

#include "shram_ex.h"
#include "pcishram_ex.h"

#define BIST_DONE_MASK                  0x4
#define BIST_RESULT_MASK                0x2

typedef struct
{
  struct pci_hrc_regs       *hrc;
  mtlk_hw_bus_t             *bus;
} _mtlk_pcig2_ccr_t;
