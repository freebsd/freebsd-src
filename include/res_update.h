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
 */

/* $FreeBSD$ */

#ifndef _RES_UPDATE_H_
#define _RES_UPDATE_H_

/*
 * This RR-like structure is particular to UPDATE.
 */
struct ns_updrec {
	struct ns_updrec *r_prev;	/* prev record */
	struct ns_updrec *r_next;	/* next record */
	u_int8_t	r_section;	/* ZONE/PREREQUISITE/UPDATE */
	char *		r_dname;	/* owner of the RR */
	u_int16_t	r_class;	/* class number */
	u_int16_t	r_type;		/* type number */
	u_int32_t	r_ttl;		/* time to live */
	u_char *	r_data;		/* rdata fields as text string */
	u_int16_t	r_size;		/* size of r_data field */
	int		r_opcode;	/* type of operation */
	/* following fields for private use by the resolver/server routines */
	struct ns_updrec *r_grpnext;	/* next record when grouped */
	struct databuf *r_dp;		/* databuf to process */
	struct databuf *r_deldp;	/* databuf's deleted/overwritten */
	u_int16_t	r_zone;		/* zone number on server */
};
typedef struct ns_updrec ns_updrec;

#define	res_freeupdrec	__res_freeupdrec
#define	res_mkupdate	__res_mkupdate
#define	res_mkupdrec	__res_mkupdrec
#define	res_nmkupdate	__res_nmkupdate
#define	res_nupdate	__res_nupdate
#if 0
#define	res_update	__res_update
#endif

__BEGIN_DECLS
void		res_freeupdrec(ns_updrec *);
int		res_mkupdate(ns_updrec *, u_char *, int);
ns_updrec *	res_mkupdrec(int, const char *, u_int, u_int, u_long);
int		res_nmkupdate(res_state, ns_updrec *, u_char *, int);
int		res_nupdate(res_state, ns_updrec *, ns_tsig_key *);
int		res_update(ns_updrec *);
__END_DECLS

#endif /* _RES_UPDATE_H_ */
