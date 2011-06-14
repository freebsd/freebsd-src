/*
 * Copyright (c) 2002-2003 Luigi Rizzo
 * Copyright (c) 1996 Alex Nash, Paul Traina, Poul-Henning Kamp
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 *
 * Idea and grammar partially left from:
 * Copyright (c) 1993 Daniel Boulet
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 * NEW command line interface for IP firewall facility
 *
 * $FreeBSD$
 *
 * In-kernel nat support
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include "ipfw2.h"

#include <ctype.h>
#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#define IPFW_INTERNAL	/* Access to protected structures in ip_fw.h. */

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h> /* def. of struct route */
#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <arpa/inet.h>
#include <alias.h>

static struct _s_x nat_params[] = {
	{ "ip",			TOK_IP },
	{ "if",			TOK_IF },
 	{ "log",		TOK_ALOG },
 	{ "deny_in",		TOK_DENY_INC },
 	{ "same_ports",		TOK_SAME_PORTS },
 	{ "unreg_only",		TOK_UNREG_ONLY },
 	{ "reset",		TOK_RESET_ADDR },
 	{ "reverse",		TOK_ALIAS_REV },
 	{ "proxy_only",		TOK_PROXY_ONLY },
	{ "redirect_addr",	TOK_REDIR_ADDR },
	{ "redirect_port",	TOK_REDIR_PORT },
	{ "redirect_proto",	TOK_REDIR_PROTO },
 	{ NULL, 0 }	/* terminator */
};


/*
 * Search for interface with name "ifn", and fill n accordingly:
 *
 * n->ip	ip address of interface "ifn"
 * n->if_name   copy of interface name "ifn"
 */
static void
set_addr_dynamic(const char *ifn, struct cfg_nat *n)
{
	size_t needed;
	int mib[6];
	char *buf, *lim, *next;
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
	struct sockaddr_dl *sdl;
	struct sockaddr_in *sin;
	int ifIndex, ifMTU;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;
/*
 * Get interface data.
 */
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) == -1)
		err(1, "iflist-sysctl-estimate");
	buf = safe_calloc(1, needed);
	if (sysctl(mib, 6, buf, &needed, NULL, 0) == -1)
		err(1, "iflist-sysctl-get");
	lim = buf + needed;
/*
 * Loop through interfaces until one with
 * given name is found. This is done to
 * find correct interface index for routing
 * message processing.
 */
	ifIndex	= 0;
	next = buf;
	while (next < lim) {
		ifm = (struct if_msghdr *)next;
		next += ifm->ifm_msglen;
		if (ifm->ifm_version != RTM_VERSION) {
			if (co.verbose)
				warnx("routing message version %d "
				    "not understood", ifm->ifm_version);
			continue;
		}
		if (ifm->ifm_type == RTM_IFINFO) {
			sdl = (struct sockaddr_dl *)(ifm + 1);
			if (strlen(ifn) == sdl->sdl_nlen &&
			    strncmp(ifn, sdl->sdl_data, sdl->sdl_nlen) == 0) {
				ifIndex = ifm->ifm_index;
				ifMTU = ifm->ifm_data.ifi_mtu;
				break;
			}
		}
	}
	if (!ifIndex)
		errx(1, "unknown interface name %s", ifn);
/*
 * Get interface address.
 */
	sin = NULL;
	while (next < lim) {
		ifam = (struct ifa_msghdr *)next;
		next += ifam->ifam_msglen;
		if (ifam->ifam_version != RTM_VERSION) {
			if (co.verbose)
				warnx("routing message version %d "
				    "not understood", ifam->ifam_version);
			continue;
		}
		if (ifam->ifam_type != RTM_NEWADDR)
			break;
		if (ifam->ifam_addrs & RTA_IFA) {
			int i;
			char *cp = (char *)(ifam + 1);

			for (i = 1; i < RTA_IFA; i <<= 1) {
				if (ifam->ifam_addrs & i)
					cp += SA_SIZE((struct sockaddr *)cp);
			}
			if (((struct sockaddr *)cp)->sa_family == AF_INET) {
				sin = (struct sockaddr_in *)cp;
				break;
			}
		}
	}
	if (sin == NULL)
		errx(1, "%s: cannot get interface address", ifn);

	n->ip = sin->sin_addr;
	strncpy(n->if_name, ifn, IF_NAMESIZE);

	free(buf);
}

/*
 * XXX - The following functions, macros and definitions come from natd.c:
 * it would be better to move them outside natd.c, in a file
 * (redirect_support.[ch]?) shared by ipfw and natd, but for now i can live
 * with it.
 */

/*
 * Definition of a port range, and macros to deal with values.
 * FORMAT:  HI 16-bits == first port in range, 0 == all ports.
 *	  LO 16-bits == number of ports in range
 * NOTES:   - Port values are not stored in network byte order.
 */

#define port_range u_long

#define GETLOPORT(x)	((x) >> 0x10)
#define GETNUMPORTS(x)	((x) & 0x0000ffff)
#define GETHIPORT(x)	(GETLOPORT((x)) + GETNUMPORTS((x)))

/* Set y to be the low-port value in port_range variable x. */
#define SETLOPORT(x,y)   ((x) = ((x) & 0x0000ffff) | ((y) << 0x10))

/* Set y to be the number of ports in port_range variable x. */
#define SETNUMPORTS(x,y) ((x) = ((x) & 0xffff0000) | (y))

static void
StrToAddr (const char* str, struct in_addr* addr)
{
	struct hostent* hp;

	if (inet_aton (str, addr))
		return;

	hp = gethostbyname (str);
	if (!hp)
		errx (1, "unknown host %s", str);

	memcpy (addr, hp->h_addr, sizeof (struct in_addr));
}

static int
StrToPortRange (const char* str, const char* proto, port_range *portRange)
{
	char*	   sep;
	struct servent*	sp;
	char*		end;
	u_short	 loPort;
	u_short	 hiPort;

	/* First see if this is a service, return corresponding port if so. */
	sp = getservbyname (str,proto);
	if (sp) {
		SETLOPORT(*portRange, ntohs(sp->s_port));
		SETNUMPORTS(*portRange, 1);
		return 0;
	}

	/* Not a service, see if it's a single port or port range. */
	sep = strchr (str, '-');
	if (sep == NULL) {
		SETLOPORT(*portRange, strtol(str, &end, 10));
		if (end != str) {
			/* Single port. */
			SETNUMPORTS(*portRange, 1);
			return 0;
		}

		/* Error in port range field. */
		errx (EX_DATAERR, "%s/%s: unknown service", str, proto);
	}

	/* Port range, get the values and sanity check. */
	sscanf (str, "%hu-%hu", &loPort, &hiPort);
	SETLOPORT(*portRange, loPort);
	SETNUMPORTS(*portRange, 0);	/* Error by default */
	if (loPort <= hiPort)
		SETNUMPORTS(*portRange, hiPort - loPort + 1);

	if (GETNUMPORTS(*portRange) == 0)
		errx (EX_DATAERR, "invalid port range %s", str);

	return 0;
}

static int
StrToProto (const char* str)
{
	if (!strcmp (str, "tcp"))
		return IPPROTO_TCP;

	if (!strcmp (str, "udp"))
		return IPPROTO_UDP;

	if (!strcmp (str, "sctp"))
		return IPPROTO_SCTP;
	errx (EX_DATAERR, "unknown protocol %s. Expected sctp, tcp or udp", str);
}

static int
StrToAddrAndPortRange (const char* str, struct in_addr* addr, char* proto,
			port_range *portRange)
{
	char*	ptr;

	ptr = strchr (str, ':');
	if (!ptr)
		errx (EX_DATAERR, "%s is missing port number", str);

	*ptr = '\0';
	++ptr;

	StrToAddr (str, addr);
	return StrToPortRange (ptr, proto, portRange);
}

/* End of stuff taken from natd.c. */

/*
 * The next 3 functions add support for the addr, port and proto redirect and
 * their logic is loosely based on SetupAddressRedirect(), SetupPortRedirect()
 * and SetupProtoRedirect() from natd.c.
 *
 * Every setup_* function fills at least one redirect entry
 * (struct cfg_redir) and zero or more server pool entry (struct cfg_spool)
 * in buf.
 *
 * The format of data in buf is:
 *
 *     cfg_nat    cfg_redir    cfg_spool    ......  cfg_spool
 *
 *    -------------------------------------        ------------
 *   |          | .....X ... |          |         |           |  .....
 *    ------------------------------------- ...... ------------
 *                     ^
 *                spool_cnt       n=0       ......   n=(X-1)
 *
 * len points to the amount of available space in buf
 * space counts the memory consumed by every function
 *
 * XXX - Every function get all the argv params so it
 * has to check, in optional parameters, that the next
 * args is a valid option for the redir entry and not
 * another token. Only redir_port and redir_proto are
 * affected by this.
 */

static int
estimate_redir_addr(int *ac, char ***av)
{
	size_t space = sizeof(struct cfg_redir);
	char *sep;

	if ((sep = strtok(**av, ",")) != NULL) {
		space += sizeof(struct cfg_spool);
		while ((sep = strtok(NULL, ",")) != NULL)
			space += sizeof(struct cfg_spool);
	}

	return (space);
}

static int
setup_redir_addr(char *buf, int *ac, char ***av)
{
	struct cfg_redir *r;
	char *sep;
	size_t space;

	r = (struct cfg_redir *)buf;
	r->mode = REDIR_ADDR;
	/* Skip cfg_redir at beginning of buf. */
	buf = &buf[sizeof(struct cfg_redir)];
	space = sizeof(struct cfg_redir);

	/* Extract local address. */
	if ((sep = strtok(**av, ",")) != NULL) {
		struct cfg_spool *spool;

		/* Setup LSNAT server pool. */
		r->laddr.s_addr = INADDR_NONE;
		while (sep != NULL) {
			spool = (struct cfg_spool *)buf;
			space += sizeof(struct cfg_spool);
			StrToAddr(sep, &spool->addr);
			spool->port = ~0;
			r->spool_cnt++;
			/* Point to the next possible cfg_spool. */
			buf = &buf[sizeof(struct cfg_spool)];
			sep = strtok(NULL, ",");
		}
	} else
		StrToAddr(**av, &r->laddr);
	(*av)++; (*ac)--;

	/* Extract public address. */
	StrToAddr(**av, &r->paddr);
	(*av)++; (*ac)--;

	return (space);
}

static int
estimate_redir_port(int *ac, char ***av)
{
	size_t space = sizeof(struct cfg_redir);
	char *sep;

	if ((sep = strtok(**av, ",")) != NULL) {
		space += sizeof(struct cfg_spool);
		while ((sep = strtok(NULL, ",")) != NULL)
			space += sizeof(struct cfg_spool);
	}

	return (space);
}

static int
setup_redir_port(char *buf, int *ac, char ***av)
{
	struct cfg_redir *r;
	char *sep, *protoName, *lsnat = NULL;
	size_t space;
	u_short numLocalPorts;
	port_range portRange;

	numLocalPorts = 0;

	r = (struct cfg_redir *)buf;
	r->mode = REDIR_PORT;
	/* Skip cfg_redir at beginning of buf. */
	buf = &buf[sizeof(struct cfg_redir)];
	space = sizeof(struct cfg_redir);

	/*
	 * Extract protocol.
	 */
	r->proto = StrToProto(**av);
	protoName = **av;
	(*av)++; (*ac)--;

	/*
	 * Extract local address.
	 */
	if ((sep = strchr(**av, ',')) != NULL) {
		r->laddr.s_addr = INADDR_NONE;
		r->lport = ~0;
		numLocalPorts = 1;
		lsnat = **av;
	} else {
		/*
		 * The sctp nat does not allow the port numbers to be mapped to
		 * new port numbers. Therefore, no ports are to be specified
		 * in the target port field.
		 */
		if (r->proto == IPPROTO_SCTP) {
			if (strchr(**av, ':'))
				errx(EX_DATAERR, "redirect_port:"
				    "port numbers do not change in sctp, so do "
				    "not specify them as part of the target");
			else
				StrToAddr(**av, &r->laddr);
		} else {
			if (StrToAddrAndPortRange(**av, &r->laddr, protoName,
			    &portRange) != 0)
				errx(EX_DATAERR, "redirect_port: "
				    "invalid local port range");

			r->lport = GETLOPORT(portRange);
			numLocalPorts = GETNUMPORTS(portRange);
		}
	}
	(*av)++; (*ac)--;

	/*
	 * Extract public port and optionally address.
	 */
	if ((sep = strchr(**av, ':')) != NULL) {
		if (StrToAddrAndPortRange(**av, &r->paddr, protoName,
		    &portRange) != 0)
			errx(EX_DATAERR, "redirect_port: "
			    "invalid public port range");
	} else {
		r->paddr.s_addr = INADDR_ANY;
		if (StrToPortRange(**av, protoName, &portRange) != 0)
			errx(EX_DATAERR, "redirect_port: "
			    "invalid public port range");
	}

	r->pport = GETLOPORT(portRange);
	if (r->proto == IPPROTO_SCTP) { /* so the logic below still works */
		numLocalPorts = GETNUMPORTS(portRange);
		r->lport = r->pport;
	}
	r->pport_cnt = GETNUMPORTS(portRange);
	(*av)++; (*ac)--;

	/*
	 * Extract remote address and optionally port.
	 */
	/*
	 * NB: isalpha(**av) => we've to check that next parameter is really an
	 * option for this redirect entry, else stop here processing arg[cv].
	 */
	if (*ac != 0 && !isalpha(***av)) {
		if ((sep = strchr(**av, ':')) != NULL) {
			if (StrToAddrAndPortRange(**av, &r->raddr, protoName,
			    &portRange) != 0)
				errx(EX_DATAERR, "redirect_port: "
				    "invalid remote port range");
		} else {
			SETLOPORT(portRange, 0);
			SETNUMPORTS(portRange, 1);
			StrToAddr(**av, &r->raddr);
		}
		(*av)++; (*ac)--;
	} else {
		SETLOPORT(portRange, 0);
		SETNUMPORTS(portRange, 1);
		r->raddr.s_addr = INADDR_ANY;
	}
	r->rport = GETLOPORT(portRange);
	r->rport_cnt = GETNUMPORTS(portRange);

	/*
	 * Make sure port ranges match up, then add the redirect ports.
	 */
	if (numLocalPorts != r->pport_cnt)
		errx(EX_DATAERR, "redirect_port: "
		    "port ranges must be equal in size");

	/* Remote port range is allowed to be '0' which means all ports. */
	if (r->rport_cnt != numLocalPorts &&
	    (r->rport_cnt != 1 || r->rport != 0))
		errx(EX_DATAERR, "redirect_port: remote port must"
		    "be 0 or equal to local port range in size");

	/* Setup LSNAT server pool. */
	if (lsnat != NULL) {
		struct cfg_spool *spool;

		sep = strtok(lsnat, ",");
		while (sep != NULL) {
			spool = (struct cfg_spool *)buf;
			space += sizeof(struct cfg_spool);
			/*
			 * The sctp nat does not allow the port numbers to
			 * be mapped to new port numbers. Therefore, no ports
			 * are to be specified in the target port field.
			 */
			if (r->proto == IPPROTO_SCTP) {
				if (strchr (sep, ':')) {
					errx(EX_DATAERR, "redirect_port:"
					    "port numbers do not change in "
					    "sctp, so do not specify them as "
					    "part of the target");
				} else {
					StrToAddr(sep, &spool->addr);
					spool->port = r->pport;
				}
			} else {
				if (StrToAddrAndPortRange(sep, &spool->addr,
					protoName, &portRange) != 0)
					errx(EX_DATAERR, "redirect_port:"
					    "invalid local port range");
				if (GETNUMPORTS(portRange) != 1)
					errx(EX_DATAERR, "redirect_port: "
					    "local port must be single in "
					    "this context");
				spool->port = GETLOPORT(portRange);
			}
			r->spool_cnt++;
			/* Point to the next possible cfg_spool. */
			buf = &buf[sizeof(struct cfg_spool)];
			sep = strtok(NULL, ",");
		}
	}

	return (space);
}

static int
setup_redir_proto(char *buf, int *ac, char ***av)
{
	struct cfg_redir *r;
	struct protoent *protoent;
	size_t space;

	r = (struct cfg_redir *)buf;
	r->mode = REDIR_PROTO;
	/* Skip cfg_redir at beginning of buf. */
	buf = &buf[sizeof(struct cfg_redir)];
	space = sizeof(struct cfg_redir);

	/*
	 * Extract protocol.
	 */
	protoent = getprotobyname(**av);
	if (protoent == NULL)
		errx(EX_DATAERR, "redirect_proto: unknown protocol %s", **av);
	else
		r->proto = protoent->p_proto;

	(*av)++; (*ac)--;

	/*
	 * Extract local address.
	 */
	StrToAddr(**av, &r->laddr);

	(*av)++; (*ac)--;

	/*
	 * Extract optional public address.
	 */
	if (*ac == 0) {
		r->paddr.s_addr = INADDR_ANY;
		r->raddr.s_addr = INADDR_ANY;
	} else {
		/* see above in setup_redir_port() */
		if (!isalpha(***av)) {
			StrToAddr(**av, &r->paddr);
			(*av)++; (*ac)--;

			/*
			 * Extract optional remote address.
			 */
			/* see above in setup_redir_port() */
			if (*ac != 0 && !isalpha(***av)) {
				StrToAddr(**av, &r->raddr);
				(*av)++; (*ac)--;
			}
		}
	}

	return (space);
}

static void
print_nat_config(unsigned char *buf)
{
	struct cfg_nat *n;
	int i, cnt, flag, off;
	struct cfg_redir *t;
	struct cfg_spool *s;
	struct protoent *p;

	n = (struct cfg_nat *)buf;
	flag = 1;
	off  = sizeof(*n);
	printf("ipfw nat %u config", n->id);
	if (strlen(n->if_name) != 0)
		printf(" if %s", n->if_name);
	else if (n->ip.s_addr != 0)
		printf(" ip %s", inet_ntoa(n->ip));
	while (n->mode != 0) {
		if (n->mode & PKT_ALIAS_LOG) {
			printf(" log");
			n->mode &= ~PKT_ALIAS_LOG;
		} else if (n->mode & PKT_ALIAS_DENY_INCOMING) {
			printf(" deny_in");
			n->mode &= ~PKT_ALIAS_DENY_INCOMING;
		} else if (n->mode & PKT_ALIAS_SAME_PORTS) {
			printf(" same_ports");
			n->mode &= ~PKT_ALIAS_SAME_PORTS;
		} else if (n->mode & PKT_ALIAS_UNREGISTERED_ONLY) {
			printf(" unreg_only");
			n->mode &= ~PKT_ALIAS_UNREGISTERED_ONLY;
		} else if (n->mode & PKT_ALIAS_RESET_ON_ADDR_CHANGE) {
			printf(" reset");
			n->mode &= ~PKT_ALIAS_RESET_ON_ADDR_CHANGE;
		} else if (n->mode & PKT_ALIAS_REVERSE) {
			printf(" reverse");
			n->mode &= ~PKT_ALIAS_REVERSE;
		} else if (n->mode & PKT_ALIAS_PROXY_ONLY) {
			printf(" proxy_only");
			n->mode &= ~PKT_ALIAS_PROXY_ONLY;
		}
	}
	/* Print all the redirect's data configuration. */
	for (cnt = 0; cnt < n->redir_cnt; cnt++) {
		t = (struct cfg_redir *)&buf[off];
		off += SOF_REDIR;
		switch (t->mode) {
		case REDIR_ADDR:
			printf(" redirect_addr");
			if (t->spool_cnt == 0)
				printf(" %s", inet_ntoa(t->laddr));
			else
				for (i = 0; i < t->spool_cnt; i++) {
					s = (struct cfg_spool *)&buf[off];
					if (i)
						printf(",");
					else
						printf(" ");
					printf("%s", inet_ntoa(s->addr));
					off += SOF_SPOOL;
				}
			printf(" %s", inet_ntoa(t->paddr));
			break;
		case REDIR_PORT:
			p = getprotobynumber(t->proto);
			printf(" redirect_port %s ", p->p_name);
			if (!t->spool_cnt) {
				printf("%s:%u", inet_ntoa(t->laddr), t->lport);
				if (t->pport_cnt > 1)
					printf("-%u", t->lport +
					    t->pport_cnt - 1);
			} else
				for (i=0; i < t->spool_cnt; i++) {
					s = (struct cfg_spool *)&buf[off];
					if (i)
						printf(",");
					printf("%s:%u", inet_ntoa(s->addr),
					    s->port);
					off += SOF_SPOOL;
				}

			printf(" ");
			if (t->paddr.s_addr)
				printf("%s:", inet_ntoa(t->paddr));
			printf("%u", t->pport);
			if (!t->spool_cnt && t->pport_cnt > 1)
				printf("-%u", t->pport + t->pport_cnt - 1);

			if (t->raddr.s_addr) {
				printf(" %s", inet_ntoa(t->raddr));
				if (t->rport) {
					printf(":%u", t->rport);
					if (!t->spool_cnt && t->rport_cnt > 1)
						printf("-%u", t->rport +
						    t->rport_cnt - 1);
				}
			}
			break;
		case REDIR_PROTO:
			p = getprotobynumber(t->proto);
			printf(" redirect_proto %s %s", p->p_name,
			    inet_ntoa(t->laddr));
			if (t->paddr.s_addr != 0) {
				printf(" %s", inet_ntoa(t->paddr));
				if (t->raddr.s_addr)
					printf(" %s", inet_ntoa(t->raddr));
			}
			break;
		default:
			errx(EX_DATAERR, "unknown redir mode");
			break;
		}
	}
	printf("\n");
}

void
ipfw_config_nat(int ac, char **av)
{
	struct cfg_nat *n;		/* Nat instance configuration. */
	int i, off, tok, ac1;
	char *id, *buf, **av1, *end;
	size_t len;

	av++; ac--;
	/* Nat id. */
	if (ac == 0)
		errx(EX_DATAERR, "missing nat id");
	id = *av;
	i = (int)strtol(id, &end, 0);
	if (i <= 0 || *end != '\0')
		errx(EX_DATAERR, "illegal nat id: %s", id);
	av++; ac--;
	if (ac == 0)
		errx(EX_DATAERR, "missing option");

	len = sizeof(struct cfg_nat);
	ac1 = ac;
	av1 = av;
	while (ac1 > 0) {
		tok = match_token(nat_params, *av1);
		ac1--; av1++;
		switch (tok) {
		case TOK_IP:
		case TOK_IF:
			ac1--; av1++;
			break;	    
		case TOK_ALOG:
		case TOK_DENY_INC:
		case TOK_SAME_PORTS:
		case TOK_UNREG_ONLY:
		case TOK_RESET_ADDR:
		case TOK_ALIAS_REV:
		case TOK_PROXY_ONLY:
			break;
		case TOK_REDIR_ADDR:
			if (ac1 < 2)
				errx(EX_DATAERR, "redirect_addr: "
				    "not enough arguments");
			len += estimate_redir_addr(&ac1, &av1);
			av1 += 2; ac1 -= 2;
			break;
		case TOK_REDIR_PORT:
			if (ac1 < 3)
				errx(EX_DATAERR, "redirect_port: "
				    "not enough arguments");
			av1++; ac1--;
			len += estimate_redir_port(&ac1, &av1);
			av1 += 2; ac1 -= 2;
			break;
		case TOK_REDIR_PROTO:
			if (ac1 < 2)
				errx(EX_DATAERR, "redirect_proto: "
				    "not enough arguments");
			len += sizeof(struct cfg_redir);
			av1 += 2; ac1 -= 2;
			break;
		default:
			errx(EX_DATAERR, "unrecognised option ``%s''", av1[-1]);
		}
	}

	if ((buf = malloc(len)) == NULL)
		errx(EX_OSERR, "malloc failed");

	/* Offset in buf: save space for n at the beginning. */
	off = sizeof(*n);
	memset(buf, 0, len);
	n = (struct cfg_nat *)buf;
	n->id = i;

	while (ac > 0) {
		tok = match_token(nat_params, *av);
		ac--; av++;
		switch (tok) {
		case TOK_IP:
			if (ac == 0)
				errx(EX_DATAERR, "missing option");
			if (!inet_aton(av[0], &(n->ip)))
				errx(EX_DATAERR, "bad ip address ``%s''",
				    av[0]);
			ac--; av++;
			break;
		case TOK_IF:
			if (ac == 0)
				errx(EX_DATAERR, "missing option");
			set_addr_dynamic(av[0], n);
			ac--; av++;
			break;
		case TOK_ALOG:
			n->mode |= PKT_ALIAS_LOG;
			break;
		case TOK_DENY_INC:
			n->mode |= PKT_ALIAS_DENY_INCOMING;
			break;
		case TOK_SAME_PORTS:
			n->mode |= PKT_ALIAS_SAME_PORTS;
			break;
		case TOK_UNREG_ONLY:
			n->mode |= PKT_ALIAS_UNREGISTERED_ONLY;
			break;
		case TOK_RESET_ADDR:
			n->mode |= PKT_ALIAS_RESET_ON_ADDR_CHANGE;
			break;
		case TOK_ALIAS_REV:
			n->mode |= PKT_ALIAS_REVERSE;
			break;
		case TOK_PROXY_ONLY:
			n->mode |= PKT_ALIAS_PROXY_ONLY;
			break;
			/*
			 * All the setup_redir_* functions work directly in
			 * the final buffer, see above for details.
			 */
		case TOK_REDIR_ADDR:
		case TOK_REDIR_PORT:
		case TOK_REDIR_PROTO:
			switch (tok) {
			case TOK_REDIR_ADDR:
				i = setup_redir_addr(&buf[off], &ac, &av);
				break;
			case TOK_REDIR_PORT:
				i = setup_redir_port(&buf[off], &ac, &av);
				break;
			case TOK_REDIR_PROTO:
				i = setup_redir_proto(&buf[off], &ac, &av);
				break;
			}
			n->redir_cnt++;
			off += i;
			break;
		}
	}

	i = do_cmd(IP_FW_NAT_CFG, buf, off);
	if (i)
		err(1, "setsockopt(%s)", "IP_FW_NAT_CFG");

	if (!co.do_quiet) {
		/* After every modification, we show the resultant rule. */
		int _ac = 3;
		const char *_av[] = {"show", "config", id};
		ipfw_show_nat(_ac, (char **)(void *)_av);
	}
}


void
ipfw_show_nat(int ac, char **av)
{
	struct cfg_nat *n;
	struct cfg_redir *e;
	int cmd, i, nbytes, do_cfg, do_rule, frule, lrule, nalloc, size;
	int nat_cnt, redir_cnt, r;
	uint8_t *data, *p;
	char *endptr;

	do_rule = 0;
	nalloc = 1024;
	size = 0;
	data = NULL;
	frule = 0;
	lrule = IPFW_DEFAULT_RULE; /* max ipfw rule number */
	ac--; av++;

	if (co.test_only)
		return;

	/* Parse parameters. */
	for (cmd = IP_FW_NAT_GET_LOG, do_cfg = 0; ac != 0; ac--, av++) {
		if (!strncmp(av[0], "config", strlen(av[0]))) {
			cmd = IP_FW_NAT_GET_CONFIG, do_cfg = 1;
			continue;
		}
		/* Convert command line rule #. */
		frule = lrule = strtoul(av[0], &endptr, 10);
		if (*endptr == '-')
			lrule = strtoul(endptr+1, &endptr, 10);
		if (lrule == 0)
			err(EX_USAGE, "invalid rule number: %s", av[0]);
		do_rule = 1;
	}

	nbytes = nalloc;
	while (nbytes >= nalloc) {
		nalloc = nalloc * 2;
		nbytes = nalloc;
		data = safe_realloc(data, nbytes);
		if (do_cmd(cmd, data, (uintptr_t)&nbytes) < 0)
			err(EX_OSERR, "getsockopt(IP_FW_GET_%s)",
			    (cmd == IP_FW_NAT_GET_LOG) ? "LOG" : "CONFIG");
	}
	if (nbytes == 0)
		exit(0);
	if (do_cfg) {
		nat_cnt = *((int *)data);
		for (i = sizeof(nat_cnt); nat_cnt; nat_cnt--) {
			n = (struct cfg_nat *)&data[i];
			if (frule <= n->id && lrule >= n->id)
				print_nat_config(&data[i]);
			i += sizeof(struct cfg_nat);
			for (redir_cnt = 0; redir_cnt < n->redir_cnt; redir_cnt++) {
				e = (struct cfg_redir *)&data[i];
				i += sizeof(struct cfg_redir) + e->spool_cnt *
				    sizeof(struct cfg_spool);
			}
		}
	} else {
		for (i = 0; 1; i += LIBALIAS_BUF_SIZE + sizeof(int)) {
			p = &data[i];
			if (p == data + nbytes)
				break;
			bcopy(p, &r, sizeof(int));
			if (do_rule) {
				if (!(frule <= r && lrule >= r))
					continue;
			}
			printf("nat %u: %s\n", r, p+sizeof(int));
		}
	}
}
