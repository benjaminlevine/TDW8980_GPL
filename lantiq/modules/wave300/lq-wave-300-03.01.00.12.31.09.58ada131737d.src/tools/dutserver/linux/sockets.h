/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef LINUX_SERVER_SOCKETS_H
#define LINUX_SERVER_SOCKETS_H

#include <stdint.h>

#define MT_UBYTE   uint8_t
#define MT_BYTE    int8_t
#define MT_UINT16  uint16_t
#define MT_INT16   int16_t
#define MT_UINT32  uint32_t
#define MT_INT32   int32_t

// Return values from public functions:
typedef int MT_RET;
#define MT_RET_OK 1
#define MT_RET_FAIL 0

int StartSocketsServer();
void EndSocketsServer();

#endif // LINUX_SERVER_SOCKETS_H
