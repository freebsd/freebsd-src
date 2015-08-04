/*	$OpenBSD: ypcat.c,v 1.16 2015/02/08 23:40:35 deraadt Exp $ */

/*
 * Copyright (c) 1992, 1993, 1996 Theo de Raadt <deraadt@theos.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <err.h>

#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp.h>
#include <rpcsvc/ypclnt.h>

void	usage(void);
int	printit(u_long, char *, int, char *, int, void *);

static const struct ypalias {
	char *alias, *name;
} ypaliases[] = {
	{ "passwd", "passwd.byname" },
	{ "master.passwd", "master.passwd.byname" },
	{ "shadow", "shadow.byname" },
	{ "group", "group.byname" },
	{ "networks", "networks.byaddr" },
	{ "hosts", "hosts.byaddr" },
	{ "protocols", "protocols.bynumber" },
	{ "services", "services.byname" },
	{ "aliases", "mail.aliases" },
	{ "ethers", "ethers.byname" },
};

static int key;

void
usage(void)
{
	fprintf(stderr,
	    "usage: ypcat [-kt] [-d domainname] mapname\n"
	    "       ypcat -x\n");
	exit(1);
}

int
printit(u_long instatus, char *inkey, int inkeylen, char *inval, int invallen,
    void *indata)
{
	if (instatus != YP_TRUE)
		return (instatus);
	if (key)
		printf("%*.*s ", inkeylen, inkeylen, inkey);
	printf("%*.*s\n", invallen, invallen, inval);
	return (0);
}

int
main(int argc, char *argv[])
{
	char *domain = NULL, *inmap;
	struct ypall_callback ypcb;
	extern char *optarg;
	extern int optind;
	int notrans, c, r;
	u_int i;

	notrans = key = 0;
	while ((c = getopt(argc, argv, "xd:kt")) != -1)
		switch (c) {
		case 'x':
			for (i=0; i<sizeof ypaliases/sizeof ypaliases[0]; i++)
				printf("Use \"%s\" for \"%s\"\n",
				    ypaliases[i].alias, ypaliases[i].name);
			exit(0);
		case 'd':
			domain = optarg;
			break;
		case 't':
			notrans = 1;
			break;
		case 'k':
			key = 1;
			break;
		default:
			usage();
		}

	if (optind + 1 != argc)
		usage();

	if (!domain)
		yp_get_default_domain(&domain);

	inmap = argv[optind];
	if (!notrans) {
		for (i=0; i<sizeof ypaliases/sizeof ypaliases[0]; i++)
			if (strcmp(inmap, ypaliases[i].alias) == 0)
				inmap = ypaliases[i].name;
	}
	ypcb.foreach = printit;
	ypcb.data = NULL;

	r = yp_all(domain, inmap, &ypcb);
	switch (r) {
	case 0:
		break;
	case YPERR_YPBIND:
		errx(1, "ypcat: not running ypbind\n");
		exit(1);
	default:
		errx(1, "No such map %s. Reason: %s\n",
		    inmap, yperr_string(r));
		exit(1);
	}
	exit(0);
}
