/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __UTILS_H__
#define __UTILS_H__

#include <sys/time.h>
#include <assert.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)  (sizeof(a)/sizeof((a)[0]))
#endif

#ifdef MTCFG_DEBUG
#define ASSERT(expr)                                            \
  do {                                                          \
    if (!(expr)) {                                              \
          ERROR("Assertion failed! %s", (#expr));               \
          assert(0);                                            \
      }                                                         \
  } while (0)
#else
#define ASSERT(expr)
#endif

typedef enum _mtlk_osdep_level {
  MTLK_OSLOG_ERR,
  MTLK_OSLOG_WARN,
  MTLK_OSLOG_INFO
} mtlk_osdep_level_e;

int  __MTLK_IFUNC _mtlk_osdep_log_init(const char *app_name);
void __MTLK_IFUNC _mtlk_osdep_log_cleanup(void);
int  __MTLK_IFUNC _mtlk_osdep_log_enable_stderr(mtlk_osdep_level_e level,
                                                BOOL               value);
void __MTLK_IFUNC _mtlk_osdep_log(mtlk_osdep_level_e level,
                                  const char        *func,
                                  int                line,
                                  const char        *fmt,
                                  ...);

#define WARNING(fmt, ...)                                               \
  _mtlk_osdep_log(MTLK_OSLOG_WARN, __FUNCTION__, __LINE__, (fmt), ## __VA_ARGS__)

#define ERROR(fmt, ...)                                                 \
  _mtlk_osdep_log(MTLK_OSLOG_ERR, __FUNCTION__, __LINE__, (fmt), ## __VA_ARGS__)

#define INFO(fmt, ...)                                                  \
  _mtlk_osdep_log(MTLK_OSLOG_INFO, __FUNCTION__, __LINE__, (fmt), ## __VA_ARGS__)

#define _LOGx(x, fmt, ...)                                              \
  do {                                                                  \
    if (DLEVEL_VAR >= (x)) {                                            \
      _mtlk_osdep_log(MTLK_OSLOG_INFO, __FUNCTION__, __LINE__,          \
                       (fmt), ## __VA_ARGS__);                          \
    }                                                                   \
  } while (0)

#ifndef DLEVEL_VAR
#define DLEVEL_VAR debug
#endif

extern int DLEVEL_VAR;

#ifdef MTCFG_DEBUG
#define MAX_DLEVEL 5
#else
#define MAX_DLEVEL 1
#endif

#if (MAX_DLEVEL >= 0)
#define LOG0(fmt, ...) _LOGx(0, fmt, ## __VA_ARGS__)
#else
#define LOG0(fmt, ...)
#endif

#if (MAX_DLEVEL >= 1)
#define LOG1(fmt, ...) _LOGx(1, fmt, ## __VA_ARGS__)
#else
#define LOG1(fmt, ...)
#endif

#if (MAX_DLEVEL >= 2)
#define LOG2(fmt, ...) _LOGx(2, fmt, ## __VA_ARGS__)
#else
#define LOG2(fmt, ...)
#endif

#if (MAX_DLEVEL >= 3)
#define LOG3(fmt, ...) _LOGx(3, fmt, ## __VA_ARGS__)
#else
#define LOG3(fmt, ...)
#endif

#if (MAX_DLEVEL >= 4)
#define LOG4(fmt, ...) _LOGx(4, fmt, ## __VA_ARGS__)
#else
#define LOG4(fmt, ...)
#endif

#if (MAX_DLEVEL >= 5)
#define LOG5(fmt, ...) _LOGx(5, fmt, ## __VA_ARGS__)
#else
#define LOG5(fmt, ...)
#endif

#if (MAX_DLEVEL >= 6)
#define LOG6(fmt, ...) _LOGx(6, fmt, ## __VA_ARGS__)
#else
#define LOG6(fmt, ...)
#endif

#if (MAX_DLEVEL >= 7)
#define LOG7(fmt, ...) _LOGx(7, fmt, ## __VA_ARGS__)
#else
#define LOG7(fmt, ...)
#endif

#if (MAX_DLEVEL >= 8)
#define LOG8(fmt, ...) _LOGx(8, fmt, ## __VA_ARGS__)
#else
#define LOG8(fmt, ...)
#endif

#if (MAX_DLEVEL >= 9)
#define LOG9(fmt, ...) _LOGx(9, fmt, ## __VA_ARGS__)
#else
#define LOG9(fmt, ...)
#endif

static inline unsigned long
timestamp(void)
{
  struct timeval ts;
  if (0 != gettimeofday(&ts, NULL))
    return 0;
  return ts.tv_usec + (ts.tv_sec * 1000000);
}

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

#endif /* __UTILS_H__ */

