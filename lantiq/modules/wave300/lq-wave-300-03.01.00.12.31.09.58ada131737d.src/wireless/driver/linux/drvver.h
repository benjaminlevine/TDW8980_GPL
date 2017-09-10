/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __DRV_VER_H__
#define __DRV_VER_H__

#ifdef MTCFG_DEBUG
#define DRV_COMPILATION_TYPE ".Debug"
#else
#  ifdef MTCFG_SILENT
#    define DRV_COMPILATION_TYPE ".Silent"
#  else
#    define DRV_COMPILATION_TYPE ".Release"
#  endif
#endif

#define NIC_NAME        "wlan"

#ifdef MTCFG_LINDRV_HW_PCIG2
# define MTLK_PCIG2 ".PciG2"
#else
# define MTLK_PCIG2
#endif

#ifdef MTCFG_LINDRV_HW_PCIG3
# define MTLK_PCIG3 ".PciG3"
#else
# define MTLK_PCIG3
#endif

#ifdef MTCFG_LINDRV_HW_PCIE
# define MTLK_PCIEG3 ".PcieG3"
#else
# define MTLK_PCIEG3
#endif

#define MTLK_PLATFORMS  MTLK_PCIG2 MTLK_PCIG3 MTLK_PCIEG3

#define DRV_NAME        "mtlk"
#define DRV_VERSION     MTLK_PACKAGE_VERSION \
                        MTLK_PLATFORMS DRV_COMPILATION_TYPE
#define DRV_DESCRIPTION "Metalink 802.11n WiFi Network Driver"
#define DRV_COPYRIGHT   "Copyright(c) 2006-2008 Metalink Broadband"

#endif /* !__DRV_VER_H__ */

