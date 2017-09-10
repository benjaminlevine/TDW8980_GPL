/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
* $Id$
*
* Copyright (c) 2006-2009 Metalink Broadband (Israel)
*
* Written by: Dmitry Fleytman
*
*/

#include "mtlkinc.h"
#include "mtlk_osal.h"
#include "mtlklist.h"

#ifdef MTCFG_ENABLE_OBJPOOL

struct _mtlk_objheader_t
{
  struct _mtlk_objheader_t **parent_obj_ptr; /* To be passed to enum clb to allow OFFSET_OF usage */
  mtlk_objtypes_t            type;
  const char*                creator_file_name;
  uint32                     creator_file_line;
  mtlk_dlist_entry_t         list_entry;
};

struct _mtlk_objpool_memory_t
{
  struct _mtlk_objpool_memory_alarm_t alarm;
  uint32                              allocated;
};

struct _mtlk_objpool_t
{
  mtlk_osal_spinlock_t          list_lock;
  mtlk_dlist_t                  objects_list_head;
  struct _mtlk_objpool_memory_t memory;
};

int __MTLK_IFUNC mtlk_objpool_init(mtlk_objpool_t* objpool)
{
  MTLK_ASSERT(NULL != objpool);

  memset(objpool, 0, sizeof(*objpool));

  mtlk_osal_lock_init_objpool(&objpool->list_lock);
  mtlk_dlist_init_objpool(&objpool->objects_list_head);
#ifndef MTLK_OBJPOOL_NO_TRACE
  LOG0(": Object pool initialized.");
#endif
  return MTLK_ERR_OK;
}

void __MTLK_IFUNC mtlk_objpool_cleanup(mtlk_objpool_t* objpool)
{
  MTLK_ASSERT(NULL != objpool);

  mtlk_objpool_dump(objpool);

  MTLK_ASSERT(mtlk_dlist_is_empty(&objpool->objects_list_head));

  mtlk_dlist_cleanup_objpool(&objpool->objects_list_head);
  mtlk_osal_lock_cleanup_objpool(&objpool->list_lock);
}

#ifndef MTCFG_SILENT
static const char* get_objtype_text(mtlk_objtypes_t objtype)
{
  switch(objtype) {
  case MTLK_MEMORY_OBJ:
    return "Memory";
  case MTLK_TIMER_OBJ:
    return "Timer";
  case MTLK_SPINLOCK_OBJ:
    return "Spinlock";
  case MTLK_EVENT_OBJ:
    return "Event";
  case MTLK_MUTEX_OBJ:
    return "Mutex";
  case MTLK_SLIST_OBJ:
    return "Slist";
  case MTLK_LSLIST_OBJ:
    return "Lslist";
  case MTLK_DLIST_OBJ:
    return "Dlist";
  case MTLK_LDLIST_OBJ:
    return "Ldlist";
  case MTLK_THREAD_OBJ:
    return "Thread";
  case MTLK_TXMM_OBJ:
    return "TXMM slot";
  default:
    LOG0("Unknown object type %d", objtype);
    MTLK_ASSERT(FALSE);
    return "Unknown";
  }
}
#endif

void __MTLK_IFUNC mtlk_objpool_dump(mtlk_objpool_t* objpool)
{
  uint32 obj_counter = 0;
  mtlk_dlist_entry_t *entry, *head;

  MTLK_ASSERT(NULL != objpool);

  mtlk_osal_lock_acquire(&objpool->list_lock);

  mtlk_dlist_foreach(&objpool->objects_list_head, entry, head)
  {
#ifndef MTCFG_SILENT
    mtlk_objheader_t* obj_header = 
      MTLK_LIST_GET_CONTAINING_RECORD(entry, mtlk_objheader_t, list_entry);

    LOG0("dump: object type \"%s\", created at %s:%d"
         , get_objtype_text(obj_header->type)
         , obj_header->creator_file_name
         , obj_header->creator_file_line);
#endif
    obj_counter++;
  }

  mtlk_osal_lock_release(&objpool->list_lock);

  if (0 == obj_counter) {
#ifndef MTLK_OBJPOOL_NO_TRACE
    LOG0("dump: Object pool is empty.");
#endif
  }
  else {
    LOG0("dump: %d object(s) are still in object pool.", obj_counter);
  }
}

void __MTLK_IFUNC 
mtlk_objpool_add_object_ex(mtlk_objpool_t* objpool, 
                           mtlk_objheader_t** object,
                           mtlk_objtypes_t object_type,
                           const char* creator_file_name,
                           uint32 creator_file_line,
                           mtlk_handle_t additional_info)
{
  MTLK_ASSERT(NULL != objpool);
  MTLK_ASSERT(NULL != object);

  MTLK_ASSERT(object_type > MTLK_OBJTYPES_START);
  MTLK_ASSERT(object_type < MTLK_OBJTYPES_END);
  MTLK_ASSERT(NULL != creator_file_name);

  /* Allocate object header */
  *object = 
    (mtlk_objheader_t*)mtlk_osal_mem_alloc_objpool(sizeof(mtlk_objheader_t), 
                                                   MTLK_MEM_TAG_OBJPOOL);
  if(NULL == *object) 
  {
    ERROR("Failed to allocate object header.");
    return;
  }

  /* Fill object header */
  (*object)->parent_obj_ptr = object;
  (*object)->type = object_type;
  (*object)->creator_file_name = creator_file_name;
  (*object)->creator_file_line = creator_file_line;

  /* Put new object to the list */
  mtlk_osal_lock_acquire(&objpool->list_lock);
  mtlk_dlist_push_back(&objpool->objects_list_head, &(*object)->list_entry);
  if (object_type == MTLK_MEMORY_OBJ) {
    objpool->memory.allocated += HANDLE_T_INT(uint32, additional_info);
    if (objpool->memory.alarm.limit && 
        objpool->memory.allocated >= objpool->memory.alarm.limit) {
      objpool->memory.alarm.clb(objpool->memory.alarm.usr_ctx, objpool->memory.allocated);
    }
  }
  mtlk_osal_lock_release(&objpool->list_lock);
}

void __MTLK_IFUNC mtlk_objpool_remove_object_ex(mtlk_objpool_t* objpool,
                                                mtlk_objheader_t** object,
                                                mtlk_objtypes_t object_type,
                                                mtlk_handle_t additional_info)
{
  MTLK_ASSERT(NULL != objpool);
  MTLK_ASSERT(NULL != object);
  MTLK_ASSERT(NULL != *object);
  MTLK_ASSERT(object_type == (*object)->type);

  mtlk_osal_lock_acquire(&objpool->list_lock);
  mtlk_dlist_remove(&objpool->objects_list_head, &(*object)->list_entry);
  if (object_type == MTLK_MEMORY_OBJ) {
    objpool->memory.allocated -= HANDLE_T_INT(uint32, additional_info);
  }
  mtlk_osal_lock_release(&objpool->list_lock);

  mtlk_osal_mem_free_objpool(*object);
}

const char* __MTLK_IFUNC 
mtlk_objpool_get_creator_file_name (mtlk_objpool_t* objpool, 
                                    mtlk_objheader_t** object)
{
  MTLK_ASSERT(NULL != objpool);
  MTLK_ASSERT(NULL != object);
  MTLK_ASSERT(NULL != *object);

  return (*object)->creator_file_name;
}

uint32 __MTLK_IFUNC 
mtlk_objpool_get_creator_file_line (mtlk_objpool_t* objpool, 
                                    mtlk_objheader_t** object)
{
  MTLK_ASSERT(NULL != objpool);
  MTLK_ASSERT(NULL != object);
  MTLK_ASSERT(NULL != *object);

  return (*object)->creator_file_line;
}

void __MTLK_IFUNC
mtlk_objpool_enum_by_type (mtlk_objpool_t* objpool,
                           mtlk_objtypes_t object_type,
                           mtlk_objpool_enum_f clb,
                           mtlk_handle_t context)
{
  mtlk_dlist_entry_t *entry, *head;

  MTLK_ASSERT(NULL != objpool);
  MTLK_ASSERT(NULL != clb);

  mtlk_osal_lock_acquire(&objpool->list_lock);

  mtlk_dlist_foreach(&objpool->objects_list_head, entry, head)
  {
    mtlk_objheader_t* obj_header = 
      MTLK_LIST_GET_CONTAINING_RECORD(entry, mtlk_objheader_t, list_entry);

    if (object_type == obj_header->type) {
      clb(objpool, obj_header->parent_obj_ptr, context);
    }
  }

  mtlk_osal_lock_release(&objpool->list_lock);
}

uint32 __MTLK_IFUNC
mtlk_objpool_get_memory_allocated (mtlk_objpool_t* objpool)
{
  return objpool->memory.allocated;
}

void __MTLK_IFUNC
mtlk_objpool_set_memory_alarm (mtlk_objpool_t* objpool, 
                               const mtlk_objpool_memory_alarm_t *alarm_info)
{
  mtlk_osal_lock_acquire(&objpool->list_lock);
  if (alarm_info) {
    objpool->memory.alarm = *alarm_info;
  }
  else {
    memset(&objpool->memory.alarm, 0, sizeof(objpool->memory.alarm));
  }
  mtlk_osal_lock_release(&objpool->list_lock);
}

mtlk_objpool_t g_objpool;

#endif /* MTCFG_ENABLE_OBJPOOL */
