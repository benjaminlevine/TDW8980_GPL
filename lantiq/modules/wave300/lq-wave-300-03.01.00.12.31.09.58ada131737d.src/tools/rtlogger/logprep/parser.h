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

#ifndef __PARSER_H__
#define __PARSER_H__

#include <stdio.h>

extern int line;
extern int col;
extern char *filename;

int register_known_gids(void);
int process_in_buf(char *in_buf, int in_size, FILE *out);

#endif // !__PARSER_H__

