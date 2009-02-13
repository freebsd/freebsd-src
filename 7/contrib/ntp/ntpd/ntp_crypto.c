/*
 * ntp_crypto.c - NTP version 4 public key routines
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef OPENSSL
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <fcntl.h>

#include "ntpd.h"
#include "ntp_stdlib.h"
#include "ntp_unixtime.h"
#include "ntp_string.h"
#include <ntp_random.h>

#include "openssl/asn1_mac.h"
#include "openssl/bn.h"
#include "openssl/err.h"
#include "openssl/evp.h"
#include "openssl/pem.h"
#include "openssl/rand.h"
#include "openssl/x509v3.h"

#ifdef KERNEL_PLL
#include "ntp_syscall.h"
#endif /* KERNEL_PLL */

/*
 * Extension field message format
 *
 * These are always signed and saved before sending in network byte
 * order. They must be converted to and from host byte order for
 * processing.
 *
 * +-------+-------+
 * |   op  |  len  | <- extension pointer
 * +-------+-------+
 * |    assocID    |
 * +---------------+
 * |   timestamp   | <- value pointer
 * +---------------+
 * |   filestamp   |
 * +---------------+
 * |   value len   |
 * +---------------+
 * |               |
 * =     value     =
 * |               |
 * +---------------+
 * | signature len |
 * +---------------+
 * |               |
 * =   signature   =
 * |               |
 * +---------------+
 *
 * The CRYPTO_RESP bit is set to 0 for requests, 1 for responses.
 * Requests carry the association ID of the receiver; responses carry
 * the association ID of the sender. Some messages include only the
 * operation/length and association ID words and so have length 8
 * octets. Ohers include the value structure and associated value and
 * signature fields. These messages include the timestamp, filestamp,
 * value and signature words and so have length at least 24 octets. The
 * signature and/or value fields can be empty, in which case the
 * respective length words are zero. An empty value with nonempty
 * signature is syntactically valid, but semantically questionable.
 *
 * The filestamp represents the time when a cryptographic data file such
 * as a public/private key pair is created. It follows every reference
 * depending on that file and serves as a means to obsolete earlier data
 * of the same type. The timestamp represents the time when the
 * cryptographic data of the message were last signed. Creation of a
 * cryptographic data file or signing a message can occur only when the
 * creator or signor is synchronized to an authoritative source and
 * proventicated to a trusted authority.
 *
 * Note there are four conditions required for server trust. First, the
 * public key on the certificate must be verified, which involves a
 * number of format, content and consistency checks. Next, the server
 * identity must be confirmed by one of four schemes: private
 * certificate, IFF scheme, GQ scheme or certificate trail hike to a
 * self signed trusted certificate. Finally, the server signature must
 * be verified.
 */
/*
 * Cryptodefines
 */
#define TAI_1972	10	/* initial TAI offset (s) */
#define MAX_LEAP	100	/* max UTC leapseconds (s) */
#define VALUE_LEN	(6 * 4) /* min response field length */
#define YEAR		(60 * 60 * 24 * 365) /* seconds in year */

/*
 * Global cryptodata in host byte order
 */
u_int32	crypto_flags = 0x0;	/* status word */

/*
 * Global cryptodata in network byte order
 */
struct cert_info *cinfo = NULL;	/* certificate info/value */
struct value hostval;		/* host value */
struct value pubkey;		/* public key */
struct value tai_leap;		/* leapseconds table */
EVP_PKEY *iffpar_pkey = NULL;	/* IFF parameters */
EVP_PKEY *gqpar_pkey = NULL;	/* GQ parameters */
EVP_PKEY *mvpar_pkey = NULL;	/* MV parameters */
char	*iffpar_file = NULL; /* IFF parameters file */
char	*gqpar_file = NULL;	/* GQ parameters file */
char	*mvpar_file = NULL;	/* MV parameters file */

/*
 * Private cryptodata in host byte order
 */
static char *passwd = NULL;	/* private key password */
static EVP_PKEY *host_pkey = NULL; /* host key */
static EVP_PKEY *sign_pkey = NULL; /* sign key */
static const EVP_MD *sign_digest = NULL; /* sign digest */
static u_int sign_siglen;	/* sign key length */
static char *rand_file = NULL;	/* random seed file */
static char *host_file = NULL;	/* host key file */
static char *sign_file = NULL;	/* sign key file */
static char *cert_file = NULL;	/* certificate file */
static char *leap_file = NULL;	/* leapseconds file */
static tstamp_t if_fstamp = 0;	/* IFF filestamp */
static tstamp_t gq_fstamp = 0;	/* GQ file stamp */
static tstamp_t mv_fstamp = 0;	/* MV filestamp */
static u_int ident_scheme = 0;	/* server identity scheme */

/*
 * Cryptotypes
 */
static	int	crypto_verify	P((struct exten *, struct value *,
				    struct peer *));
static	int	crypto_encrypt	P((struct exten *, struct value *,
				    keyid_t *));
static	int	crypto_alice	P((struct peer *, struct value *));
static	int	crypto_alice2	P((struct peer *, struct value *));
static	int	crypto_alice3	P((struct peer *, struct value *));
static	int	crypto_bob	P((struct exten *, struct value *));
static	int	crypto_bob2	P((struct exten *, struct value *));
static	int	crypto_bob3	P((struct exten *, struct value *));
static	int	crypto_iff	P((struct exten *, struct peer *));
static	int	crypto_gq	P((struct exten *, struct peer *));
static	int	crypto_mv	P((struct exten *, struct peer *));
static	u_int	crypto_send	P((struct exten *, struct value *));
static	tstamp_t crypto_time	P((void));
static	u_long	asn2ntp		P((ASN1_TIME *));
static	struct cert_info *cert_parse P((u_char *, u_int, tstamp_t));
static	int	cert_sign	P((struct exten *, struct value *));
static	int	cert_valid	P((struct cert_info *, EVP_PKEY *));
static	int	cert_install	P((struct exten *, struct peer *));
static	void	cert_free	P((struct cert_info *));
static	EVP_PKEY *crypto_key	P((char *, tstamp_t *));
static	int	bighash		P((BIGNUM *, BIGNUM *));
static	struct cert_info *crypto_cert P((char *));
static	void	crypto_tai	P((char *));

#ifdef SYS_WINNT
int
readlink(char * link, char * file, int len) {
	return (-1);
}
#endif

/*
 * session_key - generate session key
 *
 * This routine generates a session key from the source address,
 * destination address, key ID and private value. The value of the
 * session key is the MD5 hash of these values, while the next key ID is
 * the first four octets of the hash.
 *
 * Returns the next key ID
 */
keyid_t
session_key(
	struct sockaddr_storage *srcadr, /* source address */
	struct sockaddr_storage *dstadr, /* destination address */
	keyid_t	keyno,		/* key ID */
	keyid_t	private,	/* private value */
	u_long	lifetime 	/* key lifetime */
	)
{
	EVP_MD_CTX ctx;		/* message digest context */
	u_char dgst[EVP_MAX_MD_SIZE]; /* message digest */
	keyid_t	keyid;		/* key identifer */
	u_int32	header[10];	/* data in network byte order */
	u_int	hdlen, len;

	if (!dstadr)
		return 0;
	
	/*
	 * Generate the session key and key ID. If the lifetime is
	 * greater than zero, install the key and call it trusted.
	 */
	hdlen = 0;
	switch(srcadr->ss_family) {
	case AF_INET:
		header[0] = ((struct sockaddr_in *)srcadr)->sin_addr.s_addr;
		header[1] = ((struct sockaddr_in *)dstadr)->sin_addr.s_addr;
		header[2] = htonl(keyno);
		header[3] = htonl(private);
		hdlen = 4 * sizeof(u_int32);
		break;

	case AF_INET6:
		memcpy(&header[0], &GET_INADDR6(*srcadr),
		    sizeof(struct in6_addr));
		memcpy(&header[4], &GET_INADDR6(*dstadr),
		    sizeof(struct in6_addr));
		header[8] = htonl(keyno);
		header[9] = htonl(private);
		hdlen = 10 * sizeof(u_int32);
		break;
	}
	EVP_DigestInit(&ctx, EVP_md5());
	EVP_DigestUpdate(&ctx, (u_char *)header, hdlen);
	EVP_DigestFinal(&ctx, dgst, &len);
	memcpy(&keyid, dgst, 4);
	keyid = ntohl(keyid);
	if (lifetime != 0) {
		MD5auth_setkey(keyno, dgst, len);
		authtrust(keyno, lifetime);
	}
#ifdef DEBUG
	if (debug > 1)
		printf(
		    "session_key: %s > %s %08x %08x hash %08x life %lu\n",
		    stoa(srcadr), stoa(dstadr), keyno,
		    private, keyid, lifetime);
#endif
	return (keyid);
}


/*
 * make_keylist - generate key list
 *
 * Returns
 * XEVNT_OK	success
 * XEVNT_PER	host certificate expired
 *
 * This routine constructs a pseudo-random sequence by repeatedly
 * hashing the session key starting from a given source address,
 * destination address, private value and the next key ID of the
 * preceeding session key. The last entry on the list is saved along
 * with its sequence number and public signature.
 */
int
make_keylist(
	struct peer *peer,	/* peer structure pointer */
	struct interface *dstadr /* interface */
	)
{
	EVP_MD_CTX ctx;		/* signature context */
	tstamp_t tstamp;	/* NTP timestamp */
	struct autokey *ap;	/* autokey pointer */
	struct value *vp;	/* value pointer */
	keyid_t	keyid = 0;	/* next key ID */
	keyid_t	cookie;		/* private value */
	u_long	lifetime;
	u_int	len, mpoll;
	int	i;

	if (!dstadr)
		return XEVNT_OK;
	
	/*
	 * Allocate the key list if necessary.
	 */
	tstamp = crypto_time();
	if (peer->keylist == NULL)
		peer->keylist = emalloc(sizeof(keyid_t) *
		    NTP_MAXSESSION);

	/*
	 * Generate an initial key ID which is unique and greater than
	 * NTP_MAXKEY.
	 */
	while (1) {
		keyid = (ntp_random() + NTP_MAXKEY + 1) & ((1 <<
		    sizeof(keyid_t)) - 1);
		if (authhavekey(keyid))
			continue;
		break;
	}

	/*
	 * Generate up to NTP_MAXSESSION session keys. Stop if the
	 * next one would not be unique or not a session key ID or if
	 * it would expire before the next poll. The private value
	 * included in the hash is zero if broadcast mode, the peer
	 * cookie if client mode or the host cookie if symmetric modes.
	 */
	mpoll = 1 << min(peer->ppoll, peer->hpoll);
	lifetime = min(sys_automax, NTP_MAXSESSION * mpoll);
	if (peer->hmode == MODE_BROADCAST)
		cookie = 0;
	else
		cookie = peer->pcookie;
	for (i = 0; i < NTP_MAXSESSION; i++) {
		peer->keylist[i] = keyid;
		peer->keynumber = i;
		keyid = session_key(&dstadr->sin, &peer->srcadr, keyid,
		    cookie, lifetime);
		lifetime -= mpoll;
		if (auth_havekey(keyid) || keyid <= NTP_MAXKEY ||
		    lifetime <= mpoll)
			break;
	}

	/*
	 * Save the last session key ID, sequence number and timestamp,
	 * then sign these values for later retrieval by the clients. Be
	 * careful not to use invalid key media. Use the public values
	 * timestamp as filestamp. 
	 */
	vp = &peer->sndval;
	if (vp->ptr == NULL)
		vp->ptr = emalloc(sizeof(struct autokey));
	ap = (struct autokey *)vp->ptr;
	ap->seq = htonl(peer->keynumber);
	ap->key = htonl(keyid);
	vp->tstamp = htonl(tstamp);
	vp->fstamp = hostval.tstamp;
	vp->vallen = htonl(sizeof(struct autokey));
	vp->siglen = 0;
	if (tstamp != 0) {
		if (tstamp < cinfo->first || tstamp > cinfo->last)
			return (XEVNT_PER);

		if (vp->sig == NULL)
			vp->sig = emalloc(sign_siglen);
		EVP_SignInit(&ctx, sign_digest);
		EVP_SignUpdate(&ctx, (u_char *)vp, 12);
		EVP_SignUpdate(&ctx, vp->ptr, sizeof(struct autokey));
		if (EVP_SignFinal(&ctx, vp->sig, &len, sign_pkey))
			vp->siglen = htonl(len);
		else
			msyslog(LOG_ERR, "make_keys %s\n",
			    ERR_error_string(ERR_get_error(), NULL));
		peer->flags |= FLAG_ASSOC;
	}
#ifdef DEBUG
	if (debug)
		printf("make_keys: %d %08x %08x ts %u fs %u poll %d\n",
		    ntohl(ap->seq), ntohl(ap->key), cookie,
		    ntohl(vp->tstamp), ntohl(vp->fstamp), peer->hpoll);
#endif
	return (XEVNT_OK);
}


/*
 * crypto_recv - parse extension fields
 *
 * This routine is called when the packet has been matched to an
 * association and passed sanity, format and MAC checks. We believe the
 * extension field values only if the field has proper format and
 * length, the timestamp and filestamp are valid and the signature has
 * valid length and is verified. There are a few cases where some values
 * are believed even if the signature fails, but only if the proventic
 * bit is not set.
 */
int
crypto_recv(
	struct peer *peer,	/* peer structure pointer */
	struct recvbuf *rbufp	/* packet buffer pointer */
	)
{
	const EVP_MD *dp;	/* message digest algorithm */
	u_int32	*pkt;		/* receive packet pointer */
	struct autokey *ap, *bp; /* autokey pointer */
	struct exten *ep, *fp;	/* extension pointers */
	int	has_mac;	/* length of MAC field */
	int	authlen;	/* offset of MAC field */
	associd_t associd;	/* association ID */
	tstamp_t tstamp = 0;	/* timestamp */
	tstamp_t fstamp = 0;	/* filestamp */
	u_int	len;		/* extension field length */
	u_int	code;		/* extension field opcode */
	u_int	vallen = 0;	/* value length */
	X509	*cert;		/* X509 certificate */
	char	statstr[NTP_MAXSTRLEN]; /* statistics for filegen */
	keyid_t	cookie;		/* crumbles */
	int	hismode;	/* packet mode */
	int	rval = XEVNT_OK;
	u_char	*ptr;
	u_int32 temp32;

	/*
	 * Initialize. Note that the packet has already been checked for
	 * valid format and extension field lengths. First extract the
	 * field length, command code and association ID in host byte
	 * order. These are used with all commands and modes. Then check
	 * the version number, which must be 2, and length, which must
	 * be at least 8 for requests and VALUE_LEN (24) for responses.
	 * Packets that fail either test sink without a trace. The
	 * association ID is saved only if nonzero.
	 */
	authlen = LEN_PKT_NOMAC;
	hismode = (int)PKT_MODE((&rbufp->recv_pkt)->li_vn_mode);
	while ((has_mac = rbufp->recv_length - authlen) > MAX_MAC_LEN) {
		pkt = (u_int32 *)&rbufp->recv_pkt + authlen / 4;
		ep = (struct exten *)pkt;
		code = ntohl(ep->opcode) & 0xffff0000;
		len = ntohl(ep->opcode) & 0x0000ffff;
		associd = (associd_t) ntohl(pkt[1]);
		rval = XEVNT_OK;
#ifdef DEBUG
		if (debug)
			printf(
			    "crypto_recv: flags 0x%x ext offset %d len %u code 0x%x assocID %d\n",
			    peer->crypto, authlen, len, code >> 16,
			    associd);
#endif

		/*
		 * Check version number and field length. If bad,
		 * quietly ignore the packet.
		 */
		if (((code >> 24) & 0x3f) != CRYPTO_VN || len < 8) {
			sys_unknownversion++;
			code |= CRYPTO_ERROR;
		}

		/*
		 * Little vulnerability bandage here. If a perp tosses a
		 * fake association ID over the fence, we better toss it
		 * out. Only the first one counts.
		 */
		if (code & CRYPTO_RESP) {
			if (peer->assoc == 0)
				peer->assoc = associd;
			else if (peer->assoc != associd)
				code |= CRYPTO_ERROR;
		}
		if (len >= VALUE_LEN) {
			tstamp = ntohl(ep->tstamp);
			fstamp = ntohl(ep->fstamp);
			vallen = ntohl(ep->vallen);
		}
		switch (code) {

		/*
		 * Install status word, host name, signature scheme and
		 * association ID. In OpenSSL the signature algorithm is
		 * bound to the digest algorithm, so the NID completely
		 * defines the signature scheme. Note the request and
		 * response are identical, but neither is validated by
		 * signature. The request is processed here only in
		 * symmetric modes. The server name field might be
		 * useful to implement access controls in future.
		 */
		case CRYPTO_ASSOC:

			/*
			 * If the machine is running when this message
			 * arrives, the other fellow has reset and so
			 * must we. Otherwise, pass the extension field
			 * to the transmit side.
			 */
			if (peer->crypto) {
				rval = XEVNT_ERR;
				break;
			}
			fp = emalloc(len);
			memcpy(fp, ep, len);
			temp32 = CRYPTO_RESP;
			fp->opcode |= htonl(temp32);
			peer->cmmd = fp;
			/* fall through */

		case CRYPTO_ASSOC | CRYPTO_RESP:

			/*
			 * Discard the message if it has already been
			 * stored or the message has been amputated.
			 */
			if (peer->crypto)
				break;

			if (vallen == 0 || vallen > MAXHOSTNAME ||
			    len < VALUE_LEN + vallen) {
				rval = XEVNT_LEN;
				break;
			}

			/*
			 * Check the identity schemes are compatible. If
			 * the client has PC, the server must have PC,
			 * in which case the server public key and
			 * identity are presumed valid, so we skip the
			 * certificate and identity exchanges and move
			 * immediately to the cookie exchange which
			 * confirms the server signature.
			 */
#ifdef DEBUG
			if (debug)
				printf(
				    "crypto_recv: ident host 0x%x server 0x%x\n",
				    crypto_flags, fstamp);
#endif
			temp32 = (crypto_flags | ident_scheme) &
			    fstamp & CRYPTO_FLAG_MASK;
			if (crypto_flags & CRYPTO_FLAG_PRIV) {
				if (!(fstamp & CRYPTO_FLAG_PRIV)) {
					rval = XEVNT_KEY;
					break;

				} else {
					fstamp |= CRYPTO_FLAG_VALID |
					    CRYPTO_FLAG_VRFY |
					    CRYPTO_FLAG_SIGN;
				}
			/*
			 * In symmetric modes it is an error if either
			 * peer requests identity and the other peer
			 * does not support it.
			 */
			} else if ((hismode == MODE_ACTIVE || hismode ==
			    MODE_PASSIVE) && ((crypto_flags | fstamp) &
			    CRYPTO_FLAG_MASK) && !temp32) {
				rval = XEVNT_KEY;
				break;
			/*
			 * It is an error if the client requests
			 * identity and the server does not support it.
			 */
			} else if (hismode == MODE_CLIENT && (fstamp &
			    CRYPTO_FLAG_MASK) && !temp32) {
				rval = XEVNT_KEY;
				break;
			}

			/*
			 * Otherwise, the identity scheme(s) are those
			 * that both client and server support.
			 */
			fstamp = temp32 | (fstamp & ~CRYPTO_FLAG_MASK);

			/*
			 * Discard the message if the signature digest
			 * NID is not supported.
			 */
			temp32 = (fstamp >> 16) & 0xffff;
			dp =
			    (const EVP_MD *)EVP_get_digestbynid(temp32);
			if (dp == NULL) {
				rval = XEVNT_MD;
				break;
			}

			/*
			 * Save status word, host name and message
			 * digest/signature type.
			 */
			peer->crypto = fstamp;
			peer->digest = dp;
			peer->subject = emalloc(vallen + 1);
			memcpy(peer->subject, ep->pkt, vallen);
			peer->subject[vallen] = '\0';
			peer->issuer = emalloc(vallen + 1);
			strcpy(peer->issuer, peer->subject);
			temp32 = (fstamp >> 16) & 0xffff;
			sprintf(statstr,
			    "flags 0x%x host %s signature %s", fstamp,
			    peer->subject, OBJ_nid2ln(temp32));
			record_crypto_stats(&peer->srcadr, statstr);
#ifdef DEBUG
			if (debug)
				printf("crypto_recv: %s\n", statstr);
#endif
			break;

		/*
		 * Decode X509 certificate in ASN.1 format and extract
		 * the data containing, among other things, subject
		 * name and public key. In the default identification
		 * scheme, the certificate trail is followed to a self
		 * signed trusted certificate.
		 */
		case CRYPTO_CERT | CRYPTO_RESP:

			/*
			 * Discard the message if invalid.
			 */
			if ((rval = crypto_verify(ep, NULL, peer)) !=
			    XEVNT_OK)
				break;

			/*
			 * Scan the certificate list to delete old
			 * versions and link the newest version first on
			 * the list.
			 */
			if ((rval = cert_install(ep, peer)) != XEVNT_OK)
				break;

			/*
			 * If we snatch the certificate before the
			 * server certificate has been signed by its
			 * server, it will be self signed. When it is,
			 * we chase the certificate issuer, which the
			 * server has, and keep going until a self
			 * signed trusted certificate is found. Be sure
			 * to update the issuer field, since it may
			 * change.
			 */
			if (peer->issuer != NULL)
				free(peer->issuer);
			peer->issuer = emalloc(strlen(cinfo->issuer) +
			    1);
			strcpy(peer->issuer, cinfo->issuer);

			/*
			 * We plug in the public key and lifetime from
			 * the first certificate received. However, note
			 * that this certificate might not be signed by
			 * the server, so we can't check the
			 * signature/digest NID.
			 */
			if (peer->pkey == NULL) {
				ptr = (u_char *)cinfo->cert.ptr;
				cert = d2i_X509(NULL, &ptr,
				    ntohl(cinfo->cert.vallen));
				peer->pkey = X509_get_pubkey(cert);
				X509_free(cert);
			}
			peer->flash &= ~TEST8;
			temp32 = cinfo->nid;
			sprintf(statstr, "cert %s 0x%x %s (%u) fs %u",
			    cinfo->subject, cinfo->flags,
			    OBJ_nid2ln(temp32), temp32,
			    ntohl(ep->fstamp));
			record_crypto_stats(&peer->srcadr, statstr);
#ifdef DEBUG
			if (debug)
				printf("crypto_recv: %s\n", statstr);
#endif
			break;

		/*
		 * Schnorr (IFF)identity scheme. This scheme is designed
		 * for use with shared secret group keys and where the
		 * certificate may be generated by a third party. The
		 * client sends a challenge to the server, which
		 * performs a calculation and returns the result. A
		 * positive result is possible only if both client and
		 * server contain the same secret group key.
		 */
		case CRYPTO_IFF | CRYPTO_RESP:

			/*
			 * Discard the message if invalid or certificate
			 * trail not trusted.
			 */
			if (!(peer->crypto & CRYPTO_FLAG_VALID)) {
				rval = XEVNT_ERR;
				break;
			}
			if ((rval = crypto_verify(ep, NULL, peer)) !=
			    XEVNT_OK)
				break;

			/*
			 * If the the challenge matches the response,
			 * the certificate public key, as well as the
			 * server public key, signatyre and identity are
			 * all verified at the same time. The server is
			 * declared trusted, so we skip further
			 * certificate stages and move immediately to
			 * the cookie stage.
			 */
			if ((rval = crypto_iff(ep, peer)) != XEVNT_OK)
				break;

			peer->crypto |= CRYPTO_FLAG_VRFY |
			    CRYPTO_FLAG_PROV;
			peer->flash &= ~TEST8;
			sprintf(statstr, "iff fs %u",
			    ntohl(ep->fstamp));
			record_crypto_stats(&peer->srcadr, statstr);
#ifdef DEBUG
			if (debug)
				printf("crypto_recv: %s\n", statstr);
#endif
			break;

		/*
		 * Guillou-Quisquater (GQ) identity scheme. This scheme
		 * is designed for use with public certificates carrying
		 * the GQ public key in an extension field. The client
		 * sends a challenge to the server, which performs a
		 * calculation and returns the result. A positive result
		 * is possible only if both client and server contain
		 * the same group key and the server has the matching GQ
		 * private key.
		 */
		case CRYPTO_GQ | CRYPTO_RESP:

			/*
			 * Discard the message if invalid or certificate
			 * trail not trusted.
			 */
			if (!(peer->crypto & CRYPTO_FLAG_VALID)) {
				rval = XEVNT_ERR;
				break;
			}
			if ((rval = crypto_verify(ep, NULL, peer)) !=
			    XEVNT_OK)
				break;

			/*
			 * If the the challenge matches the response,
			 * the certificate public key, as well as the
			 * server public key, signatyre and identity are
			 * all verified at the same time. The server is
			 * declared trusted, so we skip further
			 * certificate stages and move immediately to
			 * the cookie stage.
			 */
			if ((rval = crypto_gq(ep, peer)) != XEVNT_OK)
				break;

			peer->crypto |= CRYPTO_FLAG_VRFY |
			    CRYPTO_FLAG_PROV;
			peer->flash &= ~TEST8;
			sprintf(statstr, "gq fs %u",
			    ntohl(ep->fstamp));
			record_crypto_stats(&peer->srcadr, statstr);
#ifdef DEBUG
			if (debug)
				printf("crypto_recv: %s\n", statstr);
#endif
			break;

		/*
		 * MV
		 */
		case CRYPTO_MV | CRYPTO_RESP:

			/*
			 * Discard the message if invalid or certificate
			 * trail not trusted.
			 */
			if (!(peer->crypto & CRYPTO_FLAG_VALID)) {
				rval = XEVNT_ERR;
				break;
			}
			if ((rval = crypto_verify(ep, NULL, peer)) !=
			    XEVNT_OK)
				break;

			/*
			 * If the the challenge matches the response,
			 * the certificate public key, as well as the
			 * server public key, signatyre and identity are
			 * all verified at the same time. The server is
			 * declared trusted, so we skip further
			 * certificate stages and move immediately to
			 * the cookie stage.
			 */
			if ((rval = crypto_mv(ep, peer)) != XEVNT_OK)
				break;

			peer->crypto |= CRYPTO_FLAG_VRFY |
			    CRYPTO_FLAG_PROV;
			peer->flash &= ~TEST8;
			sprintf(statstr, "mv fs %u",
			    ntohl(ep->fstamp));
			record_crypto_stats(&peer->srcadr, statstr);
#ifdef DEBUG
			if (debug)
				printf("crypto_recv: %s\n", statstr);
#endif
			break;

		/*
		 * Cookie request in symmetric modes. Roll a random
		 * cookie and install in symmetric mode. Encrypt for the
		 * response, which is transmitted later.
		 */
		case CRYPTO_COOK:

			/*
			 * Discard the message if invalid or certificate
			 * trail not trusted.
			 */
			if (!(peer->crypto & CRYPTO_FLAG_VALID)) {
				rval = XEVNT_ERR;
				break;
			}
			if ((rval = crypto_verify(ep, NULL, peer)) !=
			    XEVNT_OK)
				break;

			/*
			 * Pass the extension field to the transmit
			 * side. If already agreed, walk away.
			 */
			fp = emalloc(len);
			memcpy(fp, ep, len);
			temp32 = CRYPTO_RESP;
			fp->opcode |= htonl(temp32);
			peer->cmmd = fp;
			if (peer->crypto & CRYPTO_FLAG_AGREE) {
				peer->flash &= ~TEST8;
				break;
			}

			/*
			 * Install cookie values and light the cookie
			 * bit. The transmit side will pick up and
			 * encrypt it for the response.
			 */
			key_expire(peer);
			peer->cookval.tstamp = ep->tstamp;
			peer->cookval.fstamp = ep->fstamp;
			RAND_bytes((u_char *)&peer->pcookie, 4);
			peer->crypto &= ~CRYPTO_FLAG_AUTO;
			peer->crypto |= CRYPTO_FLAG_AGREE;
			peer->flash &= ~TEST8;
			sprintf(statstr, "cook %x ts %u fs %u",
			    peer->pcookie, ntohl(ep->tstamp),
			    ntohl(ep->fstamp));
			record_crypto_stats(&peer->srcadr, statstr);
#ifdef DEBUG
			if (debug)
				printf("crypto_recv: %s\n", statstr);
#endif
			break;

		/*
		 * Cookie response in client and symmetric modes. If the
		 * cookie bit is set, the working cookie is the EXOR of
		 * the current and new values.
		 */
		case CRYPTO_COOK | CRYPTO_RESP:

			/*
			 * Discard the message if invalid or identity
			 * not confirmed or signature not verified with
			 * respect to the cookie values.
			 */
			if (!(peer->crypto & CRYPTO_FLAG_VRFY)) {
				rval = XEVNT_ERR;
				break;
			}
			if ((rval = crypto_verify(ep, &peer->cookval,
			    peer)) != XEVNT_OK)
				break;

			/*
			 * Decrypt the cookie, hunting all the time for
			 * errors.
			 */
			if (vallen == (u_int) EVP_PKEY_size(host_pkey)) {
				RSA_private_decrypt(vallen,
				    (u_char *)ep->pkt,
				    (u_char *)&temp32,
				    host_pkey->pkey.rsa,
				    RSA_PKCS1_OAEP_PADDING);
				cookie = ntohl(temp32);
			} else {
				rval = XEVNT_CKY;
				break;
			}

			/*
			 * Install cookie values and light the cookie
			 * bit. If this is not broadcast client mode, we
			 * are done here.
			 */
			key_expire(peer);
			peer->cookval.tstamp = ep->tstamp;
			peer->cookval.fstamp = ep->fstamp;
			if (peer->crypto & CRYPTO_FLAG_AGREE)
				peer->pcookie ^= cookie;
			else
				peer->pcookie = cookie;
			if (peer->hmode == MODE_CLIENT &&
			    !(peer->cast_flags & MDF_BCLNT))
				peer->crypto |= CRYPTO_FLAG_AUTO;
			else
				peer->crypto &= ~CRYPTO_FLAG_AUTO;
			peer->crypto |= CRYPTO_FLAG_AGREE;
			peer->flash &= ~TEST8;
			sprintf(statstr, "cook %x ts %u fs %u",
			    peer->pcookie, ntohl(ep->tstamp),
			    ntohl(ep->fstamp));
			record_crypto_stats(&peer->srcadr, statstr);
#ifdef DEBUG
			if (debug)
				printf("crypto_recv: %s\n", statstr);
#endif
			break;

		/*
		 * Install autokey values in broadcast client and
		 * symmetric modes. We have to do this every time the
		 * sever/peer cookie changes or a new keylist is
		 * rolled. Ordinarily, this is automatic as this message
		 * is piggybacked on the first NTP packet sent upon
		 * either of these events. Note that a broadcast client
		 * or symmetric peer can receive this response without a
		 * matching request.
		 */
		case CRYPTO_AUTO | CRYPTO_RESP:

			/*
			 * Discard the message if invalid or identity
			 * not confirmed or signature not verified with
			 * respect to the receive autokey values.
			 */
			if (!(peer->crypto & CRYPTO_FLAG_VRFY)) {
				rval = XEVNT_ERR;
				break;
			}
			if ((rval = crypto_verify(ep, &peer->recval,
			    peer)) != XEVNT_OK)
				break;

			/*
			 * Install autokey values and light the
			 * autokey bit. This is not hard.
			 */
			if (peer->recval.ptr == NULL)
				peer->recval.ptr =
				    emalloc(sizeof(struct autokey));
			bp = (struct autokey *)peer->recval.ptr;
			peer->recval.tstamp = ep->tstamp;
			peer->recval.fstamp = ep->fstamp;
			ap = (struct autokey *)ep->pkt;
			bp->seq = ntohl(ap->seq);
			bp->key = ntohl(ap->key);
			peer->pkeyid = bp->key;
			peer->crypto |= CRYPTO_FLAG_AUTO;
			peer->flash &= ~TEST8;
			sprintf(statstr,
			    "auto seq %d key %x ts %u fs %u", bp->seq,
			    bp->key, ntohl(ep->tstamp),
			    ntohl(ep->fstamp));
			record_crypto_stats(&peer->srcadr, statstr);
#ifdef DEBUG
			if (debug)
				printf("crypto_recv: %s\n", statstr);
#endif
			break;
	
		/*
		 * X509 certificate sign response. Validate the
		 * certificate signed by the server and install. Later
		 * this can be provided to clients of this server in
		 * lieu of the self signed certificate in order to
		 * validate the public key.
		 */
		case CRYPTO_SIGN | CRYPTO_RESP:

			/*
			 * Discard the message if invalid or not
			 * proventic.
			 */
			if (!(peer->crypto & CRYPTO_FLAG_PROV)) {
				rval = XEVNT_ERR;
				break;
			}
			if ((rval = crypto_verify(ep, NULL, peer)) !=
			    XEVNT_OK)
				break;

			/*
			 * Scan the certificate list to delete old
			 * versions and link the newest version first on
			 * the list.
			 */
			if ((rval = cert_install(ep, peer)) != XEVNT_OK)
				break;

			peer->crypto |= CRYPTO_FLAG_SIGN;
			peer->flash &= ~TEST8;
			temp32 = cinfo->nid;
			sprintf(statstr, "sign %s 0x%x %s (%u) fs %u",
			    cinfo->issuer, cinfo->flags,
			    OBJ_nid2ln(temp32), temp32,
			    ntohl(ep->fstamp));
			record_crypto_stats(&peer->srcadr, statstr);
#ifdef DEBUG
			if (debug)
				printf("crypto_recv: %s\n", statstr);
#endif
			break;

		/*
		 * Install leapseconds table in symmetric modes. This
		 * table is proventicated to the NIST primary servers,
		 * either by copying the file containing the table from
		 * a NIST server to a trusted server or directly using
		 * this protocol. While the entire table is installed at
		 * the server, presently only the current TAI offset is
		 * provided via the kernel to other applications.
		 */
		case CRYPTO_TAI:

			/*
			 * Discard the message if invalid.
			 */
			if ((rval = crypto_verify(ep, NULL, peer)) !=
			    XEVNT_OK)
				break;

			/*
			 * Pass the extension field to the transmit
			 * side. Continue below if a leapseconds table
			 * accompanies the message.
			 */
			fp = emalloc(len);
			memcpy(fp, ep, len);
			temp32 = CRYPTO_RESP;
			fp->opcode |= htonl(temp32);
			peer->cmmd = fp;
			if (len <= VALUE_LEN) {
				peer->flash &= ~TEST8;
				break;
			}
			/* fall through */

		case CRYPTO_TAI | CRYPTO_RESP:

			/*
			 * If this is a response, discard the message if
			 * signature not verified with respect to the
			 * leapsecond table values.
			 */
			if (peer->cmmd == NULL) {
				if ((rval = crypto_verify(ep,
				    &peer->tai_leap, peer)) != XEVNT_OK)
					break;
			}

			/*
			 * Initialize peer variables with latest update.
			 */
			peer->tai_leap.tstamp = ep->tstamp;
			peer->tai_leap.fstamp = ep->fstamp;
			peer->tai_leap.vallen = ep->vallen;

			/*
			 * Install the new table if there is no stored
			 * table or the new table is more recent than
			 * the stored table. Since a filestamp may have
			 * changed, recompute the signatures.
			 */
			if (ntohl(peer->tai_leap.fstamp) >
			    ntohl(tai_leap.fstamp)) {
				tai_leap.fstamp = ep->fstamp;
				tai_leap.vallen = ep->vallen;
				if (tai_leap.ptr != NULL)
					free(tai_leap.ptr);
				tai_leap.ptr = emalloc(vallen);
				memcpy(tai_leap.ptr, ep->pkt, vallen);
				crypto_update();
			}
			crypto_flags |= CRYPTO_FLAG_TAI;
			peer->crypto |= CRYPTO_FLAG_LEAP;
			peer->flash &= ~TEST8;
			sprintf(statstr, "leap %u ts %u fs %u", vallen,
			    ntohl(ep->tstamp), ntohl(ep->fstamp));
			record_crypto_stats(&peer->srcadr, statstr);
#ifdef DEBUG
			if (debug)
				printf("crypto_recv: %s\n", statstr);
#endif
			break;

		/*
		 * We come here in symmetric modes for miscellaneous
		 * commands that have value fields but are processed on
		 * the transmit side. All we need do here is check for
		 * valid field length. Remaining checks are below and on
		 * the transmit side.
		 */
		case CRYPTO_CERT:
		case CRYPTO_IFF:
		case CRYPTO_GQ:
		case CRYPTO_MV:
		case CRYPTO_SIGN:
			if (len < VALUE_LEN) {
				rval = XEVNT_LEN;
				break;
			}
			/* fall through */

		/*
		 * We come here for miscellaneous requests and unknown
		 * requests and responses. If an unknown response or
		 * error, forget it. If a request, save the extension
		 * field for later. Unknown requests will be caught on
		 * the transmit side.
		 */
		default:
			if (code & (CRYPTO_RESP | CRYPTO_ERROR)) {
				rval = XEVNT_ERR;
			} else if ((rval = crypto_verify(ep, NULL,
			    peer)) == XEVNT_OK) {
				fp = emalloc(len);
				memcpy(fp, ep, len);
				temp32 = CRYPTO_RESP;
				fp->opcode |= htonl(temp32);
				peer->cmmd = fp;
			}
		}

		/*
		 * We don't log length/format/timestamp errors and
		 * duplicates, which are log clogging vulnerabilities.
		 * The first error found terminates the extension field
		 * scan and we return the laundry to the caller. A
		 * length/format/timestamp error on transmit is
		 * cheerfully ignored, as the message is not sent.
		 */
		if (rval > XEVNT_TSP) {
			sprintf(statstr,
			    "error %x opcode %x ts %u fs %u", rval,
			    code, tstamp, fstamp);
			record_crypto_stats(&peer->srcadr, statstr);
			report_event(rval, peer);
#ifdef DEBUG
			if (debug)
				printf("crypto_recv: %s\n", statstr);
#endif
			break;

		} else if (rval > XEVNT_OK && (code & CRYPTO_RESP)) {
			rval = XEVNT_OK;
		}
		authlen += len;
	}
	return (rval);
}


/*
 * crypto_xmit - construct extension fields
 *
 * This routine is called both when an association is configured and
 * when one is not. The only case where this matters is to retrieve the
 * autokey information, in which case the caller has to provide the
 * association ID to match the association.
 *
 * Returns length of extension field.
 */
int
crypto_xmit(
	struct pkt *xpkt,	/* transmit packet pointer */
	struct sockaddr_storage *srcadr_sin,	/* active runway */
	int	start,		/* offset to extension field */
	struct exten *ep,	/* extension pointer */
	keyid_t cookie		/* session cookie */
	)
{
	u_int32	*pkt;		/* packet pointer */
	struct peer *peer;	/* peer structure pointer */
	u_int	opcode;		/* extension field opcode */
	struct exten *fp;	/* extension pointers */
	struct cert_info *cp, *xp; /* certificate info/value pointer */
	char	certname[MAXHOSTNAME + 1]; /* subject name buffer */
	char	statstr[NTP_MAXSTRLEN]; /* statistics for filegen */
	tstamp_t tstamp;
	u_int	vallen;
	u_int	len;
	struct value vtemp;
	associd_t associd;
	int	rval;
	keyid_t tcookie;

	/*
	 * Generate the requested extension field request code, length
	 * and association ID. If this is a response and the host is not
	 * synchronized, light the error bit and go home.
	 */
	pkt = (u_int32 *)xpkt + start / 4;
	fp = (struct exten *)pkt;
	opcode = ntohl(ep->opcode);
	associd = (associd_t) ntohl(ep->associd);
	fp->associd = htonl(associd);
	len = 8;
	rval = XEVNT_OK;
	tstamp = crypto_time();
	switch (opcode & 0xffff0000) {

	/*
	 * Send association request and response with status word and
	 * host name. Note, this message is not signed and the filestamp
	 * contains only the status word.
	 */
	case CRYPTO_ASSOC | CRYPTO_RESP:
		len += crypto_send(fp, &hostval);
		fp->fstamp = htonl(crypto_flags);
		break;

	case CRYPTO_ASSOC:
		len += crypto_send(fp, &hostval);
		fp->fstamp = htonl(crypto_flags | ident_scheme);
		break;

	/*
	 * Send certificate request. Use the values from the extension
	 * field.
	 */
	case CRYPTO_CERT:
		memset(&vtemp, 0, sizeof(vtemp));
		vtemp.tstamp = ep->tstamp;
		vtemp.fstamp = ep->fstamp;
		vtemp.vallen = ep->vallen;
		vtemp.ptr = (u_char *)ep->pkt;
		len += crypto_send(fp, &vtemp);
		break;

	/*
	 * Send certificate response or sign request. Use the values
	 * from the certificate cache. If the request contains no
	 * subject name, assume the name of this host. This is for
	 * backwards compatibility. Private certificates are never sent.
	 */
	case CRYPTO_SIGN:
	case CRYPTO_CERT | CRYPTO_RESP:
		vallen = ntohl(ep->vallen);
		if (vallen == 8) {
			strcpy(certname, sys_hostname);
		} else if (vallen == 0 || vallen > MAXHOSTNAME) {
			rval = XEVNT_LEN;
			break;

		} else {
			memcpy(certname, ep->pkt, vallen);
			certname[vallen] = '\0';
		}

		/*
		 * Find all certificates with matching subject. If a
		 * self-signed, trusted certificate is found, use that.
		 * If not, use the first one with matching subject. A
		 * private certificate is never divulged or signed.
		 */
		xp = NULL;
		for (cp = cinfo; cp != NULL; cp = cp->link) {
			if (cp->flags & CERT_PRIV)
				continue;

			if (strcmp(certname, cp->subject) == 0) {
				if (xp == NULL)
					xp = cp;
				if (strcmp(certname, cp->issuer) ==
				    0 && cp->flags & CERT_TRUST) {
					xp = cp;
					break;
				}
			}
		}

		/*
		 * Be careful who you trust. If not yet synchronized,
		 * give back an empty response. If certificate not found
		 * or beyond the lifetime, return an error. This is to
		 * avoid a bad dude trying to get an expired certificate
		 * re-signed. Otherwise, send it.
		 *
		 * Note the timestamp and filestamp are taken from the
		 * certificate value structure. For all certificates the
		 * timestamp is the latest signature update time. For
		 * host and imported certificates the filestamp is the
		 * creation epoch. For signed certificates the filestamp
		 * is the creation epoch of the trusted certificate at
		 * the base of the certificate trail. In principle, this
		 * allows strong checking for signature masquerade.
		 */
		if (tstamp == 0)
			break;

		if (xp == NULL)
			rval = XEVNT_CRT;
		else if (tstamp < xp->first || tstamp > xp->last)
			rval = XEVNT_SRV;
		else
			len += crypto_send(fp, &xp->cert);
		break;

	/*
	 * Send challenge in Schnorr (IFF) identity scheme.
	 */
	case CRYPTO_IFF:
		if ((peer = findpeerbyassoc(ep->pkt[0])) == NULL) {
			rval = XEVNT_ERR;
			break;
		}
		if ((rval = crypto_alice(peer, &vtemp)) == XEVNT_OK) {
			len += crypto_send(fp, &vtemp);
			value_free(&vtemp);
		}
		break;

	/*
	 * Send response in Schnorr (IFF) identity scheme.
	 */
	case CRYPTO_IFF | CRYPTO_RESP:
		if ((rval = crypto_bob(ep, &vtemp)) == XEVNT_OK) {
			len += crypto_send(fp, &vtemp);
			value_free(&vtemp);
		}
		break;

	/*
	 * Send challenge in Guillou-Quisquater (GQ) identity scheme.
	 */
	case CRYPTO_GQ:
		if ((peer = findpeerbyassoc(ep->pkt[0])) == NULL) {
			rval = XEVNT_ERR;
			break;
		}
		if ((rval = crypto_alice2(peer, &vtemp)) == XEVNT_OK) {
			len += crypto_send(fp, &vtemp);
			value_free(&vtemp);
		}
		break;

	/*
	 * Send response in Guillou-Quisquater (GQ) identity scheme.
	 */
	case CRYPTO_GQ | CRYPTO_RESP:
		if ((rval = crypto_bob2(ep, &vtemp)) == XEVNT_OK) {
			len += crypto_send(fp, &vtemp);
			value_free(&vtemp);
		}
		break;

	/*
	 * Send challenge in MV identity scheme.
	 */
	case CRYPTO_MV:
		if ((peer = findpeerbyassoc(ep->pkt[0])) == NULL) {
			rval = XEVNT_ERR;
			break;
		}
		if ((rval = crypto_alice3(peer, &vtemp)) == XEVNT_OK) {
			len += crypto_send(fp, &vtemp);
			value_free(&vtemp);
		}
		break;

	/*
	 * Send response in MV identity scheme.
	 */
	case CRYPTO_MV | CRYPTO_RESP:
		if ((rval = crypto_bob3(ep, &vtemp)) == XEVNT_OK) {
			len += crypto_send(fp, &vtemp);
			value_free(&vtemp);
		}
		break;

	/*
	 * Send certificate sign response. The integrity of the request
	 * certificate has already been verified on the receive side.
	 * Sign the response using the local server key. Use the
	 * filestamp from the request and use the timestamp as the
	 * current time. Light the error bit if the certificate is
	 * invalid or contains an unverified signature.
	 */
	case CRYPTO_SIGN | CRYPTO_RESP:
		if ((rval = cert_sign(ep, &vtemp)) == XEVNT_OK)
			len += crypto_send(fp, &vtemp);
		value_free(&vtemp);
		break;

	/*
	 * Send public key and signature. Use the values from the public
	 * key.
	 */
	case CRYPTO_COOK:
		len += crypto_send(fp, &pubkey);
		break;

	/*
	 * Encrypt and send cookie and signature. Light the error bit if
	 * anything goes wrong.
	 */
	case CRYPTO_COOK | CRYPTO_RESP:
		if ((opcode & 0xffff) < VALUE_LEN) {
			rval = XEVNT_LEN;
			break;
		}
		if (PKT_MODE(xpkt->li_vn_mode) == MODE_SERVER) {
			tcookie = cookie;
		} else {
			if ((peer = findpeerbyassoc(associd)) == NULL) {
				rval = XEVNT_ERR;
				break;
			}
			tcookie = peer->pcookie;
		}
		if ((rval = crypto_encrypt(ep, &vtemp, &tcookie)) ==
		    XEVNT_OK)
			len += crypto_send(fp, &vtemp);
		value_free(&vtemp);
		break;

	/*
	 * Find peer and send autokey data and signature in broadcast
	 * server and symmetric modes. Use the values in the autokey
	 * structure. If no association is found, either the server has
	 * restarted with new associations or some perp has replayed an
	 * old message, in which case light the error bit.
	 */
	case CRYPTO_AUTO | CRYPTO_RESP:
		if ((peer = findpeerbyassoc(associd)) == NULL) {
			rval = XEVNT_ERR;
			break;
		}
		peer->flags &= ~FLAG_ASSOC;
		len += crypto_send(fp, &peer->sndval);
		break;

	/*
	 * Send leapseconds table and signature. Use the values from the
	 * tai structure. If no table has been loaded, just send an
	 * empty request.
	 */
	case CRYPTO_TAI:
	case CRYPTO_TAI | CRYPTO_RESP:
		if (crypto_flags & CRYPTO_FLAG_TAI)
			len += crypto_send(fp, &tai_leap);
		break;

	/*
	 * Default - Fall through for requests; for unknown responses,
	 * flag as error.
	 */
	default:
		if (opcode & CRYPTO_RESP)
			rval = XEVNT_ERR;
	}

	/*
	 * In case of error, flame the log. If a request, toss the
	 * puppy; if a response, return so the sender can flame, too.
	 */
	if (rval != XEVNT_OK) {
		opcode |= CRYPTO_ERROR;
		sprintf(statstr, "error %x opcode %x", rval, opcode);
		record_crypto_stats(srcadr_sin, statstr);
		report_event(rval, NULL);
#ifdef DEBUG
		if (debug)
			printf("crypto_xmit: %s\n", statstr);
#endif
		if (!(opcode & CRYPTO_RESP))
			return (0);
	}

	/*
	 * Round up the field length to a multiple of 8 bytes and save
	 * the request code and length.
	 */
	len = ((len + 7) / 8) * 8;
	fp->opcode = htonl((opcode & 0xffff0000) | len);
#ifdef DEBUG
	if (debug)
		printf(
		    "crypto_xmit: flags 0x%x ext offset %d len %u code 0x%x assocID %d\n",
		    crypto_flags, start, len, opcode >> 16, associd);
#endif
	return (len);
}


/*
 * crypto_verify - parse and verify the extension field and value
 *
 * Returns
 * XEVNT_OK	success
 * XEVNT_LEN	bad field format or length
 * XEVNT_TSP	bad timestamp
 * XEVNT_FSP	bad filestamp
 * XEVNT_PUB	bad or missing public key
 * XEVNT_SGL	bad signature length
 * XEVNT_SIG	signature not verified
 * XEVNT_ERR	protocol error
 */
static int
crypto_verify(
	struct exten *ep,	/* extension pointer */
	struct value *vp,	/* value pointer */
	struct peer *peer	/* peer structure pointer */
	)
{
	EVP_PKEY *pkey;		/* server public key */
	EVP_MD_CTX ctx;		/* signature context */
	tstamp_t tstamp, tstamp1 = 0; /* timestamp */
	tstamp_t fstamp, fstamp1 = 0; /* filestamp */
	u_int	vallen;		/* value length */
	u_int	siglen;		/* signature length */
	u_int	opcode, len;
	int	i;

	/*
	 * We require valid opcode and field lengths, timestamp,
	 * filestamp, public key, digest, signature length and
	 * signature, where relevant. Note that preliminary length
	 * checks are done in the main loop.
	 */
	len = ntohl(ep->opcode) & 0x0000ffff;
	opcode = ntohl(ep->opcode) & 0xffff0000;

	/*
	 * Check for valid operation code and protocol. The opcode must
	 * not have the error bit set. If a response, it must have a
	 * value header. If a request and does not contain a value
	 * header, no need for further checking.
	 */
	if (opcode & CRYPTO_ERROR)
		return (XEVNT_ERR);

 	if (opcode & CRYPTO_RESP) {
 		if (len < VALUE_LEN)
			return (XEVNT_LEN);
	} else {
 		if (len < VALUE_LEN)
			return (XEVNT_OK);
	}

	/*
	 * We have a value header. Check for valid field lengths. The
	 * field length must be long enough to contain the value header,
	 * value and signature. Note both the value and signature fields
	 * are rounded up to the next word.
	 */
	vallen = ntohl(ep->vallen);
	i = (vallen + 3) / 4;
	siglen = ntohl(ep->pkt[i++]);
	if (len < VALUE_LEN + ((vallen + 3) / 4) * 4 + ((siglen + 3) /
	    4) * 4)
		return (XEVNT_LEN);

	/*
	 * Punt if this is a response with no data. Punt if this is a
	 * request and a previous response is pending. 
	 */
	if (opcode & CRYPTO_RESP) {
		if (vallen == 0)
			return (XEVNT_LEN);
	} else {
		if (peer->cmmd != NULL)
			return (XEVNT_LEN);
	}

	/*
	 * Check for valid timestamp and filestamp. If the timestamp is
	 * zero, the sender is not synchronized and signatures are
	 * disregarded. If not, the timestamp must not precede the
	 * filestamp. The timestamp and filestamp must not precede the
	 * corresponding values in the value structure, if present. Once
	 * the autokey values have been installed, the timestamp must
	 * always be later than the corresponding value in the value
	 * structure. Duplicate timestamps are illegal once the cookie
	 * has been validated.
	 */
	tstamp = ntohl(ep->tstamp);
	fstamp = ntohl(ep->fstamp);
	if (tstamp == 0)
		return (XEVNT_OK);

	if (tstamp < fstamp)
		return (XEVNT_TSP);

	if (vp != NULL) {
		tstamp1 = ntohl(vp->tstamp);
		fstamp1 = ntohl(vp->fstamp);
		if ((tstamp < tstamp1 || (tstamp == tstamp1 &&
		    (peer->crypto & CRYPTO_FLAG_AUTO))))
			return (XEVNT_TSP);

		if ((tstamp < fstamp1 || fstamp < fstamp1))
			return (XEVNT_FSP);
	}

	/*
	 * Check for valid signature length, public key and digest
	 * algorithm.
	 */
	if (crypto_flags & peer->crypto & CRYPTO_FLAG_PRIV)
		pkey = sign_pkey;
	else
		pkey = peer->pkey;
	if (siglen == 0 || pkey == NULL || peer->digest == NULL)
		return (XEVNT_OK);

	if (siglen != (u_int)EVP_PKEY_size(pkey))
		return (XEVNT_SGL);

	/*
	 * Darn, I thought we would never get here. Verify the
	 * signature. If the identity exchange is verified, light the
	 * proventic bit. If no client identity scheme is specified,
	 * avoid doing the sign exchange.
	 */
	EVP_VerifyInit(&ctx, peer->digest);
	EVP_VerifyUpdate(&ctx, (u_char *)&ep->tstamp, vallen + 12);
	if (!EVP_VerifyFinal(&ctx, (u_char *)&ep->pkt[i], siglen, pkey))
		return (XEVNT_SIG);

	if (peer->crypto & CRYPTO_FLAG_VRFY) {
		peer->crypto |= CRYPTO_FLAG_PROV;
		if (!(crypto_flags & CRYPTO_FLAG_MASK))
			peer->crypto |= CRYPTO_FLAG_SIGN;
	}
	return (XEVNT_OK);
}


/*
 * crypto_encrypt - construct encrypted cookie and signature from
 * extension field and cookie
 *
 * Returns
 * XEVNT_OK	success
 * XEVNT_PUB	bad or missing public key
 * XEVNT_CKY	bad or missing cookie
 * XEVNT_PER	host certificate expired
 */
static int
crypto_encrypt(
	struct exten *ep,	/* extension pointer */
	struct value *vp,	/* value pointer */
	keyid_t	*cookie		/* server cookie */
	)
{
	EVP_PKEY *pkey;		/* public key */
	EVP_MD_CTX ctx;		/* signature context */
	tstamp_t tstamp;	/* NTP timestamp */
	u_int32	temp32;
	u_int	len;
	u_char	*ptr;

	/*
	 * Extract the public key from the request.
	 */
	len = ntohl(ep->vallen);
	ptr = (u_char *)ep->pkt;
	pkey = d2i_PublicKey(EVP_PKEY_RSA, NULL, &ptr, len);
	if (pkey == NULL) {
		msyslog(LOG_ERR, "crypto_encrypt %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		return (XEVNT_PUB);
	}

	/*
	 * Encrypt the cookie, encode in ASN.1 and sign.
	 */
	tstamp = crypto_time();
	memset(vp, 0, sizeof(struct value));
	vp->tstamp = htonl(tstamp);
	vp->fstamp = hostval.tstamp;
	len = EVP_PKEY_size(pkey);
	vp->vallen = htonl(len);
	vp->ptr = emalloc(len);
	temp32 = htonl(*cookie);
	if (!RSA_public_encrypt(4, (u_char *)&temp32, vp->ptr,
	    pkey->pkey.rsa, RSA_PKCS1_OAEP_PADDING)) {
		msyslog(LOG_ERR, "crypto_encrypt %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		EVP_PKEY_free(pkey);
		return (XEVNT_CKY);
	}
	EVP_PKEY_free(pkey);
	vp->siglen = 0;
	if (tstamp == 0)
		return (XEVNT_OK);

	if (tstamp < cinfo->first || tstamp > cinfo->last)
		return (XEVNT_PER);

	vp->sig = emalloc(sign_siglen);
	EVP_SignInit(&ctx, sign_digest);
	EVP_SignUpdate(&ctx, (u_char *)&vp->tstamp, 12);
	EVP_SignUpdate(&ctx, vp->ptr, len);
	if (EVP_SignFinal(&ctx, vp->sig, &len, sign_pkey))
		vp->siglen = htonl(len);
	return (XEVNT_OK);
}


/*
 * crypto_ident - construct extension field for identity scheme
 *
 * This routine determines which identity scheme is in use and
 * constructs an extension field for that scheme.
 */
u_int
crypto_ident(
	struct peer *peer	/* peer structure pointer */
	)
{
	char	filename[MAXFILENAME + 1];

	/*
	 * If the server identity has already been verified, no further
	 * action is necessary. Otherwise, try to load the identity file
	 * of the certificate issuer. If the issuer file is not found,
	 * try the host file. If nothing found, declare a cryptobust.
	 * Note we can't get here unless the trusted certificate has
	 * been found and the CRYPTO_FLAG_VALID bit is set, so the
	 * certificate issuer is valid.
	 */
	if (peer->ident_pkey != NULL)
		EVP_PKEY_free(peer->ident_pkey);
	if (peer->crypto & CRYPTO_FLAG_GQ) {
		snprintf(filename, MAXFILENAME, "ntpkey_gq_%s",
		    peer->issuer);
		peer->ident_pkey = crypto_key(filename, &peer->fstamp);
		if (peer->ident_pkey != NULL)
			return (CRYPTO_GQ);

		snprintf(filename, MAXFILENAME, "ntpkey_gq_%s",
		    sys_hostname);
		peer->ident_pkey = crypto_key(filename, &peer->fstamp);
		if (peer->ident_pkey != NULL)
			return (CRYPTO_GQ);
	}
	if (peer->crypto & CRYPTO_FLAG_IFF) {
		snprintf(filename, MAXFILENAME, "ntpkey_iff_%s",
		    peer->issuer);
		peer->ident_pkey = crypto_key(filename, &peer->fstamp);
		if (peer->ident_pkey != NULL)
			return (CRYPTO_IFF);

		snprintf(filename, MAXFILENAME, "ntpkey_iff_%s",
		    sys_hostname);
		peer->ident_pkey = crypto_key(filename, &peer->fstamp);
		if (peer->ident_pkey != NULL)
			return (CRYPTO_IFF);
	}
	if (peer->crypto & CRYPTO_FLAG_MV) {
		snprintf(filename, MAXFILENAME, "ntpkey_mv_%s",
		    peer->issuer);
		peer->ident_pkey = crypto_key(filename, &peer->fstamp);
		if (peer->ident_pkey != NULL)
			return (CRYPTO_MV);

		snprintf(filename, MAXFILENAME, "ntpkey_mv_%s",
		    sys_hostname);
		peer->ident_pkey = crypto_key(filename, &peer->fstamp);
		if (peer->ident_pkey != NULL)
			return (CRYPTO_MV);
	}

	/*
	 * No compatible identity scheme is available. Life is hard.
	 */
	msyslog(LOG_INFO,
	    "crypto_ident: no compatible identity scheme found");
	return (0);
}


/*
 * crypto_args - construct extension field from arguments
 *
 * This routine creates an extension field with current timestamps and
 * specified opcode, association ID and optional string. Note that the
 * extension field is created here, but freed after the crypto_xmit()
 * call in the protocol module.
 *
 * Returns extension field pointer (no errors).
 */
struct exten *
crypto_args(
	struct peer *peer,	/* peer structure pointer */
	u_int	opcode,		/* operation code */
	char	*str		/* argument string */
	)
{
	tstamp_t tstamp;	/* NTP timestamp */
	struct exten *ep;	/* extension field pointer */
	u_int	len;		/* extension field length */

	tstamp = crypto_time();
	len = sizeof(struct exten);
	if (str != NULL)
		len += strlen(str);
	ep = emalloc(len);
	memset(ep, 0, len);
	if (opcode == 0)
		return (ep);

	ep->opcode = htonl(opcode + len);

	/*
	 * If a response, send our ID; if a request, send the
	 * responder's ID.
	 */
	if (opcode & CRYPTO_RESP)
		ep->associd = htonl(peer->associd);
	else
		ep->associd = htonl(peer->assoc);
	ep->tstamp = htonl(tstamp);
	ep->fstamp = hostval.tstamp;
	ep->vallen = 0;
	if (str != NULL) {
		ep->vallen = htonl(strlen(str));
		memcpy((char *)ep->pkt, str, strlen(str));
	} else {
		ep->pkt[0] = peer->associd;
	}
	return (ep);
}


/*
 * crypto_send - construct extension field from value components
 *
 * Returns extension field length. Note: it is not polite to send a
 * nonempty signature with zero timestamp or a nonzero timestamp with
 * empty signature, but these rules are not enforced here.
 */
u_int
crypto_send(
	struct exten *ep,	/* extension field pointer */
	struct value *vp	/* value pointer */
	)
{
	u_int	len, temp32;
	int	i;

	/*
	 * Copy data. If the data field is empty or zero length, encode
	 * an empty value with length zero.
	 */
	ep->tstamp = vp->tstamp;
	ep->fstamp = vp->fstamp;
	ep->vallen = vp->vallen;
	len = 12;
	temp32 = ntohl(vp->vallen);
	if (temp32 > 0 && vp->ptr != NULL)
		memcpy(ep->pkt, vp->ptr, temp32);

	/*
	 * Copy signature. If the signature field is empty or zero
	 * length, encode an empty signature with length zero.
	 */
	i = (temp32 + 3) / 4;
	len += i * 4 + 4;
	ep->pkt[i++] = vp->siglen;
	temp32 = ntohl(vp->siglen);
	if (temp32 > 0 && vp->sig != NULL)
		memcpy(&ep->pkt[i], vp->sig, temp32);
	len += temp32;
	return (len);
}


/*
 * crypto_update - compute new public value and sign extension fields
 *
 * This routine runs periodically, like once a day, and when something
 * changes. It updates the timestamps on three value structures and one
 * value structure list, then signs all the structures:
 *
 * hostval	host name (not signed)
 * pubkey	public key
 * cinfo	certificate info/value list
 * tai_leap	leapseconds file
 *
 * Filestamps are proventicated data, so this routine is run only when
 * the host has been synchronized to a proventicated source. Thus, the
 * timestamp is proventicated, too, and can be used to deflect
 * clogging attacks and even cook breakfast.
 *
 * Returns void (no errors)
 */
void
crypto_update(void)
{
	EVP_MD_CTX ctx;		/* message digest context */
	struct cert_info *cp, *cpn; /* certificate info/value */
	char	statstr[NTP_MAXSTRLEN]; /* statistics for filegen */
	tstamp_t tstamp;	/* NTP timestamp */
	u_int	len;

	if ((tstamp = crypto_time()) == 0)
		return;

	hostval.tstamp = htonl(tstamp);

	/*
	 * Sign public key and timestamps. The filestamp is derived from
	 * the host key file extension from wherever the file was
	 * generated. 
	 */
	if (pubkey.vallen != 0) {
		pubkey.tstamp = hostval.tstamp;
		pubkey.siglen = 0;
		if (pubkey.sig == NULL)
			pubkey.sig = emalloc(sign_siglen);
		EVP_SignInit(&ctx, sign_digest);
		EVP_SignUpdate(&ctx, (u_char *)&pubkey, 12);
		EVP_SignUpdate(&ctx, pubkey.ptr, ntohl(pubkey.vallen));
		if (EVP_SignFinal(&ctx, pubkey.sig, &len, sign_pkey))
			pubkey.siglen = htonl(len);
	}

	/*
	 * Sign certificates and timestamps. The filestamp is derived
	 * from the certificate file extension from wherever the file
	 * was generated. Note we do not throw expired certificates
	 * away; they may have signed younger ones.
	 */
	for (cp = cinfo; cp != NULL; cp = cpn) {
		cpn = cp->link;
		cp->cert.tstamp = hostval.tstamp;
		cp->cert.siglen = 0;
		if (cp->cert.sig == NULL)
			cp->cert.sig = emalloc(sign_siglen);
		EVP_SignInit(&ctx, sign_digest);
		EVP_SignUpdate(&ctx, (u_char *)&cp->cert, 12);
		EVP_SignUpdate(&ctx, cp->cert.ptr,
		    ntohl(cp->cert.vallen));
		if (EVP_SignFinal(&ctx, cp->cert.sig, &len, sign_pkey))
			cp->cert.siglen = htonl(len);
	}

	/*
	 * Sign leapseconds table and timestamps. The filestamp is
	 * derived from the leapsecond file extension from wherever the
	 * file was generated.
	 */
	if (tai_leap.vallen != 0) {
		tai_leap.tstamp = hostval.tstamp;
		tai_leap.siglen = 0;
		if (tai_leap.sig == NULL)
			tai_leap.sig = emalloc(sign_siglen);
		EVP_SignInit(&ctx, sign_digest);
		EVP_SignUpdate(&ctx, (u_char *)&tai_leap, 12);
		EVP_SignUpdate(&ctx, tai_leap.ptr,
		    ntohl(tai_leap.vallen));
		if (EVP_SignFinal(&ctx, tai_leap.sig, &len, sign_pkey))
			tai_leap.siglen = htonl(len);
	}
	sprintf(statstr, "update ts %u", ntohl(hostval.tstamp)); 
	record_crypto_stats(NULL, statstr);
#ifdef DEBUG
	if (debug)
		printf("crypto_update: %s\n", statstr);
#endif
}


/*
 * value_free - free value structure components.
 *
 * Returns void (no errors)
 */
void
value_free(
	struct value *vp	/* value structure */
	)
{
	if (vp->ptr != NULL)
		free(vp->ptr);
	if (vp->sig != NULL)
		free(vp->sig);
	memset(vp, 0, sizeof(struct value));
}


/*
 * crypto_time - returns current NTP time in seconds.
 */
tstamp_t
crypto_time()
{
	l_fp	tstamp;		/* NTP time */	L_CLR(&tstamp);

	L_CLR(&tstamp);
	if (sys_leap != LEAP_NOTINSYNC)
		get_systime(&tstamp);
	return (tstamp.l_ui);
}


/*
 * asn2ntp - convert ASN1_TIME time structure to NTP time in seconds.
 */
u_long
asn2ntp	(
	ASN1_TIME *asn1time	/* pointer to ASN1_TIME structure */
	)
{
	char	*v;		/* pointer to ASN1_TIME string */
	struct	tm tm;		/* used to convert to NTP time */

	/*
	 * Extract time string YYMMDDHHMMSSZ from ASN1 time structure.
	 * Note that the YY, MM, DD fields start with one, the HH, MM,
	 * SS fiels start with zero and the Z character should be 'Z'
	 * for UTC. Also note that years less than 50 map to years
	 * greater than 100. Dontcha love ASN.1? Better than MIL-188.
	 */
	if (asn1time->length > 13)
		return ((u_long)(~0));	/* We can't use -1 here. It's invalid */

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
	return (timegm(&tm) + JAN_1970);
}


/*
 * bigdig() - compute a BIGNUM MD5 hash of a BIGNUM number.
 */
static int
bighash(
	BIGNUM	*bn,		/* BIGNUM * from */
	BIGNUM	*bk		/* BIGNUM * to */
	)
{
	EVP_MD_CTX ctx;		/* message digest context */
	u_char dgst[EVP_MAX_MD_SIZE]; /* message digest */
	u_char	*ptr;		/* a BIGNUM as binary string */
	u_int	len;

	len = BN_num_bytes(bn);
	ptr = emalloc(len);
	BN_bn2bin(bn, ptr);
	EVP_DigestInit(&ctx, EVP_md5());
	EVP_DigestUpdate(&ctx, ptr, len);
	EVP_DigestFinal(&ctx, dgst, &len);
	BN_bin2bn(dgst, len, bk);

	/* XXX MEMLEAK? free ptr? */

	return (1);
}


/*
 ***********************************************************************
 *								       *
 * The following routines implement the Schnorr (IFF) identity scheme  *
 *								       *
 ***********************************************************************
 *
 * The Schnorr (IFF) identity scheme is intended for use when
 * the ntp-genkeys program does not generate the certificates used in
 * the protocol and the group key cannot be conveyed in the certificate
 * itself. For this purpose, new generations of IFF values must be
 * securely transmitted to all members of the group before use. The
 * scheme is self contained and independent of new generations of host
 * keys, sign keys and certificates.
 *
 * The IFF identity scheme is based on DSA cryptography and algorithms
 * described in Stinson p. 285. The IFF values hide in a DSA cuckoo
 * structure, but only the primes and generator are used. The p is a
 * 512-bit prime, q a 160-bit prime that divides p - 1 and is a qth root
 * of 1 mod p; that is, g^q = 1 mod p. The TA rolls primvate random
 * group key b disguised as a DSA structure member, then computes public
 * key g^(q - b). These values are shared only among group members and
 * never revealed in messages. Alice challenges Bob to confirm identity
 * using the protocol described below.
 *
 * How it works
 *
 * The scheme goes like this. Both Alice and Bob have the public primes
 * p, q and generator g. The TA gives private key b to Bob and public
 * key v = g^(q - a) mod p to Alice.
 *
 * Alice rolls new random challenge r and sends to Bob in the IFF
 * request message. Bob rolls new random k, then computes y = k + b r
 * mod q and x = g^k mod p and sends (y, hash(x)) to Alice in the
 * response message. Besides making the response shorter, the hash makes
 * it effectivey impossible for an intruder to solve for b by observing
 * a number of these messages.
 * 
 * Alice receives the response and computes g^y v^r mod p. After a bit
 * of algebra, this simplifies to g^k. If the hash of this result
 * matches hash(x), Alice knows that Bob has the group key b. The signed
 * response binds this knowledge to Bob's private key and the public key
 * previously received in his certificate.
 *
 * crypto_alice - construct Alice's challenge in IFF scheme
 *
 * Returns
 * XEVNT_OK	success
 * XEVNT_PUB	bad or missing public key
 * XEVNT_ID	bad or missing group key
 */
static int
crypto_alice(
	struct peer *peer,	/* peer pointer */
	struct value *vp	/* value pointer */
	)
{
	DSA	*dsa;		/* IFF parameters */
	BN_CTX	*bctx;		/* BIGNUM context */
	EVP_MD_CTX ctx;		/* signature context */
	tstamp_t tstamp;
	u_int	len;

	/*
	 * The identity parameters must have correct format and content.
	 */
	if (peer->ident_pkey == NULL)
		return (XEVNT_ID);

	if ((dsa = peer->ident_pkey->pkey.dsa) == NULL) {
		msyslog(LOG_INFO, "crypto_alice: defective key");
		return (XEVNT_PUB);
	}

	/*
	 * Roll new random r (0 < r < q). The OpenSSL library has a bug
	 * omitting BN_rand_range, so we have to do it the hard way.
	 */
	bctx = BN_CTX_new();
	len = BN_num_bytes(dsa->q);
	if (peer->iffval != NULL)
		BN_free(peer->iffval);
	peer->iffval = BN_new();
	BN_rand(peer->iffval, len * 8, -1, 1);	/* r */
	BN_mod(peer->iffval, peer->iffval, dsa->q, bctx);
	BN_CTX_free(bctx);

	/*
	 * Sign and send to Bob. The filestamp is from the local file.
	 */
	tstamp = crypto_time();
	memset(vp, 0, sizeof(struct value));
	vp->tstamp = htonl(tstamp);
	vp->fstamp = htonl(peer->fstamp);
	vp->vallen = htonl(len);
	vp->ptr = emalloc(len);
	BN_bn2bin(peer->iffval, vp->ptr);
	vp->siglen = 0;
	if (tstamp == 0)
		return (XEVNT_OK);

	if (tstamp < cinfo->first || tstamp > cinfo->last)
		return (XEVNT_PER);

	vp->sig = emalloc(sign_siglen);
	EVP_SignInit(&ctx, sign_digest);
	EVP_SignUpdate(&ctx, (u_char *)&vp->tstamp, 12);
	EVP_SignUpdate(&ctx, vp->ptr, len);
	if (EVP_SignFinal(&ctx, vp->sig, &len, sign_pkey))
		vp->siglen = htonl(len);
	return (XEVNT_OK);
}


/*
 * crypto_bob - construct Bob's response to Alice's challenge
 *
 * Returns
 * XEVNT_OK	success
 * XEVNT_ID	bad or missing group key
 * XEVNT_ERR	protocol error
 * XEVNT_PER	host expired certificate
 */
static int
crypto_bob(
	struct exten *ep,	/* extension pointer */
	struct value *vp	/* value pointer */
	)
{
	DSA	*dsa;		/* IFF parameters */
	DSA_SIG	*sdsa;		/* DSA signature context fake */
	BN_CTX	*bctx;		/* BIGNUM context */
	EVP_MD_CTX ctx;		/* signature context */
	tstamp_t tstamp;	/* NTP timestamp */
	BIGNUM	*bn, *bk, *r;
	u_char	*ptr;
	u_int	len;

	/*
	 * If the IFF parameters are not valid, something awful
	 * happened or we are being tormented.
	 */
	if (iffpar_pkey == NULL) {
		msyslog(LOG_INFO, "crypto_bob: scheme unavailable");
		return (XEVNT_ID);
	}
	dsa = iffpar_pkey->pkey.dsa;

	/*
	 * Extract r from the challenge.
	 */
	len = ntohl(ep->vallen);
	if ((r = BN_bin2bn((u_char *)ep->pkt, len, NULL)) == NULL) {
		msyslog(LOG_ERR, "crypto_bob %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		return (XEVNT_ERR);
	}

	/*
	 * Bob rolls random k (0 < k < q), computes y = k + b r mod q
	 * and x = g^k mod p, then sends (y, hash(x)) to Alice.
	 */
	bctx = BN_CTX_new(); bk = BN_new(); bn = BN_new();
	sdsa = DSA_SIG_new();
	BN_rand(bk, len * 8, -1, 1);		/* k */
	BN_mod_mul(bn, dsa->priv_key, r, dsa->q, bctx); /* b r mod q */
	BN_add(bn, bn, bk);
	BN_mod(bn, bn, dsa->q, bctx);		/* k + b r mod q */
	sdsa->r = BN_dup(bn);
	BN_mod_exp(bk, dsa->g, bk, dsa->p, bctx); /* g^k mod p */
	bighash(bk, bk);
	sdsa->s = BN_dup(bk);
	BN_CTX_free(bctx);
	BN_free(r); BN_free(bn); BN_free(bk);

	/*
	 * Encode the values in ASN.1 and sign.
	 */
	tstamp = crypto_time();
	memset(vp, 0, sizeof(struct value));
	vp->tstamp = htonl(tstamp);
	vp->fstamp = htonl(if_fstamp);
	len = i2d_DSA_SIG(sdsa, NULL);
	if (len <= 0) {
		msyslog(LOG_ERR, "crypto_bob %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		DSA_SIG_free(sdsa);
		return (XEVNT_ERR);
	}
	vp->vallen = htonl(len);
	ptr = emalloc(len);
	vp->ptr = ptr;
	i2d_DSA_SIG(sdsa, &ptr);
	DSA_SIG_free(sdsa);
	vp->siglen = 0;
	if (tstamp == 0)
		return (XEVNT_OK);

	if (tstamp < cinfo->first || tstamp > cinfo->last)
		return (XEVNT_PER);

	vp->sig = emalloc(sign_siglen);
	EVP_SignInit(&ctx, sign_digest);
	EVP_SignUpdate(&ctx, (u_char *)&vp->tstamp, 12);
	EVP_SignUpdate(&ctx, vp->ptr, len);
	if (EVP_SignFinal(&ctx, vp->sig, &len, sign_pkey))
		vp->siglen = htonl(len);
	return (XEVNT_OK);
}


/*
 * crypto_iff - verify Bob's response to Alice's challenge
 *
 * Returns
 * XEVNT_OK	success
 * XEVNT_PUB	bad or missing public key
 * XEVNT_ID	bad or missing group key
 * XEVNT_FSP	bad filestamp
 */
int
crypto_iff(
	struct exten *ep,	/* extension pointer */
	struct peer *peer	/* peer structure pointer */
	)
{
	DSA	*dsa;		/* IFF parameters */
	BN_CTX	*bctx;		/* BIGNUM context */
	DSA_SIG	*sdsa;		/* DSA parameters */
	BIGNUM	*bn, *bk;
	u_int	len;
	const u_char	*ptr;
	int	temp;

	/*
	 * If the IFF parameters are not valid or no challenge was sent,
	 * something awful happened or we are being tormented.
	 */
	if (peer->ident_pkey == NULL) {
		msyslog(LOG_INFO, "crypto_iff: scheme unavailable");
		return (XEVNT_ID);
	}
	if (ntohl(ep->fstamp) != peer->fstamp) {
		msyslog(LOG_INFO, "crypto_iff: invalid filestamp %u",
		    ntohl(ep->fstamp));
		return (XEVNT_FSP);
	}
	if ((dsa = peer->ident_pkey->pkey.dsa) == NULL) {
		msyslog(LOG_INFO, "crypto_iff: defective key");
		return (XEVNT_PUB);
	}
	if (peer->iffval == NULL) {
		msyslog(LOG_INFO, "crypto_iff: missing challenge");
		return (XEVNT_ID);
	}

	/*
	 * Extract the k + b r and g^k values from the response.
	 */
	bctx = BN_CTX_new(); bk = BN_new(); bn = BN_new();
	len = ntohl(ep->vallen);
	ptr = (const u_char *)ep->pkt;
	if ((sdsa = d2i_DSA_SIG(NULL, &ptr, len)) == NULL) {
		msyslog(LOG_ERR, "crypto_iff %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		return (XEVNT_ERR);
	}

	/*
	 * Compute g^(k + b r) g^(q - b)r mod p.
	 */
	BN_mod_exp(bn, dsa->pub_key, peer->iffval, dsa->p, bctx);
	BN_mod_exp(bk, dsa->g, sdsa->r, dsa->p, bctx);
	BN_mod_mul(bn, bn, bk, dsa->p, bctx);

	/*
	 * Verify the hash of the result matches hash(x).
	 */
	bighash(bn, bn);
	temp = BN_cmp(bn, sdsa->s);
	BN_free(bn); BN_free(bk); BN_CTX_free(bctx);
	BN_free(peer->iffval);
	peer->iffval = NULL;
	DSA_SIG_free(sdsa);
	if (temp == 0)
		return (XEVNT_OK);

	else
		return (XEVNT_ID);
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
 * the ntp-genkeys program generates the certificates used in the
 * protocol and the group key can be conveyed in a certificate extension
 * field. The scheme is self contained and independent of new
 * generations of host keys, sign keys and certificates.
 *
 * The GQ identity scheme is based on RSA cryptography and algorithms
 * described in Stinson p. 300 (with errors). The GQ values hide in a
 * RSA cuckoo structure, but only the modulus is used. The 512-bit
 * public modulus is n = p q, where p and q are secret large primes. The
 * TA rolls random group key b disguised as a RSA structure member.
 * Except for the public key, these values are shared only among group
 * members and never revealed in messages.
 *
 * When rolling new certificates, Bob recomputes the private and
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
 *
 * crypto_alice2 - construct Alice's challenge in GQ scheme
 *
 * Returns
 * XEVNT_OK	success
 * XEVNT_PUB	bad or missing public key
 * XEVNT_ID	bad or missing group key
 * XEVNT_PER	host certificate expired
 */
static int
crypto_alice2(
	struct peer *peer,	/* peer pointer */
	struct value *vp	/* value pointer */
	)
{
	RSA	*rsa;		/* GQ parameters */
	BN_CTX	*bctx;		/* BIGNUM context */
	EVP_MD_CTX ctx;		/* signature context */
	tstamp_t tstamp;
	u_int	len;

	/*
	 * The identity parameters must have correct format and content.
	 */
	if (peer->ident_pkey == NULL)
		return (XEVNT_ID);

	if ((rsa = peer->ident_pkey->pkey.rsa) == NULL) {
		msyslog(LOG_INFO, "crypto_alice2: defective key");
		return (XEVNT_PUB);
	}

	/*
	 * Roll new random r (0 < r < n). The OpenSSL library has a bug
	 * omitting BN_rand_range, so we have to do it the hard way.
	 */
	bctx = BN_CTX_new();
	len = BN_num_bytes(rsa->n);
	if (peer->iffval != NULL)
		BN_free(peer->iffval);
	peer->iffval = BN_new();
	BN_rand(peer->iffval, len * 8, -1, 1);	/* r mod n */
	BN_mod(peer->iffval, peer->iffval, rsa->n, bctx);
	BN_CTX_free(bctx);

	/*
	 * Sign and send to Bob. The filestamp is from the local file.
	 */
	tstamp = crypto_time();
	memset(vp, 0, sizeof(struct value));
	vp->tstamp = htonl(tstamp);
	vp->fstamp = htonl(peer->fstamp);
	vp->vallen = htonl(len);
	vp->ptr = emalloc(len);
	BN_bn2bin(peer->iffval, vp->ptr);
	vp->siglen = 0;
	if (tstamp == 0)
		return (XEVNT_OK);

	if (tstamp < cinfo->first || tstamp > cinfo->last)
		return (XEVNT_PER);

	vp->sig = emalloc(sign_siglen);
	EVP_SignInit(&ctx, sign_digest);
	EVP_SignUpdate(&ctx, (u_char *)&vp->tstamp, 12);
	EVP_SignUpdate(&ctx, vp->ptr, len);
	if (EVP_SignFinal(&ctx, vp->sig, &len, sign_pkey))
		vp->siglen = htonl(len);
	return (XEVNT_OK);
}


/*
 * crypto_bob2 - construct Bob's response to Alice's challenge
 *
 * Returns
 * XEVNT_OK	success
 * XEVNT_ID	bad or missing group key
 * XEVNT_ERR	protocol error
 * XEVNT_PER	host certificate expired
 */
static int
crypto_bob2(
	struct exten *ep,	/* extension pointer */
	struct value *vp	/* value pointer */
	)
{
	RSA	*rsa;		/* GQ parameters */
	DSA_SIG	*sdsa;		/* DSA parameters */
	BN_CTX	*bctx;		/* BIGNUM context */
	EVP_MD_CTX ctx;		/* signature context */
	tstamp_t tstamp;	/* NTP timestamp */
	BIGNUM	*r, *k, *g, *y;
	u_char	*ptr;
	u_int	len;

	/*
	 * If the GQ parameters are not valid, something awful
	 * happened or we are being tormented.
	 */
	if (gqpar_pkey == NULL) {
		msyslog(LOG_INFO, "crypto_bob2: scheme unavailable");
		return (XEVNT_ID);
	}
	rsa = gqpar_pkey->pkey.rsa;

	/*
	 * Extract r from the challenge.
	 */
	len = ntohl(ep->vallen);
	if ((r = BN_bin2bn((u_char *)ep->pkt, len, NULL)) == NULL) {
		msyslog(LOG_ERR, "crypto_bob2 %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		return (XEVNT_ERR);
	}

	/*
	 * Bob rolls random k (0 < k < n), computes y = k u^r mod n and
	 * x = k^b mod n, then sends (y, hash(x)) to Alice. 
	 */
	bctx = BN_CTX_new(); k = BN_new(); g = BN_new(); y = BN_new();
	sdsa = DSA_SIG_new();
	BN_rand(k, len * 8, -1, 1);		/* k */
	BN_mod(k, k, rsa->n, bctx);
	BN_mod_exp(y, rsa->p, r, rsa->n, bctx); /* u^r mod n */
	BN_mod_mul(y, k, y, rsa->n, bctx);	/* k u^r mod n */
	sdsa->r = BN_dup(y);
	BN_mod_exp(g, k, rsa->e, rsa->n, bctx); /* k^b mod n */
	bighash(g, g);
	sdsa->s = BN_dup(g);
	BN_CTX_free(bctx);
	BN_free(r); BN_free(k); BN_free(g); BN_free(y);
 
	/*
	 * Encode the values in ASN.1 and sign.
	 */
	tstamp = crypto_time();
	memset(vp, 0, sizeof(struct value));
	vp->tstamp = htonl(tstamp);
	vp->fstamp = htonl(gq_fstamp);
	len = i2d_DSA_SIG(sdsa, NULL);
	if (len <= 0) {
		msyslog(LOG_ERR, "crypto_bob2 %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		DSA_SIG_free(sdsa);
		return (XEVNT_ERR);
	}
	vp->vallen = htonl(len);
	ptr = emalloc(len);
	vp->ptr = ptr;
	i2d_DSA_SIG(sdsa, &ptr);
	DSA_SIG_free(sdsa);
	vp->siglen = 0;
	if (tstamp == 0)
		return (XEVNT_OK);

	if (tstamp < cinfo->first || tstamp > cinfo->last)
		return (XEVNT_PER);

	vp->sig = emalloc(sign_siglen);
	EVP_SignInit(&ctx, sign_digest);
	EVP_SignUpdate(&ctx, (u_char *)&vp->tstamp, 12);
	EVP_SignUpdate(&ctx, vp->ptr, len);
	if (EVP_SignFinal(&ctx, vp->sig, &len, sign_pkey))
		vp->siglen = htonl(len);
	return (XEVNT_OK);
}


/*
 * crypto_gq - verify Bob's response to Alice's challenge
 *
 * Returns
 * XEVNT_OK	success
 * XEVNT_PUB	bad or missing public key
 * XEVNT_ID	bad or missing group keys
 * XEVNT_ERR	protocol error
 * XEVNT_FSP	bad filestamp
 */
int
crypto_gq(
	struct exten *ep,	/* extension pointer */
	struct peer *peer	/* peer structure pointer */
	)
{
	RSA	*rsa;		/* GQ parameters */
	BN_CTX	*bctx;		/* BIGNUM context */
	DSA_SIG	*sdsa;		/* RSA signature context fake */
	BIGNUM	*y, *v;
	const u_char	*ptr;
	u_int	len;
	int	temp;

	/*
	 * If the GQ parameters are not valid or no challenge was sent,
	 * something awful happened or we are being tormented.
	 */
	if (peer->ident_pkey == NULL) {
		msyslog(LOG_INFO, "crypto_gq: scheme unavailable");
		return (XEVNT_ID);
	}
	if (ntohl(ep->fstamp) != peer->fstamp) {
		msyslog(LOG_INFO, "crypto_gq: invalid filestamp %u",
		    ntohl(ep->fstamp));
		return (XEVNT_FSP);
	}
	if ((rsa = peer->ident_pkey->pkey.rsa) == NULL) {
		msyslog(LOG_INFO, "crypto_gq: defective key");
		return (XEVNT_PUB);
	}
	if (peer->iffval == NULL) {
		msyslog(LOG_INFO, "crypto_gq: missing challenge");
		return (XEVNT_ID);
	}

	/*
	 * Extract the y = k u^r and hash(x = k^b) values from the
	 * response.
	 */
	bctx = BN_CTX_new(); y = BN_new(); v = BN_new();
	len = ntohl(ep->vallen);
	ptr = (const u_char *)ep->pkt;
	if ((sdsa = d2i_DSA_SIG(NULL, &ptr, len)) == NULL) {
		msyslog(LOG_ERR, "crypto_gq %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		return (XEVNT_ERR);
	}

	/*
	 * Compute v^r y^b mod n.
	 */
	BN_mod_exp(v, peer->grpkey, peer->iffval, rsa->n, bctx);
						/* v^r mod n */
	BN_mod_exp(y, sdsa->r, rsa->e, rsa->n, bctx); /* y^b mod n */
	BN_mod_mul(y, v, y, rsa->n, bctx);	/* v^r y^b mod n */

	/*
	 * Verify the hash of the result matches hash(x).
	 */
	bighash(y, y);
	temp = BN_cmp(y, sdsa->s);
	BN_CTX_free(bctx); BN_free(y); BN_free(v);
	BN_free(peer->iffval);
	peer->iffval = NULL;
	DSA_SIG_free(sdsa);
	if (temp == 0)
		return (XEVNT_OK);

	else
		return (XEVNT_ID);
}


/*
 ***********************************************************************
 *								       *
 * The following routines implement the Mu-Varadharajan (MV) identity  *
 * scheme                                                              *
 *								       *
 ***********************************************************************
 */
/*
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
 * Let q be the product of n distinct primes s'[j] (j = 1...n), where
 * each s'[j] has m significant bits. Let p be a prime p = 2 * q + 1, so
 * that q and each s'[j] divide p - 1 and p has M = n * m + 1
 * significant bits. The elements x mod q of Zq with the elements 2 and
 * the primes removed form a field Zq* valid for polynomial arithetic.
 * Let g be a generator of Zp; that is, gcd(g, p - 1) = 1 and g^q = 1
 * mod p. We expect M to be in the 500-bit range and n relatively small,
 * like 25, so the likelihood of a randomly generated element of x mod q
 * of Zq colliding with a factor of p - 1 is very small and can be
 * avoided. Associated with each s'[j] is an element s[j] such that s[j]
 * s'[j] = s'[j] mod q. We find s[j] as the quotient (q + s'[j]) /
 * s'[j]. These are the parameters of the scheme and they are expensive
 * to compute.
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
 * The distinguishing characteristic of this scheme is the capability to
 * revoke keys. Included in the calculation of E, gbar and ghat is the
 * product s = prod(s'[j]) (j = 1...n) above. If the factor s'[j] is
 * subsequently removed from the product and E, gbar and ghat
 * recomputed, the jth client will no longer be able to compute E^-1 and
 * thus unable to decrypt the block.
 *
 * How it works
 *
 * The scheme goes like this. Bob has the server values (p, A, q, gbar,
 * ghat) and Alice the client values (p, xbar, xhat).
 *
 * Alice rolls new random challenge r (0 < r < p) and sends to Bob in
 * the MV request message. Bob rolls new random k (0 < k < q), encrypts
 * y = A^k mod p (a permutation) and sends (hash(y), gbar^k, ghat^k) to
 * Alice.
 * 
 * Alice receives the response and computes the decryption key (the
 * inverse permutation) from previously obtained (xbar, xhat) and
 * (gbar^k, ghat^k) in the message. She computes the inverse, which is
 * unique by reasons explained in the ntp-keygen.c program sources. If
 * the hash of this result matches hash(y), Alice knows that Bob has the
 * group key b. The signed response binds this knowledge to Bob's
 * private key and the public key previously received in his
 * certificate.
 *
 * crypto_alice3 - construct Alice's challenge in MV scheme
 *
 * Returns
 * XEVNT_OK	success
 * XEVNT_PUB	bad or missing public key
 * XEVNT_ID	bad or missing group key
 * XEVNT_PER	host certificate expired
 */
static int
crypto_alice3(
	struct peer *peer,	/* peer pointer */
	struct value *vp	/* value pointer */
	)
{
	DSA	*dsa;		/* MV parameters */
	BN_CTX	*bctx;		/* BIGNUM context */
	EVP_MD_CTX ctx;		/* signature context */
	tstamp_t tstamp;
	u_int	len;

	/*
	 * The identity parameters must have correct format and content.
	 */
	if (peer->ident_pkey == NULL)
		return (XEVNT_ID);

	if ((dsa = peer->ident_pkey->pkey.dsa) == NULL) {
		msyslog(LOG_INFO, "crypto_alice3: defective key");
		return (XEVNT_PUB);
	}

	/*
	 * Roll new random r (0 < r < q). The OpenSSL library has a bug
	 * omitting BN_rand_range, so we have to do it the hard way.
	 */
	bctx = BN_CTX_new();
	len = BN_num_bytes(dsa->p);
	if (peer->iffval != NULL)
		BN_free(peer->iffval);
	peer->iffval = BN_new();
	BN_rand(peer->iffval, len * 8, -1, 1);	/* r */
	BN_mod(peer->iffval, peer->iffval, dsa->p, bctx);
	BN_CTX_free(bctx);

	/*
	 * Sign and send to Bob. The filestamp is from the local file.
	 */
	tstamp = crypto_time();
	memset(vp, 0, sizeof(struct value));
	vp->tstamp = htonl(tstamp);
	vp->fstamp = htonl(peer->fstamp);
	vp->vallen = htonl(len);
	vp->ptr = emalloc(len);
	BN_bn2bin(peer->iffval, vp->ptr);
	vp->siglen = 0;
	if (tstamp == 0)
		return (XEVNT_OK);

	if (tstamp < cinfo->first || tstamp > cinfo->last)
		return (XEVNT_PER);

	vp->sig = emalloc(sign_siglen);
	EVP_SignInit(&ctx, sign_digest);
	EVP_SignUpdate(&ctx, (u_char *)&vp->tstamp, 12);
	EVP_SignUpdate(&ctx, vp->ptr, len);
	if (EVP_SignFinal(&ctx, vp->sig, &len, sign_pkey))
		vp->siglen = htonl(len);
	return (XEVNT_OK);
}


/*
 * crypto_bob3 - construct Bob's response to Alice's challenge
 *
 * Returns
 * XEVNT_OK	success
 * XEVNT_ERR	protocol error
 * XEVNT_PER	host certificate expired
 */
static int
crypto_bob3(
	struct exten *ep,	/* extension pointer */
	struct value *vp	/* value pointer */
	)
{
	DSA	*dsa;		/* MV parameters */
	DSA	*sdsa;		/* DSA signature context fake */
	BN_CTX	*bctx;		/* BIGNUM context */
	EVP_MD_CTX ctx;		/* signature context */
	tstamp_t tstamp;	/* NTP timestamp */
	BIGNUM	*r, *k, *u;
	u_char	*ptr;
	u_int	len;

	/*
	 * If the MV parameters are not valid, something awful
	 * happened or we are being tormented.
	 */
	if (mvpar_pkey == NULL) {
		msyslog(LOG_INFO, "crypto_bob3: scheme unavailable");
		return (XEVNT_ID);
	}
	dsa = mvpar_pkey->pkey.dsa;

	/*
	 * Extract r from the challenge.
	 */
	len = ntohl(ep->vallen);
	if ((r = BN_bin2bn((u_char *)ep->pkt, len, NULL)) == NULL) {
		msyslog(LOG_ERR, "crypto_bob3 %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		return (XEVNT_ERR);
	}

	/*
	 * Bob rolls random k (0 < k < q), making sure it is not a
	 * factor of q. He then computes y = A^k r and sends (hash(y),
	 * gbar^k, ghat^k) to Alice.
	 */
	bctx = BN_CTX_new(); k = BN_new(); u = BN_new();
	sdsa = DSA_new();
	sdsa->p = BN_new(); sdsa->q = BN_new(); sdsa->g = BN_new();
	while (1) {
		BN_rand(k, BN_num_bits(dsa->q), 0, 0);
		BN_mod(k, k, dsa->q, bctx);
		BN_gcd(u, k, dsa->q, bctx);
		if (BN_is_one(u))
			break;
	}
	BN_mod_exp(u, dsa->g, k, dsa->p, bctx); /* A r */
	BN_mod_mul(u, u, r, dsa->p, bctx);
	bighash(u, sdsa->p);
	BN_mod_exp(sdsa->q, dsa->priv_key, k, dsa->p, bctx); /* gbar */
	BN_mod_exp(sdsa->g, dsa->pub_key, k, dsa->p, bctx); /* ghat */
	BN_CTX_free(bctx); BN_free(k); BN_free(r); BN_free(u);

	/*
	 * Encode the values in ASN.1 and sign.
	 */
	tstamp = crypto_time();
	memset(vp, 0, sizeof(struct value));
	vp->tstamp = htonl(tstamp);
	vp->fstamp = htonl(mv_fstamp);
	len = i2d_DSAparams(sdsa, NULL);
	if (len <= 0) {
		msyslog(LOG_ERR, "crypto_bob3 %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		DSA_free(sdsa);
		return (XEVNT_ERR);
	}
	vp->vallen = htonl(len);
	ptr = emalloc(len);
	vp->ptr = ptr;
	i2d_DSAparams(sdsa, &ptr);
	DSA_free(sdsa);
	vp->siglen = 0;
	if (tstamp == 0)
		return (XEVNT_OK);

	if (tstamp < cinfo->first || tstamp > cinfo->last)
		return (XEVNT_PER);

	vp->sig = emalloc(sign_siglen);
	EVP_SignInit(&ctx, sign_digest);
	EVP_SignUpdate(&ctx, (u_char *)&vp->tstamp, 12);
	EVP_SignUpdate(&ctx, vp->ptr, len);
	if (EVP_SignFinal(&ctx, vp->sig, &len, sign_pkey))
		vp->siglen = htonl(len);
	return (XEVNT_OK);
}


/*
 * crypto_mv - verify Bob's response to Alice's challenge
 *
 * Returns
 * XEVNT_OK	success
 * XEVNT_PUB	bad or missing public key
 * XEVNT_ID	bad or missing group key
 * XEVNT_ERR	protocol error
 * XEVNT_FSP	bad filestamp
 */
int
crypto_mv(
	struct exten *ep,	/* extension pointer */
	struct peer *peer	/* peer structure pointer */
	)
{
	DSA	*dsa;		/* MV parameters */
	DSA	*sdsa;		/* DSA parameters */
	BN_CTX	*bctx;		/* BIGNUM context */
	BIGNUM	*k, *u, *v;
	u_int	len;
	const u_char	*ptr;
	int	temp;

	/*
	 * If the MV parameters are not valid or no challenge was sent,
	 * something awful happened or we are being tormented.
	 */
	if (peer->ident_pkey == NULL) {
		msyslog(LOG_INFO, "crypto_mv: scheme unavailable");
		return (XEVNT_ID);
	}
	if (ntohl(ep->fstamp) != peer->fstamp) {
		msyslog(LOG_INFO, "crypto_mv: invalid filestamp %u",
		    ntohl(ep->fstamp));
		return (XEVNT_FSP);
	}
	if ((dsa = peer->ident_pkey->pkey.dsa) == NULL) {
		msyslog(LOG_INFO, "crypto_mv: defective key");
		return (XEVNT_PUB);
	}
	if (peer->iffval == NULL) {
		msyslog(LOG_INFO, "crypto_mv: missing challenge");
		return (XEVNT_ID);
	}

	/*
	 * Extract the (hash(y), gbar, ghat) values from the response.
	 */
	bctx = BN_CTX_new(); k = BN_new(); u = BN_new(); v = BN_new();
	len = ntohl(ep->vallen);
	ptr = (const u_char *)ep->pkt;
	if ((sdsa = d2i_DSAparams(NULL, &ptr, len)) == NULL) {
		msyslog(LOG_ERR, "crypto_mv %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		return (XEVNT_ERR);
	}

	/*
	 * Compute (gbar^xhat ghat^xbar)^-1 mod p.
	 */
	BN_mod_exp(u, sdsa->q, dsa->pub_key, dsa->p, bctx);
	BN_mod_exp(v, sdsa->g, dsa->priv_key, dsa->p, bctx);
	BN_mod_mul(u, u, v, dsa->p, bctx);
	BN_mod_inverse(u, u, dsa->p, bctx);
	BN_mod_mul(v, u, peer->iffval, dsa->p, bctx);

	/*
	 * The result should match the hash of r mod p.
	 */
	bighash(v, v);
	temp = BN_cmp(v, sdsa->p);
	BN_CTX_free(bctx); BN_free(k); BN_free(u); BN_free(v);
	BN_free(peer->iffval);
	peer->iffval = NULL;
	DSA_free(sdsa);
	if (temp == 0)
		return (XEVNT_OK);

	else
		return (XEVNT_ID);
}


/*
 ***********************************************************************
 *								       *
 * The following routines are used to manipulate certificates          *
 *								       *
 ***********************************************************************
 */
/*
 * cert_parse - parse x509 certificate and create info/value structures.
 *
 * The server certificate includes the version number, issuer name,
 * subject name, public key and valid date interval. If the issuer name
 * is the same as the subject name, the certificate is self signed and
 * valid only if the server is configured as trustable. If the names are
 * different, another issuer has signed the server certificate and
 * vouched for it. In this case the server certificate is valid if
 * verified by the issuer public key.
 *
 * Returns certificate info/value pointer if valid, NULL if not.
 */
struct cert_info *		/* certificate information structure */
cert_parse(
	u_char	*asn1cert,	/* X509 certificate */
	u_int	len,		/* certificate length */
	tstamp_t fstamp		/* filestamp */
	)
{
	X509	*cert;		/* X509 certificate */
	X509_EXTENSION *ext;	/* X509v3 extension */
	struct cert_info *ret;	/* certificate info/value */
	BIO	*bp;
	X509V3_EXT_METHOD *method;
	char	pathbuf[MAXFILENAME];
	u_char	*uptr;
	char	*ptr;
	int	temp, cnt, i;

	/*
	 * Decode ASN.1 objects and construct certificate structure.
	 */
	uptr = asn1cert;
	if ((cert = d2i_X509(NULL, &uptr, len)) == NULL) {
		msyslog(LOG_ERR, "cert_parse %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		return (NULL);
	}

	/*
	 * Extract version, subject name and public key.
	 */
	ret = emalloc(sizeof(struct cert_info));
	memset(ret, 0, sizeof(struct cert_info));
	if ((ret->pkey = X509_get_pubkey(cert)) == NULL) {
		msyslog(LOG_ERR, "cert_parse %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		cert_free(ret);
		X509_free(cert);
		return (NULL);
	}
	ret->version = X509_get_version(cert);
	X509_NAME_oneline(X509_get_subject_name(cert), pathbuf,
	    MAXFILENAME - 1);
	ptr = strstr(pathbuf, "CN=");
	if (ptr == NULL) {
		msyslog(LOG_INFO, "cert_parse: invalid subject %s",
		    pathbuf);
		cert_free(ret);
		X509_free(cert);
		return (NULL);
	}
	ret->subject = emalloc(strlen(ptr) + 1);
	strcpy(ret->subject, ptr + 3);

	/*
	 * Extract remaining objects. Note that the NTP serial number is
	 * the NTP seconds at the time of signing, but this might not be
	 * the case for other authority. We don't bother to check the
	 * objects at this time, since the real crunch can happen only
	 * when the time is valid but not yet certificated.
	 */
	ret->nid = OBJ_obj2nid(cert->cert_info->signature->algorithm);
	ret->digest = (const EVP_MD *)EVP_get_digestbynid(ret->nid);
	ret->serial =
	    (u_long)ASN1_INTEGER_get(X509_get_serialNumber(cert));
	X509_NAME_oneline(X509_get_issuer_name(cert), pathbuf,
	    MAXFILENAME);
	if ((ptr = strstr(pathbuf, "CN=")) == NULL) {
		msyslog(LOG_INFO, "cert_parse: invalid issuer %s",
		    pathbuf);
		cert_free(ret);
		X509_free(cert);
		return (NULL);
	}
	ret->issuer = emalloc(strlen(ptr) + 1);
	strcpy(ret->issuer, ptr + 3);
	ret->first = asn2ntp(X509_get_notBefore(cert));
	ret->last = asn2ntp(X509_get_notAfter(cert));

	/*
	 * Extract extension fields. These are ad hoc ripoffs of
	 * currently assigned functions and will certainly be changed
	 * before prime time.
	 */
	cnt = X509_get_ext_count(cert);
	for (i = 0; i < cnt; i++) {
		ext = X509_get_ext(cert, i);
		method = X509V3_EXT_get(ext);
		temp = OBJ_obj2nid(ext->object);
		switch (temp) {

		/*
		 * If a key_usage field is present, we decode whether
		 * this is a trusted or private certificate. This is
		 * dorky; all we want is to compare NIDs, but OpenSSL
		 * insists on BIO text strings.
		 */
		case NID_ext_key_usage:
			bp = BIO_new(BIO_s_mem());
			X509V3_EXT_print(bp, ext, 0, 0);
			BIO_gets(bp, pathbuf, MAXFILENAME);
			BIO_free(bp);
#if DEBUG
			if (debug)
				printf("cert_parse: %s: %s\n",
				    OBJ_nid2ln(temp), pathbuf);
#endif
			if (strcmp(pathbuf, "Trust Root") == 0)
				ret->flags |= CERT_TRUST;
			else if (strcmp(pathbuf, "Private") == 0)
				ret->flags |= CERT_PRIV;
			break;

		/*
		 * If a NID_subject_key_identifier field is present, it
		 * contains the GQ public key.
		 */
		case NID_subject_key_identifier:
			ret->grplen = ext->value->length - 2;
			ret->grpkey = emalloc(ret->grplen);
			memcpy(ret->grpkey, &ext->value->data[2],
			    ret->grplen);
			break;
		}
	}

	/*
	 * If certificate is self signed, verify signature.
	 */
	if (strcmp(ret->subject, ret->issuer) == 0) {
		if (!X509_verify(cert, ret->pkey)) {
			msyslog(LOG_INFO,
			    "cert_parse: signature not verified %s",
			    pathbuf);
			cert_free(ret);
			X509_free(cert);
			return (NULL);
		}
	}

	/*
	 * Verify certificate valid times. Note that certificates cannot
	 * be retroactive.
	 */
	if (ret->first > ret->last || ret->first < fstamp) {
		msyslog(LOG_INFO,
		    "cert_parse: invalid certificate %s first %u last %u fstamp %u",
		    ret->subject, ret->first, ret->last, fstamp);
		cert_free(ret);
		X509_free(cert);
		return (NULL);
	}

	/*
	 * Build the value structure to sign and send later.
	 */
	ret->cert.fstamp = htonl(fstamp);
	ret->cert.vallen = htonl(len);
	ret->cert.ptr = emalloc(len);
	memcpy(ret->cert.ptr, asn1cert, len);
#ifdef DEBUG
	if (debug > 1)
		X509_print_fp(stdout, cert);
#endif
	X509_free(cert);
	return (ret);
}


/*
 * cert_sign - sign x509 certificate equest and update value structure.
 *
 * The certificate request includes a copy of the host certificate,
 * which includes the version number, subject name and public key of the
 * host. The resulting certificate includes these values plus the
 * serial number, issuer name and valid interval of the server. The
 * valid interval extends from the current time to the same time one
 * year hence. This may extend the life of the signed certificate beyond
 * that of the signer certificate.
 *
 * It is convenient to use the NTP seconds of the current time as the
 * serial number. In the value structure the timestamp is the current
 * time and the filestamp is taken from the extension field. Note this
 * routine is called only when the client clock is synchronized to a
 * proventic source, so timestamp comparisons are valid.
 *
 * The host certificate is valid from the time it was generated for a
 * period of one year. A signed certificate is valid from the time of
 * signature for a period of one year, but only the host certificate (or
 * sign certificate if used) is actually used to encrypt and decrypt
 * signatures. The signature trail is built from the client via the
 * intermediate servers to the trusted server. Each signature on the
 * trail must be valid at the time of signature, but it could happen
 * that a signer certificate expire before the signed certificate, which
 * remains valid until its expiration. 
 *
 * Returns
 * XEVNT_OK	success
 * XEVNT_PUB	bad or missing public key
 * XEVNT_CRT	bad or missing certificate
 * XEVNT_VFY	certificate not verified
 * XEVNT_PER	host certificate expired
 */
static int
cert_sign(
	struct exten *ep,	/* extension field pointer */
	struct value *vp	/* value pointer */
	)
{
	X509	*req;		/* X509 certificate request */
	X509	*cert;		/* X509 certificate */
	X509_EXTENSION *ext;	/* certificate extension */
	ASN1_INTEGER *serial;	/* serial number */
	X509_NAME *subj;	/* distinguished (common) name */
	EVP_PKEY *pkey;		/* public key */
	EVP_MD_CTX ctx;		/* message digest context */
	tstamp_t tstamp;	/* NTP timestamp */
	u_int	len;
	u_char	*ptr;
	int	i, temp;

	/*
	 * Decode ASN.1 objects and construct certificate structure.
	 * Make sure the system clock is synchronized to a proventic
	 * source.
	 */
	tstamp = crypto_time();
	if (tstamp == 0)
		return (XEVNT_TSP);

	if (tstamp < cinfo->first || tstamp > cinfo->last)
		return (XEVNT_PER);

	ptr = (u_char *)ep->pkt;
	if ((req = d2i_X509(NULL, &ptr, ntohl(ep->vallen))) == NULL) {
		msyslog(LOG_ERR, "cert_sign %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		return (XEVNT_CRT);
	}
	/*
	 * Extract public key and check for errors.
	 */
	if ((pkey = X509_get_pubkey(req)) == NULL) {
		msyslog(LOG_ERR, "cert_sign %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		X509_free(req);
		return (XEVNT_PUB);
	}

	/*
	 * Generate X509 certificate signed by this server. For this
	 * purpose the issuer name is the server name. Also copy any
	 * extensions that might be present.
	 */
	cert = X509_new();
	X509_set_version(cert, X509_get_version(req));
	serial = ASN1_INTEGER_new();
	ASN1_INTEGER_set(serial, tstamp);
	X509_set_serialNumber(cert, serial);
	X509_gmtime_adj(X509_get_notBefore(cert), 0L);
	X509_gmtime_adj(X509_get_notAfter(cert), YEAR);
	subj = X509_get_issuer_name(cert);
	X509_NAME_add_entry_by_txt(subj, "commonName", MBSTRING_ASC,
	    (u_char *)sys_hostname, strlen(sys_hostname), -1, 0);
	subj = X509_get_subject_name(req);
	X509_set_subject_name(cert, subj);
	X509_set_pubkey(cert, pkey);
	ext = X509_get_ext(req, 0);
	temp = X509_get_ext_count(req);
	for (i = 0; i < temp; i++) {
		ext = X509_get_ext(req, i);
		X509_add_ext(cert, ext, -1);
	}
	X509_free(req);

	/*
	 * Sign and verify the certificate.
	 */
	X509_sign(cert, sign_pkey, sign_digest);
	if (!X509_verify(cert, sign_pkey)) {
		printf("cert_sign\n%s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		X509_free(cert);
		return (XEVNT_VFY);
	}
	len = i2d_X509(cert, NULL);

	/*
	 * Build and sign the value structure. We have to sign it here,
	 * since the response has to be returned right away. This is a
	 * clogging hazard.
	 */
	memset(vp, 0, sizeof(struct value));
	vp->tstamp = htonl(tstamp);
	vp->fstamp = ep->fstamp;
	vp->vallen = htonl(len);
	vp->ptr = emalloc(len);
	ptr = vp->ptr;
	i2d_X509(cert, &ptr);
	vp->siglen = 0;
	vp->sig = emalloc(sign_siglen);
	EVP_SignInit(&ctx, sign_digest);
	EVP_SignUpdate(&ctx, (u_char *)vp, 12);
	EVP_SignUpdate(&ctx, vp->ptr, len);
	if (EVP_SignFinal(&ctx, vp->sig, &len, sign_pkey))
		vp->siglen = htonl(len);
#ifdef DEBUG
	if (debug > 1)
		X509_print_fp(stdout, cert);
#endif
	X509_free(cert);
	return (XEVNT_OK);
}


/*
 * cert_valid - verify certificate with given public key
 *
 * This is pretty ugly, as the certificate has to be verified in the
 * OpenSSL X509 structure, not in the DER format in the info/value
 * structure.
 *
 * Returns
 * XEVNT_OK	success
 * XEVNT_VFY	certificate not verified
 */
int
cert_valid(
	struct cert_info *cinf,	/* certificate information structure */
	EVP_PKEY *pkey		/* public key */
	)
{
	X509	*cert;		/* X509 certificate */
	u_char	*ptr;

	if (cinf->flags & CERT_SIGN)
		return (XEVNT_OK);

	ptr = (u_char *)cinf->cert.ptr;
	cert = d2i_X509(NULL, &ptr, ntohl(cinf->cert.vallen));
	if (cert == NULL || !X509_verify(cert, pkey))
		return (XEVNT_VFY);

	X509_free(cert);
	return (XEVNT_OK);
}


/*
 * cert - install certificate in certificate list
 *
 * This routine encodes an extension field into a certificate info/value
 * structure. It searches the certificate list for duplicates and
 * expunges whichever is older. It then searches the list for other
 * certificates that might be verified by this latest one. Finally, it
 * inserts this certificate first on the list.
 *
 * Returns
 * XEVNT_OK	success
 * XEVNT_FSP	bad or missing filestamp
 * XEVNT_CRT	bad or missing certificate 
 */
int
cert_install(
	struct exten *ep,	/* cert info/value */
	struct peer *peer	/* peer structure */
	)
{
	struct cert_info *cp, *xp, *yp, **zp;

	/*
	 * Parse and validate the signed certificate. If valid,
	 * construct the info/value structure; otherwise, scamper home.
	 */
	if ((cp = cert_parse((u_char *)ep->pkt, ntohl(ep->vallen),
	    ntohl(ep->fstamp))) == NULL)
		return (XEVNT_CRT);

	/*
	 * Scan certificate list looking for another certificate with
	 * the same subject and issuer. If another is found with the
	 * same or older filestamp, unlink it and return the goodies to
	 * the heap. If another is found with a later filestamp, discard
	 * the new one and leave the building.
	 *
	 * Make a note to study this issue again. An earlier certificate
	 * with a long lifetime might be overtaken by a later
	 * certificate with a short lifetime, thus invalidating the
	 * earlier signature. However, we gotta find a way to leak old
	 * stuff from the cache, so we do it anyway. 
	 */
	yp = cp;
	zp = &cinfo;
	for (xp = cinfo; xp != NULL; xp = xp->link) {
		if (strcmp(cp->subject, xp->subject) == 0 &&
		    strcmp(cp->issuer, xp->issuer) == 0) {
			if (ntohl(cp->cert.fstamp) <=
			    ntohl(xp->cert.fstamp)) {
				*zp = xp->link;;
				cert_free(xp);
			} else {
				cert_free(cp);
				return (XEVNT_FSP);
			}
			break;
		}
		zp = &xp->link;
	}
	yp->link = cinfo;
	cinfo = yp;

	/*
	 * Scan the certificate list to see if Y is signed by X. This is
	 * independent of order.
	 */
	for (yp = cinfo; yp != NULL; yp = yp->link) {
		for (xp = cinfo; xp != NULL; xp = xp->link) {

			/*
			 * If the issuer of certificate Y matches the
			 * subject of certificate X, verify the
			 * signature of Y using the public key of X. If
			 * so, X signs Y.
			 */
			if (strcmp(yp->issuer, xp->subject) != 0 ||
				xp->flags & CERT_ERROR)
				continue;

			if (cert_valid(yp, xp->pkey) != XEVNT_OK) {
				yp->flags |= CERT_ERROR;
				continue;
			}

			/*
			 * The signature Y is valid only if it begins
			 * during the lifetime of X; however, it is not
			 * necessarily an error, since some other
			 * certificate might sign Y. 
			 */
			if (yp->first < xp->first || yp->first >
			    xp->last)
				continue;

			yp->flags |= CERT_SIGN;

			/*
			 * If X is trusted, then Y is trusted. Note that
			 * we might stumble over a self-signed
			 * certificate that is not trusted, at least
			 * temporarily. This can happen when a dude
			 * first comes up, but has not synchronized the
			 * clock and had its certificate signed by its
			 * server. In case of broken certificate trail,
			 * this might result in a loop that could
			 * persist until timeout.
			 */
			if (!(xp->flags & (CERT_TRUST | CERT_VALID)))
				continue;

			yp->flags |= CERT_VALID;

			/*
			 * If subject Y matches the server subject name,
			 * then Y has completed the certificate trail.
			 * Save the group key and light the valid bit.
			 */
			if (strcmp(yp->subject, peer->subject) != 0)
				continue;

			if (yp->grpkey != NULL) {
				if (peer->grpkey != NULL)
					BN_free(peer->grpkey);
				peer->grpkey = BN_bin2bn(yp->grpkey,
				     yp->grplen, NULL);
			}
			peer->crypto |= CRYPTO_FLAG_VALID;

			/*
			 * If the server has an an identity scheme,
			 * fetch the identity credentials. If not, the
			 * identity is verified only by the trusted
			 * certificate. The next signature will set the
			 * server proventic.
			 */
			if (peer->crypto & (CRYPTO_FLAG_GQ |
			    CRYPTO_FLAG_IFF | CRYPTO_FLAG_MV))
				continue;

			peer->crypto |= CRYPTO_FLAG_VRFY;
		}
	}

	/*
	 * That was awesome. Now update the timestamps and signatures.
	 */
	crypto_update();
	return (XEVNT_OK);
}


/*
 * cert_free - free certificate information structure
 */
void
cert_free(
	struct cert_info *cinf	/* certificate info/value structure */ 
	)
{
	if (cinf->pkey != NULL)
		EVP_PKEY_free(cinf->pkey);
	if (cinf->subject != NULL)
		free(cinf->subject);
	if (cinf->issuer != NULL)
		free(cinf->issuer);
	if (cinf->grpkey != NULL)
		free(cinf->grpkey);
	value_free(&cinf->cert);
	free(cinf);
}


/*
 ***********************************************************************
 *								       *
 * The following routines are used only at initialization time         *
 *								       *
 ***********************************************************************
 */
/*
 * crypto_key - load cryptographic parameters and keys from files
 *
 * This routine loads a PEM-encoded public/private key pair and extracts
 * the filestamp from the file name.
 *
 * Returns public key pointer if valid, NULL if not. Side effect updates
 * the filestamp if valid.
 */
static EVP_PKEY *
crypto_key(
	char	*cp,		/* file name */
	tstamp_t *fstamp	/* filestamp */
	)
{
	FILE	*str;		/* file handle */
	EVP_PKEY *pkey = NULL;	/* public/private key */
	char	filename[MAXFILENAME]; /* name of key file */
	char	linkname[MAXFILENAME]; /* filestamp buffer) */
	char	statstr[NTP_MAXSTRLEN]; /* statistics for filegen */
	char	*ptr;

	/*
	 * Open the key file. If the first character of the file name is
	 * not '/', prepend the keys directory string. If something goes
	 * wrong, abandon ship.
	 */
	if (*cp == '/')
		strcpy(filename, cp);
	else
		snprintf(filename, MAXFILENAME, "%s/%s", keysdir, cp);
	str = fopen(filename, "r");
	if (str == NULL)
		return (NULL);

	/*
	 * Read the filestamp, which is contained in the first line.
	 */
	if ((ptr = fgets(linkname, MAXFILENAME, str)) == NULL) {
		msyslog(LOG_ERR, "crypto_key: no data %s\n",
		    filename);
		(void)fclose(str);
		return (NULL);
	}
	if ((ptr = strrchr(ptr, '.')) == NULL) {
		msyslog(LOG_ERR, "crypto_key: no filestamp %s\n",
		    filename);
		(void)fclose(str);
		return (NULL);
	}
	if (sscanf(++ptr, "%u", fstamp) != 1) {
		msyslog(LOG_ERR, "crypto_key: invalid timestamp %s\n",
		    filename);
		(void)fclose(str);
		return (NULL);
	}

	/*
	 * Read and decrypt PEM-encoded private key.
	 */
	pkey = PEM_read_PrivateKey(str, NULL, NULL, passwd);
	fclose(str);
	if (pkey == NULL) {
		msyslog(LOG_ERR, "crypto_key %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		return (NULL);
	}

	/*
	 * Leave tracks in the cryptostats.
	 */
	if ((ptr = strrchr(linkname, '\n')) != NULL)
		*ptr = '\0'; 
	sprintf(statstr, "%s mod %d", &linkname[2],
	    EVP_PKEY_size(pkey) * 8);
	record_crypto_stats(NULL, statstr);
#ifdef DEBUG
	if (debug)
		printf("crypto_key: %s\n", statstr);
	if (debug > 1) {
		if (EVP_MD_type(pkey) == EVP_PKEY_DSA)
			DSA_print_fp(stdout, pkey->pkey.dsa, 0);
		else
			RSA_print_fp(stdout, pkey->pkey.rsa, 0);
	}
#endif
	return (pkey);
}


/*
 * crypto_cert - load certificate from file
 *
 * This routine loads a X.509 RSA or DSA certificate from a file and
 * constructs a info/cert value structure for this machine. The
 * structure includes a filestamp extracted from the file name. Later
 * the certificate can be sent to another machine by request.
 *
 * Returns certificate info/value pointer if valid, NULL if not.
 */
static struct cert_info *	/* certificate information */
crypto_cert(
	char	*cp		/* file name */
	)
{
	struct cert_info *ret; /* certificate information */
	FILE	*str;		/* file handle */
	char	filename[MAXFILENAME]; /* name of certificate file */
	char	linkname[MAXFILENAME]; /* filestamp buffer */
	char	statstr[NTP_MAXSTRLEN]; /* statistics for filegen */
	tstamp_t fstamp;	/* filestamp */
	long	len;
	char	*ptr;
	char	*name, *header;
	u_char	*data;

	/*
	 * Open the certificate file. If the first character of the file
	 * name is not '/', prepend the keys directory string. If
	 * something goes wrong, abandon ship.
	 */
	if (*cp == '/')
		strcpy(filename, cp);
	else
		snprintf(filename, MAXFILENAME, "%s/%s", keysdir, cp);
	str = fopen(filename, "r");
	if (str == NULL)
		return (NULL);

	/*
	 * Read the filestamp, which is contained in the first line.
	 */
	if ((ptr = fgets(linkname, MAXFILENAME, str)) == NULL) {
		msyslog(LOG_ERR, "crypto_cert: no data %s\n",
		    filename);
		(void)fclose(str);
		return (NULL);
	}
	if ((ptr = strrchr(ptr, '.')) == NULL) {
		msyslog(LOG_ERR, "crypto_cert: no filestamp %s\n",
		    filename);
		(void)fclose(str);
		return (NULL);
	}
	if (sscanf(++ptr, "%u", &fstamp) != 1) {
		msyslog(LOG_ERR, "crypto_cert: invalid filestamp %s\n",
		    filename);
		(void)fclose(str);
		return (NULL);
	}

	/*
	 * Read PEM-encoded certificate and install.
	 */
	if (!PEM_read(str, &name, &header, &data, &len)) {
		msyslog(LOG_ERR, "crypto_cert %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		(void)fclose(str);
		return (NULL);
	}
	free(header);
	if (strcmp(name, "CERTIFICATE") !=0) {
		msyslog(LOG_INFO, "crypto_cert: wrong PEM type %s",
		    name);
		free(name);
		free(data);
		(void)fclose(str);
		return (NULL);
	}
	free(name);

	/*
	 * Parse certificate and generate info/value structure.
	 */
	ret = cert_parse(data, len, fstamp);
	free(data);
	(void)fclose(str);
	if (ret == NULL)
		return (NULL);

	if ((ptr = strrchr(linkname, '\n')) != NULL)
		*ptr = '\0'; 
	sprintf(statstr, "%s 0x%x len %lu", &linkname[2], ret->flags,
	    len);
	record_crypto_stats(NULL, statstr);
#ifdef DEBUG
	if (debug)
		printf("crypto_cert: %s\n", statstr);
#endif
	return (ret);
}


/*
 * crypto_tai - load leapseconds table from file
 *
 * This routine loads the ERTS leapsecond file in NIST text format,
 * converts to a value structure and extracts a filestamp from the file
 * name. The data are used to establish the TAI offset from UTC, which
 * is provided to the kernel if supported. Later the data can be sent to
 * another machine on request.
 */
static void
crypto_tai(
	char	*cp		/* file name */
	)
{
	FILE	*str;		/* file handle */
	char	buf[NTP_MAXSTRLEN];	/* file line buffer */
	u_int32	leapsec[MAX_LEAP]; /* NTP time at leaps */
	int	offset;		/* offset at leap (s) */
	char	filename[MAXFILENAME]; /* name of leapseconds file */
	char	linkname[MAXFILENAME]; /* file link (for filestamp) */
	char	statstr[NTP_MAXSTRLEN]; /* statistics for filegen */
	tstamp_t fstamp;	/* filestamp */
	u_int	len;
	u_int32	*ptr;
	char	*dp;
	int	rval, i, j;

	/*
	 * Open the file and discard comment lines. If the first
	 * character of the file name is not '/', prepend the keys
	 * directory string. If the file is not found, not to worry; it
	 * can be retrieved over the net. But, if it is found with
	 * errors, we crash and burn.
	 */
	if (*cp == '/')
		strcpy(filename, cp);
	else
		snprintf(filename, MAXFILENAME, "%s/%s", keysdir, cp);
	if ((str = fopen(filename, "r")) == NULL)
		return;

	/*
	 * Extract filestamp if present.
	 */
	rval = readlink(filename, linkname, MAXFILENAME - 1);
	if (rval > 0) {
		linkname[rval] = '\0';
		dp = strrchr(linkname, '.');
	} else {
		dp = strrchr(filename, '.');
	}
	if (dp != NULL)
		sscanf(++dp, "%u", &fstamp);
	else
		fstamp = 0;
	tai_leap.fstamp = htonl(fstamp);

	/*
	 * We are rather paranoid here, since an intruder might cause a
	 * coredump by infiltrating naughty values. Empty lines and
	 * comments are ignored. Other lines must begin with two
	 * integers followed by junk or comments. The first integer is
	 * the NTP seconds of leap insertion, the second is the offset
	 * of TAI relative to UTC after that insertion. The second word
	 * must equal the initial insertion of ten seconds on 1 January
	 * 1972 plus one second for each succeeding insertion.
	 */
	i = 0;
	while (i < MAX_LEAP) {
		dp = fgets(buf, NTP_MAXSTRLEN - 1, str);
		if (dp == NULL)
			break;

		if (strlen(buf) < 1)
			continue;

		if (*buf == '#')
			continue;

		if (sscanf(buf, "%u %d", &leapsec[i], &offset) != 2)
			continue;

		if (i != offset - TAI_1972) 
			break;

		i++;
	}
	fclose(str);
	if (dp != NULL) {
		msyslog(LOG_INFO,
		    "crypto_tai: leapseconds file %s error %d", cp,
		    rval);
		exit (-1);
	}

	/*
	 * The extension field table entries consists of the NTP seconds
	 * of leap insertion in network byte order.
	 */
	len = i * sizeof(u_int32);
	tai_leap.vallen = htonl(len);
	ptr = emalloc(len);
	tai_leap.ptr = (u_char *)ptr;
	for (j = 0; j < i; j++)
		*ptr++ = htonl(leapsec[j]);
	crypto_flags |= CRYPTO_FLAG_TAI;
	sprintf(statstr, "%s fs %u leap %u len %u", cp, fstamp,
	   leapsec[--j], len);
	record_crypto_stats(NULL, statstr);
#ifdef DEBUG
	if (debug)
		printf("crypto_tai: %s\n", statstr);
#endif
}


/*
 * crypto_setup - load keys, certificate and leapseconds table
 *
 * This routine loads the public/private host key and certificate. If
 * available, it loads the public/private sign key, which defaults to
 * the host key, and leapseconds table. The host key must be RSA, but
 * the sign key can be either RSA or DSA. In either case, the public key
 * on the certificate must agree with the sign key.
 */
void
crypto_setup(void)
{
	EVP_PKEY *pkey;		/* private/public key pair */
	char	filename[MAXFILENAME]; /* file name buffer */
	l_fp	seed;		/* crypto PRNG seed as NTP timestamp */
	tstamp_t fstamp;	/* filestamp */
	tstamp_t sstamp;	/* sign filestamp */
	u_int	len, bytes;
	u_char	*ptr;

	/*
	 * Initialize structures.
	 */
	if (!crypto_flags)
		return;

	gethostname(filename, MAXFILENAME);
	bytes = strlen(filename) + 1;
	sys_hostname = emalloc(bytes);
	memcpy(sys_hostname, filename, bytes);
	if (passwd == NULL)
		passwd = sys_hostname;
	memset(&hostval, 0, sizeof(hostval));
	memset(&pubkey, 0, sizeof(pubkey));
	memset(&tai_leap, 0, sizeof(tai_leap));

	/*
	 * Load required random seed file and seed the random number
	 * generator. Be default, it is found in the user home
	 * directory. The root home directory may be / or /root,
	 * depending on the system. Wiggle the contents a bit and write
	 * it back so the sequence does not repeat when we next restart.
	 */
	ERR_load_crypto_strings();
	if (rand_file == NULL) {
		if ((RAND_file_name(filename, MAXFILENAME)) != NULL) {
			rand_file = emalloc(strlen(filename) + 1);
			strcpy(rand_file, filename);
		}
	} else if (*rand_file != '/') {
		snprintf(filename, MAXFILENAME, "%s/%s", keysdir,
		    rand_file);
		free(rand_file);
		rand_file = emalloc(strlen(filename) + 1);
		strcpy(rand_file, filename);
	}
	if (rand_file == NULL) {
		msyslog(LOG_ERR,
		    "crypto_setup: random seed file not specified");
		exit (-1);
	}
	if ((bytes = RAND_load_file(rand_file, -1)) == 0) {
		msyslog(LOG_ERR,
		    "crypto_setup: random seed file %s not found\n",
		    rand_file);
		exit (-1);
	}
	get_systime(&seed);
	RAND_seed(&seed, sizeof(l_fp));
	RAND_write_file(rand_file);
	OpenSSL_add_all_algorithms();
#ifdef DEBUG
	if (debug)
		printf(
		    "crypto_setup: OpenSSL version %lx random seed file %s bytes read %d\n",
		    SSLeay(), rand_file, bytes);
#endif

	/*
	 * Load required host key from file "ntpkey_host_<hostname>". It
	 * also becomes the default sign key.
	 */
	if (host_file == NULL) {
		snprintf(filename, MAXFILENAME, "ntpkey_host_%s",
		    sys_hostname);
		host_file = emalloc(strlen(filename) + 1);
		strcpy(host_file, filename);
	}
	pkey = crypto_key(host_file, &fstamp);
	if (pkey == NULL) {
		msyslog(LOG_ERR,
		    "crypto_setup: host key file %s not found or corrupt",
		    host_file);
		exit (-1);
	}
	host_pkey = pkey;
	sign_pkey = pkey;
	sstamp = fstamp;
	hostval.fstamp = htonl(fstamp);
	if (EVP_MD_type(host_pkey) != EVP_PKEY_RSA) {
		msyslog(LOG_ERR,
		    "crypto_setup: host key is not RSA key type");
		exit (-1);
	}
	hostval.vallen = htonl(strlen(sys_hostname));
	hostval.ptr = (u_char *)sys_hostname;
	
	/*
	 * Construct public key extension field for agreement scheme.
	 */
	len = i2d_PublicKey(host_pkey, NULL);
	ptr = emalloc(len);
	pubkey.ptr = ptr;
	i2d_PublicKey(host_pkey, &ptr);
	pubkey.vallen = htonl(len);
	pubkey.fstamp = hostval.fstamp;

	/*
	 * Load optional sign key from file "ntpkey_sign_<hostname>". If
	 * loaded, it becomes the sign key.
	 */
	if (sign_file == NULL) {
		snprintf(filename, MAXFILENAME, "ntpkey_sign_%s",
		    sys_hostname);
		sign_file = emalloc(strlen(filename) + 1);
		strcpy(sign_file, filename);
	}
	pkey = crypto_key(sign_file, &fstamp);
	if (pkey != NULL) {
		sign_pkey = pkey;
		sstamp = fstamp;
	}
	sign_siglen = EVP_PKEY_size(sign_pkey);

	/*
	 * Load optional IFF parameters from file
	 * "ntpkey_iff_<hostname>".
	 */
	if (iffpar_file == NULL) {
		snprintf(filename, MAXFILENAME, "ntpkey_iff_%s",
		    sys_hostname);
		iffpar_file = emalloc(strlen(filename) + 1);
		strcpy(iffpar_file, filename);
	}
	iffpar_pkey = crypto_key(iffpar_file, &if_fstamp);
	if (iffpar_pkey != NULL)
		crypto_flags |= CRYPTO_FLAG_IFF;

	/*
	 * Load optional GQ parameters from file "ntpkey_gq_<hostname>".
	 */
	if (gqpar_file == NULL) {
		snprintf(filename, MAXFILENAME, "ntpkey_gq_%s",
		    sys_hostname);
		gqpar_file = emalloc(strlen(filename) + 1);
		strcpy(gqpar_file, filename);
	}
	gqpar_pkey = crypto_key(gqpar_file, &gq_fstamp);
	if (gqpar_pkey != NULL)
		crypto_flags |= CRYPTO_FLAG_GQ;

	/*
	 * Load optional MV parameters from file "ntpkey_mv_<hostname>".
	 */
	if (mvpar_file == NULL) {
		snprintf(filename, MAXFILENAME, "ntpkey_mv_%s",
		    sys_hostname);
		mvpar_file = emalloc(strlen(filename) + 1);
		strcpy(mvpar_file, filename);
	}
	mvpar_pkey = crypto_key(mvpar_file, &mv_fstamp);
	if (mvpar_pkey != NULL)
		crypto_flags |= CRYPTO_FLAG_MV;

	/*
	 * Load required certificate from file "ntpkey_cert_<hostname>".
	 */
	if (cert_file == NULL) {
		snprintf(filename, MAXFILENAME, "ntpkey_cert_%s",
		    sys_hostname);
		cert_file = emalloc(strlen(filename) + 1);
		strcpy(cert_file, filename);
	}
	if ((cinfo = crypto_cert(cert_file)) == NULL) {
		msyslog(LOG_ERR,
		    "certificate file %s not found or corrupt",
		    cert_file);
		exit (-1);
	}

	/*
	 * The subject name must be the same as the host name, unless
	 * the certificate is private, in which case it may have come
	 * from another host.
	 */
	if (!(cinfo->flags & CERT_PRIV) && strcmp(cinfo->subject,
	    sys_hostname) != 0) {
		msyslog(LOG_ERR,
		    "crypto_setup: certificate %s not for this host",
		    cert_file);
		cert_free(cinfo);
		exit (-1);
	}

	/*
	 * It the certificate is trusted, the subject must be the same
	 * as the issuer, in other words it must be self signed.
	 */
	if (cinfo->flags & CERT_TRUST && strcmp(cinfo->subject,
	    cinfo->issuer) != 0) {
		if (cert_valid(cinfo, sign_pkey) != XEVNT_OK) {
			msyslog(LOG_ERR,
			    "crypto_setup: certificate %s is trusted, but not self signed.",
			    cert_file);
			cert_free(cinfo);
			exit (-1);
		}
	}
	sign_digest = cinfo->digest;
	if (cinfo->flags & CERT_PRIV)
		crypto_flags |= CRYPTO_FLAG_PRIV;
	crypto_flags |= cinfo->nid << 16;

	/*
	 * Load optional leapseconds table from file "ntpkey_leap". If
	 * the file is missing or defective, the values can later be
	 * retrieved from a server.
	 */
	if (leap_file == NULL)
		leap_file = "ntpkey_leap";
	crypto_tai(leap_file);
#ifdef DEBUG
	if (debug)
		printf(
		    "crypto_setup: flags 0x%x host %s signature %s\n",
		    crypto_flags, sys_hostname, OBJ_nid2ln(cinfo->nid));
#endif
}


/*
 * crypto_config - configure data from crypto configuration command.
 */
void
crypto_config(
	int	item,		/* configuration item */
	char	*cp		/* file name */
	)
{
	switch (item) {

	/*
	 * Set random seed file name.
	 */
	case CRYPTO_CONF_RAND:
		rand_file = emalloc(strlen(cp) + 1);
		strcpy(rand_file, cp);
		break;

	/*
	 * Set private key password.
	 */
	case CRYPTO_CONF_PW:
		passwd = emalloc(strlen(cp) + 1);
		strcpy(passwd, cp);
		break;

	/*
	 * Set host file name.
	 */
	case CRYPTO_CONF_PRIV:
		host_file = emalloc(strlen(cp) + 1);
		strcpy(host_file, cp);
		break;

	/*
	 * Set sign key file name.
	 */
	case CRYPTO_CONF_SIGN:
		sign_file = emalloc(strlen(cp) + 1);
		strcpy(sign_file, cp);
		break;

	/*
	 * Set iff parameters file name.
	 */
	case CRYPTO_CONF_IFFPAR:
		iffpar_file = emalloc(strlen(cp) + 1);
		strcpy(iffpar_file, cp);
		break;

	/*
	 * Set gq parameters file name.
	 */
	case CRYPTO_CONF_GQPAR:
		gqpar_file = emalloc(strlen(cp) + 1);
		strcpy(gqpar_file, cp);
		break;

	/*
	 * Set mv parameters file name.
	 */
	case CRYPTO_CONF_MVPAR:
		mvpar_file = emalloc(strlen(cp) + 1);
		strcpy(mvpar_file, cp);
		break;

	/*
	 * Set identity scheme.
	 */
	case CRYPTO_CONF_IDENT:
		if (!strcasecmp(cp, "iff"))
			ident_scheme |= CRYPTO_FLAG_IFF;
		else if (!strcasecmp(cp, "gq"))
			ident_scheme |= CRYPTO_FLAG_GQ;
		else if (!strcasecmp(cp, "mv"))
			ident_scheme |= CRYPTO_FLAG_MV;
		break;

	/*
	 * Set certificate file name.
	 */
	case CRYPTO_CONF_CERT:
		cert_file = emalloc(strlen(cp) + 1);
		strcpy(cert_file, cp);
		break;

	/*
	 * Set leapseconds file name.
	 */
	case CRYPTO_CONF_LEAP:
		leap_file = emalloc(strlen(cp) + 1);
		strcpy(leap_file, cp);
		break;
	}
	crypto_flags |= CRYPTO_FLAG_ENAB;
}
# else
int ntp_crypto_bs_pubkey;
# endif /* OPENSSL */
