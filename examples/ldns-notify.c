/* 
 * ldns-notify.c - ldns-notify(8)
 * 
 * Copyright (c) 2001-2008, NLnet Labs, All right reserved
 *
 * See LICENSE for the license
 *
 * send a notify packet to a server
 */

#include "config.h"

/* ldns */
#include <ldns/ldns.h>

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#include <errno.h>

static int verbose = 1;
static int max_num_retry = 15; /* times to try */

static void
usage(void)
{
	fprintf(stderr, "usage: ldns-notify [other options] -z zone <servers>\n");
	fprintf(stderr, "Ldns notify utility\n\n");
	fprintf(stderr, " Supported options:\n");
	fprintf(stderr, "\t-z zone\t\tThe zone\n");
	fprintf(stderr, "\t-s version\tSOA version number to include\n");
	fprintf(stderr, "\t-y key:data\tTSIG sign the query\n");
	fprintf(stderr, "\t-p port\t\tport to use to send to\n");
	fprintf(stderr, "\t-v\t\tPrint version information\n");
	fprintf(stderr, "\t-d\t\tPrint verbose debug information\n");
	fprintf(stderr, "\t-r num\t\tmax number of retries (%d)\n", 
		max_num_retry);
	fprintf(stderr, "\t-h\t\tPrint this help information\n\n");
	fprintf(stderr, "Report bugs to <ldns-team@nlnetlabs.nl>\n");
	exit(1);
}

static void
version(void)
{
        fprintf(stderr, "%s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
        fprintf(stderr, "Written by NLnet Labs.\n\n");
        fprintf(stderr,
                "Copyright (C) 2001-2008 NLnet Labs.  This is free software.\n"
                "There is NO warranty; not even for MERCHANTABILITY or FITNESS\n"
                "FOR A PARTICULAR PURPOSE.\n");
        exit(0);
}

static void
notify_host(int s, struct addrinfo* res, uint8_t* wire, size_t wiresize,
	const char* addrstr)
{
	int timeout_retry = 5; /* seconds */
	int num_retry = max_num_retry;
#ifndef S_SPLINT_S
	fd_set rfds;
#endif
	struct timeval tv;
	int retval = 0;
	ssize_t received = 0;
	int got_ack = 0;
	socklen_t addrlen = 0;
	uint8_t replybuf[2048];
	ldns_status status;
	ldns_pkt* pkt = NULL;
	
	while(!got_ack) {
		/* send it */
		if(sendto(s, (void*)wire, wiresize, 0, 
			res->ai_addr, res->ai_addrlen) == -1) {
			printf("warning: send to %s failed: %s\n",
				addrstr, strerror(errno));
#ifndef USE_WINSOCK
			close(s);
#else
			closesocket(s);
#endif
			return;
		}

		/* wait for ACK packet */
#ifndef S_SPLINT_S
		FD_ZERO(&rfds);
		FD_SET(s, &rfds);
		tv.tv_sec = timeout_retry; /* seconds */
#endif
		tv.tv_usec = 0; /* microseconds */
		retval = select(s + 1, &rfds, NULL, NULL, &tv);
		if (retval == -1) {
			printf("error waiting for reply from %s: %s\n",
				addrstr, strerror(errno));
#ifndef USE_WINSOCK
			close(s);
#else
			closesocket(s);
#endif
			return;
		}
		if(retval == 0) {
			num_retry--;
			if(num_retry == 0) {
				printf("error: failed to send notify to %s.\n",
					addrstr);
				exit(1);
			}
			printf("timeout (%d s) expired, retry notify to %s.\n",
				timeout_retry, addrstr);
		}
		if (retval == 1) {
			got_ack = 1;
		}
	}

	/* got reply */
	addrlen = res->ai_addrlen;
	received = recvfrom(s, (void*)replybuf, sizeof(replybuf), 0,
		res->ai_addr, &addrlen);
	res->ai_addrlen = addrlen;

#ifndef USE_WINSOCK
	close(s);
#else
	closesocket(s);
#endif
	if (received == -1) {
		printf("recv %s failed: %s\n", addrstr, strerror(errno));
		return;
	}

	/* check reply */
	status = ldns_wire2pkt(&pkt, replybuf, (size_t)received);
	if(status != LDNS_STATUS_OK) {
		ssize_t i;
		printf("Could not parse reply packet: %s\n",
			ldns_get_errorstr_by_id(status));
		if (verbose > 1) {
			printf("hexdump of reply: ");
			for(i=0; i<received; i++)
				printf("%02x", (unsigned)replybuf[i]);
			printf("\n");
		}
		exit(1);
	}

	if(verbose) {
		ssize_t i;
		printf("# reply from %s:\n", addrstr);
		ldns_pkt_print(stdout, pkt);
		if (verbose > 1) {
			printf("hexdump of reply: ");
			for(i=0; i<received; i++)
				printf("%02x", (unsigned)replybuf[i]);
			printf("\n");
		}
	}
	ldns_pkt_free(pkt);
}

int
main(int argc, char **argv)
{
	int c;
	int i;

	/* LDNS types */
	ldns_pkt *notify;
	ldns_rr *question;
	ldns_resolver *res;
	ldns_rdf *ldns_zone_name = NULL;
	ldns_status status;
	const char *zone_name = NULL;
	int include_soa = 0;
	uint32_t soa_version = 0;
	ldns_tsig_credentials tsig_cred = {0,0,0};
	int do_hexdump = 1;
	uint8_t *wire = NULL;
	size_t wiresize = 0;
	const char *port = "53";

	srandom(time(NULL) ^ getpid());

        while ((c = getopt(argc, argv, "vhdp:r:s:y:z:")) != -1) {
                switch (c) {
                case 'd':
			verbose++;
			break;
                case 'p':
			port = optarg;
			break;
                case 'r':
			max_num_retry = atoi(optarg);
			break;
                case 's':
			include_soa = 1;
			soa_version = (uint32_t)atoi(optarg);
			break;
                case 'y':
			tsig_cred.algorithm = (char*)"hmac-md5.sig-alg.reg.int.";
			tsig_cred.keyname = optarg;
			tsig_cred.keydata = strchr(optarg, ':');
			*tsig_cred.keydata = '\0';
			tsig_cred.keydata++;
			printf("Sign with %s : %s\n", tsig_cred.keyname,
				tsig_cred.keydata);
			break;
                case 'z':
			zone_name = optarg;
			ldns_zone_name = ldns_dname_new_frm_str(zone_name);
			if(!ldns_zone_name) {
				printf("cannot parse zone name: %s\n", 
					zone_name);
				exit(1);
			}
                        break;
		case 'v':
			version();
                case 'h':
                case '?':
                default:
                        usage();
                }
        }
        argc -= optind;
        argv += optind;

        if (argc == 0 || zone_name == NULL) {
                usage();
        }

	notify = ldns_pkt_new();
	question = ldns_rr_new();
	res = ldns_resolver_new();

	if (!notify || !question || !res) {
		/* bail out */
		printf("error: cannot create ldns types\n");
		exit(1);
	}

	/* create the rr for inside the pkt */
	ldns_rr_set_class(question, LDNS_RR_CLASS_IN);
	ldns_rr_set_owner(question, ldns_zone_name);
	ldns_rr_set_type(question, LDNS_RR_TYPE_SOA);
	ldns_pkt_set_opcode(notify, LDNS_PACKET_NOTIFY);
	ldns_pkt_push_rr(notify, LDNS_SECTION_QUESTION, question);
	ldns_pkt_set_aa(notify, true);
	ldns_pkt_set_id(notify, random()&0xffff);
	if(include_soa) {
		char buf[10240];
		ldns_rr *soa_rr=NULL;
		ldns_rdf *prev=NULL;
		snprintf(buf, sizeof(buf), "%s 3600 IN SOA . . %u 0 0 0 0",
			zone_name, (unsigned)soa_version);
		/*printf("Adding soa %s\n", buf);*/
		status = ldns_rr_new_frm_str(&soa_rr, buf, 3600, NULL, &prev);
		if(status != LDNS_STATUS_OK) {
			printf("Error adding SOA version: %s\n",
				ldns_get_errorstr_by_id(status));
		}
		ldns_pkt_push_rr(notify, LDNS_SECTION_ANSWER, soa_rr);
	}

	if(tsig_cred.keyname) {
#ifdef HAVE_SSL
		status = ldns_pkt_tsig_sign(notify, tsig_cred.keyname,
			tsig_cred.keydata, 300, tsig_cred.algorithm,
			NULL);
		if(status != LDNS_STATUS_OK) {
			printf("Error TSIG sign query: %s\n",
				ldns_get_errorstr_by_id(status));
		}
#else
	fprintf(stderr, "Warning: TSIG needs OpenSSL support, which has not been compiled in, TSIG skipped\n");
#endif
	}

	if(verbose) {
		printf("# Sending packet:\n");
		ldns_pkt_print(stdout, notify);

	}

	status = ldns_pkt2wire(&wire, notify, &wiresize);
	if(wiresize == 0) {
		printf("Error converting notify packet to hex.\n");
		exit(1);
	}

	if(do_hexdump && verbose > 1) {
		printf("Hexdump of notify packet:\n");
		for(i=0; i<(int)wiresize; i++)
			printf("%02x", (unsigned)wire[i]);
		printf("\n");
	}

	for(i=0; i<argc; i++)
	{
		struct addrinfo hints, *res0, *res;
		int error;
		int default_family = AF_INET;

		if(verbose)
			printf("# sending to %s\n", argv[i]);
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = default_family;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
		error = getaddrinfo(argv[i], port, &hints, &res0);
		if (error) {
			printf("skipping bad address: %s: %s\n", argv[i],
				gai_strerror(error));
			continue;
		}
		for (res = res0; res; res = res->ai_next) {
			int s = socket(res->ai_family, res->ai_socktype, 
				res->ai_protocol);
			if(s == -1)
				continue;
			/* send the notify */
			notify_host(s, res, wire, wiresize, argv[i]);
		}
		freeaddrinfo(res0);
	}

	ldns_pkt_free(notify);
	free(wire);
        return 0;
}
