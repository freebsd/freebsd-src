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
 * $Id$
 */

/*
 * Options.
 */
#define CI_ADDRS	1	/* IP Addresses */
#define CI_COMPRESSTYPE	2	/* Compression Type */
#define	CI_ADDR		3

#define CI_MS_WINS1	128	/* Primary WINS value */
#define CI_MS_DNS1	129	/* Primary DNS value */
#define CI_MS_WINS2	130	/* Secondary WINS value */
#define CI_MS_DNS2	131	/* Secondary DNS value */

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
    u_int32_t dnsaddr[2];	/* Primary and secondary MS DNS entries */
    u_int32_t winsaddr[2];	/* Primary and secondary MS WINS entries */
} ipcp_options;

extern fsm ipcp_fsm[];
extern ipcp_options ipcp_wantoptions[];
extern ipcp_options ipcp_gotoptions[];
extern ipcp_options ipcp_allowoptions[];
extern ipcp_options ipcp_hisoptions[];

char *ip_ntoa __P((u_int32_t));

extern struct protent ipcp_protent;
