/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)if.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */

#ident "$Revision: 1.1 $"

#include "defs.h"
#include "pathnames.h"


struct parm *parms;
struct intnet *intnets;


/* parse a set of parameters for an interface
 */
char *					/* error message */
parse_parms(char *line)
{
#define PARS(str) (0 == (tgt = str, strcasecmp(tok, tgt)))
#define PARSE(str) (0 == (tgt = str, strncasecmp(tok, str "=", sizeof(str))))
#define CKF(g,b) {if (0 != (parm.parm_int_state & ((g) & ~(b)))) break;	\
	parm.parm_int_state |= (b);}
#define DELIMS " ,\t\n"
	struct parm parm, *parmp;
	struct intnet *intnetp;
	char *tok, *tgt, *p;


	/* "subnet=x.y.z.u/mask" must be alone on the line */
	if (!strncasecmp("subnet=",line,7)) {
		intnetp = (struct intnet*)malloc(sizeof(*intnetp));
		if (!getnet(&line[7], &intnetp->intnet_addr,
			    &intnetp->intnet_mask)) {
			free(intnetp);
			return line;
		}
		HTONL(intnetp->intnet_addr);
		intnetp->intnet_next = intnets;
		intnets = intnetp;
		return 0;
	}

	bzero(&parm, sizeof(parm));

	tgt = "null";
	for (tok = strtok(line, DELIMS);
	     tok != 0 && tok[0] != '\0';
	     tgt = 0, tok = strtok(0,DELIMS)) {
		if (PARSE("if")) {
			if (parm.parm_name[0] != '\0'
			    || tok[3] == '\0'
			    || strlen(tok) > IFNAMSIZ+3)
				break;
			strcpy(parm.parm_name, tok+3);

		} else if (PARSE("passwd")) {
			if (tok[7] == '\0'
			    || strlen(tok) > RIP_AUTH_PW_LEN+7)
				break;
			strcpy(parm.parm_passwd, tok+7);

		} else if (PARS("no_ag")) {
			parm.parm_int_state |= IS_NO_AG;

		} else if (PARS("no_super_ag")) {
			parm.parm_int_state |= IS_NO_SUPER_AG;

		} else if (PARS("no_rip")) {
			parm.parm_int_state |= (IS_NO_RIPV1_IN
						| IS_NO_RIPV2_IN
						| IS_NO_RIPV1_OUT
						| IS_NO_RIPV2_OUT);

		} else if (PARS("no_ripv1_in")) {
			parm.parm_int_state |= IS_NO_RIPV1_IN;

		} else if (PARS("no_ripv2_in")) {
			parm.parm_int_state |= IS_NO_RIPV2_IN;

		} else if (PARS("no_ripv2_out")) {
			parm.parm_int_state |= IS_NO_RIPV2_OUT;

		} else if (PARS("ripv2_out")) {
			if (parm.parm_int_state & IS_NO_RIPV2_OUT)
				break;
			parm.parm_int_state |= IS_NO_RIPV1_OUT;

		} else if (PARS("no_rdisc")) {
			CKF((GROUP_IS_SOL|GROUP_IS_ADV),
			    IS_NO_ADV_IN | IS_NO_SOL_OUT | IS_NO_ADV_OUT);

		} else if (PARS("no_solicit")) {
			CKF(GROUP_IS_SOL, IS_NO_SOL_OUT);

		} else if (PARS("send_solicit")) {
			CKF(GROUP_IS_SOL, IS_SOL_OUT);

		} else if (PARS("no_rdisc_adv")) {
			CKF(GROUP_IS_ADV, IS_NO_ADV_OUT);

		} else if (PARS("rdisc_adv")) {
			CKF(GROUP_IS_ADV, IS_ADV_OUT);

		} else if (PARS("bcast_rdisc")) {
			parm.parm_int_state |= IS_BCAST_RDISC;

		} else if (PARSE("rdisc_pref")) {
			if (parm.parm_rdisc_pref != 0
			    || tok[11] == '\0'
			    || (parm.parm_rdisc_pref = (int)strtol(&tok[11],
								   &p,0),
				*p != '\0'))
				break;

		} else if (PARSE("rdisc_interval")) {
			if (parm.parm_rdisc_int != 0
			    || tok[15] == '\0'
			    || (parm.parm_rdisc_int = (int)strtol(&tok[15],
								  &p,0),
				*p != '\0')
			    || parm.parm_rdisc_int < MinMaxAdvertiseInterval
			    || parm.parm_rdisc_int > MaxMaxAdvertiseInterval)
				break;

		} else if (PARSE("fake_default")) {
			if (parm.parm_d_metric != 0
			    || tok[13] == '\0'
			    || (parm.parm_d_metric=(int)strtol(&tok[13],&p,0),
				*p != '\0')
			    || parm.parm_d_metric >= HOPCNT_INFINITY-2)
				break;

		} else {
			tgt = tok;
			break;
		}
	}
	if (tgt != 0)
		return tgt;

	if (parm.parm_int_state & IS_NO_ADV_IN)
		parm.parm_int_state |= IS_NO_SOL_OUT;

	/* check for duplicate specification */
	for (parmp = parms; parmp != 0; parmp = parmp->parm_next) {
		if (strcmp(parm.parm_name, parmp->parm_name))
			continue;
		if (parmp->parm_a_h != (parm.parm_a_h & parmp->parm_m)
		    && parm.parm_a_h != (parmp->parm_a_h & parm.parm_m))
			continue;

		if (strcmp(parmp->parm_passwd, parm.parm_passwd)
		    || (0 != (parm.parm_int_state & GROUP_IS_SOL)
			&& 0 != (parmp->parm_int_state & GROUP_IS_SOL)
			&& 0 != ((parm.parm_int_state ^ parmp->parm_int_state)
				 && GROUP_IS_SOL))
		    || (0 != (parm.parm_int_state & GROUP_IS_ADV)
			&& 0 != (parmp->parm_int_state & GROUP_IS_ADV)
			&& 0 != ((parm.parm_int_state ^ parmp->parm_int_state)
				 && GROUP_IS_ADV))
		    || (parm.parm_rdisc_pref != 0
			&& parmp->parm_rdisc_pref != 0
			&& parm.parm_rdisc_pref != parmp->parm_rdisc_pref)
		    || (parm.parm_rdisc_int != 0
			&& parmp->parm_rdisc_int != 0
			&& parm.parm_rdisc_int != parmp->parm_rdisc_int)
		    || (parm.parm_d_metric != 0
			&& parmp->parm_d_metric != 0
			&& parm.parm_d_metric != parmp->parm_d_metric))
			return "duplicate";
	}

	parmp = (struct parm*)malloc(sizeof(*parmp));
	bcopy(&parm, parmp, sizeof(*parmp));
	parmp->parm_next = parms;
	parms = parmp;

	return 0;
#undef DELIMS
#undef PARS
#undef PARSE
}


/* use configured parameters
 */
void
get_parms(struct interface *ifp)
{
	struct parm *parmp;

	for (parmp = parms; parmp != 0; parmp = parmp->parm_next) {
		if ((parmp->parm_a_h == (ntohl(ifp->int_addr)
					 & parmp->parm_m)
		     && parmp->parm_name[0] == '\0')
		    || (parmp->parm_name[0] != '\0'
			&& !strcmp(ifp->int_name, parmp->parm_name))) {
			ifp->int_state |= parmp->parm_int_state;
			bcopy(parmp->parm_passwd, ifp->int_passwd,
			      sizeof(ifp->int_passwd));
			ifp->int_rdisc_pref = parmp->parm_rdisc_pref;
			ifp->int_rdisc_int = parmp->parm_rdisc_int;
			ifp->int_d_metric = parmp->parm_d_metric;
		}
	}

	if ((ifp->int_state & IS_NO_RIP_IN) == IS_NO_RIP_IN)
		ifp->int_state |= IS_NO_RIP_OUT;

	if (ifp->int_rdisc_int == 0)
		ifp->int_rdisc_int = DefMaxAdvertiseInterval;

	if ((ifp->int_state & IS_PASSIVE)
	    || (ifp->int_state & IS_REMOTE))
		ifp->int_state |= IS_NO_ADV_IN|IS_NO_SOL_OUT|IS_NO_ADV_OUT;


	if (!(ifp->int_state & IS_PASSIVE)) {
		if (!(ifp->int_if_flags & IFF_MULTICAST)
		    && !(ifp->int_if_flags & IFF_POINTOPOINT))
			ifp->int_state |= IS_NO_RIPV2_OUT;
	}

	if (!(ifp->int_if_flags & IFF_MULTICAST))
		ifp->int_state |= IS_BCAST_RDISC;

	if (ifp->int_if_flags & IFF_POINTOPOINT) {
		ifp->int_state |= IS_BCAST_RDISC;
		/* point-to-point links should be passive for the sake
		 * of demand-dialing
		 */
		if (0 == (ifp->int_state & GROUP_IS_SOL))
			ifp->int_state |= IS_NO_SOL_OUT;
		if (0 == (ifp->int_state & GROUP_IS_ADV))
			ifp->int_state |= IS_NO_ADV_OUT;
	}
}


/* Read a list of gateways from /etc/gateways and add them to our tables.
 *
 * This file contains a list of "remote" gateways.  That is usually
 * a gateway which we cannot immediately determine if it is present or
 * not as we can do for those provided by directly connected hardware.
 *
 * If a gateway is marked "passive" in the file, then we assume it
 * does not understand RIP and assume it is always present.  Those
 * not marked passive are treated as if they were directly connected
 * and assumed to be broken if they do not send us advertisements.
 * All remote interfaces are added to our list, and those not marked
 * passive are sent routing updates.
 *
 * A passive interface can also be local, hardware interface exempt
 * from RIP.
 */
void
gwkludge(void)
{
	FILE *fp;
	char *p, *lptr;
	char lbuf[200], net_host[5], dname[64+1+64+1], gname[64+1], qual[9];
	struct interface *ifp;
	naddr dst, netmask, gate;
	int metric, n;
	u_int state;
	char *type;
	struct parm *parmp;


	fp = fopen(_PATH_GATEWAYS, "r");
	if (fp == 0)
		return;

	for (;;) {
		if (0 == fgets(lbuf, sizeof(lbuf)-1, fp))
			break;
		lptr = lbuf;
		while (*lptr == ' ')
			lptr++;
		if (*lptr == '\n'	/* ignore null and comment lines */
		    || *lptr == '#')
			continue;

		/* notice parameter lines */
		if (strncasecmp("net", lptr, 3)
		    && strncasecmp("host", lptr, 4)) {
			p = parse_parms(lptr);
			if (p != 0)
				msglog("bad \"%s\" in "_PATH_GATEWAYS
				       " entry %s", lptr, p);
			continue;
		}

/*  {net | host} XX[/M] XX gateway XX metric DD [passive | external]\n */
		n = sscanf(lptr, "%4s %129[^	] gateway"
			   " %64[^ /	] metric %d %8s\n",
			   net_host, dname, gname, &metric, qual);
		if (n != 5) {
			msglog("bad "_PATH_GATEWAYS" entry %s", lptr);
			continue;
		}
		if (metric < 0 || metric >= HOPCNT_INFINITY) {
			msglog("bad metric in "_PATH_GATEWAYS" entry %s",
			       lptr);
			continue;
		}
		if (!strcmp(net_host, "host")) {
			if (!gethost(dname, &dst)) {
				msglog("bad host %s in "_PATH_GATEWAYS
				       " entry %s", dname, lptr);
				continue;
			}
			netmask = HOST_MASK;
		} else if (!strcmp(net_host, "net")) {
			if (!getnet(dname, &dst, &netmask)) {
				msglog("bad net %s in "_PATH_GATEWAYS
				       " entry %s", dname, lptr);
				continue;
			}
			HTONL(dst);
		} else {
			msglog("bad \"%s\" in "_PATH_GATEWAYS
			       " entry %s", lptr);
			continue;
		}

		if (!gethost(gname, &gate)) {
			msglog("bad gateway %s in "_PATH_GATEWAYS
			       " entry %s", gname, lptr);
			continue;
		}

		if (strcmp(qual, type = "passive") == 0) {
			/* Passive entries are not placed in our tables,
			 * only the kernel's, so we don't copy all of the
			 * external routing information within a net.
			 * Internal machines should use the default
			 * route to a suitable gateway (like us).
			 */
			state = IS_REMOTE | IS_PASSIVE;
			if (metric == 0)
				metric = 1;

		} else if (strcmp(qual, type = "external") == 0) {
			/* External entries are handled by other means
			 * such as EGP, and are placed only in the daemon
			 * tables to prevent overriding them with something
			 * else.
			 */
			state = IS_REMOTE | IS_PASSIVE | IS_EXTERNAL;
			if (metric == 0)
				metric = 1;

		} else if (qual[0] == '\0') {
			if (metric != 0) {
				/* Entries that are neither "passive" nor
				 * "external" are "remote" and must behave
				 * like physical interfaces.  If they are not
				 * heard from regularly, they are deleted.
				 */
				state = IS_REMOTE;
				type = "remote";
			} else {
				/* "remote" entries with a metric of 0
				 * are aliases for our own interfaces
				 */
				state = IS_REMOTE | IS_PASSIVE;
				type = "alias";
			}

		} else {
			msglog("bad "_PATH_GATEWAYS" entry %s", lptr);
			continue;
		}

		if (!(state & IS_EXTERNAL)) {
			/* If we are going to send packets to the gateway,
			 * it must be reachable using our physical interfaces
			 */
			if (!rtfind(gate)) {
				msglog("unreachable gateway %s in "
				       _PATH_GATEWAYS" entry %s",
				       gname, lptr);
				continue;
			}

			/* Remember to advertise the corresponding logical
			 * network.
			 */
			if (netmask != std_mask(dst))
				state |= IS_SUBNET;
		}

		parmp = (struct parm*)malloc(sizeof(*parmp));
		bzero(parmp, sizeof(*parmp));
		parmp->parm_next = parms;
		parms = parmp;
		parmp->parm_a_h = ntohl(dst);
		parmp->parm_m = -1;
		parmp->parm_d_metric = 0;
		parmp->parm_int_state = state;

		/* See if this new interface duplicates an existing
		 * interface.
		 */
		for (ifp = ifnet; 0 != ifp; ifp = ifp->int_next) {
			if (ifp->int_addr == dst
			    && ifp->int_mask == netmask)
				break;
		}
		if (ifp != 0) {
			/* Let one of our real interfaces be marked passive.
			 */
			if ((state & IS_PASSIVE) && !(state & IS_EXTERNAL)) {
				ifp->int_state |= state;
			} else {
				msglog("%s is duplicated in "_PATH_GATEWAYS
				       " by %s",
				       ifp->int_name, lptr);
			}
			continue;
		}

		tot_interfaces++;

		ifp = (struct interface *)malloc(sizeof(*ifp));
		bzero(ifp, sizeof(*ifp));
		if (ifnet != 0) {
			ifp->int_next = ifnet;
			ifnet->int_prev = ifp;
		}
		ifnet = ifp;

		ifp->int_state = state;
		ifp->int_net = ntohl(dst) & netmask;
		ifp->int_mask = netmask;
		if (netmask == HOST_MASK)
			ifp->int_if_flags |= IFF_POINTOPOINT;
		ifp->int_dstaddr = dst;
		ifp->int_addr = gate;
		ifp->int_metric = metric;
		(void)sprintf(ifp->int_name, "%s-%s", type, naddr_ntoa(dst));
		ifp->int_index = -1;

		get_parms(ifp);

		if (TRACEACTIONS)
			trace_if("Add", ifp);
	}
}


/* get a network number as a name or a number, with an optional "/xx"
 * netmask.
 */
int					/* 0=bad */
getnet(char *name,
       naddr *addr_hp,
       naddr *maskp)
{
	int i;
	struct netent *nentp;
	naddr mask;
	struct in_addr in;
	char hname[MAXHOSTNAMELEN+1];
	char *mname, *p;


	/* Detect and separate "1.2.3.4/24"
	 */
	if (0 != (mname = rindex(name,'/'))) {
		i = (int)(mname - name);
		if (i > sizeof(hname)-1)	/* name too long */
			return 0;
		bcopy(name, hname, i);
		hname[i] = '\0';
		mname++;
		name = hname;
	}

	nentp = getnetbyname(name);
	if (nentp != 0) {
		in.s_addr = (naddr)nentp->n_net;
	} else if (inet_aton(name, &in) == 1) {
		NTOHL(in.s_addr);
	} else {
		return 0;
	}

	if (mname == 0) {
		mask = std_mask(in.s_addr);
	} else {
		mask = (naddr)strtoul(mname, &p, 0);
		if (*p != '\0' || mask > 32)
			return 0;
		mask = HOST_MASK << (32-mask);
	}

	*addr_hp = in.s_addr;
	*maskp = mask;
	return 1;
}


int					/* 0=bad */
gethost(char *name,
	naddr *addrp)
{
	struct hostent *hp;
	struct in_addr in;


	/* Try for a number first, even in IRIX where gethostbyname()
	 * is smart.  This avoids hitting the name server which
	 * might be sick because routing is.
	 */
	if (inet_aton(name, &in) == 1) {
		*addrp = in.s_addr;
		return 1;
	}

	hp = gethostbyname(name);
	if (hp) {
		bcopy(hp->h_addr, addrp, sizeof(*addrp));
		return 1;
	}

	return 0;
}
