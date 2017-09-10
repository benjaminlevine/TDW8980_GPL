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
 * Authors: originaly written by Joel Isaacson;
 *  further development and support by: Andriy Tkachuk, Artem Migaev,
 *  Oleksandr Andrushchenko.
 *
 */

#ifndef _MIB_OSDEP_H_
#define _MIB_OSDEP_H_

#include <linux/ctype.h>

#include "mhi_umi.h"

#define STRING_DEFAULT_SIZE 16

#define MIB_NO_OID 0xffffff    // skip over in setting via set-mibs

typedef struct {
  char *mib_name;
  char *mib_default;
  unsigned int obj_id;
  int (*to_ascii)(char *cp, UMI_MIB *psSetMib);
  int (*from_ascii)(char *cp, UMI_MIB *psSetMib);
  int maxstring;
  int index;
} mib_act;

#include "core.h"

extern mib_act g2_mib_action[];
extern const mtlk_core_cfg_t def_card_cfg;

void  mtlk_free_mib_values(struct nic*);
void  mtlk_mib_set_nic_cfg (struct nic *nic);
int   mtlk_create_mib_sysfs(struct nic *);
void  mtlk_unregister_mib_sysfs(struct nic*);
void  mtlk_update_mib_sysfs(struct nic*);
char *mtlk_get_mib_value(char *name, struct nic*);
int32 mtlk_get_num_mib_value(char *name, struct nic*);
int   mtlk_set_mib_value(const char *name, char *value, struct nic*);
int   mtlk_set_dec_mib_value(const char *name, int32 value, struct nic *nic);
int   mtlk_set_hex_mib_value(const char *name, uint32 value, struct nic *nic);
mib_act *mtlk_get_mib (char *name, struct nic *nic);
int mtlk_a2acl (char *cp, UMI_MIB *psSetMib, char** last);

int mtlk_set_mib_values(struct nic *nic);
int mtlk_mib_set_forced_rates (struct nic *nic);

void mtlk_mib_update_pm_related_mibs (struct nic *nic, mtlk_aux_pm_related_params_t *data);

#endif // _MIB_OSDEP_H_
