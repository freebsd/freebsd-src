/*
 * Copyright (c) 1995 Ugen J.S.Antsilevich
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 * Snoop stuff.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filio.h>
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
#include <sys/ioctl_compat.h>
#endif
#include <sys/malloc.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/poll.h>
#include <sys/kernel.h>
#include <sys/snoop.h>
#include <sys/vnode.h>
#include <sys/conf.h>

static	d_open_t	snpopen;
static	d_close_t	snpclose;
static	d_read_t	snpread;
static	d_write_t	snpwrite;
static	d_ioctl_t	snpioctl;
static	d_poll_t	snppoll;

#define CDEV_MAJOR 53
static struct cdevsw snp_cdevsw = {
	/* open */	snpopen,
	/* close */	snpclose,
	/* read */	snpread,
	/* write */	snpwrite,
	/* ioctl */	snpioctl,
	/* poll */	snppoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"snp",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};


#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

static MALLOC_DEFINE(M_SNP, "snp", "Snoop device data");

#define ttytosnp(t) (struct snoop *)(t)->t_sc
static struct tty	*snpdevtotty __P((dev_t dev));
static int		snp_detach __P((struct snoop *snp));

/*
 * The number of the "snoop" line discipline.  This gets determined at
 * module load time.
 */
static int mylinedisc;

static int
dsnwrite(struct tty *tp, struct uio *uio, int flag)
{
	struct snoop *snp = ttytosnp(tp);
	int error = 0;
	char ibuf[1024];
	int ilen;
	struct iovec iov;
	struct uio uio2;

	while (uio->uio_resid) {
		ilen = MIN(sizeof(ibuf), uio->uio_resid);
		error = uiomove(ibuf, ilen, uio);
		if (error)
			break;
		snpin(snp, ibuf, ilen);
		/* Hackish, but I think it's the least of all evils. */
		iov.iov_base = ibuf;
		iov.iov_len = ilen;
		uio2.uio_iov = &iov;
		uio2.uio_iovcnt = 1;
		uio2.uio_offset = 0;
		uio2.uio_resid = ilen;
		uio2.uio_segflg = UIO_SYSSPACE;
		uio2.uio_rw = UIO_WRITE;
		uio2.uio_procp = uio->uio_procp;
		error = ttwrite(tp, &uio2, flag);
		if (error)
			break;
	}
	return (error);
}

/*
 * XXX should there be a global version of this?
 */
static int
l_nullioctl(struct tty *tp, u_long cmd, char *data, int flags, struct proc *p)
{

	return (ENOIOCTL);
}

static struct linesw snpdisc = {
	ttyopen,	ttylclose,	ttread,		dsnwrite,
	l_nullioctl,	ttyinput,	ttstart,	ttymodem };

static struct tty *
snpdevtotty (dev)
	dev_t		dev;
{
	struct cdevsw	*cdp;

	cdp = devsw(dev);
	if (cdp && cdp->d_flags & D_TTY)
		return (dev->si_tty);
	return (NULL);
}

#define SNP_INPUT_BUF	5	/* This is even too much, the maximal
				 * interactive mode write is 3 bytes
				 * length for function keys...
				 */

static	int
snpwrite(dev, uio, flag)
	dev_t           dev;
	struct uio     *uio;
	int             flag;
{
	int             len, i, error;
	struct snoop   *snp = dev->si_drv1;
	struct tty     *tp;
	char		c[SNP_INPUT_BUF];

	if (snp->snp_tty == NULL)
		return (EIO);

	tp = snp->snp_tty;

	if ((tp->t_sc == snp) && (tp->t_state & TS_SNOOP) &&
	    tp->t_line == mylinedisc)
		goto tty_input;

	printf("Snoop: attempt to write to bad tty.\n");
	return (EIO);

tty_input:
	if (!(tp->t_state & TS_ISOPEN))
		return (EIO);

	while (uio->uio_resid > 0) {
		len = MIN(uio->uio_resid, SNP_INPUT_BUF);
		if ((error = uiomove(c, len, uio)) != 0)
			return (error);
		for (i=0; i < len; i++) {
			if (ttyinput(c[i], tp))
				return (EIO);
		}
	}
	return 0;

}


static	int
snpread(dev, uio, flag)
	dev_t           dev;
	struct uio     *uio;
	int             flag;
{
	struct snoop   *snp = dev->si_drv1; 
	int             len, n, nblen, s, error = 0;
	caddr_t         from;
	char           *nbuf;

	KASSERT(snp->snp_len + snp->snp_base <= snp->snp_blen,
	    ("snoop buffer error"));

	if (snp->snp_tty == NULL)
		return (EIO);

	snp->snp_flags &= ~SNOOP_RWAIT;

	do {
		if (snp->snp_len == 0) {
			if (flag & IO_NDELAY)
				return (EWOULDBLOCK);
			snp->snp_flags |= SNOOP_RWAIT;
			tsleep((caddr_t)snp, (PZERO + 1) | PCATCH, "snprd", 0);
		}
	} while (snp->snp_len == 0);

	n = snp->snp_len;

	while (snp->snp_len > 0 && uio->uio_resid > 0 && error == 0) {
		len = MIN(uio->uio_resid, snp->snp_len);
		from = (caddr_t)(snp->snp_buf + snp->snp_base);
		if (len == 0)
			break;

		error = uiomove(from, len, uio);
		snp->snp_base += len;
		snp->snp_len -= len;
	}
	if ((snp->snp_flags & SNOOP_OFLOW) && (n < snp->snp_len)) {
		snp->snp_flags &= ~SNOOP_OFLOW;
	}
	s = spltty();
	nblen = snp->snp_blen;
	if (((nblen / 2) >= SNOOP_MINLEN) && (nblen / 2) >= snp->snp_len) {
		while (((nblen / 2) >= snp->snp_len) && ((nblen / 2) >= SNOOP_MINLEN))
			nblen = nblen / 2;
		if ((nbuf = malloc(nblen, M_SNP, M_NOWAIT)) != NULL) {
			bcopy(snp->snp_buf + snp->snp_base, nbuf, snp->snp_len);
			free(snp->snp_buf, M_SNP);
			snp->snp_buf = nbuf;
			snp->snp_blen = nblen;
			snp->snp_base = 0;
		}
	}
	splx(s);

	return error;
}

int
snpinc(struct snoop *snp, char c)
{
        char    buf;

	buf = c;
        return (snpin(snp, &buf, 1));
}


int
snpin(snp, buf, n)
	struct snoop   *snp;
	char           *buf;
	int             n;
{
	int             s_free, s_tail;
	int             s, len, nblen;
	caddr_t         from, to;
	char           *nbuf;

	KASSERT(n >= 0, ("negative snoop char count"));

	if (n == 0)
		return 0;

	if (snp->snp_flags & SNOOP_DOWN) {
		printf("Snoop: more data to down interface.\n");
		return 0;
	}

	if (snp->snp_flags & SNOOP_OFLOW) {
		printf("Snoop: buffer overflow.\n");
		/*
		 * On overflow we just repeat the standart close
		 * procedure...yes , this is waste of space but.. Then next
		 * read from device will fail if one would recall he is
		 * snooping and retry...
		 */

		return (snpdown(snp));
	}
	s_tail = snp->snp_blen - (snp->snp_len + snp->snp_base);
	s_free = snp->snp_blen - snp->snp_len;


	if (n > s_free) {
		s = spltty();
		nblen = snp->snp_blen;
		while ((n > s_free) && ((nblen * 2) <= SNOOP_MAXLEN)) {
			nblen = snp->snp_blen * 2;
			s_free = nblen - (snp->snp_len + snp->snp_base);
		}
		if ((n <= s_free) && (nbuf = malloc(nblen, M_SNP, M_NOWAIT))) {
			bcopy(snp->snp_buf + snp->snp_base, nbuf, snp->snp_len);
			free(snp->snp_buf, M_SNP);
			snp->snp_buf = nbuf;
			snp->snp_blen = nblen;
			snp->snp_base = 0;
		} else {
			snp->snp_flags |= SNOOP_OFLOW;
			if (snp->snp_flags & SNOOP_RWAIT) {
				snp->snp_flags &= ~SNOOP_RWAIT;
				wakeup((caddr_t)snp);
			}
			splx(s);
			return 0;
		}
		splx(s);
	}
	if (n > s_tail) {
		from = (caddr_t)(snp->snp_buf + snp->snp_base);
		to = (caddr_t)(snp->snp_buf);
		len = snp->snp_len;
		bcopy(from, to, len);
		snp->snp_base = 0;
	}
	to = (caddr_t)(snp->snp_buf + snp->snp_base + snp->snp_len);
	bcopy(buf, to, n);
	snp->snp_len += n;

	if (snp->snp_flags & SNOOP_RWAIT) {
		snp->snp_flags &= ~SNOOP_RWAIT;
		wakeup((caddr_t)snp);
	}
	selwakeup(&snp->snp_sel);
	snp->snp_sel.si_pid = 0;

	return n;
}

static	int
snpopen(dev, flag, mode, p)
	dev_t           dev;
	int             flag, mode;
	struct proc    *p;
{
	struct snoop   *snp;
	int error;

	if ((error = suser(p)) != 0)
		return (error);

	if (dev->si_drv1 == NULL) {
		if (!(dev->si_flags & SI_NAMED))
			make_dev(&snp_cdevsw, minor(dev), UID_ROOT, GID_WHEEL,
			    0600, "snp%d", dev2unit(dev));
		dev->si_drv1 = snp = malloc(sizeof(*snp), M_SNP, M_WAITOK|M_ZERO);
	} else
		return (EBUSY);

	/*
	 * We intentionally do not OR flags with SNOOP_OPEN, but set them so
	 * all previous settings (especially SNOOP_OFLOW) will be cleared.
	 */
	snp->snp_flags = SNOOP_OPEN;

	snp->snp_buf = malloc(SNOOP_MINLEN, M_SNP, M_WAITOK);
	snp->snp_blen = SNOOP_MINLEN;
	snp->snp_base = 0;
	snp->snp_len = 0;

	/*
	 * snp_tty == NULL  is for inactive snoop devices.
	 */
	snp->snp_tty = NULL;
	snp->snp_target = NODEV;
	return (0);
}


static int
snp_detach(snp)
	struct snoop   *snp;
{
	struct tty     *tp;

	snp->snp_base = 0;
	snp->snp_len = 0;

	/*
	 * If line disc. changed we do not touch this pointer, SLIP/PPP will
	 * change it anyway.
	 */

	if (snp->snp_tty == NULL)
		goto detach_notty;

	tp = snp->snp_tty;

	if (tp && (tp->t_sc == snp) && (tp->t_state & TS_SNOOP) &&
	    tp->t_line == mylinedisc) {
		tp->t_sc = NULL;
		tp->t_state &= ~TS_SNOOP;
		tp->t_line = snp->snp_olddisc;
	} else
		printf("Snoop: bad attached tty data.\n");

	snp->snp_tty = NULL;
	snp->snp_target = NODEV;

detach_notty:
	selwakeup(&snp->snp_sel);
	snp->snp_sel.si_pid = 0;
	if ((snp->snp_flags & SNOOP_OPEN) == 0) 
		free(snp, M_SNP);

	return (0);
}

static	int
snpclose(dev, flags, fmt, p)
	dev_t           dev;
	int             flags;
	int             fmt;
	struct proc    *p;
{
	struct snoop   *snp = dev->si_drv1;

	snp->snp_blen = 0;
	free(snp->snp_buf, M_SNP);
	snp->snp_flags &= ~SNOOP_OPEN;
	dev->si_drv1 = NULL;
	destroy_dev(dev);

	return (snp_detach(snp));
}

int
snpdown(snp)
	struct snoop	*snp;
{

	if (snp->snp_blen != SNOOP_MINLEN) {
		free(snp->snp_buf, M_SNP);
		snp->snp_buf = malloc(SNOOP_MINLEN, M_SNP, M_WAITOK);
		snp->snp_blen = SNOOP_MINLEN;
	}
	snp->snp_flags |= SNOOP_DOWN;

	return (snp_detach(snp));
}


static	int
snpioctl(dev, cmd, data, flags, p)
	dev_t           dev;
	u_long          cmd;
	caddr_t         data;
	int             flags;
	struct proc    *p;
{
	dev_t		tdev;
	struct snoop   *snp = dev->si_drv1;
	struct tty     *tp, *tpo;
	int s;

	switch (cmd) {
	case SNPSTTY:
		tdev = udev2dev(*((udev_t *)data), 0);
		if (tdev == NODEV)
			return (snpdown(snp));

		tp = snpdevtotty(tdev);
		if (!tp)
			return (EINVAL);

		s = spltty();

		if (snp->snp_target == NODEV) {
			tpo = snp->snp_tty;
			if (tpo)
				tpo->t_state &= ~TS_SNOOP;
		}

		tp->t_sc = (caddr_t)snp;
		tp->t_state |= TS_SNOOP;
		snp->snp_olddisc = tp->t_line;
		tp->t_line = mylinedisc;
		snp->snp_tty = tp;
		snp->snp_target = tdev;

		/*
		 * Clean overflow and down flags -
		 * we'll have a chance to get them in the future :)))
		 */
		snp->snp_flags &= ~SNOOP_OFLOW;
		snp->snp_flags &= ~SNOOP_DOWN;
		splx(s);
		break;

	case SNPGTTY:
		/*
		 * We keep snp_target field specially to make
		 * SNPGTTY happy, else we can't know what is device
		 * major/minor for tty.
		 */
		*((dev_t *)data) = snp->snp_target;
		break;

	case FIONBIO:
		break;

	case FIOASYNC:
		if (*(int *)data)
			snp->snp_flags |= SNOOP_ASYNC;
		else
			snp->snp_flags &= ~SNOOP_ASYNC;
		break;

	case FIONREAD:
		s = spltty();
		if (snp->snp_tty != NULL)
			*(int *)data = snp->snp_len;
		else
			if (snp->snp_flags & SNOOP_DOWN) {
				if (snp->snp_flags & SNOOP_OFLOW)
					*(int *)data = SNP_OFLOW;
				else
					*(int *)data = SNP_TTYCLOSE;
			} else {
				*(int *)data = SNP_DETACH;
			}
		splx(s);
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}


static	int
snppoll(dev, events, p)
	dev_t           dev;
	int             events;
	struct proc    *p;
{
	struct snoop   *snp = dev->si_drv1;
	int		revents = 0;


	/*
	 * If snoop is down, we don't want to poll() forever so we return 1.
	 * Caller should see if we down via FIONREAD ioctl().  The last should
	 * return -1 to indicate down state.
	 */
	if (events & (POLLIN | POLLRDNORM)) {
		if (snp->snp_flags & SNOOP_DOWN || snp->snp_len > 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(p, &snp->snp_sel);
	}
	return (revents);
}

static void
snp_clone(void *arg, char *name, int namelen, dev_t *dev)
{
	int u;

	if (*dev != NODEV)
		return;
	if (dev_stdclone(name, NULL, "snp", &u) != 1)
		return;
	*dev = make_dev(&snp_cdevsw, unit2minor(u), UID_ROOT, GID_WHEEL, 0600,
	    "snp%d", u);
	return;
}

static int
snp_modevent(module_t mod, int type, void *data)
{
	static eventhandler_tag eh_tag = NULL;

	switch (type) {
	case MOD_LOAD:
		eh_tag = EVENTHANDLER_REGISTER(dev_clone, snp_clone, 0, 1000);
		mylinedisc = ldisc_register(LDISC_LOAD, &snpdisc);
		cdevsw_add(&snp_cdevsw);
		break;
	case MOD_UNLOAD:
		EVENTHANDLER_DEREGISTER(dev_clone, eh_tag);
		ldisc_deregister(mylinedisc);
		cdevsw_remove(&snp_cdevsw);
		break;
	default:
		break;
	}
	return 0;
}

static moduledata_t snp_mod = {
        "snp",
        snp_modevent,
        NULL
};
DECLARE_MODULE(snp, snp_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE+CDEV_MAJOR);
