/*
 * WPA Supplicant / SSL/TLS interface functions for openssl
 * Copyright (c) 2004-2005, Jouni Malinen <jkmaline@cc.hut.fi>
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
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pkcs12.h>

#include "common.h"
#include "tls.h"


struct tls_connection {
	SSL *ssl;
	BIO *ssl_in, *ssl_out;
	char *subject_match;
};


static void ssl_info_cb(const SSL *ssl, int where, int ret)
{
	const char *str;
	int w;

	wpa_printf(MSG_DEBUG, "SSL: (where=0x%x ret=0x%x)", where, ret);
	w = where & ~SSL_ST_MASK;
	if (w & SSL_ST_CONNECT)
		str = "SSL_connect";
	else if (w & SSL_ST_ACCEPT)
		str = "SSL_accept";
	else
		str = "undefined";

	if (where & SSL_CB_LOOP) {
		wpa_printf(MSG_DEBUG, "SSL: %s:%s",
			   str, SSL_state_string_long(ssl));
	} else if (where & SSL_CB_ALERT) {
		wpa_printf(MSG_INFO, "SSL: SSL3 alert: %s:%s:%s",
			   where & SSL_CB_READ ?
			   "read (authentication server reported an error)" :
			   "write (local SSL3 detected an error)",
			   SSL_alert_type_string_long(ret),
			   SSL_alert_desc_string_long(ret));
	} else if (where & SSL_CB_EXIT && ret <= 0) {
		wpa_printf(MSG_DEBUG, "SSL: %s:%s in %s",
			   str, ret == 0 ? "failed" : "error",
			   SSL_state_string_long(ssl));
	}
}


void * tls_init(void)
{
	SSL_CTX *ssl;

	SSL_load_error_strings();
	SSL_library_init();
	/* TODO: if /dev/urandom is available, PRNG is seeded automatically.
	 * If this is not the case, random data should be added here. */

#ifdef PKCS12_FUNCS
	PKCS12_PBE_add();
#endif  /* PKCS12_FUNCS */

	ssl = SSL_CTX_new(TLSv1_method());
	if (ssl == NULL)
		return NULL;

	SSL_CTX_set_info_callback(ssl, ssl_info_cb);

	return ssl;
}


void tls_deinit(void *ssl_ctx)
{
	SSL_CTX *ssl = ssl_ctx;
	SSL_CTX_free(ssl);
	ERR_free_strings();
	EVP_cleanup();
}


int tls_get_errors(void *ssl_ctx)
{
	int count = 0;
	unsigned long err;

	while ((err = ERR_get_error())) {
		wpa_printf(MSG_INFO, "TLS - SSL error: %s",
			   ERR_error_string(err, NULL));
		count++;
	}

	return count;
}

struct tls_connection * tls_connection_init(void *ssl_ctx)
{
	SSL_CTX *ssl = ssl_ctx;
	struct tls_connection *conn;

	conn = malloc(sizeof(*conn));
	if (conn == NULL)
		return NULL;
	memset(conn, 0, sizeof(*conn));
	conn->ssl = SSL_new(ssl);
	if (conn->ssl == NULL) {
		wpa_printf(MSG_INFO, "TLS: Failed to initialize new SSL "
			   "connection: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		free(conn);
		return NULL;
	}

	SSL_set_options(conn->ssl,
			SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
			SSL_OP_SINGLE_DH_USE);

	conn->ssl_in = BIO_new(BIO_s_mem());
	if (!conn->ssl_in) {
		wpa_printf(MSG_INFO, "SSL: Failed to create a new BIO for "
			   "ssl_in: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		SSL_free(conn->ssl);
		free(conn);
		return NULL;
	}

	conn->ssl_out = BIO_new(BIO_s_mem());
	if (!conn->ssl_out) {
		wpa_printf(MSG_INFO, "SSL: Failed to create a new BIO for "
			   "ssl_out: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		SSL_free(conn->ssl);
		BIO_free(conn->ssl_in);
		free(conn);
		return NULL;
	}

	SSL_set_bio(conn->ssl, conn->ssl_in, conn->ssl_out);

	return conn;
}


void tls_connection_deinit(void *ssl_ctx, struct tls_connection *conn)
{
	if (conn == NULL)
		return;
	SSL_free(conn->ssl);
	free(conn->subject_match);
	free(conn);
}


int tls_connection_established(void *ssl_ctx, struct tls_connection *conn)
{
	return conn ? SSL_is_init_finished(conn->ssl) : 0;
}


int tls_connection_shutdown(void *ssl_ctx, struct tls_connection *conn)
{
	if (conn == NULL)
		return -1;

	/* Shutdown previous TLS connection without notifying the peer
	 * because the connection was already terminated in practice
	 * and "close notify" shutdown alert would confuse AS. */
	SSL_set_quiet_shutdown(conn->ssl, 1);
	SSL_shutdown(conn->ssl);
	return 0;
}


static int tls_verify_cb(int preverify_ok, X509_STORE_CTX *x509_ctx)
{
	char buf[256];
	X509 *err_cert;
	int err, depth;
	SSL *ssl;
	struct tls_connection *conn;
	char *match;

	err_cert = X509_STORE_CTX_get_current_cert(x509_ctx);
	err = X509_STORE_CTX_get_error(x509_ctx);
	depth = X509_STORE_CTX_get_error_depth(x509_ctx);
	ssl = X509_STORE_CTX_get_ex_data(x509_ctx,
					 SSL_get_ex_data_X509_STORE_CTX_idx());
	X509_NAME_oneline(X509_get_subject_name(err_cert), buf, 256);

	conn = SSL_get_app_data(ssl);
	match = conn ? conn->subject_match : NULL;

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
		}
	}

	return preverify_ok;
}


int tls_connection_ca_cert(void *ssl_ctx, struct tls_connection *conn,
			   const char *ca_cert, const char *subject_match)
{
	if (conn == NULL)
		return -1;

	free(conn->subject_match);
	conn->subject_match = NULL;
	if (subject_match) {
		conn->subject_match = strdup(subject_match);
		if (conn->subject_match == NULL)
			return -1;
	}

	if (ca_cert) {
		if (SSL_CTX_load_verify_locations(ssl_ctx, ca_cert, NULL) != 1)
		{
			wpa_printf(MSG_WARNING, "TLS: Failed to load root "
				   "certificates: %s",
				   ERR_error_string(ERR_get_error(), NULL));
			return -1;
		} else {
			wpa_printf(MSG_DEBUG, "TLS: Trusted root "
				   "certificate(s) loaded");
			tls_get_errors(ssl_ctx);
		}
		SSL_set_app_data(conn->ssl, conn);
		SSL_set_verify(conn->ssl, SSL_VERIFY_PEER, tls_verify_cb);
	} else {
		/* No ca_cert configured - do not try to verify server
		 * certificate */
		SSL_set_verify(conn->ssl, SSL_VERIFY_NONE, NULL);
	}

	return 0;
}


int tls_global_ca_cert(void *_ssl_ctx, const char *ca_cert)
{
	SSL_CTX *ssl_ctx = _ssl_ctx;
	if (ca_cert) {
		if (SSL_CTX_load_verify_locations(ssl_ctx, ca_cert, NULL) != 1)
		{
			wpa_printf(MSG_WARNING, "TLS: Failed to load root "
				   "certificates: %s",
				   ERR_error_string(ERR_get_error(), NULL));
			return -1;
		} else {
			wpa_printf(MSG_DEBUG, "TLS: Trusted root "
				   "certificate(s) loaded");
		}
	}

	return 0;
}


int tls_connection_set_verify(void *ssl_ctx, struct tls_connection *conn,
			      int verify_peer, const char *subject_match)
{
	if (conn == NULL)
		return -1;

	free(conn->subject_match);
	conn->subject_match = NULL;
	if (subject_match) {
		conn->subject_match = strdup(subject_match);
		if (conn->subject_match == NULL)
			return -1;
	}

	if (verify_peer) {
		SSL_set_app_data(conn->ssl, conn);
		SSL_set_verify(conn->ssl, SSL_VERIFY_PEER |
			       SSL_VERIFY_FAIL_IF_NO_PEER_CERT |
			       SSL_VERIFY_CLIENT_ONCE, tls_verify_cb);
	} else {
		SSL_set_verify(conn->ssl, SSL_VERIFY_NONE, NULL);
	}

	SSL_set_accept_state(conn->ssl);

	return 0;
}


int tls_connection_client_cert(void *ssl_ctx, struct tls_connection *conn,
			       const char *client_cert)
{
	if (client_cert == NULL)
		return 0;
	if (conn == NULL)
		return -1;

	if (SSL_use_certificate_file(conn->ssl, client_cert,
				     SSL_FILETYPE_ASN1) != 1 &&
	    SSL_use_certificate_file(conn->ssl, client_cert,
				     SSL_FILETYPE_PEM) != 1) {
		wpa_printf(MSG_INFO, "TLS: Failed to load client "
			   "certificate: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		return -1;
	}
	return 0;
}


int tls_global_client_cert(void *_ssl_ctx, const char *client_cert)
{
	SSL_CTX *ssl_ctx = _ssl_ctx;
	if (client_cert == NULL)
		return 0;

	if (SSL_CTX_use_certificate_file(ssl_ctx, client_cert,
					 SSL_FILETYPE_ASN1) != 1 &&
	    SSL_CTX_use_certificate_file(ssl_ctx, client_cert,
					 SSL_FILETYPE_PEM) != 1) {
		wpa_printf(MSG_INFO, "TLS: Failed to load client "
			   "certificate: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		return -1;
	}
	return 0;
}


static int tls_passwd_cb(char *buf, int size, int rwflag, void *password)
{
	if (password == NULL) {
		return 0;
	}
	strncpy(buf, (char *) password, size);
	buf[size - 1] = '\0';
	return strlen(buf);
}


static int tls_read_pkcs12(SSL_CTX *ssl_ctx, SSL *ssl, const char *private_key,
			   const char *passwd)
{
#ifdef PKCS12_FUNCS
	FILE *f;
	PKCS12 *p12;
	EVP_PKEY *pkey;
	X509 *cert;
	int res = 0;

	f = fopen(private_key, "r");
	if (f == NULL)
		return -1;

	p12 = d2i_PKCS12_fp(f, NULL);
	if (p12 == NULL) {
		wpa_printf(MSG_DEBUG, "TLS: Failed to read PKCS12 file '%s'",
			   private_key);
		fclose(f);
		return -1;
	}
	fclose(f);

	pkey = NULL;
	cert = NULL;
	if (!PKCS12_parse(p12, passwd, &pkey, &cert, NULL)) {
		wpa_printf(MSG_DEBUG, "TLS: Failed to parse PKCS12 file '%s': "
			   "%s", private_key,
			   ERR_error_string(ERR_get_error(), NULL));
		return -1;
	}
	wpa_printf(MSG_DEBUG, "TLS: Successfully parsed PKCS12 file '%s'",
		   private_key);

	if (cert) {
		wpa_printf(MSG_DEBUG, "TLS: Got certificate from PKCS12");
		if (ssl) {
			if (SSL_use_certificate(ssl, cert) != 1)
				res = -1;
		} else {
			if (SSL_CTX_use_certificate(ssl_ctx, cert) != 1)
				res = -1;
		}
		X509_free(cert);
	}

	if (pkey) {
		wpa_printf(MSG_DEBUG, "TLS: Got private key from PKCS12");
		if (ssl) {
			if (SSL_use_PrivateKey(ssl, pkey) != 1)
				res = -1;
		} else {
			if (SSL_CTX_use_PrivateKey(ssl_ctx, pkey) != 1)
				res = -1;
		}
		EVP_PKEY_free(pkey);
	}

	PKCS12_free(p12);

	return res;
#else /* PKCS12_FUNCS */
	wpa_printf(MSG_INFO, "TLS: PKCS12 support disabled - cannot read "
		   "p12/pfx files");
	return -1;
#endif  /* PKCS12_FUNCS */
}


int tls_connection_private_key(void *_ssl_ctx, struct tls_connection *conn,
			       const char *private_key,
			       const char *private_key_passwd)
{
	SSL_CTX *ssl_ctx = _ssl_ctx;
	char *passwd;

	if (private_key == NULL)
		return 0;
	if (conn == NULL)
		return -1;

	if (private_key_passwd) {
		passwd = strdup(private_key_passwd);
		if (passwd == NULL)
			return -1;
	} else
		passwd = NULL;

	SSL_CTX_set_default_passwd_cb(ssl_ctx, tls_passwd_cb);
	SSL_CTX_set_default_passwd_cb_userdata(ssl_ctx, passwd);
	if (SSL_use_PrivateKey_file(conn->ssl, private_key,
				    SSL_FILETYPE_ASN1) != 1 &&
	    SSL_use_PrivateKey_file(conn->ssl, private_key,
				    SSL_FILETYPE_PEM) != 1 &&
	    tls_read_pkcs12(ssl_ctx, conn->ssl, private_key, passwd)) {
		wpa_printf(MSG_INFO, "SSL: Failed to load private key: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		free(passwd);
		ERR_clear_error();
		return -1;
	}
	ERR_clear_error();
	free(passwd);
	SSL_CTX_set_default_passwd_cb(ssl_ctx, NULL);
	
	if (!SSL_check_private_key(conn->ssl)) {
		wpa_printf(MSG_INFO, "SSL: Private key failed "
			   "verification: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		return -1;
	}

	return 0;
}


int tls_global_private_key(void *_ssl_ctx, const char *private_key,
			   const char *private_key_passwd)
{
	SSL_CTX *ssl_ctx = _ssl_ctx;
	char *passwd;

	if (private_key == NULL)
		return 0;

	if (private_key_passwd) {
		passwd = strdup(private_key_passwd);
		if (passwd == NULL)
			return -1;
	} else
		passwd = NULL;

	SSL_CTX_set_default_passwd_cb(ssl_ctx, tls_passwd_cb);
	SSL_CTX_set_default_passwd_cb_userdata(ssl_ctx, passwd);
	if (SSL_CTX_use_PrivateKey_file(ssl_ctx, private_key,
					SSL_FILETYPE_ASN1) != 1 &&
	    SSL_CTX_use_PrivateKey_file(ssl_ctx, private_key,
					SSL_FILETYPE_PEM) != 1 &&
	    tls_read_pkcs12(ssl_ctx, NULL, private_key, passwd)) {
		wpa_printf(MSG_INFO, "SSL: Failed to load private key: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		free(passwd);
		ERR_clear_error();
		return -1;
	}
	free(passwd);
	ERR_clear_error();
	SSL_CTX_set_default_passwd_cb(ssl_ctx, NULL);
	
	if (!SSL_CTX_check_private_key(ssl_ctx)) {
		wpa_printf(MSG_INFO, "SSL: Private key failed "
			   "verification: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		return -1;
	}

	return 0;
}


int tls_connection_dh(void *ssl_ctx, struct tls_connection *conn,
		      const char *dh_file)
{
#ifdef OPENSSL_NO_DH
	if (dh_file == NULL)
		return 0;
	wpa_printf(MSG_ERROR, "TLS: openssl does not include DH support, but "
		   "dh_file specified");
	return -1;
#else /* OPENSSL_NO_DH */
	DH *dh;
	BIO *bio;

	if (dh_file == NULL)
		return 0;
	if (conn == NULL)
		return -1;

	bio = BIO_new_file(dh_file, "r");
	if (bio == NULL) {
		wpa_printf(MSG_INFO, "TLS: Failed to open DH file '%s': %s",
			   dh_file, ERR_error_string(ERR_get_error(), NULL));
		return -1;
	}
	dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
	BIO_free(bio);
#ifndef OPENSSL_NO_DSA
	while (dh == NULL) {
		DSA *dsa;
		wpa_printf(MSG_DEBUG, "TLS: Failed to parse DH file '%s': %s -"
			   " trying to parse as DSA params", dh_file,
			   ERR_error_string(ERR_get_error(), NULL));
		bio = BIO_new_file(dh_file, "r");
		if (bio == NULL)
			break;
		dsa = PEM_read_bio_DSAparams(bio, NULL, NULL, NULL);
		BIO_free(bio);
		if (!dsa) {
			wpa_printf(MSG_DEBUG, "TLS: Failed to parse DSA file "
				   "'%s': %s", dh_file,
				   ERR_error_string(ERR_get_error(), NULL));
			break;
		}

		wpa_printf(MSG_DEBUG, "TLS: DH file in DSA param format");
		dh = DSA_dup_DH(dsa);
		DSA_free(dsa);
		if (dh == NULL) {
			wpa_printf(MSG_INFO, "TLS: Failed to convert DSA "
				   "params into DH params");
			break;
		}
		break;
	}
#endif /* !OPENSSL_NO_DSA */
	if (dh == NULL) {
		wpa_printf(MSG_INFO, "TLS: Failed to read/parse DH/DSA file "
			   "'%s'", dh_file);
		return -1;
	}

	if (SSL_set_tmp_dh(conn->ssl, dh) != 1) {
		wpa_printf(MSG_INFO, "TLS: Failed to set DH params from '%s': "
			   "%s", dh_file,
			   ERR_error_string(ERR_get_error(), NULL));
		DH_free(dh);
		return -1;
	}
	DH_free(dh);
	return 0;
#endif /* OPENSSL_NO_DH */
}


int tls_connection_get_keys(void *ssl_ctx, struct tls_connection *conn,
			    struct tls_keys *keys)
{
	SSL *ssl;

	if (conn == NULL || keys == NULL)
		return -1;
	ssl = conn->ssl;
	if (ssl == NULL || ssl->s3 == NULL || ssl->session == NULL)
		return -1;

	keys->master_key = ssl->session->master_key;
	keys->master_key_len = ssl->session->master_key_length;
	keys->client_random = ssl->s3->client_random;
	keys->client_random_len = SSL3_RANDOM_SIZE;
	keys->server_random = ssl->s3->server_random;
	keys->server_random_len = SSL3_RANDOM_SIZE;

	return 0;
}


u8 * tls_connection_handshake(void *ssl_ctx, struct tls_connection *conn,
			      const u8 *in_data, size_t in_len,
			      size_t *out_len)
{
	int res;
	u8 *out_data;

	if (in_data &&
	    BIO_write(conn->ssl_in, in_data, in_len) < 0) {
		wpa_printf(MSG_INFO, "TLS: Handshake failed - BIO_write: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		return NULL;
	}

	res = SSL_connect(conn->ssl);
	if (res != 1) {
		int err = SSL_get_error(conn->ssl, res);
		if (err == SSL_ERROR_WANT_READ)
			wpa_printf(MSG_DEBUG, "SSL: SSL_connect - want "
				   "more data");
		else if (err == SSL_ERROR_WANT_WRITE)
			wpa_printf(MSG_DEBUG, "SSL: SSL_connect - want to "
				   "write");
		else {
			wpa_printf(MSG_INFO, "SSL: SSL_connect: %s",
				   ERR_error_string(ERR_get_error(), NULL));
			return NULL;
		}
	}

	res = BIO_ctrl_pending(conn->ssl_out);
	wpa_printf(MSG_DEBUG, "SSL: %d bytes pending from ssl_out", res);
	out_data = malloc(res == 0 ? 1 : res);
	if (out_data == NULL) {
		wpa_printf(MSG_DEBUG, "SSL: Failed to allocate memory for "
			   "handshake output (%d bytes)", res);
		BIO_reset(conn->ssl_out);
		*out_len = 0;
		return NULL;
	}
	res = res == 0 ? 0 : BIO_read(conn->ssl_out, out_data, res);
	if (res < 0) {
		wpa_printf(MSG_INFO, "TLS: Handshake failed - BIO_read: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		BIO_reset(conn->ssl_out);
		*out_len = 0;
		return NULL;
	}
	*out_len = res;
	return out_data;
}


u8 * tls_connection_server_handshake(void *ssl_ctx,
				     struct tls_connection *conn,
				     const u8 *in_data, size_t in_len,
				     size_t *out_len)
{
	int res;
	u8 *out_data;
	char buf[10];

	if (in_data &&
	    BIO_write(conn->ssl_in, in_data, in_len) < 0) {
		wpa_printf(MSG_INFO, "TLS: Handshake failed - BIO_write: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		return NULL;
	}

	res = SSL_read(conn->ssl, buf, sizeof(buf));
	if (res >= 0) {
		wpa_printf(MSG_DEBUG, "SSL: Unexpected data from SSL_read "
			   "(res=%d)", res);
	}

	res = BIO_ctrl_pending(conn->ssl_out);
	wpa_printf(MSG_DEBUG, "SSL: %d bytes pending from ssl_out", res);
	out_data = malloc(res == 0 ? 1 : res);
	if (out_data == NULL) {
		wpa_printf(MSG_DEBUG, "SSL: Failed to allocate memory for "
			   "handshake output (%d bytes)", res);
		BIO_reset(conn->ssl_out);
		*out_len = 0;
		return NULL;
	}
	res = res == 0 ? 0 : BIO_read(conn->ssl_out, out_data, res);
	if (res < 0) {
		wpa_printf(MSG_INFO, "TLS: Handshake failed - BIO_read: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		BIO_reset(conn->ssl_out);
		*out_len = 0;
		return NULL;
	}
	*out_len = res;
	return out_data;
}


int tls_connection_encrypt(void *ssl_ctx, struct tls_connection *conn,
			   u8 *in_data, size_t in_len,
			   u8 *out_data, size_t out_len)
{
	int res;

	if (conn == NULL)
		return -1;

	BIO_reset(conn->ssl_in);
	BIO_reset(conn->ssl_out);
	res = SSL_write(conn->ssl, in_data, in_len);
	if (res < 0) {
		wpa_printf(MSG_INFO, "TLS: Encryption failed - SSL_write: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		return res;
	}

	res = BIO_read(conn->ssl_out, out_data, out_len);
	if (res < 0) {
		wpa_printf(MSG_INFO, "TLS: Encryption failed - BIO_read: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		return res;
	}

	return res;
}


int tls_connection_decrypt(void *ssl_ctx, struct tls_connection *conn,
			   u8 *in_data, size_t in_len,
			   u8 *out_data, size_t out_len)
{
	int res;

	res = BIO_write(conn->ssl_in, in_data, in_len);
	if (res < 0) {
		wpa_printf(MSG_INFO, "TLS: Decryption failed - BIO_write: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		return res;
	}
	BIO_reset(conn->ssl_out);

	res = SSL_read(conn->ssl, out_data, out_len);
	if (res < 0) {
		wpa_printf(MSG_INFO, "TLS: Decryption failed - SSL_read: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		return res;
	}

	return res;
}


int tls_connection_resumed(void *ssl_ctx, struct tls_connection *conn)
{
	return conn ? conn->ssl->hit : 0;
}


int tls_connection_set_master_key(void *ssl_ctx, struct tls_connection *conn,
				  const u8 *key, size_t key_len)
{
	SSL *ssl;

	if (conn == NULL || key == NULL || key_len > SSL_MAX_MASTER_KEY_LENGTH)
		return -1;
	ssl = conn->ssl;
	if (ssl == NULL || ssl->session == NULL)
		return -1;

	memcpy(ssl->session->master_key, key, key_len);
	ssl->session->master_key_length = key_len;

	return 0;
}


int tls_connection_set_anon_dh(void *ssl_ctx, struct tls_connection *conn)
{
	if (conn == NULL || conn->ssl == NULL)
		return -1;

	if (SSL_set_cipher_list(conn->ssl, "ADH-AES128-SHA") != 1) {
		wpa_printf(MSG_INFO, "TLS: Anon DH configuration failed - %s",
			   ERR_error_string(ERR_get_error(), NULL));
		return -1;
	}

	return 0;
}


int tls_get_cipher(void *ssl_ctx, struct tls_connection *conn,
		   char *buf, size_t buflen)
{
	const char *name;
	if (conn == NULL || conn->ssl == NULL)
		return -1;

	name = SSL_get_cipher(conn->ssl);
	if (name == NULL)
		return -1;

	snprintf(buf, buflen, "%s", name);
	return 0;
}


int tls_connection_enable_workaround(void *ssl_ctx,
				     struct tls_connection *conn)
{
	SSL_set_options(conn->ssl, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);

	return 0;
}


#ifdef EAP_FAST
/* ClientHello TLS extensions require a patch to openssl, so this function is
 * commented out unless explicitly needed for EAP-FAST in order to be able to
 * build this file with unmodified openssl. */
int tls_connection_client_hello_ext(void *ssl_ctx, struct tls_connection *conn,
				    int ext_type, const u8 *data,
				    size_t data_len)
{
	struct tls_ext_hdr {
		u16 extensions_len;
		u16 extension_type;
		u16 extension_len;
	} *hdr;

	if (conn == NULL || conn->ssl == NULL)
		return -1;
	OPENSSL_free(conn->ssl->hello_extension);
	if (data == NULL) {
		conn->ssl->hello_extension = NULL;
		conn->ssl->hello_extension_len = 0;
		return 0;
	}
	if (data_len == 0) {
		conn->ssl->hello_extension = OPENSSL_malloc(1);
		conn->ssl->hello_extension_len = 0;
		return 0;
	}
	conn->ssl->hello_extension = OPENSSL_malloc(sizeof(*hdr) + data_len);
	if (conn->ssl->hello_extension == NULL)
		return -1;

	hdr = (struct tls_ext_hdr *) conn->ssl->hello_extension;
	hdr->extensions_len = host_to_be16(sizeof(*hdr) - 2 + data_len);
	hdr->extension_type = host_to_be16(ext_type);
	hdr->extension_len = host_to_be16(data_len);
	memcpy(hdr + 1, data, data_len);
	conn->ssl->hello_extension_len = sizeof(*hdr) + data_len;

	return 0;
}
#endif /* EAP_FAST */
