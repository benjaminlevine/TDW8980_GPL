/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id$
 *
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *
 * ADDBA OS-dependent module implementation
 *
 */
#include "mtlkinc.h"

#include "core.h"

static void addba_iterator(unsigned long data)
{
  struct nic* nic = (struct nic*)data;
  int              i         = 0;
  sta_entry*       sta;
  
  for (i = 0; i < STA_MAX_STATIONS; i++)
  {
    sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, i);
    if (sta->state == PEER_CONNECTED)
    {
      mtlk_addba_iterate_ex(&nic->slow_ctx->addba,
                            (IEEE_ADDR*)sta->mac,
                            &sta->addba_peer,
                            HANDLE_T(i));
    }
  }

  /* re-arm timer */
  nic->slow_ctx->addba_timer.expires = jiffies + msecs_to_jiffies(ADDBA_TIMER_INTERVAL);
  add_timer(&nic->slow_ctx->addba_timer);
}

void addba_cleanup(struct nic *nic)
{
  /* stop timer */
  if (nic->slow_ctx->addba_timer.function)
  {
    del_timer(&nic->slow_ctx->addba_timer);
    nic->slow_ctx->addba_timer.function = NULL;
  }

  mtlk_addba_cleanup(&nic->slow_ctx->addba);
}

int addba_init(struct nic *nic)
{
  int                   res = MTLK_ERR_UNKNOWN;
  mtlk_addba_wrap_api_t api;

  memset(&api, 0, sizeof(api));

  api.txmm                  = nic->slow_ctx->hw_cfg.txmm;

  api.usr_data              = HANDLE_T(&nic->slow_ctx->stadb);
  api.get_peer              = mtlk_stadb_get_addba_peer;
  api.get_last_rx_timestamp = mtlk_stadb_get_addba_last_rx_timestamp;
  api.on_start_aggregation  = mtlk_stadb_addba_do_nothing;
  api.on_stop_aggregation   = mtlk_stadb_addba_restart_tx_count_tid;
  api.on_start_reordering   = mtlk_stadb_addba_start_reordering;
  api.on_stop_reordering    = mtlk_stadb_addba_stop_reordering;
  api.on_ba_req_received    = mtlk_stadb_addba_revive_aggregation_all;
  api.on_ba_req_rejected    = mtlk_stadb_addba_revive_aggregation_tid;
  api.on_ba_req_unconfirmed = mtlk_stadb_addba_restart_tx_count_tid;

  res = mtlk_addba_init(&nic->slow_ctx->addba, &nic->slow_ctx->cfg.addba, &api);
  if (res != MTLK_ERR_OK)
  {
    ELOG("ADDBA init (Err=%d)", res);
    goto FINISH;
  }

  init_timer(&nic->slow_ctx->addba_timer);
  nic->slow_ctx->addba_timer.function = addba_iterator;
  nic->slow_ctx->addba_timer.data     = (unsigned long)nic;
  nic->slow_ctx->addba_timer.expires  = jiffies + msecs_to_jiffies(ADDBA_TIMER_INTERVAL);
  add_timer(&nic->slow_ctx->addba_timer);

  res = MTLK_ERR_OK;

FINISH:
  return res;
}

mtlk_addba_t *
mtlk_get_addba_related_info (mtlk_handle_t context, uint16 *rate)
{
  struct nic *nic = (struct nic *)context;

  if (rate)
    *rate = mtlk_core_get_rate_for_addba(nic);

  return &nic->slow_ctx->addba;
}

