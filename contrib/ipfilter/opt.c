/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#ifdef __sgi
# include <sys/ptimers.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifndef	linux
#include <netinet/ip_var.h>
#endif
#include <netinet/tcp.h>
#include <net/if.h>
#include <arpa/inet.h>
#include "ip_compat.h"
#include <netinet/tcpip.h>
#include "ip_fil.h"
#include "ipf.h"

#if !defined(lint)
static const char sccsid[] = "@(#)opt.c	1.8 4/10/96 (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)$Id: opt.c,v 2.2.2.2 2002/02/22 15:32:56 darrenr Exp $";
#endif

extern	int	opts;

struct	ipopt_names	ionames[] ={
	{ IPOPT_NOP,	0x000001,	1,	"nop" },
	{ IPOPT_RR,	0x000002,	7,	"rr" },		/* 1 route */
	{ IPOPT_ZSU,	0x000004,	3,	"zsu" },
	{ IPOPT_MTUP,	0x000008,	3,	"mtup" },
	{ IPOPT_MTUR,	0x000010,	3,	"mtur" },
	{ IPOPT_ENCODE,	0x000020,	3,	"encode" },
	{ IPOPT_TS,	0x000040,	8,	"ts" },		/* 1 TS */
	{ IPOPT_TR,	0x000080,	3,	"tr" },
	{ IPOPT_SECURITY,0x000100,	11,	"sec" },
	{ IPOPT_SECURITY,0x000100,	11,	"sec-class" },
	{ IPOPT_LSRR,	0x000200,	7,	"lsrr" },	/* 1 route */
	{ IPOPT_E_SEC,	0x000400,	3,	"e-sec" },
	{ IPOPT_CIPSO,	0x000800,	3,	"cipso" },
	{ IPOPT_SATID,	0x001000,	4,	"satid" },
	{ IPOPT_SSRR,	0x002000,	7,	"ssrr" },	/* 1 route */
	{ IPOPT_ADDEXT,	0x004000,	3,	"addext" },
	{ IPOPT_VISA,	0x008000,	3,	"visa" },
	{ IPOPT_IMITD,	0x010000,	3,	"imitd" },
	{ IPOPT_EIP,	0x020000,	3,	"eip" },
	{ IPOPT_FINN,	0x040000,	3,	"finn" },
	{ 0, 		0,	0,	(char *)NULL }     /* must be last */
};

struct	ipopt_names	secclass[] = {
	{ IPSO_CLASS_RES4,	0x01,	0, "reserv-4" },
	{ IPSO_CLASS_TOPS,	0x02,	0, "topsecret" },
	{ IPSO_CLASS_SECR,	0x04,	0, "secret" },
	{ IPSO_CLASS_RES3,	0x08,	0, "reserv-3" },
	{ IPSO_CLASS_CONF,	0x10,	0, "confid" },
	{ IPSO_CLASS_UNCL,	0x20,	0, "unclass" },
	{ IPSO_CLASS_RES2,	0x40,	0, "reserv-2" },
	{ IPSO_CLASS_RES1,	0x80,	0, "reserv-1" },
	{ 0, 0, 0, NULL }	/* must be last */
};


static	u_char	seclevel __P((char *));
int addipopt __P((char *, struct ipopt_names *, int, char *));

static u_char seclevel(slevel)
char *slevel;
{
	struct ipopt_names *so;

	for (so = secclass; so->on_name; so++)
		if (!strcasecmp(slevel, so->on_name))
			break;

	if (!so->on_name) {
		fprintf(stderr, "no such security level: %s\n", slevel);
		return 0;
	}
	return (u_char)so->on_value;
}


int addipopt(op, io, len, class)
char *op;
struct ipopt_names *io;
int len;
char *class;
{
	int olen = len;
	struct in_addr ipadr;
	u_short val;
	u_char lvl;
	char *s;

	if ((len + io->on_siz) > 48) {
		fprintf(stderr, "options too long\n");
		return 0;
	}
	len += io->on_siz;
	*op++ = io->on_value;
	if (io->on_siz > 1) {
		s = op;
		*op++ = io->on_siz;
		*op++ = IPOPT_MINOFF;

		if (class) {
			switch (io->on_value)
			{
			case IPOPT_SECURITY :
				lvl = seclevel(class);
				*(op - 1) = lvl;
				break;
			case IPOPT_LSRR :
			case IPOPT_SSRR :
				ipadr.s_addr = inet_addr(class);
				s[IPOPT_OLEN] = IPOPT_MINOFF - 1 + 4;
				bcopy((char *)&ipadr, op, sizeof(ipadr));
				break;
			case IPOPT_SATID :
				val = atoi(class);
				bcopy((char *)&val, op, 2);
				break;
			}
		}

		op += io->on_siz - 3;
		if (len & 3) {
			*op++ = IPOPT_NOP;
			len++;
		}
	}
	if (opts & OPT_DEBUG)
		fprintf(stderr, "bo: %s %d %#x: %d\n",
			io->on_name, io->on_value, io->on_bit, len);
	return len - olen;
}


u_32_t buildopts(cp, op, len)
char *cp, *op;
int len;
{
	struct ipopt_names *io;
	u_32_t msk = 0;
	char *s, *t;
	int inc;

	for (s = strtok(cp, ","); s; s = strtok(NULL, ",")) {
		if ((t = strchr(s, '=')))
			*t++ = '\0';
		for (io = ionames; io->on_name; io++) {
			if (strcasecmp(s, io->on_name) || (msk & io->on_bit))
				continue;
			if ((inc = addipopt(op, io, len, t))) {
				op += inc;
				len += inc;
			}
			msk |= io->on_bit;
			break;
		}
		if (!io->on_name) {
			fprintf(stderr, "unknown IP option name %s\n", s);
			return 0;
		}
	}
	*op++ = IPOPT_EOL;
	len++;
	return len;
}
