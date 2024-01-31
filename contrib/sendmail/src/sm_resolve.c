/*
 * Copyright (c) 2000-2004, 2010, 2015, 2020 Proofpoint, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sendmail.h>
#include <sm/sendmail.h>

#if NAMED_BIND
# if NETINET
#  include <netinet/in_systm.h>
#  include <netinet/ip.h>
# endif
# if DNSSEC_TEST || _FFR_NAMESERVER
#  define _DEFINE_SMR_GLOBALS 1
# endif
# include "sm_resolve.h"
# if DNSMAP || DANE

#include <arpa/inet.h>

SM_RCSID("$Id: sm_resolve.c,v 8.40 2013-11-22 20:51:56 ca Exp $")

static struct stot
{
	const char	*st_name;
	int		st_type;
} stot[] =
{
#  if NETINET
	{	"A",		T_A		},
#  endif
#  if NETINET6
	{	"AAAA",		T_AAAA		},
#  endif
	{	"NS",		T_NS		},
	{	"CNAME",	T_CNAME		},
	{	"PTR",		T_PTR		},
	{	"MX",		T_MX		},
	{	"TXT",		T_TXT		},
	{	"AFSDB",	T_AFSDB		},
	{	"SRV",		T_SRV		},
#  ifdef T_DS
	{	"DS",		T_DS		},
#  endif
	{	"RRSIG",	T_RRSIG		},
#  ifdef T_NSEC
	{	"NSEC",		T_NSEC		},
#  endif
#  ifdef T_DNSKEY
	{	"DNSKEY",	T_DNSKEY	},
#  endif
	{	"TLSA",		T_TLSA		},
	{	NULL,		0		}
};

static DNS_REPLY_T *parse_dns_reply __P((unsigned char *, int, unsigned int));
#  if DNSSEC_TEST && defined(T_TLSA)
static char *hex2bin __P((const char *, int));
#  endif

/*
**  DNS_STRING_TO_TYPE -- convert resource record name into type
**
**	Parameters:
**		name -- name of resource record type
**
**	Returns:
**		type if succeeded.
**		-1 otherwise.
*/

int
dns_string_to_type(name)
	const char *name;
{
	struct stot *p = stot;

	for (p = stot; p->st_name != NULL; p++)
		if (SM_STRCASEEQ(name, p->st_name))
			return p->st_type;
	return -1;
}

/*
**  DNS_TYPE_TO_STRING -- convert resource record type into name
**
**	Parameters:
**		type -- resource record type
**
**	Returns:
**		name if succeeded.
**		NULL otherwise.
*/

const char *
dns_type_to_string(type)
	int type;
{
	struct stot *p = stot;

	for (p = stot; p->st_name != NULL; p++)
		if (type == p->st_type)
			return p->st_name;
	return NULL;
}

/*
**  DNS_FREE_DATA -- free all components of a DNS_REPLY_T
**
**	Parameters:
**		dr -- pointer to DNS_REPLY_T
**
**	Returns:
**		none.
*/

void
dns_free_data(dr)
	DNS_REPLY_T *dr;
{
	RESOURCE_RECORD_T *rr;

	if (dr == NULL)
		return;
	if (dr->dns_r_q.dns_q_domain != NULL)
		sm_free(dr->dns_r_q.dns_q_domain);
	for (rr = dr->dns_r_head; rr != NULL; )
	{
		RESOURCE_RECORD_T *tmp = rr;

		if (rr->rr_domain != NULL)
			sm_free(rr->rr_domain);
		if (rr->rr_u.rr_data != NULL)
			sm_free(rr->rr_u.rr_data);
		rr = rr->rr_next;
		sm_free(tmp);
	}
	sm_free(dr);
}

/*
**  BIN2HEX -- convert binary TLSA RR to hex string
**
**	Parameters:
**		tlsa -- pointer to result (allocated here)
**		p --  binary data (TLSA RR)
**		size -- length of p
**		min_size -- minimum expected size
**
**	Returns:
**		>0: length of string (*tlsa)
**		-1: error
*/

static int bin2hex __P((char **, unsigned char *, int, int));

static int
bin2hex(tlsa, p, size, min_size)
	char **tlsa;
	unsigned char *p;
	int size;
	int min_size;
{
	int i, pos, txtlen;

	txtlen = size * 3;
	if (txtlen <= size || size < min_size)
	{
		if (LogLevel > 5)
			sm_syslog(LOG_WARNING, NOQID,
				  "ERROR: bin2hex: size %d wrong", size);
		return -1;
	}
	*tlsa = (char *) sm_malloc(txtlen);
	if (*tlsa == NULL)
	{
		if (tTd(8, 17))
			sm_dprintf("len=%d, rr_data=NULL\n", txtlen);
		return -1;
	}
	snprintf(*tlsa, txtlen,
		"%02X %02X %02X", p[0], p[1], p[2]);
	pos = strlen(*tlsa);

	/* why isn't there a print function like strlcat? */
	for (i = 3; i < size && pos < txtlen; i++, pos += 3)
		snprintf(*tlsa + pos, txtlen - pos, "%c%02X",
			(i == 3) ? ' ' : ':', p[i]);

	return i;
}

/*
**  PARSE_DNS_REPLY -- parse DNS reply data.
**
**	Parameters:
**		data -- pointer to dns data
**		len -- len of data
**		flags -- flags (RR_*)
**
**	Returns:
**		pointer to DNS_REPLY_T if succeeded.
**		NULL otherwise.
**
**	Note:
**		use dns_free_data() to free() the result when no longer needed.
*/

static DNS_REPLY_T *
parse_dns_reply(data, len, flags)
	unsigned char *data;
	int len;
	unsigned int flags;
{
	unsigned char *p;
	unsigned short ans_cnt, ui;
	int status;
	size_t l;
	char host[MAXHOSTNAMELEN];
	DNS_REPLY_T *dr;
	RESOURCE_RECORD_T **rr;

	if (tTd(8, 90))
	{
		FILE *fp;

		fp = fopen("dns.buffer", "w");
		if (fp != NULL)
		{
			fwrite(data, 1, len, fp);
			fclose(fp);
			fp = NULL;
		}
		else
			sm_dprintf("parse_dns_reply: fp=%p, e=%d\n",
				(void *)fp, errno);
	}

	dr = (DNS_REPLY_T *) sm_malloc(sizeof(*dr));
	if (dr == NULL)
		return NULL;
	memset(dr, 0, sizeof(*dr));

	p = data;

	/* doesn't work on Crays? */
	memcpy(&dr->dns_r_h, p, sizeof(dr->dns_r_h));
	p += sizeof(dr->dns_r_h);
	status = dn_expand(data, data + len, p, host, sizeof(host));
	if (status < 0)
		goto error;
	dr->dns_r_q.dns_q_domain = sm_strdup(host);
	if (dr->dns_r_q.dns_q_domain == NULL)
		goto error;

	ans_cnt = ntohs((unsigned short) dr->dns_r_h.ancount);
	if (tTd(8, 17))
		sm_dprintf("parse_dns_reply: ac=%d, ad=%d\n", ans_cnt,
			dr->dns_r_h.ad);

	p += status;
	GETSHORT(dr->dns_r_q.dns_q_type, p);
	GETSHORT(dr->dns_r_q.dns_q_class, p);
	rr = &dr->dns_r_head;
	ui = 0;
	while (p < data + len && ui < ans_cnt)
	{
		int type, class, ttl, size, txtlen;

		status = dn_expand(data, data + len, p, host, sizeof(host));
		if (status < 0)
			goto error;
		++ui;
		p += status;
		GETSHORT(type, p);
		GETSHORT(class, p);
		GETLONG(ttl, p);
		GETSHORT(size, p);
		if (p + size > data + len)
		{
			/*
			**  announced size of data exceeds length of
			**  data paket: someone is cheating.
			*/

			if (LogLevel > 5)
				sm_syslog(LOG_WARNING, NOQID,
					  "ERROR: DNS RDLENGTH=%d > data len=%d",
					  size, len - (int)(p - data));
			goto error;
		}
		*rr = (RESOURCE_RECORD_T *) sm_malloc(sizeof(**rr));
		if (*rr == NULL)
			goto error;
		memset(*rr, 0, sizeof(**rr));
		(*rr)->rr_domain = sm_strdup(host);
		if ((*rr)->rr_domain == NULL)
			goto error;
		(*rr)->rr_type = type;
		(*rr)->rr_class = class;
		(*rr)->rr_ttl = ttl;
		(*rr)->rr_size = size;
		switch (type)
		{
		  case T_NS:
		  case T_CNAME:
		  case T_PTR:
			status = dn_expand(data, data + len, p, host,
					   sizeof(host));
			if (status < 0)
				goto error;
			if (tTd(8, 50))
				sm_dprintf("parse_dns_reply: type=%s, host=%s\n",
					dns_type_to_string(type), host);
			(*rr)->rr_u.rr_txt = sm_strdup(host);
			if ((*rr)->rr_u.rr_txt == NULL)
				goto error;
			break;

		  case T_MX:
		  case T_AFSDB:
			status = dn_expand(data, data + len, p + 2, host,
					   sizeof(host));
			if (status < 0)
				goto error;
			l = strlen(host) + 1;
			(*rr)->rr_u.rr_mx = (MX_RECORD_T *)
				sm_malloc(sizeof(*((*rr)->rr_u.rr_mx)) + l);
			if ((*rr)->rr_u.rr_mx == NULL)
				goto error;
			(*rr)->rr_u.rr_mx->mx_r_preference = (p[0] << 8) | p[1];
			(void) sm_strlcpy((*rr)->rr_u.rr_mx->mx_r_domain,
					  host, l);
			if (tTd(8, 50))
				sm_dprintf("mx=%s, pref=%d\n", host,
					(*rr)->rr_u.rr_mx->mx_r_preference);
			break;

		  case T_SRV:
			status = dn_expand(data, data + len, p + 6, host,
					   sizeof(host));
			if (status < 0)
				goto error;
			l = strlen(host) + 1;
			(*rr)->rr_u.rr_srv = (SRV_RECORDT_T*)
				sm_malloc(sizeof(*((*rr)->rr_u.rr_srv)) + l);
			if ((*rr)->rr_u.rr_srv == NULL)
				goto error;
			(*rr)->rr_u.rr_srv->srv_r_priority = (p[0] << 8) | p[1];
			(*rr)->rr_u.rr_srv->srv_r_weight = (p[2] << 8) | p[3];
			(*rr)->rr_u.rr_srv->srv_r_port = (p[4] << 8) | p[5];
			(void) sm_strlcpy((*rr)->rr_u.rr_srv->srv_r_target,
					  host, l);
			break;

		  case T_TXT:

			/*
			**  The TXT record contains the length as
			**  leading byte, hence the value is restricted
			**  to 255, which is less than the maximum value
			**  of RDLENGTH (size). Nevertheless, txtlen
			**  must be less than size because the latter
			**  specifies the length of the entire TXT
			**  record.
			*/

			txtlen = *p;
			if (txtlen >= size)
			{
				if (LogLevel > 5)
					sm_syslog(LOG_WARNING, NOQID,
						  "ERROR: DNS TXT record size=%d <= text len=%d",
						  size, txtlen);
				goto error;
			}
			(*rr)->rr_u.rr_txt = (char *) sm_malloc(txtlen + 1);
			if ((*rr)->rr_u.rr_txt == NULL)
				goto error;
			(void) sm_strlcpy((*rr)->rr_u.rr_txt, (char*) p + 1,
					  txtlen + 1);
			break;

#  ifdef T_TLSA
		  case T_TLSA:
			if (tTd(8, 61))
				sm_dprintf("parse_dns_reply: TLSA, size=%d, flags=%X\n",
					size, flags);
			if ((flags & RR_AS_TEXT) != 0)
			{
				txtlen = bin2hex((char **)&((*rr)->rr_u.rr_data),
						p, size, 4);
				if (txtlen <= 0)
					goto error;
				break;
			}
			/* FALLTHROUGH */
			/* return "raw" data for caller to use as it pleases */
#  endif /* T_TLSA */

		  default:
			(*rr)->rr_u.rr_data = (unsigned char*) sm_malloc(size);
			if ((*rr)->rr_u.rr_data == NULL)
				goto error;
			(void) memcpy((*rr)->rr_u.rr_data, p, size);
			if (tTd(8, 61) && type == T_A)
			{
				SOCKADDR addr;

				(void) memcpy((void *)&addr.sin.sin_addr.s_addr, p, size);
				sm_dprintf("parse_dns_reply: IPv4=%s\n",
					inet_ntoa(addr.sin.sin_addr));
			}
			break;
		}
		p += size;
		rr = &(*rr)->rr_next;
	}
	*rr = NULL;
	return dr;

  error:
	dns_free_data(dr);
	return NULL;
}

#  if DNSSEC_TEST

#   include <arpa/nameser.h>
#   if _FFR_8BITENVADDR
#    include <sm/sendmail.h>
#   endif

static int gen_dns_reply __P((unsigned char *, int, unsigned char *,
		const char *, int, const char *, int, int, int, int,
		const char *, int, int, int));
static int dnscrtrr __P((const char *, const char *, int, char *, int,
	unsigned int, int *, int *, unsigned char *, int, unsigned char *));

/*
**  HERRNO2TXT -- return error text for h_errno
**
**	Parameters:
**		e -- h_errno
**
**	Returns:
**		DNS error text if available
*/

const char *
herrno2txt(e)
	int e;
{
	switch (e)
	{
	  case NETDB_INTERNAL:
		return "see errno";
	  case NETDB_SUCCESS:
		return "OK";
	  case HOST_NOT_FOUND:
		return "HOST_NOT_FOUND";
	  case TRY_AGAIN:
		return "TRY_AGAIN";
	  case NO_RECOVERY:
		return "NO_RECOVERY";
	  case NO_DATA:
		return "NO_DATA";
	}
	return "bogus h_errno";
}

/*
**  GEN_DNS_REPLY -- generate DNS reply data.
**
**	Parameters:
**		buf -- buffer to which DNS data is written
**		buflen -- length of buffer
**		bufpos -- position in buffer where DNS RRs are appended
**		query -- name of query
**		qtype -- resource record type of query
**		domain -- name of domain which has been "found"
**		class -- resource record class
**		type -- resource record type
**		ttl -- TTL
**		size -- size of data
**		data -- data
**		txtlen -- length of text
**		pref -- MX preference
**		ad -- ad flag
**
**	Returns:
**		>0 length of buffer that has been used.
**		<0 error
*/

static int
gen_dns_reply(buf, buflen, bufpos, query, qtype, domain, class, type, ttl, size, data, txtlen, pref, ad)
	unsigned char *buf;
	int buflen;
	unsigned char *bufpos;
	const char *query;
	int qtype;
	const char *domain;
	int class;
	int type;
	int ttl;
	int size;
	const char *data;
	int txtlen;
	int pref;
	int ad;
{
	unsigned short ans_cnt;
	HEADER *hp;
	unsigned char *cp, *ep;
	int n;
	static unsigned char *dnptrs[20], **dpp, **lastdnptr;

#define DN_COMP_CHK	do	\
	{	\
		if (n < 0)	\
		{	\
			if (tTd(8, 91))	\
				sm_dprintf("gen_dns_reply: dn_comp=%d\n", n); \
			return n;	\
		}	\
	} while (0)

	SM_REQUIRE(NULL != buf);
	SM_REQUIRE(buflen >= HFIXEDSZ);
	SM_REQUIRE(query != NULL);
	hp = (HEADER *) buf;
	ep = buf + buflen;
	cp = buf + HFIXEDSZ;

	if (bufpos != NULL)
		cp = bufpos;
	else
	{
		sm_dprintf("gen_dns_reply: query=%s, domain=%s, type=%s, size=%d, ad=%d\n",
			query, domain, dns_type_to_string(type), size, ad);
		dpp = dnptrs;
		*dpp++ = buf;
		*dpp++ = NULL;
		lastdnptr = dnptrs + sizeof dnptrs / sizeof dnptrs[0];

		memset(buf, 0, HFIXEDSZ);
		hp->id = 0xdead;	/* HACK */
		hp->qr = 1;
		hp->opcode = QUERY;
		hp->rd = 0;	/* recursion desired? */
		hp->rcode = 0; /* !!! */
		/* hp->aa = ?;	* !!! */
		/* hp->tc = ?;	* !!! */
		/* hp->ra = ?;	* !!! */
		hp->qdcount = htons(1);
		hp->ancount = 0;

		n = dn_comp(query, cp, ep - cp - QFIXEDSZ, dnptrs, lastdnptr);
		DN_COMP_CHK;
		cp += n;
		PUTSHORT(qtype, cp);
		PUTSHORT(class, cp);
	}
	hp->ad = ad;

	if (ep - cp < QFIXEDSZ)
	{
		if (tTd(8, 91))
			sm_dprintf("gen_dns_reply: ep-cp=%ld\n",
				(long) (ep - cp));
		return (-1);
	}
	n = dn_comp(domain, cp, ep - cp - QFIXEDSZ, dnptrs, lastdnptr);
	DN_COMP_CHK;
	cp += n;
	PUTSHORT(type, cp);
	PUTSHORT(class, cp);
	PUTLONG(ttl, cp);

	ans_cnt = ntohs((unsigned short) hp->ancount);
	++ans_cnt;
	hp->ancount = htons((unsigned short) ans_cnt);

	switch (type)
	{
	  case T_MX:
		n = dn_comp(data, cp + 4, ep - cp - QFIXEDSZ, dnptrs, lastdnptr);
		DN_COMP_CHK;
		PUTSHORT(n + 2, cp);
		PUTSHORT(pref, cp);
		cp += n;
		break;

	  case T_TXT:
		if (txtlen >= size)
			return -1;
		PUTSHORT(txtlen, cp);
		(void) sm_strlcpy((char *)cp, data, txtlen + 1);
		cp += txtlen;
		break;

	  case T_CNAME:
		n = dn_comp(data, cp + 2, ep - cp - QFIXEDSZ, dnptrs, lastdnptr);
		DN_COMP_CHK;
		PUTSHORT(n, cp);
		cp += n;
		break;

#   if defined(T_TLSA)
	  case T_TLSA:
		{
		char *tlsa;

		tlsa = hex2bin(data, size);
		if (tlsa == NULL)
			return (-1);
		n = size / 2;
		PUTSHORT(n, cp);
		(void) memcpy(cp, tlsa, n);
		cp += n;
		}
		break;
#   endif /* T_TLSA */

	  default:
		PUTSHORT(size, cp);
		(void) memcpy(cp, data, size);
		cp += size;
		break;
	}

	return (cp - buf);
}

/*
**  SETHERRNOFROMSTRING -- set h_errno based on text
**
**	Parameters:
**		str -- string which might contain h_errno text
**		prc -- pointer to rcode (EX_*)
**
**	Returns:
**		h_errno if found
**		0 otherwise
*/

int
setherrnofromstring(str, prc)
	const char *str;
	int *prc;
{
	SM_SET_H_ERRNO(0);
	if (SM_IS_EMPTY(str))
		return 0;
	if (strstr(str, "herrno:") == NULL)
		return 0;
	if (prc != NULL)
		*prc = EX_NOHOST;
	if (strstr(str, "host_not_found"))
		SM_SET_H_ERRNO(HOST_NOT_FOUND);
	else if (strstr(str, "try_again"))
	{
		SM_SET_H_ERRNO(TRY_AGAIN);
		if (prc != NULL)
			*prc = EX_TEMPFAIL;
	}
	else if (strstr(str, "no_recovery"))
		SM_SET_H_ERRNO(NO_RECOVERY);
	else if (strstr(str, "no_data"))
		SM_SET_H_ERRNO(NO_DATA);
	else
		SM_SET_H_ERRNO(NETDB_INTERNAL);
	return h_errno;
}

/*
**  GETTTLFROMSTRING -- extract ttl from a string
**
**	Parameters:
**		str -- string which might contain ttl
**
**	Returns:
**		ttl if found
**		0 otherwise
*/

int
getttlfromstring(str)
	const char *str;
{
	if (SM_IS_EMPTY(str))
		return 0;
#define TTL_PRE "ttl="
	if (strstr(str, TTL_PRE) == NULL)
		return 0;
	return strtoul(str + strlen(TTL_PRE), NULL, 10);
}


#   if defined(T_TLSA)
/*
**  HEX2BIN -- convert hex string to binary TLSA RR
**
**	Parameters:
**		p --  hex representation of TLSA RR
**		size -- length of p
**
**	Returns:
**		pointer to binary TLSA RR
**		NULL: error
*/

static char *
hex2bin(p, size)
	const char *p;
	int size;
{
	int i, pos, txtlen;
	char *tlsa;

	txtlen = size / 2;
	if (txtlen * 2 == size)
	{
		if (LogLevel > 5)
			sm_syslog(LOG_WARNING, NOQID,
				  "ERROR: hex2bin: size %d wrong", size);
		return NULL;
	}
	tlsa = sm_malloc(txtlen + 1);
	if (tlsa == NULL)
	{
		if (tTd(8, 17))
			sm_dprintf("len=%d, tlsa=NULL\n", txtlen);
		return NULL;
	}

#define CHAR2INT(c)	(((c) <= '9') ? ((c) - '0') : (toupper(c) - 'A' + 10))
	for (i = 0, pos = 0; i + 1 < size && pos < txtlen; i += 2, pos++)
		tlsa[pos] = CHAR2INT(p[i]) * 16 + CHAR2INT(p[i+1]);

	return tlsa;
}
#   endif /* T_TLSA */

const char *
rr_type2tag(rr_type)
	int rr_type;
{
	switch (rr_type)
	{
	  case T_A:
		return "ipv4";
#   if NETINET6
	  case T_AAAA:
		return "ipv6";
#   endif
	  case T_CNAME:
		return "cname";
	  case T_MX:
		return "mx";
#   ifdef T_TLSA
	  case T_TLSA:
		return "tlsa";
#   endif
	}
	return NULL;
}

/*
**  DNSCRTRR -- create DNS RR
**
**	Parameters:
**		domain -- original query domain
**		query -- name of query
**		qtype -- resource record type of query
**		value -- (list of) data to set
**		rr_type -- resource record type
**		flags -- flags how to handle various lookups
**		herr -- (pointer to) h_errno (output if non-NULL)
**		adp -- (pointer to) ad flag
**		answer -- buffer for RRs
**		anslen -- size of answer
**		anspos -- current position in answer
**
**	Returns:
**		>0: length of data in answer
**		<0: error, check *herr
*/

static int
dnscrtrr(domain, query, qtype, value, rr_type, flags, herr, adp, answer, anslen, anspos)
	const char *domain;
	const char *query;
	int qtype;
	char *value;
	int rr_type;
	unsigned int flags;
	int *herr;
	int *adp;
	unsigned char *answer;
	int anslen;
	unsigned char *anspos;
{
	SOCKADDR addr;
	int ttl, ad, rlen;
	char *p, *token;
	char data[IN6ADDRSZ];
	char rhs[MAXLINE];

	rlen = -1;
	if (SM_IS_EMPTY(value))
		return rlen;
	SM_REQUIRE(adp != NULL);
	(void) sm_strlcpy(rhs, value, sizeof(rhs));
	p = rhs;
	if (setherrnofromstring(p, NULL) != 0)
	{
		if (herr != NULL)
			*herr = h_errno;
		if (tTd(8, 16))
			sm_dprintf("dnscrtrr rhs=%s h_errno=%d (%s)\n",
				p, h_errno, herrno2txt(h_errno));
		return rlen;
	}

	ttl = 0;
	ad = 0;
	for (token = p; token != NULL && *token != '\0'; token = p)
	{
		rlen = 0;
		while (p != NULL && *p != '\0' && !SM_ISSPACE(*p))
			++p;
		if (SM_ISSPACE(*p))
			*p++ = '\0';
		sm_dprintf("dnscrtrr: token=%s\n", token);
		if (strcmp(token, "ad") == 0)
		{
			bool adflag;

			adflag = (_res.options & RES_USE_DNSSEC) != 0;

			/* maybe print this only for the final RR? */
			if (tTd(8, 61))
				sm_dprintf("dnscrtrr: ad=1, adp=%d, adflag=%d\n",
					*adp, adflag);
			if (*adp != 0 && adflag)
			{
				*adp = 1;
				ad = 1;
			}
			continue;
		}
		if (ttl == 0 && (ttl = getttlfromstring(token)) > 0)
		{
			if (tTd(8, 61))
				sm_dprintf("dnscrtrr: ttl=%d\n", ttl);
			continue;
		}

		if (rr_type == T_A)
		{
			addr.sin.sin_addr.s_addr = inet_addr(token);
			(void) memmove(data, (void *)&addr.sin.sin_addr.s_addr,
				INADDRSZ);
			rlen = gen_dns_reply(answer, anslen, anspos,
				query, qtype, domain, C_IN, rr_type, ttl,
				INADDRSZ, data, 0, 0, ad);
		}

#   if NETINET6
		if (rr_type == T_AAAA)
		{
			anynet_pton(AF_INET6, token, &addr.sin6.sin6_addr);
			memmove(data, (void *)&addr.sin6.sin6_addr, IN6ADDRSZ);
			rlen = gen_dns_reply(answer, anslen, anspos,
				query, qtype, domain, C_IN, rr_type, ttl,
				IN6ADDRSZ, data, 0, 0, ad);
		}
#   endif /* NETINET6 */

		if (rr_type == T_MX)
		{
			char *endptr;
			int pref;

			pref = (int) strtoul(token, &endptr, 10);
			if (endptr == NULL || *endptr != ':')
				goto error;
			token = endptr + 1;
			rlen = gen_dns_reply(answer, anslen, anspos,
				query, qtype, domain, C_IN, rr_type, ttl,
				strlen(token) + 1, token, 0, pref, ad);
			if (tTd(8, 50))
				sm_dprintf("dnscrtrr: mx=%s, pref=%d, rlen=%d\n",
					token, pref, rlen);
		}

#   ifdef T_TLSA
		if (rr_type == T_TLSA)
			rlen = gen_dns_reply(answer, anslen, anspos,
				query, qtype, domain, C_IN, rr_type, ttl,
				strlen(token) + 1, token, 0, 0, ad);
#   endif

		if (rr_type == T_CNAME)
			rlen = gen_dns_reply(answer, anslen, anspos,
				query, qtype, domain, C_IN, rr_type, ttl,
				strlen(token), token, 0, 0, ad);
		if (rlen < 0)
			goto error;
		if (rlen > 0)
			anspos = answer + rlen;
	}

	if (ad != 1)
		*adp = 0;

	return rlen;

  error:
	if (herr != NULL && 0 == *herr)
		*herr = NO_RECOVERY;
	return -1;
}

/*
**  TSTDNS_SEARCH -- replacement for res_search() for testing
**
**	Parameters:
**		domain -- query domain
**		class -- class
**		type -- resource record type
**		answer -- buffer for RRs
**		anslen -- size of answer
**
**	Returns:
**		>0: length of data in answer
**		<0: error, check h_errno
*/

int
tstdns_search(domain, class, type, answer, anslen)
	const char *domain;
	int class;
	int type;
	unsigned char *answer;
	int anslen;
{
	int rlen, ad, maprcode, cnt, flags, herr;
	bool found_cname;
	const char *query;
	char *p;
	const char *tag;
	char *av[2];
	STAB *map;
#   if _FFR_8BITENVADDR
	char qbuf[MAXNAME_I];
	char *qdomain;
#   else
#    define qdomain domain
#   endif
	char key[MAXNAME_I + 16];
	char rhs[MAXLINE];
	unsigned char *anspos;

	rlen = -1;
	herr = 0;
	if (class != C_IN)
		goto error;
	if (SM_IS_EMPTY(domain))
		goto error;
	tag = rr_type2tag(type);
	if (tag == NULL)
		goto error;
	maprcode = EX_OK;
	ad = -1;
	flags = 0;
#   if _FFR_8BITENVADDR
	if (tTd(8, 62))
		sm_dprintf("domain=%s\n", domain);
	(void) dequote_internal_chars((char *)domain, qbuf, sizeof(qbuf));
	query = qbuf;
	qdomain = qbuf;
	if (tTd(8, 63))
		sm_dprintf("qdomain=%s\n", qdomain);
#   else
	query = domain;
#   endif /* _FFR_8BITENVADDR */
	anspos = NULL;

	map = stab("access", ST_MAP, ST_FIND);
	if (NULL == map)
	{
		sm_dprintf("access map not found\n");
		goto error;
	}
	if (!bitset(MF_OPEN, map->s_map.map_mflags) &&
	    !openmap(&(map->s_map)))
	{
		sm_dprintf("access map open failed\n");
		goto error;
	}

/*
**  Look up tag:domain, if not found and domain does not end with a dot
**  (and the proper debug level is selected), also try with trailing dot.
*/

#define SM_LOOKUP2(tag)	\
	do {	\
		int len;	\
				\
		len = strlen(qdomain);	\
		av[0] = key;	\
		av[1] = NULL;	\
		snprintf(key, sizeof(key), "%s:%s", tag, qdomain); \
		p = (*map->s_map.map_class->map_lookup)(&map->s_map, key, av, \
			&maprcode);	\
		if (p != NULL)	\
			break;	\
		if (!tTd(8, 112) || (len > 0 && '.' == qdomain[len - 1])) \
			break;	\
		snprintf(key, sizeof(key), "%s:%s.", tag, qdomain); \
		p = (*map->s_map.map_class->map_lookup)(&map->s_map, key, av, \
			&maprcode);	\
	} while (0)

	cnt = 0;
	found_cname = false;
	while (cnt < 6)
	{
		char *last;

		/* Should this try with/without trailing dot? */
		SM_LOOKUP2(tag);
		if (p != NULL)
		{
			sm_dprintf("access map lookup key=%s, value=%s\n", key,
				p);
			break;
		}
		if (NULL == p && (flags & RR_NO_CNAME) == 0)
		{
			sm_dprintf("access map lookup failed key=%s, try cname\n",
				key);
			SM_LOOKUP2("cname");
			if (p != NULL)
			{
				sm_dprintf("cname lookup key=%s, value=%s, ad=%d\n",
					key, p, ad);
				rlen = dnscrtrr(qdomain, query, type, p, T_CNAME,
						flags, &herr, &ad, answer,
						anslen, anspos);
				if (rlen < 0)
					goto error;
				if (rlen > 0)
					anspos = answer + rlen;
				found_cname = true;
			}
		}
		if (NULL == p)
			break;

		(void) sm_strlcpy(rhs, p, sizeof(rhs));
		p = rhs;

		/* skip (leading) ad/ttl: look for last ' ' */
		if ((last = strrchr(p, ' ')) != NULL && last[1] != '\0')
			qdomain = last + 1;
		else
			qdomain = p;
		++cnt;
	}
	if (NULL == p)
	{
		int t;
		char *tags[] = { "ipv4", "mx", "tlsa",
#   if NETINET6
			"ipv6",
#   endif
			NULL
		};

		for (t = 0; tags[t] != NULL; t++)
		{
			if (strcmp(tag, tags[t]) == 0)
				continue;
			SM_LOOKUP2(tags[t]);
			if (p != NULL)
			{
				sm_dprintf("access map lookup failed key=%s:%s, but found key=%s\n",
					tag, qdomain, key);
				herr = NO_DATA;
				goto error;
			}
		}
		sm_dprintf("access map lookup failed key=%s\n", key);
		herr = HOST_NOT_FOUND;
		goto error;
	}
	if (found_cname && (flags & RR_ONLY_CNAME) != 0)
		return rlen;
	rlen = dnscrtrr(qdomain, query, type, p, type, flags, &herr, &ad,
			answer, anslen, anspos);
	if (rlen < 0)
		goto error;
	return rlen;

  error:
	if (0 == herr)
		herr = NO_RECOVERY;
	SM_SET_H_ERRNO(herr);
	sm_dprintf("rlen=%d, herr=%d\n", rlen, herr);
	return -1;
}

/*
**  TSTDNS_QUERYDOMAIN -- replacement for res_querydomain() for testing
**
**	Parameters:
**		name -- query name
**		domain -- query domain
**		class -- class
**		type -- resource record type
**		answer -- buffer for RRs
**		anslen -- size of answer
**
**	Returns:
**		>0: length of data in answer
**		<0: error, check h_errno
*/

int
tstdns_querydomain(name, domain, class, type, answer, anslen)
	const char *name;
	const char *domain;
	int class;
	int type;
	unsigned char *answer;
	int anslen;
{
	char query[MAXNAME_I];
	int len;

	if (NULL == name)
		goto error;
	if (SM_IS_EMPTY(domain))
		return tstdns_search(name, class, type, answer, anslen);

	len = snprintf(query, sizeof(query), "%s.%s", name, domain);
	if (len >= (int)sizeof(query))
		goto error;
	return tstdns_search(query, class, type, answer, anslen);

  error:
	SM_SET_H_ERRNO(NO_RECOVERY);
	return -1;
}

#  endif /* DNSSEC_TEST */

/*
**  DNS_LOOKUP_INT -- perform DNS lookup
**
**	Parameters:
**		domain -- name to look up
**		rr_class -- resource record class
**		rr_type -- resource record type
**		retrans -- retransmission timeout
**		retry -- number of retries
**		options -- DNS resolver options
**		flags -- currently only passed to parse_dns_reply()
**		err -- (pointer to) errno (output if non-NULL)
**		herr -- (pointer to) h_errno (output if non-NULL)
**
**	Returns:
**		result of lookup if succeeded.
**		NULL otherwise.
*/

DNS_REPLY_T *
dns_lookup_int(domain, rr_class, rr_type, retrans, retry, options, flags, err, herr)
	const char *domain;
	int rr_class;
	int rr_type;
	time_t retrans;
	int retry;
	unsigned int options;
	unsigned int flags;
	int *err;
	int *herr;
{
	int len;
	unsigned long old_options = 0;
	time_t save_retrans = 0;
	int save_retry = 0;
	DNS_REPLY_T *dr = NULL;
	querybuf reply_buf;
	unsigned char *reply;
	int (*resfunc) __P((const char *, int, int, u_char *, int));

#  define SMRBSIZE ((int) sizeof(reply_buf))
#  ifndef IP_MAXPACKET
#   define IP_MAXPACKET	65535
#  endif

	resfunc = res_search;
#  if DNSSEC_TEST
	if (tTd(8, 110))
		resfunc = tstdns_search;
#  endif

	old_options = _res.options;
	_res.options |= options;
	if (err != NULL)
		*err = 0;
	if (herr != NULL)
		*herr = 0;
	if (tTd(8, 16))
	{
		_res.options |= RES_DEBUG;
		sm_dprintf("dns_lookup_int(%s, %d, %s, %x)\n", domain,
			   rr_class, dns_type_to_string(rr_type), options);
	}
#  if DNSSEC_TEST
	if (tTd(8, 15))
		sm_dprintf("NS=%s, port=%d\n",
			inet_ntoa(_res.nsaddr_list[0].sin_addr),
			ntohs(_res.nsaddr_list[0].sin_port));
#  endif
	if (retrans > 0)
	{
		save_retrans = _res.retrans;
		_res.retrans = retrans;
	}
	if (retry > 0)
	{
		save_retry = _res.retry;
		_res.retry = retry;
	}
	errno = 0;
	SM_SET_H_ERRNO(0);
	reply = (unsigned char *)&reply_buf;
	len = (*resfunc)(domain, rr_class, rr_type, reply, SMRBSIZE);
	if (len >= SMRBSIZE)
	{
		if (len >= IP_MAXPACKET)
		{
			if (tTd(8, 4))
				sm_dprintf("dns_lookup: domain=%s, length=%d, default_size=%d, max=%d, status=response too long\n",
					   domain, len, SMRBSIZE, IP_MAXPACKET);
		}
		else
		{
			if (tTd(8, 6))
				sm_dprintf("dns_lookup: domain=%s, length=%d, default_size=%d, max=%d, status=response longer than default size, resizing\n",
					   domain, len, SMRBSIZE, IP_MAXPACKET);
			reply = (unsigned char *)sm_malloc(IP_MAXPACKET);
			if (reply == NULL)
				SM_SET_H_ERRNO(TRY_AGAIN);
			else
			{
				SM_SET_H_ERRNO(0);
				len = (*resfunc)(domain, rr_class, rr_type,
						 reply, IP_MAXPACKET);
			}
		}
	}
	_res.options = old_options;
	if (len < 0)
	{
		if (err != NULL)
			*err = errno;
		if (herr != NULL)
			*herr = h_errno;
		if (tTd(8, 16))
		{
			sm_dprintf("dns_lookup_int(%s, %d, %s, %x)=%d, errno=%d, h_errno=%d"
#  if DNSSEC_TEST
				" (%s)"
#  endif
				"\n",
				domain, rr_class, dns_type_to_string(rr_type),
				options, len, errno, h_errno
#  if DNSSEC_TEST
				, herrno2txt(h_errno)
#  endif
				);
		}
	}
	else if (tTd(8, 16))
	{
		sm_dprintf("dns_lookup_int(%s, %d, %s, %x)=%d\n",
			domain, rr_class, dns_type_to_string(rr_type),
			options, len);
	}
	if (len >= 0 && len < IP_MAXPACKET && reply != NULL)
		dr = parse_dns_reply(reply, len, flags);
	if (reply != (unsigned char *)&reply_buf && reply != NULL)
	{
		sm_free(reply);
		reply = NULL;
	}
	if (retrans > 0)
		_res.retrans = save_retrans;
	if (retry > 0)
		_res.retry = save_retry;
	return dr;
}

/*
**  DNS_LOOKUP_MAP -- perform DNS map lookup
**
**	Parameters:
**		domain -- name to look up
**		rr_class -- resource record class
**		rr_type -- resource record type
**		retrans -- retransmission timeout
**		retry -- number of retries
**		options -- DNS resolver options
**
**	Returns:
**		result of lookup if succeeded.
**		NULL otherwise.
*/

DNS_REPLY_T *
dns_lookup_map(domain, rr_class, rr_type, retrans, retry, options)
	const char *domain;
	int rr_class;
	int rr_type;
	time_t retrans;
	int retry;
	unsigned int options;
{
	return dns_lookup_int(domain, rr_class, rr_type, retrans, retry,
			options, RR_AS_TEXT, NULL, NULL);
}

#  if DANE
/*
**  DNS2HE -- convert DNS_REPLY_T list to hostent struct
**
**	Parameters:
**		dr -- DNS lookup result
**		family -- address family
**
**	Returns:
**		hostent struct if succeeded.
**		NULL otherwise.
**
**	Note:
**		this returns a pointer to a static struct!
*/

struct hostent *
dns2he(dr, family)
	DNS_REPLY_T *dr;
	int family;
{
#   define SM_MAX_ADDRS	256
	static struct hostent he;
	static char *he_aliases[1];
	static char *he_addr_list[SM_MAX_ADDRS];
#   ifdef IN6ADDRSZ
#    define IN_ADDRSZ IN6ADDRSZ
#   else
#    define IN_ADDRSZ INADDRSZ
#   endif
	static char he_addrs[SM_MAX_ADDRS * IN_ADDRSZ];
	static char he_name[MAXNAME_I];
	static bool he_init = false;
	struct hostent *h;
	int i;
	size_t sz;
#   if NETINET6 && DNSSEC_TEST
	struct in6_addr ia6;
	char buf6[INET6_ADDRSTRLEN];
#   endif
	RESOURCE_RECORD_T *rr;

	if (dr == NULL)
		return NULL;

	h = &he;
	if (!he_init)
	{
		he_aliases[0] = NULL;
		he.h_aliases = he_aliases;
		he.h_addr_list = he_addr_list;
		he.h_name = he_name;
		he_init = true;
	}
	h->h_addrtype = family;

	if (tTd(8, 17))
		sm_dprintf("dns2he: ad=%d\n", dr->dns_r_h.ad);

	/* do we want/need to copy the name? */
	rr = dr->dns_r_head;
	if (rr != NULL && rr->rr_domain != NULL)
		sm_strlcpy(h->h_name, rr->rr_domain, sizeof(he_name));
	else
		h->h_name[0] = '\0';

	sz = 0;
#   if NETINET
	if (family == AF_INET)
		sz = INADDRSZ;
#   endif
#   if NETINET6
	if (family == AF_INET6)
		sz = IN6ADDRSZ;
#   endif
	if (sz == 0)
		return NULL;
	h->h_length = sz;

	for (rr = dr->dns_r_head, i = 0; rr != NULL && i < SM_MAX_ADDRS - 1;
	     rr = rr->rr_next)
	{
		h->h_addr_list[i] = he_addrs + i * h->h_length;
		switch (rr->rr_type)
		{
#   if NETINET
		  case T_A:
			if (family != AF_INET)
				continue;
			memmove(h->h_addr_list[i], rr->rr_u.rr_a, INADDRSZ);
			++i;
			break;
#   endif /* NETINET */
#   if NETINET6
		  case T_AAAA:
			if (family != AF_INET6)
				continue;
			memmove(h->h_addr_list[i], rr->rr_u.rr_aaaa, IN6ADDRSZ);
			++i;
			break;
#   endif /* NETINET6 */
		  case T_CNAME:
#   if DNSSEC_TEST
			if (tTd(8, 16))
				sm_dprintf("dns2he: cname: %s ttl=%d\n",
					rr->rr_u.rr_txt, rr->rr_ttl);
#   endif
			break;
		  case T_MX:
#   if DNSSEC_TEST
			if (tTd(8, 16))
				sm_dprintf("dns2he: mx: %d %s ttl=%d\n",
					rr->rr_u.rr_mx->mx_r_preference,
					rr->rr_u.rr_mx->mx_r_domain,
					rr->rr_ttl);
#   endif
			break;

#   if defined(T_TLSA)
		  case T_TLSA:
#    if DNSSEC_TEST
			if (tTd(8, 16))
			{
				char *tlsa;
				int len;

				len = bin2hex(&tlsa, rr->rr_u.rr_data,
						rr->rr_size, 4);
				if (len > 0)
					sm_dprintf("dns2he: tlsa: %s ttl=%d\n",
						tlsa, rr->rr_ttl);
			}
#    endif
			break;
#   endif /* T_TLSA */
		}
	}

	/* complain if list is too long! */
	SM_ASSERT(i < SM_MAX_ADDRS);
	h->h_addr_list[i] = NULL;

#   if DNSSEC_TEST
	if (tTd(8, 16))
	{
		struct in_addr ia;

		for (i = 0; h->h_addr_list[i] != NULL && i < SM_MAX_ADDRS; i++)
		{
			char *addr;

			addr = NULL;
#    if NETINET6
			if (h->h_addrtype == AF_INET6)
			{
				memmove(&ia6, h->h_addr_list[i], IN6ADDRSZ);
				addr = anynet_ntop(&ia6, buf6, sizeof(buf6));
			}
			else
#    endif /* NETINET6 */
			/* "else" in #if code above */
			{
				memmove(&ia, h->h_addr_list[i], INADDRSZ);
				addr = (char *) inet_ntoa(ia);
			}
			if (addr != NULL)
				sm_dprintf("dns2he: addr[%d]: %s\n", i, addr);
		}
	}
#   endif /* DNSSEC_TEST */
	return h;
}
#  endif /* DANE */
# endif /* DNSMAP || DANE */

# if DNSSEC_TEST || _FFR_NAMESERVER
/*
**  DNS_ADDNS -- add one NS in resolver context
**
**	Parameters:
**		ns -- (IPv4 address of) nameserver
**		port -- nameserver port (host order)
**
**	Returns:
**		None.
*/

static void dns_addns __P((struct in_addr *, unsigned int));
static int nsidx = 0;
#ifndef MAXNS
# define MAXNS	3
#endif
static void
dns_addns(ns, port)
	struct in_addr *ns;
	unsigned int port;
{
	if (nsidx >= MAXNS)
		syserr("too many NameServers defined (%d max)", MAXNS);
	_res.nsaddr_list[nsidx].sin_family = AF_INET;
	_res.nsaddr_list[nsidx].sin_addr = *ns;
	if (port != 0)
		_res.nsaddr_list[nsidx].sin_port = htons(port);
	_res.nscount = ++nsidx;
	if (tTd(8, 61))
		sm_dprintf("dns_addns: nsidx=%d, ns=%s:%u\n",
			   nsidx - 1, inet_ntoa(*ns), port);
}

/*
**  NSPORTIP -- parse port@IPv4 and set NS accordingly
**
**	Parameters:
**		p -- port@IPv4
**
**	Returns:
**		<0: error
**		>=0: ok
**
**	Side Effects:
**		sets NS for DNS lookups
*/

/*
**  There should be a generic function for this...
**  milter_open(), socket_map_open(), others?
*/

int
nsportip(p)
	char *p;
{
	char *h;
	int r;
	unsigned short port;
	struct in_addr nsip;

	if (SM_IS_EMPTY(p))
		return -1;

	port = 0;
	while (SM_ISSPACE(*p))
		p++;
	if (*p == '\0')
		return -1;
	h = strchr(p, '@');
	if (h != NULL)
	{
		*h = '\0';
		if (isascii(*p) && isdigit(*p))
			port = atoi(p);
		*h = '@';
		p = h + 1;
	}
	h = strchr(p, ' ');
	if (h != NULL)
		*h = '\0';
	r = inet_pton(AF_INET, p, &nsip);
	if (r > 0)
	{
		if ((_res.options & RES_INIT) == 0)
			(void) res_init();
		dns_addns(&nsip, port);
	}
	if (h != NULL)
		*h = ' ';
	return r > 0 ? 0 : -1;
}
# endif /* DNSSEC_TEST || _FFR_NAMESERVER */
#endif /* NAMED_BIND */
