/*
 * chap_ms.c - Microsoft MS-CHAP compatible implementation.
 *
 * Copyright (c) 1995 Eric Rosenquist, Strata Software Limited.
 * http://www.strataware.com/
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Eric Rosenquist.  The name of the author may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * Modifications by Lauri Pesonen / lpesonen@clinet.fi, april 1997
 *
 *   Implemented LANManager type password response to MS-CHAP challenges.
 *   Now pppd provides both NT style and LANMan style blocks, and the
 *   prefered is set by option "ms-lanman". Default is to use NT.
 *   The hash text (StdText) was taken from Win95 RASAPI32.DLL.
 *
 *   You should also use DOMAIN\\USERNAME as described in README.MSCHAP80
 */

#ifndef lint
static char rcsid[] = "$FreeBSD$";
#endif

#ifdef CHAPMS

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include <syslog.h>
#include <unistd.h>
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#include "pppd.h"
#include "chap.h"
#include "chap_ms.h"
#include "md4.h"

#ifndef USE_CRYPT
#include <openssl/des.h>
#endif

typedef struct {
    u_char LANManResp[24];
    u_char NTResp[24];
    u_char UseNT;		/* If 1, ignore the LANMan response field */
} MS_ChapResponse;
/* We use MS_CHAP_RESPONSE_LEN, rather than sizeof(MS_ChapResponse),
   in case this struct gets padded. */


static void	ChallengeResponse(u_char *, u_char *, u_char *);
static void	DesEncrypt(u_char *, u_char *, u_char *);
static void	MakeKey(u_char *, u_char *);
static u_char	Get7Bits(u_char *, int);
static void	ChapMS_NT(char *, int, char *, int, MS_ChapResponse *);
#ifdef MSLANMAN
static void	ChapMS_LANMan(char *, int, char *, int, MS_ChapResponse *);
#endif

#ifdef USE_CRYPT
static void	Expand(u_char *, u_char *);
static void	Collapse(u_char *, u_char *);
#endif

static void
ChallengeResponse(challenge, pwHash, response)
    u_char *challenge;	/* IN   8 octets */
    u_char *pwHash;	/* IN  16 octets */
    u_char *response;	/* OUT 24 octets */
{
    char    ZPasswordHash[21];

    BZERO(ZPasswordHash, sizeof(ZPasswordHash));
    BCOPY(pwHash, ZPasswordHash, MD4_SIGNATURE_SIZE);

#if 0
    log_packet(ZPasswordHash, sizeof(ZPasswordHash), "ChallengeResponse - ZPasswordHash", LOG_DEBUG);
#endif

    DesEncrypt(challenge, ZPasswordHash +  0, response + 0);
    DesEncrypt(challenge, ZPasswordHash +  7, response + 8);
    DesEncrypt(challenge, ZPasswordHash + 14, response + 16);

#if 0
    log_packet(response, 24, "ChallengeResponse - response", LOG_DEBUG);
#endif
}


#ifdef USE_CRYPT
static void
DesEncrypt(clear, key, cipher)
    u_char *clear;	/* IN  8 octets */
    u_char *key;	/* IN  7 octets */
    u_char *cipher;	/* OUT 8 octets */
{
    u_char des_key[8];
    u_char crypt_key[66];
    u_char des_input[66];

    MakeKey(key, des_key);

    Expand(des_key, crypt_key);
    setkey(crypt_key);

#if 0
    CHAPDEBUG((LOG_INFO, "DesEncrypt: 8 octet input : %02X%02X%02X%02X%02X%02X%02X%02X",
	       clear[0], clear[1], clear[2], clear[3], clear[4], clear[5], clear[6], clear[7]));
#endif

    Expand(clear, des_input);
    encrypt(des_input, 0);
    Collapse(des_input, cipher);

#if 0
    CHAPDEBUG((LOG_INFO, "DesEncrypt: 8 octet output: %02X%02X%02X%02X%02X%02X%02X%02X",
	       cipher[0], cipher[1], cipher[2], cipher[3], cipher[4], cipher[5], cipher[6], cipher[7]));
#endif
}

#else /* USE_CRYPT */

static void
DesEncrypt(clear, key, cipher)
    u_char *clear;	/* IN  8 octets */
    u_char *key;	/* IN  7 octets */
    u_char *cipher;	/* OUT 8 octets */
{
    des_cblock		des_key;
    des_key_schedule	key_schedule;

    MakeKey(key, des_key);

    des_set_key(&des_key, key_schedule);

#if 0
    CHAPDEBUG((LOG_INFO, "DesEncrypt: 8 octet input : %02X%02X%02X%02X%02X%02X%02X%02X",
	       clear[0], clear[1], clear[2], clear[3], clear[4], clear[5], clear[6], clear[7]));
#endif

    des_ecb_encrypt((des_cblock *)clear, (des_cblock *)cipher, key_schedule, 1);

#if 0
    CHAPDEBUG((LOG_INFO, "DesEncrypt: 8 octet output: %02X%02X%02X%02X%02X%02X%02X%02X",
	       cipher[0], cipher[1], cipher[2], cipher[3], cipher[4], cipher[5], cipher[6], cipher[7]));
#endif
}

#endif /* USE_CRYPT */


static u_char Get7Bits(input, startBit)
    u_char *input;
    int startBit;
{
    register unsigned int	word;

    word  = (unsigned)input[startBit / 8] << 8;
    word |= (unsigned)input[startBit / 8 + 1];

    word >>= 15 - (startBit % 8 + 7);

    return word & 0xFE;
}

#ifdef USE_CRYPT

/* in == 8-byte string (expanded version of the 56-bit key)
 * out == 64-byte string where each byte is either 1 or 0
 * Note that the low-order "bit" is always ignored by by setkey()
 */
static void Expand(in, out)
    u_char *in;
    u_char *out;
{
        int j, c;
        int i;

        for(i = 0; i < 64; in++){
		c = *in;
                for(j = 7; j >= 0; j--)
                        *out++ = (c >> j) & 01;
                i += 8;
        }
}

/* The inverse of Expand
 */
static void Collapse(in, out)
    u_char *in;
    u_char *out;
{
        int j;
        int i;
	unsigned int c;

	for (i = 0; i < 64; i += 8, out++) {
	    c = 0;
	    for (j = 7; j >= 0; j--, in++)
		c |= *in << j;
	    *out = c & 0xff;
	}
}
#endif

static void MakeKey(key, des_key)
    u_char *key;	/* IN  56 bit DES key missing parity bits */
    u_char *des_key;	/* OUT 64 bit DES key with parity bits added */
{
    des_key[0] = Get7Bits(key,  0);
    des_key[1] = Get7Bits(key,  7);
    des_key[2] = Get7Bits(key, 14);
    des_key[3] = Get7Bits(key, 21);
    des_key[4] = Get7Bits(key, 28);
    des_key[5] = Get7Bits(key, 35);
    des_key[6] = Get7Bits(key, 42);
    des_key[7] = Get7Bits(key, 49);

#ifndef USE_CRYPT
    des_set_odd_parity((des_cblock *)des_key);
#endif

#if 0
    CHAPDEBUG((LOG_INFO, "MakeKey: 56-bit input : %02X%02X%02X%02X%02X%02X%02X",
	       key[0], key[1], key[2], key[3], key[4], key[5], key[6]));
    CHAPDEBUG((LOG_INFO, "MakeKey: 64-bit output: %02X%02X%02X%02X%02X%02X%02X%02X",
	       des_key[0], des_key[1], des_key[2], des_key[3], des_key[4], des_key[5], des_key[6], des_key[7]));
#endif
}

static void
ChapMS_NT(rchallenge, rchallenge_len, secret, secret_len, response)
    char *rchallenge;
    int rchallenge_len;
    char *secret;
    int secret_len;
    MS_ChapResponse    *response;
{
    int			i;
    MD4_CTX		md4Context;
    u_char		hash[MD4_SIGNATURE_SIZE];
    u_char		unicodePassword[MAX_NT_PASSWORD * 2];

    /* Initialize the Unicode version of the secret (== password). */
    /* This implicitly supports 8-bit ISO8859/1 characters. */
    BZERO(unicodePassword, sizeof(unicodePassword));
    for (i = 0; i < secret_len; i++)
	unicodePassword[i * 2] = (u_char)secret[i];

    MD4Init(&md4Context);
    MD4Update(&md4Context, unicodePassword, secret_len * 2);	/* Unicode is 2 bytes/char */

    MD4Final(hash, &md4Context); 	/* Tell MD4 we're done */

    ChallengeResponse(rchallenge, hash, response->NTResp);
}

#ifdef MSLANMAN
static u_char *StdText = (u_char *)"KGS!@#$%"; /* key from rasapi32.dll */

static void
ChapMS_LANMan(rchallenge, rchallenge_len, secret, secret_len, response)
    char *rchallenge;
    int rchallenge_len;
    char *secret;
    int secret_len;
    MS_ChapResponse	*response;
{
    int			i;
    u_char		UcasePassword[MAX_NT_PASSWORD]; /* max is actually 14 */
    u_char		PasswordHash[MD4_SIGNATURE_SIZE];

    /* LANMan password is case insensitive */
    BZERO(UcasePassword, sizeof(UcasePassword));
    for (i = 0; i < secret_len; i++)
       UcasePassword[i] = (u_char)toupper(secret[i]);
    DesEncrypt( StdText, UcasePassword + 0, PasswordHash + 0 );
    DesEncrypt( StdText, UcasePassword + 7, PasswordHash + 8 );
    ChallengeResponse(rchallenge, PasswordHash, response->LANManResp);
}
#endif

void
ChapMS(cstate, rchallenge, rchallenge_len, secret, secret_len)
    chap_state *cstate;
    char *rchallenge;
    int rchallenge_len;
    char *secret;
    int secret_len;
{
    MS_ChapResponse	response;
#ifdef MSLANMAN
    extern int ms_lanman;
#endif

#if 0
    CHAPDEBUG((LOG_INFO, "ChapMS: secret is '%.*s'", secret_len, secret));
#endif
    BZERO(&response, sizeof(response));

    /* Calculate both always */
    ChapMS_NT(rchallenge, rchallenge_len, secret, secret_len, &response);

#ifdef MSLANMAN
    ChapMS_LANMan(rchallenge, rchallenge_len, secret, secret_len, &response);

    /* prefered method is set by option  */
    response.UseNT = !ms_lanman;
#else
    response.UseNT = 1;
#endif

    BCOPY(&response, cstate->response, MS_CHAP_RESPONSE_LEN);
    cstate->resp_length = MS_CHAP_RESPONSE_LEN;
}

#endif /* CHAPMS */
