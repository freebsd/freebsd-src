/*-
 * Copyright (c) 2015 Dmitry Chagin <dchagin@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef _LINUX_MI_H_
#define _LINUX_MI_H_

/*
 * Private Brandinfo flags
 */
#define	LINUX_BI_FUTEX_REQUEUE	0x01000000

/*
 * poll()
 */
#define	LINUX_POLLIN		0x0001
#define	LINUX_POLLPRI		0x0002
#define	LINUX_POLLOUT		0x0004
#define	LINUX_POLLERR		0x0008
#define	LINUX_POLLHUP		0x0010
#define	LINUX_POLLNVAL		0x0020
#define	LINUX_POLLRDNORM	0x0040
#define	LINUX_POLLRDBAND	0x0080
#define	LINUX_POLLWRNORM	0x0100
#define	LINUX_POLLWRBAND	0x0200
#define	LINUX_POLLMSG		0x0400
#define	LINUX_POLLREMOVE	0x1000
#define	LINUX_POLLRDHUP		0x2000

#define	LINUX_IFHWADDRLEN	6
#define	LINUX_IFNAMSIZ		16

struct l_sockaddr {
	unsigned short	sa_family;
	char		sa_data[14];
};

#define	LINUX_ARPHRD_ETHER	1
#define	LINUX_ARPHRD_LOOPBACK	772

/*
 * Supported address families
 */
#define	LINUX_AF_UNSPEC		0
#define	LINUX_AF_UNIX		1
#define	LINUX_AF_INET		2
#define	LINUX_AF_AX25		3
#define	LINUX_AF_IPX		4
#define	LINUX_AF_APPLETALK	5
#define	LINUX_AF_INET6		10
#define	LINUX_AF_NETLINK	16

#define	LINUX_NETLINK_ROUTE		0
#define	LINUX_NETLINK_SOCK_DIAG		4
#define	LINUX_NETLINK_NFLOG		5
#define	LINUX_NETLINK_SELINUX		7
#define	LINUX_NETLINK_AUDIT		9
#define	LINUX_NETLINK_FIB_LOOKUP	10
#define	LINUX_NETLINK_NETFILTER		12
#define	LINUX_NETLINK_KOBJECT_UEVENT	15

/*
 * net device flags
 */
#define	LINUX_IFF_UP		0x0001
#define	LINUX_IFF_BROADCAST	0x0002
#define	LINUX_IFF_DEBUG		0x0004
#define	LINUX_IFF_LOOPBACK	0x0008
#define	LINUX_IFF_POINTOPOINT	0x0010
#define	LINUX_IFF_NOTRAILERS	0x0020
#define	LINUX_IFF_RUNNING	0x0040
#define	LINUX_IFF_NOARP		0x0080
#define	LINUX_IFF_PROMISC	0x0100
#define	LINUX_IFF_ALLMULTI	0x0200
#define	LINUX_IFF_MASTER	0x0400
#define	LINUX_IFF_SLAVE		0x0800
#define	LINUX_IFF_MULTICAST	0x1000
#define	LINUX_IFF_PORTSEL	0x2000
#define	LINUX_IFF_AUTOMEDIA	0x4000
#define	LINUX_IFF_DYNAMIC	0x8000

/* sigaltstack */
#define	LINUX_SS_ONSTACK	1
#define	LINUX_SS_DISABLE	2

int linux_to_bsd_sigaltstack(int lsa);
int bsd_to_linux_sigaltstack(int bsa);

/* sigset */
typedef struct {
	uint64_t	__mask;
} l_sigset_t;

/* primitives to manipulate sigset_t */
#define	LINUX_SIGEMPTYSET(set)		(set).__mask = 0
#define	LINUX_SIGISMEMBER(set, sig)	(1ULL & ((set).__mask >> _SIG_IDX(sig)))
#define	LINUX_SIGADDSET(set, sig)	(set).__mask |= 1ULL << _SIG_IDX(sig)

void linux_to_bsd_sigset(l_sigset_t *, sigset_t *);
void bsd_to_linux_sigset(sigset_t *, l_sigset_t *);

/* signaling */
#define	LINUX_SIGHUP		1
#define	LINUX_SIGINT		2
#define	LINUX_SIGQUIT		3
#define	LINUX_SIGILL		4
#define	LINUX_SIGTRAP		5
#define	LINUX_SIGABRT		6
#define	LINUX_SIGIOT		LINUX_SIGABRT
#define	LINUX_SIGBUS		7
#define	LINUX_SIGFPE		8
#define	LINUX_SIGKILL		9
#define	LINUX_SIGUSR1		10
#define	LINUX_SIGSEGV		11
#define	LINUX_SIGUSR2		12
#define	LINUX_SIGPIPE		13
#define	LINUX_SIGALRM		14
#define	LINUX_SIGTERM		15
#define	LINUX_SIGSTKFLT		16
#define	LINUX_SIGCHLD		17
#define	LINUX_SIGCONT		18
#define	LINUX_SIGSTOP		19
#define	LINUX_SIGTSTP		20
#define	LINUX_SIGTTIN		21
#define	LINUX_SIGTTOU		22
#define	LINUX_SIGURG		23
#define	LINUX_SIGXCPU		24
#define	LINUX_SIGXFSZ		25
#define	LINUX_SIGVTALRM		26
#define	LINUX_SIGPROF		27
#define	LINUX_SIGWINCH		28
#define	LINUX_SIGIO		29
#define	LINUX_SIGPOLL		LINUX_SIGIO
#define	LINUX_SIGPWR		30
#define	LINUX_SIGSYS		31
#define	LINUX_SIGTBLSZ		31
#define	LINUX_SIGRTMIN		32
#define	LINUX_SIGRTMAX		64

#define LINUX_SIG_VALID(sig)	((sig) <= LINUX_SIGRTMAX && (sig) > 0)

int linux_to_bsd_signal(int sig);
int bsd_to_linux_signal(int sig);

/* sigprocmask actions */
#define	LINUX_SIG_BLOCK		0
#define	LINUX_SIG_UNBLOCK	1
#define	LINUX_SIG_SETMASK	2

void linux_dev_shm_create(void);
void linux_dev_shm_destroy(void);

/*
 * mask=0 is not sensible for this application, so it will be taken to mean
 * a mask equivalent to the value.  Otherwise, (word & mask) == value maps to
 * (word & ~mask) | value in a bitfield for the platform we're converting to.
 */
struct bsd_to_linux_bitmap {
	int	bsd_mask;
	int	bsd_value;
	int	linux_mask;
	int	linux_value;
};

int bsd_to_linux_bits_(int value, struct bsd_to_linux_bitmap *bitmap,
    size_t mapcnt, int no_value);
int linux_to_bsd_bits_(int value, struct bsd_to_linux_bitmap *bitmap,
    size_t mapcnt, int no_value);

/*
 * These functions are used for simplification of BSD <-> Linux bit conversions.
 * Given `value`, a bit field, these functions will walk the given bitmap table
 * and set the appropriate bits for the target platform.  If any bits were
 * successfully converted, then the return value is the equivalent of value
 * represented with the bit values appropriate for the target platform.
 * Otherwise, the value supplied as `no_value` is returned.
 */
#define	bsd_to_linux_bits(_val, _bmap, _noval) \
    bsd_to_linux_bits_((_val), (_bmap), nitems((_bmap)), (_noval))
#define	linux_to_bsd_bits(_val, _bmap, _noval) \
    linux_to_bsd_bits_((_val), (_bmap), nitems((_bmap)), (_noval))

/*
 * Easy mapping helpers.  BITMAP_EASY_LINUX represents a single bit to be
 * translated, and the FreeBSD and Linux values are supplied.  BITMAP_1t1_LINUX
 * is the extreme version of this, where not only is it a single bit, but the
 * name of the macro used to represent the Linux version of a bit literally has
 * LINUX_ prepended to the normal name.
 */
#define	BITMAP_EASY_LINUX(_name, _linux_name)	\
	{					\
		.bsd_value = (_name),		\
		.linux_value = (_linux_name),	\
	}
#define	BITMAP_1t1_LINUX(_name)	BITMAP_EASY_LINUX(_name, LINUX_##_name)

int bsd_to_linux_errno(int error);
void linux_check_errtbl(void);

#define STATX_BASIC_STATS		0x07ff
#define STATX_BTIME			0x0800
#define STATX_ALL			0x0fff

#define STATX_ATTR_COMPRESSED		0x0004
#define STATX_ATTR_IMMUTABLE		0x0010
#define STATX_ATTR_APPEND		0x0020
#define STATX_ATTR_NODUMP		0x0040
#define STATX_ATTR_ENCRYPTED		0x0800
#define STATX_ATTR_AUTOMOUNT		0x1000

struct l_statx_timestamp {
	int64_t tv_sec;
	int32_t tv_nsec;
	int32_t __spare0;
};

struct l_statx {
	uint32_t stx_mask;
	uint32_t stx_blksize;
	uint64_t stx_attributes;
	uint32_t stx_nlink;
	uint32_t stx_uid;
	uint32_t stx_gid;
	uint16_t stx_mode;
	uint16_t __spare0[1];
	uint64_t stx_ino;
	uint64_t stx_size;
	uint64_t stx_blocks;
	uint64_t stx_attributes_mask;
	struct l_statx_timestamp stx_atime;
	struct l_statx_timestamp stx_btime;
	struct l_statx_timestamp stx_ctime;
	struct l_statx_timestamp stx_mtime;
	uint32_t stx_rdev_major;
	uint32_t stx_rdev_minor;
	uint32_t stx_dev_major;
	uint32_t stx_dev_minor;
	uint64_t stx_mnt_id;
	uint64_t __spare2[13];
};

/*
 * statfs f_flags
 */
#define	LINUX_ST_RDONLY			0x0001
#define	LINUX_ST_NOSUID			0x0002
#define	LINUX_ST_NODEV			0x0004	/* No native analogue */
#define	LINUX_ST_NOEXEC			0x0008
#define	LINUX_ST_SYNCHRONOUS		0x0010
#define	LINUX_ST_VALID			0x0020
#define	LINUX_ST_MANDLOCK		0x0040	/* No native analogue */
#define	LINUX_ST_NOATIME		0x0400
#define	LINUX_ST_NODIRATIME		0x0800	/* No native analogue */
#define	LINUX_ST_RELATIME		0x1000	/* No native analogue */
#define	LINUX_ST_NOSYMFOLLOW		0x2000

#define	lower_32_bits(n)	((uint32_t)((n) & 0xffffffff))

#ifdef KTRACE
#define	linux_ktrsigset(s, l)	\
	ktrstruct("l_sigset_t", (s), l)
#endif

/*
 * Criteria for interface name translation
 */
#define	IFP_IS_ETH(ifp)		((ifp)->if_type == IFT_ETHER)
#define	IFP_IS_LOOP(ifp)	((ifp)->if_type == IFT_LOOP)

struct ifnet;

bool linux_use_real_ifname(const struct ifnet *);

void linux_netlink_register(void);
void linux_netlink_deregister(void);

#endif /* _LINUX_MI_H_ */
