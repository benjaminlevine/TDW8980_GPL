/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * Copyright (c) 2007-2008 Metalink Broadband (Israel)
 *
 * Parser
 *
 * Written by: Andrey Fidrya
 *
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "logprep.h"
#include "db.h"
#include "codegen.h"
#include "utils.h"

char *filename = NULL;
int line = 1;
int col = 1;

#define MAX_LOG_MACRO_CALL_LEN 16384

//const char log_macro_name[] = "LOG";
const char log_macro_postfix[] = "0123456789 \t\r\n";
//const char error_macro_name[] = "ELOG";
//const char warning_macro_name[] = "WLOG";

const char gid_prefix[] = "GID_";
//const char sid_prefix[] = "SID_";

//const char gid_error[]   = "GID_ERROR";
//const char gid_warning[] = "GID_WARNING";
const char gid_unknown[] = "GID_UNKNOWN";
//const char sid_unknown[] = "SID_UNKNOWN";

static void advance(char **pp, int cnt);
static int process_log_macro(char **pp, FILE *out, const char *forced_gid);
static int fetch_param(char *to, size_t to_size, char **pp, FILE *out);
int is_spcrlf(char c);
static int fskip_spcrlf(char **pp, FILE *out);

int
register_known_gids (void)
{
  int rslt = 0;
 
  /* GID_ERROR is always 0 */
  if (0 != db_register_gid(GID_ERROR_NAME, &rslt) ||
      rslt != GID_ERROR) {
    rslt = -1;
    goto end;
  }

  /* GID_WARNING is always 1 */
  if (0 != db_register_gid(GID_WARNING_NAME, &rslt) ||
      rslt != GID_WARNING) {
    rslt = -1;
    goto end;
  }

  rslt = 0;
  
end:
  return rslt;
}

int
process_in_buf(char *in_buf, int in_size, FILE *out)
{
  int rslt = 0;
  char *p = in_buf;
  char *endp = p + in_size;
  int is_str = 0;
  char str_terminator;
  int escaped = 0;
  int is_comment = 0;
  int is_multiline = 0;

  int         macro_name_size = 0;
  const char *macro_name      = NULL;
  const char *forced_gid      = NULL;

  line = 1;
  col = 1;

  while (p != endp) {
    if (is_comment) {
      if (is_multiline) {
        if (p >= in_buf + 2 && *(p-2) == '*' && *(p-1) == '/')
          is_comment = 0;
      } else {
        if (p >= in_buf + 1 && *(p-1) == '\n')
          is_comment = 0;
      }
    }

    if (!is_comment && !is_str) {
      if (p >= in_buf &&
          (*p == '\"' || *p == '\'')) {
        is_str = 1;
        str_terminator = *p;
      } else {
        // comment?
        if (*p == '/') {
          if (*(p+1) == '/') {
            is_comment = 1;
            is_multiline = 0;
          } else if (*(p+1) == '*') {
            is_comment = 1;
            is_multiline = 1;
          }
        }
      }
    } else if (is_str) {
      if (!escaped) {
        if (*p == str_terminator)
          is_str = 0;
        else if (*p == '\\')
          escaped = 1;
      } else {
        escaped = 0;
      }
    }

#if 0
    printf("%s%c", is_comment ? "~" : (is_str ? "@" : ""), *p);
#endif

    if (is_comment || is_str) {
      goto no_process;
    }

    if (p != in_buf && isalpha(*(p-1))) {
      goto no_process;
    }

    if (0 == strncmp(p, ERROR_MACRO_PREFFIX, ARRAY_SIZE(ERROR_MACRO_PREFFIX) - 1)) {
      macro_name_size = ARRAY_SIZE(ERROR_MACRO_PREFFIX);
      macro_name      = ERROR_MACRO_PREFFIX;
      forced_gid      = GID_ERROR_NAME;
    }
    else if (0 == strncmp(p, WARNING_MACRO_PREFFIX, ARRAY_SIZE(WARNING_MACRO_PREFFIX) - 1)) {
      macro_name_size = ARRAY_SIZE(WARNING_MACRO_PREFFIX);
      macro_name      = WARNING_MACRO_PREFFIX;
      forced_gid      = GID_WARNING_NAME;
    }
    else if (0 == strncmp(p, LOG_MACRO_PREFFIX, ARRAY_SIZE(LOG_MACRO_PREFFIX) - 1) &&
             strchr(log_macro_postfix, p[ARRAY_SIZE(LOG_MACRO_PREFFIX) - 1])) {
      char *t;

      macro_name_size = ARRAY_SIZE(LOG_MACRO_PREFFIX);
      macro_name      = LOG_MACRO_PREFFIX;
      forced_gid      = NULL;

      // FIXME: avoids processing of #define DPR LOG macros by ignoring
      // LOG lines where brace is not opened on the same line
      for (t = p; t != endp; ++t) {
        if (*t == '\n') {
          macro_name = NULL;
          break;
        } else if (*t == '(') {
          break;
        }
      }

      // Only process log macros with bracket after them:
      for (t = p + (macro_name_size - 1) + 1 /* postfix */;
           t != endp; ++t) {
        if (*t == '\n') {
          macro_name = NULL;
          break;
        } else if (*t == '(') {
          break;
        }
      }      
    }
    else {
      goto no_process;
    }

    if (macro_name) {
      if (!gen_db_only && 1 != fwrite(p, macro_name_size - 1, 1, out)) {
        ERROR("Write error: %s", strerror(errno));
        rslt = -1;
        goto cleanup;
      }
      advance(&p, macro_name_size - 1);
  
      if (0 != process_log_macro(&p, out, forced_gid)) {
        rslt = -1;
        goto cleanup;
      }
      continue;
    }

no_process:
    if (!gen_db_only && EOF == fputc(*p, out)) {
      ERROR("Write error: %s", strerror(errno));
      rslt = -1;
      goto cleanup;
    }
    advance(&p, 1);
  }

  /*
  if (1 != fwrite(in_buf, in_size, 1, out)) {
    ERROR("Unable to write to a temporary file: %s",
        strerror(errno));
    rslt = -1;
    goto cleanup;
  }
  */

cleanup:
  return rslt;
}

static void
advance(char **pp, int cnt)
{
  char *p = *pp;
  while (cnt-- > 0) {
    ASSERT(*p != '\0');
    switch (*p) {
      case '\n':
        ++line;
        col = 1;
        ++p;
        continue;
      default:
        break;
    }
    ++col;
    ++p;
  }
  *pp = p;
}

static int
process_log_macro(char **pp, FILE *out, const char *forced_gid)
{
  char gid[MAX_ID_LEN];
  char sid[MAX_ID_LEN];
  char text[MAX_LOG_TEXT_LEN];
  int new_sid;
  int rslt = 0;
  char *p = *pp;
  char *old_p;
  int param_num;
  int retval;
  int no_gid = 0;
  int no_sid = 0;
  int gid_id = 0;
  int textlen;
  char buf[MAX_LOG_MACRO_CALL_LEN];
  int buf_at = 0;
  char tokens[MAX_TOKENS];
  char loglevel_buf[MAX_LOGLEVEL_LEN] = {0};
  int loglevel_at = 0;
  int loglevel = 0; /* ERRORs and WARNINGs are always on level 0 */

  gid[0] = '\0';
  sid[0] = '\0';
  text[0] = '\0';

  if (!forced_gid) {
    // Save log level (n): LOG[n]_[X] (...);
    while (*p && isdigit(*p)) {
      if (0 != buf_append_char(loglevel_buf, ARRAY_SIZE(loglevel_buf), &loglevel_at,
                               *p)) {
        ERROR_LC("Too many digits in loglevel");
        rslt = -1;
        goto cleanup;
      }
      if (!gen_db_only && EOF == fputc(*p, out)) {
        ERROR("Write error: %s", strerror(errno));
        rslt = -1;
        goto cleanup;
      }
      advance(&p, 1);
    }
    loglevel = atoi(loglevel_buf);
    param_num = 1; /* for LOG - get GID */
  }
  else {
    strcpy(gid, forced_gid);
    param_num = 2; /* for ERROR/WARNING - skip GID */
  }

  // Skip data type postfix (X): it will be regenerated:
  // LOG[n]_[X] (...);
  while (*p && *p != '(') {
    advance(&p, 1);
  }

  if (*p != '(') {
    ERROR_LC("LOG macro: syntax error");
    rslt = -1;
    goto cleanup;
  }

  if (0 != buf_append_char(buf, ARRAY_SIZE(buf), &buf_at, *p)) {
    ERROR_LC("LOG macro length must not exceed %d byte(s)", ARRAY_SIZE(buf));
    rslt = -1;
    goto cleanup;
  }
  advance(&p, 1); // skip '('

  do {
    while (*p && is_spcrlf(*p)) {
      if (0 != buf_append_char(buf, ARRAY_SIZE(buf), &buf_at, *p)) {
        ERROR_LC("LOG macro length must not exceed %d byte(s)", ARRAY_SIZE(buf));
        rslt = -1;
        goto cleanup;
      }
      advance(&p, 1);
    }

    switch (param_num) {
    case 1: // GID
      old_p = p;
      retval = fetch_param(gid, ARRAY_SIZE(gid), &p, NULL);
      if (retval == 0 || retval == ERR_OVERFLOW) {
        // Make sure this is GID and not text message
        if (0 != strncmp(gid, gid_prefix, strlen(gid_prefix))) {
          INFO_LC("Log message has no GID: setting to %s", gid_unknown);
          strcpy(gid, gid_unknown);
          no_gid = 1;
        } else {
          if (retval == ERR_OVERFLOW) {
            ERROR_LC("GID too long");
            rslt = -1;
            goto cleanup;
          }
        }
        if (0 != db_register_gid(gid, &gid_id)) {
          rslt = -1;
          goto cleanup;
        }
        if (0 != buf_append_buf(buf, ARRAY_SIZE(buf), &buf_at,
            gid, strlen(gid))) {
          ERROR_LC("LOG macro length must not exceed %d byte(s)", ARRAY_SIZE(buf));
          rslt = -1;
          goto cleanup;
        }

        if (no_gid) {
          // GID was added at the front of parameters list, so append a comma
          if (0 != buf_append_buf(buf, ARRAY_SIZE(buf), &buf_at, ", ", 2)) {
            ERROR_LC("LOG macro length must not exceed %d byte(s)", ARRAY_SIZE(buf));
            rslt = -1;
            goto cleanup;
          }
          
          // We inserted GID, but still haven't processed the original
          // first parameter. Rollback the pointer
          p = old_p;
        }
      } else {
        ERROR_LC("Syntax error");
        rslt = -1;
        goto cleanup;
      }
      break;

    case 2: // SID
      if (no_gid) {
        // If no gid, then probably, no SID also
        INFO_LC("Log message has no GID, so no SID also: creating a new SID");
        no_sid = 1;
      } else {
        old_p = p;
        retval = fetch_param(sid, ARRAY_SIZE(sid), &p, NULL);
        if (retval == 0 || retval == ERR_OVERFLOW) {
          // Make sure this is SID and not text message
          if (sid[0] == '\0' || !isdigit(sid[0])) {
            INFO_LC("Log message has no SID: creating a new SID");
            no_sid = 1;
            p = old_p;
          } else {
            if (retval == ERR_OVERFLOW) {
              ERROR_LC("SID too long");
              rslt = -1;
              goto cleanup;
            }
          }
        } else {
          ERROR_LC("Syntax error");
          rslt = -1;
          goto cleanup;
        }
      }
      if (no_sid || sid_no_reuse) {
        int gid_num = 0;
        if (0 != db_get_gid(gid, &gid_num)) {
          INFO_LC("Cannot generate a new SID for GID %s: GID not found in "
            "database", gid);
          rslt = -1;
          goto cleanup;
        }
        if (0 != db_generate_sid(gid_num, &new_sid)) {
          INFO_LC("Cannot generate a new SID for GID %s", gid);
          rslt = -1;
          goto cleanup;
        }
        sprintf(sid, "%d", new_sid);
        LOG2("Generated a new SID: %s", sid);
      } else {
        LOG2("Reused SID: %s", sid);
      }
        
      if (0 != buf_append_buf(buf, ARRAY_SIZE(buf), &buf_at,
          sid, strlen(sid))) {
          ERROR_LC("LOG macro length must not exceed %d byte(s)", ARRAY_SIZE(buf));
          rslt = -1;
          goto cleanup;
      }
      if (no_sid) {
        if (0 != buf_append_buf(buf, ARRAY_SIZE(buf), &buf_at, ", ", 2)) {
          ERROR_LC("LOG macro length must not exceed %d byte(s)", ARRAY_SIZE(buf));
          rslt = -1;
          goto cleanup;
        }
      }
      break;

    case 3: // TEXT
      retval = fetch_param(text, ARRAY_SIZE(text), &p, NULL);
      if (retval == ERR_OVERFLOW) {
        ERROR_LC("Log message text is too long (%d bytes max)",
            MAX_LOG_TEXT_LEN);
        rslt = -1;
        goto cleanup;
      }
      if (retval != 0) {
        ERROR_LC("Syntax error");
        rslt = -1;
        goto cleanup;
      }
      LOG2("Text: %s", text);
      if (!no_sid || !gen_db_only)
      {
        int gid_num = 0;
        if (0 != db_get_gid(gid, &gid_num)) {
          INFO_LC("GID %s not found in database", gid);
          rslt = -1;
          goto cleanup;
        }
        /* Unregister old text if exists */
        db_unregister_text(filename_id, gid_num, atoi(sid), loglevel);
        /* Register new one */
        if (0 != db_register_text(filename_id, gid_num, atoi(sid), 
            loglevel, text)) {
          rslt = -1;
          goto cleanup;
        }
        if (0 != cg_register_fmt_str(gid_num, loglevel, text)) {
          rslt = -1;
          goto cleanup;
        }
      }
      textlen = strlen(text);
      if (textlen > 0) {
        if (0 != buf_append_buf(buf, ARRAY_SIZE(buf), &buf_at,
            text, textlen)) {
          ERROR_LC("LOG macro length must not exceed %d byte(s)", ARRAY_SIZE(buf));
          rslt = -1;
          goto cleanup;
        }
      }
      break;

    default:
      // Skip parameter:
      if (0 != fetch_param(buf + buf_at, ARRAY_SIZE(buf) - buf_at, &p, NULL)) {
        if (retval == ERR_OVERFLOW) {
          ERROR_LC("LOG macro length must not exceed %d byte(s)", ARRAY_SIZE(buf));
        } else {
          ERROR_LC("Syntax error");
        }
        rslt = -1;
        goto cleanup;
      }
      buf_at = strlen(buf);
      break;
    }

    while (*p && is_spcrlf(*p)) {
      if (0 != buf_append_char(buf, ARRAY_SIZE(buf), &buf_at, *p)) {
        ERROR_LC("LOG macro length must not exceed %d byte(s)", ARRAY_SIZE(buf));
        rslt = -1;
        goto cleanup;
      }
      advance(&p, 1);
    }

    if (*p == ',') {
      if (0 != buf_append_char(buf, ARRAY_SIZE(buf), &buf_at, *p)) {
        ERROR_LC("LOG macro length must not exceed %d byte(s)", ARRAY_SIZE(buf));
        rslt = -1;
        goto cleanup;
      }
      advance(&p, 1);
    }
    
    ++param_num;
  } while (*p && *p != ')');

  // Zero-terminate the buffer:
  if (0 != buf_append_char(buf, ARRAY_SIZE(buf), &buf_at, '\0')) {
    ERROR_LC("LOG macro length must not exceed %d byte(s)", ARRAY_SIZE(buf));
    rslt = -1;
    goto cleanup;
  }

  // Regenerate data type postfix (X):
  // LOG[n]_[X] (...);
  if (0 != cg_make_fmt_str(text, tokens, ARRAY_SIZE(tokens))) {
    rslt = -1;
    goto cleanup;
  }
  if (*tokens) {
    if (!gen_db_only && EOF == fputc('_', out)) {
      ERROR("Write error: %s", strerror(errno));
      rslt = -1;
      goto cleanup;
    }
    str_to_upper(tokens);
    if (!gen_db_only && 1 != fwrite(tokens, strlen(tokens), 1, out)) {
      ERROR("Write error: %s", strerror(errno));
      rslt = -1;
      goto cleanup;
    }
  }

  // Append accumulated parameter data
  if (!gen_db_only && 1 != fwrite(buf, strlen(buf), 1, out)) {
    ERROR("Write error: %s", strerror(errno));
    rslt = -1;
    goto cleanup;
  }

  if (*p != ')') {
    ERROR_LC("LOG macro: syntax error");
    rslt = -1;
    goto cleanup;
  }

  if (!gen_db_only && EOF == fputc(*p, out)) {
    ERROR("Write error: %s", strerror(errno));
    rslt = -1;
    goto cleanup;
  }
  advance(&p, 1); // skip ')'

cleanup:
  *pp = p;
  return rslt;
}

static int
fetch_param(char *to, size_t to_size, char **pp, FILE *out)
{
  char *p = *pp;
  int rslt = 0;
  int is_str = 0;
  char str_terminator;
  int escaped = 0;
  int brace_recurse_level = 0;

  if (!*p || *p == ')') {
    rslt = ERR_UNKNOWN;
    goto cleanup;
  }

  while (*p) {
    if (!is_str) {
      if (*p == ',' || *p == ')') {
        if (!brace_recurse_level)
          break;
        else if (*p == ')')
          --brace_recurse_level;
      } else if (*p == '(') {
        ++brace_recurse_level;
      }
    }

    if (!is_str) {
      if (*p == '\"' || *p == '\'') {
        is_str = 1;
        str_terminator = *p;
      }
    } else { // is_str
      if (!escaped) {
        if (*p == str_terminator)
          is_str = 0;
        else if (*p == '\\')
          escaped = 1;
      } else {
        escaped = 0;
      }
    }
    if (to) {
      // reserve 1 for '\0'
      if (to_size > 1) {
        *to++ = *p;
        --to_size;
      } else {
        rslt = ERR_OVERFLOW;
        goto cleanup;
      }
    } else {
      if (EOF == fputc(*p, out)) {
        ERROR("Write error: %s", strerror(errno));
        rslt = -1;
        goto cleanup;
      }
    }

    advance(&p, 1);
  }

  if (0 != fskip_spcrlf(&p, out)) {
    rslt = -1;
    goto cleanup;
  }

  if (to)
    *to = '\0';

cleanup:
  if (rslt == 0)
    *pp = p;
  return rslt;
}

static int
fskip_spcrlf(char **pp, FILE *out)
{
  int rslt = 0;
  
  for (;;) {
    switch (**pp) {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
      if (EOF == fputc(**pp, out)) {
        ERROR("Write error: %s", strerror(errno));
        rslt = ERR_UNKNOWN;
        goto cleanup;
      }
      advance(pp, 1);
      continue;
    case '\0':
    default:
      goto cleanup;
    }
  }
cleanup:
  return rslt;
}

