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
 *
 * $Id: lcp.h,v 1.8 1995/06/12 11:22:47 paulus Exp $
 */

/*
 * Options.
 */
#define CI_MRU		1	/* Maximum Receive Unit */
#define CI_ASYNCMAP	2	/* Async Control Character Map */
#define CI_AUTHTYPE	3	/* Authentication Type */
#define CI_QUALITY	4	/* Quality Protocol */
#define CI_MAGICNUMBER	5	/* Magic Number */
#define CI_PCOMPRESSION	7	/* Protocol Field Compression */
#define CI_ACCOMPRESSION 8	/* Address/Control Field Compression */

/*
 * LCP-specific packet types.
 */
#define PROTREJ		8	/* Protocol Reject */
#define ECHOREQ		9	/* Echo Request */
#define ECHOREP		10	/* Echo Reply */
#define DISCREQ		11	/* Discard Request */

/*
 * The state of options is described by an lcp_options structure.
 */
typedef struct lcp_options {
    int passive : 1;		/* Don't die if we don't get a response */
    int silent : 1;		/* Wait for the other end to start first */
    int restart : 1;		/* Restart vs. exit after close */
    int neg_mru : 1;		/* Negotiate the MRU? */
    int neg_asyncmap : 1;	/* Negotiate the async map? */
    int neg_upap : 1;		/* Ask for UPAP authentication? */
    int neg_chap : 1;		/* Ask for CHAP authentication? */
    int neg_magicnumber : 1;	/* Ask for magic number? */
    int neg_pcompression : 1;	/* HDLC Protocol Field Compression? */
    int neg_accompression : 1;	/* HDLC Address/Control Field Compression? */
    int neg_lqr : 1;		/* Negotiate use of Link Quality Reports */
    u_short mru;		/* Value of MRU */
    u_char chap_mdtype;		/* which MD type (hashing algorithm) */
    u_int32_t asyncmap;		/* Value of async map */
    u_int32_t magicnumber;
    int numloops;		/* Number of loops during magic number neg. */
    u_int32_t lqr_period;	/* Reporting period for LQR 1/100ths second */
} lcp_options;

extern fsm lcp_fsm[];
extern lcp_options lcp_wantoptions[];
extern lcp_options lcp_gotoptions[];
extern lcp_options lcp_allowoptions[];
extern lcp_options lcp_hisoptions[];
extern u_int32_t xmit_accm[][8];

#define DEFMRU	1500		/* Try for this */
#define MINMRU	128		/* No MRUs below this */
#define MAXMRU	16384		/* Normally limit MRU to this */

void lcp_init __P((int));
void lcp_open __P((int));
void lcp_close __P((int));
void lcp_lowerup __P((int));
void lcp_lowerdown __P((int));
void lcp_input __P((int, u_char *, int));
void lcp_protrej __P((int));
void lcp_sprotrej __P((int, u_char *, int));
int  lcp_printpkt __P((u_char *, int,
		       void (*) __P((void *, char *, ...)), void *));

/* Default number of times we receive our magic number from the peer
   before deciding the link is looped-back. */
#define DEFLOOPBACKFAIL	5
