/*-
 * Copyright (c) 2024 Lexi Winter
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

//
// options: parse command-line options.
//

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <err.h>
#include <sysexits.h>

#include "traceroute.h"

_Noreturn static void
usage(void) {
	(void)fprintf(stderr,
		      "usage: %s [-46adDeEFInrSvx]"
		      " [-a <AS server>] [-f <first ttl>] [-g <gateway>]"
		      " [-i <interface>] [-m <max ttl>] [-M <first ttl>]"
		      " [-p <port>] [-P <protocol>] [-q <nprobes>]"
		      " [-s <source>] [-t <tos>] [-w <wait time>]"
		      " [-z <pause time>]"
		      " <hostname> [<packet length>]",
		      getprogname());
	exit(EX_USAGE);
}

static struct options *
get_default_options(void) {
	struct options *ret;

	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		err(EX_OSERR, "calloc");

	ret->family = AF_UNSPEC;
	ret->protocol = IPPROTO_UDP;
	ret->packetlen = 0;
	ret->first_ttl = 1;
	ret->max_ttl = 64;
	ret->wait_time = 5;
	ret->port = 32768 + 666;

	return ret;
}

static unsigned
parse_unsigned(char const *name, char const *value) {
	char *endptr = NULL;

	errno = 0;
	unsigned long n = strtoul(value, &endptr, 10);

	if (*endptr != '\0')
		errx(EX_USAGE, "%s: invalid number: %s", name, value);

	if (errno || n > UINT_MAX)
		errx(EX_USAGE, "%s: value out of range: %s", name, value);

	return (unsigned)n;
}

static uint16_t
parse_uint16(char const *name, char const *value) {
	char *endptr = NULL;

	errno = 0;
	unsigned long n = strtoul(value, &endptr, 10);

	if (*endptr != '\0')
		errx(EX_USAGE, "%s: invalid number: %s", name, value);

	if (errno || n > UINT16_MAX)
		errx(EX_USAGE, "%s: value out of range: %s", name, value);

	return (uint16_t)n;
}

struct options *
parse_options(int *argc, char **argv) {
	struct options *opts = get_default_options();

	bool is_traceroute6 = (strcmp(getprogname(), "traceroute6") == 0);

	int ch;
	while ((ch = getopt(*argc, argv,
			    "46aA:dDeEf:Fg:i:IP:m:nNp:q:rs:St:TUvw:xz:")) != -1) {
		switch (ch) {
		case '4':
			if (opts->family != AF_UNSPEC)
				errx(EX_USAGE,
				     "cannot specify both -4 and -6");

			opts->family = AF_INET;
			break;

		case '6':
			if (opts->family != AF_UNSPEC)
				errx(EX_USAGE,
				     "cannot specify both -4 and -6");

			opts->family = AF_INET6;
			break;

		case 'a':
			opts->asn_lookups = true;
			break;

		case 'A':
			opts->asn_lookup_server = optarg;
			break;

		case 'd':
			opts->socket_debug = true;
			break;

		case 'e':
			opts->fixed_port = true;
			break;

		case 'E':
			opts->detect_ecn_bleaching = true;
			break;

		case 'f':
			opts->first_ttl = parse_unsigned("-f", optarg);
			break;

		case 'F':
			opts->set_df = true;
			break;

		case 'g': {
			char *newgw = strdup(optarg);
			if (!newgw)
				err(EX_OSERR, "strdup");

			++opts->ngateways;
			char **nbuf = realloc(opts->gateways,
					 opts->ngateways * sizeof(char *));
			if (!nbuf)
				err(EX_OSERR, "realloc");
			opts->gateways = nbuf;
			opts->gateways[opts->ngateways - 1] = newgw;
			break;
		}

		case 'i':
			opts->interface = strdup(optarg);
			if (!opts->interface)
				err(EX_OSERR, "strdup");
			break;

		case 'I': // synonym for -Picmp
			opts->protocol = IPPROTO_ICMP;
			break;

		case 'P':
			// XXX: implement
			break;

		case 'm':
			opts->max_ttl = parse_unsigned("-m", optarg);
			break;

		case 'n':
			opts->no_dns = true;
			break;

		case 'N': // traceroute6 compatibility
			opts->protocol = IPPROTO_NONE;
			break;

		case 'p':
			opts->port = parse_uint16("-p", optarg);
			break;

		case 'q':
			opts->nprobes = parse_unsigned("-q", optarg);
			break;

		case 'r':
			opts->send_direct = true;
			break;

		case 's':
			opts->source_hostname = strdup(optarg);
			if (!opts->source_hostname)
				err(EX_OSERR, "strdup");
			break;

		case 'S':
			// in traceroute, this means print summary; in
			// traceroute6, it means use SCTP.

			if (is_traceroute6)
				opts->protocol = IPPROTO_SCTP;
			else
				opts->summary = true;
			break;

		case 't':
			opts->tos = parse_unsigned("-t", optarg);
			break;

		case 'T': // traceroute6 compatibility
			opts->protocol = IPPROTO_TCP;
			break;

		case 'U': // traceroute6 compatibility
			opts->protocol = IPPROTO_UDP;
			break;

		case 'v':
			opts->verbose = true;
			break;

		case 'w':
			opts->wait_time = parse_unsigned("-w", optarg);
			break;

		case 'x':
			opts->toggle_checksums = true;
			break;

		case 'z':
			opts->pause_msecs = parse_unsigned("-z", optarg);
			break;

		default:
			usage();
		}
	}

	if (opts->nprobes == 0)
		opts->nprobes = opts->icmp_diff ? 1 : 3;

	if (opts->first_ttl > opts->max_ttl)
		errx(EX_USAGE, "first_ttl (%d) must be <= max_ttl (%d)",
		     opts->first_ttl, opts->max_ttl);

	memmove(argv, argv + optind, *argc - optind);
	*argc -= optind;

	if (*argc == 0)
		usage();

	opts->destination_hostname = strdup(argv[0]);
	if (!opts->destination_hostname)
		err(EX_OSERR, "strdup");

	// XXX set packetlen

	return opts;
}
