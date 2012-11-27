/*
 * Copyright (C) 2009  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: sample-async.c,v 1.5 2009/09/29 15:06:07 fdupont Exp $ */

#include <config.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <isc/app.h>
#include <isc/buffer.h>
#include <isc/lib.h>
#include <isc/mem.h>
#include <isc/socket.h>
#include <isc/sockaddr.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <dns/client.h>
#include <dns/fixedname.h>
#include <dns/lib.h>
#include <dns/name.h>
#include <dns/rdataset.h>
#include <dns/rdatatype.h>
#include <dns/result.h>

#define MAX_SERVERS 10
#define MAX_QUERIES 100

static dns_client_t *client = NULL;
static isc_task_t *query_task = NULL;
static isc_appctx_t *query_actx = NULL;
static unsigned int outstanding_queries = 0;
static const char *def_server = "127.0.0.1";
static FILE *fp;

struct query_trans {
	int id;
	isc_boolean_t inuse;
	dns_rdatatype_t type;
	dns_fixedname_t fixedname;
	dns_name_t *qname;
	dns_namelist_t answerlist;
	dns_clientrestrans_t *xid;
};

static struct query_trans query_array[MAX_QUERIES];

static isc_result_t dispatch_query(struct query_trans *trans);

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

static isc_result_t
printdata(dns_rdataset_t *rdataset, dns_name_t *owner) {
	isc_buffer_t target;
	isc_result_t result;
	isc_region_t r;
	char t[4096];

	isc_buffer_init(&target, t, sizeof(t));

	if (!dns_rdataset_isassociated(rdataset))
		return (ISC_R_SUCCESS);
	result = dns_rdataset_totext(rdataset, owner, ISC_FALSE, ISC_FALSE,
				     &target);
	if (result != ISC_R_SUCCESS)
		return (result);
	isc_buffer_usedregion(&target, &r);
	printf("  %.*s", (int)r.length, (char *)r.base);

	return (ISC_R_SUCCESS);
}

static void
process_answer(isc_task_t *task, isc_event_t *event) {
	struct query_trans *trans = event->ev_arg;
	dns_clientresevent_t *rev = (dns_clientresevent_t *)event;
	dns_name_t *name;
	dns_rdataset_t *rdataset;
	isc_result_t result;

	REQUIRE(task == query_task);
	REQUIRE(trans->inuse == ISC_TRUE);
	REQUIRE(outstanding_queries > 0);

	printf("answer[%2d]\n", trans->id);

	if (rev->result != ISC_R_SUCCESS)
		printf("  failed: %d(%s)\n", rev->result,
		       dns_result_totext(rev->result));

	for (name = ISC_LIST_HEAD(rev->answerlist); name != NULL;
	     name = ISC_LIST_NEXT(name, link)) {
		for (rdataset = ISC_LIST_HEAD(name->list);
		     rdataset != NULL;
		     rdataset = ISC_LIST_NEXT(rdataset, link)) {
			(void)printdata(rdataset, name);
		}
	}

	dns_client_freeresanswer(client, &rev->answerlist);
	dns_client_destroyrestrans(&trans->xid);

	isc_event_free(&event);

	trans->inuse = ISC_FALSE;
	dns_fixedname_invalidate(&trans->fixedname);
	trans->qname = NULL;
	outstanding_queries--;

	result = dispatch_query(trans);
#if 0				/* for cancel test */
	if (result == ISC_R_SUCCESS) {
		static int count = 0;

		if ((++count) % 10 == 0)
			dns_client_cancelresolve(trans->xid);
	}
#endif
	if (result == ISC_R_NOMORE && outstanding_queries == 0)
		isc_app_ctxshutdown(query_actx);
}

static isc_result_t
dispatch_query(struct query_trans *trans) {
	isc_result_t result;
	size_t namelen;
	isc_buffer_t b;
	char buf[4096];	/* XXX ad hoc constant, but should be enough */
	char *cp;

	REQUIRE(trans != NULL);
	REQUIRE(trans->inuse == ISC_FALSE);
	REQUIRE(ISC_LIST_EMPTY(trans->answerlist));
	REQUIRE(outstanding_queries < MAX_QUERIES);

	/* Construct qname */
	cp = fgets(buf, sizeof(buf), fp);
	if (cp == NULL)
		return (ISC_R_NOMORE);
	/* zap NL if any */
	if ((cp = strchr(buf, '\n')) != NULL)
		*cp = '\0';
	namelen = strlen(buf);
	isc_buffer_init(&b, buf, namelen);
	isc_buffer_add(&b, namelen);
	dns_fixedname_init(&trans->fixedname);
	trans->qname = dns_fixedname_name(&trans->fixedname);
	result = dns_name_fromtext(trans->qname, &b, dns_rootname, 0, NULL);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	/* Start resolution */
	result = dns_client_startresolve(client, trans->qname,
					 dns_rdataclass_in, trans->type, 0,
					 query_task, process_answer, trans,
					 &trans->xid);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	trans->inuse = ISC_TRUE;
	outstanding_queries++;

	return (ISC_R_SUCCESS);

 cleanup:
	dns_fixedname_invalidate(&trans->fixedname);

	return (result);
}

ISC_PLATFORM_NORETURN_PRE static void
usage(void) ISC_PLATFORM_NORETURN_POST;

static void
usage(void) {
	fprintf(stderr, "usage: sample-async [-s server_address] [-t RR type] "
		"input_file\n");

	exit(1);
}

int
main(int argc, char *argv[]) {
	int ch;
	isc_textregion_t tr;
	isc_mem_t *mctx = NULL;
	isc_taskmgr_t *taskmgr = NULL;
	isc_socketmgr_t *socketmgr = NULL;
	isc_timermgr_t *timermgr = NULL;
	int nservers = 0;
	const char *serveraddr[MAX_SERVERS];
	isc_sockaddr_t sa[MAX_SERVERS];
	isc_sockaddrlist_t servers;
	dns_rdatatype_t type = dns_rdatatype_a;
	struct in_addr inaddr;
	isc_result_t result;
	int i;

	while ((ch = getopt(argc, argv, "s:t:")) != -1) {
		switch (ch) {
		case 't':
			tr.base = optarg;
			tr.length = strlen(optarg);
			result = dns_rdatatype_fromtext(&type, &tr);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr,
					"invalid RRtype: %s\n", optarg);
				exit(1);
			}
			break;
		case 's':
			if (nservers == MAX_SERVERS) {
				fprintf(stderr,
					"too many servers (up to %d)\n",
					MAX_SERVERS);
				exit(1);
			}
			serveraddr[nservers++] = (const char *)optarg;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc < 1)
		usage();

	if (nservers == 0) {
		nservers = 1;
		serveraddr[0] = def_server;
	}

	for (i = 0; i < MAX_QUERIES; i++) {
		query_array[i].id = i;
		query_array[i].inuse = ISC_FALSE;
		query_array[i].type = type;
		dns_fixedname_init(&query_array[i].fixedname);
		query_array[i].qname = NULL;
		ISC_LIST_INIT(query_array[i].answerlist);
		query_array[i].xid = NULL;
	}

	isc_lib_register();
	result = dns_lib_init();
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "dns_lib_init failed: %d\n", result);
		exit(1);
	}

	result = ctxs_init(&mctx, &query_actx, &taskmgr, &socketmgr,
			   &timermgr);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "ctx create failed: %d\n", result);
		exit(1);
	}

	isc_app_ctxstart(query_actx);

	result = dns_client_createx(mctx, query_actx, taskmgr, socketmgr,
				    timermgr, 0, &client);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "dns_client_createx failed: %d\n", result);
		exit(1);
	}

	/* Set nameservers */
	ISC_LIST_INIT(servers);
	for (i = 0; i < nservers; i++) {
		if (inet_pton(AF_INET, serveraddr[i], &inaddr) != 1) {
			fprintf(stderr, "failed to parse IPv4 address %s\n",
				serveraddr[i]);
			exit(1);
		}
		isc_sockaddr_fromin(&sa[i], &inaddr, 53);
		ISC_LIST_APPEND(servers, &sa[i], link);
	}
	result = dns_client_setservers(client, dns_rdataclass_in, NULL,
				       &servers);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "set server failed: %d\n", result);
		exit(1);
	}

	/* Create the main task */
	query_task = NULL;
	result = isc_task_create(taskmgr, 0, &query_task);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "failed to create task: %d\n", result);
		exit(1);
	}

	/* Open input file */
	fp = fopen(argv[0], "r");
	if (fp == NULL) {
		fprintf(stderr, "failed to open input file: %s\n", argv[1]);
		exit(1);
	}

	/* Dispatch initial queries */
	for (i = 0; i < MAX_QUERIES; i++) {
		result = dispatch_query(&query_array[i]);
		if (result == ISC_R_NOMORE)
			break;
	}

	/* Start event loop */
	isc_app_ctxrun(query_actx);

	/* Sanity check */
	for (i = 0; i < MAX_QUERIES; i++)
		INSIST(query_array[i].inuse == ISC_FALSE);

	/* Cleanup */
	isc_task_detach(&query_task);
	dns_client_destroy(&client);
	dns_lib_shutdown();
	isc_app_ctxfinish(query_actx);
	ctxs_destroy(&mctx, &query_actx, &taskmgr, &socketmgr, &timermgr);

	exit(0);
}
