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

#ifndef __CODEGEN_H__
#define __CODEGEN_H__

#define MAX_TOKENS 64

int cg_register_fmt_str(int gid, int loglevel, char *fmt);
int cg_make_fmt_str(char *fmt, char *tokens, int max_tokens);
int cg_create_inc_src_files(void);
int cg_create_scd_file(void);

#endif // !__CODEGEN_H__

