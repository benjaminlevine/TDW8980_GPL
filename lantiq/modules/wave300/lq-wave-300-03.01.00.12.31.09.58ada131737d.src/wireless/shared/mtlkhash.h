/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __MTLK_HASH_H__
#define __MTLK_HASH_H__

#include "mtlklist.h"

/******************************************************************************
 * Generic Hash/mHash definitions:
 *  - Common Hash/mHash __internal__ definitions - DO NOT USE!
 *  - Common Hash/mHash auxilliary macros
 *  - Hash related data types
 *  - Hash declaration macros
 *  - Hash definition macros
 *  - mHash related data types
 *  - mHash declaration macros
 *  - mHash definition macros
 ******************************************************************************/

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/******************************************************************************
 * Common Hash/mHash __internal__ definitions - DO NOT USE!
 ******************************************************************************/
#ifdef MTCFG_DEBUG
#define __MTLK_HASH_POISON(type, x) { (x)=(type)0x02010201; }
#else
#define __MTLK_HASH_POISON(type, x)
#endif

#define __mtlk_hash_init(hash, max_key)                                       \
  {                                                                           \
    hash->lists =                                                             \
      (mtlk_dlist_t *)mtlk_osal_mem_alloc(max_key * sizeof(hash->lists[0]),   \
                                          MTLK_MEM_TAG_HASH);                 \
                                                                              \
    if (hash->lists) {                                                        \
      for (hash->max_key = 0; hash->max_key < max_key; ++hash->max_key) {     \
        mtlk_dlist_init(&hash->lists[hash->max_key]);                         \
      }                                                                       \
      return MTLK_ERR_OK;                                                     \
    }                                                                         \
    else {                                                                    \
      __MTLK_HASH_POISON(mtlk_dlist_t *, hash->lists);                        \
      ERROR("Can't allocate list array of %u", (unsigned)max_key);            \
      return MTLK_ERR_NO_MEM;                                                 \
    }                                                                         \
}

#define __mtlk_hash_cleanup(hash)                                             \
  {                                                                           \
    uint32 i = 0;                                                             \
    for (; i < hash->max_key; ++i) {                                          \
      MTLK_ASSERT(mtlk_dlist_is_empty(&hash->lists[i]));                      \
      mtlk_dlist_cleanup(&hash->lists[i]);                                    \
    }                                                                         \
    mtlk_osal_mem_free(hash->lists);                                          \
    __MTLK_HASH_POISON(mtlk_dlist_t *, hash->lists);                          \
}

#define __mtlk_hash_enum_first(hash, name, e)                                 \
  {                                                                           \
    e->data2 = 0;                                                             \
    e->data1 =                                                                \
      HANDLE_T(mtlk_dlist_next(mtlk_dlist_head(&hash->lists[e->data2])));     \
  }

#define __mtlk_hash_enum_next(hash, entry_type, name, e)                      \
  {                                                                           \
    uint32              hash_idx = e->data2;                                  \
    mtlk_dlist_entry_t *head     = mtlk_dlist_head(&hash->lists[hash_idx]);   \
    mtlk_dlist_entry_t *curr     = HANDLE_T_PTR(mtlk_dlist_entry_t, e->data1);\
    MTLK_ASSERT(hash_idx < hash->max_key);                                    \
    /* If current list done and not a last => find next not empty list */     \
    while (curr == head && hash_idx < hash->max_key - 1) {                    \
      /* Continue to next list */                                             \
      ++hash_idx;                                                             \
      head = mtlk_dlist_head(&hash->lists[hash_idx]);                         \
      curr = mtlk_dlist_next(head);                                           \
    }                                                                         \
    if (curr != head) {                                                       \
      entry_type *entry =                                                     \
        MTLK_CONTAINER_OF(curr, entry_type, lentry);                          \
      e->data1 = HANDLE_T(mtlk_dlist_next(curr));                             \
      e->data2 = hash_idx;                                                    \
      return entry;                                                           \
    }                                                                         \
    __MTLK_HASH_POISON(mtlk_handle_t, e->data1);                              \
    __MTLK_HASH_POISON(uint32, e->data1);                                     \
    return NULL;                                                              \
  }

#define __mtlk_hash_insert(hash, key, entry, hash_f)                          \
  {                                                                           \
    entry->key      = *key;                                                   \
    entry->hash_idx = hash_f(key);                                            \
    mtlk_dlist_push_back(&hash->lists[entry->hash_idx], &entry->lentry);      \
  }

#define __mtlk_hash_remove(hash, entry)                                       \
  {                                                                           \
    mtlk_dlist_remove(&hash->lists[entry->hash_idx],                          \
                      &entry->lentry);                                        \
  }

/******************************************************************************
 * Common Hash/mHash auxilliary macros
 ******************************************************************************/
#define MTLK_HASH_VALUE_GET_KEY(value, field)   (&(value)->field.key)


/******************************************************************************
 * Hash related data types
 ******************************************************************************/
#define MTLK_HASH_ENTRY_T(name)                 struct mtlk_hash_entry_##name

#define MTLK_HASH_DECLARE_ENTRY_T(name, key_type)                             \
  MTLK_HASH_ENTRY_T(name)                                                     \
  {                                                                           \
    key_type           key;                                                   \
    /* Fields below are for private hash usage only */                        \
    mtlk_dlist_entry_t lentry;                                                \
    uint32             hash_idx;                                              \
  } __MTLK_IDATA;

typedef struct
{
  uint32        max_key;
  mtlk_dlist_t *lists;
} __MTLK_IDATA mtlk_hash_t;

typedef struct _mtlk_hash_enum_t
{
  mtlk_handle_t data1; /* Don't use this - for HASH internal use only !*/
  uint32        data2; /* Don't use this - for HASH internal use only !*/
} __MTLK_IDATA mtlk_hash_enum_t;

/******************************************************************************
 * Hash declaration macros
 ******************************************************************************/
#define MTLK_HASH_DECLARE(access, name, key_type)                             \
  access int                                                                  \
  mtlk_hash_init_##name (mtlk_hash_t *hash, uint32 max_key);                  \
  access void                                                                 \
  mtlk_hash_cleanup_##name (mtlk_hash_t *hash);                               \
  access MTLK_HASH_ENTRY_T(name) *                                            \
  mtlk_hash_insert_##name(mtlk_hash_t             *hash,                      \
                          const key_type          *key,                       \
                          MTLK_HASH_ENTRY_T(name) *entry);                    \
  access MTLK_HASH_ENTRY_T(name) *                                            \
  mtlk_hash_insert_replace_##name(mtlk_hash_t             *hash,              \
                                  const key_type          *key,               \
                                  MTLK_HASH_ENTRY_T(name) *entry);            \
  access void                                                                 \
  mtlk_hash_remove_##name(mtlk_hash_t             *hash,                      \
                          MTLK_HASH_ENTRY_T(name) *entry);                    \
  access MTLK_HASH_ENTRY_T(name) *                                            \
  mtlk_hash_find_##name(mtlk_hash_t    *hash,                                 \
                        const key_type *key);                                 \
  access MTLK_HASH_ENTRY_T(name) *                                            \
  mtlk_hash_enum_first_##name(mtlk_hash_t      *hash,                         \
                              mtlk_hash_enum_t *e);                           \
  access MTLK_HASH_ENTRY_T(name) *                                            \
  mtlk_hash_enum_next_##name(mtlk_hash_t      *hash,                          \
                             mtlk_hash_enum_t *e);

#define MTLK_HASH_DECLARE_EXTERN(name, key_type)                              \
  MTLK_HASH_DECLARE(extern, name, key_type)
#define MTLK_HASH_DECLARE_INLINE(name, key_type)                              \
  MTLK_HASH_DECLARE(static __INLINE, name, key_type)
#define MTLK_HASH_DECLARE_STATIC(name, key_type)                              \
  MTLK_HASH_DECLARE(static, name, key_type)

/******************************************************************************
 * Hash definition macros
 ******************************************************************************/
#define MTLK_HASH_DEFINE(access, name, key_type, hash_f, key_cmp_f)           \
  access int                                                                  \
  mtlk_hash_init_##name (mtlk_hash_t *hash, uint32 max_key)                   \
  {                                                                           \
    __mtlk_hash_init(hash, max_key);                                          \
  }                                                                           \
                                                                              \
  access void                                                                 \
  mtlk_hash_cleanup_##name (mtlk_hash_t *hash)                                \
  {                                                                           \
    __mtlk_hash_cleanup(hash);                                                \
  }                                                                           \
                                                                              \
  access MTLK_HASH_ENTRY_T(name) *                                            \
  mtlk_hash_insert_##name (mtlk_hash_t             *hash,                     \
                           const key_type          *key,                      \
                           MTLK_HASH_ENTRY_T(name) *entry)                    \
  {                                                                           \
    MTLK_HASH_ENTRY_T(name) *prev = mtlk_hash_find_##name(hash, key);         \
    if (!prev) {                                                              \
      __mtlk_hash_insert(hash, key, entry, hash_f);                           \
    }                                                                         \
    return prev;                                                              \
  }                                                                           \
                                                                              \
  access MTLK_HASH_ENTRY_T(name) *                                            \
  mtlk_hash_insert_replace_##name(mtlk_hash_t             *hash,              \
                                  const key_type          *key,               \
                                  MTLK_HASH_ENTRY_T(name) *entry)             \
  {                                                                           \
    MTLK_HASH_ENTRY_T(name) *prev = mtlk_hash_find_##name(hash, key);         \
    if (prev) {                                                               \
      __mtlk_hash_remove(hash, prev);                                         \
    }                                                                         \
    __mtlk_hash_insert(hash, key, entry, hash_f);                             \
    return prev;                                                              \
  }                                                                           \
                                                                              \
  access void                                                                 \
  mtlk_hash_remove_##name (mtlk_hash_t             *hash,                     \
                           MTLK_HASH_ENTRY_T(name) *entry)                    \
  {                                                                           \
    __mtlk_hash_remove(hash, entry);                                          \
  }                                                                           \
                                                                              \
  access MTLK_HASH_ENTRY_T(name) *                                            \
  mtlk_hash_find_##name (mtlk_hash_t    *hash,                                \
                         const key_type *key)                                 \
  {                                                                           \
    uint32              hash_idx = hash_f(key);                               \
    mtlk_dlist_entry_t *head;                                                 \
    mtlk_dlist_entry_t *curr;                                                 \
    MTLK_ASSERT(key != NULL);                                                 \
    MTLK_ASSERT(hash_idx < hash->max_key);                                    \
    mtlk_dlist_foreach(&hash->lists[hash_idx], curr, head) {                  \
      MTLK_HASH_ENTRY_T(name) *entry =                                        \
        MTLK_CONTAINER_OF(curr, MTLK_HASH_ENTRY_T(name), lentry);             \
      if (key_cmp_f(key, &entry->key) == 0) {                                 \
        return entry;                                                         \
      }                                                                       \
    }                                                                         \
    return NULL;                                                              \
  }                                                                           \
                                                                              \
  access MTLK_HASH_ENTRY_T(name) *                                            \
  mtlk_hash_enum_first_##name (mtlk_hash_t      *hash,                        \
                               mtlk_hash_enum_t *e)                           \
  {                                                                           \
    __mtlk_hash_enum_first(hash, name, e);                                    \
    return mtlk_hash_enum_next_##name(hash, e);                               \
  }                                                                           \
                                                                              \
  access MTLK_HASH_ENTRY_T(name) *                                            \
  mtlk_hash_enum_next_##name (mtlk_hash_t      *hash,                         \
                              mtlk_hash_enum_t *e)                            \
  {                                                                           \
    __mtlk_hash_enum_next(hash, MTLK_HASH_ENTRY_T(name), name, e);            \
  }

#define MTLK_HASH_DEFINE_EXTERN(name, key_type, hash_f, key_cmp_f)            \
  MTLK_HASH_DEFINE(extern, name, key_type, hash_f, key_cmp_f)
#define MTLK_HASH_DEFINE_INLINE(name, key_type, hash_f, key_cmp_f)            \
  MTLK_HASH_DEFINE(static __INLINE, name, key_type, hash_f, key_cmp_f)
#define MTLK_HASH_DEFINE_STATIC(name, key_type, hash_f, key_cmp_f)            \
  MTLK_HASH_DEFINE(static, name,  key_type, hash_f, key_cmp_f)

/******************************************************************************
 * mHash related data types
 ******************************************************************************/
#define MTLK_MHASH_ENTRY_T(name)               struct mtlk_mhash_entry_##name

#define MTLK_MHASH_DECLARE_ENTRY_T(name, key_type)                            \
  MTLK_MHASH_ENTRY_T(name) __MTLK_IDATA                                       \
  {                                                                           \
    key_type           key;                                                   \
    /* Fields below are for private hash usage only */                        \
    mtlk_dlist_entry_t lentry;                                                \
    uint32             hash_idx;                                              \
  };

typedef struct
{
  uint32        max_key;
  mtlk_dlist_t *lists;
} __MTLK_IDATA mtlk_mhash_t;

typedef struct _mtlk_mhash_find_t
{
  mtlk_handle_t data1; /* Don't use this - for HASH internal use only !*/
  uint32        data2; /* Don't use this - for HASH internal use only !*/
} __MTLK_IDATA mtlk_mhash_find_t;

typedef struct _mtlk_mhash_enum_t
{
  mtlk_handle_t data1; /* Don't use this - for HASH internal use only !*/
  uint32        data2; /* Don't use this - for HASH internal use only !*/
} __MTLK_IDATA mtlk_mhash_enum_t;

/******************************************************************************
 * mHash declaration macros
 ******************************************************************************/
#define MTLK_MHASH_DECLARE(access, name, key_type)                            \
  access int                                                                  \
  mtlk_mhash_init_##name (mtlk_mhash_t *hash, uint32 max_key);                \
  access void                                                                 \
  mtlk_mhash_cleanup_##name (mtlk_mhash_t *hash);                             \
  access void                                                                 \
  mtlk_mhash_insert_##name(mtlk_mhash_t            *hash,                     \
                           const key_type          *key,                      \
                           MTLK_MHASH_ENTRY_T(name) *entry);                  \
  access void                                                                 \
  mtlk_mhash_remove_##name(mtlk_mhash_t            *hash,                     \
                           MTLK_MHASH_ENTRY_T(name) *entry);                  \
  access MTLK_MHASH_ENTRY_T(name) *                                           \
  mtlk_mhash_find_first_##name(mtlk_mhash_t      *hash,                       \
                               const key_type    *key,                        \
                               mtlk_mhash_find_t *f);                         \
  access MTLK_MHASH_ENTRY_T(name) *                                           \
  mtlk_mhash_find_next_##name(mtlk_mhash_t      *hash,                        \
                              const key_type    *key,                         \
                              mtlk_mhash_find_t *f);                          \
  access MTLK_MHASH_ENTRY_T(name) *                                           \
  mtlk_mhash_enum_first_##name(mtlk_mhash_t      *hash,                       \
                               mtlk_mhash_enum_t *e);                         \
  access MTLK_MHASH_ENTRY_T(name) *                                           \
  mtlk_mhash_enum_next_##name(mtlk_mhash_t    *hash,                          \
                              mtlk_mhash_enum_t *e);

#define MTLK_MHASH_DECLARE_EXTERN(name, key_type)                             \
  MTLK_MHASH_DECLARE(extern, name, key_type)
#define MTLK_MHASH_DECLARE_INLINE(name, key_type)                             \
  MTLK_MHASH_DECLARE(static __INLINE, name, key_type)
#define MTLK_MHASH_DECLARE_STATIC(name, key_type)                             \
  MTLK_MHASH_DECLARE(static, name, key_type)

/******************************************************************************
 * mHash definition macros
 ******************************************************************************/
#define MTLK_MHASH_DEFINE(access, name, key_type, hash_f, key_cmp_f)          \
  access int                                                                  \
  mtlk_mhash_init_##name (mtlk_mhash_t *hash, uint32 max_key)                 \
  {                                                                           \
    __mtlk_hash_init(hash, max_key);                                          \
  }                                                                           \
  access void                                                                 \
  mtlk_mhash_cleanup_##name (mtlk_mhash_t *hash)                              \
  {                                                                           \
    __mtlk_hash_cleanup(hash);                                                \
  }                                                                           \
  access void                                                                 \
  mtlk_mhash_insert_##name (mtlk_mhash_t             *hash,                   \
                            const key_type           *key,                    \
                            MTLK_MHASH_ENTRY_T(name) *entry)                  \
  {                                                                           \
    __mtlk_hash_insert(hash, key, entry, hash_f);                             \
  }                                                                           \
                                                                              \
  access void                                                                 \
  mtlk_mhash_remove_##name (mtlk_mhash_t             *hash,                   \
                            MTLK_MHASH_ENTRY_T(name) *entry)                  \
  {                                                                           \
    __mtlk_hash_remove(hash, entry);                                          \
  }                                                                           \
                                                                              \
  access MTLK_MHASH_ENTRY_T(name) *                                           \
  mtlk_mhash_find_first_##name (mtlk_mhash_t     *hash,                       \
                               const key_type    *key,                        \
                               mtlk_mhash_find_t *f)                          \
  {                                                                           \
    MTLK_ASSERT(key != NULL);                                                 \
    f->data2 = hash_f(key);                                                   \
    f->data1 =                                                                \
      HANDLE_T(mtlk_dlist_next(mtlk_dlist_head(&hash->lists[f->data2])));     \
    return mtlk_mhash_find_next_##name(hash, key, f);                         \
  }                                                                           \
                                                                              \
  access MTLK_MHASH_ENTRY_T(name) *                                           \
  mtlk_mhash_find_next_##name (mtlk_mhash_t     *hash,                        \
                              const key_type    *key,                         \
                              mtlk_mhash_find_t *f)                           \
  {                                                                           \
    uint32              hash_idx = f->data2;                                  \
    mtlk_dlist_entry_t *head     = mtlk_dlist_head(&hash->lists[hash_idx]);   \
    mtlk_dlist_entry_t *curr     = HANDLE_T_PTR(mtlk_dlist_entry_t, f->data1);\
    MTLK_ASSERT(key != NULL);                                                 \
    MTLK_ASSERT(hash_idx < hash->max_key);                                    \
    while (curr != head) {                                                    \
      MTLK_MHASH_ENTRY_T(name) *entry =                                       \
        MTLK_CONTAINER_OF(curr, MTLK_MHASH_ENTRY_T(name), lentry);            \
      if (key_cmp_f(key, &entry->key) == 0) {                                 \
        f->data1 = HANDLE_T(mtlk_dlist_next(curr));                           \
        f->data2 = hash_idx;                                                  \
        return entry;                                                         \
      }                                                                       \
      curr = mtlk_dlist_next(curr);                                           \
    }                                                                         \
    __MTLK_HASH_POISON(mtlk_handle_t, f->data1);                              \
    __MTLK_HASH_POISON(uint32, f->data2);                                     \
    return NULL;                                                              \
  }                                                                           \
                                                                              \
  access MTLK_MHASH_ENTRY_T(name) *                                           \
  mtlk_mhash_enum_first_##name (mtlk_mhash_t      *hash,                      \
                                mtlk_mhash_enum_t *e)                         \
  {                                                                           \
    __mtlk_hash_enum_first(hash, name, e);                                    \
    return mtlk_mhash_enum_next_##name(hash, e);                              \
  }                                                                           \
                                                                              \
  access MTLK_MHASH_ENTRY_T(name) *                                           \
  mtlk_mhash_enum_next_##name (mtlk_mhash_t      *hash,                       \
                               mtlk_mhash_enum_t *e)                          \
  {                                                                           \
    __mtlk_hash_enum_next(hash,MTLK_MHASH_ENTRY_T(name),  name, e);           \
  }

#define MTLK_MHASH_DEFINE_EXTERN(name, key_type, hash_f, key_cmp_f)           \
  MTLK_MHASH_DEFINE(extern, name, key_type, hash_f, key_cmp_f)
#define MTLK_MHASH_DEFINE_INLINE(name, key_type, hash_f, key_cmp_f)           \
  MTLK_MHASH_DEFINE(static __INLINE, name, key_type, hash_f, key_cmp_f)
#define MTLK_MHASH_DEFINE_STATIC(name, key_type, hash_f, key_cmp_f)           \
  MTLK_MHASH_DEFINE(static, name,  key_type, hash_f, key_cmp_f)

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_HASH_H__ */
