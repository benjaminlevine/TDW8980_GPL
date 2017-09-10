/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id$
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Utilities.
 *
 * Originally written by Andrey Fidrya
 *
 */
#include "mtlkinc.h"

#include <linux/ctype.h>

#include "compat.h"
#include "utils.h"

#include <asm/uaccess.h>
#include <linux/vmalloc.h>

/*****************************************************************************
**
** NAME         copy_str_to_userspace
**
** PARAMETERS   str                 String to be copied to userspace
**                                  (not neccessarily null-terminated)
**              str_len             Length of the string
**                                  (without null-terminator)
**              userspace_buffer    Destination buffer
**              at                  Position in destination buffer
**              space_left          Space left in destination buffer
**
** RETURNS      0                   Success
**              -EFAULT             Failure
**
** DESCRIPTION  Copies a non null-terminated string to userspace
**
******************************************************************************/
int
copy_str_to_userspace(const char* str, size_t str_len,
  void __user *userspace_buffer, int *at, size_t *space_left)
{
#if 1
  // FIXME: this version silently ignores overflows,
  // replace it with the version below
  int copy_limit = MIN(str_len, *space_left);
  if (0 != copy_to_user(userspace_buffer + *at, str, copy_limit))
    return -EFAULT;
  *at += copy_limit;
  *space_left -= copy_limit;
#else
  // TODO: use this version after implementing text buffering
  // in all procfs handlers
  if (str_len > *space_left)
    return -EFAULT;
  if (0 != copy_to_user(userspace_buffer + *at, str, str_len))
    return -EFAULT;
  *at += str_len;
  *space_left -= str_len;
#endif
  return 0;
}

/*****************************************************************************
**
** NAME         copy_zstr_to_userspace
**
** PARAMETERS   str                 Zero-terminated string to be copied to
**                                  userspace
**              userspace_buffer    Destination buffer
**              at                  Position in destination buffer
**              space_left          Space left in destination buffer
**
** RETURNS      0                   Success
**              -EFAULT             Failure
**
** DESCRIPTION  Copies a null-terminated string to userspace
**
******************************************************************************/
int
copy_zstr_to_userspace(const char* str, 
  void __user *userspace_buffer, int *at, size_t *space_left)
{
  return copy_str_to_userspace(str, strlen(str), userspace_buffer, at, space_left);
}

/*****************************************************************************
**
** NAME         copy_zstr_to_userspace_fmt
**
** PARAMETERS   buf                 Buffer for temporarily storing the
**                                  formatted string
**              max_buf_len         Size of temporary buffer
**              userspace_buffer    Destination buffer
**              at                  Position in destination buffer
**              space_left          Space left in destination buffer
**              fmt                 Format string
**              ...                 Arguments
**
** RETURNS      0                   Success
**              -EFAULT             Failure
**
** DESCRIPTION  Copies a null-terminated formatted string to userspace
**
******************************************************************************/
int
copy_zstr_to_userspace_fmt(char *buf, size_t max_buf_len,
  void __user *userspace_buffer, int *at, size_t *space_left,
  const char *fmt, ...)
{
  // FIXME: crashes when trying to print more data than the size of a buffer
  // supplied
  int len;
  va_list args;

  va_start(args, fmt);
  len = vsnprintf(buf, max_buf_len, fmt, args);
  va_end(args);

  // NOTE: if len == max_buf_len, then buf contents are not null-terminated
  if (len < 0 || len > max_buf_len)
    return -EFAULT;

  if (0 != copy_str_to_userspace(buf, len, userspace_buffer, at, space_left))
    return -EFAULT;

  return 0;
}


/*****************************************************************************
**
** NAME         str_count
**
** PARAMETERS   str                 String
**              c                   Character
**
** RETURNS      Number of occurences
**
** DESCRIPTION  Returns number of occurences of a character in a string
**
******************************************************************************/
size_t
str_count (const char *str, char c)
{
  size_t cnt = 0;
  while (*str) {
    if (*str == c)
      ++cnt;
    ++str;
  }
  return cnt;
}

/*! \fn void *mtlk_utils_memchr(const void *s, int c, size_t n)
+    \brief Find a character in an area of memory.
+    \param s The memory area.
+    \param c The byte to search for.
+    \param n The size of the area.
+    \return the address of the first occurrence of \a c, or NULL if \a c is not found
+*/
void *
mtlk_utils_memchr(const void *s, int c, size_t n)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)
  // Prior to this version some architectures don't 
  // export memchr
  return memchr(s, c, n);
#else
  const char *cp;

  for (cp = s; n > 0; n--, cp++) {
    if (*cp == c)
      return (char *) cp; /* Casting away the const here */
  }

  return NULL;
#endif
}
