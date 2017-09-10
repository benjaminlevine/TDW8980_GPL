/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef SAFE_PLACE_TO_INCLUDE_DFNET_OSDEP 
#error "You shouldn't include this file directly!"
#endif

static __INLINE void *
mtlk_df_ndev_priv (mtlk_ndev_t *ndev)
{
  return netdev_priv(ndev);
}

static __INLINE int
mtlk_df_ndev_rx (mtlk_ndev_t *ndev, mtlk_nbuf_t *nbuf)
{
  nbuf->dev = ndev;
  netif_rx(nbuf);
  return MTLK_ERR_OK;
}

static __INLINE int
mtlk_df_ndev_tx_disable (mtlk_ndev_t *ndev)
{
  netif_stop_queue(ndev);
  return MTLK_ERR_OK;
}

static __INLINE int
mtlk_df_ndev_tx_enable (mtlk_ndev_t *ndev)
{
  netif_start_queue(ndev); 
  return MTLK_ERR_OK;
}

static __INLINE BOOL
mtlk_df_ndev_tx_is_enabled (mtlk_ndev_t *ndev)
{
  return !netif_queue_stopped(ndev);
}

static __INLINE int
mtlk_df_ndev_carrier_on (mtlk_ndev_t *ndev)
{
  netif_carrier_on(ndev);
  return MTLK_ERR_OK;
}

static __INLINE int
mtlk_df_ndev_carrier_off (mtlk_ndev_t *ndev)
{
  netif_carrier_off(ndev);
  return MTLK_ERR_OK;
}

static __INLINE BOOL
mtlk_df_ndev_carrier_is_on (mtlk_ndev_t *ndev)
{
  return netif_carrier_ok(ndev);
}

static __INLINE void *
mtlk_df_nbuf_priv(mtlk_nbuf_t *nbuf)
{
  return (void *)nbuf->cb;
}

static __INLINE mtlk_nbuf_t *
mtlk_df_nbuf_alloc (mtlk_df_t *df, uint32 size)
{
  return dev_alloc_skb(size);
}

static __INLINE void
mtlk_df_nbuf_free (mtlk_df_t *df, mtlk_nbuf_t *nbuf)
{
  dev_kfree_skb(nbuf);
}

static __INLINE void *
mtlk_df_nbuf_get_virt_addr (mtlk_nbuf_t *nbuf)
{
  return nbuf->data;
}

static __INLINE uint32
mtlk_df_nbuf_map_to_phys_addr (mtlk_df_t       *df,
                               mtlk_nbuf_t     *nbuf,
                               uint32           size,
                               mtlk_nbuf_type_e type)
{
  int dir = (type == MTLK_NBUF_RX)?PCI_DMA_FROMDEVICE:PCI_DMA_TODEVICE;

  return pci_map_single(df->dev,
                        nbuf->data,
                        size,
                        dir); 
}

static __INLINE uint32
mtlk_map_to_phys_addr (mtlk_df_t       *df,
                       void            *buffer,
                       uint32           size,
                       mtlk_nbuf_type_e type)
{
  int dir = (type == MTLK_NBUF_RX)?PCI_DMA_FROMDEVICE:PCI_DMA_TODEVICE;

  return pci_map_single(df->dev,
                        buffer,
                        size,
                        dir); 
}

static __INLINE void 
mtlk_df_nbuf_unmap_phys_addr (mtlk_df_t       *df,
                              mtlk_nbuf_t     *nbuf,
                              uint32           addr,
                              uint32           size,
                              mtlk_nbuf_type_e type)
{
  int dir = (type == MTLK_NBUF_RX)?PCI_DMA_FROMDEVICE:PCI_DMA_TODEVICE;
  pci_unmap_single(df->dev,
                   addr,
                   size,
                   dir);
}

static __INLINE void 
mtlk_unmap_phys_addr (mtlk_df_t       *df,
                      uint32           addr,
                      uint32           size,
                      mtlk_nbuf_type_e type)
{
  int dir = (type == MTLK_NBUF_RX)?PCI_DMA_FROMDEVICE:PCI_DMA_TODEVICE;
  pci_unmap_single(df->dev,
                   addr,
                   size,
                   dir);
}

static __INLINE void 
mtlk_df_nbuf_reserve (mtlk_nbuf_t *nbuf, uint32 len)
{
  skb_reserve(nbuf, len);
}

static __INLINE void *
mtlk_df_nbuf_put (mtlk_nbuf_t *nbuf, uint32 len)
{
  return skb_put(nbuf, len);
}

static __INLINE void
mtlk_df_nbuf_trim (mtlk_nbuf_t *nbuf, uint32 len)
{
  skb_trim(nbuf, len);
}

static __INLINE void *
mtlk_df_nbuf_pull (mtlk_nbuf_t *nbuf, uint32 len)
{
  return skb_pull(nbuf, len);
}

#undef SAFE_PLACE_TO_INCLUDE_DFNET_OSDEP
