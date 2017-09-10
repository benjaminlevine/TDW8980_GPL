/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __COMPAT_H__
#define __COMPAT_H__

#define __PACKED __attribute__((aligned(1), packed))

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

#include <stdint.h> 

typedef enum _BOOL
{
  FALSE,
  TRUE
} BOOL;


typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#endif // !__COMPAT_H__

