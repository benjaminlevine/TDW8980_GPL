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
 * ioctl support
 *
 */

#ifndef __IOCTL_H__
#define __IOCTL_H__

#include "compat.h"

int mtlk_ioctl_do_ioctl (struct net_device *dev, struct ifreq *ifr, int cmd);
struct iw_statistics *mtlk_linux_get_iw_stats (struct net_device *dev);

#endif

