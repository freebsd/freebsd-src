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

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <rpcsvc/ypclnt.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp.h>
struct dom_binding{};

#define ERR_USAGE	1	/* bad arguments - display 'usage' message */
#define ERR_NOSUCHHOST	2	/* no such host */
#define ERR_NOBINDING	3	/* error from ypbind -- domain not bound */
#define ERR_NOYPBIND	4	/* ypbind not running */
#define ERR_NOMASTER	5	/* could not find master server */

extern bool_t xdr_domainname();

struct ypalias {
	const char *alias, *name;
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

static void
usage(void)
{
	fprintf(stderr, "%s\n%s\n",
		"usage: ypwhich [-d domain] [[-t] -m [mname] | host]",
		"       ypwhich -x");
	exit(ERR_USAGE);
}


/*
 * Like yp_bind except can query a specific host
 */
static int
bind_host(char *dom, struct sockaddr_in *lsin)
{
	struct hostent *hent = NULL;
	struct ypbind_resp ypbr;
	struct timeval tv;
	CLIENT *client;
	int sock, r;
	struct in_addr ss_addr;

	sock = RPC_ANYSOCK;
	tv.tv_sec = 15;
	tv.tv_usec = 0;
	client = clntudp_create(lsin, YPBINDPROG, YPBINDVERS, tv, &sock);
	if (client == NULL) {
		warnx("can't clntudp_create: %s", yperr_string(YPERR_YPBIND));
		return (YPERR_YPBIND);
	}

	tv.tv_sec = 5;
	tv.tv_usec = 0;
	r = clnt_call(client, YPBINDPROC_DOMAIN,
		xdr_domainname, &dom, xdr_ypbind_resp, &ypbr, tv);
	if (r != RPC_SUCCESS) {
		warnx("can't clnt_call: %s", yperr_string(YPERR_YPBIND));
		clnt_destroy(client);
		return (YPERR_YPBIND);
	} else {
		if (ypbr.ypbind_status != YPBIND_SUCC_VAL) {
			warnx("can't yp_bind: reason: %s",
				ypbinderr_string(ypbr.ypbind_resp_u.ypbind_error));
			clnt_destroy(client);
			return (r);
		}
	}
	clnt_destroy(client);

	ss_addr = *(struct in_addr *)ypbr.ypbind_resp_u.ypbind_bindinfo.ypbind_binding_addr;
	/*printf("%08x\n", ss_addr);*/
	hent = gethostbyaddr((char *)&ss_addr, sizeof(ss_addr), AF_INET);
	if (hent)
		printf("%s\n", hent->h_name);
	else
		printf("%s\n", inet_ntoa(ss_addr));
	return (0);
}

int
main(int argc, char *argv[])
{
	char *domnam = NULL, *master;
	const char *map = NULL;
	struct ypmaplist *ypml, *y;
	struct hostent *hent;
	struct sockaddr_in lsin;
	int notrans, mode, getmap;
	int c, r;
	u_int i;

	getmap = notrans = mode = 0;
	while ((c = getopt(argc, argv, "xd:mt")) != -1)
		switch (c) {
		case 'x':
			for (i = 0; i<sizeof ypaliases/sizeof ypaliases[0]; i++)
				printf("\"%s\" is an alias for \"%s\"\n",
					ypaliases[i].alias,
					ypaliases[i].name);
			exit(0);
		case 'd':
			domnam = optarg;
			break;
		case 't':
			notrans++;
			break;
		case 'm':
			mode++;
			break;
		default:
			usage();
		}

	if (!domnam)
		yp_get_default_domain(&domnam);

	if (mode == 0) {
		switch (argc-optind) {
		case 0:
			bzero(&lsin, sizeof lsin);
			lsin.sin_family = AF_INET;
			lsin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

			if (bind_host(domnam, &lsin))
				exit(ERR_NOBINDING);
			break;
		case 1:
			bzero(&lsin, sizeof lsin);
			lsin.sin_family = AF_INET;
			if ((lsin.sin_addr.s_addr = inet_addr(argv[optind])) == -1) {
				hent = gethostbyname(argv[optind]);
				if (!hent)
					errx(ERR_NOSUCHHOST, "host %s unknown", argv[optind]);
				bcopy((char *)hent->h_addr_list[0],
					(char *)&lsin.sin_addr, sizeof lsin.sin_addr);
			}
			if (bind_host(domnam, &lsin))
				exit(ERR_NOBINDING);
			break;
		default:
			usage();
		}
		exit(0);
	}

	if (argc-optind > 1)
		usage();

	if (argv[optind]) {
		map = argv[optind];
		for (i = 0; (!notrans) && i<sizeof ypaliases/sizeof ypaliases[0]; i++)
			if (strcmp(map, ypaliases[i].alias) == 0)
				map = ypaliases[i].name;
		r = yp_master(domnam, map, &master);
		switch (r) {
		case 0:
			printf("%s\n", master);
			free(master);
			break;
		case YPERR_YPBIND:
			errx(ERR_NOYPBIND, "not running ypbind");
		default:
			errx(ERR_NOMASTER, "can't find master for map %s. reason: %s",
				map, yperr_string(r));
		}
		exit(0);
	}

	ypml = NULL;
	r = yp_maplist(domnam, &ypml);
	switch (r) {
	case 0:
		for (y = ypml; y;) {
			ypml = y;
			r = yp_master(domnam, ypml->map, &master);
			switch (r) {
			case 0:
				printf("%s %s\n", ypml->map, master);
				free(master);
				break;
			default:
				warnx("can't find the master of %s: reason: %s",
					ypml->map, yperr_string(r));
				break;
			}
			y = ypml->next;
			free(ypml);
		}
		break;
	case YPERR_YPBIND:
		errx(ERR_NOYPBIND, "not running ypbind");
	default:
		errx(ERR_NOMASTER, "can't get map list for domain %s. reason: %s",
			domnam, yperr_string(r));
	}
	exit(0);
}
