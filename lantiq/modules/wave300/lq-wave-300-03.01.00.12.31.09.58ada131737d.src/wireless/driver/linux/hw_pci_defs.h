/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#if !defined (SAFE_PLACE_TO_INCLUDE_HW_PCI_DEFS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_HW_PCI_... */

#undef SAFE_PLACE_TO_INCLUDE_HW_PCI_DEFS

#include "mmb_ops.h" 
#include <linux/delay.h>

static __INLINE uint32 
mtlk_hw_bus_readl (mtlk_hw_bus_t       *bus,
                   const volatile void *addr)
{
  return mtlk_readl(addr);
}

static __INLINE void
mtlk_hw_bus_writel (mtlk_hw_bus_t *bus,
                    uint32         val,
                    volatile void *addr)
{
  mtlk_writel(val, addr);
}

static __INLINE uint32 
mtlk_hw_bus_raw_readl (mtlk_hw_bus_t       *bus,
                       const volatile void *addr)
{
  return mtlk_raw_readl(addr);
}

static __INLINE void
mtlk_hw_bus_raw_writel (mtlk_hw_bus_t *bus,
                        uint32         val,
                        volatile void *addr)
{
  mtlk_raw_writel(val, addr);
}

int         __MTLK_IFUNC mtlk_pci_get_file_buffer(mtlk_hw_bus_t          *bus,
                                                  const char             *name,
                                                  mtlk_mw_bus_file_buf_t *fb);
void        __MTLK_IFUNC mtlk_pci_release_file_buffer(mtlk_hw_bus_t          *bus,
                                                      mtlk_mw_bus_file_buf_t *fb);
const char* __MTLK_IFUNC mtlk_pci_dev_name(mtlk_hw_bus_t *bus);

static __INLINE int mtlk_hw_bus_get_file_buffer(mtlk_hw_bus_t          *bus,
                                                const char             *name,
                                                mtlk_mw_bus_file_buf_t *fb)
{
  return mtlk_pci_get_file_buffer(bus, name, fb);
}

static __INLINE void mtlk_hw_bus_release_file_buffer(mtlk_hw_bus_t          *bus,
                                                     mtlk_mw_bus_file_buf_t *fb)
{
  return mtlk_pci_release_file_buffer(bus, fb);
}

static __INLINE void
mtlk_hw_bus_udelay (mtlk_hw_bus_t *bus,
                    uint32         us)
{
  udelay(us);
}

static __INLINE const char* 
mtlk_hw_bus_dev_name (mtlk_hw_bus_t *bus)
{
  return mtlk_pci_dev_name(bus);
}

int __MTLK_IFUNC
mtlk_pci_handle_interrupt(mtlk_hw_pci_t *obj);

#define SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DEFS
#include "mtlk_ccr_defs.h"

