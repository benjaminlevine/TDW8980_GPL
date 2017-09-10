/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "iniparser.h"
#include "mtlkerr.h"

#define INIPARSER_KEY_DELIM ":"

/* Key creation as required by iniparser. 
 * NOTE: gkey means global key (i.e. out of sections key)
 */
#define iniparser_aux_key_const(section, key) (section INIPARSER_KEY_DELIM key)
#define iniparser_aux_gkey_const(key)         iniparser_aux_key_const("", key)

static __INLINE int
iniparser_aux_key (const char *section, 
                   const char *key, 
                   char       *buf,
                   uint32      buf_size)
{
  return snprintf(buf, buf_size, "%s" INIPARSER_KEY_DELIM "%s", section, key);
}

static __INLINE int
iniparser_aux_gkey (const char *key, 
                    char       *buf,
                    uint32      buf_size)
{
  return snprintf(buf, buf_size, INIPARSER_KEY_DELIM "%s", key);
}

static __INLINE int
iniparser_aux_get_str (dictionary *dict,
                       const char *key,
                       char       *buf,
                       uint32      buf_size)
{
  int   res = MTLK_ERR_UNKNOWN;
  char *tmp = iniparser_getstr(dict, key);

  if (!tmp) {
    res = MTLK_ERR_NOT_IN_USE;
  }
  else if (strlen(tmp) >= buf_size) {
    strncpy(buf, tmp, buf_size - 1);
    buf[buf_size - 1] = 0;
    return MTLK_ERR_BUF_TOO_SMALL;
  }
  else {
    strcpy(buf, tmp);
    res = MTLK_ERR_OK;
  }

  return res;
}
