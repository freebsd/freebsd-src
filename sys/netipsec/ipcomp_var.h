/*	$FreeBSD$	*/
/*	$KAME: ipcomp.h,v 1.8 2000/09/26 07:55:14 itojun Exp $	*/

/*-
 * Copyright (C) 1999 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
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
 */

#ifndef _NETIPSEC_IPCOMP_VAR_H_
#define _NETIPSEC_IPCOMP_VAR_H_

/*
 * These define the algorithm indices into the histogram.  They're
 * presently based on the PF_KEY v2 protocol values which is bogus;
 * they should be decoupled from the protocol at which time we can
 * pack them and reduce the size of the array to a minimum.
 */
#define	IPCOMP_ALG_MAX	8

struct ipcompstat {
	u_int32_t	ipcomps_hdrops;	/* Packet shorter than header shows */
	u_int32_t	ipcomps_nopf;	/* Protocol family not supported */
	u_int32_t	ipcomps_notdb;
	u_int32_t	ipcomps_badkcr;
	u_int32_t	ipcomps_qfull;
	u_int32_t	ipcomps_noxform;
	u_int32_t	ipcomps_wrap;
	u_int32_t	ipcomps_input;	/* Input IPcomp packets */
	u_int32_t	ipcomps_output;	/* Output IPcomp packets */
	u_int32_t	ipcomps_invalid;/* Trying to use an invalid TDB */
	u_int64_t	ipcomps_ibytes;	/* Input bytes */
	u_int64_t	ipcomps_obytes;	/* Output bytes */
	u_int32_t	ipcomps_toobig;	/* Packet got > IP_MAXPACKET */
	u_int32_t	ipcomps_pdrops;	/* Packet blocked due to policy */
	u_int32_t	ipcomps_crypto;	/* "Crypto" processing failure */
	u_int32_t	ipcomps_hist[IPCOMP_ALG_MAX];/* Per-algorithm op count */
};

#ifdef _KERNEL
extern	int ipcomp_enable;
extern	struct ipcompstat ipcompstat;
#endif /* _KERNEL */
#endif /*_NETIPSEC_IPCOMP_VAR_H_*/
