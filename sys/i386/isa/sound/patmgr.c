/*
 * linux/kernel/chr_drv/sound/patmgr.c
 * 
 * The patch maneger interface for the /dev/sequencer
 * 
 * (C) 1993  Hannu Savolainen (hsavolai@cs.helsinki.fi) See COPYING for further
 * details. Should be distributed with this file.
 */

#define PATMGR_C
#include "sound_config.h"

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_SEQUENCER)

DEFINE_WAIT_QUEUES (server_procs[MAX_SYNTH_DEV],
		    server_wait_flag[MAX_SYNTH_DEV]);

static struct patmgr_info *mbox[MAX_SYNTH_DEV] =
{NULL};
static volatile int msg_direction[MAX_SYNTH_DEV] =
{0};

static int      pmgr_opened[MAX_SYNTH_DEV] =
{0};

#define A_TO_S	1
#define S_TO_A 	2

DEFINE_WAIT_QUEUE (appl_proc, appl_wait_flag);

int
pmgr_open (int dev)
{
  if (dev < 0 || dev >= num_synths)
    return RET_ERROR (ENXIO);

  if (pmgr_opened[dev])
    return RET_ERROR (EBUSY);
  pmgr_opened[dev] = 1;

  return 0;
}

void
pmgr_release (int dev)
{

  if (mbox[dev])		/* Killed in action. Inform the client */
    {

      mbox[dev]->key = PM_ERROR;
      mbox[dev]->parm1 = RET_ERROR (EIO);

      if (appl_wait_flag)
	WAKE_UP (appl_proc);
    }

  pmgr_opened[dev] = 0;
}

int
pmgr_read (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  unsigned long   flags;
  int             ok = 0;

  if (count != sizeof (struct patmgr_info))
    {
      printk ("PATMGR%d: Invalid read count\n", dev);
      return RET_ERROR (EIO);
    }

  while (!ok && !PROCESS_ABORTING)
    {
      DISABLE_INTR (flags);

      while (!(mbox[dev] && msg_direction[dev] == A_TO_S) && !PROCESS_ABORTING)
	{
	  INTERRUPTIBLE_SLEEP_ON (server_procs[dev], server_wait_flag[dev]);
	}

      if (mbox[dev] && msg_direction[dev] == A_TO_S)
	{
	  COPY_TO_USER (buf, 0, (char *) mbox[dev], count);
	  msg_direction[dev] = 0;
	  ok = 1;
	}

      RESTORE_INTR (flags);

    }

  if (!ok)
    return RET_ERROR (EINTR);
  return count;
}

int
pmgr_write (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  unsigned long   flags;

  if (count < 4)
    {
      printk ("PATMGR%d: Write count < 4\n", dev);
      return RET_ERROR (EIO);
    }

  COPY_FROM_USER (mbox[dev], buf, 0, 4);

  if (*(unsigned char *) mbox[dev] == SEQ_FULLSIZE)
    {
      int             tmp_dev;

      tmp_dev = ((unsigned short *) mbox[dev])[2];
      if (tmp_dev != dev)
	return RET_ERROR (ENXIO);

      return synth_devs[dev]->load_patch (dev, *(unsigned short *) mbox[dev],
					  buf, 4, count, 1);
    }

  if (count != sizeof (struct patmgr_info))
    {
      printk ("PATMGR%d: Invalid write count\n", dev);
      return RET_ERROR (EIO);
    }

  /*
   * If everything went OK, there should be a preallocated buffer in the
   * mailbox and a client waiting.
   */

  DISABLE_INTR (flags);

  if (mbox[dev] && !msg_direction[dev])
    {
      COPY_FROM_USER (&((char *) mbox[dev])[4], buf, 4, count - 4);
      msg_direction[dev] = S_TO_A;

      if (appl_wait_flag)
	{
	  WAKE_UP (appl_proc);
	}
    }

  RESTORE_INTR (flags);

  return count;
}

int
pmgr_access (int dev, struct patmgr_info *rec)
{
  unsigned long   flags;
  int             err = 0;

  DISABLE_INTR (flags);

  if (mbox[dev])
    printk ("  PATMGR: Server %d mbox full. Why?\n", dev);
  else
    {
      rec->key = PM_K_COMMAND;
      mbox[dev] = rec;
      msg_direction[dev] = A_TO_S;

      if (server_wait_flag[dev])
	{
	  WAKE_UP (server_procs[dev]);
	}

      INTERRUPTIBLE_SLEEP_ON (appl_proc, appl_wait_flag);

      if (msg_direction[dev] != S_TO_A)
	{
	  rec->key = PM_ERROR;
	  rec->parm1 = RET_ERROR (EIO);
	}
      else if (rec->key == PM_ERROR)
	{
	  err = rec->parm1;
	  if (err > 0)
	    err = -err;
	}

      mbox[dev] = NULL;
      msg_direction[dev] = 0;
    }

  RESTORE_INTR (flags);

  return err;
}

int
pmgr_inform (int dev, int event, unsigned long p1, unsigned long p2,
	     unsigned long p3, unsigned long p4)
{
  unsigned long   flags;
  int             err = 0;

  if (!pmgr_opened[dev])
    return 0;

  DISABLE_INTR (flags);

  if (mbox[dev])
    printk ("  PATMGR: Server %d mbox full. Why?\n", dev);
  else
    {
      mbox[dev] =
	(struct patmgr_info *) KERNEL_MALLOC (sizeof (struct patmgr_info));

      mbox[dev]->key = PM_K_EVENT;
      mbox[dev]->command = event;
      mbox[dev]->parm1 = p1;
      mbox[dev]->parm2 = p2;
      mbox[dev]->parm3 = p3;
      msg_direction[dev] = A_TO_S;

      if (server_wait_flag[dev])
	{
	  WAKE_UP (server_procs[dev]);
	}

      INTERRUPTIBLE_SLEEP_ON (appl_proc, appl_wait_flag);
      if (mbox[dev])
	KERNEL_FREE (mbox[dev]);
      mbox[dev] = NULL;
      msg_direction[dev] = 0;
    }

  RESTORE_INTR (flags);

  return err;
}

#endif
