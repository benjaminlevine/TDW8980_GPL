/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"
#include "mtlk_osal.h"
#include "mtlkirbm.h"
#include "mtlkirbm_k.h"
#include "nl.h"
#include "nlmsgs.h"
#include "dataex.h"

struct mtlk_irb_media
{
  mtlk_atomic_t sequence_number;
};

static struct mtlk_irb_media irbm;
static int                   irbm_inited = 0;

int __MTLK_IFUNC 
mtlk_irb_media_init (void)
{
  mtlk_osal_atomic_set(&irbm.sequence_number, 0);

  irbm_inited = 1;

  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_irb_media_start (void)
{
  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_irb_media_stop (void)
{
  return MTLK_ERR_OK;
}

void __MTLK_IFUNC
mtlk_irb_media_cleanup (void)
{
}

int __MTLK_IFUNC
mtlk_irb_media_ioctl_on_drv_call (void *buffer)
{
  IRBM_DRIVER_CALL_HDR hdr;
  void* data = NULL;
  int res = 0;

  MTLK_ASSERT(irbm_inited);
  MTLK_ASSERT(NULL != buffer);

  if (0 != copy_from_user(&hdr, buffer, sizeof(hdr))) {
    res = -EINVAL;
    goto FINISH;
  }

  data = mtlk_osal_mem_alloc(hdr.data_length, MTLK_MEM_TAG_IRBCALL);

  if (NULL == data) {
    res = -ENOMEM;
    goto FINISH;
  }

  if (0 != copy_from_user(data, buffer + sizeof(hdr), hdr.data_length)) {
    res = -EINVAL;
    goto FINISH;
  }

  mtlk_irb_on_evt(&hdr.event, data, &hdr.data_length);
  if (0 != copy_to_user(buffer + sizeof(hdr), data, hdr.data_length)) {
    res = -EINVAL;
    goto FINISH;
  }
  
FINISH:
  if(NULL != data)
    mtlk_osal_mem_free(data);

  return res;
}

int __MTLK_IFUNC
mtlk_irb_media_notify_app (const mtlk_guid_t *evt,
                           const void        *buffer,
                           uint32             size)
{
  int res;
  char* msg_buf;
  IRBM_APP_NOTIFY_HDR* msg_header;

  MTLK_ASSERT(irbm_inited);
  MTLK_ASSERT((0 == size) || (NULL != buffer));

  msg_buf = mtlk_osal_mem_alloc(sizeof(IRBM_APP_NOTIFY_HDR) + size,
                                MTLK_MEM_TAG_IRBNOTIFY);
  msg_header = (IRBM_APP_NOTIFY_HDR*) msg_buf;

  if(NULL == msg_header) {
    return MTLK_ERR_NO_MEM;
  }
    
  msg_header->event       = *evt;
  msg_header->data_length = size;
  msg_header->sequence_number = 
    mtlk_osal_atomic_inc(&irbm.sequence_number);

  memcpy(msg_header + 1, buffer, size);

  res = mtlk_nl_send_brd_msg(msg_buf, sizeof(IRBM_APP_NOTIFY_HDR) + size, GFP_ATOMIC,
                             NETLINK_IRBM_GROUP, NL_DRV_IRBM_NOTIFY);
  mtlk_osal_mem_free(msg_buf);
  return res;
}
