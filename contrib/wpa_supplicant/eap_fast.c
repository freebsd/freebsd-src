/*
 * WPA Supplicant / EAP-FAST (draft-cam-winget-eap-fast-00.txt)
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

#include "common.h"
#include "eap_i.h"
#include "eap_tls_common.h"
#include "wpa_supplicant.h"
#include "config_ssid.h"
#include "tls.h"
#include "eap_tlv.h"
#include "sha1.h"

/* TODO:
 * - encrypt PAC-Key in the PAC file
 * - test session resumption and enable it if it interoperates
 */

#define EAP_FAST_VERSION 1
#define EAP_FAST_KEY_LEN 64
#define EAP_FAST_PAC_KEY_LEN 32

#define TLS_EXT_PAC_OPAQUE 35

static char *pac_file_hdr = "wpa_supplicant EAP-FAST PAC file - version 1";


static void eap_fast_deinit(struct eap_sm *sm, void *priv);


#define PAC_TYPE_PAC_KEY 1
#define PAC_TYPE_PAC_OPAQUE 2
#define PAC_TYPE_CRED_LIFETIME 3
#define PAC_TYPE_A_ID 4
#define PAC_TYPE_I_ID 5
#define PAC_TYPE_SERVER_PROTECTED_DATA 6
#define PAC_TYPE_A_ID_INFO 7
#define PAC_TYPE_PAC_ACKNOWLEDGEMENT 8
#define PAC_TYPE_PAC_INFO 9

struct pac_tlv_hdr {
	u16 type;
	u16 len;
};


/* draft-cam-winget-eap-fast-00.txt, 6.2 EAP-FAST Authentication Phase 1 */
struct eap_fast_key_block_auth {
	/* RC4-128-SHA */
	u8 client_write_MAC_secret[20];
	u8 server_write_MAC_secret[20];
	u8 client_write_key[16];
	u8 server_write_key[16];
	/* u8 client_write_IV[0]; */
	/* u8 server_write_IV[0]; */
	u8 session_key_seed[40];
};


/* draft-cam-winget-eap-fast-00.txt, 7.3 EAP-FAST Provisioning Exchange */
struct eap_fast_key_block_provisioning {
	/* AES128-SHA */
	u8 client_write_MAC_secret[20];
	u8 server_write_MAC_secret[20];
	u8 client_write_key[16];
	u8 server_write_key[16];
	u8 client_write_IV[16];
	u8 server_write_IV[16];
	u8 session_key_seed[40];
	u8 server_challenge[16];
	u8 client_challenge[16];
};


struct eap_fast_pac {
	struct eap_fast_pac *next;

	u8 pac_key[EAP_FAST_PAC_KEY_LEN];
	u8 *pac_opaque;
	size_t pac_opaque_len;
	u8 *pac_info;
	size_t pac_info_len;
	u8 *a_id;
	size_t a_id_len;
	u8 *i_id;
	size_t i_id_len;
	u8 *a_id_info;
	size_t a_id_info_len;
};


struct eap_fast_data {
	struct eap_ssl_data ssl;

	int fast_version;

	const struct eap_method *phase2_method;
	void *phase2_priv;
	int phase2_success;

	u8 phase2_type;
	u8 *phase2_types;
	size_t num_phase2_types;
	int resuming; /* starting a resumed session */
	struct eap_fast_key_block_auth *key_block_a;
	struct eap_fast_key_block_provisioning *key_block_p;
	int provisioning_allowed; /* is PAC provisioning allowed */
	int provisioning; /* doing PAC provisioning (not the normal auth) */

	u8 key_data[EAP_FAST_KEY_LEN];
	int success;

	struct eap_fast_pac *pac;
	struct eap_fast_pac *current_pac;

	int tls_master_secret_set;
};


static void eap_fast_free_pac(struct eap_fast_pac *pac)
{
	free(pac->pac_opaque);
	free(pac->pac_info);
	free(pac->a_id);
	free(pac->i_id);
	free(pac->a_id_info);
	free(pac);
}


static struct eap_fast_pac * eap_fast_get_pac(struct eap_fast_data *data,
					      const u8 *a_id, size_t a_id_len)
{
	struct eap_fast_pac *pac = data->pac;

	while (pac) {
		if (pac->a_id_len == a_id_len &&
		    memcmp(pac->a_id, a_id, a_id_len) == 0) {
			return pac;
		}
		pac = pac->next;
	}
	return NULL;
}


static int eap_fast_add_pac(struct eap_fast_data *data,
			    struct eap_fast_pac *entry)
{
	struct eap_fast_pac *pac, *prev;

	if (entry == NULL || entry->a_id == NULL)
		return -1;

	/* Remove a possible old entry for the matching A-ID. */
	pac = data->pac;
	prev = NULL;
	while (pac) {
		if (pac->a_id_len == entry->a_id_len &&
		    memcmp(pac->a_id, entry->a_id, pac->a_id_len) == 0) {
			if (prev == NULL) {
				data->pac = pac->next;
			} else {
				prev->next = pac->next;
			}
			if (data->current_pac == pac)
				data->current_pac = NULL;
			eap_fast_free_pac(pac);
			break;
		}
		prev = pac;
		pac = pac->next;
	}

	/* Allocate a new entry and add it to the list of PACs. */
	pac = malloc(sizeof(*pac));
	if (pac == NULL)
		return -1;

	memset(pac, 0, sizeof(*pac));
	memcpy(pac->pac_key, entry->pac_key, EAP_FAST_PAC_KEY_LEN);
	if (entry->pac_opaque) {
		pac->pac_opaque = malloc(entry->pac_opaque_len);
		if (pac->pac_opaque == NULL) {
			eap_fast_free_pac(pac);
			return -1;
		}
		memcpy(pac->pac_opaque, entry->pac_opaque,
		       entry->pac_opaque_len);
		pac->pac_opaque_len = entry->pac_opaque_len;
	}
	if (entry->pac_info) {
		pac->pac_info = malloc(entry->pac_info_len);
		if (pac->pac_info == NULL) {
			eap_fast_free_pac(pac);
			return -1;
		}
		memcpy(pac->pac_info, entry->pac_info,
		       entry->pac_info_len);
		pac->pac_info_len = entry->pac_info_len;
	}
	if (entry->a_id) {
		pac->a_id = malloc(entry->a_id_len);
		if (pac->a_id == NULL) {
			eap_fast_free_pac(pac);
			return -1;
		}
		memcpy(pac->a_id, entry->a_id,
		       entry->a_id_len);
		pac->a_id_len = entry->a_id_len;
	}
	if (entry->i_id) {
		pac->i_id = malloc(entry->i_id_len);
		if (pac->i_id == NULL) {
			eap_fast_free_pac(pac);
			return -1;
		}
		memcpy(pac->i_id, entry->i_id,
		       entry->i_id_len);
		pac->i_id_len = entry->i_id_len;
	}
	if (entry->a_id_info) {
		pac->a_id_info = malloc(entry->a_id_info_len);
		if (pac->a_id_info == NULL) {
			eap_fast_free_pac(pac);
			return -1;
		}
		memcpy(pac->a_id_info, entry->a_id_info,
		       entry->a_id_info_len);
		pac->a_id_info_len = entry->a_id_info_len;
	}
	pac->next = data->pac;
	data->pac = pac;
	return 0;
}


static int eap_fast_read_line(FILE *f, char *buf, size_t buf_len)
{
	char *pos;

	if (fgets(buf, buf_len, f) == NULL) {
		return -1;
	}

	buf[buf_len - 1] = '\0';
	pos = buf;
	while (*pos != '\0') {
		if (*pos == '\n' || *pos == '\r') {
			*pos = '\0';
			break;
		}
		pos++;
	}

	return 0;
}


static u8 * eap_fast_parse_hex(const char *value, size_t *len)
{
	int hlen;
	u8 *buf;

	if (value == NULL)
		return NULL;
	hlen = strlen(value);
	if (hlen & 1)
		return NULL;
	*len = hlen / 2;
	buf = malloc(*len);
	if (buf == NULL)
		return NULL;
	if (hexstr2bin(value, buf, *len)) {
		free(buf);
		return NULL;
	}
	return buf;
}


static int eap_fast_load_pac(struct eap_fast_data *data, const char *pac_file)
{
	FILE *f;
	struct eap_fast_pac *pac = NULL;
	int count = 0;
	char *buf, *pos;
	const int buf_len = 2048;
	int ret = 0, line = 0;

	if (pac_file == NULL)
		return -1;

	f = fopen(pac_file, "r");
	if (f == NULL) {
		wpa_printf(MSG_INFO, "EAP-FAST: No PAC file '%s' - assume no "
			   "PAC entries have been provisioned", pac_file);
		return 0;
	}

	buf = malloc(buf_len);
	if (buf == NULL) {
		return -1;
	}

	line++;
	if (eap_fast_read_line(f, buf, buf_len) < 0 ||
	    strcmp(pac_file_hdr, buf) != 0) {
		wpa_printf(MSG_INFO, "EAP-FAST: Unrecognized header line in "
			   "PAC file '%s'", pac_file);
		free(buf);
		fclose(f);
		return -1;
	}

	while (eap_fast_read_line(f, buf, buf_len) == 0) {
		line++;
		pos = strchr(buf, '=');
		if (pos) {
			*pos++ = '\0';
		}

		if (strcmp(buf, "START") == 0) {
			if (pac) {
				wpa_printf(MSG_INFO, "EAP-FAST: START line "
					   "without END in '%s:%d'",
					   pac_file, line);
				ret = -1;
				break;
			}
			pac = malloc(sizeof(*pac));
			if (pac == NULL) {
				wpa_printf(MSG_INFO, "EAP-FAST: No memory for "
					   "PAC entry");
				ret = -1;
				break;
			}
			memset(pac, 0, sizeof(*pac));
		} else if (strcmp(buf, "END") == 0) {
			if (pac == NULL) {
				wpa_printf(MSG_INFO, "EAP-FAST: END line "
					   "without START in '%s:%d'",
					   pac_file, line);
				ret = -1;
				break;
			}
			pac->next = data->pac;
			data->pac = pac;
			pac = NULL;
			count++;
		} else if (pac && strcmp(buf, "PAC-Key") == 0) {
			u8 *key;
			size_t key_len;
			key = eap_fast_parse_hex(pos, &key_len);
			if (key == NULL || key_len != EAP_FAST_PAC_KEY_LEN) {
				wpa_printf(MSG_INFO, "EAP-FAST: Invalid "
					   "PAC-Key '%s:%d'", pac_file, line);
				ret = -1;
				free(key);
				break;
			}

			memcpy(pac->pac_key, key, EAP_FAST_PAC_KEY_LEN);
			free(key);
		} else if (pac && strcmp(buf, "PAC-Opaque") == 0) {
			pac->pac_opaque =
				eap_fast_parse_hex(pos, &pac->pac_opaque_len);
			if (pac->pac_opaque == NULL) {
				wpa_printf(MSG_INFO, "EAP-FAST: Invalid "
					   "PAC-Opaque '%s:%d'",
					   pac_file, line);
				ret = -1;
				break;
			}
		} else if (pac && strcmp(buf, "A-ID") == 0) {
			pac->a_id = eap_fast_parse_hex(pos, &pac->a_id_len);
			if (pac->a_id == NULL) {
				wpa_printf(MSG_INFO, "EAP-FAST: Invalid "
					   "A-ID '%s:%d'", pac_file, line);
				ret = -1;
				break;
			}
		} else if (pac && strcmp(buf, "I-ID") == 0) {
			pac->i_id = eap_fast_parse_hex(pos, &pac->i_id_len);
			if (pac->i_id == NULL) {
				wpa_printf(MSG_INFO, "EAP-FAST: Invalid "
					   "I-ID '%s:%d'", pac_file, line);
				ret = -1;
				break;
			}
		} else if (pac && strcmp(buf, "A-ID-Info") == 0) {
			pac->a_id_info =
				eap_fast_parse_hex(pos, &pac->a_id_info_len);
			if (pac->a_id_info == NULL) {
				wpa_printf(MSG_INFO, "EAP-FAST: Invalid "
					   "A-ID-Info '%s:%d'",
					   pac_file, line);
				ret = -1;
				break;
			}
		}
	}

	if (pac) {
		wpa_printf(MSG_INFO, "EAP-FAST: PAC block not terminated with "
			   "END in '%s'", pac_file);
		eap_fast_free_pac(pac);
		ret = -1;
	}

	free(buf);
	fclose(f);

	if (ret == 0) {
		wpa_printf(MSG_DEBUG, "EAP-FAST: read %d PAC entries from "
			   "'%s'", count, pac_file);
	}

	return ret;
}


static void eap_fast_write(FILE *f, const char *field, const u8 *data,
			   size_t len, int txt)
{
	int i;

	if (data == NULL)
		return;

	fprintf(f, "%s=", field);
	for (i = 0; i < len; i++) {
		fprintf(f, "%02x", data[i]);
	}
	fprintf(f, "\n");

	if (txt) {
		fprintf(f, "%s-txt=", field);
		for (i = 0; i < len; i++) {
			fprintf(f, "%c", data[i]);
		}
		fprintf(f, "\n");
	}
}


static int eap_fast_save_pac(struct eap_fast_data *data, const char *pac_file)
{
	FILE *f;
	struct eap_fast_pac *pac;
	int count = 0;

	if (pac_file == NULL)
		return -1;

	f = fopen(pac_file, "w");
	if (f == NULL) {
		wpa_printf(MSG_INFO, "EAP-FAST: Failed to open PAC file '%s' "
			   "for writing", pac_file);
		return -1;
	}

	fprintf(f, "%s\n", pac_file_hdr);

	pac = data->pac;
	while (pac) {
		fprintf(f, "START\n");
		eap_fast_write(f, "PAC-Key", pac->pac_key,
			       EAP_FAST_PAC_KEY_LEN, 0);
		eap_fast_write(f, "PAC-Opaque", pac->pac_opaque,
			       pac->pac_opaque_len, 0);
		eap_fast_write(f, "PAC-Info", pac->pac_info,
			       pac->pac_info_len, 0);
		eap_fast_write(f, "A-ID", pac->a_id, pac->a_id_len, 0);
		eap_fast_write(f, "I-ID", pac->i_id, pac->i_id_len, 1);
		eap_fast_write(f, "A-ID-Info", pac->a_id_info,
			       pac->a_id_info_len, 1);
		fprintf(f, "END\n");
		count++;
		pac = pac->next;
	}

	fclose(f);

	wpa_printf(MSG_DEBUG, "EAP-FAST: wrote %d PAC entries into '%s'",
		   count, pac_file);

	return 0;
}


static void * eap_fast_init(struct eap_sm *sm)
{
	struct eap_fast_data *data;
	struct wpa_ssid *config = eap_get_config(sm);

	data = malloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	memset(data, 0, sizeof(*data));
	data->fast_version = EAP_FAST_VERSION;

	if (config && config->phase1) {
		if (strstr(config->phase1, "fast_provisioning=1")) {
			data->provisioning_allowed = 1;
			wpa_printf(MSG_DEBUG, "EAP-FAST: Automatic PAC "
				   "provisioning is allowed");
		}
	}

	if (config && config->phase2) {
		char *start, *pos, *buf;
		u8 method, *methods = NULL, *_methods;
		size_t num_methods = 0;
		start = buf = strdup(config->phase2);
		if (buf == NULL) {
			eap_fast_deinit(sm, data);
			return NULL;
		}
		while (start && *start != '\0') {
			pos = strstr(start, "auth=");
			if (pos == NULL)
				break;
			if (start != pos && *(pos - 1) != ' ') {
				start = pos + 5;
				continue;
			}

			start = pos + 5;
			pos = strchr(start, ' ');
			if (pos)
				*pos++ = '\0';
			method = eap_get_phase2_type(start);
			if (method == EAP_TYPE_NONE) {
				wpa_printf(MSG_ERROR, "EAP-FAST: Unsupported "
					   "Phase2 method '%s'", start);
			} else {
				num_methods++;
				_methods = realloc(methods, num_methods);
				if (_methods == NULL) {
					free(methods);
					eap_fast_deinit(sm, data);
					return NULL;
				}
				methods = _methods;
				methods[num_methods - 1] = method;
			}

			start = pos;
		}
		free(buf);
		data->phase2_types = methods;
		data->num_phase2_types = num_methods;
	}
	if (data->phase2_types == NULL) {
		data->phase2_types =
			eap_get_phase2_types(config, &data->num_phase2_types);
	}
	if (data->phase2_types == NULL) {
		wpa_printf(MSG_ERROR, "EAP-FAST: No Phase2 method available");
		eap_fast_deinit(sm, data);
		return NULL;
	}
	wpa_hexdump(MSG_DEBUG, "EAP-FAST: Phase2 EAP types",
		    data->phase2_types, data->num_phase2_types);
	data->phase2_type = EAP_TYPE_NONE;

	if (eap_tls_ssl_init(sm, &data->ssl, config)) {
		wpa_printf(MSG_INFO, "EAP-FAST: Failed to initialize SSL.");
		eap_fast_deinit(sm, data);
		return NULL;
	}

	/* The local RADIUS server in a Cisco AP does not seem to like empty
	 * fragments before data, so disable that workaround for CBC.
	 * TODO: consider making this configurable */
	tls_connection_enable_workaround(sm->ssl_ctx, data->ssl.conn);

	if (eap_fast_load_pac(data, config->pac_file) < 0) {
		eap_fast_deinit(sm, data);
		return NULL;
	}

	if (data->pac == NULL && !data->provisioning_allowed) {
		wpa_printf(MSG_INFO, "EAP-FAST: No PAC configured and "
			   "provisioning disabled");
		eap_fast_deinit(sm, data);
		return NULL;
	}

	return data;
}


static void eap_fast_deinit(struct eap_sm *sm, void *priv)
{
	struct eap_fast_data *data = priv;
	struct eap_fast_pac *pac, *prev;

	if (data == NULL)
		return;
	if (data->phase2_priv && data->phase2_method)
		data->phase2_method->deinit(sm, data->phase2_priv);
	free(data->phase2_types);
	free(data->key_block_a);
	free(data->key_block_p);
	eap_tls_ssl_deinit(sm, &data->ssl);

	pac = data->pac;
	prev = NULL;
	while (pac) {
		prev = pac;
		pac = pac->next;
		eap_fast_free_pac(prev);
	}
	free(data);
}


static int eap_fast_encrypt(struct eap_sm *sm, struct eap_fast_data *data,
			    int id, u8 *plain, size_t plain_len,
			    u8 **out_data, size_t *out_len)
{
	int res;
	u8 *pos;
	struct eap_hdr *resp;

	/* TODO: add support for fragmentation, if needed. This will need to
	 * add TLS Message Length field, if the frame is fragmented. */
	resp = malloc(sizeof(struct eap_hdr) + 2 + data->ssl.tls_out_limit);
	if (resp == NULL)
		return 0;

	resp->code = EAP_CODE_RESPONSE;
	resp->identifier = id;

	pos = (u8 *) (resp + 1);
	*pos++ = EAP_TYPE_FAST;
	*pos++ = data->fast_version;

	res = tls_connection_encrypt(sm->ssl_ctx, data->ssl.conn,
				     plain, plain_len,
				     pos, data->ssl.tls_out_limit);
	if (res < 0) {
		wpa_printf(MSG_INFO, "EAP-FAST: Failed to encrypt Phase 2 "
			   "data");
		free(resp);
		return 0;
	}

	*out_len = sizeof(struct eap_hdr) + 2 + res;
	resp->length = host_to_be16(*out_len);
	*out_data = (u8 *) resp;
	return 0;
}


static int eap_fast_phase2_nak(struct eap_sm *sm,
			       struct eap_fast_data *data,
			       struct eap_hdr *hdr,
			       u8 **resp, size_t *resp_len)
{
	struct eap_hdr *resp_hdr;
	u8 *pos = (u8 *) (hdr + 1);

	wpa_printf(MSG_DEBUG, "EAP-FAST: Phase 2 Request: Nak type=%d", *pos);
	wpa_hexdump(MSG_DEBUG, "EAP-FAST: Allowed Phase2 EAP types",
		    data->phase2_types, data->num_phase2_types);
	*resp_len = sizeof(struct eap_hdr) + 1 + data->num_phase2_types;
	*resp = malloc(*resp_len);
	if (*resp == NULL)
		return -1;

	resp_hdr = (struct eap_hdr *) (*resp);
	resp_hdr->code = EAP_CODE_RESPONSE;
	resp_hdr->identifier = hdr->identifier;
	resp_hdr->length = host_to_be16(*resp_len);
	pos = (u8 *) (resp_hdr + 1);
	*pos++ = EAP_TYPE_NAK;
	memcpy(pos, data->phase2_types, data->num_phase2_types);

	return 0;
}


static int eap_fast_derive_msk(struct eap_sm *sm, struct eap_fast_data *data)
{
	u8 isk[32];
	u8 imck[60];

	if (data->key_block_a == NULL)
		return -1;

	memset(isk, 0, sizeof(isk));
	sha1_t_prf(data->key_block_a->session_key_seed,
		   sizeof(data->key_block_a->session_key_seed),
		   "Inner Methods Compound Keys",
		   isk, sizeof(isk), imck, sizeof(imck));
	sha1_t_prf(imck, 40, "Session Key Generating Function", (u8 *) "", 0,
		   data->key_data, EAP_FAST_KEY_LEN);

	wpa_hexdump_key(MSG_DEBUG, "EAP-FAST: Derived key (MSK)",
			data->key_data, EAP_FAST_KEY_LEN);

	data->success = 1;

	return 0;
}


static int eap_fast_set_tls_master_secret(struct eap_sm *sm,
					  struct eap_fast_data *data,
					  const u8 *tls, size_t tls_len)
{
	struct tls_keys keys;
	u8 master_secret[48], *seed;
	const u8 *server_random;
	size_t seed_len, server_random_len;

	if (data->tls_master_secret_set || !data->current_pac ||
	    tls_connection_get_keys(sm->ssl_ctx, data->ssl.conn, &keys)) {
		return 0;
	}

	wpa_hexdump(MSG_DEBUG, "EAP-FAST: client_random",
		    keys.client_random, keys.client_random_len);

	/* TLS master secret is needed before TLS library has processed this
	 * message which includes both ServerHello and an encrypted handshake
	 * message, so we need to parse server_random from this message before
	 * passing it to TLS library.
	 *
	 * Example TLS packet header:
	 * (16 03 01 00 2a 02 00 00 26 03 01 <32 bytes server_random>)
	 * Content Type: Handshake: 0x16
	 * Version: TLS 1.0 (0x0301)
	 * Lenghth: 42 (0x002a)
	 * Handshake Type: Server Hello: 0x02
	 * Length: 38 (0x000026)
	 * Version TLS 1.0 (0x0301)
	 * Random: 32 bytes
	 */
	if (tls_len < 43 || tls[0] != 0x16 ||
	    tls[1] != 0x03 || tls[2] != 0x01 ||
	    tls[5] != 0x02 || tls[9] != 0x03 || tls[10] != 0x01) {
		wpa_hexdump(MSG_DEBUG, "EAP-FAST: unrecognized TLS "
			    "ServerHello", tls, tls_len);
		return -1;
	}
	server_random = tls + 11;
	server_random_len = 32;
	wpa_hexdump(MSG_DEBUG, "EAP-FAST: server_random",
		    server_random, server_random_len);


	seed_len = keys.client_random_len + server_random_len;
	seed = malloc(seed_len);
	if (seed == NULL)
		return -1;
	memcpy(seed, server_random, server_random_len);
	memcpy(seed + server_random_len,
	       keys.client_random, keys.client_random_len);

	wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: T-PRF seed", seed, seed_len);
	wpa_hexdump_key(MSG_MSGDUMP, "EAP-FAST: PAC-Key",
			data->current_pac->pac_key, EAP_FAST_PAC_KEY_LEN);
	/* master_secret = T-PRF(PAC-Key, "PAC to master secret label hash", 
	 * server_random + client_random, 48) */
	sha1_t_prf(data->current_pac->pac_key, EAP_FAST_PAC_KEY_LEN,
		   "PAC to master secret label hash",
		   seed, seed_len, master_secret, sizeof(master_secret));
	free(seed);
	wpa_hexdump_key(MSG_DEBUG, "EAP-FAST: TLS pre-master-secret",
			master_secret, sizeof(master_secret));

	data->tls_master_secret_set = 1;

	return tls_connection_set_master_key(sm->ssl_ctx, data->ssl.conn,
					     master_secret,
					     sizeof(master_secret));
}


static u8 * eap_fast_derive_key(struct eap_sm *sm, struct eap_ssl_data *data,
				char *label, size_t len)
{
	struct tls_keys keys;
	u8 *random;
	u8 *out;

	if (tls_connection_get_keys(sm->ssl_ctx, data->conn, &keys))
		return NULL;
	out = malloc(len);
	random = malloc(keys.client_random_len + keys.server_random_len);
	if (out == NULL || random == NULL) {
		free(out);
		free(random);
		return NULL;
	}
	memcpy(random, keys.server_random, keys.server_random_len);
	memcpy(random + keys.server_random_len, keys.client_random,
	       keys.client_random_len);

	wpa_hexdump_key(MSG_MSGDUMP, "EAP-FAST: master_secret for key "
			"expansion", keys.master_key, keys.master_key_len);
	if (tls_prf(keys.master_key, keys.master_key_len,
		    label, random, keys.client_random_len +
		    keys.server_random_len, out, len)) {
		free(random);
		free(out);
		return NULL;
	}
	free(random);
	return out;
}


static void eap_fast_derive_key_auth(struct eap_sm *sm,
				     struct eap_fast_data *data)
{
	free(data->key_block_a);
	data->key_block_a = (struct eap_fast_key_block_auth *)
		eap_fast_derive_key(sm, &data->ssl, "key expansion",
				    sizeof(*data->key_block_a));
	wpa_hexdump_key(MSG_DEBUG, "EAP-FAST: session_key_seed",
			data->key_block_a->session_key_seed,
			sizeof(data->key_block_a->session_key_seed));
}


static void eap_fast_derive_key_provisioning(struct eap_sm *sm,
					     struct eap_fast_data *data)
{
	free(data->key_block_p);
	data->key_block_p = (struct eap_fast_key_block_provisioning *)
		eap_fast_derive_key(sm, &data->ssl, "key expansion",
				    sizeof(*data->key_block_p));
	if (data->key_block_p == NULL) {
		wpa_printf(MSG_DEBUG, "EAP-FAST: Failed to derive key block");
		return;
	}
	wpa_hexdump_key(MSG_DEBUG, "EAP-FAST: session_key_seed",
			data->key_block_p->session_key_seed,
			sizeof(data->key_block_p->session_key_seed));
	wpa_hexdump_key(MSG_DEBUG, "EAP-FAST: server_challenge",
			data->key_block_p->server_challenge,
			sizeof(data->key_block_p->server_challenge));
	wpa_hexdump_key(MSG_DEBUG, "EAP-FAST: client_challenge",
			data->key_block_p->client_challenge,
			sizeof(data->key_block_p->client_challenge));
}


static void eap_fast_derive_keys(struct eap_sm *sm, struct eap_fast_data *data)
{
	if (data->current_pac) {
		eap_fast_derive_key_auth(sm, data);
	} else {
		eap_fast_derive_key_provisioning(sm, data);
	}
}


static int eap_fast_phase2_request(struct eap_sm *sm,
				   struct eap_fast_data *data,
				   struct eap_method_ret *ret,
				   struct eap_hdr *req,
				   struct eap_hdr *hdr,
				   u8 **resp, size_t *resp_len)
{
	size_t len = be_to_host16(hdr->length);
	u8 *pos;
	struct eap_method_ret iret;

	if (len <= sizeof(struct eap_hdr)) {
		wpa_printf(MSG_INFO, "EAP-FAST: too short "
			   "Phase 2 request (len=%lu)", (unsigned long) len);
		return -1;
	}
	pos = (u8 *) (hdr + 1);
	wpa_printf(MSG_DEBUG, "EAP-FAST: Phase 2 Request: type=%d", *pos);
	switch (*pos) {
	case EAP_TYPE_IDENTITY:
		*resp = eap_sm_buildIdentity(sm, req->identifier, resp_len, 1);
		break;
	default:
		if (data->phase2_type == EAP_TYPE_NONE) {
			int i;
			for (i = 0; i < data->num_phase2_types; i++) {
				if (data->phase2_types[i] != *pos)
					continue;

				data->phase2_type = *pos;
				wpa_printf(MSG_DEBUG, "EAP-FAST: Selected "
					   "Phase 2 EAP method %d",
					   data->phase2_type);
				break;
			}
		}
		if (*pos != data->phase2_type || *pos == EAP_TYPE_NONE) {
			if (eap_fast_phase2_nak(sm, data, hdr, resp, resp_len))
				return -1;
			return 0;
		}

		if (data->phase2_priv == NULL) {
			data->phase2_method = eap_sm_get_eap_methods(*pos);
			if (data->phase2_method) {
				if (data->key_block_p) {
					sm->auth_challenge =
						data->key_block_p->
						server_challenge;
					sm->peer_challenge =
						data->key_block_p->
						client_challenge;
				}
				sm->init_phase2 = 1;
				data->phase2_priv =
					data->phase2_method->init(sm);
				sm->init_phase2 = 0;
				sm->auth_challenge = NULL;
				sm->peer_challenge = NULL;
			}
		}
		if (data->phase2_priv == NULL || data->phase2_method == NULL) {
			wpa_printf(MSG_INFO, "EAP-FAST: failed to initialize "
				   "Phase 2 EAP method %d", *pos);
			ret->methodState = METHOD_DONE;
			ret->decision = DECISION_FAIL;
			return -1;
		}
		memset(&iret, 0, sizeof(iret));
		*resp = data->phase2_method->process(sm, data->phase2_priv,
						     &iret, (u8 *) hdr, len,
						     resp_len);
		if (*resp == NULL ||
		    (iret.methodState == METHOD_DONE &&
		     iret.decision == DECISION_FAIL)) {
			ret->methodState = METHOD_DONE;
			ret->decision = DECISION_FAIL;
		} else if ((iret.methodState == METHOD_DONE ||
			    iret.methodState == METHOD_MAY_CONT) &&
			   (iret.decision == DECISION_UNCOND_SUCC ||
			    iret.decision == DECISION_COND_SUCC)) {
			data->phase2_success = 1;
		}
		if (*resp == NULL)
			return -1;
		break;
	}
	return 0;
}


static u8 * eap_fast_tlv_nak(int vendor_id, int tlv_type, size_t *len)
{
	struct eap_tlv_nak_tlv *nak;
	*len = sizeof(*nak);
	nak = malloc(*len);
	if (nak == NULL)
		return NULL;
	nak->tlv_type = host_to_be16(EAP_TLV_TYPE_MANDATORY | EAP_TLV_NAK_TLV);
	nak->length = host_to_be16(6);
	nak->vendor_id = host_to_be32(vendor_id);
	nak->nak_type = host_to_be16(tlv_type);
	return (u8 *) nak;
}


static u8 * eap_fast_tlv_result(int status, int intermediate, size_t *len)
{
	struct eap_tlv_intermediate_result_tlv *result;
	*len = sizeof(*result);
	result = malloc(*len);
	if (result == NULL)
		return NULL;
	result->tlv_type = host_to_be16(EAP_TLV_TYPE_MANDATORY |
					(intermediate ?
					 EAP_TLV_INTERMEDIATE_RESULT_TLV :
					 EAP_TLV_RESULT_TLV));
	result->length = host_to_be16(2);
	result->status = host_to_be16(status);
	return (u8 *) result;
}


static u8 * eap_fast_tlv_pac_ack(size_t *len)
{
	struct eap_tlv_result_tlv *res;
	struct eap_tlv_pac_ack_tlv *ack;

	*len = sizeof(*res) + sizeof(*ack);
	res = malloc(*len);
	if (res == NULL)
		return NULL;

	memset(res, 0, *len);
	res->tlv_type = host_to_be16(EAP_TLV_RESULT_TLV |
				     EAP_TLV_TYPE_MANDATORY);
	res->length = host_to_be16(sizeof(*res) - sizeof(struct eap_tlv_hdr));
	res->status = host_to_be16(EAP_TLV_RESULT_SUCCESS);

	ack = (struct eap_tlv_pac_ack_tlv *) (res + 1);
	ack->tlv_type = host_to_be16(EAP_TLV_PAC_TLV |
				     EAP_TLV_TYPE_MANDATORY);
	ack->length = host_to_be16(sizeof(*ack) - sizeof(struct eap_tlv_hdr));
	ack->pac_type = host_to_be16(PAC_TYPE_PAC_ACKNOWLEDGEMENT);
	ack->pac_len = host_to_be16(2);
	ack->result = host_to_be16(EAP_TLV_RESULT_SUCCESS);

	return (u8 *) res;
}


static u8 * eap_fast_tlv_eap_payload(u8 *buf, size_t *len)
{
	struct eap_tlv_hdr *tlv;

	/* Encapsulate EAP packet in EAP Payload TLV */
	tlv = malloc(sizeof(*tlv) + *len);
	if (tlv == NULL) {
		wpa_printf(MSG_DEBUG, "EAP-FAST: Failed to "
			   "allocate memory for TLV "
			   "encapsulation");
		free(buf);
		return NULL;
	}
	tlv->tlv_type = host_to_be16(EAP_TLV_TYPE_MANDATORY |
				     EAP_TLV_EAP_PAYLOAD_TLV);
	tlv->length = host_to_be16(*len);
	memcpy(tlv + 1, buf, *len);
	free(buf);
	*len += sizeof(*tlv);
	return (u8 *) tlv;
}


static u8 * eap_fast_process_crypto_binding(
	struct eap_sm *sm, struct eap_fast_data *data,
	struct eap_method_ret *ret,
	struct eap_tlv_crypto_binding__tlv *bind, size_t bind_len,
	size_t *resp_len, int final)
{
	u8 *resp, *sks = NULL;
	struct eap_tlv_intermediate_result_tlv *rresult;
	struct eap_tlv_crypto_binding__tlv *rbind;
	u8 isk[32], imck[60], *cmk, cmac[20], *key;
	size_t key_len;
	int res;

	wpa_printf(MSG_DEBUG, "EAP-FAST: Crypto-Binding TLV: Version %d "
		   "Received Version %d SubType %d",
		   bind->version, bind->received_version, bind->subtype);
	wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: NONCE",
		    bind->nonce, sizeof(bind->nonce));
	wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: Compound MAC",
		    bind->compound_mac, sizeof(bind->compound_mac));

	if (bind->version != EAP_FAST_VERSION ||
	    bind->received_version != EAP_FAST_VERSION ||
	    bind->subtype != EAP_TLV_CRYPTO_BINDING_SUBTYPE_REQUEST) {
		wpa_printf(MSG_INFO, "EAP-FAST: Invalid version/subtype in "
			   "Crypto-Binding TLV: Version %d "
			   "Received Version %d SubType %d",
			   bind->version, bind->received_version,
			   bind->subtype);
		resp = eap_fast_tlv_result(EAP_TLV_RESULT_FAILURE, 1,
					   resp_len);
		return resp;
	}


	if (data->provisioning) {
		if (data->key_block_p) {
			sks = data->key_block_p->session_key_seed;
		}
	} else {
		if (data->key_block_a) {
			sks = data->key_block_a->session_key_seed;
		}
	}
	if (sks == NULL) {
		wpa_printf(MSG_INFO, "EAP-FAST: No Session Key Seed available "
			   "for processing Crypto-Binding TLV");
		return NULL;
	}

	wpa_printf(MSG_DEBUG, "EAP-FAST: Determining CMK for Compound MIC "
		   "calculation");
	wpa_hexdump_key(MSG_MSGDUMP, "EAP-FAST: S-IMCK[0] = SKS", sks, 40);

	memset(isk, 0, sizeof(isk));
	if (data->phase2_method == NULL || data->phase2_priv == NULL) {
		wpa_printf(MSG_DEBUG, "EAP-FAST: Phase 2 method not "
			   "available");
		return NULL;
	}
	if (data->phase2_method->isKeyAvailable && data->phase2_method->getKey)
	{
		if (!data->phase2_method->isKeyAvailable(sm, data->phase2_priv)
		    ||
		    (key = data->phase2_method->getKey(sm, data->phase2_priv,
						       &key_len)) == NULL) {
			wpa_printf(MSG_DEBUG, "EAP-FAST: Could not get key "
				   "material from Phase 2");
			return NULL;
		}
		if (key_len > sizeof(isk))
			key_len = sizeof(isk);
		/* FIX: which end is being padded? */
#if 0
		memcpy(isk + (sizeof(isk) - key_len), key, key_len);
#else
		memcpy(isk, key, key_len);
#endif
		free(key);
	}
	wpa_hexdump_key(MSG_MSGDUMP, "EAP-FAST: ISK[0]", isk, sizeof(isk));
	sha1_t_prf(sks, 40, "Inner Methods Compound Keys",
		   isk, sizeof(isk), imck, sizeof(imck));
	/* S-IMCK[1] = imkc[0..39] */
	wpa_hexdump_key(MSG_MSGDUMP, "EAP-FAST: S-IMCK[1]", imck, 40);
	cmk = imck + 40;
	wpa_hexdump_key(MSG_MSGDUMP, "EAP-FAST: CMK", cmk, 20);

	memcpy(cmac, bind->compound_mac, sizeof(cmac));
	memset(bind->compound_mac, 0, sizeof(cmac));
	wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: Crypto-Binding TLV for Compound "
		    "MAC calculation", (u8 *) bind, bind_len);
	hmac_sha1(cmk, 20, (u8 *) bind, bind_len, bind->compound_mac);
	res = memcmp(cmac, bind->compound_mac, sizeof(cmac));
	wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: Received Compound MAC",
		    cmac, sizeof(cmac));
	wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: Calculated Compound MAC",
		    bind->compound_mac, sizeof(cmac));
	if (res != 0) {
		wpa_printf(MSG_INFO, "EAP-FAST: Compound MAC did not match");
		resp = eap_fast_tlv_result(EAP_TLV_RESULT_FAILURE, 1,
					   resp_len);
		memcpy(bind->compound_mac, cmac, sizeof(cmac));
		return resp;
	}

	*resp_len = sizeof(*rresult) + sizeof(*rbind);
	resp = malloc(*resp_len);
	if (resp == NULL)
		return NULL;
	memset(resp, 0, *resp_len);

	/* Both intermediate and final Result TLVs are identical, so ok to use
	 * the same structure definition for them. */
	rresult = (struct eap_tlv_intermediate_result_tlv *) resp;
	rresult->tlv_type = host_to_be16(EAP_TLV_TYPE_MANDATORY |
					 (final ? EAP_TLV_RESULT_TLV :
					  EAP_TLV_INTERMEDIATE_RESULT_TLV));
	rresult->length = host_to_be16(2);
	rresult->status = host_to_be16(EAP_TLV_RESULT_SUCCESS);

	if (!data->provisioning && data->phase2_success &&
	    eap_fast_derive_msk(sm, data) < 0) {
		wpa_printf(MSG_INFO, "EAP-FAST: Failed to generate MSK");
		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_FAIL;
		rresult->status = host_to_be16(EAP_TLV_RESULT_FAILURE);
		data->phase2_success = 0;
	}

	rbind = (struct eap_tlv_crypto_binding__tlv *) (rresult + 1);
	rbind->tlv_type = host_to_be16(EAP_TLV_TYPE_MANDATORY |
				       EAP_TLV_CRYPTO_BINDING_TLV_);
	rbind->length = host_to_be16(sizeof(*rbind) -
				     sizeof(struct eap_tlv_hdr));
	rbind->version = EAP_FAST_VERSION;
	rbind->received_version = bind->version;
	rbind->subtype = EAP_TLV_CRYPTO_BINDING_SUBTYPE_RESPONSE;
	memcpy(rbind->nonce, bind->nonce, sizeof(bind->nonce));
	inc_byte_array(rbind->nonce, sizeof(bind->nonce));
	hmac_sha1(cmk, 20, (u8 *) rbind, sizeof(*rbind), rbind->compound_mac);

	wpa_printf(MSG_DEBUG, "EAP-FAST: Reply Crypto-Binding TLV: Version %d "
		   "Received Version %d SubType %d",
		   rbind->version, rbind->received_version, rbind->subtype);
	wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: NONCE",
		    rbind->nonce, sizeof(rbind->nonce));
	wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: Compound MAC",
		    rbind->compound_mac, sizeof(rbind->compound_mac));

	if (final && data->phase2_success) {
		wpa_printf(MSG_DEBUG, "EAP-FAST: Authentication completed "
			   "successfully.");
		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_UNCOND_SUCC;
	}

	return resp;
}


static u8 * eap_fast_process_pac(struct eap_sm *sm, struct eap_fast_data *data,
				 struct eap_method_ret *ret,
				 u8 *pac, size_t pac_len, size_t *resp_len)
{
	struct wpa_ssid *config = eap_get_config(sm);
	struct pac_tlv_hdr *hdr;
	u8 *pos;
	size_t left, len;
	int type, pac_key_found = 0;
	struct eap_fast_pac entry;

	memset(&entry, 0, sizeof(entry));
	pos = pac;
	left = pac_len;
	while (left > sizeof(*hdr)) {
		hdr = (struct pac_tlv_hdr *) pos;
		type = be_to_host16(hdr->type);
		len = be_to_host16(hdr->len);
		pos += sizeof(*hdr);
		left -= sizeof(*hdr);
		if (len > left) {
			wpa_printf(MSG_DEBUG, "EAP-FAST: PAC TLV overrun "
				   "(type=%d len=%lu left=%lu)",
				   type, (unsigned long) len,
				   (unsigned long) left);
			return eap_fast_tlv_result(EAP_TLV_RESULT_FAILURE, 0,
						   resp_len);
		}
		switch (type) {
		case PAC_TYPE_PAC_KEY:
			wpa_hexdump_key(MSG_DEBUG, "EAP-FAST: PAC-Key",
					pos, len);
			if (len != EAP_FAST_PAC_KEY_LEN) {
				wpa_printf(MSG_DEBUG, "EAP-FAST: Invalid "
					   "PAC-Key length %lu",
					   (unsigned long) len);
				break;
			}
			pac_key_found = 1;
			memcpy(entry.pac_key, pos, len);
			break;
		case PAC_TYPE_PAC_OPAQUE:
			wpa_hexdump(MSG_DEBUG, "EAP-FAST: PAC-Opaque",
					pos, len);
			entry.pac_opaque = pos;
			entry.pac_opaque_len = len;
			break;
		case PAC_TYPE_PAC_INFO:
			wpa_hexdump(MSG_DEBUG, "EAP-FAST: PAC-Info",
				    pos, len);
			entry.pac_info = pos;
			entry.pac_info_len = len;
			break;
		default:
			wpa_printf(MSG_DEBUG, "EAP-FAST: Ignored unknown PAC "
				   "type %d", type);
			break;
		}

		pos += len;
		left -= len;
	}

	if (!pac_key_found || !entry.pac_opaque || !entry.pac_info) {
		wpa_printf(MSG_DEBUG, "EAP-FAST: PAC TLV does not include "
			   "all the required fields");
		return eap_fast_tlv_result(EAP_TLV_RESULT_FAILURE, 0,
					   resp_len);
	}

	pos = entry.pac_info;
	left = entry.pac_info_len;
	while (left > sizeof(*hdr)) {
		hdr = (struct pac_tlv_hdr *) pos;
		type = be_to_host16(hdr->type);
		len = be_to_host16(hdr->len);
		pos += sizeof(*hdr);
		left -= sizeof(*hdr);
		if (len > left) {
			wpa_printf(MSG_DEBUG, "EAP-FAST: PAC-Info overrun "
				   "(type=%d len=%lu left=%lu)",
				   type, (unsigned long) len,
				   (unsigned long) left);
			return eap_fast_tlv_result(EAP_TLV_RESULT_FAILURE, 0,
						   resp_len);
		}
		switch (type) {
		case PAC_TYPE_A_ID:
			wpa_hexdump_ascii(MSG_DEBUG, "EAP-FAST: PAC-Info - "
					  "A-ID", pos, len);
			entry.a_id = pos;
			entry.a_id_len = len;
			break;
		case PAC_TYPE_I_ID:
			wpa_hexdump_ascii(MSG_DEBUG, "EAP-FAST: PAC-Info - "
					  "I-ID", pos, len);
			entry.i_id = pos;
			entry.i_id_len = len;
			break;
		case PAC_TYPE_A_ID_INFO:
			wpa_hexdump_ascii(MSG_DEBUG, "EAP-FAST: PAC-Info - "
					  "A-ID-Info", pos, len);
			entry.a_id_info = pos;
			entry.a_id_info_len = len;
			break;
		default:
			wpa_printf(MSG_DEBUG, "EAP-FAST: Ignored unknown "
				   "PAC-Info type %d", type);
			break;
		}

		pos += len;
		left -= len;
	}

	if (entry.a_id == NULL || entry.a_id_info == NULL) {
		wpa_printf(MSG_DEBUG, "EAP-FAST: PAC-Info does not include "
			   "all the required fields");
		return eap_fast_tlv_result(EAP_TLV_RESULT_FAILURE, 0,
					   resp_len);
	}

	eap_fast_add_pac(data, &entry);
	eap_fast_save_pac(data, config->pac_file);

	if (data->provisioning) {
		/* EAP-FAST provisioning does not provide keying material and
		 * must end with an EAP-Failure. Authentication will be done
		 * separately after this. */
		data->success = 0;
		ret->decision = DECISION_FAIL;
		wpa_printf(MSG_DEBUG, "EAP-FAST: Send PAC-Acknowledgement TLV "
			   "- Provisioning completed successfully");
	} else {
		/* This is PAC refreshing, i.e., normal authentication that is
		 * expected to be completed with an EAP-Success. */
		wpa_printf(MSG_DEBUG, "EAP-FAST: Send PAC-Acknowledgement TLV "
			   "- PAC refreshing completed successfully");
		ret->decision = DECISION_UNCOND_SUCC;
	}
	ret->methodState = METHOD_DONE;
	return eap_fast_tlv_pac_ack(resp_len);
}


static int eap_fast_decrypt(struct eap_sm *sm, struct eap_fast_data *data,
			    struct eap_method_ret *ret, struct eap_hdr *req,
			    u8 *in_data, size_t in_len,
			    u8 **out_data, size_t *out_len)
{
	u8 *in_decrypted, *pos, *end;
	int buf_len, len_decrypted, len, res;
	struct eap_hdr *hdr;
	u8 *resp = NULL;
	size_t resp_len;
	int mandatory, tlv_type;
	u8 *eap_payload_tlv = NULL, *pac = NULL;
	size_t eap_payload_tlv_len = 0, pac_len = 0;
	int iresult = 0, result = 0;
	struct eap_tlv_crypto_binding__tlv *crypto_binding = NULL;
	size_t crypto_binding_len = 0;

	wpa_printf(MSG_DEBUG, "EAP-FAST: received %lu bytes encrypted data for"
		   " Phase 2", (unsigned long) in_len);

	res = eap_tls_data_reassemble(sm, &data->ssl, &in_data, &in_len);
	if (res < 0 || res == 1)
		return res;

	buf_len = in_len;
	if (data->ssl.tls_in_total > buf_len)
		buf_len = data->ssl.tls_in_total;
	in_decrypted = malloc(buf_len);
	if (in_decrypted == NULL) {
		free(data->ssl.tls_in);
		data->ssl.tls_in = NULL;
		data->ssl.tls_in_len = 0;
		wpa_printf(MSG_WARNING, "EAP-FAST: failed to allocate memory "
			   "for decryption");
		return -1;
	}

	len_decrypted = tls_connection_decrypt(sm->ssl_ctx, data->ssl.conn,
					       in_data, in_len,
					       in_decrypted, buf_len);
	free(data->ssl.tls_in);
	data->ssl.tls_in = NULL;
	data->ssl.tls_in_len = 0;
	if (len_decrypted < 0) {
		wpa_printf(MSG_INFO, "EAP-FAST: Failed to decrypt Phase 2 "
			   "data");
		free(in_decrypted);
		return -1;
	}

	wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: Decrypted Phase 2 TLV(s)",
		    in_decrypted, len_decrypted);

	if (len_decrypted < 4) {
		free(in_decrypted);
		wpa_printf(MSG_INFO, "EAP-FAST: Too short Phase 2 "
			   "TLV frame (len=%d)", len_decrypted);
		return -1;
	}

	pos = in_decrypted;
	end = in_decrypted + len_decrypted;
	while (pos < end) {
		mandatory = pos[0] & 0x80;
		tlv_type = ((pos[0] & 0x3f) << 8) | pos[1];
		len = (pos[2] << 8) | pos[3];
		pos += 4;
		if (pos + len > end) {
			free(in_decrypted);
			wpa_printf(MSG_INFO, "EAP-FAST: TLV overflow");
			return 0;
		}
		wpa_printf(MSG_DEBUG, "EAP-FAST: received Phase 2: "
			   "TLV type %d length %d%s",
			   tlv_type, len, mandatory ? " (mandatory)" : "");

		switch (tlv_type) {
		case EAP_TLV_EAP_PAYLOAD_TLV:
			wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: EAP Payload TLV",
				    pos, len);
			eap_payload_tlv = pos;
			eap_payload_tlv_len = len;
			break;
		case EAP_TLV_RESULT_TLV:
			wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: Result TLV",
				    pos, len);
			if (len < 2) {
				wpa_printf(MSG_DEBUG, "EAP-FAST: Too short "
					   "Result TLV");
				result = EAP_TLV_RESULT_FAILURE;
				break;
			}
			result = (pos[0] << 16) | pos[1];
			if (result != EAP_TLV_RESULT_SUCCESS &&
			    result != EAP_TLV_RESULT_FAILURE) {
				wpa_printf(MSG_DEBUG, "EAP-FAST: Unknown "
					   "Result %d", result);
				result = EAP_TLV_RESULT_FAILURE;
			}
			wpa_printf(MSG_DEBUG, "EAP-FAST: Result: %s",
				   result == EAP_TLV_RESULT_SUCCESS ?
				   "Success" : "Failure");
			break;
		case EAP_TLV_INTERMEDIATE_RESULT_TLV:
			wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: Intermediate "
				    "Result TLV", pos, len);
			if (len < 2) {
				wpa_printf(MSG_DEBUG, "EAP-FAST: Too short "
					   "Intermediate Result TLV");
				iresult = EAP_TLV_RESULT_FAILURE;
				break;
			}
			iresult = (pos[0] << 16) | pos[1];
			if (iresult != EAP_TLV_RESULT_SUCCESS &&
			    iresult != EAP_TLV_RESULT_FAILURE) {
				wpa_printf(MSG_DEBUG, "EAP-FAST: Unknown "
					   "Intermediate Result %d", iresult);
				iresult = EAP_TLV_RESULT_FAILURE;
			}
			wpa_printf(MSG_DEBUG,
				   "EAP-FAST: Intermediate Result: %s",
				   iresult == EAP_TLV_RESULT_SUCCESS ?
				   "Success" : "Failure");
			break;
		case EAP_TLV_CRYPTO_BINDING_TLV_:
			wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: Crypto-Binding "
				    "TLV", pos, len);
			crypto_binding_len = sizeof(struct eap_tlv_hdr) + len;
			if (crypto_binding_len < sizeof(*crypto_binding)) {
				wpa_printf(MSG_DEBUG, "EAP-FAST: Too short "
					   "Crypto-Binding TLV");
				iresult = EAP_TLV_RESULT_FAILURE;
				pos = end;
				break;
			}
			crypto_binding =
				(struct eap_tlv_crypto_binding__tlv *)
				(pos - sizeof(struct eap_tlv_hdr));
			break;
		case EAP_TLV_PAC_TLV:
			wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: PAC TLV",
				    pos, len);
			pac = pos;
			pac_len = len;
			break;
		default:
			if (mandatory) {
				wpa_printf(MSG_DEBUG, "EAP-FAST: Nak unknown "
					   "mandatory TLV type %d", tlv_type);
				resp = eap_fast_tlv_nak(0, tlv_type,
							&resp_len);
				pos = end;
			} else {
				wpa_printf(MSG_DEBUG, "EAP-FAST: ignored "
					   "unknown optional TLV type %d",
					   tlv_type);
			}
			break;
		}

		pos += len;
	}

	if (!resp && result == EAP_TLV_RESULT_FAILURE) {
		resp = eap_fast_tlv_result(EAP_TLV_RESULT_FAILURE, 0,
					   &resp_len);
		if (!resp) {
			free(in_decrypted);
			return 0;
		}
	}

	if (!resp && iresult == EAP_TLV_RESULT_FAILURE) {
		resp = eap_fast_tlv_result(EAP_TLV_RESULT_FAILURE, 1,
					   &resp_len);
		if (!resp) {
			free(in_decrypted);
			return 0;
		}
	}

	if (!resp && eap_payload_tlv) {
		if (eap_payload_tlv_len < sizeof(*hdr)) {
			wpa_printf(MSG_DEBUG, "EAP-FAST: too short EAP "
				   "Payload TLV (len=%lu)",
				   (unsigned long) eap_payload_tlv_len);
			free(in_decrypted);
			return 0;
		}
		hdr = (struct eap_hdr *) eap_payload_tlv;
		if (be_to_host16(hdr->length) > eap_payload_tlv_len) {
			wpa_printf(MSG_DEBUG, "EAP-FAST: EAP packet overflow "
				   "in EAP Payload TLV");
		}
		if (hdr->code == EAP_CODE_REQUEST) {
			if (eap_fast_phase2_request(sm, data, ret, req, hdr,
						    &resp, &resp_len)) {
				free(in_decrypted);
				wpa_printf(MSG_INFO, "EAP-FAST: Phase2 "
					   "Request processing failed");
				return 0;
			}
			resp = eap_fast_tlv_eap_payload(resp, &resp_len);
			if (resp == NULL) {
				free(in_decrypted);
				return 0;
			}
		} else {
			wpa_printf(MSG_INFO, "EAP-FAST: Unexpected code=%d in "
				   "Phase 2 EAP header", hdr->code);
			free(in_decrypted);
			return 0;
		}
	}

	if (!resp && crypto_binding) {
		int final = result == EAP_TLV_RESULT_SUCCESS;
		resp = eap_fast_process_crypto_binding(sm, data, ret,
						       crypto_binding,
						       crypto_binding_len,
						       &resp_len, final);
		if (!resp) {
			free(in_decrypted);
			return 0;
		}
	}

	if (!resp && pac && result != EAP_TLV_RESULT_SUCCESS) {
		wpa_printf(MSG_DEBUG, "EAP-FAST: PAC TLV without Result TLV "
			   "acnowledging success");
		resp = eap_fast_tlv_result(EAP_TLV_RESULT_FAILURE, 0,
					   &resp_len);
		if (!resp) {
			free(in_decrypted);
			return 0;
		}
	}

	if (!resp && pac && result == EAP_TLV_RESULT_SUCCESS) {
		resp = eap_fast_process_pac(sm, data, ret, pac, pac_len,
					    &resp_len);
		if (!resp) {
			free(in_decrypted);
			return 0;
		}
	}

	free(in_decrypted);

	if (resp == NULL) {
		wpa_printf(MSG_DEBUG, "EAP-FAST: No recognized TLVs - send "
			   "empty response packet");
		resp = malloc(0);
		if (resp == NULL)
			return 0;
		resp_len = 0;
	}

	wpa_hexdump(MSG_DEBUG, "EAP-FAST: Encrypting Phase 2 data",
		    resp, resp_len);
	if (eap_fast_encrypt(sm, data, req->identifier, resp, resp_len,
			     out_data, out_len)) {
		wpa_printf(MSG_INFO, "EAP-FAST: Failed to encrypt a Phase 2 "
			   "frame");
	}
	free(resp);

	return 0;
}


static u8 * eap_fast_process(struct eap_sm *sm, void *priv,
			     struct eap_method_ret *ret,
			     u8 *reqData, size_t reqDataLen,
			     size_t *respDataLen)
{
	struct eap_hdr *req;
	int left, res;
	unsigned int tls_msg_len;
	u8 flags, *pos, *resp, id;
	struct eap_fast_data *data = priv;

	if (tls_get_errors(sm->ssl_ctx)) {
		wpa_printf(MSG_INFO, "EAP-FAST: TLS errors detected");
		ret->ignore = TRUE;
		return NULL;
	}

	req = (struct eap_hdr *) reqData;
	pos = (u8 *) (req + 1);
	if (reqDataLen < sizeof(*req) + 2 || *pos != EAP_TYPE_FAST ||
	    (left = host_to_be16(req->length)) > reqDataLen) {
		wpa_printf(MSG_INFO, "EAP-FAST: Invalid frame");
		ret->ignore = TRUE;
		return NULL;
	}
	left -= sizeof(struct eap_hdr);
	id = req->identifier;
	pos++;
	flags = *pos++;
	left -= 2;
	wpa_printf(MSG_DEBUG, "EAP-FAST: Received packet(len=%lu) - "
		   "Flags 0x%02x", (unsigned long) reqDataLen, flags);
	if (flags & EAP_TLS_FLAGS_LENGTH_INCLUDED) {
		if (left < 4) {
			wpa_printf(MSG_INFO, "EAP-FAST: Short frame with TLS "
				   "length");
			ret->ignore = TRUE;
			return NULL;
		}
		tls_msg_len = (pos[0] << 24) | (pos[1] << 16) | (pos[2] << 8) |
			pos[3];
		wpa_printf(MSG_DEBUG, "EAP-FAST: TLS Message Length: %d",
			   tls_msg_len);
		if (data->ssl.tls_in_left == 0) {
			data->ssl.tls_in_total = tls_msg_len;
			data->ssl.tls_in_left = tls_msg_len;
			free(data->ssl.tls_in);
			data->ssl.tls_in = NULL;
			data->ssl.tls_in_len = 0;
		}
		pos += 4;
		left -= 4;
	}

	ret->ignore = FALSE;
	ret->methodState = METHOD_CONT;
	ret->decision = DECISION_FAIL;
	ret->allowNotifications = TRUE;

	if (flags & EAP_TLS_FLAGS_START) {
		u8 *a_id;
		size_t a_id_len;
		struct pac_tlv_hdr *hdr;

		wpa_printf(MSG_DEBUG, "EAP-FAST: Start (server ver=%d, own "
			   "ver=%d)", flags & EAP_PEAP_VERSION_MASK,
			data->fast_version);
		if ((flags & EAP_PEAP_VERSION_MASK) < data->fast_version)
			data->fast_version = flags & EAP_PEAP_VERSION_MASK;
		wpa_printf(MSG_DEBUG, "EAP-FAST: Using FAST version %d",
			   data->fast_version);

		a_id = pos;
		a_id_len = left;
		if (left > sizeof(*hdr)) {
			int tlen;
			hdr = (struct pac_tlv_hdr *) pos;
			tlen = be_to_host16(hdr->len);
			if (be_to_host16(hdr->type) == PAC_TYPE_A_ID &&
			    sizeof(*hdr) + tlen <= left) {
				a_id = (u8 *) (hdr + 1);
				a_id_len = tlen;
			}
		}
		wpa_hexdump_ascii(MSG_DEBUG, "EAP-FAST: A-ID", a_id, a_id_len);

		data->current_pac = eap_fast_get_pac(data, a_id, a_id_len);
		if (data->current_pac) {
			wpa_printf(MSG_DEBUG, "EAP-FAST: PAC found for this "
				   "A-ID");
			wpa_hexdump_ascii(MSG_MSGDUMP, "EAP-FAST: A-ID-Info",
					  data->current_pac->a_id_info,
					  data->current_pac->a_id_info_len);
		}

		if (data->resuming && data->current_pac) {
			wpa_printf(MSG_DEBUG, "EAP-FAST: Trying to resume "
				   "session - do not add PAC-Opaque to TLS "
				   "ClientHello");
			if (tls_connection_client_hello_ext(
				    sm->ssl_ctx, data->ssl.conn,
				    TLS_EXT_PAC_OPAQUE, NULL, 0) < 0) {
				wpa_printf(MSG_DEBUG, "EAP-FAST: Failed to "
					   "remove PAC-Opaque TLS extension");
				return NULL;
			}

		} else if (data->current_pac) {
			u8 *tlv;
			size_t tlv_len, olen;
			struct eap_tlv_hdr *hdr;
			olen = data->current_pac->pac_opaque_len;
			tlv_len = sizeof(*hdr) + olen;
			tlv = malloc(tlv_len);
			if (tlv) {
				hdr = (struct eap_tlv_hdr *) tlv;
				hdr->tlv_type =
					host_to_be16(PAC_TYPE_PAC_OPAQUE);
				hdr->length = host_to_be16(olen);
				memcpy(hdr + 1, data->current_pac->pac_opaque,
				       olen);
			}
			if (tlv == NULL ||
			    tls_connection_client_hello_ext(
				    sm->ssl_ctx, data->ssl.conn,
				    TLS_EXT_PAC_OPAQUE, tlv, tlv_len) < 0) {
				wpa_printf(MSG_DEBUG, "EAP-FAST: Failed to "
					   "add PAC-Opaque TLS extension");
				free(tlv);
				return NULL;
			}
			free(tlv);
		} else {
			if (!data->provisioning_allowed) {
				wpa_printf(MSG_DEBUG, "EAP-FAST: No PAC found "
					   "and provisioning disabled");
				return NULL;
			}
			wpa_printf(MSG_DEBUG, "EAP-FAST: No PAC found - "
				   "starting provisioning");
			if (tls_connection_set_anon_dh(sm->ssl_ctx,
						       data->ssl.conn)) {
				wpa_printf(MSG_INFO, "EAP-FAST: Could not "
					   "configure anonymous DH for TLS "
					   "connection");
				return NULL;
			}
			if (tls_connection_client_hello_ext(
				    sm->ssl_ctx, data->ssl.conn,
				    TLS_EXT_PAC_OPAQUE, NULL, 0) < 0) {
				wpa_printf(MSG_DEBUG, "EAP-FAST: Failed to "
					   "remove PAC-Opaque TLS extension");
				return NULL;
			}
			data->provisioning = 1;
		}

		left = 0; /* A-ID is not used in further packet processing */
	}

	resp = NULL;
	if (tls_connection_established(sm->ssl_ctx, data->ssl.conn) &&
	    !data->resuming) {
		res = eap_fast_decrypt(sm, data, ret, req, pos, left,
				       &resp, respDataLen);
		if (res < 0) {
			ret->methodState = METHOD_DONE;
			ret->decision = DECISION_FAIL;
			/* Ack possible Alert that may have caused failure in
			 * decryption */
			res = 1;
		}
	} else {
		if (eap_fast_set_tls_master_secret(sm, data, pos, left) < 0) {
			wpa_printf(MSG_DEBUG, "EAP-FAST: Failed to configure "
				   "TLS master secret");
			ret->methodState = METHOD_DONE;
			ret->decision = DECISION_FAIL;
			return NULL;
		}

		res = eap_tls_process_helper(sm, &data->ssl, EAP_TYPE_FAST,
					     data->fast_version, id, pos, left,
					     &resp, respDataLen);

		if (tls_connection_established(sm->ssl_ctx, data->ssl.conn)) {
			wpa_printf(MSG_DEBUG,
				   "EAP-FAST: TLS done, proceed to Phase 2");
			data->resuming = 0;
			eap_fast_derive_keys(sm, data);
		}
	}

	if (res == 1)
		return eap_tls_build_ack(&data->ssl, respDataLen, id,
					 EAP_TYPE_FAST, data->fast_version);
	return resp;
}


#if 0 /* FIX */
static Boolean eap_fast_has_reauth_data(struct eap_sm *sm, void *priv)
{
	struct eap_fast_data *data = priv;
	return tls_connection_established(sm->ssl_ctx, data->ssl.conn);
}


static void eap_fast_deinit_for_reauth(struct eap_sm *sm, void *priv)
{
}


static void * eap_fast_init_for_reauth(struct eap_sm *sm, void *priv)
{
	struct eap_fast_data *data = priv;
	if (eap_tls_reauth_init(sm, &data->ssl)) {
		free(data);
		return NULL;
	}
	data->phase2_success = 0;
	data->resuming = 1;
	data->provisioning = 0;
	return priv;
}
#endif


static int eap_fast_get_status(struct eap_sm *sm, void *priv, char *buf,
			       size_t buflen, int verbose)
{
	struct eap_fast_data *data = priv;
	return eap_tls_status(sm, &data->ssl, buf, buflen, verbose);
}


static Boolean eap_fast_isKeyAvailable(struct eap_sm *sm, void *priv)
{
	struct eap_fast_data *data = priv;
	return data->success;
}


static u8 * eap_fast_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_fast_data *data = priv;
	u8 *key;

	if (!data->success)
		return NULL;

	key = malloc(EAP_FAST_KEY_LEN);
	if (key == NULL)
		return NULL;

	*len = EAP_FAST_KEY_LEN;
	memcpy(key, data->key_data, EAP_FAST_KEY_LEN);

	return key;
}


const struct eap_method eap_method_fast =
{
	.method = EAP_TYPE_FAST,
	.name = "FAST",
	.init = eap_fast_init,
	.deinit = eap_fast_deinit,
	.process = eap_fast_process,
	.isKeyAvailable = eap_fast_isKeyAvailable,
	.getKey = eap_fast_getKey,
	.get_status = eap_fast_get_status,
#if 0
	.has_reauth_data = eap_fast_has_reauth_data,
	.deinit_for_reauth = eap_fast_deinit_for_reauth,
	.init_for_reauth = eap_fast_init_for_reauth,
#endif
};
