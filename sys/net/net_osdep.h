/*	$FreeBSD$	*/
/*	$KAME: net_osdep.h,v 1.68 2001/12/21 08:14:58 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * glue for kernel code programming differences.
 */

/*
 * OS dependencies:
 * - ioctl
 *   FreeBSD 3 and later warn when sys/ioctl.h is included in a kernel source
 *   file.  For socket ioctl, we are suggested to use sys/sockio.h.
 *
 * - RTFREE()
 *   bsdi does not escape this macro using do-clause, so it is recommended
 *   to escape the macro explicitly.
 *   e.g.
 *	if (rt) {
 *		RTFREE(rt);
 *	}
 *
 * - whether the IPv4 input routine convert the byte order of some fileds
 *   of the IP header (x: convert to the host byte order, s: strip the header
 *   length for possible reassembly)
 *             ip_len ip_id ip_off
 * bsdi3:          xs     x      x
 * bsdi4:          xs            x
 * freebsd[23]:    xs     x      x 
 * freebsd4:       xs            x
 * NetBSD:          x            x
 * OpenBSD:        xs     x      x
 *
 * - ifa_ifwithaf()
 *   bsdi[34], netbsd, and openbsd define it in sys/net/if.c
 *   freebsd (all versions) does not have it.
 *  
 * - struct rt_addrinfo
 *   bsdi4, netbsd 1.5R and beyond: rti_addrs, rti_info[], rti_flags, rti_ifa,
 *	rti_ifp, and rti_rtm.
 *   others: rti_addrs and rti_info[] only.
 *
 * - ifa->ifa_rtrequest
 *   bsdi4, netbsd 1.5R and beyond: rt_addrinfo *
 *   others: sockaddr * (note that sys/net/route.c:rtrequest() has an unsafe
 *	typecast code, from 4.3BSD-reno)
 *
 * - side effects of rtrequest{,1}(RTM_DELETE)
 *	BSDI[34]: delete all cloned routes underneath the route.
 *	FreeBSD[234]: delete all protocol-cloned routes underneath the route.
 *		      note that cloned routes from an interface direct route
 *		      still remain.
 *	NetBSD: 1.5 have no side effects.  KAME/netbsd15, and post-1.5R, have
 *		the same effects as of BSDI.
 *	OpenBSD: have no side effects.  KAME/openbsd has the same effects as
 *		of BSDI (the change is not merged - yet).
 *
 * - privileged process
 *	NetBSD, FreeBSD 3
 *		struct proc *p;
 *		if (p && !suser(p->p_ucred, &p->p_acflag))
 *			privileged;
 *	FreeBSD 4
 *		struct proc *p;
 *		if (p && !suser(p))
 *			privileged;
 *	OpenBSD, BSDI [34], FreeBSD 2
 *		struct socket *so;
 *		if (so->so_state & SS_PRIV)
 *			privileged;
 * - foo_control
 *	NetBSD, FreeBSD 3
 *		needs to give struct proc * as argument
 *	OpenBSD, BSDI [34], FreeBSD 2
 *		do not need struct proc *
 *
 * - bpf:
 *	OpenBSD, NetBSD 1.5, BSDI [34]
 *		need caddr_t * (= if_bpf **) and struct ifnet *
 *	FreeBSD 2, FreeBSD 3, NetBSD post-1.5N
 *		need only struct ifnet * as argument
 *
 * - struct ifnet
 *			use queue.h?	member names	if name
 *			---		---		---
 *	FreeBSD 2	no		old standard	if_name+unit
 *	FreeBSD 3	yes		strange		if_name+unit
 *	OpenBSD		yes		standard	if_xname
 *	NetBSD		yes		standard	if_xname
 *	BSDI [34]	no		old standard	if_name+unit
 *
 * - usrreq
 *	NetBSD, OpenBSD, BSDI [34], FreeBSD 2
 *		single function with PRU_xx, arguments are mbuf
 *	FreeBSD 3
 *		separates functions, non-mbuf arguments
 *
 * - {set,get}sockopt
 *	NetBSD, OpenBSD, BSDI [34], FreeBSD 2
 *		manipulation based on mbuf
 *	FreeBSD 3
 *		non-mbuf manipulation using sooptcopy{in,out}()
 *
 * - timeout() and untimeout()
 *	NetBSD 1.4.x, OpenBSD, BSDI [34], FreeBSD 2
 *		timeout() is a void function
 *	FreeBSD 3
 *		timeout() is non-void, must keep returned value for untimeout()
 *		callout_xx is also available (sys/callout.h)
 *	NetBSD 1.5
 *		timeout() is obsoleted, use callout_xx (sys/callout.h)
 *	OpenBSD 2.8
 *		timeout_{add,set,del} is encouraged (sys/timeout.h)
 *
 * - kernel internal time structure
 *	FreeBSD 2, NetBSD, OpenBSD, BSD/OS
 *		mono_time.tv_u?sec, time.tv_u?sec
 *	FreeBSD [34]
 *		time_second
 *	if you need portability, #ifdef out FreeBSD[34], or use microtime(&tv)
 *	then touch tv.tv_sec (note: microtime is an expensive operation).
 *
 * - sysctl
 *	NetBSD, OpenBSD
 *		foo_sysctl()
 *	BSDI [34]
 *		foo_sysctl() but with different style.  sysctl_int_arr() takes
 *		care of most of the cases.
 *	FreeBSD
 *		linker hack.  however, there are freebsd version differences
 *		(how wonderful!).
 *		on FreeBSD[23] function arg #define includes paren.
 *			int foo SYSCTL_HANDLER_ARGS;
 *		on FreeBSD4, function arg #define does not include paren.
 *			int foo(SYSCTL_HANDLER_ARGS);
 *		on some versions, forward reference to the tree is okay.
 *		on some versions, you need SYSCTL_DECL().  you need things
 *		like this.
 *			#ifdef SYSCTL_DECL
 *			SYSCTL_DECL(net_inet_ip6);
 *			#endif
 *		it is hard to share functions between freebsd and non-freebsd.
 *
 * - if_ioctl
 *	NetBSD, FreeBSD 3, BSDI [34]
 *		2nd argument is u_long cmd
 *	FreeBSD 2
 *		2nd argument is int cmd
 *
 * - if attach routines
 *	NetBSD
 *		void xxattach(int);
 *	FreeBSD 2, FreeBSD 3
 *		void xxattach(void *);
 *		PSEUDO_SET(xxattach, if_xx);
 *
 * - ovbcopy()
 *	in NetBSD 1.4 or later, ovbcopy() is not supplied in the kernel.
 *	we have updated sys/systm.h to include declaration.
 *
 * - splnet()
 *	NetBSD 1.4 or later requires splsoftnet().
 *	other operating systems use splnet().
 *
 * - splimp()
 *	NetBSD-current (2001/4/13): use splnet() in network, splvm() in vm.
 *	other operating systems: use splimp().
 *
 * - dtom()
 *	NEVER USE IT!
 *
 * - struct ifnet for loopback interface
 *	BSDI3: struct ifnet loif;
 *	BSDI4: struct ifnet *loifp;
 *	NetBSD, OpenBSD 2.8, FreeBSD2: struct ifnet loif[NLOOP];
 *	OpenBSD 2.9: struct ifnet *lo0ifp;
 *
 *	odd thing is that many of them refers loif as ifnet *loif,
 *	not loif[NLOOP], from outside of if_loop.c.
 *
 * - number of bpf pseudo devices
 *	others: bpfilter.h, NBPFILTER
 *	FreeBSD4: bpf.h, NBPF
 *	solution:
 *		#if defined(__FreeBSD__) && __FreeBSD__ >= 4
 *		#include "bpf.h"
 *		#define NBPFILTER	NBPF
 *		#else
 *		#include "bpfilter.h"
 *		#endif
 *
 * - protosw for IPv4 (sys/netinet)
 *	FreeBSD4: struct ipprotosw in netinet/ipprotosw.h
 *	others: struct protosw in sys/protosw.h
 *
 * - protosw in general.
 *	NetBSD 1.5 has extra member for ipfilter (netbsd-current dropped
 *	it so it will go away in 1.6).
 *	NetBSD 1.5 requires PR_LISTEN flag bit with protocols that permit
 *	listen/accept (like tcp).
 *
 * - header files with defopt (opt_xx.h)
 *	FreeBSD3: opt_{inet,ipsec,ip6fw,altq}.h
 *	FreeBSD4: opt_{inet,inet6,ipsec,ip6fw,altq}.h
 *	NetBSD: opt_{inet,ipsec,altq}.h
 *	others: does not use defopt
 *
 * - (m->m_flags & M_EXT) != 0 does *not* mean that the max data length of
 *   the mbuf == MCLBYTES.
 *
 * - sys/kern/uipc_mbuf.c:m_dup()
 *	freebsd[34]: copies the whole mbuf chain.
 *	netbsd: similar arg with m_copym().
 *	others: no m_dup().
 *
 * - ifa_refcnt (struct ifaddr) management (IFAREF/IFAFREE).
 *	NetBSD 1.5: always use IFAREF whenever reference gets added.
 *		always use IFAFREE whenever reference gets freed.
 *		IFAFREE frees ifaddr when ifa_refcnt reaches 0.
 *	others: do not increase refcnt for ifp->if_addrlist and in_ifaddr.
 *		use IFAFREE once when ifaddr is disconnected from
 *		ifp->if_addrlist and in_ifaddr.  IFAFREE frees ifaddr when
 *		ifa_refcnt goes negative.  in KAME environment, IFAREF is
 *		provided as a compatibility wrapper (use it instead of
 *		ifa_refcnt++ to reduce #ifdef).
 *
 * - ifnet.if_lastchange
 *	freebsd, bsdi, netbsd-current (jun 14 2001-),
 *	openbsd-current (jun 15 2001-): updated only when IFF_UP changes.
 *		(RFC1573 ifLastChange interpretation)
 *	netbsd151, openbsd29: updated whenever packets go through the interface.
 *		(4.4BSD interpretation)
 *
 * - kernel compilation options ("options HOGE" in kernel config file)
 *	freebsd4: sys/conf/options has to have mapping between option
 *		and a header file (opt_hoge.h).
 *	netbsd: by default, -DHOGE will go into
 *		sys/arch/foo/compile/BAR/Makefile.
 *		if you define mapping in sys/conf/files, you can create
 *		a header file like opt_hoge.h to help make dependencies.
 *	bsdi/openbsd: always use -DHOGE in Makefile.  there's no need/way
 *		to have opt_hoge.h.
 *
 *	therefore, opt_hoge.h is mandatory on freebsd4 only.
 *
 * - MALLOC() macro
 *	Use it only if the size of the allocation is constant.
 *	When we do NOT collect statistics about kernel memory usage, the result
 *	of macro expansion contains a large set of condition branches.  If the
 *	size is not constant, compilation optimization cannot be applied, and
 *	a bunch of the large branch will be embedded in the kernel code.
 *
 * - M_COPY_PKTHDR
 *	openbsd30: M_COPY_PKTHDR is deprecated.  use M_MOVE_PKTHDR or
 *		M_DUP_PKTHDR, depending on how you want to handle m_tag.
 *	others: M_COPY_PKTHDR is available as usual.
 */

#ifndef __NET_NET_OSDEP_H_DEFINED_
#define __NET_NET_OSDEP_H_DEFINED_
#ifdef _KERNEL

struct ifnet;
extern const char *if_name __P((struct ifnet *));

#define HAVE_OLD_BPF

#define ifa_list	ifa_link
#define if_addrlist	if_addrhead
#define if_list		if_link

/* sys/net/if.h */
#define IFAREF(ifa)	do { ++(ifa)->ifa_refcnt; } while (0)

#define WITH_CONVERT_AND_STRIP_IP_LEN

#if 1				/* at this moment, all OSes do this */
#define WITH_CONVERT_IP_OFF
#endif

#endif /*_KERNEL*/
#endif /*__NET_NET_OSDEP_H_DEFINED_ */
