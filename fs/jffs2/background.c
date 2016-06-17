/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2001 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@cambridge.redhat.com>
 *
 * The original JFFS, from which the design for JFFS2 was derived,
 * was designed and implemented by Axis Communications AB.
 *
 * The contents of this file are subject to the Red Hat eCos Public
 * License Version 1.1 (the "Licence"); you may not use this file
 * except in compliance with the Licence.  You may obtain a copy of
 * the Licence at http://www.redhat.com/
 *
 * Software distributed under the Licence is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.
 * See the Licence for the specific language governing rights and
 * limitations under the Licence.
 *
 * The Original Code is JFFS2 - Journalling Flash File System, version 2
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License version 2 (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of the
 * above.  If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the RHEPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the RHEPL or the GPL.
 *
 * $Id: background.c,v 1.16 2001/10/08 09:22:38 dwmw2 Exp $
 *
 */

#define __KERNEL_SYSCALLS__

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/jffs2.h>
#include <linux/mtd/mtd.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include "nodelist.h"


static int jffs2_garbage_collect_thread(void *);
static int thread_should_wake(struct jffs2_sb_info *c);

void jffs2_garbage_collect_trigger(struct jffs2_sb_info *c)
{
	spin_lock_bh(&c->erase_completion_lock);
        if (c->gc_task && thread_should_wake(c))
                send_sig(SIGHUP, c->gc_task, 1);
	spin_unlock_bh(&c->erase_completion_lock);
}

/* This must only ever be called when no GC thread is currently running */
int jffs2_start_garbage_collect_thread(struct jffs2_sb_info *c)
{
	pid_t pid;
	int ret = 0;

	if (c->gc_task)
		BUG();

	init_MUTEX_LOCKED(&c->gc_thread_start);
	init_completion(&c->gc_thread_exit);

	pid = kernel_thread(jffs2_garbage_collect_thread, c, CLONE_FS|CLONE_FILES);
	if (pid < 0) {
		printk(KERN_WARNING "fork failed for JFFS2 garbage collect thread: %d\n", -pid);
		complete(&c->gc_thread_exit);
		ret = pid;
	} else {
		/* Wait for it... */
		D1(printk(KERN_DEBUG "JFFS2: Garbage collect thread is pid %d\n", pid));
		down(&c->gc_thread_start);
	}
 
	return ret;
}

void jffs2_stop_garbage_collect_thread(struct jffs2_sb_info *c)
{
	spin_lock_bh(&c->erase_completion_lock);
	if (c->gc_task) {
		D1(printk(KERN_DEBUG "jffs2: Killing GC task %d\n", c->gc_task->pid));
		send_sig(SIGKILL, c->gc_task, 1);
	}
	spin_unlock_bh(&c->erase_completion_lock);
	wait_for_completion(&c->gc_thread_exit);
}

static int jffs2_garbage_collect_thread(void *_c)
{
	struct jffs2_sb_info *c = _c;

	daemonize();
	current->tty = NULL;
	c->gc_task = current;
	up(&c->gc_thread_start);

        sprintf(current->comm, "jffs2_gcd_mtd%d", c->mtd->index);

	/* FIXME in the 2.2 backport */
	current->nice = 10;

	for (;;) {
		spin_lock_irq(&current->sigmask_lock);
		siginitsetinv (&current->blocked, sigmask(SIGHUP) | sigmask(SIGKILL) | sigmask(SIGSTOP) | sigmask(SIGCONT));
		recalc_sigpending(current);
		spin_unlock_irq(&current->sigmask_lock);

		if (!thread_should_wake(c)) {
                        set_current_state (TASK_INTERRUPTIBLE);
			D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread sleeping...\n"));
			/* Yes, there's a race here; we checked thread_should_wake() before
			   setting current->state to TASK_INTERRUPTIBLE. But it doesn't
			   matter - We don't care if we miss a wakeup, because the GC thread
			   is only an optimisation anyway. */
			schedule();
		}
                
		if (current->need_resched)
			schedule();

                /* Put_super will send a SIGKILL and then wait on the sem. 
                 */
                while (signal_pending(current)) {
                        siginfo_t info;
                        unsigned long signr;

                        spin_lock_irq(&current->sigmask_lock);
                        signr = dequeue_signal(&current->blocked, &info);
                        spin_unlock_irq(&current->sigmask_lock);

                        switch(signr) {
                        case SIGSTOP:
                                D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread(): SIGSTOP received.\n"));
                                set_current_state(TASK_STOPPED);
                                schedule();
                                break;

                        case SIGKILL:
                                D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread(): SIGKILL received.\n"));
				spin_lock_bh(&c->erase_completion_lock);
                                c->gc_task = NULL;
				spin_unlock_bh(&c->erase_completion_lock);
				complete_and_exit(&c->gc_thread_exit, 0);

			case SIGHUP:
				D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread(): SIGHUP received.\n"));
				break;
			default:
				D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread(): signal %ld received\n", signr));

                        }
                }
		/* We don't want SIGHUP to interrupt us. STOP and KILL are OK though. */
		spin_lock_irq(&current->sigmask_lock);
		siginitsetinv (&current->blocked, sigmask(SIGKILL) | sigmask(SIGSTOP) | sigmask(SIGCONT));
		recalc_sigpending(current);
		spin_unlock_irq(&current->sigmask_lock);

		D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread(): pass\n"));
		jffs2_garbage_collect_pass(c);
	}
}

static int thread_should_wake(struct jffs2_sb_info *c)
{
	D1(printk(KERN_DEBUG "thread_should_wake(): nr_free_blocks %d, nr_erasing_blocks %d, dirty_size 0x%x\n", 
		  c->nr_free_blocks, c->nr_erasing_blocks, c->dirty_size));
	if (c->nr_free_blocks + c->nr_erasing_blocks < JFFS2_RESERVED_BLOCKS_GCTRIGGER &&
	    c->dirty_size > c->sector_size)
		return 1;
	else 
		return 0;
}
