/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * Copyright (c) 2007-2008 Metalink Broadband (Israel)
 *
 * Compatibility macros
 *
 * Written by: Andrey Fidrya
 *
 */

#ifndef __COMPAT_H__
#define __COMPAT_H__

#define __PACKED __attribute__((aligned(1), packed))

typedef enum _BOOL
{
  FALSE,
  TRUE
} BOOL;

typedef signed char            int8;
typedef signed short int       int16;
typedef signed long int        int32;
typedef signed long long int   int64;

typedef unsigned char          uint8;
typedef unsigned short int     uint16;
typedef unsigned long int      uint32;
typedef unsigned long long int uint64;

#endif // !__COMPAT_H__

