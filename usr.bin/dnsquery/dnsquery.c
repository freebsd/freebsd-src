#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#include <errno.h>

#include "../conf/portability.h"

extern int errno;
extern int h_errno;
extern char *h_errlist[];

main(argc, argv)
int argc;
char *argv[];
{
	char name[MAXDNAME];
	u_char answer[8*1024];
	register int c, i = 0;
	unsigned long ul;
	int nameservers = 0, class, type, len;
	struct in_addr q_nsaddr[MAXNS];
	struct hostent *q_nsname;
	extern int optind, opterr;
	extern char *optarg;
	HEADER *hp;
	int stream = 0, debug = 0;

	/* set defaults */
	len = MAXDNAME;
	gethostname(name, len);
	class = C_IN;
	type = T_ANY;

	/* if no args, exit */
	if (argc == 1) {
		fprintf(stderr, "Usage:  %s [-h] host [-n ns] [-t type] [-c class] [-r retry] [-p period] [-s] [-v] [-d] [-a]\n", argv[0]);
		exit(-1);
	}

	/* handle args */
	while ((c = getopt(argc, argv, "c:dh:n:p:r:st:u:v")) != EOF) {
		switch (c) {

		case 'r' :	_res.retry = atoi(optarg);
				break;

		case 'p' :	_res.retrans = atoi(optarg);
				break;

		case 'h' :	strcpy(name, optarg);
				break;

		case 'c' :	if (!strcasecmp(optarg, "IN"))
					class = C_IN;
				else if (!strcasecmp(optarg, "HS"))
					class = C_HS;
				else if (!strcasecmp(optarg, "CHAOS"))
					class = C_CHAOS;
				else if (!strcasecmp(optarg, "ANY"))
					class = C_ANY;
				else {
					class = T_ANY;
					fprintf(stderr, "optarg=%s\n", optarg);
				}
				break;

		case 't' :	if (!strcasecmp(optarg, "A"))
					type = T_A;
				else if (!strcasecmp(optarg, "NS"))
					type = T_NS;
				else if (!strcasecmp(optarg, "MD"))
					type = T_MD;
				else if (!strcasecmp(optarg, "MF"))
					type = T_MF;
				else if (!strcasecmp(optarg, "CNAME"))
					type = T_CNAME;
				else if (!strcasecmp(optarg, "SOA"))
					type = T_SOA;
				else if (!strcasecmp(optarg, "MB"))
					type = T_MB;
				else if (!strcasecmp(optarg, "MG"))
					type = T_MG;
				else if (!strcasecmp(optarg, "MR"))
					type = T_MR;
				else if (!strcasecmp(optarg, "NULL"))
					type = T_NULL;
				else if (!strcasecmp(optarg, "WKS"))
					type = T_WKS;
				else if (!strcasecmp(optarg, "PTR"))
					type = T_PTR;
				else if (!strcasecmp(optarg, "HINFO"))
					type = T_HINFO;
				else if (!strcasecmp(optarg, "MINFO"))
					type = T_MINFO;
				else if (!strcasecmp(optarg, "MX"))
					type = T_MX;
				else if (!strcasecmp(optarg, "TXT"))
					type = T_TXT;
				else if (!strcasecmp(optarg, "RP"))
					type = T_RP;
				else if (!strcasecmp(optarg, "AFSDB"))
					type = T_AFSDB;
				else if (!strcasecmp(optarg, "ANY"))
					type = T_ANY;
				else if (!strcasecmp(optarg, "X25"))
					type = T_X25;
				else if (!strcasecmp(optarg, "ISDN"))
					type = T_ISDN;
				else if (!strcasecmp(optarg, "RT"))
					type = T_RT;
				else if (!strcasecmp(optarg, "NSAP"))
					type = T_NSAP;
				else if (!strcasecmp(optarg, "SIG"))
					type = T_SIG;
				else if (!strcasecmp(optarg, "KEY"))
					type = T_KEY;
				else if (!strcasecmp(optarg, "PX"))
					type = T_PX;
				else if (!strcasecmp(optarg, "GPOS"))
					type = T_GPOS;
				else if (!strcasecmp(optarg, "AAAA"))
					type = T_AAAA;
				else if (!strcasecmp(optarg, "LOC"))
					type = T_LOC;
				else {
					fprintf(stderr, "Bad type (%s)\n", optarg);
					exit(-1);
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
				if (!(_res.options & RES_INIT))
					if (res_init() == -1) {
						fprintf(stderr,
							"res_init() failed\n");
						exit(-1);
				}
				if (nameservers >= MAXNS) break;
				(void) inet_aton(optarg,
						 &q_nsaddr[nameservers]);
				if (!inet_aton(optarg, &ul)) {
					q_nsname = gethostbyname(optarg);
					if (q_nsname == 0) {
						fprintf(stderr,
						       "Bad nameserver (%s)\n",
							optarg);
						exit(-1);
					}
					bcopy((char *) q_nsname->h_addr,
					      (char *) &q_nsaddr[nameservers],
					      INADDRSZ);
				}
				else
					q_nsaddr[nameservers].s_addr = ul;
				nameservers++;
				break;

		default : 	fprintf(stderr, 
				"\tUsage:  %s [-n ns] [-h host] [-t type] [-c class] [-r retry] [-p period] [-s] [-v] [-d] [-a]\n", argv[0]);
				exit(-1);
		}
	}
	if (optind < argc)
		strcpy(name, argv[optind]);

	len = sizeof(answer);

	/* 
	 * set these here so they aren't set for a possible call to
	 * gethostbyname above
	*/
	if (debug) 
		_res.options |= RES_DEBUG;
	if (stream)
		 _res.options |= RES_USEVC;

	/* if the -n flag was used, add them to the resolver's list */
	if (nameservers != 0) {
		_res.nscount = nameservers;
		for (i = nameservers - 1; i >= 0; i--) {
			_res.nsaddr_list[i].sin_addr.s_addr = q_nsaddr[i].s_addr;
			_res.nsaddr_list[i].sin_family = AF_INET;
			_res.nsaddr_list[i].sin_port = htons(NAMESERVER_PORT);
		}
	}

	/*
	 * if the -h arg is fully-qualified, use res_query() since
	 * using res_search() will lead to use of res_querydomain()
	 * which will strip the trailing dot
	 */
	if (name[strlen(name) - 1] == '.') {
		if (res_query(name, class, type, answer, len) < 0) {
			hp = (HEADER *) answer;
			if ((hp->rcode == 0) && (hp->ancount > 0))
				__p_query(answer);
			else
				fprintf(stderr, "Query failed (h_errno = %d) : %s\n", 
						h_errno, h_errlist[h_errno]);
			exit(-1);
		}
	}
	else if (res_search(name, class, type, answer, len) < 0) {
		hp = (HEADER *) answer;
		if ((hp->rcode == 0) && (hp->ancount > 0))
			__p_query(answer);
		else
			fprintf(stderr, "Query failed (h_errno = %d) : %s\n", 
						h_errno, h_errlist[h_errno]);
		exit(-1);
	}
	__p_query(answer);
	exit(0);
}
