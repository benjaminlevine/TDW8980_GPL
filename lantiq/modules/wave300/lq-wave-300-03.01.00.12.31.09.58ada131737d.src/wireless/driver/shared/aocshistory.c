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
#include "mtlkinc.h"

#include "aocshistory.h"

#define MAX_HISTORY_LENGTH 20

void __MTLK_IFUNC 
mtlk_aocs_history_init(mtlk_aocs_history_t *history)
{
    mtlk_dlist_init(history);
}

void __MTLK_IFUNC 
mtlk_aocs_history_clean(mtlk_aocs_history_t *history)
{
    mtlk_dlist_entry_t* entry;
    mtlk_aocs_history_entry_t* history_entry;
    
    while( NULL != (entry = mtlk_dlist_pop_back(history)) )
    {
        history_entry = MTLK_LIST_GET_CONTAINING_RECORD(entry,
            mtlk_aocs_history_entry_t, list_entry);
        mtlk_osal_mem_free(history_entry);
    }

    mtlk_dlist_cleanup(history);
}

#ifdef MTCFG_DEBUG

static BOOL
aocs_history_validate_sw_info(mtlk_sw_info_t* info)
{
    return (info->reason   > SWR_LOWER_BOUND)  &&
           (info->reason   < SWR_HIGHER_BOUND) &&
           (info->criteria > CHC_LOWER_BOUND)  &&
           (info->criteria < CHC_HIGHER_BOUND);
}

#endif

BOOL __MTLK_IFUNC 
mtlk_aocs_history_add(mtlk_aocs_history_t *history, 
                    mtlk_sw_info_t* info)
{
    mtlk_aocs_history_entry_t* history_entry;

    MTLK_ASSERT(aocs_history_validate_sw_info(info));

    if(mtlk_dlist_size(history) < MAX_HISTORY_LENGTH)
    {
        history_entry = mtlk_osal_mem_alloc(sizeof(*history_entry), 
            MTLK_MEM_TAG_ANTENNA_GAIN);

        if(NULL == history_entry)
        {
            ELOG("Failed to allocate channel switch history entry.");
            return FALSE;
        }
    }
    else
    {
        mtlk_dlist_entry_t* entry;
        entry = mtlk_dlist_pop_front(history);

        history_entry = MTLK_LIST_GET_CONTAINING_RECORD(entry,
            mtlk_aocs_history_entry_t, list_entry);
    }

    history_entry->info = *info;
    history_entry->timestamp = mtlk_osal_timestamp();
    mtlk_dlist_push_back(history, &history_entry->list_entry);

    return TRUE;
}

char* __MTLK_IFUNC
mtlk_cl_sw_reason_text(switch_reasons_t reason)
{
    switch(reason)
    {
    case SWR_LOW_THROUGHPUT:
        return "Low TX rate";
    case SWR_HIGH_SQ_LOAD:
        return "High SQ load";
    case SWR_RADAR_DETECTED:
        return "Radar";
    case SWR_CHANNEL_LOAD_CHANGED:
        return "Channel load";
    case SWR_INITIAL_SELECTION:
        return "Initial selection";
    case SWR_MAC_PRESSURE_TEST:
        return "Pressure test";
    default:
        MTLK_ASSERT(FALSE);
        return "UNKNOWN";
    }
};

void __MTLK_IFUNC
mtlk_cl_sw_criteria_text(channel_criteria_t criteria,
                         channel_criteria_details_t* criteria_details,
                         char* buff)
{
    switch(criteria)
    {
    case CHC_SCAN_RANK:
        sprintf(buff, "Scan rank (%u)", criteria_details->scan.rank);
        return;
    case CHC_CONFIRM_RANK:
        sprintf(buff, "Confirm rank (%u --> %u)", 
          criteria_details->confirm.old_rank, criteria_details->confirm.new_rank);
        return;
    case CHC_RANDOM:
        strcpy(buff, "Random");
        return;
    case CHC_USERDEF:
        strcpy(buff, "Defined by user");
        return;
    case CHC_2GHZ_BSS_MAJORITY:
        strcpy(buff, "BSS Majority");
        return;
    case CHC_LOWEST_TIMEOUT:
        strcpy(buff, "Minimal timeout");
        return;
    default:
        strcpy(buff, "UNKNOWN");
        MTLK_ASSERT(FALSE);
        return;
    }
};
