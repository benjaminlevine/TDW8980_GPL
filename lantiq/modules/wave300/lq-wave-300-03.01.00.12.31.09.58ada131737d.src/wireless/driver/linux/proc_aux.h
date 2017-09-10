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
 * Auxiliary functions for registering and handling proc entries.
 *
 */

#ifndef __PROC_AUX_H__
#define __PROC_AUX_H__

#include <linux/seq_file.h>

/* structure to hold seq_ops near mtlk_core_t pointer
 * so we can get it from seq_ functions
 */
struct mtlk_seq_ops {
  struct list_head      list;
  mtlk_core_t          *nic;
  struct semaphore      sem;
  struct seq_operations seq_ops;
};


/* register proc_dir_entry and init it. */
struct proc_dir_entry *
register_one_proc_entry(mtlk_core_t *nic, struct proc_dir_entry *parent,
                        read_proc_t rfn, write_proc_t wfn, char *name, mode_t mode);


/* register proc_dir_entry for "one function" sequential files */
struct proc_dir_entry *
prepare_simple_seq_file(mtlk_core_t *nic, 
                        struct proc_dir_entry *parent,
                        char *name,
                        struct seq_operations *ops);

/* register proc_dir_entry for "one function" sequential files */
static inline struct proc_dir_entry *
prepare_one_fn_seq_file(mtlk_core_t *nic, 
                        int (*show_fn)(struct seq_file *, void *),
                        struct proc_dir_entry *parent,
                        char *name)
{
  struct seq_operations ops = (struct seq_operations) {
    .show = show_fn
  };
  
  return prepare_simple_seq_file(nic, 
                                 parent,
                                 name,
                                 &ops);
}

int mtlk_proc_aux_read_string (char *page, char **start, off_t off,
                               int count, int *eof, void *data);

int mtlk_proc_aux_read (char *page, char **start, off_t off,
                        int count, int *eof, void *data);

#endif /* __PROC_AUX_H__ */
