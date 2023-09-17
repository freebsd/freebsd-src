/*
 * Copyright (c) 2003 Proofpoint, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 * Contributed by Jose Marcio Martins da Cruz - Ecole des Mines de Paris
 *   Jose-Marcio.Martins@ensmp.fr
 */

/* a part of this code is based on inetd.c for which this copyright applies: */
/*
 * Copyright (c) 1983, 1991, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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

#ifndef RATECTRL_H
#define RATECTRL_H 1

#include <sendmail.h>

/*
**  stuff included - given some warnings (inet_ntoa)
**	- surely not everything is needed
*/

#if NETINET || NETINET6
# include <arpa/inet.h>
#endif

#include <sm/time.h>

#ifndef HASH_ALG
# define HASH_ALG	2
#endif

#ifndef RATECTL_DEBUG
# define RATECTL_DEBUG  0
#endif

/* this should be a power of 2, otherwise CPMHMASK doesn't work well */
#ifndef CPMHSIZE
# define CPMHSIZE	1024
#endif

#define CPMHMASK	(CPMHSIZE-1)
#define CHTSIZE		6

/* Number of connections for a certain "tick" */
typedef struct CTime
{
	unsigned long	ct_Ticks;
	int		ct_Count;
}
CTime_T;

typedef struct CHash
{
#if NETINET6 && NETINET
	union
	{
		struct in_addr	c4_Addr;
		struct in6_addr	c6_Addr;
	} cu_Addr;
# define ch_Addr4	cu_Addr.c4_Addr
# define ch_Addr6	cu_Addr.c6_Addr
#else /* NETINET6 && NETINET */
# if NETINET6
	struct in6_addr	ch_Addr;
#  define ch_Addr6	ch_Addr
# else /* NETINET6 */
	struct in_addr ch_Addr;
#  define ch_Addr4	ch_Addr
# endif /* NETINET6 */
#endif /* NETINET6 && NETINET */

	int		ch_Family;
	time_t		ch_LTime;
	unsigned long	ch_colls;

	/* 6 buckets for ticks: 60s */
	CTime_T		ch_Times[CHTSIZE];
#if _FFR_OCC
	int		ch_oc;	/* open connections */
#endif
}
CHash_T;

#define SM_CLFL_NONE	0x00
#define SM_CLFL_UPDATE	0x01
#define SM_CLFL_EXC	0x02	/* check if limit is exceeded */

extern void	connection_rate_check __P((SOCKADDR *, ENVELOPE *));
extern int	conn_limits __P((ENVELOPE *, time_t, SOCKADDR *, int, CHash_T *, int, int));
extern bool	occ_exceeded __P((ENVELOPE *, MCI *, const char *, SOCKADDR *));
extern bool	occ_close __P((ENVELOPE *, MCI *, const char *, SOCKADDR *));
extern void	dump_ch __P((SM_FILE_T *));
#endif /* ! RATECTRL_H */
