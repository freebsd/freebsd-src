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
#ident "$Revision: 1.10 $"

#include "defs.h"
#include "pathnames.h"


struct parm *parms;
struct intnet *intnets;
struct tgate *tgates;


/* use configured parameters
 */
void
get_parms(struct interface *ifp)
{
	static warned_auth_in, warned_auth_out;
	struct parm *parmp;

	/* get all relevant parameters
	 */
	for (parmp = parms; parmp != 0; parmp = parmp->parm_next) {
		if (parmp->parm_name[0] == '\0'
		    || !strcmp(ifp->int_name, parmp->parm_name)
		    || (parmp->parm_name[0] == '\n'
			&& on_net(ifp->int_addr,
				  parmp->parm_net, parmp->parm_mask))) {

			/* This group of parameters is relevant,
			 * so get its settings
			 */
			ifp->int_state |= parmp->parm_int_state;
			if (parmp->parm_auth.type != RIP_AUTH_NONE)
				bcopy(&parmp->parm_auth, &ifp->int_auth,
				      sizeof(ifp->int_auth));
			if (parmp->parm_rdisc_pref != 0)
				ifp->int_rdisc_pref = parmp->parm_rdisc_pref;
			if (parmp->parm_rdisc_int != 0)
				ifp->int_rdisc_int = parmp->parm_rdisc_int;
			if (parmp->parm_d_metric != 0)
				ifp->int_d_metric = parmp->parm_d_metric;
		}
	}

	/* Set general defaults.
	 *
	 * Default poor-man's router discovery to a metric that will
	 * be heard by old versions of `routed`.  They ignored received
	 * routes with metric 15.
	 */
	if ((ifp->int_state & IS_PM_RDISC)
	    && ifp->int_d_metric == 0)
		ifp->int_d_metric = FAKE_METRIC;

	if (ifp->int_rdisc_int == 0)
		ifp->int_rdisc_int = DefMaxAdvertiseInterval;

	if (!(ifp->int_if_flags & IFF_MULTICAST)
	    && !(ifp->int_state & IS_REMOTE))
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
		ifp->int_state |= IS_NO_RIP;

	if (!IS_RIP_IN_OFF(ifp->int_state)
	    && ifp->int_auth.type != RIP_AUTH_NONE
	    && !(ifp->int_state & IS_NO_RIPV1_IN)
	    && !warned_auth_in) {
		msglog("Warning: RIPv1 input via %s"
		       " will be accepted without authentication",
		       ifp->int_name);
		warned_auth_in = 1;
	}
	if (!IS_RIP_OUT_OFF(ifp->int_state)
	    && ifp->int_auth.type != RIP_AUTH_NONE
	    && !(ifp->int_state & IS_NO_RIPV1_OUT)
	    && !warned_auth_out) {
		msglog("Warning: RIPv1 output via %s"
		       " will be sent without authentication",
		       ifp->int_name);
		warned_auth_out = 1;
		ifp->int_auth.type = RIP_AUTH_NONE;
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
		while (*p == '\n' || *p == ' ')
			*p-- = '\0';

		/* notice newfangled parameter lines
		 */
		if (strncasecmp("net", lptr, 3)
		    && strncasecmp("host", lptr, 4)) {
			p = parse_parms(lptr);
			if (p != 0) {
				if (strcasecmp(p,lptr))
					msglog("bad %s in "_PATH_GATEWAYS
					       " entry \"%s\"", p, lptr);
				else
					msglog("bad \"%s\" in "_PATH_GATEWAYS,
					       lptr);
			}
			continue;
		}

/*  {net | host} XX[/M] XX gateway XX metric DD [passive | external]\n */
		qual[0] = '\0';
		n = sscanf(lptr, "%4s %129[^ \t] gateway"
			   " %64[^ / \t] metric %u %8s\n",
			   net_host, dname, gname, &metric, qual);
		if (n != 4 && n != 5) {
			msglog("bad "_PATH_GATEWAYS" entry \"%s\"; %d values",
			       lptr, n);
			continue;
		}
		if (metric >= HOPCNT_INFINITY) {
			msglog("bad metric in "_PATH_GATEWAYS" entry \"%s\"",
			       lptr);
			continue;
		}
		if (!strcasecmp(net_host, "host")) {
			if (!gethost(dname, &dst)) {
				msglog("bad host \"%s\" in "_PATH_GATEWAYS
				       " entry \"%s\"", dname, lptr);
				continue;
			}
			netmask = HOST_MASK;
		} else if (!strcasecmp(net_host, "net")) {
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

		if (!strcasecmp(qual, type = "passive")) {
			/* Passive entries are not placed in our tables,
			 * only the kernel's, so we don't copy all of the
			 * external routing information within a net.
			 * Internal machines should use the default
			 * route to a suitable gateway (like us).
			 */
			state = IS_REMOTE | IS_PASSIVE;
			if (metric == 0)
				metric = 1;

		} else if (!strcasecmp(qual, type = "external")) {
			/* External entries are handled by other means
			 * such as EGP, and are placed only in the daemon
			 * tables to prevent overriding them with something
			 * else.
			 */
			strcpy(qual,"external");
			state = IS_REMOTE | IS_PASSIVE | IS_EXTERNAL;
			if (metric == 0)
				metric = 1;

		} else if (!strcasecmp(qual, "active")
			   || qual[0] == '\0') {
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
				state = IS_REMOTE | IS_PASSIVE | IS_ALIAS;
				type = "alias";
			}

		} else {
			msglog("bad "_PATH_GATEWAYS" entry \"%s\";"
			       " unknown type %s", lptr, qual);
			continue;
		}

		if (0 != (state & (IS_PASSIVE | IS_REMOTE)))
			state |= IS_NO_RDISC;
		if (state & IS_PASSIVE)
			state |= IS_NO_RIP;

		ifp = check_dup(gate,dst,netmask,0);
		if (ifp != 0) {
			msglog("duplicate "_PATH_GATEWAYS" entry \"%s\"",lptr);
			continue;
		}

		ifp = (struct interface *)malloc(sizeof(*ifp));
		bzero(ifp, sizeof(*ifp));

		ifp->int_state = state;
		if (netmask == HOST_MASK)
			ifp->int_if_flags = IFF_POINTOPOINT | IFF_UP_RUNNING;
		else
			ifp->int_if_flags = IFF_UP_RUNNING;
		ifp->int_act_time = NEVER;
		ifp->int_addr = gate;
		ifp->int_dstaddr = dst;
		ifp->int_mask = netmask;
		ifp->int_ripv1_mask = netmask;
		ifp->int_std_mask = std_mask(gate);
		ifp->int_net = ntohl(dst);
		ifp->int_std_net = ifp->int_net & ifp->int_std_mask;
		ifp->int_std_addr = htonl(ifp->int_std_net);
		ifp->int_metric = metric;
		if (!(state & IS_EXTERNAL)
		    && ifp->int_mask != ifp->int_std_mask)
			ifp->int_state |= IS_SUBNET;
		(void)sprintf(ifp->int_name, "%s(%s)", type, gname);
		ifp->int_index = -1;

		if_link(ifp);
	}

	/* After all of the parameter lines have been read,
	 * apply them to any remote interfaces.
	 */
	for (ifp = ifnet; 0 != ifp; ifp = ifp->int_next) {
		get_parms(ifp);

		tot_interfaces++;
		if (!IS_RIP_OFF(ifp->int_state))
			rip_interfaces++;

		trace_if("Add", ifp);
	}
}


/* strtok(), but honoring backslash
 */
static int				/* -1=bad */
parse_quote(char **linep,
	    char *delims,
	    char *delimp,
	    char *buf,
	    int	lim)
{
	char c, *pc, *p;


	pc = *linep;
	if (*pc == '\0')
		return -1;

	for (;;) {
		if (lim == 0)
			return -1;
		c = *pc++;
		if (c == '\0')
			break;

		if (c == '\\' && pc != '\0') {
			if ((c = *pc++) == 'n') {
				c = '\n';
			} else if (c == 'r') {
				c = '\r';
			} else if (c == 't') {
				c = '\t';
			} else if (c == 'b') {
				c = '\b';
			} else if (c >= '0' && c <= '7') {
				c -= '0';
				if (*pc >= '0' && *pc <= '7') {
					c = (c<<3)+(*pc++ - '0');
					if (*pc >= '0' && *pc <= '7')
					    c = (c<<3)+(*pc++ - '0');
				}
			}

		} else {
			for (p = delims; *p != '\0'; ++p) {
				if (*p == c)
					goto exit;
			}
		}

		*buf++ = c;
		--lim;
	}
exit:
	if (delimp != 0)
		*delimp = c;
	*linep = pc-1;
	if (lim != 0)
		*buf = '\0';
	return 0;
}


/* Parse password timestamp
 */
static char *
parse_ts(time_t *tp,
	 char **valp,
	 char *val0,
	 char *delimp,
	 char *buf,
	 u_int bufsize)
{
	struct tm tm;

	if (0 > parse_quote(valp, "| ,\n\r", delimp,
			    buf,bufsize)
	    || buf[bufsize-1] != '\0'
	    || buf[bufsize-2] != '\0') {
		sprintf(buf,"timestamp %.25s", val0);
		return buf;
	}
	strcat(buf,"\n");
	bzero(&tm, sizeof(tm));
	if (5 != sscanf(buf, "%u/%u/%u@%u:%u\n",
			&tm.tm_year, &tm.tm_mon, &tm.tm_mday,
			&tm.tm_hour, &tm.tm_min)) {
		sprintf(buf,"timestamp %.25s", val0);
		return buf;
	}
	if (tm.tm_year <= 37)
		tm.tm_year += 100;

	if ((*tp = mktime(&tm)) == -1) {
		sprintf(buf,"timestamp %.25s", val0);
		return buf;
	}

	return 0;
}


/* Get one or more password, key ID's, and expiration dates in
 * the format
 *	passwd|keyID|year/mon/day@hour:min|year/mon/day@hour:min|passwd|...
 */
static char *				/* 0 or error message */
get_passwds(char *tgt,
	    char *val,
	    struct parm *parmp,
	    u_char type)
{
	static char buf[80];
	char *val0, *p, delim;
	struct auth_key *akp, *akp2;
	int i;
	u_long l;


	if (parmp->parm_auth.type != RIP_AUTH_NONE)
		return "duplicate authentication";
	parmp->parm_auth.type = type;

	bzero(buf, sizeof(buf));

	akp = parmp->parm_auth.keys;
	for (i = 0; i < MAX_AUTH_KEYS; i++, val++, akp++) {
		if ((delim = *val) == '\0')
			break;
		val0 = val;
		if (0 > parse_quote(&val, "| ,\n\r", &delim,
				    (char *)akp->key, sizeof(akp->key)))
			return tgt;

		akp->end = -1;

		if (delim != '|') {
			if (type == RIP_AUTH_MD5)
				return "missing Keyid";
			break;
		}
		val0 = ++val;
		if (0 > parse_quote(&val, "| ,\n\r", &delim, buf,sizeof(buf))
		    || buf[sizeof(buf)-1] != '\0'
		    || (l = strtoul(buf,&p,0)) > 255
		    || *p != '\0') {
			sprintf(buf,"KeyID \"%.20s\"", val0);
			return buf;
		}
		for (akp2 = parmp->parm_auth.keys; akp2 < akp; akp2++) {
			if (akp2->keyid == l) {
				*val = '\0';
				sprintf(buf,"duplicate KeyID \"%.20s\"", val0);
				return buf;
			}
		}
		akp->keyid = (int)l;

		if (delim != '|')
			break;

		val0 = ++val;
		if (0 != (p = parse_ts(&akp->start,&val,val0,&delim,
				       buf,sizeof(buf))))
			return p;
		if (delim != '|')
			return "missing second timestamp";
		val0 = ++val;
		if (0 != (p = parse_ts(&akp->end,&val,val0,&delim,
				       buf,sizeof(buf))))
			return p;
		if ((u_long)akp->start > (u_long)akp->end) {
			sprintf(buf,"out of order timestamp %.30s",val0);
			return buf;
		}

		if (delim != '|')
			break;
	}

	return (delim != '\0') ? tgt : 0;
}


/* Parse a set of parameters for an interface.
 */
char *					/* 0 or error message */
parse_parms(char *line)
{
#define PARS(str) (!strcasecmp(tgt, str))
#define PARSEQ(str) (!strncasecmp(tgt, str"=", sizeof(str)))
#define CKF(g,b) {if (0 != (parm.parm_int_state & ((g) & ~(b)))) break;	\
	parm.parm_int_state |= (b);}
	struct parm parm;
	struct intnet *intnetp;
	struct tgate *tg;
	naddr addr, mask;
	char delim, *val0, *tgt, *val, *p;
	char buf[64];


	/* "subnet=x.y.z.u/mask,metric" must be alone on the line */
	if (!strncasecmp(line, "subnet=", sizeof("subnet=")-1)
	    && *(val = &line[sizeof("subnet=")]) != '\0') {
		intnetp = (struct intnet*)malloc(sizeof(*intnetp));
		intnetp->intnet_metric = 1;
		if ((p = strrchr(val,','))) {
			*p++ = '\0';
			intnetp->intnet_metric = (int)strtol(p,&p,0);
			if (*p != '\0'
			    || intnetp->intnet_metric <= 0
			    || intnetp->intnet_metric >= HOPCNT_INFINITY)
				return line;
		}
		if (!getnet(val, &intnetp->intnet_addr, &intnetp->intnet_mask)
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
	for (;;) {
		tgt = line + strspn(line, " ,\n\r");
		if (*tgt == '\0')
			break;

		line += strcspn(tgt, "= ,\n\r");
		delim = *line;
		if (delim == '=') {
			val0 = ++line;
			if (0 > parse_quote(&line," ,\n\r",&delim,
					    buf,sizeof(buf)))
				return tgt;
		}
		if (delim != '\0')
			*line++ = '\0';

		if (PARSEQ("if")) {
			if (parm.parm_name[0] != '\0'
			    || strlen(buf) > IFNAMSIZ)
				return tgt;
			strcpy(parm.parm_name, buf);

		} else if (PARSEQ("addr")) {
			/* This is a bad idea, because the address based
			 * sets of parameters cannot be checked for
			 * consistency with the interface name parameters.
			 * The parm_net stuff is needed to allow several
			 * -F settings.
			 */
			if (!getnet(val, &addr, &mask)
			    || parm.parm_name[0] != '\0')
				return tgt;
			parm.parm_net = addr;
			parm.parm_mask = mask;
			parm.parm_name[0] = '\n';

		} else if (PARSEQ("passwd")) {
			tgt = get_passwds(tgt, val0, &parm, RIP_AUTH_PW);
			if (tgt)
				return tgt;

		} else if (PARSEQ("md5_passwd")) {
			tgt = get_passwds(tgt, val0, &parm, RIP_AUTH_MD5);
			if (tgt)
				return tgt;

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
				return tgt;
			parm.parm_int_state |= IS_NO_RIPV1_OUT;

		} else if (PARS("ripv2")) {
			if ((parm.parm_int_state & IS_NO_RIPV2_OUT)
			    || (parm.parm_int_state & IS_NO_RIPV2_IN))
				return tgt;
			parm.parm_int_state |= (IS_NO_RIPV1_IN
						| IS_NO_RIPV1_OUT);

		} else if (PARS("no_rip")) {
			CKF(IS_PM_RDISC, IS_NO_RIP);

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

		} else if (PARSEQ("rdisc_pref")) {
			if (parm.parm_rdisc_pref != 0
			    || (parm.parm_rdisc_pref = (int)strtoul(buf, &p,0),
				*p != '\0'))
				return tgt;

		} else if (PARS("pm_rdisc")) {
			if (IS_RIP_OUT_OFF(parm.parm_int_state))
				return tgt;
			parm.parm_int_state |= IS_PM_RDISC;

		} else if (PARSEQ("rdisc_interval")) {
			if (parm.parm_rdisc_int != 0
			    || (parm.parm_rdisc_int = (int)strtoul(buf,&p,0),
				*p != '\0')
			    || parm.parm_rdisc_int < MinMaxAdvertiseInterval
			    || parm.parm_rdisc_int > MaxMaxAdvertiseInterval)
				return tgt;

		} else if (PARSEQ("fake_default")) {
			if (parm.parm_d_metric != 0
			    || IS_RIP_OUT_OFF(parm.parm_int_state)
			    || (parm.parm_d_metric = (int)strtoul(buf,&p,0),
				*p != '\0')
			    || parm.parm_d_metric > HOPCNT_INFINITY-1)
				return tgt;

		} else if (PARSEQ("trust_gateway")) {
			if (!gethost(buf,&addr))
				return tgt;
			tg = (struct tgate *)malloc(sizeof(*tg));
			tg->tgate_next = tgates;
			tg->tgate_addr = addr;
			tgates = tg;
			parm.parm_int_state |= IS_DISTRUST;

		} else {
			return tgt;	/* error */
		}
	}

	return check_parms(&parm);
#undef PARS
#undef PARSEQ
}


/* check for duplicate parameter specifications */
char *					/* 0 or error message */
check_parms(struct parm *new)
{
	struct parm *parmp;

	/* set implicit values
	 */
	if (new->parm_int_state & IS_NO_ADV_IN)
		new->parm_int_state |= IS_NO_SOL_OUT;

	/* compare with existing sets of parameters
	 */
	for (parmp = parms; parmp != 0; parmp = parmp->parm_next) {
		if (strcmp(new->parm_name, parmp->parm_name))
			continue;
		if (!on_net(htonl(parmp->parm_net),
			    new->parm_net, new->parm_mask)
		    && !on_net(htonl(new->parm_net),
			       parmp->parm_net, parmp->parm_mask))
			continue;

		if (parmp->parm_auth.type != RIP_AUTH_NONE
		    && new->parm_auth.type != RIP_AUTH_NONE
		    && bcmp(&parmp->parm_auth, &new->parm_auth,
			    sizeof(parmp->parm_auth))) {
			return "conflicting, duplicate authentication";
		}

		if ((0 != (new->parm_int_state & GROUP_IS_SOL)
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
			&& new->parm_rdisc_int != parmp->parm_rdisc_int)) {
			return ("conflicting, duplicate router discovery"
				" parameters");

		}

		if (new->parm_d_metric != 0
		     && parmp->parm_d_metric != 0
		     && new->parm_d_metric != parmp->parm_d_metric) {
			return ("conflicting, duplicate poor man's router"
				" discovery or fake default metric");
		}
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
       naddr *netp,			/* host byte order */
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
		NTOHL(in.s_addr);
	} else if (!mname && !strcasecmp(name,"default")) {
		in.s_addr = RIP_DEFAULT;
	} else {
		return 0;
	}

	if (!mname) {
		/* we cannot use the interfaces here because we have not
		 * looked at them yet.
		 */
		mask = std_mask(in.s_addr);
		if ((~mask & in.s_addr) != 0)
			mask = HOST_MASK;
	} else {
		mask = (naddr)strtoul(mname, &p, 0);
		if (*p != '\0' || mask > 32)
			return 0;
		mask = HOST_MASK << (32-mask);
	}

	/* must have mask of 0 with default */
	if (mask != 0 && in.s_addr == RIP_DEFAULT)
		return 0;
	/* no host bits allowed in a network number */
	if ((~mask & in.s_addr) != 0)
		return 0;
	/* require non-zero network number */
	if ((mask & in.s_addr) == 0 && in.s_addr != RIP_DEFAULT)
		return 0;
	if (in.s_addr>>24 == 0 && in.s_addr != RIP_DEFAULT)
		return 0;
	if (in.s_addr>>24 == 0xff)
		return 0;

	*netp = in.s_addr;
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
		/* get a good number, but check that it it makes some
		 * sense.
		 */
		if (ntohl(in.s_addr)>>24 == 0
		    || ntohl(in.s_addr)>>24 == 0xff)
			return 0;
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
