/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	From:	@(#)nfsrvcache.h	7.3 (Berkeley) 6/28/90
 *	$Id: nfsrvcache.h,v 1.2 1993/09/09 22:06:26 rgrimes Exp $
 */

#ifndef __h_nfsrvcache
#define __h_nfsrvcache 1

/*
 * Definitions for the server recent request cache
 */

#define	NFSRVCACHESIZ	128
#define	NFSRCHSZ	32

struct nfsrvcache {
	struct	nfsrvcache *rc_chain[2];	/* Hash chain links */
	struct	nfsrvcache *rc_next;	/* Lru list */
	struct	nfsrvcache *rc_prev;
	int	rc_state;		/* Current state of request */
	int	rc_flag;		/* Flag bits */
	struct	mbuf rc_nam;		/* Sockaddr of requestor */
	u_long	rc_xid;			/* rpc id number */
	int	rc_proc;		/* rpc proc number */
	long	rc_timestamp;		/* Time stamp */
	union {
		struct mbuf *rc_repmb;	/* Reply mbuf list OR */
		int rc_repstat;		/* Reply status */
	} rc_un;
};

#define	rc_forw		rc_chain[0]
#define	rc_back		rc_chain[1]
#define	rc_status	rc_un.rc_repstat
#define	rc_reply	rc_un.rc_repmb

#define	put_at_head(rp) \
		(rp)->rc_prev->rc_next = (rp)->rc_next; \
		(rp)->rc_next->rc_prev = (rp)->rc_prev; \
		(rp)->rc_next = nfsrvcachehead.rc_next; \
		(rp)->rc_next->rc_prev = (rp); \
		nfsrvcachehead.rc_next = (rp); \
		(rp)->rc_prev = &nfsrvcachehead

/* Cache entry states */
#define	RC_UNUSED	0
#define	RC_INPROG	1
#define	RC_DONE		2

/* Return values */
#define	RC_DROPIT	0
#define	RC_REPLY	1
#define	RC_DOIT		2

/* Flag bits */
#define	RC_LOCKED	0x1
#define	RC_WANTED	0x2
#define	RC_REPSTATUS	0x4
#define	RC_REPMBUF	0x8

/* Delay time after completion that request is dropped */
#define	RC_DELAY	2		/* seconds */


#endif /* __h_nfsrvcache */
