/*
 * HTTP wrapper
 * Copyright (c) 2012-2013, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

struct http_ctx;

struct http_ctx * http_init_ctx(void *upper_ctx, struct xml_node_ctx *xml_ctx);
void http_ocsp_set(struct http_ctx *ctx, int val);
void http_deinit_ctx(struct http_ctx *ctx);

int http_download_file(struct http_ctx *ctx, const char *url,
		       const char *fname, const char *ca_fname);
char * http_post(struct http_ctx *ctx, const char *url, const char *data,
		 const char *content_type, const char *ext_hdr,
		 const char *ca_fname,
		 const char *username, const char *password,
		 const char *client_cert, const char *client_key,
		 size_t *resp_len);
const char * http_get_err(struct http_ctx *ctx);

#endif /* HTTP_UTILS_H */
