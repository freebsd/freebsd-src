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

#include "opt_ddb.h"
#include "opt_kdb.h"
#include "opt_panic.h"
#include "opt_sched.h"
#include "opt_watchdog.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/eventhandler.h>
#include <sys/jail.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/kerneldump.h>
#include <sys/kthread.h>
#include <sys/ktr.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/resourcevar.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/vnode.h>
#include <sys/watchdog.h>

#include <ddb/ddb.h>

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
int panic_reboot_wait_time = PANIC_REBOOT_WAIT_TIME;
SYSCTL_INT(_kern, OID_AUTO, panic_reboot_wait_time, CTLFLAG_RW | CTLFLAG_TUN,
    &panic_reboot_wait_time, 0,
    "Seconds to wait before rebooting after a panic");
TUNABLE_INT("kern.panic_reboot_wait_time", &panic_reboot_wait_time);

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
SYSCTL_INT(_debug, OID_AUTO, debugger_on_panic,
    CTLFLAG_RW | CTLFLAG_SECURE | CTLFLAG_TUN,
    &debugger_on_panic, 0, "Run debugger on kernel panic");
TUNABLE_INT("debug.debugger_on_panic", &debugger_on_panic);

#ifdef KDB_TRACE
static int trace_on_panic = 1;
#else
static int trace_on_panic = 0;
#endif
SYSCTL_INT(_debug, OID_AUTO, trace_on_panic,
    CTLFLAG_RW | CTLFLAG_SECURE | CTLFLAG_TUN,
    &trace_on_panic, 0, "Print stack trace on kernel panic");
TUNABLE_INT("debug.trace_on_panic", &trace_on_panic);
#endif /* KDB */

static int sync_on_panic = 0;
SYSCTL_INT(_kern, OID_AUTO, sync_on_panic, CTLFLAG_RW | CTLFLAG_TUN,
	&sync_on_panic, 0, "Do a sync before rebooting from a panic");
TUNABLE_INT("kern.sync_on_panic", &sync_on_panic);

static SYSCTL_NODE(_kern, OID_AUTO, shutdown, CTLFLAG_RW, 0,
    "Shutdown environment");

#ifndef DIAGNOSTIC
static int show_busybufs;
#else
static int show_busybufs = 1;
#endif
SYSCTL_INT(_kern_shutdown, OID_AUTO, show_busybufs, CTLFLAG_RW,
	&show_busybufs, 0, "");

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
lwpid_t dumptid;			/* Thread ID. */

static void poweroff_wait(void *, int);
static void shutdown_halt(void *junk, int howto);
static void shutdown_panic(void *junk, int howto);
static void shutdown_reset(void *junk, int howto);
static void vpanic(const char *fmt, va_list ap) __dead2;

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

SYSINIT(shutdown_conf, SI_SUB_INTRINSIC, SI_ORDER_ANY, shutdown_conf, NULL);

/*
 * The system call that results in a reboot.
 */
/* ARGSUSED */
int
sys_reboot(struct thread *td, struct reboot_args *uap)
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
		kern_reboot(uap->opt);
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
		kern_psignal(initproc, SIGINT);
		PROC_UNLOCK(initproc);
	} else {
		/* No init(8) running, so simply reboot */
		kern_reboot(RB_NOSYNC);
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

int
doadump(boolean_t textdump)
{
	boolean_t coredump;

	if (dumping)
		return (EBUSY);
	if (dumper.dumper == NULL)
		return (ENXIO);

	savectx(&dumppcb);
	dumptid = curthread->td_tid;
	dumping++;

	coredump = TRUE;
#ifdef DDB
	if (textdump && textdump_pending) {
		coredump = FALSE;
		textdump_dumpsys(&dumper);
	}
#endif
	if (coredump)
		dumpsys(&dumper);

	dumping--;
	return (0);
}

static int
isbufbusy(struct buf *bp)
{
	if (((bp->b_flags & (B_INVAL | B_PERSISTENT)) == 0 &&
	    BUF_ISLOCKED(bp)) ||
	    ((bp->b_flags & (B_DELWRI | B_INVAL)) == B_DELWRI))
		return (1);
	return (0);
}

/*
 * Shutdown the system cleanly to prepare for reboot, halt, or power off.
 */
void
kern_reboot(int howto)
{
	static int first_buf_printf = 1;

#if defined(SMP)
	/*
	 * Bind us to CPU 0 so that all shutdown code runs there.  Some
	 * systems don't shutdown properly (i.e., ACPI power off) if we
	 * run on another processor.
	 */
	if (!SCHEDULER_STOPPED()) {
		thread_lock(curthread);
		sched_bind(curthread, 0);
		thread_unlock(curthread);
		KASSERT(PCPU_GET(cpuid) == 0, ("boot: not running on cpu 0"));
	}
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

		wdog_kern_pat(WD_LASTVAL);
		sys_sync(curthread, NULL);

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

			wdog_kern_pat(WD_LASTVAL);
			sys_sync(curthread, NULL);

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
				if (show_busybufs > 0) {
					printf(
	    "%d: buf:%p, vnode:%p, flags:%0x, blkno:%jd, lblkno:%jd, buflock:",
					    nbusy, bp, bp->b_vp, bp->b_flags,
					    (intmax_t)bp->b_blkno,
					    (intmax_t)bp->b_lblkno);
					BUF_LOCKPRINTINFO(bp);
					if (show_busybufs > 1)
						vn_printf(bp->b_vp,
						    "vnode content: ");
				}
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

	cngrab();

	/*
	 * Ok, now do things that assume all filesystem activity has
	 * been completed.
	 */
	EVENTHANDLER_INVOKE(shutdown_post_sync, howto);

	if ((howto & (RB_HALT|RB_DUMP)) == RB_DUMP && !cold && !dumping) 
		doadump(TRUE);

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
		if (panic_reboot_wait_time != 0) {
			if (panic_reboot_wait_time != -1) {
				printf("Automatic reboot in %d seconds - "
				       "press a key on the console to abort\n",
					panic_reboot_wait_time);
				for (loop = panic_reboot_wait_time * 10;
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

	/*
	 * Acquiring smp_ipi_mtx here has a double effect:
	 * - it disables interrupts avoiding CPU0 preemption
	 *   by fast handlers (thus deadlocking  against other CPUs)
	 * - it avoids deadlocks against smp_rendezvous() or, more 
	 *   generally, threads busy-waiting, with this spinlock held,
	 *   and waiting for responses by threads on other CPUs
	 *   (ie. smp_tlb_shootdown()).
	 *
	 * For the !SMP case it just needs to handle the former problem.
	 */
#ifdef SMP
	mtx_lock_spin(&smp_ipi_mtx);
#else
	spinlock_enter();
#endif

	/* cpu_boot(howto); */ /* doesn't do anything at the moment */
	cpu_reset();
	/* NOTREACHED */ /* assuming reset worked */
}

#if defined(WITNESS) || defined(INVARIANTS)
static int kassert_warn_only = 0;
#ifdef KDB
static int kassert_do_kdb = 0;
#endif
#ifdef KTR
static int kassert_do_ktr = 0;
#endif
static int kassert_do_log = 1;
static int kassert_log_pps_limit = 4;
static int kassert_log_mute_at = 0;
static int kassert_log_panic_at = 0;
static int kassert_warnings = 0;

SYSCTL_NODE(_debug, OID_AUTO, kassert, CTLFLAG_RW, NULL, "kassert options");

SYSCTL_INT(_debug_kassert, OID_AUTO, warn_only, CTLFLAG_RW | CTLFLAG_TUN,
    &kassert_warn_only, 0,
    "KASSERT triggers a panic (1) or just a warning (0)");
TUNABLE_INT("debug.kassert.warn_only", &kassert_warn_only);

#ifdef KDB
SYSCTL_INT(_debug_kassert, OID_AUTO, do_kdb, CTLFLAG_RW | CTLFLAG_TUN,
    &kassert_do_kdb, 0, "KASSERT will enter the debugger");
TUNABLE_INT("debug.kassert.do_kdb", &kassert_do_kdb);
#endif

#ifdef KTR
SYSCTL_UINT(_debug_kassert, OID_AUTO, do_ktr, CTLFLAG_RW | CTLFLAG_TUN,
    &kassert_do_ktr, 0,
    "KASSERT does a KTR, set this to the KTRMASK you want");
TUNABLE_INT("debug.kassert.do_ktr", &kassert_do_ktr);
#endif

SYSCTL_INT(_debug_kassert, OID_AUTO, do_log, CTLFLAG_RW | CTLFLAG_TUN,
    &kassert_do_log, 0, "KASSERT triggers a panic (1) or just a warning (0)");
TUNABLE_INT("debug.kassert.do_log", &kassert_do_log);

SYSCTL_INT(_debug_kassert, OID_AUTO, warnings, CTLFLAG_RW | CTLFLAG_TUN,
    &kassert_warnings, 0, "number of KASSERTs that have been triggered");
TUNABLE_INT("debug.kassert.warnings", &kassert_warnings);

SYSCTL_INT(_debug_kassert, OID_AUTO, log_panic_at, CTLFLAG_RW | CTLFLAG_TUN,
    &kassert_log_panic_at, 0, "max number of KASSERTS before we will panic");
TUNABLE_INT("debug.kassert.log_panic_at", &kassert_log_panic_at);

SYSCTL_INT(_debug_kassert, OID_AUTO, log_pps_limit, CTLFLAG_RW | CTLFLAG_TUN,
    &kassert_log_pps_limit, 0, "limit number of log messages per second");
TUNABLE_INT("debug.kassert.log_pps_limit", &kassert_log_pps_limit);

SYSCTL_INT(_debug_kassert, OID_AUTO, log_mute_at, CTLFLAG_RW | CTLFLAG_TUN,
    &kassert_log_mute_at, 0, "max number of KASSERTS to log");
TUNABLE_INT("debug.kassert.log_mute_at", &kassert_log_mute_at);

static int kassert_sysctl_kassert(SYSCTL_HANDLER_ARGS);

SYSCTL_PROC(_debug_kassert, OID_AUTO, kassert,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_SECURE, NULL, 0,
    kassert_sysctl_kassert, "I", "set to trigger a test kassert");

static int
kassert_sysctl_kassert(SYSCTL_HANDLER_ARGS)
{
	int error, i;

	error = sysctl_wire_old_buffer(req, sizeof(int));
	if (error == 0) {
		i = 0;
		error = sysctl_handle_int(oidp, &i, 0, req);
	}
	if (error != 0 || req->newptr == NULL)
		return (error);
	KASSERT(0, ("kassert_sysctl_kassert triggered kassert %d", i));
	return (0);
}

/*
 * Called by KASSERT, this decides if we will panic
 * or if we will log via printf and/or ktr.
 */
void
kassert_panic(const char *fmt, ...)
{
	static char buf[256];
	va_list ap;

	va_start(ap, fmt);
	(void)vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	/*
	 * panic if we're not just warning, or if we've exceeded
	 * kassert_log_panic_at warnings.
	 */
	if (!kassert_warn_only ||
	    (kassert_log_panic_at > 0 &&
	     kassert_warnings >= kassert_log_panic_at)) {
		va_start(ap, fmt);
		vpanic(fmt, ap);
		/* NORETURN */
	}
#ifdef KTR
	if (kassert_do_ktr)
		CTR0(ktr_mask, buf);
#endif /* KTR */
	/*
	 * log if we've not yet met the mute limit.
	 */
	if (kassert_do_log &&
	    (kassert_log_mute_at == 0 ||
	     kassert_warnings < kassert_log_mute_at)) {
		static  struct timeval lasterr;
		static  int curerr;

		if (ppsratecheck(&lasterr, &curerr, kassert_log_pps_limit)) {
			printf("KASSERT failed: %s\n", buf);
			kdb_backtrace();
		}
	}
#ifdef KDB
	if (kassert_do_kdb) {
		kdb_enter(KDB_WHY_KASSERT, buf);
	}
#endif
	atomic_add_int(&kassert_warnings, 1);
}
#endif

/*
 * Panic is called on unresolvable fatal errors.  It prints "panic: mesg",
 * and then reboots.  If we are called twice, then we avoid trying to sync
 * the disks as this often leads to recursive panics.
 */
void
panic(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vpanic(fmt, ap);
}

static void
vpanic(const char *fmt, va_list ap)
{
#ifdef SMP
	cpuset_t other_cpus;
#endif
	struct thread *td = curthread;
	int bootopt, newpanic;
	static char buf[256];

	spinlock_enter();

#ifdef SMP
	/*
	 * stop_cpus_hard(other_cpus) should prevent multiple CPUs from
	 * concurrently entering panic.  Only the winner will proceed
	 * further.
	 */
	if (panicstr == NULL && !kdb_active) {
		other_cpus = all_cpus;
		CPU_CLR(PCPU_GET(cpuid), &other_cpus);
		stop_cpus_hard(other_cpus);
	}

	/*
	 * We set stop_scheduler here and not in the block above,
	 * because we want to ensure that if panic has been called and
	 * stop_scheduler_on_panic is true, then stop_scheduler will
	 * always be set.  Even if panic has been entered from kdb.
	 */
	td->td_stopsched = 1;
#endif

	bootopt = RB_AUTOBOOT;
	newpanic = 0;
	if (panicstr)
		bootopt |= RB_NOSYNC;
	else {
		bootopt |= RB_DUMP;
		panicstr = fmt;
		newpanic = 1;
	}

	if (newpanic) {
		(void)vsnprintf(buf, sizeof(buf), fmt, ap);
		panicstr = buf;
		cngrab();
		printf("panic: %s\n", buf);
	} else {
		printf("panic: ");
		vprintf(fmt, ap);
		printf("\n");
	}
#ifdef SMP
	printf("cpuid = %d\n", PCPU_GET(cpuid));
#endif

#ifdef KDB
	if (newpanic && trace_on_panic)
		kdb_backtrace();
	if (debugger_on_panic)
		kdb_enter(KDB_WHY_PANIC, "panic");
#endif
	/*thread_lock(td); */
	td->td_flags |= TDF_INPANIC;
	/* thread_unlock(td); */
	if (!sync_on_panic)
		bootopt |= RB_NOSYNC;
	kern_reboot(bootopt);
}

/*
 * Support for poweroff delay.
 *
 * Please note that setting this delay too short might power off your machine
 * before the write cache on your hard disk has been flushed, leading to
 * soft-updates inconsistencies.
 */
#ifndef POWEROFF_DELAY
# define POWEROFF_DELAY 5000
#endif
static int poweroff_delay = POWEROFF_DELAY;

SYSCTL_INT(_kern_shutdown, OID_AUTO, poweroff_delay, CTLFLAG_RW,
    &poweroff_delay, 0, "Delay before poweroff to write disk caches (msec)");

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
    &kproc_shutdown_wait, 0, "Max wait time (sec) to stop for each process");

void
kproc_shutdown(void *arg, int howto)
{
	struct proc *p;
	int error;

	if (panicstr)
		return;

	p = (struct proc *)arg;
	printf("Waiting (max %d seconds) for system process `%s' to stop...",
	    kproc_shutdown_wait, p->p_comm);
	error = kproc_suspend(p, kproc_shutdown_wait * hz);

	if (error == EWOULDBLOCK)
		printf("timed out\n");
	else
		printf("done\n");
}

void
kthread_shutdown(void *arg, int howto)
{
	struct thread *td;
	int error;

	if (panicstr)
		return;

	td = (struct thread *)arg;
	printf("Waiting (max %d seconds) for system thread `%s' to stop...",
	    kproc_shutdown_wait, td->td_name);
	error = kthread_suspend(td, kproc_shutdown_wait * hz);

	if (error == EWOULDBLOCK)
		printf("timed out\n");
	else
		printf("done\n");
}

static char dumpdevname[sizeof(((struct cdev*)NULL)->si_name)];
SYSCTL_STRING(_kern_shutdown, OID_AUTO, dumpdevname, CTLFLAG_RD,
    dumpdevname, 0, "Device for kernel dumps");

/* Registration of dumpers */
int
set_dumper(struct dumperinfo *di, const char *devname)
{
	size_t wantcopy;

	if (di == NULL) {
		bzero(&dumper, sizeof dumper);
		dumpdevname[0] = '\0';
		return (0);
	}
	if (dumper.dumper != NULL)
		return (EBUSY);
	dumper = *di;
	wantcopy = strlcpy(dumpdevname, devname, sizeof(dumpdevname));
	if (wantcopy >= sizeof(dumpdevname)) {
		printf("set_dumper: device name truncated from '%s' -> '%s'\n",
			devname, dumpdevname);
	}
	return (0);
}

/* Call dumper with bounds checking. */
int
dump_write(struct dumperinfo *di, void *virtual, vm_offset_t physical,
    off_t offset, size_t length)
{

	if (length != 0 && (offset < di->mediaoffset ||
	    offset - di->mediaoffset + length > di->mediasize)) {
		printf("Attempt to write outside dump device boundaries.\n"
	    "offset(%jd), mediaoffset(%jd), length(%ju), mediasize(%jd).\n",
		    (intmax_t)offset, (intmax_t)di->mediaoffset,
		    (uintmax_t)length, (intmax_t)di->mediasize);
		return (ENOSPC);
	}
	return (di->dumper(di->priv, virtual, physical, offset, length));
}

void
mkdumpheader(struct kerneldumpheader *kdh, char *magic, uint32_t archver,
    uint64_t dumplen, uint32_t blksz)
{

	bzero(kdh, sizeof(*kdh));
	strncpy(kdh->magic, magic, sizeof(kdh->magic));
	strncpy(kdh->architecture, MACHINE_ARCH, sizeof(kdh->architecture));
	kdh->version = htod32(KERNELDUMPVERSION);
	kdh->architectureversion = htod32(archver);
	kdh->dumplength = htod64(dumplen);
	kdh->dumptime = htod64(time_second);
	kdh->blocksize = htod32(blksz);
	strncpy(kdh->hostname, prison0.pr_hostname, sizeof(kdh->hostname));
	strncpy(kdh->versionstring, version, sizeof(kdh->versionstring));
	if (panicstr != NULL)
		strncpy(kdh->panicstring, panicstr, sizeof(kdh->panicstring));
	kdh->parity = kerneldump_parity(kdh);
}
