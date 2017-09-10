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
 * ADDBA OS-dependent module interface
 *
 */

#ifndef _ADDBA_OSDEP_H_
#define _ADDBA_OSDEP_H_

#include "addba.h"

#define addba_reconfigure(p) mtlk_addba_reconfigure(&(p)->slow_ctx->addba, &(p)->slow_ctx->cfg.addba)

int addba_init(struct nic *nic);
void addba_cleanup(struct nic *nic);
mtlk_addba_t *mtlk_get_addba_related_info (mtlk_handle_t context, uint16 *rate);

#endif

