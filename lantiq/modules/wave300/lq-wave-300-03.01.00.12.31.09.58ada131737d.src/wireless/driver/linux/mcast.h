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
 *  Originaly written by Artem Migaev
 *
 */

#ifndef _MCAST_H_
#define _MCAST_H_

#include <linux/in.h>
#include <linux/in6.h>

#include "stadb.h"

#define IPV6_FMT "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x"
#define IPV6_ARG(x)  x.in6_u.u6_addr8[0],x.in6_u.u6_addr8[1],x.in6_u.u6_addr8[2],x.in6_u.u6_addr8[3], \
                     x.in6_u.u6_addr8[4],x.in6_u.u6_addr8[5],x.in6_u.u6_addr8[6],x.in6_u.u6_addr8[7], \
                     x.in6_u.u6_addr8[8],x.in6_u.u6_addr8[9],x.in6_u.u6_addr8[10],x.in6_u.u6_addr8[11], \
                     x.in6_u.u6_addr8[12],x.in6_u.u6_addr8[13],x.in6_u.u6_addr8[14],x.in6_u.u6_addr8[15]

#define IPV4_FMT "%d.%d.%d.%d"
#define IPV4_ARG(x) (u8)(x.s_addr>>24),(u8)(x.s_addr>>16),(u8)(x.s_addr>>8),(u8)(x.s_addr)

/***************************************************/

#define MCAST_HASH_SIZE   (64)  // must be power of 2

typedef struct _mcast_sta {
  struct _mcast_sta *next;
  sta_entry         *sta;
} mcast_sta;

typedef struct _mcast_ip4_gr {
  struct _mcast_ip4_gr *next;
  struct in_addr       ip4_addr;
  mcast_sta            *mcsta;
} mcast_ip4_gr;

typedef struct _mcast_ip6_gr {
  struct _mcast_ip6_gr *next;
  struct in6_addr      ip6_addr;
  mcast_sta            *mcsta;
} mcast_ip6_gr;

typedef struct _mcast_ctx {
  mcast_ip4_gr *mcast_ip4_hash[MCAST_HASH_SIZE];
  mcast_ip6_gr *mcast_ip6_hash[MCAST_HASH_SIZE];
  sta_entry    *querier;
} mcast_ctx;

#include "core.h"

int  mtlk_mc_parse (struct sk_buff *skb);
int  mtlk_mc_transmit (struct sk_buff *skb);
void mtlk_mc_drop_sta (struct nic *nic, unsigned char *mac);
int  mtlk_mc_dump_groups (struct nic *nic, char *buffer);
void mtlk_mc_restore_mac (struct sk_buff *skb);
void mtlk_mc_init (struct nic *nic);
void mtlk_mc_reset_querier (struct nic *nic, sta_entry *sta);
void mtlk_mc_drop_querier (struct nic *nic);

#endif // _MCAST_H_


