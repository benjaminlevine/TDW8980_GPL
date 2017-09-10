/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __DRIVER_API_H__
#define __DRIVER_API_H__

#include "compat.h"

#define UMI_DEBUG_BLOCK_SIZE (384)
#define UMI_C100_DATA_SIZE   (UMI_DEBUG_BLOCK_SIZE - \
    (sizeof(uint16) + sizeof(uint16) + sizeof(c100_msg_header)))

typedef struct _c100_msg_header
{
  uint8 task;
  uint8 instance;
  uint16 msg_id;
} c100_msg_header;

typedef struct _umi_c100
{
  uint16 length;
  uint16 stream;
  c100_msg_header c100_hdr;
  uint8 data[UMI_C100_DATA_SIZE];
} umi_c100;

// Note: if changing this constant, also update driver source code
//define RECV_C100_MSG_MAX_SIZE ((sizeof(umi_c100) * 2) + 2)
#define RECV_C100_MSG_MAX_SIZE 400

int driver_stop();
int driver_restart(int wlanIndex);
int driver_upload_progmodel(const char *hyp_prog_model,
    const char *rf_prog_model);
int driver_send_c100_command(const umi_c100 *send_msg, char *recv_buf);

#endif // !__DRIVER_API_H__

