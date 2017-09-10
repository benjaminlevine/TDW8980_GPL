/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"

#include "progmodel.h"
#include "mtlkhal.h"
#include "mtlkparams.h"
#include "core.h"
#include "mhi_umi.h" 

#define MTLK_PRGMDL_LOAD_START_TIMEOUT 10000 /* ms */ 

static void
mtlk_progmodel_free_os(mtlk_progmodel_t *fw);

struct mtlk_progmodel
{
   mtlk_core_firmware_file_t ff[MTLK_PRGMDL_TYPE_LAST];
   mtlk_prgmdl_parse_t       prgmdl_params;
   BOOL                      is_loaded_from_os;
   struct nic               *nic;
   mtlk_txmm_t              *txmm;
};

static int 
mtlk_progmodel_prepare_loading_to_hw (const mtlk_progmodel_t *fw) 
{ 
  int               res = MTLK_ERR_UNKNOWN; 
  mtlk_txmm_msg_t   man_msg; 
  mtlk_txmm_data_t *man_entry = NULL; 
 
  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, 
                                                 fw->txmm, 
                                                 &res); 
  if (!man_entry) { 
    ELOG("Can't prepare progmodel loading due to lack of MM (err=%d)", res); 
    goto FINISH; 
  } 
 
   man_entry->id = UM_MAN_DOWNLOAD_PROG_MODEL_PERMISSION_REQ; 
   man_entry->payload_size = 0; 
   res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_PRGMDL_LOAD_START_TIMEOUT); 
   if (res != MTLK_ERR_OK) { 
     ELOG("Can't prepare FW for downloading programming model, timed-out. Err#%d", res); 
     goto FINISH; 
   } 
 
   res = MTLK_ERR_OK; 
 
FINISH: 
  if (man_entry) { 
    mtlk_txmm_msg_cleanup(&man_msg); 
  } 
 
  return res; 
} 

static void
_mtlk_progmodel_cleanup(mtlk_progmodel_t *fw)
{
  if (fw->is_loaded_from_os)
    mtlk_progmodel_free_os(fw);

}

static int
_mtlk_progmodel_init(mtlk_progmodel_t *fw, mtlk_txmm_t *txmm, struct nic *nic, int freq, int force, int spectrum)
{
  char *mib_val;
  
  MTLK_ASSERT(  fw != NULL);
  MTLK_ASSERT( nic != NULL);
  MTLK_ASSERT(txmm != NULL);

  fw->prgmdl_params.freq = freq;
  fw->nic = nic;
  fw->txmm = txmm;

  /* determine the spectrum to use (aka Channel Bonding) */
  if (!force) 
  {
    if (is_ht_net_mode(nic->slow_ctx->net_mode_cur)) 
    {
      mib_val = mtlk_get_mib_value(PRM_SPECTRUM_MODE, nic);
      fw->prgmdl_params.spectrum_mode = mtlk_aux_atol(mib_val);
    } 
    else 
    {
      fw->prgmdl_params.spectrum_mode = MIB_SPECTRUM_20M;
    }
  } 
  else 
  {
    /* spectrum is forced by the caller */
    fw->prgmdl_params.spectrum_mode = spectrum;
  }

  if (!nic->slow_ctx->ee_data.valid) 
  {
    goto ERROR;
  } 
  else 
  {
    fw->prgmdl_params.hw_type = mtlk_eeprom_get_nic_type(&nic->slow_ctx->ee_data);
    fw->prgmdl_params.hw_revision = mtlk_eeprom_get_nic_revision(&nic->slow_ctx->ee_data);
  }
  
  return MTLK_ERR_OK;

ERROR:
  return MTLK_ERR_EEPROM;  
}

mtlk_progmodel_t * __MTLK_IFUNC
mtlk_progmodel_create(mtlk_txmm_t *txmm, struct nic *nic, int freq, int force, int spectrum)
{
  mtlk_progmodel_t *fw = mtlk_osal_mem_alloc(sizeof(mtlk_progmodel_t), MTLK_MEM_TAG_PROGMODEL);
  
  MTLK_ASSERT(freq == MTLK_HW_BAND_5_2_GHZ || freq == MTLK_HW_BAND_2_4_GHZ);

  if (fw != NULL)
  {
	memset(fw, 0, sizeof(*fw));
	
    if (_mtlk_progmodel_init(fw, txmm, nic, freq, force, spectrum) == MTLK_ERR_OK) 
    {
      LOG1("Progmodel initialized");
    }
    else 
    {
      ERROR("Can't init Progmodel object");
      mtlk_osal_mem_free(fw);
      fw = NULL;
    }  
  }
  else
  {
    ELOG("Unable to create progmodel object");
  }

  return fw;
}

uint8 __MTLK_IFUNC
mtlk_progmodel_get_spectrum(mtlk_progmodel_t *fw)
{
  return fw->prgmdl_params.spectrum_mode;
}

static void
mtlk_progmodel_free_os(mtlk_progmodel_t *fw)
{
  int i;

  MTLK_ASSERT(fw->is_loaded_from_os); /* "free already freed" attempts should be eliminated */
  if (!fw->is_loaded_from_os)
    return;

  /* we shouldn't free by ourself,
   * instead we should rely on HW and OS dependent mechanisms
   */
  for (i = 0; i < MTLK_PRGMDL_TYPE_LAST; i++)
    mtlk_hw_set_prop(fw->nic->hw, MTLK_HW_PROGMODEL_FREE, &fw->ff[i], 0);

  fw->is_loaded_from_os = FALSE;
}

int __MTLK_IFUNC
mtlk_progmodel_load_from_os(mtlk_progmodel_t *fw)
{
  int i, t;
  int res;

  MTLK_ASSERT(!fw->is_loaded_from_os); /* double load attempts should be eliminated */
  if (fw->is_loaded_from_os)
    return MTLK_ERR_OK;

  for (i = 0; i < MTLK_PRGMDL_TYPE_LAST; i++) {
    /* get file name to download */
    res = mtlk_prgmdl_get_fname(i, &fw->prgmdl_params, fw->ff[i].fname);

    if (res != MTLK_ERR_OK) {
      ELOG("Couldn't get progmodel %d file name: error %d", i, res);
      res = MTLK_ERR_UNKNOWN;
      goto ERROR;
    }

    ILOG1(GID_PROGMODEL, "Requesting firmware %s", fw->ff[i].fname);

    res = mtlk_hw_get_prop(fw->nic->hw, MTLK_HW_PROGMODEL, &fw->ff[i], 0);
    if (res != MTLK_ERR_OK) {
      ELOG("ERROR: Progmodel (%s) loading failed. Err#%d", fw->ff[i].fname, res);
      res = MTLK_ERR_UNKNOWN;
      goto ERROR;
    }
  }

  fw->is_loaded_from_os = TRUE;

  return MTLK_ERR_OK;

ERROR:
  /* free what was loaded previously */
  for (t = 0; t < i; t++)
    mtlk_hw_set_prop(fw->nic->hw, MTLK_HW_PROGMODEL_FREE, &fw->ff[t], 0);

  return res;
}

BOOL __MTLK_IFUNC
mtlk_progmodel_is_loaded(const mtlk_progmodel_t *fw)
{
  if (fw->prgmdl_params.freq == mtlk_core_get_last_pm_freq(fw->nic) &&
      fw->prgmdl_params.spectrum_mode == mtlk_core_get_last_pm_spectrum(fw->nic))
    /* already loaded */
    return TRUE;

  return FALSE;
}

int __MTLK_IFUNC
mtlk_progmodel_load_to_hw(const mtlk_progmodel_t *fw)
{
  int i;
  int res = mtlk_progmodel_prepare_loading_to_hw(fw);

  if (res != MTLK_ERR_OK) { 
	    goto FINISH; 
  } 

  for (i = 0; i < MTLK_PRGMDL_TYPE_LAST; i++) {
    /* we pray that HW won't truncate our file buffer,
     * therefore we cast away constness 
     */
    res = mtlk_hw_set_prop(fw->nic->hw, MTLK_HW_PROGMODEL, (mtlk_core_firmware_file_t *)&fw->ff[i], 0);
    if (res != MTLK_ERR_OK) {
      ELOG("ERROR: Progmodel (%s) loading failed. Err#%d", fw->ff[i].fname, res);
      goto FINISH;
    }
  }

  ILOG4(GID_PROGMODEL, "Loaded progmodel for freq = %d", fw->prgmdl_params.freq);
  fw->nic->slow_ctx->last_pm_freq = fw->prgmdl_params.freq;
  fw->nic->slow_ctx->last_pm_spectrum = fw->prgmdl_params.spectrum_mode;

FINISH:
  return res;
}

void __MTLK_IFUNC
mtlk_progmodel_delete(mtlk_progmodel_t *fw)
{
  MTLK_ASSERT(fw != NULL);

  _mtlk_progmodel_cleanup(fw);
  mtlk_osal_mem_free(fw);  
}

/* Progmodel loading */

/* the caller can force spectrum (CB/nCB) to be used by setting
 * "force" to non-zero and passing needed mode in "spectrum"
 * parameter, in case of "force" set to zero PRM_SPECTRUM_MODE
 * mib value will be used.
 */
int
mtlk_progmodel_load(mtlk_txmm_t *txmm, struct nic *nic, int freq, int force, int spectrum)
{
  mtlk_progmodel_t *fw = mtlk_progmodel_create(txmm, nic, freq, force, spectrum);
  int res;

  if (fw == NULL) {
    ELOG("Unable to create progmodel");
    return MTLK_ERR_NO_MEM;
  }

  /* Check if we need to load progmodels */
  if (mtlk_progmodel_is_loaded(fw)) {
    ILOG3(GID_PROGMODEL, "Progmodel already loaded");
    res = MTLK_ERR_OK;
    goto FINISH;
  }

  res = mtlk_progmodel_load_from_os(fw);
  if (res != MTLK_ERR_OK) {
    ELOG("ERROR: Progmodel load from OS failed. Err#%d", res);
    goto FINISH;
  }

  res = mtlk_progmodel_load_to_hw(fw);
  if (res != MTLK_ERR_OK)
    ELOG("ERROR: Progmodel load to HW failed. Err#%d", res);

FINISH:
  mtlk_progmodel_delete(fw);

  return res;
}

