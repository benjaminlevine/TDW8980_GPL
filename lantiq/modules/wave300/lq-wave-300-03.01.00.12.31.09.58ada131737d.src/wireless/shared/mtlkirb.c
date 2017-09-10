/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"
#include "mtlkirb.h"
#include "mtlkhash.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

MTLK_MHASH_DECLARE_ENTRY_T(irb, mtlk_guid_t);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

struct mtlk_irb_evt_handler
{
  mtlk_irb_evt_handler_f  func;
  mtlk_handle_t           context;
  uint32                  owner;
  MTLK_MHASH_ENTRY_T(irb) hentry;
};

MTLK_MHASH_DECLARE_STATIC(irb, mtlk_guid_t);

#define IRB_HASH_MAX_IDX 10

static __INLINE uint32
irb_hash_hash_func (const mtlk_guid_t *key)
{
  return key->data1 % IRB_HASH_MAX_IDX;
}

static __INLINE int
irb_hash_key_cmp_func (const mtlk_guid_t *key1,
                        const mtlk_guid_t *key2)
{
  return mtlk_guid_compare(key1, key2);
}

MTLK_MHASH_DEFINE_STATIC(irb, 
                         mtlk_guid_t,
                         irb_hash_hash_func,
                         irb_hash_key_cmp_func);

struct mtlk_irb
{
  mtlk_mhash_t      hash;
  mtlk_atomic_t     owner_id;
  mtlk_osal_mutex_t lock;
  uint32            state;
};

static struct mtlk_irb irb;
static int             irb_inited = 0;

#define MTLK_IRBS_LOCK_INITED  0x00000001
#define MTLK_IRBS_HASH_INITED  0x00000002

static int __MTLK_IFUNC
_mtlk_irb_add_owner_node(mtlk_handle_t          owner,
                         const mtlk_guid_t     *evt,
                         mtlk_irb_evt_handler_f handler,
                         mtlk_handle_t          context)
{
  int                          res = MTLK_ERR_UNKNOWN;
  struct mtlk_irb_evt_handler *info;

  info = 
    (struct mtlk_irb_evt_handler *)mtlk_osal_mem_alloc(sizeof(*info), 
                                                       MTLK_MEM_TAG_IRB);
  if (!info) {
    ERROR("Can't allocate IRB node!");
    res = MTLK_ERR_NO_MEM;
    goto end;
  }

  info->func    = handler;
  info->context = context;
  info->owner   = owner;

  mtlk_osal_mutex_acquire(&irb.lock);
  mtlk_mhash_insert_irb(&irb.hash, evt, &info->hentry);
  mtlk_osal_mutex_release(&irb.lock);

  res = MTLK_ERR_OK;

end:
  if (res != MTLK_ERR_OK && info) {
    mtlk_osal_mem_free(info);
  }

  return res;
}

static  __INLINE void __MTLK_IFUNC
_mtlk_irb_remove_all_nodes (void)
{
  mtlk_mhash_enum_t        ctx;
  MTLK_MHASH_ENTRY_T(irb) *h;

  h = mtlk_mhash_enum_first_irb(&irb.hash, &ctx);
  while (h) {
    struct mtlk_irb_evt_handler *info = 
      MTLK_CONTAINER_OF(h, struct mtlk_irb_evt_handler, hentry);
    mtlk_mhash_remove_irb(&irb.hash, &info->hentry);
    mtlk_osal_mem_free(info);

    h = mtlk_mhash_enum_next_irb(&irb.hash, &ctx);
  }
}

static  __INLINE BOOL __MTLK_IFUNC
_mtlk_irb_remove_owner_nodes (mtlk_handle_t owner)
{
  mtlk_mhash_enum_t        ctx;
  MTLK_MHASH_ENTRY_T(irb) *h;

  h = mtlk_mhash_enum_first_irb(&irb.hash, &ctx);
  while (h) {
    struct mtlk_irb_evt_handler *info = 
      MTLK_CONTAINER_OF(h, struct mtlk_irb_evt_handler, hentry);
    if (info->owner == owner) {
      mtlk_mhash_remove_irb(&irb.hash, &info->hentry);
      mtlk_osal_mem_free(info);
      break;
    }
    h = mtlk_mhash_enum_next_irb(&irb.hash, &ctx);
  }

  return (h != NULL);
}

int __MTLK_IFUNC
mtlk_irb_init (void)
{
  int res = MTLK_ERR_UNKNOWN;

  memset(&irb, 0, sizeof(irb));

  res = mtlk_osal_mutex_init(&irb.lock);
  if (res != MTLK_ERR_OK) {
    ERROR("Can't initialize lock (err=%d)", res);
    goto end;
  }
  irb.state |= MTLK_IRBS_LOCK_INITED;

  res = mtlk_mhash_init_irb(&irb.hash, IRB_HASH_MAX_IDX);
  if (res != MTLK_ERR_OK) {
    ERROR("Can't initialize hash (err=%d)", res);
    goto end;
  }
  irb.state |= MTLK_IRBS_HASH_INITED;

  mtlk_osal_atomic_set(&irb.owner_id, 0);

  irb_inited = 1;

  res = MTLK_ERR_OK;

end:
  if (res != MTLK_ERR_OK) {
    mtlk_irb_cleanup();
  }

  return res;
}

mtlk_handle_t __MTLK_IFUNC
mtlk_irb_register (const mtlk_guid_t     *evts,
                   uint32                 nof_evts,
                   mtlk_irb_evt_handler_f handler,
                   mtlk_handle_t          context)
{
  int    err   = MTLK_ERR_UNKNOWN;
  uint32 owner;

  MTLK_ASSERT(irb_inited);

  owner = mtlk_osal_atomic_inc(&irb.owner_id);

  MTLK_ASSERT(handler != 0);

  while (nof_evts) {
    err = _mtlk_irb_add_owner_node(owner,
                                   &evts[0],
                                   handler,
                                   context);
    if (err != MTLK_ERR_OK) {
      goto end;
    }

    evts++;
    nof_evts--;
  }

  err = MTLK_ERR_OK;

end:
  if (err != MTLK_ERR_OK) {
    mtlk_irb_unregister(owner);
    owner = 0;
  }

  return HANDLE_T(owner);
}

void __MTLK_IFUNC
mtlk_irb_unregister (mtlk_handle_t owner)
{
  MTLK_ASSERT(irb_inited);

  if (owner) {
    mtlk_osal_mutex_acquire(&irb.lock);
    _mtlk_irb_remove_owner_nodes(owner);
    mtlk_osal_mutex_release(&irb.lock);
  }
}

void __MTLK_IFUNC
mtlk_irb_on_evt (const mtlk_guid_t *evt,
                 void              *buffer,
                 uint32            *size)
{
  mtlk_mhash_find_t        ctx;
  MTLK_MHASH_ENTRY_T(irb) *h;

  mtlk_osal_mutex_acquire(&irb.lock);
  h = mtlk_mhash_find_first_irb(&irb.hash, evt, &ctx);
  while (h) {
    struct mtlk_irb_evt_handler *info = 
      MTLK_CONTAINER_OF(h, struct mtlk_irb_evt_handler, hentry);
    info->func(info->context, evt, buffer, size);
    h = mtlk_mhash_find_next_irb(&irb.hash, evt, &ctx);
  }
  mtlk_osal_mutex_release(&irb.lock);
}

void __MTLK_IFUNC
mtlk_irb_cleanup (void)
{
  if (irb.state & MTLK_IRBS_HASH_INITED) {  
    _mtlk_irb_remove_all_nodes();  
    mtlk_mhash_cleanup_irb(&irb.hash);
  }

  if (irb.state & MTLK_IRBS_LOCK_INITED) {
    mtlk_osal_mutex_cleanup(&irb.lock);
  }

  irb.state  = 0;
  irb_inited = 0;
}

