/*
  This software is available to you under a choice of one of two
  licenses.  You may choose to be licensed under the terms of the GNU
  General Public License (GPL) Version 2, available at
  <http://www.fsf.org/copyleft/gpl.html>, or the OpenIB.org BSD
  license, available in the LICENSE.TXT file accompanying this
  software.  These details are also available at
  <http://openib.org/license.html>.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  Copyright (c) 2004 Topspin Communications.  All rights reserved.
  Copyright (c) 2005-2006 Mellanox Technologies Ltd.  All rights reserved.

  $Id$
*/

/*
 * system includes
 */
#if HAVE_CONFIG_H
#  include <config.h>
#endif							/* HAVE_CONFIG_H */

#ifdef SOLARIS_BUILD
/* Our prototypes for ioctl, get*name and accept do not strictly
   match the headers - we use the following lines to move the header
   versions 'out of the way' temporarily. */
#define ioctl __real_ioctl
#define getsockname __real_getsockname
#define getpeername __real_getpeername
#define accept __real_accept
#define FASYNC 0
#include <libgen.h>
#endif
#ifdef __FreeBSD__
#include <libgen.h>
#endif
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __USE_GNU
#define _GNU_SOURCE				/* define RTLD_NEXT */
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/poll.h>
#ifdef __linux__
#include <sys/epoll.h>
#endif

#ifdef SOLARIS_BUILD
/* We're done protecting ourselves from the header prototypes */
#undef ioctl
#undef getsockname
#undef getpeername
#undef accept
#endif

/*
 * SDP specific includes
 */
#include "libsdp.h"

/* We can not use sizeof(sockaddr_in6) as the extra scope_id field is not a must have */
#define IPV6_ADDR_IN_MIN_LEN 24

/* setsockopt() level and optname declarations */
#define SOL_SDP		1025
#define SDP_UNBIND	259			/* Unbind socket */

/* Solaris has two entry socket creation functions */
#define SOCKET_SEMANTIC_DEFAULT 0
#define SOCKET_SEMANTIC_XNET 1

/* HACK: filter ioctl errors for FIONREAD */
#define FIONREAD 0x541B

void __attribute__ ((constructor)) __sdp_init(void);
void __attribute__ ((destructor)) __sdp_fini(void);

/* --------------------------------------------------------------------- */
/* library type definitions.                                             */
/* --------------------------------------------------------------------- */

typedef int (*ioctl_func_t) (int fd,
							 int request,
							 void *arg0,
							 void *arg1,
							 void *arg2,
							 void *arg3,
							 void *arg4, void *arg5, void *arg6, void *arg7);

typedef int (*fcntl_func_t) (int fd, int cmd, ...);

typedef int (*socket_func_t) (int domain, int type, int protocol);

typedef int (*setsockopt_func_t) (int s,
								  int level,
								  int optname,
								  const void *optval, socklen_t optlen);

typedef int (*connect_func_t) (int sockfd,
							   const struct sockaddr * serv_addr,
							   socklen_t addrlen);

typedef int (*listen_func_t) (int s, int backlog);

typedef int (*bind_func_t) (int sockfd,
							const struct sockaddr * my_addr, socklen_t addrlen);

typedef int (*close_func_t) (int fd);

typedef int (*dup_func_t) (int fd);

typedef int (*dup2_func_t) (int oldfd, int newfd);

typedef int (*getsockname_func_t) (int fd,
								   struct sockaddr * name, socklen_t * namelen);

typedef int (*getpeername_func_t) (int fd,
								   struct sockaddr * name, socklen_t * namelen);

typedef int (*accept_func_t) (int fd,
							  struct sockaddr * addr, socklen_t * addrlen);

typedef int (*select_func_t) (int n,
							  fd_set * readfds,
							  fd_set * writefds,
							  fd_set * exceptfds, struct timeval * timeout);

typedef int (*pselect_func_t) (int n,
							   fd_set * readfds,
							   fd_set * writefds,
							   fd_set * exceptfds,
							   const struct timespec * timeout,
							   const sigset_t * sigmask);

typedef int (*poll_func_t) (struct pollfd * ufds,
							unsigned long int nfds, int timeout);

#ifdef __linux__
typedef int (*epoll_create_func_t) (int size);

typedef int (*epoll_ctl_func_t) (int epfd,
								 int op, int fd, struct epoll_event * event);

typedef int (*epoll_wait_func_t) (int epfd,
								  struct epoll_event * events,
								  int maxevents, int timeout);

typedef int (*epoll_pwait_func_t) (int epfd,
								   struct epoll_event * events,
								   int maxevents,
								   int timeout, const sigset_t * sigmask);
#endif


struct socket_lib_funcs {
	ioctl_func_t ioctl;
	fcntl_func_t fcntl;
	socket_func_t socket;
	setsockopt_func_t setsockopt;
	connect_func_t connect;
	listen_func_t listen;
	bind_func_t bind;
	close_func_t close;
	dup_func_t dup;
	dup2_func_t dup2;
	getpeername_func_t getpeername;
	getsockname_func_t getsockname;
	accept_func_t accept;
	select_func_t select;
	pselect_func_t pselect;
	poll_func_t poll;
#ifdef __linux__
	epoll_create_func_t epoll_create;
	epoll_ctl_func_t epoll_ctl;
	epoll_wait_func_t epoll_wait;
	epoll_pwait_func_t epoll_pwait;
#endif
};								/* socket_lib_funcs */

#ifdef SOLARIS_BUILD
/* Solaris has another interface to socket functions prefixed with __xnet_ */
struct socket_lib_xnet_funcs {
	socket_func_t socket;
	connect_func_t connect;
	listen_func_t listen;
	bind_func_t bind;
};
#endif

static int simple_sdp_library;
static int max_file_descriptors;
static int dev_null_fd;
volatile static int init_status = 0;	/* 0: idle, 1:during, 2:ready */

/* --------------------------------------------------------------------- */
/* library static and global variables                                   */
/* --------------------------------------------------------------------- */

/* glibc provides these symbols - for Solaris builds we fake them
 * until _init is called, at which point we quiz libdl.. */
#if defined(SOLARIS_BUILD) || defined(__FreeBSD__)
char *program_invocation_name = "[progname]", *program_invocation_short_name =
	"[short_progname]";
#else
extern char *program_invocation_name, *program_invocation_short_name;
#endif

#ifdef RTLD_NEXT
static void *__libc_dl_handle = RTLD_NEXT;
#else
static void *__libc_dl_handle;
#endif

/* extra fd attributes we need for our algorithms */
struct sdp_extra_fd_attributes {
	int shadow_fd;				/* file descriptor of shadow sdp socket    */
	short last_accept_was_tcp;	/* used by accept to alternate tcp and sdp */
	short is_sdp;				/* 1 if the fd represents an sdp socket    */
};								/* sdp_extra_fd_attributes */

/* stores the extra attributes struct by fd */
static struct sdp_extra_fd_attributes *libsdp_fd_attributes;

static struct socket_lib_funcs _socket_funcs = {
	.socket = NULL,
	/* Automatically sets all other elements to NULL */
};								/* _socket_funcs */

#ifdef SOLARIS_BUILD
static struct socket_lib_xnet_funcs _socket_xnet_funcs = {
	.socket = NULL,
	/* Automatically sets all other elements to NULL */
};
#endif

/* --------------------------------------------------------------------- */
/* Prototypes                                                            */
/* --------------------------------------------------------------------- */
void __sdp_init(void);

/* --------------------------------------------------------------------- */
/*                                                                       */
/* local static functions.                                               */
/*                                                                       */
/* --------------------------------------------------------------------- */

/* ========================================================================= */
/*..init_extra_attribute -- initialize the set of extra attributes for a fd */
static void init_extra_attribute(int fd)
{
	if ((0 <= fd) && (max_file_descriptors > fd)) {
		libsdp_fd_attributes[fd].shadow_fd = -1;
		libsdp_fd_attributes[fd].is_sdp = 0;
		libsdp_fd_attributes[fd].last_accept_was_tcp = -1;
	}
}

static inline int is_valid_fd(int fd)
{
	return (0 <= fd) && (fd < max_file_descriptors);
}

/* ========================================================================= */
/*..get_shadow_fd_by_fd -- given an fd return its shadow fd if exists        */
static inline int get_shadow_fd_by_fd(int fd)
{
	if (is_valid_fd(fd))
		return libsdp_fd_attributes[fd].shadow_fd;
	else
		return -1;
}

/* ========================================================================= */
/*..set_shadow_for_fd --                                                     */
static inline void set_shadow_for_fd(int fd, int shadow_fd)
{
	if (is_valid_fd(fd))
		libsdp_fd_attributes[fd].shadow_fd = shadow_fd;
}

/* ========================================================================= */
/*..set_is_sdp_socket --                                                     */
static inline void set_is_sdp_socket(int fd, short is_sdp)
{
	if (is_valid_fd(fd))
		libsdp_fd_attributes[fd].is_sdp = is_sdp;
}

/* ========================================================================= */
/*..get_is_sdp_socket -- given an fd return 1 if it is an SDP socket         */
static inline int get_is_sdp_socket(int fd)
{
	if (is_valid_fd(fd))
		return libsdp_fd_attributes[fd].is_sdp;
	else
		return 0;
}

/* ========================================================================= */
/*..last_accept_was_tcp -- given an fd return 1 if last accept was tcp       */
static inline int last_accept_was_tcp(int fd)
{
	if (is_valid_fd(fd))
		return libsdp_fd_attributes[fd].last_accept_was_tcp;
	else
		return 0;
}

/* ========================================================================= */
/*..set_last_accept -- given an fd set last accept was tcp                   */
static inline void set_last_accept(int fd, int was_tcp)
{
	if (is_valid_fd(fd))
		libsdp_fd_attributes[fd].last_accept_was_tcp = was_tcp;
}

/* ========================================================================= */
/*..cleanup_shadow -- an error occured on an SDP socket, cleanup             */
static int cleanup_shadow(int fd)
{
	int shadow_fd = get_shadow_fd_by_fd(fd);

	if (shadow_fd == -1)
		return 0;
	libsdp_fd_attributes[fd].shadow_fd = -1;
	libsdp_fd_attributes[fd].last_accept_was_tcp = 0;
	return (_socket_funcs.close(shadow_fd));
}								/* cleanup_shadow */

/* ========================================================================= */
/*..replace_fd_with_its_shadow -- perform all required for such promotion    */
static int replace_fd_with_its_shadow(int fd)
{
	int shadow_fd = libsdp_fd_attributes[fd].shadow_fd;

	if (shadow_fd == -1) {
		__sdp_log(8, "Error replace_fd_with_its_shadow: no shadow for fd:%d\n",
				  fd);
		return EINVAL;
	}

	/* copy the attributes of the shadow before we clean them up */
	libsdp_fd_attributes[fd] = libsdp_fd_attributes[shadow_fd];
	libsdp_fd_attributes[fd].shadow_fd = -1;
	if (_socket_funcs.dup2(shadow_fd, fd) < 0) {
		init_extra_attribute(fd);
		_socket_funcs.close(shadow_fd);
		return EINVAL;
	}
	_socket_funcs.close(shadow_fd);
	return 0;
}

static sa_family_t get_sdp_domain(int domain)
{
	if (AF_INET_SDP == domain || AF_INET6_SDP == domain)
		return domain;

	if (AF_INET == domain)
		return AF_INET_SDP;
	else if (AF_INET6 == domain)
		return AF_INET6_SDP;

	__sdp_log(8, "Error %s: unknown TCP domain: %d\n", __func__, domain);

	return -1;
}

static int get_sock_domain(int sd)
{
	struct sockaddr_storage tmp_sin;
	socklen_t tmp_sinlen = sizeof(tmp_sin);

	if (_socket_funcs.getsockname(sd, (struct sockaddr *) &tmp_sin, &tmp_sinlen) < 0) {
		__sdp_log(8, "Error %s: getsockname return <%d> for socket\n", __func__, errno);
		return -1;
	}

	return ((struct sockaddr *)&tmp_sin)->sa_family;
}

/* ========================================================================= */
/*..is_filtered_unsuported_sockopt -- return 1 if to filter sockopt failure  */
static inline int is_filtered_unsuported_sockopt(int level, int optname)
{
	/* 
	 * TODO: until we know exactly which unsupported opts are really 
	 * a don't care we always pass the error
	 */
	return 0;
#if 0
	/* these are the SOL_TCP OPTS we should consider filterring */
	TCP_NODELAY 1				/* Don't delay send to coalesce packets  */
		TCP_MAXSEG 2			/* Set maximum segment size  */
		TCP_CORK 3				/* Control sending of partial frames  */
		TCP_KEEPIDLE 4			/* Start keeplives after this period */
		TCP_KEEPINTVL 5			/* Interval between keepalives */
		TCP_KEEPCNT 6			/* Number of keepalives before death */
		TCP_SYNCNT 7			/* Number of SYN retransmits */
		TCP_LINGER2 8			/* Life time of orphaned FIN-WAIT-2 state */
		TCP_DEFER_ACCEPT 9		/* Wake up listener only when data arrive */
		TCP_WINDOW_CLAMP 10		/* Bound advertised window */
		TCP_INFO 11				/* Information about this connection. */
		TCP_QUICKACK 12			/* Bock/reenable quick ACKs.  */
#endif
}

/* ========================================================================= */
/*..is_invalid_addr -- return 1 if given pointer is not valid                */
/* NOTE: invalidation of the size is going to happen during actual call      */
static inline int is_invalid_addr(const void *p)
{
	/* HACK: on some systems we can not write to check for pointer validity */
	size_t ret = fcntl(dev_null_fd, F_GETLK, p);

	ret = (errno == EFAULT);
	errno = 0;
	return ret;
}

/* ========================================================================= */
/*..get_addr_str -- fill in the given buffer with addr str or return 1       */
static int get_addr_str(const struct sockaddr *addr, char *buf, size_t len)
{
	const struct sockaddr_in *sin = (struct sockaddr_in *) addr;
	const struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) addr;
	char const *conv_res;

	if (sin->sin_family == AF_INET) {
		conv_res = inet_ntop(AF_INET, (void *) &(sin->sin_addr), buf, len);
	} else if (sin6->sin6_family == AF_INET6) {
		conv_res = inet_ntop(AF_INET6, (void *) &sin6->sin6_addr, buf, len);
	} else {
		strncpy(buf, "unknown address family", len);
		conv_res = (char *) 1;
	}
	return conv_res == NULL;
}

/* --------------------------------------------------------------------- */
/*                                                                       */
/* Socket library function overrides.                                    */
/*                                                                       */
/* --------------------------------------------------------------------- */

/* ========================================================================= */
/*..ioctl -- replacement ioctl call. */
int
ioctl(int fd,
	  int request,
	  void *arg0,
	  void *arg1,
	  void *arg2, void *arg3, void *arg4, void *arg5, void *arg6, void *arg7)
{
	int shadow_fd;
	int sret = 0;
	int ret = 0;

	if (init_status == 0)
		__sdp_init();

	if (NULL == _socket_funcs.ioctl) {
		__sdp_log(9, "Error ioctl: no implementation for ioctl found\n");
		return -1;
	}

	shadow_fd = get_shadow_fd_by_fd(fd);

	__sdp_log(2, "IOCTL: <%s:%d:%d> request <%d>\n",
			  program_invocation_short_name, fd, shadow_fd, request);

	ret = _socket_funcs.ioctl(fd, request,
							  arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);

	/* HACK: avoid failing on FIONREAD error as SDP does not support it at the moment */
	if ((ret < 0) && get_is_sdp_socket(fd) && (request == FIONREAD)) {
		__sdp_log(8, "Warning ioctl: "
				  "Ignoring FIONREAD error for SDP socket.\n");
		ret = 0;
	}

	/* if shadow and no error on tcp */
	if ((ret >= 0) && (-1 != shadow_fd)) {
		sret = _socket_funcs.ioctl(shadow_fd, request,
								   arg0, arg1, arg2, arg3, arg4, arg5, arg6,
								   arg7);
		/* HACK: avoid failing on FIONREAD error as SDP does not support it at the moment */
		if ((sret < 0) && (request == FIONREAD)) {
			__sdp_log(8, "Warning ioctl: "
					  "Ignoring FIONREAD error for shadow SDP socket.\n");
			sret = 0;
		}

		if (sret < 0) {
			__sdp_log(8, "Error ioctl: "
					  "<%d> calling ioctl for SDP socket, closing it.\n",
					  errno);
			cleanup_shadow(fd);
		}
	}

	__sdp_log(2, "IOCTL: <%s:%d:%d> result <%d:%d>\n",
			  program_invocation_short_name, fd, shadow_fd, ret, sret);

	return ret;
}								/* ioctl */

/* ========================================================================= */
/*..fcntl -- replacement fcntl call.                                         */
int fcntl(int fd, int cmd, ...)
{
	int shadow_fd;
	int sret = 0;
	int ret = 0;

	void *arg;
	va_list ap;

	va_start(ap, cmd);
	arg = va_arg(ap, void *);
	va_end(ap);


	if (init_status == 0)
		__sdp_init();

	if (NULL == _socket_funcs.fcntl) {
		__sdp_log(9, "Error fcntl: no implementation for fcntl found\n");
		return -1;
	}

	shadow_fd = get_shadow_fd_by_fd(fd);

	__sdp_log(2, "FCNTL: <%s:%d:%d> command <%d> argument <%p>\n",
			  program_invocation_short_name, fd, shadow_fd, cmd, arg);

	ret = _socket_funcs.fcntl(fd, cmd, arg);
	if ((ret >= 0) && (-1 != shadow_fd)) {
		sret = _socket_funcs.fcntl(shadow_fd, cmd, arg);
		if (sret < 0) {
			__sdp_log(8, "Error fcntl:"
					  " <%d> calling fcntl(%d, %d, %p) for SDP socket. Closing it.\n",
					  shadow_fd, cmd, arg, errno);
			cleanup_shadow(fd);
		}
	}

	__sdp_log(2, "FCNTL: <%s:%d:%d> result <%d:%d>\n",
			  program_invocation_short_name, fd, shadow_fd, ret, sret);

	return ret;
}								/* fcntl */

/* ========================================================================= */
/*..setsockopt -- replacement setsockopt call.                               */
int
setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	int shadow_fd;
	int sret = 0;
	int ret = 0;

	if (init_status == 0)
		__sdp_init();

	if (NULL == _socket_funcs.setsockopt) {
		__sdp_log(9, "Error setsockopt:"
				  " no implementation for setsockopt found\n");
		return -1;
	}

	shadow_fd = get_shadow_fd_by_fd(fd);

	__sdp_log(2, "SETSOCKOPT: <%s:%d:%d> level <%d> name <%d>\n",
			  program_invocation_short_name, fd, shadow_fd, level, optname);

	if (level == SOL_SOCKET && optname == SO_KEEPALIVE && get_is_sdp_socket(fd)) {
		level = AF_INET_SDP;
		__sdp_log(2, "SETSOCKOPT: <%s:%d:%d> substitute level %d\n",
				  program_invocation_short_name, fd, shadow_fd, level);
	}

	ret = _socket_funcs.setsockopt(fd, level, optname, optval, optlen);
	if ((ret >= 0) && (shadow_fd != -1)) {
		if (level == SOL_SOCKET && optname == SO_KEEPALIVE &&
			get_is_sdp_socket(shadow_fd)) {
			level = AF_INET_SDP;
			__sdp_log(2, "SETSOCKOPT: <%s:%d:%d> substitute level %d\n",
					  program_invocation_short_name, fd, shadow_fd, level);
		}

		sret = _socket_funcs.setsockopt(shadow_fd, level, optname, optval, optlen);
		if (sret < 0) {
			__sdp_log(8, "Warning sockopts:"
					  " ignoring error on shadow SDP socket fd:<%d>\n", fd);
			/* 
			 * HACK: we should allow some errors as some sock opts are unsupported  
			 * __sdp_log(8, "Error %d calling setsockopt for SDP socket, closing\n", errno); 
			 * cleanup_shadow(fd); 
			 */
		}
	}

	/* Due to SDP limited implmentation of sockopts we ignore some errors */
	if ((ret < 0) && get_is_sdp_socket(fd) &&
		is_filtered_unsuported_sockopt(level, optname)) {
		__sdp_log(8, "Warning sockopts: "
				  "ignoring error on non implemented sockopt on SDP socket"
				  " fd:<%d> level:<%d> opt:<%d>\n", fd, level, optval);
		ret = 0;
	}

	__sdp_log(2, "SETSOCKOPT: <%s:%d:%d> result <%d:%d>\n",
			  program_invocation_short_name, fd, shadow_fd, ret, sret);

	return ret;
}								/* setsockopt */

/* ========================================================================= */
/*..socket -- replacement socket call.                                       */

static inline int __create_socket_semantic(int domain,
										   int type,
										   int protocol, int semantics)
{
	return
#ifdef SOLARIS_BUILD
		(semantics == SOCKET_SEMANTIC_XNET) ?
		_socket_xnet_funcs.socket(domain, type, protocol) :
#endif
		_socket_funcs.socket(domain, type, protocol);
}

/* Contains the main logic for creating shadow SDP sockets */
static int __create_socket(int domain, int type, int protocol, int semantics)
{
	int s = -1;
	int shadow_fd = -1;
	use_family_t family_by_prog;
	int sdp_domain;

	if (init_status == 0)
		__sdp_init();

	if (NULL == _socket_funcs.socket) {
		__sdp_log(9, "Error socket: no implementation for socket found\n");
		return -1;
	}

	__sdp_log(2, "SOCKET: <%s> domain <%d> type <%d> protocol <%d>\n",
			  program_invocation_short_name, domain, type, protocol);

	if (!(AF_INET == domain || AF_INET6 == domain ||
			AF_INET_SDP == domain || AF_INET6_SDP == domain)) {
		__sdp_log(1, "SOCKET: making other socket\n");
		s = __create_socket_semantic(domain, type, protocol, semantics);
		goto done;
	}

	sdp_domain = get_sdp_domain(domain);
	if (sdp_domain < 0) {
		errno = EAFNOSUPPORT;
		s = -1;
		goto done;
	}

	/* check to see if we can skip the shadow */
	if ((AF_INET == domain || AF_INET6 == domain) && (SOCK_STREAM == type))
		if (simple_sdp_library)
			family_by_prog = USE_SDP;
		else
			family_by_prog = __sdp_match_by_program();
	else if (AF_INET_SDP == domain || AF_INET6_SDP == domain)
		family_by_prog = USE_SDP;
	else
		family_by_prog = USE_TCP;

	if (family_by_prog == USE_TCP) {
		__sdp_log(1, "SOCKET: making TCP only socket (no shadow)\n");
		s = __create_socket_semantic(domain, type, protocol, semantics);
		init_extra_attribute(s);
		set_is_sdp_socket(s, 0);
		goto done;
	}

	if (family_by_prog == USE_SDP) {
		/* HACK: convert the protocol if IPPROTO_IP */
		if (protocol == 0)
			protocol = IPPROTO_TCP;

		__sdp_log(1, "SOCKET: making SDP socket type:%d proto:%d\n",
				  type, protocol);
		s = __create_socket_semantic(sdp_domain, type, protocol, semantics);
		init_extra_attribute(s);
		set_is_sdp_socket(s, 1);
		goto done;
	}

	/* HACK: if we fail creating the TCP socket should we abort ? */
	__sdp_log(1, "SOCKET: making TCP socket\n");
	s = __create_socket_semantic(domain, type, protocol, semantics);
	init_extra_attribute(s);
	set_is_sdp_socket(s, 0);
	if (is_valid_fd(s)) {
		if (((AF_INET == domain) || (AF_INET6 == domain)) &&
			(SOCK_STREAM == type)) {

			if (protocol == 0)
				protocol = IPPROTO_TCP;
			__sdp_log(1, "SOCKET: making SDP shadow socket type:%d proto:%d\n",
					  type, protocol);
			shadow_fd =
				__create_socket_semantic(sdp_domain, type, protocol,
										 semantics);
			if (is_valid_fd(shadow_fd)) {
				init_extra_attribute(shadow_fd);
				if (libsdp_fd_attributes[s].shadow_fd != -1) {
					__sdp_log(8, "Warning socket: "
							  "overriding existing shadow fd:%d for fd:%d\n",
							  libsdp_fd_attributes[s].shadow_fd, s);
				}
				set_is_sdp_socket(shadow_fd, 1);
				set_shadow_for_fd(s, shadow_fd);
			} else {
				__sdp_log(8,
						  "Error socket: <%d> calling socket for SDP socket\n",
						  errno);
				/* fail if we did not make the SDP socket */
				__sdp_log(1, "SOCKET: closing TCP socket:<%d>\n", s);
				_socket_funcs.close(s);
				s = -1;
			}
		}
	} else {
		__sdp_log(8, "Error socket: "
				  "ignoring SDP socket since TCP fd:%d out of range\n", s);
	}

done:
	__sdp_log(2, "SOCKET: <%s:%d:%d>\n",
			  program_invocation_short_name, s, shadow_fd);

	return s;
}								/* socket */

int socket(int domain, int type, int protocol)
{
	return __create_socket(domain, type, protocol, SOCKET_SEMANTIC_DEFAULT);
}

#ifdef SOLARIS_BUILD
int __xnet_socket(int domain, int type, int protocol)
{
	return __create_socket(domain, type, protocol, SOCKET_SEMANTIC_XNET);
}
#endif

/* ========================================================================= */
/*..get_fd_addr_port_num - obtain the port the fd is attached to             */
static int get_fd_addr_port_num(int sd)
{
	struct sockaddr_storage addr;
	int ret;
	const struct sockaddr_in *sin;
	socklen_t addrlen = sizeof(addr);

	ret = _socket_funcs.getsockname(sd, (struct sockaddr *) &addr, &addrlen);

	if (ret) {
		__sdp_log(8, "Error: in get_fd_addr_port_num - Failed to get getsockname\n");
		return -1;
	}

	/* port num is in same location for IPv4 and IPv6 */
	sin = (const struct sockaddr_in *) &addr;
	return ntohs(sin->sin_port);
}

/* ========================================================================= */
/*..set_addr_port_num - sets the port in the given address                   */
static int set_addr_port_num(const struct sockaddr *addr, int port)
{
	struct sockaddr_in *sin = (struct sockaddr_in *) addr;

	/* port num is in same location for IPv4 and IPv6 */
	sin->sin_port = htons(port);
	return 0;
}

/* ========================================================================= */
/*  perform a bind with the given socket semantics                           */
static inline int
__bind_semantics(int fd,
				 const struct sockaddr *my_addr,
				 socklen_t addrlen, int semantics)
{
	return
#ifdef SOLARIS_BUILD
		(semantics == SOCKET_SEMANTIC_XNET) ?
		_socket_xnet_funcs.bind(fd, my_addr, addrlen) :
#endif
		_socket_funcs.bind(fd, my_addr, addrlen);
}

/* ========================================================================= */
/*..find_free_port - find same free port on both TCP and SDP                 */
#define MAX_BIND_ANY_PORT_TRIES 20000
static int
find_free_port(const struct sockaddr *sin_addr,
			   const socklen_t addrlen,
			   int orig_sd,
			   int *sdp_sd, int *tcp_sd, int semantics)
{
	static int tcp_turn = 1;
	int tmp_turn = tcp_turn;
	int num_of_loops = 0;
	int port = -1;
	int tmp_sd[2];
	unsigned int yes = 1;
	int ret;
	int domain, sdp_domain;

	__sdp_log(2, "find_free_port: starting search for common free port\n");

	/* need to obtain the address family from the fd */
	domain = get_sock_domain(orig_sd);
	if (domain == -1) {
		errno = EFAULT;
		goto done;
	}

	sdp_domain = get_sdp_domain(domain);
	if (sdp_domain < 0) {
		errno = EFAULT;
		goto done;
	}

	do {
		__sdp_log(1, "find_free_port: taking loop (%d)\n", ++num_of_loops);

		__sdp_log(1, "find_free_port: creating the two sockets\n");
		tmp_sd[0] = _socket_funcs.socket(sdp_domain, SOCK_STREAM, IPPROTO_TCP);
		if (tmp_sd[0] < 0) {
			__sdp_log(8, "Warning find_free_port: creating first socket failed\n");
			goto done;
		}

		_socket_funcs.setsockopt(tmp_sd[0], SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

		tmp_sd[1] = _socket_funcs.socket(domain, SOCK_STREAM, IPPROTO_TCP);
		if (tmp_sd[1] < 0) {
			__sdp_log(8, "Warning find_free_port: creating second socket failed\n");
			_socket_funcs.close(tmp_sd[0]);
			goto done;
		}

		_socket_funcs.setsockopt(tmp_sd[1], SOL_SOCKET, SO_REUSEADDR, &yes,
								 sizeof(yes));

		__sdp_log(1, "find_free_port: binding first %s socket\n",
				  tmp_turn ? "tcp" : "sdp");
		ret = __bind_semantics(tmp_sd[tmp_turn], sin_addr, addrlen, semantics);
		if (ret < 0) {
			__sdp_log(8,
					  "Warning find_free_port: binding first socket failed:%s\n",
					  strerror(errno));
			_socket_funcs.close(tmp_sd[0]);
			_socket_funcs.close(tmp_sd[1]);
			goto done;
		}

		__sdp_log(1, "find_free_port: listening on first socket\n");
		ret = _socket_funcs.listen(tmp_sd[tmp_turn], 5);
		if (ret < 0) {
			__sdp_log(8, "Warning find_free_port: listening on first socket failed:%s\n",
					strerror(errno));
			_socket_funcs.close(tmp_sd[0]);
			_socket_funcs.close(tmp_sd[1]);
			goto done;
		}

		port = get_fd_addr_port_num(tmp_sd[tmp_turn]);
		if (port < 0) {
			__sdp_log(8, "Warning find_free_port: first socket port:%d < 0\n",
					port);
			_socket_funcs.close(tmp_sd[0]);
			_socket_funcs.close(tmp_sd[1]);
			goto done;
		}
		__sdp_log(1, "find_free_port: first socket port:%u\n", port);

		set_addr_port_num(sin_addr, port);

		__sdp_log(1, "find_free_port: binding second socket\n");
		ret = __bind_semantics(tmp_sd[1 - tmp_turn], sin_addr, addrlen, semantics);
		if (ret < 0) {
			/* bind() for sdp socket failed. It is acceptable only
			 * if the IP is not part of IB network. */

			if (errno != EADDRINUSE) {
				__sdp_log(8, "Warning find_free_port: "
						  "binding second socket failed with %s\n",
						  strerror(errno));
				goto close_and_mark;
			} else {
				int err;
#ifdef __linux__
				socklen_t len = sizeof(int);

				ret = getsockopt(tmp_sd[1 - tmp_turn], SOL_TCP,
								 SDP_LAST_BIND_ERR, &err, &len);
				if (-1 == ret) {
					__sdp_log(8, "Error %s:getsockopt: %s\n",
							  __func__, strerror(errno));
					goto close_and_mark;
				}
#else
				err = -errno;
#endif
				if (-ENOENT == err || -EADDRINUSE != err) {
					/* bind() failed due to either:
					 * 1. IP is ETH, not IB, so can't bind() to sdp socket.
					 * 2. real error.
					 * Continue only with TCP */
					goto close_and_mark;
				}
				__sdp_log(1, "find_free_port: %s port %u was busy\n",
					  1 - tmp_turn ? "tcp" : "sdp",
					  ntohs(((const struct sockaddr_in *)sin_addr)->sin_port));
			}

			/* close the sockets - we will need new ones ... */
			__sdp_log(1,
					  "find_free_port: closing the two sockets before next loop\n");
			_socket_funcs.close(tmp_sd[0]);
			_socket_funcs.close(tmp_sd[1]);

			port = -1;
			/* we always start with tcp so we keep the original setting for now */
			/* tmp_turn = 1 - tmp_turn; */
		}

	} while ((port < 0) && (num_of_loops < MAX_BIND_ANY_PORT_TRIES));

setfds:
	tcp_turn = tmp_turn;
	*sdp_sd = tmp_sd[0];
	*tcp_sd = tmp_sd[1];

done:
	__sdp_log(2, "find_free_port: return port:<%d>\n", port);
	return port;

close_and_mark:
	_socket_funcs.close(tmp_sd[0]);
	tmp_sd[0] = -1;				/* mark with error */
	goto setfds;

}

/* ========================================================================= */
/*..check_legal_bind - check if given address is okay for both TCP and SDP   */
static int
check_legal_bind(const struct sockaddr *sin_addr,
				 const socklen_t addrlen,
				 int orig_sd,
				 int *sdp_sd, int *tcp_sd, int semantics)
{
	unsigned int yes = 1;
	int ret = -1;
	int sret = -1;
	int domain, sdp_domain;

	/* need to obtain the address family from the fd */
	domain = get_sock_domain(orig_sd);
	if (domain == -1) {
		errno = EFAULT;
		ret = -1;
		goto done;
	}

	sdp_domain = get_sdp_domain(domain);
	if (sdp_domain < 0) {
		errno = EFAULT;
		goto done;
	}

	__sdp_log(2, "check_legal_bind: binding two temporary sockets\n");
	*sdp_sd = _socket_funcs.socket(sdp_domain, SOCK_STREAM, IPPROTO_TCP);
	if (*sdp_sd < 0) {
		__sdp_log(8, "Error check_legal_bind: " "creating SDP socket failed\n");
		goto done;
	}

	__sdp_log(2, "check_legal_bind: reusing <%d> \n", *sdp_sd);
	sret =
		_socket_funcs.setsockopt(*sdp_sd, SOL_SOCKET, SO_REUSEADDR, &yes,
								 sizeof(yes));
	if (sret < 0) {
		__sdp_log(8, "Error bind: Could not setsockopt sdp_sd\n");
	}

	*tcp_sd = _socket_funcs.socket(domain, SOCK_STREAM, IPPROTO_TCP);
	if (*tcp_sd < 0) {
		__sdp_log(8, "Error check_legal_bind: "
				  "creating second socket failed:%s\n", strerror(errno));
		_socket_funcs.close(*sdp_sd);
		goto done;
	}

	__sdp_log(2, "check_legal_bind: reusing <%d> \n", *tcp_sd);
	sret =
		_socket_funcs.setsockopt(*tcp_sd, SOL_SOCKET, SO_REUSEADDR, &yes,
								 sizeof(yes));
	if (sret < 0) {
		__sdp_log(8, "Error bind: Could not setsockopt tcp_sd\n");
	}

	__sdp_log(1, "check_legal_bind: binding SDP socket\n");
	ret = __bind_semantics(*sdp_sd, sin_addr, addrlen, semantics);
	if (ret < 0) {
		/* bind() for sdp socket failed. It is acceptable only if
		 * the IP is not part of IB network. */
		int err;
		socklen_t len = sizeof(int);

		if (EADDRINUSE != errno)
			goto done;
#ifdef __linux__
		if (-1 == getsockopt(*sdp_sd, SOL_TCP, SDP_LAST_BIND_ERR, &err, &len)) {
			__sdp_log(8, "Error check_legal_bind:getsockopt: %s\n",
					  strerror(errno));
			goto done;
		}
#else
		err = -errno;
#endif
		if (-ENOENT != err) {
			/* bind() failed due to real error. Can't continue */
			__sdp_log(8, "Error check_legal_bind: "
					  "binding SDP socket failed:%s\n", strerror(errno));
			_socket_funcs.close(*sdp_sd);
			_socket_funcs.close(*tcp_sd);

			/* TCP and SDP without library return EINVAL */
			if (errno == EADDRINUSE)
				errno = EINVAL;

			goto done;
		}
		/* IP is ETH, not IB, so can't bind() to sdp socket */
		/* Continue only with TCP */
		_socket_funcs.close(*sdp_sd);
		*sdp_sd = -1;
	}

	__sdp_log(1, "check_legal_bind: binding TCP socket\n");
	ret = __bind_semantics(*tcp_sd, sin_addr, addrlen, semantics);
	if (ret < 0) {
		__sdp_log(8, "Error check_legal_bind: "
				  "binding TCP socket failed:%s\n", strerror(errno));
		if (-1 != *sdp_sd)
			_socket_funcs.close(*sdp_sd);
		_socket_funcs.close(*tcp_sd);
		goto done;
	}
	ret = 0;
	__sdp_log(2, "check_legal_bind: result:<%d>\n", ret);
done:
	return ret;
}

/* ========================================================================= */
/*..close_and_bind - close an open fd and bind another one immediately       */
static int
close_and_bind(int old_sd,
			   int new_sd,
			   const struct sockaddr *addr, socklen_t addrlen, int semantics)
{
	int ret;

	__sdp_log(2, "close_and_bind: closing <%d> binding <%d>\n", old_sd, new_sd);
	ret = _socket_funcs.close(old_sd);
	if (ret < 0) {
		__sdp_log(8, "Error bind: Could not close old_sd\n");
		goto done;
	}

	ret = __bind_semantics(new_sd, addr, addrlen, semantics);
	if (ret < 0)
		__sdp_log(8, "Error bind: Could not bind new_sd\n");

done:
	__sdp_log(2, "close_and_bind: returning <%d>\n", ret);
	return ret;
}

/* ========================================================================= */
/*..bind -- replacement bind call.                                           */
/* 
   As we do not know the role of this socket yet so we cannot choose AF. 
   We need to be able to handle shadow too.
   SDP sockets (may be shadow or not) must be using converted address 
   
	Since there is no way to "rebind" a socket we have to avoid "false" bind:
	1. When the given address for the bind includes a port we need to 
	   guarantee the port is free on both address families. We do that 
      by creating temporary sockets and biding them first. Then we close and
		re-use the address on the real sockets. 
	2. When ANY_PORT is requested we need to make sure the port we obtain from 
	   the first address family is also free on the second one. We use temporary
		sockets for that task too. We loop several times to find such common 
		available socket
*/
static int
__perform_bind(int fd,
			   const struct sockaddr *addr, socklen_t addrlen, int semantics)
{
	int shadow_fd;
	struct sockaddr_in *sin_addr = (struct sockaddr_in *) addr;
	int ret, sret = -1;
	char buf[MAX_ADDR_STR_LEN];

	if (init_status == 0)
		__sdp_init();

	if (NULL == _socket_funcs.bind) {
		__sdp_log(9, "Error bind: no implementation for bind found\n");
		return -1;
	}

	shadow_fd = get_shadow_fd_by_fd(fd);

	if ((addr == NULL) || is_invalid_addr(addr)) {
		errno = EFAULT;
		__sdp_log(8, "Error bind: illegal address provided\n");
		return -1;
	}

	if (get_addr_str(addr, buf, MAX_ADDR_STR_LEN)) {
		__sdp_log(8, "Error bind: provided illegal address: %s\n",
				  strerror(errno));
		return -1;
	}

	__sdp_log(2, "BIND: <%s:%d:%d> type <%d> IP <%s> port <%d>\n",
			  program_invocation_short_name, fd, shadow_fd,
			  sin_addr->sin_family, buf, ntohs(sin_addr->sin_port));

	if (get_is_sdp_socket(fd)) {
		__sdp_log(1, "BIND: binding SDP socket:<%d>\n", fd);
		ret = __bind_semantics(fd, addr, addrlen, semantics);
		goto done;
	} else if (shadow_fd != -1) {
		/* has shadow */
		/* we need to validate the given address or find a common port 
		 * so we use the following tmp address and sockets */
		struct sockaddr_storage tmp_addr;
		int sdp_sd = -1, tcp_sd = -1, port;

		memcpy(&tmp_addr, addr, addrlen);
		ret = 0;
		if (ntohs(sin_addr->sin_port) == 0) {
			/* When we get ANY_PORT we need to make sure that both TCP 
			 * and SDP sockets will use the same port */

			port = find_free_port(addr, addrlen, fd, &sdp_sd, &tcp_sd, semantics);
			if (port < 0) {
				ret = -1;
				__sdp_log(9, "BIND: Failed to find common free port\n");
				/* We cannot bind both tcp and sdp on the same port, we will close
				 * the sdp and continue with tcp only */
				goto done;
			} else {
				/* copy the port to the tmp address */
				set_addr_port_num((struct sockaddr *) &tmp_addr, port);
			}
		} else {
			/* have a shadow but requested specific port - check that we 
			 * can actually bind the two addresses and then reuse */
			ret = check_legal_bind(addr, addrlen, fd, &sdp_sd, &tcp_sd, semantics);
			if (ret < 0) {
				__sdp_log(8, "Error bind: "
						  "Provided address can not bind on the two sockets\n");
			}
		}

		/* if we fail to find a common port or given address can not be used 
		 * we return error */
		if (ret < 0) {
			/* Temporary sockets already closed by check_legal_bind or 
			 * find_free_port */
			errno = EADDRINUSE;
			goto done;
		}

		/* close temporary sockets and reuse their address */
		/* HACK: close_and_bind might race with other applications. */
		/* When the race occur we return EADDRINUSE */
		ret = close_and_bind(tcp_sd, fd, (struct sockaddr *) &tmp_addr,
						   addrlen, semantics);
		if (ret < 0) {
			__sdp_log(8, "Error bind: " "Could not close_and_bind TCP side\n");
			if (-1 != sdp_sd)
				_socket_funcs.close(sdp_sd);
			goto done;
		}

		if (-1 != sdp_sd) {
			ret = close_and_bind(sdp_sd, shadow_fd, (struct sockaddr *) &tmp_addr,
							   addrlen, semantics);

			if (ret < 0) {
				__sdp_log(8,
						  "Error bind: " "Could not close_and_bind sdp side\n");
				goto done;
			}
		}
		goto done;
	}

	/* we can only get here on single TCP socket */
	__sdp_log(1, "BIND: binding TCP socket:<%d>\n", fd);
	ret = __bind_semantics(fd, addr, addrlen, semantics);

done:
	__sdp_log(2, "BIND: <%s:%d:%d> result <%d:%d>\n",
			  program_invocation_short_name, fd, shadow_fd, ret, sret);

	return ret;
}								/* bind */


int bind(int fd, const struct sockaddr *my_addr, socklen_t addrlen)
{
	return __perform_bind(fd, my_addr, addrlen, SOCKET_SEMANTIC_DEFAULT);
}

#ifdef SOLARIS_BUILD
int __xnet_bind(int fd, const struct sockaddr *my_addr, socklen_t addrlen)
{
	return __perform_bind(fd, my_addr, addrlen, SOCKET_SEMANTIC_XNET);
}
#endif


/* ========================================================================= */
/*..connect -- replacement connect call.                                     */
/*
  Given the connect address we can take out AF decision                     
  if target AF == both it means SDP and fall back to TCP                   
  if any connect worked we are fine
*/
static inline int
__connect_semantics(int fd,
					const struct sockaddr *serv_addr,
					socklen_t addrlen, int semantics)
{
	return
#ifdef SOLARIS_BUILD
		(semantics == SOCKET_SEMANTIC_XNET) ?
		_socket_xnet_funcs.connect(fd, serv_addr, addrlen) :
#endif
		_socket_funcs.connect(fd, serv_addr, addrlen);
}

static int
__perform_connect(int fd, const struct sockaddr *serv_addr,
		socklen_t addrlen, int semantics)
{
	struct sockaddr_in *serv_sin = (struct sockaddr_in *) serv_addr;
	char buf[MAX_ADDR_STR_LEN];
	int shadow_fd;
	int ret = -1, dup_ret;
	use_family_t target_family;
	int fopts;

	if (init_status == 0)
		__sdp_init();

	if (NULL == _socket_funcs.connect) {
		__sdp_log(9, "Error connect: no implementation for connect found\n");
		return -1;
	}

	shadow_fd = get_shadow_fd_by_fd(fd);

	if ((serv_addr == NULL) || is_invalid_addr(serv_addr)) {
		errno = EFAULT;
		__sdp_log(8, "Error connect: illegal address provided\n");
		return -1;
	}

	if (get_addr_str(serv_addr, buf, MAX_ADDR_STR_LEN)) {
		__sdp_log(8, "Error connect: provided illegal address: %s\n",
				  strerror(errno));
		return EADDRNOTAVAIL;
	}

	__sdp_log(2, "CONNECT: <%s:%d:%d> domain <%d> IP <%s> port <%d>\n",
			  program_invocation_short_name, fd, shadow_fd,
			  serv_sin->sin_family, buf, ntohs(serv_sin->sin_port));


	/* obtain the target address family */
	target_family = __sdp_match_connect(serv_addr, addrlen);

	/* if we do not have a shadow - just do the work */
	if (shadow_fd == -1) {
		__sdp_log(1, "CONNECT: connectingthrough %s\n",
				get_is_sdp_socket(fd) ? "SDP" : "TCP");
		ret = __connect_semantics(fd, serv_addr, addrlen, semantics);
		if ((ret == 0) || (errno == EINPROGRESS)) {
			__sdp_log(7, "CONNECT: connected SDP fd:%d to:%s port %d\n",
					fd, buf, ntohs(serv_sin->sin_port));
		}
		goto done;
	}

	if ((target_family == USE_SDP) || (target_family == USE_BOTH)) {
		/* NOTE: the entire if sequence is negative logic */
		__sdp_log(1, "CONNECT: connecting SDP fd:%d\n", shadow_fd);

		/* make the socket blocking on shadow SDP */
		fopts = _socket_funcs.fcntl(shadow_fd, F_GETFL);
		if ((target_family == USE_BOTH) && (fopts & O_NONBLOCK)) {
			__sdp_log(1,
					  "CONNECT: shadow_fd <%d> will be blocking during connect\n",
					  shadow_fd);
			_socket_funcs.fcntl(shadow_fd, F_SETFL, fopts & (~O_NONBLOCK));
		}

		ret = __connect_semantics(shadow_fd, serv_addr, addrlen, semantics);
		if ((ret < 0) && (errno != EINPROGRESS)) {
			__sdp_log(7, "Error connect: "
					  "failed for SDP fd:%d with error:%m\n", shadow_fd);
		} else {
			__sdp_log(7, "CONNECT: connected SDP fd:%d to:%s port %d\n",
					  fd, buf, ntohs(serv_sin->sin_port));
		}

		/* restore socket options */
		_socket_funcs.fcntl(shadow_fd, F_SETFL, fopts);

		/* if target is SDP or we succeeded we need to dup SDP fd into TCP fd */
		if ((target_family == USE_SDP) || (ret >= 0)) {
			dup_ret = replace_fd_with_its_shadow(fd);
			if (dup_ret < 0) {
				__sdp_log(9, "Error connect: "
						  "failed to dup2 shadow into orig fd:%d\n", fd);
				ret = dup_ret;
			} else {
				/* we can skip the TCP option if we are done */
				__sdp_log(1, "CONNECT: "
						  "matched SDP fd:%d so shadow dup into TCP\n", fd);
				goto done;
			}
		}
	}

	if ((target_family == USE_TCP) || (target_family == USE_BOTH)) {
		__sdp_log(1, "CONNECT: connecting TCP fd:%d\n", fd);
		ret = __connect_semantics(fd, serv_addr, addrlen, semantics);
		if ((ret < 0) && (errno != EINPROGRESS))
			__sdp_log(8, "Error connect: for TCP fd:%d failed with error:%m\n",
					  fd);
		else
			__sdp_log(7, "CONNECT: connected TCP fd:%d to:%s port %d\n",
					  fd, buf, ntohs(serv_sin->sin_port));

		if ((target_family == USE_TCP) || (ret >= 0) || (errno == EINPROGRESS)) {
			if (cleanup_shadow(fd) < 0)
				__sdp_log(8,
						  "Error connect: failed to cleanup shadow for fd:%d\n",
						  fd);
		}
	}

done:
	__sdp_log(2, "CONNECT: <%s:%d:%d> result <%d>\n",
			  program_invocation_short_name, fd, shadow_fd, ret);

	return ret;
}								/* connect */

int connect(int fd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
	return __perform_connect(fd, serv_addr, addrlen, SOCKET_SEMANTIC_DEFAULT);
}

#if defined( SOLARIS_BUILD )
int __xnet_connect(int fd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
	return __perform_connect(fd, serv_addr, addrlen, SOCKET_SEMANTIC_XNET);
}
#endif

/* ========================================================================= */
/*..listen -- replacement listen call.                                       */
/* 
   Now we know our role (passive/server) and our address so we can get AF.
   If both we should try listening on both
*/

static inline int __listen_semantics(int fd, int backlog, int semantics)
{
	return
#ifdef SOLARIS_BUILD
		(semantics == SOCKET_SEMANTIC_XNET) ?
		_socket_xnet_funcs.listen(fd, backlog) :
#endif
		_socket_funcs.listen(fd, backlog);
}

static int __perform_listen(int fd, int backlog, int semantics)
{
	use_family_t target_family;
	int shadow_fd;
	int ret = 0, sret = 0;
	struct sockaddr_storage tmp_sin;
	socklen_t tmp_sinlen = sizeof(tmp_sin);
	struct sockaddr_in *sin4 = (struct sockaddr_in *) &tmp_sin;
	char buf[MAX_ADDR_STR_LEN];
	int actual_port;

	if (init_status == 0)
		__sdp_init();

	if (NULL == _socket_funcs.listen) {
		__sdp_log(9, "Error listen: no implementation for listen found\n");
		return -1;
	}

	shadow_fd = get_shadow_fd_by_fd(fd);
	__sdp_log(2, "LISTEN: <%s:%d:%d>\n",
			  program_invocation_short_name, fd, shadow_fd);

	/* if there is no shadow - simply call listen */
	if (shadow_fd == -1) {
		__sdp_log(1, "LISTEN: calling listen on fd:%d\n", fd);
		ret = __listen_semantics(fd, backlog, semantics);
		goto done;
	}

	/* we need to obtain the address from the fd */
	if (_socket_funcs.getsockname(fd, (struct sockaddr *) &tmp_sin, &tmp_sinlen)
		< 0) {
		__sdp_log(8, "Error listen: getsockname return <%d> for TCP socket\n",
				  errno);
		errno = EADDRNOTAVAIL;
		sret = -1;
		goto done;
	}

	if (get_addr_str((struct sockaddr *) &tmp_sin, buf, MAX_ADDR_STR_LEN)) {
		__sdp_log(8, "Error listen: provided illegal address: %s\n",
				  strerror(errno));
	}

	__sdp_log(2, "LISTEN: <%s:%d:%d> domain <%d> IP <%s> port <%d>\n",
			  program_invocation_short_name, fd, shadow_fd,
			  sin4->sin_family, buf, ntohs(sin4->sin_port));

	target_family =
		__sdp_match_listen((struct sockaddr *) &tmp_sin, sizeof(tmp_sin));

	/* 
	 * in case of an implicit bind and "USE_BOTH" rule we need to first bind the 
	 * two sockets to the same port number 
	 */
	actual_port = ntohs(sin4->sin_port);

	/* do we need to implicit bind both */
	if ((actual_port == 0) && (target_family == USE_BOTH)) {
		int sdp_sd = -1, tcp_sd = -1;

		actual_port = find_free_port((struct sockaddr *) &tmp_sin, tmp_sinlen,
				fd, &sdp_sd, &tcp_sd, semantics);
		if (actual_port < 0) {
			ret = -1;
			__sdp_log(8, "LISTEN: Failed to find common free port. Only TCP will be used.\n");
			target_family = USE_TCP;
		} else {
			/* copy the port to the tmp address */
			set_addr_port_num((struct sockaddr *) sin4, actual_port);

			__sdp_log(2, "LISTEN: BOTH on IP <%s> port <%d>\n",
					  buf, actual_port);
			/* perform the bind */
			ret = close_and_bind(tcp_sd, fd, (struct sockaddr *) sin4,
					tmp_sinlen, semantics);
			if (ret < 0) {
				__sdp_log(8, "Error listen: "
						  "Could not close_and_bind TCP side\n");
			}

			ret = close_and_bind(sdp_sd, shadow_fd, (struct sockaddr *) sin4,
					tmp_sinlen, semantics);
			if (ret < 0) {
				__sdp_log(8, "Error listen: "
						  "Could not close_and_bind SDP side\n");
			}
		}
	}

	if ((target_family == USE_TCP) || (target_family == USE_BOTH)) {
		__sdp_log(1, "LISTEN: calling listen on TCP fd:%d\n", fd);
		ret = __listen_semantics(fd, backlog, semantics);
		if (ret < 0) {
			__sdp_log(8, "Error listen: failed with code <%d> on TCP fd:<%d>\n",
					  errno, fd);
		} else {
			__sdp_log(7, "LISTEN: fd:%d listening on TCP bound to:%s port:%d\n",
					  fd, buf, actual_port);
		}
	}

	if ((target_family == USE_SDP) || (target_family == USE_BOTH)) {
		__sdp_log(1, "LISTEN: calling listen on SDP fd:<%d>\n", shadow_fd);
		sret = __listen_semantics(shadow_fd, backlog, semantics);
		if (sret < 0) {
			__sdp_log(8, "Error listen: failed with code <%d> SDP fd:<%d>\n",
					  errno, shadow_fd);
		} else {
			__sdp_log(7, "LISTEN: fd:%d listening on SDP bound to:%s port:%d\n",
					  fd, buf, actual_port);
		}
	}

	/* cleanup the un-needed shadow if TCP and did not fail */
	if ((target_family == USE_TCP) && (ret >= 0)) {
		__sdp_log(1, "LISTEN: cleaning up shadow SDP\n");
		if (cleanup_shadow(fd) < 0)
			__sdp_log(8, "Error listen: failed to cleanup shadow for fd:%d\n",
					  fd);
	}

	/* cleanup the TCP socket and replace with SDP */
	if ((target_family == USE_SDP) && (sret >= 0)) {
		__sdp_log(1, "LISTEN: cleaning TCP socket and dup2 SDP into it\n");
		if (0 > (sret = replace_fd_with_its_shadow(fd)))
			__sdp_log(9, "Error listen: "
					  "failed to dup2 shadow into orig fd:%d\n", fd);
	}

done:
	__sdp_log(2, "LISTEN: <%s:%d:%d> result <%d>\n",
			  program_invocation_short_name, fd, shadow_fd, ret);
	/* its a success only if both are ok */
	if (ret < 0)
		return (ret);
	if (sret < 0)
		return (sret);
	return 0;
}								/* listen */

int listen(int fd, int backlog)
{
	return __perform_listen(fd, backlog, SOCKET_SEMANTIC_DEFAULT);
}

#ifdef SOLARIS_BUILD
int __xnet_listen(int fd, int backlog)
{
	return __perform_listen(fd, backlog, SOCKET_SEMANTIC_XNET);
}
#endif

/* ========================================================================= */
/*..close -- replacement close call. */
int close(int fd)
{
	int shadow_fd;
	int ret;

	if (init_status == 0)
		__sdp_init();

	if (NULL == _socket_funcs.close) {
		__sdp_log(9, "Error close: no implementation for close found\n");
		return -1;
	}

	shadow_fd = get_shadow_fd_by_fd(fd);

	__sdp_log(2, "CLOSE: <%s:%d:%d>\n",
			  program_invocation_short_name, fd, shadow_fd);

	if (shadow_fd != -1) {
		__sdp_log(1, "CLOSE: closing shadow fd:<%d>\n", shadow_fd);
		if (cleanup_shadow(fd) < 0)
			__sdp_log(8, "Error close: failed to cleanup shadow for fd:%d\n",
					  fd);
	}

	init_extra_attribute(fd);
	ret = _socket_funcs.close(fd);
	__sdp_log(2, "CLOSE: <%s:%d:%d> result <%d>\n",
			  program_invocation_short_name, fd, shadow_fd, ret);
	return ret;
}								/* close */

/* ========================================================================= */
/*..dup -- replacement dup call.                                             */
/* we duplicate the fd and its shadow if exists - ok if the main worked      */
int dup(int fd)
{
	int newfd, new_shadow_fd = -1;
	int shadow_fd;

	if (init_status == 0)
		__sdp_init();

	if (NULL == _socket_funcs.dup) {
		__sdp_log(9, "Error dup: no implementation for dup found\n");
		return -1;
	}

	shadow_fd = get_shadow_fd_by_fd(fd);

	__sdp_log(2, "DUP: <%s:%d:%d>\n",
			  program_invocation_short_name, fd, shadow_fd);

	__sdp_log(1, "DUP: duplication fd:<%d>\n", fd);
	newfd = _socket_funcs.dup(fd);

	if (newfd == fd)
		return (fd);

	if (!is_valid_fd(newfd)) {
		__sdp_log(8, "Error dup: new fd <%d> out of range.\n", newfd);
	} else {
		/* copy attributes from old fd */
		libsdp_fd_attributes[newfd] = libsdp_fd_attributes[fd];
		libsdp_fd_attributes[newfd].shadow_fd = -1;

		if (shadow_fd != -1) {
			__sdp_log(1, "DUP: duplication shadow fd:<%d>\n", shadow_fd);
			new_shadow_fd = _socket_funcs.dup(shadow_fd);
			if ((new_shadow_fd > max_file_descriptors) || (new_shadow_fd < 0)) {
				__sdp_log(8, "Error dup: new shadow fd <%d> out of range.\n",
						  new_shadow_fd);
			} else {
				libsdp_fd_attributes[new_shadow_fd] =
					libsdp_fd_attributes[shadow_fd];
				libsdp_fd_attributes[newfd].shadow_fd = new_shadow_fd;
			}
		}						/* shadow exists */
	}

	__sdp_log(2, "DUP: <%s:%d:%d> return <%d:%d>\n",
			  program_invocation_short_name, fd, shadow_fd, newfd,
			  new_shadow_fd);

	return newfd;
}								/* dup */

/* ========================================================================= */
/*..dup2 -- replacement dup2 call.                                           */
/* since only the main new fd is given we only move the shadow if exists     */
int dup2(int fd, int newfd)
{
	int shadow_fd;
	int shadow_newfd;
	int new_shadow_fd = -1;
	int ret = 0;

	if (init_status == 0)
		__sdp_init();

	if (NULL == _socket_funcs.dup2) {
		__sdp_log(9, "Error dup2: no implementation for dup2 found\n");
		return -1;
	}

	shadow_fd = get_shadow_fd_by_fd(fd);
	shadow_newfd = get_shadow_fd_by_fd(newfd);

	__sdp_log(2, "DUP2: <%s:%d:%d>\n",
			  program_invocation_short_name, fd, shadow_fd);

	if (newfd == fd) {
		__sdp_log(1, "DUP2: skip duplicating fd:<%d> into:<%d>\n", fd, newfd);
		goto done;
	}

	/* dup2 closes the target file desc if it is a valid fd */
	if (shadow_newfd != -1) {
		__sdp_log(1, "DUP2: closing newfd:<%d> shadow:<%d>\n", newfd,
				  shadow_newfd);
		ret = _socket_funcs.close(shadow_newfd);
		if (ret != 0) {
			__sdp_log(8,
					  "DUP2: fail to close newfd:<%d> shadow:<%d> with: %d %s\n",
					  newfd, shadow_newfd, ret, strerror(errno));
		}
	}

	__sdp_log(1, "DUP2: duplicating fd:<%d> into:<%d>\n", fd, newfd);
	newfd = _socket_funcs.dup2(fd, newfd);
	if ((newfd > max_file_descriptors) || (newfd < 0)) {
		__sdp_log(8, "Error dup2: new fd <%d> out of range.\n", newfd);
	} else {
		/* copy attributes from old fd */
		libsdp_fd_attributes[fd].shadow_fd = -1;
		libsdp_fd_attributes[newfd] = libsdp_fd_attributes[fd];

		/* if it had a shadow create a new shadow */
		if (shadow_fd != -1) {
			__sdp_log(1, "DUP2: duplication shadow fd:<%d>\n", shadow_fd);
			new_shadow_fd = _socket_funcs.dup(shadow_fd);
			if ((new_shadow_fd > max_file_descriptors) || (new_shadow_fd < 0)) {
				__sdp_log(8, "Error dup2: new shadow fd <%d> out of range.\n",
						  new_shadow_fd);
			} else {
				libsdp_fd_attributes[new_shadow_fd] =
					libsdp_fd_attributes[shadow_fd];
				libsdp_fd_attributes[newfd].shadow_fd = new_shadow_fd;
			}
		}						/* newfd is ok */
	}

done:
	__sdp_log(2, "DUP2: <%s:%d:%d> return <%d:%d>\n",
			  program_invocation_short_name, fd, shadow_fd, newfd,
			  new_shadow_fd);

	return newfd;
}								/* dup */

/* ========================================================================= */
/*..getsockname -- replacement getsocknanme call.                            */
int getsockname(int fd, struct sockaddr *name, socklen_t * namelen)
{
	int ret = 0;
	char buf[MAX_ADDR_STR_LEN];

	if (init_status == 0)
		__sdp_init();

	/*
	 * ensure the SDP protocol family is not exposed to the user, since
	 * this is meant to be a transparency layer.
	 */
	if (NULL == _socket_funcs.getsockname) {
		__sdp_log(9,
				  "Error getsockname: no implementation for getsockname found\n");
		return -1;
	}

	/* double check provided pointers */
	if ((name == NULL) || is_invalid_addr(name)) {
		errno = EFAULT;
		__sdp_log(8, "Error getsockname: illegal address provided\n");
		return -1;
	}

	if ((namelen != NULL) && is_invalid_addr(namelen)) {
		errno = EFAULT;
		__sdp_log(8, "Error getsockname: illegal address length pointer provided\n");
		return -1;
	}

	__sdp_log(2, "GETSOCKNAME <%s:%d>\n", program_invocation_short_name, fd);

	ret = _socket_funcs.getsockname(fd, name, namelen);

	if (__sdp_log_get_level() <= 1) {
		if (get_addr_str(name, buf, MAX_ADDR_STR_LEN)) {
			__sdp_log(1, "GETSOCKNAME: " "address is illegal\n");
		} else {
			__sdp_log(1, "GETSOCKNAME: address is:%s port:%d\n", buf,
					  ntohs(((struct sockaddr_in *) name)->sin_port));
		}
	}
	__sdp_log(2, "GETSOCKNAME <%s:%d> result <%d>\n",
			  program_invocation_short_name, fd, ret);

	return ret;
}								/* getsockname */

/* ========================================================================= */
/*..getpeername -- replacement getpeername call. */
int getpeername(int fd, struct sockaddr *name, socklen_t * namelen)
{
	int ret = 0;

	if (init_status == 0)
		__sdp_init();

	if (NULL == _socket_funcs.getpeername) {
		__sdp_log(9, "Error getpeername: "
				  "no implementation for getpeername found\n");
		return -1;
	}

	/* double check provided pointers */
	if ((name == NULL) || is_invalid_addr(name)) {
		errno = EFAULT;
		__sdp_log(8, "Error getsockname: illegal address provided\n");
		return -1;
	}

	if ((namelen != NULL) && is_invalid_addr(namelen)) {
		errno = EFAULT;
		__sdp_log(8,
				  "Error getsockname: illegal address length pointer provided\n");
		return -1;
	}

	__sdp_log(2, "GETPEERNAME <%s:%d>\n", program_invocation_short_name, fd);

	ret = _socket_funcs.getpeername(fd, name, namelen);

	__sdp_log(2, "GETPEERNAME <%s:%d> result <%d:%d> family=%d s_addr=%d\n",
			  program_invocation_short_name, fd, ret,
			  (!(0 > ret) ? 0 : -1), name->sa_family,
			  ((struct sockaddr_in *) name)->sin_addr.s_addr);

	return ret;
}								/* getpeername */



/* ========================================================================= */
/*..accept -- replacement accept call.                                       */
/*
  If we have a shadow we need to decide which socket we want to accept on
  so we select first and then give priority based on previous selection
*/
int accept(int fd, struct sockaddr *addr, socklen_t * addrlen)
{
	int shadow_fd;
	int ret = 0;
	fd_set fds;
	socklen_t saved_addrlen = 0;
	int fopts;
	char buf[MAX_ADDR_STR_LEN];

	if (init_status == 0)
		__sdp_init();

	shadow_fd = get_shadow_fd_by_fd(fd);

	/*
	 * ensure the SDP protocol family is not exposed to the user, since
	 * this is meant to be a transparency layer.
	 */
	if (NULL == _socket_funcs.accept) {
		__sdp_log(9, "Error accept: no implementation for accept found\n");
		return -1;
	}

	/* double check provided pointers */
	if ((addr != NULL) && is_invalid_addr(addr)) {
		errno = EINVAL;
		__sdp_log(8, "Error accept: illegal address provided\n");
		return -1;
	}

	if ((addrlen != NULL) && is_invalid_addr(addrlen)) {
		errno = EINVAL;
		__sdp_log(8, "Error accept: illegal address length pointer provided\n");
		return -1;
	}

	if (addr && addrlen)
		saved_addrlen = *addrlen;

	__sdp_log(2, "ACCEPT: <%s:%d>\n", program_invocation_short_name, fd);

	if (shadow_fd == -1) {
		fopts = _socket_funcs.fcntl(fd, F_GETFL);
		__sdp_log(1, "ACCEPT: fd <%d> opts are <0x%x>\n", fd, fopts);

		__sdp_log(7, "ACCEPT: accepting on single fd:<%d>\n", fd);
		ret = _socket_funcs.accept(fd, addr, addrlen);
		if (ret < 0) {
			if (!(fopts & O_NONBLOCK && errno == EWOULDBLOCK))
				__sdp_log(8, "Error accept: accept returned :<%d> %s\n",
						  ret, strerror(errno));
		} else {
			set_is_sdp_socket(ret, get_is_sdp_socket(fd));
		}
	} else {

		fopts = _socket_funcs.fcntl(shadow_fd, F_GETFL);
		__sdp_log(1, "ACCEPT: shadow_fd <%d> opts are <0x%x>\n",
				  shadow_fd, fopts);

		/* we need different behavior for NONBLOCK or signal IO and BLOCK */
		if ((fopts > 0) && (fopts & (O_NONBLOCK | FASYNC))) {
			__sdp_log(1, "ACCEPT: accepting (nonblock) on SDP fd:<%d>\n", shadow_fd);

			ret = _socket_funcs.accept(shadow_fd, addr, addrlen);
			if (ret >= 0) {
				set_is_sdp_socket(ret, 1);

				__sdp_log(7, "ACCEPT: accepted (nonblock) SDP fd:<%d>\n",
						  shadow_fd);
			} else {
				__sdp_log(1, "ACCEPT: accept on SDP fd:<%d> return:%d errno:%d\n",
						  shadow_fd, ret, errno);

				__sdp_log(1, "ACCEPT: accepting (nonblock) on TCP fd:<%d>\n", fd);
				ret = _socket_funcs.accept(fd, addr, addrlen);
				if (ret >= 0) {
					__sdp_log(7, "ACCEPT: accepted (nonblock) TCP fd:<%d>\n",
							  shadow_fd);
				} else {
					__sdp_log(1, "ACCEPT: accept on TCP fd:<%d> "
							  "return:%d errno:%d\n", fd, ret, errno);
				}
			}
		} else {
			__sdp_log(1, "ACCEPT: selecting both fd:<%d> and shadow:<%d>\n",
					  fd, shadow_fd);
			FD_ZERO(&fds);
			FD_SET(fd, &fds);
			FD_SET(shadow_fd, &fds);
			ret =
				_socket_funcs.select(1 + ((fd > shadow_fd) ? fd : shadow_fd),
									 &fds, NULL, NULL, NULL);
			if (ret >= 0) {
				if (last_accept_was_tcp(fd) == 0) {
					if (FD_ISSET(fd, &fds)) {
						set_last_accept(fd, 1);
						__sdp_log(7, "ACCEPT: accepting on TCP fd:<%d>\n", fd);
						ret = _socket_funcs.accept(fd, addr, addrlen);
					} else {
						__sdp_log(7, "ACCEPT: accepting on SDP fd:<%d>\n",
								  shadow_fd);
						ret = _socket_funcs.accept(shadow_fd, addr, addrlen);
						if (ret >= 0)
							set_is_sdp_socket(ret, 1);
					}
				} else {
					if (FD_ISSET(shadow_fd, &fds)) {
						set_last_accept(fd, 1);
						__sdp_log(7, "ACCEPT: accepting on SDP fd:<%d>\n",
								  shadow_fd);
						ret = _socket_funcs.accept(shadow_fd, addr, addrlen);
						if (ret >= 0)
							set_is_sdp_socket(ret, 1);
					} else {
						__sdp_log(7, "ACCEPT: accepting on TCP fd:<%d>\n", fd);
						ret = _socket_funcs.accept(fd, addr, addrlen);
					}
				}
			} else {
				if (errno != EINTR) {
					__sdp_log(8,
							  "Error accept: select returned :<%d> (%d) %s\n",
							  ret, errno, strerror(errno));
				} else {
					__sdp_log(1, "ACCEPT: select returned :<%d> (%d) %s\n",
							  ret, errno, strerror(errno));
				}
			}
		}						/* blocking mode */
	}							/* shadow fd */

	if ((__sdp_log_get_level() <= 1) && (ret >= 0) && addr && addrlen) {
		get_addr_str(addr, buf, *addrlen);
		__sdp_log(1, "ACCEPT: accepted from:%s port:%d into fd:%d\n",
				  buf, ntohs(((struct sockaddr_in *) addr)->sin_port), ret);
	}
	__sdp_log(2, "ACCEPT: <%s:%d> return <%d>\n",
			  program_invocation_short_name, fd, ret);

	return ret;
}								/* accept */

/* ========================================================================= */
/*..select -- replacement socket call.                                       */
/* 
   if we have shadow we must select on it too - which requires a hack back 
   and forth
*/
int
select(int n,
	   fd_set * readfds,
	   fd_set * writefds, fd_set * exceptfds, struct timeval *timeout)
{
	int shadow_fd;
	int ret;
	int current;
	int maxi = 0;
	fd_set new_fds;

	if (init_status == 0)
		__sdp_init();

	if (NULL == _socket_funcs.select) {
		__sdp_log(9, "Error select: no implementation for select found\n");
		return -1;
	}

	__sdp_log(2, "SELECT: <%s:%d>\n", program_invocation_short_name, n);

	/* if we do not read - nothing to do */
	if (readfds == NULL) {
		ret = _socket_funcs.select(n, readfds, writefds, exceptfds, timeout);
		goto done;
	}

	FD_ZERO(&new_fds);
	if (n > 0) {
		maxi = n - 1;
	}

	/* add shadow bits */
	for (current = 0; current < n; current++) {
		if (FD_ISSET(current, readfds)) {
			FD_SET(current, &new_fds);
			if (current > maxi) {
				maxi = current;
			}
			shadow_fd = get_shadow_fd_by_fd(current);
			if (shadow_fd != -1) {
				__sdp_log(1,
						  "SELECT: adding fd:<%d> shadow_fd:<%d> to readfs\n",
						  current, shadow_fd);
				FD_SET(shadow_fd, &new_fds);
				if (shadow_fd > maxi) {
					maxi = shadow_fd;
				}
			}
		}
	}

	__sdp_log(1, "SELECT: invoking select n=<%d>\n", 1 + maxi);
	ret = _socket_funcs.select(1 + maxi,
							   &new_fds, writefds, exceptfds, timeout);

	/* remove the count and bits of the shadows */
	if (ret >= 0) {
		for (current = 0; current < n; current++) {
			shadow_fd = get_shadow_fd_by_fd(current);
			if (shadow_fd == -1) {
				if (FD_ISSET(current, readfds) &&
					FD_ISSET(current, &new_fds) == 0) {
					FD_CLR(current, readfds);
				}
			} else {
				if (FD_ISSET(current, readfds) && FD_ISSET(current, &new_fds)
					&& FD_ISSET(shadow_fd, &new_fds)) {
					ret -= 1;
				}
				if (FD_ISSET(current, readfds) &&
					FD_ISSET(current, &new_fds) == 0 &&
					FD_ISSET(shadow_fd, &new_fds) == 0) {
					FD_CLR(current, readfds);
				}
			}
		}
	}

done:

	__sdp_log(2, "SELECT: <%s:%d> return <%d>\n",
			  program_invocation_short_name, n, ret);
	return ret;
}								/* select */

/* ========================================================================= */
/*..pselect -- replacement socket call.                                      */
/* 
   if we have shadow we must pselect on it too - which requires a hack back 
   and forth
*/
int
pselect(int n,
		fd_set * readfds,
		fd_set * writefds,
		fd_set * exceptfds,
		const struct timespec *timeout, const sigset_t * sigmask)
{
	int shadow_fd;
	int ret;
	int current;
	int maxi = 0;
	fd_set new_fds;

	if (init_status == 0)
		__sdp_init();

	if (NULL == _socket_funcs.pselect) {
		__sdp_log(9, "Error pselect: no implementation for pselect found\n");
		return -1;
	}

	__sdp_log(2, "PSELECT: <%s:%d>\n", program_invocation_short_name, n);

	/* if we do not read - nothing to do */
	if (readfds == NULL) {
		ret =
			_socket_funcs.pselect(n, readfds, writefds, exceptfds, timeout,
								  sigmask);
		goto done;
	}

	FD_ZERO(&new_fds);
	if (n > 0) {
		maxi = n - 1;
	}

	/* add shadow bits */
	for (current = 0; current < n; current++) {
		if (FD_ISSET(current, readfds)) {
			FD_SET(current, &new_fds);
			if (current > maxi) {
				maxi = current;
			}
			shadow_fd = get_shadow_fd_by_fd(current);
			if (shadow_fd != -1) {
				__sdp_log(1,
						  "PSELECT: adding fd:<%d> shadow_fd:<%d> to readfs\n",
						  current, shadow_fd);
				FD_SET(shadow_fd, &new_fds);
				if (shadow_fd > maxi) {
					maxi = shadow_fd;
				}
			}
		}
	}

	__sdp_log(1, "PSELECT: invoking pselect n=<%d>\n", 1 + maxi);
	ret = _socket_funcs.pselect(1 + maxi,
								&new_fds, writefds, exceptfds,
								timeout, sigmask);

	/* remove the count and bits of the shadows */
	if (ret >= 0) {
		for (current = 0; current < n; current++) {
			shadow_fd = get_shadow_fd_by_fd(current);
			if (shadow_fd == -1) {
				if (FD_ISSET(current, readfds) &&
					FD_ISSET(current, &new_fds) == 0) {
					FD_CLR(current, readfds);
				}
			} else {
				if (FD_ISSET(current, readfds) && FD_ISSET(current, &new_fds)
					&& FD_ISSET(shadow_fd, &new_fds)) {
					ret -= 1;
				}
				if (FD_ISSET(current, readfds) &&
					FD_ISSET(current, &new_fds) == 0 &&
					FD_ISSET(shadow_fd, &new_fds) == 0) {
					FD_CLR(current, readfds);
				}
			}
		}
	}

done:

	__sdp_log(2, "PSELECT: <%s:%d> return <%d>\n",
			  program_invocation_short_name, n, ret);
	return ret;
}								/* pselect */

/* ========================================================================= */
/*..poll -- replacement socket call.                                      */
/* 
   if we have shadow we must poll on it too - which requires a hack back 
   and forth
*/
int poll(struct pollfd *ufds, nfds_t nfds, int timeout)
{
	int ret;
	int shadow_fd;
	int current;
	int extra = 0;
	struct pollfd *poll_fds = NULL;
	struct pollfd *poll_fd_ptr = NULL;

	if (init_status == 0)
		__sdp_init();

	if (NULL == _socket_funcs.poll) {
		__sdp_log(9, "Error poll: no implementation for poll found\n");
		return -1;
	}

	__sdp_log(2, "POLL: <%s:%d>\n", program_invocation_short_name, nfds);

	/* if we do not have any file desc - nothing to do */
	if (ufds == NULL) {
		ret = _socket_funcs.poll(ufds, nfds, timeout);
		goto done;
	}

	/* scan for how many extra fds are required */
	for (current = 0; current < nfds; current++) {
		shadow_fd = get_shadow_fd_by_fd(ufds[current].fd);
		if (shadow_fd != -1)
			extra++;
	}

	if (!extra) {
		poll_fds = ufds;
	} else {
		poll_fds =
			(struct pollfd *) malloc((nfds + extra) * sizeof(struct pollfd));
		if (!poll_fds) {
			__sdp_log(9,
					  "Error poll: malloc of extended pollfd array failed\n");
			ret = -1;
			errno = ENOMEM;
			goto done;
		}
		poll_fd_ptr = poll_fds;
		for (current = 0; current < nfds; current++) {
			*poll_fd_ptr = ufds[current];
			poll_fd_ptr++;
			shadow_fd = get_shadow_fd_by_fd(ufds[current].fd);
			if (shadow_fd != -1) {
				__sdp_log(1, "POLL: adding fd:<%d> shadow_fd:<%d> to readfs\n",
						  current, shadow_fd);
				*poll_fd_ptr = ufds[current];
				poll_fd_ptr->fd = shadow_fd;
				poll_fd_ptr++;
			}
		}
	}

	__sdp_log(1, "POLL: invoking poll nfds=<%d>\n", nfds + extra);
	ret = _socket_funcs.poll(poll_fds, nfds + extra, timeout);

	/* refactor into original list if any events */
	if ((ret > 0) && extra) {
		poll_fd_ptr = poll_fds;
		for (current = 0; current < nfds; current++) {
			shadow_fd = get_shadow_fd_by_fd(ufds[current].fd);
			if (shadow_fd == -1) {
				ufds[current] = *poll_fd_ptr;
			} else {
				ufds[current] = *poll_fd_ptr;
				poll_fd_ptr++;
				if (poll_fd_ptr->revents) {
					if (ufds[current].revents)
						ret--;
					ufds[current].revents |= poll_fd_ptr->revents;
				}
			}
			poll_fd_ptr++;
		}
	}

	if (extra)
		free(poll_fds);
done:

	__sdp_log(2, "POLL: <%s:%d> return <%d>\n",
			  program_invocation_short_name, nfds, ret);
	return ret;
}								/* poll */

#ifdef __linux__
/* ========================================================================= */
/*..epoll_create -- replacement socket call.                                 */
/*
   Need to make the size twice as large for shadow fds
*/
int epoll_create(int size)
{
	int epfd;

	if (init_status == 0)
		__sdp_init();

	if (NULL == _socket_funcs.epoll_create) {
		__sdp_log(9,
				  "Error epoll_create: no implementation for epoll_create found\n");
		return -1;
	}

	__sdp_log(2, "EPOLL_CREATE: <%s:%d>\n", program_invocation_short_name,
			  size);

	epfd = _socket_funcs.epoll_create(size * 2);

	__sdp_log(2, "EPOLL_CREATE: <%s:%d> return %d\n",
			  program_invocation_short_name, size, epfd);
	return epfd;
}								/* epoll_create */

/* ========================================================================= */
/*..epoll_ctl -- replacement socket call.                                   */
/*
   Need to add/delete/modify shadow fds as well
*/
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	int ret, shadow_fd, ret2;

	if (init_status == 0)
		__sdp_init();

	if (NULL == _socket_funcs.epoll_ctl) {
		__sdp_log(9,
				  "Error epoll_ctl: no implementation for epoll_ctl found\n");
		return -1;
	}

	__sdp_log(2, "EPOLL_CTL: <%s:%d> op <%d:%d>\n",
			  program_invocation_short_name, epfd, op, fd);

	ret = _socket_funcs.epoll_ctl(epfd, op, fd, event);

	shadow_fd = get_shadow_fd_by_fd(fd);
	if (shadow_fd != -1) {
		ret2 = _socket_funcs.epoll_ctl(epfd, op, shadow_fd, event);
		if (ret2 < 0) {
			__sdp_log(8, "Error epoll_ctl <%s:%d:%d>",
					  program_invocation_short_name, fd, shadow_fd);
			return ret2;
		}
	}

	__sdp_log(2, "EPOLL_CTL: <%s:%d> return <%d>\n",
			  program_invocation_short_name, epfd, ret);
	return ret;
}								/* epoll_ctl */

/* ========================================================================= */
/*..epoll_wait -- replacement socket call.                                   */
/*
   We don't care who generated the event because all we get is user-context
   values.
*/
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	int ret;

	if (init_status == 0)
		__sdp_init();

	if (NULL == _socket_funcs.epoll_wait) {
		__sdp_log(9,
				  "Error epoll_wait: no implementation for epoll_wait found\n");
		return -1;
	}

	__sdp_log(2, "EPOLL_WAIT: <%s:%d>\n", program_invocation_short_name, epfd);

	ret = _socket_funcs.epoll_wait(epfd, events, maxevents, timeout);

	__sdp_log(2, "EPOLL_WAIT: <%s:%d> return <%d>\n",
			  program_invocation_short_name, epfd, ret);
	return ret;
}								/* epoll_wait */

/* ========================================================================= */
/*..epoll_pwait -- replacement socket call.                                  */
/*
   We don't care who generated the event because all we get is user-context
   values.
*/
int
epoll_pwait(int epfd,
			struct epoll_event *events,
			int maxevents, int timeout, const sigset_t * sigmask)
{
	int ret;

	if (init_status == 0)
		__sdp_init();

	if (NULL == _socket_funcs.epoll_pwait) {
		__sdp_log(9,
				  "Error epoll_pwait: no implementation for epoll_pwait found\n");
		return -1;
	}

	__sdp_log(2, "EPOLL_PWAIT: <%s:%d>\n", program_invocation_short_name, epfd);

	ret = _socket_funcs.epoll_pwait(epfd, events, maxevents, timeout, sigmask);

	__sdp_log(2, "EPOLL_PWAIT: <%s:%d> return <%d>\n",
			  program_invocation_short_name, epfd, ret);
	return ret;
}								/* epoll_pwait */
#endif

/* ========================================================================= */

/* --------------------------------------------------------------------- */
/*                                                                       */
/* Library load/unload initialization/cleanup                            */
/*                                                                       */
/* --------------------------------------------------------------------- */
/* ========================================================================= */
/*..__sdp_init -- intialize the library */
void __sdp_init(void)
{
	char *config_file, *error_str;
	int fd;
	struct rlimit nofiles_limit;

	/* HACK: races might apply here: can we assume init is happening
	   only within one thread ? */
	if (init_status != 0)
		return;
	init_status = 1;

	dev_null_fd = open("/dev/null", O_WRONLY);

	/* figure out the max number of file descriptors */
	if (getrlimit(RLIMIT_NOFILE, &nofiles_limit))
		max_file_descriptors = 1024;
	else
		max_file_descriptors = nofiles_limit.rlim_cur;

	/* allocate and initialize the shadow sdp sockets array */
	libsdp_fd_attributes =
		(struct sdp_extra_fd_attributes *) calloc(max_file_descriptors,
												  sizeof(struct
														 sdp_extra_fd_attributes));
	for (fd = 0; fd < max_file_descriptors; fd++)
		init_extra_attribute(fd);

#ifndef RTLD_NEXT
	/*
	 * open libc for original socket call.
	 * Solaris relies on RTLD next - since the socket calls are
	 * actually in libsocket rather than libc.
	 */
	__libc_dl_handle = dlopen("/lib64/libc.so.6", RTLD_LAZY);
	if (NULL == __libc_dl_handle) {
		__libc_dl_handle = dlopen("/lib/libc.so.6", RTLD_LAZY);
		if (NULL == __libc_dl_handle) {
			fprintf(stderr, "%s\n", dlerror());
			return;
		}
	}
#endif

	/*
	 * Get the original functions
	 */
	_socket_funcs.ioctl = dlsym(__libc_dl_handle, "ioctl");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_funcs.fcntl = dlsym(__libc_dl_handle, "fcntl");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_funcs.socket = dlsym(__libc_dl_handle, "socket");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_funcs.setsockopt = dlsym(__libc_dl_handle, "setsockopt");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_funcs.connect = dlsym(__libc_dl_handle, "connect");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_funcs.listen = dlsym(__libc_dl_handle, "listen");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_funcs.bind = dlsym(__libc_dl_handle, "bind");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_funcs.close = dlsym(__libc_dl_handle, "close");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_funcs.dup = dlsym(__libc_dl_handle, "dup");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_funcs.dup2 = dlsym(__libc_dl_handle, "dup2");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_funcs.getpeername = dlsym(__libc_dl_handle, "getpeername");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_funcs.getsockname = dlsym(__libc_dl_handle, "getsockname");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_funcs.accept = dlsym(__libc_dl_handle, "accept");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_funcs.select = dlsym(__libc_dl_handle, "select");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_funcs.pselect = dlsym(__libc_dl_handle, "pselect");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_funcs.poll = dlsym(__libc_dl_handle, "poll");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

#ifdef __linux__
	_socket_funcs.epoll_create = dlsym(__libc_dl_handle, "epoll_create");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_funcs.epoll_ctl = dlsym(__libc_dl_handle, "epoll_ctl");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_funcs.epoll_wait = dlsym(__libc_dl_handle, "epoll_wait");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_funcs.epoll_pwait = dlsym(__libc_dl_handle, "epoll_pwait");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}
#endif
#ifdef SOLARIS_BUILD
	_socket_xnet_funcs.socket = dlsym(__libc_dl_handle, "__xnet_socket");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_xnet_funcs.connect = dlsym(__libc_dl_handle, "__xnet_connect");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_xnet_funcs.listen = dlsym(__libc_dl_handle, "__xnet_listen");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	_socket_xnet_funcs.bind = dlsym(__libc_dl_handle, "__xnet_bind");
	if (NULL != (error_str = dlerror())) {
		fprintf(stderr, "%s\n", error_str);
	}

	/* Determine program name by asking libdl */
	Dl_argsinfo args_info;
	if (NULL != dlinfo(RTLD_SELF, RTLD_DI_ARGSINFO, &args_info)) {
		fprintf(stderr, "args_info: %s\n", dlerror());
	} else {
		program_invocation_name = args_info.dla_argv[0];
		program_invocation_short_name = basename(args_info.dla_argv[0]);
	}
#endif
#ifdef __FreeBSD__
	program_invocation_short_name = (char *)getprogname();
	program_invocation_name = program_invocation_short_name;
#endif

	if (getenv("SIMPLE_LIBSDP") != NULL) {
		simple_sdp_library = 1;
	}

	if (getenv("ALWAYS_USE_SDP") != NULL) {
		simple_sdp_library = 1;
	}
#define LIBSDP_DEFAULT_CONFIG_FILE  SYSCONFDIR "/libsdp.conf"
	if (!simple_sdp_library) {
		config_file = getenv("LIBSDP_CONFIG_FILE");
		if (!config_file)
			config_file = LIBSDP_DEFAULT_CONFIG_FILE;

		if (__sdp_parse_config(config_file)) {
			fprintf(stderr,
					"libsdp Error: failed to parse config file:%s. Using defaults.\n",
					config_file);
		}
	}

	__sdp_log(1, "Max file descriptors:%d\n", max_file_descriptors);
	init_status = 2;

}								/* __sdp_init */

/* ========================================================================= */
/*..__sdp_fini -- when the library is unloaded this is called */
void __sdp_fini(void)
{
	struct use_family_rule *rule;
	for (rule = __sdp_clients_family_rules_head; rule != NULL;
		 rule = rule->next)
		free(rule->prog_name_expr);
	for (rule = __sdp_servers_family_rules_head; rule != NULL;
		 rule = rule->next)
		free(rule->prog_name_expr);

	free(libsdp_fd_attributes);

#ifndef RTLD_NEXT
	dlclose(__libc_dl_handle);
#endif
}								/* _fini */
