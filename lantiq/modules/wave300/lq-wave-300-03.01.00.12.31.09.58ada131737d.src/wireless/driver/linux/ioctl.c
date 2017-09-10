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

#include "mtlkinc.h"

#include <linux/netdevice.h>
#include <net/iw_handler.h>
#include <asm/unaligned.h>

#include "ioctl.h"
#include "core.h"
#include "mtlkparams.h"
#include "mtlkmib.h"
#include "dfs_osdep.h"
#include "sq.h"
#include "l2nat.h"
#include "mtlkirbm_k.h"
#include "wpa.h"
#include "bitrate.h"

#define TEXT_SIZE IW_PRIV_SIZE_MASK /* 2047 */
#define INTVEC_SIZE 32
#define ADDRVEC_SIZE 32

#define TYPE_INT     (IW_PRIV_TYPE_INT|IW_PRIV_SIZE_FIXED|1)
#define TYPE_INTVEC  (IW_PRIV_TYPE_INT|INTVEC_SIZE)
#define TYPE_ADDR    (IW_PRIV_TYPE_ADDR|IW_PRIV_SIZE_FIXED|1)
#define TYPE_ADDRVEC (IW_PRIV_TYPE_ADDR|ADDRVEC_SIZE)
#define TYPE_TEXT    (IW_PRIV_TYPE_CHAR|TEXT_SIZE) 

#define GET_PARAMETER( cmd , type , name ) { cmd , 0 , type , name },
#define SET_PARAMETER( cmd , type , name ) { cmd , type , 0 , name },

#define SET_INT( cmd , name )     SET_PARAMETER( cmd , TYPE_INT     , name )
#define GET_INT( cmd , name )     GET_PARAMETER( cmd , TYPE_INT     , name )
#define SET_INTVEC( cmd , name )  SET_PARAMETER( cmd , TYPE_INTVEC  , name )
#define GET_INTVEC( cmd , name )  GET_PARAMETER( cmd , TYPE_INTVEC  , name )
#define SET_ADDR( cmd , name )    SET_PARAMETER( cmd , TYPE_ADDR    , name )
#define GET_ADDR( cmd , name )    GET_PARAMETER( cmd , TYPE_ADDR    , name )
#define SET_ADDRVEC( cmd , name ) SET_PARAMETER( cmd , TYPE_ADDRVEC , name )
#define GET_ADDRVEC( cmd , name ) GET_PARAMETER( cmd , TYPE_ADDRVEC , name )
#define SET_TEXT( cmd , name )    SET_PARAMETER( cmd , TYPE_TEXT    , name )
#define GET_TEXT( cmd , name )    GET_PARAMETER( cmd , TYPE_TEXT    , name )

#define WPA_IE_SIZE(bss) (((bss)->wpa_offs == 0xffff)? 0:(bss)->info_elements[(bss)->wpa_offs+1])
#define WPA_IE(bss) (&(bss)->info_elements[(bss)->wpa_offs])
#define RSN_IE_SIZE(bss) (((bss)->rsn_offs == 0xffff)? 0:(bss)->info_elements[(bss)->rsn_offs+1])
#define RSN_IE(bss) (&(bss)->info_elements[(bss)->rsn_offs])
#define WPS_IE_SIZE(bss) (((bss)->wps_offs == 0xffff)? 0:(bss)->info_elements[(bss)->wps_offs+1])
#define WPS_IE(bss) (&(bss)->info_elements[(bss)->wps_offs])
#define WPS_IE_FOUND(bss) ((bss)->wps_offs != 0xffff)

static int
set_force_rate (struct nic *nic, uint16 u16ObjectID, char *rate)
{
  uint16 forced_rate;
  int res = mtlk_bitrate_str_to_idx(rate,
                                    mtlk_get_num_mib_value(PRM_SPECTRUM_MODE, nic),
                                    mtlk_get_num_mib_value(PRM_SHORT_CYCLIC_PREFIX, nic),
                                    &forced_rate);

  if (res != MTLK_ERR_OK)
    return res;

  if (forced_rate == NO_RATE)
    goto apply;

  if (!((1 << forced_rate) & mtlk_core_get_available_bitrates(nic))) {
    ILOG0(GID_IOCTL, "Rate doesn't fall into list of supported rates for current network mode");
    return MTLK_ERR_PARAMS;
  }

apply:

  switch (u16ObjectID) {
  case MIB_LEGACY_FORCE_RATE:
    nic->slow_ctx->cfg.legacy_forced_rate = forced_rate;
    break;
  case MIB_HT_FORCE_RATE:
    nic->slow_ctx->cfg.ht_forced_rate = forced_rate;
    break;
  default:
    break;
  }

  mtlk_mib_set_forced_rates(nic);

  return MTLK_ERR_OK;
}

static int
get_force_rate (char *rate, 
                unsigned *length,
                struct nic *nic, 
                uint16 u16ObjectID)
{
  uint16 forced_rate = 0; /* prevent compiler from complaining */

  switch (u16ObjectID) {
  case MIB_HT_FORCE_RATE:
    forced_rate = nic->slow_ctx->cfg.ht_forced_rate;
    break;
  case MIB_LEGACY_FORCE_RATE:
    forced_rate = nic->slow_ctx->cfg.legacy_forced_rate;
    break;
  default:
    break;
  }

  return mtlk_bitrate_idx_to_str(forced_rate,
                                 mtlk_get_num_mib_value(PRM_SPECTRUM_MODE, nic),
                                 mtlk_get_num_mib_value(PRM_SHORT_CYCLIC_PREFIX, nic),
                                 rate,
                                 length);
}

static int
get_supported_countries(char *buf,
        				unsigned *length,
        				struct nic *nic)
{
  uint32 i;
  int32 printed_len = 0;
  mtlk_country_name_t countries[MAX_COUNTRIES];

  memset(countries, 0, sizeof(countries));
  get_all_countries_for_domain(country_code_to_domain(nic->slow_ctx->cfg.country_code),
		  	  	  	  	  	   countries,
		  	  	  	  	  	   MAX_COUNTRIES);

  for(i=0; i<MAX_COUNTRIES; i++) {
    /* stop if no more countries left */
    /* we got a zero-filled memory here */
    if(0 == *countries[i].name) {
      break;
    }

    /* no more iwpriv buffer left */
    if((length-printed_len) < 0) {
      break;
    }

    /* newline each 8 elements */
    if(0 == i%8) {
      printed_len += snprintf(buf+printed_len, *length-printed_len, "\n");
    }

    printed_len += snprintf(buf+printed_len, *length-printed_len, "%s  ", countries[i].name);
  }

  *length = printed_len;
  return MTLK_ERR_OK;
}

static int 
mtlk_linux_ioctl_setap (struct net_device *dev,
          struct iw_request_info *info, struct sockaddr *sa, char *extra)
{ 
  struct nic *nic = netdev_priv(dev);
  bss_data_t bss_found;
  int result, freq;
  mtlk_aux_pm_related_params_t pm_params;
    
  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (nic->slow_ctx->hw_cfg.ap) {
    return -EOPNOTSUPP;
  }

  if (!netif_running(dev)) {
    ILOG0(GID_IOCTL, "%s: You should bring interface up first", dev->name);
    return -EOPNOTSUPP;
  }
 
  if (is_broadcast_ether_addr(sa->sa_data)) {
    return -EOPNOTSUPP;
  }

  if (is_zero_ether_addr(sa->sa_data)) {
    return 0;
  }
  
  ILOG1(GID_IOCTL, "%s: Handle request: connect to %Y", dev->name, sa->sa_data);

  if ((NET_STATE_READY != mtlk_core_get_net_state(nic))
      || mtlk_scan_is_running(&nic->slow_ctx->scan)
      || mtlk_core_is_stopping(nic)) {
    ILOG1(GID_IOCTL, "%s: Can't connect to AP - inappropriate state", dev->name);
    return -EAGAIN;
  }

  if (0 == mtlk_cache_find_bss_by_bssid(
              &nic->slow_ctx->cache, (u8*)&sa->sa_data, &bss_found, NULL)) {
    ILOG1(GID_IOCTL, "%s: Can't connect to AP - unknown BSS", dev->name);
    return -EINVAL;
  } 

  /* update regulation limits for the BSS */
  if (mtlk_core_get_dot11d(nic)) {
    if (!bss_found.country_ie) {
      struct country_ie_t *country_ie;

      /* AP we are connecting to has no Country IE - use from the first found */
      country_ie = mtlk_cache_find_first_country_ie(
                      &nic->slow_ctx->cache,
                      mtlk_core_get_country_code(nic));
      if (country_ie) {
        ILOG1(GID_IOCTL, "%s: Updating regulation limits from the first found BSS's country IE", dev->name);
        mtlk_update_reg_limit_table(HANDLE_T(nic), country_ie, bss_found.power);
        mtlk_osal_mem_free(country_ie);
      }
    } else {
      ILOG1(GID_IOCTL, "%s: Updating regulation limits from the current BSS's country IE", dev->name);
      mtlk_update_reg_limit_table(HANDLE_T(nic), bss_found.country_ie, bss_found.power);
    }

    if (bss_found.country_code) {
      nic->slow_ctx->cfg.country_code = bss_found.country_code;
      ILOG1(GID_IOCTL, "%s: Adopted country code: %s(0x%02x)",
          dev->name, country_code_to_country(bss_found.country_code),bss_found.country_code);
    }
  }
  
  nic->slow_ctx->channel = bss_found.channel;
  /* save ESSID */
  memcpy(nic->slow_ctx->essid, bss_found.essid, sizeof(nic->slow_ctx->essid));
  /* save BSSID so we can use it on activation */
  memcpy(nic->slow_ctx->bssid, sa->sa_data, sizeof(nic->slow_ctx->bssid));
  /* set bonding according to the AP */
  nic->slow_ctx->bonding = bss_found.upper_lower;

  /* set current frequency band */
  freq = channel_to_band(nic->slow_ctx->channel);
  nic->slow_ctx->frequency_band_cur = freq;

  /* set current HT capabilities */
  if (mtlk_cache_is_wep_enabled(&bss_found) && nic->slow_ctx->wep_enabled) {
    /* no HT is allowed for WEP connections */
    nic->slow_ctx->is_ht_cur = 0;
  }
  else {
    nic->slow_ctx->is_ht_cur = (nic->slow_ctx->is_ht_cfg && bss_found.is_ht);
  }

  /* set TKIP */
  nic->slow_ctx->is_tkip = 0;
  if (nic->slow_ctx->is_ht_cur && nic->slow_ctx->rsnie.au8RsnIe[0]) {
    struct wpa_ie_data d;
    ASSERT(nic->slow_ctx->rsnie.au8RsnIe[1]+2 <= sizeof(nic->slow_ctx->rsnie));
    if (wpa_parse_wpa_ie(nic->slow_ctx->rsnie.au8RsnIe,
                         nic->slow_ctx->rsnie.au8RsnIe[1] + 2, &d) < 0) {
      ELOG("%s: Can not parse WPA/RSN IE", dev->name);
      return -EINVAL;
    }
    if (d.pairwise_cipher == UMI_RSN_CIPHER_SUITE_TKIP) {
      WLOG("%s: Connection in HT mode with TKIP is prohibited by standard, trying non-HT mode...",
           dev->name);
      nic->slow_ctx->is_ht_cur = 0;
      nic->slow_ctx->is_tkip = 1;
    }
  }

  /* for STA spectrum mode should be set according to our own HT capabilities */
  if (nic->slow_ctx->is_ht_cur == 0) {
   /* e.g. if we connect to A/N AP, but STA is A then we should select 20MHz  */
    nic->slow_ctx->spectrum_mode = SPECTRUM_20MHZ;
  } else {
    nic->slow_ctx->spectrum_mode = bss_found.spectrum;

    if (SPECTRUM_40MHZ == bss_found.spectrum) {
      /* force set spectrum mode */
      if (   (SPECTRUM_20MHZ == nic->slow_ctx->sta_force_spectrum_mode)
          || (   (SPECTRUM_AUTO == nic->slow_ctx->sta_force_spectrum_mode)
              && (MTLK_HW_BAND_2_4_GHZ == nic->slow_ctx->frequency_band_cur))) {

        nic->slow_ctx->spectrum_mode = SPECTRUM_20MHZ;
      }
    }
  }
  mtlk_set_dec_mib_value(PRM_SPECTRUM_MODE, nic->slow_ctx->spectrum_mode, nic);
  ILOG3(GID_IOCTL, "%s: Set SpectrumMode: %s MHz", dev->name, nic->slow_ctx->spectrum_mode ? "40": "20");

  /* set current Network Mode */
  /* previously set network mode shouldn't be overriden,
   * but in case it "MTLK_HW_BAND_BOTH" it need to be recalculated, this value is not
   * acceptable for MAC! */
  if(MTLK_HW_BAND_BOTH == net_mode_to_band(nic->slow_ctx->net_mode_cur)) {
    nic->slow_ctx->net_mode_cur = get_net_mode(freq, nic->slow_ctx->is_ht_cur);
  }

  /* perform CB scan, but only once per band */
  if (!(nic->cb_scanned_bands & 
        (freq == MTLK_HW_BAND_2_4_GHZ ? CB_SCANNED_2_4 : CB_SCANNED_5_2)))
  {
    /* perform CB scan to collect CB calibration data */
    if ((result = mtlk_scan_sync(&nic->slow_ctx->scan, freq, 1)) != MTLK_ERR_OK)
      return -EAGAIN;

    if (mtlk_core_is_stopping(nic))
      return -ESHUTDOWN;

    /* add the band to CB scanned ones */
    nic->cb_scanned_bands |= (freq == MTLK_HW_BAND_2_4_GHZ ? CB_SCANNED_2_4 : CB_SCANNED_5_2);
  }

  /* Load Progmodel */
  result = mtlk_progmodel_load(nic->slow_ctx->hw_cfg.txmm, nic, freq, 0, 0);
  if (MTLK_ERR_OK != result) {
    ELOG("%s: Can't configure progmodel for connection. (err=%d)", dev->name, result);
    return -EFAULT;
  }

  if (mtlk_core_is_stopping(nic))
    return -ESHUTDOWN;

  result = mtlk_aux_pm_related_params_set_bss_based(
              nic->slow_ctx->hw_cfg.txmm,
              &bss_found,
              nic->slow_ctx->net_mode_cur,
              nic->slow_ctx->spectrum_mode,
              &pm_params);
  if (MTLK_ERR_OK !=  result) {
    ELOG("%s: Can't set PM related params (err=%d)", dev->name, result);
    return -EFAULT;
  }

  mtlk_mib_update_pm_related_mibs(nic, &pm_params);

  /* Activate connection to BSS */
  if (mtlk_core_is_stopping(nic) || (MTLK_ERR_OK != mtlk_send_activate(nic))) {
      /* rollback network mode in case of failure */
      nic->slow_ctx->net_mode_cur = nic->slow_ctx->net_mode_cfg;
      return -EAGAIN;
  }

  return 0;
}

static int
iw_stop_lower_mac (struct net_device *dev,
                   struct iw_request_info *info,
                   union iwreq_data *wrqu, char *extra)
{
  int res = 0;
  struct nic *nic;
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t* man_entry = NULL;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  nic = netdev_priv(dev);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, nic->slow_ctx->hw_cfg.txmm, NULL);
  if (man_entry == NULL) {
    ELOG("Can't stop lower MAC due to the lack of MAN_MSG");
    res = -EINVAL;
    goto end;
  }

  man_entry->id           = UM_LM_STOP_REQ;
  man_entry->payload_size = 0;

  if (mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
    ELOG("Can't stop lower MAC, timed-out");
    res = -EINVAL;
    goto end;
  }

end:
  if (man_entry)
    mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}

static int
iw_mac_calibrate (struct net_device *dev,
                  struct iw_request_info *info,
                  union iwreq_data *wrqu, char *extra)
{
  int res = 0;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t* man_entry = NULL;
  struct nic *nic;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  nic = netdev_priv(dev);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, nic->slow_ctx->hw_cfg.txmm, NULL);
  if (man_entry == NULL) {
    ELOG("Can't calibrate due to the lack of MAN_MSG");
    res = -EINVAL;
    goto end;
  }

  man_entry->id           = UM_PER_CHANNEL_CALIBR_REQ;
  man_entry->payload_size = 0;

  if (mtlk_txmm_msg_send_blocked(&man_msg,
                                 MTLK_MM_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
    ELOG("Can't calibrate, timed-out");
    res = -EINVAL;
    goto end;
  }

end:
  if (man_entry)
    mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}

static int
iw_generic (struct net_device *dev,
            struct iw_request_info *info,
            union iwreq_data *wrqu, char *extra)
{
  int res = 0;
  UMI_GENERIC_MAC_REQUEST* preq;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t* man_entry = NULL;
  struct nic *nic;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  nic = netdev_priv(dev);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, nic->slow_ctx->hw_cfg.txmm, NULL);
  if (man_entry == NULL) {
    ELOG("Can't send request to MAC due to the lack of MAN_MSG");
    res = -EINVAL;
    goto end;
  }

  preq = (UMI_GENERIC_MAC_REQUEST*)man_entry->payload;
  if (copy_from_user(preq, wrqu->data.pointer, sizeof(*preq)) != 0) {
    res = -EINVAL;
    goto end;
  }

  ILOG2(GID_IOCTL, "Generic opcode %ux, %ud dwords, action 0x%ux results 0x%ux 0x%ux 0x%ux\n",
         (unsigned int)preq->opcode,
         (unsigned int)preq->size,
         (unsigned int)preq->action,
         (unsigned int)preq->res0,
         (unsigned int)preq->res1,
         (unsigned int)preq->res2);

  man_entry->id           = UM_MAN_GENERIC_MAC_REQ;
  man_entry->payload_size = sizeof(*preq);

  preq->opcode=  cpu_to_le32(preq->opcode);
  preq->size=  cpu_to_le32(preq->size);
  preq->action=  cpu_to_le32(preq->action);
  preq->res0=  cpu_to_le32(preq->res0);
  preq->res1=  cpu_to_le32(preq->res1);
  preq->res2=  cpu_to_le32(preq->res2);
  preq->retStatus=  cpu_to_le32(preq->retStatus);

  if (mtlk_txmm_msg_send_blocked(&man_msg,
                                 MTLK_MM_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
    ELOG("Can't send generic request to MAC, timed-out");
    res = -EINVAL;
    goto end;
  }

  //  Check status ??

  preq->opcode=  cpu_to_le32(preq->opcode);
  preq->size=  cpu_to_le32(preq->size);
  preq->action=  cpu_to_le32(preq->action);
  preq->res0=  cpu_to_le32(preq->res0);
  preq->res1=  cpu_to_le32(preq->res1);
  preq->res2=  cpu_to_le32(preq->res2);
  preq->retStatus=  cpu_to_le32(preq->retStatus);

  ILOG2(GID_IOCTL, "Generic opcode %ux, %ud dwords, action 0x%ux results 0x%ux 0x%ux 0x%ux\n",
         (unsigned int)preq->opcode,
         (unsigned int)preq->size,
         (unsigned int)preq->action,
         (unsigned int)preq->res0,
         (unsigned int)preq->res1,
         (unsigned int)preq->res2);

  if (copy_to_user(wrqu->data.pointer, preq, sizeof(*preq)) != 0)
    res = -EINVAL;
end:
  if (man_entry)
    mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}

static int 
iw_bcl_drv_data_exchange (struct net_device *dev,
            struct iw_request_info *info,
            union iwreq_data *wrqu, char *extra)
{ 
  struct nic *nic = netdev_priv(dev);
  BCL_DRV_DATA_EX_REQUEST *preq = NULL;
  char* pdata = NULL;

  int rslt = 0;
  //struct nic *nic = netdev_priv(dev);
  
  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);
  
  if ((preq = kmalloc_tag(sizeof(*preq), GFP_ATOMIC, MTLK_MEM_TAG_IOCTL)) == NULL) {
    rslt = -ENOMEM;
    goto cleanup;
  }
    
    
  if (copy_from_user(preq, wrqu->data.pointer, sizeof(*preq))) {
    rslt = -EINVAL;
    goto cleanup;
  }

  switch (preq->mode) {
  case BclDrvModeCatInit:
    // Make sure there's enough space to fit the counter:
    if (preq->datalen != sizeof(uint32)) {
      rslt = -EINVAL;
      goto cleanup;
    }
    // Return category items counter to BCLSockServer:
    {
      uint32 item_cnt = 0;
      if (mtlk_debug_bcl_category_init(nic, preq->category, /* out */ &item_cnt) != 0) {
        rslt = -EINVAL;
        goto cleanup;
      }
      if (copy_to_user(wrqu->data.pointer + sizeof(*preq),
          &item_cnt, sizeof(uint32)) != 0) {
        rslt = -EINVAL;
        goto cleanup;
      }
    }
    break;
  case BclDrvModeCatFree:
    if (mtlk_debug_bcl_category_free(nic, preq->category) != 0) {
      rslt = -EINVAL;
      goto cleanup;
    }
    break;
  case BclDrvModeNameGet:
    if ((pdata = kmalloc_tag(preq->datalen * sizeof(char), GFP_ATOMIC, MTLK_MEM_TAG_IOCTL)) == NULL) {
      rslt = -ENOMEM;
      goto cleanup;
    }
    if (mtlk_debug_bcl_name_get(nic, preq->category, preq->index, pdata,
        preq->datalen) != 0) {
      rslt = -EINVAL;
      goto cleanup;
    }
    if (copy_to_user(wrqu->data.pointer + sizeof(*preq),
        pdata, strlen(pdata) + 1)) {
      rslt = -EINVAL;
      goto cleanup;
    }
    break;
  case BclDrvModeValGet:
    // Make sure there's enough space to store the value:
    if (preq->datalen != sizeof(uint32)) {
      rslt = -EINVAL;
      goto cleanup;
    }
    // Return the value to BCLSockServer:
    {
      uint32 val = 0;
      if (mtlk_debug_bcl_val_get(nic, preq->category, preq->index,
          /* out */ &val) != 0) {
        rslt = -EINVAL;
        goto cleanup;
      }
      if (copy_to_user(wrqu->data.pointer + sizeof(*preq),
          &val, sizeof(uint32)) != 0) {
        rslt = -EINVAL;
        goto cleanup;
      }
    }
    break;
  case BclDrvModeValSet:
    // Make sure the value is present:
    if (preq->datalen != sizeof(uint32)) {
      rslt = -EINVAL;
      goto cleanup;
    }
    // Process the value:
    {
      uint32 val = 0;
      if (copy_from_user(&val, wrqu->data.pointer + sizeof(*preq),
          sizeof(uint32)) != 0) {
        rslt = -EINVAL;
        goto cleanup;
      }
      if (mtlk_debug_bcl_val_put(nic, preq->category, preq->index, val) != 0) {
        rslt = -EINVAL;
        goto cleanup;
      }
    }
    break;
  default:
    ELOG("Unknown data exchange mode (%u)", preq->mode);
    rslt = -EINVAL;
    goto cleanup;
  }


/*

  if (copy_from_user(preq, wrqu->data.pointer, sizeof(*preq)))
*/


cleanup:
  if (preq)
    kfree_tag(preq);
  if (pdata)
    kfree_tag(pdata);

  return rslt;
}

struct iw_statistics *
mtlk_linux_get_iw_stats (struct net_device *dev)
{
  struct nic *nic = netdev_priv(dev);
  
  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);
  
  nic->slow_ctx->iw_stats.status = mtlk_core_get_net_state(nic);
  
  read_lock_bh(&nic->slow_ctx->stat_lock);

  if (!nic->slow_ctx->hw_cfg.ap && (mtlk_core_get_net_state(nic) == NET_STATE_CONNECTED)) {
    uint8 *rssi = nic->slow_ctx->stadb.connected_stations[0].rssi;
    uint8 max_rssi = MAX(rssi[0], MAX(rssi[1], rssi[2]));
    nic->slow_ctx->iw_stats.qual.updated = IW_QUAL_ALL_UPDATED | IW_QUAL_DBM;
    nic->slow_ctx->iw_stats.qual.noise = nic->slow_ctx->noise;
    nic->slow_ctx->iw_stats.qual.level = max_rssi;
    nic->slow_ctx->iw_stats.qual.qual = mtlk_core_calc_signal_strength((int)max_rssi - 256);
  }

  nic->slow_ctx->iw_stats.discard.fragment = nic->slow_ctx->mac_stat[STAT_FRAGMENT_FAILED];
  nic->slow_ctx->iw_stats.discard.retries = nic->slow_ctx->mac_stat[STAT_TX_MAX_RETRY];
  nic->slow_ctx->iw_stats.discard.code =
    + nic->slow_ctx->mac_stat[STAT_WEP_UNDECRYPTABLE]
    + nic->slow_ctx->mac_stat[STAT_WEP_ICV_ERROR]
    + nic->slow_ctx->mac_stat[STAT_WEP_EXCLUDED]
    + nic->slow_ctx->mac_stat[STAT_DECRYPTION_FAILED];
  nic->slow_ctx->iw_stats.discard.misc =
    + nic->slow_ctx->mac_stat[STAT_RX_DISCARD]
    + nic->slow_ctx->mac_stat[STAT_TX_FAIL]
    - nic->slow_ctx->iw_stats.discard.code
    - nic->slow_ctx->iw_stats.discard.retries;

  read_unlock_bh(&nic->slow_ctx->stat_lock);

  return &nic->slow_ctx->iw_stats;
} 

static int
iw_handler_nop (struct net_device *dev,
                struct iw_request_info *info,
                union iwreq_data *wrqu,
                char *extra)
{
  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);
  return 0;
}

static int
mtlk_linux_ioctl_getname (struct net_device *dev,
                          struct iw_request_info *info,
                          char *name,
                          char *extra)
{
  struct nic *nic = netdev_priv(dev);
  uint8 net_mode;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (mtlk_core_get_net_state(nic) == NET_STATE_CONNECTED)
    net_mode = nic->slow_ctx->net_mode_cur;
  else 
    net_mode = nic->slow_ctx->net_mode_cfg;
  
  strcpy(name, net_mode_to_string(net_mode));

  return 0;
}

static int
mtlk_ioctl_get_wmce_stats (struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu,
                           char *extra)
{
  struct nic *nic = netdev_priv(dev);
  WLAN_WIRELESS_STATISTICS stats;
  mtlk_txmm_msg_t     man_msg;
  UMI_GET_STATISTICS *pmac_stats;
  int res = 0;

  memset(&stats, 0, sizeof(stats));

  if (mtlk_txmm_msg_init(&man_msg) != MTLK_ERR_OK) {
    ELOG("Unable to init MM");
    goto end_no_mm_init;
  }

  pmac_stats = mac_get_stats(nic, &man_msg);
  if (!pmac_stats) {
    ELOG("Unable to get statistics from MAC");
    goto end;
  }

  stats.TransmittedFragmentCount = le32_to_cpu(
      pmac_stats->sStats.au32Statistics[STAT_TX_MSDU]);
  stats.MulticastTransmittedFrameCount = le32_to_cpu(
      pmac_stats->sStats.au32Statistics[STAT_TX_MULTICAST_DATA]);
  stats.FailedCount = le32_to_cpu(
      pmac_stats->sStats.au32Statistics[STAT_TX_FAIL]);
  stats.RetryCount = le32_to_cpu(
      pmac_stats->sStats.au32Statistics[STAT_TX_RETRY]);
  stats.MultipleRetryCount = le32_to_cpu(
      pmac_stats->sStats.au32Statistics[STAT_TX_MAX_RETRY]);
  stats.RTSSuccessCount = le32_to_cpu(
      pmac_stats->sStats.au32Statistics[STAT_TX_RTS_SUCCESS]);
  stats.RTSFailureCount = le32_to_cpu(
      pmac_stats->sStats.au32Statistics[STAT_TX_RTS_FAIL]);
  stats.ACKFailureCount = le32_to_cpu(
      pmac_stats->sStats.au32Statistics[STAT_TX_RETRY]);
  stats.FrameDuplicateCount = le32_to_cpu(
      pmac_stats->sStats.au32Statistics[STAT_RX_DUPLICATE]);
  stats.ReceivedFragmentCount = le32_to_cpu(
      pmac_stats->sStats.au32Statistics[STAT_RX_UNICAST_DATA]);
  stats.MulticastReceivedFrameCount = le32_to_cpu(
      pmac_stats->sStats.au32Statistics[STAT_RX_MULTICAST_DATA]);
  stats.FCSErrorCount = le32_to_cpu(
      pmac_stats->sStats.au32Statistics[STAT_FCS_ERROR]);

end:
  mtlk_txmm_msg_cleanup(&man_msg);

end_no_mm_init:
  if (copy_to_user(wrqu->data.pointer, &stats, sizeof(stats)) != 0) {
    ELOG("Unable to copy WMCE data");
    res = -EINVAL;
  }
  return res;
}

static int
mtlk_ioctl_drv_gen_data_exchange (struct nic *nic, struct ifreq *ifr)
{
  WE_GEN_DATAEX_REQUEST req;
  WE_GEN_DATAEX_RESPONSE resp;
  int res = 0;
  const char *reason = "system error";

  // Make sure no fields will be left uninitialized by command handler
  memset(&resp, 0, sizeof(resp));

  if (copy_from_user(&req, ifr->ifr_data, sizeof(req)) != 0) {
    res = -EINVAL;
    goto end;
  }

  if (req.ver != WE_GEN_DATAEX_PROTO_VER) {
    uint32 proto_ver;
    WLOG("Data exchange protocol version mismatch (version is %u, expected %u)", req.ver, WE_GEN_DATAEX_PROTO_VER);
    resp.status = WE_GEN_DATAEX_PROTO_MISMATCH;
    resp.datalen = sizeof(uint32);
    proto_ver = WE_GEN_DATAEX_PROTO_VER;
    if (copy_to_user(ifr->ifr_data + sizeof(WE_GEN_DATAEX_RESPONSE),
          &proto_ver, sizeof(uint32)) != 0) {
      res = -EINVAL;
    }
    goto end;
  }

  switch (req.cmd_id) {
  case WE_GEN_DATAEX_CMD_CONNECTION_STATS:
    res = mtlk_core_gen_dataex_get_connection_stats(nic, &req, &resp,
        ifr->ifr_data + sizeof(WE_GEN_DATAEX_RESPONSE));
    break;
  case WE_GEN_DATAEX_CMD_STATUS:
    res = mtlk_core_gen_dataex_get_status(nic, &req, &resp,
        ifr->ifr_data + sizeof(WE_GEN_DATAEX_RESPONSE));
    break;
  case WE_GEN_DATAEX_CMD_LEDS_MAC:
    res = mtlk_core_gen_dataex_send_mac_leds(nic, &req, &resp,
        ifr->ifr_data + sizeof(WE_GEN_DATAEX_REQUEST));
    break;

  default:
    WLOG("Data exchange protocol: unknown command %u", req.cmd_id);
    resp.status = WE_GEN_DATAEX_UNKNOWN_CMD;
    resp.datalen = 0;
    break;
  }
end:
  if (res != 0 && mtlk_txmm_is_running(nic->slow_ctx->hw_cfg.txmm)) {
    ELOG("Error during data exchange request");
  } else {
    if (resp.status == WE_GEN_DATAEX_FAIL) {
      // Return failure reason
      size_t reason_sz = strlen(reason) + 1;
      if (req.datalen >= reason_sz) {
        if (copy_to_user(ifr->ifr_data + sizeof(WE_GEN_DATAEX_RESPONSE),
              reason, reason_sz) != 0) {
          res = -EINVAL;
          goto end;
        }
        resp.datalen = reason_sz;
      } else {
        resp.datalen = 0; // error string does not fit
      }
    }
    if (copy_to_user(ifr->ifr_data, &resp, sizeof(resp)) != 0) {
      res = -EINVAL;
      goto end;
    }
  }
  return res;
}

int
mtlk_ioctl_do_ioctl (struct net_device *dev, struct ifreq *ifr, int cmd)
{
  struct nic *nic = netdev_priv(dev);
  int res = -EOPNOTSUPP;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i), cmd is 0x%04x", dev->name, current->comm, current->pid, cmd);
  /* we only support private ioctls */
  if ((cmd < MTLK_IOCTL_FIRST) || (cmd > MTLK_IOCTL_LAST))
    goto FINISH;
  switch (cmd) {
  case MTLK_IOCTL_DATAEX:
    res = mtlk_ioctl_drv_gen_data_exchange(nic, ifr);
    break;
  case MTLK_IOCTL_IRBM:
    res = mtlk_irb_media_ioctl_on_drv_call(ifr->ifr_data);
    break;
  default:
    ELOG("ioctl not supported: 0x%04x", cmd);
    break;
  }
FINISH:
  return res;
}

static int
mtlk_linux_ioctl_setfreq (struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu,
                          char *extra)
{
  struct nic *nic = netdev_priv(dev);
  int retval = 0;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (!nic->slow_ctx->hw_cfg.ap) {
    ILOG2(GID_IOCTL, "%s: Channel selection unsupported on STA.", dev->name);
    retval = -EOPNOTSUPP;
    goto end;
  }

/*
 * To enable AOCS
 * with WE < 17 use 'iwconfig <IF> channel 0'
 * with WE >= 17 also can use 'iwconfig <IF> channel auto'
 */

#if WIRELESS_EXT < 17
  if ((wrqu->freq.e == 0) && (wrqu->freq.m == 0)) {
#else
  if ( ((wrqu->freq.e == 0) && (wrqu->freq.m == 0)) ||
       ((IW_FREQ_FIXED & wrqu->freq.flags) == 0) ) {
#endif /* WIRELESS_EXT < 17 */
      ILOG2(GID_IOCTL, "%s: Enable channel autoselection", dev->name);
      nic->slow_ctx->channel = 0;
      goto end;
  }

  if (wrqu->freq.e == 0) {
    mtlk_get_channel_data_t param;

    if (!nic->slow_ctx->cfg.country_code) {
      WLOG("%s: Set channel to %i. (AP Workaround due to invalid Driver parameters setting at BSP startup)",
          dev->name, wrqu->freq.m);
      nic->slow_ctx->channel = wrqu->freq.m;
      goto end;
    }

    param.reg_domain = country_code_to_domain(nic->slow_ctx->cfg.country_code);
    param.is_ht = nic->slow_ctx->is_ht_cfg;
    param.ap = nic->ap;
    param.bonding = nic->slow_ctx->bonding;
    param.spectrum_mode = nic->slow_ctx->spectrum_mode;
    param.frequency_band = nic->slow_ctx->frequency_band_cfg;
    param.disable_sm_channels = mtlk_core_is_sm_channels_disabled(nic);

    if (MTLK_ERR_OK == mtlk_check_channel(&param, wrqu->freq.m)) {
      ILOG1(GID_IOCTL, "%s: Set channel to %i", dev->name, wrqu->freq.m);
      nic->slow_ctx->channel = wrqu->freq.m;
      goto end;
    }
  }

  WLOG("%s: Channel (%i) is not supported in current configuration.", dev->name, wrqu->freq.m);
  mtlk_core_configuration_dump(nic);
  retval = -EINVAL;

end:
  return retval;
}

static int
mtlk_linux_ioctl_getfreq (struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu,
                          char *extra)
{
  struct nic *nic = netdev_priv(dev);
  int value = nic->slow_ctx->channel;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (value == 0) {
    ILOG3(GID_IOCTL, "Channel autoselection enabled on %s", dev->name);
    wrqu->freq.e = 0;
    wrqu->freq.m = 0;
    wrqu->freq.i = 0;
#if WIRELESS_EXT >= 17
    wrqu->freq.flags = IW_FREQ_AUTO;
#endif
  } else {
    ILOG3(GID_IOCTL, "Value of channel is %i on %s", value, dev->name);
    wrqu->freq.e = 0;
    wrqu->freq.m = value;
    wrqu->freq.i = 0;
#if WIRELESS_EXT >= 17
    wrqu->freq.flags = IW_FREQ_FIXED;
#endif
  }

  return 0;
}

static int
mtlk_linux_ioctl_getmode (struct net_device *dev,
                                 struct iw_request_info *info,
                                 u32 *mode,
                                 char *extra)
{
  struct nic *nic = netdev_priv(dev);

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (nic->slow_ctx->hw_cfg.ap)
    *mode = IW_MODE_MASTER;
  else
    *mode = IW_MODE_INFRA;

  return 0;
}

static int
mtlk_linux_ioctl_getaplist (struct net_device *dev,
                            struct iw_request_info *info,
                            union iwreq_data *wrqu,
                            char *extra)
{
  struct nic *nic = netdev_priv(dev);
  int retval;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (0 == nic->slow_ctx->hw_cfg.ap) {
    /* This is STA - do not print anything */
    retval = -EOPNOTSUPP;
    wrqu->data.length = 0;
  } else {
    /* This is AP - print list of connected STAs */
    struct sockaddr *list = (struct sockaddr*)extra;
    sta_entry *sta = nic->slow_ctx->stadb.connected_stations;
    int i, j = 0;
    for (i = 0; i < STA_MAX_STATIONS; i++, sta++) {
      if (j == wrqu->data.length) break;

      if (sta->state == PEER_UNUSED) continue;

      /* In WPA security don't print STAs with which 4-way handshake
       * is not completed yet
       */
      if (nic->group_cipher && !sta->cipher && !sta->peer_ap)
        continue;

      list[j].sa_family = ARPHRD_ETHER;
      memcpy(list[j].sa_data, sta->mac, ETH_ALEN);
      j++;
    }
    wrqu->data.length = j;
    retval = 0;
  }

  return retval;
}

static int
mtlk_ctrl_mac_gpio (struct net_device *dev,
                         struct iw_request_info *info,
                         union iwreq_data *wrqu,
                         char *extra)
{
  struct nic *nic = netdev_priv(dev);
  //union mtlk_param_value value;
  int retval = -EINVAL;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  int *ptr_extra = (int *)extra;
  UMI_SET_LED* pdata;
  UMI_SET_LED leds_status;

  ILOG3(GID_IOCTL, "ptr_extra = 0x%x, ptr_extra1 = 0x%x",
       ptr_extra[0], ptr_extra[1]);

  memset(&leds_status, 0, sizeof(leds_status));

  leds_status.u8BasebLed = (uint8)(ptr_extra[0] & 0xFF);
  leds_status.u8LedStatus = (uint8)(ptr_extra[1] & 0xFF);


  ILOG3(GID_IOCTL, "u8BasebLed = %d, u8LedStatus = %d",
    leds_status.u8BasebLed, leds_status.u8LedStatus);

  if (nic->slow_ctx->scan.initialized == 1) {
    man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, nic->slow_ctx->hw_cfg.txmm, NULL);
    if (!man_entry) {
      ELOG("No MM entry to set BCMC_RATE");
      retval = -EINVAL;
      goto end;
    }
    pdata = (UMI_SET_LED*)man_entry->payload;
    man_entry->id           = UM_MAN_SET_LED_REQ;
    man_entry->payload_size = sizeof(leds_status);
    pdata->u8BasebLed = leds_status.u8BasebLed;
    pdata->u8LedStatus = leds_status.u8LedStatus;
    retval = mtlk_txmm_msg_send_blocked(&man_msg,
                                        MTLK_MM_BLOCKED_SEND_TIMEOUT);

    mtlk_txmm_msg_cleanup(&man_msg);
  }

  retval = 0;

end:
  return retval;
}

static int
mtlk_linux_ioctl_settxpower (struct net_device *dev,
                             struct iw_request_info *info,
                             union iwreq_data *wrqu,
                             char *extra)
{
  struct nic *nic = netdev_priv(dev);
  int value;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (wrqu->txpower.value > 0) {
    ILOG2(GID_IOCTL, "Set TxPower to specified value %i mW", wrqu->txpower.value);
    value = wrqu->txpower.value;
  } else {
    ILOG2(GID_IOCTL, "Enable TxPower auto-calculation");
    value = 0; /* Auto */
  }
  mtlk_set_hex_mib_value(PRM_TX_POWER, value, nic);

  return 0;
}

static int
mtlk_linux_ioctl_gettxpower (struct net_device *dev,
                             struct iw_request_info *info,
                             union iwreq_data *wrqu,
                             char *extra)
{
  struct nic *nic = netdev_priv(dev);
  int    value;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  value = mtlk_aux_atol(mtlk_get_mib_value(PRM_TX_POWER, nic));

  if (value) {
    wrqu->txpower.value = value;
    wrqu->txpower.fixed = 1;
    wrqu->txpower.flags = IW_TXPOW_DBM;
    wrqu->txpower.disabled = 0;
    return 0;
  } else {
    return -EAGAIN;
  }
}

static int
mtlk_linux_ioctl_getap (struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu,
                        char *extra)
{
  struct nic *nic = netdev_priv(dev);

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  wrqu->addr.sa_family = ARPHRD_ETHER;
  if (nic->net_state == NET_STATE_CONNECTED)
    memcpy(wrqu->addr.sa_data, nic->slow_ctx->bssid, ETH_ALEN);
  else
    memset(wrqu->addr.sa_data, 0, ETH_ALEN);

  return 0;
}

#define RTS_THRESHOLD_MIN 100

static int
mtlk_linux_ioctl_setrtsthr (struct net_device *dev,
                            struct iw_request_info *info,
                            union iwreq_data *wrqu,
                            char *extra)
{
  struct nic *nic = netdev_priv(dev);
  mtlk_txmm_t *txmm = nic->slow_ctx->hw_cfg.txmm;
  uint16 value;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (mtlk_scan_is_running(&nic->slow_ctx->scan)) {
    ILOG1(GID_IOCTL, "%s: Request eliminated due to running scan", dev->name);
    return -EAGAIN;
  }

  if (wrqu->rts.value < 0)
    return -EINVAL;

  value = (wrqu->rts.value < RTS_THRESHOLD_MIN ? RTS_THRESHOLD_MIN : wrqu->rts.value);

  ILOG2(GID_IOCTL, "Set RTS/CTS threshold to value %i", value);
  if (mtlk_set_mib_value_uint16(txmm, MIB_RTS_THRESHOLD, value) != MTLK_ERR_OK)
    return -EFAULT;

  return 0;
}

static int
mtlk_linux_ioctl_getrtsthr (struct net_device *dev,
                            struct iw_request_info *info,
                            union iwreq_data *wrqu,
                            char *extra)
{
  struct nic *nic = netdev_priv(dev);
  mtlk_txmm_t *txmm = nic->slow_ctx->hw_cfg.txmm;
  uint16 value;
  int res = 0;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (mtlk_scan_is_running(&nic->slow_ctx->scan)) {
    ILOG1(GID_IOCTL, "%s: Request eliminated due to running scan", dev->name);
    return -EAGAIN;
  }

  res = mtlk_get_mib_value_uint16(txmm, MIB_RTS_THRESHOLD, &value);
  wrqu->rts.value = value;
  if (res == MTLK_ERR_OK) {
    ILOG3(GID_IOCTL, "RTS/CTS threshold is %i", wrqu->rts.value);
    wrqu->rts.fixed = 1;
    wrqu->rts.disabled = 0;
    wrqu->rts.flags = 0;
  } else
    res = -EFAULT;

  return res;
}

static int
mtlk_linux_ioctl_setmlme (struct net_device *dev,
                                     struct iw_request_info *info,
                                     struct iw_point *data,
                                     char *extra)
{
  struct nic *nic = netdev_priv(dev);
  struct iw_mlme *mlme = (struct iw_mlme *) extra;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (mtlk_scan_is_running(&nic->slow_ctx->scan)) {
    ILOG1(GID_IOCTL, "%s: Request eliminated due to running scan", dev->name);
    return -EAGAIN;
  }

  ASSERT(mlme != NULL);
  ILOG3(GID_IOCTL, "cmd %i", mlme->cmd);

  if (mtlk_core_get_net_state(nic) != NET_STATE_CONNECTED) {
    ILOG1(GID_IOCTL, "%s: not connected - request rejected (%Y)", dev->name, mlme->addr.sa_data);
    return -EAGAIN;
  }

  switch (mlme->cmd) {
  case IW_MLME_DEAUTH:
  case IW_MLME_DISASSOC:
    ILOG1(GID_DISCONNECT, "%s: Got MLME Disconnect/Disassociate ioctl (%Y)", dev->name, mlme->addr.sa_data);
    if (!nic->slow_ctx->hw_cfg.ap) {
      return mtlk_disconnect_me(nic);
    }
    else if (!mtlk_osal_is_valid_ether_addr(mlme->addr.sa_data)) {
      ILOG1(GID_DISCONNECT, "%s: Invalid MAC address (%Y)!", dev->name, mlme->addr.sa_data);
      return -EINVAL;
    }
    else {
      return mtlk_disconnect_sta(nic, mlme->addr.sa_data);
    }
    break;
  default:
    return -EOPNOTSUPP;
  }
}

static int 
ioctl_set_wep_mib (struct nic *nic, int enable)
{
  int res = MTLK_ERR_NOT_SUPPORTED;

  if (enable && is_ht_net_mode(nic->slow_ctx->net_mode_cfg)) {
    if (nic->ap) {
      ELOG("AP: Can't set WEP for HT Network Mode (%s)", 
           net_mode_to_string(nic->slow_ctx->net_mode_cfg));
      goto end;
    }
    else if (!is_mixed_net_mode(nic->slow_ctx->net_mode_cfg)) {
      ELOG("STA: Can't set WEP for HT-only Network Mode (%s)",
        net_mode_to_string(nic->slow_ctx->net_mode_cfg));
      goto end;
    }
  }

  res = mtlk_set_mib_value_uint8(nic->slow_ctx->hw_cfg.txmm, 
                                   MIB_PRIVACY_INVOKED,
                                   enable);
  if (res == MTLK_ERR_OK) {
    nic->slow_ctx->wep_enabled = enable;
  }

end:
  return res;
}

static int
mtlk_linux_ioctl_setencext (struct net_device *dev,
                            struct iw_request_info *info,
                            struct iw_point *encoding,
                            char *extra)
{
  struct nic *nic = netdev_priv(dev);
  int alg;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_SET_KEY *umi_key;
  uint16 status;
  int res = -EINVAL, idx, key_len=0;
  u8 *addr;
  int sta_id = -1;
  struct iw_encode_ext *ext = NULL;
  sta_entry *sta = NULL;
  mtlk_pckt_filter_e sta_filter_stored = MTLK_PCKT_FLTR_ALLOW_ALL;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if ((mtlk_core_get_net_state(nic) & (NET_STATE_READY | NET_STATE_CONNECTED)) == 0) {
    ILOG1(GID_IOCTL, "Invalid card state - request rejected");
    res = -EAGAIN;
    goto end;
  }

  ext = (struct iw_encode_ext *)extra;
  if (ext == NULL)
    goto end;

  alg = ext->alg;

  ILOG2(GID_IOCTL, "alg %04x, enc flags %04x, ext flags %08x",
      alg, encoding->flags, ext->ext_flags);

  /* Determine and validate the key index */
  idx = encoding->flags & IW_ENCODE_INDEX;
  if (idx) {
    if (idx < 1 || idx > WEP_KEYS) {
      WLOG("SIOCSIWENCODEEXT: Invalid key index");
      goto end;
    }
    idx--;
  } else {
    idx = nic->slow_ctx->default_key;
    if (alg == IW_ENCODE_ALG_WEP)
      idx = nic->slow_ctx->default_wep_key;
  }

  if (ext->addr.sa_family != ARPHRD_ETHER) {
    WLOG("SIOCSIWENCODEEXT: Invalid address");
    goto end;
  }
  addr = ext->addr.sa_data;
  ILOG2(GID_IOCTL, "%Y", addr);

  if (encoding->flags & IW_ENCODE_DISABLED)
    alg = IW_ENCODE_ALG_NONE;

  if (ext->ext_flags & (IW_ENCODE_EXT_SET_TX_KEY|IW_ENCODE_EXT_GROUP_KEY)) {
    if (ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY) {
      if (alg == IW_ENCODE_ALG_WEP)
        nic->slow_ctx->default_wep_key = idx;
      else
        nic->slow_ctx->default_key = idx;
    }
    key_len = ext->key_len;
    if (key_len > (UMI_RSN_TK1_LEN + UMI_RSN_TK2_LEN)) {
      WLOG("SIOCSIWENCODEEXT: Invalid key length");
      goto end;
    }
  }
  ILOG2(GID_IOCTL, "key: idx %i, len %i", idx, ext->key_len);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, nic->slow_ctx->hw_cfg.txmm, NULL);
  if (!man_entry) {
    ELOG("No man entry available");
    goto end;
  }

  umi_key = (UMI_SET_KEY*)man_entry->payload;
  memset(umi_key, 0, sizeof(*umi_key));

  man_entry->id           = UM_MAN_SET_KEY_REQ;
  man_entry->payload_size = sizeof(*umi_key);

  /* ant: DO NOT SET THIS: supplicant is doing the job for us
   * (the job of swapping 16 bytes of umi_key->au8Tk2 in TKIP)
   * (umi_key->au8Tk2 is used in TKIP only)
  if (nic->ap)
    umi_key->u16StationRole = cpu_to_le16(UMI_RSN_AUTHENTICATOR);
  else
    umi_key->u16StationRole = cpu_to_le16(UMI_RSN_SUPPLICANT);
    */
  switch (alg) {
  case IW_ENCODE_ALG_NONE:
    umi_key->u16CipherSuite = cpu_to_le16(UMI_RSN_CIPHER_SUITE_NONE);
    break;
  case IW_ENCODE_ALG_WEP:
    umi_key->u16CipherSuite = cpu_to_le16(UMI_RSN_CIPHER_SUITE_WEP40);
    break;
  case IW_ENCODE_ALG_TKIP:
    umi_key->u16CipherSuite = cpu_to_le16(UMI_RSN_CIPHER_SUITE_TKIP);
    break;
  case IW_ENCODE_ALG_CCMP:
    umi_key->u16CipherSuite = cpu_to_le16(UMI_RSN_CIPHER_SUITE_CCMP);
    break;
  }

  if (mtlk_osal_eth_is_broadcast(addr)) {
    umi_key->u16KeyType = cpu_to_le16(UMI_RSN_GROUP_KEY);
    nic->group_cipher = alg;
    memset(nic->group_rsc[idx], 0, sizeof(nic->group_rsc[0]));
    if (ext->ext_flags & IW_ENCODE_EXT_RX_SEQ_VALID) {
      nic->group_rsc[idx][0] = ext->rx_seq[5];
      nic->group_rsc[idx][1] = ext->rx_seq[4];
      nic->group_rsc[idx][2] = ext->rx_seq[3];
      nic->group_rsc[idx][3] = ext->rx_seq[2];
      nic->group_rsc[idx][4] = ext->rx_seq[1];
      nic->group_rsc[idx][5] = ext->rx_seq[0];
    }
    DUMP2(nic->group_rsc[idx], sizeof(nic->group_rsc[0]), "group RSC");
  } else {
    int i;
    umi_key->u16KeyType = cpu_to_le16(UMI_RSN_PAIRWISE_KEY);
    sta_id = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, addr);
    if (sta_id == -1) {
    /* Supplicant reset keys for AP from which we just were disconnected */
      ILOG1(GID_IOCTL, "there is no connection with %Y", addr);
      goto end;
    }
    sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, sta_id);
    if (sta->state != PEER_CONNECTED) {
      ILOG1(GID_IOCTL, "there is no connection with %Y already", addr);
      goto end;
    }
    sta->cipher = alg;
    for (i=0; i < ARRAY_SIZE(sta->rod_queue); i++)
        mtlk_rod_queue_clear_reply_counter(&sta->rod_queue[i]);
    if (key_len == 0) {
      mtlk_stadb_sta_set_filter(sta, MTLK_PCKT_FLTR_ALLOW_802_1X);
      ILOG1(GID_IOCTL, "%Y: turn on 802.1x filtering", sta->mac);
    }
    
    /* Don't set the key until msg4 is sent to MAC */
    if (!nic->ap && key_len != 0)
      while (!mtlk_sq_is_empty(&sta->sq_peer_ctx))
        msleep(100);
  }

  memcpy(umi_key->sStationID.au8Addr, addr, ETH_ALEN);

  umi_key->u16DefaultKeyIndex = cpu_to_le16(idx);

  /* ant, 13 Apr 07: replay detection is performed by driver,
   * so MAC does not need this.
  if (ext->ext_flags & IW_ENCODE_EXT_RX_SEQ_VALID)
    memcpy(umi_key->au8RxSeqNum, ext->rx_seq, ARRAY_SIZE(umi_key->au8RxSeqNum));
    */
  if (key_len) {
    if (key_len > UMI_RSN_TK1_LEN*2) {
      ELOG("key length is too big  - %i bytes (more then %i bytes)",
          key_len, UMI_RSN_TK1_LEN*2);
      key_len = UMI_RSN_TK1_LEN*2;
    }
    memcpy(umi_key->au8Tk1, ext->key, key_len);
    umi_key->au8TxSeqNum[0] = 1;
  }

  if (alg == IW_ENCODE_ALG_WEP)
  {
    nic->slow_ctx->wep_keys.sKey[idx].u8KeyLength = key_len;
    memset(nic->slow_ctx->wep_keys.sKey[idx].au8KeyData, 0,
       sizeof(nic->slow_ctx->wep_keys.sKey[0].au8KeyData));
    if (key_len)
      memcpy(nic->slow_ctx->wep_keys.sKey[idx].au8KeyData,
        ext->key, key_len);

    mtlk_set_mib_value_raw(nic->slow_ctx->hw_cfg.txmm, MIB_WEP_DEFAULT_KEYS,
        (MIB_VALUE*)&nic->slow_ctx->wep_keys);

    res = ioctl_set_wep_mib(nic, 1);
    if (res != MTLK_ERR_OK) {
      WLOG("SIOCSIWENCODEEXT: Cannot set WEP (err=%d)", res);
      goto end;
    }

    mtlk_set_mib_value_uint8(nic->slow_ctx->hw_cfg.txmm, MIB_WEP_DEFAULT_KEYID,
      nic->slow_ctx->default_wep_key);

    res = 0;
  }

  /* It is preudo-loop which usually runs only one time.
   * The only case when it may run several times is WEP (see below) */
  for (;;) {
    UMI_SET_KEY stored_umi_key = *(UMI_SET_KEY*)man_entry->payload;
    mtlk_txmm_data_t stored_man_entry = *man_entry;
    int i;
    DUMP2(umi_key, sizeof(UMI_SET_KEY), "dump of UMI_SET_KEY msg:");
    
    /* stop packets from OS to station */
    if (sta) {
      sta_filter_stored = mtlk_stadb_sta_get_filter(sta);
      mtlk_stadb_sta_set_filter(sta, MTLK_PCKT_FLTR_DISCARD_ALL);
    
      /* drop all packets in sta sendqueue */
      mtlk_sq_peer_ctx_cancel_all_packets(nic->sq, &sta->sq_peer_ctx);

      /* wait till all messages to MAC to be confirmed */
      while (TRUE != mtlk_sq_is_all_packets_confirmed(&sta->sq_peer_ctx)) {
        mtlk_osal_msleep(10);
      }
    }

    res = mtlk_txmm_msg_send_blocked(&man_msg,
                                     MTLK_MM_BLOCKED_SEND_TIMEOUT);
    
    if (sta) {
      /* restore previous state of sta packets filter */
      mtlk_stadb_sta_set_filter(sta, sta_filter_stored);
    }

    if (res < MTLK_ERR_OK) {
      ELOG("mtlk_mm_send_blocked failed: %i", res);
      res = -EFAULT;
      goto end;
    }
    res = 0;
    status = le16_to_cpu(umi_key->u16Status);
    if (status != UMI_OK) {
      switch (status) {
      case UMI_NOT_SUPPORTED:
        WLOG("SIOCSIWENCODEEXT: RSN mode is disabled or an unsupported cipher suite was selected.");
        res = -EOPNOTSUPP;
        break;
      case UMI_STATION_UNKNOWN:
        WLOG("SIOCSIWENCODEEXT: Unknown station was selected.");
        res = -EINVAL;
        break;
      default:
        WLOG("invalid status of last msg %04x sending to MAC - %i",
            man_entry->id, status);
      }
      goto end;
    }

    /* Firmware (TalH) asked to always update default WEP key also */
    i = nic->slow_ctx->default_wep_key;
    if (alg == IW_ENCODE_ALG_WEP && umi_key->u16DefaultKeyIndex != cpu_to_le16(i)) {
      ILOG2(GID_IOCTL, "reset tx key according to default key idx %i", i);
      *(UMI_SET_KEY*)man_entry->payload = stored_umi_key;
      *man_entry = stored_man_entry;
      umi_key->u16DefaultKeyIndex = cpu_to_le16(i);
      memcpy(umi_key->au8Tk1, nic->slow_ctx->wep_keys.sKey[i].au8KeyData,
                              nic->slow_ctx->wep_keys.sKey[i].u8KeyLength);
      continue;
    }
    break;
  }

  if (sta_id != -1) {
    /* Now we have already set the keys => 
       we can start ADDBA and disable filter if required */
    sta_entry *sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, sta_id);

    if (key_len) {
      mtlk_stadb_sta_set_filter(sta, MTLK_PCKT_FLTR_ALLOW_ALL);
    }
  }

end:
  if (man_entry)
    mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}

static int
mtlk_linux_ioctl_getencext (struct net_device *dev,
                                     struct iw_request_info *info,
                                     struct iw_point *encoding,
                                     char *extra)
{
  struct nic *nic = netdev_priv(dev);
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t* man_entry = NULL;
  UMI_GROUP_PN *umi_gpn;
  int res = -EINVAL;
  uint32 net_state;
  struct iw_encode_ext *ext = NULL;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  ext = (struct iw_encode_ext *)extra;
  net_state = mtlk_core_get_net_state(nic);
  if (net_state == NET_STATE_HALTED || net_state == NET_STATE_IDLE) { 
    ILOG1(GID_IOCTL, "SIOCGIWENCODEEXT: invalid card state (%s)",
      mtlk_net_state_to_string(net_state));
    res = -EAGAIN;
    goto end;
  }
  if (ext == NULL)
    goto end;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, nic->slow_ctx->hw_cfg.txmm, NULL);
  if (!man_entry) {
    ELOG("No man entry available");
    res = -EAGAIN;
    goto end;
  }

  umi_gpn = (UMI_GROUP_PN*)man_entry->payload;
  memset(umi_gpn, 0, sizeof(UMI_GROUP_PN));

  man_entry->id           = UM_MAN_GET_GROUP_PN_REQ;
  man_entry->payload_size = sizeof(UMI_GROUP_PN);

  if (mtlk_txmm_msg_send_blocked(&man_msg,
                                 MTLK_MM_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
    ELOG("Timeout expired while waiting for CFM from MAC");
    goto end;
  }
  res = 0;
  umi_gpn->u16Status = le16_to_cpu(umi_gpn->u16Status);
  if (umi_gpn->u16Status != UMI_OK) {
    ELOG("GET_GROUP_PN_REQ failed: %i", umi_gpn->u16Status);
    res = -EAGAIN;
    goto end;
  }

  memcpy(ext->rx_seq, umi_gpn->au8TxSeqNum, 6);
  ILOG2(GID_IOCTL, "RSC:  %02x%02x%02x%02x%02x%02x",
      ext->rx_seq[0], ext->rx_seq[1], ext->rx_seq[2],
      ext->rx_seq[3], ext->rx_seq[4], ext->rx_seq[5]);

end:
  if (man_entry)
    mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}


static int
mtlk_linux_ioctl_setgenie (struct net_device *dev,
                                     struct iw_request_info *info,
                                     struct iw_point *data,
                                     char *extra)
{
  struct nic *nic = netdev_priv(dev);
  u8   *ie = (u8 *)extra;
  u8   *oui = ie +2;
  u8    wpa_oui[] = {0x00, 0x50, 0xf2, 0x01};
  u8    wps_oui[] = {0x00, 0x50, 0xf2, 0x04};
  int   ie_len = data->length;
  u8    rsn_control_mib = 0;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (   mtlk_scan_is_running(&nic->slow_ctx->scan)
      || mtlk_core_is_stopping(nic)) {
    ILOG1(GID_IOCTL, "%s: Request eliminated due to running scan", dev->name);
    return -EAGAIN;
  }

  ILOG2(GID_IOCTL, "IE %i, length %i", ie ? ie[0] : 0, ie_len);

  if (ie_len && ie == NULL)
    return -EINVAL;

  if (ie && ie_len && ie[0] == 0xdd && memcmp(oui, wpa_oui, sizeof(wpa_oui)) != 0) {
    ILOG2(GID_IOCTL, "WPS IE, type %i", data->flags);
    DUMP2(ie, ie_len, "dump of WPS IE:");
    // Set wps_in_progress flag. In AP mode parse WPS IE
    // and check Selected Registrar attribute. In STA mode
    // it is enough to check that WPS IE is not zero.
    if (nic->ap) {
      // Parse WPS IE and 
      u8 *p = ie;
      // Go to WPS OUI
      while (memcmp(oui, wps_oui, sizeof(wps_oui)) != 0) {
        p += p[1] + 2;
        if (p >= ie+ie_len) {
          WLOG("WPS OUI was not found");
          return -EINVAL;
        }
        oui = p + 2;
      }
      p = oui + sizeof(wps_oui);
      nic->slow_ctx->wps_in_progress = 0;
      while (p < ie+ie_len) {
        ILOG2(GID_IOCTL, "WPS IE, attr %04x", ntohs(*(uint16*)p));
        if (ntohs(get_unaligned((uint16*)p)) == 0x1041) { // Selected Registrar
          if (p[4] == 1)
            nic->slow_ctx->wps_in_progress = 1;
          break;
        }
        p += 4 + ntohs(get_unaligned((uint16*)(p+2)));  // Go to next attribute
      }
    } else {
      memset(&nic->slow_ctx->rsnie, 0, sizeof(nic->slow_ctx->rsnie));
      nic->slow_ctx->wps_in_progress = 1;
    }
    if (nic->slow_ctx->wps_in_progress)
      ILOG1(GID_IOCTL, "WPS in progress");
    else
      ILOG1(GID_IOCTL, "WPS stopped");
    return mtlk_core_set_gen_ie(nic, ie, ie_len, data->flags);
  }

  if (ie_len > sizeof(UMI_RSN_IE)) {
    WLOG("invalid RSN IE length (%i > %i)", ie_len, (int)sizeof(UMI_RSN_IE));
    return -EINVAL;
  }

  memset(&nic->slow_ctx->rsnie, 0, sizeof(nic->slow_ctx->rsnie));
  if (ie_len) {
    memcpy(&nic->slow_ctx->rsnie, ie, ie_len);
    DUMP2(&nic->slow_ctx->rsnie, ie_len, "dump of rsnie:");
    if (nic->ap && (ie[0] == 0xdd || ie[0] == 0x30))
      rsn_control_mib = 1;
  } else {  /* reset WPS IE case */
    // Note: in WPS mode data->flags represents the type of
    // WPS IE (for beacons, probe responces or probe reqs).
    // In STA mode flags == 1 (probe reqs type). So we
    // check it to avoid collisions with WPA IE reset.
#ifdef MTCFG_HOSTAP_06
    if (!nic->ap)
#else
    if (!nic->ap && data->flags)
#endif
    {
      nic->slow_ctx->wps_in_progress = 0;
      ILOG1(GID_IOCTL, "WPS stopped");
    }
    mtlk_core_set_gen_ie(nic, ie, ie_len, data->flags);
  }

  if (0 != mtlk_set_mib_rsn(nic->slow_ctx->hw_cfg.txmm, rsn_control_mib))
    return -EINVAL;

  return 0;
}


static int
mtlk_linux_ioctl_setauth (struct net_device *dev,
                          struct iw_request_info *info,
                          union  iwreq_data *wrqu,
                          char   *extra)
{
  struct iw_param *param = &wrqu->param;
  struct nic *nic = netdev_priv(dev);
  mtlk_txmm_t *txmm = nic->slow_ctx->hw_cfg.txmm;
  int res=0, auth=0;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (   mtlk_scan_is_running(&nic->slow_ctx->scan)
      || mtlk_core_is_stopping(nic)) {
    ILOG1(GID_IOCTL, "%s: Request eliminated due to running scan", dev->name);
    return -EAGAIN;
  }

  ILOG2(GID_IOCTL, "flags %i, value %i", param->flags, param->value);

  switch (param->flags & IW_AUTH_INDEX) {
  case IW_AUTH_WPA_VERSION:
    if ((param->value & IW_AUTH_WPA_VERSION_WPA) ||
        (param->value & IW_AUTH_WPA_VERSION_WPA2)) {
      if (0 != mtlk_set_mib_rsn(nic->slow_ctx->hw_cfg.txmm, 1))
        return -EINVAL;
      return 0;
    }
    break;

  case IW_AUTH_CIPHER_PAIRWISE:
  case IW_AUTH_CIPHER_GROUP:
    if (param->value & (IW_AUTH_CIPHER_WEP40|IW_AUTH_CIPHER_WEP104))
      auth = 1;
    res = (ioctl_set_wep_mib(nic, auth) == MTLK_ERR_OK)?0:-EINVAL;
    break;

  case IW_AUTH_KEY_MGMT:
  case IW_AUTH_PRIVACY_INVOKED:
  case IW_AUTH_RX_UNENCRYPTED_EAPOL:
  case IW_AUTH_DROP_UNENCRYPTED:
  case IW_AUTH_TKIP_COUNTERMEASURES:
    return 0;
  case IW_AUTH_80211_AUTH_ALG:
    if ((param->value & IW_AUTH_ALG_OPEN_SYSTEM) &&
        (param->value & IW_AUTH_ALG_SHARED_KEY)) {   /* automatic mode */
      auth = 2;
    } else if (param->value & IW_AUTH_ALG_SHARED_KEY) {
      auth = 1;
    } else if (param->value & IW_AUTH_ALG_OPEN_SYSTEM) {
      auth = 0;
    } else {
      return -EINVAL;
    }
    res = mtlk_set_mib_value_uint8(txmm, MIB_AUTHENTICATION_PREFERENCE, auth);
    if (res != MTLK_ERR_OK) {
      ELOG("Failed to switch access policy to %i", auth);
      res = -EFAULT;
      goto end;
    }
    ILOG1(GID_IOCTL, "Access policy switched to %i", auth);
    break;

  case IW_AUTH_WPA_ENABLED:
    return 0;

  default:
    return -EOPNOTSUPP;
  }

end:
  return res;
}

static int
iw_cipher_encode2auth(int enc_cipher)
{
  switch (enc_cipher) {
  case IW_ENCODE_ALG_NONE:
    return IW_AUTH_CIPHER_NONE;
  case IW_ENCODE_ALG_WEP:
    return IW_AUTH_CIPHER_WEP40;
  case IW_ENCODE_ALG_TKIP:
    return IW_AUTH_CIPHER_TKIP;
  case IW_ENCODE_ALG_CCMP:
    return IW_AUTH_CIPHER_CCMP;
  default:
    return 0;
  }
}

static int
mtlk_linux_ioctl_getauth (struct net_device *dev,
                                     struct iw_request_info *info,
                                     struct iw_param *param,
                                     char *extra)
{
  struct nic *nic = netdev_priv(dev);
  sta_entry *sta;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  ILOG2(GID_IOCTL, "flags %i, value %i", param->flags, param->value);

  param->value = 0;
  switch (param->flags & IW_AUTH_INDEX) {
  case IW_AUTH_WPA_VERSION:
    if (!nic->slow_ctx->rsnie.au8RsnIe[0])
      return -EINVAL;
    if (nic->slow_ctx->rsnie.au8RsnIe[0] == 0x30)
      param->value = IW_AUTH_WPA_VERSION_WPA2;
    else if (nic->slow_ctx->rsnie.au8RsnIe[0] == 0xdd)
      param->value = IW_AUTH_WPA_VERSION_WPA;
    return 0;

  case IW_AUTH_CIPHER_PAIRWISE:
    if (nic->slow_ctx->hw_cfg.ap)
      return -EOPNOTSUPP;
    sta = mtlk_stadb_get_sta_by_id(&nic->slow_ctx->stadb, 0);
    if (sta->state != PEER_CONNECTED)
      return -EINVAL;
    param->value = iw_cipher_encode2auth(sta->cipher);
    return 0;

  case IW_AUTH_CIPHER_GROUP:
    if (!nic->group_cipher)
      return -EINVAL;

    param->value = iw_cipher_encode2auth(nic->group_cipher);
    return 0;

  default:
    return -EOPNOTSUPP;
  }

  return 0;
}

static int
mtlk_linux_ioctl_getrange (struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu,
                           char *extra)
{
  struct nic *nic = netdev_priv(dev);
  struct iw_range *range = (struct iw_range *) extra;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  wrqu->data.length = sizeof(struct iw_range);
  /* In kernes < 2.6.22 range is not zeroed */
  memset(range, 0, sizeof(struct iw_range));

  range->we_version_compiled = WIRELESS_EXT;

  /* kernel MUST be patched up to this version if security is needed */
  if (WIRELESS_EXT < 18)
    range->we_version_compiled = 18;

  range->we_version_source = 18; /* what version we support */

  /* WEP stuff */
  range->num_encoding_sizes  = 2;
  range->encoding_size[0]    = 5;  /* 40 bit */
  range->encoding_size[1]    = 13; /* 104 bit */
  range->max_encoding_tokens = 4;

  /* Bitrate stuff */
  {
    int avail = mtlk_core_get_available_bitrates(nic);
    int value; /* Bitrate's value */
    int sm, scp; /* MIB values */
    int i; /* Bitrate index */
    int j; /* Index in table returned to userspace */
    int k, l; /* Counters, used for sorting */

    sm  = mtlk_aux_atol(mtlk_get_mib_value(PRM_SPECTRUM_MODE, nic));
    scp = mtlk_aux_atol(mtlk_get_mib_value(PRM_SHORT_CYCLIC_PREFIX, nic));

    /* Array of bitrates is sorted and consist of only unique elements */
    j = 0;
    for (i = BITRATE_FIRST; i <= BITRATE_LAST; i++) {
      if ((1 << i) & avail) {
        value = mtlk_bitrate_get_value(i, sm, scp);
        range->bitrate[j] = value;
        k = j;
        while (k && (range->bitrate[k-1] >= value)) k--; /* Position found */
        if ((k == j) || (range->bitrate[k] != value)) {
          for (l = j; l > k; l--)
            range->bitrate[l] = range->bitrate[l-1];
          range->bitrate[k] = value;
          j++;
        }
      }
    }

    ASSERT((0 < j) && (j <= IW_MAX_BITRATES));
    range->num_bitrates = j;
  }

  /* Channels stuff */
  {
    uint8 channels[MAX_CHANNELS];
    uint8 band;
    int num_channels = 0;
    mtlk_get_channel_data_t param;
    int i;

    range->num_frequency = 0;
    range->num_channels = 0;

    if (!nic->ap && mtlk_core_get_net_state(nic) == NET_STATE_CONNECTED)
      band = nic->slow_ctx->frequency_band_cur;
    else
      band = nic->slow_ctx->frequency_band_cfg;

    param.reg_domain = country_code_to_domain(nic->slow_ctx->cfg.country_code);
    param.is_ht = nic->slow_ctx->is_ht_cfg;
    param.ap = nic->ap;
    param.bonding = nic->slow_ctx->bonding;
    param.spectrum_mode = nic->slow_ctx->spectrum_mode;
    param.frequency_band = band;
    param.disable_sm_channels = mtlk_core_is_sm_channels_disabled(nic);

    num_channels = mtlk_get_avail_channels(&param, channels);

    if (0 <  num_channels) {
      for(i=0; i < num_channels; i++) {
        if (i >= IW_MAX_FREQUENCIES)
          break;
        range->freq[i].i = channels[i];
        range->freq[i].m = channel_to_frequency(channels[i]);
        range->freq[i].e = 6;
        //range->freq[i].flags = 0; /* TODO: fixed/auto (not used by iwlist currently)*/
        range->num_frequency++;
        range->num_channels = range->num_frequency;
      }
    }
  }

  /* TxPower stuff */
  range->txpower_capa = IW_TXPOW_DBM;

  /* Maximum quality */
  range->max_qual.qual = 5;

  return 0;
}

static int
iw_bcl_mac_data_get (struct net_device *dev,
            struct iw_request_info *info,
            union iwreq_data *wrqu, char *extra)
{
  int res;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t* man_entry = NULL;
  UMI_BCL_REQUEST* preq = NULL;
  struct nic *nic = netdev_priv(dev);
  int exception;
  mtlk_hw_state_e hw_state;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  hw_state = mtlk_core_get_hw_state(nic);
  exception = (((hw_state == MTLK_HW_STATE_EXCEPTION) || 
                (hw_state == MTLK_HW_STATE_APPFATAL)) &&
               !nic->slow_ctx->mac_stuck_detected_by_sw);
  if (exception)
  {
    if ((preq = kmalloc_tag(sizeof(*preq), GFP_ATOMIC, MTLK_MEM_TAG_IOCTL)) == NULL) {
      res = -ENOMEM;
      goto end;
    }

    if (copy_from_user(preq, wrqu->data.pointer, sizeof(*preq)) != 0) {
      res = -EINVAL;
      goto end;
    }

    mtlk_debug_bswap_bcl_request(preq, TRUE);

    mtlk_hw_get_prop(nic->hw, MTLK_HW_BCL_ON_EXCEPTION, preq, sizeof(*preq));
  }
  else
  {
    man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, nic->slow_ctx->hw_cfg.txmm, NULL);
    if (!man_entry) {
      ELOG("Can't send BCL request to MAC due to the lack of MAN_MSG");
      res = -EINVAL;
      goto end;
    }

    preq = (UMI_BCL_REQUEST*)man_entry->payload;
    if (copy_from_user(preq, wrqu->data.pointer, sizeof(*preq)) != 0) {
      res = -EINVAL;
      goto end;
    }

    ILOG4(GID_IOCTL, "Getting %u dwords from address 0x%x on unit %d (%x)",
         (unsigned int)preq->Size,
         (unsigned int)preq->Address,
         (int)preq->Unit,
         (unsigned int)preq->Data[0]);

    man_entry->id           = UM_MAN_QUERY_BCL_VALUE;
    man_entry->payload_size = sizeof(*preq);

    DUMP3(preq, sizeof(*preq), "dump of the UM_MAN_QUERY_BCL_VALUE");

    mtlk_debug_bswap_bcl_request(preq, TRUE);

    if (mtlk_txmm_msg_send_blocked(&man_msg,
                                   MTLK_MM_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
      ELOG("Can't send BCL request to MAC, timed-out");
      res = -EINVAL;
      goto end;
    }
  }

  mtlk_debug_bswap_bcl_request(preq, FALSE);

  res = 0;

  if (copy_to_user(wrqu->data.pointer, preq, sizeof(*preq)) != 0)
    res =  -EINVAL;

end:
  if (preq && exception)
    kfree_tag(preq);

  if (man_entry)
    mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}

static int
iw_bcl_mac_data_set (struct net_device *dev,
            struct iw_request_info *info,
            union iwreq_data *wrqu, char *extra)
{
  int res;
  UMI_BCL_REQUEST* preq = NULL;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t* man_entry = NULL;
  struct nic *nic = netdev_priv(dev);
  int exception;
  mtlk_hw_state_e hw_state;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  hw_state = mtlk_core_get_hw_state(nic);
  exception = (((hw_state == MTLK_HW_STATE_EXCEPTION) || 
                (hw_state == MTLK_HW_STATE_APPFATAL)) &&
               !nic->slow_ctx->mac_stuck_detected_by_sw);
  if (exception)
  {
    if ((preq = kmalloc_tag(sizeof(*preq), GFP_ATOMIC, MTLK_MEM_TAG_IOCTL)) == NULL)
      return -ENOMEM;

    if (copy_from_user(preq, wrqu->data.pointer, sizeof(*preq))) {
      res = -EINVAL;
      goto end;
    }

    mtlk_debug_bswap_bcl_request(preq, FALSE);

    mtlk_hw_set_prop(nic->hw, MTLK_HW_BCL_ON_EXCEPTION, preq, sizeof(*preq));
  }
  else
  {
    man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, nic->slow_ctx->hw_cfg.txmm, NULL);
    if (man_entry == NULL) {
      ELOG("Can't send BCL request to MAC due to the lack of MAN_MSG");
      res = -EINVAL;
      goto end;
    }

    preq = (UMI_BCL_REQUEST*)man_entry->payload;
    if (copy_from_user(preq, wrqu->data.pointer, sizeof(*preq)) != 0) {
      res = -EINVAL;
      goto end;
    }

    ILOG4(GID_IOCTL, "Setting %ud dwords from address 0x%ux on unit %ud (%ud)",
         (unsigned int)preq->Size,
         (unsigned int)preq->Address,
         (unsigned int)preq->Unit,
         (unsigned int)preq->Data[0]);

    man_entry->id           = UM_MAN_SET_BCL_VALUE;
    man_entry->payload_size = sizeof(*preq);

    mtlk_debug_bswap_bcl_request(preq, FALSE);

    if (mtlk_txmm_msg_send_blocked(&man_msg,
                                   MTLK_MM_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
      ELOG("Can't send BCL request to MAC, timed-out");
      res = -EINVAL;
      goto end;
    }
  }

  res = 0;

end:
  if (preq && exception)
    kfree_tag(preq);

  if (man_entry)
    mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}

static int
mtlk_linux_ioctl_setretry (struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu,
                           char *extra)
{
  struct nic *nic = netdev_priv(dev);
  mtlk_txmm_t *txmm = nic->slow_ctx->hw_cfg.txmm;
  uint16 obj_id;
  int    res = -EINVAL;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (mtlk_scan_is_running(&nic->slow_ctx->scan)) {
    ILOG1(GID_IOCTL, "%s: Request eliminated due to running scan", dev->name);
    return -EAGAIN;
  }

  /* Determine requested parameter */
  switch (wrqu->retry.flags) {
  case IW_RETRY_LIMIT:
  case IW_RETRY_LIMIT | IW_RETRY_MIN:
    obj_id = MIB_SHORT_RETRY_LIMIT;
    break;
  case IW_RETRY_LIMIT | IW_RETRY_MAX:
    obj_id = MIB_LONG_RETRY_LIMIT;
    break;
  case IW_RETRY_LIFETIME:
  case IW_RETRY_LIFETIME | IW_RETRY_MIN:
  case IW_RETRY_LIFETIME | IW_RETRY_MAX:
    /* WT send to us value in microseconds, but MAC accepts miliseconds */
    if (wrqu->retry.value < 1000) return -EINVAL;
    wrqu->retry.value = wrqu->retry.value/1000;
    obj_id = MIB_TX_MSDU_LIFETIME;
    break;
  default:
    WLOG("Unknown parameter type: 0x%04x", wrqu->retry.flags);
    res = -EINVAL;
    goto end;
  }

  if (wrqu->retry.value >= 0) {
    res = mtlk_set_mib_value_uint16(txmm, obj_id, wrqu->retry.value);
    if (res) res = -EFAULT;
  }

end:
  return res;
}

static int
mtlk_linux_ioctl_getretry (struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu,
                           char *extra)
{
  struct nic *nic = netdev_priv(dev);
  mtlk_txmm_t *txmm = nic->slow_ctx->hw_cfg.txmm;
  uint16 obj_id, value;
  int    scale = 1;
  int    res = 0;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (mtlk_scan_is_running(&nic->slow_ctx->scan)) {
    ILOG1(GID_IOCTL, "%s: Request eliminated due to running scan", dev->name);
    return -EAGAIN;
  }

  /* Determine requested parameter */
  switch (wrqu->retry.flags) {
  case 0:
  case IW_RETRY_LIFETIME:
  case IW_RETRY_LIFETIME | IW_RETRY_MIN:
  case IW_RETRY_LIFETIME | IW_RETRY_MAX:
    obj_id = MIB_TX_MSDU_LIFETIME;
    /* WT expects value in microseconds, but MAC work with miliseconds */
    scale = 1000;
    wrqu->retry.flags = IW_RETRY_LIFETIME;
    break;
  case IW_RETRY_LIMIT:
  case IW_RETRY_LIMIT | IW_RETRY_MIN:
    obj_id = MIB_SHORT_RETRY_LIMIT;
    wrqu->retry.flags = IW_RETRY_LIMIT | IW_RETRY_MIN;
    break;
  case IW_RETRY_LIMIT | IW_RETRY_MAX:
    obj_id = MIB_LONG_RETRY_LIMIT;
    wrqu->retry.flags = IW_RETRY_LIMIT | IW_RETRY_MAX;
    break;
  default:
    WLOG("Unknown parameter type: 0x%04x", wrqu->retry.flags);
    res = -EINVAL;
    goto end;
  }

  res = mtlk_get_mib_value_uint16(txmm, obj_id, &value);
  value = wrqu->retry.value;
  if (res == MTLK_ERR_OK) {
    wrqu->retry.value = scale*wrqu->retry.value;
    wrqu->retry.fixed = 1;
    wrqu->retry.disabled = 0;
  } else
    res = -EFAULT;

end:
  return res;
}

static int
mtlk_linux_ioctl_setenc (struct net_device *dev,
                         struct iw_request_info *info,
                         union  iwreq_data *wrqu,
                         char   *extra)
{
  struct nic *nic = netdev_priv(dev);
  mtlk_txmm_t *txmm = nic->slow_ctx->hw_cfg.txmm;
  int index, auth=-1;
  uint8 tx_key;
  char *s = NULL;
  int res = 0;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if ((mtlk_core_get_net_state(nic) & (NET_STATE_READY | NET_STATE_CONNECTED)) == 0) {
    ILOG1(GID_IOCTL, "Invalid card state - request rejected");
    res = -EAGAIN;
    goto end;
  }

  if ((wrqu->data.flags & IW_ENCODE_DISABLED) && nic->slow_ctx->wep_enabled) {
    if (ioctl_set_wep_mib(nic, 0) != MTLK_ERR_OK) {
      ELOG("Failed to disable WEP encryption");
      res = -EFAULT;
      goto end; /* res == -EFAULT */
    }
    ILOG2(GID_IOCTL, "WEP encryption disabled");
    if (mtlk_set_mib_value_uint8(txmm, MIB_AUTHENTICATION_PREFERENCE, 0)) {
      ELOG("Failed to switch access policy to 'Open system'");
      res = -EFAULT;
      goto end; /* res == -EFAULT */
    }
    ILOG2(GID_IOCTL, "Access policy switched to 'Open system'");
    goto end; /* res == 0 */
  }

  res = mtlk_get_mib_value_uint8(txmm, MIB_WEP_DEFAULT_KEYID, &tx_key);
  if (res != MTLK_ERR_OK) {
    ELOG("Unable to get WEP TX key index");
    res = -EFAULT;
    goto end; /* res == -EFAULT */
  }

  if ((wrqu->data.flags & IW_ENCODE_OPEN) &&
      (wrqu->data.flags & IW_ENCODE_RESTRICTED)) { /* auto mode */
    auth = 2;
    s = "Auto";
  } else if (wrqu->data.flags & IW_ENCODE_OPEN) {
    auth = 0;
    s = "Open";
  } else if (wrqu->data.flags & IW_ENCODE_RESTRICTED) {
    auth = 1;
    s = "Restricted";
  }
  if (auth >=0) {
    if (mtlk_set_mib_value_uint8(txmm, MIB_AUTHENTICATION_PREFERENCE, auth)) {
      ELOG("Failed to switch access policy to %s", s);
      res = -EFAULT;
      goto end; /* res == -EFAULT */
    }
    ILOG1(GID_IOCTL, "Access policy switched to %s", s);
  }

  /* Validate and adjust key index
   *
   * Up to 4 WEP keys supported.
   * WE enumerate WEP keys from 1 to N, and 0 - is current TX key.
   */
  index = (int)(wrqu->data.flags & IW_ENCODE_INDEX);
  if ((index > MIB_WEP_N_DEFAULT_KEYS) ||
      (index < 0)) {
    WLOG("Unsupported WEP key index");
    res = -EINVAL;
    goto end; /* res == -EINVAL */
  }
  if (index == 0) {
    wrqu->data.flags |= (tx_key + 1);
    index = tx_key;
  }
  else {
    index--;
  }

  if ((wrqu->data.length == 0) && (tx_key != index)) {
    /* If WEP key not given - TX key index may be changed to requested */
    res = mtlk_set_mib_value_uint8(txmm, MIB_WEP_DEFAULT_KEYID, index);
    if (res != MTLK_ERR_OK) {
      ELOG("Unable to set WEP TX key index");
      res = -EFAULT;
      goto end; /* res == -EFAULT */
    }
    nic->slow_ctx->default_wep_key = index;
    ILOG2(GID_IOCTL, "Set WEP TX key index to %i", index);
  } else if (wrqu->data.length){
    /* Set WEP key */
    MIB_WEP_DEF_KEYS wep_keys;
    /* Validate key first */
    if (mtlk_core_validate_wep_key(extra, wrqu->data.length) != MTLK_ERR_OK) {
      WLOG("Bad WEP key");
      res = -EINVAL;
      goto end; /* res = -EINVAL */
    }
    memcpy(&wep_keys, &nic->slow_ctx->wep_keys, sizeof(wep_keys));
    wep_keys.sKey[index].u8KeyLength = wrqu->data.length;
    memcpy(wep_keys.sKey[index].au8KeyData, extra, wrqu->data.length);
    res = mtlk_set_mib_value_raw(txmm, MIB_WEP_DEFAULT_KEYS, (MIB_VALUE*)&wep_keys);
    if (res == MTLK_ERR_OK) {
      nic->slow_ctx->wep_keys = wep_keys;
      DUMP2(extra, wrqu->data.length, "Successfully set WEP key #%i", index);
    } else {
      ELOG("Failed to set WEP key");
      res = -EFAULT;
      goto end; /* res == -EFAULT */
    }
  }

  if (!(wrqu->data.flags & IW_ENCODE_DISABLED) && !nic->slow_ctx->wep_enabled) {
    res = ioctl_set_wep_mib(nic, 1);
    if (res != MTLK_ERR_OK) {
      ELOG("Failed to enable WEP encryption (err=%d)", res);
      res = -EFAULT;
      goto end; /* res == -EFAULT */
    }
    ILOG2(GID_IOCTL, "WEP encryption enabled");
    res = 0;
  }
end:
  return res;
}

static int
mtlk_linux_ioctl_getenc (struct net_device *dev,
                         struct iw_request_info *info,
                         union  iwreq_data *wrqu,
                         char   *extra)
{
  struct nic *nic = netdev_priv(dev);
  mtlk_txmm_t *txmm = nic->slow_ctx->hw_cfg.txmm;
  int index;
  uint8 auth;
  int res = 0;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (mtlk_scan_is_running(&nic->slow_ctx->scan)) {
    ILOG1(GID_IOCTL, "%s: Request eliminated due to running scan", dev->name);
    return -EAGAIN;
  }

  if (!nic->slow_ctx->wep_enabled) {
    wrqu->data.length = 0;
    wrqu->data.flags  = IW_ENCODE_DISABLED;
    goto end; /* res == 0 */
  }

  /* Validate and adjust key index
   *
   * Up to 4 WEP keys supported.
   * WE enumerate WEP keys from 1 to N, and 0 - is current TX key.
   */
  index = (int)(wrqu->data.flags & IW_ENCODE_INDEX);
  if ((index > MIB_WEP_N_DEFAULT_KEYS) ||
      (index < 0)) {
    WLOG("Unsupported WEP key index");
    res = -EINVAL;
    goto end; /* res == -EINVAL */
  }
  if (index == 0) {
    uint8 tx_key;
    res = mtlk_get_mib_value_uint8(txmm, MIB_WEP_DEFAULT_KEYID, &tx_key);
    if (res != MTLK_ERR_OK) {
      ELOG("Unable to get WEP TX key index");
      res = -EFAULT;
      goto end; /* res == -EFAULT */
    }
    wrqu->data.flags |= (tx_key + 1);
    index = tx_key;
  }
  else {
    index--;
  }

  /* Report access policy */
  res = mtlk_get_mib_value_uint8(txmm, MIB_AUTHENTICATION_PREFERENCE, &auth);
  if (res != MTLK_ERR_OK) {
    ELOG("Failed to read WEP access policy");
    res = -EFAULT;
    goto end; /* res == -EFAULT */
  }
  if (auth == MIB_AUTHENTICATION_SHARED_KEY)
    wrqu->data.flags |= IW_ENCODE_RESTRICTED;
  else if (auth == MIB_AUTHENTICATION_OPEN_SYSTEM)
    wrqu->data.flags |= IW_ENCODE_OPEN;
  else if (auth == MIB_AUTHENTICATION_AUTO)
    wrqu->data.flags |= IW_ENCODE_OPEN|IW_ENCODE_RESTRICTED;

  /* Get requested key */
  wrqu->data.length = nic->slow_ctx->wep_keys.sKey[index].u8KeyLength;
  memcpy(extra, nic->slow_ctx->wep_keys.sKey[index].au8KeyData,
         wrqu->data.length);

end:
  return res;
}

static int
mtlk_ioctl_set_mac_addr (struct net_device *dev,
                         struct iw_request_info *info,
                         union  iwreq_data *wrqu,
                         char   *extra)
{
  struct nic *nic = netdev_priv(dev);
  struct sockaddr *addr = &wrqu->addr;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  /* Allow to set MAC address only if !IFF_UP */
  if (dev->flags & IFF_UP) {
    ILOG2(GID_IOCTL, "%s: Can't set MAC address with IFF_UP set", dev->name);
    return -EBUSY;
  }

  /* Validate address family */
  if ((addr->sa_family != ARPHRD_IEEE80211) && (addr->sa_family != ARPHRD_ETHER)) {
    ILOG2(GID_IOCTL, "%s: Can't set MAC address - invalid sa_family", dev->name);
    return -EINVAL;
  }

  /* Validate MAC address */
  if (!is_valid_ether_addr(wrqu->addr.sa_data)) {
    ILOG2(GID_IOCTL, "%s: Can't set MAC address - invalid sa_data", dev->name);
    return -EINVAL;
  }

  if (mtlk_core_send_mac_addr_tohw(nic, addr->sa_data)) {
    return -EFAULT;
  }

  ILOG1(GID_IOCTL, "%s: New HW MAC address: %Y", dev->name, addr->sa_data);

  return 0;
}

static int
mtlk_ioctl_get_mac_addr (struct net_device *dev,
                         struct iw_request_info *info,
                         union  iwreq_data *wrqu,
                         char   *extra)
{
  struct nic *nic = netdev_priv(dev);
  struct sockaddr *sa = &wrqu->addr;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  sa->sa_family = ARPHRD_IEEE80211;
  memcpy(sa->sa_data, nic->mac_addr, ETH_ALEN);

  return 0;
}

static int
mtlk_linux_ioctl_setnick (struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu,
                          char *extra)
{
  struct nic *nic = netdev_priv(dev);

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  /* TODO: add validation */
  strncpy(nic->slow_ctx->nickname, extra, IW_ESSID_MAX_SIZE);
  ILOG3(GID_IOCTL, "%s: Set NICKNAME to \"%s\"", dev->name, nic->slow_ctx->nickname);

  return 0;
}

static int
mtlk_linux_ioctl_getnick (struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu,
                          char *extra)
{
  struct nic *nic = netdev_priv(dev);

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  memcpy(extra, nic->slow_ctx->nickname, IW_ESSID_MAX_SIZE);
  wrqu->data.length = strnlen(nic->slow_ctx->nickname, IW_ESSID_MAX_SIZE);

  ILOG3(GID_IOCTL, "%s: NICKNAME \"%s\"", dev->name, nic->slow_ctx->nickname);
  return 0;
}

static int
mtlk_linux_ioctl_setessid (struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu,
                           char *extra)
{
  struct nic *nic = netdev_priv(dev);
  unsigned len = wrqu->essid.length;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  memset(nic->slow_ctx->essid, 0, sizeof(nic->slow_ctx->essid));

  /* In WE21 it is possible that ESSID will be non-NUL-terminated,
   * and length will not include termination.
   * see http://lwn.net/Articles/202838 for details. */
  if (len > 0 && extra[len-1] == '\0')
    len--;
  if (len > IW_ESSID_MAX_SIZE) {
    ELOG("%s: ESSID length (%i) is bigger then maximum expected (%i)",
        dev->name, len, IW_ESSID_MAX_SIZE);
    return -EINVAL;
  }
  // we can't use strncpy() because string might be not NULL-terminated
  memcpy(nic->slow_ctx->essid, extra, len);

  ILOG3(GID_IOCTL, "%s: Set ESSID to \"%s\"", dev->name, nic->slow_ctx->essid);

  return 0;
}

static int
mtlk_linux_ioctl_getessid (struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu,
                           char *extra)
{
  struct nic *nic = netdev_priv(dev);

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  memcpy(extra, nic->slow_ctx->essid, IW_ESSID_MAX_SIZE);
  wrqu->data.length = strnlen(extra, IW_ESSID_MAX_SIZE);
  wrqu->data.flags = 1;

  ILOG3(GID_IOCTL, "%s: ESSID \"%s\"", dev->name, nic->slow_ctx->essid);
  return 0;
}

/*!
        \fn     iw_handler_set_scan()
        \param  net_device Network device for scan to be performed on
        \param  extra Scan options (if any) starting from WE18
        \return Zero on success, negative error code on failure
        \brief  Handle 'start scan' request

        Handler for SIOCSIWSCAN - request for scan schedule. Process scan
        options (if any), and schedule scan. If scan already running - return
        zero, to avoid 'setscan-getscan' infinite loops in user applications.
        If scheduling succeed - return zero. If scan can't be started - return
        -EAGAIN
*/
static int
iw_handler_set_scan (struct net_device *dev,
                     struct iw_request_info *info,
                     union iwreq_data *wrqu,
                     char *extra)
{
  struct nic *nic = netdev_priv(dev);
  char *essid = NULL;
  int net_state;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);
  
  net_state = mtlk_core_get_net_state(nic);

  /* allow scanning in net states ready and connected only */
  if ((net_state & (NET_STATE_READY | NET_STATE_CONNECTED)) == 0)
    goto CANNOT_SCAN;
  if ((net_state == NET_STATE_CONNECTED) && !nic->slow_ctx->cfg.is_background_scan) {
    ILOG2(GID_IOCTL, "BG scan is off");
    goto CANNOT_SCAN;
  }

  if (   mtlk_scan_is_running(&nic->slow_ctx->scan)
      || mtlk_core_is_stopping(nic)) {
    /* Although we won't start scan when the previous hasn't completed yet anyway
     * (scan module prohibits this) -
     * we need to return 0 in this case to indicate the scan 
     * has been started successfully.
     * Otherwise wpa_supplicant will wait for scan completion for 3 seconds only 
     * (which is not enough for us in the majority of cases to finish scan) 
     * and iwlist will simply report error to the user.
     * If we return 0 - they start polling us for scan results, which is 
     * an expected behavior.
     */
    return 0;
  }

#if WIRELESS_EXT >= 18
  if (extra && wrqu->data.pointer) {
    struct iw_scan_req *scanopt = (struct iw_scan_req*)extra;
    if (wrqu->data.flags & IW_SCAN_THIS_ESSID) { // iwlist wlan0 scan <ESSID>
      ILOG2(GID_IOCTL, "Set ESSID pattern to \"%s\"", scanopt->essid);
      essid = scanopt->essid;
    }
  }
#endif

  if (net_state == NET_STATE_CONNECTED)
    mtlk_scan_set_background(&nic->slow_ctx->scan, TRUE);
  else
    mtlk_scan_set_background(&nic->slow_ctx->scan, FALSE);

  if (mtlk_scan_async(&nic->slow_ctx->scan, nic->slow_ctx->frequency_band_cfg, essid) != MTLK_ERR_OK)
    return -EAGAIN;

  return 0;
CANNOT_SCAN:
  ILOG2(GID_IOCTL, "Cannot start scan in %s", mtlk_net_state_to_string(net_state));
  if (nic->slow_ctx->hw_cfg.ap)
    return -EPERM; /* to make iwlist continue with scan results */
  return -EAGAIN;
}

/*! 
        \fn     iw_handler_get_scan()
        \return Zero and scan results on success, negative error code on failure
        \brief  Handle 'get scan results' request
    
        Handler for SIOCGIWSCAN - request for scan results. If scan running -
        return -EAGAIN and required buffer size. If not enough memory to store
        all scan results - return -E2BIG. On success return zero and cached
        scan results
*/  
static int
iw_handler_get_scan (struct net_device *dev,
                     struct iw_request_info *info,
                     union iwreq_data *wrqu,
                     char *extra)
{   
  struct nic *nic = netdev_priv(dev);
  struct iw_event iwe;
  char *stream, *border, *s;
  bss_data_t bss_found;
  size_t size;
  char   buf[32]; /* used for 3 RSSI string: "-xxx:-xxx:-xxx dBm" */

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);
    
  if (   mtlk_scan_is_running(&nic->slow_ctx->scan)
      || mtlk_core_is_stopping(nic)) {
    ILOG3(GID_IOCTL, "%s: Can't get scan results - scan in progress", dev->name);
    return -EAGAIN;
  } 
    
  memset(extra, 0, wrqu->data.length);
  stream = extra;
  border = extra + wrqu->data.length;
  size = 0;

  mtlk_cache_rewind(&nic->slow_ctx->cache);
  while (mtlk_cache_get_next_bss(&nic->slow_ctx->cache, &bss_found, NULL, NULL)) {
    iwe.cmd = SIOCGIWAP;
    memcpy(iwe.u.ap_addr.sa_data, bss_found.bssid, ETH_ALEN);
    iwe.u.ap_addr.sa_family = ARPHRD_IEEE80211;
    size += IW_EV_ADDR_LEN;
    stream = mtlk_iwe_stream_add_event(info, stream, border, &iwe, IW_EV_ADDR_LEN);
  
    iwe.cmd = SIOCGIWESSID;
    iwe.u.data.length = strnlen(bss_found.essid, IW_ESSID_MAX_SIZE);
    iwe.u.data.flags = 1;
    size += IW_EV_POINT_LEN + iwe.u.data.length;
    stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, bss_found.essid);

    iwe.cmd = SIOCGIWFREQ;
    iwe.u.freq.m = bss_found.channel;
    iwe.u.freq.e = 0;
    size += IW_EV_FREQ_LEN;
    stream = mtlk_iwe_stream_add_event(info, stream, border, &iwe, IW_EV_FREQ_LEN);

    if (RSN_IE_SIZE(&bss_found)) {
      iwe.cmd = IWEVGENIE;
      iwe.u.data.length = RSN_IE_SIZE(&bss_found) + sizeof(ie_t);
      size += IW_EV_POINT_LEN + iwe.u.data.length;
      stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, RSN_IE(&bss_found));
    }

    if (WPA_IE_SIZE(&bss_found)) {
      iwe.cmd = IWEVGENIE;
      iwe.u.data.length = WPA_IE_SIZE(&bss_found) + sizeof(ie_t);
      size += IW_EV_POINT_LEN + iwe.u.data.length;
      stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, WPA_IE(&bss_found));
    }

    if (WPS_IE_SIZE(&bss_found)) {
      iwe.cmd = IWEVGENIE;
      iwe.u.data.length = WPS_IE_SIZE(&bss_found) + sizeof(ie_t);
      size += IW_EV_POINT_LEN + iwe.u.data.length;
      stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, WPS_IE(&bss_found));
    }

    iwe.cmd = SIOCGIWENCODE;
    iwe.u.data.flags = mtlk_cache_is_wep_enabled(&bss_found)? IW_ENCODE_ENABLED:IW_ENCODE_DISABLED;
    iwe.u.data.length = 0;
    size += IW_EV_POINT_LEN;
    stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, NULL);

    iwe.cmd = IWEVQUAL;
    mtlk_init_iw_qual(&iwe.u.qual, bss_found.max_rssi, bss_found.noise);
    size += IW_EV_QUAL_LEN;
    stream = mtlk_iwe_stream_add_event(info, stream, border, &iwe, IW_EV_QUAL_LEN);

    iwe.cmd = IWEVCUSTOM;
    s = (bss_found.is_2_4)? "2.4 band":"5.2 band";
    iwe.u.data.length = strlen(s);
    size += IW_EV_POINT_LEN + iwe.u.data.length;
    stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, s);

    iwe.cmd = IWEVCUSTOM;
    s = (bss_found.is_ht)? "HT":"not HT";
    iwe.u.data.length = strlen(s);
    size += IW_EV_POINT_LEN + iwe.u.data.length;
    stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, s);

    iwe.cmd = IWEVCUSTOM;
    s = (bss_found.spectrum == SPECTRUM_40MHZ)? "40 MHz":"20 MHz";
    iwe.u.data.length = strlen(s);
    size += IW_EV_POINT_LEN + iwe.u.data.length;
    stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, s);

    iwe.cmd = IWEVCUSTOM;
    s = WPS_IE_FOUND(&bss_found)? "WPS":"not WPS";
    iwe.u.data.length = strlen(s);
    size += IW_EV_POINT_LEN + iwe.u.data.length;
    stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, s);

    iwe.cmd = IWEVCUSTOM;
    sprintf(buf, "%d:%d:%d dBm", 
            MTLK_NORMALIZED_RSSI(bss_found.all_rssi[0]),
            MTLK_NORMALIZED_RSSI(bss_found.all_rssi[1]),
            MTLK_NORMALIZED_RSSI(bss_found.all_rssi[2]));
    s = buf;
    iwe.u.data.length = strlen(s);
    size += IW_EV_POINT_LEN + iwe.u.data.length;
    stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, s);

    ILOG1(GID_IOCTL, "\"%-32s\" %Y %3i",
      bss_found.essid, bss_found.bssid, bss_found.channel);
  }

  wrqu->data.length = size;
  if (stream - extra < size) {
    ILOG1(GID_IOCTL, "%s: Can't get scan results - not enough memory", dev->name);
    return -E2BIG;
  } else {
    return 0;
  }
}

static int
iw_handler_get_connection_info (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra)
{
  struct nic *nic = netdev_priv(dev);
  char *s = extra;

  wrqu->data.length = 0;
  if (mtlk_core_get_net_state(nic) != NET_STATE_CONNECTED)
    return 0;
  /* in case we don't add anything to the string we relay on string length */
  *s = '\0';

  if (nic->slow_ctx->hw_cfg.ap) {
    sta_entry *sta = nic->slow_ctx->stadb.connected_stations;
    int i;
    for (i = 0; i < STA_MAX_STATIONS; i++, sta++) {
      if (sta->state == PEER_UNUSED) continue;

      /* In WPA security don't print STAs with which 4-way handshake
       * is not completed yet
       */
      if (nic->group_cipher && !sta->cipher) continue;

      s += sprintf(s, "\n" MAC_FMT, MAC_ARG(sta->mac));
      s += sprintf(s, " %10d", sta->stats.rx_packets);
      s += sprintf(s, " %10d", sta->stats.tx_packets);
    }
  } else {
    s += sprintf(s, "%sGHz,%d,%dMHz",
      nic->slow_ctx->frequency_band_cur == MTLK_HW_BAND_5_2_GHZ ? "5.2" : "2.4",
      nic->slow_ctx->channel, nic->slow_ctx->spectrum_mode == SPECTRUM_20MHZ ? 20 : 40);
    if (nic->slow_ctx->spectrum_mode == SPECTRUM_40MHZ)
      s += sprintf(s, nic->slow_ctx->bonding == ALTERNATE_LOWER ? ",Lower" : ",Upper");
  }

  wrqu->data.length = strlen(extra);
  return 0;
}

/*
 * Structures to export the Wireless Handlers
 */
static const iw_handler mtlk_linux_handler[] = {
  iw_handler_nop,                                            /* SIOCSIWCOMMIT */
  /* get name == wireless protocol */
  (iw_handler) mtlk_linux_ioctl_getname,  /* SIOCGIWNAME */

/* Basic operations */
  /* set network id (pre-802.11) */
  (iw_handler) NULL,                      /* SIOCSIWNWID */
  /* get network id (the cell) */
  (iw_handler) NULL,                      /* SIOCGIWNWID */
  /* set channel/frequency (Hz) */
  (iw_handler) mtlk_linux_ioctl_setfreq,  /* SIOCSIWFREQ */
  /* get channel/frequency (Hz) */
  (iw_handler) mtlk_linux_ioctl_getfreq,  /* SIOCGIWFREQ */
  /* set operation mode */
  iw_handler_nop,                                              /* SIOCSIWMODE */
  /* get operation mode */
  (iw_handler) mtlk_linux_ioctl_getmode,  /* SIOCGIWMODE */
  /* set sensitivity (dBm) */
  (iw_handler) NULL,                      /* SIOCSIWSENS */
  /* get sensitivity (dBm) */
  (iw_handler) NULL,                      /* SIOCGIWSENS */

/* Informative stuff */
  /* Unused */
  (iw_handler) NULL,                      /* SIOCSIWRANGE */
  /* Get range of parameters */
  (iw_handler) mtlk_linux_ioctl_getrange, /* SIOCGIWRANGE */
  /* Unused */
  (iw_handler) NULL,                      /* SIOCSIWPRIV */
  /* get private ioctl interface info */
  (iw_handler) NULL,                      /* SIOCGIWPRIV */
  /* Unused */
  (iw_handler) NULL,                      /* SIOCSIWSTATS */
  /* Get /proc/net/wireless stats */
  /* SIOCGIWSTATS is strictly used between user space and the kernel, and
   * is never passed to the driver (i.e. the driver will never see it). */
  (iw_handler) NULL,                      /* SIOCGIWSTATS */
  (iw_handler) NULL,                      /* SIOCSIWSPY */
  (iw_handler) NULL,                      /* SIOCGIWSPY */
  /* set spy threshold (spy event) */
  (iw_handler) NULL,                      /* SIOCSIWTHRSPY */
  /* get spy threshold */
  (iw_handler) NULL,                      /* SIOCGIWTHRSPY */

/* Access Point manipulation */
  /* set access point MAC addresses */
  (iw_handler) mtlk_linux_ioctl_setap,    /* SIOCSIWAP */
  /* get access point MAC addresses */
  (iw_handler) mtlk_linux_ioctl_getap,    /* SIOCGIWAP */
  /* request MLME operation; uses  struct iw_mlme */
  (iw_handler) mtlk_linux_ioctl_setmlme,  /* SIOCSIWMLME */
  (iw_handler) mtlk_linux_ioctl_getaplist,/* SIOCGIWAPLIST */

  iw_handler_set_scan,                                         /* SIOCSIWSCAN */
  iw_handler_get_scan,                                         /* SIOCGIWSCAN */

/* 802.11 specific support */
  /* set ESSID (network name) */
  (iw_handler) mtlk_linux_ioctl_setessid, /* SIOCSIWESSID */
  /* get ESSID */
  (iw_handler) mtlk_linux_ioctl_getessid, /* SIOCGIWESSID */
  /* set node name/nickname */
  (iw_handler) mtlk_linux_ioctl_setnick,  /* SIOCSIWNICKN */
  /* get node name/nickname */
  (iw_handler) mtlk_linux_ioctl_getnick,  /* SIOCGIWNICKN */

/* Other parameters useful in 802.11 and some other devices */
  (iw_handler) NULL,                      /* -- hole -- */
  (iw_handler) NULL,                      /* -- hole -- */
  (iw_handler) NULL,                      /* SIOCSIWRATE      */
  (iw_handler) NULL,                      /* SIOCGIWRATE      */
  (iw_handler) mtlk_linux_ioctl_setrtsthr, /* SIOCSIWRTS */
  (iw_handler) mtlk_linux_ioctl_getrtsthr, /* SIOCGIWRTS */
  /* set fragmentation thr (bytes) */
  (iw_handler) NULL,   // TBD?            /* SIOCSIWFRAG */
  /* get fragmentation thr (bytes) */
  (iw_handler) NULL,   // TBD?            /* SIOCGIWFRAG */
  /* set transmit power (dBm) */
  (iw_handler) mtlk_linux_ioctl_settxpower,/* SIOCSIWTXPOW */
  /* get transmit power (dBm) */
  (iw_handler) mtlk_linux_ioctl_gettxpower,/* SIOCGIWTXPOW */
  /* set retry limits and lifetime */
  (iw_handler) mtlk_linux_ioctl_setretry, /* SIOCSIWRETRY */
  /* get retry limits and lifetime */
  (iw_handler) mtlk_linux_ioctl_getretry, /* SIOCGIWRETRY */
  (iw_handler) mtlk_linux_ioctl_setenc,   /* SIOCSIWENCODE */
  (iw_handler) mtlk_linux_ioctl_getenc,   /* SIOCGIWENCODE */
/* Power saving stuff (power management, unicast and multicast) */
  /* set Power Management settings */
  (iw_handler) NULL,                      /* SIOCSIWPOWER */
  /* get Power Management settings */
  (iw_handler) NULL,                      /* SIOCGIWPOWER */
  /* Unused */
  (iw_handler) NULL,                      /* -- hole -- */
  /* Unused */
  (iw_handler) NULL,                      /* -- hole -- */

/* WPA : Generic IEEE 802.11 informatiom element (e.g., for WPA/RSN/WMM).
 * This ioctl uses struct iw_point and data buffer that includes IE id and len
 * fields. More than one IE may be included in the request. Setting the generic
 * IE to empty buffer (len=0) removes the generic IE from the driver. Drivers
 * are allowed to generate their own WPA/RSN IEs, but in these cases, drivers
 * are required to report the used IE as a wireless event, e.g., when
 * associating with an AP. */
  /* set generic IE */
  (iw_handler) mtlk_linux_ioctl_setgenie, /* SIOCSIWGENIE */
  /* get generic IE */
  (iw_handler) NULL,                      /* SIOCGIWGENIE */

/* WPA : Authentication mode parameters */
  (iw_handler) mtlk_linux_ioctl_setauth,  /* SIOCSIWAUTH */
  (iw_handler) mtlk_linux_ioctl_getauth,  /* SIOCGIWAUTH */

/* WPA : Extended version of encoding configuration */
  /* set encoding token & mode */
  (iw_handler) mtlk_linux_ioctl_setencext,/* SIOCSIWENCODEEXT */
   /* get encoding token & mode */
  (iw_handler) mtlk_linux_ioctl_getencext,/* SIOCGIWENCODEEXT */

/* WPA2 : PMKSA cache management */
  (iw_handler) NULL,                      /* SIOCSIWPMKSA */
};

enum {
  PRM_ID_FIRST = 0x7fff, /* Range 0x0000 - 0x7fff reserved for MIBs */
  PRM_ID_BRIDGE_MODE,
  PRM_ID_L2NAT_DEFAULT_HOST,
  PRM_ID_STADB_LOCAL_MAC,
  PRM_ID_L2NAT_AGING_TIMEOUT,
  PRM_ID_AOCS_WEIGHT_CL,
  PRM_ID_AOCS_WEIGHT_TX,
  PRM_ID_AOCS_WEIGHT_BSS,
  PRM_ID_AOCS_WEIGHT_SM,
  PRM_ID_AOCS_CFM_RANK_SW_THRESHOLD,
  PRM_ID_AOCS_SCAN_AGING,
  PRM_ID_AOCS_CONFIRM_RANK_AGING,
  PRM_ID_AOCS_EN_PENALTIES,
  PRM_ID_AOCS_PENALTIES,
  PRM_ID_AOCS_RESTRICTED_CHANNELS,
  PRM_ID_AOCS_AFILTER,
  PRM_ID_AOCS_BONDING,
  PRM_ID_AOCS_MSDU_THRESHOLD,
  PRM_ID_AOCS_WIN_TIME,
  PRM_ID_AOCS_MSDU_PER_WIN_THRESHOLD,
  PRM_ID_AOCS_LOWER_THRESHOLD,
  PRM_ID_AOCS_THRESHOLD_WINDOW,
  PRM_ID_AOCS_MSDU_DEBUG_ENABLED,
  PRM_ID_AOCS_IS_ENABLED,
  PRM_ID_AOCS_MSDU_TX_AC,
  PRM_ID_AOCS_MSDU_RX_AC,
#ifdef AOCS_DEBUG_40MHZ_INT
  PRM_ID_AOCS_DEBUG_40MHZ_INT,
#endif
  PRM_ID_AOCS_MEASUREMENT_WINDOW,
  PRM_ID_AOCS_THROUGHPUT_THRESHOLD,
  PRM_ID_PACK_SCHED_ENABLED,
  PRM_ID_SQ_LIMITS,
  PRM_ID_SQ_PEER_LIMITS,
  PRM_ID_ACTIVE_SCAN_SSID,
  PRM_ID_SCAN_CACHE_LIFETIME,
  PRM_DBG_SW_WD_ENABLE,
  PRM_ID_STA_KEEPALIVE_TIMEOUT,
  PRM_ID_STA_KEEPALIVE_INTERVAL,
  PRM_ID_WDS_HOST_TIMEOUT,
  PRM_ID_RELIABLE_MULTICAST,
  PRM_ID_AP_FORWARDING,
  PRM_ID_SPECTRUM_MODE,
  PRM_ID_POWER_SELECTION,
  PRM_ID_11D_RESTORE_DEFAULTS,
  PRM_ID_11D,
  PRM_ID_RADAR_DETECTION,
  PRM_ID_11H_ENABLE_SM_CHANNELS,
  PRM_ID_11H_BEACON_COUNT,
  PRM_ID_11H_CHANNEL_AVAILABILITY_CHECK_TIME,
  PRM_ID_11H_EMULATE_RADAR_DETECTION,
  PRM_ID_11H_SWITCH_CHANNEL,
  PRM_ID_11H_NEXT_CHANNEL,
  PRM_ID_11H_STATUS,
  PRM_ID_EEPROM,
  PRM_ID_BE_BAUSE,
  PRM_ID_BK_BAUSE,
  PRM_ID_VI_BAUSE,
  PRM_ID_VO_BAUSE,
  PRM_ID_BE_BAACCEPT,
  PRM_ID_BK_BAACCEPT,
  PRM_ID_VI_BAACCEPT,
  PRM_ID_VO_BAACCEPT,
  PRM_ID_BE_BATIMEOUT,
  PRM_ID_BK_BATIMEOUT,
  PRM_ID_VI_BATIMEOUT,
  PRM_ID_VO_BATIMEOUT,
  PRM_ID_BE_BAWINSIZE,
  PRM_ID_BK_BAWINSIZE,
  PRM_ID_VI_BAWINSIZE,
  PRM_ID_VO_BAWINSIZE,
  PRM_ID_BE_AGGRMAXBTS,
  PRM_ID_BK_AGGRMAXBTS,
  PRM_ID_VI_AGGRMAXBTS,
  PRM_ID_VO_AGGRMAXBTS,
  PRM_ID_BE_AGGRMAXPKTS,
  PRM_ID_BK_AGGRMAXPKTS,
  PRM_ID_VI_AGGRMAXPKTS,
  PRM_ID_VO_AGGRMAXPKTS,
  PRM_ID_BE_AGGRMINPTSZ,
  PRM_ID_BK_AGGRMINPTSZ,
  PRM_ID_VI_AGGRMINPTSZ,
  PRM_ID_VO_AGGRMINPTSZ,
  PRM_ID_BE_AGGRTIMEOUT,
  PRM_ID_BK_AGGRTIMEOUT,
  PRM_ID_VI_AGGRTIMEOUT,
  PRM_ID_VO_AGGRTIMEOUT,
  PRM_ID_BE_AIFSN,
  PRM_ID_BK_AIFSN,
  PRM_ID_VI_AIFSN,
  PRM_ID_VO_AIFSN,
  PRM_ID_BE_AIFSNAP,
  PRM_ID_BK_AIFSNAP,
  PRM_ID_VI_AIFSNAP,
  PRM_ID_VO_AIFSNAP,
  PRM_ID_BE_CWMAX,
  PRM_ID_BK_CWMAX,
  PRM_ID_VI_CWMAX,
  PRM_ID_VO_CWMAX,
  PRM_ID_BE_CWMAXAP,
  PRM_ID_BK_CWMAXAP,
  PRM_ID_VI_CWMAXAP,
  PRM_ID_VO_CWMAXAP,
  PRM_ID_BE_CWMIN,
  PRM_ID_BK_CWMIN,
  PRM_ID_VI_CWMIN,
  PRM_ID_VO_CWMIN,
  PRM_ID_BE_CWMINAP,
  PRM_ID_BK_CWMINAP,
  PRM_ID_VI_CWMINAP,
  PRM_ID_VO_CWMINAP,
  PRM_ID_BE_TXOP,
  PRM_ID_BK_TXOP,
  PRM_ID_VI_TXOP,
  PRM_ID_VO_TXOP,
  PRM_ID_BE_TXOPAP,
  PRM_ID_BK_TXOPAP,
  PRM_ID_VI_TXOPAP,
  PRM_ID_VO_TXOPAP,
  PRM_ID_HW_LIMIT,
  PRM_ID_ANT_GAIN,
  PRM_ID_AOCS_NON_OCCUPANCY_PERIOD,
  PRM_ID_USE_8021Q,
  PRM_ID_ACL,
  PRM_ID_ACL_DEL,
  PRM_ID_ACL_RANGE,
  PRM_ID_LEGACY_FORCE_RATE,
  PRM_ID_HT_FORCE_RATE,
  PRM_ID_CORE_COUNTRIES_SUPPORTED,
  PRM_ID_NETWORK_MODE,
  PRM_ID_IS_BACKGROUND_SCAN,
  PRM_ID_BG_SCAN_CH_LIMIT,
  PRM_ID_BG_SCAN_PAUSE,
  PRM_ID_MAC_WATCHDOG_TIMEOUT_MS,
  PRM_ID_MAC_WATCHDOG_PERIOD_MS,
#ifdef MTCFG_IRB_DEBUG
  PRM_ID_IRB_PINGER_ENABLED,
  PRM_ID_IRB_PINGER_STATS,
#endif
#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
  PRM_ID_PPA_API_DIRECTPATH,
#endif
  PRM_ID_COC_LOW_POWER_MODE,
  PRM_CHANGE_TX_POWER_LIMIT,
  PRM_ID_ADD_PEERAP,
  PRM_ID_DEL_PEERAP,
  PRM_ID_PEERAP_KEY_IDX,
  PRM_ID_AGGR_OPEN_THRESHOLD,
  PRM_ID_IS_HALT_FW_ON_DISC_TIMEOUT
};

static int
mtlk_ioctl_set_int (struct net_device *dev,
               struct iw_request_info *info,
               union iwreq_data *wrqu,
               char *extra)
{
  struct nic *nic = netdev_priv(dev);
  mtlk_txmm_t *txmm = nic->slow_ctx->hw_cfg.txmm;
  uint32 subcmd;
  uint8 net_mode;
  uint32 v;
  int res = 0;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  subcmd = wrqu->mode; extra += sizeof(uint32);
  v = *(uint32*)extra;

  ILOG2(GID_IOCTL, "subcmd = 0x%04x", subcmd);

  switch (subcmd) {
  case PRM_ID_BE_BAUSE:
    nic->slow_ctx->cfg.addba.tid[0].use_aggr = !!v;
    break;
  case PRM_ID_BK_BAUSE:
    nic->slow_ctx->cfg.addba.tid[1].use_aggr = !!v;
    break;
  case PRM_ID_VI_BAUSE:
    nic->slow_ctx->cfg.addba.tid[5].use_aggr = !!v;
    break;
  case PRM_ID_VO_BAUSE:
    nic->slow_ctx->cfg.addba.tid[6].use_aggr = !!v;
    break;
  case PRM_ID_BE_BAACCEPT:
    nic->slow_ctx->cfg.addba.tid[0].accept_aggr = !!v;
    nic->slow_ctx->cfg.addba.tid[3].accept_aggr = !!v;
    break;
  case PRM_ID_BK_BAACCEPT:
    nic->slow_ctx->cfg.addba.tid[1].accept_aggr = !!v;
    nic->slow_ctx->cfg.addba.tid[2].accept_aggr = !!v;
    break;
  case PRM_ID_VI_BAACCEPT:
    nic->slow_ctx->cfg.addba.tid[5].accept_aggr = !!v;
    nic->slow_ctx->cfg.addba.tid[4].accept_aggr = !!v;
    break;
  case PRM_ID_VO_BAACCEPT:
    nic->slow_ctx->cfg.addba.tid[6].accept_aggr = !!v;
    nic->slow_ctx->cfg.addba.tid[7].accept_aggr = !!v;
    break;
  case PRM_ID_BE_BATIMEOUT:
    nic->slow_ctx->cfg.addba.tid[0].addba_timeout = v;
    break;
  case PRM_ID_BK_BATIMEOUT:
    nic->slow_ctx->cfg.addba.tid[1].addba_timeout = v;
    break;
  case PRM_ID_VI_BATIMEOUT:
    nic->slow_ctx->cfg.addba.tid[5].addba_timeout = v;
    break;
  case PRM_ID_VO_BATIMEOUT:
    nic->slow_ctx->cfg.addba.tid[6].addba_timeout = v;
    break;
  case PRM_ID_BE_BAWINSIZE:
    nic->slow_ctx->cfg.addba.tid[0].aggr_win_size = v;
    break;
  case PRM_ID_BK_BAWINSIZE:
    nic->slow_ctx->cfg.addba.tid[1].aggr_win_size = v;
    break;
  case PRM_ID_VI_BAWINSIZE:
    nic->slow_ctx->cfg.addba.tid[5].aggr_win_size = v;
    break;
  case PRM_ID_VO_BAWINSIZE:
    nic->slow_ctx->cfg.addba.tid[6].aggr_win_size = v;
    break;
  case PRM_ID_BE_AGGRMAXBTS:
    nic->slow_ctx->cfg.addba.tid[0].max_nof_bytes = v;
    break;
  case PRM_ID_BK_AGGRMAXBTS:
    nic->slow_ctx->cfg.addba.tid[1].max_nof_bytes = v;
    break;
  case PRM_ID_VI_AGGRMAXBTS:
    nic->slow_ctx->cfg.addba.tid[5].max_nof_bytes = v;
    break;
  case PRM_ID_VO_AGGRMAXBTS:
    nic->slow_ctx->cfg.addba.tid[6].max_nof_bytes = v;
    break;
  case PRM_ID_BE_AGGRMAXPKTS:
    nic->slow_ctx->cfg.addba.tid[0].max_nof_packets = v;
    break;
  case PRM_ID_BK_AGGRMAXPKTS:
    nic->slow_ctx->cfg.addba.tid[1].max_nof_packets = v;
    break;
  case PRM_ID_VI_AGGRMAXPKTS:
    nic->slow_ctx->cfg.addba.tid[5].max_nof_packets = v;
    break;
  case PRM_ID_VO_AGGRMAXPKTS:
    nic->slow_ctx->cfg.addba.tid[6].max_nof_packets = v;
    break;
  case PRM_ID_BE_AGGRMINPTSZ:
    nic->slow_ctx->cfg.addba.tid[0].min_packet_size_in_aggr = v;
    break;
  case PRM_ID_BK_AGGRMINPTSZ:
    nic->slow_ctx->cfg.addba.tid[1].min_packet_size_in_aggr = v;
    break;
  case PRM_ID_VI_AGGRMINPTSZ:
    nic->slow_ctx->cfg.addba.tid[5].min_packet_size_in_aggr = v;
    break;
  case PRM_ID_VO_AGGRMINPTSZ:
    nic->slow_ctx->cfg.addba.tid[6].min_packet_size_in_aggr = v;
    break;
  case PRM_ID_BE_AGGRTIMEOUT:
    nic->slow_ctx->cfg.addba.tid[0].timeout_interval = v;
    break;
  case PRM_ID_BK_AGGRTIMEOUT:
    nic->slow_ctx->cfg.addba.tid[1].timeout_interval = v;
    break;
  case PRM_ID_VI_AGGRTIMEOUT:
    nic->slow_ctx->cfg.addba.tid[5].timeout_interval = v;
    break;
  case PRM_ID_VO_AGGRTIMEOUT:
    nic->slow_ctx->cfg.addba.tid[6].timeout_interval = v;
    break;
  case PRM_ID_BE_AIFSN:
    nic->slow_ctx->cfg.wme_bss.wme_class[0].aifsn = v;
    break;
  case PRM_ID_BK_AIFSN:
    nic->slow_ctx->cfg.wme_bss.wme_class[1].aifsn = v;
    break;
  case PRM_ID_VI_AIFSN:
    nic->slow_ctx->cfg.wme_bss.wme_class[2].aifsn = v;
    break;
  case PRM_ID_VO_AIFSN:
    nic->slow_ctx->cfg.wme_bss.wme_class[3].aifsn = v;
    break;
  case PRM_ID_BE_AIFSNAP:
    nic->slow_ctx->cfg.wme_ap.wme_class[0].aifsn = v;
    break;
  case PRM_ID_BK_AIFSNAP:
    nic->slow_ctx->cfg.wme_ap.wme_class[1].aifsn = v;
    break;
  case PRM_ID_VI_AIFSNAP:
    nic->slow_ctx->cfg.wme_ap.wme_class[2].aifsn = v;
    break;
  case PRM_ID_VO_AIFSNAP:
    nic->slow_ctx->cfg.wme_ap.wme_class[3].aifsn = v;
    break;
  case PRM_ID_BE_CWMAX:
    nic->slow_ctx->cfg.wme_bss.wme_class[0].cwmax = v;
    break;
  case PRM_ID_BK_CWMAX:
    nic->slow_ctx->cfg.wme_bss.wme_class[1].cwmax = v;
    break;
  case PRM_ID_VI_CWMAX:
    nic->slow_ctx->cfg.wme_bss.wme_class[2].cwmax = v;
    break;
  case PRM_ID_VO_CWMAX:
    nic->slow_ctx->cfg.wme_bss.wme_class[3].cwmax = v;
    break;
  case PRM_ID_BE_CWMAXAP:
    nic->slow_ctx->cfg.wme_ap.wme_class[0].cwmax = v;
    break;
  case PRM_ID_BK_CWMAXAP:
    nic->slow_ctx->cfg.wme_ap.wme_class[1].cwmax = v;
    break;
  case PRM_ID_VI_CWMAXAP:
    nic->slow_ctx->cfg.wme_ap.wme_class[2].cwmax = v;
    break;
  case PRM_ID_VO_CWMAXAP:
    nic->slow_ctx->cfg.wme_ap.wme_class[3].cwmax = v;
    break;
  case PRM_ID_BE_CWMIN:
    nic->slow_ctx->cfg.wme_bss.wme_class[0].cwmin = v;
    break;
  case PRM_ID_BK_CWMIN:
    nic->slow_ctx->cfg.wme_bss.wme_class[1].cwmin = v;
    break;
  case PRM_ID_VI_CWMIN:
    nic->slow_ctx->cfg.wme_bss.wme_class[2].cwmin = v;
    break;
  case PRM_ID_VO_CWMIN:
    nic->slow_ctx->cfg.wme_bss.wme_class[3].cwmin = v;
    break;
  case PRM_ID_BE_CWMINAP:
    nic->slow_ctx->cfg.wme_ap.wme_class[0].cwmin = v;
    break;
  case PRM_ID_BK_CWMINAP:
    nic->slow_ctx->cfg.wme_ap.wme_class[1].cwmin = v;
    break;
  case PRM_ID_VI_CWMINAP:
    nic->slow_ctx->cfg.wme_ap.wme_class[2].cwmin = v;
    break;
  case PRM_ID_VO_CWMINAP:
    nic->slow_ctx->cfg.wme_ap.wme_class[3].cwmin = v;
    break;
  case PRM_ID_BE_TXOP:
    nic->slow_ctx->cfg.wme_bss.wme_class[0].txop = v;
    break;
  case PRM_ID_BK_TXOP:
    nic->slow_ctx->cfg.wme_bss.wme_class[1].txop = v;
    break;
  case PRM_ID_VI_TXOP:
    nic->slow_ctx->cfg.wme_bss.wme_class[2].txop = v;
    break;
  case PRM_ID_VO_TXOP:
    nic->slow_ctx->cfg.wme_bss.wme_class[3].txop = v;
    break;
  case PRM_ID_BE_TXOPAP:
    nic->slow_ctx->cfg.wme_ap.wme_class[0].txop = v;
    break;
  case PRM_ID_BK_TXOPAP:
    nic->slow_ctx->cfg.wme_ap.wme_class[1].txop = v;
    break;
  case PRM_ID_VI_TXOPAP:
    nic->slow_ctx->cfg.wme_ap.wme_class[2].txop = v;
    break;
  case PRM_ID_VO_TXOPAP:
    nic->slow_ctx->cfg.wme_ap.wme_class[3].txop = v;
    break;
  case PRM_ID_11H_EMULATE_RADAR_DETECTION:
    res = mtlk_dot11h_debug_event(dev, MTLK_DFS_EVENT_RADAR_DETECTED, v);
    break;
  case PRM_ID_11H_SWITCH_CHANNEL:
    res = mtlk_dot11h_debug_event(dev, MTLK_DFS_EVENT_CHANGE_CHANNEL_NORMAL, (v < 0)? 0: v);
    break;
  case PRM_ID_11H_NEXT_CHANNEL:
    res = mtlk_dot11h_set_next_channel(dev, v);
    break;
  case PRM_ID_11H_BEACON_COUNT:
    nic->slow_ctx->cfg.dot11h_debug_params.debugChannelSwitchCount = v;
    break;
  case PRM_ID_11H_CHANNEL_AVAILABILITY_CHECK_TIME:
    nic->slow_ctx->cfg.dot11h_debug_params.debugChannelAvailabilityCheckTime = v;
    break;
  case MIB_ACL_MODE:
    if (!nic->ap) {
      res = -EOPNOTSUPP;
      ELOG("%s: The command is supported only in AP mode", dev->name);
      break;
    }
    if (v > 2) {
      res = -EINVAL;
      ELOG("%s: usage: 0 - OFF, 1 - white list, 2 - black list", dev->name);
      break;
    }
    mtlk_set_dec_mib_value(PRM_ACL_MODE, v, nic);
    {
      MIB_VALUE m;
      memset(&m, 0, sizeof(m));
      m.sAclMode.u8ACLMode = v;
      res = mtlk_set_mib_value_raw(nic->slow_ctx->hw_cfg.txmm, MIB_ACL_MODE, &m);
      if (res != MTLK_ERR_OK) {
        ELOG("%s: Updating MIB_ACL_MODE failed - %d", dev->name, res);
        res = -EFAULT;
      } else {
        res = 0;
      }
    }
    break;
  case MIB_ACL_MAX_CONNECTIONS:
    mtlk_set_dec_mib_value(PRM_ACL_MAX_CONNECTIONS, v, nic);
    break;
  case PRM_ID_RADAR_DETECTION:
    nic->slow_ctx->dot11h.cfg._11h_radar_detect = v;
    break;
  case PRM_ID_11H_ENABLE_SM_CHANNELS:
    if (v)
      mtlk_aocs_enable_smrequired(&nic->slow_ctx->aocs);
    else
      mtlk_aocs_disable_smrequired(&nic->slow_ctx->aocs);
    break;
  case PRM_ID_11D:
    if (mtlk_core_get_net_state(nic) != NET_STATE_READY)
      return -EBUSY;
    if (!nic->ap) {
      // Switched on
      if (v && !nic->slow_ctx->cfg.dot11d)
        nic->slow_ctx->cfg.country_code = 0;
      // Switched off
      else if(!v && nic->slow_ctx->cfg.dot11d) {
        nic->slow_ctx->cfg.country_code = mtlk_eeprom_get_country_code(&nic->slow_ctx->ee_data);
        if (nic->slow_ctx->cfg.country_code == 0)
          nic->slow_ctx->cfg.country_code = country_to_country_code("US");
      }
    }
    nic->slow_ctx->cfg.dot11d = !!v;
    break;
  case PRM_ID_11D_RESTORE_DEFAULTS:
    mtlk_reset_tx_limit_tables(&nic->slow_ctx->tx_limits);
    break;
  case PRM_ID_POWER_SELECTION:
    nic->slow_ctx->power_selection = v;
    break;
  case PRM_ID_SPECTRUM_MODE:
    {
      /* for 20, 40, auto; for STA: 20, 40 */
      if (v != SPECTRUM_20MHZ && v != SPECTRUM_40MHZ && v != SPECTRUM_AUTO) {
        ELOG("Invalid value");
        return -EOPNOTSUPP;
      }

      if (nic->ap)
      {
        BOOL is_auto_spectrum;

        if (!is_ht_net_mode(nic->slow_ctx->net_mode_cfg) && (v != SPECTRUM_20MHZ))
          return -EINVAL;

        is_auto_spectrum = mtlk_aocs_set_auto_spectrum(&nic->slow_ctx->aocs, v);
        if (is_auto_spectrum)
          v = SPECTRUM_40MHZ;

        mtlk_set_dec_mib_value(PRM_SPECTRUM_MODE, v, nic);
        nic->slow_ctx->spectrum_mode = v;
      } else {
        nic->slow_ctx->sta_force_spectrum_mode = v;
      }
    }
    break;
  case MIB_CALIBRATION_ALGO_MASK:
    mtlk_set_dec_mib_value(PRM_ALGO_CALIBR_MASK, v, nic);
    break;
  case MIB_POWER_INCREASE_VS_DUTY_CYCLE:
    mtlk_set_dec_mib_value(PRM_POWER_INCREASE_VS_DUTY_CYCLE, v, nic);
    break;
  case MIB_USE_SHORT_CYCLIC_PREFIX:
    mtlk_set_dec_mib_value(PRM_SHORT_CYCLIC_PREFIX, v, nic);
    break;
  case MIB_SHORT_PREAMBLE_OPTION_IMPLEMENTED:
    mtlk_set_dec_mib_value(PRM_SHORT_PREAMBLE, v, nic);
    break;
  case MIB_SHORT_SLOT_TIME_OPTION_ENABLED_11G:
    mtlk_set_dec_mib_value(PRM_SHORT_SLOT_TIME_OPTION_ENABLED, v, nic);
    break;
  case PRM_ID_WDS_HOST_TIMEOUT:
    nic->slow_ctx->stadb.wds_host_timeout = v;
    break;
  case PRM_ID_STA_KEEPALIVE_TIMEOUT:
    nic->slow_ctx->stadb.sta_keepalive_timeout = v;
    break;
  case PRM_ID_STA_KEEPALIVE_INTERVAL:
    nic->slow_ctx->stadb.keepalive_interval = v;
    break;
  case MIB_SHORT_RETRY_LIMIT:
  case MIB_LONG_RETRY_LIMIT:
  case MIB_TX_MSDU_LIFETIME:
  case MIB_CURRENT_TX_ANTENNA:
  case MIB_BEACON_PERIOD:
  case MIB_DISCONNECT_ON_NACKS_WEIGHT:
    mtlk_set_mib_value_uint16(txmm, subcmd, v);
    break;
  case MIB_SM_ENABLE:
    mtlk_set_dec_mib_value(PRM_CHANNEL_ANNOUNCEMENT_ENABLED, v, nic);
    mtlk_set_mib_value_uint8(txmm, subcmd, v);
    break;
  case MIB_HIDDEN_SSID:
  case MIB_ADVANCED_CODING_SUPPORTED:
  case MIB_OVERLAPPING_PROTECTION_ENABLE:
  case MIB_OFDM_PROTECTION_METHOD:
  case MIB_HT_PROTECTION_METHOD:
  case MIB_DTIM_PERIOD:
  case MIB_RECEIVE_AMPDU_MAX_LENGTH:
  case MIB_CB_DATABINS_PER_SYMBOL:
  case MIB_USE_LONG_PREAMBLE_FOR_MULTICAST:
  case MIB_USE_SPACE_TIME_BLOCK_CODE:
  case MIB_ONLINE_CALIBRATION_ALGO_MASK:
  case MIB_DISCONNECT_ON_NACKS_ENABLE:
    mtlk_set_mib_value_uint8(txmm, subcmd, v);
    break;
  case PRM_ID_BRIDGE_MODE:
    /* allow bridge mode change only if not connected */
    if (nic->net_state == NET_STATE_CONNECTED) {
      ELOG("Cannot change bridge mode to (%d) while connected.", v);
      res = -EINVAL;
    }
    /* check for only allowed values */
    if (v < BR_MODE_NONE || v > BR_MODE_LAST) {
      ELOG("Unsupported bridge mode value: %d.", v);
      res = -EINVAL;
    }
    /* on AP only NONE and WDS allowed */
    if (nic->slow_ctx->hw_cfg.ap && v != BR_MODE_NONE && v != BR_MODE_WDS) {
      ELOG("Unsupported (on AP) bridge mode value: %d.", v);
      res = -EINVAL;
    }
    if (res == 0)
      mtlk_set_dec_mib_value(PRM_BRIDGE_MODE, v, nic);
    break;
  case PRM_ID_L2NAT_AGING_TIMEOUT:
    if (nic->slow_ctx->hw_cfg.ap) {
      ILOG0(GID_IOCTL, "Aging timeout setting is useless, since L2NAT is not supported in AP mode.");
      res =  -EINVAL;
    }
    if (v < 0) {
      ELOG("Wrong value for aging timeout: %d", v);
      res =  -EINVAL;
    }
    if (v > 0 && v < 60) {
      ELOG("The lowest timeout allowed is 60 seconds.");
      res =  -EINVAL;
    }
    if (res == 0)
      mtlk_set_dec_mib_value(PRM_L2NAT_AGING_TIMEOUT, v, nic);
    break;
  case PRM_ID_AOCS_WEIGHT_CL:
    if (mtlk_aocs_set_weight(&nic->slow_ctx->aocs, AOCS_WEIGHT_IDX_CL, v))
      res = -EINVAL;
    break;
  case PRM_ID_AOCS_WEIGHT_TX:
    if (mtlk_aocs_set_weight(&nic->slow_ctx->aocs, AOCS_WEIGHT_IDX_TX, v))
      res = -EINVAL;
    break;
  case PRM_ID_AOCS_WEIGHT_BSS:
    if (mtlk_aocs_set_weight(&nic->slow_ctx->aocs, AOCS_WEIGHT_IDX_BSS, v))
      res = -EINVAL;
    break;
  case PRM_ID_AOCS_WEIGHT_SM:
    if (mtlk_aocs_set_weight(&nic->slow_ctx->aocs, AOCS_WEIGHT_IDX_SM, v))
      res = -EINVAL;
    break;
  case PRM_ID_AOCS_CFM_RANK_SW_THRESHOLD:
    if (mtlk_aocs_set_cfm_rank_sw_threshold(&nic->slow_ctx->aocs, v))
      res = -EINVAL;
    break;
  case PRM_ID_AOCS_SCAN_AGING:
    if (mtlk_aocs_set_scan_aging(&nic->slow_ctx->aocs, v))
      res = -EINVAL;
    break;
  case PRM_ID_AOCS_CONFIRM_RANK_AGING:
    if (mtlk_aocs_set_confirm_rank_aging(&nic->slow_ctx->aocs, v))
        res = -EINVAL;
    break;
  case PRM_ID_AOCS_AFILTER:
    if (mtlk_aocs_set_afilter(&nic->slow_ctx->aocs, v))
      res = -EINVAL;
    break;
  case PRM_ID_AOCS_BONDING:
    if (MTLK_ERR_OK != mtlk_core_set_bonding(nic, v)) {
      res = -EINVAL;
    }
    break;
  case PRM_ID_AOCS_EN_PENALTIES:
    if (mtlk_aocs_set_penalty_enabled(&nic->slow_ctx->aocs, v))
      res = -EINVAL;
    break;
  case PRM_ID_AOCS_WIN_TIME:
    if (mtlk_aocs_set_win_time(&nic->slow_ctx->aocs, v))
      res = -EINVAL;
    break;
  case PRM_ID_AOCS_LOWER_THRESHOLD:
      if (mtlk_aocs_set_lower_threshold(&nic->slow_ctx->aocs, v))
          res = -EINVAL;
      break;
  case PRM_ID_AOCS_THRESHOLD_WINDOW:
      if (mtlk_aocs_set_threshold_window(&nic->slow_ctx->aocs, v))
          res = -EINVAL;
      break;
  case PRM_ID_AOCS_MSDU_DEBUG_ENABLED:
    if (mtlk_aocs_set_msdu_debug_enabled(&nic->slow_ctx->aocs, v))
      res = -EINVAL;
    break;
  case PRM_ID_AOCS_IS_ENABLED:
    if (mtlk_aocs_set_type(&nic->slow_ctx->aocs, v))
      res = -EINVAL;
    break;
  case PRM_ID_AOCS_MSDU_PER_WIN_THRESHOLD:
    if (mtlk_aocs_set_msdu_win_thr(&nic->slow_ctx->aocs, v))
        res = -EINVAL;
    break;
  case PRM_ID_AOCS_MSDU_THRESHOLD:
    if (mtlk_aocs_set_msdu_threshold(&nic->slow_ctx->aocs, v))
      res = -EINVAL;
    break;
  case PRM_ID_AOCS_MEASUREMENT_WINDOW:
    if (mtlk_aocs_set_measurement_window(&nic->slow_ctx->aocs, v) != MTLK_ERR_OK) {
      res = -EINVAL;
    }
    break;
  case PRM_ID_AOCS_THROUGHPUT_THRESHOLD:
    if (mtlk_aocs_set_troughput_threshold(&nic->slow_ctx->aocs, v) != MTLK_ERR_OK) {
      res = -EINVAL;
    }
    break;
  case PRM_ID_PACK_SCHED_ENABLED:
    mtlk_set_dec_mib_value(PRM_ENABLE_PACK_SCHED, v, nic);
    break;
  case PRM_DBG_SW_WD_ENABLE:
    mtlk_core_sw_reset_enable_set(v, nic);
    mtlk_set_mib_value_uint8(txmm, MIB_SWRESET_ON_ASSERT, v);
    break;
  case PRM_ID_SCAN_CACHE_LIFETIME:
    nic->slow_ctx->cache.cache_expire = v;
    break;
  case PRM_ID_RELIABLE_MULTICAST:
    nic->reliable_mcast = !!v;
    break;
  case PRM_ID_AP_FORWARDING:
    mtlk_set_dec_mib_value(PRM_AP_FORWARDING, v, nic);
    break;
  case MIB_BSS_BASIC_RATE_SET:
    if (nic->ap == 0)
      return -EOPNOTSUPP;
    if (mtlk_core_get_net_state(nic) != NET_STATE_READY)
      return -EAGAIN;
    if ((v != CFG_BASIC_RATE_SET_DEFAULT)
        && (v != CFG_BASIC_RATE_SET_EXTRA)
        && (v != CFG_BASIC_RATE_SET_LEGACY))
      return -EINVAL;
    if ((v == CFG_BASIC_RATE_SET_LEGACY)
        && (nic->slow_ctx->frequency_band_cfg != MTLK_HW_BAND_2_4_GHZ))
      return -EINVAL;
    nic->slow_ctx->cfg.basic_rate_set = v;
    break;
  case PRM_ID_AOCS_NON_OCCUPANCY_PERIOD:
    nic->slow_ctx->aocs.dbg_non_occupied_period = v;
    return 0;
  case PRM_ID_USE_8021Q:
    if (MTLK_ERR_OK != mtlk_qos_set_map(v))
    {
      return -EINVAL;
    }
    break;
  case PRM_ID_NETWORK_MODE:
    if(mtlk_core_scan_is_running(nic))
    {
      ELOG("Cannot set network mode while scan is running");
      return -EAGAIN;
    }
    else if (mtlk_core_get_net_state(nic) == NET_STATE_CONNECTED)
    {
      return -EBUSY;
    }
    else
    {
      net_mode = net_mode_ingress_filter(v);
      if (net_mode != NETWORK_NONE) 
        if (MTLK_ERR_OK == mtlk_core_set_network_mode(nic, net_mode))
          return 0;

      return -EINVAL;
    }
  case PRM_ID_IS_BACKGROUND_SCAN:
    if (nic->ap)
      res = -EINVAL;
    else
      nic->slow_ctx->cfg.is_background_scan = !!v;
    break;
  case PRM_ID_BG_SCAN_CH_LIMIT:
    if (nic->ap)
      res = -EINVAL;
    else
      nic->slow_ctx->scan.params.channels_per_chunk_limit = v;
    break;
  case PRM_ID_BG_SCAN_PAUSE:
    if (nic->ap)
      res = -EINVAL;
    else
      nic->slow_ctx->scan.params.pause_between_chunks = v;
    break;
  case PRM_ID_MAC_WATCHDOG_TIMEOUT_MS:
    if ((v > (uint16)(-1)) || (v < 1000))
      res = -EINVAL;
    else
      nic->slow_ctx->mac_watchdog_timeout_ms = v;
    break;
  case PRM_ID_MAC_WATCHDOG_PERIOD_MS:
    if (v == 0)
      res = -EINVAL;
    else
      nic->slow_ctx->mac_watchdog_period_ms = v;
    break;
#ifdef MTCFG_IRB_DEBUG
  case PRM_ID_IRB_PINGER_ENABLED:
    if (mtlk_irb_pinger_is_started(&nic->slow_ctx->pinger)) {
      mtlk_irb_pinger_stop(&nic->slow_ctx->pinger);
    }
    if (v  && 
        mtlk_irb_pinger_start(&nic->slow_ctx->pinger, v) != MTLK_ERR_OK) {
      res = -EAGAIN;  /* start failed */
    }
    break;
  case PRM_ID_IRB_PINGER_STATS:
    mtlk_irb_pinger_zero_stats(&nic->slow_ctx->pinger);
    break;
#endif
#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
  case PRM_ID_PPA_API_DIRECTPATH:
    if (!mtlk_core_ppa_is_available(nic)) {
      res = -EOPNOTSUPP;
    }
    else if (!v && mtlk_core_ppa_is_registered(nic)) {
      mtlk_core_ppa_unregister(nic);
    }
    else if (v && !mtlk_core_ppa_is_registered(nic)) {
      res = (mtlk_core_ppa_register(nic) == MTLK_ERR_OK)?0:-EAGAIN;
    }
    break;
#endif
  case PRM_ID_COC_LOW_POWER_MODE:
    if (NET_STATE_CONNECTED != mtlk_core_get_net_state(nic))
    {
      ELOG("Cannot set CoC power mode while not connected");
      res = -EAGAIN;
    }
    else if (mtlk_core_scan_is_running(nic))
    {
      ELOG("Cannot set CoC power mode while scan is running");
      res = -EAGAIN;
    }
    else
    {
      int coc_res = MTLK_ERR_OK;
      switch ((eCOC_POWER_MODE)v)
      {
        case COC_HIGH_POWER_MODE:
          coc_res = mtlk_coc_high_power_mode_enable(nic->slow_ctx->coc_mngmt);
          break;
        case COC_RADIO_OFF_POWER_MODE:
          coc_res = mtlk_coc_radiooff_power_mode_enable(nic->slow_ctx->coc_mngmt);
          break;
        case COC_LOW_POWER_MODE:
        default:
          coc_res = mtlk_coc_low_power_mode_enable(nic->slow_ctx->coc_mngmt);
          break;
      }
      if (MTLK_ERR_OK != coc_res)
        res = -EAGAIN;
    }
    break;
  case PRM_CHANGE_TX_POWER_LIMIT:
    if (mtlk_core_scan_is_running(nic))
    {
      ELOG("Cannot set TxPowerLimitOption while scan is running");
      res = -EAGAIN;
    }
    else if ( MTLK_ERR_OK != mtlk_core_change_tx_power_limit(nic, v))
    {
      res = -EFAULT;
    }
    break;
  case PRM_ID_PEERAP_KEY_IDX:
    if (!nic->ap) {
      res = -EOPNOTSUPP;
      ELOG("%s: The command is supported only in AP mode", dev->name);
    } else {
      if (v < 0 || v > 4) {
        ELOG("%s: usage: <[1-4]|0> (0 means Open)", dev->name);
        return -EINVAL;
      }
      if (nic->slow_ctx->wep_keys.sKey[v].u8KeyLength == 0) {
        ELOG("%s: WEP key [%d] is not set yet", dev->name, v);
        return -EFAULT;
      }
      nic->slow_ctx->peerAPs_key_idx = v;
    }
    break;
  case PRM_ID_AGGR_OPEN_THRESHOLD:
    if (v != 0) {
      nic->slow_ctx->stadb.aggr_open_threshold = v;
    }
    break;
  case PRM_ID_IS_HALT_FW_ON_DISC_TIMEOUT:
    nic->slow_ctx->is_halt_fw_on_disc_timeout = !!v;
    break;
  default:
    res = -EOPNOTSUPP;
    ILOG2(GID_IOCTL, "Unsupported subcmd: 0x%04x", subcmd);
    break;
  };

  return res;
}


static int
mtlk_ioctl_get_int (struct net_device *dev,
               struct iw_request_info *info,
               union iwreq_data *wrqu,
               char *extra)
{
  struct nic *nic = netdev_priv(dev);
  mtlk_txmm_t *txmm = nic->slow_ctx->hw_cfg.txmm;
  uint32 subcmd;
  uint32 v = 0;
  int res = 0;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  subcmd = wrqu->mode;
  ILOG2(GID_IOCTL, "subcmd = 0x%04x", subcmd);

  switch (subcmd) {
  case PRM_ID_BE_BAUSE:
    v = nic->slow_ctx->cfg.addba.tid[0].use_aggr;
    break;
  case PRM_ID_BK_BAUSE:
    v = nic->slow_ctx->cfg.addba.tid[1].use_aggr;
    break;
  case PRM_ID_VI_BAUSE:
    v = nic->slow_ctx->cfg.addba.tid[5].use_aggr;
    break;
  case PRM_ID_VO_BAUSE:
    v = nic->slow_ctx->cfg.addba.tid[6].use_aggr;
    break;
  case PRM_ID_BE_BAACCEPT:
    v = nic->slow_ctx->cfg.addba.tid[0].accept_aggr;
    v = nic->slow_ctx->cfg.addba.tid[3].accept_aggr;
    break;
  case PRM_ID_BK_BAACCEPT:
    v = nic->slow_ctx->cfg.addba.tid[1].accept_aggr;
    v = nic->slow_ctx->cfg.addba.tid[2].accept_aggr;
    break;
  case PRM_ID_VI_BAACCEPT:
    v = nic->slow_ctx->cfg.addba.tid[5].accept_aggr;
    v = nic->slow_ctx->cfg.addba.tid[4].accept_aggr;
    break;
  case PRM_ID_VO_BAACCEPT:
    v = nic->slow_ctx->cfg.addba.tid[6].accept_aggr;
    v = nic->slow_ctx->cfg.addba.tid[7].accept_aggr;
    break;
  case PRM_ID_BE_BATIMEOUT:
    v = nic->slow_ctx->cfg.addba.tid[0].addba_timeout;
    break;
  case PRM_ID_BK_BATIMEOUT:
    v = nic->slow_ctx->cfg.addba.tid[1].addba_timeout;
    break;
  case PRM_ID_VI_BATIMEOUT:
    v = nic->slow_ctx->cfg.addba.tid[5].addba_timeout;
    break;
  case PRM_ID_VO_BATIMEOUT:
    v = nic->slow_ctx->cfg.addba.tid[6].addba_timeout;
    break;
  case PRM_ID_BE_BAWINSIZE:
    v = nic->slow_ctx->cfg.addba.tid[0].aggr_win_size;
    break;
  case PRM_ID_BK_BAWINSIZE:
    v = nic->slow_ctx->cfg.addba.tid[1].aggr_win_size;
    break;
  case PRM_ID_VI_BAWINSIZE:
    v = nic->slow_ctx->cfg.addba.tid[5].aggr_win_size;
    break;
  case PRM_ID_VO_BAWINSIZE:
    v = nic->slow_ctx->cfg.addba.tid[6].aggr_win_size;
    break;
  case PRM_ID_BE_AGGRMAXBTS:
    v = nic->slow_ctx->cfg.addba.tid[0].max_nof_bytes;
    break;
  case PRM_ID_BK_AGGRMAXBTS:
    v = nic->slow_ctx->cfg.addba.tid[1].max_nof_bytes;
    break;
  case PRM_ID_VI_AGGRMAXBTS:
    v = nic->slow_ctx->cfg.addba.tid[5].max_nof_bytes;
    break;
  case PRM_ID_VO_AGGRMAXBTS:
    v = nic->slow_ctx->cfg.addba.tid[6].max_nof_bytes;
    break;
  case PRM_ID_BE_AGGRMAXPKTS:
    v = nic->slow_ctx->cfg.addba.tid[0].max_nof_packets;
    break;
  case PRM_ID_BK_AGGRMAXPKTS:
    v = nic->slow_ctx->cfg.addba.tid[1].max_nof_packets;
    break;
  case PRM_ID_VI_AGGRMAXPKTS:
    v = nic->slow_ctx->cfg.addba.tid[5].max_nof_packets;
    break;
  case PRM_ID_VO_AGGRMAXPKTS:
    v = nic->slow_ctx->cfg.addba.tid[6].max_nof_packets;
    break;
  case PRM_ID_BE_AGGRMINPTSZ:
    v = nic->slow_ctx->cfg.addba.tid[0].min_packet_size_in_aggr;
    break;
  case PRM_ID_BK_AGGRMINPTSZ:
    v = nic->slow_ctx->cfg.addba.tid[1].min_packet_size_in_aggr;
    break;
  case PRM_ID_VI_AGGRMINPTSZ:
    v = nic->slow_ctx->cfg.addba.tid[5].min_packet_size_in_aggr;
    break;
  case PRM_ID_VO_AGGRMINPTSZ:
    v = nic->slow_ctx->cfg.addba.tid[6].min_packet_size_in_aggr;
    break;
  case PRM_ID_BE_AGGRTIMEOUT:
    v = nic->slow_ctx->cfg.addba.tid[0].timeout_interval;
    break;
  case PRM_ID_BK_AGGRTIMEOUT:
    v = nic->slow_ctx->cfg.addba.tid[1].timeout_interval;
    break;
  case PRM_ID_VI_AGGRTIMEOUT:
    v = nic->slow_ctx->cfg.addba.tid[5].timeout_interval;
    break;
  case PRM_ID_VO_AGGRTIMEOUT:
    v = nic->slow_ctx->cfg.addba.tid[6].timeout_interval;
    break;
  case PRM_ID_BE_AIFSN:
    v = nic->slow_ctx->cfg.wme_bss.wme_class[0].aifsn;
    break;
  case PRM_ID_BK_AIFSN:
    v = nic->slow_ctx->cfg.wme_bss.wme_class[1].aifsn;
    break;
  case PRM_ID_VI_AIFSN:
    v = nic->slow_ctx->cfg.wme_bss.wme_class[2].aifsn;
    break;
  case PRM_ID_VO_AIFSN:
    v = nic->slow_ctx->cfg.wme_bss.wme_class[3].aifsn;
    break;
  case PRM_ID_BE_AIFSNAP:
    v = nic->slow_ctx->cfg.wme_ap.wme_class[0].aifsn;
    break;
  case PRM_ID_BK_AIFSNAP:
    v = nic->slow_ctx->cfg.wme_ap.wme_class[1].aifsn;
    break;
  case PRM_ID_VI_AIFSNAP:
    v = nic->slow_ctx->cfg.wme_ap.wme_class[2].aifsn;
    break;
  case PRM_ID_VO_AIFSNAP:
    v = nic->slow_ctx->cfg.wme_ap.wme_class[3].aifsn;
    break;
  case PRM_ID_BE_CWMAX:
    v = nic->slow_ctx->cfg.wme_bss.wme_class[0].cwmax;
    break;
  case PRM_ID_BK_CWMAX:
    v = nic->slow_ctx->cfg.wme_bss.wme_class[1].cwmax;
    break;
  case PRM_ID_VI_CWMAX:
    v = nic->slow_ctx->cfg.wme_bss.wme_class[2].cwmax;
    break;
  case PRM_ID_VO_CWMAX:
    v = nic->slow_ctx->cfg.wme_bss.wme_class[3].cwmax;
    break;
  case PRM_ID_BE_CWMAXAP:
    v = nic->slow_ctx->cfg.wme_ap.wme_class[0].cwmax;
    break;
  case PRM_ID_BK_CWMAXAP:
    v = nic->slow_ctx->cfg.wme_ap.wme_class[1].cwmax;
    break;
  case PRM_ID_VI_CWMAXAP:
    v = nic->slow_ctx->cfg.wme_ap.wme_class[2].cwmax;
    break;
  case PRM_ID_VO_CWMAXAP:
    v = nic->slow_ctx->cfg.wme_ap.wme_class[3].cwmax;
    break;
  case PRM_ID_BE_CWMIN:
    v = nic->slow_ctx->cfg.wme_bss.wme_class[0].cwmin;
    break;
  case PRM_ID_BK_CWMIN:
    v = nic->slow_ctx->cfg.wme_bss.wme_class[1].cwmin;
    break;
  case PRM_ID_VI_CWMIN:
    v = nic->slow_ctx->cfg.wme_bss.wme_class[2].cwmin;
    break;
  case PRM_ID_VO_CWMIN:
    v = nic->slow_ctx->cfg.wme_bss.wme_class[3].cwmin;
    break;
  case PRM_ID_BE_CWMINAP:
    v = nic->slow_ctx->cfg.wme_ap.wme_class[0].cwmin;
    break;
  case PRM_ID_BK_CWMINAP:
    v = nic->slow_ctx->cfg.wme_ap.wme_class[1].cwmin;
    break;
  case PRM_ID_VI_CWMINAP:
    v = nic->slow_ctx->cfg.wme_ap.wme_class[2].cwmin;
    break;
  case PRM_ID_VO_CWMINAP:
    v = nic->slow_ctx->cfg.wme_ap.wme_class[3].cwmin;
    break;
  case PRM_ID_BE_TXOP:
    v = nic->slow_ctx->cfg.wme_bss.wme_class[0].txop;
    break;
  case PRM_ID_BK_TXOP:
    v = nic->slow_ctx->cfg.wme_bss.wme_class[1].txop;
    break;
  case PRM_ID_VI_TXOP:
    v = nic->slow_ctx->cfg.wme_bss.wme_class[2].txop;
    break;
  case PRM_ID_VO_TXOP:
    v = nic->slow_ctx->cfg.wme_bss.wme_class[3].txop;
    break;
  case PRM_ID_BE_TXOPAP:
    v = nic->slow_ctx->cfg.wme_ap.wme_class[0].txop;
    break;
  case PRM_ID_BK_TXOPAP:
    v = nic->slow_ctx->cfg.wme_ap.wme_class[1].txop;
    break;
  case PRM_ID_VI_TXOPAP:
    v = nic->slow_ctx->cfg.wme_ap.wme_class[2].txop;
    break;
  case PRM_ID_VO_TXOPAP:
    v = nic->slow_ctx->cfg.wme_ap.wme_class[3].txop;
    break;
  case PRM_ID_11H_BEACON_COUNT:
    v = nic->slow_ctx->cfg.dot11h_debug_params.debugChannelSwitchCount;
    break;
  case PRM_ID_11H_CHANNEL_AVAILABILITY_CHECK_TIME:
    v = nic->slow_ctx->cfg.dot11h_debug_params.debugChannelAvailabilityCheckTime;
    break;
  case PRM_ID_11H_NEXT_CHANNEL:
    v = nic->slow_ctx->cfg.dot11h_debug_params.debugNewChannel;
  case MIB_ACL_MODE:
    v = mtlk_get_num_mib_value(PRM_ACL_MODE, nic);
    break;
  case MIB_ACL_MAX_CONNECTIONS:
    v = mtlk_get_num_mib_value(PRM_ACL_MAX_CONNECTIONS, nic);
    break;
  case PRM_ID_RADAR_DETECTION:
    v = nic->slow_ctx->dot11h.cfg._11h_radar_detect;
    break;
  case PRM_ID_11H_ENABLE_SM_CHANNELS:
    v = !mtlk_aocs_is_smrequired_disabled(&nic->slow_ctx->aocs);
    break;
  case PRM_ID_11D:
    v = nic->slow_ctx->cfg.dot11d;
    break;
  case PRM_ID_POWER_SELECTION:
    v = nic->slow_ctx->power_selection;
    break;
  case PRM_ID_SPECTRUM_MODE:
    if (nic->ap) {
      v = mtlk_get_num_mib_value(PRM_SPECTRUM_MODE, nic);
    } else {
      v = nic->slow_ctx->sta_force_spectrum_mode;
    }
    break;
  case MIB_CALIBRATION_ALGO_MASK:
    v = mtlk_get_num_mib_value(PRM_ALGO_CALIBR_MASK, nic);
    break;
  case MIB_POWER_INCREASE_VS_DUTY_CYCLE:
    v = mtlk_get_num_mib_value(PRM_POWER_INCREASE_VS_DUTY_CYCLE, nic);
    break;
  case MIB_USE_SHORT_CYCLIC_PREFIX:
    v = mtlk_get_num_mib_value(PRM_SHORT_CYCLIC_PREFIX, nic);
    break;
  case MIB_SHORT_PREAMBLE_OPTION_IMPLEMENTED:
    v = mtlk_get_num_mib_value(PRM_SHORT_PREAMBLE, nic);
    break;
  case MIB_SHORT_SLOT_TIME_OPTION_ENABLED_11G:
    v = mtlk_get_num_mib_value(PRM_SHORT_SLOT_TIME_OPTION_ENABLED, nic);
    break;
  case PRM_ID_WDS_HOST_TIMEOUT:
    v = nic->slow_ctx->stadb.wds_host_timeout;
    break;
  case PRM_ID_STA_KEEPALIVE_TIMEOUT:
    v = nic->slow_ctx->stadb.sta_keepalive_timeout;
    break;
  case PRM_ID_STA_KEEPALIVE_INTERVAL:
    v = nic->slow_ctx->stadb.keepalive_interval;
    break;
  case MIB_SHORT_RETRY_LIMIT:
  case MIB_LONG_RETRY_LIMIT:
  case MIB_TX_MSDU_LIFETIME:
  case MIB_CURRENT_TX_ANTENNA:
  case MIB_BEACON_PERIOD:
  case MIB_DISCONNECT_ON_NACKS_WEIGHT:
    {
      uint16 v16;
      mtlk_get_mib_value_uint16(txmm, subcmd, &v16);
      v = v16;
    }
    break;
  case MIB_HIDDEN_SSID:
  case MIB_ADVANCED_CODING_SUPPORTED:
  case MIB_OVERLAPPING_PROTECTION_ENABLE:
  case MIB_OFDM_PROTECTION_METHOD:
  case MIB_HT_PROTECTION_METHOD:
  case MIB_DTIM_PERIOD:
  case MIB_RECEIVE_AMPDU_MAX_LENGTH:
  case MIB_CB_DATABINS_PER_SYMBOL:
  case MIB_USE_LONG_PREAMBLE_FOR_MULTICAST:
  case MIB_USE_SPACE_TIME_BLOCK_CODE:
  case MIB_ONLINE_CALIBRATION_ALGO_MASK:
  case MIB_DISCONNECT_ON_NACKS_ENABLE:  
    {
      uint8 v8;
      mtlk_get_mib_value_uint8(txmm, subcmd, &v8);
      v = v8;
    }
    break;
  case MIB_SM_ENABLE:
    v = mtlk_get_num_mib_value(PRM_CHANNEL_ANNOUNCEMENT_ENABLED, nic);
    break;
  case PRM_ID_BRIDGE_MODE:
    v = mtlk_aux_atol(mtlk_get_mib_value(PRM_BRIDGE_MODE, nic));
    break;
  case PRM_ID_L2NAT_AGING_TIMEOUT:
    v = mtlk_aux_atol(mtlk_get_mib_value(PRM_L2NAT_AGING_TIMEOUT, nic));
    break;
  case PRM_ID_AOCS_WEIGHT_CL:
    v = mtlk_aocs_get_weight(&nic->slow_ctx->aocs, AOCS_WEIGHT_IDX_CL);
    break;
  case PRM_ID_AOCS_WEIGHT_TX:
    v = mtlk_aocs_get_weight(&nic->slow_ctx->aocs, AOCS_WEIGHT_IDX_TX);
    break;
  case PRM_ID_AOCS_WEIGHT_BSS:
    v = mtlk_aocs_get_weight(&nic->slow_ctx->aocs, AOCS_WEIGHT_IDX_BSS);
    break;
  case PRM_ID_AOCS_WEIGHT_SM:
    v = mtlk_aocs_get_weight(&nic->slow_ctx->aocs, AOCS_WEIGHT_IDX_SM);
    break;
  case PRM_ID_AOCS_SCAN_AGING:
    v = mtlk_aocs_get_scan_aging(&nic->slow_ctx->aocs);
    break;
  case PRM_ID_AOCS_CONFIRM_RANK_AGING:
    v = mtlk_aocs_get_confirm_rank_aging(&nic->slow_ctx->aocs);
    break;
  case PRM_ID_AOCS_CFM_RANK_SW_THRESHOLD:
    v = mtlk_aocs_get_cfm_rank_sw_threshold(&nic->slow_ctx->aocs);
    break;
  case PRM_ID_AOCS_AFILTER:
    v = mtlk_aocs_get_afilter(&nic->slow_ctx->aocs);
    break;
  case PRM_ID_AOCS_BONDING:
    v = mtlk_core_get_bonding(nic);
    break;
  case PRM_ID_AOCS_EN_PENALTIES:
    v = mtlk_aocs_get_penalty_enabled(&nic->slow_ctx->aocs);
    break;
  case PRM_ID_AOCS_WIN_TIME:
    v = mtlk_aocs_get_win_time(&nic->slow_ctx->aocs);
    break;
  case PRM_ID_AOCS_LOWER_THRESHOLD:
    v = mtlk_aocs_get_lower_threshold(&nic->slow_ctx->aocs);
    break;
  case PRM_ID_AOCS_THRESHOLD_WINDOW:
    v = mtlk_aocs_get_threshold_window(&nic->slow_ctx->aocs);
    break;
  case PRM_ID_AOCS_MSDU_DEBUG_ENABLED:
    v = mtlk_aocs_get_msdu_debug_enabled(&nic->slow_ctx->aocs);
    break;
  case PRM_ID_AOCS_IS_ENABLED:
    v = mtlk_aocs_get_type(&nic->slow_ctx->aocs);
    break;
  case PRM_ID_AOCS_MSDU_PER_WIN_THRESHOLD:
    v = mtlk_aocs_get_msdu_win_thr(&nic->slow_ctx->aocs);
    break;
  case PRM_ID_AOCS_MSDU_THRESHOLD:
    v = mtlk_aocs_get_msdu_threshold(&nic->slow_ctx->aocs);
    break;
  case PRM_ID_AOCS_MEASUREMENT_WINDOW:
    v = mtlk_aocs_get_measurement_window(&nic->slow_ctx->aocs);
    break;
  case PRM_ID_AOCS_THROUGHPUT_THRESHOLD:
    v = mtlk_aocs_get_troughput_threshold(&nic->slow_ctx->aocs);
    break;
  case PRM_ID_PACK_SCHED_ENABLED:
    v = mtlk_aux_atol(mtlk_get_mib_value(PRM_ENABLE_PACK_SCHED, nic));
    break;
  case PRM_DBG_SW_WD_ENABLE:
    v = mtlk_core_sw_reset_enable_get(nic);
    break;
  case PRM_ID_SCAN_CACHE_LIFETIME:
    v = nic->slow_ctx->cache.cache_expire;
    break;
  case PRM_ID_RELIABLE_MULTICAST:
    v = nic->reliable_mcast;
    break;
  case PRM_ID_AP_FORWARDING:
    v = mtlk_aux_atol(mtlk_get_mib_value(PRM_AP_FORWARDING, nic));
    break;
  case MIB_BSS_BASIC_RATE_SET:
    if (nic->ap == 0)
      return -EOPNOTSUPP;
    v = nic->slow_ctx->cfg.basic_rate_set;
    break;
  case PRM_ID_AOCS_NON_OCCUPANCY_PERIOD:
    v = nic->slow_ctx->aocs.dbg_non_occupied_period;
    break;
  case PRM_ID_USE_8021Q:
    v = mtlk_qos_get_map();
    break;
  case PRM_ID_NETWORK_MODE:
    v = net_mode_egress_filter(nic->slow_ctx->net_mode_cfg);
    break;
  case PRM_ID_IS_BACKGROUND_SCAN:
    v = nic->slow_ctx->cfg.is_background_scan;
    break;
  case PRM_ID_BG_SCAN_CH_LIMIT:
    v = nic->slow_ctx->scan.params.channels_per_chunk_limit;
    break;
  case PRM_ID_BG_SCAN_PAUSE:
    v = nic->slow_ctx->scan.params.pause_between_chunks;
    break;
  case PRM_ID_MAC_WATCHDOG_TIMEOUT_MS:
    v = nic->slow_ctx->mac_watchdog_timeout_ms;
    break;
  case PRM_ID_MAC_WATCHDOG_PERIOD_MS:
    v = nic->slow_ctx->mac_watchdog_period_ms;
    break;
#ifdef MTCFG_IRB_DEBUG
  case PRM_ID_IRB_PINGER_ENABLED:
    v = mtlk_irb_pinger_get_ping_period_ms(&nic->slow_ctx->pinger);
    break;
#endif
#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
  case PRM_ID_PPA_API_DIRECTPATH:
    if (!mtlk_core_ppa_is_available(nic)) {
      res = -EOPNOTSUPP;
    }
    else {
      v = mtlk_core_ppa_is_registered(nic);
    }
    break;
#endif
  case PRM_ID_COC_LOW_POWER_MODE:
    v = mtlk_coc_low_power_mode_get(nic->slow_ctx->coc_mngmt);
    break;
  case PRM_CHANGE_TX_POWER_LIMIT:
    {
      uint8 power_limit = 0;
      if (mtlk_core_get_tx_power_limit(nic, &power_limit) == MTLK_ERR_OK) {
        v = (uint32)power_limit;
      }
      else {
        res = -EAGAIN;
      }
    }
    break;
  case PRM_ID_PEERAP_KEY_IDX:
    if (!nic->ap) {
      res = -EOPNOTSUPP;
      ELOG("%s: The command is supported only in AP mode", dev->name);
    } else {
      v = nic->slow_ctx->peerAPs_key_idx;
    }
    break;
  case PRM_ID_AGGR_OPEN_THRESHOLD:
    v= nic->slow_ctx->stadb.aggr_open_threshold;
    break;
  case PRM_ID_IS_HALT_FW_ON_DISC_TIMEOUT:
    v= nic->slow_ctx->is_halt_fw_on_disc_timeout;
    break;
  default:
    res = -EOPNOTSUPP;
    ILOG2(GID_IOCTL, "Unsupported subcmd: 0x%04x", subcmd);
    break;
  }

  *(uint32*)extra = v;

  return res;
}

static int
mtlk_ioctl_set_intvec (struct net_device *dev,
                  struct iw_request_info *info,
                  union iwreq_data *wrqu,
                  char *extra)
{
  struct nic *nic = netdev_priv(dev);
  int nof_ints = wrqu->data.length;
  uint32 subcmd;
  int32 *v;
  int res = 0;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  subcmd = wrqu->data.flags;
  v = (int32*)extra;
  ILOG2(GID_IOCTL, "subcmd = 0x%04x", subcmd);

  switch (subcmd) {

  case PRM_ID_SQ_LIMITS:
    /* call the "set limits" handler */
    if (sq_set_limits(nic, v, nof_ints) != MTLK_ERR_OK )  
      res = -EINVAL;
    break;
  case PRM_ID_SQ_PEER_LIMITS:
    /* call the "set limits" handler */
    if (sq_set_peer_limits(nic, v, nof_ints) != MTLK_ERR_OK )  
      res = -EINVAL;
    break;
  case PRM_ID_AOCS_PENALTIES:
    if (mtlk_aocs_set_tx_penalty(&nic->slow_ctx->aocs, v, nof_ints) != MTLK_ERR_OK )
      res = -EINVAL;
    break;
  default:
    res = -EOPNOTSUPP;
    ILOG2(GID_IOCTL, "Unsupported subcmd: 0x%04x", subcmd);
    break;
  };

  return res;
}

static int
mtlk_ioctl_get_intvec (struct net_device *dev,
                  struct iw_request_info *info,
                  union iwreq_data *wrqu,
                  char *extra)
{
/* SendQueue currently is the only user of "intvec" private ioctls
 * and when it's not compiled into the driver this variable causes
 * "unused variable" warning. This ifdef should be removed when
 * some other "intvec" parameter is added.
 */
  struct nic *nic = netdev_priv(dev);
  uint32 subcmd;
  int32 *v;
  int length = wrqu->data.length;
  int res = 0;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  subcmd = wrqu->data.flags;
  v = (int32*)extra;
  ILOG2(GID_IOCTL, "subcmd = 0x%04x", subcmd);

  switch (subcmd) {

  case PRM_ID_SQ_LIMITS:
    /* call the "get limits" handler */
    if ( sq_get_limits(nic, v, &length) != MTLK_ERR_OK )
      res = -EINVAL;
    break;
  case PRM_ID_SQ_PEER_LIMITS:
    /* call the "get limits" handler */
    if ( sq_get_peer_limits(nic, v, &length) != MTLK_ERR_OK )
      res = -EINVAL;
    break;

  default:
    res = -EOPNOTSUPP;
    ILOG2(GID_IOCTL, "Unsupported subcmd: 0x%04x", subcmd);
    break;
  }

  wrqu->data.length = length;

  return res;
}


static int
mtlk_wds_add_peer_ap (struct nic *nic, struct sockaddr *sa, int add)
{
  int res = 0;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_PEERAP *umi_pap;
  uint16 status;

  if (sa->sa_family != ARPHRD_ETHER || !is_valid_ether_addr(sa->sa_data))
    return -EINVAL;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, nic->slow_ctx->hw_cfg.txmm, NULL);
  if (!man_entry) {
    ELOG("No man entry available");
    return -ENOMEM;
  }

  umi_pap = (UMI_PEERAP*) man_entry->payload;
  memset(umi_pap, 0, sizeof(*umi_pap));

  man_entry->id           = UM_MAN_SET_PEERAP_REQ;
  man_entry->payload_size = sizeof(*umi_pap);

  memcpy(umi_pap->sStationID.au8Addr, sa->sa_data, ETH_ALEN);
  umi_pap->u16Status = cpu_to_le16(UMI_OK);
  if (!add) /* Remove */
    umi_pap->u16Status = cpu_to_le16(UMI_BAD_VALUE);

  res = mtlk_txmm_msg_send_blocked(&man_msg,
                                   MTLK_MM_BLOCKED_SEND_TIMEOUT);

  if (res < MTLK_ERR_OK) {
    ELOG("mtlk_mm_send_blocked failed: %i", res);
    res = -EFAULT;
    goto end;
  }
  res = 0;
  status = le16_to_cpu(umi_pap->u16Status);
  if (status != UMI_OK) {
    switch (status) {
    case UMI_OUT_OF_MEMORY:
      WLOG("No more peer-AP allowed");
      res = -ENOMEM;
      break;
    case UMI_BAD_VALUE:
      WLOG("There is no such peer-AP configured");
      res = -EINVAL;
      break;
    default:
      WLOG("invalid status of last msg %04x sending to MAC - %i",
          man_entry->id, status);
      res = -EFAULT;
    }
    goto end;
  }

end:
  if (man_entry)
    mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}


static int
mtlk_ioctl_set_addr (struct net_device *dev,
                struct iw_request_info *info,
                union iwreq_data *wrqu,
                char *extra)
{
  struct nic *nic = netdev_priv(dev);
  uint32 subcmd;
  struct sockaddr v;
  int res = 0;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  subcmd = wrqu->data.flags;
  v = *(struct sockaddr*)extra;

  ILOG2(GID_IOCTL, "subcmd = 0x%04x", subcmd);

  switch (subcmd) {
  case PRM_ID_L2NAT_DEFAULT_HOST:
    res = mtlk_l2nat_user_set_def_host(nic, &v);
    break;
  case PRM_ID_STADB_LOCAL_MAC:
    res = mtlk_stadb_set_local_mac((mtlk_handle_t)&nic->slow_ctx->stadb, v.sa_data);
    if (res != MTLK_ERR_OK)
      res = -EINVAL;
    else
      res = 0;
    break;
  case PRM_ID_ADD_PEERAP:
  case PRM_ID_DEL_PEERAP:
    if (!nic->ap) {
      ELOG("%s: The command is supported only in AP mode", dev->name);
      res = -EOPNOTSUPP;
      break;
    }
    if (subcmd == PRM_ID_ADD_PEERAP)
      res = mtlk_wds_add_peer_ap(nic, &v, 1);
    else
      res = mtlk_wds_add_peer_ap(nic, &v, 0);
    break;
  default:
    res = -EOPNOTSUPP;
    ILOG2(GID_IOCTL, "Unsupported subcmd: 0x%04x", subcmd);
    break;
  };

  return res;
}


static int
mtlk_ioctl_get_addr (struct net_device *dev,
                struct iw_request_info *info,
                union iwreq_data *wrqu,
                char *extra)
{
  struct nic *nic = netdev_priv(dev);
  uint32 subcmd;
  struct sockaddr v;
  int res = 0;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  subcmd = wrqu->mode;
  ILOG2(GID_IOCTL, "subcmd = 0x%04x", subcmd);

  switch (subcmd) {
  case PRM_ID_L2NAT_DEFAULT_HOST:
    mtlk_l2nat_get_def_host(nic, &v);
    break;
  case PRM_ID_STADB_LOCAL_MAC:
    mtlk_stadb_get_local_mac((mtlk_handle_t)&nic->slow_ctx->stadb, v.sa_data);
    break;
  default:
    res = -EOPNOTSUPP;
    ILOG2(GID_IOCTL, "Unsupported subcmd: 0x%04x", subcmd);
    break;
  }

  *(struct sockaddr*)extra = v;

  return res;
}

static int
mtlk_ioctl_set_addrvec (struct net_device *dev,
                   struct iw_request_info *info,
                   union iwreq_data *wrqu,
                   char *extra)
{
  struct nic *nic = netdev_priv(dev);
  uint32 subcmd;
  struct sockaddr *v;
  int length = wrqu->data.length;
  int res = 0;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  subcmd = wrqu->data.flags;
  v = (struct sockaddr*)extra;
  ILOG2(GID_IOCTL, "subcmd = 0x%04x", subcmd);

  switch (subcmd) {
  case PRM_ID_ACL:
    {
      IEEE_ADDR mac;
      int i;

      for (i = 0; i < length; i++) {
        memcpy(mac.au8Addr, v[i].sa_data, sizeof(mac.au8Addr));
        if (MTLK_ERR_OK != mtlk_core_set_acl(nic, &mac, NULL)) {
          res = -EINVAL;
          break;
        }
      }

      if (0 == res) {
        /* Update ACL in MAC*/
        if (MTLK_ERR_OK !=
            mtlk_set_mib_acl(nic->slow_ctx->hw_cfg.txmm, nic->slow_ctx->acl, nic->slow_ctx->acl_mask)) {
          res = -EFAULT;
        }
      }
    }
    break;
  case PRM_ID_ACL_RANGE:
    {
      IEEE_ADDR mac;
      IEEE_ADDR mac_mask;
      int i;

      if (0 != (length % 2)) {
        ILOG2(GID_IOCTL, "Address vector length should be even. length(%u)", length);
        res = -EINVAL;
      } else {

        for (i = 0; i < length; i++) {
          memcpy(mac.au8Addr, v[i].sa_data, sizeof(mac.au8Addr));
          i++;
          memcpy(mac_mask.au8Addr, v[i].sa_data, sizeof(mac.au8Addr));

          if (MTLK_ERR_OK != mtlk_core_set_acl(nic, &mac, &mac_mask)) {
            res = -EINVAL;
            break;
          }
        }
      }

      if (0 == res) {
        /* Update ACL in MAC*/
        if (MTLK_ERR_OK !=
            mtlk_set_mib_acl(nic->slow_ctx->hw_cfg.txmm, nic->slow_ctx->acl, nic->slow_ctx->acl_mask)) {
          res = -EFAULT;
        }
      }
    }
    break;
  case PRM_ID_ACL_DEL:
    {
      IEEE_ADDR mac;
      int i;

      for (i = 0; i < length; i++) {
        memcpy(mac.au8Addr, v[i].sa_data, sizeof(mac.au8Addr));
        if (mtlk_core_del_acl(nic, &mac) != MTLK_ERR_OK) {
          res = -EINVAL;
          break;
        }
      }

      if (0 == res) {
        /* Update ACL in MAC*/
        if (MTLK_ERR_OK !=
            mtlk_set_mib_acl(nic->slow_ctx->hw_cfg.txmm, nic->slow_ctx->acl, nic->slow_ctx->acl_mask)) {
          res = -EFAULT;
        }
      }
    }
    break;
  default:
    res = -EOPNOTSUPP;
    ILOG2(GID_IOCTL, "Unsupported subcmd: 0x%04x", subcmd);
    break;
  };

  return res;
}

static int
mtlk_ioctl_get_addrvec (struct net_device *dev,
                   struct iw_request_info *info,
                   union iwreq_data *wrqu,
                   char *extra)
{
  struct nic *nic = netdev_priv(dev);
  uint32 subcmd;
  struct sockaddr *v;
  int length = wrqu->data.length;
  int res = 0;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  subcmd = wrqu->data.flags;
  v = (struct sockaddr*)extra;
  ILOG2(GID_IOCTL, "subcmd = 0x%04x", subcmd);

  switch (subcmd) {
  case PRM_ID_ACL:
    {
      int i;

      for (i = 0, length = 0; i < MAX_ADDRESSES_IN_ACL; i++) {
        if (mtlk_osal_is_zero_address(nic->slow_ctx->acl[i].au8Addr))
          continue;
        memcpy(v[length++].sa_data, nic->slow_ctx->acl[i].au8Addr, sizeof(IEEE_ADDR));
      }
    }
    break;
  case PRM_ID_ACL_RANGE:
    {
      int i;

      for (i = 0, length = 0; i < MAX_ADDRESSES_IN_ACL; i++) {
        if (mtlk_osal_is_zero_address(nic->slow_ctx->acl[i].au8Addr))
          continue;
        memcpy(v[length++].sa_data, nic->slow_ctx->acl[i].au8Addr, sizeof(IEEE_ADDR));
        memcpy(v[length++].sa_data, nic->slow_ctx->acl_mask[i].au8Addr, sizeof(IEEE_ADDR));
      }
    }
    break;
  default:
    res = -EOPNOTSUPP;
    ILOG2(GID_IOCTL, "Unsupported subcmd: 0x%04x", subcmd);
    break;
  }

  wrqu->data.length = length;

  return res;
}

static int
mtlk_ioctl_set_text (struct net_device *dev,
                struct iw_request_info *info,
                union iwreq_data *wrqu,
                char *extra)
{
  struct nic *nic = netdev_priv(dev);
  uint32 subcmd;
  char *v;
  int length = wrqu->data.length;
  int res = 0;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  subcmd = wrqu->data.flags;
  v = extra;
  v[length - 1] = '\0'; /* force string null-termination */
  ILOG2(GID_IOCTL, "subcmd = 0x%04x", subcmd);

  switch (subcmd) {
  case MIB_COUNTRY:
    if (mtlk_core_get_net_state(nic) != NET_STATE_READY)
      return -EBUSY;
    if (nic->ap && mtlk_eeprom_get_country_code(&nic->slow_ctx->ee_data))
      return -EOPNOTSUPP;
    if (!nic->ap && nic->slow_ctx->cfg.dot11d)
      return -EOPNOTSUPP;
    if (strncmp(v, "??", 2) && country_to_domain(v) == 0)
      return -EINVAL;
    nic->slow_ctx->cfg.country_code = country_to_country_code(v);
    break;
  case PRM_ID_AOCS_RESTRICTED_CHANNELS:
    mtlk_aocs_set_restricted_ch(&nic->slow_ctx->aocs, v, length);
    break;
  case PRM_ID_AOCS_MSDU_TX_AC:
    if (mtlk_aocs_set_msdu_tx_ac(&nic->slow_ctx->aocs, v, length) != MTLK_ERR_OK) {
      res = -EINVAL;
      break;
    }
    break;
  case PRM_ID_AOCS_MSDU_RX_AC:
    if (mtlk_aocs_set_msdu_rx_ac(&nic->slow_ctx->aocs, v, length) != MTLK_ERR_OK) {
      res = -EINVAL;
      break;
    }
    break;
#ifdef AOCS_DEBUG_40MHZ_INT
  case PRM_ID_AOCS_DEBUG_40MHZ_INT:
    mtlk_aocs_debug_set_40mhz_int(&nic->slow_ctx->aocs, v, length);
    break;
#endif
  case PRM_ID_ACTIVE_SCAN_SSID:
    mtlk_scan_set_essid(&nic->slow_ctx->scan, v);
    break;
  case MIB_SUPPORTED_TX_ANTENNAS:
    mtlk_set_mib_value(PRM_TX_ANTENNAS, v, nic);
    break;
  case MIB_SUPPORTED_RX_ANTENNAS:
    mtlk_set_mib_value(PRM_RX_ANTENNAS, v, nic);
    break;
  case PRM_ID_HW_LIMIT:
    if (mtlk_set_hw_limit(&nic->slow_ctx->tx_limits, v) != MTLK_ERR_OK)
      res = EINVAL;
    break;
  case PRM_ID_ANT_GAIN:
    if (mtlk_set_ant_gain(&nic->slow_ctx->tx_limits, v) != MTLK_ERR_OK)
      res = EINVAL;
    break;
  case PRM_ID_LEGACY_FORCE_RATE:
    v[length] = 0; /* Protect from out-of-border memory access in sscanf() */
    if (set_force_rate(nic, MIB_LEGACY_FORCE_RATE, v) == MTLK_ERR_OK)
      return 0;
    return -EINVAL;
  case PRM_ID_HT_FORCE_RATE:
    v[length] = 0; /* Protect from out-of-border memory access in sscanf() */
    return set_force_rate(nic, MIB_HT_FORCE_RATE, v);
  default:
    res = -EOPNOTSUPP;
    ILOG2(GID_IOCTL, "Unsupported subcmd: 0x%04x", subcmd);
    break;
  };

  wrqu->data.length = length;

  return res;
}

static int
mtlk_ioctl_get_text (struct net_device *dev,
                struct iw_request_info *info,
                union iwreq_data *wrqu,
                char *extra)
{
  struct nic *nic = netdev_priv(dev);
  uint32 subcmd;
  char *v;
  int length = wrqu->data.length;
  int res = 0;

  ILOG3(GID_IOCTL, "%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  subcmd = wrqu->data.flags;
  v = extra;
  ILOG2(GID_IOCTL, "subcmd = 0x%04x", subcmd);

  switch (subcmd) {
  case PRM_ID_EEPROM:
    if (dev->flags & IFF_UP)
      sprintf(v, "EE data is not available since IF is up\n");
    else
      mtlk_get_eeprom_text_info(&nic->slow_ctx->ee_data, nic->slow_ctx->hw_cfg.txmm, v);
    length = strlen(v);
    break;
  case PRM_ID_11H_STATUS:
    strcpy(v, mtlk_dot11h_status(dev));
    length = strlen(v);
    break;
  case MIB_COUNTRY:
    strncpy(v, country_code_to_country(nic->slow_ctx->cfg.country_code), 2);
    length = 2;
    break;
  case PRM_ID_AOCS_RESTRICTED_CHANNELS:
    length = mtlk_aocs_get_restricted_ch(&nic->slow_ctx->aocs, v, TEXT_SIZE);
    break;
  case PRM_ID_AOCS_MSDU_TX_AC:
    length = mtlk_aocs_get_msdu_tx_ac(&nic->slow_ctx->aocs, v, TEXT_SIZE);
    break;
  case PRM_ID_AOCS_MSDU_RX_AC:
    length = mtlk_aocs_get_msdu_rx_ac(&nic->slow_ctx->aocs, v, TEXT_SIZE);
    break;
#ifdef AOCS_DEBUG_40MHZ_INT
  case PRM_ID_AOCS_DEBUG_40MHZ_INT:
    length = mtlk_aocs_debug_get_40mhz_int(&nic->slow_ctx->aocs, v, TEXT_SIZE);
    break;
#endif
  case PRM_ID_ACTIVE_SCAN_SSID:
    length = mtlk_scan_get_essid(&nic->slow_ctx->scan, v);
    break;
  case MIB_SUPPORTED_TX_ANTENNAS:
    strcpy(v, mtlk_get_mib_value(PRM_TX_ANTENNAS, nic));
    length = strlen(v);
    break;
  case MIB_SUPPORTED_RX_ANTENNAS:
    strcpy(v, mtlk_get_mib_value(PRM_RX_ANTENNAS, nic));
    length = strlen(v);
    break;
  case PRM_ID_LEGACY_FORCE_RATE:
    { 
      unsigned l = TEXT_SIZE;
      res = get_force_rate(v, &l, nic, MIB_LEGACY_FORCE_RATE);
      if (res == MTLK_ERR_OK)
        length = l;
    
      break;
    }
  case PRM_ID_HT_FORCE_RATE:
    {
      unsigned l = TEXT_SIZE;
      res = get_force_rate(v, &l, nic, MIB_HT_FORCE_RATE);
      if (res == MTLK_ERR_OK)
        length = l;

      break;
    }
  case PRM_ID_CORE_COUNTRIES_SUPPORTED:
    {
      unsigned l = TEXT_SIZE;
      res = get_supported_countries(v, &l, nic);
      if (res == MTLK_ERR_OK)
        length = l;

      break;
    }
#ifdef MTCFG_IRB_DEBUG
  case PRM_ID_IRB_PINGER_STATS: 
    {
      struct mtlk_irb_pinger_stats stats;
      uint64                       avg_delay_us = 0;

      mtlk_irb_pinger_get_stats(&nic->slow_ctx->pinger, &stats);

      if (stats.nof_recvd_pongs) {
        /* NOTE: 64-bit division is not supported by default in linux kernel space =>
         *       we should use the do_div() ASM macro here.
         */
        avg_delay_us = stats.all_delay_us;
        do_div(avg_delay_us, stats.nof_recvd_pongs); /* the result is stotred in avg_delay_us */
      }
      
      length = snprintf(v, TEXT_SIZE, "NofPongs=%u NofMissed=%u NofOOO=%u AllDly=%llu AvgDly=%llu PeakDly=%llu\n",
                        stats.nof_recvd_pongs,
                        stats.nof_missed_pongs,
                        stats.nof_ooo_pongs,
                        stats.all_delay_us,
                        avg_delay_us,
                        stats.peak_delay_us);
    }
    break;
#endif
  default:
    res = -EOPNOTSUPP;
    ILOG2(GID_IOCTL, "Unsupported subcmd: 0x%04x", subcmd);
    break;
  }

  wrqu->data.length = length;

  return res;
}

static const struct iw_priv_args mtlk_linux_privtab[] = {
  SET_ADDR    (SIOCIWFIRSTPRIV + 16                        , "sMAC"            )
  GET_ADDR    (SIOCIWFIRSTPRIV + 17                        , "gMAC"            )

  /* Sub-ioctl handlers */
  SET_INT     (SIOCIWFIRSTPRIV + 0                         , ""                )
  GET_INT     (SIOCIWFIRSTPRIV + 1                         , ""                )
  /* 2, 3 are reserved for WMCE */
  SET_INTVEC  (SIOCIWFIRSTPRIV + 4                         , ""                )
  GET_INTVEC  (SIOCIWFIRSTPRIV + 5                         , ""                )
  SET_ADDR    (SIOCIWFIRSTPRIV + 6                         , ""                )
  GET_ADDR    (SIOCIWFIRSTPRIV + 7                         , ""                )
  SET_TEXT    (SIOCIWFIRSTPRIV + 8                         , ""                )
  GET_TEXT    (SIOCIWFIRSTPRIV + 9                         , ""                )
  SET_ADDRVEC (SIOCIWFIRSTPRIV + 10                        , ""                )
  GET_ADDRVEC (SIOCIWFIRSTPRIV + 11                        , ""                )

  /* Sub-ioctls */
  SET_INT     (MIB_SHORT_RETRY_LIMIT                       , "sShortRetryLim"  )
  GET_INT     (MIB_SHORT_RETRY_LIMIT                       , "gShortRetryLim"  )
  SET_INT     (MIB_LONG_RETRY_LIMIT                        , "sLongRetryLimit" )
  GET_INT     (MIB_LONG_RETRY_LIMIT                        , "gLongRetryLimit" )
  SET_INT     (MIB_TX_MSDU_LIFETIME                        , "sMSDULifetime"   )
  GET_INT     (MIB_TX_MSDU_LIFETIME                        , "gMSDULifetime"   )
  SET_INT     (MIB_CURRENT_TX_ANTENNA                      , "sPrimaryAntenna" )
  GET_INT     (MIB_CURRENT_TX_ANTENNA                      , "gPrimaryAntenna" )
  SET_INT     (MIB_HIDDEN_SSID                             , "sHiddenSSID"     )
  GET_INT     (MIB_HIDDEN_SSID                             , "gHiddenSSID"     )
  SET_INT     (MIB_ADVANCED_CODING_SUPPORTED               , "sAdvancedCoding" )
  GET_INT     (MIB_ADVANCED_CODING_SUPPORTED               , "gAdvancedCoding" )
  SET_INT     (MIB_OVERLAPPING_PROTECTION_ENABLE           , "sBSSProtection"  )
  GET_INT     (MIB_OVERLAPPING_PROTECTION_ENABLE           , "gBSSProtection"  )
  SET_INT     (MIB_OFDM_PROTECTION_METHOD                  , "sERPProtection"  )
  GET_INT     (MIB_OFDM_PROTECTION_METHOD                  , "gERPProtection"  )
  SET_INT     (MIB_HT_PROTECTION_METHOD                    , "s11nProtection"  )
  GET_INT     (MIB_HT_PROTECTION_METHOD                    , "g11nProtection"  )
  SET_INT     (MIB_DTIM_PERIOD                             , "sDTIMPeriod"     )
  GET_INT     (MIB_DTIM_PERIOD                             , "gDTIMPeriod"     )
  SET_INT     (MIB_RECEIVE_AMPDU_MAX_LENGTH                , "sAMPDUMaxLength" )
  GET_INT     (MIB_RECEIVE_AMPDU_MAX_LENGTH                , "gAMPDUMaxLength" )
  SET_INT     (MIB_SM_ENABLE                               , "sChAnnounce"     )
  GET_INT     (MIB_SM_ENABLE                               , "gChAnnounce"     )
  SET_INT     (MIB_BEACON_PERIOD                           , "sBeaconPeriod"   )
  GET_INT     (MIB_BEACON_PERIOD                           , "gBeaconPeriod"   )
  SET_INT     (MIB_CB_DATABINS_PER_SYMBOL                  , "sDatabins"       )
  GET_INT     (MIB_CB_DATABINS_PER_SYMBOL                  , "gDatabins"       )
  SET_INT     (MIB_USE_LONG_PREAMBLE_FOR_MULTICAST         , "sLongPreambleMC" )
  GET_INT     (MIB_USE_LONG_PREAMBLE_FOR_MULTICAST         , "gLongPreambleMC" )
  SET_INT     (MIB_USE_SPACE_TIME_BLOCK_CODE               , "sSTBC"           )
  GET_INT     (MIB_USE_SPACE_TIME_BLOCK_CODE               , "gSTBC"           )
  SET_INT     (MIB_ONLINE_CALIBRATION_ALGO_MASK            , "sOnlineACM"      )
  GET_INT     (MIB_ONLINE_CALIBRATION_ALGO_MASK            , "gOnlineACM"      )

  SET_INT     (PRM_ID_BRIDGE_MODE                          , "sBridgeMode"     )
  GET_INT     (PRM_ID_BRIDGE_MODE                          , "gBridgeMode"     )
  SET_INT     (PRM_ID_L2NAT_AGING_TIMEOUT                  , "sL2NATAgeTO"     )
  GET_INT     (PRM_ID_L2NAT_AGING_TIMEOUT                  , "gL2NATAgeTO"     )

  SET_ADDR    (PRM_ID_L2NAT_DEFAULT_HOST                   , "sL2NATDefHost"   )
  GET_ADDR    (PRM_ID_L2NAT_DEFAULT_HOST                   , "gL2NATDefHost"   )
  SET_ADDR    (PRM_ID_STADB_LOCAL_MAC                      , "sL2NATLocMAC"    )
  GET_ADDR    (PRM_ID_STADB_LOCAL_MAC                      , "gL2NATLocMAC"    )

  SET_ADDR    (PRM_ID_ADD_PEERAP                           , "sAddPeerAP"      )
  SET_ADDR    (PRM_ID_DEL_PEERAP                           , "sDelPeerAP"      )
  SET_INT     (PRM_ID_PEERAP_KEY_IDX                       , "sPeerAPkeyIdx"   )
  GET_INT     (PRM_ID_PEERAP_KEY_IDX                       , "gPeerAPkeyIdx"   )

  SET_INT     (PRM_ID_AOCS_WEIGHT_CL                       , "sAocsWeightCl"   )
  GET_INT     (PRM_ID_AOCS_WEIGHT_CL                       , "gAocsWeightCl"   )
  SET_INT     (PRM_ID_AOCS_WEIGHT_TX                       , "sAocsWeightTx"   )
  GET_INT     (PRM_ID_AOCS_WEIGHT_TX                       , "gAocsWeightTx"   )
  SET_INT     (PRM_ID_AOCS_WEIGHT_BSS                      , "sAocsWeightBss"  )
  GET_INT     (PRM_ID_AOCS_WEIGHT_BSS                      , "gAocsWeightBss"  )
  SET_INT     (PRM_ID_AOCS_WEIGHT_SM                       , "sAocsWeightSm"   )
  GET_INT     (PRM_ID_AOCS_WEIGHT_SM                       , "gAocsWeightSm"   )
  SET_INT     (PRM_ID_AOCS_CFM_RANK_SW_THRESHOLD           , "sAocsCfmRnkThr"  )
  GET_INT     (PRM_ID_AOCS_CFM_RANK_SW_THRESHOLD           , "gAocsCfmRnkThr"  )
  SET_INT     (PRM_ID_AOCS_SCAN_AGING                      , "sAocsScanAging"  )
  GET_INT     (PRM_ID_AOCS_SCAN_AGING                      , "gAocsScanAging"  )
  SET_INT     (PRM_ID_AOCS_CONFIRM_RANK_AGING              , "sAocsCfmRAging"  )
  GET_INT     (PRM_ID_AOCS_CONFIRM_RANK_AGING              , "gAocsCfmRAging"  )
  SET_INT     (PRM_ID_AOCS_AFILTER                         , "sAocsAFilter"    )
  GET_INT     (PRM_ID_AOCS_AFILTER                         , "gAocsAFilter"    )
  SET_INT     (PRM_ID_AOCS_BONDING                         , "sAocsBonding"    )
  GET_INT     (PRM_ID_AOCS_BONDING                         , "gAocsBonding"    )
  SET_INT     (PRM_ID_AOCS_EN_PENALTIES                    , "sAocsEnPenalty"  )
  GET_INT     (PRM_ID_AOCS_EN_PENALTIES                    , "gAocsEnPenalty"  )
  SET_INT     (PRM_ID_AOCS_WIN_TIME                        , "sAocsWinTime"    )
  GET_INT     (PRM_ID_AOCS_WIN_TIME                        , "gAocsWinTime"    )
  SET_INT     (PRM_ID_AOCS_MSDU_DEBUG_ENABLED              , "sAocsEnMsduDbg"  )
  GET_INT     (PRM_ID_AOCS_MSDU_DEBUG_ENABLED              , "gAocsEnMsduDbg"  )
  SET_INT     (PRM_ID_AOCS_IS_ENABLED                      , "sAocsIsEnabled"  )
  GET_INT     (PRM_ID_AOCS_IS_ENABLED                      , "gAocsIsEnabled"  )
  SET_INT     (PRM_ID_AOCS_MSDU_THRESHOLD                  , "sAocsMsduThr"    )
  GET_INT     (PRM_ID_AOCS_MSDU_THRESHOLD                  , "gAocsMsduThr"    )
  SET_INT     (PRM_ID_AOCS_LOWER_THRESHOLD                 , "sAocsLwrThr"     )
  GET_INT     (PRM_ID_AOCS_LOWER_THRESHOLD                 , "gAocsLwrThr"     )
  SET_INT     (PRM_ID_AOCS_THRESHOLD_WINDOW                , "sAocsThrWindow"  )
  GET_INT     (PRM_ID_AOCS_THRESHOLD_WINDOW                , "gAocsThrWindow"  )
  SET_INT     (PRM_ID_AOCS_MSDU_PER_WIN_THRESHOLD          , "sAocsMsduWinThr" )
  GET_INT     (PRM_ID_AOCS_MSDU_PER_WIN_THRESHOLD          , "gAocsMsduWinThr" )
  SET_INTVEC  (PRM_ID_AOCS_PENALTIES                       , "sAocsPenalty"    )
  SET_TEXT    (PRM_ID_AOCS_RESTRICTED_CHANNELS             , "sAocsRestrictCh" )
  GET_TEXT    (PRM_ID_AOCS_RESTRICTED_CHANNELS             , "gAocsRestrictCh" )
  SET_TEXT    (PRM_ID_AOCS_MSDU_TX_AC                      , "sAocsMsduTxAc"   )
  GET_TEXT    (PRM_ID_AOCS_MSDU_TX_AC                      , "gAocsMsduTxAc"   )
  SET_TEXT    (PRM_ID_AOCS_MSDU_RX_AC                      , "sAocsMsduRxAc"   )
  GET_TEXT    (PRM_ID_AOCS_MSDU_RX_AC                      , "gAocsMsduRxAc"   )
#ifdef AOCS_DEBUG_40MHZ_INT
  SET_TEXT    (PRM_ID_AOCS_DEBUG_40MHZ_INT                 , "sAocsDbg40MHzInt")
  GET_TEXT    (PRM_ID_AOCS_DEBUG_40MHZ_INT                 , "sAocsDbg40MHzInt")
#endif
  SET_INT     (PRM_ID_AOCS_MEASUREMENT_WINDOW              , "sAocsMeasurWnd"  )
  GET_INT     (PRM_ID_AOCS_MEASUREMENT_WINDOW              , "gAocsMeasurWnd"  )
  SET_INT     (PRM_ID_AOCS_THROUGHPUT_THRESHOLD            , "sAocsThrThr"     )
  GET_INT     (PRM_ID_AOCS_THROUGHPUT_THRESHOLD            , "gAocsThrThr"     )

  SET_INT     (PRM_ID_PACK_SCHED_ENABLED                   , "sSQEnabled"      )
  GET_INT     (PRM_ID_PACK_SCHED_ENABLED                   , "gSQEnabled"      )

  SET_INTVEC  (PRM_ID_SQ_LIMITS                            , "sSQLimits"       )
  GET_INTVEC  (PRM_ID_SQ_LIMITS                            , "gSQLimits"       )
  SET_INTVEC  (PRM_ID_SQ_PEER_LIMITS                       , "sSQPeerLimits"   )
  GET_INTVEC  (PRM_ID_SQ_PEER_LIMITS                       , "gSQPeerLimits"   )

  SET_TEXT    (PRM_ID_ACTIVE_SCAN_SSID                     , "sActiveScanSSID" )
  GET_TEXT    (PRM_ID_ACTIVE_SCAN_SSID                     , "gActiveScanSSID" )
  SET_INT     (PRM_ID_SCAN_CACHE_LIFETIME                  , "sScanCacheLifeT" )
  GET_INT     (PRM_ID_SCAN_CACHE_LIFETIME                  , "gScanCacheLifeT" )

  SET_INT     (MIB_CALIBRATION_ALGO_MASK                   , "sAlgoCalibrMask" )
  GET_INT     (MIB_CALIBRATION_ALGO_MASK                   , "gAlgoCalibrMask" )
  SET_INT     (PRM_ID_STA_KEEPALIVE_TIMEOUT                , "sStaKeepaliveTO" )
  GET_INT     (PRM_ID_STA_KEEPALIVE_TIMEOUT                , "gStaKeepaliveTO" )
  SET_INT     (PRM_ID_STA_KEEPALIVE_INTERVAL               , "sStaKeepaliveIN" )
  GET_INT     (PRM_ID_STA_KEEPALIVE_INTERVAL               , "gStaKeepaliveIN" )
  SET_INT     (MIB_POWER_INCREASE_VS_DUTY_CYCLE            , "sPwrVsDutyCycle" )
  GET_INT     (MIB_POWER_INCREASE_VS_DUTY_CYCLE            , "gPwrVsDutyCycle" )
  SET_INT     (MIB_USE_SHORT_CYCLIC_PREFIX                 , "sShortCyclcPrfx" )
  GET_INT     (MIB_USE_SHORT_CYCLIC_PREFIX                 , "gShortCyclcPrfx" )
  SET_INT     (MIB_SHORT_PREAMBLE_OPTION_IMPLEMENTED       , "sShortPreamble"  )
  GET_INT     (MIB_SHORT_PREAMBLE_OPTION_IMPLEMENTED       , "gShortPreamble"  )
  SET_INT     (MIB_SHORT_SLOT_TIME_OPTION_ENABLED_11G      , "sShortSlotTime"  )
  GET_INT     (MIB_SHORT_SLOT_TIME_OPTION_ENABLED_11G      , "gShortSlotTime"  )
  SET_INT     (PRM_ID_WDS_HOST_TIMEOUT                     , "sWDSHostTO"      )
  GET_INT     (PRM_ID_WDS_HOST_TIMEOUT                     , "gWDSHostTO"      )

  SET_INT     (PRM_DBG_SW_WD_ENABLE                        , "sSwWdEnabled"    )
  GET_INT     (PRM_DBG_SW_WD_ENABLE                        , "gSwWdEnabled"    )

  SET_INT     (PRM_ID_RELIABLE_MULTICAST                   , "sReliableMcast"  )
  GET_INT     (PRM_ID_RELIABLE_MULTICAST                   , "gReliableMcast"  )
  SET_INT     (PRM_ID_AP_FORWARDING                        , "sAPforwarding"   )
  GET_INT     (PRM_ID_AP_FORWARDING                        , "gAPforwarding"   )
  SET_TEXT    (MIB_SUPPORTED_TX_ANTENNAS                   , "sTxAntennas"     )
  GET_TEXT    (MIB_SUPPORTED_TX_ANTENNAS                   , "gTxAntennas"     )
  SET_TEXT    (MIB_SUPPORTED_RX_ANTENNAS                   , "sRxAntennas"     )
  GET_TEXT    (MIB_SUPPORTED_RX_ANTENNAS                   , "gRxAntennas"     )
  SET_INT     (PRM_ID_SPECTRUM_MODE                        , "sFortyMHzOpMode" )
  GET_INT     (PRM_ID_SPECTRUM_MODE                        , "gFortyMHzOpMode" )
  SET_INT     (PRM_ID_POWER_SELECTION                      , "sPowerSelection" )
  GET_INT     (PRM_ID_POWER_SELECTION                      , "gPowerSelection" )
  SET_INT     (PRM_ID_11D                                  , "s11dActive"      )
  GET_INT     (PRM_ID_11D                                  , "g11dActive"      )
  SET_INT     (PRM_ID_RADAR_DETECTION                      , "s11hRadarDetect" )
  GET_INT     (PRM_ID_RADAR_DETECTION                      , "g11hRadarDetect" )
  SET_INT     (PRM_ID_11H_ENABLE_SM_CHANNELS               , "s11hEnSMChnls"   )
  GET_INT     (PRM_ID_11H_ENABLE_SM_CHANNELS               , "g11hEnSMChnls"   )
  SET_TEXT    (MIB_COUNTRY                                 , "sCountry"        )
  GET_TEXT    (MIB_COUNTRY                                 , "gCountry"        )

  SET_INT     (MIB_ACL_MODE                                , "sAclMode"        )
  GET_INT     (MIB_ACL_MODE                                , "gAclMode"        )
  SET_INT     (MIB_ACL_MAX_CONNECTIONS                     , "sMaxConnections" )
  GET_INT     (MIB_ACL_MAX_CONNECTIONS                     , "gMaxConnections" )
  SET_ADDRVEC (PRM_ID_ACL                                  , "sACL"            )
  SET_ADDRVEC (PRM_ID_ACL_RANGE                            , "sACLRange"       )
  SET_ADDRVEC (PRM_ID_ACL_DEL                              , "sDelACL"         )
  GET_ADDRVEC (PRM_ID_ACL                                  , "gACL"            )
  GET_ADDRVEC (PRM_ID_ACL_RANGE                            , "gACLRange"       )

  SET_INT     (PRM_ID_11D_RESTORE_DEFAULTS                 , "s11dReset"       )

  SET_INT     (PRM_ID_11H_BEACON_COUNT                     , "s11hBeaconCount" )
  GET_INT     (PRM_ID_11H_BEACON_COUNT                     , "g11hBeaconCount" )
  SET_INT     (PRM_ID_11H_CHANNEL_AVAILABILITY_CHECK_TIME  , "s11hChCheckTime" )
  GET_INT     (PRM_ID_11H_CHANNEL_AVAILABILITY_CHECK_TIME  , "g11hChCheckTime" )

  SET_INT     (PRM_ID_11H_EMULATE_RADAR_DETECTION          , "s11hEmulatRadar" )
  SET_INT     (PRM_ID_11H_SWITCH_CHANNEL                   , "s11hSwitchChnnl" )
  SET_INT     (PRM_ID_11H_NEXT_CHANNEL                     , "s11hNextChannel" )
  GET_INT     (PRM_ID_11H_NEXT_CHANNEL                     , "g11hNextChannel" )
  GET_TEXT    (PRM_ID_11H_STATUS                           , "g11hStatus"      )

  GET_TEXT    (PRM_ID_EEPROM                               , "gEEPROM"         )

  SET_INT     (PRM_ID_BE_BAUSE                             , "sBE.BAUse"       )
  GET_INT     (PRM_ID_BE_BAUSE                             , "gBE.BAUse"       )
  SET_INT     (PRM_ID_BE_BAACCEPT                          , "sBE.BAAccept"    )
  GET_INT     (PRM_ID_BE_BAACCEPT                          , "gBE.BAAccept"    )
  SET_INT     (PRM_ID_BE_BATIMEOUT                         , "sBE.BATimeout"   )
  GET_INT     (PRM_ID_BE_BATIMEOUT                         , "gBE.BATimeout"   )
  SET_INT     (PRM_ID_BE_BAWINSIZE                         , "sBE.BAWinSize"   )
  GET_INT     (PRM_ID_BE_BAWINSIZE                         , "gBE.BAWinSize"   )
  SET_INT     (PRM_ID_BE_AGGRMAXBTS                        , "sBE.AggrMaxBts"  )
  GET_INT     (PRM_ID_BE_AGGRMAXBTS                        , "gBE.AggrMaxBts"  )
  SET_INT     (PRM_ID_BE_AGGRMAXPKTS                       , "sBE.AggrMaxPkts" )
  GET_INT     (PRM_ID_BE_AGGRMAXPKTS                       , "gBE.AggrMaxPkts" )
  SET_INT     (PRM_ID_BE_AGGRMINPTSZ                       , "sBE.AggrMinPtSz" )
  GET_INT     (PRM_ID_BE_AGGRMINPTSZ                       , "gBE.AggrMinPtSz" )
  SET_INT     (PRM_ID_BE_AGGRTIMEOUT                       , "sBE.AggrTimeout" )
  GET_INT     (PRM_ID_BE_AGGRTIMEOUT                       , "gBE.AggrTimeout" )
  SET_INT     (PRM_ID_BE_AIFSN                             , "sBE.AIFSN"       )
  GET_INT     (PRM_ID_BE_AIFSN                             , "gBE.AIFSN"       )
  SET_INT     (PRM_ID_BE_AIFSNAP                           , "sBE.AIFSNAP"     )
  GET_INT     (PRM_ID_BE_AIFSNAP                           , "gBE.AIFSNAP"     )
  SET_INT     (PRM_ID_BE_CWMAX                             , "sBE.CWMax"       )
  GET_INT     (PRM_ID_BE_CWMAX                             , "gBE.CWMax"       )
  SET_INT     (PRM_ID_BE_CWMAXAP                           , "sBE.CWMaxAP"     )
  GET_INT     (PRM_ID_BE_CWMAXAP                           , "gBE.CWMaxAP"     )
  SET_INT     (PRM_ID_BE_CWMIN                             , "sBE.CWMin"       )
  GET_INT     (PRM_ID_BE_CWMIN                             , "gBE.CWMin"       )
  SET_INT     (PRM_ID_BE_CWMINAP                           , "sBE.CWMinAP"     )
  GET_INT     (PRM_ID_BE_CWMINAP                           , "gBE.CWMinAP"     )
  SET_INT     (PRM_ID_BE_TXOP                              , "sBE.TXOP"        )
  GET_INT     (PRM_ID_BE_TXOP                              , "gBE.TXOP"        )
  SET_INT     (PRM_ID_BE_TXOPAP                            , "sBE.TXOPAP"      )
  GET_INT     (PRM_ID_BE_TXOPAP                            , "gBE.TXOPAP"      )

  SET_INT     (PRM_ID_BK_BAUSE                             , "sBK.BAUse"       )
  GET_INT     (PRM_ID_BK_BAUSE                             , "gBK.BAUse"       )
  SET_INT     (PRM_ID_BK_BAACCEPT                          , "sBK.BAAccept"    )
  GET_INT     (PRM_ID_BK_BAACCEPT                          , "gBK.BAAccept"    )
  SET_INT     (PRM_ID_BK_BATIMEOUT                         , "sBK.BATimeout"   )
  GET_INT     (PRM_ID_BK_BATIMEOUT                         , "gBK.BATimeout"   )
  SET_INT     (PRM_ID_BK_BAWINSIZE                         , "sBK.BAWinSize"   )
  GET_INT     (PRM_ID_BK_BAWINSIZE                         , "gBK.BAWinSize"   )
  SET_INT     (PRM_ID_BK_AGGRMAXBTS                        , "sBK.AggrMaxBts"  )
  GET_INT     (PRM_ID_BK_AGGRMAXBTS                        , "gBK.AggrMaxBts"  )
  SET_INT     (PRM_ID_BK_AGGRMAXPKTS                       , "sBK.AggrMaxPkts" )
  GET_INT     (PRM_ID_BK_AGGRMAXPKTS                       , "gBK.AggrMaxPkts" )
  SET_INT     (PRM_ID_BK_AGGRMINPTSZ                       , "sBK.AggrMinPtSz" )
  GET_INT     (PRM_ID_BK_AGGRMINPTSZ                       , "gBK.AggrMinPtSz" )
  SET_INT     (PRM_ID_BK_AGGRTIMEOUT                       , "sBK.AggrTimeout" )
  GET_INT     (PRM_ID_BK_AGGRTIMEOUT                       , "gBK.AggrTimeout" )
  SET_INT     (PRM_ID_BK_AIFSN                             , "sBK.AIFSN"       )
  GET_INT     (PRM_ID_BK_AIFSN                             , "gBK.AIFSN"       )
  SET_INT     (PRM_ID_BK_AIFSNAP                           , "sBK.AIFSNAP"     )
  GET_INT     (PRM_ID_BK_AIFSNAP                           , "gBK.AIFSNAP"     )
  SET_INT     (PRM_ID_BK_CWMAX                             , "sBK.CWMax"       )
  GET_INT     (PRM_ID_BK_CWMAX                             , "gBK.CWMax"       )
  SET_INT     (PRM_ID_BK_CWMAXAP                           , "sBK.CWMaxAP"     )
  GET_INT     (PRM_ID_BK_CWMAXAP                           , "gBK.CWMaxAP"     )
  SET_INT     (PRM_ID_BK_CWMIN                             , "sBK.CWMin"       )
  GET_INT     (PRM_ID_BK_CWMIN                             , "gBK.CWMin"       )
  SET_INT     (PRM_ID_BK_CWMINAP                           , "sBK.CWMinAP"     )
  GET_INT     (PRM_ID_BK_CWMINAP                           , "gBK.CWMinAP"     )
  SET_INT     (PRM_ID_BK_TXOP                              , "sBK.TXOP"        )
  GET_INT     (PRM_ID_BK_TXOP                              , "gBK.TXOP"        )
  SET_INT     (PRM_ID_BK_TXOPAP                            , "sBK.TXOPAP"      )
  GET_INT     (PRM_ID_BK_TXOPAP                            , "gBK.TXOPAP"      )

  SET_INT     (PRM_ID_VI_BAUSE                             , "sVI.BAUse"       )
  GET_INT     (PRM_ID_VI_BAUSE                             , "gVI.BAUse"       )
  SET_INT     (PRM_ID_VI_BAACCEPT                          , "sVI.BAAccept"    )
  GET_INT     (PRM_ID_VI_BAACCEPT                          , "gVI.BAAccept"    )
  SET_INT     (PRM_ID_VI_BATIMEOUT                         , "sVI.BATimeout"   )
  GET_INT     (PRM_ID_VI_BATIMEOUT                         , "gVI.BATimeout"   )
  SET_INT     (PRM_ID_VI_BAWINSIZE                         , "sVI.BAWinSize"   )
  GET_INT     (PRM_ID_VI_BAWINSIZE                         , "gVI.BAWinSize"   )
  SET_INT     (PRM_ID_VI_AGGRMAXBTS                        , "sVI.AggrMaxBts"  )
  GET_INT     (PRM_ID_VI_AGGRMAXBTS                        , "gVI.AggrMaxBts"  )
  SET_INT     (PRM_ID_VI_AGGRMAXPKTS                       , "sVI.AggrMaxPkts" )
  GET_INT     (PRM_ID_VI_AGGRMAXPKTS                       , "gVI.AggrMaxPkts" )
  SET_INT     (PRM_ID_VI_AGGRMINPTSZ                       , "sVI.AggrMinPtSz" )
  GET_INT     (PRM_ID_VI_AGGRMINPTSZ                       , "gVI.AggrMinPtSz" )
  SET_INT     (PRM_ID_VI_AGGRTIMEOUT                       , "sVI.AggrTimeout" )
  GET_INT     (PRM_ID_VI_AGGRTIMEOUT                       , "gVI.AggrTimeout" )
  SET_INT     (PRM_ID_VI_AIFSN                             , "sVI.AIFSN"       )
  GET_INT     (PRM_ID_VI_AIFSN                             , "gVI.AIFSN"       )
  SET_INT     (PRM_ID_VI_AIFSNAP                           , "sVI.AIFSNAP"     )
  GET_INT     (PRM_ID_VI_AIFSNAP                           , "gVI.AIFSNAP"     )
  SET_INT     (PRM_ID_VI_CWMAX                             , "sVI.CWMax"       )
  GET_INT     (PRM_ID_VI_CWMAX                             , "gVI.CWMax"       )
  SET_INT     (PRM_ID_VI_CWMAXAP                           , "sVI.CWMaxAP"     )
  GET_INT     (PRM_ID_VI_CWMAXAP                           , "gVI.CWMaxAP"     )
  SET_INT     (PRM_ID_VI_CWMIN                             , "sVI.CWMin"       )
  GET_INT     (PRM_ID_VI_CWMIN                             , "gVI.CWMin"       )
  SET_INT     (PRM_ID_VI_CWMINAP                           , "sVI.CWMinAP"     )
  GET_INT     (PRM_ID_VI_CWMINAP                           , "gVI.CWMinAP"     )
  SET_INT     (PRM_ID_VI_TXOP                              , "sVI.TXOP"        )
  GET_INT     (PRM_ID_VI_TXOP                              , "gVI.TXOP"        )
  SET_INT     (PRM_ID_VI_TXOPAP                            , "sVI.TXOPAP"      )
  GET_INT     (PRM_ID_VI_TXOPAP                            , "gVI.TXOPAP"      )

  SET_INT     (PRM_ID_VO_BAUSE                             , "sVO.BAUse"       )
  GET_INT     (PRM_ID_VO_BAUSE                             , "gVO.BAUse"       )
  SET_INT     (PRM_ID_VO_BAACCEPT                          , "sVO.BAAccept"    )
  GET_INT     (PRM_ID_VO_BAACCEPT                          , "gVO.BAAccept"    )
  SET_INT     (PRM_ID_VO_BATIMEOUT                         , "sVO.BATimeout"   )
  GET_INT     (PRM_ID_VO_BATIMEOUT                         , "gVO.BATimeout"   )
  SET_INT     (PRM_ID_VO_BAWINSIZE                         , "sVO.BAWinSize"   )
  GET_INT     (PRM_ID_VO_BAWINSIZE                         , "gVO.BAWinSize"   )
  SET_INT     (PRM_ID_VO_AGGRMAXBTS                        , "sVO.AggrMaxBts"  )
  GET_INT     (PRM_ID_VO_AGGRMAXBTS                        , "gVO.AggrMaxBts"  )
  SET_INT     (PRM_ID_VO_AGGRMAXPKTS                       , "sVO.AggrMaxPkts" )
  GET_INT     (PRM_ID_VO_AGGRMAXPKTS                       , "gVO.AggrMaxPkts" )
  SET_INT     (PRM_ID_VO_AGGRMINPTSZ                       , "sVO.AggrMinPtSz" )
  GET_INT     (PRM_ID_VO_AGGRMINPTSZ                       , "gVO.AggrMinPtSz" )
  SET_INT     (PRM_ID_VO_AGGRTIMEOUT                       , "sVO.AggrTimeout" )
  GET_INT     (PRM_ID_VO_AGGRTIMEOUT                       , "gVO.AggrTimeout" )
  SET_INT     (PRM_ID_VO_AIFSN                             , "sVO.AIFSN"       )
  GET_INT     (PRM_ID_VO_AIFSN                             , "gVO.AIFSN"       )
  SET_INT     (PRM_ID_VO_AIFSNAP                           , "sVO.AIFSNAP"     )
  GET_INT     (PRM_ID_VO_AIFSNAP                           , "gVO.AIFSNAP"     )
  SET_INT     (PRM_ID_VO_CWMAX                             , "sVO.CWMax"       )
  GET_INT     (PRM_ID_VO_CWMAX                             , "gVO.CWMax"       )
  SET_INT     (PRM_ID_VO_CWMAXAP                           , "sVO.CWMaxAP"     )
  GET_INT     (PRM_ID_VO_CWMAXAP                           , "gVO.CWMaxAP"     )
  SET_INT     (PRM_ID_VO_CWMIN                             , "sVO.CWMin"       )
  GET_INT     (PRM_ID_VO_CWMIN                             , "gVO.CWMin"       )
  SET_INT     (PRM_ID_VO_CWMINAP                           , "sVO.CWMinAP"     )
  GET_INT     (PRM_ID_VO_CWMINAP                           , "gVO.CWMinAP"     )
  SET_INT     (PRM_ID_VO_TXOP                              , "sVO.TXOP"        )
  GET_INT     (PRM_ID_VO_TXOP                              , "gVO.TXOP"        )
  SET_INT     (PRM_ID_VO_TXOPAP                            , "sVO.TXOPAP"      )
  GET_INT     (PRM_ID_VO_TXOPAP                            , "gVO.TXOPAP"      )

  SET_TEXT    (PRM_ID_HW_LIMIT                             , "sHWLimit"        )
  SET_TEXT    (PRM_ID_ANT_GAIN                             , "sAntGain"        )

  SET_INTVEC  (SIOCIWFIRSTPRIV + 30                        , "sMacGpio"        )
  GET_TEXT    (SIOCIWFIRSTPRIV + 31                        , "gConInfo"        )

  /* backward compatibility - scheduled for removal in 2.3.15 */
  SET_INT     (MIB_DISCONNECT_ON_NACKS_ENABLE              , "sDisconnOnNACKs" ) /* alias */
  GET_INT     (MIB_DISCONNECT_ON_NACKS_ENABLE              , "gDisconnOnNACKs" ) /* alias */

  SET_INT     (MIB_DISCONNECT_ON_NACKS_ENABLE              , "sDiscNACKEnable" )
  GET_INT     (MIB_DISCONNECT_ON_NACKS_ENABLE              , "gDiscNACKEnable" )
  SET_INT     (MIB_DISCONNECT_ON_NACKS_WEIGHT              , "sDiscNACKWeight" )
  GET_INT     (MIB_DISCONNECT_ON_NACKS_WEIGHT              , "gDiscNACKWeight" )

  SET_INT     (MIB_BSS_BASIC_RATE_SET                      , "sBasicRateSet"   )
  GET_INT     (MIB_BSS_BASIC_RATE_SET                      , "gBasicRateSet"   )
  SET_INT     (PRM_ID_AOCS_NON_OCCUPANCY_PERIOD            , "sNonOccupatePrd" )
  GET_INT     (PRM_ID_AOCS_NON_OCCUPANCY_PERIOD            , "gNonOccupatePrd" )
  SET_INT     (PRM_ID_USE_8021Q                            , "sUse1QMap"       )
  GET_INT     (PRM_ID_USE_8021Q                            , "gUse1QMap"       )

  SET_TEXT    (PRM_ID_LEGACY_FORCE_RATE                    , "sFixedRate"      )
  GET_TEXT    (PRM_ID_LEGACY_FORCE_RATE                    , "gFixedRate"      )
  SET_TEXT    (PRM_ID_HT_FORCE_RATE                        , "sFixedHTRate"    )
  GET_TEXT    (PRM_ID_HT_FORCE_RATE                        , "gFixedHTRate"    )
  GET_TEXT    (PRM_ID_CORE_COUNTRIES_SUPPORTED             , "gCountries"      )
  SET_INT     (PRM_ID_NETWORK_MODE                         , "sNetworkMode"    )
  GET_INT     (PRM_ID_NETWORK_MODE                         , "gNetworkMode"    )
  SET_INT     (PRM_ID_IS_BACKGROUND_SCAN                   , "sIsBGScan"       )
  GET_INT     (PRM_ID_IS_BACKGROUND_SCAN                   , "gIsBGScan"       )
  SET_INT     (PRM_ID_BG_SCAN_CH_LIMIT                     , "sBGScanChLimit"  )
  GET_INT     (PRM_ID_BG_SCAN_CH_LIMIT                     , "gBGScanChLimit"  )
  SET_INT     (PRM_ID_BG_SCAN_PAUSE                        , "sBGScanPause"    )
  GET_INT     (PRM_ID_BG_SCAN_PAUSE                        , "gBGScanPause"    )

  SET_INT     (PRM_ID_MAC_WATCHDOG_TIMEOUT_MS              , "sMACWdTimeoutMs" )
  GET_INT     (PRM_ID_MAC_WATCHDOG_TIMEOUT_MS              , "gMACWdTimeoutMs" )
  SET_INT     (PRM_ID_MAC_WATCHDOG_PERIOD_MS               , "sMACWdPeriodMs"  )
  GET_INT     (PRM_ID_MAC_WATCHDOG_PERIOD_MS               , "gMACWdPeriodMs"  )

#ifdef MTCFG_IRB_DEBUG
  SET_INT     (PRM_ID_IRB_PINGER_ENABLED                   , "sIRBPngEnabled"  )
  GET_INT     (PRM_ID_IRB_PINGER_ENABLED                   , "gIRBPngEnabled"  )

  SET_INT     (PRM_ID_IRB_PINGER_STATS                     , "sIRBPngStatsRst" )
  GET_TEXT    (PRM_ID_IRB_PINGER_STATS                     , "gIRBPngStats"    )
#endif
#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
  SET_INT     (PRM_ID_PPA_API_DIRECTPATH                   , "sIpxPpaEnabled"  )
  GET_INT     (PRM_ID_PPA_API_DIRECTPATH                   , "gIpxPpaEnabled"  )
#endif
  SET_INT     (PRM_ID_COC_LOW_POWER_MODE                   , "sCoCLowPower"    )
  GET_INT     (PRM_ID_COC_LOW_POWER_MODE                   , "gCoCLowPower"    )
  SET_INT     (PRM_CHANGE_TX_POWER_LIMIT                   , "sTxPowerLimOpt"  )
  GET_INT     (PRM_CHANGE_TX_POWER_LIMIT                   , "gTxPowerLimOpt"  )
  SET_INT     (PRM_ID_AGGR_OPEN_THRESHOLD                  , "sAggrOpenThrsh"  )
  GET_INT     (PRM_ID_AGGR_OPEN_THRESHOLD                  , "gAggrOpenThrsh"  )

  /* Restart FW in case of unconfirmed disconnect request */
  SET_INT     (PRM_ID_IS_HALT_FW_ON_DISC_TIMEOUT           , "sHaltFwDiscTm"   )
  GET_INT     (PRM_ID_IS_HALT_FW_ON_DISC_TIMEOUT           , "gHaltFwDiscTm"   )
};

static const iw_handler mtlk_linux_private_handler[] = {
  [ 0] = mtlk_ioctl_set_int,
  [ 1] = mtlk_ioctl_get_int,
  [ 3] = mtlk_ioctl_get_wmce_stats,
  [ 4] = mtlk_ioctl_set_intvec,
  [ 5] = mtlk_ioctl_get_intvec,
  [ 6] = mtlk_ioctl_set_addr,
  [ 7] = mtlk_ioctl_get_addr,
  [ 8] = mtlk_ioctl_set_text,
  [ 9] = mtlk_ioctl_get_text,
  [10] = mtlk_ioctl_set_addrvec,
  [11] = mtlk_ioctl_get_addrvec,
  [16] = mtlk_ioctl_set_mac_addr,
  [17] = mtlk_ioctl_get_mac_addr,
  [20] = iw_bcl_mac_data_get,
  [21] = iw_bcl_mac_data_set,
  [22] = iw_bcl_drv_data_exchange,
  [26] = iw_stop_lower_mac,
  [27] = iw_mac_calibrate,
  [28] = iw_generic,
  [30] = mtlk_ctrl_mac_gpio,
  [31] = iw_handler_get_connection_info,
};

const struct iw_handler_def mtlk_linux_handler_def = {
  .num_standard = ARRAY_SIZE(mtlk_linux_handler),
  .num_private = ARRAY_SIZE(mtlk_linux_private_handler),
  .num_private_args = ARRAY_SIZE(mtlk_linux_privtab),
  .standard = (iw_handler *) mtlk_linux_handler,
  .private = (iw_handler *) mtlk_linux_private_handler,
  .private_args = (struct iw_priv_args *) mtlk_linux_privtab,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  .get_wireless_stats = mtlk_linux_get_iw_stats,
#endif
};

