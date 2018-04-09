/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (C) 2011-2016 Universita` di Pisa
 * All rights reserved.
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
 *		we can access ring->cur, ring->head, ring->tail, etc.
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

#define NETMAP_DEVICE_NAME "/dev/netmap"

#ifdef __CYGWIN__
/*
 * we can compile userspace apps with either cygwin or msvc,
 * and we use _WIN32 to identify windows specific code
 */
#ifndef _WIN32
#define _WIN32
#endif	/* _WIN32 */

#endif	/* __CYGWIN__ */

#ifdef _WIN32
#undef NETMAP_DEVICE_NAME
#define NETMAP_DEVICE_NAME "/proc/sys/DosDevices/Global/netmap"
#include <windows.h>
#include <WinDef.h>
#include <sys/cygwin.h>
#endif /* _WIN32 */

#include <stdint.h>
#include <sys/socket.h>		/* apple needs sockaddr */
#include <net/if.h>		/* IFNAMSIZ */
#include <ctype.h>

#ifndef likely
#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)
#endif /* likely and unlikely */

#include <net/netmap.h>
#include <net/netmap_virt.h> /* nmreq_pointer_get() */

/* helper macro */
#define _NETMAP_OFFSET(type, ptr, offset) \
	((type)(void *)((char *)(ptr) + (offset)))

#define NETMAP_IF(_base, _ofs)	_NETMAP_OFFSET(struct netmap_if *, _base, _ofs)

#define NETMAP_TXRING(nifp, index) _NETMAP_OFFSET(struct netmap_ring *, \
	nifp, (nifp)->ring_ofs[index] )

#define NETMAP_RXRING(nifp, index) _NETMAP_OFFSET(struct netmap_ring *,	\
	nifp, (nifp)->ring_ofs[index + (nifp)->ni_tx_rings + 1] )

#define NETMAP_BUF(ring, index)				\
	((char *)(ring) + (ring)->buf_ofs + ((long)(index)*(ring)->nr_buf_size))

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
		struct timeval _t0;				\
		gettimeofday(&_t0, NULL);			\
		fprintf(stderr, "%03d.%06d %s [%d] " _fmt "\n",	\
		    (int)(_t0.tv_sec % 1000), (int)_t0.tv_usec,	\
		    __FUNCTION__, __LINE__, ##__VA_ARGS__);	\
        } while (0)

/* Rate limited version of "D", lps indicates how many per second */
#define RD(lps, format, ...)                                    \
    do {                                                        \
        static int __t0, __cnt;                                 \
        struct timeval __xxts;                                  \
        gettimeofday(&__xxts, NULL);                            \
        if (__t0 != __xxts.tv_sec) {                            \
            __t0 = __xxts.tv_sec;                               \
            __cnt = 0;                                          \
        }                                                       \
        if (__cnt++ < lps) {                                    \
            D(format, ##__VA_ARGS__);                           \
        }                                                       \
    } while (0)
#endif

struct nm_pkthdr {	/* first part is the same as pcap_pkthdr */
	struct timeval	ts;
	uint32_t	caplen;
	uint32_t	len;

	uint64_t flags;	/* NM_MORE_PKTS etc */
#define NM_MORE_PKTS	1
	struct nm_desc *d;
	struct netmap_slot *slot;
	uint8_t *buf;
};

struct nm_stat {	/* same as pcap_stat	*/
	u_int	ps_recv;
	u_int	ps_drop;
	u_int	ps_ifdrop;
#ifdef WIN32 /* XXX or _WIN32 ? */
	u_int	bs_capt;
#endif /* WIN32 */
};

#define NM_ERRBUF_SIZE	512

struct nm_desc {
	struct nm_desc *self; /* point to self if netmap. */
	int fd;
	void *mem;
	uint64_t memsize;
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
 */
static inline void
nm_pkt_copy(const void *_src, void *_dst, int l)
{
	const uint64_t *src = (const uint64_t *)_src;
	uint64_t *dst = (uint64_t *)_dst;

	if (unlikely(l >= 1024 || l % 64)) {
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
 *		*		bind host and NIC ring pairs
 *		-NN		bind individual NIC ring pair
 *		{NN		bind master side of pipe NN
 *		}NN		bind slave side of pipe NN
 *		a suffix starting with / and the following flags,
 *		in any order:
 *		x		exclusive access
 *		z		zero copy monitor (both tx and rx)
 *		t		monitor tx side (copy monitor)
 *		r		monitor rx side (copy monitor)
 *		R		bind only RX ring(s)
 *		T		bind only TX ring(s)
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
	NM_OPEN_EXTMEM =       0x1000000,
};


/*
 * nm_close()	closes and restores the port to its previous state
 */

static int nm_close(struct nm_desc *);

/*
 * nm_mmap()    do mmap or inherit from parent if the nr_arg2
 *              (memory block) matches.
 */

static int nm_mmap(struct nm_desc *, const struct nm_desc *);

/*
 * nm_inject() is the same as pcap_inject()
 * nm_dispatch() is the same as pcap_dispatch()
 * nm_nextpkt() is the same as pcap_next()
 */

static int nm_inject(struct nm_desc *, const void *, size_t);
static int nm_dispatch(struct nm_desc *, int, nm_cb_t, u_char *);
static u_char *nm_nextpkt(struct nm_desc *, struct nm_pkthdr *);

#ifdef _WIN32

intptr_t _get_osfhandle(int); /* defined in io.h in windows */

/*
 * In windows we do not have yet native poll support, so we keep track
 * of file descriptors associated to netmap ports to emulate poll on
 * them and fall back on regular poll on other file descriptors.
 */
struct win_netmap_fd_list {
	struct win_netmap_fd_list *next;
	int win_netmap_fd;
	HANDLE win_netmap_handle;
};

/*
 * list head containing all the netmap opened fd and their
 * windows HANDLE counterparts
 */
static struct win_netmap_fd_list *win_netmap_fd_list_head;

static void
win_insert_fd_record(int fd)
{
	struct win_netmap_fd_list *curr;

	for (curr = win_netmap_fd_list_head; curr; curr = curr->next) {
		if (fd == curr->win_netmap_fd) {
			return;
		}
	}
	curr = calloc(1, sizeof(*curr));
	curr->next = win_netmap_fd_list_head;
	curr->win_netmap_fd = fd;
	curr->win_netmap_handle = IntToPtr(_get_osfhandle(fd));
	win_netmap_fd_list_head = curr;
}

void
win_remove_fd_record(int fd)
{
	struct win_netmap_fd_list *curr = win_netmap_fd_list_head;
	struct win_netmap_fd_list *prev = NULL;
	for (; curr ; prev = curr, curr = curr->next) {
		if (fd != curr->win_netmap_fd)
			continue;
		/* found the entry */
		if (prev == NULL) { /* we are freeing the first entry */
			win_netmap_fd_list_head = curr->next;
		} else {
			prev->next = curr->next;
		}
		free(curr);
		break;
	}
}


HANDLE
win_get_netmap_handle(int fd)
{
	struct win_netmap_fd_list *curr;

	for (curr = win_netmap_fd_list_head; curr; curr = curr->next) {
		if (fd == curr->win_netmap_fd) {
			return curr->win_netmap_handle;
		}
	}
	return NULL;
}

/*
 * we need to wrap ioctl and mmap, at least for the netmap file descriptors
 */

/*
 * use this function only from netmap_user.h internal functions
 * same as ioctl, returns 0 on success and -1 on error
 */
static int
win_nm_ioctl_internal(HANDLE h, int32_t ctlCode, void *arg)
{
	DWORD bReturn = 0, szIn, szOut;
	BOOL ioctlReturnStatus;
	void *inParam = arg, *outParam = arg;

	switch (ctlCode) {
	case NETMAP_POLL:
		szIn = sizeof(POLL_REQUEST_DATA);
		szOut = sizeof(POLL_REQUEST_DATA);
		break;
	case NETMAP_MMAP:
		szIn = 0;
		szOut = sizeof(void*);
		inParam = NULL; /* nothing on input */
		break;
	case NIOCTXSYNC:
	case NIOCRXSYNC:
		szIn = 0;
		szOut = 0;
		break;
	case NIOCREGIF:
		szIn = sizeof(struct nmreq);
		szOut = sizeof(struct nmreq);
		break;
	case NIOCCONFIG:
		D("unsupported NIOCCONFIG!");
		return -1;

	default: /* a regular ioctl */
		D("invalid ioctl %x on netmap fd", ctlCode);
		return -1;
	}

	ioctlReturnStatus = DeviceIoControl(h,
				ctlCode, inParam, szIn,
				outParam, szOut,
				&bReturn, NULL);
	// XXX note windows returns 0 on error or async call, 1 on success
	// we could call GetLastError() to figure out what happened
	return ioctlReturnStatus ? 0 : -1;
}

/*
 * this function is what must be called from user-space programs
 * same as ioctl, returns 0 on success and -1 on error
 */
static int
win_nm_ioctl(int fd, int32_t ctlCode, void *arg)
{
	HANDLE h = win_get_netmap_handle(fd);

	if (h == NULL) {
		return ioctl(fd, ctlCode, arg);
	} else {
		return win_nm_ioctl_internal(h, ctlCode, arg);
	}
}

#define ioctl win_nm_ioctl /* from now on, within this file ... */

/*
 * We cannot use the native mmap on windows
 * The only parameter used is "fd", the other ones are just declared to
 * make this signature comparable to the FreeBSD/Linux one
 */
static void *
win32_mmap_emulated(void *addr, size_t length, int prot, int flags, int fd, int32_t offset)
{
	HANDLE h = win_get_netmap_handle(fd);

	if (h == NULL) {
		return mmap(addr, length, prot, flags, fd, offset);
	} else {
		MEMORY_ENTRY ret;

		return win_nm_ioctl_internal(h, NETMAP_MMAP, &ret) ?
			NULL : ret.pUsermodeVirtualAddress;
	}
}

#define mmap win32_mmap_emulated

#include <sys/poll.h> /* XXX needed to use the structure pollfd */

static int
win_nm_poll(struct pollfd *fds, int nfds, int timeout)
{
	HANDLE h;

	if (nfds != 1 || fds == NULL || (h = win_get_netmap_handle(fds->fd)) == NULL) {;
		return poll(fds, nfds, timeout);
	} else {
		POLL_REQUEST_DATA prd;

		prd.timeout = timeout;
		prd.events = fds->events;

		win_nm_ioctl_internal(h, NETMAP_POLL, &prd);
		if ((prd.revents == POLLERR) || (prd.revents == STATUS_TIMEOUT)) {
			return -1;
		}
		return 1;
	}
}

#define poll win_nm_poll

static int
win_nm_open(char* pathname, int flags)
{

	if (strcmp(pathname, NETMAP_DEVICE_NAME) == 0) {
		int fd = open(NETMAP_DEVICE_NAME, O_RDWR);
		if (fd < 0) {
			return -1;
		}

		win_insert_fd_record(fd);
		return fd;
	} else {
		return open(pathname, flags);
	}
}

#define open win_nm_open

static int
win_nm_close(int fd)
{
	if (fd != -1) {
		close(fd);
		if (win_get_netmap_handle(fd) != NULL) {
			win_remove_fd_record(fd);
		}
	}
	return 0;
}

#define close win_nm_close

#endif /* _WIN32 */

static int
nm_is_identifier(const char *s, const char *e)
{
	for (; s != e; s++) {
		if (!isalnum(*s) && *s != '_') {
			return 0;
		}
	}

	return 1;
}

static void
nm_init_offsets(struct nm_desc *d)
{
	struct netmap_if *nifp = NETMAP_IF(d->mem, d->req.nr_offset);
	struct netmap_ring *r = NETMAP_RXRING(nifp, d->first_rx_ring);
	if ((void *)r == (void *)nifp) {
		/* the descriptor is open for TX only */
		r = NETMAP_TXRING(nifp, d->first_tx_ring);
	}

	*(struct netmap_if **)(uintptr_t)&(d->nifp) = nifp;
	*(struct netmap_ring **)(uintptr_t)&d->some_ring = r;
	*(void **)(uintptr_t)&d->buf_start = NETMAP_BUF(r, 0);
	*(void **)(uintptr_t)&d->buf_end =
		(char *)d->mem + d->memsize;
}

#define MAXERRMSG 80
#define NM_PARSE_OK	  	0
#define NM_PARSE_MEMID 		1
static int
nm_parse_one(const char *ifname, struct nmreq *d, char **out, int memid_allowed)
{
	int is_vale;
	const char *port = NULL;
	const char *vpname = NULL;
	u_int namelen;
	uint32_t nr_ringid = 0, nr_flags;
	char errmsg[MAXERRMSG] = "";
	long num;
	uint16_t nr_arg2 = 0;
	enum { P_START, P_RNGSFXOK, P_GETNUM, P_FLAGS, P_FLAGSOK, P_MEMID } p_state;

	errno = 0;

	if (strncmp(ifname, "netmap:", 7) &&
			strncmp(ifname, NM_BDG_NAME, strlen(NM_BDG_NAME))) {
		snprintf(errmsg, MAXERRMSG, "invalid port name: %s", ifname);
		errno = EINVAL;
		goto fail;
	}

	is_vale = (ifname[0] == 'v');
	if (is_vale) {
		port = index(ifname, ':');
		if (port == NULL) {
			snprintf(errmsg, MAXERRMSG,
				 "missing ':' in vale name");
			goto fail;
		}

		if (!nm_is_identifier(ifname + 4, port)) {
			snprintf(errmsg, MAXERRMSG, "invalid bridge name");
			goto fail;
		}

		vpname = ++port;
	} else {
		ifname += 7;
		port = ifname;
	}

	/* scan for a separator */
	for (; *port && !index("-*^{}/@", *port); port++)
		;

	if (is_vale && !nm_is_identifier(vpname, port)) {
		snprintf(errmsg, MAXERRMSG, "invalid bridge port name");
		goto fail;
	}

	namelen = port - ifname;
	if (namelen >= sizeof(d->nr_name)) {
		snprintf(errmsg, MAXERRMSG, "name too long");
		goto fail;
	}
	memcpy(d->nr_name, ifname, namelen);
	d->nr_name[namelen] = '\0';
	D("name %s", d->nr_name);

	p_state = P_START;
	nr_flags = NR_REG_ALL_NIC; /* default for no suffix */
	while (*port) {
		switch (p_state) {
		case P_START:
			switch (*port) {
			case '^': /* only SW ring */
				nr_flags = NR_REG_SW;
				p_state = P_RNGSFXOK;
				break;
			case '*': /* NIC and SW */
				nr_flags = NR_REG_NIC_SW;
				p_state = P_RNGSFXOK;
				break;
			case '-': /* one NIC ring pair */
				nr_flags = NR_REG_ONE_NIC;
				p_state = P_GETNUM;
				break;
			case '{': /* pipe (master endpoint) */
				nr_flags = NR_REG_PIPE_MASTER;
				p_state = P_GETNUM;
				break;
			case '}': /* pipe (slave endoint) */
				nr_flags = NR_REG_PIPE_SLAVE;
				p_state = P_GETNUM;
				break;
			case '/': /* start of flags */
				p_state = P_FLAGS;
				break;
			case '@': /* start of memid */
				p_state = P_MEMID;
				break;
			default:
				snprintf(errmsg, MAXERRMSG, "unknown modifier: '%c'", *port);
				goto fail;
			}
			port++;
			break;
		case P_RNGSFXOK:
			switch (*port) {
			case '/':
				p_state = P_FLAGS;
				break;
			case '@':
				p_state = P_MEMID;
				break;
			default:
				snprintf(errmsg, MAXERRMSG, "unexpected character: '%c'", *port);
				goto fail;
			}
			port++;
			break;
		case P_GETNUM:
			num = strtol(port, (char **)&port, 10);
			if (num < 0 || num >= NETMAP_RING_MASK) {
				snprintf(errmsg, MAXERRMSG, "'%ld' out of range [0, %d)",
						num, NETMAP_RING_MASK);
				goto fail;
			}
			nr_ringid = num & NETMAP_RING_MASK;
			p_state = P_RNGSFXOK;
			break;
		case P_FLAGS:
		case P_FLAGSOK:
			if (*port == '@') {
				port++;
				p_state = P_MEMID;
				break;
			}
			switch (*port) {
			case 'x':
				nr_flags |= NR_EXCLUSIVE;
				break;
			case 'z':
				nr_flags |= NR_ZCOPY_MON;
				break;
			case 't':
				nr_flags |= NR_MONITOR_TX;
				break;
			case 'r':
				nr_flags |= NR_MONITOR_RX;
				break;
			case 'R':
				nr_flags |= NR_RX_RINGS_ONLY;
				break;
			case 'T':
				nr_flags |= NR_TX_RINGS_ONLY;
				break;
			default:
				snprintf(errmsg, MAXERRMSG, "unrecognized flag: '%c'", *port);
				goto fail;
			}
			port++;
			p_state = P_FLAGSOK;
			break;
		case P_MEMID:
			if (!memid_allowed) {
				snprintf(errmsg, MAXERRMSG, "double setting of memid");
				goto fail;
			}
			num = strtol(port, (char **)&port, 10);
			if (num <= 0) {
				ND("non-numeric memid %s (out = %p)", port, out);
				if (out == NULL)
					goto fail;
				*out = (char *)port;
				while (*port)
					port++;
			} else {
				nr_arg2 = num;
				memid_allowed = 0;
				p_state = P_RNGSFXOK;
			}
			break;
		}
	}
	if (p_state != P_START && p_state != P_RNGSFXOK &&
	    p_state != P_FLAGSOK && p_state != P_MEMID) {
		snprintf(errmsg, MAXERRMSG, "unexpected end of port name");
		goto fail;
	}
	ND("flags: %s %s %s %s",
			(nr_flags & NR_EXCLUSIVE) ? "EXCLUSIVE" : "",
			(nr_flags & NR_ZCOPY_MON) ? "ZCOPY_MON" : "",
			(nr_flags & NR_MONITOR_TX) ? "MONITOR_TX" : "",
			(nr_flags & NR_MONITOR_RX) ? "MONITOR_RX" : "");

	d->nr_flags |= nr_flags;
	d->nr_ringid |= nr_ringid;
	d->nr_arg2 = nr_arg2;

	return (p_state == P_MEMID) ? NM_PARSE_MEMID : NM_PARSE_OK;
fail:
	if (!errno)
		errno = EINVAL;
	if (out)
		*out = strdup(errmsg);
	return -1;
}

static int
nm_interp_memid(const char *memid, struct nmreq *req, char **err)
{
	int fd = -1;
	char errmsg[MAXERRMSG] = "";
	struct nmreq greq;
	off_t mapsize;
	struct netmap_pools_info *pi;

	/* first, try to look for a netmap port with this name */
	fd = open("/dev/netmap", O_RDONLY);
	if (fd < 0) {
		snprintf(errmsg, MAXERRMSG, "cannot open /dev/netmap: %s", strerror(errno));
		goto fail;
	}
	memset(&greq, 0, sizeof(greq));
	if (nm_parse_one(memid, &greq, err, 0) == NM_PARSE_OK) {
		greq.nr_version = NETMAP_API;
		if (ioctl(fd, NIOCGINFO, &greq) < 0) {
			if (errno == ENOENT || errno == ENXIO)
				goto try_external;
			snprintf(errmsg, MAXERRMSG, "cannot getinfo for %s: %s", memid, strerror(errno));
			goto fail;
		}
		req->nr_arg2 = greq.nr_arg2;
		close(fd);
		return 0;
	}
try_external:
	D("trying with external memory");
	close(fd);
	fd = open(memid, O_RDWR);
	if (fd < 0) {
		snprintf(errmsg, MAXERRMSG, "cannot open %s: %s", memid, strerror(errno));
		goto fail;
	}
	mapsize = lseek(fd, 0, SEEK_END);
	if (mapsize < 0) {
		snprintf(errmsg, MAXERRMSG, "failed to obtain filesize of %s: %s", memid, strerror(errno));
		goto fail;
	}
	pi = mmap(0, mapsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (pi == MAP_FAILED) {
		snprintf(errmsg, MAXERRMSG, "cannot map %s: %s", memid, strerror(errno));
		goto fail;
	}
	req->nr_cmd = NETMAP_POOLS_CREATE;
	pi->memsize = mapsize;
	nmreq_pointer_put(req, pi);
	D("mapped %zu bytes at %p from file %s", mapsize, pi, memid);
	return 0;

fail:
	D("%s", errmsg);
	close(fd);
	if (err && !*err)
		*err = strdup(errmsg);
	return errno;
}

static int
nm_parse(const char *ifname, struct nm_desc *d, char *errmsg)
{
	char *err;
	switch (nm_parse_one(ifname, &d->req, &err, 1)) {
	case NM_PARSE_OK:
		D("parse OK");
		break;
	case NM_PARSE_MEMID:
		D("memid: %s", err);
		errno = nm_interp_memid(err, &d->req, &err);
		D("errno = %d", errno);
		if (!errno)
			break;
		/* fallthrough */
	default:
		D("error");
		strncpy(errmsg, err, MAXERRMSG);
		errmsg[MAXERRMSG-1] = '\0';
		free(err);
		return -1;
	}
	D("parsed name: %s", d->req.nr_name);
	d->self = d;
	return 0;
}

/*
 * Try to open, return descriptor if successful, NULL otherwise.
 * An invalid netmap name will return errno = 0;
 * You can pass a pointer to a pre-filled nm_desc to add special
 * parameters. Flags is used as follows
 * NM_OPEN_NO_MMAP	use the memory from arg, only
 *			if the nr_arg2 (memory block) matches.
 *			Special case: if arg is NULL, skip the
 *			mmap entirely (maybe because you are going
 *			to do it by yourself, or you plan to call
 *			nm_mmap() only later)
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
	char errmsg[MAXERRMSG] = "";
	uint32_t nr_reg;
	struct netmap_pools_info *pi = NULL;

	if (strncmp(ifname, "netmap:", 7) &&
			strncmp(ifname, NM_BDG_NAME, strlen(NM_BDG_NAME))) {
		errno = 0; /* name not recognised, not an error */
		return NULL;
	}

	d = (struct nm_desc *)calloc(1, sizeof(*d));
	if (d == NULL) {
		snprintf(errmsg, MAXERRMSG, "nm_desc alloc failure");
		errno = ENOMEM;
		return NULL;
	}
	d->self = d;	/* set this early so nm_close() works */
	d->fd = open(NETMAP_DEVICE_NAME, O_RDWR);
	if (d->fd < 0) {
		snprintf(errmsg, MAXERRMSG, "cannot open /dev/netmap: %s", strerror(errno));
		goto fail;
	}

	if (req) {
		d->req = *req;
	} else {
		d->req.nr_arg1 = 4;
		d->req.nr_arg2 = 0;
		d->req.nr_arg3 = 0;
	}

	if (!(new_flags & NM_OPEN_IFNAME)) {
		char *err;
		switch (nm_parse_one(ifname, &d->req, &err, 1)) {
		case NM_PARSE_OK:
			break;
		case NM_PARSE_MEMID:
			if ((new_flags & NM_OPEN_NO_MMAP) &&
					IS_NETMAP_DESC(parent)) {
				/* ignore the memid setting, since we are
				 * going to use the parent's one
				 */
				break;
			}
			errno = nm_interp_memid(err, &d->req, &err);
			if (!errno)
				break;
			/* fallthrough */
		default:
			strncpy(errmsg, err, MAXERRMSG);
			errmsg[MAXERRMSG-1] = '\0';
			free(err);
			goto fail;
		}
		d->self = d;
	}

	/* compatibility checks for POOL_SCREATE and NM_OPEN flags
	 * the first check may be dropped once we have a larger nreq
	 */
	if (d->req.nr_cmd == NETMAP_POOLS_CREATE) {
		if (IS_NETMAP_DESC(parent)) {
		       	if (new_flags & (NM_OPEN_ARG1 | NM_OPEN_ARG2 | NM_OPEN_ARG3)) {
				snprintf(errmsg, MAXERRMSG,
						"POOLS_CREATE is incompatibile "
						"with NM_OPEN_ARG? flags");
				errno = EINVAL;
				goto fail;
			}
			if (new_flags & NM_OPEN_NO_MMAP) {
				snprintf(errmsg, MAXERRMSG,
						"POOLS_CREATE is incompatible "
						"with NM_OPEN_NO_MMAP flag");
				errno = EINVAL;
				goto fail;
			}
		}
	}

	d->req.nr_version = NETMAP_API;
	d->req.nr_ringid &= NETMAP_RING_MASK;

	/* optionally import info from parent */
	if (IS_NETMAP_DESC(parent) && new_flags) {
		if (new_flags & NM_OPEN_EXTMEM) {
			if (parent->req.nr_cmd == NETMAP_POOLS_CREATE) {
				d->req.nr_cmd = NETMAP_POOLS_CREATE;
				nmreq_pointer_put(&d->req, nmreq_pointer_get(&parent->req));
				D("Warning: not overriding arg[1-3] since external memory is being used");
				new_flags &= ~(NM_OPEN_ARG1 | NM_OPEN_ARG2 | NM_OPEN_ARG3);
			}
		}
		if (new_flags & NM_OPEN_ARG1) {
			D("overriding ARG1 %d", parent->req.nr_arg1);
			d->req.nr_arg1 = parent->req.nr_arg1;
		}
		if (new_flags & (NM_OPEN_ARG2 | NM_OPEN_NO_MMAP)) {
			D("overriding ARG2 %d", parent->req.nr_arg2);
			d->req.nr_arg2 = parent->req.nr_arg2;
		}
		if (new_flags & NM_OPEN_ARG3) {
			D("overriding ARG3 %d", parent->req.nr_arg3);
			d->req.nr_arg3 = parent->req.nr_arg3;
		}
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

	if (d->req.nr_cmd == NETMAP_POOLS_CREATE) {
		pi = nmreq_pointer_get(&d->req);
	}

	if (ioctl(d->fd, NIOCREGIF, &d->req)) {
		snprintf(errmsg, MAXERRMSG, "NIOCREGIF failed: %s", strerror(errno));
		goto fail;
	}

	if (pi != NULL) {
		d->mem = pi;
		d->memsize = pi->memsize;
		nm_init_offsets(d);
	} else if ((!(new_flags & NM_OPEN_NO_MMAP) || parent)) {
		/* if parent is defined, do nm_mmap() even if NM_OPEN_NO_MMAP is set */
	        errno = nm_mmap(d, parent);
		if (errno) {
			snprintf(errmsg, MAXERRMSG, "mmap failed: %s", strerror(errno));
			goto fail;
		}
	}

	nr_reg = d->req.nr_flags & NR_REG_MASK;

	if (nr_reg == NR_REG_SW) { /* host stack */
		d->first_tx_ring = d->last_tx_ring = d->req.nr_tx_rings;
		d->first_rx_ring = d->last_rx_ring = d->req.nr_rx_rings;
	} else if (nr_reg ==  NR_REG_ALL_NIC) { /* only nic */
		d->first_tx_ring = 0;
		d->first_rx_ring = 0;
		d->last_tx_ring = d->req.nr_tx_rings - 1;
		d->last_rx_ring = d->req.nr_rx_rings - 1;
	} else if (nr_reg ==  NR_REG_NIC_SW) {
		d->first_tx_ring = 0;
		d->first_rx_ring = 0;
		d->last_tx_ring = d->req.nr_tx_rings;
		d->last_rx_ring = d->req.nr_rx_rings;
	} else if (nr_reg == NR_REG_ONE_NIC) {
		/* XXX check validity */
		d->first_tx_ring = d->last_tx_ring =
		d->first_rx_ring = d->last_rx_ring = d->req.nr_ringid & NETMAP_RING_MASK;
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
	if (errmsg[0])
		D("%s %s", errmsg, ifname);
	if (errno == 0)
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
		  (void *)nm_dispatch, (void *)nm_nextpkt,
	          (void *)nm_parse } ;

	if (d == NULL || d->self != d)
		return EINVAL;
	if (d->done_mmap && d->mem)
		munmap(d->mem, d->memsize);
	if (d->fd != -1) {
		close(d->fd);
	}

	bzero(d, sizeof(*d));
	free(d);
	return 0;
}


static int
nm_mmap(struct nm_desc *d, const struct nm_desc *parent)
{
	//XXX TODO: check if mmap is already done

	if (IS_NETMAP_DESC(parent) && parent->mem &&
	    parent->req.nr_arg2 == d->req.nr_arg2) {
		/* do not mmap, inherit from parent */
		D("do not mmap, inherit from parent");
		d->memsize = parent->memsize;
		d->mem = parent->mem;
	} else {
		/* XXX TODO: check if memsize is too large (or there is overflow) */
		d->memsize = d->req.nr_memsize;
		d->mem = mmap(0, d->memsize, PROT_WRITE | PROT_READ, MAP_SHARED,
				d->fd, 0);
		if (d->mem == MAP_FAILED) {
			goto fail;
		}
		d->done_mmap = 1;
	}

	nm_init_offsets(d);
	return 0;

fail:
	return EINVAL;
}

/*
 * Same prototype as pcap_inject(), only need to cast.
 */
static int
nm_inject(struct nm_desc *d, const void *buf, size_t size)
{
	u_int c, n = d->last_tx_ring - d->first_tx_ring + 1,
		ri = d->cur_tx_ring;

	for (c = 0; c < n ; c++, ri++) {
		/* compute current ring to use */
		struct netmap_ring *ring;
		uint32_t i, idx;

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
	d->hdr.buf = NULL;
	d->hdr.flags = NM_MORE_PKTS;
	d->hdr.d = d;

	if (cnt == 0)
		cnt = -1;
	/* cnt == -1 means infinite, but rings have a finite amount
	 * of buffers and the int is large enough that we never wrap,
	 * so we can omit checking for -1
	 */
	for (c=0; c < n && cnt != got; c++, ri++) {
		/* compute current ring to use */
		struct netmap_ring *ring;

		if (ri > d->last_rx_ring)
			ri = d->first_rx_ring;
		ring = NETMAP_RXRING(d->nifp, ri);
		for ( ; !nm_ring_empty(ring) && cnt != got; got++) {
			u_int idx, i;
			if (d->hdr.buf) { /* from previous round */
				cb(arg, &d->hdr, d->hdr.buf);
			}
			i = ring->cur;
			idx = ring->slot[i].buf_idx;
			/* d->cur_rx_ring doesn't change inside this loop, but
			 * set it here, so it reflects d->hdr.buf's ring */
			d->cur_rx_ring = ri;
			d->hdr.slot = &ring->slot[i];
			d->hdr.buf = (u_char *)NETMAP_BUF(ring, idx);
			// __builtin_prefetch(buf);
			d->hdr.len = d->hdr.caplen = ring->slot[i].len;
			d->hdr.ts = ring->ts;
			ring->head = ring->cur = nm_ring_next(ring, i);
		}
	}
	if (d->hdr.buf) { /* from previous round */
		d->hdr.flags = 0;
		cb(arg, &d->hdr, d->hdr.buf);
	}
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
