/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"
#include "bitrate.h"
#include "mhi_ieee_address.h"
#include "mhi_mib_id.h"

/*****************************************************************************
**
** NAME         mtlk_bitrate_get_value
**
** PARAMETERS   index  requested bitrate index
**              sm     SpectrumMode
**              scp    ShortCyclicPrefix
**
** RETURNS      Value of bitrate in bits/second
**
** DESCRIPTION  Function returns value of bitrate 
**              depend on current HW parameters and bitrate index
**
******************************************************************************/

/*
 * Table below contain all available bitrates.
 * Values representation is fixed point, i.e. 60 means 6.0 Mbit/s.
 *
 * Index in table composed of several parameters:
 * bits | param
 * ======================
 *    0 | ShortCyclicPrefix
 *    1 | SpectrumMode
 *  2-6 | bitrate index
 *
 *  see get_bitrate_value() for details
 */
static const short int bitrates[] = {
/*************** 802.11a rates */
    60,   60,   60,   60, /*  0 */
    90,   90,   90,   90, /*  1 */
   120,  120,  120,  120, /*  2 */
   180,  180,  180,  180, /*  3 */
   240,  240,  240,  240, /*  4 */
   360,  360,  360,  360, /*  5 */
   480,  480,  480,  480, /*  6 */
   540,  540,  540,  540, /*  7 */
/*************** 802.11bg rates */
    20,   20,   20,   20, /*  8 */
    55,   55,   55,   55, /*  9 */
   110,  110,  110,  110, /* 10 */
    10,   10,   10,   10, /* 11 */
    20,   20,   20,   20, /* 12 */
    55,   55,   55,   55, /* 13 */
   110,  110,  110,  110, /* 14 */
/*************** 802.11n rates */
    65,   72,  135,  150, /* 15 */
   130,  144,  270,  300, /* 16 */
   195,  217,  405,  450, /* 17 */
   260,  289,  540,  600, /* 18 */
   390,  433,  810,  900, /* 19 */
   520,  578, 1080, 1200, /* 20 */
   585,  650, 1215, 1350, /* 21 */
   650,  722, 1350, 1500, /* 22 */
   130,  144,  270,  300, /* 23 */
   260,  289,  540,  600, /* 24 */
   390,  433,  810,  900, /* 25 */
   520,  578, 1080, 1200, /* 26 */
   780,  867, 1620, 1800, /* 27 */
  1040, 1156, 2160, 2400, /* 28 */
  1170, 1300, 2430, 2700, /* 29 */
  1300, 1444, 2700, 3000, /* 30 */
  1300, 1444, 2700, 3000, /* 31 */
};

int __MTLK_IFUNC
mtlk_bitrate_get_value (int index, int sm, int scp)
{
  int i; /* index in table */
  int value;

  ASSERT((BITRATE_FIRST <= index) && (index <= BITRATE_LAST));
  ASSERT((0 == sm) || (1 == sm));
  ASSERT((0 == scp) || (1 == scp));

  i = index << 2;
  i |= sm   << 1;
  i |= scp  << 0;

  /* In table values in fixed-point representation: 60 -> 6.0 Mbit/s */
  value = 100*1000*bitrates[i];

  return value;
}

int __MTLK_IFUNC
mtlk_bitrate_str_to_idx(const char *rate,
                        int spectrum_mode,
                        int short_cyclic_prefix,
                        uint16 *res)
{
  int whole, fractional;
  /*
   * Check whether requested value is bare index or mpbs.
   * We can distinguish with sscanf() return value (number of tokens read)
   */
  if (sscanf(rate, "%i.%i", &whole, &fractional) == 2) {
    int i;
    /*
     * Try to convert mbps value into rate index.
     * Resulting index should fall into rate_set.
     * If failed - forced_rate should be NO_RATE.
     */
    for (i = BITRATE_FIRST; i <= BITRATE_LAST; i++)
      if (((1000000*whole + 100000*fractional) ==
          mtlk_bitrate_get_value(i, spectrum_mode, short_cyclic_prefix))) {
        *res = i;
        return MTLK_ERR_OK;
      }
  } else if ((sscanf(rate, "%i", &whole) == 1) &&
             (BITRATE_FIRST <= whole) && (whole <= BITRATE_LAST)) {
      *res = whole;
      return MTLK_ERR_OK;
  } else if (!strcmp(rate, "auto")) {
     *res = NO_RATE;
     return MTLK_ERR_OK;
  } 
  
  return MTLK_ERR_PARAMS;
}

int __MTLK_IFUNC
mtlk_bitrate_idx_to_str(uint16 rate,
                        int spectrum_mode,
                        int short_cyclic_prefix,
                        char *res,
                        unsigned *length)
{
  if (rate == NO_RATE)
    *length =  snprintf(res, *length, "auto");
  else if (rate <= BITRATE_LAST) {
    uint32 bps = mtlk_bitrate_get_value(rate,
                                        spectrum_mode,
                                        short_cyclic_prefix);
    /* bps should be converted into `xxx.x mbps' notation, i.e 13500000 -> 13.5 mbps */
    *length = snprintf(res, *length, "%i.%i mbps", bps/(1000000), (bps%(1000000))/100000);
  } else
    return MTLK_ERR_PARAMS;

  return MTLK_ERR_OK;
}

