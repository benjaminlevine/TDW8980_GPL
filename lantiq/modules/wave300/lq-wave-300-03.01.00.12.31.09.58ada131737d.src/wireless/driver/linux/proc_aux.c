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
#include "mtlkinc.h"

#include <linux/module.h>

#include "core.h"
#include "proc_aux.h"

enum dbg_assert_type
{
	MTLK_DBGAT_NONE,
	MTLK_DBGAT_FW_UMIPS,
	MTLK_DBGAT_FW_LMIPS,
	MTLK_DBGAT_DRV_DIV0,
	MTLK_DBGAT_DRV_BLOOP,
	MTLK_DBGAT_LAST
};

/* these three functions are used to "emulate" simplified
 * seq_file methods (single_*). we can't use it because
 * we have older kernels (e.g. 2.4.20 on cavium) which 
 * lack this functionality.
 */

static void *one_fn_seq_start(struct seq_file *s, loff_t *pos)
{
  /* this function must return non-NULL pointer
   * to start new sequence, so the address of this 
   * variable is used. just don't want to return
   * pointer to "something".
   */
  static unsigned long counter = 0;

  /* beginning a new sequence ? */
  if ( *pos == 0 )
    return &counter;
  else {
    /* no => it's the end of the sequence, return NULL to stop reading */
    *pos = 0;
    return NULL;
  }
}

static void *one_fn_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
  /* increase position, so the next invokation of
   * .seq_start will return NULL.
   */
  (*pos)++;

  /* return NULL to stop the current sequence */
  return NULL;
}

static void one_fn_seq_stop(struct seq_file *s, void *v)
{
  /* do nothing here */
  return;
}

/* these two functions are registered as .open and .release
 * methods of the file_operations structure used by all
 * "one-function" seq files.
 */
static int one_fn_seq_fop_open(struct inode *inode, struct file *file)
{
  struct mtlk_seq_ops *ops = (PDE(inode))->data;

  /* try to get semaphore without sleep first */
  if(down_trylock(&ops->sem)) {
    if(file->f_flags & O_NONBLOCK)
      return -EAGAIN;
    /* ok, we'll have to sleep if it's still not free */
    if(down_interruptible(&ops->sem))
      return -EINTR;
  };

  /* we've got semaphore, so proceed */
  return seq_open(file, &ops->seq_ops);
}

static int one_fn_seq_fop_release(struct inode *inode, struct file *file)
{
  struct mtlk_seq_ops *ops = (PDE(inode))->data;

  /* release semaphore acquired in the .open function */
  up(&ops->sem);
  return seq_release(inode, file);
}

/* this is the common structure used for all one
 * function seq_file files. open method gets per
 * file semaphore and calls seq_open, release 
 * method releases per-file semaphore and calls
 * seq_release.
 */
static struct file_operations one_fn_seq_fops = {
  .owner   = THIS_MODULE,
  .open    = one_fn_seq_fop_open,
  .read    = seq_read,
  .llseek  = seq_lseek,
  .release = one_fn_seq_fop_release
};

/* this function registers proc_dir_entry with fops for
 * "one_fn" sequential files.
 */
struct proc_dir_entry *
prepare_simple_seq_file (mtlk_core_t *nic, 
                         struct proc_dir_entry *parent,
                         char *name,
                         struct seq_operations *ops)
{
  struct mtlk_seq_ops *tmp_seq_ops;
  struct proc_dir_entry *pde;

  tmp_seq_ops = kmalloc_tag(sizeof(*tmp_seq_ops), GFP_KERNEL, MTLK_MEM_TAG_PROC);
  if (!tmp_seq_ops){
    ELOG("Cannot allocate memory for mtlk_seq_ops structure.");
    goto alloc_err;
  }
  memset(tmp_seq_ops, 0, sizeof(*tmp_seq_ops));

  /* initialize newly allocated mtlk_seq_ops structure */
  INIT_LIST_HEAD(&tmp_seq_ops->list);
  init_MUTEX(&tmp_seq_ops->sem);
  tmp_seq_ops->nic = nic;
  tmp_seq_ops->seq_ops = (struct seq_operations) {
    .start = ops->start?ops->start:one_fn_seq_start,
    .next  = ops->next?ops->next:one_fn_seq_next,
    .stop  = ops->stop?ops->stop:one_fn_seq_stop,
    .show  = ops->show
  };

  /* create and init proc entry */
  pde = create_proc_entry(name, S_IFREG|S_IRUSR|S_IRGRP|S_IROTH, parent);
  if (!pde) {
    ELOG("Failed to create proc entry for %s", name);
    goto pde_err;
  };

  pde->uid = MTLK_PROCFS_UID;
  pde->gid = MTLK_PROCFS_GID;
  pde->data = tmp_seq_ops;
  pde->proc_fops = &one_fn_seq_fops;
  nic->slow_ctx->procfs_entry[nic->slow_ctx->pentry_num++] = pde;

  /* adding allocated structure to the list, so it
   * can be easily tracked and freed on cleanup
   */
  list_add(&tmp_seq_ops->list, &nic->slow_ctx->seq_ops_list);

  return pde;

pde_err:
  kfree_tag(tmp_seq_ops);

alloc_err:
  return NULL;
}


/* this function creates and initialize proc entry.
 * .data field of the struct proc_dir_entry is set
 * to nic parameter.
 */
struct proc_dir_entry *
register_one_proc_entry(mtlk_core_t *nic, struct proc_dir_entry *parent,
                        read_proc_t rfn, write_proc_t wfn, char *name, mode_t mode)
{
  struct proc_dir_entry *pde;

  /* create and init entry */
  pde = create_proc_entry(name, mode, parent);
  if (!pde) {
    ELOG("Failed to create proc entry: %s", name);
    return NULL;
  };

  pde->uid = MTLK_PROCFS_UID;
  pde->gid = MTLK_PROCFS_GID;
  pde->data = nic;

  if(rfn)
    pde->read_proc = rfn;

  if(wfn)
    pde->write_proc = wfn;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
  /* set the .owner field, so that reference count of the
   * module increment when this file is opened 
   */
  pde->owner = THIS_MODULE;
#endif

  /* add this entry to the .procfs_entry of struct nic,
   * so that it will be removed on cleanup
   */
  nic->slow_ctx->procfs_entry[nic->slow_ctx->pentry_num++] = pde;

  return pde;
}

int
mtlk_proc_aux_read_string (char *page, char **start, off_t off,
                           int count, int *eof, void *data)
{
  if (off) {
    *eof = 1;
    return 0;
  }

  strcpy(page, data);

  return strlen(data) + 1;
}

int
do_debug_assert_write (struct file *file, const char *buf,
                            unsigned long count, void *data)
{
  char str[MAX_PROC_STR_LEN];
  int assert_type;
  int param_cnt = 0;
  mtlk_core_t* nic = (mtlk_core_t*) data;

  if (count > MAX_PROC_STR_LEN)
    return -EINVAL;

  memset(str, 0, sizeof(str));
  if (copy_from_user(str, buf, count))
    return -EFAULT;

  param_cnt = sscanf(str, "%d", &assert_type);

  if (1 > param_cnt) {
    assert_type = MTLK_DBGAT_NONE;
  }

  ILOG0("Rise MAC assert (param_cnt=%d) (type=%d) (dev_name=%s)", param_cnt, assert_type, nic->ndev->name);

  switch (assert_type) {
  case MTLK_DBGAT_FW_UMIPS:
  case MTLK_DBGAT_FW_LMIPS:
    {
      mtlk_hw_t *hw = nic->hw;

      if (NULL != hw) {
	    uint32 mips_no = (assert_type == MTLK_DBGAT_FW_UMIPS)?UMIPS:LMIPS;
	    int res        = mtlk_hw_set_prop(hw, MTLK_HW_DBG_ASSERT_FW, &mips_no, sizeof(mips_no));

		if (res != MTLK_ERR_OK) {
 			ELOG("Can't assert FW MIPS#%d (res=%d) (dev_name=%s)", mips_no, res, nic->ndev->name);
		}
      }
    }
    break;
  case MTLK_DBGAT_DRV_DIV0:
    {
      volatile int do_bug = 0;
      do_bug = 1/do_bug;
      ILOG0("do_bug = %d", do_bug); /* To avoid compilation optimization */
    }
    break;
  case MTLK_DBGAT_DRV_BLOOP:
    while (1) {;}
    break;
  case MTLK_DBGAT_NONE:
  case MTLK_DBGAT_LAST:
  default:
    ILOG0("Unsupported assert type: %d", assert_type);
    break;
  };

  return count;
}

int
mtlk_proc_aux_read (char *page, char **start, off_t off,
                    int count, int *eof, void *data)
{
    int  *var = (int *)data;
    char *p   = page;

    if (off != 0)
    {
        *eof = 1;
        return 0;
    }
  
    p += sprintf(p, "%d\n", *var);

    return(p - page);
}

