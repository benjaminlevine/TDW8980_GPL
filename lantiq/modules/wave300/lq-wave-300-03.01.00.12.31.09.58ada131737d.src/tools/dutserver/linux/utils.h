/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>

extern int debug;

unsigned long timestamp();

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

#define SIZEOF_MEMB(type, field) (sizeof(((type *)0)->field))

#define USE_TIMESTAMP

#ifdef USE_TIMESTAMP
#define TIMESTAMP fprintf(stderr, "[%010lu] ", timestamp())
#else
#define TIMESTAMP
#endif

#define MSG(fmt, ...)                                             \
  do {                                                            \
    TIMESTAMP;                                                    \
    fprintf(stderr, "dutserver (%s:%d): " fmt "\n",               \
      __FUNCTION__, __LINE__, ## __VA_ARGS__);              \
    } while (0)

#define WARNING(fmt, ...) MSG("WARNING: " fmt, ## __VA_ARGS__)
#define ERROR(fmt, ...) MSG("ERROR: " fmt , ## __VA_ARGS__)

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

#define LOG(log_level, fmt, ...)                                  \
  do {                                                            \
    TIMESTAMP;                                                    \
    fprintf(stderr, "dutserver" #log_level " (%s:%d): " fmt "\n", \
      __FUNCTION__, __LINE__ , ## __VA_ARGS__);                   \
    } while (0)

#define INFO(fmt, ...)                                            \
  do {                                                            \
    TIMESTAMP;                                                    \
    fprintf(stderr, "dutserver: " fmt "\n", ## __VA_ARGS__);      \
  } while (0)

# define LOG0(fmt, ...) if (debug >= 0) LOG(0, fmt, ## __VA_ARGS__)
# define LOG1(fmt, ...) if (debug >= 1) LOG(1, fmt, ## __VA_ARGS__)
# define LOG2(fmt, ...) if (debug >= 2) LOG(2, fmt, ## __VA_ARGS__)
# define LOG3(fmt, ...) if (debug >= 3) LOG(3, fmt, ## __VA_ARGS__)
# define LOG4(fmt, ...) if (debug >= 4) LOG(4, fmt, ## __VA_ARGS__)
# define LOG5(fmt, ...) if (debug >= 5) LOG(5, fmt, ## __VA_ARGS__)
# define LOG6(fmt, ...) if (debug >= 6) LOG(6, fmt, ## __VA_ARGS__)
# define LOG7(fmt, ...) if (debug >= 7) LOG(7, fmt, ## __VA_ARGS__)
# define LOG8(fmt, ...) if (debug >= 8) LOG(8, fmt, ## __VA_ARGS__)
# define LOG9(fmt, ...) if (debug >= 9) LOG(9, fmt, ## __VA_ARGS__)

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

