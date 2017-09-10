/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * Copyright (c) 2007-2008 Metalink Broadband (Israel)
 *
 * Code generation
 *
 * Written by: Andrey Fidrya
 *
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

#include "logprep.h"
#include "compat.h"
#include "db.h"
#include "utils.h"
#include "codegen.h"

static char mname[256];
static char mbody[512];
static char fname[512];

/* printf symbol for IP4 ADDR extension */
#define MTLK_IP4_FMT    'B'
/* printf symbol for MAC ADDR extension */
#define MTLK_MAC_FMT    'Y'
/* printf symbol for IP6 ADDR extension */
#define MTLK_IP6_FMT    'K'

#define MTLK_IP6_ALEN   (sizeof(uint32) * 4)

struct logmacro
{
  int gid;
  int loglevel;
  char *tokens;

  struct logmacro *next;
};

struct logmacro *logmacros = NULL;

static int simplify_token(char token, int long_specs_cnt, char *pnew_token);
static int create_macro(FILE *fl_inc, FILE *fl_src, struct logmacro *ent);

int
cg_make_fmt_str(char *fmt, char *tokens, int max_tokens)
{
  int rslt = 0;
  char *p;
  int long_specs_cnt;
  char new_token;
  int num_tokens = 0;

  //LOG2("fmt: %s", fmt);

  p = fmt;
  while (*p) {
    if (*p != '%') {
      ++p;
      continue;
    }

    // Token found
    ++p; // skip '%'

    if (*p == '-' || *p == '+')
      ++p;
    while (isdigit(*p))
      ++p;
    if (*p == '.')
      ++p;
    while (isdigit(*p))
      ++p;

    long_specs_cnt = 0;

    /* Proces length modifiers */
    while (NULL != strchr("lhLqjztZ", *p)) {
      if('l' == p[0]) ++long_specs_cnt;
      if('z' == p[0]) long_specs_cnt += 2;
      ++p;
    }

    switch (*p) {
      case '%':
        break;
      case 's':
      case 'd':
      case 'x':
      case 'X':
      case 'i':
      case 'u':
      case 'p':
      case 'c':
      case MTLK_IP4_FMT:
      case MTLK_IP6_FMT:
      case MTLK_MAC_FMT:
        if (0 != simplify_token(*p, long_specs_cnt, &new_token)) {
          rslt = -1;
          goto cleanup;
        }
        if (num_tokens >= max_tokens) {
          ERROR("Too many tokens in format string");
          rslt = -1;
          goto cleanup;
        }
        tokens[num_tokens++] = new_token;
        break;
      case '\0':
        ERROR("End of string encountered while looking for token");
        rslt = -1;
        goto cleanup;
      default:
        ERROR_LC("Unknown token: %%[#.#]%c", *p);
        rslt = -1;
        goto cleanup;
    }

    ++p;
  }

  if (!num_tokens) {
    if (max_tokens < 1) {
      ERROR("Too many tokens in format string");
      rslt = -1;
      goto cleanup;
    }
    tokens[num_tokens++] = 'v';
  }

  if (num_tokens >= max_tokens) {
    ERROR("Too many tokens in format string");
    rslt = -1;
    goto cleanup;
  }
  tokens[num_tokens] = '\0';

cleanup:
  return rslt;
}

int
cg_register_fmt_str(int gid, int loglevel, char *fmt)
{
  int rslt = 0;
  char tokens[MAX_TOKENS];
  struct logmacro *ent;
  struct logmacro *new_ent = NULL;
  int token_exists;

  if (0 != cg_make_fmt_str(fmt, tokens, ARRAY_SIZE(tokens))) {
    rslt = -1;
    goto cleanup;
  }
  str_to_upper(tokens);

  token_exists = 0;
  for (ent = logmacros; !token_exists && ent; ent = ent->next) {
    if (GID_IS_LOG(gid)) {
      token_exists = (GID_IS_LOG(ent->gid) && 
                      ent->loglevel == loglevel && 
                      !strncmp(ent->tokens, tokens, MAX_TOKENS));
    }
    else {
      token_exists = (ent->gid == gid && !strncmp(ent->tokens, tokens, MAX_TOKENS));
    }
  }

  if (token_exists) {
    LOG2("Token GID#%d string already exists: %s", gid, tokens);
  }

  if (!token_exists) {
    new_ent = (struct logmacro *) malloc(sizeof(struct logmacro));
    if (!new_ent) {
      ERROR("Out of memory");
      rslt = -1;
      goto cleanup;
    }
    memset(new_ent, 0, sizeof(*new_ent));

    new_ent->gid = gid;
    new_ent->loglevel = loglevel;
    new_ent->tokens = strdup(tokens);
    if (!new_ent->tokens) {
      ERROR("Out of memory");
      rslt = -1;
      goto cleanup;
    }

    new_ent->next = logmacros;
    logmacros = new_ent;
  }
  

cleanup:
  if (rslt != 0 && new_ent) {
    if (new_ent->tokens)
      free(new_ent->tokens);
    free(new_ent);
  }
  return rslt;
}

static int
simplify_token(char token, int long_specs_cnt, char *pnew_token)
{
  int rslt = 0;

  switch (token) {
  case 'd':
  case 'u':
  case 'x':
  case 'X':
  case 'i':
    *pnew_token = (long_specs_cnt < 2) ? 'd' : 'h';
    break;
  case 'p':
    *pnew_token = 'p';
    break;
  case MTLK_IP4_FMT: /*special case, printf format extension for IP4 addr */
    *pnew_token = 'd';
    break;
  case MTLK_IP6_FMT: /*special case, printf format extension for IP6 addr */
    *pnew_token = MTLK_IP6_FMT;
    break;
  case MTLK_MAC_FMT: /*special case, printf format extension for MAC addr */
    *pnew_token = MTLK_MAC_FMT;
    break;
  case 's':
    *pnew_token = 's';
    break;
  case 'c':
    *pnew_token = 'c';
    break;
  default:
    ERROR("Cannot simplify token: %%[#.#]%c", token);
    rslt = -1;
    goto cleanup;
  }

cleanup:
  return rslt;
}

int cg_create_inc_src_files(void)
{
  int rslt = 0;
  FILE *fl_inc = NULL;
  FILE *fl_src = NULL;
  int retval;
  struct db_gid *gid_ent;
  struct db_text *text_ent;
  struct logmacro *lm_ent;
  struct stat fstats;
  int inc_file_exists = 0;
  int src_file_exists = 0;

  LOG1("Registering log format strings for macros generation");
  for (text_ent = pdb_text; text_ent; text_ent = text_ent->next) {
    if (0 != cg_register_fmt_str(text_ent->gid, text_ent->loglevel, text_ent->text)) {
      rslt = -1;
      goto cleanup;
    }
  }

  LOG1("Generating inc file: %s", fl_inc_fullname);
  LOG1("Generating src file: %s", fl_src_fullname);

  if (preserve_dt) {
    retval = stat(fl_inc_fullname, &fstats);
    if (retval != 0) {
      if (errno != ENOENT) {
        ERROR("%s: unable to stat file: %s",
            fl_inc_fullname, strerror(errno));
        rslt = -1;
        goto cleanup;
      }
    } else {
      inc_file_exists = 1;
    }

    retval = stat(fl_src_fullname, &fstats);
    if (retval != 0) {
      if (errno != ENOENT) {
        ERROR("%s: unable to stat file: %s",
            fl_src_fullname, strerror(errno));
        rslt = -1;
        goto cleanup;
      }
    } else {
      src_file_exists = 1;
    }
  }

  fl_inc = fopen(fl_inc_fullname, "wb");
  if (!fl_inc) {
    ERROR("%s: unable to open for writing: %s",
        fl_inc_fullname, strerror(errno));
    rslt = -1;
    goto cleanup;
  }

  fl_src = fopen(fl_src_fullname, "wb");
  if (!fl_src) {
    ERROR("%s: unable to open for writing: %s",
        fl_src_fullname, strerror(errno));
    rslt = -1;
    goto cleanup;
  }

  retval = fprintf(fl_inc,
    "// This file is automatically generated by logprep tool\n"
    "// DO NOT MODIFY\n"
    "\n"
    "#ifndef __LOGMACROS_H__\n"
    "#define __LOGMACROS_H__\n"
    "\n"
    "#include \"log_osdep.h\"\n"
    "\n"
    "#define MTLK_IP6_ALEN %d\n"
    "\n", MTLK_IP6_ALEN);
  if (retval < 0) {
    ERROR("%s: cannot write to file: %s", fl_inc_fullname, strerror(errno));
    rslt = -1;
    goto cleanup;
  }

  for (gid_ent = pdb_gids; gid_ent; gid_ent = gid_ent->next) {
    retval = fprintf(fl_inc, "#define %s %d\n", gid_ent->gid_name, gid_ent->gid);
    if (retval < 0) {
      ERROR("%s: cannot write to file: %s", fl_inc_fullname, strerror(errno));
      rslt = -1;
      goto cleanup;
    }
  }

  retval = fprintf(fl_src, "#include \"%s.h\"\n",
                   logdefs_fname?logdefs_fname:DEF_LOGDEFS_FNAME);
  if (retval < 0) {
    ERROR("%s: cannot write to file: %s", fl_src_fullname, strerror(errno));
    rslt = -1;
    goto cleanup;
  }

  for (lm_ent = logmacros; lm_ent; lm_ent = lm_ent->next) {
    if (0 != create_macro(fl_inc, fl_src, lm_ent)){
      rslt = -1;
      goto cleanup;
    }
  }

  retval = fprintf(fl_inc, "\n"
                   "#ifndef RTLOG_USE_FCALLS\n"
                   "#include \"%s.c\"\n"
                   "#endif // RTLOG_USE_FCALLS\n",
                   logdefs_fname?logdefs_fname:DEF_LOGDEFS_FNAME);
  if (retval < 0) {
    ERROR("%s: cannot write to file: %s", fl_inc_fullname, strerror(errno));
    rslt = -1;
    goto cleanup;
  }

  retval = fprintf(fl_inc, "%s",
    "\n"
    "#endif // __LOGMACROS_H__\n"
  );
  if (retval < 0) {
    ERROR("%s: cannot write to file: %s", fl_inc_fullname, strerror(errno));
    rslt = -1;
    goto cleanup;
  }

  if (fl_inc) {
    retval = fclose(fl_inc);
    fl_inc = NULL;
    if (retval != 0) {
      ERROR("%s: cannot close: %s", fl_inc_fullname, strerror(errno));
      rslt = -1;
      goto cleanup;
    }
  }

  if (fl_src) {
    retval = fclose(fl_src);
    fl_src = NULL;
    if (retval != 0) {
      ERROR("%s: cannot close: %s", fl_src_fullname, strerror(errno));
      rslt = -1;
      goto cleanup;
    }
  }

  if (preserve_dt) {
    if (inc_file_exists) {
      struct utimbuf tm;
      memset(&tm, 0, sizeof(tm));
      tm.actime = fstats.st_atime;
      tm.modtime = fstats.st_mtime;
      if (0 != utime(fl_inc_fullname, &tm)) {
        ERROR("%s: unable to reset file modification time: %s",
            fl_inc_fullname, strerror(errno));
        rslt = -1;
        goto cleanup;
      }
    }

    if (src_file_exists) {
      struct utimbuf tm;
      memset(&tm, 0, sizeof(tm));
      tm.actime = fstats.st_atime;
      tm.modtime = fstats.st_mtime;
      if (0 != utime(fl_src_fullname, &tm)) {
        ERROR("%s: unable to reset file modification time: %s",
            fl_src_fullname, strerror(errno));
        rslt = -1;
        goto cleanup;
      }
    }
  }

cleanup:
  if (fl_inc) {
    if (0 != fclose(fl_inc))
      ERROR("%s: cannot close: %s", fl_inc_fullname, strerror(errno));
    fl_inc = NULL;

    remove(fl_inc_fullname); // Try to delete partial file
  }

  if (fl_src) {
    if (0 != fclose(fl_src))
      ERROR("%s: cannot close: %s", fl_src_fullname, strerror(errno));
    fl_src = NULL;

    remove(fl_src_fullname); // Try to delete partial file
  }

  return rslt;
}

static void
addstrf (char **buff, int *size, const char *fmt, ...)
{
  int     len;
  va_list args;

  va_start(args, fmt);
  len = vsnprintf(*buff, *size, fmt, args);
  va_end(args);

  *buff += len;
  *size -= len;
}

static int
create_macro_name (char *macro_name_buf, int macro_name_buf_size, 
                   char *macro_body_buf, int macro_body_buf_size, 
                   char *func_name_buf, int func_name_buf_size, 
                   uint8 *token_ctrs,
                   struct logmacro *ent)
{
  int res = 0, num_tokens = 0, i = 0;

  memset(token_ctrs, 0, 256);

  *macro_name_buf = 0;
  *macro_body_buf = 0;
  *func_name_buf  = 0;

  addstrf(&macro_name_buf, &macro_name_buf_size, "#define ");
  addstrf(&func_name_buf, &func_name_buf_size, "__MTLK_FLOG void\n");

  if (ent->gid == GID_ERROR) {
    addstrf(&macro_name_buf, &macro_name_buf_size, 
            ERROR_MACRO_PREFFIX "%s%s(sid, fmt",
            (*ent->tokens ? "_" : ""),
            ent->tokens);
    addstrf(&macro_body_buf, &macro_body_buf_size, 
            "__" ERROR_MACRO_PREFFIX "%s%s(__FUNCTION__, __LINE__, (sid), (fmt)",
            (*ent->tokens ? "_" : ""),
            ent->tokens);
    addstrf(&func_name_buf, &func_name_buf_size, 
            "__" ERROR_MACRO_PREFFIX "%s%s (const char *fname, int line_no, uint16 sid, const char *fmt",
            (*ent->tokens ? "_" : ""),
            ent->tokens);
  }
  else if (ent->gid == GID_WARNING) {
    addstrf(&macro_name_buf, &macro_name_buf_size, 
            WARNING_MACRO_PREFFIX "%s%s(sid, fmt",
            (*ent->tokens ? "_" : ""),
            ent->tokens);
    addstrf(&macro_body_buf, &macro_body_buf_size, 
            "__" WARNING_MACRO_PREFFIX "%s%s(__FUNCTION__, __LINE__, (sid), (fmt)",
            (*ent->tokens ? "_" : ""),
            ent->tokens);
    addstrf(&func_name_buf, &func_name_buf_size, 
            "__" WARNING_MACRO_PREFFIX "%s%s (const char *fname, int line_no, uint16 sid, const char *fmt",
            (*ent->tokens ? "_" : ""),
            ent->tokens);
  }
  else {
    addstrf(&macro_name_buf, &macro_name_buf_size, 
            LOG_MACRO_PREFFIX "%d%s%s(gid, sid, fmt",
            ent->loglevel,
            (*ent->tokens ? "_" : ""),
            ent->tokens);
    addstrf(&macro_body_buf, &macro_body_buf_size, 
            "__" LOG_MACRO_PREFFIX "%d%s%s(__FUNCTION__, __LINE__, (gid), (sid), (fmt)",
            ent->loglevel,
            (*ent->tokens ? "_" : ""),
            ent->tokens);
    addstrf(&func_name_buf, &func_name_buf_size, 
            "__" LOG_MACRO_PREFFIX "%d%s%s(const char *fname, int line_no, uint8 gid, uint16 sid, const char *fmt",
            ent->loglevel,
            (*ent->tokens ? "_" : ""),
            ent->tokens);
  }

  if (strchr(ent->tokens, 'V'))
    num_tokens = 0;
  else
    num_tokens = strlen(ent->tokens);

  for (i = 0; i < num_tokens; ++i) {
    char  token = (uint8) ent->tokens[i];
    uint8 ctr   = ++token_ctrs[(uint8) token];
    switch (ent->tokens[i])
    {
    case 'S':
      addstrf(&macro_name_buf, &macro_name_buf_size, 
              ", s%d",
              ctr);
      addstrf(&macro_body_buf, &macro_body_buf_size, 
              ", (const char *)(s%d)",
              ctr);
      addstrf(&func_name_buf, &func_name_buf_size, 
              ", const char *s%d",
              ctr);
      break;
    case 'D':
      addstrf(&macro_name_buf, &macro_name_buf_size, 
              ", d%d",
              ctr);
      addstrf(&macro_body_buf, &macro_body_buf_size, 
              ", (int32)(d%d)",
              ctr);
      addstrf(&func_name_buf, &func_name_buf_size, 
              ", int32 d%d",
              ctr);
      break;
    case 'C':
      addstrf(&macro_name_buf, &macro_name_buf_size, 
              ", c%d",
              ctr);
      addstrf(&macro_body_buf, &macro_body_buf_size, 
              ", (int8)(c%d)",
              ctr);
      addstrf(&func_name_buf, &func_name_buf_size, 
              ", int8 c%d",
              ctr);
      break;
    case 'P':
      addstrf(&macro_name_buf, &macro_name_buf_size, 
              ", p%d",
              ctr);
      addstrf(&macro_body_buf, &macro_body_buf_size, 
              ", (const void *)(p%d)",
              ctr);
      addstrf(&func_name_buf, &func_name_buf_size, 
              ", const void *p%d",
              ctr);
      break;
    case 'H':
      addstrf(&macro_name_buf, &macro_name_buf_size, 
              ", h%d",
              ctr);
      addstrf(&macro_body_buf, &macro_body_buf_size, 
              ", (int64)(h%d)",
              ctr);
      addstrf(&func_name_buf, &func_name_buf_size, 
              ", uint64 h%d",
              ctr);
      break;
    case MTLK_MAC_FMT:
      addstrf(&macro_name_buf, &macro_name_buf_size, 
              ", mac%d",
              ctr);
      addstrf(&macro_body_buf, &macro_body_buf_size, 
              ", (const void *)(mac%d)",
              ctr);
      addstrf(&func_name_buf, &func_name_buf_size, 
              ", const void *mac%d",
              ctr);
      break;
    case MTLK_IP6_FMT:
      addstrf(&macro_name_buf, &macro_name_buf_size, 
              ", ip6%d",
              ctr);
      addstrf(&macro_body_buf, &macro_body_buf_size, 
              ", (const void *)(ip6%d)",
              ctr);
      addstrf(&func_name_buf, &func_name_buf_size, 
              ", const void *ip6%d",
              ctr);
      break;
    default:
      ERROR("Unknown token type encountered ('%c')", token);
      res = -1;
      goto end;
    }
  }

  addstrf(&macro_name_buf, &macro_name_buf_size, ")");
  addstrf(&macro_body_buf, &macro_body_buf_size, ")");
  addstrf(&func_name_buf, &func_name_buf_size,  ")");

end:
  return res;
}

static int
create_macro(FILE *fl_inc, FILE *fl_src, struct logmacro *ent)
{
  uint8 token_ctrs[sizeof(ent->tokens[0]) * 256];
  size_t num_tokens;
  int i;
  int rslt = 0;
  uint8 ctr;
  char token;
  const char *gid_str = "gid";
  const char *clog_str = CILOG_MACRO_NAME;
  char loglevel[32];
  char *clog_arg;

  rslt = create_macro_name(mname, sizeof(mname),
                           mbody, sizeof(mbody),
                           fname, sizeof(fname),
                           token_ctrs, ent);

  if (ent->gid == GID_ERROR) {
    gid_str  = GID_ERROR_NAME;
    clog_str = CERROR_MACRO_NAME;
    strcpy(loglevel, "RTLOG_ERROR_DLEVEL");
  }
  else if (ent->gid == GID_WARNING) {
    gid_str  = GID_WARNING_NAME;
    clog_str = CWARNING_MACRO_NAME;
    strcpy(loglevel, "RTLOG_ERROR_DLEVEL");
  }
  else {
    snprintf(loglevel, sizeof(loglevel), "%d", ent->loglevel);
  }

  if (rslt == 0 && 
      0 > fprintf(fl_inc,
                  "\n#if (RTLOG_MAX_DLEVEL < %s)\n"
                  "%s\n"
                  "#else\n",
                  loglevel,
                  mname)) {
    rslt = -1;
  }

 if (rslt == 0 && 
     0 > fprintf(fl_inc,
                 "\n%s;\n\n",
                 fname)) {
   rslt = -1;
 }

 if (rslt == 0 && 
     0 > fprintf(fl_inc,
                 "%s \\\n"
                 "    %s\n\n",
                 mname, mbody)) {
   rslt = -1;
 }

  if (rslt == 0 && 
      0 > fprintf(fl_src,
                  "\n#if (RTLOG_MAX_DLEVEL >= %s)\n",
                  loglevel)) {
    rslt = -1;
  }

 if (rslt == 0 && 
     0 > fprintf(fl_src,
                 "%s\n",
                 fname)) {
   rslt = -1;
 }

  if (strchr(ent->tokens, 'V'))
    num_tokens = 0;
  else
    num_tokens = strlen(ent->tokens);

  if (rslt == 0 && 
      0 > fprintf(fl_src,
      "{\n"
      "#if (RTLOG_FLAGS & (RTLF_REMOTE_ENABLED | RTLF_CONSOLE_ENABLED))\n"
      "  int flags__ = mtlk_log_get_flags(%d, LOG_ORIGIN_ID, %s, sid);\n"
      "#endif\n"
      "#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)\n"
      "  if ((flags__ & LOG_TARGET_REMOTE) != 0) {\n"
      "    size_t datalen__;\n",
     ent->loglevel, 
     gid_str)) {
    rslt = -1;
  }

  // For string tokens calculate their lengths
  for (i = 1; i <= token_ctrs[(uint8) 'S']; ++i) {
    if (rslt == 0 && 0 > fprintf(fl_src,
        "    size_t s%dlen__ = strlen(s%d) + 1;\n",
        i, i)) {
      rslt = -1;
    }
  }

  if (rslt == 0 && 0 > fprintf(fl_src,
      "    uint8 *pdata__; \n"
      "    mtlk_log_buf_entry_t *pbuf__; \n")) {
    rslt = -1;
  }

  if (num_tokens == 0) {
    if (rslt == 0 && 0 > fprintf(fl_src,
        "    datalen__ = 0")) {
      rslt = -1;
    }
  } else {
    if (rslt == 0 && 0 > fprintf(fl_src,
        "    datalen__ = (sizeof(mtlk_log_event_data_t) * %d)"
        , num_tokens)) {
      rslt = -1;
    }

    // For string tokens reserve space in data packet
    for (i = 1; i <= token_ctrs[(uint8) 'S']; ++i) {
      if (rslt == 0 && 0 > fprintf(fl_src,
          " + \n"
          "      sizeof(mtlk_log_lstring_t) + s%dlen__",
          i)) {
        rslt = -1;
      }
    }

    // For int8 reserve space in data packet
    if (token_ctrs[(uint8) 'C'] > 0 && rslt == 0 && 0 > fprintf(fl_src,
        " + \\\n"
        "      sizeof(int8)*%d", token_ctrs[(uint8) 'C'])) {
            rslt = -1;
    }

    // For int32 reserve space in data packet
    if (token_ctrs[(uint8) 'D'] > 0 && rslt == 0 && 0 > fprintf(fl_src,
        " + \n"
        "      sizeof(int32)*%d", token_ctrs[(uint8) 'D'])) {
            rslt = -1;
    }

    // For void* reserve space in data packet
    if (token_ctrs[(uint8) 'P'] > 0 && rslt == 0 && 0 > fprintf(fl_src,
        " + \n"
        "      sizeof(void *)*%d", token_ctrs[(uint8) 'P'])) {
            rslt = -1;
    }

    // For int64 reserve space in data packet
    if (token_ctrs[(uint8) 'H'] > 0 && rslt == 0 && 0 > fprintf(fl_src,
        " + \n"
        "      sizeof(int64)*%d", token_ctrs[(uint8) 'H'])) {
            rslt = -1;
    }

    // For MAC addresses reserve space in data packet
    if (token_ctrs[(uint8) MTLK_MAC_FMT] > 0 && rslt == 0 && 0 > fprintf(fl_src,
        " + \n"
        "      ETH_ALEN*%d", token_ctrs[(uint8) MTLK_MAC_FMT])) {
            rslt = -1;
    }

    // For IP v6 addresses reserve space in data packet
    if (token_ctrs[(uint8) MTLK_IP6_FMT] > 0 && rslt == 0 && 0 > fprintf(fl_src,
        " + \n"
        "      MTLK_IP6_ALEN*%d", token_ctrs[(uint8) MTLK_IP6_FMT])) {
            rslt = -1;
    }
  }

  if (rslt == 0 && 0 > fprintf(fl_src, 
      ";\n"
      "    pbuf__ = mtlk_log_new_pkt_reserve(sizeof(mtlk_log_event_t) + "
        "datalen__,  \n"
      "        &pdata__); \n"
      "    if (pbuf__ != NULL) { \n"
      "      uint8 *p__ = pdata__; \n"
      "      MTLK_ASSERT(pdata__ != NULL); \n"
      "      ((mtlk_log_event_t *) p__)->timestamp = "
        "jiffies_to_msecs(jiffies); \n"
      "      ASSERT_GID_VALID(%s); \n"
      "      ASSERT_SID_VALID(sid); \n"
      "      ((mtlk_log_event_t *) p__)->info = "
        "LOG_MAKE_INFO(LOG_ORIGIN_ID, %s, sid,  datalen__); \n"
      "      p__ += sizeof(mtlk_log_event_t); \n", 
        gid_str, gid_str)) {
    rslt = -1;
  }
 
  // Add tokens to log packet
  memset(token_ctrs, 0, sizeof(token_ctrs));
  for (i = 0; i < num_tokens; ++i) {
    token = (uint8) ent->tokens[i];
    ctr = ++token_ctrs[(uint8) token];
    switch (ent->tokens[i])
    {
    case 'S':
      if (rslt == 0 && 0 > fprintf(fl_src,
          "      ((mtlk_log_event_data_t *) p__)->datatype = "
            "LOG_DT_LSTRING; \n"
          "      p__ += sizeof(mtlk_log_event_data_t); \n"
          "      ((mtlk_log_lstring_t *) p__)->len = s%dlen__; \n"
          "      p__ += sizeof(mtlk_log_lstring_t); \n"
          "      memcpy(p__, s%d, s%dlen__); \n"
          "      p__ += s%dlen__; \n",
          ctr, ctr, ctr, ctr)) {
        rslt = -1;
      }
      break;
    case 'D':
      if (rslt == 0 && 0 > fprintf(fl_src,
          "      ((mtlk_log_event_data_t *) p__)->datatype = "
            "LOG_DT_INT32; \n"
          "      p__ += sizeof(mtlk_log_event_data_t); \n"
          "      *((int32 *) p__) = (int32)(d%d); \n"
          "      p__ += sizeof(int32); \n",
          ctr)) {
        rslt = -1;
      }
      break;
    case 'C':
      if (rslt == 0 && 0 > fprintf(fl_src,
          "      ((mtlk_log_event_data_t *) p__)->datatype = "
            "LOG_DT_INT8; \n"
          "      p__ += sizeof(mtlk_log_event_data_t); \n"
          "      *((int8 *) p__) = (int8)(c%d); \n"
          "      p__ += sizeof(int8); \n",
          ctr)) {
        rslt = -1;
      }
      break;
    case 'H':
      if (rslt == 0 && 0 > fprintf(fl_src,
          "      ((mtlk_log_event_data_t *) p__)->datatype = "
            "LOG_DT_INT64; \n"
          "      p__ += sizeof(mtlk_log_event_data_t); \n"
          "      *((int64 *) p__) = (int64)(h%d); \n"
          "      p__ += sizeof(int64); \n",
          ctr)) {
        rslt = -1;
      }
      break;
    case 'P':
      if (rslt == 0 && 0 > fprintf(fl_src,
          "      if (sizeof(void *) == sizeof(uint32)) { \n"
          "        ((mtlk_log_event_data_t *) p__)->datatype = LOG_DT_INT32; \n"
          "      } \n"
          "      else if (sizeof(void *) == sizeof(uint64)) { \n"
          "        ((mtlk_log_event_data_t *) p__)->datatype = LOG_DT_INT64; \n"
          "      } \n"
          "      else { \n"
          "        MTLK_ASSERT(!\"Invalid pointer size\");\n"
          "      } \n"
          "      p__ += sizeof(mtlk_log_event_data_t); \n"
          "      *((void **) p__) = (void *)(p%d); \n"
          "      p__ += sizeof(void *); \n",
          ctr)) {
        rslt = -1;
      }
      break;
    case MTLK_MAC_FMT:
      if (rslt == 0 && 0 > fprintf(fl_src,
          "      ((mtlk_log_event_data_t *) p__)->datatype = "
            "LOG_DT_MACADDR; \n"
          "      p__ += sizeof(mtlk_log_event_data_t); \n"
          "      mtlk_osal_copy_eth_addresses(p__, (void*) mac%d); \n"
          "      p__ += ETH_ALEN; \n",
          ctr)) {
        rslt = -1;
      }
      break;
    case MTLK_IP6_FMT:
      if (rslt == 0 && 0 > fprintf(fl_src,
          "      ((mtlk_log_event_data_t *) p__)->datatype = "
            "LOG_DT_IP6ADDR; \n"
          "      p__ += sizeof(mtlk_log_event_data_t); \n"
          "      memcpy(p__, (void*) ip6%d, MTLK_IP6_ALEN); \n"
          "      p__ += MTLK_IP6_ALEN; \n",
          ctr)) {
        rslt = -1;
      }
      break;
    default:
      ERROR("Unknown token type encountered ('%c')", token);
      rslt = -1;
      goto end;
    }
  }

  if (rslt == 0 && 
      0 > fprintf(fl_src,
                  "      MTLK_ASSERT(p__ == pdata__ + datalen__ + "
                  "sizeof(mtlk_log_event_t)); \n"
                  "      mtlk_log_new_pkt_release(pbuf__); \n"
                  "    } \n"
                  "  } \n"
                  "#endif\n"
                  "#if (RTLOG_FLAGS & RTLF_CONSOLE_ENABLED)\n"
                  "  if ((flags__ & LOG_TARGET_CONSOLE) != 0) { \n")) {
    rslt = -1;
  }

  if (GID_IS_LOG(ent->gid)) {
    if (rslt == 0 && 
        0 > fprintf(fl_src,
                    "    %s(fname, line_no, %d",
                    clog_str,
                    ent->loglevel)) {
      rslt = -1;
    }
  }
  else {
    if (rslt == 0 && 
        0 > fprintf(fl_src,
                    "    %s(fname, line_no",
                    clog_str)) {
      rslt = -1;
    }
  }

  if (rslt == 0 &&
      0 > fprintf(fl_src, "%s", 
                  num_tokens?", fmt":", \"\%s\", fmt")) {
    rslt = -1;
  }

  memset(token_ctrs, 0, sizeof(token_ctrs));
  for (i = 0; i < num_tokens; ++i) {
    char t[2] = {0};
    token = (uint8) ent->tokens[i];
    ctr = ++token_ctrs[(uint8) token];
    switch (token) {
    case MTLK_MAC_FMT:
      clog_arg = "mac";
      break; 
    case MTLK_IP6_FMT:
      clog_arg = "ip6";
      break;
    default:
      t[0] = tolower((uint8) token);
      clog_arg = t;
      break;
    }
    if (rslt == 0 && 0 > fprintf(fl_src, ", %s%d", clog_arg, (int)ctr)) {
      rslt = -1;
    }
  }
  if (rslt == 0 && 
      0 > fprintf(fl_src,
                  "); \n"
                  "  } \n"
                  "#endif\n"
                  "} \n")) {
    rslt = -1;
  }

  if (rslt == 0 && 
      0 > fprintf(fl_inc,
                  "#endif /* RTLOG_MAX_DLEVEL */\n")) {
    rslt = -1;
  }

  if (rslt == 0 && 
      0 > fprintf(fl_src,
                  "#endif /* RTLOG_MAX_DLEVEL */\n")) {
    rslt = -1;
  }

  if (rslt != 0) {
    ERROR("%s: cannot write to file(s): %s or %s", fl_inc_fullname, fl_src_fullname, strerror(errno));
    goto end;
  }

end:
  return rslt;
}

static const char*
get_origin_name (int origin_id)
{
  switch (origin_id) {
  case 0:
    return "driver";
  default:
    return "unknown";
  }
}

int
cg_create_scd_file(void)
{
  int rslt = 0;
  int retval;
  FILE *fl = NULL;
  struct db_text *pentry;
  struct db_gid *gid_ent;

  LOG1("Dumping SCD to: %s", filename);
  fl = fopen(filename, "wb");
  if (!fl) {
    ERROR("%s: unable to open for writing: %s",
        filename, strerror(errno));
    rslt = -1;
    goto cleanup;
  }

  retval = fprintf(fl, "O %d %s\n", origin_id, get_origin_name(origin_id));
  if (retval < 0) {
    ERROR("%s: cannot write to file: %s", fl_inc_fullname, strerror(errno));
    rslt = -1;
    goto cleanup;
  }

  for (gid_ent = pdb_gids; gid_ent; gid_ent = gid_ent->next) {
    retval = fprintf(fl, "G %d %d %s\n", origin_id, gid_ent->gid, gid_ent->gid_name);
    if (retval < 0) {
      ERROR("%s: cannot write to file: %s", fl_inc_fullname, strerror(errno));
      rslt = -1;
      goto cleanup;
    }
  }

  for (pentry = pdb_text; pentry; pentry = pentry->next) {
    retval = fprintf(fl, "S %d %d %d %s\n",
                     origin_id, pentry->gid, pentry->sid, pentry->text);
    if (retval < 0) {
      ERROR("%s: cannot write to file: %s", filename, strerror(errno));
      rslt = -1;
      goto cleanup;
    }
  }

  if (fl) {
    retval = fclose(fl);
    fl = NULL;
    if (retval != 0) {
      ERROR("%s: cannot close: %s", filename, strerror(errno));
      rslt = -1;
      goto cleanup;
    }
  }

cleanup:
  if (fl) {
    if (0 != fclose(fl))
      ERROR("%s: cannot close: %s", filename, strerror(errno));
    fl = NULL;
  }
  return rslt;
}

