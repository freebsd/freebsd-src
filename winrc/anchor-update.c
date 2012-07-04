/*
 * winrc/anchor-update.c - windows trust anchor update util
 *
 * Copyright (c) 2009, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 *
 * This file is made because contrib/update-anchor.sh does not work on
 * windows (no shell). 
 */
#include "config.h"
#include <ldns/ldns.h>
#include "libunbound/unbound.h"

/** usage */
static void
usage(void)
{
	printf("usage: { name-of-domain filename }+ \n");
	printf("exit codes: 0 anchors updated, 1 no changes, 2 errors.\n");
	exit(1);
}

/** fatal exit */
static void fatal(const char* str)
{
	printf("fatal error: %s\n", str);
	exit(2);
}

/** lookup data */
static struct ub_result*
do_lookup(struct ub_ctx* ctx, char* domain)
{
	struct ub_result* result = NULL;
	int r;
	r = ub_resolve(ctx, domain, LDNS_RR_TYPE_DNSKEY, LDNS_RR_CLASS_IN,
		&result);
	if(r) {
		printf("failed to lookup %s\n", ub_strerror(r));
		fatal("ub_resolve failed");
	}
	if(!result->havedata && (result->rcode == LDNS_RCODE_SERVFAIL ||
		result->rcode == LDNS_RCODE_REFUSED))
		return NULL; /* probably no internet connection */
	if(!result->havedata) fatal("result has no data");
	if(!result->secure) fatal("result is not secure");
	return result;
}

/** get answer into ldns rr list */
static ldns_rr_list*
result2answer(struct ub_result* result)
{
	ldns_pkt* p = NULL;
	ldns_rr_list* a;
	if(ldns_wire2pkt(&p, result->answer_packet, (size_t)result->answer_len) 
		!= LDNS_STATUS_OK) 
		return NULL;
	a = ldns_pkt_answer(p);
	ldns_pkt_set_answer(p, NULL);
	ldns_pkt_free(p);
	return a;
}

/** print result to file */
static void
do_print(struct ub_result* result, char* file)
{
	FILE* out;
	ldns_rr_list* list = result2answer(result);
	if(!list) fatal("result2answer failed");
	
	out = fopen(file, "w");
	if(!out) {
		perror(file);
		fatal("fopen failed");
	}
	ldns_rr_list_print(out, list);
	fclose(out);
	ldns_rr_list_deep_free(list);
}

/** update domain to file */
static int
do_update(char* domain, char* file)
{
	struct ub_ctx* ctx;
	struct ub_result* result;
	int r;
	printf("updating %s to %s\n", domain, file);
	ctx = ub_ctx_create();
	if(!ctx) fatal("ub_ctx_create failed");

	if((r=ub_ctx_add_ta_file(ctx, file))) {
		printf("%s\n", ub_strerror(r));
		fatal("ub_ctx_add_ta_file failed");
	}

	if(!(result=do_lookup(ctx, domain))) {
		ub_ctx_delete(ctx);
		return 1;
	}
	ub_ctx_delete(ctx);
	do_print(result, file);
	ub_resolve_free(result);
	return 0;
}

/** anchor update main */
int main(int argc, char** argv)
{
	int retcode = 1;
	if(argc == 1) {
		usage();
	}
	argc--;
	argv++;
	while(argc > 0) {
		int r = do_update(argv[0], argv[1]);
		if(r == 0) retcode = 0;

		/* next */
		argc-=2;
		argv+=2;
	}
	return retcode;
}
