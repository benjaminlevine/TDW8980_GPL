/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id: $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Written by: Grygorii Strashko
 *
 * Power management functionality implementation in compliance with
 * Code of Conduct on Energy Consumption of Broadband Equipment (a.k.a CoC)
 *
 */


#ifndef __MTLK_POWER_MANAGEMENT_H__
#define __MTLK_POWER_MANAGEMENT_H__

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/**********************************************************************
 * Public declaration
***********************************************************************/
typedef struct _mtlk_coc_t mtlk_coc_t;

typedef struct _mtlk_coc_antenna_cfg_t
{
  uint8 num_tx_antennas;
  uint8 num_rx_antennas;
} __MTLK_IDATA mtlk_coc_antenna_cfg_t;

typedef struct _mtlk_coc_cfg_t
{
  mtlk_txmm_t *txmm;
  mtlk_coc_antenna_cfg_t hw_antenna_cfg;
} __MTLK_IDATA mtlk_coc_cfg_t;

typedef enum
{
  COC_HIGH_POWER_MODE = 0,
  COC_LOW_POWER_MODE,
  COC_RADIO_OFF_POWER_MODE
} eCOC_POWER_MODE;

/**********************************************************************
 * Public function declaration
***********************************************************************/
mtlk_coc_t* __MTLK_IFUNC
mtlk_coc_create(const mtlk_coc_cfg_t *cfg);

void __MTLK_IFUNC
mtlk_coc_delete(mtlk_coc_t *coc_obj);

int __MTLK_IFUNC
mtlk_coc_low_power_mode_enable(mtlk_coc_t *coc_obj);

int __MTLK_IFUNC
mtlk_coc_high_power_mode_enable(mtlk_coc_t *coc_obj);

int __MTLK_IFUNC
mtlk_coc_radiooff_power_mode_enable(mtlk_coc_t *coc_obj);

eCOC_POWER_MODE __MTLK_IFUNC
mtlk_coc_low_power_mode_get(mtlk_coc_t *coc_obj);

int __MTLK_IFUNC
mtlk_coc_set_mode(mtlk_coc_t *coc_obj, const mtlk_coc_antenna_cfg_t *antenna_cfg);

int __MTLK_IFUNC
mtlk_coc_get_mode(mtlk_coc_t *coc_obj, mtlk_coc_antenna_cfg_t *antenna_cfg);


#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_POWER_MANAGEMENT_H__ */
