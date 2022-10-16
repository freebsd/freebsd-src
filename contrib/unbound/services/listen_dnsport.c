/*
 * services/listen_dnsport.c - listen on port 53 for incoming DNS queries.
 *
 * Copyright (c) 2007, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 *
 * This file has functions to get queries from clients.
 */
#include "config.h"
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include <sys/time.h>
#include <limits.h>
#ifdef USE_TCP_FASTOPEN
#include <netinet/tcp.h>
#endif
#include <ctype.h>
#include "services/listen_dnsport.h"
#include "services/outside_network.h"
#include "util/netevent.h"
#include "util/log.h"
#include "util/config_file.h"
#include "util/net_help.h"
#include "sldns/sbuffer.h"
#include "sldns/parseutil.h"
#include "services/mesh.h"
#include "util/fptr_wlist.h"
#include "util/locks.h"

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#include <fcntl.h>

#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif

/** number of queued TCP connections for listen() */
#define TCP_BACKLOG 256 

#ifndef THREADS_DISABLED
/** lock on the counter of stream buffer memory */
static lock_basic_type stream_wait_count_lock;
/** lock on the counter of HTTP2 query buffer memory */
static lock_basic_type http2_query_buffer_count_lock;
/** lock on the counter of HTTP2 response buffer memory */
static lock_basic_type http2_response_buffer_count_lock;
#endif
/** size (in bytes) of stream wait buffers */
static size_t stream_wait_count = 0;
/** is the lock initialised for stream wait buffers */
static int stream_wait_lock_inited = 0;
/** size (in bytes) of HTTP2 query buffers */
static size_t http2_query_buffer_count = 0;
/** is the lock initialised for HTTP2 query buffers */
static int http2_query_buffer_lock_inited = 0;
/** size (in bytes) of HTTP2 response buffers */
static size_t http2_response_buffer_count = 0;
/** is the lock initialised for HTTP2 response buffers */
static int http2_response_buffer_lock_inited = 0;

/**
 * Debug print of the getaddrinfo returned address.
 * @param addr: the address returned.
 */
static void
verbose_print_addr(struct addrinfo *addr)
{
	if(verbosity >= VERB_ALGO) {
		char buf[100];
		void* sinaddr = &((struct sockaddr_in*)addr->ai_addr)->sin_addr;
#ifdef INET6
		if(addr->ai_family == AF_INET6)
			sinaddr = &((struct sockaddr_in6*)addr->ai_addr)->
				sin6_addr;
#endif /* INET6 */
		if(inet_ntop(addr->ai_family, sinaddr, buf,
			(socklen_t)sizeof(buf)) == 0) {
			(void)strlcpy(buf, "(null)", sizeof(buf));
		}
		buf[sizeof(buf)-1] = 0;
		verbose(VERB_ALGO, "creating %s%s socket %s %d", 
			addr->ai_socktype==SOCK_DGRAM?"udp":
			addr->ai_socktype==SOCK_STREAM?"tcp":"otherproto",
			addr->ai_family==AF_INET?"4":
			addr->ai_family==AF_INET6?"6":
			"_otherfam", buf, 
			ntohs(((struct sockaddr_in*)addr->ai_addr)->sin_port));
	}
}

void
verbose_print_unbound_socket(struct unbound_socket* ub_sock)
{
	if(verbosity >= VERB_ALGO) {
		log_info("listing of unbound_socket structure:");
		verbose_print_addr(ub_sock->addr);
		log_info("s is: %d, fam is: %s", ub_sock->s, ub_sock->fam == AF_INET?"AF_INET":"AF_INET6");
	}
}

#ifdef HAVE_SYSTEMD
static int
systemd_get_activated(int family, int socktype, int listen,
		      struct sockaddr *addr, socklen_t addrlen,
		      const char *path)
{
	int i = 0;
	int r = 0;
	int s = -1;
	const char* listen_pid, *listen_fds;

	/* We should use "listen" option only for stream protocols. For UDP it should be -1 */

	if((r = sd_booted()) < 1) {
		if(r == 0)
			log_warn("systemd is not running");
		else
			log_err("systemd sd_booted(): %s", strerror(-r));
		return -1;
	}

	listen_pid = getenv("LISTEN_PID");
	listen_fds = getenv("LISTEN_FDS");

	if (!listen_pid) {
		log_warn("Systemd mandatory ENV variable is not defined: LISTEN_PID");
		return -1;
	}

	if (!listen_fds) {
		log_warn("Systemd mandatory ENV variable is not defined: LISTEN_FDS");
		return -1;
	}

	if((r = sd_listen_fds(0)) < 1) {
		if(r == 0)
			log_warn("systemd: did not return socket, check unit configuration");
		else
			log_err("systemd sd_listen_fds(): %s", strerror(-r));
		return -1;
	}
	
	for(i = 0; i < r; i++) {
		if(sd_is_socket(SD_LISTEN_FDS_START + i, family, socktype, listen)) {
			s = SD_LISTEN_FDS_START + i;
			break;
		}
	}
	if (s == -1) {
		if (addr)
			log_err_addr("systemd sd_listen_fds()",
				     "no such socket",
				     (struct sockaddr_storage *)addr, addrlen);
		else
			log_err("systemd sd_listen_fds(): %s", path);
	}
	return s;
}
#endif

int
create_udp_sock(int family, int socktype, struct sockaddr* addr,
        socklen_t addrlen, int v6only, int* inuse, int* noproto,
	int rcv, int snd, int listen, int* reuseport, int transparent,
	int freebind, int use_systemd, int dscp)
{
	int s;
	char* err;
#if defined(SO_REUSEADDR) || defined(SO_REUSEPORT) || defined(IPV6_USE_MIN_MTU)  || defined(IP_TRANSPARENT) || defined(IP_BINDANY) || defined(IP_FREEBIND) || defined (SO_BINDANY)
	int on=1;
#endif
#ifdef IPV6_MTU
	int mtu = IPV6_MIN_MTU;
#endif
#if !defined(SO_RCVBUFFORCE) && !defined(SO_RCVBUF)
	(void)rcv;
#endif
#if !defined(SO_SNDBUFFORCE) && !defined(SO_SNDBUF)
	(void)snd;
#endif
#ifndef IPV6_V6ONLY
	(void)v6only;
#endif
#if !defined(IP_TRANSPARENT) && !defined(IP_BINDANY) && !defined(SO_BINDANY)
	(void)transparent;
#endif
#if !defined(IP_FREEBIND)
	(void)freebind;
#endif
#ifdef HAVE_SYSTEMD
	int got_fd_from_systemd = 0;

	if (!use_systemd
	    || (use_systemd
		&& (s = systemd_get_activated(family, socktype, -1, addr,
					      addrlen, NULL)) == -1)) {
#else
	(void)use_systemd;
#endif
	if((s = socket(family, socktype, 0)) == -1) {
		*inuse = 0;
#ifndef USE_WINSOCK
		if(errno == EAFNOSUPPORT || errno == EPROTONOSUPPORT) {
			*noproto = 1;
			return -1;
		}
#else
		if(WSAGetLastError() == WSAEAFNOSUPPORT || 
			WSAGetLastError() == WSAEPROTONOSUPPORT) {
			*noproto = 1;
			return -1;
		}
#endif
		log_err("can't create socket: %s", sock_strerror(errno));
		*noproto = 0;
		return -1;
	}
#ifdef HAVE_SYSTEMD
	} else {
		got_fd_from_systemd = 1;
	}
#endif
	if(listen) {
#ifdef SO_REUSEADDR
		if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (void*)&on, 
			(socklen_t)sizeof(on)) < 0) {
			log_err("setsockopt(.. SO_REUSEADDR ..) failed: %s",
				sock_strerror(errno));
#ifndef USE_WINSOCK
			if(errno != ENOSYS) {
				close(s);
				*noproto = 0;
				*inuse = 0;
				return -1;
			}
#else
			closesocket(s);
			*noproto = 0;
			*inuse = 0;
			return -1;
#endif
		}
#endif /* SO_REUSEADDR */
#ifdef SO_REUSEPORT
#  ifdef SO_REUSEPORT_LB
		/* on FreeBSD 12 we have SO_REUSEPORT_LB that does loadbalance
		 * like SO_REUSEPORT on Linux.  This is what the users want
		 * with the config option in unbound.conf; if we actually
		 * need local address and port reuse they'll also need to
		 * have SO_REUSEPORT set for them, assume it was _LB they want.
		 */
		if (reuseport && *reuseport &&
		    setsockopt(s, SOL_SOCKET, SO_REUSEPORT_LB, (void*)&on,
			(socklen_t)sizeof(on)) < 0) {
#ifdef ENOPROTOOPT
			if(errno != ENOPROTOOPT || verbosity >= 3)
				log_warn("setsockopt(.. SO_REUSEPORT_LB ..) failed: %s",
					strerror(errno));
#endif
			/* this option is not essential, we can continue */
			*reuseport = 0;
		}
#  else /* no SO_REUSEPORT_LB */

		/* try to set SO_REUSEPORT so that incoming
		 * queries are distributed evenly among the receiving threads.
		 * Each thread must have its own socket bound to the same port,
		 * with SO_REUSEPORT set on each socket.
		 */
		if (reuseport && *reuseport &&
		    setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (void*)&on,
			(socklen_t)sizeof(on)) < 0) {
#ifdef ENOPROTOOPT
			if(errno != ENOPROTOOPT || verbosity >= 3)
				log_warn("setsockopt(.. SO_REUSEPORT ..) failed: %s",
					strerror(errno));
#endif
			/* this option is not essential, we can continue */
			*reuseport = 0;
		}
#  endif /* SO_REUSEPORT_LB */
#else
		(void)reuseport;
#endif /* defined(SO_REUSEPORT) */
#ifdef IP_TRANSPARENT
		if (transparent &&
		    setsockopt(s, IPPROTO_IP, IP_TRANSPARENT, (void*)&on,
		    (socklen_t)sizeof(on)) < 0) {
			log_warn("setsockopt(.. IP_TRANSPARENT ..) failed: %s",
			strerror(errno));
		}
#elif defined(IP_BINDANY)
		if (transparent &&
		    setsockopt(s, (family==AF_INET6? IPPROTO_IPV6:IPPROTO_IP),
		    (family == AF_INET6? IPV6_BINDANY:IP_BINDANY),
		    (void*)&on, (socklen_t)sizeof(on)) < 0) {
			log_warn("setsockopt(.. IP%s_BINDANY ..) failed: %s",
			(family==AF_INET6?"V6":""), strerror(errno));
		}
#elif defined(SO_BINDANY)
		if (transparent &&
		    setsockopt(s, SOL_SOCKET, SO_BINDANY, (void*)&on,
		    (socklen_t)sizeof(on)) < 0) {
			log_warn("setsockopt(.. SO_BINDANY ..) failed: %s",
			strerror(errno));
		}
#endif /* IP_TRANSPARENT || IP_BINDANY || SO_BINDANY */
	}
#ifdef IP_FREEBIND
	if(freebind &&
	    setsockopt(s, IPPROTO_IP, IP_FREEBIND, (void*)&on,
	    (socklen_t)sizeof(on)) < 0) {
		log_warn("setsockopt(.. IP_FREEBIND ..) failed: %s",
		strerror(errno));
	}
#endif /* IP_FREEBIND */
	if(rcv) {
#ifdef SO_RCVBUF
		int got;
		socklen_t slen = (socklen_t)sizeof(got);
#  ifdef SO_RCVBUFFORCE
		/* Linux specific: try to use root permission to override
		 * system limits on rcvbuf. The limit is stored in 
		 * /proc/sys/net/core/rmem_max or sysctl net.core.rmem_max */
		if(setsockopt(s, SOL_SOCKET, SO_RCVBUFFORCE, (void*)&rcv, 
			(socklen_t)sizeof(rcv)) < 0) {
			if(errno != EPERM) {
				log_err("setsockopt(..., SO_RCVBUFFORCE, "
					"...) failed: %s", sock_strerror(errno));
				sock_close(s);
				*noproto = 0;
				*inuse = 0;
				return -1;
			}
#  endif /* SO_RCVBUFFORCE */
			if(setsockopt(s, SOL_SOCKET, SO_RCVBUF, (void*)&rcv, 
				(socklen_t)sizeof(rcv)) < 0) {
				log_err("setsockopt(..., SO_RCVBUF, "
					"...) failed: %s", sock_strerror(errno));
				sock_close(s);
				*noproto = 0;
				*inuse = 0;
				return -1;
			}
			/* check if we got the right thing or if system
			 * reduced to some system max.  Warn if so */
			if(getsockopt(s, SOL_SOCKET, SO_RCVBUF, (void*)&got, 
				&slen) >= 0 && got < rcv/2) {
				log_warn("so-rcvbuf %u was not granted. "
					"Got %u. To fix: start with "
					"root permissions(linux) or sysctl "
					"bigger net.core.rmem_max(linux) or "
					"kern.ipc.maxsockbuf(bsd) values.",
					(unsigned)rcv, (unsigned)got);
			}
#  ifdef SO_RCVBUFFORCE
		}
#  endif
#endif /* SO_RCVBUF */
	}
	/* first do RCVBUF as the receive buffer is more important */
	if(snd) {
#ifdef SO_SNDBUF
		int got;
		socklen_t slen = (socklen_t)sizeof(got);
#  ifdef SO_SNDBUFFORCE
		/* Linux specific: try to use root permission to override
		 * system limits on sndbuf. The limit is stored in 
		 * /proc/sys/net/core/wmem_max or sysctl net.core.wmem_max */
		if(setsockopt(s, SOL_SOCKET, SO_SNDBUFFORCE, (void*)&snd, 
			(socklen_t)sizeof(snd)) < 0) {
			if(errno != EPERM) {
				log_err("setsockopt(..., SO_SNDBUFFORCE, "
					"...) failed: %s", sock_strerror(errno));
				sock_close(s);
				*noproto = 0;
				*inuse = 0;
				return -1;
			}
#  endif /* SO_SNDBUFFORCE */
			if(setsockopt(s, SOL_SOCKET, SO_SNDBUF, (void*)&snd, 
				(socklen_t)sizeof(snd)) < 0) {
				log_err("setsockopt(..., SO_SNDBUF, "
					"...) failed: %s", sock_strerror(errno));
				sock_close(s);
				*noproto = 0;
				*inuse = 0;
				return -1;
			}
			/* check if we got the right thing or if system
			 * reduced to some system max.  Warn if so */
			if(getsockopt(s, SOL_SOCKET, SO_SNDBUF, (void*)&got, 
				&slen) >= 0 && got < snd/2) {
				log_warn("so-sndbuf %u was not granted. "
					"Got %u. To fix: start with "
					"root permissions(linux) or sysctl "
					"bigger net.core.wmem_max(linux) or "
					"kern.ipc.maxsockbuf(bsd) values.",
					(unsigned)snd, (unsigned)got);
			}
#  ifdef SO_SNDBUFFORCE
		}
#  endif
#endif /* SO_SNDBUF */
	}
	err = set_ip_dscp(s, family, dscp);
	if(err != NULL)
		log_warn("error setting IP DiffServ codepoint %d on UDP socket: %s", dscp, err);
	if(family == AF_INET6) {
# if defined(IPV6_MTU_DISCOVER) && defined(IP_PMTUDISC_DONT)
		int omit6_set = 0;
		int action;
# endif
# if defined(IPV6_V6ONLY)
		if(v6only) {
			int val=(v6only==2)?0:1;
			if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, 
				(void*)&val, (socklen_t)sizeof(val)) < 0) {
				log_err("setsockopt(..., IPV6_V6ONLY"
					", ...) failed: %s", sock_strerror(errno));
				sock_close(s);
				*noproto = 0;
				*inuse = 0;
				return -1;
			}
		}
# endif
# if defined(IPV6_USE_MIN_MTU)
		/*
		 * There is no fragmentation of IPv6 datagrams
		 * during forwarding in the network. Therefore
		 * we do not send UDP datagrams larger than
		 * the minimum IPv6 MTU of 1280 octets. The
		 * EDNS0 message length can be larger if the
		 * network stack supports IPV6_USE_MIN_MTU.
		 */
		if (setsockopt(s, IPPROTO_IPV6, IPV6_USE_MIN_MTU,
			(void*)&on, (socklen_t)sizeof(on)) < 0) {
			log_err("setsockopt(..., IPV6_USE_MIN_MTU, "
				"...) failed: %s", sock_strerror(errno));
			sock_close(s);
			*noproto = 0;
			*inuse = 0;
			return -1;
		}
# elif defined(IPV6_MTU)
#   ifndef USE_WINSOCK
		/*
		 * On Linux, to send no larger than 1280, the PMTUD is
		 * disabled by default for datagrams anyway, so we set
		 * the MTU to use.
		 */
		if (setsockopt(s, IPPROTO_IPV6, IPV6_MTU,
			(void*)&mtu, (socklen_t)sizeof(mtu)) < 0) {
			log_err("setsockopt(..., IPV6_MTU, ...) failed: %s",
				sock_strerror(errno));
			sock_close(s);
			*noproto = 0;
			*inuse = 0;
			return -1;
		}
#   elif defined(IPV6_USER_MTU)
		/* As later versions of the mingw crosscompiler define
		 * IPV6_MTU, do the same for windows but use IPV6_USER_MTU
		 * instead which is writable; IPV6_MTU is readonly there. */
		if (setsockopt(s, IPPROTO_IPV6, IPV6_USER_MTU,
			(void*)&mtu, (socklen_t)sizeof(mtu)) < 0) {
			log_err("setsockopt(..., IPV6_USER_MTU, ...) failed: %s",
				wsa_strerror(WSAGetLastError()));
			sock_close(s);
			*noproto = 0;
			*inuse = 0;
			return -1;
		}
#   endif /* USE_WINSOCK */
# endif /* IPv6 MTU */
# if defined(IPV6_MTU_DISCOVER) && defined(IP_PMTUDISC_DONT)
#  if defined(IP_PMTUDISC_OMIT)
		action = IP_PMTUDISC_OMIT;
		if (setsockopt(s, IPPROTO_IPV6, IPV6_MTU_DISCOVER,
			&action, (socklen_t)sizeof(action)) < 0) {

			if (errno != EINVAL) {
				log_err("setsockopt(..., IPV6_MTU_DISCOVER, IP_PMTUDISC_OMIT...) failed: %s",
					strerror(errno));
				sock_close(s);
				*noproto = 0;
				*inuse = 0;
				return -1;
			}
		}
		else
		{
		    omit6_set = 1;
		}
#  endif
		if (omit6_set == 0) {
			action = IP_PMTUDISC_DONT;
			if (setsockopt(s, IPPROTO_IPV6, IPV6_MTU_DISCOVER,
				&action, (socklen_t)sizeof(action)) < 0) {
				log_err("setsockopt(..., IPV6_MTU_DISCOVER, IP_PMTUDISC_DONT...) failed: %s",
					strerror(errno));
				sock_close(s);
				*noproto = 0;
				*inuse = 0;
				return -1;
			}
		}
# endif /* IPV6_MTU_DISCOVER */
	} else if(family == AF_INET) {
#  if defined(IP_MTU_DISCOVER) && defined(IP_PMTUDISC_DONT)
/* linux 3.15 has IP_PMTUDISC_OMIT, Hannes Frederic Sowa made it so that
 * PMTU information is not accepted, but fragmentation is allowed
 * if and only if the packet size exceeds the outgoing interface MTU
 * (and also uses the interface mtu to determine the size of the packets).
 * So there won't be any EMSGSIZE error.  Against DNS fragmentation attacks.
 * FreeBSD already has same semantics without setting the option. */
		int omit_set = 0;
		int action;
#   if defined(IP_PMTUDISC_OMIT)
		action = IP_PMTUDISC_OMIT;
		if (setsockopt(s, IPPROTO_IP, IP_MTU_DISCOVER, 
			&action, (socklen_t)sizeof(action)) < 0) {

			if (errno != EINVAL) {
				log_err("setsockopt(..., IP_MTU_DISCOVER, IP_PMTUDISC_OMIT...) failed: %s",
					strerror(errno));
				sock_close(s);
				*noproto = 0;
				*inuse = 0;
				return -1;
			}
		}
		else
		{
		    omit_set = 1;
		}
#   endif
		if (omit_set == 0) {
   			action = IP_PMTUDISC_DONT;
			if (setsockopt(s, IPPROTO_IP, IP_MTU_DISCOVER,
				&action, (socklen_t)sizeof(action)) < 0) {
				log_err("setsockopt(..., IP_MTU_DISCOVER, IP_PMTUDISC_DONT...) failed: %s",
					strerror(errno));
				sock_close(s);
				*noproto = 0;
				*inuse = 0;
				return -1;
			}
		}
#  elif defined(IP_DONTFRAG) && !defined(__APPLE__)
		/* the IP_DONTFRAG option if defined in the 11.0 OSX headers,
		 * but does not work on that version, so we exclude it */
		int off = 0;
		if (setsockopt(s, IPPROTO_IP, IP_DONTFRAG, 
			&off, (socklen_t)sizeof(off)) < 0) {
			log_err("setsockopt(..., IP_DONTFRAG, ...) failed: %s",
				strerror(errno));
			sock_close(s);
			*noproto = 0;
			*inuse = 0;
			return -1;
		}
#  endif /* IPv4 MTU */
	}
	if(
#ifdef HAVE_SYSTEMD
		!got_fd_from_systemd &&
#endif
		bind(s, (struct sockaddr*)addr, addrlen) != 0) {
		*noproto = 0;
		*inuse = 0;
#ifndef USE_WINSOCK
#ifdef EADDRINUSE
		*inuse = (errno == EADDRINUSE);
		/* detect freebsd jail with no ipv6 permission */
		if(family==AF_INET6 && errno==EINVAL)
			*noproto = 1;
		else if(errno != EADDRINUSE &&
			!(errno == EACCES && verbosity < 4 && !listen)
#ifdef EADDRNOTAVAIL
			&& !(errno == EADDRNOTAVAIL && verbosity < 4 && !listen)
#endif
			) {
			log_err_addr("can't bind socket", strerror(errno),
				(struct sockaddr_storage*)addr, addrlen);
		}
#endif /* EADDRINUSE */
#else /* USE_WINSOCK */
		if(WSAGetLastError() != WSAEADDRINUSE &&
			WSAGetLastError() != WSAEADDRNOTAVAIL &&
			!(WSAGetLastError() == WSAEACCES && verbosity < 4 && !listen)) {
			log_err_addr("can't bind socket", 
				wsa_strerror(WSAGetLastError()),
				(struct sockaddr_storage*)addr, addrlen);
		}
#endif /* USE_WINSOCK */
		sock_close(s);
		return -1;
	}
	if(!fd_set_nonblock(s)) {
		*noproto = 0;
		*inuse = 0;
		sock_close(s);
		return -1;
	}
	return s;
}

int
create_tcp_accept_sock(struct addrinfo *addr, int v6only, int* noproto,
	int* reuseport, int transparent, int mss, int nodelay, int freebind,
	int use_systemd, int dscp)
{
	int s;
	char* err;
#if defined(SO_REUSEADDR) || defined(SO_REUSEPORT) || defined(IPV6_V6ONLY) || defined(IP_TRANSPARENT) || defined(IP_BINDANY) || defined(IP_FREEBIND) || defined(SO_BINDANY)
	int on = 1;
#endif
#ifdef HAVE_SYSTEMD
	int got_fd_from_systemd = 0;
#endif
#ifdef USE_TCP_FASTOPEN
	int qlen;
#endif
#if !defined(IP_TRANSPARENT) && !defined(IP_BINDANY) && !defined(SO_BINDANY)
	(void)transparent;
#endif
#if !defined(IP_FREEBIND)
	(void)freebind;
#endif
	verbose_print_addr(addr);
	*noproto = 0;
#ifdef HAVE_SYSTEMD
	if (!use_systemd ||
	    (use_systemd
	     && (s = systemd_get_activated(addr->ai_family, addr->ai_socktype, 1,
					   addr->ai_addr, addr->ai_addrlen,
					   NULL)) == -1)) {
#else
	(void)use_systemd;
#endif
	if((s = socket(addr->ai_family, addr->ai_socktype, 0)) == -1) {
#ifndef USE_WINSOCK
		if(errno == EAFNOSUPPORT || errno == EPROTONOSUPPORT) {
			*noproto = 1;
			return -1;
		}
#else
		if(WSAGetLastError() == WSAEAFNOSUPPORT ||
			WSAGetLastError() == WSAEPROTONOSUPPORT) {
			*noproto = 1;
			return -1;
		}
#endif
		log_err("can't create socket: %s", sock_strerror(errno));
		return -1;
	}
	if(nodelay) {
#if defined(IPPROTO_TCP) && defined(TCP_NODELAY)
		if(setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (void*)&on,
			(socklen_t)sizeof(on)) < 0) {
			#ifndef USE_WINSOCK
			log_err(" setsockopt(.. TCP_NODELAY ..) failed: %s",
				strerror(errno));
			#else
			log_err(" setsockopt(.. TCP_NODELAY ..) failed: %s",
				wsa_strerror(WSAGetLastError()));
			#endif
		}
#else
		log_warn(" setsockopt(TCP_NODELAY) unsupported");
#endif /* defined(IPPROTO_TCP) && defined(TCP_NODELAY) */
	}
	if (mss > 0) {
#if defined(IPPROTO_TCP) && defined(TCP_MAXSEG)
		if(setsockopt(s, IPPROTO_TCP, TCP_MAXSEG, (void*)&mss,
			(socklen_t)sizeof(mss)) < 0) {
			log_err(" setsockopt(.. TCP_MAXSEG ..) failed: %s",
				sock_strerror(errno));
		} else {
			verbose(VERB_ALGO,
				" tcp socket mss set to %d", mss);
		}
#else
		log_warn(" setsockopt(TCP_MAXSEG) unsupported");
#endif /* defined(IPPROTO_TCP) && defined(TCP_MAXSEG) */
	}
#ifdef HAVE_SYSTEMD
	} else {
		got_fd_from_systemd = 1;
    }
#endif
#ifdef SO_REUSEADDR
	if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (void*)&on, 
		(socklen_t)sizeof(on)) < 0) {
		log_err("setsockopt(.. SO_REUSEADDR ..) failed: %s",
			sock_strerror(errno));
		sock_close(s);
		return -1;
	}
#endif /* SO_REUSEADDR */
#ifdef IP_FREEBIND
	if (freebind && setsockopt(s, IPPROTO_IP, IP_FREEBIND, (void*)&on,
	    (socklen_t)sizeof(on)) < 0) {
		log_warn("setsockopt(.. IP_FREEBIND ..) failed: %s",
		strerror(errno));
	}
#endif /* IP_FREEBIND */
#ifdef SO_REUSEPORT
	/* try to set SO_REUSEPORT so that incoming
	 * connections are distributed evenly among the receiving threads.
	 * Each thread must have its own socket bound to the same port,
	 * with SO_REUSEPORT set on each socket.
	 */
	if (reuseport && *reuseport &&
		setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (void*)&on,
		(socklen_t)sizeof(on)) < 0) {
#ifdef ENOPROTOOPT
		if(errno != ENOPROTOOPT || verbosity >= 3)
			log_warn("setsockopt(.. SO_REUSEPORT ..) failed: %s",
				strerror(errno));
#endif
		/* this option is not essential, we can continue */
		*reuseport = 0;
	}
#else
	(void)reuseport;
#endif /* defined(SO_REUSEPORT) */
#if defined(IPV6_V6ONLY)
	if(addr->ai_family == AF_INET6 && v6only) {
		if(setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, 
			(void*)&on, (socklen_t)sizeof(on)) < 0) {
			log_err("setsockopt(..., IPV6_V6ONLY, ...) failed: %s",
				sock_strerror(errno));
			sock_close(s);
			return -1;
		}
	}
#else
	(void)v6only;
#endif /* IPV6_V6ONLY */
#ifdef IP_TRANSPARENT
	if (transparent &&
	    setsockopt(s, IPPROTO_IP, IP_TRANSPARENT, (void*)&on,
	    (socklen_t)sizeof(on)) < 0) {
		log_warn("setsockopt(.. IP_TRANSPARENT ..) failed: %s",
			strerror(errno));
	}
#elif defined(IP_BINDANY)
	if (transparent &&
	    setsockopt(s, (addr->ai_family==AF_INET6? IPPROTO_IPV6:IPPROTO_IP),
	    (addr->ai_family == AF_INET6? IPV6_BINDANY:IP_BINDANY),
	    (void*)&on, (socklen_t)sizeof(on)) < 0) {
		log_warn("setsockopt(.. IP%s_BINDANY ..) failed: %s",
		(addr->ai_family==AF_INET6?"V6":""), strerror(errno));
	}
#elif defined(SO_BINDANY)
	if (transparent &&
	    setsockopt(s, SOL_SOCKET, SO_BINDANY, (void*)&on, (socklen_t)
	    sizeof(on)) < 0) {
		log_warn("setsockopt(.. SO_BINDANY ..) failed: %s",
		strerror(errno));
	}
#endif /* IP_TRANSPARENT || IP_BINDANY || SO_BINDANY */
	err = set_ip_dscp(s, addr->ai_family, dscp);
	if(err != NULL)
		log_warn("error setting IP DiffServ codepoint %d on TCP socket: %s", dscp, err);
	if(
#ifdef HAVE_SYSTEMD
		!got_fd_from_systemd &&
#endif
        bind(s, addr->ai_addr, addr->ai_addrlen) != 0) {
#ifndef USE_WINSOCK
		/* detect freebsd jail with no ipv6 permission */
		if(addr->ai_family==AF_INET6 && errno==EINVAL)
			*noproto = 1;
		else {
			log_err_addr("can't bind socket", strerror(errno),
				(struct sockaddr_storage*)addr->ai_addr,
				addr->ai_addrlen);
		}
#else
		log_err_addr("can't bind socket", 
			wsa_strerror(WSAGetLastError()),
			(struct sockaddr_storage*)addr->ai_addr,
			addr->ai_addrlen);
#endif
		sock_close(s);
		return -1;
	}
	if(!fd_set_nonblock(s)) {
		sock_close(s);
		return -1;
	}
	if(listen(s, TCP_BACKLOG) == -1) {
		log_err("can't listen: %s", sock_strerror(errno));
		sock_close(s);
		return -1;
	}
#ifdef USE_TCP_FASTOPEN
	/* qlen specifies how many outstanding TFO requests to allow. Limit is a defense
	   against IP spoofing attacks as suggested in RFC7413 */
#ifdef __APPLE__
	/* OS X implementation only supports qlen of 1 via this call. Actual
	   value is configured by the net.inet.tcp.fastopen_backlog kernel parm. */
	qlen = 1;
#else
	/* 5 is recommended on linux */
	qlen = 5;
#endif
	if ((setsockopt(s, IPPROTO_TCP, TCP_FASTOPEN, &qlen, 
		  sizeof(qlen))) == -1 ) {
#ifdef ENOPROTOOPT
		/* squelch ENOPROTOOPT: freebsd server mode with kernel support
		   disabled, except when verbosity enabled for debugging */
		if(errno != ENOPROTOOPT || verbosity >= 3) {
#endif
		  if(errno == EPERM) {
		  	log_warn("Setting TCP Fast Open as server failed: %s ; this could likely be because sysctl net.inet.tcp.fastopen.enabled, net.inet.tcp.fastopen.server_enable, or net.ipv4.tcp_fastopen is disabled", strerror(errno));
		  } else {
		  	log_err("Setting TCP Fast Open as server failed: %s", strerror(errno));
		  }
#ifdef ENOPROTOOPT
		}
#endif
	}
#endif
	return s;
}

char*
set_ip_dscp(int socket, int addrfamily, int dscp)
{
	int ds;

	if(dscp == 0)
		return NULL;
	ds = dscp << 2;
	switch(addrfamily) {
	case AF_INET6:
	#ifdef IPV6_TCLASS
		if(setsockopt(socket, IPPROTO_IPV6, IPV6_TCLASS, (void*)&ds,
			sizeof(ds)) < 0)
			return sock_strerror(errno);
		break;
	#else
		return "IPV6_TCLASS not defined on this system";
	#endif
	default:
		if(setsockopt(socket, IPPROTO_IP, IP_TOS, (void*)&ds, sizeof(ds)) < 0)
			return sock_strerror(errno);
		break;
	}
	return NULL;
}

int
create_local_accept_sock(const char *path, int* noproto, int use_systemd)
{
#ifdef HAVE_SYSTEMD
	int ret;

	if (use_systemd && (ret = systemd_get_activated(AF_LOCAL, SOCK_STREAM, 1, NULL, 0, path)) != -1)
		return ret;
	else {
#endif
#ifdef HAVE_SYS_UN_H
	int s;
	struct sockaddr_un usock;
#ifndef HAVE_SYSTEMD
	(void)use_systemd;
#endif

	verbose(VERB_ALGO, "creating unix socket %s", path);
#ifdef HAVE_STRUCT_SOCKADDR_UN_SUN_LEN
	/* this member exists on BSDs, not Linux */
	usock.sun_len = (unsigned)sizeof(usock);
#endif
	usock.sun_family = AF_LOCAL;
	/* length is 92-108, 104 on FreeBSD */
	(void)strlcpy(usock.sun_path, path, sizeof(usock.sun_path));

	if ((s = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1) {
		log_err("Cannot create local socket %s (%s)",
			path, strerror(errno));
		return -1;
	}

	if (unlink(path) && errno != ENOENT) {
		/* The socket already exists and cannot be removed */
		log_err("Cannot remove old local socket %s (%s)",
			path, strerror(errno));
		goto err;
	}

	if (bind(s, (struct sockaddr *)&usock,
		(socklen_t)sizeof(struct sockaddr_un)) == -1) {
		log_err("Cannot bind local socket %s (%s)",
			path, strerror(errno));
		goto err;
	}

	if (!fd_set_nonblock(s)) {
		log_err("Cannot set non-blocking mode");
		goto err;
	}

	if (listen(s, TCP_BACKLOG) == -1) {
		log_err("can't listen: %s", strerror(errno));
		goto err;
	}

	(void)noproto; /*unused*/
	return s;

err:
	sock_close(s);
	return -1;

#ifdef HAVE_SYSTEMD
	}
#endif
#else
	(void)use_systemd;
	(void)path;
	log_err("Local sockets are not supported");
	*noproto = 1;
	return -1;
#endif
}


/**
 * Create socket from getaddrinfo results
 */
static int
make_sock(int stype, const char* ifname, const char* port, 
	struct addrinfo *hints, int v6only, int* noip6, size_t rcv, size_t snd,
	int* reuseport, int transparent, int tcp_mss, int nodelay, int freebind,
	int use_systemd, int dscp, struct unbound_socket* ub_sock)
{
	struct addrinfo *res = NULL;
	int r, s, inuse, noproto;
	hints->ai_socktype = stype;
	*noip6 = 0;
	if((r=getaddrinfo(ifname, port, hints, &res)) != 0 || !res) {
#ifdef USE_WINSOCK
		if(r == EAI_NONAME && hints->ai_family == AF_INET6){
			*noip6 = 1; /* 'Host not found' for IP6 on winXP */
			return -1;
		}
#endif
		log_err("node %s:%s getaddrinfo: %s %s", 
			ifname?ifname:"default", port, gai_strerror(r),
#ifdef EAI_SYSTEM
			r==EAI_SYSTEM?(char*)strerror(errno):""
#else
			""
#endif
		);
		return -1;
	}
	if(stype == SOCK_DGRAM) {
		verbose_print_addr(res);
		s = create_udp_sock(res->ai_family, res->ai_socktype,
			(struct sockaddr*)res->ai_addr, res->ai_addrlen,
			v6only, &inuse, &noproto, (int)rcv, (int)snd, 1,
			reuseport, transparent, freebind, use_systemd, dscp);
		if(s == -1 && inuse) {
			log_err("bind: address already in use");
		} else if(s == -1 && noproto && hints->ai_family == AF_INET6){
			*noip6 = 1;
		}
	} else	{
		s = create_tcp_accept_sock(res, v6only, &noproto, reuseport,
			transparent, tcp_mss, nodelay, freebind, use_systemd,
			dscp);
		if(s == -1 && noproto && hints->ai_family == AF_INET6){
			*noip6 = 1;
		}
	}

	ub_sock->addr = res;
	ub_sock->s = s;
	ub_sock->fam = hints->ai_family;

	return s;
}

/** make socket and first see if ifname contains port override info */
static int
make_sock_port(int stype, const char* ifname, const char* port, 
	struct addrinfo *hints, int v6only, int* noip6, size_t rcv, size_t snd,
	int* reuseport, int transparent, int tcp_mss, int nodelay, int freebind,
	int use_systemd, int dscp, struct unbound_socket* ub_sock)
{
	char* s = strchr(ifname, '@');
	if(s) {
		/* override port with ifspec@port */
		char p[16];
		char newif[128];
		if((size_t)(s-ifname) >= sizeof(newif)) {
			log_err("ifname too long: %s", ifname);
			*noip6 = 0;
			return -1;
		}
		if(strlen(s+1) >= sizeof(p)) {
			log_err("portnumber too long: %s", ifname);
			*noip6 = 0;
			return -1;
		}
		(void)strlcpy(newif, ifname, sizeof(newif));
		newif[s-ifname] = 0;
		(void)strlcpy(p, s+1, sizeof(p));
		p[strlen(s+1)]=0;
		return make_sock(stype, newif, p, hints, v6only, noip6, rcv,
			snd, reuseport, transparent, tcp_mss, nodelay, freebind,
			use_systemd, dscp, ub_sock);
	}
	return make_sock(stype, ifname, port, hints, v6only, noip6, rcv, snd,
		reuseport, transparent, tcp_mss, nodelay, freebind, use_systemd,
		dscp, ub_sock);
}

/**
 * Add port to open ports list.
 * @param list: list head. changed.
 * @param s: fd.
 * @param ftype: if fd is UDP.
 * @param ub_sock: socket with address.
 * @return false on failure. list in unchanged then.
 */
static int
port_insert(struct listen_port** list, int s, enum listen_type ftype, struct unbound_socket* ub_sock)
{
	struct listen_port* item = (struct listen_port*)malloc(
		sizeof(struct listen_port));
	if(!item)
		return 0;
	item->next = *list;
	item->fd = s;
	item->ftype = ftype;
	item->socket = ub_sock;
	*list = item;
	return 1;
}

/** set fd to receive source address packet info */
static int
set_recvpktinfo(int s, int family) 
{
#if defined(IPV6_RECVPKTINFO) || defined(IPV6_PKTINFO) || (defined(IP_RECVDSTADDR) && defined(IP_SENDSRCADDR)) || defined(IP_PKTINFO)
	int on = 1;
#else
	(void)s;
#endif
	if(family == AF_INET6) {
#           ifdef IPV6_RECVPKTINFO
		if(setsockopt(s, IPPROTO_IPV6, IPV6_RECVPKTINFO,
			(void*)&on, (socklen_t)sizeof(on)) < 0) {
			log_err("setsockopt(..., IPV6_RECVPKTINFO, ...) failed: %s",
				strerror(errno));
			return 0;
		}
#           elif defined(IPV6_PKTINFO)
		if(setsockopt(s, IPPROTO_IPV6, IPV6_PKTINFO,
			(void*)&on, (socklen_t)sizeof(on)) < 0) {
			log_err("setsockopt(..., IPV6_PKTINFO, ...) failed: %s",
				strerror(errno));
			return 0;
		}
#           else
		log_err("no IPV6_RECVPKTINFO and IPV6_PKTINFO options, please "
			"disable interface-automatic or do-ip6 in config");
		return 0;
#           endif /* defined IPV6_RECVPKTINFO */

	} else if(family == AF_INET) {
#           ifdef IP_PKTINFO
		if(setsockopt(s, IPPROTO_IP, IP_PKTINFO,
			(void*)&on, (socklen_t)sizeof(on)) < 0) {
			log_err("setsockopt(..., IP_PKTINFO, ...) failed: %s",
				strerror(errno));
			return 0;
		}
#           elif defined(IP_RECVDSTADDR) && defined(IP_SENDSRCADDR)
		if(setsockopt(s, IPPROTO_IP, IP_RECVDSTADDR,
			(void*)&on, (socklen_t)sizeof(on)) < 0) {
			log_err("setsockopt(..., IP_RECVDSTADDR, ...) failed: %s",
				strerror(errno));
			return 0;
		}
#           else
		log_err("no IP_SENDSRCADDR or IP_PKTINFO option, please disable "
			"interface-automatic or do-ip4 in config");
		return 0;
#           endif /* IP_PKTINFO */

	}
	return 1;
}

/** see if interface is ssl, its port number == the ssl port number */
static int
if_is_ssl(const char* ifname, const char* port, int ssl_port,
	struct config_strlist* tls_additional_port)
{
	struct config_strlist* s;
	char* p = strchr(ifname, '@');
	if(!p && atoi(port) == ssl_port)
		return 1;
	if(p && atoi(p+1) == ssl_port)
		return 1;
	for(s = tls_additional_port; s; s = s->next) {
		if(p && atoi(p+1) == atoi(s->str))
			return 1;
		if(!p && atoi(port) == atoi(s->str))
			return 1;
	}
	return 0;
}

/**
 * Helper for ports_open. Creates one interface (or NULL for default).
 * @param ifname: The interface ip address.
 * @param do_auto: use automatic interface detection.
 * 	If enabled, then ifname must be the wildcard name.
 * @param do_udp: if udp should be used.
 * @param do_tcp: if tcp should be used.
 * @param hints: for getaddrinfo. family and flags have to be set by caller.
 * @param port: Port number to use (as string).
 * @param list: list of open ports, appended to, changed to point to list head.
 * @param rcv: receive buffer size for UDP
 * @param snd: send buffer size for UDP
 * @param ssl_port: ssl service port number
 * @param tls_additional_port: list of additional ssl service port numbers.
 * @param https_port: DoH service port number
 * @param reuseport: try to set SO_REUSEPORT if nonNULL and true.
 * 	set to false on exit if reuseport failed due to no kernel support.
 * @param transparent: set IP_TRANSPARENT socket option.
 * @param tcp_mss: maximum segment size of tcp socket. default if zero.
 * @param freebind: set IP_FREEBIND socket option.
 * @param http2_nodelay: set TCP_NODELAY on HTTP/2 connection
 * @param use_systemd: if true, fetch sockets from systemd.
 * @param dnscrypt_port: dnscrypt service port number
 * @param dscp: DSCP to use.
 * @return: returns false on error.
 */
static int
ports_create_if(const char* ifname, int do_auto, int do_udp, int do_tcp, 
	struct addrinfo *hints, const char* port, struct listen_port** list,
	size_t rcv, size_t snd, int ssl_port,
	struct config_strlist* tls_additional_port, int https_port,
	int* reuseport, int transparent, int tcp_mss, int freebind,
	int http2_nodelay, int use_systemd, int dnscrypt_port, int dscp)
{
	int s, noip6=0;
	int is_https = if_is_https(ifname, port, https_port);
	int nodelay = is_https && http2_nodelay;
	struct unbound_socket* ub_sock;
#ifdef USE_DNSCRYPT
	int is_dnscrypt = ((strchr(ifname, '@') && 
			atoi(strchr(ifname, '@')+1) == dnscrypt_port) ||
			(!strchr(ifname, '@') && atoi(port) == dnscrypt_port));
#else
	int is_dnscrypt = 0;
	(void)dnscrypt_port;
#endif

	if(!do_udp && !do_tcp)
		return 0;

	if(do_auto) {
		ub_sock = calloc(1, sizeof(struct unbound_socket));
		if(!ub_sock)
			return 0;
		if((s = make_sock_port(SOCK_DGRAM, ifname, port, hints, 1, 
			&noip6, rcv, snd, reuseport, transparent,
			tcp_mss, nodelay, freebind, use_systemd, dscp, ub_sock)) == -1) {
			freeaddrinfo(ub_sock->addr);
			free(ub_sock);
			if(noip6) {
				log_warn("IPv6 protocol not available");
				return 1;
			}
			return 0;
		}
		/* getting source addr packet info is highly non-portable */
		if(!set_recvpktinfo(s, hints->ai_family)) {
			sock_close(s);
			freeaddrinfo(ub_sock->addr);
			free(ub_sock);
			return 0;
		}
		if(!port_insert(list, s,
		   is_dnscrypt?listen_type_udpancil_dnscrypt:listen_type_udpancil, ub_sock)) {
			sock_close(s);
			freeaddrinfo(ub_sock->addr);
			free(ub_sock);
			return 0;
		}
	} else if(do_udp) {
		ub_sock = calloc(1, sizeof(struct unbound_socket));
		if(!ub_sock)
			return 0;
		/* regular udp socket */
		if((s = make_sock_port(SOCK_DGRAM, ifname, port, hints, 1, 
			&noip6, rcv, snd, reuseport, transparent,
			tcp_mss, nodelay, freebind, use_systemd, dscp, ub_sock)) == -1) {
			freeaddrinfo(ub_sock->addr);
			free(ub_sock);
			if(noip6) {
				log_warn("IPv6 protocol not available");
				return 1;
			}
			return 0;
		}
		if(!port_insert(list, s,
		   is_dnscrypt?listen_type_udp_dnscrypt:listen_type_udp, ub_sock)) {
			sock_close(s);
			freeaddrinfo(ub_sock->addr);
			free(ub_sock);
			return 0;
		}
	}
	if(do_tcp) {
		int is_ssl = if_is_ssl(ifname, port, ssl_port,
			tls_additional_port);
		enum listen_type port_type;
		ub_sock = calloc(1, sizeof(struct unbound_socket));
		if(!ub_sock)
			return 0;
		if(is_ssl)
			port_type = listen_type_ssl;
		else if(is_https)
			port_type = listen_type_http;
		else if(is_dnscrypt)
			port_type = listen_type_tcp_dnscrypt;
		else
			port_type = listen_type_tcp;
		if((s = make_sock_port(SOCK_STREAM, ifname, port, hints, 1, 
			&noip6, 0, 0, reuseport, transparent, tcp_mss, nodelay,
			freebind, use_systemd, dscp, ub_sock)) == -1) {
			freeaddrinfo(ub_sock->addr);
			free(ub_sock);
			if(noip6) {
				/*log_warn("IPv6 protocol not available");*/
				return 1;
			}
			return 0;
		}
		if(is_ssl)
			verbose(VERB_ALGO, "setup TCP for SSL service");
		if(!port_insert(list, s, port_type, ub_sock)) {
			sock_close(s);
			freeaddrinfo(ub_sock->addr);
			free(ub_sock);
			return 0;
		}
	}
	return 1;
}

/** 
 * Add items to commpoint list in front.
 * @param c: commpoint to add.
 * @param front: listen struct.
 * @return: false on failure.
 */
static int
listen_cp_insert(struct comm_point* c, struct listen_dnsport* front)
{
	struct listen_list* item = (struct listen_list*)malloc(
		sizeof(struct listen_list));
	if(!item)
		return 0;
	item->com = c;
	item->next = front->cps;
	front->cps = item;
	return 1;
}

void listen_setup_locks(void)
{
	if(!stream_wait_lock_inited) {
		lock_basic_init(&stream_wait_count_lock);
		stream_wait_lock_inited = 1;
	}
	if(!http2_query_buffer_lock_inited) {
		lock_basic_init(&http2_query_buffer_count_lock);
		http2_query_buffer_lock_inited = 1;
	}
	if(!http2_response_buffer_lock_inited) {
		lock_basic_init(&http2_response_buffer_count_lock);
		http2_response_buffer_lock_inited = 1;
	}
}

void listen_desetup_locks(void)
{
	if(stream_wait_lock_inited) {
		stream_wait_lock_inited = 0;
		lock_basic_destroy(&stream_wait_count_lock);
	}
	if(http2_query_buffer_lock_inited) {
		http2_query_buffer_lock_inited = 0;
		lock_basic_destroy(&http2_query_buffer_count_lock);
	}
	if(http2_response_buffer_lock_inited) {
		http2_response_buffer_lock_inited = 0;
		lock_basic_destroy(&http2_response_buffer_count_lock);
	}
}

struct listen_dnsport* 
listen_create(struct comm_base* base, struct listen_port* ports,
	size_t bufsize, int tcp_accept_count, int tcp_idle_timeout,
	int harden_large_queries, uint32_t http_max_streams,
	char* http_endpoint, int http_notls, struct tcl_list* tcp_conn_limit,
	void* sslctx, struct dt_env* dtenv, comm_point_callback_type* cb,
	void *cb_arg)
{
	struct listen_dnsport* front = (struct listen_dnsport*)
		malloc(sizeof(struct listen_dnsport));
	if(!front)
		return NULL;
	front->cps = NULL;
	front->udp_buff = sldns_buffer_new(bufsize);
#ifdef USE_DNSCRYPT
	front->dnscrypt_udp_buff = NULL;
#endif
	if(!front->udp_buff) {
		free(front);
		return NULL;
	}

	/* create comm points as needed */
	while(ports) {
		struct comm_point* cp = NULL;
		if(ports->ftype == listen_type_udp ||
		   ports->ftype == listen_type_udp_dnscrypt) {
			cp = comm_point_create_udp(base, ports->fd,
				front->udp_buff, cb, cb_arg, ports->socket);
		} else if(ports->ftype == listen_type_tcp ||
				ports->ftype == listen_type_tcp_dnscrypt) {
			cp = comm_point_create_tcp(base, ports->fd,
				tcp_accept_count, tcp_idle_timeout,
				harden_large_queries, 0, NULL,
				tcp_conn_limit, bufsize, front->udp_buff,
				ports->ftype, cb, cb_arg, ports->socket);
		} else if(ports->ftype == listen_type_ssl ||
			ports->ftype == listen_type_http) {
			cp = comm_point_create_tcp(base, ports->fd,
				tcp_accept_count, tcp_idle_timeout,
				harden_large_queries,
				http_max_streams, http_endpoint,
				tcp_conn_limit, bufsize, front->udp_buff,
				ports->ftype, cb, cb_arg, ports->socket);
			if(ports->ftype == listen_type_http) {
				if(!sslctx && !http_notls) {
					log_warn("HTTPS port configured, but "
						"no TLS tls-service-key or "
						"tls-service-pem set");
				}
#ifndef HAVE_SSL_CTX_SET_ALPN_SELECT_CB
				if(!http_notls) {
					log_warn("Unbound is not compiled "
						"with an OpenSSL version "
						"supporting ALPN "
						"(OpenSSL >= 1.0.2). This "
						"is required to use "
						"DNS-over-HTTPS");
				}
#endif
#ifndef HAVE_NGHTTP2_NGHTTP2_H
				log_warn("Unbound is not compiled with "
					"nghttp2. This is required to use "
					"DNS-over-HTTPS.");
#endif
			}
		} else if(ports->ftype == listen_type_udpancil ||
				  ports->ftype == listen_type_udpancil_dnscrypt) {
			cp = comm_point_create_udp_ancil(base, ports->fd,
				front->udp_buff, cb, cb_arg, ports->socket);
		}
		if(!cp) {
			log_err("can't create commpoint");
			listen_delete(front);
			return NULL;
		}
		if((http_notls && ports->ftype == listen_type_http) ||
			(ports->ftype == listen_type_tcp) ||
			(ports->ftype == listen_type_udp) ||
			(ports->ftype == listen_type_udpancil) ||
			(ports->ftype == listen_type_tcp_dnscrypt) ||
			(ports->ftype == listen_type_udp_dnscrypt) ||
			(ports->ftype == listen_type_udpancil_dnscrypt))
			cp->ssl = NULL;
		else
			cp->ssl = sslctx;
		cp->dtenv = dtenv;
		cp->do_not_close = 1;
#ifdef USE_DNSCRYPT
		if (ports->ftype == listen_type_udp_dnscrypt ||
			ports->ftype == listen_type_tcp_dnscrypt ||
			ports->ftype == listen_type_udpancil_dnscrypt) {
			cp->dnscrypt = 1;
			cp->dnscrypt_buffer = sldns_buffer_new(bufsize);
			if(!cp->dnscrypt_buffer) {
				log_err("can't alloc dnscrypt_buffer");
				comm_point_delete(cp);
				listen_delete(front);
				return NULL;
			}
			front->dnscrypt_udp_buff = cp->dnscrypt_buffer;
		}
#endif
		if(!listen_cp_insert(cp, front)) {
			log_err("malloc failed");
			comm_point_delete(cp);
			listen_delete(front);
			return NULL;
		}
		ports = ports->next;
	}
	if(!front->cps) {
		log_err("Could not open sockets to accept queries.");
		listen_delete(front);
		return NULL;
	}

	return front;
}

void
listen_list_delete(struct listen_list* list)
{
	struct listen_list *p = list, *pn;
	while(p) {
		pn = p->next;
		comm_point_delete(p->com);
		free(p);
		p = pn;
	}
}

void 
listen_delete(struct listen_dnsport* front)
{
	if(!front) 
		return;
	listen_list_delete(front->cps);
#ifdef USE_DNSCRYPT
	if(front->dnscrypt_udp_buff &&
		front->udp_buff != front->dnscrypt_udp_buff) {
		sldns_buffer_free(front->dnscrypt_udp_buff);
	}
#endif
	sldns_buffer_free(front->udp_buff);
	free(front);
}

#ifdef HAVE_GETIFADDRS
static int
resolve_ifa_name(struct ifaddrs *ifas, const char *search_ifa, char ***ip_addresses, int *ip_addresses_size)
{
	struct ifaddrs *ifa;
	void *tmpbuf;
	int last_ip_addresses_size = *ip_addresses_size;

	for(ifa = ifas; ifa != NULL; ifa = ifa->ifa_next) {
		sa_family_t family;
		const char* atsign;
#ifdef INET6      /* |   address ip    | % |  ifa name  | @ |  port  | nul */
		char addr_buf[INET6_ADDRSTRLEN + 1 + IF_NAMESIZE + 1 + 16 + 1];
#else
		char addr_buf[INET_ADDRSTRLEN + 1 + 16 + 1];
#endif

		if((atsign=strrchr(search_ifa, '@')) != NULL) {
			if(strlen(ifa->ifa_name) != (size_t)(atsign-search_ifa)
			   || strncmp(ifa->ifa_name, search_ifa,
			   atsign-search_ifa) != 0)
				continue;
		} else {
			if(strcmp(ifa->ifa_name, search_ifa) != 0)
				continue;
			atsign = "";
		}

		if(ifa->ifa_addr == NULL)
			continue;

		family = ifa->ifa_addr->sa_family;
		if(family == AF_INET) {
			char a4[INET_ADDRSTRLEN + 1];
			struct sockaddr_in *in4 = (struct sockaddr_in *)
				ifa->ifa_addr;
			if(!inet_ntop(family, &in4->sin_addr, a4, sizeof(a4))) {
				log_err("inet_ntop failed");
				return 0;
			}
			snprintf(addr_buf, sizeof(addr_buf), "%s%s",
				a4, atsign);
		}
#ifdef INET6
		else if(family == AF_INET6) {
			struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)
				ifa->ifa_addr;
			char a6[INET6_ADDRSTRLEN + 1];
			char if_index_name[IF_NAMESIZE + 1];
			if_index_name[0] = 0;
			if(!inet_ntop(family, &in6->sin6_addr, a6, sizeof(a6))) {
				log_err("inet_ntop failed");
				return 0;
			}
			(void)if_indextoname(in6->sin6_scope_id,
				(char *)if_index_name);
			if (strlen(if_index_name) != 0) {
				snprintf(addr_buf, sizeof(addr_buf),
					"%s%%%s%s", a6, if_index_name, atsign);
			} else {
				snprintf(addr_buf, sizeof(addr_buf), "%s%s",
					a6, atsign);
			}
		}
#endif
		else {
			continue;
		}
		verbose(4, "interface %s has address %s", search_ifa, addr_buf);

		tmpbuf = realloc(*ip_addresses, sizeof(char *) * (*ip_addresses_size + 1));
		if(!tmpbuf) {
			log_err("realloc failed: out of memory");
			return 0;
		} else {
			*ip_addresses = tmpbuf;
		}
		(*ip_addresses)[*ip_addresses_size] = strdup(addr_buf);
		if(!(*ip_addresses)[*ip_addresses_size]) {
			log_err("strdup failed: out of memory");
			return 0;
		}
		(*ip_addresses_size)++;
	}

	if (*ip_addresses_size == last_ip_addresses_size) {
		tmpbuf = realloc(*ip_addresses, sizeof(char *) * (*ip_addresses_size + 1));
		if(!tmpbuf) {
			log_err("realloc failed: out of memory");
			return 0;
		} else {
			*ip_addresses = tmpbuf;
		}
		(*ip_addresses)[*ip_addresses_size] = strdup(search_ifa);
		if(!(*ip_addresses)[*ip_addresses_size]) {
			log_err("strdup failed: out of memory");
			return 0;
		}
		(*ip_addresses_size)++;
	}
	return 1;
}
#endif /* HAVE_GETIFADDRS */

int resolve_interface_names(char** ifs, int num_ifs,
	struct config_strlist* list, char*** resif, int* num_resif)
{
#ifdef HAVE_GETIFADDRS
	struct ifaddrs *addrs = NULL;
	if(num_ifs == 0 && list == NULL) {
		*resif = NULL;
		*num_resif = 0;
		return 1;
	}
	if(getifaddrs(&addrs) == -1) {
		log_err("failed to list interfaces: getifaddrs: %s",
			strerror(errno));
		freeifaddrs(addrs);
		return 0;
	}
	if(ifs) {
		int i;
		for(i=0; i<num_ifs; i++) {
			if(!resolve_ifa_name(addrs, ifs[i], resif, num_resif)) {
				freeifaddrs(addrs);
				config_del_strarray(*resif, *num_resif);
				*resif = NULL;
				*num_resif = 0;
				return 0;
			}
		}
	}
	if(list) {
		struct config_strlist* p;
		for(p = list; p; p = p->next) {
			if(!resolve_ifa_name(addrs, p->str, resif, num_resif)) {
				freeifaddrs(addrs);
				config_del_strarray(*resif, *num_resif);
				*resif = NULL;
				*num_resif = 0;
				return 0;
			}
}
	}
	freeifaddrs(addrs);
	return 1;
#else
	struct config_strlist* p;
	if(num_ifs == 0 && list == NULL) {
		*resif = NULL;
		*num_resif = 0;
		return 1;
	}
	*num_resif = num_ifs;
	for(p = list; p; p = p->next) {
		(*num_resif)++;
	}
	*resif = calloc(*num_resif, sizeof(**resif));
	if(!*resif) {
		log_err("out of memory");
		return 0;
	}
	if(ifs) {
		int i;
		for(i=0; i<num_ifs; i++) {
			(*resif)[i] = strdup(ifs[i]);
			if(!((*resif)[i])) {
				log_err("out of memory");
				config_del_strarray(*resif, *num_resif);
				*resif = NULL;
				*num_resif = 0;
				return 0;
			}
		}
	}
	if(list) {
		int idx = num_ifs;
		for(p = list; p; p = p->next) {
			(*resif)[idx] = strdup(p->str);
			if(!((*resif)[idx])) {
				log_err("out of memory");
				config_del_strarray(*resif, *num_resif);
				*resif = NULL;
				*num_resif = 0;
				return 0;
			}
			idx++;
		}
	}
	return 1;
#endif /* HAVE_GETIFADDRS */
}

struct listen_port* 
listening_ports_open(struct config_file* cfg, char** ifs, int num_ifs,
	int* reuseport)
{
	struct listen_port* list = NULL;
	struct addrinfo hints;
	int i, do_ip4, do_ip6;
	int do_tcp, do_auto;
	char portbuf[32];
	snprintf(portbuf, sizeof(portbuf), "%d", cfg->port);
	do_ip4 = cfg->do_ip4;
	do_ip6 = cfg->do_ip6;
	do_tcp = cfg->do_tcp;
	do_auto = cfg->if_automatic && cfg->do_udp;
	if(cfg->incoming_num_tcp == 0)
		do_tcp = 0;

	/* getaddrinfo */
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	/* no name lookups on our listening ports */
	if(num_ifs > 0)
		hints.ai_flags |= AI_NUMERICHOST;
	hints.ai_family = AF_UNSPEC;
#ifndef INET6
	do_ip6 = 0;
#endif
	if(!do_ip4 && !do_ip6) {
		return NULL;
	}
	/* create ip4 and ip6 ports so that return addresses are nice. */
	if(do_auto || num_ifs == 0) {
		if(do_auto && cfg->if_automatic_ports &&
			cfg->if_automatic_ports[0]!=0) {
			char* now = cfg->if_automatic_ports;
			while(now && *now) {
				char* after;
				int extraport;
				while(isspace((unsigned char)*now))
					now++;
				if(!*now)
					break;
				after = now;
				extraport = (int)strtol(now, &after, 10);
				if(extraport < 0 || extraport > 65535) {
					log_err("interface-automatic-ports port number out of range, at position %d of '%s'", (int)(now-cfg->if_automatic_ports)+1, cfg->if_automatic_ports);
					listening_ports_free(list);
					return NULL;
				}
				if(extraport == 0 && now == after) {
					log_err("interface-automatic-ports could not be parsed, at position %d of '%s'", (int)(now-cfg->if_automatic_ports)+1, cfg->if_automatic_ports);
					listening_ports_free(list);
					return NULL;
				}
				now = after;
				snprintf(portbuf, sizeof(portbuf), "%d", extraport);
				if(do_ip6) {
					hints.ai_family = AF_INET6;
					if(!ports_create_if("::0",
						do_auto, cfg->do_udp, do_tcp,
						&hints, portbuf, &list,
						cfg->so_rcvbuf, cfg->so_sndbuf,
						cfg->ssl_port, cfg->tls_additional_port,
						cfg->https_port, reuseport, cfg->ip_transparent,
						cfg->tcp_mss, cfg->ip_freebind,
						cfg->http_nodelay, cfg->use_systemd,
						cfg->dnscrypt_port, cfg->ip_dscp)) {
						listening_ports_free(list);
						return NULL;
					}
				}
				if(do_ip4) {
					hints.ai_family = AF_INET;
					if(!ports_create_if("0.0.0.0",
						do_auto, cfg->do_udp, do_tcp,
						&hints, portbuf, &list,
						cfg->so_rcvbuf, cfg->so_sndbuf,
						cfg->ssl_port, cfg->tls_additional_port,
						cfg->https_port, reuseport, cfg->ip_transparent,
						cfg->tcp_mss, cfg->ip_freebind,
						cfg->http_nodelay, cfg->use_systemd,
						cfg->dnscrypt_port, cfg->ip_dscp)) {
						listening_ports_free(list);
						return NULL;
					}
				}
			}
			return list;
		}
		if(do_ip6) {
			hints.ai_family = AF_INET6;
			if(!ports_create_if(do_auto?"::0":"::1", 
				do_auto, cfg->do_udp, do_tcp, 
				&hints, portbuf, &list,
				cfg->so_rcvbuf, cfg->so_sndbuf,
				cfg->ssl_port, cfg->tls_additional_port,
				cfg->https_port, reuseport, cfg->ip_transparent,
				cfg->tcp_mss, cfg->ip_freebind,
				cfg->http_nodelay, cfg->use_systemd,
				cfg->dnscrypt_port, cfg->ip_dscp)) {
				listening_ports_free(list);
				return NULL;
			}
		}
		if(do_ip4) {
			hints.ai_family = AF_INET;
			if(!ports_create_if(do_auto?"0.0.0.0":"127.0.0.1", 
				do_auto, cfg->do_udp, do_tcp, 
				&hints, portbuf, &list,
				cfg->so_rcvbuf, cfg->so_sndbuf,
				cfg->ssl_port, cfg->tls_additional_port,
				cfg->https_port, reuseport, cfg->ip_transparent,
				cfg->tcp_mss, cfg->ip_freebind,
				cfg->http_nodelay, cfg->use_systemd,
				cfg->dnscrypt_port, cfg->ip_dscp)) {
				listening_ports_free(list);
				return NULL;
			}
		}
	} else for(i = 0; i<num_ifs; i++) {
		if(str_is_ip6(ifs[i])) {
			if(!do_ip6)
				continue;
			hints.ai_family = AF_INET6;
			if(!ports_create_if(ifs[i], 0, cfg->do_udp,
				do_tcp, &hints, portbuf, &list, 
				cfg->so_rcvbuf, cfg->so_sndbuf,
				cfg->ssl_port, cfg->tls_additional_port,
				cfg->https_port, reuseport, cfg->ip_transparent,
				cfg->tcp_mss, cfg->ip_freebind,
				cfg->http_nodelay, cfg->use_systemd,
				cfg->dnscrypt_port, cfg->ip_dscp)) {
				listening_ports_free(list);
				return NULL;
			}
		} else {
			if(!do_ip4)
				continue;
			hints.ai_family = AF_INET;
			if(!ports_create_if(ifs[i], 0, cfg->do_udp,
				do_tcp, &hints, portbuf, &list, 
				cfg->so_rcvbuf, cfg->so_sndbuf,
				cfg->ssl_port, cfg->tls_additional_port,
				cfg->https_port, reuseport, cfg->ip_transparent,
				cfg->tcp_mss, cfg->ip_freebind,
				cfg->http_nodelay, cfg->use_systemd,
				cfg->dnscrypt_port, cfg->ip_dscp)) {
				listening_ports_free(list);
				return NULL;
			}
		}
	}

	return list;
}

void listening_ports_free(struct listen_port* list)
{
	struct listen_port* nx;
	while(list) {
		nx = list->next;
		if(list->fd != -1) {
			sock_close(list->fd);
		}
		/* rc_ports don't have ub_socket */
		if(list->socket) {
			freeaddrinfo(list->socket->addr);
			free(list->socket);
		}
		free(list);
		list = nx;
	}
}

size_t listen_get_mem(struct listen_dnsport* listen)
{
	struct listen_list* p;
	size_t s = sizeof(*listen) + sizeof(*listen->base) + 
		sizeof(*listen->udp_buff) + 
		sldns_buffer_capacity(listen->udp_buff);
#ifdef USE_DNSCRYPT
	s += sizeof(*listen->dnscrypt_udp_buff);
	if(listen->udp_buff != listen->dnscrypt_udp_buff){
		s += sldns_buffer_capacity(listen->dnscrypt_udp_buff);
	}
#endif
	for(p = listen->cps; p; p = p->next) {
		s += sizeof(*p);
		s += comm_point_get_mem(p->com);
	}
	return s;
}

void listen_stop_accept(struct listen_dnsport* listen)
{
	/* do not stop the ones that have no tcp_free list
	 * (they have already stopped listening) */
	struct listen_list* p;
	for(p=listen->cps; p; p=p->next) {
		if(p->com->type == comm_tcp_accept &&
			p->com->tcp_free != NULL) {
			comm_point_stop_listening(p->com);
		}
	}
}

void listen_start_accept(struct listen_dnsport* listen)
{
	/* do not start the ones that have no tcp_free list, it is no
	 * use to listen to them because they have no free tcp handlers */
	struct listen_list* p;
	for(p=listen->cps; p; p=p->next) {
		if(p->com->type == comm_tcp_accept &&
			p->com->tcp_free != NULL) {
			comm_point_start_listening(p->com, -1, -1);
		}
	}
}

struct tcp_req_info*
tcp_req_info_create(struct sldns_buffer* spoolbuf)
{
	struct tcp_req_info* req = (struct tcp_req_info*)malloc(sizeof(*req));
	if(!req) {
		log_err("malloc failure for new stream outoforder processing structure");
		return NULL;
	}
	memset(req, 0, sizeof(*req));
	req->spool_buffer = spoolbuf;
	return req;
}

void
tcp_req_info_delete(struct tcp_req_info* req)
{
	if(!req) return;
	tcp_req_info_clear(req);
	/* cp is pointer back to commpoint that owns this struct and
	 * called delete on us */
	/* spool_buffer is shared udp buffer, not deleted here */
	free(req);
}

void tcp_req_info_clear(struct tcp_req_info* req)
{
	struct tcp_req_open_item* open, *nopen;
	struct tcp_req_done_item* item, *nitem;
	if(!req) return;

	/* free outstanding request mesh reply entries */
	open = req->open_req_list;
	while(open) {
		nopen = open->next;
		mesh_state_remove_reply(open->mesh, open->mesh_state, req->cp);
		free(open);
		open = nopen;
	}
	req->open_req_list = NULL;
	req->num_open_req = 0;
	
	/* free pending writable result packets */
	item = req->done_req_list;
	while(item) {
		nitem = item->next;
		lock_basic_lock(&stream_wait_count_lock);
		stream_wait_count -= (sizeof(struct tcp_req_done_item)
			+item->len);
		lock_basic_unlock(&stream_wait_count_lock);
		free(item->buf);
		free(item);
		item = nitem;
	}
	req->done_req_list = NULL;
	req->num_done_req = 0;
	req->read_is_closed = 0;
}

void
tcp_req_info_remove_mesh_state(struct tcp_req_info* req, struct mesh_state* m)
{
	struct tcp_req_open_item* open, *prev = NULL;
	if(!req || !m) return;
	open = req->open_req_list;
	while(open) {
		if(open->mesh_state == m) {
			struct tcp_req_open_item* next;
			if(prev) prev->next = open->next;
			else req->open_req_list = open->next;
			/* caller has to manage the mesh state reply entry */
			next = open->next;
			free(open);
			req->num_open_req --;

			/* prev = prev; */
			open = next;
			continue;
		}
		prev = open;
		open = open->next;
	}
}

/** setup listening for read or write */
static void
tcp_req_info_setup_listen(struct tcp_req_info* req)
{
	int wr = 0;
	int rd = 0;

	if(req->cp->tcp_byte_count != 0) {
		/* cannot change, halfway through */
		return;
	}

	if(!req->cp->tcp_is_reading)
		wr = 1;
	if(!req->read_is_closed)
		rd = 1;
	
	if(wr) {
		req->cp->tcp_is_reading = 0;
		comm_point_stop_listening(req->cp);
		comm_point_start_listening(req->cp, -1,
			adjusted_tcp_timeout(req->cp));
	} else if(rd) {
		req->cp->tcp_is_reading = 1;
		comm_point_stop_listening(req->cp);
		comm_point_start_listening(req->cp, -1,
			adjusted_tcp_timeout(req->cp));
		/* and also read it (from SSL stack buffers), so
		 * no event read event is expected since the remainder of
		 * the TLS frame is sitting in the buffers. */
		req->read_again = 1;
	} else {
		comm_point_stop_listening(req->cp);
		comm_point_start_listening(req->cp, -1,
			adjusted_tcp_timeout(req->cp));
		comm_point_listen_for_rw(req->cp, 0, 0);
	}
}

/** remove first item from list of pending results */
static struct tcp_req_done_item*
tcp_req_info_pop_done(struct tcp_req_info* req)
{
	struct tcp_req_done_item* item;
	log_assert(req->num_done_req > 0 && req->done_req_list);
	item = req->done_req_list;
	lock_basic_lock(&stream_wait_count_lock);
	stream_wait_count -= (sizeof(struct tcp_req_done_item)+item->len);
	lock_basic_unlock(&stream_wait_count_lock);
	req->done_req_list = req->done_req_list->next;
	req->num_done_req --;
	return item;
}

/** Send given buffer and setup to write */
static void
tcp_req_info_start_write_buf(struct tcp_req_info* req, uint8_t* buf,
	size_t len)
{
	sldns_buffer_clear(req->cp->buffer);
	sldns_buffer_write(req->cp->buffer, buf, len);
	sldns_buffer_flip(req->cp->buffer);

	req->cp->tcp_is_reading = 0; /* we are now writing */
}

/** pick up the next result and start writing it to the channel */
static void
tcp_req_pickup_next_result(struct tcp_req_info* req)
{
	if(req->num_done_req > 0) {
		/* unlist the done item from the list of pending results */
		struct tcp_req_done_item* item = tcp_req_info_pop_done(req);
		tcp_req_info_start_write_buf(req, item->buf, item->len);
		free(item->buf);
		free(item);
	}
}

/** the read channel has closed */
int
tcp_req_info_handle_read_close(struct tcp_req_info* req)
{
	verbose(VERB_ALGO, "tcp channel read side closed %d", req->cp->fd);
	/* reset byte count for (potential) partial read */
	req->cp->tcp_byte_count = 0;
	/* if we still have results to write, pick up next and write it */
	if(req->num_done_req != 0) {
		tcp_req_pickup_next_result(req);
		tcp_req_info_setup_listen(req);
		return 1;
	}
	/* if nothing to do, this closes the connection */
	if(req->num_open_req == 0 && req->num_done_req == 0)
		return 0;
	/* otherwise, we must be waiting for dns resolve, wait with timeout */
	req->read_is_closed = 1;
	tcp_req_info_setup_listen(req);
	return 1;
}

void
tcp_req_info_handle_writedone(struct tcp_req_info* req)
{
	/* back to reading state, we finished this write event */
	sldns_buffer_clear(req->cp->buffer);
	if(req->num_done_req == 0 && req->read_is_closed) {
		/* no more to write and nothing to read, close it */
		comm_point_drop_reply(&req->cp->repinfo);
		return;
	}
	req->cp->tcp_is_reading = 1;
	/* see if another result needs writing */
	tcp_req_pickup_next_result(req);

	/* see if there is more to write, if not stop_listening for writing */
	/* see if new requests are allowed, if so, start_listening
	 * for reading */
	tcp_req_info_setup_listen(req);
}

void
tcp_req_info_handle_readdone(struct tcp_req_info* req)
{
	struct comm_point* c = req->cp;

	/* we want to read up several requests, unless there are
	 * pending answers */

	req->is_drop = 0;
	req->is_reply = 0;
	req->in_worker_handle = 1;
	sldns_buffer_set_limit(req->spool_buffer, 0);
	/* handle the current request */
	/* this calls the worker handle request routine that could give
	 * a cache response, or localdata response, or drop the reply,
	 * or schedule a mesh entry for later */
	fptr_ok(fptr_whitelist_comm_point(c->callback));
	if( (*c->callback)(c, c->cb_arg, NETEVENT_NOERROR, &c->repinfo) ) {
		req->in_worker_handle = 0;
		/* there is an answer, put it up.  It is already in the
		 * c->buffer, just send it. */
		/* since we were just reading a query, the channel is
		 * clear to write to */
	send_it:
		c->tcp_is_reading = 0;
		comm_point_stop_listening(c);
		comm_point_start_listening(c, -1, adjusted_tcp_timeout(c));
		return;
	}
	req->in_worker_handle = 0;
	/* it should be waiting in the mesh for recursion.
	 * If mesh failed to add a new entry and called commpoint_drop_reply. 
	 * Then the mesh state has been cleared. */
	if(req->is_drop) {
		/* the reply has been dropped, stream has been closed. */
		return;
	}
	/* If mesh failed(mallocfail) and called commpoint_send_reply with
	 * something like servfail then we pick up that reply below. */
	if(req->is_reply) {
		goto send_it;
	}

	sldns_buffer_clear(c->buffer);
	/* if pending answers, pick up an answer and start sending it */
	tcp_req_pickup_next_result(req);

	/* if answers pending, start sending answers */
	/* read more requests if we can have more requests */
	tcp_req_info_setup_listen(req);
}

int
tcp_req_info_add_meshstate(struct tcp_req_info* req,
	struct mesh_area* mesh, struct mesh_state* m)
{
	struct tcp_req_open_item* item;
	log_assert(req && mesh && m);
	item = (struct tcp_req_open_item*)malloc(sizeof(*item));
	if(!item) return 0;
	item->next = req->open_req_list;
	item->mesh = mesh;
	item->mesh_state = m;
	req->open_req_list = item;
	req->num_open_req++;
	return 1;
}

/** Add a result to the result list.  At the end. */
static int
tcp_req_info_add_result(struct tcp_req_info* req, uint8_t* buf, size_t len)
{
	struct tcp_req_done_item* last = NULL;
	struct tcp_req_done_item* item;
	size_t space;

	/* see if we have space */
	space = sizeof(struct tcp_req_done_item) + len;
	lock_basic_lock(&stream_wait_count_lock);
	if(stream_wait_count + space > stream_wait_max) {
		lock_basic_unlock(&stream_wait_count_lock);
		verbose(VERB_ALGO, "drop stream reply, no space left, in stream-wait-size");
		return 0;
	}
	stream_wait_count += space;
	lock_basic_unlock(&stream_wait_count_lock);

	/* find last element */
	last = req->done_req_list;
	while(last && last->next)
		last = last->next;
	
	/* create new element */
	item = (struct tcp_req_done_item*)malloc(sizeof(*item));
	if(!item) {
		log_err("malloc failure, for stream result list");
		return 0;
	}
	item->next = NULL;
	item->len = len;
	item->buf = memdup(buf, len);
	if(!item->buf) {
		free(item);
		log_err("malloc failure, adding reply to stream result list");
		return 0;
	}

	/* link in */
	if(last) last->next = item;
	else req->done_req_list = item;
	req->num_done_req++;
	return 1;
}

void
tcp_req_info_send_reply(struct tcp_req_info* req)
{
	if(req->in_worker_handle) {
		/* reply from mesh is in the spool_buffer */
		/* copy now, so that the spool buffer is free for other tasks
		 * before the callback is done */
		sldns_buffer_clear(req->cp->buffer);
		sldns_buffer_write(req->cp->buffer,
			sldns_buffer_begin(req->spool_buffer),
			sldns_buffer_limit(req->spool_buffer));
		sldns_buffer_flip(req->cp->buffer);
		req->is_reply = 1;
		return;
	}
	/* now that the query has been handled, that mesh_reply entry
	 * should be removed, from the tcp_req_info list,
	 * the mesh state cleanup removes then with region_cleanup and
	 * replies_sent true. */
	/* see if we can send it straight away (we are not doing
	 * anything else).  If so, copy to buffer and start */
	if(req->cp->tcp_is_reading && req->cp->tcp_byte_count == 0) {
		/* buffer is free, and was ready to read new query into,
		 * but we are now going to use it to send this answer */
		tcp_req_info_start_write_buf(req,
			sldns_buffer_begin(req->spool_buffer),
			sldns_buffer_limit(req->spool_buffer));
		/* switch to listen to write events */
		comm_point_stop_listening(req->cp);
		comm_point_start_listening(req->cp, -1,
			adjusted_tcp_timeout(req->cp));
		return;
	}
	/* queue up the answer behind the others already pending */
	if(!tcp_req_info_add_result(req, sldns_buffer_begin(req->spool_buffer),
		sldns_buffer_limit(req->spool_buffer))) {
		/* drop the connection, we are out of resources */
		comm_point_drop_reply(&req->cp->repinfo);
	}
}

size_t tcp_req_info_get_stream_buffer_size(void)
{
	size_t s;
	if(!stream_wait_lock_inited)
		return stream_wait_count;
	lock_basic_lock(&stream_wait_count_lock);
	s = stream_wait_count;
	lock_basic_unlock(&stream_wait_count_lock);
	return s;
}

size_t http2_get_query_buffer_size(void)
{
	size_t s;
	if(!http2_query_buffer_lock_inited)
		return http2_query_buffer_count;
	lock_basic_lock(&http2_query_buffer_count_lock);
	s = http2_query_buffer_count;
	lock_basic_unlock(&http2_query_buffer_count_lock);
	return s;
}

size_t http2_get_response_buffer_size(void)
{
	size_t s;
	if(!http2_response_buffer_lock_inited)
		return http2_response_buffer_count;
	lock_basic_lock(&http2_response_buffer_count_lock);
	s = http2_response_buffer_count;
	lock_basic_unlock(&http2_response_buffer_count_lock);
	return s;
}

#ifdef HAVE_NGHTTP2
/** nghttp2 callback. Used to copy response from rbuffer to nghttp2 session */
static ssize_t http2_submit_response_read_callback(
	nghttp2_session* ATTR_UNUSED(session),
	int32_t stream_id, uint8_t* buf, size_t length, uint32_t* data_flags,
	nghttp2_data_source* source, void* ATTR_UNUSED(cb_arg))
{
	struct http2_stream* h2_stream;
	struct http2_session* h2_session = source->ptr;
	size_t copylen = length;
	if(!(h2_stream = nghttp2_session_get_stream_user_data(
		h2_session->session, stream_id))) {
		verbose(VERB_QUERY, "http2: cannot get stream data, closing "
			"stream");
		return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
	}
	if(!h2_stream->rbuffer ||
		sldns_buffer_remaining(h2_stream->rbuffer) == 0) {
		verbose(VERB_QUERY, "http2: cannot submit buffer. No data "
			"available in rbuffer");
		/* rbuffer will be free'd in frame close cb */
		return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
	}

	if(copylen > sldns_buffer_remaining(h2_stream->rbuffer))
		copylen = sldns_buffer_remaining(h2_stream->rbuffer);
	if(copylen > SSIZE_MAX)
		copylen = SSIZE_MAX; /* will probably never happen */

	memcpy(buf, sldns_buffer_current(h2_stream->rbuffer), copylen);
	sldns_buffer_skip(h2_stream->rbuffer, copylen);

	if(sldns_buffer_remaining(h2_stream->rbuffer) == 0) {
		*data_flags |= NGHTTP2_DATA_FLAG_EOF;
		lock_basic_lock(&http2_response_buffer_count_lock);
		http2_response_buffer_count -=
			sldns_buffer_capacity(h2_stream->rbuffer);
		lock_basic_unlock(&http2_response_buffer_count_lock);
		sldns_buffer_free(h2_stream->rbuffer);
		h2_stream->rbuffer = NULL;
	}

	return copylen;
}

/**
 * Send RST_STREAM frame for stream.
 * @param h2_session: http2 session to submit frame to
 * @param h2_stream: http2 stream containing frame ID to use in RST_STREAM
 * @return 0 on error, 1 otherwise
 */
static int http2_submit_rst_stream(struct http2_session* h2_session,
		struct http2_stream* h2_stream)
{
	int ret = nghttp2_submit_rst_stream(h2_session->session,
		NGHTTP2_FLAG_NONE, h2_stream->stream_id,
		NGHTTP2_INTERNAL_ERROR);
	if(ret) {
		verbose(VERB_QUERY, "http2: nghttp2_submit_rst_stream failed, "
			"error: %s", nghttp2_strerror(ret));
		return 0;
	}
	return 1;
}

/**
 * DNS response ready to be submitted to nghttp2, to be prepared for sending
 * out. Response is stored in c->buffer. Copy to rbuffer because the c->buffer
 * might be used before this will be sent out.
 * @param h2_session: http2 session, containing c->buffer which contains answer
 * @return 0 on error, 1 otherwise
 */
int http2_submit_dns_response(struct http2_session* h2_session)
{
	int ret;
	nghttp2_data_provider data_prd;
	char status[4];
	nghttp2_nv headers[3];
	struct http2_stream* h2_stream = h2_session->c->h2_stream;
	size_t rlen;
	char rlen_str[32];

	if(h2_stream->rbuffer) {
		log_err("http2 submit response error: rbuffer already "
			"exists");
		return 0;
	}
	if(sldns_buffer_remaining(h2_session->c->buffer) == 0) {
		log_err("http2 submit response error: c->buffer not complete");
		return 0;
	}

	if(snprintf(status, 4, "%d", h2_stream->status) != 3) {
		verbose(VERB_QUERY, "http2: submit response error: "
			"invalid status");
		return 0;
	}

	rlen = sldns_buffer_remaining(h2_session->c->buffer);
	snprintf(rlen_str, sizeof(rlen_str), "%u", (unsigned)rlen);

	lock_basic_lock(&http2_response_buffer_count_lock);
	if(http2_response_buffer_count + rlen > http2_response_buffer_max) {
		lock_basic_unlock(&http2_response_buffer_count_lock);
		verbose(VERB_ALGO, "reset HTTP2 stream, no space left, "
			"in https-response-buffer-size");
		return http2_submit_rst_stream(h2_session, h2_stream);
	}
	http2_response_buffer_count += rlen;
	lock_basic_unlock(&http2_response_buffer_count_lock);

	if(!(h2_stream->rbuffer = sldns_buffer_new(rlen))) {
		lock_basic_lock(&http2_response_buffer_count_lock);
		http2_response_buffer_count -= rlen;
		lock_basic_unlock(&http2_response_buffer_count_lock);
		log_err("http2 submit response error: malloc failure");
		return 0;
	}

	headers[0].name = (uint8_t*)":status";
	headers[0].namelen = 7;
	headers[0].value = (uint8_t*)status;
	headers[0].valuelen = 3;
	headers[0].flags = NGHTTP2_NV_FLAG_NONE;

	headers[1].name = (uint8_t*)"content-type";
	headers[1].namelen = 12;
	headers[1].value = (uint8_t*)"application/dns-message";
	headers[1].valuelen = 23;
	headers[1].flags = NGHTTP2_NV_FLAG_NONE;

	headers[2].name = (uint8_t*)"content-length";
	headers[2].namelen = 14;
	headers[2].value = (uint8_t*)rlen_str;
	headers[2].valuelen = strlen(rlen_str);
	headers[2].flags = NGHTTP2_NV_FLAG_NONE;

	sldns_buffer_write(h2_stream->rbuffer,
		sldns_buffer_current(h2_session->c->buffer),
		sldns_buffer_remaining(h2_session->c->buffer));
	sldns_buffer_flip(h2_stream->rbuffer);

	data_prd.source.ptr = h2_session;
	data_prd.read_callback = http2_submit_response_read_callback;
	ret = nghttp2_submit_response(h2_session->session, h2_stream->stream_id,
		headers, 3, &data_prd);
	if(ret) {
		verbose(VERB_QUERY, "http2: set_stream_user_data failed, "
			"error: %s", nghttp2_strerror(ret));
		return 0;
	}
	return 1;
}
#else
int http2_submit_dns_response(void* ATTR_UNUSED(v))
{
	return 0;
}
#endif

#ifdef HAVE_NGHTTP2
/** HTTP status to descriptive string */
static char* http_status_to_str(enum http_status s)
{
	switch(s) {
		case HTTP_STATUS_OK:
			return "OK";
		case HTTP_STATUS_BAD_REQUEST:
			return "Bad Request";
		case HTTP_STATUS_NOT_FOUND:
			return "Not Found";
		case HTTP_STATUS_PAYLOAD_TOO_LARGE:
			return "Payload Too Large";
		case HTTP_STATUS_URI_TOO_LONG:
			return "URI Too Long";
		case HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE:
			return "Unsupported Media Type";
		case HTTP_STATUS_NOT_IMPLEMENTED:
			return "Not Implemented";
	}
	return "Status Unknown";
}

/** nghttp2 callback. Used to copy error message to nghttp2 session */
static ssize_t http2_submit_error_read_callback(
	nghttp2_session* ATTR_UNUSED(session),
	int32_t stream_id, uint8_t* buf, size_t length, uint32_t* data_flags,
	nghttp2_data_source* source, void* ATTR_UNUSED(cb_arg))
{
	struct http2_stream* h2_stream;
	struct http2_session* h2_session = source->ptr;
	char* msg;
	if(!(h2_stream = nghttp2_session_get_stream_user_data(
		h2_session->session, stream_id))) {
		verbose(VERB_QUERY, "http2: cannot get stream data, closing "
			"stream");
		return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
	}
	*data_flags |= NGHTTP2_DATA_FLAG_EOF;
	msg = http_status_to_str(h2_stream->status);
	if(length < strlen(msg))
		return 0; /* not worth trying over multiple frames */
	memcpy(buf, msg, strlen(msg));
	return strlen(msg);

}

/**
 * HTTP error response ready to be submitted to nghttp2, to be prepared for
 * sending out. Message body will contain descriptive string for HTTP status.
 * @param h2_session: http2 session to submit to
 * @param h2_stream: http2 stream containing HTTP status to use for error
 * @return 0 on error, 1 otherwise
 */
static int http2_submit_error(struct http2_session* h2_session,
	struct http2_stream* h2_stream)
{
	int ret;
	char status[4];
	nghttp2_data_provider data_prd;
	nghttp2_nv headers[1]; /* will be copied by nghttp */
	if(snprintf(status, 4, "%d", h2_stream->status) != 3) {
		verbose(VERB_QUERY, "http2: submit error failed, "
			"invalid status");
		return 0;
	}
	headers[0].name = (uint8_t*)":status";
	headers[0].namelen = 7;
	headers[0].value = (uint8_t*)status;
	headers[0].valuelen = 3;
	headers[0].flags = NGHTTP2_NV_FLAG_NONE;

	data_prd.source.ptr = h2_session;
	data_prd.read_callback = http2_submit_error_read_callback;

	ret = nghttp2_submit_response(h2_session->session, h2_stream->stream_id,
		headers, 1, &data_prd);
	if(ret) {
		verbose(VERB_QUERY, "http2: submit error failed, "
			"error: %s", nghttp2_strerror(ret));
		return 0;
	}
	return 1;
}

/**
 * Start query handling. Query is stored in the stream, and will be free'd here.
 * @param h2_session: http2 session, containing comm point
 * @param h2_stream: stream containing buffered query
 * @return: -1 on error, 1 if answer is stored in c->buffer, 0 if there is no
 * reply available (yet).
 */
static int http2_query_read_done(struct http2_session* h2_session,
	struct http2_stream* h2_stream)
{
	log_assert(h2_stream->qbuffer);

	if(h2_session->c->h2_stream) {
		verbose(VERB_ALGO, "http2_query_read_done failure: shared "
			"buffer already assigned to stream");
		return -1;
	}
    
    /* the c->buffer might be used by mesh_send_reply and no be cleard
	 * need to be cleared before use */
	sldns_buffer_clear(h2_session->c->buffer);
	if(sldns_buffer_remaining(h2_session->c->buffer) <
		sldns_buffer_remaining(h2_stream->qbuffer)) {
		/* qbuffer will be free'd in frame close cb */
		sldns_buffer_clear(h2_session->c->buffer);
		verbose(VERB_ALGO, "http2_query_read_done failure: can't fit "
			"qbuffer in c->buffer");
		return -1;
	}

	sldns_buffer_write(h2_session->c->buffer,
		sldns_buffer_current(h2_stream->qbuffer),
		sldns_buffer_remaining(h2_stream->qbuffer));

	lock_basic_lock(&http2_query_buffer_count_lock);
	http2_query_buffer_count -= sldns_buffer_capacity(h2_stream->qbuffer);
	lock_basic_unlock(&http2_query_buffer_count_lock);
	sldns_buffer_free(h2_stream->qbuffer);
	h2_stream->qbuffer = NULL;

	sldns_buffer_flip(h2_session->c->buffer);
	h2_session->c->h2_stream = h2_stream;
	fptr_ok(fptr_whitelist_comm_point(h2_session->c->callback));
	if((*h2_session->c->callback)(h2_session->c, h2_session->c->cb_arg,
		NETEVENT_NOERROR, &h2_session->c->repinfo)) {
		return 1; /* answer in c->buffer */
	}
	sldns_buffer_clear(h2_session->c->buffer);
	h2_session->c->h2_stream = NULL;
	return 0; /* mesh state added, or dropped */
}

/** nghttp2 callback. Used to check if the received frame indicates the end of a
 * stream. Gather collected request data and start query handling. */
static int http2_req_frame_recv_cb(nghttp2_session* session,
	const nghttp2_frame* frame, void* cb_arg)
{
	struct http2_session* h2_session = (struct http2_session*)cb_arg;
	struct http2_stream* h2_stream;
	int query_read_done;

	if((frame->hd.type != NGHTTP2_DATA &&
		frame->hd.type != NGHTTP2_HEADERS) ||
		!(frame->hd.flags & NGHTTP2_FLAG_END_STREAM)) {
			return 0;
	}

	if(!(h2_stream = nghttp2_session_get_stream_user_data(
		session, frame->hd.stream_id)))
		return 0;

	if(h2_stream->invalid_endpoint) {
		h2_stream->status = HTTP_STATUS_NOT_FOUND;
		goto submit_http_error;
	}

	if(h2_stream->invalid_content_type) {
		h2_stream->status = HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE;
		goto submit_http_error;
	}

	if(h2_stream->http_method != HTTP_METHOD_GET &&
		h2_stream->http_method != HTTP_METHOD_POST) {
		h2_stream->status = HTTP_STATUS_NOT_IMPLEMENTED;
		goto submit_http_error;
	}

	if(h2_stream->query_too_large) {
		if(h2_stream->http_method == HTTP_METHOD_POST)
			h2_stream->status = HTTP_STATUS_PAYLOAD_TOO_LARGE;
		else
			h2_stream->status = HTTP_STATUS_URI_TOO_LONG;
		goto submit_http_error;
	}

	if(!h2_stream->qbuffer) {
		h2_stream->status = HTTP_STATUS_BAD_REQUEST;
		goto submit_http_error;
	}

	if(h2_stream->status) {
submit_http_error:
		verbose(VERB_QUERY, "http2 request invalid, returning :status="
			"%d", h2_stream->status);
		if(!http2_submit_error(h2_session, h2_stream)) {
			return NGHTTP2_ERR_CALLBACK_FAILURE;
		}
		return 0;
	}
	h2_stream->status = HTTP_STATUS_OK;

	sldns_buffer_flip(h2_stream->qbuffer);
	h2_session->postpone_drop = 1;
	query_read_done = http2_query_read_done(h2_session, h2_stream);
	if(query_read_done < 0)
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	else if(!query_read_done) {
		if(h2_session->is_drop) {
			/* connection needs to be closed. Return failure to make
			 * sure no other action are taken anymore on comm point.
			 * failure will result in reclaiming (and closing)
			 * of comm point. */
			verbose(VERB_QUERY, "http2 query dropped in worker cb");
			h2_session->postpone_drop = 0;
			return NGHTTP2_ERR_CALLBACK_FAILURE;
		}
		/* nothing to submit right now, query added to mesh. */
		h2_session->postpone_drop = 0;
		return 0;
	}
	if(!http2_submit_dns_response(h2_session)) {
		sldns_buffer_clear(h2_session->c->buffer);
		h2_session->c->h2_stream = NULL;
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	}
	verbose(VERB_QUERY, "http2 query submitted to session");
	sldns_buffer_clear(h2_session->c->buffer);
	h2_session->c->h2_stream = NULL;
	return 0;
}

/** nghttp2 callback. Used to detect start of new streams. */
static int http2_req_begin_headers_cb(nghttp2_session* session,
	const nghttp2_frame* frame, void* cb_arg)
{
	struct http2_session* h2_session = (struct http2_session*)cb_arg;
	struct http2_stream* h2_stream;
	int ret;
	if(frame->hd.type != NGHTTP2_HEADERS ||
		frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
		/* only interested in request headers */
		return 0;
	}
	if(!(h2_stream = http2_stream_create(frame->hd.stream_id))) {
		log_err("malloc failure while creating http2 stream");
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	}
	http2_session_add_stream(h2_session, h2_stream);
	ret = nghttp2_session_set_stream_user_data(session,
		frame->hd.stream_id, h2_stream);
	if(ret) {
		/* stream does not exist */
		verbose(VERB_QUERY, "http2: set_stream_user_data failed, "
			"error: %s", nghttp2_strerror(ret));
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	}

	return 0;
}

/**
 * base64url decode, store in qbuffer
 * @param h2_session: http2 session
 * @param h2_stream: http2 stream
 * @param start: start of the base64 string
 * @param length: length of the base64 string
 * @return: 0 on error, 1 otherwise. query will be stored in h2_stream->qbuffer,
 * buffer will be NULL is unparseble.
 */
static int http2_buffer_uri_query(struct http2_session* h2_session,
	struct http2_stream* h2_stream, const uint8_t* start, size_t length)
{
	size_t expectb64len;
	int b64len;
	if(h2_stream->http_method == HTTP_METHOD_POST)
		return 1;
	if(length == 0)
		return 1;
	if(h2_stream->qbuffer) {
		verbose(VERB_ALGO, "http2_req_header fail, "
			"qbuffer already set");
		return 0;
	}

	/* calculate size, might be a bit bigger than the real
	 * decoded buffer size */
	expectb64len = sldns_b64_pton_calculate_size(length);
	log_assert(expectb64len > 0);
	if(expectb64len >
		h2_session->c->http2_stream_max_qbuffer_size) {
		h2_stream->query_too_large = 1;
		return 1;
	}

	lock_basic_lock(&http2_query_buffer_count_lock);
	if(http2_query_buffer_count + expectb64len > http2_query_buffer_max) {
		lock_basic_unlock(&http2_query_buffer_count_lock);
		verbose(VERB_ALGO, "reset HTTP2 stream, no space left, "
			"in http2-query-buffer-size");
		return http2_submit_rst_stream(h2_session, h2_stream);
	}
	http2_query_buffer_count += expectb64len;
	lock_basic_unlock(&http2_query_buffer_count_lock);
	if(!(h2_stream->qbuffer = sldns_buffer_new(expectb64len))) {
		lock_basic_lock(&http2_query_buffer_count_lock);
		http2_query_buffer_count -= expectb64len;
		lock_basic_unlock(&http2_query_buffer_count_lock);
		log_err("http2_req_header fail, qbuffer "
			"malloc failure");
		return 0;
	}

	if(sldns_b64_contains_nonurl((char const*)start, length)) {
		char buf[65536+4];
		verbose(VERB_ALGO, "HTTP2 stream contains wrong b64 encoding");
		/* copy to the scratch buffer temporarily to terminate the
		 * string with a zero */
		if(length+1 > sizeof(buf)) {
			/* too long */
			lock_basic_lock(&http2_query_buffer_count_lock);
			http2_query_buffer_count -= expectb64len;
			lock_basic_unlock(&http2_query_buffer_count_lock);
			sldns_buffer_free(h2_stream->qbuffer);
			h2_stream->qbuffer = NULL;
			return 1;
		}
		memmove(buf, start, length);
		buf[length] = 0;
		if(!(b64len = sldns_b64_pton(buf, sldns_buffer_current(
			h2_stream->qbuffer), expectb64len)) || b64len < 0) {
			lock_basic_lock(&http2_query_buffer_count_lock);
			http2_query_buffer_count -= expectb64len;
			lock_basic_unlock(&http2_query_buffer_count_lock);
			sldns_buffer_free(h2_stream->qbuffer);
			h2_stream->qbuffer = NULL;
			return 1;
		}
	} else {
		if(!(b64len = sldns_b64url_pton(
			(char const *)start, length,
			sldns_buffer_current(h2_stream->qbuffer),
			expectb64len)) || b64len < 0) {
			lock_basic_lock(&http2_query_buffer_count_lock);
			http2_query_buffer_count -= expectb64len;
			lock_basic_unlock(&http2_query_buffer_count_lock);
			sldns_buffer_free(h2_stream->qbuffer);
			h2_stream->qbuffer = NULL;
			/* return without error, method can be an
			 * unknown POST */
			return 1;
		}
	}
	sldns_buffer_skip(h2_stream->qbuffer, (size_t)b64len);
	return 1;
}

/** nghttp2 callback. Used to parse headers from HEADER frames. */
static int http2_req_header_cb(nghttp2_session* session,
	const nghttp2_frame* frame, const uint8_t* name, size_t namelen,
	const uint8_t* value, size_t valuelen, uint8_t ATTR_UNUSED(flags),
	void* cb_arg)
{
	struct http2_stream* h2_stream = NULL;
	struct http2_session* h2_session = (struct http2_session*)cb_arg;
	/* nghttp2 deals with CONTINUATION frames and provides them as part of
	 * the HEADER */
	if(frame->hd.type != NGHTTP2_HEADERS ||
		frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
		/* only interested in request headers */
		return 0;
	}
	if(!(h2_stream = nghttp2_session_get_stream_user_data(session,
		frame->hd.stream_id)))
		return 0;

	/* earlier checks already indicate we can stop handling this query */
	if(h2_stream->http_method == HTTP_METHOD_UNSUPPORTED ||
		h2_stream->invalid_content_type ||
		h2_stream->invalid_endpoint)
		return 0;


	/* nghttp2 performs some sanity checks in the headers, including:
	 * name and value are guaranteed to be null terminated
	 * name is guaranteed to be lowercase
	 * content-length value is guaranteed to contain digits
	 */

	if(!h2_stream->http_method && namelen == 7 &&
		memcmp(":method", name, namelen) == 0) {
		/* Case insensitive check on :method value to be on the safe
		 * side. I failed to find text about case sensitivity in specs.
		 */
		if(valuelen == 3 && strcasecmp("GET", (const char*)value) == 0)
			h2_stream->http_method = HTTP_METHOD_GET;
		else if(valuelen == 4 &&
			strcasecmp("POST", (const char*)value) == 0) {
			h2_stream->http_method = HTTP_METHOD_POST;
			if(h2_stream->qbuffer) {
				/* POST method uses query from DATA frames */
				lock_basic_lock(&http2_query_buffer_count_lock);
				http2_query_buffer_count -=
					sldns_buffer_capacity(h2_stream->qbuffer);
				lock_basic_unlock(&http2_query_buffer_count_lock);
				sldns_buffer_free(h2_stream->qbuffer);
				h2_stream->qbuffer = NULL;
			}
		} else
			h2_stream->http_method = HTTP_METHOD_UNSUPPORTED;
		return 0;
	}
	if(namelen == 5 && memcmp(":path", name, namelen) == 0) {
		/* :path may contain DNS query, depending on method. Method might
		 * not be known yet here, so check after finishing receiving
		 * stream. */
#define	HTTP_QUERY_PARAM "?dns="
		size_t el = strlen(h2_session->c->http_endpoint);
		size_t qpl = strlen(HTTP_QUERY_PARAM);

		if(valuelen < el || memcmp(h2_session->c->http_endpoint,
			value, el) != 0) {
			h2_stream->invalid_endpoint = 1;
			return 0;
		}
		/* larger than endpoint only allowed if it is for the query
		 * parameter */
		if(valuelen <= el+qpl ||
			memcmp(HTTP_QUERY_PARAM, value+el, qpl) != 0) {
			if(valuelen != el)
				h2_stream->invalid_endpoint = 1;
			return 0;
		}

		if(!http2_buffer_uri_query(h2_session, h2_stream,
			value+(el+qpl), valuelen-(el+qpl))) {
			return NGHTTP2_ERR_CALLBACK_FAILURE;
		}
		return 0;
	}
	/* Content type is a SHOULD (rfc7231#section-3.1.1.5) when using POST,
	 * and not needed when using GET. Don't enfore.
	 * If set only allow lowercase "application/dns-message".
	 *
	 * Clients SHOULD (rfc8484#section-4.1) set an accept header, but MUST
	 * be able to handle "application/dns-message". Since that is the only
	 * content-type supported we can ignore the accept header.
	 */
	if((namelen == 12 && memcmp("content-type", name, namelen) == 0)) {
		if(valuelen != 23 || memcmp("application/dns-message", value,
			valuelen) != 0) {
			h2_stream->invalid_content_type = 1;
		}
	}

	/* Only interested in content-lentg for POST (on not yet known) method.
	 */
	if((!h2_stream->http_method ||
		h2_stream->http_method == HTTP_METHOD_POST) &&
		!h2_stream->content_length && namelen  == 14 &&
		memcmp("content-length", name, namelen) == 0) {
		if(valuelen > 5) {
			h2_stream->query_too_large = 1;
			return 0;
		}
		/* guaranteed to only contain digits and be null terminated */
		h2_stream->content_length = atoi((const char*)value);
		if(h2_stream->content_length >
			h2_session->c->http2_stream_max_qbuffer_size) {
			h2_stream->query_too_large = 1;
			return 0;
		}
	}
	return 0;
}

/** nghttp2 callback. Used to get data from DATA frames, which can contain
 * queries in POST requests. */
static int http2_req_data_chunk_recv_cb(nghttp2_session* ATTR_UNUSED(session),
	uint8_t ATTR_UNUSED(flags), int32_t stream_id, const uint8_t* data,
	size_t len, void* cb_arg)
{
	struct http2_session* h2_session = (struct http2_session*)cb_arg;
	struct http2_stream* h2_stream;
	size_t qlen = 0;

	if(!(h2_stream = nghttp2_session_get_stream_user_data(
		h2_session->session, stream_id))) {
		return 0;
	}

	if(h2_stream->query_too_large)
		return 0;

	if(!h2_stream->qbuffer) {
		if(h2_stream->content_length) {
			if(h2_stream->content_length < len)
				/* getting more data in DATA frame than
				 * advertised in content-length header. */
				return NGHTTP2_ERR_CALLBACK_FAILURE;
			qlen = h2_stream->content_length;
		} else if(len <= h2_session->c->http2_stream_max_qbuffer_size) {
			/* setting this to msg-buffer-size can result in a lot
			 * of memory consuption. Most queries should fit in a
			 * single DATA frame, and most POST queries will
			 * contain content-length which does not impose this
			 * limit. */
			qlen = len;
		}
	}
	if(!h2_stream->qbuffer && qlen) {
		lock_basic_lock(&http2_query_buffer_count_lock);
		if(http2_query_buffer_count + qlen > http2_query_buffer_max) {
			lock_basic_unlock(&http2_query_buffer_count_lock);
			verbose(VERB_ALGO, "reset HTTP2 stream, no space left, "
				"in http2-query-buffer-size");
			return http2_submit_rst_stream(h2_session, h2_stream);
		}
		http2_query_buffer_count += qlen;
		lock_basic_unlock(&http2_query_buffer_count_lock);
		if(!(h2_stream->qbuffer = sldns_buffer_new(qlen))) {
			lock_basic_lock(&http2_query_buffer_count_lock);
			http2_query_buffer_count -= qlen;
			lock_basic_unlock(&http2_query_buffer_count_lock);
		}
	}

	if(!h2_stream->qbuffer ||
		sldns_buffer_remaining(h2_stream->qbuffer) < len) {
		verbose(VERB_ALGO, "http2 data_chunck_recv failed. Not enough "
			"buffer space for POST query. Can happen on multi "
			"frame requests without content-length header");
		h2_stream->query_too_large = 1;
		return 0;
	}

	sldns_buffer_write(h2_stream->qbuffer, data, len);

	return 0;
}

void http2_req_stream_clear(struct http2_stream* h2_stream)
{
	if(h2_stream->qbuffer) {
		lock_basic_lock(&http2_query_buffer_count_lock);
		http2_query_buffer_count -=
			sldns_buffer_capacity(h2_stream->qbuffer);
		lock_basic_unlock(&http2_query_buffer_count_lock);
		sldns_buffer_free(h2_stream->qbuffer);
		h2_stream->qbuffer = NULL;
	}
	if(h2_stream->rbuffer) {
		lock_basic_lock(&http2_response_buffer_count_lock);
		http2_response_buffer_count -=
			sldns_buffer_capacity(h2_stream->rbuffer);
		lock_basic_unlock(&http2_response_buffer_count_lock);
		sldns_buffer_free(h2_stream->rbuffer);
		h2_stream->rbuffer = NULL;
	}
}

nghttp2_session_callbacks* http2_req_callbacks_create(void)
{
	nghttp2_session_callbacks *callbacks;
	if(nghttp2_session_callbacks_new(&callbacks) == NGHTTP2_ERR_NOMEM) {
		log_err("failed to initialize nghttp2 callback");
		return NULL;
	}
	/* reception of header block started, used to create h2_stream */
	nghttp2_session_callbacks_set_on_begin_headers_callback(callbacks,
		http2_req_begin_headers_cb);
	/* complete frame received, used to get data from stream if frame
	 * has end stream flag, and start processing query */
	nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks,
		http2_req_frame_recv_cb);
	/* get request info from headers */
	nghttp2_session_callbacks_set_on_header_callback(callbacks,
		http2_req_header_cb);
	/* get data from DATA frames, containing POST query */
	nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks,
		http2_req_data_chunk_recv_cb);

	/* generic HTTP2 callbacks */
	nghttp2_session_callbacks_set_recv_callback(callbacks, http2_recv_cb);
	nghttp2_session_callbacks_set_send_callback(callbacks, http2_send_cb);
	nghttp2_session_callbacks_set_on_stream_close_callback(callbacks,
		http2_stream_close_cb);

	return callbacks;
}
#endif /* HAVE_NGHTTP2 */
