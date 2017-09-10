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
 * 802.11n frame processing routines.
 *
 * Originaly written by Artem Migaev
 *
 */
#include "mtlkinc.h"
#include "scan.h"
#include "frame.h"
#include "mtlkaux.h"
#include "channels.h"
#include "mtlk_core_iface.h"
#include "aocs.h"

/*!
	\fn	ie_extract_phy_rates()
	\brief	Process Rates informational elements
	\param	ie_data Pointer to IE
	\param	length IE length
	\param	bss_data Pointer to BSS description structure.
  
	'Supported Rates' and 'Extended Supported Rates' IEs are parsed
	by this function and filled to the supplied bss_data structure.
	All known rates are extracted, unsopported silently ignored.
*/
static __INLINE int
ie_extract_phy_rates (uint8      *ie_data,
                      int32       length,
                      bss_data_t *bss_data)
{
  int res = MTLK_ERR_OK;

  while (length) {
    uint32 rate = 0;
    uint8  val  = *ie_data;
    switch (val & 0x7F) {
    case 6*2: /* 11a rate 0 6mbps */
      rate = BASIC_RATE_SET_11A_6MBPS;
      break;

    case 9*2: /* 11a rate 1 9mbps */
      rate = BASIC_RATE_SET_11A_9MBPS;
      break;

    case 12*2: /* 11a rate 2 12mbps */
      rate = BASIC_RATE_SET_11A_12MBPS;
      break;

    case 18*2: /* 11a rate 3 18mbps */
      rate = BASIC_RATE_SET_11A_18MBPS;
      break;

    case 24*2: /* 11a rate 4 24mbps */
      rate = BASIC_RATE_SET_11A_24MBPS;
      break;

    case 36*2: /* 11a rate 5 36mbps */
      rate = BASIC_RATE_SET_11A_36MBPS;
      break;

    case 48*2: /* 11a rate 6 48mbps */
      rate = BASIC_RATE_SET_11A_48MBPS;
      break;

    case 54*2: /* 11a rate 7 54mbps */
      rate = BASIC_RATE_SET_11A_54MBPS;
      break;

    case 1*2: /* 11b rate 11 1mbps-long-preamble */
      rate = BASIC_RATE_SET_11B_1MBPS_LONG;
      break;

    case 2*2: /* 11b rate 8+12 2mbps-short+long */
      rate = BASIC_RATE_SET_11B_2MBPS_SHORT_LONG;
      break;

    case 11: /* (5.5*2) 11b rate 9+13 5.5mbps-short+long */
      rate = BASIC_RATE_SET_11B_5DOT5MBPS_SHORT_LONG;
      break;

    case 11*2: /* 11b rate 10+14 11mbps-short+long */
      rate = BASIC_RATE_SET_11B_11MBPS_SHORT_LONG;
      break;

    default: /* no other rates are allowed */
      ILOG2(GID_FRAME, "Unexpected PHY rate: 0x%02X", (int)val);
      res = MTLK_ERR_PARAMS;
      break;
    };

    bss_data->operational_rate_set |= rate;
    if (val & 0x80)
      bss_data->basic_rate_set |= rate;

    length--;
    ie_data++;
  }

  if (bss_data->operational_rate_set & BASIC_RATE_SET_2DOT4_OPERATIONAL_RATE)
    bss_data->is_2_4 = 1;

  return res;
}



/*!
	\fn	ie_extract_ht_info()
	\brief	Process HT informational element
	\param	ie_data Pointer to IE
	\param	lenfth IE length
	\param	bss_data Pointer to BSS description structure.

	'HT' IE parsed by this function and filled to the supplied 
	bss_data structure. CB and Spectrum information extracted.
*/
static __INLINE int
ie_extract_ht_info (uint8      *ie_data,
                    int32       length,
                    bss_data_t *bss_data)
{
  int res = MTLK_ERR_PARAMS;

  if (length < 2) {
    ELOG("Wrong HT info length: %d", (int)length);
    goto FINISH;
  }

  bss_data->channel = ie_data[0];
  ILOG4(GID_FRAME, "HT info channel is %u", bss_data->channel);

  switch (ie_data[1] & 0x07) {
  case 7: /* 40 lower */
    bss_data->spectrum    = 1;
    bss_data->upper_lower = 1;
    break;

  case 5: /* 40 upper */
    bss_data->spectrum    = 1;
    bss_data->upper_lower = 0;
    break;

  default:
    bss_data->spectrum    = 0;
    bss_data->upper_lower = 0;
    break;
  };

  res = MTLK_ERR_OK;

FINISH:
  return res;
}



/*!
	\fn	ie_extract_htcap_info()
	\brief	Process HT capabilities informational element
	\param	ie_data Pointer to IE
	\param	lenfth IE length
	\param	bss_data Pointer to BSS description structure.
  
	'HT Capabilities' IE parsed by this function and filled to the
	supplied bss_data structure. Rates information extracted.
*/
static __INLINE int
ie_extract_htcap_info (uint8      *ie_data,
                       int32       length,
                       bss_data_t *bss_data)
{
  int res = MTLK_ERR_PARAMS;
  uint8 val = 0;
  htcap_ie_t *htcap = (htcap_ie_t *)ie_data;

  if (length < 7) {
    ELOG("Wrong HT Capabilities length: %d", (int)length);
    goto FINISH;
  }
  htcap->info = WLAN_TO_HOST16(htcap->info);
  bss_data->forty_mhz_intolerant = htcap->info & HTCAP_40MHZ_INTOLERANT;

  /**********************************************************
   * Modulation and coding scheme parsing.
   *
   * This is a 'Supported MCS Set' field of HT Capabilities
   * Information Element. We're interested only in Rx MCS
   * bitmask (78 bits) subfield. See sections 7.3.2.52.1,
   * 7.3.2.52.4 and 20.6 of 802.11n Draft 3.02
   ***********************************************************/

  val = htcap->supported_mcs[0];
  /* MCS 0..7 are filled to OperationalRateSet bits 15..22
   * For 1 spartial stream with 1 BCC modulator
   * -------------------------
   * Idx          20 MHz                40 MHz
   *      Normal GI   Short GI  Normal GI   Short GI
   * 0    6.5         7.2       13.5        15
   * 1    13          14.4      27          30
   * 2    19.5        21.7      40.5        45
   * 3    26          28.9      54          60
   * 4    39          43.3      81          90
   * 5    52          57.8      108         120
   * 6    58.5        65.0      121.5       135
   * 7    65          72.2      135         150
   */
  bss_data->operational_rate_set |= ((uint32)val) << 15;

  val = htcap->supported_mcs[1];
  /* MCS 8..15 are filled to OperationalRateSet bits 23..30
   * For 2 spartial streams equal modulation with 1 BCC modulator
   * -------------------------
   * Idx          20 MHz                40 MHz
   *      Normal GI   Short GI  Normal GI   Short GI
   * 8    13          14.4      27          30
   * 9    26          28.9      54          60
   * 10   39          43.3      81          90
   * 11   52          57.8      108         120
   * 12   78          86.7      162         180
   * 13   104         115.6     216         240
   * 14   117         130       243         270
   * 15   130         144.4     270         300
   */
  bss_data->operational_rate_set |= ((uint32)val) << 23;

  val = htcap->supported_mcs[4];
  /* MCS 32 is filled to OperationalRateSet bit 31
   * For 1 spartial stream with 1 BCC modulator
   * -------------------------
   * Idx          20 MHz                40 MHz
   *      Normal GI   Short GI  Normal GI   Short GI
   * 32   x           x         6           6.7
   */
  bss_data->operational_rate_set |= ((uint32)val) << 31;

  res = MTLK_ERR_OK;

FINISH:
  return res;
}



/*!
	\fn	ie_process()
	\brief	Process management frame IEs (Beacon or Probe Response)
	\param	fbuf Pointer to IE
	\param	flen IE length
	\param	bss_data Pointer to BSS description structure.

	All supported IEs parsed by this function and filled to the
	supplied bss_data structure. 
*/
static void
ie_process (mtlk_handle_t context, uint8 *fbuf, int32 flen, bss_data_t *bss_data)
{
  static const uint8 wpa_oui_type[] = {0x00, 0x50, 0xF2, 0x01};
  static const uint8 wps_oui_type[] = {0x00, 0x50, 0xF2, 0x04};

  ie_t *ie;
  int32 copy_length;

  ILOG4(GID_FRAME, "Frame length: %d", flen);
  bss_data->basic_rate_set       = 0;
  bss_data->operational_rate_set = 0;
  bss_data->ie_used_length = 0;
  bss_data->rsn_offs = 0xFFFF;
  bss_data->wpa_offs = 0xFFFF;
  bss_data->wps_offs = 0xFFFF;

  while (flen > sizeof(ie_t))
  {
    ie = (ie_t *)fbuf;  /* WARNING! check on different platforms */
    if ((int32)(ie->length + sizeof(ie_t)) > flen) {
        flen = 0;
        break;
    }

    if (bss_data->ie_used_length + ie->length + sizeof(ie_t) > sizeof(bss_data->info_elements))
    {
        ILOG4(GID_FRAME, "Not enough room to store all information elements!!!");
        flen = 0;
        break;
    }

    if (ie->length) {
        memcpy(&bss_data->info_elements[bss_data->ie_used_length], ie, ie->length + sizeof(ie_t));
        switch (ie->id)
        {
        case IE_EXT_SUPP_RATES:
          bss_data->is_2_4 = 1;
          /* FALLTHROUGH */
        case IE_SUPPORTED_RATES:
          ie_extract_phy_rates(GET_IE_DATA_PTR(ie),
                               ie->length,
                               bss_data);
          break;
        case IE_HT_INFORMATION:
          ie_extract_ht_info(GET_IE_DATA_PTR(ie),
                             ie->length,
                             bss_data);
          break;
        case IE_SSID:
          copy_length = ie->length;
          if (copy_length > BSS_ESSID_MAX_SIZE) {
            ILOG1(GID_FRAME, "SSID IE truncated (%u > %d)!!!",
                 copy_length, (int)BSS_ESSID_MAX_SIZE);
            copy_length = BSS_ESSID_MAX_SIZE;
          }
          memcpy(bss_data->essid, GET_IE_DATA_PTR(ie), copy_length);
          bss_data->essid[copy_length] = '\0';  // ESSID always zero-terminated
          ILOG4(GID_FRAME, "ESSID : %s", bss_data->essid);
          break;
        case IE_RSN:
          bss_data->rsn_offs = bss_data->ie_used_length;
          DUMP4(ie, ie->length + sizeof(ie_t),
                "RSN information element (WPA2)");
          break;
        case IE_VENDOR_SPECIFIC:
          if (!memcmp(GET_IE_DATA_PTR(ie), wpa_oui_type, sizeof(wpa_oui_type))) {
            bss_data->wpa_offs = bss_data->ie_used_length;
            DUMP4(ie, ie->length + sizeof(ie_t),
                  "Vendor Specific information element (WPA)");
          }
          else if (!memcmp(GET_IE_DATA_PTR(ie), wps_oui_type, sizeof(wps_oui_type))) {
            bss_data->wps_offs = bss_data->ie_used_length;
            DUMP4(ie, ie->length + sizeof(ie_t),
                  "Vendor Specific information element (WPS)");
          }
          break;
        case IE_DS_PARAM_SET:
          bss_data->channel = GET_IE_DATA_PTR(ie)[0];
          ILOG4(GID_FRAME, "DS parameter set channel: %u", bss_data->channel);
          break;
        case IE_HT_CAPABILITIES:
          bss_data->is_ht = 1;
          DUMP4(ie, ie->length + sizeof(ie_t),
                "HTCap information element");
          ie_extract_htcap_info(GET_IE_DATA_PTR(ie),
                                ie->length,
                                bss_data);
          break;
        case IE_PWR_CONSTRAINT:
          bss_data->power = GET_IE_DATA_PTR(ie)[0];
          ILOG4(GID_FRAME, "Power constraint: %d", bss_data->power);
          break;
        case IE_COUNTRY:
          bss_data->country_code = country_to_country_code((const char*)GET_IE_DATA_PTR(ie));
          bss_data->country_ie = (struct country_ie_t *)ie;
          break;
        /* TODO: Add required IE processing here */
        default:
          break;
        } /*switch*/
        bss_data->ie_used_length += ie->length + sizeof(ie_t);
    }
    ILOG4(GID_FRAME, "IE length: %d", ie->length);
    fbuf += ie->length + sizeof(ie_t);
    flen -= ie->length + sizeof(ie_t);
  } /*while*/

  /* Discard IEs that cause MAC failures afterward. */
  if ( ((!bss_data->is_2_4) && ((bss_data->basic_rate_set & BASIC_RATE_SET_OFDM_MANDATORY_RATES)
        != BASIC_RATE_SET_OFDM_MANDATORY_RATES_MASK)) ||
       (bss_data->is_2_4 && (bss_data->channel > BSS_DATA_2DOT4_CHANNEL_MAX)) ||
        !bss_data->channel ||
        !bss_data->operational_rate_set ||
       ((bss_data->spectrum &&
       !(bss_data->operational_rate_set & BASIC_RATE_SET_11N_RATE_MSK))))
  {
    memset(&bss_data->essid, 0, sizeof(bss_data->essid));
  }
}

static void
mtlk_process_action_frame_block_ack (mtlk_handle_t    context,
                                     uint8           *fbuf, 
                                     int32            flen,
                                     const IEEE_ADDR *src_addr)
{
  uint8         action = *(uint8 *)fbuf;
  uint16        rate   = 0;
  mtlk_addba_t *addba  = mtlk_get_addba_related_info(context, &rate);

  fbuf += sizeof(action);
  flen -= sizeof(action);

  ILOG2(GID_FRAME, "ACTION BA: action=%d", (int)action);

  /* NOTE: no WLAN_TO_HOST required for bitfields */
  switch (action) {
  case ACTION_FRAME_ACTION_ADDBA_REQUEST:
    {
      frame_ba_addba_req_t *req       = (frame_ba_addba_req_t *)fbuf;
      uint16                param_set = WLAN_TO_HOST16(req->param_set);
      uint16                ssn       = WLAN_TO_HOST16(req->ssn);
 
      mtlk_addba_on_addba_req_rx(addba,
                                 (IEEE_ADDR *)src_addr,
                                 MTLK_BFIELD_GET(ssn, ADDBA_SSN_SSN),
                                 MTLK_BFIELD_GET(param_set, ADDBA_PARAM_SET_TID),
                                 (uint8)MTLK_BFIELD_GET(param_set, ADDBA_PARAM_SET_SIZE),
                                 (uint8)req->dlgt,
                                 WLAN_TO_HOST16((uint16)req->timeout),
                                 MTLK_BFIELD_GET(param_set, ADDBA_PARAM_SET_POLICY),
                                 rate);
    }
    break;
  case ACTION_FRAME_ACTION_ADDBA_RESPONSE:
    {
      frame_ba_addba_res_t *res       = (frame_ba_addba_res_t *)fbuf;
      uint16                param_set = WLAN_TO_HOST16(res->param_set);

      mtlk_addba_on_addba_res_rx(addba,
                                 src_addr,
                                 WLAN_TO_HOST16((uint16)res->scode),
                                 MTLK_BFIELD_GET(param_set, ADDBA_PARAM_SET_TID),
                                 (uint8)MTLK_BFIELD_GET(param_set, ADDBA_PARAM_SET_SIZE),
                                 (uint8)res->dlgt);
    }
    break;
  case ACTION_FRAME_ACTION_DELBA:
    {
      frame_ba_delba_t* delba     = (frame_ba_delba_t *)fbuf;
      uint16            param_set = WLAN_TO_HOST16(delba->param_set);

      mtlk_addba_on_delba_req_rx(addba,
                                 src_addr,
                                 MTLK_BFIELD_GET(param_set, DELBA_PARAM_SET_TID),
                                 WLAN_TO_HOST16((uint16)delba->reason),
                                 MTLK_BFIELD_GET(param_set, DELBA_PARAM_SET_INITIATOR));
    }
    break;
  default:
    break;
  }
}

static void
mtlk_process_action_frame_vendor_specific (mtlk_handle_t    context,
                                           uint8           *fbuf, 
                                           int32            flen,
                                           const IEEE_ADDR *src_addr)
{
  MTLK_VS_ACTION_FRAME_PAYLOAD_HEADER *vsaf_hdr = NULL;

  if (fbuf[0] != MTLK_OUI_0 ||
      fbuf[1] != MTLK_OUI_1 ||
      fbuf[2] != MTLK_OUI_2) {
    ILOG0(GID_FRAME, "ACTION Vendor Specific: %02x-%02x-%02x",
         (int)fbuf[0],
         (int)fbuf[1],
         (int)fbuf[2]);
    goto end;
  }
  
  /* Skip OUI */
  fbuf += 3;
  flen -= 3;

  ILOG2(GID_FRAME, "ACTION Vendor Specific: MTLK");

  DUMP3(fbuf, 16, "VSAF of %u bytes", flen);

  vsaf_hdr = (MTLK_VS_ACTION_FRAME_PAYLOAD_HEADER *)fbuf;

  /* Skip VSAF payload header */
  fbuf += sizeof(*vsaf_hdr);
  flen -= sizeof(*vsaf_hdr);

  /* Check VSAF payload data size */
  vsaf_hdr->u32DataSize = WLAN_TO_HOST32(vsaf_hdr->u32DataSize);
  if (vsaf_hdr->u32DataSize != flen) {
    WLOG("Incorrect VSAF length (%u != %u), skipped",
            vsaf_hdr->u32DataSize,
            flen);
    goto end;
  }

  /* Check VSAF format version */
  vsaf_hdr->u32Version = WLAN_TO_HOST32(vsaf_hdr->u32Version);
  if (vsaf_hdr->u32Version != CURRENT_VSAF_FMT_VERSION) {
    WLOG("Incorrect VSAF FMT version (%u != %u), skipped",
            vsaf_hdr->u32Version,
            CURRENT_VSAF_FMT_VERSION);
    goto end;
  }

  /* Parse VSAF items */
  vsaf_hdr->u32nofItems = WLAN_TO_HOST32(vsaf_hdr->u32nofItems);
  while (vsaf_hdr->u32nofItems) {
    MTLK_VS_ACTION_FRAME_ITEM_HEADER *item_hdr = 
      (MTLK_VS_ACTION_FRAME_ITEM_HEADER *)fbuf;

    fbuf += sizeof(*item_hdr);
    flen -= sizeof(*item_hdr);

    item_hdr->u32DataSize = WLAN_TO_HOST32(item_hdr->u32DataSize);
    item_hdr->u32ID       = WLAN_TO_HOST32(item_hdr->u32ID);

    switch (item_hdr->u32ID) {
#ifdef MTCFG_RF_MANAGEMENT_MTLK
    case MTLK_VSAF_ITEM_ID_SPR:
      {
        mtlk_rf_mgmt_t *rf_mgmt = mtlk_get_rf_mgmt(context);
      
        ILOG2(GID_FRAME, "SPR received of %u bytes",
             item_hdr->u32DataSize);

        mtlk_rf_mgmt_handle_spr(rf_mgmt, src_addr, fbuf, flen);
      }
      break;
#endif
    default:
      WLOG("Unsupported VSAF item (id=0x%08x, size = %u)",
              item_hdr->u32ID,
              item_hdr->u32DataSize);
      break;
    }

    fbuf += item_hdr->u32DataSize;
    flen -= item_hdr->u32DataSize;

    vsaf_hdr->u32nofItems--;
  }

end:
  return;
}

int
mtlk_process_man_frame (mtlk_handle_t context, struct mtlk_scan *scan_data, scan_cache_t* cache, 
    mtlk_aocs_t *aocs, uint8 *fbuf, int32 flen, const MAC_RX_ADDITIONAL_INFO_T *mac_rx_info)
{
  mtlk_core_t *core = ((mtlk_core_t*)context);
  uint16 subtype;
  frame_head_t *head;
  frame_beacon_head_t *beacon_head;
  bss_data_t *bss_data;

  head = (frame_head_t *)fbuf;
  fbuf += sizeof(frame_head_t);
  flen -= sizeof(frame_head_t);

  DUMP4(head, sizeof(frame_head_t), "802.11n head:");

  subtype = head->frame_control;
  subtype = (MAC_TO_HOST16(subtype) & FRAME_SUBTYPE_MASK) >> FRAME_SUBTYPE_SHIFT;

  ILOG4(GID_FRAME, "Subtype is %d", subtype);

  switch (subtype)
  {
  case MAN_TYPE_BEACON:
  case MAN_TYPE_PROBE_RES:
    beacon_head = (frame_beacon_head_t *)fbuf;
    fbuf += sizeof(frame_beacon_head_t);
    flen -= sizeof(frame_beacon_head_t);

    ILOG4(GID_FRAME, "RX_INFO: en=%u mn=%u c=%u rsn=%u prl=%u r=%u r=%u:%u:%u mr=%u",
         (unsigned)mac_rx_info->u8EstimatedNoise,
         (unsigned)mac_rx_info->u8MinNoise,
         (unsigned)mac_rx_info->u8Channel,
         (unsigned)mac_rx_info->u8RSN,
         (unsigned)MAC_TO_HOST16(mac_rx_info->u16PhyRxLen),
         (unsigned)mac_rx_info->u8RxRate,
         (unsigned)mac_rx_info->au8Rssi[0],
         (unsigned)mac_rx_info->au8Rssi[1],
         (unsigned)mac_rx_info->au8Rssi[2],
         (unsigned)mac_rx_info->u8MaxRssi);
    
    bss_data = mtlk_cache_temp_bss_acquire(cache);

    memset(bss_data, 0, sizeof(*bss_data));
    bss_data->channel = mac_rx_info->u8Channel;
    mtlk_osal_copy_eth_addresses(bss_data->bssid, head->bssid);
    ILOG4(GID_FRAME, "BSS %Y found on channel %d", bss_data->bssid,
          bss_data->channel);
    bss_data->capability = MAC_TO_HOST16(beacon_head->capability);
    bss_data->beacon_interval = MAC_TO_HOST16(beacon_head->beacon_interval);
    bss_data->beacon_timestamp = WLAN_TO_HOST64(beacon_head->beacon_timestamp);
    ILOG4(GID_FRAME, "Advertised capabilities : 0x%04X", bss_data->capability);
    if (bss_data->capability & CAPABILITY_IBSS_MASK) {
      bss_data->bss_type = UMI_BSS_ADHOC;
      ILOG4(GID_FRAME, "BSS type is Ad-Hoc");
    } else {
      bss_data->bss_type = UMI_BSS_INFRA;
      ILOG4(GID_FRAME, "BSS type is Infra");
      if (bss_data->capability & CAPABILITY_NON_POLL) {
        bss_data->bss_type = UMI_BSS_INFRA_PCF;
        ILOG4(GID_FRAME, "PCF supported");
      }
    }

    // RSSI
    ASSERT(sizeof(mac_rx_info->au8Rssi) == sizeof(bss_data->all_rssi));
    memcpy(bss_data->all_rssi, mac_rx_info->au8Rssi, sizeof(mac_rx_info->au8Rssi));
    bss_data->max_rssi = mac_rx_info->u8MaxRssi;
    bss_data->noise    = mac_rx_info->u8EstimatedNoise;

    ILOG3(GID_FRAME, "BSS %Y, channel %u, max_rssi is %d, noise is %d",
         bss_data->bssid, bss_data->channel, 
         MTLK_NORMALIZED_RSSI(bss_data->max_rssi),
         MTLK_NORMALIZED_RSSI(bss_data->noise));

    // Process information elements
    ie_process(context, fbuf, (int32)flen, bss_data);

    if (mtlk_core_get_dot11d(core) && bss_data->country_code && !mtlk_core_get_country_code(core)) {
      ILOG1(GID_FRAME, "Adopted country code: %s(0x%02x)",
          country_code_to_country(bss_data->country_code), bss_data->country_code);
      mtlk_core_set_country_code(core, bss_data->country_code);
      mtlk_scan_schedule_rescan(scan_data);
    }

    mtlk_find_and_update_ap(context, head->src_addr, bss_data);
    // APs are updated on beacons

    mtlk_scan_handle_bss_found_ind (scan_data, bss_data->channel);

    if (mtlk_scan_is_running(scan_data) || !mtlk_core_get_country_code(core))
      mtlk_cache_register_bss(cache, bss_data);

    mtlk_aocs_on_bss_data_update(aocs, bss_data);

    mtlk_cache_temp_bss_release(cache);

    break;
  case MAN_TYPE_ACTION:
    {
      uint8 category = *(uint8 *)fbuf;

      fbuf += sizeof(category);
      flen -= sizeof(category);

      ILOG2(GID_FRAME, "ACTION: category=%d", (int)category);

      switch (category) {
      case ACTION_FRAME_CATEGORY_BLOCK_ACK:
        mtlk_process_action_frame_block_ack(context,
                                            fbuf,
                                            flen,
                                            (IEEE_ADDR *)head->src_addr);
        break;
      case ACTION_FRAME_CATEGORY_VENDOR_SPECIFIC:
        mtlk_process_action_frame_vendor_specific(context,
                                                  fbuf,
                                                  flen,
                                                  (IEEE_ADDR *)head->src_addr);
        break;
      default:
        break;
      }
    }
    break;
  default:
    break;
  }
  return 0;
}



int
mtlk_process_ctl_frame (mtlk_handle_t context, uint8 *fbuf, int32 flen)
{
  uint16 fc, subtype;

  MTLK_UNREFERENCED_PARAM(flen);

  fc = MAC_TO_HOST16(*(uint16 *)fbuf);
  subtype = WLAN_FC_GET_STYPE(fc);

  ILOG4(GID_FRAME, "Subtype is 0x%04X", subtype);

  switch (subtype)
  {
  case IEEE80211_STYPE_BAR:
    {
      frame_bar_t  *bar = (frame_bar_t*)fbuf;

      uint16 bar_ctl = MAC_TO_HOST16(bar->ctl);
      uint16 ssn = MAC_TO_HOST16(bar->ssn);
      uint8  tid = (uint8)((bar_ctl & IEEE80211_BAR_TID) >> IEEE80211_BAR_TID_S);

      ssn = (ssn & IEEE80211_SCTL_SEQ) >> 4; /* 0x000F - frag#, 0xFFF0 - seq */

      mtlk_handle_bar(context, bar->ta, tid, ssn);
    }
    break;
  default:
    break;
  }
  return 0;
}
