/*
 * Program to generate cryptographic keys for NTP clients and servers
 *
 * This program generates files "ntpkey_<type>_<hostname>.<filestamp>",
 * where <type> is the file type, <hostname> is the generating host and
 * <filestamp> is the NTP seconds in decimal format. The NTP programs
 * expect generic names such as "ntpkey_<type>_whimsy.udel.edu" with the
 * association maintained by soft links.
 *
 * Files are prefixed with a header giving the name and date of creation
 * followed by a type-specific descriptive label and PEM-encoded data
 * string compatible with programs of the OpenSSL library.
 *
 * Note that private keys can be password encrypted as per OpenSSL
 * conventions.
 *
 * The file types include
 *
 * ntpkey_MD5key_<hostname>.<filestamp>
 * 	MD5 (128-bit) keys used to compute message digests in symmetric
 *	key cryptography
 *
 * ntpkey_RSAkey_<hostname>.<filestamp>
 * ntpkey_host_<hostname> (RSA) link
 *	RSA private/public host key pair used for public key signatures
 *	and data encryption
 *
 * ntpkey_DSAkey_<hostname>.<filestamp>
 * ntpkey_sign_<hostname> (RSA or DSA) link
 *	DSA private/public sign key pair used for public key signatures,
 *	but not data encryption
 *
 * ntpkey_IFFpar_<hostname>.<filestamp>
 * ntpkey_iff_<hostname> (IFF server/client) link
 * ntpkey_iffkey_<hostname> (IFF client) link
 *	Schnorr (IFF) server/client identity parameters
 *
 * ntpkey_IFFkey_<hostname>.<filestamp>
 *	Schnorr (IFF) client identity parameters
 *
 * ntpkey_GQpar_<hostname>.<filestamp>,
 * ntpkey_gq_<hostname> (GQ) link
 *	Guillou-Quisquater (GQ) identity parameters
 *
 * ntpkey_MVpar_<hostname>.<filestamp>,
 *	Mu-Varadharajan (MV) server identity parameters 
 *
 * ntpkey_MVkeyX_<hostname>.<filestamp>,
 * ntpkey_mv_<hostname> (MV server) link
 * ntpkey_mvkey_<hostname> (MV client) link
 *	Mu-Varadharajan (MV) client identity parameters
 *
 * ntpkey_XXXcert_<hostname>.<filestamp>
 * ntpkey_cert_<hostname> (RSA or DSA) link
 *	X509v3 certificate using RSA or DSA public keys and signatures.
 *	XXX is a code identifying the message digest and signature
 *	encryption algorithm
 *
 * Available digest/signature schemes
 *
 * RSA:	RSA-MD2, RSA-MD5, RSA-SHA, RSA-SHA1, RSA-MDC2, EVP-RIPEMD160
 * DSA:	DSA-SHA, DSA-SHA1
 *
 * Note: Once in a while because of some statistical fluke this program
 * fails to generate and verify some cryptographic data, as indicated by
 * exit status -1. In this case simply run the program again. If the
 * program does complete with return code 0, the data are correct as
 * verified.
 *
 * These cryptographic routines are characterized by the prime modulus
 * size in bits. The default value of 512 bits is a compromise between
 * cryptographic strength and computing time and is ordinarily
 * considered adequate for this application. The routines have been
 * tested with sizes of 256, 512, 1024 and 2048 bits. Not all message
 * digest and signature encryption schemes work with sizes less than 512
 * bits. The computing time for sizes greater than 2048 bits is
 * prohibitive on all but the fastest processors. An UltraSPARC Blade
 * 1000 took something over nine minutes to generate and verify the
 * values with size 2048. An old SPARC IPC would take a week.
 *
 * The OpenSSL library used by this program expects a random seed file.
 * As described in the OpenSSL documentation, the file name defaults to
 * first the RANDFILE environment variable in the user's home directory
 * and then .rnd in the user's home directory.
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include "ntp_types.h"
#include "ntp_random.h"
#include "l_stdlib.h"

#include "ntp-keygen-opts.h"

#ifdef SYS_WINNT
extern	int	ntp_getopt	P((int, char **, const char *));
#define getopt ntp_getopt
#define optarg ntp_optarg
#endif

#ifdef OPENSSL
#include "openssl/bn.h"
#include "openssl/evp.h"
#include "openssl/err.h"
#include "openssl/rand.h"
#include "openssl/pem.h"
#include "openssl/x509v3.h"
#include <openssl/objects.h>
#endif /* OPENSSL */

/*
 * Cryptodefines
 */
#define	MD5KEYS		16	/* number of MD5 keys generated */
#define	JAN_1970	ULONG_CONST(2208988800) /* NTP seconds */
#define YEAR		((long)60*60*24*365) /* one year in seconds */
#define MAXFILENAME	256	/* max file name length */
#define MAXHOSTNAME	256	/* max host name length */
#ifdef OPENSSL
#define	PLEN		512	/* default prime modulus size (bits) */

/*
 * Strings used in X509v3 extension fields
 */
#define KEY_USAGE		"digitalSignature,keyCertSign"
#define BASIC_CONSTRAINTS	"critical,CA:TRUE"
#define EXT_KEY_PRIVATE		"private"
#define EXT_KEY_TRUST		"trustRoot"
#endif /* OPENSSL */

/*
 * Prototypes
 */
FILE	*fheader	P((const char *, const char *));
void	fslink		P((const char *, const char *));
int	gen_md5		P((char *));
#ifdef OPENSSL
EVP_PKEY *gen_rsa	P((char *));
EVP_PKEY *gen_dsa	P((char *));
EVP_PKEY *gen_iff	P((char *));
EVP_PKEY *gen_gqpar	P((char *));
EVP_PKEY *gen_gqkey	P((char *, EVP_PKEY *));
EVP_PKEY *gen_mv	P((char *));
int	x509		P((EVP_PKEY *, const EVP_MD *, char *, char *));
void	cb		P((int, int, void *));
EVP_PKEY *genkey	P((char *, char *));
u_long	asn2ntp		P((ASN1_TIME *));
#endif /* OPENSSL */

/*
 * Program variables
 */
extern char *optarg;		/* command line argument */
int	debug = 0;		/* debug, not de bug */
int	rval;			/* return status */
#ifdef OPENSSL
u_int	modulus = PLEN;		/* prime modulus size (bits) */
#endif
int	nkeys = 0;		/* MV keys */
time_t	epoch;			/* Unix epoch (seconds) since 1970 */
char	*hostname;		/* host name (subject name) */
char	*trustname;		/* trusted host name (issuer name) */
char	filename[MAXFILENAME + 1]; /* file name */
char	*passwd1 = NULL;	/* input private key password */
char	*passwd2 = NULL;	/* output private key password */
#ifdef OPENSSL
long	d0, d1, d2, d3;		/* callback counters */
#endif /* OPENSSL */

#ifdef SYS_WINNT
BOOL init_randfile();

/*
 * Don't try to follow symbolic links
 */
int
readlink(char * link, char * file, int len) {
	return (-1);
}
/*
 * Don't try to create a symbolic link for now.
 * Just move the file to the name you need.
 */
int
symlink(char *filename, char *linkname) {
	DeleteFile(linkname);
	MoveFile(filename, linkname);
	return 0;
}
void
InitWin32Sockets() {
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2,0);
	if (WSAStartup(wVersionRequested, &wsaData))
	{
		fprintf(stderr, "No useable winsock.dll");
		exit(1);
	}
}
#endif /* SYS_WINNT */

/*
 * Main program
 */
int
main(
	int	argc,		/* command line options */
	char	**argv
	)
{
	struct timeval tv;	/* initialization vector */
	int	md5key = 0;	/* generate MD5 keys */
#ifdef OPENSSL
	X509	*cert = NULL;	/* X509 certificate */
	EVP_PKEY *pkey_host = NULL; /* host key */
	EVP_PKEY *pkey_sign = NULL; /* sign key */
	EVP_PKEY *pkey_iff = NULL; /* IFF parameters */
	EVP_PKEY *pkey_gq = NULL; /* GQ parameters */
	EVP_PKEY *pkey_mv = NULL; /* MV parameters */
	int	hostkey = 0;	/* generate RSA keys */
	int	iffkey = 0;	/* generate IFF parameters */
	int	gqpar = 0;	/* generate GQ parameters */
	int	gqkey = 0;	/* update GQ keys */
	int	mvpar = 0;	/* generate MV parameters */
	int	mvkey = 0;	/* update MV keys */
	char	*sign = NULL;	/* sign key */
	EVP_PKEY *pkey = NULL;	/* temp key */
	const EVP_MD *ectx;	/* EVP digest */
	char	pathbuf[MAXFILENAME + 1];
	const char *scheme = NULL; /* digest/signature scheme */
	char	*exten = NULL;	/* private extension */
	char	*grpkey = NULL;	/* identity extension */
	int	nid;		/* X509 digest/signature scheme */
	FILE	*fstr = NULL;	/* file handle */
	u_int	temp;
#define iffsw   HAVE_OPT(ID_KEY)
#endif /* OPENSSL */
	char	hostbuf[MAXHOSTNAME + 1];

#ifdef SYS_WINNT
	/* Initialize before OpenSSL checks */
	InitWin32Sockets();
	if(!init_randfile())
		fprintf(stderr, "Unable to initialize .rnd file\n");
#endif

#ifdef OPENSSL
	/*
	 * OpenSSL version numbers: MNNFFPPS: major minor fix patch status
	 * We match major, minor, fix and status (not patch)
	 */
	if ((SSLeay() ^ OPENSSL_VERSION_NUMBER) & ~0xff0L) {
		fprintf(stderr,
		    "OpenSSL version mismatch. Built against %lx, you have %lx\n",
		    OPENSSL_VERSION_NUMBER, SSLeay());
		return (-1);

	} else {
		fprintf(stderr,
		    "Using OpenSSL version %lx\n", SSLeay());
	}
#endif /* OPENSSL */

	/*
	 * Process options, initialize host name and timestamp.
	 */
	gethostname(hostbuf, MAXHOSTNAME);
	hostname = hostbuf;
#ifdef OPENSSL
	trustname = hostbuf;
	passwd1 = hostbuf;
#endif
#ifndef SYS_WINNT
	gettimeofday(&tv, 0);
#else
	gettimeofday(&tv);
#endif
	epoch = tv.tv_sec;
	rval = 0;

	{
		int optct = optionProcess(&ntp_keygenOptions, argc, argv);
		argc -= optct;
		argv += optct;
	}

#ifdef OPENSSL
	if (HAVE_OPT( CERTIFICATE ))
	    scheme = OPT_ARG( CERTIFICATE );
#endif

	debug = DESC(DEBUG_LEVEL).optOccCt;

#ifdef OPENSSL
	if (HAVE_OPT( GQ_PARAMS ))
	    gqpar++;

	if (HAVE_OPT( GQ_KEYS ))
	    gqkey++;

	if (HAVE_OPT( HOST_KEY ))
	    hostkey++;

	if (HAVE_OPT( IFFKEY ))
	    iffkey++;

	if (HAVE_OPT( ISSUER_NAME ))
	    trustname = OPT_ARG( ISSUER_NAME );
#endif

	if (HAVE_OPT( MD5KEY ))
	    md5key++;

#ifdef OPENSSL
	if (HAVE_OPT( MODULUS ))
	    modulus = OPT_VALUE_MODULUS;

	if (HAVE_OPT( PVT_CERT ))
	    exten = EXT_KEY_PRIVATE;

	if (HAVE_OPT( PVT_PASSWD ))
	    passwd2 = OPT_ARG( PVT_PASSWD );

	if (HAVE_OPT( GET_PVT_PASSWD ))
	    passwd1 = OPT_ARG( GET_PVT_PASSWD );

	if (HAVE_OPT( SIGN_KEY ))
	    sign = OPT_ARG( SIGN_KEY );

	if (HAVE_OPT( SUBJECT_NAME ))
	    hostname = OPT_ARG( SUBJECT_NAME );

	if (HAVE_OPT( TRUSTED_CERT ))
	    exten = EXT_KEY_TRUST;

	if (HAVE_OPT( MV_PARAMS )) {
		mvpar++;
		nkeys = OPT_VALUE_MV_PARAMS;
	}

	if (HAVE_OPT( MV_KEYS )) {
		mvkey++;
		nkeys = OPT_VALUE_MV_KEYS;
	}
#endif

	if (passwd1 != NULL && passwd2 == NULL)
		passwd2 = passwd1;
#ifdef OPENSSL
	/*
	 * Seed random number generator and grow weeds.
	 */
	ERR_load_crypto_strings();
	OpenSSL_add_all_algorithms();
	if (RAND_file_name(pathbuf, MAXFILENAME) == NULL) {
		fprintf(stderr, "RAND_file_name %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		return (-1);
	}
	temp = RAND_load_file(pathbuf, -1);
	if (temp == 0) {
		fprintf(stderr,
		    "RAND_load_file %s not found or empty\n", pathbuf);
		return (-1);
	}
	fprintf(stderr,
	    "Random seed file %s %u bytes\n", pathbuf, temp);
	RAND_add(&epoch, sizeof(epoch), 4.0);
#endif

	/*
	 * Generate new parameters and keys as requested. These replace
	 * any values already generated.
	 */
	if (md5key)
		gen_md5("MD5");
#ifdef OPENSSL
	if (hostkey)
		pkey_host = genkey("RSA", "host");
	if (sign != NULL)
		pkey_sign = genkey(sign, "sign");
	if (iffkey)
		pkey_iff = gen_iff("iff");
	if (gqpar)
		pkey_gq = gen_gqpar("gq");
	if (mvpar)
		pkey_mv = gen_mv("mv");

	/*
	 * If there is no new host key, look for an existing one. If not
	 * found, create it.
	 */
	while (pkey_host == NULL && rval == 0 && !HAVE_OPT(ID_KEY)) {
		sprintf(filename, "ntpkey_host_%s", hostname);
		if ((fstr = fopen(filename, "r")) != NULL) {
			pkey_host = PEM_read_PrivateKey(fstr, NULL,
			    NULL, passwd1);
			fclose(fstr);
			readlink(filename, filename,  sizeof(filename));
			if (pkey_host == NULL) {
				fprintf(stderr, "Host key\n%s\n",
				    ERR_error_string(ERR_get_error(),
				    NULL));
				rval = -1;
			} else {
				fprintf(stderr,
				    "Using host key %s\n", filename);
			}
			break;

		} else if ((pkey_host = genkey("RSA", "host")) ==
		    NULL) {
			rval = -1;
			break;
		}
	}

	/*
	 * If there is no new sign key, look for an existing one. If not
	 * found, use the host key instead.
	 */
	pkey = pkey_sign;
	while (pkey_sign == NULL && rval == 0 && !HAVE_OPT(ID_KEY)) {
		sprintf(filename, "ntpkey_sign_%s", hostname);
		if ((fstr = fopen(filename, "r")) != NULL) {
			pkey_sign = PEM_read_PrivateKey(fstr, NULL,
			    NULL, passwd1);
			fclose(fstr);
			readlink(filename, filename, sizeof(filename));
			if (pkey_sign == NULL) {
				fprintf(stderr, "Sign key\n%s\n",
				    ERR_error_string(ERR_get_error(),
				    NULL));
				rval = -1;
			} else {
				fprintf(stderr, "Using sign key %s\n",
				    filename);
			}
			break;
		} else {
			pkey = pkey_host;
			fprintf(stderr, "Using host key as sign key\n");
			break;
		}
	}

	/*
	 * If there is no new IFF file, look for an existing one.
	 */
	if (pkey_iff == NULL && rval == 0) {
		sprintf(filename, "ntpkey_iff_%s", hostname);
		if ((fstr = fopen(filename, "r")) != NULL) {
			pkey_iff = PEM_read_PrivateKey(fstr, NULL,
			    NULL, passwd1);
			fclose(fstr);
			readlink(filename, filename, sizeof(filename));
			if (pkey_iff == NULL) {
				fprintf(stderr, "IFF parameters\n%s\n",
				    ERR_error_string(ERR_get_error(),
				    NULL));
				rval = -1;
			} else {
				fprintf(stderr,
				    "Using IFF parameters %s\n",
				    filename);
			}
		}
	}

	/*
	 * If there is no new GQ file, look for an existing one.
	 */
	if (pkey_gq == NULL && rval == 0 && !HAVE_OPT(ID_KEY)) {
		sprintf(filename, "ntpkey_gq_%s", hostname);
		if ((fstr = fopen(filename, "r")) != NULL) {
			pkey_gq = PEM_read_PrivateKey(fstr, NULL, NULL,
			    passwd1);
			fclose(fstr);
			readlink(filename, filename, sizeof(filename));
			if (pkey_gq == NULL) {
				fprintf(stderr, "GQ parameters\n%s\n",
				    ERR_error_string(ERR_get_error(),
				    NULL));
				rval = -1;
			} else {
				fprintf(stderr,
				    "Using GQ parameters %s\n",
				    filename);
			}
		}
	}

	/*
	 * If there is a GQ parameter file, create GQ private/public
	 * keys and extract the public key for the certificate.
	 */
	if (pkey_gq != NULL && rval == 0) {
		gen_gqkey("gq", pkey_gq);
		grpkey = BN_bn2hex(pkey_gq->pkey.rsa->q);
	}

	/*
	 * Generate a X509v3 certificate.
	 */
	while (scheme == NULL && rval == 0 && !HAVE_OPT(ID_KEY)) {
		sprintf(filename, "ntpkey_cert_%s", hostname);
		if ((fstr = fopen(filename, "r")) != NULL) {
			cert = PEM_read_X509(fstr, NULL, NULL, NULL);
			fclose(fstr);
			readlink(filename, filename, sizeof(filename));
			if (cert == NULL) {
				fprintf(stderr, "Cert \n%s\n",
				    ERR_error_string(ERR_get_error(),
				    NULL));
				rval = -1;
			} else {
				nid = OBJ_obj2nid(
				 cert->cert_info->signature->algorithm);
				scheme = OBJ_nid2sn(nid);
				fprintf(stderr,
				    "Using scheme %s from %s\n", scheme,
				     filename);
				break;
			}
		}
		scheme = "RSA-MD5";
	}
	if (pkey != NULL && rval == 0 && !HAVE_OPT(ID_KEY)) {
		ectx = EVP_get_digestbyname(scheme);
		if (ectx == NULL) {
			fprintf(stderr,
			    "Invalid digest/signature combination %s\n",
			    scheme);
			rval = -1;
		} else {
			x509(pkey, ectx, grpkey, exten);
		}
	}

	/*
	 * Write the IFF client parameters and keys as a DSA private key
	 * encoded in PEM. Note the private key is obscured.
	 */
	if (pkey_iff != NULL && rval == 0 && HAVE_OPT(ID_KEY)) {
		DSA	*dsa;
		char	*sptr;
		char	*tld;

		sptr = strrchr(filename, '.');
		tld = malloc(strlen(sptr));	/* we have an extra byte ... */
		strcpy(tld, 1+sptr);		/* ... see? */
		sprintf(filename, "ntpkey_IFFkey_%s.%s", trustname,
		    tld);
		free(tld);
		fprintf(stderr, "Writing new IFF key %s\n", filename);
		fprintf(stdout, "# %s\n# %s", filename, ctime(&epoch));
		dsa = pkey_iff->pkey.dsa;
		BN_copy(dsa->priv_key, BN_value_one());
		pkey = EVP_PKEY_new();
		EVP_PKEY_assign_DSA(pkey, dsa);
		PEM_write_PrivateKey(stdout, pkey, passwd2 ?
		    EVP_des_cbc() : NULL, NULL, 0, NULL, passwd2);
		fclose(stdout);
		if (debug)
			DSA_print_fp(stdout, dsa, 0);
	}

	/*
	 * Return the marbles.
	 */
	if (grpkey != NULL)
		OPENSSL_free(grpkey);
	if (pkey_host != NULL)
		EVP_PKEY_free(pkey_host);
	if (pkey_sign != NULL)
		EVP_PKEY_free(pkey_sign);
	if (pkey_iff != NULL)
		EVP_PKEY_free(pkey_iff);
	if (pkey_gq != NULL)
		EVP_PKEY_free(pkey_gq);
	if (pkey_mv != NULL)
		EVP_PKEY_free(pkey_mv);
#endif /* OPENSSL */
	return (rval);
}


#if 0
/*
 * Generate random MD5 key with password.
 */
int
gen_md5(
	char	*id		/* file name id */
	)
{
	BIGNUM	*key;
	BIGNUM	*keyid;
	FILE	*str;
	u_char	bin[16];

	fprintf(stderr, "Generating MD5 keys...\n");
	str = fheader("MD5key", hostname);
	keyid = BN_new(); key = BN_new();
	BN_rand(keyid, 16, -1, 0);
	BN_rand(key, 128, -1, 0);
	BN_bn2bin(key, bin);
	PEM_write_fp(str, MD5, NULL, bin);
	fclose(str);
	fslink(id, hostname);
	return (1);
}


#else
/*
 * Generate semi-random MD5 keys compatible with NTPv3 and NTPv4
 */
int
gen_md5(
	char	*id		/* file name id */
	)
{
	u_char	md5key[16];	/* MD5 key */
	FILE	*str;
	u_int	temp = 0;	/* Initialize to prevent warnings during compile */
	int	i, j;

	fprintf(stderr, "Generating MD5 keys...\n");
	str = fheader("MD5key", hostname);
	ntp_srandom(epoch);
	for (i = 1; i <= MD5KEYS; i++) {
		for (j = 0; j < 16; j++) {
			while (1) {
				temp = ntp_random() & 0xff;
				if (temp == '#')
					continue;
				if (temp > 0x20 && temp < 0x7f)
					break;
			}
			md5key[j] = (u_char)temp;
		}
		md5key[15] = '\0';
		fprintf(str, "%2d MD5 %16s	# MD5 key\n", i,
		    md5key);
	}
	fclose(str);
	fslink(id, hostname);
	return (1);
}
#endif /* OPENSSL */


#ifdef OPENSSL
/*
 * Generate RSA public/private key pair
 */
EVP_PKEY *			/* public/private key pair */
gen_rsa(
	char	*id		/* file name id */
	)
{
	EVP_PKEY *pkey;		/* private key */
	RSA	*rsa;		/* RSA parameters and key pair */
	FILE	*str;

	fprintf(stderr, "Generating RSA keys (%d bits)...\n", modulus);
	rsa = RSA_generate_key(modulus, 3, cb, "RSA");
	fprintf(stderr, "\n");
	if (rsa == NULL) {
		fprintf(stderr, "RSA generate keys fails\n%s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		rval = -1;
		return (NULL);
	}

	/*
	 * For signature encryption it is not necessary that the RSA
	 * parameters be strictly groomed and once in a while the
	 * modulus turns out to be non-prime. Just for grins, we check
	 * the primality.
	 */
	if (!RSA_check_key(rsa)) {
		fprintf(stderr, "Invalid RSA key\n%s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		RSA_free(rsa);
		rval = -1;
		return (NULL);
	}

	/*
	 * Write the RSA parameters and keys as a RSA private key
	 * encoded in PEM.
	 */
	str = fheader("RSAkey", hostname);
	pkey = EVP_PKEY_new();
	EVP_PKEY_assign_RSA(pkey, rsa);
	PEM_write_PrivateKey(str, pkey, passwd2 ? EVP_des_cbc() : NULL,
	    NULL, 0, NULL, passwd2);
	fclose(str);
	if (debug)
		RSA_print_fp(stdout, rsa, 0);
	fslink(id, hostname);
	return (pkey);
}

 
/*
 * Generate DSA public/private key pair
 */
EVP_PKEY *			/* public/private key pair */
gen_dsa(
	char	*id		/* file name id */
	)
{
	EVP_PKEY *pkey;		/* private key */
	DSA	*dsa;		/* DSA parameters */
	u_char	seed[20];	/* seed for parameters */
	FILE	*str;

	/*
	 * Generate DSA parameters.
	 */
	fprintf(stderr,
	    "Generating DSA parameters (%d bits)...\n", modulus);
	RAND_bytes(seed, sizeof(seed));
	dsa = DSA_generate_parameters(modulus, seed, sizeof(seed), NULL,
	    NULL, cb, "DSA");
	fprintf(stderr, "\n");
	if (dsa == NULL) {
		fprintf(stderr, "DSA generate parameters fails\n%s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		rval = -1;
		return (NULL);
	}

	/*
	 * Generate DSA keys.
	 */
	fprintf(stderr, "Generating DSA keys (%d bits)...\n", modulus);
	if (!DSA_generate_key(dsa)) {
		fprintf(stderr, "DSA generate keys fails\n%s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		DSA_free(dsa);
		rval = -1;
		return (NULL);
	}

	/*
	 * Write the DSA parameters and keys as a DSA private key
	 * encoded in PEM.
	 */
	str = fheader("DSAkey", hostname);
	pkey = EVP_PKEY_new();
	EVP_PKEY_assign_DSA(pkey, dsa);
	PEM_write_PrivateKey(str, pkey, passwd2 ? EVP_des_cbc() : NULL,
	    NULL, 0, NULL, passwd2);
	fclose(str);
	if (debug)
		DSA_print_fp(stdout, dsa, 0);
	fslink(id, hostname);
	return (pkey);
}


/*
 * Generate Schnorr (IFF) parameters and keys
 *
 * The Schnorr (IFF)identity scheme is intended for use when
 * certificates are generated by some other trusted certificate
 * authority and the parameters cannot be conveyed in the certificate
 * itself. For this purpose, new generations of IFF values must be
 * securely transmitted to all members of the group before use. There
 * are two kinds of files: server/client files that include private and
 * public parameters and client files that include only public
 * parameters. The scheme is self contained and independent of new
 * generations of host keys, sign keys and certificates.
 *
 * The IFF values hide in a DSA cuckoo structure which uses the same
 * parameters. The values are used by an identity scheme based on DSA
 * cryptography and described in Stimson p. 285. The p is a 512-bit
 * prime, g a generator of Zp* and q a 160-bit prime that divides p - 1
 * and is a qth root of 1 mod p; that is, g^q = 1 mod p. The TA rolls a
 * private random group key b (0 < b < q), then computes public
 * v = g^(q - a). All values except the group key are known to all group
 * members; the group key is known to the group servers, but not the
 * group clients. Alice challenges Bob to confirm identity using the
 * protocol described below.
 */
EVP_PKEY *			/* DSA cuckoo nest */
gen_iff(
	char	*id		/* file name id */
	)
{
	EVP_PKEY *pkey;		/* private key */
	DSA	*dsa;		/* DSA parameters */
	u_char	seed[20];	/* seed for parameters */
	BN_CTX	*ctx;		/* BN working space */
	BIGNUM	*b, *r, *k, *u, *v, *w; /* BN temp */
	FILE	*str;
	u_int	temp;

	/*
	 * Generate DSA parameters for use as IFF parameters.
	 */
	fprintf(stderr, "Generating IFF parameters (%d bits)...\n",
	    modulus);
	RAND_bytes(seed, sizeof(seed));
	dsa = DSA_generate_parameters(modulus, seed, sizeof(seed), NULL,
	    NULL, cb, "IFF");
	fprintf(stderr, "\n");
	if (dsa == NULL) {
		fprintf(stderr, "DSA generate parameters fails\n%s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		rval = -1;
		return (NULL);;
	}

	/*
	 * Generate the private and public keys. The DSA parameters and
	 * these keys are distributed to all members of the group.
	 */
	fprintf(stderr, "Generating IFF keys (%d bits)...\n", modulus);
	b = BN_new(); r = BN_new(); k = BN_new();
	u = BN_new(); v = BN_new(); w = BN_new(); ctx = BN_CTX_new();
	BN_rand(b, BN_num_bits(dsa->q), -1, 0);	/* a */
	BN_mod(b, b, dsa->q, ctx);
	BN_sub(v, dsa->q, b);
	BN_mod_exp(v, dsa->g, v, dsa->p, ctx); /* g^(q - b) mod p */
	BN_mod_exp(u, dsa->g, b, dsa->p, ctx);	/* g^b mod p */
	BN_mod_mul(u, u, v, dsa->p, ctx);
	temp = BN_is_one(u);
	fprintf(stderr,
	    "Confirm g^(q - b) g^b = 1 mod p: %s\n", temp == 1 ?
	    "yes" : "no");
	if (!temp) {
		BN_free(b); BN_free(r); BN_free(k);
		BN_free(u); BN_free(v); BN_free(w); BN_CTX_free(ctx);
		rval = -1;
		return (NULL);
	}
	dsa->priv_key = BN_dup(b);		/* private key */
	dsa->pub_key = BN_dup(v);		/* public key */

	/*
	 * Here is a trial round of the protocol. First, Alice rolls
	 * random r (0 < r < q) and sends it to Bob. She needs only
	 * modulus q.
	 */
	BN_rand(r, BN_num_bits(dsa->q), -1, 0);	/* r */
	BN_mod(r, r, dsa->q, ctx);

	/*
	 * Bob rolls random k (0 < k < q), computes y = k + b r mod q
	 * and x = g^k mod p, then sends (y, x) to Alice. He needs
	 * moduli p, q and the group key b.
	 */
	BN_rand(k, BN_num_bits(dsa->q), -1, 0);	/* k, 0 < k < q  */
	BN_mod(k, k, dsa->q, ctx);
	BN_mod_mul(v, dsa->priv_key, r, dsa->q, ctx); /* b r mod q */
	BN_add(v, v, k);
	BN_mod(v, v, dsa->q, ctx);		/* y = k + b r mod q */
	BN_mod_exp(u, dsa->g, k, dsa->p, ctx);	/* x = g^k mod p */

	/*
	 * Alice computes g^y v^r and verifies the result is equal to x.
	 * She needs modulus p, generator g, and the public key v, as
	 * well as her original r.
	 */
	BN_mod_exp(v, dsa->g, v, dsa->p, ctx); /* g^y mod p */
	BN_mod_exp(w, dsa->pub_key, r, dsa->p, ctx); /* v^r */
	BN_mod_mul(v, w, v, dsa->p, ctx);	/* product mod p */
	temp = BN_cmp(u, v);
	fprintf(stderr,
	    "Confirm g^k = g^(k + b r) g^(q - b) r: %s\n", temp ==
	    0 ? "yes" : "no");
	BN_free(b); BN_free(r);	BN_free(k);
	BN_free(u); BN_free(v); BN_free(w); BN_CTX_free(ctx);
	if (temp != 0) {
		DSA_free(dsa);
		rval = -1;
		return (NULL);
	}

	/*
	 * Write the IFF server parameters and keys as a DSA private key
	 * encoded in PEM.
	 *
	 * p	modulus p
	 * q	modulus q
	 * g	generator g
	 * priv_key b
	 * public_key v
	 */
	str = fheader("IFFpar", trustname);
	pkey = EVP_PKEY_new();
	EVP_PKEY_assign_DSA(pkey, dsa);
	PEM_write_PrivateKey(str, pkey, passwd2 ? EVP_des_cbc() : NULL,
	    NULL, 0, NULL, passwd2);
	fclose(str);
	if (debug)
		DSA_print_fp(stdout, dsa, 0);
	fslink(id, trustname);
	return (pkey);
}


/*
 * Generate Guillou-Quisquater (GQ) parameters and keys
 *
 * The Guillou-Quisquater (GQ) identity scheme is intended for use when
 * the parameters, keys and certificates are generated by this program.
 * The scheme uses a certificate extension field do convey the public
 * key of a particular group identified by a group key known only to
 * members of the group. The scheme is self contained and independent of
 * new generations of host keys and sign keys.
 *
 * The GQ parameters hide in a RSA cuckoo structure which uses the same
 * parameters. The values are used by an identity scheme based on RSA
 * cryptography and described in Stimson p. 300 (with errors). The 512-
 * bit public modulus is n = p q, where p and q are secret large primes.
 * The TA rolls private random group key b as RSA exponent. These values
 * are known to all group members.
 *
 * When rolling new certificates, a member recomputes the private and
 * public keys. The private key u is a random roll, while the public key
 * is the inverse obscured by the group key v = (u^-1)^b. These values
 * replace the private and public keys normally generated by the RSA
 * scheme. Alice challenges Bob to confirm identity using the protocol
 * described below.
 */
EVP_PKEY *			/* RSA cuckoo nest */
gen_gqpar(
	char	*id		/* file name id */
	)
{
	EVP_PKEY *pkey;		/* private key */
	RSA	*rsa;		/* GQ parameters */
	BN_CTX	*ctx;		/* BN working space */
	FILE	*str;

	/*
	 * Generate RSA parameters for use as GQ parameters.
	 */
	fprintf(stderr,
	    "Generating GQ parameters (%d bits)...\n", modulus);
	rsa = RSA_generate_key(modulus, 3, cb, "GQ");
	fprintf(stderr, "\n");
	if (rsa == NULL) {
		fprintf(stderr, "RSA generate keys fails\n%s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		rval = -1;
		return (NULL);
	}

	/*
	 * Generate the group key b, which is saved in the e member of
	 * the RSA structure. These values are distributed to all
	 * members of the group, but shielded from all other groups. We
	 * don't use all the parameters, but set the unused ones to a
	 * small number to minimize the file size.
	 */
	ctx = BN_CTX_new();
	BN_rand(rsa->e, BN_num_bits(rsa->n), -1, 0); /* b */
	BN_mod(rsa->e, rsa->e, rsa->n, ctx);
	BN_copy(rsa->d, BN_value_one());
	BN_copy(rsa->p, BN_value_one());
	BN_copy(rsa->q, BN_value_one());
	BN_copy(rsa->dmp1, BN_value_one());
	BN_copy(rsa->dmq1, BN_value_one());
	BN_copy(rsa->iqmp, BN_value_one());

	/*
	 * Write the GQ parameters as a RSA private key encoded in PEM.
	 * The public and private keys are filled in later.
	 *
	 * n	modulus n
	 * e	group key b
	 * (remaining values are not used)
	 */
	str = fheader("GQpar", trustname);
	pkey = EVP_PKEY_new();
	EVP_PKEY_assign_RSA(pkey, rsa);
	PEM_write_PrivateKey(str, pkey, passwd2 ? EVP_des_cbc() : NULL,
	    NULL, 0, NULL, passwd2);
	fclose(str);
	if (debug)
		RSA_print_fp(stdout, rsa, 0);
	fslink(id, trustname);
	return (pkey);
}


/*
 * Update Guillou-Quisquater (GQ) parameters
 */
EVP_PKEY *			/* RSA cuckoo nest */
gen_gqkey(
	char	*id,		/* file name id */
	EVP_PKEY *gqpar		/* GQ parameters */
	)
{
	EVP_PKEY *pkey;		/* private key */
	RSA	*rsa;		/* RSA parameters */
	BN_CTX	*ctx;		/* BN working space */
	BIGNUM	*u, *v, *g, *k, *r, *y; /* BN temps */
	FILE	*str;
	u_int	temp;

	/*
	 * Generate GQ keys. Note that the group key b is the e member
	 * of
	 * the GQ parameters.
	 */
	fprintf(stderr, "Updating GQ keys (%d bits)...\n", modulus);
	ctx = BN_CTX_new(); u = BN_new(); v = BN_new();
	g = BN_new(); k = BN_new(); r = BN_new(); y = BN_new();

	/*
	 * When generating his certificate, Bob rolls random private key
	 * u. 
	 */
	rsa = gqpar->pkey.rsa;
	BN_rand(u, BN_num_bits(rsa->n), -1, 0); /* u */
	BN_mod(u, u, rsa->n, ctx);
	BN_mod_inverse(v, u, rsa->n, ctx);	/* u^-1 mod n */
	BN_mod_mul(k, v, u, rsa->n, ctx);

	/*
	 * Bob computes public key v = (u^-1)^b, which is saved in an
	 * extension field on his certificate. We check that u^b v =
	 * 1 mod n.
	 */
	BN_mod_exp(v, v, rsa->e, rsa->n, ctx);
	BN_mod_exp(g, u, rsa->e, rsa->n, ctx); /* u^b */
	BN_mod_mul(g, g, v, rsa->n, ctx); /* u^b (u^-1)^b */
	temp = BN_is_one(g);
	fprintf(stderr,
	    "Confirm u^b (u^-1)^b = 1 mod n: %s\n", temp ? "yes" :
	    "no");
	if (!temp) {
		BN_free(u); BN_free(v);
		BN_free(g); BN_free(k); BN_free(r); BN_free(y);
		BN_CTX_free(ctx);
		RSA_free(rsa);
		rval = -1;
		return (NULL);
	}
	BN_copy(rsa->p, u);			/* private key */
	BN_copy(rsa->q, v);			/* public key */

	/*
	 * Here is a trial run of the protocol. First, Alice rolls
	 * random r (0 < r < n) and sends it to Bob. She needs only
	 * modulus n from the parameters.
	 */
	BN_rand(r, BN_num_bits(rsa->n), -1, 0);	/* r */
	BN_mod(r, r, rsa->n, ctx);

	/*
	 * Bob rolls random k (0 < k < n), computes y = k u^r mod n and
	 * g = k^b mod n, then sends (y, g) to Alice. He needs modulus n
	 * from the parameters and his private key u. 
	 */
	BN_rand(k, BN_num_bits(rsa->n), -1, 0);	/* k */
	BN_mod(k, k, rsa->n, ctx);
	BN_mod_exp(y, rsa->p, r, rsa->n, ctx);	/* u^r mod n */
	BN_mod_mul(y, k, y, rsa->n, ctx);	/* y = k u^r mod n */
	BN_mod_exp(g, k, rsa->e, rsa->n, ctx); /* g = k^b mod n */

	/*
	 * Alice computes v^r y^b mod n and verifies the result is equal
	 * to g. She needs modulus n, generator g and group key b from
	 * the parameters and Bob's public key v = (u^-1)^b from his
	 * certificate.
	 */
	BN_mod_exp(v, rsa->q, r, rsa->n, ctx);	/* v^r mod n */
	BN_mod_exp(y, y, rsa->e, rsa->n, ctx); /* y^b mod n */
	BN_mod_mul(y, v, y, rsa->n, ctx);	/* v^r y^b mod n */
	temp = BN_cmp(y, g);
	fprintf(stderr, "Confirm g^k = v^r y^b mod n: %s\n", temp == 0 ?
	    "yes" : "no");
	BN_CTX_free(ctx); BN_free(u); BN_free(v);
	BN_free(g); BN_free(k); BN_free(r); BN_free(y);
	if (temp != 0) {
		RSA_free(rsa);
		rval = -1;
		return (NULL);
	}

	/*
	 * Write the GQ parameters and keys as a RSA private key encoded
	 * in PEM.
	 *
	 * n	modulus n
	 * e	group key b
	 * p	private key u
	 * q	public key (u^-1)^b
	 * (remaining values are not used)
	 */
	str = fheader("GQpar", trustname);
	pkey = EVP_PKEY_new();
	EVP_PKEY_assign_RSA(pkey, rsa);
	PEM_write_PrivateKey(str, pkey, passwd2 ? EVP_des_cbc() : NULL,
	    NULL, 0, NULL, passwd2);
	fclose(str);
	if (debug)
		RSA_print_fp(stdout, rsa, 0);
	fslink(id, trustname);
	return (pkey);
}


/*
 * Generate Mu-Varadharajan (MV) parameters and keys
 *
 * The Mu-Varadharajan (MV) cryptosystem is useful when servers
 * broadcast messages to clients, but clients never send messages to
 * servers. There is one encryption key for the server and a separate
 * decryption key for each client. It operates something like a
 * pay-per-view satellite broadcasting system where the session key is
 * encrypted by the broadcaster and the decryption keys are held in a
 * tamperproof set-top box. We don't use it this way, but read on.
 *
 * The MV parameters and private encryption key hide in a DSA cuckoo
 * structure which uses the same parameters, but generated in a
 * different way. The values are used in an encryption scheme similar to
 * El Gamal cryptography and a polynomial formed from the expansion of
 * product terms (x - x[j]), as described in Mu, Y., and V.
 * Varadharajan: Robust and Secure Broadcasting, Proc. Indocrypt 2001,
 * 223-231. The paper has significant errors and serious omissions.
 *
 * Let q be the product of n distinct primes s'[j] (j = 1...n), where
 * each s'[j] has m significant bits. Let p be a prime p = 2 * q + 1, so
 * that q and each s'[j] divide p - 1 and p has M = n * m + 1
 * significant bits. Let g be a generator of Zp; that is, gcd(g, p - 1)
 * = 1 and g^q = 1 mod p. We do modular arithmetic over Zq and then
 * project into Zp* as exponents of g. Sometimes we have to compute an
 * inverse b^-1 of random b in Zq, but for that purpose we require
 * gcd(b, q) = 1. We expect M to be in the 500-bit range and n
 * relatively small, like 30. Associated with each s'[j] is an element
 * s[j] such that s[j] s'[j] = s'[j] mod q. We find s[j] as the quotient
 * (q + s'[j]) / s'[j]. These are the parameters of the scheme and they
 * are expensive to compute.
 *
 * We set up an instance of the scheme as follows. A set of random
 * values x[j] mod q (j = 1...n), are generated as the zeros of a
 * polynomial of order n. The product terms (x - x[j]) are expanded to
 * form coefficients a[i] mod q (i = 0...n) in powers of x. These are
 * used as exponents of the generator g mod p to generate the private
 * encryption key A. The pair (gbar, ghat) of public server keys and the
 * pairs (xbar[j], xhat[j]) (j = 1...n) of private client keys are used
 * to construct the decryption keys. The devil is in the details.
 *
 * This routine generates a private encryption file including the
 * private encryption key E and public key (gbar, ghat). It then
 * generates decryption files including the private key (xbar[j],
 * xhat[j]) for each client. E is a permutation that encrypts a block
 * y = E x. The jth client computes the inverse permutation E^-1 =
 * gbar^xhat[j] ghat^xbar[j] and decrypts the block x = E^-1 y.
 *
 * The distinguishing characteristic of this scheme is the capability to
 * revoke keys. Included in the calculation of E, gbar and ghat is the
 * product s = prod(s'[j]) (j = 1...n) above. If the factor s'[j] is
 * subsequently removed from the product and E, gbar and ghat
 * recomputed, the jth client will no longer be able to compute E^-1 and
 * thus unable to decrypt the block.
 */
EVP_PKEY *			/* DSA cuckoo nest */
gen_mv(
	char	*id		/* file name id */
	)
{
	EVP_PKEY *pkey, *pkey1;	/* private key */
	DSA	*dsa;		/* DSA parameters */
	DSA	*sdsa;		/* DSA parameters */
	BN_CTX	*ctx;		/* BN working space */
	BIGNUM	**x;		/* polynomial zeros vector */
	BIGNUM	**a;		/* polynomial coefficient vector */
	BIGNUM	**g;		/* public key vector */
	BIGNUM	**s, **s1;	/* private enabling keys */
	BIGNUM	**xbar, **xhat;	/* private keys vector */
	BIGNUM	*b;		/* group key */
	BIGNUM	*b1;		/* inverse group key */
	BIGNUM	*ss;		/* enabling key */
	BIGNUM	*biga;		/* master encryption key */
	BIGNUM	*bige;		/* session encryption key */
	BIGNUM	*gbar, *ghat;	/* public key */
	BIGNUM	*u, *v, *w;	/* BN scratch */
	int	i, j, n;
	FILE	*str;
	u_int	temp;
	char	ident[20];

	/*
	 * Generate MV parameters.
	 *
	 * The object is to generate a multiplicative group Zp* modulo a
	 * prime p and a subset Zq mod q, where q is the product of n
	 * distinct primes s'[j] (j = 1...n) and q divides p - 1. We
	 * first generate n distinct primes, which may have to be
	 * regenerated later. As a practical matter, it is tough to find
	 * more than 31 distinct primes for modulus 512 or 61 primes for
	 * modulus 1024. The latter can take several hundred iterations
	 * and several minutes on a Sun Blade 1000.
	 */
	n = nkeys;
	fprintf(stderr,
	    "Generating MV parameters for %d keys (%d bits)...\n", n,
	    modulus / n);
	ctx = BN_CTX_new(); u = BN_new(); v = BN_new(); w = BN_new();
	b = BN_new(); b1 = BN_new();
	dsa = DSA_new();
	dsa->p = BN_new();
	dsa->q = BN_new();
	dsa->g = BN_new();
	s = malloc((n + 1) * sizeof(BIGNUM));
	s1 = malloc((n + 1) * sizeof(BIGNUM));
	for (j = 1; j <= n; j++)
		s1[j] = BN_new();
	temp = 0;
	for (j = 1; j <= n; j++) {
		while (1) {
			fprintf(stderr, "Birthdays %d\r", temp);
			BN_generate_prime(s1[j], modulus / n, 0, NULL,
			    NULL, NULL, NULL);
			for (i = 1; i < j; i++) {
				if (BN_cmp(s1[i], s1[j]) == 0)
					break;
			}
			if (i == j)
				break;
			temp++;
		}
	}
	fprintf(stderr, "Birthday keys rejected %d\n", temp);

	/*
	 * Compute the modulus q as the product of the primes. Compute
	 * the modulus p as 2 * q + 1 and test p for primality. If p
	 * is composite, replace one of the primes with a new distinct
	 * one and try again. Note that q will hardly be a secret since
	 * we have to reveal p to servers and clients. However,
	 * factoring q to find the primes should be adequately hard, as
	 * this is the same problem considered hard in RSA. Question: is
	 * it as hard to find n small prime factors totalling n bits as
	 * it is to find two large prime factors totalling n bits?
	 * Remember, the bad guy doesn't know n.
	 */
	temp = 0;
	while (1) {
		fprintf(stderr, "Duplicate keys rejected %d\r", ++temp);
		BN_one(dsa->q);
		for (j = 1; j <= n; j++)
			BN_mul(dsa->q, dsa->q, s1[j], ctx);
		BN_copy(dsa->p, dsa->q);
		BN_add(dsa->p, dsa->p, dsa->p);
		BN_add_word(dsa->p, 1);
		if (BN_is_prime(dsa->p, BN_prime_checks, NULL, ctx,
		    NULL))
			break;

		j = temp % n + 1;
		while (1) {
			BN_generate_prime(u, modulus / n, 0, 0, NULL,
			    NULL, NULL);
			for (i = 1; i <= n; i++) {
				if (BN_cmp(u, s1[i]) == 0)
					break;
			}
			if (i > n)
				break;
		}
		BN_copy(s1[j], u);
	}
	fprintf(stderr, "Duplicate keys rejected %d\n", temp);

	/*
	 * Compute the generator g using a random roll such that
	 * gcd(g, p - 1) = 1 and g^q = 1. This is a generator of p, not
	 * q.
	 */
	BN_copy(v, dsa->p);
	BN_sub_word(v, 1);
	while (1) {
		BN_rand(dsa->g, BN_num_bits(dsa->p) - 1, 0, 0);
		BN_mod(dsa->g, dsa->g, dsa->p, ctx);
		BN_gcd(u, dsa->g, v, ctx);
		if (!BN_is_one(u))
			continue;

		BN_mod_exp(u, dsa->g, dsa->q, dsa->p, ctx);
		if (BN_is_one(u))
			break;
	}

	/*
	 * Compute s[j] such that s[j] * s'[j] = s'[j] for all j. The
	 * easy way to do this is to compute q + s'[j] and divide the
	 * result by s'[j]. Exercise for the student: prove the
	 * remainder is always zero.
	 */
	for (j = 1; j <= n; j++) {
		s[j] = BN_new();
		BN_add(s[j], dsa->q, s1[j]);
		BN_div(s[j], u, s[j], s1[j], ctx);
	}

	/*
	 * Setup is now complete. Roll random polynomial roots x[j]
	 * (0 < x[j] < q) for all j. While it may not be strictly
	 * necessary, Make sure each root has no factors in common with
	 * q.
	 */
	fprintf(stderr,
	    "Generating polynomial coefficients for %d roots (%d bits)\n",
	    n, BN_num_bits(dsa->q)); 
	x = malloc((n + 1) * sizeof(BIGNUM));
	for (j = 1; j <= n; j++) {
		x[j] = BN_new();
		while (1) {
			BN_rand(x[j], BN_num_bits(dsa->q), 0, 0);
			BN_mod(x[j], x[j], dsa->q, ctx);
			BN_gcd(u, x[j], dsa->q, ctx);
			if (BN_is_one(u))
				break;
		}
	}

	/*
	 * Generate polynomial coefficients a[i] (i = 0...n) from the
	 * expansion of root products (x - x[j]) mod q for all j. The
	 * method is a present from Charlie Boncelet.
	 */
	a = malloc((n + 1) * sizeof(BIGNUM));
	for (i = 0; i <= n; i++) {
		a[i] = BN_new();
		BN_one(a[i]);
	}
	for (j = 1; j <= n; j++) {
		BN_zero(w);
		for (i = 0; i < j; i++) {
			BN_copy(u, dsa->q);
			BN_mod_mul(v, a[i], x[j], dsa->q, ctx);
			BN_sub(u, u, v);
			BN_add(u, u, w);
			BN_copy(w, a[i]);
			BN_mod(a[i], u, dsa->q, ctx);
		}
	}

	/*
	 * Generate g[i] = g^a[i] mod p for all i and the generator g.
	 */
	fprintf(stderr, "Generating g[i] parameters\n");
	g = malloc((n + 1) * sizeof(BIGNUM));
	for (i = 0; i <= n; i++) {
		g[i] = BN_new();
		BN_mod_exp(g[i], dsa->g, a[i], dsa->p, ctx);
	}

	/*
	 * Verify prod(g[i]^(a[i] x[j]^i)) = 1 for all i, j; otherwise,
	 * exit. Note the a[i] x[j]^i exponent is computed mod q, but
	 * the g[i] is computed mod p. also note the expression given in
	 * the paper is incorrect.
	 */
	temp = 1;
	for (j = 1; j <= n; j++) {
		BN_one(u);
		for (i = 0; i <= n; i++) {
			BN_set_word(v, i);
			BN_mod_exp(v, x[j], v, dsa->q, ctx);
			BN_mod_mul(v, v, a[i], dsa->q, ctx);
			BN_mod_exp(v, dsa->g, v, dsa->p, ctx);
			BN_mod_mul(u, u, v, dsa->p, ctx);
		}
		if (!BN_is_one(u))
			temp = 0;
	}
	fprintf(stderr,
	    "Confirm prod(g[i]^(x[j]^i)) = 1 for all i, j: %s\n", temp ?
	    "yes" : "no");
	if (!temp) {
		rval = -1;
		return (NULL);
	}

	/*
	 * Make private encryption key A. Keep it around for awhile,
	 * since it is expensive to compute.
	 */
	biga = BN_new();
	BN_one(biga);
	for (j = 1; j <= n; j++) {
		for (i = 0; i < n; i++) {
			BN_set_word(v, i);
			BN_mod_exp(v, x[j], v, dsa->q, ctx);
			BN_mod_exp(v, g[i], v, dsa->p, ctx);
			BN_mod_mul(biga, biga, v, dsa->p, ctx);
		}
	}

	/*
	 * Roll private random group key b mod q (0 < b < q), where
	 * gcd(b, q) = 1 to guarantee b^1 exists, then compute b^-1
	 * mod q. If b is changed, the client keys must be recomputed.
	 */
	while (1) {
		BN_rand(b, BN_num_bits(dsa->q), 0, 0);
		BN_mod(b, b, dsa->q, ctx);
		BN_gcd(u, b, dsa->q, ctx);
		if (BN_is_one(u))
			break;
	}
	BN_mod_inverse(b1, b, dsa->q, ctx);

	/*
	 * Make private client keys (xbar[j], xhat[j]) for all j. Note
	 * that the keys for the jth client involve s[j], but not s'[j]
	 * or the product s = prod(s'[j]) mod q, which is the enabling
	 * key.
	 */
	xbar = malloc((n + 1) * sizeof(BIGNUM));
	xhat = malloc((n + 1) * sizeof(BIGNUM));
	for (j = 1; j <= n; j++) {
		xbar[j] = BN_new(); xhat[j] = BN_new();
		BN_zero(xbar[j]);
		BN_set_word(v, n);
		for (i = 1; i <= n; i++) {
			if (i == j)
				continue;
			BN_mod_exp(u, x[i], v, dsa->q, ctx);
			BN_add(xbar[j], xbar[j], u);
		}
		BN_mod_mul(xbar[j], xbar[j], b1, dsa->q, ctx);
		BN_mod_exp(xhat[j], x[j], v, dsa->q, ctx);
		BN_mod_mul(xhat[j], xhat[j], s[j], dsa->q, ctx);
	}

	/*
	 * The enabling key is initially q by construction. We can
	 * revoke client j by dividing q by s'[j]. The quotient becomes
	 * the enabling key s. Note we always have to revoke one key;
	 * otherwise, the plaintext and cryptotext would be identical.
	 */
	ss = BN_new();
	BN_copy(ss, dsa->q);
	BN_div(ss, u, dsa->q, s1[n], ctx);

	/*
	 * Make private server encryption key E = A^s and public server
	 * keys gbar = g^s mod p and ghat = g^(s b) mod p. The (gbar,
	 * ghat) is the public key provided to the server, which uses it
	 * to compute the session encryption key and public key included
	 * in its messages. These values must be regenerated if the
	 * enabling key is changed.
	 */
	bige = BN_new(); gbar = BN_new(); ghat = BN_new();
	BN_mod_exp(bige, biga, ss, dsa->p, ctx);
	BN_mod_exp(gbar, dsa->g, ss, dsa->p, ctx);
	BN_mod_mul(v, ss, b, dsa->q, ctx);
	BN_mod_exp(ghat, dsa->g, v, dsa->p, ctx);

	/*
	 * We produce the key media in three steps. The first step is to
	 * generate the private values that do not depend on the
	 * enabling key. These include the server values p, q, g, b, A
	 * and the client values s'[j], xbar[j] and xhat[j] for each j.
	 * The p, xbar[j] and xhat[j] values are encoded in private
	 * files which are distributed to respective clients. The p, q,
	 * g, A and s'[j] values (will be) written to a secret file to
	 * be read back later.
	 *
	 * The secret file (will be) read back at some later time to
	 * enable/disable individual keys and generate/regenerate the
	 * enabling key s. The p, q, E, gbar and ghat values are written
	 * to a secret file to be read back later by the server.
	 *
	 * The server reads the secret file and rolls the session key
	 * k, which is used only once, then computes E^k, gbar^k and
	 * ghat^k. The E^k is the session encryption key. The encrypted
	 * data, gbar^k and ghat^k are transmtted to clients in an
	 * extension field. The client receives the message and computes
	 * x = (gbar^k)^xbar[j] (ghat^k)^xhat[j], finds the session
	 * encryption key E^k as the inverse x^-1 and decrypts the data.
	 */
	BN_copy(dsa->g, bige);
	dsa->priv_key = BN_dup(gbar);
	dsa->pub_key = BN_dup(ghat);

	/*
	 * Write the MV server parameters and keys as a DSA private key
	 * encoded in PEM.
	 *
	 * p	modulus p
	 * q	modulus q (used only to generate k)
	 * g	E mod p
	 * priv_key gbar mod p
	 * pub_key ghat mod p
	 */
	str = fheader("MVpar", trustname);
	pkey = EVP_PKEY_new();
	EVP_PKEY_assign_DSA(pkey, dsa);
	PEM_write_PrivateKey(str, pkey, passwd2 ? EVP_des_cbc() : NULL,
	    NULL, 0, NULL, passwd2);
	fclose(str);
	if (debug)
		DSA_print_fp(stdout, dsa, 0);
	fslink(id, trustname);

	/*
	 * Write the parameters and private key (xbar[j], xhat[j]) for
	 * all j as a DSA private key encoded in PEM. It is used only by
	 * the designated recipient(s) who pay a suitably outrageous fee
	 * for its use.
	 */
	sdsa = DSA_new();
	sdsa->p = BN_dup(dsa->p);
	sdsa->q = BN_dup(BN_value_one());
	sdsa->g = BN_dup(BN_value_one());
	sdsa->priv_key = BN_new();
	sdsa->pub_key = BN_new();
	for (j = 1; j <= n; j++) {
		BN_copy(sdsa->priv_key, xbar[j]);
		BN_copy(sdsa->pub_key, xhat[j]);
		BN_mod_exp(v, dsa->priv_key, sdsa->pub_key, dsa->p,
		    ctx);
		BN_mod_exp(u, dsa->pub_key, sdsa->priv_key, dsa->p,
		    ctx);
		BN_mod_mul(u, u, v, dsa->p, ctx);
		BN_mod_mul(u, u, dsa->g, dsa->p, ctx);
		BN_free(xbar[j]); BN_free(xhat[j]);
		BN_free(x[j]); BN_free(s[j]); BN_free(s1[j]);
		if (!BN_is_one(u)) {
			fprintf(stderr, "Revoke key %d\n", j);
			continue;
		}

		/*
		 * Write the client parameters as a DSA private key
		 * encoded in PEM. We don't make links for these.
		 *
		 * p	modulus p
		 * priv_key xbar[j] mod q
		 * pub_key xhat[j] mod q
		 * (remaining values are not used)
		 */
		sprintf(ident, "MVkey%d", j);
		str = fheader(ident, trustname);
		pkey1 = EVP_PKEY_new();
		EVP_PKEY_set1_DSA(pkey1, sdsa);
		PEM_write_PrivateKey(str, pkey1, passwd2 ?
		    EVP_des_cbc() : NULL, NULL, 0, NULL, passwd2);
		fclose(str);
		fprintf(stderr, "ntpkey_%s_%s.%lu\n", ident, trustname,
		    epoch + JAN_1970);
		if (debug)
			DSA_print_fp(stdout, sdsa, 0);
		EVP_PKEY_free(pkey1);
	}

	/*
	 * Free the countries.
	 */
	for (i = 0; i <= n; i++) {
		BN_free(a[i]);
		BN_free(g[i]);
	}
	BN_free(u); BN_free(v); BN_free(w); BN_CTX_free(ctx);
	BN_free(b); BN_free(b1); BN_free(biga); BN_free(bige);
	BN_free(ss); BN_free(gbar); BN_free(ghat);
	DSA_free(sdsa);

	/*
	 * Free the world.
	 */
	free(x); free(a); free(g); free(s); free(s1);
	free(xbar); free(xhat);
	return (pkey);
}


/*
 * Generate X509v3 scertificate.
 *
 * The certificate consists of the version number, serial number,
 * validity interval, issuer name, subject name and public key. For a
 * self-signed certificate, the issuer name is the same as the subject
 * name and these items are signed using the subject private key. The
 * validity interval extends from the current time to the same time one
 * year hence. For NTP purposes, it is convenient to use the NTP seconds
 * of the current time as the serial number.
 */
int
x509	(
	EVP_PKEY *pkey,		/* generic signature algorithm */
	const EVP_MD *md,	/* generic digest algorithm */
	char	*gqpub,		/* identity extension (hex string) */
	char	*exten		/* private cert extension */
	)
{
	X509	*cert;		/* X509 certificate */
	X509_NAME *subj;	/* distinguished (common) name */
	X509_EXTENSION *ex;	/* X509v3 extension */
	FILE	*str;		/* file handle */
	ASN1_INTEGER *serial;	/* serial number */
	const char *id;		/* digest/signature scheme name */
	char	pathbuf[MAXFILENAME + 1];

	/*
	 * Generate X509 self-signed certificate.
	 *
	 * Set the certificate serial to the NTP seconds for grins. Set
	 * the version to 3. Set the subject name and issuer name to the
	 * subject name in the request. Set the initial validity to the
	 * current time and the final validity one year hence. 
	 */
	id = OBJ_nid2sn(md->pkey_type);
	fprintf(stderr, "Generating certificate %s\n", id);
	cert = X509_new();
	X509_set_version(cert, 2L);
	serial = ASN1_INTEGER_new();
	ASN1_INTEGER_set(serial, epoch + JAN_1970);
	X509_set_serialNumber(cert, serial);
	ASN1_INTEGER_free(serial);
	X509_time_adj(X509_get_notBefore(cert), 0L, &epoch);
	X509_time_adj(X509_get_notAfter(cert), YEAR, &epoch);
	subj = X509_get_subject_name(cert);
	X509_NAME_add_entry_by_txt(subj, "commonName", MBSTRING_ASC,
	    (unsigned char *) hostname, strlen(hostname), -1, 0);
	subj = X509_get_issuer_name(cert);
	X509_NAME_add_entry_by_txt(subj, "commonName", MBSTRING_ASC,
	    (unsigned char *) trustname, strlen(trustname), -1, 0);
	if (!X509_set_pubkey(cert, pkey)) {
		fprintf(stderr, "Assign key fails\n%s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		X509_free(cert);
		rval = -1;
		return (0);
	}

	/*
	 * Add X509v3 extensions if present. These represent the minimum
	 * set defined in RFC3280 less the certificate_policy extension,
	 * which is seriously obfuscated in OpenSSL.
	 */
	/*
	 * The basic_constraints extension CA:TRUE allows servers to
	 * sign client certficitates.
	 */
	fprintf(stderr, "%s: %s\n", LN_basic_constraints,
	    BASIC_CONSTRAINTS);
	ex = X509V3_EXT_conf_nid(NULL, NULL, NID_basic_constraints,
	    BASIC_CONSTRAINTS);
	if (!X509_add_ext(cert, ex, -1)) {
		fprintf(stderr, "Add extension field fails\n%s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		rval = -1;
		return (0);
	}
	X509_EXTENSION_free(ex);

	/*
	 * The key_usage extension designates the purposes the key can
	 * be used for.
	 */
	fprintf(stderr, "%s: %s\n", LN_key_usage, KEY_USAGE);
	ex = X509V3_EXT_conf_nid(NULL, NULL, NID_key_usage, KEY_USAGE);
	if (!X509_add_ext(cert, ex, -1)) {
		fprintf(stderr, "Add extension field fails\n%s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		rval = -1;
		return (0);
	}
	X509_EXTENSION_free(ex);
	/*
	 * The subject_key_identifier is used for the GQ public key.
	 * This should not be controversial.
	 */
	if (gqpub != NULL) {
		fprintf(stderr, "%s\n", LN_subject_key_identifier);
		ex = X509V3_EXT_conf_nid(NULL, NULL,
		    NID_subject_key_identifier, gqpub);
		if (!X509_add_ext(cert, ex, -1)) {
			fprintf(stderr,
			    "Add extension field fails\n%s\n",
			    ERR_error_string(ERR_get_error(), NULL));
			rval = -1;
			return (0);
		}
		X509_EXTENSION_free(ex);
	}

	/*
	 * The extended key usage extension is used for special purpose
	 * here. The semantics probably do not conform to the designer's
	 * intent and will likely change in future.
	 * 
	 * "trustRoot" designates a root authority
	 * "private" designates a private certificate
	 */
	if (exten != NULL) {
		fprintf(stderr, "%s: %s\n", LN_ext_key_usage, exten);
		ex = X509V3_EXT_conf_nid(NULL, NULL,
		    NID_ext_key_usage, exten);
		if (!X509_add_ext(cert, ex, -1)) {
			fprintf(stderr,
			    "Add extension field fails\n%s\n",
			    ERR_error_string(ERR_get_error(), NULL));
			rval = -1;
			return (0);
		}
		X509_EXTENSION_free(ex);
	}

	/*
	 * Sign and verify.
	 */
	X509_sign(cert, pkey, md);
	if (!X509_verify(cert, pkey)) {
		fprintf(stderr, "Verify %s certificate fails\n%s\n", id,
		    ERR_error_string(ERR_get_error(), NULL));
		X509_free(cert);
		rval = -1;
		return (0);
	}

	/*
	 * Write the certificate encoded in PEM.
	 */
	sprintf(pathbuf, "%scert", id);
	str = fheader(pathbuf, hostname);
	PEM_write_X509(str, cert);
	fclose(str);
	if (debug)
		X509_print_fp(stdout, cert);
	X509_free(cert);
	fslink("cert", hostname);
	return (1);
}

#if 0	/* asn2ntp is not used */
/*
 * asn2ntp - convert ASN1_TIME time structure to NTP time
 */
u_long
asn2ntp	(
	ASN1_TIME *asn1time	/* pointer to ASN1_TIME structure */
	)
{
	char	*v;		/* pointer to ASN1_TIME string */
	struct	tm tm;		/* time decode structure time */

	/*
	 * Extract time string YYMMDDHHMMSSZ from ASN.1 time structure.
	 * Note that the YY, MM, DD fields start with one, the HH, MM,
	 * SS fiels start with zero and the Z character should be 'Z'
	 * for UTC. Also note that years less than 50 map to years
	 * greater than 100. Dontcha love ASN.1?
	 */
	if (asn1time->length > 13)
		return (-1);
	v = (char *)asn1time->data;
	tm.tm_year = (v[0] - '0') * 10 + v[1] - '0';
	if (tm.tm_year < 50)
		tm.tm_year += 100;
	tm.tm_mon = (v[2] - '0') * 10 + v[3] - '0' - 1;
	tm.tm_mday = (v[4] - '0') * 10 + v[5] - '0';
	tm.tm_hour = (v[6] - '0') * 10 + v[7] - '0';
	tm.tm_min = (v[8] - '0') * 10 + v[9] - '0';
	tm.tm_sec = (v[10] - '0') * 10 + v[11] - '0';
	tm.tm_wday = 0;
	tm.tm_yday = 0;
	tm.tm_isdst = 0;
	return (mktime(&tm) + JAN_1970);
}
#endif

/*
 * Callback routine
 */
void
cb	(
	int	n1,		/* arg 1 */
	int	n2,		/* arg 2 */
	void	*chr		/* arg 3 */
	)
{
	switch (n1) {
	case 0:
		d0++;
		fprintf(stderr, "%s %d %d %lu\r", (char *)chr, n1, n2,
		    d0);
		break;
	case 1:
		d1++;
		fprintf(stderr, "%s\t\t%d %d %lu\r", (char *)chr, n1,
		    n2, d1);
		break;
	case 2:
		d2++;
		fprintf(stderr, "%s\t\t\t\t%d %d %lu\r", (char *)chr,
		    n1, n2, d2);
		break;
	case 3:
		d3++;
		fprintf(stderr, "%s\t\t\t\t\t\t%d %d %lu\r",
		    (char *)chr, n1, n2, d3);
		break;
	}
}


/*
 * Generate key
 */
EVP_PKEY *			/* public/private key pair */
genkey(
	char	*type,		/* key type (RSA or DSA) */
	char	*id		/* file name id */
	)
{
	if (type == NULL)
		return (NULL);
	if (strcmp(type, "RSA") == 0)
		return (gen_rsa(id));

	else if (strcmp(type, "DSA") == 0)
		return (gen_dsa(id));

	fprintf(stderr, "Invalid %s key type %s\n", id, type);
	rval = -1;
	return (NULL);
}
#endif /* OPENSSL */


/*
 * Generate file header
 */
FILE *
fheader	(
	const char *id,		/* file name id */
	const char *name	/* owner name */
	)
{
	FILE	*str;		/* file handle */

	sprintf(filename, "ntpkey_%s_%s.%lu", id, name, epoch +
	    JAN_1970);
	if ((str = fopen(filename, "w")) == NULL) {
		perror("Write");
		exit (-1);
	}
	fprintf(str, "# %s\n# %s", filename, ctime(&epoch));
	return (str);
}


/*
 * Generate symbolic links
 */
void
fslink(
	const char *id,		/* file name id */
	const char *name	/* owner name */
	)
{
	char	linkname[MAXFILENAME]; /* link name */
	int	temp;

	sprintf(linkname, "ntpkey_%s_%s", id, name);
	remove(linkname);
	temp = symlink(filename, linkname);
	if (temp < 0)
		perror(id);
	fprintf(stderr, "Generating new %s file and link\n", id);
	fprintf(stderr, "%s->%s\n", linkname, filename);
}
