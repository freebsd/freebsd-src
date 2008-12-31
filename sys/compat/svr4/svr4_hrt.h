/*-
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1994 Christos Zoulas
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * $FreeBSD: src/sys/compat/svr4/svr4_hrt.h,v 1.4.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef	_SVR4_HRT_H_
#define	_SVR4_HRT_H_

#define SVR4_HRT_CNTL		0
#define SVR4_HRT_CNTL_RES	0
#define SVR4_HRT_CNTL_TOFD	1
#define SVR4_HRT_CNTL_START	2
#define SVR4_HRT_CNTL_GET	3

#define SVR4_HRT_ALRM		1
#define SVR4_HRT_ALRM_DO	4
#define SVR4_HRT_ALRM_REP	5
#define SVR4_HRT_ALRM_TOD	6
#define SVR4_HRT_ALRM_FUTREP	7
#define SVR4_HRT_ALRM_TODREP	8
#define SVR4_HRT_ALRM_PEND	9

#define SVR4_HRT_SLP		2
#define SVR4_HRT_SLP_INT	10
#define SVR4_HRT_SLP_TOD	11

#define SVR4_HRT_BSD		12
#define SVR4_HRT_BSD_PEND	13
#define SVR4_HRT_BSD_REP1	14
#define SVR4_HRT_BSD_REP2	15
#define SVR4_HRT_BSD_CANCEL	16

#define SVR4_HRT_CAN		3

#define	SVR4_HRT_SEC		         1
#define	SVR4_HRT_MSEC		      1000
#define SVR4_HRT_USEC		   1000000
#define SVR4_HRT_NSEC		1000000000

#define SVR4_HRT_TRUNC	0
#define SVR4_HRT_RND	1

typedef	struct {
	u_long	i_word1;
	u_long	i_word2;
	int	i_clock;
} svr4_hrt_interval_t;

typedef struct {
	u_long	h_sec;
	long	h_rem;
	u_long	h_res;
} svr4_hrt_time_t;

#define	SVR4_HRT_DONE	1
#define	SVR4_HRT_ERROR	2	

#define SVR4_HRT_CLK_STD	1
#define SVR4_HRT_CLK_USERVIRT	2
#define SVR4_HRT_CLK_PROCVIRT	4	

#endif /* !_SVR4_HRT_H_ */
