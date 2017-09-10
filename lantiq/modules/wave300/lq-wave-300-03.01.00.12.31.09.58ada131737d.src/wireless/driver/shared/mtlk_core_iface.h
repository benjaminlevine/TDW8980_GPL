/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
* $Id$
*
* Copyright (c) 2006-2008 Metalink Broadband (Israel)
*
* Cross - platfrom core module code
*/

#ifndef _MTLK_CORE_IFACE_H_
#define _MTLK_CORE_IFACE_H_

#include "mtlkhal.h"
#include "eeprom.h"
#ifdef MTCFG_RF_MANAGEMENT_MTLK
#include "mtlkasel.h"
#endif

mtlk_eeprom_data_t* __MTLK_IFUNC mtlk_core_get_eeprom(mtlk_core_t* core);

mtlk_hw_t* __MTLK_IFUNC mtlk_core_get_hw(mtlk_core_t* core);

uint8 mtlk_core_get_country_code (mtlk_core_t *core);
void mtlk_core_set_country_code (mtlk_core_t *core, uint8 country_code);

uint8 mtlk_core_get_dot11d (mtlk_core_t *core);
void mtlk_core_set_dot11d (mtlk_core_t *core, uint8 dot11d);

uint8 __MTLK_IFUNC mtlk_is_11h_radar_detection_enabled(mtlk_handle_t context);
uint8 __MTLK_IFUNC mtlk_is_device_busy(mtlk_handle_t context);
uint8 __MTLK_IFUNC
mtlk_core_get_bonding(mtlk_core_t *core);

/* Move the following prototype to frame master interface
   when frame module design is introduced */
#ifdef MTCFG_RF_MANAGEMENT_MTLK
mtlk_rf_mgmt_t *mtlk_get_rf_mgmt(mtlk_handle_t context);
#endif

#endif //_MTLK_CORE_IFACE_H_
