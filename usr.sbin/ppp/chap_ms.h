/*
 * chap.h - Cryptographic Handshake Authentication Protocol definitions.
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
 * $FreeBSD$
 */

/* Max # of (Unicode) chars in an NT password */
#define MAX_NT_PASSWORD	256

/* Don't rely on sizeof(MS_ChapResponse) in case of struct padding */  
#define MS_CHAP_RESPONSE_LEN    49
#define CHAP81_RESPONSE_LEN     49
#define CHAP81_NTRESPONSE_LEN   24
#define CHAP81_NTRESPONSE_OFF   24
#define CHAP81_HASH_LEN         16
#define CHAP81_AUTHRESPONSE_LEN	42
#define CHAP81_CHALLENGE_LEN    16

extern void mschap_NT(char *, char *);
extern void mschap_LANMan(char *, char *, char *);
extern void GenerateNTResponse(char *, char *, char *, int, char *, int , char *);
extern void HashNtPasswordHash(char *, char *);
extern void NtPasswordHash(char *, int, char *);
extern void ChallengeHash(char *, char *, char *UserName, int, char *);
extern void GenerateAuthenticatorResponse(char *, int, char *, char *, char *, char *, int, char *);
extern void GetAsymetricStartKey(char *, char *, int, int, int);
extern void GetMasterKey(char *, char *, char *);
extern void GetNewKeyFromSHA(char *, char *, long, char *);
