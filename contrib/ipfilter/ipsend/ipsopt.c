/*
 * (C)opyright 1995 by Darren Reed.
 *
 * This code may be freely distributed as long as it retains this notice
 * and is not changed in any way.  The author accepts no responsibility
 * for the use of this software.  I hate legaleese, don't you ?
 */
#if !defined(lint) && defined(LIBC_SCCS)
static	char	sccsid[] = "@(#)ipsopt.c	1.2 1/11/96 (C)1995 Darren Reed";
#endif
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include "ip_compat.h"


#ifndef	__P
# ifdef	__STDC__
#  define	__P(x)	x
# else
#  define	__P(x)	()
# endif
#endif


struct ipopt_names {
	int	on_value;
	int	on_bit;
	int	on_siz;
	char	*on_name;
};

struct ipopt_names ionames[] = {
	{ IPOPT_EOL,	0x01,	1, "eol" },
	{ IPOPT_NOP,	0x02,	1, "nop" },
	{ IPOPT_RR,	0x04,	7, "rr" },	/* 1 route */
	{ IPOPT_TS,	0x08,	8, "ts" },	/* 1 TS */
	{ IPOPT_SECURITY, 0x08,	11, "sec-level" },
	{ IPOPT_LSRR,	0x10,	7, "lsrr" },	/* 1 route */
	{ IPOPT_SATID,	0x20,	4, "satid" },
	{ IPOPT_SSRR,	0x40,	7, "ssrr" },	/* 1 route */
	{ 0, 0, 0, NULL }	/* must be last */
};

struct	ipopt_names secnames[] = {
	{ IPOPT_SECUR_UNCLASS,	0x0100,	0, "unclass" },
	{ IPOPT_SECUR_CONFID,	0x0200,	0, "confid" },
	{ IPOPT_SECUR_EFTO,	0x0400,	0, "efto" },
	{ IPOPT_SECUR_MMMM,	0x0800,	0, "mmmm" },
	{ IPOPT_SECUR_RESTR,	0x1000,	0, "restr" },
	{ IPOPT_SECUR_SECRET,	0x2000,	0, "secret" },
	{ IPOPT_SECUR_TOPSECRET, 0x4000,0, "topsecret" },
	{ 0, 0, 0, NULL }	/* must be last */
};


u_short seclevel __P((char *));
u_long optname __P((char *, char *));


u_short seclevel(slevel)
char *slevel;
{
	struct ipopt_names *so;

	for (so = secnames; so->on_name; so++)
		if (!strcasecmp(slevel, so->on_name))
			break;

	if (!so->on_name) {
		fprintf(stderr, "no such security level: %s\n", slevel);
		return 0;
	}
	return so->on_value;
}


u_long optname(cp, op)
char *cp, *op;
{
	struct ipopt_names *io;
	u_short lvl;
	u_long msk = 0;
	char *s, *t;
	int len = 0;

	for (s = strtok(cp, ","); s; s = strtok(NULL, ",")) {
		if ((t = strchr(s, '=')))
			*t++ = '\0';
		for (io = ionames; io->on_name; io++) {
			if (strcasecmp(s, io->on_name) || (msk & io->on_bit))
				continue;
			if ((len + io->on_siz) > 48) {
				fprintf(stderr, "options too long\n");
				return 0;
			}
			len += io->on_siz;
			*op++ = io->on_value;
			if (io->on_siz > 1) {
				*op++ = io->on_siz;
				*op++ = IPOPT_MINOFF;

				if (t && !strcasecmp(s, "sec-level")) {
					lvl = seclevel(t);
					bcopy(&lvl, op, sizeof(lvl));
				}
				op += io->on_siz - 3;
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
