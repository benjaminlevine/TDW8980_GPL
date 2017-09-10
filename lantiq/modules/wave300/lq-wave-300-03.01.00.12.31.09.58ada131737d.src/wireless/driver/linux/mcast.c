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
 * Reliable Multicast routines.
 * 
 *  Originaly written by Artem Migaev & Andriy Tkachuk
 *
 */

#include "mtlkinc.h"

#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/igmp.h>
#include <linux/icmpv6.h>
#include <linux/etherdevice.h>
#include <net/ipv6.h>
#include <net/addrconf.h>        // for ipv6_addr_is_multicast()

#include "compat.h"
#include "mcast.h"
#include "utils.h"
#include "mtlk_sq.h"
#include "sq.h"

#if (defined MTLK_DEBUG_IPERF_PAYLOAD_RX) || (defined MTLK_DEBUG_IPERF_PAYLOAD_TX)
#include "iperf_debug.h"
#endif

#define MCAST_HASH(ip)      (ip & (MCAST_HASH_SIZE - 1))
/* Define this if you want to copy skbuufs (debug only) */

/*****************************************************************************/
/***                        Module definitions                             ***/
/*****************************************************************************/

/* IPv4*/
static int           parse_igmp  (mcast_ctx *mcast, struct igmphdr *igmp_header, sta_entry *sta);
static mcast_sta*    add_ip4_sta (mcast_ctx *mcast, sta_entry *sta, struct in_addr *addr);
static int           del_ip4_sta (mcast_ctx *mcast, sta_entry *sta, struct in_addr *addr);
static mcast_ip4_gr* find_ip4_gr (mcast_ctx *mcast, struct in_addr *addr);
static int           del_ip4_gr  (mcast_ctx *mcast, mcast_ip4_gr *gr);

/* IPv6 */
static int           parse_icmp6 (mcast_ctx *mcast, struct icmp6hdr *icmp6_header, sta_entry *sta);
static mcast_sta*    add_ip6_sta (mcast_ctx *mcast, sta_entry *sta, struct in6_addr *addr);
static int           del_ip6_sta (mcast_ctx *mcast, sta_entry *sta, struct in6_addr *addr);
static mcast_ip6_gr* find_ip6_gr (mcast_ctx *mcast, struct in6_addr *addr);
static int           del_ip6_gr  (mcast_ctx *mcast, mcast_ip6_gr *gr);

/* MLDv2 headers definitions. Taken from <linux/igmp.h> */
struct mldv2_grec {
  uint8  grec_type;
  uint8  grec_auxwords;
  uint16 grec_nsrcs;
  struct in6_addr grec_mca;
  struct in6_addr grec_src[0];
};

struct mldv2_report {
  uint8  type;
  uint8  resv1;
  uint16 csum;
  uint16 resv2;
  uint16 ngrec;
  struct mldv2_grec grec[0];
};

struct mldv2_query {
  uint8  type;
	uint8  code;
	uint16 csum;
  uint16 max_response;
  uint16 resv1;
  struct in6_addr group;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	uint8 qrv:3,
        suppress:1,
        resv2:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
	uint8 resv2:4,
       suppress:1,
       qrv:3;
#else
#error "Please define endianess!"
#endif
  uint8 qqic;
  uint16 nsrcs;
  struct in6_addr srcs[0];
};


/*****************************************************************************/
/***                        API Implementation                             ***/
/*****************************************************************************/


void
mtlk_mc_init (struct nic *nic)
{
  memset(&nic->mcast, 0, sizeof(mcast_ctx));
  // Reset querier STA
  nic->mcast.querier = NULL;
  // ReliableMulticast is enabled by default
  nic->reliable_mcast = 1;
}



void
mtlk_mc_reset_querier (struct nic *nic, sta_entry *sta)
{
  // Check if disconnected STA was the querier 
  if (nic->mcast.querier == sta)
    nic->mcast.querier = NULL;
}



void
mtlk_mc_drop_querier (struct nic *nic)
{
  nic->mcast.querier = NULL;
}



int
mtlk_mc_parse (struct sk_buff *skb)
{
  int res = -1;
  struct nic *nic = netdev_priv(skb->dev);
  struct skb_private *skb_priv = (struct skb_private *)(skb->cb);
  struct ethhdr *ether_header;
  sta_entry *sta = skb_priv->src_sta;

  // Check that we got packet from the one of connected STAs
  // TODO: Do we need this? Probably this was already checked before...
  if (sta == NULL) {
    WLOG("Source STA not found among connected!");
    return -1;
  }

  // TODO: This could also be checked prior to calling mc_parse()
  if (!nic->slow_ctx->hw_cfg.ap)
    return -1;

  // Check IP content in MAC header (Ethernet II assumed)
  ether_header = (struct ethhdr *)skb->data;
  switch (ntohs(ether_header->h_proto))
  {
  case ETH_P_IP:
    {
      // Check IGMP content in IP header
      struct iphdr *ip_header = (struct iphdr *)
            ((unsigned long)ether_header + sizeof(struct ethhdr));
      struct igmphdr *igmp_header = (struct igmphdr *)
            ((unsigned long)ip_header + ip_header->ihl * 4);

      if (ip_header->protocol == IPPROTO_IGMP)
        res = parse_igmp(&nic->mcast, igmp_header, sta);
      break;
    }
  case ETH_P_IPV6:
    {
      // Check MLD content in IPv6 header
      struct ipv6hdr *ipv6_header = (struct ipv6hdr *)
            ((unsigned long)ether_header + sizeof(struct ethhdr));
      struct ipv6_hopopt_hdr *hopopt_header = (struct ipv6_hopopt_hdr *)
            ((unsigned long)ipv6_header + sizeof(struct ipv6hdr));
      uint16 *hopopt_data = (uint16 *)
            ((unsigned long)hopopt_header + 2);
      uint16 *rtalert_data = (uint16 *)
            ((unsigned long)hopopt_data + 2);
      struct icmp6hdr *icmp6_header = (struct icmp6hdr *)
            ((unsigned long)hopopt_header + 8 * (hopopt_header->hdrlen + 1));

      if (
          (ipv6_header->nexthdr == IPPROTO_HOPOPTS) &&  // hop-by-hop option in IPv6 packet header
          (htons(*hopopt_data) == 0x0502) &&            // Router Alert option in hop-by-hop option header
          (*rtalert_data == 0) &&                       // MLD presence in Router Alert option header
          (hopopt_header->nexthdr == IPPROTO_ICMPV6)    // ICMPv6 next option in hop-by-hop option header
         )
        res = parse_icmp6(&nic->mcast, icmp6_header, sta);
      break;
    }
  default:
    break;
  }
  return res;
}



/*
 * dst_sta_id determined here only for unicast
 * or reliable multicast transfers
 * */
static int
xmit (struct nic *nic, struct sk_buff *skb, int ac, int clone)
{
  int res = MTLK_ERR_OK;

  if (nic->pack_sched_enabled) {
    /* enqueue cloned packets to the back of the queue */
    if (clone)
      res = mtlk_sq_enqueue_clone(nic->sq, skb);
    else
      res = mtlk_sq_enqueue(nic->sq, ac, SQ_PUT_BACK, skb);

    if (likely(res == MTLK_ERR_OK)) {
      struct skb_private *skb_priv = (struct skb_private *)(skb->cb);
      if (nic->ap) {
        mtlk_aocs_on_tx_msdu_enqued(&nic->slow_ctx->aocs, ac, 
                                    mtlk_sq_get_qsize(nic->sq, ac), 
                                    mtlk_sq_get_limit(nic->sq, ac));
      }
      if (skb_priv->dst_sta) {
        mtlk_stadb_update_sta_tx(&nic->slow_ctx->stadb, skb_priv->dst_sta, skb->priority);
      }
    }

    /* schedule flush of the queue */
    mtlk_sq_schedule_flush(nic);
  } else {
    res = mtlk_xmit(skb, skb->dev);
  }

  return res;
}

int
mtlk_mc_transmit (struct sk_buff *skb)
{
  mcast_sta *mcsta = NULL;
  int res = MTLK_ERR_OK;
  struct nic *nic = netdev_priv(skb->dev);
  struct skb_private *skb_priv = (struct skb_private *)(skb->cb);
  struct ethhdr *ether_header;
  uint16 ac = mtlk_qos_get_ac_by_tid(skb->priority);
  
#ifdef MTLK_DEBUG_IPERF_PAYLOAD_TX
  debug_ooo_analyze_packet(FALSE, skb, 0);
#endif

  // Save pointer to ethernet header before conversion
  ether_header = (struct ethhdr *)skb->data;

  // Determine destination STA id for unicast and backqueued
  // reliable multicast, if STA not found - drop
  if (skb_priv->flags & SKB_UNICAST) {
    if (skb_priv->dst_sta != NULL)
      goto MTLK_MC_TRANSMIT_NONRMCAST;
    else
      goto MTLK_MC_TRANSMIT_CONSUME;
  }

  // Broadcast transmitted as usual broadcast
  if (skb_priv->flags & SKB_BROADCAST)
    goto MTLK_MC_TRANSMIT_NONRMCAST;

  // Perform checks:
  // - AP
  // - Reliable Multicast enabled
  // If failed - transmit as 802.11n generic multicast
  if ((nic->reliable_mcast == 0) ||
      (nic->slow_ctx->hw_cfg.ap == 0))
    goto MTLK_MC_TRANSMIT_NONRMCAST;

  switch (ntohs(ether_header->h_proto))
  {
  case ETH_P_IP:
    {
      mcast_ip4_gr *group = NULL;
      struct in_addr ip_addr;
      struct iphdr *ip_header = (struct iphdr *)
          ((unsigned long)ether_header + sizeof(struct ethhdr));

      ip_addr.s_addr = ntohl(ip_header->daddr);

      ILOG3(GID_MCAST, "IPv4: %B", ip_addr.s_addr);

      // Check link local multicast space 224.0.0.0/24
      if ((ip_addr.s_addr & 0xFFFFFF00) == 0xE0000000) {
        goto MTLK_MC_TRANSMIT_NONRMCAST;
      }

      // Drop unknown multicast
      if ((group = find_ip4_gr(&nic->mcast, &ip_addr)) == NULL)
        goto MTLK_MC_TRANSMIT_CONSUME;

      // Get first STA in group 
      mcsta = group->mcsta;

      break;
    }
  case ETH_P_IPV6:
    {
      mcast_ip6_gr *group = NULL;
      struct in6_addr ip_addr;
      struct ipv6hdr *ip_header = (struct ipv6hdr *)
          ((unsigned long)ether_header + sizeof(struct ethhdr));

      ip_addr.in6_u.u6_addr32[0] = ntohl(ip_header->daddr.in6_u.u6_addr32[0]);
      ip_addr.in6_u.u6_addr32[1] = ntohl(ip_header->daddr.in6_u.u6_addr32[1]);
      ip_addr.in6_u.u6_addr32[2] = ntohl(ip_header->daddr.in6_u.u6_addr32[2]);
      ip_addr.in6_u.u6_addr32[3] = ntohl(ip_header->daddr.in6_u.u6_addr32[3]);

      ILOG3(GID_MCAST, "IPv6: %K", ip_addr.s6_addr);

      // Check link local multicast space FF02::/16
      if ((ip_addr.in6_u.u6_addr32[0] & 0xFF0F0000) == 0xFF020000) {
        goto MTLK_MC_TRANSMIT_NONRMCAST;
      }

      // Drop unknown multicast
      if ((group = find_ip6_gr(&nic->mcast, &ip_addr)) == NULL) {
        goto MTLK_MC_TRANSMIT_CONSUME;
      }

      // Get first STA in group 
      mcsta = group->mcsta;

      break;
    }

  default:
    goto MTLK_MC_TRANSMIT_NONRMCAST;
  }

  // Now skbuff is reliable multicast packet
  skb_priv->flags |= SKB_RMCAST;
  if (mtlk_sq_enqueue_clone_begin(nic->sq, ac, SQ_PUT_BACK, skb) != MTLK_ERR_OK)
    goto MTLK_MC_TRANSMIT_CONSUME;

  // Transmit packet to all STAs in group
  for (; mcsta != NULL; mcsta = mcsta->next) {
    struct sk_buff *nskb;
    sta_entry *sta = mcsta->sta;
    /* Skip source STA */
    if (skb_priv->src_sta == sta)
      continue;

    /* Set new destination address */
    skb_priv->dst_sta = sta;

    /* Clone buffer (all private information copied) */
    nskb = skb_clone(skb, GFP_ATOMIC);
    if (unlikely(nskb == NULL)) {
      nic->pstats.rmcast_dropped++;
      continue;
    }

    res = xmit(nic, nskb, ac, 1);
  }
  mtlk_sq_enqueue_clone_end(nic->sq, skb);

  if (nic->pack_sched_enabled) {
    /* schedule flush of the queue */
    mtlk_sq_schedule_flush(nic);
  }

MTLK_MC_TRANSMIT_CONSUME:
  dev_kfree_skb(skb);
  return MTLK_ERR_OK;

MTLK_MC_TRANSMIT_NONRMCAST:

  /* Send m/bcast frames to peerAPs as unicast */
  if (skb_priv->flags & SKB_BROADCAST || skb_priv->flags & SKB_MULTICAST) {
    int i;
    sta_entry *sta = nic->slow_ctx->stadb.connected_stations;
    int sa_sid = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, ether_header->h_source);

    if (mtlk_sq_enqueue_clone_begin(nic->sq, ac, SQ_PUT_BACK, skb) != MTLK_ERR_OK)
      goto MTLK_MC_TRANSMIT_CONSUME;
    for (i = 0; i < STA_MAX_STATIONS; i++, sta++) {
      if (sta->state != PEER_CONNECTED) continue;
      if (sta->peer_ap) {
        struct skb_private *skb_priv;
        struct sk_buff *nskb;
        /* Don't resend the packet to the same peerAP
         * we received it from to avoid loops */
        if (i == sa_sid)
          continue;
        nskb = skb_clone(skb, GFP_ATOMIC);
        if (unlikely(nskb == NULL))
          continue;
        skb_priv = (struct skb_private *)(nskb->cb);
        skb_priv->flags |= SKB_RMCAST;
        skb_priv->dst_sta = sta;
        res = xmit(nic, nskb, ac, 1);
      }
    }
    mtlk_sq_enqueue_clone_end(nic->sq, skb);
    if (nic->pack_sched_enabled)
      mtlk_sq_schedule_flush(nic);
  }

  res = xmit(nic, skb, ac, 0);

  return res;
}



void
mtlk_mc_drop_sta (struct nic *nic, unsigned char *mac)
{
  mcast_sta *mcsta, *last_mcsta;
  sta_entry *sta;
  int i;

  // RM option disabled or not AP
  if (!nic->slow_ctx->hw_cfg.ap)
    return;

  // IPv4 hash
  for (i = 0; i < MCAST_HASH_SIZE; i++) {
    mcast_ip4_gr *gr, *gr_next;
    for (gr = nic->mcast.mcast_ip4_hash[i]; gr != NULL;) {
      last_mcsta = NULL;
      gr_next = gr->next;
      for (mcsta = gr->mcsta; mcsta != NULL; mcsta = mcsta->next) {
        sta = mcsta->sta;
        if (mtlk_osal_compare_eth_addresses(mac, sta->mac) == 0) {
          if (last_mcsta == NULL)
            gr->mcsta = mcsta->next;
          else
            last_mcsta->next = mcsta->next;
          kfree_tag(mcsta);

          if (gr->mcsta == NULL)
            del_ip4_gr(&nic->mcast, gr);

          break;
        }
        last_mcsta = mcsta;
      }
      gr = gr_next;
    }
  }

  // IPv6 hash
  for (i = 0; i < MCAST_HASH_SIZE; i++) {
    mcast_ip6_gr *gr, *gr_next;
    for (gr = nic->mcast.mcast_ip6_hash[i]; gr != NULL;) {
      last_mcsta = NULL;
      gr_next = gr->next;
      for (mcsta = gr->mcsta; mcsta != NULL; mcsta = mcsta->next) {
        sta = mcsta->sta;
        if (mtlk_osal_compare_eth_addresses(mac, sta->mac) == 0) {
          if (last_mcsta == NULL)
            gr->mcsta = mcsta->next;
          else
            last_mcsta->next = mcsta->next;
          kfree_tag(mcsta);

          if (gr->mcsta == NULL)
            del_ip6_gr(&nic->mcast, gr);

          break;
        }
        last_mcsta = mcsta;
      }
      gr = gr_next;
    }
  }

  ILOG1(GID_MCAST, "STA %Y dropped", mac);

  return;
}



void
mtlk_mc_restore_mac (struct sk_buff *skb)
{
  struct ethhdr *ether_header = (struct ethhdr *)skb->data;

  switch (ntohs(ether_header->h_proto))
  {
  case ETH_P_IP:
    {
      struct in_addr ip_addr;
      struct iphdr *ip_header = (struct iphdr *)
          ((unsigned long)ether_header + sizeof(struct ethhdr));
      
      /* do nothing if destination address is:
       *   - not multicast;
       *   - link local multicast.
       */
      if (!mtlk_ipv4_is_multicast(ip_header->daddr) || 
           mtlk_ipv4_is_local_multicast(ip_header->daddr))

        return;
  
      ip_addr.s_addr = ntohl(ip_header->daddr);

      ether_header->h_dest[0] = 0x01;
      ether_header->h_dest[1] = 0x00;
      ether_header->h_dest[2] = 0x5E;
      ether_header->h_dest[3] = (ip_addr.s_addr >> 16) & 0x7F;
      ether_header->h_dest[4] = (ip_addr.s_addr >> 8) & 0xFF;
      ether_header->h_dest[5] = ip_addr.s_addr & 0xFF;

      break;
    }
  case ETH_P_IPV6:
    {
      struct in6_addr ip_addr;
      struct ipv6hdr *ip_header = (struct ipv6hdr *)
          ((unsigned long)ether_header + sizeof(struct ethhdr));

      /* do nothing if destination address is not multicast */
      if (ipv6_addr_is_multicast(&ip_header->daddr))
        return;

      ip_addr.in6_u.u6_addr32[0] = ntohl(ip_header->daddr.in6_u.u6_addr32[0]);
      ip_addr.in6_u.u6_addr32[1] = ntohl(ip_header->daddr.in6_u.u6_addr32[1]);
      ip_addr.in6_u.u6_addr32[2] = ntohl(ip_header->daddr.in6_u.u6_addr32[2]);
      ip_addr.in6_u.u6_addr32[3] = ntohl(ip_header->daddr.in6_u.u6_addr32[3]);

      // Check link local multicast space FF02::/16
      if ((ip_addr.in6_u.u6_addr32[0] & 0xFF0F0000) == 0xFF020000)
        return;

      ether_header->h_dest[0] = 0x33;
      ether_header->h_dest[1] = 0x33;
      ether_header->h_dest[2] = ip_addr.in6_u.u6_addr8[12];
      ether_header->h_dest[3] = ip_addr.in6_u.u6_addr8[13];
      ether_header->h_dest[4] = ip_addr.in6_u.u6_addr8[14];
      ether_header->h_dest[5] = ip_addr.in6_u.u6_addr8[15];

      break;
    }
  default:
    break;
  }
}



int
mtlk_mc_dump_groups (struct nic *nic, char *buffer)
{
  mcast_sta *mcsta;
  sta_entry *sta;
  int i, res, len = 0;

  // IPv4 hash
  for (i = 0; i < MCAST_HASH_SIZE; i++) {
    mcast_ip4_gr *gr;
    for (gr = nic->mcast.mcast_ip4_hash[i]; gr != NULL; gr = gr->next) {
      res = sprintf(buffer, "IPv4 mcast group " IPV4_FMT "\n", IPV4_ARG(gr->ip4_addr));
      buffer += res;
      len += res;
      for (mcsta = gr->mcsta; mcsta != NULL; mcsta = mcsta->next) {
        sta = mcsta->sta;
        res = sprintf(buffer, "    " MAC_FMT "\n", MAC_ARG(sta->mac));
        buffer += res;
        len += res;
      }
    }
  }

  // IPv6 hash
  for (i = 0; i < MCAST_HASH_SIZE; i++) {
    mcast_ip6_gr *gr;
    for (gr = nic->mcast.mcast_ip6_hash[i]; gr != NULL; gr = gr->next) {
      res = sprintf(buffer, "IPv6 mcast group " IPV6_FMT "\n", IPV6_ARG(gr->ip6_addr));
      buffer += res;
      len += res;
      for (mcsta = gr->mcsta; mcsta != NULL; mcsta = mcsta->next) {
        sta = mcsta->sta;
        res = sprintf(buffer, "    " MAC_FMT "\n", MAC_ARG(sta->mac));
        buffer += res;
        len += res;
      }
    }
  }


  return len;
}


/*****************************************************************************/
/***                        Local IPv4 functions                           ***/
/*****************************************************************************/


static mcast_sta*
add_ip4_sta (mcast_ctx *mcast, sta_entry *sta, struct in_addr *addr)
{
  mcast_sta *mcsta;
  unsigned int hash = MCAST_HASH(addr->s_addr);
  mcast_ip4_gr *gr;

  // Check link local IPv4 multicast space 224.0.0.0/24 and source station ID
  if (((addr->s_addr & 0xFFFFFF00) == 0xE0000000) || (sta == NULL))
    return NULL;

  gr = find_ip4_gr(mcast, addr);
  if (gr == NULL) {
    gr = kmalloc_tag(sizeof(mcast_ip4_gr), GFP_ATOMIC, MTLK_MEM_TAG_MCAST);
    ASSERT(gr != NULL);

    if (gr == NULL)
      return NULL;
 
    gr->mcsta = NULL;
    gr->ip4_addr = *addr;
    gr->next = mcast->mcast_ip4_hash[hash];
    mcast->mcast_ip4_hash[hash] = gr;
  }

  for (mcsta = gr->mcsta; mcsta != NULL; mcsta = mcsta->next) {
    if (mcsta->sta == sta)
      return NULL;
  }
  
  mcsta = kmalloc_tag(sizeof(mcast_sta), GFP_ATOMIC, MTLK_MEM_TAG_MCAST);
  ASSERT(mcsta != NULL);

  if (mcsta == NULL)
    return NULL;

  mcsta->next = gr->mcsta;
  mcsta->sta = sta;
  gr->mcsta = mcsta;

  return mcsta;
}



static int
del_ip4_sta (mcast_ctx *mcast, sta_entry *sta, struct in_addr *addr)
{
  mcast_sta *mcsta, *last_mcsta = NULL;
  mcast_ip4_gr *gr = find_ip4_gr(mcast, addr);

  if (gr == NULL)
    return 0;

  if (sta == NULL)
    return 0;
  
  for (mcsta = gr->mcsta; mcsta != NULL; mcsta = mcsta->next) {
    if (mcsta->sta == sta)
      break;
    last_mcsta = mcsta;
  }

  if (mcsta == NULL)
    return 0;

  if (last_mcsta == NULL)
    gr->mcsta = mcsta->next;
  else
    last_mcsta->next = mcsta->next;
  kfree_tag(mcsta);

  if (gr->mcsta == NULL)
    return del_ip4_gr(mcast, gr);

  return 1;
}



static int
del_ip4_gr (mcast_ctx *mcast, mcast_ip4_gr *gr)
{
  unsigned int hash;
  mcast_ip4_gr *last_gr;

  if (gr == NULL)
    return 0;

  hash = MCAST_HASH(gr->ip4_addr.s_addr);

  if (mcast->mcast_ip4_hash[hash] == gr) {
    mcast->mcast_ip4_hash[hash] = gr->next;
  } else {
    for (last_gr = mcast->mcast_ip4_hash[hash];
         last_gr != NULL;
         last_gr = last_gr->next)
    {
      if (last_gr->next == gr) {
        last_gr->next = gr->next;
        break;
      }
    }
    if (last_gr == NULL)
      return 0;
  }
  kfree_tag(gr);

  return 1;
}



static mcast_ip4_gr *
find_ip4_gr (mcast_ctx *mcast, struct in_addr *addr)
{
  unsigned int hash = MCAST_HASH(addr->s_addr);
  mcast_ip4_gr *gr = mcast->mcast_ip4_hash[hash];

  for (; gr != NULL; gr = gr->next)
    if (gr->ip4_addr.s_addr == addr->s_addr)
      break;

  return gr;
}



static int
parse_igmp (mcast_ctx *mcast, struct igmphdr *igmp_header, sta_entry *sta)
{
  uint32 grp_num, src_num;
  struct igmpv3_report *report;
  struct igmpv3_grec *record;
  int i;
  struct in_addr grp_addr;
 
  grp_addr.s_addr = ntohl(igmp_header->group);

  switch (igmp_header->type) {
  case IGMP_HOST_MEMBERSHIP_QUERY:
    ILOG1(GID_MCAST, "Membership query: IPv4 %B, sta %p", grp_addr.s_addr, sta);
    mcast->querier = sta;
    break;
  case IGMP_HOST_MEMBERSHIP_REPORT:
    /* fallthrough */
  case IGMPV2_HOST_MEMBERSHIP_REPORT:
    ILOG1(GID_MCAST, "Membership report: IPv4 %B, sta %p", grp_addr.s_addr, sta);
    add_ip4_sta(mcast, sta, &grp_addr);
    break;
  case IGMP_HOST_LEAVE_MESSAGE:
    ILOG1(GID_MCAST, "Leave group report: IPv4 %B, sta %p", grp_addr.s_addr, sta);
    del_ip4_sta(mcast, sta, &grp_addr);
    break;
  case IGMPV3_HOST_MEMBERSHIP_REPORT:
    report = (struct igmpv3_report *)igmp_header;
    grp_num = ntohs(report->ngrec);
    ILOG1(GID_MCAST, "IGMPv3 report: %d record(s), sta %p", grp_num, sta);
    record = report->grec;
    for (i = 0; i < grp_num; i++) {
      src_num = ntohs(record->grec_nsrcs);
      grp_addr.s_addr = ntohl(record->grec_mca);
      ILOG1(GID_MCAST, " *** IPv4 %B", grp_addr.s_addr);
      switch (record->grec_type) {
      case IGMPV3_MODE_IS_INCLUDE:
        /* fallthrough */
      case IGMPV3_CHANGE_TO_INCLUDE:
        ILOG1(GID_MCAST, " --- Mode is include, %d source(s)", src_num);
        // Station removed from the multicast list only if
        // no sources included
        if (src_num == 0)
          del_ip4_sta(mcast, sta, &grp_addr);  
        break;
      case IGMPV3_MODE_IS_EXCLUDE:
        /* fallthrough */
      case IGMPV3_CHANGE_TO_EXCLUDE:
        ILOG1(GID_MCAST, " --- Mode is exclude, %d source(s)", src_num);
        // Station added to the multicast llist no matter
        // how much sources are excluded
        add_ip4_sta(mcast, sta, &grp_addr);
        break;
      case IGMPV3_ALLOW_NEW_SOURCES:
        ILOG1(GID_MCAST, " --- Allow new sources, %d source(s)", src_num);
        // ignore
        break;
      case IGMPV3_BLOCK_OLD_SOURCES:
        ILOG1(GID_MCAST, " --- Block old sources, %d source(s)", src_num);
        // ignore
        break;
      default:  // Unknown IGMPv3 record
        ILOG1(GID_MCAST, " --- Unknown record type %d", record->grec_type);
        break;
      }
      record = (struct igmpv3_grec *)((void *)record +
               sizeof(struct igmpv3_grec) +
               sizeof(struct in_addr) * src_num +
               sizeof(u32) * record->grec_auxwords);
    }
    break;
  default:      // Unknown IGMP message
    ILOG1(GID_MCAST, "Unknown IGMP message type %d", igmp_header->type);
    break;
  }
  return 0;
}


/*****************************************************************************/
/***                        Local IPv6 functions                           ***/
/*****************************************************************************/


static mcast_sta *
add_ip6_sta (mcast_ctx *mcast, sta_entry *sta, struct in6_addr *addr)
{
  mcast_sta *mcsta;
  unsigned int hash = MCAST_HASH(addr->in6_u.u6_addr32[3]);
  mcast_ip6_gr *gr;

  // Check link local multicast space FF02::/16 and source station ID
  if (((addr->in6_u.u6_addr32[0] & 0xFF0F0000) == 0xFF020000) || (sta == NULL))
    return NULL;
 
  gr = find_ip6_gr(mcast, addr); 
  if (gr == NULL) {
    gr = kmalloc_tag(sizeof(mcast_ip6_gr), GFP_ATOMIC, MTLK_MEM_TAG_MCAST);
    ASSERT(gr != NULL);

    if (gr == NULL)
      return NULL;
 
    gr->mcsta = NULL;
    gr->ip6_addr = *addr;
    gr->next = mcast->mcast_ip6_hash[hash];
    mcast->mcast_ip6_hash[hash] = gr;
  }

  for (mcsta = gr->mcsta; mcsta != NULL; mcsta = mcsta->next) {
    if (mcsta->sta == sta)
      return NULL;
  }
  
  mcsta = kmalloc_tag(sizeof(mcast_sta), GFP_ATOMIC, MTLK_MEM_TAG_MCAST);
  ASSERT(mcsta != NULL);

  if (mcsta == NULL)
    return NULL;

  mcsta->next = gr->mcsta;
  mcsta->sta = sta;
  gr->mcsta = mcsta;

  return mcsta;
}



static int
del_ip6_sta (mcast_ctx *mcast, sta_entry *sta, struct in6_addr *addr)
{
  mcast_sta *mcsta, *last_mcsta = NULL;
  mcast_ip6_gr *gr = find_ip6_gr(mcast, addr);

  if (gr == NULL)
    return 0;

  if (sta == NULL)
    return 0;
  
  for (mcsta = gr->mcsta; mcsta != NULL; mcsta = mcsta->next) {
    if (mcsta->sta == sta)
      break;
    last_mcsta = mcsta;
  }

  if (mcsta == NULL)
    return 0;

  if (last_mcsta == NULL)
    gr->mcsta = mcsta->next;
  else
    last_mcsta->next = mcsta->next;
  kfree_tag(mcsta);

  if (gr->mcsta == NULL)
    return del_ip6_gr(mcast, gr);

  return 1;
}



static int
del_ip6_gr (mcast_ctx *mcast, mcast_ip6_gr *gr)
{
  unsigned int hash;
  mcast_ip6_gr *last_gr;

  if (gr == NULL)
    return 0;

  hash = MCAST_HASH(gr->ip6_addr.in6_u.u6_addr32[3]);

  if (mcast->mcast_ip6_hash[hash] == gr) {
    mcast->mcast_ip6_hash[hash] = gr->next;
  } else {
    for (last_gr = mcast->mcast_ip6_hash[hash];
         last_gr != NULL;
         last_gr = last_gr->next)
    {
      if (last_gr->next == gr) {
        last_gr->next = gr->next;
        break;
      }
    }
    if (last_gr == NULL)
      return 0;
  }
  kfree_tag(gr);

  return 1;
}



static mcast_ip6_gr*
find_ip6_gr (mcast_ctx *mcast, struct in6_addr *addr)
{
  unsigned int hash = MCAST_HASH(addr->in6_u.u6_addr32[3]);
  mcast_ip6_gr *gr = mcast->mcast_ip6_hash[hash];

  for (; gr != NULL; gr = gr->next)
    if (ipv6_addr_equal(&gr->ip6_addr, addr))
      break;

  return gr;
}



static int
parse_icmp6 (mcast_ctx *mcast, struct icmp6hdr *icmp6_header, sta_entry *sta)
{
  uint32 grp_num, src_num;
  int i;
  struct mldv2_report *report;
  struct mldv2_grec *record;
  struct in6_addr grp_addr, *addr;
 
  addr = (struct in6_addr *)
    ((unsigned long)icmp6_header + sizeof(struct icmp6hdr));
  grp_addr.in6_u.u6_addr32[0] = ntohl(addr->in6_u.u6_addr32[0]);
  grp_addr.in6_u.u6_addr32[1] = ntohl(addr->in6_u.u6_addr32[1]);
  grp_addr.in6_u.u6_addr32[2] = ntohl(addr->in6_u.u6_addr32[2]);
  grp_addr.in6_u.u6_addr32[3] = ntohl(addr->in6_u.u6_addr32[3]);

  switch (icmp6_header->icmp6_type)
  {
  case ICMPV6_MGM_QUERY:
    ILOG1(GID_MCAST, "MLD membership query: IPv6 %K, sta %p", grp_addr.s6_addr, sta);
    mcast->querier = sta;
    break;
  case ICMPV6_MGM_REPORT:
    ILOG1(GID_MCAST, "MLD membership report: IPv6 %K, sta %p", grp_addr.s6_addr, sta);
    add_ip6_sta(mcast, sta, &grp_addr);
    break;
  case ICMPV6_MGM_REDUCTION:
    ILOG1(GID_MCAST, "MLD membership done: IPv6 %K, sta %p", grp_addr.s6_addr, sta);
    del_ip6_sta(mcast, sta, &grp_addr);
    break;
  case ICMPV6_MLD2_REPORT:
    report = (struct mldv2_report *)icmp6_header;
    grp_num = ntohs(report->ngrec);
    ILOG1(GID_MCAST, "MLDv2 report: %d record(s), sta %p", grp_num, sta);
    record = report->grec;
    for (i = 0; i < grp_num; i++) {
      src_num = ntohs(record->grec_nsrcs);
      addr = (struct in6_addr *)
        ((unsigned long)icmp6_header + sizeof(struct icmp6hdr));
      grp_addr.in6_u.u6_addr32[0] = ntohl(record->grec_mca.in6_u.u6_addr32[0]);
      grp_addr.in6_u.u6_addr32[1] = ntohl(record->grec_mca.in6_u.u6_addr32[1]);
      grp_addr.in6_u.u6_addr32[2] = ntohl(record->grec_mca.in6_u.u6_addr32[2]);
      grp_addr.in6_u.u6_addr32[3] = ntohl(record->grec_mca.in6_u.u6_addr32[3]);
      ILOG1(GID_MCAST, " *** IPv6 %K", grp_addr.s6_addr);
      switch (record->grec_type) {
      case MLD2_MODE_IS_INCLUDE:
        /* fallthrough */
      case MLD2_CHANGE_TO_INCLUDE:
        ILOG1(GID_MCAST, " --- Mode is include, %d source(s)", src_num);
        // Station removed from the multicast list only if
        // no sources included
        if (src_num == 0)
          del_ip6_sta(mcast, sta, &grp_addr);  
        break;
      case MLD2_MODE_IS_EXCLUDE:
        /* fallthrough */
      case MLD2_CHANGE_TO_EXCLUDE:
        ILOG1(GID_MCAST, " --- Mode is exclude, %d source(s)", src_num);
        // Station added to the multicast llist no matter
        // how much sources are excluded
        add_ip6_sta(mcast, sta, &grp_addr);
        break;
      case MLD2_ALLOW_NEW_SOURCES:
        ILOG1(GID_MCAST, " --- Allow new sources, %d source(s)", src_num);
        // ignore
        break;
      case MLD2_BLOCK_OLD_SOURCES:
        ILOG1(GID_MCAST, " --- Block old sources, %d source(s)", src_num);
        // ignore
        break;
      default:  // Unknown MLDv2 record
        ILOG1(GID_MCAST, " --- Unknown record type %d", record->grec_type);
        break;
      }
      record = (struct mldv2_grec *)((void *)record +
               sizeof(struct mldv2_grec) +
               sizeof(struct in6_addr) * src_num +
               sizeof(u32) * record->grec_auxwords);
    }
    break;
  default:
    ILOG1(GID_MCAST, "Unknown ICMPv6/MLD message");
    break;
  }
  return 0;
}

