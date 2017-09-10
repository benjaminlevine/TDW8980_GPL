/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __MTLK_IRBM_H__
#define __MTLK_IRBM_H__

#include "mtlkerr.h"
#include "mtlkguid.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/************************************************************************
 * IRB media - depend on OS and ring
 ************************************************************************/
int __MTLK_IFUNC  mtlk_irb_media_init(void);
int __MTLK_IFUNC  mtlk_irb_media_start(void);
int __MTLK_IFUNC  mtlk_irb_media_stop(void);
void __MTLK_IFUNC mtlk_irb_media_cleanup(void);

/* Drv/App media API - will be called by IRB to pass events */
int  __MTLK_IFUNC mtlk_irb_media_notify_app(const mtlk_guid_t *evt,
                                            const void        *buffer,
                                            uint32             size);
int  __MTLK_IFUNC mtlk_irb_media_call_drv(const mtlk_guid_t *evt,
                                          void              *buffer,
                                          uint32             size);

/* Common Drv/App event handler - must be called by IRB Media to handle events */
void __MTLK_IFUNC mtlk_irb_on_evt(const mtlk_guid_t *evt,
                                  void              *buffer,
                                  uint32            *size);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_IRBM_H__ */
