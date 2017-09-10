/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id:$
 *
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *  
 * Linux dependant send queue parts.
 */

#ifndef __MTLK_SQ_OSDEP_H__
#define __MTLK_SQ_OSDEP_H__

/* not used */
typedef int mtlk_os_status_t;
/* not used */
typedef struct {} mtlk_completion_ctx_t;

/* functions called from shared SendQueue code */
int  _mtlk_sq_send_to_hw(void *pq, struct sk_buff *skb, uint16 prio, int *status);

void _mtlk_sq_cancel_packet(struct sk_buff *skb)
{
  dev_kfree_skb(skb);
};

/* these are not used currently */
void _mtlk_sq_create_completion_ctx(mtlk_completion_ctx_t *cmpl){};
void _mtlk_sq_release_completion_ctx(void *pq, mtlk_completion_ctx_t * cmpl){};
void _mtlk_sq_complete_packet(mtlk_completion_ctx_t *cmpl, struct sk_buff *skb, int status){};

#endif /* __MTLK_SQ_OSDEP_H__ */
