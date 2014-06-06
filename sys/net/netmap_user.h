/*
 * Copyright (C) 2011-2014 Universita` di Pisa. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $FreeBSD$
 *
 * Functions and macros to manipulate netmap structures and packets
 * in userspace. See netmap(4) for more information.
 *
 * The address of the struct netmap_if, say nifp, is computed from the
 * value returned from ioctl(.., NIOCREG, ...) and the mmap region:
 *	ioctl(fd, NIOCREG, &req);
 *	mem = mmap(0, ... );
 *	nifp = NETMAP_IF(mem, req.nr_nifp);
 *		(so simple, we could just do it manually)
 *
 * From there:
 *	struct netmap_ring *NETMAP_TXRING(nifp, index)
 *	struct netmap_ring *NETMAP_RXRING(nifp, index)
 *		we can access ring->nr_cur, ring->nr_avail, ring->nr_flags
 *
 *	ring->slot[i] gives us the i-th slot (we can access
 *		directly len, flags, buf_idx)
 *
 *	char *buf = NETMAP_BUF(ring, x) returns a pointer to
 *		the buffer numbered x
 *
 * All ring indexes (head, cur, tail) should always move forward.
 * To compute the next index in a circular ring you can use
 *	i = nm_ring_next(ring, i);
 *
 * To ease porting apps from pcap to netmap we supply a few fuctions
 * that can be called to open, close, read and write on netmap in a way
 * similar to libpcap. Note that the read/write function depend on
 * an ioctl()/select()/poll() being issued to refill rings or push
 * packets out.
 *
 * In order to use these, include #define NETMAP_WITH_LIBS
 * in the source file that invokes these functions.
 */

#ifndef _NET_NETMAP_USER_H_
#define _NET_NETMAP_USER_H_

#include <stdint.h>
#include <sys/socket.h>		/* apple needs sockaddr */
#include <net/if.h>		/* IFNAMSIZ */

#ifndef likely
#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)
#endif /* likely and unlikely */

#include <net/netmap.h>

/* helper macro */
#define _NETMAP_OFFSET(type, ptr, offset) \
	((type)(void *)((char *)(ptr) + (offset)))

#define NETMAP_IF(_base, _ofs)	_NETMAP_OFFSET(struct netmap_if *, _base, _ofs)

#define NETMAP_TXRING(nifp, index) _NETMAP_OFFSET(struct netmap_ring *, \
	nifp, (nifp)->ring_ofs[index] )

#define NETMAP_RXRING(nifp, index) _NETMAP_OFFSET(struct netmap_ring *,	\
	nifp, (nifp)->ring_ofs[index + (nifp)->ni_tx_rings + 1] )

#define NETMAP_BUF(ring, index)				\
	((char *)(ring) + (ring)->buf_ofs + ((index)*(ring)->nr_buf_size))

#define NETMAP_BUF_IDX(ring, buf)			\
	( ((char *)(buf) - ((char *)(ring) + (ring)->buf_ofs) ) / \
		(ring)->nr_buf_size )


static inline uint32_t
nm_ring_next(struct netmap_ring *r, uint32_t i)
{
	return ( unlikely(i + 1 == r->num_slots) ? 0 : i + 1);
}


/*
 * Return 1 if we have pending transmissions in the tx ring.
 * When everything is complete ring->head = ring->tail + 1 (modulo ring size)
 */
static inline int
nm_tx_pending(struct netmap_ring *r)
{
	return nm_ring_next(r, r->tail) != r->head;
}


static inline uint32_t
nm_ring_space(struct netmap_ring *ring)
{
        int ret = ring->tail - ring->cur;
        if (ret < 0)
                ret += ring->num_slots;
        return ret;
}


#ifdef NETMAP_WITH_LIBS
/*
 * Support for simple I/O libraries.
 * Include other system headers required for compiling this.
 */

#ifndef HAVE_NETMAP_WITH_LIBS
#define HAVE_NETMAP_WITH_LIBS

#include <stdio.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <string.h>	/* memset */
#include <sys/ioctl.h>
#include <sys/errno.h>	/* EINVAL */
#include <fcntl.h>	/* O_RDWR */
#include <unistd.h>	/* close() */
#include <signal.h>
#include <stdlib.h>

#ifndef ND /* debug macros */
/* debug support */
#define ND(_fmt, ...) do {} while(0)
#define D(_fmt, ...)						\
	do {							\
		struct timeval t0;				\
		gettimeofday(&t0, NULL);			\
		fprintf(stderr, "%03d.%06d %s [%d] " _fmt "\n",	\
		    (int)(t0.tv_sec % 1000), (int)t0.tv_usec,	\
		    __FUNCTION__, __LINE__, ##__VA_ARGS__);	\
        } while (0)

/* Rate limited version of "D", lps indicates how many per second */
#define RD(lps, format, ...)                                    \
    do {                                                        \
        static int t0, __cnt;                                   \
        struct timeval __xxts;                                  \
        gettimeofday(&__xxts, NULL);                            \
        if (t0 != __xxts.tv_sec) {                              \
            t0 = __xxts.tv_sec;                                 \
            __cnt = 0;                                          \
        }                                                       \
        if (__cnt++ < lps) {                                    \
            D(format, ##__VA_ARGS__);                           \
        }                                                       \
    } while (0)
#endif

struct nm_pkthdr {	/* same as pcap_pkthdr */
	struct timeval	ts;
	uint32_t	caplen;
	uint32_t	len;
};

struct nm_stat {	/* same as pcap_stat	*/
	u_int	ps_recv;
	u_int	ps_drop;
	u_int	ps_ifdrop;
#ifdef WIN32
	u_int	bs_capt;
#endif /* WIN32 */
};

#define NM_ERRBUF_SIZE	512

struct nm_desc {
	struct nm_desc *self; /* point to self if netmap. */
	int fd;
	void *mem;
	uint32_t memsize;
	int done_mmap;	/* set if mem is the result of mmap */
	struct netmap_if * const nifp;
	uint16_t first_tx_ring, last_tx_ring, cur_tx_ring;
	uint16_t first_rx_ring, last_rx_ring, cur_rx_ring;
	struct nmreq req;	/* also contains the nr_name = ifname */
	struct nm_pkthdr hdr;

	/*
	 * The memory contains netmap_if, rings and then buffers.
	 * Given a pointer (e.g. to nm_inject) we can compare with
	 * mem/buf_start/buf_end to tell if it is a buffer or
	 * some other descriptor in our region.
	 * We also store a pointer to some ring as it helps in the
	 * translation from buffer indexes to addresses.
	 */
	struct netmap_ring * const some_ring;
	void * const buf_start;
	void * const buf_end;
	/* parameters from pcap_open_live */
	int snaplen;
	int promisc;
	int to_ms;
	char *errbuf;

	/* save flags so we can restore them on close */
	uint32_t if_flags;
        uint32_t if_reqcap;
        uint32_t if_curcap;

	struct nm_stat st;
	char msg[NM_ERRBUF_SIZE];
};

/*
 * when the descriptor is open correctly, d->self == d
 * Eventually we should also use some magic number.
 */
#define P2NMD(p)		((struct nm_desc *)(p))
#define IS_NETMAP_DESC(d)	((d) && P2NMD(d)->self == P2NMD(d))
#define NETMAP_FD(d)		(P2NMD(d)->fd)


/*
 * this is a slightly optimized copy routine which rounds
 * to multiple of 64 bytes and is often faster than dealing
 * with other odd sizes. We assume there is enough room
 * in the source and destination buffers.
 *
 * XXX only for multiples of 64 bytes, non overlapped.
 */
static inline void
nm_pkt_copy(const void *_src, void *_dst, int l)
{
	const uint64_t *src = (const uint64_t *)_src;
	uint64_t *dst = (uint64_t *)_dst;

	if (unlikely(l >= 1024)) {
		memcpy(dst, src, l);
		return;
	}
	for (; likely(l > 0); l-=64) {
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
	}
}


/*
 * The callback, invoked on each received packet. Same as libpcap
 */
typedef void (*nm_cb_t)(u_char *, const struct nm_pkthdr *, const u_char *d);

/*
 *--- the pcap-like API ---
 *
 * nm_open() opens a file descriptor, binds to a port and maps memory.
 *
 * ifname	(netmap:foo or vale:foo) is the port name
 *		a suffix can indicate the follwing:
 *		^		bind the host (sw) ring pair
 *		*		bind host and NIC ring pairs (transparent)
 *		-NN		bind individual NIC ring pair
 *		{NN		bind master side of pipe NN
 *		}NN		bind slave side of pipe NN
 *
 * req		provides the initial values of nmreq before parsing ifname.
 *		Remember that the ifname parsing will override the ring
 *		number in nm_ringid, and part of nm_flags;
 * flags	special functions, normally 0
 *		indicates which fields of *arg are significant
 * arg		special functions, normally NULL
 *		if passed a netmap_desc with mem != NULL,
 *		use that memory instead of mmap.
 */

static struct nm_desc *nm_open(const char *ifname, const struct nmreq *req,
	uint64_t flags, const struct nm_desc *arg);

/*
 * nm_open can import some fields from the parent descriptor.
 * These flags control which ones.
 * Also in flags you can specify NETMAP_NO_TX_POLL and NETMAP_DO_RX_POLL,
 * which set the initial value for these flags.
 * Note that the 16 low bits of the flags are reserved for data
 * that may go into the nmreq.
 */
enum {
	NM_OPEN_NO_MMAP =	0x040000, /* reuse mmap from parent */
	NM_OPEN_IFNAME =	0x080000, /* nr_name, nr_ringid, nr_flags */
	NM_OPEN_ARG1 =		0x100000,
	NM_OPEN_ARG2 =		0x200000,
	NM_OPEN_ARG3 =		0x400000,
	NM_OPEN_RING_CFG =	0x800000, /* tx|rx rings|slots */
};


/*
 * nm_close()	closes and restores the port to its previous state
 */

static int nm_close(struct nm_desc *);

/*
 * nm_inject() is the same as pcap_inject()
 * nm_dispatch() is the same as pcap_dispatch()
 * nm_nextpkt() is the same as pcap_next()
 */

static int nm_inject(struct nm_desc *, const void *, size_t);
static int nm_dispatch(struct nm_desc *, int, nm_cb_t, u_char *);
static u_char *nm_nextpkt(struct nm_desc *, struct nm_pkthdr *);


/*
 * Try to open, return descriptor if successful, NULL otherwise.
 * An invalid netmap name will return errno = 0;
 * You can pass a pointer to a pre-filled nm_desc to add special
 * parameters. Flags is used as follows
 * NM_OPEN_NO_MMAP	use the memory from arg, only
 *			if the nr_arg2 (memory block) matches.
 * NM_OPEN_ARG1		use req.nr_arg1 from arg
 * NM_OPEN_ARG2		use req.nr_arg2 from arg
 * NM_OPEN_RING_CFG	user ring config from arg
 */
static struct nm_desc *
nm_open(const char *ifname, const struct nmreq *req,
	uint64_t new_flags, const struct nm_desc *arg)
{
	struct nm_desc *d = NULL;
	const struct nm_desc *parent = arg;
	u_int namelen;
	uint32_t nr_ringid = 0, nr_flags;
	const char *port = NULL;
	const char *errmsg = NULL;

	if (strncmp(ifname, "netmap:", 7) && strncmp(ifname, "vale", 4)) {
		errno = 0; /* name not recognised, not an error */
		return NULL;
	}
	if (ifname[0] == 'n')
		ifname += 7;
	/* scan for a separator */
	for (port = ifname; *port && !index("-*^{}", *port); port++)
		;
	namelen = port - ifname;
	if (namelen >= sizeof(d->req.nr_name)) {
		errmsg = "name too long";
		goto fail;
	}
	switch (*port) {
	default:  /* '\0', no suffix */
		nr_flags = NR_REG_ALL_NIC;
		break;
	case '-': /* one NIC */
		nr_flags = NR_REG_ONE_NIC;
		nr_ringid = atoi(port + 1);
		break;
	case '*': /* NIC and SW, ignore port */
		nr_flags = NR_REG_NIC_SW;
		if (port[1]) {
			errmsg = "invalid port for nic+sw";
			goto fail;
		}
		break;
	case '^': /* only sw ring */
		nr_flags = NR_REG_SW;
		if (port[1]) {
			errmsg = "invalid port for sw ring";
			goto fail;
		}
		break;
	case '{':
		nr_flags = NR_REG_PIPE_MASTER;
		nr_ringid = atoi(port + 1);
		break;
	case '}':
		nr_flags = NR_REG_PIPE_SLAVE;
		nr_ringid = atoi(port + 1);
		break;
	}

	if (nr_ringid >= NETMAP_RING_MASK) {
		errmsg = "invalid ringid";
		goto fail;
	}

	d = (struct nm_desc *)calloc(1, sizeof(*d));
	if (d == NULL) {
		errmsg = "nm_desc alloc failure";
		errno = ENOMEM;
		return NULL;
	}
	d->self = d;	/* set this early so nm_close() works */
	d->fd = open("/dev/netmap", O_RDWR);
	if (d->fd < 0) {
		errmsg = "cannot open /dev/netmap";
		goto fail;
	}

	if (req)
		d->req = *req;
	d->req.nr_version = NETMAP_API;
	d->req.nr_ringid &= ~NETMAP_RING_MASK;

	/* these fields are overridden by ifname and flags processing */
	d->req.nr_ringid |= nr_ringid;
	d->req.nr_flags = nr_flags;
	memcpy(d->req.nr_name, ifname, namelen);
	d->req.nr_name[namelen] = '\0';
	/* optionally import info from parent */
	if (IS_NETMAP_DESC(parent) && new_flags) {
		if (new_flags & NM_OPEN_ARG1)
			D("overriding ARG1 %d", parent->req.nr_arg1);
		d->req.nr_arg1 = new_flags & NM_OPEN_ARG1 ?
			parent->req.nr_arg1 : 4;
		if (new_flags & NM_OPEN_ARG2)
			D("overriding ARG2 %d", parent->req.nr_arg2);
		d->req.nr_arg2 = new_flags & NM_OPEN_ARG2 ?
			parent->req.nr_arg2 : 0;
		if (new_flags & NM_OPEN_ARG3)
			D("overriding ARG3 %d", parent->req.nr_arg3);
		d->req.nr_arg3 = new_flags & NM_OPEN_ARG3 ?
			parent->req.nr_arg3 : 0;
		if (new_flags & NM_OPEN_RING_CFG) {
			D("overriding RING_CFG");
			d->req.nr_tx_slots = parent->req.nr_tx_slots;
			d->req.nr_rx_slots = parent->req.nr_rx_slots;
			d->req.nr_tx_rings = parent->req.nr_tx_rings;
			d->req.nr_rx_rings = parent->req.nr_rx_rings;
		}
		if (new_flags & NM_OPEN_IFNAME) {
			D("overriding ifname %s ringid 0x%x flags 0x%x",
				parent->req.nr_name, parent->req.nr_ringid,
				parent->req.nr_flags);
			memcpy(d->req.nr_name, parent->req.nr_name,
				sizeof(d->req.nr_name));
			d->req.nr_ringid = parent->req.nr_ringid;
			d->req.nr_flags = parent->req.nr_flags;
		}
	}
	/* add the *XPOLL flags */
	d->req.nr_ringid |= new_flags & (NETMAP_NO_TX_POLL | NETMAP_DO_RX_POLL);

	if (ioctl(d->fd, NIOCREGIF, &d->req)) {
		errmsg = "NIOCREGIF failed";
		goto fail;
	}

	if (IS_NETMAP_DESC(parent) && parent->mem &&
	    parent->req.nr_arg2 == d->req.nr_arg2) {
		/* do not mmap, inherit from parent */
		d->memsize = parent->memsize;
		d->mem = parent->mem;
	} else {
		/* XXX TODO: check if memsize is too large (or there is overflow) */
		d->memsize = d->req.nr_memsize;
		d->mem = mmap(0, d->memsize, PROT_WRITE | PROT_READ, MAP_SHARED,
				d->fd, 0);
		if (d->mem == MAP_FAILED) {
			errmsg = "mmap failed";
			goto fail;
		}
		d->done_mmap = 1;
	}
	{
		struct netmap_if *nifp = NETMAP_IF(d->mem, d->req.nr_offset);
		struct netmap_ring *r = NETMAP_RXRING(nifp, );

		*(struct netmap_if **)(uintptr_t)&(d->nifp) = nifp;
		*(struct netmap_ring **)(uintptr_t)&d->some_ring = r;
		*(void **)(uintptr_t)&d->buf_start = NETMAP_BUF(r, 0);
		*(void **)(uintptr_t)&d->buf_end =
			(char *)d->mem + d->memsize;
	}

	if (nr_flags ==  NR_REG_SW) { /* host stack */
		d->first_tx_ring = d->last_tx_ring = d->req.nr_tx_rings;
		d->first_rx_ring = d->last_rx_ring = d->req.nr_rx_rings;
	} else if (nr_flags ==  NR_REG_ALL_NIC) { /* only nic */
		d->first_tx_ring = 0;
		d->first_rx_ring = 0;
		d->last_tx_ring = d->req.nr_tx_rings - 1;
		d->last_rx_ring = d->req.nr_rx_rings - 1;
	} else if (nr_flags ==  NR_REG_NIC_SW) {
		d->first_tx_ring = 0;
		d->first_rx_ring = 0;
		d->last_tx_ring = d->req.nr_tx_rings;
		d->last_rx_ring = d->req.nr_rx_rings;
	} else if (nr_flags == NR_REG_ONE_NIC) {
		/* XXX check validity */
		d->first_tx_ring = d->last_tx_ring =
		d->first_rx_ring = d->last_rx_ring = nr_ringid;
	} else { /* pipes */
		d->first_tx_ring = d->last_tx_ring = 0;
		d->first_rx_ring = d->last_rx_ring = 0;
	}

#ifdef DEBUG_NETMAP_USER
    { /* debugging code */
	int i;

	D("%s tx %d .. %d %d rx %d .. %d %d", ifname,
		d->first_tx_ring, d->last_tx_ring, d->req.nr_tx_rings,
                d->first_rx_ring, d->last_rx_ring, d->req.nr_rx_rings);
	for (i = 0; i <= d->req.nr_tx_rings; i++) {
		struct netmap_ring *r = NETMAP_TXRING(d->nifp, i);
		D("TX%d %p h %d c %d t %d", i, r, r->head, r->cur, r->tail);
	}
	for (i = 0; i <= d->req.nr_rx_rings; i++) {
		struct netmap_ring *r = NETMAP_RXRING(d->nifp, i);
		D("RX%d %p h %d c %d t %d", i, r, r->head, r->cur, r->tail);
	}
    }
#endif /* debugging */

	d->cur_tx_ring = d->first_tx_ring;
	d->cur_rx_ring = d->first_rx_ring;
	return d;

fail:
	nm_close(d);
	if (errmsg)
		D("%s %s", errmsg, ifname);
	errno = EINVAL;
	return NULL;
}


static int
nm_close(struct nm_desc *d)
{
	/*
	 * ugly trick to avoid unused warnings
	 */
	static void *__xxzt[] __attribute__ ((unused))  =
		{ (void *)nm_open, (void *)nm_inject,
		  (void *)nm_dispatch, (void *)nm_nextpkt } ;

	if (d == NULL || d->self != d)
		return EINVAL;
	if (d->done_mmap && d->mem)
		munmap(d->mem, d->memsize);
	if (d->fd != -1)
		close(d->fd);
	bzero(d, sizeof(*d));
	free(d);
	return 0;
}


/*
 * Same prototype as pcap_inject(), only need to cast.
 */
static int
nm_inject(struct nm_desc *d, const void *buf, size_t size)
{
	u_int c, n = d->last_tx_ring - d->first_tx_ring + 1;

	for (c = 0; c < n ; c++) {
		/* compute current ring to use */
		struct netmap_ring *ring;
		uint32_t i, idx;
		uint32_t ri = d->cur_tx_ring + c;

		if (ri > d->last_tx_ring)
			ri = d->first_tx_ring;
		ring = NETMAP_TXRING(d->nifp, ri);
		if (nm_ring_empty(ring)) {
			continue;
		}
		i = ring->cur;
		idx = ring->slot[i].buf_idx;
		ring->slot[i].len = size;
		nm_pkt_copy(buf, NETMAP_BUF(ring, idx), size);
		d->cur_tx_ring = ri;
		ring->head = ring->cur = nm_ring_next(ring, i);
		return size;
	}
	return 0; /* fail */
}


/*
 * Same prototype as pcap_dispatch(), only need to cast.
 */
static int
nm_dispatch(struct nm_desc *d, int cnt, nm_cb_t cb, u_char *arg)
{
	int n = d->last_rx_ring - d->first_rx_ring + 1;
	int c, got = 0, ri = d->cur_rx_ring;

	if (cnt == 0)
		cnt = -1;
	/* cnt == -1 means infinite, but rings have a finite amount
	 * of buffers and the int is large enough that we never wrap,
	 * so we can omit checking for -1
	 */
	for (c=0; c < n && cnt != got; c++) {
		/* compute current ring to use */
		struct netmap_ring *ring;

		ri = d->cur_rx_ring + c;
		if (ri > d->last_rx_ring)
			ri = d->first_rx_ring;
		ring = NETMAP_RXRING(d->nifp, ri);
		for ( ; !nm_ring_empty(ring) && cnt != got; got++) {
			u_int i = ring->cur;
			u_int idx = ring->slot[i].buf_idx;
			u_char *buf = (u_char *)NETMAP_BUF(ring, idx);

			// __builtin_prefetch(buf);
			d->hdr.len = d->hdr.caplen = ring->slot[i].len;
			d->hdr.ts = ring->ts;
			cb(arg, &d->hdr, buf);
			ring->head = ring->cur = nm_ring_next(ring, i);
		}
	}
	d->cur_rx_ring = ri;
	return got;
}

static u_char *
nm_nextpkt(struct nm_desc *d, struct nm_pkthdr *hdr)
{
	int ri = d->cur_rx_ring;

	do {
		/* compute current ring to use */
		struct netmap_ring *ring = NETMAP_RXRING(d->nifp, ri);
		if (!nm_ring_empty(ring)) {
			u_int i = ring->cur;
			u_int idx = ring->slot[i].buf_idx;
			u_char *buf = (u_char *)NETMAP_BUF(ring, idx);

			// __builtin_prefetch(buf);
			hdr->ts = ring->ts;
			hdr->len = hdr->caplen = ring->slot[i].len;
			ring->cur = nm_ring_next(ring, i);
			/* we could postpone advancing head if we want
			 * to hold the buffer. This can be supported in
			 * the future.
			 */
			ring->head = ring->cur;
			d->cur_rx_ring = ri;
			return buf;
		}
		ri++;
		if (ri > d->last_rx_ring)
			ri = d->first_rx_ring;
	} while (ri != d->cur_rx_ring);
	return NULL; /* nothing found */
}

#endif /* !HAVE_NETMAP_WITH_LIBS */

#endif /* NETMAP_WITH_LIBS */

#endif /* _NET_NETMAP_USER_H_ */
