/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id:
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Shared Auxiliary routines
 *
 */

#ifndef _MTLK_AUX_H
#define _MTLK_AUX_H

#include "mtlk_osal.h"

#define DPR_AUX_PFX "AUX: "

/*
 * Driver-only network mode.
 * Should be used on STA for dual-band scan
 */
#define NETWORK_11ABG_MIXED  NUM_OF_NETWORK_MODES
#define NETWORK_11ABGN_MIXED (NUM_OF_NETWORK_MODES + 1)
#define NETWORK_NONE (NUM_OF_NETWORK_MODES + 2)

#define CFG_BASIC_RATE_SET_DEFAULT  0
#define CFG_BASIC_RATE_SET_EXTRA    1
#define CFG_BASIC_RATE_SET_LEGACY   2
#define NUM_OF_BASIC_RATE_SET_MODES 3

#define LM_PHY_11B_RATE_MSK 0x00007F00
#define LM_PHY_11A_RATE_MSK 0x000000FF
#define LM_PHY_11N_RATE_MSK 0xFFFF8000

#define LM_PHY_11N_RATE_6_5      15// ,  11N    15     0x40
#define LM_PHY_11N_RATE_6_DUP    31//    11N    31     0x50

#define MIB_MANDATORY_RATES_A         0x00000015
#define MIB_BASIC_RATE_SET_2_B        0x00007800
#define MIB_BASIC_RATE_SET_2_B_LEGACY 0x00001800

#define MAKE_PROTOCOL_INDEX(is_ht, frequency_band) ((((is_ht) & 0x1) << 1) + ((frequency_band) & 0x1))
#define IS_HT_PROTOCOL_INDEX(index) (((index) & 0x2) >> 1)
#define IS_2_4_PROTOCOL_INDEX(index) ((index) & 0x1)

/* sequential print support - context */
typedef struct _mtlk_seq_printf_t mtlk_seq_printf_t;

/* Function converts string to signed long */
int32 __MTLK_IFUNC
mtlk_aux_atol(const char *s);

/* Function outputs buffer in hex format */
void __MTLK_IFUNC
mtlk_aux_print_hex (void *buf, unsigned int l);

/* sequential print support */
int mtlk_aux_seq_printf(mtlk_seq_printf_t *seq_ctx, const char *fmt, ...);

static __INLINE int
mtlk_aux_is_11n_rate (uint8 rate)
{
  return (rate >= LM_PHY_11N_RATE_6_5 &&
          rate <= LM_PHY_11N_RATE_6_DUP);
}

void mtlk_shexdump (char *buffer, uint8 *data, size_t size);

char * __MTLK_IFUNC mtlk_get_token (char *str, char *buf, size_t len, char delim);

#define AUX_MS_IN_SECOND    (1000)
#define AUX_SEC_IN_MINUTE   (60)
#define AUX_MINUTES_IN_HOUR (60)

static __INLINE mtlk_osal_msec_t
mtlk_timestamp_to_ms_ago(mtlk_osal_timestamp_t timestamp)
{
  return mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp_diff(mtlk_osal_timestamp(), timestamp));
}

static __INLINE int
mtlk_get_mseconds_ago(mtlk_osal_timestamp_t timestamp)
{
  return mtlk_timestamp_to_ms_ago(timestamp) % AUX_MS_IN_SECOND;
}

static __INLINE int
mtlk_get_seconds_ago(mtlk_osal_timestamp_t timestamp)
{
  return (mtlk_timestamp_to_ms_ago(timestamp) / AUX_MS_IN_SECOND) 
    % AUX_SEC_IN_MINUTE;
}

static __INLINE int
mtlk_get_minutes_ago(mtlk_osal_timestamp_t timestamp)
{
  return (mtlk_timestamp_to_ms_ago(timestamp) / 
    (AUX_MS_IN_SECOND * AUX_SEC_IN_MINUTE)) 
      % AUX_MINUTES_IN_HOUR;
}

static __INLINE int
mtlk_get_hours_ago(mtlk_osal_timestamp_t timestamp)
{
  return (mtlk_timestamp_to_ms_ago(timestamp) / 
    (AUX_MS_IN_SECOND * AUX_SEC_IN_MINUTE * AUX_MINUTES_IN_HOUR));
}

uint32 __MTLK_IFUNC get_operate_rate_set (uint8 net_mode);
uint32 __MTLK_IFUNC get_basic_rate_set (uint8 net_mode, uint8 mode);

uint8 __MTLK_IFUNC get_net_mode (uint8 band, uint8 is_ht);
uint8 __MTLK_IFUNC net_mode_to_band (uint8 net_mode);
BOOL __MTLK_IFUNC is_ht_net_mode (uint8 net_mode);
BOOL __MTLK_IFUNC is_mixed_net_mode (uint8 net_mode);
const char * __MTLK_IFUNC net_mode_to_string (uint8 net_mode);

/*
 * These magic numbers we got from WEB team.
 * Should be replaced by meaningful protocol names.
 */
enum {
  NETWORK_MODE_11AN = 14,
  NETWORK_MODE_11A = 10,
  NETWORK_MODE_11N5 = 12,
  NETWORK_MODE_11BGN = 23,
  NETWORK_MODE_11BG = 19,
  NETWORK_MODE_11B = 17,
  NETWORK_MODE_11G = 18,
  NETWORK_MODE_11N2 = 20,
  NETWORK_MODE_11GN = 22,
  NETWORK_MODE_11ABGN = 30,
  NETWORK_MODE_11ABG = 0,
};

uint8 __MTLK_IFUNC net_mode_ingress_filter (uint8 ingress_net_mode);
uint8 __MTLK_IFUNC net_mode_egress_filter (uint8 egress_net_mode);

#undef AUX_MS_IN_SECOND
#undef AUX_SEC_IN_MINUTE
#undef AUX_MINUTES_IN_HOUR

#endif // _MTLK_AUX_H

