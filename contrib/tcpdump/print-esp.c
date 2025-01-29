/*	$NetBSD: print-ah.c,v 1.4 1996/05/20 00:41:16 fvdl Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* \summary: IPSEC Encapsulating Security Payload (ESP) printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include <string.h>
#include <stdlib.h>

/* Any code in this file that depends on HAVE_LIBCRYPTO depends on
 * HAVE_OPENSSL_EVP_H too. Undefining the former when the latter isn't defined
 * is the simplest way of handling the dependency.
 */
#ifdef HAVE_LIBCRYPTO
#ifdef HAVE_OPENSSL_EVP_H
#include <openssl/evp.h>
#else
#undef HAVE_LIBCRYPTO
#endif
#endif

#include "netdissect.h"
#include "extract.h"

#include "diag-control.h"

#ifdef HAVE_LIBCRYPTO
#include "strtoaddr.h"
#include "ascii_strcasecmp.h"
#endif

#include "ip.h"
#include "ip6.h"

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * RFC1827/2406 Encapsulated Security Payload.
 */

struct newesp {
	nd_uint32_t	esp_spi;	/* ESP */
	nd_uint32_t	esp_seq;	/* Sequence number */
	/*variable size*/		/* (IV and) Payload data */
	/*variable size*/		/* padding */
	/*8bit*/			/* pad size */
	/*8bit*/			/* next header */
	/*8bit*/			/* next header */
	/*variable size, 32bit bound*/	/* Authentication data */
};

#ifdef HAVE_LIBCRYPTO
union inaddr_u {
	nd_ipv4 in4;
	nd_ipv6 in6;
};
struct sa_list {
	struct sa_list	*next;
	u_int		daddr_version;
	union inaddr_u	daddr;
	uint32_t	spi;          /* if == 0, then IKEv2 */
	int             initiator;
	u_char          spii[8];      /* for IKEv2 */
	u_char          spir[8];
	const EVP_CIPHER *evp;
	u_int		ivlen;
	int		authlen;
	u_char          authsecret[256];
	int             authsecret_len;
	u_char		secret[256];  /* is that big enough for all secrets? */
	int		secretlen;
};

#ifndef HAVE_EVP_CIPHER_CTX_NEW
/*
 * Allocate an EVP_CIPHER_CTX.
 * Used if we have an older version of OpenSSL that doesn't provide
 * routines to allocate and free them.
 */
static EVP_CIPHER_CTX *
EVP_CIPHER_CTX_new(void)
{
	EVP_CIPHER_CTX *ctx;

	ctx = malloc(sizeof(*ctx));
	if (ctx == NULL)
		return (NULL);
	memset(ctx, 0, sizeof(*ctx));
	return (ctx);
}

static void
EVP_CIPHER_CTX_free(EVP_CIPHER_CTX *ctx)
{
	EVP_CIPHER_CTX_cleanup(ctx);
	free(ctx);
}
#endif

#ifdef HAVE_EVP_DECRYPTINIT_EX
/*
 * Initialize the cipher by calling EVP_DecryptInit_ex(), because
 * calling EVP_DecryptInit() will reset the cipher context, clearing
 * the cipher, so calling it twice, with the second call having a
 * null cipher, will clear the already-set cipher.  EVP_DecryptInit_ex(),
 * however, won't reset the cipher context, so you can use it to specify
 * the IV in a second call after a first call to EVP_DecryptInit_ex()
 * to set the cipher and the key.
 *
 * XXX - is there some reason why we need to make two calls?
 */
static int
set_cipher_parameters(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
		      const unsigned char *key,
		      const unsigned char *iv)
{
	return EVP_DecryptInit_ex(ctx, cipher, NULL, key, iv);
}
#else
/*
 * Initialize the cipher by calling EVP_DecryptInit(), because we don't
 * have EVP_DecryptInit_ex(); we rely on it not trashing the context.
 */
static int
set_cipher_parameters(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
		      const unsigned char *key,
		      const unsigned char *iv)
{
	return EVP_DecryptInit(ctx, cipher, key, iv);
}
#endif

static u_char *
do_decrypt(netdissect_options *ndo, const char *caller, struct sa_list *sa,
    const u_char *iv, const u_char *ct, unsigned int ctlen)
{
	EVP_CIPHER_CTX *ctx;
	unsigned int block_size;
	unsigned int ptlen;
	u_char *pt;
	int len;

	ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL) {
		/*
		 * Failed to initialize the cipher context.
		 * From a look at the OpenSSL code, this appears to
		 * mean "couldn't allocate memory for the cipher context";
		 * note that we're not passing any parameters, so there's
		 * not much else it can mean.
		 */
		(*ndo->ndo_error)(ndo, S_ERR_ND_MEM_ALLOC,
		    "%s: can't allocate memory for cipher context", caller);
		return NULL;
	}

	if (set_cipher_parameters(ctx, sa->evp, sa->secret, NULL) < 0) {
		EVP_CIPHER_CTX_free(ctx);
		(*ndo->ndo_warning)(ndo, "%s: espkey init failed", caller);
		return NULL;
	}
	if (set_cipher_parameters(ctx, NULL, NULL, iv) < 0) {
		EVP_CIPHER_CTX_free(ctx);
		(*ndo->ndo_warning)(ndo, "%s: IV init failed", caller);
		return NULL;
	}

	/*
	 * At least as I read RFC 5996 section 3.14 and RFC 4303 section 2.4,
	 * if the cipher has a block size of which the ciphertext's size must
	 * be a multiple, the payload must be padded to make that happen, so
	 * the ciphertext length must be a multiple of the block size.  Fail
	 * if that's not the case.
	 */
	block_size = (unsigned int)EVP_CIPHER_CTX_block_size(ctx);
	if ((ctlen % block_size) != 0) {
		EVP_CIPHER_CTX_free(ctx);
		(*ndo->ndo_warning)(ndo,
		    "%s: ciphertext size %u is not a multiple of the cipher block size %u",
		    caller, ctlen, block_size);
		return NULL;
	}

	/*
	 * Attempt to allocate a buffer for the decrypted data, because
	 * we can't decrypt on top of the input buffer.
	 */
	ptlen = ctlen;
	pt = (u_char *)calloc(1, ptlen);
	if (pt == NULL) {
		EVP_CIPHER_CTX_free(ctx);
		(*ndo->ndo_error)(ndo, S_ERR_ND_MEM_ALLOC,
		    "%s: can't allocate memory for decryption buffer", caller);
		return NULL;
	}

	/*
	 * The size of the ciphertext handed to us is a multiple of the
	 * cipher block size, so we don't need to worry about padding.
	 */
	if (!EVP_CIPHER_CTX_set_padding(ctx, 0)) {
		free(pt);
		EVP_CIPHER_CTX_free(ctx);
		(*ndo->ndo_warning)(ndo,
		    "%s: EVP_CIPHER_CTX_set_padding failed", caller);
		return NULL;
	}
	if (!EVP_DecryptUpdate(ctx, pt, &len, ct, ctlen)) {
		free(pt);
		EVP_CIPHER_CTX_free(ctx);
		(*ndo->ndo_warning)(ndo, "%s: EVP_DecryptUpdate failed",
		    caller);
		return NULL;
	}
	EVP_CIPHER_CTX_free(ctx);
	return pt;
}

/*
 * This will allocate a new buffer containing the decrypted data.
 * It returns 1 on success and 0 on failure.
 *
 * It will push the new buffer and the values of ndo->ndo_packetp and
 * ndo->ndo_snapend onto the buffer stack, and change ndo->ndo_packetp
 * and ndo->ndo_snapend to refer to the new buffer.
 *
 * Our caller must pop the buffer off the stack when it's finished
 * dissecting anything in it and before it does any dissection of
 * anything in the old buffer.  That will free the new buffer.
 */
DIAG_OFF_DEPRECATION
int esp_decrypt_buffer_by_ikev2_print(netdissect_options *ndo,
				      int initiator,
				      const u_char spii[8],
				      const u_char spir[8],
				      const u_char *buf, const u_char *end)
{
	struct sa_list *sa;
	const u_char *iv;
	const u_char *ct;
	unsigned int ctlen;
	u_char *pt;

	/* initiator arg is any non-zero value */
	if(initiator) initiator=1;

	/* see if we can find the SA, and if so, decode it */
	for (sa = ndo->ndo_sa_list_head; sa != NULL; sa = sa->next) {
		if (sa->spi == 0
		    && initiator == sa->initiator
		    && memcmp(spii, sa->spii, 8) == 0
		    && memcmp(spir, sa->spir, 8) == 0)
			break;
	}

	if(sa == NULL) return 0;
	if(sa->evp == NULL) return 0;

	/*
	 * remove authenticator, and see if we still have something to
	 * work with
	 */
	end = end - sa->authlen;
	iv  = buf;
	ct = iv + sa->ivlen;
	ctlen = end-ct;

	if(end <= ct) return 0;

	pt = do_decrypt(ndo, __func__, sa, iv,
	    ct, ctlen);
	if (pt == NULL)
		return 0;

	/*
	 * Switch to the output buffer for dissection, and save it
	 * on the buffer stack so it can be freed; our caller must
	 * pop it when done.
	 */
	if (!nd_push_buffer(ndo, pt, pt, ctlen)) {
		free(pt);
		(*ndo->ndo_error)(ndo, S_ERR_ND_MEM_ALLOC,
			"%s: can't push buffer on buffer stack", __func__);
	}

	return 1;
}
DIAG_ON_DEPRECATION

static void esp_print_addsa(netdissect_options *ndo,
			    const struct sa_list *sa, int sa_def)
{
	/* copy the "sa" */

	struct sa_list *nsa;

	/* malloc() return used in a 'struct sa_list': do not free() */
	nsa = (struct sa_list *)malloc(sizeof(struct sa_list));
	if (nsa == NULL)
		(*ndo->ndo_error)(ndo, S_ERR_ND_MEM_ALLOC,
				  "%s: malloc", __func__);

	*nsa = *sa;

	if (sa_def)
		ndo->ndo_sa_default = nsa;

	nsa->next = ndo->ndo_sa_list_head;
	ndo->ndo_sa_list_head = nsa;
}


static u_int hexdigit(netdissect_options *ndo, char hex)
{
	if (hex >= '0' && hex <= '9')
		return (hex - '0');
	else if (hex >= 'A' && hex <= 'F')
		return (hex - 'A' + 10);
	else if (hex >= 'a' && hex <= 'f')
		return (hex - 'a' + 10);
	else {
		(*ndo->ndo_error)(ndo, S_ERR_ND_ESP_SECRET,
				  "invalid hex digit %c in espsecret\n", hex);
	}
}

static u_int hex2byte(netdissect_options *ndo, char *hexstring)
{
	u_int byte;

	byte = (hexdigit(ndo, hexstring[0]) << 4) + hexdigit(ndo, hexstring[1]);
	return byte;
}

/*
 * returns size of binary, 0 on failure.
 */
static int
espprint_decode_hex(netdissect_options *ndo,
		    u_char *binbuf, unsigned int binbuf_len, char *hex)
{
	unsigned int len;
	int i;

	len = strlen(hex) / 2;

	if (len > binbuf_len) {
		(*ndo->ndo_warning)(ndo, "secret is too big: %u\n", len);
		return 0;
	}

	i = 0;
	while (hex[0] != '\0' && hex[1]!='\0') {
		binbuf[i] = hex2byte(ndo, hex);
		hex += 2;
		i++;
	}

	return i;
}

/*
 * decode the form:    SPINUM@IP <tab> ALGONAME:0xsecret
 */

DIAG_OFF_DEPRECATION
static int
espprint_decode_encalgo(netdissect_options *ndo,
			char *decode, struct sa_list *sa)
{
	size_t i;
	const EVP_CIPHER *evp;
	int authlen = 0;
	char *colon, *p;

	colon = strchr(decode, ':');
	if (colon == NULL) {
		(*ndo->ndo_warning)(ndo, "failed to decode espsecret: %s\n", decode);
		return 0;
	}
	*colon = '\0';

	if (strlen(decode) > strlen("-hmac96") &&
	    !strcmp(decode + strlen(decode) - strlen("-hmac96"),
		    "-hmac96")) {
		p = strstr(decode, "-hmac96");
		*p = '\0';
		authlen = 12;
	}
	if (strlen(decode) > strlen("-cbc") &&
	    !strcmp(decode + strlen(decode) - strlen("-cbc"), "-cbc")) {
		p = strstr(decode, "-cbc");
		*p = '\0';
	}
	evp = EVP_get_cipherbyname(decode);

	if (!evp) {
		(*ndo->ndo_warning)(ndo, "failed to find cipher algo %s\n", decode);
		sa->evp = NULL;
		sa->authlen = 0;
		sa->ivlen = 0;
		return 0;
	}

	sa->evp = evp;
	sa->authlen = authlen;
	/* This returns an int, but it should never be negative */
	sa->ivlen = EVP_CIPHER_iv_length(evp);

	colon++;
	if (colon[0] == '0' && colon[1] == 'x') {
		/* decode some hex! */

		colon += 2;
		sa->secretlen = espprint_decode_hex(ndo, sa->secret, sizeof(sa->secret), colon);
		if(sa->secretlen == 0) return 0;
	} else {
		i = strlen(colon);

		if (i < sizeof(sa->secret)) {
			memcpy(sa->secret, colon, i);
			sa->secretlen = i;
		} else {
			memcpy(sa->secret, colon, sizeof(sa->secret));
			sa->secretlen = sizeof(sa->secret);
		}
	}

	return 1;
}
DIAG_ON_DEPRECATION

/*
 * for the moment, ignore the auth algorithm, just hard code the authenticator
 * length. Need to research how openssl looks up HMAC stuff.
 */
static int
espprint_decode_authalgo(netdissect_options *ndo,
			 char *decode, struct sa_list *sa)
{
	char *colon;

	colon = strchr(decode, ':');
	if (colon == NULL) {
		(*ndo->ndo_warning)(ndo, "failed to decode espsecret: %s\n", decode);
		return 0;
	}
	*colon = '\0';

	if(ascii_strcasecmp(decode,"sha1") == 0 ||
	   ascii_strcasecmp(decode,"md5") == 0) {
		sa->authlen = 12;
	}
	return 1;
}

static void esp_print_decode_ikeline(netdissect_options *ndo, char *line,
				     const char *file, int lineno)
{
	/* it's an IKEv2 secret, store it instead */
	struct sa_list sa1;

	char *init;
	char *icookie, *rcookie;
	int   ilen, rlen;
	char *authkey;
	char *enckey;

	init = strsep(&line, " \t");
	icookie = strsep(&line, " \t");
	rcookie = strsep(&line, " \t");
	authkey = strsep(&line, " \t");
	enckey  = strsep(&line, " \t");

	/* if any fields are missing */
	if(!init || !icookie || !rcookie || !authkey || !enckey) {
		(*ndo->ndo_warning)(ndo, "print_esp: failed to find all fields for ikev2 at %s:%u",
				    file, lineno);

		return;
	}

	ilen = strlen(icookie);
	rlen = strlen(rcookie);

	if((init[0]!='I' && init[0]!='R')
	   || icookie[0]!='0' || icookie[1]!='x'
	   || rcookie[0]!='0' || rcookie[1]!='x'
	   || ilen!=18
	   || rlen!=18) {
		(*ndo->ndo_warning)(ndo, "print_esp: line %s:%u improperly formatted.",
				    file, lineno);

		(*ndo->ndo_warning)(ndo, "init=%s icookie=%s(%u) rcookie=%s(%u)",
				    init, icookie, ilen, rcookie, rlen);

		return;
	}

	sa1.spi = 0;
	sa1.initiator = (init[0] == 'I');
	if(espprint_decode_hex(ndo, sa1.spii, sizeof(sa1.spii), icookie+2)!=8)
		return;

	if(espprint_decode_hex(ndo, sa1.spir, sizeof(sa1.spir), rcookie+2)!=8)
		return;

	if(!espprint_decode_encalgo(ndo, enckey, &sa1)) return;

	if(!espprint_decode_authalgo(ndo, authkey, &sa1)) return;

	esp_print_addsa(ndo, &sa1, FALSE);
}

/*
 *
 * special form: file /name
 * causes us to go read from this file instead.
 *
 */
static void esp_print_decode_onesecret(netdissect_options *ndo, char *line,
				       const char *file, int lineno)
{
	struct sa_list sa1;
	int sa_def;

	char *spikey;
	char *decode;

	spikey = strsep(&line, " \t");
	sa_def = 0;
	memset(&sa1, 0, sizeof(struct sa_list));

	/* if there is only one token, then it is an algo:key token */
	if (line == NULL) {
		decode = spikey;
		spikey = NULL;
		/* sa1.daddr.version = 0; */
		/* memset(&sa1.daddr, 0, sizeof(sa1.daddr)); */
		/* sa1.spi = 0; */
		sa_def    = 1;
	} else
		decode = line;

	if (spikey && ascii_strcasecmp(spikey, "file") == 0) {
		/* open file and read it */
		FILE *secretfile;
		char  fileline[1024];
		int   subfile_lineno=0;
		char  *nl;
		char *filename = line;

		secretfile = fopen(filename, FOPEN_READ_TXT);
		if (secretfile == NULL) {
			(*ndo->ndo_error)(ndo, S_ERR_ND_OPEN_FILE,
					  "%s: can't open %s: %s\n",
					  __func__, filename, strerror(errno));
		}

		while (fgets(fileline, sizeof(fileline)-1, secretfile) != NULL) {
			subfile_lineno++;
			/* remove newline from the line */
			nl = strchr(fileline, '\n');
			if (nl)
				*nl = '\0';
			if (fileline[0] == '#') continue;
			if (fileline[0] == '\0') continue;

			esp_print_decode_onesecret(ndo, fileline, filename, subfile_lineno);
		}
		fclose(secretfile);

		return;
	}

	if (spikey && ascii_strcasecmp(spikey, "ikev2") == 0) {
		esp_print_decode_ikeline(ndo, line, file, lineno);
		return;
	}

	if (spikey) {

		char *spistr, *foo;
		uint32_t spino;

		spistr = strsep(&spikey, "@");
		if (spistr == NULL) {
			(*ndo->ndo_warning)(ndo, "print_esp: failed to find the @ token");
			return;
		}

		spino = strtoul(spistr, &foo, 0);
		if (spistr == foo || !spikey) {
			(*ndo->ndo_warning)(ndo, "print_esp: failed to decode spi# %s\n", foo);
			return;
		}

		sa1.spi = spino;

		if (strtoaddr6(spikey, &sa1.daddr.in6) == 1) {
			sa1.daddr_version = 6;
		} else if (strtoaddr(spikey, &sa1.daddr.in4) == 1) {
			sa1.daddr_version = 4;
		} else {
			(*ndo->ndo_warning)(ndo, "print_esp: can not decode IP# %s\n", spikey);
			return;
		}
	}

	if (decode) {
		/* skip any blank spaces */
		while (*decode == ' ' || *decode == '\t' || *decode == '\r' || *decode == '\n')
			decode++;

		if(!espprint_decode_encalgo(ndo, decode, &sa1)) {
			return;
		}
	}

	esp_print_addsa(ndo, &sa1, sa_def);
}

DIAG_OFF_DEPRECATION
static void esp_init(netdissect_options *ndo _U_)
{
	/*
	 * 0.9.6 doesn't appear to define OPENSSL_API_COMPAT, so
	 * we check whether it's undefined or it's less than the
	 * value for 1.1.0.
	 */
#if !defined(OPENSSL_API_COMPAT) || OPENSSL_API_COMPAT < 0x10100000L
	OpenSSL_add_all_algorithms();
#endif
	EVP_add_cipher_alias(SN_des_ede3_cbc, "3des");
}
DIAG_ON_DEPRECATION

void esp_decodesecret_print(netdissect_options *ndo)
{
	char *line;
	char *p;
	static int initialized = 0;

	if (!initialized) {
		esp_init(ndo);
		initialized = 1;
	}

	p = ndo->ndo_espsecret;

	while (p && p[0] != '\0') {
		/* pick out the first line or first thing until a comma */
		if ((line = strsep(&p, "\n,")) == NULL) {
			line = p;
			p = NULL;
		}

		esp_print_decode_onesecret(ndo, line, "cmdline", 0);
	}

	ndo->ndo_espsecret = NULL;
}

#endif

#ifdef HAVE_LIBCRYPTO
#define USED_IF_LIBCRYPTO
#else
#define USED_IF_LIBCRYPTO _U_
#endif

#ifdef HAVE_LIBCRYPTO
DIAG_OFF_DEPRECATION
#endif
void
esp_print(netdissect_options *ndo,
	  const u_char *bp, u_int length,
	  const u_char *bp2 USED_IF_LIBCRYPTO,
	  u_int ver USED_IF_LIBCRYPTO,
	  int fragmented USED_IF_LIBCRYPTO,
	  u_int ttl_hl USED_IF_LIBCRYPTO)
{
	const struct newesp *esp;
	const u_char *ep;
#ifdef HAVE_LIBCRYPTO
	const struct ip *ip;
	struct sa_list *sa = NULL;
	const struct ip6_hdr *ip6 = NULL;
	const u_char *iv;
	u_int ivlen;
	u_int payloadlen;
	const u_char *ct;
	u_char *pt;
	u_int padlen;
	u_int nh;
#endif

	ndo->ndo_protocol = "esp";
	esp = (const struct newesp *)bp;

	/* 'ep' points to the end of available data. */
	ep = ndo->ndo_snapend;

	if ((const u_char *)(esp + 1) >= ep) {
		nd_print_trunc(ndo);
		return;
	}
	ND_PRINT("ESP(spi=0x%08x", GET_BE_U_4(esp->esp_spi));
	ND_PRINT(",seq=0x%x)", GET_BE_U_4(esp->esp_seq));
	ND_PRINT(", length %u", length);

#ifdef HAVE_LIBCRYPTO
	/* initialize SAs */
	if (ndo->ndo_sa_list_head == NULL) {
		if (!ndo->ndo_espsecret)
			return;

		esp_decodesecret_print(ndo);
	}

	if (ndo->ndo_sa_list_head == NULL)
		return;

	ip = (const struct ip *)bp2;
	switch (ver) {
	case 6:
		ip6 = (const struct ip6_hdr *)bp2;
		/* we do not attempt to decrypt jumbograms */
		if (!GET_BE_U_2(ip6->ip6_plen))
			return;
		/* XXX - check whether it's fragmented? */
		/* if we can't get nexthdr, we do not need to decrypt it */

		/* see if we can find the SA, and if so, decode it */
		for (sa = ndo->ndo_sa_list_head; sa != NULL; sa = sa->next) {
			if (sa->spi == GET_BE_U_4(esp->esp_spi) &&
			    sa->daddr_version == 6 &&
			    UNALIGNED_MEMCMP(&sa->daddr.in6, &ip6->ip6_dst,
				   sizeof(nd_ipv6)) == 0) {
				break;
			}
		}
		break;
	case 4:
		/* nexthdr & padding are in the last fragment */
		if (fragmented)
			return;

		/* see if we can find the SA, and if so, decode it */
		for (sa = ndo->ndo_sa_list_head; sa != NULL; sa = sa->next) {
			if (sa->spi == GET_BE_U_4(esp->esp_spi) &&
			    sa->daddr_version == 4 &&
			    UNALIGNED_MEMCMP(&sa->daddr.in4, &ip->ip_dst,
				   sizeof(nd_ipv4)) == 0) {
				break;
			}
		}
		break;
	default:
		return;
	}

	/* if we didn't find the specific one, then look for
	 * an unspecified one.
	 */
	if (sa == NULL)
		sa = ndo->ndo_sa_default;

	/* if not found fail */
	if (sa == NULL)
		return;

	/* pointer to the IV, if there is one */
	iv = (const u_char *)(esp + 1) + 0;
	/* length of the IV, if there is one; 0, if there isn't */
	ivlen = sa->ivlen;

	/*
	 * Get a pointer to the ciphertext.
	 *
	 * p points to the beginning of the payload, i.e. to the
	 * initialization vector, so if we skip past the initialization
	 * vector, it points to the beginning of the ciphertext.
	 */
	ct = iv + ivlen;

	/*
	 * Make sure the authentication data/integrity check value length
	 * isn't bigger than the total amount of data available after
	 * the ESP header and initialization vector is removed and,
	 * if not, slice the authentication data/ICV off.
	 */
	if (ep - ct < sa->authlen) {
		nd_print_trunc(ndo);
		return;
	}
	ep = ep - sa->authlen;

	/*
	 * Calculate the length of the ciphertext.  ep points to
	 * the beginning of the authentication data/integrity check
	 * value, i.e. right past the end of the ciphertext;
	 */
	payloadlen = ep - ct;

	if (sa->evp == NULL)
		return;

	/*
	 * If the next header value is past the end of the available
	 * data, we won't be able to fetch it once we've decrypted
	 * the ciphertext, so there's no point in decrypting the data.
	 *
	 * Report it as truncation.
	 */
	if (!ND_TTEST_1(ep - 1)) {
		nd_print_trunc(ndo);
		return;
	}

	pt = do_decrypt(ndo, __func__, sa, iv, ct, payloadlen);
	if (pt == NULL)
		return;

	/*
	 * Switch to the output buffer for dissection, and
	 * save it on the buffer stack so it can be freed.
	 */
	if (!nd_push_buffer(ndo, pt, pt, payloadlen)) {
		free(pt);
		(*ndo->ndo_error)(ndo, S_ERR_ND_MEM_ALLOC,
			"%s: can't push buffer on buffer stack", __func__);
	}

	/*
	 * Sanity check for pad length; if it, plus 2 for the pad
	 * length and next header fields, is bigger than the ciphertext
	 * length (which is also the plaintext length), it's too big.
	 *
	 * XXX - the check can fail if the packet is corrupt *or* if
	 * it was not decrypted with the correct key, so that the
	 * "plaintext" is not what was being sent.
	 */
	padlen = GET_U_1(pt + payloadlen - 2);
	if (padlen + 2 > payloadlen) {
		nd_print_trunc(ndo);
		return;
	}

	/* Get the next header */
	nh = GET_U_1(pt + payloadlen - 1);

	ND_PRINT(": ");

	/*
	 * Don't put padding + padding length(1 byte) + next header(1 byte)
	 * in the buffer because they are not part of the plaintext to decode.
	 */
	if (!nd_push_snaplen(ndo, pt, payloadlen - (padlen + 2))) {
		(*ndo->ndo_error)(ndo, S_ERR_ND_MEM_ALLOC,
			"%s: can't push snaplen on buffer stack", __func__);
	}

	/* Now dissect the plaintext. */
	ip_demux_print(ndo, pt, payloadlen - (padlen + 2), ver, fragmented,
		       ttl_hl, nh, bp2);

	/* Pop the buffer, freeing it. */
	nd_pop_packet_info(ndo);
	/* Pop the nd_push_snaplen */
	nd_pop_packet_info(ndo);
#endif
}
#ifdef HAVE_LIBCRYPTO
DIAG_ON_DEPRECATION
#endif
