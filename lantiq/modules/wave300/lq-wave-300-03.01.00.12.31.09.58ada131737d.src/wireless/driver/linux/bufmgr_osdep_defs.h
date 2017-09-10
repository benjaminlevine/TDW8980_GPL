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
#if !defined(SAFE_PLACE_TO_INCLUDE_BUFMGR_OSDEP_DEFS)
#error "You shouldn't include this file directly!"
#endif

#undef SAFE_PLACE_TO_INCLUDE_BUFMGR_OSDEP_DEFS

static __INLINE int
mtlk_bufmgr_read (mtlk_buffer_t *pbuffer, 
                  uint32         offset, 
                  uint32         length, 
                  uint8         *destination)
{
  struct skb_private * skb_priv;

  ASSERT(pbuffer != NULL);
  skb_priv = (struct skb_private*)(pbuffer->cb);

  if (unlikely(skb_copy_bits(pbuffer, 
                             offset, 
                             destination, 
                             length)))
      return MTLK_ERR_UNKNOWN;

  return MTLK_ERR_OK;
}

/* Since Metlink driver doesn't set NETIF_F_SG and NETIF_F_FRAGLIST
 * feature flags, kernel network subsystem linearizes all buffers
 * before calling driver's transmit function, therefore memcpy()
 * can be used here. Actually, the right function here should be
 * skb_strore_bits(), but it's not supported in older kernels.
 */
static __INLINE int
mtlk_bufmgr_write (mtlk_buffer_t *pbuffer, 
                   uint32         offset, 
                   uint32         length, 
                   uint8         *source)
{
  struct skb_private * skb_priv;

  ASSERT(pbuffer != NULL);
  skb_priv = (struct skb_private*)(pbuffer->cb);

  if (unlikely((offset + length) > pbuffer->len))
    return MTLK_ERR_UNKNOWN;

  memcpy(pbuffer->data + offset, source, length);
  return MTLK_ERR_OK;
}

/* Again, skb is assumed to be linearized (i.e. skb->data_len == 0) */
static __INLINE int32
mtlk_bufmgr_query_length (mtlk_buffer_t *pbuffer)
{
  ASSERT(pbuffer != NULL);
  return pbuffer->len;
}

static __INLINE uint16
mtlk_bufmgr_get_priority (mtlk_buffer_t *pbuffer)
{
  ASSERT(pbuffer != NULL);
  return pbuffer->priority;
}

static __INLINE void
mtlk_bufmgr_set_priority (mtlk_buffer_t *pbuffer, 
                          uint16         priority)
{
  ASSERT(pbuffer != NULL);
  pbuffer->priority = priority;
}

static __INLINE uint8
mtlk_bufmgr_is_urgent (mtlk_buffer_t *pbuffer)
{
  struct skb_private * skb_priv;
  ASSERT(pbuffer != NULL);
  skb_priv = (struct skb_private*)(pbuffer->cb);
  return (skb_priv->flags & SKB_URGENT) ? 1 : 0;
}

static __INLINE void
mtlk_bufmgr_set_urgency (mtlk_buffer_t *pbuffer)
{
  struct skb_private * skb_priv;
  ASSERT(pbuffer != NULL);
  skb_priv = (struct skb_private*)(pbuffer->cb);
  skb_priv->flags |= SKB_URGENT;
}

static __INLINE void
mtlk_bufmgr_clear_urgency (mtlk_buffer_t *pbuffer)
{
  struct skb_private * skb_priv;
  ASSERT(pbuffer != NULL);
  skb_priv = (struct skb_private*)(pbuffer->cb);
  skb_priv->flags &= ~SKB_URGENT;
}

static __INLINE void *
mtlk_bufmgr_get_cancel_id (mtlk_buffer_t *pbuffer)
{
  ASSERT(pbuffer != NULL);
  return NULL;
}

static __INLINE void
mtlk_bufmgr_set_cancel_id (mtlk_buffer_t *pbuffer, 
                           void          *cancelid)
{
  ASSERT(pbuffer != NULL);
}


/****************
 * buffer lists *
 ****************/

static __INLINE void
mtlk_buflist_init (mtlk_buflist_t *pbuflist)
{
  ASSERT(pbuflist != NULL);
  skb_queue_head_init(pbuflist);
}

static __INLINE void
mtlk_buflist_cleanup (mtlk_buflist_t *pbuflist)
{
  ASSERT(pbuflist != NULL);
  ASSERT(skb_queue_empty(pbuflist));
  skb_queue_head_init(pbuflist);
}

static __INLINE void
mtlk_buflist_push_front (mtlk_buflist_t       *pbuflist,
                         mtlk_buflist_entry_t *pentry)
{
  ASSERT(pbuflist != NULL);
  ASSERT(pentry != NULL);
  __skb_queue_head(pbuflist, pentry);
}

static __INLINE mtlk_buflist_entry_t *
mtlk_buflist_pop_front (mtlk_buflist_t *pbuflist)
{
  ASSERT(pbuflist != NULL);
  return __skb_dequeue(pbuflist);
}

static __INLINE void
mtlk_buflist_push_back (mtlk_buflist_t       *pbuflist,
                        mtlk_buflist_entry_t *pentry)
{
  ASSERT(pbuflist != NULL);
  ASSERT(pentry != NULL);
  __skb_queue_tail(pbuflist, pentry);
}

static __INLINE mtlk_buflist_entry_t *
mtlk_buflist_pop_back (mtlk_buflist_t *pbuflist)
{
  ASSERT(pbuflist != NULL);
  return __skb_dequeue_tail(pbuflist);
}

static __INLINE mtlk_buflist_entry_t *
mtlk_buflist_remove_entry (mtlk_buflist_t       *pbuflist,
                           mtlk_buflist_entry_t *pentry)
{
  struct sk_buff *ret_skb;
  ASSERT(pbuflist != NULL);
  ASSERT(pentry != NULL);
  ret_skb = pentry->next;
  __skb_unlink(pentry, pbuflist);
  return ret_skb;
}

static __INLINE mtlk_buflist_entry_t *
mtlk_buflist_head (mtlk_buflist_t *pbuflist)
{
  ASSERT(pbuflist != NULL);
  return (mtlk_buflist_entry_t*)pbuflist; 
}

static __INLINE int8
mtlk_buflist_is_empty (mtlk_buflist_t *pbuflist)
{
  ASSERT(pbuflist != NULL);
  return skb_queue_empty(pbuflist);
}

static __INLINE uint32
mtlk_buflist_size (mtlk_buflist_t* pbuflist)
{
  ASSERT(pbuflist != NULL);
  return skb_queue_len(pbuflist);
}

static __INLINE mtlk_buflist_entry_t *
mtlk_buflist_next (mtlk_buflist_entry_t *pentry)
{
  ASSERT(pentry != NULL);
  return pentry->next;
}

static __INLINE mtlk_buflist_entry_t *
mtlk_buflist_prev (mtlk_buflist_entry_t *pentry)
{
  ASSERT(pentry != NULL);
  return pentry->prev;
}

static __INLINE mtlk_buflist_entry_t *
mtlk_bufmgr_get_buflist_entry(mtlk_buffer_t *pbuffer)
{
  ASSERT(pbuffer != NULL);
  return pbuffer;
}

static __INLINE mtlk_buffer_t *
mtlk_bufmgr_get_by_buflist_entry (mtlk_buflist_entry_t *pentry)
{
  ASSERT(pentry != NULL);
  return pentry;
}
