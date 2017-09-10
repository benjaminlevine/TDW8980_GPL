/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __MTLK_IRB_H__
#define __MTLK_IRB_H__

#include "mtlk_osal.h"
#include "mtlklist.h"
#include "mtlkguid.h"
#include "mtlkirbm.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/************************************************************************
 * IRB - CP
 ************************************************************************/

typedef void (__MTLK_IFUNC * mtlk_irb_evt_handler_f)(mtlk_handle_t      context,
                                                     const mtlk_guid_t *evt,
                                                     void              *buffer,
                                                     uint32            *size);

/* Common Drv/App API - called once by application/driver */
int           __MTLK_IFUNC mtlk_irb_init(void);
void          __MTLK_IFUNC mtlk_irb_cleanup(void);

/* Common Drv/App API - called by modules */
mtlk_handle_t __MTLK_IFUNC mtlk_irb_register(const mtlk_guid_t     *evts,
                                             uint32                 nof_evts,
                                             mtlk_irb_evt_handler_f handler,
                                             mtlk_handle_t          context);
void          __MTLK_IFUNC mtlk_irb_unregister(mtlk_handle_t owner);

/* Drv specific API - called by driver modules */
static __INLINE int 
mtlk_irb_notify_app (const mtlk_guid_t *evt,
                     void              *buffer,
                     uint32             size)
{
  return mtlk_irb_media_notify_app(evt, buffer, size);
}

/* App specific API - called by allication modules */
static __INLINE int
mtlk_irb_call_drv (const mtlk_guid_t *evt,
                   void              *buffer,
                   uint32             size)
{
  return mtlk_irb_media_call_drv(evt, buffer, size);
}

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_IRB_H__ */
