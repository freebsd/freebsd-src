/*
 * hostapd / EAP-SIM database/authenticator gateway
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

/* This is an example implementation of the EAP-SIM database/authentication
 * gateway interface that is expected to be replaced with an implementation of
 * SS7 gateway to GSM authentication center (HLR/AuC) or a local
 * implementation of SIM triplet generator.
 *
 * The example implementation here reads triplets from a text file in
 * IMSI:Kc:SRES:RAND format, IMSI in ASCII, other fields as hex strings. This
 * is used to simulate an HLR/AuC. As such, it is not very useful for real life
 * authentication, but it is useful both as an example implementation and for
 * EAP-SIM testing.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "eap_sim_common.h"
#include "eap_sim_db.h"


/* TODO: add an alternative callback based version of the interface. This is
 * needed to work better with the single threaded design of hostapd. For this,
 * the EAP data has to be stored somewhere and eap_sim_db is given a context
 * pointer for this and a callback function. The callback function will re-send
 * the EAP data through normal operations which will eventually end up calling
 * eap_sim_db_get_gsm_triplets() again for the same user. This time, eap_sim_db
 * should have the triplets available immediately. */


struct eap_sim_db_data {
	char *fname;
};

#define KC_LEN 8
#define SRES_LEN 4
#define RAND_LEN 16


/* Initialize EAP-SIM database/authentication gateway interface.
 * Returns pointer to a private data structure. */
void * eap_sim_db_init(const char *config)
{
	struct eap_sim_db_data *data;

	data = malloc(sizeof(*data));
	if (data == NULL) {
		return NULL;
	}

	memset(data, 0, sizeof(*data));
	data->fname = strdup(config);
	if (data->fname == NULL) {
		free(data);
		return NULL;
	}

	return data;
}

/* Deinitialize EAP-SIM database/authentication gateway interface.
 * priv is the pointer from eap_sim_db_init(). */
void eap_sim_db_deinit(void *priv)
{
	struct eap_sim_db_data *data = priv;
	free(data->fname);
	free(data);
}


/* Get GSM triplets for user name identity (identity_len bytes). In most cases,
 * the user name is '1' | IMSI, i.e., 1 followed by the IMSI in ASCII format.
 * The identity may also include NAI realm (@realm).
 * priv is the pointer from eap_sim_db_init().
 * Returns the number of triplets received (has to be less than or equal to
 * max_chal) or -1 on error (e.g., user not found). rand, kc, and sres are
 * pointers to data areas for the triplets. */
int eap_sim_db_get_gsm_triplets(void *priv, const u8 *identity,
				size_t identity_len, int max_chal,
				u8 *rand, u8 *kc, u8 *sres)
{
	struct eap_sim_db_data *data = priv;
	FILE *f;
	int count, i;
	char buf[80], *pos, *next;

	f = fopen(data->fname, "r");
	if (f == NULL) {
		wpa_printf(MSG_DEBUG, "EAP-SIM DB: could not open triplet "
			   "file '%s'", data->fname);
		return -1;
	}

	if (identity_len < 2 || identity[0] != '1') {
		wpa_hexdump_ascii(MSG_DEBUG, "EAP-SIM DB: unexpected identity",
				  identity, identity_len);
		fclose(f);
		return -1;
	}
	identity++;
	identity_len--;
	for (i = 0; i < identity_len; i++) {
		if (identity[i] == '@') {
			identity_len = i;
			break;
		}
	}
	wpa_hexdump_ascii(MSG_DEBUG, "EAP-SIM DB: get triplets for IMSI",
			  identity, identity_len);

	count = 0;
	while (count < max_chal && fgets(buf, sizeof(buf), f)) {
		/* Parse IMSI:Kc:SRES:RAND and match IMSI with identity. */
		buf[sizeof(buf) - 1] = '\0';
		pos = buf;
		while (*pos != '\0' && *pos != '\n')
			pos++;
		if (*pos == '\n')
			*pos = '\0';
		if (pos - buf < 60 || pos[0] == '#')
			continue;

		pos = strchr(buf, ':');
		if (pos == NULL)
			continue;
		*pos++ = '\0';
		if (strlen(buf) != identity_len ||
		    memcmp(buf, identity, identity_len) != 0)
			continue;

		next = strchr(pos, ':');
		if (next == NULL)
			continue;
		*next++ = '\0';
		if (hexstr2bin(pos, &kc[count * KC_LEN], KC_LEN) < 0)
			continue;

		pos = next;
		next = strchr(pos, ':');
		if (next == NULL)
			continue;
		*next++ = '\0';
		if (hexstr2bin(pos, &sres[count * SRES_LEN], SRES_LEN) < 0)
			continue;

		if (hexstr2bin(next, &rand[count * RAND_LEN], RAND_LEN) < 0)
			continue;

		count++;
	}

	fclose(f);

	if (count == 0) {
		wpa_printf(MSG_DEBUG, "EAP-SIM DB: no triplets found");
		count = -1;
	}

	return count;
}


/* Verify whether the given user identity (identity_len bytes) is known. In
 * most cases, the user name is '1' | IMSI, i.e., 1 followed by the IMSI in
 * ASCII format.
 * priv is the pointer from eap_sim_db_init().
 * Returns 0 if the user is found and GSM triplets would be available for it or
 * -1 on error (e.g., user not found or no triplets available). */
int eap_sim_db_identity_known(void *priv, const u8 *identity,
			      size_t identity_len)
{
	struct eap_sim_db_data *data = priv;
	FILE *f;
	char buf[80], *pos;
	int i;

	if (identity_len < 1 || identity[0] != '1') {
		return -1;
	}

	f = fopen(data->fname, "r");
	if (f == NULL) {
		wpa_printf(MSG_DEBUG, "EAP-SIM DB: could not open triplet "
			   "file '%s'", data->fname);
		return -1;
	}

	if (identity_len < 2 || identity[0] != '1') {
		wpa_hexdump_ascii(MSG_DEBUG, "EAP-SIM DB: unexpected identity",
				  identity, identity_len);
		return -1;
	}
	identity++;
	identity_len--;
	for (i = 0; i < identity_len; i++) {
		if (identity[i] == '@') {
			identity_len = i;
			break;
		}
	}

	while (fgets(buf, sizeof(buf), f)) {
		/* Parse IMSI:Kc:SRES:RAND and match IMSI with identity. */
		buf[sizeof(buf) - 1] = '\0';
		pos = buf;
		while (*pos != '\0' && *pos != '\n')
			pos++;
		if (*pos == '\n')
			*pos = '\0';
		if (pos - buf < 60 || pos[0] == '#')
			continue;

		pos = strchr(buf, ':');
		if (pos == NULL)
			continue;
		*pos++ = '\0';
		if (strlen(buf) != identity_len ||
		    memcmp(buf, identity, identity_len) != 0)
			continue;

		fclose(f);
		return 0;
	}

	/* IMSI not found */

	fclose(f);
	return -1;
}
