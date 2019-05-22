/*-
 * Copyright (c) 2015 Dmitry Chagin
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ctype.h>
#include <sys/jail.h>
#include <sys/lock.h>
#include <sys/signalvar.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <compat/linux/linux.h>
#include <compat/linux/linux_common.h>

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
	SIGRTMIN,	/* LINUX_SIGPWR */
	SIGSYS		/* LINUX_SIGSYS */
};

/*
 * Map Linux RT signals to the FreeBSD RT signals.
 */
static inline int
linux_to_bsd_rt_signal(int sig)
{

	return (SIGRTMIN + 1 + sig - LINUX_SIGRTMIN);
}

static inline int
bsd_to_linux_rt_signal(int sig)
{

	return (sig - SIGRTMIN - 1 + LINUX_SIGRTMIN);
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
	if (sig == SIGRTMIN)
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
	int is_eth, is_lo, index;

	for (len = 0; len < LINUX_IFNAMSIZ; ++len)
		if (!isalpha(lxname[len]) || lxname[len] == 0)
			break;
	if (len == 0 || len == LINUX_IFNAMSIZ)
		return (NULL);
	/* Linux loopback interface name is lo (not lo0) */
	is_lo = (len == 2 && !strncmp(lxname, "lo", len)) ? 1 : 0;
	unit = (int)strtoul(lxname + len, &ep, 10);
	if ((ep == NULL || ep == lxname + len || ep >= lxname + LINUX_IFNAMSIZ) &&
	    is_lo == 0)
		return (NULL);
	index = 0;
	is_eth = (len == 3 && !strncmp(lxname, "eth", len)) ? 1 : 0;

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

	*flags = (ifp->if_flags | ifp->if_drv_flags) & 0xffff;
	/* these flags have no Linux equivalent */
	*flags &= ~(IFF_DRV_OACTIVE|IFF_SIMPLEX|
	    IFF_LINK0|IFF_LINK1|IFF_LINK2);
	/* Linux' multicast flag is in a different bit */
	if (*flags & IFF_MULTICAST) {
		*flags &= ~IFF_MULTICAST;
		*flags |= 0x1000;
	}
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
