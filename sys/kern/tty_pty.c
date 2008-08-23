/*-
 * Copyright (c) 2008 Ed Schouten <ed@FreeBSD.org>
 * All rights reserved.
 *
 * Portions of this software were developed under sponsorship from Snow
 * B.V., the Netherlands.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/eventhandler.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/tty.h>

/*
 * This driver implements a BSD-style compatibility naming scheme for
 * the pts(4) driver. We just call into pts(4) to create the actual PTY.
 * To make sure we don't use the same PTY multiple times, we abuse
 * si_drv1 inside the cdev to mark whether the PTY is in use.
 */

static unsigned int pty_warningcnt = 10;
SYSCTL_UINT(_kern, OID_AUTO, tty_pty_warningcnt, CTLFLAG_RW,
	&pty_warningcnt, 0,
	"Warnings that will be triggered upon PTY allocation");

static int
ptydev_fdopen(struct cdev *dev, int fflags, struct thread *td, struct file *fp)
{
	int u, error;
	char name[] = "ttyXX";

	if (!atomic_cmpset_ptr((uintptr_t *)&dev->si_drv1, 0, 1))
		return (EBUSY);

	/* Generate device name and create PTY. */
	u = dev2unit(dev);
	name[3] = u >> 8;
	name[4] = u;

	error = pts_alloc_external(fflags & (FREAD|FWRITE), td, fp, dev, name);
	if (error != 0) {
		destroy_dev_sched(dev);
		return (error);
	}

	/* Raise a warning when a legacy PTY has been allocated. */
	if (pty_warningcnt > 0) {
		pty_warningcnt--;
		printf("pid %d (%s) is using legacy pty devices%s\n", 
		    td->td_proc->p_pid, td->td_name,
		    pty_warningcnt ? "" : " - not logging anymore");
	}

	return (0);
}

static struct cdevsw ptydev_cdevsw = {
	.d_version	= D_VERSION,
	.d_fdopen	= ptydev_fdopen,
	.d_name		= "ptydev",
};

static void
pty_clone(void *arg, struct ucred *cr, char *name, int namelen,
    struct cdev **dev)
{
	int u;

	/* Cloning is already satisfied. */
	if (*dev != NULL)
		return;

	/* Only catch /dev/ptyXX. */
	if (namelen != 5 || bcmp(name, "pty", 3) != 0)
		return;

	/* Only catch /dev/pty[l-sL-S]X. */
	if (!(name[3] >= 'l' && name[3] <= 's') &&
	    !(name[3] >= 'L' && name[3] <= 'S'))
		return;

	/* Only catch /dev/pty[l-sL-S][0-9a-v]. */
	if (!(name[4] >= '0' && name[4] <= '9') &&
	    !(name[4] >= 'a' && name[4] <= 'v'))
		return;

	/* Create the controller device node. */
	u = (unsigned int)name[3] << 8 | name[4];
	*dev = make_dev_credf(MAKEDEV_REF, &ptydev_cdevsw, u,
	    NULL, UID_ROOT, GID_WHEEL, 0666, name);
}

static void
pty_init(void *unused)
{

	EVENTHANDLER_REGISTER(dev_clone, pty_clone, 0, 1000);
}

SYSINIT(pty, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, pty_init, NULL);
