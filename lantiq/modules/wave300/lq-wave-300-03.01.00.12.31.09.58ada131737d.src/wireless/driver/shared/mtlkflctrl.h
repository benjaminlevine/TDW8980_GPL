/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __MTLK_FLOW_CTRL_H__
#define __MTLK_FLOW_CTRL_H__

#include "mtlkerr.h"
#include "mtlk_osal.h"
//#include "mtlkerr.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

typedef struct _mtlk_flctrl_api_t
{
  mtlk_handle_t  usr_data;
  int            (__MTLK_IFUNC *start)(mtlk_handle_t usr_data);
  int            (__MTLK_IFUNC *wake)(mtlk_handle_t usr_data);
  int            (__MTLK_IFUNC *stop)(mtlk_handle_t usr_data);
} __MTLK_IDATA mtlk_flctrl_api_t;

typedef struct _mtlk_flctrl_t
{
  uint32               stop_requests_mask;
  uint32               available_bits;
  mtlk_flctrl_api_t    api;
  mtlk_osal_spinlock_t lock;
  MTLK_DECLARE_INIT_STATUS;
} __MTLK_IDATA mtlk_flctrl_t;

int  __MTLK_IFUNC mtlk_flctrl_init(mtlk_flctrl_t           *obj,
                                   const mtlk_flctrl_api_t *api);
int  __MTLK_IFUNC mtlk_flctrl_register(mtlk_flctrl_t *obj, mtlk_handle_t *id);
int  __MTLK_IFUNC mtlk_flctrl_stop(mtlk_flctrl_t *obj, mtlk_handle_t id);
int  __MTLK_IFUNC mtlk_flctrl_start(mtlk_flctrl_t *obj, mtlk_handle_t id);
int  __MTLK_IFUNC mtlk_flctrl_wake(mtlk_flctrl_t *obj, mtlk_handle_t id);
int  __MTLK_IFUNC mtlk_flctrl_unregister(mtlk_flctrl_t *obj, mtlk_handle_t id);
void __MTLK_IFUNC mtlk_flctrl_cleanup(mtlk_flctrl_t *obj);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_FLOW_CTRL_H__*/
