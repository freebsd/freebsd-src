/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bpf.h"
#include "opt_compat.h"
#include "opt_netgraph.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/jail.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/time.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/filio.h>
#include <sys/sockio.h>
#include <sys/ttycom.h>
#include <sys/uio.h>

#include <sys/event.h>
#include <sys/file.h>
#include <sys/poll.h>
#include <sys/proc.h>

#include <sys/socket.h>

#include <net/if.h>
#include <net/bpf.h>
#include <net/bpf_buffer.h>
#ifdef BPF_JITTER
#include <net/bpf_jitter.h>
#endif
#include <net/bpf_zerocopy.h>
#include <net/bpfdesc.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <net80211/ieee80211_freebsd.h>

#include <security/mac/mac_framework.h>

MALLOC_DEFINE(M_BPF, "BPF", "BPF data");

#if defined(DEV_BPF) || defined(NETGRAPH_BPF)

#define PRINET  26			/* interruptible */

#define	SIZEOF_BPF_HDR(type)	\
    (offsetof(type, bh_hdrlen) + sizeof(((type *)0)->bh_hdrlen))

#ifdef COMPAT_FREEBSD32
#include <sys/mount.h>
#include <compat/freebsd32/freebsd32.h>
#define BPF_ALIGNMENT32 sizeof(int32_t)
#define BPF_WORDALIGN32(x) (((x)+(BPF_ALIGNMENT32-1))&~(BPF_ALIGNMENT32-1))

#ifndef BURN_BRIDGES
/*
 * 32-bit version of structure prepended to each packet.  We use this header
 * instead of the standard one for 32-bit streams.  We mark the a stream as
 * 32-bit the first time we see a 32-bit compat ioctl request.
 */
struct bpf_hdr32 {
	struct timeval32 bh_tstamp;	/* time stamp */
	uint32_t	bh_caplen;	/* length of captured portion */
	uint32_t	bh_datalen;	/* original length of packet */
	uint16_t	bh_hdrlen;	/* length of bpf header (this struct
					   plus alignment padding) */
};
#endif

struct bpf_program32 {
	u_int bf_len;
	uint32_t bf_insns;
};

struct bpf_dltlist32 {
	u_int	bfl_len;
	u_int	bfl_list;
};

#define	BIOCSETF32	_IOW('B', 103, struct bpf_program32)
#define	BIOCSRTIMEOUT32	_IOW('B', 109, struct timeval32)
#define	BIOCGRTIMEOUT32	_IOR('B', 110, struct timeval32)
#define	BIOCGDLTLIST32	_IOWR('B', 121, struct bpf_dltlist32)
#define	BIOCSETWF32	_IOW('B', 123, struct bpf_program32)
#define	BIOCSETFNR32	_IOW('B', 130, struct bpf_program32)
#endif

/*
 * bpf_iflist is a list of BPF interface structures, each corresponding to a
 * specific DLT.  The same network interface might have several BPF interface
 * structures registered by different layers in the stack (i.e., 802.11
 * frames, ethernet frames, etc).
 */
static LIST_HEAD(, bpf_if)	bpf_iflist;
static struct mtx	bpf_mtx;		/* bpf global lock */
static int		bpf_bpfd_cnt;

static void	bpf_attachd(struct bpf_d *, struct bpf_if *);
static void	bpf_detachd(struct bpf_d *);
static void	bpf_freed(struct bpf_d *);
static int	bpf_movein(struct uio *, int, struct ifnet *, struct mbuf **,
		    struct sockaddr *, int *, struct bpf_insn *);
static int	bpf_setif(struct bpf_d *, struct ifreq *);
static void	bpf_timed_out(void *);
static __inline void
		bpf_wakeup(struct bpf_d *);
static void	catchpacket(struct bpf_d *, u_char *, u_int, u_int,
		    void (*)(struct bpf_d *, caddr_t, u_int, void *, u_int),
		    struct bintime *);
static void	reset_d(struct bpf_d *);
static int	 bpf_setf(struct bpf_d *, struct bpf_program *, u_long cmd);
static int	bpf_getdltlist(struct bpf_d *, struct bpf_dltlist *);
static int	bpf_setdlt(struct bpf_d *, u_int);
static void	filt_bpfdetach(struct knote *);
static int	filt_bpfread(struct knote *, long);
static void	bpf_drvinit(void *);
static int	bpf_stats_sysctl(SYSCTL_HANDLER_ARGS);

SYSCTL_NODE(_net, OID_AUTO, bpf, CTLFLAG_RW, 0, "bpf sysctl");
int bpf_maxinsns = BPF_MAXINSNS;
SYSCTL_INT(_net_bpf, OID_AUTO, maxinsns, CTLFLAG_RW,
    &bpf_maxinsns, 0, "Maximum bpf program instructions");
static int bpf_zerocopy_enable = 0;
SYSCTL_INT(_net_bpf, OID_AUTO, zerocopy_enable, CTLFLAG_RW,
    &bpf_zerocopy_enable, 0, "Enable new zero-copy BPF buffer sessions");
SYSCTL_NODE(_net_bpf, OID_AUTO, stats, CTLFLAG_MPSAFE | CTLFLAG_RW,
    bpf_stats_sysctl, "bpf statistics portal");

static	d_open_t	bpfopen;
static	d_read_t	bpfread;
static	d_write_t	bpfwrite;
static	d_ioctl_t	bpfioctl;
static	d_poll_t	bpfpoll;
static	d_kqfilter_t	bpfkqfilter;

static struct cdevsw bpf_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	bpfopen,
	.d_read =	bpfread,
	.d_write =	bpfwrite,
	.d_ioctl =	bpfioctl,
	.d_poll =	bpfpoll,
	.d_name =	"bpf",
	.d_kqfilter =	bpfkqfilter,
};

static struct filterops bpfread_filtops = {
	.f_isfd = 1,
	.f_detach = filt_bpfdetach,
	.f_event = filt_bpfread,
};

/*
 * Wrapper functions for various buffering methods.  If the set of buffer
 * modes expands, we will probably want to introduce a switch data structure
 * similar to protosw, et.
 */
static void
bpf_append_bytes(struct bpf_d *d, caddr_t buf, u_int offset, void *src,
    u_int len)
{

	BPFD_LOCK_ASSERT(d);

	switch (d->bd_bufmode) {
	case BPF_BUFMODE_BUFFER:
		return (bpf_buffer_append_bytes(d, buf, offset, src, len));

	case BPF_BUFMODE_ZBUF:
		d->bd_zcopy++;
		return (bpf_zerocopy_append_bytes(d, buf, offset, src, len));

	default:
		panic("bpf_buf_append_bytes");
	}
}

static void
bpf_append_mbuf(struct bpf_d *d, caddr_t buf, u_int offset, void *src,
    u_int len)
{

	BPFD_LOCK_ASSERT(d);

	switch (d->bd_bufmode) {
	case BPF_BUFMODE_BUFFER:
		return (bpf_buffer_append_mbuf(d, buf, offset, src, len));

	case BPF_BUFMODE_ZBUF:
		d->bd_zcopy++;
		return (bpf_zerocopy_append_mbuf(d, buf, offset, src, len));

	default:
		panic("bpf_buf_append_mbuf");
	}
}

/*
 * This function gets called when the free buffer is re-assigned.
 */
static void
bpf_buf_reclaimed(struct bpf_d *d)
{

	BPFD_LOCK_ASSERT(d);

	switch (d->bd_bufmode) {
	case BPF_BUFMODE_BUFFER:
		return;

	case BPF_BUFMODE_ZBUF:
		bpf_zerocopy_buf_reclaimed(d);
		return;

	default:
		panic("bpf_buf_reclaimed");
	}
}

/*
 * If the buffer mechanism has a way to decide that a held buffer can be made
 * free, then it is exposed via the bpf_canfreebuf() interface.  (1) is
 * returned if the buffer can be discarded, (0) is returned if it cannot.
 */
static int
bpf_canfreebuf(struct bpf_d *d)
{

	BPFD_LOCK_ASSERT(d);

	switch (d->bd_bufmode) {
	case BPF_BUFMODE_ZBUF:
		return (bpf_zerocopy_canfreebuf(d));
	}
	return (0);
}

/*
 * Allow the buffer model to indicate that the current store buffer is
 * immutable, regardless of the appearance of space.  Return (1) if the
 * buffer is writable, and (0) if not.
 */
static int
bpf_canwritebuf(struct bpf_d *d)
{

	BPFD_LOCK_ASSERT(d);

	switch (d->bd_bufmode) {
	case BPF_BUFMODE_ZBUF:
		return (bpf_zerocopy_canwritebuf(d));
	}
	return (1);
}

/*
 * Notify buffer model that an attempt to write to the store buffer has
 * resulted in a dropped packet, in which case the buffer may be considered
 * full.
 */
static void
bpf_buffull(struct bpf_d *d)
{

	BPFD_LOCK_ASSERT(d);

	switch (d->bd_bufmode) {
	case BPF_BUFMODE_ZBUF:
		bpf_zerocopy_buffull(d);
		break;
	}
}

/*
 * Notify the buffer model that a buffer has moved into the hold position.
 */
void
bpf_bufheld(struct bpf_d *d)
{

	BPFD_LOCK_ASSERT(d);

	switch (d->bd_bufmode) {
	case BPF_BUFMODE_ZBUF:
		bpf_zerocopy_bufheld(d);
		break;
	}
}

static void
bpf_free(struct bpf_d *d)
{

	switch (d->bd_bufmode) {
	case BPF_BUFMODE_BUFFER:
		return (bpf_buffer_free(d));

	case BPF_BUFMODE_ZBUF:
		return (bpf_zerocopy_free(d));

	default:
		panic("bpf_buf_free");
	}
}

static int
bpf_uiomove(struct bpf_d *d, caddr_t buf, u_int len, struct uio *uio)
{

	if (d->bd_bufmode != BPF_BUFMODE_BUFFER)
		return (EOPNOTSUPP);
	return (bpf_buffer_uiomove(d, buf, len, uio));
}

static int
bpf_ioctl_sblen(struct bpf_d *d, u_int *i)
{

	if (d->bd_bufmode != BPF_BUFMODE_BUFFER)
		return (EOPNOTSUPP);
	return (bpf_buffer_ioctl_sblen(d, i));
}

static int
bpf_ioctl_getzmax(struct thread *td, struct bpf_d *d, size_t *i)
{

	if (d->bd_bufmode != BPF_BUFMODE_ZBUF)
		return (EOPNOTSUPP);
	return (bpf_zerocopy_ioctl_getzmax(td, d, i));
}

static int
bpf_ioctl_rotzbuf(struct thread *td, struct bpf_d *d, struct bpf_zbuf *bz)
{

	if (d->bd_bufmode != BPF_BUFMODE_ZBUF)
		return (EOPNOTSUPP);
	return (bpf_zerocopy_ioctl_rotzbuf(td, d, bz));
}

static int
bpf_ioctl_setzbuf(struct thread *td, struct bpf_d *d, struct bpf_zbuf *bz)
{

	if (d->bd_bufmode != BPF_BUFMODE_ZBUF)
		return (EOPNOTSUPP);
	return (bpf_zerocopy_ioctl_setzbuf(td, d, bz));
}

/*
 * General BPF functions.
 */
static int
bpf_movein(struct uio *uio, int linktype, struct ifnet *ifp, struct mbuf **mp,
    struct sockaddr *sockp, int *hdrlen, struct bpf_insn *wfilter)
{
	const struct ieee80211_bpf_params *p;
	struct ether_header *eh;
	struct mbuf *m;
	int error;
	int len;
	int hlen;
	int slen;

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
		sockp->sa_family = AF_UNSPEC;
		hlen = 0;
		break;

	case DLT_NULL:
		/*
		 * null interface types require a 4 byte pseudo header which
		 * corresponds to the address family of the packet.
		 */
		sockp->sa_family = AF_UNSPEC;
		hlen = 4;
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

	case DLT_IEEE802_11:		/* IEEE 802.11 wireless */
		sockp->sa_family = AF_IEEE80211;
		hlen = 0;
		break;

	case DLT_IEEE802_11_RADIO:	/* IEEE 802.11 wireless w/ phy params */
		sockp->sa_family = AF_IEEE80211;
		sockp->sa_len = 12;	/* XXX != 0 */
		hlen = sizeof(struct ieee80211_bpf_params);
		break;

	default:
		return (EIO);
	}

	len = uio->uio_resid;

	if (len - hlen > ifp->if_mtu)
		return (EMSGSIZE);

	if ((unsigned)len > MJUM16BYTES)
		return (EIO);

	if (len <= MHLEN)
		MGETHDR(m, M_WAIT, MT_DATA);
	else if (len <= MCLBYTES)
		m = m_getcl(M_WAIT, MT_DATA, M_PKTHDR);
	else
		m = m_getjcl(M_WAIT, MT_DATA, M_PKTHDR,
#if (MJUMPAGESIZE > MCLBYTES)
		    len <= MJUMPAGESIZE ? MJUMPAGESIZE :
#endif
		    (len <= MJUM9BYTES ? MJUM9BYTES : MJUM16BYTES));
	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = NULL;
	*mp = m;

	if (m->m_len < hlen) {
		error = EPERM;
		goto bad;
	}

	error = uiomove(mtod(m, u_char *), len, uio);
	if (error)
		goto bad;

	slen = bpf_filter(wfilter, mtod(m, u_char *), len, len);
	if (slen == 0) {
		error = EPERM;
		goto bad;
	}

	/* Check for multicast destination */
	switch (linktype) {
	case DLT_EN10MB:
		eh = mtod(m, struct ether_header *);
		if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
			if (bcmp(ifp->if_broadcastaddr, eh->ether_dhost,
			    ETHER_ADDR_LEN) == 0)
				m->m_flags |= M_BCAST;
			else
				m->m_flags |= M_MCAST;
		}
		break;
	}

	/*
	 * Make room for link header, and copy it to sockaddr
	 */
	if (hlen != 0) {
		if (sockp->sa_family == AF_IEEE80211) {
			/*
			 * Collect true length from the parameter header
			 * NB: sockp is known to be zero'd so if we do a
			 *     short copy unspecified parameters will be
			 *     zero.
			 * NB: packet may not be aligned after stripping
			 *     bpf params
			 * XXX check ibp_vers
			 */
			p = mtod(m, const struct ieee80211_bpf_params *);
			hlen = p->ibp_len;
			if (hlen > sizeof(sockp->sa_data)) {
				error = EINVAL;
				goto bad;
			}
		}
		bcopy(m->m_data, sockp->sa_data, hlen);
	}
	*hdrlen = hlen;

	return (0);
bad:
	m_freem(m);
	return (error);
}

/*
 * Attach file to the bpf interface, i.e. make d listen on bp.
 */
static void
bpf_attachd(struct bpf_d *d, struct bpf_if *bp)
{
	/*
	 * Point d at bp, and add d to the interface's list of listeners.
	 * Finally, point the driver's bpf cookie at the interface so
	 * it will divert packets to bpf.
	 */
	BPFIF_LOCK(bp);
	d->bd_bif = bp;
	LIST_INSERT_HEAD(&bp->bif_dlist, d, bd_next);

	bpf_bpfd_cnt++;
	BPFIF_UNLOCK(bp);

	EVENTHANDLER_INVOKE(bpf_track, bp->bif_ifp, bp->bif_dlt, 1);
}

/*
 * Detach a file from its interface.
 */
static void
bpf_detachd(struct bpf_d *d)
{
	int error;
	struct bpf_if *bp;
	struct ifnet *ifp;

	bp = d->bd_bif;
	BPFIF_LOCK(bp);
	BPFD_LOCK(d);
	ifp = d->bd_bif->bif_ifp;

	/*
	 * Remove d from the interface's descriptor list.
	 */
	LIST_REMOVE(d, bd_next);

	bpf_bpfd_cnt--;
	d->bd_bif = NULL;
	BPFD_UNLOCK(d);
	BPFIF_UNLOCK(bp);

	EVENTHANDLER_INVOKE(bpf_track, ifp, bp->bif_dlt, 0);

	/*
	 * Check if this descriptor had requested promiscuous mode.
	 * If so, turn it off.
	 */
	if (d->bd_promisc) {
		d->bd_promisc = 0;
		CURVNET_SET(ifp->if_vnet);
		error = ifpromisc(ifp, 0);
		CURVNET_RESTORE();
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
}

/*
 * Close the descriptor by detaching it from its interface,
 * deallocating its buffers, and marking it free.
 */
static void
bpf_dtor(void *data)
{
	struct bpf_d *d = data;

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
	mac_bpfdesc_destroy(d);
#endif /* MAC */
	seldrain(&d->bd_sel);
	knlist_destroy(&d->bd_sel.si_note);
	callout_drain(&d->bd_callout);
	bpf_freed(d);
	free(d, M_BPF);
}

/*
 * Open ethernet device.  Returns ENXIO for illegal minor device number,
 * EBUSY if file is open by another process.
 */
/* ARGSUSED */
static	int
bpfopen(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct bpf_d *d;
	int error;

	d = malloc(sizeof(*d), M_BPF, M_WAITOK | M_ZERO);
	error = devfs_set_cdevpriv(d, bpf_dtor);
	if (error != 0) {
		free(d, M_BPF);
		return (error);
	}

	/*
	 * For historical reasons, perform a one-time initialization call to
	 * the buffer routines, even though we're not yet committed to a
	 * particular buffer method.
	 */
	bpf_buffer_init(d);
	d->bd_bufmode = BPF_BUFMODE_BUFFER;
	d->bd_sig = SIGIO;
	d->bd_direction = BPF_D_INOUT;
	d->bd_pid = td->td_proc->p_pid;
#ifdef MAC
	mac_bpfdesc_init(d);
	mac_bpfdesc_create(td->td_ucred, d);
#endif
	mtx_init(&d->bd_mtx, devtoname(dev), "bpf cdev lock", MTX_DEF);
	callout_init_mtx(&d->bd_callout, &d->bd_mtx, 0);
	knlist_init_mtx(&d->bd_sel.si_note, &d->bd_mtx);

	return (0);
}

/*
 *  bpfread - read next chunk of packets from buffers
 */
static	int
bpfread(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct bpf_d *d;
	int error;
	int non_block;
	int timed_out;

	error = devfs_get_cdevpriv((void **)&d);
	if (error != 0)
		return (error);

	/*
	 * Restrict application to use a buffer the same size as
	 * as kernel buffers.
	 */
	if (uio->uio_resid != d->bd_bufsize)
		return (EINVAL);

	non_block = ((ioflag & O_NONBLOCK) != 0);

	BPFD_LOCK(d);
	d->bd_pid = curthread->td_proc->p_pid;
	if (d->bd_bufmode != BPF_BUFMODE_BUFFER) {
		BPFD_UNLOCK(d);
		return (EOPNOTSUPP);
	}
	if (d->bd_state == BPF_WAITING)
		callout_stop(&d->bd_callout);
	timed_out = (d->bd_state == BPF_TIMED_OUT);
	d->bd_state = BPF_IDLE;
	/*
	 * If the hold buffer is empty, then do a timed sleep, which
	 * ends when the timeout expires or when enough packets
	 * have arrived to fill the store buffer.
	 */
	while (d->bd_hbuf == NULL) {
		if (d->bd_slen != 0) {
			/*
			 * A packet(s) either arrived since the previous
			 * read or arrived while we were asleep.
			 */
			if (d->bd_immediate || non_block || timed_out) {
				/*
				 * Rotate the buffers and return what's here
				 * if we are in immediate mode, non-blocking
				 * flag is set, or this descriptor timed out.
				 */
				ROTATE_BUFFERS(d);
				break;
			}
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

		if (non_block) {
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
	 *
	 * XXXRW: More synchronization needed here: what if a second thread
	 * issues a read on the same fd at the same time?  Don't want this
	 * getting invalidated.
	 */
	error = bpf_uiomove(d, d->bd_hbuf, d->bd_hlen, uio);

	BPFD_LOCK(d);
	d->bd_fbuf = d->bd_hbuf;
	d->bd_hbuf = NULL;
	d->bd_hlen = 0;
	bpf_buf_reclaimed(d);
	BPFD_UNLOCK(d);

	return (error);
}

/*
 * If there are processes sleeping on this descriptor, wake them up.
 */
static __inline void
bpf_wakeup(struct bpf_d *d)
{

	BPFD_LOCK_ASSERT(d);
	if (d->bd_state == BPF_WAITING) {
		callout_stop(&d->bd_callout);
		d->bd_state = BPF_IDLE;
	}
	wakeup(d);
	if (d->bd_async && d->bd_sig && d->bd_sigio)
		pgsigio(&d->bd_sigio, d->bd_sig, 0);

	selwakeuppri(&d->bd_sel, PRINET);
	KNOTE_LOCKED(&d->bd_sel.si_note, 0);
}

static void
bpf_timed_out(void *arg)
{
	struct bpf_d *d = (struct bpf_d *)arg;

	BPFD_LOCK_ASSERT(d);

	if (callout_pending(&d->bd_callout) || !callout_active(&d->bd_callout))
		return;
	if (d->bd_state == BPF_WAITING) {
		d->bd_state = BPF_TIMED_OUT;
		if (d->bd_slen != 0)
			bpf_wakeup(d);
	}
}

static int
bpf_ready(struct bpf_d *d)
{

	BPFD_LOCK_ASSERT(d);

	if (!bpf_canfreebuf(d) && d->bd_hlen != 0)
		return (1);
	if ((d->bd_immediate || d->bd_state == BPF_TIMED_OUT) &&
	    d->bd_slen != 0)
		return (1);
	return (0);
}

static int
bpfwrite(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct bpf_d *d;
	struct ifnet *ifp;
	struct mbuf *m, *mc;
	struct sockaddr dst;
	int error, hlen;

	error = devfs_get_cdevpriv((void **)&d);
	if (error != 0)
		return (error);

	d->bd_pid = curthread->td_proc->p_pid;
	d->bd_wcount++;
	if (d->bd_bif == NULL) {
		d->bd_wdcount++;
		return (ENXIO);
	}

	ifp = d->bd_bif->bif_ifp;

	if ((ifp->if_flags & IFF_UP) == 0) {
		d->bd_wdcount++;
		return (ENETDOWN);
	}

	if (uio->uio_resid == 0) {
		d->bd_wdcount++;
		return (0);
	}

	bzero(&dst, sizeof(dst));
	m = NULL;
	hlen = 0;
	error = bpf_movein(uio, (int)d->bd_bif->bif_dlt, ifp,
	    &m, &dst, &hlen, d->bd_wfilter);
	if (error) {
		d->bd_wdcount++;
		return (error);
	}
	d->bd_wfcount++;
	if (d->bd_hdrcmplt)
		dst.sa_family = pseudo_AF_HDRCMPLT;

	if (d->bd_feedback) {
		mc = m_dup(m, M_DONTWAIT);
		if (mc != NULL)
			mc->m_pkthdr.rcvif = ifp;
		/* Set M_PROMISC for outgoing packets to be discarded. */
		if (d->bd_direction == BPF_D_INOUT)
			m->m_flags |= M_PROMISC;
	} else
		mc = NULL;

	m->m_pkthdr.len -= hlen;
	m->m_len -= hlen;
	m->m_data += hlen;	/* XXX */

	CURVNET_SET(ifp->if_vnet);
#ifdef MAC
	BPFD_LOCK(d);
	mac_bpfdesc_create_mbuf(d, m);
	if (mc != NULL)
		mac_bpfdesc_create_mbuf(d, mc);
	BPFD_UNLOCK(d);
#endif

	error = (*ifp->if_output)(ifp, m, &dst, NULL);
	if (error)
		d->bd_wdcount++;

	if (mc != NULL) {
		if (error == 0)
			(*ifp->if_input)(ifp, mc);
		else
			m_freem(mc);
	}
	CURVNET_RESTORE();

	return (error);
}

/*
 * Reset a descriptor by flushing its packet buffer and clearing the receive
 * and drop counts.  This is doable for kernel-only buffers, but with
 * zero-copy buffers, we can't write to (or rotate) buffers that are
 * currently owned by userspace.  It would be nice if we could encapsulate
 * this logic in the buffer code rather than here.
 */
static void
reset_d(struct bpf_d *d)
{

	mtx_assert(&d->bd_mtx, MA_OWNED);

	if ((d->bd_hbuf != NULL) &&
	    (d->bd_bufmode != BPF_BUFMODE_ZBUF || bpf_canfreebuf(d))) {
		/* Free the hold buffer. */
		d->bd_fbuf = d->bd_hbuf;
		d->bd_hbuf = NULL;
		d->bd_hlen = 0;
		bpf_buf_reclaimed(d);
	}
	if (bpf_canwritebuf(d))
		d->bd_slen = 0;
	d->bd_rcount = 0;
	d->bd_dcount = 0;
	d->bd_fcount = 0;
	d->bd_wcount = 0;
	d->bd_wfcount = 0;
	d->bd_wdcount = 0;
	d->bd_zcopy = 0;
}

/*
 *  FIONREAD		Check for read packet available.
 *  SIOCGIFADDR		Get interface address - convenient hook to driver.
 *  BIOCGBLEN		Get buffer len [for read()].
 *  BIOCSETF		Set read filter.
 *  BIOCSETFNR		Set read filter without resetting descriptor.
 *  BIOCSETWF		Set write filter.
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
 *  BIOCGDIRECTION	Get packet direction flag
 *  BIOCSDIRECTION	Set packet direction flag
 *  BIOCGTSTAMP		Get time stamp format and resolution.
 *  BIOCSTSTAMP		Set time stamp format and resolution.
 *  BIOCLOCK		Set "locked" flag
 *  BIOCFEEDBACK	Set packet feedback mode.
 *  BIOCSETZBUF		Set current zero-copy buffer locations.
 *  BIOCGETZMAX		Get maximum zero-copy buffer size.
 *  BIOCROTZBUF		Force rotation of zero-copy buffer
 *  BIOCSETBUFMODE	Set buffer mode.
 *  BIOCGETBUFMODE	Get current buffer mode.
 */
/* ARGSUSED */
static	int
bpfioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flags,
    struct thread *td)
{
	struct bpf_d *d;
	int error;

	error = devfs_get_cdevpriv((void **)&d);
	if (error != 0)
		return (error);

	/*
	 * Refresh PID associated with this descriptor.
	 */
	BPFD_LOCK(d);
	d->bd_pid = td->td_proc->p_pid;
	if (d->bd_state == BPF_WAITING)
		callout_stop(&d->bd_callout);
	d->bd_state = BPF_IDLE;
	BPFD_UNLOCK(d);

	if (d->bd_locked == 1) {
		switch (cmd) {
		case BIOCGBLEN:
		case BIOCFLUSH:
		case BIOCGDLT:
		case BIOCGDLTLIST:
#ifdef COMPAT_FREEBSD32
		case BIOCGDLTLIST32:
#endif
		case BIOCGETIF:
		case BIOCGRTIMEOUT:
#ifdef COMPAT_FREEBSD32
		case BIOCGRTIMEOUT32:
#endif
		case BIOCGSTATS:
		case BIOCVERSION:
		case BIOCGRSIG:
		case BIOCGHDRCMPLT:
		case BIOCSTSTAMP:
		case BIOCFEEDBACK:
		case FIONREAD:
		case BIOCLOCK:
		case BIOCSRTIMEOUT:
#ifdef COMPAT_FREEBSD32
		case BIOCSRTIMEOUT32:
#endif
		case BIOCIMMEDIATE:
		case TIOCGPGRP:
		case BIOCROTZBUF:
			break;
		default:
			return (EPERM);
		}
	}
#ifdef COMPAT_FREEBSD32
	/*
	 * If we see a 32-bit compat ioctl, mark the stream as 32-bit so
	 * that it will get 32-bit packet headers.
	 */
	switch (cmd) {
	case BIOCSETF32:
	case BIOCSETFNR32:
	case BIOCSETWF32:
	case BIOCGDLTLIST32:
	case BIOCGRTIMEOUT32:
	case BIOCSRTIMEOUT32:
		d->bd_compat32 = 1;
	}
#endif

	CURVNET_SET(TD_TO_VNET(td));
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

			if (d->bd_bif == NULL)
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
		error = bpf_ioctl_sblen(d, (u_int *)addr);
		break;

	/*
	 * Set link layer read filter.
	 */
	case BIOCSETF:
	case BIOCSETFNR:
	case BIOCSETWF:
#ifdef COMPAT_FREEBSD32
	case BIOCSETF32:
	case BIOCSETFNR32:
	case BIOCSETWF32:
#endif
		error = bpf_setf(d, (struct bpf_program *)addr, cmd);
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
		if (d->bd_bif == NULL) {
			/*
			 * No interface attached yet.
			 */
			error = EINVAL;
			break;
		}
		if (d->bd_promisc == 0) {
			error = ifpromisc(d->bd_bif->bif_ifp, 1);
			if (error == 0)
				d->bd_promisc = 1;
		}
		break;

	/*
	 * Get current data link type.
	 */
	case BIOCGDLT:
		if (d->bd_bif == NULL)
			error = EINVAL;
		else
			*(u_int *)addr = d->bd_bif->bif_dlt;
		break;

	/*
	 * Get a list of supported data link types.
	 */
#ifdef COMPAT_FREEBSD32
	case BIOCGDLTLIST32:
		{
			struct bpf_dltlist32 *list32;
			struct bpf_dltlist dltlist;

			list32 = (struct bpf_dltlist32 *)addr;
			dltlist.bfl_len = list32->bfl_len;
			dltlist.bfl_list = PTRIN(list32->bfl_list);
			if (d->bd_bif == NULL)
				error = EINVAL;
			else {
				error = bpf_getdltlist(d, &dltlist);
				if (error == 0)
					list32->bfl_len = dltlist.bfl_len;
			}
			break;
		}
#endif

	case BIOCGDLTLIST:
		if (d->bd_bif == NULL)
			error = EINVAL;
		else
			error = bpf_getdltlist(d, (struct bpf_dltlist *)addr);
		break;

	/*
	 * Set data link type.
	 */
	case BIOCSDLT:
		if (d->bd_bif == NULL)
			error = EINVAL;
		else
			error = bpf_setdlt(d, *(u_int *)addr);
		break;

	/*
	 * Get interface name.
	 */
	case BIOCGETIF:
		if (d->bd_bif == NULL)
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
#ifdef COMPAT_FREEBSD32
	case BIOCSRTIMEOUT32:
#endif
		{
			struct timeval *tv = (struct timeval *)addr;
#ifdef COMPAT_FREEBSD32
			struct timeval32 *tv32;
			struct timeval tv64;

			if (cmd == BIOCSRTIMEOUT32) {
				tv32 = (struct timeval32 *)addr;
				tv = &tv64;
				tv->tv_sec = tv32->tv_sec;
				tv->tv_usec = tv32->tv_usec;
			} else
#endif
				tv = (struct timeval *)addr;

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
#ifdef COMPAT_FREEBSD32
	case BIOCGRTIMEOUT32:
#endif
		{
			struct timeval *tv;
#ifdef COMPAT_FREEBSD32
			struct timeval32 *tv32;
			struct timeval tv64;

			if (cmd == BIOCGRTIMEOUT32)
				tv = &tv64;
			else
#endif
				tv = (struct timeval *)addr;

			tv->tv_sec = d->bd_rtout / hz;
			tv->tv_usec = (d->bd_rtout % hz) * tick;
#ifdef COMPAT_FREEBSD32
			if (cmd == BIOCGRTIMEOUT32) {
				tv32 = (struct timeval32 *)addr;
				tv32->tv_sec = tv->tv_sec;
				tv32->tv_usec = tv->tv_usec;
			}
#endif

			break;
		}

	/*
	 * Get packet stats.
	 */
	case BIOCGSTATS:
		{
			struct bpf_stat *bs = (struct bpf_stat *)addr;

			/* XXXCSJP overflow */
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
	 * Get packet direction flag
	 */
	case BIOCGDIRECTION:
		*(u_int *)addr = d->bd_direction;
		break;

	/*
	 * Set packet direction flag
	 */
	case BIOCSDIRECTION:
		{
			u_int	direction;

			direction = *(u_int *)addr;
			switch (direction) {
			case BPF_D_IN:
			case BPF_D_INOUT:
			case BPF_D_OUT:
				d->bd_direction = direction;
				break;
			default:
				error = EINVAL;
			}
		}
		break;

	/*
	 * Get packet timestamp format and resolution.
	 */
	case BIOCGTSTAMP:
		*(u_int *)addr = d->bd_tstamp;
		break;

	/*
	 * Set packet timestamp format and resolution.
	 */
	case BIOCSTSTAMP:
		{
			u_int	func;

			func = *(u_int *)addr;
			if (BPF_T_VALID(func))
				d->bd_tstamp = func;
			else
				error = EINVAL;
		}
		break;

	case BIOCFEEDBACK:
		d->bd_feedback = *(u_int *)addr;
		break;

	case BIOCLOCK:
		d->bd_locked = 1;
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

	case BIOCGETBUFMODE:
		*(u_int *)addr = d->bd_bufmode;
		break;

	case BIOCSETBUFMODE:
		/*
		 * Allow the buffering mode to be changed as long as we
		 * haven't yet committed to a particular mode.  Our
		 * definition of commitment, for now, is whether or not a
		 * buffer has been allocated or an interface attached, since
		 * that's the point where things get tricky.
		 */
		switch (*(u_int *)addr) {
		case BPF_BUFMODE_BUFFER:
			break;

		case BPF_BUFMODE_ZBUF:
			if (bpf_zerocopy_enable)
				break;
			/* FALLSTHROUGH */

		default:
			CURVNET_RESTORE();
			return (EINVAL);
		}

		BPFD_LOCK(d);
		if (d->bd_sbuf != NULL || d->bd_hbuf != NULL ||
		    d->bd_fbuf != NULL || d->bd_bif != NULL) {
			BPFD_UNLOCK(d);
			CURVNET_RESTORE();
			return (EBUSY);
		}
		d->bd_bufmode = *(u_int *)addr;
		BPFD_UNLOCK(d);
		break;

	case BIOCGETZMAX:
		error = bpf_ioctl_getzmax(td, d, (size_t *)addr);
		break;

	case BIOCSETZBUF:
		error = bpf_ioctl_setzbuf(td, d, (struct bpf_zbuf *)addr);
		break;

	case BIOCROTZBUF:
		error = bpf_ioctl_rotzbuf(td, d, (struct bpf_zbuf *)addr);
		break;
	}
	CURVNET_RESTORE();
	return (error);
}

/*
 * Set d's packet filter program to fp.  If this file already has a filter,
 * free it and replace it.  Returns EINVAL for bogus requests.
 */
static int
bpf_setf(struct bpf_d *d, struct bpf_program *fp, u_long cmd)
{
	struct bpf_insn *fcode, *old;
	u_int wfilter, flen, size;
#ifdef BPF_JITTER
	bpf_jit_filter *ofunc;
#endif
#ifdef COMPAT_FREEBSD32
	struct bpf_program32 *fp32;
	struct bpf_program fp_swab;

	if (cmd == BIOCSETWF32 || cmd == BIOCSETF32 || cmd == BIOCSETFNR32) {
		fp32 = (struct bpf_program32 *)fp;
		fp_swab.bf_len = fp32->bf_len;
		fp_swab.bf_insns = (struct bpf_insn *)(uintptr_t)fp32->bf_insns;
		fp = &fp_swab;
		if (cmd == BIOCSETWF32)
			cmd = BIOCSETWF;
	}
#endif
	if (cmd == BIOCSETWF) {
		old = d->bd_wfilter;
		wfilter = 1;
#ifdef BPF_JITTER
		ofunc = NULL;
#endif
	} else {
		wfilter = 0;
		old = d->bd_rfilter;
#ifdef BPF_JITTER
		ofunc = d->bd_bfilter;
#endif
	}
	if (fp->bf_insns == NULL) {
		if (fp->bf_len != 0)
			return (EINVAL);
		BPFD_LOCK(d);
		if (wfilter)
			d->bd_wfilter = NULL;
		else {
			d->bd_rfilter = NULL;
#ifdef BPF_JITTER
			d->bd_bfilter = NULL;
#endif
			if (cmd == BIOCSETF)
				reset_d(d);
		}
		BPFD_UNLOCK(d);
		if (old != NULL)
			free((caddr_t)old, M_BPF);
#ifdef BPF_JITTER
		if (ofunc != NULL)
			bpf_destroy_jit_filter(ofunc);
#endif
		return (0);
	}
	flen = fp->bf_len;
	if (flen > bpf_maxinsns)
		return (EINVAL);

	size = flen * sizeof(*fp->bf_insns);
	fcode = (struct bpf_insn *)malloc(size, M_BPF, M_WAITOK);
	if (copyin((caddr_t)fp->bf_insns, (caddr_t)fcode, size) == 0 &&
	    bpf_validate(fcode, (int)flen)) {
		BPFD_LOCK(d);
		if (wfilter)
			d->bd_wfilter = fcode;
		else {
			d->bd_rfilter = fcode;
#ifdef BPF_JITTER
			d->bd_bfilter = bpf_jitter(fcode, flen);
#endif
			if (cmd == BIOCSETF)
				reset_d(d);
		}
		BPFD_UNLOCK(d);
		if (old != NULL)
			free((caddr_t)old, M_BPF);
#ifdef BPF_JITTER
		if (ofunc != NULL)
			bpf_destroy_jit_filter(ofunc);
#endif

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
bpf_setif(struct bpf_d *d, struct ifreq *ifr)
{
	struct bpf_if *bp;
	struct ifnet *theywant;

	theywant = ifunit(ifr->ifr_name);
	if (theywant == NULL || theywant->if_bpf == NULL)
		return (ENXIO);

	bp = theywant->if_bpf;

	/*
	 * Behavior here depends on the buffering model.  If we're using
	 * kernel memory buffers, then we can allocate them here.  If we're
	 * using zero-copy, then the user process must have registered
	 * buffers by the time we get here.  If not, return an error.
	 *
	 * XXXRW: There are locking issues here with multi-threaded use: what
	 * if two threads try to set the interface at once?
	 */
	switch (d->bd_bufmode) {
	case BPF_BUFMODE_BUFFER:
		if (d->bd_sbuf == NULL)
			bpf_buffer_alloc(d);
		KASSERT(d->bd_sbuf != NULL, ("bpf_setif: bd_sbuf NULL"));
		break;

	case BPF_BUFMODE_ZBUF:
		if (d->bd_sbuf == NULL)
			return (EINVAL);
		break;

	default:
		panic("bpf_setif: bufmode %d", d->bd_bufmode);
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

/*
 * Support for select() and poll() system calls
 *
 * Return true iff the specific operation will not block indefinitely.
 * Otherwise, return false but make a note that a selwakeup() must be done.
 */
static int
bpfpoll(struct cdev *dev, int events, struct thread *td)
{
	struct bpf_d *d;
	int revents;

	if (devfs_get_cdevpriv((void **)&d) != 0 || d->bd_bif == NULL)
		return (events &
		    (POLLHUP|POLLIN|POLLRDNORM|POLLOUT|POLLWRNORM));

	/*
	 * Refresh PID associated with this descriptor.
	 */
	revents = events & (POLLOUT | POLLWRNORM);
	BPFD_LOCK(d);
	d->bd_pid = td->td_proc->p_pid;
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
bpfkqfilter(struct cdev *dev, struct knote *kn)
{
	struct bpf_d *d;

	if (devfs_get_cdevpriv((void **)&d) != 0 ||
	    kn->kn_filter != EVFILT_READ)
		return (1);

	/*
	 * Refresh PID associated with this descriptor.
	 */
	BPFD_LOCK(d);
	d->bd_pid = curthread->td_proc->p_pid;
	kn->kn_fop = &bpfread_filtops;
	kn->kn_hook = d;
	knlist_add(&d->bd_sel.si_note, kn, 1);
	BPFD_UNLOCK(d);

	return (0);
}

static void
filt_bpfdetach(struct knote *kn)
{
	struct bpf_d *d = (struct bpf_d *)kn->kn_hook;

	knlist_remove(&d->bd_sel.si_note, kn, 0);
}

static int
filt_bpfread(struct knote *kn, long hint)
{
	struct bpf_d *d = (struct bpf_d *)kn->kn_hook;
	int ready;

	BPFD_LOCK_ASSERT(d);
	ready = bpf_ready(d);
	if (ready) {
		kn->kn_data = d->bd_slen;
		if (d->bd_hbuf)
			kn->kn_data += d->bd_hlen;
	} else if (d->bd_rtout > 0 && d->bd_state == BPF_IDLE) {
		callout_reset(&d->bd_callout, d->bd_rtout,
		    bpf_timed_out, d);
		d->bd_state = BPF_WAITING;
	}

	return (ready);
}

#define	BPF_TSTAMP_NONE		0
#define	BPF_TSTAMP_FAST		1
#define	BPF_TSTAMP_NORMAL	2
#define	BPF_TSTAMP_EXTERN	3

static int
bpf_ts_quality(int tstype)
{

	if (tstype == BPF_T_NONE)
		return (BPF_TSTAMP_NONE);
	if ((tstype & BPF_T_FAST) != 0)
		return (BPF_TSTAMP_FAST);

	return (BPF_TSTAMP_NORMAL);
}

static int
bpf_gettime(struct bintime *bt, int tstype, struct mbuf *m)
{
	struct m_tag *tag;
	int quality;

	quality = bpf_ts_quality(tstype);
	if (quality == BPF_TSTAMP_NONE)
		return (quality);

	if (m != NULL) {
		tag = m_tag_locate(m, MTAG_BPF, MTAG_BPF_TIMESTAMP, NULL);
		if (tag != NULL) {
			*bt = *(struct bintime *)(tag + 1);
			return (BPF_TSTAMP_EXTERN);
		}
	}
	if (quality == BPF_TSTAMP_NORMAL)
		binuptime(bt);
	else
		getbinuptime(bt);

	return (quality);
}

/*
 * Incoming linkage from device drivers.  Process the packet pkt, of length
 * pktlen, which is stored in a contiguous buffer.  The packet is parsed
 * by each process' filter, and if accepted, stashed into the corresponding
 * buffer.
 */
void
bpf_tap(struct bpf_if *bp, u_char *pkt, u_int pktlen)
{
	struct bintime bt;
	struct bpf_d *d;
#ifdef BPF_JITTER
	bpf_jit_filter *bf;
#endif
	u_int slen;
	int gottime;

	gottime = BPF_TSTAMP_NONE;
	BPFIF_LOCK(bp);
	LIST_FOREACH(d, &bp->bif_dlist, bd_next) {
		BPFD_LOCK(d);
		++d->bd_rcount;
		/*
		 * NB: We dont call BPF_CHECK_DIRECTION() here since there is no
		 * way for the caller to indiciate to us whether this packet
		 * is inbound or outbound.  In the bpf_mtap() routines, we use
		 * the interface pointers on the mbuf to figure it out.
		 */
#ifdef BPF_JITTER
		bf = bpf_jitter_enable != 0 ? d->bd_bfilter : NULL;
		if (bf != NULL)
			slen = (*(bf->func))(pkt, pktlen, pktlen);
		else
#endif
		slen = bpf_filter(d->bd_rfilter, pkt, pktlen, pktlen);
		if (slen != 0) {
			d->bd_fcount++;
			if (gottime < bpf_ts_quality(d->bd_tstamp))
				gottime = bpf_gettime(&bt, d->bd_tstamp, NULL);
#ifdef MAC
			if (mac_bpfdesc_check_receive(d, bp->bif_ifp) == 0)
#endif
				catchpacket(d, pkt, pktlen, slen,
				    bpf_append_bytes, &bt);
		}
		BPFD_UNLOCK(d);
	}
	BPFIF_UNLOCK(bp);
}

#define	BPF_CHECK_DIRECTION(d, r, i)				\
	    (((d)->bd_direction == BPF_D_IN && (r) != (i)) ||	\
	    ((d)->bd_direction == BPF_D_OUT && (r) == (i)))

/*
 * Incoming linkage from device drivers, when packet is in an mbuf chain.
 */
void
bpf_mtap(struct bpf_if *bp, struct mbuf *m)
{
	struct bintime bt;
	struct bpf_d *d;
#ifdef BPF_JITTER
	bpf_jit_filter *bf;
#endif
	u_int pktlen, slen;
	int gottime;

	/* Skip outgoing duplicate packets. */
	if ((m->m_flags & M_PROMISC) != 0 && m->m_pkthdr.rcvif == NULL) {
		m->m_flags &= ~M_PROMISC;
		return;
	}

	pktlen = m_length(m, NULL);

	gottime = BPF_TSTAMP_NONE;
	BPFIF_LOCK(bp);
	LIST_FOREACH(d, &bp->bif_dlist, bd_next) {
		if (BPF_CHECK_DIRECTION(d, m->m_pkthdr.rcvif, bp->bif_ifp))
			continue;
		BPFD_LOCK(d);
		++d->bd_rcount;
#ifdef BPF_JITTER
		bf = bpf_jitter_enable != 0 ? d->bd_bfilter : NULL;
		/* XXX We cannot handle multiple mbufs. */
		if (bf != NULL && m->m_next == NULL)
			slen = (*(bf->func))(mtod(m, u_char *), pktlen, pktlen);
		else
#endif
		slen = bpf_filter(d->bd_rfilter, (u_char *)m, pktlen, 0);
		if (slen != 0) {
			d->bd_fcount++;
			if (gottime < bpf_ts_quality(d->bd_tstamp))
				gottime = bpf_gettime(&bt, d->bd_tstamp, m);
#ifdef MAC
			if (mac_bpfdesc_check_receive(d, bp->bif_ifp) == 0)
#endif
				catchpacket(d, (u_char *)m, pktlen, slen,
				    bpf_append_mbuf, &bt);
		}
		BPFD_UNLOCK(d);
	}
	BPFIF_UNLOCK(bp);
}

/*
 * Incoming linkage from device drivers, when packet is in
 * an mbuf chain and to be prepended by a contiguous header.
 */
void
bpf_mtap2(struct bpf_if *bp, void *data, u_int dlen, struct mbuf *m)
{
	struct bintime bt;
	struct mbuf mb;
	struct bpf_d *d;
	u_int pktlen, slen;
	int gottime;

	/* Skip outgoing duplicate packets. */
	if ((m->m_flags & M_PROMISC) != 0 && m->m_pkthdr.rcvif == NULL) {
		m->m_flags &= ~M_PROMISC;
		return;
	}

	pktlen = m_length(m, NULL);
	/*
	 * Craft on-stack mbuf suitable for passing to bpf_filter.
	 * Note that we cut corners here; we only setup what's
	 * absolutely needed--this mbuf should never go anywhere else.
	 */
	mb.m_next = m;
	mb.m_data = data;
	mb.m_len = dlen;
	pktlen += dlen;

	gottime = BPF_TSTAMP_NONE;
	BPFIF_LOCK(bp);
	LIST_FOREACH(d, &bp->bif_dlist, bd_next) {
		if (BPF_CHECK_DIRECTION(d, m->m_pkthdr.rcvif, bp->bif_ifp))
			continue;
		BPFD_LOCK(d);
		++d->bd_rcount;
		slen = bpf_filter(d->bd_rfilter, (u_char *)&mb, pktlen, 0);
		if (slen != 0) {
			d->bd_fcount++;
			if (gottime < bpf_ts_quality(d->bd_tstamp))
				gottime = bpf_gettime(&bt, d->bd_tstamp, m);
#ifdef MAC
			if (mac_bpfdesc_check_receive(d, bp->bif_ifp) == 0)
#endif
				catchpacket(d, (u_char *)&mb, pktlen, slen,
				    bpf_append_mbuf, &bt);
		}
		BPFD_UNLOCK(d);
	}
	BPFIF_UNLOCK(bp);
}

#undef	BPF_CHECK_DIRECTION

#undef	BPF_TSTAMP_NONE
#undef	BPF_TSTAMP_FAST
#undef	BPF_TSTAMP_NORMAL
#undef	BPF_TSTAMP_EXTERN

static int
bpf_hdrlen(struct bpf_d *d)
{
	int hdrlen;

	hdrlen = d->bd_bif->bif_hdrlen;
#ifndef BURN_BRIDGES
	if (d->bd_tstamp == BPF_T_NONE ||
	    BPF_T_FORMAT(d->bd_tstamp) == BPF_T_MICROTIME)
#ifdef COMPAT_FREEBSD32
		if (d->bd_compat32)
			hdrlen += SIZEOF_BPF_HDR(struct bpf_hdr32);
		else
#endif
			hdrlen += SIZEOF_BPF_HDR(struct bpf_hdr);
	else
#endif
		hdrlen += SIZEOF_BPF_HDR(struct bpf_xhdr);
#ifdef COMPAT_FREEBSD32
	if (d->bd_compat32)
		hdrlen = BPF_WORDALIGN32(hdrlen);
	else
#endif
		hdrlen = BPF_WORDALIGN(hdrlen);

	return (hdrlen - d->bd_bif->bif_hdrlen);
}

static void
bpf_bintime2ts(struct bintime *bt, struct bpf_ts *ts, int tstype)
{
	struct bintime bt2;
	struct timeval tsm;
	struct timespec tsn;

	if ((tstype & BPF_T_MONOTONIC) == 0) {
		bt2 = *bt;
		bintime_add(&bt2, &boottimebin);
		bt = &bt2;
	}
	switch (BPF_T_FORMAT(tstype)) {
	case BPF_T_MICROTIME:
		bintime2timeval(bt, &tsm);
		ts->bt_sec = tsm.tv_sec;
		ts->bt_frac = tsm.tv_usec;
		break;
	case BPF_T_NANOTIME:
		bintime2timespec(bt, &tsn);
		ts->bt_sec = tsn.tv_sec;
		ts->bt_frac = tsn.tv_nsec;
		break;
	case BPF_T_BINTIME:
		ts->bt_sec = bt->sec;
		ts->bt_frac = bt->frac;
		break;
	}
}

/*
 * Move the packet data from interface memory (pkt) into the
 * store buffer.  "cpfn" is the routine called to do the actual data
 * transfer.  bcopy is passed in to copy contiguous chunks, while
 * bpf_append_mbuf is passed in to copy mbuf chains.  In the latter case,
 * pkt is really an mbuf.
 */
static void
catchpacket(struct bpf_d *d, u_char *pkt, u_int pktlen, u_int snaplen,
    void (*cpfn)(struct bpf_d *, caddr_t, u_int, void *, u_int),
    struct bintime *bt)
{
	struct bpf_xhdr hdr;
#ifndef BURN_BRIDGES
	struct bpf_hdr hdr_old;
#ifdef COMPAT_FREEBSD32
	struct bpf_hdr32 hdr32_old;
#endif
#endif
	int caplen, curlen, hdrlen, totlen;
	int do_wakeup = 0;
	int do_timestamp;
	int tstype;

	BPFD_LOCK_ASSERT(d);

	/*
	 * Detect whether user space has released a buffer back to us, and if
	 * so, move it from being a hold buffer to a free buffer.  This may
	 * not be the best place to do it (for example, we might only want to
	 * run this check if we need the space), but for now it's a reliable
	 * spot to do it.
	 */
	if (d->bd_fbuf == NULL && bpf_canfreebuf(d)) {
		d->bd_fbuf = d->bd_hbuf;
		d->bd_hbuf = NULL;
		d->bd_hlen = 0;
		bpf_buf_reclaimed(d);
	}

	/*
	 * Figure out how many bytes to move.  If the packet is
	 * greater or equal to the snapshot length, transfer that
	 * much.  Otherwise, transfer the whole packet (unless
	 * we hit the buffer size limit).
	 */
	hdrlen = bpf_hdrlen(d);
	totlen = hdrlen + min(snaplen, pktlen);
	if (totlen > d->bd_bufsize)
		totlen = d->bd_bufsize;

	/*
	 * Round up the end of the previous packet to the next longword.
	 *
	 * Drop the packet if there's no room and no hope of room
	 * If the packet would overflow the storage buffer or the storage
	 * buffer is considered immutable by the buffer model, try to rotate
	 * the buffer and wakeup pending processes.
	 */
#ifdef COMPAT_FREEBSD32
	if (d->bd_compat32)
		curlen = BPF_WORDALIGN32(d->bd_slen);
	else
#endif
		curlen = BPF_WORDALIGN(d->bd_slen);
	if (curlen + totlen > d->bd_bufsize || !bpf_canwritebuf(d)) {
		if (d->bd_fbuf == NULL) {
			/*
			 * There's no room in the store buffer, and no
			 * prospect of room, so drop the packet.  Notify the
			 * buffer model.
			 */
			bpf_buffull(d);
			++d->bd_dcount;
			return;
		}
		ROTATE_BUFFERS(d);
		do_wakeup = 1;
		curlen = 0;
	} else if (d->bd_immediate || d->bd_state == BPF_TIMED_OUT)
		/*
		 * Immediate mode is set, or the read timeout has already
		 * expired during a select call.  A packet arrived, so the
		 * reader should be woken up.
		 */
		do_wakeup = 1;
	caplen = totlen - hdrlen;
	tstype = d->bd_tstamp;
	do_timestamp = tstype != BPF_T_NONE;
#ifndef BURN_BRIDGES
	if (tstype == BPF_T_NONE || BPF_T_FORMAT(tstype) == BPF_T_MICROTIME) {
		struct bpf_ts ts;
		if (do_timestamp)
			bpf_bintime2ts(bt, &ts, tstype);
#ifdef COMPAT_FREEBSD32
		if (d->bd_compat32) {
			bzero(&hdr32_old, sizeof(hdr32_old));
			if (do_timestamp) {
				hdr32_old.bh_tstamp.tv_sec = ts.bt_sec;
				hdr32_old.bh_tstamp.tv_usec = ts.bt_frac;
			}
			hdr32_old.bh_datalen = pktlen;
			hdr32_old.bh_hdrlen = hdrlen;
			hdr32_old.bh_caplen = caplen;
			bpf_append_bytes(d, d->bd_sbuf, curlen, &hdr32_old,
			    sizeof(hdr32_old));
			goto copy;
		}
#endif
		bzero(&hdr_old, sizeof(hdr_old));
		if (do_timestamp) {
			hdr_old.bh_tstamp.tv_sec = ts.bt_sec;
			hdr_old.bh_tstamp.tv_usec = ts.bt_frac;
		}
		hdr_old.bh_datalen = pktlen;
		hdr_old.bh_hdrlen = hdrlen;
		hdr_old.bh_caplen = caplen;
		bpf_append_bytes(d, d->bd_sbuf, curlen, &hdr_old,
		    sizeof(hdr_old));
		goto copy;
	}
#endif

	/*
	 * Append the bpf header.  Note we append the actual header size, but
	 * move forward the length of the header plus padding.
	 */
	bzero(&hdr, sizeof(hdr));
	if (do_timestamp)
		bpf_bintime2ts(bt, &hdr.bh_tstamp, tstype);
	hdr.bh_datalen = pktlen;
	hdr.bh_hdrlen = hdrlen;
	hdr.bh_caplen = caplen;
	bpf_append_bytes(d, d->bd_sbuf, curlen, &hdr, sizeof(hdr));

	/*
	 * Copy the packet data into the store buffer and update its length.
	 */
#ifndef BURN_BRIDGES
copy:
#endif
	(*cpfn)(d, d->bd_sbuf, curlen + hdrlen, pkt, caplen);
	d->bd_slen = curlen + totlen;

	if (do_wakeup)
		bpf_wakeup(d);
}

/*
 * Free buffers currently in use by a descriptor.
 * Called on close.
 */
static void
bpf_freed(struct bpf_d *d)
{

	/*
	 * We don't need to lock out interrupts since this descriptor has
	 * been detached from its interface and it yet hasn't been marked
	 * free.
	 */
	bpf_free(d);
	if (d->bd_rfilter != NULL) {
		free((caddr_t)d->bd_rfilter, M_BPF);
#ifdef BPF_JITTER
		if (d->bd_bfilter != NULL)
			bpf_destroy_jit_filter(d->bd_bfilter);
#endif
	}
	if (d->bd_wfilter != NULL)
		free((caddr_t)d->bd_wfilter, M_BPF);
	mtx_destroy(&d->bd_mtx);
}

/*
 * Attach an interface to bpf.  dlt is the link layer type; hdrlen is the
 * fixed size of the link header (variable length headers not yet supported).
 */
void
bpfattach(struct ifnet *ifp, u_int dlt, u_int hdrlen)
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
bpfattach2(struct ifnet *ifp, u_int dlt, u_int hdrlen, struct bpf_if **driverp)
{
	struct bpf_if *bp;

	bp = malloc(sizeof(*bp), M_BPF, M_NOWAIT | M_ZERO);
	if (bp == NULL)
		panic("bpfattach");

	LIST_INIT(&bp->bif_dlist);
	bp->bif_ifp = ifp;
	bp->bif_dlt = dlt;
	mtx_init(&bp->bif_mtx, "bpf interface lock", NULL, MTX_DEF);
	KASSERT(*driverp == NULL, ("bpfattach2: driverp already initialized"));
	*driverp = bp;

	mtx_lock(&bpf_mtx);
	LIST_INSERT_HEAD(&bpf_iflist, bp, bif_next);
	mtx_unlock(&bpf_mtx);

	bp->bif_hdrlen = hdrlen;

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
bpfdetach(struct ifnet *ifp)
{
	struct bpf_if	*bp;
	struct bpf_d	*d;

	/* Locate BPF interface information */
	mtx_lock(&bpf_mtx);
	LIST_FOREACH(bp, &bpf_iflist, bif_next) {
		if (ifp == bp->bif_ifp)
			break;
	}

	/* Interface wasn't attached */
	if ((bp == NULL) || (bp->bif_ifp == NULL)) {
		mtx_unlock(&bpf_mtx);
		printf("bpfdetach: %s was not attached\n", ifp->if_xname);
		return;
	}

	LIST_REMOVE(bp, bif_next);
	mtx_unlock(&bpf_mtx);

	while ((d = LIST_FIRST(&bp->bif_dlist)) != NULL) {
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
bpf_getdltlist(struct bpf_d *d, struct bpf_dltlist *bfl)
{
	int n, error;
	struct ifnet *ifp;
	struct bpf_if *bp;

	ifp = d->bd_bif->bif_ifp;
	n = 0;
	error = 0;
	mtx_lock(&bpf_mtx);
	LIST_FOREACH(bp, &bpf_iflist, bif_next) {
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
bpf_setdlt(struct bpf_d *d, u_int dlt)
{
	int error, opromisc;
	struct ifnet *ifp;
	struct bpf_if *bp;

	if (d->bd_bif->bif_dlt == dlt)
		return (0);
	ifp = d->bd_bif->bif_ifp;
	mtx_lock(&bpf_mtx);
	LIST_FOREACH(bp, &bpf_iflist, bif_next) {
		if (bp->bif_ifp == ifp && bp->bif_dlt == dlt)
			break;
	}
	mtx_unlock(&bpf_mtx);
	if (bp != NULL) {
		opromisc = d->bd_promisc;
		bpf_detachd(d);
		bpf_attachd(d, bp);
		BPFD_LOCK(d);
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

static void
bpf_drvinit(void *unused)
{
	struct cdev *dev;

	mtx_init(&bpf_mtx, "bpf global lock", NULL, MTX_DEF);
	LIST_INIT(&bpf_iflist);

	dev = make_dev(&bpf_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "bpf");
	/* For compatibility */
	make_dev_alias(dev, "bpf0");
}

/*
 * Zero out the various packet counters associated with all of the bpf
 * descriptors.  At some point, we will probably want to get a bit more
 * granular and allow the user to specify descriptors to be zeroed.
 */
static void
bpf_zero_counters(void)
{
	struct bpf_if *bp;
	struct bpf_d *bd;

	mtx_lock(&bpf_mtx);
	LIST_FOREACH(bp, &bpf_iflist, bif_next) {
		BPFIF_LOCK(bp);
		LIST_FOREACH(bd, &bp->bif_dlist, bd_next) {
			BPFD_LOCK(bd);
			bd->bd_rcount = 0;
			bd->bd_dcount = 0;
			bd->bd_fcount = 0;
			bd->bd_wcount = 0;
			bd->bd_wfcount = 0;
			bd->bd_zcopy = 0;
			BPFD_UNLOCK(bd);
		}
		BPFIF_UNLOCK(bp);
	}
	mtx_unlock(&bpf_mtx);
}

static void
bpfstats_fill_xbpf(struct xbpf_d *d, struct bpf_d *bd)
{

	bzero(d, sizeof(*d));
	BPFD_LOCK_ASSERT(bd);
	d->bd_structsize = sizeof(*d);
	d->bd_immediate = bd->bd_immediate;
	d->bd_promisc = bd->bd_promisc;
	d->bd_hdrcmplt = bd->bd_hdrcmplt;
	d->bd_direction = bd->bd_direction;
	d->bd_feedback = bd->bd_feedback;
	d->bd_async = bd->bd_async;
	d->bd_rcount = bd->bd_rcount;
	d->bd_dcount = bd->bd_dcount;
	d->bd_fcount = bd->bd_fcount;
	d->bd_sig = bd->bd_sig;
	d->bd_slen = bd->bd_slen;
	d->bd_hlen = bd->bd_hlen;
	d->bd_bufsize = bd->bd_bufsize;
	d->bd_pid = bd->bd_pid;
	strlcpy(d->bd_ifname,
	    bd->bd_bif->bif_ifp->if_xname, IFNAMSIZ);
	d->bd_locked = bd->bd_locked;
	d->bd_wcount = bd->bd_wcount;
	d->bd_wdcount = bd->bd_wdcount;
	d->bd_wfcount = bd->bd_wfcount;
	d->bd_zcopy = bd->bd_zcopy;
	d->bd_bufmode = bd->bd_bufmode;
}

static int
bpf_stats_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct xbpf_d *xbdbuf, *xbd, zerostats;
	int index, error;
	struct bpf_if *bp;
	struct bpf_d *bd;

	/*
	 * XXX This is not technically correct. It is possible for non
	 * privileged users to open bpf devices. It would make sense
	 * if the users who opened the devices were able to retrieve
	 * the statistics for them, too.
	 */
	error = priv_check(req->td, PRIV_NET_BPF);
	if (error)
		return (error);
	/*
	 * Check to see if the user is requesting that the counters be
	 * zeroed out.  Explicitly check that the supplied data is zeroed,
	 * as we aren't allowing the user to set the counters currently.
	 */
	if (req->newptr != NULL) {
		if (req->newlen != sizeof(zerostats))
			return (EINVAL);
		bzero(&zerostats, sizeof(zerostats));
		xbd = req->newptr;
		if (bcmp(xbd, &zerostats, sizeof(*xbd)) != 0)
			return (EINVAL);
		bpf_zero_counters();
		return (0);
	}
	if (req->oldptr == NULL)
		return (SYSCTL_OUT(req, 0, bpf_bpfd_cnt * sizeof(*xbd)));
	if (bpf_bpfd_cnt == 0)
		return (SYSCTL_OUT(req, 0, 0));
	xbdbuf = malloc(req->oldlen, M_BPF, M_WAITOK);
	mtx_lock(&bpf_mtx);
	if (req->oldlen < (bpf_bpfd_cnt * sizeof(*xbd))) {
		mtx_unlock(&bpf_mtx);
		free(xbdbuf, M_BPF);
		return (ENOMEM);
	}
	index = 0;
	LIST_FOREACH(bp, &bpf_iflist, bif_next) {
		BPFIF_LOCK(bp);
		LIST_FOREACH(bd, &bp->bif_dlist, bd_next) {
			xbd = &xbdbuf[index++];
			BPFD_LOCK(bd);
			bpfstats_fill_xbpf(xbd, bd);
			BPFD_UNLOCK(bd);
		}
		BPFIF_UNLOCK(bp);
	}
	mtx_unlock(&bpf_mtx);
	error = SYSCTL_OUT(req, xbdbuf, index * sizeof(*xbd));
	free(xbdbuf, M_BPF);
	return (error);
}

SYSINIT(bpfdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE,bpf_drvinit,NULL);

#else /* !DEV_BPF && !NETGRAPH_BPF */
/*
 * NOP stubs to allow bpf-using drivers to load and function.
 *
 * A 'better' implementation would allow the core bpf functionality
 * to be loaded at runtime.
 */
static struct bpf_if bp_null;

void
bpf_tap(struct bpf_if *bp, u_char *pkt, u_int pktlen)
{
}

void
bpf_mtap(struct bpf_if *bp, struct mbuf *m)
{
}

void
bpf_mtap2(struct bpf_if *bp, void *d, u_int l, struct mbuf *m)
{
}

void
bpfattach(struct ifnet *ifp, u_int dlt, u_int hdrlen)
{

	bpfattach2(ifp, dlt, hdrlen, &ifp->if_bpf);
}

void
bpfattach2(struct ifnet *ifp, u_int dlt, u_int hdrlen, struct bpf_if **driverp)
{

	*driverp = &bp_null;
}

void
bpfdetach(struct ifnet *ifp)
{
}

u_int
bpf_filter(const struct bpf_insn *pc, u_char *p, u_int wirelen, u_int buflen)
{
	return -1;	/* "no filter" behaviour */
}

int
bpf_validate(const struct bpf_insn *f, int len)
{
	return 0;		/* false */
}

#endif /* !DEV_BPF && !NETGRAPH_BPF */
