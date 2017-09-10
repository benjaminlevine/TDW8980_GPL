/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * Copyright (c) 2007-2008 Metalink Broadband (Israel)
 *
 * Logger Source Code Preprocessor
 *
 * Written by: Andrey Fidrya
 *
 */

#ifndef __LOGPREP_H__
#define __LOGPREP_H__

extern int filename_id;
extern char *working_dir;
extern int preserve_dt;
extern int sid_no_reuse;
extern char *logdefs_fname;
extern int origin_id;
extern int gen_db_only;

#define GID_ERROR   0
#define GID_WARNING 1

#define GID_ERROR_NAME   "GID_ERROR"
#define GID_WARNING_NAME "GID_WARNING"

#define GID_IS_LOG(gid) ((gid) != GID_ERROR && (gid) != GID_WARNING)

#define ERROR_MACRO_PREFFIX   "ELOG"
#define WARNING_MACRO_PREFFIX "WLOG"
#define LOG_MACRO_PREFFIX     "ILOG"

#define CERROR_MACRO_NAME     "CERROR"
#define CWARNING_MACRO_NAME   "CWARNING"
#define CILOG_MACRO_NAME       "CILOG"

#define DEF_LOGDEFS_FNAME "logmacros"

#endif // !__LOGPREP_H__

