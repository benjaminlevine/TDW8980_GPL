/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/


/***************************************************************************
****************************************************************************
**
** COMPONENT:        ENET Upper MAC    SW
**
** MODULE:           $File: //bwp/enet/demo153_sw/develop/src/shared/mhi_umi.h $
**
** VERSION:          $Revision: #4 $
**
** DATED:            $Date: 2007/03/04 $
**
** AUTHOR:           S Sondergaard
**
** DESCRIPTION:      Upper MAC Public Header
**
****************************************************************************
*
* Copyright (c) TTPCom Limited, 2003
*
* Copyright (c) Metalink Ltd., 2006 - 2007
*
****************************************************************************/

#ifndef __MHI_UMI_INCLUDED_H
#define __MHI_UMI_INCLUDED_H

#include "mhi_ieee_address.h"
#include "mhi_frame.h"
#include "mhi_rsn_caps.h"
#include "msgid.h"
#include "mhi_statistics.h"
#include "umi_rsn.h"
#include "MT_mac_host_adapt.h"
#include "mtlkbfield.h"

#define   MTLK_PACK_ON
#include "mtlkpack.h"

#define  TU                             1024 /* 1TU (Time Unit) = 1024 microseconds - defined in 802.11 */

/***************************************************************************/
/***                       Types and Defines                             ***/
/***************************************************************************/

#define UMI_MAX_MSDU_LENGTH             (MSDU_MAX_LENGTH)

typedef uint8 UMI_STATUS;
#define UMI_OK                          0
#define UMI_NOT_INITIALISED             1
#define UMI_BAD_PARAMETER               2
#define UMI_BAD_VALUE                   3
#define UMI_BAD_LENGTH                  4
#define UMI_MC_BUSY                     5
#define UMI_ALREADY_ENABLED             6
#define UMI_HW_FAILURE                  7
#define UMI_BSS_ALREADY_ACTIVE          8
#define UMI_BSS_HAS_NO_CFP              9
#define UMI_BSS_UNKNOWN                 10
#define UMI_STATION_UNKNOWN             11
#define UMI_NOT_ENABLED                 12
#define UMI_OUT_OF_MEMORY               13
#define UMI_TIMEOUT                     14
#define UMI_NOT_CONNECTED               15
#define UMI_UNKNOWN_OBJECT              16
#define UMI_READ_ONLY                   17
#define UMI_WRITE_ONLY                  18
#define UMI_RATE_MISMATCH               19
#define UMI_TRANSFER_ALREADY_ACTIVE     20
#define UMI_TRANSFER_FAILED             21
#define UMI_NOT_SUPPORTED               22
#define UMI_RSN_IE_FORMAT_ERROR         23
#define UMI_RSN_BAD_CAPABILITIES        24
#define UMI_INTERNAL_MAC_ERROR          25
#define UMI_UM_BUSY						26
#define UMI_PS_NOT_ENABLED				27

/* Status codes for memory allocation */
#define UMI_ALLOC_OK                    UMI_OK
#define UMI_ALLOC_FAILED                UMI_OUT_OF_MEMORY
#define UMI_ALLOC_FWD_POOL_OK           26
#define UMI_STATUS_TOTAL_NUMBER         27

typedef uint8 UMI_NOTIFICATION;
#define UMI_NOTIFICATION_OK             0
#define UMI_NOTIFICATION_MIC_FAILURE    1

#define UMI_MAX_CHANNELS_PER_SCAN_REQ   16
typedef uint16 UMI_BSS_TYPE;
#define UMI_BSS_INFRA                   0
#define UMI_BSS_INFRA_PCF               1
#define UMI_BSS_ADHOC                   2
#define UMI_BSS_ANY                     3

typedef uint16 UMI_NETWORK_STATUS;
#define UMI_BSS_CREATED                 0   // We have created a network (BSS)
#define UMI_BSS_CONNECTING              1   // STA is trying to connect to AP
#define UMI_BSS_CONNECTED               2   // STA has connected to network (auth and assoc) or AP/STA resume connection after channel switch
#define UMI_BSS_FAILED                  4   // STA is unable to connect with any network
#define UMI_BSS_RADAR_NORM              5   // Regular radar was detected.
#define UMI_BSS_RADAR_HOP               6   // Frequency Hopping radar was detected.
#define UMI_BSS_CHANNEL_SWITCH_NORMAL   7   // STA received a channel announcement with non-silent mode.
#define UMI_BSS_CHANNEL_SWITCH_SILENT   8   // STA received a channel announcement with silent mode.
#define UMI_BSS_CHANNEL_SWITCH_DONE     9   // AP/STA have just switched channel (but traffic may be started only after UMI_BSS_CONNECTED event)
#define UMI_BSS_CHANNEL_PRE_SWITCH_DONE 10  //
#define UMI_BSS_OVER_CHANNEL_LOAD_THRESHOLD 11  // Channel load threshold over.


//PHY characteristics parameters
#define UMI_PHY_TYPE_802_11A          	0x00    /* 802.11a  */
#define UMI_PHY_TYPE_802_11B          	0x01    /* 802.11b  */
#define UMI_PHY_TYPE_802_11G          	0x02    /* 802.11g  */
#define UMI_PHY_TYPE_802_11B_L      	0x81    /* 802.11b with long preamble*/
#define UMI_PHY_TYPE_802_11N_5_2_BAND 	0x04    /* 802.11n_5.2G  */
#define UMI_PHY_TYPE_802_11N_2_4_BAND 	0x05    /* 802.11n_2.4G  */

#define UMI_PHY_TYPE_BAND_5_2_GHZ       0                  
#define UMI_PHY_TYPE_BAND_2_4_GHZ       1

#define UMI_PHY_TYPE_UPPER_CHANNEL      0       /* define UPPER secondary channel offset */
#define UMI_PHY_TYPE_LOWER_CHANNEL      1       /* define LOWER secondary channel offset */

//Channel SwitchMode values
#define UMI_CHANNEL_SW_MODE_NORMAL 0x00
#define UMI_CHANNEL_SW_MODE_SILENT 0x01
#define UMI_CHANNEL_SW_MODE_UPPER  0x10
#define UMI_CHANNEL_SW_MODE_LOWER  0x30
#define UMI_CHANNEL_SW_MODE_NCB    0x00


/* Reason for network connection/disconnection */

/*
 * IMPORTANT NOTE: the following enum definitions will not work with c100 environment (since the
 * scripts recognize constant values by the prefix #define).
 * => For testing there is a need to replace all the enum's with defines.
 */

/* D.S  when using C100 on uart*/
#ifndef ENET_FPGA_MAC_BORAD

#define CONNECT_REASON(defname, defval, comment)
#define CONNECT_REASONS \
        CONNECT_REASON(UMI_BSS_NEW_NETWORK,            0,  "We have created a network (BSS)") \
        CONNECT_REASON(UMI_BSS_JOINED,                 1,  "We have joined a network (BSS)") \
        CONNECT_REASON(UMI_BSS_DEAUTHENTICATED,        2,  "STA was deauthenticated from AP") \
        CONNECT_REASON(UMI_BSS_DISASSOCIATED,          3,  "STA was disassociated from AP") \
        CONNECT_REASON(UMI_BSS_JOIN_FAILED,            4,  "Join has failed or timed out") \
        CONNECT_REASON(UMI_BSS_AUTH_FAILED,            5,  "Authentication has failed (or timed out)") \
        CONNECT_REASON(UMI_BSS_ASSOC_FAILED,           6,  "Association has failed (or timed out)") \
        CONNECT_REASON(UMI_BSS_BEACON_TIMEOUT,         7,  "STA has lost contact with network") \
        CONNECT_REASON(UMI_BSS_ROAMING,                8,  "STA is roaming to another BSS") \
        CONNECT_REASON(UMI_BSS_MANUAL_DISCONNECT,      9,  "UMI has forced a disconnet") \
        CONNECT_REASON(UMI_BSS_NO_NETWORK,             10, "There is no network to join") \
        CONNECT_REASON(UMI_BSS_IBSS_COALESCE,          11, "We are coasescing with another IBSS") \
        CONNECT_REASON(UMI_BSS_11H,                    12, "BSS 11h - Radar-Detection or Channel Switch related event") \
        CONNECT_REASON(UMI_BSS_AGED_OUT,               13, "STA did not responded more than reasonable time")

#undef CONNECT_REASON
#define CONNECT_REASON(defname, defval, comment) defname = defval,
enum
{
    CONNECT_REASONS
};

#else /* ENET_FPGA_MAC_BORAD*/

#define UMI_BSS_NEW_NETWORK             0
#define UMI_BSS_JOINED                  1
#define UMI_BSS_DEAUTHENTICATED         2
#define UMI_BSS_DISASSOCIATED           3
#define UMI_BSS_JOIN_FAILED             4
#define UMI_BSS_AUTH_FAILED             5
#define UMI_BSS_ASSOC_FAILED            6
#define UMI_BSS_BEACON_TIMEOUT          7
#define UMI_BSS_ROAMING                 8
#define UMI_BSS_MANUAL_DISCONNECT       9
#define UMI_BSS_NO_NETWORK             10
#define UMI_BSS_IBSS_COALESCE          11
#define UMI_BSS_11H                    12
#define UMI_BSS_AGED_OUT               13

#endif /* ENET_FPGA_MAC_BORAD*/

/* Stop reasons */
#define BSS_STOP_REASON_JOINED	  		0	               
#define BSS_STOP_REASON_DISCONNECT		1
#define BSS_STOP_REASON_JOINED_FAILED	2
#define BSS_STOP_REASON_SCAN			3
#define BSS_STOP_REASON_MC_REQ			4
#define BSS_STOP_REASON_BGSCAN			5
#define BSS_STOP_REASON_UM_REQ			6
#define BSS_STOP_REASON_NONE	 		0xFF

typedef uint16 UMI_CONNECTION_STATUS;
#define UMI_CONNECTED                   0
#define UMI_DISCONNECTED                1
#define UMI_RECONNECTED                 2

typedef uint16 UMI_PCF_CAPABILITY;
#define UMI_NO_PCF                      0
#define UMI_HAS_PCF                     1

typedef uint16 UMI_ACCESS_PROTOCOL;
#define UMI_USE_DCF                     0
#define UMI_USE_PCF                     1


#define UMI_DEBUG_BLOCK_SIZE            (256)
#define UMI_C100_DATA_SIZE   (UMI_DEBUG_BLOCK_SIZE - (sizeof(uint16) + sizeof(uint16) + sizeof(C100_MSG_HEADER)))
#define UMI_DEBUG_DATA_SIZE  (UMI_DEBUG_BLOCK_SIZE - (sizeof(uint16) + sizeof(uint16)))

#define PS_REQ_MODE_ON					1
#define PS_REQ_MODE_OFF					0



/*************************************
* For the use of the Generic message.
*/
#define MAC_VARIABLES_REQ       1
#define MAC_EEPROM_REQ          2
#define MAC_OCS_TIMER_START     3
#define MAC_OCS_TIMER_STOP      4
#define MAC_OCS_TIMER_TIMEOUT   5


#define NEW_CASE                6

/*************************************
* LBF defines.
*/

#define LBF_NUM_MAT_SETS 8
#define LBF_NUM_CDD_SETS 16
#define LBF_DISABLED_SET 0xff

/***************************************************************************/
/***                         memory array definition                     ***/
/***************************************************************************/
#define ARRAY_NULL              0
#define ARRAY_DAT_IND           1
#define ARRAY_DAT_REQ           2
#define ARRAY_MAN_IND           3
#define ARRAY_MAN_REQ           4
#define ARRAY_DBG_IND           5
#define ARRAY_DBG_REQ           6
#define ARRAY_DAT_REQ_FAIL      7
#define ARRAY_DAT_IND_SND       8 /* Data Sounding packet */

/***************************************************************************/
/***                         Message IDs                                 ***/
/***************************************************************************/

#define UMI_MSG_ID_INVALID              (MSG_COMP_MASK | MSG_NUM_MASK)

/* message NULL reserved K_NULL_MSG  */

/* Management messages */
#define UM_MAN_READY_REQ               (K_MSG_TYPE)0x0401 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 1)*/
#define MC_MAN_READY_CFM               (K_MSG_TYPE)0x1401 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 1)*/

#define UM_MAN_SET_MIB_REQ             (K_MSG_TYPE)0x0402 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 2)*/
#define MC_MAN_SET_MIB_CFM             (K_MSG_TYPE)0x1402 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 2)*/

#define UM_MAN_GET_MIB_REQ             (K_MSG_TYPE)0x0403 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 3)*/
#define MC_MAN_GET_MIB_CFM             (K_MSG_TYPE)0x1403 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 3)*/

#define UM_MAN_SCAN_REQ                (K_MSG_TYPE)0x0404 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 4)*/
#define MC_MAN_SCAN_CFM                (K_MSG_TYPE)0x1404 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 4)*/

#define UM_DOWNLOAD_PROG_MODEL_REQ     (K_MSG_TYPE)0x0405 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 5)*/
#define MC_DOWNLOAD_PROG_MODEL_CFM     (K_MSG_TYPE)0x1405 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 5)*/

#define UM_MAN_ACTIVATE_REQ            (K_MSG_TYPE)0x0406 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 6)*/
#define MC_MAN_ACTIVATE_CFM            (K_MSG_TYPE)0x1406 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 6)*/

#define UM_MAN_DISCONNECT_REQ          (K_MSG_TYPE)0x0409 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 9)*/
#define MC_MAN_DISCONNECT_CFM          (K_MSG_TYPE)0x1409 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 9)*/

/*connection status*/
#define UM_MAN_GET_CONNECTION_STATUS_REQ   (K_MSG_TYPE)0x040A /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 10)*/
#define MC_MAN_GET_CONNECTION_STATUS_CFM   (K_MSG_TYPE)0x140A /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 10)*/

#define UM_MAN_RESET_REQ               (K_MSG_TYPE)0x040B /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 11)*/
#define MC_MAN_RESET_CFM               (K_MSG_TYPE)0x140B /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 11)*/

#define UM_MAN_POWER_MODE_REQ          (K_MSG_TYPE)0x040C /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 12)*/
#define MC_MAN_POWER_MODE_CFM          (K_MSG_TYPE)0x140C /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 12)*/


#define UM_MAN_SET_KEY_REQ             (K_MSG_TYPE)0x040E /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 14)*/
#define MC_MAN_SET_KEY_CFM             (K_MSG_TYPE)0x140E /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 14)*/

#define UM_MAN_CLEAR_KEY_REQ           (K_MSG_TYPE)0x040F /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 15)*/
#define MC_MAN_CLEAR_KEY_CFM           (K_MSG_TYPE)0x140F /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 15)*/

#define UM_START_DOWNLOAD_PROG_MODEL   (K_MSG_TYPE)0x0010 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 16)*/

#define UM_MAN_SET_BCL_VALUE           (K_MSG_TYPE)0x0411 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 17)*/
#define MC_MAN_SET_BCL_CFM             (K_MSG_TYPE)0x1411 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 17)*/

#define UM_MAN_QUERY_BCL_VALUE         (K_MSG_TYPE)0x0412 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 18)*/
#define MC_MAN_QUERY_BCL_CFM           (K_MSG_TYPE)0x1412 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 18)*/

#define UM_MAN_GET_MAC_VERSION_REQ     (K_MSG_TYPE)0x0413 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 19)*/
#define MC_MAN_GET_MAC_VERSION_CFM     (K_MSG_TYPE)0x1413 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 19)*/

#define UM_MAN_OPEN_AGGR_REQ           (K_MSG_TYPE)0x0414 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 20)*/
#define MC_MAN_OPEN_AGGR_CFM           (K_MSG_TYPE)0x1414 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 20)*/

#define UM_MAN_ADDBA_REQ_TX_REQ        (K_MSG_TYPE)0x0415 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 21)*/
#define MC_MAN_ADDBA_REQ_TX_CFM        (K_MSG_TYPE)0x1415 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 21)*/

#define UM_MAN_ADDBA_RES_TX_REQ        (K_MSG_TYPE)0x0417 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 23)*/
#define MC_MAN_ADDBA_RES_TX_CFM        (K_MSG_TYPE)0x1417 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 23)*/

#define UM_MAN_GENERIC_MAC_REQ         (K_MSG_TYPE)0x0418 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 24)*/
#define MC_MAN_GENERIC_MAC_CFM         (K_MSG_TYPE)0x1418 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 24)*/

#define UM_MAN_SW_RESET_MAC_REQ        (K_MSG_TYPE)0x0419 /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 25)*/
#define MC_MAN_SW_RESET_MAC_CFM        (K_MSG_TYPE)0x1419 /*(MSG_CFM + MSG_UMI_COMP + UMI_DBG_MSG + 25)*/

#define UM_MAN_DELBA_REQ               (K_MSG_TYPE)0x041A /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 26)*/
#define MC_MAN_DELBA_CFM               (K_MSG_TYPE)0x141A /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 26)*/

#define UM_PER_CHANNEL_CALIBR_REQ      (K_MSG_TYPE)0x041B /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 27)*/
#define MC_PER_CHANNEL_CALIBR_CFM      (K_MSG_TYPE)0x141B /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 27)*/


#define UM_LM_STOP_REQ                 (K_MSG_TYPE)0x041C /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 22)*/
#define MC_LM_STOP_CFM                 (K_MSG_TYPE)0x141C /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 22)*/

#define UM_MAN_CLOSE_AGGR_REQ          (K_MSG_TYPE)0x041D /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 29)*/
#define MC_MAN_CLOSE_AGGR_CFM          (K_MSG_TYPE)0x141D /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 29)*/

#define UM_START_PER_CHANNEL_CALIBR    (K_MSG_TYPE)0x0020 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 32)*/

#define UM_MAN_GET_GROUP_PN_REQ        (K_MSG_TYPE)0x0430 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 48)*/
#define MC_MAN_GET_GROUP_PN_CFM        (K_MSG_TYPE)0x1430 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 48)*/

#define UM_MAN_SET_IE_REQ              (K_MSG_TYPE)0x0431 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 49)*/
#define MC_MAN_SET_IE_CFM              (K_MSG_TYPE)0x1431 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 49)*/

#define UM_SET_CHAN_REQ                (K_MSG_TYPE)0x0432 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 50)*/
#define UM_SET_CHAN_CFM                (K_MSG_TYPE)0x1432 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 50)*/

//channel load msg
#define UM_MAN_SET_CHANNEL_LOAD_VAR_REQ (K_MSG_TYPE)0x0433 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 51)*/
#define MC_MAN_SET_CHANNEL_LOAD_VAR_CFM (K_MSG_TYPE)0x1433 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 51)*/

#define UM_MAN_SET_LED_REQ             (K_MSG_TYPE)0x0434 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 52)*/
#define MC_MAN_SET_LED_CFM             (K_MSG_TYPE)0x1434 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 52)*/
#define UM_MAN_SET_DEF_RF_MGMT_DATA_REQ (K_MSG_TYPE)0x0435 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 53)*/
#define MC_MAN_SET_DEF_RF_MGMT_DATA_CFM (K_MSG_TYPE)0x1435 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 53)*/

#define UM_MAN_GET_DEF_RF_MGMT_DATA_REQ (K_MSG_TYPE)0x0436 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 54)*/
#define MC_MAN_GET_DEF_RF_MGMT_DATA_CFM (K_MSG_TYPE)0x1436 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 54)*/
#define UM_MAN_SEND_MTLK_VSAF_REQ      (K_MSG_TYPE)0x0437 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 55)*/
#define MC_MAN_SEND_MTLK_VSAF_CFM      (K_MSG_TYPE)0x1437 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 55)*/

#define MC_UM_PS_REQ				   (K_MSG_TYPE)0x0438 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 56)*/
#define UM_MC_PS_CFM				   (K_MSG_TYPE)0x1438 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 56)*/

#define MC_MAN_CHANGE_POWER_STATE_REQ  (K_MSG_TYPE)0x0439 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 57)*/
#define UM_MAN_CHANGE_POWER_STATE_CFM  (K_MSG_TYPE)0x1439 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 57)*/

#define UM_MAN_RF_MGMT_SET_TYPE_REQ    (K_MSG_TYPE)0x0440 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 58)*/
#define MC_MAN_RF_MGMT_SET_TYPE_CFM    (K_MSG_TYPE)0x1440 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 58)*/

#define UM_MAN_RF_MGMT_GET_TYPE_REQ    (K_MSG_TYPE)0x0441 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 59)*/
#define MC_MAN_RF_MGMT_GET_TYPE_CFM    (K_MSG_TYPE)0x1441 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 59)*/

#define UM_MAN_DOWNLOAD_PROG_MODEL_PERMISSION_REQ (K_MSG_TYPE)0x0442 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 60)*/
#define MC_MAN_DOWNLOAD_PROG_MODEL_PERMISSION_CFM (K_MSG_TYPE)0x1442 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 60)*/

#define UM_MAN_SET_PEERAP_REQ          (K_MSG_TYPE)0x0443 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 0x43)*/
#define MC_MAN_SET_PEERAP_CFM          (K_MSG_TYPE)0x1443 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 0x43)*/

#define UM_MAN_CHANGE_TX_POWER_LIMIT_REQ (K_MSG_TYPE)0x0444 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 61)*/
#define MC_MAN_CHANGE_TX_POWER_LIMIT_CFM (K_MSG_TYPE)0x1444 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 61)*/

#define MC_MAN_NETWORK_EVENT_IND       (K_MSG_TYPE)0x3307 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 7)*/
#define UM_MAN_NETWORK_EVENT_RES       (K_MSG_TYPE)0x2307 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 7)*/

#define MC_MAN_CONNECTION_EVENT_IND    (K_MSG_TYPE)0x3308 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 8)*/
#define UM_MAN_CONNECTION_EVENT_RES    (K_MSG_TYPE)0x2308 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 8)*/

#define MC_MAN_SECURITY_ALERT_IND      (K_MSG_TYPE)0x3310 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 16)*/
#define UM_MAN_SECURITY_ALERT_RES      (K_MSG_TYPE)0x2310 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 16)*/

#define MC_MAN_BAR_IND                 (K_MSG_TYPE)0x3314 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 20)*/
#define UM_MAN_BAR_RES                 (K_MSG_TYPE)0x2314 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 20)*/

#define MC_MAN_ADDBA_REQ_RX_IND        (K_MSG_TYPE)0x3316 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 22)*/
#define UM_MAN_ADDBA_REQ_RX_RES        (K_MSG_TYPE)0x2316 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 22)*/

#define MC_MAN_ADDBA_RES_RX_IND        (K_MSG_TYPE)0x3319 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 25)*/
#define UM_MAN_ADDBA_RES_RX_RES        (K_MSG_TYPE)0x2319 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 25)*/

#define MC_MAN_DELBA_IND               (K_MSG_TYPE)0x332B /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 43)*/
#define UM_MAN_DELBA_RES               (K_MSG_TYPE)0x232B /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 43)*/

#define MC_MAN_DYNAMIC_PARAM_IND       (K_MSG_TYPE)0x3344 /*(MSG_IND + MSG_UMI_COMP + UMI_MEM_MSG + 4)*/
#define UM_MAN_DYNAMIC_PARAM_RES       (K_MSG_TYPE)0x2344 /*(MSG_RES + MSG_UMI_COMP + UMI_MEM_MSG + 4)*/

#define UM_MAN_AOCS_REQ                (K_MSG_TYPE)0x0421 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 33)*/
#define MC_MAN_AOCS_CFM                (K_MSG_TYPE)0x1421 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 33)*/

#define MC_MAN_AOCS_IND                (K_MSG_TYPE)0x3321 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 33)*/
#define UM_MAN_AOCS_RES                (K_MSG_TYPE)0x2321 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 33)*/

#define MC_MAN_PM_UPDATE_IND		   (K_MSG_TYPE)0x3332 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 50)*/
#define UM_MAN_PM_UPDATE_RES		   (K_MSG_TYPE)0x2332 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 50)*/

/* Data Messages */
#define UM_DAT_TXDATA_REQ              (K_MSG_TYPE)0x0240 /*(MSG_REQ + MSG_UMI_COMP + UMI_DAT_MSG + 0)*/
#define MC_DAT_TXDATA_CFM              (K_MSG_TYPE)0x1240 /*(MSG_CFM + MSG_UMI_COMP + UMI_DAT_MSG + 0)*/
#define MC_DAT_TXFAIL_CFM              (K_MSG_TYPE)0x1740 /*(MSG_CFM + MSG_UMI_COMP + UMI_DAT_MSG + 0)*/

#define MC_DAT_RXDATA_IND              (K_MSG_TYPE)0x3141 /*(MSG_IND + MSG_UMI_COMP + UMI_DAT_MSG + 1)*/
#define UM_DAT_RXDATA_RES              (K_MSG_TYPE)0x2141 /*(MSG_RES + MSG_UMI_COMP + UMI_DAT_MSG + 1)*/
#define MC_DAT_RXDATA_IND_SND          (K_MSG_TYPE)0x3841 /*(MSG_IND + MSG_UMI_COMP + UMI_DAT_MSG + 1)*/
/* Memory Messages */
#define MC_MEM_COPY_FROM_MAC_IND       (K_MSG_TYPE)0x3080 /*(MSG_IND + MSG_UMI_COMP + UMI_MEM_MSG + 0)*/
#define UM_MEM_COPY_FROM_MAC_RES       (K_MSG_TYPE)0x2080 /*(MSG_RES + MSG_UMI_COMP + UMI_MEM_MSG + 0)*/

#define MC_MEM_COPY_TO_MAC_IND         (K_MSG_TYPE)0x3081 /*(MSG_IND + MSG_UMI_COMP + UMI_MEM_MSG + 1)*/
#define UM_MEM_COPY_TO_MAC_RES         (K_MSG_TYPE)0x2081 /*(MSG_RES + MSG_UMI_COMP + UMI_MEM_MSG + 1)*/

/* Debug Messages */
#define UM_DBG_RESET_STATISTICS_REQ    (K_MSG_TYPE)0x06C0 /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 0)*/
#define MC_DBG_RESET_STATISTICS_CFM    (K_MSG_TYPE)0x16C0 /*(MSG_CFM + MSG_UMI_COMP + UMI_DBG_MSG + 0)*/

#define UM_DBG_GET_STATISTICS_REQ      (K_MSG_TYPE)0x06C1 /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 1)*/
#define MC_DBG_GET_STATISTICS_CFM      (K_MSG_TYPE)0x16C1 /*(MSG_CFM + MSG_UMI_COMP + UMI_DBG_MSG + 1)*/

#define UM_DBG_INPUT_REQ               (K_MSG_TYPE)0x06C2 /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 2)*/
#define MC_DBG_INPUT_CFM               (K_MSG_TYPE)0x16C2 /*(MSG_CFM + MSG_UMI_COMP + UMI_DBG_MSG + 2)*/

#define MC_DBG_OUTPUT_IND              (K_MSG_TYPE)0x35C3 /*(MSG_IND + MSG_UMI_COMP + UMI_DBG_MSG + 3)*/
#define UM_DBG_OUTPUT_RES              (K_MSG_TYPE)0x25C3 /*(MSG_RES + MSG_UMI_COMP + UMI_DBG_MSG + 3)*/

#define UM_DBG_C100_IN_REQ             (K_MSG_TYPE)0x06C4 /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 4)*/
#define MC_DBG_C100_IN_CFM             (K_MSG_TYPE)0x16C4 /*(MSG_CFM + MSG_UMI_COMP + UMI_DBG_MSG + 4)*/

#define MC_DBG_C100_OUT_IND            (K_MSG_TYPE)0x35C5 /*(MSG_IND + MSG_UMI_COMP + UMI_DBG_MSG + 5)*/
#define UM_DBG_C100_OUT_RES            (K_MSG_TYPE)0x25C5 /*(MSG_RES + MSG_UMI_COMP + UMI_DBG_MSG + 5)*/

#define UM_DBG_MAC_WATCHDOG_REQ        (K_MSG_TYPE)0x06C6 /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 57)*/
#define MC_DBG_MAC_WATCHDOG_CFM        (K_MSG_TYPE)0x16C6 /*(MSG_CFM + MSG_UMI_COMP + UMI_DBG_MSG + 57)*/


/***************************************************************************/
/***                          Management Messages                        ***/
/***************************************************************************/

typedef struct _UMI_MAC_VERSION
{
    uint8 u8Length;
    uint8 reserved[3];
    char  acVer[MTLK_PAD4(32 + 1)]; /* +1 allows zero termination for debug output */
} __MTLK_PACKED UMI_MAC_VERSION;


/***************************************************************************
**
** NAME         UM_MAN_RESET_REQ
**
** PARAMETERS   none
**
** DESCRIPTION  This message should be sent to disable the MAC. After sending
**              the message, ownership of the message buffer passes to the MAC
**              which will respond in due course to indicate the result of the
**              operation. This message is will only be successful when the
**              MAC is in the UMI_MAC_ENABLED state. After receiving the
**              message, the MAC will abort all transmit, receive or scan
**              operations in progress and return any associated buffers.
**
****************************************************************************/


/***************************************************************************
**
** NAME         MC_MAN_RESET_CFM
**
** PARAMETERS   u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_NOT_ENABLED
**
** DESCRIPTION  Response to request to reset the MAC.
**
****************************************************************************/
typedef struct _UMI_RESET
{
    uint16 u16Status;
    uint8  reserved[2];
} __MTLK_PACKED UMI_RESET;

/***************************************************************************
**
** NAME         UMI_POWER_MODE_REQ
**
** PARAMETERS   u16PowerMode        0 - Always powered on
**                                  1 - Power saving
**                                  2 - Dynamic selection
**
** DESCRIPTION  Station Only, change the Power saving mode request.
**              REVISIT - REPLACE MAGIC NUMBERS WITH DEFINES!
**
****************************************************************************/
typedef struct _UMI_POWER_MODE
{
    uint16 u16Status;
    uint16 u16PowerMode;
} __MTLK_PACKED UMI_POWER_MODE;


/***************************************************************************
**
** NAME         UMI_POWER_MODE_CFM
**
** PARAMETERS   u16PowerMode        0 - Always powered on
**                                  1 - Power saving
**                                  2 - Dynamic selection
**              u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_NOT_ENABLED
**
**
** DESCRIPTION  Station Only, change the Power saving mode confirmation.
**
****************************************************************************/

/* UMI_POWER_MODE (above) */


/***************************************************************************
**
** NAME         MC_MAN_READY_IND
**
** PARAMETERS   u32Magic        if UMI_READY_MAGIC, indicates following parameters
**              (little endian) are valid else it's legacy and they're not
**
**              u32MibSoftwareConfig   The MIB variable MIB_SOFTWARE_CONFIG
**              (little endian)
**
**              u32BoardStateAddressOffset    The address offset in shared RAM where the
**              (little endian)               status of the board can be read
**
** DESCRIPTION  Indication to the MAC Client that the MAC has completed its
**              initialisation phase and is ready to start or join a network.
**
**              The parameters to this message are used by the internal TTPCom
**              regression testing system.
**
****************************************************************************/
#define UMI_READY_MAGIC     0x98765432

typedef struct _UMI_READY
{
    uint32 u32Magic;
    uint32 u32MibSoftwareConfig;
#if defined(PLATFORM_FPGA_TTPCOM_SINGULLAR)
    uint32 u32BoardStateAddressOffset;
#endif
} __MTLK_PACKED UMI_READY;




/***************************************************************************
**
** NAME         MC_MAN_READY_RES
**
** PARAMETERS   none
**
** DESCRIPTION  Response to indication
**
****************************************************************************/


/***************************************************************************
**
** NAME         UM_MAN_SET_MIB_REQ
**
** PARAMETERS   u16ObjectID         ID of the MIB Object to be set
**              uValue              Value to which the MID object should be
**                                  set.
**
** DESCRIPTION  A request to the Upper MAC to set the value of a Managed
**              Object in the MIB.
**
****************************************************************************/
typedef struct _UMI_MIB
{
    uint16    u16ObjectID;        /* ID of the MIB Object to be set */
    uint16    u16Status;          /* Status of request - confirms only */
    MIB_VALUE uValue;             /* New value for object */
} __MTLK_PACKED UMI_MIB;


/***************************************************************************
**
** NAME         MC_MAN_SET_MIB_CFM
**
** PARAMETERS   u16ObjectID         ID of the MIB Object that was to be set
**              u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_UNKNOWN_OBJECT
**                                  UMI_BAD_VALUE
**                                  UMI_NOT_NOW
**                                  UMI_READ_ONLY
**                                  UMI_RATE_MISMATCH
**
** DESCRIPTION  Response to set MIB request.
**
****************************************************************************/

/* UMI_MIB (above) */


/***************************************************************************
**
** NAME         UM_MAN_GET_MIB_REQ
**
** PARAMETERS   u16ObjectID         ID of the MIB Object to be retrieved.
**
** DESCRIPTION: This message should be sent to retrieve the value of a
**              Managed Object in the MIB.
**
****************************************************************************/

/* UMI_MIB (above) */



/***************************************************************************
**
** NAME         UM_MAN_SCAN_REQ
**
** PARAMETERS   u16BSStype          UMI_BSS_INFRA
**                                  UMI_BSS_INFRA_PCF
**                                  UMI_BSS_ADHOC
**                                  UMI_BSS_ANY
**
** DESCRIPTION: This message should be sent to request a scan of all
**              available BSSs. The BSS type parameter instructs the Upper
**              MAC only to report BSSs matching the specified type.
**
****************************************************************************/
typedef struct _UMI_SCAN_HDR
{
    IEEE_ADDR   sBSSID;
    uint8       u8BSStype;
    uint8       u8ProbeRequestRate;
    uint16      u16MinScanTime;
    uint16      u16MaxScanTime;
    MIB_ESS_ID  sSSID;
    uint8       u8NumChannels;
    uint8       u8NumProbeRequests;
    uint16      u16Status;
} __MTLK_PACKED UMI_SCAN_HDR;

/***************************************************************************
**
** NAME         UMI_PS
**
** PARAMETERS   PS_Mode          PS_REQ_MODE_ON
**                               PS_REQ_MODE_OFF
**
** DESCRIPTION: This message should be sent to request a chnage in 
**              power management mode where PS_Mode specifies On or Off request
**
****************************************************************************/

typedef struct _UMI_PS
{
	uint8  PS_Mode;
	uint8  status;
	uint16 reserved;
} __MTLK_PACKED UMI_PS;

/***************************************************************************
**
** NAME         UMI_PM_UPDATE
**
** PARAMETERS   sStationID   - IEEE address
**				newPowerMode - new power mode of station
**				reserved	 - FFU
**
** DESCRIPTION: This message should be sent to request a chnage in 
**              power management mode where PS_Mode specifies On or Off request
**
****************************************************************************/

typedef struct _UMI_PM_UPDATE
{
	IEEE_ADDR sStationID;
	uint8	  newPowerMode;
	uint8	  reserved;
} __MTLK_PACKED UMI_PM_UPDATE;


#define UMI_STATION_ACTIVE 0
#define UMI_STATION_IN_PS  1

/***************************************************************************
**
** NAME         UMI_CHANGE_POWER_STATE
**
** PARAMETERS   powerStateType - power state type to switch to
**				status         - return status
**				reserved	   - FFU
**
** DESCRIPTION: This message should be sent to request a change in RF
**              power state
**
****************************************************************************/

typedef struct
{
	uint8 TxNum;
	uint8 RxNum;
	uint8 status;
	uint8 reserved;
} __MTLK_PACKED UMI_CHANGE_POWER_STATE;

/***************************************************************************
**
** NAME         MC_MAN_SCAN_CFM
**
** PARAMETERS   u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_BAD_PARAMETER
**                                  UMI_BSS_ALREADY_ACTIVE
**                                  UMI_NOT_ENABLED
**                                  UMI_OUT_OF_MEMORY
**
** DESCRIPTION  Confirms the completion of a scan.
**
****************************************************************************/

/* UMI_SCAN */



/***************************************************************************
**
** NAME         UM_MAN_ACTIVATE_REQ
**
** PARAMETERS   sBSSID              The ID which identifies the Network to
**                                  be created or connected to. If the node
**                                  is a Infrastructure Station and a null
**                                  MAC Address is specified then the
**                                  request is interpreted to mean join any
**                                  suitable network.
**              sSSID               The Service Set Identifier of the ESS
**              sRSNie              RSN Information Element
**
** DESCRIPTION  Activate Request. This request should be sent to the Upper
**              MAC to start or connect to a network.
**
*****************************************************************************/
#define UMI_SC_BAND_MAX_LEN 32

/* RSN Information Element */
typedef struct _UMI_RSN_IE
{
    uint8   au8RsnIe[MTLK_PAD4(UMI_RSN_IE_MAX_LEN)];
} __MTLK_PACKED UMI_RSN_IE;

typedef struct _UMI_SUPPORTED_CHANNELS_IE
{
    uint8 asSBand[MTLK_PAD4(UMI_SC_BAND_MAX_LEN*2)]; // even bytes = u8FirstChannelNumber (0,2,4,...)
                                                     // odd bytes  = u8NumberOfChannels   (1,3,5,...)
}__MTLK_PACKED UMI_SUPPORTED_CHANNELS_IE;

typedef struct _UMI_ACTIVATE_HDR
{
    IEEE_ADDR  sBSSID;
    uint16     u16Status;
    uint16     u16RestrictedChannel;
    uint16     u16BSStype;
    MIB_ESS_ID sSSID;
    UMI_RSN_IE sRSNie;              /* RSN Specific Parameter */
} __MTLK_PACKED UMI_ACTIVATE_HDR;

/***************************************************************************
**
** NAME         MC_MAN_ACTIVATE_CFM
**
** PARAMETERS   u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_BAD_PARAMETER
**                                  UMI_BSS_ALREADY_ACTIVE
**                                  UMI_NOT_ENABLED
**                                  UMI_BSS_UNKNOWN
**
** DESCRIPTION: Confirmation of an activate request.
**
****************************************************************************/

/* UMI_ACTIVATE */


/***************************************************************************
**
** NAME         MC_MAN_NETWORK_EVENT_IND
**
** PARAMETERS   sBSSID              The ID which identifies this Network
**              u16BSSstatus        UMI_BSS_CREATED
**                                  UMI_BSS_CONNECTED
**                                  UMI_BSS_DISCONNECTED
**                                  UMI_BSS_ID_CHANGED
**                                  UMI_BSS_LOST
**                                  UMI_BSS_FAILED
**              u16CFflag           Infrastructure Connection only -
**                                  indicates whether the Access Point has a
**                                  Point Coordinator function:
**                                  UMI_HAS_PCF
**                                  UMI_NO_PCF
**              u16Reason           Disconnection only - indicates the reason
**                                  for disassociation as defined in the
**                                  802.11a specification.
**
** DESCRIPTION  Network Event Indication. This message will be sent by the
**              Upper MAC to indicate changes in condition of the Network.
**
****************************************************************************/
typedef struct _UMI_NETWORK_EVENT
{
    IEEE_ADDR sBSSID;
    uint16    u16BSSstatus;
    uint16    u16CFflag;
    uint16    u16Reason;
} __MTLK_PACKED UMI_NETWORK_EVENT;


/***************************************************************************
**
** NAME         UM_MAN_NETWORK_EVENT_RES
**
** PARAMETERS   none
**
** DESCRIPTION  Return message buffer
**
****************************************************************************/


/***************************************************************************
**
** NAME         MC_MAN_CONNECTION_EVENT_IND
**
** PARAMETERS   u16Event            UMI_CONNECTED
**                                  UMI_RECONNECTED
**                                  UMI_DISCONNECTED
**              sStationID          The MAC Address of the station to which
**                                  the event applies.
**              sPrevBSSID          Reconnection only - indicates the
**                                  previous BSS that the station was
**                                  associated with.
**              u16Reason           Disconnection only - indicates the reason
**                                  for disassociation as defined in the
**                                  802.11a specification.
**              u16RSNmode          Specified if connecting station supports RSN
**              sRSNie              RSN Information Element
**
** DESCRIPTION  Connection Event Indication. This message is sent to indicate
**              when Stations connect, disconnect and reconnect to the
**              network.
**
****************************************************************************/
typedef struct _UMI_CONNECTION_EVENT
{
    uint16     u16Event;
    IEEE_ADDR  sStationID;
    IEEE_ADDR  sPrevBSSID;
    uint16     u16Reason;
    uint16     u16RSNmode;
    uint8      u8HTmode;
    uint8      u8PeerAP;
    UMI_RSN_IE sRSNie;
    UMI_SUPPORTED_CHANNELS_IE sSupportedChannelsIE;
	uint32	   u32SupportedRates;
} __MTLK_PACKED UMI_CONNECTION_EVENT;


/***************************************************************************
**
** NAME         UM_MAN_SET_PEERAP_REQ
**
** PARAMETERS   sStationID     IEEE address (BSSID of the peer AP)
**              u16Status      UMI_OK
**                             UMI_OUT_OF_MEMORY (no more peer APs allowed)
**                             UMI_BAD_VALUE (drop peer AP)
**
** DESCRIPTION: This message is sent when adding (or removing if u16Status
**              is set to UMI_BAD_VALUE) peer APs
**
****************************************************************************/

typedef struct _UMI_PEERAP
{
    uint16    u16Status;
    IEEE_ADDR sStationID;
} __MTLK_PACKED UMI_PEERAP;


/***************************************************************************
**
** NAME         UM_MAN_CONNECTION_EVENT_RES
**
** PARAMETERS   none
**
** DESCRIPTION  Returns message buffer
**
****************************************************************************/



/***************************************************************************
**
** NAME         MC_MAN_MAC_EVENT_IND
**
** PARAMETERS
**
** DESCRIPTION  MAC Event Indication. This message will be sent by the
**              MAC ( upper or lower ) to indicate the exception occurrence.
**
****************************************************************************/
typedef struct _UMI_MAC_EVENT
{
    uint32  u32CPU;                 /* Upper or Lower MAC */
    uint32  u32CauseReg;            /* Cause register */
    uint32  u32EPCReg;              /* EPC register */
    uint32  u32StatusReg;           /* Status register */
} __MTLK_PACKED UMI_MAC_EVENT;

/***************************************************************************
**
** NAME         MC_MAN_MAC_EVENT_RES
**
** PARAMETERS   none
**
** DESCRIPTION  Returns message buffer
**
****************************************************************************/

/* Kalish */

/***************************************************************************
**
** NAME         MC_MAN_DYNAMIC_PARAM_IND
**
** PARAMETERS   ACM_StateTable
**
** DESCRIPTION
**
****************************************************************************/
typedef struct _UMI_DYNAMIC_PARAM_TABLE
{
    uint8 ACM_StateTable[MTLK_PAD4(MAX_USER_PRIORITIES)];
    /* This table is implemented in a STA */
    /* it refers to the ACM bit which arrives from the AP*/
    /* The structure of the Array is [ AC ACM state, ...] this repeats itself four.*/
} __MTLK_PACKED UMI_DYNAMIC_PARAM_TABLE;


/***************************************************************************
**
** NAME         UM_MAN_DISCONNECT_REQ
**
** PARAMETERS   sStationID          Address of station (AP only).
**
** DESCRIPTION  Disconnect Request. This message is only sent within Access
**              Points to request disassociation of a Station from the BSS.
**
****************************************************************************/
typedef struct _UMI_DISCONNECT
{
    IEEE_ADDR sStationID;
    uint16    u16Status;
} __MTLK_PACKED UMI_DISCONNECT;


/***************************************************************************
**
** NAME         MC_MAN_DISCONNECT_CFM
**
** PARAMETERS   sStationID          Address of station (AP only).
**              u16Status           UMI_OK
**                                  UMI_STATION_NOT_FOUND
**
** DESCRIPTION  Disconnect Confirm
**
****************************************************************************/

/* UMI_DISCONNECT */

/***************************************************************************
**
** NAME         UMI_GET_CONNECTION_STATUS
**
** PARAMETERS   u8DeviceIndex        Index of first device to report in sDeviceStatus[].
**
** DESCRIPTION  Query various connection information.
**              Query global noise, channel load and info about associated devices.
**              For STA there is only one connected device - sDeviceStatus[0] - is AP.
**              As far as number of connected devices can exceed DEVICE_STATUS_ARRAY_SIZE
**              remaining info can be accessed with consequent requests.
**              On first request u8DeviceIndex should be set to zero.
**              On response MAC sets u8DeviceIndex to index in it's STA db,
**              or to zero if all information already reported.
**              If u8DeviceIndex reported by MAC isn't zero, MAC can be 
**              queried again for remaining info with reported u8DeviceIndex.
**              In u8NumOfDeviceStatus MAC reports number of actually written
**              entries in sDeviceStatus[].
**
****************************************************************************/

#define NUM_OF_RX_ANT 3
#define DEVICE_STATUS_ARRAY_SIZE 16

typedef struct _DEVICE_STATUS
{
     IEEE_ADDR      sMacAdd;
     uint16         u16TxRate;
     uint8          u8NetworkMode;
     uint8          au8RSSI[NUM_OF_RX_ANT];
} __MTLK_PACKED DEVICE_STATUS;

typedef struct _UMI_GET_CONNECTION_STATUS
{
     uint8          u8DeviceIndex;
     uint8          u8NumOfDeviceStatus;
     uint8          u8GlobalNoise;
     uint8          u8ChannelLoad;
     DEVICE_STATUS  sDeviceStatus[DEVICE_STATUS_ARRAY_SIZE];
} __MTLK_PACKED UMI_GET_CONNECTION_STATUS;

/***************************************************************************
**
** NAME         UM_MAN_GET_RSSI_REQ
**
** PARAMETERS   sStationID          The MAC Address for which an RSSI value
**                                  is desired.
**              u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_STATION_UNKNOWN
**
** DESCRIPTION  Get Receive Signal Strength Request
**
****************************************************************************/
typedef struct _UMI_GET_RSSI
{
    IEEE_ADDR sStationID;
    uint16    u16Status;
    uint16    u16RSSIvalue;
    uint8     reserved[2];
} __MTLK_PACKED UMI_GET_RSSI;


/***************************************************************************
**
** NAME         MC_MAN_GET_RSSI_CFM
**
** PARAMETERS   sStationID          The MAC Address for which an RSSI value
**                                  is required.
**              u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_STATION_UNKNOWN
**              u8RSSIvalue         The reported RSSI value.
**
** DESCRIPTION  Get Receive Signal Strength Confirm
**
****************************************************************************/

/* UMI_GET_RSSI */

/***************************************************************************
**
** NAME         UM_MAN_SET_CHANNEL_LOAD_REQ
**
** 
** DESCRIPTION  Set channel load request
**
****************************************************************************/
typedef struct _UMI_SET_CHANNEL_LOAD_VAR
{
    /* alpha-filter coefficient */
    uint8     uAlphaFilterCoefficient;
    /* channel load threshold, % */
    uint8     uChannelLoadThreshold;
    uint8     uReserved[2];
} __MTLK_PACKED UMI_SET_CHANNEL_LOAD_VAR;




/***************************************************************************
**                     SECURITY MESSAGES BEGIN                            **
***************************************************************************/

/***************************************************************************
**
** NAME         UM_MAN_SET_KEY_REQ
**
** PARAMETERS   u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_STATION_UNKNOWN
**              u16KeyType          Pairwise or group key
**              sStationID          MAC address of station
**              u16StationRole      Authenticator or supplicant
**              u16CipherSuite      Cipher suite selector
**              u16DefaultKeyIndex  For legacy WEP modes
**              au8RxSeqNum         Initial RX sequence number (little endian)
**              au8TxSeqNum         Initial TX sequence number (little endian)
**              au8Tk1              Temporal key 1
**              au8Tk2              Temporal key 2
**
** DESCRIPTION  Sets the temporal encryption key for the specified station
**
****************************************************************************/
typedef struct _UMI_SET_KEY
{
    uint16      u16Status;
    uint16      u16KeyType;
    IEEE_ADDR   sStationID;
    uint16      u16StationRole;
    uint16      u16CipherSuite;
    uint16      u16DefaultKeyIndex;
    uint8       au8RxSeqNum[MTLK_PAD4(UMI_RSN_SEQ_NUM_LEN)];
    uint8       au8TxSeqNum[MTLK_PAD4(UMI_RSN_SEQ_NUM_LEN)];
    uint8       au8Tk1[MTLK_PAD4(UMI_RSN_TK1_LEN)];
    uint8       au8Tk2[MTLK_PAD4(UMI_RSN_TK2_LEN)];
} __MTLK_PACKED UMI_SET_KEY;

/***************************************************************************
**
** NAME         UM_MAN_CLEAR_KEY_REQ
**
** PARAMETERS   u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_STATION_UNKNOWN
**              u16KeyType          Pairwise or group key
**              sStationID          MAC address of station
**
** DESCRIPTION  Clears the temporal encryption key for the specified station
**
****************************************************************************/
typedef struct _UMI_CLEAR_KEY
{
    uint16      u16Status;
    uint16      u16KeyType;
    IEEE_ADDR   sStationID;
    uint8       reserved[2];
} __MTLK_PACKED UMI_CLEAR_KEY;

/***************************************************************************
**
** NAME         UM_MAN_GET_GROUP_PN_REQ
**
** PARAMETERS   UMI_GROUP_PN: empty structure to be filled on CFM
**
** DESCRIPTION  Requests the group transmit security sequence number
**
****************************************************************************/

/***************************************************************************
**
** NAME         UM_MAN_GET_GROUP_PN_CFM
**
** PARAMETERS   u16Status            UMI_OK
**                                   UMI_NOT_INITIALISED
**              au8TxSeqNum[6]       Group transmit security sequence count
**
** DESCRIPTION  Sends the group transmit security sequence number to higher
**              layer
**
****************************************************************************/
typedef struct _UMI_GROUP_PN
{
    uint16      u16Status;
    uint8       reserved[2];
    uint8       au8TxSeqNum[MTLK_PAD4(UMI_RSN_SEQ_NUM_LEN)];
} __MTLK_PACKED UMI_GROUP_PN;

/***************************************************************************
**
** NAME         UM_MAN_SECURITY_ALERT_IND
**
** PARAMETERS   u16Status           Event code - see rsn.h
**              u16KeyType          Pairwise or group key
**              sStationID          MAC address of station
**
** DESCRIPTION  Alerts higher layers that a security related event has
**              occurred that may need acting on by re-keying or
**              disconnecting
**
****************************************************************************/
typedef struct _UMI_SECURITY_ALERT
{
    uint16      u16EventCode;
    uint16      u16KeyType;
    IEEE_ADDR   sStationID;
    uint8       reserved[2];
} __MTLK_PACKED UMI_SECURITY_ALERT;

/***************************************************************************
**                        SECURITY MESSAGES END                           **
***************************************************************************/


/***************************************************************************
**
** NAME         UM_MAN_SET_BCL_VALUE/UM_MAN_QUERY_BCL_VALUE
**
** PARAMETERS   Unit
**              Address
**              Size
**              Data
**
** DESCRIPTION  Sets/queries BCL data from MAC
**
****************************************************************************/
#define MAX_GENERIC_REQ_DATA                    64

typedef struct _UMI_BCL_REQUEST
{
    uint32         Unit;
    uint32         Address;
    uint32         Size;
    uint32         Data[MAX_GENERIC_REQ_DATA];
} __MTLK_PACKED UMI_BCL_REQUEST;


/* Kalish */

/***************************************************************************
**
** NAME         UM_MAN_OPEN_AGGR_REQ
**
** PARAMETERS   sDA
**              u16AccessProtocol   - former u16Priority
**              pad
**              u16MaxNumOfPackets
**              u32MaxNumOfBytes
**              u32TimeoutInterval
**              u32MinSizeOfPacketInAggr
**
**              u16Index - removed
**
** DESCRIPTION  Directive to set the aggregation parameters
**
****************************************************************************/
typedef struct _UMI_OPEN_AGGR_REQ
{
    IEEE_ADDR   sDA;
    uint16      u16AccessProtocol;
    uint16      u16MaxNumOfPackets;
    uint8       reserved[2];
    uint32      u32MaxNumOfBytes;
    uint32      u32TimeoutInterval;
    uint32      u32MinSizeOfPacketInAggr;
	uint32 		windowSize;
} __MTLK_PACKED UMI_OPEN_AGGR_REQ;

/***************************************************************************
**
** NAME         UM_MAN_CLOSE_AGGR_REQ
**
** PARAMETERS   sDA
**              u16AccessProtocol
**
** DESCRIPTION  Directive to set the aggregation parameters
**
****************************************************************************/
typedef struct _UMI_CLOSE_AGGR_REQ
{
    IEEE_ADDR   sDA;
    uint16      u16AccessProtocol;
} __MTLK_PACKED UMI_CLOSE_AGGR_REQ;

typedef struct _UMI_ADDBA_REQ_SEND
{
    IEEE_ADDR   sDA;
    uint8       u8DialogToken;
    uint8       u8BA_WinSize_O;
    uint16      u16AccessProtocol;
    uint16      u16BATimeout;
} __MTLK_PACKED UMI_ADDBA_REQ_SEND;

typedef struct _UMI_ADDBA_REQ_RCV
{
    IEEE_ADDR   sSA;                /* Transmitter Address RA*/
    uint8       u8DialogToken;
    uint8       u8WinSize;          /* Extracted from the ADDBA request */
    uint16      u16AccessProtocol;  /* TID */
    uint16      u16StartSN;         /* set to SSN value extracted from the ADDBA request */
    uint16      u16AckPolicy;       /* Ack Policy*/
    uint16      u16BATimeout;       /* Timeout */
} __MTLK_PACKED UMI_ADDBA_REQ_RCV;

typedef struct _UMI_ADDBA_RES_SEND
{
    IEEE_ADDR   sDA;                /* Receiver Address TA*/
    uint8       u8DialogToken;
    uint8       u8WinSize;          /* Actual buffer size */
    uint16      u16AccessProtocol;  /* TID */
    uint16      u16ResultCode;      /* Response Status */
    uint16      u16BATimeout;
    uint8       reserved[2];
} __MTLK_PACKED UMI_ADDBA_RES_SEND;

typedef struct _UMI_ADDBA_RES_RCV
{
    IEEE_ADDR   sSA;                /* Transmitter Address RA*/
    uint8       u8DialogToken;
    uint8       reserved[1];
    uint16      u16AccessProtocol;  /* TID */
    uint16      u16ResultCode;      /* Response status */
} __MTLK_PACKED UMI_ADDBA_RES_RCV;

typedef struct _UMI_DELBA_REQ_SEND
{
    IEEE_ADDR   sDA;                /* Receiver Address TA*/
    uint16      u16Intiator;        /* ADDBA agreement side 1- Stop aggregation that we initiate
                                    0 -Stop aggregation from connected side */
    uint16      u16AccessProtocol;  /* TID */
    uint16      u16ResonCode;       /* Response Status */
} __MTLK_PACKED UMI_DELBA_REQ_SEND;

typedef struct _UMI_DELBA_REQ_RCV
{
    IEEE_ADDR   sSA;                /* Transmitter Address RA*/
    uint16      u16Intiator;        /* ADDBA agreement side 0- Stop aggregation that we initiate
                                       1 -Stop aggregation from connected side */
    uint16      u16AccessProtocol;  /* TID */
    uint16      u16ResultCode;      /* Response Status */
} __MTLK_PACKED UMI_DELBA_REQ_RCV;

typedef struct _UMI_ACTION_FRAME_GENERAL_SEND
{
    IEEE_ADDR   sDA;                /* Receiver Address TA*/
    uint8       u8ActionCode;
    uint8       u8DialogToken;
} __MTLK_PACKED UMI_ACTION_FRAME_GENERAL_SEND;


/***************************************************************************
**
** NAME         MC_MAN_BAR_IND
**
** PARAMETERS   u16AccessProtocol   This field is used to transfer
**                                  TID (Traffic ID) of the traffic stream (TS)
**              sSA                 Transmitter Source Ethernet Address,
**                                  used as Transmitter Address (TA)
**              u16SSN              Set to SSN value extracted from the BAR MSDU
**
** DESCRIPTION  Indication to the upper MAC including info on the BAR
**
****************************************************************************/
typedef struct _UMI_BAR_IND
{
    uint16      u16AccessProtocol;
    IEEE_ADDR   sSA;
    uint16      u16SSN;
    uint8       reserved[2];
} __MTLK_PACKED UMI_BAR_IND;

/***************************************************************************
**
** NAME         UM_DAT_BAR_RES
**
** PARAMETERS   None
**
** DESCRIPTION  MC response upon receiving BAR indication
**
****************************************************************************/



/***************************************************************************
**
** NAME         UMI_GENERIC_MAC_REQUEST
**
** PARAMETERS   none
**
** DESCRIPTION  TODO
**
****************************************************************************/
typedef struct _UMI_GENERIC_MAC_REQUEST
{
    uint32 opcode;
    uint32 size;
    uint32 action;
    uint32 res0;
    uint32 res1;
    uint32 res2;
    uint32 retStatus;
    uint32 data[MAX_GENERIC_REQ_DATA];
} __MTLK_PACKED UMI_GENERIC_MAC_REQUEST;

#define MT_REQUEST_GET                  0
#define MT_REQUEST_SET                  1

#define EEPROM_ILLEGAL_ADDRESS          0x3
/***************************************************************************
**
** NAME         UMI_GENERIC_IE
**
** PARAMETERS
**              u8Type            IE type. Predefined types:
**                                  0(UMI_WPS_IE_BEACON) - WPS IE in beacon ;
**                                  1(UMI_WPS_IE_PROBEREQUEST) - WPS IE in probe request;
**                                  2(UMI_WPS_IE_PROBERESPONSE) - WPS IE in probe response;
**                                  3(UMI_WPS_IE_ASSOCIATIONREQUEST) - WPS IE in association request (optional);
**                                  4(UMI_WPS_IE_ASSOCIATIONRESPONSE) - WPS IE in association response (optional).
**              u8reserved[1]     Added for aligning.
**              u16Length         Size of WPS IE. If u16Length == 0 then WPS IE deleted.
**              au8IE             Whole IE.
**
** DESCRIPTION  Used in request from the driver to add WPS IE to beacons,
**              probe requests and responses
**
****************************************************************************/
#define UMI_MAX_GENERIC_IE_SIZE         257
#define UMI_WPS_IE_BEACON               0
#define UMI_WPS_IE_PROBEREQUEST         1
#define UMI_WPS_IE_PROBERESPONSE        2
#define UMI_WPS_IE_ASSOCIATIONREQUEST   3
#define UMI_WPS_IE_ASSOCIATIONRESPONSE  4

typedef struct _UMI_GENERIC_IE
{
    uint8  u8Type;
    uint8  u8reserved[1];
    uint16 u16Length;
    uint8  au8IE[UMI_MAX_GENERIC_IE_SIZE];
} __MTLK_PACKED UMI_GENERIC_IE;

/***************************************************************************
**
** NAME         UM_MAN_SET_LED_REQ
**
** PARAMETERS   u8BasebLed - 
**              u8LedStatus - 
**              reserved
**
** DESCRIPTION  TODO
**
****************************************************************************/
typedef struct _UMI_SET_LED
{
    uint8 u8BasebLed;
	uint8 u8LedStatus;
	uint8 reserved[2];
} __MTLK_PACKED UMI_SET_LED;


typedef struct _UMI_DEF_RF_MGMT_DATA
{
	uint8     u8Data;        /* set: IN - a RF Management data (depend on RF MGMT type), OUT - ignored 
						      * get: IN - ignored, OUT - a RF Management data (depend on RF MGMT type) */
	uint8     u8Status;      /* set & get: IN - ignored, OUT - a UMI_STATUS error code */
	uint8     u8Reserved[2]; /* set & get: IN - ignored, OUT - ignored */
} __MTLK_PACKED UMI_DEF_RF_MGMT_DATA;

typedef struct _UMI_RF_MGMT_TYPE
{
	uint8  u8RFMType;  /* set: IN - a MTLK_RF_MGMT_TYPE_... value, OUT - ignored
						* get: IN - ignored, OUT - a MTLK_RF_MGMT_TYPE_... value
						*/
	uint8  u8HWType;  /* set & get: IN - ignored, OUT - a MTLK_HW_TYPE_... value */
	uint16 u16Status; /* set & get: IN - ignored, OUT - a UMI_STATUS error code */
} __MTLK_PACKED UMI_RF_MGMT_TYPE;

/***************************************************************************
**
** NAME         UMI_CHANGE_TX_POWER_LIMIT
**
** PARAMETERS   PowerLimitOption - 
**
** DESCRIPTION: This message should be sent to request a change to the transmit
**              power limit table.
**
****************************************************************************/
typedef struct UMI_TX_POWER_LIMIT
{
	uint8 TxPowerLimitOption;
    uint8 Status;
	uint8 Reserved[2];
} __MTLK_PACKED UMI_TX_POWER_LIMIT;

/* MTLK Vendor Specific Action Frame UM_MAN_SEND_MTLK_VSAF_REQ 
 */
#define MAX_VSAF_DATA_SIZE (MAX_GENERIC_REQ_DATA * sizeof(uint32))

typedef struct _UMI_VSAF_INFO
{
  IEEE_ADDR sDA;
  uint16    u16Size;
  uint8     u8RFMgmtData;
  uint8		u8Status;
  uint8		u8Rank;//Num of spatial streams 
  uint8     u8Reserved[1];
  /**********************************************************************
   * NOTE: u8Category and au8OUI are added for the MAC convenience.
   *       They are constants and should be always set by the driver to:
   *        - u8Category = ACTION_FRAME_CATEGORY_VENDOR_SPECIFIC
   *        - au8Data    = { MTLK_OUI_0, MTLK_OUI_1, MTLK_OUI_2 }
   **********************************************************************/
  uint8     u8Category;
  uint8     au8OUI[3]; /* */
  /**********************************************************************/
  uint8     au8Data[MTLK_PAD4(MAX_VSAF_DATA_SIZE)];
} __MTLK_PACKED UMI_VSAF_INFO;

/***************************************************************************
**
** NAME         UMI_MAC_WATCHDOG
**
** PARAMETERS   none
**
** DESCRIPTION  MAC Soft Watchdog
**
****************************************************************************/

typedef struct _UMI_MAC_WATCHDOG
{
	uint8  u8Status;  /* WD Status */
	uint8  u8Reserved[1];
	uint16 u16Timeout; /* Timeout for waiting answer from LM in milliseconds*/
} __MTLK_PACKED UMI_MAC_WATCHDOG;

/***************************************************************************/
/***                           Debug Messages                            ***/
/***************************************************************************/

/***************************************************************************
**
** NAME         UM_DBG_RESET_STATISTICS_REQ
**
** PARAMETERS   none
**
** DESCRIPTION  This message should be sent to reset the statistics
**              maintained by the MAC software to their default values. The
**              MAC will confirm that the request has been processed with a
**              MC_MAN_RESET_STATISTICS_CFM.
**
****************************************************************************/


/***************************************************************************
**
** NAME         MC_DBG_RESET_STATISTICS_CFM
**
** PARAMETERS   u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**
** DESCRIPTION  Response to reset statistics request.
**
****************************************************************************/
typedef struct _UMI_RESET_STATISTICS
{
    uint16 u16Status;
    uint8  reserved[2];
} __MTLK_PACKED UMI_RESET_STATISTICS;


/***************************************************************************
**
** NAME         UM_DBG_GET_STATISTICS_REQ
**
** PARAMETERS   u16Status           Not used
**              u16Ident            REVISIT - what is this?
**              sStats              Structure containing statistics.
**
** DESCRIPTION  This message should be sent to request a copy of the current
**              set of statistics maintained by the MAC software.
**
****************************************************************************/
/* MAC statistics counters */
typedef struct _UMI_STATISTICS
{
    uint32 au32Statistics[STAT_TOTAL_NUMBER];
} __MTLK_PACKED UMI_STATISTICS;

typedef struct _UMI_GET_STATISTICS
{
    uint16         u16Status;
    uint16         u16Ident;
    UMI_STATISTICS sStats;
} __MTLK_PACKED UMI_GET_STATISTICS;


/***************************************************************************
**
** NAME         MC_DBG_GET_STATISTICS_CFM
**
** PARAMETERS   u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**
** DESCRIPTION  Response to get statistics request to return message buffer.
**
****************************************************************************/

/* UMI_GET_STATISTICS */


/***************************************************************************
**
** NAME         UM_DBG_INPUT_REQ
**
** PARAMETERS   u16Length           The number of bytes of input stream
**                                  contained in this message.
**              au8Data             An array of characters containing a
**                                  section of debug input stream.
**
** DESCRIPTION  Debug Input Request
**
****************************************************************************/
typedef struct _UMI_DEBUG
{
    uint16 u16Length;
    uint16 u16stream;
    uint8  au8Data[MTLK_PAD4(UMI_DEBUG_DATA_SIZE)];
} __MTLK_PACKED UMI_DEBUG;


/***************************************************************************
**
** NAME         MC_DBG_INPUT_CFM
**
** PARAMETERS   none
**
** DESCRIPTION  Return message buffer.
**
****************************************************************************/


/***************************************************************************
**
** NAME         MC_DBG_OUTPUT_IND
**
** PARAMETERS   pau8Addr            The address in shared memory of a section
**                                  of output stream.
**              u16Length           The number of bytes of output stream to
**                                  be read.
**
** DESCRIPTION  Debug Output Indication
**
****************************************************************************/

/* UMI_DEBUG */


/***************************************************************************
**
** NAME         UM_DBG_OUTPUT_RES
**
** PARAMETERS   none
**
** DESCRIPTION  Returns message buffer.
**
****************************************************************************/


/***************************************************************************
**
** NAME         UM_DBG_C100_IN_REQ
**
** PARAMETERS   u16Length           The number of bytes of the C100 stream
**                                  contained in this message.
**              au8Data             An array of characters containing a
**                                  section of C100 input stream.
**
** DESCRIPTION  C100 Input Request
**
****************************************************************************/
/* Offsets of message type in message format from/to C100 */
typedef struct _C100_MSG_HEADER
{
    uint8   u8Task;
    uint8   u8Instance;
    uint16  u16MsgId;
} __MTLK_PACKED C100_MSG_HEADER;



typedef struct _UMI_C100
{
    uint16 u16Length;
    uint16 u16stream;
    C100_MSG_HEADER sC100hdr;
    uint8  au8Data[MTLK_PAD4(UMI_C100_DATA_SIZE)];
} __MTLK_PACKED UMI_C100;


/***************************************************************************
**
** NAME         MC_DBG_C100_IN_CFM
**
** PARAMETERS   none
**
** DESCRIPTION  Return message buffer.
**
****************************************************************************/


/***************************************************************************
**
** NAME         MC_DBG_C100_OUT_IND
**
** PARAMETERS   pau8Addr            The address in shared memory of a section
**                                  of C100 output stream.
**              u16Length           The number of bytes of output stream to
**                                  be read.
**
** DESCRIPTION  C100 Output Indication
**
****************************************************************************/

/* UMI_C100 */


/***************************************************************************
**
** NAME         UM_DBG_C100_OUT_RES
**
** PARAMETERS   none
**
** DESCRIPTION  Returns message buffer.
**
****************************************************************************/


/***************************************************************************
**
** NAME         UMI_DBG
**
** DESCRIPTION  A union of all Debug Messages.
**
****************************************************************************/
typedef union _UMI_DBG
{
    UMI_RESET_STATISTICS sResetStatistics;
    UMI_GET_STATISTICS   sGetStatistics;
    UMI_DEBUG            sDebug;
    UMI_C100             sC100;
} __MTLK_PACKED UMI_DBG;



/***************************************************************************/
/***                            Data Messages                            ***/
/***************************************************************************/

/***************************************************************************
**
** NAME         UM_DAT_TXDATA_REQ
**
** PARAMETERS   u32MSDUtag          Reference to the buffer containing the
**                                  payload of the MSDU in external memory.
**              u16MSDUlength       Length of the MSDU payload in the range
**                                  0..UMI_MAX_MSDU_LENGTH.
**              u16AccessProtocol   UMI_USE_DCF
**                                  UMI_USE_PCF
**              sSA                 Source MAC Address (AP only).
**              sDA                 Destination MAC Address.
**              sWDSA               Wireless Distribution System Address
**                                  (reserved).
**              u16Status           Not used.
**              pvMyMsdu            Reserved for use by the MAC.
**
** DESCRIPTION  Transmit Data Request
**
****************************************************************************/
/* <O.H> - Data Request Message Descriptor (TX) */

#define TX_DATA_INFO_WDS    MTLK_BFIELD_INFO(0, 1)  /*  1 bit  starting bit0 */
#define TX_DATA_INFO_TID    MTLK_BFIELD_INFO(1, 3)  /*  3 bits starting bit1 */
#define TX_DATA_INFO_LENGTH MTLK_BFIELD_INFO(4, 12) /* 12 bits starting bit4 */

#define TX_EXTRA_ENCAP_TYPE  MTLK_BFIELD_INFO(0, 7)  /* 7 LS bits */
#define TX_EXTRA_IS_SOUNDING MTLK_BFIELD_INFO(7, 1)  /* 1 MS bit  */

#define MTLK_RF_MGMT_DATA_DEFAULT 0x00

/* Values for u8PacketType in UMI_DATA_TX struct */
#define ENCAP_TYPE_RFC1042           0
#define ENCAP_TYPE_STT               1
#define ENCAP_TYPE_8022              2
#define ENCAP_TYPE_ILLEGAL           MTLK_BFIELD_VALUE(TX_EXTRA_ENCAP_TYPE, -1, uint8)

typedef struct _UMI_DATA_TX
{
    IEEE_ADDR sRA;
    uint16    u16FrameInfo; /* use FRAME_INFO_... macros for access */
    uint32    u32HostPayloadAddr;
    uint8     u8RFMgmtData;
    uint8     u8Status;
    uint8     u8Reserved;
    uint8     u8ExtraData; /* see TX_EXTRA_... for available values */
} __MTLK_PACKED UMI_DATA_TX;

typedef UMI_DATA_TX TXDAT_REQ_MSG_DESC;

/* This was the old Data Request Message Descriptor */
typedef struct _UMI_DATA_RX
{
    uint32    u32MSDUtag;
    uint16    u16MSDUlength;
	uint8     u8Notification;
	uint8     u8Offset;
    uint16    u16AccessProtocol;
    IEEE_ADDR sSA;
    IEEE_ADDR sDA;
    IEEE_ADDR sWDSA;
    mtlk_void_ptr psMyMsdu;
#if (WILD_PACKETS)
	uint32		u32PacketRate; 
#endif/*(WILD_PACKETS)*/
} __MTLK_PACKED UMI_DATA_RX;


/***************************************************************************
**
** NAME         MC_DAT_TXDATA_CFM
**
** PARAMETERS   u32MSDUtag          Reference to the buffer containing the
**                                  payload of the MSDU that was transmitted.
**              u16MSDUlength       As request.
**              u16AccessProtocol   As request.
**              sSA                 As request.
**              sDA                 As request.
**              sWDSA               As request.
**              u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_BAD_LENGTH
**                                  UMI_TX_TIMEOUT
**                                  UMI_BSS_HAS_NO_PCF
**                                  UMI_NOT_CONNECTED
**                                  UMI_NOT_INITIALISED
**
** DESCRIPTION  Transmit Data Confirm
**
****************************************************************************/

/* UMI_DATA */


/***************************************************************************
**
** NAME         RXDAT_IND_MSG_DESC (used for MC_DAT_RXDATA_IND)
**
** PARAMETERS   u32HostPayloadAddr       Reference to the payload address in the host memory
**
** DESCRIPTION  Receive Data Indication
**
****************************************************************************/
/* <O.H> - new Data Indication Message Descriptor (RX) */
typedef struct
{
    uint32 u32HostPayloadAddr;
} __MTLK_PACKED RXDAT_IND_MSG_DESC;


/***************************************************************************
**
** NAME         RX_ADDITIONAL_INFO
**
** PARAMETERS   u8EstimatedNoise
**				u8MinNoise      
**              u8Channel
**              u8RSN
**				u16Length
**				u8RxRate
**				au8Rssi - all 3 rssi
**				u8MaxRssi
**
** DESCRIPTION  Additional Receive Data Information
**
****************************************************************************/
typedef struct MAC_RX_ADDITIONAL_INFO
{
    uint8	u8EstimatedNoise; /* the estimated noise in RF (noise in BB + rx path noise gain)*/     
	uint8	u8MinNoise;   
    uint8	u8Channel;
    uint8	u8RSN; /* MAC will send here 0 or 1 */
	uint16  u16PhyRxLen;
	uint8  	u8RxRate;          
	uint8   u8Mcs;
	uint8   au8Rssi[3];
	uint8   u8MaxRssi;/* Can be calculated in driver in case we need this memory for another variable */
	
}__MTLK_PACKED MAC_RX_ADDITIONAL_INFO_T;



/***************************************************************************
**
** NAME         UM_DAT_RXDATA_RES
**
** PARAMETERS   u32MSDUtag          Reference to the RX buffer that was
**                                  received.
**              u16MSDUlength       As request.
**              u16AccessProtocol   As request.
**              sSA                 As request.
**              sDA                 As request.
**              sWDSA               As request.
**              u16Status           UMI_OK
**
** DESCRIPTION  Sent by the MAC Client in response to a MC_DAT_RXDATA_IND
**              indication. Returns the message buffer to the Upper MAC.
**
****************************************************************************/

/* UMI_DATA */


/***************************************************************************
**
** NAME         MC_DAT_ALLOC_BUFFER_IND
**
** PARAMETERS   u32MSDUtag          Not used.
**              u16MSDUlength       The size of the required buffer in bytes.
**              u16AccessProtocol   Not used.
**              sSA                 Not used.
**              sDA                 Not used.
**              sWDSA               Not used.
**              u16Status           Not used.
**
** DESCRIPTION  Request for MAC Client to allocate a buffer in external
**              memory.
**
****************************************************************************/

/* UMI_DATA */


/***************************************************************************
**
** NAME         UM_DAT_ALLOC_BUFFER_RES
**
** PARAMETERS   u32MSDUtag          Reference to the buffer in external
**                                  memory which has been allocated.
**              u16MSDUlength       Length of the buffer that was allocated.
**              u16AccessProtocol   Not used.
**              sSA                 Not used.
**              sDA                 Not used.
**              sWDSA               Not used.
**              u16Status           UMI_OK
**                                  UMI_OUT_OF_MEMORY
**
** DESCRIPTION  Response to Upper MAC request for the MAC Client to allocate
**              a buffer in external memory.
**
****************************************************************************/

/* UMI_DATA */


/***************************************************************************
**
** NAME         MC_DAT_FREE_BUFFER_IND
**
** PARAMETERS   u32MSDUtag          Reference to the buffer in external
**                                  memory to be deallocated.
**              u16MSDUlength       Not used.
**              u16AccessProtocol   Not used.
**              sSA                 Not used.
**              sDA                 Not used.
**              sWDSA               Not used.
**              u16Status           Not used.
**
** DESCRIPTION  Returns a previously allocated buffer to the MAC Client.
**
****************************************************************************/

/* UMI_DATA */


/***************************************************************************
**
** NAME         UM_DAT_FREE_BUFFER_RES
**
** PARAMETERS   none
**
** DESCRIPTION  Return message buffer
**
****************************************************************************/


/***************************************************************************
**
** NAME         UMI_MEM
**
** DESCRIPTION  A union of all Data Messages.
**
****************************************************************************/
typedef union _UMI_DAT
{
    UMI_DATA_TX sDataTx;
    UMI_DATA_RX sDataRx;
} __MTLK_PACKED UMI_DAT;



/***************************************************************************/
/***                         Memory Messages                             ***/
/***************************************************************************/

/***************************************************************************
**
** NAME         MC_MEM_COPY_FROM_MAC_IND & UM_MEM_COPY_FROM_MAC_RES
**
** PARAMETERS   pau8SourceAddr      Source address for the transfer.
**              u32DestinationTag   The destination tag for the transfer
**                                  (i.e. address only understood by the MAC
**                                  Client).
**              u16TransferOffset   Offset from Destination Address in bytes
**                                  at which the transfer should start.
**              u16TransferLength   Number of bytes to transfer.
**              u16Status           Only used in response:
**                                  UMI_OK
**                                  UMI_TRANSFER_ALREADY_ACTIVE
**                                  UMI_TRANSFER_FAILED
**
** DESCRIPTION  Request from Upper MAC to copy a block of data from the
**              specified location in the MAC memory to external memory. The
**              same message structure is used in the reply from the MAC
**              Client.
**
****************************************************************************/
typedef struct _UMI_COPY_FROM_MAC
{
    mtlk_uint8_ptr pau8SourceAddr;
    mtlk_umidata_ptr psDestinationUmiData;
    uint16 u16TransferOffset;
    uint16 u16TransferLength;
    uint16 u16Status;
    uint8  reseved[2];
} __MTLK_PACKED UMI_COPY_FROM_MAC;


/***************************************************************************
**
** NAME         MC_MEM_COPY_TO_MAC_IND & UM_MEM_COPY_TO_MAC_RES
**
** PARAMETERS   u32SourceTag        Source tag for the transfer (i.e. address
**                                  only understood by the MAC Client).
**              pau8DestinationAddr Destination address for the transfer.
**              u16TransferOffset   Offset from Destination Address in bytes
**                                  at which the transfer should start.
**              u16TransferLength   Number of bytes to transfer.
**              u16Status           Only used in response:
**                                  UMI_OK
**                                  UMI_TRANSFER_ALREADY_ACTIVE
**                                  UMI_TRANSFER_FAILED
**
** DESCRIPTION  Request from Upper MAC to copy a block of data from external
**              memory to the specified location in MAC memory. The same
**              message structure is used in the reply from the MAC Client.
**
****************************************************************************/
typedef struct _UMI_COPY_TO_MAC
{
    mtlk_umidata_ptr psSourceUmiData;
    mtlk_uint8_ptr pau8DestinationAddr;
    uint16 u16TransferOffset;
    uint16 u16TransferLength;
    uint16 u16Status;
    uint8  reseved[2];
} __MTLK_PACKED UMI_COPY_TO_MAC;



/***************************************************************************
**
** NAME         UMI_MEM
**
** DESCRIPTION  A union of all Memory Messages.
**
****************************************************************************/
typedef union _UMI_MEM
{
    UMI_COPY_FROM_MAC sCopyFromMAC;
    UMI_COPY_TO_MAC   sCopyToMAC;
} __MTLK_PACKED UMI_MEM;

/***                      Public Function Prototypes                     ***/
/***************************************************************************/

/*
 * Message between the MC and UM have a header.  The MC only needs the position
 * of the type field within the message and the length of the header.  All other
 * elements of the header are unused in the LM
*/

typedef struct _UMI_MSG
{
    mtlk_umimsg_ptr psNext;       /* Used to link list structures */
    uint8  u8Pad1;
    uint8  u8Persistent;
    uint16 u16MsgId;
    uint32 u32Pad2;                 /* For MIPS 8 bytes alignment */
    uint32 u32MessageRef;           /* Address in Host for Message body copy by DMA */
    uint8  abData[1];
} __MTLK_PACKED UMI_MSG;

typedef struct _UMI_MSG_HEADER
{
    mtlk_umimsg_ptr psNext;       /* Used to link list structures */
    uint8  u8Pad1;
    uint8  u8Persistent;
    uint16 u16MsgId;
    uint32 u32Pad2;                 /* For MIPS 8 bytes alignment */
    uint32 u32MessageRef;           /* Address in Host for Message body copy by DMA */
} __MTLK_PACKED UMI_MSG_HEADER;

/* REVISIT - was in shram.h - maybe should be in a him .h file but here is better for now */
/* linked UMI_DATA, MSDU, Host memory */
typedef struct _UMI_DATA_RX_STORAGE_ELEMENT
{
    UMI_MSG_HEADER    sMsgHeader;
    UMI_DATA_RX       sDATA;

} __MTLK_PACKED UMI_DATA_RX_STORAGE_ELEMENT;


typedef struct _UMI_DATA_TX_STORAGE_ELEMENT
{
    UMI_MSG_HEADER    sMsgHeader;
    UMI_DATA_TX       sDATA;

} __MTLK_PACKED UMI_DATA_TX_STORAGE_ELEMENT;

/***************************************************************************/

/* Memory messages between MAC and host - REVISIT - are none so why does this struct exist! */
typedef struct _SHRAM_MEM_MSG
{
    UMI_MSG_HEADER sHdr;                 /* Kernel Message Header */
    UMI_MEM        sMsg;                 /* UMI Memory Message */
} __MTLK_PACKED SHRAM_MEM_MSG;

/* Data transfer messages between MAC and Host */
typedef struct _SHRAM_DAT_REQ_MSG
{
    UMI_MSG_HEADER sHdr;                 /* Kernel Message Header */
    UMI_DATA_TX    sMsg;                 /* UMI Data Message */
} __MTLK_PACKED SHRAM_DAT_REQ_MSG;

/*Channel number for Fast Reboot - fast calibration  E.B */
typedef struct _UMI_CHNUM_FR
{
    uint16    u16CHNumber;
    uint8     calibrationAlgoMask;
    uint8     u8NumOfRxChains;
    uint8     u8NumOfTxChains;
    uint8     Reserved[3];
} __MTLK_PACKED UMI_CHNUM_FR;

/***************************************************************************
**
** NAME
**
** DESCRIPTION     Trace buffer Protocol Struct
**
****************************************************************************/


/*
*  HwTraceBuffer: The Trace Buffer Object
*/
#if defined(BSFTR_DEBUG)
#define TRACE_BUFFER_SIZE 512
typedef struct _TRACE_BUFFER
{
    uint32   data[MTLK_PAD4(TRACE_BUFFER_SIZE)];
} __MTLK_PACKED TRACE_BUFFER;

#endif

#define   MTLK_PACK_OFF
#include "mtlkpack.h"

//LBF structures & defines

//Rank1/Rank2 rates mask
#define RANK_TWO_NUMBER_OF_RATES	9
#define RANK_TWO_SHIFT				23
#define RANK_TWO_RATES_MASK			MASK(RANK_TWO_NUMBER_OF_RATES, RANK_TWO_SHIFT, uint32)
#define RANK_ONE_RATES_MASK			~RANK_TWO_RATES_MASK

#endif /* !__MHI_UMI_INCLUDED_H */
