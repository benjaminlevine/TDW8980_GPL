/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef _MTLKINC_H_
#define _MTLKINC_H_

#define __MTLK_IFUNC 
#define __INLINE     inline
#define __LIKELY     likely
#define __UNLIKELY   unlikely

#include <stdlib.h>

#ifdef MTCFG_DEBUG
#define MTLK_DEBUG
#endif

typedef enum _BOOL
{
  FALSE,
  TRUE
} BOOL;

#ifndef HANDLE_T_DEFINED
typedef unsigned long mtlk_handle_t;
#define HANDLE_T_DEFINED

#define HANDLE_T(x)       ((mtlk_handle_t)(x))
#define HANDLE_T_PTR(t,x) ((t*)(x))
#define HANDLE_T_INT(t,x) ((t)(x))
#endif

#include <arpa/inet.h> /* hton... */

# if __BYTE_ORDER == __BIG_ENDIAN

#include <byteswap.h>

/******************************************
 * MAC <=> Driver interface conversion
 *****************************************/
#define MAC_TO_HOST16(x)   bswap_16(x)
#define MAC_TO_HOST32(x)   bswap_32(x)

#define HOST_TO_MAC16(x)   bswap_16(x)
#define HOST_TO_MAC32(x)   bswap_32(x)
/******************************************/

/******************************************
 * 802.11 conversion
 *****************************************/
#define WLAN_TO_HOST16(x)  bswap_16(x)
#define WLAN_TO_HOST32(x)  bswap_32(x)
#define WLAN_TO_HOST64(x)  bswap_64(x)

#define HOST_TO_WLAN16(x)  bswap_16(x)
#define HOST_TO_WLAN32(x)  bswap_32(x)
#define HOST_TO_WLAN64(x)  bswap_64(x)
/******************************************/
#else /*__BYTE_ORDER == __BIG_ENDIAN */
/******************************************
 * MAC <=> Driver interface conversion
 *****************************************/
#define MAC_TO_HOST16(x)   (x)
#define MAC_TO_HOST32(x)   (x)

#define HOST_TO_MAC16(x)   (x)
#define HOST_TO_MAC32(x)   (x)
/******************************************/

/******************************************
 * 802.11 conversion
 *****************************************/
#define WLAN_TO_HOST16(x)  (x)
#define WLAN_TO_HOST32(x)  (x)
#define WLAN_TO_HOST64(x)  (x)

#define HOST_TO_WLAN16(x)  (x)
#define HOST_TO_WLAN32(x)  (x)
#define HOST_TO_WLAN64(x)  (x)
/******************************************/
#endif
/******************************************
 * Network conversion
 *****************************************/
#define NET_TO_HOST16(x)        ntohs(x)
#define NET_TO_HOST32(x)        ntohl(x)
#define NET_TO_HOST16_CONST(x)  (__constant_ntohs(x))
#define HOST_TO_NET16_CONST(x)  (__constant_htons(x))

#define HOST_TO_NET16(x)        htons(x)
#define HOST_TO_NET32(x)        htonl(x)
/******************************************/

#define __MTLK_ASSERT(x)       ASSERT(x)
#define MTLK_ASSERT(x)         __MTLK_ASSERT(x)

#define MTLK_UNREFERENCED_PARAM(x)  ((x) = (x))
#define MTLK_OFFSET_OF(type, field) \
  (uint32)(&((type*)0)->field)
#define MTLK_CONTAINER_OF(address, type, field) \
  (type *)((uint8 *)(address) - MTLK_OFFSET_OF(type, field))

#ifndef NULL
#define NULL 0
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;
typedef int64_t  int64;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#ifndef MAC_FMT
#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#ifndef MAC_ARG
#define MAC_ARG(x) ((uint8*)(x))[0],((uint8*)(x))[1],((uint8*)(x))[2],((uint8*)(x))[3],((uint8*)(x))[4],((uint8*)(x))[5]
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

#include "utils.h"
#include "memdefs.h"

#endif /* !_MTLKINC_H_ */
