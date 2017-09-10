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
 * Written by: Dmitry Fleytman
 *
 */

#ifndef _MTLK_BUFMGR_H_
#define _MTLK_BUFMGR_H_

#include "mtlk_packets.h"
#include "mtlkaux.h"
#include "mtlkqos.h"

#define SAFE_PLACE_TO_INCLUDE_BUFMGR_OSDEP_DECLS
#include "bufmgr_osdep_decls.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"
/** 
*\file bufmgr.h
*\brief Buffer Management API
*\brief Cross platform API to manipulate OS-dependent network buffers.
*Buffer Management API is intended to manipulate OS-dependent network buffers in a cross-platform manner.
*\defgroup BufMgrModule Buffer management

*\{
*/

/*! 
\fn      static __INLINE int mtlk_bufmgr_read(mtlk_buffer_t* pbuffer, uint32 offset, uint32 length, uint8* destination)
\brief   Read data from data buffer. 

\param   pbuffer        Pointer to data buffer
\param   offset         Offset from buffer beginning
\param   length         Number of bytes to read
\param   destination    Destination buffer

\return  MTLK_ERR... values, MTLK_ERR_OK if succeeded
*/
static __INLINE int mtlk_bufmgr_read(mtlk_buffer_t *pbuffer, 
                                     uint32         offset, 
                                     uint32         length, 
                                     uint8         *destination);

/*! 
\fn      static __INLINE int mtlk_bufmgr_write(mtlk_buffer_t* pbuffer, uint32 offset, uint32 length, uint8* source)
\brief   Write data to data buffer. 

\param   pbuffer        Pointer to data buffer
\param   offset         Offset from buffer beginning
\param   length         Number of bytes to write
\param   destination    Source buffer

\return  MTLK_ERR... values, MTLK_ERR_OK if succeeded
*/
static __INLINE int mtlk_bufmgr_write(mtlk_buffer_t *pbuffer, 
                                      uint32         offset, 
                                      uint32         length, 
                                      uint8         *source);

/*! 
\fn      static __INLINE int32 mtlk_bufmgr_query_length(mtlk_buffer_t* pbuffer)
\brief   Query total data buffer length. 

\param   pbuffer        Pointer to data buffer

\return  Number of bytes
*/

static __INLINE int32 mtlk_bufmgr_query_length(mtlk_buffer_t *pbuffer);

/*! 
\fn      static __INLINE int16 mtlk_bufmgr_get_priority(mtlk_buffer_t* pbuffer)
\brief   Get packet priority supplied by OS. 

\param   pbuffer        Pointer to data buffer

\return  Priority value
*/
static __INLINE uint16 mtlk_bufmgr_get_priority(mtlk_buffer_t *pbuffer);

/*! 
\fn      static __INLINE int32 mtlk_bufmgr_set_priority(mtlk_buffer_t* pbuffer, uint16 priority)
\brief   Set packet's priority for OS. 

\param   pbuffer        Pointer to data buffer
\param   priority       Paket priority value
*/
static __INLINE void mtlk_bufmgr_set_priority(mtlk_buffer_t *pbuffer, 
                                              uint16         priority);

/*! 
\fn      static __INLINE int16 mtlk_bufmgr_is_urgent(mtlk_buffer_t* pbuffer)
\brief   Returns packet urgency sign. 

\param   pbuffer        Pointer to data buffer

\return  Priority value
*/
static __INLINE uint8 mtlk_bufmgr_is_urgent(mtlk_buffer_t *pbuffer);

/*! 
\fn      static __INLINE int32 mtlk_bufmgr_set_urgency(mtlk_buffer_t* pbuffer)
\brief   Mark packet as urgent. 

\param   pbuffer        Pointer to data buffer
*/
static __INLINE void mtlk_bufmgr_set_urgency(mtlk_buffer_t *pbuffer);

/*! 
\fn      static __INLINE int32 mtlk_bufmgr_clear_urgency(mtlk_buffer_t* pbuffer)
\brief   Mark packet as not urgent. 

\param   pbuffer        Pointer to data buffer
*/
static __INLINE void mtlk_bufmgr_clear_urgency(mtlk_buffer_t *pbuffer);

/*! 
\fn      static __INLINE void* mtlk_bufmgr_get_cancel_id(mtlk_buffer_t* pbuffer)
\brief   Get packet's cancel ID. 

\param   pbuffer        Pointer to data buffer

\return  Cancel ID
*/
static __INLINE void *mtlk_bufmgr_get_cancel_id(mtlk_buffer_t *pbuffer);

/*! 
\fn      static __INLINE void mtlk_bufmgr_set_cancel_id(mtlk_buffer_t* pbuffer, void* cancelid)
\brief   Set packet's cancel ID. 

\param   pbuffer        Pointer to data buffer
\param   cancelid       Cancel ID to set
*/

static __INLINE void mtlk_bufmgr_set_cancel_id(mtlk_buffer_t *pbuffer, 
                                               void          *cancelid);

/*****************************************************************************/
/* Buffer lists (doubly linked)                                              */
/*****************************************************************************/

/*! 
  \fn      static __INLINE void mtlk_buflist_init(mtlk_buflist_t* pbuflist)
  \brief   Function initializes list object

  \param   pbuflist List object
 */
static __INLINE void mtlk_buflist_init(mtlk_buflist_t *pbuflist);

/*! 
  \fn      static __INLINE void mtlk_buflist_cleanup(mtlk_buflist_t* pbuflist)
  \brief   Function cleanups list object, also asserts in case list is not empty.

  \param   pbuflist List object
 */
static __INLINE void mtlk_buflist_cleanup(mtlk_buflist_t *pbuflist);

/*! 
  \fn      static __INLINE void mtlk_buflist_push_front(mtlk_buflist_t* pbuflist, mtlk_buflist_entry_t* pentry)
  \brief   Function adds entry to the beginning of a list

  \param   pbuflist List object
           pentry List entry
 */
static __INLINE void mtlk_buflist_push_front(mtlk_buflist_t       *pbuflist,
                                             mtlk_buflist_entry_t *pentry);

/*! 
  \fn      static __INLINE mtlk_buflist_entry_t* mtlk_buflist_pop_front(mtlk_buflist_t* pbuflist)
  \brief   Function removes entry from the beginning of a list

  \param   pbuflist List object           
  
  \return  Removed entry or NULL if list is empty
 */
static __INLINE mtlk_buflist_entry_t *mtlk_buflist_pop_front(mtlk_buflist_t *pbuflist);

/*! 
  \fn      static __INLINE void mtlk_buflist_push_back(mtlk_buflist_t* pbuflist, mtlk_buflist_entry_t* pentry)
  \brief   Function adds entry to the end of a list

  \param   pbuflist List object
           pentry List entry
 */
static __INLINE void mtlk_buflist_push_back(mtlk_buflist_t       *pbuflist,
                                            mtlk_buflist_entry_t *pentry);

/*! 
  \fn      static __INLINE mtlk_buflist_entry_t* mtlk_buflist_pop_back(mtlk_buflist_t* pbuflist)
  \brief   Function removes entry from the end of a list

  \param   pbuflist List object           
  
  \return  Removed entry or NULL if list is empty
 */
static __INLINE mtlk_buflist_entry_t *mtlk_buflist_pop_back(mtlk_buflist_t *pbuflist);

/*! 
  \fn      mtlk_buflist_remove_entry(mtlk_buflist_t* pbuflist, mtlk_buflist_entry_t* pentry)
  \brief   Function removes entry from the list

  \param   pbuflist List object
           pentry List entry to be removed

  \return  Next entry after removed
 */
static __INLINE mtlk_buflist_entry_t *mtlk_buflist_remove_entry(mtlk_buflist_t       *pbuflist,
                                                                mtlk_buflist_entry_t *pentry);

/*! 
\fn      static __INLINE mtlk_buflist_entry_t* mtlk_buflist_head(mtlk_buflist_t* pbuflist)
\brief   Function returns the head element of a list (virtual entry that points to the first entry)

\param   pbuflist List object

\return  The first element of a list
*/
static __INLINE mtlk_buflist_entry_t *mtlk_buflist_head(mtlk_buflist_t *pbuflist);

/*! 
  \fn      static __INLINE int8 mtlk_buflist_is_empty(mtlk_buflist_t* pbuflist)
  \brief   Function checks if list is empty

  \param   pbuflist List object           
  
  \return  1 if list is empty, 0 - otherwise
 */
static __INLINE int8 mtlk_buflist_is_empty(mtlk_buflist_t *pbuflist);

/*! 
  \fn      static __INLINE uint32 mtlk_buflist_size(mtlk_buflist_t* pbuflist)
  \brief   Function returns count of elements which are in list

  \param   pbuflist List object

  \return  Count of elements which are in list
 */
static __INLINE uint32 mtlk_buflist_size(mtlk_buflist_t* pbuflist);

/*! 
  \fn      static __INLINE mtlk_buflist_entry_t* mtlk_buflist_next(mtlk_buflist_entry_t* pbuflist)
  \brief   Function returns the next element after given entry

  \param   pentry Given entry

  \return  The next entry after given
 */
static __INLINE mtlk_buflist_entry_t *mtlk_buflist_next(mtlk_buflist_entry_t *pentry);

/*! 
  \fn      static __INLINE mtlk_buflist_entry_t* mtlk_buflist_prev(mtlk_buflist_entry_t* pentry)
  \brief   Function returns the previous element before given entry

  \param   pentry Given entry

  \return  The previous entry after given
 */
static __INLINE mtlk_buflist_entry_t *mtlk_buflist_prev(mtlk_buflist_entry_t *pentry);

/*! 
\fn      static __INLINE mtlk_buflist_entry_t* mtlk_bufmgr_get_buflist_entry(mtlk_buffer_t* pbuffer)
\brief   Returns pointer to packet's buffer list entry. 

\param   pbuffer        Pointer to data buffer

\return  Pointer to buffer list entry
*/
static __INLINE mtlk_buflist_entry_t *mtlk_bufmgr_get_buflist_entry(mtlk_buffer_t *pbuffer);

/*! 
\fn      static __INLINE mtlk_buffer_t* mtlk_bufmgr_get_by_buflist_entry(mtlk_buflist_entry_t* pentry)
\brief   Returns pointer to packet by its buffer list entry. 



\return  Pointer to buffer list entry
*/

static __INLINE mtlk_buffer_t *mtlk_bufmgr_get_by_buflist_entry(mtlk_buflist_entry_t *pentry);

/*! 
\fn      \fn      static __INLINE mtlk_buffer_t* mtlk_bufmgr_get_by_buflist_entry(mtlk_buflist_entry_t* pentry)
\brief   Returns priority according to the buffer's TOS bits

\param   pbuffer        Pointer to data buffer
\param   ppriority      Pointer to variable that will store the priority

\return  MTLK_ERR... error code
*/
static __INLINE int 
mtlk_bufmgr_get_priority_from_tos (mtlk_buffer_t* pbuffer, uint16* ppriority)
{
  mtlk_tos_field_t *tos;
  uint8 ds_tos;
  uint8 ether_type_and_tos[4];

  ASSERT (pbuffer != NULL);  

  if(mtlk_bufmgr_read(pbuffer,
                      MTLK_ETH_HDR_SIZE - MTLK_ETH_SIZEOF_ETHERNET_TYPE,
                      sizeof(ether_type_and_tos),
                      ether_type_and_tos) != MTLK_ERR_OK)
  {
      return MTLK_ERR_UNKNOWN;
  }

  switch (NET_TO_HOST16(*(u16*)ether_type_and_tos))
  {
  case MTLK_ETH_DGRAM_TYPE_IP:
      ds_tos = get_ip4_tos(ether_type_and_tos + MTLK_ETH_SIZEOF_ETHERNET_TYPE);
      break;
  case MTLK_ETH_DGRAM_TYPE_IPV6:
      ds_tos = get_ip6_tos(ether_type_and_tos + MTLK_ETH_SIZEOF_ETHERNET_TYPE);
      break;
  default:
      //Unknown protocol
      *ppriority = MTLK_WMM_ACI_DEFAULT_CLASS;
      return MTLK_ERR_OK;
  }

  tos = (mtlk_tos_field_t *)&ds_tos;

  // tos->mgmt ignored
  *ppriority = tos->priority;

  return MTLK_ERR_OK;
}

/*\}*/

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#define SAFE_PLACE_TO_INCLUDE_BUFMGR_OSDEP_DEFS
#include "bufmgr_osdep_defs.h"

#endif /* !_MTLK_BUFMGR_H_ */
