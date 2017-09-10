/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#if !defined (SAFE_PLACE_TO_INCLUDE_HW_PCI_DECLS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_HW_PCI_... */

#undef SAFE_PLACE_TO_INCLUDE_HW_PCI_DECLS


typedef struct _mtlk_hw_pci_t mtlk_hw_pci_t;         /* for hw_pci.c      */
typedef struct _mtlk_hw_pci_t mtlk_hw_bus_t;         /* for hw_mmb.h/.c   */

typedef enum
{
  MTLK_CARD_UNKNOWN = 0x8888, /* Just magics to avoid    */
  MTLK_CARD_FIRST = 0xABCD,   /* accidental coincidences */

#ifdef MTCFG_LINDRV_HW_PCIG2
  MTLK_CARD_PCIG2,
#endif
#ifdef MTCFG_LINDRV_HW_PCIE
  MTLK_CARD_PCIE,
#endif
#ifdef MTCFG_LINDRV_HW_PCIG3
  MTLK_CARD_PCIG3,
#endif

  MTLK_CARD_LAST
} mtlk_card_type_t;

#define SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DECLS
#include "mtlk_ccr_decls.h"

struct _mtlk_hw_pci_t
{
  struct pci_dev       *dev;
  /* in struct pci_dev member irq has unsigned int type,
   * we need it to be signed for -1 (was not requested)
   */
  unsigned char        *bar0;
  unsigned char        *bar1;
  mtlk_hw_t            *mmb;
  mtlk_core_t          *core;
  struct tasklet_struct mmb_tasklet;
  mtlk_ccr_t            ccr;
  mtlk_card_type_t      card_type;

  MTLK_DECLARE_INIT_STATUS;
};

#define MTLK_HW_MAX_CARDS  10
