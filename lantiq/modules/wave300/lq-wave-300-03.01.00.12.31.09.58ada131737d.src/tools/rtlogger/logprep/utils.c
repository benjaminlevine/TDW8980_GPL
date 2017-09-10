/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * Copyright (c) 2007-2008 Metalink Broadband (Israel)
 *
 * Utilities
 *
 * Written by: Andrey Fidrya
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <sys/time.h>
#include <linux/limits.h>
#include <libgen.h>

#include "utils.h"

int debug = 9;

unsigned long
timestamp(void)
{
  struct timeval ts;
  if (0 != gettimeofday(&ts, NULL))
    return 0;
  return ts.tv_usec + (ts.tv_sec * 1000000);
}

int
get_dir(char *filename, char buf[PATH_MAX], char **pdirname)
{
  int rslt = 0;
  int cnt;
  // Note: some implementations of dirname modify it's argument, so we copy
  // it to an array first.
  cnt = snprintf(buf, PATH_MAX, "%s", filename);
  if (cnt < 0 || cnt >= PATH_MAX) {
    ERROR("Unable to get a pathname for temporary file: pathname too long");
    rslt = -1;
    goto cleanup;
  }
  *pdirname = dirname(buf);
  ASSERT(*pdirname);
cleanup:
  return rslt;
}

int
create_filename(char *dir, char *file, char buf[PATH_MAX + NAME_MAX],
    char *fullname)
{
  int rslt = 0;
  int cnt;
  cnt = snprintf(buf, PATH_MAX + NAME_MAX, "%s%s%s",
      dir, (ends_with(dir, "/") ? "" : "/"), file);
  if (cnt < 0 || cnt >= PATH_MAX + NAME_MAX) {
    ERROR("Unable to create a filename for temporary file: filename too long");
    rslt = -1;
    goto cleanup;
  }
cleanup:
  return rslt;
}

int
replace_filename(char *filename, char *new_filename,
    char buf[PATH_MAX + NAME_MAX])
{
  char path[PATH_MAX];
  int cnt;
  char *dir = NULL;
  int rslt = 0;

  // Get pathname from the filename we are processing
  ASSERT(ARRAY_SIZE(path) == PATH_MAX);
  if (0 != get_dir(filename, path, &dir)) {
    rslt = -1;
    goto cleanup;
  }

  // Combine filename with path
  cnt = snprintf(buf, PATH_MAX + NAME_MAX, "%s%s%s",
      dir, (ends_with(dir, "/") ? "" : "/"), new_filename);
  if (cnt < 0 || cnt >= PATH_MAX + NAME_MAX) {
    ERROR("Unable to create a filename for temporary file: filename too long");
    rslt = -1;
    goto cleanup;
  }

cleanup:
  return rslt;
}

int
ends_with(char *str, char *ends_with)
{
  if (!*str)
    return 0;
  if (strchr(ends_with, str[strlen(str) - 1]))
    return 1;
  return 0;
}

int
alloc_buf_from_file(char **ppbuf, int *buf_size, char *filename)
{
  int rslt = 0;
  FILE *in = NULL;
  int in_size = 0;
  char *in_buf = NULL;

  in = fopen(filename, "rb");
  if (!in) {
    ERROR("%s: unable to open for reading: %s",
        filename, strerror(errno));
    rslt = -1;
    goto cleanup;
  }

  if (0 != fseek(in, 0, SEEK_END)) {
    ERROR("%s: seek error: %s",
        filename, strerror(errno));
    rslt = -1;
    goto cleanup;
  }

  in_size = ftell(in);
  if (in_size == -1) {
    ERROR("%s: unable to determine file position: %s", 
        filename, strerror(errno));
    rslt = -1;
    goto cleanup;
  }

  rewind(in);

  in_buf = (char *) malloc(in_size + 1);
  if (!in_buf) {
    ERROR("Out of memory");
    rslt = -1;
    goto cleanup;
  }
  
  if (1 != fread(in_buf, in_size, 1, in)) {
    ERROR("%s: while reading from file: %s", 
        filename, strerror(errno));
    rslt = -1;
    goto cleanup;
  }
  in_buf[in_size] = '\0';
  
cleanup:
  if (in) {
    if (0 != fclose(in))
      ERROR("%s: cannot close: %s", filename, strerror(errno));
  }
  if (rslt != 0 && in_buf) {
    free(in_buf);
    in_buf = NULL;
  }
  *ppbuf = in_buf;
  *buf_size = in_size;

  return rslt;
}

int
get_line(char *filename, char *buf, size_t buf_size, FILE *fl,
  int trim_crlf, int *peof)
{
  int rslt = 0;
  int num_read;

  *peof = 0;

  if (!fgets(buf, buf_size, fl)) {
    if (feof(fl)) {
      *peof = 1;
      goto cleanup;
    }
    ERROR("%s: cannot read: %s", filename, strerror(errno));
    rslt = -1;
    goto cleanup;
  }

  num_read = strlen(buf);
  if (num_read >= 1 && buf[num_read - 1] != '\n') {
    ERROR("%s: line too long: %s", filename, buf);
    rslt = -1;
    goto cleanup;
  }

  if (trim_crlf) {
    while (num_read >= 1 &&
        (buf[num_read - 1] == '\n' || buf[num_read - 1] == '\r')) {
      buf[--num_read] = '\0';
    }
  }

cleanup:
  return rslt;
}

int
get_word(char **pp, char *buf, size_t buf_size)
{
  int rslt = 0;
  int at = 0;
  char c;

  skip_spcrlf(pp);

  for (;;) {
    c = **pp;
    if (!c || is_spcrlf(c))
      break;
    if (at >= buf_size - 1) {
      rslt = -1;
      goto cleanup;
    }
    buf[at] = c;
    ++at;
    ++(*pp);
  }
  buf[at] = '\0';
    
cleanup:
  return rslt;
}

int
is_spcrlf(char c)
{
  switch (c) {
  case ' ':
  case '\t':
  case '\r':
  case '\n':
    return 1;
  default:
    break;
  }
  return 0;
}

void skip_spcrlf(char **pp)
{
  for (;;) {
    switch (**pp) {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
      ++(*pp);
      continue;
    case '\0':
    default:
      return;
    }
  }
}

char *
malloc_str_encode(char *str)
{
  char *pbuf;
  int len = strlen(str);
  char *p;
  char *to;
  char c;
  
  // New string may be 2 times longer than original string
  // abc: \a\b\c
  // Also reserve space for two "'s and a NULL terminator
  pbuf = (char *) malloc((2 * len * sizeof(char)) + 2 + 1);
  if (!pbuf)
    return NULL;

  p = str;
  to = pbuf;

  *to++ = '\"';

  while ((c = *p) != '\0') {
    ++p;
    switch (c) {
    case '\r':
      *to++ = '\\';
      *to++ = 'r';
      continue;
    case '\n':
      *to++ = '\\';
      *to++ = 'n';
      continue;
    case '\\':
      *to++ = '\\';
      *to++ = '\\';
      continue;
    case '\'':
      *to++ = '\\';
      *to++ = '\'';
      continue;
    case '\"':
      *to++ = '\\';
      *to++ = '\"';
      continue;
    default:
      *to++ = c;
      break;
    }
  }

  *to++ = '\"';

  *to = '\0';

  return pbuf;
}

int
str_decode(char *str)
{
  char *p = str;
  char *to = str;
  char c;
  int rslt = 0;

  skip_spcrlf(&p);

  if (*p != '\"') {
    rslt = -1;
    goto cleanup;
  }
  ++p;

  while ((c = *p) != '\0') {
    if (c == '\"')
      break;
    ++p;

    if (c == '\\') {
      switch (*p) {
      case 'r':
        *to++ = '\r';
        break;
      case 'n':
        *to++ = '\n';
        break;
      case '\\':
        *to++ = '\\';
        break;
      case '\'':
        *to++ = '\'';
        break;
      case '\"':
        *to++ = '\"';
        break;
      default:
        rslt = -1;
        goto cleanup;
      }
      ++p;
    } else {
      *to++ = c;
    }
  }

  if (c == '\0') {
    // Unterminated string
    rslt = -1;
    goto cleanup;
  }

  *to = '\0';

cleanup:
  return rslt;
}

int
buf_append_char(char *buf, int buf_size, int *pbuf_at, char c)
{
  int rslt = 0;

  if (*pbuf_at >= buf_size) {
    rslt = -1;
    goto cleanup;
  }

  buf[*pbuf_at] = c;
  ++(*pbuf_at);

cleanup:
  return rslt;
}

int
buf_append_buf(char *buf, int buf_size, int *pbuf_at,
    char *src, int src_size)
{
  int rslt = 0;

  if (*pbuf_at + src_size > buf_size) {
    rslt = -1;
    goto cleanup;
  }

  memcpy(buf + *pbuf_at, src, src_size);
  (*pbuf_at) += src_size;

cleanup:
  return rslt;
}

void
str_to_upper(char *str)
{
  while (*str) {
    *str = toupper(*str);
    ++str;
  }
}


