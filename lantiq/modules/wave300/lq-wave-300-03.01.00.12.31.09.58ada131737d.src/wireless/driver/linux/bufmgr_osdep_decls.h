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
 * Linux driver implementation of packet buffers for shared code.
 *
 *
 */
#if !defined(SAFE_PLACE_TO_INCLUDE_BUFMGR_OSDEP_DECLS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_BUFMGR_OSDEP_... */

#undef SAFE_PLACE_TO_INCLUDE_BUFMGR_OSDEP_DECLS

typedef struct sk_buff_head mtlk_buflist_t;
typedef struct sk_buff      mtlk_buflist_entry_t;

// sk_buff flags can be set of following
#define SKB_DIRECTED    0x01u   // this unit receiver address (802.11n ADDR1)
#define SKB_UNICAST     0x02u   // unicast destination address (802.3 DA)
#define SKB_MULTICAST   0x04u   // multicast destination address (802.3 DA)
#define SKB_BROADCAST   0x08u   // broadcast destination address (802.3 DA)
#define SKB_FORWARD     0x10u   // sk_buff should be forwarded
#define SKB_CONSUME     0x20u   // sk_buff should be consumed by OS
#define SKB_RMCAST      0x40u   // reliable multicast used
#define SKB_URGENT      0x100u  // this skb describes urgent data

typedef struct skb_ext {
  mtlk_atomic_t ref_cnt;
  uint16        ac;
  BOOL          front;
} skb_ext_t;

typedef struct _sta_entry sta_entry;

// located in skb->cb[] (size 40 bytes max in 2.6.11)
// Automatically allocated along with skb.
typedef struct skb_private {    // located in skb->cb[] (size 40 bytes max)
  uint32 flags;
  sta_entry *dst_sta;           // destination STA (NULL if unknown)
  sta_entry *src_sta;           // source STA
  uint8 rsc_buf[8];             // unparsed (TKIP/CCMP) storage for Replay Sequence Counter
  skb_ext_t *extra;
  struct sk_buff *sub_frame;    // pointer to skb subframe for a-msdu
  uint8 rsn_bits;
#ifdef MTLK_DEBUG_CHARIOT_OOO
  uint16 seq_num;
  uint8 seq_qos;
#endif
} __attribute__((aligned(1), packed)) skb_priv;

