/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * Copyright (c) 2007-2008 Metalink Broadband (Israel)
 *
 * Utilities
 *
 * Written by: Andrey Fidrya
 *
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include "parser.h"

#include <stdio.h>
#include <linux/limits.h>

extern int debug;

unsigned long timestamp(void);
int get_dir(char *filename, char buf[PATH_MAX], char **pdirname);
int create_filename(char *dir, char *file, char buf[PATH_MAX + NAME_MAX],
    char *fullname);
int ends_with(char *str, char *ends_with);
int replace_filename(char *filename, char *new_filename,
    char buf[PATH_MAX + NAME_MAX]);
int alloc_buf_from_file(char **ppbuf, int *buf_size, char *filename);
int get_line(char *filename, char *buf, size_t buf_size, FILE *fl,
  int trim_crlf, int *peof);
int get_word(char **pp, char *buf, size_t buf_size);
void skip_spcrlf(char **pp);
int is_spcrlf(char c);
char *malloc_str_encode(char *str);
int str_decode(char *str);
int buf_append_char(char *buf, int buf_size, int *pbuf_at, char c);
int buf_append_buf(char *buf, int buf_size, int *pbuf_at,
    char *src, int src_size);
void str_to_upper(char *str);

#define ERR_UNKNOWN    -1
#define ERR_OVERFLOW   -2
#define ERR_NOENT      -3

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))
#endif

#define SIZEOF_MEMB(type, field) (sizeof(((type *)0)->field))

#define USE_TIMESTAMP

#ifdef USE_TIMESTAMP
#define TIMESTAMP fprintf(stderr, "[%010lu] ", timestamp())
#else
#define TIMESTAMP
#endif

#define MSG(fmt, ...)                                           \
  do {                                                          \
    TIMESTAMP;                                                  \
    fprintf(stderr, "logprep (%s:%d:%s): " fmt "\n",            \
      __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__);        \
  } while (0)

#define WARNING(fmt, ...)                                       \
  do {                                                          \
    if (debug >= 0) MSG("WARNING: " fmt, ## __VA_ARGS__);       \
  } while (0)

#define ERROR(fmt, ...)                                         \
  do {                                                          \
    if (debug >= 0) MSG("ERROR: " fmt , ## __VA_ARGS__);        \
  } while (0)

#define INFO(fmt, ...)                                          \
  do {                                                          \
    if (debug >= 0) MSG(fmt , ## __VA_ARGS__);                  \
  } while (0)

#define LOG(log_level, fmt, ...)                                \
  do {                                                          \
    if (log_level <= debug) MSG(fmt , ## __VA_ARGS__);          \
  } while (0)


#if 1
// Use GCC-style version to be compatible with VIM:
#define INFO_LC(fmt, ...) do {                            \
  if (debug >= 0) {                                       \
    fprintf(stderr, "%s:%d: warning: col %d: " fmt "\n",  \
      filename, line, col, ## __VA_ARGS__);               \
  }                                                       \
} while (0)

#define ERROR_LC(fmt, ...) do {                           \
  if (debug >= 0) {                                       \
    fprintf(stderr, "%s:%d: error: col %d: " fmt "\n",    \
      filename, line, col, ## __VA_ARGS__);               \
  }                                                       \
} while (0)
#else
#define INFO_LC(fmt, ...)  INFO("line %d, col %d: " fmt, line, col, ## __VA_ARGS__)
#define ERROR_LC(fmt, ...) ERROR("line %d, col %d: " fmt, line, col, ## __VA_ARGS__)
#endif

#ifdef MTCFG_DEBUG
#define ASSERT(expr)                                            \
  do {                                                          \
    if (!(expr)) {                                              \
          ERROR("Assertion failed! %s", (#expr));               \
      }                                                         \
  } while (0)
#else
#define ASSERT(expr)
#endif

#ifdef MTCFG_DEBUG
#define VERIFY(expr) ASSERT(expr)
#else
#define VERIFY(expr) ((void)(expr))
#endif

# define LOG0(fmt, ...) LOG(0, fmt, ## __VA_ARGS__)
# define LOG1(fmt, ...) LOG(1, fmt, ## __VA_ARGS__)
# define LOG2(fmt, ...) LOG(2, fmt, ## __VA_ARGS__)
# define LOG3(fmt, ...) LOG(3, fmt, ## __VA_ARGS__)
# define LOG4(fmt, ...) LOG(4, fmt, ## __VA_ARGS__)
# define LOG5(fmt, ...) LOG(5, fmt, ## __VA_ARGS__)
# define LOG6(fmt, ...) LOG(6, fmt, ## __VA_ARGS__)
# define LOG7(fmt, ...) LOG(7, fmt, ## __VA_ARGS__)
# define LOG8(fmt, ...) LOG(8, fmt, ## __VA_ARGS__)
# define LOG9(fmt, ...) LOG(9, fmt, ## __VA_ARGS__)

#define LIST_REMOVE(head, item, type, next)  \
  do {                                       \
    if ((item) == (head)) {                  \
      (head) = (item)->next;                 \
    } else {                                 \
      type *temp = (head);                   \
      while (temp && (temp->next != (item))) \
        temp = temp->next;                   \
        if (temp)                            \
          temp->next = (item)->next;         \
    }                                        \
  } while(0)

#define LIST_PUSH_FRONT(head, item, next)    \
  do {                                       \
    (item)->next = (head);                   \
    (head) = (item);                         \
  } while(0)

#endif // !__UTILS_H__

