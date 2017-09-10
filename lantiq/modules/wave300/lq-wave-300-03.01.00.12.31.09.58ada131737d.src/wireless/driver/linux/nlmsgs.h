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
 * Subsystem providing communication with userspace over
 * NETLINK_USERSOCK netlink protocol.
 *
 */

#ifndef __NLMSGS_H__
#define __NLMSGS_H__

#include "nl.h"

struct nic;

int mtlk_nl_init(void);
void mtlk_nl_cleanup(void);
int mtlk_nl_send_brd_msg(void *data, int length, gfp_t flags, u32 dst_group, u8 cmd); 

#endif /* __NLMSGS_H__ */
