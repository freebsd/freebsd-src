/*
 * WPA Supplicant / SSL/TLS interface functions for Microsoft Schannel
 * Copyright (c) 2005, Jouni Malinen <jkmaline@cc.hut.fi>
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

/*
 * FIX: Go through all SSPI functions and verify what needs to be freed
 * FIX: session resumption
 * TODO: add support for server cert chain validation
 * TODO: add support for CA cert validation
 * TODO: add support for EAP-TLS (client cert/key conf)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <wincrypt.h>
#include <schannel.h>
#define SECURITY_WIN32
#include <security.h>
#include <sspi.h>

#include "common.h"
#include "tls.h"


struct tls_global {
	HMODULE hsecurity;
	PSecurityFunctionTable sspi;
	HCERTSTORE my_cert_store;
};

struct tls_connection {
	int established, start;
	int failed, read_alerts, write_alerts;

	SCHANNEL_CRED schannel_cred;
	CredHandle creds;
	CtxtHandle context;

	u8 eap_tls_prf[128];
	int eap_tls_prf_set;
};


static int schannel_load_lib(struct tls_global *global)
{
	INIT_SECURITY_INTERFACE pInitSecurityInterface;

	global->hsecurity = LoadLibrary("Secur32.dll");
	if (global->hsecurity == NULL) {
		wpa_printf(MSG_ERROR, "%s: Could not load Secur32.dll - 0x%x",
			   __func__, (unsigned int) GetLastError());
		return -1;
	}

	pInitSecurityInterface = (INIT_SECURITY_INTERFACE) GetProcAddress(
		global->hsecurity, "InitSecurityInterfaceA");
	if (pInitSecurityInterface == NULL) {
		wpa_printf(MSG_ERROR, "%s: Could not find "
			   "InitSecurityInterfaceA from Secur32.dll",
			   __func__);
		FreeLibrary(global->hsecurity);
		global->hsecurity = NULL;
		return -1;
	}

	global->sspi = pInitSecurityInterface();
	if (global->sspi == NULL) {
		wpa_printf(MSG_ERROR, "%s: Could not read security "
			   "interface - 0x%x",
			   __func__, (unsigned int) GetLastError());
		FreeLibrary(global->hsecurity);
		global->hsecurity = NULL;
		return -1;
	}

	return 0;
}


void * tls_init(const struct tls_config *conf)
{
	struct tls_global *global;

	global = malloc(sizeof(*global));
	if (global == NULL)
		return NULL;
	memset(global, 0, sizeof(*global));
	if (schannel_load_lib(global)) {
		free(global);
		return NULL;
	}
	return global;
}


void tls_deinit(void *ssl_ctx)
{
	struct tls_global *global = ssl_ctx;

	if (global->my_cert_store)
		CertCloseStore(global->my_cert_store, 0);
	FreeLibrary(global->hsecurity);
	free(global);
}


int tls_get_errors(void *ssl_ctx)
{
	return 0;
}


struct tls_connection * tls_connection_init(void *ssl_ctx)
{
	struct tls_connection *conn;

	conn = malloc(sizeof(*conn));
	if (conn == NULL)
		return NULL;
	memset(conn, 0, sizeof(*conn));
	conn->start = 1;

	return conn;
}


void tls_connection_deinit(void *ssl_ctx, struct tls_connection *conn)
{
	if (conn == NULL)
		return;

	free(conn);
}


int tls_connection_established(void *ssl_ctx, struct tls_connection *conn)
{
	return conn ? conn->established : 0;
}


int tls_connection_shutdown(void *ssl_ctx, struct tls_connection *conn)
{
	struct tls_global *global = ssl_ctx;
	if (conn == NULL)
		return -1;

	conn->eap_tls_prf_set = 0;
	conn->established = conn->failed = 0;
	conn->read_alerts = conn->write_alerts = 0;
	global->sspi->DeleteSecurityContext(&conn->context);
	/* FIX: what else needs to be reseted? */

	return 0;
}


int tls_global_ca_cert(void *_ssl_ctx, const char *ca_cert)
{
	return -1;
}


int tls_global_set_verify(void *ssl_ctx, int check_crl)
{
	return -1;
}


int tls_connection_set_verify(void *ssl_ctx, struct tls_connection *conn,
			      int verify_peer)
{
	return -1;
}


int tls_global_client_cert(void *_ssl_ctx, const char *client_cert)
{
	return -1;
}


int tls_global_private_key(void *_ssl_ctx, const char *private_key,
			   const char *private_key_passwd)
{
	return -1;
}


int tls_connection_get_keys(void *ssl_ctx, struct tls_connection *conn,
			    struct tls_keys *keys)
{
	if (conn == NULL || keys == NULL || !conn->eap_tls_prf_set)
		return -1;

	memset(keys, 0, sizeof(*keys));

	/*
	 * Cannot get master_key from Schannel, but EapKeyBlock can be used to
	 * generate session keys for EAP-TLS and EAP-PEAPv0. EAP-PEAPv2 and
	 * EAP-TTLS cannot use this, though, since they are using different
	 * labels. The only option could be to implement TLSv1 completely here
	 * and just use Schannel or CryptoAPI for low-level crypto
	 * functionality..
	 */
	keys->eap_tls_prf = conn->eap_tls_prf;
	keys->eap_tls_prf_len = sizeof(conn->eap_tls_prf);

	return 0;
}


static u8 * tls_conn_hs_clienthello(struct tls_global *global,
				    struct tls_connection *conn,
				    size_t *out_len)
{
	DWORD sspi_flags, sspi_flags_out;
	SecBufferDesc outbuf;
	SecBuffer outbufs[1];
	SECURITY_STATUS status;
	TimeStamp ts_expiry;

	sspi_flags = ISC_REQ_REPLAY_DETECT |
		ISC_REQ_CONFIDENTIALITY |
		ISC_RET_EXTENDED_ERROR |
		ISC_REQ_ALLOCATE_MEMORY |
		ISC_REQ_MANUAL_CRED_VALIDATION;

	wpa_printf(MSG_DEBUG, "%s: Generating ClientHello", __func__);

	outbufs[0].pvBuffer = NULL;
	outbufs[0].BufferType = SECBUFFER_TOKEN;
	outbufs[0].cbBuffer = 0;

	outbuf.cBuffers = 1;
	outbuf.pBuffers = outbufs;
	outbuf.ulVersion = SECBUFFER_VERSION;

	status = global->sspi->InitializeSecurityContextA(
		&conn->creds, NULL, NULL /* server name */, sspi_flags, 0,
		SECURITY_NATIVE_DREP, NULL, 0, &conn->context,
		&outbuf, &sspi_flags_out, &ts_expiry);
	if (status != SEC_I_CONTINUE_NEEDED) {
		wpa_printf(MSG_ERROR, "%s: InitializeSecurityContextA "
			   "failed - 0x%x",
			   __func__, (unsigned int) status);
		return NULL;
	}

	if (outbufs[0].cbBuffer != 0 && outbufs[0].pvBuffer) {
		u8 *buf;
		wpa_hexdump(MSG_MSGDUMP, "SChannel - ClientHello",
			    outbufs[0].pvBuffer, outbufs[0].cbBuffer);
		conn->start = 0;
		*out_len = outbufs[0].cbBuffer;
		buf = malloc(*out_len);
		if (buf == NULL)
			return NULL;
		memcpy(buf, outbufs[0].pvBuffer, *out_len);
		global->sspi->FreeContextBuffer(outbufs[0].pvBuffer);
		return buf;
	}

	wpa_printf(MSG_ERROR, "SChannel: Failed to generate ClientHello");

	return NULL;
}


#ifndef SECPKG_ATTR_EAP_KEY_BLOCK
#define SECPKG_ATTR_EAP_KEY_BLOCK 0x5b

typedef struct _SecPkgContext_EapKeyBlock {
	BYTE rgbKeys[128];
	BYTE rgbIVs[64];
} SecPkgContext_EapKeyBlock, *PSecPkgContext_EapKeyBlock;
#endif /* !SECPKG_ATTR_EAP_KEY_BLOCK */

static int tls_get_eap(struct tls_global *global, struct tls_connection *conn)
{
	SECURITY_STATUS status;
	SecPkgContext_EapKeyBlock kb;

	/* Note: Windows NT and Windows Me/98/95 do not support getting
	 * EapKeyBlock */

	status = global->sspi->QueryContextAttributes(
		&conn->context, SECPKG_ATTR_EAP_KEY_BLOCK, &kb);
	if (status != SEC_E_OK) {
		wpa_printf(MSG_DEBUG, "%s: QueryContextAttributes("
			   "SECPKG_ATTR_EAP_KEY_BLOCK) failed (%d)",
			   __func__, (int) status);
		return -1;
	}

	wpa_hexdump_key(MSG_MSGDUMP, "Schannel - EapKeyBlock - rgbKeys",
			kb.rgbKeys, sizeof(kb.rgbKeys));
	wpa_hexdump_key(MSG_MSGDUMP, "Schannel - EapKeyBlock - rgbIVs",
			kb.rgbIVs, sizeof(kb.rgbIVs));

	memcpy(conn->eap_tls_prf, kb.rgbKeys, sizeof(kb.rgbKeys));
	conn->eap_tls_prf_set = 1;
}


u8 * tls_connection_handshake(void *ssl_ctx, struct tls_connection *conn,
			      const u8 *in_data, size_t in_len,
			      size_t *out_len)
{
	struct tls_global *global = ssl_ctx;
	DWORD sspi_flags, sspi_flags_out;
	SecBufferDesc inbuf, outbuf;
	SecBuffer inbufs[2], outbufs[1];
	SECURITY_STATUS status;
	TimeStamp ts_expiry;
	u8 *out_buf = NULL;

	if (conn->start) {
		return tls_conn_hs_clienthello(global, conn, out_len);
	}

	wpa_printf(MSG_DEBUG, "SChannel: %d bytes handshake data to process",
		   in_len);

	sspi_flags = ISC_REQ_REPLAY_DETECT |
		ISC_REQ_CONFIDENTIALITY |
		ISC_RET_EXTENDED_ERROR |
		ISC_REQ_ALLOCATE_MEMORY |
		ISC_REQ_MANUAL_CRED_VALIDATION;

	/* Input buffer for Schannel */
	inbufs[0].pvBuffer = (u8 *) in_data;
	inbufs[0].cbBuffer = in_len;
	inbufs[0].BufferType = SECBUFFER_TOKEN;

	/* Place for leftover data from Schannel */
	inbufs[1].pvBuffer = NULL;
	inbufs[1].cbBuffer = 0;
	inbufs[1].BufferType = SECBUFFER_EMPTY;

	inbuf.cBuffers = 2;
	inbuf.pBuffers = inbufs;
	inbuf.ulVersion = SECBUFFER_VERSION;

	/* Output buffer for Schannel */
	outbufs[0].pvBuffer = NULL;
	outbufs[0].cbBuffer = 0;
	outbufs[0].BufferType = SECBUFFER_TOKEN;

	outbuf.cBuffers = 1;
	outbuf.pBuffers = outbufs;
	outbuf.ulVersion = SECBUFFER_VERSION;

	status = global->sspi->InitializeSecurityContextA(
		&conn->creds, &conn->context, NULL, sspi_flags, 0,
		SECURITY_NATIVE_DREP, &inbuf, 0, NULL,
		&outbuf, &sspi_flags_out, &ts_expiry);

	wpa_printf(MSG_MSGDUMP, "Schannel: InitializeSecurityContextA -> "
		   "status=%d inlen[0]=%d intype[0]=%d inlen[1]=%d "
		   "intype[1]=%d outlen[0]=%d",
		   (int) status, (int) inbufs[0].cbBuffer,
		   (int) inbufs[0].BufferType, (int) inbufs[1].cbBuffer,
		   (int) inbufs[1].BufferType,
		   (int) outbufs[0].cbBuffer);
	if (status == SEC_E_OK || status == SEC_I_CONTINUE_NEEDED ||
	    (FAILED(status) && (sspi_flags_out & ISC_RET_EXTENDED_ERROR))) {
		if (outbufs[0].cbBuffer != 0 && outbufs[0].pvBuffer) {
			wpa_hexdump(MSG_MSGDUMP, "SChannel - output",
				    outbufs[0].pvBuffer, outbufs[0].cbBuffer);
			*out_len = outbufs[0].cbBuffer;
			out_buf = malloc(*out_len);
			if (out_buf == NULL)
				return NULL;
			memcpy(out_buf, outbufs[0].pvBuffer, *out_len);
			global->sspi->FreeContextBuffer(outbufs[0].pvBuffer);
			outbufs[0].pvBuffer = NULL;
		}
	}

	switch (status) {
	case SEC_E_INCOMPLETE_MESSAGE:
		wpa_printf(MSG_DEBUG, "Schannel: SEC_E_INCOMPLETE_MESSAGE");
		break;
	case SEC_I_CONTINUE_NEEDED:
		wpa_printf(MSG_DEBUG, "Schannel: SEC_I_CONTINUE_NEEDED");
		break;
	case SEC_E_OK:
		/* TODO: verify server certificate chain */
		wpa_printf(MSG_DEBUG, "Schannel: SEC_E_OK - Handshake "
			   "completed successfully");
		conn->established = 1;
		tls_get_eap(global, conn);

		/* Need to return something to get final TLS ACK. */
		if (out_buf == NULL)
			out_buf = malloc(1);

		if (inbufs[1].BufferType == SECBUFFER_EXTRA) {
			wpa_hexdump(MSG_MSGDUMP, "SChannel - Encrypted "
				    "application data",
				    inbufs[1].pvBuffer, inbufs[1].cbBuffer);
			/* FIX: need to fix TLS API to allow this data to be
			 * passed to the caller */
			global->sspi->FreeContextBuffer(inbufs[1].pvBuffer);
			inbufs[1].pvBuffer = NULL;
		}
		break;
	case SEC_I_INCOMPLETE_CREDENTIALS:
		wpa_printf(MSG_DEBUG,
			   "Schannel: SEC_I_INCOMPLETE_CREDENTIALS");
		break;
	case SEC_E_WRONG_PRINCIPAL:
		wpa_printf(MSG_DEBUG, "Schannel: SEC_E_WRONG_PRINCIPAL");
		break;
	case SEC_E_INTERNAL_ERROR:
		wpa_printf(MSG_DEBUG, "Schannel: SEC_E_INTERNAL_ERROR");
		break;
	}

	if (FAILED(status)) {
		wpa_printf(MSG_DEBUG, "Schannel: Handshake failed "
			   "(out_buf=%p)", out_buf);
		conn->failed++;
		global->sspi->DeleteSecurityContext(&conn->context);
		return out_buf;
	}

	if (inbufs[1].BufferType == SECBUFFER_EXTRA) {
		/* TODO: Can this happen? What to do with this data? */
		wpa_hexdump(MSG_MSGDUMP, "SChannel - Leftover data",
			    inbufs[1].pvBuffer, inbufs[1].cbBuffer);
		global->sspi->FreeContextBuffer(inbufs[1].pvBuffer);
		inbufs[1].pvBuffer = NULL;
	}

	return out_buf;
}


u8 * tls_connection_server_handshake(void *ssl_ctx,
				     struct tls_connection *conn,
				     const u8 *in_data, size_t in_len,
				     size_t *out_len)
{
	return NULL;
}


int tls_connection_encrypt(void *ssl_ctx, struct tls_connection *conn,
			   const u8 *in_data, size_t in_len,
			   u8 *out_data, size_t out_len)
{
	struct tls_global *global = ssl_ctx;
	SECURITY_STATUS status;
	SecBufferDesc buf;
	SecBuffer bufs[4];
	SecPkgContext_StreamSizes sizes;
	int i;
	size_t total_len;

	status = global->sspi->QueryContextAttributes(&conn->context,
						      SECPKG_ATTR_STREAM_SIZES,
						      &sizes);
	if (status != SEC_E_OK) {
		wpa_printf(MSG_DEBUG, "%s: QueryContextAttributes failed",
			   __func__);
		return -1;
	}
	wpa_printf(MSG_DEBUG, "%s: Stream sizes: header=%u trailer=%u",
		   __func__,
		   (unsigned int) sizes.cbHeader,
		   (unsigned int) sizes.cbTrailer);

	total_len = sizes.cbHeader + in_len + sizes.cbTrailer;

	if (out_len < total_len) {
		wpa_printf(MSG_DEBUG, "%s: too short out_data (out_len=%lu "
			   "in_len=%lu total_len=%lu)", __func__,
			   (unsigned long) out_len, (unsigned long) in_len,
			   (unsigned long) total_len);
		return -1;
	}

	memset(&bufs, 0, sizeof(bufs));
	bufs[0].pvBuffer = out_data;
	bufs[0].cbBuffer = sizes.cbHeader;
	bufs[0].BufferType = SECBUFFER_STREAM_HEADER;

	memcpy(out_data + sizes.cbHeader, in_data, in_len);
	bufs[1].pvBuffer = out_data + sizes.cbHeader;
	bufs[1].cbBuffer = in_len;
	bufs[1].BufferType = SECBUFFER_DATA;

	bufs[2].pvBuffer = out_data + sizes.cbHeader + in_len;
	bufs[2].cbBuffer = sizes.cbTrailer;
	bufs[2].BufferType = SECBUFFER_STREAM_TRAILER;

	buf.ulVersion = SECBUFFER_VERSION;
	buf.cBuffers = 3;
	buf.pBuffers = bufs;

	status = global->sspi->EncryptMessage(&conn->context, 0, &buf, 0);

	wpa_printf(MSG_MSGDUMP, "Schannel: EncryptMessage -> "
		   "status=%d len[0]=%d type[0]=%d len[1]=%d type[1]=%d "
		   "len[2]=%d type[2]=%d",
		   (int) status,
		   (int) bufs[0].cbBuffer, (int) bufs[0].BufferType,
		   (int) bufs[1].cbBuffer, (int) bufs[1].BufferType,
		   (int) bufs[2].cbBuffer, (int) bufs[2].BufferType);
	wpa_printf(MSG_MSGDUMP, "Schannel: EncryptMessage pointers: "
		   "out_data=%p bufs %p %p %p",
		   out_data, bufs[0].pvBuffer, bufs[1].pvBuffer,
		   bufs[2].pvBuffer);

	for (i = 0; i < 3; i++) {
		if (bufs[i].pvBuffer && bufs[i].BufferType != SECBUFFER_EMPTY)
		{
			wpa_hexdump(MSG_MSGDUMP, "SChannel: bufs",
				    bufs[i].pvBuffer, bufs[i].cbBuffer);
		}
	}

	if (status == SEC_E_OK) {
		wpa_printf(MSG_DEBUG, "%s: SEC_E_OK", __func__);
		wpa_hexdump_key(MSG_MSGDUMP, "Schannel: Encrypted data from "
				"EncryptMessage", out_data, total_len);
		return total_len;
	}

	wpa_printf(MSG_DEBUG, "%s: Failed - status=%d",
		   __func__, (int) status);
	return -1;
}


int tls_connection_decrypt(void *ssl_ctx, struct tls_connection *conn,
			   const u8 *in_data, size_t in_len,
			   u8 *out_data, size_t out_len)
{
	struct tls_global *global = ssl_ctx;
	SECURITY_STATUS status;
	SecBufferDesc buf;
	SecBuffer bufs[4];
	int i;

	if (out_len < in_len) {
		wpa_printf(MSG_DEBUG, "%s: out_len=%lu < in_len=%lu", __func__,
			   (unsigned long) out_len, (unsigned long) in_len);
		return -1;
	}

	wpa_hexdump(MSG_MSGDUMP, "Schannel: Encrypted data to DecryptMessage",
		    in_data, in_len);
	memset(&bufs, 0, sizeof(bufs));
	memcpy(out_data, in_data, in_len);
	bufs[0].pvBuffer = out_data;
	bufs[0].cbBuffer = in_len;
	bufs[0].BufferType = SECBUFFER_DATA;

	bufs[1].BufferType = SECBUFFER_EMPTY;
	bufs[2].BufferType = SECBUFFER_EMPTY;
	bufs[3].BufferType = SECBUFFER_EMPTY;

	buf.ulVersion = SECBUFFER_VERSION;
	buf.cBuffers = 4;
	buf.pBuffers = bufs;

	status = global->sspi->DecryptMessage(&conn->context, &buf, 0,
						    NULL);
	wpa_printf(MSG_MSGDUMP, "Schannel: DecryptMessage -> "
		   "status=%d len[0]=%d type[0]=%d len[1]=%d type[1]=%d "
		   "len[2]=%d type[2]=%d len[3]=%d type[3]=%d",
		   (int) status,
		   (int) bufs[0].cbBuffer, (int) bufs[0].BufferType,
		   (int) bufs[1].cbBuffer, (int) bufs[1].BufferType,
		   (int) bufs[2].cbBuffer, (int) bufs[2].BufferType,
		   (int) bufs[3].cbBuffer, (int) bufs[3].BufferType);
	wpa_printf(MSG_MSGDUMP, "Schannel: DecryptMessage pointers: "
		   "out_data=%p bufs %p %p %p %p",
		   out_data, bufs[0].pvBuffer, bufs[1].pvBuffer,
		   bufs[2].pvBuffer, bufs[3].pvBuffer);

	switch (status) {
	case SEC_E_INCOMPLETE_MESSAGE:
		wpa_printf(MSG_DEBUG, "%s: SEC_E_INCOMPLETE_MESSAGE",
			   __func__);
		break;
	case SEC_E_OK:
		wpa_printf(MSG_DEBUG, "%s: SEC_E_OK", __func__);
		for (i = 0; i < 4; i++) {
			if (bufs[i].BufferType == SECBUFFER_DATA)
				break;
		}
		if (i == 4) {
			wpa_printf(MSG_DEBUG, "%s: No output data from "
				   "DecryptMessage", __func__);
			return -1;
		}
		wpa_hexdump_key(MSG_MSGDUMP, "Schannel: Decrypted data from "
				"DecryptMessage",
				bufs[i].pvBuffer, bufs[i].cbBuffer);
		if (bufs[i].cbBuffer > out_len) {
			wpa_printf(MSG_DEBUG, "%s: Too long output data",
				   __func__);
			return -1;
		}
		memmove(out_data, bufs[i].pvBuffer, bufs[i].cbBuffer);
		return bufs[i].cbBuffer;
	}

	wpa_printf(MSG_DEBUG, "%s: Failed - status=%d",
		   __func__, (int) status);
	return -1;
}


int tls_connection_resumed(void *ssl_ctx, struct tls_connection *conn)
{
	return 0;
}


#ifdef EAP_FAST
int tls_connection_set_master_key(void *ssl_ctx, struct tls_connection *conn,
				  const u8 *key, size_t key_len)
{
	return -1;
}
#endif /* EAP_FAST */


int tls_connection_set_anon_dh(void *ssl_ctx, struct tls_connection *conn)
{
	return -1;
}


int tls_get_cipher(void *ssl_ctx, struct tls_connection *conn,
		   char *buf, size_t buflen)
{
	return -1;
}


int tls_connection_enable_workaround(void *ssl_ctx,
				     struct tls_connection *conn)
{
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


int tls_connection_set_params(void *tls_ctx, struct tls_connection *conn,
			      const struct tls_connection_params *params)
{
	struct tls_global *global = tls_ctx;
	ALG_ID algs[1];
	SECURITY_STATUS status;
	TimeStamp ts_expiry;

	if (conn == NULL)
		return -1;

	if (global->my_cert_store == NULL &&
	    (global->my_cert_store = CertOpenSystemStore(0, "MY")) == NULL) {
		wpa_printf(MSG_ERROR, "%s: CertOpenSystemStore failed - 0x%x",
			   __func__, (unsigned int) GetLastError());
		return -1;
	}

	memset(&conn->schannel_cred, 0, sizeof(conn->schannel_cred));
	conn->schannel_cred.dwVersion = SCHANNEL_CRED_VERSION;
	conn->schannel_cred.grbitEnabledProtocols = SP_PROT_TLS1;
	algs[0] = CALG_RSA_KEYX;
	conn->schannel_cred.cSupportedAlgs = 1;
	conn->schannel_cred.palgSupportedAlgs = algs;
	conn->schannel_cred.dwFlags |= SCH_CRED_NO_DEFAULT_CREDS;
	status = global->sspi->AcquireCredentialsHandleA(
		NULL, UNISP_NAME_A, SECPKG_CRED_OUTBOUND, NULL,
		&conn->schannel_cred, NULL, NULL, &conn->creds, &ts_expiry);
	if (status != SEC_E_OK) {
		wpa_printf(MSG_DEBUG, "%s: AcquireCredentialsHandleA failed - "
			   "0x%x", __func__, (unsigned int) status);
		return -1;
	}

	return 0;
}
