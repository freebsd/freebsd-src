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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <opt_inet6.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/ctype.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/jail.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netlink/netlink.h>

#include <sys/un.h>
#include <netinet/in.h>

#include <compat/linux/linux.h>
#include <compat/linux/linux_common.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_util.h>

CTASSERT(LINUX_IFNAMSIZ == IFNAMSIZ);

static int bsd_to_linux_sigtbl[LINUX_SIGTBLSZ] = {
	LINUX_SIGHUP,	/* SIGHUP */
	LINUX_SIGINT,	/* SIGINT */
	LINUX_SIGQUIT,	/* SIGQUIT */
	LINUX_SIGILL,	/* SIGILL */
	LINUX_SIGTRAP,	/* SIGTRAP */
	LINUX_SIGABRT,	/* SIGABRT */
	0,		/* SIGEMT */
	LINUX_SIGFPE,	/* SIGFPE */
	LINUX_SIGKILL,	/* SIGKILL */
	LINUX_SIGBUS,	/* SIGBUS */
	LINUX_SIGSEGV,	/* SIGSEGV */
	LINUX_SIGSYS,	/* SIGSYS */
	LINUX_SIGPIPE,	/* SIGPIPE */
	LINUX_SIGALRM,	/* SIGALRM */
	LINUX_SIGTERM,	/* SIGTERM */
	LINUX_SIGURG,	/* SIGURG */
	LINUX_SIGSTOP,	/* SIGSTOP */
	LINUX_SIGTSTP,	/* SIGTSTP */
	LINUX_SIGCONT,	/* SIGCONT */
	LINUX_SIGCHLD,	/* SIGCHLD */
	LINUX_SIGTTIN,	/* SIGTTIN */
	LINUX_SIGTTOU,	/* SIGTTOU */
	LINUX_SIGIO,	/* SIGIO */
	LINUX_SIGXCPU,	/* SIGXCPU */
	LINUX_SIGXFSZ,	/* SIGXFSZ */
	LINUX_SIGVTALRM,/* SIGVTALRM */
	LINUX_SIGPROF,	/* SIGPROF */
	LINUX_SIGWINCH,	/* SIGWINCH */
	0,		/* SIGINFO */
	LINUX_SIGUSR1,	/* SIGUSR1 */
	LINUX_SIGUSR2	/* SIGUSR2 */
};

#define	LINUX_SIGPWREMU	(SIGRTMIN + (LINUX_SIGRTMAX - LINUX_SIGRTMIN) + 1)

static int linux_to_bsd_sigtbl[LINUX_SIGTBLSZ] = {
	SIGHUP,		/* LINUX_SIGHUP */
	SIGINT,		/* LINUX_SIGINT */
	SIGQUIT,	/* LINUX_SIGQUIT */
	SIGILL,		/* LINUX_SIGILL */
	SIGTRAP,	/* LINUX_SIGTRAP */
	SIGABRT,	/* LINUX_SIGABRT */
	SIGBUS,		/* LINUX_SIGBUS */
	SIGFPE,		/* LINUX_SIGFPE */
	SIGKILL,	/* LINUX_SIGKILL */
	SIGUSR1,	/* LINUX_SIGUSR1 */
	SIGSEGV,	/* LINUX_SIGSEGV */
	SIGUSR2,	/* LINUX_SIGUSR2 */
	SIGPIPE,	/* LINUX_SIGPIPE */
	SIGALRM,	/* LINUX_SIGALRM */
	SIGTERM,	/* LINUX_SIGTERM */
	SIGBUS,		/* LINUX_SIGSTKFLT */
	SIGCHLD,	/* LINUX_SIGCHLD */
	SIGCONT,	/* LINUX_SIGCONT */
	SIGSTOP,	/* LINUX_SIGSTOP */
	SIGTSTP,	/* LINUX_SIGTSTP */
	SIGTTIN,	/* LINUX_SIGTTIN */
	SIGTTOU,	/* LINUX_SIGTTOU */
	SIGURG,		/* LINUX_SIGURG */
	SIGXCPU,	/* LINUX_SIGXCPU */
	SIGXFSZ,	/* LINUX_SIGXFSZ */
	SIGVTALRM,	/* LINUX_SIGVTALARM */
	SIGPROF,	/* LINUX_SIGPROF */
	SIGWINCH,	/* LINUX_SIGWINCH */
	SIGIO,		/* LINUX_SIGIO */
	/*
	 * FreeBSD does not have SIGPWR signal, map Linux SIGPWR signal
	 * to the first unused FreeBSD signal number. Since Linux supports
	 * signals from 1 to 64 we are ok here as our SIGRTMIN = 65.
	 */
	LINUX_SIGPWREMU,/* LINUX_SIGPWR */
	SIGSYS		/* LINUX_SIGSYS */
};

static struct cdev *dev_shm_cdev;
static struct cdevsw dev_shm_cdevsw = {
     .d_version = D_VERSION,
     .d_name    = "dev_shm",
};

/*
 * Map Linux RT signals to the FreeBSD RT signals.
 */
static inline int
linux_to_bsd_rt_signal(int sig)
{

	return (SIGRTMIN + sig - LINUX_SIGRTMIN);
}

static inline int
bsd_to_linux_rt_signal(int sig)
{

	return (sig - SIGRTMIN + LINUX_SIGRTMIN);
}

int
linux_to_bsd_signal(int sig)
{

	KASSERT(sig > 0 && sig <= LINUX_SIGRTMAX, ("invalid Linux signal %d\n", sig));

	if (sig < LINUX_SIGRTMIN)
		return (linux_to_bsd_sigtbl[_SIG_IDX(sig)]);

	return (linux_to_bsd_rt_signal(sig));
}

int
bsd_to_linux_signal(int sig)
{

	if (sig <= LINUX_SIGTBLSZ)
		return (bsd_to_linux_sigtbl[_SIG_IDX(sig)]);
	if (sig == LINUX_SIGPWREMU)
		return (LINUX_SIGPWR);

	return (bsd_to_linux_rt_signal(sig));
}

int
linux_to_bsd_sigaltstack(int lsa)
{
	int bsa = 0;

	if (lsa & LINUX_SS_DISABLE)
		bsa |= SS_DISABLE;
	/*
	 * Linux ignores SS_ONSTACK flag for ss
	 * parameter while FreeBSD prohibits it.
	 */
	return (bsa);
}

int
bsd_to_linux_sigaltstack(int bsa)
{
	int lsa = 0;

	if (bsa & SS_DISABLE)
		lsa |= LINUX_SS_DISABLE;
	if (bsa & SS_ONSTACK)
		lsa |= LINUX_SS_ONSTACK;
	return (lsa);
}

void
linux_to_bsd_sigset(l_sigset_t *lss, sigset_t *bss)
{
	int b, l;

	SIGEMPTYSET(*bss);
	for (l = 1; l <= LINUX_SIGRTMAX; l++) {
		if (LINUX_SIGISMEMBER(*lss, l)) {
			b = linux_to_bsd_signal(l);
			if (b)
				SIGADDSET(*bss, b);
		}
	}
}

void
bsd_to_linux_sigset(sigset_t *bss, l_sigset_t *lss)
{
	int b, l;

	LINUX_SIGEMPTYSET(*lss);
	for (b = 1; b <= SIGRTMAX; b++) {
		if (SIGISMEMBER(*bss, b)) {
			l = bsd_to_linux_signal(b);
			if (l)
				LINUX_SIGADDSET(*lss, l);
		}
	}
}

/*
 * Translate a Linux interface name to a FreeBSD interface name,
 * and return the associated ifnet structure
 * bsdname and lxname need to be least IFNAMSIZ bytes long, but
 * can point to the same buffer.
 */
struct ifnet *
ifname_linux_to_bsd(struct thread *td, const char *lxname, char *bsdname)
{
	struct ifnet *ifp;
	int len, unit;
	char *ep;
	int index;
	bool is_eth, is_lo;

	for (len = 0; len < LINUX_IFNAMSIZ; ++len)
		if (!isalpha(lxname[len]) || lxname[len] == '\0')
			break;
	if (len == 0 || len == LINUX_IFNAMSIZ)
		return (NULL);
	/* Linux loopback interface name is lo (not lo0) */
	is_lo = (len == 2 && strncmp(lxname, "lo", len) == 0);
	unit = (int)strtoul(lxname + len, &ep, 10);
	if ((ep == NULL || ep == lxname + len || ep >= lxname + LINUX_IFNAMSIZ) &&
	    is_lo == 0)
		return (NULL);
	index = 0;
	is_eth = (len == 3 && strncmp(lxname, "eth", len) == 0);

	CURVNET_SET(TD_TO_VNET(td));
	IFNET_RLOCK();
	CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		/*
		 * Allow Linux programs to use FreeBSD names. Don't presume
		 * we never have an interface named "eth", so don't make
		 * the test optional based on is_eth.
		 */
		if (strncmp(ifp->if_xname, lxname, LINUX_IFNAMSIZ) == 0)
			break;
		if (is_eth && IFP_IS_ETH(ifp) && unit == index++)
			break;
		if (is_lo && IFP_IS_LOOP(ifp))
			break;
	}
	IFNET_RUNLOCK();
	CURVNET_RESTORE();
	if (ifp != NULL && bsdname != NULL)
		strlcpy(bsdname, ifp->if_xname, IFNAMSIZ);
	return (ifp);
}

void
linux_ifflags(struct ifnet *ifp, short *flags)
{
	unsigned short fl;

	fl = (ifp->if_flags | ifp->if_drv_flags) & 0xffff;
	*flags = 0;
	if (fl & IFF_UP)
		*flags |= LINUX_IFF_UP;
	if (fl & IFF_BROADCAST)
		*flags |= LINUX_IFF_BROADCAST;
	if (fl & IFF_DEBUG)
		*flags |= LINUX_IFF_DEBUG;
	if (fl & IFF_LOOPBACK)
		*flags |= LINUX_IFF_LOOPBACK;
	if (fl & IFF_POINTOPOINT)
		*flags |= LINUX_IFF_POINTOPOINT;
	if (fl & IFF_DRV_RUNNING)
		*flags |= LINUX_IFF_RUNNING;
	if (fl & IFF_NOARP)
		*flags |= LINUX_IFF_NOARP;
	if (fl & IFF_PROMISC)
		*flags |= LINUX_IFF_PROMISC;
	if (fl & IFF_ALLMULTI)
		*flags |= LINUX_IFF_ALLMULTI;
	if (fl & IFF_MULTICAST)
		*flags |= LINUX_IFF_MULTICAST;
}

int
linux_ifhwaddr(struct ifnet *ifp, struct l_sockaddr *lsa)
{
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;

	if (IFP_IS_LOOP(ifp)) {
		bzero(lsa, sizeof(*lsa));
		lsa->sa_family = LINUX_ARPHRD_LOOPBACK;
		return (0);
	}

	if (!IFP_IS_ETH(ifp))
		return (ENOENT);

	CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		sdl = (struct sockaddr_dl*)ifa->ifa_addr;
		if (sdl != NULL && (sdl->sdl_family == AF_LINK) &&
		    (sdl->sdl_type == IFT_ETHER)) {
			bzero(lsa, sizeof(*lsa));
			lsa->sa_family = LINUX_ARPHRD_ETHER;
			bcopy(LLADDR(sdl), lsa->sa_data, LINUX_IFHWADDRLEN);
			return (0);
		}
	}

	return (ENOENT);
}

int
linux_to_bsd_domain(int domain)
{

	switch (domain) {
	case LINUX_AF_UNSPEC:
		return (AF_UNSPEC);
	case LINUX_AF_UNIX:
		return (AF_LOCAL);
	case LINUX_AF_INET:
		return (AF_INET);
	case LINUX_AF_INET6:
		return (AF_INET6);
	case LINUX_AF_AX25:
		return (AF_CCITT);
	case LINUX_AF_IPX:
		return (AF_IPX);
	case LINUX_AF_APPLETALK:
		return (AF_APPLETALK);
	case LINUX_AF_NETLINK:
		return (AF_NETLINK);
	}
	return (-1);
}

int
bsd_to_linux_domain(int domain)
{

	switch (domain) {
	case AF_UNSPEC:
		return (LINUX_AF_UNSPEC);
	case AF_LOCAL:
		return (LINUX_AF_UNIX);
	case AF_INET:
		return (LINUX_AF_INET);
	case AF_INET6:
		return (LINUX_AF_INET6);
	case AF_CCITT:
		return (LINUX_AF_AX25);
	case AF_IPX:
		return (LINUX_AF_IPX);
	case AF_APPLETALK:
		return (LINUX_AF_APPLETALK);
	case AF_NETLINK:
		return (LINUX_AF_NETLINK);
	}
	return (-1);
}

/*
 * Based on the fact that:
 * 1. Native and Linux storage of struct sockaddr
 * and struct sockaddr_in6 are equal.
 * 2. On Linux sa_family is the first member of all struct sockaddr.
 */
int
bsd_to_linux_sockaddr(const struct sockaddr *sa, struct l_sockaddr **lsa,
    socklen_t len)
{
	struct l_sockaddr *kosa;
	int bdom;

	*lsa = NULL;
	if (len < 2 || len > UCHAR_MAX)
		return (EINVAL);
	bdom = bsd_to_linux_domain(sa->sa_family);
	if (bdom == -1)
		return (EAFNOSUPPORT);

	kosa = malloc(len, M_LINUX, M_WAITOK);
	bcopy(sa, kosa, len);
	kosa->sa_family = bdom;
	*lsa = kosa;
	return (0);
}

int
linux_to_bsd_sockaddr(const struct l_sockaddr *osa, struct sockaddr **sap,
    socklen_t *len)
{
	struct sockaddr *sa;
	struct l_sockaddr *kosa;
#ifdef INET6
	struct sockaddr_in6 *sin6;
	bool  oldv6size;
#endif
	char *name;
	int salen, bdom, error, hdrlen, namelen;

	if (*len < 2 || *len > UCHAR_MAX)
		return (EINVAL);

	salen = *len;

#ifdef INET6
	oldv6size = false;
	/*
	 * Check for old (pre-RFC2553) sockaddr_in6. We may accept it
	 * if it's a v4-mapped address, so reserve the proper space
	 * for it.
	 */
	if (salen == sizeof(struct sockaddr_in6) - sizeof(uint32_t)) {
		salen += sizeof(uint32_t);
		oldv6size = true;
	}
#endif

	kosa = malloc(salen, M_SONAME, M_WAITOK);

	if ((error = copyin(osa, kosa, *len)))
		goto out;

	bdom = linux_to_bsd_domain(kosa->sa_family);
	if (bdom == -1) {
		error = EAFNOSUPPORT;
		goto out;
	}

#ifdef INET6
	/*
	 * Older Linux IPv6 code uses obsolete RFC2133 struct sockaddr_in6,
	 * which lacks the scope id compared with RFC2553 one. If we detect
	 * the situation, reject the address and write a message to system log.
	 *
	 * Still accept addresses for which the scope id is not used.
	 */
	if (oldv6size) {
		if (bdom == AF_INET6) {
			sin6 = (struct sockaddr_in6 *)kosa;
			if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr) ||
			    (!IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) &&
			     !IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr) &&
			     !IN6_IS_ADDR_V4COMPAT(&sin6->sin6_addr) &&
			     !IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr) &&
			     !IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr))) {
				sin6->sin6_scope_id = 0;
			} else {
				linux_msg(curthread,
				    "obsolete pre-RFC2553 sockaddr_in6 rejected");
				error = EINVAL;
				goto out;
			}
		} else
			salen -= sizeof(uint32_t);
	}
#endif
	if (bdom == AF_INET) {
		if (salen < sizeof(struct sockaddr_in)) {
			error = EINVAL;
			goto out;
		}
		salen = sizeof(struct sockaddr_in);
	}

	if (bdom == AF_LOCAL && salen > sizeof(struct sockaddr_un)) {
		hdrlen = offsetof(struct sockaddr_un, sun_path);
		name = ((struct sockaddr_un *)kosa)->sun_path;
		if (*name == '\0') {
			/*
			 * Linux abstract namespace starts with a NULL byte.
			 * XXX We do not support abstract namespace yet.
			 */
			namelen = strnlen(name + 1, salen - hdrlen - 1) + 1;
		} else
			namelen = strnlen(name, salen - hdrlen);
		salen = hdrlen + namelen;
		if (salen > sizeof(struct sockaddr_un)) {
			error = ENAMETOOLONG;
			goto out;
		}
	}

	if (bdom == AF_NETLINK) {
		if (salen < sizeof(struct sockaddr_nl)) {
			error = EINVAL;
			goto out;
		}
		salen = sizeof(struct sockaddr_nl);
	}

	sa = (struct sockaddr *)kosa;
	sa->sa_family = bdom;
	sa->sa_len = salen;

	*sap = sa;
	*len = salen;
	return (0);

out:
	free(kosa, M_SONAME);
	return (error);
}

void
linux_dev_shm_create(void)
{
	int error;

	error = make_dev_p(MAKEDEV_CHECKNAME | MAKEDEV_WAITOK, &dev_shm_cdev,
	    &dev_shm_cdevsw, NULL, UID_ROOT, GID_WHEEL, 0, "shm/.mountpoint");
	if (error != 0) {
		printf("%s: failed to create device node, error %d\n",
		    __func__, error);
	}
}

void
linux_dev_shm_destroy(void)
{

	destroy_dev(dev_shm_cdev);
}

int
bsd_to_linux_bits_(int value, struct bsd_to_linux_bitmap *bitmap,
    size_t mapcnt, int no_value)
{
	int bsd_mask, bsd_value, linux_mask, linux_value;
	int linux_ret;
	size_t i;
	bool applied;

	applied = false;
	linux_ret = 0;
	for (i = 0; i < mapcnt; ++i) {
		bsd_mask = bitmap[i].bsd_mask;
		bsd_value = bitmap[i].bsd_value;
		if (bsd_mask == 0)
			bsd_mask = bsd_value;

		linux_mask = bitmap[i].linux_mask;
		linux_value = bitmap[i].linux_value;
		if (linux_mask == 0)
			linux_mask = linux_value;

		/*
		 * If a mask larger than just the value is set, we explicitly
		 * want to make sure that only this bit we mapped within that
		 * mask is set.
		 */
		if ((value & bsd_mask) == bsd_value) {
			linux_ret = (linux_ret & ~linux_mask) | linux_value;
			applied = true;
		}
	}

	if (!applied)
		return (no_value);
	return (linux_ret);
}

int
linux_to_bsd_bits_(int value, struct bsd_to_linux_bitmap *bitmap,
    size_t mapcnt, int no_value)
{
	int bsd_mask, bsd_value, linux_mask, linux_value;
	int bsd_ret;
	size_t i;
	bool applied;

	applied = false;
	bsd_ret = 0;
	for (i = 0; i < mapcnt; ++i) {
		bsd_mask = bitmap[i].bsd_mask;
		bsd_value = bitmap[i].bsd_value;
		if (bsd_mask == 0)
			bsd_mask = bsd_value;

		linux_mask = bitmap[i].linux_mask;
		linux_value = bitmap[i].linux_value;
		if (linux_mask == 0)
			linux_mask = linux_value;

		/*
		 * If a mask larger than just the value is set, we explicitly
		 * want to make sure that only this bit we mapped within that
		 * mask is set.
		 */
		if ((value & linux_mask) == linux_value) {
			bsd_ret = (bsd_ret & ~bsd_mask) | bsd_value;
			applied = true;
		}
	}

	if (!applied)
		return (no_value);
	return (bsd_ret);
}

void
linux_to_bsd_poll_events(struct thread *td, int fd, short lev,
    short *bev)
{
	struct file *fp;
	int error;
	short bits = 0;

	if (lev & LINUX_POLLIN)
		bits |= POLLIN;
	if (lev & LINUX_POLLPRI)
		bits |=	POLLPRI;
	if (lev & LINUX_POLLOUT)
		bits |= POLLOUT;
	if (lev & LINUX_POLLERR)
		bits |= POLLERR;
	if (lev & LINUX_POLLHUP)
		bits |= POLLHUP;
	if (lev & LINUX_POLLNVAL)
		bits |= POLLNVAL;
	if (lev & LINUX_POLLRDNORM)
		bits |= POLLRDNORM;
	if (lev & LINUX_POLLRDBAND)
		bits |= POLLRDBAND;
	if (lev & LINUX_POLLWRBAND)
		bits |= POLLWRBAND;
	if (lev & LINUX_POLLWRNORM)
		bits |= POLLWRNORM;

	if (lev & LINUX_POLLRDHUP) {
		/*
		 * It seems that the Linux silencly ignores POLLRDHUP
		 * on non-socket file descriptors unlike FreeBSD, where
		 * events bits is more strictly checked (POLLSTANDARD).
		 */
		error = fget_unlocked(td, fd, &cap_no_rights, &fp);
		if (error == 0) {
			/*
			 * XXX. On FreeBSD POLLRDHUP applies only to
			 * stream sockets.
			 */
			if (fp->f_type == DTYPE_SOCKET)
				bits |= POLLRDHUP;
			fdrop(fp, td);
		}
	}

	if (lev & LINUX_POLLMSG)
		LINUX_RATELIMIT_MSG_OPT1("unsupported POLLMSG, events(%d)", lev);
	if (lev & LINUX_POLLREMOVE)
		LINUX_RATELIMIT_MSG_OPT1("unsupported POLLREMOVE, events(%d)", lev);

	*bev = bits;
}

void
bsd_to_linux_poll_events(short bev, short *lev)
{
	short bits = 0;

	if (bev & POLLIN)
		bits |= LINUX_POLLIN;
	if (bev & POLLPRI)
		bits |=	LINUX_POLLPRI;
	if (bev & (POLLOUT | POLLWRNORM))
		/*
		 * POLLWRNORM is equal to POLLOUT on FreeBSD,
		 * but not on Linux
		 */
		bits |= LINUX_POLLOUT;
	if (bev & POLLERR)
		bits |= LINUX_POLLERR;
	if (bev & POLLHUP)
		bits |= LINUX_POLLHUP;
	if (bev & POLLNVAL)
		bits |= LINUX_POLLNVAL;
	if (bev & POLLRDNORM)
		bits |= LINUX_POLLRDNORM;
	if (bev & POLLRDBAND)
		bits |= LINUX_POLLRDBAND;
	if (bev & POLLWRBAND)
		bits |= LINUX_POLLWRBAND;
	if (bev & POLLRDHUP)
		bits |= LINUX_POLLRDHUP;

	*lev = bits;
}
