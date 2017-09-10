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
 * Utilities.
 *
 * Originally written by Andrey Fidrya
 *
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#include <linux/vmalloc.h>
#include <linux/ctype.h>

#ifdef MTCFG_SILENT
#undef MTCFG_DEBUG
#endif

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

#ifndef MTCFG_SILENT
#define USE_TIMESTAMP
#endif

#ifdef USE_TIMESTAMP
#define TIMESTAMP printk("[%010u] ", jiffies_to_msecs(jiffies))
#else
#define TIMESTAMP
#endif

#ifdef MTCFG_SILENT
#define MSG(level, fmt, ...)
#else
#define MSG(level, fmt, ...)                                    \
  do {                                                          \
    TIMESTAMP;                                                  \
    printk(level "mtlk (%s:%d): " fmt "\n",                     \
           __FUNCTION__, __LINE__ , ## __VA_ARGS__);            \
  } while (0)
#endif
#define WARNING(fmt, ...) MSG(KERN_WARNING, "WARNING: " fmt, ## __VA_ARGS__)
#define ERROR(fmt, ...) MSG(KERN_ERR, "ERROR: " fmt , ## __VA_ARGS__)

#define SIZEOF(a) ((unsigned long)sizeof(a)/sizeof((a)[0]))
#define INC_WRAP_IDX(i,s) do { (i)++; if ((i) == (s)) (i) = 0; } while (0)

#ifdef MTCFG_DEBUG
#define ASSERT(expr)                                            \
  do {                                                          \
    if (unlikely(!(expr))) {                                    \
          ERROR("Assertion failed! %s", (#expr));               \
          BUG();                                                \
      }                                                         \
  } while (0)
#else
#define ASSERT(expr)
#endif

#ifdef MTCFG_SILENT
#define LOG(log_level, fmt, ...)
#else
#define LOG(log_level, fmt, ...)                                \
  do {                                                          \
    TIMESTAMP;                                                  \
    printk("mtlk" #log_level " (%s:%d): " fmt "\n",             \
            __FUNCTION__, __LINE__ , ## __VA_ARGS__);           \
  } while (0)
#endif

#ifdef MTCFG_SILENT
#define INFO(fmt, ...)
#else
#define INFO(fmt, ...)                                          \
  do {                                                          \
    TIMESTAMP;                                                  \
    printk(KERN_INFO "mtlk: " fmt "\n", ## __VA_ARGS__);        \
  } while (0)
#endif

/* List of existing dlevel variables    */
extern int debug;       /* Common       */
#ifdef MTCFG_RF_MANAGEMENT_MTLK
extern int debug_rf_mgmt; /* RF MGMT module */
#endif

#ifndef DLEVEL_VAR
#define DLEVEL_VAR debug
#endif

#ifdef MTCFG_DEBUG
#define MAX_DLEVEL 2
#else
#define MAX_DLEVEL 1
#endif

#if (MAX_DLEVEL >= 0)
# define LOG0(fmt, ...) do if (DLEVEL_VAR >= 0) LOG(0, fmt, ## __VA_ARGS__); while (0)
# define DUMP0(buf, len, fmt, ...) if (DLEVEL_VAR >= 0) \
      {LOG(0, fmt, ## __VA_ARGS__); mtlk_aux_print_hex(buf, len);}
#else
# define LOG0(args...)
# define DUMP0(args...)
#endif

#if (MAX_DLEVEL >= 1)
# define LOG1(fmt, ...) do if (DLEVEL_VAR >= 1) LOG(1, fmt, ## __VA_ARGS__); while (0)
# define DUMP1(buf, len, fmt, ...) if (DLEVEL_VAR >= 1) \
      {LOG(1, fmt, ## __VA_ARGS__); mtlk_aux_print_hex(buf, len);}
#else
# define LOG1(args...)
# define DUMP1(args...)
#endif

#if (MAX_DLEVEL >= 2)
# define LOG2(fmt, ...) do if (DLEVEL_VAR >= 2) LOG(2, fmt, ## __VA_ARGS__); while (0)
# define DUMP2(buf, len, fmt, ...) if (DLEVEL_VAR >= 2) \
      {LOG(2, fmt, ## __VA_ARGS__); mtlk_aux_print_hex(buf, len);}
#else
# define LOG2(args...)
# define DUMP2(args...)
#endif

#if (MAX_DLEVEL >= 3)
# define LOG3(fmt, ...) do if (DLEVEL_VAR >= 3) LOG(3, fmt, ## __VA_ARGS__); while (0)
# define DUMP3(buf, len, fmt, ...) if (DLEVEL_VAR >= 3) \
      {LOG(3, fmt, ## __VA_ARGS__); mtlk_aux_print_hex(buf, len);}
#else
# define LOG3(args...)
# define DUMP3(args...)
#endif

#if (MAX_DLEVEL >= 4)
# define LOG4(fmt, ...) do if (DLEVEL_VAR >= 4) LOG(4, fmt, ## __VA_ARGS__); while (0)
# define DUMP4(buf, len, fmt, ...) if (DLEVEL_VAR >= 4) \
      {LOG(4, fmt, ## __VA_ARGS__); mtlk_aux_print_hex(buf, len);}
#else
# define LOG4(args...)
# define DUMP4(args...)
#endif

#if (MAX_DLEVEL >= 5)
# define LOG5(fmt, ...) do if (DLEVEL_VAR >= 5) LOG(5, fmt, ## __VA_ARGS__); while (0)
# define DUMP5(buf, len, fmt, ...) if (DLEVEL_VAR >= 5) \
      {LOG(5, fmt, ## __VA_ARGS__); mtlk_aux_print_hex(buf, len);}
#else
# define LOG5(args...)
# define DUMP5(args...)
#endif

#if (MAX_DLEVEL >= 6)
# define LOG6(fmt, ...) do if (DLEVEL_VAR >= 6) LOG(6, fmt, ## __VA_ARGS__); while (0)
# define DUMP6(buf, len, fmt, ...) if (DLEVEL_VAR >= 6) \
      {LOG(6, fmt, ## __VA_ARGS__); mtlk_aux_print_hex(buf, len);}
#else
# define LOG6(args...)
# define DUMP6(args...)
#endif

#if (MAX_DLEVEL >= 7)
# define LOG7(fmt, ...) do if (DLEVEL_VAR >= 7) LOG(7, fmt, ## __VA_ARGS__); while (0)
# define DUMP7(buf, len, fmt, ...) if (DLEVEL_VAR >= 7) \
      {LOG(7, fmt, ## __VA_ARGS__); mtlk_aux_print_hex(buf, len);}
#else
# define LOG7(args...)
# define DUMP7(args...)
#endif

#if (MAX_DLEVEL >= 8)
# define LOG8(fmt, ...) do if (DLEVEL_VAR >= 8) LOG(8, fmt, ## __VA_ARGS__); while (0)
# define DUMP8(buf, len, fmt, ...) if (DLEVEL_VAR >= 8) \
      {LOG(8, fmt, ## __VA_ARGS__); mtlk_aux_print_hex(buf, len);}
#else
# define LOG8(args...)
# define DUMP8(args...)
#endif

#if (MAX_DLEVEL >= 9)
# define LOG9(fmt, ...) do if (DLEVEL_VAR >= 9) LOG(9, fmt, ## __VA_ARGS__); while (0)
# define DUMP9(buf, len, fmt, ...) if (DLEVEL_VAR >= 9) \
      {LOG(9, fmt, ## __VA_ARGS__); mtlk_aux_print_hex(buf, len);}
#else
# define LOG9(args...)
# define DUMP9(args...)
#endif

#ifndef __user
#define __user
#endif

// These are defined as macroses for __LINE__ to work properly:
#define ERROR_OUT_OF_MEMORY() \
  ELOG("Out of memory")
#define ERROR_CAT_UNSUPPORTED(category) \
  ELOG("Unsupported data category (%u) requested", category)
#define ERROR_IDX_OUT_OF_BOUNDS(category, index) \
  ELOG("Index out of bounds (category %u, index %u)", category, index)
#define ERROR_DRV_STATS_ITER() \
  ELOG("Error while iterating driver statistics")
#define WARN_STRING_TRUNCATED(datalen, category, index) \
  WLOG("Buffer size (%u) too small: string was truncated (category %u, index %u)", \
  datalen, category, index)

static inline void *
vm_allocator (size_t size)
{
  return vmalloc_tag(size, MTLK_MEM_TAG_DEBUG_DATA);
}

static inline void
vm_deallocator (void *ptr)
{
  vfree_tag(ptr);
}

static inline int
atohex(char c)
{
  if (isdigit(c))
    return c - '0';
  else
    return tolower(c)-'a'+10;
}

int copy_str_to_userspace(const char* str, size_t str_len,
  void __user *userspace_buffer, int *at, size_t *space_left);
int copy_zstr_to_userspace(const char* str, 
  void __user *userspace_buffer, int *at, size_t *space_left);
int copy_zstr_to_userspace_fmt(char *buf, size_t max_buf_len,
  void __user *userspace_buffer, int *at, size_t *space_left,
  const char *fmt, ...)
  __attribute__ ((__format__ (__printf__, 6, 7)));

size_t str_count (const char *str, char c);
void *mtlk_utils_memchr(const void *s, int c, size_t n);

#endif // _UTILS_H_

