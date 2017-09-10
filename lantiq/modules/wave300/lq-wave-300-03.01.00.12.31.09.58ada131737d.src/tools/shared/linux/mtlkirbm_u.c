/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "mtlkinc.h"
#include "mtlkirb.h"
#include "driver_api.h"
#include "dataex.h"
#include "mtlknlink.h"

#define DPR_PFX "IRBM: "

struct mtlk_irb_media
{
  pthread_t notification_thread;
  uint32 sequence_number;
  int thread_exit_code;
  int stop_pipe_fd[2];
};

static struct mtlk_irb_media irbm;
static int                   irbm_inited = 0;

static void 
irbm_packet_processor (void* param, void* packet)
{
  IRBM_APP_NOTIFY_HDR *hdr = (IRBM_APP_NOTIFY_HDR *) packet;

  /* Check sequence number of received packet */
  if( (-1 != irbm.sequence_number) && 
      (hdr->sequence_number - irbm.sequence_number > 1) )
    WARNING("APP notifications arrived out of order "
            "(current SN: %d, received SN: %d)", 
            irbm.sequence_number, hdr->sequence_number);
  irbm.sequence_number = hdr->sequence_number;

  mtlk_irb_on_evt(&hdr->event, hdr + 1, &hdr->data_length);
}

static void *
irbm_notification_thread_proc (void* param)
{
  int res;
  mtlk_nlink_socket_t nl_socket;
  
  res = mtlk_nlink_create(&nl_socket, irbm_packet_processor, &irbm);
  if(res < 0) {
    ERROR(DPR_PFX "Failed to create netlink socket: %s (%d)", strerror(res), res);
    goto end;
  }
  
  res = mtlk_nlink_receive_loop(&nl_socket, irbm.stop_pipe_fd[0]);
  if(res < 0) {
    ERROR(DPR_PFX "Netlink socket receive failed: %s (%d)", strerror(res), res);
  }

  mtlk_nlink_cleanup(&nl_socket);

end:
  irbm.thread_exit_code = res;
  pthread_exit(&irbm.thread_exit_code);
}

static int
irbm_create_notification_thread (void)
{
  int res;
  pthread_attr_t attr;

  /* Initialize and set thread detached attribute */
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  /* Create stop pipe */
  res = pipe(irbm.stop_pipe_fd);
  if(0 != res) {
    ERROR(DPR_PFX "Failed to create the thread notification pipe: %s (%d)", 
      strerror(res), res);
    goto end;
  }

  /* Create thread */
  res = pthread_create(&irbm.notification_thread, &attr,
    irbm_notification_thread_proc, NULL);

  /* Free resources */
  pthread_attr_destroy(&attr);

  if(0 != res) {
    ERROR(DPR_PFX "Failed to create the notification thread: %s (%d)", 
      strerror(res), res);
    goto error_cleanup;
  }

  goto end;

error_cleanup:
  close(irbm.stop_pipe_fd[0]);
  close(irbm.stop_pipe_fd[1]);
end:
  return (res == 0)?MTLK_ERR_OK:MTLK_ERR_NO_RESOURCES;
}

static int
irbm_terminate_notification_thread (void)
{
  void* status;
  int res;

  /* Signal the notification thread to stop */
  write(irbm.stop_pipe_fd[1], "x", 1);

  /* Wait for the notification thread to process the signal */
  res = pthread_join(irbm.notification_thread, &status);

  if(0 != res)
    ERROR(DPR_PFX "Failed to terminate the notification thread: %s (%d)", 
      strerror(res), res);

  close(irbm.stop_pipe_fd[0]);
  close(irbm.stop_pipe_fd[1]);

  return (res == 0)?MTLK_ERR_OK:MTLK_ERR_UNKNOWN;
}

int __MTLK_IFUNC
mtlk_irb_media_init (void)
{
  //TODO: When DriverHelper will be finally switched to IRB
  //      connection to driver must be established here
  MTLK_ASSERT(driver_connected());

  irbm.sequence_number = -1;
  irbm_inited          = 1;

  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_irb_media_start (void)
{
  MTLK_ASSERT(irbm_inited);
  MTLK_ASSERT(driver_connected());

  return irbm_create_notification_thread();
}

int __MTLK_IFUNC
mtlk_irb_media_stop (void)
{
  MTLK_ASSERT(irbm_inited);
  MTLK_ASSERT(driver_connected());

  return irbm_terminate_notification_thread();
}

void __MTLK_IFUNC
mtlk_irb_media_cleanup (void)
{
  MTLK_ASSERT(irbm_inited);
  MTLK_ASSERT(driver_connected());

  //TODO: When DriverHelper will be finally switched to IRB
  //      disconnection from driver must be performed here
  irbm_inited = 0;
}

int __MTLK_IFUNC
mtlk_irb_media_call_drv (const mtlk_guid_t *evt,
                         void              *buffer,
                         uint32             size)
{
  int res;
  uint8 *pkt = mtlk_osal_mem_alloc(sizeof(IRBM_DRIVER_CALL_HDR) + size, MTLK_MEM_TAG_IRB);
  IRBM_DRIVER_CALL_HDR *hdr = (IRBM_DRIVER_CALL_HDR *) pkt;
  MTLK_ASSERT(irbm_inited);
  MTLK_ASSERT(driver_connected());
  /* Send ioctl to the driver */
  if(NULL == pkt)
    return -1;

  hdr->event       = *evt;
  hdr->data_length = size;

  /* Copy data to the ioctl buffer */
  memcpy(pkt + sizeof(IRBM_DRIVER_CALL_HDR), buffer, size);

  res = (driver_ioctl(MTLK_IOCTL_IRBM, (char *)pkt, size) == 0)?MTLK_ERR_OK:MTLK_ERR_UNKNOWN;

  /* Get results from the ioctl buffer */
  memcpy(buffer, pkt + sizeof(IRBM_DRIVER_CALL_HDR), size);

  mtlk_osal_mem_free(pkt);

  return res;
}
