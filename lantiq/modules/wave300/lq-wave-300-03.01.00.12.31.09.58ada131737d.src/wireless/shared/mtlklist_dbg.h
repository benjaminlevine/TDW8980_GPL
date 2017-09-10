/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
* $Id$
*
* Copyright (c) 2006 - 2007 Metalink Broadband Ltd.
*
*/


#ifndef __MTLKLIST_DBG_H__
#define __MTLKLIST_DBG_H__

MTLK_DECLARE_OBJPOOL(g_objpool);

static __INLINE void mtlk_slist_init_debug(mtlk_slist_t *slist,
                                           const char* file_name,
                                           uint32 line)
{
  mtlk_slist_init(slist);
  mtlk_objpool_add_object(&g_objpool, &((slist)->obj_hdr), MTLK_SLIST_OBJ,
    file_name, line);
}

static __INLINE void mtlk_slist_cleanup_debug(mtlk_slist_t *slist)
{
  mtlk_objpool_remove_object(&g_objpool, &((slist)->obj_hdr), MTLK_SLIST_OBJ);
  mtlk_slist_cleanup(slist);
}

#define mtlk_slist_init(slist) \
  mtlk_slist_init_debug(slist, __FILE__, __LINE__)

#define mtlk_slist_cleanup(slist) \
  mtlk_slist_cleanup_debug(slist)

static __INLINE void mtlk_lslist_init_debug(mtlk_lslist_t *lslist,
                                           const char* file_name,
                                           uint32 line)
{
  mtlk_lslist_init(lslist);
  mtlk_objpool_add_object(&g_objpool, &((lslist)->obj_hdr), MTLK_LSLIST_OBJ,
    file_name, line);
}

static __INLINE void mtlk_lslist_cleanup_debug(mtlk_lslist_t *lslist)
{
  mtlk_objpool_remove_object(&g_objpool, &((lslist)->obj_hdr), MTLK_LSLIST_OBJ);
  mtlk_lslist_cleanup(lslist);
}

#define mtlk_lslist_init(lslist) \
  mtlk_lslist_init_debug(lslist, __FILE__, __LINE__)

#define mtlk_lslist_cleanup(lslist) \
  mtlk_lslist_cleanup_debug(lslist)

static __INLINE void mtlk_dlist_init_debug(mtlk_dlist_t *dlist,
                                            const char* file_name,
                                            uint32 line)
{
  mtlk_dlist_init(dlist);
  mtlk_objpool_add_object(&g_objpool, &((dlist)->obj_hdr), MTLK_DLIST_OBJ,
    file_name, line);
}

static __INLINE void mtlk_dlist_cleanup_debug(mtlk_dlist_t *dlist)
{
  mtlk_objpool_remove_object(&g_objpool, &((dlist)->obj_hdr), MTLK_DLIST_OBJ);
  mtlk_dlist_cleanup(dlist);
}

static __INLINE void mtlk_dlist_init_objpool(mtlk_dlist_t *dlist)
{
  return mtlk_dlist_init(dlist);
}

static __INLINE void mtlk_dlist_cleanup_objpool(mtlk_dlist_t *dlist)
{
  mtlk_dlist_cleanup(dlist);
}

#define mtlk_dlist_init(dlist) \
  mtlk_dlist_init_debug(dlist, __FILE__, __LINE__)

#define mtlk_dlist_cleanup(dlist) \
  mtlk_dlist_cleanup_debug(dlist)

static __INLINE void mtlk_ldlist_init_debug(mtlk_ldlist_t *ldlist,
                                           const char* file_name,
                                           uint32 line)
{
  mtlk_ldlist_init(ldlist);
  mtlk_objpool_add_object(&g_objpool, &((ldlist)->obj_hdr), MTLK_LDLIST_OBJ,
    file_name, line);
}

static __INLINE void mtlk_ldlist_cleanup_debug(mtlk_ldlist_t *ldlist)
{
  mtlk_objpool_remove_object(&g_objpool, &((ldlist)->obj_hdr), MTLK_LDLIST_OBJ);
  mtlk_ldlist_cleanup(ldlist);
}

#define mtlk_ldlist_init(ldlist) \
  mtlk_ldlist_init_debug(ldlist, __FILE__, __LINE__)

#define mtlk_ldlist_cleanup(ldlist) \
  mtlk_ldlist_cleanup_debug(ldlist)

#endif /* !__MTLKLIST_DBG_H__ */
