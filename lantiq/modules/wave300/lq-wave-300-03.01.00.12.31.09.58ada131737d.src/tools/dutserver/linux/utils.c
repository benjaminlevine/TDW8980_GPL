/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * Copyright (c) 2007 Metalink Broadband (Israel)
 *
 * Utilities.
 *
 */

#include <stdio.h>
#include <sys/time.h>

int debug = 0;

unsigned long timestamp()
{
  struct timeval ts;
  if (0 != gettimeofday(&ts, NULL))
    return 0;
  return ts.tv_usec + (ts.tv_sec * 1000000);
}

