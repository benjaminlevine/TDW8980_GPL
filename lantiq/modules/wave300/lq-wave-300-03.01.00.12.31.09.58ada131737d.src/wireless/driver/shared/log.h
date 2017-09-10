/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 *
 * Copyright (c) 2008 Metalink Broadband (Israel)
 *
 * Written by: Andriy Fidrya
 *
 */

#ifndef __LOG_H__
#define __LOG_H__

#include "mtlklist.h"

/* RTLOGGER compilation time options (RTLOG_FLAGS) */
#define RTLF_REMOTE_ENABLED  0x00000001
#define RTLF_CONSOLE_ENABLED 0x00000002

/* RTLOGGER special logs compilation control Debug Levels 
   Used to enable/disable logs by debug level in compile time 
   (RTLOG_MAX_DLEVEL)
*/
#define RTLOG_ERROR_DLEVEL   -2
#define RTLOG_WARNING_DLEVEL -1

/* Well-known GIDs */
#define GID_ERROR   0
#define GID_WARNING 1

#include "log_osdep.h"

#ifndef RTLOG_FLAGS /* combination of RTLF_... flags */
#error RTLOG_FLAGS must be defined!
#endif

#ifndef RTLOG_MAX_DLEVEL
#error RTLOG_MAX_DLEVEL must be defined!
#endif

#ifdef FMT_NO_ATTR_CHK
#define __attribute__(x)
#endif

#define LOG_TARGET_CONSOLE (1 << 0)
#define LOG_TARGET_REMOTE  (1 << 1)

#if (defined MTCFG_RT_LOGGER_INLINES) || (defined MTCFG_RT_LOGGER_FUNCTIONS)


// ---------------
// Data structures
// ---------------

#define MTLK_IDEFS_ON
#include "mtlkidefs.h"

typedef struct _mtlk_log_buf_entry_t
{
  mtlk_ldlist_entry_t entry; // List entry data
  uint32 refcount;           // Reference counter
  
  //  struct _mtlk_payload_desc_t *payload; // Allocated buffer
  struct sk_buff *buf;         // Allocated buffer
  uint32 data_size;          // Data size in buffer
  uint32 expired;            // Buffer swap timer expiration flag
} __MTLK_IDATA mtlk_log_buf_entry_t;

// --------------------
// Functions and macros
// --------------------
/*!
 * \fn      BOOL mtlk_log_check_version(uint16 major, uint16 minor)
 * \brief   Check logger version.
 *
 * Checks logger version of a component that wants to log using this log driver.
 *
 * \return TRUE if verion is supported, FALSE otherwise.
 */
extern BOOL mtlk_log_check_version(uint16 major, uint16 minor);

/*!
 * \fn      int mtlk_log_get_flags(int level, int oid, int gid, int sid)
 * \brief   Query log settings.
 *
 * Queries log settings for level, gid and sid combination.
 *
 * \return bitmask specifying log targets.
 *         Bitmask is 0 if logger is not initialized or no log targets
 *         are set.
 *         LOG_TARGET_CONSOLE - console logging requested.
 *         LOG_TARGET_REMOTE - remote logging requested.
 */
extern int mtlk_log_get_flags(int level, int oid, int gid, int sid);

/*!
 * \fn      mtlk_log_buf_entry_t *mtlk_log_new_pkt_reserve(uint32 pkt_size,
            uint8 **ppdata)
 * \brief   Reserve space for a new packet in active buffer.
 *
 * Returns a pointer to active buffer (as return value) and a pointer to
 * beginning of free space in this buffer (ppdata).
 *
 * Caller writes packet contents to ppdata, then releases the buffer by
 * calling mtlk_log_new_pkt_release.
 *
 * Caller must silently ignore errors; reporting is done by
 * mtlk_log_new_pkt_reserve function itself via CERROR.
 *
 * Implementation details:
 *  - If enough space is available in current active buffer, it's refcount
 *    is increased by 1 and size by pkt_size. Buffer is protected by refcount
 *    so it won't move to ready buffers pool before packet creation is
 *    complete.
 *  - If not enough space is available and active buffer's refcount is 0,
 *    sends active buffer to ready buffers pool and requests another free
 *    buffer.
 *  - If not enough space is available, but active buffer's refcount is
 *    greater than 0, requests another free buffer, then replaces pointer
 *    to active buffer with a pointer to the newly obtained buffer.
 *    Note: in this case old buffer will not be lost:
 *    mtlk_log_new_pkt_release will move it to ready buffers pool after
 *    refcount reaches zero.
 *
 * \return pointer to a buffer on success, ppdata points to beginning of
 *         data.
 * \return NULL if error has occured. Do not report this error to user.
 */
extern mtlk_log_buf_entry_t *mtlk_log_new_pkt_reserve(uint32 pkt_size,
    uint8 **ppdata);

/*!
 * \fn      void __MTLK_IFUNC mtlk_log_new_pkt_release(mtlk_log_buf_entry_t *buf)
 * \brief   Release a buffer captured with mtlk_log_new_pkt_reserve call.
 *
 * Implementation details:
 *  - Decreases buf's refcount. After this:
 *  - If refcount is zero and buf is active buffer, does nothing (this buffer
 *    still can accept data as active buffer).
 *  - If refcount is zero, but buf is not an active buffer, sends the buffer
 *    to ready buffers pool. Note that the buffer was already invalidated by
 *    mtlk_log_new_pkt_reserve.
 *  - If refcount is not zero, no more actions are taken.
 */
extern void mtlk_log_new_pkt_release(mtlk_log_buf_entry_t *buf);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#else /* (defined MTCFG_RT_LOGGER_INLINES) || (defined MTCFG_RT_LOGGER_FUNCTIONS) */

static int __INLINE
mtlk_log_get_flags (int level, int oid, int gid, int sid)
{
  return (level <= log_osdep_get_level(gid)) ? LOG_TARGET_CONSOLE : 0;
}

#endif /* (defined MTCFG_RT_LOGGER_INLINES) || (defined MTCFG_RT_LOGGER_FUNCTIONS) */

#include "logdefs.h"
#include "logmacros.h"

#endif /* __LOG_H__ */
