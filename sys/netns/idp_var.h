/*
 * Copyright (c) 1984, 1985, 1986, 1987 Regents of the University of California.
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
 *	from: @(#)idp_var.h	7.4 (Berkeley) 6/28/90
 *	$Id: idp_var.h,v 1.4 1993/12/19 00:53:53 wollman Exp $
 */

#ifndef _NETNS_IDP_VAR_H_
#define _NETNS_IDP_VAR_H_ 1

/*
 * IDP Kernel Structures and Variables
 */
struct	idpstat {
	int	idps_badsum;		/* checksum bad */
	int	idps_tooshort;		/* packet too short */
	int	idps_toosmall;		/* not enough data */
	int	idps_badhlen;		/* ip header length < data size */
	int	idps_badlen;		/* ip length < ip header length */
};

#ifdef KERNEL
extern struct	idpstat	idpstat;
struct nspcb;
extern void idp_input(struct mbuf *, struct nspcb *);
extern void idp_abort(struct nspcb *, int);
extern void idp_drop(struct nspcb *, int);
extern int idp_output(struct nspcb *, struct mbuf *);
extern int idp_ctloutput(int, struct socket *, int, int, struct mbuf **);
extern int idp_usrreq(struct socket *, int, struct mbuf *, struct mbuf *, 
		      struct mbuf *, struct mbuf *);
extern int idp_raw_usrreq(struct socket *, int, struct mbuf *, struct mbuf *,
			  struct mbuf *, struct mbuf *);


#endif /* KERNEL */
#endif /* _NETNS_IDP_VAR_H_ */
