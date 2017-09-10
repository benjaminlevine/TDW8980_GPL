/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef _MTLKINC_H_
#define _MTLKINC_H_

#include <linux/ctype.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <asm/div64.h> /* 64-bit division macros here */

/* AirPeek specific compiling elimination */
#define WILD_PACKETS           0

#define DPR_SFX
#define __MTLK_IFUNC 
#define __INLINE     inline
#define __LIKELY     likely
#define __UNLIKELY   unlikely

#ifdef MTCFG_DEBUG
#define MTLK_DEBUG
#endif

#ifdef MTCFG_SILENT
#define LOG_BUFFER
#else
#define LOG_BUFFER             printk
#endif

#define MTLK_TEXT(x)           x
#define __MTLK_ASSERT(x)       ASSERT(x)
#define MTLK_ASSERT(x)         __MTLK_ASSERT(x)

#define ARGUMENT_PRESENT
#define MTLK_UNREFERENCED_PARAM(x) ((x) = (x))

#define MTLK_OFFSET_OF(type, field)             offsetof(type, field)
#define MTLK_CONTAINER_OF(address, type, field) container_of(address, type, field)

typedef struct iphdr mtlk_ip_hdr_t;
typedef struct ipv6hdr mtlk_ipv6_hdr_t;
typedef struct sk_buff mtlk_buffer_t;
/* type of the slot in l2nat hash bucket. core.h needs
 * it for "struct nic" definition. didn't find any better
 * place to make it visible without duplications.
 */
typedef u8 l2nat_bslot_t;

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

/******************************************
 * MAC <=> Driver interface conversion
 *****************************************/
#define MAC_TO_HOST16(x)   le16_to_cpu(x)
#define MAC_TO_HOST32(x)   le32_to_cpu(x)

#define HOST_TO_MAC16(x)   cpu_to_le16(x)
#define HOST_TO_MAC32(x)   cpu_to_le32(x)
/******************************************/

/******************************************
 * 802.11 conversion
 *****************************************/
#define WLAN_TO_HOST16(x)  le16_to_cpu(x)
#define WLAN_TO_HOST32(x)  le32_to_cpu(x)
#define WLAN_TO_HOST64(x)  le64_to_cpu(x)

#define HOST_TO_WLAN16(x)  cpu_to_le16(x)
#define HOST_TO_WLAN32(x)  cpu_to_le32(x)
#define HOST_TO_WLAN64(x)  cpu_to_le64(x)
/******************************************/

/******************************************
 * Network conversion
 *****************************************/
#define NET_TO_HOST16(x)        ntohs(x)
#define NET_TO_HOST32(x)        ntohl(x)
#define NET_TO_HOST16_CONST(x)  (__constant_ntohs(x))
#define HOST_TO_NET16_CONST(x)  (__constant_htons(x))

#define HOST_TO_NET16(x)   htons(x)
#define HOST_TO_NET32(x)   htonl(x)
/******************************************/

typedef s8  int8;
typedef s16 int16;
typedef s32 int32;
typedef s64 int64;

typedef u8  uint8;
typedef u16 uint16;
typedef u32 uint32;
typedef u64 uint64;

typedef uint16      K_MSG_TYPE;

/* neither Linux version define this yet */
#ifndef IEEE80211_STYPE_BAR
#define IEEE80211_STYPE_BAR    0x0080
#endif

#ifndef NETDEV_TX_OK
#define NETDEV_TX_OK 0
#endif

#ifndef NETDEV_TX_BUSY
#define NETDEV_TX_BUSY 1
#endif

#ifndef MAC_FMT
#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#ifndef MAC_ARG
#define MAC_ARG(x) ((u8*)(x))[0],((u8*)(x))[1],((u8*)(x))[2],((u8*)(x))[3],((u8*)(x))[4],((u8*)(x))[5]
#endif

#define _mtlk_seq_printf_t seq_file
#define mtlk_aux_seq_printf seq_printf

#include "compat.h"
#include "memdefs.h"
#include "utils.h"

#include "mtlkdfdefs.h"
#include "mtlkstartup.h"
#include "cpu_stat.h"
#include "log.h"

#endif /* !_MTLKINC_H_ */
