/*
 * Copyright (c) 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence
 * Berkeley Laboratory.
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
 *      @(#)bpf.c	8.4 (Berkeley) 1/9/95
 *
 * $FreeBSD$
 */

#include "opt_bpf.h"
#include "opt_mac.h"
#include "opt_netgraph.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/filio.h>
#include <sys/sockio.h>
#include <sys/ttycom.h>
#include <sys/filedesc.h>

#include <sys/event.h>
#include <sys/file.h>
#include <sys/poll.h>
#include <sys/proc.h>

#include <sys/socket.h>
#include <sys/vnode.h>

#include <net/if.h>
#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

static MALLOC_DEFINE(M_BPF, "BPF", "BPF data");

#if defined(DEV_BPF) || defined(NETGRAPH_BPF)

#define PRINET  26			/* interruptible */

/*
 * The default read buffer size is patchable.
 */
static int bpf_bufsize = 4096;
SYSCTL_INT(_debug, OID_AUTO, bpf_bufsize, CTLFLAG_RW,
	&bpf_bufsize, 0, "");
static int bpf_maxbufsize = BPF_MAXBUFSIZE;
SYSCTL_INT(_debug, OID_AUTO, bpf_maxbufsize, CTLFLAG_RW,
	&bpf_maxbufsize, 0, "");

/*
 *  bpf_iflist is the list of interfaces; each corresponds to an ifnet
 */
static struct bpf_if	*bpf_iflist;
static struct mtx	bpf_mtx;		/* bpf global lock */

static int	bpf_allocbufs(struct bpf_d *);
static void	bpf_attachd(struct bpf_d *d, struct bpf_if *bp);
static void	bpf_detachd(struct bpf_d *d);
static void	bpf_freed(struct bpf_d *);
static void	bpf_mcopy(const void *, void *, size_t);
static int	bpf_movein(struct uio *, int,
		    struct mbuf **, struct sockaddr *, int *);
static int	bpf_setif(struct bpf_d *, struct ifreq *);
static void	bpf_timed_out(void *);
static __inline void
		bpf_wakeup(struct bpf_d *);
static void	catchpacket(struct bpf_d *, u_char *, u_int,
		    u_int, void (*)(const void *, void *, size_t));
static void	reset_d(struct bpf_d *);
static int	 bpf_setf(struct bpf_d *, struct bpf_program *);
static int	bpf_getdltlist(struct bpf_d *, struct bpf_dltlist *);
static int	bpf_setdlt(struct bpf_d *, u_int);
static void	filt_bpfdetach(struct knote *);
static int	filt_bpfread(struct knote *, long);

static	d_open_t	bpfopen;
static	d_close_t	bpfclose;
static	d_read_t	bpfread;
static	d_write_t	bpfwrite;
static	d_ioctl_t	bpfioctl;
static	d_poll_t	bpfpoll;
static	d_kqfilter_t	bpfkqfilter;

#define CDEV_MAJOR 23
static struct cdevsw bpf_cdevsw = {
	.d_open =	bpfopen,
	.d_close =	bpfclose,
	.d_read =	bpfread,
	.d_write =	bpfwrite,
	.d_ioctl =	bpfioctl,
	.d_poll =	bpfpoll,
	.d_name =	"bpf",
	.d_maj =	CDEV_MAJOR,
	.d_kqfilter =	bpfkqfilter,
};

static struct filterops bpfread_filtops =
	{ 1, NULL, filt_bpfdetach, filt_bpfread };	

static int
bpf_movein(uio, linktype, mp, sockp, datlen)
	struct uio *uio;
	int linktype, *datlen;
	struct mbuf **mp;
	struct sockaddr *sockp;
{
	struct mbuf *m;
	int error;
	int len;
	int hlen;

	/*
	 * Build a sockaddr based on the data link layer type.
	 * We do this at this level because the ethernet header
	 * is copied directly into the data field of the sockaddr.
	 * In the case of SLIP, there is no header and the packet
	 * is forwarded as is.
	 * Also, we are careful to leave room at the front of the mbuf
	 * for the link level header.
	 */
	switch (linktype) {

	case DLT_SLIP:
		sockp->sa_family = AF_INET;
		hlen = 0;
		break;

	case DLT_EN10MB:
		sockp->sa_family = AF_UNSPEC;
		/* XXX Would MAXLINKHDR be better? */
		hlen = ETHER_HDR_LEN;
		break;

	case DLT_FDDI:
		sockp->sa_family = AF_IMPLINK;
		hlen = 0;
		break;

	case DLT_RAW:
	case DLT_NULL:
		sockp->sa_family = AF_UNSPEC;
		hlen = 0;
		break;

	case DLT_ATM_RFC1483:
		/*
		 * en atm driver requires 4-byte atm pseudo header.
		 * though it isn't standard, vpi:vci needs to be
		 * specified anyway.
		 */
		sockp->sa_family = AF_UNSPEC;
		hlen = 12;	/* XXX 4(ATM_PH) + 3(LLC) + 5(SNAP) */
		break;

	case DLT_PPP:
		sockp->sa_family = AF_UNSPEC;
		hlen = 4;	/* This should match PPP_HDRLEN */
		break;

	default:
		return (EIO);
	}

	len = uio->uio_resid;
	*datlen = len - hlen;
	if ((unsigned)len > MCLBYTES)
		return (EIO);

	if (len > MHLEN) {
		m = m_getcl(M_TRYWAIT, MT_DATA, M_PKTHDR);
	} else {
		MGETHDR(m, M_TRYWAIT, MT_DATA);
	}
	if (m == NULL)
		return (ENOBUFS);
	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = NULL;
	*mp = m;

	/*
	 * Make room for link header.
	 */
	if (hlen != 0) {
		m->m_pkthdr.len -= hlen;
		m->m_len -= hlen;
#if BSD >= 199103
		m->m_data += hlen; /* XXX */
#else
		m->m_off += hlen;
#endif
		error = uiomove(sockp->sa_data, hlen, uio);
		if (error)
			goto bad;
	}
	error = uiomove(mtod(m, void *), len - hlen, uio);
	if (!error)
		return (0);
bad:
	m_freem(m);
	return (error);
}

/*
 * Attach file to the bpf interface, i.e. make d listen on bp.
 */
static void
bpf_attachd(d, bp)
	struct bpf_d *d;
	struct bpf_if *bp;
{
	/*
	 * Point d at bp, and add d to the interface's list of listeners.
	 * Finally, point the driver's bpf cookie at the interface so
	 * it will divert packets to bpf.
	 */
	BPFIF_LOCK(bp);
	d->bd_bif = bp;
	d->bd_next = bp->bif_dlist;
	bp->bif_dlist = d;

	*bp->bif_driverp = bp;
	BPFIF_UNLOCK(bp);
}

/*
 * Detach a file from its interface.
 */
static void
bpf_detachd(d)
	struct bpf_d *d;
{
	int error;
	struct bpf_d **p;
	struct bpf_if *bp;

	/* XXX locking */
	bp = d->bd_bif;
	d->bd_bif = 0;
	/*
	 * Check if this descriptor had requested promiscuous mode.
	 * If so, turn it off.
	 */
	if (d->bd_promisc) {
		d->bd_promisc = 0;
		error = ifpromisc(bp->bif_ifp, 0);
		if (error != 0 && error != ENXIO) {
			/*
			 * ENXIO can happen if a pccard is unplugged
			 * Something is really wrong if we were able to put
			 * the driver into promiscuous mode, but can't
			 * take it out.
			 */
			if_printf(bp->bif_ifp,
				"bpf_detach: ifpromisc failed (%d)\n", error);
		}
	}
	/* Remove d from the interface's descriptor list. */
	BPFIF_LOCK(bp);
	p = &bp->bif_dlist;
	while (*p != d) {
		p = &(*p)->bd_next;
		if (*p == 0)
			panic("bpf_detachd: descriptor not in list");
	}
	*p = (*p)->bd_next;
	if (bp->bif_dlist == 0)
		/*
		 * Let the driver know that there are no more listeners.
		 */
		*bp->bif_driverp = 0;
	BPFIF_UNLOCK(bp);
}

/*
 * Open ethernet device.  Returns ENXIO for illegal minor device number,
 * EBUSY if file is open by another process.
 */
/* ARGSUSED */
static	int
bpfopen(dev, flags, fmt, td)
	dev_t dev;
	int flags;
	int fmt;
	struct thread *td;
{
	struct bpf_d *d;

	mtx_lock(&bpf_mtx);
	d = dev->si_drv1;
	/*
	 * Each minor can be opened by only one process.  If the requested
	 * minor is in use, return EBUSY.
	 */
	if (d) {
		mtx_unlock(&bpf_mtx);
		return (EBUSY);
	}
	dev->si_drv1 = (struct bpf_d *)~0;	/* mark device in use */
	mtx_unlock(&bpf_mtx);

	if ((dev->si_flags & SI_NAMED) == 0)
		make_dev(&bpf_cdevsw, minor(dev), UID_ROOT, GID_WHEEL, 0600,
		    "bpf%d", dev2unit(dev));
	MALLOC(d, struct bpf_d *, sizeof(*d), M_BPF, M_WAITOK | M_ZERO);
	dev->si_drv1 = d;
	d->bd_bufsize = bpf_bufsize;
	d->bd_sig = SIGIO;
	d->bd_seesent = 1;
#ifdef MAC
	mac_init_bpfdesc(d);
	mac_create_bpfdesc(td->td_ucred, d);
#endif
	mtx_init(&d->bd_mtx, devtoname(dev), "bpf cdev lock", MTX_DEF);
	callout_init(&d->bd_callout, CALLOUT_MPSAFE);

	return (0);
}

/*
 * Close the descriptor by detaching it from its interface,
 * deallocating its buffers, and marking it free.
 */
/* ARGSUSED */
static	int
bpfclose(dev, flags, fmt, td)
	dev_t dev;
	int flags;
	int fmt;
	struct thread *td;
{
	struct bpf_d *d = dev->si_drv1;

	BPFD_LOCK(d);
	if (d->bd_state == BPF_WAITING)
		callout_stop(&d->bd_callout);
	d->bd_state = BPF_IDLE;
	BPFD_UNLOCK(d);
	funsetown(&d->bd_sigio);
	mtx_lock(&bpf_mtx);
	if (d->bd_bif)
		bpf_detachd(d);
	mtx_unlock(&bpf_mtx);
#ifdef MAC
	mac_destroy_bpfdesc(d);
#endif /* MAC */
	bpf_freed(d);
	dev->si_drv1 = 0;
	free(d, M_BPF);

	return (0);
}


/*
 * Rotate the packet buffers in descriptor d.  Move the store buffer
 * into the hold slot, and the free buffer into the store slot.
 * Zero the length of the new store buffer.
 */
#define ROTATE_BUFFERS(d) \
	(d)->bd_hbuf = (d)->bd_sbuf; \
	(d)->bd_hlen = (d)->bd_slen; \
	(d)->bd_sbuf = (d)->bd_fbuf; \
	(d)->bd_slen = 0; \
	(d)->bd_fbuf = 0;
/*
 *  bpfread - read next chunk of packets from buffers
 */
static	int
bpfread(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	struct bpf_d *d = dev->si_drv1;
	int timed_out;
	int error;

	/*
	 * Restrict application to use a buffer the same size as
	 * as kernel buffers.
	 */
	if (uio->uio_resid != d->bd_bufsize)
		return (EINVAL);

	BPFD_LOCK(d);
	if (d->bd_state == BPF_WAITING)
		callout_stop(&d->bd_callout);
	timed_out = (d->bd_state == BPF_TIMED_OUT);
	d->bd_state = BPF_IDLE;
	/*
	 * If the hold buffer is empty, then do a timed sleep, which
	 * ends when the timeout expires or when enough packets
	 * have arrived to fill the store buffer.
	 */
	while (d->bd_hbuf == 0) {
		if ((d->bd_immediate || timed_out) && d->bd_slen != 0) {
			/*
			 * A packet(s) either arrived since the previous
			 * read or arrived while we were asleep.
			 * Rotate the buffers and return what's here.
			 */
			ROTATE_BUFFERS(d);
			break;
		}

		/*
		 * No data is available, check to see if the bpf device
		 * is still pointed at a real interface.  If not, return
		 * ENXIO so that the userland process knows to rebind
		 * it before using it again.
		 */
		if (d->bd_bif == NULL) {
			BPFD_UNLOCK(d);
			return (ENXIO);
		}

		if (ioflag & IO_NDELAY) {
			BPFD_UNLOCK(d);
			return (EWOULDBLOCK);
		}
		error = msleep(d, &d->bd_mtx, PRINET|PCATCH,
		     "bpf", d->bd_rtout);
		if (error == EINTR || error == ERESTART) {
			BPFD_UNLOCK(d);
			return (error);
		}
		if (error == EWOULDBLOCK) {
			/*
			 * On a timeout, return what's in the buffer,
			 * which may be nothing.  If there is something
			 * in the store buffer, we can rotate the buffers.
			 */
			if (d->bd_hbuf)
				/*
				 * We filled up the buffer in between
				 * getting the timeout and arriving
				 * here, so we don't need to rotate.
				 */
				break;

			if (d->bd_slen == 0) {
				BPFD_UNLOCK(d);
				return (0);
			}
			ROTATE_BUFFERS(d);
			break;
		}
	}
	/*
	 * At this point, we know we have something in the hold slot.
	 */
	BPFD_UNLOCK(d);

	/*
	 * Move data from hold buffer into user space.
	 * We know the entire buffer is transferred since
	 * we checked above that the read buffer is bpf_bufsize bytes.
	 */
	error = uiomove(d->bd_hbuf, d->bd_hlen, uio);

	BPFD_LOCK(d);
	d->bd_fbuf = d->bd_hbuf;
	d->bd_hbuf = 0;
	d->bd_hlen = 0;
	BPFD_UNLOCK(d);

	return (error);
}


/*
 * If there are processes sleeping on this descriptor, wake them up.
 */
static __inline void
bpf_wakeup(d)
	struct bpf_d *d;
{
	if (d->bd_state == BPF_WAITING) {
		callout_stop(&d->bd_callout);
		d->bd_state = BPF_IDLE;
	}
	wakeup(d);
	if (d->bd_async && d->bd_sig && d->bd_sigio)
		pgsigio(&d->bd_sigio, d->bd_sig, 0);

	selwakeup(&d->bd_sel);
	KNOTE(&d->bd_sel.si_note, 0);
}

static void
bpf_timed_out(arg)
	void *arg;
{
	struct bpf_d *d = (struct bpf_d *)arg;

	BPFD_LOCK(d);
	if (d->bd_state == BPF_WAITING) {
		d->bd_state = BPF_TIMED_OUT;
		if (d->bd_slen != 0)
			bpf_wakeup(d);
	}
	BPFD_UNLOCK(d);
}

static	int
bpfwrite(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	struct bpf_d *d = dev->si_drv1;
	struct ifnet *ifp;
	struct mbuf *m;
	int error;
	static struct sockaddr dst;
	int datlen;

	if (d->bd_bif == 0)
		return (ENXIO);

	ifp = d->bd_bif->bif_ifp;

	if (uio->uio_resid == 0)
		return (0);

	error = bpf_movein(uio, (int)d->bd_bif->bif_dlt, &m, &dst, &datlen);
	if (error)
		return (error);

	if (datlen > ifp->if_mtu)
		return (EMSGSIZE);

	if (d->bd_hdrcmplt)
		dst.sa_family = pseudo_AF_HDRCMPLT;

	mtx_lock(&Giant);
#ifdef MAC
	mac_create_mbuf_from_bpfdesc(d, m);
#endif
	error = (*ifp->if_output)(ifp, m, &dst, (struct rtentry *)0);
	mtx_unlock(&Giant);
	/*
	 * The driver frees the mbuf.
	 */
	return (error);
}

/*
 * Reset a descriptor by flushing its packet buffer and clearing the
 * receive and drop counts.
 */
static void
reset_d(d)
	struct bpf_d *d;
{

	mtx_assert(&d->bd_mtx, MA_OWNED);
	if (d->bd_hbuf) {
		/* Free the hold buffer. */
		d->bd_fbuf = d->bd_hbuf;
		d->bd_hbuf = 0;
	}
	d->bd_slen = 0;
	d->bd_hlen = 0;
	d->bd_rcount = 0;
	d->bd_dcount = 0;
}

/*
 *  FIONREAD		Check for read packet available.
 *  SIOCGIFADDR		Get interface address - convenient hook to driver.
 *  BIOCGBLEN		Get buffer len [for read()].
 *  BIOCSETF		Set ethernet read filter.
 *  BIOCFLUSH		Flush read packet buffer.
 *  BIOCPROMISC		Put interface into promiscuous mode.
 *  BIOCGDLT		Get link layer type.
 *  BIOCGETIF		Get interface name.
 *  BIOCSETIF		Set interface.
 *  BIOCSRTIMEOUT	Set read timeout.
 *  BIOCGRTIMEOUT	Get read timeout.
 *  BIOCGSTATS		Get packet stats.
 *  BIOCIMMEDIATE	Set immediate mode.
 *  BIOCVERSION		Get filter language version.
 *  BIOCGHDRCMPLT	Get "header already complete" flag
 *  BIOCSHDRCMPLT	Set "header already complete" flag
 *  BIOCGSEESENT	Get "see packets sent" flag
 *  BIOCSSEESENT	Set "see packets sent" flag
 */
/* ARGSUSED */
static	int
bpfioctl(dev, cmd, addr, flags, td)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flags;
	struct thread *td;
{
	struct bpf_d *d = dev->si_drv1;
	int error = 0;

	BPFD_LOCK(d);
	if (d->bd_state == BPF_WAITING)
		callout_stop(&d->bd_callout);
	d->bd_state = BPF_IDLE;
	BPFD_UNLOCK(d);

	switch (cmd) {

	default:
		error = EINVAL;
		break;

	/*
	 * Check for read packet available.
	 */
	case FIONREAD:
		{
			int n;

			BPFD_LOCK(d);
			n = d->bd_slen;
			if (d->bd_hbuf)
				n += d->bd_hlen;
			BPFD_UNLOCK(d);

			*(int *)addr = n;
			break;
		}

	case SIOCGIFADDR:
		{
			struct ifnet *ifp;

			if (d->bd_bif == 0)
				error = EINVAL;
			else {
				ifp = d->bd_bif->bif_ifp;
				error = (*ifp->if_ioctl)(ifp, cmd, addr);
			}
			break;
		}

	/*
	 * Get buffer len [for read()].
	 */
	case BIOCGBLEN:
		*(u_int *)addr = d->bd_bufsize;
		break;

	/*
	 * Set buffer length.
	 */
	case BIOCSBLEN:
		if (d->bd_bif != 0)
			error = EINVAL;
		else {
			u_int size = *(u_int *)addr;

			if (size > bpf_maxbufsize)
				*(u_int *)addr = size = bpf_maxbufsize;
			else if (size < BPF_MINBUFSIZE)
				*(u_int *)addr = size = BPF_MINBUFSIZE;
			d->bd_bufsize = size;
		}
		break;

	/*
	 * Set link layer read filter.
	 */
	case BIOCSETF:
		error = bpf_setf(d, (struct bpf_program *)addr);
		break;

	/*
	 * Flush read packet buffer.
	 */
	case BIOCFLUSH:
		BPFD_LOCK(d);
		reset_d(d);
		BPFD_UNLOCK(d);
		break;

	/*
	 * Put interface into promiscuous mode.
	 */
	case BIOCPROMISC:
		if (d->bd_bif == 0) {
			/*
			 * No interface attached yet.
			 */
			error = EINVAL;
			break;
		}
		if (d->bd_promisc == 0) {
			mtx_lock(&Giant);
			error = ifpromisc(d->bd_bif->bif_ifp, 1);
			mtx_unlock(&Giant);
			if (error == 0)
				d->bd_promisc = 1;
		}
		break;

	/*
	 * Get current data link type.
	 */
	case BIOCGDLT:
		if (d->bd_bif == 0)
			error = EINVAL;
		else
			*(u_int *)addr = d->bd_bif->bif_dlt;
		break;

	/*
	 * Get a list of supported data link types.
	 */
	case BIOCGDLTLIST:
		if (d->bd_bif == 0)
			error = EINVAL;
		else
			error = bpf_getdltlist(d, (struct bpf_dltlist *)addr);
		break;

	/*
	 * Set data link type.
	 */
	case BIOCSDLT:
		if (d->bd_bif == 0)
			error = EINVAL;
		else
			error = bpf_setdlt(d, *(u_int *)addr);
		break;

	/*
	 * Get interface name.
	 */
	case BIOCGETIF:
		if (d->bd_bif == 0)
			error = EINVAL;
		else {
			struct ifnet *const ifp = d->bd_bif->bif_ifp;
			struct ifreq *const ifr = (struct ifreq *)addr;

			strlcpy(ifr->ifr_name, ifp->if_xname,
			    sizeof(ifr->ifr_name));
		}
		break;

	/*
	 * Set interface.
	 */
	case BIOCSETIF:
		error = bpf_setif(d, (struct ifreq *)addr);
		break;

	/*
	 * Set read timeout.
	 */
	case BIOCSRTIMEOUT:
		{
			struct timeval *tv = (struct timeval *)addr;

			/*
			 * Subtract 1 tick from tvtohz() since this isn't
			 * a one-shot timer.
			 */
			if ((error = itimerfix(tv)) == 0)
				d->bd_rtout = tvtohz(tv) - 1;
			break;
		}

	/*
	 * Get read timeout.
	 */
	case BIOCGRTIMEOUT:
		{
			struct timeval *tv = (struct timeval *)addr;

			tv->tv_sec = d->bd_rtout / hz;
			tv->tv_usec = (d->bd_rtout % hz) * tick;
			break;
		}

	/*
	 * Get packet stats.
	 */
	case BIOCGSTATS:
		{
			struct bpf_stat *bs = (struct bpf_stat *)addr;

			bs->bs_recv = d->bd_rcount;
			bs->bs_drop = d->bd_dcount;
			break;
		}

	/*
	 * Set immediate mode.
	 */
	case BIOCIMMEDIATE:
		d->bd_immediate = *(u_int *)addr;
		break;

	case BIOCVERSION:
		{
			struct bpf_version *bv = (struct bpf_version *)addr;

			bv->bv_major = BPF_MAJOR_VERSION;
			bv->bv_minor = BPF_MINOR_VERSION;
			break;
		}

	/*
	 * Get "header already complete" flag
	 */
	case BIOCGHDRCMPLT:
		*(u_int *)addr = d->bd_hdrcmplt;
		break;

	/*
	 * Set "header already complete" flag
	 */
	case BIOCSHDRCMPLT:
		d->bd_hdrcmplt = *(u_int *)addr ? 1 : 0;
		break;

	/*
	 * Get "see sent packets" flag
	 */
	case BIOCGSEESENT:
		*(u_int *)addr = d->bd_seesent;
		break;

	/*
	 * Set "see sent packets" flag
	 */
	case BIOCSSEESENT:
		d->bd_seesent = *(u_int *)addr;
		break;

	case FIONBIO:		/* Non-blocking I/O */
		break;

	case FIOASYNC:		/* Send signal on receive packets */
		d->bd_async = *(int *)addr;
		break;

	case FIOSETOWN:
		error = fsetown(*(int *)addr, &d->bd_sigio);
		break;

	case FIOGETOWN:
		*(int *)addr = fgetown(&d->bd_sigio);
		break;

	/* This is deprecated, FIOSETOWN should be used instead. */
	case TIOCSPGRP:
		error = fsetown(-(*(int *)addr), &d->bd_sigio);
		break;

	/* This is deprecated, FIOGETOWN should be used instead. */
	case TIOCGPGRP:
		*(int *)addr = -fgetown(&d->bd_sigio);
		break;

	case BIOCSRSIG:		/* Set receive signal */
		{
			u_int sig;

			sig = *(u_int *)addr;

			if (sig >= NSIG)
				error = EINVAL;
			else
				d->bd_sig = sig;
			break;
		}
	case BIOCGRSIG:
		*(u_int *)addr = d->bd_sig;
		break;
	}
	return (error);
}

/*
 * Set d's packet filter program to fp.  If this file already has a filter,
 * free it and replace it.  Returns EINVAL for bogus requests.
 */
static int
bpf_setf(d, fp)
	struct bpf_d *d;
	struct bpf_program *fp;
{
	struct bpf_insn *fcode, *old;
	u_int flen, size;

	old = d->bd_filter;
	if (fp->bf_insns == 0) {
		if (fp->bf_len != 0)
			return (EINVAL);
		BPFD_LOCK(d);
		d->bd_filter = 0;
		reset_d(d);
		BPFD_UNLOCK(d);
		if (old != 0)
			free((caddr_t)old, M_BPF);
		return (0);
	}
	flen = fp->bf_len;
	if (flen > BPF_MAXINSNS)
		return (EINVAL);

	size = flen * sizeof(*fp->bf_insns);
	fcode = (struct bpf_insn *)malloc(size, M_BPF, M_WAITOK);
	if (copyin((caddr_t)fp->bf_insns, (caddr_t)fcode, size) == 0 &&
	    bpf_validate(fcode, (int)flen)) {
		BPFD_LOCK(d);
		d->bd_filter = fcode;
		reset_d(d);
		BPFD_UNLOCK(d);
		if (old != 0)
			free((caddr_t)old, M_BPF);

		return (0);
	}
	free((caddr_t)fcode, M_BPF);
	return (EINVAL);
}

/*
 * Detach a file from its current interface (if attached at all) and attach
 * to the interface indicated by the name stored in ifr.
 * Return an errno or 0.
 */
static int
bpf_setif(d, ifr)
	struct bpf_d *d;
	struct ifreq *ifr;
{
	struct bpf_if *bp;
	int error;
	struct ifnet *theywant;

	theywant = ifunit(ifr->ifr_name);
	if (theywant == 0)
		return ENXIO;

	/*
	 * Look through attached interfaces for the named one.
	 */
	mtx_lock(&bpf_mtx);
	for (bp = bpf_iflist; bp != 0; bp = bp->bif_next) {
		struct ifnet *ifp = bp->bif_ifp;

		if (ifp == 0 || ifp != theywant)
			continue;
		/* skip additional entry */
		if (bp->bif_driverp != (struct bpf_if **)&ifp->if_bpf)
			continue;

		mtx_unlock(&bpf_mtx);
		/*
		 * We found the requested interface.
		 * If it's not up, return an error.
		 * Allocate the packet buffers if we need to.
		 * If we're already attached to requested interface,
		 * just flush the buffer.
		 */
		if ((ifp->if_flags & IFF_UP) == 0)
			return (ENETDOWN);

		if (d->bd_sbuf == 0) {
			error = bpf_allocbufs(d);
			if (error != 0)
				return (error);
		}
		if (bp != d->bd_bif) {
			if (d->bd_bif)
				/*
				 * Detach if attached to something else.
				 */
				bpf_detachd(d);

			bpf_attachd(d, bp);
		}
		BPFD_LOCK(d);
		reset_d(d);
		BPFD_UNLOCK(d);
		return (0);
	}
	mtx_unlock(&bpf_mtx);
	/* Not found. */
	return (ENXIO);
}

/*
 * Support for select() and poll() system calls
 *
 * Return true iff the specific operation will not block indefinitely.
 * Otherwise, return false but make a note that a selwakeup() must be done.
 */
static int
bpfpoll(dev, events, td)
	dev_t dev;
	int events;
	struct thread *td;
{
	struct bpf_d *d;
	int revents;

	d = dev->si_drv1;
	if (d->bd_bif == NULL)
		return (ENXIO);

	revents = events & (POLLOUT | POLLWRNORM);
	BPFD_LOCK(d);
	if (events & (POLLIN | POLLRDNORM)) {
		if (bpf_ready(d))
			revents |= events & (POLLIN | POLLRDNORM);
		else {
			selrecord(td, &d->bd_sel);
			/* Start the read timeout if necessary. */
			if (d->bd_rtout > 0 && d->bd_state == BPF_IDLE) {
				callout_reset(&d->bd_callout, d->bd_rtout,
				    bpf_timed_out, d);
				d->bd_state = BPF_WAITING;
			}
		}
	}
	BPFD_UNLOCK(d);
	return (revents);
}

/*
 * Support for kevent() system call.  Register EVFILT_READ filters and
 * reject all others.
 */
int
bpfkqfilter(dev, kn)
	dev_t dev;
	struct knote *kn;
{
	struct bpf_d *d = (struct bpf_d *)dev->si_drv1;

	if (kn->kn_filter != EVFILT_READ)
		return (1);

	kn->kn_fop = &bpfread_filtops;
	kn->kn_hook = d;
	BPFD_LOCK(d);
	SLIST_INSERT_HEAD(&d->bd_sel.si_note, kn, kn_selnext);
	BPFD_UNLOCK(d);

	return (0);
}

static void
filt_bpfdetach(kn)
	struct knote *kn;
{
	struct bpf_d *d = (struct bpf_d *)kn->kn_hook;

	BPFD_LOCK(d);
	SLIST_REMOVE(&d->bd_sel.si_note, kn, knote, kn_selnext);
	BPFD_UNLOCK(d);
}

static int
filt_bpfread(kn, hint)
	struct knote *kn;
	long hint;
{
	struct bpf_d *d = (struct bpf_d *)kn->kn_hook;
	int ready;

	BPFD_LOCK(d);
	ready = bpf_ready(d);
	if (ready) {
		kn->kn_data = d->bd_slen;
		if (d->bd_hbuf)
			kn->kn_data += d->bd_hlen;
	}
	else if (d->bd_rtout > 0 && d->bd_state == BPF_IDLE) {
		callout_reset(&d->bd_callout, d->bd_rtout,
		    bpf_timed_out, d);
		d->bd_state = BPF_WAITING;
	}
	BPFD_UNLOCK(d);
	
	return (ready);
}

/*
 * Incoming linkage from device drivers.  Process the packet pkt, of length
 * pktlen, which is stored in a contiguous buffer.  The packet is parsed
 * by each process' filter, and if accepted, stashed into the corresponding
 * buffer.
 */
void
bpf_tap(bp, pkt, pktlen)
	struct bpf_if *bp;
	u_char *pkt;
	u_int pktlen;
{
	struct bpf_d *d;
	u_int slen;

	BPFIF_LOCK(bp);
	for (d = bp->bif_dlist; d != 0; d = d->bd_next) {
		BPFD_LOCK(d);
		++d->bd_rcount;
		slen = bpf_filter(d->bd_filter, pkt, pktlen, pktlen);
		if (slen != 0) {
#ifdef MAC
			if (mac_check_bpfdesc_receive(d, bp->bif_ifp) == 0)
#endif
				catchpacket(d, pkt, pktlen, slen, bcopy);
		}
		BPFD_UNLOCK(d);
	}
	BPFIF_UNLOCK(bp);
}

/*
 * Copy data from an mbuf chain into a buffer.  This code is derived
 * from m_copydata in sys/uipc_mbuf.c.
 */
static void
bpf_mcopy(src_arg, dst_arg, len)
	const void *src_arg;
	void *dst_arg;
	size_t len;
{
	const struct mbuf *m;
	u_int count;
	u_char *dst;

	m = src_arg;
	dst = dst_arg;
	while (len > 0) {
		if (m == 0)
			panic("bpf_mcopy");
		count = min(m->m_len, len);
		bcopy(mtod(m, void *), dst, count);
		m = m->m_next;
		dst += count;
		len -= count;
	}
}

/*
 * Incoming linkage from device drivers, when packet is in an mbuf chain.
 */
void
bpf_mtap(bp, m)
	struct bpf_if *bp;
	struct mbuf *m;
{
	struct bpf_d *d;
	u_int pktlen, slen;

	pktlen = m_length(m, NULL);
	if (pktlen == m->m_len) {
		bpf_tap(bp, mtod(m, u_char *), pktlen);
		return;
	}

	BPFIF_LOCK(bp);
	for (d = bp->bif_dlist; d != 0; d = d->bd_next) {
		if (!d->bd_seesent && (m->m_pkthdr.rcvif == NULL))
			continue;
		BPFD_LOCK(d);
		++d->bd_rcount;
		slen = bpf_filter(d->bd_filter, (u_char *)m, pktlen, 0);
		if (slen != 0)
#ifdef MAC
			if (mac_check_bpfdesc_receive(d, bp->bif_ifp) == 0)
#endif
				catchpacket(d, (u_char *)m, pktlen, slen,
				    bpf_mcopy);
		BPFD_UNLOCK(d);
	}
	BPFIF_UNLOCK(bp);
}

/*
 * Move the packet data from interface memory (pkt) into the
 * store buffer.  Return 1 if it's time to wakeup a listener (buffer full),
 * otherwise 0.  "copy" is the routine called to do the actual data
 * transfer.  bcopy is passed in to copy contiguous chunks, while
 * bpf_mcopy is passed in to copy mbuf chains.  In the latter case,
 * pkt is really an mbuf.
 */
static void
catchpacket(d, pkt, pktlen, snaplen, cpfn)
	struct bpf_d *d;
	u_char *pkt;
	u_int pktlen, snaplen;
	void (*cpfn)(const void *, void *, size_t);
{
	struct bpf_hdr *hp;
	int totlen, curlen;
	int hdrlen = d->bd_bif->bif_hdrlen;
	/*
	 * Figure out how many bytes to move.  If the packet is
	 * greater or equal to the snapshot length, transfer that
	 * much.  Otherwise, transfer the whole packet (unless
	 * we hit the buffer size limit).
	 */
	totlen = hdrlen + min(snaplen, pktlen);
	if (totlen > d->bd_bufsize)
		totlen = d->bd_bufsize;

	/*
	 * Round up the end of the previous packet to the next longword.
	 */
	curlen = BPF_WORDALIGN(d->bd_slen);
	if (curlen + totlen > d->bd_bufsize) {
		/*
		 * This packet will overflow the storage buffer.
		 * Rotate the buffers if we can, then wakeup any
		 * pending reads.
		 */
		if (d->bd_fbuf == 0) {
			/*
			 * We haven't completed the previous read yet,
			 * so drop the packet.
			 */
			++d->bd_dcount;
			return;
		}
		ROTATE_BUFFERS(d);
		bpf_wakeup(d);
		curlen = 0;
	}
	else if (d->bd_immediate || d->bd_state == BPF_TIMED_OUT)
		/*
		 * Immediate mode is set, or the read timeout has
		 * already expired during a select call.  A packet
		 * arrived, so the reader should be woken up.
		 */
		bpf_wakeup(d);

	/*
	 * Append the bpf header.
	 */
	hp = (struct bpf_hdr *)(d->bd_sbuf + curlen);
	microtime(&hp->bh_tstamp);
	hp->bh_datalen = pktlen;
	hp->bh_hdrlen = hdrlen;
	/*
	 * Copy the packet data into the store buffer and update its length.
	 */
	(*cpfn)(pkt, (u_char *)hp + hdrlen, (hp->bh_caplen = totlen - hdrlen));
	d->bd_slen = curlen + totlen;
}

/*
 * Initialize all nonzero fields of a descriptor.
 */
static int
bpf_allocbufs(d)
	struct bpf_d *d;
{
	d->bd_fbuf = (caddr_t)malloc(d->bd_bufsize, M_BPF, M_WAITOK);
	if (d->bd_fbuf == 0)
		return (ENOBUFS);

	d->bd_sbuf = (caddr_t)malloc(d->bd_bufsize, M_BPF, M_WAITOK);
	if (d->bd_sbuf == 0) {
		free(d->bd_fbuf, M_BPF);
		return (ENOBUFS);
	}
	d->bd_slen = 0;
	d->bd_hlen = 0;
	return (0);
}

/*
 * Free buffers currently in use by a descriptor.
 * Called on close.
 */
static void
bpf_freed(d)
	struct bpf_d *d;
{
	/*
	 * We don't need to lock out interrupts since this descriptor has
	 * been detached from its interface and it yet hasn't been marked
	 * free.
	 */
	if (d->bd_sbuf != 0) {
		free(d->bd_sbuf, M_BPF);
		if (d->bd_hbuf != 0)
			free(d->bd_hbuf, M_BPF);
		if (d->bd_fbuf != 0)
			free(d->bd_fbuf, M_BPF);
	}
	if (d->bd_filter)
		free((caddr_t)d->bd_filter, M_BPF);
	mtx_destroy(&d->bd_mtx);
}

/*
 * Attach an interface to bpf.  dlt is the link layer type; hdrlen is the
 * fixed size of the link header (variable length headers not yet supported).
 */
void
bpfattach(ifp, dlt, hdrlen)
	struct ifnet *ifp;
	u_int dlt, hdrlen;
{

	bpfattach2(ifp, dlt, hdrlen, &ifp->if_bpf);
}

/*
 * Attach an interface to bpf.  ifp is a pointer to the structure
 * defining the interface to be attached, dlt is the link layer type,
 * and hdrlen is the fixed size of the link header (variable length
 * headers are not yet supporrted).
 */
void
bpfattach2(ifp, dlt, hdrlen, driverp)
	struct ifnet *ifp;
	u_int dlt, hdrlen;
	struct bpf_if **driverp;
{
	struct bpf_if *bp;
	bp = (struct bpf_if *)malloc(sizeof(*bp), M_BPF, M_NOWAIT | M_ZERO);
	if (bp == 0)
		panic("bpfattach");

	bp->bif_dlist = 0;
	bp->bif_driverp = driverp;
	bp->bif_ifp = ifp;
	bp->bif_dlt = dlt;
	mtx_init(&bp->bif_mtx, "bpf interface lock", NULL, MTX_DEF);

	mtx_lock(&bpf_mtx);
	bp->bif_next = bpf_iflist;
	bpf_iflist = bp;
	mtx_unlock(&bpf_mtx);

	*bp->bif_driverp = 0;

	/*
	 * Compute the length of the bpf header.  This is not necessarily
	 * equal to SIZEOF_BPF_HDR because we want to insert spacing such
	 * that the network layer header begins on a longword boundary (for
	 * performance reasons and to alleviate alignment restrictions).
	 */
	bp->bif_hdrlen = BPF_WORDALIGN(hdrlen + SIZEOF_BPF_HDR) - hdrlen;

	if (bootverbose)
		if_printf(ifp, "bpf attached\n");
}

/*
 * Detach bpf from an interface.  This involves detaching each descriptor
 * associated with the interface, and leaving bd_bif NULL.  Notify each
 * descriptor as it's detached so that any sleepers wake up and get
 * ENXIO.
 */
void
bpfdetach(ifp)
	struct ifnet *ifp;
{
	struct bpf_if	*bp, *bp_prev;
	struct bpf_d	*d;

	/* Locate BPF interface information */
	bp_prev = NULL;

	mtx_lock(&bpf_mtx);
	for (bp = bpf_iflist; bp != NULL; bp = bp->bif_next) {
		if (ifp == bp->bif_ifp)
			break;
		bp_prev = bp;
	}

	/* Interface wasn't attached */
	if ((bp == NULL) || (bp->bif_ifp == NULL)) {
		mtx_unlock(&bpf_mtx);
		printf("bpfdetach: %s was not attached\n", ifp->if_xname);
		return;
	}

	if (bp_prev) {
		bp_prev->bif_next = bp->bif_next;
	} else {
		bpf_iflist = bp->bif_next;
	}
	mtx_unlock(&bpf_mtx);

	while ((d = bp->bif_dlist) != NULL) {
		bpf_detachd(d);
		BPFD_LOCK(d);
		bpf_wakeup(d);
		BPFD_UNLOCK(d);
	}

	mtx_destroy(&bp->bif_mtx);
	free(bp, M_BPF);
}

/*
 * Get a list of available data link type of the interface.
 */
static int
bpf_getdltlist(d, bfl)
	struct bpf_d *d;
	struct bpf_dltlist *bfl;
{
	int n, error;
	struct ifnet *ifp;
	struct bpf_if *bp;

	ifp = d->bd_bif->bif_ifp;
	n = 0;
	error = 0;
	mtx_lock(&bpf_mtx);
	for (bp = bpf_iflist; bp != NULL; bp = bp->bif_next) {
		if (bp->bif_ifp != ifp)
			continue;
		if (bfl->bfl_list != NULL) {
			if (n >= bfl->bfl_len) {
				mtx_unlock(&bpf_mtx);
				return (ENOMEM);
			}
			error = copyout(&bp->bif_dlt,
			    bfl->bfl_list + n, sizeof(u_int));
		}
		n++;
	}
	mtx_unlock(&bpf_mtx);
	bfl->bfl_len = n;
	return (error);
}

/*
 * Set the data link type of a BPF instance.
 */
static int
bpf_setdlt(d, dlt)
	struct bpf_d *d;
	u_int dlt;
{
	int error, opromisc;
	struct ifnet *ifp;
	struct bpf_if *bp;

	if (d->bd_bif->bif_dlt == dlt)
		return (0);
	ifp = d->bd_bif->bif_ifp;
	mtx_lock(&bpf_mtx);
	for (bp = bpf_iflist; bp != NULL; bp = bp->bif_next) {
		if (bp->bif_ifp == ifp && bp->bif_dlt == dlt)
			break;
	}
	mtx_unlock(&bpf_mtx);
	if (bp != NULL) {
		BPFD_LOCK(d);
		opromisc = d->bd_promisc;
		bpf_detachd(d);
		bpf_attachd(d, bp);
		reset_d(d);
		BPFD_UNLOCK(d);
		if (opromisc) {
			error = ifpromisc(bp->bif_ifp, 1);
			if (error)
				if_printf(bp->bif_ifp,
					"bpf_setdlt: ifpromisc failed (%d)\n",
					error);
			else
				d->bd_promisc = 1;
		}
	}
	return (bp == NULL ? EINVAL : 0);
}

static void bpf_drvinit(void *unused);

static void bpf_clone(void *arg, char *name, int namelen, dev_t *dev);

static void
bpf_clone(arg, name, namelen, dev)
	void *arg;
	char *name;
	int namelen;
	dev_t *dev;
{
	int u;

	if (*dev != NODEV)
		return;
	if (dev_stdclone(name, NULL, "bpf", &u) != 1)
		return;
	*dev = make_dev(&bpf_cdevsw, unit2minor(u), UID_ROOT, GID_WHEEL, 0600,
	    "bpf%d", u);
	(*dev)->si_flags |= SI_CHEAPCLONE;
	return;
}

static void
bpf_drvinit(unused)
	void *unused;
{

	mtx_init(&bpf_mtx, "bpf global lock", NULL, MTX_DEF);
	EVENTHANDLER_REGISTER(dev_clone, bpf_clone, 0, 1000);
}

SYSINIT(bpfdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,bpf_drvinit,NULL)

#else /* !DEV_BPF && !NETGRAPH_BPF */
/*
 * NOP stubs to allow bpf-using drivers to load and function.
 *
 * A 'better' implementation would allow the core bpf functionality
 * to be loaded at runtime.
 */

void
bpf_tap(bp, pkt, pktlen)
	struct bpf_if *bp;
	u_char *pkt;
	u_int pktlen;
{
}

void
bpf_mtap(bp, m)
	struct bpf_if *bp;
	struct mbuf *m;
{
}

void
bpfattach(ifp, dlt, hdrlen)
	struct ifnet *ifp;
	u_int dlt, hdrlen;
{
}

void
bpfattach2(ifp, dlt, hdrlen, driverp)
	struct ifnet *ifp;
	u_int dlt, hdrlen;
	struct bpf_if **driverp;
{
}

void
bpfdetach(ifp)
	struct ifnet *ifp;
{
}

u_int
bpf_filter(pc, p, wirelen, buflen)
	const struct bpf_insn *pc;
	u_char *p;
	u_int wirelen;
	u_int buflen;
{
	return -1;	/* "no filter" behaviour */
}

int
bpf_validate(f, len)
	const struct bpf_insn *f;
	int len;
{
	return 0;		/* false */
}

#endif /* !DEV_BPF && !NETGRAPH_BPF */
