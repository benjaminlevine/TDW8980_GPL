/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * Copyright (c) 2007 Metalink Broadband (Israel)
 *
 * Log server.
 *
 * Originally written by Andrey Fidrya.
 *
 */

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include "dutserver.h"
#include "utils.h"
#include "driver_api.h"
#include "sockets.h"

#if 0
#define DUT_TEST
#endif

#define DPR0    LOG0
#define DPR1    LOG1
#define DPR2    LOG2
#define DPR9    LOG9
#define DPR_PFX "DUTSERVER: "

const char *driver_name = "m.ko";
const char *insmod_params = "";
const char *disconnect_cmd = NULL;

#define TUM_LMAC_INIT_REQ       0x010D // 0D01 reversed
#define TUM_LMAC_INIT_CFM       0x011D
#define TUM_MAN_QUERY_BCL_VALUE 0x1200
#define TASK_TUM                47
#define TASK_TUM_BCL            65
#define TUM_INIT_STATUS_OK      0
#define TUM_INIT_STATUS_FAILED  1

int
dut_test_c100(void)
{
  umi_c100 send_msg;
  char recv_buf[RECV_C100_MSG_MAX_SIZE];
  int i;
  int num_repeats = 100;
  int rep;

  // First, let's send a TUM_LMAC_INIT_REQ message.
  // This message has no payload data, so u16Length is 0.
  // If the message has payload data, au8Data points to that data
#if 0
  send_msg.length = 0;
  send_msg.c100_hdr.msg_id = TUM_LMAC_INIT_REQ;
  send_msg.c100_hdr.instance = 0;
  send_msg.c100_hdr.task = TASK_TUM;
  send_msg.stream = 1;
#else
  send_msg.length = 12 << 8;
  send_msg.c100_hdr.msg_id = TUM_MAN_QUERY_BCL_VALUE;
  send_msg.c100_hdr.instance = 0;
  send_msg.c100_hdr.task = TASK_TUM_BCL;
  send_msg.stream = 1;
  memset(send_msg.data, 0, 3 * sizeof(int32));
  send_msg.data[7] = 0xA6;
  send_msg.data[8] = 1;
#endif
  
  for (rep = 0; rep < num_repeats; ++rep) {
    if (0 != driver_send_c100_command(&send_msg, recv_buf))
    {
      ERROR("Failed to send C100 message");
      return -1;
    }
    //recv_buf[RECV_C100_MSG_MAX_SIZE-1] = '\0';
    //DPR0("recv_buf contents: %s", recv_buf);
    DPR0("recv_buf contents:");
    for (i = 0; i < sizeof(recv_buf) / sizeof(recv_buf[0]); ++i) {
      fprintf(stderr, "%c", recv_buf[i]);
    }
  }
  DPR0("---END---");
  return 0;
}

int
dut_test(void)
{
  if (0 != driver_restart(0)) {
    ERROR("Failed to initialize the driver");
    return -1;
  }

  if (0 != driver_upload_progmodel("ProgModel_A_CB",
      "ProgModel_A_CB_C4_RevD")) {
    ERROR("Failed to reset MAC");
    return -1;
  }

  if (0 != dut_test_c100()) {
    ERROR("Unable to send a test command");
    return -1;
  }

  INFO("Test completed successfully");
  return 0;
}

int
main(int argc, char *argv[])
{
#ifdef DUT_TEST
  int mother_socket;
#endif

  setsid();
  INFO("Metalink DUT Server v." MTLK_PACKAGE_VERSION);

  if (argc < 2) {
    INFO("Usage:");
    INFO("  dutserver driver_name [insmod_params] [disconnect_cmd]");
    INFO("- driver_name is the driver's filename");
    INFO("- insmod_params will be passed to insmod when loading the driver");
    INFO("  note: \"dut=1\" parameter is always set, do not specify it");
    INFO("- disconnect_cmd is a system command to be executed on client "
        "disconnection");
    INFO("Example:");
    INFO("  dutserver /tmp/jffs2/dut/mtlk.ko \"debug=3\" \"rm -rf "
        "/tmp/jffs2/images/d /tmp/jffs2/dut /tmp/d\"");
    return 0;
  }
  driver_name = argv[1];
  if (argc >= 3) {
    insmod_params = argv[2];
    INFO("insmod_params=%s", insmod_params);
  }
  if (argc >= 4) {
    disconnect_cmd = argv[3];
    INFO("disconnect_cmd=%s", disconnect_cmd);
  }
  
  //driver_restart();
  
#ifndef DUT_TEST
  // Start listen socket
  if (StartSocketsServer() != MT_RET_OK)
  {
    printf("Failed to start listen socket. Aborting.\n");
    getchar();
    return 0;
  }

  printf("Press 'x' to exit\n");
  while (getchar() != 'x') {}
    EndSocketsServer();
          
#else
  if (0 != dut_test()) {
    ERROR("DUT test failed");
    return 1;
  }
#endif
  
  INFO("Terminated");

  return 0;
}

