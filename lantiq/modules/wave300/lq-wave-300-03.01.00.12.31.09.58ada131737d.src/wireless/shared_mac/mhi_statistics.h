/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/

/****************************************************************************
****************************************************************************
**
** COMPONENT:      ENET Upper MAC SW
**
** MODULE:         $File: //bwp/enet/demo153_sw/develop/src/shared/mhi_statistics.h $
**
** VERSION:        $Revision: #2 $
**
** DATED:          $Date: 2004/03/22 $
**
** AUTHOR:         S Sondergaard
**
** DESCRIPTION:    Statistics header
**
**
** LAST MODIFIED BY:   $Author: prh $
**
**
****************************************************************************
*
*   Copyright (c)TTPCom Limited, 2001
*
*   Copyright (c) Metalink Ltd., 2006 - 2007
*
****************************************************************************/

#ifndef __MHI_STATISTICS_INC
#define __MHI_STATISTICS_INC

#define   MTLK_PACK_ON
#include "mtlkpack.h"

#define STAT_RX_UNICAST_DATA                0   /* Unicats data frames received */
#define STAT_RX_DUPLICATE                   1   /* Duplicate frames received */
#define STAT_RX_MULTICAST_DATA              2   /* Number of multicast frames received */
#define STAT_RX_DISCARD                     3   /* Frames Discarded */
#define STAT_RX_UNKNOWN                     4   /* Unknown Rx */
#define STAT_RX_UNAUTH                      5   /* Reception From Unauthenticated STA */
#define STAT_RX_UNASSOC                     6   /* AP: Frames Rx from Unassociated STA */
#define STAT_RX_INTS                        7   /* Rx Interrupts */
#define STAT_RX_CONTROL                     8   /* RX Control type frames */

#define STAT_BEACON_TX                      9   /* Beacons Sent */
#define STAT_BEACON_RX                      10  /* Beacons Received */
#define STAT_AUTHENTICATE_TX                11  /* Authentication Requests Sent */
#define STAT_AUTHENTICATE_RX                12  /* Authentication Requests Received */

#define STAT_ASSOC_REQ_TX                   14  /* Association Requests Sent */
#define STAT_ASSOC_REQ_RX                   15  /* Association Requests Received */
#define STAT_ASSOC_RES_TX                   16  /* Association Replies Sent */
#define STAT_ASSOC_RES_RX                   17  /* Association Replies Received */

#define STAT_REASSOC_REQ_TX                 18  /* ReAssociation Requests Sent */
#define STAT_REASSOC_REQ_RX                 19  /* ReAssociation Requests Received */
#define STAT_REASSOC_RES_TX                 20  /* ReAssociation Replies Sent */
#define STAT_REASSOC_RES_RX                 21  /* ReAssociation Replies Received */

#define STAT_DEAUTH_TX                      22  /* Deauthentication Notifications Sent */
#define STAT_DEAUTH_RX                      23  /* Deauthentication Notifications Received */

#define STAT_DISASSOC_TX                    24  /* Disassociation Notifications Sent */
#define STAT_DISASSOC_RX                    25  /* Disassociation Notifications Received */

#define STAT_PROBE_REQ_TX                   26  /* Probe Requests sent */
#define STAT_PROBE_REQ_RX                   27  /* Probe Requests received */
#define STAT_PROBE_RES_TX                   28  /* Probe Responses sent */
#define STAT_PROBE_RES_RX                   29  /* Probe Responses received */

#define STAT_ATIM_TX                        30  /* ATIMs Transmitted successfully */
#define STAT_ATIM_RX                        31  /* ATIMs received */
#define STAT_ATIM_TX_FAIL                   32  /* ATIMs Failing transmission */

#define STAT_TX_MSDU                        33  /* TX msdus that have been sent */

#define STAT_TX_FAIL                        34  /* TX frames that have failed */
#define STAT_TX_RETRY                       35  /* TX retries to date */
#define STAT_TX_DEFER_PS                    36  /* Transmits deferred due to Power Mgmnt */
#define STAT_TX_DEFER_UNAUTH                37  /* Transmit deferred pending authentication */

#define STAT_BEACON_TIMEOUT                 38  /* Authentication Timeouts */
#define STAT_AUTH_TIMEOUT                   39  /* Authentication Timeouts */
#define STAT_ASSOC_TIMEOUT                  40  /* Association Timeouts */
#define STAT_ROAM_SCAN_TIMEOUT              41  /* Roam Scan timeout */

#define STAT_WEP_TOTAL_PACKETS              42  /* total number of packets passed through WEP */
#define STAT_WEP_EXCLUDED                   43  /* unencrypted packets received when WEP is active */
#define STAT_WEP_UNDECRYPTABLE              44  /* packets with no valid keys for decryption */
#define STAT_WEP_ICV_ERROR                  45  /* packets with incorrect WEP ICV */
#define STAT_TX_PS_POLL                     46  /* TX PS POLL */
#define STAT_RX_PS_POLL                     47  /* RX PS POLL */

#define STAT_MAN_ACTION_TX                  48  /* Management Actions sent */
#define STAT_MAN_ACTION_RX                  49  /* Management Actions received */

#define STAT_OUT_OF_RX_MSDUS                50  /* Management Actions received */

#define STAT_HOST_TX_REQ                    51  /* Requests from PC to Tx data - UM_DAT_TXDATA_REQ */
#define STAT_HOST_TX_CFM                    52  /* Confirm to PC by MAC of Tx data - MC_DAT_TXDATA_CFM */
#define STAT_BSS_DISCONNECT                 53  /* Station remove from database due to beacon/data timeout */

#define STAT_RX_DUPLICATE_WITH_RETRY_BIT_0  54  /* Duplicate frames received with retry bit set to 0 */
#define STAT_RX_NULL_DATA                   55  /* total number of received NULL DATA packets */
#define STAT_TX_NULL_DATA                   56  /* total number of sent NULL DATA packets */
#define STAT_TX_BAR                         57  /* <E.Z> - BAR Request sent */
#define STAT_RX_BAR                         58  /* <E.Z> */
#define STAT_TX_TOTAL_MANAGMENT_PACKETS     59  /*Total managment packet transmitted)*/
#define STAT_RX_TOTAL_MANAGMENT_PACKETS     60  /*Total Total managment packet recieved*/
#define STAT_RX_TOTAL_DATA_PACKETS          61
#define STAT_RX_FAIL_NO_DECRYPTION_KEY      62  /* RX Failures due to no key loaded (needed by Windows) */
#define STAT_RX_DECRYPTION_SUCCESSFUL       63  /* RX decryption successful (needed by Windows) */
#define STAT_TX_BAR_FAIL                    64
//PS_STATS
#define STAT_NUM_UNI_PS_INACTIVE            65  /* Number of unicast packets in PS-Inactive queue */
#define STAT_NUM_MULTI_PS_INACTIVE          66  /* Number of multicast packets in PS-Inactive queue */
#define STAT_TOT_PKS_PS_INACTIVE            67  /* total number of packets in PS-Inactive queue */
#define STAT_NUM_MULTI_PS_ACTIVE            68  /* Number of multicast packets in PS-Active queue */
#define STAT_NUM_TIME_IN_PS                 69  /* Number of STAs in power-save */
//WDS_STATS
#define STAT_WDS_TX_UNICAST                 70  /* Number of unicast WDS frames transmitted */
#define STAT_WDS_TX_MULTICAST               71  /* Number of multicast WDS frames transmitted */
#define STAT_WDS_RX_UNICAST                 72  /* Number of unicast WDS frames received */
#define STAT_WDS_RX_MULTICAST               73  /* Number of multicast WDS frames received */

#define STAT_CTS2SELF_TX                    74  /* CTS2SELF packets that have been sent */
#define STAT_CTS2SELF_TX_FAIL               75  /* CTS2SELF packets that have failed */

#define STAT_DECRYPTION_FAILED              76  /* Number of frames with decryption failed */
#define STAT_FRAGMENT_FAILED                77  /* Number of frames with wrong fragment number */
#define STAT_TX_MAX_RETRY                   78  /* Number of TX dropped packets with retry limit exceeded */

#define STAT_TX_RTS_SUCCESS					79
#define STAT_TX_RTS_FAIL					80

#define STAT_TX_MULTICAST_DATA				81

#define STAT_FCS_ERROR						82

#define STAT_RX_ADDBA_REQ					83
#define STAT_RX_ADDBA_RES					84
#define STAT_RX_DELBA_PKT					85

//DEBUG only
/*
added chipvars to support manual configuration of LBF

chipvar_set(0,9,field,data)

  STAT_ENABLE_AUTO_LBF:
	1 - enable implicit LBF, bypass host configuration
	2 - enable manual LBF, bypass host configuration
  STA_MANUAL_LBF
	manually set LBF sets
		bits[0:3] - CDD sets index
		bits[4-7] - matrix index
  STAT_MASK_RATES
    0 - no masking
	1 - only rank1 rates
	2 - only rank2 rates
  STAT_LBF_PROBING_FREQ
	manually configured probing frequency
  STAT_LBF_SAMPLE_FREQ
    manually configured sampling frequency
  STAT_LBF_TX_TH
    manually configured Tx threshold for statistics
*/
#define STAT_ENABLE_AUTO_LBF				86
#define STAT_MANUAL_LBF						87
#define STAT_MASK_RATES						88
#define STAT_LBF_PROBING_FREQ				89
#define STAT_LBF_SAMPLE_FREQ				90
#define STAT_LBF_TX_TH						91

/* counts times BAR has filed and transmitted again on receiving BA */
#define STAT_BAR_RETRANSMIT_ON_BA			92

#define STAT_TOTAL_NUMBER                   93  /* Size of stats array  */
//end of debug: STA_TOTAL_NUMBER should be set back to 86

#define Managment_Min_Index                 9
#define Managment_Max_Index                 31

/*************************************************************************************************/
/************************ Aggregation Counters ***************************************************/
/*************************************************************************************************/

#define STAT_TX_AGG_PKT                      0
#define STAT_RETRY_PKT                       1
#define STAT_DISCARD_PKT                     2
#define STAT_TX_AGG                          3
#define STAT_CLOSE_COND_SHORT_PKT            4  /* Agg closed due to minimum sized packet */
#define STAT_CLOSE_COND_MAX_PKTS             5  /* Agg closed due to max amount of pkts */
#define STAT_CLOSE_COND_MAX_BYTES            6  /* Agg closed due to max amount of bytes */
#define STAT_CLOSE_COND_TIMEOUT              7  /* Agg closed due to timeout interrupt */
#define STAT_CLOSE_COND_OUT_OF_WINDOW        8  /* Agg closed due to packet out of window  */
#define STAT_CLOSE_COND_MAX_MEM_USAGE        9  /* Agg closed due to max usage of agrregation memory (no more space for any additional packets) */
#define STAT_RECIEVED_BA                    10
#define STAT_NACK_EVENT                     11
#define STAT_SUB_FRAME_ATTACHED             12  /* Number of sub frames within the aggregate that we still atatched to  fsdu's (txaggrcounter) */
#define STAT_SUB_FRAME_CFM                  13  /* Number of sub frames within the aggregate already confirmed (R5). */
#define STAT_TX_CLOSED_PKT                  14
#define STAT_AGGR_STATE                     15
#define STAT_AGGR_LAST_CLOSED_REASON        16
#define STAT_BA_PROCESSED                   17  /* <O.H> - number of processed block acks */
#define STAT_BA_CORRUPTED                   18  /* Number of corrupted block ack frames */
#define STAT_BAR_TRANSMIT                   19  /* <E.Z> */
#define STAT_NEW_AGG_NOT_ALLOWED			20
#define STAT_CLOSE_COND_SOUNDING_PACKET		21
#define STAT_AGGR_TOTAL_NUMBER              22  /* Size of stats array - this define is always last  */

/*********************************************************************************************/
/******************** Debug Counters *********************************************************/
/*********************************************************************************************/

#define STAT_TX_OUT_OF_TX_MSDUS             0   /* No free Tx MSDUs for Host Tx request */
#define STAT_TX_LM_Q                        1   /* In vTXST_PutTxFsdu - Place data on LM Tx Q */
#define STAT_TX_FREE_FSDU_0                 2   /* vTXST_ReturnFsdu - UM frees data FSDU on priority 0 */
#define STAT_TX_FREE_FSDU_1                 3   /* vTXST_ReturnFsdu - UM frees data FSDU on priority 1 */
#define STAT_TX_FREE_FSDU_2                 4   /* vTXST_ReturnFsdu - UM frees data FSDU on priority 2 */
#define STAT_TX_FREE_FSDU_3                 5   /* vTXST_ReturnFsdu - UM frees data FSDU on priority 3 */
#define STAT_TX_PROTOCOL_CFM                6   /* vTXST_FsduCallback - Packet returned from LM confirm to Protocol task */
#define STAT_TX_DATA_REQ                    7   /* Call vTXP_SendDataPkt from vTx_dataReq - each req from Host */
#define STAT_TX_RX_SEND_PACKET              8   /* Call vTXP_SendDataPkt for Rx data - forward data */
#define STAT_TX_SEND_PACKET_AA              9   /* Confirm Tx on requests from Host */
#define STAT_TX_FSDU_UNKNOWN_STATE          10  /* FSDU is received in an unknown state - not confirmed to Host */
#define STAT_TX_FSDU_CALLBACK               11  /* Free FSDU from Host Tx Req */
#define STAT_TX_R5                          12  /* R5 indication from LM */
#define STAT_LM_TX_FSDU_CFM                 13  /* vLMIfsduConfirmViaKnl - Transmit cfm from LM (not aggregated) */
#define STAT_TX_LEGACY_PACKETS              14  /* vTXP_SendDataPkt - Total leagacy packets tx */
#define STAT_TX_AGGR_PACKETS                15  /* vTXP_SendDataPkt - Total aggr packets tx */
#define STAT_TX_UNICAST_PACKETS_FROM_HOST   16  /* vTx_dataReq - Totla unicast packets recieved from host */
#define STAT_TX_UNICAST_PACKETS_CFM         17  /* vTXQU_Callback - Total transmitted unicast packets */
#define STAT_TX_MULTICAST_PACKETS_FROM_HOST 18  /* vTx_dataReq - Totla multicast packets recieved from host */
#define STAT_TX_MULTICAST_PACKETS_CFM       19  /* vTXQU_Callback - Total transmitted multicast packets */
#define STAT_AGGR_TX_PKT_TO_LM_Q            20
#define STAT_BA_ERR                         21

#define DEBUG_STAT_TOTAL_NUMBER             22  /* Size of stats array  */

#define   MTLK_PACK_OFF
#include "mtlkpack.h"

#endif /* !__MHI_STATISTICS_INC */
