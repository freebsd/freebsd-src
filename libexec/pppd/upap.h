/*
 * upap.h - User/Password Authentication Protocol definitions.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * Packet header = Code, id, length.
 */
#define UPAP_HEADERLEN	(sizeof (u_char) + sizeof (u_char) + sizeof (u_short))


/*
 * UPAP codes.
 */
#define UPAP_AUTH	1	/* Authenticate */
#define UPAP_AUTHACK	2	/* Authenticate Ack */
#define UPAP_AUTHNAK	3	/* Authenticate Nak */


/*
 * Each interface is described by upap structure.
 */
typedef struct upap_state {
    int us_unit;		/* Interface unit number */
    char *us_user;		/* User */
    int us_userlen;		/* User length */
    char *us_passwd;		/* Password */
    int us_passwdlen;		/* Password length */
    int us_clientstate;		/* Client state */
    int us_serverstate;		/* Server state */
    int us_flags;		/* Flags */
    u_char us_id;		/* Current id */
    int us_timeouttime;		/* Timeout time in milliseconds */
    int us_retransmits;		/* Number of retransmissions */
} upap_state;


/*
 * Client states.
 */
#define UPAPCS_CLOSED	1	/* Connection down */
#define UPAPCS_AUTHSENT	2	/* We've sent an Authenticate */
#define UPAPCS_OPEN	3	/* We've received an Ack */

/*
 * Server states.
 */
#define UPAPSS_CLOSED	1	/* Connection down */
#define UPAPSS_LISTEN	2	/* Listening for an Authenticate */
#define UPAPSS_OPEN	3	/* We've sent an Ack */

/*
 * Flags.
 */
#define UPAPF_LOWERUP	1	/* The lower level is UP */
#define UPAPF_AWPPENDING 2	/* Auth with peer pending */
#define UPAPF_APPENDING	4	/* Auth peer pending */
#define UPAPF_UPVALID	8	/* User/passwd values valid */
#define UPAPF_UPPENDING	0x10	/* User/passwd values pending */


/*
 * Timeouts.
 */
#define UPAP_DEFTIMEOUT	3	/* Timeout time in seconds */


extern upap_state upap[];

void upap_init __ARGS((int));
void upap_authwithpeer __ARGS((int));
void upap_authpeer __ARGS((int));
void upap_lowerup __ARGS((int));
void upap_lowerdown __ARGS((int));
void upap_input __ARGS((int, u_char *, int));
void upap_protrej __ARGS((int));
