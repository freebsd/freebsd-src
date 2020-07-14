/*
 * Copyright (c) 2015 Proofpoint, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */


#ifndef _TLS_H
# define _TLS_H 1


#if STARTTLS
# include <openssl/ssl.h>
# if !TLS_NO_RSA
#  if _FFR_FIPSMODE
#   define RSA_KEYLENGTH	1024
#  else
#   define RSA_KEYLENGTH	512
#  endif
# endif /* !TLS_NO_RSA */

# if OPENSSL_VERSION_NUMBER >= 0x10100000L && OPENSSL_VERSION_NUMBER < 0x20000000L
#  define TLS_version_num OpenSSL_version_num
# else
#  define TLS_version_num SSLeay
# endif

#ifdef _DEFINE
# define EXTERN
#else
# define EXTERN extern
#endif

#if _FFR_TLS_EC && !defined(TLS_EC)
# define TLS_EC _FFR_TLS_EC
#endif

#if DANE
extern int gettlsa __P((char *, char *, STAB **, unsigned long, unsigned int, unsigned int));
# define MAX_TLSA_RR	8

# define DANE_VRFY_NONE	0	/* no TLSAs */
# define DANE_VRFY_OK		1	/* TLSA check was ok */
# define DANE_VRFY_FAIL	(-1)	/* TLSA check failed */

/* return values for dane_tlsa_chk() */
# define TLSA_BOGUS	(-10)
# define TLSA_UNSUPP	(-1)
/* note: anything >= 0 is ok and refers to the hash algorithm */
# define TLSA_IS_KNOWN(r)	((r) >= 0)
# define TLSA_IS_VALID(r)	((r) >= TLSA_UNSUPP)

struct dane_tlsa_S
{
	time_t		 dane_tlsa_exp;
	int		 dane_tlsa_n;
	int		 dane_tlsa_dnsrc;
	unsigned long	 dane_tlsa_flags;
	unsigned char	 dane_tlsa_usage[MAX_TLSA_RR];
	unsigned char	 dane_tlsa_selector[MAX_TLSA_RR];
	unsigned char	 dane_tlsa_digest[MAX_TLSA_RR];
	void		*dane_tlsa_rr[MAX_TLSA_RR];
	int		 dane_tlsa_len[MAX_TLSA_RR];
	char		*dane_tlsa_sni;
};

# define TLSAFLNONE	0x00000000	/* currently unused */
/* Dane Mode */
# define TLSAFLALWAYS	0x00000001
# define TLSAFLSECURE	0x00000002
# define DANEMODE(fl)	((fl) & 0x3)
# define TLSAFLNOEXP	0x00000010	/* do not check expiration */

# define TLSAFLADMX	0x00000100
# define TLSAFLADTLSA	0x00000200	/* currently unused */

/* could be used to replace DNSRC */
# define TLSAFLTEMP	0x00001000
/* no TLSA? -- _n == 0 */
# define TLSAFLNOTLSA	0x00002000	/* currently unused */

/*
**  Do not use this record, and do not look up new TLSA RRs because
**  the MX/host lookup was not secure.
**  XXX: to determine: interaction with DANE=always
*/

# define TLSAFLNOADMX	0x00010000
# define TLSAFLNOADTLSA	0x00020000	/* TLSA: no AD - for DANE=always? */

# define TLSA_SET_FL(dane_tlsa, fl)	(dane_tlsa)->dane_tlsa_flags |= (fl)
# define TLSA_CLR_FL(dane_tlsa, fl)	(dane_tlsa)->dane_tlsa_flags &= ~(fl)
# define TLSA_IS_FL(dane_tlsa, fl)	((dane_tlsa)->dane_tlsa_flags & (fl))
# define TLSA_STORE_FL(fl)	((fl) >= TLSAFLTEMP)

# define GETTLSA(host, pste, port)	gettlsa(host, NULL, pste, TLSAFLNONE, 0, port)
# define GETTLSANOX(host, pste, port)	gettlsa(host, NULL, pste, TLSAFLNOEXP, 0, port)

/* values for DANE option and dane_vrfy_chk */
# define DANE_NEVER	TLSAFLNONE
# define DANE_ALWAYS	TLSAFLALWAYS		/* NOT documented, testing... */
# define DANE_SECURE	TLSAFLSECURE
# define CHK_DANE(dane)	((dane) != DANE_NEVER)

/* temp fails? others? */
# define TLSA_RR_TEMPFAIL(dane_tlsa) (((dane_tlsa) != NULL) && (dane_tlsa)->dane_tlsa_dnsrc == TRY_AGAIN)

#endif /* DANE */

/*
**  TLS
*/

/* what to do in the TLS initialization */
#define TLS_I_NONE	0x00000000	/* no requirements... */
#define TLS_I_CERT_EX	0x00000001	/* cert must exist */
#define TLS_I_CERT_UNR	0x00000002	/* cert must be g/o unreadable */
#define TLS_I_KEY_EX	0x00000004	/* key must exist */
#define TLS_I_KEY_UNR	0x00000008	/* key must be g/o unreadable */
#define TLS_I_CERTP_EX	0x00000010	/* CA cert path must exist */
#define TLS_I_CERTP_UNR	0x00000020	/* CA cert path must be g/o unreadable */
#define TLS_I_CERTF_EX	0x00000040	/* CA cert file must exist */
#define TLS_I_CERTF_UNR	0x00000080	/* CA cert file must be g/o unreadable */
#define TLS_I_RSA_TMP	0x00000100	/* RSA TMP must be generated */
#define TLS_I_USE_KEY	0x00000200	/* private key must usable */
#define TLS_I_USE_CERT	0x00000400	/* certificate must be usable */
#define TLS_I_VRFY_PATH	0x00000800	/* load verify path must succeed */
#define TLS_I_VRFY_LOC	0x00001000	/* load verify default must succeed */
#define TLS_I_CACHE	0x00002000	/* require cache */
#define TLS_I_TRY_DH	0x00004000	/* try DH certificate */
#define TLS_I_REQ_DH	0x00008000	/* require DH certificate */
#define TLS_I_DHPAR_EX	0x00010000	/* require DH parameters */
#define TLS_I_DHPAR_UNR	0x00020000	/* DH param. must be g/o unreadable */
#define TLS_I_DH512	0x00040000	/* generate 512bit DH param */
#define TLS_I_DH1024	0x00080000	/* generate 1024bit DH param */
#define TLS_I_DH2048	0x00100000	/* generate 2048bit DH param */
#define TLS_I_NO_VRFY	0x00200000	/* do not require authentication */
#define TLS_I_KEY_OUNR	0x00400000	/* Key must be other unreadable */
#define TLS_I_CRLF_EX	0x00800000	/* CRL file must exist */
#define TLS_I_CRLF_UNR	0x01000000	/* CRL file must be g/o unreadable */
#define TLS_I_DHFIXED	0x02000000	/* use fixed DH param */

/* require server cert */
#define TLS_I_SRV_CERT	 (TLS_I_CERT_EX | TLS_I_KEY_EX | \
			  TLS_I_KEY_UNR | TLS_I_KEY_OUNR | \
			  TLS_I_CERTP_EX | TLS_I_CERTF_EX | \
			  TLS_I_USE_KEY | TLS_I_USE_CERT | TLS_I_CACHE)

/* server requirements */
#define TLS_I_SRV	(TLS_I_SRV_CERT | TLS_I_RSA_TMP | TLS_I_VRFY_PATH | \
			 TLS_I_VRFY_LOC | TLS_I_TRY_DH | TLS_I_CACHE)

/* client requirements */
#define TLS_I_CLT	(TLS_I_KEY_UNR | TLS_I_KEY_OUNR)

#define TLS_AUTH_OK	0
#define TLS_AUTH_NO	1
#define TLS_AUTH_FAIL	(-1)

# ifndef TLS_VRFY_PER_CTX
#  define TLS_VRFY_PER_CTX 1
# endif

#define SM_SSL_FREE(ssl)			\
	do {					\
		if (ssl != NULL)		\
		{				\
			SSL_free(ssl);		\
			ssl = NULL;		\
		}				\
	} while (0)

/* functions */
extern int	endtls __P((SSL **, const char *));
extern int	get_tls_se_options __P((ENVELOPE *, SSL *, tlsi_ctx_T *, bool));
extern int	init_tls_library __P((bool _fipsmode));
extern bool	inittls __P((SSL_CTX **, unsigned long, unsigned long, bool, char *, char *, char *, char *, char *));
extern bool	initclttls __P((bool));
extern bool	initsrvtls __P((bool));
extern bool	load_certkey __P((SSL *, bool, char *, char *));
/* extern bool	load_crlpath __P((SSL_CTX *, bool , char *)); */
extern void	setclttls __P((bool));
extern int	tls_get_info __P((SSL *, bool, char *, MACROS_T *, bool));
extern void	tlslogerr __P((int, int, const char *));
extern void	tls_set_verify __P((SSL_CTX *, SSL *, bool));
# if DANE
extern int dane_tlsa_chk __P((const char *, int, const char *, bool));
extern int dane_tlsa_clr __P((dane_tlsa_P));
extern int dane_tlsa_free __P((dane_tlsa_P));
# endif

EXTERN char	*CACertPath;	/* path to CA certificates (dir. with hashes) */
EXTERN char	*CACertFile;	/* file with CA certificate */
#if _FFR_CLIENTCA
EXTERN char	*CltCACertPath;	/* path to CA certificates (dir. with hashes) */
EXTERN char	*CltCACertFile;	/* file with CA certificate */
#endif
EXTERN char	*CltCertFile;	/* file with client certificate */
EXTERN char	*CltKeyFile;	/* file with client private key */
EXTERN char	*CipherList;	/* list of ciphers */
EXTERN char	*CertFingerprintAlgorithm;	/* name of fingerprint alg */
EXTERN const EVP_MD	*EVP_digest;	/* digest for cert fp */
EXTERN char	*DHParams;	/* file with DH parameters */
EXTERN char	*RandFile;	/* source of random data */
EXTERN char	*SrvCertFile;	/* file with server certificate */
EXTERN char	*SrvKeyFile;	/* file with server private key */
EXTERN char	*CRLFile;	/* file CRLs */
EXTERN char	*CRLPath;	/* path to CRLs (dir. with hashes) */
EXTERN unsigned long	TLS_Srv_Opts;	/* TLS server options */
EXTERN unsigned long	Srv_SSL_Options, Clt_SSL_Options; /* SSL options */
EXTERN bool	TLSFallbacktoClear;

EXTERN char	*SSLEngine;
EXTERN char	*SSLEnginePath;
EXTERN bool	SSLEngineprefork;

# if USE_OPENSSL_ENGINE
#define TLS_set_engine(id, prefork) SSL_set_engine(id)
# else
int TLS_set_engine __P((const char *, bool));
# endif

extern int	set_tls_rd_tmo __P((int));
extern int data2hex __P((unsigned char *, int, unsigned char *, int));
# if DANE
extern int pubkey_fp __P((X509 *, const char*, char **));
extern dane_tlsa_P dane_get_tlsa __P((dane_vrfy_ctx_P));
# endif

#else /* STARTTLS */
# define set_tls_rd_tmo(rd_tmo)	0
#endif /* STARTTLS */
#undef EXTERN
#endif /* ! _TLS_H */
