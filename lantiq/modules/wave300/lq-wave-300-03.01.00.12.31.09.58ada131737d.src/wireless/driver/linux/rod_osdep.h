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
* Written by: Dmitry Fleytman 
*
*/
#ifndef __ROD_OSDEP_H__
#define __ROD_OSDEP_H__

typedef struct sk_buff rod_buff;

#define mtlk_rod_drop_packet             mtlk_df_skb_free_sub_frames
#define mtlk_rod_detect_replay_or_sendup mtlk_detect_replay_or_sendup

#endif /* !__ROD_OSDEP_H__ */
