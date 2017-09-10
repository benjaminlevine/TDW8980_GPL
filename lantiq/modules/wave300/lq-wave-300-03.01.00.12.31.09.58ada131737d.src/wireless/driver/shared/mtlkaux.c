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
#include "mtlkinc.h"

#include "mtlkaux.h"
#include "mtlkqos.h"
#include "mtlk_packets.h"
#include "eeprom.h"
#include "mhi_umi.h"
#include "bufmgr.h"

static int
mtlk_aux_digit(int c, int base)
{
  if (base == 16) {
    if (c >= '0' && c <= '9')
      return c - '0';

    c = tolower(c);
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;

    return -1;
  }

  if (base == 10) {
    if (c >= '0' && c <= '9')
      return c - '0';
    else
      return -1;
  }

  if (base == 8) {
    if (c >= '0' && c <= '7')
      return c - '0';
    else
      return -1;
  }

  return -1;
}


/*
 * Function converts string to signed long
 */
int32 __MTLK_IFUNC
mtlk_aux_atol(const char *s)
{
  const char *p = s;
  int32 n=0;
  int v;
  char sign;

  while (*p == ' ' || *p == '\t')
    p++;

  if (*p == '0' && *(p+1) == 'x') {
    p += 2;
    while ((v = mtlk_aux_digit(*p, 16)) != -1) {
      n = 16*n + v;
      p++;
    }
    return n;
  }

  if (*p == '0') {
    p += 1;
    while ((v = mtlk_aux_digit(*p, 8)) != -1) {
      n = 8*n + v;
      p++;
    }
    return n;
  }

  sign = FALSE;
  if (*p == '-') {
    sign = TRUE;
    p++;
  }

  while ((v = mtlk_aux_digit(*p, 10)) != -1) {
    n = 10*n + v;
    p++;
  }
  if (sign)
    n = -n;
  return n;
}

/*
 * Function outputs buffer in hex format
 */
#ifdef MTCFG_SILENT
void __MTLK_IFUNC
mtlk_aux_print_hex (void *buf, unsigned int l)
{
}
#else
void __MTLK_IFUNC
mtlk_aux_print_hex (void *buf, unsigned int l)
{
  unsigned int i,j;
  unsigned char *cp = (unsigned char*)buf;
  LOG_BUFFER("cp= 0x%p l=%d\n", cp, l);
  for (i = 0; i < l/16; i++) {
    LOG_BUFFER("%04x:  ", 16*i);
    for (j = 0; j < 16; j++)
      LOG_BUFFER("%02x %s", *cp++, j== 7 ? " " : "");
    LOG_BUFFER("\n");
  }
  if (l & 0x0f) {
    LOG_BUFFER("%04x:  ", 16*i);
    for (j = 0; j < (l&0x0f); j++)
      LOG_BUFFER("%02x %s", *cp++, j== 7 ? " " : "");
    LOG_BUFFER("\n");
  }
}
#endif

void
mtlk_shexdump (char *buffer, uint8 *data, size_t size)
{
  uint8 line, i;

  for (line = 0; size; line++) {
    buffer += sprintf(buffer, "%04x: ", line * 0x10);
    for (i = 0x10; i && size; size--,i--,data++)
      buffer +=sprintf(buffer, " %02x", *data);
    buffer += sprintf(buffer, "\n");
  }
}

char * __MTLK_IFUNC
mtlk_get_token (char *str, char *buf, size_t len, char delim)
{
  char *dlm;

  if (!str) {
    buf[0] = 0;
    return NULL;
  }
  dlm = strchr(str, delim);
  if (dlm && ((size_t)(dlm - str) < len)) {
    memcpy(buf, str, dlm - str);
    buf[dlm - str] = 0;
  } else {
    memcpy(buf, str, len - 1);
    buf[len - 1] = 0;
  }
  ILOG4(GID_UNKNOWN, "Get token: '%s'", buf);
  if (dlm)
    return dlm + 1;
  return dlm;
}

static const uint32 OperateRateSet[] = {
  0x00007f00, /* 11b@2.4GHz */
  0x00007fff, /* 11g@2.4GHz */
  0xffffffff, /* 11n@2.4GHz */
  0x00007fff, /* 11bg@2.4GHz */
  0xffffffff, /* 11gn@2.4GHz */
  0xffffffff, /* 11bgn@2.4GHz */
  0x000000ff, /* 11a@5.2GHz */
  0xffff80ff, /* 11n@5.2GHz */
  0xffff80ff, /* 11an@5.2GHz */

  /* these ARE NOT real network modes. they are for
     unconfigured dual-band STA which may be NETWORK_11ABG_MIXED
     or NETWORK_11ABGN_MIXED */
  0x00007fff, /* 11abg */
  0xffffffff, /* 11abgn */
};

/*
 * There are three available BSSBasicRateSet modes:
 * CFG_BASIC_RATE_SET_DEFAULT:
 *   BSSBasicRateSet according to standard requirements:
 *   for 2.4GHz is 1, 2, 5.5 and 11 mbps,
 *   for 5.2GHz is 6, 12 and 24 mbps.
 * CFG_BASIC_RATE_SET_EXTRA:
 *   Make BSSBasicRateSet equal to OperateRateSet & ~11nRateSet.
 *   E.g. only STA which can work on all rates, which we support, can connect.
 * CFG_BASIC_RATE_SET_LEGACY:
 *   For 2.4GHz band only, includes 1 and 2 mbps,
 *   used for compatibility with old STAs, which supports only those two rates.
 */
static const uint32 BSSBasicRateSet[NUM_OF_NETWORK_MODES][NUM_OF_BASIC_RATE_SET_MODES] = {
  { 0x00007800, 0x00007f00, 0x00001800}, /* 11b@2.4GHz */
  { 0x00007800, 0x00007fff, 0x00001800}, /* 11g@2.4GHz */
  { 0x00007800, 0x00007fff, 0x00001800}, /* 11n@2.4GHz */
  { 0x00001800, 0x00007fff, 0x00001800}, /* 11bg@2.4GHz */
  { 0x00007800, 0x00007fff, 0x00001800}, /* 11gn@2.4GHz */
  { 0x00007800, 0x00007fff, 0x00001800}, /* 11bgn@2.4GHz */
  { 0x00000015, 0x000000ff, 0xfeedbeef}, /* 11a@5.2GHz */
  { 0x00000015, 0x000000ff, 0xfeedbeef}, /* 11n@5.2GHz */
  { 0x00000015, 0x000000ff, 0xfeedbeef}, /* 11an@5.2GHz */
};

uint32 __MTLK_IFUNC
get_operate_rate_set (uint8 net_mode)
{
  ASSERT(net_mode < NETWORK_NONE);
  return OperateRateSet[net_mode];
}

uint32 __MTLK_IFUNC
get_basic_rate_set (uint8 net_mode, uint8 mode)
{
  ASSERT(net_mode < NUM_OF_NETWORK_MODES);
  ASSERT(mode < NUM_OF_BASIC_RATE_SET_MODES);
  return BSSBasicRateSet[net_mode][mode];
}

/*
 * It should be noted, that
 * get_net_mode(net_mode_to_band(net_mode), is_ht_net_mode(net_mode)) != net_mode
 * because there are {ht, !ht}x{2.4GHz, 5.2GHz, both} == 6 combinations,
 * and there are 11 Network Modes.
 */
uint8 __MTLK_IFUNC get_net_mode (uint8 band, uint8 is_ht)
{
  switch (band) {
  case MTLK_HW_BAND_2_4_GHZ:
    if (is_ht)
      return NETWORK_11BGN_MIXED;
    else
      return NETWORK_11BG_MIXED;
  case MTLK_HW_BAND_5_2_GHZ:
    if (is_ht)
      return NETWORK_11AN_MIXED;
    else
      return NETWORK_11A_ONLY;
  case MTLK_HW_BAND_BOTH:
    if (is_ht)
      return NETWORK_11ABGN_MIXED;
    else
      return NETWORK_11ABG_MIXED;
  default:
    break;
  }

  ASSERT(0);

  return 0; /* just fake cc */
}

uint8 __MTLK_IFUNC net_mode_to_band (uint8 net_mode)
{
  switch (net_mode) {
  case NETWORK_11BG_MIXED: /* walk through */
  case NETWORK_11BGN_MIXED: /* walk through */
  case NETWORK_11B_ONLY: /* walk through */
  case NETWORK_11GN_MIXED: /* walk through */
  case NETWORK_11G_ONLY: /* walk through */
  case NETWORK_11N_2_4_ONLY:
    return MTLK_HW_BAND_2_4_GHZ;
  case NETWORK_11AN_MIXED: /* walk through */
  case NETWORK_11A_ONLY: /* walk through */
  case NETWORK_11N_5_ONLY:
    return MTLK_HW_BAND_5_2_GHZ;
  case NETWORK_11ABG_MIXED: /* walk through */
  case NETWORK_11ABGN_MIXED:
    return MTLK_HW_BAND_BOTH;
  default:
    break;
  }

  ASSERT(0);

  return 0; /* just fake cc */
}

BOOL __MTLK_IFUNC is_ht_net_mode (uint8 net_mode)
{
  switch (net_mode) {
  case NETWORK_11ABG_MIXED: /* walk through */
  case NETWORK_11A_ONLY: /* walk through */
  case NETWORK_11BG_MIXED: /* walk through */
  case NETWORK_11B_ONLY: /* walk through */
  case NETWORK_11G_ONLY:
    return FALSE;
  case NETWORK_11ABGN_MIXED: /* walk through */
  case NETWORK_11AN_MIXED: /* walk through */
  case NETWORK_11BGN_MIXED: /* walk through */
  case NETWORK_11GN_MIXED: /* walk through */
  case NETWORK_11N_2_4_ONLY: /* walk through */
  case NETWORK_11N_5_ONLY:
    return TRUE;
  default:
    break;
  }

  ASSERT(0);

  return FALSE; /* just fake cc */
}

BOOL __MTLK_IFUNC is_mixed_net_mode (uint8 net_mode)
{
  switch (net_mode) {
  case NETWORK_11ABG_MIXED:
  case NETWORK_11BG_MIXED:
  case NETWORK_11ABGN_MIXED:
  case NETWORK_11AN_MIXED:
  case NETWORK_11BGN_MIXED:
  case NETWORK_11GN_MIXED:
    return TRUE;
  case NETWORK_11B_ONLY:
  case NETWORK_11G_ONLY:
  case NETWORK_11A_ONLY:
  case NETWORK_11N_2_4_ONLY:
  case NETWORK_11N_5_ONLY:
    return FALSE;
  default:
    break;
  }

  ASSERT(0);

  return FALSE; /* just fake cc */
}

const char * __MTLK_IFUNC
net_mode_to_string (uint8 net_mode)
{ 
  switch (net_mode) {
  case NETWORK_11B_ONLY:
    return "802.11b";
  case NETWORK_11G_ONLY:
    return "802.11g";
  case NETWORK_11N_2_4_ONLY:
    return "802.11n(2.4)";
  case NETWORK_11BG_MIXED:
    return "802.11bg";
  case NETWORK_11GN_MIXED:
    return "802.11gn";
  case NETWORK_11BGN_MIXED:
    return "802.11bgn"; 
  case NETWORK_11A_ONLY:
    return "802.11a";
  case NETWORK_11N_5_ONLY:
    return "802.11n(5.2)";
  case NETWORK_11AN_MIXED:
    return "802.11an";
  case NETWORK_11ABG_MIXED:
    return "802.11abg";
  case NETWORK_11ABGN_MIXED:
    return "802.11abgn";
  }

  return "invalid mode";
}

/*
 * Due to lack of synchronization between fw-to-driver and driver-to-user
 * Network Mode should be converted on driver-to-user border.
 */
uint8 __MTLK_IFUNC
net_mode_ingress_filter (uint8 ingress_net_mode)
{
  switch (ingress_net_mode) {
  case NETWORK_MODE_11A:
    return NETWORK_11A_ONLY;
  case NETWORK_MODE_11ABG:
    return NETWORK_11ABG_MIXED;
  case NETWORK_MODE_11ABGN:
    return NETWORK_11ABGN_MIXED;
  case NETWORK_MODE_11AN:
    return NETWORK_11AN_MIXED;
  case NETWORK_MODE_11B:
    return NETWORK_11B_ONLY;
  case NETWORK_MODE_11BG:
    return NETWORK_11BG_MIXED;
  case NETWORK_MODE_11BGN:
    return NETWORK_11BGN_MIXED;
  case NETWORK_MODE_11GN:
    return NETWORK_11GN_MIXED;
  case NETWORK_MODE_11G:
    return NETWORK_11G_ONLY;
  case NETWORK_MODE_11N2:
    return NETWORK_11N_2_4_ONLY;
  case NETWORK_MODE_11N5:
    return NETWORK_11N_5_ONLY;
  default:
    break;
  }

  return NETWORK_NONE;
}

uint8 __MTLK_IFUNC
net_mode_egress_filter (uint8 egress_net_mode)
{
  switch (egress_net_mode) {
  case NETWORK_11ABG_MIXED:
    return NETWORK_MODE_11ABG;
  case NETWORK_11ABGN_MIXED:
    return NETWORK_MODE_11ABGN;
  case NETWORK_11B_ONLY:
    return NETWORK_MODE_11B;
  case NETWORK_11G_ONLY:
    return NETWORK_MODE_11G;
  case NETWORK_11N_2_4_ONLY:
    return NETWORK_MODE_11N2;
  case NETWORK_11BG_MIXED:
    return NETWORK_MODE_11BG;
  case NETWORK_11GN_MIXED:
    return NETWORK_MODE_11GN;
  case NETWORK_11BGN_MIXED:
    return NETWORK_MODE_11BGN;
  case NETWORK_11A_ONLY:
    return NETWORK_MODE_11A;
  case NETWORK_11N_5_ONLY:
    return NETWORK_MODE_11N5;
  case NETWORK_11AN_MIXED:
    return NETWORK_MODE_11AN;
  default:
    break;
  }

  ASSERT(0);

  return 0; /* just fake cc */
}

