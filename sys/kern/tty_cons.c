/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	from: @(#)cons.c	7.2 (Berkeley) 5/9/91
 * $FreeBSD$
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <ddb/ddb.h>

#include <machine/cpu.h>

static	d_open_t	cnopen;
static	d_close_t	cnclose;
static	d_read_t	cnread;
static	d_write_t	cnwrite;
static	d_ioctl_t	cnioctl;
static	d_poll_t	cnpoll;
static	d_kqfilter_t	cnkqfilter;

#define	CDEV_MAJOR	0
static struct cdevsw cn_cdevsw = {
	/* open */	cnopen,
	/* close */	cnclose,
	/* read */	cnread,
	/* write */	cnwrite,
	/* ioctl */	cnioctl,
	/* poll */	cnpoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"console",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY | D_KQFILTER,
	/* kqfilter */	cnkqfilter,
};

struct cn_device {
	STAILQ_ENTRY(cn_device) cnd_next;
	char		cnd_name[16];
	struct		vnode *cnd_vp;
	struct		consdev *cnd_cn;
};

#define CNDEVPATHMAX	32
#define CNDEVTAB_SIZE	4
static struct cn_device cn_devtab[CNDEVTAB_SIZE];
static STAILQ_HEAD(, cn_device) cn_devlist =
    STAILQ_HEAD_INITIALIZER(cn_devlist);

#define CND_INVALID(cnd, td) 						\
	(cnd == NULL || cnd->cnd_vp == NULL ||				\
	    (cnd->cnd_vp->v_type == VBAD && !cn_devopen(cnd, td, 1)))

static udev_t	cn_udev_t;
SYSCTL_OPAQUE(_machdep, CPU_CONSDEV, consdev, CTLFLAG_RD,
	&cn_udev_t, sizeof cn_udev_t, "T,dev_t", "");

int	cons_unavail = 0;	/* XXX:
				 * physical console not available for
				 * input (i.e., it is in graphics mode)
				 */
static int cn_mute;
static int openflag;			/* how /dev/console was opened */
static int cn_is_open;
static dev_t cn_devfsdev;		/* represents the device private info */
static u_char console_pausing;		/* pause after each line during probe */
static char *console_pausestr=
"<pause; press any key to proceed to next line or '.' to end pause mode>";

void	cndebug(char *);

CONS_DRIVER(cons, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
SET_DECLARE(cons_set, struct consdev);

void
cninit(void)
{
	struct consdev *best_cn, *cn, **list;

	/*
	 * Check if we should mute the console (for security reasons perhaps)
	 * It can be changes dynamically using sysctl kern.consmute
	 * once we are up and going.
	 * 
	 */
        cn_mute = ((boothowto & (RB_MUTE
			|RB_SINGLE
			|RB_VERBOSE
			|RB_ASKNAME
			|RB_CONFIG)) == RB_MUTE);

	/*
	 * Find the first console with the highest priority.
	 */
	best_cn = NULL;
	SET_FOREACH(list, cons_set) {
		cn = *list;
		if (cn->cn_probe == NULL)
			continue;
		cn->cn_probe(cn);
		if (cn->cn_pri == CN_DEAD)
			continue;
		if (best_cn == NULL || cn->cn_pri > best_cn->cn_pri)
			best_cn = cn;
		if (boothowto & RB_MULTIPLE) {
			/*
			 * Initialize console, and attach to it.
			 */
			cnadd(cn);
			cn->cn_init(cn);
		}
	}
	if (best_cn == NULL)
		return;
	if ((boothowto & RB_MULTIPLE) == 0) {
		cnadd(best_cn);
		best_cn->cn_init(best_cn);
	}
	if (boothowto & RB_PAUSE)
		console_pausing = 1;
	/*
	 * Make the best console the preferred console.
	 */
	cnselect(best_cn);
}

void
cninit_finish()
{
	console_pausing = 0;
} 

/* add a new physical console to back the virtual console */
int
cnadd(struct consdev *cn)
{
	struct cn_device *cnd;
	int i;

	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next)
		if (cnd->cnd_cn == cn)
			return (0);
	for (i = 0; i < CNDEVTAB_SIZE; i++) {
		cnd = &cn_devtab[i];
		if (cnd->cnd_cn == NULL)
			break;
	}
	if (cnd->cnd_cn != NULL)
		return (ENOMEM);
	cnd->cnd_cn = cn;
	STAILQ_INSERT_TAIL(&cn_devlist, cnd, cnd_next);
	return (0);
}

void
cnremove(struct consdev *cn)
{
	struct cn_device *cnd;

	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next) {
		if (cnd->cnd_cn != cn)
			continue;
		STAILQ_REMOVE(&cn_devlist, cnd, cn_device, cnd_next);
		if (cnd->cnd_vp != NULL)
			vn_close(cnd->cnd_vp, openflag, NOCRED, NULL);
		cnd->cnd_vp = NULL;
		cnd->cnd_cn = NULL;
		cnd->cnd_name[0] = '\0';
#if 0
		/*
		 * XXX
		 * syscons gets really confused if console resources are
		 * freed after the system has initialized.
		 */
		if (cn->cn_term != NULL)
			cn->cn_term(cn);
#endif
		return;
	}
}

void
cnselect(struct consdev *cn)
{
	struct cn_device *cnd;

	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next) {
		if (cnd->cnd_cn != cn)
			continue;
		if (cnd == STAILQ_FIRST(&cn_devlist))
			return;
		STAILQ_REMOVE(&cn_devlist, cnd, cn_device, cnd_next);
		STAILQ_INSERT_HEAD(&cn_devlist, cnd, cnd_next);
		return;
	}
}

void
cndebug(char *str)
{
	int i, len;

	len = strlen(str);
	cnputc('>'); cnputc('>'); cnputc('>'); cnputc(' '); 
	for (i = 0; i < len; i++)
		cnputc(str[i]);
	cnputc('\n');
}

static int
sysctl_kern_console(SYSCTL_HANDLER_ARGS)
{
	struct cn_device *cnd;
	struct consdev *cp, **list;
	char *name, *p;
	int delete, len, error;

	len = 2;
	SET_FOREACH(list, cons_set) {
		cp = *list;
		if (cp->cn_dev != NULL)
			len += strlen(devtoname(cp->cn_dev)) + 1;
	}
	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next)
		len += strlen(devtoname(cnd->cnd_cn->cn_dev)) + 1;
	len = len > CNDEVPATHMAX ? len : CNDEVPATHMAX;
	MALLOC(name, char *, len, M_TEMP, M_WAITOK | M_ZERO);
	p = name;
	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next)
		p += sprintf(p, "%s,", devtoname(cnd->cnd_cn->cn_dev));
	*p++ = '/';
	SET_FOREACH(list, cons_set) {
		cp = *list;
		if (cp->cn_dev != NULL)
			p += sprintf(p, "%s,", devtoname(cp->cn_dev));
	}
	error = sysctl_handle_string(oidp, name, len, req);
	if (error == 0 && req->newptr != NULL) {
		p = name;
		error = ENXIO;
		delete = 0;
		if (*p == '-') {
			delete = 1;
			p++;
		}
		SET_FOREACH(list, cons_set) {
			cp = *list;
			if (cp->cn_dev == NULL ||
			    strcmp(p, devtoname(cp->cn_dev)) != 0)
				continue;
			if (delete) {
				cnremove(cp);
				error = 0;
			} else {
				error = cnadd(cp);
				if (error == 0)
					cnselect(cp);
			}
			break;
		}
	}
	FREE(name, M_TEMP);
	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, console, CTLTYPE_STRING|CTLFLAG_RW,
	0, 0, sysctl_kern_console, "A", "Console device control");

/*
 * User has changed the state of the console muting.
 * This may require us to open or close the device in question.
 */
static int
sysctl_kern_consmute(SYSCTL_HANDLER_ARGS)
{
	int error;
	int ocn_mute;

	ocn_mute = cn_mute;
	error = sysctl_handle_int(oidp, &cn_mute, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (ocn_mute && !cn_mute && cn_is_open)
		error = cnopen(NODEV, openflag, 0, curthread);
	else if (!ocn_mute && cn_mute && cn_is_open) {
		error = cnclose(NODEV, openflag, 0, curthread);
		cn_is_open = 1;		/* XXX hack */
	}
	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, consmute, CTLTYPE_INT|CTLFLAG_RW,
	0, sizeof(cn_mute), sysctl_kern_consmute, "I", "");

static int
cn_devopen(struct cn_device *cnd, struct thread *td, int forceopen)
{
	char path[CNDEVPATHMAX];
	struct nameidata nd;
	struct vnode *vp;
	dev_t dev;
	int error;

	if ((vp = cnd->cnd_vp) != NULL) {
		if (!forceopen && vp->v_type != VBAD) {
			dev = vp->v_rdev;
			return ((*devsw(dev)->d_open)(dev, openflag, 0, td));
		}
		cnd->cnd_vp = NULL;
		vn_close(vp, openflag, td->td_ucred, td);
	}
	if (cnd->cnd_name[0] == '\0')
		strncpy(cnd->cnd_name, devtoname(cnd->cnd_cn->cn_dev),
		    sizeof(cnd->cnd_name));
	snprintf(path, sizeof(path), "/dev/%s", cnd->cnd_name);
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, path, td);
	error = vn_open(&nd, &openflag, 0);
	if (error == 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		VOP_UNLOCK(nd.ni_vp, 0, td);
		if (nd.ni_vp->v_type == VCHR)
			cnd->cnd_vp = nd.ni_vp;
		else
			vn_close(nd.ni_vp, openflag, td->td_ucred, td);
	}
	return (cnd->cnd_vp != NULL);
}

static int
cnopen(dev_t dev, int flag, int mode, struct thread *td)
{
	struct cn_device *cnd;

	openflag = flag | FWRITE;	/* XXX */
	cn_is_open = 1;			/* console is logically open */
	if (cn_mute)
		return (0);
	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next)
		cn_devopen(cnd, td, 0);
	return (0);
}

static int
cnclose(dev_t dev, int flag, int mode, struct thread *td)
{
	struct cn_device *cnd;
	struct vnode *vp;

	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next) {
		if ((vp = cnd->cnd_vp) == NULL)
			continue; 
		cnd->cnd_vp = NULL;
		vn_close(vp, openflag, td->td_ucred, td);
	}
	cn_is_open = 0;
	return (0);
}

static int
cnread(dev_t dev, struct uio *uio, int flag)
{
	struct cn_device *cnd;

	cnd = STAILQ_FIRST(&cn_devlist);
	if (cn_mute || CND_INVALID(cnd, curthread))
		return (0);
	dev = cnd->cnd_vp->v_rdev;
	return ((*devsw(dev)->d_read)(dev, uio, flag));
}

static int
cnwrite(dev_t dev, struct uio *uio, int flag)
{
	struct cn_device *cnd;

	cnd = STAILQ_FIRST(&cn_devlist);
	if (cn_mute || CND_INVALID(cnd, curthread))
		goto done;
	if (constty)
		dev = constty->t_dev;
	else
		dev = cnd->cnd_vp->v_rdev;
	if (dev != NULL) {
		log_console(uio);
		return ((*devsw(dev)->d_write)(dev, uio, flag));
	}
done:
	uio->uio_resid = 0; /* dump the data */
	return (0);
}

static int
cnioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	struct cn_device *cnd;
	int error;

	cnd = STAILQ_FIRST(&cn_devlist);
	if (cn_mute || CND_INVALID(cnd, td))
		return (0);
	/*
	 * Superuser can always use this to wrest control of console
	 * output from the "virtual" console.
	 */
	if (cmd == TIOCCONS && constty) {
		error = suser_td(td);
		if (error)
			return (error);
		constty = NULL;
		return (0);
	}
	dev = cnd->cnd_vp->v_rdev;
	if (dev != NULL)
		return ((*devsw(dev)->d_ioctl)(dev, cmd, data, flag, td));
	return (0);
}

/*
 * XXX
 * poll/kqfilter do not appear to be correct
 */
static int
cnpoll(dev_t dev, int events, struct thread *td)
{
	struct cn_device *cnd;

	cnd = STAILQ_FIRST(&cn_devlist);
	if (cn_mute || CND_INVALID(cnd, td))
		return (0);
	dev = cnd->cnd_vp->v_rdev;
	if (dev != NULL)
		return ((*devsw(dev)->d_poll)(dev, events, td));
	return (0);
}

static int
cnkqfilter(dev_t dev, struct knote *kn)
{
	struct cn_device *cnd;

	cnd = STAILQ_FIRST(&cn_devlist);
	if (cn_mute || CND_INVALID(cnd, curthread))
		return (1);
	dev = cnd->cnd_vp->v_rdev;
	if (dev != NULL)
		return ((*devsw(dev)->d_kqfilter)(dev, kn));
	return (1);
}

/*
 * Low level console routines.
 */
int
cngetc(void)
{
	int c;

	if (cn_mute)
		return (-1);
	while ((c = cncheckc()) == -1)
		;
	if (c == '\r')
		c = '\n';		/* console input is always ICRNL */
	return (c);
}

int
cncheckc(void)
{
	struct cn_device *cnd;
	struct consdev *cn;
	int c;

	if (cn_mute)
		return (-1);
	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next) {
		cn = cnd->cnd_cn;
		c = cn->cn_checkc(cn->cn_dev);
		if (c != -1) {
			return (c);
		}
	}
	return (-1);
}

void
cnputc(int c)
{
	struct cn_device *cnd;
	struct consdev *cn;
	char *cp;

	if (cn_mute || c == '\0')
		return;
	STAILQ_FOREACH(cnd, &cn_devlist, cnd_next) {
		cn = cnd->cnd_cn;
		if (c == '\n')
			cn->cn_putc(cn->cn_dev, '\r');
		cn->cn_putc(cn->cn_dev, c);
	}
#ifdef DDB
	if (console_pausing && !db_active && (c == '\n')) {
#else
	if (console_pausing && (c == '\n')) {
#endif
		for (cp = console_pausestr; *cp != '\0'; cp++)
			cnputc(*cp);
		if (cngetc() == '.')
			console_pausing = 0;
		cnputc('\r');
		for (cp = console_pausestr; *cp != '\0'; cp++)
			cnputc(' ');
		cnputc('\r');
	}
}

void
cndbctl(int on)
{
	struct cn_device *cnd;
	struct consdev *cn;
	static int refcount;

	if (!on)
		refcount--;
	if (refcount == 0)
		STAILQ_FOREACH(cnd, &cn_devlist, cnd_next) {
			cn = cnd->cnd_cn;
			if (cn->cn_dbctl != NULL)
				cn->cn_dbctl(cn->cn_dev, on);
		}
	if (on)
		refcount++;
}

static void
cn_drvinit(void *unused)
{

	cn_devfsdev = make_dev(&cn_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	    "console");
}

SYSINIT(cndev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,cn_drvinit,NULL)
