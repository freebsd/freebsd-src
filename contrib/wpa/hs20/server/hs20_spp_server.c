/*
 * Hotspot 2.0 SPP server - standalone version
 * Copyright (c) 2012-2013, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <time.h>
#include <sqlite3.h>

#include "common.h"
#include "common/version.h"
#include "xml-utils.h"
#include "spp_server.h"


static void write_timestamp(FILE *f)
{
	time_t t;
	struct tm *tm;

	time(&t);
	tm = localtime(&t);

	fprintf(f, "%04u-%02u-%02u %02u:%02u:%02u ",
		tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);
}


void debug_print(struct hs20_svc *ctx, int print, const char *fmt, ...)
{
	va_list ap;

	if (ctx->debug_log == NULL)
		return;

	write_timestamp(ctx->debug_log);
	va_start(ap, fmt);
	vfprintf(ctx->debug_log, fmt, ap);
	va_end(ap);

	fprintf(ctx->debug_log, "\n");
}


void debug_dump_node(struct hs20_svc *ctx, const char *title, xml_node_t *node)
{
	char *str;

	if (ctx->debug_log == NULL)
		return;
	str = xml_node_to_str(ctx->xml, node);
	if (str == NULL)
		return;

	write_timestamp(ctx->debug_log);
	fprintf(ctx->debug_log, "%s: '%s'\n", title, str);
	os_free(str);
}


static int process(struct hs20_svc *ctx)
{
	int dmacc = 0;
	xml_node_t *soap, *spp, *resp;
	char *user, *realm, *post, *str;

	ctx->addr = getenv("HS20ADDR");
	if (ctx->addr)
		debug_print(ctx, 1, "Connection from %s", ctx->addr);
	ctx->test = getenv("HS20TEST");
	if (ctx->test)
		debug_print(ctx, 1, "Requested test functionality: %s",
			    ctx->test);

	user = getenv("HS20USER");
	if (user && strlen(user) == 0)
		user = NULL;
	realm = getenv("HS20REALM");
	if (realm == NULL) {
		debug_print(ctx, 1, "HS20REALM not set");
		return -1;
	}
	post = getenv("HS20POST");
	if (post == NULL) {
		debug_print(ctx, 1, "HS20POST not set");
		return -1;
	}

	ctx->imsi = getenv("HS20IMSI");
	if (ctx->imsi)
		debug_print(ctx, 1, "IMSI %s", ctx->imsi);

	ctx->eap_method = getenv("HS20EAPMETHOD");
	if (ctx->eap_method)
		debug_print(ctx, 1, "EAP method %s", ctx->eap_method);

	ctx->id_hash = getenv("HS20IDHASH");
	if (ctx->id_hash)
		debug_print(ctx, 1, "ID-HASH %s", ctx->id_hash);

	soap = xml_node_from_buf(ctx->xml, post);
	if (soap == NULL) {
		debug_print(ctx, 1, "Could not parse SOAP data");
		return -1;
	}
	debug_dump_node(ctx, "Received SOAP message", soap);
	spp = soap_get_body(ctx->xml, soap);
	if (spp == NULL) {
		debug_print(ctx, 1, "Could not get SPP message");
		xml_node_free(ctx->xml, soap);
		return -1;
	}
	debug_dump_node(ctx, "Received SPP message", spp);

	resp = hs20_spp_server_process(ctx, spp, user, realm, dmacc);
	xml_node_free(ctx->xml, soap);
	if (resp == NULL && user == NULL) {
		debug_print(ctx, 1, "Request HTTP authentication");
		return 2; /* Request authentication */
	}
	if (resp == NULL) {
		debug_print(ctx, 1, "No response");
		return -1;
	}

	soap = soap_build_envelope(ctx->xml, resp);
	if (soap == NULL) {
		debug_print(ctx, 1, "SOAP envelope building failed");
		return -1;
	}
	str = xml_node_to_str(ctx->xml, soap);
	xml_node_free(ctx->xml, soap);
	if (str == NULL) {
		debug_print(ctx, 1, "Could not get node string");
		return -1;
	}
	printf("%s", str);
	free(str);

	return 0;
}


static void usage(void)
{
	printf("usage:\n"
	       "hs20_spp_server -r<root directory> [-f<debug log>]\n");
}


int main(int argc, char *argv[])
{
	struct hs20_svc ctx;
	int ret;

	os_memset(&ctx, 0, sizeof(ctx));
	for (;;) {
		int c = getopt(argc, argv, "f:r:v");
		if (c < 0)
			break;
		switch (c) {
		case 'f':
			if (ctx.debug_log)
				break;
			ctx.debug_log = fopen(optarg, "a");
			if (ctx.debug_log == NULL) {
				printf("Could not write to %s\n", optarg);
				return -1;
			}
			break;
		case 'r':
			ctx.root_dir = optarg;
			break;
		case 'v':
			printf("hs20_spp_server v%s\n", VERSION_STR);
			return 0;
		default:
			usage();
			return -1;
		}
	}
	if (ctx.root_dir == NULL) {
		usage();
		return -1;
	}
	ctx.xml = xml_node_init_ctx(&ctx, NULL);
	if (ctx.xml == NULL)
		return -1;
	if (hs20_spp_server_init(&ctx) < 0) {
		xml_node_deinit_ctx(ctx.xml);
		return -1;
	}

	ret = process(&ctx);
	debug_print(&ctx, 1, "process() --> %d", ret);

	xml_node_deinit_ctx(ctx.xml);
	hs20_spp_server_deinit(&ctx);
	if (ctx.debug_log)
		fclose(ctx.debug_log);

	return ret;
}
