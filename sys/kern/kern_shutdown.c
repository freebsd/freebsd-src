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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 * $FreeBSD$
 */

#include "opt_ddb.h"
#include "opt_hw_wdog.h"
#include "opt_panic.h"
#include "opt_show_busybufs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/disklabel.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/resourcevar.h>
#include <sys/smp.h>		/* smp_active */
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/vnode.h>

#include <machine/pcb.h>
#include <machine/md_var.h>

#include <sys/signalvar.h>
#ifdef DDB
#include <ddb/ddb.h>
#endif

#ifndef PANIC_REBOOT_WAIT_TIME
#define PANIC_REBOOT_WAIT_TIME 15 /* default to 15 seconds */
#endif

/*
 * Note that stdarg.h and the ANSI style va_start macro is used for both
 * ANSI and traditional C compilers.
 */
#include <machine/stdarg.h>

#ifdef DDB
#ifdef DDB_UNATTENDED
int debugger_on_panic = 0;
#else
int debugger_on_panic = 1;
#endif
SYSCTL_INT(_debug, OID_AUTO, debugger_on_panic, CTLFLAG_RW,
	&debugger_on_panic, 0, "Run debugger on kernel panic");
#endif

int sync_on_panic = 1;
SYSCTL_INT(_kern, OID_AUTO, sync_on_panic, CTLFLAG_RW,
	&sync_on_panic, 0, "Do a sync before rebooting from a panic");

SYSCTL_NODE(_kern, OID_AUTO, shutdown, CTLFLAG_RW, 0, "Shutdown environment");

#ifdef	HW_WDOG
/*
 * If there is a hardware watchdog, point this at the function needed to
 * hold it off.
 * It's needed when the kernel needs to do some lengthy operations.
 * e.g. in wd.c when dumping core.. It's most annoying to have
 * your precious core-dump only half written because the wdog kicked in.
 */
watchdog_tickle_fn wdog_tickler = NULL;
#endif	/* HW_WDOG */

/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
const char *panicstr;

int dumping;				 /* system is dumping */

static void boot(int) __dead2;
static void dumpsys(void);
static void poweroff_wait(void *, int);
static void shutdown_halt(void *junk, int howto);
static void shutdown_panic(void *junk, int howto);
static void shutdown_reset(void *junk, int howto);

/* register various local shutdown events */
static void 
shutdown_conf(void *unused)
{
	EVENTHANDLER_REGISTER(shutdown_final, poweroff_wait, NULL, SHUTDOWN_PRI_FIRST);
	EVENTHANDLER_REGISTER(shutdown_final, shutdown_halt, NULL, SHUTDOWN_PRI_LAST + 100);
	EVENTHANDLER_REGISTER(shutdown_final, shutdown_panic, NULL, SHUTDOWN_PRI_LAST + 100);
	EVENTHANDLER_REGISTER(shutdown_final, shutdown_reset, NULL, SHUTDOWN_PRI_LAST + 200);
}

SYSINIT(shutdown_conf, SI_SUB_INTRINSIC, SI_ORDER_ANY, shutdown_conf, NULL)

/*
 * The system call that results in a reboot
 *
 * MPSAFE
 */
/* ARGSUSED */
int
reboot(struct thread *td, struct reboot_args *uap)
{
	int error;

	mtx_lock(&Giant);
	if ((error = suser_td(td)) == 0)
		boot(uap->opt);
	mtx_unlock(&Giant);
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
static struct pcb dumppcb;

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

/*
 *  Go through the rigmarole of shutting down..
 * this used to be in machdep.c but I'll be dammned if I could see
 * anything machine dependant in it.
 */
static void
boot(int howto)
{

	/* collect extra flags that shutdown_nice might have set */
	howto |= shutdown_howto;

#ifdef DDB
	/* We are out of the debugger now. */
	db_active = 0;
#endif

#ifdef SMP
	if (smp_active)
		printf("boot() called on cpu#%d\n", PCPU_GET(cpuid));
#endif
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
		int subiter;

		waittime = 0;
		printf("\nsyncing disks... ");

		sync(thread0, NULL);

		/*
		 * With soft updates, some buffers that are
		 * written will be remarked as dirty until other
		 * buffers are written.
		 */
		for (iter = pbusy = 0; iter < 20; iter++) {
			nbusy = 0;
			for (bp = &buf[nbuf]; --bp >= buf; ) {
				if ((bp->b_flags & B_INVAL) == 0 &&
				    BUF_REFCNT(bp) > 0) {
					nbusy++;
				} else if ((bp->b_flags & (B_DELWRI | B_INVAL))
						== B_DELWRI) {
					/* bawrite(bp);*/
					nbusy++;
				}
			}
			if (nbusy == 0)
				break;
			printf("%d ", nbusy);
			if (nbusy < pbusy)
				iter = 0;
			pbusy = nbusy;
			sync(thread0, NULL);
 			if (curthread != NULL) {
				DROP_GIANT_NOSWITCH();
   				for (subiter = 0; subiter < 50 * iter; subiter++) {
     					mtx_lock_spin(&sched_lock);
     					setrunqueue(curthread);
					curthread->td_proc->p_stats->p_ru.ru_nvcsw++;
     					mi_switch(); /* Allow interrupt threads to run */
     					mtx_unlock_spin(&sched_lock);
     					DELAY(1000);
   				}
				PICKUP_GIANT();
 			} else
			DELAY(50000 * iter);
		}
		printf("\n");
		/*
		 * Count only busy local buffers to prevent forcing 
		 * a fsck if we're just a client of a wedged NFS server
		 */
		nbusy = 0;
		for (bp = &buf[nbuf]; --bp >= buf; ) {
			if (((bp->b_flags&B_INVAL) == 0 && BUF_REFCNT(bp)) ||
			    ((bp->b_flags & (B_DELWRI|B_INVAL)) == B_DELWRI)) {
				if (bp->b_dev == NODEV) {
					TAILQ_REMOVE(&mountlist,
					    bp->b_vp->v_mount, mnt_list);
					continue;
				}
				nbusy++;
#if defined(SHOW_BUSYBUFS) || defined(DIAGNOSTIC)
				printf(
			    "%d: dev:%s, flags:%08lx, blkno:%ld, lblkno:%ld\n",
				    nbusy, devtoname(bp->b_dev),
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
			printf("giving up on %d buffers\n", nbusy);
			DELAY(5000000);	/* 5 seconds */
		} else {
			printf("done\n");
			/*
			 * Unmount filesystems
			 */
			if (panicstr == 0)
				vfs_unmountall();
		}
		DELAY(100000);		/* wait for console output to finish */
	}

	print_uptime();

	/*
	 * Ok, now do things that assume all filesystem activity has
	 * been completed.
	 */
	EVENTHANDLER_INVOKE(shutdown_post_sync, howto);
	splhigh();
	if ((howto & (RB_HALT|RB_DUMP)) == RB_DUMP && !cold)
		dumpsys();

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
		printf("--> Press a key on the console to reboot <--\n");
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

/*
 * Magic number for savecore
 *
 * exported (symorder) and used at least by savecore(8)
 *
 */
static u_long const	dumpmag = 0x8fca0101UL;	

static int	dumpsize = 0;		/* also for savecore */

static int	dodump = 1;

SYSCTL_INT(_machdep, OID_AUTO, do_dump, CTLFLAG_RW, &dodump, 0,
    "Try to perform coredump on kernel panic");

static int
setdumpdev(dev_t dev)
{
	int psize;
	long newdumplo;

	if (dev == NODEV) {
		dumpdev = dev;
		return (0);
	}
	if (devsw(dev) == NULL)
		return (ENXIO);		/* XXX is this right? */
	if (devsw(dev)->d_psize == NULL)
		return (ENXIO);		/* XXX should be ENODEV ? */
	psize = devsw(dev)->d_psize(dev);
	if (psize == -1)
		return (ENXIO);		/* XXX should be ENODEV ? */
	/*
	 * XXX should clean up checking in dumpsys() to be more like this.
	 */
	newdumplo = psize - Maxmem * (PAGE_SIZE / DEV_BSIZE);
	if (newdumplo <= LABELSECTOR)
		return (ENOSPC);
	dumpdev = dev;
	dumplo = newdumplo;
	return (0);
}


/* ARGSUSED */
static void
dump_conf(void *dummy)
{
	if (setdumpdev(dumpdev) != 0)
		dumpdev = NODEV;
}

SYSINIT(dump_conf, SI_SUB_DUMP_CONF, SI_ORDER_FIRST, dump_conf, NULL)

static int
sysctl_kern_dumpdev(SYSCTL_HANDLER_ARGS)
{
	int error;
	udev_t ndumpdev;

	ndumpdev = dev2udev(dumpdev);
	error = sysctl_handle_opaque(oidp, &ndumpdev, sizeof ndumpdev, req);
	if (error == 0 && req->newptr != NULL)
		error = setdumpdev(udev2dev(ndumpdev, 0));
	return (error);
}

SYSCTL_PROC(_kern, KERN_DUMPDEV, dumpdev, CTLTYPE_OPAQUE|CTLFLAG_RW,
	0, sizeof dumpdev, sysctl_kern_dumpdev, "T,dev_t", "");

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
static void
dumpsys(void)
{
	int	error;

	savectx(&dumppcb);
	if (!dodump)
		return;
	if (dumpdev == NODEV)
		return;
	if (!(devsw(dumpdev)))
		return;
	if (!(devsw(dumpdev)->d_dump))
		return;
	if (dumping++) {
		dumping--;
		printf("Dump already in progress, bailing...\n");
		return;
	}
	dumpsize = Maxmem;
	printf("\ndumping to dev %s, offset %ld\n", devtoname(dumpdev), dumplo);
	printf("dump ");
	error = (*devsw(dumpdev)->d_dump)(dumpdev);
	dumping--;
	if (error == 0) {
		printf("succeeded\n");
		return;
	}
	printf("failed, reason: ");
	switch (error) {
	case ENODEV:
		printf("device doesn't support a dump routine\n");
		break;

	case ENXIO:
		printf("device bad\n");
		break;

	case EFAULT:
		printf("device not ready\n");
		break;

	case EINVAL:
		printf("area improper\n");
		break;

	case EIO:
		printf("i/o error\n");
		break;

	case EINTR:
		printf("aborted from console\n");
		break;

	default:
		printf("unknown, error = %d\n", error);
		break;
	}
}

int
dumpstatus(vm_offset_t addr, off_t count)
{
	int c;

	if (addr % (1024 * 1024) == 0) {
#ifdef HW_WDOG
		if (wdog_tickler)
			(*wdog_tickler)();
#endif   
		printf("%ld ", (long)(count / (1024 * 1024)));
	}

	if ((c = cncheckc()) == 0x03)
		return -1;
	else if (c != -1)
		printf("[CTRL-C to abort] ");
	
	return 0;
}

#ifdef SMP
static u_int panic_cpu = NOCPU;
#endif

/*
 * Panic is called on unresolvable fatal errors.  It prints "panic: mesg",
 * and then reboots.  If we are called twice, then we avoid trying to sync
 * the disks as this often leads to recursive panics.
 *
 * MPSAFE
 */
void
panic(const char *fmt, ...)
{
	int bootopt;
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
	if (panicstr)
		bootopt |= RB_NOSYNC;
	else
		panicstr = fmt;

	va_start(ap, fmt);
	(void)vsnprintf(buf, sizeof(buf), fmt, ap);
	if (panicstr == fmt)
		panicstr = buf;
	va_end(ap);
	printf("panic: %s\n", buf);
#ifdef SMP
	/* two separate prints in case of an unmapped page and trap */
	printf("cpuid = %d; ", PCPU_GET(cpuid));
#ifdef APIC_IO
	printf("lapic.id = %08x\n", lapic.id);
#endif
#endif

#if defined(DDB)
	if (debugger_on_panic)
		Debugger ("panic");
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
	if(!(howto & RB_POWEROFF) || poweroff_delay <= 0)
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
	int error;

	if (panicstr)
		return;

	p = (struct proc *)arg;
	printf("Waiting (max %d seconds) for system process `%s' to stop...",
	    kproc_shutdown_wait, p->p_comm);
	error = kthread_suspend(p, kproc_shutdown_wait * hz);

	if (error == EWOULDBLOCK)
		printf("timed out\n");
	else
		printf("stopped\n");
}
