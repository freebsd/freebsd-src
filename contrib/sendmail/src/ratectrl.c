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

#include <ratectrl.h>
SM_RCSID("@(#)$Id: ratectrl.c,v 8.14 2013-11-22 20:51:56 ca Exp $")

static int client_rate __P((time_t, SOCKADDR *, int));
static int total_rate __P((time_t, bool));
static unsigned int gen_hash __P((SOCKADDR *));
static void rate_init __P((void));

/*
**  CONNECTION_RATE_CHECK - updates connection history data
**      and computes connection rate for the given host
**
**	Parameters:
**		hostaddr -- IP address of SMTP client
**		e -- envelope
**
**	Returns:
**		none
**
**	Side Effects:
**		updates connection history
**
**	Warnings:
**		For each connection, this call shall be
**		done only once with the value true for the
**		update parameter.
**		Typically, this call is done with the value
**		true by the father, and once again with
**		the value false by the children.
*/

void
connection_rate_check(hostaddr, e)
	SOCKADDR *hostaddr;
	ENVELOPE *e;
{
	time_t now;
	int totalrate, clientrate;
	static int clientconn = 0;

	now = time(NULL);
#if RATECTL_DEBUG
	sm_syslog(LOG_INFO, NOQID, "connection_rate_check entering...");
#endif

	/* update server connection rate */
	totalrate = total_rate(now, e == NULL);
#if RATECTL_DEBUG
	sm_syslog(LOG_INFO, NOQID, "global connection rate: %d", totalrate);
#endif

	/* update client connection rate */
	clientrate = client_rate(now, hostaddr, e == NULL ? SM_CLFL_UPDATE : SM_CLFL_NONE);

	if (e == NULL)
		clientconn = count_open_connections(hostaddr);

	if (e != NULL)
	{
		char s[16];

		sm_snprintf(s, sizeof(s), "%d", clientrate);
		macdefine(&e->e_macro, A_TEMP, macid("{client_rate}"), s);
		sm_snprintf(s, sizeof(s), "%d", totalrate);
		macdefine(&e->e_macro, A_TEMP, macid("{total_rate}"), s);
		sm_snprintf(s, sizeof(s), "%d", clientconn);
		macdefine(&e->e_macro, A_TEMP, macid("{client_connections}"),
				s);
	}
	return;
}

/*
**  Data declarations needed to evaluate connection rate
*/

static int CollTime = 60;

/*
**  time granularity: 10s (that's one "tick")
**  will be initialised to ConnectionRateWindowSize/CHTSIZE
**  before being used the first time
*/

static int ChtGran = -1;
static CHash_T CHashAry[CPMHSIZE];
static CTime_T srv_Times[CHTSIZE];

#ifndef MAX_CT_STEPS
# define MAX_CT_STEPS	10
#endif

/*
**  RATE_INIT - initialize local data
**
**	Parameters:
**		none
**
**	Returns:
**		none
**
**	Side effects:
**		initializes static global data
*/

static void
rate_init()
{
	if (ChtGran > 0)
		return;
	ChtGran = ConnectionRateWindowSize / CHTSIZE;
	if (ChtGran <= 0)
		ChtGran = 10;
	memset(CHashAry, 0, sizeof(CHashAry));
	memset(srv_Times, 0, sizeof(srv_Times));
	return;
}

/*
**  GEN_HASH - calculate a hash value
**
**	Parameters:
**		saddr - client address
**
**	Returns:
**		hash value
*/

static unsigned int
gen_hash(saddr)
	SOCKADDR *saddr;
{
	unsigned int hv;
	int i;
	int addrlen;
	char *p;
#if HASH_ALG != 1
	int c, d;
#endif

	hv = 0xABC3D20F;
	switch (saddr->sa.sa_family)
	{
#if NETINET
	  case AF_INET:
		p = (char *)&saddr->sin.sin_addr;
		addrlen = sizeof(struct in_addr);
		break;
#endif /* NETINET */
#if NETINET6
	  case AF_INET6:
		p = (char *)&saddr->sin6.sin6_addr;
		addrlen = sizeof(struct in6_addr);
		break;
#endif /* NETINET6 */
	  default:
		/* should not happen */
		return -1;
	}

	/* compute hash value */
	for (i = 0; i < addrlen; ++i, ++p)
#if HASH_ALG == 1
		hv = (hv << 5) ^ (hv >> 23) ^ *p;
	hv = (hv ^ (hv >> 16));
#elif HASH_ALG == 2
	{
		d = *p;
		c = d;
		c ^= c<<6;
		hv += (c<<11) ^ (c>>1);
		hv ^= (d<<14) + (d<<7) + (d<<4) + d;
	}
#elif HASH_ALG == 3
	{
		hv = (hv << 4) + *p;
		d = hv & 0xf0000000;
		if (d != 0)
		{
			hv ^= (d >> 24);
			hv ^= d;
		}
	}
#else /* HASH_ALG == 1 */
# error "unsupported HASH_ALG"
	hv = ((hv << 1) ^ (*p & 0377)) % cctx->cc_size; ???
#endif /* HASH_ALG == 1 */

	return hv;
}

/*
**  CONN_LIMIT - Evaluate connection limits
**
**	Parameters:
**		e -- envelope (_FFR_OCC, for logging only)
**		now - current time in secs
**		saddr - client address
**		clflags - update data / check only / ...
**		hashary - hash array
**		ratelimit - rate limit (_FFR_OCC only)
**		conclimit - concurrency limit (_FFR_OCC only)
**
**	Returns:
#if _FFR_OCC
**		outgoing: limit exceeded?
#endif
**		incoming:
**		  connection rate (connections / ConnectionRateWindowSize)
*/

int
conn_limits(e, now, saddr, clflags, hashary, ratelimit, conclimit)
	ENVELOPE *e;
	time_t now;
	SOCKADDR *saddr;
	int clflags;
	CHash_T hashary[];
	int ratelimit;
	int conclimit;
{
	int i;
	int cnt;
	bool coll;
	CHash_T *chBest = NULL;
	CTime_T *ct = NULL;
	unsigned int ticks;
	unsigned int hv;
#if _FFR_OCC
	bool exceeded = false;
	int *prv, *pcv;
#endif
#if RATECTL_DEBUG || _FFR_OCC
	bool logit = false;
#endif

	cnt = 0;
	hv = gen_hash(saddr);
	ticks = now / ChtGran;

	coll = true;
	for (i = 0; i < MAX_CT_STEPS; ++i)
	{
		CHash_T *ch = &hashary[(hv + i) & CPMHMASK];

#if NETINET
		if (saddr->sa.sa_family == AF_INET &&
		    ch->ch_Family == AF_INET &&
		    (saddr->sin.sin_addr.s_addr == ch->ch_Addr4.s_addr ||
		     ch->ch_Addr4.s_addr == 0))
		{
			chBest = ch;
			coll = false;
			break;
		}
#endif /* NETINET */
#if NETINET6
		if (saddr->sa.sa_family == AF_INET6 &&
		    ch->ch_Family == AF_INET6 &&
		    (IN6_ARE_ADDR_EQUAL(&saddr->sin6.sin6_addr,
				       &ch->ch_Addr6) != 0 ||
		     IN6_IS_ADDR_UNSPECIFIED(&ch->ch_Addr6)))
		{
			chBest = ch;
			coll = false;
			break;
		}
#endif /* NETINET6 */
		if (chBest == NULL || ch->ch_LTime == 0 ||
		    ch->ch_LTime < chBest->ch_LTime)
			chBest = ch;
	}

	/* Let's update data... */
	if ((clflags & (SM_CLFL_UPDATE|SM_CLFL_EXC)) != 0)
	{
		if (coll && (now - chBest->ch_LTime < CollTime))
		{
			/*
			**  increment the number of collisions last
			**  CollTime for this client
			*/

			chBest->ch_colls++;

			/*
			**  Maybe shall log if collision rate is too high...
			**  and take measures to resize tables
			**  if this is the case
			*/
		}

		/*
		**  If it's not a match, then replace the data.
		**  Note: this purges the history of a colliding entry,
		**  which may cause "overruns", i.e., if two entries are
		**  "cancelling" each other out, then they may exceed
		**  the limits that are set. This might be mitigated a bit
		**  by the above "best of 5" function however.
		**
		**  Alternative approach: just use the old data, which may
		**  cause false positives however.
		**  To activate this, deactivate the memset() call.
		*/

		if (coll)
		{
#if NETINET
			if (saddr->sa.sa_family == AF_INET)
			{
				chBest->ch_Family = AF_INET;
				chBest->ch_Addr4 = saddr->sin.sin_addr;
			}
#endif /* NETINET */
#if NETINET6
			if (saddr->sa.sa_family == AF_INET6)
			{
				chBest->ch_Family = AF_INET6;
				chBest->ch_Addr6 = saddr->sin6.sin6_addr;
			}
#endif /* NETINET6 */
			memset(chBest->ch_Times, '\0',
			       sizeof(chBest->ch_Times));
		}

		chBest->ch_LTime = now;
		ct = &chBest->ch_Times[ticks % CHTSIZE];

		if (ct->ct_Ticks != ticks)
		{
			ct->ct_Ticks = ticks;
			ct->ct_Count = 0;
		}
		if ((clflags & SM_CLFL_UPDATE) != 0)
			++ct->ct_Count;
	}

	/* Now let's count connections on the window */
	for (i = 0; i < CHTSIZE; ++i)
	{
		CTime_T *cth;

		cth = &chBest->ch_Times[i];
		if (cth->ct_Ticks <= ticks && cth->ct_Ticks >= ticks - CHTSIZE)
			cnt += cth->ct_Count;
	}
#if _FFR_OCC
	prv = pcv = NULL;
	if (ct != NULL && ((clflags & SM_CLFL_EXC) != 0))
	{
		if (ratelimit > 0)
		{
			if (cnt < ratelimit)
				prv = &(ct->ct_Count);
			else
				exceeded = true;
		}
		else if (ratelimit < 0 && ct->ct_Count > 0)
			--ct->ct_Count;
	}

	if (chBest != NULL && ((clflags & SM_CLFL_EXC) != 0))
	{
		if (conclimit > 0)
		{
			if (chBest->ch_oc < conclimit)
				pcv = &(chBest->ch_oc);
			else
				exceeded = true;
		}
		else if (conclimit < 0 && chBest->ch_oc > 0)
			--chBest->ch_oc;
	}
#endif

#if RATECTL_DEBUG
	logit = true;
#endif
#if RATECTL_DEBUG || _FFR_OCC
# if _FFR_OCC
	if (!exceeded)
	{
		if (prv != NULL)
			++*prv, ++cnt;
		if (pcv != NULL)
			++*pcv;
	}
	logit = exceeded || LogLevel > 11;
# endif
	if (logit)
		sm_syslog(LOG_DEBUG, e != NULL ? e->e_id : NOQID,
			"conn_limits: addr=%s, flags=0x%x, rate=%d/%d, conc=%d/%d, exc=%d",
			saddr->sa.sa_family == AF_INET
				? inet_ntoa(saddr->sin.sin_addr) : "???",
			clflags, cnt, ratelimit,
# if _FFR_OCC
			chBest != NULL ? chBest->ch_oc : -1
# else
			-2
# endif
			, conclimit
# if _FFR_OCC
			, exceeded
# else
			, 0
# endif
			);
#endif
#if _FFR_OCC
	if ((clflags & SM_CLFL_EXC) != 0)
		return exceeded;
#endif
	return cnt;
}

/*
**  CLIENT_RATE - Evaluate connection rate per SMTP client
**
**	Parameters:
**		now - current time in secs
**		saddr - client address
**		clflags - update data / check only
**
**	Returns:
**		connection rate (connections / ConnectionRateWindowSize)
**
**	Side effects:
**		update static global data
*/

static int
client_rate(now, saddr, clflags)
	time_t now;
	SOCKADDR *saddr;
	int clflags;
{
	rate_init();
	return conn_limits(NULL, now, saddr, clflags, CHashAry, 0, 0);
}

/*
**  TOTAL_RATE - Evaluate global connection rate
**
**	Parameters:
**		now - current time in secs
**		update - update data / check only
**
**	Returns:
**		connection rate (connections / ConnectionRateWindowSize)
*/

static int
total_rate(now, update)
	time_t now;
	bool update;
{
	int i;
	int cnt = 0;
	CTime_T *ct;
	unsigned int ticks;

	rate_init();
	ticks = now / ChtGran;

	/* Let's update data */
	if (update)
	{
		ct = &srv_Times[ticks % CHTSIZE];

		if (ct->ct_Ticks != ticks)
		{
			ct->ct_Ticks = ticks;
			ct->ct_Count = 0;
		}
		++ct->ct_Count;
	}

	/* Let's count connections on the window */
	for (i = 0; i < CHTSIZE; ++i)
	{
		ct = &srv_Times[i];

		if (ct->ct_Ticks <= ticks && ct->ct_Ticks >= ticks - CHTSIZE)
			cnt += ct->ct_Count;
	}

#if RATECTL_DEBUG
	sm_syslog(LOG_WARNING, NOQID,
		"total: cnt=%d, CHTSIZE=%d, ChtGran=%d",
		cnt, CHTSIZE, ChtGran);
#endif

	return cnt;
}

#if RATECTL_DEBUG || _FFR_OCC
void
dump_ch(fp)
	SM_FILE_T *fp;
{
	int i, j, cnt;
	unsigned int ticks;

	ticks = time(NULL) / ChtGran;
	sm_io_fprintf(fp, SM_TIME_DEFAULT, "dump_ch\n");
	for (i = 0; i < CPMHSIZE; i++)
	{
		CHash_T *ch = &CHashAry[i];
		bool valid;

		valid = false;
# if NETINET
		valid = (ch->ch_Family == AF_INET);
		if (valid)
			sm_io_fprintf(fp, SM_TIME_DEFAULT, "ip=%s ",
				inet_ntoa(ch->ch_Addr4));
# endif /* NETINET */
# if NETINET6
		if (ch->ch_Family == AF_INET6)
		{
			char buf[64], *str;

			valid = true;
			str = anynet_ntop(&ch->ch_Addr6, buf, sizeof(buf));
			if (str != NULL)
				sm_io_fprintf(fp, SM_TIME_DEFAULT, "ip=%s ",
					str);
		}
# endif /* NETINET6 */
		if (!valid)
			continue;

		cnt = 0;
		for (j = 0; j < CHTSIZE; ++j)
		{
			CTime_T *cth;

			cth = &ch->ch_Times[j];
			if (cth->ct_Ticks <= ticks && cth->ct_Ticks >= ticks - CHTSIZE)
				cnt += cth->ct_Count;
		}

		sm_io_fprintf(fp, SM_TIME_DEFAULT, "time=%ld cnt=%d ",
			(long) ch->ch_LTime, cnt);
# if _FFR_OCC
		sm_io_fprintf(fp, SM_TIME_DEFAULT, "oc=%d", ch->ch_oc);
# endif
		sm_io_fprintf(fp, SM_TIME_DEFAULT, "\n");
	}
	sm_io_flush(fp, SM_TIME_DEFAULT);
}

#endif /* RATECTL_DEBUG || _FFR_OCC */
