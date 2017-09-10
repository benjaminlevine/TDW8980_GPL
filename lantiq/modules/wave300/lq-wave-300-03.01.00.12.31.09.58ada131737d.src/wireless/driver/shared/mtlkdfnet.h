/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __MTLK_DF_NET_H__
#define __MTLK_DF_NET_H__

#include "mtlkdfdefs.h" /* mtlk_df_t must be defined here */

/*****************************************************************************
 * Driver Framework: Network Interface related part
 *****************************************************************************/
/* net_osdep.h contains the following definitions:
 *  - mtlk_ndev_t         - network device 
 *  - mtlk_nbuf_t         - network buffer
 *  - MTLK_NDEV_PRIV_SIZE - network device's private area size (see NOTE 1)
 *  - MTLK_NBUF_PRIV_SIZE - network buffer's private area size (see NOTE 2)
 *
 * NOTE 1:  this defines size of data should be accessed via the mtlk_ndev_t.
 *          It is used to store OS specific data required for the 
 *          mtlk_df_ndev_clb_... handling.
 *          For example, Linux driver will store Core object there. 
 *          It will be reviewed once we have unified drivers structure 
 *          in all the OSes.
 * NOTE 2:  this defines size of data should be accessed via mtlk_nbuf_t.
 *          It is used to store store Packet Processing related data
 *          during the packets handling.
 *          For example, Linux driver will store struct skb_private there. 
 *          It will be reviewed once we have unified drivers structure 
 *          in all the OSes.
 * WARNING: MTLK_NBUF_PRIV_SIZE should not exceed 40 bytes because of Linux
 *          sk_buff private area limitation in older kernels.
 */
#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/*****************************************************************************
 * Driver => OS Network Stack calls
 *****************************************************************************/
static __INLINE void *mtlk_df_ndev_priv(mtlk_ndev_t *ndev);
static __INLINE int   mtlk_df_ndev_rx(mtlk_ndev_t *ndev, mtlk_nbuf_t *nbuf);
static __INLINE int   mtlk_df_ndev_tx_disable(mtlk_ndev_t *ndev);
static __INLINE int   mtlk_df_ndev_tx_enable(mtlk_ndev_t *ndev);
static __INLINE BOOL  mtlk_df_ndev_tx_is_enabled(mtlk_ndev_t *ndev);
static __INLINE int   mtlk_df_ndev_carrier_on(mtlk_ndev_t *ndev);
static __INLINE int   mtlk_df_ndev_carrier_off(mtlk_ndev_t *ndev);
static __INLINE BOOL  mtlk_df_ndev_carrier_is_on(mtlk_ndev_t *ndev);
/*****************************************************************************/

/*****************************************************************************
 * OS Network Stack => Driver calls
 *****************************************************************************/
int          mtlk_df_ndev_clb_tx(mtlk_ndev_t *ndev, mtlk_nbuf_t *nbuf);

int          mtlk_df_ndev_clb_rx_disable(mtlk_ndev_t *ndev);
int          mtlk_df_ndev_clb_rx_enable(mtlk_ndev_t *ndev);
BOOL         mtlk_df_ndev_clb_rx_is_enabled(mtlk_ndev_t *ndev);
/*****************************************************************************/

/*****************************************************************************
 * Network Buffer (former - Payload) Interface
 *****************************************************************************/
typedef enum
{
  MTLK_NBUF_RX,
  MTLK_NBUF_TX,
  MTLK_NBUF_LAST
} mtlk_nbuf_type_e;

static __INLINE void        *mtlk_df_nbuf_priv(mtlk_nbuf_t *nbuf);
static __INLINE mtlk_nbuf_t *mtlk_df_nbuf_alloc(mtlk_df_t *df, uint32 size);
static __INLINE void         mtlk_df_nbuf_free(mtlk_df_t *df, mtlk_nbuf_t *nbuf);
static __INLINE void        *mtlk_df_nbuf_get_virt_addr(mtlk_nbuf_t *nbuf);
static __INLINE uint32       mtlk_df_nbuf_map_to_phys_addr(mtlk_df_t       *df,
                                                           mtlk_nbuf_t     *nbuf,
                                                           uint32           size,
                                                           mtlk_nbuf_type_e type);
static __INLINE void         mtlk_df_nbuf_unmap_phys_addr(mtlk_df_t       *df,
                                                          mtlk_nbuf_t     *nbuf,
                                                          uint32           addr,
                                                          uint32           size,
                                                          mtlk_nbuf_type_e type);
static __INLINE void         mtlk_df_nbuf_reserve(mtlk_nbuf_t *nbuf, uint32 len);
static __INLINE void        *mtlk_df_nbuf_put(mtlk_nbuf_t *nbuf, uint32 len);
static __INLINE void         mtlk_df_nbuf_trim(mtlk_nbuf_t *nbuf, uint32 len);
static __INLINE void         *mtlk_df_nbuf_pull(mtlk_nbuf_t *nbuf, uint32 len);
/*****************************************************************************/

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#define  SAFE_PLACE_TO_INCLUDE_DFNET_OSDEP 
#include "dfnet_osdep.h" 

#endif /* __MTLK_DF_NET_H__ */
