/*-
 * Copyright (c) 1983, 1987, 1989, 1993
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
 *
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
 * --Copyright--
 *
 *	@(#)resolv.h	8.1 (Berkeley) 6/2/93
 *	From Id: resolv.h,v 4.9.1.2 1993/05/17 09:59:01 vixie Exp
 *	$Id: resolv.h,v 1.5 1996/01/07 05:01:50 peter Exp $
 */

#ifndef _RESOLV_H_
#define	_RESOLV_H_

#include <sys/param.h>
#include <sys/types.h>
#include <sys/cdefs.h>
#include <stdio.h>

/*
 * revision information.  this is the release date in YYYYMMDD format.
 * it can change every day so the right thing to do with it is use it
 * in preprocessor commands such as "#if (__RES > 19931104)".  do not
 * compare for equality; rather, use it to determine whether your resolver
 * is new enough to contain a certain feature.
 */

#define	__RES	19951031

/*
 * Resolver configuration file.
 * Normally not present, but may contain the address of the
 * inital name server(s) to query and the domain search list.
 */

#ifndef _PATH_RESCONF
#define	_PATH_RESCONF        "/etc/resolv.conf"
#endif

/*
 * Global defines and variables for resolver stub.
 */
#define	MAXNS			3	/* max # name servers we'll track */
#define	MAXDFLSRCH		3	/* # default domain levels to try */
#define	MAXDNSRCH		6	/* max # domains in search path */
#define	LOCALDOMAINPARTS	2	/* min levels in name that is "local" */

#define	RES_TIMEOUT		5	/* min. seconds between retries */
#define	MAXRESOLVSORT		10	/* number of net to sort on */
#define	RES_MAXNDOTS		15	/* should reflect bit field size */

struct __res_state {
	int	retrans;	 	/* retransmition time interval */
	int	retry;			/* number of times to retransmit */
	u_long	options;		/* option flags - see below. */
	int	nscount;		/* number of name servers */
	struct sockaddr_in
		nsaddr_list[MAXNS];	/* address of name server */
#define	nsaddr	nsaddr_list[0]		/* for backward compatibility */
	u_short	id;			/* current packet id */
	char	*dnsrch[MAXDNSRCH+1];	/* components of domain to search */
	char	defdname[MAXDNAME];	/* default domain */
	u_long	pfcode;			/* RES_PRF_ flags - see below. */
	unsigned ndots:4;		/* threshold for initial abs. query */
	unsigned nsort:4;		/* number of elements in sort_list[] */
	char	unused[3];
	struct {
		struct in_addr	addr;
		u_int32_t	mask;
	} sort_list[MAXRESOLVSORT];
	char	pad[72];		/* On an i386 this means 512b total. */
};

/*
 * Resolver options (keep these in synch with res_debug.c, please)
 */
#define RES_INIT	0x00000001	/* address initialized */
#define RES_DEBUG	0x00000002	/* print debug messages */
#define RES_AAONLY	0x00000004	/* authoritative answers only (!IMPL)*/
#define RES_USEVC	0x00000008	/* use virtual circuit */
#define RES_PRIMARY	0x00000010	/* query primary server only (!IMPL) */
#define RES_IGNTC	0x00000020	/* ignore truncation errors */
#define RES_RECURSE	0x00000040	/* recursion desired */
#define RES_DEFNAMES	0x00000080	/* use default domain name */
#define RES_STAYOPEN	0x00000100	/* Keep TCP socket open */
#define RES_DNSRCH	0x00000200	/* search up local domain tree */
#define	RES_INSECURE1	0x00000400	/* type 1 security disabled */
#define	RES_INSECURE2	0x00000800	/* type 2 security disabled */
#define	RES_NOALIASES	0x00001000	/* shuts off HOSTALIASES feature */

#define RES_DEFAULT	(RES_RECURSE | RES_DEFNAMES | RES_DNSRCH)

/*
 * Resolver "pfcode" values.  Used by dig.
 */
#define	RES_PRF_STATS	0x00000001
/*			0x00000002	*/
#define	RES_PRF_CLASS   0x00000004
#define	RES_PRF_CMD	0x00000008
#define	RES_PRF_QUES	0x00000010
#define	RES_PRF_ANS	0x00000020
#define	RES_PRF_AUTH	0x00000040
#define	RES_PRF_ADD	0x00000080
#define	RES_PRF_HEAD1	0x00000100
#define	RES_PRF_HEAD2	0x00000200
#define	RES_PRF_TTLID	0x00000400
#define	RES_PRF_HEADX	0x00000800
#define	RES_PRF_QUERY	0x00001000
#define	RES_PRF_REPLY	0x00002000
#define	RES_PRF_INIT    0x00004000
/*			0x00008000	*/

/* hooks are still experimental as of 4.9.2 */
typedef enum { res_goahead, res_nextns, res_modified, res_done, res_error }
	res_sendhookact;

typedef res_sendhookact (*res_send_qhook)__P((struct sockaddr_in * const *ns,
					      const u_char **query,
					      int *querylen,
					      u_char *ans,
					      int anssiz,
					      int *resplen));

typedef res_sendhookact (*res_send_rhook)__P((const struct sockaddr_in *ns,
					      const u_char *query,
					      int querylen,
					      u_char *ans,
					      int anssiz,
					      int *resplen));

extern struct __res_state _res;

/* Private routines shared between libc/net, named, nslookup and others. */
#define	dn_skipname	__dn_skipname
#define	fp_query	__fp_query
#define	fp_nquery	__fp_nquery
#define	hostalias	__hostalias
#define	putlong		__putlong
#define	putshort	__putshort
#define	p_class		__p_class
#define	p_time		__p_time
#define	p_type		__p_type
#define	p_cdnname	__p_cdnname
#define	p_cdname	__p_cdname
#define	p_fqname	__p_fqname
#define	p_rr		__p_rr
#define	p_option	__p_option
#define	res_randomid	__res_randomid
#define	res_isourserver	__res_isourserver
#define	res_nameinquery	__res_nameinquery
#define	res_queriesmatch __res_queriesmatch

__BEGIN_DECLS
int	 __dn_skipname __P((const u_char *, const u_char *));
void	 __fp_resstat __P((struct __res_state *, FILE *));
void	 __fp_query __P((const u_char *, FILE *));
void	 __fp_nquery __P((const u_char *, int, FILE *));
char	*__hostalias __P((const char *));
void	 __putlong __P((u_int32_t, u_char *));
void	 __putshort __P((u_int16_t, u_char *));
char	*__p_time __P((u_int32_t));
void	 __p_query __P((const u_char *));
const u_char *__p_cdnname __P((const u_char *, const u_char *, int, FILE *));
const u_char *__p_cdname __P((const u_char *, const u_char *, FILE *));
const u_char *__p_fqname __P((const u_char *, const u_char *, FILE *));
const u_char *__p_rr __P((const u_char *, const u_char *, FILE *));
const char *__p_type __P((int));
const char *__p_class __P((int));
const char *__p_option __P((u_long option));
int	 dn_comp __P((const char *, u_char *, int, u_char **, u_char **));
int	 dn_expand __P((const u_char *, const u_char *, const u_char *,
			char *, int));
int	 res_init __P((void));
u_int16_t res_randomid __P((void));
int	 res_query __P((const char *, int, int, u_char *, int));
int	 res_search __P((const char *, int, int, u_char *, int));
int	 res_querydomain __P((const char *, const char *, int, int,
			      u_char *, int));
int	 res_mkquery __P((int, const char *, int, int, const u_char *, int,
			  const u_char *, u_char *, int));
int	 res_send __P((const u_char *, int, u_char *, int));
int	 res_isourserver __P((const struct sockaddr_in *));
int	 res_nameinquery __P((const char *, int, int,
			      const u_char *, const u_char *));
int	 res_queriesmatch __P((const u_char *, const u_char *,
			       const u_char *, const u_char *));
__END_DECLS

#endif /* !_RESOLV_H_ */
