/*
 * Copyright (c) 1992/3 Theo de Raadt <deraadt@fsa.ca>
 * Copyright (c) 1992/3 John Brezak
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
static char rcsid[] = "yppoll.c,v 1.2 1993/08/02 17:57:20 mycroft Exp";
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <time.h>

#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

usage()
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\typpoll [-h host] [-d domainname] mapname\n");
	exit(1);
}

int
main(argc, argv)
char **argv;
{
	char *domainname;
        char *hostname = "localhost";
        char *inmap, *master;
        int order;
	extern char *optarg;
	extern int optind;
	int c, r;

        yp_get_default_domain(&domainname);

	while( (c=getopt(argc, argv, "h:d:?")) != -1)
		switch(c) {
		case 'd':
                        domainname = optarg;
			break;
                case 'h':
                        hostname = optarg;
                        break;
                case '?':
                        usage();
                        /*NOTREACHED*/
		}

	if(optind + 1 != argc )
		usage();

	inmap = argv[optind];

	r = yp_order(domainname, inmap, &order);
        if (r != 0) {
		fprintf(stderr, "No such map %s. Reason: %s\n",
			inmap, yperr_string(r));
                exit(1);
	}
        printf("Map %s has order number %d. %s", inmap, order, ctime((time_t *)&order));
	r = yp_master(domainname, inmap, &master);
        if (r != 0) {
		fprintf(stderr, "No such map %s. Reason: %s\n",
			inmap, yperr_string(r));
                exit(1);
	}
        printf("The master server is %s.\n", master);
        
        exit(0);
}
