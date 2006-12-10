/*
 * Copyright (C) 2004-2006  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: host.c,v 1.76.2.5.2.16 2006/05/23 04:43:47 marka Exp $ */

#include <config.h>
#include <limits.h>

#include <isc/app.h>
#include <isc/commandline.h>
#include <isc/netaddr.h>
#include <isc/print.h>
#include <isc/string.h>
#include <isc/util.h>
#include <isc/task.h>
#include <isc/stdlib.h>

#include <dns/byaddr.h>
#include <dns/fixedname.h>
#include <dns/message.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdataset.h>
#include <dns/rdatatype.h>
#include <dns/rdatastruct.h>

#include <dig/dig.h>

static isc_boolean_t short_form = ISC_TRUE, listed_server = ISC_FALSE;
static isc_boolean_t default_lookups = ISC_TRUE;
static int seen_error = -1;
static isc_boolean_t list_addresses = ISC_TRUE;
static dns_rdatatype_t list_type = dns_rdatatype_a;
static isc_boolean_t printed_server = ISC_FALSE;

static const char *opcodetext[] = {
	"QUERY",
	"IQUERY",
	"STATUS",
	"RESERVED3",
	"NOTIFY",
	"UPDATE",
	"RESERVED6",
	"RESERVED7",
	"RESERVED8",
	"RESERVED9",
	"RESERVED10",
	"RESERVED11",
	"RESERVED12",
	"RESERVED13",
	"RESERVED14",
	"RESERVED15"
};

static const char *rcodetext[] = {
	"NOERROR",
	"FORMERR",
	"SERVFAIL",
	"NXDOMAIN",
	"NOTIMP",
	"REFUSED",
	"YXDOMAIN",
	"YXRRSET",
	"NXRRSET",
	"NOTAUTH",
	"NOTZONE",
	"RESERVED11",
	"RESERVED12",
	"RESERVED13",
	"RESERVED14",
	"RESERVED15",
	"BADVERS"
};

struct rtype {
	unsigned int type;
	const char *text;
};

struct rtype rtypes[] = {
	{ 1, 	"has address" },
	{ 2, 	"name server" },
	{ 5, 	"is an alias for" },
	{ 11,	"has well known services" },
	{ 12,	"domain name pointer" },
	{ 13,	"host information" },
	{ 15,	"mail is handled by" },
	{ 16,	"descriptive text" },
	{ 19,	"x25 address" },
	{ 20,	"ISDN address" },
	{ 24,	"has signature" },
	{ 25,	"has key" },
	{ 28,	"has IPv6 address" },
	{ 29,	"location" },
	{ 0, NULL }
};

static void
show_usage(void) {
	fputs(
"Usage: host [-aCdlriTwv] [-c class] [-N ndots] [-t type] [-W time]\n"
"            [-R number] hostname [server]\n"
"       -a is equivalent to -v -t *\n"
"       -c specifies query class for non-IN data\n"
"       -C compares SOA records on authoritative nameservers\n"
"       -d is equivalent to -v\n"
"       -l lists all hosts in a domain, using AXFR\n"
"       -i IP6.INT reverse lookups\n"
"       -N changes the number of dots allowed before root lookup is done\n"
"       -r disables recursive processing\n"
"       -R specifies number of retries for UDP packets\n"
"       -t specifies the query type\n"
"       -T enables TCP/IP mode\n"
"       -v enables verbose output\n"
"       -w specifies to wait forever for a reply\n"
"       -W specifies how long to wait for a reply\n"
"       -4 use IPv4 query transport only\n"
"       -6 use IPv6 query transport only\n", stderr);
	exit(1);
}

void
dighost_shutdown(void) {
	isc_app_shutdown();
}

void
received(int bytes, isc_sockaddr_t *from, dig_query_t *query) {
	isc_time_t now;
	int diff;

	if (!short_form) {
		char fromtext[ISC_SOCKADDR_FORMATSIZE];
		isc_sockaddr_format(from, fromtext, sizeof(fromtext));
		TIME_NOW(&now);
		diff = (int) isc_time_microdiff(&now, &query->time_sent);
		printf("Received %u bytes from %s in %d ms\n",
		       bytes, fromtext, diff/1000);
	}
}

void
trying(char *frm, dig_lookup_t *lookup) {
	UNUSED(lookup);

	if (!short_form)
		printf("Trying \"%s\"\n", frm);
}

static void
say_message(dns_name_t *name, const char *msg, dns_rdata_t *rdata,
	    dig_query_t *query)
{
	isc_buffer_t *b = NULL;
	char namestr[DNS_NAME_FORMATSIZE];
	isc_region_t r;
	isc_result_t result;
	unsigned int bufsize = BUFSIZ;

	dns_name_format(name, namestr, sizeof(namestr));
 retry:
	result = isc_buffer_allocate(mctx, &b, bufsize);
	check_result(result, "isc_buffer_allocate");
	result = dns_rdata_totext(rdata, NULL, b);
	if (result == ISC_R_NOSPACE) {
		isc_buffer_free(&b);
		bufsize *= 2;
		goto retry;
	}
	check_result(result, "dns_rdata_totext");
	isc_buffer_usedregion(b, &r);
	if (query->lookup->identify_previous_line) {
		printf("Nameserver %s:\n\t",
			query->servname);
	}
	printf("%s %s %.*s", namestr,
	       msg, (int)r.length, (char *)r.base);
	if (query->lookup->identify) {
		printf(" on server %s", query->servname);
	}
	printf("\n");
	isc_buffer_free(&b);
}
#ifdef DIG_SIGCHASE
/* Just for compatibility : not use in host program */
isc_result_t
printrdataset(dns_name_t *owner_name, dns_rdataset_t *rdataset,
	      isc_buffer_t *target)
{
  UNUSED(owner_name);
  UNUSED(rdataset);
  UNUSED(target);
  return(ISC_FALSE);
}
#endif
static isc_result_t
printsection(dns_message_t *msg, dns_section_t sectionid,
	     const char *section_name, isc_boolean_t headers,
	     dig_query_t *query)
{
	dns_name_t *name, *print_name;
	dns_rdataset_t *rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_buffer_t target;
	isc_result_t result, loopresult;
	isc_region_t r;
	dns_name_t empty_name;
	char t[4096];
	isc_boolean_t first;
	isc_boolean_t no_rdata;

	if (sectionid == DNS_SECTION_QUESTION)
		no_rdata = ISC_TRUE;
	else
		no_rdata = ISC_FALSE;

	if (headers)
		printf(";; %s SECTION:\n", section_name);

	dns_name_init(&empty_name, NULL);

	result = dns_message_firstname(msg, sectionid);
	if (result == ISC_R_NOMORE)
		return (ISC_R_SUCCESS);
	else if (result != ISC_R_SUCCESS)
		return (result);

	for (;;) {
		name = NULL;
		dns_message_currentname(msg, sectionid, &name);

		isc_buffer_init(&target, t, sizeof(t));
		first = ISC_TRUE;
		print_name = name;

		for (rdataset = ISC_LIST_HEAD(name->list);
		     rdataset != NULL;
		     rdataset = ISC_LIST_NEXT(rdataset, link)) {
			if (query->lookup->rdtype == dns_rdatatype_axfr &&
			    !((!list_addresses &&
			       (list_type == dns_rdatatype_any ||
			        rdataset->type == list_type)) ||
			      (list_addresses &&
			       (rdataset->type == dns_rdatatype_a ||
			        rdataset->type == dns_rdatatype_aaaa ||
				rdataset->type == dns_rdatatype_ns ||
				rdataset->type == dns_rdatatype_ptr))))
				continue;
			if (!short_form) {
				result = dns_rdataset_totext(rdataset,
							     print_name,
							     ISC_FALSE,
							     no_rdata,
							     &target);
				if (result != ISC_R_SUCCESS)
					return (result);
#ifdef USEINITALWS
				if (first) {
					print_name = &empty_name;
					first = ISC_FALSE;
				}
#else
				UNUSED(first); /* Shut up compiler. */
#endif
			} else {
				loopresult = dns_rdataset_first(rdataset);
				while (loopresult == ISC_R_SUCCESS) {
					struct rtype *t;
					const char *rtt;
					char typebuf[DNS_RDATATYPE_FORMATSIZE];
					char typebuf2[DNS_RDATATYPE_FORMATSIZE
						     + 20];
					dns_rdataset_current(rdataset, &rdata);

					for (t = rtypes; t->text != NULL; t++) {
						if (t->type == rdata.type) {
							rtt = t->text;
							goto found;
						}
					}

					dns_rdatatype_format(rdata.type,
							     typebuf,
							     sizeof(typebuf));
					snprintf(typebuf2, sizeof(typebuf2),
						 "has %s record", typebuf);
					rtt = typebuf2;
				found:
					say_message(print_name, rtt,
						    &rdata, query);
					dns_rdata_reset(&rdata);
					loopresult =
						dns_rdataset_next(rdataset);
				}
			}
		}
		if (!short_form) {
			isc_buffer_usedregion(&target, &r);
			if (no_rdata)
				printf(";%.*s", (int)r.length,
				       (char *)r.base);
			else
				printf("%.*s", (int)r.length, (char *)r.base);
		}

		result = dns_message_nextname(msg, sectionid);
		if (result == ISC_R_NOMORE)
			break;
		else if (result != ISC_R_SUCCESS)
			return (result);
	}

	return (ISC_R_SUCCESS);
}

static isc_result_t
printrdata(dns_message_t *msg, dns_rdataset_t *rdataset, dns_name_t *owner,
	   const char *set_name, isc_boolean_t headers)
{
	isc_buffer_t target;
	isc_result_t result;
	isc_region_t r;
	char t[4096];

	UNUSED(msg);
	if (headers)
		printf(";; %s SECTION:\n", set_name);

	isc_buffer_init(&target, t, sizeof(t));

	result = dns_rdataset_totext(rdataset, owner, ISC_FALSE, ISC_FALSE,
				     &target);
	if (result != ISC_R_SUCCESS)
		return (result);
	isc_buffer_usedregion(&target, &r);
	printf("%.*s", (int)r.length, (char *)r.base);

	return (ISC_R_SUCCESS);
}

static void
chase_cnamechain(dns_message_t *msg, dns_name_t *qname) {
	isc_result_t result;
	dns_rdataset_t *rdataset;
	dns_rdata_cname_t cname;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	unsigned int i = msg->counts[DNS_SECTION_ANSWER];

 	while (i-- > 0) {
		rdataset = NULL;
		result = dns_message_findname(msg, DNS_SECTION_ANSWER, qname,
					      dns_rdatatype_cname, 0, NULL,
					      &rdataset);
		if (result != ISC_R_SUCCESS)
			return;
		result = dns_rdataset_first(rdataset);
		check_result(result, "dns_rdataset_first");
		dns_rdata_reset(&rdata);
		dns_rdataset_current(rdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &cname, NULL);
		check_result(result, "dns_rdata_tostruct");
		dns_name_copy(&cname.cname, qname, NULL);
		dns_rdata_freestruct(&cname);
	}
}

isc_result_t
printmessage(dig_query_t *query, dns_message_t *msg, isc_boolean_t headers) {
	isc_boolean_t did_flag = ISC_FALSE;
	dns_rdataset_t *opt, *tsig = NULL;
	dns_name_t *tsigname;
	isc_result_t result = ISC_R_SUCCESS;
	int force_error;

	UNUSED(headers);

	/*
	 * We get called multiple times.
	 * Preserve any existing error status.
	 */
	force_error = (seen_error == 1) ? 1 : 0;
	seen_error = 1;
	if (listed_server && !printed_server) {
		char sockstr[ISC_SOCKADDR_FORMATSIZE];

		printf("Using domain server:\n");
		printf("Name: %s\n", query->userarg);
		isc_sockaddr_format(&query->sockaddr, sockstr,
				    sizeof(sockstr));
		printf("Address: %s\n", sockstr);
		printf("Aliases: \n\n");
		printed_server = ISC_TRUE;
	}

	if (msg->rcode != 0) {
		char namestr[DNS_NAME_FORMATSIZE];
		dns_name_format(query->lookup->name, namestr, sizeof(namestr));
		printf("Host %s not found: %d(%s)\n", namestr,
		       msg->rcode, rcodetext[msg->rcode]);
		return (ISC_R_SUCCESS);
	}

	if (default_lookups && query->lookup->rdtype == dns_rdatatype_a) {
		char namestr[DNS_NAME_FORMATSIZE];
		dig_lookup_t *lookup;
		dns_fixedname_t fixed;
		dns_name_t *name;

		/* Add AAAA and MX lookups. */
		dns_fixedname_init(&fixed);
		name = dns_fixedname_name(&fixed);
		dns_name_copy(query->lookup->name, name, NULL);
		chase_cnamechain(msg, name);
		dns_name_format(name, namestr, sizeof(namestr));
		lookup = clone_lookup(query->lookup, ISC_FALSE);
		if (lookup != NULL) {
			strncpy(lookup->textname, namestr,
				sizeof(lookup->textname));
			lookup->textname[sizeof(lookup->textname)-1] = 0;
			lookup->rdtype = dns_rdatatype_aaaa;
                        lookup->rdtypeset = ISC_TRUE;
			lookup->origin = NULL;
			lookup->retries = tries;
			ISC_LIST_APPEND(lookup_list, lookup, link);
		}
		lookup = clone_lookup(query->lookup, ISC_FALSE);
		if (lookup != NULL) {
			strncpy(lookup->textname, namestr,
				sizeof(lookup->textname));
			lookup->textname[sizeof(lookup->textname)-1] = 0;
			lookup->rdtype = dns_rdatatype_mx;
                        lookup->rdtypeset = ISC_TRUE;
			lookup->origin = NULL;
			lookup->retries = tries;
			ISC_LIST_APPEND(lookup_list, lookup, link);
		}
	}

	if (!short_form) {
		printf(";; ->>HEADER<<- opcode: %s, status: %s, id: %u\n",
		       opcodetext[msg->opcode], rcodetext[msg->rcode],
		       msg->id);
		printf(";; flags: ");
		if ((msg->flags & DNS_MESSAGEFLAG_QR) != 0) {
			printf("qr");
			did_flag = ISC_TRUE;
		}
		if ((msg->flags & DNS_MESSAGEFLAG_AA) != 0) {
			printf("%saa", did_flag ? " " : "");
			did_flag = ISC_TRUE;
		}
		if ((msg->flags & DNS_MESSAGEFLAG_TC) != 0) {
			printf("%stc", did_flag ? " " : "");
			did_flag = ISC_TRUE;
		}
		if ((msg->flags & DNS_MESSAGEFLAG_RD) != 0) {
			printf("%srd", did_flag ? " " : "");
			did_flag = ISC_TRUE;
		}
		if ((msg->flags & DNS_MESSAGEFLAG_RA) != 0) {
			printf("%sra", did_flag ? " " : "");
			did_flag = ISC_TRUE;
		}
		if ((msg->flags & DNS_MESSAGEFLAG_AD) != 0) {
			printf("%sad", did_flag ? " " : "");
			did_flag = ISC_TRUE;
		}
		if ((msg->flags & DNS_MESSAGEFLAG_CD) != 0) {
			printf("%scd", did_flag ? " " : "");
			did_flag = ISC_TRUE;
		}
		printf("; QUERY: %u, ANSWER: %u, "
		       "AUTHORITY: %u, ADDITIONAL: %u\n",
		       msg->counts[DNS_SECTION_QUESTION],
		       msg->counts[DNS_SECTION_ANSWER],
		       msg->counts[DNS_SECTION_AUTHORITY],
		       msg->counts[DNS_SECTION_ADDITIONAL]);
		opt = dns_message_getopt(msg);
		if (opt != NULL)
			printf(";; EDNS: version: %u, udp=%u\n",
			       (unsigned int)((opt->ttl & 0x00ff0000) >> 16),
			       (unsigned int)opt->rdclass);
		tsigname = NULL;
		tsig = dns_message_gettsig(msg, &tsigname);
		if (tsig != NULL)
			printf(";; PSEUDOSECTIONS: TSIG\n");
	}
	if (! ISC_LIST_EMPTY(msg->sections[DNS_SECTION_QUESTION]) &&
	    !short_form) {
		printf("\n");
		result = printsection(msg, DNS_SECTION_QUESTION, "QUESTION",
				      ISC_TRUE, query);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	if (! ISC_LIST_EMPTY(msg->sections[DNS_SECTION_ANSWER])) {
		if (!short_form)
			printf("\n");
		result = printsection(msg, DNS_SECTION_ANSWER, "ANSWER",
				      ISC_TF(!short_form), query);
		if (result != ISC_R_SUCCESS)
			return (result);
	}

	if (! ISC_LIST_EMPTY(msg->sections[DNS_SECTION_AUTHORITY]) &&
	    !short_form) {
		printf("\n");
		result = printsection(msg, DNS_SECTION_AUTHORITY, "AUTHORITY",
				      ISC_TRUE, query);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	if (! ISC_LIST_EMPTY(msg->sections[DNS_SECTION_ADDITIONAL]) &&
	    !short_form) {
		printf("\n");
		result = printsection(msg, DNS_SECTION_ADDITIONAL,
				      "ADDITIONAL", ISC_TRUE, query);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	if ((tsig != NULL) && !short_form) {
		printf("\n");
		result = printrdata(msg, tsig, tsigname,
				    "PSEUDOSECTION TSIG", ISC_TRUE);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	if (!short_form)
		printf("\n");

	if (short_form && !default_lookups &&
	    ISC_LIST_EMPTY(msg->sections[DNS_SECTION_ANSWER])) {
		char namestr[DNS_NAME_FORMATSIZE];
		char typestr[DNS_RDATATYPE_FORMATSIZE];
		dns_name_format(query->lookup->name, namestr, sizeof(namestr));
		dns_rdatatype_format(query->lookup->rdtype, typestr,
				     sizeof(typestr));
		printf("%s has no %s record\n", namestr, typestr);
	}
	seen_error = force_error;
	return (result);
}

static void
parse_args(isc_boolean_t is_batchfile, int argc, char **argv) {
	char hostname[MXNAME];
	dig_lookup_t *lookup;
	int c;
	char store[MXNAME];
	isc_textregion_t tr;
	isc_result_t result = ISC_R_SUCCESS;
	dns_rdatatype_t rdtype;
	dns_rdataclass_t rdclass;
	isc_uint32_t serial = 0;

	UNUSED(is_batchfile);

	lookup = make_empty_lookup();

	while ((c = isc_commandline_parse(argc, argv, "lvwrdt:c:aTCN:R:W:Dni46"))
	       != EOF) {
		switch (c) {
		case 'l':
			lookup->tcp_mode = ISC_TRUE;
			lookup->rdtype = dns_rdatatype_axfr;
			lookup->rdtypeset = ISC_TRUE;
			fatalexit = 3;
			break;
		case 'v':
		case 'd':
			short_form = ISC_FALSE;
			break;
		case 'r':
			lookup->recurse = ISC_FALSE;
			break;
		case 't':
			if (strncasecmp(isc_commandline_argument,
					"ixfr=", 5) == 0) {
				rdtype = dns_rdatatype_ixfr;
				/* XXXMPA add error checking */
				serial = strtoul(isc_commandline_argument + 5,
						 NULL, 10);
				result = ISC_R_SUCCESS;
			} else {
				tr.base = isc_commandline_argument;
				tr.length = strlen(isc_commandline_argument);
				result = dns_rdatatype_fromtext(&rdtype,
						   (isc_textregion_t *)&tr);
			}

			if (result != ISC_R_SUCCESS) {
				fatalexit = 2;
				fatal("invalid type: %s\n",
				      isc_commandline_argument);
			}
			if (!lookup->rdtypeset ||
			    lookup->rdtype != dns_rdatatype_axfr)
				lookup->rdtype = rdtype;
			lookup->rdtypeset = ISC_TRUE;
			if (rdtype == dns_rdatatype_axfr) {
				/* -l -t any -v */
				list_type = dns_rdatatype_any;
				short_form = ISC_FALSE;
				lookup->tcp_mode = ISC_TRUE;
			} else if (rdtype == dns_rdatatype_ixfr) {
				lookup->ixfr_serial = serial;
				list_type = rdtype;
			} else
				list_type = rdtype;
			list_addresses = ISC_FALSE;
			default_lookups = ISC_FALSE;
			break;
		case 'c':
			tr.base = isc_commandline_argument;
			tr.length = strlen(isc_commandline_argument);
			result = dns_rdataclass_fromtext(&rdclass,
						   (isc_textregion_t *)&tr);

			if (result != ISC_R_SUCCESS) {
				fatalexit = 2;
				fatal("invalid class: %s\n",
				      isc_commandline_argument);
			} else {
				lookup->rdclass = rdclass;
				lookup->rdclassset = ISC_TRUE;
			}
			default_lookups = ISC_FALSE;
			break;
		case 'a':
			if (!lookup->rdtypeset ||
			    lookup->rdtype != dns_rdatatype_axfr)
				lookup->rdtype = dns_rdatatype_any;
			list_type = dns_rdatatype_any;
			list_addresses = ISC_FALSE;
			lookup->rdtypeset = ISC_TRUE;
			short_form = ISC_FALSE;
			default_lookups = ISC_FALSE;
			break;
		case 'i':
			lookup->ip6_int = ISC_TRUE;
			break;
		case 'n':
			/* deprecated */
			break;
		case 'w':
			/*
			 * The timer routines are coded such that
			 * timeout==MAXINT doesn't enable the timer
			 */
			timeout = INT_MAX;
			break;
		case 'W':
			timeout = atoi(isc_commandline_argument);
			if (timeout < 1)
				timeout = 1;
			break;
		case 'R':
			tries = atoi(isc_commandline_argument) + 1;
			if (tries < 2)
				tries = 2;
			break;
		case 'T':
			lookup->tcp_mode = ISC_TRUE;
			break;
		case 'C':
			debug("showing all SOAs");
			lookup->rdtype = dns_rdatatype_ns;
			lookup->rdtypeset = ISC_TRUE;
			lookup->rdclass = dns_rdataclass_in;
			lookup->rdclassset = ISC_TRUE;
			lookup->ns_search_only = ISC_TRUE;
			lookup->trace_root = ISC_TRUE;
			lookup->identify_previous_line = ISC_TRUE;
			default_lookups = ISC_FALSE;
			break;
		case 'N':
			debug("setting NDOTS to %s",
			      isc_commandline_argument);
			ndots = atoi(isc_commandline_argument);
			break;
		case 'D':
			debugging = ISC_TRUE;
			break;
		case '4':
			if (have_ipv4) {
				isc_net_disableipv6();
				have_ipv6 = ISC_FALSE;
			} else
				fatal("can't find IPv4 networking");
			break;
		case '6':
			if (have_ipv6) {
				isc_net_disableipv4();
				have_ipv4 = ISC_FALSE;
			} else
				fatal("can't find IPv6 networking");
			break;
		}
	}

	lookup->retries = tries;

	if (isc_commandline_index >= argc)
		show_usage();

	strncpy(hostname, argv[isc_commandline_index], sizeof(hostname));
	hostname[sizeof(hostname)-1]=0;
	if (argc > isc_commandline_index + 1) {
		set_nameserver(argv[isc_commandline_index+1]);
		debug("server is %s", argv[isc_commandline_index+1]);
		listed_server = ISC_TRUE;
	}

	lookup->pending = ISC_FALSE;
	if (get_reverse(store, sizeof(store), hostname,
			lookup->ip6_int, ISC_TRUE) == ISC_R_SUCCESS) {
		strncpy(lookup->textname, store, sizeof(lookup->textname));
		lookup->textname[sizeof(lookup->textname)-1] = 0;
		lookup->rdtype = dns_rdatatype_ptr;
		lookup->rdtypeset = ISC_TRUE;
		default_lookups = ISC_FALSE;
	} else {
		strncpy(lookup->textname, hostname, sizeof(lookup->textname));
		lookup->textname[sizeof(lookup->textname)-1]=0;
	}
	lookup->new_search = ISC_TRUE;
	ISC_LIST_APPEND(lookup_list, lookup, link);

	usesearch = ISC_TRUE;
}

int
main(int argc, char **argv) {
	isc_result_t result;

	tries = 2;

	ISC_LIST_INIT(lookup_list);
	ISC_LIST_INIT(server_list);
	ISC_LIST_INIT(search_list);
	
	fatalexit = 1;

	debug("main()");
	progname = argv[0];
	result = isc_app_start();
	check_result(result, "isc_app_start");
	setup_libs();
	parse_args(ISC_FALSE, argc, argv);
	setup_system();
	result = isc_app_onrun(mctx, global_task, onrun_callback, NULL);
	check_result(result, "isc_app_onrun");
	isc_app_run();
	cancel_all();
	destroy_libs();
	isc_app_finish();
	return ((seen_error == 0) ? 0 : 1);
}

