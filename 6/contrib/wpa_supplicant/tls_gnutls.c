/*
 * WPA Supplicant / SSL/TLS interface functions for openssl
 * Copyright (c) 2004-2006, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include "common.h"
#include "tls.h"


/*
 * It looks like gnutls does not provide access to client/server_random and
 * master_key. This is somewhat unfortunate since these are needed for key
 * derivation in EAP-{TLS,TTLS,PEAP,FAST}. Workaround for now is a horrible
 * hack that copies the gnutls_session_int definition from gnutls_int.h so that
 * we can get the needed information.
 */

typedef u8 uint8;
#define TLS_RANDOM_SIZE 32
#define TLS_MASTER_SIZE 48
typedef unsigned char opaque;
typedef struct {
    uint8 suite[2];
} cipher_suite_st;

typedef struct {
	gnutls_connection_end_t entity;
	gnutls_kx_algorithm_t kx_algorithm;
	gnutls_cipher_algorithm_t read_bulk_cipher_algorithm;
	gnutls_mac_algorithm_t read_mac_algorithm;
	gnutls_compression_method_t read_compression_algorithm;
	gnutls_cipher_algorithm_t write_bulk_cipher_algorithm;
	gnutls_mac_algorithm_t write_mac_algorithm;
	gnutls_compression_method_t write_compression_algorithm;
	cipher_suite_st current_cipher_suite;
	opaque master_secret[TLS_MASTER_SIZE];
	opaque client_random[TLS_RANDOM_SIZE];
	opaque server_random[TLS_RANDOM_SIZE];
	/* followed by stuff we are not interested in */
} security_parameters_st;

struct gnutls_session_int {
	security_parameters_st security_parameters;
	/* followed by things we are not interested in */
};

static int tls_gnutls_ref_count = 0;

struct tls_connection {
	gnutls_session session;
	char *subject_match, *altsubject_match;
	int read_alerts, write_alerts, failed;

	u8 *pre_shared_secret;
	size_t pre_shared_secret_len;
	int established;
	int verify_peer;

	u8 *push_buf, *pull_buf, *pull_buf_offset;
	size_t push_buf_len, pull_buf_len;

	gnutls_certificate_credentials_t xcred;
};


static void tls_log_func(int level, const char *msg)
{
	char *s, *pos;
	if (level == 6 || level == 7) {
		/* These levels seem to be mostly I/O debug and msg dumps */
		return;
	}

	s = strdup(msg);
	if (s == NULL)
		return;

	pos = s;
	while (*pos != '\0') {
		if (*pos == '\n') {
			*pos = '\0';
			break;
		}
		pos++;
	}
	wpa_printf(level > 3 ? MSG_MSGDUMP : MSG_DEBUG,
		   "gnutls<%d> %s", level, s);
	free(s);
}


extern int wpa_debug_show_keys;

void * tls_init(const struct tls_config *conf)
{
	/* Because of the horrible hack to get master_secret and client/server
	 * random, we need to make sure that the gnutls version is something
	 * that is expected to have same structure definition for the session
	 * data.. */
	const char *ver;
	const char *ok_ver[] = { "1.2.3", "1.2.4", "1.2.5", "1.2.6", NULL };
	int i;

	if (tls_gnutls_ref_count == 0 && gnutls_global_init() < 0)
		return NULL;
	tls_gnutls_ref_count++;

	ver = gnutls_check_version(NULL);
	if (ver == NULL)
		return NULL;
	wpa_printf(MSG_DEBUG, "%s - gnutls version %s", __func__, ver);
	for (i = 0; ok_ver[i]; i++) {
		if (strcmp(ok_ver[i], ver) == 0)
			break;
	}
	if (ok_ver[i] == NULL) {
		wpa_printf(MSG_INFO, "Untested gnutls version %s - this needs "
			   "to be tested and enabled in tls_gnutls.c", ver);
		return NULL;
	}

	gnutls_global_set_log_function(tls_log_func);
	if (wpa_debug_show_keys)
		gnutls_global_set_log_level(11);
	return (void *) 1;
}


void tls_deinit(void *ssl_ctx)
{
	tls_gnutls_ref_count--;
	if (tls_gnutls_ref_count == 0)
		gnutls_global_deinit();
}


int tls_get_errors(void *ssl_ctx)
{
	return 0;
}


static ssize_t tls_pull_func(gnutls_transport_ptr ptr, void *buf,
			     size_t len)
{
	struct tls_connection *conn = (struct tls_connection *) ptr;
	u8 *end;
	if (conn->pull_buf == NULL) {
		errno = EWOULDBLOCK;
		return -1;
	}

	end = conn->pull_buf + conn->pull_buf_len;
	if (end - conn->pull_buf_offset < len)
		len = end - conn->pull_buf_offset;
	memcpy(buf, conn->pull_buf_offset, len);
	conn->pull_buf_offset += len;
	if (conn->pull_buf_offset == end) {
		wpa_printf(MSG_DEBUG, "%s - pull_buf consumed", __func__);
		free(conn->pull_buf);
		conn->pull_buf = conn->pull_buf_offset = NULL;
		conn->pull_buf_len = 0;
	} else {
		wpa_printf(MSG_DEBUG, "%s - %d bytes remaining in pull_buf",
			   __func__, end - conn->pull_buf_offset);
	}
	return len;
}


static ssize_t tls_push_func(gnutls_transport_ptr ptr, const void *buf,
			     size_t len)
{
	struct tls_connection *conn = (struct tls_connection *) ptr;
	u8 *nbuf;

	nbuf = realloc(conn->push_buf, conn->push_buf_len + len);
	if (nbuf == NULL) {
		errno = ENOMEM;
		return -1;
	}
	memcpy(nbuf + conn->push_buf_len, buf, len);
	conn->push_buf = nbuf;
	conn->push_buf_len += len;

	return len;
}


struct tls_connection * tls_connection_init(void *ssl_ctx)
{
	struct tls_connection *conn;
	const int cert_types[2] = { GNUTLS_CRT_X509, 0 };
	const int protos[2] = { GNUTLS_TLS1, 0 };


	conn = malloc(sizeof(*conn));
	if (conn == NULL)
		return NULL;
	memset(conn, 0, sizeof(*conn));
	if (gnutls_init(&conn->session, GNUTLS_CLIENT) < 0) {
		wpa_printf(MSG_INFO, "TLS: Failed to initialize new TLS "
			   "connection");
		free(conn);
		return NULL;
	}

	gnutls_set_default_priority(conn->session);
	gnutls_certificate_type_set_priority(conn->session, cert_types);
	gnutls_protocol_set_priority(conn->session, protos);

	gnutls_transport_set_pull_function(conn->session, tls_pull_func);
	gnutls_transport_set_push_function(conn->session, tls_push_func);
	gnutls_transport_set_ptr(conn->session, (gnutls_transport_ptr) conn);

	gnutls_certificate_allocate_credentials(&conn->xcred);

	return conn;
}


void tls_connection_deinit(void *ssl_ctx, struct tls_connection *conn)
{
	if (conn == NULL)
		return;
	gnutls_certificate_free_credentials(conn->xcred);
	gnutls_deinit(conn->session);
	free(conn->pre_shared_secret);
	free(conn->subject_match);
	free(conn->altsubject_match);
	free(conn->push_buf);
	free(conn->pull_buf);
	free(conn);
}


int tls_connection_established(void *ssl_ctx, struct tls_connection *conn)
{
	return conn ? conn->established : 0;
}


int tls_connection_shutdown(void *ssl_ctx, struct tls_connection *conn)
{
	if (conn == NULL)
		return -1;

	/* Shutdown previous TLS connection without notifying the peer
	 * because the connection was already terminated in practice
	 * and "close notify" shutdown alert would confuse AS. */
	gnutls_bye(conn->session, GNUTLS_SHUT_RDWR);
	free(conn->push_buf);
	conn->push_buf = NULL;
	conn->push_buf_len = 0;
	conn->established = 0;
	/* TODO: what to do trigger new handshake for re-auth? */
	return 0;
}


#if 0
static int tls_match_altsubject(X509 *cert, const char *match)
{
	GENERAL_NAME *gen;
	char *field, *tmp;
	void *ext;
	int i, found = 0;
	size_t len;

	ext = X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);

	for (i = 0; ext && i < sk_GENERAL_NAME_num(ext); i++) {
		gen = sk_GENERAL_NAME_value(ext, i);
		switch (gen->type) {
		case GEN_EMAIL:
			field = "EMAIL";
			break;
		case GEN_DNS:
			field = "DNS";
			break;
		case GEN_URI:
			field = "URI";
			break;
		default:
			field = NULL;
			wpa_printf(MSG_DEBUG, "TLS: altSubjectName: "
				   "unsupported type=%d", gen->type);
			break;
		}

		if (!field)
			continue;

		wpa_printf(MSG_DEBUG, "TLS: altSubjectName: %s:%s",
			   field, gen->d.ia5->data);
		len = strlen(field) + 1 + strlen((char *) gen->d.ia5->data) +
			1;
		tmp = malloc(len);
		if (tmp == NULL)
			continue;
		snprintf(tmp, len, "%s:%s", field, gen->d.ia5->data);
		if (strstr(tmp, match))
			found++;
		free(tmp);
	}

	return found;
}
#endif


#if 0
static int tls_verify_cb(int preverify_ok, X509_STORE_CTX *x509_ctx)
{
	char buf[256];
	X509 *err_cert;
	int err, depth;
	SSL *ssl;
	struct tls_connection *conn;
	char *match, *altmatch;

	err_cert = X509_STORE_CTX_get_current_cert(x509_ctx);
	err = X509_STORE_CTX_get_error(x509_ctx);
	depth = X509_STORE_CTX_get_error_depth(x509_ctx);
	ssl = X509_STORE_CTX_get_ex_data(x509_ctx,
					 SSL_get_ex_data_X509_STORE_CTX_idx());
	X509_NAME_oneline(X509_get_subject_name(err_cert), buf, sizeof(buf));

	conn = SSL_get_app_data(ssl);
	match = conn ? conn->subject_match : NULL;
	altmatch = conn ? conn->altsubject_match : NULL;

	if (!preverify_ok) {
		wpa_printf(MSG_WARNING, "TLS: Certificate verification failed,"
			   " error %d (%s) depth %d for '%s'", err,
			   X509_verify_cert_error_string(err), depth, buf);
	} else {
		wpa_printf(MSG_DEBUG, "TLS: tls_verify_cb - "
			   "preverify_ok=%d err=%d (%s) depth=%d buf='%s'",
			   preverify_ok, err,
			   X509_verify_cert_error_string(err), depth, buf);
		if (depth == 0 && match && strstr(buf, match) == NULL) {
			wpa_printf(MSG_WARNING, "TLS: Subject '%s' did not "
				   "match with '%s'", buf, match);
			preverify_ok = 0;
		} else if (depth == 0 && altmatch &&
			   !tls_match_altsubject(err_cert, altmatch)) {
			wpa_printf(MSG_WARNING, "TLS: altSubjectName match "
				   "'%s' not found", altmatch);
			preverify_ok = 0;
		}
	}

	return preverify_ok;
}
#endif


int tls_connection_set_params(void *tls_ctx, struct tls_connection *conn,
			      const struct tls_connection_params *params)
{
	int ret;

	if (conn == NULL || params == NULL)
		return -1;

	free(conn->subject_match);
	conn->subject_match = NULL;
	if (params->subject_match) {
		conn->subject_match = strdup(params->subject_match);
		if (conn->subject_match == NULL)
			return -1;
	}

	free(conn->altsubject_match);
	conn->altsubject_match = NULL;
	if (params->altsubject_match) {
		conn->altsubject_match = strdup(params->altsubject_match);
		if (conn->altsubject_match == NULL)
			return -1;
	}

	/* TODO: gnutls_certificate_set_verify_flags(xcred, flags); 
	 * to force peer validation(?) */

	if (params->ca_cert) {
		conn->verify_peer = 1;
		ret = gnutls_certificate_set_x509_trust_file(
			conn->xcred, params->ca_cert, GNUTLS_X509_FMT_PEM);
		if (ret < 0) {
			wpa_printf(MSG_DEBUG, "Failed to read CA cert '%s' "
				   "in PEM format: %s", params->ca_cert,
				   gnutls_strerror(ret));
			ret = gnutls_certificate_set_x509_trust_file(
				conn->xcred, params->ca_cert,
				GNUTLS_X509_FMT_DER);
			if (ret < 0) {
				wpa_printf(MSG_DEBUG, "Failed to read CA cert "
					   "'%s' in DER format: %s",
					   params->ca_cert,
					   gnutls_strerror(ret));
			}
		}
	}

	if (params->client_cert && params->private_key) {
		/* TODO: private_key_passwd? */
		ret = gnutls_certificate_set_x509_key_file(
			conn->xcred, params->client_cert, params->private_key,
			GNUTLS_X509_FMT_PEM);
		if (ret < 0) {
			wpa_printf(MSG_DEBUG, "Failed to read client cert/key "
				   "in PEM format: %s", gnutls_strerror(ret));
			ret = gnutls_certificate_set_x509_key_file(
				conn->xcred, params->client_cert,
				params->private_key, GNUTLS_X509_FMT_DER);
			if (ret < 0) {
				wpa_printf(MSG_DEBUG, "Failed to read client "
					   "cert/key in DER format: %s",
					   gnutls_strerror(ret));
				return ret;
			}
		}
	}

	ret = gnutls_credentials_set(conn->session, GNUTLS_CRD_CERTIFICATE,
				     conn->xcred);
	if (ret < 0) {
		wpa_printf(MSG_INFO, "Failed to configure credentials: %s",
			   gnutls_strerror(ret));
	}

	return ret;
}


int tls_global_ca_cert(void *_ssl_ctx, const char *ca_cert)
{
	/* TODO */
	return -1;
}


int tls_global_set_verify(void *ssl_ctx, int check_crl)
{
	/* TODO */
	return -1;
}


int tls_connection_set_verify(void *ssl_ctx, struct tls_connection *conn,
			      int verify_peer)
{
	if (conn == NULL)
		return -1;

	/* TODO */

	return 1;
}


int tls_global_client_cert(void *_ssl_ctx, const char *client_cert)
{
	/* TODO */
	return -1;
}


int tls_global_private_key(void *_ssl_ctx, const char *private_key,
			   const char *private_key_passwd)
{
	/* TODO */
	return -1;
}


int tls_connection_get_keys(void *ssl_ctx, struct tls_connection *conn,
			    struct tls_keys *keys)
{
	security_parameters_st *sec;

	if (conn == NULL || conn->session == NULL || keys == NULL)
		return -1;

	memset(keys, 0, sizeof(*keys));
	sec = &conn->session->security_parameters;
	keys->master_key = sec->master_secret;
	keys->master_key_len = TLS_MASTER_SIZE;
	keys->client_random = sec->client_random;
	keys->client_random_len = TLS_RANDOM_SIZE;
	keys->server_random = sec->server_random;
	keys->server_random_len = TLS_RANDOM_SIZE;

	return 0;
}


static int tls_connection_verify_peer(struct tls_connection *conn)
{
	unsigned int status, num_certs, i;
	time_t now;
	const gnutls_datum_t *certs;
	gnutls_x509_crt_t cert;

	if (gnutls_certificate_verify_peers2(conn->session, &status) < 0) {
		wpa_printf(MSG_INFO, "TLS: Failed to verify peer "
			   "certificate chain");
		return -1;
	}

	if (conn->verify_peer && (status & GNUTLS_CERT_INVALID)) {
		wpa_printf(MSG_INFO, "TLS: Peer certificate not trusted");
		return -1;
	}

	if (status & GNUTLS_CERT_SIGNER_NOT_FOUND) {
		wpa_printf(MSG_INFO, "TLS: Peer certificate does not have a "
			   "known issuer");
		return -1;
	}

	if (status & GNUTLS_CERT_REVOKED) {
		wpa_printf(MSG_INFO, "TLS: Peer certificate has been revoked");
		return -1;
	}

	now = time(NULL);

	certs = gnutls_certificate_get_peers(conn->session, &num_certs);
	if (certs == NULL) {
		wpa_printf(MSG_INFO, "TLS: No peer certificate chain "
			   "received");
		return -1;
	}

	for (i = 0; i < num_certs; i++) {
		char *buf;
		size_t len;
		if (gnutls_x509_crt_init(&cert) < 0) {
			wpa_printf(MSG_INFO, "TLS: Certificate initialization "
				   "failed");
			return -1;
		}

		if (gnutls_x509_crt_import(cert, &certs[i],
					   GNUTLS_X509_FMT_DER) < 0) {
			wpa_printf(MSG_INFO, "TLS: Could not parse peer "
				   "certificate %d/%d", i + 1, num_certs);
			gnutls_x509_crt_deinit(cert);
			return -1;
		}

		gnutls_x509_crt_get_dn(cert, NULL, &len);
		len++;
		buf = malloc(len + 1);
		if (buf) {
			buf[0] = buf[len] = '\0';
			gnutls_x509_crt_get_dn(cert, buf, &len);
		}
		wpa_printf(MSG_DEBUG, "TLS: Peer cert chain %d/%d: %s",
			   i + 1, num_certs, buf);

		if (i == 0) {
			/* TODO: validate subject_match and altsubject_match */
		}

		free(buf);

		if (gnutls_x509_crt_get_expiration_time(cert) < now ||
		    gnutls_x509_crt_get_activation_time(cert) > now) {
			wpa_printf(MSG_INFO, "TLS: Peer certificate %d/%d is "
				   "not valid at this time",
				   i + 1, num_certs);
			gnutls_x509_crt_deinit(cert);
			return -1;
		}

		gnutls_x509_crt_deinit(cert);
	}

	return 0;
}


u8 * tls_connection_handshake(void *ssl_ctx, struct tls_connection *conn,
			      const u8 *in_data, size_t in_len,
			      size_t *out_len)
{
	u8 *out_data;
	int ret;

	if (in_data && in_len) {
		if (conn->pull_buf) {
			wpa_printf(MSG_DEBUG, "%s - %d bytes remaining in "
				   "pull_buf", __func__, conn->pull_buf_len);
			free(conn->pull_buf);
		}
		conn->pull_buf = malloc(in_len);
		if (conn->pull_buf == NULL)
			return NULL;
		memcpy(conn->pull_buf, in_data, in_len);
		conn->pull_buf_offset = conn->pull_buf;
		conn->pull_buf_len = in_len;
	}

	ret = gnutls_handshake(conn->session);
	if (ret < 0) {
		switch (ret) {
		case GNUTLS_E_AGAIN:
			break;
		case GNUTLS_E_FATAL_ALERT_RECEIVED:
			wpa_printf(MSG_DEBUG, "%s - received fatal '%s' alert",
				   __func__, gnutls_alert_get_name(
					   gnutls_alert_get(conn->session)));
			conn->read_alerts++;
			/* continue */
		default:
			wpa_printf(MSG_DEBUG, "%s - gnutls_handshake failed "
				   "-> %s", __func__, gnutls_strerror(ret));
			conn->failed++;
		}
	} else {
		wpa_printf(MSG_DEBUG, "TLS: Handshake completed successfully");
		if (conn->verify_peer && tls_connection_verify_peer(conn)) {
			wpa_printf(MSG_INFO, "TLS: Peer certificate chain "
				   "failed validation");
			conn->failed++;
			return NULL;
		}
		conn->established = 1;
		if (conn->push_buf == NULL) {
			/* Need to return something to get final TLS ACK. */
			conn->push_buf = malloc(1);
		}
	}

	out_data = conn->push_buf;
	*out_len = conn->push_buf_len;
	conn->push_buf = NULL;
	conn->push_buf_len = 0;
	return out_data;
}


u8 * tls_connection_server_handshake(void *ssl_ctx,
				     struct tls_connection *conn,
				     const u8 *in_data, size_t in_len,
				     size_t *out_len)
{
	/* TODO */
	return NULL;
}


int tls_connection_encrypt(void *ssl_ctx, struct tls_connection *conn,
			   const u8 *in_data, size_t in_len,
			   u8 *out_data, size_t out_len)
{
	ssize_t res;
	res = gnutls_record_send(conn->session, in_data, in_len);
	if (conn->push_buf == NULL)
		return -1;
	if (conn->push_buf_len < out_len)
		out_len = conn->push_buf_len;
	memcpy(out_data, conn->push_buf, out_len);
	free(conn->push_buf);
	conn->push_buf = NULL;
	conn->push_buf_len = 0;
	return out_len;
}


int tls_connection_decrypt(void *ssl_ctx, struct tls_connection *conn,
			   const u8 *in_data, size_t in_len,
			   u8 *out_data, size_t out_len)
{
	ssize_t res;

	if (conn->pull_buf) {
		wpa_printf(MSG_DEBUG, "%s - %d bytes remaining in "
			   "pull_buf", __func__, conn->pull_buf_len);
		free(conn->pull_buf);
	}
	conn->pull_buf = malloc(in_len);
	if (conn->pull_buf == NULL)
		return -1;
	memcpy(conn->pull_buf, in_data, in_len);
	conn->pull_buf_offset = conn->pull_buf;
	conn->pull_buf_len = in_len;

	res = gnutls_record_recv(conn->session, out_data, out_len);
	if (res < 0) {
		wpa_printf(MSG_DEBUG, "%s - gnutls_record_recv failed: %d "
			   "(%s)", __func__, res, gnutls_strerror(res));
	}

	return res;
}


int tls_connection_resumed(void *ssl_ctx, struct tls_connection *conn)
{
	if (conn == NULL)
		return 0;
	return gnutls_session_is_resumed(conn->session);
}


#ifdef EAP_FAST
int tls_connection_set_master_key(void *ssl_ctx, struct tls_connection *conn,
				  const u8 *key, size_t key_len)
{
	/* TODO */
	return -1;
}
#endif /* EAP_FAST */


int tls_connection_set_anon_dh(void *ssl_ctx, struct tls_connection *conn)
{
	/* TODO: set ADH-AES128-SHA cipher */
	return -1;
}


int tls_get_cipher(void *ssl_ctx, struct tls_connection *conn,
		   char *buf, size_t buflen)
{
	/* TODO */
	buf[0] = '\0';
	return 0;
}


int tls_connection_enable_workaround(void *ssl_ctx,
				     struct tls_connection *conn)
{
	/* TODO: set SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS */
	return 0;
}


#ifdef EAP_FAST
int tls_connection_client_hello_ext(void *ssl_ctx, struct tls_connection *conn,
				    int ext_type, const u8 *data,
				    size_t data_len)
{
	/* TODO */
	return -1;
}
#endif /* EAP_FAST */


int tls_connection_get_failed(void *ssl_ctx, struct tls_connection *conn)
{
	if (conn == NULL)
		return -1;
	return conn->failed;
}


int tls_connection_get_read_alerts(void *ssl_ctx, struct tls_connection *conn)
{
	if (conn == NULL)
		return -1;
	return conn->read_alerts;
}


int tls_connection_get_write_alerts(void *ssl_ctx, struct tls_connection *conn)
{
	if (conn == NULL)
		return -1;
	return conn->write_alerts;
}
