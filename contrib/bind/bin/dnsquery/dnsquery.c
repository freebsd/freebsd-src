#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: dnsquery.c,v 8.19.10.1 2003/06/02 09:15:45 marka Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include "port_before.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "port_after.h"

#include <resolv.h>

extern int errno;
extern int h_errno;
extern char *h_errlist[];

struct __res_state res;

static int
newserver(char *srv, union res_sockaddr_union *u, int ns, int max) {
	struct addrinfo *answer = NULL;
	struct addrinfo *cur = NULL;
	struct addrinfo hint;
	short port = htons(NAMESERVER_PORT);

	memset(&hint, 0, sizeof(hint));
	hint.ai_socktype = SOCK_DGRAM;
	if (!getaddrinfo(srv, NULL, &hint, &answer)) {
		for (cur = answer; cur != NULL; cur = cur->ai_next) {
			if (ns >= max)
				break;
			switch (cur->ai_addr->sa_family) {
			case AF_INET6:
				u[ns].sin6 =
					 *(struct sockaddr_in6*)cur->ai_addr;
				u[ns++].sin6.sin6_port = port;
				break;
			case AF_INET:
				u[ns].sin = *(struct sockaddr_in*)cur->ai_addr;
				u[ns++].sin6.sin6_port = port;
				break;
			}
		}
		freeaddrinfo(answer);
	} else {
		fprintf(stderr, "Bad nameserver (%s)\n", srv);
		exit(1);
	}
	return (ns);
}

int
main(int argc, char *argv[]) {
	char name[MAXDNAME];
	u_char answer[8*1024];
	int c, n;
	int nameservers = 0, class, type, len;
	union res_sockaddr_union q_nsaddr[MAXNS];
	extern int optind;
	extern char *optarg;
	int stream = 0, debug = 0;

	/* set defaults */
	len = MAXDNAME;
	gethostname(name, len);
	class = C_IN;
	type = T_ANY;

	/* if no args, exit */
	if (argc == 1) {
		fprintf(stderr, "Usage:  %s [-h] host [-n ns] [-t type] [-c class] [-r retry] [-p period] [-s] [-v] [-d] [-a]\n", argv[0]);
		exit(1);
	}

	/* handle args */
	while ((c = getopt(argc, argv, "c:dh:n:p:r:st:u:v")) != -1) {
		switch (c) {

		case 'r' :	res.retry = atoi(optarg);
				break;

		case 'p' :	res.retrans = atoi(optarg);
				break;

		case 'h' :	if (strlen(optarg) >= sizeof(name)) {
					fprintf(stderr,
						"Domain name too long (%s)\n", optarg);
					exit(1);
				} else
					strcpy(name, optarg);
				break;

		case 'c' : {
				int success, proto_class;

				proto_class = sym_ston(__p_class_syms,
						       optarg, &success);
				if (success)
					class = proto_class;
				else {
				    fprintf(stderr, "Bad class (%s)\n", optarg);
					exit(1);
				}
			    }
				break;

		case 't' : {
				int success, proto_type;

				proto_type = sym_ston(__p_type_syms,
						      optarg, &success);
				if (success)
					type = proto_type;
				else {
				    fprintf(stderr, "Bad type (%s)\n", optarg);
					exit(1);
				}
			    }
				break;

		case 'd' :	debug++;
				break;

		case 's' :	
		case 'v' :	stream++;
				break;

		case 'n' :	
				/*
				 *  If we set some nameservers here without
				 *  using gethostbyname() first, then they will
				 *  get overwritten when we do the first query.
				 *  So, we must init the resolver before any 
				 *  of this.
				 */
				if (!(res.options & RES_INIT))
					if (res_ninit(&res) == -1) {
						fprintf(stderr,
							"res_ninit() failed\n"
							);
						exit(1);
				}
				nameservers = newserver(optarg, q_nsaddr,
							nameservers, MAXNS);
				break;

		default : 	fprintf(stderr, 
				"\tUsage:  %s [-n ns] [-h host] [-t type] [-c class] [-r retry] [-p period] [-s] [-v] [-d] [-a]\n", argv[0]);
				exit(1);
		}
	}
	if (optind < argc) {
		if (strlen(argv[optind]) >= sizeof(name)) {
			fprintf(stderr,
				"Domain name too long (%s)\n", argv[optind]);
			exit(1);
		} else {
			strcpy(name, argv[optind]);
		}
	}

	len = sizeof(answer);

	if (!(res.options & RES_INIT))
		if (res_ninit(&res) == -1) {
			fprintf(stderr, "res_ninit() failed\n");
			exit(1);
		}

	/* 
	 * set these here so they aren't set for a possible call to
	 * gethostbyname above
	*/
	if (debug) 
		res.options |= RES_DEBUG;
	if (stream)
	 	res.options |= RES_USEVC;

	/* if the -n flag was used, add them to the resolver's list */
	if (nameservers != 0)
		res_setservers(&res, q_nsaddr, nameservers);

	/*
	 * if the -h arg is fully-qualified, use res_query() since
	 * using res_search() will lead to use of res_querydomain()
	 * which will strip the trailing dot
	 */
	if (name[strlen(name) - 1] == '.') {
		n = res_nquery(&res, name, class, type, answer, len);
		if (n < 0) {
			fprintf(stderr, "Query failed (h_errno = %d) : %s\n", 
				h_errno, h_errlist[h_errno]);
			exit(1);
		}
	} else if ((n = res_nsearch(&res, name, class, type,
				    answer, len)) < 0) {
		fprintf(stderr, "Query failed (h_errno = %d) : %s\n", 
			h_errno, h_errlist[h_errno]);
		exit(1);
	}
	res_pquery(&res, answer, n, stdout);
	exit(0);
}
