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
 *
 * $Id: $
 *
 */

#include <sys/types.h>

#include <des.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <syslog.h>

#include "mbuf.h"
#include "timer.h"
#include "chap.h"
#include "chap_ms.h"

/* unused, for documentation only */
/* only NTResp is filled in for FreeBSD */
typedef struct {
    u_char LANManResp[24];
    u_char NTResp[24];
    u_char UseNT;	/* If 1, ignore the LANMan response field */
} MS_ChapResponse;

static void DesEncrypt(u_char *, u_char *, u_char *);
static void MakeKey(u_char *, u_char *);

static void      /* IN 8 octets      IN 16 octets     OUT 24 octets */
ChallengeResponse(u_char *challenge, u_char *pwHash, u_char *response)
{
    char    ZPasswordHash[21];

    memset(ZPasswordHash, '\0', sizeof(ZPasswordHash));
    memcpy(ZPasswordHash, pwHash, 16);

    DesEncrypt(challenge, ZPasswordHash +  0, response + 0);
    DesEncrypt(challenge, ZPasswordHash +  7, response + 8);
    DesEncrypt(challenge, ZPasswordHash + 14, response + 16);
}

static void /* IN 8 octets IN 7 octest OUT 8 octets */
DesEncrypt(u_char *clear, u_char *key, u_char *cipher)
{
    des_cblock		des_key;
    des_key_schedule	key_schedule;

    MakeKey(key, des_key);
    des_set_key(&des_key, key_schedule);
    des_ecb_encrypt((des_cblock *)clear, (des_cblock *)cipher, key_schedule, 1);
}

static u_char Get7Bits(u_char *input, int startBit)
{
    register unsigned int	word;

    word  = (unsigned)input[startBit / 8] << 8;
    word |= (unsigned)input[startBit / 8 + 1];

    word >>= 15 - (startBit % 8 + 7);

    return word & 0xFE;
}

/* IN  56 bit DES key missing parity bits
   OUT 64 bit DES key with parity bits added */
static void MakeKey(u_char *key, u_char *des_key)
{
    des_key[0] = Get7Bits(key,  0);
    des_key[1] = Get7Bits(key,  7);
    des_key[2] = Get7Bits(key, 14);
    des_key[3] = Get7Bits(key, 21);
    des_key[4] = Get7Bits(key, 28);
    des_key[5] = Get7Bits(key, 35);
    des_key[6] = Get7Bits(key, 42);
    des_key[7] = Get7Bits(key, 49);

    des_set_odd_parity((des_cblock *)des_key);
}

/* passwordHash 16-bytes MD4 hashed password
   challenge    8-bytes peer CHAP challenge
   since passwordHash is in a 24-byte buffer, response is written in there */
void
ChapMS(char *passwordHash, char *challenge, int challenge_len)
{
    u_char response[24];

    ChallengeResponse(challenge, passwordHash, response);
    memcpy(passwordHash, response, 24);
    passwordHash += 24;
    *passwordHash = 1;
}
