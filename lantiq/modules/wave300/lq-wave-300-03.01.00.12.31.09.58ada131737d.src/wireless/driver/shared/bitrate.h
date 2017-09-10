/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __BITRATE_H__
#define __BITRATE_H__

#include "mtlkidefs.h"

#define BITRATE_FIRST         0
#define BITRATE_LAST          31

int __MTLK_IFUNC mtlk_bitrate_get_value (int index, int sm, int scp);
int __MTLK_IFUNC 
mtlk_bitrate_str_to_idx(const char *rate,
                        int spectrum_mode,
                        int short_cyclic_prefix,
                        uint16 *res);
int __MTLK_IFUNC
mtlk_bitrate_idx_to_str(uint16 rate,
                        int spectrum_mode,
                        int short_cyclic_prefix,
                        char *res,
                        unsigned *length);
#endif

