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

#ifndef LINT
static char rcsid[] = "ypmatch.c,v 1.2 1993/05/16 02:49:03 deraadt Exp";
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <ctype.h>

#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

struct ypalias {
	char *alias, *name;
} ypaliases[] = {
	{ "passwd", "passwd.byname" },
	{ "group", "group.byname" },
	{ "networks", "networks.byaddr" },
	{ "hosts", "hosts.byaddr" },
	{ "protocols", "protocols.bynumber" },
	{ "services", "services.byname" },
	{ "aliases", "mail.aliases" },
	{ "ethers", "ethers.byname" },
};

usage()
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\typmatch [-d domain] [-t] [-k] key [key ...] mname\n");
	fprintf(stderr, "\typmatch -x\n");
	fprintf(stderr, "where\n");
	fprintf(stderr, "\tmname may be either a mapname or a nickname for a map\n");
	fprintf(stderr, "\t-t inhibits map nickname translation\n");
	fprintf(stderr, "\t-k prints keys as well as values.\n");
	fprintf(stderr, "\t-x dumps the map nickname translation table.\n");
	exit(1);
}

int
main(argc, argv)
char **argv;
{
	char *domainname;
	char *inkey, *inmap, *outbuf;
	extern char *optarg;
	extern int optind;
	int outbuflen, key, notrans;
	int c, r, i;

	notrans = key = 0;
	yp_get_default_domain(&domainname);

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

	if( (argc-optind) < 2 )
		usage();

	inmap = argv[argc-1];
	for(i=0; (!notrans) && i<sizeof ypaliases/sizeof ypaliases[0]; i++)
		if( strcmp(inmap, ypaliases[i].alias) == 0)
			inmap = ypaliases[i].name;
	for(; optind < argc-1; optind++) {
		inkey = argv[optind];

		r = yp_match(domainname, inmap, inkey,
			strlen(inkey), &outbuf, &outbuflen);
		switch(r) {
		case 0:
			if(key)
				printf("%s ", inkey);
			printf("%*.*s\n", outbuflen, outbuflen, outbuf);
			break;
		case YPERR_YPBIND:
			fprintf(stderr, "yp_match: not running ypbind\n");
			exit(1);
		default:
			fprintf(stderr, "Can't match key %s in map %s. Reason: %s\n",
				inkey, inmap, yperr_string(r));
			break;
		}
	}
	exit(0);
}
