/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/

#include "mtlkinc.h"
#include "addba.h"
#include "mtlkqos.h"
#include "mtlkaux.h"

#define ADDBA_TU_TO_MS(nof_tu) ((nof_tu) * MTLK_ADDBA_BA_TIMEOUT_UNIT_US / 1000) /* 1TU = 1024 us */

static void
_mtlk_addba_on_addba_req_sent(mtlk_addba_t      *obj,
                              mtlk_addba_peer_t *peer,
                              uint16             tid_idx);
static void
_mtlk_addba_fill_addba_req(mtlk_addba_t       *obj,
                           const IEEE_ADDR    *addr,
                           mtlk_addba_peer_t  *peer,
                           uint16              tid_idx,
                           mtlk_txmm_data_t   *tx_data);

static __INLINE int
_mtlk_addba_is_allowed_rate (uint16 rate) 
{
  int res = 1;
  if (rate != MTLK_ADDBA_RATE_ADAPTIVE &&
      !mtlk_aux_is_11n_rate((uint8)rate)) {
    res = 0;
  }

  return res;
}

static __INLINE BOOL
_mtlk_addba_reset_tx_state (mtlk_addba_t      *obj,
                            mtlk_addba_peer_t *peer,
                            uint16             tid_idx)
{
  BOOL res = FALSE;

  if (peer->tid[tid_idx].tx.state != MTLK_ADDBA_TX_NONE)
  {
    if (peer->tid[tid_idx].tx.aggr_on) {
      peer->tid[tid_idx].tx.aggr_on = FALSE;
      --obj->nof_aggr_on;
      res = TRUE;
    }
    peer->tid[tid_idx].tx.state = MTLK_ADDBA_TX_NONE;
  }

  return res;
}

static __INLINE BOOL
_mtlk_addba_reset_rx_state (mtlk_addba_t      *obj,
                            mtlk_addba_peer_t *peer,
                            uint16             tid_idx)
{
  BOOL res = FALSE;

  if (peer->tid[tid_idx].rx.state != MTLK_ADDBA_RX_NONE)
  {
    if (peer->tid[tid_idx].rx.reord_on) {
     peer->tid[tid_idx].rx.reord_on = FALSE;
     --obj->nof_reord_on;
       res = TRUE;
    }
    peer->tid[tid_idx].rx.state = MTLK_ADDBA_RX_NONE;
  }

  return res;
}

static __INLINE void
_mtlk_addba_correct_res_win_size (mtlk_addba_t *obj,
                                  uint8        *win_size)
{
  uint8 res = *win_size;

  MTLK_UNREFERENCED_PARAM(obj);
  if (!res)
  {
    res = MTLK_ADDBA_MAX_REORD_WIN_SIZE;
  }

  if (*win_size != res)
  {
    ILOG2(GID_ADDBA, "WinSize correction: %d=>%d", (int)*win_size, (int)res);
    *win_size = res;
  }
}

#define _mtlk_addba_correct_res_timeout(o,t) /* VOID, We accept any timeout */

#define _mtlk_addba_peer(o,a)      _mtlk_addba_peer_ex((o), (a), NULL)

static __INLINE mtlk_addba_peer_t*
_mtlk_addba_peer_ex (mtlk_addba_t    *obj,
                     const IEEE_ADDR *addr,
                     mtlk_handle_t   *param)
{
  mtlk_addba_peer_t* peer = obj->api.get_peer(obj->api.usr_data, addr, param);
  if (!peer)
  {
     WLOG("No peer found for %Y", addr->au8Addr);
  }

  return peer;
}

/******************************************************************************************************
 * CFM callbacks for TXMM
*******************************************************************************************************/
static mtlk_txmm_clb_action_e __MTLK_IFUNC
_mtlk_addba_on_delba_req_tx_cfm_clb (mtlk_handle_t          clb_usr_data,
                                     mtlk_txmm_data_t      *data,
                                     mtlk_txmm_clb_reason_e reason)
{
  mtlk_addba_t*       obj       = (mtlk_addba_t*)clb_usr_data;
  UMI_DELBA_REQ_SEND* delba_req = (UMI_DELBA_REQ_SEND*)data->payload;
  mtlk_addba_peer_t*  peer      = NULL;
  uint16              tid_idx   = MAC_TO_HOST16(delba_req->u16AccessProtocol);
  mtlk_handle_t       reord_val = HANDLE_T(0);
  int                 tx_path   = MAC_TO_HOST16(delba_req->u16Intiator);
  BOOL                stop      = FALSE;

  if (reason != MTLK_TXMM_CLBR_CONFIRMED)
  {
    ELOG("%s %Y TID=%d DELBA isn't confirmed",
          tx_path ? "TX":"RX", delba_req->sDA.au8Addr, (int)tid_idx);
  }
  else
  {
    ILOG2(GID_ADDBA, "%s %Y TID=%d DELBA sent",
         tx_path ? "TX":"RX", delba_req->sDA.au8Addr, (int)tid_idx);
  }

  peer = _mtlk_addba_peer_ex(obj, &delba_req->sDA, &reord_val);
  if (!peer) {
    ELOG("no peer found");
    goto end;
  }

  mtlk_osal_lock_acquire(&obj->lock);
  if (tx_path)
  {
    /* Anton: on Tx should already be NONE after mtlk_addba_on_del_aggr_cfm */
    _mtlk_addba_reset_tx_state(obj, peer, tid_idx);
  }
  else
  {
    stop = _mtlk_addba_reset_rx_state(obj, peer, tid_idx);
  }
  mtlk_osal_lock_release(&obj->lock);

  if (stop)
    obj->api.on_stop_reordering(obj->api.usr_data, reord_val, tid_idx);

end:
  return MTLK_TXMM_CLBA_FREE;
}

static mtlk_txmm_clb_action_e __MTLK_IFUNC
_mtlk_addba_on_close_aggr_cfm_clb (mtlk_handle_t          clb_usr_data,
                                   mtlk_txmm_data_t      *data,
                                   mtlk_txmm_clb_reason_e reason)
{
  mtlk_addba_t* obj = (mtlk_addba_t*)clb_usr_data;
  UMI_CLOSE_AGGR_REQ *close_aggr_req = (UMI_CLOSE_AGGR_REQ*)data->payload;
  mtlk_addba_peer_t *peer;
  uint16 tid_idx = MAC_TO_HOST16(close_aggr_req->u16AccessProtocol);
  BOOL stop = FALSE;
  mtlk_handle_t container_handle = HANDLE_T(0);

  if (reason != MTLK_TXMM_CLBR_CONFIRMED)
  {
    ELOG("TX %Y TID=%d aggregation closing isn't confirmed",
          close_aggr_req->sDA.au8Addr, tid_idx);
  }
  else
  {
    ILOG2(GID_ADDBA, "TX %Y TID=%d aggregation closed",
         close_aggr_req->sDA.au8Addr, tid_idx);
  }

  peer = _mtlk_addba_peer_ex(obj, &close_aggr_req->sDA, &container_handle);
  if (!peer) {
    ELOG("no peer found");
    goto end;
  }

  mtlk_osal_lock_acquire(&obj->lock);
  stop = _mtlk_addba_reset_tx_state(obj, peer, tid_idx);
  mtlk_osal_lock_release(&obj->lock);

  if (stop)
  {
    obj->api.on_stop_aggregation(obj->api.usr_data, container_handle, tid_idx);
  }

end:
  return MTLK_TXMM_CLBA_FREE;
}

static mtlk_txmm_clb_action_e __MTLK_IFUNC
_mtlk_addba_on_open_aggr_cfm_clb (mtlk_handle_t          clb_usr_data,
                                  mtlk_txmm_data_t      *data,
                                  mtlk_txmm_clb_reason_e reason)
{
  mtlk_addba_t* obj = (mtlk_addba_t*)clb_usr_data;
  UMI_OPEN_AGGR_REQ *add_aggr_req = (UMI_OPEN_AGGR_REQ*)data->payload;
  mtlk_addba_peer_t *peer;
  uint16 tid_idx = MAC_TO_HOST16(add_aggr_req->u16AccessProtocol);
  mtlk_handle_t container_handle = HANDLE_T(0);

  if (reason != MTLK_TXMM_CLBR_CONFIRMED)
  {
    ELOG("TX %Y TID=%d aggregation opening isn't confirmed",
          add_aggr_req->sDA.au8Addr, tid_idx);
    goto FAIL;
  }
  else
  {
    ILOG2(GID_ADDBA, "TX %Y TID=%d aggregation opened", add_aggr_req->sDA.au8Addr, tid_idx);
  }

  peer = _mtlk_addba_peer_ex(obj, &add_aggr_req->sDA, &container_handle);
  if (!peer) {
    ELOG("no peer found");
    goto FAIL;
  }

  mtlk_osal_lock_acquire(&obj->lock);
  MTLK_ASSERT(peer->tid[tid_idx].tx.aggr_on == FALSE);
  peer->tid[tid_idx].tx.state = MTLK_ADDBA_TX_AGGR_OPENED;
  peer->tid[tid_idx].tx.aggr_on = TRUE;
  mtlk_osal_lock_release(&obj->lock);

  obj->api.on_start_aggregation(obj->api.usr_data, container_handle, tid_idx);
  return MTLK_TXMM_CLBA_FREE;

FAIL:
  mtlk_osal_lock_acquire(&obj->lock);
  --obj->nof_aggr_on;
  mtlk_osal_lock_release(&obj->lock);

  return MTLK_TXMM_CLBA_FREE;
}

static mtlk_txmm_clb_action_e __MTLK_IFUNC
_mtlk_addba_on_addba_req_tx_cfm_clb (mtlk_handle_t          clb_usr_data,
                                     mtlk_txmm_data_t      *data,
                                     mtlk_txmm_clb_reason_e reason)
{
  mtlk_addba_t*       obj       = (mtlk_addba_t*)clb_usr_data;
  UMI_ADDBA_REQ_SEND* addba_req = (UMI_ADDBA_REQ_SEND*)data->payload;
  mtlk_addba_peer_t*  peer      = NULL;
  uint16              tid_idx   = MAC_TO_HOST16(addba_req->u16AccessProtocol);

  if (reason != MTLK_TXMM_CLBR_CONFIRMED)
  {
    ELOG("TX %Y TID=%d TAG=%d request isn't confirmed",
          addba_req->sDA.au8Addr, tid_idx, addba_req->u8DialogToken);
  }
  else
  {
    ILOG2(GID_ADDBA, "TX %Y TID=%d TAG=%d request sent",
         addba_req->sDA.au8Addr, tid_idx, addba_req->u8DialogToken);
  }

  peer = _mtlk_addba_peer(obj, &addba_req->sDA);
  if (!peer) {
    ELOG("no peer found");
    goto end;
  }

  mtlk_osal_lock_acquire(&obj->lock);
  peer->tid[tid_idx].tx.state               = MTLK_ADDBA_TX_ADDBA_REQ_CFMD;
  peer->tid[tid_idx].tx.addba_req_cfmd_time = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
  mtlk_osal_lock_release(&obj->lock);

end:
  return MTLK_TXMM_CLBA_FREE;
}

static mtlk_txmm_clb_action_e __MTLK_IFUNC
_mtlk_addba_on_addba_res_tx_cfm_clb (mtlk_handle_t          clb_usr_data,
                                     mtlk_txmm_data_t*      data,
                                     mtlk_txmm_clb_reason_e reason)
{
  mtlk_addba_t*       obj       = (mtlk_addba_t*)clb_usr_data;
  UMI_ADDBA_RES_SEND* addba_res = (UMI_ADDBA_RES_SEND*)data->payload;
  mtlk_addba_peer_t*  peer      = NULL;
  uint16              tid_idx   = MAC_TO_HOST16(addba_res->u16AccessProtocol);

  if (reason != MTLK_TXMM_CLBR_CONFIRMED)
  {
    ELOG("RX %Y TID=%d TAG=%d response isn't confirmed",
          addba_res->sDA.au8Addr, tid_idx, addba_res->u8DialogToken);
  }
  else
  {
    ILOG2(GID_ADDBA, "RX %Y TID=%d TAG=%d response sent",
         addba_res->sDA.au8Addr, tid_idx, addba_res->u8DialogToken);
  }

  peer = _mtlk_addba_peer(obj, &addba_res->sDA);
  if (!peer) {
    ELOG("no peer found");
    goto end;
  }

  mtlk_osal_lock_acquire(&obj->lock);
  peer->tid[tid_idx].rx.state = MTLK_ADDBA_RX_REORD_IN_PROCESS;
  mtlk_osal_lock_release(&obj->lock);

end:
  return MTLK_TXMM_CLBA_FREE;
}
/******************************************************************************************************/

static void
_mtlk_addba_on_addba_req_sent (mtlk_addba_t      *obj,
                               mtlk_addba_peer_t *peer,
                               uint16             tid_idx)
{
  peer->tid[tid_idx].tx.state          = MTLK_ADDBA_TX_ADDBA_REQ_SENT;
  peer->tid[tid_idx].tx.addba_req_dlgt = obj->next_dlg_token;

  ++obj->next_dlg_token;
}

static void
_mtlk_addba_fill_addba_req (mtlk_addba_t       *obj,
                            const IEEE_ADDR    *addr,
                            mtlk_addba_peer_t  *peer,
                            uint16              tid_idx,
                            mtlk_txmm_data_t   *tx_data)
{
  uint8 win_size_req = obj->cfg.tid[tid_idx].aggr_win_size;
  UMI_ADDBA_REQ_SEND *addba_req = (UMI_ADDBA_REQ_SEND *)tx_data->payload;

  memset(addba_req, 0, sizeof(*addba_req));

  tx_data->id           = UM_MAN_ADDBA_REQ_TX_REQ;
  tx_data->payload_size = sizeof(*addba_req);

  /* Limit requested window size to sane value */
  if(win_size_req == 0)
  {
    /* This is special case: we do not propose any win size to our peer */
  }
  else if(win_size_req > MTLK_ADDBA_MAX_REORD_WIN_SIZE)
  {
    win_size_req = MTLK_ADDBA_MAX_REORD_WIN_SIZE;
  }
  else if(win_size_req < MTLK_ADDBA_MIN_REORD_WIN_SIZE)
  {
    win_size_req = MTLK_ADDBA_MIN_REORD_WIN_SIZE;
  }

  addba_req->sDA               = *addr;
  addba_req->u8DialogToken     = obj->next_dlg_token;
  addba_req->u16AccessProtocol = HOST_TO_MAC16(tid_idx);
  addba_req->u8BA_WinSize_O    = win_size_req;
  addba_req->u16BATimeout      = HOST_TO_MAC16(obj->cfg.tid[tid_idx].addba_timeout);

  /* Later we will compare this value with the value in ADDBA response */
  peer->tid[tid_idx].tx.win_size_req = win_size_req;

  ILOG2(GID_ADDBA, "TX %Y TID=%d TAG=%d request WSIZE=%d TM=%d",
       addr,
       (int)tid_idx,
       (int)obj->next_dlg_token,
       win_size_req,
       (int)obj->cfg.tid[tid_idx].addba_timeout);
}

static void
_mtlk_addba_tx_addba_req (mtlk_addba_t      *obj,
                          const IEEE_ADDR   *addr,
                          mtlk_addba_peer_t *peer,
                          uint16             tid_idx)
{
  if (peer->tid[tid_idx].tx.state != MTLK_ADDBA_TX_NONE)
  {
    ILOG3(GID_ADDBA, "TX %Y TID=%d: duplicate ADDBA request? - ignored", addr->au8Addr, tid_idx);
    return;
  }

  if (obj->nof_aggr_on < obj->cfg.max_aggr_supported)
  { /* format and send ADDBA request */
    mtlk_txmm_data_t* tx_data = 
      mtlk_txmm_msg_get_empty_data(&peer->tid[tid_idx].tx.man_msg,
                                   obj->api.txmm);
    if (tx_data)
    {
      int sres;

      _mtlk_addba_fill_addba_req(obj, addr, peer, tid_idx, tx_data);

      sres = mtlk_txmm_msg_send(&peer->tid[tid_idx].tx.man_msg, _mtlk_addba_on_addba_req_tx_cfm_clb,
                                HANDLE_T(obj), 0);

      if (sres == MTLK_ERR_OK)
      {
        ++obj->nof_aggr_on;
        _mtlk_addba_on_addba_req_sent(obj, peer, tid_idx);
      }
      else
      {
        ELOG("Can't send ADDBA req due to TXMM err#%d", sres);
      }
    }
    else
    {
      ELOG("Can't send ADDBA req due to lack of MAN_MSG");
    }
  }
  else
  {
    WLOG("TX: ADDBA won't be sent (aggegations limit reached: %d >= %d)",
            obj->nof_aggr_on, obj->cfg.max_aggr_supported);
  }
}

static void 
_mtlk_addba_close_aggr_req (mtlk_addba_t      *obj, 
                            const IEEE_ADDR   *addr, 
                            mtlk_addba_peer_t *peer, 
                            uint16             tid_idx)
{
  mtlk_txmm_data_t* tx_data;
  UMI_CLOSE_AGGR_REQ* close_aggr_req;
  int state = peer->tid[tid_idx].tx.state;
  int sres;

  if (state != MTLK_ADDBA_TX_ADD_AGGR_REQ_SENT &&
      state != MTLK_ADDBA_TX_AGGR_OPENED)
  {
    WLOG("TX %Y TID=%d: trying to close not opened aggregation", addr, tid_idx);
    return;
  }

  tx_data = mtlk_txmm_msg_get_empty_data(&peer->tid[tid_idx].tx.man_msg,
                                         obj->api.txmm);

  if (!tx_data)
  {
    ELOG("Can't close Aggr due to lack of MAN_MSG");
    return;
  }

  tx_data->id           = UM_MAN_CLOSE_AGGR_REQ;
  tx_data->payload_size = sizeof(*close_aggr_req);

  close_aggr_req = (UMI_CLOSE_AGGR_REQ*)tx_data->payload;
  close_aggr_req->sDA               = *addr;
  close_aggr_req->u16AccessProtocol = HOST_TO_MAC16(tid_idx);

  ILOG2(GID_ADDBA, "TX %Y TID=%d closing aggregation", addr, (int)tid_idx);

  sres = mtlk_txmm_msg_send(&peer->tid[tid_idx].tx.man_msg, _mtlk_addba_on_close_aggr_cfm_clb,
                            HANDLE_T(obj), 0);
  if (sres == MTLK_ERR_OK) {
    peer->tid[tid_idx].tx.state = MTLK_ADDBA_TX_DEL_AGGR_REQ_SENT;
  }
  else
  {
    ELOG("Can't close Aggr due to TXMM err#%d", sres);
  }
}

static void
_mtlk_addba_send_delba_req (mtlk_addba_t      *obj,
                          const IEEE_ADDR   *addr,
                          mtlk_addba_peer_t *peer,
                          uint16             tid_idx,
                          int                tx_path)
{
  mtlk_txmm_msg_t  *tx_msg;
  mtlk_txmm_data_t *tx_data;
  uint16            initiator;

  if (tx_path)
  {
    tx_msg    = &peer->tid[tid_idx].tx.man_msg;
    initiator = 1;
  }
  else
  {
    tx_msg    = &peer->tid[tid_idx].rx.man_msg;
    initiator = 0;
  }

  tx_data = mtlk_txmm_msg_get_empty_data(tx_msg, obj->api.txmm);
  if (tx_data)
  {
    UMI_DELBA_REQ_SEND *delba_req = (UMI_DELBA_REQ_SEND*)tx_data->payload;
    int                 sres;

    tx_data->id           = UM_MAN_DELBA_REQ;
    tx_data->payload_size = sizeof(*delba_req);

    delba_req->sDA               = *addr;
    delba_req->u16AccessProtocol = HOST_TO_MAC16(tid_idx);
    delba_req->u16ResonCode      = HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_SUCCESS);
    delba_req->u16Intiator       = HOST_TO_MAC16(initiator);

    ILOG2(GID_ADDBA, "%s %Y TID=%d send DELBA", tx_path ? "TX":"RX", addr, tid_idx);

    sres = mtlk_txmm_msg_send(tx_msg, _mtlk_addba_on_delba_req_tx_cfm_clb,
                              HANDLE_T(obj), 0);
    if (sres != MTLK_ERR_OK)
    {
      ELOG("Can't send DELBA req due to TXMM err#%d", sres);
    }
    else if (tx_path)
      peer->tid[tid_idx].tx.state = MTLK_ADDBA_TX_DELBA_REQ_SENT;
    else
      peer->tid[tid_idx].rx.state = MTLK_ADDBA_RX_DELBA_REQ_SENT;
  }
  else
  {
    ELOG("no msg available");
  }
}

static void
_mtlk_addba_cleanup_peer (mtlk_addba_t      *obj,
                          mtlk_addba_peer_t *peer,
                          const IEEE_ADDR   *addr,
                          mtlk_handle_t      container_handle)
{
  uint16 tid_idx = 0;
  BOOL stop = FALSE;
  MTLK_UNREFERENCED_PARAM(addr);

  for (tid_idx = 0; tid_idx < SIZEOF(peer->tid); tid_idx++)
  {
    mtlk_osal_lock_acquire(&obj->lock);
    stop = _mtlk_addba_reset_tx_state(obj, peer, tid_idx);
    mtlk_osal_lock_release(&obj->lock);

    /* After _mtlk_addba_peer_reset_tx_state invocation state machine */ 
    /* is in state MTLK_ADDBA_TX_NONE so no new retries will be sent */ 
    /* by timer. There still may be outstanding TXMM message with */ 
    /* pending callback. It is not a problem if it is called now, */ 
    /* but since we don't want it to be called later, we have to */ 
    /* cancel the message. */ 
    mtlk_txmm_msg_cancel(&peer->tid[tid_idx].tx.man_msg); 

    if (stop)
    {
      obj->api.on_stop_aggregation(obj->api.usr_data, container_handle, tid_idx);
      ILOG2_YD("TX %Y TID=%d aggregation closed", addr, tid_idx);
    }

    mtlk_osal_lock_acquire(&obj->lock);
    stop = _mtlk_addba_reset_rx_state(obj, peer, tid_idx);
    mtlk_osal_lock_release(&obj->lock);

    /* After _mtlk_addba_peer_reset_rx_state invocation state machine */ 
    /* is in state MTLK_ADDBA_RX_NONE so no new retries will be sent */ 
    /* by timer. There still may be outstanding TXMM message with */ 
    /* pending callback. It is not a problem if it is called now, */ 
    /* but since we don't want it to be called later, we have to */ 
    /* cancel the message. */ 
    mtlk_txmm_msg_cancel(&peer->tid[tid_idx].rx.man_msg); 

    if (stop)
    {
      obj->api.on_stop_reordering(obj->api.usr_data, container_handle, tid_idx);
      ILOG2_YD("RX %Y TID=%d reordering closed", addr, tid_idx);
    }

    /* Reset peer including the is_active flag, but excluding the man_msg's */
    memset(&peer->tid[tid_idx].tx, 0, MTLK_OFFSET_OF(mtlk_addba_peer_tx_t, man_msg));
    memset(&peer->tid[tid_idx].rx, 0, MTLK_OFFSET_OF(mtlk_addba_peer_rx_t, man_msg));
  }
}

static void
_mtlk_addba_start_negotiation (mtlk_addba_t      *obj,
                               mtlk_addba_peer_t *peer,
                               const IEEE_ADDR   *addr)
{
  uint16 tid, i = 0;

  for (i = 0; i < NTS_PRIORITIES; i++)
  {
    tid = mtlk_qos_get_tid_by_ac(i);
    ILOG2(GID_ADDBA, "use_aggr[%d (ac=%d)]=%d", (int)tid, (int)i, obj->cfg.tid[tid].use_aggr);
    if (obj->cfg.tid[tid].use_aggr)
      _mtlk_addba_tx_addba_req(obj, addr, peer, tid);
  }
}

static __INLINE void
_mtlk_addba_check_addba_res_rx_timeouts (mtlk_addba_t      *obj,
                                         const IEEE_ADDR   *addr,
                                         mtlk_addba_peer_t *peer,
                                         mtlk_handle_t      container_handle)
{
  uint8 tid_idx = 0;

  for (tid_idx = 0; tid_idx < SIZEOF(peer->tid); tid_idx++)
  {
    int notify = 0;
    mtlk_osal_lock_acquire(&obj->lock);
    if (peer->tid[tid_idx].tx.state == MTLK_ADDBA_TX_ADDBA_REQ_CFMD)
    {
      mtlk_osal_msec_t now_time  = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
      mtlk_osal_ms_diff_t diff_time = mtlk_osal_ms_time_diff(now_time, peer->tid[tid_idx].tx.addba_req_cfmd_time);
      if (diff_time >= MTLK_ADDBA_RE_TX_REQ_TIMEOUT_MS)
      {
        BOOL stop = _mtlk_addba_reset_tx_state(obj, peer, tid_idx);

        MTLK_ASSERT(stop == FALSE); /* Aggregations cannot be started at this phase */
        MTLK_UNREFERENCED_PARAM(stop); /* For release compilation */

        --obj->nof_aggr_on;

        ILOG2(GID_ADDBA, "TX %Y TID=%d request timeout expired (%u >= %u)",
            addr->au8Addr, tid_idx, diff_time,
            MTLK_ADDBA_RE_TX_REQ_TIMEOUT_MS);

        notify = 1;
      }
    }
    mtlk_osal_lock_release(&obj->lock);

    if (notify)
    {
      obj->api.on_ba_req_unconfirmed(obj->api.usr_data, container_handle, tid_idx);
    }
  }
}

static __INLINE void
_mtlk_addba_check_delba_timeouts (mtlk_addba_t      *obj,
                                  const IEEE_ADDR   *addr,
                                  mtlk_addba_peer_t *peer,
                                  mtlk_handle_t      container_handle)
{
  uint16 tid_idx = 0;

  for (tid_idx = 0; tid_idx < SIZEOF(peer->tid); tid_idx++)
  {
    int stop_reordering = 0;

    mtlk_osal_lock_acquire(&obj->lock);
    /* check DELBA send timeout */
    if (peer->tid[tid_idx].rx.state == MTLK_ADDBA_RX_REORD_IN_PROCESS)
    {
      if (peer->tid[tid_idx].rx.delba_timeout)
      {
        uint32 last_rx, now, diff;

        last_rx = obj->api.get_last_rx_timestamp(obj->api.usr_data, container_handle, tid_idx);

        if (last_rx < peer->tid[tid_idx].rx.req_tstamp)
          last_rx = peer->tid[tid_idx].rx.req_tstamp;

        now  = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
        diff = mtlk_osal_ms_time_diff(now, last_rx);

        if (diff >= peer->tid[tid_idx].rx.delba_timeout)
        {
          ILOG2(GID_ADDBA, "RX %Y TID=%d DELBA timeout expired (%u >= %u)",
              addr->au8Addr, tid_idx,
              diff, peer->tid[tid_idx].rx.delba_timeout);
          _mtlk_addba_send_delba_req(obj, addr, peer, tid_idx, 0);
          stop_reordering = 1;
        }
      }
    }
    mtlk_osal_lock_release(&obj->lock);

    if (stop_reordering)
    {
      obj->api.on_stop_reordering(obj->api.usr_data, container_handle, tid_idx);
    }
  }
}

MTLK_INIT_STEPS_LIST_BEGIN(addba)
  MTLK_INIT_STEPS_LIST_ENTRY(addba, INIT_LOCK)
MTLK_INIT_INNER_STEPS_BEGIN(addba)
MTLK_INIT_STEPS_LIST_END(addba);

int __MTLK_IFUNC
mtlk_addba_init (mtlk_addba_t                *obj,
                 const mtlk_addba_cfg_t      *cfg,
                 const mtlk_addba_wrap_api_t *api)
{
  MTLK_ASSERT(cfg != NULL);
  MTLK_ASSERT(api != NULL);
  MTLK_ASSERT(api->txmm != NULL);
  MTLK_ASSERT(api->get_peer != NULL);
  MTLK_ASSERT(api->get_last_rx_timestamp != NULL);
  MTLK_ASSERT(api->on_start_aggregation != NULL);
  MTLK_ASSERT(api->on_stop_aggregation != NULL);
  MTLK_ASSERT(api->on_start_reordering != NULL);
  MTLK_ASSERT(api->on_stop_reordering != NULL);
  MTLK_ASSERT(api->on_ba_req_received != NULL);
  MTLK_ASSERT(api->on_ba_req_rejected != NULL);
  MTLK_ASSERT(api->on_ba_req_unconfirmed != NULL);

  MTLK_INIT_TRY(addba, MTLK_OBJ_PTR(obj))
    MTLK_INIT_STEP(addba, INIT_LOCK, MTLK_OBJ_PTR(obj), 
                   mtlk_osal_lock_init,  (&obj->lock));
    obj->cfg = *cfg;
    obj->api = *api;
  MTLK_INIT_FINALLY(addba, MTLK_OBJ_PTR(obj))
  MTLK_INIT_RETURN(addba, MTLK_OBJ_PTR(obj), mtlk_addba_cleanup, (obj))
}

void __MTLK_IFUNC
mtlk_addba_cleanup (mtlk_addba_t *obj)
{
  MTLK_CLEANUP_BEGIN(addba, MTLK_OBJ_PTR(obj))
    MTLK_CLEANUP_STEP(addba, INIT_LOCK, MTLK_OBJ_PTR(obj),
                      mtlk_osal_lock_cleanup, (&obj->lock));
  MTLK_CLEANUP_END(addba, MTLK_OBJ_PTR(obj));
}

int  __MTLK_IFUNC 
mtlk_addba_reconfigure (mtlk_addba_t           *obj, 
                        const mtlk_addba_cfg_t *cfg)
{
  mtlk_osal_lock_acquire(&obj->lock);
  obj->cfg = *cfg;
  mtlk_osal_lock_release(&obj->lock);
  
  return MTLK_ERR_OK;
}

MTLK_INIT_STEPS_LIST_BEGIN(addba_peer)
  MTLK_INIT_STEPS_LIST_ENTRY(addba_peer, TXMM_MSG_INIT_TX)
  MTLK_INIT_STEPS_LIST_ENTRY(addba_peer, TXMM_MSG_INIT_RX)
MTLK_INIT_INNER_STEPS_BEGIN(addba_peer)
MTLK_INIT_STEPS_LIST_END(addba_peer);

int  __MTLK_IFUNC
mtlk_addba_peer_init (mtlk_addba_peer_t *peer)
{
  uint8 i;
  MTLK_INIT_TRY(addba_peer, MTLK_OBJ_PTR(peer))
    for (i = 0; i < ARRAY_SIZE(peer->tid); i++) {
      MTLK_INIT_STEP_LOOP(addba_peer, TXMM_MSG_INIT_TX, MTLK_OBJ_PTR(peer),
                          mtlk_txmm_msg_init, (&peer->tid[i].tx.man_msg));
    }
    for (i = 0; i < ARRAY_SIZE(peer->tid); i++) {
      MTLK_INIT_STEP_LOOP(addba_peer, TXMM_MSG_INIT_RX, MTLK_OBJ_PTR(peer),
                           mtlk_txmm_msg_init, (&peer->tid[i].rx.man_msg));
    }
    mtlk_osal_atomic_set(&peer->is_active, 0);
  MTLK_INIT_FINALLY(addba_peer, MTLK_OBJ_PTR(peer))
  MTLK_INIT_RETURN(addba_peer, MTLK_OBJ_PTR(peer), mtlk_addba_peer_cleanup, (peer))
}

void __MTLK_IFUNC
mtlk_addba_peer_cleanup (mtlk_addba_peer_t *peer)
{
  uint8 i;

  MTLK_CLEANUP_BEGIN(addba_peer, MTLK_OBJ_PTR(peer))
	for (i = 0; MTLK_CLEANUP_ITERATONS_LEFT(MTLK_OBJ_PTR(peer), TXMM_MSG_INIT_RX) > 0; i++) {
      MTLK_CLEANUP_STEP_LOOP(addba_peer, TXMM_MSG_INIT_RX, MTLK_OBJ_PTR(peer),
                             mtlk_txmm_msg_cleanup, (&peer->tid[i].rx.man_msg));
	}
                               
	for (i = 0; MTLK_CLEANUP_ITERATONS_LEFT(MTLK_OBJ_PTR(peer), TXMM_MSG_INIT_TX) > 0; i++) {
      MTLK_CLEANUP_STEP_LOOP(addba_peer, TXMM_MSG_INIT_TX, MTLK_OBJ_PTR(peer),
                             mtlk_txmm_msg_cleanup, (&peer->tid[i].tx.man_msg));
	}
  MTLK_CLEANUP_END(addba_peer, MTLK_OBJ_PTR(peer))
}

void __MTLK_IFUNC 
mtlk_addba_on_delba_req_rx (mtlk_addba_t    *obj, 
                            const IEEE_ADDR *addr,
                            uint16           tid_idx,
                            uint16           res_code,
                            uint16           initiator)
{
  mtlk_addba_peer_t* peer      = NULL;
  mtlk_handle_t      reord_val = HANDLE_T(0);

  ILOG2(GID_ADDBA, "TX %Y TID=%d DELBA recvd, RES=%d IN=%d", addr->au8Addr,
       (int)tid_idx, (int)res_code, (int)initiator);

  peer = _mtlk_addba_peer_ex(obj, addr, &reord_val);
  if (peer && mtlk_osal_atomic_get(&peer->is_active))
  {
    BOOL stop = FALSE;
    mtlk_osal_lock_acquire(&obj->lock);

    if (initiator) {
      stop = _mtlk_addba_reset_rx_state(obj, peer, tid_idx);
    }
    else {
      /* 
         Sent by receipient of the data => our TX-related =>
         we should stop the aggregations transmission.
      */
      _mtlk_addba_close_aggr_req(obj, addr, peer, tid_idx);
    }

    mtlk_osal_lock_release(&obj->lock);

    if (stop)
    {
      obj->api.on_stop_reordering(obj->api.usr_data, reord_val, tid_idx);
    }
  }
}

void __MTLK_IFUNC 
mtlk_addba_on_addba_res_rx (mtlk_addba_t    *obj, 
                            const IEEE_ADDR *addr,
                            uint16           res_code,
                            uint16           tid_idx,
                            uint8            win_size_rsp,
                            uint8            dlgt)
{
  mtlk_addba_peer_t *peer             = NULL;
  int                notify_reject    = 0;
  mtlk_handle_t      container_handle = HANDLE_T(0);

  ILOG2(GID_ADDBA, "TX %Y TID=%d TAG=%d response recvd RES=%d",
       addr, (int)tid_idx, (int)dlgt, (int)res_code);

  peer = _mtlk_addba_peer_ex(obj, addr, &container_handle);
  if (peer && mtlk_osal_atomic_get(&peer->is_active))
  {
    mtlk_osal_lock_acquire(&obj->lock);

    if (peer->tid[tid_idx].tx.state != MTLK_ADDBA_TX_ADDBA_REQ_SENT &&
        peer->tid[tid_idx].tx.state != MTLK_ADDBA_TX_ADDBA_REQ_CFMD) {
      WLOG("TX: %Y TID=%d TAG=%d invalid state: %d - ignoring response",
              addr->au8Addr, (int)tid_idx, (int)dlgt,
              (int)peer->tid[tid_idx].tx.state);
      goto out;
    }

    if (res_code == MTLK_ADDBA_RES_CODE_SUCCESS)
    {
      mtlk_txmm_data_t  *tx_data;
      UMI_OPEN_AGGR_REQ *add_aggr_req;
      int                sres;
      uint8              win_size_req = peer->tid[tid_idx].tx.win_size_req;
      uint8              win_size_fw = 0;

      ILOG2(GID_ADDBA, "TX %Y TID=%d TAG=%d WSIZE=%d accepted by peer",
          addr->au8Addr, (int)tid_idx, (int)dlgt, win_size_rsp);

      peer->tid[tid_idx].tx.addba_res_rejects = 0;

      tx_data = mtlk_txmm_msg_get_empty_data(&peer->tid[tid_idx].tx.man_msg,
                                             obj->api.txmm);
      if (!tx_data)
      {
        ELOG("no msg available");
        goto out;
      }

       /* Select window size */

      if(win_size_req == 0)
      {
        /* We don't know what we want. Just accept what our peer wants. */
        win_size_fw = win_size_rsp;
      }
      else
      {
        /* Choose smallest of what is supported by us and by peer */
        win_size_fw = MIN(win_size_req, win_size_rsp);
      }

      if(win_size_fw > MTLK_ADDBA_MAX_REORD_WIN_SIZE)
      {
        win_size_fw = MTLK_ADDBA_MAX_REORD_WIN_SIZE;
      }
      else if(win_size_fw < MTLK_ADDBA_MIN_REORD_WIN_SIZE)
      {
        WLOG("TX: %Y TID=%d TAG=%d too small wsize: %d - ignoring response",
                addr->au8Addr, (int)tid_idx, (int)dlgt,
                win_size_fw);
        goto out;
      }

      add_aggr_req = (UMI_OPEN_AGGR_REQ*)tx_data->payload;

      tx_data->id           = UM_MAN_OPEN_AGGR_REQ;
      tx_data->payload_size = sizeof(*add_aggr_req);
    
      add_aggr_req->sDA                      = *addr;
      add_aggr_req->u16AccessProtocol        = HOST_TO_MAC16(tid_idx);
      add_aggr_req->u16MaxNumOfPackets       = HOST_TO_MAC16(obj->cfg.tid[tid_idx].max_nof_packets);
      add_aggr_req->u32MaxNumOfBytes         = HOST_TO_MAC32(obj->cfg.tid[tid_idx].max_nof_bytes);
      add_aggr_req->u32TimeoutInterval       = HOST_TO_MAC32(obj->cfg.tid[tid_idx].timeout_interval);
      add_aggr_req->u32MinSizeOfPacketInAggr = HOST_TO_MAC32(obj->cfg.tid[tid_idx].min_packet_size_in_aggr);
      add_aggr_req->windowSize               = HOST_TO_MAC32((uint32) win_size_fw);

      ILOG2(GID_ADDBA, "TX %Y TID=%d opening aggregation",
           add_aggr_req->sDA.au8Addr, (int)tid_idx);

      sres = mtlk_txmm_msg_send(&peer->tid[tid_idx].tx.man_msg, _mtlk_addba_on_open_aggr_cfm_clb,
                                HANDLE_T(obj), 0);
      if (sres == MTLK_ERR_OK)
      {
        peer->tid[tid_idx].tx.state = MTLK_ADDBA_TX_ADD_AGGR_REQ_SENT;
      }
      else
      {
        ELOG("Can't open AGGR due to TXMM err#%d", sres);
      }
    }
    else
    {
      BOOL stop = _mtlk_addba_reset_tx_state (obj, peer, tid_idx);

      MTLK_ASSERT(stop == FALSE); /* Aggregations cannot be started at this phase */
      MTLK_UNREFERENCED_PARAM(stop); /* For Release compilation */

      --obj->nof_aggr_on;

      ILOG2(GID_ADDBA, "TX %Y TID=%d TAG=%d rejected by peer",
          addr->au8Addr, (int)tid_idx, (int)dlgt);

      ++peer->tid[tid_idx].tx.addba_res_rejects;
      if (peer->tid[tid_idx].tx.addba_res_rejects  < MTLK_ADDBA_MAX_REJECTS)
      {
        notify_reject = 1;
      }
      else
      {
        ILOG1("ADDBA rejects limit reached for peer %Y, TID=%d. "
          "No more notifications will be sent to upper layer",
          addr->au8Addr, tid_idx);
        peer->tid[tid_idx].tx.addba_res_rejects = 0;
      }
    }
out:
    mtlk_osal_lock_release(&obj->lock);

    if (notify_reject)
    {
      obj->api.on_ba_req_rejected(obj->api.usr_data, container_handle, tid_idx);
    }
  }
}

void __MTLK_IFUNC 
mtlk_addba_on_addba_req_rx (mtlk_addba_t    *obj, 
                            const IEEE_ADDR *addr,
                            uint16           ssn,
                            uint16           tid_idx,
                            uint8            win_size,
                            uint8            dlgt,
                            uint16           tmout,
                            uint16           ack_policy,
                            uint16           rate)
{
  mtlk_addba_peer_t* peer      = NULL;
  mtlk_handle_t      reord_val = HANDLE_T(0);

  ILOG2(GID_ADDBA, "RX %Y TID=%d TAG=%d req recvd WSIZE=%d SSN=%d TMBA=%d ACP=%d",
       addr->au8Addr,
       (int)tid_idx,
       (int)dlgt,
       (int)win_size,
       (int)ssn,
       (int)tmout,
       (int)ack_policy);

  peer = _mtlk_addba_peer_ex(obj, addr, &reord_val);
  if (peer && mtlk_osal_atomic_get(&peer->is_active))
  {
    int start_reord = 0;

    mtlk_osal_lock_acquire(&obj->lock);
    if (tid_idx >= SIZEOF(peer->tid))
    {
      ELOG("RX %Y TID=%d TAG=%d: wrong priority (%d >= %ld)",
          addr->au8Addr, (int)tid_idx, (int)dlgt,
          tid_idx, SIZEOF(peer->tid));
    }
    else
    {
      mtlk_txmm_data_t* tx_data = NULL;

      if (peer->tid[tid_idx].rx.state != MTLK_ADDBA_RX_NONE)
      {
        ILOG2(GID_ADDBA, "RX %Y TID=%d TAG=%d: invalid state %d",
            addr->au8Addr, (int)tid_idx, (int)dlgt,
            peer->tid[tid_idx].rx.state);
      }

      tx_data = mtlk_txmm_msg_get_empty_data(&peer->tid[tid_idx].rx.man_msg,
                                             obj->api.txmm);
      if (tx_data)
      {
        UMI_ADDBA_RES_SEND* addba_res = (UMI_ADDBA_RES_SEND*)tx_data->payload;
        int                 sres;

        tx_data->id           = UM_MAN_ADDBA_RES_TX_REQ;
        tx_data->payload_size = sizeof(*addba_res);

        addba_res->sDA               = *addr;
        addba_res->u8DialogToken     = dlgt;
        addba_res->u16AccessProtocol = HOST_TO_MAC16(tid_idx);

        if (!_mtlk_addba_is_allowed_rate(rate))
        {
          ILOG2(GID_ADDBA, "RX %Y TID=%d TAG=%d (DECLINED: RATE == %d)",
               addr->au8Addr, (int)tid_idx, (int)dlgt, (int)rate);
          addba_res->u16ResultCode = HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_FAILURE);
        }
        else if (obj->nof_reord_on >= obj->cfg.max_reord_supported)
        {
          ILOG2(GID_ADDBA, "RX %Y TID=%d TAG=%d (DECLINED: LIM %d >= %d)",
              addr->au8Addr, (int)tid_idx, (int)dlgt,
              obj->nof_aggr_on, obj->cfg.max_aggr_supported);
          addba_res->u16ResultCode = HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_FAILURE);
        }
        else if (!obj->cfg.tid[tid_idx].accept_aggr)
        {
          ILOG2(GID_ADDBA, "RX %Y TID=%d TAG=%d (DECLINED: CFG OFF)",
              addr->au8Addr, (int)tid_idx, (int)dlgt);
          addba_res->u16ResultCode = HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_FAILURE);
        }
        else if (!ack_policy)
        {
          ILOG2(GID_ADDBA, "RX %Y TID=%d TAG=%d (DECLINED: ACP == 0)",
              addr->au8Addr, (int)tid_idx, (int)dlgt);
          addba_res->u16ResultCode = HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_FAILURE);
        }
        else
        {
          _mtlk_addba_correct_res_win_size(obj, &win_size);
          _mtlk_addba_correct_res_timeout(obj, &tmout);

          addba_res->u16ResultCode = HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_SUCCESS);
          addba_res->u16BATimeout  = HOST_TO_MAC16(tmout);
          addba_res->u8WinSize     = win_size;

          ILOG2(GID_ADDBA, "RX %Y TID=%d TAG=%d (ACCEPTED: TMBA=%d WSIZE=%d)",
              addr->au8Addr, (int)tid_idx, (int)dlgt,
              (int)tmout, (int)win_size);

          start_reord = 1;
        }

        sres = mtlk_txmm_msg_send(&peer->tid[tid_idx].rx.man_msg, _mtlk_addba_on_addba_res_tx_cfm_clb,
                                  HANDLE_T(obj), 0);
        if (sres == MTLK_ERR_OK)
        {
          peer->tid[tid_idx].rx.state         = MTLK_ADDBA_RX_ADDBA_RES_SENT;
          peer->tid[tid_idx].rx.delba_timeout = (uint16)ADDBA_TU_TO_MS(tmout);
          peer->tid[tid_idx].rx.req_tstamp    = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());

          ++obj->nof_reord_on;
        }
        else
        {
          ELOG("Can't send ADDBA response due to TXMM err#%d", sres);
          start_reord = 0; /* Other side didn't get our ADDBA RES => don't open reordering */
        }
      }
      else
      {
        ELOG("Can't send ADDBA resp due to lack of MAN_MSG");
      }
    }
    mtlk_osal_lock_release(&obj->lock);

    if (start_reord)
    {
      obj->api.on_start_reordering(obj->api.usr_data,
                                   reord_val,
                                   tid_idx,
                                   ssn,
                                   win_size);
    }

    obj->api.on_ba_req_received(obj->api.usr_data, reord_val, tid_idx);
  }
}

void __MTLK_IFUNC
mtlk_addba_iterate (mtlk_addba_t    *obj,
                    const IEEE_ADDR *addr)
{
  mtlk_addba_iterate_ex(obj, addr, NULL, HANDLE_T(0));
}

void __MTLK_IFUNC
mtlk_addba_iterate_ex (mtlk_addba_t      *obj,
                       const IEEE_ADDR   *addr,
                       mtlk_addba_peer_t *peer,
                       mtlk_handle_t      container_handle)
{
  if (!peer)
  {
    peer = _mtlk_addba_peer_ex(obj, addr, &container_handle);
  }

  if (peer && mtlk_osal_atomic_get(&peer->is_active))
  {
    _mtlk_addba_check_delba_timeouts(obj, addr, peer, container_handle);
    _mtlk_addba_check_addba_res_rx_timeouts(obj, addr, peer, container_handle);
  }
}

void __MTLK_IFUNC
mtlk_addba_on_connect (mtlk_addba_t    *obj,
                       const IEEE_ADDR *addr)
{
  mtlk_addba_on_connect_ex(obj, addr, NULL, HANDLE_T(0));
}

void __MTLK_IFUNC
mtlk_addba_on_connect_ex (mtlk_addba_t      *obj,
                          const IEEE_ADDR   *addr,
                          mtlk_addba_peer_t *peer,
                          mtlk_handle_t      container_handle)
{
  MTLK_UNREFERENCED_PARAM(container_handle);

  if (!peer)
  {
    peer = _mtlk_addba_peer(obj, addr);
  }

  /* Clean up the peer and mark it active */
  if (peer)
  {
      uint32 was_active = mtlk_osal_atomic_xchg(&peer->is_active, 1);
      if (was_active == 0)
      {
        _mtlk_addba_cleanup_peer(obj, peer, addr, container_handle);
      }
      else
      {
        WLOG("Peer %Y is already active!", addr);
      }
  }
}

void __MTLK_IFUNC
mtlk_addba_start_negotiation (mtlk_addba_t    *obj,
                              const IEEE_ADDR *addr,
                              uint16           rate)
{
  mtlk_addba_start_negotiation_ex(obj, addr, NULL, HANDLE_T(0), rate);
}

void __MTLK_IFUNC
mtlk_addba_start_negotiation_ex (mtlk_addba_t      *obj,
                                 const IEEE_ADDR   *addr,
                                 mtlk_addba_peer_t *peer,
                                 mtlk_handle_t      container_handle,
                                 uint16             rate)
{
  MTLK_UNREFERENCED_PARAM(container_handle);

  if (!_mtlk_addba_is_allowed_rate(rate)) 
  {
     /* Aggregations is not allowed for this rate */
    ILOG2(GID_ADDBA, "Aggregations are not allowed (rate=%d)", (int)rate);
    peer = NULL;
  }
  else if (!peer)
  {
    peer = _mtlk_addba_peer(obj, addr);
  }

  if (peer && mtlk_osal_atomic_get(&peer->is_active))
  {
    mtlk_osal_lock_acquire(&obj->lock);
    _mtlk_addba_start_negotiation(obj, peer, addr);
    mtlk_osal_lock_release(&obj->lock);
  }
}

void __MTLK_IFUNC
mtlk_addba_on_disconnect (mtlk_addba_t    *obj,
                          const IEEE_ADDR *addr)
{
  mtlk_addba_on_disconnect_ex(obj, addr, NULL, HANDLE_T(0));
}

void __MTLK_IFUNC
mtlk_addba_on_disconnect_ex (mtlk_addba_t      *obj,
                             const IEEE_ADDR   *addr,
                             mtlk_addba_peer_t *peer,
                             mtlk_handle_t      container_handle)
{
  MTLK_UNREFERENCED_PARAM(container_handle);

  if (!peer)
  {
    peer = _mtlk_addba_peer_ex(obj, addr, &container_handle);
  }
  if (peer)
  {
    uint32 was_active = mtlk_osal_atomic_xchg(&peer->is_active, 0);
    if (was_active)
    {
      _mtlk_addba_cleanup_peer(obj, peer, addr, container_handle);
    }
    else
    {
      WLOG("Peer %Y is already inactive!", addr);
    }
  }
}

void __MTLK_IFUNC
mtlk_addba_start_aggr_negotiation (mtlk_addba_t    *obj,
                                   const IEEE_ADDR *addr,
                                   uint16           tid)
{
  mtlk_addba_start_aggr_negotiation_ex(obj, addr, NULL, HANDLE_T(0), tid);
}

void __MTLK_IFUNC
mtlk_addba_start_aggr_negotiation_ex (mtlk_addba_t      *obj,
                                      const IEEE_ADDR   *addr,
                                      mtlk_addba_peer_t *peer, 
                                      mtlk_handle_t      container_handle,
                                      uint16             tid)
{
  MTLK_UNREFERENCED_PARAM(container_handle);

  if (!peer)
  {
    peer = _mtlk_addba_peer(obj, addr);
  }

  if (peer && mtlk_osal_atomic_get(&peer->is_active))
  {
    mtlk_osal_lock_acquire(&obj->lock);
    if (obj->cfg.tid[tid].use_aggr)
    {
      _mtlk_addba_tx_addba_req(obj, addr, peer, tid);
    }
    mtlk_osal_lock_release(&obj->lock);
  }
}
