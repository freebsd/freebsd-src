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

#if !defined(lint) && !defined(sgi) && !defined(__NetBSD__)
static char sccsid[] = "@(#)if.c	8.1 (Berkeley) 6/5/93";
#elif defined(__NetBSD__)
static char rcsid[] = "$NetBSD$";
#endif
#ident "$Revision: 1.9 $"

#include "defs.h"
#include "pathnames.h"


struct parm *parms;
struct intnet *intnets;


/* use configured parameters
 */
void
get_parms(struct interface *ifp)
{
	struct parm *parmp;

	/* get all relevant parameters
	 */
	for (parmp = parms; parmp != 0; parmp = parmp->parm_next) {
		if ((parmp->parm_name[0] == '\0'
		     && on_net(ifp->int_addr,
			       parmp->parm_addr_h, parmp->parm_mask))
		    || (parmp->parm_name[0] != '\0'
			&& !strcmp(ifp->int_name, parmp->parm_name))) {
			/* this group of parameters is relevant,
			 * so get its settings
			 */
			ifp->int_state |= parmp->parm_int_state;
			if (parmp->parm_passwd[0] != '\0')
				bcopy(parmp->parm_passwd, ifp->int_passwd,
				      sizeof(ifp->int_passwd));
			if (parmp->parm_rdisc_pref != 0)
				ifp->int_rdisc_pref = parmp->parm_rdisc_pref;
			if (parmp->parm_rdisc_int != 0)
				ifp->int_rdisc_int = parmp->parm_rdisc_int;
			if (parmp->parm_d_metric != 0)
				ifp->int_d_metric = parmp->parm_d_metric;
			}
	}
	/* default poor-man's router discovery to a metric that will
	 * be heard by old versions of routed.
	 */
	if ((ifp->int_state & IS_PM_RDISC)
	    && ifp->int_d_metric == 0)
		ifp->int_d_metric = HOPCNT_INFINITY-2;

	if (IS_RIP_IN_OFF(ifp->int_state))
		ifp->int_state |= IS_NO_RIP_OUT;

	if (ifp->int_rdisc_int == 0)
		ifp->int_rdisc_int = DefMaxAdvertiseInterval;

	if (!(ifp->int_if_flags & IFF_MULTICAST)
	    && !(ifp->int_if_flags & IFF_POINTOPOINT))
		ifp->int_state |= IS_NO_RIPV2_OUT;

	if (!(ifp->int_if_flags & IFF_MULTICAST))
		ifp->int_state |= IS_BCAST_RDISC;

	if (ifp->int_if_flags & IFF_POINTOPOINT) {
		ifp->int_state |= IS_BCAST_RDISC;
		/* By default, point-to-point links should be passive
		 * about router-discovery for the sake of demand-dialing.
		 */
		if (0 == (ifp->int_state & GROUP_IS_SOL))
			ifp->int_state |= IS_NO_SOL_OUT;
		if (0 == (ifp->int_state & GROUP_IS_ADV))
			ifp->int_state |= IS_NO_ADV_OUT;
	}

	if (0 != (ifp->int_state & (IS_PASSIVE | IS_REMOTE)))
		ifp->int_state |= IS_NO_RDISC;
	if (ifp->int_state & IS_PASSIVE)
		ifp->int_state |= (IS_NO_RIP | IS_NO_RDISC);
	if ((ifp->int_state & (IS_NO_RIP | IS_NO_RDISC))
	    == (IS_NO_RIP|IS_NO_RDISC))
		ifp->int_state |= IS_PASSIVE;
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
		p = lptr+strlen(lptr)-1;
		while (*p == '\n'
		       || *p == ' ')
			*p-- = '\0';

		/* notice newfangled parameter lines
		 */
		if (strncasecmp("net", lptr, 3)
		    && strncasecmp("host", lptr, 4)) {
			p = parse_parms(lptr);
			if (p != 0) {
				if (strcmp(p,lptr))
					msglog("bad \"%s\" in "_PATH_GATEWAYS
					       " entry \"%s\"", lptr, p);
				else
					msglog("bad \"%s\" in "_PATH_GATEWAYS,
					       lptr);
			}
			continue;
		}

/*  {net | host} XX[/M] XX gateway XX metric DD [passive | external]\n */
		n = sscanf(lptr, "%4s %129[^ \t] gateway"
			   " %64[^ / \t] metric %d %8s\n",
			   net_host, dname, gname, &metric, qual);
		if (n != 5) {
			msglog("bad "_PATH_GATEWAYS" entry \"%s\"", lptr);
			continue;
		}
		if (metric < 0 || metric >= HOPCNT_INFINITY) {
			msglog("bad metric in "_PATH_GATEWAYS" entry \"%s\"",
			       lptr);
			continue;
		}
		if (!strcmp(net_host, "host")) {
			if (!gethost(dname, &dst)) {
				msglog("bad host \"%s\" in "_PATH_GATEWAYS
				       " entry \"%s\"", dname, lptr);
				continue;
			}
			netmask = HOST_MASK;
		} else if (!strcmp(net_host, "net")) {
			if (!getnet(dname, &dst, &netmask)) {
				msglog("bad net \"%s\" in "_PATH_GATEWAYS
				       " entry \"%s\"", dname, lptr);
				continue;
			}
		} else {
			msglog("bad \"%s\" in "_PATH_GATEWAYS
			       " entry \"%s\"", lptr);
			continue;
		}

		if (!gethost(gname, &gate)) {
			msglog("bad gateway \"%s\" in "_PATH_GATEWAYS
			       " entry \"%s\"", gname, lptr);
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
			msglog("bad "_PATH_GATEWAYS" entry \"%s\"", lptr);
			continue;
		}

		/* Remember to advertise the corresponding logical network.
		 */
		if (!(state & IS_EXTERNAL)
		    && netmask != std_mask(dst))
			state |= IS_SUBNET;

		if (0 != (state & (IS_PASSIVE | IS_REMOTE)))
			state |= IS_NO_RDISC;
		if (state & IS_PASSIVE)
			state |= (IS_NO_RIP | IS_NO_RDISC);
		if ((state & (IS_NO_RIP | IS_NO_RDISC))
		    == (IS_NO_RIP|IS_NO_RDISC))
			state |= IS_PASSIVE;

		parmp = (struct parm*)malloc(sizeof(*parmp));
		bzero(parmp, sizeof(*parmp));
		parmp->parm_next = parms;
		parms = parmp;
		parmp->parm_addr_h = ntohl(dst);
		parmp->parm_mask = -1;
		parmp->parm_d_metric = 0;
		parmp->parm_int_state = state;

		/* See if this new interface duplicates an existing
		 * interface.
		 */
		for (ifp = ifnet; 0 != ifp; ifp = ifp->int_next) {
			if (ifp->int_mask == netmask
			    && ((ifp->int_addr == dst
				 && netmask != HOST_MASK)
				|| (ifp->int_dstaddr == dst
				    && netmask == HOST_MASK)))
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

		trace_if("Add", ifp);
	}
}


/* parse a set of parameters for an interface
 */
char *					/* 0 or error message */
parse_parms(char *line)
{
#define PARS(str) (0 == (tgt = str, strcasecmp(tok, tgt)))
#define PARSE(str) (0 == (tgt = str, strncasecmp(tok, str "=", sizeof(str))))
#define CKF(g,b) {if (0 != (parm.parm_int_state & ((g) & ~(b)))) break;	\
	parm.parm_int_state |= (b);}
#define DELIMS " ,\t\n"
	struct parm parm;
	struct intnet *intnetp;
	char *tok, *tgt, *p;


	/* "subnet=x.y.z.u/mask" must be alone on the line */
	if (!strncasecmp("subnet=",line,7)) {
		intnetp = (struct intnet*)malloc(sizeof(*intnetp));
		intnetp->intnet_metric = 1;
		if ((p = strrchr(line,','))) {
			*p++ = '\0';
			intnetp->intnet_metric = (int)strtol(p,&p,0);
			if (*p != '\0'
			    || intnetp->intnet_metric <= 0
			    || intnetp->intnet_metric >= HOPCNT_INFINITY)
				return line;
		}
		if (!getnet(&line[7], &intnetp->intnet_addr,
			    &intnetp->intnet_mask)
		    || intnetp->intnet_mask == HOST_MASK
		    || intnetp->intnet_addr == RIP_DEFAULT) {
			free(intnetp);
			return line;
		}
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
			parm.parm_int_state |= (IS_NO_AG | IS_NO_SUPER_AG);

		} else if (PARS("no_super_ag")) {
			parm.parm_int_state |= IS_NO_SUPER_AG;

		} else if (PARS("no_ripv1_in")) {
			parm.parm_int_state |= IS_NO_RIPV1_IN;

		} else if (PARS("no_ripv2_in")) {
			parm.parm_int_state |= IS_NO_RIPV2_IN;

		} else if (PARS("ripv2_out")) {
			if (parm.parm_int_state & IS_NO_RIPV2_OUT)
				break;
			parm.parm_int_state |= IS_NO_RIPV1_OUT;

		} else if (PARS("no_rip")) {
			parm.parm_int_state |= IS_NO_RIP;

		} else if (PARS("no_rdisc")) {
			CKF((GROUP_IS_SOL|GROUP_IS_ADV), IS_NO_RDISC);

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

		} else if (PARS("passive")) {
			CKF((GROUP_IS_SOL|GROUP_IS_ADV), IS_NO_RDISC);
			parm.parm_int_state |= IS_NO_RIP;

		} else if (PARSE("rdisc_pref")) {
			if (parm.parm_rdisc_pref != 0
			    || tok[11] == '\0'
			    || (parm.parm_rdisc_pref = (int)strtol(&tok[11],
								   &p,0),
				*p != '\0'))
				break;

		} else if (PARS("pm_rdisc")) {
			parm.parm_int_state |= IS_PM_RDISC;

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
			    || parm.parm_d_metric > HOPCNT_INFINITY-1)
				break;

		} else {
			tgt = tok;
			break;
		}
	}
	if (tgt != 0)
		return tgt;

	return check_parms(&parm);
#undef DELIMS
#undef PARS
#undef PARSE
}


/* check for duplicate parameter specifications */
char *					/* 0 or error message */
check_parms(struct parm *new)
{
	struct parm *parmp;


	/* set implicit values
	 */
	if (!supplier && supplier_set)
		new->parm_int_state |= (IS_NO_RIPV1_OUT
					| IS_NO_RIPV2_OUT
					| IS_NO_ADV_OUT);
	if (new->parm_int_state & IS_NO_ADV_IN)
		new->parm_int_state |= IS_NO_SOL_OUT;

	if ((new->parm_int_state & (IS_NO_RIP | IS_NO_RDISC))
	    == (IS_NO_RIP | IS_NO_RDISC))
		new->parm_int_state |= IS_PASSIVE;

	/* compare with existing sets of parameters
	 */
	for (parmp = parms; parmp != 0; parmp = parmp->parm_next) {
		if (strcmp(new->parm_name, parmp->parm_name))
			continue;
		if (!on_net(htonl(parmp->parm_addr_h),
			    new->parm_addr_h, new->parm_mask)
		    && !on_net(htonl(new->parm_addr_h),
			       parmp->parm_addr_h, parmp->parm_mask))
			continue;

		if (strcmp(parmp->parm_passwd, new->parm_passwd)
		    || (0 != (new->parm_int_state & GROUP_IS_SOL)
			&& 0 != (parmp->parm_int_state & GROUP_IS_SOL)
			&& 0 != ((new->parm_int_state ^ parmp->parm_int_state)
				 && GROUP_IS_SOL))
		    || (0 != (new->parm_int_state & GROUP_IS_ADV)
			&& 0 != (parmp->parm_int_state & GROUP_IS_ADV)
			&& 0 != ((new->parm_int_state ^ parmp->parm_int_state)
				 && GROUP_IS_ADV))
		    || (new->parm_rdisc_pref != 0
			&& parmp->parm_rdisc_pref != 0
			&& new->parm_rdisc_pref != parmp->parm_rdisc_pref)
		    || (new->parm_rdisc_int != 0
			&& parmp->parm_rdisc_int != 0
			&& new->parm_rdisc_int != parmp->parm_rdisc_int)
		    || (new->parm_d_metric != 0
			&& parmp->parm_d_metric != 0
			&& new->parm_d_metric != parmp->parm_d_metric))
			return "duplicate";
	}

	parmp = (struct parm*)malloc(sizeof(*parmp));
	bcopy(new, parmp, sizeof(*parmp));
	parmp->parm_next = parms;
	parms = parmp;

	return 0;
}


/* get a network number as a name or a number, with an optional "/xx"
 * netmask.
 */
int					/* 0=bad */
getnet(char *name,
       naddr *addrp,			/* host byte order */
       naddr *maskp)
{
	int i;
	struct netent *np;
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

	np = getnetbyname(name);
	if (np != 0) {
		in.s_addr = (naddr)np->n_net;
	} else if (inet_aton(name, &in) == 1) {
		HTONL(in.s_addr);
	} else {
		return 0;
	}

	if (mname == 0) {
		/* we cannot use the interfaces here because we have not
		 * looked at them yet.
		 */
		mask = std_mask(in.s_addr);
		if ((~mask & ntohl(in.s_addr)) != 0)
			mask = HOST_MASK;
	} else {
		mask = (naddr)strtoul(mname, &p, 0);
		if (*p != '\0' || mask > 32)
			return 0;
		mask = HOST_MASK << (32-mask);
	}
	if (mask != 0 && in.s_addr == RIP_DEFAULT)
		return 0;
	if ((~mask & ntohl(in.s_addr)) != 0)
		return 0;

	*addrp = in.s_addr;
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
