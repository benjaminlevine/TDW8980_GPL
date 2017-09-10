/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __LOG_DEFS_H__
#define __LOG_DEFS_H__

#include "mtlkbfield.h"

#define MTLK_IDEFS_ON
#define MTLK_IDEFS_PACKING 1
#include "mtlkidefs.h"

#define RTLOGGER_VER_MAJOR 0
#define RTLOGGER_VER_MINOR 1

/****************************************************************************************
 * 'info' member is bitmask as following:
 *
 * BITS | 31 | 30 | 29 | 28 | 27 | 26 | 25 | 24 | 23 | 22 | 21 | 20 | 19 | 18 | 17 | 16 |
 *      |      OID     |       SID HIGH         | E2 |    GID                           |
 *
 * BITS | 15 | 14 | 13 | 12 | 11 | 10 | 09 | 08 | 07 | 06 | 05 | 04 | 03 | 02 | 01 | 00 |
 *      | E1 |      SID LOW      |  Data Length                                         |
 *
 * E1, E2 - endianess bits (E1 = 0, E2 = 1).
 *          These bits are used for Endianess detection during record parsing:
 *          if the E1 == 0 and E2 == 1 then the record source runs with the same
 *          endianess as parser, otherwise (E1 == 1 and E2 == 0) - the endianess
 *          WARNING: these bits CAN NOT be moved to other positions!
 *          of the parser is different from the record source's one.
 * OID    - record's Origin ID (for example, Driver, LMAC, UMAC etc.)
 * GID    - record's Group ID (for example, per component inside a origin)
 * SID    - record's String ID (particular log event ID). It is divided to 2 parts
 *          (high bits and low bits) but both these parts are representing single SID
 *          value.
 ****************************************************************************************/

#define MAX_OID (1 << 3)
#define MAX_GID (1 << 7)
#define MAX_SID (1 << 9)

#define LOG_INFO_OID   MTLK_BFIELD_INFO(29, 3)
#define LOG_INFO_SIDH  MTLK_BFIELD_INFO(24, 5)
#define LOG_INFO_E2    MTLK_BFIELD_INFO(23, 1)
#define LOG_INFO_GID   MTLK_BFIELD_INFO(16, 7)
#define LOG_INFO_E1    MTLK_BFIELD_INFO(15, 1)
#define LOG_INFO_SIDL  MTLK_BFIELD_INFO(11, 4)
#define LOG_INFO_DSIZE MTLK_BFIELD_INFO(0,  11)

#define LOG_MAKE_INFO(o, g, s, l)                              \
  MTLK_BFIELD_VALUE(LOG_INFO_OID,               (o), uint32) | \
  MTLK_BFIELD_VALUE(LOG_INFO_SIDH, ((s) & 0x000001F0) >> 4, uint32) | \
  MTLK_BFIELD_VALUE(LOG_INFO_E2,                  1, uint32) | \
  MTLK_BFIELD_VALUE(LOG_INFO_GID,               (g), uint32) | \
  MTLK_BFIELD_VALUE(LOG_INFO_E1,                  0, uint32) | \
  MTLK_BFIELD_VALUE(LOG_INFO_SIDL, (s) & 0x0000000F, uint32) | \
  MTLK_BFIELD_VALUE(LOG_INFO_DSIZE,             (l), uint32)

#define LOG_IS_CORRECT_INFO(info)               \
  (MTLK_BFIELD_GET(info, LOG_INFO_E2) != MTLK_BFIELD_GET(info, LOG_INFO_E1))

#define LOG_IS_STRAIGHT_ENDIAN(info)            \
  (MTLK_BFIELD_GET(info, LOG_INFO_E2) == 1)

#define LOG_IS_INVERSED_ENDIAN(info)            \
  (MTLK_BFIELD_GET(info, LOG_INFO_E1) == 1)

#define LOG_INFO_GET_OID(info)                  \
  MTLK_BFIELD_GET((info), LOG_INFO_OID)
#define LOG_INFO_GET_GID(info)                  \
  MTLK_BFIELD_GET((info), LOG_INFO_GID)
#define LOG_INFO_GET_SID(info)                  \
  ((MTLK_BFIELD_GET((info), LOG_INFO_SIDH) << 4) | MTLK_BFIELD_GET((info), LOG_INFO_SIDL))
#define LOG_INFO_GET_DSIZE(info)                \
  MTLK_BFIELD_GET((info), LOG_INFO_DSIZE)

#define ASSERT_OID_VALID(oid)                   \
  ASSERT(((oid) & LOG_INFO_GET_OID(-1)) == (oid))
#define ASSERT_GID_VALID(gid)                   \
  ASSERT(((gid) & LOG_INFO_GET_GID(-1)) == (gid))
#define ASSERT_SID_VALID(sid)                   \
  ASSERT(((sid) & LOG_INFO_GET_SID(-1)) == (sid))

typedef struct _mtlk_log_event_t
{
  uint32 info;
  uint32 timestamp;
} __MTLK_IDATA mtlk_log_event_t;

typedef struct _mtlk_log_event_data_t
{
  uint8 datatype;
} __MTLK_IDATA mtlk_log_event_data_t;

typedef struct _mtlk_log_lstring_t
{
  uint16 len;
} __MTLK_IDATA mtlk_log_lstring_t;

#define LOG_DT_LSTRING  0
#define LOG_DT_INT8     1
#define LOG_DT_INT32    2
#define LOG_DT_INT64    3
#define LOG_DT_MACADDR  4
#define LOG_DT_IP6ADDR  5

/* printf symbol for IP4 ADDR extension */
#define MTLK_IP4_FMT    'B'
/* printf symbol for IP6 ADDR extension */
#define MTLK_IP6_FMT    'K'
/* printf symbol for MAC ADDR extension */
#define MTLK_MAC_FMT    'Y'

typedef enum _mtlk_log_ctrl_id_e
{
  MTLK_LOG_CTRL_ID_VERINFO, 
  MTLK_LOG_CTRL_ID_LAST
} mtlk_log_ctrl_id_e;

typedef struct _mtlk_log_ctrl_hdr_t
{
  uint16 id;
  uint16 data_size;
} __MTLK_IDATA mtlk_log_ctrl_hdr_t;

typedef struct _mtlk_log_ctrl_ver_info_data_t
{
  uint16              major;
  uint16              minor;
} __MTLK_IDATA mtlk_log_ctrl_ver_info_data_t;

#define MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif
