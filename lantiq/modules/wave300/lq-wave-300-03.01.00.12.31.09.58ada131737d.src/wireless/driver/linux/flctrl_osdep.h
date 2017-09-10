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
 * flctrl (Flow Control)  OS-dependent module interface
 *
 */

#ifndef _FLCTRL_OSDEP_H_
#define _FLCTRL_OSDEP_H_

int flctrl_init(struct nic *nic);
void flctrl_cleanup(struct nic *nic);

#endif

