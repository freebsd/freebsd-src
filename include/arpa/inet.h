/*
 * Copyright (c) 1983, 1993
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
 */

/*
 *	@(#)inet.h	8.1 (Berkeley) 6/2/93
 *	From: Id: inet.h,v 8.5 1997/01/29 08:48:09 vixie Exp $
 * $FreeBSD$
 */

#ifndef _ARPA_INET_H_
#define	_ARPA_INET_H_

/* External definitions for functions in inet(3), addr2ascii(3) */

#include <sys/cdefs.h>
#include <machine/ansi.h>

#ifndef	_IN_ADDR_T_DECLARED_
typedef	__uint32_t	in_addr_t;
#define	_IN_ADDR_T_DECLARED_
#endif

#ifndef	_IN_PORT_T_DECLARED_
typedef	_uint16_t	in_port_t;
#define	_IN_PORT_T_DECLARED_
#endif

#ifndef _POSIX_SOURCE
#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_	size_t;
#undef	_BSD_SIZE_T_
#endif
#endif /* !_POSIX_SOURCE */

/*
 * XXX socklen_t is used by a POSIX.1-200x interface, but not required by
 * POSIX.1-200x.
 */
#ifdef	_BSD_SOCKLEN_T_
typedef	_BSD_SOCKLEN_T_	socklen_t;
#undef	_BSD_SOCKLEN_T_
#endif

#ifndef _STRUCT_IN_ADDR_DECLARED
struct in_addr {
	in_addr_t s_addr;
};
#define	_STRUCT_IN_ADDR_DECLARED
#endif

/* XXX all new diversions!! argh!! */
#ifndef _POSIX_SOURCE
#define	inet_addr	__inet_addr
#define	inet_aton	__inet_aton
#define	inet_lnaof	__inet_lnaof
#define	inet_makeaddr	__inet_makeaddr
#define	inet_neta	__inet_neta
#define	inet_netof	__inet_netof
#define	inet_network	__inet_network
#define	inet_net_ntop	__inet_net_ntop
#define	inet_net_pton	__inet_net_pton
#define	inet_ntoa	__inet_ntoa
#define	inet_pton	__inet_pton
#define	inet_ntop	__inet_ntop
#define	inet_nsap_addr	__inet_nsap_addr
#define	inet_nsap_ntoa	__inet_nsap_ntoa
#endif /* !_POSIX_SOURCE */

__BEGIN_DECLS
in_addr_t	 inet_addr __P((const char *));
char		*inet_ntoa __P((struct in_addr));
const char	*inet_ntop __P((int, const void *, char *, socklen_t));
int		 inet_pton __P((int, const char *, void *));
/*
 * XXX missing: ntohl() family.
 */

/* Nonstandard functions. */
#ifndef _POSIX_SOURCE
int		 ascii2addr __P((int, const char *, void *));
char		*addr2ascii __P((int, const void *, int, char *));
int		 inet_aton __P((const char *, struct in_addr *));
in_addr_t	 inet_lnaof __P((struct in_addr));
struct in_addr	 inet_makeaddr __P((in_addr_t, in_addr_t));
char *		 inet_neta __P((in_addr_t, char *, size_t));
in_addr_t	 inet_netof __P((struct in_addr));
in_addr_t	 inet_network __P((const char *));
char		*inet_net_ntop __P((int, const void *, int, char *, size_t));
int		 inet_net_pton __P((int, const char *, void *, size_t));
unsigned	 inet_nsap_addr __P((const char *, unsigned char *, int));
char		*inet_nsap_ntoa __P((int, const unsigned char *, char *));
#endif /* !_POSIX_SOURCE */
__END_DECLS

#endif /* !_ARPA_INET_H_ */
