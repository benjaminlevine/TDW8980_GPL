/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef _MTLKMSG_H_
#define _MTLKMSG_H_

#include "mhi_umi.h"

typedef enum
{
  CMT_MAN,
  CMT_DBG
} mtlk_hw_cm_type_e;

typedef union
{
  SHRAM_MAN_MSG man;
  SHRAM_DBG_MSG dbg;
} mtlk_hw_cm_pas_data_u;

typedef struct _mtlk_hw_cm_msg_t
{
  mtlk_hw_cm_pas_data_u pas_data;
} mtlk_hw_cm_msg_t;

#define MSG_OBJ          _mtlk_hw_cm_msg_t
typedef mtlk_hw_cm_msg_t *PMSG_OBJ;

#define MSG_OBJ_GET_ID(p)     (p)->pas_data.man.sHdr.u16MsgId
#define MSG_OBJ_SET_ID(p, id) (p)->pas_data.man.sHdr.u16MsgId = (id)
#define MSG_OBJ_PAYLOAD(p)    ((void*)(&(p)->pas_data.man.sMsg))

#endif /* !_STDMSG_H_ */ 
