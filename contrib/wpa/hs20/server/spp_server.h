/*
 * Hotspot 2.0 SPP server
 * Copyright (c) 2012-2013, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef SPP_SERVER_H
#define SPP_SERVER_H

struct hs20_svc {
	const void *ctx;
	struct xml_node_ctx *xml;
	char *root_dir;
	FILE *debug_log;
	sqlite3 *db;
	const char *addr;
	const char *test;
	const char *imsi;
	const char *eap_method;
	const char *id_hash;
};


void debug_print(struct hs20_svc *ctx, int print, const char *fmt, ...)
	__attribute__ ((format (printf, 3, 4)));
void debug_dump_node(struct hs20_svc *ctx, const char *title, xml_node_t *node);

xml_node_t * hs20_spp_server_process(struct hs20_svc *ctx, xml_node_t *node,
				     const char *auth_user,
				     const char *auth_realm, int dmacc);
int hs20_spp_server_init(struct hs20_svc *ctx);
void hs20_spp_server_deinit(struct hs20_svc *ctx);

#endif /* SPP_SERVER_H */
