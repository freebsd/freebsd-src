/*
 * Copyright (c) 1995, Mike Mitchell
 * Copyright (c) 1984, 1988, 1993
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
 *	@(#)ipx_error.h
 *
 * $FreeBSD$
 */

#ifndef _NETIPX_IPX_ERROR_H_
#define	_NETIPX_IPX_ERROR_H_

/*
 * IPX error messages
 */

struct ipx_errp {
	u_short		ipx_err_num;		/* Error Number */
	u_short		ipx_err_param;		/* Error Parameter */
	struct ipx	ipx_err_ipx;		/* Initial segment of offending
						   packet */
	u_char		ipx_err_lev2[12];	/* at least this much higher
						   level protocol */
};
struct  ipx_epipx {
	struct ipx ipx_ep_ipx;
	struct ipx_errp ipx_ep_errp;
};

#define	IPX_ERR_UNSPEC	0	/* Unspecified Error detected at dest. */
#define	IPX_ERR_BADSUM	1	/* Bad Checksum detected at dest */
#define	IPX_ERR_NOSOCK	2	/* Specified socket does not exist at dest*/
#define	IPX_ERR_FULLUP	3	/* Dest. refuses packet due to resource lim.*/
#define	IPX_ERR_UNSPEC_T	0x200	/* Unspec. Error occured before reaching dest*/
#define	IPX_ERR_BADSUM_T	0x201	/* Bad Checksum detected in transit */
#define	IPX_ERR_UNREACH_HOST	0x202	/* Dest cannot be reached from here*/
#define	IPX_ERR_TOO_OLD	0x203	/* Packet x'd 15 routers without delivery*/
#define	IPX_ERR_TOO_BIG	0x204	/* Packet too large to be forwarded through
				   some intermediate gateway.  The error
				   parameter field contains the max packet
				   size that can be accommodated */
#define IPX_ERR_MAX 20

/*
 * Variables related to this implementation
 * of the network systems error message protocol.
 */
struct	ipx_errstat {
/* statistics related to ipx_err packets generated */
	int	ipx_es_error;		/* # of calls to ipx_error */
	int	ipx_es_oldshort;		/* no error 'cuz old ip too short */
	int	ipx_es_oldipx_err;	/* no error 'cuz old was ipx_err */
	int	ipx_es_outhist[IPX_ERR_MAX];
/* statistics related to input messages processed */
	int	ipx_es_badcode;		/* ipx_err_code out of range */
	int	ipx_es_tooshort;		/* packet < IPX_MINLEN */
	int	ipx_es_checksum;		/* bad checksum */
	int	ipx_es_badlen;		/* calculated bound mismatch */
	int	ipx_es_reflect;		/* number of responses */
	int	ipx_es_inhist[IPX_ERR_MAX];
	u_short	ipx_es_codes[IPX_ERR_MAX];/* which error code for outhist
					   since we might not know all */
};

#ifdef KERNEL
extern struct ipx_errstat ipx_errstat;

int	ipx_echo __P((struct mbuf *m));
void	ipx_err_input __P((struct mbuf *m));
int	ipx_err_x __P((int c));
void	ipx_error __P((struct mbuf *om, int type, int param));
void	ipx_printhost __P((struct ipx_addr *addr));
#endif

#endif /* !_NETIPX_IPX_ERROR_H_ */
