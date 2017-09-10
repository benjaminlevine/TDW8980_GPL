/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id$
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Written by: Roman Sikorskyy 
 *
 */

#ifndef __CHANNELS_H__
#define __CHANNELS_H__

#include "mhi_umi.h"
#include "mhi_mib_id.h"
#include "scan.h"
#include "aocs.h"

#define MTLK_IDEFS_ON
#include "mtlkidefs.h"

#define SCAN_ACTIVE  MIB_ST_ACTIVE
#define SCAN_PASSIVE MIB_ST_PASSIVE

#define MAX_CHANNELS        	50
#define MTLK_COUNTRY_NAME_SIZE	3

/* Need to be synchronized with ARRAY_SIZE(country_reg_table) */
/* it can't be shared directly */
#define MAX_COUNTRIES       249

#define ALTERNATE_LOWER     1
#define ALTERNATE_UPPER     0
#define ALTERNATE_NONE      (-1)
#define SPECTRUM_20MHZ      MIB_SPECTRUM_20M
#define SPECTRUM_40MHZ      MIB_SPECTRUM_40M
#define SPECTRUM_AUTO       2

#define REG_LIMITS    0x01
#define HW_LIMITS     0x02
#define ANTENNA_GAIN  0x04
#define ALL_LIMITS    (REG_LIMITS | HW_LIMITS | ANTENNA_GAIN)

#define MTLK_CHNLS_DOT11H_CALLER    1
#define MTLK_CHNLS_SCAN_CALLER      2
#define MTLK_CHNLS_COUNTRY_CALLER   3


typedef struct _drv_params_t
{
  uint8 band;
  uint8 bandwidth;
  uint8 upper_lower;
  uint8 reg_domain;
  uint8 spectrum_mode;
} __MTLK_IDATA drv_params_t;

struct reg_tx_limit;
struct hw_reg_tx_limit;
struct antenna_gain;

typedef struct _tx_limit_t
{
  struct reg_tx_limit *reg_lim;
  struct hw_reg_tx_limit *hw_lim;
  size_t num_gains;
  struct antenna_gain *gain;
  /* this is needed for HW limits - to load initial data */
  uint16 vendor_id;
  uint16 device_id;
  uint8 hw_type;
  uint8 hw_revision;
  uint8 num_tx_antennas;
  uint8 num_rx_antennas;
} __MTLK_IDATA tx_limit_t;

struct reg_class_t
{
  uint8 reg_class;
  uint16 start_freq;
  uint8 spacing;
  int8 max_power;
  uint8 scan_type; /* 0 - active, 1 - passive */
  uint8 sm_required;
  int8 mitigation;
  uint8 num_channels;
  const uint8 *channels;
  uint16 channelAvailabilityCheckTime;
  uint8 radar_detection_validity_time;
  uint8 non_occupied_period;
};

struct reg_domain_t
{
  uint8 num_classes;
  const struct reg_class_t *classes;
};

typedef struct _mtlk_get_channel_data_t {
  uint8 reg_domain;
  uint8 is_ht;
  BOOL ap;
  uint8 spectrum_mode;
  uint8 bonding;
  uint16 channel;
  uint8 frequency_band;
  BOOL disable_sm_channels;
} mtlk_get_channel_data_t;

typedef struct _mtlk_country_name_t {
  char name[MTLK_COUNTRY_NAME_SIZE];
} mtlk_country_name_t;

int16 __MTLK_IFUNC
mtlk_calc_tx_power_lim (tx_limit_t *lim, uint16 channel, uint8 reg_domain, 
    uint8 spectrum_mode, int8 upper_lower, uint8 num_antennas);
#if 0
int __MTLK_IFUNC mtlk_fill_freq_element (mtlk_handle_t context, FREQUENCY_ELEMENT *el, uint8 channel, uint8 band, uint8 bw, uint8 reg_domain);
#endif
int16 __MTLK_IFUNC mtlk_get_antenna_gain(tx_limit_t *lim, uint16 channel);

int __MTLK_IFUNC mtlk_init_tx_limit_tables (tx_limit_t *lim, uint16 vendor_id, uint16 device_id,
  uint8 hw_type, uint8 hw_revision);
int __MTLK_IFUNC mtlk_cleanup_tx_limit_tables (tx_limit_t *lim);
int __MTLK_IFUNC mtlk_reset_tx_limit_tables (tx_limit_t *lim);
int __MTLK_IFUNC mtlk_update_reg_limit_table (mtlk_handle_t handle, struct country_ie_t *ie, int8 power_constraint);

int __MTLK_IFUNC mtlk_set_hw_limit (tx_limit_t *lim, char *str);
int __MTLK_IFUNC mtlk_set_ant_gain (tx_limit_t *lim, char *str);

/* This wrappers defined in core.c */
int16 __MTLK_IFUNC mtlk_calc_tx_power_lim_wrapper(mtlk_handle_t usr_data, int8 spectrum_mode, uint8 channel); 
int16 __MTLK_IFUNC mtlk_scan_calc_tx_power_lim_wrapper(mtlk_handle_t usr_data, int8 spectrum_mode, uint8 reg_domain, uint8 channel); 
int16 __MTLK_IFUNC mtlk_get_antenna_gain_wrapper(mtlk_handle_t usr_data, uint8 channel); 
int __MTLK_IFUNC mtlk_reload_tpc_wrapper (uint8 channel, mtlk_handle_t usr_data);
void __MTLK_IFUNC mtlk_print_tx_limit_table (tx_limit_t *lim, mtlk_seq_printf_t *s, uint8 type);

int __MTLK_IFUNC mtlk_set_country_mib (mtlk_txmm_t *txmm, 
                                       uint8 reg_domain, 
                                       uint8 is_ht, 
                                       uint8 frequency_band, 
                                       uint8 ap, 
                                       const char *country,
                                       BOOL is_dot11d_active);
uint8 __MTLK_IFUNC mtlk_get_channel_mitigation(uint8 reg_domain, uint8 is_ht, uint8 spectrum_mode, uint16 channel);
int __MTLK_IFUNC
mtlk_get_avail_channels(mtlk_get_channel_data_t *param, uint8 *channels);
int __MTLK_IFUNC
mtlk_check_channel(mtlk_get_channel_data_t *param, uint8 channel);

int __MTLK_IFUNC mtlk_prepare_scan_vector (mtlk_handle_t context, struct mtlk_scan *scan_data, int freq, uint8 reg_domain);
void __MTLK_IFUNC mtlk_free_scan_vector (struct mtlk_scan_vector *vector);

const struct reg_domain_t * __MTLK_IFUNC mtlk_get_domain(uint8 reg_domain
                                                         ,int *result
                                                         ,uint16 *num_of_protocols
                                                         ,uint8 u8Upper
                                                         ,uint16 caller);
uint8 __MTLK_IFUNC mtlk_get_channels_for_reg_domain (struct mtlk_scan *scan_data,
                                                     FREQUENCY_ELEMENT *channels,
                                                     uint8 *num_channels);
int __MTLK_IFUNC mtlk_get_channel_data(mtlk_get_channel_data_t *params,
  FREQUENCY_ELEMENT *freq_element, uint8 *non_occupied_period,
  uint8 *radar_detection_validity_time);

uint8 __MTLK_IFUNC mtlk_select_reg_domain (uint16 channel);

uint16 __MTLK_IFUNC
mtlk_calc_start_freq (drv_params_t *param, uint16 channel);

uint8 __MTLK_IFUNC mtlk_get_chnl_switch_mode (uint8 spectrum_mode, uint8 bonding, uint8 is_silent_sw);

static __INLINE uint8 __MTLK_IFUNC
channel_to_band (uint16 channel)
{
#define FIRST_5_2_CHANNEL 36
  return (channel < FIRST_5_2_CHANNEL)? MTLK_HW_BAND_2_4_GHZ : MTLK_HW_BAND_5_2_GHZ;
#undef FIRST_5_2_CHANNEL
}

static __INLINE uint16 __MTLK_IFUNC
channel_to_frequency (uint16 channel)
{
#define CHANNEL_THRESHOLD 180
  if (channel_to_band(channel) == MTLK_HW_BAND_2_4_GHZ)
    return 2407 + 5*channel;
  else if (channel < CHANNEL_THRESHOLD)
    return 5000 + 5*channel;
  else
    return 4000 + 5*channel;
#undef CHANNEL_THRESHOLD
}

uint8 __MTLK_IFUNC country_code_to_domain (uint8 country_code);
const char * __MTLK_IFUNC country_code_to_country (uint8 country_code);
uint8 __MTLK_IFUNC country_to_country_code (const char *country);

void __MTLK_IFUNC
get_all_countries_for_domain(uint8 domain, mtlk_country_name_t *countries, uint32 countries_buffer_size);

static uint8 __INLINE __MTLK_IFUNC
country_to_domain (const char *country)
{
  return country_code_to_domain(country_to_country_code(country));
}

BOOL __MTLK_IFUNC mtlk_channels_does_domain_exist(uint8 reg_domain);

#define MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __CHANNELS_H__ */
