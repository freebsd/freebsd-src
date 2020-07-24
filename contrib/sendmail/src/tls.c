/*
 * Copyright (c) 2000-2006, 2008, 2009, 2011, 2013-2016 Proofpoint, Inc. and its suppliers.
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
# include <openssl/err.h>
# include <openssl/bio.h>
# include <openssl/pem.h>
# ifndef HASURANDOMDEV
#  include <openssl/rand.h>
# endif
# include <openssl/engine.h>
# if _FFR_TLS_ALTNAMES
#  include <openssl/x509v3.h>
# endif

# if defined(OPENSSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER <= 0x00907000L
# ERROR: OpenSSL version OPENSSL_VERSION_NUMBER is unsupported.
# endif

# if OPENSSL_VERSION_NUMBER >= 0x10100000L && OPENSSL_VERSION_NUMBER < 0x20000000L
#  define MTA_HAVE_DH_set0_pqg 1
#  define MTA_HAVE_DSA_GENERATE_EX	1

#  define MTA_HAVE_OPENSSL_init_ssl	1
#  define MTA_ASN1_STRING_data ASN1_STRING_get0_data
#  include <openssl/bn.h>
#  include <openssl/dsa.h>
# else
#  define X509_STORE_CTX_get0_cert(ctx)	(ctx)->cert
#  define MTA_RSA_TMP_CB	1
#  define MTA_ASN1_STRING_data ASN1_STRING_data
# endif

# if !TLS_NO_RSA && MTA_RSA_TMP_CB
static RSA *rsa_tmp = NULL;	/* temporary RSA key */
static RSA *tmp_rsa_key __P((SSL *, int, int));
# endif
static int	tls_verify_cb __P((X509_STORE_CTX *, void *));

static int x509_verify_cb __P((int, X509_STORE_CTX *));

static void	apps_ssl_info_cb __P((const SSL *, int , int));
static bool	tls_ok_f __P((char *, char *, int));
static bool	tls_safe_f __P((char *, long, bool));
static int	tls_verify_log __P((int, X509_STORE_CTX *, const char *));

int TLSsslidx = -1;

# if !NO_DH
# include <openssl/dh.h>
static DH *get_dh512 __P((void));

static unsigned char dh512_p[] =
{
	0xDA,0x58,0x3C,0x16,0xD9,0x85,0x22,0x89,0xD0,0xE4,0xAF,0x75,
	0x6F,0x4C,0xCA,0x92,0xDD,0x4B,0xE5,0x33,0xB8,0x04,0xFB,0x0F,
	0xED,0x94,0xEF,0x9C,0x8A,0x44,0x03,0xED,0x57,0x46,0x50,0xD3,
	0x69,0x99,0xDB,0x29,0xD7,0x76,0x27,0x6B,0xA2,0xD3,0xD4,0x12,
	0xE2,0x18,0xF4,0xDD,0x1E,0x08,0x4C,0xF6,0xD8,0x00,0x3E,0x7C,
	0x47,0x74,0xE8,0x33
};
static unsigned char dh512_g[] =
{
	0x02
};

static DH *
get_dh512()
{
	DH *dh = NULL;
#  if MTA_HAVE_DH_set0_pqg
	BIGNUM *dhp_bn, *dhg_bn;
#  endif

	if ((dh = DH_new()) == NULL)
		return NULL;
#  if MTA_HAVE_DH_set0_pqg
	dhp_bn = BN_bin2bn(dh512_p, sizeof (dh512_p), NULL);
	dhg_bn = BN_bin2bn(dh512_g, sizeof (dh512_g), NULL);
	if (dhp_bn == NULL || dhg_bn == NULL || !DH_set0_pqg(dh, dhp_bn, NULL, dhg_bn))  {
		DH_free(dh);
		BN_free(dhp_bn);
		BN_free(dhg_bn);
		return NULL;
	}
#  else
	dh->p = BN_bin2bn(dh512_p, sizeof(dh512_p), NULL);
	dh->g = BN_bin2bn(dh512_g, sizeof(dh512_g), NULL);
	if ((dh->p == NULL) || (dh->g == NULL))
	{
		DH_free(dh);
		return NULL;
	}
#  endif
	return dh;
}

#  if 0

This is the data from which the C code has been generated:

-----BEGIN DH PARAMETERS-----
MIIBCAKCAQEArDcgcLpxEksQHPlolRKCUJ2szKRziseWV9cUSQNZGxoGw7KkROz4
HF9QSbg5axyNIG+QbZYtx0jp3l6/GWq1dLOj27yZkgYgaYgFrvKPiZ2jJ5xETQVH
UpZwbjRcyjyWkWYJVsx1aF4F/iY4kT0n/+iGEoimI3C9V3KXTJ2S6jIkyJ6M/CrN
EtrDynMlUMGlc7S1ouXVOTrtKeqy3S2L9eBLxVI+sChEijGIfELupdVeXihK006p
MgnABPDbkTx6OOtYmSZaGQX+OLW2FPmwvcrzgCz9t9cAsuUcBZv1LeHEqZZttyLU
oK0jjSXgFyeU4/NfyA+zuNeWzUL6bHmigwIBAg==
-----END DH PARAMETERS-----
#  endif /* 0 */

static DH *
get_dh2048()
{
	static unsigned char dh2048_p[]={
		0xAC,0x37,0x20,0x70,0xBA,0x71,0x12,0x4B,0x10,0x1C,0xF9,0x68,
		0x95,0x12,0x82,0x50,0x9D,0xAC,0xCC,0xA4,0x73,0x8A,0xC7,0x96,
		0x57,0xD7,0x14,0x49,0x03,0x59,0x1B,0x1A,0x06,0xC3,0xB2,0xA4,
		0x44,0xEC,0xF8,0x1C,0x5F,0x50,0x49,0xB8,0x39,0x6B,0x1C,0x8D,
		0x20,0x6F,0x90,0x6D,0x96,0x2D,0xC7,0x48,0xE9,0xDE,0x5E,0xBF,
		0x19,0x6A,0xB5,0x74,0xB3,0xA3,0xDB,0xBC,0x99,0x92,0x06,0x20,
		0x69,0x88,0x05,0xAE,0xF2,0x8F,0x89,0x9D,0xA3,0x27,0x9C,0x44,
		0x4D,0x05,0x47,0x52,0x96,0x70,0x6E,0x34,0x5C,0xCA,0x3C,0x96,
		0x91,0x66,0x09,0x56,0xCC,0x75,0x68,0x5E,0x05,0xFE,0x26,0x38,
		0x91,0x3D,0x27,0xFF,0xE8,0x86,0x12,0x88,0xA6,0x23,0x70,0xBD,
		0x57,0x72,0x97,0x4C,0x9D,0x92,0xEA,0x32,0x24,0xC8,0x9E,0x8C,
		0xFC,0x2A,0xCD,0x12,0xDA,0xC3,0xCA,0x73,0x25,0x50,0xC1,0xA5,
		0x73,0xB4,0xB5,0xA2,0xE5,0xD5,0x39,0x3A,0xED,0x29,0xEA,0xB2,
		0xDD,0x2D,0x8B,0xF5,0xE0,0x4B,0xC5,0x52,0x3E,0xB0,0x28,0x44,
		0x8A,0x31,0x88,0x7C,0x42,0xEE,0xA5,0xD5,0x5E,0x5E,0x28,0x4A,
		0xD3,0x4E,0xA9,0x32,0x09,0xC0,0x04,0xF0,0xDB,0x91,0x3C,0x7A,
		0x38,0xEB,0x58,0x99,0x26,0x5A,0x19,0x05,0xFE,0x38,0xB5,0xB6,
		0x14,0xF9,0xB0,0xBD,0xCA,0xF3,0x80,0x2C,0xFD,0xB7,0xD7,0x00,
		0xB2,0xE5,0x1C,0x05,0x9B,0xF5,0x2D,0xE1,0xC4,0xA9,0x96,0x6D,
		0xB7,0x22,0xD4,0xA0,0xAD,0x23,0x8D,0x25,0xE0,0x17,0x27,0x94,
		0xE3,0xF3,0x5F,0xC8,0x0F,0xB3,0xB8,0xD7,0x96,0xCD,0x42,0xFA,
		0x6C,0x79,0xA2,0x83,
		};
	static unsigned char dh2048_g[]={ 0x02, };
	DH *dh;
#  if MTA_HAVE_DH_set0_pqg
	BIGNUM *dhp_bn, *dhg_bn;
#  endif

	if ((dh=DH_new()) == NULL)
		return(NULL);
#  if MTA_HAVE_DH_set0_pqg
	dhp_bn = BN_bin2bn(dh2048_p, sizeof (dh2048_p), NULL);
	dhg_bn = BN_bin2bn(dh2048_g, sizeof (dh2048_g), NULL);
	if (dhp_bn == NULL || dhg_bn == NULL || !DH_set0_pqg(dh, dhp_bn, NULL, dhg_bn))  {
		DH_free(dh);
		BN_free(dhp_bn);
		BN_free(dhg_bn);
		return NULL;
	}
#  else
	dh->p=BN_bin2bn(dh2048_p,sizeof(dh2048_p),NULL);
	dh->g=BN_bin2bn(dh2048_g,sizeof(dh2048_g),NULL);
	if ((dh->p == NULL) || (dh->g == NULL))
	{
		DH_free(dh);
		return(NULL);
	}
#  endif
	return(dh);
}
# endif /* !NO_DH */


/*
**  TLS_RAND_INIT -- initialize STARTTLS random generator
**
**	Parameters:
**		randfile -- name of file with random data
**		logl -- loglevel
**
**	Returns:
**		success/failure
**
**	Side Effects:
**		initializes PRNG for tls library.
*/

# define MIN_RAND_BYTES	128	/* 1024 bits */

# define RF_OK		0	/* randfile OK */
# define RF_MISS	1	/* randfile == NULL || *randfile == '\0' */
# define RF_UNKNOWN	2	/* unknown prefix for randfile */

# define RI_NONE	0	/* no init yet */
# define RI_SUCCESS	1	/* init was successful */
# define RI_FAIL	2	/* init failed */

static bool	tls_rand_init __P((char *, int));

static bool
tls_rand_init(randfile, logl)
	char *randfile;
	int logl;
{
# ifndef HASURANDOMDEV
	/* not required if /dev/urandom exists, OpenSSL does it internally */

	bool ok;
	int randdef;
	static int done = RI_NONE;

	/*
	**  initialize PRNG
	*/

	/* did we try this before? if yes: return old value */
	if (done != RI_NONE)
		return done == RI_SUCCESS;

	/* set default values */
	ok = false;
	done = RI_FAIL;
	randdef = (randfile == NULL || *randfile == '\0') ? RF_MISS : RF_OK;
#  if EGD
	if (randdef == RF_OK && sm_strncasecmp(randfile, "egd:", 4) == 0)
	{
		randfile += 4;
		if (RAND_egd(randfile) < 0)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS: RAND_egd(%s) failed: random number generator not seeded",
				   randfile);
		}
		else
			ok = true;
	}
	else
#  endif /* EGD */
	if (randdef == RF_OK && sm_strncasecmp(randfile, "file:", 5) == 0)
	{
		int fd;
		long sff;
		struct stat st;

		randfile += 5;
		sff = SFF_SAFEDIRPATH | SFF_NOWLINK
		      | SFF_NOGWFILES | SFF_NOWWFILES
		      | SFF_NOGRFILES | SFF_NOWRFILES
		      | SFF_MUSTOWN | SFF_ROOTOK | SFF_OPENASROOT;
		if (DontLockReadFiles)
			sff |= SFF_NOLOCK;
		if ((fd = safeopen(randfile, O_RDONLY, 0, sff)) >= 0)
		{
			if (fstat(fd, &st) < 0)
			{
				if (LogLevel > logl)
					sm_syslog(LOG_ERR, NOQID,
						  "STARTTLS: can't fstat(%s)",
						  randfile);
			}
			else
			{
				bool use, problem;

				use = true;
				problem = false;

				/* max. age of file: 10 minutes */
				if (st.st_mtime + 600 < curtime())
				{
					use = bitnset(DBS_INSUFFICIENTENTROPY,
						      DontBlameSendmail);
					problem = true;
					if (LogLevel > logl)
						sm_syslog(LOG_ERR, NOQID,
							  "STARTTLS: RandFile %s too old: %s",
							  randfile,
							  use ? "unsafe" :
								"unusable");
				}
				if (use && st.st_size < MIN_RAND_BYTES)
				{
					use = bitnset(DBS_INSUFFICIENTENTROPY,
						      DontBlameSendmail);
					problem = true;
					if (LogLevel > logl)
						sm_syslog(LOG_ERR, NOQID,
							  "STARTTLS: size(%s) < %d: %s",
							  randfile,
							  MIN_RAND_BYTES,
							  use ? "unsafe" :
								"unusable");
				}
				if (use)
					ok = RAND_load_file(randfile, -1) >=
					     MIN_RAND_BYTES;
				if (use && !ok)
				{
					if (LogLevel > logl)
						sm_syslog(LOG_WARNING, NOQID,
							  "STARTTLS: RAND_load_file(%s) failed: random number generator not seeded",
							  randfile);
				}
				if (problem)
					ok = false;
			}
			if (ok || bitnset(DBS_INSUFFICIENTENTROPY,
					  DontBlameSendmail))
			{
				/* add this even if fstat() failed */
				RAND_seed((void *) &st, sizeof(st));
			}
			(void) close(fd);
		}
		else
		{
			if (LogLevel > logl)
				sm_syslog(LOG_WARNING, NOQID,
					  "STARTTLS: Warning: safeopen(%s) failed",
					  randfile);
		}
	}
	else if (randdef == RF_OK)
	{
		if (LogLevel > logl)
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS: Error: no proper random file definition %s",
				  randfile);
		randdef = RF_UNKNOWN;
	}
	if (randdef == RF_MISS)
	{
		if (LogLevel > logl)
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS: Error: missing random file definition");
	}
	if (!ok && bitnset(DBS_INSUFFICIENTENTROPY, DontBlameSendmail))
	{
		int i;
		long r;
		unsigned char buf[MIN_RAND_BYTES];

		/* assert((MIN_RAND_BYTES % sizeof(long)) == 0); */
		for (i = 0; i <= sizeof(buf) - sizeof(long); i += sizeof(long))
		{
			r = get_random();
			(void) memcpy(buf + i, (void *) &r, sizeof(long));
		}
		RAND_seed(buf, sizeof(buf));
		if (LogLevel > logl)
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS: Warning: random number generator not properly seeded");
		ok = true;
	}
	done = ok ? RI_SUCCESS : RI_FAIL;
	return ok;
# else /* ! HASURANDOMDEV */
	return true;
# endif /* ! HASURANDOMDEV */
}
/*
**  INIT_TLS_LIBRARY -- Calls functions which setup TLS library for global use.
**
**	Parameters:
**		fipsmode -- use FIPS?
**
**	Returns:
**		0: OK
**		<0: perm.fail
**		>0: fail but can continue
*/

int
init_tls_library(fipsmode)
	bool fipsmode;
{
	bool bv;

	/*
	**  OPENSSL_init_ssl(3): "As of version 1.1.0 OpenSSL will
	**  automatically allocate all resources that it needs
	**  so no explicit initialisation is required."
	*/

# if !MTA_HAVE_OPENSSL_init_ssl
	/* basic TLS initialization, ignore result for now */
	SSL_library_init();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();
# endif

	bv = true;
	if (TLSsslidx < 0)
	{
		TLSsslidx = SSL_get_ex_new_index(0, 0, 0, 0, 0);
		if (TLSsslidx < 0)
		{
			if (LogLevel > 0)
				sm_syslog(LOG_ERR, NOQID,
					"STARTTLS=init, SSL_get_ex_new_index=%d",
					TLSsslidx);
			bv = false;
		}
	}

	if (bv)
		bv = tls_rand_init(RandFile, 7);
# if _FFR_FIPSMODE
	if (bv && fipsmode)
	{
		if (!FIPS_mode_set(1))
		{
			unsigned long err;

			err = ERR_get_error();
			if (LogLevel > 0)
				sm_syslog(LOG_ERR, NOQID,
					"STARTTLS=init, FIPSMode=%s",
					ERR_error_string(err, NULL));
			return -1;
		}
		else
		{
			if (LogLevel > 9)
				sm_syslog(LOG_INFO, NOQID,
					"STARTTLS=init, FIPSMode=ok");
		}
		if (CertFingerprintAlgorithm == NULL)
			CertFingerprintAlgorithm = "sha1";
	}
# endif /* _FFR_FIPSMODE  */

	if (!TLS_set_engine(SSLEngine, true))
	{
		if (LogLevel > 0)
			sm_syslog(LOG_ERR, NOQID,
				  "STARTTLS=init, engine=%s, TLS_set_engine=failed",
				  SSLEngine);
		return -1;
	}

	if (bv && CertFingerprintAlgorithm != NULL)
	{
		const EVP_MD *md;

		md = EVP_get_digestbyname(CertFingerprintAlgorithm);
		if (NULL == md)
		{
			bv = false;
			if (LogLevel > 0)
				sm_syslog(LOG_ERR, NOQID,
					"STARTTLS=init, CertFingerprintAlgorithm=%s, status=invalid"
					, CertFingerprintAlgorithm);
		}
		else
			EVP_digest = md;
	}
	return bv ? 0 : 1;
}

/*
**  TLS_SET_VERIFY -- request client certificate?
**
**	Parameters:
**		ctx -- TLS context
**		ssl -- TLS session context
**		vrfy -- request certificate?
**
**	Returns:
**		none.
**
**	Side Effects:
**		Sets verification state for TLS
**
# if TLS_VRFY_PER_CTX
**	Notice:
**		This is per TLS context, not per TLS structure;
**		the former is global, the latter per connection.
**		It would be nice to do this per connection, but this
**		doesn't work in the current TLS libraries :-(
# endif * TLS_VRFY_PER_CTX *
*/

void
tls_set_verify(ctx, ssl, vrfy)
	SSL_CTX *ctx;
	SSL *ssl;
	bool vrfy;
{
# if !TLS_VRFY_PER_CTX
	SSL_set_verify(ssl, vrfy ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, NULL);
# else
	SSL_CTX_set_verify(ctx, vrfy ? SSL_VERIFY_PEER : SSL_VERIFY_NONE,
			NULL);
# endif
}

/*
**  status in initialization
**  these flags keep track of the status of the initialization
**  i.e., whether a file exists (_EX) and whether it can be used (_OK)
**  [due to permissions]
*/

# define TLS_S_NONE	0x00000000	/* none yet */
# define TLS_S_CERT_EX	0x00000001	/* cert file exists */
# define TLS_S_CERT_OK	0x00000002	/* cert file is ok */
# define TLS_S_KEY_EX	0x00000004	/* key file exists */
# define TLS_S_KEY_OK	0x00000008	/* key file is ok */
# define TLS_S_CERTP_EX	0x00000010	/* CA cert path exists */
# define TLS_S_CERTP_OK	0x00000020	/* CA cert path is ok */
# define TLS_S_CERTF_EX	0x00000040	/* CA cert file exists */
# define TLS_S_CERTF_OK	0x00000080	/* CA cert file is ok */
# define TLS_S_CRLF_EX	0x00000100	/* CRL file exists */
# define TLS_S_CRLF_OK	0x00000200	/* CRL file is ok */

# define TLS_S_CERT2_EX	0x00001000	/* 2nd cert file exists */
# define TLS_S_CERT2_OK	0x00002000	/* 2nd cert file is ok */
# define TLS_S_KEY2_EX	0x00004000	/* 2nd key file exists */
# define TLS_S_KEY2_OK	0x00008000	/* 2nd key file is ok */

# define TLS_S_DH_OK	0x00200000	/* DH cert is ok */
# define TLS_S_DHPAR_EX	0x00400000	/* DH param file exists */
# define TLS_S_DHPAR_OK	0x00800000	/* DH param file is ok to use */

/* Type of variable */
# define TLS_T_OTHER	0
# define TLS_T_SRV	1
# define TLS_T_CLT	2

/*
**  TLS_OK_F -- can var be an absolute filename?
**
**	Parameters:
**		var -- filename
**		fn -- what is the filename used for?
**		type -- type of variable
**
**	Returns:
**		ok?
*/

static bool
tls_ok_f(var, fn, type)
	char *var;
	char *fn;
	int type;
{
	/* must be absolute pathname */
	if (var != NULL && *var == '/')
		return true;
	if (LogLevel > 12)
		sm_syslog(LOG_WARNING, NOQID, "STARTTLS: %s%s missing",
			  type == TLS_T_SRV ? "Server" :
			  (type == TLS_T_CLT ? "Client" : ""), fn);
	return false;
}
/*
**  TLS_SAFE_F -- is a file safe to use?
**
**	Parameters:
**		var -- filename
**		sff -- flags for safefile()
**		srv -- server side?
**
**	Returns:
**		ok?
*/

static bool
tls_safe_f(var, sff, srv)
	char *var;
	long sff;
	bool srv;
{
	int ret;

	if ((ret = safefile(var, RunAsUid, RunAsGid, RunAsUserName, sff,
			    S_IRUSR, NULL)) == 0)
		return true;
	if (LogLevel > 7)
		sm_syslog(LOG_WARNING, NOQID, "STARTTLS=%s: file %s unsafe: %s",
			  srv ? "server" : "client", var, sm_errstring(ret));
	return false;
}

/*
**  TLS_OK_F -- macro to simplify calls to tls_ok_f
**
**	Parameters:
**		var -- filename
**		fn -- what is the filename used for?
**		req -- is the file required?
**		st -- status bit to set if ok
**		type -- type of variable
**
**	Side Effects:
**		uses r, ok; may change ok and status.
**
*/

# define TLS_OK_F(var, fn, req, st, type) if (ok) \
	{ \
		r = tls_ok_f(var, fn, type); \
		if (r) \
			status |= st; \
		else if (req) \
			ok = false; \
	}

/*
**  TLS_UNR -- macro to return whether a file should be unreadable
**
**	Parameters:
**		bit -- flag to test
**		req -- flags
**
**	Returns:
**		0/SFF_NORFILES
*/

# define TLS_UNR(bit, req)	(bitset(bit, req) ? SFF_NORFILES : 0)
# define TLS_OUNR(bit, req)	(bitset(bit, req) ? SFF_NOWRFILES : 0)
# define TLS_KEYSFF(req)	\
	(bitnset(DBS_GROUPREADABLEKEYFILE, DontBlameSendmail) ?	\
		TLS_OUNR(TLS_I_KEY_OUNR, req) :			\
		TLS_UNR(TLS_I_KEY_UNR, req))

/*
**  TLS_SAFE_F -- macro to simplify calls to tls_safe_f
**
**	Parameters:
**		var -- filename
**		sff -- flags for safefile()
**		req -- is the file required?
**		ex -- does the file exist?
**		st -- status bit to set if ok
**		srv -- server side?
**
**	Side Effects:
**		uses r, ok, ex; may change ok and status.
**
*/

# define TLS_SAFE_F(var, sff, req, ex, st, srv) if (ex && ok) \
	{ \
		r = tls_safe_f(var, sff, srv); \
		if (r) \
			status |= st;	\
		else if (req) \
			ok = false;	\
	}

/*
**  LOAD_CERTKEY -- load cert/key for TLS session
**
**	Parameters:
**		ssl -- TLS session context
**		srv -- server side?
**		certfile -- filename of certificate
**		keyfile -- filename of private key
**
**	Returns:
**		succeeded?
*/

bool
load_certkey(ssl, srv, certfile, keyfile)
	SSL *ssl;
	bool srv;
	char *certfile;
	char *keyfile;
{
	bool ok;
	int r;
	long sff, status;
	unsigned long req;
	char *who;

	ok = true;
	who = srv ? "server" : "client";
	status = TLS_S_NONE;
	req = TLS_I_CERT_EX|TLS_I_KEY_EX;
	TLS_OK_F(certfile, "CertFile", bitset(TLS_I_CERT_EX, req),
		 TLS_S_CERT_EX, srv ? TLS_T_SRV : TLS_T_CLT);
	TLS_OK_F(keyfile, "KeyFile", bitset(TLS_I_KEY_EX, req),
		 TLS_S_KEY_EX, srv ? TLS_T_SRV : TLS_T_CLT);

	/* certfile etc. must be "safe". */
	sff = SFF_REGONLY | SFF_SAFEDIRPATH | SFF_NOWLINK
	     | SFF_NOGWFILES | SFF_NOWWFILES
	     | SFF_MUSTOWN | SFF_ROOTOK | SFF_OPENASROOT;
	if (DontLockReadFiles)
		sff |= SFF_NOLOCK;

	TLS_SAFE_F(certfile, sff | TLS_UNR(TLS_I_CERT_UNR, req),
		   bitset(TLS_I_CERT_EX, req),
		   bitset(TLS_S_CERT_EX, status), TLS_S_CERT_OK, srv);
	TLS_SAFE_F(keyfile, sff | TLS_KEYSFF(req),
		   bitset(TLS_I_KEY_EX, req),
		   bitset(TLS_S_KEY_EX, status), TLS_S_KEY_OK, srv);

# define SSL_use_cert(ssl, certfile) \
	SSL_use_certificate_file(ssl, certfile, SSL_FILETYPE_PEM)
# define SSL_USE_CERT "SSL_use_certificate_file"

	if (bitset(TLS_S_CERT_OK, status) &&
	    SSL_use_cert(ssl, certfile) <= 0)
	{
		if (LogLevel > 7)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, error: %s(%s) failed",
				  who, SSL_USE_CERT, certfile);
			tlslogerr(LOG_WARNING, 9, who);
		}
		if (bitset(TLS_I_USE_CERT, req))
			return false;
	}
	if (bitset(TLS_S_KEY_OK, status) &&
	    SSL_use_PrivateKey_file(ssl, keyfile, SSL_FILETYPE_PEM) <= 0)
	{
		if (LogLevel > 7)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, error: SSL_use_PrivateKey_file(%s) failed",
				  who, keyfile);
			tlslogerr(LOG_WARNING, 9, who);
		}
		if (bitset(TLS_I_USE_KEY, req))
			return false;
	}

	/* check the private key */
	if (bitset(TLS_S_KEY_OK, status) &&
	    (r = SSL_check_private_key(ssl)) <= 0)
	{
		/* Private key does not match the certificate public key */
		if (LogLevel > 5)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, error: SSL_check_private_key failed(%s): %d",
				  who, keyfile, r);
			tlslogerr(LOG_WARNING, 9, who);
		}
		if (bitset(TLS_I_USE_KEY, req))
			return false;
	}

	return true;
}

/*
**  LOAD_CRLFILE -- load a file holding a CRL into the TLS context
**
**	Parameters:
**		ctx -- TLS context
**		srv -- server side?
**		filename -- filename of CRL
**
**	Returns:
**		succeeded?
*/

static bool load_crlfile __P((SSL_CTX *, bool, char *));

static bool
load_crlfile(ctx, srv, filename)
	SSL_CTX *ctx;
	bool srv;
	char *filename;
{
	char *who;
	BIO *crl_file;
	X509_CRL *crl;
	X509_STORE *store;

	who = srv ? "server" : "client";
	crl_file = BIO_new(BIO_s_file());
	if (crl_file == NULL)
	{
		if (LogLevel > 9)
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, error: BIO_new=failed", who);
		return false;
	}

	if (BIO_read_filename(crl_file, filename) < 0)
	{
		if (LogLevel > 9)
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, error: BIO_read_filename(%s)=failed",
				  who, filename);

		/* avoid memory leaks */
		BIO_free(crl_file);
		return false;
	}

	crl = PEM_read_bio_X509_CRL(crl_file, NULL, NULL, NULL);
	if (crl == NULL)
	{
		if (LogLevel > 9)
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, error: PEM_read_bio_X509_CRL(%s)=failed",
				  who, filename);
		BIO_free(crl_file);
		return true;	/* XXX should probably be 'false' */
	}

	BIO_free(crl_file);

	/* get a pointer to the current certificate validation store */
	store = SSL_CTX_get_cert_store(ctx);	/* does not fail */

	if (X509_STORE_add_crl(store, crl) == 0)
	{
		if (LogLevel > 9)
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, error: X509_STORE_add_crl=failed",
				  who);
		X509_CRL_free(crl);
		return false;
	}

	X509_CRL_free(crl);

	X509_STORE_set_flags(store,
		X509_V_FLAG_CRL_CHECK|X509_V_FLAG_CRL_CHECK_ALL);
	X509_STORE_set_verify_cb_func(store, x509_verify_cb);

	return true;
}

/*
**  LOAD_CRLPATH -- configure the TLS context to lookup CRLs in a directory
**
**	Parameters:
**		ctx -- TLS context
**		srv -- server side?
**		path -- path of hashed directory of CRLs
**
**	Returns:
**		succeeded?
*/

static bool load_crlpath __P((SSL_CTX *, bool, char *));

static bool
load_crlpath(ctx, srv, path)
	SSL_CTX *ctx;
	bool srv;
	char *path;
{
	char *who;
	X509_STORE *store;
	X509_LOOKUP *lookup;

	who = srv ? "server" : "client";

	/* get a pointer to the current certificate validation store */
	store = SSL_CTX_get_cert_store(ctx);	/* does not fail */

	lookup = X509_STORE_add_lookup(store, X509_LOOKUP_hash_dir());
	if (lookup == NULL)
	{
		if (LogLevel > 9)
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, error: X509_STORE_add_lookup(hash)=failed",
				  who);
		return false;
	}

	if (X509_LOOKUP_add_dir(lookup, path, X509_FILETYPE_PEM) == 0)
	{
		if (LogLevel > 9)
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, error: X509_LOOKUP_add_dir(%s)=failed",
				  who, path);
		return false;
	}

	X509_STORE_set_flags(store,
		X509_V_FLAG_CRL_CHECK|X509_V_FLAG_CRL_CHECK_ALL);
	X509_STORE_set_verify_cb_func(store, x509_verify_cb);

	return true;
}

/*
**  INITTLS -- initialize TLS
**
**	Parameters:
**		ctx -- pointer to context
**		req -- requirements for initialization (see sendmail.h)
**		options -- options
**		srv -- server side?
**		certfile -- filename of certificate
**		keyfile -- filename of private key
**		cacertpath -- path to CAs
**		cacertfile -- file with CA(s)
**		dhparam -- parameters for DH
**
**	Returns:
**		succeeded?
*/

/*
**  The session_id_context identifies the service that created a session.
**  This information is used to distinguish between multiple TLS-based
**  servers running on the same server. We use the name of the mail system.
**  Note: the session cache is not persistent.
*/

static char server_session_id_context[] = "sendmail8";

/* 0.9.8a and b have a problem with SSL_OP_TLS_BLOCK_PADDING_BUG */
# if (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
#  define SM_SSL_OP_TLS_BLOCK_PADDING_BUG	1
# else
#  define SM_SSL_OP_TLS_BLOCK_PADDING_BUG	0
# endif

bool
inittls(ctx, req, options, srv, certfile, keyfile, cacertpath, cacertfile, dhparam)
	SSL_CTX **ctx;
	unsigned long req;
	unsigned long options;
	bool srv;
	char *certfile, *keyfile, *cacertpath, *cacertfile, *dhparam;
{
# if !NO_DH
	static DH *dh = NULL;
# endif
	int r;
	bool ok;
	long sff, status;
	char *who;
	char *cf2, *kf2;
# if SM_CONF_SHM && !TLS_NO_RSA && MTA_RSA_TMP_CB
	extern int ShmId;
# endif
# if SM_SSL_OP_TLS_BLOCK_PADDING_BUG
	long rt_version;
	STACK_OF(SSL_COMP) *comp_methods;
# endif

	status = TLS_S_NONE;
	who = srv ? "server" : "client";
	if (ctx == NULL)
	{
		syserr("STARTTLS=%s, inittls: ctx == NULL", who);
		/* NOTREACHED */
		SM_ASSERT(ctx != NULL);
	}

	/* already initialized? (we could re-init...) */
	if (*ctx != NULL)
		return true;
	ok = true;

	/*
	**  look for a second filename: it must be separated by a ','
	**  no blanks allowed (they won't be skipped).
	**  we change a global variable here! this change will be undone
	**  before return from the function but only if it returns true.
	**  this isn't a problem since in a failure case this function
	**  won't be called again with the same (overwritten) values.
	**  otherwise each return must be replaced with a goto endinittls.
	*/

	cf2 = NULL;
	kf2 = NULL;
	if (certfile != NULL && (cf2 = strchr(certfile, ',')) != NULL)
	{
		*cf2++ = '\0';
		if (keyfile != NULL && (kf2 = strchr(keyfile, ',')) != NULL)
			*kf2++ = '\0';
	}

	/*
	**  Check whether files/paths are defined
	*/

	TLS_OK_F(certfile, "CertFile", bitset(TLS_I_CERT_EX, req),
		 TLS_S_CERT_EX, srv ? TLS_T_SRV : TLS_T_CLT);
	TLS_OK_F(keyfile, "KeyFile", bitset(TLS_I_KEY_EX, req),
		 TLS_S_KEY_EX, srv ? TLS_T_SRV : TLS_T_CLT);
	TLS_OK_F(cacertpath, "CACertPath", bitset(TLS_I_CERTP_EX, req),
		 TLS_S_CERTP_EX, TLS_T_OTHER);
	TLS_OK_F(cacertfile, "CACertFile", bitset(TLS_I_CERTF_EX, req),
		 TLS_S_CERTF_EX, TLS_T_OTHER);

	TLS_OK_F(CRLFile, "CRLFile", bitset(TLS_I_CRLF_EX, req),
		 TLS_S_CRLF_EX, TLS_T_OTHER);

	/*
	**  if the second file is specified it must exist
	**  XXX: it is possible here to define only one of those files
	*/

	if (cf2 != NULL)
	{
		TLS_OK_F(cf2, "CertFile", bitset(TLS_I_CERT_EX, req),
			 TLS_S_CERT2_EX, srv ? TLS_T_SRV : TLS_T_CLT);
	}
	if (kf2 != NULL)
	{
		TLS_OK_F(kf2, "KeyFile", bitset(TLS_I_KEY_EX, req),
			 TLS_S_KEY2_EX, srv ? TLS_T_SRV : TLS_T_CLT);
	}

	/*
	**  valid values for dhparam are (only the first char is checked)
	**  none	no parameters: don't use DH
	**  i		use precomputed 2048 bit parameters
	**  512		use precomputed 512 bit parameters
	**  1024	generate 1024 bit parameters
	**  2048	generate 2048 bit parameters
	**  /file/name	read parameters from /file/name
	*/

# define SET_DH_DFL	\
	do {	\
		dhparam = "I";	\
		req |= TLS_I_DHFIXED;	\
	} while (0)

	if (bitset(TLS_I_TRY_DH, req))
	{
		if (dhparam != NULL)
		{
			char c = *dhparam;

			if (c == '1')
				req |= TLS_I_DH1024;
			else if (c == 'I' || c == 'i')
				req |= TLS_I_DHFIXED;
			else if (c == '2')
				req |= TLS_I_DH2048;
			else if (c == '5')
				req |= TLS_I_DH512;
			else if (c == 'n' || c == 'N')
				req &= ~TLS_I_TRY_DH;
			else if (c != '/')
			{
				if (LogLevel > 12)
					sm_syslog(LOG_WARNING, NOQID,
						  "STARTTLS=%s, error: illegal value '%s' for DHParameters",
						  who, dhparam);
				dhparam = NULL;
			}
		}
		if (dhparam == NULL)
			SET_DH_DFL;
		else if (*dhparam == '/')
		{
			TLS_OK_F(dhparam, "DHParameters",
				 bitset(TLS_I_DHPAR_EX, req),
				 TLS_S_DHPAR_EX, TLS_T_OTHER);
		}
	}
	if (!ok)
		return ok;

	/* certfile etc. must be "safe". */
	sff = SFF_REGONLY | SFF_SAFEDIRPATH | SFF_NOWLINK
	     | SFF_NOGWFILES | SFF_NOWWFILES
	     | SFF_MUSTOWN | SFF_ROOTOK | SFF_OPENASROOT;
	if (DontLockReadFiles)
		sff |= SFF_NOLOCK;

	TLS_SAFE_F(certfile, sff | TLS_UNR(TLS_I_CERT_UNR, req),
		   bitset(TLS_I_CERT_EX, req),
		   bitset(TLS_S_CERT_EX, status), TLS_S_CERT_OK, srv);
	TLS_SAFE_F(keyfile, sff | TLS_KEYSFF(req),
		   bitset(TLS_I_KEY_EX, req),
		   bitset(TLS_S_KEY_EX, status), TLS_S_KEY_OK, srv);
	TLS_SAFE_F(cacertfile, sff | TLS_UNR(TLS_I_CERTF_UNR, req),
		   bitset(TLS_I_CERTF_EX, req),
		   bitset(TLS_S_CERTF_EX, status), TLS_S_CERTF_OK, srv);
	if (dhparam != NULL && *dhparam == '/')
	{
		TLS_SAFE_F(dhparam, sff | TLS_UNR(TLS_I_DHPAR_UNR, req),
			   bitset(TLS_I_DHPAR_EX, req),
			   bitset(TLS_S_DHPAR_EX, status), TLS_S_DHPAR_OK, srv);
		if (!bitset(TLS_S_DHPAR_OK, status))
			SET_DH_DFL;
	}
	TLS_SAFE_F(CRLFile, sff | TLS_UNR(TLS_I_CRLF_UNR, req),
		   bitset(TLS_I_CRLF_EX, req),
		   bitset(TLS_S_CRLF_EX, status), TLS_S_CRLF_OK, srv);
	if (!ok)
		return ok;
	if (cf2 != NULL)
	{
		TLS_SAFE_F(cf2, sff | TLS_UNR(TLS_I_CERT_UNR, req),
			   bitset(TLS_I_CERT_EX, req),
			   bitset(TLS_S_CERT2_EX, status), TLS_S_CERT2_OK, srv);
	}
	if (kf2 != NULL)
	{
		TLS_SAFE_F(kf2, sff | TLS_KEYSFF(req),
			   bitset(TLS_I_KEY_EX, req),
			   bitset(TLS_S_KEY2_EX, status), TLS_S_KEY2_OK, srv);
	}

	/* create a method and a new context */
	if ((*ctx = SSL_CTX_new(srv ? SSLv23_server_method() :
				      SSLv23_client_method())) == NULL)
	{
		if (LogLevel > 7)
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, error: SSL_CTX_new(SSLv23_%s_method()) failed",
				  who, who);
		tlslogerr(LOG_WARNING, 9, who);
		return false;
	}

# if _FFR_VRFY_TRUSTED_FIRST
	if (!tTd(88, 101))
	{
		X509_STORE *store;

		/* get a pointer to the current certificate validation store */
		store = SSL_CTX_get_cert_store(*ctx);	/* does not fail */
		SM_ASSERT(store != NULL);
		X509_STORE_set_flags(store, X509_V_FLAG_TRUSTED_FIRST);
	}
# endif

	if (CRLFile != NULL && !load_crlfile(*ctx, srv, CRLFile))
		return false;
	if (CRLPath != NULL && !load_crlpath(*ctx, srv, CRLPath))
		return false;

# if defined(SSL_MODE_AUTO_RETRY) && OPENSSL_VERSION_NUMBER >= 0x10100000L && OPENSSL_VERSION_NUMBER < 0x20000000L
	/*
	 *  Turn off blocking I/O handling in OpenSSL: someone turned
	 *  this on by default in 1.1? should we check first?
	 */
#  if _FFR_TESTS
	if (LogLevel > 9) {
		sff = SSL_CTX_get_mode(*ctx);
		if (sff & SSL_MODE_AUTO_RETRY)
			sm_syslog(LOG_WARNING, NOQID,
				"STARTTLS=%s, SSL_MODE_AUTO_RETRY=set, mode=%#lx",
				who, sff);
	}

	/* hack for testing! */
	if (tTd(96, 101) || getenv("SSL_MODE_AUTO_RETRY") != NULL)
			SSL_CTX_set_mode(*ctx, SSL_MODE_AUTO_RETRY);
	else
#  endif
	SSL_CTX_clear_mode(*ctx, SSL_MODE_AUTO_RETRY);
# endif /* defined(SSL_MODE_AUTO_RETRY) && OPENSSL_VERSION_NUMBER >= 0x10100000L && OPENSSL_VERSION_NUMBER < 0x20000000L */


# if TLS_NO_RSA
	/* turn off backward compatibility, required for no-rsa */
	SSL_CTX_set_options(*ctx, SSL_OP_NO_SSLv2);
# endif


# if !TLS_NO_RSA && MTA_RSA_TMP_CB
	/*
	**  Create a temporary RSA key
	**  XXX  Maybe we shouldn't create this always (even though it
	**  is only at startup).
	**  It is a time-consuming operation and it is not always necessary.
	**  maybe we should do it only on demand...
	*/

	if (bitset(TLS_I_RSA_TMP, req)
#  if SM_CONF_SHM
	    && ShmId != SM_SHM_NO_ID &&
	    (rsa_tmp = RSA_generate_key(RSA_KEYLENGTH, RSA_F4, NULL,
					NULL)) == NULL
#  else /* SM_CONF_SHM */
	    && 0	/* no shared memory: no need to generate key now */
#  endif /* SM_CONF_SHM */
	   )
	{
		if (LogLevel > 7)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, error: RSA_generate_key failed",
				  who);
			tlslogerr(LOG_WARNING, 9, who);
		}
		return false;
	}
# endif /* !TLS_NO_RSA && MTA_RSA_TMP_CB */

	/*
	**  load private key
	**  XXX change this for DSA-only version
	*/

	if (bitset(TLS_S_KEY_OK, status) &&
	    SSL_CTX_use_PrivateKey_file(*ctx, keyfile,
					 SSL_FILETYPE_PEM) <= 0)
	{
		if (LogLevel > 7)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, error: SSL_CTX_use_PrivateKey_file(%s) failed",
				  who, keyfile);
			tlslogerr(LOG_WARNING, 9, who);
		}
		if (bitset(TLS_I_USE_KEY, req))
			return false;
	}

# if _FFR_TLS_USE_CERTIFICATE_CHAIN_FILE
#  define SSL_CTX_use_cert(ssl_ctx, certfile) \
	SSL_CTX_use_certificate_chain_file(ssl_ctx, certfile)
#  define SSL_CTX_USE_CERT "SSL_CTX_use_certificate_chain_file"
# else
#  define SSL_CTX_use_cert(ssl_ctx, certfile) \
	SSL_CTX_use_certificate_file(ssl_ctx, certfile, SSL_FILETYPE_PEM)
#  define SSL_CTX_USE_CERT "SSL_CTX_use_certificate_file"
# endif

	/* get the certificate file */
	if (bitset(TLS_S_CERT_OK, status) &&
	    SSL_CTX_use_cert(*ctx, certfile) <= 0)
	{
		if (LogLevel > 7)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, error: %s(%s) failed",
				  who, SSL_CTX_USE_CERT, certfile);
			tlslogerr(LOG_WARNING, 9, who);
		}
		if (bitset(TLS_I_USE_CERT, req))
			return false;
	}

	/* check the private key */
	if (bitset(TLS_S_KEY_OK, status) &&
	    (r = SSL_CTX_check_private_key(*ctx)) <= 0)
	{
		/* Private key does not match the certificate public key */
		if (LogLevel > 5)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, error: SSL_CTX_check_private_key failed(%s): %d",
				  who, keyfile, r);
			tlslogerr(LOG_WARNING, 9, who);
		}
		if (bitset(TLS_I_USE_KEY, req))
			return false;
	}

	/* XXX this code is pretty much duplicated from above! */

	/* load private key */
	if (bitset(TLS_S_KEY2_OK, status) &&
	    SSL_CTX_use_PrivateKey_file(*ctx, kf2, SSL_FILETYPE_PEM) <= 0)
	{
		if (LogLevel > 7)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, error: SSL_CTX_use_PrivateKey_file(%s) failed",
				  who, kf2);
			tlslogerr(LOG_WARNING, 9, who);
		}
	}

	/* get the certificate file */
	if (bitset(TLS_S_CERT2_OK, status) &&
	    SSL_CTX_use_cert(*ctx, cf2) <= 0)
	{
		if (LogLevel > 7)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, error: %s(%s) failed",
				  who, SSL_CTX_USE_CERT, cf2);
			tlslogerr(LOG_WARNING, 9, who);
		}
	}

	/* also check the private key */
	if (bitset(TLS_S_KEY2_OK, status) &&
	    (r = SSL_CTX_check_private_key(*ctx)) <= 0)
	{
		/* Private key does not match the certificate public key */
		if (LogLevel > 5)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, error: SSL_CTX_check_private_key 2 failed: %d",
				  who, r);
			tlslogerr(LOG_WARNING, 9, who);
		}
	}

	/* SSL_CTX_set_quiet_shutdown(*ctx, 1); violation of standard? */

# if SM_SSL_OP_TLS_BLOCK_PADDING_BUG

	/*
	**  In OpenSSL 0.9.8[ab], enabling zlib compression breaks the
	**  padding bug work-around, leading to false positives and
	**  failed connections. We may not interoperate with systems
	**  with the bug, but this is better than breaking on all 0.9.8[ab]
	**  systems that have zlib support enabled.
	**  Note: this checks the runtime version of the library, not
	**  just the compile time version.
	*/

	rt_version = TLS_version_num();
	if (rt_version >= 0x00908000L && rt_version <= 0x0090802fL)
	{
		comp_methods = SSL_COMP_get_compression_methods();
		if (comp_methods != NULL && sk_SSL_COMP_num(comp_methods) > 0)
			options &= ~SSL_OP_TLS_BLOCK_PADDING_BUG;
	}
# endif
	SSL_CTX_set_options(*ctx, (long) options);

# if !NO_DH
	/* Diffie-Hellman initialization */
	if (bitset(TLS_I_TRY_DH, req))
	{
#  if TLS_EC == 1
		EC_KEY *ecdh;
#  endif

		if (tTd(96, 8))
			sm_dprintf("inittls: req=%#lx, status=%#lx\n",
				req, status);
		if (bitset(TLS_S_DHPAR_OK, status))
		{
			BIO *bio;

			if ((bio = BIO_new_file(dhparam, "r")) != NULL)
			{
				dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
				BIO_free(bio);
				if (dh == NULL && LogLevel > 7)
				{
					unsigned long err;

					err = ERR_get_error();
					sm_syslog(LOG_WARNING, NOQID,
						  "STARTTLS=%s, error: cannot read DH parameters(%s): %s",
						  who, dhparam,
						  ERR_error_string(err, NULL));
					tlslogerr(LOG_WARNING, 9, who);
					SET_DH_DFL;
				}
			}
			else
			{
				if (LogLevel > 5)
				{
					sm_syslog(LOG_WARNING, NOQID,
						  "STARTTLS=%s, error: BIO_new_file(%s) failed",
						  who, dhparam);
					tlslogerr(LOG_WARNING, 9, who);
				}
			}
		}
		if (dh == NULL && bitset(TLS_I_DH1024|TLS_I_DH2048, req))
		{
			int bits;
			DSA *dsa;

			bits = bitset(TLS_I_DH2048, req) ? 2048 : 1024;
			if (tTd(96, 2))
				sm_dprintf("inittls: Generating %d bit DH parameters\n", bits);

#  if MTA_HAVE_DSA_GENERATE_EX
			dsa = DSA_new();
			if (dsa != NULL)
			{
				r = DSA_generate_parameters_ex(dsa, bits, NULL,
							0, NULL, NULL, NULL);
				if (r != 0)
					dh = DSA_dup_DH(dsa);
			}
#  else
			/* this takes a while! */
			dsa = DSA_generate_parameters(bits, NULL, 0, NULL,
						      NULL, 0, NULL);
			dh = DSA_dup_DH(dsa);
#  endif
			DSA_free(dsa);
		}
		else if (dh == NULL && bitset(TLS_I_DHFIXED, req))
		{
			if (tTd(96, 2))
				sm_dprintf("inittls: Using precomputed 2048 bit DH parameters\n");
			dh = get_dh2048();
		}
		else if (dh == NULL && bitset(TLS_I_DH512, req))
		{
			if (tTd(96, 2))
				sm_dprintf("inittls: Using precomputed 512 bit DH parameters\n");
			dh = get_dh512();
		}

		if (dh == NULL)
		{
			if (LogLevel > 9)
			{
				unsigned long err;

				err = ERR_get_error();
				sm_syslog(LOG_WARNING, NOQID,
					  "STARTTLS=%s, error: cannot read or set DH parameters(%s): %s",
					  who, dhparam,
					  ERR_error_string(err, NULL));
			}
			if (bitset(TLS_I_REQ_DH, req))
				return false;
		}
		else
		{
			/* important to avoid small subgroup attacks */
			SSL_CTX_set_options(*ctx, SSL_OP_SINGLE_DH_USE);

			SSL_CTX_set_tmp_dh(*ctx, dh);
			if (LogLevel > 13)
				sm_syslog(LOG_INFO, NOQID,
					  "STARTTLS=%s, Diffie-Hellman init, key=%d bit (%c)",
					  who, 8 * DH_size(dh), *dhparam);
			DH_free(dh);
		}

#  if TLS_EC == 2
		SSL_CTX_set_options(*ctx, SSL_OP_SINGLE_ECDH_USE);
		SSL_CTX_set_ecdh_auto(*ctx, 1);
#  elif TLS_EC == 1
		ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
		if (ecdh != NULL)
		{
			SSL_CTX_set_options(*ctx, SSL_OP_SINGLE_ECDH_USE);
			SSL_CTX_set_tmp_ecdh(*ctx, ecdh);
			EC_KEY_free(ecdh);
		}
		else if (LogLevel > 9)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, EC_KEY_new_by_curve_name(NID_X9_62_prime256v1)=failed, error=%s",
				  who, ERR_error_string(ERR_get_error(), NULL));
		}
#  endif /* TLS_EC */

	}
# endif /* !NO_DH */


	/* XXX do we need this cache here? */
	if (bitset(TLS_I_CACHE, req))
	{
		SSL_CTX_sess_set_cache_size(*ctx, 1);
		SSL_CTX_set_timeout(*ctx, 1);
		SSL_CTX_set_session_id_context(*ctx,
			(void *) &server_session_id_context,
			sizeof(server_session_id_context));
		(void) SSL_CTX_set_session_cache_mode(*ctx,
				SSL_SESS_CACHE_SERVER);
	}
	else
	{
		(void) SSL_CTX_set_session_cache_mode(*ctx,
				SSL_SESS_CACHE_OFF);
	}

	/* load certificate locations and default CA paths */
	if (bitset(TLS_S_CERTP_EX, status) && bitset(TLS_S_CERTF_EX, status))
	{
		if ((r = SSL_CTX_load_verify_locations(*ctx, cacertfile,
						       cacertpath)) == 1)
		{
# if !TLS_NO_RSA && MTA_RSA_TMP_CB
			if (bitset(TLS_I_RSA_TMP, req))
				SSL_CTX_set_tmp_rsa_callback(*ctx, tmp_rsa_key);
# endif

			/*
			**  We have to install our own verify callback:
			**  SSL_VERIFY_PEER requests a client cert but even
			**  though *FAIL_IF* isn't set, the connection
			**  will be aborted if the client presents a cert
			**  that is not "liked" (can't be verified?) by
			**  the TLS library :-(
			*/

			/*
			**  XXX currently we could call tls_set_verify()
			**  but we hope that that function will later on
			**  only set the mode per connection.
			*/

			SSL_CTX_set_verify(*ctx,
				bitset(TLS_I_NO_VRFY, req) ? SSL_VERIFY_NONE
							   : SSL_VERIFY_PEER,
				NULL);

			if (srv)
			{
				SSL_CTX_set_client_CA_list(*ctx,
					SSL_load_client_CA_file(cacertfile));
			}
			SSL_CTX_set_cert_verify_callback(*ctx, tls_verify_cb,
							NULL);
		}
		else
		{
			/*
			**  can't load CA data; do we care?
			**  the data is necessary to authenticate the client,
			**  which in turn would be necessary
			**  if we want to allow relaying based on it.
			*/

			if (LogLevel > 5)
			{
				sm_syslog(LOG_WARNING, NOQID,
					  "STARTTLS=%s, error: load verify locs %s, %s failed: %d",
					  who, cacertpath, cacertfile, r);
				tlslogerr(LOG_WARNING,
					bitset(TLS_I_VRFY_LOC, req) ? 8 : 9,
					who);
			}
			if (bitset(TLS_I_VRFY_LOC, req))
				return false;
		}
	}

	/* XXX: make this dependent on an option? */
	if (tTd(96, 9))
		SSL_CTX_set_info_callback(*ctx, apps_ssl_info_cb);

	/* install our own cipher list */
	if (CipherList != NULL && *CipherList != '\0')
	{
		if (SSL_CTX_set_cipher_list(*ctx, CipherList) <= 0)
		{
			if (LogLevel > 7)
			{
				sm_syslog(LOG_WARNING, NOQID,
					  "STARTTLS=%s, error: SSL_CTX_set_cipher_list(%s) failed, list ignored",
					  who, CipherList);

				tlslogerr(LOG_WARNING, 9, who);
			}
			/* failure if setting to this list is required? */
		}
	}

	if (LogLevel > 12)
		sm_syslog(LOG_INFO, NOQID, "STARTTLS=%s, init=%d", who, ok);

# if 0
	/*
	**  this label is required if we want to have a "clean" exit
	**  see the comments above at the initialization of cf2
	*/

    endinittls:
# endif /* 0 */

	/* undo damage to global variables */
	if (cf2 != NULL)
		*--cf2 = ',';
	if (kf2 != NULL)
		*--kf2 = ',';

	return ok;
}

/*
**  CERT_FP -- get cert fingerprint
**
**	Parameters:
**		cert -- TLS cert
**		evp_digest -- digest algorithm
**		mac -- macro storage
**		macro -- where to store cert fp
**
**	Returns:
**		<=0: cert fp calculation failed
**		>0: cert fp calculation ok
*/

static int
cert_fp(cert, evp_digest, mac, macro)
	X509 *cert;
	const EVP_MD *evp_digest;
	MACROS_T *mac;
	char *macro;
{
	unsigned int n;
	int r;
	unsigned char md[EVP_MAX_MD_SIZE];
	char md5h[EVP_MAX_MD_SIZE * 3];
	static const char hexcodes[] = "0123456789ABCDEF";

	n = 0;
	if (X509_digest(cert, EVP_digest, md, &n) == 0 || n <= 0)
	{
		macdefine(mac, A_TEMP, macid(macro), "");
		return 0;
	}

	SM_ASSERT((n * 3) + 2 < sizeof(md5h));
	for (r = 0; r < (int) n; r++)
	{
		md5h[r * 3] = hexcodes[(md[r] & 0xf0) >> 4];
		md5h[(r * 3) + 1] = hexcodes[(md[r] & 0x0f)];
		md5h[(r * 3) + 2] = ':';
	}
	md5h[(n * 3) - 1] = '\0';
	macdefine(mac, A_TEMP, macid(macro), md5h);
	return 1;
}

/* host for logging */
#define whichhost	host == NULL ? "local" : host

# if _FFR_TLS_ALTNAMES

/*
**  CLEARCLASS -- clear the specified class (called from stabapply)
**
**	Parameters:
**		s -- STAB
**		id -- class id
**
**	Returns:
**		none.
*/

static void
clearclass(s, id)
	STAB *s;
	int id;
{
	if (s->s_symtype != ST_CLASS)
		return;
	if (bitnset(bitidx(id), s->s_class))
		clrbitn(bitidx(id), s->s_class);
}

/*
**  GETALTNAMES -- set subject_alt_name
**
**	Parameters:
**		cert -- cert
**		srv -- server side?
**		host -- hostname of other side
**
**	Returns:
**		none.
*/

static void
getaltnames(cert, srv, host)
	X509 *cert;
	bool srv;
	const char *host;
{
	STACK_OF(GENERAL_NAME) *gens;
	int i, j, len, r;
	const GENERAL_NAME *gn;
	char *dnsname, *who;

	if (!SetCertAltnames)
		return;
	who = srv ? "server" : "client";
	gens = X509_get_ext_d2i(cert, NID_subject_alt_name, 0, 0);
	if (gens == NULL)
		return;

	r = sk_GENERAL_NAME_num(gens);
	for (i = 0; i < r; i++)
	{
		gn = sk_GENERAL_NAME_value(gens, i);
		if (gn == NULL || gn->type != GEN_DNS)
			continue;

		/* Ensure data is IA5 */
		if (ASN1_STRING_type(gn->d.ia5) != V_ASN1_IA5STRING)
		{
			if (LogLevel > 6)
				sm_syslog(LOG_INFO, NOQID,
					"STARTTLS=%s, relay=%.100s, field=AltName, status=value contains non IA5",
					who, whichhost);
			continue;
		}
		dnsname = (char *) MTA_ASN1_STRING_data(gn->d.ia5);
		if (dnsname == NULL)
			continue;
		len = ASN1_STRING_length(gn->d.ia5);

		/*
		**  "remove" trailing NULs (except for one of course),
		**  those can happen and are OK (not a sign of an attack)
		*/

		while (len > 0 && '\0' == dnsname[len - 1])
			len--;

#define ISPRINT(c)	(isascii(c) && isprint(c))

		/* just check for printable char for now */
		for (j = 0; j < len && ISPRINT(dnsname[j]); j++)
			;
		if (dnsname[j] != '\0' || len != j)
			continue;

		setclass(macid("{cert_altnames}"), xtextify(dnsname, "<>\")"));
		if (LogLevel > 14)
			sm_syslog(LOG_DEBUG, NOQID,
				"STARTTLS=%s, relay=%.100s, AltName=%s",
				who, whichhost, xtextify(dnsname, "<>\")"));
	}
}
# else
#  define getaltnames(cert, srv, host)
# endif /* _FFR_TLS_ALTNAMES */

/*
**  TLS_GET_INFO -- get information about TLS connection
**
**	Parameters:
**		ssl -- TLS session context
**		srv -- server side?
**		host -- hostname of other side
**		mac -- macro storage
**		certreq -- did we ask for a cert?
**
**	Returns:
**		result of authentication.
**
**	Side Effects:
**		sets various TLS related macros.
*/

int
tls_get_info(ssl, srv, host, mac, certreq)
	SSL *ssl;
	bool srv;
	char *host;
	MACROS_T *mac;
	bool certreq;
{
	const SSL_CIPHER *c;
	int b, r;
	long verifyok;
	char *s, *who;
	char bitstr[16];
	X509 *cert;
# if DANE
	dane_vrfy_ctx_P dane_vrfy_ctx;
# endif

	c = SSL_get_current_cipher(ssl);

	/* cast is just workaround for compiler warning */
	macdefine(mac, A_TEMP, macid("{cipher}"),
		  (char *) SSL_CIPHER_get_name(c));
	b = SSL_CIPHER_get_bits(c, &r);
	(void) sm_snprintf(bitstr, sizeof(bitstr), "%d", b);
	macdefine(mac, A_TEMP, macid("{cipher_bits}"), bitstr);
	(void) sm_snprintf(bitstr, sizeof(bitstr), "%d", r);
	macdefine(mac, A_TEMP, macid("{alg_bits}"), bitstr);
	s = (char *) SSL_get_version(ssl);
	if (s == NULL)
		s = "UNKNOWN";
	macdefine(mac, A_TEMP, macid("{tls_version}"), s);

	who = srv ? "server" : "client";
	cert = SSL_get_peer_certificate(ssl);
	verifyok = SSL_get_verify_result(ssl);
	if (LogLevel > 14)
		sm_syslog(LOG_INFO, NOQID,
			  "STARTTLS=%s, get_verify: %ld get_peer: 0x%lx",
			  who, verifyok, (unsigned long) cert);
# if _FFR_TLS_ALTNAMES
	stabapply(clearclass, macid("{cert_altnames}"));
# endif
	if (cert != NULL)
	{
		X509_NAME *subj, *issuer;
		char buf[MAXNAME];

		subj = X509_get_subject_name(cert);
		issuer = X509_get_issuer_name(cert);
		X509_NAME_oneline(subj, buf, sizeof(buf));
		macdefine(mac, A_TEMP, macid("{cert_subject}"),
			 xtextify(buf, "<>\")"));
		X509_NAME_oneline(issuer, buf, sizeof(buf));
		macdefine(mac, A_TEMP, macid("{cert_issuer}"),
			 xtextify(buf, "<>\")"));

#  define LL_BADCERT	8

#define CERTFPMACRO (CertFingerprintAlgorithm != NULL ? "{cert_fp}" : "{cert_md5}")

#define CHECK_X509_NAME(which)	\
	do {	\
		if (r == -1)	\
		{		\
			sm_strlcpy(buf, "BadCertificateUnknown", sizeof(buf)); \
			if (LogLevel > LL_BADCERT)	\
				sm_syslog(LOG_INFO, NOQID,	\
					"STARTTLS=%s, relay=%.100s, field=%s, status=failed to extract CN",	\
					who, whichhost,	which);	\
		}		\
		else if ((size_t)r >= sizeof(buf) - 1)	\
		{		\
			sm_strlcpy(buf, "BadCertificateTooLong", sizeof(buf)); \
			if (LogLevel > 7)	\
				sm_syslog(LOG_INFO, NOQID,	\
					"STARTTLS=%s, relay=%.100s, field=%s, status=CN too long",	\
					who, whichhost, which);	\
		}		\
		else if ((size_t)r > strlen(buf))	\
		{		\
			sm_strlcpy(buf, "BadCertificateContainsNUL",	\
				sizeof(buf));	\
			if (LogLevel > 7)	\
				sm_syslog(LOG_INFO, NOQID,	\
					"STARTTLS=%s, relay=%.100s, field=%s, status=CN contains NUL",	\
					who, whichhost, which);	\
		}		\
	} while (0)

		r = X509_NAME_get_text_by_NID(subj, NID_commonName, buf,
			sizeof buf);
		CHECK_X509_NAME("cn_subject");
		macdefine(mac, A_TEMP, macid("{cn_subject}"),
			 xtextify(buf, "<>\")"));
		r = X509_NAME_get_text_by_NID(issuer, NID_commonName, buf,
			sizeof buf);
		CHECK_X509_NAME("cn_issuer");
		macdefine(mac, A_TEMP, macid("{cn_issuer}"),
			 xtextify(buf, "<>\")"));
		(void) cert_fp(cert, EVP_digest, mac, CERTFPMACRO);
		getaltnames(cert, srv, host);
	}
	else
	{
		macdefine(mac, A_PERM, macid("{cert_subject}"), "");
		macdefine(mac, A_PERM, macid("{cert_issuer}"), "");
		macdefine(mac, A_PERM, macid("{cn_subject}"), "");
		macdefine(mac, A_PERM, macid("{cn_issuer}"), "");
		macdefine(mac, A_TEMP, macid(CERTFPMACRO), "");
	}
# if DANE
	dane_vrfy_ctx = NULL;
	if (TLSsslidx >= 0)
	{
		tlsi_ctx_T *tlsi_ctx;

		tlsi_ctx = (tlsi_ctx_P) SSL_get_ex_data(ssl, TLSsslidx);
		if (tlsi_ctx != NULL)
			dane_vrfy_ctx = &(tlsi_ctx->tlsi_dvc);
	}
#  define DANE_VRFY_RES_IS(r) \
	((dane_vrfy_ctx != NULL) && dane_vrfy_ctx->dane_vrfy_res == (r))
	if (DANE_VRFY_RES_IS(DANE_VRFY_OK))
	{
		s = "TRUSTED";
		r = TLS_AUTH_OK;
	}
	else if (DANE_VRFY_RES_IS(DANE_VRFY_FAIL))
	{
		s = "DANE_FAIL";
		r = TLS_AUTH_FAIL;
	}
	else
# endif /* if DANE */
	switch (verifyok)
	{
	  case X509_V_OK:
		if (cert != NULL)
		{
			s = "OK";
			r = TLS_AUTH_OK;
		}
		else
		{
			s = certreq ? "NO" : "NOT",
			r = TLS_AUTH_NO;
		}
		break;
	  default:
		s = "FAIL";
		r = TLS_AUTH_FAIL;
		break;
	}
	macdefine(mac, A_PERM, macid("{verify}"), s);
	if (cert != NULL)
		X509_free(cert);

	/* do some logging */
	if (LogLevel > 8)
	{
		char *vers, *s1, *s2, *cbits, *algbits;

		vers = macget(mac, macid("{tls_version}"));
		cbits = macget(mac, macid("{cipher_bits}"));
		algbits = macget(mac, macid("{alg_bits}"));
		s1 = macget(mac, macid("{verify}"));
		s2 = macget(mac, macid("{cipher}"));

# if DANE
#  define LOG_DANE_FP	\
	('\0' != dane_vrfy_ctx->dane_vrfy_fp[0] && DANE_VRFY_RES_IS(DANE_VRFY_FAIL))
# endif
		/* XXX: maybe cut off ident info? */
		sm_syslog(LOG_INFO, NOQID,
			  "STARTTLS=%s, relay=%.100s, version=%.16s, verify=%.16s, cipher=%.64s, bits=%.6s/%.6s%s%s",
			  who,
			  host == NULL ? "local" : host,
			  vers, s1, s2, /* sm_snprintf() can deal with NULL */
			  algbits == NULL ? "0" : algbits,
			  cbits == NULL ? "0" : cbits
# if DANE
			, LOG_DANE_FP ? ", pubkey_fp=" : ""
			, LOG_DANE_FP ? dane_vrfy_ctx->dane_vrfy_fp : ""
# else
			, "", ""
# endif
			);
		if (LogLevel > 11)
		{
			/*
			**  Maybe run xuntextify on the strings?
			**  That is easier to read but makes it maybe a bit
			**  more complicated to figure out the right values
			**  for the access map...
			*/

			s1 = macget(mac, macid("{cert_subject}"));
			s2 = macget(mac, macid("{cert_issuer}"));
			sm_syslog(LOG_INFO, NOQID,
				  "STARTTLS=%s, cert-subject=%.256s, cert-issuer=%.256s, verifymsg=%s",
				  who, s1, s2,
				  X509_verify_cert_error_string(verifyok));
		}
	}
	return r;
}

/*
**  ENDTLS -- shutdown secure connection
**
**	Parameters:
**		pssl -- pointer to TLS session context
**		who -- server/client (for logging).
**
**	Returns:
**		success? (EX_* code)
*/

int
endtls(pssl, who)
	SSL **pssl;
	const char *who;
{
	SSL *ssl;
	int ret, r;

	SM_REQUIRE(pssl != NULL);
	ret = EX_OK;
	ssl = *pssl;
	if (ssl == NULL)
		return ret;

	if ((r = SSL_shutdown(ssl)) < 0)
	{
		if (LogLevel > 11)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, SSL_shutdown failed: %d",
				  who, r);
			tlslogerr(LOG_WARNING, 11, who);
		}
		ret = EX_SOFTWARE;
	}

	/*
	**  Bug in OpenSSL (at least up to 0.9.6b):
	**  From: Lutz.Jaenicke@aet.TU-Cottbus.DE
	**  Message-ID: <20010723152244.A13122@serv01.aet.tu-cottbus.de>
	**  To: openssl-users@openssl.org
	**  Subject: Re: SSL_shutdown() woes (fwd)
	**
	**  The side sending the shutdown alert first will
	**  not care about the answer of the peer but will
	**  immediately return with a return value of "0"
	**  (ssl/s3_lib.c:ssl3_shutdown()). SSL_get_error will evaluate
	**  the value of "0" and as the shutdown alert of the peer was
	**  not received (actually, the program did not even wait for
	**  the answer), an SSL_ERROR_SYSCALL is flagged, because this
	**  is the default rule in case everything else does not apply.
	**
	**  For your server the problem is different, because it
	**  receives the shutdown first (setting SSL_RECEIVED_SHUTDOWN),
	**  then sends its response (SSL_SENT_SHUTDOWN), so for the
	**  server the shutdown was successful.
	**
	**  As is by know, you would have to call SSL_shutdown() once
	**  and ignore an SSL_ERROR_SYSCALL returned. Then call
	**  SSL_shutdown() again to actually get the server's response.
	**
	**  In the last discussion, Bodo Moeller concluded that a
	**  rewrite of the shutdown code would be necessary, but
	**  probably with another API, as the change would not be
	**  compatible to the way it is now.  Things do not become
	**  easier as other programs do not follow the shutdown
	**  guidelines anyway, so that a lot error conditions and
	**  compitibility issues would have to be caught.
	**
	**  For now the recommondation is to ignore the error message.
	*/

	else if (r == 0)
	{
		if (LogLevel > 15)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=%s, SSL_shutdown not done",
				  who);
			tlslogerr(LOG_WARNING, 15, who);
		}
		ret = EX_SOFTWARE;
	}
	SM_SSL_FREE(*pssl);
	return ret;
}

# if !TLS_NO_RSA && MTA_RSA_TMP_CB
/*
**  TMP_RSA_KEY -- return temporary RSA key
**
**	Parameters:
**		ssl -- TLS session context
**		export --
**		keylength --
**
**	Returns:
**		temporary RSA key.
*/

#  ifndef MAX_RSA_TMP_CNT
#   define MAX_RSA_TMP_CNT	1000	/* XXX better value? */
#  endif

/* ARGUSED0 */
static RSA *
tmp_rsa_key(s, export, keylength)
	SSL *s;
	int export;
	int keylength;
{
#  if SM_CONF_SHM
	extern int ShmId;
	extern int *PRSATmpCnt;

	if (ShmId != SM_SHM_NO_ID && rsa_tmp != NULL &&
	    ++(*PRSATmpCnt) < MAX_RSA_TMP_CNT)
		return rsa_tmp;
#  endif /* SM_CONF_SHM */

	if (rsa_tmp != NULL)
		RSA_free(rsa_tmp);
	rsa_tmp = RSA_generate_key(RSA_KEYLENGTH, RSA_F4, NULL, NULL);
	if (rsa_tmp == NULL)
	{
		if (LogLevel > 0)
			sm_syslog(LOG_ERR, NOQID,
				  "STARTTLS=server, tmp_rsa_key: RSA_generate_key failed!");
	}
	else
	{
#  if SM_CONF_SHM
#   if 0
		/*
		**  XXX we can't (yet) share the new key...
		**	The RSA structure contains pointers hence it can't be
		**	easily kept in shared memory.  It must be transformed
		**	into a continuous memory region first, then stored,
		**	and later read out again (each time re-transformed).
		*/

		if (ShmId != SM_SHM_NO_ID)
			*PRSATmpCnt = 0;
#   endif /* 0 */
#  endif /* SM_CONF_SHM */
		if (LogLevel > 9)
			sm_syslog(LOG_ERR, NOQID,
				  "STARTTLS=server, tmp_rsa_key: new temp RSA key");
	}
	return rsa_tmp;
}
# endif /* !TLS_NO_RSA && MTA_RSA_TMP_CB */

/*
**  APPS_SSL_INFO_CB -- info callback for TLS connections
**
**	Parameters:
**		ssl -- TLS session context
**		where -- state in handshake
**		ret -- return code of last operation
**
**	Returns:
**		none.
*/

static void
apps_ssl_info_cb(ssl, where, ret)
	const SSL *ssl;
	int where;
	int ret;
{
	int w;
	char *str;
	BIO *bio_err = NULL;

	if (LogLevel > 14)
		sm_syslog(LOG_INFO, NOQID,
			  "STARTTLS: info_callback where=0x%x, ret=%d",
			  where, ret);

	w = where & ~SSL_ST_MASK;
	if (bio_err == NULL)
		bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);

	if (bitset(SSL_ST_CONNECT, w))
		str = "SSL_connect";
	else if (bitset(SSL_ST_ACCEPT, w))
		str = "SSL_accept";
	else
		str = "undefined";

	if (bitset(SSL_CB_LOOP, where))
	{
		if (LogLevel > 12)
			sm_syslog(LOG_NOTICE, NOQID,
				"STARTTLS: %s:%s",
				str, SSL_state_string_long(ssl));
	}
	else if (bitset(SSL_CB_ALERT, where))
	{
		str = bitset(SSL_CB_READ, where) ? "read" : "write";
		if (LogLevel > 12)
			sm_syslog(LOG_NOTICE, NOQID,
				"STARTTLS: SSL3 alert %s:%s:%s",
				str, SSL_alert_type_string_long(ret),
				SSL_alert_desc_string_long(ret));
	}
	else if (bitset(SSL_CB_EXIT, where))
	{
		if (ret == 0)
		{
			if (LogLevel > 7)
				sm_syslog(LOG_WARNING, NOQID,
					"STARTTLS: %s:failed in %s",
					str, SSL_state_string_long(ssl));
		}
		else if (ret < 0)
		{
			if (LogLevel > 7)
				sm_syslog(LOG_WARNING, NOQID,
					"STARTTLS: %s:error in %s",
					str, SSL_state_string_long(ssl));
		}
	}
}

/*
**  TLS_VERIFY_LOG -- log verify error for TLS certificates
**
**	Parameters:
**		ok -- verify ok?
**		ctx -- X509 context
**		name -- from where is this called?
**
**	Returns:
**		1 -- ok
*/

static int
tls_verify_log(ok, ctx, name)
	int ok;
	X509_STORE_CTX *ctx;
	const char *name;
{
	X509 *cert;
	int reason, depth;
	char buf[512];

	cert = X509_STORE_CTX_get_current_cert(ctx);
	reason = X509_STORE_CTX_get_error(ctx);
	depth = X509_STORE_CTX_get_error_depth(ctx);
	X509_NAME_oneline(X509_get_subject_name(cert), buf, sizeof(buf));
	sm_syslog(LOG_INFO, NOQID,
		  "STARTTLS: %s cert verify: depth=%d %s, state=%d, reason=%s",
		  name, depth, buf, ok, X509_verify_cert_error_string(reason));
	return 1;
}

/*
**  Declaration and access to tlsi_ctx in callbacks.
**  Currently only used in one of them.
*/

#define SM_DECTLSI	\
	tlsi_ctx_T *tlsi_ctx;	\
	SSL *ssl
#define SM_GETTLSI	\
	do {		\
		tlsi_ctx = NULL;	\
		if (TLSsslidx >= 0)	\
		{	\
			ssl = (SSL *) X509_STORE_CTX_get_ex_data(ctx,	\
				SSL_get_ex_data_X509_STORE_CTX_idx());	\
			if (ssl != NULL)	\
				tlsi_ctx = (tlsi_ctx_P) SSL_get_ex_data(ssl, TLSsslidx);	\
		}	\
	}	\
	while (0)


# if DANE

/*
**  DANE_GET_TLSA -- Retrieve TLSA RR for DANE
**
**	Parameters:
**		dane -- dane verify context
**
**	Returns:
**		dane_tlsa if TLSA RR is available
**		NULL otherwise
*/

dane_tlsa_P
dane_get_tlsa(dane_vrfy_ctx)
	dane_vrfy_ctx_P dane_vrfy_ctx;
{
	STAB *s;
	dane_tlsa_P dane_tlsa;

	dane_tlsa = NULL;
	if (NULL == dane_vrfy_ctx)
		return NULL;
	if (dane_vrfy_ctx->dane_vrfy_chk == DANE_NEVER ||
	    dane_vrfy_ctx->dane_vrfy_host == NULL)
		return NULL;

	GETTLSANOX(dane_vrfy_ctx->dane_vrfy_host, &s,
		dane_vrfy_ctx->dane_vrfy_port);
	if (NULL == s)
		goto notfound;
	dane_tlsa = s->s_tlsa;
	if (NULL == dane_tlsa)
		goto notfound;
	if (0 == dane_tlsa->dane_tlsa_n)
		goto notfound;
	if (tTd(96, 4))
		sm_dprintf("dane_get_tlsa, chk=%d, host=%s, n=%d, stat=entry found\n",
			dane_vrfy_ctx->dane_vrfy_chk,
			dane_vrfy_ctx->dane_vrfy_host, dane_tlsa->dane_tlsa_n);
	return dane_tlsa;

  notfound:
	if (tTd(96, 4))
		sm_dprintf("dane_get_tlsa, chk=%d, host=%s, stat=no valid entry found\n",
			dane_vrfy_ctx->dane_vrfy_chk,
			dane_vrfy_ctx->dane_vrfy_host);
	return NULL;
}

/*
**  DANE_VERIFY -- verify callback for TLS certificates
**
**	Parameters:
**		ctx -- X509 context
**		dane_vrfy_ctx -- callback context
**
**	Returns:
**		DANE_VRFY_{OK,NONE,FAIL}
*/

/* NOTE: this only works because the "matching type" is 0, 1, 2 for these! */
static const char *dane_mdalgs[] = { "", "sha256", "sha512" };

static int
dane_verify(ctx, dane_vrfy_ctx)
	X509_STORE_CTX *ctx;
	dane_vrfy_ctx_P dane_vrfy_ctx;
{
	int r, i, ok, mdalg;
	X509 *cert;
	dane_tlsa_P dane_tlsa;
	char *fp;

	dane_tlsa = dane_get_tlsa(dane_vrfy_ctx);
	if (dane_tlsa == NULL)
		return DANE_VRFY_NONE;

	dane_vrfy_ctx->dane_vrfy_fp[0] = '\0';
	cert = X509_STORE_CTX_get0_cert(ctx);
	if (tTd(96, 8))
		sm_dprintf("dane_verify, cert=%p\n", (void *)cert);
	if (cert == NULL)
		return DANE_VRFY_FAIL;

	ok = DANE_VRFY_NONE;
	fp = NULL;

	/*
	**  If the TLSA RRs would be sorted the two loops below could
	**  be merged into one and simply change mdalg when it changes
	**  in dane_tlsa->dane_tlsa_rr.
	*/

	/* use a different order? */
	for (mdalg = 0; mdalg < SM_ARRAY_SIZE(dane_mdalgs); mdalg++)
	{
		SM_FREE(fp);
		r = 0;
		for (i = 0; i < dane_tlsa->dane_tlsa_n; i++)
		{
			char *p;
			int alg;

			p = dane_tlsa->dane_tlsa_rr[i];

			/* ignore bogus/unsupported TLSA RRs */
			alg = dane_tlsa_chk(p, dane_tlsa->dane_tlsa_len[i],
					  dane_vrfy_ctx->dane_vrfy_host, false);
			if (tTd(96, 8))
				sm_dprintf("dane_verify, alg=%d, mdalg=%d\n",
					alg, mdalg);
			if (alg != mdalg)
				continue;

			if (NULL == fp)
			{
				r = pubkey_fp(cert, dane_mdalgs[mdalg], &fp);
				if (NULL == fp)
					return DANE_VRFY_FAIL;
					/* or continue? */
			}

			/* just for logging */
			if (r > 0 && fp != NULL)
			{
				(void) data2hex((unsigned char *)fp, r,
					(unsigned char *)dane_vrfy_ctx->dane_vrfy_fp,
					sizeof(dane_vrfy_ctx->dane_vrfy_fp));
			}

			if (tTd(96, 4))
				sm_dprintf("dane_verify, alg=%d, r=%d, len=%d\n",
					alg, r, dane_tlsa->dane_tlsa_len[i]);
			if (r != dane_tlsa->dane_tlsa_len[i] - 3)
				continue;
			ok = DANE_VRFY_FAIL;

			/*
			**  Note: Type is NOT checked because only 3-1-x
			**  is supported.
			*/

			if (memcmp(p + 3, fp, r) == 0)
			{
				if (tTd(96, 2))
					sm_dprintf("dane_verify, status=match\n");
				if (tTd(96, 8))
				{
					unsigned char hex[256];

					data2hex((unsigned char *)p,
						dane_tlsa->dane_tlsa_len[i],
						hex, sizeof(hex));
					sm_dprintf("dane_verify, pubkey_fp=%s\n"
						, hex);
				}
				dane_vrfy_ctx->dane_vrfy_res = DANE_VRFY_OK;
				SM_FREE(fp);
				return DANE_VRFY_OK;
			}
		}
	}

	SM_FREE(fp);
	dane_vrfy_ctx->dane_vrfy_res = ok;
	return ok;
}
# endif /* DANE */

/*
**  TLS_VERIFY_CB -- verify callback for TLS certificates
**
**	Parameters:
**		ctx -- X509 context
**		cb_ctx -- callback context
**
**	Returns:
**		accept connection?
**		currently: always yes.
*/

static int
tls_verify_cb(ctx, cb_ctx)
	X509_STORE_CTX *ctx;
	void *cb_ctx;
{
	int ok;
# if DANE
	SM_DECTLSI;
# endif

	/*
	**  SSL_CTX_set_cert_verify_callback(3):
	**  callback should return 1 to indicate verification success
	**  and 0 to indicate verification failure.
	*/

# if DANE
	SM_GETTLSI;
	if (tlsi_ctx != NULL)
	{
		dane_vrfy_ctx_P dane_vrfy_ctx;

		dane_vrfy_ctx = &(tlsi_ctx->tlsi_dvc);
		ok = dane_verify(ctx, dane_vrfy_ctx);
		if (tTd(96, 2))
			sm_dprintf("dane_verify=%d, res=%d\n", ok,
				dane_vrfy_ctx->dane_vrfy_res);
		if (ok != DANE_VRFY_NONE)
			return 1;
	}
# endif /* DANE */

	ok = X509_verify_cert(ctx);
	if (ok <= 0)
	{
		if (LogLevel > 13)
			return tls_verify_log(ok, ctx, "TLS");
	}
	else if (LogLevel > 14)
		(void) tls_verify_log(ok, ctx, "TLS");
	return 1;
}

/*
**  TLSLOGERR -- log the errors from the TLS error stack
**
**	Parameters:
**		priority -- syslog priority
**		ll -- loglevel
**		who -- server/client (for logging).
**
**	Returns:
**		none.
*/

void
tlslogerr(priority, ll, who)
	int priority;
	int ll;
	const char *who;
{
	unsigned long l;
	int line, flags;
	char *file, *data;
	char buf[256];

	if (LogLevel <= ll)
		return;
	while ((l = ERR_get_error_line_data((const char **) &file, &line,
					    (const char **) &data, &flags))
		!= 0)
	{
		sm_syslog(priority, NOQID,
			  "STARTTLS=%s: %s:%s:%d:%s", who,
			  ERR_error_string(l, buf),
			  file, line,
			  bitset(ERR_TXT_STRING, flags) ? data : "");
	}
}

/*
**  X509_VERIFY_CB -- verify callback
**
**	Parameters:
**		ok -- current result
**		ctx -- X509 context
**
**	Returns:
**		accept connection?
**		currently: always yes.
*/

static int
x509_verify_cb(ok, ctx)
	int ok;
	X509_STORE_CTX *ctx;
{
	SM_DECTLSI;

	if (ok != 0)
		return ok;

	SM_GETTLSI;
	if (LogLevel > 13)
		tls_verify_log(ok, ctx, "X509");
	if (X509_STORE_CTX_get_error(ctx) == X509_V_ERR_UNABLE_TO_GET_CRL &&
	    !SM_TLSI_IS(tlsi_ctx, TLSI_FL_CRLREQ))
	{
		X509_STORE_CTX_set_error(ctx, 0);
		return 1;	/* override it */
	}
	return ok;
}

# if !USE_OPENSSL_ENGINE
/*
**  TLS_SET_ENGINE -- set up ENGINE if needed
**
**	Parameters:
**		id -- id for ENGINE
**		isprefork -- called before fork()?
**
**	Returns: (OpenSSL "semantics", reverse it to allow returning error codes)
**		0: failure
**		!=0: ok
*/

int
TLS_set_engine(id, isprefork)
	const char *id;
	bool isprefork;
{
	static bool TLSEngineInitialized = false;
	ENGINE *e;
	char enginepath[MAXPATHLEN];

	/*
	**  Todo: put error for logging into a string and log it in error:
	*/

	if (LogLevel > 13)
		sm_syslog(LOG_DEBUG, NOQID,
			"engine=%s, path=%s, ispre=%d, pre=%d, initialized=%d",
			id, SSLEnginePath, isprefork, SSLEngineprefork,
			TLSEngineInitialized);
	if (TLSEngineInitialized)
		return 1;
	if (id == NULL || *id == '\0')
		return 1;

	/* is this the "right time" to initialize the engine? */
	if (isprefork != SSLEngineprefork)
		return 1;

	e = NULL;
	ENGINE_load_builtin_engines();

	if (SSLEnginePath != NULL && *SSLEnginePath != '\0')
	{
		if ((e = ENGINE_by_id("dynamic")) == NULL)
		{
			if (LogLevel > 1)
				sm_syslog(LOG_ERR, NOQID,
					"engine=%s, by_id=failed", "dynamic");
			goto error;
		}
		(void) sm_snprintf(enginepath, sizeof(enginepath),
			"%s/lib%s.so", SSLEnginePath, id);

		if (!ENGINE_ctrl_cmd_string(e, "SO_PATH", enginepath, 0))
		{
			if (LogLevel > 1)
				sm_syslog(LOG_ERR, NOQID,
					"engine=%s, SO_PATH=%s, status=failed",
					id, enginepath);
			goto error;
		}

		if (!ENGINE_ctrl_cmd_string(e, "ID", id, 0))
		{
			if (LogLevel > 1)
				sm_syslog(LOG_ERR, NOQID,
					"engine=%s, ID=failed", id);
			goto error;
		}

		if (!ENGINE_ctrl_cmd_string(e, "LOAD", NULL, 0))
		{
			if (LogLevel > 1)
				sm_syslog(LOG_ERR, NOQID,
					"engine=%s, LOAD=failed", id);
			goto error;
		}
	}
	else if ((e = ENGINE_by_id(id)) == NULL)
	{
		if (LogLevel > 1)
			sm_syslog(LOG_ERR, NOQID, "engine=%s, by_id=failed",
				id);
		return 0;
	}

	if (!ENGINE_init(e))
	{
		if (LogLevel > 1)
			sm_syslog(LOG_ERR, NOQID, "engine=%s, init=failed", id);
		goto error;
	}
	if (!ENGINE_set_default(e, ENGINE_METHOD_ALL))
	{
		if (LogLevel > 1)
			sm_syslog(LOG_ERR, NOQID,
				"engine=%s, set_default=failed", id);
		goto error;
	}
#  ifdef ENGINE_CTRL_CHIL_SET_FORKCHECK
	if (strcmp(id, "chil") == 0)
		ENGINE_ctrl(e, ENGINE_CTRL_CHIL_SET_FORKCHECK, 1, 0, 0);
#  endif

	/* Free our "structural" reference. */
	ENGINE_free(e);
	if (LogLevel > 10)
		sm_syslog(LOG_INFO, NOQID, "engine=%s, loaded=ok", id);
	TLSEngineInitialized = true;
	return 1;

  error:
	tlslogerr(LOG_WARNING, 7, "init");
	if (e != NULL)
		ENGINE_free(e);
	return 0;
}
# endif /* !USE_OPENSSL_ENGINE */
#endif /* STARTTLS */
