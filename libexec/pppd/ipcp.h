/*
 * ipcp.h - IP Control Protocol definitions.
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
#define CI_ADDRS	1	/* IP Addresses */
#define CI_COMPRESSTYPE	2	/* Compression Type */
#define	CI_ADDR		3

#define MAX_STATES 16		/* from slcompress.h */

#define IPCP_VJMODE_OLD 1		/* "old" mode (option # = 0x0037) */
#define IPCP_VJMODE_RFC1172 2		/* "old-rfc"mode (option # = 0x002d) */
#define IPCP_VJMODE_RFC1332 3		/* "new-rfc"mode (option # = 0x002d, */
                                        /*  maxslot and slot number */
				        /*  compression from Aug. 1991 */
					/*  ipcp draft RFC) */

#define IPCP_VJ_COMP 0x002d	/* current value for VJ compression option*/
#define IPCP_VJ_COMP_OLD 0x0037	/* "old" (i.e, broken) value for VJ */
				/* compression option*/ 

typedef struct ipcp_options {
    int neg_addrs : 1;		/* Negotiate IP Addresses? */
    int neg_addr : 1;		/* Negotiate IP Address? */
    int got_addr : 1;		/* Got IP Address? */
    u_long ouraddr, hisaddr;	/* Addresses in NETWORK BYTE ORDER */
    int neg_vj : 1;		/* Van Jacobson Compression? */
    u_char maxslotindex, cflag;	/* fields for Aug. 1991 Draft VJ */
				/* compression negotiation */
} ipcp_options;

extern ipcp_options ipcp_wantoptions[];
extern ipcp_options ipcp_gotoptions[];
extern ipcp_options ipcp_allowoptions[];
extern ipcp_options ipcp_hisoptions[];

void ipcp_init __ARGS((int));
void ipcp_vj_setmode __ARGS((int));
void ipcp_activeopen __ARGS((int));
void ipcp_passiveopen __ARGS((int));
void ipcp_close __ARGS((int));
void ipcp_lowerup __ARGS((int));
void ipcp_lowerdown __ARGS((int));
void ipcp_input __ARGS((int, u_char *, int));
void ipcp_protrej __ARGS((int));
