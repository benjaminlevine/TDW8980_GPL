/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id$
 *
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *
 * L2NAT driver subsystem.
 *
 */

#ifndef __L2NAT_H__
#define __L2NAT_H__

struct l2nat_hash_entry {
  u32               ip;
  u8                mac[ETH_ALEN];
  u16               flags;
#define L2NAT_ENTRY_ACTIVE                        0x1
#define L2NAT_ENTRY_STOP_AGING_TIMER              0x2

  u32               pkts_from;

  unsigned long     last_pkt_timestamp;
  unsigned long     first_pkt_timestamp;

  struct list_head  list;

  struct timer_list timer;
  struct nic   *nic;
};


int  mtlk_l2nat_init (struct nic *nic);
void mtlk_l2nat_on_rx (struct nic *nic, struct sk_buff *skb);
struct sk_buff * mtlk_l2nat_on_tx (struct nic *nic, struct sk_buff *skb);
void mtlk_l2nat_cleanup (struct nic *nic);
void mtlk_l2nat_show_stats(struct seq_file *s, void *v, mtlk_core_t *nic);
void mtlk_l2nat_clear_table (struct nic *nic);
int mtlk_l2nat_user_set_def_host (struct nic *nic, struct sockaddr *sa);
void mtlk_l2nat_get_def_host(struct nic *nic, struct sockaddr *sa);
void mtlk_l2nat_user_set_local_mac (struct nic *nic, struct sockaddr *sa);

#endif /* __L2NAT_H__ */
