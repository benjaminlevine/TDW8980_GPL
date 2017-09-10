/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * Copyright (c) 2007 Metalink Broadband (Israel)
 *
 * MTDump.
 *
 * Originally written by Andrey Fidrya.
 *
 */

#include "mtlkinc.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "mtdump.h"
#include "driver_api.h"
#include "utils.h"

/*****************************************
 * Included from the mhi_umi.h
 * TODO: Think how to use common headers
 *****************************************/
//PHY characteristics parameters
#define UMI_PHY_TYPE_802_11A            0x00    /* 802.11a  */
#define UMI_PHY_TYPE_802_11B            0x01    /* 802.11b  */
#define UMI_PHY_TYPE_802_11G            0x02    /* 802.11g  */
#define UMI_PHY_TYPE_802_11B_L          0x81    /* 802.11b with long preamble*/
#define UMI_PHY_TYPE_802_11N_5_2_BAND   0x04    /* 802.11n_5.2G  */
#define UMI_PHY_TYPE_802_11N_2_4_BAND   0x05    /* 802.11n_2.4G  */


#define DPR0    LOG0
#define DPR1    LOG1
#define DPR2    LOG2
#define DPR9    LOG9
#define DPR_PFX "MTDUMP: "

const char *ifname = NULL;
int ifcmd = 0;
size_t ifdatalen = 0;
int mode_binary = 0;

#define CMD_ID_WMCE       0
#define CMD_ID_CONSTATUS  1
#define CMD_ID_UNKNOWN   -1

static inline uint32 uint32_to_host(uint32 val)
{
#ifdef HOST_BIG_ENDIAN
  return ((val & 0xFF) << 24) |
    ((val & 0xFF00) << 8) |
    ((val & 0xFF0000) >> 8) |
    (val >> 24);
#else
  return val;
#endif
}

static inline uint64 uint64_to_host(uint64 val)
{
#ifdef HOST_BIG_ENDIAN
  return (0x00000000000000FFULL & (val >> 56))
      | (0x000000000000FF00ULL & (val >> 40))
      | (0x0000000000FF0000ULL & (val >> 24))
      | (0x00000000FF000000ULL & (val >> 8))
      | (0x000000FF00000000ULL & (val << 8))
      | (0x0000FF0000000000ULL & (val << 24))
      | (0x00FF000000000000ULL & (val << 40))
      | (0xFF00000000000000ULL & (val << 56));
#else
  return val;
#endif
}

#define NETWORK_11B_ONLY     0
#define NETWORK_11G_ONLY     1
#define NETWORK_11N_2_4_ONLY 2
#define NETWORK_11BG_MIXED   3
#define NETWORK_11GN_MIXED   4
#define NETWORK_11BGN_MIXED  5
#define NETWORK_11A_ONLY     6
#define NETWORK_11N_5_ONLY   7
#define NETWORK_11AN_MIXED   8

static const char *get_network_mode_name (uint8 net_mode)
{
  switch (net_mode) {
  case NETWORK_11B_ONLY:
    return "b";
  case NETWORK_11G_ONLY:
    return "g";
  case NETWORK_11N_2_4_ONLY:
    return "n(2.4)";
  case NETWORK_11BG_MIXED:
    return "bg";
  case NETWORK_11GN_MIXED:
    return "gn";
  case NETWORK_11BGN_MIXED:
    return "bgn";
  case NETWORK_11A_ONLY:
    return "a";
  case NETWORK_11N_5_ONLY:
    return "n(5.2)";
  case NETWORK_11AN_MIXED:
    return "an";
  }

  return "?";
}

void
cmd_dump_wmce(uint64 *pdata);

void
gen_dataex_dump_constatus(WE_GEN_DATAEX_CONNECTION_STATUS *pqual);

int
cmd_find(const char *cmd_name)
{
  if (!strcasecmp(cmd_name, "wmce"))
    return CMD_ID_WMCE;
  else if (!strcasecmp(cmd_name, "constatus"))
    return CMD_ID_CONSTATUS;
  return CMD_ID_UNKNOWN;
}

int
cmd_get_params(int cmd_id, int *pioctl, size_t *pdatalen)
{
  switch (cmd_id) {
  case CMD_ID_WMCE:
    *pioctl = MTLK_WE_IOCTL_WMCE;
    *pdatalen = 96;
    break;
  case CMD_ID_CONSTATUS:
    *pioctl = MTLK_IOCTL_DATAEX;
    *pdatalen = 4096;
    break;
  default:
    ERROR("Unknown cmd_id (%d)", cmd_id);
    *pioctl = 0;
    *pdatalen = 0;
    return -1;
  }
  return 0;
}

int
cmd_init_data(int cmd_id, void *pdata, uint32 datalen)
{
  switch (cmd_id) {
  case CMD_ID_WMCE:
    break;
  case CMD_ID_CONSTATUS:
    {
      WE_GEN_DATAEX_REQUEST *preq = (WE_GEN_DATAEX_REQUEST *) pdata;
      preq->ver = WE_GEN_DATAEX_PROTO_VER;
      preq->cmd_id = WE_GEN_DATAEX_CMD_CONNECTION_STATS;
      preq->datalen = datalen;
    }
    break;
  default:
    ERROR("Unknown cmd_id (%d)", cmd_id);
    return -1;
  }
  return 0;
}

int
cmd_dump(int cmd_id, char *pdata)
{
  switch (cmd_id) {
  case CMD_ID_WMCE:
    cmd_dump_wmce((uint64 *) pdata);
    break;
  default:
    ERROR("Unknown cmd_id (%d)", cmd_id);
    return -1;
  }
  return 0;
}

void
cmd_dump_wmce(uint64 *pdata)
{
  WLAN_WIRELESS_STATISTICS *stats = (WLAN_WIRELESS_STATISTICS *)pdata;

  fprintf(stdout,
      
    "%20llu TransmittedFragmentCount       "
    "packet fragments sent\n"
    
    "%20llu MulticastTransmittedFrameCount "
    "multicast frames sent\n"
    
    "%20llu FailedCount                    "
    "number of frames sent that failed to correctly transmit\n"
    
    "%20llu RetryCount                     "
    "number of frames that were retried\n"
    
    "%20llu MultipleRetryCount             "
    "number of frames that were retried multiple times\n"
    
    "%20llu RTSSuccessCount                "
    "successful request to send (RTS) frames\n"
    
    "%20llu RTSFailureCount                "
    "failed request to send (RTS) frames\n"
    
    "%20llu ACKFailureCount                "
    "failed acknowledge (ACK) frames\n"
    
    "%20llu FrameDuplicateCount            "
    "duplicate frames\n"
    
    "%20llu ReceivedFragmentCount          "
    "received fragments\n"
    
    "%20llu MulticastReceivedFrameCount    "
    "received multicast frames\n"
    
    "%20llu FCSErrorCount                  "
    "number of received frames containing frame check sequence "
    "(FCS) errors\n",
   
    stats->TransmittedFragmentCount, stats->MulticastTransmittedFrameCount,
    stats->FailedCount, stats->RetryCount, stats->MultipleRetryCount,
    stats->RTSSuccessCount, stats->RTSFailureCount, stats->ACKFailureCount,
    stats->FrameDuplicateCount, stats->ReceivedFragmentCount,
    stats->MulticastReceivedFrameCount, stats->FCSErrorCount);
}

int
gen_dataex_dump(uint32 cmd_id, void *pdata, int datalen)
{
  switch (cmd_id) {
  case WE_GEN_DATAEX_CMD_CONNECTION_STATS:
    gen_dataex_dump_constatus((WE_GEN_DATAEX_CONNECTION_STATUS *) pdata);
    break;
  default:
    ERROR("Unknown cmd_id (%d)", cmd_id);
    return -1;
  }
  return 0;
}

void
gen_dataex_dump_constatus(WE_GEN_DATAEX_CONNECTION_STATUS *pqual)
{
  int noise_dbm = (int)pqual->u8GlobalNoise - 256;
  uint8 n;

  fprintf(stdout,
    "Connection Status\n"
    "=============================\n"
    "Noise Level  %8d dBm\n"
    "Channel Load %8u %%\n"
    "Disconnections %8u\n"
    "\n"
    " MAC address       | RSSI dBm       | PHY Rate Mb/s | Type   | Tx packets  | Tx dropped  | Rx packets\n",
    noise_dbm,
    pqual->u8ChannelLoad,
    pqual->u16NumDisconnections
  );

  for (n = 0; n < pqual->u8NumOfConnections; ++n) {
    WE_GEN_DATAEX_DEVICE_STATUS *devst = &pqual->sDeviceStatus[n];
    int rssi[3];

    rssi[0] = (int)pqual->sDeviceStatus[n].au8RSSI[0] - 256;
    rssi[1] = (int)pqual->sDeviceStatus[n].au8RSSI[1] - 256;
    rssi[2] = (int)pqual->sDeviceStatus[n].au8RSSI[2] - 256;

    fprintf(stdout, 
      " " MAC_FMT " | %+4i %+4i %+4i | %11u.%d | %-6s | %-11d | %-11d | %-11d\n",
      MAC_ARG(pqual->sDeviceStatus[n].sMacAdd.au8Addr),
      rssi[0],
      rssi[1],
      rssi[2],
      devst->u16TxRate / 10,
      devst->u16TxRate % 10,
      get_network_mode_name(devst->u8NetworkMode),
      devst->u32TxCount,
      devst->u32TxDropped,
      devst->u32RxCount
    );
  }
}

int
main(int argc, char *argv[])
{
  int curarg = 1;
  int retval = 0;
  char *pdata = NULL;
  int cmd_id = -1;
  int ioctl_rslt;

  if (argc < 2) {
    fprintf(stdout,"Metalink DUMP utility v. %s\n", MTLK_PACKAGE_VERSION);
    fprintf(stdout, "Usage:\n\n"
      "1)\n"
      "  mtdump <interface> ioctl <ioctl> <length>\n"
      "Calls ioctl and dumps data to stdout in binary form.\n"
      "- interface is network interface name (for example: wlan0)\n"
      "- ioctl is private ioctl number to call\n"
      "- length is number of bytes to read\n"
      "Example:\n"
      "  mtdump wlan0 ioctl 0x8BE3 96\n\n"
      "2)\n"
      "  mtdump <interface> <cmd>\n"
      "Reads data from driver and parses it according to command specification.\n"
      "Dumps results to stdout in user-readable form.\n"
      "- interface is network interface name (for example: wlan0)\n"
      "- cmd is command to execute. The following commands are supported:\n"
      "    wmce      - Windows Media Center Extender Statistics\n"
      "    constatus - Connection Status Statistics\n"
      "Example:\n"
      "  mtdump wlan0 wmce\n");
    goto end;
  }

  LOG1("Metalink DUMP utility v." MTLK_PACKAGE_VERSION);

  ifname = argv[curarg];
  ++curarg;

  if (curarg >= argc) {
    ERROR("Command name expected");
    goto end;
  }

  mode_binary = !strcmp(argv[curarg], "ioctl");

  if (mode_binary) {
    ++curarg;

    if (curarg >= argc) {
      ERROR("Ioctl number expected");
      goto end;
    }
    if (!strncmp(argv[curarg], "0x", 2))
      ifcmd = strtol(argv[curarg] + 2, NULL, 16);
    else
      ifcmd = strtol(argv[curarg], NULL, 10);
    ++curarg;
    
    if (curarg >= argc) {
      ERROR("Data length expected");
      goto end;
    }
    ifdatalen = strtoul(argv[curarg], NULL, 10);
    ++curarg;

  } else { // !mode_binary
    cmd_id = cmd_find(argv[curarg]);
    if (cmd_id == CMD_ID_UNKNOWN) {
      ERROR("Unknown command \"%s\"", argv[curarg]);
      goto end;
    }
    if (0 != cmd_get_params(cmd_id, &ifcmd, &ifdatalen))
      goto end;
    ++curarg;
  }
  
  if (0 != driver_setup_connection(ifname)) {
    retval = 1;
    goto end;
  }

  pdata = (char *) calloc(1, ifdatalen * sizeof(char));
  if (!pdata) {
    ERROR("Out of memory");
    retval = 1;
    goto end;
  }
  
  if (!mode_binary) {
    if (0 != cmd_init_data(cmd_id, pdata, ifdatalen))
      goto end;
  }
  
  ioctl_rslt = driver_ioctl(ifcmd, pdata, ifdatalen);
  if (ioctl_rslt == -1) {
    retval = 1;
    goto end;
  }

  /*
  if (NULL == freopen(NULL, "wb", stdout)) {
    ERROR("Unable to reopen stdout in binary mode: %s", strerror(errno));
    retval = 1;
    goto end;
  }
  */
  
  LOG1("Dumping to stdout");
  if (mode_binary) {
    if (1 != fwrite(pdata, ifdatalen, 1, stdout)) {
      ERROR("While dumping data to stdout: %s", strerror(errno));
      retval = 1;
      goto end;
    }
  } else { // !mode_binary
    if (!mode_binary && ifcmd == MTLK_IOCTL_DATAEX) {
      // Process GEN_DATAEX response
      WE_GEN_DATAEX_RESPONSE *presp = (WE_GEN_DATAEX_RESPONSE *) pdata;
      if (presp->ver != WE_GEN_DATAEX_PROTO_VER) {
        ERROR("Protocol version mismatch");
        retval = 1;
        goto end;
      }
      if (ioctl_rslt == WE_GEN_DATAEX_SUCCESS) {
        if (0 != gen_dataex_dump(cmd_id, (void *) (presp + 1), presp->datalen))
          goto end;
      } else if (ioctl_rslt == WE_GEN_DATAEX_FAIL) {
        ERROR("Command failed, please check driver logs");
      } else if (ioctl_rslt == WE_GEN_DATAEX_PROTO_MISMATCH) {
        ERROR("Protocol version mismatch (got %u, expected %u",
          (uint32) &pdata[0], WE_GEN_DATAEX_PROTO_VER);
      } else if (ioctl_rslt == WE_GEN_DATAEX_UNKNOWN_CMD) {
        ERROR("Driver cannot process this command");
      } else if (ioctl_rslt == WE_GEN_DATAEX_DATABUF_TOO_SMALL) {
        ERROR("Data buffer supplied is too small");
      }
    } else {
      // Process other commands (MS API etc)
      if (0 != cmd_dump(cmd_id, pdata))
        goto end;
    }
  }
 
end:
  if (pdata)
    free(pdata);

  LOG1("Terminated");

  return retval;
}

