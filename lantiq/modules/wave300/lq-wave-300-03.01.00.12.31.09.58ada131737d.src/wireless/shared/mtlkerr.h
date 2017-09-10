/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef _MTLK_ERR_H_
#define _MTLK_ERR_H_

#define MTLK_SUCCESS(code)     ( (code) >= 0)
#define MTLK_FAILURE(code)     ( (code) < 0)

#define MTLK_ERR_OK             (0)  /* no error                  */
#define MTLK_ERR_PARAMS        (-1)  /* invalid parameters        */
#define MTLK_ERR_UNKNOWN       (-2)  /* unknown error             */
#define MTLK_ERR_NOT_SUPPORTED (-3)  /* feature is not supported  */
#define MTLK_ERR_TIMEOUT       (-4)  /* timeout reached           */
#define MTLK_ERR_EEPROM        (-5)  /* EEPROM error              */
#define MTLK_ERR_NO_MEM        (-6)  /* mem allocation failed     */
#define MTLK_ERR_HW            (-7)  /* HW initializtion failed   */
#define MTLK_ERR_FW            (-8)  /* firmware not found/failed */
#define MTLK_ERR_UMI           (-9)  /* an UMI error              */
#define MTLK_ERR_PENDING       (-10) /* operation pending         */
#define MTLK_ERR_NOT_IN_USE    (-11) /* object not in use         */
#define MTLK_ERR_NO_RESOURCES  (-12) /* not enoug resources       */
#define MTLK_ERR_WRONG_CONTEXT (-13) /* unsuitable context for the required operation */
#define MTLK_ERR_NOT_READY     (-14) /* a necessary condition for beginning the operation is not met */
#define MTLK_ERR_SCAN_FAILED   (-15) /* failure during scan       */
#define MTLK_ERR_AOCS_FAILED   (-16) /* failure during aocs       */
#define MTLK_ERR_PROHIB        (-17) /* requested action is currently prohibited */
#define MTLK_ERR_BUF_TOO_SMALL (-18) /* buffer too small          */
#define MTLK_ERR_PKT_DROPPED   (-19) /* packet was dropped for some reason */
#define MTLK_ERR_FILEOP        (-20) /* an file operation failed for some reason */
#define MTLK_ERR_VALUE         (-21) /* invalid value */
#define MTLK_ERR_BUSY          (-22) /* object busy */
#define MTLK_ERR_MAC           (-23) /* MAC returns error */

#endif /* _MTLK_ERR_H_ */
