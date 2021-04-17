/*
 * GCM with GMAC Protocol (GCMP)
 * Copyright (c) 2012, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "crypto/aes.h"
#include "crypto/aes_wrap.h"
#include "wlantest.h"


static void gcmp_aad_nonce(const struct ieee80211_hdr *hdr, const u8 *data,
			   u8 *aad, size_t *aad_len, u8 *nonce)
{
	u16 fc, stype, seq;
	int qos = 0, addr4 = 0;
	u8 *pos;

	fc = le_to_host16(hdr->frame_control);
	stype = WLAN_FC_GET_STYPE(fc);
	if ((fc & (WLAN_FC_TODS | WLAN_FC_FROMDS)) ==
	    (WLAN_FC_TODS | WLAN_FC_FROMDS))
		addr4 = 1;

	if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_DATA) {
		fc &= ~0x0070; /* Mask subtype bits */
		if (stype & 0x08) {
			const u8 *qc;
			qos = 1;
			fc &= ~WLAN_FC_ORDER;
			qc = (const u8 *) (hdr + 1);
			if (addr4)
				qc += ETH_ALEN;
		}
	}

	fc &= ~(WLAN_FC_RETRY | WLAN_FC_PWRMGT | WLAN_FC_MOREDATA);
	WPA_PUT_LE16(aad, fc);
	pos = aad + 2;
	os_memcpy(pos, hdr->addr1, 3 * ETH_ALEN);
	pos += 3 * ETH_ALEN;
	seq = le_to_host16(hdr->seq_ctrl);
	seq &= ~0xfff0; /* Mask Seq#; do not modify Frag# */
	WPA_PUT_LE16(pos, seq);
	pos += 2;

	os_memcpy(pos, hdr + 1, addr4 * ETH_ALEN + qos * 2);
	pos += addr4 * ETH_ALEN;
	if (qos) {
		pos[0] &= ~0x70;
		if (1 /* FIX: either device has SPP A-MSDU Capab = 0 */)
			pos[0] &= ~0x80;
		pos++;
		*pos++ = 0x00;
	}

	*aad_len = pos - aad;

	os_memcpy(nonce, hdr->addr2, ETH_ALEN);
	nonce[6] = data[7]; /* PN5 */
	nonce[7] = data[6]; /* PN4 */
	nonce[8] = data[5]; /* PN3 */
	nonce[9] = data[4]; /* PN2 */
	nonce[10] = data[1]; /* PN1 */
	nonce[11] = data[0]; /* PN0 */
}


u8 * gcmp_decrypt(const u8 *tk, size_t tk_len, const struct ieee80211_hdr *hdr,
		  const u8 *data, size_t data_len, size_t *decrypted_len)
{
	u8 aad[30], nonce[12], *plain;
	size_t aad_len, mlen;
	const u8 *m;

	if (data_len < 8 + 16)
		return NULL;

	plain = os_malloc(data_len + AES_BLOCK_SIZE);
	if (plain == NULL)
		return NULL;

	m = data + 8;
	mlen = data_len - 8 - 16;

	os_memset(aad, 0, sizeof(aad));
	gcmp_aad_nonce(hdr, data, aad, &aad_len, nonce);
	wpa_hexdump(MSG_EXCESSIVE, "GCMP AAD", aad, aad_len);
	wpa_hexdump(MSG_EXCESSIVE, "GCMP nonce", nonce, sizeof(nonce));

	if (aes_gcm_ad(tk, tk_len, nonce, sizeof(nonce), m, mlen, aad, aad_len,
		       m + mlen, plain) < 0) {
		u16 seq_ctrl = le_to_host16(hdr->seq_ctrl);
		wpa_printf(MSG_INFO, "Invalid GCMP frame: A1=" MACSTR
			   " A2=" MACSTR " A3=" MACSTR " seq=%u frag=%u",
			   MAC2STR(hdr->addr1), MAC2STR(hdr->addr2),
			   MAC2STR(hdr->addr3),
			   WLAN_GET_SEQ_SEQ(seq_ctrl),
			   WLAN_GET_SEQ_FRAG(seq_ctrl));
		os_free(plain);
		return NULL;
	}

	*decrypted_len = mlen;
	return plain;
}


u8 * gcmp_encrypt(const u8 *tk, size_t tk_len, const u8 *frame, size_t len,
		  size_t hdrlen, const u8 *qos,
		  const u8 *pn, int keyid, size_t *encrypted_len)
{
	u8 aad[30], nonce[12], *crypt, *pos;
	size_t aad_len, plen;
	struct ieee80211_hdr *hdr;

	if (len < hdrlen || hdrlen < 24)
		return NULL;
	plen = len - hdrlen;

	crypt = os_malloc(hdrlen + 8 + plen + 16 + AES_BLOCK_SIZE);
	if (crypt == NULL)
		return NULL;

	os_memcpy(crypt, frame, hdrlen);
	hdr = (struct ieee80211_hdr *) crypt;
	pos = crypt + hdrlen;
	*pos++ = pn[5]; /* PN0 */
	*pos++ = pn[4]; /* PN1 */
	*pos++ = 0x00; /* Rsvd */
	*pos++ = 0x20 | (keyid << 6);
	*pos++ = pn[3]; /* PN2 */
	*pos++ = pn[2]; /* PN3 */
	*pos++ = pn[1]; /* PN4 */
	*pos++ = pn[0]; /* PN5 */

	os_memset(aad, 0, sizeof(aad));
	gcmp_aad_nonce(hdr, crypt + hdrlen, aad, &aad_len, nonce);
	wpa_hexdump(MSG_EXCESSIVE, "GCMP AAD", aad, aad_len);
	wpa_hexdump(MSG_EXCESSIVE, "GCMP nonce", nonce, sizeof(nonce));

	if (aes_gcm_ae(tk, tk_len, nonce, sizeof(nonce), frame + hdrlen, plen,
		       aad, aad_len, pos, pos + plen) < 0) {
		os_free(crypt);
		return NULL;
	}

	wpa_hexdump(MSG_EXCESSIVE, "GCMP MIC", pos + plen, 16);
	wpa_hexdump(MSG_EXCESSIVE, "GCMP encrypted", pos, plen);

	*encrypted_len = hdrlen + 8 + plen + 16;

	return crypt;
}
