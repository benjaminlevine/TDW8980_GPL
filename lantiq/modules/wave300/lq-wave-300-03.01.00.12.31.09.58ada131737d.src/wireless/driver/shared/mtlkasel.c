/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"
#include "mtlkasel.h"
#include "mtlkmib.h"

#include "mtlkirb.h"
#include "mtlkaselirb.h"

#define DPR0    LOG2
#define DPR_PFX "RFMGMT: "

const static mtlk_guid_t IRBE_RF_MGMT_SET_TYPE      = MTLK_IRB_GUID_RF_MGMT_SET_TYPE;
const static mtlk_guid_t IRBE_RF_MGMT_GET_TYPE      = MTLK_IRB_GUID_RF_MGMT_GET_TYPE;
const static mtlk_guid_t IRBE_RF_MGMT_SET_DEF_DATA  = MTLK_IRB_GUID_RF_MGMT_SET_DEF_DATA;
const static mtlk_guid_t IRBE_RF_MGMT_GET_DEF_DATA  = MTLK_IRB_GUID_RF_MGMT_GET_DEF_DATA;
const static mtlk_guid_t IRBE_RF_MGMT_GET_PEER_DATA = MTLK_IRB_GUID_RF_MGMT_GET_PEER_DATA;
const static mtlk_guid_t IRBE_RF_MGMT_SET_PEER_DATA = MTLK_IRB_GUID_RF_MGMT_SET_PEER_DATA;
const static mtlk_guid_t IRBE_RF_MGMT_SEND_SP       = MTLK_IRB_GUID_RF_MGMT_SEND_SP;
const static mtlk_guid_t IRBE_RF_MGMT_GET_SPR       = MTLK_IRB_GUID_RF_MGMT_GET_SPR;
const static mtlk_guid_t IRBE_RF_MGMT_SPR_ARRIVED   = MTLK_IRB_GUID_RF_MGMT_SPR_ARRIVED;

struct mtlk_rf_mgmt_drv_evt_handler
{
  const mtlk_guid_t     *evt;
  mtlk_irb_evt_handler_f func;
};

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_set_type(mtlk_handle_t      context,
                            const mtlk_guid_t *evt,
                            void              *buffer,
                            uint32            *size);

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_get_type(mtlk_handle_t      context,
                            const mtlk_guid_t *evt,
                            void              *buffer,
                            uint32            *size);

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_set_def_data(mtlk_handle_t      context,
                                const mtlk_guid_t *evt,
                                void              *buffer,
                                uint32            *size);
static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_get_def_data(mtlk_handle_t      context,
                                const mtlk_guid_t *evt,
                                void              *buffer,
                                uint32            *size);
static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_set_peer_data(mtlk_handle_t      context,
                                 const mtlk_guid_t *evt,
                                 void              *buffer,
                                 uint32            *size);
static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_get_peer_data(mtlk_handle_t      context,
                                 const mtlk_guid_t *evt,
                                 void              *buffer,
                                 uint32            *size);
static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_send_sp(mtlk_handle_t      context,
                           const mtlk_guid_t *evt,
                           void              *buffer,
                           uint32            *size);
static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_get_spr(mtlk_handle_t      context,
                           const mtlk_guid_t *evt,
                           void              *buffer,
                           uint32            *size);

const static struct mtlk_rf_mgmt_drv_evt_handler rf_mgmt_drv_evts[] = {
  { &IRBE_RF_MGMT_SET_TYPE,      _mtlk_rf_mgmt_irbh_set_type      },
  { &IRBE_RF_MGMT_GET_TYPE,      _mtlk_rf_mgmt_irbh_get_type      },
  { &IRBE_RF_MGMT_SET_DEF_DATA,  _mtlk_rf_mgmt_irbh_set_def_data  },
  { &IRBE_RF_MGMT_GET_DEF_DATA,  _mtlk_rf_mgmt_irbh_get_def_data  },
  { &IRBE_RF_MGMT_SET_PEER_DATA, _mtlk_rf_mgmt_irbh_set_peer_data },
  { &IRBE_RF_MGMT_GET_PEER_DATA, _mtlk_rf_mgmt_irbh_get_peer_data },
  { &IRBE_RF_MGMT_SEND_SP,       _mtlk_rf_mgmt_irbh_send_sp       },
  { &IRBE_RF_MGMT_GET_SPR,       _mtlk_rf_mgmt_irbh_get_spr       }
};

#define RF_MGMT_SET_TYPE_TIMEOUT     3000
#define RF_MGMT_SEND_DATA_TIMEOUT    2000
#define RF_MGMT_SEND_SP_VSAF_TIMEOUT 2000

typedef struct
{
  mtlk_ldlist_entry_t lentry;
  mtlk_rf_mgmt_spr_t  spr;
} mtlk_rf_mgmt_db_entry_spr_t;

#define MTLK_RF_MGMT_DB_ENSURE_DLIM(db, type, lim)                      \
  {                                                                     \
    while (mtlk_dlist_size(&(db)->data) > (lim)) {                      \
      mtlk_ldlist_entry_t *e__ = mtlk_dlist_pop_front(&(db)->data);     \
      type                *d__ = MTLK_CONTAINER_OF(e__, type, lentry);  \
      mtlk_osal_mem_free(d__);                                          \
    }                                                                   \
  }

#define MTLK_RF_MGMT_DB_ADD_ENSURE_DLIM(db, el, type)                   \
  {                                                                     \
    uint32 l__ = (db)->dlim?((db)->dlim - 1):0;                         \
    mtlk_osal_lock_acquire(&(db)->lock);                                \
    MTLK_RF_MGMT_DB_ENSURE_DLIM(db, type, l__);                         \
    if ((db)->dlim && (el)) {                                           \
      mtlk_dlist_push_back(&(db)->data, &((type *)(el))->lentry);       \
      (el) = NULL;                                                      \
    }                                                                   \
    mtlk_osal_lock_release(&(db)->lock);                                \
  }


static mtlk_txmm_clb_action_e __MTLK_IFUNC 
_mtlk_rf_mgmt_sp_send_clb (mtlk_handle_t          clb_usr_data, 
                           mtlk_txmm_data_t*      man_entry, 
                           mtlk_txmm_clb_reason_e reason)
{
  UMI_VSAF_INFO                       *vsaf_info = (UMI_VSAF_INFO *)man_entry->payload;
  MTLK_VS_ACTION_FRAME_PAYLOAD_HEADER *vsaf_hdr  = NULL;
  MTLK_VS_ACTION_FRAME_ITEM_HEADER    *item_hdr  = NULL;
  void                                *sp_data   = NULL;

  vsaf_hdr  = (MTLK_VS_ACTION_FRAME_PAYLOAD_HEADER *)vsaf_info->au8Data;
  item_hdr  = (MTLK_VS_ACTION_FRAME_ITEM_HEADER *)(vsaf_info->au8Data + sizeof(*vsaf_hdr));
  sp_data   = (void *)(vsaf_info->au8Data + sizeof(*vsaf_hdr) + sizeof(*item_hdr));

  if (reason == MTLK_TXMM_CLBR_CONFIRMED) {
    ILOG2(GID_ASEL, "CFMed: SP DA=%Y", vsaf_info->sDA.au8Addr);
  }
  else {
    ELOG("Send failed (r#%d): SP DA=%Y", reason, vsaf_info->sDA.au8Addr);
  }

  return MTLK_TXMM_CLBA_FREE;
}

static uint16
_mtlk_rf_mgmt_fill_sp (mtlk_rf_mgmt_t *rf_mgmt, 
                       UMI_VSAF_INFO  *vsaf_info, 
                       const void     *data,
                       uint32          data_size,     
                       uint8           rf_mgmt_data,
                       uint8           rank)
{
  MTLK_VS_ACTION_FRAME_PAYLOAD_HEADER *vsaf_hdr = NULL;
  MTLK_VS_ACTION_FRAME_ITEM_HEADER    *item_hdr = NULL;
  void                                *sp_data  = NULL;

  /* Get pointers */
  vsaf_hdr  = (MTLK_VS_ACTION_FRAME_PAYLOAD_HEADER *)vsaf_info->au8Data;
  item_hdr  = (MTLK_VS_ACTION_FRAME_ITEM_HEADER *)(vsaf_info->au8Data + sizeof(*vsaf_hdr));
  sp_data   = (void *)(vsaf_info->au8Data + sizeof(*vsaf_hdr) + sizeof(*item_hdr));

  /* Format UMI message (sDA must be set ouside this function) */
  vsaf_info->u8Category   = ACTION_FRAME_CATEGORY_VENDOR_SPECIFIC;
  vsaf_info->au8OUI[0]    = MTLK_OUI_0;
  vsaf_info->au8OUI[1]    = MTLK_OUI_1;
  vsaf_info->au8OUI[2]    = MTLK_OUI_2;
  vsaf_info->u8RFMgmtData = rf_mgmt_data;
  vsaf_info->u8Rank       = rank;
  vsaf_info->u16Size      = HOST_TO_MAC16(sizeof(*vsaf_hdr) + 
                                          sizeof(*item_hdr) + 
                                          data_size);

  /* Format VSAF header */
  vsaf_hdr->u32Version    = HOST_TO_WLAN32(CURRENT_VSAF_FMT_VERSION);
  vsaf_hdr->u32DataSize   = HOST_TO_WLAN32(sizeof(*item_hdr) + data_size);
  vsaf_hdr->u32nofItems   = HOST_TO_WLAN32(1);

  /* Format VSAF SP Item header */
  item_hdr->u32DataSize   = HOST_TO_WLAN32(data_size);
  item_hdr->u32ID         = HOST_TO_WLAN32(MTLK_VSAF_ITEM_ID_SP);

  /* Format VSAF SP Item data */
  memcpy(sp_data, data, data_size);

  /* return VSAF payload size */
  return (uint16)(sizeof(*vsaf_hdr) + /* MTLK_VS_ACTION_FRAME_PAYLOAD_HEADER */
                  sizeof(*item_hdr) + /* MTLK_VS_ACTION_FRAME_ITEM_HEADER    */
                  data_size);         /* SP data                             */
}

static int
_mtlk_rf_mgmt_send_access_type_blocked (mtlk_rf_mgmt_t  *rf_mgmt, 
                                        UMI_RF_MGMT_TYPE *data,
                                        BOOL              set)
{
  int               res       = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg,
                                                 rf_mgmt->cfg.txmm,
                                                 &res);
  if (!man_entry) {
    ELOG("Can't set RF MGMT type due to lack of MM (err=%d)", res);
    goto end;
  }

  man_entry->id           = set?UM_MAN_RF_MGMT_SET_TYPE_REQ:
                                UM_MAN_RF_MGMT_GET_TYPE_REQ;
  man_entry->payload_size = sizeof(*data);

  memcpy(man_entry->payload, data, sizeof(*data));
  
  res = mtlk_txmm_msg_send_blocked(&man_msg, RF_MGMT_SET_TYPE_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG("Can't set RF MGMT type due to lTXMM err#%d", res);
    goto end;
  }

  memcpy(data, man_entry->payload, sizeof(*data));

  if (data->u16Status != UMI_OK) {
    res = MTLK_ERR_MAC;
    goto end;
  }

  if (set) {
    rf_mgmt->type = data->u8RFMType;
  }
  res = MTLK_ERR_OK;

end:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return res;
}

static int
_mtlk_rf_mgmt_send_def_data_blocked (mtlk_rf_mgmt_t       *rf_mgmt, 
                                     BOOL                  set,
                                     UMI_DEF_RF_MGMT_DATA *data)
{
  int               res       = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, rf_mgmt->cfg.txmm, NULL);
  if (!man_entry) {
    ELOG("Can't %s default RF MGMT data due to lack of MAN_MSG", set?"set":"get");
    res = MTLK_ERR_NO_RESOURCES;
    goto end;
  }

  man_entry->id           = set?
    UM_MAN_SET_DEF_RF_MGMT_DATA_REQ:UM_MAN_GET_DEF_RF_MGMT_DATA_REQ;
  man_entry->payload_size = sizeof(*data);

  memcpy(man_entry->payload, data, sizeof(*data));

  res = mtlk_txmm_msg_send_blocked(&man_msg, 
                                   RF_MGMT_SEND_DATA_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG("default RF MGMT data sending(b) error#%d", res);
    goto end;
  }

  memcpy(data, man_entry->payload, sizeof(*data));

  if (data->u8Status != UMI_OK) {
    ELOG("RF MGMT data %s MAC error#%d", set?"set":"get", data->u8Status);
    goto end;
  }

  rf_mgmt->def_rf_mgmt_data = data->u8Data;
  res                       = MTLK_ERR_OK;

end:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return res;
}

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_set_type (mtlk_handle_t      context,
                             const mtlk_guid_t *evt,
                             void              *buffer,
                             uint32            *size)
{
  mtlk_rf_mgmt_t               *rf_mgmt = HANDLE_T_PTR(mtlk_rf_mgmt_t, context);
  struct mtlk_rf_mgmt_evt_type *data    =
    (struct mtlk_rf_mgmt_evt_type *)buffer;
  
  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_RF_MGMT_SET_TYPE) == 0);
  MTLK_ASSERT(*size == sizeof(*data));
  MTLK_ASSERT(data->type.u8RFMType == MTLK_RF_MGMT_TYPE_OFF || data->spr_queue_size != 0);

  data->result = _mtlk_rf_mgmt_send_access_type_blocked(rf_mgmt, &data->type, TRUE);
  if (data->result == MTLK_ERR_OK) {
    mtlk_rf_mgmt_db_spr_set_lim(rf_mgmt, data->spr_queue_size);
  }
}

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_get_type (mtlk_handle_t      context,
                             const mtlk_guid_t *evt,
                             void              *buffer,
                             uint32            *size)
{
  mtlk_rf_mgmt_t               *rf_mgmt = HANDLE_T_PTR(mtlk_rf_mgmt_t, context);
  struct mtlk_rf_mgmt_evt_type *data    =
    (struct mtlk_rf_mgmt_evt_type *)buffer;

  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_RF_MGMT_GET_TYPE) == 0);
  MTLK_ASSERT(*size == sizeof(*data));

  data->result = _mtlk_rf_mgmt_send_access_type_blocked(rf_mgmt, &data->type, FALSE);
}

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_set_def_data (mtlk_handle_t      context,
                                 const mtlk_guid_t *evt,
                                 void              *buffer,
                                 uint32            *size)
{
  mtlk_rf_mgmt_t                   *rf_mgmt = HANDLE_T_PTR(mtlk_rf_mgmt_t, context);
  struct mtlk_rf_mgmt_evt_def_data *data    = 
    (struct mtlk_rf_mgmt_evt_def_data*)buffer;

  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_RF_MGMT_SET_DEF_DATA) == 0);
  MTLK_ASSERT(*size == sizeof(*data));

  data->result = _mtlk_rf_mgmt_send_def_data_blocked(rf_mgmt, TRUE, &data->data);
}

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_get_def_data (mtlk_handle_t      context,
                                 const mtlk_guid_t *evt,
                                 void              *buffer,
                                 uint32            *size)
{
  mtlk_rf_mgmt_t                   *rf_mgmt = HANDLE_T_PTR(mtlk_rf_mgmt_t, context);
  struct mtlk_rf_mgmt_evt_def_data *data    = 
    (struct mtlk_rf_mgmt_evt_def_data*)buffer;

  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_RF_MGMT_GET_DEF_DATA) == 0);
  MTLK_ASSERT(*size == sizeof(*data));

  data->result = _mtlk_rf_mgmt_send_def_data_blocked(rf_mgmt, FALSE, &data->data);
}

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_set_peer_data (mtlk_handle_t      context,
                                  const mtlk_guid_t *evt,
                                  void              *buffer,
                                  uint32            *size)
{
  mtlk_rf_mgmt_t                    *rf_mgmt = HANDLE_T_PTR(mtlk_rf_mgmt_t, context);
  struct mtlk_rf_mgmt_evt_peer_data *data    = 
    (struct mtlk_rf_mgmt_evt_peer_data *)buffer;

  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_RF_MGMT_SET_PEER_DATA) == 0);
  MTLK_ASSERT(*size == sizeof(*data));

  data->result = mtlk_rf_mgmt_set_sta_data(rf_mgmt, data->mac, data->rf_mgmt_data);
}

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_get_peer_data (mtlk_handle_t      context,
                                  const mtlk_guid_t *evt,
                                  void              *buffer,
                                  uint32            *size)
{
  mtlk_rf_mgmt_t                    *rf_mgmt = HANDLE_T_PTR(mtlk_rf_mgmt_t, context);
  struct mtlk_rf_mgmt_evt_peer_data *data    = 
    (struct mtlk_rf_mgmt_evt_peer_data *)buffer;

  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_RF_MGMT_GET_PEER_DATA) == 0);
  MTLK_ASSERT(*size == sizeof(*data));

  data->result = mtlk_rf_mgmt_get_sta_data(rf_mgmt, data->mac, &data->rf_mgmt_data);
}

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_send_sp (mtlk_handle_t      context,
                            const mtlk_guid_t *evt,
                            void              *buffer,
                            uint32            *size)
{
  mtlk_rf_mgmt_t                  *rf_mgmt = HANDLE_T_PTR(mtlk_rf_mgmt_t, context);
  struct mtlk_rf_mgmt_evt_send_sp *data    = 
    (struct mtlk_rf_mgmt_evt_send_sp *)buffer;

  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_RF_MGMT_SEND_SP) == 0);
  MTLK_ASSERT(*size == sizeof(*data) + data->data_size);

  if (rf_mgmt->cfg.device_is_busy(HANDLE_T(rf_mgmt->cfg.stadb->nic)))
  {
    data->result = MTLK_ERR_BUSY;
    ILOG4(GID_ASEL, "SP rejected");
  } else {
    data->result = mtlk_rf_mgmt_send_sp_blocked(rf_mgmt,
                                              data->rf_mgmt_data,
                                              data->rank,
                                              mtlk_rf_mgmt_evt_send_sp_data(data),
                                              data->data_size);
    ILOG4(GID_ASEL, "SP accepted");
  }
}

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_get_spr (mtlk_handle_t      context,
                            const mtlk_guid_t *evt,
                            void              *buffer,
                            uint32            *size)
{
  mtlk_rf_mgmt_t                  *rf_mgmt = HANDLE_T_PTR(mtlk_rf_mgmt_t, context);
  struct mtlk_rf_mgmt_evt_get_spr *data    = 
    (struct mtlk_rf_mgmt_evt_get_spr *)buffer;
  mtlk_rf_mgmt_spr_t              *spr;

  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_RF_MGMT_GET_SPR) == 0);

  /* Get next SPR from the DB */
  spr = mtlk_rf_mgmt_db_spr_get(rf_mgmt);

  if (!spr) { /* There are no SPRs waiting */
    data->result = MTLK_ERR_NOT_READY;
    goto end;
  }
  
  if (data->buffer_size < spr->data_size) { /* Data buffer is too small */
    /* Put the SPR back to the DB */
    mtlk_rf_mgmt_db_spr_return(rf_mgmt, spr);
    data->buffer_size = spr->data_size;
    data->result      = MTLK_ERR_BUF_TOO_SMALL;
    goto end;
  }

  /* Copy actual SPR Src Address and Data */
  memcpy(data->mac, spr->src_addr.au8Addr, ETH_ALEN);
  memcpy(mtlk_rf_mgmt_evt_get_spr_data(data), 
         spr->data,
         spr->data_size);

  /* Release the SPR since we don't need it anymore */
  mtlk_rf_mgmt_db_spr_release(rf_mgmt, spr);

  data->buffer_size = spr->data_size;
  data->result      = MTLK_ERR_OK;

end:
  return;
}


MTLK_INIT_STEPS_LIST_BEGIN(mtlkasel)
  MTLK_INIT_STEPS_LIST_ENTRY(mtlkasel, IRB_REG)
  MTLK_INIT_STEPS_LIST_ENTRY(mtlkasel, TXMM_MSG_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(mtlkasel, DLIST_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(mtlkasel, LOCK_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(mtlkasel, SPR_SET_LIM)
MTLK_INIT_INNER_STEPS_BEGIN(mtlkasel)
MTLK_INIT_STEPS_LIST_END(mtlkasel);

int __MTLK_IFUNC
mtlk_rf_mgmt_init (mtlk_rf_mgmt_t *rf_mgmt, const mtlk_rf_mgmt_cfg_t *cfg)
{
  int i   = 0;

  MTLK_ASSERT(cfg        != NULL);
  MTLK_ASSERT(cfg->stadb != NULL);
  MTLK_ASSERT(cfg->txmm  != NULL);
  MTLK_ASSERT(ARRAY_SIZE(rf_mgmt->irbh) == ARRAY_SIZE(rf_mgmt_drv_evts) ||
              !"Wrong RF MGMT IRB DRV events number");

  rf_mgmt->cfg = *cfg;

  MTLK_INIT_TRY(mtlkasel, MTLK_OBJ_PTR(rf_mgmt))
    for (i = 0; i < ARRAY_SIZE(rf_mgmt_drv_evts); ++i) {
      MTLK_INIT_STEP_LOOP_EX(mtlkasel, IRB_REG, MTLK_OBJ_PTR(rf_mgmt),
                             mtlk_irb_register, 
                             (rf_mgmt_drv_evts[i].evt, 1, rf_mgmt_drv_evts[i].func, HANDLE_T(rf_mgmt)),
                             rf_mgmt->irbh[i],
                             rf_mgmt->irbh[i],
                             MTLK_ERR_NO_RESOURCES);
    }  
    for (i = 0; i < ARRAY_SIZE(rf_mgmt->vsaf_man_msg); ++i) {
      MTLK_INIT_STEP_LOOP(mtlkasel, TXMM_MSG_INIT, MTLK_OBJ_PTR(rf_mgmt),
                          mtlk_txmm_msg_init, (&rf_mgmt->vsaf_man_msg[i]));	
    }                         
    MTLK_INIT_STEP_VOID(mtlkasel, DLIST_INIT, MTLK_OBJ_PTR(rf_mgmt), 
                        mtlk_dlist_init, (&rf_mgmt->spr_db.data));  
    MTLK_INIT_STEP(mtlkasel, LOCK_INIT, MTLK_OBJ_PTR(rf_mgmt), 
                   mtlk_osal_lock_init, (&rf_mgmt->spr_db.lock));
    MTLK_INIT_STEP_VOID(mtlkasel, SPR_SET_LIM, MTLK_OBJ_PTR(rf_mgmt),
                        MTLK_NOACTION, ());
  MTLK_INIT_FINALLY(mtlkasel, MTLK_OBJ_PTR(rf_mgmt))
  MTLK_INIT_RETURN(mtlkasel, MTLK_OBJ_PTR(rf_mgmt), mtlk_rf_mgmt_cleanup, (rf_mgmt));
}

int __MTLK_IFUNC
mtlk_rf_mgmt_set_type_blocked (mtlk_rf_mgmt_t *rf_mgmt, uint8 type)
{
  UMI_RF_MGMT_TYPE  rf_mgmt_type;

  rf_mgmt_type.u8RFMType = type;

  return _mtlk_rf_mgmt_send_access_type_blocked(rf_mgmt, &rf_mgmt_type, TRUE);
}

int __MTLK_IFUNC
mtlk_rf_mgmt_set_def_data_blocked (mtlk_rf_mgmt_t *rf_mgmt, 
                                   uint8           rf_mgmt_data)
{

  UMI_DEF_RF_MGMT_DATA rf_mgmt_def_data;

  rf_mgmt_def_data.u8Data = rf_mgmt_data;
  
  return _mtlk_rf_mgmt_send_def_data_blocked(rf_mgmt, TRUE, &rf_mgmt_def_data);
}

int __MTLK_IFUNC
mtlk_rf_mgmt_set_sta_data (mtlk_rf_mgmt_t *rf_mgmt, 
                           const uint8    *mac_addr,
                           uint8           rf_mgmt_data)
{
  int res    = MTLK_ERR_NOT_IN_USE;
  int sta_id = mtlk_stadb_find_sta(rf_mgmt->cfg.stadb, mac_addr);

  if (sta_id >= 0) {
    sta_entry *sta = mtlk_stadb_get_sta_by_id(rf_mgmt->cfg.stadb, sta_id);
    
    mtlk_stadb_set_sta_rf_mgmt_data(sta, rf_mgmt_data);

    res = MTLK_ERR_OK;
  }

  return res;
}

int   __MTLK_IFUNC 
mtlk_rf_mgmt_get_sta_data (mtlk_rf_mgmt_t *rf_mgmt, 
                           const uint8    *mac_addr,
                           uint8          *rf_mgmt_data)
{
  int res    = MTLK_ERR_NOT_IN_USE;
  int sta_id = mtlk_stadb_find_sta(rf_mgmt->cfg.stadb, mac_addr);

  MTLK_ASSERT(rf_mgmt_data != NULL);

  if (sta_id >= 0) {
    sta_entry *sta = mtlk_stadb_get_sta_by_id(rf_mgmt->cfg.stadb, sta_id);
    
    *rf_mgmt_data = mtlk_stadb_get_sta_rf_mgmt_data(sta);

    res = MTLK_ERR_OK;
  }

  return res;
}

int  __MTLK_IFUNC
mtlk_rf_mgmt_send_sp (mtlk_rf_mgmt_t *rf_mgmt, 
                      uint8           rf_mgmt_data, 
                      uint8           rank,
                      const void     *data, 
                      uint32          data_size)
{
  int                         res = MTLK_ERR_UNKNOWN;
  int                         i   = 0;

  for (; i < STA_MAX_STATIONS; i++) {
    sta_entry *sta = mtlk_stadb_get_sta_by_id(rf_mgmt->cfg.stadb, i);

    if (sta->state == PEER_CONNECTED) {
      mtlk_txmm_data_t *man_entry = NULL;
      UMI_VSAF_INFO    *vsaf_info = NULL; 
  
      man_entry = mtlk_txmm_msg_get_empty_data(&rf_mgmt->vsaf_man_msg[i], 
                                               rf_mgmt->cfg.txmm);
      if (!man_entry) {
        ELOG("Can't send Sounding Packet due to lack of MAN_MSG");
        res = MTLK_ERR_NO_RESOURCES;
        goto end;
      }

      vsaf_info = (UMI_VSAF_INFO *)man_entry->payload;

      man_entry->id           = UM_MAN_SEND_MTLK_VSAF_REQ;
      man_entry->payload_size = 
        /* header size before the data buffer + data size of VSAF with SP */
        sizeof(*vsaf_info) - sizeof(vsaf_info->au8Data) +
        _mtlk_rf_mgmt_fill_sp(rf_mgmt, vsaf_info, data, data_size, rf_mgmt_data, rank);

      memcpy(vsaf_info->sDA.au8Addr, sta->mac, ETH_ALEN);

      ILOG2(GID_ASEL, "SP sending to %Y", sta->mac);
      res = mtlk_txmm_msg_send(&rf_mgmt->vsaf_man_msg[i],
                               _mtlk_rf_mgmt_sp_send_clb,
                               HANDLE_T(rf_mgmt),
                               RF_MGMT_SEND_SP_VSAF_TIMEOUT);
      if (res != MTLK_ERR_OK) {
        ELOG("SP sending error#%d", res);
        goto end;
      }
    }
  }

  res = MTLK_ERR_OK;

end:
  return res;
}

int __MTLK_IFUNC
mtlk_rf_mgmt_send_sp_blocked (mtlk_rf_mgmt_t *rf_mgmt, 
                              uint8           rf_mgmt_data,
                              uint8           rank,
                              const void     *data, 
                              uint32          data_size)
{
  int                 res       = MTLK_ERR_UNKNOWN;
  int                 i         = 0;
  mtlk_txmm_msg_t     man_msg;
  mtlk_txmm_data_t   *man_entry = NULL;
  UMI_VSAF_INFO      *vsaf_info = NULL; 

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, rf_mgmt->cfg.txmm, NULL);
  if (!man_entry) {
    ELOG("Can't send Sounding Packet due to lack of MAN_MSG");
    res = MTLK_ERR_NO_RESOURCES;
    goto end;
  }

  vsaf_info = (UMI_VSAF_INFO *)man_entry->payload;

  man_entry->id           = UM_MAN_SEND_MTLK_VSAF_REQ;
  man_entry->payload_size = 
    /* header size before the data buffer + data size of VSAF with SP */
    sizeof(*vsaf_info) - sizeof(vsaf_info->au8Data) +
    _mtlk_rf_mgmt_fill_sp(rf_mgmt, vsaf_info, data, data_size, rf_mgmt_data, rank);

  for (; i < STA_MAX_STATIONS; i++) {
    sta_entry *sta = mtlk_stadb_get_sta_by_id(rf_mgmt->cfg.stadb, i);

    if (sta->state == PEER_CONNECTED) {
      memcpy(vsaf_info->sDA.au8Addr, sta->mac, ETH_ALEN);

      ILOG2(GID_ASEL, "SP(b) sending to %Y", sta->mac);
      res = mtlk_txmm_msg_send_blocked(&man_msg, 
                                       RF_MGMT_SEND_SP_VSAF_TIMEOUT);
      if (res != MTLK_ERR_OK) {
        ELOG("SP sending(b) error#%d", res);
        goto end;
      }
    }
  }

  res = MTLK_ERR_OK;

end:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return res;
}

int  __MTLK_IFUNC
mtlk_rf_mgmt_handle_spr (mtlk_rf_mgmt_t  *rf_mgmt, 
                         const IEEE_ADDR *src_addr, 
                         uint8           *buffer, 
                         uint16           size)
{
  int                          res       = MTLK_ERR_UNKNOWN;
  mtlk_rf_mgmt_db_entry_spr_t *spr_entry = NULL;
  struct mtlk_rf_mgmt_evt_spr_arrived spr_evt;

  MTLK_ASSERT(buffer != NULL);
  MTLK_ASSERT(size != 0);

  ILOG2(GID_ASEL, "SPR received: src=%Y size=%d bytes", src_addr->au8Addr, size);

  if (!rf_mgmt->spr_db.dlim) {
    /* no SPR dbg records required */
    res = MTLK_ERR_OK;
    goto end;
  }

  spr_entry = 
    (mtlk_rf_mgmt_db_entry_spr_t *)mtlk_osal_mem_alloc(sizeof(*spr_entry) -
                                                       sizeof(spr_entry->spr.data) + 
                                                       size,
                                                       MTLK_MEM_TAG_RFMGMT);
  if (!spr_entry) {
    ELOG("Can't allocate SPR entry of %d bytes", 
          sizeof(*spr_entry) - sizeof(spr_entry->spr.data) + size);
    res = MTLK_ERR_NO_MEM;
    goto end;
  }

  spr_entry->spr.src_addr  = *src_addr;
  spr_entry->spr.data_size = size;
  memcpy(spr_entry->spr.data, buffer, size);

  MTLK_RF_MGMT_DB_ADD_ENSURE_DLIM(&rf_mgmt->spr_db, spr_entry, mtlk_rf_mgmt_db_entry_spr_t);

  if (spr_entry) { /* has not been added for some reason */
    mtlk_osal_mem_free(spr_entry);
    res = MTLK_ERR_UNKNOWN;
    goto end;
  }

  spr_evt.required_buff_size = size;

  res = mtlk_irb_notify_app(&IRBE_RF_MGMT_SPR_ARRIVED, &spr_evt, sizeof(spr_evt));
  if (res != MTLK_ERR_OK) {
    WLOG("Can't notify IRB application (err = %d)", res); 
  }
  
  res = MTLK_ERR_OK;

end:
  return res;
}

uint32 __MTLK_IFUNC
mtlk_rf_mgmt_db_spr_get_available (mtlk_rf_mgmt_t *rf_mgmt)
{
  uint32 res;

  MTLK_ASSERT(rf_mgmt != NULL);

  mtlk_osal_lock_acquire(&rf_mgmt->spr_db.lock);
  res = mtlk_dlist_size(&rf_mgmt->spr_db.data);
  mtlk_osal_lock_release(&rf_mgmt->spr_db.lock);  

  return res;
}

mtlk_rf_mgmt_spr_t * __MTLK_IFUNC
mtlk_rf_mgmt_db_spr_get (mtlk_rf_mgmt_t *rf_mgmt)
{
  mtlk_rf_mgmt_spr_t  *spr = NULL;
  mtlk_ldlist_entry_t *e   = NULL;

  mtlk_osal_lock_acquire(&rf_mgmt->spr_db.lock);
  e = mtlk_dlist_pop_front(&rf_mgmt->spr_db.data);
  mtlk_osal_lock_release(&rf_mgmt->spr_db.lock);

  if (e != NULL) {
    mtlk_rf_mgmt_db_entry_spr_t *spr_entry = 
      MTLK_CONTAINER_OF(e, mtlk_rf_mgmt_db_entry_spr_t, lentry);
    spr = &spr_entry->spr;
  }

  return spr;
}

void   __MTLK_IFUNC
mtlk_rf_mgmt_db_spr_release (mtlk_rf_mgmt_t     *rf_mgmt,
                             mtlk_rf_mgmt_spr_t *spr)
{
  mtlk_rf_mgmt_db_entry_spr_t *spr_entry = 
    MTLK_CONTAINER_OF(spr, mtlk_rf_mgmt_db_entry_spr_t, spr);

  mtlk_osal_mem_free(spr_entry);
}

void   __MTLK_IFUNC
mtlk_rf_mgmt_db_spr_return (mtlk_rf_mgmt_t     *rf_mgmt,
                            mtlk_rf_mgmt_spr_t *spr)
{
  mtlk_rf_mgmt_db_entry_spr_t *spr_entry = 
    MTLK_CONTAINER_OF(spr, mtlk_rf_mgmt_db_entry_spr_t, spr);

  mtlk_osal_lock_acquire(&rf_mgmt->spr_db.lock);
  mtlk_dlist_push_front(&rf_mgmt->spr_db.data, &spr_entry->lentry);
  mtlk_osal_lock_release(&rf_mgmt->spr_db.lock);
}

void __MTLK_IFUNC
mtlk_rf_mgmt_db_spr_set_lim (mtlk_rf_mgmt_t *rf_mgmt,
                             uint32          lim)
{
  MTLK_ASSERT(rf_mgmt != NULL);

  mtlk_osal_lock_acquire(&rf_mgmt->spr_db.lock);
  rf_mgmt->spr_db.dlim = lim;
  MTLK_RF_MGMT_DB_ENSURE_DLIM(&rf_mgmt->spr_db, mtlk_rf_mgmt_db_entry_spr_t, lim);
  mtlk_osal_lock_release(&rf_mgmt->spr_db.lock);
}

uint32 __MTLK_IFUNC
mtlk_rf_mgmt_db_spr_get_lim (mtlk_rf_mgmt_t *rf_mgmt)
{
  return rf_mgmt->spr_db.dlim;
}

void __MTLK_IFUNC
mtlk_rf_mgmt_cleanup (mtlk_rf_mgmt_t *rf_mgmt)
{
  int i = 0;

  MTLK_CLEANUP_BEGIN(mtlkasel, MTLK_OBJ_PTR(rf_mgmt))
    MTLK_CLEANUP_STEP(mtlkasel, SPR_SET_LIM, MTLK_OBJ_PTR(rf_mgmt),
                      mtlk_rf_mgmt_db_spr_set_lim, (rf_mgmt, 0));
    MTLK_CLEANUP_STEP(mtlkasel, LOCK_INIT, MTLK_OBJ_PTR(rf_mgmt),
                      mtlk_osal_lock_cleanup, (&rf_mgmt->spr_db.lock));
    MTLK_CLEANUP_STEP(mtlkasel, DLIST_INIT, MTLK_OBJ_PTR(rf_mgmt),
                      mtlk_dlist_cleanup, (&rf_mgmt->spr_db.data));
    for (i = 0; MTLK_CLEANUP_ITERATONS_LEFT(MTLK_OBJ_PTR(rf_mgmt), TXMM_MSG_INIT) > 0; i++) {
      MTLK_CLEANUP_STEP_LOOP(mtlkasel, TXMM_MSG_INIT, MTLK_OBJ_PTR(rf_mgmt),
                             mtlk_txmm_msg_cleanup, (&rf_mgmt->vsaf_man_msg[i]));
    }
    for (i = 0; MTLK_CLEANUP_ITERATONS_LEFT(MTLK_OBJ_PTR(rf_mgmt), IRB_REG) > 0; i++) {
      MTLK_CLEANUP_STEP_LOOP(mtlkasel, IRB_REG, MTLK_OBJ_PTR(rf_mgmt),
                             mtlk_irb_unregister, (rf_mgmt->irbh[i]));
    }
  MTLK_CLEANUP_END(mtlkasel, MTLK_OBJ_PTR(rf_mgmt));
}




