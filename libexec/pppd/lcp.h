/*
 * lcp.h - Link Control Protocol definitions.
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
 * Options.
 */
#define CI_MRU		1	/* Maximum Receive Unit */
#define CI_ASYNCMAP	2	/* Async Control Character Map */
#define CI_AUTHTYPE	3	/* Authentication Type */
#define CI_NOTDEFINED	4	/* not defined (used to be Encryption Type) */
#define CI_MAGICNUMBER	5	/* Magic Number */
#define CI_KEEPALIVE	6	/* Keep Alive Parameters */
#define CI_PCOMPRESSION	7	/* Protocol Field Compression */
#define CI_ACCOMPRESSION 8	/* Address/Control Field Compression */


/*
 * The state of options is described by an lcp_options structure.
 */
typedef struct lcp_options {
    int passive : 1;		/* Passives vs. active open */
    int restart : 1;		/* Restart vs. exit after close */
    int neg_mru : 1;		/* Negotiate the MRU? */
    u_short mru;		/* Value of MRU */
    int neg_asyncmap : 1;	/* Async map? */
    u_long asyncmap;
    int neg_upap : 1;		/* UPAP authentication? */
    int neg_chap : 1;		/* CHAP authentication? */
    char chap_mdtype;		/* which MD type */
    char chap_callback;		/* callback ? */
    int neg_magicnumber : 1;	/* Magic number? */
    u_long magicnumber;
    int numloops;		/* Number loops during magic number negot. */
    int neg_pcompression : 1;	/* HDLC Protocol Field Compression? */
    int neg_accompression : 1;	/* HDLC Address/Control Field Compression? */
} lcp_options;

extern fsm lcp_fsm[];
extern lcp_options lcp_wantoptions[];
extern lcp_options lcp_gotoptions[];
extern lcp_options lcp_allowoptions[];
extern lcp_options lcp_hisoptions[];

#define DEFMRU	1500		/* Try for this */
#define MINMRU	128		/* No MRUs below this */

void lcp_init __ARGS((int));
void lcp_activeopen __ARGS((int));
void lcp_passiveopen __ARGS((int));
void lcp_close __ARGS((int));
void lcp_lowerup __ARGS((int));
void lcp_lowerdown __ARGS((int));
void lcp_input __ARGS((int, u_char *, int));
void lcp_protrej __ARGS((int));
void lcp_sprotrej __ARGS((int, u_char *, int));
