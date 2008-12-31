/*-
 * Copyright (c) 2004 Poul-Henning Kamp
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/watchdog/watchdog.c,v 1.5.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/watchdog.h>
#include <sys/bus.h>
#include <machine/bus.h>

static struct cdev *wd_dev;

static int
wd_ioctl(struct cdev *dev __unused, u_long cmd, caddr_t data,
    int flags __unused, struct thread *td)
{
	int error;
	u_int u;

	if (cmd != WDIOCPATPAT)
		return (ENOIOCTL);
	u = *(u_int *)data;
	if (u & ~(WD_ACTIVE | WD_PASSIVE | WD_INTERVAL))
		return (EINVAL);
	if ((u & (WD_ACTIVE | WD_PASSIVE)) == (WD_ACTIVE | WD_PASSIVE))
		return (EINVAL);
	if ((u & (WD_ACTIVE | WD_PASSIVE)) == 0 && (u & WD_INTERVAL) > 0)
		return (EINVAL);
	if (u & WD_PASSIVE)
		return (ENOSYS);	/* XXX Not implemented yet */
	if ((u & WD_INTERVAL) == WD_TO_NEVER) {
		u = 0;
		/* Assume all is well; watchdog signals failure. */
		error = 0;
	} else {
		/* Assume no watchdog available; watchdog flags success */
		error = EOPNOTSUPP;
	}
	EVENTHANDLER_INVOKE(watchdog_list, u, &error);
	return (error);
}

static struct cdevsw wd_cdevsw = {
	.d_version =	D_VERSION,
	.d_ioctl =	wd_ioctl,
	.d_name =	"watchdog",
};

static int
watchdog_modevent(module_t mod __unused, int type, void *data __unused)
{
	switch(type) {
	case MOD_LOAD:
		wd_dev = make_dev(&wd_cdevsw, 0,
		    UID_ROOT, GID_WHEEL, 0600, _PATH_WATCHDOG);
		return 0;
	case MOD_UNLOAD:
		destroy_dev(wd_dev);
		return 0;
	case MOD_SHUTDOWN:
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

DEV_MODULE(watchdog, watchdog_modevent, NULL);
