/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"
#include "mem_leak.h"
#include "mtlklist.h"
#include "mtlk_objpool.h"

#ifdef MTCFG_ENABLE_OBJPOOL

MTLK_DECLARE_OBJPOOL(g_objpool);

/********************************************************************************
 * Private memory leaks debug releated stuff
 ********************************************************************************/
struct mem_obj
{
  MTLK_DECLARE_OBJECT_HEADER(obj_hdr);
  uint32 size;
  char   buffer[1];
};

#define MEM_OBJ_HDR_SIZE ((uint32)MTLK_OFFSET_OF(struct mem_obj, buffer))

#define FREED_MEM_FILL_CHAR (0xC)

static const uint32  FRONT_GUARD = 0xF0F0F0F0;
static const uint32  BACK_GUARD  = 0xBABABABA;

#define GET_GUARDED_BY_MEM(mem) (mem->buffer + sizeof(FRONT_GUARD))
#define GET_MEM_BY_GUARDED(buf) MTLK_CONTAINER_OF(((char*)buf) - sizeof(FRONT_GUARD), struct mem_obj, buffer[0])

#define GET_FGUARD_POS(mem)     (mem->buffer)
#define GET_BGUARD_POS(mem)     (mem->buffer + sizeof(FRONT_GUARD) + mem->size)

static void 
guards_set (struct mem_obj *mem)
{
  memcpy(GET_FGUARD_POS(mem), &FRONT_GUARD, sizeof(FRONT_GUARD));
  memcpy(GET_BGUARD_POS(mem), &BACK_GUARD, sizeof(BACK_GUARD));
}

static void 
guards_check (struct mem_obj *mem)
{
  if (memcmp(GET_FGUARD_POS(mem), &FRONT_GUARD, sizeof(FRONT_GUARD)))
  {
    ERROR("FGUARD corruption (file: %s:%u, ptr: %p, size %u)",
          mtlk_objpool_get_creator_file_name(&g_objpool, &mem->obj_hdr),
          mtlk_objpool_get_creator_file_line(&g_objpool, &mem->obj_hdr),
          GET_GUARDED_BY_MEM(mem), 
          mem->size);
    MTLK_ASSERT(0);
  }

  if (memcmp(GET_BGUARD_POS(mem), &BACK_GUARD, sizeof(BACK_GUARD)))
  {
    ERROR("BGUARD corruption (file: %s:%u, ptr: %p, size %u)",
          mtlk_objpool_get_creator_file_name(&g_objpool, &mem->obj_hdr),
          mtlk_objpool_get_creator_file_line(&g_objpool, &mem->obj_hdr),
          GET_GUARDED_BY_MEM(mem), 
          mem->size);
    MTLK_ASSERT(0);
  }
}

uint32 __MTLK_IFUNC
mem_leak_get_full_allocation_size (uint32 size)
{
  /* mem_dbg structure + requested size + guards */
  return (uint32)MEM_OBJ_HDR_SIZE + size + sizeof(FRONT_GUARD) + sizeof(BACK_GUARD);
}

void * __MTLK_IFUNC
mem_leak_handle_allocated_buffer (void *mem_dbg_buffer, uint32 size, 
                                  const char* file_name, uint32 file_line)
{
  struct mem_obj *mem = (struct mem_obj *)mem_dbg_buffer;

  if (!mem) {
    return NULL;
  }

  mem->size = size;

  mtlk_objpool_add_object_ex(&g_objpool, &mem->obj_hdr, MTLK_MEMORY_OBJ,
                             file_name, file_line, HANDLE_T(mem->size));
  guards_set(mem);

  LOG5("%p (%p %d)", GET_GUARDED_BY_MEM(mem), mem, mem->size);

  return GET_GUARDED_BY_MEM(mem);
}

void * __MTLK_IFUNC
mem_leak_handle_buffer_to_free (void *buffer)
{
  struct mem_obj *mem = GET_MEM_BY_GUARDED(buffer);

  LOG5("%p (%p %d)", buffer, mem, mem->size);

  mtlk_objpool_remove_object_ex(&g_objpool, &mem->obj_hdr, MTLK_MEMORY_OBJ, HANDLE_T(mem->size));
  guards_check(mem);
  memset(mem, FREED_MEM_FILL_CHAR, MEM_OBJ_HDR_SIZE + mem->size + sizeof(FRONT_GUARD) + sizeof(BACK_GUARD));

  return mem;
}

uint32 __MTLK_IFUNC
mem_leak_dbg_get_allocated_size (void)
{
  return mtlk_objpool_get_memory_allocated(&g_objpool);
}

struct alloc_info
{
  struct mem_leak_dbg_ainfo data;
  mtlk_slist_entry_t        lentry;
};

static BOOL __MTLK_IFUNC
_mem_leak_collect_ainfo_clb (mtlk_objpool_t* objpool, 
                             mtlk_objheader_t** object,
                             mtlk_handle_t context)
{
  mtlk_slist_t       *alloc_info_list;
  struct mem_obj     *mem;
  const char         *fname;
  uint32              fline;
  mtlk_slist_entry_t *entry, *prev_entry;
  struct alloc_info  *info = NULL;

  alloc_info_list = HANDLE_T_PTR(mtlk_slist_t, context);
  mem             = MTLK_CONTAINER_OF(object, struct mem_obj, obj_hdr);
  fname           = mtlk_objpool_get_creator_file_name(objpool, object);
  fline           = mtlk_objpool_get_creator_file_line(objpool, object);

  mtlk_slist_foreach(alloc_info_list, entry, prev_entry) {
    info = MTLK_CONTAINER_OF(entry, struct alloc_info, lentry);

    if (fline == info->data.fline && 
        !strncmp(fname, info->data.fname, sizeof(info->data.fname) - 1)) {
      /* entry found */
      break;
    }

    info = NULL;
  }

  if (!info) {
    info = (struct alloc_info *)mtlk_osal_mem_alloc_objpool(sizeof(*info), 0);
    if (!info) {
      ERROR("Can't allocate struct alloc_info! Abotred!");
      return FALSE;
    }
    memset(info, 0, sizeof(*info));
      
    strncpy(info->data.fname, fname, sizeof(info->data.fname) - 1);
    info->data.fline = fline;

    mtlk_slist_push(alloc_info_list, &info->lentry);
  }

  ++info->data.count;
  info->data.size += mem->size;

  return TRUE;
}

void __MTLK_IFUNC
mem_leak_dbg_enum_allocators_info (mem_leak_dbg_enum_f enum_func,
                                   mtlk_handle_t       enum_ctx)
{
  mtlk_slist_t        list, rlist /* ranked */;
  mtlk_slist_entry_t *le;
  
  mtlk_slist_init(&list);

  mtlk_objpool_enum_by_type(&g_objpool, 
                            MTLK_MEMORY_OBJ,
                            _mem_leak_collect_ainfo_clb, 
                            HANDLE_T(&list));

  mtlk_slist_init(&rlist);

  while ((le = mtlk_slist_pop(&list)) != NULL) {
    struct alloc_info *info = 
      MTLK_CONTAINER_OF(le, struct alloc_info, lentry);
    mtlk_slist_entry_t *entry, *prev;
    
    mtlk_slist_foreach(&rlist, entry, prev) {
      struct alloc_info *rinfo = 
        MTLK_CONTAINER_OF(entry, struct alloc_info, lentry);
      if (rinfo->data.size < info->data.size) {
        break;
      }
    }
    
    mtlk_slist_insert_next(&rlist, prev, &info->lentry);
  }

  while ((le = mtlk_slist_pop(&rlist)) != NULL) {
    struct alloc_info *info = 
      MTLK_CONTAINER_OF(le, struct alloc_info, lentry);

    enum_func(enum_ctx, &info->data);

    mtlk_osal_mem_free_objpool(info);
  }

  mtlk_slist_cleanup(&rlist);
  mtlk_slist_cleanup(&list);
}

struct mem_leak_dbg_printf_ctx
{
  mem_leak_dbg_printf_f printf_func;
  mtlk_handle_t         printf_ctx;
  uint32                total_size;
  uint32                total_count;
};

static void __MTLK_IFUNC
_mem_leak_dbg_printf_allocator_clb (mtlk_handle_t                    _ctx,
                                    const struct mem_leak_dbg_ainfo *ainfo)
{
  struct mem_leak_dbg_printf_ctx *ctx = 
    HANDLE_T_PTR(struct mem_leak_dbg_printf_ctx, _ctx);
  const char *fname = strrchr(ainfo->fname, '/');

  if (fname) {  /* skip Linux path and last '/' */
    fname += 1;
  }
  else {
    fname = ainfo->fname;
  }

  ctx->printf_func(ctx->printf_ctx, "| %7u | %3u | %4u | %s",
                   ainfo->size, ainfo->count, ainfo->fline, fname);

  ctx->total_size  += ainfo->size;
  ctx->total_count += ainfo->count;
}

void __MTLK_IFUNC
mem_leak_dbg_print_allocators_info (mem_leak_dbg_printf_f printf_func,
                                    mtlk_handle_t         printf_ctx)
{
  struct mem_leak_dbg_printf_ctx ctx;

  MTLK_ASSERT(printf_func != NULL);

  ctx.printf_func = printf_func;
  ctx.printf_ctx  = printf_ctx;
  ctx.total_count = 0;
  ctx.total_size  = 0;

  printf_func(printf_ctx, "Allocations dump:");
  printf_func(printf_ctx, "---------------------------------------------");
  printf_func(printf_ctx, "|  size   | cnt | line | file");
  printf_func(printf_ctx, "---------------------------------------------");
  mem_leak_dbg_enum_allocators_info(_mem_leak_dbg_printf_allocator_clb, 
                                    HANDLE_T(&ctx));
  printf_func(printf_ctx, "=============================================");
  printf_func(printf_ctx, " Total: %u allocations = %u bytes",
              ctx.total_count, ctx.total_size);
  printf_func(printf_ctx, "=============================================");
}

#endif /* MTCFG_ENABLE_OBJPOOL */
