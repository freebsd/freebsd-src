/*
 * HTTP wrapper for libcurl
 * Copyright (c) 2012-2014, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <curl/curl.h>
#ifdef EAP_TLS_OPENSSL
#include <openssl/ssl.h>
#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/x509v3.h>

#ifdef SSL_set_tlsext_status_type
#ifndef OPENSSL_NO_TLSEXT
#define HAVE_OCSP
#include <openssl/err.h>
#include <openssl/ocsp.h>
#endif /* OPENSSL_NO_TLSEXT */
#endif /* SSL_set_tlsext_status_type */
#endif /* EAP_TLS_OPENSSL */

#include "common.h"
#include "xml-utils.h"
#include "http-utils.h"
#ifdef EAP_TLS_OPENSSL
#include "crypto/tls_openssl.h"
#endif /* EAP_TLS_OPENSSL */


struct http_ctx {
	void *ctx;
	struct xml_node_ctx *xml;
	CURL *curl;
	struct curl_slist *curl_hdr;
	char *svc_address;
	char *curl_buf;
	size_t curl_buf_len;

	enum {
		NO_OCSP, OPTIONAL_OCSP, MANDATORY_OCSP
	} ocsp;
	X509 *peer_cert;
	X509 *peer_issuer;
	X509 *peer_issuer_issuer;

	const char *last_err;
	const char *url;
};


static void clear_curl(struct http_ctx *ctx)
{
	if (ctx->curl) {
		curl_easy_cleanup(ctx->curl);
		ctx->curl = NULL;
	}
	if (ctx->curl_hdr) {
		curl_slist_free_all(ctx->curl_hdr);
		ctx->curl_hdr = NULL;
	}
}


static void debug_dump(struct http_ctx *ctx, const char *title,
		       const char *buf, size_t len)
{
	char *txt;
	size_t i;

	for (i = 0; i < len; i++) {
		if (buf[i] < 32 && buf[i] != '\t' && buf[i] != '\n' &&
		    buf[i] != '\r') {
			wpa_hexdump_ascii(MSG_MSGDUMP, title, buf, len);
			return;
		}
	}

	txt = os_malloc(len + 1);
	if (txt == NULL)
		return;
	os_memcpy(txt, buf, len);
	txt[len] = '\0';
	while (len > 0) {
		len--;
		if (txt[len] == '\n' || txt[len] == '\r')
			txt[len] = '\0';
		else
			break;
	}
	wpa_printf(MSG_MSGDUMP, "%s[%s]", title, txt);
	os_free(txt);
}


static int curl_cb_debug(CURL *curl, curl_infotype info, char *buf, size_t len,
			 void *userdata)
{
	struct http_ctx *ctx = userdata;
	switch (info) {
	case CURLINFO_TEXT:
		debug_dump(ctx, "CURLINFO_TEXT", buf, len);
		break;
	case CURLINFO_HEADER_IN:
		debug_dump(ctx, "CURLINFO_HEADER_IN", buf, len);
		break;
	case CURLINFO_HEADER_OUT:
		debug_dump(ctx, "CURLINFO_HEADER_OUT", buf, len);
		break;
	case CURLINFO_DATA_IN:
		debug_dump(ctx, "CURLINFO_DATA_IN", buf, len);
		break;
	case CURLINFO_DATA_OUT:
		debug_dump(ctx, "CURLINFO_DATA_OUT", buf, len);
		break;
	case CURLINFO_SSL_DATA_IN:
		wpa_printf(MSG_DEBUG, "debug - CURLINFO_SSL_DATA_IN - %d",
			   (int) len);
		break;
	case CURLINFO_SSL_DATA_OUT:
		wpa_printf(MSG_DEBUG, "debug - CURLINFO_SSL_DATA_OUT - %d",
			   (int) len);
		break;
	case CURLINFO_END:
		wpa_printf(MSG_DEBUG, "debug - CURLINFO_END - %d",
			   (int) len);
		break;
	}
	return 0;
}


static size_t curl_cb_write(void *ptr, size_t size, size_t nmemb,
			    void *userdata)
{
	struct http_ctx *ctx = userdata;
	char *n;
	n = os_realloc(ctx->curl_buf, ctx->curl_buf_len + size * nmemb + 1);
	if (n == NULL)
		return 0;
	ctx->curl_buf = n;
	os_memcpy(n + ctx->curl_buf_len, ptr, size * nmemb);
	n[ctx->curl_buf_len + size * nmemb] = '\0';
	ctx->curl_buf_len += size * nmemb;
	return size * nmemb;
}


#ifdef EAP_TLS_OPENSSL

static void debug_dump_cert(const char *title, X509 *cert)
{
	BIO *out;
	char *txt;
	size_t rlen;

	out = BIO_new(BIO_s_mem());
	if (!out)
		return;

	X509_print_ex(out, cert, XN_FLAG_COMPAT, X509_FLAG_COMPAT);
	rlen = BIO_ctrl_pending(out);
	txt = os_malloc(rlen + 1);
	if (txt) {
		int res = BIO_read(out, txt, rlen);
		if (res > 0) {
			txt[res] = '\0';
			wpa_printf(MSG_MSGDUMP, "%s:\n%s", title, txt);
		}
		os_free(txt);
	}
	BIO_free(out);
}


static int curl_cb_ssl_verify(int preverify_ok, X509_STORE_CTX *x509_ctx)
{
	struct http_ctx *ctx;
	X509 *cert;
	int err, depth;
	char buf[256];
	X509_NAME *name;
	const char *err_str;
	SSL *ssl;
	SSL_CTX *ssl_ctx;

	ssl = X509_STORE_CTX_get_ex_data(x509_ctx,
					 SSL_get_ex_data_X509_STORE_CTX_idx());
	ssl_ctx = SSL_get_SSL_CTX(ssl);
	ctx = SSL_CTX_get_app_data(ssl_ctx);

	wpa_printf(MSG_DEBUG, "curl_cb_ssl_verify, preverify_ok: %d",
		   preverify_ok);

	err = X509_STORE_CTX_get_error(x509_ctx);
	err_str = X509_verify_cert_error_string(err);
	depth = X509_STORE_CTX_get_error_depth(x509_ctx);
	cert = X509_STORE_CTX_get_current_cert(x509_ctx);
	if (!cert) {
		wpa_printf(MSG_INFO, "No server certificate available");
		ctx->last_err = "No server certificate available";
		return 0;
	}

	if (depth == 0)
		ctx->peer_cert = cert;
	else if (depth == 1)
		ctx->peer_issuer = cert;
	else if (depth == 2)
		ctx->peer_issuer_issuer = cert;

	name = X509_get_subject_name(cert);
	X509_NAME_oneline(name, buf, sizeof(buf));
	wpa_printf(MSG_INFO, "Server certificate chain - depth=%d err=%d (%s) subject=%s",
		   depth, err, err_str, buf);
	debug_dump_cert("Server certificate chain - certificate", cert);

#ifdef OPENSSL_IS_BORINGSSL
	if (depth == 0 && ctx->ocsp != NO_OCSP && preverify_ok) {
		enum ocsp_result res;

		res = check_ocsp_resp(ssl_ctx, ssl, cert, ctx->peer_issuer,
				      ctx->peer_issuer_issuer);
		if (res == OCSP_REVOKED) {
			preverify_ok = 0;
			wpa_printf(MSG_INFO, "OCSP: certificate revoked");
			if (err == X509_V_OK)
				X509_STORE_CTX_set_error(
					x509_ctx, X509_V_ERR_CERT_REVOKED);
		} else if (res != OCSP_GOOD && (ctx->ocsp == MANDATORY_OCSP)) {
			preverify_ok = 0;
			wpa_printf(MSG_INFO,
				   "OCSP: bad certificate status response");
		}
	}
#endif /* OPENSSL_IS_BORINGSSL */

	if (!preverify_ok)
		ctx->last_err = "TLS validation failed";

	return preverify_ok;
}


#ifdef HAVE_OCSP

static void ocsp_debug_print_resp(OCSP_RESPONSE *rsp)
{
	BIO *out;
	size_t rlen;
	char *txt;
	int res;

	out = BIO_new(BIO_s_mem());
	if (!out)
		return;

	OCSP_RESPONSE_print(out, rsp, 0);
	rlen = BIO_ctrl_pending(out);
	txt = os_malloc(rlen + 1);
	if (!txt) {
		BIO_free(out);
		return;
	}

	res = BIO_read(out, txt, rlen);
	if (res > 0) {
		txt[res] = '\0';
		wpa_printf(MSG_MSGDUMP, "OpenSSL: OCSP Response\n%s", txt);
	}
	os_free(txt);
	BIO_free(out);
}


static void tls_show_errors(const char *func, const char *txt)
{
	unsigned long err;

	wpa_printf(MSG_DEBUG, "OpenSSL: %s - %s %s",
		   func, txt, ERR_error_string(ERR_get_error(), NULL));

	while ((err = ERR_get_error())) {
		wpa_printf(MSG_DEBUG, "OpenSSL: pending error: %s",
			   ERR_error_string(err, NULL));
	}
}


static int ocsp_resp_cb(SSL *s, void *arg)
{
	struct http_ctx *ctx = arg;
	const unsigned char *p;
	int len, status, reason, res;
	OCSP_RESPONSE *rsp;
	OCSP_BASICRESP *basic;
	OCSP_CERTID *id;
	ASN1_GENERALIZEDTIME *produced_at, *this_update, *next_update;
	X509_STORE *store;
	STACK_OF(X509) *certs = NULL;

	len = SSL_get_tlsext_status_ocsp_resp(s, &p);
	if (!p) {
		wpa_printf(MSG_DEBUG, "OpenSSL: No OCSP response received");
		if (ctx->ocsp == MANDATORY_OCSP)
			ctx->last_err = "No OCSP response received";
		return (ctx->ocsp == MANDATORY_OCSP) ? 0 : 1;
	}

	wpa_hexdump(MSG_DEBUG, "OpenSSL: OCSP response", p, len);

	rsp = d2i_OCSP_RESPONSE(NULL, &p, len);
	if (!rsp) {
		wpa_printf(MSG_INFO, "OpenSSL: Failed to parse OCSP response");
		ctx->last_err = "Failed to parse OCSP response";
		return 0;
	}

	ocsp_debug_print_resp(rsp);

	status = OCSP_response_status(rsp);
	if (status != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
		wpa_printf(MSG_INFO, "OpenSSL: OCSP responder error %d (%s)",
			   status, OCSP_response_status_str(status));
		ctx->last_err = "OCSP responder error";
		return 0;
	}

	basic = OCSP_response_get1_basic(rsp);
	if (!basic) {
		wpa_printf(MSG_INFO, "OpenSSL: Could not find BasicOCSPResponse");
		ctx->last_err = "Could not find BasicOCSPResponse";
		return 0;
	}

	store = SSL_CTX_get_cert_store(SSL_get_SSL_CTX(s));
	if (ctx->peer_issuer) {
		wpa_printf(MSG_DEBUG, "OpenSSL: Add issuer");
		debug_dump_cert("OpenSSL: Issuer certificate",
				ctx->peer_issuer);

		if (X509_STORE_add_cert(store, ctx->peer_issuer) != 1) {
			tls_show_errors(__func__,
					"OpenSSL: Could not add issuer to certificate store");
		}
		certs = sk_X509_new_null();
		if (certs) {
			X509 *cert;
			cert = X509_dup(ctx->peer_issuer);
			if (cert && !sk_X509_push(certs, cert)) {
				tls_show_errors(
					__func__,
					"OpenSSL: Could not add issuer to OCSP responder trust store");
				X509_free(cert);
				sk_X509_free(certs);
				certs = NULL;
			}
			if (certs && ctx->peer_issuer_issuer) {
				cert = X509_dup(ctx->peer_issuer_issuer);
				if (cert && !sk_X509_push(certs, cert)) {
					tls_show_errors(
						__func__,
						"OpenSSL: Could not add issuer's issuer to OCSP responder trust store");
					X509_free(cert);
				}
			}
		}
	}

	status = OCSP_basic_verify(basic, certs, store, OCSP_TRUSTOTHER);
	sk_X509_pop_free(certs, X509_free);
	if (status <= 0) {
		tls_show_errors(__func__,
				"OpenSSL: OCSP response failed verification");
		OCSP_BASICRESP_free(basic);
		OCSP_RESPONSE_free(rsp);
		ctx->last_err = "OCSP response failed verification";
		return 0;
	}

	wpa_printf(MSG_DEBUG, "OpenSSL: OCSP response verification succeeded");

	if (!ctx->peer_cert) {
		wpa_printf(MSG_DEBUG, "OpenSSL: Peer certificate not available for OCSP status check");
		OCSP_BASICRESP_free(basic);
		OCSP_RESPONSE_free(rsp);
		ctx->last_err = "Peer certificate not available for OCSP status check";
		return 0;
	}

	if (!ctx->peer_issuer) {
		wpa_printf(MSG_DEBUG, "OpenSSL: Peer issuer certificate not available for OCSP status check");
		OCSP_BASICRESP_free(basic);
		OCSP_RESPONSE_free(rsp);
		ctx->last_err = "Peer issuer certificate not available for OCSP status check";
		return 0;
	}

	id = OCSP_cert_to_id(EVP_sha256(), ctx->peer_cert, ctx->peer_issuer);
	if (!id) {
		wpa_printf(MSG_DEBUG,
			   "OpenSSL: Could not create OCSP certificate identifier (SHA256)");
		OCSP_BASICRESP_free(basic);
		OCSP_RESPONSE_free(rsp);
		ctx->last_err = "Could not create OCSP certificate identifier";
		return 0;
	}

	res = OCSP_resp_find_status(basic, id, &status, &reason, &produced_at,
				    &this_update, &next_update);
	if (!res) {
		id = OCSP_cert_to_id(NULL, ctx->peer_cert, ctx->peer_issuer);
		if (!id) {
			wpa_printf(MSG_DEBUG,
				   "OpenSSL: Could not create OCSP certificate identifier (SHA1)");
			OCSP_BASICRESP_free(basic);
			OCSP_RESPONSE_free(rsp);
			ctx->last_err =
				"Could not create OCSP certificate identifier";
			return 0;
		}

		res = OCSP_resp_find_status(basic, id, &status, &reason,
					    &produced_at, &this_update,
					    &next_update);
	}

	if (!res) {
		wpa_printf(MSG_INFO, "OpenSSL: Could not find current server certificate from OCSP response%s",
			   (ctx->ocsp == MANDATORY_OCSP) ? "" :
			   " (OCSP not required)");
		OCSP_CERTID_free(id);
		OCSP_BASICRESP_free(basic);
		OCSP_RESPONSE_free(rsp);
		if (ctx->ocsp == MANDATORY_OCSP)

			ctx->last_err = "Could not find current server certificate from OCSP response";
		return (ctx->ocsp == MANDATORY_OCSP) ? 0 : 1;
	}
	OCSP_CERTID_free(id);

	if (!OCSP_check_validity(this_update, next_update, 5 * 60, -1)) {
		tls_show_errors(__func__, "OpenSSL: OCSP status times invalid");
		OCSP_BASICRESP_free(basic);
		OCSP_RESPONSE_free(rsp);
		ctx->last_err = "OCSP status times invalid";
		return 0;
	}

	OCSP_BASICRESP_free(basic);
	OCSP_RESPONSE_free(rsp);

	wpa_printf(MSG_DEBUG, "OpenSSL: OCSP status for server certificate: %s",
		   OCSP_cert_status_str(status));

	if (status == V_OCSP_CERTSTATUS_GOOD)
		return 1;
	if (status == V_OCSP_CERTSTATUS_REVOKED) {
		ctx->last_err = "Server certificate has been revoked";
		return 0;
	}
	if (ctx->ocsp == MANDATORY_OCSP) {
		wpa_printf(MSG_DEBUG, "OpenSSL: OCSP status unknown, but OCSP required");
		ctx->last_err = "OCSP status unknown";
		return 0;
	}
	wpa_printf(MSG_DEBUG, "OpenSSL: OCSP status unknown, but OCSP was not required, so allow connection to continue");
	return 1;
}


#if OPENSSL_VERSION_NUMBER < 0x10100000L
static SSL_METHOD patch_ssl_method;
static const SSL_METHOD *real_ssl_method;

static int curl_patch_ssl_new(SSL *s)
{
	SSL_CTX *ssl = SSL_get_SSL_CTX(s);
	int ret;

	ssl->method = real_ssl_method;
	s->method = real_ssl_method;

	ret = s->method->ssl_new(s);
	SSL_set_tlsext_status_type(s, TLSEXT_STATUSTYPE_ocsp);

	return ret;
}
#endif /* OpenSSL < 1.1.0 */

#endif /* HAVE_OCSP */


static CURLcode curl_cb_ssl(CURL *curl, void *sslctx, void *parm)
{
	struct http_ctx *ctx = parm;
	SSL_CTX *ssl = sslctx;

	wpa_printf(MSG_DEBUG, "curl_cb_ssl");
	SSL_CTX_set_app_data(ssl, ctx);
	SSL_CTX_set_verify(ssl, SSL_VERIFY_PEER, curl_cb_ssl_verify);

#ifdef HAVE_OCSP
	if (ctx->ocsp != NO_OCSP) {
		SSL_CTX_set_tlsext_status_cb(ssl, ocsp_resp_cb);
		SSL_CTX_set_tlsext_status_arg(ssl, ctx);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
		/*
		 * Use a temporary SSL_METHOD to get a callback on SSL_new()
		 * from libcurl since there is no proper callback registration
		 * available for this.
		 */
		os_memset(&patch_ssl_method, 0, sizeof(patch_ssl_method));
		patch_ssl_method.ssl_new = curl_patch_ssl_new;
		real_ssl_method = ssl->method;
		ssl->method = &patch_ssl_method;
#endif /* OpenSSL < 1.1.0 */
	}
#endif /* HAVE_OCSP */

	return CURLE_OK;
}

#endif /* EAP_TLS_OPENSSL */


static CURL * setup_curl_post(struct http_ctx *ctx, const char *address,
			      const char *ca_fname, const char *username,
			      const char *password, const char *client_cert,
			      const char *client_key)
{
	CURL *curl;
#ifdef EAP_TLS_OPENSSL
	const char *extra = " tls=openssl";
#else /* EAP_TLS_OPENSSL */
	const char *extra = "";
#endif /* EAP_TLS_OPENSSL */

	wpa_printf(MSG_DEBUG, "Start HTTP client: address=%s ca_fname=%s "
		   "username=%s%s", address, ca_fname, username, extra);

	curl = curl_easy_init();
	if (curl == NULL)
		return NULL;

	curl_easy_setopt(curl, CURLOPT_URL, address);
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	if (ca_fname) {
		curl_easy_setopt(curl, CURLOPT_CAINFO, ca_fname);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
#ifdef EAP_TLS_OPENSSL
		curl_easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, curl_cb_ssl);
		curl_easy_setopt(curl, CURLOPT_SSL_CTX_DATA, ctx);
#if defined(OPENSSL_IS_BORINGSSL) || (OPENSSL_VERSION_NUMBER >= 0x10100000L)
		/* For now, using the CURLOPT_SSL_VERIFYSTATUS option only
		 * with BoringSSL since the OpenSSL specific callback hack to
		 * enable OCSP is not available with BoringSSL. The OCSP
		 * implementation within libcurl is not sufficient for the
		 * Hotspot 2.0 OSU needs, so cannot use this with OpenSSL.
		 */
		if (ctx->ocsp != NO_OCSP)
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 1L);
#endif /* OPENSSL_IS_BORINGSSL */
#endif /* EAP_TLS_OPENSSL */
	} else {
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	}
	if (client_cert && client_key) {
		curl_easy_setopt(curl, CURLOPT_SSLCERT, client_cert);
		curl_easy_setopt(curl, CURLOPT_SSLKEY, client_key);
	}
	/* TODO: use curl_easy_getinfo() with CURLINFO_CERTINFO to fetch
	 * information about the server certificate */
	curl_easy_setopt(curl, CURLOPT_CERTINFO, 1L);
	curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, curl_cb_debug);
	curl_easy_setopt(curl, CURLOPT_DEBUGDATA, ctx);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_cb_write);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, ctx);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	if (username) {
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANYSAFE);
		curl_easy_setopt(curl, CURLOPT_USERNAME, username);
		curl_easy_setopt(curl, CURLOPT_PASSWORD, password);
	}

	return curl;
}


static void free_curl_buf(struct http_ctx *ctx)
{
	os_free(ctx->curl_buf);
	ctx->curl_buf = NULL;
	ctx->curl_buf_len = 0;
}


struct http_ctx * http_init_ctx(void *upper_ctx, struct xml_node_ctx *xml_ctx)
{
	struct http_ctx *ctx;

	ctx = os_zalloc(sizeof(*ctx));
	if (ctx == NULL)
		return NULL;
	ctx->ctx = upper_ctx;
	ctx->xml = xml_ctx;
	ctx->ocsp = OPTIONAL_OCSP;

	curl_global_init(CURL_GLOBAL_ALL);

	return ctx;
}


void http_ocsp_set(struct http_ctx *ctx, int val)
{
	if (val == 0)
		ctx->ocsp = NO_OCSP;
	else if (val == 1)
		ctx->ocsp = OPTIONAL_OCSP;
	if (val == 2)
		ctx->ocsp = MANDATORY_OCSP;
}


void http_deinit_ctx(struct http_ctx *ctx)
{
	clear_curl(ctx);
	os_free(ctx->curl_buf);
	curl_global_cleanup();

	os_free(ctx->svc_address);

	os_free(ctx);
}


int http_download_file(struct http_ctx *ctx, const char *url,
		       const char *fname, const char *ca_fname)
{
	CURL *curl;
	FILE *f = NULL;
	CURLcode res;
	long http = 0;
	int ret = -1;

	ctx->last_err = NULL;
	ctx->url = url;

	wpa_printf(MSG_DEBUG, "curl: Download file from %s to %s (ca=%s)",
		   url, fname, ca_fname);
	curl = curl_easy_init();
	if (curl == NULL)
		goto fail;

	f = fopen(fname, "wb");
	if (!f)
		goto fail;

	curl_easy_setopt(curl, CURLOPT_URL, url);
	if (ca_fname) {
		curl_easy_setopt(curl, CURLOPT_CAINFO, ca_fname);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
		curl_easy_setopt(curl, CURLOPT_CERTINFO, 1L);
	} else {
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	}
	curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, curl_cb_debug);
	curl_easy_setopt(curl, CURLOPT_DEBUGDATA, ctx);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		if (!ctx->last_err)
			ctx->last_err = curl_easy_strerror(res);
		wpa_printf(MSG_ERROR, "curl_easy_perform() failed: %s",
			   ctx->last_err);
		goto fail;
	}

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
	wpa_printf(MSG_DEBUG, "curl: Server response code %ld", http);
	if (http != 200) {
		ctx->last_err = "HTTP download failed";
		wpa_printf(MSG_INFO, "HTTP download failed - code %ld", http);
		goto fail;
	}

	ret = 0;

fail:
	ctx->url = NULL;
	if (curl)
		curl_easy_cleanup(curl);
	if (f)
		fclose(f);

	return ret;
}


char * http_post(struct http_ctx *ctx, const char *url, const char *data,
		 const char *content_type, const char *ext_hdr,
		 const char *ca_fname,
		 const char *username, const char *password,
		 const char *client_cert, const char *client_key,
		 size_t *resp_len)
{
	long http = 0;
	CURLcode res;
	char *ret = NULL;
	CURL *curl;
	struct curl_slist *curl_hdr = NULL;

	ctx->last_err = NULL;
	ctx->url = url;
	wpa_printf(MSG_DEBUG, "curl: HTTP POST to %s", url);
	curl = setup_curl_post(ctx, url, ca_fname, username, password,
			       client_cert, client_key);
	if (curl == NULL)
		goto fail;

	if (content_type) {
		char ct[200];
		snprintf(ct, sizeof(ct), "Content-Type: %s", content_type);
		curl_hdr = curl_slist_append(curl_hdr, ct);
	}
	if (ext_hdr)
		curl_hdr = curl_slist_append(curl_hdr, ext_hdr);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_hdr);

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
	free_curl_buf(ctx);

	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		if (!ctx->last_err)
			ctx->last_err = curl_easy_strerror(res);
		wpa_printf(MSG_ERROR, "curl_easy_perform() failed: %s",
			   ctx->last_err);
		goto fail;
	}

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
	wpa_printf(MSG_DEBUG, "curl: Server response code %ld", http);
	if (http != 200) {
		ctx->last_err = "HTTP POST failed";
		wpa_printf(MSG_INFO, "HTTP POST failed - code %ld", http);
		goto fail;
	}

	if (ctx->curl_buf == NULL)
		goto fail;

	ret = ctx->curl_buf;
	if (resp_len)
		*resp_len = ctx->curl_buf_len;
	ctx->curl_buf = NULL;
	ctx->curl_buf_len = 0;

	wpa_printf(MSG_MSGDUMP, "Server response:\n%s", ret);

fail:
	free_curl_buf(ctx);
	ctx->url = NULL;
	return ret;
}


const char * http_get_err(struct http_ctx *ctx)
{
	return ctx->last_err;
}
