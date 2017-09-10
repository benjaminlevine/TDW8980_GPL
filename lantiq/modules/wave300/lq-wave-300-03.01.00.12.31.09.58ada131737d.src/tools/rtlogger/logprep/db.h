/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * Copyright (c) 2007-2008 Metalink Broadband (Israel)
 *
 * ID database
 *
 * Written by: Andrey Fidrya
 *
 */

#ifndef __DB_H__
#define __DB_H__

#include <linux/limits.h>

#define MAX_LOGLEVEL_LEN 8
#define MAX_ID_LEN 64
#define MAX_LOG_TEXT_LEN 8192

struct db_file
{
  int file_id;
  char filename[PATH_MAX + NAME_MAX];

  struct db_file *next;
};

struct db_gid
{
  int gid;
  char gid_name[MAX_ID_LEN];

  struct db_gid *next;
};

struct db_text
{
  int file_id;
  int gid;
  int sid;
  int loglevel;
  char *text;

  struct db_text *next;
};

extern char fl_inc_fullname[PATH_MAX + NAME_MAX];
extern char fl_src_fullname[PATH_MAX + NAME_MAX];
extern struct db_gid *pdb_gids;
extern struct db_text *pdb_text;

int db_init(char *filename);
int db_destroy(void);
int create_inc_file(void);
int create_scd_file(void);

int db_register_filename(char *filename, int *pid);
int db_register_gid(const char *name, int *pid);
int db_get_gid(char *name, int *pgid);
void db_destroy_text(int file_id);
int db_register_text(int file_id, int gid, int sid, int loglevel, char *text);
void db_unregister_text(int file_id, int gid, int sid, int loglevel);
int db_generate_sid(int gid, int *psid);

#endif // !__DB_H__

