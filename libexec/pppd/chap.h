/*
 * chap.h - Cryptographic Handshake Authentication Protocol definitions.
 *          based on November 1991 draft of PPP Authentication RFC
 *
 * Copyright (c) 1991 Gregory M. Christy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the author.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef __CHAP_INCLUDE__

/* Code + ID + length */
#define CHAP_HEADERLEN	(sizeof (u_char) + sizeof (u_char) + sizeof (u_short))

/*
 * CHAP codes.
 */

#define CHAP_DIGEST_MD5 5	/* use MD5 algorithm */

#define MD5_SIGNATURE_SIZE 16	/* 16 bytes in a MD5 message digest */

#define CHAP_NOCALLBACK 0	/* don't call back after successful auth */
#define CHAP_CALLBACK 1		/* do call back */

#define CHAP_CHALLENGE	1
#define CHAP_RESPONSE	2
#define CHAP_SUCCESS	3
#define CHAP_FAILURE    4

/*
 *  Challenge lengths
 */

#define MIN_CHALLENGE_LENGTH 64
#define MAX_CHALLENGE_LENGTH 128

#define MAX_SECRET_LEN 128
/*
 * Each interface is described by chap structure.
 */

typedef struct chap_state {
    int unit;		/* Interface unit number */
    u_char chal_str[MAX_CHALLENGE_LENGTH + 1]; /* challenge string */
    u_char chal_len;		/* challenge length */
    int clientstate;		/* Client state */
    int serverstate;		/* Server state */
    int flags;		/* Flags */
    unsigned char id;		/* Current id */
    int timeouttime;		/* Timeout time in milliseconds */
    int retransmits;		/* Number of retransmissions */
} chap_state;


/*
 * Client states.
 */
#define CHAPCS_CLOSED	1	/* Connection down */
#define CHAPCS_CHALLENGE_SENT	2	/* We've sent a challenge */
#define CHAPCS_OPEN	3	/* We've received an Ack */

/*
 * Server states.
 */
#define CHAPSS_CLOSED	1	/* Connection down */
#define CHAPSS_LISTEN	2	/* Listening for a challenge */
#define CHAPSS_OPEN	3	/* We've sent an Ack */

/*
 * Flags.
 */
#define CHAPF_LOWERUP	 0x01	/* The lower level is UP */
#define CHAPF_AWPPENDING 0x02	/* Auth with peer pending */
#define CHAPF_APPENDING	 0x04	/* Auth peer pending */
#define CHAPF_UPVALID	 0x08	/* values valid */
#define CHAPF_UPPENDING	 0x10	/* values pending */


/*
 * Timeouts.
 */
#define CHAP_DEFTIMEOUT	3	/* Timeout time in seconds */

extern chap_state chap[];

void ChapInit __ARGS((int));
void ChapAuthWithPeer __ARGS((int));
void ChapAuthPeer __ARGS((int));
void ChapLowerUp __ARGS((int));
void ChapLowerDown __ARGS((int));
void ChapInput __ARGS((int, u_char *, int));
void ChapProtocolReject __ARGS((int));

#define __CHAP_INCLUDE__
#endif /* __CHAP_INCLUDE__ */
