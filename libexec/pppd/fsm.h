/*
 * fsm.h - {Link, IP} Control Protocol Finite State Machine definitions.
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
#define HEADERLEN	(sizeof (u_char) + sizeof (u_char) + sizeof (u_short))


/*
 *  CP (LCP, IPCP, etc.) codes.
 */
#define CONFREQ		1	/* Configuration Request */
#define CONFACK		2	/* Configuration Ack */
#define CONFNAK		3	/* Configuration Nak */
#define CONFREJ		4	/* Configuration Reject */
#define TERMREQ		5	/* Termination Request */
#define TERMACK		6	/* Termination Ack */
#define CODEREJ		7	/* Code Reject */
#define PROTREJ		8	/* Protocol Reject */
#define ECHOREQ		9	/* Echo Request */
#define ECHOREP		10	/* Echo Reply */
#define DISCREQ		11	/* Discard Request */
#define KEEPALIVE	12	/* Keepalive */


/*
 * Each FSM is described by a fsm_callbacks and a fsm structure.
 */
typedef struct fsm_callbacks {
    void (*resetci)();		/* Reset our Configuration Information */
    int (*cilen)();		/* Length of our Configuration Information */
    void (*addci)();		/* Add our Configuration Information */
    int (*ackci)();		/* ACK our Configuration Information */
    void (*nakci)();		/* NAK our Configuration Information */
    void (*rejci)();		/* Reject our Configuration Information */
    u_char (*reqci)();		/* Request peer's Configuration Information */
    void (*up)();		/* Called when fsm reaches OPEN state */
    void (*down)();		/* Called when fsm leaves OPEN state */
    void (*closed)();		/* Called when fsm reaches CLOSED state */
    void (*protreject)();	/* Called when Protocol-Reject received */
    void (*retransmit)();	/* Retransmission is necessary */
} fsm_callbacks;


typedef struct fsm {
    int unit;			/* Interface unit number */
    u_short protocol;		/* Data Link Layer Protocol field value */
    int state;			/* State */
    int flags;			/* Flags */
    u_char id;			/* Current id */
    u_char reqid;		/* Current request id */
    int timeouttime;		/* Timeout time in milliseconds */
    int maxconfreqtransmits;	/* Maximum Configure-Request transmissions */
    int retransmits;		/* Number of retransmissions */
    int maxtermtransmits;	/* Maximum Terminate-Request transmissions */
    int nakloops;		/* Number of nak loops since last timeout */
    int maxnakloops;		/* Maximum number of nak loops tolerated */
    fsm_callbacks *callbacks;	/* Callback routines */
} fsm;


/*
 * Link states.
 */
#define CLOSED		1	/* Connection closed */
#define LISTEN		2	/* Listening for a Config Request */
#define REQSENT		3	/* We've sent a Config Request */
#define ACKSENT		4	/* We've sent a Config Ack */
#define ACKRCVD		5	/* We've received a Config Ack */
#define OPEN		6	/* Connection open */
#define TERMSENT	7	/* We've sent a Terminate Request */


/*
 * Flags.
 */
#define LOWERUP		1	/* The lower level is UP */
#define AOPENDING	2	/* Active Open pending timeout of request */
#define POPENDING	4	/* Passive Open pending timeout of request */


/*
 * Timeouts.
 */
#define DEFTIMEOUT	3	/* Timeout time in seconds */
#define DEFMAXTERMTRANSMITS 10	/* Maximum Terminate-Request transmissions */
#define DEFMAXCONFIGREQS 10	/* Maximum Configure-Request transmissions */


#define DEFMAXNAKLOOPS	10	/* Maximum number of nak loops */


void fsm_init __ARGS((fsm *));
void fsm_activeopen __ARGS((fsm *));
void fsm_passiveopen __ARGS((fsm *));
void fsm_close __ARGS((fsm *));
void fsm_lowerup __ARGS((fsm *));
void fsm_lowerdown __ARGS((fsm *));
void fsm_protreject __ARGS((fsm *));
void fsm_input __ARGS((fsm *, u_char *, int));
void fsm_sdata __ARGS((fsm *, int, int, u_char *, int));
