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
 * 3. Neither the name of the University nor the names of its contributors
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
#include "opt_ekcd.h"
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
#include <sys/filedesc.h>
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

#include <crypto/rijndael/rijndael-api-fst.h>
#include <crypto/sha2/sha256.h>

#include <ddb/ddb.h>

#include <machine/cpu.h>
#include <machine/dump.h>
#include <machine/pcb.h>
#include <machine/smp.h>

#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/swap_pager.h>

#include <sys/signalvar.h>

static MALLOC_DEFINE(M_DUMPER, "dumper", "dumper block buffer");

#ifndef PANIC_REBOOT_WAIT_TIME
#define PANIC_REBOOT_WAIT_TIME 15 /* default to 15 seconds */
#endif
static int panic_reboot_wait_time = PANIC_REBOOT_WAIT_TIME;
SYSCTL_INT(_kern, OID_AUTO, panic_reboot_wait_time, CTLFLAG_RWTUN,
    &panic_reboot_wait_time, 0,
    "Seconds to wait before rebooting after a panic");

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
    CTLFLAG_RWTUN | CTLFLAG_SECURE,
    &debugger_on_panic, 0, "Run debugger on kernel panic");

#ifdef KDB_TRACE
static int trace_on_panic = 1;
#else
static int trace_on_panic = 0;
#endif
SYSCTL_INT(_debug, OID_AUTO, trace_on_panic,
    CTLFLAG_RWTUN | CTLFLAG_SECURE,
    &trace_on_panic, 0, "Print stack trace on kernel panic");
#endif /* KDB */

static int sync_on_panic = 0;
SYSCTL_INT(_kern, OID_AUTO, sync_on_panic, CTLFLAG_RWTUN,
	&sync_on_panic, 0, "Do a sync before rebooting from a panic");

static SYSCTL_NODE(_kern, OID_AUTO, shutdown, CTLFLAG_RW, 0,
    "Shutdown environment");

#ifndef DIAGNOSTIC
static int show_busybufs;
#else
static int show_busybufs = 1;
#endif
SYSCTL_INT(_kern_shutdown, OID_AUTO, show_busybufs, CTLFLAG_RW,
	&show_busybufs, 0, "");

int suspend_blocked = 0;
SYSCTL_INT(_kern, OID_AUTO, suspend_blocked, CTLFLAG_RW,
	&suspend_blocked, 0, "Block suspend due to a pending shutdown");

#ifdef EKCD
FEATURE(ekcd, "Encrypted kernel crash dumps support");

MALLOC_DEFINE(M_EKCD, "ekcd", "Encrypted kernel crash dumps data");

struct kerneldumpcrypto {
	uint8_t			kdc_encryption;
	uint8_t			kdc_iv[KERNELDUMP_IV_MAX_SIZE];
	keyInstance		kdc_ki;
	cipherInstance		kdc_ci;
	off_t			kdc_nextoffset;
	uint32_t		kdc_dumpkeysize;
	struct kerneldumpkey	kdc_dumpkey[];
};
#endif

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

static struct cdevsw reroot_cdevsw = {
     .d_version = D_VERSION,
     .d_name    = "reroot",
};

static void poweroff_wait(void *, int);
static void shutdown_halt(void *junk, int howto);
static void shutdown_panic(void *junk, int howto);
static void shutdown_reset(void *junk, int howto);
static int kern_reroot(void);

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
 * The only reason this exists is to create the /dev/reroot/ directory,
 * used by reroot code in init(8) as a mountpoint for tmpfs.
 */
static void
reroot_conf(void *unused)
{
	int error;
	struct cdev *cdev;

	error = make_dev_p(MAKEDEV_CHECKNAME | MAKEDEV_WAITOK, &cdev,
	    &reroot_cdevsw, NULL, UID_ROOT, GID_WHEEL, 0600, "reroot/reroot");
	if (error != 0) {
		printf("%s: failed to create device node, error %d",
		    __func__, error);
	}
}

SYSINIT(reroot_conf, SI_SUB_DEVFS, SI_ORDER_ANY, reroot_conf, NULL);

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
		if (uap->opt & RB_REROOT) {
			error = kern_reroot();
		} else {
			mtx_lock(&Giant);
			kern_reboot(uap->opt);
			mtx_unlock(&Giant);
		}
	}
	return (error);
}

/*
 * Called by events that want to shut down.. e.g  <CTL><ALT><DEL> on a PC
 */
void
shutdown_nice(int howto)
{

	if (initproc != NULL) {
		/* Send a signal to init(8) and have it shutdown the world. */
		PROC_LOCK(initproc);
		if (howto & RB_POWEROFF)
			kern_psignal(initproc, SIGUSR2);
		else if (howto & RB_HALT)
			kern_psignal(initproc, SIGUSR1);
		else
			kern_psignal(initproc, SIGINT);
		PROC_UNLOCK(initproc);
	} else {
		/* No init(8) running, so simply reboot. */
		kern_reboot(howto | RB_NOSYNC);
	}
}

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
	int error;

	error = 0;
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
		error = dumpsys(&dumper);

	dumping--;
	return (error);
}

/*
 * Shutdown the system cleanly to prepare for reboot, halt, or power off.
 */
void
kern_reboot(int howto)
{
	static int once = 0;

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

	/* We are out of the debugger now. */
	kdb_active = 0;

	/*
	 * Do any callouts that should be done BEFORE syncing the filesystems.
	 */
	EVENTHANDLER_INVOKE(shutdown_pre_sync, howto);

	/* 
	 * Now sync filesystems
	 */
	if (!cold && (howto & RB_NOSYNC) == 0 && once == 0) {
		once = 1;
		bufshutdown(show_busybufs);
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
 * The system call that results in changing the rootfs.
 */
static int
kern_reroot(void)
{
	struct vnode *oldrootvnode, *vp;
	struct mount *mp, *devmp;
	int error;

	if (curproc != initproc)
		return (EPERM);

	/*
	 * Mark the filesystem containing currently-running executable
	 * (the temporary copy of init(8)) busy.
	 */
	vp = curproc->p_textvp;
	error = vn_lock(vp, LK_SHARED);
	if (error != 0)
		return (error);
	mp = vp->v_mount;
	error = vfs_busy(mp, MBF_NOWAIT);
	if (error != 0) {
		vfs_ref(mp);
		VOP_UNLOCK(vp, 0);
		error = vfs_busy(mp, 0);
		vn_lock(vp, LK_SHARED | LK_RETRY);
		vfs_rel(mp);
		if (error != 0) {
			VOP_UNLOCK(vp, 0);
			return (ENOENT);
		}
		if (vp->v_iflag & VI_DOOMED) {
			VOP_UNLOCK(vp, 0);
			vfs_unbusy(mp);
			return (ENOENT);
		}
	}
	VOP_UNLOCK(vp, 0);

	/*
	 * Remove the filesystem containing currently-running executable
	 * from the mount list, to prevent it from being unmounted
	 * by vfs_unmountall(), and to avoid confusing vfs_mountroot().
	 *
	 * Also preserve /dev - forcibly unmounting it could cause driver
	 * reinitialization.
	 */

	vfs_ref(rootdevmp);
	devmp = rootdevmp;
	rootdevmp = NULL;

	mtx_lock(&mountlist_mtx);
	TAILQ_REMOVE(&mountlist, mp, mnt_list);
	TAILQ_REMOVE(&mountlist, devmp, mnt_list);
	mtx_unlock(&mountlist_mtx);

	oldrootvnode = rootvnode;

	/*
	 * Unmount everything except for the two filesystems preserved above.
	 */
	vfs_unmountall();

	/*
	 * Add /dev back; vfs_mountroot() will move it into its new place.
	 */
	mtx_lock(&mountlist_mtx);
	TAILQ_INSERT_HEAD(&mountlist, devmp, mnt_list);
	mtx_unlock(&mountlist_mtx);
	rootdevmp = devmp;
	vfs_rel(rootdevmp);

	/*
	 * Mount the new rootfs.
	 */
	vfs_mountroot();

	/*
	 * Update all references to the old rootvnode.
	 */
	mountcheckdirs(oldrootvnode, rootvnode);

	/*
	 * Add the temporary filesystem back and unbusy it.
	 */
	mtx_lock(&mountlist_mtx);
	TAILQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	mtx_unlock(&mountlist_mtx);
	vfs_unbusy(mp);

	return (0);
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

#if defined(WITNESS) || defined(INVARIANT_SUPPORT)
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

SYSCTL_INT(_debug_kassert, OID_AUTO, warn_only, CTLFLAG_RWTUN,
    &kassert_warn_only, 0,
    "KASSERT triggers a panic (1) or just a warning (0)");

#ifdef KDB
SYSCTL_INT(_debug_kassert, OID_AUTO, do_kdb, CTLFLAG_RWTUN,
    &kassert_do_kdb, 0, "KASSERT will enter the debugger");
#endif

#ifdef KTR
SYSCTL_UINT(_debug_kassert, OID_AUTO, do_ktr, CTLFLAG_RWTUN,
    &kassert_do_ktr, 0,
    "KASSERT does a KTR, set this to the KTRMASK you want");
#endif

SYSCTL_INT(_debug_kassert, OID_AUTO, do_log, CTLFLAG_RWTUN,
    &kassert_do_log, 0, "KASSERT triggers a panic (1) or just a warning (0)");

SYSCTL_INT(_debug_kassert, OID_AUTO, warnings, CTLFLAG_RWTUN,
    &kassert_warnings, 0, "number of KASSERTs that have been triggered");

SYSCTL_INT(_debug_kassert, OID_AUTO, log_panic_at, CTLFLAG_RWTUN,
    &kassert_log_panic_at, 0, "max number of KASSERTS before we will panic");

SYSCTL_INT(_debug_kassert, OID_AUTO, log_pps_limit, CTLFLAG_RWTUN,
    &kassert_log_pps_limit, 0, "limit number of log messages per second");

SYSCTL_INT(_debug_kassert, OID_AUTO, log_mute_at, CTLFLAG_RWTUN,
    &kassert_log_mute_at, 0, "max number of KASSERTS to log");

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

void
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
#endif

	/*
	 * Ensure that the scheduler is stopped while panicking, even if panic
	 * has been entered from kdb.
	 */
	td->td_stopsched = 1;

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
	printf("time = %jd\n", (intmax_t )time_second);
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
	printf("Waiting (max %d seconds) for system process `%s' to stop... ",
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
	printf("Waiting (max %d seconds) for system thread `%s' to stop... ",
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

#ifdef EKCD
static struct kerneldumpcrypto *
kerneldumpcrypto_create(size_t blocksize, uint8_t encryption,
    const uint8_t *key, uint32_t encryptedkeysize, const uint8_t *encryptedkey)
{
	struct kerneldumpcrypto *kdc;
	struct kerneldumpkey *kdk;
	uint32_t dumpkeysize;

	dumpkeysize = roundup2(sizeof(*kdk) + encryptedkeysize, blocksize);
	kdc = malloc(sizeof(*kdc) + dumpkeysize, M_EKCD, M_WAITOK | M_ZERO);

	arc4rand(kdc->kdc_iv, sizeof(kdc->kdc_iv), 0);

	kdc->kdc_encryption = encryption;
	switch (kdc->kdc_encryption) {
	case KERNELDUMP_ENC_AES_256_CBC:
		if (rijndael_makeKey(&kdc->kdc_ki, DIR_ENCRYPT, 256, key) <= 0)
			goto failed;
		break;
	default:
		goto failed;
	}

	kdc->kdc_dumpkeysize = dumpkeysize;
	kdk = kdc->kdc_dumpkey;
	kdk->kdk_encryption = kdc->kdc_encryption;
	memcpy(kdk->kdk_iv, kdc->kdc_iv, sizeof(kdk->kdk_iv));
	kdk->kdk_encryptedkeysize = htod32(encryptedkeysize);
	memcpy(kdk->kdk_encryptedkey, encryptedkey, encryptedkeysize);

	return (kdc);
failed:
	explicit_bzero(kdc, sizeof(*kdc) + dumpkeysize);
	free(kdc, M_EKCD);
	return (NULL);
}
#endif /* EKCD */

int
kerneldumpcrypto_init(struct kerneldumpcrypto *kdc)
{
#ifndef EKCD
	return (0);
#else
	uint8_t hash[SHA256_DIGEST_LENGTH];
	SHA256_CTX ctx;
	struct kerneldumpkey *kdk;
	int error;

	error = 0;

	if (kdc == NULL)
		return (0);

	/*
	 * When a user enters ddb it can write a crash dump multiple times.
	 * Each time it should be encrypted using a different IV.
	 */
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, kdc->kdc_iv, sizeof(kdc->kdc_iv));
	SHA256_Final(hash, &ctx);
	bcopy(hash, kdc->kdc_iv, sizeof(kdc->kdc_iv));

	switch (kdc->kdc_encryption) {
	case KERNELDUMP_ENC_AES_256_CBC:
		if (rijndael_cipherInit(&kdc->kdc_ci, MODE_CBC,
		    kdc->kdc_iv) <= 0) {
			error = EINVAL;
			goto out;
		}
		break;
	default:
		error = EINVAL;
		goto out;
	}

	kdc->kdc_nextoffset = 0;

	kdk = kdc->kdc_dumpkey;
	memcpy(kdk->kdk_iv, kdc->kdc_iv, sizeof(kdk->kdk_iv));
out:
	explicit_bzero(hash, sizeof(hash));
	return (error);
#endif
}

uint32_t
kerneldumpcrypto_dumpkeysize(const struct kerneldumpcrypto *kdc)
{

#ifdef EKCD
	if (kdc == NULL)
		return (0);
	return (kdc->kdc_dumpkeysize);
#else
	return (0);
#endif
}

/* Registration of dumpers */
int
set_dumper(struct dumperinfo *di, const char *devname, struct thread *td,
    uint8_t encryption, const uint8_t *key, uint32_t encryptedkeysize,
    const uint8_t *encryptedkey)
{
	size_t wantcopy;
	int error;

	error = priv_check(td, PRIV_SETDUMPER);
	if (error != 0)
		return (error);

	if (di == NULL) {
		error = 0;
		goto cleanup;
	}
	if (dumper.dumper != NULL)
		return (EBUSY);
	dumper = *di;
	dumper.blockbuf = NULL;
	dumper.kdc = NULL;

	if (encryption != KERNELDUMP_ENC_NONE) {
#ifdef EKCD
		dumper.kdc = kerneldumpcrypto_create(di->blocksize, encryption,
		    key, encryptedkeysize, encryptedkey);
		if (dumper.kdc == NULL) {
			error = EINVAL;
			goto cleanup;
		}
#else
		error = EOPNOTSUPP;
		goto cleanup;
#endif
	}

	wantcopy = strlcpy(dumpdevname, devname, sizeof(dumpdevname));
	if (wantcopy >= sizeof(dumpdevname)) {
		printf("set_dumper: device name truncated from '%s' -> '%s'\n",
			devname, dumpdevname);
	}

	dumper.blockbuf = malloc(di->blocksize, M_DUMPER, M_WAITOK | M_ZERO);
	return (0);
cleanup:
#ifdef EKCD
	if (dumper.kdc != NULL) {
		explicit_bzero(dumper.kdc, sizeof(*dumper.kdc) +
		    dumper.kdc->kdc_dumpkeysize);
		free(dumper.kdc, M_EKCD);
	}
#endif
	if (dumper.blockbuf != NULL) {
		explicit_bzero(dumper.blockbuf, dumper.blocksize);
		free(dumper.blockbuf, M_DUMPER);
	}
	explicit_bzero(&dumper, sizeof(dumper));
	dumpdevname[0] = '\0';
	return (error);
}

static int
dump_check_bounds(struct dumperinfo *di, off_t offset, size_t length)
{

	if (length != 0 && (offset < di->mediaoffset ||
	    offset - di->mediaoffset + length > di->mediasize)) {
		printf("Attempt to write outside dump device boundaries.\n"
	    "offset(%jd), mediaoffset(%jd), length(%ju), mediasize(%jd).\n",
		    (intmax_t)offset, (intmax_t)di->mediaoffset,
		    (uintmax_t)length, (intmax_t)di->mediasize);
		return (ENOSPC);
	}

	return (0);
}

#ifdef EKCD
static int
dump_encrypt(struct kerneldumpcrypto *kdc, uint8_t *buf, size_t size)
{

	switch (kdc->kdc_encryption) {
	case KERNELDUMP_ENC_AES_256_CBC:
		if (rijndael_blockEncrypt(&kdc->kdc_ci, &kdc->kdc_ki, buf,
		    8 * size, buf) <= 0) {
			return (EIO);
		}
		if (rijndael_cipherInit(&kdc->kdc_ci, MODE_CBC,
		    buf + size - 16 /* IV size for AES-256-CBC */) <= 0) {
			return (EIO);
		}
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

/* Encrypt data and call dumper. */
static int
dump_encrypted_write(struct dumperinfo *di, void *virtual, vm_offset_t physical,
    off_t offset, size_t length)
{
	static uint8_t buf[KERNELDUMP_BUFFER_SIZE];
	struct kerneldumpcrypto *kdc;
	int error;
	size_t nbytes;
	off_t nextoffset;

	kdc = di->kdc;

	error = dump_check_bounds(di, offset, length);
	if (error != 0)
		return (error);

	/* Signal completion. */
	if (virtual == NULL && physical == 0 && offset == 0 && length == 0) {
		return (di->dumper(di->priv, virtual, physical, offset,
		    length));
	}

	/* Data have to be aligned to block size. */
	if ((length % di->blocksize) != 0)
		return (EINVAL);

	/*
	 * Data have to be written continuously becase we're encrypting using
	 * CBC mode which has this assumption.
	 */
	if (kdc->kdc_nextoffset != 0 && kdc->kdc_nextoffset != offset)
		return (EINVAL);

	nextoffset = offset + (off_t)length;

	while (length > 0) {
		nbytes = MIN(length, sizeof(buf));
		bcopy(virtual, buf, nbytes);

		if (dump_encrypt(kdc, buf, nbytes) != 0)
			return (EIO);

		error = di->dumper(di->priv, buf, physical, offset, nbytes);
		if (error != 0)
			return (error);

		offset += nbytes;
		virtual = (void *)((uint8_t *)virtual + nbytes);
		length -= nbytes;
	}

	kdc->kdc_nextoffset = nextoffset;

	return (0);
}
#endif /* EKCD */

/* Call dumper with bounds checking. */
static int
dump_raw_write(struct dumperinfo *di, void *virtual, vm_offset_t physical,
    off_t offset, size_t length)
{
	int error;

	error = dump_check_bounds(di, offset, length);
	if (error != 0)
		return (error);

	return (di->dumper(di->priv, virtual, physical, offset, length));
}

int
dump_write(struct dumperinfo *di, void *virtual, vm_offset_t physical,
    off_t offset, size_t length)
{

#ifdef EKCD
	if (di->kdc != NULL) {
		return (dump_encrypted_write(di, virtual, physical, offset,
		    length));
	}
#endif

	return (dump_raw_write(di, virtual, physical, offset, length));
}

static int
dump_pad(struct dumperinfo *di, void *virtual, size_t length, void **buf,
    size_t *size)
{

	if (length > di->blocksize)
		return (ENOMEM);

	*size = di->blocksize;
	if (length == di->blocksize) {
		*buf = virtual;
	} else {
		*buf = di->blockbuf;
		memcpy(*buf, virtual, length);
		memset((uint8_t *)*buf + length, 0, di->blocksize - length);
	}

	return (0);
}

static int
dump_raw_write_pad(struct dumperinfo *di, void *virtual, vm_offset_t physical,
    off_t offset, size_t length, size_t *size)
{
	void *buf;
	int error;

	error = dump_pad(di, virtual, length, &buf, size);
	if (error != 0)
		return (error);

	return (dump_raw_write(di, buf, physical, offset, *size));
}

int
dump_write_pad(struct dumperinfo *di, void *virtual, vm_offset_t physical,
    off_t offset, size_t length, size_t *size)
{
	void *buf;
	int error;

	error = dump_pad(di, virtual, length, &buf, size);
	if (error != 0)
		return (error);

	return (dump_write(di, buf, physical, offset, *size));
}

int
dump_write_header(struct dumperinfo *di, struct kerneldumpheader *kdh,
    vm_offset_t physical, off_t offset)
{
	size_t size;
	int ret;

	ret = dump_raw_write_pad(di, kdh, physical, offset, sizeof(*kdh),
	    &size);
	if (ret == 0 && size != di->blocksize)
		ret = EINVAL;
	return (ret);
}

int
dump_write_key(struct dumperinfo *di, vm_offset_t physical, off_t offset)
{
#ifndef EKCD
	return (0);
#else /* EKCD */
	struct kerneldumpcrypto *kdc;

	kdc = di->kdc;
	if (kdc == NULL)
		return (0);

	return (dump_raw_write(di, kdc->kdc_dumpkey, physical, offset,
	    kdc->kdc_dumpkeysize));
#endif /* !EKCD */
}

void
mkdumpheader(struct kerneldumpheader *kdh, char *magic, uint32_t archver,
    uint64_t dumplen, uint32_t dumpkeysize, uint32_t blksz)
{
	size_t dstsize;

	bzero(kdh, sizeof(*kdh));
	strlcpy(kdh->magic, magic, sizeof(kdh->magic));
	strlcpy(kdh->architecture, MACHINE_ARCH, sizeof(kdh->architecture));
	kdh->version = htod32(KERNELDUMPVERSION);
	kdh->architectureversion = htod32(archver);
	kdh->dumplength = htod64(dumplen);
	kdh->dumptime = htod64(time_second);
	kdh->dumpkeysize = htod32(dumpkeysize);
	kdh->blocksize = htod32(blksz);
	strlcpy(kdh->hostname, prison0.pr_hostname, sizeof(kdh->hostname));
	dstsize = sizeof(kdh->versionstring);
	if (strlcpy(kdh->versionstring, version, dstsize) >= dstsize)
		kdh->versionstring[dstsize - 2] = '\n';
	if (panicstr != NULL)
		strlcpy(kdh->panicstring, panicstr, sizeof(kdh->panicstring));
	kdh->parity = kerneldump_parity(kdh);
}

#ifdef DDB
DB_SHOW_COMMAND(panic, db_show_panic)
{

	if (panicstr == NULL)
		db_printf("panicstr not set\n");
	else
		db_printf("panic: %s\n", panicstr);
}
#endif
