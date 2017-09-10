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

#ifndef _MTLK_OBJPOOL_H_
#define _MTLK_OBJPOOL_H_

/** 
*\file mtlk_objpool.h
*\brief OSAL object tracking module
*/

#ifdef MTCFG_ENABLE_OBJPOOL

typedef enum _mtlk_objtypes_t
{
  /* ID's are started from magic number on order  */
  /* to catch situation when uninitialized object */
  /* is being passed to ObjPool functions         */

  MTLK_OBJTYPES_START = 0xABCD,

  MTLK_MEMORY_OBJ,
  MTLK_TIMER_OBJ,
  MTLK_SPINLOCK_OBJ,
  MTLK_EVENT_OBJ,
  MTLK_MUTEX_OBJ,
  MTLK_SLIST_OBJ,
  MTLK_LSLIST_OBJ,
  MTLK_DLIST_OBJ,
  MTLK_LDLIST_OBJ,
  MTLK_THREAD_OBJ, /* applications only */
  MTLK_TXMM_OBJ,   /* driver only */

  MTLK_OBJTYPES_END
} mtlk_objtypes_t;

struct _mtlk_objheader_t;
struct _mtlk_objpool_t;

typedef struct _mtlk_objheader_t mtlk_objheader_t;
typedef struct _mtlk_objpool_t mtlk_objpool_t;

#define MTLK_DECLARE_OBJECT_HEADER(name) \
  mtlk_objheader_t *name;

#define MTLK_DECLARE_OBJPOOL(name) \
  extern mtlk_objpool_t name;

int __MTLK_IFUNC mtlk_objpool_init(mtlk_objpool_t* objpool);
void __MTLK_IFUNC mtlk_objpool_cleanup(mtlk_objpool_t* objpool);
void __MTLK_IFUNC mtlk_objpool_dump(mtlk_objpool_t* objpool);

/* mtlk_objpool_enum_f return FALSE to stop enumeration, TRUE - to continue it */
typedef BOOL (__MTLK_IFUNC *mtlk_objpool_enum_f)(mtlk_objpool_t* objpool, 
                                                 mtlk_objheader_t** object,
                                                 mtlk_handle_t context);

void __MTLK_IFUNC mtlk_objpool_enum_by_type(mtlk_objpool_t* objpool,
                                            mtlk_objtypes_t object_type,
                                            mtlk_objpool_enum_f clb,
                                            mtlk_handle_t context);

void __MTLK_IFUNC 
mtlk_objpool_add_object_ex(mtlk_objpool_t* objpool, 
                           mtlk_objheader_t** object,
                           mtlk_objtypes_t object_type,
                           const char* creator_file_name,
                           uint32 creator_file_line,
                           mtlk_handle_t additional_info);

void __MTLK_IFUNC mtlk_objpool_remove_object_ex(mtlk_objpool_t* objpool,
                                                mtlk_objheader_t** object,
                                                mtlk_objtypes_t object_type,
                                                mtlk_handle_t additional_info);

const char* __MTLK_IFUNC 
mtlk_objpool_get_creator_file_name (mtlk_objpool_t* objpool, 
                                    mtlk_objheader_t** object);
uint32 __MTLK_IFUNC 
mtlk_objpool_get_creator_file_line (mtlk_objpool_t* objpool, 
                                    mtlk_objheader_t** object);

uint32 __MTLK_IFUNC
mtlk_objpool_get_memory_allocated(mtlk_objpool_t* objpool);

typedef struct _mtlk_objpool_memory_alarm_t
{
  uint32        limit;
  void          (__MTLK_IFUNC *clb)(mtlk_handle_t usr_ctx,
                                    uint32        allocated);
  mtlk_handle_t usr_ctx;
} mtlk_objpool_memory_alarm_t;

void __MTLK_IFUNC
mtlk_objpool_set_memory_alarm(mtlk_objpool_t* objpool, 
                              const mtlk_objpool_memory_alarm_t *alarm_info);

#else /* MTCFG_ENABLE_OBJPOOL */

#define MTLK_DECLARE_OBJECT_HEADER(name)
#define MTLK_DECLARE_OBJPOOL(name)

#define mtlk_objpool_init(objpool)    (MTLK_ERR_OK)
#define mtlk_objpool_cleanup(objpool)
#define mtlk_objpool_dump(objpool)
#define mtlk_objpool_enum_by_type(objpool, object_type, clb, context)

#define mtlk_objpool_add_object_ex(objpool, object, object_type, creator_file_name, creator_file_line, additional_info)
#define mtlk_objpool_remove_object_ex(objpool, object, object_type, additional_info)

#define mtlk_objpool_set_memory_alarm(objpool, alarm_info)

#endif /* MTCFG_ENABLE_OBJPOOL */

#define mtlk_objpool_add_object(objpool, object, object_type, creator_file_name, creator_file_line) \
  mtlk_objpool_add_object_ex((objpool), (object), (object_type), (creator_file_name), (creator_file_line), HANDLE_T(0))

#define mtlk_objpool_remove_object(objpool, object, object_type) \
  mtlk_objpool_remove_object_ex((objpool), (object), (object_type), HANDLE_T(0))

#endif /* _MTLK_OBJPOOL_H_ */
