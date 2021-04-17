/*
 * Wired Equivalent Privacy (WEP)
 * Copyright (c) 2010, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/crc32.h"
#include "common/ieee802_11_defs.h"
#include "wlantest.h"


void wep_crypt(u8 *key, u8 *buf, size_t plen)
{
	u32 i, j, k;
	u8 S[256];
#define S_SWAP(a,b) do { u8 t = S[a]; S[a] = S[b]; S[b] = t; } while(0)
	u8 *pos;

	/* Setup RC4 state */
	for (i = 0; i < 256; i++)
		S[i] = i;
	j = 0;
	for (i = 0; i < 256; i++) {
		j = (j + S[i] + key[i & 0x0f]) & 0xff;
		S_SWAP(i, j);
	}

	/* Apply RC4 to data */
	pos = buf;
	i = j = 0;
	for (k = 0; k < plen; k++) {
		i = (i + 1) & 0xff;
		j = (j + S[i]) & 0xff;
		S_SWAP(i, j);
		*pos ^= S[(S[i] + S[j]) & 0xff];
		pos++;
	}
}


static int try_wep(const u8 *key, size_t key_len, const u8 *data,
		   size_t data_len, u8 *plain)
{
	u32 icv, rx_icv;
	u8 k[16];
	int i, j;

	for (i = 0, j = 0; i < sizeof(k); i++) {
		k[i] = key[j];
		j++;
		if (j >= key_len)
			j = 0;
	}

	os_memcpy(plain, data, data_len);
	wep_crypt(k, plain, data_len);
	icv = crc32(plain, data_len - 4);
	rx_icv = WPA_GET_LE32(plain + data_len - 4);
	if (icv != rx_icv)
		return -1;

	return 0;
}


u8 * wep_decrypt(struct wlantest *wt, const struct ieee80211_hdr *hdr,
		 const u8 *data, size_t data_len, size_t *decrypted_len)
{
	u8 *plain;
	struct wlantest_wep *w;
	int found = 0;
	u8 key[16];

	if (dl_list_empty(&wt->wep))
		return NULL;

	if (data_len < 4 + 4)
		return NULL;
	plain = os_malloc(data_len - 4);
	if (plain == NULL)
		return NULL;

	dl_list_for_each(w, &wt->wep, struct wlantest_wep, list) {
		os_memcpy(key, data, 3);
		os_memcpy(key + 3, w->key, w->key_len);
		if (try_wep(key, 3 + w->key_len, data + 4, data_len - 4, plain)
		    == 0) {
			found = 1;
			break;
		}
	}
	if (!found) {
		os_free(plain);
		return NULL;
	}

	*decrypted_len = data_len - 4 - 4;
	return plain;
}
