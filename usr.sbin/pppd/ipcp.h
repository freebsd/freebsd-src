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
 *
 * $FreeBSD$
 */

/*
 * Options.
 */
#define CI_ADDRS	1	/* IP Addresses */
#define CI_COMPRESSTYPE	2	/* Compression Type */
#define	CI_ADDR		3
#define CI_DNS1		129	/* Primary DNS */
#define CI_NBNS1	130	/* Primary NBNS */
#define CI_DNS2		131	/* Secondary DNS */
#define CI_NBNS2	132	/* Secondary NBNS */

#define MAX_STATES 16		/* from slcompress.h */

#define IPCP_VJMODE_OLD 1	/* "old" mode (option # = 0x0037) */
#define IPCP_VJMODE_RFC1172 2	/* "old-rfc"mode (option # = 0x002d) */
#define IPCP_VJMODE_RFC1332 3	/* "new-rfc"mode (option # = 0x002d, */
                                /*  maxslot and slot number compression) */

#define IPCP_VJ_COMP 0x002d	/* current value for VJ compression option*/
#define IPCP_VJ_COMP_OLD 0x0037	/* "old" (i.e, broken) value for VJ */
				/* compression option*/ 

typedef struct ipcp_options {
    int neg_addr : 1;		/* Negotiate IP Address? */
    int old_addrs : 1;		/* Use old (IP-Addresses) option? */
    int req_addr : 1;		/* Ask peer to send IP address? */
    int default_route : 1;	/* Assign default route through interface? */
    int proxy_arp : 1;		/* Make proxy ARP entry for peer? */
    int neg_vj : 1;		/* Van Jacobson Compression? */
    int old_vj : 1;		/* use old (short) form of VJ option? */
    int accept_local : 1;	/* accept peer's value for ouraddr */
    int accept_remote : 1;	/* accept peer's value for hisaddr */
    u_short vj_protocol;	/* protocol value to use in VJ option */
    u_char maxslotindex, cflag;	/* values for RFC1332 VJ compression neg. */
    u_int32_t ouraddr, hisaddr;	/* Addresses in NETWORK BYTE ORDER */
} ipcp_options;

extern fsm ipcp_fsm[];
extern ipcp_options ipcp_wantoptions[];
extern ipcp_options ipcp_gotoptions[];
extern ipcp_options ipcp_allowoptions[];
extern ipcp_options ipcp_hisoptions[];

void ipcp_init __P((int));
void ipcp_open __P((int));
void ipcp_close __P((int));
void ipcp_lowerup __P((int));
void ipcp_lowerdown __P((int));
void ipcp_input __P((int, u_char *, int));
void ipcp_protrej __P((int));
int  ipcp_printpkt __P((u_char *, int, void (*)(), void *));
