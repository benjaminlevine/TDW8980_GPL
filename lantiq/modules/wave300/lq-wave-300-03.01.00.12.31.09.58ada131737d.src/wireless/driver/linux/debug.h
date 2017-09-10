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
 * Debug API.
 *
 * Originally written by Andrey Fidrya
 *
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <linux/spinlock.h>

#include "mhi_umi.h"

struct nic;

// Driver message categories for DRV_* commands
// Do not change these values (synchronized with BBStudio)
#define DRVCAT_DBG_API_RESET         1
#define DRVCAT_DBG_API_GENERAL_PKT   2
#define DRVCAT_DBG_API_GENERAL       3
#define DRVCAT_DBG_API_MAC_STATS     4
#define DRVCAT_DBG_API_RR_STATS      5

// Subcommand indices for DRVCAT_DBG_API_RESET:
// Do not change these values (synchronized with BBStudio)
#define IDX_DBG_API_RESET_ALL  1

void mtlk_debug_reset_counters (struct nic *nic);

int mtlk_debug_category_init(struct nic *nic, uint32 category,
    uint32 *pcnt);
int mtlk_debug_category_free(struct nic *nic, uint32 category);
int mtlk_debug_name_get(struct nic *nic, uint32 category,
    uint32 index, char *pdata, uint32 datalen);
int mtlk_debug_val_get(struct nic *nic, uint32 category,
    uint32 index, uint32 *pval);
int mtlk_debug_val_put(struct nic *nic, uint32 category,
    uint32 index, uint32 val);


int mtlk_debug_register_proc_entries(struct nic *nic);

int mtlk_debug_igmp_read (char *page, char **start, off_t off,
                          int count, int *eof, void *data );

int mtlk_debug_addr_read (char *page, char **start, off_t off,
                          int count, int *eof, void *data );

int mtlk_debug_addr_write (struct file *file, const char *buf, 
		           unsigned long count, void *data );

int mtlk_debug_size_read (char *page, char **start, off_t off,
                          int count, int *eof, void *data );

int mtlk_debug_size_write (struct file *file, const char *buf, 
		           unsigned long count, void *data );

int mtlk_debug_dump_read (char *page, char **start, off_t off,
                          int count, int *eof, void *data );

// BCL (Metalink Broadband Studio application for MAC debug) support
void mtlk_debug_bswap_bcl_request(UMI_BCL_REQUEST *req, BOOL hdr_only);
int mtlk_debug_bcl_category_init(struct nic *nic, uint32 category, uint32 *cnt);
int mtlk_debug_bcl_category_free(struct nic *nic, uint32 category);
int mtlk_debug_bcl_name_get(struct nic *nic, uint32 category, uint32 index, char *pdata, uint32 datalen);
int mtlk_debug_bcl_val_get(struct nic *nic, uint32 category, uint32 index, uint32 *pval);
int mtlk_debug_bcl_val_put(struct nic *nic, uint32 category, uint32 index, uint32 val);

void mtlk_debug_procfs_cleanup(struct nic *nic, unsigned num_boards_alive);

#define MAX_PROC_STR_LEN 32

#endif /* __DEBUG_H__ */
