/*
 * ++Copyright++ 1980, 1983, 1988, 1993
 * -
 * Copyright (c) 1980, 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * Portions Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by WIDE Project and
 *    its contributors.
 * 4. Neither the name of the project nor the names of its contributors
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
 * -
 * --Copyright--
 */

/*
 *      @(#)netdb.h	8.1 (Berkeley) 6/2/93
 *	$Id: netdb.h,v 1.15.18.7 2008-02-28 05:49:37 marka Exp $
 */

#ifndef _NETDB_H_
#define _NETDB_H_

#include <sys/param.h>
#include <sys/types.h>
#if (!defined(BSD)) || (BSD < 199306)
# include <sys/bitypes.h>
#endif
#include <sys/cdefs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

#ifndef _PATH_HEQUIV
#define _PATH_HEQUIV	"/etc/hosts.equiv"
#endif
#ifndef _PATH_HOSTS
#define	_PATH_HOSTS	"/etc/hosts"
#endif
#ifndef _PATH_NETWORKS
#define	_PATH_NETWORKS	"/etc/networks"
#endif
#ifndef _PATH_PROTOCOLS
#define	_PATH_PROTOCOLS	"/etc/protocols"
#endif
#ifndef _PATH_SERVICES
#define	_PATH_SERVICES	"/etc/services"
#endif

#if (__GLIBC__ > 2 || __GLIBC__ == 2 &&  __GLIBC_MINOR__ >= 3)
#define __h_errno __h_errno_location
#endif
__BEGIN_DECLS
extern int * __h_errno __P((void));
__END_DECLS
#if defined(_REENTRANT) || \
    (__GLIBC__ > 2 || __GLIBC__ == 2 &&  __GLIBC_MINOR__ >= 3)
#define	h_errno (*__h_errno())
#else
extern int h_errno;
#endif

/*%
 * Structures returned by network data base library.  All addresses are
 * supplied in host order, and returned in network order (suitable for
 * use in system calls).
 */
struct	hostent {
	char	*h_name;	/*%< official name of host */
	char	**h_aliases;	/*%< alias list */
	int	h_addrtype;	/*%< host address type */
	int	h_length;	/*%< length of address */
	char	**h_addr_list;	/*%< list of addresses from name server */
#define	h_addr	h_addr_list[0]	/*%< address, for backward compatiblity */
};

/*%
 * Assumption here is that a network number
 * fits in an unsigned long -- probably a poor one.
 */
struct	netent {
	char		*n_name;	/*%< official name of net */
	char		**n_aliases;	/*%< alias list */
	int		n_addrtype;	/*%< net address type */
	unsigned long	n_net;		/*%< network # */
};

struct	servent {
	char	*s_name;	/*%< official service name */
	char	**s_aliases;	/*%< alias list */
	int	s_port;		/*%< port # */
	char	*s_proto;	/*%< protocol to use */
};

struct	protoent {
	char	*p_name;	/*%< official protocol name */
	char	**p_aliases;	/*%< alias list */
	int	p_proto;	/*%< protocol # */
};

struct	addrinfo {
	int		ai_flags;	/*%< AI_PASSIVE, AI_CANONNAME */
	int		ai_family;	/*%< PF_xxx */
	int		ai_socktype;	/*%< SOCK_xxx */
	int		ai_protocol;	/*%< 0 or IPPROTO_xxx for IPv4 and IPv6 */
#if defined(sun) && defined(_SOCKLEN_T)
#ifdef __sparcv9
	int		_ai_pad;
#endif
	socklen_t	ai_addrlen;
#else
	size_t		ai_addrlen;	/*%< length of ai_addr */
#endif
#ifdef __linux
	struct sockaddr	*ai_addr; 	/*%< binary address */
	char		*ai_canonname;	/*%< canonical name for hostname */
#else
	char		*ai_canonname;	/*%< canonical name for hostname */
	struct sockaddr	*ai_addr; 	/*%< binary address */
#endif
	struct addrinfo	*ai_next; 	/*%< next structure in linked list */
};

/*%
 * Error return codes from gethostbyname() and gethostbyaddr()
 * (left in extern int h_errno).
 */

#define	NETDB_INTERNAL	-1	/*%< see errno */
#define	NETDB_SUCCESS	0	/*%< no problem */
#define	HOST_NOT_FOUND	1 /*%< Authoritative Answer Host not found */
#define	TRY_AGAIN	2 /*%< Non-Authoritive Host not found, or SERVERFAIL */
#define	NO_RECOVERY	3 /*%< Non recoverable errors, FORMERR, REFUSED, NOTIMP */
#define	NO_DATA		4 /*%< Valid name, no data record of requested type */
#define	NO_ADDRESS	NO_DATA		/*%< no address, look for MX record */
/*
 * Error return codes from getaddrinfo()
 */
#define	EAI_ADDRFAMILY	 1	/*%< address family for hostname not supported */
#define	EAI_AGAIN	 2	/*%< temporary failure in name resolution */
#define	EAI_BADFLAGS	 3	/*%< invalid value for ai_flags */
#define	EAI_FAIL	 4	/*%< non-recoverable failure in name resolution */
#define	EAI_FAMILY	 5	/*%< ai_family not supported */
#define	EAI_MEMORY	 6	/*%< memory allocation failure */
#define	EAI_NODATA	 7	/*%< no address associated with hostname */
#define	EAI_NONAME	 8	/*%< hostname nor servname provided, or not known */
#define	EAI_SERVICE	 9	/*%< servname not supported for ai_socktype */
#define	EAI_SOCKTYPE	10	/*%< ai_socktype not supported */
#define	EAI_SYSTEM	11	/*%< system error returned in errno */
#define EAI_BADHINTS	12
#define EAI_PROTOCOL	13
#define EAI_MAX		14

/*%
 * Flag values for getaddrinfo()
 */
#define	AI_PASSIVE	0x00000001
#define	AI_CANONNAME	0x00000002
#define AI_NUMERICHOST	0x00000004
#define	AI_MASK		0x00000007

/*%
 * Flag values for getipnodebyname()
 */
#define AI_V4MAPPED	0x00000008
#define AI_ALL		0x00000010
#define AI_ADDRCONFIG	0x00000020
#define AI_DEFAULT	(AI_V4MAPPED|AI_ADDRCONFIG)

/*%
 * Constants for getnameinfo()
 */
#define	NI_MAXHOST	1025
#define	NI_MAXSERV	32

/*%
 * Flag values for getnameinfo()
 */
#define	NI_NOFQDN	0x00000001
#define	NI_NUMERICHOST	0x00000002
#define	NI_NAMEREQD	0x00000004
#define	NI_NUMERICSERV	0x00000008
#define	NI_DGRAM	0x00000010
#define	NI_WITHSCOPEID	0x00000020
#define NI_NUMERICSCOPE	0x00000040

/*%
 * Scope delimit character
 */
#define SCOPE_DELIMITER	'%'


#ifdef _REENTRANT
#if defined (__hpux) || defined(__osf__) || defined(_AIX)
#define	_MAXALIASES	35
#define	_MAXLINELEN	1024
#define	_MAXADDRS	35
#define	_HOSTBUFSIZE	(BUFSIZ + 1)

struct hostent_data {
	struct in_addr	host_addr;
	char		*h_addr_ptrs[_MAXADDRS + 1];
	char		hostaddr[_MAXADDRS];
	char		hostbuf[_HOSTBUFSIZE];
	char		*host_aliases[_MAXALIASES];
	char		*host_addrs[2];
	FILE		*hostf;
#ifdef __osf__
	int		svc_gethostflag;
	int		svc_gethostbind;
#endif
#ifdef __hpux
	short		_nsw_src;
	short		_flags;
	char		*current;
	int		currentlen;
#endif
};

struct  netent_data {
	FILE	*net_fp;
#if defined(__osf__) || defined(_AIX)
	char	line[_MAXLINELEN];
#endif
#ifdef __hpux
	char	line[_MAXLINELEN+1];
#endif
	char	*net_aliases[_MAXALIASES];
#ifdef __osf__
	int	_net_stayopen;
	int	svc_getnetflag;
#endif
#ifdef __hpux
	short	_nsw_src;
	short	_flags;
	char	*current;
	int	currentlen;
#endif
#ifdef _AIX
        int     _net_stayopen;
        char    *current;
        int     currentlen;
        void    *_net_reserv1;          /* reserved for future use */
        void    *_net_reserv2;          /* reserved for future use */
#endif
};

struct	protoent_data {
	FILE	*proto_fp;
#ifdef _AIX
	int     _proto_stayopen;
	char	line[_MAXLINELEN];
#endif
#ifdef __osf__
	char	line[1024];
#endif
#ifdef __hpux
	char	line[_MAXLINELEN+1];
#endif
	char	*proto_aliases[_MAXALIASES];
#ifdef __osf__
	int	_proto_stayopen;
	int	svc_getprotoflag;
#endif
#ifdef __hpux
	short	_nsw_src;
	short	_flags;
	char	*current;
	int	currentlen;
#endif
#ifdef _AIX
        int     currentlen;
        char    *current;
        void    *_proto_reserv1;        /* reserved for future use */
        void    *_proto_reserv2;        /* reserved for future use */
#endif
};

struct	servent_data {
	FILE	*serv_fp;
#if defined(__osf__) || defined(_AIX)
	char	line[_MAXLINELEN];
#endif
#ifdef __hpux
	char	line[_MAXLINELEN+1];
#endif
	char	*serv_aliases[_MAXALIASES];
#ifdef __osf__
	int	_serv_stayopen;
	int	svc_getservflag;
#endif
#ifdef __hpux
	short	_nsw_src;
	short	_flags;
	char	*current;
	int	currentlen;
#endif
#ifdef _AIX
	int     _serv_stayopen;
	char     *current;
	int     currentlen;
	void    *_serv_reserv1;         /* reserved for future use */
	void    *_serv_reserv2;         /* reserved for future use */
#endif
};
#endif
#endif
__BEGIN_DECLS
void		endhostent __P((void));
void		endnetent __P((void));
void		endprotoent __P((void));
void		endservent __P((void));
void		freehostent __P((struct hostent *));
struct hostent	*gethostbyaddr __P((const char *, int, int));
struct hostent	*gethostbyname __P((const char *));
struct hostent	*gethostbyname2 __P((const char *, int));
struct hostent	*gethostent __P((void));
struct hostent	*getipnodebyaddr __P((const void *, size_t, int, int *));
struct hostent	*getipnodebyname __P((const char *, int, int, int *));
struct netent	*getnetbyaddr __P((unsigned long, int));
struct netent	*getnetbyname __P((const char *));
struct netent	*getnetent __P((void));
struct protoent	*getprotobyname __P((const char *));
struct protoent	*getprotobynumber __P((int));
struct protoent	*getprotoent __P((void));
struct servent	*getservbyname __P((const char *, const char *));
struct servent	*getservbyport __P((int, const char *));
struct servent	*getservent __P((void));
void		herror __P((const char *));
const char	*hstrerror __P((int));
void		sethostent __P((int));
/* void		sethostfile __P((const char *)); */
void		setnetent __P((int));
void		setprotoent __P((int));
void		setservent __P((int));
int		getaddrinfo __P((const char *, const char *,
				 const struct addrinfo *, struct addrinfo **));
int		getnameinfo __P((const struct sockaddr *, size_t, char *,
				 size_t, char *, size_t, int));
void		freeaddrinfo __P((struct addrinfo *));
const char	*gai_strerror __P((int));
struct hostent  *getipnodebyname __P((const char *, int, int, int *));
struct hostent	*getipnodebyaddr __P((const void *, size_t, int, int *));
void		freehostent __P((struct hostent *));
#ifdef __GLIBC__
int		getnetgrent __P((/* const */ char **, /* const */ char **,
				 /* const */ char **));
void		setnetgrent __P((const char *));
void		endnetgrent __P((void));
int		innetgr __P((const char *, const char *, const char *,
			     const char *));
#endif

#ifdef _REENTRANT
#if defined(__hpux) || defined(__osf__) || defined(_AIX)
int		gethostbyaddr_r __P((const char *, int, int, struct hostent *,
					struct hostent_data *));
int		gethostbyname_r __P((const char *, struct hostent *, 
					struct hostent_data *));
int		gethostent_r __P((struct hostent *, struct hostent_data *));
#if defined(_AIX)
void		sethostent_r __P((int, struct hostent_data *));
#else
int		sethostent_r __P((int, struct hostent_data *));
#endif 
#if defined(__hpux)
int		endhostent_r __P((struct hostent_data *));
#else
void		endhostent_r __P((struct hostent_data *));
#endif

#if defined(__hpux) || defined(__osf__)
int		getnetbyaddr_r __P((int, int,
				struct netent *, struct netent_data *));
#else
int		getnetbyaddr_r __P((long, int,
				struct netent *, struct netent_data *));
#endif
int		getnetbyname_r __P((const char *,
				struct netent *, struct netent_data *));
int		getnetent_r __P((struct netent *, struct netent_data *));
int		setnetent_r __P((int, struct netent_data *));
#ifdef __hpux
int		endnetent_r __P((struct netent_data *buffer));
#else
void		endnetent_r __P((struct netent_data *buffer));
#endif

int		getprotobyname_r __P((const char *,
				struct protoent *, struct protoent_data *));
int		getprotobynumber_r __P((int,
				struct protoent *, struct protoent_data *));
int		getprotoent_r __P((struct protoent *, struct protoent_data *));
int		setprotoent_r __P((int, struct protoent_data *));
#ifdef __hpux
int		endprotoent_r __P((struct protoent_data *));
#else
void		endprotoent_r __P((struct protoent_data *));
#endif

int		getservbyname_r __P((const char *, const char *,
				struct servent *, struct servent_data *));
int		getservbyport_r __P((int, const char *,
				struct servent *, struct servent_data *));
int		getservent_r __P((struct servent *, struct servent_data *));
int		setservent_r __P((int, struct servent_data *));
#ifdef __hpux
int		endservent_r __P((struct servent_data *));
#else
void		endservent_r __P((struct servent_data *));
#endif
#ifdef _AIX
int		setnetgrent_r __P((char *, void **));
void		endnetgrent_r __P((void **));
/*
 * Note: AIX's netdb.h declares innetgr_r() as: 
 *	int innetgr_r(char *, char *, char *, char *, struct innetgr_data *);
 */
int		innetgr_r __P((const char *, const char *, const char *,
			       const char *));
#endif
#else
 /* defined(sun) || defined(bsdi) */
#if defined(__GLIBC__) || defined(__FreeBSD__) && (__FreeBSD_version + 0 >= 601103)
int gethostbyaddr_r __P((const char *, int, int, struct hostent *,
		         char *, size_t, struct hostent **, int *));
int gethostbyname_r __P((const char *, struct hostent *,
		        char *, size_t, struct hostent **, int *));
int gethostent_r __P((struct hostent *, char *, size_t,
			 struct hostent **, int *));
#else
struct hostent	*gethostbyaddr_r __P((const char *, int, int, struct hostent *,
					char *, int, int *));
struct hostent	*gethostbyname_r __P((const char *, struct hostent *,
					char *, int, int *));
struct hostent	*gethostent_r __P((struct hostent *, char *, int, int *));
#endif
void		sethostent_r __P((int));
void		endhostent_r __P((void));

#if defined(__GLIBC__) || defined(__FreeBSD__) && (__FreeBSD_version + 0 >= 601103)
int getnetbyname_r __P((const char *, struct netent *,
			char *, size_t, struct netent **, int*));
int getnetbyaddr_r __P((unsigned long int, int, struct netent *,
			char *, size_t, struct netent **, int*));
int getnetent_r __P((struct netent *, char *, size_t, struct netent **, int*));
#else
struct netent	*getnetbyname_r __P((const char *, struct netent *,
					char *, int));
struct netent	*getnetbyaddr_r __P((long, int, struct netent *,
					char *, int));
struct netent	*getnetent_r __P((struct netent *, char *, int));
#endif
void		setnetent_r __P((int));
void		endnetent_r __P((void));

#if defined(__GLIBC__) || defined(__FreeBSD__) && (__FreeBSD_version + 0 >= 601103)
int getprotobyname_r __P((const char *, struct protoent *, char *,
			  size_t, struct protoent **));
int getprotobynumber_r __P((int, struct protoent *, char *, size_t,
			    struct protoent **));
int getprotoent_r __P((struct protoent *, char *, size_t, struct protoent **));
#else
struct protoent	*getprotobyname_r __P((const char *,
				struct protoent *, char *, int));
struct protoent	*getprotobynumber_r __P((int,
				struct protoent *, char *, int));
struct protoent	*getprotoent_r __P((struct protoent *, char *, int));
#endif
void		setprotoent_r __P((int));
void		endprotoent_r __P((void));

#if defined(__GLIBC__) || defined(__FreeBSD__) && (__FreeBSD_version + 0 >= 601103)
int getservbyname_r __P((const char *name, const char *,
			 struct servent *, char *, size_t, struct servent **));
int getservbyport_r __P((int port, const char *,
			 struct servent *, char *, size_t, struct servent **));
int getservent_r __P((struct servent *, char *, size_t, struct servent **));
#else
struct servent	*getservbyname_r __P((const char *name, const char *,
					struct servent *, char *, int));
struct servent	*getservbyport_r __P((int port, const char *,
					struct servent *, char *, int));
struct servent	*getservent_r __P((struct servent *, char *, int));
#endif
void		setservent_r __P((int));
void		endservent_r __P((void));

#ifdef __GLIBC__
int		getnetgrent_r __P((char **, char **, char **, char *, size_t));
#endif

#endif
#endif
__END_DECLS

/* This is nec'y to make this include file properly replace the sun version. */
#ifdef sun
#ifdef __GNU_LIBRARY__
#include <rpc/netdb.h>
#else
struct rpcent {
	char	*r_name;	/*%< name of server for this rpc program */
	char	**r_aliases;	/*%< alias list */
	int	r_number;	/*%< rpc program number */
};
struct rpcent	*getrpcbyname(), *getrpcbynumber(), *getrpcent();
#endif /* __GNU_LIBRARY__ */
#endif /* sun */
#endif /* !_NETDB_H_ */
/*! \file */
