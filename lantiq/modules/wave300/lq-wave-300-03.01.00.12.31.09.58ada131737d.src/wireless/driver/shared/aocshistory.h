/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id$
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Written by: Dmitry Fleytman
 *
 */

#ifndef __aocshistory_h__
#define __aocshistory_h__

#include "mtlkerr.h"
#include "mtlk_osal.h"
#include "mtlklist.h"

#define MTLK_IDEFS_ON
#include "mtlkidefs.h"

typedef mtlk_dlist_t mtlk_aocs_history_t;

typedef enum
{
    //Zero means that reason field was not filled
    SWR_UNKNOWN = 0,
    SWR_LOWER_BOUND,

    SWR_LOW_THROUGHPUT,
    SWR_HIGH_SQ_LOAD,
    SWR_RADAR_DETECTED,
    SWR_CHANNEL_LOAD_CHANGED,
    SWR_INITIAL_SELECTION,
    SWR_MAC_PRESSURE_TEST,

    SWR_HIGHER_BOUND
} switch_reasons_t;

typedef enum
{
    //Zero means that criteria field was not filled
    CHC_UNKNOWN = 0,
    CHC_LOWER_BOUND,

    CHC_SCAN_RANK,
    CHC_CONFIRM_RANK,
    CHC_RANDOM,
    CHC_USERDEF,
    CHC_2GHZ_BSS_MAJORITY,
    CHC_LOWEST_TIMEOUT,

    CHC_HIGHER_BOUND
} channel_criteria_t;

typedef union _channel_criteria_details_t
{
  struct {
    uint8 rank; 
  } scan;
  struct {
    uint8 old_rank;
    uint8 new_rank;
  } confirm;
} channel_criteria_details_t;

typedef struct
{
    uint16                      primary_channel;
    uint16                      secondary_channel;
    switch_reasons_t            reason;
    channel_criteria_t          criteria;
    channel_criteria_details_t  criteria_details;
} mtlk_sw_info_t;

typedef struct
{
    mtlk_sw_info_t          info;
    mtlk_osal_timestamp_t   timestamp;
    mtlk_dlist_entry_t      list_entry;
} mtlk_aocs_history_entry_t;

void __MTLK_IFUNC 
mtlk_aocs_history_init(mtlk_aocs_history_t *history);

void __MTLK_IFUNC 
mtlk_aocs_history_clean(mtlk_aocs_history_t *history);

BOOL __MTLK_IFUNC 
mtlk_aocs_history_add(mtlk_aocs_history_t *history, 
                      mtlk_sw_info_t* info);

char* __MTLK_IFUNC
mtlk_cl_sw_reason_text(switch_reasons_t reason);

void __MTLK_IFUNC
mtlk_cl_sw_criteria_text(channel_criteria_t criteria,
                         channel_criteria_details_t* criteria_details,
                         char* buff);

#define MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __aocshistory_h__ */
