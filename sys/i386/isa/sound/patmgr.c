/*
 * sound/patmgr.c
 *
 * The patch manager interface for the /dev/sequencer
 *
 * Copyright by Hannu Savolainen 1993
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
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

  RESET_WAIT_QUEUE (server_procs[dev], server_wait_flag[dev]);

  return 0;
}

void
pmgr_release (int dev)
{

  if (mbox[dev])		/*
				 * Killed in action. Inform the client
				 */
    {

      mbox[dev]->key = PM_ERROR;
      mbox[dev]->parm1 = RET_ERROR (EIO);

      if (SOMEONE_WAITING (appl_proc, appl_wait_flag))
	WAKE_UP (appl_proc, appl_wait_flag);
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

  while (!ok && !PROCESS_ABORTING (server_procs[dev], server_wait_flag[dev]))
    {
      DISABLE_INTR (flags);

      while (!(mbox[dev] && msg_direction[dev] == A_TO_S) &&
	     !PROCESS_ABORTING (server_procs[dev], server_wait_flag[dev]))
	{
	  DO_SLEEP (server_procs[dev], server_wait_flag[dev], 0);
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

      if (SOMEONE_WAITING (appl_proc, appl_wait_flag))
	{
	  WAKE_UP (appl_proc, appl_wait_flag);
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

      if (SOMEONE_WAITING (server_procs[dev], server_wait_flag[dev]))
	{
	  WAKE_UP (server_procs[dev], server_wait_flag[dev]);
	}

      DO_SLEEP (appl_proc, appl_wait_flag, 0);

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

      if (SOMEONE_WAITING (server_procs[dev], server_wait_flag[dev]))
	{
	  WAKE_UP (server_procs[dev], server_wait_flag[dev]);
	}

      DO_SLEEP (appl_proc, appl_wait_flag, 0);
      if (mbox[dev])
	KERNEL_FREE (mbox[dev]);
      mbox[dev] = NULL;
      msg_direction[dev] = 0;
    }

  RESTORE_INTR (flags);

  return err;
}

#endif
