/*-
 * Copyright (c) 1986, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_shutdown.c	8.3 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_kdb.h"
#include "opt_mac.h"
#include "opt_panic.h"
#include "opt_show_busybufs.h"
#include "opt_sched.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/eventhandler.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/smp.h>		/* smp_active */
#include <sys/sysctl.h>
#include <sys/sysproto.h>

#include <machine/cpu.h>
#include <machine/pcb.h>
#include <machine/smp.h>

#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/swap_pager.h>

#include <sys/signalvar.h>

#ifndef PANIC_REBOOT_WAIT_TIME
#define PANIC_REBOOT_WAIT_TIME 15 /* default to 15 seconds */
#endif

/*
 * Note that stdarg.h and the ANSI style va_start macro is used for both
 * ANSI and traditional C compilers.
 */
#include <machine/stdarg.h>

#ifdef KDB
#ifdef KDB_UNATTENDED
int debugger_on_panic = 0;
#else
int debugger_on_panic = 1;
#endif
SYSCTL_INT(_debug, OID_AUTO, debugger_on_panic, CTLFLAG_RW,
	&debugger_on_panic, 0, "Run debugger on kernel panic");

#ifdef KDB_TRACE
int trace_on_panic = 1;
#else
int trace_on_panic = 0;
#endif
SYSCTL_INT(_debug, OID_AUTO, trace_on_panic, CTLFLAG_RW,
	&trace_on_panic, 0, "Print stack trace on kernel panic");
#endif /* KDB */

int sync_on_panic = 0;
SYSCTL_INT(_kern, OID_AUTO, sync_on_panic, CTLFLAG_RW,
	&sync_on_panic, 0, "Do a sync before rebooting from a panic");

SYSCTL_NODE(_kern, OID_AUTO, shutdown, CTLFLAG_RW, 0, "Shutdown environment");

/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
const char *panicstr;

int dumping;				/* system is dumping */
int rebooting;				/* system is rebooting */
static struct dumperinfo dumper;	/* our selected dumper */

/* Context information for dump-debuggers. */
static struct pcb dumppcb;		/* Registers. */
static lwpid_t dumptid;			/* Thread ID. */

static void boot(int) __dead2;
static void poweroff_wait(void *, int);
static void shutdown_halt(void *junk, int howto);
static void shutdown_panic(void *junk, int howto);
static void shutdown_reset(void *junk, int howto);

/* register various local shutdown events */
static void
shutdown_conf(void *unused)
{

	EVENTHANDLER_REGISTER(shutdown_final, poweroff_wait, NULL,
	    SHUTDOWN_PRI_FIRST);
	EVENTHANDLER_REGISTER(shutdown_final, shutdown_halt, NULL,
	    SHUTDOWN_PRI_LAST + 100);
	EVENTHANDLER_REGISTER(shutdown_final, shutdown_panic, NULL,
	    SHUTDOWN_PRI_LAST + 100);
	EVENTHANDLER_REGISTER(shutdown_final, shutdown_reset, NULL,
	    SHUTDOWN_PRI_LAST + 200);
}

SYSINIT(shutdown_conf, SI_SUB_INTRINSIC, SI_ORDER_ANY, shutdown_conf, NULL)

/*
 * The system call that results in a reboot.
 */
/* ARGSUSED */
int
reboot(struct thread *td, struct reboot_args *uap)
{
	int error;

	error = 0;
#ifdef MAC
	error = mac_system_check_reboot(td->td_ucred, uap->opt);
#endif
	if (error == 0)
		error = priv_check(td, PRIV_REBOOT);
	if (error == 0) {
		mtx_lock(&Giant);
		boot(uap->opt);
		mtx_unlock(&Giant);
	}
	return (error);
}

/*
 * Called by events that want to shut down.. e.g  <CTL><ALT><DEL> on a PC
 */
static int shutdown_howto = 0;

void
shutdown_nice(int howto)
{

	shutdown_howto = howto;

	/* Send a signal to init(8) and have it shutdown the world */
	if (initproc != NULL) {
		PROC_LOCK(initproc);
		psignal(initproc, SIGINT);
		PROC_UNLOCK(initproc);
	} else {
		/* No init(8) running, so simply reboot */
		boot(RB_NOSYNC);
	}
	return;
}
static int	waittime = -1;

static void
print_uptime(void)
{
	int f;
	struct timespec ts;

	getnanouptime(&ts);
	printf("Uptime: ");
	f = 0;
	if (ts.tv_sec >= 86400) {
		printf("%ldd", (long)ts.tv_sec / 86400);
		ts.tv_sec %= 86400;
		f = 1;
	}
	if (f || ts.tv_sec >= 3600) {
		printf("%ldh", (long)ts.tv_sec / 3600);
		ts.tv_sec %= 3600;
		f = 1;
	}
	if (f || ts.tv_sec >= 60) {
		printf("%ldm", (long)ts.tv_sec / 60);
		ts.tv_sec %= 60;
		f = 1;
	}
	printf("%lds\n", (long)ts.tv_sec);
}

static void
doadump(void)
{

	/*
	 * Sometimes people have to call this from the kernel debugger. 
	 * (if 'panic' can not dump)
	 * Give them a clue as to why they can't dump.
	 */
	if (dumper.dumper == NULL) {
		printf("Cannot dump. No dump device defined.\n");
		return;
	}

	savectx(&dumppcb);
	dumptid = curthread->td_tid;
	dumping++;
	dumpsys(&dumper);
}

static int
isbufbusy(struct buf *bp)
{
	if (((bp->b_flags & (B_INVAL | B_PERSISTENT)) == 0 &&
	    BUF_REFCNT(bp) > 0) ||
	    ((bp->b_flags & (B_DELWRI | B_INVAL)) == B_DELWRI))
		return (1);
	return (0);
}

/*
 * Shutdown the system cleanly to prepare for reboot, halt, or power off.
 */
static void
boot(int howto)
{
	static int first_buf_printf = 1;

#if defined(SMP)
	/*
	 * Bind us to CPU 0 so that all shutdown code runs there.  Some
	 * systems don't shutdown properly (i.e., ACPI power off) if we
	 * run on another processor.
	 */
	thread_lock(curthread);
	sched_bind(curthread, 0);
	thread_unlock(curthread);
	KASSERT(PCPU_GET(cpuid) == 0, ("boot: not running on cpu 0"));
#endif
	/* We're in the process of rebooting. */
	rebooting = 1;

	/* collect extra flags that shutdown_nice might have set */
	howto |= shutdown_howto;

	/* We are out of the debugger now. */
	kdb_active = 0;

	/*
	 * Do any callouts that should be done BEFORE syncing the filesystems.
	 */
	EVENTHANDLER_INVOKE(shutdown_pre_sync, howto);

	/* 
	 * Now sync filesystems
	 */
	if (!cold && (howto & RB_NOSYNC) == 0 && waittime < 0) {
		register struct buf *bp;
		int iter, nbusy, pbusy;
#ifndef PREEMPTION
		int subiter;
#endif

		waittime = 0;

		sync(curthread, NULL);

		/*
		 * With soft updates, some buffers that are
		 * written will be remarked as dirty until other
		 * buffers are written.
		 */
		for (iter = pbusy = 0; iter < 20; iter++) {
			nbusy = 0;
			for (bp = &buf[nbuf]; --bp >= buf; )
				if (isbufbusy(bp))
					nbusy++;
			if (nbusy == 0) {
				if (first_buf_printf)
					printf("All buffers synced.");
				break;
			}
			if (first_buf_printf) {
				printf("Syncing disks, buffers remaining... ");
				first_buf_printf = 0;
			}
			printf("%d ", nbusy);
			if (nbusy < pbusy)
				iter = 0;
			pbusy = nbusy;
			sync(curthread, NULL);

#ifdef PREEMPTION
			/*
			 * Drop Giant and spin for a while to allow
			 * interrupt threads to run.
			 */
			DROP_GIANT();
			DELAY(50000 * iter);
			PICKUP_GIANT();
#else
			/*
			 * Drop Giant and context switch several times to
			 * allow interrupt threads to run.
			 */
			DROP_GIANT();
			for (subiter = 0; subiter < 50 * iter; subiter++) {
				thread_lock(curthread);
				mi_switch(SW_VOL, NULL);
				thread_unlock(curthread);
				DELAY(1000);
			}
			PICKUP_GIANT();
#endif
		}
		printf("\n");
		/*
		 * Count only busy local buffers to prevent forcing 
		 * a fsck if we're just a client of a wedged NFS server
		 */
		nbusy = 0;
		for (bp = &buf[nbuf]; --bp >= buf; ) {
			if (isbufbusy(bp)) {
#if 0
/* XXX: This is bogus.  We should probably have a BO_REMOTE flag instead */
				if (bp->b_dev == NULL) {
					TAILQ_REMOVE(&mountlist,
					    bp->b_vp->v_mount, mnt_list);
					continue;
				}
#endif
				nbusy++;
#if defined(SHOW_BUSYBUFS) || defined(DIAGNOSTIC)
				printf(
			    "%d: bufobj:%p, flags:%0x, blkno:%ld, lblkno:%ld\n",
				    nbusy, bp->b_bufobj,
				    bp->b_flags, (long)bp->b_blkno,
				    (long)bp->b_lblkno);
#endif
			}
		}
		if (nbusy) {
			/*
			 * Failed to sync all blocks. Indicate this and don't
			 * unmount filesystems (thus forcing an fsck on reboot).
			 */
			printf("Giving up on %d buffers\n", nbusy);
			DELAY(5000000);	/* 5 seconds */
		} else {
			if (!first_buf_printf)
				printf("Final sync complete\n");
			/*
			 * Unmount filesystems
			 */
			if (panicstr == 0)
				vfs_unmountall();
		}
		swapoff_all();
		DELAY(100000);		/* wait for console output to finish */
	}

	print_uptime();

	/*
	 * Ok, now do things that assume all filesystem activity has
	 * been completed.
	 */
	EVENTHANDLER_INVOKE(shutdown_post_sync, howto);

	/* XXX This doesn't disable interrupts any more.  Reconsider? */
	splhigh();

	if ((howto & (RB_HALT|RB_DUMP)) == RB_DUMP && !cold && !dumping) 
		doadump();

	/* Now that we're going to really halt the system... */
	EVENTHANDLER_INVOKE(shutdown_final, howto);

	for(;;) ;	/* safety against shutdown_reset not working */
	/* NOTREACHED */
}

/*
 * If the shutdown was a clean halt, behave accordingly.
 */
static void
shutdown_halt(void *junk, int howto)
{

	if (howto & RB_HALT) {
		printf("\n");
		printf("The operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");
		switch (cngetc()) {
		case -1:		/* No console, just die */
			cpu_halt();
			/* NOTREACHED */
		default:
			howto &= ~RB_HALT;
			break;
		}
	}
}

/*
 * Check to see if the system paniced, pause and then reboot
 * according to the specified delay.
 */
static void
shutdown_panic(void *junk, int howto)
{
	int loop;

	if (howto & RB_DUMP) {
		if (PANIC_REBOOT_WAIT_TIME != 0) {
			if (PANIC_REBOOT_WAIT_TIME != -1) {
				printf("Automatic reboot in %d seconds - "
				       "press a key on the console to abort\n",
					PANIC_REBOOT_WAIT_TIME);
				for (loop = PANIC_REBOOT_WAIT_TIME * 10;
				     loop > 0; --loop) {
					DELAY(1000 * 100); /* 1/10th second */
					/* Did user type a key? */
					if (cncheckc() != -1)
						break;
				}
				if (!loop)
					return;
			}
		} else { /* zero time specified - reboot NOW */
			return;
		}
		printf("--> Press a key on the console to reboot,\n");
		printf("--> or switch off the system now.\n");
		cngetc();
	}
}

/*
 * Everything done, now reset
 */
static void
shutdown_reset(void *junk, int howto)
{

	printf("Rebooting...\n");
	DELAY(1000000);	/* wait 1 sec for printf's to complete and be read */
	/* cpu_boot(howto); */ /* doesn't do anything at the moment */
	cpu_reset();
	/* NOTREACHED */ /* assuming reset worked */
}

#ifdef SMP
static u_int panic_cpu = NOCPU;
#endif

/*
 * Panic is called on unresolvable fatal errors.  It prints "panic: mesg",
 * and then reboots.  If we are called twice, then we avoid trying to sync
 * the disks as this often leads to recursive panics.
 */
void
panic(const char *fmt, ...)
{
	struct thread *td = curthread;
	int bootopt, newpanic;
	va_list ap;
	static char buf[256];

#ifdef SMP
	/*
	 * We don't want multiple CPU's to panic at the same time, so we
	 * use panic_cpu as a simple spinlock.  We have to keep checking
	 * panic_cpu if we are spinning in case the panic on the first
	 * CPU is canceled.
	 */
	if (panic_cpu != PCPU_GET(cpuid))
		while (atomic_cmpset_int(&panic_cpu, NOCPU,
		    PCPU_GET(cpuid)) == 0)
			while (panic_cpu != NOCPU)
				; /* nothing */
#endif

	bootopt = RB_AUTOBOOT | RB_DUMP;
	newpanic = 0;
	if (panicstr)
		bootopt |= RB_NOSYNC;
	else {
		panicstr = fmt;
		newpanic = 1;
	}

	va_start(ap, fmt);
	if (newpanic) {
		(void)vsnprintf(buf, sizeof(buf), fmt, ap);
		panicstr = buf;
		printf("panic: %s\n", buf);
	} else {
		printf("panic: ");
		vprintf(fmt, ap);
		printf("\n");
	}
	va_end(ap);
#ifdef SMP
	printf("cpuid = %d\n", PCPU_GET(cpuid));
#endif

#ifdef KDB
	if (newpanic && trace_on_panic)
		kdb_backtrace();
	if (debugger_on_panic)
		kdb_enter("panic");
#ifdef RESTARTABLE_PANICS
	/* See if the user aborted the panic, in which case we continue. */
	if (panicstr == NULL) {
#ifdef SMP
		atomic_store_rel_int(&panic_cpu, NOCPU);
#endif
		return;
	}
#endif
#endif
	/*thread_lock(td); */
	td->td_flags |= TDF_INPANIC;
	/* thread_unlock(td); */
	if (!sync_on_panic)
		bootopt |= RB_NOSYNC;
	boot(bootopt);
}

/*
 * Support for poweroff delay.
 */
#ifndef POWEROFF_DELAY
# define POWEROFF_DELAY 5000
#endif
static int poweroff_delay = POWEROFF_DELAY;

SYSCTL_INT(_kern_shutdown, OID_AUTO, poweroff_delay, CTLFLAG_RW,
	&poweroff_delay, 0, "");

static void
poweroff_wait(void *junk, int howto)
{

	if (!(howto & RB_POWEROFF) || poweroff_delay <= 0)
		return;
	DELAY(poweroff_delay * 1000);
}

/*
 * Some system processes (e.g. syncer) need to be stopped at appropriate
 * points in their main loops prior to a system shutdown, so that they
 * won't interfere with the shutdown process (e.g. by holding a disk buf
 * to cause sync to fail).  For each of these system processes, register
 * shutdown_kproc() as a handler for one of shutdown events.
 */
static int kproc_shutdown_wait = 60;
SYSCTL_INT(_kern_shutdown, OID_AUTO, kproc_shutdown_wait, CTLFLAG_RW,
    &kproc_shutdown_wait, 0, "");

void
kproc_shutdown(void *arg, int howto)
{
	struct proc *p;
	char procname[MAXCOMLEN + 1];
	int error;

	if (panicstr)
		return;

	p = (struct proc *)arg;
	strlcpy(procname, p->p_comm, sizeof(procname));
	printf("Waiting (max %d seconds) for system process `%s' to stop...",
	    kproc_shutdown_wait, procname);
	error = kproc_suspend(p, kproc_shutdown_wait * hz);

	if (error == EWOULDBLOCK)
		printf("timed out\n");
	else
		printf("done\n");
}

/* Registration of dumpers */
int
set_dumper(struct dumperinfo *di)
{

	if (di == NULL) {
		bzero(&dumper, sizeof dumper);
		return (0);
	}
	if (dumper.dumper != NULL)
		return (EBUSY);
	dumper = *di;
	return (0);
}

#if defined(__powerpc__)
void
dumpsys(struct dumperinfo *di __unused)
{

	printf("Kernel dumps not implemented on this architecture\n");
}
#endif
