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
 * L2NAT driver subsystem.
 *
 */
#include "mtlkinc.h"

#include <linux/module.h>
#include <linux/version.h>


#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/udp.h>
#include <linux/seq_file.h>
#include <net/sock.h>
#include <net/ipx.h>
#include <asm/checksum.h>

#include "core.h"
#include "l2nat.h"
#include "mtlkirb.h"
#include "dataex.h"
#include "mtlk_packets.h"

/* 16 PCs plus 1 entry for STA itself */
#define L2NAT_NUM_ENTRIES         17

/* number of indexes in one bucket should be >= L2NAT_NUM_ENTRIES */
#define L2NAT_BUCKET_SIZE         17

/* value of the bucket slot which means the slot is empty */
#define L2NAT_EMPTY_SLOT          0xff

/* buckets of the hash table */
#define L2NAT_BUCKETS_SHIFT       4
#define L2NAT_BUCKETS_NUM         (1UL << L2NAT_BUCKETS_SHIFT)
#define L2NAT_BUCKETS_MASK        (L2NAT_BUCKETS_NUM - 1)

#define L2NAT_AGING_WAIT_INFO     10
#define L2NAT_AGING_WAIT_ARP      10

#define L2NAT_SEND_ARP_LIMIT      (HZ/3)

/* kernel doesn't have structure of the BOOTP/DHCP header
 * so here it is defined according to rfc2131 
 */

struct dhcphdr {
  u8      op;
#define BOOTREQUEST   1
#define BOOTREPLY     2
  u8      htype;
  u8      hlen;
  u8      hops;
  u32     xid;
  u16     secs;
  u16     flags;
#define BOOTP_BRD_FLAG 0x8000
  u32     ciaddr;
  u32     yiaddr;
  u32     siaddr;
  u32     giaddr;
  u8      chaddr[16];
  u8      sname[64];
  u8      file[128];
  u32     magic; /* NB: actually magic is a part of options */
  u8      options[0];
} __attribute__((aligned(1), packed));

/* RFC 2132: 9.1. Requested IP Address
 * RFC 2132: 9.7. Server Identifier
 */

struct dhcpopt_hdr {
  u8      code;
#define DHCP_OPT_REQ_IP   50
#define DHCP_OPT_MSG_TYPE 53
#define DHCP_OPT_SRV_ID   54
#define DHCP_OPT_PAD      0
#define DHCP_OPT_END      255
  u8      len;
} __attribute__((aligned(1), packed));

struct dhcpopt_ip {
  struct dhcpopt_hdr hdr;
  u32                ip;
}__attribute__((aligned(1), packed));

struct dhcpopt_msg_type {
  struct dhcpopt_hdr hdr;
#define DHCPREQUEST     3
  u8                 type;
}__attribute__((aligned(1), packed));

/* magic cookie for DHCP, put at the start of the options field. 
 * it is in network byte order.
 */
#define DHCP_MAGIC 0x63825363

/* udp ports used in client-server communication */
#define BOOTP_SERVER_PORT  67

static const mtlk_guid_t IRBE_ARP_REQ = MTLK_IRB_GUID_ARP_REQ;

static void gen_fake_arp_req(struct nic *nic, u32 sip, u8* smac, u32 dip )
{
  struct mtlk_arp_data arp_data;

  /* just check for zero IPs */
  if (unlikely (!sip || !dip))
    return;

  /* kind of rate limiting for the messages */
  if (jiffies - nic->l2nat_last_arp_sent_timestamp < L2NAT_SEND_ARP_LIMIT)
    return;

  /* fill the arp data payload */
  strncpy(arp_data.ifname, nic->ndev->name, IFNAMSIZ);
  arp_data.daddr = dip;
  arp_data.saddr = sip;
  memcpy(arp_data.smac, smac, ETH_ALEN);  

  mtlk_irb_notify_app(&IRBE_ARP_REQ, &arp_data, sizeof(arp_data));
  nic->l2nat_last_arp_sent_timestamp = jiffies;
  
  ILOG2(GID_L2NAT, "L2NAT: NL sending fake ARP request to %B from %B %Y",
    dip, sip, smac);
}


static inline u32 hash_ip(u32 addr)
{
  /* swap the bytes in IP-address, so that the addresses of hosts in 
   * one IP network (e.g. 192.168.10.X) have different lower bits and
   * therefore more random hash on little-endian platforms
   */
  return hash_32(ntohl(addr), L2NAT_BUCKETS_SHIFT);
}

/* called when user sets default host by means of iwpriv tool */
int mtlk_l2nat_user_set_def_host (struct nic *nic, struct sockaddr *sa)
{
  int res = 0;

  if (sa->sa_family != ARPHRD_ETHER)
    return -EINVAL;

  spin_lock_bh(&nic->l2nat_lock);
  if (is_zero_ether_addr(sa->sa_data)) {
    /* reset default gateway */
    memset(nic->l2nat_default_host, 0, ETH_ALEN);
    nic->l2nat_flags &= ~L2NAT_DEF_SET_BY_USER;
    goto out;
  }
  if (!is_valid_ether_addr(sa->sa_data)) {
    res = -EINVAL;
    goto out;
  }
  /* set new user defined gateway */
  memcpy(nic->l2nat_default_host, sa->sa_data, ETH_ALEN);
  nic->l2nat_flags |= L2NAT_DEF_SET_BY_USER;
out:
  spin_unlock_bh(&nic->l2nat_lock);
  return res;
}

void mtlk_l2nat_get_def_host(struct nic *nic, struct sockaddr *sa)
{
  u8 *pmac = NULL;

  spin_lock_bh(&nic->l2nat_lock);
  if (nic->l2nat_flags & L2NAT_DEF_SET_BY_USER)
    pmac = nic->l2nat_default_host;
  else
    pmac = mtlk_stadb_get_default_host(&nic->slow_ctx->stadb);

  if (pmac)
    memcpy(sa->sa_data, pmac, ETH_ALEN);
  else
    memset(sa->sa_data, 0, ETH_ALEN);
  spin_unlock_bh(&nic->l2nat_lock);
}

/* must be called with nic->l2nat_lock held */
static void delete_entry(struct nic *nic, l2nat_bslot_t del_ind)
{
  struct l2nat_hash_entry *dent = &nic->l2nat_hash_entries[del_ind];
  u32 del_hash;
  int i, ind, last_used = -1, just_freed = -1;
  
  dent->flags |= L2NAT_ENTRY_STOP_AGING_TIMER;
  del_timer(&dent->timer);

  if (!(dent->flags & L2NAT_ENTRY_ACTIVE))
    return;

  /* delete entry from active list */
  list_del(&dent->list);

  /* get the base index of the bucket to which IP is hashed */
  del_hash = hash_ip(dent->ip);
  ind = (del_hash & L2NAT_BUCKETS_MASK) * L2NAT_BUCKET_SIZE;

  /* cycle through this bucket clearing all slots "pointing" to 
   * the entry being deleted; save the location of the first such
   * slot and the last non-empty slot.
   */
  for (i=0; i < L2NAT_BUCKET_SIZE; i++)
    if(nic->l2nat_hash[ind + i] == del_ind) {
      nic->l2nat_hash[ind + i] = L2NAT_EMPTY_SLOT;
      if (just_freed == -1)
         just_freed = ind + i;
    } else if (nic->l2nat_hash[ind + i] != L2NAT_EMPTY_SLOT) {
      last_used = ind + i;
    }

  /* move the last used slot into just freed one */
  if (last_used != -1 && just_freed != -1 && just_freed < last_used) {
    nic->l2nat_hash[just_freed] = nic->l2nat_hash[last_used];
    nic->l2nat_hash[last_used]  = L2NAT_EMPTY_SLOT;
  }

  ILOG2(GID_L2NAT, "L2NAT: DELETED entry %d, %B %Y", del_ind,
        dent->ip, dent->mac);

  /* clear pkt counters */
  dent->pkts_from = 0;

  /* mark as not used and add to free list */
  dent->flags &= ~(L2NAT_ENTRY_ACTIVE);
  list_add(&dent->list, &nic->l2nat_free_entries);
}


/* must be called with nic->l2nat_lock held  */
struct l2nat_hash_entry * find_entry(struct nic *nic, u32 ip)
{
  struct l2nat_hash_entry *ent = NULL;
  u32 hash_res = 0;
  u16 ind;
  int i;

  hash_res = hash_ip(ip);

  ind = (hash_res & L2NAT_BUCKETS_MASK) * L2NAT_BUCKET_SIZE;
  for (i=0; i < L2NAT_BUCKET_SIZE; i++) {
    l2nat_bslot_t ent_ind = nic->l2nat_hash[ind + i];

    if (unlikely(ent_ind == L2NAT_EMPTY_SLOT))
      continue;

    ent = &nic->l2nat_hash_entries[ent_ind];

    /* if entry exists, update stats end exit */
    if (likely(ent->flags & L2NAT_ENTRY_ACTIVE && ent->ip == ip))
      return ent;
  }

  return NULL;
}


static int check_and_add_entry(struct nic *nic, u32 ip, u8 *mac)
{
  u32 hash_res;
  int i, found = 0;
  u16 ind;
  struct l2nat_hash_entry *ent;
  unsigned long cur_jiffies, interval;
  
  spin_lock_bh(&nic->l2nat_lock);
  
  ent = find_entry(nic, ip);
  if(likely(ent != NULL))
    goto update_stats;

  /* entry was not found, try to add one*/

  /* don't add entry for 0.0.0.0 IP address (seen in DHCP Discover) */
  if (!ip) {
    spin_unlock_bh(&nic->l2nat_lock);
    return 0;
  }

  /* first, check whether there are any free entries? */
  if (list_empty(&nic->l2nat_free_entries)) {
    unsigned long max_delta = 0, cur_delta, cur_jiffies;
    l2nat_bslot_t oldest_ind = L2NAT_EMPTY_SLOT;

    /* find the least recently used entry */
    cur_jiffies = jiffies;
    for (i=0; i < L2NAT_NUM_ENTRIES; i++) {
      cur_delta = cur_jiffies - nic->l2nat_hash_entries[i].last_pkt_timestamp;
      if (cur_delta > max_delta) {
        oldest_ind = i;
        max_delta  = cur_delta;
      }
    }

    /* possible if all PCs are actively generating traffic and have delta == 0 */
    if (oldest_ind == L2NAT_EMPTY_SLOT) {
      ILOG2(GID_L2NAT, "can't find the LRU entry, new entry dropped.");
      spin_unlock_bh(&nic->l2nat_lock);
      return 0;
    }

    delete_entry(nic, oldest_ind);
  }
  
  /* get the first from the free list */
  ASSERT(!list_empty(&nic->l2nat_free_entries));
  ent = container_of(nic->l2nat_free_entries.next, struct l2nat_hash_entry, list);
  list_del(&ent->list);

  /* add "pointer" to entry into the hash */
  hash_res = hash_ip(ip);
  ind = (hash_res & L2NAT_BUCKETS_MASK) * L2NAT_BUCKET_SIZE;

  for (i=0; i < L2NAT_BUCKET_SIZE; i++)
    if (nic->l2nat_hash[ind+i] == L2NAT_EMPTY_SLOT) {
      nic->l2nat_hash[ind+i] = (l2nat_bslot_t)(ent - nic->l2nat_hash_entries);
      found = 1;
      break;
    }

  if (!found) {
    ILOG2(GID_L2NAT, "L2NAT: FAILED to add entry, since the bucket is full, %B %Y",
          ip, mac);
    ent->flags &= ~L2NAT_ENTRY_ACTIVE;
    list_add(&ent->list, &nic->l2nat_free_entries);
    spin_unlock_bh(&nic->l2nat_lock);
    return 0;  
  }

  /* set entry's fields */
  ent->ip = ip;
  memcpy(&ent->mac, mac, ETH_ALEN);
  ent->flags = L2NAT_ENTRY_ACTIVE;

  /* add to active list, check default host */
  list_add_tail(&ent->list, &nic->l2nat_active_entries);

  cur_jiffies = jiffies;
  ent->first_pkt_timestamp = cur_jiffies;

  /* run entry timer if aging is enabled */
  if (nic->l2nat_aging_timeout) {
    interval = nic->l2nat_aging_timeout - HZ*(L2NAT_AGING_WAIT_INFO + L2NAT_AGING_WAIT_ARP);
    mod_timer(&ent->timer, cur_jiffies + interval);
  }

  ILOG2(GID_L2NAT, "L2NAT: ADDED entry %d, %B %Y",
       (l2nat_bslot_t)(ent - nic->l2nat_hash_entries), ent->ip, ent->mac);
update_stats:
  ent->last_pkt_timestamp = jiffies;
  ent->pkts_from++;

  spin_unlock_bh(&nic->l2nat_lock);
  return 1;
}


static void entry_aging_timer_function(unsigned long data)
{
  struct l2nat_hash_entry *ent = (struct l2nat_hash_entry *)data;
  unsigned long delta, next_time_offset, send_arp_timeout, get_info_timeout;
  struct nic *nic = ent->nic;

  if (!nic->l2nat_aging_timeout) {
    ILOG2(GID_L2NAT, "L2NAT entry timer function activated while aging is disabled. #%d %B %Y",
      (int)(ent - nic->l2nat_hash_entries), ent->ip, ent->mac);
    return;
  }

  spin_lock_bh(&nic->l2nat_lock);

  /* do not reschedule itself if requested */
  if(ent->flags & L2NAT_ENTRY_STOP_AGING_TIMER) {
    ent->flags &= ~L2NAT_ENTRY_STOP_AGING_TIMER;
    goto out;
  }

  delta = jiffies - ent->last_pkt_timestamp;
  send_arp_timeout = nic->l2nat_aging_timeout - L2NAT_AGING_WAIT_ARP * HZ;
  get_info_timeout = send_arp_timeout - L2NAT_AGING_WAIT_INFO * HZ;

  /* delete entry if its inactive longer than allowed */
  if (delta >= nic->l2nat_aging_timeout) {
    ILOG2(GID_L2NAT, "L2NAT: TIMER deleting entry %B inactive %u" , ent->ip,
          jiffies_to_msecs((unsigned long)(jiffies - ent->last_pkt_timestamp)));

    delete_entry(nic, (ent - nic->l2nat_hash_entries));
    goto out;
  }

  if (delta < get_info_timeout) {
    /* no need for any actions, reschedule on the moment
     * of next arp attempt, clear waiting flags if any.
     */
    next_time_offset = get_info_timeout - delta;
    ILOG2(GID_L2NAT, "L2NAT: TIMER entry %B is active, offset %lu, delta %lu",
          ent->ip, next_time_offset, delta);
    goto modify_out;
  }
  
  if (delta < send_arp_timeout ) {
    nic->l2nat_flags |= L2NAT_NEED_ARP_INFO;
    next_time_offset = L2NAT_AGING_WAIT_INFO * HZ;
    ILOG2(GID_L2NAT, "L2NAT: TIMER request ip-mac info to generate arp to %B, offset %lu, delta %lu", 
          ent->ip, next_time_offset, delta);
    goto modify_out;
  }

  gen_fake_arp_req(nic, nic->l2nat_ip_for_arp.s_addr, nic->l2nat_mac_for_arp, 
                                                                ent->ip);
  next_time_offset = L2NAT_AGING_WAIT_ARP * HZ;
  ILOG2(GID_L2NAT, "L2NAT: TIMER sent arp to %B, offset %lu", 
                        ent->ip, next_time_offset);

modify_out:
  mod_timer(&ent->timer, jiffies + next_time_offset);
out:
  spin_unlock_bh(&nic->l2nat_lock);
}


/* prepare all needed data structures */
MTLK_INIT_STEPS_LIST_BEGIN(l2nat)
  MTLK_INIT_STEPS_LIST_ENTRY(l2nat, ALLOC_HASH_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(l2nat, ALLOC_IPMAC_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(l2nat, TIMER_INIT)
MTLK_INIT_INNER_STEPS_BEGIN(l2nat)
MTLK_INIT_STEPS_LIST_END(l2nat);

int mtlk_l2nat_init (struct nic *nic)
{
  struct l2nat_hash_entry  *ent = NULL;
  l2nat_bslot_t *pind = NULL;
  int i;

  /* init lists of free and active entries in L2NAT table */
  INIT_LIST_HEAD(&nic->l2nat_free_entries);
  INIT_LIST_HEAD(&nic->l2nat_active_entries);

  MTLK_INIT_TRY(l2nat, MTLK_OBJ_PTR(nic))
    /* allocate hash table of indexes of entries */
    MTLK_INIT_STEP_EX(l2nat, ALLOC_HASH_INIT, MTLK_OBJ_PTR(nic), 
                      kmalloc_tag, 
                      (sizeof(*pind)*L2NAT_BUCKETS_NUM*L2NAT_BUCKET_SIZE, GFP_KERNEL, MTLK_MEM_TAG_L2NAT),
                      pind, 
                      pind,
                      MTLK_MEM_TAG_L2NAT);

    memset(pind, 0xFF, sizeof(*pind) * L2NAT_BUCKETS_NUM * L2NAT_BUCKET_SIZE);
    
    /* allocate IP-MAC table entries */
    MTLK_INIT_STEP_EX(l2nat, ALLOC_IPMAC_INIT, MTLK_OBJ_PTR(nic), 
                      kmalloc_tag, 
                      (sizeof(*ent)*L2NAT_NUM_ENTRIES, GFP_KERNEL, MTLK_MEM_TAG_L2NAT),
                      ent, 
                      ent,
                      MTLK_MEM_TAG_L2NAT);

    memset(ent, 0, sizeof(*ent) * L2NAT_NUM_ENTRIES);                      
 
    /* set appropriate pointers in nic */
    nic->l2nat_hash_entries = ent;
    nic->l2nat_hash         = pind;

    /* init all entries */
    for (i = 0; i < L2NAT_NUM_ENTRIES; i++) {
      ent = &nic->l2nat_hash_entries[i];
      list_add_tail(&ent->list, &nic->l2nat_free_entries);
      ent->nic = nic;
      MTLK_INIT_STEP_VOID_LOOP(l2nat, TIMER_INIT, MTLK_OBJ_PTR(nic),
                               init_timer, (&ent->timer));
      ent->timer.function = entry_aging_timer_function;
      ent->timer.data = (unsigned long)ent;
    }
                          
    nic->l2nat_last_arp_sent_timestamp = jiffies;

    MTLK_INIT_FINALLY(l2nat, MTLK_OBJ_PTR(nic))
  MTLK_INIT_RETURN(l2nat, MTLK_OBJ_PTR(nic), mtlk_l2nat_cleanup, (nic));
}


void mtlk_l2nat_cleanup (struct nic *nic)
{
  int i;

  /* free all allocated resources, reinit pointers&list */
  MTLK_CLEANUP_BEGIN(l2nat, MTLK_OBJ_PTR(nic))

    for (i = 0; MTLK_CLEANUP_ITERATONS_LEFT(MTLK_OBJ_PTR(nic), TIMER_INIT) > 0; i++) {
      MTLK_CLEANUP_STEP_LOOP(l2nat, TIMER_INIT, MTLK_OBJ_PTR(nic),
                             del_timer, (&nic->l2nat_hash_entries[i].timer));
    }

    MTLK_CLEANUP_STEP(l2nat, ALLOC_IPMAC_INIT, MTLK_OBJ_PTR(nic),
                      kfree_tag, (nic->l2nat_hash_entries));
    
    nic->l2nat_hash_entries = NULL;
    
    MTLK_CLEANUP_STEP(l2nat, ALLOC_HASH_INIT, MTLK_OBJ_PTR(nic),
                      kfree_tag, (nic->l2nat_hash));
                      
    nic->l2nat_hash = NULL;                  
                      
  MTLK_CLEANUP_END(l2nat, MTLK_OBJ_PTR(nic));  
  
  INIT_LIST_HEAD(&nic->l2nat_free_entries);
  INIT_LIST_HEAD(&nic->l2nat_active_entries);

}

static inline
struct dhcpopt_hdr *dhcp_req_find_opt(struct sk_buff *skb,
  struct dhcpopt_hdr *hdr, u8 opt_code)
{
  ILOG4(GID_L2NAT, "Looking for %d", opt_code);
  while (hdr->code != DHCP_OPT_END && hdr->code != DHCP_OPT_PAD &&
         hdr->code != opt_code && (u8 *)hdr < skb_tail_pointer(skb)) {
    ILOG4(GID_L2NAT, "option %d (0x%02x), length %d", hdr->code, hdr->code, hdr->len);
    hdr = (struct dhcpopt_hdr *)((u8 *)hdr + hdr->len + sizeof(*hdr));
  }
  if (hdr->code == opt_code)
    return hdr;
  return NULL;
}

static inline struct sk_buff *
dhcp_req_update (struct sk_buff *skb, ptrdiff_t offset)
{
  struct iphdr *ip_header;
  struct udphdr *udp_header;
  struct dhcphdr *dhcp_header;
  struct dhcpopt_hdr *opt_header;
  struct dhcpopt_ip *opt_ip;
  struct sk_buff * nskb;
  int extra_size;
  BOOL      add_req_ip_opt;
  BOOL      add_srv_id_opt;

  ip_header = (struct iphdr *)(skb->data + offset);

  /* check the protocol to be UDP */
  if (ip_header->protocol != IPPROTO_UDP)
    return skb;

  udp_header = (struct udphdr *)((char *)ip_header + ip_header->ihl * 4);

  /* destination port should be 67 (bootps) */
  if (likely(udp_header->dest != __constant_htons(BOOTP_SERVER_PORT)))
    return skb;

  dhcp_header = (struct dhcphdr *)((char *)udp_header + sizeof(*udp_header));

  /* should be bootp request, no broadcast flag set, 
   * and have dhcp magic number in .options */
  if (dhcp_header->op != BOOTREQUEST ||
      dhcp_header->htype != ARPHRD_ETHER ||
      dhcp_header->magic != __constant_ntohl(DHCP_MAGIC))
    return skb;

  add_req_ip_opt = FALSE;
  add_srv_id_opt = FALSE;
  extra_size = 0;

  /* we're up to make a change in the packet, so unshare */
   /* GS: we're always unshare (alloc and copy) SKB so lets expand its anyway for
    * adding new options in case when DstAddr!=Broadcast */
   if (ip_header->daddr == __constant_htonl(INADDR_BROADCAST))
   {
     /*
      * Don't add DHCP_OPT_REQ_IP and DHCP_OPT_SRV_ID to DHCP request
      * with limited broadcast destination IP (REBOOT-INIT DHCP state).
      */
     nskb = skb_unshare(skb, GFP_ATOMIC);

     /* check DHCP message type */
     opt_header = dhcp_req_find_opt(skb, (struct dhcpopt_hdr *)dhcp_header->options, DHCP_OPT_MSG_TYPE);
     if (opt_header)
     {
       ILOG4("DHCP message type is %d %B %B", ((struct dhcpopt_msg_type *)opt_header)->type,
           ip_header->saddr, ip_header->daddr);
     }

   }
   else
   {
     do {
       /* no options */
       opt_header = dhcp_req_find_opt(skb, (struct dhcpopt_hdr *)dhcp_header->options, DHCP_OPT_END);
       if (!opt_header)
         break;

       /* check DHCP message type */
       opt_header = dhcp_req_find_opt(skb, (struct dhcpopt_hdr *)dhcp_header->options, DHCP_OPT_MSG_TYPE);
       if (!opt_header)
         break;

       ILOG4("DHCP message type is %d %B %B", ((struct dhcpopt_msg_type *)opt_header)->type,
           ip_header->saddr, ip_header->daddr);
       if (((struct dhcpopt_msg_type *)opt_header)->type != DHCPREQUEST)
         break;

       /* Check if the options should be added*/
       if (!dhcp_req_find_opt(skb, (struct dhcpopt_hdr *)dhcp_header->options, DHCP_OPT_REQ_IP))
       {
         add_req_ip_opt = TRUE;
         extra_size += sizeof(struct dhcpopt_ip);
       }

       if (!dhcp_req_find_opt(skb, (struct dhcpopt_hdr *)dhcp_header->options, DHCP_OPT_SRV_ID))
       {
         add_srv_id_opt = TRUE;
         extra_size += sizeof(struct dhcpopt_ip);
       }

       if (skb->len + extra_size > 65535)
       {
         add_req_ip_opt = FALSE;
         add_srv_id_opt = FALSE;
         extra_size = 0;
         break;
       }
     } while (0);

     if (!extra_size) {
       extra_size = skb_tailroom(skb);
     }
     /* Allocate memory for options DHCP_OPT_REQ_IP and DHCP_OPT_SRV_ID*/
     nskb = skb_copy_expand(
         skb, skb_headroom(skb), extra_size, GFP_ATOMIC);
     /* transfer socket to new skb */
     if (likely(nskb != NULL))
     {
       if (skb->sk)
         skb_set_owner_w(nskb, skb->sk);
       kfree_skb(skb);
     }
   }

   if (likely(nskb != NULL)) {
     skb = nskb;

     /* update pointers after unshare */
     ip_header = (struct iphdr*)(skb->data + offset);
     udp_header = (struct udphdr *)((char *)ip_header + ip_header->ihl * 4);
     dhcp_header = (struct dhcphdr *)((char *)udp_header + sizeof(*udp_header));
   }

  if (!(dhcp_header->flags & __constant_htons(BOOTP_BRD_FLAG))) {
    /* set the broadcast flag */
    dhcp_header->flags |= __constant_ntohs(BOOTP_BRD_FLAG);
    /* change checksum according to added broadcast flag.
     * ip_decrease_ttl was used as an example
     */
    if (udp_header->check) {
      u32 check;

      check = (u32)udp_header->check;
      check += (u32)__constant_ntohs(~((u16)BOOTP_BRD_FLAG));
      udp_header->check = (u16)( check + ((check>=0xffff) ? 1 : 0) );

      /* in udp 0 csum means no checksumming,
       * so if csum is 0 it should be set to 0xffff */
      if (!udp_header->check)
        udp_header->check = 0xffff;
    }
  }

  /* At this point we always have enough space for additional DHCP options */

  /* all DHCP request packet modification which is done below are required
   * to force DHCP server to send DHCP ACK as broadcast message*/

  /* adjust to new size */
  skb_put(skb, extra_size);
  extra_size = 0;

  /* Add additional field "requested IP" and "server id" -
   * it's resolved issues with HW like Linksys WRT610N, WRT160*/
  if (TRUE == add_req_ip_opt) {
    ILOG4(GID_L2NAT, "Adding requested ip");
    opt_header = dhcp_req_find_opt(skb, (struct dhcpopt_hdr *)dhcp_header->options, DHCP_OPT_END);
    opt_ip = (struct dhcpopt_ip *)opt_header;
    /* 9.1. Requested IP Address */
    opt_ip[0].hdr.code = DHCP_OPT_REQ_IP;
    opt_ip[0].hdr.len = 4;
    opt_ip[0].ip = dhcp_header->ciaddr;
    opt_ip[1].hdr.code = DHCP_OPT_END;

    extra_size += sizeof(struct dhcpopt_ip);
  }

  if (TRUE == add_srv_id_opt) {
    ILOG4(GID_L2NAT, "Adding server id");
    opt_header = dhcp_req_find_opt(skb, (struct dhcpopt_hdr *)dhcp_header->options, DHCP_OPT_END);
    opt_ip = (struct dhcpopt_ip *)opt_header;
    /* 9.7. Server Identifier */
    opt_ip[0].hdr.code = DHCP_OPT_SRV_ID;
    opt_ip[0].hdr.len = 4;
    opt_ip[0].ip = ip_header->daddr;
    opt_ip[1].hdr.code = DHCP_OPT_END;

    extra_size += sizeof(struct dhcpopt_ip);
  }

  /* cleanup CI address fields DHCP Request - it's resolved issues with
   * HW like Cisco E2000 router */
  dhcp_header->ciaddr = 0;

  /* Update size fields in headers */
  ip_header->tot_len = htons(ntohs(ip_header->tot_len) + extra_size);
  udp_header->len = htons(ntohs(udp_header->len) + extra_size);

  /* recalculate check sums */
  ip_header->check = 0;
  ip_header->check = ip_fast_csum((unsigned char *)ip_header, ip_header->ihl);
  if (udp_header->check) {
    int data_len = ntohs(udp_header->len);
    udp_header->check = 0;
    udp_header->check = csum_tcpudp_magic(ip_header->saddr,
      ip_header->daddr, data_len, IPPROTO_UDP,
      csum_partial((char *)udp_header, data_len, 0));
    if (udp_header->check == 0)
      udp_header->check = -1;
  }

  return skb;
}

struct vlanhdr {
  uint16 h_vlan_TCI;
  uint16 h_vlan_encapsulated_proto;
};

struct sk_buff * mtlk_l2nat_on_tx (struct nic *nic, struct sk_buff *skb)
{
  struct ethhdr *ether_header;
  struct vlanhdr *vlan_header;
  struct ipxhdr *ipx_header;
  struct arphdr *arp_header;
  struct iphdr *ip_header;
  uint16 proto_ne;
  ptrdiff_t offset = sizeof(struct ethhdr);

  /* get the type of the frame and set ip-mac pair to be checked
   * (and possibly added) in table
   */
  ether_header = (struct ethhdr *)skb->data;
  proto_ne = ether_header->h_proto;

analyse:

  switch (proto_ne) {

  case __constant_htons(ETH_P_8021Q):
    vlan_header = (struct vlanhdr *)(skb->data + offset);
    proto_ne = vlan_header->h_vlan_encapsulated_proto;
    offset += sizeof(struct vlanhdr);
    goto analyse;

  case __constant_htons(ETH_P_IP):
    ip_header = (struct iphdr*)(skb->data + offset);

    /* check in the table, add entry if needed.
     * ip_header->saddr assumed to be 4-bytes aligned
     */
    check_and_add_entry(nic, ip_header->saddr, ether_header->h_source);

    /* if dhcp req set broadcast flag,  add DHCP opts if needed
     * XXX: ether_header and ip_header are invalid after this point */
    skb = dhcp_req_update(skb, offset);

    break;

  case __constant_htons(ETH_P_ARP):
    arp_header = (struct arphdr *)(skb->data + offset);

    if (arp_header->ar_pro == htons(ETH_P_IP) &&
        arp_header->ar_hrd == htons(ARPHRD_ETHER)) {

      struct sk_buff * nskb;
      u8 *p;
      u32 ip;

      /* we're going to modify packet, so unshare
       * XXX: ether_header and arp_header are invalid after this point */
      nskb = skb_unshare(skb, GFP_ATOMIC);
      if (likely(nskb != NULL)) {
        skb = nskb;

        /* update the arp_header after unshare */
        arp_header = (struct arphdr *)(skb->data + offset);
      }

      p = (char*)arp_header + sizeof(*arp_header);

      /* save ip and mac */

      /* sender IP is 2-bytes aligned, so copy it to temporal storage
       * to prevent unaligned access with subsequent data abort handling
       */
      memcpy(&ip, p + ETH_ALEN, sizeof(ip));

      /* check in the table, add entry if needed */
      check_and_add_entry(nic, ip, p);

      /* change src mac */
      memcpy(p, nic->mac_addr, ETH_ALEN);
    }
    break;

  case __constant_htons(ETH_P_IPX):
    {
      /* we're going to modify packet, so unshare
       * XXX: ether_header and arp_header are invalid after this point */
      struct sk_buff * nskb = skb_unshare(skb, GFP_ATOMIC);
      if (likely(nskb != NULL))
        skb = nskb;

      ipx_header = (struct ipxhdr *)(skb->data + offset);

      /* set source node to wilreless MAC address */
      memcpy(ipx_header->ipx_source.node, nic->mac_addr, IPX_NODE_LEN);
    }
    break;

  default:
    /* 802.3 frame header with Length in T/E field */
    if (ntohs(proto_ne) <= ETH_DATA_LEN) {
      mtlk_snap_hdr_t *snap_hdr;
      proto_ne = *(u16 *)(skb->data + offset);
      switch (proto_ne) {
      case 0xFFFF: /* checksum of IPX over 802.3 with raw encapsulation */
        proto_ne = __constant_htons(ETH_P_IPX);
        goto analyse;
      case 0xE0E0: /* part of LLC header used with IPX over 802.2 Novel encapsulation */
        proto_ne = __constant_htons(ETH_P_IPX);
        offset += sizeof(mtlk_llc_hdr_t); /* skip LLC header */
        goto analyse;
      case 0xAAAA: /* part oh LLC/SNAP header used with (possible) IPX over 802.2 SNAP encapsulation */
        snap_hdr = (mtlk_snap_hdr_t *)(skb->data + offset + sizeof(mtlk_llc_hdr_t));
        proto_ne = snap_hdr->ether_type;
        offset += sizeof(mtlk_llc_hdr_t) + sizeof(mtlk_snap_hdr_t); /* skip LLC/SNAP header */
        goto analyse;
      default:
        break;
      }
    }
    break;
  }

  return skb;
}

void mtlk_l2nat_on_rx (struct nic *nic, struct sk_buff *skb)
{
  struct l2nat_hash_entry *ent;
  struct ethhdr *ether_header;
  struct vlanhdr *vlan_header;
  struct arphdr *arp_header;
  struct iphdr  *ip_header;
  struct ipxhdr *ipx_header;
  u32 ip;
  uint16 proto_ne;
  u8 *pmac = NULL, *p;
  ptrdiff_t offset = sizeof(struct ethhdr);

  ether_header = (struct ethhdr *)skb->data;
  proto_ne = ether_header->h_proto;
  
  if (mtlk_osal_eth_is_group_addr(ether_header->h_dest))
    return;

  spin_lock_bh(&nic->l2nat_lock);

  if (likely(nic->l2nat_flags & L2NAT_DEF_SET_BY_USER))
    pmac = nic->l2nat_default_host;
  else
    pmac = mtlk_stadb_get_default_host(&nic->slow_ctx->stadb);

analyse:
    
  switch(proto_ne) {
  case __constant_htons(ETH_P_PAE):
    spin_unlock_bh(&nic->l2nat_lock);
    return;
  case __constant_htons(ETH_P_8021Q):
    vlan_header = (struct vlanhdr *)(skb->data + offset);
    proto_ne = vlan_header->h_vlan_encapsulated_proto;
    offset += sizeof(struct vlanhdr);
    goto analyse;
  case __constant_htons(ETH_P_IP):

    ip_header = (struct iphdr*)(skb->data + offset);

    ent = find_entry(nic, ip_header->daddr);
    if (unlikely(!ent)) {
      gen_fake_arp_req(nic, ip_header->saddr, ether_header->h_source, ip_header->daddr);
      break;
    }

    if (unlikely(nic->l2nat_flags & L2NAT_NEED_ARP_INFO)) {
      /* copy IP and MAC to be used when generating*/
      memcpy(&nic->l2nat_mac_for_arp, ether_header->h_source, ETH_ALEN);
      nic->l2nat_ip_for_arp.s_addr = ip_header->saddr;

      nic->l2nat_flags &= ~L2NAT_NEED_ARP_INFO;
      nic->l2nat_flags |= L2NAT_GOT_ARP_INFO;
    }

    pmac = ent->mac;
        
    break;
  case __constant_htons(ETH_P_ARP):

    arp_header = (struct arphdr*)(skb->data + offset);
    if (arp_header->ar_pro != htons(ETH_P_IP) ||
        arp_header->ar_hrd != htons(ARPHRD_ETHER))
      break;

    /* set p to start of arp payload*/
    p = (u8*)arp_header + sizeof(*arp_header); 
    /* copy target ip into local variable */
    memcpy(&ip, p + 2*ETH_ALEN + sizeof(ip), sizeof(ip));

    /* find entry, set dest mac */
    ent = find_entry(nic, ip);
    if (!ent) {
      gen_fake_arp_req(nic, *(u32*)(p + ETH_ALEN) , p, ip);
      break;
    }

    switch (ntohs(arp_header->ar_op)) {
    case ARPOP_REPLY:
      /* change target hw addr to one from found entry */
      memcpy(p + ETH_ALEN + sizeof(ip), &ent->mac, ETH_ALEN);
      /* do not break, so that the pmac will be set later */
    case ARPOP_REQUEST: /* arp solicit requests */
      pmac = ent->mac;
      break;
    default:
      break;
    }

    break;

  case __constant_htons(ETH_P_IPX):
    if (likely(pmac != NULL)) {
      ipx_header = (struct ipxhdr *)(skb->data + offset);
      /* set dest node node to default GW MAC address (pmac is already set to it) */
      memcpy(ipx_header->ipx_dest.node, pmac, IPX_NODE_LEN);
    }
    break;

  default:
    /* 802.3 frame header with Length in T/E field */
    if (ntohs(proto_ne) <= ETH_DATA_LEN) {
      mtlk_snap_hdr_t *snap_hdr;
      proto_ne = *(u16 *)(skb->data + offset);
      switch (proto_ne) {
      case 0xFFFF: /* checksum of IPX over 802.3 with raw encapsulation */
        proto_ne = __constant_htons(ETH_P_IPX);
        goto analyse;
      case 0xE0E0: /* part of LLC header used with IPX over 802.2 Novel encapsulation */
        proto_ne = __constant_htons(ETH_P_IPX);
        offset += sizeof(mtlk_llc_hdr_t); /* skip LLC header */
        goto analyse;
      case 0xAAAA: /* part oh LLC/SNAP header used with (possible) IPX over 802.2 SNAP encapsulation */
        snap_hdr = (mtlk_snap_hdr_t *)(skb->data + offset + sizeof(mtlk_llc_hdr_t));
        proto_ne = snap_hdr->ether_type;
        offset += sizeof(mtlk_llc_hdr_t) + sizeof(mtlk_snap_hdr_t); /* skip LLC/SNAP header */
        goto analyse;
      default:
        break;
      }
    }
    break;
  }
  
  if (likely(pmac != NULL))
    memcpy(&ether_header->h_dest, pmac, ETH_ALEN);

  spin_unlock_bh(&nic->l2nat_lock);

  return;
}

static int calc_bslots(struct nic *nic, int bucket_start)
{
  int i, num = 0;

  for (i=0; i < L2NAT_BUCKET_SIZE; i++)
    if (nic->l2nat_hash[bucket_start + i] != L2NAT_EMPTY_SLOT)
      num ++;

  return num;
}


void mtlk_l2nat_show_stats(struct seq_file *s, void *v, mtlk_core_t *nic)
{
  struct l2nat_hash_entry *ent = nic->l2nat_hash_entries;
  int i;
  char str[20];
  int blens[9]= {0,}, blen;
  
  spin_lock_bh(&nic->l2nat_lock);

  if ((nic->bridge_mode != BR_MODE_L2NAT) || !ent)
    goto out;

  seq_printf(s, "\n"
      "L2NAT Statistics\n"
      "\n"
      "IP               MAC                Inactive (ms)       Age(secs)  Pkts from\n"
      "----------------------------------------------------------------------------\n");

  for(i=0; i<L2NAT_NUM_ENTRIES; i++, ent++)
    if (ent->flags & L2NAT_ENTRY_ACTIVE) {
      snprintf(str, sizeof(str), NIPQUAD_FMT, NIPQUAD(ent->ip));
      seq_printf(s, "%-16s " MAC_FMT "%15u %15lu %10u\n", str, MAC_ARG(&ent->mac),
            jiffies_to_msecs((unsigned long)(jiffies - ent->last_pkt_timestamp)),
            ((unsigned long)(jiffies - ent->first_pkt_timestamp))/HZ,
            ent->pkts_from);
    }

  seq_printf(s,
      "----------------------------------------------------------------------------\n");


  /* calculate bucket length distribution */
  for (i=0; i < L2NAT_BUCKETS_NUM; i++) {
    blen = calc_bslots(nic, i * L2NAT_BUCKET_SIZE * sizeof(l2nat_bslot_t));
    if (blen < ARRAY_SIZE(blens) - 1)
      blens[blen]++;
    else
      blens[ARRAY_SIZE(blens)-1]++;
  }

  /* print out distribution table calculated above */
  seq_printf(s, "\n\nHash table statistics\n\n"
                "Bucket length     :");

  for (i=0; i < ARRAY_SIZE(blens) - 1; i++)
    seq_printf(s, "   %3d", i);
  seq_printf(s, "   >%2d\n", (int)(ARRAY_SIZE(blens) - 2) );

  seq_printf(s, "Number of buckets :");

  for (i=0; i < ARRAY_SIZE(blens) ; i++)
    seq_printf(s, "   %3d", blens[i]);
  seq_printf(s,"\n\n");


out:
  spin_unlock_bh(&nic->l2nat_lock);
}

void mtlk_l2nat_clear_table(struct nic *nic) 
{
  int i;

  spin_lock_bh(&nic->l2nat_lock);

  for(i=0; i<L2NAT_NUM_ENTRIES; i++) {
    delete_entry(nic,i);
  }

  spin_unlock_bh(&nic->l2nat_lock);
}
