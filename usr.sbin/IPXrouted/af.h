/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1995 John Hay.  All rights reserved.
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
 *	@(#)af.h	5.1 (Berkeley) 6/4/85 (routed/af.h)
 *
 *	@(#)af.h	8.1 (Berkeley) 6/5/93
 *
 *	$Id: af.h,v 1.3 1995/10/11 18:57:09 jhay Exp $
 */

/*
 * Routing table management daemon.
 */

/*
 * Structure returned by af_hash routines.
 */
struct afhash {
	u_int	afh_hosthash;		/* host based hash */
	u_int	afh_nethash;		/* network based hash */
};

/*
 * Per address family routines.
 */
typedef void af_hash_t(struct sockaddr *, struct afhash *);
typedef int  af_netmatch_t(struct sockaddr *, struct sockaddr *);
typedef void af_output_t(int, int, struct sockaddr *, int);
typedef int  af_portmatch_t(struct sockaddr *);
typedef int  af_portcheck_t(struct sockaddr *);
typedef int  af_checkhost_t(struct sockaddr *);
typedef int  af_ishost_t(struct sockaddr *);
typedef void af_canon_t(struct sockaddr *);

struct afswitch {
	af_hash_t	*af_hash;	/* returns keys based on address */
	af_netmatch_t	*af_netmatch;	/* verifies net # matching */
	af_output_t	*af_output;	/* interprets address for sending */
	af_portmatch_t	*af_portmatch;	/* packet from some other router? */
	af_portcheck_t	*af_portcheck;	/* packet from privileged peer? */
	af_checkhost_t	*af_checkhost;	/* tells if address for host or net */
	af_ishost_t	*af_ishost;	/* tells if address is valid */
	af_canon_t	*af_canon;	/* canonicalize address for compares */
};

struct	afswitch afswitch[AF_MAX];	/* table proper */
