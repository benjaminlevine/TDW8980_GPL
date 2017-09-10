/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __ASEL_H__
#define __ASEL_H__

#include "mtlkcontainer.h"

extern const mtlk_component_api_t rf_mgmt_api;

struct mtlk_rf_mgmt_cfg
{
  uint32 rf_mgmt_type;
  uint32 refresh_time_ms;
  uint32 keep_alive_tmout_sec;
  uint32 averaging_alpha;
  uint32 margin_threshold;
};

extern struct mtlk_rf_mgmt_cfg rf_mgmt_cfg;

#endif /* __ASEL_H__ */


