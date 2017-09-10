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
 * Queue of events
 *
 */
#include "mtlkinc.h"

#include "mtlk_eq.h"
#include "core.h"

static BOOL
is_high_priority_event(EVENT_ID id)
{
  return FALSE;
}

int __MTLK_IFUNC
mtlk_eq_notify (struct mtlk_eq *eq, EVENT_ID id, const void *payload, unsigned size)
{
  if (is_high_priority_event(id)) 
    // Process immediately
    mtlk_core_handle_event(eq->nic, id, payload);
  else {
    // enqueue 
    mtlk_eq_entry_t *entry = mtlk_osal_mem_alloc(sizeof(mtlk_eq_entry_t), MTLK_MEM_TAG_EQ);
    if (entry == NULL)
      return MTLK_ERR_NO_MEM;
    memset(entry, 0, sizeof(mtlk_eq_entry_t));

    entry->id = id;

    if (size > 0 && payload != NULL) {
      entry->payload = mtlk_osal_mem_alloc(size, MTLK_MEM_TAG_EQ);
      if (entry->payload == NULL) {
        mtlk_osal_mem_free(entry);
        return MTLK_ERR_NO_MEM;
      }

      memcpy(entry->payload, payload, size);
    }

    mtlk_ldlist_push_back(&eq->events, &entry->link_entry);

    if (eq->started) // in case queue is started - trigger event processing
                     // otherwise - just accumulate events in queue
      _mtlk_eq_osdep_notify(&eq->osdep);
  }

  return MTLK_ERR_OK;
}

void __MTLK_IFUNC 
mtlk_eq_handle_events (struct mtlk_eq *eq)
{
  mtlk_ldlist_entry_t *entry;
  mtlk_eq_entry_t *e;

  while ((entry = mtlk_ldlist_pop_front(&eq->events)) != NULL) {
    e = MTLK_LIST_GET_CONTAINING_RECORD(entry, mtlk_eq_entry_t, link_entry);
    mtlk_core_handle_event(eq->nic, e->id, e->payload);
    if (e->payload != NULL)
      mtlk_osal_mem_free((void*)e->payload);
    mtlk_osal_mem_free(e);
  }
}

int __MTLK_IFUNC
mtlk_eq_init (struct mtlk_eq *eq, struct nic *nic)
{
  int res;

  ASSERT(!eq->initialized);
  ASSERT(!eq->started);

  memset(eq, 0, sizeof(struct mtlk_eq));

  mtlk_ldlist_init(&eq->events);
  eq->nic = nic;
  res = _mtlk_eq_osdep_init(&eq->osdep);
  if (res != MTLK_ERR_OK) {
    ELOG("Failed to initialize OSDEP part of the event queue with err code %i", res);
    goto err;
  }

  eq->initialized = TRUE;

err:
  return res;
}

void __MTLK_IFUNC
mtlk_eq_cleanup (struct mtlk_eq *eq)
{
  mtlk_ldlist_entry_t *entry;
  mtlk_eq_entry_t *e;

  ASSERT(eq->initialized);
  ASSERT(!eq->started);

  _mtlk_eq_osdep_cleanup(&eq->osdep);

  while ((entry = mtlk_ldlist_pop_front(&eq->events)) != NULL) {
    e = MTLK_LIST_GET_CONTAINING_RECORD(entry, mtlk_eq_entry_t, link_entry);
    if (e->payload != NULL)
      mtlk_osal_mem_free((void*)e->payload);
    mtlk_osal_mem_free(e);
  }

  mtlk_ldlist_cleanup(&eq->events);

  eq->initialized = FALSE;
}

int __MTLK_IFUNC
mtlk_eq_start(struct mtlk_eq* eq)
{
  int res;

  ASSERT(eq->initialized);
  ASSERT(!eq->started);
  
  eq->started = TRUE;

  res = _mtlk_eq_osdep_start(&eq->osdep);
  if (res != MTLK_ERR_OK) {
    eq->started = FALSE;
    ELOG("Failed to start OSDEP part of the event queue with err code %i", res);
  }
  
  return res;
}

void __MTLK_IFUNC
mtlk_eq_stop(struct mtlk_eq *eq)
{
  ASSERT(eq->initialized);
  ASSERT(eq->started);

  eq->started = FALSE;
  _mtlk_eq_osdep_stop(&eq->osdep);
}

