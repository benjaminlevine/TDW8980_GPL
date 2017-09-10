/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef _MTLK_EEPROM_H_
#define _MTLK_EEPROM_H_

#include "mhi_mib_id.h"
#include "txmm.h"

#define  MTLK_IDEFS_ON
#define  MTLK_IDEFS_PACKING 1
#include "mtlkidefs.h"

/* PCI configuration */
typedef struct _mtlk_eeprom_pci_cfg_t {           /* len  ofs */
  uint16 eeprom_executive_signature;              /*  2    2  */
  uint16 ee_control_configuration;                /*  2    4  */
  uint16 ee_executive_configuration;              /*  2    6  */
  uint8  revision_id;                             /*  1    7  */
  uint8  class_code[3];                           /*  3   10  */
  uint8  bist;                                    /*  1   11  */
#if defined(__LITTLE_ENDIAN_BITFIELD)
  uint8  misc : 4;
  uint8  status : 4;                              /*  1   12  */
#elif defined(__BIG_ENDIAN_BITFIELD)
  uint8  status : 4;                              /*  1   12  */
  uint8  misc : 4;
#else
  # error Endianess not defined!
#endif
  uint32 card_bus_cis_pointer;                    /*  4   16  */
  uint16 subsystem_vendor_id;                     /*  2   18  */
  uint16 subsystem_id;                            /*  2   20  */
  uint8  max_lat;                                 /*  1   21  */
  uint8  min_gnt;                                 /*  1   22  */
  uint16 power_management_capabilities;           /*  2   24  */
  uint32 hrc_runtime_base_address_register_range; /*  4   28  */
  uint32 local1_base_address_register_range;      /*  4   32  */
  uint16 hrc_target_configuration;                /*  2   34  */
  uint16 hrc_initiator_configuration;             /*  2   36  */
  uint16 vendor_id;                               /*  2   38  */
  uint16 device_id;                               /*  2   40  */
  uint32 reserved[6];                             /* 24   64  */
} __MTLK_IDATA mtlk_eeprom_pci_cfg_t;

/* version of the EEPROM */
typedef struct _mtlk_eeprom_version_t {
  uint8 version0;
  uint8 version1;
} __MTLK_IDATA mtlk_eeprom_version_t;

/* From TTPCom document "PCI/CardBus Host Reference Configuration
   Hardware Specification" p. 32 */
#define MTLK_EEPROM_EXEC_SIGNATURE              7164

typedef struct _mtlk_eeprom_t {
  mtlk_eeprom_pci_cfg_t config_area;
  uint16                cis_size;
  uint32                cis_base_address;
  mtlk_eeprom_version_t version;
  uint8                 cis[1];
} __MTLK_IDATA mtlk_eeprom_t;

/* NOTE: all cards that are not updated will have 0x42 value by default */
typedef union _mtlk_dev_opt_mask_t {
  struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
    uint8 ap_disabled:1;
    uint8 disable_sm_channels:1;
    uint8 __reserved:6;
#elif defined(__BIG_ENDIAN_BITFIELD)
    uint8 __reserved:6;
    uint8 disable_sm_channels:1;
    uint8 ap_disabled:1;
#else
  # error Endianess not defined!
#endif
  } __MTLK_IDATA s;
  uint8 d;
} __MTLK_IDATA mtlk_eeprom_dev_opt_mask_t;

/* CardBus Information Structure header */
typedef struct _mtlk_cis_header_t {
  uint8 tpl_code;
  uint8 link;
  uint8 data[1];
} __MTLK_IDATA mtlk_eeprom_cis_header_t;

/* CIS: card ID */
typedef struct _mtlk_cis_cardid_t {
  uint8 type;
  uint8 revision;
  uint8 country_code;
  mtlk_eeprom_dev_opt_mask_t dev_opt_mask;
  uint8 rf_chip_number;
  uint8 mac_address[6];
  uint8 sn[3];
  uint8 production_week;
  uint8 production_year;
} __MTLK_IDATA mtlk_eeprom_cis_cardid_t;

/* CIS item: TPC for version1 == 1 */
typedef struct _mtlk_cis_tpc_item_v1_t {
  uint8 channel;

#if defined(__LITTLE_ENDIAN_BITFIELD)
  uint8 ant0_tpc:5;
  uint8 ant0_band:1;
  uint8 pa0_a_hi:2;
#elif defined(__BIG_ENDIAN_BITFIELD)
  uint8 pa0_a_hi:2;
  uint8 ant0_band:1;
  uint8 ant0_tpc:5;
#else
  # error Endianess not defined!
#endif

  uint8 ant0_max_power;
  uint8 pa0_a_lo;
  int8 pa0_b;

#if defined(__LITTLE_ENDIAN_BITFIELD)
  uint8 ant1_tpc:5;
  uint8 ant1_band:1;
  uint8 pa1_a_hi:2;
#elif defined(__BIG_ENDIAN_BITFIELD)
  uint8 pa1_a_hi:2;
  uint8 ant1_band:1;
  uint8 ant1_tpc:5;
#else
  # error Endianess not defined!
#endif

  uint8 ant1_max_power;
  uint8 pa1_a_lo;
  int8 pa1_b;

} __MTLK_IDATA mtlk_eeprom_cis_tpc_item_v1_t;

/* CIS item: TPC for version1 == 2 */
typedef struct _mtlk_cis_tpc_item_v2_t {
  uint8 channel;

#if defined(__LITTLE_ENDIAN_BITFIELD)
  uint8 ant0_tpc:5;
  uint8 ant0_band:1;
  uint8 padding_1:2;
#elif defined(__BIG_ENDIAN_BITFIELD)
  uint8 padding_1:2;
  uint8 ant0_band:1;
  uint8 ant0_tpc:5;
#else
  # error Endianess not defined!
#endif

  uint8 ant0_max_power;
  uint8 pa0_a_lo;
  uint8 pa0_b_lo;

#if defined(__LITTLE_ENDIAN_BITFIELD)
  uint8 pa0_a_hi:4;
  uint8 pa0_b_hi:4;
  uint8 ant1_tpc:5;
  uint8 ant1_band:1;
  uint8 padding_2:2;
#elif defined(__BIG_ENDIAN_BITFIELD)
  uint8 pa0_b_hi:4;
  uint8 pa0_a_hi:4;
  uint8 padding_2:2;
  uint8 ant1_band:1;
  uint8 ant1_tpc:5;
#else
  # error Endianess not defined!
#endif

  uint8 ant1_max_power;
  uint8 pa1_a_lo;
  uint8 pa1_b_lo;

#if defined(__LITTLE_ENDIAN_BITFIELD)
  uint8 pa1_a_hi:4;
  uint8 pa1_b_hi:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
  uint8 pa1_b_hi:4;
  uint8 pa1_a_hi:4;
#else
  # error Endianess not defined!
#endif


} __MTLK_IDATA mtlk_eeprom_cis_tpc_item_v2_t;

/* number of TPC entries in the EEPROM */
#define MTLK_EEPROM_NUM_TPC_5_2_GHZ    5
#define MTLK_EEPROM_NUM_TPC_2_4_GHZ    1
#define MTLK_EEPROM_NUM_TPC            (MTLK_EEPROM_NUM_TPC_5_2_GHZ + MTLK_EEPROM_NUM_TPC_2_4_GHZ)

/* ****** EEPROM v3 ****** */
typedef struct _mtlk_cis_tpc_header_v3
{
  uint8 size_24;
  uint8 size_52;
  uint8 data[1];
} __MTLK_IDATA mtlk_eeprom_cis_tpc_header_t;

#define TPC_CIS_HEADER_V3_SIZE 2 /* size of tpc cis header, excluding data field */

struct tpc_point_t
{
  uint16 x;
  uint8 y;
};

#define NUM_TX_ANTENNAS_GEN2 (2)
#define NUM_TX_ANTENNAS_GEN3 (3)
#define MAX_NUM_TX_ANTENNAS  NUM_TX_ANTENNAS_GEN3
#define MTLK_MAKE_EEPROM_VERSION(major, minor) (((major) << 8) | (minor))

typedef struct _mtlk_eeprom_tpc_data_t
{
  uint8 channel;
  uint16 freq;
  uint8 band;
  uint8 spectrum_mode;
  uint8 tpc_values[MAX_NUM_TX_ANTENNAS];
  uint8 backoff_values[MAX_NUM_TX_ANTENNAS];
  uint8 backoff_mult_values[MAX_NUM_TX_ANTENNAS];
  uint8 num_points;
  struct tpc_point_t *points[MAX_NUM_TX_ANTENNAS];
  struct _mtlk_eeprom_tpc_data_t *next;
} __MTLK_IDATA mtlk_eeprom_tpc_data_t;

#define POINTS_4LN  5
#define POINTS_3LN  4
#define POINTS_2LN  3
#define POINTS_1LN  2

#define MINIMAL_TXPOWER_VAL 64
#define SECOND_TXPOWER_DELTA 24

typedef struct _mtlk_eeprom_tpc_v3_head
{
  uint8 channel;
#if defined(__LITTLE_ENDIAN_BITFIELD)
  uint8 ant0_tpc:5;
  uint8 band:1;
  uint8 mode:1;
  uint8 pa0_x1_hi:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
  uint8 pa0_x1_hi:1;
  uint8 mode:1;
  uint8 band:1;
  uint8 ant0_tpc:5;
#else
  # error Endianess not defined!
#endif

#if defined(__LITTLE_ENDIAN_BITFIELD)
  uint8 ant1_tpc:5;
//  uint8 reserved:2;
  uint8 backoff_a0:1;
  uint8 backoff_a1:1;
  uint8 pa1_x1_hi:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
  uint8 pa1_x1_hi:1;
//  uint8 reserved:2;
  uint8 backoff_a1:1;
  uint8 backoff_a0:1;
  uint8 ant1_tpc:5;
#else
  # error Endianess not defined!
#endif
  uint8 pa0_x1;
  uint8 pa0_y1; /* == max_power */
  uint8 pa0_x2;
  uint8 pa0_y2;
  uint8 pa1_x1; 
  uint8 pa1_y1; /* == max_power */
  uint8 pa1_x2;
  uint8 pa1_y2;
#if defined(__LITTLE_ENDIAN_BITFIELD)
  uint8 pa0_x2_hi:1;
  uint8 pa0_x3_hi:1;
  uint8 pa0_x4_hi:1;
  uint8 pa0_x5_hi:1;
  uint8 pa1_x2_hi:1;
  uint8 pa1_x3_hi:1;
  uint8 pa1_x4_hi:1;
  uint8 pa1_x5_hi:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
  uint8 pa1_x5_hi:1;
  uint8 pa1_x4_hi:1;
  uint8 pa1_x3_hi:1;
  uint8 pa1_x2_hi:1;
  uint8 pa0_x5_hi:1;
  uint8 pa0_x4_hi:1;
  uint8 pa0_x3_hi:1;
  uint8 pa0_x2_hi:1;
#else
  # error Endianess not defined!
#endif
  uint8 backoff0;
  uint8 backoff1;
} __MTLK_IDATA mtlk_eeprom_cis_tpc_item_v3_header;

typedef struct _mtlk_eeprom_tpc_v3_4ln
{
  mtlk_eeprom_cis_tpc_item_v3_header head;
  uint8 pa0_x3;
  uint8 pa0_y3;
  uint8 pa1_x3;
  uint8 pa1_y3;
  uint8 pa0_x4;
  uint8 pa0_y4;
  uint8 pa1_x4;
  uint8 pa1_y4;
  uint8 pa0_x5;
  uint8 pa0_y5;
  uint8 pa1_x5;
  uint8 pa1_y5;
} __MTLK_IDATA mtlk_eeprom_cis_tpc_item_v3_4ln_t;

typedef struct _mtlk_eeprom_tpc_v3_3ln
{
  mtlk_eeprom_cis_tpc_item_v3_header head;
  uint8 pa0_x3;
  uint8 pa0_y3;
  uint8 pa1_x3;
  uint8 pa1_y3;
  uint8 pa0_x4;
  uint8 pa0_y4;
  uint8 pa1_x4;
  uint8 pa1_y4;
} __MTLK_IDATA mtlk_eeprom_cis_tpc_item_v3_3ln_t;

typedef struct _mtlk_eeprom_tpc_v3_2ln
{
  mtlk_eeprom_cis_tpc_item_v3_header head;
  uint8 pa0_x3;
  uint8 pa0_y3;
  uint8 pa1_x3;
  uint8 pa1_y3;
} __MTLK_IDATA mtlk_eeprom_cis_tpc_item_v3_2ln_t;

typedef struct _mtlk_eeprom_tpc_v3_1ln
{
  mtlk_eeprom_cis_tpc_item_v3_header head;
} __MTLK_IDATA mtlk_eeprom_cis_tpc_item_v3_1ln_t;
/* ****** EEPROM v3 ****** */

/* CIS item: LNA */
typedef struct _mtlk_cis_lna_item_t {
  uint8 channel;
#if defined(__LITTLE_ENDIAN_BITFIELD)
  uint8 reserved:7;
  uint8 band:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
  uint8 band:1;
  uint8 reserved:7;
#else
  # error Endianess not defined!
#endif
  uint8 lna_gain0;
  uint8 lna_gain1;
  uint8 lna_gain2;
} __MTLK_IDATA mtlk_eeprom_cis_lna_item_t;

/* number of LNA entries in the EEPROM */
#define MTLK_EEPROM_NUM_LNA_5_2_GHZ    5
#define MTLK_EEPROM_NUM_LNA_2_4_GHZ    1
#define MTLK_EEPROM_NUM_LNA            (MTLK_EEPROM_NUM_LNA_5_2_GHZ + MTLK_EEPROM_NUM_LNA_2_4_GHZ)

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/* band */
typedef enum _mtlk_hw_band {
  MTLK_HW_BAND_5_2_GHZ,
  MTLK_HW_BAND_2_4_GHZ,
  MTLK_HW_BAND_BOTH,
  MTLK_HW_BAND_NONE
} mtlk_hw_band;

/* TPC antenna */
typedef enum _mtlk_tpc_antenna {
  MTLK_TPC_ANT_0,
  MTLK_TPC_ANT_1,
  MTLK_TPC_ANT_LAST
} mtlk_tpc_antenna;

/* all data read from the EEPROM (except PCI configuration) as a structure */
typedef struct _mtlk_eeprom_data_t {
  uint8             valid;
  uint16            eeprom_version;
  mtlk_eeprom_cis_cardid_t card_id;
  /* TPC calibration data */
  uint8             tpc_valid;
  //List_Of_Tpc_2_4_Ghz tpc_2_4_GHz[MTLK_TPC_ANT_LAST];
  //List_Of_Tpc_5_Ghz tpc_5_2_GHz[MTLK_TPC_ANT_LAST];
  uint16 vendor_id;
  uint16 device_id;
  uint16 sub_vendor_id;
  uint16 sub_device_id;
  /* EEPROM data */
  mtlk_eeprom_tpc_data_t *tpc_24;
  mtlk_eeprom_tpc_data_t *tpc_52;
} __MTLK_IDATA mtlk_eeprom_data_t;

#define MTLK_EE_BLOCKED_SEND_TIMEOUT     (10000) /* ms */

/* hardware codes as read from the EEPROM.
   These constants are used for Windows debug messages */
#define MTLK_EEPROM_HW_CODE_EVM          0xC5
#define MTLK_EEPROM_HW_CODE_CARDBUS      0xC6
#define MTLK_EEPROM_HW_CODE_LONGPCI      0xC4
#define MTLK_EEPROM_HW_CODE_SHORTPCI     0xC9

/* Progmodel type.
 * Here should be consequtive numbering!!!
 */
typedef enum _mtlk_prgmdl_type_e {
  MTLK_PRGMDL_TYPE_PHY,
  MTLK_PRGMDL_TYPE_HW,
  MTLK_PRGMDL_TYPE_LAST,
} mtlk_prgmdl_type_e;

typedef struct _mtlk_prgmdl_parse_t {
  uint32 spectrum_mode;
  uint32 hw_type;
  uint32 hw_revision;
  uint8  freq;
} __MTLK_IDATA mtlk_prgmdl_parse_t;

int __MTLK_IFUNC mtlk_eeprom_read_and_parse(mtlk_eeprom_data_t* ee_data, mtlk_txmm_t *txmm);

int __MTLK_IFUNC mtlk_reload_tpc (uint8 spectrum_mode, uint8 upper_lower, uint16 channel, mtlk_txmm_t *txmm, 
                                  mtlk_txmm_msg_t *msgs, uint32 nof_msgs, mtlk_eeprom_data_t *eeprom);
mtlk_eeprom_tpc_data_t* __MTLK_IFUNC mtlk_find_closest_freq (uint8 channel, mtlk_eeprom_data_t *eeprom);

uint16 __MTLK_IFUNC
mtlk_get_max_tx_power(mtlk_eeprom_data_t* eeprom, uint8 channel);

void __MTLK_IFUNC mtlk_clean_eeprom_data(mtlk_eeprom_data_t *eeprom_data);

int __MTLK_IFUNC mtlk_prgmdl_get_fname(mtlk_prgmdl_type_e prgmdl_type, mtlk_prgmdl_parse_t *param, char *fname);

char* __MTLK_IFUNC mtlk_eeprom_band_to_string(unsigned band);

int __MTLK_IFUNC mtlk_eeprom_is_band_supported(const mtlk_eeprom_data_t *ee_data, unsigned band);

/* EEPROM data accessors */
static __INLINE uint16
mtlk_eeprom_get_version(mtlk_eeprom_data_t *eeprom_data)
{
    return eeprom_data->eeprom_version;
}
static __INLINE uint8 
mtlk_eeprom_get_nic_type(mtlk_eeprom_data_t *eeprom_data)
{
  return eeprom_data->card_id.type;
}
static __INLINE uint8
mtlk_eeprom_get_nic_revision(mtlk_eeprom_data_t *eeprom_data)
{
    return eeprom_data->card_id.revision;
}
static __INLINE const uint8*
mtlk_eeprom_get_nic_mac_addr(mtlk_eeprom_data_t *eeprom_data)
{
    return eeprom_data->card_id.mac_address;
}

/*
 * If eeprom has no valid country (i.e country code has no associated domain)
 * or eeprom is not valid at all - return zero, thus indicate *unknown* country.
 * Country validation performed on initial eeprom read.
 */
static __INLINE uint8
mtlk_eeprom_get_country_code(mtlk_eeprom_data_t *eeprom_data)
{
  if (eeprom_data->valid)
    return eeprom_data->card_id.country_code;
  else
    return 0;
}

void mtlk_get_eeprom_text_info (mtlk_eeprom_data_t *eeprom, mtlk_txmm_t *txmm, char *buffer);

uint32 __MTLK_IFUNC mtlk_eeprom_get_caps (const mtlk_eeprom_data_t *eeprom, char *buffer, uint32 size);

static __INLINE uint8
mtlk_eeprom_get_num_antennas(mtlk_eeprom_data_t *eeprom)
{
  return ( eeprom->eeprom_version >= MTLK_MAKE_EEPROM_VERSION(4,0) ) ? NUM_TX_ANTENNAS_GEN3 
                                                                     : NUM_TX_ANTENNAS_GEN2;
}

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif
