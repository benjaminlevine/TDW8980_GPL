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
 * AOCS history OS-dependent module implementation
 *
 */

#include "mtlkinc.h"
#include "aocshistory.h"
#include "mtlkaux.h"

void __MTLK_IFUNC
mtlk_aocs_history_print(mtlk_aocs_history_t *history, mtlk_seq_printf_t *s)
{
  mtlk_dlist_entry_t *list_head;
  mtlk_dlist_entry_t *current_list_entry;
  mtlk_aocs_history_entry_t *history_entry;
  char criteria_text[32];

  mtlk_aux_seq_printf(s, "Channel switch history:\n"
    "Time (ago)            Ch (2nd)"
    "      Switch reason         Selection criteria"
    "\n");

  list_head = mtlk_dlist_head(history);
  current_list_entry = mtlk_dlist_next(list_head);
  while (current_list_entry != list_head) {
    history_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
        mtlk_aocs_history_entry_t, list_entry);

    mtlk_cl_sw_criteria_text(history_entry->info.criteria, 
        &history_entry->info.criteria_details, criteria_text);

    mtlk_aux_seq_printf(s,
      "%04dh %02dm %02d.%03ds    %3d (%3d)  %17s %26s\n"
      , mtlk_get_hours_ago(history_entry->timestamp)
      , mtlk_get_minutes_ago(history_entry->timestamp)
      , mtlk_get_seconds_ago(history_entry->timestamp)
      , mtlk_get_mseconds_ago(history_entry->timestamp)
      , history_entry->info.primary_channel
      , history_entry->info.secondary_channel
      , mtlk_cl_sw_reason_text(history_entry->info.reason)
      , criteria_text);
    /* get next item */
    current_list_entry = mtlk_dlist_next(current_list_entry);
  }
}
