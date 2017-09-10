/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * Copyright (c) 2007 Metalink Broadband (Israel)
 *
 * Driver API
 *
 */

#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/if.h>
#include <linux/wireless.h>

#include "compat.h"

#include "utils.h"
#include "driver_api.h"
#include "dutserver.h"

#define DPR0    LOG0
#define DPR1    LOG1
#define DPR2    LOG2
#define DPR3    LOG3
#define DPR_PFX "DRIVER_API: "

#define DRVCTL_LOAD_PROGMODEL 1

#ifndef DUT_IMAGES_DIR
#define DUT_IMAGES_DIR "/tmp/d"
#endif // DUT_IMAGES_DIR

#ifndef DUT_DECOMPRESS_IMAGE_CMND
#define DUT_DECOMPRESS_IMAGE_CMND "tar -C %s -x -z -f %s/progmodels.tar.gz %s"
#endif // DUT_DECOMPRESS_IMAGE_CMND

typedef struct _network_interface
{
    char name[IFNAMSIZ];
} network_interface;

typedef struct _drv_ctl
{
    int32 cmd;
    int32 datalen;
} drv_ctl;

int gsocket = -1;

int gifcount = 0;
network_interface gifs[32];

// Taken from BCLSockServer
static int find_interfaces( void )
{
  FILE *f;
  char buffer[1024];
  char ifname[IFNAMSIZ];
  char *tmp, *tmp2;
  struct iwreq req;

  f = fopen("/proc/net/dev", "r");
  if (!f) {
    // FIXME: make errors more verbose
    ERROR("fopen(): %s", strerror(errno));
    return -1;
  }

  // Skipping first two lines...
  if (!fgets(buffer, 1024, f)) {
    ERROR("fgets(): %s", strerror(errno));
    fclose(f);
    return -1;
  }

  if (!fgets(buffer, 1024, f)) {
    perror("fgets()");
    fclose(f);
    return -1;
  }

  // Reading interface descriptions...
  while (fgets(buffer, 1024, f)) {
    memset(ifname, 0, IFNAMSIZ);

    // Skipping through leading space characters...
    tmp = buffer;
    while (*tmp == ' ') {
      tmp++;
    }

    // Getting interface name...
    tmp2 = strstr(tmp, ":");
    strncpy(ifname, tmp,
      tmp2 - tmp > IFNAMSIZ ? IFNAMSIZ : tmp2 - tmp);

    INFO("Found network interface \"%s\"", ifname);

    // Checking if interface supports wireless extensions...
    strncpy(req.ifr_name, ifname, IFNAMSIZ);
    if (ioctl(gsocket, SIOCGIWNAME, &req)) {
      INFO("Interface \"%s\" has no wireless extensions...", ifname);
      continue;
    } else {
      INFO("Interface \"%s\" has wireless extensions...", ifname);
    }

    // Adding interface into our database...
    memcpy(gifs[gifcount].name, ifname, IFNAMSIZ);
    gifcount++;
  }

  fclose(f);

  return 0;
}

// driver_init()
// Called after insmod to setup a connection to the driver
static int
driver_init()
{
  int soc;
  DPR1("Init driver called");

  soc = socket(AF_INET, SOCK_DGRAM, 0);
  if (soc <= 0) {
    ERROR("socket(): %s", strerror(errno));
    return -1;
  }

  gsocket = soc;

  if (find_interfaces() < 0) {
    close(gsocket);
    gsocket = -1;
    return -1;
  }

  return 0;
}

static const char *
get_filename(const char *path)
{
  char *pslash = strrchr(path, '/');
  if (!pslash)
    return path;
  return pslash + 1;
}

// driver_stop()
// Stops the driver
int driver_stop()
{
  char filename[4096];
  size_t filename_maxlen = sizeof(filename) / sizeof(filename[0]);
  char *pdot;
#ifdef USE_RMMOD
  char buf[4096];
#endif

  DPR1("Stopping the driver");
  
  strncpy(filename, get_filename(driver_name), filename_maxlen);
  filename[filename_maxlen - 1] = '\0';

  pdot = strchr(filename, '.');
  if (pdot)
    *pdot = '\0'; // m.ko -> m
#ifdef USE_RMMOD
  snprintf(buf, sizeof(buf) / sizeof(buf[0]), "rmmod %s", filename);
  system(buf);
#else
  /* remove module and wait until usage count == 0 if needed */
  if (syscall(SYS_delete_module, filename, O_EXCL) != 0)
    DPR1("rmmod error during unloading %s: %s", filename, strerror(errno));
#endif
  return 0;
}

// driver_restart()
// Resets the MAC by reloading the driver
int driver_restart(int wlanIndex)
{
  char buf[4096];

  if (gsocket != -1) {
    DPR1("Closing the old gsocket");
    close(gsocket);
    gsocket = -1;
  }
  gifcount = 0;

  driver_stop();

  DPR1("Restarting the driver");

  sprintf(buf, "insmod %s dut=1 wlan=%d %s", driver_name, wlanIndex, insmod_params);
  system(buf);

  driver_init();
  
  return 0;
}

static int
upload_progmodel(const char *prog_model)
{
  struct iwreq req;
  drv_ctl *pctl = NULL;
  int datalen;
  int rslt = 0;
  char dut_prefix[] = "d/";
  int dut_prefix_len =
    (sizeof(dut_prefix) / sizeof(dut_prefix[0])) - 1;
  char *pdata;
  int prog_model_len = strlen(prog_model);

  DPR1("Uploading prog model: %s", prog_model);

  memcpy(req.ifr_ifrn.ifrn_name, gifs[0].name, IFNAMSIZ);

  datalen = dut_prefix_len + prog_model_len + 1;
  pctl = (drv_ctl *) malloc(sizeof(drv_ctl) + datalen);
  if (!pctl) {
    ERROR("Out of memory");
    rslt = -1;
    goto cleanup;
  }
  pctl->cmd = DRVCTL_LOAD_PROGMODEL;
  pctl->datalen = datalen;
  pdata = (char *) (pctl + 1);
  memcpy(pdata, dut_prefix, dut_prefix_len);
  memcpy(pdata + dut_prefix_len, prog_model, prog_model_len);
  pdata[dut_prefix_len + prog_model_len] = '\0';

  DPR3("Calling ioctl SIOCIWFIRSTPRIV+23");
  req.u.data.pointer = (caddr_t) pctl;
  if (ioctl(gsocket, SIOCIWFIRSTPRIV + 23, &req)) {
    rslt = -1;
    goto cleanup;
  }

cleanup:
  if (pctl)
    free(pctl);

  return rslt;
}

// RF Progmodels should be decompressed on demand, for the first time we use them, in order
// to save RAM space (we don't need most of the progmodels in the tar.gz file)
static int decompressProgmodel(const char* filename)
{
  char fullName[256], cmnd[512];
  sprintf(fullName, "%s/%s", DUT_IMAGES_DIR, filename);
  FILE* pFile = fopen(fullName, "r");
  if (pFile)
  {
    fclose(pFile);
    return 0; // OK, progmodel already exists
  }
  // Decompress the progmodel
  sprintf(cmnd, DUT_DECOMPRESS_IMAGE_CMND, DUT_IMAGES_DIR, DUT_IMAGES_DIR, filename); 
  system(cmnd);

  pFile = fopen(fullName, "r");
  if (!pFile) return 1; // Progmodel is still not here, return an error
  fclose(pFile);
  return 0;
}

// driver_upload_progmodel
// Parameters: 2 prog model files
// Reads the contents of the prog models, and sends them to the driver.
int driver_upload_progmodel(const char *hyp_prog_model,
    const char *rf_prog_model)
{
  DPR1("Uploading progmodels (hyp_prog_model=%s, rf_prog_model=%s)",
    hyp_prog_model, rf_prog_model);

  if (0 != upload_progmodel(hyp_prog_model))
    return 1;

  if (0 != decompressProgmodel(rf_prog_model)) 
    return 1;

  if (0 != upload_progmodel(rf_prog_model))
    return 1;

  DPR1("Progmodels uploaded successfuly");
  return 0;
}

// Requests the driver to send a UM_DBG_C100_IN_REQ.
// The driver should wait for MC_DBG_C100_IN_CFM from the MAC, and then
// for MC_DBG_C100_OUT_IND.
// Then, the driver should send UM_DBG_C100_OUT_RES to the MAC, and return
// the content of the MC_DBG_C100_OUT_IND back to this application.
//
// Note that the content of the C100 message is returned in ASCII, so it's
// not exactly UMI_C100.
// DUT_DLL in Windows does the convertion from the ASCII to the UMI_C100,
// so in here we return exactly the content of the C100_IND message.
// outBuf must be at least RECV_C100_MSG_MAX_SIZE bytes.
int driver_send_c100_command(const umi_c100 *send_msg, char *recv_buf)
{
  struct iwreq req;

  DPR1("Received a C100 command 0x%04X", send_msg->c100_hdr.msg_id);

  //memset(recv_buf, 0, RECV_C100_MSG_MAX_SIZE);

  memcpy(req.ifr_ifrn.ifrn_name, gifs[0].name, IFNAMSIZ);

  DPR3("Calling ioctl SIOCIWFIRSTPRIV+25");
  req.u.data.pointer = (caddr_t) send_msg;
  if (ioctl(gsocket, SIOCIWFIRSTPRIV + 25, &req))
    return -1;

  req.u.data.pointer = (caddr_t) recv_buf;
  DPR3("Calling ioctl SIOCIWFIRSTPRIV+24");
  if (ioctl(gsocket, SIOCIWFIRSTPRIV + 24, &req))
    return -1;

  DPR1("Send C100 command: success");
  return 0;
}


