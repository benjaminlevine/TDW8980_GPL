/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __MTLK_ASEL_H__
#define __MTLK_ASEL_H__

#include "mtlk_osal.h"
#include "txmm.h"
#include "stadb.h"


#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

typedef struct
{
  sta_db      *stadb;
  mtlk_txmm_t *txmm;
  uint8    (__MTLK_IDATA *device_is_busy)(mtlk_handle_t context);
} __MTLK_IDATA mtlk_rf_mgmt_cfg_t;

typedef struct
{
  IEEE_ADDR src_addr;
  uint32    data_size;
  uint8     data[1];
} __MTLK_IDATA mtlk_rf_mgmt_spr_t;

typedef struct
{
  mtlk_dlist_t         data;
  uint32               dlim; /* MAX data items to store */
  mtlk_osal_spinlock_t lock;
} __MTLK_IDATA mtlk_rf_mgmt_db_t;

typedef enum 
{
  MTLK_RF_MGMT_DEID_SET_TYPE,
  MTLK_RF_MGMT_DEID_GET_TYPE,
  MTLK_RF_MGMT_DEID_SET_DEF_ASET,
  MTLK_RF_MGMT_DEID_GET_DEF_ASET,
  MTLK_RF_MGMT_DEID_GET_PEER_ASET,
  MTLK_RF_MGMT_DEID_SET_PEER_ASET,
  MTLK_RF_MGMT_DEID_SEND_SP,
  MTLK_RF_MGMT_DEID_GET_SPR,
  MTLK_RF_MGMT_DEID_LAST
} mtlk_rf_mgmt_drv_evtid_e;

typedef struct
{
  mtlk_rf_mgmt_cfg_t cfg;
  uint8              type;
  uint8              def_rf_mgmt_data;
  mtlk_rf_mgmt_db_t  spr_db;
  mtlk_handle_t      irbh[MTLK_RF_MGMT_DEID_LAST];
  mtlk_txmm_msg_t    vsaf_man_msg[STA_MAX_STATIONS];
  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_INIT_LOOP(IRB_REG);
  MTLK_DECLARE_INIT_LOOP(TXMM_MSG_INIT);  
} __MTLK_IDATA mtlk_rf_mgmt_t;

int    __MTLK_IFUNC mtlk_rf_mgmt_init(mtlk_rf_mgmt_t *rf_mgmt, const mtlk_rf_mgmt_cfg_t *cfg);
int    __MTLK_IFUNC mtlk_rf_mgmt_set_type_blocked(mtlk_rf_mgmt_t *rf_mgmt, uint8 type);
int    __MTLK_IFUNC mtlk_rf_mgmt_set_def_data_blocked(mtlk_rf_mgmt_t *rf_mgmt, 
                                                      uint8           rf_mgmt_data);
int    __MTLK_IFUNC mtlk_rf_mgmt_set_sta_data(mtlk_rf_mgmt_t *rf_mgmt, 
                                              const uint8    *mac_addr,
                                              uint8           rf_mgmt_data);
int    __MTLK_IFUNC mtlk_rf_mgmt_get_sta_data(mtlk_rf_mgmt_t *rf_mgmt, 
                                              const uint8    *mac_addr,
                                              uint8          *rf_mgmt_data);

int    __MTLK_IFUNC mtlk_rf_mgmt_send_sp(mtlk_rf_mgmt_t *rf_mgmt, 
                                         uint8           rf_mgmt_data, 
                                         uint8           rank,
                                         const void     *data, 
                                         uint32          data_size);
int     __MTLK_IFUNC mtlk_rf_mgmt_send_sp_blocked(mtlk_rf_mgmt_t *rf_mgmt, 
                                                  uint8           rf_mgmt_data, 
                                                  uint8           rank,
                                                  const void     *data, 
                                                  uint32          data_size);
int    __MTLK_IFUNC mtlk_rf_mgmt_handle_spr(mtlk_rf_mgmt_t  *rf_mgmt, 
                                            const IEEE_ADDR *src_addr, 
                                            uint8           *buffer, 
                                            uint16           size);

uint32 __MTLK_IFUNC mtlk_rf_mgmt_db_spr_get_available(mtlk_rf_mgmt_t *rf_mgmt);
mtlk_rf_mgmt_spr_t *
       __MTLK_IFUNC mtlk_rf_mgmt_db_spr_get(mtlk_rf_mgmt_t *rf_mgmt);
void   __MTLK_IFUNC mtlk_rf_mgmt_db_spr_release(mtlk_rf_mgmt_t     *rf_mgmt,
                                                mtlk_rf_mgmt_spr_t *spr);
void   __MTLK_IFUNC mtlk_rf_mgmt_db_spr_return(mtlk_rf_mgmt_t     *rf_mgmt,
                                               mtlk_rf_mgmt_spr_t *spr);
void   __MTLK_IFUNC mtlk_rf_mgmt_db_spr_set_lim(mtlk_rf_mgmt_t *rf_mgmt,
                                                uint32          lim);
uint32 __MTLK_IFUNC mtlk_rf_mgmt_db_spr_get_lim(mtlk_rf_mgmt_t *rf_mgmt);
void   __MTLK_IFUNC mtlk_rf_mgmt_cleanup(mtlk_rf_mgmt_t *rf_mgmt);

static __INLINE uint8
mtlk_rf_mgmt_get_type (mtlk_rf_mgmt_t *rf_mgmt)
{
  return rf_mgmt->type;
}

static __INLINE uint8
mtlk_rf_mgmt_get_def_data (mtlk_rf_mgmt_t *rf_mgmt)
{
  return rf_mgmt->def_rf_mgmt_data;
}

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_ASEL_H__ */
