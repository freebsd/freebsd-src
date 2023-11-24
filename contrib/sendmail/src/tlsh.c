/*
 * Copyright (c) 2015 Proofpoint, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#include <sendmail.h>

SM_RCSID("@(#)$Id: tls.c,v 8.127 2013-11-27 02:51:11 gshapiro Exp $")

#if STARTTLS
# include <tls.h>

/*
**  DATA2HEX -- create a printable hex string from binary data ("%02X:")
**
**	Parameters:
**		buf -- data
**		len -- length of data
**		hex -- output buffer
**		hlen -- length of output buffer
**
**	Returns:
**		<0: errno
**		>0: length of data in hex
*/

int
data2hex(buf, blen, hex, hlen)
	unsigned char *buf;
	int blen;
	unsigned char *hex;
	int hlen;
{
	int r, h;
	static const char hexcodes[] = "0123456789ABCDEF";

	SM_REQUIRE(buf != NULL);
	SM_REQUIRE(hex != NULL);
	if (blen * 3 + 2 > hlen)
		return -ERANGE;

	for (r = 0, h = 0; r < blen && h + 3 < hlen; r++)
	{
		hex[h++] = hexcodes[(buf[r] & 0xf0) >> 4];
		hex[h++] = hexcodes[(buf[r] & 0x0f)];
		if (r + 1 < blen)
			hex[h++] = ':';
	}
	if (h >= hlen)
		return -ERANGE;
	hex[h] = '\0';
	return h;
}

# if DANE

/*
**  TLS_DATA_MD -- calculate MD for data
**
**	Parameters:
**		buf -- data (in and out!)
**		len -- length of data
**		md -- digest algorithm
**
**	Returns:
**		<=0: cert fp calculation failed
**		>0: len of fp
**
**	Side Effects:
**		writes digest to buf
*/

static int
tls_data_md(buf, len, md)
	unsigned char *buf;
	int len;
	const EVP_MD *md;
{
	unsigned int md_len;
	EVP_MD_CTX *mdctx;
	unsigned char md_buf[EVP_MAX_MD_SIZE];

	SM_REQUIRE(buf != NULL);
	SM_REQUIRE(md != NULL);
	SM_REQUIRE(len >= EVP_MAX_MD_SIZE);

	mdctx = EVP_MD_CTX_create();
	if (EVP_DigestInit_ex(mdctx, md, NULL) != 1)
		return -EINVAL;
	if (EVP_DigestUpdate(mdctx, (void *)buf, len) != 1)
		return -EINVAL;
	if (EVP_DigestFinal_ex(mdctx, md_buf, &md_len) != 1)
		return -EINVAL;
	EVP_MD_CTX_destroy(mdctx);

	if (md_len > len)
		return -ERANGE;
	(void) memcpy(buf, md_buf, md_len);
	return (int)md_len;
}

/*
**  PUBKEY_FP -- get public key fingerprint
**
**	Parameters:
**		cert -- TLS cert
**		mdalg -- name of digest algorithm
**		fp -- (pointer to) fingerprint buffer
**
**	Returns:
**		<=0: cert fp calculation failed
**		>0: len of fp
*/

int
pubkey_fp(cert, mdalg, fp)
	X509 *cert;
	const char *mdalg;
	char **fp;
{
	int len, r;
	unsigned char *buf, *end;
	const EVP_MD *md;

	SM_ASSERT(cert != NULL);
	SM_ASSERT(fp != NULL);
	SM_ASSERT(mdalg != NULL);

	len = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(cert), NULL);

	/* what's an acceptable upper limit? */
	if (len <= 0 || len >= 8192)
		return -EINVAL;
	if (len < EVP_MAX_MD_SIZE)
		len = EVP_MAX_MD_SIZE;
	end = buf = sm_malloc(len);
	if (NULL == buf)
		return -ENOMEM;

	if ('\0' == mdalg[0])
	{
		r = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(cert), &end);
		if (r <= 0 || r != len)
			return -EINVAL;
		*fp = (char *)buf;
		return len;
	}

	md = EVP_get_digestbyname(mdalg);
	if (NULL == md)
	{
		SM_FREE(buf);
		return DANE_VRFY_FAIL;
	}
	len = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(cert), &end);
	r = tls_data_md(buf, len, md);
	if (r < 0)
		sm_free(buf);
	else
		*fp = (char *)buf;
	return r;
}

/*
**  DANE_TLSA_CHK -- check whether a TLSA RR is ok to use
**
**	Parameters:
**		rr -- RR
**		len -- length of RR
**		host -- name of host for RR (only for logging)
**		log -- whether to log problems
**
**	Returns:
**		TLSA_*, see tls.h
*/

int
dane_tlsa_chk(rr, len, host, log)
	const char *rr;
	int len;
	const char *host;
	bool log;
{
	int alg;

	if (len < 4)
	{
		if (log && LogLevel > 8)
			sm_syslog(LOG_WARNING, NOQID,
				  "TLSA=%s, len=%d, status=bogus",
				  host, len);
		return TLSA_BOGUS;
	}
	SM_ASSERT(rr != NULL);

	alg = (int)rr[2];
	if ((int)rr[0] == 3 && (int)rr[1] == 1 && (alg >= 0 && alg <= 2))
		return alg;
	if (log && LogLevel > 9)
		sm_syslog(LOG_NOTICE, NOQID,
			  "TLSA=%s, type=%d-%d-%d:%02x, status=unsupported",
			  host, (int)rr[0], (int)rr[1], (int)rr[2],
			  (int)rr[3]);
	return TLSA_UNSUPP;
}

/*
**  DANE_TLSA_CLR -- clear data in a dane_tlsa structure (for use)
**
**	Parameters:
**		dane_tlsa -- dane_tlsa to clear
**
**	Returns:
**		1 if NULL
**		0 if ok
*/

int
dane_tlsa_clr(dane_tlsa)
	dane_tlsa_P dane_tlsa;
{
	int i;

	if (dane_tlsa == NULL)
		return 1;
	for (i = 0; i < dane_tlsa->dane_tlsa_n; i++)
	{
		SM_FREE(dane_tlsa->dane_tlsa_rr[i]);
		dane_tlsa->dane_tlsa_len[i] = 0;
	}
	SM_FREE(dane_tlsa->dane_tlsa_sni);
	memset(dane_tlsa, '\0', sizeof(*dane_tlsa));
	return 0;

}

/*
**  DANE_TLSA_FREE -- free a dane_tlsa structure
**
**	Parameters:
**		dane_tlsa -- dane_tlsa to free
**
**	Returns:
**		0 if ok
**		1 if NULL
*/

int
dane_tlsa_free(dane_tlsa)
	dane_tlsa_P dane_tlsa;
{
	if (dane_tlsa == NULL)
		return 1;
	dane_tlsa_clr(dane_tlsa);
	SM_FREE(dane_tlsa);
	return 0;

}
# endif /* DANE */

#endif /* STARTTLS */
