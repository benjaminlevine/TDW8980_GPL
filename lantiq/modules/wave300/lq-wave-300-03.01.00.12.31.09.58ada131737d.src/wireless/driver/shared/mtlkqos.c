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
 * Shared QoS
 *
 */
#include "mtlkinc.h"

#include "mtlkqos.h"
#include "bufmgr.h"

static mtlk_qos_do_classify_f do_classify = NULL;

static uint8 __wmm_aci_class_80211[NTS_TIDS] = {AC_BE, AC_BK, AC_BK, AC_BE, AC_VI, AC_VI, AC_VO, AC_VO};
static uint8 __wmm_aci_class_8021Q[NTS_TIDS] = {AC_BE, AC_BK, AC_VI, AC_VI, AC_VI, AC_VO, AC_VO, AC_VO};

static uint8 __wmm_aci_class_80211_inverted_vi_be[NTS_TIDS] = {AC_VI, AC_BK, AC_BK, AC_VI, AC_BE, AC_BE, AC_VO, AC_VO};
static uint8 __wmm_aci_class_8021Q_inverted_vi_be[NTS_TIDS] = {AC_VI, AC_BK, AC_BE, AC_BE, AC_BE, AC_VO, AC_VO, AC_VO};

static uint8 __wmm_ac_to_tid_mapping_80211[NTS_PRIORITIES] = {0, 1, 5, 6};
static uint8 __wmm_ac_to_tid_mapping_8021Q[NTS_PRIORITIES] = {0, 1, 4, 6};

static int    map_type              = USE_80211_MAP;
static uint8 *wmm_aci_class         = __wmm_aci_class_80211;
static uint8 *wmm_aci_class_for_map = __wmm_aci_class_80211; /* used for initial mapping - on TX packet arrival */
static uint8 *wmm_ac_to_tid_mapping = __wmm_ac_to_tid_mapping_80211;

MTLK_INIT_STEPS_LIST_BEGIN(mtlkqos)
  MTLK_INIT_STEPS_LIST_ENTRY(mtlkqos, LOCK_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(mtlkqos, RESET_ACM_BITS)
  MTLK_INIT_STEPS_LIST_ENTRY(mtlkqos, SET_MAP)
MTLK_INIT_INNER_STEPS_BEGIN(mtlkqos)
MTLK_INIT_STEPS_LIST_END(mtlkqos);


/*****************************************************************************
**
** NAME         video_classifier_register / video_classifier_unregister
**
** DESCRIPTION  This functions are used for registration of Metalink's
**              video classifier module
**
******************************************************************************/
int mtlk_qos_classifier_register (mtlk_qos_do_classify_f classify_fn)
{
  do_classify = classify_fn;
  return MTLK_ERR_OK;
}


void mtlk_qos_classifier_unregister (void)
{
  do_classify = NULL;
}

int __MTLK_IFUNC
mtlk_qos_init (struct mtlk_qos *qos)
{
  MTLK_INIT_TRY(mtlkqos, MTLK_OBJ_PTR(qos))
    MTLK_INIT_STEP(mtlkqos, LOCK_INIT, MTLK_OBJ_PTR(qos), 
                   mtlk_osal_lock_init, (&qos->acm_lock));
    MTLK_INIT_STEP_VOID(mtlkqos, RESET_ACM_BITS, MTLK_OBJ_PTR(qos), 
                        mtlk_qos_reset_acm_bits, (qos));
    MTLK_INIT_STEP_VOID(mtlkqos, SET_MAP, MTLK_OBJ_PTR(qos), 
                        mtlk_qos_set_map, (USE_80211_MAP));
  MTLK_INIT_FINALLY(mtlkqos, MTLK_OBJ_PTR(qos))
  MTLK_INIT_RETURN(mtlkqos, MTLK_OBJ_PTR(qos), mtlk_qos_cleanup, (qos));
}


void __MTLK_IFUNC
mtlk_qos_cleanup (struct mtlk_qos *qos)
{
  MTLK_CLEANUP_BEGIN(mtlkqos, MTLK_OBJ_PTR(qos))
    MTLK_CLEANUP_STEP(mtlkqos, SET_MAP, MTLK_OBJ_PTR(qos),
                      MTLK_NOACTION, ());
    MTLK_CLEANUP_STEP(mtlkqos, RESET_ACM_BITS, MTLK_OBJ_PTR(qos),
                      MTLK_NOACTION, ());
    MTLK_CLEANUP_STEP(mtlkqos, LOCK_INIT, MTLK_OBJ_PTR(qos),
                      mtlk_osal_lock_cleanup, (&qos->acm_lock));
  MTLK_CLEANUP_END(mtlkqos, MTLK_OBJ_PTR(qos));
}


int __MTLK_IFUNC
mtlk_qos_set_map (int map)
{
 int         res      = MTLK_ERR_PARAMS;
  const char *map_name = NULL;

  switch (map) {
  case USE_80211_MAP:
    wmm_aci_class         = __wmm_aci_class_80211;
    wmm_aci_class_for_map = __wmm_aci_class_80211;
    wmm_ac_to_tid_mapping = __wmm_ac_to_tid_mapping_80211;
    map_name = "802.11";
    break;
  case USE_8021Q_MAP:
    wmm_aci_class         = __wmm_aci_class_8021Q;
    wmm_aci_class_for_map = __wmm_aci_class_8021Q;
    wmm_ac_to_tid_mapping = __wmm_ac_to_tid_mapping_8021Q;
    map_name ="802.1Q";
    break;
  case USE_80211_MAP_INVERTED_VI_BE:
    wmm_aci_class         = __wmm_aci_class_80211;
    wmm_aci_class_for_map = __wmm_aci_class_80211_inverted_vi_be;
    wmm_ac_to_tid_mapping = __wmm_ac_to_tid_mapping_80211;
    map_name = "802.11 (VI <=> BE)";
    break;
  case USE_8021Q_MAP_INVERTED_VI_BE:
    wmm_aci_class         = __wmm_aci_class_8021Q;
    wmm_aci_class_for_map = __wmm_aci_class_8021Q_inverted_vi_be;
    wmm_ac_to_tid_mapping = __wmm_ac_to_tid_mapping_8021Q;
    map_name ="802.1Q (VI <=> BE)";
    break;
  default:
    break;
  }

  if (map_name) {
    LOG1("Set %s TID-to-AC mapping", map_name);
    map_type = map;
    res      = MTLK_ERR_OK;
  }

  return res;
}

int __MTLK_IFUNC
mtlk_qos_get_map (void)
{
  return map_type;
}

BOOL __MTLK_IFUNC
mtlk_qos_is_map_8021Q_based (void)
{
  return (map_type == USE_8021Q_MAP || map_type == USE_8021Q_MAP_INVERTED_VI_BE);
}

static void
reset_acm_bits_unsafe (struct mtlk_qos *qos)
{
  uint8 i;
  for (i = 0; i < NTS_PRIORITIES; i++)
    qos->acm_substitution[i] = i;
}


void __MTLK_IFUNC
mtlk_qos_set_acm_bits (struct mtlk_qos *qos, const uint8 *acm_state_table)
{
  uint16 i;

  mtlk_osal_lock_acquire(&qos->acm_lock);

  reset_acm_bits_unsafe(qos);

  for (i = 0; i < NTS_PRIORITIES; i++) {
    uint16 ac = mtlk_get_ac_by_number(i);
    ILOG1(GID_QOS, "Set ACM bit for priority %d: %d", i, acm_state_table[i]);

    if (acm_state_table[ac]) {
      qos->acm_substitution[ac] =
                ( (i == 0) ? (AC_INVALID)
                           : (qos->acm_substitution[mtlk_get_ac_by_number(i - 1)]) );
    }
  }

  mtlk_osal_lock_release(&qos->acm_lock);
}


void __MTLK_IFUNC
mtlk_qos_reset_acm_bits (struct mtlk_qos *qos)
{
  ASSERT(qos != NULL);

  mtlk_osal_lock_acquire(&qos->acm_lock);
  reset_acm_bits_unsafe(qos);
  mtlk_osal_lock_release(&qos->acm_lock);
}


static __INLINE uint16
acm_adjust_ac (struct mtlk_qos *qos, uint16 ac)
{
  uint16 adjusted_ac;

  ASSERT(qos != NULL);
  ASSERT(ac < ARRAY_SIZE(qos->acm_substitution));

  mtlk_osal_lock_acquire(&qos->acm_lock);
  adjusted_ac = qos->acm_substitution[ac];
  mtlk_osal_lock_release(&qos->acm_lock);

  return adjusted_ac;
}

static __INLINE uint16 
_mtlk_qos_map_ac_by_tid (uint16 tid)
{
  ASSERT(tid < NTS_TIDS);
  return wmm_aci_class_for_map[tid & 0x07];
}


uint16 __MTLK_IFUNC
mtlk_qos_get_ac (struct mtlk_qos *qos, mtlk_buffer_t *pbuf)
{
  uint16 priority = MTLK_WMM_ACI_DEFAULT_CLASS;
  uint16 ac;

  /* try to classify packet with external classifier, override default
   * skb's priority in case of success */
  if (do_classify != NULL) {
    (*do_classify)(pbuf);
    /* try packet's priority assigned by OS */
    priority = mtlk_bufmgr_get_priority(pbuf);
  } else {
    /* Get priority from TOS bits (IP priority) */
    mtlk_bufmgr_get_priority_from_tos(pbuf, &priority);
  }

  /* get AC for a given priority */
  ac = _mtlk_qos_map_ac_by_tid(priority);

  /* perform AC substitution */
  ac = acm_adjust_ac(qos, ac);
  if (__LIKELY(ac != AC_INVALID)) {
    /* update packet's priority */
    mtlk_bufmgr_set_priority(pbuf, mtlk_qos_get_tid_by_ac(ac));
  }

  return ac;
}


/*
* Getting the QoS value from the priority
*/
uint16 __MTLK_IFUNC
mtlk_qos_get_ac_by_tid (uint16 tid)
{
  ASSERT(tid < NTS_TIDS);
  return wmm_aci_class[tid & 0x07];
}

uint16 __MTLK_IFUNC
mtlk_qos_get_tid_by_ac (uint16 ac)
{
  ASSERT(ac < NTS_PRIORITIES);
  return wmm_ac_to_tid_mapping[ac & 0x03];
}

const char * __MTLK_IFUNC
mtlk_qos_get_ac_name (int ac_idx)
{
  static const char *ac_names[NTS_PRIORITIES] =
  {
    "AC_BE",
    "AC_BK",
    "AC_VI",
    "AC_VO"
  };   

  ASSERT (ac_idx < NTS_PRIORITIES && ac_idx >= 0);

  return ac_names[ac_idx];
}

