/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/malloc.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/poll.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/snoop.h>
#include <sys/uio.h>

static	l_close_t	snplclose;
static	l_write_t	snplwrite;
static	d_open_t	snpopen;
static	d_close_t	snpclose;
static	d_read_t	snpread;
static	d_write_t	snpwrite;
static	d_ioctl_t	snpioctl;
static	d_poll_t	snppoll;

static struct cdevsw snp_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_PSEUDO | D_NEEDGIANT,
	.d_open =	snpopen,
	.d_close =	snpclose,
	.d_read =	snpread,
	.d_write =	snpwrite,
	.d_ioctl =	snpioctl,
	.d_poll =	snppoll,
	.d_name =	"snp",
};

static struct linesw snpdisc = {
	.l_open =	tty_open,
	.l_close =	snplclose,
	.l_read =	ttread,
	.l_write =	snplwrite,
	.l_ioctl =	l_nullioctl,
	.l_rint =	ttyinput,
	.l_start =	ttstart,
	.l_modem =	ttymodem
};

/*
 * This is the main snoop per-device structure.
 */
struct snoop {
	LIST_ENTRY(snoop)	snp_list;	/* List glue. */
	int			snp_unit;	/* Device number. */
	struct cdev *snp_target;	/* Target tty device. */
	struct tty		*snp_tty;	/* Target tty pointer. */
	u_long			 snp_len;	/* Possible length. */
	u_long			 snp_base;	/* Data base. */
	u_long			 snp_blen;	/* Used length. */
	caddr_t			 snp_buf;	/* Allocation pointer. */
	int			 snp_flags;	/* Flags. */
	struct selinfo		 snp_sel;	/* Select info. */
	int			 snp_olddisc;	/* Old line discipline. */
};

/*
 * Possible flags.
 */
#define SNOOP_ASYNC		0x0002
#define SNOOP_OPEN		0x0004
#define SNOOP_RWAIT		0x0008
#define SNOOP_OFLOW		0x0010
#define SNOOP_DOWN		0x0020

/*
 * Other constants.
 */
#define SNOOP_MINLEN		(4*1024)	/* This should be power of 2.
						 * 4K tested to be the minimum
						 * for which on normal tty
						 * usage there is no need to
						 * allocate more.
						 */
#define SNOOP_MAXLEN		(64*1024)	/* This one also,64K enough
						 * If we grow more,something
						 * really bad in this world..
						 */

static MALLOC_DEFINE(M_SNP, "snp", "Snoop device data");
/*
 * The number of the "snoop" line discipline.  This gets determined at
 * module load time.
 */
static int snooplinedisc;

static LIST_HEAD(, snoop) snp_sclist = LIST_HEAD_INITIALIZER(&snp_sclist);
static struct clonedevs	  *snpclones;

static struct tty	*snpdevtotty(struct cdev *dev);
static void		snp_clone(void *arg, struct ucred *cred, char *name,
			    int namelen, struct cdev **dev);
static int		snp_detach(struct snoop *snp);
static int		snp_down(struct snoop *snp);
static int		snp_in(struct snoop *snp, char *buf, int n);
static int		snp_modevent(module_t mod, int what, void *arg);
static struct snoop	*ttytosnp(struct tty *);

static struct snoop *
ttytosnp(struct tty *tp)
{
	struct snoop *snp;
	
	LIST_FOREACH(snp, &snp_sclist, snp_list) {
		if (snp->snp_tty == tp)
			return (snp);
	}
	return (NULL);
}

static int
snplclose(tp, flag)
	struct tty *tp;
	int flag;
{
	struct snoop *snp;
	int error;

	snp = ttytosnp(tp);
	error = snp_down(snp);
	if (error != 0)
		return (error);
	error = ttylclose(tp, flag);
	return (error);
}

static int
snplwrite(tp, uio, flag)
	struct tty *tp;
	struct uio *uio;
	int flag;
{
	struct iovec iov;
	struct uio uio2;
	struct snoop *snp;
	int error, ilen;
	char *ibuf;

	error = 0;
	ibuf = NULL;
	snp = ttytosnp(tp);
	while (uio->uio_resid > 0) {
		ilen = imin(512, uio->uio_resid);
		ibuf = malloc(ilen, M_SNP, M_WAITOK);
		error = uiomove(ibuf, ilen, uio);
		if (error != 0)
			break;
		snp_in(snp, ibuf, ilen);
		/* Hackish, but probably the least of all evils. */
		iov.iov_base = ibuf;
		iov.iov_len = ilen;
		uio2.uio_iov = &iov;
		uio2.uio_iovcnt = 1;
		uio2.uio_offset = 0;
		uio2.uio_resid = ilen;
		uio2.uio_segflg = UIO_SYSSPACE;
		uio2.uio_rw = UIO_WRITE;
		uio2.uio_td = uio->uio_td;
		error = ttwrite(tp, &uio2, flag);
		if (error != 0)
			break;
		free(ibuf, M_SNP);
		ibuf = NULL;
	}
	if (ibuf != NULL)
		free(ibuf, M_SNP);
	return (error);
}

static struct tty *
snpdevtotty(dev)
	struct cdev *dev;
{
	struct cdevsw *cdp;
	struct tty *tp;

	cdp = dev_refthread(dev);
	if (cdp == NULL)
		return (NULL);
	if (!(cdp->d_flags & D_TTY))
		tp = NULL;
	else
		tp = dev->si_tty;
	dev_relthread(dev);
	return (tp);
}

#define SNP_INPUT_BUF	5	/* This is even too much, the maximal
				 * interactive mode write is 3 bytes
				 * length for function keys...
				 */

static int
snpwrite(dev, uio, flag)
	struct cdev *dev;
	struct uio *uio;
	int flag;
{
	struct snoop *snp;
	struct tty *tp;
	int error, i, len;
	unsigned char c[SNP_INPUT_BUF];

	snp = dev->si_drv1;
	tp = snp->snp_tty;
	if (tp == NULL)
		return (EIO);
	if ((tp->t_state & TS_SNOOP) && tp->t_line == snooplinedisc)
		goto tty_input;

	printf("snp%d: attempt to write to bad tty\n", snp->snp_unit);
	return (EIO);

tty_input:
	if (!(tp->t_state & TS_ISOPEN))
		return (EIO);

	while (uio->uio_resid > 0) {
		len = imin(uio->uio_resid, SNP_INPUT_BUF);
		if ((error = uiomove(c, len, uio)) != 0)
			return (error);
		for (i=0; i < len; i++) {
			if (ttyinput(c[i], tp))
				return (EIO);
		}
	}
	return (0);
}


static int
snpread(dev, uio, flag)
	struct cdev *dev;
	struct uio *uio;
	int flag;
{
	struct snoop *snp;
	int error, len, n, nblen, s;
	caddr_t from;
	char *nbuf;

	snp = dev->si_drv1;
	KASSERT(snp->snp_len + snp->snp_base <= snp->snp_blen,
	    ("snoop buffer error"));

	if (snp->snp_tty == NULL)
		return (EIO);

	snp->snp_flags &= ~SNOOP_RWAIT;

	do {
		if (snp->snp_len == 0) {
			if (flag & O_NONBLOCK)
				return (EWOULDBLOCK);
			snp->snp_flags |= SNOOP_RWAIT;
			error = tsleep(snp, (PZERO + 1) | PCATCH,
			    "snprd", 0);
			if (error != 0)
				return (error);
		}
	} while (snp->snp_len == 0);

	n = snp->snp_len;

	error = 0;
	while (snp->snp_len > 0 && uio->uio_resid > 0 && error == 0) {
		len = min((unsigned)uio->uio_resid, snp->snp_len);
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
		while (nblen / 2 >= snp->snp_len && nblen / 2 >= SNOOP_MINLEN)
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

	return (error);
}

static int
snp_in(snp, buf, n)
	struct snoop *snp;
	char *buf;
	int n;
{
	int s_free, s_tail;
	int s, len, nblen;
	caddr_t from, to;
	char *nbuf;

	KASSERT(n >= 0, ("negative snoop char count"));

	if (n == 0)
		return (0);

	if (snp->snp_flags & SNOOP_DOWN) {
		printf("snp%d: more data to down interface\n", snp->snp_unit);
		return (0);
	}

	if (snp->snp_flags & SNOOP_OFLOW) {
		printf("snp%d: buffer overflow\n", snp->snp_unit);
		/*
		 * On overflow we just repeat the standart close
		 * procedure...yes , this is waste of space but.. Then next
		 * read from device will fail if one would recall he is
		 * snooping and retry...
		 */

		return (snp_down(snp));
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
				wakeup(snp);
			}
			splx(s);
			return (0);
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
		wakeup(snp);
	}
	selwakeuppri(&snp->snp_sel, PZERO + 1);

	return (n);
}

static int
snpopen(dev, flag, mode, td)
	struct cdev *dev;
	int flag, mode;
	struct thread *td;
{
	struct snoop *snp;

	if (dev->si_drv1 == NULL) {
		dev->si_flags &= ~SI_CHEAPCLONE;
		dev->si_drv1 = snp = malloc(sizeof(*snp), M_SNP,
		    M_WAITOK | M_ZERO);
		snp->snp_unit = dev2unit(dev);
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
	snp->snp_target = NULL;

	LIST_INSERT_HEAD(&snp_sclist, snp, snp_list);
	return (0);
}


static int
snp_detach(snp)
	struct snoop *snp;
{
	struct tty *tp;

	snp->snp_base = 0;
	snp->snp_len = 0;

	/*
	 * If line disc. changed we do not touch this pointer, SLIP/PPP will
	 * change it anyway.
	 */
	tp = snp->snp_tty;
	if (tp == NULL)
		goto detach_notty;

	if ((tp->t_state & TS_SNOOP) && tp->t_line == snooplinedisc) {
		tp->t_state &= ~TS_SNOOP;
		tp->t_line = snp->snp_olddisc;
	} else
		printf("snp%d: bad attached tty data\n", snp->snp_unit);

	snp->snp_tty = NULL;
	snp->snp_target = NULL;

detach_notty:
	selwakeuppri(&snp->snp_sel, PZERO + 1);
	if ((snp->snp_flags & SNOOP_OPEN) == 0) 
		free(snp, M_SNP);

	return (0);
}

static int
snpclose(dev, flags, fmt, td)
	struct cdev *dev;
	int flags;
	int fmt;
	struct thread *td;
{
	struct snoop *snp;

	snp = dev->si_drv1;
	snp->snp_blen = 0;
	LIST_REMOVE(snp, snp_list);
	free(snp->snp_buf, M_SNP);
	snp->snp_flags &= ~SNOOP_OPEN;
	dev->si_drv1 = NULL;
	destroy_dev(dev);

	return (snp_detach(snp));
}

static int
snp_down(snp)
	struct snoop *snp;
{

	if (snp->snp_blen != SNOOP_MINLEN) {
		free(snp->snp_buf, M_SNP);
		snp->snp_buf = malloc(SNOOP_MINLEN, M_SNP, M_WAITOK);
		snp->snp_blen = SNOOP_MINLEN;
	}
	snp->snp_flags |= SNOOP_DOWN;

	return (snp_detach(snp));
}

static int
snpioctl(dev, cmd, data, flags, td)
	struct cdev *dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct thread *td;
{
	struct snoop *snp;
	struct tty *tp, *tpo;
	struct cdev *tdev;
	int s;

	snp = dev->si_drv1;
	switch (cmd) {
	case SNPSTTY:
#if 0
		tdev = findcdev(*((dev_t *)data));
#else
		tdev = NULL;
#endif
		if (tdev == NULL)
			return (snp_down(snp));

		tp = snpdevtotty(tdev);
		if (!tp)
			return (EINVAL);
		if (tp->t_state & TS_SNOOP)
			return (EBUSY);

		s = spltty();

		if (snp->snp_target == NULL) {
			tpo = snp->snp_tty;
			if (tpo)
				tpo->t_state &= ~TS_SNOOP;
		}

		tp->t_state |= TS_SNOOP;
		snp->snp_olddisc = tp->t_line;
		tp->t_line = snooplinedisc;
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
		*((dev_t *)data) = dev2udev(snp->snp_target);
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

static int
snppoll(dev, events, td)
	struct cdev *dev;
	int events;
	struct thread *td;
{
	struct snoop *snp;
	int revents;

	snp = dev->si_drv1;
	revents = 0;
	/*
	 * If snoop is down, we don't want to poll() forever so we return 1.
	 * Caller should see if we down via FIONREAD ioctl().  The last should
	 * return -1 to indicate down state.
	 */
	if (events & (POLLIN | POLLRDNORM)) {
		if (snp->snp_flags & SNOOP_DOWN || snp->snp_len > 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &snp->snp_sel);
	}
	return (revents);
}

static void
snp_clone(arg, cred, name, namelen, dev)
	void *arg;
	struct ucred *cred;
	char *name;
	int namelen;
	struct cdev **dev;
{
	int u, i;

	if (*dev != NULL)
		return;
	if (dev_stdclone(name, NULL, "snp", &u) != 1)
		return;
	i = clone_create(&snpclones, &snp_cdevsw, &u, dev, 0);
	if (i)
		*dev = make_dev(&snp_cdevsw, unit2minor(u),
		     UID_ROOT, GID_WHEEL, 0600, "snp%d", u);
	if (*dev != NULL) {
		dev_ref(*dev);
		(*dev)->si_flags |= SI_CHEAPCLONE;
	}
}

static int
snp_modevent(mod, type, data)
	module_t mod;
	int type;
	void *data;
{
	static eventhandler_tag eh_tag;

	switch (type) {
	case MOD_LOAD:
		/* XXX error checking. */
		clone_setup(&snpclones);
		eh_tag = EVENTHANDLER_REGISTER(dev_clone, snp_clone, 0, 1000);
		snooplinedisc = ldisc_register(LDISC_LOAD, &snpdisc);
		break;
	case MOD_UNLOAD:
		if (!LIST_EMPTY(&snp_sclist))
			return (EBUSY);
		EVENTHANDLER_DEREGISTER(dev_clone, eh_tag);
		clone_cleanup(&snpclones);
		ldisc_deregister(snooplinedisc);
		break;
	default:
		return (EOPNOTSUPP);
		break;
	}
	return (0);
}

static moduledata_t snp_mod = {
        "snp",
        snp_modevent,
        NULL
};
DECLARE_MODULE(snp, snp_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
