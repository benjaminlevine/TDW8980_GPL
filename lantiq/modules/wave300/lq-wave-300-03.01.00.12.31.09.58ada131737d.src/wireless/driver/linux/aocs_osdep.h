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
 * AOCS OS-dependent module interface
 *
 */

#ifndef _AOCS_OSDEP_H_
#define _AOCS_OSDEP_H_

int aocs_init(struct nic *nic);
void aocs_cleanup(struct nic *nic);
#ifdef AOCS_DEBUG
int aocs_proc_cl(struct file *file, const char *buf, unsigned long count, void *data);
#endif

#endif

