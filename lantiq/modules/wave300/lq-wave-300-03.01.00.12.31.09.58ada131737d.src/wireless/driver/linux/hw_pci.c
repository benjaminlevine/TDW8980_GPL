/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"
#include "hw_mmb.h"
#include "drvver.h"
#include "nlmsgs.h"
#include "mem_leak.h"
#include "mtlkirb.h"
#include "mtlkirbm_k.h"
#include "dataex.h"

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#ifdef MTCFG_BENCHMARK_TOOLS
#include "mtlk_dbg.h"
#endif

#if defined(CONFIG_ARCH_STR9100) && defined(CONFIG_CPU_DSPAD_ENABLE)
#include <linux/str9100/str9100_dspad.h>
#endif

#define MTLK_VENDOR_ID               0x1a30
#define HYPERION_I_PCI_DEVICE_ID     0x0600
#define HYPERION_II_PCI_DEVICE_ID_A1 0x0680
#define HYPERION_II_PCI_DEVICE_ID_A2 0x0681
#define HYPERION_III_PCI_DEVICE      0x0700
#define HYPERION_III_PCIE_DEVICE     0x0710

#define MTLK_DEF_HW_NO_TX_CFM_SEC    60 /* sec */

/* we only support 32-bit addresses */
#define PCI_SUPPORTED_DMA_MASK       0xffffffff

MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_AUTHOR(DRV_COPYRIGHT);
MODULE_LICENSE("Proprietary");

static int ap[MTLK_HW_MAX_CARDS] = {0};
int        debug                 = 0;
#ifdef MTLK_DEBUG
int       step_to_fail           = 0;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
MODULE_PARM(ap, "1-" __MODULE_STRING(MTLK_HW_MAX_CARDS) "i");
MODULE_PARM(debug, "i");
#ifdef MTLK_DEBUG
MODULE_PARM(step_to_fail, "i");
#endif
#else
module_param_array(ap, int, NULL, 0);
module_param(debug, int, 0);
#ifdef MTLK_DEBUG
module_param(step_to_fail, int, 0);
#endif
#endif

MODULE_PARM_DESC(ap, "Make an access point");
MODULE_PARM_DESC(debug, "Debug level");
#ifdef MTLK_DEBUG
MODULE_PARM_DESC(step_to_fail, "Init step to simulate fail");
#endif

static const struct firmware *
_mtlk_pci_request_firmware(mtlk_hw_pci_t *obj, const char *fname)
{
  const struct firmware *fw_entry = NULL;
  int result = 0;
  int try = 0;

  /* on kernels 2.6 it could be that request_firmware returns -EEXIST
     it means that we tried to load firmware file before this time
     and kernel still didn't close sysfs entries it uses for download
     (see hotplug for details). In order to avoid such problems we
     will try number of times to load FW */
try_load_again:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) 
  result = request_firmware(&fw_entry, fname, pci_name(obj->dev));
#else
  result = request_firmware(&fw_entry, fname, &obj->dev->dev);
#endif
  if (result == -EEXIST) {
    try++;
    if (try < 10) {
      msleep(10);
      goto try_load_again;
    }
  }
  if (result != 0) 
  {
    ELOG("Firmware (%s) is missing",
         fname);
    fw_entry = NULL;
  }
  else
  {
    ILOG3(GID_HW_PCI, "%s firmware: size=0x%x, data=0x%p",
         fname, (unsigned int)fw_entry->size, fw_entry->data);
  }

  return fw_entry;
}

/**************************************************************
 * Auxilliary OS/bus abstractions required for MMB
 **************************************************************/
int __MTLK_IFUNC
mtlk_pci_get_file_buffer (mtlk_hw_bus_t          *bus,
                          const char             *name,
                          mtlk_mw_bus_file_buf_t *fb)
{
  int                    res      = MTLK_ERR_UNKNOWN;
  mtlk_hw_pci_t         *obj      = (mtlk_hw_pci_t *)bus;
  const struct firmware *fw_entry = _mtlk_pci_request_firmware(obj, name);

  if (fw_entry) {
    fb->buffer  = fw_entry->data;
    fb->size    = fw_entry->size;
    fb->context = HANDLE_T(fw_entry);
    res         = MTLK_ERR_OK;
  }

  return res;
}

void __MTLK_IFUNC
mtlk_pci_release_file_buffer (mtlk_hw_bus_t          *bus,
                              mtlk_mw_bus_file_buf_t *fb)
{
  const struct firmware *fw_entry = (const struct firmware *)fb->context;

  release_firmware(fw_entry);
}

const char* __MTLK_IFUNC
mtlk_pci_dev_name(mtlk_hw_bus_t *bus)
{
  return pci_name(bus->dev);
}
/**************************************************************/

static mtlk_hw_mmb_t mtlk_pci_mmb;

static void
_mtlk_pci_tasklet(unsigned long param) // bottom half of PCI irq handling
{
  mtlk_hw_pci_t *obj = (mtlk_hw_pci_t *)param;
  mtlk_hw_mmb_deferred_handler(obj->mmb);
}

int __MTLK_IFUNC
mtlk_pci_handle_interrupt(mtlk_hw_pci_t *obj)
{
  int res;
  
  CPU_STAT_BEGIN_TRACK(CPU_STAT_ID_ISR);
  res = mtlk_hw_mmb_interrupt_handler(obj->mmb);
  CPU_STAT_END_TRACK(CPU_STAT_ID_ISR);

  if (res == MTLK_ERR_OK)
    return MTLK_ERR_OK;
  else if (res == MTLK_ERR_PENDING) {
    tasklet_schedule(&obj->mmb_tasklet);
    return MTLK_ERR_OK;
  }

  return MTLK_ERR_NOT_SUPPORTED;
}

static irqreturn_t
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
_mtlk_pci_interrupt_handler(int irq, void *ptr, struct pt_regs *regs)
#else
_mtlk_pci_interrupt_handler(int irq, void *ptr)
#endif
{
  /* Multiple returns are used to support void return */
  mtlk_hw_pci_t *obj = (mtlk_hw_pci_t *)ptr;
  MTLK_UNREFERENCED_PARAM(irq);

  if(MTLK_ERR_OK == mtlk_ccr_handle_interrupt(&obj->ccr))
    return IRQ_HANDLED;
  else
    return IRQ_NONE;
}

static void
_mtlk_pci_clear_interrupts(mtlk_hw_pci_t *obj, struct pci_dev *dev)
{
#ifdef MTCFG_LINDRV_HW_PCIE
  if(MTLK_CARD_PCIE == obj->card_type)
  {
    pci_disable_msi(dev);
  }
#endif
}

MTLK_INIT_STEPS_LIST_BEGIN(hw_pci)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_pci, PCI_ENABLE_DEVICE)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_pci, PCI_SET_DMA_MASK)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_pci, PCI_MAP_BAR0)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_pci, PCI_MAP_BAR1)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_pci, PCI_SETUP_INTERRUPTS)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_pci, PCI_MMB_ADD_CARD)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_pci, PCI_CREATE_CCR)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_pci, PCI_CREATE_CORE)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_pci, PCI_MMB_INIT_CARD)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_pci, PCI_REQUEST_IRQ)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_pci, PCI_MMB_START_CARD)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_pci, PCI_START_CORE)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_pci, PCI_START_MAC_EVENTS)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_pci, PCI_CORE_PREPARE_STOP)
MTLK_INIT_INNER_STEPS_BEGIN(hw_pci)
MTLK_INIT_STEPS_LIST_END(hw_pci);

static void
_mtlk_pci_cleanup(mtlk_hw_pci_t *obj)
{
  MTLK_CLEANUP_BEGIN(hw_pci, MTLK_OBJ_PTR(obj))
    MTLK_CLEANUP_STEP(hw_pci, PCI_CORE_PREPARE_STOP, MTLK_OBJ_PTR(obj),
                      mtlk_core_prepare_stop, (obj->core));
    MTLK_CLEANUP_STEP(hw_pci, PCI_START_MAC_EVENTS, MTLK_OBJ_PTR(obj),
                      mtlk_hw_mmb_stop_mac_events, (obj->mmb));
    MTLK_CLEANUP_STEP(hw_pci, PCI_START_CORE, MTLK_OBJ_PTR(obj), 
                      mtlk_core_stop, (obj->core, mtlk_hw_mmb_get_cards_no(&mtlk_pci_mmb) - 1));
    MTLK_CLEANUP_STEP(hw_pci, PCI_MMB_START_CARD, MTLK_OBJ_PTR(obj), 
                      mtlk_hw_mmb_stop_card, (obj->mmb));
    MTLK_CLEANUP_STEP(hw_pci, PCI_REQUEST_IRQ, MTLK_OBJ_PTR(obj),
                      free_irq, (obj->dev->irq, obj));
    MTLK_CLEANUP_STEP(hw_pci, PCI_MMB_INIT_CARD, MTLK_OBJ_PTR(obj), 
                      mtlk_hw_mmb_cleanup_card, (obj->mmb));
    MTLK_CLEANUP_STEP(hw_pci, PCI_CREATE_CORE, MTLK_OBJ_PTR(obj), 
                      mtlk_core_delete, (obj->core));
    MTLK_CLEANUP_STEP(hw_pci, PCI_CREATE_CCR, MTLK_OBJ_PTR(obj),
                      mtlk_ccr_cleanup, (&obj->ccr));
    MTLK_CLEANUP_STEP(hw_pci, PCI_MMB_ADD_CARD, MTLK_OBJ_PTR(obj), 
                      mtlk_hw_mmb_remove_card, (&mtlk_pci_mmb, obj->mmb));
    MTLK_CLEANUP_STEP(hw_pci, PCI_SETUP_INTERRUPTS, MTLK_OBJ_PTR(obj), 
                      _mtlk_pci_clear_interrupts, (obj, obj->dev));
    MTLK_CLEANUP_STEP(hw_pci, PCI_MAP_BAR1, MTLK_OBJ_PTR(obj), 
                      iounmap, (obj->bar1));
    MTLK_CLEANUP_STEP(hw_pci, PCI_MAP_BAR0, MTLK_OBJ_PTR(obj), 
                      iounmap, (obj->bar0));
    MTLK_CLEANUP_STEP(hw_pci, PCI_SET_DMA_MASK, MTLK_OBJ_PTR(obj), 
                      MTLK_NOACTION, ());
    MTLK_CLEANUP_STEP(hw_pci, PCI_ENABLE_DEVICE, MTLK_OBJ_PTR(obj), 
                      MTLK_NOACTION, ());
  MTLK_CLEANUP_END(hw_pci, MTLK_OBJ_PTR(obj));

}

static int __init
_mtlk_pci_request_irq(mtlk_hw_pci_t *obj, struct pci_dev *dev)
{
  int retval;

  retval = request_irq(dev->irq, &_mtlk_pci_interrupt_handler,
    IRQF_SHARED, DRV_NAME, obj);

  if(0 != retval)
    ELOG("Failed to allocate PCI interrupt %d, error code: %d", dev->irq, retval);

  return retval;
}

static int __init
_mtlk_pci_setup_interrupts(mtlk_hw_pci_t *obj, struct pci_dev *dev)
{
#ifdef MTCFG_LINDRV_HW_PCIE
  int retval;

  if(MTLK_CARD_PCIE == obj->card_type)
  {
    retval = pci_enable_msi(dev);
    if(0 != retval)
    {
      ELOG("Failed to enable MSI interrupts for the device, error code: %d", retval);
      return retval;
    }
  }
#endif

  return 0;
}

static void* __init
_mtlk_pci_map_resource(struct pci_dev *dev, int res_id)
{
  void* ptr = ioremap(pci_resource_start(dev, res_id), 
                      pci_resource_len(dev, res_id));

  ILOG2(GID_HW_PCI, "BAR%d=0x%llX Len=0x%llX VA=0x%p", res_id, 
               (uint64) pci_resource_start(dev, res_id), 
               (uint64) pci_resource_len(dev, res_id),
               ptr);

  return ptr;
};

static int __init
_mtlk_pci_init(mtlk_hw_pci_t* obj, struct pci_dev *dev, 
               const struct pci_device_id *ent)
{
  int result;
  mtlk_core_hw_cfg_t core_cfg;
  mtlk_hw_mmb_card_cfg_t card_cfg;

  memset(obj, 0, sizeof(*obj));
  obj->dev = dev;
  obj->card_type = (mtlk_card_type_t) ent->driver_data;

  MTLK_ASSERT(_known_card_type(obj->card_type));

  MTLK_INIT_TRY(hw_pci, MTLK_OBJ_PTR(obj))
    MTLK_INIT_STEP_EX(hw_pci, PCI_ENABLE_DEVICE, MTLK_OBJ_PTR(obj),
                      pci_enable_device, (dev), result, 0 == result, 
                      MTLK_ERR_UNKNOWN);

    pci_set_drvdata(dev, obj);
    
    MTLK_INIT_STEP_EX(hw_pci, PCI_SET_DMA_MASK, MTLK_OBJ_PTR(obj),
                      pci_set_dma_mask, (dev, PCI_SUPPORTED_DMA_MASK),
                      result, 0 == result,
                      MTLK_ERR_UNKNOWN);

    pci_set_master(dev);

    MTLK_INIT_STEP_EX(hw_pci, PCI_MAP_BAR0, MTLK_OBJ_PTR(obj),
                      _mtlk_pci_map_resource, (dev, 0),
                      obj->bar0, NULL != obj->bar0,
                      MTLK_ERR_UNKNOWN);

    MTLK_INIT_STEP_EX(hw_pci, PCI_MAP_BAR1, MTLK_OBJ_PTR(obj),
                      _mtlk_pci_map_resource, (dev, 1),
                      obj->bar1, NULL != obj->bar1,
                      MTLK_ERR_UNKNOWN);

    MTLK_INIT_STEP_EX(hw_pci, PCI_SETUP_INTERRUPTS, MTLK_OBJ_PTR(obj),
                      _mtlk_pci_setup_interrupts, (obj, dev),
                      result, 0 == result,
                      MTLK_ERR_UNKNOWN);
 
    tasklet_init(&obj->mmb_tasklet, _mtlk_pci_tasklet, (unsigned long)obj);

    card_cfg.ccr               = NULL;
    card_cfg.pas               = obj->bar1;
    card_cfg.bus               = obj;
    card_cfg.df                = obj;
    card_cfg.ap                = ap[mtlk_hw_mmb_get_cards_no(&mtlk_pci_mmb)];
    card_cfg.max_no_tx_cfm_sec = MTLK_DEF_HW_NO_TX_CFM_SEC;

    MTLK_INIT_STEP_EX(hw_pci, PCI_MMB_ADD_CARD, MTLK_OBJ_PTR(obj),
                      mtlk_hw_mmb_add_card, (&mtlk_pci_mmb, &card_cfg),
                      obj->mmb, NULL != obj->mmb, MTLK_ERR_UNKNOWN);

    MTLK_INIT_STEP(hw_pci, PCI_CREATE_CCR, MTLK_OBJ_PTR(obj), mtlk_ccr_init,
                   (&obj->ccr, obj->card_type, obj, obj->bar0, obj->bar1) );

    MTLK_INIT_STEP_EX(hw_pci, PCI_CREATE_CORE, MTLK_OBJ_PTR(obj),
                      mtlk_core_create, (NULL),
                      obj->core, NULL != obj->core, MTLK_ERR_UNKNOWN);

    MTLK_INIT_STEP(hw_pci, PCI_MMB_INIT_CARD, MTLK_OBJ_PTR(obj),
                   mtlk_hw_mmb_init_card, (obj->mmb, obj->core, &obj->ccr));

    memset(&core_cfg, 0, sizeof(core_cfg));
    core_cfg.txmm = mtlk_hw_mmb_get_txmm(obj->mmb);
    core_cfg.txdm = mtlk_hw_mmb_get_txdm(obj->mmb);
    core_cfg.ap = card_cfg.ap;

    MTLK_INIT_STEP_EX(hw_pci, PCI_REQUEST_IRQ, MTLK_OBJ_PTR(obj),
                      _mtlk_pci_request_irq, (obj, dev),
                      result, 0 == result, MTLK_ERR_UNKNOWN);
    ILOG2(GID_HW_PCI, "%s IRQ 0x%x", DRV_NAME, dev->irq);

    MTLK_INIT_STEP(hw_pci, PCI_MMB_START_CARD, MTLK_OBJ_PTR(obj),
                   mtlk_hw_mmb_start_card, (obj->mmb));

    MTLK_INIT_STEP(hw_pci, PCI_START_CORE, MTLK_OBJ_PTR(obj),
                   mtlk_core_start, (obj->core, obj->mmb, 
                                     &core_cfg, mtlk_hw_mmb_get_cards_no(&mtlk_pci_mmb)));

    MTLK_INIT_STEP_VOID(hw_pci, PCI_START_MAC_EVENTS, MTLK_OBJ_PTR(obj),
                        MTLK_NOACTION, ());
    MTLK_INIT_STEP_VOID(hw_pci, PCI_CORE_PREPARE_STOP, MTLK_OBJ_PTR(obj),
                        MTLK_NOACTION, ());
  MTLK_INIT_FINALLY(hw_pci, MTLK_OBJ_PTR(obj))    
  MTLK_INIT_RETURN(hw_pci, MTLK_OBJ_PTR(obj), _mtlk_pci_cleanup, (obj))
}

static mtlk_hw_pci_t* __init
_mtlk_pci_alloc(void)
{
#if defined(CONFIG_ARCH_STR9100) && defined(CONFIG_CPU_DSPAD_ENABLE)
  LOG0("Using D-scratchpad for hot context");
  return str9100_dspad_alloc(sizeof(mtlk_hw_pci_t));
#else
  return kmalloc_tag(sizeof(mtlk_hw_pci_t), GFP_KERNEL, MTLK_MEM_TAG_PCI);
#endif
}

static void
_mtlk_pci_free(mtlk_hw_pci_t* obj)
{
#if defined(CONFIG_ARCH_STR9100) && defined(CONFIG_CPU_DSPAD_ENABLE)
  str9100_dspad_free(obj);
#else
  kfree_tag(obj);
#endif
}

static int __init
_mtlk_pci_probe(struct pci_dev *dev, const struct pci_device_id *ent)
{
    mtlk_hw_pci_t *obj = _mtlk_pci_alloc();

    if(!obj) 
      return -ENODEV;

    if(MTLK_ERR_OK != _mtlk_pci_init(obj, dev, ent)) {
      _mtlk_pci_free(obj);
      return -ENODEV;
    }

    return 0;
}

static void __devexit
_mtlk_pci_remove(struct pci_dev *pdev)
{
  ILOG2(GID_HW_PCI, "%s CleanUp", pci_name(pdev));

  _mtlk_pci_cleanup(pci_get_drvdata(pdev));
  _mtlk_pci_free(pci_get_drvdata(pdev));

  ILOG2(GID_HW_PCI, "%s CleanUp finished", pci_name(pdev));
}

static struct pci_device_id mtlk_pci_tbl[] = {
#ifdef MTCFG_LINDRV_HW_PCIG2
  { MTLK_VENDOR_ID,     HYPERION_II_PCI_DEVICE_ID_A1, PCI_ANY_ID, PCI_ANY_ID, 0, 0, MTLK_CARD_PCIG2},
  { MTLK_VENDOR_ID,     HYPERION_II_PCI_DEVICE_ID_A2, PCI_ANY_ID, PCI_ANY_ID, 0, 0, MTLK_CARD_PCIG2},
#endif

#ifdef MTCFG_LINDRV_HW_PCIE
  { MTLK_VENDOR_ID,     HYPERION_III_PCIE_DEVICE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, MTLK_CARD_PCIE},
#endif

#ifdef MTCFG_LINDRV_HW_PCIG3
  { MTLK_VENDOR_ID,     HYPERION_III_PCI_DEVICE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, MTLK_CARD_PCIG3},
#endif
  { 0,}
};

MODULE_DEVICE_TABLE(pci, mtlk_pci_tbl);

static struct pci_driver mtlk_pci_driver = {
  .name     = "mtlk",
  .id_table = mtlk_pci_tbl,
  .probe    = _mtlk_pci_probe,
  .remove   = __devexit_p(_mtlk_pci_remove),
};

static mtlk_hw_mmb_cfg_t mtlk_pci_mmb_cfg =   {
  .bist_check_permitted  = 1,
  .no_pll_write_delay_us = 0,
};

struct mtlk_osdep_state
{
  MTLK_DECLARE_INIT_STATUS;
};

static struct mtlk_osdep_state osdep_state = {0};

MTLK_INIT_STEPS_LIST_BEGIN(pci_osdep)
  MTLK_INIT_STEPS_LIST_ENTRY(pci_osdep, OSDEP_OSAL_INIT)
#ifdef MTCFG_BENCHMARK_TOOLS
  MTLK_INIT_STEPS_LIST_ENTRY(pci_osdep, OSDEP_DBG_INIT)
#endif
#ifdef MTCFG_CPU_STAT
  MTLK_INIT_STEPS_LIST_ENTRY(pci_osdep, OSDEP_STAT_INIT)
#endif
  MTLK_INIT_STEPS_LIST_ENTRY(pci_osdep, OSDEP_MMB_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(pci_osdep, OSDEP_NL_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(pci_osdep, OSDEP_IRBM_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(pci_osdep, OSDEP_IRB_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(pci_osdep, OSDEP_IRBM_START)
MTLK_INIT_INNER_STEPS_BEGIN(pci_osdep)
MTLK_INIT_STEPS_LIST_END(pci_osdep);

static void
osdep_cleanup (void)
{
  MTLK_CLEANUP_BEGIN(pci_osdep, MTLK_OBJ_PTR(&osdep_state))
    MTLK_CLEANUP_STEP(pci_osdep, OSDEP_IRBM_START, MTLK_OBJ_PTR(&osdep_state), 
                      mtlk_irb_media_stop, ());
    MTLK_CLEANUP_STEP(pci_osdep, OSDEP_IRB_INIT, MTLK_OBJ_PTR(&osdep_state), 
                      mtlk_irb_cleanup, ());
    MTLK_CLEANUP_STEP(pci_osdep, OSDEP_IRBM_INIT, MTLK_OBJ_PTR(&osdep_state), 
                      mtlk_irb_media_cleanup, ());
    MTLK_CLEANUP_STEP(pci_osdep, OSDEP_NL_INIT, MTLK_OBJ_PTR(&osdep_state), 
                      mtlk_nl_cleanup, ());
    MTLK_CLEANUP_STEP(pci_osdep, OSDEP_MMB_INIT, MTLK_OBJ_PTR(&osdep_state), 
                      mtlk_hw_mmb_cleanup, (&mtlk_pci_mmb));
#ifdef MTCFG_CPU_STAT
    MTLK_CLEANUP_STEP(pci_osdep, OSDEP_STAT_INIT, MTLK_OBJ_PTR(&osdep_state),
                      mtlk_cpu_stat_cleanup, ())
#endif
#ifdef MTCFG_BENCHMARK_TOOLS
    MTLK_CLEANUP_STEP(pci_osdep, OSDEP_DBG_INIT, MTLK_OBJ_PTR(&osdep_state), 
                      mtlk_dbg_cleanup, ())
#endif
    MTLK_CLEANUP_STEP(pci_osdep, OSDEP_OSAL_INIT, MTLK_OBJ_PTR(&osdep_state), 
                      mtlk_osal_cleanup, ());
  MTLK_CLEANUP_END(pci_osdep, MTLK_OBJ_PTR(&osdep_state))

  memset(&osdep_state, 0, sizeof(osdep_state));
}

static int __init
osdep_init (void)
{
  MTLK_INIT_TRY(pci_osdep, MTLK_OBJ_PTR(&osdep_state))
    MTLK_INIT_STEP(pci_osdep, OSDEP_OSAL_INIT, MTLK_OBJ_PTR(&osdep_state), 
                   mtlk_osal_init, ());
#ifdef MTCFG_BENCHMARK_TOOLS
    MTLK_INIT_STEP(pci_osdep, OSDEP_DBG_INIT, MTLK_OBJ_PTR(&osdep_state), 
                   mtlk_dbg_init, ());
#endif
#ifdef MTCFG_CPU_STAT
    MTLK_INIT_STEP_VOID(pci_osdep, OSDEP_STAT_INIT, MTLK_OBJ_PTR(&osdep_state),
                        mtlk_cpu_stat_init, ())
#endif
    MTLK_INIT_STEP(pci_osdep, OSDEP_MMB_INIT, MTLK_OBJ_PTR(&osdep_state), 
                   mtlk_hw_mmb_init, (&mtlk_pci_mmb, &mtlk_pci_mmb_cfg));
    MTLK_INIT_STEP(pci_osdep, OSDEP_NL_INIT, MTLK_OBJ_PTR(&osdep_state), 
                   mtlk_nl_init, ());
    MTLK_INIT_STEP(pci_osdep, OSDEP_IRBM_INIT, MTLK_OBJ_PTR(&osdep_state), 
                   mtlk_irb_media_init, ());
    MTLK_INIT_STEP(pci_osdep, OSDEP_IRB_INIT, MTLK_OBJ_PTR(&osdep_state), 
                   mtlk_irb_init, ());
    MTLK_INIT_STEP(pci_osdep, OSDEP_IRBM_START, MTLK_OBJ_PTR(&osdep_state), 
                   mtlk_irb_media_start, ());
  MTLK_INIT_FINALLY(pci_osdep, MTLK_OBJ_PTR(&osdep_state))
  MTLK_INIT_RETURN(pci_osdep, MTLK_OBJ_PTR(&osdep_state), osdep_cleanup, ())
}

struct mtlk_drv_state
{
  int os_res;
  int init_res;
  MTLK_DECLARE_INIT_STATUS;
};

static struct mtlk_drv_state drv_state = {0};

MTLK_INIT_STEPS_LIST_BEGIN(pci_drv)
  MTLK_INIT_STEPS_LIST_ENTRY(pci_drv, DRV_LOG_OSDEP_INIT)
#ifdef MTLK_DEBUG
  MTLK_INIT_STEPS_LIST_ENTRY(pci_drv, DRV_STEP_TO_FAIL_SET)
#endif
  MTLK_INIT_STEPS_LIST_ENTRY(pci_drv, DRV_OSDEP_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(pci_drv, DRV_MODULE_REGISTER)
MTLK_INIT_INNER_STEPS_BEGIN(pci_drv)
MTLK_INIT_STEPS_LIST_END(pci_drv);

static void
__mtlk_pci_cleanup_module (void)
{
  MTLK_CLEANUP_BEGIN(pci_drv, MTLK_OBJ_PTR(&drv_state))
    MTLK_CLEANUP_STEP(pci_drv, DRV_MODULE_REGISTER, MTLK_OBJ_PTR(&drv_state),
                      pci_unregister_driver, (&mtlk_pci_driver));
    MTLK_CLEANUP_STEP(pci_drv, DRV_OSDEP_INIT, MTLK_OBJ_PTR(&drv_state),
                      osdep_cleanup, ());
#ifdef MTLK_DEBUG
    MTLK_CLEANUP_STEP(pci_drv, DRV_STEP_TO_FAIL_SET, MTLK_OBJ_PTR(&drv_state),
                      MTLK_NOACTION, ());
#endif
    MTLK_CLEANUP_STEP(pci_drv, DRV_LOG_OSDEP_INIT, MTLK_OBJ_PTR(&drv_state),
                      log_osdep_cleanup, ());
  MTLK_CLEANUP_END(pci_drv, MTLK_OBJ_PTR(&drv_state))
}

static int __init
__mtlk_pci_init_module (void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22) 
#define pci_init_f pci_module_init
#else
#define pci_init_f pci_register_driver
#endif

  MTLK_INIT_TRY(pci_drv, MTLK_OBJ_PTR(&drv_state))
    MTLK_INIT_STEP_VOID(pci_drv, DRV_LOG_OSDEP_INIT, MTLK_OBJ_PTR(&drv_state),
                        log_osdep_init, ());
#ifdef MODULE
    ILOG2(GID_HW_PCI, "Starting MTLK");
#endif
#ifdef MTLK_DEBUG
    MTLK_INIT_STEP_VOID(pci_drv, DRV_STEP_TO_FAIL_SET, MTLK_OBJ_PTR(&drv_state),
                        mtlk_startup_set_step_to_fail, (step_to_fail));
#endif
    MTLK_INIT_STEP(pci_drv, DRV_OSDEP_INIT, MTLK_OBJ_PTR(&drv_state),
                   osdep_init, ());
    MTLK_INIT_STEP_EX(pci_drv, DRV_MODULE_REGISTER, MTLK_OBJ_PTR(&drv_state),
                      pci_init_f, (&mtlk_pci_driver),
                      drv_state.os_res, drv_state.os_res == 0, MTLK_ERR_NO_RESOURCES);
  MTLK_INIT_FINALLY(pci_drv, MTLK_OBJ_PTR(&drv_state))
  MTLK_INIT_RETURN(pci_drv, MTLK_OBJ_PTR(&drv_state), __mtlk_pci_cleanup_module, ())
}

static int __init
_mtlk_pci_init_module (void)
{
  drv_state.init_res = __mtlk_pci_init_module();
  return (drv_state.init_res == MTLK_ERR_OK)?0:drv_state.os_res;
}

static void __exit
_mtlk_pci_cleanup_module (void)
{
  ILOG2(GID_HW_PCI, "Cleanup");
  if (drv_state.init_res == MTLK_ERR_OK) {
    /* Call cleanup only if init succeeds, 
     * otherwise it will be called by macros on init itself 
     */
    __mtlk_pci_cleanup_module();
  }
}

module_init(_mtlk_pci_init_module);
module_exit(_mtlk_pci_cleanup_module);

