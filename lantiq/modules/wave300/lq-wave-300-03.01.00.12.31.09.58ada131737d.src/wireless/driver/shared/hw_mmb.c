/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"
#include "hw_mmb.h"

#include "shram.h"
#include "mtlkaux.h"
#include "mtlklist.h"
#include "mtlkdfnet.h"

#ifdef MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS

#define TSF_CARD_IDX 0

typedef struct 
{
  mtlk_osal_spinlock_t lock;
  mtlk_hw_t           *card;
  BOOL                 started;
} hw_tsf_info_t;

static hw_tsf_info_t hw_tsf_info;
#endif

#define MTLK_FRMW_LOAD_CHUNK_TIMEOUT   2000 /* ms */
#define MTLK_MAC_BOOT_TIMEOUT          2000 /* ms */
#define MTLK_CHI_MAGIC_TIMEOUT         2000 /* ms */
#define MTLK_READY_CFM_TIMEOUT         5000 /* ms */
#define MTLK_SW_RESET_CFM_TIMEOUT      5000 /* ms */
#define MTLK_PRGMDL_LOAD_CHUNK_TIMEOUT 5000 /* ms */
#define MTLK_RX_BUFFS_RECOVERY_PERIOD  5000 /* ms */

#define MTLK_MAX_RX_BUFFS_TO_RECOVER   ((uint16)-1) /* no limit */

#define RX_MAX_MSG_OFFSET       2    /* alignment offset from MAC. TODO: ???? */

#define HW_PCI_TXMM_MAX_MSGS 32
#define HW_PCI_TXDM_MAX_MSGS 2
#define HW_PCI_TXMM_GRANULARITY 1000
#define HW_PCI_TXDM_GRANULARITY 1000

#define HW_PCI_TXM_MAX_FAILS 5

#define DAT_CFM_ID_NONE      0xFF

#ifndef MTLK_RX_BUFF_ALIGNMENT
#define MTLK_RX_BUFF_ALIGNMENT 0     /* No special alignment required */
#endif

#ifndef HIBYTE
#define HIBYTE(s) ((uint8)((uint16)(s) >> 8))
#endif
#ifndef LOBYTE
#define LOBYTE(s) ((uint8)(s))
#endif

#define MTLK_HW_MAX_RX_DATA_QUEUES MAX_RX_DATA_QUEUES

typedef struct
{
  uint8  percentage;  /* percent */
  uint8  min_buffers; /* nof     */
  uint16 data_size;   /* bytes   */
} mtlk_hw_rx_queue_cfg_t;

typedef struct
{
  uint32                 nof_queues_enabled;
  mtlk_hw_rx_queue_cfg_t queue[MTLK_HW_MAX_RX_DATA_QUEUES];
} mtlk_hw_rx_queues_cfg_t;

static mtlk_hw_rx_queue_cfg_t default_rx_queues_cfg[] = {
  /* %%,  nof, bytes - MUST BE ARRANGED BY BUFFER SIZE (increasing) */
  {  40,  10,   100 },
  {  40,  10,  1600 },
  {  20,   2,  4082 /* 4096 (page size) - 14 (overhead size) */ }
};

typedef struct
{
  uint16 que_size; /* Current queue size */
  uint8  min_size; /* Minimal queue size */
  uint16 buf_size; /* Queue buffer size  */
} mtlk_hw_rx_queue_t;

typedef struct
{
  uint16             nof_in_use; /* Number of queues in use */
  mtlk_hw_rx_queue_t queue[MTLK_HW_MAX_RX_DATA_QUEUES];
} mtlk_hw_rx_queues_t;


/*****************************************************
 * IND/REQ BD-related definitions
 *****************************************************/
typedef struct 
{
  uint32 offset; /* BD offset (PAS) */
  uint16 size;   /* BD size         */
  uint16 idx;    /* BD access index */
} mtlk_hw_bd_t;

typedef struct 
{
  mtlk_hw_bd_t ind;
  mtlk_hw_bd_t req;
} mtlk_hw_ind_req_bd_t;
/*****************************************************/

/*****************************************************
 * Data Tx-related definitions
 *****************************************************/
typedef struct 
{
  uint8                index;      /* index in mirror array */
  mtlk_dlist_entry_t   list_entry; /* for mirror elements list */
  mtlk_nbuf_t         *nbuf;       /* Network buffer   */
  uint32               dma_addr;   /* DMA mapped address */
  uint32               size;       /* Data buffer size */
  uint8                ac;         /* Packet's access category */
  mtlk_osal_timestamp_t ts;        /* Timestamp (used for TX monitoring) */
} mtlk_hw_data_req_mirror_t;
/*****************************************************/

/*****************************************************
 * Data Rx-related definitions
 *****************************************************/
typedef struct 
{
  uint8                index;   /* index in mirror array */
  mtlk_nbuf_t         *nbuf;    /* Network buffer   */
  uint32               dma_addr;/* DMA mapped address */
  uint32               size;    /* Data buffer size */
  uint8                que_idx; /* Rx Queue Index */
  mtlk_lslist_entry_t  pend_l;  /* Pending list entry */
} mtlk_hw_data_ind_mirror_t;
/*****************************************************/

/*****************************************************
 * Control Messages (CM = MM and DM) Tx-related definitions
 *****************************************************/
typedef struct _mtlk_hw_cm_req_obj_t
{
  uint8              index;      /* index in mirror array */
  mtlk_dlist_entry_t list_entry; /* for mirror elements list */
  mtlk_hw_cm_msg_t   msg;        /* SHRAM_MAN_MSG or SHRAM_DBG_MSG */
#ifdef MTCFG_DEBUG
  mtlk_atomic_t      usage_cnt;  /* message usage counter */
#endif
} mtlk_hw_cm_req_mirror_t;
/*****************************************************/

/*****************************************************
 * Control Messages (CM = MM and DM) Rx-related definitions
 * NOTE: msg member must be 1st in these structures because
 *       it is used for copying messages to/from PAS and
 *       buffers that are used for copying to/from PAS must
 *       be aligned to 32 bits boundary (see 
 *       _mtlk_mmb_memcpy...() functions)
 *****************************************************/
typedef struct 
{
  SHRAM_MAN_MSG msg;         /* Must be 1st for alignment */
  uint8         index;       /* index in mirror array */
  int8          reserved[3]; /* padding */
} mtlk_hw_man_ind_mirror_t;

typedef struct 
{
  SHRAM_DBG_MSG msg;         /* Must be 1st for alignment */
  uint8         index;       /* index in mirror array */
  int8          reserved[3]; /* padding */
} mtlk_hw_dbg_ind_mirror_t;
/*****************************************************/

/*****************************************************
 * Auxilliary BD ring-related definitions
 *****************************************************/
/********************************************************************
 * Number of BD descriptors
 * PAS offset of BD array (ring)
 * Local BD mirror (array)
*********************************************************************/
#define DECLARE_BASIC_BDR_STRUCT(name, mirror_el_type, pas_el_type)\
  typedef struct {                                                 \
    uint16          nof_bds;  /* Number of BD descriptors*/        \
    pas_el_type    *pas_array;                                     \
    mirror_el_type *mirror;                                        \
  } name;

/********************************************************************
 * Number of BD descriptors
 * PAS offset of BD array (ring)
 * Local BD mirror (array)
 * Free BD descriptors list
 * BD ring lock object
*********************************************************************/
#define DECLARE_ADVANCED_BDR_STRUCT(name, mirror_el_type, pas_el_type)\
  typedef struct {                                                    \
    uint16               nof_bds;                                     \
    pas_el_type         *pas_array;                                   \
    mirror_el_type      *mirror;                                      \
    mtlk_dlist_t         free_list;                                   \
    mtlk_dlist_t         used_list;                                   \
    mtlk_osal_spinlock_t lock;                                        \
  } name;

/* Anton TODO: we must use TXDAT_REQ_MSG_DESC instead the SHRAM_DAT_REQ_MSG
 * for mtlk_hw_tx_data_t in the future. Now MAC needs it because of internal 
 * data arrangement.
 */
DECLARE_ADVANCED_BDR_STRUCT(mtkl_hw_tx_data_t, 
                            mtlk_hw_data_req_mirror_t,
                            SHRAM_DAT_REQ_MSG);
DECLARE_BASIC_BDR_STRUCT(mtkl_hw_rx_data_t, 
                         mtlk_hw_data_ind_mirror_t,
                         RXDAT_IND_MSG_DESC);
DECLARE_ADVANCED_BDR_STRUCT(mtkl_hw_tx_man_t,
                            mtlk_hw_cm_req_mirror_t,
                            SHRAM_MAN_MSG);
DECLARE_ADVANCED_BDR_STRUCT(mtkl_hw_tx_dbg_t,
                            mtlk_hw_cm_req_mirror_t,
                            SHRAM_DBG_MSG);
DECLARE_BASIC_BDR_STRUCT(mtkl_hw_rx_man_t,
                         mtlk_hw_man_ind_mirror_t,
                         SHRAM_MAN_MSG);
DECLARE_BASIC_BDR_STRUCT(mtkl_hw_rx_dbg_t,
                         mtlk_hw_dbg_ind_mirror_t,
                         SHRAM_DBG_MSG);

#define MTLK_CLEANUP_BASIC_BDR(o, bdr_field)                            \
  do {                                                                  \
    if ((o)->bdr_field.mirror)                                          \
      mtlk_osal_mem_free((o)->bdr_field.mirror);                        \
    memset(&(o)->bdr_field, 0, sizeof((o)->bdr_field));                 \
  } while (0)


#define MTLK_INIT_BASIC_BDR(o, bdr_field,  mirror_el_type, pas_el_type, \
                            _nof_bds, _pas_offset)                      \
  do {                                                                  \
    memset(&(o)->bdr_field, 0, sizeof((o)->bdr_field));                 \
    (o)->bdr_field.nof_bds   = (uint16)MAC_TO_HOST32(_nof_bds);         \
    (o)->bdr_field.pas_array = (pas_el_type *)                          \
      ((o)->cfg.pas + MAC_TO_HOST32(_pas_offset));                      \
    (o)->bdr_field.mirror    = (mirror_el_type *)                       \
      mtlk_osal_mem_alloc((o)->bdr_field.nof_bds *                      \
                          sizeof(mirror_el_type), MTLK_MEM_TAG_BDR);    \
    memset((o)->bdr_field.mirror,                                       \
           0,                                                           \
           (o)->bdr_field.nof_bds * sizeof(mirror_el_type));            \
    if ((o)->bdr_field.mirror) {                                        \
      uint16 ___i = 0;                                                  \
      for (; ___i < (o)->bdr_field.nof_bds; ___i++)                     \
        (o)->bdr_field.mirror[___i].index = (uint8)___i;                \
    }                                                                   \
  } while (0)

#define MTLK_CLEANUP_ADVANCED_BDR(o, bdr_field)                         \
  do {                                                                  \
    mtlk_dlist_cleanup(&(o)->bdr_field.used_list);                      \
    /* Empty list to prevent ASSERT on cleanup */                       \
    while (mtlk_dlist_pop_front(&(o)->bdr_field.free_list)) ;           \
    mtlk_dlist_cleanup(&(o)->bdr_field.free_list);                      \
    mtlk_osal_lock_cleanup(&(o)->bdr_field.lock);                       \
  } while (0)

#define MTLK_INIT_ADVANCED_BDR(o, bdr_field)                            \
  do {                                                                  \
    uint16 ___i = 0;                                                    \
    mtlk_osal_lock_init(&(o)->bdr_field.lock);                          \
    mtlk_dlist_init(&(o)->bdr_field.free_list);                         \
    for (; ___i < (o)->bdr_field.nof_bds; ___i++) {                     \
      mtlk_dlist_push_back(&(o)->bdr_field.free_list,                   \
                           &(o)->bdr_field.mirror[___i].list_entry);    \
    }                                                                   \
    mtlk_dlist_init(&(o)->bdr_field.used_list);                         \
  } while (0)
/*****************************************************/

typedef struct
{
  mtlk_lslist_t     lbufs; /* Rx Data Buffers to be re-allocated */
  mtlk_osal_timer_t timer; /* Recovery Timer */
} mtlk_hw_rx_pbufs_t; /* failed RX buffers allocations recovery */

typedef enum
{
  MTLK_ISR_NONE,
  MTLK_ISR_INIT_EVT,
  MTLK_ISR_MSGS_PUMP,
  MTLK_ISR_LAST
} mtlk_hw_mmb_isr_type_e;

typedef struct
{
  VECTOR_AREA_CALIBR_EXTENSION_DATA ext_data;
  void                             *buffer;
  uint32                            dma_addr;
} mtlk_calibr_data_t;

typedef struct
{
  VECTOR_AREA_MIPS_CONTROL_EXTENSION_DATA ext_data;
} mtlk_mips_ctrl_data_t;

struct _mtlk_hw_t 
{
  mtlk_hw_mmb_card_cfg_t cfg;
  mtlk_hw_mmb_t         *mmb;

  mtlk_core_t           *core; 
  mtlk_hw_state_e        state;

  VECTOR_AREA_BASIC      chi_data; /* Can be removed? */
  mtlk_calibr_data_t     calibr;   /* Calibration Extension related */
  mtlk_mips_ctrl_data_t  mips_ctrl;/* MIPS Control Extension related */

  mtlk_osal_spinlock_t   reg_lock;
  volatile int           init_evt; /* used during the initialization */
  mtlk_hw_mmb_isr_type_e isr_type;

  mtlk_hw_ind_req_bd_t   bds;     /* IND/REQ BD */

  mtkl_hw_tx_data_t      tx_data; /* Tx Data related */
  uint16                 tx_data_nof_free_bds; /* Number of free REQ BD descriptors */
  uint16                 tx_data_max_used_bds; /* Maximal number of used REQ BD descriptors */

  mtkl_hw_rx_data_t      rx_data; /* Rx Data related */
  mtlk_hw_rx_queues_t    rx_data_queues; /* Dynamic Rx Buffer Queues */
  mtlk_hw_rx_pbufs_t     rx_data_pending; /* Rx Data Buffers recovery */

  mtkl_hw_tx_man_t       tx_man;  /* Tx MM related */
  int                    tx_man_in_use; /* Mailslot is in use by TXMM */
  mtkl_hw_rx_man_t       rx_man;  /* Rx MM related */
  mtkl_hw_tx_dbg_t       tx_dbg;  /* Tx DM related */
  mtkl_hw_rx_dbg_t       rx_dbg;  /* Rx DM related */

  mtlk_txmm_t            txmm;

  mtlk_txmm_t            txdm;

  int                    mac_events_stopped; /* No INDs must be passed to Core except those needed to perform cleanup */
  int                    mac_events_stopped_completely; /* No INDs must be passed to Core at all*/
  BOOL                   mac_reset_logic_initialized;

  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_START_STATUS;
};

/**********************************************************************
 * INIT event: impemented as flag + sleep
 * NOTE: can't be OSAL event because of SET from ISR (OSAL limitation)
 **********************************************************************/
#define MTLK_HW_INIT_EVT_STEP_MS      20 /* INIT_EVT WAIT resolution */

static __INLINE int
MTLK_HW_INIT_EVT_INIT (mtlk_hw_t *hw)
{
  hw->init_evt = 0;
  return MTLK_ERR_OK;
}

static __INLINE void
MTLK_HW_INIT_EVT_SET (mtlk_hw_t *hw)
{
  hw->init_evt = 1;
}

static __INLINE void
MTLK_HW_INIT_EVT_RESET (mtlk_hw_t *hw)
{
  hw->init_evt = 0;
}

static __INLINE void
MTLK_HW_INIT_EVT_CLEANUP (mtlk_hw_t *hw)
{
}

static __INLINE int 
MTLK_HW_INIT_EVT_WAIT (mtlk_hw_t *hw, uint32 msec)
{
  int res = MTLK_ERR_UNKNOWN;

  while (1) {
    if (hw->init_evt) {
      res = MTLK_ERR_OK;
      break;
    }
    else if (msec < MTLK_HW_INIT_EVT_STEP_MS) {
      res = MTLK_ERR_TIMEOUT;
      break;
    }
    else {
      mtlk_osal_msleep(MTLK_HW_INIT_EVT_STEP_MS);
      msec -= MTLK_HW_INIT_EVT_STEP_MS;
    }
  }

  return res;
}
/**********************************************************************/

#define _mtlk_mmb_pas_writel(hw, comment, index, v)                 \
  for(;;) {                                                         \
    ILOG6(GID_HW_MMB, "Write PAS: %s", comment);                         \
    mtlk_hw_bus_writel((hw)->cfg.bus,                               \
                       (v),(hw)->cfg.pas + (uint32)(index));        \
	break;															                              \
  }
#define _mtlk_mmb_pas_readl(hw, comment, index, v)                  \
  for(;;) {                                                         \
    ILOG6(GID_HW_MMB, "Read PAS: %s", comment);                          \
    (v) = mtlk_hw_bus_readl((hw)->cfg.bus,                          \
                            (hw)->cfg.pas + (uint32)(index));       \
	break;															                              \
  }

static __INLINE int
_mtlk_mmb_memcpy_fromio (mtlk_hw_t  *hw,
                         void       *to,
                         const void *from,
                         uint32      count)
{
  if ((((unsigned long)to | (unsigned long)from | count) & 0x3) == 0) {
    while (count) {
      *((uint32 *)to) = mtlk_hw_bus_raw_readl(hw->cfg.bus, from);
      from   = ((uint8 *)from) + 4;
      to     = ((uint8 *)to) + 4;
      count -= 4;
    }
    return 1;
  }
  else {
    ELOG("Unaligned access (to=0x%p, from=0x%p, size=%d)",
          to, from, count);
    MTLK_ASSERT(FALSE);
    return 0;
  }
}

static __INLINE int
_mtlk_mmb_memcpy_toio (mtlk_hw_t  *hw,
                       void       *to,
                       const void *from,
                       uint32      count)
{
  if ((((unsigned long)to | (unsigned long)from | count) & 0x3) == 0) {
    while (count) {
      mtlk_hw_bus_raw_writel(hw->cfg.bus, *(uint32 *)from, to);
      from   = ((uint8 *)from) + 4;
      to     = ((uint8 *)to) + 4;
      count -= 4;
    }
    return 1;
  }
  else {
    ELOG("Unaligned access (to=0x%p, from=0x%p, size=%d)",
          to, from, count);
    MTLK_ASSERT(FALSE);
    return 0;
  }
}

static __INLINE int
_mtlk_mmb_memcpy_toio_no_pll (mtlk_hw_t  *hw,
                              void       *to,
                              const void *from,
                              uint32     count)
{
  if ((((unsigned long)to | (unsigned long)from | count) & 0x3) == 0) {
    while (count) {
      if (hw->mmb->cfg.no_pll_write_delay_us) {
        mtlk_hw_bus_udelay(hw->cfg.bus, 
                           hw->mmb->cfg.no_pll_write_delay_us);
      }
      mtlk_hw_bus_raw_writel(hw->cfg.bus, *(uint32 *)from, to);
      from   = ((uint8 *)from) + 4;
      to     = ((uint8 *)to) + 4;
      count -= 4;
    }
    return 1;
  }
  else {
    ELOG("Unaligned access (to=0x%p, from=0x%p, size=%d)",
          to, from, count);
    MTLK_ASSERT(FALSE);
    return 0;
  }
}

#define _mtlk_mmb_pas_get(hw, comment, index, ptr, n) \
  _mtlk_mmb_memcpy_fromio((hw), (ptr), (hw)->cfg.pas + (index), (n))
#define _mtlk_mmb_pas_put(hw, comment, index, ptr, n) \
  _mtlk_mmb_memcpy_toio((hw), (hw)->cfg.pas + (index), (ptr), (n))

static int _mtlk_mmb_send_msg (mtlk_hw_t *hw, 
                               uint8      msg_type,
                               uint8      msg_index,
                               uint16     msg_info);

static void txmm_on_cfm(mtlk_hw_t *hw, PMSG_OBJ pmsg);
static void txdm_on_cfm(mtlk_hw_t *hw, PMSG_OBJ pmsg);

static int _mtlk_mmb_txmm_init(mtlk_hw_t *hw);
static int _mtlk_mmb_txdm_init(mtlk_hw_t *hw);
static void _mtlk_mmb_free_unconfirmed_tx_buffers(mtlk_hw_t *hw);

#define HW_MSG_PTR(msg)          ((mtlk_hw_msg_t *)(msg))
#define DATA_REQ_MIRROR_PTR(msg) ((mtlk_hw_data_req_mirror_t *)(msg))
#define MAN_IND_MIRROR_PTR(msg)  ((mtlk_hw_man_ind_mirror_t *)(msg))
#define MAN_DBG_MIRROR_PTR(msg)  ((mtlk_hw_dbg_ind_mirror_t *)(msg))

#define HW_MSG_IN_BDR(o, bdr_field, msg)                          \
  (((uint8 *)msg) >= ((uint8 *)(o)->bdr_field.mirror) &&          \
   ((uint8 *)msg) < ((uint8 *)(o)->bdr_field.mirror) +            \
    (o)->bdr_field.nof_bds * sizeof((o)->bdr_field.mirror[0]))

#if MTLK_RX_BUFF_ALIGNMENT
static __INLINE mtlk_nbuf_t *
_mtlk_mmb_nbuf_alloc (mtlk_hw_t *hw,
                      uint32     size)
{
  mtlk_nbuf_t *nbuf = mtlk_df_nbuf_alloc(hw->cfg.df, size);
  if (nbuf) {
    /* Align skbuffer if required by HW */
    unsigned long tail = ((unsigned long)mtlk_df_nbuf_get_virt_addr(nbuf)) & 
                         (MTLK_RX_BUFF_ALIGNMENT - 1);
    if (tail) {
      unsigned long nof_pad_bytes = MTLK_RX_BUFF_ALIGNMENT - tail;
      mtlk_df_nbuf_reserve(nbuf, nof_pad_bytes);
    }
  }
  
  return nbuf;
}
#else
#define _mtlk_mmb_nbuf_alloc(hw, size)  mtlk_df_nbuf_alloc((hw)->cfg.df, (size))
#endif
#define _mtlk_mmb_nbuf_free(hw, nbuf)   mtlk_df_nbuf_free((hw)->cfg.df, nbuf)

static __INLINE void 
_mtlk_mmb_set_rx_payload_addr (mtlk_hw_t *hw, 
                               uint16     ind_idx, 
                               uint32     dma_addr)
{
  RXDAT_IND_MSG_DESC bd;

  bd.u32HostPayloadAddr = HOST_TO_MAC32(dma_addr);
  
  /* Rx IND descriptor (DMA address) */
  _mtlk_mmb_memcpy_toio(hw,
                        &hw->rx_data.pas_array[ind_idx],
                        &bd,
                        sizeof(bd));
}

static  __INLINE uint8
_mtlk_mmb_rxque_get_next_que_idx (mtlk_hw_t *hw, 
                                 uint8      que_idx, 
                                 uint32     data_size)
{
  MTLK_UNREFERENCED_PARAM(hw);
  MTLK_UNREFERENCED_PARAM(data_size);

  return que_idx;
}

static int 
_mtlk_mmb_rxque_set_default_cfg (mtlk_hw_t *hw)
{
  int    res              = MTLK_ERR_UNKNOWN;
  int    i                = 0;
  uint16 total_percentage = 0;
  uint16 total_bds        = 0;

  memset(&hw->rx_data_queues, 0, sizeof(hw->rx_data_queues));
 
  for (; i < ARRAY_SIZE(default_rx_queues_cfg); i++) {
    mtlk_hw_rx_queue_t *queue = &hw->rx_data_queues.queue[i];

    if (i == ARRAY_SIZE(default_rx_queues_cfg) - 1) {
      /* Last queue - take all the rest (round percentage) */
      queue->que_size = (uint16)(hw->rx_data.nof_bds - total_bds);
    }
    else {
      /* Take by percentage */
      queue->que_size = 
        (uint16)(hw->rx_data.nof_bds * default_rx_queues_cfg[i].percentage / 100);
    }

    queue->min_size = default_rx_queues_cfg[i].min_buffers;
    queue->buf_size = default_rx_queues_cfg[i].data_size + 
                      sizeof(MAC_RX_ADDITIONAL_INFO_T) + RX_MAX_MSG_OFFSET;

    hw->rx_data_queues.nof_in_use++;

    ILOG2(GID_HW_MMB, "Rx Queue#d: size = [%d...%d], buffer = %d",
         (int)queue->min_size,
         (int)queue->que_size,
         (int)queue->buf_size);

    total_percentage = total_percentage + default_rx_queues_cfg[i].percentage;
    total_bds        = total_bds + queue->que_size;
  }

  if (total_percentage > 100) {
    ELOG("Incorrect Rx Queues Percentage Table (total=%d)", 
          (int)total_percentage);
    res = MTLK_ERR_PARAMS;
  }
  else if (total_bds != hw->rx_data.nof_bds) {
    ELOG("Incorrect Rx Queues total (%d!=%d)", 
          (int)total_bds,
          (int)hw->rx_data.nof_bds);
    res = MTLK_ERR_UNKNOWN;
  }
  else if (hw->rx_data_queues.nof_in_use) {
    res = MTLK_ERR_OK;
  }

  return res;
}

static int
_mtlk_mmb_notify_firmware (mtlk_hw_t    *hw, 
                           const char   *fname, 
                           const char   *buffer, 
                           unsigned long size)
{
  int res = MTLK_ERR_UNKNOWN;
  if (hw->core)
  {
    mtlk_core_firmware_file_t fw_buffer;

    memset(&fw_buffer, 0, sizeof(fw_buffer));

    strcpy(fw_buffer.fname, fname);
    fw_buffer.content.buffer = buffer;
    fw_buffer.content.size   = (uint32)size;

    res = mtlk_core_set_prop(hw->core, 
                             MTLK_CORE_PROP_FIRMWARE_BIN_BUFFER, 
                             &fw_buffer, 
                             sizeof(fw_buffer));
  }

  return res;
}


static mtlk_hw_data_req_mirror_t *
_mtlk_mmb_get_msg_from_data_pool(mtlk_hw_t *hw)
{ 
  mtlk_hw_data_req_mirror_t *data_req = NULL;
  uint16 nof_used_bds;
  
  mtlk_osal_lock_acquire(&hw->tx_data.lock);
  if (mtlk_dlist_size(&hw->tx_data.free_list)) {
    mtlk_dlist_entry_t *node = mtlk_dlist_pop_front(&hw->tx_data.free_list);
    
    data_req = MTLK_LIST_GET_CONTAINING_RECORD(node, 
                                               mtlk_hw_data_req_mirror_t,
                                               list_entry);
    hw->tx_data_nof_free_bds--;

    nof_used_bds = hw->tx_data.nof_bds - hw->tx_data_nof_free_bds;
    if (nof_used_bds > hw->tx_data_max_used_bds)
      hw->tx_data_max_used_bds = nof_used_bds;

    /* add to the "used" list */
    mtlk_dlist_push_back(&hw->tx_data.used_list, &data_req->list_entry);
  }
  mtlk_osal_lock_release(&hw->tx_data.lock);

  ILOG4(GID_HW_MMB, "got msg %p, %d free msgs", data_req, hw->tx_data_nof_free_bds);

  return data_req;
}

static int
_mtlk_mmb_free_sent_msg_to_data_pool(mtlk_hw_t                 *hw, 
                                     mtlk_hw_data_req_mirror_t *data_req)
{
  mtlk_osal_lock_acquire(&hw->tx_data.lock);
  /* remove from the "used" list */
  mtlk_dlist_remove(&hw->tx_data.used_list,
                    &data_req->list_entry);
  /* add to the "free" list */
  mtlk_dlist_push_back(&hw->tx_data.free_list,
                       &data_req->list_entry);
  hw->tx_data_nof_free_bds++;
  mtlk_osal_lock_release(&hw->tx_data.lock);

  ILOG4(GID_HW_MMB, "%d msg freed, %d free msgs", data_req->index, hw->tx_data_nof_free_bds);

  return hw->tx_data_nof_free_bds;
}

static int
_mtlk_mmb_resp_man_ind (mtlk_hw_t                      *hw, 
                        const mtlk_hw_man_ind_mirror_t *man_ind)
{
  /* RX Man IND BD */
  _mtlk_mmb_memcpy_toio(hw,
                        &hw->rx_man.pas_array[man_ind->index],
                        &man_ind->msg,
                        sizeof(man_ind->msg));

  _mtlk_mmb_send_msg(hw, 
                     ARRAY_MAN_IND,
                     man_ind->index,
                     0);

  return MTLK_ERR_OK;
}

static int
_mtlk_mmb_resp_dbg_ind (mtlk_hw_t                      *hw, 
                        const mtlk_hw_dbg_ind_mirror_t *dbg_ind)
{
  /* RX Dbg IND BD */
  _mtlk_mmb_memcpy_toio(hw,
                        &hw->rx_dbg.pas_array[dbg_ind->index],
                        &dbg_ind->msg,
                        sizeof(dbg_ind->msg));

  _mtlk_mmb_send_msg(hw, 
                     ARRAY_DBG_IND,
                     dbg_ind->index,
                     0);

  return MTLK_ERR_OK;
}

static __INLINE void
_mtlk_mmb_handle_tx_cfm (mtlk_hw_t *hw, 
                         uint8      index,
                         UMI_STATUS status)
{
  mtlk_core_release_tx_data_t data;
  mtlk_hw_data_req_mirror_t  *data_req;
  
  data_req = &hw->tx_data.mirror[index];
  
  if (data_req->dma_addr) { /* NULL keep-alive packets are not mapped */
      mtlk_df_nbuf_unmap_phys_addr(hw->cfg.df,
                                   data_req->nbuf,
                                   data_req->dma_addr,
                                   data_req->size,
                                   MTLK_NBUF_TX);
  }

  data.msg             = HW_MSG_PTR(data_req);
  data.nbuf            = data_req->nbuf;
  data.size            = data_req->size;
  data.access_category = data_req->ac;
  data.status          = status;
  data.resources_free  = 
    (hw->tx_data_nof_free_bds * 100 / hw->tx_data.nof_bds) > 10;

  mtlk_core_release_tx_data(hw->core, &data);
    
  _mtlk_mmb_free_sent_msg_to_data_pool(hw, data_req);
}

static __INLINE int
_mtlk_mmb_handle_dat_ind (mtlk_hw_t *hw, 
                          uint8      msg_index,
                          uint16     msg_info)
{
  int                        res      = MTLK_ERR_UNKNOWN;
  // offset is the 1 LSB of the u16Info. 0 -> offset is 0; 1 -> offset is 2
  uint8                      offset   = (uint8)((msg_info & 0x1) << 1);
  // size is the 15 MSB of the u16Info
  uint32                     data_size= (uint32)(msg_info >> 1);
  mtlk_core_handle_rx_data_t data;
  mtlk_hw_data_ind_mirror_t *data_ind = &hw->rx_data.mirror[msg_index];
  uint8                      que_idx  = 0;
  uint8                     *buffer   = 
    (uint8 *)mtlk_df_nbuf_get_virt_addr(data_ind->nbuf);

  data.info = (MAC_RX_ADDITIONAL_INFO_T *)(buffer + offset);
  offset += sizeof(*data.info); /* skip meta-data header */

  mtlk_df_nbuf_unmap_phys_addr(hw->cfg.df,
                               data_ind->nbuf,
                               data_ind->dma_addr,
                               data_size + offset,
                               MTLK_NBUF_RX);

  que_idx = _mtlk_mmb_rxque_get_next_que_idx(hw, 
                                             data_ind->que_idx, 
                                             data_size);

  hw->rx_data_queues.queue[data_ind->que_idx].que_size--;
  data_ind->que_idx  = que_idx;
  data.nbuf     = data_ind->nbuf;
  data.size     = data_size; /* size of data in buffer */
  data.offset   = offset;

  ILOG6(GID_HW_MMB, "RX IND: al=%d o=%d ro=%d",
       (int)data.size,
       (int)msg_info,
       (int)data.offset);

  res = mtlk_core_handle_rx_data(hw->core, &data);

  if (res == MTLK_ERR_NOT_IN_USE) {
    _mtlk_mmb_nbuf_free(hw, data_ind->nbuf);
  }
  
  data_ind->size = (uint32)hw->rx_data_queues.queue[que_idx].buf_size;
  data_ind->nbuf = _mtlk_mmb_nbuf_alloc(hw, data_ind->size);
  if (__UNLIKELY(data_ind->nbuf == NULL)) { 
    /* Handler failed. Fill requested buffer size and put the 
       RX Data Ind mirror element to Pending list to allow 
       recovery (reallocation) later.
     */
    mtlk_lslist_push(&hw->rx_data_pending.lbufs, &data_ind->pend_l);
    ILOG2(GID_HW_MMB, "RX Data HANDLE_REALLOC failed! Slot#%d (%d bytes) added to pending list", 
         (int)msg_index,
         (int)data_ind->size);
    goto FINISH;
  }

  data_ind->dma_addr = mtlk_df_nbuf_map_to_phys_addr(hw->cfg.df, 
                                                     data_ind->nbuf, 
                                                     data_ind->size,
                                                     MTLK_NBUF_RX);

  hw->rx_data_queues.queue[data_ind->que_idx].que_size++;

  /* Set new payload buffer address */
  _mtlk_mmb_set_rx_payload_addr(hw, msg_index, data_ind->dma_addr);
  
  /* Send response */
  _mtlk_mmb_send_msg(hw, ARRAY_DAT_IND, msg_index, (uint16)que_idx);

FINISH:
  return res;
}

static __INLINE int
_mtlk_mmb_handle_dat_ind_on_stop (mtlk_hw_t *hw, 
                                  uint8      msg_index,
                                  uint16     msg_info)
{
  mtlk_hw_data_ind_mirror_t *data_ind = &hw->rx_data.mirror[msg_index];

  /* Send response */
  _mtlk_mmb_send_msg(hw, ARRAY_DAT_IND, msg_index, data_ind->que_idx);

  return MTLK_ERR_OK;
}

static __INLINE int
_mtlk_mmb_handle_dat_cfm (mtlk_hw_t *hw, 
                          uint8      msg_index,
                          uint16     msg_info)
{
  uint8 idxs[3];
  int   i = 0;

  idxs[0] = msg_index;
  idxs[1] = HIBYTE(msg_info);
  idxs[2] = LOBYTE(msg_info);

  for (; i < ARRAY_SIZE(idxs); i++) {

    if (idxs[i] == DAT_CFM_ID_NONE)
      continue;

    _mtlk_mmb_handle_tx_cfm(hw, idxs[i], UMI_OK);
  }

  return MTLK_ERR_OK;
}

static __INLINE int
_mtlk_mmb_handle_dat_fail_cfm (mtlk_hw_t *obj, 
                               uint8      msg_index,
                               uint16     msg_info)
{
  MTLK_UNREFERENCED_PARAM(msg_info);

  _mtlk_mmb_handle_tx_cfm(obj, msg_index, (UMI_STATUS)msg_info);
  return MTLK_ERR_OK;
}

static __INLINE int
_mtlk_mmb_handle_man_ind (mtlk_hw_t *hw, 
                          uint8      msg_index,
                          uint16     msg_info)
{
  int                       res     = MTLK_ERR_UNKNOWN;
  uint32                    msg_id  = 0;
  mtlk_hw_man_ind_mirror_t *ind_obj = &hw->rx_man.mirror[msg_index];

  MTLK_UNREFERENCED_PARAM(msg_info);

  /* get MAN ind header + data */
  _mtlk_mmb_memcpy_fromio(hw,
                          &ind_obj->msg,
                          &hw->rx_man.pas_array[msg_index],
                          sizeof(ind_obj->msg));

  msg_id = (uint32)MAC_TO_HOST16(ind_obj->msg.sHdr.u16MsgId);

  res = mtlk_core_handle_rx_ctrl(hw->core,
                                 HW_MSG_PTR(ind_obj),
                                 msg_id,
                                 &ind_obj->msg.sMsg);
  switch (res) {
  case MTLK_ERR_OK:
    _mtlk_mmb_resp_man_ind(hw, ind_obj);
    break;
  case MTLK_ERR_PENDING:
    ILOG3(GID_HW_MMB, "MAN message pending: M=%d ID=%d", ind_obj->index, msg_id);
    break;
  default:
    ILOG2(GID_HW_MMB, "WARNING: Ctrl message handling error#%d: M=%d ID=%d", 
         res, ind_obj->index, msg_id);
    _mtlk_mmb_resp_man_ind(hw, ind_obj);
    break;
  }

  return res;
}

static __INLINE void
_mtlk_mmb_dbg_verify_msg_send(mtlk_hw_cm_req_mirror_t *obj)
{
#ifdef MTCFG_DEBUG
  if (1 != mtlk_osal_atomic_inc(&obj->usage_cnt)) {
    ERROR("Message being sent twice, msg id 0x%x",
      MAC_TO_HOST16(obj->msg.pas_data.man.sHdr.u16MsgId));
    MTLK_ASSERT(FALSE);
  }
#else
  MTLK_UNREFERENCED_PARAM(obj);
#endif
}

static __INLINE void
_mtlk_mmb_dbg_verify_msg_recv(mtlk_hw_cm_req_mirror_t *obj)
{
#ifdef MTCFG_DEBUG
  if (0 != mtlk_osal_atomic_dec(&obj->usage_cnt)) {
    ERROR("Message received from HW twice, msg id 0x%x",
      MAC_TO_HOST16(obj->msg.pas_data.man.sHdr.u16MsgId));
    MTLK_ASSERT(FALSE);
  }
#else
  MTLK_UNREFERENCED_PARAM(obj);
#endif
}

static __INLINE void
_mtlk_mmb_dbg_init_msg_verifier(mtlk_hw_cm_req_mirror_t *mirror_array, uint16 array_size)
{
#ifdef MTCFG_DEBUG
  int i;
  for (i = 0; i < array_size; i++) {
    mtlk_osal_atomic_set(&mirror_array[i].usage_cnt, 0);
  }
#else
  MTLK_UNREFERENCED_PARAM(mirror_array);
  MTLK_UNREFERENCED_PARAM(array_size);
#endif
}

static __INLINE int
_mtlk_mmb_handle_man_cfm (mtlk_hw_t *hw, 
                          uint8      msg_index,
                          uint16     msg_info)
{
  mtlk_hw_cm_req_mirror_t *req_obj = &hw->tx_man.mirror[msg_index];

  MTLK_UNREFERENCED_PARAM(msg_info);
  
  _mtlk_mmb_dbg_verify_msg_recv(req_obj);

  /* get MAN ind header + data */
  _mtlk_mmb_memcpy_fromio(hw,
                          &req_obj->msg.pas_data.man,
                          &hw->tx_man.pas_array[msg_index],
                          sizeof(req_obj->msg.pas_data.man));

  /*send it to TXMM */
  txmm_on_cfm(hw, &req_obj->msg); /* Confirmations are handled in TXMM callbacks */

  return MTLK_ERR_OK;
}

static __INLINE int
_mtlk_mmb_handle_dbg_ind (mtlk_hw_t *hw, 
                          uint8      msg_index,
                          uint16     msg_info)
{
  int                       res     = MTLK_ERR_UNKNOWN;
  uint32                    msg_id  = 0;
  mtlk_hw_dbg_ind_mirror_t *ind_obj = &hw->rx_dbg.mirror[msg_index];

  MTLK_UNREFERENCED_PARAM(msg_info);

  /* get DBG ind header + data */
  _mtlk_mmb_memcpy_fromio(hw,
                          &ind_obj->msg,
                          &hw->rx_dbg.pas_array[msg_index],
                          sizeof(ind_obj->msg));
  
  msg_id = (uint32)MAC_TO_HOST16(ind_obj->msg.sHdr.u16MsgId);

  res = mtlk_core_handle_rx_ctrl(hw->core,
                                 HW_MSG_PTR(ind_obj),
                                 msg_id,
                                 &ind_obj->msg.sMsg);
  switch (res) {
  case MTLK_ERR_OK:
    _mtlk_mmb_resp_dbg_ind(hw, ind_obj);
    break;
  case MTLK_ERR_PENDING:
    ILOG3(GID_HW_MMB, "DBG message pending: M=%d ID=%d", ind_obj->index, msg_id);
    break;
  default:
    ILOG2(GID_HW_MMB, "WARNING: Ctrl message handling error#%d: M=%d ID=%d", 
         res, ind_obj->index, msg_id);
    _mtlk_mmb_resp_dbg_ind(hw, ind_obj);
    break;
  }

  return res;
}

static __INLINE int
_mtlk_mmb_handle_dbg_cfm (mtlk_hw_t *hw, 
                          uint8      msg_index,
                          uint16     msg_info)
{
  mtlk_hw_cm_req_mirror_t *req_obj = &hw->tx_dbg.mirror[msg_index];

  MTLK_UNREFERENCED_PARAM(msg_info);
  
  _mtlk_mmb_dbg_verify_msg_recv(req_obj);

  /* get MAN ind header + data */
  _mtlk_mmb_memcpy_fromio(hw,
                          &req_obj->msg.pas_data.dbg,
                          &hw->tx_dbg.pas_array[msg_index],
                          sizeof(req_obj->msg.pas_data.dbg));

  /*send it to TXDM */
  txdm_on_cfm(hw, &req_obj->msg); /* Confirmations are handled in TXMM callbacks */

  return MTLK_ERR_OK;
}

static void
_mtlk_mmb_handle_received_msg (mtlk_hw_t *hw, 
                               uint8      type, 
                               uint8      index, 
                               uint16     info)
{
  switch (type) {
  case ARRAY_DAT_IND:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_DAT);
    if (__LIKELY(!hw->mac_events_stopped))
      _mtlk_mmb_handle_dat_ind(hw, index, info);
    else
      _mtlk_mmb_handle_dat_ind_on_stop(hw, index, info);
    break;
  case ARRAY_DAT_REQ:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_NDAT);
    if (__LIKELY(!hw->mac_events_stopped))
      _mtlk_mmb_handle_dat_cfm(hw, index, info);
    break;
  case ARRAY_MAN_IND:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_NDAT);
    if (__LIKELY(!hw->mac_events_stopped))
      _mtlk_mmb_handle_man_ind(hw, index, info);
    break;
  case ARRAY_MAN_REQ:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_NDAT);
    if (__LIKELY(!hw->mac_events_stopped_completely))
      _mtlk_mmb_handle_man_cfm(hw, index, info);
    break;
  case ARRAY_DBG_IND:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_NDAT);
    if (__LIKELY(!hw->mac_events_stopped))
      _mtlk_mmb_handle_dbg_ind(hw, index, info);
    break;
  case ARRAY_DBG_REQ:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_NDAT);
    if (__LIKELY(!hw->mac_events_stopped_completely))
      _mtlk_mmb_handle_dbg_cfm(hw, index, info);
    break;
  case ARRAY_DAT_REQ_FAIL:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_NDAT);
    if (__LIKELY(!hw->mac_events_stopped_completely))
      _mtlk_mmb_handle_dat_fail_cfm(hw, index, info);
    break;
  case ARRAY_NULL:
  default:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_NDAT);
    ELOG("Wrong message type (%d)", type);
    break;
  }
}

static __INLINE void
_mtlk_mmb_read_ind_or_cfm(mtlk_hw_t *hw)
{
  int processed_count = 0;
#ifdef MTCFG_CPU_STAT
  static const cpu_stat_track_id_e ids[] = {
    CPU_STAT_ID_RX_DAT,
    CPU_STAT_ID_RX_NDAT,
    CPU_STAT_ID_RX_EMPTY
  };
  cpu_stat_track_id_e ts_handle;
#endif

  MTLK_ASSERT(NULL != hw->cfg.ccr);

  while (processed_count++ < hw->bds.ind.size) {
    IND_REQ_BUF_DESC_ELEM elem;

    /* WLS-2479 zero_elem must be 4-aligned.                   */
    /* Considering variety of kernel configuration options     */
    /* related to packing and alignment, the only fool proof   */
    /* way to secure this requirement is to declare it as 4    */
    /* bytes integer type.                                     */
    static const uint32 zero_elem = 0;
    MTLK_ASSERT(sizeof(zero_elem) == sizeof(IND_REQ_BUF_DESC_ELEM));

    CPU_STAT_BEGIN_TRACK_SET(ids, ARRAY_SIZE(ids), &ts_handle);

    CPU_STAT_BEGIN_TRACK(CPU_STAT_ID_RX_HW);
    _mtlk_mmb_pas_get(hw,
                      "get next message",
                      hw->bds.ind.offset + hw->bds.ind.idx * sizeof(elem),
                      &elem,
                      sizeof(elem));

    ILOG5(GID_HW_MMB, "MSG READ: t=%d idx=%d inf=%d",
         (int)elem.u8Type,
         (int)elem.u8Index,
         (int)MAC_TO_HOST16(elem.u16Info));

    if (elem.u8Type == 0) /* NULL type means empty descriptor */
    {
      CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_EMPTY);
      CPU_STAT_END_TRACK_SET(ts_handle);
      CPU_STAT_END_TRACK(CPU_STAT_ID_RX_HW);
      break;
    }

    /***********************************************************************
     * Zero handled BD
     ***********************************************************************/
    _mtlk_mmb_pas_put(hw,
                      "zero received message",
                      hw->bds.ind.offset + hw->bds.ind.idx * sizeof(IND_REQ_BUF_DESC_ELEM),
                      &zero_elem,
                      sizeof(IND_REQ_BUF_DESC_ELEM));
    /***********************************************************************/
    CPU_STAT_END_TRACK(CPU_STAT_ID_RX_HW);

    _mtlk_mmb_handle_received_msg(hw, 
                                  elem.u8Type, 
                                  elem.u8Index, 
                                  MAC_TO_HOST16(elem.u16Info));

    hw->bds.ind.idx++;
    if (hw->bds.ind.idx >= hw->bds.ind.size)
      hw->bds.ind.idx = 0;

    CPU_STAT_END_TRACK_SET(ts_handle);
  }

  mtlk_ccr_enable_interrupts(hw->cfg.ccr);
}

static int
_mtlk_mmb_cause_mac_assert (mtlk_hw_t *hw, uint32 mips_no)
{
  uint32 pas_offset;
  uint32 val = 0;

  if (!hw->mips_ctrl.ext_data.u32DescriptorLocation) {
    return MTLK_ERR_NOT_SUPPORTED;
  }

  pas_offset = 
    hw->mips_ctrl.ext_data.u32DescriptorLocation + 
    MTLK_OFFSET_OF(MIPS_CONTROL_DESCRIPTOR, u32MIPSctrl[mips_no]);

  _mtlk_mmb_pas_readl(hw,
                      "MIPS Ctrl",
                      pas_offset,
                      val);

  MTLK_BFIELD_SET(val, MIPS_CTRL_DO_ASSERT, 1);

  _mtlk_mmb_pas_writel(hw,
                       "MIPS Ctrl",
                       pas_offset,
                       val);

  return MTLK_ERR_OK;
}

static int
_mtlk_mmb_handle_sw_trap (mtlk_hw_t *hw)
{
  int res = MTLK_ERR_UNKNOWN;

  if (hw->mips_ctrl.ext_data.u32DescriptorLocation) {
    /* MIPS Ctrl extension supported => cause MAC assert => 
     * Core will receive and handle it in regular way
     */
    res = _mtlk_mmb_cause_mac_assert(hw, UMIPS);
  }
  else {
    /* MIPS Ctrl extension NOT supported => notify Core => 
     * Core will "simulate" MAC assertion
     */ 
    res = mtlk_core_set_prop(hw->core, 
                             MTLK_CORE_PROP_MAC_STUCK_DETECTED, 
                             NULL, 
                             0);
  }
  
  return res;
}

static int
_mtlk_mmb_process_bcl(mtlk_hw_t *hw, UMI_BCL_REQUEST* preq, int get_req)
{
  int res     = MTLK_ERR_OK;
  int bcl_ctl = 0;
  int i       = 0;

  /* WARNING: _mtlk_mmb_pas_writel can't be used here since both
   * header and data should came in the same endianness
   */
  _mtlk_mmb_pas_put(hw, "Write unit", SHARED_RAM_BCL_ON_EXCEPTION_UNIT, 
                    &preq->Unit, sizeof(preq->Unit));
  _mtlk_mmb_pas_put(hw, "Write size", SHARED_RAM_BCL_ON_EXCEPTION_SIZE, 
                    &preq->Size, sizeof(preq->Size));
  _mtlk_mmb_pas_put(hw, "Write adress", SHARED_RAM_BCL_ON_EXCEPTION_ADDR, 
                    &preq->Address, sizeof(preq->Address));

  if (get_req)
  {
    bcl_ctl = BCL_READ;
  }
  else
  {
    _mtlk_mmb_pas_put(hw, "", SHARED_RAM_BCL_ON_EXCEPTION_DATA, preq->Data, sizeof(preq->Data));
    bcl_ctl = BCL_WRITE;
  }

  _mtlk_mmb_pas_writel(hw, "Write request", SHARED_RAM_BCL_ON_EXCEPTION_CTL, bcl_ctl);

  // need to wait 150ms
  for (i = 0; i < 15; i++)
  {
    mtlk_osal_msleep(10);
    _mtlk_mmb_pas_readl(hw, "Reading BCL request status", SHARED_RAM_BCL_ON_EXCEPTION_CTL, bcl_ctl);
    //      bcl_ctl = le32_to_cpu(bcl_ctl);
    if (bcl_ctl == BCL_IDLE)
      break;
  }

  if (bcl_ctl != BCL_IDLE)
  {
    WARNING("Timeout on BCL request");
    res = MTLK_ERR_TIMEOUT;
  }

  if (get_req)
  {
    _mtlk_mmb_pas_get(hw, "", SHARED_RAM_BCL_ON_EXCEPTION_DATA, preq->Data, sizeof(preq->Data));
  }

  return res;
}

static int
_mtlk_mmb_alloc_and_set_rx_buffer (mtlk_hw_t                 *hw, 
                                   mtlk_hw_data_ind_mirror_t *data_ind,
                                   uint16                     req_size)
{
  int res  = MTLK_ERR_NO_MEM;

  data_ind->nbuf = _mtlk_mmb_nbuf_alloc(hw, req_size);
  if (!data_ind->nbuf) {
    ILOG2(GID_HW_MMB, "WARNING: failed to allocate buffer of %d bytes",
         (int)req_size);
    goto FINISH;
  }

  data_ind->size     = req_size;
  data_ind->dma_addr = mtlk_df_nbuf_map_to_phys_addr(hw->cfg.df,
                                                     data_ind->nbuf,
                                                     req_size,
                                                     MTLK_NBUF_RX);

  ILOG3(GID_HW_MMB, "hbuff: p=0x%p l=%d", 
       data_ind->nbuf,
       (int)req_size);
    
  _mtlk_mmb_set_rx_payload_addr(hw, data_ind->index, data_ind->dma_addr);

  res = MTLK_ERR_OK;

FINISH:
  return res;
}

static void
_mtlk_mmb_recover_rx_buffers (mtlk_hw_t *hw, uint16 max_buffers)
{
  uint16 i = 0;
  for (; i < max_buffers; i++) {
    int                        ares     = MTLK_ERR_UNKNOWN;
    mtlk_lslist_entry_t       *lentry   = mtlk_lslist_pop(&hw->rx_data_pending.lbufs);
    mtlk_hw_data_ind_mirror_t *data_ind = NULL;

    if (!lentry) /* no more pending entries */
      break;

    data_ind = MTLK_LIST_GET_CONTAINING_RECORD(lentry, 
                                               mtlk_hw_data_ind_mirror_t, 
                                               pend_l);

    ares = _mtlk_mmb_alloc_and_set_rx_buffer(hw, 
                                             data_ind, 
                                             (uint16) data_ind->size);
    if (ares != MTLK_ERR_OK) {
      /* Failed again. Put it back to the pending list and stop recovery. */
      mtlk_lslist_push(&hw->rx_data_pending.lbufs, &data_ind->pend_l);
      break;
    }

    /* Succeeded. Send it to MAC as response. */
    hw->rx_data_queues.queue[data_ind->que_idx].que_size++;
    _mtlk_mmb_send_msg(hw, ARRAY_DAT_IND, data_ind->index, (uint16)data_ind->que_idx);
    ILOG2(GID_HW_MMB, "Slot#%d (%d bytes) returned to MAC", 
         (int)data_ind->index,
         (int)data_ind->size);
  }
}

static uint32 __MTLK_IFUNC 
_mtlk_mmb_on_rx_buffs_recovery_timer (mtlk_osal_timer_t *timer, 
                                      mtlk_handle_t      clb_usr_data)
{
  mtlk_hw_t *hw = (mtlk_hw_t*)clb_usr_data;
  
  MTLK_UNREFERENCED_PARAM(timer);
  _mtlk_mmb_recover_rx_buffers(hw, MTLK_MAX_RX_BUFFS_TO_RECOVER);

  return MTLK_RX_BUFFS_RECOVERY_PERIOD;
}

static void
_mtlk_mmb_power_on (mtlk_hw_t *hw)
{
  uint32 val = 0;

  MTLK_ASSERT(NULL != hw->cfg.ccr);

  mtlk_ccr_enable_xo(hw->cfg.ccr);
  mtlk_ccr_release_ctl_from_reset(hw->cfg.ccr);

  hw->mmb->bist_passed = 1;
  if (hw->mmb->cfg.bist_check_permitted) {
    if (!mtlk_ccr_check_bist(hw->cfg.ccr, &val)) {
      LOG0("WARNING: Device self test status: 0x%08lu", (unsigned long)val);
      hw->mmb->bist_passed = 0;
    }
  }

  mtlk_ccr_switch_clock(hw->cfg.ccr);
  mtlk_ccr_boot_from_bus(hw->cfg.ccr);
  mtlk_ccr_exit_debug_mode(hw->cfg.ccr);
  mtlk_ccr_power_on_cpus(hw->cfg.ccr);
}

static int _mtlk_mmb_fw_loader_connect (mtlk_hw_t *hw);
static int _mtlk_mmb_fw_loader_load_file (mtlk_hw_t*   hw,
                                          const uint8* buffer,
                                          uint32       size,
                                          uint8        cpu_num);
static int _mtlk_mmb_fw_loader_disconnect (mtlk_hw_t *hw);

static int
_mtlk_mmb_load_firmware(mtlk_hw_t* hw)
{
  /* Download sta_upper.bin (for STA) or ap_upper.bin (for AP)
   * Here we work off interrupts to call the "load_file" routine
  */
  int                    res     = MTLK_ERR_FW;
  const char            *fw_name = NULL;
  mtlk_mw_bus_file_buf_t fb;
  int                    fb_ok   = 0;

  fw_name  = hw->cfg.ap?MTLK_FRMW_UPPER_AP_NAME:MTLK_FRMW_UPPER_STA_NAME;
  res = mtlk_hw_bus_get_file_buffer(hw->cfg.bus,
                                    fw_name,
                                    &fb);
  if (res != MTLK_ERR_OK) {
    ELOG("can not start (%s is missing)",
         fw_name);
    goto FINISH;
  }

  ILOG2(GID_HW_MMB, "Loading '%s' of %d bytes", fw_name, fb.size);

  fb_ok = 1;

  res = _mtlk_mmb_fw_loader_load_file(hw,
                                      fb.buffer,
                                      fb.size,
                                      CHI_CPU_NUM_UM);
  if (res != MTLK_ERR_OK) {
    ILOG2(GID_HW_MMB, "%s load timed out or interrupted", fw_name);
    goto FINISH;
  }

  _mtlk_mmb_notify_firmware(hw, fw_name, (const char*)fb.buffer, fb.size);
  mtlk_hw_bus_release_file_buffer(hw->cfg.bus, &fb);

  fb_ok    = 0;
  fw_name  = MTLK_FRMW_LOWER_NAME;

  res = mtlk_hw_bus_get_file_buffer(hw->cfg.bus,
                                    fw_name,
                                    &fb);
  if (res != MTLK_ERR_OK) {
    ELOG("can not start (%s is missing)",
         fw_name);
    goto FINISH;
  }

  ILOG2(GID_HW_MMB, "Loading '%s' of %d bytes", fw_name, fb.size);

  fb_ok = 1;

  res = _mtlk_mmb_fw_loader_load_file(hw,
                                      fb.buffer,
                                      fb.size,
                                      CHI_CPU_NUM_LM);
  if (res != MTLK_ERR_OK) {
    ILOG2(GID_HW_MMB, "%s load timed out or interrupted", fw_name);
    goto FINISH;
  }

  _mtlk_mmb_notify_firmware(hw, fw_name, (const char*)fb.buffer, fb.size);

  res = MTLK_ERR_OK;

FINISH:
  if (fb_ok) {
    mtlk_hw_bus_release_file_buffer(hw->cfg.bus, &fb);
  }

  return res;
}

static int
_mtlk_mmb_load_progmodel_from_os (mtlk_hw_t  *hw,
                                  mtlk_core_firmware_file_t *ff)
{
  int res = MTLK_ERR_FW;
  mtlk_mw_bus_file_buf_t fb;

  res = mtlk_hw_bus_get_file_buffer(hw->cfg.bus, ff->fname, &fb);

  if (res != MTLK_ERR_OK) {
    ELOG("can not start (%s is missing)",
         ff->fname);
    return MTLK_ERR_UNKNOWN;
  }

  ff->content.buffer = fb.buffer;
  ff->content.size = fb.size;
  ff->context = fb.context;

  return MTLK_ERR_OK;
}

static int
_mtlk_mmb_load_progmodel_to_hw (mtlk_hw_t   *hw,
                                const mtlk_core_firmware_file_t *ff)
{
  int                    res       = MTLK_ERR_FW;
  unsigned int           loc       = 0;
  mtlk_txmm_msg_t        man_msg;
  mtlk_txmm_data_t      *man_entry = NULL;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, &hw->txmm, NULL);
  if (!man_entry) {
    ELOG("can not get TXMM slot");
    goto FINISH;
  }

  ILOG2(GID_HW_MMB, "%s: size=0x%x, data=0x%p",
       ff->fname, (unsigned)ff->content.size, ff->content.buffer);

  while (loc < ff->content.size) {
    unsigned int left;

    if ((ff->content.size - loc) >  PROGMODEL_CHUNK_SIZE)
      left = PROGMODEL_CHUNK_SIZE;
    else
      left = ff->content.size - loc;

    // XXX: what is 4*5 here? (ant)
    _mtlk_mmb_pas_put(hw, "",  4*5, ff->content.buffer + loc, left);
    ILOG4(GID_HW_MMB, "wrote %d bytes to PAS offset 0x%x\n",
        (int)left, 4*5);

    man_entry->id           = UM_DOWNLOAD_PROG_MODEL_REQ;
    man_entry->payload_size = 0;

    res = mtlk_txmm_msg_send_blocked(&man_msg,
                                     MTLK_PRGMDL_LOAD_CHUNK_TIMEOUT);

    if (res != MTLK_ERR_OK) {
      ELOG("Can't download programming model, timed-out. Err#%d", res);
#if 1
      /* a2k - do not exit - allow to connect to driver through BCL for debugging */
      res = MTLK_ERR_OK;
#endif
      goto FINISH;
    }

    loc+= left;
    ILOG3(GID_HW_MMB, "loc %d, left %d", loc, left);
  }

  ILOG3(GID_HW_MMB, "End program mode");
  _mtlk_mmb_notify_firmware(hw, ff->fname, ff->content.buffer, ff->content.size);

  res = MTLK_ERR_OK;

FINISH:

  if (man_entry)
    mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}

static void
_mtlk_mmb_run_firmware(mtlk_hw_t *hw)
{
  MTLK_ASSERT(NULL != hw->cfg.ccr);

  mtlk_ccr_disable_interrupts(hw->cfg.ccr);
  mtlk_ccr_switch_to_iram_boot(hw->cfg.ccr);
  mtlk_ccr_release_cpus_reset(hw->cfg.ccr);
}

static void
_mtlk_mmb_parse_chi_extensions (mtlk_hw_t *hw)
{
  uint32 offset = CHI_ADDR + sizeof(hw->chi_data);

  ILOG2(GID_HW_MMB, "offset = %d, CHI_ADDR = 0x%08x, sizeof(hw->chi_data) = %lu",offset,CHI_ADDR,(unsigned long)sizeof(hw->chi_data));

  while (1) {
    VECTOR_AREA_EXTENSION_HEADER ext_hdr;

    _mtlk_mmb_pas_get(hw,
                      "CHI Vector Extension Header",
                      offset,
                      &ext_hdr,
                      sizeof(ext_hdr));

    ILOG2(GID_HW_MMB, "HOST_EXTENSION_MAGIC = 0x%08x",HOST_EXTENSION_MAGIC);
    ILOG2(GID_HW_MMB, "ext_hdr.u32ExtensionMagic = 0x%08x",ext_hdr.u32ExtensionMagic);
    
    if (MAC_TO_HOST32(ext_hdr.u32ExtensionMagic) != HOST_EXTENSION_MAGIC)
      break; /* No more extensions in CHI Area */

    ext_hdr.u32ExtensionID       = MAC_TO_HOST32(ext_hdr.u32ExtensionID);
    ext_hdr.u32ExtensionDataSize = MAC_TO_HOST32(ext_hdr.u32ExtensionDataSize);
    
    ILOG2(GID_HW_MMB, "ext_hdr.u32ExtensionID = 0x%08x",ext_hdr.u32ExtensionID);
    ILOG2(GID_HW_MMB, "ext_hdr.u32ExtensionDataSize = %d",ext_hdr.u32ExtensionDataSize);

    offset += sizeof(ext_hdr); /* skip to extension data */

    switch (ext_hdr.u32ExtensionID) {
    case VECTOR_AREA_CALIBR_EXTENSION_ID:
      if (ext_hdr.u32ExtensionDataSize == sizeof(VECTOR_AREA_CALIBR_EXTENSION_DATA)) {
        _mtlk_mmb_pas_get(hw,
                          "CHI Vector Extension Data",
                          offset,
                          &hw->calibr.ext_data,
                          sizeof(hw->calibr.ext_data));
      }
      else {
        WARNING("Invalid Calibration Extension Data size (%d != %d)",
                (int)ext_hdr.u32ExtensionDataSize,
                (int)sizeof(VECTOR_AREA_CALIBR_EXTENSION_DATA));
        memset(&hw->calibr, 0, sizeof(hw->calibr));
      }
      break;
    case VECTOR_AREA_MIPS_CONTROL_EXTENSION_ID:
      if (ext_hdr.u32ExtensionDataSize == sizeof(VECTOR_AREA_MIPS_CONTROL_EXTENSION_DATA)) {
        _mtlk_mmb_pas_get(hw,
                          "CHI Vector Extension Data",
                          offset,
                          &hw->mips_ctrl.ext_data,
                          sizeof(hw->mips_ctrl.ext_data));
      }
      else {
        WARNING("Invalid MIPS Control Extension Data size (%d != %d)",
                (int)ext_hdr.u32ExtensionDataSize,
                (int)sizeof(VECTOR_AREA_MIPS_CONTROL_EXTENSION_DATA));
        memset(&hw->mips_ctrl, 0, sizeof(hw->mips_ctrl));
      }
      break;
    default:
      ILOG2(GID_HW_MMB, "Unrecognized extension#%d", (int)ext_hdr.u32ExtensionID);
      break;
    }

    offset += ext_hdr.u32ExtensionDataSize; /* skip to next extension */
  }
}

static int
_mtlk_mmb_wait_chi_magic(mtlk_hw_t *hw)
{
  int res = MTLK_ERR_HW;

#ifdef MTCFG_USE_INTERRUPT_POLLING
  mtlk_osal_timestamp_t start_ts = mtlk_osal_timestamp();
#endif

  MTLK_ASSERT(NULL != hw->cfg.ccr);

  /* Check for the magic value and then get the base address and length of the CHI area */

  MTLK_HW_INIT_EVT_RESET(hw);
  mtlk_ccr_enable_interrupts(hw->cfg.ccr);

  res = MTLK_HW_INIT_EVT_WAIT(hw, MTLK_CHI_MAGIC_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG("No interrupts while waiting for CHI magic");
    return res;
  }

#ifdef MTCFG_USE_INTERRUPT_POLLING
  do
  {
#endif

    _mtlk_mmb_pas_get(hw,
                      "CHI Vector Area",
                      CHI_ADDR,
                      &hw->chi_data,
                      sizeof(hw->chi_data));

    if (MAC_TO_HOST32(hw->chi_data.u32Magic) == HOST_MAGIC) {
      _mtlk_mmb_parse_chi_extensions(hw);
      return MTLK_ERR_OK;
    }

#ifdef MTCFG_USE_INTERRUPT_POLLING
    mtlk_osal_msleep(100);
  }
  while( mtlk_osal_time_passed_ms(start_ts) <= MTLK_CHI_MAGIC_TIMEOUT );
#endif

  ELOG("Bad Magic in check_for_chi_magic (0x%08x)",
       MAC_TO_HOST32(hw->chi_data.u32Magic));
  return MTLK_ERR_HW;
}

static int
_mtlk_mmb_send_ready_blocked (mtlk_hw_t *hw)
{
  int               res       = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry;
  READY_REQ*        ready_req = NULL;
  uint16            nque      = 0;

  MTLK_ASSERT(NULL != hw->cfg.ccr);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, &hw->txmm, NULL);
  if (!man_entry) {
    ELOG("can not get TXMM slot");
    goto FINISH;
  }

  man_entry->id           = UM_MAN_READY_REQ;
  man_entry->payload_size = 0;

  ready_req = (READY_REQ *) man_entry->payload;

  memset(ready_req, 0, sizeof(*ready_req));


  for (nque = 0; nque < hw->rx_data_queues.nof_in_use; nque++) {
    /* NOTE: we should pass queues to MAC (fill the READY request):
       - arrenged by size (growing)
       - with whole buffer sizes (including max offset, header size etc.) 
    */
    ready_req->asQueueParams[nque].u16QueueSize  = 
      HOST_TO_MAC16(hw->rx_data_queues.queue[nque].que_size);
    ready_req->asQueueParams[nque].u16BufferSize = 
      HOST_TO_MAC16(hw->rx_data_queues.queue[nque].buf_size);
  }

  mtlk_ccr_enable_interrupts(hw->cfg.ccr);
  res = mtlk_txmm_msg_send_blocked(&man_msg, 
                                   MTLK_READY_CFM_TIMEOUT);

  if (res != MTLK_ERR_OK) {
    ELOG("MAC initialization failed (err=%d)", res);
    goto FINISH;
  }

  res = MTLK_ERR_OK;

FINISH:
  if (man_entry)
    mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}

static void
_mtlk_mmb_clenup_man_req_bdr (mtlk_hw_t *hw)
{
  MTLK_CLEANUP_ADVANCED_BDR(hw, tx_man);
  MTLK_CLEANUP_BASIC_BDR(hw, tx_man);
}

static void
_mtlk_mmb_clenup_man_ind_bdr (mtlk_hw_t *hw)
{
  MTLK_CLEANUP_BASIC_BDR(hw, rx_man);
}

static void
_mtlk_mmb_clenup_dbg_req_bdr (mtlk_hw_t *hw)
{
  MTLK_CLEANUP_ADVANCED_BDR(hw, tx_dbg);
  MTLK_CLEANUP_BASIC_BDR(hw, tx_dbg);
}

static void
_mtlk_mmb_clenup_dbg_ind_bdr (mtlk_hw_t *hw)
{
  MTLK_CLEANUP_BASIC_BDR(hw, rx_dbg);
}

static void
_mtlk_mmb_clenup_data_req_bdr (mtlk_hw_t *hw)
{
  _mtlk_mmb_free_unconfirmed_tx_buffers(hw);
  MTLK_CLEANUP_ADVANCED_BDR(hw, tx_data);
  MTLK_CLEANUP_BASIC_BDR(hw, tx_data);
}

static void
_mtlk_mmb_clenup_data_ind_bdr (mtlk_hw_t *hw)
{
  MTLK_CLEANUP_BASIC_BDR(hw, rx_data);
}

#define LOG_CHI_AREA(d)                                    \
  ILOG2(GID_HW_MMB, "CHI: " #d ": is=0x%x in=%d rs=0x%x rn=%d", \
       MAC_TO_HOST32(hw->chi_data.d.u32IndStartOffset),   \
       MAC_TO_HOST32(hw->chi_data.d.u32IndNumOfElements), \
       MAC_TO_HOST32(hw->chi_data.d.u32ReqStartOffset),   \
       MAC_TO_HOST32(hw->chi_data.d.u32ReqNumOfElements))

static int
_mtlk_mmb_prepare_man_req_bdr(mtlk_hw_t *hw)
{
  /* Management Requests BD initialization */
  MTLK_INIT_BASIC_BDR(hw,
                      tx_man,
                      mtlk_hw_cm_req_mirror_t,
                      SHRAM_MAN_MSG,
                      hw->chi_data.sMAN.u32ReqNumOfElements,
                      hw->chi_data.sMAN.u32ReqStartOffset);
  if (!hw->tx_man.mirror) {
    return MTLK_ERR_NO_MEM;
  }
  MTLK_INIT_ADVANCED_BDR(hw, tx_man);

  _mtlk_mmb_dbg_init_msg_verifier(hw->tx_man.mirror, hw->tx_man.nof_bds);

  return MTLK_ERR_OK;
}

static int
_mtlk_mmb_prepare_man_ind_bdr(mtlk_hw_t *hw)
{
  /* Management Indications BD initialization */
  MTLK_INIT_BASIC_BDR(hw, 
                      rx_man,
                      mtlk_hw_man_ind_mirror_t,
                      SHRAM_MAN_MSG,
                      hw->chi_data.sMAN.u32IndNumOfElements,
                      hw->chi_data.sMAN.u32IndStartOffset);

  return hw->rx_man.mirror ? MTLK_ERR_OK : MTLK_ERR_NO_MEM;
}

static int
_mtlk_mmb_prepare_dbg_req_bdr(mtlk_hw_t *hw)
{
  /* DBG Requests BD initialization */
  MTLK_INIT_BASIC_BDR(hw, 
                      tx_dbg,
                      mtlk_hw_cm_req_mirror_t,
                      SHRAM_DBG_MSG,
                      hw->chi_data.sDBG.u32ReqNumOfElements,
                      hw->chi_data.sDBG.u32ReqStartOffset);
  if (!hw->tx_dbg.mirror) {
    return MTLK_ERR_NO_MEM;
  }
  MTLK_INIT_ADVANCED_BDR(hw, tx_dbg);

  _mtlk_mmb_dbg_init_msg_verifier(hw->tx_dbg.mirror, hw->tx_dbg.nof_bds);

  return MTLK_ERR_OK;
}

static int
_mtlk_mmb_prepare_dbg_ind_bdr(mtlk_hw_t *hw)
{
  /* DBG Indications BD initialization */
  MTLK_INIT_BASIC_BDR(hw, 
                      rx_dbg,
                      mtlk_hw_dbg_ind_mirror_t,
                      SHRAM_DBG_MSG,
                      hw->chi_data.sDBG.u32IndNumOfElements,
                      hw->chi_data.sDBG.u32IndStartOffset);

  return hw->rx_dbg.mirror ? MTLK_ERR_OK : MTLK_ERR_NO_MEM;
}

static int
_mtlk_mmb_prepare_data_req_bdr(mtlk_hw_t *hw)
{
  /* Data Requests BD initialization */
  MTLK_INIT_BASIC_BDR(hw, 
                      tx_data,
                      mtlk_hw_data_req_mirror_t,
                      SHRAM_DAT_REQ_MSG,
                      hw->chi_data.sDAT.u32ReqNumOfElements,
                      hw->chi_data.sDAT.u32ReqStartOffset);
  if (!hw->tx_data.mirror) {
    return MTLK_ERR_NO_MEM;
  }
  MTLK_INIT_ADVANCED_BDR(hw, tx_data);
  hw->tx_data_nof_free_bds = hw->tx_data.nof_bds;
  hw->tx_data_max_used_bds = 0;

  return MTLK_ERR_OK;
}

static int
_mtlk_mmb_prepare_data_ind_bdr(mtlk_hw_t *hw)
{
  /* Data Indications BD initialization */
  MTLK_INIT_BASIC_BDR(hw, 
                      rx_data,
                      mtlk_hw_data_ind_mirror_t,
                      RXDAT_IND_MSG_DESC,
                      hw->chi_data.sDAT.u32IndNumOfElements,
                      hw->chi_data.sDAT.u32IndStartOffset);
  
  return hw->rx_data.mirror ? MTLK_ERR_OK : MTLK_ERR_NO_MEM;
}

static void
_mtlk_mmb_free_unconfirmed_tx_buffers(mtlk_hw_t *hw)                  
{
  ILOG3(GID_HW_MMB, "Freeing unconfirmed TX buffers");

  while (TRUE) {
    mtlk_core_release_tx_data_t data;
    mtlk_hw_data_req_mirror_t  *data_req;
    mtlk_dlist_entry_t         *node = 
      mtlk_dlist_pop_front(&hw->tx_data.used_list);

    if (!node) {
      break; /* No more buffers */
    }

    data_req = MTLK_LIST_GET_CONTAINING_RECORD(node, 
                                               mtlk_hw_data_req_mirror_t,
                                               list_entry);

    if (data_req->dma_addr) {
      mtlk_df_nbuf_unmap_phys_addr(hw->cfg.df,
                                   data_req->nbuf,
                                   data_req->dma_addr,
                                   data_req->size,
                                   MTLK_NBUF_TX);
    }

    data.msg             = HW_MSG_PTR(data_req);
    data.nbuf            = data_req->nbuf;
    data.size            = data_req->size;
    data.access_category = data_req->ac;
    data.status          = UMI_OK;
    data.resources_free  = 0;

    mtlk_core_release_tx_data(hw->core, &data);
  }
}

static void
_mtlk_mmb_free_preallocated_rx_buffers (mtlk_hw_t *hw)
{
  uint16 i = 0;

  for (i = 0; i < hw->rx_data.nof_bds; i++) {
    mtlk_hw_data_ind_mirror_t *data_ind = &hw->rx_data.mirror[i];

    if (!data_ind->nbuf)
      continue;

    mtlk_df_nbuf_unmap_phys_addr(hw->cfg.df,
                                 data_ind->nbuf,
                                 data_ind->dma_addr,
                                 data_ind->size,
                                 MTLK_NBUF_RX);

    _mtlk_mmb_nbuf_free(hw, data_ind->nbuf);
  }
}

static void
_mtlk_mmb_cleanup_extensions (mtlk_hw_t *hw)
{
  if (hw->calibr.dma_addr) {
    mtlk_unmap_phys_addr(hw->cfg.df,
                         hw->calibr.dma_addr,
                         hw->calibr.ext_data.u32BufferRequestedSize,
                         PCI_DMA_FROMDEVICE);
  }

  if (hw->calibr.buffer) {
    mtlk_osal_mem_free(hw->calibr.buffer);
  }

  memset(&hw->calibr, 0, sizeof(hw->calibr));
}

static int
_mtlk_mmb_prealloc_rx_buffers (mtlk_hw_t *hw)
{
  uint8  nque      = 0;
  uint16 bd_index  = 0;

  /* TODO: alloc buffers in the reverse order (bigger first) */
  for (nque = 0; nque < hw->rx_data_queues.nof_in_use; nque++) {
    mtlk_hw_rx_queue_t *queue = &hw->rx_data_queues.queue[nque];
    uint16              i     = 0;
    int                 ares  = MTLK_ERR_OK;
    for (i = 0; i < queue->que_size ; i++) {
      mtlk_hw_data_ind_mirror_t *data_ind = &hw->rx_data.mirror[bd_index + i];

      data_ind->que_idx = nque;
      
      if (ares == MTLK_ERR_OK) {
         /* No holes in BDR pointers yet: 1st allocation, 
            or all the previous allocations succeeded 
         */
        ares = _mtlk_mmb_alloc_and_set_rx_buffer(hw, 
                                                 data_ind, 
                                                 queue->buf_size);
        if (ares != MTLK_ERR_OK) {
          /* BRD pointer hole produced here (NULL-pointer) */
           ILOG2(GID_HW_MMB, "WARNING: Can't preallocate buffer of %d bytes.",
               (int)queue->buf_size);
        }
      }

      if (ares != MTLK_ERR_OK) {
        /* Some allocation has failed.
           The rest of allocations for this queue will be inserted to
           pending list, since the MAC can't hanlde holes and runs untill
           1st NULL.
           Then we'll try to recover (reallocate)
         */
        data_ind->size = queue->buf_size;
        mtlk_lslist_push(&hw->rx_data_pending.lbufs, &data_ind->pend_l);
      }
    }

    ILOG2(GID_HW_MMB, "Total %d from %d buffers allocated for queue#%d (%d bytes each)",
         (int)i,
         (int)queue->que_size,
         (int)nque,
         (int)queue->buf_size);

    bd_index = bd_index + queue->que_size;
  }

  return MTLK_ERR_OK;
}

static int
_mtlk_mmb_send_msg (mtlk_hw_t *hw, 
                    uint8      msg_type,
                    uint8      msg_index,
                    uint16     msg_info)
{
  IND_REQ_BUF_DESC_ELEM elem;
  mtlk_handle_t         lock_val;

  MTLK_ASSERT(NULL != hw->cfg.ccr);

  elem.u8Type  = msg_type;
  elem.u8Index = msg_index;
  elem.u16Info = HOST_TO_MAC16(msg_info);

  ILOG5(GID_HW_MMB, "MSG WRITE: t=%d idx=%d inf=%d", 
       (int)msg_type, (int)msg_index, (int)msg_info);

  lock_val = mtlk_osal_lock_acquire_irq(&hw->reg_lock);

  _mtlk_mmb_pas_put(hw, 
                    "new BD descriptor",
                    hw->bds.req.offset + (hw->bds.req.idx * sizeof(elem)), /* DB Array [BD Idx] */
                    &elem,
                    sizeof(elem));

  hw->bds.req.idx++;
  if (hw->bds.req.idx >= hw->bds.req.size)
    hw->bds.req.idx = 0;  

  mtlk_ccr_initiate_doorbell_inerrupt(hw->cfg.ccr);

  mtlk_osal_lock_release_irq(&hw->reg_lock, lock_val);

  return MTLK_ERR_OK;
}

static int
_mtlk_mmb_send_sw_reset_mac_req(mtlk_hw_t *hw)
{
  int               res       = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, &hw->txmm, NULL);
  if (!man_entry) {
    ELOG("Can't send request to MAC due to the lack of MAN_MSG");
    goto FINISH;
  }

  man_entry->id           = UM_MAN_SW_RESET_MAC_REQ;
  man_entry->payload_size = 0;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_SW_RESET_CFM_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG("Can't send sw reset request to MAC, timed-out");
    goto FINISH;
  }

  res = MTLK_ERR_OK;

FINISH:
  if (man_entry)
    mtlk_txmm_msg_cleanup(&man_msg);
  return res;
}

static void
_mtlk_mmb_cleanup_reset_mac(mtlk_hw_t *hw)
{
  MTLK_ASSERT(NULL != hw->cfg.ccr);

  mtlk_ccr_unswitch_clock(hw->cfg.ccr);
  mtlk_ccr_clear_boot_from_bus(hw->cfg.ccr);
  mtlk_ccr_put_ctl_to_reset(hw->cfg.ccr);
  mtlk_ccr_disable_xo(hw->cfg.ccr);
}

static void
_mtlk_mmb_stop_events_completely(mtlk_hw_t *hw)
{
  /* NOTE: mac_events_stopped must be also set here to avoid additional checks
   * in _mtlk_mmb_handle_received_msg() (hw->mac_events_stopped || hw->mac_events_stopped_completely) */
  hw->mac_events_stopped            = 1;
  hw->mac_events_stopped_completely = 1;
}

static void
_mtlk_mmb_reset_all_events(mtlk_hw_t *hw)
{
    hw->mac_events_stopped            = 0;
    hw->mac_events_stopped_completely = 0;
}

static int
_mtlk_mmb_init_extensions (mtlk_hw_t *hw)
{
  int res = MTLK_ERR_OK;

  /****************************************************************
   * Calibration Cache Extension
   ****************************************************************/
  if (hw->calibr.ext_data.u32BufferRequestedSize) {
    hw->calibr.ext_data.u32BufferRequestedSize = 
      MAC_TO_HOST32(hw->calibr.ext_data.u32BufferRequestedSize);
    hw->calibr.ext_data.u32DescriptorLocation = 
      MAC_TO_HOST32(hw->calibr.ext_data.u32DescriptorLocation);

    ILOG2(GID_HW_MMB, "DEBUG: Calibration Cache req_size=%d location=0x%08x",
         (int)hw->calibr.ext_data.u32BufferRequestedSize,
         hw->calibr.ext_data.u32DescriptorLocation);
 
    hw->calibr.buffer = mtlk_osal_mem_dma_alloc(hw->calibr.ext_data.u32BufferRequestedSize,
                                                 MTLK_MEM_TAG_EXTENSION);

    ILOG2(GID_HW_MMB, "hw->calibr.buffer = 0x%p",hw->calibr.buffer);
    if (hw->calibr.buffer) {
      hw->calibr.dma_addr = mtlk_map_to_phys_addr(hw->cfg.df, 
                                                  hw->calibr.buffer,
                                                  hw->calibr.ext_data.u32BufferRequestedSize,
                                                  PCI_DMA_FROMDEVICE);

      ILOG2(GID_HW_MMB, "hw->calibr.dma_addr = 0x%08x",hw->calibr.dma_addr);
 
      _mtlk_mmb_pas_writel(hw, "Calibration Cache buffer pointer",
        hw->calibr.ext_data.u32DescriptorLocation, hw->calibr.dma_addr);


      ILOG2(GID_HW_MMB, "DEBUG: Calibration Cache buffer pointer (v=0x%p, d=0x%08x, s=%d) written to 0x%08x",
           hw->calibr.buffer,
           hw->calibr.dma_addr,
           (int)hw->calibr.ext_data.u32BufferRequestedSize,
           hw->calibr.ext_data.u32DescriptorLocation);
    }
    else {
      WARNING("Can't allocate Calibration buffer of %d bytes",
              (int)hw->calibr.ext_data.u32BufferRequestedSize);
      res = MTLK_ERR_NO_MEM;
    }
  }
  /****************************************************************/

  /****************************************************************
   * MIPS Control Extension
   ****************************************************************/
  if (hw->mips_ctrl.ext_data.u32DescriptorLocation) {
    hw->mips_ctrl.ext_data.u32DescriptorLocation = 
      MAC_TO_HOST32(hw->mips_ctrl.ext_data.u32DescriptorLocation);
    ILOG2(GID_HW_MMB, "MIPS Ctrl Descriptor PAS offset: 0x%x", 
         hw->mips_ctrl.ext_data.u32DescriptorLocation);
  }
  /****************************************************************/

  return res;
}

MTLK_INIT_STEPS_LIST_BEGIN(hw_mmb_card)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_mmb_card, HW_RX_DATA_LIST)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_mmb_card, HW_REQ_BD_LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_mmb_card, HW_INIT_EVT)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_mmb_card, HW_RX_PEND_TIMER)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_mmb_card, HW_TXMM)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_mmb_card, HW_TXDM)
MTLK_INIT_INNER_STEPS_BEGIN(hw_mmb_card)
MTLK_INIT_STEPS_LIST_END(hw_mmb_card);


MTLK_START_STEPS_LIST_BEGIN(hw_mmb_card)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_POWER_ON)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_LDR_CONNECT)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_LOAD_FIRMWARE)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_LDR_DISCONNECT)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_RUN_FIRMWARE)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_WAIT_CHI_MAGIC)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_MAN_REQ_BDR)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_MAN_IND_BDR)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_DBG_REQ_BDR)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_DBG_IND_BDR)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_DAT_REQ_BDR)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_DAT_IND_BDR)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_RX_QUEUES)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_RX_BUFFERS)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_EXTENSIONS)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_TXMM)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_TXDM)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_SEND_READY)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_RX_PEND_TIMER)
MTLK_START_INNER_STEPS_BEGIN(hw_mmb_card)
MTLK_START_STEPS_LIST_END(hw_mmb_card);

void __MTLK_IFUNC 
mtlk_hw_mmb_stop_card(mtlk_hw_t *hw)
{
  int res;
  uint32 mac_soft_reset_enable = 0;
  int exception = (hw->state == MTLK_HW_STATE_EXCEPTION) || 
                  (hw->state == MTLK_HW_STATE_APPFATAL);

  MTLK_ASSERT(NULL != hw->cfg.ccr);

#ifdef MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS
  mtlk_osal_lock_acquire(&hw_tsf_info.lock);
  if (hw_tsf_info.card == hw) {
    hw_tsf_info.started = FALSE;
  }
  mtlk_osal_lock_release(&hw_tsf_info.lock);
#endif

  hw->state = MTLK_HW_STATE_UNLOADING;

  if (hw->core)
  {
    res = mtlk_core_get_prop(hw->core,
                             MTLK_CORE_PROP_MAC_SW_RESET_ENABLED,
                             &mac_soft_reset_enable,
                             sizeof(mac_soft_reset_enable));
    if (res != MTLK_ERR_OK)
      mac_soft_reset_enable = 0;
  }

  MTLK_STOP_BEGIN(hw_mmb_card, MTLK_OBJ_PTR(hw))
    MTLK_STOP_STEP(hw_mmb_card, HW_RX_PEND_TIMER, MTLK_OBJ_PTR(hw), 
                   mtlk_osal_timer_cancel_sync, (&hw->rx_data_pending.timer));
    MTLK_STOP_STEP(hw_mmb_card, HW_SEND_READY, MTLK_OBJ_PTR(hw), MTLK_NOACTION,());

    if (hw->mac_reset_logic_initialized && !exception) {
      ILOG3(GID_HW_MMB, "Calling _mtlk_pci_send_sw_reset_mac_req");
      if (_mtlk_mmb_send_sw_reset_mac_req(hw) != MTLK_ERR_OK) {
        hw->mac_reset_logic_initialized = FALSE;
      }
    } 
    else if (exception && (mac_soft_reset_enable == 0)) {
      hw->mac_reset_logic_initialized = FALSE;
    }

    mtlk_ccr_disable_interrupts(hw->cfg.ccr);
    _mtlk_mmb_stop_events_completely(hw);
    hw->isr_type = MTLK_ISR_NONE;

    MTLK_STOP_STEP(hw_mmb_card, HW_TXDM, MTLK_OBJ_PTR(hw), 
                   mtlk_txmm_stop, (&hw->txdm));
    MTLK_STOP_STEP(hw_mmb_card, HW_TXMM, MTLK_OBJ_PTR(hw), 
                   mtlk_txmm_stop, (&hw->txmm));
    MTLK_STOP_STEP(hw_mmb_card, HW_EXTENSIONS, MTLK_OBJ_PTR(hw), 
                   _mtlk_mmb_cleanup_extensions, (hw));
    MTLK_STOP_STEP(hw_mmb_card, HW_RX_BUFFERS, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_free_preallocated_rx_buffers, (hw));
    MTLK_STOP_STEP(hw_mmb_card, HW_RX_QUEUES, MTLK_OBJ_PTR(hw), MTLK_NOACTION, ());

    MTLK_STOP_STEP(hw_mmb_card, HW_DAT_IND_BDR, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_clenup_data_ind_bdr, (hw));
    MTLK_STOP_STEP(hw_mmb_card, HW_DAT_REQ_BDR, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_clenup_data_req_bdr, (hw));
    MTLK_STOP_STEP(hw_mmb_card, HW_DBG_IND_BDR, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_clenup_dbg_ind_bdr, (hw));
    MTLK_STOP_STEP(hw_mmb_card, HW_DBG_REQ_BDR, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_clenup_dbg_req_bdr, (hw));
    MTLK_STOP_STEP(hw_mmb_card, HW_MAN_IND_BDR, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_clenup_man_ind_bdr, (hw));
    MTLK_STOP_STEP(hw_mmb_card, HW_MAN_REQ_BDR, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_clenup_man_req_bdr, (hw));

    mtlk_ccr_put_cpus_to_reset(hw->cfg.ccr);

#ifndef MTCFG_NO_FW_RESET_ON_STOP
    if (hw->mac_reset_logic_initialized) {
      ILOG3(GID_HW_MMB, "reset the mac");
      _mtlk_mmb_cleanup_reset_mac(hw);
    }
#endif

    MTLK_STOP_STEP(hw_mmb_card, HW_WAIT_CHI_MAGIC, MTLK_OBJ_PTR(hw), MTLK_NOACTION, ());
    MTLK_STOP_STEP(hw_mmb_card, HW_RUN_FIRMWARE, MTLK_OBJ_PTR(hw), MTLK_NOACTION, ());
    MTLK_STOP_STEP(hw_mmb_card, HW_LDR_DISCONNECT, MTLK_OBJ_PTR(hw), MTLK_NOACTION, ());
    MTLK_STOP_STEP(hw_mmb_card, HW_LOAD_FIRMWARE, MTLK_OBJ_PTR(hw), MTLK_NOACTION, ());
    MTLK_STOP_STEP(hw_mmb_card, HW_LDR_CONNECT, MTLK_OBJ_PTR(hw), MTLK_NOACTION, ());
    MTLK_STOP_STEP(hw_mmb_card, HW_POWER_ON, MTLK_OBJ_PTR(hw), MTLK_NOACTION, ());
  MTLK_STOP_END(hw_mmb_card, MTLK_OBJ_PTR(hw))
}

void __MTLK_IFUNC 
mtlk_hw_mmb_cleanup_card(mtlk_hw_t *hw)
{
  MTLK_CLEANUP_BEGIN(hw_mmb_card, MTLK_OBJ_PTR(hw))
    MTLK_CLEANUP_STEP(hw_mmb_card, HW_TXDM, MTLK_OBJ_PTR(hw),
                      mtlk_txmm_cleanup, (&hw->txdm));
    MTLK_CLEANUP_STEP(hw_mmb_card, HW_TXMM, MTLK_OBJ_PTR(hw),
                      mtlk_txmm_cleanup, (&hw->txmm));
    MTLK_CLEANUP_STEP(hw_mmb_card, HW_RX_PEND_TIMER, MTLK_OBJ_PTR(hw),
                      mtlk_osal_timer_cleanup, (&hw->rx_data_pending.timer));
    MTLK_CLEANUP_STEP(hw_mmb_card, HW_INIT_EVT, MTLK_OBJ_PTR(hw),
                      MTLK_HW_INIT_EVT_CLEANUP, (hw));
    MTLK_CLEANUP_STEP(hw_mmb_card, HW_REQ_BD_LOCK, MTLK_OBJ_PTR(hw),
                      mtlk_osal_lock_cleanup, (&hw->reg_lock));
    MTLK_CLEANUP_STEP(hw_mmb_card, HW_RX_DATA_LIST, MTLK_OBJ_PTR(hw),
                      mtlk_lslist_cleanup, (&hw->rx_data_pending.lbufs));
  MTLK_CLEANUP_END(hw_mmb_card, MTLK_OBJ_PTR(hw));

  hw->core = NULL;
}

int __MTLK_IFUNC 
mtlk_hw_mmb_init_card(mtlk_hw_t   *hw,
                      mtlk_core_t *core,
                      mtlk_ccr_t *ccr)
{
  hw->cfg.ccr = ccr;
  hw->core  = core;
  hw->state = MTLK_HW_STATE_INITIATING;
 
  MTLK_INIT_TRY(hw_mmb_card, MTLK_OBJ_PTR(hw))
    MTLK_INIT_STEP_VOID(hw_mmb_card, HW_RX_DATA_LIST, MTLK_OBJ_PTR(hw),
                        mtlk_lslist_init, (&hw->rx_data_pending.lbufs));
    MTLK_INIT_STEP(hw_mmb_card, HW_REQ_BD_LOCK, MTLK_OBJ_PTR(hw),
                   mtlk_osal_lock_init, (&hw->reg_lock));
    MTLK_INIT_STEP(hw_mmb_card, HW_INIT_EVT, MTLK_OBJ_PTR(hw),
                   MTLK_HW_INIT_EVT_INIT, (hw));
    MTLK_INIT_STEP(hw_mmb_card, HW_RX_PEND_TIMER, MTLK_OBJ_PTR(hw),
                   mtlk_osal_timer_init, (&hw->rx_data_pending.timer,
                                          _mtlk_mmb_on_rx_buffs_recovery_timer,
                                          HANDLE_T(hw)));
    MTLK_INIT_STEP(hw_mmb_card, HW_TXMM, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_txmm_init, (hw));
    MTLK_INIT_STEP(hw_mmb_card, HW_TXDM, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_txdm_init, (hw));

    _mtlk_mmb_reset_all_events(hw);

  MTLK_INIT_FINALLY(hw_mmb_card, MTLK_OBJ_PTR(hw));
  MTLK_INIT_RETURN(hw_mmb_card, MTLK_OBJ_PTR(hw), mtlk_hw_mmb_cleanup_card, (hw));
}

int __MTLK_IFUNC 
mtlk_hw_mmb_start_card(mtlk_hw_t *hw)
{
  MTLK_START_TRY(hw_mmb_card, MTLK_OBJ_PTR(hw))

#ifdef MTCFG_NO_FW_RESET_ON_STOP
    /* If MAC reset is not being performed on stop */
    /* it has to be performed on start, otherwise  */
    /* FW will be not restartable.                 */
    _mtlk_mmb_cleanup_reset_mac(hw);
#endif

    MTLK_START_STEP_VOID(hw_mmb_card, HW_POWER_ON, MTLK_OBJ_PTR(hw),
                         _mtlk_mmb_power_on, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_LDR_CONNECT, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_fw_loader_connect, (hw));

    hw->isr_type = MTLK_ISR_INIT_EVT;

    MTLK_START_STEP(hw_mmb_card, HW_LOAD_FIRMWARE, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_load_firmware, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_LDR_DISCONNECT, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_fw_loader_disconnect, (hw));
    MTLK_START_STEP_VOID(hw_mmb_card, HW_RUN_FIRMWARE, MTLK_OBJ_PTR(hw),
                         _mtlk_mmb_run_firmware, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_WAIT_CHI_MAGIC, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_wait_chi_magic, (hw));

    LOG_CHI_AREA(sFifoQ);
    LOG_CHI_AREA(sDAT);
    LOG_CHI_AREA(sMAN);
    LOG_CHI_AREA(sDBG);

    hw->bds.ind.offset = MAC_TO_HOST32(hw->chi_data.sFifoQ.u32IndStartOffset);
    hw->bds.ind.size   = (uint16)MAC_TO_HOST32(hw->chi_data.sFifoQ.u32IndNumOfElements);
    hw->bds.ind.idx    = 0;

    hw->bds.req.offset = MAC_TO_HOST32(hw->chi_data.sFifoQ.u32ReqStartOffset);
    hw->bds.req.size   = (uint16)MAC_TO_HOST32(hw->chi_data.sFifoQ.u32ReqNumOfElements);
    hw->bds.req.idx    = 0;

    MTLK_START_STEP(hw_mmb_card, HW_MAN_REQ_BDR, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_prepare_man_req_bdr, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_MAN_IND_BDR, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_prepare_man_ind_bdr, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_DBG_REQ_BDR, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_prepare_dbg_req_bdr, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_DBG_IND_BDR, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_prepare_dbg_ind_bdr, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_DAT_REQ_BDR, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_prepare_data_req_bdr, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_DAT_IND_BDR, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_prepare_data_ind_bdr, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_RX_QUEUES, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_rxque_set_default_cfg, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_RX_BUFFERS, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_prealloc_rx_buffers, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_EXTENSIONS, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_init_extensions, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_TXMM, MTLK_OBJ_PTR(hw),
                    mtlk_txmm_start, (&hw->txmm));

    hw->mac_reset_logic_initialized = TRUE;

    MTLK_START_STEP(hw_mmb_card, HW_TXDM, MTLK_OBJ_PTR(hw),
                    mtlk_txmm_start, (&hw->txdm));

    hw->state    = MTLK_HW_STATE_WAITING_READY;
    hw->isr_type = MTLK_ISR_MSGS_PUMP;

    MTLK_START_STEP(hw_mmb_card, HW_SEND_READY, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_send_ready_blocked, (hw));

    /* Must be done after READY message since the recovery may
       send pseudo-responses for non-allocated messages.
       Such pseudo-responses sending is allowed after the MAC has finished 
       its initialization (i.e. after READY CFM from driver's point of view).
     */
    MTLK_START_STEP(hw_mmb_card, HW_RX_PEND_TIMER, MTLK_OBJ_PTR(hw),
                    mtlk_osal_timer_set, (&hw->rx_data_pending.timer,
                                          MTLK_RX_BUFFS_RECOVERY_PERIOD));

    hw->state  = MTLK_HW_STATE_READY;
    ILOG2(GID_HW_MMB, "HW layer activated");

#ifdef MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS
    mtlk_osal_lock_acquire(&hw_tsf_info.lock);
    if (hw_tsf_info.card == hw) {
      hw_tsf_info.started = TRUE;
    }
    mtlk_osal_lock_release(&hw_tsf_info.lock);
#endif

  MTLK_START_FINALLY(hw_mmb_card, MTLK_OBJ_PTR(hw));
  MTLK_START_RETURN(hw_mmb_card, MTLK_OBJ_PTR(hw), mtlk_hw_mmb_stop_card, (hw));
}

/**************************************************************
 * TX MAN MSG module wrapper
 **************************************************************/
#define CM_REQ_MIRROR_BY_MSG_OBJ(pmsg)                                  \
  (mtlk_hw_cm_req_mirror_t *)(((uint8 *)pmsg) -                         \
                              offsetof(mtlk_hw_cm_req_mirror_t, msg))

static struct MSG_OBJ* __MTLK_IFUNC 
_txmm_msg_get_from_pool (mtlk_handle_t usr_data)
{
  mtlk_hw_t    *hw    = (mtlk_hw_t *)usr_data;
  PMSG_OBJ      pmsg  = NULL;

  mtlk_osal_lock_acquire(&hw->tx_man.lock);
  if (mtlk_dlist_size(&hw->tx_man.free_list))
  {
    mtlk_dlist_entry_t      *node = mtlk_dlist_pop_front(&hw->tx_man.free_list);
    mtlk_hw_cm_req_mirror_t *man_req;

    man_req = MTLK_LIST_GET_CONTAINING_RECORD(node,
                                              mtlk_hw_cm_req_mirror_t,
                                              list_entry);
    pmsg = &man_req->msg;
  }
  mtlk_osal_lock_release(&hw->tx_man.lock);

  return pmsg;
}

static void __MTLK_IFUNC 
_txmm_msg_free_to_pool (mtlk_handle_t usr_data, struct MSG_OBJ* pmsg)
{
  mtlk_hw_t               *hw      = (mtlk_hw_t *)usr_data;
  mtlk_hw_cm_req_mirror_t *man_req = CM_REQ_MIRROR_BY_MSG_OBJ(pmsg);

  mtlk_osal_lock_acquire(&hw->tx_man.lock);
  mtlk_dlist_push_back(&hw->tx_man.free_list,
                       &man_req->list_entry);
  mtlk_osal_lock_release(&hw->tx_man.lock);
}

static void __MTLK_IFUNC 
_txmm_send (mtlk_handle_t usr_data, struct MSG_OBJ* pmsg)
{
  mtlk_hw_t               *hw      = (mtlk_hw_t *)usr_data;
  mtlk_hw_cm_req_mirror_t *man_req = CM_REQ_MIRROR_BY_MSG_OBJ(pmsg);
  uint16                   msg_id  = man_req->msg.pas_data.man.sHdr.u16MsgId;

  _mtlk_mmb_dbg_verify_msg_send(man_req);

  /* Must do this in order to deal with MsgID endianess */
  man_req->msg.pas_data.man.sHdr.u16MsgId = 
    HOST_TO_MAC16(man_req->msg.pas_data.man.sHdr.u16MsgId);

  /* Tx MAN BD */
  _mtlk_mmb_memcpy_toio(hw,
                        &hw->tx_man.pas_array[man_req->index],
                        &man_req->msg.pas_data.man,
                        sizeof(man_req->msg.pas_data.man));

  man_req->msg.pas_data.man.sHdr.u16MsgId = msg_id;

  _mtlk_mmb_send_msg(hw, ARRAY_MAN_REQ, man_req->index, 0);
}

static struct MSG_OBJ* __MTLK_IFUNC 
_txdm_msg_get_from_pool (mtlk_handle_t usr_data)
{
  mtlk_hw_t    *hw    = (mtlk_hw_t *)usr_data;
  PMSG_OBJ      pmsg  = NULL;
  
  mtlk_osal_lock_acquire(&hw->tx_dbg.lock);
  if (mtlk_dlist_size(&hw->tx_dbg.free_list))
  {
    mtlk_dlist_entry_t      *node = mtlk_dlist_pop_front(&hw->tx_dbg.free_list);
    mtlk_hw_cm_req_mirror_t *dbg_req;
    
    dbg_req = MTLK_LIST_GET_CONTAINING_RECORD(node,
                                              mtlk_hw_cm_req_mirror_t,
                                              list_entry);
    pmsg = &dbg_req->msg;
  }
  mtlk_osal_lock_release(&hw->tx_dbg.lock);

  return pmsg;
}

static void __MTLK_IFUNC 
_txdm_msg_free_to_pool (mtlk_handle_t usr_data, struct MSG_OBJ* pmsg)
{
  mtlk_hw_t               *hw      = (mtlk_hw_t *)usr_data;
  mtlk_hw_cm_req_mirror_t *dbg_req = CM_REQ_MIRROR_BY_MSG_OBJ(pmsg);
  
  mtlk_osal_lock_acquire(&hw->tx_dbg.lock);
  mtlk_dlist_push_back(&hw->tx_dbg.free_list,
                       &dbg_req->list_entry);
  mtlk_osal_lock_release(&hw->tx_dbg.lock);
}

static void __MTLK_IFUNC 
_txdm_send (mtlk_handle_t usr_data, struct MSG_OBJ* pmsg)
{
  mtlk_hw_t               *hw      = (mtlk_hw_t *)usr_data;
  mtlk_hw_cm_req_mirror_t *dbg_req = CM_REQ_MIRROR_BY_MSG_OBJ(pmsg);
  uint16                   msg_id  = dbg_req->msg.pas_data.dbg.sHdr.u16MsgId;

  _mtlk_mmb_dbg_verify_msg_send(dbg_req);

  /* Must do this in order to deal with MsgID endianess */
  dbg_req->msg.pas_data.dbg.sHdr.u16MsgId = 
    HOST_TO_MAC16(dbg_req->msg.pas_data.dbg.sHdr.u16MsgId);

  /* TX Dbg BD */
  _mtlk_mmb_memcpy_toio(hw,
                        &hw->tx_dbg.pas_array[dbg_req->index],
                        &dbg_req->msg.pas_data.dbg,
                        sizeof(dbg_req->msg.pas_data.dbg));

  dbg_req->msg.pas_data.dbg.sHdr.u16MsgId = msg_id;

  _mtlk_mmb_send_msg(hw, ARRAY_DBG_REQ, dbg_req->index, 0);
}

static int 
_mtlk_mmb_txmm_init(mtlk_hw_t *hw)
{
  mtlk_txmm_cfg_t      cfg;
  mtlk_txmm_wrap_api_t api;

  memset(&cfg, 0, sizeof(cfg));
  memset(&api, 0, sizeof(api));

  cfg.max_msgs          = HW_PCI_TXMM_MAX_MSGS;
  cfg.max_payload_size  = sizeof(UMI_MAN);
  cfg.tmr_granularity   = HW_PCI_TXMM_GRANULARITY;

  api.usr_data          = HANDLE_T(hw);
  api.msg_get_from_pool = _txmm_msg_get_from_pool;
  api.msg_free_to_pool  = _txmm_msg_free_to_pool;
  api.msg_send          = _txmm_send;

  return mtlk_txmm_init(&hw->txmm, &cfg, &api);
}

static int 
_mtlk_mmb_txdm_init(mtlk_hw_t *hw)
{
  mtlk_txmm_cfg_t      cfg;
  mtlk_txmm_wrap_api_t api;

  memset(&cfg, 0, sizeof(cfg));
  memset(&api, 0, sizeof(api));

  cfg.max_msgs          = HW_PCI_TXDM_MAX_MSGS;
  cfg.max_payload_size  = sizeof(UMI_DBG);
  cfg.tmr_granularity   = HW_PCI_TXDM_GRANULARITY;

  api.usr_data          = HANDLE_T(hw);
  api.msg_get_from_pool = _txdm_msg_get_from_pool;
  api.msg_free_to_pool  = _txdm_msg_free_to_pool;
  api.msg_send          = _txdm_send;

  return mtlk_txmm_init(&hw->txdm, &cfg, &api);
}

static void 
txmm_on_cfm (mtlk_hw_t *hw, PMSG_OBJ pmsg)
{
  mtlk_txmm_on_cfm(&hw->txmm, pmsg);
  mtlk_txmm_pump(&hw->txmm, pmsg);
}

static void 
txdm_on_cfm(mtlk_hw_t *hw, PMSG_OBJ pmsg)
{
  mtlk_txmm_on_cfm(&hw->txdm, pmsg);
  mtlk_txmm_pump(&hw->txdm, pmsg);
}

/**************************************************************/

/**************************************************************
 * HW interface implementation
 **************************************************************/
mtlk_hw_msg_t* __MTLK_IFUNC 
mtlk_hw_get_msg_to_send(mtlk_hw_t *hw, uint32* nof_free_tx_msgs)
{
  mtlk_hw_data_req_mirror_t *data_req = _mtlk_mmb_get_msg_from_data_pool(hw);

  if (nof_free_tx_msgs)
    *nof_free_tx_msgs = (uint32)hw->tx_data_nof_free_bds;

  return HW_MSG_PTR(data_req);
}

#ifndef MTCFG_RF_MANAGEMENT_MTLK
#define HIDE_PAYLOAD_TYPE_BUG
#endif

#ifdef HIDE_PAYLOAD_TYPE_BUG
/* WARNING: We suspect the PayloadType feature harms the throughput, so
 *          writing the last TXDAT_REQ_MSG_DESC's DWORD including 
 *          u8RFMgmtData and u8PayloadType to Shared RAM is prohibited
 *          until the bug is fixed whether in driver or MAC.
 *          Once the bug is fixed, TXDATA_INFO_SIZE define could be
 *          removed from the code and _mtlk_mmb_memcpy_toio below can
 *          use sizeof(tx_bd) instead.
 */
#define TXDATA_INFO_SIZE offsetof(TXDAT_REQ_MSG_DESC, u8RFMgmtData)
#else
#define TXDATA_INFO_SIZE sizeof(TXDAT_REQ_MSG_DESC)
#endif

int __MTLK_IFUNC 
mtlk_hw_send_data(mtlk_hw_t                 *hw, 
                  const mtlk_hw_send_data_t *data)
{
  int                        res      = MTLK_ERR_UNKNOWN;
  mtlk_hw_data_req_mirror_t *data_req = DATA_REQ_MIRROR_PTR(data->msg);
  TXDAT_REQ_MSG_DESC         tx_bd;
  uint16                     info;

  data_req->ac      = data->access_category;
  data_req->nbuf    = data->nbuf;
  data_req->size    = data->size;
  data_req->ts      = mtlk_osal_timestamp();

  if (data->size != 0) { /* not a NULL-packet */
    data_req->dma_addr = 
      mtlk_df_nbuf_map_to_phys_addr(hw->cfg.df,
                                    data->nbuf,
                                    data->size,
                                    MTLK_NBUF_TX);
  }
  else {
    data_req->dma_addr = 0;
  }

  tx_bd.u8ExtraData        = MTLK_BFIELD_VALUE(TX_EXTRA_ENCAP_TYPE,
                                               data->encap_type,
                                               uint8);
  tx_bd.u32HostPayloadAddr = HOST_TO_MAC32(data_req->dma_addr);
  tx_bd.sRA                = *data->rcv_addr;
  info                     = MTLK_BFIELD_VALUE(TX_DATA_INFO_LENGTH,
                                               data_req->size,
                                               uint16) +
                             MTLK_BFIELD_VALUE(TX_DATA_INFO_TID,
                                               data_req->ac,
                                               uint16) +
                             MTLK_BFIELD_VALUE(TX_DATA_INFO_WDS,
                                               data->wds,
                                               uint16);
  tx_bd.u16FrameInfo       = HOST_TO_MAC16(info);

#ifdef MTCFG_RF_MANAGEMENT_MTLK
  tx_bd.u8RFMgmtData       = data->rf_mgmt_data;
#endif

  ILOG4(GID_HW_MMB, "Mapping %08x, data %p", 
       (int)data_req->dma_addr, 
       data_req->nbuf);


  CPU_STAT_BEGIN_TRACK(CPU_STAT_ID_TX_HW);
  /* TX Data BD */
  _mtlk_mmb_memcpy_toio(hw,
                        &hw->tx_data.pas_array[data_req->index].sMsg,
                        &tx_bd,
                        TXDATA_INFO_SIZE);

  res = _mtlk_mmb_send_msg(hw, 
                           ARRAY_DAT_REQ,
                           data_req->index,
                           0);

  CPU_STAT_END_TRACK(CPU_STAT_ID_TX_HW);

  if (res != MTLK_ERR_OK)
  {
    if ((data->size != 0) && data_req->dma_addr)
      mtlk_df_nbuf_unmap_phys_addr(hw->cfg.df,
                                   data->nbuf,
                                   data_req->dma_addr,
                                   data->size,
                                   MTLK_NBUF_TX);
  }

  return res;
}

int __MTLK_IFUNC 
mtlk_hw_release_msg_to_send (mtlk_hw_t     *hw, 
                             mtlk_hw_msg_t *msg)
{
  mtlk_hw_data_req_mirror_t *data_req = DATA_REQ_MIRROR_PTR(msg);

  _mtlk_mmb_free_sent_msg_to_data_pool(hw, data_req);

  return MTLK_ERR_OK;
}

int __MTLK_IFUNC 
mtlk_hw_resp_rx_ctrl(mtlk_hw_t *hw, const mtlk_hw_msg_t *msg)
{
  int res = MTLK_ERR_UNKNOWN;

  if (HW_MSG_IN_BDR(hw, rx_man, msg)) {
    res = _mtlk_mmb_resp_man_ind(hw, MAN_IND_MIRROR_PTR(msg));
  }
  else if (HW_MSG_IN_BDR(hw, rx_dbg, msg)) {
    res = _mtlk_mmb_resp_dbg_ind(hw, MAN_DBG_MIRROR_PTR(msg));
  }
  else {
    ELOG("Wrong control message to respond (0x%p)", msg);
    res = MTLK_ERR_PARAMS;
  }

  return res;
}

int __MTLK_IFUNC 
mtlk_hw_set_prop(mtlk_hw_t *hw, mtlk_hw_prop_e prop_id, void *buffer, uint32 size)
{
  int res = MTLK_ERR_NOT_SUPPORTED;

  switch (prop_id)
  {
  case MTLK_HW_PROGMODEL:
    res = _mtlk_mmb_load_progmodel_to_hw(hw, buffer);
    break;
  case MTLK_HW_PROP_STATE:
    if (size == sizeof(mtlk_hw_state_e))
    {
      mtlk_hw_state_e *val = (mtlk_hw_state_e *)buffer;
      hw->state = *val;
      res       = MTLK_ERR_OK;
      if ((hw->state == MTLK_HW_STATE_APPFATAL) ||
          (hw->state == MTLK_HW_STATE_EXCEPTION)) {
        mtlk_txmm_halt(&hw->txmm);
        mtlk_txmm_halt(&hw->txdm);
      }
    }
    break;
  case MTLK_HW_BCL_ON_EXCEPTION:
    if (size == sizeof(UMI_BCL_REQUEST))
    {
      UMI_BCL_REQUEST *preq = (UMI_BCL_REQUEST *)buffer;
      res = _mtlk_mmb_process_bcl(hw, preq, 0);
    }
    break;
  case MTLK_HW_PROGMODEL_FREE:
    {
      mtlk_core_firmware_file_t *ff = buffer;
      mtlk_mw_bus_file_buf_t fb;
      fb.buffer = ff->content.buffer;
      fb.size = ff->content.size;
      fb.context = ff->context;
      mtlk_hw_bus_release_file_buffer(hw->cfg.bus, &fb);
      res = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_RESET:
    {
      _mtlk_mmb_handle_sw_trap(hw);
      res = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_DBG_ASSERT_FW:
    if (buffer && size == sizeof(uint32))
    {
      uint32 *mips_no = (uint32 *)buffer;
      res = _mtlk_mmb_cause_mac_assert(hw, *mips_no);
    }
    break;
  default:
    break;
  }

  return res;
}

int __MTLK_IFUNC 
mtlk_hw_get_prop(mtlk_hw_t *hw, mtlk_hw_prop_e prop_id, void *buffer, uint32 size)
{
  int res = MTLK_ERR_NOT_SUPPORTED;

  switch (prop_id)
  {
  case MTLK_HW_PROP_STATE:
    if (size == sizeof(mtlk_hw_state_e))
    {
      mtlk_hw_state_e *val = (mtlk_hw_state_e *)buffer;
      *val = hw->state;
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_FREE_TX_MSGS:
    if (size == sizeof(uint32))
    {
      uint32 *val = (uint32 *)buffer;
      *val = hw->tx_data_nof_free_bds;
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_TX_MSGS_USED_PEAK:
    if (size == sizeof(uint32))
    {
      uint32 *val = (uint32 *)buffer;
      *val = hw->tx_data_max_used_bds;
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_DUMP:
    if (size == sizeof(mtlk_hw_dump_t))
    {
      mtlk_hw_dump_t *dump = (mtlk_hw_dump_t *)buffer;
      _mtlk_mmb_pas_get(hw, "dbg dump", dump->addr, dump->buffer, dump->size);
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_BCL_ON_EXCEPTION:
    if (size == sizeof(UMI_BCL_REQUEST))
    {
      UMI_BCL_REQUEST *preq = (UMI_BCL_REQUEST *)buffer;
      res = _mtlk_mmb_process_bcl(hw, preq, 1);
    }
    break;
  case MTLK_HW_PRINT_BUS_INFO:
    {
      char *str = (char *)buffer;

      snprintf(str, size - 1, "PCI-%s", mtlk_hw_bus_dev_name(hw->cfg.bus)); 
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_BIST:
    if (size == sizeof(uint32))
    {
      uint32 *val = (uint32 *)buffer;
      *val = hw->mmb->bist_passed;
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_PROGMODEL:
    res = _mtlk_mmb_load_progmodel_from_os(hw, buffer);
    break;
  case MTLK_HW_GENERATION:
    res = MTLK_ERR_UNKNOWN;
    MTLK_ASSERT(sizeof(mtlk_hw_gen_e) == size);
    CARD_SELECTOR_START(hw->cfg.bus->card_type)
      IF_CARD_G2 ( *((mtlk_hw_gen_e*) buffer) = MTLK_HW_GEN2; res = MTLK_ERR_OK; );
      IF_CARD_G3 ( *((mtlk_hw_gen_e*) buffer) = MTLK_HW_GEN3; res = MTLK_ERR_OK; );
    CARD_SELECTOR_END();
    MTLK_ASSERT(MTLK_ERR_OK == res);
    break;
  default:
    break;
  }

  return res;
}
/**************************************************************/

/**************************************************************
 * MMB interface implementation
 **************************************************************/

MTLK_INIT_STEPS_LIST_BEGIN(hw_mmb)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_mmb, HW_MMB_LOCK)
MTLK_INIT_INNER_STEPS_BEGIN(hw_mmb)
MTLK_INIT_STEPS_LIST_END(hw_mmb);

int __MTLK_IFUNC 
mtlk_hw_mmb_init (mtlk_hw_mmb_t *mmb, const mtlk_hw_mmb_cfg_t *cfg)
{
  memset(mmb, 0, sizeof(*mmb));
  mmb->cfg = *cfg;

#if MTLK_RX_BUFF_ALIGNMENT
  ILOG2(GID_HW_MMB, "HW requires Rx buffer alignment to %d (0x%02x)", 
       MTLK_RX_BUFF_ALIGNMENT,
       MTLK_RX_BUFF_ALIGNMENT);
#endif

#ifdef MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS
  memset(&hw_tsf_info, 0, sizeof(hw_tsf_info));
  mtlk_osal_lock_init(&hw_tsf_info.lock);
#endif
  MTLK_INIT_TRY(hw_mmb, MTLK_OBJ_PTR(mmb))
    MTLK_INIT_STEP(hw_mmb, HW_MMB_LOCK, MTLK_OBJ_PTR(mmb), 
                   mtlk_osal_lock_init, (&mmb->lock));
  MTLK_INIT_FINALLY(hw_mmb, MTLK_OBJ_PTR(mmb))
  MTLK_INIT_RETURN(hw_mmb, MTLK_OBJ_PTR(mmb),
                   mtlk_hw_mmb_cleanup, (mmb));
}

void __MTLK_IFUNC
mtlk_hw_mmb_cleanup (mtlk_hw_mmb_t *mmb)
{
#ifdef MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS
  mtlk_osal_lock_cleanup(&hw_tsf_info.lock);
  memset(&hw_tsf_info, 0, sizeof(hw_tsf_info));
#endif

  MTLK_CLEANUP_BEGIN(hw_mmb, MTLK_OBJ_PTR(mmb))
    MTLK_CLEANUP_STEP(hw_mmb, HW_MMB_LOCK, MTLK_OBJ_PTR(mmb),
                      mtlk_osal_lock_cleanup, (&mmb->lock));
  MTLK_CLEANUP_END(hw_mmb, MTLK_OBJ_PTR(mmb))

  memset(mmb, 0, sizeof(*mmb));
}

uint32 __MTLK_IFUNC
mtlk_hw_mmb_get_cards_no (mtlk_hw_mmb_t *mmb)
{
  return mmb->nof_cards;
}

mtlk_txmm_t *__MTLK_IFUNC
mtlk_hw_mmb_get_txmm (mtlk_hw_t *hw)
{
  return &hw->txmm;
}

mtlk_txmm_t *__MTLK_IFUNC
mtlk_hw_mmb_get_txdm (mtlk_hw_t *hw)
{
  return &hw->txdm;
}

void __MTLK_IFUNC
mtlk_hw_mmb_stop_mac_events (mtlk_hw_t *hw)
{
  hw->mac_events_stopped = 1;
}

mtlk_hw_t * __MTLK_IFUNC 
mtlk_hw_mmb_add_card (mtlk_hw_mmb_t                *mmb,
                      const mtlk_hw_mmb_card_cfg_t *card_cfg)
{
  mtlk_hw_t    *hw   = NULL;
  int           i    = 0;

  /* FIXME: allocate memory now, avoid OSAL-related problems on Linux:
           mtlk_osal_lock_acquire disables interrupts and mtlk_osal_mem_alloc
           cannot recognize that it is GFP_ATOMIC context now */
  hw = (mtlk_hw_t *)mtlk_osal_mem_alloc(sizeof(*hw), MTLK_MEM_TAG_HW);

  mtlk_osal_lock_acquire(&mmb->lock);

  if (!hw) {
    ELOG("Can't allocate HW object");
    goto FINISH;
  }

  if (mmb->nof_cards >= ARRAY_SIZE(mmb->cards)) {
    ELOG("Maximum %d boards supported", (int)ARRAY_SIZE(mmb->cards));
    mtlk_osal_mem_free(hw);
    hw = NULL;
    goto FINISH;
  }

  memset(hw, 0, sizeof(*hw));

  hw->cfg = *card_cfg;
  hw->mmb = mmb;

  for (i = 0; i < ARRAY_SIZE(mmb->cards); i++) {
    if (!mmb->cards[i]) {
      mmb->cards[i] = hw;
      mmb->nof_cards++;
#ifdef MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS
      if (TSF_CARD_IDX == i) {
        mtlk_osal_lock_acquire(&hw_tsf_info.lock);
        hw_tsf_info.card = hw;
        mtlk_osal_lock_release(&hw_tsf_info.lock);
      }
#endif
      break;
    }
  }

FINISH:
  mtlk_osal_lock_release(&mmb->lock);

  return hw;
}

void __MTLK_IFUNC 
mtlk_hw_mmb_remove_card (mtlk_hw_mmb_t *mmb,
                         mtlk_hw_t     *hw)
{
  int           i    = 0;

  mtlk_osal_lock_acquire(&mmb->lock);
  for (i = 0; i < ARRAY_SIZE(mmb->cards); i++) {
    if (mmb->cards[i] == hw) {
      mmb->cards[i] = NULL;
      mmb->nof_cards--;
#ifdef MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS
      if (TSF_CARD_IDX == i) {
        mtlk_osal_lock_acquire(&hw_tsf_info.lock);
        hw_tsf_info.card = NULL;
        mtlk_osal_lock_release(&hw_tsf_info.lock);
      }
#endif
      break;
    }
  }
  mtlk_osal_mem_free(hw);
  mtlk_osal_lock_release(&mmb->lock);
}

int __MTLK_IFUNC 
mtlk_hw_mmb_interrupt_handler (mtlk_hw_t *hw)
{
  ILOG4(GID_HW_MMB, "o=%p s=%d", hw, hw->state);
  MTLK_ASSERT(NULL != hw->cfg.ccr);
  
  if (!mtlk_ccr_disable_interrupts_if_pending(hw->cfg.ccr)) {
    return MTLK_ERR_UNKNOWN; /* not an our interrupt */
  }

  if (!mtlk_ccr_clear_interrupts_if_pending(hw->cfg.ccr)) {
    mtlk_ccr_enable_interrupts(hw->cfg.ccr);
    return MTLK_ERR_OK;
  }

  if (hw->state == MTLK_HW_STATE_HALTED) {
    WARNING("Interrupt received while HW is in halted state.");
    mtlk_ccr_enable_interrupts(hw->cfg.ccr);
    return MTLK_ERR_OK;
  }

  switch (hw->isr_type) {
  case MTLK_ISR_INIT_EVT:
    MTLK_HW_INIT_EVT_SET(hw); /* Interrupts will be enabled by bootstrap code */
    return MTLK_ERR_OK;
  case MTLK_ISR_MSGS_PUMP:
    return MTLK_ERR_PENDING; /* Interrupts will be enabled by tasklet */
  case MTLK_ISR_NONE:
  case MTLK_ISR_LAST:
  default:
    ELOG("Interrupt of unknown type (%d) received", hw->isr_type);
    mtlk_ccr_enable_interrupts(hw->cfg.ccr);
    return MTLK_ERR_OK;
  }
}

void __MTLK_IFUNC 
mtlk_hw_mmb_deferred_handler (mtlk_hw_t *hw)
{
  _mtlk_mmb_read_ind_or_cfm(hw);
}

void
mtlk_mmb_sync_isr (mtlk_hw_t         *hw, 
                   mtlk_hw_bus_sync_f func,
                   void              *context)
{
  mtlk_handle_t lock_val;
  lock_val = mtlk_osal_lock_acquire_irq(&hw->reg_lock);
  func(context);
  mtlk_osal_lock_release_irq(&hw->reg_lock, lock_val);
}

#ifdef MTCFG_LINDRV_HW_PCIG2

/* Definitions for the bootloader's protocol */

#define MTLK_FRMW_BOOTLOADER_NAME "bootloader.bin"
#define CHI_ADDR_XFER_CTRL_BASE   (0x0F00)
#define CHI_ADDR_BOOT_MAGIC       (CHI_ADDR_XFER_CTRL_BASE + 0)
#define CHI_ADDR_BOOT_NUM         (CHI_ADDR_XFER_CTRL_BASE + 4)
#define CHI_ADDR_XFER_VECTOR      (CHI_ADDR_XFER_CTRL_BASE + 8)
#define CHI_ADDR_XFER_MAX         (CHI_ADDR_XFER_CTRL_BASE + 12)
#define CHI_ADDR_XFER_LEN         (CHI_ADDR_XFER_CTRL_BASE + 16)
#define CHI_ADDR_XFER_CTRL(x)     (CHI_ADDR_XFER_CTRL_BASE + 20 + ((x) * 4))

#define CHI_BOOT_READY                  (1)
#define CHI_BOOT_DATA                   (2)
#define CHI_BOOT_READING                (3)
#define CHI_BOOT_END                    (4)
#define CHI_BOOT_WAITING                (5)
#define CHI_BOOT_GO                     (6)
#define CHI_BOOT_STORE                  (7)

#define BOOT_RETRIES 10
#define MAGIC_BOOT   0x43215678

static int
__mtlk_mmb_fw_loader_g2_connect (mtlk_hw_t *hw)
{
  /* On Gen2 we are using bootloader, so we have to */
  /* load and run it.                               */
  int                    res   = MTLK_ERR_UNKNOWN;
  mtlk_mw_bus_file_buf_t fb;
  int                    fb_ok = 0;
  ILOG2(GID_HW_MMB, "%s loading...", MTLK_FRMW_BOOTLOADER_NAME);

  res = mtlk_hw_bus_get_file_buffer(hw->cfg.bus,
                                    MTLK_FRMW_BOOTLOADER_NAME,
                                    &fb);
  if (res != MTLK_ERR_OK) {
    ELOG("can not load bootloader (%s). Err=%d",
          MTLK_FRMW_BOOTLOADER_NAME,
          res);
    goto FINISH;
  }

  fb_ok = 1;

  ILOG2(GID_HW_MMB, "%s: size=0x%04x, data=0x%p", MTLK_FRMW_BOOTLOADER_NAME,
       (unsigned)fb.size, fb.buffer);

  _mtlk_mmb_memcpy_toio_no_pll(hw, hw->cfg.pas, fb.buffer, fb.size);
  
  _mtlk_mmb_pas_writel(hw, "write magic number", 
                       CHI_ADDR_BOOT_MAGIC, 0);

  _mtlk_mmb_notify_firmware(hw, 
                            MTLK_FRMW_BOOTLOADER_NAME, 
                            (const char*)fb.buffer,
                            fb.size);
  
  if (hw->state == MTLK_HW_STATE_ERROR) {
    /* We got a spurious interrupt? */
    ELOG("Got spurious interrup");
    goto FINISH;
  }
  
  res = MTLK_ERR_OK;

FINISH:
  if (fb_ok) 
    mtlk_hw_bus_release_file_buffer(hw->cfg.bus, &fb);

  return res;
}

static int
_mtlk_mmb_fw_loader_g2_load_file (mtlk_hw_t*     hw,
                                  const uint8*   buffer,
                                  uint32         size,
                                  uint8          cpu_num)
{
  int    res            = MTLK_ERR_UNKNOWN;
  uint32 written        = 0;  /* nof bytes written (done) */
  uint32 max_chunk_size = 0;  /* Firware chunk max size */
  uint32 load_addr      = 0;  /* PAS offset to write firmware */
  int i;

  MTLK_ASSERT(NULL != hw->cfg.ccr);

  MTLK_HW_INIT_EVT_RESET(hw);
  
  /* Load Upper CPU's IRAM using the boot loader */
  _mtlk_mmb_pas_writel(hw, "Write cpu number", 
                       CHI_ADDR_BOOT_NUM, 
                       cpu_num);

  if(CHI_CPU_NUM_UM == cpu_num) 
    mtlk_ccr_release_ucpu_reset(hw->cfg.ccr);
  else 
    mtlk_ccr_release_lcpu_reset(hw->cfg.ccr);

  /* re-enable interrupts in case we turn them off for spurious interrupts */
  mtlk_ccr_enable_interrupts(hw->cfg.ccr);

  while (TRUE) {
    uint32 xfer_cntrl = 0;
    uint32 chunk_size = 0;

    res = MTLK_HW_INIT_EVT_WAIT(hw, MTLK_FRMW_LOAD_CHUNK_TIMEOUT);
    if (res != MTLK_ERR_OK) {
      ELOG("No interrupt received within %d ms", 
            MTLK_FRMW_LOAD_CHUNK_TIMEOUT);
      break; /* return error */
    }

    ILOG3(GID_HW_MMB, "loading: %d bytes", (int)size);

    if (!written) { /* 1st chunk */
      for (i = 0; i < BOOT_RETRIES; i++) {
        unsigned int magic;
        _mtlk_mmb_pas_readl(hw, 
                            "Magic boot loader value", 
                            CHI_ADDR_BOOT_MAGIC, 
                            magic);

        if (magic == MAGIC_BOOT) {
          /* Bootloader is ready to receive firmware */
          break;
        }
        /* wait before the next try */
        mtlk_osal_msleep(10);
      }

      if (i >= BOOT_RETRIES) {
        ELOG("failed to read MAGIC_BOOT after %i tries from card",
              BOOT_RETRIES);
        res = MTLK_ERR_HW;
        break; /* return error */
      } 
      else {
        ILOG3(GID_HW_MMB, "boot tries %d", i);
        _mtlk_mmb_pas_readl(hw, 
                            "Maximum transfer size",
                            CHI_ADDR_XFER_MAX,
                            max_chunk_size);
        max_chunk_size *= sizeof(uint32);
        _mtlk_mmb_pas_readl(hw, 
                            "PAS offset of transfer area",
                            CHI_ADDR_XFER_VECTOR,
                            load_addr);
      }
    }

    for (i = 0; i < BOOT_RETRIES; i++) {
      _mtlk_mmb_pas_readl(hw,
                          "Is MAC waiting for the next chunk?",
                          CHI_ADDR_XFER_CTRL(cpu_num),
                          xfer_cntrl);
      if (xfer_cntrl == CHI_BOOT_READY || xfer_cntrl == CHI_BOOT_READING) {
        ILOG3(GID_HW_MMB, "All systems go");
        /* continue with the next chunk */
        break;
      }
      /* wait before the next try */
      mtlk_osal_msleep(10);
    }
    /* check if bootloader is ready for the next chunk */
    if (i >= BOOT_RETRIES) {
      ELOG("MAC is not ready to receive next firmware chunk: %x", xfer_cntrl);
      res = MTLK_ERR_HW;
      break; /* return error */
    }
    
    if (!size) { /* all done! */
      res = MTLK_ERR_OK;
      break; /* return */
    }

    chunk_size = (size > max_chunk_size) ? max_chunk_size : size;

    _mtlk_mmb_pas_put(hw, "next chunk", load_addr, buffer, chunk_size);
    ILOG3(GID_HW_MMB, "Wrote to PAS offset 0x%x, from %p, size %d bytes", 
         load_addr,
         buffer, 
         chunk_size);

    size    -= chunk_size;
    written += chunk_size;
    buffer  += chunk_size;

    _mtlk_mmb_pas_writel(hw, 
                         "Transfer length", 
                         CHI_ADDR_XFER_LEN, 
                         chunk_size/sizeof(uint32));
    _mtlk_mmb_pas_writel(hw, 
                         "Transfer command", 
                         CHI_ADDR_XFER_CTRL(cpu_num), 
                         CHI_BOOT_DATA);

    MTLK_HW_INIT_EVT_RESET(hw); /* Prepare event for the next interrupt */
    mtlk_ccr_enable_interrupts(hw->cfg.ccr);
  }

  return res;
}

static int
__mtlk_mmb_fw_loader_g2_disconnect (mtlk_hw_t *hw)
{
  /* On Gen2 we are using bootloader, so we have to  */
  /* tell it that we finished with firmware loading. */
  int    res = MTLK_ERR_HW;
  uint32 xfer_cntrl;

  MTLK_ASSERT(NULL != hw->cfg.ccr);

  /*
   * Check if both cpu's are in the waiting state
   */

  MTLK_HW_INIT_EVT_RESET(hw);

  _mtlk_mmb_pas_writel(hw, 
                       "finish downloading upper cpu", 
                       CHI_ADDR_XFER_CTRL(CHI_CPU_NUM_UM), 
                       CHI_BOOT_END);
  _mtlk_mmb_pas_writel(hw, 
                       "finish downloading lower cpu", 
                       CHI_ADDR_XFER_CTRL(CHI_CPU_NUM_LM), 
                       CHI_BOOT_END);
  mtlk_ccr_enable_interrupts(hw->cfg.ccr);

  res = MTLK_HW_INIT_EVT_WAIT(hw, MTLK_MAC_BOOT_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG("No interrupts while waiting for MAC boot");
    goto FINISH;
  }
  
  ILOG2(GID_HW_MMB, "check_for_boot");
  _mtlk_mmb_pas_readl(hw, 
                      "Check if upper cpu is in a waiting state", 
                      CHI_ADDR_XFER_CTRL(CHI_CPU_NUM_UM), 
                      xfer_cntrl);
  if (xfer_cntrl != CHI_BOOT_WAITING) {
    ELOG("Upper CPU not in waiting state");
    res = MTLK_ERR_HW;
    goto FINISH;
  }

  _mtlk_mmb_pas_readl(hw, 
                      "Check if lower cpu is in a waiting state", 
                      CHI_ADDR_XFER_CTRL(CHI_CPU_NUM_LM), 
                      xfer_cntrl);
  if (xfer_cntrl != CHI_BOOT_WAITING) {
    ELOG("Lower CPU not in waiting state");
    res = MTLK_ERR_HW;
    goto FINISH;
  }

  _mtlk_mmb_pas_writel(hw, 
                       "start card", 
                       CHI_ADDR_XFER_CTRL(CHI_CPU_NUM_UM),
                       CHI_BOOT_GO); /* TODO: why? we're going to reset both CPUs */

  ILOG2(GID_HW_MMB, "cleaning up a possible G2 Bootloader ghost interrupt...");
  mtlk_ccr_clear_interrupts_if_pending(hw->cfg.ccr);

  res = MTLK_ERR_OK;

FINISH:
  return res;
}

#endif /* #ifdef MTCFG_LINDRV_HW_PCIG2 */

#if defined(MTCFG_LINDRV_HW_PCIE) || defined (MTCFG_LINDRV_HW_PCIG3)

static int
__mtlk_mmb_fw_loader_g3_connect (mtlk_hw_t *hw)
{
  /* On Gen3 we are writing firmware right to  */
  /* the IRAM, so no preparations are required */

  return MTLK_ERR_OK;
}

typedef struct __mmb_cpu_memory_chunk
{
  int start;
  int length;
} _mmb_cpu_memory_chunk;

#define MEMORY_CHUNK_SIZE                    (0x8000)
#define UCPU_INTERNAL_MEMORY_CHUNKS          (3)
#define LCPU_INTERNAL_MEMORY_CHUNKS          (3)
#define TOTAL_EXTERNAL_MEMORY_CHUNKS         (5)
#define LCPU_EXTERNAL_MEMORY_CHUNKS          (1)
#define UCPU_INTERNAL_MEMORY_START           (0x240000)
#define LCPU_INTERNAL_MEMORY_START           (0x2C0000)

#define UCPU_EXTERNAL_MEMORY_CHUNKS          (TOTAL_EXTERNAL_MEMORY_CHUNKS \
                                              - LCPU_EXTERNAL_MEMORY_CHUNKS)
#define UCPU_TOTAL_MEMORY_CHUNKS             (UCPU_INTERNAL_MEMORY_CHUNKS \
                                              + UCPU_EXTERNAL_MEMORY_CHUNKS)
#define UCPU_TOTAL_MEMORY_SIZE               (UCPU_TOTAL_MEMORY_CHUNKS \
                                               * MEMORY_CHUNK_SIZE)
#define UCPU_INTERNAL_MEMORY_SIZE            (UCPU_INTERNAL_MEMORY_CHUNKS \
                                               * MEMORY_CHUNK_SIZE)
#define UCPU_EXTERNAL_MEMORY_SIZE            (UCPU_EXTERNAL_MEMORY_CHUNKS \
                                               * MEMORY_CHUNK_SIZE)
#define UCPU_EXTERNAL_MEMORY_START           (UCPU_INTERNAL_MEMORY_START \
                                               + UCPU_INTERNAL_MEMORY_SIZE)

#define LCPU_TOTAL_MEMORY_CHUNKS             (LCPU_INTERNAL_MEMORY_CHUNKS \
                                               + LCPU_EXTERNAL_MEMORY_CHUNKS)
#define LCPU_TOTAL_MEMORY_SIZE               (LCPU_TOTAL_MEMORY_CHUNKS \
                                               * MEMORY_CHUNK_SIZE)
#define LCPU_INTERNAL_MEMORY_SIZE            (LCPU_INTERNAL_MEMORY_CHUNKS \
                                               * MEMORY_CHUNK_SIZE)
#define LCPU_EXTERNAL_MEMORY_SIZE            (LCPU_EXTERNAL_MEMORY_CHUNKS \
                                               * MEMORY_CHUNK_SIZE)
#define LCPU_EXTERNAL_MEMORY_START           (UCPU_INTERNAL_MEMORY_START \
                                               + UCPU_TOTAL_MEMORY_SIZE)

static int
_mtlk_mmb_fw_loader_g3_load_file (mtlk_hw_t*     hw,
                                  const uint8*   buffer,
                                  uint32         size,
                                  uint8          cpu_num)
{
  static _mmb_cpu_memory_chunk _ucpu_memory_chunks[] =
        { {UCPU_INTERNAL_MEMORY_START, UCPU_INTERNAL_MEMORY_SIZE}, 
          {UCPU_EXTERNAL_MEMORY_START, UCPU_EXTERNAL_MEMORY_SIZE},
          {0,0} };
  static _mmb_cpu_memory_chunk _lcpu_memory_chunks[] =
        { {LCPU_INTERNAL_MEMORY_START, LCPU_INTERNAL_MEMORY_SIZE},
          {LCPU_EXTERNAL_MEMORY_START, LCPU_EXTERNAL_MEMORY_SIZE},
          {0,0} };

  uint32 bytes_written = 0;
  int i;

  _mmb_cpu_memory_chunk* memory_chunks = 
    (CHI_CPU_NUM_UM == cpu_num) ? _ucpu_memory_chunks : _lcpu_memory_chunks;

  for(i = 0; (0 != memory_chunks[i].length) && (bytes_written < size); i++ ) {
    if(!_mtlk_mmb_pas_put(hw, "write firmware to internal memory of the corresponding CPU",
                      memory_chunks[i].start, buffer + bytes_written,
                      MIN(memory_chunks[i].length, size - bytes_written))) {
      ELOG("Failed to put firmware to shared memory");
      return MTLK_ERR_FW;
    }
    bytes_written += MIN(memory_chunks[i].length, size - bytes_written);
    MTLK_ASSERT(bytes_written <= size);
  }
  
  if (bytes_written == size) {
    return MTLK_ERR_OK; 
  } else {
    ELOG("Firmware file is to big to fit into the %s cpu memory (%d > %d)",
           (CHI_CPU_NUM_UM == cpu_num) ? "upper" : "lower",
           size,
           (CHI_CPU_NUM_UM == cpu_num) ? UCPU_TOTAL_MEMORY_SIZE : LCPU_TOTAL_MEMORY_SIZE);
    return MTLK_ERR_FW;
  }
}

static int
__mtlk_mmb_fw_loader_g3_disconnect (mtlk_hw_t *hw)
{
  /* On Gen3 we are writing firmware right to  */
  /* the IRAM, so no "disconnect" is required  */
  return MTLK_ERR_OK;
}

#endif /* defined(MTCFG_LINDRV_HW_PCIE) || defined (MTCFG_LINDRV_HW_PCIG3) */

static int
_mtlk_mmb_fw_loader_connect (mtlk_hw_t *hw)
{
  CARD_SELECTOR_START(hw->cfg.bus->card_type)
    IF_CARD_G2 ( return __mtlk_mmb_fw_loader_g2_connect(hw) );
    IF_CARD_G3 ( return __mtlk_mmb_fw_loader_g3_connect(hw) );
  CARD_SELECTOR_END();

  MTLK_ASSERT(!"Should never be here");
  return MTLK_ERR_PARAMS;
}

static int
_mtlk_mmb_fw_loader_load_file (mtlk_hw_t*     hw,
                               const uint8*   buffer,
                               uint32         size,
                               uint8          cpu_num)
{
  CARD_SELECTOR_START(hw->cfg.bus->card_type)
    IF_CARD_G2 ( return _mtlk_mmb_fw_loader_g2_load_file(hw, buffer, 
                                                         size, cpu_num) );
    IF_CARD_G3 ( return _mtlk_mmb_fw_loader_g3_load_file(hw, buffer, 
                                                         size, cpu_num) );
  CARD_SELECTOR_END();

  MTLK_ASSERT(!"Should never be here");
  return MTLK_ERR_PARAMS;
}

static int
_mtlk_mmb_fw_loader_disconnect (mtlk_hw_t *hw)
{
  CARD_SELECTOR_START(hw->cfg.bus->card_type)
    IF_CARD_G2 ( return __mtlk_mmb_fw_loader_g2_disconnect(hw) );
    IF_CARD_G3 ( return __mtlk_mmb_fw_loader_g3_disconnect(hw) );
  CARD_SELECTOR_END();

  MTLK_ASSERT(!"Should never be here");
  return MTLK_ERR_PARAMS;
}

#ifdef MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS
uint32 __MTLK_IFUNC
mtlk_hw_get_tsf_card_time_stamp (void)
{
  uint32 low = 0;
  uint32 high;

  mtlk_osal_lock_acquire(&hw_tsf_info.lock);
  if (hw_tsf_info.card && hw_tsf_info.card->cfg.ccr) {
    /*  Bar1                                  */
    /*  equ mac_pac_tsf_timer_low 0x200738    */
    /*  equ mac_pac_tsf_timer_high 0x20073C   */

    mtlk_ccr_read_hw_timestamp(hw_tsf_info.card->cfg.ccr, &low, &high);
  }
  mtlk_osal_lock_release(&hw_tsf_info.lock);

  return low;
}

#endif /* MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS */
