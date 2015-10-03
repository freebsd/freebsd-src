/*
 * Copyright (C) 2009-2015  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

/* $Id$ */

#include <config.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

#include <isc/app.h>
#include <isc/buffer.h>
#include <isc/lib.h>
#include <isc/mem.h>
#include <isc/print.h>
#include <isc/socket.h>
#include <isc/sockaddr.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <dns/client.h>
#include <dns/fixedname.h>
#include <dns/lib.h>
#include <dns/message.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/result.h>

#define MAX_PROBES 1000

static dns_client_t *client = NULL;
static isc_task_t *probe_task = NULL;
static isc_appctx_t *actx = NULL;
static isc_mem_t *mctx = NULL;
static unsigned int outstanding_probes = 0;
const char *cacheserver = "127.0.0.1";
static FILE *input;

typedef enum {
	none,
	exist,
	nxdomain,
	othererr,
	multiplesoa,
	multiplecname,
	brokenanswer,
	lame,
	timedout,
	notype,
	unexpected
} query_result_t;

struct server {
	ISC_LINK(struct server) link;

	isc_sockaddr_t address;
	query_result_t result_a;
	query_result_t result_aaaa;
};

struct probe_ns {
	ISC_LINK(struct probe_ns) link;

	dns_fixedname_t fixedname;
	dns_name_t *name;
	struct server *current_server;
	ISC_LIST(struct server) servers;
};

struct probe_trans {
	isc_boolean_t inuse;
	char *domain;
	dns_fixedname_t fixedname;
	dns_name_t *qname;
	const char **qlabel;
	isc_boolean_t qname_found;
	dns_clientrestrans_t *resid;
	dns_message_t *qmessage;
	dns_message_t *rmessage;
	dns_clientreqtrans_t *reqid;

	/* NS list */
	struct probe_ns *current_ns;
	ISC_LIST(struct probe_ns) nslist;
};

struct lcl_stat {
	unsigned long valid;
	unsigned long ignore;
	unsigned long nxdomain;
	unsigned long othererr;
	unsigned long multiplesoa;
	unsigned long multiplecname;
	unsigned long brokenanswer;
	unsigned long lame;
	unsigned long unknown;
} server_stat, domain_stat;

static unsigned long number_of_domains = 0;
static unsigned long number_of_servers = 0;
static unsigned long multiple_error_domains = 0;
static isc_boolean_t debug_mode = ISC_FALSE;
static int verbose_level = 0;
static const char *qlabels[] = {"www.", "ftp.", NULL};
static struct probe_trans probes[MAX_PROBES];

static isc_result_t probe_domain(struct probe_trans *trans);
static void reset_probe(struct probe_trans *trans);
static isc_result_t fetch_nsaddress(struct probe_trans *trans);
static isc_result_t probe_name(struct probe_trans *trans,
			       dns_rdatatype_t type);

/* Dump an rdataset for debug */
static isc_result_t
print_rdataset(dns_rdataset_t *rdataset, dns_name_t *owner) {
	isc_buffer_t target;
	isc_result_t result;
	isc_region_t r;
	char t[4096];

	if (!debug_mode)
		return (ISC_R_SUCCESS);

	isc_buffer_init(&target, t, sizeof(t));

	if (!dns_rdataset_isassociated(rdataset))
		return (ISC_R_SUCCESS);
	result = dns_rdataset_totext(rdataset, owner, ISC_FALSE, ISC_FALSE,
				     &target);
	if (result != ISC_R_SUCCESS)
		return (result);
	isc_buffer_usedregion(&target, &r);
	printf("%.*s", (int)r.length, (char *)r.base);

	return (ISC_R_SUCCESS);
}

static isc_result_t
print_name(dns_name_t *name) {
	isc_result_t result;
	isc_buffer_t target;
	isc_region_t r;
	char t[4096];

	isc_buffer_init(&target, t, sizeof(t));
	result = dns_name_totext(name, ISC_TRUE, &target);
	if (result == ISC_R_SUCCESS) {
		isc_buffer_usedregion(&target, &r);
		printf("%.*s", (int)r.length, (char *)r.base);
	} else
		printf("(invalid name)");

	return (result);
}

static isc_result_t
print_address(FILE *fp, isc_sockaddr_t *addr) {
	char buf[NI_MAXHOST];

	if (getnameinfo(&addr->type.sa, addr->length, buf, sizeof(buf),
			NULL, 0, NI_NUMERICHOST) == 0) {
		fprintf(fp, "%s", buf);
	} else {
		fprintf(fp, "(invalid address)");
	}

	return (ISC_R_SUCCESS);
}

static void
ctxs_destroy(isc_mem_t **mctxp, isc_appctx_t **actxp,
	     isc_taskmgr_t **taskmgrp, isc_socketmgr_t **socketmgrp,
	     isc_timermgr_t **timermgrp)
{
	if (*taskmgrp != NULL)
		isc_taskmgr_destroy(taskmgrp);

	if (*timermgrp != NULL)
		isc_timermgr_destroy(timermgrp);

	if (*socketmgrp != NULL)
		isc_socketmgr_destroy(socketmgrp);

	if (*actxp != NULL)
		isc_appctx_destroy(actxp);

	if (*mctxp != NULL)
		isc_mem_destroy(mctxp);
}

static isc_result_t
ctxs_init(isc_mem_t **mctxp, isc_appctx_t **actxp,
	  isc_taskmgr_t **taskmgrp, isc_socketmgr_t **socketmgrp,
	  isc_timermgr_t **timermgrp)
{
	isc_result_t result;

	result = isc_mem_create(0, 0, mctxp);
	if (result != ISC_R_SUCCESS)
		goto fail;

	result = isc_appctx_create(*mctxp, actxp);
	if (result != ISC_R_SUCCESS)
		goto fail;

	result = isc_taskmgr_createinctx(*mctxp, *actxp, 1, 0, taskmgrp);
	if (result != ISC_R_SUCCESS)
		goto fail;

	result = isc_socketmgr_createinctx(*mctxp, *actxp, socketmgrp);
	if (result != ISC_R_SUCCESS)
		goto fail;

	result = isc_timermgr_createinctx(*mctxp, *actxp, timermgrp);
	if (result != ISC_R_SUCCESS)
		goto fail;

	return (ISC_R_SUCCESS);

 fail:
	ctxs_destroy(mctxp, actxp, taskmgrp, socketmgrp, timermgrp);

	return (result);
}

/*
 * Common routine to make query data
 */
static isc_result_t
make_querymessage(dns_message_t *message, dns_name_t *qname0,
		  dns_rdatatype_t rdtype)
{
	dns_name_t *qname = NULL;
	dns_rdataset_t *qrdataset = NULL;
	isc_result_t result;

	message->opcode = dns_opcode_query;
	message->rdclass = dns_rdataclass_in;

	result = dns_message_gettempname(message, &qname);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = dns_message_gettemprdataset(message, &qrdataset);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	dns_name_init(qname, NULL);
	dns_name_clone(qname0, qname);
	dns_rdataset_init(qrdataset);
	dns_rdataset_makequestion(qrdataset, message->rdclass, rdtype);
	ISC_LIST_APPEND(qname->list, qrdataset, link);
	dns_message_addname(message, qname, DNS_SECTION_QUESTION);

	return (ISC_R_SUCCESS);

 cleanup:
	if (qname != NULL)
		dns_message_puttempname(message, &qname);
	if (qrdataset != NULL)
		dns_message_puttemprdataset(message, &qrdataset);
	return (result);
}

/*
 * Update statistics
 */
static inline void
increment_entry(unsigned long *entryp) {
	(*entryp)++;
	INSIST(*entryp != 0U);	/* check overflow */
}

static void
update_stat(struct probe_trans *trans) {
	struct probe_ns *pns;
	struct server *server;
	struct lcl_stat local_stat;
	unsigned int err_count = 0;
	const char *stattype;

	increment_entry(&number_of_domains);
	memset(&local_stat, 0, sizeof(local_stat));

	/* Update per sever statistics */
	for (pns = ISC_LIST_HEAD(trans->nslist); pns != NULL;
	     pns = ISC_LIST_NEXT(pns, link)) {
		for (server = ISC_LIST_HEAD(pns->servers); server != NULL;
		     server = ISC_LIST_NEXT(server, link)) {
			increment_entry(&number_of_servers);

			if (server->result_aaaa == exist ||
			    server->result_aaaa == notype) {
				/*
				 * Don't care about the result of A query if
				 * the answer to AAAA query was expected.
				 */
				stattype = "valid";
				increment_entry(&server_stat.valid);
				increment_entry(&local_stat.valid);
			} else if (server->result_a == exist) {
				switch (server->result_aaaa) {
				case exist:
				case notype:
					stattype = "valid";
					increment_entry(&server_stat.valid);
					increment_entry(&local_stat.valid);
					break;
				case timedout:
					stattype = "ignore";
					increment_entry(&server_stat.ignore);
					increment_entry(&local_stat.ignore);
					break;
				case nxdomain:
					stattype = "nxdomain";
					increment_entry(&server_stat.nxdomain);
					increment_entry(&local_stat.nxdomain);
					break;
				case othererr:
					stattype = "othererr";
					increment_entry(&server_stat.othererr);
					increment_entry(&local_stat.othererr);
					break;
				case multiplesoa:
					stattype = "multiplesoa";
					increment_entry(&server_stat.multiplesoa);
					increment_entry(&local_stat.multiplesoa);
					break;
				case multiplecname:
					stattype = "multiplecname";
					increment_entry(&server_stat.multiplecname);
					increment_entry(&local_stat.multiplecname);
					break;
				case brokenanswer:
					stattype = "brokenanswer";
					increment_entry(&server_stat.brokenanswer);
					increment_entry(&local_stat.brokenanswer);
					break;
				case lame:
					stattype = "lame";
					increment_entry(&server_stat.lame);
					increment_entry(&local_stat.lame);
					break;
				default:
					stattype = "unknown";
					increment_entry(&server_stat.unknown);
					increment_entry(&local_stat.unknown);
					break;
				}
			} else {
				stattype = "unknown";
				increment_entry(&server_stat.unknown);
				increment_entry(&local_stat.unknown);
			}

			if (verbose_level > 1 ||
			    (verbose_level == 1 &&
			     strcmp(stattype, "valid") != 0 &&
			     strcmp(stattype, "unknown") != 0)) {
				print_name(pns->name);
				putchar('(');
				print_address(stdout, &server->address);
				printf(") for %s:%s\n", trans->domain,
				       stattype);
			}
		}
	}

	/* Update per domain statistics */
	if (local_stat.ignore > 0U) {
		if (verbose_level > 0)
			printf("%s:ignore\n", trans->domain);
		increment_entry(&domain_stat.ignore);
		err_count++;
	}
	if (local_stat.nxdomain > 0U) {
		if (verbose_level > 0)
			printf("%s:nxdomain\n", trans->domain);
		increment_entry(&domain_stat.nxdomain);
		err_count++;
	}
	if (local_stat.othererr > 0U) {
		if (verbose_level > 0)
			printf("%s:othererr\n", trans->domain);
		increment_entry(&domain_stat.othererr);
		err_count++;
	}
	if (local_stat.multiplesoa > 0U) {
		if (verbose_level > 0)
			printf("%s:multiplesoa\n", trans->domain);
		increment_entry(&domain_stat.multiplesoa);
		err_count++;
	}
	if (local_stat.multiplecname > 0U) {
		if (verbose_level > 0)
			printf("%s:multiplecname\n", trans->domain);
		increment_entry(&domain_stat.multiplecname);
		err_count++;
	}
	if (local_stat.brokenanswer > 0U) {
		if (verbose_level > 0)
			printf("%s:brokenanswer\n", trans->domain);
		increment_entry(&domain_stat.brokenanswer);
		err_count++;
	}
	if (local_stat.lame > 0U) {
		if (verbose_level > 0)
			printf("%s:lame\n", trans->domain);
		increment_entry(&domain_stat.lame);
		err_count++;
	}

	if (err_count > 1U)
		increment_entry(&multiple_error_domains);

	/*
	 * We regard the domain as valid if and only if no authoritative server
	 * has a problem and at least one server is known to be valid.
	 */
	if (local_stat.valid > 0U && err_count == 0U) {
		if (verbose_level > 1)
			printf("%s:valid\n", trans->domain);
		increment_entry(&domain_stat.valid);
	}

	/*
	 * If the domain has no available server or all servers have the
	 * 'unknown' result, the domain's result is also regarded as unknown.
	 */
	if (local_stat.valid == 0U && err_count == 0U) {
		if (verbose_level > 1)
			printf("%s:unknown\n", trans->domain);
		increment_entry(&domain_stat.unknown);
	}
}

/*
 * Search for an existent name with an A RR
 */

static isc_result_t
set_nextqname(struct probe_trans *trans) {
	isc_result_t result;
	size_t domainlen;
	isc_buffer_t b;
	char buf[4096];	/* XXX ad-hoc constant, but should be enough */

	if (*trans->qlabel == NULL)
		return (ISC_R_NOMORE);

	result = isc_string_copy(buf, sizeof(buf), *trans->qlabel);
	if (result != ISC_R_SUCCESS)
		return (result);
	result = isc_string_append(buf, sizeof(buf), trans->domain);
	if (result != ISC_R_SUCCESS)
		return (result);

	domainlen = strlen(buf);
	isc_buffer_init(&b, buf, domainlen);
	isc_buffer_add(&b, domainlen);
	dns_fixedname_init(&trans->fixedname);
	trans->qname = dns_fixedname_name(&trans->fixedname);
	result = dns_name_fromtext(trans->qname, &b, dns_rootname,
				   0, NULL);

	trans->qlabel++;

	return (result);
}

static void
request_done(isc_task_t *task, isc_event_t *event) {
	struct probe_trans *trans = event->ev_arg;
	dns_clientreqevent_t *rev = (dns_clientreqevent_t *)event;
	dns_message_t *rmessage;
	struct probe_ns *pns;
	struct server *server;
	isc_result_t result;
	query_result_t *resultp;
	dns_name_t *name;
	dns_rdataset_t *rdataset;
	dns_rdatatype_t type;

	REQUIRE(task == probe_task);
	REQUIRE(trans != NULL && trans->inuse == ISC_TRUE);
	rmessage = rev->rmessage;
	REQUIRE(rmessage == trans->rmessage);
	INSIST(outstanding_probes > 0);

	server = trans->current_ns->current_server;
	INSIST(server != NULL);

	if (server->result_a == none) {
		type = dns_rdatatype_a;
		resultp = &server->result_a;
	} else {
		resultp = &server->result_aaaa;
		type = dns_rdatatype_aaaa;
	}

	if (rev->result == ISC_R_SUCCESS) {
		if ((rmessage->flags & DNS_MESSAGEFLAG_AA) == 0)
			*resultp = lame;
		else if (rmessage->rcode == dns_rcode_nxdomain)
			*resultp = nxdomain;
		else if (rmessage->rcode != dns_rcode_noerror)
			*resultp = othererr;
		else if (rmessage->counts[DNS_SECTION_ANSWER] == 0) {
			/* no error but empty answer */
			*resultp = notype;
		} else {
			result = dns_message_firstname(rmessage,
						       DNS_SECTION_ANSWER);
			while (result == ISC_R_SUCCESS) {
				name = NULL;
				dns_message_currentname(rmessage,
							DNS_SECTION_ANSWER,
							&name);
				for (rdataset = ISC_LIST_HEAD(name->list);
				     rdataset != NULL;
				     rdataset = ISC_LIST_NEXT(rdataset,
							      link)) {
					(void)print_rdataset(rdataset, name);

					if (rdataset->type ==
					    dns_rdatatype_cname ||
					    rdataset->type ==
					    dns_rdatatype_dname) {
						/* Should chase the chain? */
						*resultp = exist;
						goto found;
					} else if (rdataset->type == type) {
						*resultp = exist;
						goto found;
					}
				}
				result = dns_message_nextname(rmessage,
							      DNS_SECTION_ANSWER);
			}

			/*
			 * Something unexpected happened: the response
			 * contained a non-empty authoritative answer, but we
			 * could not find an expected result.
			 */
			*resultp = unexpected;
		}
	} else if (rev->result == DNS_R_RECOVERABLE ||
		   rev->result == DNS_R_BADLABELTYPE) {
		/* Broken response.  Try identifying known cases. */
		*resultp = brokenanswer;

		if (rmessage->counts[DNS_SECTION_ANSWER] > 0) {
			result = dns_message_firstname(rmessage,
						       DNS_SECTION_ANSWER);
			while (result == ISC_R_SUCCESS) {
				/*
				 * Check to see if the response has multiple
				 * CNAME RRs.  Update the result code if so.
				 */
				name = NULL;
				dns_message_currentname(rmessage,
							DNS_SECTION_ANSWER,
							&name);
				for (rdataset = ISC_LIST_HEAD(name->list);
				     rdataset != NULL;
				     rdataset = ISC_LIST_NEXT(rdataset,
							      link)) {
					if (rdataset->type ==
					    dns_rdatatype_cname &&
					    dns_rdataset_count(rdataset) > 1) {
						*resultp = multiplecname;
						goto found;
					}
				}
				result = dns_message_nextname(rmessage,
							      DNS_SECTION_ANSWER);
			}
		}

		if (rmessage->counts[DNS_SECTION_AUTHORITY] > 0) {
			result = dns_message_firstname(rmessage,
						       DNS_SECTION_AUTHORITY);
			while (result == ISC_R_SUCCESS) {
				/*
				 * Check to see if the response has multiple
				 * SOA RRs.  Update the result code if so.
				 */
				name = NULL;
				dns_message_currentname(rmessage,
							DNS_SECTION_AUTHORITY,
							&name);
				for (rdataset = ISC_LIST_HEAD(name->list);
				     rdataset != NULL;
				     rdataset = ISC_LIST_NEXT(rdataset,
							      link)) {
					if (rdataset->type ==
					    dns_rdatatype_soa &&
					    dns_rdataset_count(rdataset) > 1) {
						*resultp = multiplesoa;
						goto found;
					}
				}
				result = dns_message_nextname(rmessage,
							      DNS_SECTION_AUTHORITY);
			}
		}
	} else if (rev->result == ISC_R_TIMEDOUT)
		*resultp = timedout;
	else {
		fprintf(stderr, "unexpected result: %d (domain=%s, server=",
			rev->result, trans->domain);
		print_address(stderr, &server->address);
		fputc('\n', stderr);
		*resultp = unexpected;
	}

 found:
	INSIST(*resultp != none);
	if (type == dns_rdatatype_a && *resultp == exist)
		trans->qname_found = ISC_TRUE;

	dns_client_destroyreqtrans(&trans->reqid);
	isc_event_free(&event);
	dns_message_reset(trans->rmessage, DNS_MESSAGE_INTENTPARSE);

	result = probe_name(trans, type);
	if (result == ISC_R_NOMORE) {
		/* We've tried all addresses of all servers. */
		if (type == dns_rdatatype_a && trans->qname_found) {
			/*
			 * If we've explored A RRs and found an existent
			 * record, we can move to AAAA.
			 */
			trans->current_ns = ISC_LIST_HEAD(trans->nslist);
			probe_name(trans, dns_rdatatype_aaaa);
			result = ISC_R_SUCCESS;
		} else if (type == dns_rdatatype_a) {
			/*
			 * No server provided an existent A RR of this name.
			 * Try next label.
			 */
			dns_fixedname_invalidate(&trans->fixedname);
			trans->qname = NULL;
			result = set_nextqname(trans);
			if (result == ISC_R_SUCCESS) {
				trans->current_ns =
					ISC_LIST_HEAD(trans->nslist);
				for (pns = trans->current_ns; pns != NULL;
				     pns = ISC_LIST_NEXT(pns, link)) {
					for (server = ISC_LIST_HEAD(pns->servers);
					     server != NULL;
					     server = ISC_LIST_NEXT(server,
								    link)) {
						INSIST(server->result_aaaa ==
						       none);
						server->result_a = none;
					}
				}
				result = probe_name(trans, dns_rdatatype_a);
			}
		}
		if (result != ISC_R_SUCCESS) {
			/*
			 * We've explored AAAA RRs or failed to find a valid
			 * query label.  Wrap up the result and move to the
			 * next domain.
			 */
			reset_probe(trans);
		}
	} else if (result != ISC_R_SUCCESS)
		reset_probe(trans); /* XXX */
}

static isc_result_t
probe_name(struct probe_trans *trans, dns_rdatatype_t type) {
	isc_result_t result;
	struct probe_ns *pns;
	struct server *server;

	REQUIRE(trans->reqid == NULL);
	REQUIRE(type == dns_rdatatype_a || type == dns_rdatatype_aaaa);

	for (pns = trans->current_ns; pns != NULL;
	     pns = ISC_LIST_NEXT(pns, link)) {
		for (server = ISC_LIST_HEAD(pns->servers); server != NULL;
		     server = ISC_LIST_NEXT(server, link)) {
			if ((type == dns_rdatatype_a &&
			     server->result_a == none) ||
			    (type == dns_rdatatype_aaaa &&
			     server->result_aaaa == none)) {
				pns->current_server = server;
				goto found;
			}
		}
	}

 found:
	trans->current_ns = pns;
	if (pns == NULL)
		return (ISC_R_NOMORE);

	INSIST(pns->current_server != NULL);
	dns_message_reset(trans->qmessage, DNS_MESSAGE_INTENTRENDER);
	result = make_querymessage(trans->qmessage, trans->qname, type);
	if (result != ISC_R_SUCCESS)
		return (result);
	result = dns_client_startrequest(client, trans->qmessage,
					 trans->rmessage,
					 &pns->current_server->address,
					 0, DNS_MESSAGEPARSE_BESTEFFORT,
					 NULL, 120, 0, 4,
					 probe_task, request_done, trans,
					 &trans->reqid);

	return (result);
}

/*
 * Get IP addresses of NSes
 */

static void
resolve_nsaddress(isc_task_t *task, isc_event_t *event) {
	struct probe_trans *trans = event->ev_arg;
	dns_clientresevent_t *rev = (dns_clientresevent_t *)event;
	dns_name_t *name;
	dns_rdataset_t *rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	struct probe_ns *pns = trans->current_ns;
	isc_result_t result;

	REQUIRE(task == probe_task);
	REQUIRE(trans->inuse == ISC_TRUE);
	REQUIRE(pns != NULL);
	INSIST(outstanding_probes > 0);

	for (name = ISC_LIST_HEAD(rev->answerlist); name != NULL;
	     name = ISC_LIST_NEXT(name, link)) {
		for (rdataset = ISC_LIST_HEAD(name->list);
		     rdataset != NULL;
		     rdataset = ISC_LIST_NEXT(rdataset, link)) {
			(void)print_rdataset(rdataset, name);

			if (rdataset->type != dns_rdatatype_a)
				continue;

			for (result = dns_rdataset_first(rdataset);
			     result == ISC_R_SUCCESS;
			     result = dns_rdataset_next(rdataset)) {
				dns_rdata_in_a_t rdata_a;
				struct server *server;

				dns_rdataset_current(rdataset, &rdata);
				result = dns_rdata_tostruct(&rdata, &rdata_a,
							    NULL);
				if (result != ISC_R_SUCCESS)
					continue;

				server = isc_mem_get(mctx, sizeof(*server));
				if (server == NULL) {
					fprintf(stderr, "resolve_nsaddress: "
						"mem_get failed");
					result = ISC_R_NOMEMORY;
					POST(result);
					goto cleanup;
				}
				isc_sockaddr_fromin(&server->address,
						    &rdata_a.in_addr, 53);
				ISC_LINK_INIT(server, link);
				server->result_a = none;
				server->result_aaaa = none;
				ISC_LIST_APPEND(pns->servers, server, link);
			}
		}
	}

 cleanup:
	dns_client_freeresanswer(client, &rev->answerlist);
	dns_client_destroyrestrans(&trans->resid);
	isc_event_free(&event);

 next_ns:
	trans->current_ns = ISC_LIST_NEXT(pns, link);
	if (trans->current_ns == NULL) {
		trans->current_ns = ISC_LIST_HEAD(trans->nslist);
		dns_fixedname_invalidate(&trans->fixedname);
		trans->qname = NULL;
		result = set_nextqname(trans);
		if (result == ISC_R_SUCCESS)
			 result = probe_name(trans, dns_rdatatype_a);
	} else {
		result = fetch_nsaddress(trans);
		if (result != ISC_R_SUCCESS)
			goto next_ns; /* XXX: this is unlikely to succeed */
	}

	if (result != ISC_R_SUCCESS)
		reset_probe(trans);
}

static isc_result_t
fetch_nsaddress(struct probe_trans *trans) {
	struct probe_ns *pns;

	pns = trans->current_ns;
	REQUIRE(pns != NULL);

	return (dns_client_startresolve(client, pns->name, dns_rdataclass_in,
					dns_rdatatype_a, 0, probe_task,
					resolve_nsaddress, trans,
					&trans->resid));
}

/*
 * Get NS RRset for a given domain
 */

static void
reset_probe(struct probe_trans *trans) {
	struct probe_ns *pns;
	struct server *server;
	isc_result_t result;

	REQUIRE(trans->resid == NULL);
	REQUIRE(trans->reqid == NULL);

	update_stat(trans);

	dns_message_reset(trans->qmessage, DNS_MESSAGE_INTENTRENDER);
	dns_message_reset(trans->rmessage, DNS_MESSAGE_INTENTPARSE);

	trans->inuse = ISC_FALSE;
	if (trans->domain != NULL)
		isc_mem_free(mctx, trans->domain);
	trans->domain = NULL;
	if (trans->qname != NULL)
		dns_fixedname_invalidate(&trans->fixedname);
	trans->qname = NULL;
	trans->qlabel = qlabels;
	trans->qname_found = ISC_FALSE;
	trans->current_ns = NULL;

	while ((pns = ISC_LIST_HEAD(trans->nslist)) != NULL) {
		ISC_LIST_UNLINK(trans->nslist, pns, link);
		while ((server = ISC_LIST_HEAD(pns->servers)) != NULL) {
			ISC_LIST_UNLINK(pns->servers, server, link);
			isc_mem_put(mctx, server, sizeof(*server));
		}
		isc_mem_put(mctx, pns, sizeof(*pns));
	}

	outstanding_probes--;

	result = probe_domain(trans);
	if (result == ISC_R_NOMORE && outstanding_probes == 0)
		isc_app_ctxshutdown(actx);
}

static void
resolve_ns(isc_task_t *task, isc_event_t *event) {
	struct probe_trans *trans = event->ev_arg;
	dns_clientresevent_t *rev = (dns_clientresevent_t *)event;
	dns_name_t *name;
	dns_rdataset_t *rdataset;
	isc_result_t result = ISC_R_SUCCESS;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	struct probe_ns *pns;

	REQUIRE(task == probe_task);
	REQUIRE(trans->inuse == ISC_TRUE);
	INSIST(outstanding_probes > 0);

	for (name = ISC_LIST_HEAD(rev->answerlist); name != NULL;
	     name = ISC_LIST_NEXT(name, link)) {
		for (rdataset = ISC_LIST_HEAD(name->list);
		     rdataset != NULL;
		     rdataset = ISC_LIST_NEXT(rdataset, link)) {
			(void)print_rdataset(rdataset, name);

			if (rdataset->type != dns_rdatatype_ns)
				continue;

			for (result = dns_rdataset_first(rdataset);
			     result == ISC_R_SUCCESS;
			     result = dns_rdataset_next(rdataset)) {
				dns_rdata_ns_t ns;

				dns_rdataset_current(rdataset, &rdata);
				/*
				 * Extract the name from the NS record.
				 */
				result = dns_rdata_tostruct(&rdata, &ns, NULL);
				if (result != ISC_R_SUCCESS)
					continue;

				pns = isc_mem_get(mctx, sizeof(*pns));
				if (pns == NULL) {
					fprintf(stderr,
						"resolve_ns: mem_get failed");
					result = ISC_R_NOMEMORY;
					POST(result);
					/*
					 * XXX: should we continue with the
					 * available servers anyway?
					 */
					goto cleanup;
				}

				dns_fixedname_init(&pns->fixedname);
				pns->name =
					dns_fixedname_name(&pns->fixedname);
				ISC_LINK_INIT(pns, link);
				ISC_LIST_APPEND(trans->nslist, pns, link);
				ISC_LIST_INIT(pns->servers);

				dns_name_copy(&ns.name, pns->name, NULL);
				dns_rdata_reset(&rdata);
				dns_rdata_freestruct(&ns);
			}
		}
	}

 cleanup:
	dns_client_freeresanswer(client, &rev->answerlist);
	dns_client_destroyrestrans(&trans->resid);
	isc_event_free(&event);

	if (!ISC_LIST_EMPTY(trans->nslist)) {
		/* Go get addresses of NSes */
		trans->current_ns = ISC_LIST_HEAD(trans->nslist);
		result = fetch_nsaddress(trans);
	} else
		result = ISC_R_FAILURE;

	if (result == ISC_R_SUCCESS)
		return;

	reset_probe(trans);
}

static isc_result_t
probe_domain(struct probe_trans *trans) {
	isc_result_t result;
	size_t domainlen;
	isc_buffer_t b;
	char buf[4096];	/* XXX ad hoc constant, but should be enough */
	char *cp;

	REQUIRE(trans != NULL);
	REQUIRE(trans->inuse == ISC_FALSE);
	REQUIRE(outstanding_probes < MAX_PROBES);

	/* Construct domain */
	cp = fgets(buf, sizeof(buf), input);
	if (cp == NULL)
		return (ISC_R_NOMORE);
	if ((cp = strchr(buf, '\n')) != NULL) /* zap NL if any */
		*cp = '\0';
	trans->domain = isc_mem_strdup(mctx, buf);
	if (trans->domain == NULL) {
		fprintf(stderr,
			"failed to allocate memory for domain: %s", cp);
		return (ISC_R_NOMEMORY);
	}

	/* Start getting NS for the domain */
	domainlen = strlen(buf);
	isc_buffer_init(&b, buf, domainlen);
	isc_buffer_add(&b, domainlen);
	dns_fixedname_init(&trans->fixedname);
	trans->qname = dns_fixedname_name(&trans->fixedname);
	result = dns_name_fromtext(trans->qname, &b, dns_rootname, 0, NULL);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
	result = dns_client_startresolve(client, trans->qname,
					 dns_rdataclass_in, dns_rdatatype_ns,
					 0, probe_task, resolve_ns, trans,
					 &trans->resid);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	trans->inuse = ISC_TRUE;
	outstanding_probes++;

	return (ISC_R_SUCCESS);

 cleanup:
	isc_mem_free(mctx, trans->domain);
	dns_fixedname_invalidate(&trans->fixedname);

	return (result);
}

ISC_PLATFORM_NORETURN_PRE static void
usage(void) ISC_PLATFORM_NORETURN_POST;

static void
usage(void) {
	fprintf(stderr, "usage: nsprobe [-d] [-v [-v...]] [-c cache_address] "
		"[input_file]\n");

	exit(1);
}

int
main(int argc, char *argv[]) {
	int i, ch, error;
	struct addrinfo hints, *res;
	isc_result_t result;
	isc_sockaddr_t sa;
	isc_sockaddrlist_t servers;
	isc_taskmgr_t *taskmgr = NULL;
	isc_socketmgr_t *socketmgr = NULL;
	isc_timermgr_t *timermgr = NULL;

	while ((ch = getopt(argc, argv, "c:dhv")) != -1) {
		switch (ch) {
		case 'c':
			cacheserver = optarg;
			break;
		case 'd':
			debug_mode = ISC_TRUE;
			break;
		case 'h':
			usage();
			break;
		case 'v':
			verbose_level++;
			break;
		default:
			usage();
			break;
		}
	}

	argc -= optind;
	argv += optind;

	/* Common set up */
	isc_lib_register();
	result = dns_lib_init();
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "dns_lib_init failed: %d\n", result);
		exit(1);
	}

	result = ctxs_init(&mctx, &actx, &taskmgr, &socketmgr,
			   &timermgr);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "ctx create failed: %d\n", result);
		exit(1);
	}

	isc_app_ctxstart(actx);

	result = dns_client_createx(mctx, actx, taskmgr, socketmgr,
				    timermgr, 0, &client);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "dns_client_createx failed: %d\n", result);
		exit(1);
	}

	/* Set local cache server */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	error = getaddrinfo(cacheserver, "53", &hints, &res);
	if (error != 0) {
		fprintf(stderr, "failed to convert server name (%s): %s\n",
			cacheserver, gai_strerror(error));
		exit(1);
	}

	if (res->ai_addrlen > sizeof(sa.type)) {
		fprintf(stderr,
			"assumption failure: addrlen is too long: %ld\n",
			(long)res->ai_addrlen);
		exit(1);
	}
	memmove(&sa.type.sa, res->ai_addr, res->ai_addrlen);
	sa.length = res->ai_addrlen;
	freeaddrinfo(res);
	ISC_LINK_INIT(&sa, link);
	ISC_LIST_INIT(servers);
	ISC_LIST_APPEND(servers, &sa, link);
	result = dns_client_setservers(client, dns_rdataclass_in, NULL,
				       &servers);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "failed to set server: %d\n", result);
		exit(1);
	}

	/* Create the main task */
	probe_task = NULL;
	result = isc_task_create(taskmgr, 0, &probe_task);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "failed to create task: %d\n", result);
		exit(1);
	}

	/* Open input file */
	if (argc == 0)
		input = stdin;
	else {
		input = fopen(argv[0], "r");
		if (input == NULL) {
			fprintf(stderr, "failed to open input file: %s\n",
				argv[0]);
			exit(1);
		}
	}

	/* Set up and start probe */
	for (i = 0; i < MAX_PROBES; i++) {
		probes[i].inuse = ISC_FALSE;
		probes[i].domain = NULL;
		dns_fixedname_init(&probes[i].fixedname);
		probes[i].qname = NULL;
		probes[i].qlabel = qlabels;
		probes[i].qname_found = ISC_FALSE;
		probes[i].resid = NULL;
		ISC_LIST_INIT(probes[i].nslist);
		probes[i].reqid = NULL;

		probes[i].qmessage = NULL;
		result = dns_message_create(mctx, DNS_MESSAGE_INTENTRENDER,
					    &probes[i].qmessage);
		if (result == ISC_R_SUCCESS) {
			result = dns_message_create(mctx,
						    DNS_MESSAGE_INTENTPARSE,
						    &probes[i].rmessage);
		}
		if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "initialization failure\n");
			exit(1);
		}
	}
	for (i = 0; i < MAX_PROBES; i++) {
		result = probe_domain(&probes[i]);
		if (result == ISC_R_NOMORE)
			break;
		else if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "failed to issue an initial probe\n");
			exit(1);
		}
	}

	/* Start event loop */
	isc_app_ctxrun(actx);

	/* Dump results */
	printf("Per domain results (out of %lu domains):\n",
	       number_of_domains);
	printf("  valid: %lu\n"
	       "  ignore: %lu\n"
	       "  nxdomain: %lu\n"
	       "  othererr: %lu\n"
	       "  multiplesoa: %lu\n"
	       "  multiplecname: %lu\n"
	       "  brokenanswer: %lu\n"
	       "  lame: %lu\n"
	       "  unknown: %lu\n"
	       "  multiple errors: %lu\n",
	       domain_stat.valid, domain_stat.ignore, domain_stat.nxdomain,
	       domain_stat.othererr, domain_stat.multiplesoa,
	       domain_stat.multiplecname, domain_stat.brokenanswer,
	       domain_stat.lame, domain_stat.unknown, multiple_error_domains);
	printf("Per server results (out of %lu servers):\n",
	       number_of_servers);
	printf("  valid: %lu\n"
	       "  ignore: %lu\n"
	       "  nxdomain: %lu\n"
	       "  othererr: %lu\n"
	       "  multiplesoa: %lu\n"
	       "  multiplecname: %lu\n"
	       "  brokenanswer: %lu\n"
	       "  lame: %lu\n"
	       "  unknown: %lu\n",
	       server_stat.valid, server_stat.ignore, server_stat.nxdomain,
	       server_stat.othererr, server_stat.multiplesoa,
	       server_stat.multiplecname, server_stat.brokenanswer,
	       server_stat.lame, server_stat.unknown);

	/* Cleanup */
	for (i = 0; i < MAX_PROBES; i++) {
		dns_message_destroy(&probes[i].qmessage);
		dns_message_destroy(&probes[i].rmessage);
	}
	isc_task_detach(&probe_task);
	dns_client_destroy(&client);
	dns_lib_shutdown();
	isc_app_ctxfinish(actx);
	ctxs_destroy(&mctx, &actx, &taskmgr, &socketmgr, &timermgr);

	return (0);
}
