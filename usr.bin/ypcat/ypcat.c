/*
 * Copyright (c) 1992/3 Theo de Raadt <deraadt@fsa.ca>
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

struct ypalias {
	char *alias, *name;
} ypaliases[] = {
	{ "passwd", "passwd.byname" },
	{ "master.passwd", "master.passwd.byname" },
	{ "group", "group.byname" },
	{ "networks", "networks.byaddr" },
	{ "hosts", "hosts.byaddr" },
	{ "protocols", "protocols.bynumber" },
	{ "services", "services.byname" },
	{ "aliases", "mail.aliases" },
	{ "ethers", "ethers.byname" },
};

int key;

static void
usage()
{
	fprintf(stderr, "%s\n%s\n",
		"usage: ypcat [-k] [-d domainname] [-t] mapname",
		"       ypcat -x");
	exit(1);
}

int
printit(instatus, inkey, inkeylen, inval, invallen, indata)
int instatus;
char *inkey;
int inkeylen;
char *inval;
int invallen;
char *indata;
{
	if(instatus != YP_TRUE)
		return instatus;
	if(key)
		printf("%*.*s ", inkeylen, inkeylen, inkey);
	printf("%*.*s\n", invallen, invallen, inval);
	return 0;
}

int
main(argc, argv)
char **argv;
{
	char *domainname = NULL;
	struct ypall_callback ypcb;
	char *inmap;
	int notrans;
	int c, r, i;

	notrans = key = 0;

	while( (c=getopt(argc, argv, "xd:kt")) != -1)
		switch(c) {
		case 'x':
			for(i=0; i<sizeof ypaliases/sizeof ypaliases[0]; i++)
				printf("Use \"%s\" for \"%s\"\n",
					ypaliases[i].alias,
					ypaliases[i].name);
			exit(0);
		case 'd':
			domainname = optarg;
			break;
		case 't':
			notrans++;
			break;
		case 'k':
			key++;
			break;
		default:
			usage();
		}

	if(optind + 1 != argc )
		usage();

	if (!domainname)
		yp_get_default_domain(&domainname);

	inmap = argv[optind];
	for(i=0; (!notrans) && i<sizeof ypaliases/sizeof ypaliases[0]; i++)
		if( strcmp(inmap, ypaliases[i].alias) == 0)
			inmap = ypaliases[i].name;
	ypcb.foreach = printit;
	ypcb.data = NULL;

	r = yp_all(domainname, inmap, &ypcb);
	switch(r) {
	case 0:
		break;
	case YPERR_YPBIND:
		errx(1, "not running ypbind");
	default:
		errx(1, "no such map %s. reason: %s", inmap, yperr_string(r));
	}
	exit(0);
}
