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
 * Module responsible for configuration.
 *
 * Authors: originaly written by Joel Isaacson;
 *  further development and support by: Andriy Tkachuk, Artem Migaev,
 *  Oleksandr Andrushchenko.
 *
 */

#include "mtlkinc.h"
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/proc_fs.h>

#include <asm/irq.h>
#include <asm/uaccess.h>

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <linux/ctype.h>
#include <linux/sysctl.h>

#include "compat.h"
#include "utils.h"
#include "mtlkaux.h"
#include "mhi_mac_event.h"
#include "mib_osdep.h"
#include "mtlkparams.h"
#include "debug.h"
#include "scan.h"
#include "eeprom.h"
#include "drvver.h"
#include "aocs.h" /*for dfs.h*/
#include "dfs.h"
#include "dfs_osdep.h"
#include "mtlkmib.h"

/* UseAggregation
   AcceptAggregation
   MaxNumOfPackets
   MaxNumOfBytes
   TimeoutInterval
   MinSizeOfPacketInAggr
   ADDBATimeout
   AggregationWindowSize */
#define ADDBA_BK_DEFAULTS { 0, 1, 7,  12000, 3,  10, 0, MAX_REORD_WINDOW}
#define ADDBA_BE_DEFAULTS { 1, 1, 10, 16000, 3,  10, 0, MAX_REORD_WINDOW}
#define ADDBA_VI_DEFAULTS { 1, 1, 7,  12000, 3,  10, 0, MAX_REORD_WINDOW}
#define ADDBA_VO_DEFAULTS { 1, 1, 2,  10000, 10, 10, 0, MAX_REORD_WINDOW}

#define DEFAULT_TX_POWER        "17"

const mtlk_core_cfg_t def_card_cfg = 
{
  { /* addba */
    { /* per TIDs  */
      ADDBA_BE_DEFAULTS,
      ADDBA_BK_DEFAULTS,
      ADDBA_BK_DEFAULTS,
      ADDBA_BE_DEFAULTS,
      ADDBA_VI_DEFAULTS,
      ADDBA_VI_DEFAULTS,
      ADDBA_VO_DEFAULTS,
      ADDBA_VO_DEFAULTS
    },
    MTLK_ADDBA_DEF_MAX_AGGR_SUPPORTED,
    MTLK_ADDBA_DEF_MAX_REORD_SUPPORTED
  },
  { /* wme_bss */
    { /* class */
      /*   cwmin    cwmax   aifs    txop */
      {     4,      10,     3,      0   }, /* class[0] - BE */
      {     4,      10,     7,      0   }, /* class[1] - BK */
      {     3,       4,     2,   3008   }, /* class[2] - VI */
      {     2,       3,     2,   1504   }  /* class[3] - VO */
    }
  },
  { /* wme_ap */
    { /* class */
      /*   cwmin    cwmax   aifs    txop */
      {     4,       6,     3,      0   }, /* class[0] - BE */
      {     4,      10,     7,      0   }, /* class[1] - BK */
      {     3,       4,     1,   3008   }, /* class[2] - VI */
      {     2,       3,     1,   1504   }  /* class[3] - VO */
    }
  }
};


int b2a(char *cp, UMI_MIB *psSetMib)
{
  sprintf(cp, "%d", psSetMib->uValue.u8Uint8);
  return 0;
}

int a2b(char *cp, UMI_MIB *psSetMib)
{
  int x = mtlk_aux_atol(cp);
  psSetMib->uValue.u8Uint8= x;
  return 0;
}

int x2a(char *cp,UMI_MIB *psSetMib)
{
  sprintf(cp, "0x%x", le32_to_cpu(psSetMib->uValue.u32Uint32));
  return 0;
}

int a2x(char *cp,UMI_MIB *psSetMib)
{
  unsigned int v;
  v = mtlk_aux_atol(cp);
  psSetMib->uValue.u32Uint32=  cpu_to_le32(v);
  return 0;
}

int xb2a(char *cp, UMI_MIB *psSetMib)
{
  sprintf(cp, "0x%02x", psSetMib->uValue.u8Uint8);
  return 0;
}

int a2xb(char *cp, UMI_MIB *psSetMib)
{
  int x = mtlk_aux_atol(cp);
  psSetMib->uValue.u8Uint8= x;
  return 0;
}

int xh2a(char *cp, UMI_MIB *psSetMib)
{
  sprintf(cp, "0x%04x", le16_to_cpu(psSetMib->uValue.u16Uint16));
  return 0;
}

int a2xh(char *cp, UMI_MIB *psSetMib)
{
  int x = mtlk_aux_atol(cp);
  psSetMib->uValue.u16Uint16= cpu_to_le16(x);
  return 0;
}

int a2array(char *cp, UMI_MIB *psSetMib)
{
  int i= 0;
  while(*cp){
    psSetMib->uValue.au8ListOfu8.au8Elements[i++]= *cp++ - '0';
  }
  return 0;
}

int array2a(char *cp, UMI_MIB *psSetMib)
{
  //UMI_MIB *psSetMib = (UMI_MIB *)pmsg->aucPayload;
  // TODO
  return 0;
}

int mcon2a (char *cp, UMI_MIB *psSetMib)
{
  uint8 conn = psSetMib->uValue.sMaxConnections.u8MaxAllowedConnections;
  sprintf(cp, "%d", conn);
  return 0;
}

int a2mcon (char* cp, UMI_MIB *psSetMib)
{
  uint8 conn = mtlk_aux_atol(cp);
  
  if (conn > STA_MAX_STATIONS)
    conn = STA_MAX_STATIONS;

  psSetMib->uValue.sMaxConnections.u8MaxAllowedConnections = conn;
  return 0;
}

int mode2a (char *cp, UMI_MIB *psSetMib)
{
  uint8 mode = psSetMib->uValue.sAclMode.u8ACLMode;
  sprintf(cp, "%d", mode);
  return 0;
}

int a2mode (char *cp, UMI_MIB *psSetMib)
{
  uint8 mode = mtlk_aux_atol(cp);
  
  if (mode > 2)
    mode = 0;
  
  psSetMib->uValue.sAclMode.u8ACLMode = mode;
  return 0;
}

// free all kmalloced mib values
void mtlk_free_mib_values(struct nic *nic)
{
  mib_act *pma;
  for (pma=nic->slow_ctx->mib; *pma->mib_name; pma++) {
    if (nic->slow_ctx->mib_value[pma->index]) {
      kfree_tag(nic->slow_ctx->mib_value[pma->index]);
      nic->slow_ctx->mib_value[pma->index] = NULL;
    }
  }
}

/*****************************************************************************
**
** NAME         mtlk_mib_set_nic_cfg
**
** PARAMETERS   nic            Card context
**
** RETURNS      none
**
** DESCRIPTION  Fills the card configuration structure with user defined
**              values (or default ones)
**
******************************************************************************/
void mtlk_mib_set_nic_cfg (struct nic *nic)
{
  nic->slow_ctx->cfg = def_card_cfg;
}

/*****************************************************************************
**
** NAME         mtlk_create_mib_sysfs
**
** PARAMETERS   nic           Card context
**
** RETURNS      none
**
** DESCRIPTION  Create entries in /proc/sys/ filesystem (sysctl) for card
**              configuration and activation
**
******************************************************************************/
int
mtlk_create_mib_sysfs (struct nic *nic)
{
  mib_act* pma;
  int mib_cnt, sysctl_cnt;
  int sz;
  int cur_sysctl = 0;
  static uint32 board_idx = 0;

  pma = nic->slow_ctx->mib;
  mib_cnt = 0;
  
  while (*pma->mib_name) {
    pma++;
    mib_cnt++;
  }
  
  sz = mib_cnt * (STRING_DEFAULT_SIZE + 1);
  nic->slow_ctx->mib_sysfs_strings = kmalloc_tag(sz, GFP_KERNEL, MTLK_MEM_TAG_MIB);
  ASSERT(nic->slow_ctx->mib_sysfs_strings != NULL);
  memset(nic->slow_ctx->mib_sysfs_strings, 0, sz); 

  sysctl_cnt = mib_cnt;

  sz = sizeof(ctl_table) * sysctl_cnt;
  nic->slow_ctx->nlm_sysctls = kmalloc_tag(sz, GFP_KERNEL, MTLK_MEM_TAG_MIB);
  ASSERT(nic->slow_ctx->nlm_sysctls != NULL);
  memset(nic->slow_ctx->nlm_sysctls, 0, sz); 

  for (pma=nic->slow_ctx->mib; *pma->mib_name; pma++, cur_sysctl++) {
    ASSERT(cur_sysctl < sysctl_cnt);
    nic->slow_ctx->nlm_sysctls[cur_sysctl].procname = pma->mib_name;

    if (pma->maxstring) {
      nic->slow_ctx->nlm_sysctls[cur_sysctl].data = kmalloc_tag(1+pma->maxstring,
        GFP_KERNEL, MTLK_MEM_TAG_MIB);
      ASSERT(nic->slow_ctx->nlm_sysctls[cur_sysctl].data != NULL);
      memset(nic->slow_ctx->nlm_sysctls[cur_sysctl].data, 0, 1+pma->maxstring);
      nic->slow_ctx->nlm_sysctls[cur_sysctl].maxlen = pma->maxstring;
    } else {
      nic->slow_ctx->nlm_sysctls[cur_sysctl].data = nic->slow_ctx->mib_sysfs_strings + cur_sysctl*(STRING_DEFAULT_SIZE+1);
      nic->slow_ctx->nlm_sysctls[cur_sysctl].maxlen = STRING_DEFAULT_SIZE;
    }

    if (nic->slow_ctx->mib_value[pma->index])
      strncpy(nic->slow_ctx->nlm_sysctls[cur_sysctl].data, nic->slow_ctx->mib_value[pma->index], nic->slow_ctx->nlm_sysctls[cur_sysctl].maxlen);
    else
      strncpy(nic->slow_ctx->nlm_sysctls[cur_sysctl].data, pma->mib_default, nic->slow_ctx->nlm_sysctls[cur_sysctl].maxlen);

    ILOG2(GID_MIB, "%s configured with %s", pma->mib_name, (char*)nic->slow_ctx->nlm_sysctls[cur_sysctl].data);
  }

  board_idx++;
  return 0;
}


mib_act *
mtlk_get_mib (char *name, struct nic *nic)
{
  mib_act *pma;

  for (pma=nic->slow_ctx->mib; *pma->mib_name; pma++)
    if (strcmp(pma->mib_name, name) == 0)
      return pma;

  ELOG("MIB %s not found", name);
  return NULL;
}

char *
mtlk_get_mib_value (char *name, struct nic *nic)
{
  mib_act *pma = mtlk_get_mib(name, nic);

  if (!pma)
    return "";

  if (nic->slow_ctx->nlm_sysctls)
    return nic->slow_ctx->nlm_sysctls[pma->index].data;
  else
    return "";
}

int32
mtlk_get_num_mib_value (char *name, struct nic *nic)
{
  return mtlk_aux_atol(mtlk_get_mib_value(name, nic));
}

int
mtlk_set_dec_mib_value (const char *name, int32 value, struct nic *nic)
{
   char buf[20];
   int printed;
   printed = snprintf(buf, SIZEOF(buf), "%d", (int)value);
   if (printed < 0)
     buf[0] = '\0';
   else if (printed >= SIZEOF(buf))
     buf[SIZEOF(buf) - 1] = '\0';

   return mtlk_set_mib_value(name, buf, nic);
}

int
mtlk_set_hex_mib_value (const char *name, uint32 value, struct nic *nic)
{
   char buf[20];
   int printed;
   printed = snprintf(buf, SIZEOF(buf), "0x%08x", (unsigned)value);
   if (printed < 0)
     buf[0] = '\0';
   else if (printed >= SIZEOF(buf))
     buf[SIZEOF(buf) - 1] = '\0';

   return mtlk_set_mib_value(name, buf, nic);
}

int
mtlk_set_mib_value (const char *name, char *value, struct nic *nic)
{
  mib_act *pma;
  int i;

  ILOG2(GID_MIB, "setting %s to %s", name, value);

  for (i=0, pma=nic->slow_ctx->mib; *pma->mib_name; i++, pma++) {
    if (strcmp(pma->mib_name, name) == 0) {
      int size= STRING_DEFAULT_SIZE;
      if(pma->maxstring != 0)
        size= pma->maxstring;
      if (nic->slow_ctx->nlm_sysctls == NULL) return -1;
      strncpy(nic->slow_ctx->nlm_sysctls[i].data, value, size);
      ILOG3(GID_MIB, "done");
      return 0;
    }
  }

  ELOG("param %s was not found", name);
  return -1;
}

void
mtlk_update_mib_sysfs (struct nic *nic)
{
  mib_act *pma;
  int i;

  for (i=0, pma=nic->slow_ctx->mib; *pma->mib_name; i++, pma++) {
    char *old_conf, *new_conf;

    if(nic->slow_ctx->mib_value[pma->index])
      old_conf= nic->slow_ctx->mib_value[pma->index];
    else
      old_conf= pma->mib_default;

    new_conf = nic->slow_ctx->nlm_sysctls[i].data;

    if (strcmp(old_conf, new_conf) != 0) {
      ILOG2(GID_MIB, "%s '%s' -> '%s'", pma->mib_name, old_conf, new_conf);
      if (nic->slow_ctx->mib_value[pma->index])
        kfree_tag(nic->slow_ctx->mib_value[pma->index]);
      nic->slow_ctx->mib_value[pma->index] = kmalloc_tag(strlen(new_conf) +1,
        GFP_KERNEL, MTLK_MEM_TAG_MIB_VALUES);
      ASSERT(nic->slow_ctx->mib_value[pma->index] != NULL);
      strcpy(nic->slow_ctx->mib_value[pma->index], new_conf);
    }
  }
}

void
mtlk_unregister_mib_sysfs (struct nic *nic)
{
  mib_act *pma=nic->slow_ctx->mib;
  int i=0;

  if (pma != NULL) {
    while(*pma->mib_name) { // We have to deallocate any entry that has a maxstring element
      if(pma->maxstring) {
        if (nic->slow_ctx->nlm_sysctls)
          kfree_tag(nic->slow_ctx->nlm_sysctls[i].data);
        pma->maxstring = 0;
      }
      pma++;
      i++;
    }
  }
  if (nic->slow_ctx->nlm_sysctls) {
    kfree_tag(nic->slow_ctx->nlm_sysctls);
    nic->slow_ctx->nlm_sysctls = NULL;
  }
  if (nic->slow_ctx->mib_sysfs_strings) {
    kfree_tag(nic->slow_ctx->mib_sysfs_strings);
    nic->slow_ctx->mib_sysfs_strings = NULL;
  }
}

mib_act g2_mib_action[]=    // Gen2
{
  {PRM_SHORT_PREAMBLE, "1", MIB_SHORT_PREAMBLE_OPTION_IMPLEMENTED, xb2a, a2xb, 0},
  {PRM_TX_ANTENNAS, "120", MIB_SUPPORTED_TX_ANTENNAS, array2a, a2array, 0},
  {PRM_RX_ANTENNAS, "123", MIB_SUPPORTED_RX_ANTENNAS, array2a, a2array, 0},
  {PRM_SPECTRUM_MODE, "1", MIB_NO_OID, xb2a, a2xb, 0},
  //{"Service", "1", MIB_SERVICE_REQUIRED, xb2a, a2xb, 0},
  {PRM_TX_POWER, "0", MIB_TX_POWER, xb2a, a2xb, 0},
  {PRM_SHORT_CYCLIC_PREFIX, "0", MIB_USE_SHORT_CYCLIC_PREFIX, xb2a, a2xb, 0},
  {PRM_ALGO_CALIBR_MASK, "255", MIB_CALIBRATION_ALGO_MASK, xb2a, a2xb, 0},
  {PRM_AP_FORWARDING, "1", MIB_NO_OID, xb2a, a2xb, 0},
  {PRM_FORCE_TPC0, "-1", MIB_FORCE_TPC_0, b2a, a2b, 0},
  {PRM_FORCE_TPC1, "-1", MIB_FORCE_TPC_1, b2a, a2b, 0},
  {PRM_FORCE_TPC2, "-1", MIB_FORCE_TPC_2, b2a, a2b, 0},
//  {PRM_TPC_ANT0_PER_FREQ_A_B_2_4GHZ, "10/0/0/15/0", MIB_TPC_ANT_0_FREQ_A_B_2_4_GHz, tpc2ghz2a, a2tpc2ghz, 0},
//  {PRM_TPC_ANT0_PER_FREQ_A_B_5_2GHZ, "181/0/0/15/0;200/0/0/15/0;40/0/0/15/0;80/0/0/15/0;120/0/0/15/0", MIB_TPC_ANT_0_FREQ_A_B_5_GHz, tpc5ghz2a, a2tpc5ghz, 64},
//  {PRM_TPC_ANT1_PER_FREQ_A_B_2_4GHZ, "10/0/0/15/0", MIB_TPC_ANT_1_FREQ_A_B_2_4_GHz, tpc2ghz2a, a2tpc2ghz, 0},
//  {PRM_TPC_ANT1_PER_FREQ_A_B_5_2GHZ, "181/0/0/15/0;200/0/0/15/0;40/0/0/15/0;80/0/0/15/0;120/0/0/15/0", MIB_TPC_ANT_1_FREQ_A_B_5_GHz, tpc5ghz2a, a2tpc5ghz, 64},
  {PRM_SHORT_SLOT_TIME_OPTION_ENABLED, "1", MIB_SHORT_SLOT_TIME_OPTION_ENABLED_11G, b2a, a2b, 0},
  //{"NetworkType", "0", MIB_NO_OID, xb2a, a2xb, 0},      // 0 - STA, 1 - AP
  {PRM_ACL_MAX_CONNECTIONS, "16", MIB_ACL_MAX_CONNECTIONS, mcon2a, a2mcon, 0},
  {PRM_ACL_MODE, "0", MIB_ACL_MODE, mode2a, a2mode, 0},
  {PRM_POWER_INCREASE_VS_DUTY_CYCLE, "0", MIB_POWER_INCREASE_VS_DUTY_CYCLE, b2a, a2b, 0},
  {PRM_BRIDGE_MODE, "1", MIB_NO_OID, xb2a, a2xb, 0},
  {PRM_L2NAT_AGING_TIMEOUT, "600", MIB_NO_OID, xb2a, a2xb, 0},
  {PRM_CHANNEL_ANNOUNCEMENT_ENABLED, "1", MIB_SM_ENABLE, xb2a, a2xb, 0},
  {PRM_ENABLE_PACK_SCHED, "1", MIB_NO_OID, xb2a, a2xb, 0},
  {"","",0,NULL,NULL,0},
};

int
mtlk_mib_set_forced_rates (struct nic *nic)
{
  uint16 is_force_rate; /* FORCED_RATE_LEGACY_MASK & FORCED_RATE_HT_MASK */

  /*
   * Driver should first disable adaptive rate,
   * in order to avoid condition
   * where MIB_IS_FORCE_RATE configured to use forced rate
   * and MIB_{LEGACY,HT}_FORCE_RATE configured to use NO_RATE.
   */
  mtlk_set_mib_value_uint16(nic->slow_ctx->hw_cfg.txmm, MIB_IS_FORCE_RATE, 0);

  mtlk_set_mib_value_uint16(nic->slow_ctx->hw_cfg.txmm,
    MIB_LEGACY_FORCE_RATE, nic->slow_ctx->cfg.legacy_forced_rate);
  mtlk_set_mib_value_uint16(nic->slow_ctx->hw_cfg.txmm,
    MIB_HT_FORCE_RATE, nic->slow_ctx->cfg.ht_forced_rate);

  is_force_rate = 0;
  if (nic->slow_ctx->cfg.legacy_forced_rate != NO_RATE)
    is_force_rate |= FORCED_RATE_LEGACY_MASK;
  if (nic->slow_ctx->cfg.ht_forced_rate != NO_RATE)
    is_force_rate |= FORCED_RATE_HT_MASK;

  mtlk_set_mib_value_uint16(nic->slow_ctx->hw_cfg.txmm, MIB_IS_FORCE_RATE, is_force_rate);

  return MTLK_ERR_OK;
}

static int
mtlk_set_mib_values_ex (struct nic *nic, mtlk_txmm_msg_t* man_msg)
{
  mtlk_txmm_data_t* man_entry;
  UMI_MIB *psSetMib;
  mib_act *pmib;

  // Loop through the mib_action array and set the MIB values
  int r, i;
  char *mib_val;
  uint8 calibration;

  man_entry = mtlk_txmm_msg_get_empty_data(man_msg, nic->slow_ctx->hw_cfg.txmm);
  if (!man_entry) {
    ELOG("Can't get MM data");
    return -ENOMEM;
  }

  ILOG2(GID_MIB, "Must do MIB's");
  // Check if TPC close loop is ON and we have calibrations in EEPROM for
  // selected frequency band.
  mib_val = mtlk_get_mib_value(PRM_ALGO_CALIBR_MASK, nic);
  if (mib_val) {
    calibration = (uint8)mtlk_aux_atol(mib_val);

    if (calibration & 0x80) {
      if ((nic->slow_ctx->frequency_band_cur == MTLK_HW_BAND_5_2_GHZ &&
            nic->slow_ctx->ee_data.tpc_52 == NULL) ||
          (nic->slow_ctx->frequency_band_cur == MTLK_HW_BAND_2_4_GHZ &&
           nic->slow_ctx->ee_data.tpc_24 == NULL)) {
        mtlk_set_hw_state(nic, MTLK_HW_STATE_ERROR);
        ELOG("TPC close loop is ON and no calibrations for current frequency (%s GHz) in EEPROM",
          nic->slow_ctx->frequency_band_cur ? "2.4" : "5");
      }
    } else { // TPC close loop is OFF. Check if TxPower is not zero.
      mib_val = mtlk_get_mib_value(PRM_TX_POWER, nic);
      ILOG1(GID_MIB, "TxPower = %s", mib_val);
      if (mib_val) {
        long val = mtlk_aux_atol(mib_val);
        if (!val) {
          mtlk_set_mib_value(PRM_TX_POWER, DEFAULT_TX_POWER, nic);
        }
      }
    }
  }

  mtlk_update_mib_sysfs(nic);

  mtlk_mib_set_forced_rates(nic);

  for (pmib=nic->slow_ctx->mib; *pmib->mib_name; pmib++) {
    if (pmib->obj_id == MIB_NO_OID)
      continue;

    ILOG2(GID_MIB, "Mib name %s, default %s, value %s, mib_id 0x%04x",
          pmib->mib_name, pmib->mib_default,
          nic->slow_ctx->mib_value[pmib->index] ? nic->slow_ctx->mib_value[pmib->index] : "NULL" , 
          pmib->obj_id);

    if (nic->slow_ctx->mib_value[pmib->index])
      mib_val = nic->slow_ctx->mib_value[pmib->index];
    else
      mib_val = pmib->mib_default;

    man_entry->id           = UM_MAN_SET_MIB_REQ;
    man_entry->payload_size = sizeof(*psSetMib);

    psSetMib = (UMI_MIB*)man_entry->payload;

    // Convert from ascii to message format
    memset(psSetMib, 0, sizeof(*psSetMib));
    r = (*pmib->from_ascii)(mib_val, psSetMib);

    if (r != 0) {
      ELOG("Mib %s - conversion of value %s failed", pmib->mib_name, mib_val);
      continue;
    }

    psSetMib->u16ObjectID = cpu_to_le16(pmib->obj_id);

    if (nic->slow_ctx->hw_cfg.ap) {
      // MAC filtering MIBs requires Network Index (for M-BSSID)
      if (pmib->obj_id == MIB_ACL_MAX_CONNECTIONS)
        psSetMib->uValue.sMaxConnections.Network_Index = 0;
      if (pmib->obj_id == MIB_ACL_MODE)
        psSetMib->uValue.sAclMode.Network_Index = 0;
    } else {
      if (pmib->obj_id == MIB_ACL_MAX_CONNECTIONS ||
          pmib->obj_id == MIB_ACL_MODE) {
        continue;
      }
    }


    //mtlk_aux_print_hex(pmsg->aucPayload, sizeof(UMI_MIB));

    if (mtlk_txmm_msg_send_blocked(man_msg,
                                   MTLK_MM_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
      ELOG("Failed to set MIB item 0x%x, timed-out",
          le16_to_cpu(psSetMib->u16ObjectID));
      return -ENODEV;
    }

    if (psSetMib->u16Status == cpu_to_le16(UMI_OK)) {
      ILOG2(GID_MIB, "Successfully set MIB item 0x%04x.",
        le16_to_cpu(psSetMib->u16ObjectID));
    } else {
      ELOG("Failed to set MIB item 0x%x, error %d",
        le16_to_cpu(psSetMib->u16ObjectID), le16_to_cpu(psSetMib->u16Status));
    }
  }

  if (MTLK_ERR_OK != mtlk_set_mib_acl(nic->slow_ctx->hw_cfg.txmm, nic->slow_ctx->acl, nic->slow_ctx->acl_mask))
    return -ENODEV;

  if (nic->slow_ctx->hw_cfg.ap)
  {
    // set WME BSS parameters (relevant only on AP)
    man_entry->id           = UM_MAN_SET_MIB_REQ;
    man_entry->payload_size = sizeof(*psSetMib);

    psSetMib = (UMI_MIB*)man_entry->payload;

    memset(psSetMib, 0, sizeof(*psSetMib));

    psSetMib->u16ObjectID = cpu_to_le16(MIB_WME_PARAMETERS);

    for (i = 0; i < NTS_PRIORITIES; i++) {
      psSetMib->uValue.sWMEParameters.au8CWmin[i] = nic->slow_ctx->cfg.wme_bss.wme_class[i].cwmin;
      psSetMib->uValue.sWMEParameters.au16Cwmax[i] = cpu_to_le16(nic->slow_ctx->cfg.wme_bss.wme_class[i].cwmax);
      psSetMib->uValue.sWMEParameters.au8AIFS[i] = nic->slow_ctx->cfg.wme_bss.wme_class[i].aifsn;
      psSetMib->uValue.sWMEParameters.au16TXOPlimit[i] = cpu_to_le16(nic->slow_ctx->cfg.wme_bss.wme_class[i].txop / 32);
    }

    if (mtlk_txmm_msg_send_blocked(man_msg,
                                   MTLK_MM_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
      ELOG("Failed to set MIB item 0x%x, timed-out",
          le16_to_cpu(psSetMib->u16ObjectID));
      return -ENODEV;
    }

    if (psSetMib->u16Status == cpu_to_le16(UMI_OK)) {
      ILOG2(GID_MIB, "Successfully set MIB item 0x%04x.",
        le16_to_cpu(psSetMib->u16ObjectID));
    } else {
      ELOG("Failed to set MIB item 0x%x, error %d.",
        le16_to_cpu(psSetMib->u16ObjectID), le16_to_cpu(psSetMib->u16Status));
    }

    // set WME AP parameters (relevant only on AP)
    man_entry->id           = UM_MAN_SET_MIB_REQ;
    man_entry->payload_size = sizeof(*psSetMib);

    psSetMib = (UMI_MIB*)man_entry->payload;

    memset(psSetMib, 0, sizeof(*psSetMib));

    psSetMib->u16ObjectID = cpu_to_le16(MIB_WME_QAP_PARAMETERS);

    for (i = 0; i < NTS_PRIORITIES; i++) {
      psSetMib->uValue.sWMEParameters.au8CWmin[i] = nic->slow_ctx->cfg.wme_ap.wme_class[i].cwmin;
      psSetMib->uValue.sWMEParameters.au16Cwmax[i] = cpu_to_le16(nic->slow_ctx->cfg.wme_ap.wme_class[i].cwmax);
      psSetMib->uValue.sWMEParameters.au8AIFS[i] = nic->slow_ctx->cfg.wme_ap.wme_class[i].aifsn;
      psSetMib->uValue.sWMEParameters.au16TXOPlimit[i] = cpu_to_le16(nic->slow_ctx->cfg.wme_ap.wme_class[i].txop / 32);
    }

    if (mtlk_txmm_msg_send_blocked(man_msg,
                                   MTLK_MM_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
      ELOG("Failed to set MIB item 0x%x, timed-out",
          le16_to_cpu(psSetMib->u16ObjectID));
      return -ENODEV;
    }

    if (psSetMib->u16Status == cpu_to_le16(UMI_OK)) {
      ILOG2(GID_MIB, "Successfully set MIB item 0x%04x.",
        le16_to_cpu(psSetMib->u16ObjectID));
    } else {
      ELOG("Failed to set MIB item 0x%x, error %d",
        le16_to_cpu(psSetMib->u16ObjectID), le16_to_cpu(psSetMib->u16Status));
    }
  }

  // set driver features
  mib_val = mtlk_get_mib_value(PRM_SPECTRUM_MODE, nic);
  nic->slow_ctx->spectrum_mode = mtlk_aux_atol(mib_val);

  if (nic->slow_ctx->hw_cfg.ap) {
    mtlk_set_mib_value_uint8(nic->slow_ctx->hw_cfg.txmm,
      MIB_SPECTRUM_MODE, nic->slow_ctx->spectrum_mode);
  }
  mib_val = mtlk_get_mib_value(PRM_AP_FORWARDING, nic);
  nic->ap_forwarding = mtlk_aux_atol(mib_val);

  mib_val = mtlk_get_mib_value(PRM_L2NAT_AGING_TIMEOUT, nic);
  nic->l2nat_aging_timeout = HZ * mtlk_aux_atol(mib_val);
  ILOG1(GID_MIB, "l2nat_aging_timeout set to %u", nic->l2nat_aging_timeout);

  mib_val = mtlk_get_mib_value(PRM_ENABLE_PACK_SCHED, nic);
  nic->pack_sched_enabled = mtlk_aux_atol(mib_val);
  ILOG1(GID_MIB, "pack_sched_enabled set to %u", nic->pack_sched_enabled);

  mib_val = mtlk_get_mib_value(PRM_BRIDGE_MODE, nic);
  nic->bridge_mode = mtlk_aux_atol(mib_val);
  ILOG1(GID_MIB, "bridge_mode set to %u", nic->bridge_mode);

  switch(nic->bridge_mode) {
  case BR_MODE_NONE:
  case BR_MODE_WDS:
  case BR_MODE_L2NAT:
  case BR_MODE_MAC_CLONING:
    break;
  default:
    ILOG1(GID_MIB, "Unknown value of bridge_mode: %d", nic->bridge_mode);
    nic->bridge_mode = BR_MODE_NONE;
    break;
  }

  if (nic->slow_ctx->hw_cfg.ap) {
    nic->slow_ctx->pm_params.u8NetworkMode = nic->slow_ctx->net_mode_cur;
    nic->slow_ctx->pm_params.u8SpectrumMode = nic->slow_ctx->spectrum_mode;
    nic->slow_ctx->pm_params.u32BSSbasicRateSet = get_basic_rate_set(nic->slow_ctx->net_mode_cur, nic->slow_ctx->cfg.basic_rate_set);
    nic->slow_ctx->pm_params.u32OperationalRateSet = get_operate_rate_set(nic->slow_ctx->net_mode_cfg);
    nic->slow_ctx->pm_params.u8ShortSlotTimeOptionEnabled11g = mtlk_aux_atol(mtlk_get_mib_value(PRM_SHORT_SLOT_TIME_OPTION_ENABLED, nic));
    nic->slow_ctx->pm_params.u8ShortPreambleOptionImplemented = mtlk_aux_atol(mtlk_get_mib_value(PRM_SHORT_PREAMBLE, nic));
    nic->slow_ctx->pm_params.u8UpperLowerChannel = nic->slow_ctx->bonding;
  }

  /* this is requirement from MAC */
  MTLK_ASSERT(ASSOCIATE_FAILURE_TIMEOUT < CONNECT_TIMEOUT);
  mtlk_set_mib_value_uint32(nic->slow_ctx->hw_cfg.txmm, MIB_ASSOCIATE_FAILURE_TIMEOUT, ASSOCIATE_FAILURE_TIMEOUT);

  ILOG2(GID_MIB, "End Mibs");
  return 0;
}

int
mtlk_set_mib_values(struct nic *nic)
{
  int             res       = -ENOMEM;
  mtlk_txmm_msg_t man_msg;

  if (mtlk_txmm_msg_init(&man_msg) == MTLK_ERR_OK) {
    res = mtlk_set_mib_values_ex(nic, &man_msg);
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  else {
    ELOG("Can't init TXMM msg");
  }

  return res;
}

void
mtlk_mib_update_pm_related_mibs (struct nic *nic,
                        mtlk_aux_pm_related_params_t *data)
{
  /* Update MIB DB */
  mtlk_set_dec_mib_value(PRM_SPECTRUM_MODE,
                         (int)data->u8SpectrumMode,
                         nic);
  mtlk_set_dec_mib_value(PRM_SHORT_SLOT_TIME_OPTION_ENABLED,
                         (int)data->u8ShortSlotTimeOptionEnabled11g,
                         nic);
  mtlk_set_dec_mib_value(PRM_SHORT_PREAMBLE,
                         (int)data->u8ShortPreambleOptionImplemented,
                         nic);
  nic->slow_ctx->bonding = (int)data->u8UpperLowerChannel;
  mtlk_update_mib_sysfs(nic);
}

