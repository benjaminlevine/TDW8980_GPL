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

#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <linux/limits.h>

#include "logprep.h"
#include "parser.h"
#include "db.h"
#include "codegen.h"
#include "utils.h"

volatile int terminated = 0;
int deadlock_ctr = 0;
int sigints = 0;
int filename_id = -1;
char *working_dir = NULL;
int preserve_dt = 0;
int sid_no_reuse = 0;
int filename_scd = 0;
char *logdefs_fname = NULL;
int origin_id = 0;
int gen_db_only = 0;

int
main_loop(void)
{
  int rslt = 0;
  FILE *out = NULL;
  //char outfl_name[] = ".logprep_XXXXXX";
  char path[PATH_MAX];
  char *dir = NULL;
  int cnt;
  char outfl_name[] = ".logprep_temp";
  char outfl_fullname[PATH_MAX + NAME_MAX];
  char *in_buf = NULL;
  int in_size;
  char *out_buf = NULL;
  int out_size;

  // Update ticks
  deadlock_ctr = 1;
  sigints = 0;

  // Generate a filename
  /* FIXME: mkstemp opens file
  if (-1 == mkstemp(outfl_name)) {
    ERROR("Unable to create a filename for temporary file: %s",
        strerror(errno));
    rslt = -1;
    goto cleanup;
  }
  */
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

#if 0
  if (0 != replace_filename(filename, outfl_name, /* out */ outfl_fullname)) {
    rslt = -1;
    goto cleanup;
  }
#else
  cnt = snprintf(outfl_fullname, ARRAY_SIZE(outfl_fullname), "%s%s%s",
      dir, (ends_with(dir, "/") ? "" : "/"), outfl_name);
  if (cnt < 0 || cnt >= ARRAY_SIZE(outfl_fullname)) {
    ERROR("Unable to create a filename for temporary file: filename too long");
    rslt = -1;
    goto cleanup;
  }
#endif
  
  if (0 != alloc_buf_from_file(&in_buf, &in_size, filename)) {
    rslt = -1;
    goto cleanup;
  }

  if (gen_db_only) {
    if (0 != process_in_buf(in_buf, in_size, NULL)) {
      rslt = -1;
    }    
    goto cleanup;
  }
  
  out = fopen(outfl_fullname, "wb");
  if (!out) {
    ERROR("%s: unable to open for writing: %s",
          outfl_fullname, strerror(errno));
    rslt = -1;
    goto cleanup;
  }

  if (0 != process_in_buf(in_buf, in_size, out)) {
    rslt = -1;
    goto cleanup;
  }

  if (0 != fclose(out)) {
    out = NULL; // trying to close it again will lead to unexpected behavior
    ERROR("%s: cannot close: %s", outfl_fullname, strerror(errno));
    rslt = -1;
    goto cleanup;
  }
  out = NULL;

  // Load the file we created into memory to compare it with original file
  // contents
  if (0 != alloc_buf_from_file(&out_buf, &out_size, outfl_fullname)) {
    rslt = -1;
    goto cleanup;
  }
  
  // If the resulting file and the original file are the same, do not replace
  // the original file
  if (in_size == out_size &&
      0 == memcmp(in_buf, out_buf, in_size)) {
    if (0 != remove(outfl_fullname)) {
      ERROR("%s: cannot remove: %s", outfl_fullname, strerror(errno));
      rslt = -1;
      goto cleanup;
    }
  } else {
    if (0 != rename(outfl_fullname, filename)) {
      ERROR("%s: cannot rename to \"%s\": %s",
          outfl_fullname, filename, strerror(errno));
      rslt = -1;
      goto cleanup;
    }
  }

  if (0 != cg_create_inc_src_files()) {
    rslt = -1;
    goto cleanup;
  }

cleanup:
  if (out) {
    if (0 != fclose(out))
      ERROR("%s: cannot close: %s", outfl_fullname, strerror(errno));
  }
  if (in_buf)
    free(in_buf);
  if (out_buf)
    free(out_buf);

  return rslt;
}

void
on_sighup(int sig)
{
  WARNING("Received SIGHUP: shutting down");
  terminated = 1;
}

void
on_sigchld(int sig)
{
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
  signal(SIGCHLD, on_sigchld);
}

void
on_sigint(int sig)
{
  WARNING("Received SIGINT: shutting down");
  ++sigints; // on deadlock multiple sigints force abort
  terminated = 1;

  if (sigints >= 2) {
    ERROR("Deadlock state (previous SIGINT wasn't processed), aborting");
    abort();
  }
}

void
on_sigterm(int sig)
{
  WARNING("Received SIGTERM: shutting down");
  terminated = 1;
}

void
on_sigvtalrm(int sig)
{
  LOG9("Received sigvtalrm: deadlock check");
  if (deadlock_ctr == 0) {
    ERROR("Deadlock state, aborting");
    abort();
  } else {
    deadlock_ctr = 0;
  }
}

void
setup_signals()
{
  struct itimerval itv;
  struct timeval tv;

  signal(SIGHUP,  on_sighup);
  signal(SIGCHLD, on_sigchld);
  signal(SIGINT,  on_sigint);
  signal(SIGTERM, on_sigterm);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGALRM, SIG_IGN);

  /*
   * Deadlock protection
   */
  tv.tv_sec = 10;
  tv.tv_usec = 0;
  itv.it_interval = itv.it_value = tv;
  setitimer(ITIMER_VIRTUAL, &itv, NULL);
  signal(SIGVTALRM, on_sigvtalrm);
}

void
process_commandline(int argc, char *argv[])
{
  int filename_warned = 0;
  //char *endptr = NULL;

  int i;
  for (i = 1; i < argc; ++i) {
    char *p = argv[i];
    if (*p == '-') {
      ++p;
      
      switch (tolower(*p++)) {

      case '-': // --option
        if (!strcasecmp(p, "silent")) {
          debug = -1;
          continue;
        } else if (!strcasecmp(p, "workdir")) {
          ++i;
          if (i < argc)
            working_dir = argv[i];
          continue;
        } else if (!strcasecmp(p, "logdefs-file")) {
          ++i;
          if (i < argc)
            logdefs_fname = argv[i];
          continue;
        } else if (!strcasecmp(p, "preserve-dt")) {
          preserve_dt = -1;
          continue;
        } else if (!strcasecmp(p, "sid-no-reuse")) {
          sid_no_reuse = 1;
          continue;
        } else if (!strcasecmp(p, "oid")) {
          ++i;
          if (i < argc)
            origin_id = atoi(argv[i]);
          continue;
        } else if (!strcasecmp(p, "gen-db-only")) {
          gen_db_only = 1;
          continue;
        }
        break;

      case 'd':
        debug = atoi(p++);
        continue;

      case 'o':
        origin_id = atoi(p++);
        continue;

      case 's':
        filename_scd = 1;
        continue;

      default:
        break;
      }

      ERROR("Unknown commandline parameter: %s", argv[i]);
    } else {
      if (filename) {
        if (!filename_warned) {
          ERROR("Multiple filenames specified: can process only one file at a "
              "time");
          filename_warned = 1;
        }
        continue;
      } else {
        filename = argv[i];
        continue;
      }
    }
  }
}

int
main(int argc, char *argv[])
{
  int rslt = 0;

  process_commandline(argc, argv);

  INFO("Metalink Logger Source Code Preprocessor v." MTLK_PACKAGE_VERSION);
  
  setup_signals();
  
  if (!filename) {
    ERROR("Please specify a filename to process");
    rslt = 1;
    goto cleanup;
  }

  if (0 != db_init(filename)) {
    rslt = 1;
    goto cleanup;
  }

  if (0 != register_known_gids()) {
    rslt = 1;
    goto cleanup;
  }

  if (!filename_scd) {
    if (0 != db_register_filename(filename, &filename_id)) {
      rslt = 1;
      goto cleanup;
    }

    if (sid_no_reuse) {
      // Destroy log text messages for this file because we're going to
      // replace them:
      db_destroy_text(filename_id);
    }

    if (0 != main_loop()) {
      rslt = 1;
      goto cleanup;
    }
  } else {
    if (0 != cg_create_scd_file()) {
      rslt = 1;
      goto cleanup;
    }
  }

  if (0 != db_destroy()) {
    rslt = 1;
    goto cleanup;
  }

  INFO("Terminated");

cleanup:
  return rslt;
}

