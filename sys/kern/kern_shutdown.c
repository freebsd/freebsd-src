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
 * $Id: kern_shutdown.c,v 1.25 1997/11/06 19:29:13 phk Exp $
 */

#include "opt_ddb.h"
#include "opt_panic.h"
#include "opt_show_busybufs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mount.h>
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
static int debugger_on_panic = 0;
#else
static int debugger_on_panic = 1;
#endif
SYSCTL_INT(_debug, OID_AUTO, debugger_on_panic, CTLFLAG_RW,
	&debugger_on_panic, 0, "");
#endif

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
	struct shutdown_list_element *next;
	bootlist_fn function;
	void *arg;
} *sle_p;

/*
 * there are two shutdown lists. Some things need to be shut down
 * Earlier than others.
 */
static sle_p shutdown_list1;
static sle_p shutdown_list2;

static void boot __P((int)) __dead2;
static void dumpsys __P((void));

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

	if ((error = suser(p->p_ucred, &p->p_acflag)))
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
	int c, spins;

	/* The MPSPEC says that the BSP must do the shutdown */
	if (smp_active) {
		smp_active = 0;

		spins = 100;

		printf("boot() called on cpu#%d\n", cpuid);
		while ((c = cpuid) != 0) {
			if (spins-- < 1) {
				printf("timeout waiting for cpu #0!\n");
				break;
			}
			printf("I'm on cpu#%d, I need to be on cpu#0, sleeping..\n", c);
			tsleep((caddr_t)&smp_active, PZERO, "cpu0wt", 10);
		}
	}
#endif
	/*
	 * Do any callouts that should be done BEFORE syncing the filesystems.
	 */
	ep = shutdown_list1;
	while (ep) {
		shutdown_list1 = ep->next;
		(*ep->function)(howto, ep->arg);
		ep = ep->next;
	}

	/* 
	 * Now sync filesystems
	 */
	if (!cold && (howto & RB_NOSYNC) == 0 && waittime < 0) {
		register struct buf *bp;
		int iter, nbusy;

		waittime = 0;
		printf("\nsyncing disks... ");

		sync(&proc0, NULL);

		for (iter = 0; iter < 20; iter++) {
			nbusy = 0;
			for (bp = &buf[nbuf]; --bp >= buf; ) {
				if ((bp->b_flags & (B_BUSY | B_INVAL)) == B_BUSY) {
					nbusy++;
				}
			}
			if (nbusy == 0)
				break;
			printf("%d ", nbusy);
			DELAY(40000 * iter);
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
				if ((bp->b_flags & (B_BUSY | B_INVAL)) == B_BUSY) {
					nbusy++;
					printf("%d: dev:%08x, flags:%08x, blkno:%d, lblkno:%d\n", nbusy, bp->b_dev, bp->b_flags, bp->b_blkno, bp->b_lblkno);
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
		DELAY(100000);			/* wait for console output to finish */
	}

	/*
	 * Ok, now do things that assume all filesystem activity has
	 * been completed.
	 */
	ep = shutdown_list2;
	while (ep) {
		shutdown_list2 = ep->next;
		(*ep->function)(howto, ep->arg);
		ep = ep->next;
	}
	splhigh();
	if (howto & RB_HALT) {
		cpu_power_down();
		printf("\n");
		printf("The operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");
		switch (cngetc()) {
		case -1:		/* No console, just die */
			cpu_halt();
			/* NOTREACHED */
		default:
			break;
		}
	} else {
		if (howto & RB_DUMP) {
			if (!cold) {
				savectx(&dumppcb);
				dumppcb.pcb_cr3 = rcr3();
				dumpsys();
			}

			if (PANIC_REBOOT_WAIT_TIME != 0) {
				if (PANIC_REBOOT_WAIT_TIME != -1) {
					int loop;
					printf("Automatic reboot in %d seconds - press a key on the console to abort\n",
						PANIC_REBOOT_WAIT_TIME);
					for (loop = PANIC_REBOOT_WAIT_TIME * 10; loop > 0; --loop) {
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
SYSCTL_INT(_machdep, OID_AUTO, do_dump, CTLFLAG_RW, &dodump, 0, "");

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
	if ((minor(dumpdev)&07) != 1)
		return;
	if (!(bdevsw[major(dumpdev)]))
		return;
	if (!(bdevsw[major(dumpdev)]->d_dump))
		return;
	dumpsize = Maxmem;
	printf("\ndumping to dev %lx, offset %ld\n", dumpdev, dumplo);
	printf("dump ");
	switch ((*bdevsw[major(dumpdev)]->d_dump)(dumpdev)) {

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

	bootopt = RB_AUTOBOOT | RB_DUMP;
	if (panicstr)
		bootopt |= RB_NOSYNC;
	else
		panicstr = fmt;

	printf("panic: ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
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
 * Two routines to handle adding/deleting items on the
 * shutdown callout lists
 *
 * at_shutdown():
 * Take the arguments given and put them onto the shutdown callout list.
 * However first make sure that it's not already there.
 * returns 0 on success.
 */
int
at_shutdown(bootlist_fn function, void *arg, int position)
{
	sle_p ep, *epp;

	switch(position) {
	case SHUTDOWN_PRE_SYNC:
		epp = &shutdown_list1;
		break;
	case SHUTDOWN_POST_SYNC:
		epp = &shutdown_list2;
		break;
	default:
		printf("bad exit callout list specified\n");
		return (EINVAL);
	}
	if (rm_at_shutdown(function, arg))
		printf("exit callout entry already present\n");
	ep = malloc(sizeof(*ep), M_TEMP, M_NOWAIT);
	if (ep == NULL)
		return (ENOMEM);
	ep->next = *epp;
	ep->function = function;
	ep->arg = arg;
	*epp = ep;
	return (0);
}

/*
 * Scan the exit callout lists for the given items and remove them.
 * Returns the number of items removed.
 */
int
rm_at_shutdown(bootlist_fn function, void *arg)
{
	sle_p *epp, ep;
	int count;

	count = 0;
	epp = &shutdown_list1;
	ep = *epp;
	while (ep) {
		if ((ep->function == function) && (ep->arg == arg)) {
			*epp = ep->next;
			free(ep, M_TEMP);
			count++;
		} else {
			epp = &ep->next;
		}
		ep = *epp;
	}
	epp = &shutdown_list2;
	ep = *epp;
	while (ep) {
		if ((ep->function == function) && (ep->arg == arg)) {
			*epp = ep->next;
			free(ep, M_TEMP);
			count++;
		} else {
			epp = &ep->next;
		}
		ep = *epp;
	}
	return (count);
}

