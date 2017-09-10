/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __MTLK_11H_H__
#define __MTLK_11H_H__


#include "txmm.h"
#include "mtlk_osal.h"
#include "mtlkflctrl.h"
#include "aocs.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

#define DOT11H_TIMER_INTERVAL        (10000)

/**********************************************************************
 * definitions and database
***********************************************************************/

#define MT_FLCTRL_ENABLED   1

//#define MT_ENABLE_ETSI_SPECIAL 0

typedef enum _mtlk_dot11h_status_e{
  MTLK_DOT11H_IDLE,
  MTLK_DOT11H_IN_PROCESS,
  MTLK_DOT11H_IN_AVAIL_CHECK_TIME,
  MTLK_DOT11H_ERROR,
  MTLK_DOT11H_FINISHED_OK
} __MTLK_IDATA mtlk_dot11h_status_e;

typedef enum _mtlk_dfs_event_e {
  MTLK_DFS_EVENT_RADAR_DETECTED,
  MTLK_DFS_EVENT_CHANGE_CHANNEL_NORMAL,
  MTLK_DFS_EVENT_CHANGE_CHANNEL_SILENT,
  MTLK_DFS_CHANGE_CHANNEL_DONE,
  MTLK_DFS_CHANGE_CHANNEL_FAILED_TIMEOUT,
  MTLK_DFS_CHANGE_CHANNEL_FAILED_STOP,
  MTLK_DFS_EVENT_LAST
} mtlk_dfs_event_e;

typedef struct _mtlk_dot11h_debug_params_t
{
  int16    debugNewChannel; /*for driver use*/
  int16    debugChannelAvailabilityCheckTime;
  int8     debugChannelSwitchCount;
  int8     debugSmRequired;
  uint8    pad1;
} __MTLK_IDATA mtlk_dot11h_debug_params_t;

typedef struct _mtlk_dot11h_cfg_t
{
  uint16   u16Channel;
  uint16   u16ChannelAvailabilityCheckTime;
  uint8    u8ScanType;
  uint8    u8ChannelSwitchCount;
  uint8    u8SwitchMode;
  uint8    u8Bonding;
  uint8    u8IsHT;
  uint8    u8FrequencyBand;
  uint8    u8SpectrumMode;
  int8     i8CbTransmitPowerLimit;
  int8     i8nCbTransmitPowerLimit;
  int8     i8AntennaGain;
  uint8    u8SmRequired;
  int8     _11h_radar_detect; 
} __MTLK_IDATA mtlk_dot11h_cfg_t;

typedef struct _mtlk_dot11h_wrap_api_t
{
  mtlk_txmm_t*      txmm;
  mtlk_aocs_t*      aocs;
#if MT_FLCTRL_ENABLED
  mtlk_flctrl_t*    flctrl;
#endif
  mtlk_handle_t     usr_data;
  mtlk_handle_t     flctrl_id;
#ifdef MT_FLCTRL_ENABLED
#if !MT_FLCTRL_ENABLED
  void              (__MTLK_IDATA *start_data)(mtlk_handle_t usr_data);
  void              (__MTLK_IDATA *start_wake)(mtlk_handle_t usr_data);
  void              (__MTLK_IDATA *stop_data)(mtlk_handle_t usr_data);
#endif
#endif
  void              (__MTLK_IDATA *get_dot11h_debug_params)(mtlk_handle_t usr_data, mtlk_dot11h_debug_params_t *dot11h_debug_params);
  int               (__MTLK_IDATA *device_is_ap)(mtlk_handle_t usr_data);
  uint16            (__MTLK_IDATA *get_sq_size)(mtlk_handle_t usr_data, uint16 access_category);
} __MTLK_IDATA mtlk_dot11h_wrap_api_t;


typedef int (__MTLK_IDATA *mtlk_dot11h_callback_f)(mtlk_handle_t clb_usr_data, mtlk_handle_t context, int change_res);

typedef struct _mtlk_dot11h_t
{
  uint32                    init_mask;
  mtlk_dot11h_cfg_t         cfg;
  mtlk_dot11h_wrap_api_t    api;
  mtlk_handle_t             clb_context;
  mtlk_core_t               *core;
  mtlk_dot11h_callback_f    clb;
  uint16                    set_channel;
  uint8                     status;
  uint8                     data_stop; /*STA only, for NULL Packet stop*/
  mtlk_dfs_event_e          event; /*aocs.h to be included before dfs.h*/
  mtlk_osal_spinlock_t      lock;
  mtlk_osal_timer_t         timer;
  mtlk_txmm_msg_t           man_msg;
  MTLK_DECLARE_INIT_STATUS;
} __MTLK_IDATA mtlk_dot11h_t;



/**********************************************************************
 * function declaration
***********************************************************************/


int __MTLK_IFUNC mtlk_dot11h_init(mtlk_dot11h_t                *obj,
                                  const mtlk_dot11h_cfg_t      *cfg,
                                  const mtlk_dot11h_wrap_api_t *api,
                                  mtlk_core_t *core);
void __MTLK_IFUNC mtlk_fill_default_dot11h_params(mtlk_dot11h_cfg_t *params);
void __MTLK_IFUNC mtlk_fill_default_dot11h_debug_params(mtlk_dot11h_debug_params_t *params);
void __MTLK_IFUNC mtlk_dot11h_get_cfg(mtlk_dot11h_t            *obj,
                                      mtlk_dot11h_cfg_t        *cfg);
void __MTLK_IFUNC mtlk_dot11h_set_cfg(mtlk_dot11h_t            *obj,
                                      const mtlk_dot11h_cfg_t  *cfg);
void __MTLK_IFUNC mtlk_dot11h_initiate_channel_switch(mtlk_dot11h_t *obj,
  mtlk_aocs_evt_select_t *switch_data, BOOL is_aocs_switch, mtlk_dot11h_callback_f clb);
void __MTLK_IFUNC mtlk_dot11h_on_channel_switch_done_ind(mtlk_dot11h_t *obj);
void __MTLK_IFUNC mtlk_dot11h_on_channel_switch_announcement_ind(mtlk_dot11h_t *obj);
void __MTLK_IFUNC mtlk_dot11h_cleanup(mtlk_dot11h_t *obj);

BOOL __MTLK_IFUNC
#if 1
mtlk_dot11h_cfm_clb(mtlk_dot11h_t *obj);
#else
mtlk_dot11h_cfm_clb(mtlk_handle_t clb_usr_data,
                    mtlk_txmm_data_t* data,
                    mtlk_txmm_clb_reason_e reason);
#endif

BOOL __MTLK_IFUNC mtlk_dot11h_can_switch_now(mtlk_dot11h_t *obj);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_11H_H__ */


