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

#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp.h>
struct dom_binding{};
#include <rpcsvc/ypclnt.h>
#include <arpa/inet.h>

extern bool_t xdr_domainname();

static void
usage()
{
	fprintf(stderr, "usage: ypset [-h host] [-d domain] server\n");
	exit(1);
}

int
bind_tohost(sin, dom, server)
struct sockaddr_in *sin;
char *dom, *server;
{
	struct ypbind_setdom ypsd;
	struct timeval tv;
	struct hostent *hp;
	CLIENT *client;
	int sock, port;
	int r;
	unsigned long server_addr;

	if ((port = htons(getrpcport(server, YPPROG, YPPROC_NULL, IPPROTO_UDP))) == 0)
		errx(1, "%s not running ypserv", server);

	bzero(&ypsd, sizeof ypsd);

	if ((hp = gethostbyname (server)) != NULL) {
		/* is this the most compatible way?? */
		bcopy (hp->h_addr_list[0],
		       (u_long *)&ypsd.ypsetdom_binding.ypbind_binding_addr,
		       sizeof (unsigned long));
	} else if ((long)(server_addr = inet_addr (server)) == -1) {
		errx(1, "can't find address for %s", server);
	} else
		bcopy (&server_addr,
		       (u_long *)&ypsd.ypsetdom_binding.ypbind_binding_addr,
		       sizeof (server_addr));

/*	strncpy(ypsd.ypsetdom_domain, dom, sizeof ypsd.ypsetdom_domain); */
	ypsd.ypsetdom_domain = dom;
	*(u_long *)&ypsd.ypsetdom_binding.ypbind_binding_port = port;
	ypsd.ypsetdom_vers = YPVERS;

	tv.tv_sec = 15;
	tv.tv_usec = 0;
	sock = RPC_ANYSOCK;
	client = clntudp_create(sin, YPBINDPROG, YPBINDVERS, tv, &sock);
	if (client == NULL) {
		warnx("can't yp_bind, reason: %s", yperr_string(YPERR_YPBIND));
		return (YPERR_YPBIND);
	}
	client->cl_auth = authunix_create_default();

	r = clnt_call(client, YPBINDPROC_SETDOM,
		xdr_ypbind_setdom, &ypsd, xdr_void, NULL, tv);
	if (r) {
		warnx("sorry, cannot ypset for domain %s on host", dom);
		clnt_destroy(client);
		return (YPERR_YPBIND);
	}
	clnt_destroy(client);
	return (0);
}

int
main(argc, argv)
char **argv;
{
	struct sockaddr_in sin;
	struct hostent *hent;
	extern char *optarg;
	extern int optind;
	char *domainname;
	int c;

	yp_get_default_domain(&domainname);

	bzero(&sin, sizeof sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(0x7f000001);

	while ((c = getopt(argc, argv, "h:d:")) != -1)
		switch (c) {
		case 'd':
			domainname = optarg;
			break;
		case 'h':
			if ((sin.sin_addr.s_addr = inet_addr(optarg)) == -1) {
				hent = gethostbyname(optarg);
				if (hent == NULL)
					errx(1, "host %s unknown", optarg);
				bcopy(hent->h_addr_list[0], &sin.sin_addr,
					sizeof sin.sin_addr);
			}
			break;
		default:
			usage();
		}

	if (optind + 1 != argc)
		usage();

	if (bind_tohost(&sin, domainname, argv[optind]))
		exit(1);
	exit(0);
}
