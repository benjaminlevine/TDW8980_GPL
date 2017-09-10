/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __FORMATS_H__
#define __FORMATS_H__

#define MAC_ADDR_LENGTH (6)
#define MAC_ADDR_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_ADDR_ARG(x) \
  ((char*)(x))[0],((char*)(x))[1],((char*)(x))[2],((char*)(x))[3],((char*)(x))[4],((char*)(x))[5]

#define IP4_ADDR_LENGTH (4)
#define IP4_ADDR_FMT "%u.%u.%u.%u"
#define IP4_ADDR_ARG(x) \
  ((char*)(x))[3],((char*)(x))[2],((char*)(x))[1],((char*)(x))[0]

#define IP6_ADDR_LENGTH (16)
#define IP6_ADDR_FMT "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x"
#define MAKE_WORD(h,l) ((((int16)(h)) << 8) | (l))
#define IP6_ADDR_ARG(x) \
  MAKE_WORD(((char*)(x))[ 0], ((char*)(x))[ 1]), \
  MAKE_WORD(((char*)(x))[ 2], ((char*)(x))[ 3]), \
  MAKE_WORD(((char*)(x))[ 4], ((char*)(x))[ 5]), \
  MAKE_WORD(((char*)(x))[ 6], ((char*)(x))[ 7]), \
  MAKE_WORD(((char*)(x))[ 8], ((char*)(x))[ 9]), \
  MAKE_WORD(((char*)(x))[10], ((char*)(x))[11]), \
  MAKE_WORD(((char*)(x))[12], ((char*)(x))[13]), \
  MAKE_WORD(((char*)(x))[14], ((char*)(x))[15])

#endif /* __FORMATS_H__ */
