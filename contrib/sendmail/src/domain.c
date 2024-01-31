/*
 * Copyright (c) 1998-2004, 2006, 2010, 2020-2023 Proofpoint, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1986, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#include <sendmail.h>
#include "map.h"

#if NAMED_BIND
SM_RCSID("@(#)$Id: domain.c,v 8.205 2013-11-22 20:51:55 ca Exp $ (with name server)")
#else
SM_RCSID("@(#)$Id: domain.c,v 8.205 2013-11-22 20:51:55 ca Exp $ (without name server)")
#endif

#include <sm/sendmail.h>

#if NAMED_BIND
# include <arpa/inet.h>
# include "sm_resolve.h"
# if DANE
#  include <tls.h>
#  ifndef SM_NEG_TTL
#   define SM_NEG_TTL 60 /* "negative" TTL */
#  endif
# endif

#if USE_EAI
#include <unicode/uidna.h>
#endif


# ifndef MXHOSTBUFSIZE
#  define MXHOSTBUFSIZE	(128 * MAXMXHOSTS)
# endif

static char	MXHostBuf[MXHOSTBUFSIZE];
# if (MXHOSTBUFSIZE < 2) || (MXHOSTBUFSIZE >= INT_MAX/2)
	ERROR: _MXHOSTBUFSIZE is out of range
# endif

# ifndef MAXDNSRCH
#  define MAXDNSRCH	6	/* number of possible domains to search */
# endif

# ifndef RES_DNSRCH_VARIABLE
#  define RES_DNSRCH_VARIABLE	_res.dnsrch
# endif

# ifndef HFIXEDSZ
#  define HFIXEDSZ	12	/* sizeof(HEADER) */
# endif

# define MAXCNAMEDEPTH	10	/* maximum depth of CNAME recursion */

# if defined(__RES) && (__RES >= 19940415)
#  define RES_UNC_T	char *
# else
#  define RES_UNC_T	unsigned char *
# endif

static int	mxrand __P((char *));
static int	fallbackmxrr __P((int, unsigned short *, char **));

# if DANE

static void tlsa_rr_print __P((const unsigned char *, unsigned int));

static void
tlsa_rr_print(rr, len)
	const unsigned char *rr;
	unsigned int len;
{
	unsigned int i, l;

	if (!tTd(8, 2))
		return;

	sm_dprintf("len=%u, %02x-%02x-%02x",
		len, (int)rr[0], (int)rr[1], (int)rr[2]);
	l = tTd(8, 8) ? len : 4;
	for (i = 3; i < l; i++)
		sm_dprintf(":%02X", (int)rr[i]);
	sm_dprintf("\n");
}

/*
**  TLSA_RR_CMP -- Compare two TLSA RRs
**
**	Parameters:
**		rr1 -- TLSA RR (entry to be added)
**		l1 -- length of rr1
**		rr2 -- TLSA RR
**		l2 -- length of rr2
**
**	Returns:
**		 0: rr1 == rr2
**		 1: rr1 is unsupported
*/

static int tlsa_rr_cmp __P((unsigned char *, int, unsigned char *, int));

static int
tlsa_rr_cmp(rr1, l1, rr2, l2)
	unsigned char *rr1;
	int l1;
	unsigned char *rr2;
	int l2;
{
/* temporary #if while investigating the implications of the alternative */
#if FULL_COMPARE
	unsigned char r1, r2;
	int cmp;
#endif /* FULL_COMPARE */

	SM_REQUIRE(NULL != rr1);
	SM_REQUIRE(NULL != rr2);
	SM_REQUIRE(l1 > 3);
	SM_REQUIRE(l2 > 3);

#if FULL_COMPARE
	/*
	**  certificate usage
	**  3: cert/fp must match
	**  2: cert/fp must be trust anchor
	*/

	/* preference[]: lower value: higher preference */
	r1 = rr1[0];
	r2 = rr2[0];
	if (r1 != r2)
	{
		int preference[] = { 3, 2, 1, 0 };

		SM_ASSERT(r1 <= SM_ARRAY_SIZE(preference));
		SM_ASSERT(r2 <= SM_ARRAY_SIZE(preference));
		return preference[r1] - preference[r2];
	}

	/*
	**  selector:
	**  0: full cert
	**  1: fp
	*/

	r1 = rr1[1];
	r2 = rr2[1];
	if (r1 != r2)
	{
		int preference[] = { 1, 0 };

		SM_ASSERT(r1 <= SM_ARRAY_SIZE(preference));
		SM_ASSERT(r2 <= SM_ARRAY_SIZE(preference));
		return preference[r1] - preference[r2];
	}

	/*
	**  matching type:
	**  0 -- Exact match
	**  1 -- SHA-256 hash
	**  2 -- SHA-512 hash
	*/

	r1 = rr1[2];
	r2 = rr2[2];
	if (r1 != r2)
	{
		int preference[] = { 2, 0, 1 };

		SM_ASSERT(r1 <= SM_ARRAY_SIZE(preference));
		SM_ASSERT(r2 <= SM_ARRAY_SIZE(preference));
		return preference[r1] - preference[r2];
	}

	/* not the same length despite the same type? */
	if (l1 != l2)
		return 1;
	cmp = memcmp(rr1, rr2, l1);
	if (0 == cmp)
		return 0;
	return 1;
#else /* FULL_COMPARE */
	/* identical? */
	if (l1 == l2 && 0 == memcmp(rr1, rr2, l1))
		return 0;

	/* new entry is unsupported? -> append */
	if (TLSA_UNSUPP == dane_tlsa_chk(rr1, l1, "", false))
		return 1;
	/* current entry is unsupported? -> insert new one */
	if (TLSA_UNSUPP == dane_tlsa_chk(rr2, l2, "", false))
		return -1;

	/* default: preserve order */
	return 1;
#endif /* FULL_COMPARE */
}

/*
**  TLSAINSERT -- Insert a TLSA RR
**
**	Parameters:
**		dane_tlsa -- dane_tlsa entry
**		rr -- TLSA RR
**		pn -- (point to) number of entries
**
**	Returns:
**		SM_SUCCESS: rr inserted
**		SM_NOTDONE: rr not inserted: exists
**		SM_FULL: rr not inserted: no space left
*/

static int tlsainsert __P((dane_tlsa_P, RESOURCE_RECORD_T *, int *));

static int
tlsainsert(dane_tlsa, rr, pn)
	dane_tlsa_P dane_tlsa;
	RESOURCE_RECORD_T *rr;
	int *pn;
{
	int i, l1, ret;
	unsigned char *r1;

	SM_ASSERT(pn != NULL);
	SM_ASSERT(*pn <= MAX_TLSA_RR);
	r1 = rr->rr_u.rr_data;
	l1 = rr->rr_size;

	ret = SM_SUCCESS;
	for (i = 0; i < *pn; i++)
	{
		int r, j;

		r = tlsa_rr_cmp(r1, l1, dane_tlsa->dane_tlsa_rr[i],
			dane_tlsa->dane_tlsa_len[i]);

		if (0 == r)
		{
			if (tTd(8, 80))
				sm_dprintf("func=tlsainsert, i=%d, n=%d, status=exists\n", i, *pn);
			ret = SM_NOTDONE;
			goto done;
		}
		if (r > 0)
			continue;

		if (*pn + 1 >= MAX_TLSA_RR)
		{
			j = MAX_TLSA_RR - 1;
			SM_FREE(dane_tlsa->dane_tlsa_rr[j]);
			dane_tlsa->dane_tlsa_len[j] = 0;
		}
		else
			(*pn)++;

		for (j = MAX_TLSA_RR - 2; j >= i; j--)
		{
			dane_tlsa->dane_tlsa_rr[j + 1] = dane_tlsa->dane_tlsa_rr[j];
			dane_tlsa->dane_tlsa_len[j + 1] = dane_tlsa->dane_tlsa_len[j];
		}
		SM_ASSERT(i < MAX_TLSA_RR);
		dane_tlsa->dane_tlsa_rr[i] = r1;
		dane_tlsa->dane_tlsa_len[i] = l1;
		if (tTd(8, 80))
			sm_dprintf("func=tlsainsert, i=%d, n=%d, status=inserted\n", i, *pn);
		goto added;
	}

	if (*pn + 1 <= MAX_TLSA_RR)
	{
		dane_tlsa->dane_tlsa_rr[*pn] = r1;
		dane_tlsa->dane_tlsa_len[*pn] = l1;
		(*pn)++;
		if (tTd(8, 80))
			sm_dprintf("func=tlsainsert, n=%d, status=appended\n", *pn);
	}
	else
	{
		if (tTd(8, 80))
			sm_dprintf("func=tlsainsert, n=%d, status=full\n", *pn);
		return SM_FULL;
	}

  added:
	/* hack: instead of copying the data, just "take it over" */
	rr->rr_u.rr_data = NULL;
  done:
	return ret;
}

/*
**  TLSAADD -- add TLSA records to dane_tlsa entry
**
**	Parameters:
**		name -- key for stab entry (for debugging output)
**		dr -- DNS reply
**		dane_tlsa -- dane_tlsa entry
**		dnsrc -- DNS lookup return code (h_errno)
**		nr -- current number of TLSA records in dane_tlsa entry
**		pttl -- (pointer to) TTL (in/out)
**		level -- recursion level (CNAMEs)
**
**	Returns:
**		new number of TLSA records
**
**	NOTE: the array for TLSA RRs could be "full" which is not
**		handled well (yet).
*/

static int tlsaadd __P((const char *, DNS_REPLY_T *, dane_tlsa_P, int, int,
			unsigned int *, int));

static int
tlsaadd(name, dr, dane_tlsa, dnsrc, nr, pttl, level)
	const char *name;
	DNS_REPLY_T *dr;
	dane_tlsa_P dane_tlsa;
	int dnsrc;
	int nr;
	unsigned int *pttl;
	int level;
{
	RESOURCE_RECORD_T *rr;
	unsigned int ttl;
	int nprev;

	if (dnsrc != 0)
	{
		if (tTd(8, 2))
			sm_dprintf("tlsaadd, name=%s, prev=%d, dnsrc=%d\n",
				name, dane_tlsa->dane_tlsa_dnsrc, dnsrc);

		/* check previous error and keep the "most important" one? */
		dane_tlsa->dane_tlsa_dnsrc = dnsrc;
#  if DNSSEC_TEST
		if (tTd(8, 110))
			*pttl = tTdlevel(8)-110; /* how to make this an option? */
		else
#  endif
		/* "else" in #if code above */
			*pttl = SM_NEG_TTL;
		return nr;
	}
	if (dr == NULL)
		return nr;
	if (dr->dns_r_h.ad != 1 && Dane == DANE_SECURE)	/* not secure? */
		return nr;
	ttl = *pttl;

	/* first: try to find TLSA records */
	nprev = nr;
	for (rr = dr->dns_r_head; rr != NULL; rr = rr->rr_next)
	{
		int tlsa_chk, r;

		if (rr->rr_type != T_TLSA)
		{
			if (rr->rr_type != T_CNAME && tTd(8, 8))
				sm_dprintf("tlsaadd: name=%s, type=%s\n", name,
					dns_type_to_string(rr->rr_type));
			continue;
		}
		tlsa_chk = dane_tlsa_chk(rr->rr_u.rr_data, rr->rr_size, name,
					true);
		if (TLSA_UNSUPP == tlsa_chk)
			TLSA_SET_FL(dane_tlsa, TLSAFLUNS);
		if (!TLSA_IS_VALID(tlsa_chk))
			continue;
		if (TLSA_IS_SUPPORTED(tlsa_chk))
			TLSA_SET_FL(dane_tlsa, TLSAFLSUP);

		/*
		**  Note: rr_u.rr_data might be NULL after tlsainsert()
		**  for nice debug output: print the data into a string
		**  and then use it after tlsainsert().
		*/

		if (tTd(8, 2))
		{
			sm_dprintf("tlsaadd: name=%s, nr=%d, ", name, nr);
			tlsa_rr_print(rr->rr_u.rr_data, rr->rr_size);
		}
		r = tlsainsert(dane_tlsa, rr, &nr);
		if (SM_FULL == r)
			TLSA_SET_FL(dane_tlsa, TLSAFL2MANY);
		if (tTd(8, 2))
			sm_dprintf("tlsainsert=%d, nr=%d\n", r, nr);

		/* require some minimum TTL? */
		if (ttl > rr->rr_ttl && rr->rr_ttl > 0)
			ttl = rr->rr_ttl;
	}

	if (tTd(8, 2))
	{
		unsigned int ui;

		SM_ASSERT(nr <= MAX_TLSA_RR);
		for (ui = 0; ui < (unsigned int)nr; ui++)
		{
			sm_dprintf("tlsaadd: name=%s, ui=%u, ", name, ui);
			tlsa_rr_print(dane_tlsa->dane_tlsa_rr[ui],
				dane_tlsa->dane_tlsa_len[ui]);
		}
	}

	if (TLSA_IS_FL(dane_tlsa, TLSAFL2MANY))
	{
		if (tTd(8, 20))
			sm_dprintf("tlsaadd: name=%s, rr=%p, nr=%d, toomany=%d\n", name, rr, nr, TLSA_IS_FL(dane_tlsa, TLSAFL2MANY));
	}

	/* second: check for CNAME records, but only if no TLSA RR was added */
	for (rr = dr->dns_r_head; rr != NULL && nprev == nr; rr = rr->rr_next)
	{
		DNS_REPLY_T *drc;
		int err, herr;

		if (rr->rr_type != T_CNAME)
			continue;
		if (level > 1)
		{
			if (tTd(8, 2))
				sm_dprintf("tlsaadd: name=%s, CNAME=%s, level=%d\n",
					name, rr->rr_u.rr_txt, level);
			continue;
		}

		drc = dns_lookup_int(rr->rr_u.rr_txt, C_IN, T_TLSA, 0, 0,
			Dane == DANE_SECURE ? SM_RES_DNSSEC : 0,
			RR_RAW, &err, &herr);

		if (tTd(8, 2))
			sm_dprintf("tlsaadd: name=%s, CNAME=%s, level=%d, dr=%p, ad=%d, err=%d, herr=%d\n",
				name, rr->rr_u.rr_txt, level,
				(void *)drc, drc != NULL ? drc->dns_r_h.ad : -1,
				err, herr);
		nprev = nr = tlsaadd(name, drc, dane_tlsa, herr, nr, pttl,
				level + 1);
		dns_free_data(drc);
		drc = NULL;
	}

	if (TLSA_IS_FL(dane_tlsa, TLSAFLUNS) &&
	    !TLSA_IS_FL(dane_tlsa, TLSAFLSUP) && LogLevel > 9)
	{
		sm_syslog(LOG_NOTICE, NOQID,
			  "TLSA=%s, records=%d%s",
			  name, nr, ONLYUNSUPTLSARR);
	}
	*pttl = ttl;
	return nr;
}

/*
**  GETTLSA -- get TLSA records for named host using DNS
**
**	Parameters:
**		host -- host
**		name -- name for stab entry key (if NULL: host)
**		pste -- (pointer to) stab entry (output)
**		flags -- TLSAFL*
**		mxttl -- TTL of MX (or host)
**		port -- port number used in TLSA queries (_PORT._tcp.)
**
**	Returns:
**		The number of TLSA records found.
**		<0 if there is an internal failure.
**
**	Side effects:
**		Enters TLSA RRs into stab().
**		If the DNS lookup fails temporarily, an "empty" entry is
**		created with that DNS error code.
*/

int
gettlsa(host, name, pste, flags, mxttl, port)
	char *host;
	char *name;
	STAB **pste;
	unsigned long flags;
	unsigned int mxttl;
	unsigned int port;
{
	DNS_REPLY_T *dr;
	dane_tlsa_P dane_tlsa;
	STAB *ste;
	time_t now;
	unsigned int ttl;
	int n_rrs, len, err, herr;
	bool isrname, expired;
	char nbuf[MAXDNAME];
	char key[MAXDNAME];

	SM_REQUIRE(host != NULL);
	if (pste != NULL)
		*pste = NULL;
	if ('\0' == *host)
		return 0;

	expired = false;
	isrname = NULL == name;
	if (isrname)
		name = host;

	/*
	**  If host->MX lookup was not secure then do not look up TLSA RRs.
	**  Note: this is currently a hack: TLSAFLADMX is used as input flag,
	**  it is (SHOULD!) NOT stored in dane_tlsa->dane_tlsa_flags
	*/

	if (DANE_SECURE == Dane && 0 == (TLSAFLADMX & flags) &&
	    0 != (TLSAFLNEW & flags))
	{
		if (tTd(8, 2))
			sm_dprintf("gettlsa: host=%s, flags=%#lx, no ad but Dane=Secure\n",
				host, flags);
		return 0;
	}

	now = 0;
	n_rrs = 0;
	dr = NULL;
	dane_tlsa = NULL;
	len = strlen(name);
	if (len > 1 && name[len - 1] == '.')
	{
		len--;
		name[len] = '\0';
	}
	else
		len = -1;
	if (0 == port || tTd(66, 101))
		port = 25;
	(void) sm_snprintf(key, sizeof(key), "_%u.%s", port, name);
	ste = stab(key, ST_TLSA_RR, ST_FIND);
	if (tTd(8, 2))
		sm_dprintf("gettlsa: host=%s, %s, ste=%p, pste=%p, flags=%#lx, port=%d\n",
			host, isrname ? "" : name, (void *)ste, (void *)pste,
			flags, port);

	if (ste != NULL)
		dane_tlsa = ste->s_tlsa;

#if 0
//	/* Do not reload TLSA RRs if the MX RRs were not securely retrieved. */
//	if (pste != NULL
//	    && dane_tlsa != NULL && TLSA_IS_FL(dane_tlsa, TLSAFLNOADMX)
//	    && DANE_SECURE == Dane)
//		goto end;
#endif
	if (ste != NULL)
	{
		SM_ASSERT(dane_tlsa != NULL);
		now = curtime();
		if (tTd(8, 20))
			sm_dprintf("gettlsa: host=%s, found-ste=%p, ste_flags=%#lx, expired=%d\n", host, ste, ste->s_tlsa->dane_tlsa_flags, dane_tlsa->dane_tlsa_exp <= now);
		if (dane_tlsa->dane_tlsa_exp <= now
		    && 0 == (TLSAFLNOEXP & flags))
		{
			dane_tlsa_clr(dane_tlsa);
			expired = true;
		}
		else
		{
			n_rrs = dane_tlsa->dane_tlsa_n;
			goto end;
		}
	}

	/* get entries if none exist yet? */
	if ((0 == (TLSAFLNEW & flags)) && !expired)
		goto end;

	if (dane_tlsa == NULL)
	{
		dane_tlsa = (dane_tlsa_P) sm_malloc(sizeof(*dane_tlsa));
		if (dane_tlsa == NULL)
		{
			n_rrs = -ENOMEM;
			goto end;
		}
		memset(dane_tlsa, '\0', sizeof(*dane_tlsa));
	}

	/* There are flags to store -- just set those, do nothing else. */
	if (TLSA_STORE_FL(flags))
	{
		dane_tlsa->dane_tlsa_flags = flags;
		ttl = mxttl > 0 ? mxttl: SM_DEFAULT_TTL;
		goto done;
	}

	(void) sm_snprintf(nbuf, sizeof(nbuf), "_%u._tcp.%s", port, host);
	dr = dns_lookup_int(nbuf, C_IN, T_TLSA, 0, 0,
		(TLSAFLADMX & flags) ? SM_RES_DNSSEC : 0,
		RR_RAW, &err, &herr);
	if (tTd(8, 2))
	{
#if 0
/* disabled -- what to do with these two counters? log them "somewhere"? */
//		if (NULL != dr && tTd(8, 12))
//		{
//			RESOURCE_RECORD_T *rr;
//			unsigned int ntlsarrs, usable;
//
//			ntlsarrs = usable = 0;
//			for (rr = dr->dns_r_head; rr != NULL; rr = rr->rr_next)
//			{
//				int tlsa_chk;
//
//				if (rr->rr_type != T_TLSA)
//					continue;
//				++ntlsarrs;
//				tlsa_chk = dane_tlsa_chk(rr->rr_u.rr_data,
//						rr->rr_size, name, false);
//				if (TLSA_IS_SUPPORTED(tlsa_chk))
//					++usable;
//
//			}
//			sm_dprintf("gettlsa: host=%s, ntlsarrs=%u, usable\%u\n", host, ntlsarrs, usable);
//		}
#endif /* 0 */
		sm_dprintf("gettlsa: host=%s, dr=%p, ad=%d, err=%d, herr=%d\n",
			host, (void *)dr,
			dr != NULL ? dr->dns_r_h.ad : -1, err, herr);
	}
	ttl = UINT_MAX;
	n_rrs = tlsaadd(key, dr, dane_tlsa, herr, n_rrs, &ttl, 0);

	/* no valid entries found? */
	if (n_rrs == 0 && !TLSA_RR_TEMPFAIL(dane_tlsa))
	{
		if (tTd(8, 2))
			sm_dprintf("gettlsa: host=%s, n_rrs=%d, herr=%d, status=NOT_ADDED\n",
				host, n_rrs, dane_tlsa->dane_tlsa_dnsrc);
		goto cleanup;
	}

  done:
	dane_tlsa->dane_tlsa_n = n_rrs;
	if (!isrname)
	{
		SM_FREE(dane_tlsa->dane_tlsa_sni);
		dane_tlsa->dane_tlsa_sni = sm_strdup(host);
	}
	if (NULL == ste)
	{
		ste = stab(key, ST_TLSA_RR, ST_ENTER);
		if (NULL == ste)
			goto error;
	}
	ste->s_tlsa = dane_tlsa;
	if (now == 0)
		now = curtime();
	dane_tlsa->dane_tlsa_exp = now + SM_MIN(ttl, SM_DEFAULT_TTL);
	dns_free_data(dr);
	dr = NULL;
	goto end;

  error:
	if (tTd(8, 2))
		sm_dprintf("gettlsa: host=%s, key=%s, status=error\n", host, key);
	n_rrs = -1;
  cleanup:
	if (NULL == ste)
		dane_tlsa_free(dane_tlsa);
	dns_free_data(dr);
	dr = NULL;

  end:
	if (pste != NULL && ste != NULL)
		*pste = ste;
	if (len > 0)
		host[len] = '.';
	return n_rrs;
}
# endif /* DANE */

/*
**  GETFALLBACKMXRR -- get MX resource records for fallback MX host.
**
**	We have to initialize this once before doing anything else.
**	Moreover, we have to repeat this from time to time to avoid
**	stale data, e.g., in persistent queue runners.
**	This should be done in a parent process so the child
**	processes have the right data.
**
**	Parameters:
**		host -- the name of the fallback MX host.
**
**	Returns:
**		number of MX records.
**
**	Side Effects:
**		Populates NumFallbackMXHosts and fbhosts.
**		Sets renewal time (based on TTL).
*/

int NumFallbackMXHosts = 0;	/* Number of fallback MX hosts (after MX expansion) */
static char *fbhosts[MAXMXHOSTS + 1];

int
getfallbackmxrr(host)
	char *host;
{
	int i, rcode;
	int ttl;
	static time_t renew = 0;

# if 0
	/* This is currently done before this function is called. */
	if (SM_IS_EMPTY(host))
		return 0;
# endif /* 0 */
	if (NumFallbackMXHosts > 0 && renew > curtime())
		return NumFallbackMXHosts;

	/*
	**  For DANE we need to invoke getmxrr() to get the TLSA RRs.
	**  Hack: don't do that if its not a FQHN (e.g., [localhost])
	**  This also triggers for IPv4 addresses, but not IPv6!
	*/

	if (host[0] == '[' && (!Dane || strchr(host, '.') == NULL))
	{
		fbhosts[0] = host;
		NumFallbackMXHosts = 1;
	}
	else
	{
		/* free old data */
		for (i = 0; i < NumFallbackMXHosts; i++)
			sm_free(fbhosts[i]);

		/*
		**  Get new data.
		**  Note: passing 0 as port is not correct but we cannot
		**  determine the port number as there is no mailer.
		*/

		NumFallbackMXHosts = getmxrr(host, fbhosts, NULL,
# if DANE
					(DANE_SECURE == Dane) ? ISAD :
# endif
					0, &rcode, &ttl, 0, NULL);
		renew = curtime() + ttl;
		for (i = 0; i < NumFallbackMXHosts; i++)
			fbhosts[i] = newstr(fbhosts[i]);
	}
	if (NumFallbackMXHosts == NULLMX)
		NumFallbackMXHosts = 0;
	return NumFallbackMXHosts;
}

/*
**  FALLBACKMXRR -- add MX resource records for fallback MX host to list.
**
**	Parameters:
**		nmx -- current number of MX records.
**		prefs -- array of preferences.
**		mxhosts -- array of MX hosts (maximum size: MAXMXHOSTS)
**
**	Returns:
**		new number of MX records.
**
**	Side Effects:
**		If FallbackMX was set, it appends the MX records for
**		that host to mxhosts (and modifies prefs accordingly).
*/

static int
fallbackmxrr(nmx, prefs, mxhosts)
	int nmx;
	unsigned short *prefs;
	char **mxhosts;
{
	int i;

	for (i = 0; i < NumFallbackMXHosts && nmx < MAXMXHOSTS; i++)
	{
		if (nmx > 0)
			prefs[nmx] = prefs[nmx - 1] + 1;
		else
			prefs[nmx] = 0;
		mxhosts[nmx++] = fbhosts[i];
	}
	return nmx;
}

# if USE_EAI

/*
**  HN2ALABEL -- convert hostname in U-label format to A-label format
**
**	Parameters:
**		hostname -- hostname in U-label format
**
**	Returns:
**		hostname in A-label format in a local static buffer.
**		It must be copied before the function is called again.
*/

const char *
hn2alabel(hostname)
	const char *hostname;
{
	UErrorCode error = U_ZERO_ERROR;
	UIDNAInfo info = UIDNA_INFO_INITIALIZER;
	UIDNA *idna;
	static char buf[MAXNAME_I];	/* XXX ??? */

	if (str_is_print(hostname))
		return hostname;
	idna = uidna_openUTS46(UIDNA_NONTRANSITIONAL_TO_ASCII, &error);
	(void) uidna_nameToASCII_UTF8(idna, hostname, strlen(hostname),
				     buf, sizeof(buf) - 1,
				     &info, &error);
	uidna_close(idna);
	return buf;
}
# endif /* USE_EAI */

/*
**  GETMXRR -- get MX resource records for a domain
**
**	Parameters:
**		host -- the name of the host to MX [must be x]
**		mxhosts -- a pointer to a return buffer of MX records.
**		mxprefs -- a pointer to a return buffer of MX preferences.
**			If NULL, don't try to populate.
**		flags -- flags:
**			DROPLOCALHOST -- If true, all MX records less preferred
**			than the local host (as determined by $=w) will
**			be discarded.
**			TRYFALLBACK -- add also fallback MX host?
**			ISAD -- host lookup was secure?
**		rcode -- a pointer to an EX_ status code.
**		pttl -- pointer to return TTL (can be NULL).
**		port -- port number used in TLSA queries (_PORT._tcp.)
**		pad -- (output parameter, pointer to) AD flag (can be NULL)
**
**	Returns:
**		The number of MX records found.
**		-1 if there is an internal failure.
**		If no MX records are found, mxhosts[0] is set to host
**			and 1 is returned.
**
**	Side Effects:
**		The entries made for mxhosts point to a static array
**		MXHostBuf[MXHOSTBUFSIZE], so the data needs to be copied,
**		if it must be preserved across calls to this function.
*/

int
getmxrr(host, mxhosts, mxprefs, flags, rcode, pttl, port, pad)
	char *host;
	char **mxhosts;
	unsigned short *mxprefs;
	unsigned int flags;
	int *rcode;
	int *pttl;
	int port;
	int *pad;
{
	register unsigned char *eom, *cp;
	register int i, j, n;
	int nmx = 0;
	register char *bp;
	HEADER *hp;
	querybuf answer;
	int ancount, qdcount, buflen;
	bool seenlocal = false;
	unsigned short pref, type;
	unsigned short localpref = 256;
	char *fallbackMX = FallbackMX;
	bool trycanon = false;
	unsigned short *prefs;
	int (*resfunc) __P((const char *, int, int, u_char *, int));
	unsigned short prefer[MAXMXHOSTS];
	int weight[MAXMXHOSTS];
	int ttl = 0;
	bool ad;
	bool seennullmx = false;
	extern int res_query __P((const char *, int, int, u_char *, int));
	extern int res_search __P((const char *, int, int , u_char *, int));
# if DANE
	bool cname2mx;
	char qname[MAXNAME];	/* EAI: copy of host: ok? */
	unsigned long old_options = 0;
# endif

	if (tTd(8, 2))
		sm_dprintf("getmxrr(%s, droplocalhost=%d, flags=%X, port=%d)\n",
			   host, (flags & DROPLOCALHOST) != 0, flags, port);
	ad = (flags & ISAD) != 0;
	*rcode = EX_OK;
	if (pttl != NULL)
		*pttl = SM_DEFAULT_TTL;
	if (*host == '\0')
		return 0;
# if DANE
	cname2mx = false;
	qname[0] = '\0';
	old_options = _res.options;
	if (ad)
		_res.options |= SM_RES_DNSSEC;
# endif

	if ((fallbackMX != NULL && (flags & DROPLOCALHOST) != 0 &&
	     wordinclass(fallbackMX, 'w')) || (flags & TRYFALLBACK) == 0)
	{
		/* don't use fallback for this pass */
		fallbackMX = NULL;
	}

	if (mxprefs != NULL)
		prefs = mxprefs;
	else
		prefs = prefer;

	/* efficiency hack -- numeric or non-MX lookups */
	if (host[0] == '[')
		goto punt;

# if DANE
	/*
	**  NOTE: This only works if nocanonify is used,
	**  otherwise the name is already rewritten.
	*/

	/* always or only when "needed"? */
	if (DANE_ALWAYS == Dane || (ad && DANE_SECURE == Dane))
		(void) sm_strlcpy(qname, host, sizeof(qname));
# endif /* DANE */

# if USE_EAI
	if (!str_is_print(host))
	{
		/* XXX memory leak? */
		host = sm_rpool_strdup_x(CurEnv->e_rpool, hn2alabel(host));
	}
# endif /* USE_EAI */

	/*
	**  If we don't have MX records in our host switch, don't
	**  try for MX records.  Note that this really isn't "right",
	**  since we might be set up to try NIS first and then DNS;
	**  if the host is found in NIS we really shouldn't be doing
	**  MX lookups.  However, that should be a degenerate case.
	*/

	if (!UseNameServer)
		goto punt;
	if (HasWildcardMX && ConfigLevel >= 6)
		resfunc = res_query;
	else
		resfunc = res_search;
# if DNSSEC_TEST
	if (tTd(8, 110))
		resfunc = tstdns_search;
# endif

	errno = 0;
	hp = (HEADER *)&answer;
	n = (*resfunc)(host, C_IN, T_MX, (unsigned char *) &answer,
		       sizeof(answer));
	if (n < 0)
	{
		if (tTd(8, 1))
# if DNSSEC_TEST
			sm_dprintf("getmxrr: res_search(%s) failed (errno=%d (%s), h_errno=%d (%s))\n",
				host, errno, strerror(errno),
				h_errno, herrno2txt(h_errno));
# else
			sm_dprintf("getmxrr: res_search(%s) failed, h_errno=%d\n",
				host, h_errno);
# endif
		switch (h_errno)
		{
		  case NO_DATA:
			trycanon = true;
			/* FALLTHROUGH */

		  case NO_RECOVERY:
			/* no MX data on this host */
			goto punt;

		  case HOST_NOT_FOUND:
# if BROKEN_RES_SEARCH
		  case 0: /* Ultrix resolver returns failure w/ h_errno=0 */
# endif
			/* host doesn't exist in DNS; might be in /etc/hosts */
			trycanon = true;
			*rcode = EX_NOHOST;
			goto punt;

		  case TRY_AGAIN:
		  case -1:
			/* couldn't connect to the name server */
			if (fallbackMX != NULL)
			{
				/* name server is hosed -- push to fallback */
				nmx = fallbackmxrr(nmx, prefs, mxhosts);
				goto done;
			}
			/* it might come up later; better queue it up */
			*rcode = EX_TEMPFAIL;
			break;

		  default:
			syserr("getmxrr: res_search (%s) failed with impossible h_errno (%d)",
				host, h_errno);
			*rcode = EX_OSERR;
			break;
		}

		/* irreconcilable differences */
		goto error;
	}

	ad = ad && hp->ad;
	if (tTd(8, 2))
		sm_dprintf("getmxrr(%s), hp=%p, ad=%d\n", host, (void*)hp, ad);
	if (pad != NULL)
		*pad = ad;

	/* avoid problems after truncation in tcp packets */
	if (n > sizeof(answer))
		n = sizeof(answer);

	/* find first satisfactory answer */
	cp = (unsigned char *)&answer + HFIXEDSZ;
	eom = (unsigned char *)&answer + n;

	for (qdcount = ntohs((unsigned short) hp->qdcount);
	     qdcount--;
	     cp += n + QFIXEDSZ)
	{
		if ((n = dn_skipname(cp, eom)) < 0)
			goto punt;
	}

	/* NOTE: see definition of MXHostBuf! */
	buflen = sizeof(MXHostBuf) - 1;
	SM_ASSERT(buflen > 0);
	bp = MXHostBuf;
	ancount = ntohs((unsigned short) hp->ancount);

	/* See RFC 1035 for layout of RRs. */
	/* XXX leave room for FallbackMX ? */
	while (--ancount >= 0 && cp < eom && nmx < MAXMXHOSTS - 1)
	{
		if ((n = dn_expand((unsigned char *)&answer, eom, cp,
				   (RES_UNC_T) bp, buflen)) < 0)
			break;
		cp += n;
		GETSHORT(type, cp);
		cp += INT16SZ;		/* skip over class */
		GETLONG(ttl, cp);
		GETSHORT(n, cp);	/* rdlength */
# if DANE
		if (type == T_CNAME)
			cname2mx = true;
# endif
		if (type != T_MX)
		{
			if ((tTd(8, 8) || _res.options & RES_DEBUG)
# if DANE
			    && type != T_RRSIG
# endif
			    )
				sm_dprintf("unexpected answer type %s, size %d\n",
					dns_type_to_string(type), n);
			cp += n;
			continue;
		}
		GETSHORT(pref, cp);
		if ((n = dn_expand((unsigned char *)&answer, eom, cp,
				   (RES_UNC_T) bp, buflen)) < 0)
			break;
		cp += n;
		n = strlen(bp);

		/* Support for RFC7505 "MX 0 ." */
		if (pref == 0 && *bp == '\0')
			seennullmx = true;

		if (wordinclass(bp, 'w'))
		{
			if (tTd(8, 3))
				sm_dprintf("found localhost (%s) in MX list, pref=%d\n",
					bp, pref);
			if ((flags & DROPLOCALHOST) != 0)
			{
				if (!seenlocal || pref < localpref)
					localpref = pref;
				seenlocal = true;
				continue;
			}
			weight[nmx] = 0;
		}
		else
			weight[nmx] = mxrand(bp);
		prefs[nmx] = pref;
		mxhosts[nmx++] = bp;
# if DANE
		if (CHK_DANE(Dane) && port >= 0)
		{
			int nrr;
			unsigned long flags;

			flags = TLSAFLNEW;
			if (pad != NULL && *pad)
				flags |= TLSAFLADMX;
			if (tTd(8, 20))
				sm_dprintf("getmxrr: 1: host=%s, mx=%s, flags=%#lx\n", host, bp, flags);
			nrr = gettlsa(bp, NULL, NULL, flags, ttl, port);

			/* Only check qname if no TLSA RRs were found */
			if (0 == nrr && cname2mx && '\0' != qname[0] &&
			    strcmp(qname, bp))
			{
				if (tTd(8, 20))
					sm_dprintf("getmxrr: 2: host=%s, qname=%s, flags=%#lx\n", host, qname, flags);
				gettlsa(qname, bp, NULL, flags, ttl, port);
			/* XXX is this the right ad flag? */
			}
		}
# endif

		/*
		**  Note: n can be 0 for something like:
		**  host MX 0 .
		**  See RFC 7505
		*/

		bp += n;
		if (0 == n || bp[-1] != '.')
		{
			*bp++ = '.';
			n++;
		}
		*bp++ = '\0';
		if (buflen < n + 1)
		{
			/* don't want to wrap buflen */
			break;
		}
		buflen -= n + 1;
	}

	/* Support for RFC7505 "MX 0 ." */
	if (seennullmx && nmx == 1)
	{
		if (tTd(8, 4))
			sm_dprintf("getmxrr: Null MX record found, domain doesn't accept mail (RFC7505)\n");
		*rcode = EX_UNAVAILABLE;
		return NULLMX;
	}

	/* return only one TTL entry, that should be sufficient */
	if (ttl > 0 && pttl != NULL)
		*pttl = ttl;

	/* sort the records */
	for (i = 0; i < nmx; i++)
	{
		for (j = i + 1; j < nmx; j++)
		{
			if (prefs[i] > prefs[j] ||
			    (prefs[i] == prefs[j] && weight[i] > weight[j]))
			{
				register int temp;
				register char *temp1;

				temp = prefs[i];
				prefs[i] = prefs[j];
				prefs[j] = temp;
				temp1 = mxhosts[i];
				mxhosts[i] = mxhosts[j];
				mxhosts[j] = temp1;
				temp = weight[i];
				weight[i] = weight[j];
				weight[j] = temp;
			}
		}
		if (seenlocal && prefs[i] >= localpref)
		{
			/* truncate higher preference part of list */
			nmx = i;
		}
	}

	/* delete duplicates from list (yes, some bozos have duplicates) */
	for (i = 0; i < nmx - 1; )
	{
		if (!SM_STRCASEEQ(mxhosts[i], mxhosts[i + 1]))
			i++;
		else
		{
			/* compress out duplicate */
			for (j = i + 1; j < nmx; j++)
			{
				mxhosts[j] = mxhosts[j + 1];
				prefs[j] = prefs[j + 1];
			}
			nmx--;
		}
	}

	if (nmx == 0)
	{
punt:
		if (seenlocal)
		{
			struct hostent *h = NULL;

			/*
			**  If we have deleted all MX entries, this is
			**  an error -- we should NEVER send to a host that
			**  has an MX, and this should have been caught
			**  earlier in the config file.
			**
			**  Some sites prefer to go ahead and try the
			**  A record anyway; that case is handled by
			**  setting TryNullMXList.  I believe this is a
			**  bad idea, but it's up to you....
			*/

			if (TryNullMXList)
			{
				SM_SET_H_ERRNO(0);
				errno = 0;
				h = sm_gethostbyname(host, AF_INET);
				if (h == NULL)
				{
					if (errno == ETIMEDOUT ||
					    h_errno == TRY_AGAIN ||
					    (errno == ECONNREFUSED &&
					     UseNameServer))
					{
						*rcode = EX_TEMPFAIL;
						goto error;
					}
# if NETINET6
					SM_SET_H_ERRNO(0);
					errno = 0;
					h = sm_gethostbyname(host, AF_INET6);
					if (h == NULL &&
					    (errno == ETIMEDOUT ||
					     h_errno == TRY_AGAIN ||
					     (errno == ECONNREFUSED &&
					      UseNameServer)))
					{
						*rcode = EX_TEMPFAIL;
						goto error;
					}
# endif /* NETINET6 */
				}
			}

			if (h == NULL)
			{
				*rcode = EX_CONFIG;
				syserr("MX list for %s points back to %s",
				       host, MyHostName);
				goto error;
			}
# if NETINET6
			freehostent(h);
			h = NULL;
# endif
		}
		if (strlen(host) >= sizeof(MXHostBuf))
		{
			*rcode = EX_CONFIG;
			syserr("Host name %s too long",
			       shortenstring(host, MAXSHORTSTR));
			goto error;
		}
		(void) sm_strlcpy(MXHostBuf, host, sizeof(MXHostBuf));
		mxhosts[0] = MXHostBuf;
		prefs[0] = 0;
		if (host[0] == '[')
		{
			register char *p;
# if NETINET6
			struct sockaddr_in6 tmp6;
# endif

			/* this may be an MX suppression-style address */
			p = strchr(MXHostBuf, ']');
			if (p != NULL)
			{
				*p = '\0';

				if (inet_addr(&MXHostBuf[1]) != INADDR_NONE)
				{
					nmx++;
					*p = ']';
				}
# if NETINET6
				else if (anynet_pton(AF_INET6, &MXHostBuf[1],
						     &tmp6.sin6_addr) == 1)
				{
					nmx++;
					*p = ']';
				}
# endif /* NETINET6 */
				else
				{
# if USE_EAI
					char *hn;

					hn = MXHostBuf + 1;
					if (!str_is_print(hn))
					{
						const char *ahn;

						ahn = hn2alabel(hn);
						if (strlen(ahn) >= sizeof(MXHostBuf) - 1)
						{
							*rcode = EX_CONFIG;
							syserr("Encoded host name %s too long",
							       shortenstring(ahn, MAXSHORTSTR));
							goto error;
						}
						(void) sm_strlcpy(hn, ahn, sizeof(MXHostBuf) - 1);
					}
# endif /* USE_EAI */
					trycanon = true;
					mxhosts[0]++;
				}
			}
		}
		if (trycanon &&
		    (n = getcanonname(mxhosts[0], sizeof(MXHostBuf) - 2, false,
				pttl)) != HOST_NOTFOUND)
		{
			/* XXX MXHostBuf == "" ?  is that possible? */
			bp = &MXHostBuf[strlen(MXHostBuf)];
			if (bp[-1] != '.')
			{
				*bp++ = '.';
				*bp = '\0';
			}
			nmx = 1;
# if DANE
			if (tTd(8, 3))
				sm_dprintf("getmxrr=%s, getcanonname=%d\n",
					mxhosts[0], n);
			if (CHK_DANE(Dane) && port >= 0)
			{
				int nrr;
				unsigned long flags;
				unsigned int cttl;

				if (pttl != NULL)
					cttl = *pttl;
				else if (ttl > 0)
					cttl = ttl;
				else
					cttl = SM_DEFAULT_TTL;

				flags = TLSAFLNEW;
				if (ad && HOST_SECURE == n)
				{
					flags |= TLSAFLADMX;
					if (pad != NULL)
						*pad = ad;
				}
				if (TTD(8, 20))
					sm_dprintf("getmxrr: 3: host=%s, mx=%s, flags=%#lx, ad=%d\n",
						host, mxhosts[0], flags, ad);
				nrr = gettlsa(mxhosts[0], NULL, NULL, flags,
						cttl, port);

				/*
				**  Only check qname if no TLSA RRs were found
				**  XXX: what about (temp) DNS errors?
				*/

				if (0 == nrr && '\0' != qname[0] &&
				    strcmp(qname, mxhosts[0]))
				{
					gettlsa(qname, mxhosts[0], NULL, flags,
						cttl, port);
					if (tTd(8, 20))
						sm_dprintf("getmxrr: 4: host=%s, qname=%s, flags=%#lx\n", host, qname, flags);
				/* XXX is this the right ad flag? */
				}
			}
# endif
		}
	}

	/* if we have a default lowest preference, include that */
	if (fallbackMX != NULL && !seenlocal)
	{
		/* TODO: DNSSEC status of fallbacks */
		nmx = fallbackmxrr(nmx, prefs, mxhosts);
	}
    done:
# if DANE
	_res.options = old_options;
# endif
	return nmx;

   error:
# if DANE
	_res.options = old_options;
# endif
	return -1;
}

/*
**  MXRAND -- create a randomizer for equal MX preferences
**
**	If two MX hosts have equal preferences we want to randomize
**	the selection.  But in order for signatures to be the same,
**	we need to randomize the same way each time.  This function
**	computes a pseudo-random hash function from the host name.
**
**	Parameters:
**		host -- the name of the host.
**
**	Returns:
**		A random but repeatable value based on the host name.
*/

static int
mxrand(host)
	register char *host;
{
	int hfunc;
	static unsigned int seed;

	if (seed == 0)
	{
		seed = (int) curtime() & 0xffff;
		if (seed == 0)
			seed++;
	}

	if (tTd(17, 9))
		sm_dprintf("mxrand(%s)", host);

	hfunc = seed;
	while (*host != '\0')
	{
		int c = *host++;

		if (isascii(c) && isupper(c))
			c = tolower(c);
		hfunc = ((hfunc << 1) ^ c) % 2003;
	}

	hfunc &= 0xff;
	hfunc++;

	if (tTd(17, 9))
		sm_dprintf(" = %d\n", hfunc);
	return hfunc;
}
/*
**  BESTMX -- find the best MX for a name
**
**	This is really a hack, but I don't see any obvious way
**	to generalize it at the moment.
*/

/* ARGSUSED3 */
char *
bestmx_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	int nmx;
	int saveopts = _res.options;
	int i;
	ssize_t len = 0;
	char *result;
	char *mxhosts[MAXMXHOSTS + 1];
# if _FFR_BESTMX_BETTER_TRUNCATION
	char *buf;
# else
	char *p;
	char buf[PSBUFSIZE / 2];
# endif

	_res.options &= ~(RES_DNSRCH|RES_DEFNAMES);
	nmx = getmxrr(name, mxhosts, NULL, 0, statp, NULL, -1, NULL);
	_res.options = saveopts;
	if (nmx <= 0)
		return NULL;
	if (bitset(MF_MATCHONLY, map->map_mflags))
		return map_rewrite(map, name, strlen(name), NULL);
	if ((map->map_coldelim == '\0') || (nmx == 1))
		return map_rewrite(map, mxhosts[0], strlen(mxhosts[0]), av);

	/*
	**  We were given a -z flag (return all MXs) and there are multiple
	**  ones.  We need to build them all into a list.
	*/

# if _FFR_BESTMX_BETTER_TRUNCATION
	for (i = 0; i < nmx; i++)
	{
		if (strchr(mxhosts[i], map->map_coldelim) != NULL)
		{
			syserr("bestmx_map_lookup: MX host %.64s includes map delimiter character 0x%02X",
			       mxhosts[i], map->map_coldelim);
			return NULL;
		}
		len += strlen(mxhosts[i]) + 1;
		if (len < 0)
		{
			len -= strlen(mxhosts[i]) + 1;
			break;
		}
	}
	buf = (char *) sm_malloc(len);
	if (buf == NULL)
	{
		*statp = EX_UNAVAILABLE;
		return NULL;
	}
	*buf = '\0';
	for (i = 0; i < nmx; i++)
	{
		int end;

		end = sm_strlcat(buf, mxhosts[i], len);
		if (i != nmx && end + 1 < len)
		{
			buf[end] = map->map_coldelim;
			buf[end + 1] = '\0';
		}
	}

	/* Cleanly truncate for rulesets */
	truncate_at_delim(buf, PSBUFSIZE / 2, map->map_coldelim);
# else /* _FFR_BESTMX_BETTER_TRUNCATION */
	p = buf;
	for (i = 0; i < nmx; i++)
	{
		size_t slen;

		if (strchr(mxhosts[i], map->map_coldelim) != NULL)
		{
			syserr("bestmx_map_lookup: MX host %.64s includes map delimiter character 0x%02X",
			       mxhosts[i], map->map_coldelim);
			return NULL;
		}
		slen = strlen(mxhosts[i]);
		if (len + slen + 2 > sizeof(buf))
			break;
		if (i > 0)
		{
			*p++ = map->map_coldelim;
			len++;
		}
		(void) sm_strlcpy(p, mxhosts[i], sizeof(buf) - len);
		p += slen;
		len += slen;
	}
# endif /* _FFR_BESTMX_BETTER_TRUNCATION */

	result = map_rewrite(map, buf, len, av);
# if _FFR_BESTMX_BETTER_TRUNCATION
	sm_free(buf);
# endif
	return result;
}
/*
**  DNS_GETCANONNAME -- get the canonical name for named host using DNS
**
**	This algorithm tries to be smart about wildcard MX records.
**	This is hard to do because DNS doesn't tell is if we matched
**	against a wildcard or a specific MX.
**
**	We always prefer A & CNAME records, since these are presumed
**	to be specific.
**
**	If we match an MX in one pass and lose it in the next, we use
**	the old one.  For example, consider an MX matching *.FOO.BAR.COM.
**	A hostname bletch.foo.bar.com will match against this MX, but
**	will stop matching when we try bletch.bar.com -- so we know
**	that bletch.foo.bar.com must have been right.  This fails if
**	there was also an MX record matching *.BAR.COM, but there are
**	some things that just can't be fixed.
**
**	Parameters:
**		host -- a buffer containing the name of the host.
**			This is a value-result parameter.
**		hbsize -- the size of the host buffer.
**		trymx -- if set, try MX records as well as A and CNAME.
**		statp -- pointer to place to store status.
**		pttl -- pointer to return TTL (can be NULL).
**
**	Returns:
**		>0 -- if the host was found.
**		0 -- otherwise.
*/

int
dns_getcanonname(host, hbsize, trymx, statp, pttl)
	char *host;
	int hbsize;
	bool trymx;
	int *statp;
	int *pttl;
{
	register unsigned char *eom, *ap;
	register char *cp;
	register int n;
	HEADER *hp;
	querybuf answer;
	int ancount, qdcount, ret, type, qtype, initial, loopcnt, ttl, sli;
	char **domain;
	char *dp;
	char *mxmatch;
	bool amatch, gotmx, ad;
	char nbuf[SM_MAX(MAXPACKET, MAXDNAME*2+2)];
# if DNSSEC_TEST
#  define ADDSL	1 /* NameSearchList may add another entry to searchlist! */
# else
#  define ADDSL	0
# endif
	char *searchlist[MAXDNSRCH + 2 + ADDSL];
# define SLSIZE SM_ARRAY_SIZE(searchlist)
	int (*resqdomain) __P((const char *, const char *, int, int, unsigned char *, int));
# if DANE
	unsigned long old_options = 0;
# endif

	ttl = 0;
	gotmx = false;
	ad = true;
	if (tTd(8, 2))
		sm_dprintf("dns_getcanonname(%s, trymx=%d)\n", host, trymx);

	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
	{
		*statp = EX_UNAVAILABLE;
		return HOST_NOTFOUND;
	}

# if DANE
	old_options = _res.options;
	if (DANE_SECURE == Dane)
		_res.options |= SM_RES_DNSSEC;
# endif

	*statp = EX_OK;
	resqdomain = res_querydomain;
# if DNSSEC_TEST
	if (tTd(8, 110))
		resqdomain = tstdns_querydomain;
# endif

	/*
	**  Initialize domain search list.  If there is at least one
	**  dot in the name, search the unmodified name first so we
	**  find "vse.CS" in Czechoslovakia instead of in the local
	**  domain (e.g., vse.CS.Berkeley.EDU).  Note that there is no
	**  longer a country named Czechoslovakia but this type of problem
	**  is still present.
	**
	**  Older versions of the resolver could create this
	**  list by tearing apart the host name.
	*/

	loopcnt = 0;
cnameloop:
	/* Check for dots in the name */
	for (cp = host, n = 0; *cp != '\0'; cp++)
		if (*cp == '.')
			n++;

	/*
	**  Build the search list.
	**	If there is at least one dot in name, start with a null
	**	domain to search the unmodified name first.
	**	If name does not end with a dot and search up local domain
	**	tree desired, append each local domain component to the
	**	search list; if name contains no dots and default domain
	**	name is desired, append default domain name to search list;
	**	else if name ends in a dot, remove that dot.
	*/

	sli = 0;
	if (n > 0)
		searchlist[sli++] = "";
# if DNSSEC_TEST
	if (NameSearchList != NULL)
	{
		SM_ASSERT(sli < SLSIZE);
		searchlist[sli++] = NameSearchList;
	}
# endif
	if (n >= 0 && *--cp != '.' && bitset(RES_DNSRCH, _res.options))
	{
		/* make sure there are less than MAXDNSRCH domains */
		for (domain = RES_DNSRCH_VARIABLE, ret = 0;
		     *domain != NULL && ret < MAXDNSRCH && sli < SLSIZE;
		     ret++)
			searchlist[sli++] = *domain++;
	}
	else if (n == 0 && bitset(RES_DEFNAMES, _res.options))
	{
		SM_ASSERT(sli < SLSIZE);
		searchlist[sli++] = _res.defdname;
	}
	else if (*cp == '.')
	{
		*cp = '\0';
	}
	SM_ASSERT(sli < SLSIZE);
	searchlist[sli] = NULL;

	/*
	**  Now loop through the search list, appending each domain in turn
	**  name and searching for a match.
	*/

	mxmatch = NULL;
	initial = T_A;
# if NETINET6
	if (InetMode == AF_INET6)
		initial = T_AAAA;
# endif
	qtype = initial;

	for (sli = 0; sli < SLSIZE; )
	{
		dp = searchlist[sli];
		if (NULL == dp)
			break;
		if (qtype == initial)
			gotmx = false;
		if (tTd(8, 5))
			sm_dprintf("dns_getcanonname: trying %s.%s (%s)\n",
				host, dp,
# if NETINET6
				qtype == T_AAAA ? "AAAA" :
# endif
				qtype == T_A ? "A" :
				qtype == T_MX ? "MX" :
				"???");
		errno = 0;
		hp = (HEADER *) &answer;
		ret = (*resqdomain)(host, dp, C_IN, qtype,
				      answer.qb2, sizeof(answer.qb2));
		if (ret <= 0)
		{
			int save_errno = errno;

			if (tTd(8, 7))
				sm_dprintf("\tNO: errno=%d, h_errno=%d\n",
					   save_errno, h_errno);

			if (save_errno == ECONNREFUSED || h_errno == TRY_AGAIN)
			{
				/*
				**  the name server seems to be down or broken.
				*/

				SM_SET_H_ERRNO(TRY_AGAIN);
				if (*dp == '\0')
				{
					if (*statp == EX_OK)
						*statp = EX_TEMPFAIL;
					goto nexttype;
				}
				*statp = EX_TEMPFAIL;

				if (WorkAroundBrokenAAAA)
				{
					/*
					**  Only return if not TRY_AGAIN as an
					**  attempt with a different qtype may
					**  succeed (res_querydomain() calls
					**  res_query() calls res_send() which
					**  sets errno to ETIMEDOUT if the
					**  nameservers could be contacted but
					**  didn't give an answer).
					*/

					if (save_errno != ETIMEDOUT)
						goto error;
				}
				else
					goto error;
			}

nexttype:
			if (h_errno != HOST_NOT_FOUND)
			{
				/* might have another type of interest */
# if NETINET6
				if (qtype == T_AAAA)
				{
					qtype = T_A;
					continue;
				}
				else
# endif /* NETINET6 */
				if (qtype == T_A && !gotmx &&
				    (trymx || *dp == '\0'))
				{
					qtype = T_MX;
					continue;
				}
			}

			/* definite no -- try the next domain */
			sli++;
			qtype = initial;
			continue;
		}
		else if (tTd(8, 7))
			sm_dprintf("\tYES\n");

		/* avoid problems after truncation in tcp packets */
		if (ret > sizeof(answer))
			ret = sizeof(answer);
		SM_ASSERT(ret >= 0);

		/*
		**  Appear to have a match.  Confirm it by searching for A or
		**  CNAME records.  If we don't have a local domain
		**  wild card MX record, we will accept MX as well.
		*/

		ap = (unsigned char *) &answer + HFIXEDSZ;
		eom = (unsigned char *) &answer + ret;

		if (0 == hp->ad)
			ad = false;

		/* skip question part of response -- we know what we asked */
		for (qdcount = ntohs((unsigned short) hp->qdcount);
		     qdcount--;
		     ap += ret + QFIXEDSZ)
		{
			if ((ret = dn_skipname(ap, eom)) < 0)
			{
				if (tTd(8, 20))
					sm_dprintf("qdcount failure (%d)\n",
						ntohs((unsigned short) hp->qdcount));
				*statp = EX_SOFTWARE;
				goto error;
			}
		}

		amatch = false;
		for (ancount = ntohs((unsigned short) hp->ancount);
		     --ancount >= 0 && ap < eom;
		     ap += n)
		{
			n = dn_expand((unsigned char *) &answer, eom, ap,
				      (RES_UNC_T) nbuf, sizeof(nbuf));
			if (n < 0)
				break;
			ap += n;
			GETSHORT(type, ap);
			ap += INT16SZ;		/* skip over class */
			GETLONG(ttl, ap);
			GETSHORT(n, ap);	/* rdlength */
			switch (type)
			{
			  case T_MX:
				gotmx = true;
				if (*dp != '\0' && HasWildcardMX)
				{
					/*
					**  If we are using MX matches and have
					**  not yet gotten one, save this one
					**  but keep searching for an A or
					**  CNAME match.
					*/

					if (trymx && mxmatch == NULL)
						mxmatch = dp;
					continue;
				}

				/*
				**  If we did not append a domain name, this
				**  must have been a canonical name to start
				**  with.  Even if we did append a domain name,
				**  in the absence of a wildcard MX this must
				**  still be a real MX match.
				**  Such MX matches are as good as an A match,
				**  fall through.
				*/
				/* FALLTHROUGH */

# if NETINET6
			  case T_AAAA:
# endif
			  case T_A:
				/* Flag that a good match was found */
				amatch = true;

				/* continue in case a CNAME also exists */
				continue;

			  case T_CNAME:
				if (DontExpandCnames)
				{
					/* got CNAME -- guaranteed canonical */
					amatch = true;
					break;
				}

				if (loopcnt++ > MAXCNAMEDEPTH)
				{
					/*XXX should notify postmaster XXX*/
					message("DNS failure: CNAME loop for %s",
						host);
					if (CurEnv->e_message == NULL)
					{
						char ebuf[MAXLINE];

						(void) sm_snprintf(ebuf,
							sizeof(ebuf),
							"Deferred: DNS failure: CNAME loop for %.100s",
							host);
						CurEnv->e_message =
						    sm_rpool_strdup_x(
							CurEnv->e_rpool, ebuf);
					}
					SM_SET_H_ERRNO(NO_RECOVERY);
					*statp = EX_CONFIG;
					goto error;
				}

				/* value points at name */
				if ((ret = dn_expand((unsigned char *)&answer,
						     eom, ap, (RES_UNC_T) nbuf,
						     sizeof(nbuf))) < 0)
					break;
				(void) sm_strlcpy(host, nbuf, hbsize);

				/*
				**  RFC 1034 section 3.6 specifies that CNAME
				**  should point at the canonical name -- but
				**  urges software to try again anyway.
				*/

				goto cnameloop;

			  default:
				/* not a record of interest */
				continue;
			}
		}

		if (amatch)
		{
			/*
			**  Got a good match -- either an A, CNAME, or an
			**  exact MX record.  Save it and get out of here.
			*/

			mxmatch = dp;
			break;
		}

		/*
		**  Nothing definitive yet.
		**	If this was a T_A query and we haven't yet found a MX
		**		match, try T_MX if allowed to do so.
		**	Otherwise, try the next domain.
		*/

# if NETINET6
		if (qtype == T_AAAA)
			qtype = T_A;
		else
# endif
		if (qtype == T_A && !gotmx && (trymx || *dp == '\0'))
			qtype = T_MX;
		else
		{
			qtype = initial;
			sli++;
		}
	}

	/* if nothing was found, we are done */
	if (mxmatch == NULL)
	{
		if (*statp == EX_OK)
			*statp = EX_NOHOST;
		goto error;
	}

	/*
	**  Create canonical name and return.
	**  If saved domain name is null, name was already canonical.
	**  Otherwise append the saved domain name.
	*/

	(void) sm_snprintf(nbuf, sizeof(nbuf), "%.*s%s%.*s", MAXDNAME, host,
			   *mxmatch == '\0' ? "" : ".",
			   MAXDNAME, mxmatch);
	(void) sm_strlcpy(host, nbuf, hbsize);
	if (tTd(8, 5))
		sm_dprintf("dns_getcanonname: %s\n", host);
	*statp = EX_OK;

	/* return only one TTL entry, that should be sufficient */
	if (ttl > 0 && pttl != NULL)
		*pttl = ttl;
# if DANE
	_res.options = old_options;
# endif
	return ad ? HOST_SECURE : HOST_OK;

  error:
# if DANE
	_res.options = old_options;
# endif
	return HOST_NOTFOUND;
}

#endif /* NAMED_BIND */
