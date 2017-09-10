/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __MEM_LEAK_H__
#define __MEM_LEAK_H__

/***************************************************************************
 * Memory leak/overwrite control API. To add your allocation to the memory 
 * leak/overwrite control system you must create your allocator/de-allocator
 * routines like:
 * 
 * void *my_malloc (int size, ...)
 * {
 *   void *buf = <system_allocator>(mem_leak_get_full_allocation_size(size), ...);
 *   return mem_leak_handle_allocated_buffer(buf, size, file_name, line);
 * }
 *
 * void my_free (void *buffer)
 * {
 *   void *buf = mem_leak_handle_buffer_to_free(buffer);
 *   <system_deallocator>(buf);
 * }
 *
 ***************************************************************************/
void * __MTLK_IFUNC
mem_leak_handle_allocated_buffer(void       *mem_leak_buffer, 
                                 uint32      size, 
                                 const char *file_name, 
                                 uint32      file_line);
void * __MTLK_IFUNC
mem_leak_handle_buffer_to_free(void *buffer);
uint32 __MTLK_IFUNC
mem_leak_get_full_allocation_size(uint32 size);


/* DEBUG abilities */
uint32 __MTLK_IFUNC
mem_leak_dbg_get_allocated_size(void);

struct mem_leak_dbg_ainfo /* allocator info */
{
  char   fname[256];
  uint32 fline;
  uint32 count;
  uint32 size;
};

typedef void (__MTLK_IFUNC * mem_leak_dbg_enum_f)(mtlk_handle_t                    ctx,
                                                  const struct mem_leak_dbg_ainfo *ainfo);

void __MTLK_IFUNC
mem_leak_dbg_enum_allocators_info(mem_leak_dbg_enum_f enum_func,
                                  mtlk_handle_t       enum_ctx);

typedef int (__MTLK_IFUNC *mem_leak_dbg_printf_f)(mtlk_handle_t printf_ctx,
                                                  const char   *format,
                                                  ...);

void __MTLK_IFUNC
mem_leak_dbg_print_allocators_info(mem_leak_dbg_printf_f printf_func,
                                   mtlk_handle_t         printf_ctx);

#endif

