/*
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * OSPF support contributed by Jeffrey Honig (jch@mitchell.cit.cornell.edu)
 */

#ifndef lint
static char rcsid[] =
    "@(#) $Header: print-ospf.c,v 1.19 96/07/14 19:39:03 leres Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <ctype.h>
#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"

#include "ospf.h"

struct bits {
    u_int32_t bit;
    const char *str;
};

static const struct bits ospf_option_bits[] = {
	{ OSPF_OPTION_T,	"T" },
	{ OSPF_OPTION_E,	"E" },
	{ OSPF_OPTION_MC,	"MC" },
	{ 0,			NULL }
};

static const struct bits ospf_rla_flag_bits[] = {
	{ RLA_FLAG_B,		"B" },
	{ RLA_FLAG_E,		"E" },
	{ RLA_FLAG_W1,		"W1" },
	{ RLA_FLAG_W2,		"W2" },
	{ 0,			NULL }
};

static const char *ospf_types[OSPF_TYPE_MAX] = {
  (char *) 0,
  "hello",
  "dd",
  "ls_req",
  "ls_upd",
  "ls_ack"
};

static inline void
ospf_print_seqage(register u_int32_t seq, register time_t us)
{
    register time_t sec = us % 60;
    register time_t mins = (us / 60) % 60;
    register time_t hour = us/3600;

    printf(" S %X age ", seq);
    if (hour) {
	printf("%u:%02u:%02u",
	       (u_int32_t)hour,
	       (u_int32_t)mins,
	       (u_int32_t)sec);
    } else if (mins) {
	printf("%u:%02u",
	       (u_int32_t)mins,
	       (u_int32_t)sec);
    } else {
	printf("%u",
	       (u_int32_t)sec);
    }
}


static inline void
ospf_print_bits(register const struct bits *bp, register u_char options)
{
    char sep = ' ';

    do {
	if (options & bp->bit) {
	    printf("%c%s",
		   sep,
		   bp->str);
	    sep = '/';
	}
    } while ((++bp)->bit) ;
}


#define	LS_PRINT(lsp, type) switch (type) { \
    case LS_TYPE_ROUTER: \
	printf(" rtr %s ", ipaddr_string(&lsp->ls_router)); break; \
    case LS_TYPE_NETWORK: \
	printf(" net dr %s if %s", ipaddr_string(&lsp->ls_router), ipaddr_string(&lsp->ls_stateid)); break; \
    case LS_TYPE_SUM_IP: \
	printf(" sum %s abr %s", ipaddr_string(&lsp->ls_stateid), ipaddr_string(&lsp->ls_router)); break; \
    case LS_TYPE_SUM_ABR: \
	printf(" abr %s rtr %s", ipaddr_string(&lsp->ls_router), ipaddr_string(&lsp->ls_stateid)); break; \
    case LS_TYPE_ASE: \
	printf(" ase %s asbr %s", ipaddr_string(&lsp->ls_stateid), ipaddr_string(&lsp->ls_router)); break; \
    case LS_TYPE_GROUP: \
	printf(" group %s rtr %s", ipaddr_string(&lsp->ls_stateid), ipaddr_string(&lsp->ls_router)); break; \
    }

static int
ospf_print_lshdr(register const struct lsa_hdr *lshp, const caddr_t end)
{
    if ((caddr_t) (lshp + 1) > end) {
	return 1;
    }

    printf(" {");						/* } (ctags) */

    if (!lshp->ls_type || lshp->ls_type >= LS_TYPE_MAX) {
	printf(" ??LS type %d?? }", lshp->ls_type);		/* { (ctags) */
	return 1;
    }

    ospf_print_bits(ospf_option_bits, lshp->ls_options);
    ospf_print_seqage(ntohl(lshp->ls_seq), ntohs(lshp->ls_age));

    LS_PRINT(lshp, lshp->ls_type);

    return 0;
}


/*
 * Print a single link state advertisement.  If truncated return 1, else 0.
 */

static int
ospf_print_lsa(register const struct lsa *lsap, const caddr_t end)
{
    register const char *ls_end;
    const struct rlalink *rlp;
    const struct tos_metric *tosp;
    const struct in_addr *ap;
    const struct aslametric *almp;
    const struct mcla *mcp;
    const u_int32_t *lp;
    int j, k;

    if (ospf_print_lshdr(&lsap->ls_hdr, end)) {
	return 1;
    }

    ls_end = (caddr_t) lsap + ntohs(lsap->ls_hdr.ls_length);

    if (ls_end > end) {
	printf(" }");						/* { (ctags) */
	return 1;
    }

    switch (lsap->ls_hdr.ls_type) {
    case LS_TYPE_ROUTER:
	ospf_print_bits(ospf_rla_flag_bits, lsap->lsa_un.un_rla.rla_flags);

	j = ntohs(lsap->lsa_un.un_rla.rla_count);
	rlp = lsap->lsa_un.un_rla.rla_link;
	while (j--) {
	    struct rlalink *rln = (struct rlalink *) ((caddr_t) (rlp + 1) + ((rlp->link_toscount) * sizeof (struct tos_metric)));

	    if ((caddr_t) rln > ls_end) {
		break;
	    }
	    printf(" {");					/* } (ctags) */

	    switch (rlp->link_type) {
	    case RLA_TYPE_VIRTUAL:
		printf(" virt");
		/* Fall through */

	    case RLA_TYPE_ROUTER:
		printf(" nbrid %s if %s",
		       ipaddr_string(&rlp->link_id),
		       ipaddr_string(&rlp->link_data));
		break;

	    case RLA_TYPE_TRANSIT:
		printf(" dr %s if %s",
		       ipaddr_string(&rlp->link_id),
		       ipaddr_string(&rlp->link_data));
		break;

	    case RLA_TYPE_STUB:
		printf(" net %s mask %s",
		       ipaddr_string(&rlp->link_id),
		       ipaddr_string(&rlp->link_data));
		break;

	    default:
		printf(" ??RouterLinksType %d?? }",		/* { (ctags) */
		       rlp->link_type);
		return 0;
	    }
	    printf(" tos 0 metric %d",
		   ntohs(rlp->link_tos0metric));
	    tosp = (struct tos_metric *) ((sizeof rlp->link_tos0metric) + (caddr_t) rlp);
	    for (k = 0; k < rlp->link_toscount; k++, tosp++) {
		printf(" tos %d metric %d",
		       tosp->tos_type,
		       ntohs(tosp->tos_metric));
	    }
	    printf(" }");					/* { (ctags) */
	    rlp = rln;
	}
	break;

    case LS_TYPE_NETWORK:
	printf(" mask %s rtrs",
	       ipaddr_string(&lsap->lsa_un.un_nla.nla_mask));
	for (ap = lsap->lsa_un.un_nla.nla_router;
	     (caddr_t) (ap + 1) <= ls_end;
	     ap++) {
	    printf(" %s",
		   ipaddr_string(ap));
	}
	break;

    case LS_TYPE_SUM_IP:
	printf(" mask %s",
	       ipaddr_string(&lsap->lsa_un.un_sla.sla_mask));
	/* Fall through */

    case LS_TYPE_SUM_ABR:

	for (lp = lsap->lsa_un.un_sla.sla_tosmetric;
	     (caddr_t) (lp + 1) <= ls_end;
	     lp++) {
	    u_int32_t ul = ntohl(*lp);

	    printf(" tos %d metric %d",
		   (ul & SLA_MASK_TOS) >> SLA_SHIFT_TOS,
		   ul & SLA_MASK_METRIC);
	}
	break;

    case LS_TYPE_ASE:
	printf(" mask %s",
	       ipaddr_string(&lsap->lsa_un.un_asla.asla_mask));

	for (almp = lsap->lsa_un.un_asla.asla_metric;
	     (caddr_t) (almp + 1) <= ls_end;
	     almp++) {
	    u_int32_t ul = ntohl(almp->asla_tosmetric);

	    printf(" type %d tos %d metric %d",
		   (ul & ASLA_FLAG_EXTERNAL) ? 2 : 1,
		   (ul & ASLA_MASK_TOS) >> ASLA_SHIFT_TOS,
		   (ul & ASLA_MASK_METRIC));
	    if (almp->asla_forward.s_addr) {
		printf(" forward %s",
		       ipaddr_string(&almp->asla_forward));
	    }
	    if (almp->asla_tag.s_addr) {
		printf(" tag %s",
		       ipaddr_string(&almp->asla_tag));
	    }
	}
	break;

    case LS_TYPE_GROUP:
	/* Multicast extensions as of 23 July 1991 */
	for (mcp = lsap->lsa_un.un_mcla;
	     (caddr_t) (mcp + 1) <= ls_end;
	     mcp++) {
	    switch (ntohl(mcp->mcla_vtype)) {
	    case MCLA_VERTEX_ROUTER:
		printf(" rtr rtrid %s",
		       ipaddr_string(&mcp->mcla_vid));
		break;

	    case MCLA_VERTEX_NETWORK:
		printf(" net dr %s",
		       ipaddr_string(&mcp->mcla_vid));
		break;

	    default:
		printf(" ??VertexType %u??",
		       (u_int32_t)ntohl(mcp->mcla_vtype));
		break;
	    }
	}
    }

    printf(" }");						/* { (ctags) */
    return 0;
}


void
ospf_print(register const u_char *bp, register u_int length,
	   register const u_char *bp2)
{
    register const struct ospfhdr *op;
    register const struct ip *ip;
    register const caddr_t end = (caddr_t)snapend;
    register const struct lsa *lsap;
    register const struct lsa_hdr *lshp;
    char sep;
    int i, j;
    const struct in_addr *ap;
    const struct lsr *lsrp;

    op = (struct ospfhdr *)bp;
    ip = (struct ip  *)bp2;
    /* Print the source and destination address	*/
    (void) printf("%s > %s:",
		  ipaddr_string(&ip->ip_src),
		  ipaddr_string(&ip->ip_dst));

    if ((caddr_t) (&op->ospf_len + 1) > end) {
	goto trunc_test;
    }

    /* If the type is valid translate it, or just print the type */
    /* value.  If it's not valid, say so and return */
    if (op->ospf_type || op->ospf_type < OSPF_TYPE_MAX) {
	printf(" OSPFv%d-%s %d:",
	       op->ospf_version,
	       ospf_types[op->ospf_type],
	       length);
    } else {
	printf(" ospf-v%d-??type %d?? %d:",
	       op->ospf_version,
	       op->ospf_type,
	       length);
	return;
    }

    if (length != ntohs(op->ospf_len)) {
	printf(" ??len %d??",
	       ntohs(op->ospf_len));
	goto trunc_test;
    }

    if ((caddr_t) (&op->ospf_routerid + 1) > end) {
	goto trunc_test;
    }

    /* Print the routerid if it is not the same as the source */
    if (ip->ip_src.s_addr != op->ospf_routerid.s_addr) {
	printf(" rtrid %s",
	       ipaddr_string(&op->ospf_routerid));
    }

    if ((caddr_t) (&op->ospf_areaid + 1) > end) {
	goto trunc_test;
    }

    if (op->ospf_areaid.s_addr) {
	printf(" area %s",
	       ipaddr_string(&op->ospf_areaid));
    } else {
	printf(" backbone");
    }

    if ((caddr_t) (op->ospf_authdata + OSPF_AUTH_SIZE) > end) {
	goto trunc_test;
    }

    if (vflag) {
	/* Print authentication data (should we really do this?) */
	switch (ntohs(op->ospf_authtype)) {
	case OSPF_AUTH_NONE:
	    break;

	case OSPF_AUTH_SIMPLE:
	    printf(" auth ");
	    j = 0;
	    for (i = 0; i < sizeof (op->ospf_authdata); i++) {
		if (!isprint(op->ospf_authdata[i])) {
		    j = 1;
		    break;
		}
	    }
	    if (j) {
		/* Print the auth-data as a string of octets */
		printf("%s.%s",
		       ipaddr_string((struct in_addr *) op->ospf_authdata),
		       ipaddr_string((struct in_addr *) &op->ospf_authdata[sizeof (struct in_addr)]));
	    } else {
		/* Print the auth-data as a text string */
		printf("'%.8s'",
		       op->ospf_authdata);
	    }
	    break;

	default:
	    printf(" ??authtype-%d??",
		   ntohs(op->ospf_authtype));
	    return;
	}
    }


    /* Do rest according to version.	*/
    switch (op->ospf_version) {
    case 2:
        /* ospf version 2	*/
	switch (op->ospf_type) {
	case OSPF_TYPE_UMD:		/* Rob Coltun's special monitoring packets; do nothing	*/
	    break;

	case OSPF_TYPE_HELLO:
	    if ((caddr_t) (&op->ospf_hello.hello_deadint + 1) > end) {
		break;
	    }
	    if (vflag) {
		ospf_print_bits(ospf_option_bits, op->ospf_hello.hello_options);
		printf(" mask %s int %d pri %d dead %u",
		       ipaddr_string(&op->ospf_hello.hello_mask),
		       ntohs(op->ospf_hello.hello_helloint),
		       op->ospf_hello.hello_priority,
		       (u_int32_t)ntohl(op->ospf_hello.hello_deadint));
	    }

	    if ((caddr_t) (&op->ospf_hello.hello_dr + 1) > end) {
		break;
	    }
	    if (op->ospf_hello.hello_dr.s_addr) {
		printf(" dr %s",
		       ipaddr_string(&op->ospf_hello.hello_dr));
	    }

	    if ((caddr_t) (&op->ospf_hello.hello_bdr + 1) > end) {
		break;
	    }
	    if (op->ospf_hello.hello_bdr.s_addr) {
		printf(" bdr %s",
		       ipaddr_string(&op->ospf_hello.hello_bdr));
	    }

	    if (vflag) {
		if ((caddr_t) (op->ospf_hello.hello_neighbor + 1) > end) {
		    break;
		}
		printf(" nbrs");
		for (ap = op->ospf_hello.hello_neighbor;
		     (caddr_t) (ap + 1) <= end;
		     ap++) {
		    printf(" %s",
			   ipaddr_string(ap));
		}
	    }
	    break;			/*  HELLO	*/

	case OSPF_TYPE_DB:
	    if ((caddr_t) (&op->ospf_db.db_seq + 1) > end) {
		break;
	    }
	    ospf_print_bits(ospf_option_bits, op->ospf_db.db_options);
	    sep = ' ';
	    if (op->ospf_db.db_flags & OSPF_DB_INIT) {
		printf("%cI",
		       sep);
		sep = '/';
	    }
	    if (op->ospf_db.db_flags & OSPF_DB_MORE) {
		printf("%cM",
		       sep);
		sep = '/';
	    }
	    if (op->ospf_db.db_flags & OSPF_DB_MASTER) {
		printf("%cMS",
		       sep);
		sep = '/';
	    }
	    printf(" S %X", (u_int32_t)ntohl(op->ospf_db.db_seq));

	    if (vflag) {
		/* Print all the LS adv's */
		lshp = op->ospf_db.db_lshdr;

		while (!ospf_print_lshdr(lshp, end)) {
		    printf(" }");				/* { (ctags) */
		    lshp++;
		}
	    }
	    break;

	case OSPF_TYPE_LSR:
	    if (vflag) {
		for (lsrp = op->ospf_lsr; (caddr_t) (lsrp+1) <= end; lsrp++) {
		    int32_t type;

		    if ((caddr_t) (lsrp + 1) > end) {
			break;
		    }

		    printf(" {");				/* } (ctags) */
		    if (!(type = ntohl(lsrp->ls_type)) || type >= LS_TYPE_MAX) {
			printf(" ??LinkStateType %d }", type);	/* { (ctags) */
			printf(" }");				/* { (ctags) */
			break;
		    }

		    LS_PRINT(lsrp, type);
		    printf(" }");				/* { (ctags) */
		}
	    }
	    break;

	case OSPF_TYPE_LSU:
	    if (vflag) {
		lsap = op->ospf_lsu.lsu_lsa;
		i = ntohl(op->ospf_lsu.lsu_count);

		while (i-- &&
		       !ospf_print_lsa(lsap, end)) {
		    lsap = (struct lsa *) ((caddr_t) lsap + ntohs(lsap->ls_hdr.ls_length));
		}
	    }
	    break;


	case OSPF_TYPE_LSA:
	    if (vflag) {
		lshp = op->ospf_lsa.lsa_lshdr;

		while (!ospf_print_lshdr(lshp, end)) {
		    printf(" }");				/* { (ctags) */
		    lshp++;
		}
		break;
	    }
	}			/* end switch on v2 packet type	*/
	break;

    default:
	printf(" ospf [version %d]",
	       op->ospf_version);
	break;
    }					/* end switch on version	*/

  trunc_test:
    if ((snapend - bp) < length) {
	printf(" [|]");
    }

    return;				/* from ospf_print	*/
}
