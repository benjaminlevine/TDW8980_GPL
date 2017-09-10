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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "logprep.h"
#include "db.h"
#include "codegen.h"
#include "utils.h"

char fl_inc_fullname[PATH_MAX + NAME_MAX];
char fl_src_fullname[PATH_MAX + NAME_MAX];

char fl_files_fullname[PATH_MAX + NAME_MAX];
struct db_file *pdb_files = NULL;

char fl_gids_fullname[PATH_MAX + NAME_MAX];
struct db_gid *pdb_gids = NULL;

char fl_text_fullname[PATH_MAX + NAME_MAX];
struct db_text *pdb_text = NULL;

static int filenames_unload(void);
static int filenames_load(void);

static int gids_unload(void);
static int gids_load(void);

static int text_unload(void);
static int text_load(void);

int
db_init(char *filename)
{
  char path[PATH_MAX];
  char *dir = NULL;
  int rslt = 0;
  int cnt;
  int retval;

  if (strstr(filename, DEF_LOGDEFS_FNAME)) {
    LOG1("Skipping %s", filename);
    rslt = -1;
    goto cleanup;
  }
  // If working directory is specified, use it
  if (working_dir) {
    dir = working_dir;
  } else {
    // Get pathname from the filename we are processing
    ASSERT(ARRAY_SIZE(path) == PATH_MAX);
    if (0 != get_dir(filename, path, &dir)) {
      rslt = -1;
      goto cleanup;
    }
  }

  cnt = snprintf(fl_inc_fullname, ARRAY_SIZE(fl_inc_fullname), "%s%s%s.h",
      dir, (ends_with(dir, "/") ? "" : "/"), logdefs_fname?logdefs_fname:DEF_LOGDEFS_FNAME);
  if (cnt < 0 || cnt >= ARRAY_SIZE(fl_inc_fullname)) {
    ERROR("Unable to create a filename for inc file: filename too long");
    rslt = -1;
    goto cleanup;
  }

  cnt = snprintf(fl_src_fullname, ARRAY_SIZE(fl_inc_fullname), "%s%s%s.c",
      dir, (ends_with(dir, "/") ? "" : "/"), logdefs_fname?logdefs_fname:DEF_LOGDEFS_FNAME);
  if (cnt < 0 || cnt >= ARRAY_SIZE(fl_src_fullname)) {
    ERROR("Unable to create a filename for src file: filename too long");
    rslt = -1;
    goto cleanup;
  }

  cnt = snprintf(fl_files_fullname, ARRAY_SIZE(fl_files_fullname), "%s%s%s",
      dir, (ends_with(dir, "/") ? "" : "/"), ".files");
  if (cnt < 0 || cnt >= ARRAY_SIZE(fl_files_fullname)) {
    ERROR("Unable to create a filename for .files file: filename too long");
    rslt = -1;
    goto cleanup;
  }
  retval = filenames_load();
  if (retval != 0 && retval != ERR_NOENT) {
    rslt = -1;
    goto cleanup;
  }

  cnt = snprintf(fl_gids_fullname, ARRAY_SIZE(fl_gids_fullname), "%s%s%s",
      dir, (ends_with(dir, "/") ? "" : "/"), ".gids");
  if (cnt < 0 || cnt >= ARRAY_SIZE(fl_gids_fullname)) {
    ERROR("Unable to create a filename for .gids file: filename too long");
    rslt = -1;
    goto cleanup;
  }
  retval = gids_load();
  if (retval != 0 && retval != ERR_NOENT) {
    rslt = -1;
    goto cleanup;
  }

  cnt = snprintf(fl_text_fullname, ARRAY_SIZE(fl_text_fullname), "%s%s%s",
      dir, (ends_with(dir, "/") ? "" : "/"), ".text");
  if (cnt < 0 || cnt >= ARRAY_SIZE(fl_text_fullname)) {
    ERROR("Unable to create a filename for .text file: filename too long");
    rslt = -1;
    goto cleanup;
  }
  retval = text_load();
  if (retval != 0 && retval != ERR_NOENT) {
    rslt = -1;
    goto cleanup;
  }

cleanup:
  return rslt;
}

int
db_destroy(void)
{
  int rslt = 0;

  if (0 != filenames_unload()) {
    rslt = -1;
    goto cleanup;
  }

  if (0 != gids_unload()) {
    rslt = -1;
    goto cleanup;
  }

  if (0 != text_unload()) {
    rslt = -1;
    goto cleanup;
  }

cleanup:
  return rslt;
}

/****************************************************************************/
/* SOURCE FILES                                                             */
/****************************************************************************/

static int
filenames_push_back(char *filename, int id)
{
  int rslt = 0;
  struct db_file *ent;

  ent = (struct db_file *) malloc(sizeof(struct db_file));
  if (!ent) {
    ERROR("Out of memory");
    rslt = -1;
    goto cleanup;
  }

  ent->file_id = id;

  if (strlen(filename) >= ARRAY_SIZE(ent->filename)) {
    ERROR("Filenames DB error: filename too long: %s", filename);
    rslt = -1;
    goto cleanup;
  }
  strcpy(ent->filename, filename);

  ent->next = pdb_files;
  pdb_files = ent;
  LOG2("File added to db (id %d): %s", id, filename);

cleanup:
  if (rslt != 0 && ent)
    free(ent);
  return rslt;
}

int
db_register_filename(char *filename, int *pid)
{
  int rslt = 0;
  int maxid = -1;
  struct db_file *ent;

  for (ent = pdb_files; ent; ent = ent->next) {
    if (!strcmp(ent->filename, filename)) {
      LOG1("file found in db: %s", filename);
      *pid = ent->file_id;
      rslt = 0;
      goto cleanup;
    }
    if (ent->file_id > maxid)
      maxid = ent->file_id;
  }
  
  ++maxid;
  if (0 != filenames_push_back(filename, maxid)) {
    rslt = -1;
    goto cleanup;
  }
  *pid = maxid;

cleanup:
  return rslt;
}

static int
filenames_unload(void)
{
  int rslt = 0;
  int retval;
  FILE *fl = NULL;
  struct db_file *pnext;
  char *enc = NULL;

  LOG1("Dumping files database to: %s", fl_files_fullname);
  fl = fopen(fl_files_fullname, "wb");
  if (!fl) {
    ERROR("%s: unable to open for writing: %s",
        fl_files_fullname, strerror(errno));
    rslt = -1;
    goto cleanup;
  }

  while (pdb_files) {
    pnext = pdb_files->next;

    enc = malloc_str_encode(pdb_files->filename);
    if (!enc) {
      ERROR("Out of memory");
      rslt = -1;
      goto cleanup;
    }
    retval = fprintf(fl, "%d %s\n",
        pdb_files->file_id, enc);
    if (retval < 0) {
      ERROR("%s: cannot write to file: %s", fl_files_fullname, strerror(errno));
      rslt = -1;
      goto cleanup;
    }

    free(enc);
    enc = NULL;
    
    free(pdb_files);
    pdb_files = pnext;
  }

  if (fl) {
    retval = fclose(fl);
    fl = NULL;
    if (retval != 0) {
      ERROR("%s: cannot close: %s", fl_files_fullname, strerror(errno));
      rslt = -1;
      goto cleanup;
    }
  }

cleanup:
  if (fl) {
    if (0 != fclose(fl))
      ERROR("%s: cannot close: %s", fl_files_fullname, strerror(errno));
    fl = NULL;
  }
  if (enc)
    free(enc);
  return rslt;
}

static int
filenames_load(void)
{
  FILE *fl = NULL;
  int rslt = 0;
  int retval;
  char buf[8192];
  char id_buf[64];
  int eof = 0;
  char *p;

  fl = fopen(fl_files_fullname, "rb");
  if (!fl) {
    LOG2("%s: unable to open for reading: %s",
        fl_files_fullname, strerror(errno));
    LOG2("Assuming an empty database");
    rslt = ERR_NOENT;
    goto cleanup;
  }

  for (;;) {
    retval = get_line(fl_files_fullname, buf, ARRAY_SIZE(buf), fl, 1, &eof);
    if (retval != 0) {
      rslt = -1;
      goto cleanup;
    }
    if (eof)
      break;
    LOG2("read line: %s", buf);

    p = buf;
    retval = get_word(&p, id_buf, ARRAY_SIZE(id_buf));
    if (retval != 0) {
      ERROR("%s: syntax error", fl_files_fullname);
      rslt = -1;
      goto cleanup;
    }
    
    skip_spcrlf(&p);
    if (0 != str_decode(p)) {
      ERROR("%s: syntax error", fl_files_fullname);
      rslt = -1;
      goto cleanup;
    }

    if (0 != filenames_push_back(p, atoi(id_buf))) {
      rslt = -1;
      goto cleanup;
    }
  }

  if (fl) {
    retval = fclose(fl);
    fl = NULL;
    if (retval != 0) {
      ERROR("%s: cannot close: %s", fl_files_fullname, strerror(errno));
      rslt = -1;
      goto cleanup;
    }
  }

cleanup:
  if (rslt != 0) {
    if (fl) {
      if (0 != fclose(fl))
        ERROR("%s: cannot close: %s", fl_files_fullname, strerror(errno));
      fl = NULL;
    }
  }
  return rslt;
}


/****************************************************************************/
/* GIDS                                                                     */
/****************************************************************************/

static int
gids_push_back(const char *name, int id)
{
  int rslt = 0;
  struct db_gid *ent;

  ent = (struct db_gid *) malloc(sizeof(struct db_gid));
  if (!ent) {
    ERROR("Out of memory");
    rslt = -1;
    goto cleanup;
  }

  ent->gid = id;

  if (strlen(name) >= ARRAY_SIZE(ent->gid_name)) {
    ERROR("GIDs DB error: GID name too long: %s", name);
    rslt = -1;
    goto cleanup;
  }
  strcpy(ent->gid_name, name);

  ent->next = pdb_gids;
  pdb_gids = ent;
  LOG2("GID added to db (id %d): %s", id, name);

cleanup:
  if (rslt != 0 && ent)
    free(ent);
  return rslt;
}

int
db_register_gid(const char *name, int *pid)
{
  int rslt = 0;
  int maxid = -1;
  struct db_gid *ent;

  for (ent = pdb_gids; ent; ent = ent->next) {
    if (!strcmp(ent->gid_name, name)) {
      LOG1("GID found in db: %s", name);
      *pid = ent->gid;
      rslt = 0;
      goto cleanup;
    }
    if (ent->gid > maxid)
      maxid = ent->gid;
  }
  
  ++maxid;
  if (0 != gids_push_back(name, maxid)) {
    rslt = -1;
    goto cleanup;
  }
  *pid = maxid;

cleanup:
  return rslt;
}

int db_get_gid(char *name, int *pgid)
{
  struct db_gid *ent;

  for (ent = pdb_gids; ent; ent = ent->next) {
    if (!strcmp(ent->gid_name, name)) {
      *pgid = ent->gid;
      return 0;
    }
  }
  return -1;
}

static int
gids_unload(void)
{
  int rslt = 0;
  int retval;
  FILE *fl = NULL;
  struct db_gid *pnext;
  char *enc = NULL;

  LOG1("Dumping GIDs database to: %s", fl_gids_fullname);
  fl = fopen(fl_gids_fullname, "wb");
  if (!fl) {
    ERROR("%s: unable to open for writing: %s",
        fl_gids_fullname, strerror(errno));
    rslt = -1;
    goto cleanup;
  }

  while (pdb_gids) {
    pnext = pdb_gids->next;

    enc = malloc_str_encode(pdb_gids->gid_name);
    if (!enc) {
      ERROR("Out of memory");
      rslt = -1;
      goto cleanup;
    }
    retval = fprintf(fl, "%d %s\n", pdb_gids->gid, enc);
    if (retval < 0) {
      ERROR("%s: cannot write to file: %s", fl_gids_fullname, strerror(errno));
      rslt = -1;
      goto cleanup;
    }

    free(enc);
    enc = NULL;
    
    free(pdb_gids);
    pdb_gids = pnext;
  }

  if (fl) {
    retval = fclose(fl);
    fl = NULL;
    if (retval != 0) {
      ERROR("%s: cannot close: %s", fl_gids_fullname, strerror(errno));
      rslt = -1;
      goto cleanup;
    }
  }

cleanup:
  if (fl) {
    if (0 != fclose(fl))
      ERROR("%s: cannot close: %s", fl_gids_fullname, strerror(errno));
    fl = NULL;
  }
  if (enc)
    free(enc);
  return rslt;
}

static int
gids_load(void)
{
  FILE *fl = NULL;
  int rslt = 0;
  int retval;
  char buf[8192];
  char id_buf[64];
  int eof = 0;
  char *p;

  fl = fopen(fl_gids_fullname, "rb");
  if (!fl) {
    LOG2("%s: unable to open for reading: %s",
        fl_gids_fullname, strerror(errno));
    LOG2("Assuming an empty database");
    rslt = ERR_NOENT;
    goto cleanup;
  }

  for (;;) {
    retval = get_line(fl_gids_fullname, buf, ARRAY_SIZE(buf), fl, 1, &eof);
    if (retval != 0) {
      rslt = -1;
      goto cleanup;
    }
    if (eof)
      break;
    LOG2("read line: %s", buf);

    p = buf;
    retval = get_word(&p, id_buf, ARRAY_SIZE(id_buf));
    if (retval != 0) {
      ERROR("%s: syntax error", fl_gids_fullname);
      rslt = -1;
      goto cleanup;
    }
    
    skip_spcrlf(&p);
    if (0 != str_decode(p)) {
      ERROR("%s: syntax error", fl_gids_fullname);
      rslt = -1;
      goto cleanup;
    }

    if (0 != gids_push_back(p, atoi(id_buf))) {
      rslt = -1;
      goto cleanup;
    }
  }

  if (fl) {
    retval = fclose(fl);
    fl = NULL;
    if (retval != 0) {
      ERROR("%s: cannot close: %s", fl_gids_fullname, strerror(errno));
      rslt = -1;
      goto cleanup;
    }
  }

cleanup:
  if (rslt != 0) {
    if (fl) {
      if (0 != fclose(fl))
        ERROR("%s: cannot close: %s", fl_gids_fullname, strerror(errno));
      fl = NULL;
    }
  }
  return rslt;
}

/****************************************************************************/
/* TEXT                                                                     */
/****************************************************************************/
int
db_generate_sid(int gid, int *psid)
{
  struct db_text *ent;
  int sid = 0;
  int sid_exists;

  for (;;) {
    sid_exists = 0;
    for (ent = pdb_text; ent; ent = ent->next) {
      if (ent->gid == gid && ent->sid == sid) {
        sid_exists = 1;
        break;
      }
    }
    if (!sid_exists)
      break;
    ++sid;
  }
  *psid = sid;

  return 0;
}

static int
text_push_back(int file_id, int gid, int sid, int loglevel, char *text)
{
  int rslt = 0;
  struct db_text *ent;

  ent = (struct db_text *) malloc(sizeof(struct db_text));
  if (!ent) {
    ERROR("Out of memory");
    rslt = -1;
    goto cleanup;
  }
  memset(ent, 0, sizeof(*ent));

  ent->file_id = file_id;
  ent->gid = gid;
  ent->sid = sid;
  ent->loglevel = loglevel;
  ent->text = strdup(text);
  if (!ent->text) {
    ERROR("Out of memory");
    rslt = -1;
    goto cleanup;
  }

  ent->next = pdb_text;
  pdb_text = ent;
  LOG2("Text added to db (file_id %d, gid %d, sid %d): %s",
      file_id, gid, sid, text);

cleanup:
  if (rslt != 0) {
    if (ent) {
      if (ent->text)
        free(ent->text);
      free(ent);
    }
  }
  return rslt;
}

void
db_destroy_text(int file_id)
{
  struct db_text *ent;
  struct db_text *next_ent;

  for (ent = pdb_text; ent; ent = next_ent) {
    next_ent = ent->next;
    if (ent->file_id == file_id) {
      LIST_REMOVE(pdb_text, ent, struct db_text, next);
      free(ent);
    }
  }
}

int
db_register_text(int file_id, int gid, int sid, int loglevel, char *text)
{
  int rslt = 0;
  struct db_text *ent;

  for (ent = pdb_text; ent; ent = ent->next) {
    if (ent->gid == gid && ent->sid == sid) {
      ERROR("Duplicate text entry found for GID/SID pair: %d,%d",
          gid, sid);
      /*
      LOG1("Duplicate text entry found for GID/SID pair: %d,%d",
          gid, sid);
      if (0 != db_generate_sid(gid, &sid)) {
        LOG1("Cannot generate a new SID for GID %d");
        rslt = -1;
        goto cleanup;
      }
      LOG1("Generated a new SID: %d", sid);
      break;
      */
      rslt = -1;
      goto cleanup;
    }
  }
  
  if (0 != text_push_back(file_id, gid, sid, loglevel, text)) {
    rslt = -1;
    goto cleanup;
  }

cleanup:
  return rslt;
}

void
db_unregister_text(int file_id, int gid, int sid, int loglevel)
{
  struct db_text *ent;
  struct db_text *next_ent;
  
  for (ent = pdb_text; ent; ent = next_ent) {
    next_ent = ent->next;
    if (ent->gid == gid && 
        ent->sid == sid &&
        ent->loglevel == loglevel) {
      LIST_REMOVE(pdb_text, ent, struct db_text, next);
      free(ent);
    }
  }
}

static int
text_unload(void)
{
  int rslt = 0;
  int retval;
  FILE *fl = NULL;
  struct db_text *pnext;
  char *enc = NULL;

  LOG1("Dumping TEXT database to: %s", fl_text_fullname);
  fl = fopen(fl_text_fullname, "wb");
  if (!fl) {
    ERROR("%s: unable to open for writing: %s",
        fl_text_fullname, strerror(errno));
    rslt = -1;
    goto cleanup;
  }

  while (pdb_text) {
    pnext = pdb_text->next;

    enc = malloc_str_encode(pdb_text->text);
    if (!enc) {
      ERROR("Out of memory");
      rslt = -1;
      goto cleanup;
    }
    retval = fprintf(fl, "%d %d %d %d %s\n",
        pdb_text->file_id, pdb_text->gid, pdb_text->sid,
        pdb_text->loglevel, enc);
    if (retval < 0) {
      ERROR("%s: cannot write to file: %s", fl_text_fullname, strerror(errno));
      rslt = -1;
      goto cleanup;
    }

    free(enc);
    enc = NULL;
    
    // FIXME: free pdb_text->text ?

    free(pdb_text);
    pdb_text = pnext;
  }

  if (fl) {
    retval = fclose(fl);
    fl = NULL;
    if (retval != 0) {
      ERROR("%s: cannot close: %s", fl_text_fullname, strerror(errno));
      rslt = -1;
      goto cleanup;
    }
  }

cleanup:
  if (fl) {
    if (0 != fclose(fl))
      ERROR("%s: cannot close: %s", fl_text_fullname, strerror(errno));
    fl = NULL;
  }
  if (enc)
    free(enc);
  return rslt;
}

static int
text_load(void)
{
  FILE *fl = NULL;
  int rslt = 0;
  int retval;
  char buf[8192];
  char id_buf[64];
  int file_id, gid, sid;
  int eof = 0;
  char *p;
  char loglevel_buf[MAX_LOGLEVEL_LEN];
  int loglevel = 0;

  fl = fopen(fl_text_fullname, "rb");
  if (!fl) {
    LOG2("%s: unable to open for reading: %s",
        fl_text_fullname, strerror(errno));
    LOG2("Assuming an empty database");
    rslt = ERR_NOENT;
    goto cleanup;
  }

  for (;;) {
    retval = get_line(fl_text_fullname, buf, ARRAY_SIZE(buf), fl, 1, &eof);
    if (retval != 0) {
      rslt = -1;
      goto cleanup;
    }
    if (eof)
      break;
    LOG2("read line: %s", buf);

    p = buf;

    retval = get_word(&p, id_buf, ARRAY_SIZE(id_buf));
    if (retval != 0) {
      ERROR("%s: syntax error", fl_text_fullname);
      rslt = -1;
      goto cleanup;
    }
    file_id = atoi(id_buf);

    retval = get_word(&p, id_buf, ARRAY_SIZE(id_buf));
    if (retval != 0) {
      ERROR("%s: syntax error", fl_text_fullname);
      rslt = -1;
      goto cleanup;
    }
    gid = atoi(id_buf);

    retval = get_word(&p, id_buf, ARRAY_SIZE(id_buf));
    if (retval != 0) {
      ERROR("%s: syntax error", fl_text_fullname);
      rslt = -1;
      goto cleanup;
    }
    sid = atoi(id_buf);

    retval = get_word(&p, loglevel_buf, ARRAY_SIZE(loglevel_buf));
    if (retval != 0) {
      ERROR("%s: syntax error", fl_text_fullname);
      rslt = -1;
      goto cleanup;
    }
    loglevel = atoi(loglevel_buf);
    
    skip_spcrlf(&p);
    if (0 != str_decode(p)) {
      ERROR("%s: syntax error", fl_text_fullname);
      rslt = -1;
      goto cleanup;
    }

    //if (0 != text_push_back(file_id, gid, sid, loglevel, p)) {
    if (0 != db_register_text(file_id, gid, sid, loglevel, p)) {
      rslt = -1;
      goto cleanup;
    }
  }

  if (fl) {
    retval = fclose(fl);
    fl = NULL;
    if (retval != 0) {
      ERROR("%s: cannot close: %s", fl_text_fullname, strerror(errno));
      rslt = -1;
      goto cleanup;
    }
  }

cleanup:
  if (rslt != 0) {
    if (fl) {
      if (0 != fclose(fl))
        ERROR("%s: cannot close: %s", fl_text_fullname, strerror(errno));
      fl = NULL;
    }
  }
  return rslt;
}

