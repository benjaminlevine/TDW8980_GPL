/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id$
 *
 * Copyright (c) 2007 Metalink Broadband Ltd.
 *
 * HAL interface
 *
 */

#ifndef __MTLKHAL_H__
#define __MTLKHAL_H__

#include "mtlkerr.h"
#include "mtlkmsg.h"
#include "txmm.h"
#include "mtlkdfdefs.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/**********************************************************************
 * Common definitions. Used by HW and Core (logic module)
***********************************************************************/
typedef struct _mtlk_hw_msg_t                 mtlk_hw_msg_t;
typedef struct _mtlk_persistent_device_data_t mtlk_persistent_device_data_t;

typedef struct _mtlk_hal_file_t
{
  const char* buffer;
  uint32      size;
} __MTLK_IDATA mtlk_hal_file_t;
/**********************************************************************/

/**********************************************************************
 * HW => Core (logic module):
 *  - must be implemented by core (logic module)
 *  - called HW layer
***********************************************************************/
typedef struct _mtlk_core_hw_cfg_t
{
  int          ap;
  mtlk_txmm_t* txmm;
  mtlk_txmm_t* txdm;
} __MTLK_IDATA mtlk_core_hw_cfg_t;

typedef struct _mtlk_core_release_tx_data_t
{
  mtlk_hw_msg_t       *msg;             /* HW Message passed to mtlk_hw_send_data() */
  mtlk_nbuf_t         *nbuf;            /* Network buffer                           */
  uint32               size;            /* Data size                                */
  uint8                access_category; /* Packet's access category                 */
  uint32               resources_free;  /* HW TX resources are available flag       */
  UMI_STATUS           status;          /* Is Tx failed?                            */
} __MTLK_IDATA mtlk_core_release_tx_data_t;

/* MTLK_CORE_PROP_TXM_STUCK_DETECTED additional info */
typedef enum
{
  MTLK_TXM_MAN,
  MTLK_TXM_DBG,
  MTLK_TXM_BCL,
  MTLK_TXM_LAST
} mtlk_txm_obj_id_e;

typedef enum _mtlk_core_prop_e
{/* prop_id */
  MTLK_CORE_PROP_FIRMWARE_BIN_BUFFER, /* SET: mtlk_core_fw_buffer_t*, GET: unsupported */
  MTLK_CORE_PROP_MAC_SW_RESET_ENABLED,/* SET: unsupported,            GET: uint32*     */
  MTLK_CORE_PROP_MAC_STUCK_DETECTED,  /* SET: ignored,                GET: unsupported */
  MTLK_CORE_PROP_LAST
} mtlk_core_prop_e;

#define MAX_FIRMWARE_FILENAME_LEN 64

typedef struct _mtlk_core_firmware_file_t
{
  char                   fname[MAX_FIRMWARE_FILENAME_LEN];
  mtlk_hal_file_t        content;
  mtlk_handle_t          context; /* for internal usage */
} __MTLK_IDATA mtlk_core_firmware_file_t;

typedef struct _mtlk_core_handle_rx_data_t
{
  mtlk_nbuf_t               *nbuf;     /* Network buffer        */
  uint32                     size;     /* Data size             */
  uint8                      offset;   /* Offset                */
  MAC_RX_ADDITIONAL_INFO_T  *info;     /* MAC additional info   */
} __MTLK_IDATA mtlk_core_handle_rx_data_t;

mtlk_core_t* __MTLK_IFUNC mtlk_core_create(mtlk_persistent_device_data_t *persistent);
int          __MTLK_IFUNC mtlk_core_start(mtlk_core_t *core, mtlk_hw_t *hw, const mtlk_core_hw_cfg_t *cfg, uint32 num_boards_alive);
int          __MTLK_IFUNC mtlk_core_release_tx_data(mtlk_core_t *core, const mtlk_core_release_tx_data_t *data);
int          __MTLK_IFUNC mtlk_core_handle_rx_data(mtlk_core_t                *core,
                                                   mtlk_core_handle_rx_data_t *data);
int          __MTLK_IFUNC mtlk_core_handle_rx_ctrl(mtlk_core_t         *core,     /* MTLK_ERR_PENDING for pending. In this   */
                                                   const mtlk_hw_msg_t *msg,      /*   case mtlk_hw_resp_rx_ctrl(...) will   */
                                                   uint32               id,       /*   be used to respond later              */
                                                   void*                payload);
int          __MTLK_IFUNC mtlk_core_get_prop(mtlk_core_t *core, mtlk_core_prop_e prop_id, void* buffer, uint32 size);
int          __MTLK_IFUNC mtlk_core_set_prop(mtlk_core_t *core, mtlk_core_prop_e prop_id, void* buffer, uint32 size);
int          __MTLK_IFUNC mtlk_core_stop(mtlk_core_t *core, uint32 num_boards_alive);
void         __MTLK_IFUNC mtlk_core_delete(mtlk_core_t *core);
void         __MTLK_IFUNC mtlk_core_prepare_stop(mtlk_core_t *core);

/**********************************************************************/

/**********************************************************************
 * Core (logic module) => HW :
 *  - must be implemented by HW layer
 *  - called by core (logic module)
***********************************************************************/

typedef enum _mtlk_hw_prop_e
{/* prop_id */
  MTLK_HW_PROP_STATE,        /* buffer: GET: mtlk_hw_state_e*,              SET - mtlk_hw_state_e*                              */
  MTLK_HW_FREE_TX_MSGS,      /* buffer: GET: uint32*,                       SET - not supported                                 */
  MTLK_HW_TX_MSGS_USED_PEAK, /* buffer: GET: uint32*,                       SET - not supported                                 */
  MTLK_HW_PROGMODEL,         /* buffer: GET: mtlk_core_firmware_file_t*,    SET - const mtlk_core_firmware_file_t*              */
  MTLK_HW_DUMP,              /* buffer: GET: mtlk_hw_dump_t*,               SET - not supported                                 */
  MTLK_HW_BCL_ON_EXCEPTION,  /* buffer: GET: UMI_BCL_REQUEST*,              SET - UMI_BCL_REQUEST*                              */
  MTLK_HW_PRINT_BUS_INFO,    /* buffer: GET: char*,                         SET - not supported                                 */
  MTLK_HW_BIST,              /* buffer: GET: uint32*,                       SET - not supported                                 */
  MTLK_HW_PROGMODEL_FREE,    /* buffer: GET: not supported,                 SET - mtlk_core_firmware_file_t*                    */
  MTLK_HW_RESET,             /* buffer: GET: not supported,                 SET - void                                          */
  MTLK_HW_GENERATION,        /* buffer: GET: mtlk_hw_gen_e*,                SET - not supported                                 */
  MTLK_HW_DBG_ASSERT_FW,     /* buffer: GET: not supported,                 SET - uint32* (LMIPS or UMIPS)                      */

  MTLK_HW_PROP_LAST
} mtlk_hw_prop_e;

typedef struct _mtlk_hw_dump_t
{
  uint32 addr;
  uint32 size;
  void*  buffer;
} __MTLK_IDATA mtlk_hw_dump_t;

typedef struct _mtlk_hw_send_data_t
{
  mtlk_hw_msg_t       *msg;             /* HW Message returned by mtlk_hw_get_msg_to_send() */
  mtlk_nbuf_t         *nbuf;            /* Network buffer                                   */
  uint32               size;            /* Data size                                        */
  const IEEE_ADDR     *rcv_addr;        /* WDS MAC address                                  */
  uint8                access_category; /* Packet's access category                         */
  uint8                wds;             /* WDS packet flag                                  */
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  uint8                rf_mgmt_data;    /* RF Management related data, 
                                         * MTLK_RF_MGMT_DATA_DEFAULT if none    
                                         */
#endif
  uint8                encap_type;      /* Encapsulation type, one of defined by UMI        */
} __MTLK_IDATA mtlk_hw_send_data_t;

typedef enum _mtlk_hw_state_e
{
  MTLK_HW_STATE_HALTED,
  MTLK_HW_STATE_INITIATING,
  MTLK_HW_STATE_WAITING_READY,
  MTLK_HW_STATE_READY,
  MTLK_HW_STATE_ERROR,
  MTLK_HW_STATE_EXCEPTION,
  MTLK_HW_STATE_APPFATAL,
  MTLK_HW_STATE_UNLOADING,
  MTLK_HW_STATE_LAST
} mtlk_hw_state_e;

typedef enum _mtlk_hw_gen_e
{
  MTLK_HW_GEN2,
  MTLK_HW_GEN3
} mtlk_hw_gen_e;

mtlk_hw_msg_t*  __MTLK_IFUNC mtlk_hw_get_msg_to_send(mtlk_hw_t *obj,
                                                     uint32     *nof_free_tx_msgs);
int             __MTLK_IFUNC mtlk_hw_send_data(mtlk_hw_t                 *obj,
                                               const mtlk_hw_send_data_t *data);
int             __MTLK_IFUNC mtlk_hw_release_msg_to_send(mtlk_hw_t     *obj,
                                                         mtlk_hw_msg_t *msg);
int             __MTLK_IFUNC mtlk_hw_resp_rx_ctrl(mtlk_hw_t           *obj,
                                                  const mtlk_hw_msg_t *msg);
int             __MTLK_IFUNC mtlk_hw_set_prop(mtlk_hw_t     *obj,
                                              mtlk_hw_prop_e prop_id,
                                              void          *buffer,
                                              uint32         size);
int             __MTLK_IFUNC mtlk_hw_get_prop(mtlk_hw_t     *obj,
                                              mtlk_hw_prop_e prop_id,
                                              void          *buffer,
                                              uint32         size);
/**********************************************************************/

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* !__MTLKHAL_H__ */
