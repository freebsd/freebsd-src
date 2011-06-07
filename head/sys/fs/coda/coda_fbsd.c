/*-
 *             Coda: an Experimental Distributed File System
 *                              Release 3.1
 *
 *           Copyright (c) 1987-1998 Carnegie Mellon University
 *                          All Rights Reserved
 *
 * Permission  to  use, copy, modify and distribute this software and its
 * documentation is hereby granted,  provided  that  both  the  copyright
 * notice  and  this  permission  notice  appear  in  all  copies  of the
 * software, derivative works or  modified  versions,  and  any  portions
 * thereof, and that both notices appear in supporting documentation, and
 * that credit is given to Carnegie Mellon University  in  all  documents
 * and publicity pertaining to direct or indirect use of this code or its
 * derivatives.
 *
 * CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
 * SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
 * FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
 * DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
 * RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
 * ANY DERIVATIVE WORK.
 *
 * Carnegie  Mellon  encourages  users  of  this  software  to return any
 * improvements or extensions that  they  make,  and  to  grant  Carnegie
 * Mellon the rights to redistribute these changes without encumbrance.
 *
 * 	@(#) src/sys/coda/coda_fbsd.cr,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/ucred.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vnode_pager.h>

#include <fs/coda/coda.h>
#include <fs/coda/cnode.h>
#include <fs/coda/coda_vnops.h>
#include <fs/coda/coda_psdev.h>

static struct cdevsw codadevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	vc_open,
	.d_close =	vc_close,
	.d_read =	vc_read,
	.d_write =	vc_write,
	.d_ioctl =	vc_ioctl,
	.d_poll =	vc_poll,
	.d_name =	"coda",
};

static eventhandler_tag clonetag;

static LIST_HEAD(, coda_mntinfo) coda_mnttbl;

/*
 * For DEVFS, using bpf & tun drivers as examples.
 *
 * XXX: Why use a cloned interface, aren't we really just interested in
 * having a single /dev/cfs0?  It's not clear the coda module knows what to
 * do with more than one.
 */
static void coda_fbsd_clone(void *arg, struct ucred *cred, char *name,
    int namelen, struct cdev **dev);

static int
codadev_modevent(module_t mod, int type, void *data)
{
	struct coda_mntinfo *mnt;

	switch (type) {
	case MOD_LOAD:
		LIST_INIT(&coda_mnttbl);
		clonetag = EVENTHANDLER_REGISTER(dev_clone, coda_fbsd_clone,
		    0, 1000);
		break;

	case MOD_UNLOAD:
		/*
		 * XXXRW: At the very least, a busy check should occur here
		 * to prevent untimely unload.  Much more serious collection
		 * of allocated memory needs to take place; right now we leak
		 * like a sieve.
		 */
		EVENTHANDLER_DEREGISTER(dev_clone, clonetag);
		while ((mnt = LIST_FIRST(&coda_mnttbl)) != NULL) {
			LIST_REMOVE(mnt, mi_list);
			destroy_dev(mnt->dev);
			free(mnt, M_CODA);
		}
		break;

	default:
		return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t codadev_mod = {
	"codadev",
	codadev_modevent,
	NULL
};
DECLARE_MODULE(codadev, codadev_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);

static void
coda_fbsd_clone(void *arg, struct ucred *cred, char *name, int namelen,
    struct cdev **dev)
{
	struct coda_mntinfo *mnt;
	int u;

	if (*dev != NULL)
		return;
	if (dev_stdclone(name, NULL, "cfs", &u) != 1)
		return;
	*dev = make_dev(&codadevsw, u, UID_ROOT, GID_WHEEL, 0600,
	    "cfs%d", u);
	dev_ref(*dev);
	mnt = malloc(sizeof(struct coda_mntinfo), M_CODA, M_WAITOK|M_ZERO);
	LIST_INSERT_HEAD(&coda_mnttbl, mnt, mi_list);
	mnt->dev = *dev;
}

struct coda_mntinfo *
dev2coda_mntinfo(struct cdev *dev)
{
	struct coda_mntinfo *mnt;

	LIST_FOREACH(mnt, &coda_mnttbl, mi_list) {
		if (mnt->dev == dev)
			return (mnt);
	}
	return (NULL);
}
