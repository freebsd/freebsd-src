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
 * $Id: kern_shutdown.c,v 1.55 1999/07/17 20:47:50 phk Exp $
 */

#include "opt_ddb.h"
#include "opt_hw_wdog.h"
#include "opt_panic.h"
#include "opt_show_busybufs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/conf.h>
#include <sys/sysproto.h>

#include <machine/pcb.h>
#include <machine/clock.h>
#include <machine/cons.h>
#include <machine/md_var.h>
#ifdef SMP
#include <machine/smp.h>		/* smp_active, cpuid */
#endif

#include <sys/signalvar.h>

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

/*
 * callout list for things to do a shutdown
 */
typedef struct shutdown_list_element {
	LIST_ENTRY(shutdown_list_element) links;
	bootlist_fn function;
	void *arg;
	int priority;
} *sle_p;

/*
 * There are three shutdown lists. Some things need to be shut down
 * earlier than others.
 */
LIST_HEAD(shutdown_list, shutdown_list_element);

static struct shutdown_list shutdown_lists[SHUTDOWN_FINAL + 1];

static void boot __P((int)) __dead2;
static void dumpsys __P((void));
static int setdumpdev __P((dev_t dev));


#ifndef _SYS_SYSPROTO_H_
struct reboot_args {
	int	opt;
};
#endif
/* ARGSUSED */

/*
 * The system call that results in a reboot
 */
int
reboot(p, uap)
	struct proc *p;
	struct reboot_args *uap;
{
	int error;

	if ((error = suser(p)))
		return (error);

	boot(uap->opt);
	return (0);
}

/*
 * Called by events that want to shut down.. e.g  <CTL><ALT><DEL> on a PC
 */
void
shutdown_nice()
{
	/* Send a signal to init(8) and have it shutdown the world */
	if (initproc != NULL) {
		psignal(initproc, SIGINT);
	} else {
		/* No init(8) running, so simply reboot */
		boot(RB_NOSYNC);
	}
	return;
}
static int	waittime = -1;
static struct pcb dumppcb;

/*
 *  Go through the rigmarole of shutting down..
 * this used to be in machdep.c but I'll be dammned if I could see
 * anything machine dependant in it.
 */
static void
boot(howto)
	int howto;
{
	sle_p ep;

#ifdef SMP
	if (smp_active) {
		printf("boot() called on cpu#%d\n", cpuid);
	}
#endif
	/*
	 * Do any callouts that should be done BEFORE syncing the filesystems.
	 */
	LIST_FOREACH(ep, &shutdown_lists[SHUTDOWN_PRE_SYNC], links)
		(*ep->function)(howto, ep->arg);

	/* 
	 * Now sync filesystems
	 */
	if (!cold && (howto & RB_NOSYNC) == 0 && waittime < 0) {
		register struct buf *bp;
		int iter, nbusy;

		waittime = 0;
		printf("\nsyncing disks... ");

		sync(&proc0, NULL);

		/*
		 * With soft updates, some buffers that are
		 * written will be remarked as dirty until other
		 * buffers are written.
		 */
		for (iter = 0; iter < 20; iter++) {
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
			sync(&proc0, NULL);
			DELAY(50000 * iter);
		}
		/*
		 * Count only busy local buffers to prevent forcing 
		 * a fsck if we're just a client of a wedged NFS server
		 */
		nbusy = 0;
		for (bp = &buf[nbuf]; --bp >= buf; ) {
			if (((bp->b_flags&B_INVAL) == 0 && BUF_REFCNT(bp)) ||
			    ((bp->b_flags & (B_DELWRI|B_INVAL)) == B_DELWRI)) {
				if (bp->b_dev == NODEV)
					CIRCLEQ_REMOVE(&mountlist,
					    bp->b_vp->v_mount, mnt_list);
				else
					nbusy++;
			}


		}
		if (nbusy) {
			/*
			 * Failed to sync all blocks. Indicate this and don't
			 * unmount filesystems (thus forcing an fsck on reboot).
			 */
			printf("giving up\n");
#ifdef SHOW_BUSYBUFS
			nbusy = 0;
			for (bp = &buf[nbuf]; --bp >= buf; ) {
				if ((bp->b_flags & B_INVAL) == 0 &&
				    BUF_REFCNT(bp) > 0) {
					nbusy++;
					printf(
			"%d: dev:%08lx, flags:%08lx, blkno:%ld, lblkno:%ld\n",
					    nbusy, (u_long)bp->b_dev,
					    bp->b_flags, (long)bp->b_blkno,
					    (long)bp->b_lblkno);
				}
			}
			DELAY(5000000);	/* 5 seconds */
#endif
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

	/*
	 * Ok, now do things that assume all filesystem activity has
	 * been completed.
	 */
	LIST_FOREACH(ep, &shutdown_lists[SHUTDOWN_POST_SYNC], links)
		(*ep->function)(howto, ep->arg);
	splhigh();
	if ((howto & (RB_HALT|RB_DUMP)) == RB_DUMP && !cold) {
		savectx(&dumppcb);
#ifdef __i386__
		dumppcb.pcb_cr3 = rcr3();
#endif
		dumpsys();
	}

	/* Now that we're going to really halt the system... */
	LIST_FOREACH(ep, &shutdown_lists[SHUTDOWN_FINAL], links)
		(*ep->function)(howto, ep->arg);

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
	} else if (howto & RB_DUMP) {
		/* System Paniced */

		if (PANIC_REBOOT_WAIT_TIME != 0) {
			if (PANIC_REBOOT_WAIT_TIME != -1) {
				int loop;
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
					goto die;
			}
		} else { /* zero time specified - reboot NOW */
			goto die;
		}
		printf("--> Press a key on the console to reboot <--\n");
		cngetc();
	}
die:
	printf("Rebooting...\n");
	DELAY(1000000);	/* wait 1 sec for printf's to complete and be read */
	/* cpu_boot(howto); */ /* doesn't do anything at the moment */
	cpu_reset();
	for(;;) ;
	/* NOTREACHED */
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
setdumpdev(dev)
	dev_t dev;
{
	int maj, psize;
	long newdumplo;

	if (dev == NODEV) {
		dumpdev = dev;
		return (0);
	}
	maj = major(dev);
	if (bdevsw(dev) == NULL)
		return (ENXIO);		/* XXX is this right? */
	if (bdevsw(dev)->d_psize == NULL)
		return (ENXIO);		/* XXX should be ENODEV ? */
	psize = bdevsw(dev)->d_psize(dev);
	if (psize == -1)
		return (ENXIO);		/* XXX should be ENODEV ? */
	/*
	 * XXX should clean up checking in dumpsys() to be more like this,
	 * and nuke dodump sysctl (too many knobs).
	 */
	newdumplo = psize - Maxmem * PAGE_SIZE / DEV_BSIZE;
	if (newdumplo < 0)
		return (ENOSPC);
	dumpdev = dev;
	dumplo = newdumplo;
	return (0);
}


/* ARGSUSED */
static void dump_conf __P((void *dummy));
static void
dump_conf(dummy)
	void *dummy;
{
	if (setdumpdev(dumpdev) != 0)
		dumpdev = NODEV;
}

SYSINIT(dump_conf, SI_SUB_DUMP_CONF, SI_ORDER_FIRST, dump_conf, NULL)

static int
sysctl_kern_dumpdev SYSCTL_HANDLER_ARGS
{
	int error;
	udev_t ndumpdev;

	if (dumpdev != NODEV)
		ndumpdev = makeudev(devsw(dumpdev)->d_bmaj, minor(dumpdev));
	else
		ndumpdev = dev2udev(dumpdev);
	error = sysctl_handle_opaque(oidp, &ndumpdev, sizeof ndumpdev, req);
	if (error == 0 && req->newptr != NULL)
		error = setdumpdev(udev2dev(ndumpdev, 1));
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

	if (!dodump)
		return;
	if (dumpdev == NODEV)
		return;
	if (!(bdevsw(dumpdev)))
		return;
	if (!(bdevsw(dumpdev)->d_dump))
		return;
	dumpsize = Maxmem;
	printf("\ndumping to dev (%d,%d), offset %ld\n",
		major(dumpdev), minor(dumpdev), dumplo);
	printf("dump ");
	switch ((*bdevsw(dumpdev)->d_dump)(dumpdev)) {

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
		printf("succeeded\n");
		break;
	}
}

/*
 * Panic is called on unresolvable fatal errors.  It prints "panic: mesg",
 * and then reboots.  If we are called twice, then we avoid trying to sync
 * the disks as this often leads to recursive panics.
 */
void
panic(const char *fmt, ...)
{
	int bootopt;
	va_list ap;
	static char buf[256];

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
	/* three seperate prints in case of an unmapped page and trap */
	printf("mp_lock = %08x; ", mp_lock);
	printf("cpuid = %d; ", cpuid);
	printf("lapic.id = %08x\n", lapic.id);
#endif

#if defined(DDB)
	if (debugger_on_panic)
		Debugger ("panic");
#endif
	boot(bootopt);
}

/*
 * Three routines to handle adding/deleting items on the
 * shutdown callout lists
 *
 * at_shutdown():
 * Take the arguments given and put them onto the shutdown callout list.
 * However first make sure that it's not already there.
 * returns 0 on success.
 */
int
at_shutdown(bootlist_fn function, void *arg, int queue)
{
	return(at_shutdown_pri(function, arg, queue, SHUTDOWN_PRI_DEFAULT));
}

/*
 * at_shutdown_pri():
 * Take the arguments given and put them onto the shutdown callout list
 * with the given execution priority.
 * returns 0 on success.
 */
int
at_shutdown_pri(bootlist_fn function, void *arg, int queue, int pri)
{
	sle_p op, ep, ip;

	op = NULL;		/* shut up gcc */
	if (queue < SHUTDOWN_PRE_SYNC
	 || queue > SHUTDOWN_FINAL) {
		printf("at_shutdown: bad exit callout queue %d specified\n",
		       queue);
		return (EINVAL);
	}
	if (rm_at_shutdown(function, arg))
		printf("at_shutdown: exit callout entry was already present\n");
	ep = malloc(sizeof(*ep), M_TEMP, M_NOWAIT);
	if (ep == NULL)
		return (ENOMEM);
	ep->function = function;
	ep->arg = arg;
	ep->priority = pri;

	/* Sort into list of items on this queue */
	ip = LIST_FIRST(&shutdown_lists[queue]);
	if (ip == NULL) {
		LIST_INSERT_HEAD(&shutdown_lists[queue], ep, links);
	} else {
		for (; ip != NULL; op = ip, ip = LIST_NEXT(ip, links)) {
			if (ep->priority < ip->priority) {
				LIST_INSERT_BEFORE(ip, ep, links);
				ep = NULL;
				break;
			}
		}
		if (ep != NULL)
			LIST_INSERT_AFTER(op, ep, links);
	}
	return (0);
}

/*
 * Scan the exit callout lists for the given items and remove them.
 * Returns the number of items removed.
 */
int
rm_at_shutdown(bootlist_fn function, void *arg)
{
	sle_p ep;
	int   count;
	int   queue;

	count = 0;
	for (queue = SHUTDOWN_PRE_SYNC; queue < SHUTDOWN_FINAL; queue++) {
		LIST_FOREACH(ep, &shutdown_lists[queue], links) {
			if ((ep->function == function) && (ep->arg == arg)) {
				LIST_REMOVE(ep, links);
				free(ep, M_TEMP);
				count++;
			}
		}
	}
	return (count);
}

/*
 * Support for poweroff delay.
 */
static int poweroff_delay = 0;
SYSCTL_INT(_kern_shutdown, OID_AUTO, poweroff_delay, CTLFLAG_RW,
	&poweroff_delay, 0, "");

static void poweroff_wait(int howto, void *unused)
{
	if(!(howto & RB_POWEROFF) || poweroff_delay <= 0)
		return;
	DELAY(poweroff_delay * 1000);
}

/*
 * XXX OK? This implies I know SHUTDOWN_PRI_LAST > SHUTDOWN_PRI_FIRST
 */
static void poweroff_conf(void *unused)
{
	at_shutdown_pri(poweroff_wait, NULL, SHUTDOWN_FINAL, SHUTDOWN_PRI_FIRST);
}

SYSINIT(poweroff_conf, SI_SUB_INTRINSIC, SI_ORDER_ANY, poweroff_conf, NULL)
