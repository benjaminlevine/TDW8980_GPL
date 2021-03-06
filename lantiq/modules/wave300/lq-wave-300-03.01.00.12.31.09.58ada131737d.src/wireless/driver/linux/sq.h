/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id:$
 *
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *  
 * Parts of "send queue" internal to Linux driver.
 *
 *
 */

#ifndef __SQ_H__
#define __SQ_H__

/**
 * \brief   Initialize all the stuff needed to use SendQueue.
 * \fn      int sq_init (struct nic *)
 * \param   nic pointer to "struct nic" of current network device.
 * \return  Success/failure status (MTLK_ERR_*).
 */

int sq_init (struct nic *nic);

/**
 * \brief   Clean all resources used by SendQueue.
 * \fn      int sq_cleanup (struct nic *)
 * \param   nic pointer to "struct nic" of current network device.
 * \return  Success/failure status (MTLK_ERR_*).
 */

int sq_cleanup (struct nic *nic);

/**
 * \brief   Send to UM as much packets as possible from the SendQueue.
 * \fn      void mtlk_xmit_sq_flush (struct nic *nic)
 * \param   nic pointer to "struct nic" of current network device.
 */

void mtlk_xmit_sq_flush (struct nic *nic);

/**
 * \brief   Schedule SendQueue "flush".
 * \fn      void mtlk_sq_schedule_flush(struct nic *nic);
 * \param   nic pointer to "struct nic" of current network device.
 */

void mtlk_sq_schedule_flush(struct nic *nic);

/**
 * \brief   Prints debug info.
 * \fn      void mtlk_sq_show_stats (struct seq_file *sf, void *v, struct nic *nic);
 * \param   sf  sequential file to print to.
 * \param   v   sequential file "iterator".
 * \param   nic pointer to "struct nic" of current network device.
 */

void mtlk_sq_show_stats (struct seq_file *sf, void *v, struct nic *nic);

/**
 * \brief   Puts current SendQueue limits into array of int 
 *          (called from private ioctl)
 * \fn      int sq_get_limits(struct nic *nic, int32 *limits, int *written_num);
 * \param   nic         pointer to "struct nic" of current network device.
 * \param   limits      pointer to the destination array.
 * \param   written_num address of the variable to store number of written values.
 */

int sq_get_limits(struct nic *nic, int32 *limits, int *written_num);

/**
 * \brief   Puts current SendQueue peer limits into array of int 
 *          (called from private ioctl)
 * \fn      int sq_get_peer_limits(struct nic *nic, int32 *ratio, int *written_num);
 * \param   nic         pointer to "struct nic" of current network device.
 * \param   ratio       pointer to the destination array.
 * \param   written_num address of the variable to store number of written values.
 */

int sq_get_peer_limits(struct nic *nic, int32 *ratio, int *written_num);

/**
 * \brief   Sets SendQueue limits according to values from array of int 
 *          (called from private ioctl)
 * \fn      int sq_set_limits(struct nic *nic, int32 *global_queue_size, int num);
 * \param   nic         pointer to "struct nic" of current network device.
 * \param   global_queue_size pointer to the array of new limits.
 * \param   num         number of values in the array.
 */

int sq_set_limits(struct nic *nic, int32 *global_queue_size, int num);

/**
 * \brief   Sets SendQueue peer limits according to values from array of int 
 *          (called from private ioctl)
 * \fn      int sq_set_peer_limits(struct nic *nic, int32 *ratio, int num);
 * \param   nic         pointer to "struct nic" of current network device.
 * \param   ratio       pointer to the array of new limits.
 * \param   num         number of values in the array.
 */

int sq_set_peer_limits(struct nic *nic, int32 *ratio, int num);

#endif /* __SQ_H__ */
