/*
 * Program to generate cryptographic keys for ntp clients and servers
 *
 * This program generates password encrypted data files for use with the
 * Autokey security protocol and Network Time Protocol Version 4. Files
 * are prefixed with a header giving the name and date of creation
 * followed by a type-specific descriptive label and PEM-encoded data
 * structure compatible with programs of the OpenSSL library.
 *
 * All file names are like "ntpkey_<type>_<hostname>.<filestamp>", where
 * <type> is the file type, <hostname> the generating host name and
 * <filestamp> the generation time in NTP seconds. The NTP programs
 * expect generic names such as "ntpkey_<type>_whimsy.udel.edu" with the
 * association maintained by soft links. Following is a list of file
 * types; the first line is the file name and the second link name.
 *
 * ntpkey_MD5key_<hostname>.<filestamp>
 * 	MD5 (128-bit) keys used to compute message digests in symmetric
 *	key cryptography
 *
 * ntpkey_RSAhost_<hostname>.<filestamp>
 * ntpkey_host_<hostname>
 *	RSA private/public host key pair used for public key signatures
 *
 * ntpkey_RSAsign_<hostname>.<filestamp>
 * ntpkey_sign_<hostname>
 *	RSA private/public sign key pair used for public key signatures
 *
 * ntpkey_DSAsign_<hostname>.<filestamp>
 * ntpkey_sign_<hostname>
 *	DSA Private/public sign key pair used for public key signatures
 *
 * Available digest/signature schemes
 *
 * RSA:	RSA-MD2, RSA-MD5, RSA-SHA, RSA-SHA1, RSA-MDC2, EVP-RIPEMD160
 * DSA:	DSA-SHA, DSA-SHA1
 *
 * ntpkey_XXXcert_<hostname>.<filestamp>
 * ntpkey_cert_<hostname>
 *	X509v3 certificate using RSA or DSA public keys and signatures.
 *	XXX is a code identifying the message digest and signature
 *	encryption algorithm
 *
 * Identity schemes. The key type par is used for the challenge; the key
 * type key is used for the response.
 *
 * ntpkey_IFFkey_<groupname>.<filestamp>
 * ntpkey_iffkey_<groupname>
 *	Schnorr (IFF) identity parameters and keys
 *
 * ntpkey_GQkey_<groupname>.<filestamp>,
 * ntpkey_gqkey_<groupname>
 *	Guillou-Quisquater (GQ) identity parameters and keys
 *
 * ntpkey_MVkeyX_<groupname>.<filestamp>,
 * ntpkey_mvkey_<groupname>
 *	Mu-Varadharajan (MV) identity parameters and keys
 *
 * Note: Once in a while because of some statistical fluke this program
 * fails to generate and verify some cryptographic data, as indicated by
 * exit status -1. In this case simply run the program again. If the
 * program does complete with exit code 0, the data are correct as
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
#include <sys/types.h>
#include "ntp_types.h"
#include "ntp_random.h"
#include "ntp_stdlib.h"
#include "ntp_assert.h"

#include "ntp_libopts.h"
#include "ntp-keygen-opts.h"

#ifdef OPENSSL
#include "openssl/bn.h"
#include "openssl/evp.h"
#include "openssl/err.h"
#include "openssl/rand.h"
#include "openssl/pem.h"
#include "openssl/x509v3.h"
#include <openssl/objects.h>
#endif /* OPENSSL */
#include <ssl_applink.c>

/*
 * Cryptodefines
 */
#define	MD5KEYS		10	/* number of keys generated of each type */
#define	MD5SIZE		20	/* maximum key size */
#define	JAN_1970	2208988800UL /* NTP seconds */
#define YEAR		((long)60*60*24*365) /* one year in seconds */
#define MAXFILENAME	256	/* max file name length */
#define MAXHOSTNAME	256	/* max host name length */
#ifdef OPENSSL
#define	PLEN		512	/* default prime modulus size (bits) */
#define	ILEN		256	/* default identity modulus size (bits) */
#define	MVMAX		100	/* max MV parameters */

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
FILE	*fheader	(const char *, const char *, const char *);
int	gen_md5		(char *);
#ifdef OPENSSL
EVP_PKEY *gen_rsa	(char *);
EVP_PKEY *gen_dsa	(char *);
EVP_PKEY *gen_iffkey	(char *);
EVP_PKEY *gen_gqkey	(char *);
EVP_PKEY *gen_mvkey	(char *, EVP_PKEY **);
void	gen_mvserv	(char *, EVP_PKEY **);
int	x509		(EVP_PKEY *, const EVP_MD *, char *, char *,
			    char *);
void	cb		(int, int, void *);
EVP_PKEY *genkey	(char *, char *);
EVP_PKEY *readkey	(char *, char *, u_int *, EVP_PKEY **);
void	writekey	(char *, char *, u_int *, EVP_PKEY **);
u_long	asn2ntp		(ASN1_TIME *);
#endif /* OPENSSL */

/*
 * Program variables
 */
extern char *optarg;		/* command line argument */
char	*progname;
volatile int	debug = 0;		/* debug, not de bug */
#ifdef OPENSSL
u_int	modulus = PLEN;		/* prime modulus size (bits) */
u_int	modulus2 = ILEN;	/* identity modulus size (bits) */
#endif
int	nkeys;			/* MV keys */
time_t	epoch;			/* Unix epoch (seconds) since 1970 */
u_int	fstamp;			/* NTP filestamp */
char	*hostname = NULL;	/* host name (subject name) */
char	*groupname = NULL;	/* trusted host name (issuer name) */
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
readlink(char *link, char *file, int len)
{
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
	return (0);
}
void
InitWin32Sockets() {
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2,0);
	if (WSAStartup(wVersionRequested, &wsaData))
	{
		fprintf(stderr, "No useable winsock.dll\n");
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
	X509_EXTENSION *ext;	/* X509v3 extension */
	EVP_PKEY *pkey_host = NULL; /* host key */
	EVP_PKEY *pkey_sign = NULL; /* sign key */
	EVP_PKEY *pkey_iffkey = NULL; /* IFF sever keys */
	EVP_PKEY *pkey_gqkey = NULL; /* GQ server keys */
	EVP_PKEY *pkey_mvkey = NULL; /* MV trusted agen keys */
	EVP_PKEY *pkey_mvpar[MVMAX]; /* MV cleient keys */
	int	hostkey = 0;	/* generate RSA keys */
	int	iffkey = 0;	/* generate IFF keys */
	int	gqkey = 0;	/* generate GQ keys */
	int	mvkey = 0;	/* update MV keys */
	int	mvpar = 0;	/* generate MV parameters */
	char	*sign = NULL;	/* sign key */
	EVP_PKEY *pkey = NULL;	/* temp key */
	const EVP_MD *ectx;	/* EVP digest */
	char	pathbuf[MAXFILENAME + 1];
	const char *scheme = NULL; /* digest/signature scheme */
	char	*exten = NULL;	/* private extension */
	char	*grpkey = NULL;	/* identity extension */
	int	nid;		/* X509 digest/signature scheme */
	FILE	*fstr = NULL;	/* file handle */
#define iffsw   HAVE_OPT(ID_KEY)
#endif /* OPENSSL */
	char	hostbuf[MAXHOSTNAME + 1];
	char	groupbuf[MAXHOSTNAME + 1];

	progname = argv[0];

#ifdef SYS_WINNT
	/* Initialize before OpenSSL checks */
	InitWin32Sockets();
	if (!init_randfile())
		fprintf(stderr, "Unable to initialize .rnd file\n");
	ssl_applink();
#endif

#ifdef OPENSSL
	ssl_check_version();
#endif /* OPENSSL */

	/*
	 * Process options, initialize host name and timestamp.
	 */
	gethostname(hostbuf, MAXHOSTNAME);
	hostname = hostbuf;
	gettimeofday(&tv, 0);

	epoch = tv.tv_sec;

	{
		int optct = ntpOptionProcess(&ntp_keygenOptions,
					     argc, argv);
		argc -= optct;
		argv += optct;
	}

#ifdef OPENSSL
	if (SSLeay() == SSLEAY_VERSION_NUMBER)
		fprintf(stderr, "Using OpenSSL version %s\n",
			SSLeay_version(SSLEAY_VERSION));
	else
		fprintf(stderr, "Built against OpenSSL %s, using version %s\n",
			OPENSSL_VERSION_TEXT, SSLeay_version(SSLEAY_VERSION));
#endif /* OPENSSL */

	debug = DESC(DEBUG_LEVEL).optOccCt;
	if (HAVE_OPT( MD5KEY ))
		md5key++;

#ifdef OPENSSL
	passwd1 = hostbuf;
	if (HAVE_OPT( PVT_PASSWD ))
		passwd1 = strdup(OPT_ARG( PVT_PASSWD ));

	if (HAVE_OPT( GET_PVT_PASSWD ))
		passwd2 = strdup(OPT_ARG( GET_PVT_PASSWD ));

	if (HAVE_OPT( HOST_KEY ))
		hostkey++;

	if (HAVE_OPT( SIGN_KEY ))
		sign = strdup(OPT_ARG( SIGN_KEY ));

	if (HAVE_OPT( GQ_PARAMS ))
		gqkey++;

	if (HAVE_OPT( IFFKEY ))
		iffkey++;

	if (HAVE_OPT( MV_PARAMS )) {
		mvkey++;
		nkeys = OPT_VALUE_MV_PARAMS;
	}
	if (HAVE_OPT( MV_KEYS )) {
		mvpar++;
		nkeys = OPT_VALUE_MV_KEYS;
	}
	if (HAVE_OPT( MODULUS ))
		modulus = OPT_VALUE_MODULUS;

	if (HAVE_OPT( CERTIFICATE ))
		scheme = OPT_ARG( CERTIFICATE );

	if (HAVE_OPT( SUBJECT_NAME ))
		hostname = strdup(OPT_ARG( SUBJECT_NAME ));

	if (HAVE_OPT( ISSUER_NAME ))
		groupname = strdup(OPT_ARG( ISSUER_NAME ));

	if (HAVE_OPT( PVT_CERT ))
		exten = EXT_KEY_PRIVATE;

	if (HAVE_OPT( TRUSTED_CERT ))
		exten = EXT_KEY_TRUST;

	/*
	 * Seed random number generator and grow weeds.
	 */
	ERR_load_crypto_strings();
	OpenSSL_add_all_algorithms();
	if (!RAND_status()) {
		u_int	temp;

		if (RAND_file_name(pathbuf, MAXFILENAME) == NULL) {
			fprintf(stderr, "RAND_file_name %s\n",
			    ERR_error_string(ERR_get_error(), NULL));
			exit (-1);
		}
		temp = RAND_load_file(pathbuf, -1);
		if (temp == 0) {
			fprintf(stderr,
			    "RAND_load_file %s not found or empty\n",
			    pathbuf);
			exit (-1);
		}
		fprintf(stderr,
		    "Random seed file %s %u bytes\n", pathbuf, temp);
		RAND_add(&epoch, sizeof(epoch), 4.0);
	}

	/*
	 * Load previous certificate if available.
	 */
	sprintf(filename, "ntpkey_cert_%s", hostname);
	if ((fstr = fopen(filename, "r")) != NULL) {
		cert = PEM_read_X509(fstr, NULL, NULL, NULL);
		fclose(fstr);
	}
	if (cert != NULL) {

		/*
		 * Extract subject name.
		 */
		X509_NAME_oneline(X509_get_subject_name(cert), groupbuf,
		    MAXFILENAME);

		/*
		 * Extract digest/signature scheme.
		 */
		if (scheme == NULL) {
			nid = OBJ_obj2nid(cert->cert_info->
			    signature->algorithm);
			scheme = OBJ_nid2sn(nid);
		}

		/*
		 * If a key_usage extension field is present, determine
		 * whether this is a trusted or private certificate.
		 */
		if (exten == NULL) {
			BIO	*bp;
			int	i, cnt;
			char	*ptr;

			ptr = strstr(groupbuf, "CN=");
			cnt = X509_get_ext_count(cert);
			for (i = 0; i < cnt; i++) {
				ext = X509_get_ext(cert, i);
				if (OBJ_obj2nid(ext->object) ==
				    NID_ext_key_usage) {
					bp = BIO_new(BIO_s_mem());
					X509V3_EXT_print(bp, ext, 0, 0);
					BIO_gets(bp, pathbuf,
					    MAXFILENAME);
					BIO_free(bp);
					if (strcmp(pathbuf,
					    "Trust Root") == 0)
						exten = EXT_KEY_TRUST;
					else if (strcmp(pathbuf,
					    "Private") == 0)
						exten = EXT_KEY_PRIVATE;
					if (groupname == NULL)
						groupname = ptr + 3;
				}
			}
		}
	}
	if (scheme == NULL)
		scheme = "RSA-MD5";
	if (groupname == NULL)
		groupname = hostname;
	fprintf(stderr, "Using host %s group %s\n", hostname,
	    groupname);
	if ((iffkey || gqkey || mvkey) && exten == NULL)
		fprintf(stderr,
		    "Warning: identity files may not be useful with a nontrusted certificate.\n");
#endif /* OPENSSL */

	/*
	 * Create new unencrypted MD5 keys file if requested. If this
	 * option is selected, ignore all other options.
	 */
	if (md5key) {
		gen_md5("md5");
		exit (0);
	}

#ifdef OPENSSL
	/*
	 * Create a new encrypted RSA host key file if requested;
	 * otherwise, look for an existing host key file. If not found,
	 * create a new encrypted RSA host key file. If that fails, go
	 * no further.
	 */
	if (hostkey)
		pkey_host = genkey("RSA", "host");
	if (pkey_host == NULL) {
		sprintf(filename, "ntpkey_host_%s", hostname);
		pkey_host = readkey(filename, passwd1, &fstamp, NULL);
		if (pkey_host != NULL) {
			readlink(filename, filename, sizeof(filename));
			fprintf(stderr, "Using host key %s\n",
			    filename);
		} else {
			pkey_host = genkey("RSA", "host");
		}
	}
	if (pkey_host == NULL) {
		fprintf(stderr, "Generating host key fails\n");
		exit (-1);
	}

	/*
	 * Create new encrypted RSA or DSA sign keys file if requested;
	 * otherwise, look for an existing sign key file. If not found,
	 * use the host key instead.
	 */
	if (sign != NULL)
		pkey_sign = genkey(sign, "sign");
	if (pkey_sign == NULL) {
		sprintf(filename, "ntpkey_sign_%s", hostname);
		pkey_sign = readkey(filename, passwd1, &fstamp, NULL);
		if (pkey_sign != NULL) {
			readlink(filename, filename, sizeof(filename));
			fprintf(stderr, "Using sign key %s\n",
			    filename);
		} else if (pkey_host != NULL) {
			pkey_sign = pkey_host;
			fprintf(stderr, "Using host key as sign key\n");
		}
	}

	/*
	 * Create new encrypted GQ server keys file if requested;
	 * otherwise, look for an exisiting file. If found, fetch the
	 * public key for the certificate.
	 */
	if (gqkey)
		pkey_gqkey = gen_gqkey("gqkey");
	if (pkey_gqkey == NULL) {
		sprintf(filename, "ntpkey_gqkey_%s", groupname);
		pkey_gqkey = readkey(filename, passwd1, &fstamp, NULL);
		if (pkey_gqkey != NULL) {
			readlink(filename, filename, sizeof(filename));
			fprintf(stderr, "Using GQ parameters %s\n",
			    filename);
		}
	}
	if (pkey_gqkey != NULL)
		grpkey = BN_bn2hex(pkey_gqkey->pkey.rsa->q);

	/*
	 * Write the nonencrypted GQ client parameters to the stdout
	 * stream. The parameter file is the server key file with the
	 * private key obscured.
	 */
	if (pkey_gqkey != NULL && HAVE_OPT(ID_KEY)) {
		RSA	*rsa;

		epoch = fstamp - JAN_1970;
		sprintf(filename, "ntpkey_gqpar_%s.%u", groupname,
		    fstamp);
		fprintf(stderr, "Writing GQ parameters %s to stdout\n",
		    filename);
		fprintf(stdout, "# %s\n# %s\n", filename,
		    ctime(&epoch));
		rsa = pkey_gqkey->pkey.rsa;
		BN_copy(rsa->p, BN_value_one());
		BN_copy(rsa->q, BN_value_one());
		pkey = EVP_PKEY_new();
		EVP_PKEY_assign_RSA(pkey, rsa);
		PEM_write_PrivateKey(stdout, pkey, NULL, NULL, 0, NULL,
		    NULL);
		fclose(stdout);
		if (debug)
			RSA_print_fp(stderr, rsa, 0);
	}

	/*
	 * Write the encrypted GQ server keys to the stdout stream.
	 */
	if (pkey_gqkey != NULL && passwd2 != NULL) {
		RSA	*rsa;

		sprintf(filename, "ntpkey_gqkey_%s.%u", groupname,
		    fstamp);
		fprintf(stderr, "Writing GQ keys %s to stdout\n",
		    filename);
		fprintf(stdout, "# %s\n# %s\n", filename,
		    ctime(&epoch));
		rsa = pkey_gqkey->pkey.rsa;
		pkey = EVP_PKEY_new();
		EVP_PKEY_assign_RSA(pkey, rsa);
		PEM_write_PrivateKey(stdout, pkey,
		    EVP_des_cbc(), NULL, 0, NULL, passwd2);
		fclose(stdout);
		if (debug)
			RSA_print_fp(stderr, rsa, 0);
	}

	/*
	 * Create new encrypted IFF server keys file if requested;
	 * otherwise, look for existing file.
	 */
	if (iffkey)
		pkey_iffkey = gen_iffkey("iffkey");
	if (pkey_iffkey == NULL) {
		sprintf(filename, "ntpkey_iffkey_%s", groupname);
		pkey_iffkey = readkey(filename, passwd1, &fstamp, NULL);
		if (pkey_iffkey != NULL) {
			readlink(filename, filename, sizeof(filename));
			fprintf(stderr, "Using IFF keys %s\n",
			    filename);
		}
	}

	/*
	 * Write the nonencrypted IFF client parameters to the stdout
	 * stream. The parameter file is the server key file with the
	 * private key obscured.
	 */
	if (pkey_iffkey != NULL && HAVE_OPT(ID_KEY)) {
		DSA	*dsa;

		epoch = fstamp - JAN_1970;
		sprintf(filename, "ntpkey_iffpar_%s.%u", groupname,
		    fstamp);
		fprintf(stderr, "Writing IFF parameters %s to stdout\n",
		    filename);
		fprintf(stdout, "# %s\n# %s\n", filename,
		    ctime(&epoch));
		dsa = pkey_iffkey->pkey.dsa;
		BN_copy(dsa->priv_key, BN_value_one());
		pkey = EVP_PKEY_new();
		EVP_PKEY_assign_DSA(pkey, dsa);
		PEM_write_PrivateKey(stdout, pkey, NULL, NULL, 0, NULL,
		    NULL);
		fclose(stdout);
		if (debug)
			DSA_print_fp(stderr, dsa, 0);
	}

	/*
	 * Write the encrypted IFF server keys to the stdout stream.
	 */
	if (pkey_iffkey != NULL && passwd2 != NULL) {
		DSA	*dsa;

		epoch = fstamp - JAN_1970;
		sprintf(filename, "ntpkey_iffkey_%s.%u", groupname,
		    fstamp);
		fprintf(stderr, "Writing IFF keys %s to stdout\n",
		    filename);
		fprintf(stdout, "# %s\n# %s\n", filename,
		    ctime(&epoch));
		dsa = pkey_iffkey->pkey.dsa;
		pkey = EVP_PKEY_new();
		EVP_PKEY_assign_DSA(pkey, dsa);
		PEM_write_PrivateKey(stdout, pkey, EVP_des_cbc(), NULL,
		    0, NULL, passwd2);
		fclose(stdout);
		if (debug)
			DSA_print_fp(stderr, dsa, 0);
	}

	/*
	 * Create new encrypted MV trusted-authority keys file if
	 * requested; otherwise, look for existing keys file.
	 */
	if (mvkey)
		pkey_mvkey = gen_mvkey("mv", pkey_mvpar);
	if (pkey_mvkey == NULL) {
		sprintf(filename, "ntpkey_mvta_%s", groupname);
		pkey_mvkey = readkey(filename, passwd1, &fstamp,
		   pkey_mvpar);
		if (pkey_mvkey != NULL) {
			readlink(filename, filename, sizeof(filename));
			fprintf(stderr, "Using MV keys %s\n",
			    filename);
		}
	}

	/*
	 * Write the nonencrypted MV client parameters to the stdout
	 * stream. For the moment, we always use the client parameters
	 * associated with client key 1.
	 */
	if (pkey_mvkey != NULL && HAVE_OPT(ID_KEY)) {
		epoch = fstamp - JAN_1970;
		sprintf(filename, "ntpkey_mvpar_%s.%u", groupname,
		    fstamp);
		fprintf(stderr, "Writing MV parameters %s to stdout\n",
		    filename);
		fprintf(stdout, "# %s\n# %s\n", filename,
		    ctime(&epoch));
		pkey = pkey_mvpar[2];
		PEM_write_PrivateKey(stdout, pkey, NULL, NULL, 0, NULL,
		    NULL);
		fclose(stdout);
		if (debug)
			DSA_print_fp(stderr, pkey->pkey.dsa, 0);
	}

	/*
	 * Write the encrypted MV server keys to the stdout stream.
	 */
	if (pkey_mvkey != NULL && passwd2 != NULL) {
		epoch = fstamp - JAN_1970;
		sprintf(filename, "ntpkey_mvkey_%s.%u", groupname,
		    fstamp);
		fprintf(stderr, "Writing MV keys %s to stdout\n",
		    filename);
		fprintf(stdout, "# %s\n# %s\n", filename,
		    ctime(&epoch));
		pkey = pkey_mvpar[1];
		PEM_write_PrivateKey(stdout, pkey, EVP_des_cbc(), NULL,
		    0, NULL, passwd2);
		fclose(stdout);
		if (debug)
			DSA_print_fp(stderr, pkey->pkey.dsa, 0);
	}

	/*
	 * Don't generate a certificate if no host keys or extracting
	 * encrypted or nonencrypted keys to the standard output stream.
	 */
	if (pkey_host == NULL || HAVE_OPT(ID_KEY) || passwd2 != NULL)
		exit (0);

	/*
	 * Decode the digest/signature scheme. If trusted, set the
	 * subject and issuer names to the group name; if not set both
	 * to the host name.
	 */
	ectx = EVP_get_digestbyname(scheme);
	if (ectx == NULL) {
		fprintf(stderr,
		    "Invalid digest/signature combination %s\n",
		    scheme);
			exit (-1);
	}
	if (exten == NULL)
		x509(pkey_sign, ectx, grpkey, exten, hostname);
	else
		x509(pkey_sign, ectx, grpkey, exten, groupname);
#endif /* OPENSSL */
	exit (0);
}


/*
 * Generate semi-random MD5 keys compatible with NTPv3 and NTPv4. Also,
 * if OpenSSL is around, generate random SHA1 keys compatible with
 * symmetric key cryptography.
 */
int
gen_md5(
	char	*id		/* file name id */
	)
{
	u_char	md5key[MD5SIZE + 1];	/* MD5 key */
	FILE	*str;
	int	i, j;
#ifdef OPENSSL
	u_char	keystr[MD5SIZE];
	u_char	hexstr[2 * MD5SIZE + 1];
	u_char	hex[] = "0123456789abcdef";
#endif /* OPENSSL */

	str = fheader("MD5key", id, groupname);
	ntp_srandom((u_long)epoch);
	for (i = 1; i <= MD5KEYS; i++) {
		for (j = 0; j < MD5SIZE; j++) {
			int temp;

			while (1) {
				temp = ntp_random() & 0xff;
				if (temp == '#')
					continue;

				if (temp > 0x20 && temp < 0x7f)
					break;
			}
			md5key[j] = (u_char)temp;
		}
		md5key[j] = '\0';
		fprintf(str, "%2d MD5 %s  # MD5 key\n", i,
		    md5key);
	}
#ifdef OPENSSL
	for (i = 1; i <= MD5KEYS; i++) {
		RAND_bytes(keystr, 20);
		for (j = 0; j < MD5SIZE; j++) {
			hexstr[2 * j] = hex[keystr[j] >> 4];
			hexstr[2 * j + 1] = hex[keystr[j] & 0xf];
		}
		hexstr[2 * MD5SIZE] = '\0';
		fprintf(str, "%2d SHA1 %s  # SHA1 key\n", i + MD5KEYS,
		    hexstr);
	}
#endif /* OPENSSL */
	fclose(str);
	return (1);
}


#ifdef OPENSSL
/*
 * readkey - load cryptographic parameters and keys
 *
 * This routine loads a PEM-encoded file of given name and password and
 * extracts the filestamp from the file name. It returns a pointer to
 * the first key if valid, NULL if not.
 */
EVP_PKEY *			/* public/private key pair */
readkey(
	char	*cp,		/* file name */
	char	*passwd,	/* password */
	u_int	*estamp,	/* file stamp */
	EVP_PKEY **evpars	/* parameter list pointer */
	)
{
	FILE	*str;		/* file handle */
	EVP_PKEY *pkey = NULL;	/* public/private key */
	u_int	gstamp;		/* filestamp */
	char	linkname[MAXFILENAME]; /* filestamp buffer) */
	EVP_PKEY *parkey;
	char	*ptr;
	int	i;

	/*
	 * Open the key file.
	 */
	str = fopen(cp, "r");
	if (str == NULL)
		return (NULL);

	/*
	 * Read the filestamp, which is contained in the first line.
	 */
	if ((ptr = fgets(linkname, MAXFILENAME, str)) == NULL) {
		fprintf(stderr, "Empty key file %s\n", cp);
		fclose(str);
		return (NULL);
	}
	if ((ptr = strrchr(ptr, '.')) == NULL) {
		fprintf(stderr, "No filestamp found in %s\n", cp);
		fclose(str);
		return (NULL);
	}
	if (sscanf(++ptr, "%u", &gstamp) != 1) {
		fprintf(stderr, "Invalid filestamp found in %s\n", cp);
		fclose(str);
		return (NULL);
	}

	/*
	 * Read and decrypt PEM-encoded private keys. The first one
	 * found is returned. If others are expected, add them to the
	 * parameter list.
	 */
	for (i = 0; i <= MVMAX - 1;) {
		parkey = PEM_read_PrivateKey(str, NULL, NULL, passwd);
		if (evpars != NULL) {
			evpars[i++] = parkey;
			evpars[i] = NULL;
		}
		if (parkey == NULL)
			break;

		if (pkey == NULL)
			pkey = parkey;
		if (debug) {
			if (parkey->type == EVP_PKEY_DSA)
				DSA_print_fp(stderr, parkey->pkey.dsa,
				    0);
			else if (parkey->type == EVP_PKEY_RSA)
				RSA_print_fp(stderr, parkey->pkey.rsa,
				    0);
		}
	}
	fclose(str);
	if (pkey == NULL) {
		fprintf(stderr, "Corrupt file %s or wrong key %s\n%s\n",
		    cp, passwd, ERR_error_string(ERR_get_error(),
		    NULL));
		exit (-1);
	}
	*estamp = gstamp;
	return (pkey);
}


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
		return (NULL);
	}

	/*
	 * Write the RSA parameters and keys as a RSA private key
	 * encoded in PEM.
	 */
	if (strcmp(id, "sign") == 0)
		str = fheader("RSAsign", id, hostname);
	else
		str = fheader("RSAhost", id, hostname);
	pkey = EVP_PKEY_new();
	EVP_PKEY_assign_RSA(pkey, rsa);
	PEM_write_PrivateKey(str, pkey, EVP_des_cbc(), NULL, 0, NULL,
	    passwd1);
	fclose(str);
	if (debug)
		RSA_print_fp(stderr, rsa, 0);
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
		return (NULL);
	}

	/*
	 * Write the DSA parameters and keys as a DSA private key
	 * encoded in PEM.
	 */
	str = fheader("DSAsign", id, hostname);
	pkey = EVP_PKEY_new();
	EVP_PKEY_assign_DSA(pkey, dsa);
	PEM_write_PrivateKey(str, pkey, EVP_des_cbc(), NULL, 0, NULL,
	    passwd1);
	fclose(str);
	if (debug)
		DSA_print_fp(stderr, dsa, 0);
	return (pkey);
}


/*
 ***********************************************************************
 *								       *
 * The following routines implement the Schnorr (IFF) identity scheme  *
 *								       *
 ***********************************************************************
 *
 * The Schnorr (IFF) identity scheme is intended for use when
 * certificates are generated by some other trusted certificate
 * authority and the certificate cannot be used to convey public
 * parameters. There are two kinds of files: encrypted server files that
 * contain private and public values and nonencrypted client files that
 * contain only public values. New generations of server files must be
 * securely transmitted to all servers of the group; client files can be
 * distributed by any means. The scheme is self contained and
 * independent of new generations of host keys, sign keys and
 * certificates.
 *
 * The IFF values hide in a DSA cuckoo structure which uses the same
 * parameters. The values are used by an identity scheme based on DSA
 * cryptography and described in Stimson p. 285. The p is a 512-bit
 * prime, g a generator of Zp* and q a 160-bit prime that divides p - 1
 * and is a qth root of 1 mod p; that is, g^q = 1 mod p. The TA rolls a
 * private random group key b (0 < b < q) and public key v = g^b, then
 * sends (p, q, g, b) to the servers and (p, q, g, v) to the clients.
 * Alice challenges Bob to confirm identity using the protocol described
 * below.
 *
 * How it works
 *
 * The scheme goes like this. Both Alice and Bob have the public primes
 * p, q and generator g. The TA gives private key b to Bob and public
 * key v to Alice.
 *
 * Alice rolls new random challenge r (o < r < q) and sends to Bob in
 * the IFF request message. Bob rolls new random k (0 < k < q), then
 * computes y = k + b r mod q and x = g^k mod p and sends (y, hash(x))
 * to Alice in the response message. Besides making the response
 * shorter, the hash makes it effectivey impossible for an intruder to
 * solve for b by observing a number of these messages.
 * 
 * Alice receives the response and computes g^y v^r mod p. After a bit
 * of algebra, this simplifies to g^k. If the hash of this result
 * matches hash(x), Alice knows that Bob has the group key b. The signed
 * response binds this knowledge to Bob's private key and the public key
 * previously received in his certificate.
 */
/*
 * Generate Schnorr (IFF) keys.
 */
EVP_PKEY *			/* DSA cuckoo nest */
gen_iffkey(
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
	fprintf(stderr, "Generating IFF keys (%d bits)...\n",
	    modulus2);
	RAND_bytes(seed, sizeof(seed));
	dsa = DSA_generate_parameters(modulus2, seed, sizeof(seed), NULL,
	    NULL, cb, "IFF");
	fprintf(stderr, "\n");
	if (dsa == NULL) {
		fprintf(stderr, "DSA generate parameters fails\n%s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		return (NULL);;
	}

	/*
	 * Generate the private and public keys. The DSA parameters and
	 * private key are distributed to the servers, while all except
	 * the private key are distributed to the clients.
	 */
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
		return (NULL);
	}
	dsa->priv_key = BN_dup(b);		/* private key */
	dsa->pub_key = BN_dup(v);		/* public key */

	/*
	 * Here is a trial round of the protocol. First, Alice rolls
	 * random nonce r mod q and sends it to Bob. She needs only
	 * q from parameters.
	 */
	BN_rand(r, BN_num_bits(dsa->q), -1, 0);	/* r */
	BN_mod(r, r, dsa->q, ctx);

	/*
	 * Bob rolls random nonce k mod q, computes y = k + b r mod q
	 * and x = g^k mod p, then sends (y, x) to Alice. He needs
	 * p, q and b from parameters and r from Alice.
	 */
	BN_rand(k, BN_num_bits(dsa->q), -1, 0);	/* k, 0 < k < q  */
	BN_mod(k, k, dsa->q, ctx);
	BN_mod_mul(v, dsa->priv_key, r, dsa->q, ctx); /* b r mod q */
	BN_add(v, v, k);
	BN_mod(v, v, dsa->q, ctx);		/* y = k + b r mod q */
	BN_mod_exp(u, dsa->g, k, dsa->p, ctx);	/* x = g^k mod p */

	/*
	 * Alice verifies x = g^y v^r to confirm that Bob has group key
	 * b. She needs p, q, g from parameters, (y, x) from Bob and the
	 * original r. We omit the detail here thatt only the hash of y
	 * is sent.
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
		return (NULL);
	}

	/*
	 * Write the IFF keys as an encrypted DSA private key encoded in
	 * PEM.
	 *
	 * p	modulus p
	 * q	modulus q
	 * g	generator g
	 * priv_key b
	 * public_key v
	 * kinv	not used
	 * r	not used
	 */
	str = fheader("IFFkey", id, groupname);
	pkey = EVP_PKEY_new();
	EVP_PKEY_assign_DSA(pkey, dsa);
	PEM_write_PrivateKey(str, pkey, EVP_des_cbc(), NULL, 0, NULL,
	    passwd1);
	fclose(str);
	if (debug)
		DSA_print_fp(stderr, dsa, 0);
	return (pkey);
}


/*
 ***********************************************************************
 *								       *
 * The following routines implement the Guillou-Quisquater (GQ)        *
 * identity scheme                                                     *
 *								       *
 ***********************************************************************
 *
 * The Guillou-Quisquater (GQ) identity scheme is intended for use when
 * the certificate can be used to convey public parameters. The scheme
 * uses a X509v3 certificate extension field do convey the public key of
 * a private key known only to servers. There are two kinds of files:
 * encrypted server files that contain private and public values and
 * nonencrypted client files that contain only public values. New
 * generations of server files must be securely transmitted to all
 * servers of the group; client files can be distributed by any means.
 * The scheme is self contained and independent of new generations of
 * host keys and sign keys. The scheme is self contained and independent
 * of new generations of host keys and sign keys.
 *
 * The GQ parameters hide in a RSA cuckoo structure which uses the same
 * parameters. The values are used by an identity scheme based on RSA
 * cryptography and described in Stimson p. 300 (with errors). The 512-
 * bit public modulus is n = p q, where p and q are secret large primes.
 * The TA rolls private random group key b as RSA exponent. These values
 * are known to all group members.
 *
 * When rolling new certificates, a server recomputes the private and
 * public keys. The private key u is a random roll, while the public key
 * is the inverse obscured by the group key v = (u^-1)^b. These values
 * replace the private and public keys normally generated by the RSA
 * scheme. Alice challenges Bob to confirm identity using the protocol
 * described below.
 *
 * How it works
 *
 * The scheme goes like this. Both Alice and Bob have the same modulus n
 * and some random b as the group key. These values are computed and
 * distributed in advance via secret means, although only the group key
 * b is truly secret. Each has a private random private key u and public
 * key (u^-1)^b, although not necessarily the same ones. Bob and Alice
 * can regenerate the key pair from time to time without affecting
 * operations. The public key is conveyed on the certificate in an
 * extension field; the private key is never revealed.
 *
 * Alice rolls new random challenge r and sends to Bob in the GQ
 * request message. Bob rolls new random k, then computes y = k u^r mod
 * n and x = k^b mod n and sends (y, hash(x)) to Alice in the response
 * message. Besides making the response shorter, the hash makes it
 * effectivey impossible for an intruder to solve for b by observing
 * a number of these messages.
 * 
 * Alice receives the response and computes y^b v^r mod n. After a bit
 * of algebra, this simplifies to k^b. If the hash of this result
 * matches hash(x), Alice knows that Bob has the group key b. The signed
 * response binds this knowledge to Bob's private key and the public key
 * previously received in his certificate.
 */
/*
 * Generate Guillou-Quisquater (GQ) parameters file.
 */
EVP_PKEY *			/* RSA cuckoo nest */
gen_gqkey(
	char	*id		/* file name id */
	)
{
	EVP_PKEY *pkey;		/* private key */
	RSA	*rsa;		/* RSA parameters */
	BN_CTX	*ctx;		/* BN working space */
	BIGNUM	*u, *v, *g, *k, *r, *y; /* BN temps */
	FILE	*str;
	u_int	temp;

	/*
	 * Generate RSA parameters for use as GQ parameters.
	 */
	fprintf(stderr,
	    "Generating GQ parameters (%d bits)...\n",
	     modulus2);
	rsa = RSA_generate_key(modulus2, 3, cb, "GQ");
	fprintf(stderr, "\n");
	if (rsa == NULL) {
		fprintf(stderr, "RSA generate keys fails\n%s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		return (NULL);
	}
	ctx = BN_CTX_new(); u = BN_new(); v = BN_new();
	g = BN_new(); k = BN_new(); r = BN_new(); y = BN_new();

	/*
	 * Generate the group key b, which is saved in the e member of
	 * the RSA structure. The group key is transmitted to each group
	 * member encrypted by the member private key.
	 */
	ctx = BN_CTX_new();
	BN_rand(rsa->e, BN_num_bits(rsa->n), -1, 0); /* b */
	BN_mod(rsa->e, rsa->e, rsa->n, ctx);

	/*
	 * When generating his certificate, Bob rolls random private key
	 * u, then computes inverse v = u^-1. 
	 */
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
		return (NULL);
	}
	BN_copy(rsa->p, u);			/* private key */
	BN_copy(rsa->q, v);			/* public key */

	/*
	 * Here is a trial run of the protocol. First, Alice rolls
	 * random nonce r mod n and sends it to Bob. She needs only n
	 * from parameters.
	 */
	BN_rand(r, BN_num_bits(rsa->n), -1, 0);	/* r */
	BN_mod(r, r, rsa->n, ctx);

	/*
	 * Bob rolls random nonce k mod n, computes y = k u^r mod n and
	 * g = k^b mod n, then sends (y, g) to Alice. He needs n, u, b
	 * from parameters and r from Alice. 
	 */
	BN_rand(k, BN_num_bits(rsa->n), -1, 0);	/* k */
	BN_mod(k, k, rsa->n, ctx);
	BN_mod_exp(y, rsa->p, r, rsa->n, ctx);	/* u^r mod n */
	BN_mod_mul(y, k, y, rsa->n, ctx);	/* y = k u^r mod n */
	BN_mod_exp(g, k, rsa->e, rsa->n, ctx);	/* g = k^b mod n */

	/*
	 * Alice verifies g = v^r y^b mod n to confirm that Bob has
	 * private key u. She needs n, g from parameters, public key v =
	 * (u^-1)^b from the certificate, (y, g) from Bob and the
	 * original r. We omit the detaul here that only the hash of g
	 * is sent.
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
		return (NULL);
	}

	/*
	 * Write the GQ parameter file as an encrypted RSA private key
	 * encoded in PEM.
	 *
	 * n	modulus n
	 * e	group key b
	 * d	not used
	 * p	private key u
	 * q	public key (u^-1)^b
	 * dmp1	not used
	 * dmq1	not used
	 * iqmp	not used
	 */
	BN_copy(rsa->d, BN_value_one());
	BN_copy(rsa->dmp1, BN_value_one());
	BN_copy(rsa->dmq1, BN_value_one());
	BN_copy(rsa->iqmp, BN_value_one());
	str = fheader("GQkey", id, groupname);
	pkey = EVP_PKEY_new();
	EVP_PKEY_assign_RSA(pkey, rsa);
	PEM_write_PrivateKey(str, pkey, EVP_des_cbc(), NULL, 0, NULL,
	    passwd1);
	fclose(str);
	if (debug)
		RSA_print_fp(stderr, rsa, 0);
	return (pkey);
}


/*
 ***********************************************************************
 *								       *
 * The following routines implement the Mu-Varadharajan (MV) identity  *
 * scheme                                                              *
 *								       *
 ***********************************************************************
 *
 * The Mu-Varadharajan (MV) cryptosystem was originally intended when
 * servers broadcast messages to clients, but clients never send
 * messages to servers. There is one encryption key for the server and a
 * separate decryption key for each client. It operated something like a
 * pay-per-view satellite broadcasting system where the session key is
 * encrypted by the broadcaster and the decryption keys are held in a
 * tamperproof set-top box.
 *
 * The MV parameters and private encryption key hide in a DSA cuckoo
 * structure which uses the same parameters, but generated in a
 * different way. The values are used in an encryption scheme similar to
 * El Gamal cryptography and a polynomial formed from the expansion of
 * product terms (x - x[j]), as described in Mu, Y., and V.
 * Varadharajan: Robust and Secure Broadcasting, Proc. Indocrypt 2001,
 * 223-231. The paper has significant errors and serious omissions.
 *
 * Let q be the product of n distinct primes s1[j] (j = 1...n), where
 * each s1[j] has m significant bits. Let p be a prime p = 2 * q + 1, so
 * that q and each s1[j] divide p - 1 and p has M = n * m + 1
 * significant bits. Let g be a generator of Zp; that is, gcd(g, p - 1)
 * = 1 and g^q = 1 mod p. We do modular arithmetic over Zq and then
 * project into Zp* as exponents of g. Sometimes we have to compute an
 * inverse b^-1 of random b in Zq, but for that purpose we require
 * gcd(b, q) = 1. We expect M to be in the 500-bit range and n
 * relatively small, like 30. These are the parameters of the scheme and
 * they are expensive to compute.
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
 * This routine generates a private server encryption file including the
 * private encryption key E and partial decryption keys gbar and ghat.
 * It then generates public client decryption files including the public
 * keys xbar[j] and xhat[j] for each client j. The partial decryption
 * files are used to compute the inverse of E. These values are suitably
 * blinded so secrets are not revealed.
 *
 * The distinguishing characteristic of this scheme is the capability to
 * revoke keys. Included in the calculation of E, gbar and ghat is the
 * product s = prod(s1[j]) (j = 1...n) above. If the factor s1[j] is
 * subsequently removed from the product and E, gbar and ghat
 * recomputed, the jth client will no longer be able to compute E^-1 and
 * thus unable to decrypt the messageblock.
 *
 * How it works
 *
 * The scheme goes like this. Bob has the server values (p, E, q, gbar,
 * ghat) and Alice has the client values (p, xbar, xhat).
 *
 * Alice rolls new random nonce r mod p and sends to Bob in the MV
 * request message. Bob rolls random nonce k mod q, encrypts y = r E^k
 * mod p and sends (y, gbar^k, ghat^k) to Alice.
 * 
 * Alice receives the response and computes the inverse (E^k)^-1 from
 * the partial decryption keys gbar^k, ghat^k, xbar and xhat. She then
 * decrypts y and verifies it matches the original r. The signed
 * response binds this knowledge to Bob's private key and the public key
 * previously received in his certificate.
 */
EVP_PKEY *			/* DSA cuckoo nest */
gen_mvkey(
	char	*id,		/* file name id */
	EVP_PKEY **evpars	/* parameter list pointer */
	)
{
	EVP_PKEY *pkey, *pkey1;	/* private keys */
	DSA	*dsa, *dsa2, *sdsa; /* DSA parameters */
	BN_CTX	*ctx;		/* BN working space */
	BIGNUM	*a[MVMAX];	/* polynomial coefficient vector */
	BIGNUM	*g[MVMAX];	/* public key vector */
	BIGNUM	*s1[MVMAX];	/* private enabling keys */
	BIGNUM	*x[MVMAX];	/* polynomial zeros vector */
	BIGNUM	*xbar[MVMAX], *xhat[MVMAX]; /* private keys vector */
	BIGNUM	*b;		/* group key */
	BIGNUM	*b1;		/* inverse group key */
	BIGNUM	*s;		/* enabling key */
	BIGNUM	*biga;		/* master encryption key */
	BIGNUM	*bige;		/* session encryption key */
	BIGNUM	*gbar, *ghat;	/* public key */
	BIGNUM	*u, *v, *w;	/* BN scratch */
	int	i, j, n;
	FILE	*str;
	u_int	temp;

	/*
	 * Generate MV parameters.
	 *
	 * The object is to generate a multiplicative group Zp* modulo a
	 * prime p and a subset Zq mod q, where q is the product of n
	 * distinct primes s1[j] (j = 1...n) and q divides p - 1. We
	 * first generate n m-bit primes, where the product n m is in
	 * the order of 512 bits. One or more of these may have to be
	 * replaced later. As a practical matter, it is tough to find
	 * more than 31 distinct primes for 512 bits or 61 primes for
	 * 1024 bits. The latter can take several hundred iterations
	 * and several minutes on a Sun Blade 1000.
	 */
	n = nkeys;
	fprintf(stderr,
	    "Generating MV parameters for %d keys (%d bits)...\n", n,
	    modulus2 / n);
	ctx = BN_CTX_new(); u = BN_new(); v = BN_new(); w = BN_new();
	b = BN_new(); b1 = BN_new();
	dsa = DSA_new();
	dsa->p = BN_new(); dsa->q = BN_new(); dsa->g = BN_new();
	dsa->priv_key = BN_new(); dsa->pub_key = BN_new();
	temp = 0;
	for (j = 1; j <= n; j++) {
		s1[j] = BN_new();
		while (1) {
			BN_generate_prime(s1[j], modulus2 / n, 0, NULL,
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
	fprintf(stderr, "Birthday keys regenerated %d\n", temp);

	/*
	 * Compute the modulus q as the product of the primes. Compute
	 * the modulus p as 2 * q + 1 and test p for primality. If p
	 * is composite, replace one of the primes with a new distinct
	 * one and try again. Note that q will hardly be a secret since
	 * we have to reveal p to servers, but not clients. However,
	 * factoring q to find the primes should be adequately hard, as
	 * this is the same problem considered hard in RSA. Question: is
	 * it as hard to find n small prime factors totalling n bits as
	 * it is to find two large prime factors totalling n bits?
	 * Remember, the bad guy doesn't know n.
	 */
	temp = 0;
	while (1) {
		BN_one(dsa->q);
		for (j = 1; j <= n; j++)
			BN_mul(dsa->q, dsa->q, s1[j], ctx);
		BN_copy(dsa->p, dsa->q);
		BN_add(dsa->p, dsa->p, dsa->p);
		BN_add_word(dsa->p, 1);
		if (BN_is_prime(dsa->p, BN_prime_checks, NULL, ctx,
		    NULL))
			break;

		temp++;
		j = temp % n + 1;
		while (1) {
			BN_generate_prime(u, modulus2 / n, 0, 0, NULL,
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
	fprintf(stderr, "Defective keys regenerated %d\n", temp);

	/*
	 * Compute the generator g using a random roll such that
	 * gcd(g, p - 1) = 1 and g^q = 1. This is a generator of p, not
	 * q. This may take several iterations.
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
	 * Setup is now complete. Roll random polynomial roots x[j]
	 * (j = 1...n) for all j. While it may not be strictly
	 * necessary, Make sure each root has no factors in common with
	 * q.
	 */
	fprintf(stderr,
	    "Generating polynomial coefficients for %d roots (%d bits)\n",
	    n, BN_num_bits(dsa->q)); 
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
	for (i = 0; i <= n; i++) {
		g[i] = BN_new();

		BN_mod_exp(g[i], dsa->g, a[i], dsa->p, ctx);
	}

	/*
	 * Verify prod(g[i]^(a[i] x[j]^i)) = 1 for all i, j. Note the
	 * a[i] x[j]^i exponent is computed mod q, but the g[i] is
	 * computed mod p. also note the expression given in the paper
	 * is incorrect.
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
	 * gcd(b, q) = 1 to guarantee b^-1 exists, then compute b^-1
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
	 * that the keys for the jth client do not s1[j] or the product
	 * s1[j]) (j = 1...n) which is q by construction.
	 *
	 * Compute the factor w such that w s1[j] = s1[j] for all j. The
	 * easy way to do this is to compute (q + s1[j]) / s1[j].
	 * Exercise for the student: prove the remainder is always zero.
	 */
	for (j = 1; j <= n; j++) {
		xbar[j] = BN_new(); xhat[j] = BN_new();

		BN_add(w, dsa->q, s1[j]);
		BN_div(w, u, w, s1[j], ctx);
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
		BN_mod_mul(xhat[j], xhat[j], w, dsa->q, ctx);
	}

	/*
	 * We revoke client j by dividing q by s1[j]. The quotient
	 * becomes the enabling key s. Note we always have to revoke
	 * one key; otherwise, the plaintext and cryptotext would be
	 * identical. For the present there are no provisions to revoke
	 * additional keys, so we sail on with only token revocations.
	 */
	s = BN_new();

	BN_copy(s, dsa->q);
	BN_div(s, u, s, s1[n], ctx);

	/*
	 * For each combination of clients to be revoked, make private
	 * encryption key E = A^s and partial decryption keys gbar = g^s
	 * and ghat = g^(s b), all mod p. The servers use these keys to
	 * compute the session encryption key and partial decryption
	 * keys. These values must be regenerated if the enabling key is
	 * changed.
	 */
	bige = BN_new(); gbar = BN_new(); ghat = BN_new();

	BN_mod_exp(bige, biga, s, dsa->p, ctx);
	BN_mod_exp(gbar, dsa->g, s, dsa->p, ctx);
	BN_mod_mul(v, s, b, dsa->q, ctx);
	BN_mod_exp(ghat, dsa->g, v, dsa->p, ctx);
	
	/*
	 * Notes: We produce the key media in three steps. The first
	 * step is to generate the system parameters p, q, g, b, A and
	 * the enabling keys s1[j]. Associated with each s1[j] are
	 * parameters xbar[j] and xhat[j]. All of these parameters are
	 * retained in a data structure protecteted by the trusted-agent
	 * password. The p, xbar[j] and xhat[j] paremeters are
	 * distributed to the j clients. When the client keys are to be
	 * activated, the enabled keys are multipied together to form
	 * the master enabling key s. This and the other parameters are
	 * used to compute the server encryption key E and the partial
	 * decryption keys gbar and ghat.
	 *
	 * In the identity exchange the client rolls random r and sends
	 * it to the server. The server rolls random k, which is used
	 * only once, then computes the session key E^k and partial
	 * decryption keys gbar^k and ghat^k. The server sends the
	 * encrypted r along with gbar^k and ghat^k to the client. The
	 * client completes the decryption and verifies it matches r.
	 */
	/*
	 * Write the MV trusted-agent parameters and keys as a DSA
	 * private key encoded in PEM.
	 *
	 * p	modulus p
	 * q	modulus q
	 * g	generator g
	 * priv_key A mod p
	 * pub_key b mod q
	 * (remaining values are not used)
	 */
	i = 0;
	str = fheader("MVta", "mvta", groupname);
	fprintf(stderr, "Generating MV trusted-authority keys\n");
	BN_copy(dsa->priv_key, biga);
	BN_copy(dsa->pub_key, b);
	pkey = EVP_PKEY_new();
	EVP_PKEY_assign_DSA(pkey, dsa);
	PEM_write_PrivateKey(str, pkey, EVP_des_cbc(), NULL, 0, NULL,
	    passwd1);
	evpars[i++] = pkey;
	if (debug)
		DSA_print_fp(stderr, dsa, 0);

	/*
	 * Append the MV server parameters and keys as a DSA key encoded
	 * in PEM.
	 *
	 * p	modulus p
	 * q	modulus q (used only when generating k)
	 * g	bige
	 * priv_key gbar
	 * pub_key ghat
	 * (remaining values are not used)
	 */
	fprintf(stderr, "Generating MV server keys\n");
	dsa2 = DSA_new();
	dsa2->p = BN_dup(dsa->p);
	dsa2->q = BN_dup(dsa->q); 
	dsa2->g = BN_dup(bige); 
	dsa2->priv_key = BN_dup(gbar);
	dsa2->pub_key = BN_dup(ghat);
	pkey1 = EVP_PKEY_new();
	EVP_PKEY_assign_DSA(pkey1, dsa2);
	PEM_write_PrivateKey(str, pkey1, EVP_des_cbc(), NULL, 0, NULL,
	    passwd1);
	evpars[i++] = pkey1;
	if (debug)
		DSA_print_fp(stderr, dsa2, 0);

	/*
	 * Append the MV client parameters for each client j as DSA keys
	 * encoded in PEM.
	 *
	 * p	modulus p
	 * priv_key xbar[j] mod q
	 * pub_key xhat[j] mod q
	 * (remaining values are not used)
	 */
	fprintf(stderr, "Generating %d MV client keys\n", n);
	for (j = 1; j <= n; j++) {
		sdsa = DSA_new();

		sdsa->p = BN_dup(dsa->p);
		sdsa->q = BN_dup(BN_value_one()); 
		sdsa->g = BN_dup(BN_value_one()); 
		sdsa->priv_key = BN_dup(xbar[j]);
		sdsa->pub_key = BN_dup(xhat[j]);
		pkey1 = EVP_PKEY_new();
		EVP_PKEY_set1_DSA(pkey1, sdsa);
		PEM_write_PrivateKey(str, pkey1, EVP_des_cbc(), NULL, 0,
		    NULL, passwd1);
		evpars[i++] = pkey1;
		if (debug)
			DSA_print_fp(stderr, sdsa, 0);

		/*
		 * The product gbar^k)^xbar[j] (ghat^k)^xhat[j] and E
		 * are inverses of each other. We check that the product
		 * is one for each client except the ones that have been
		 * revoked. 
		 */
		BN_mod_exp(v, dsa2->priv_key, sdsa->pub_key, dsa->p,
		    ctx);
		BN_mod_exp(u, dsa2->pub_key, sdsa->priv_key, dsa->p,
		    ctx);
		BN_mod_mul(u, u, v, dsa->p, ctx);
		BN_mod_mul(u, u, bige, dsa->p, ctx);
		if (!BN_is_one(u)) {
			fprintf(stderr, "Revoke key %d\n", j);
			continue;
		}
	}
	evpars[i++] = NULL;
	fclose(str);

	/*
	 * Free the countries.
	 */
	for (i = 0; i <= n; i++) {
		BN_free(a[i]); BN_free(g[i]);
	}
	for (j = 1; j <= n; j++) {
		BN_free(x[j]); BN_free(xbar[j]); BN_free(xhat[j]);
		BN_free(s1[j]); 
	}
	return (pkey);
}


/*
 * Generate X509v3 certificate.
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
	char	*exten,		/* private cert extension */
	char	*name		/* subject/issuer namd */
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
	 * the version to 3. Set the initial validity to the current
	 * time and the finalvalidity one year hence.
	 */
 	id = OBJ_nid2sn(md->pkey_type);
	fprintf(stderr, "Generating new certificate %s %s\n", name, id);
	cert = X509_new();
	X509_set_version(cert, 2L);
	serial = ASN1_INTEGER_new();
	ASN1_INTEGER_set(serial, (long)epoch + JAN_1970);
	X509_set_serialNumber(cert, serial);
	ASN1_INTEGER_free(serial);
	X509_time_adj(X509_get_notBefore(cert), 0L, &epoch);
	X509_time_adj(X509_get_notAfter(cert), YEAR, &epoch);
	subj = X509_get_subject_name(cert);
	X509_NAME_add_entry_by_txt(subj, "commonName", MBSTRING_ASC,
	    (unsigned char *) name, strlen(name), -1, 0);
	subj = X509_get_issuer_name(cert);
	X509_NAME_add_entry_by_txt(subj, "commonName", MBSTRING_ASC,
	    (unsigned char *) name, strlen(name), -1, 0);
	if (!X509_set_pubkey(cert, pkey)) {
		fprintf(stderr, "Assign key fails\n%s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		X509_free(cert);
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
			return (0);
		}
		X509_EXTENSION_free(ex);
	}

	/*
	 * Sign and verify.
	 */
	X509_sign(cert, pkey, md);
	if (X509_verify(cert, pkey) <= 0) {
		fprintf(stderr, "Verify %s certificate fails\n%s\n", id,
		    ERR_error_string(ERR_get_error(), NULL));
		X509_free(cert);
		return (0);
	}

	/*
	 * Write the certificate encoded in PEM.
	 */
	sprintf(pathbuf, "%scert", id);
	str = fheader(pathbuf, "cert", hostname);
	PEM_write_X509(str, cert);
	fclose(str);
	if (debug)
		X509_print_fp(stderr, cert);
	X509_free(cert);
	return (1);
}

#if 0	/* asn2ntp is used only with commercial certificates */
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
	return (NULL);
}
#endif /* OPENSSL */


/*
 * Generate file header and link
 */
FILE *
fheader	(
	const char *file,	/* file name id */
	const char *ulink,	/* linkname */
	const char *owner	/* owner name */
	)
{
	FILE	*str;		/* file handle */
	char	linkname[MAXFILENAME]; /* link name */
	int	temp;

	sprintf(filename, "ntpkey_%s_%s.%lu", file, owner, epoch +
	    JAN_1970);
	if ((str = fopen(filename, "w")) == NULL) {
		perror("Write");
		exit (-1);
	}
	sprintf(linkname, "ntpkey_%s_%s", ulink, owner);
	remove(linkname);
	temp = symlink(filename, linkname);
	if (temp < 0)
		perror(file);
	fprintf(stderr, "Generating new %s file and link\n", ulink);
	fprintf(stderr, "%s->%s\n", linkname, filename);
	fprintf(str, "# %s\n# %s\n", filename, ctime(&epoch));
	return (str);
}
