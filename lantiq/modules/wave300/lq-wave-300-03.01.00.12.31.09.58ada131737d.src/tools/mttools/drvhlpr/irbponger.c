/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"
#include "irbponger.h"
#include "dataex.h"
#include "mtlkirb.h"

static const mtlk_guid_t IRBE_PING = MTLK_IRB_GUID_PING;
static const mtlk_guid_t IRBE_PONG = MTLK_IRB_GUID_PONG;

static void __MTLK_IFUNC
_irb_ping_handler (mtlk_handle_t      context,
                   const mtlk_guid_t *evt,
                   void              *buffer,
                   uint32            *size)
{
  MTLK_UNREFERENCED_PARAM(context);
  MTLK_ASSERT(mtlk_guid_compare(&IRBE_PING, evt) == 0);

  mtlk_irb_call_drv(&IRBE_PONG, buffer, *size);
}


static mtlk_handle_t
irb_ponger_start (void)
{
  return mtlk_irb_register(&IRBE_PING, 1, _irb_ping_handler, 0);
}

static void
irb_ponger_stop (mtlk_handle_t ctx)
{
  mtlk_irb_unregister(ctx);
}

const mtlk_component_api_t
irb_ponger_component_api = {
  irb_ponger_start,
  NULL,
  irb_ponger_stop
};
