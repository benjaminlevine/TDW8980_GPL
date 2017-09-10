/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *
 * Written by: Andriy Fidrya
 *
 */
#ifndef __LOG_OSDEP_H__
#define __LOG_OSDEP_H__

#ifndef MTCFG_SILENT
uint8 __MTLK_IFUNC log_osdep_get_level (uint8 gid);
void __MTLK_IFUNC log_osdep_set_level (uint8 gid, uint8 level);
void __MTLK_IFUNC log_osdep_reset_levels (uint8 level);
void __MTLK_IFUNC log_osdep_init(void);
void __MTLK_IFUNC log_osdep_cleanup(void);
void __MTLK_IFUNC log_osdep_do(const char *fname, 
                               int         line_no, 
                               const char *level, 
                               const char *fmt, ...);
#else
static __INLINE uint8 log_osdep_get_level (uint8 gid) 
{ 
  MTLK_UNREFERENCED_PARAM(gid); 
  return 0;
}
static __INLINE void log_osdep_set_level (uint8 gid, uint8 level)
{
  MTLK_UNREFERENCED_PARAM(gid);
  MTLK_UNREFERENCED_PARAM(level);
}
static __INLINE void log_osdep_reset_levels (uint8 level)
{
  MTLK_UNREFERENCED_PARAM(level);
}
#define log_osdep_init()
#define log_osdep_cleanup()
#define log_osdep_do(...)
#endif

#define CILOG(fname, line_no, log_level, fmt, ...)               \
  log_osdep_do(fname, line_no, #log_level, fmt, ## __VA_ARGS__)
#define CINFO(fname, line_no, fmt, ...)                         \
  log_osdep_do("", 0, "", fmt, ## __VA_ARGS__)
#define CERROR(fname, line_no, fmt, ...)                        \
  log_osdep_do(fname, line_no, KERN_ERR, fmt, ## __VA_ARGS__)
#define CWARNING(fname, line_no, fmt, ...)                      \
  log_osdep_do(fname, line_no, KERN_WARNING, fmt, ## __VA_ARGS__)

#ifndef RTLOG_FLAGS
#if (defined MTCFG_RT_LOGGER_INLINES) || (defined MTCFG_RT_LOGGER_FUNCTIONS)
#define RTLOG_FLAGS (RTLF_REMOTE_ENABLED | RTLF_CONSOLE_ENABLED)
#else
#define RTLOG_FLAGS (RTLF_CONSOLE_ENABLED)
#endif
#endif

#ifndef RTLOG_MAX_DLEVEL
#if defined(MTCFG_SILENT)
#define RTLOG_MAX_DLEVEL (RTLOG_ERROR_DLEVEL - 1)
#elif defined(MTCFG_DEBUG)
#define RTLOG_MAX_DLEVEL 9
#else
#define RTLOG_MAX_DLEVEL 1
#endif
#endif

#if defined (MTCFG_RT_LOGGER_OFF)
/* Console only - use inlines */
#define __MTLK_FLOG static __INLINE
#elif defined (MTCFG_RT_LOGGER_INLINES)
/* Console & Remote - use inlines */
#define __MTLK_FLOG static __INLINE
#elif defined(MTCFG_RT_LOGGER_FUNCTIONS)
/* Console & Remote - use function calls */
#define __MTLK_FLOG
#define RTLOG_USE_FCALLS
#else
#error Wrong RTLOGGER configuration!
#endif

#ifndef LOG_ORIGIN_ID
#error LOG_ORIGIN_ID must be defined!
#endif

#endif /* __LOG_OSDEP_H__ */
