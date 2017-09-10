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
 * Written by: Roman Sikorskyy 
 *
 */
#ifndef __SCAN_OSDEP_H__
#define __SCAN_OSDEP_H__
  
int mtlk_scan_osdep_init(struct mtlk_scan* scan, struct nic* nic);
void _mtlk_scan_osdep_send_completed_event(struct nic *nic);
void _mtlk_scan_osdep_handle_scan_cfm(struct mtlk_scan *scan, UMI_SCAN* payload);
void _mtlk_scan_osdep_handle_pause_elapsed(struct mtlk_scan *scan);

#endif

