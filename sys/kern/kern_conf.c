/*-
 * Parts Copyright (c) 1995 Terrence R. Lambert
 * Copyright (c) 1995 Julian R. Elischer
 * All rights reserved.
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
 *      This product includes software developed by Terrence R. Lambert.
 * 4. The name Terrence R. Lambert may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Julian R. Elischer ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE TERRENCE R. LAMBERT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/vnode.h>
#include <sys/queue.h>
#include <sys/ctype.h>
#include <machine/stdarg.h>

#define cdevsw_ALLOCSTART	(NUMCDEVSW/2)

static struct cdevsw 	*cdevsw[NUMCDEVSW];

static MALLOC_DEFINE(M_DEVT, "dev_t", "dev_t storage");

/*
 * This is the number of hash-buckets.  Experiements with 'real-life'
 * udev_t's show that a prime halfway between two powers of two works
 * best.
 */
#define DEVT_HASH 83

/* The number of dev_t's we can create before malloc(9) kick in.  */
#define DEVT_STASH 50

static struct specinfo devt_stash[DEVT_STASH];

static LIST_HEAD(, specinfo) dev_hash[DEVT_HASH];

static LIST_HEAD(, specinfo) dev_free;

devfs_create_t *devfs_create_hook;
devfs_destroy_t *devfs_destroy_hook;
int devfs_present;

static int ready_for_devs;

static int free_devt;
SYSCTL_INT(_debug, OID_AUTO, free_devt, CTLFLAG_RW, &free_devt, 0, "");

/* XXX: This is a hack */
void disk_dev_synth(dev_t dev);

struct cdevsw *
devsw(dev_t dev)
{
	if (dev->si_devsw)
		return (dev->si_devsw);
	/* XXX: Hack around our backwards disk code */
	disk_dev_synth(dev);
	if (dev->si_devsw)
		return (dev->si_devsw);
	if (devfs_present)
		return (NULL);
        return(cdevsw[major(dev)]);
}

/*
 *  Add a cdevsw entry
 */

int
cdevsw_add(struct cdevsw *newentry)
{

	if (newentry->d_maj < 0 || newentry->d_maj >= NUMCDEVSW) {
		printf("%s: ERROR: driver has bogus cdevsw->d_maj = %d\n",
		    newentry->d_name, newentry->d_maj);
		return (EINVAL);
	}

	if (cdevsw[newentry->d_maj]) {
		printf("WARNING: \"%s\" is usurping \"%s\"'s cdevsw[]\n",
		    newentry->d_name, cdevsw[newentry->d_maj]->d_name);
	}

	cdevsw[newentry->d_maj] = newentry;

	return (0);
}

/*
 *  Remove a cdevsw entry
 */

int
cdevsw_remove(struct cdevsw *oldentry)
{
	if (oldentry->d_maj < 0 || oldentry->d_maj >= NUMCDEVSW) {
		printf("%s: ERROR: driver has bogus cdevsw->d_maj = %d\n",
		    oldentry->d_name, oldentry->d_maj);
		return EINVAL;
	}

	cdevsw[oldentry->d_maj] = NULL;

	return 0;
}

/*
 * dev_t and u_dev_t primitives
 */

int
major(dev_t x)
{
	if (x == NODEV)
		return NOUDEV;
	return((x->si_udev >> 8) & 0xff);
}

int
minor(dev_t x)
{
	if (x == NODEV)
		return NOUDEV;
	return(x->si_udev & 0xffff00ff);
}

int
dev2unit(dev_t x)
{
	int i;

	if (x == NODEV)
		return NOUDEV;
	i = minor(x);
	return ((i & 0xff) | (i >> 8));
}

int
unit2minor(int unit)
{

	KASSERT(unit <= 0xffffff, ("Invalid unit (%d) in unit2minor", unit));
	return ((unit & 0xff) | ((unit << 8) & ~0xffff));
}

static dev_t
allocdev(void)
{
	static int stashed;
	struct specinfo *si;

	if (stashed >= DEVT_STASH) {
		MALLOC(si, struct specinfo *, sizeof(*si), M_DEVT,
		    M_USE_RESERVE | M_ZERO);
	} else if (LIST_FIRST(&dev_free)) {
		si = LIST_FIRST(&dev_free);
		LIST_REMOVE(si, si_hash);
	} else {
		si = devt_stash + stashed++;
		bzero(si, sizeof *si);
	si->si_flags |= SI_STASHED;
	}
	LIST_INIT(&si->si_children);
	TAILQ_INIT(&si->si_snapshots);
	return (si);
}

dev_t
makedev(int x, int y)
{
	struct specinfo *si;
	udev_t	udev;
	int hash;

	if (x == umajor(NOUDEV) && y == uminor(NOUDEV))
		panic("makedev of NOUDEV");
	udev = (x << 8) | y;
	hash = udev % DEVT_HASH;
	LIST_FOREACH(si, &dev_hash[hash], si_hash) {
		if (si->si_udev == udev)
			return (si);
	}
	si = allocdev();
	si->si_udev = udev;
	LIST_INSERT_HEAD(&dev_hash[hash], si, si_hash);
        return (si);
}

void
freedev(dev_t dev)
{

	if (!free_devt)
		return;
	if (SLIST_FIRST(&dev->si_hlist))
		return;
	if (dev->si_devsw || dev->si_drv1 || dev->si_drv2)
		return;
	LIST_REMOVE(dev, si_hash);
	if (dev->si_flags & SI_STASHED) {
		bzero(dev, sizeof(*dev));
		dev->si_flags |= SI_STASHED;
		LIST_INSERT_HEAD(&dev_free, dev, si_hash);
	} else {
		FREE(dev, M_DEVT);
	}
}

udev_t
dev2udev(dev_t x)
{
	if (x == NODEV)
		return NOUDEV;
	return (x->si_udev);
}

dev_t
udev2dev(udev_t x, int b)
{

	if (x == NOUDEV)
		return (NODEV);
	switch (b) {
		case 0:
			return makedev(umajor(x), uminor(x));
		case 1:
			return (NODEV);
		default:
			Debugger("udev2dev(...,X)");
			return NODEV;
	}
}

int
uminor(udev_t dev)
{
	return(dev & 0xffff00ff);
}

int
umajor(udev_t dev)
{
	return((dev & 0xff00) >> 8);
}

udev_t
makeudev(int x, int y)
{
        return ((x << 8) | y);
}

dev_t
make_dev(struct cdevsw *devsw, int minor, uid_t uid, gid_t gid, int perms, const char *fmt, ...)
{
	dev_t	dev;
	va_list ap;
	int i;

	KASSERT(umajor(makeudev(devsw->d_maj, minor)) == devsw->d_maj,
	    ("Invalid minor (%d) in make_dev", minor));

	if (!ready_for_devs) {
		printf("WARNING: Driver mistake: make_dev(%s) called before SI_SUB_DRIVERS\n",
		       fmt);
		/* XXX panic here once drivers are cleaned up */
	}

	dev = makedev(devsw->d_maj, minor);
	if (dev->si_flags & SI_NAMED) {
		printf( "WARNING: Driver mistake: repeat make_dev(\"%s\")\n",
		    dev->si_name);
		panic("don't do that");
		return (dev);
	}
	va_start(ap, fmt);
	i = kvprintf(fmt, NULL, dev->si_name, 32, ap);
	dev->si_name[i] = '\0';
	va_end(ap);
	dev->si_devsw = devsw;
	dev->si_uid = uid;
	dev->si_gid = gid;
	dev->si_mode = perms;
	dev->si_flags |= SI_NAMED;

	if (devfs_create_hook)
		devfs_create_hook(dev);
	return (dev);
}

int
dev_named(dev_t pdev, const char *name)
{
	dev_t cdev;

	if (strcmp(devtoname(pdev), name) == 0)
		return (1);
	LIST_FOREACH(cdev, &pdev->si_children, si_siblings)
		if (strcmp(devtoname(cdev), name) == 0)
			return (1);
	return (0);
}

void
dev_depends(dev_t pdev, dev_t cdev)
{

	cdev->si_parent = pdev;
	cdev->si_flags |= SI_CHILD;
	LIST_INSERT_HEAD(&pdev->si_children, cdev, si_siblings);
}

dev_t
make_dev_alias(dev_t pdev, const char *fmt, ...)
{
	dev_t	dev;
	va_list ap;
	int i;

	dev = allocdev();
	dev->si_flags |= SI_ALIAS;
	dev->si_flags |= SI_NAMED;
	dev_depends(pdev, dev);
	va_start(ap, fmt);
	i = kvprintf(fmt, NULL, dev->si_name, 32, ap);
	dev->si_name[i] = '\0';
	va_end(ap);

	if (devfs_create_hook)
		devfs_create_hook(dev);
	return (dev);
}

void
revoke_and_destroy_dev(dev_t dev)
{
	struct vnode *vp;

	GIANT_REQUIRED;

	vp = SLIST_FIRST(&dev->si_hlist);
	if (vp != NULL)
		VOP_REVOKE(vp, REVOKEALL);
	destroy_dev(dev);
}

void
destroy_dev(dev_t dev)
{
	
	if (!(dev->si_flags & SI_NAMED)) {
		printf( "WARNING: Driver mistake: destroy_dev on %d/%d\n",
		    major(dev), minor(dev));
		panic("don't do that");
		return;
	}
		
	if (devfs_destroy_hook)
		devfs_destroy_hook(dev);
	if (dev->si_flags & SI_CHILD) {
		LIST_REMOVE(dev, si_siblings);
		dev->si_flags &= ~SI_CHILD;
	}
	while (!LIST_EMPTY(&dev->si_children))
		destroy_dev(LIST_FIRST(&dev->si_children));
	dev->si_drv1 = 0;
	dev->si_drv2 = 0;
	dev->si_devsw = 0;
	dev->si_flags &= ~SI_NAMED;
	dev->si_flags &= ~SI_ALIAS;
	freedev(dev);
}

const char *
devtoname(dev_t dev)
{
	char *p;
	int mynor;

	if (dev->si_name[0] == '#' || dev->si_name[0] == '\0') {
		p = dev->si_name;
		if (devsw(dev))
			sprintf(p, "#%s/", devsw(dev)->d_name);
		else
			sprintf(p, "#%d/", major(dev));
		p += strlen(p);
		mynor = minor(dev);
		if (mynor < 0 || mynor > 255)
			sprintf(p, "%#x", (u_int)mynor);
		else
			sprintf(p, "%d", mynor);
	}
	return (dev->si_name);
}

int
dev_stdclone(char *name, char **namep, const char *stem, int *unit)
{
	int u, i;

	i = strlen(stem);
	if (bcmp(stem, name, i) != 0)
		return (0);
	if (!isdigit(name[i]))
		return (0);
	u = 0;
	if (name[i] == '0' && isdigit(name[i+1]))
		return (0);
	while (isdigit(name[i])) {
		u *= 10;
		u += name[i++] - '0';
	}
	*unit = u;
	if (namep)
		*namep = &name[i];
	if (name[i]) 
		return (2);
	return (1);
}

/*
 * Helper sysctl for devname(3).  We're given a {u}dev_t and return
 * the name, if any, registered by the device driver.
 */
static int
sysctl_devname(SYSCTL_HANDLER_ARGS)
{
	int error;
	udev_t ud;
	dev_t dev;

	error = SYSCTL_IN(req, &ud, sizeof (ud));
	if (error)
		return (error);
	if (ud == NOUDEV)
		return(EINVAL);
	dev = makedev(umajor(ud), uminor(ud));
	if (dev->si_name[0] == '\0')
		error = ENOENT;
	else
		error = SYSCTL_OUT(req, dev->si_name, strlen(dev->si_name) + 1);
	freedev(dev);
	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, devname, CTLTYPE_OPAQUE|CTLFLAG_RW|CTLFLAG_ANYBODY,
	NULL, 0, sysctl_devname, "", "devname(3) handler");

/*
 * Set ready_for_devs; prior to this point, device creation is not allowed.
 */	
static void
dev_set_ready(void *junk)
{
	ready_for_devs = 1;
}

SYSINIT(dev_ready, SI_SUB_DEVFS, SI_ORDER_FIRST, dev_set_ready, NULL);
