/*
 * DPP functionality shared between hostapd and wpa_supplicant
 * Copyright (c) 2017, Qualcomm Atheros, Inc.
 * Copyright (c) 2018-2020, The Linux Foundation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/base64.h"
#include "utils/json.h"
#include "common/ieee802_11_common.h"
#include "common/wpa_ctrl.h"
#include "common/gas.h"
#include "eap_common/eap_defs.h"
#include "crypto/crypto.h"
#include "crypto/random.h"
#include "crypto/aes.h"
#include "crypto/aes_siv.h"
#include "drivers/driver.h"
#include "dpp.h"
#include "dpp_i.h"


static const char * dpp_netrole_str(enum dpp_netrole netrole);

#ifdef CONFIG_TESTING_OPTIONS
#ifdef CONFIG_DPP3
int dpp_version_override = 3;
#elif defined(CONFIG_DPP2)
int dpp_version_override = 2;
#else
int dpp_version_override = 1;
#endif
enum dpp_test_behavior dpp_test = DPP_TEST_DISABLED;
#endif /* CONFIG_TESTING_OPTIONS */


void dpp_auth_fail(struct dpp_authentication *auth, const char *txt)
{
	wpa_msg(auth->msg_ctx, MSG_INFO, DPP_EVENT_FAIL "%s", txt);
}


struct wpabuf * dpp_alloc_msg(enum dpp_public_action_frame_type type,
			      size_t len)
{
	struct wpabuf *msg;

	msg = wpabuf_alloc(8 + len);
	if (!msg)
		return NULL;
	wpabuf_put_u8(msg, WLAN_ACTION_PUBLIC);
	wpabuf_put_u8(msg, WLAN_PA_VENDOR_SPECIFIC);
	wpabuf_put_be24(msg, OUI_WFA);
	wpabuf_put_u8(msg, DPP_OUI_TYPE);
	wpabuf_put_u8(msg, 1); /* Crypto Suite */
	wpabuf_put_u8(msg, type);
	return msg;
}


const u8 * dpp_get_attr(const u8 *buf, size_t len, u16 req_id, u16 *ret_len)
{
	u16 id, alen;
	const u8 *pos = buf, *end = buf + len;

	while (end - pos >= 4) {
		id = WPA_GET_LE16(pos);
		pos += 2;
		alen = WPA_GET_LE16(pos);
		pos += 2;
		if (alen > end - pos)
			return NULL;
		if (id == req_id) {
			*ret_len = alen;
			return pos;
		}
		pos += alen;
	}

	return NULL;
}


static const u8 * dpp_get_attr_next(const u8 *prev, const u8 *buf, size_t len,
				    u16 req_id, u16 *ret_len)
{
	u16 id, alen;
	const u8 *pos, *end = buf + len;

	if (!prev)
		pos = buf;
	else
		pos = prev + WPA_GET_LE16(prev - 2);
	while (end - pos >= 4) {
		id = WPA_GET_LE16(pos);
		pos += 2;
		alen = WPA_GET_LE16(pos);
		pos += 2;
		if (alen > end - pos)
			return NULL;
		if (id == req_id) {
			*ret_len = alen;
			return pos;
		}
		pos += alen;
	}

	return NULL;
}


int dpp_check_attrs(const u8 *buf, size_t len)
{
	const u8 *pos, *end;
	int wrapped_data = 0;

	pos = buf;
	end = buf + len;
	while (end - pos >= 4) {
		u16 id, alen;

		id = WPA_GET_LE16(pos);
		pos += 2;
		alen = WPA_GET_LE16(pos);
		pos += 2;
		wpa_printf(MSG_MSGDUMP, "DPP: Attribute ID %04x len %u",
			   id, alen);
		if (alen > end - pos) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Truncated message - not enough room for the attribute - dropped");
			return -1;
		}
		if (wrapped_data) {
			wpa_printf(MSG_DEBUG,
				   "DPP: An unexpected attribute included after the Wrapped Data attribute");
			return -1;
		}
		if (id == DPP_ATTR_WRAPPED_DATA)
			wrapped_data = 1;
		pos += alen;
	}

	if (end != pos) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Unexpected octets (%d) after the last attribute",
			   (int) (end - pos));
		return -1;
	}

	return 0;
}


void dpp_bootstrap_info_free(struct dpp_bootstrap_info *info)
{
	if (!info)
		return;
	os_free(info->uri);
	os_free(info->info);
	os_free(info->chan);
	os_free(info->pk);
	crypto_ec_key_deinit(info->pubkey);
	str_clear_free(info->configurator_params);
	os_free(info);
}


const char * dpp_bootstrap_type_txt(enum dpp_bootstrap_type type)
{
	switch (type) {
	case DPP_BOOTSTRAP_QR_CODE:
		return "QRCODE";
	case DPP_BOOTSTRAP_PKEX:
		return "PKEX";
	case DPP_BOOTSTRAP_NFC_URI:
		return "NFC-URI";
	}
	return "??";
}


static int dpp_uri_valid_info(const char *info)
{
	while (*info) {
		unsigned char val = *info++;

		if (val < 0x20 || val > 0x7e || val == 0x3b)
			return 0;
	}

	return 1;
}


static int dpp_clone_uri(struct dpp_bootstrap_info *bi, const char *uri)
{
	bi->uri = os_strdup(uri);
	return bi->uri ? 0 : -1;
}


int dpp_parse_uri_chan_list(struct dpp_bootstrap_info *bi,
			    const char *chan_list)
{
	const char *pos = chan_list, *pos2;
	int opclass = -1, channel, freq;

	while (pos && *pos && *pos != ';') {
		pos2 = pos;
		while (*pos2 >= '0' && *pos2 <= '9')
			pos2++;
		if (*pos2 == '/') {
			opclass = atoi(pos);
			pos = pos2 + 1;
		}
		if (opclass <= 0)
			goto fail;
		channel = atoi(pos);
		if (channel <= 0)
			goto fail;
		while (*pos >= '0' && *pos <= '9')
			pos++;
		freq = ieee80211_chan_to_freq(NULL, opclass, channel);
		wpa_printf(MSG_DEBUG,
			   "DPP: URI channel-list: opclass=%d channel=%d ==> freq=%d",
			   opclass, channel, freq);
		bi->channels_listed = true;
		if (freq < 0) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Ignore unknown URI channel-list channel (opclass=%d channel=%d)",
				   opclass, channel);
		} else if (bi->num_freq == DPP_BOOTSTRAP_MAX_FREQ) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Too many channels in URI channel-list - ignore list");
			bi->num_freq = 0;
			break;
		} else {
			bi->freq[bi->num_freq++] = freq;
		}

		if (*pos == ';' || *pos == '\0')
			break;
		if (*pos != ',')
			goto fail;
		pos++;
	}

	return 0;
fail:
	wpa_printf(MSG_DEBUG, "DPP: Invalid URI channel-list");
	return -1;
}


int dpp_parse_uri_mac(struct dpp_bootstrap_info *bi, const char *mac)
{
	if (!mac)
		return 0;

	if (hwaddr_aton2(mac, bi->mac_addr) < 0) {
		wpa_printf(MSG_DEBUG, "DPP: Invalid URI mac");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "DPP: URI mac: " MACSTR, MAC2STR(bi->mac_addr));

	return 0;
}


int dpp_parse_uri_info(struct dpp_bootstrap_info *bi, const char *info)
{
	const char *end;

	if (!info)
		return 0;

	end = os_strchr(info, ';');
	if (!end)
		end = info + os_strlen(info);
	bi->info = os_malloc(end - info + 1);
	if (!bi->info)
		return -1;
	os_memcpy(bi->info, info, end - info);
	bi->info[end - info] = '\0';
	wpa_printf(MSG_DEBUG, "DPP: URI(information): %s", bi->info);
	if (!dpp_uri_valid_info(bi->info)) {
		wpa_printf(MSG_DEBUG, "DPP: Invalid URI information payload");
		return -1;
	}

	return 0;
}


int dpp_parse_uri_version(struct dpp_bootstrap_info *bi, const char *version)
{
#ifdef CONFIG_DPP2
	if (!version || DPP_VERSION < 2)
		return 0;

	if (*version == '1')
		bi->version = 1;
	else if (*version == '2')
		bi->version = 2;
	else if (*version == '3')
		bi->version = 3;
	else
		wpa_printf(MSG_DEBUG, "DPP: Unknown URI version");

	wpa_printf(MSG_DEBUG, "DPP: URI version: %d", bi->version);
#endif /* CONFIG_DPP2 */

	return 0;
}


static int dpp_parse_uri_pk(struct dpp_bootstrap_info *bi, const char *info)
{
	u8 *data;
	size_t data_len;
	int res;
	const char *end;

	end = os_strchr(info, ';');
	if (!end)
		return -1;

	data = base64_decode(info, end - info, &data_len);
	if (!data) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Invalid base64 encoding on URI public-key");
		return -1;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: Base64 decoded URI public-key",
		    data, data_len);

	res = dpp_get_subject_public_key(bi, data, data_len);
	os_free(data);
	return res;
}


static struct dpp_bootstrap_info * dpp_parse_uri(const char *uri)
{
	const char *pos = uri;
	const char *end;
	const char *chan_list = NULL, *mac = NULL, *info = NULL, *pk = NULL;
	const char *version = NULL;
	struct dpp_bootstrap_info *bi;

	wpa_hexdump_ascii(MSG_DEBUG, "DPP: URI", uri, os_strlen(uri));

	if (os_strncmp(pos, "DPP:", 4) != 0) {
		wpa_printf(MSG_INFO, "DPP: Not a DPP URI");
		return NULL;
	}
	pos += 4;

	for (;;) {
		end = os_strchr(pos, ';');
		if (!end)
			break;

		if (end == pos) {
			/* Handle terminating ";;" and ignore unexpected ";"
			 * for parsing robustness. */
			pos++;
			continue;
		}

		if (pos[0] == 'C' && pos[1] == ':' && !chan_list)
			chan_list = pos + 2;
		else if (pos[0] == 'M' && pos[1] == ':' && !mac)
			mac = pos + 2;
		else if (pos[0] == 'I' && pos[1] == ':' && !info)
			info = pos + 2;
		else if (pos[0] == 'K' && pos[1] == ':' && !pk)
			pk = pos + 2;
		else if (pos[0] == 'V' && pos[1] == ':' && !version)
			version = pos + 2;
		else
			wpa_hexdump_ascii(MSG_DEBUG,
					  "DPP: Ignore unrecognized URI parameter",
					  pos, end - pos);
		pos = end + 1;
	}

	if (!pk) {
		wpa_printf(MSG_INFO, "DPP: URI missing public-key");
		return NULL;
	}

	bi = os_zalloc(sizeof(*bi));
	if (!bi)
		return NULL;

	if (dpp_clone_uri(bi, uri) < 0 ||
	    dpp_parse_uri_chan_list(bi, chan_list) < 0 ||
	    dpp_parse_uri_mac(bi, mac) < 0 ||
	    dpp_parse_uri_info(bi, info) < 0 ||
	    dpp_parse_uri_version(bi, version) < 0 ||
	    dpp_parse_uri_pk(bi, pk) < 0) {
		dpp_bootstrap_info_free(bi);
		bi = NULL;
	}

	return bi;
}


void dpp_build_attr_status(struct wpabuf *msg, enum dpp_status_error status)
{
	wpa_printf(MSG_DEBUG, "DPP: Status %d", status);
	wpabuf_put_le16(msg, DPP_ATTR_STATUS);
	wpabuf_put_le16(msg, 1);
	wpabuf_put_u8(msg, status);
}


void dpp_build_attr_r_bootstrap_key_hash(struct wpabuf *msg, const u8 *hash)
{
	if (hash) {
		wpa_printf(MSG_DEBUG, "DPP: R-Bootstrap Key Hash");
		wpabuf_put_le16(msg, DPP_ATTR_R_BOOTSTRAP_KEY_HASH);
		wpabuf_put_le16(msg, SHA256_MAC_LEN);
		wpabuf_put_data(msg, hash, SHA256_MAC_LEN);
	}
}


static int dpp_channel_ok_init(struct hostapd_hw_modes *own_modes,
			       u16 num_modes, unsigned int freq)
{
	u16 m;
	int c, flag;

	if (!own_modes || !num_modes)
		return 1;

	for (m = 0; m < num_modes; m++) {
		for (c = 0; c < own_modes[m].num_channels; c++) {
			if ((unsigned int) own_modes[m].channels[c].freq !=
			    freq)
				continue;
			flag = own_modes[m].channels[c].flag;
			if (!(flag & (HOSTAPD_CHAN_DISABLED |
				      HOSTAPD_CHAN_NO_IR |
				      HOSTAPD_CHAN_RADAR)))
				return 1;
		}
	}

	wpa_printf(MSG_DEBUG, "DPP: Peer channel %u MHz not supported", freq);
	return 0;
}


static int freq_included(const unsigned int freqs[], unsigned int num,
			 unsigned int freq)
{
	while (num > 0) {
		if (freqs[--num] == freq)
			return 1;
	}
	return 0;
}


static void freq_to_start(unsigned int freqs[], unsigned int num,
			  unsigned int freq)
{
	unsigned int i;

	for (i = 0; i < num; i++) {
		if (freqs[i] == freq)
			break;
	}
	if (i == 0 || i >= num)
		return;
	os_memmove(&freqs[1], &freqs[0], i * sizeof(freqs[0]));
	freqs[0] = freq;
}


static int dpp_channel_intersect(struct dpp_authentication *auth,
				 struct hostapd_hw_modes *own_modes,
				 u16 num_modes)
{
	struct dpp_bootstrap_info *peer_bi = auth->peer_bi;
	unsigned int i, freq;

	for (i = 0; i < peer_bi->num_freq; i++) {
		freq = peer_bi->freq[i];
		if (freq_included(auth->freq, auth->num_freq, freq))
			continue;
		if (dpp_channel_ok_init(own_modes, num_modes, freq))
			auth->freq[auth->num_freq++] = freq;
	}
	if (!auth->num_freq) {
		wpa_printf(MSG_INFO,
			   "DPP: No available channels for initiating DPP Authentication");
		return -1;
	}
	auth->curr_freq = auth->freq[0];
	return 0;
}


static int dpp_channel_local_list(struct dpp_authentication *auth,
				  struct hostapd_hw_modes *own_modes,
				  u16 num_modes)
{
	u16 m;
	int c, flag;
	unsigned int freq;

	auth->num_freq = 0;

	if (!own_modes || !num_modes) {
		auth->freq[0] = 2412;
		auth->freq[1] = 2437;
		auth->freq[2] = 2462;
		auth->num_freq = 3;
		return 0;
	}

	for (m = 0; m < num_modes; m++) {
		for (c = 0; c < own_modes[m].num_channels; c++) {
			freq = own_modes[m].channels[c].freq;
			flag = own_modes[m].channels[c].flag;
			if (flag & (HOSTAPD_CHAN_DISABLED |
				    HOSTAPD_CHAN_NO_IR |
				    HOSTAPD_CHAN_RADAR))
				continue;
			if (freq_included(auth->freq, auth->num_freq, freq))
				continue;
			auth->freq[auth->num_freq++] = freq;
			if (auth->num_freq == DPP_BOOTSTRAP_MAX_FREQ) {
				m = num_modes;
				break;
			}
		}
	}

	return auth->num_freq == 0 ? -1 : 0;
}


int dpp_prepare_channel_list(struct dpp_authentication *auth,
			     unsigned int neg_freq,
			     struct hostapd_hw_modes *own_modes, u16 num_modes)
{
	int res;
	char freqs[DPP_BOOTSTRAP_MAX_FREQ * 6 + 10], *pos, *end;
	unsigned int i;

	if (!own_modes) {
		if (!neg_freq)
			return -1;
		auth->num_freq = 1;
		auth->freq[0] = neg_freq;
		auth->curr_freq = neg_freq;
		return 0;
	}

	if (auth->peer_bi->num_freq > 0)
		res = dpp_channel_intersect(auth, own_modes, num_modes);
	else
		res = dpp_channel_local_list(auth, own_modes, num_modes);
	if (res < 0)
		return res;

	/* Prioritize 2.4 GHz channels 6, 1, 11 (in this order) to hit the most
	 * likely channels first. */
	freq_to_start(auth->freq, auth->num_freq, 2462);
	freq_to_start(auth->freq, auth->num_freq, 2412);
	freq_to_start(auth->freq, auth->num_freq, 2437);

	auth->freq_idx = 0;
	auth->curr_freq = auth->freq[0];

	pos = freqs;
	end = pos + sizeof(freqs);
	for (i = 0; i < auth->num_freq; i++) {
		res = os_snprintf(pos, end - pos, " %u", auth->freq[i]);
		if (os_snprintf_error(end - pos, res))
			break;
		pos += res;
	}
	*pos = '\0';
	wpa_printf(MSG_DEBUG, "DPP: Possible frequencies for initiating:%s",
		   freqs);

	return 0;
}


int dpp_gen_uri(struct dpp_bootstrap_info *bi)
{
	char macstr[ETH_ALEN * 2 + 10];
	size_t len;

	len = 4; /* "DPP:" */
	if (bi->chan)
		len += 3 + os_strlen(bi->chan); /* C:...; */
	if (is_zero_ether_addr(bi->mac_addr))
		macstr[0] = '\0';
	else
		os_snprintf(macstr, sizeof(macstr), "M:" COMPACT_MACSTR ";",
			    MAC2STR(bi->mac_addr));
	len += os_strlen(macstr); /* M:...; */
	if (bi->info)
		len += 3 + os_strlen(bi->info); /* I:...; */
#ifdef CONFIG_DPP2
	len += 4; /* V:2; */
#endif /* CONFIG_DPP2 */
	len += 4 + os_strlen(bi->pk); /* K:...;; */

	os_free(bi->uri);
	bi->uri = os_malloc(len + 1);
	if (!bi->uri)
		return -1;
	os_snprintf(bi->uri, len + 1, "DPP:%s%s%s%s%s%s%s%sK:%s;;",
		    bi->chan ? "C:" : "", bi->chan ? bi->chan : "",
		    bi->chan ? ";" : "",
		    macstr,
		    bi->info ? "I:" : "", bi->info ? bi->info : "",
		    bi->info ? ";" : "",
		    DPP_VERSION == 3 ? "V:3;" :
		    (DPP_VERSION == 2 ? "V:2;" : ""),
		    bi->pk);
	return 0;
}


struct dpp_authentication *
dpp_alloc_auth(struct dpp_global *dpp, void *msg_ctx)
{
	struct dpp_authentication *auth;

	auth = os_zalloc(sizeof(*auth));
	if (!auth)
		return NULL;
	auth->global = dpp;
	auth->msg_ctx = msg_ctx;
	auth->conf_resp_status = 255;
	return auth;
}


static struct wpabuf * dpp_build_conf_req_attr(struct dpp_authentication *auth,
					       const char *json)
{
	size_t nonce_len;
	size_t json_len, clear_len;
	struct wpabuf *clear = NULL, *msg = NULL;
	u8 *wrapped;
	size_t attr_len;

	wpa_printf(MSG_DEBUG, "DPP: Build configuration request");

	nonce_len = auth->curve->nonce_len;
	if (random_get_bytes(auth->e_nonce, nonce_len)) {
		wpa_printf(MSG_ERROR, "DPP: Failed to generate E-nonce");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: E-nonce", auth->e_nonce, nonce_len);
	json_len = os_strlen(json);
	wpa_hexdump_ascii(MSG_DEBUG, "DPP: configRequest JSON", json, json_len);

	/* { E-nonce, configAttrib }ke */
	clear_len = 4 + nonce_len + 4 + json_len;
	clear = wpabuf_alloc(clear_len);
	attr_len = 4 + clear_len + AES_BLOCK_SIZE;
#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_AFTER_WRAPPED_DATA_CONF_REQ)
		attr_len += 5;
#endif /* CONFIG_TESTING_OPTIONS */
	msg = wpabuf_alloc(attr_len);
	if (!clear || !msg)
		goto fail;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_E_NONCE_CONF_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no E-nonce");
		goto skip_e_nonce;
	}
	if (dpp_test == DPP_TEST_INVALID_E_NONCE_CONF_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - invalid E-nonce");
		wpabuf_put_le16(clear, DPP_ATTR_ENROLLEE_NONCE);
		wpabuf_put_le16(clear, nonce_len - 1);
		wpabuf_put_data(clear, auth->e_nonce, nonce_len - 1);
		goto skip_e_nonce;
	}
	if (dpp_test == DPP_TEST_NO_WRAPPED_DATA_CONF_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Wrapped Data");
		goto skip_wrapped_data;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* E-nonce */
	wpabuf_put_le16(clear, DPP_ATTR_ENROLLEE_NONCE);
	wpabuf_put_le16(clear, nonce_len);
	wpabuf_put_data(clear, auth->e_nonce, nonce_len);

#ifdef CONFIG_TESTING_OPTIONS
skip_e_nonce:
	if (dpp_test == DPP_TEST_NO_CONFIG_ATTR_OBJ_CONF_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no configAttrib");
		goto skip_conf_attr_obj;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* configAttrib */
	wpabuf_put_le16(clear, DPP_ATTR_CONFIG_ATTR_OBJ);
	wpabuf_put_le16(clear, json_len);
	wpabuf_put_data(clear, json, json_len);

#ifdef CONFIG_TESTING_OPTIONS
skip_conf_attr_obj:
#endif /* CONFIG_TESTING_OPTIONS */

	wpabuf_put_le16(msg, DPP_ATTR_WRAPPED_DATA);
	wpabuf_put_le16(msg, wpabuf_len(clear) + AES_BLOCK_SIZE);
	wrapped = wpabuf_put(msg, wpabuf_len(clear) + AES_BLOCK_SIZE);

	/* No AES-SIV AD */
	wpa_hexdump_buf(MSG_DEBUG, "DPP: AES-SIV cleartext", clear);
	if (aes_siv_encrypt(auth->ke, auth->curve->hash_len,
			    wpabuf_head(clear), wpabuf_len(clear),
			    0, NULL, NULL, wrapped) < 0)
		goto fail;
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV ciphertext",
		    wrapped, wpabuf_len(clear) + AES_BLOCK_SIZE);

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_AFTER_WRAPPED_DATA_CONF_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - attr after Wrapped Data");
		dpp_build_attr_status(msg, DPP_STATUS_OK);
	}
skip_wrapped_data:
#endif /* CONFIG_TESTING_OPTIONS */

	wpa_hexdump_buf(MSG_DEBUG,
			"DPP: Configuration Request frame attributes", msg);
	wpabuf_free(clear);
	return msg;

fail:
	wpabuf_free(clear);
	wpabuf_free(msg);
	return NULL;
}


void dpp_write_adv_proto(struct wpabuf *buf)
{
	/* Advertisement Protocol IE */
	wpabuf_put_u8(buf, WLAN_EID_ADV_PROTO);
	wpabuf_put_u8(buf, 8); /* Length */
	wpabuf_put_u8(buf, 0x7f);
	wpabuf_put_u8(buf, WLAN_EID_VENDOR_SPECIFIC);
	wpabuf_put_u8(buf, 5);
	wpabuf_put_be24(buf, OUI_WFA);
	wpabuf_put_u8(buf, DPP_OUI_TYPE);
	wpabuf_put_u8(buf, 0x01);
}


void dpp_write_gas_query(struct wpabuf *buf, struct wpabuf *query)
{
	/* GAS Query */
	wpabuf_put_le16(buf, wpabuf_len(query));
	wpabuf_put_buf(buf, query);
}


struct wpabuf * dpp_build_conf_req(struct dpp_authentication *auth,
				   const char *json)
{
	struct wpabuf *buf, *conf_req;

	conf_req = dpp_build_conf_req_attr(auth, json);
	if (!conf_req) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No configuration request data available");
		return NULL;
	}

	buf = gas_build_initial_req(0, 10 + 2 + wpabuf_len(conf_req));
	if (!buf) {
		wpabuf_free(conf_req);
		return NULL;
	}

	dpp_write_adv_proto(buf);
	dpp_write_gas_query(buf, conf_req);
	wpabuf_free(conf_req);
	wpa_hexdump_buf(MSG_MSGDUMP, "DPP: GAS Config Request", buf);

	return buf;
}


struct wpabuf * dpp_build_conf_req_helper(struct dpp_authentication *auth,
					  const char *name,
					  enum dpp_netrole netrole,
					  const char *mud_url, int *opclasses)
{
	size_t len, name_len;
	const char *tech = "infra";
	const char *dpp_name;
	struct wpabuf *buf, *json;
	char *csr = NULL;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_INVALID_CONFIG_ATTR_OBJ_CONF_REQ) {
		static const char *bogus_tech = "knfra";

		wpa_printf(MSG_INFO, "DPP: TESTING - invalid Config Attr");
		tech = bogus_tech;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	dpp_name = name ? name : "Test";
	name_len = os_strlen(dpp_name);

	len = 100 + name_len * 6 + 1 + int_array_len(opclasses) * 4;
	if (mud_url && mud_url[0])
		len += 10 + os_strlen(mud_url);
#ifdef CONFIG_DPP2
	if (auth->csr) {
		size_t csr_len;

		csr = base64_encode_no_lf(wpabuf_head(auth->csr),
					  wpabuf_len(auth->csr), &csr_len);
		if (!csr)
			return NULL;
		len += 30 + csr_len;
	}
#endif /* CONFIG_DPP2 */
	json = wpabuf_alloc(len);
	if (!json)
		return NULL;

	json_start_object(json, NULL);
	if (json_add_string_escape(json, "name", dpp_name, name_len) < 0) {
		wpabuf_free(json);
		return NULL;
	}
	json_value_sep(json);
	json_add_string(json, "wi-fi_tech", tech);
	json_value_sep(json);
	json_add_string(json, "netRole", dpp_netrole_str(netrole));
	if (mud_url && mud_url[0]) {
		json_value_sep(json);
		json_add_string(json, "mudurl", mud_url);
	}
	if (opclasses) {
		int i;

		json_value_sep(json);
		json_start_array(json, "bandSupport");
		for (i = 0; opclasses[i]; i++)
			wpabuf_printf(json, "%s%u", i ? "," : "", opclasses[i]);
		json_end_array(json);
	}
	if (csr) {
		json_value_sep(json);
		json_add_string(json, "pkcs10", csr);
	}
	json_end_object(json);

	buf = dpp_build_conf_req(auth, wpabuf_head(json));
	wpabuf_free(json);
	os_free(csr);

	return buf;
}


static int bin_str_eq(const char *val, size_t len, const char *cmp)
{
	return os_strlen(cmp) == len && os_memcmp(val, cmp, len) == 0;
}


struct dpp_configuration * dpp_configuration_alloc(const char *type)
{
	struct dpp_configuration *conf;
	const char *end;
	size_t len;

	conf = os_zalloc(sizeof(*conf));
	if (!conf)
		goto fail;

	end = os_strchr(type, ' ');
	if (end)
		len = end - type;
	else
		len = os_strlen(type);

	if (bin_str_eq(type, len, "psk"))
		conf->akm = DPP_AKM_PSK;
	else if (bin_str_eq(type, len, "sae"))
		conf->akm = DPP_AKM_SAE;
	else if (bin_str_eq(type, len, "psk-sae") ||
		 bin_str_eq(type, len, "psk+sae"))
		conf->akm = DPP_AKM_PSK_SAE;
	else if (bin_str_eq(type, len, "sae-dpp") ||
		 bin_str_eq(type, len, "dpp+sae"))
		conf->akm = DPP_AKM_SAE_DPP;
	else if (bin_str_eq(type, len, "psk-sae-dpp") ||
		 bin_str_eq(type, len, "dpp+psk+sae"))
		conf->akm = DPP_AKM_PSK_SAE_DPP;
	else if (bin_str_eq(type, len, "dpp"))
		conf->akm = DPP_AKM_DPP;
	else if (bin_str_eq(type, len, "dot1x"))
		conf->akm = DPP_AKM_DOT1X;
	else
		goto fail;

	return conf;
fail:
	dpp_configuration_free(conf);
	return NULL;
}


int dpp_akm_psk(enum dpp_akm akm)
{
	return akm == DPP_AKM_PSK || akm == DPP_AKM_PSK_SAE ||
		akm == DPP_AKM_PSK_SAE_DPP;
}


int dpp_akm_sae(enum dpp_akm akm)
{
	return akm == DPP_AKM_SAE || akm == DPP_AKM_PSK_SAE ||
		akm == DPP_AKM_SAE_DPP || akm == DPP_AKM_PSK_SAE_DPP;
}


int dpp_akm_legacy(enum dpp_akm akm)
{
	return akm == DPP_AKM_PSK || akm == DPP_AKM_PSK_SAE ||
		akm == DPP_AKM_SAE;
}


int dpp_akm_dpp(enum dpp_akm akm)
{
	return akm == DPP_AKM_DPP || akm == DPP_AKM_SAE_DPP ||
		akm == DPP_AKM_PSK_SAE_DPP;
}


int dpp_akm_ver2(enum dpp_akm akm)
{
	return akm == DPP_AKM_SAE_DPP || akm == DPP_AKM_PSK_SAE_DPP;
}


int dpp_configuration_valid(const struct dpp_configuration *conf)
{
	if (conf->ssid_len == 0)
		return 0;
	if (dpp_akm_psk(conf->akm) && !conf->passphrase && !conf->psk_set)
		return 0;
	if (dpp_akm_sae(conf->akm) && !conf->passphrase)
		return 0;
	return 1;
}


void dpp_configuration_free(struct dpp_configuration *conf)
{
	if (!conf)
		return;
	str_clear_free(conf->passphrase);
	os_free(conf->group_id);
	os_free(conf->csrattrs);
	bin_clear_free(conf, sizeof(*conf));
}


static int dpp_configuration_parse_helper(struct dpp_authentication *auth,
					  const char *cmd, int idx)
{
	const char *pos, *end;
	struct dpp_configuration *conf_sta = NULL, *conf_ap = NULL;
	struct dpp_configuration *conf = NULL;
	size_t len;

	pos = os_strstr(cmd, " conf=sta-");
	if (pos) {
		conf_sta = dpp_configuration_alloc(pos + 10);
		if (!conf_sta)
			goto fail;
		conf_sta->netrole = DPP_NETROLE_STA;
		conf = conf_sta;
	}

	pos = os_strstr(cmd, " conf=ap-");
	if (pos) {
		conf_ap = dpp_configuration_alloc(pos + 9);
		if (!conf_ap)
			goto fail;
		conf_ap->netrole = DPP_NETROLE_AP;
		conf = conf_ap;
	}

	pos = os_strstr(cmd, " conf=configurator");
	if (pos)
		auth->provision_configurator = 1;

	if (!conf)
		return 0;

	pos = os_strstr(cmd, " ssid=");
	if (pos) {
		pos += 6;
		end = os_strchr(pos, ' ');
		conf->ssid_len = end ? (size_t) (end - pos) : os_strlen(pos);
		conf->ssid_len /= 2;
		if (conf->ssid_len > sizeof(conf->ssid) ||
		    hexstr2bin(pos, conf->ssid, conf->ssid_len) < 0)
			goto fail;
	} else {
#ifdef CONFIG_TESTING_OPTIONS
		/* use a default SSID for legacy testing reasons */
		os_memcpy(conf->ssid, "test", 4);
		conf->ssid_len = 4;
#else /* CONFIG_TESTING_OPTIONS */
		goto fail;
#endif /* CONFIG_TESTING_OPTIONS */
	}

	pos = os_strstr(cmd, " ssid_charset=");
	if (pos) {
		if (conf_ap) {
			wpa_printf(MSG_INFO,
				   "DPP: ssid64 option (ssid_charset param) not allowed for AP enrollee");
			goto fail;
		}
		conf->ssid_charset = atoi(pos + 14);
	}

	pos = os_strstr(cmd, " pass=");
	if (pos) {
		size_t pass_len;

		pos += 6;
		end = os_strchr(pos, ' ');
		pass_len = end ? (size_t) (end - pos) : os_strlen(pos);
		pass_len /= 2;
		if (pass_len > 63 || pass_len < 8)
			goto fail;
		conf->passphrase = os_zalloc(pass_len + 1);
		if (!conf->passphrase ||
		    hexstr2bin(pos, (u8 *) conf->passphrase, pass_len) < 0)
			goto fail;
	}

	pos = os_strstr(cmd, " psk=");
	if (pos) {
		pos += 5;
		if (hexstr2bin(pos, conf->psk, PMK_LEN) < 0)
			goto fail;
		conf->psk_set = 1;
	}

	pos = os_strstr(cmd, " group_id=");
	if (pos) {
		size_t group_id_len;

		pos += 10;
		end = os_strchr(pos, ' ');
		group_id_len = end ? (size_t) (end - pos) : os_strlen(pos);
		conf->group_id = os_malloc(group_id_len + 1);
		if (!conf->group_id)
			goto fail;
		os_memcpy(conf->group_id, pos, group_id_len);
		conf->group_id[group_id_len] = '\0';
	}

	pos = os_strstr(cmd, " expiry=");
	if (pos) {
		long int val;

		pos += 8;
		val = strtol(pos, NULL, 0);
		if (val <= 0)
			goto fail;
		conf->netaccesskey_expiry = val;
	}

	pos = os_strstr(cmd, " csrattrs=");
	if (pos) {
		pos += 10;
		end = os_strchr(pos, ' ');
		len = end ? (size_t) (end - pos) : os_strlen(pos);
		conf->csrattrs = os_zalloc(len + 1);
		if (!conf->csrattrs)
			goto fail;
		os_memcpy(conf->csrattrs, pos, len);
	}

	if (!dpp_configuration_valid(conf))
		goto fail;

	if (idx == 0) {
		auth->conf_sta = conf_sta;
		auth->conf_ap = conf_ap;
	} else if (idx == 1) {
		auth->conf2_sta = conf_sta;
		auth->conf2_ap = conf_ap;
	} else {
		goto fail;
	}
	return 0;

fail:
	dpp_configuration_free(conf_sta);
	dpp_configuration_free(conf_ap);
	return -1;
}


static int dpp_configuration_parse(struct dpp_authentication *auth,
				   const char *cmd)
{
	const char *pos;
	char *tmp;
	size_t len;
	int res;

	pos = os_strstr(cmd, " @CONF-OBJ-SEP@ ");
	if (!pos)
		return dpp_configuration_parse_helper(auth, cmd, 0);

	len = pos - cmd;
	tmp = os_malloc(len + 1);
	if (!tmp)
		goto fail;
	os_memcpy(tmp, cmd, len);
	tmp[len] = '\0';
	res = dpp_configuration_parse_helper(auth, cmd, 0);
	str_clear_free(tmp);
	if (res)
		goto fail;
	res = dpp_configuration_parse_helper(auth, cmd + len, 1);
	if (res)
		goto fail;
	return 0;
fail:
	dpp_configuration_free(auth->conf_sta);
	dpp_configuration_free(auth->conf2_sta);
	dpp_configuration_free(auth->conf_ap);
	dpp_configuration_free(auth->conf2_ap);
	return -1;
}


static struct dpp_configurator *
dpp_configurator_get_id(struct dpp_global *dpp, unsigned int id)
{
	struct dpp_configurator *conf;

	if (!dpp)
		return NULL;

	dl_list_for_each(conf, &dpp->configurator,
			 struct dpp_configurator, list) {
		if (conf->id == id)
			return conf;
	}
	return NULL;
}


int dpp_set_configurator(struct dpp_authentication *auth, const char *cmd)
{
	const char *pos;
	char *tmp = NULL;
	int ret = -1;

	if (!cmd || auth->configurator_set)
		return 0;
	auth->configurator_set = 1;

	if (cmd[0] != ' ') {
		size_t len;

		len = os_strlen(cmd);
		tmp = os_malloc(len + 2);
		if (!tmp)
			goto fail;
		tmp[0] = ' ';
		os_memcpy(tmp + 1, cmd, len + 1);
		cmd = tmp;
	}

	wpa_printf(MSG_DEBUG, "DPP: Set configurator parameters: %s", cmd);

	pos = os_strstr(cmd, " configurator=");
	if (!auth->conf && pos) {
		pos += 14;
		auth->conf = dpp_configurator_get_id(auth->global, atoi(pos));
		if (!auth->conf) {
			wpa_printf(MSG_INFO,
				   "DPP: Could not find the specified configurator");
			goto fail;
		}
	}

	pos = os_strstr(cmd, " conn_status=");
	if (pos) {
		pos += 13;
		auth->send_conn_status = atoi(pos);
	}

	pos = os_strstr(cmd, " akm_use_selector=");
	if (pos) {
		pos += 18;
		auth->akm_use_selector = atoi(pos);
	}

	if (dpp_configuration_parse(auth, cmd) < 0) {
		wpa_msg(auth->msg_ctx, MSG_INFO,
			"DPP: Failed to set configurator parameters");
		goto fail;
	}
	ret = 0;
fail:
	os_free(tmp);
	return ret;
}


void dpp_auth_deinit(struct dpp_authentication *auth)
{
	unsigned int i;

	if (!auth)
		return;
	dpp_configuration_free(auth->conf_ap);
	dpp_configuration_free(auth->conf2_ap);
	dpp_configuration_free(auth->conf_sta);
	dpp_configuration_free(auth->conf2_sta);
	crypto_ec_key_deinit(auth->own_protocol_key);
	crypto_ec_key_deinit(auth->peer_protocol_key);
	crypto_ec_key_deinit(auth->reconfig_old_protocol_key);
	wpabuf_free(auth->req_msg);
	wpabuf_free(auth->resp_msg);
	wpabuf_free(auth->conf_req);
	wpabuf_free(auth->reconfig_req_msg);
	wpabuf_free(auth->reconfig_resp_msg);
	for (i = 0; i < auth->num_conf_obj; i++) {
		struct dpp_config_obj *conf = &auth->conf_obj[i];

		os_free(conf->connector);
		wpabuf_free(conf->c_sign_key);
		wpabuf_free(conf->certbag);
		wpabuf_free(conf->certs);
		wpabuf_free(conf->cacert);
		os_free(conf->server_name);
		wpabuf_free(conf->pp_key);
	}
#ifdef CONFIG_DPP2
	dpp_free_asymmetric_key(auth->conf_key_pkg);
	os_free(auth->csrattrs);
	wpabuf_free(auth->csr);
	wpabuf_free(auth->priv_key);
	wpabuf_free(auth->cacert);
	wpabuf_free(auth->certbag);
	os_free(auth->trusted_eap_server_name);
	wpabuf_free(auth->conf_resp_tcp);
#endif /* CONFIG_DPP2 */
	wpabuf_free(auth->net_access_key);
	dpp_bootstrap_info_free(auth->tmp_own_bi);
	if (auth->tmp_peer_bi) {
		dl_list_del(&auth->tmp_peer_bi->list);
		dpp_bootstrap_info_free(auth->tmp_peer_bi);
	}
#ifdef CONFIG_TESTING_OPTIONS
	os_free(auth->config_obj_override);
	os_free(auth->discovery_override);
	os_free(auth->groups_override);
#endif /* CONFIG_TESTING_OPTIONS */
	bin_clear_free(auth, sizeof(*auth));
}


static struct wpabuf *
dpp_build_conf_start(struct dpp_authentication *auth,
		     struct dpp_configuration *conf, size_t tailroom)
{
	struct wpabuf *buf;

#ifdef CONFIG_TESTING_OPTIONS
	if (auth->discovery_override)
		tailroom += os_strlen(auth->discovery_override);
#endif /* CONFIG_TESTING_OPTIONS */

	buf = wpabuf_alloc(200 + tailroom);
	if (!buf)
		return NULL;
	json_start_object(buf, NULL);
	json_add_string(buf, "wi-fi_tech", "infra");
	json_value_sep(buf);
#ifdef CONFIG_TESTING_OPTIONS
	if (auth->discovery_override) {
		wpa_printf(MSG_DEBUG, "DPP: TESTING - discovery override: '%s'",
			   auth->discovery_override);
		wpabuf_put_str(buf, "\"discovery\":");
		wpabuf_put_str(buf, auth->discovery_override);
		json_value_sep(buf);
		return buf;
	}
#endif /* CONFIG_TESTING_OPTIONS */
	json_start_object(buf, "discovery");
	if (((!conf->ssid_charset || auth->peer_version < 2) &&
	     json_add_string_escape(buf, "ssid", conf->ssid,
				    conf->ssid_len) < 0) ||
	    ((conf->ssid_charset && auth->peer_version >= 2) &&
	     json_add_base64url(buf, "ssid64", conf->ssid,
				conf->ssid_len) < 0)) {
		wpabuf_free(buf);
		return NULL;
	}
	if (conf->ssid_charset > 0) {
		json_value_sep(buf);
		json_add_int(buf, "ssid_charset", conf->ssid_charset);
	}
	json_end_object(buf);
	json_value_sep(buf);

	return buf;
}


int dpp_build_jwk(struct wpabuf *buf, const char *name,
		  struct crypto_ec_key *key, const char *kid,
		  const struct dpp_curve_params *curve)
{
	struct wpabuf *pub;
	const u8 *pos;
	int ret = -1;

	pub = crypto_ec_key_get_pubkey_point(key, 0);
	if (!pub)
		goto fail;

	json_start_object(buf, name);
	json_add_string(buf, "kty", "EC");
	json_value_sep(buf);
	json_add_string(buf, "crv", curve->jwk_crv);
	json_value_sep(buf);
	pos = wpabuf_head(pub);
	if (json_add_base64url(buf, "x", pos, curve->prime_len) < 0)
		goto fail;
	json_value_sep(buf);
	pos += curve->prime_len;
	if (json_add_base64url(buf, "y", pos, curve->prime_len) < 0)
		goto fail;
	if (kid) {
		json_value_sep(buf);
		json_add_string(buf, "kid", kid);
	}
	json_end_object(buf);
	ret = 0;
fail:
	wpabuf_free(pub);
	return ret;
}


static void dpp_build_legacy_cred_params(struct wpabuf *buf,
					 struct dpp_configuration *conf)
{
	if (conf->passphrase && os_strlen(conf->passphrase) < 64) {
		json_add_string_escape(buf, "pass", conf->passphrase,
				       os_strlen(conf->passphrase));
	} else if (conf->psk_set) {
		char psk[2 * sizeof(conf->psk) + 1];

		wpa_snprintf_hex(psk, sizeof(psk),
				 conf->psk, sizeof(conf->psk));
		json_add_string(buf, "psk_hex", psk);
		forced_memzero(psk, sizeof(psk));
	}
}


static const char * dpp_netrole_str(enum dpp_netrole netrole)
{
	switch (netrole) {
	case DPP_NETROLE_STA:
		return "sta";
	case DPP_NETROLE_AP:
		return "ap";
	case DPP_NETROLE_CONFIGURATOR:
		return "configurator";
	default:
		return "??";
	}
}


static struct wpabuf *
dpp_build_conf_obj_dpp(struct dpp_authentication *auth,
		       struct dpp_configuration *conf)
{
	struct wpabuf *buf = NULL;
	char *signed_conn = NULL;
	size_t tailroom;
	const struct dpp_curve_params *curve;
	struct wpabuf *dppcon = NULL;
	size_t extra_len = 1000;
	int incl_legacy;
	enum dpp_akm akm;
	const char *akm_str;

	if (!auth->conf) {
		wpa_printf(MSG_INFO,
			   "DPP: No configurator specified - cannot generate DPP config object");
		goto fail;
	}
	curve = auth->conf->curve;

	akm = conf->akm;
	if (dpp_akm_ver2(akm) && auth->peer_version < 2) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Convert DPP+legacy credential to DPP-only for peer that does not support version 2");
		akm = DPP_AKM_DPP;
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (auth->groups_override)
		extra_len += os_strlen(auth->groups_override);
#endif /* CONFIG_TESTING_OPTIONS */

	if (conf->group_id)
		extra_len += os_strlen(conf->group_id);

	/* Connector (JSON dppCon object) */
	dppcon = wpabuf_alloc(extra_len + 2 * auth->curve->prime_len * 4 / 3);
	if (!dppcon)
		goto fail;
#ifdef CONFIG_TESTING_OPTIONS
	if (auth->groups_override) {
		wpabuf_put_u8(dppcon, '{');
		if (auth->groups_override) {
			wpa_printf(MSG_DEBUG,
				   "DPP: TESTING - groups override: '%s'",
				   auth->groups_override);
			wpabuf_put_str(dppcon, "\"groups\":");
			wpabuf_put_str(dppcon, auth->groups_override);
			json_value_sep(dppcon);
		}
		goto skip_groups;
	}
#endif /* CONFIG_TESTING_OPTIONS */
	json_start_object(dppcon, NULL);
	json_start_array(dppcon, "groups");
	json_start_object(dppcon, NULL);
	json_add_string(dppcon, "groupId",
			conf->group_id ? conf->group_id : "*");
	json_value_sep(dppcon);
	json_add_string(dppcon, "netRole", dpp_netrole_str(conf->netrole));
	json_end_object(dppcon);
	json_end_array(dppcon);
	json_value_sep(dppcon);
#ifdef CONFIG_TESTING_OPTIONS
skip_groups:
#endif /* CONFIG_TESTING_OPTIONS */
	if (!auth->peer_protocol_key ||
	    dpp_build_jwk(dppcon, "netAccessKey", auth->peer_protocol_key, NULL,
			  auth->curve) < 0) {
		wpa_printf(MSG_DEBUG, "DPP: Failed to build netAccessKey JWK");
		goto fail;
	}
	if (conf->netaccesskey_expiry) {
		struct os_tm tm;
		char expiry[30];

		if (os_gmtime(conf->netaccesskey_expiry, &tm) < 0) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Failed to generate expiry string");
			goto fail;
		}
		os_snprintf(expiry, sizeof(expiry),
			    "%04u-%02u-%02uT%02u:%02u:%02uZ",
			    tm.year, tm.month, tm.day,
			    tm.hour, tm.min, tm.sec);
		json_value_sep(dppcon);
		json_add_string(dppcon, "expiry", expiry);
	}
#ifdef CONFIG_DPP3
	json_value_sep(dppcon);
	json_add_int(dppcon, "version", auth->peer_version);
#endif /* CONFIG_DPP3 */
	json_end_object(dppcon);
	wpa_printf(MSG_DEBUG, "DPP: dppCon: %s",
		   (const char *) wpabuf_head(dppcon));

	signed_conn = dpp_sign_connector(auth->conf, dppcon);
	if (!signed_conn)
		goto fail;

	incl_legacy = dpp_akm_psk(akm) || dpp_akm_sae(akm);
	tailroom = 1000;
	tailroom += 2 * curve->prime_len * 4 / 3 + os_strlen(auth->conf->kid);
	tailroom += os_strlen(signed_conn);
	if (incl_legacy)
		tailroom += 1000;
	if (akm == DPP_AKM_DOT1X) {
		if (auth->certbag)
			tailroom += 2 * wpabuf_len(auth->certbag);
		if (auth->cacert)
			tailroom += 2 * wpabuf_len(auth->cacert);
		if (auth->trusted_eap_server_name)
			tailroom += os_strlen(auth->trusted_eap_server_name);
		tailroom += 1000;
	}
	buf = dpp_build_conf_start(auth, conf, tailroom);
	if (!buf)
		goto fail;

	if (auth->akm_use_selector && dpp_akm_ver2(akm))
		akm_str = dpp_akm_selector_str(akm);
	else
		akm_str = dpp_akm_str(akm);
	json_start_object(buf, "cred");
	json_add_string(buf, "akm", akm_str);
	json_value_sep(buf);
	if (incl_legacy) {
		dpp_build_legacy_cred_params(buf, conf);
		json_value_sep(buf);
	}
	if (akm == DPP_AKM_DOT1X) {
		json_start_object(buf, "entCreds");
		if (!auth->certbag)
			goto fail;
		json_add_base64(buf, "certBag", wpabuf_head(auth->certbag),
				wpabuf_len(auth->certbag));
		if (auth->cacert) {
			json_value_sep(buf);
			json_add_base64(buf, "caCert",
					wpabuf_head(auth->cacert),
					wpabuf_len(auth->cacert));
		}
		if (auth->trusted_eap_server_name) {
			json_value_sep(buf);
			json_add_string(buf, "trustedEapServerName",
					auth->trusted_eap_server_name);
		}
		json_value_sep(buf);
		json_start_array(buf, "eapMethods");
		wpabuf_printf(buf, "%d", EAP_TYPE_TLS);
		json_end_array(buf);
		json_end_object(buf);
		json_value_sep(buf);
	}
	wpabuf_put_str(buf, "\"signedConnector\":\"");
	wpabuf_put_str(buf, signed_conn);
	wpabuf_put_str(buf, "\"");
	json_value_sep(buf);
	if (dpp_build_jwk(buf, "csign", auth->conf->csign, auth->conf->kid,
			  curve) < 0) {
		wpa_printf(MSG_DEBUG, "DPP: Failed to build csign JWK");
		goto fail;
	}
#ifdef CONFIG_DPP2
	if (auth->peer_version >= 2 && auth->conf->pp_key) {
		json_value_sep(buf);
		if (dpp_build_jwk(buf, "ppKey", auth->conf->pp_key, NULL,
				  curve) < 0) {
			wpa_printf(MSG_DEBUG, "DPP: Failed to build ppKey JWK");
			goto fail;
		}
	}
#endif /* CONFIG_DPP2 */

	json_end_object(buf);
	json_end_object(buf);

	wpa_hexdump_ascii_key(MSG_DEBUG, "DPP: Configuration Object",
			      wpabuf_head(buf), wpabuf_len(buf));

out:
	os_free(signed_conn);
	wpabuf_free(dppcon);
	return buf;
fail:
	wpa_printf(MSG_DEBUG, "DPP: Failed to build configuration object");
	wpabuf_free(buf);
	buf = NULL;
	goto out;
}


static struct wpabuf *
dpp_build_conf_obj_legacy(struct dpp_authentication *auth,
			  struct dpp_configuration *conf)
{
	struct wpabuf *buf;
	const char *akm_str;

	buf = dpp_build_conf_start(auth, conf, 1000);
	if (!buf)
		return NULL;

	if (auth->akm_use_selector && dpp_akm_ver2(conf->akm))
		akm_str = dpp_akm_selector_str(conf->akm);
	else
		akm_str = dpp_akm_str(conf->akm);
	json_start_object(buf, "cred");
	json_add_string(buf, "akm", akm_str);
	json_value_sep(buf);
	dpp_build_legacy_cred_params(buf, conf);
	json_end_object(buf);
	json_end_object(buf);

	wpa_hexdump_ascii_key(MSG_DEBUG, "DPP: Configuration Object (legacy)",
			      wpabuf_head(buf), wpabuf_len(buf));

	return buf;
}


static struct wpabuf *
dpp_build_conf_obj(struct dpp_authentication *auth, enum dpp_netrole netrole,
		   int idx, bool cert_req)
{
	struct dpp_configuration *conf = NULL;

#ifdef CONFIG_TESTING_OPTIONS
	if (auth->config_obj_override) {
		if (idx != 0)
			return NULL;
		wpa_printf(MSG_DEBUG, "DPP: Testing - Config Object override");
		return wpabuf_alloc_copy(auth->config_obj_override,
					 os_strlen(auth->config_obj_override));
	}
#endif /* CONFIG_TESTING_OPTIONS */

	if (idx == 0) {
		if (netrole == DPP_NETROLE_STA)
			conf = auth->conf_sta;
		else if (netrole == DPP_NETROLE_AP)
			conf = auth->conf_ap;
	} else if (idx == 1) {
		if (netrole == DPP_NETROLE_STA)
			conf = auth->conf2_sta;
		else if (netrole == DPP_NETROLE_AP)
			conf = auth->conf2_ap;
	}
	if (!conf) {
		if (idx == 0)
			wpa_printf(MSG_DEBUG,
				   "DPP: No configuration available for Enrollee(%s) - reject configuration request",
				   dpp_netrole_str(netrole));
		return NULL;
	}

	if (conf->akm == DPP_AKM_DOT1X) {
		if (!auth->conf) {
			wpa_printf(MSG_DEBUG,
				   "DPP: No Configurator data available");
			return NULL;
		}
		if (!cert_req && !auth->certbag) {
			wpa_printf(MSG_DEBUG,
				   "DPP: No certificate data available for dot1x configuration");
			return NULL;
		}
		return dpp_build_conf_obj_dpp(auth, conf);
	}
	if (dpp_akm_dpp(conf->akm) || (auth->peer_version >= 2 && auth->conf))
		return dpp_build_conf_obj_dpp(auth, conf);
	return dpp_build_conf_obj_legacy(auth, conf);
}


struct wpabuf *
dpp_build_conf_resp(struct dpp_authentication *auth, const u8 *e_nonce,
		    u16 e_nonce_len, enum dpp_netrole netrole, bool cert_req)
{
	struct wpabuf *conf = NULL, *conf2 = NULL, *env_data = NULL;
	size_t clear_len, attr_len;
	struct wpabuf *clear = NULL, *msg = NULL;
	u8 *wrapped;
	const u8 *addr[1];
	size_t len[1];
	enum dpp_status_error status;

	if (auth->force_conf_resp_status != DPP_STATUS_OK) {
		status = auth->force_conf_resp_status;
		goto forced_status;
	}

	if (netrole == DPP_NETROLE_CONFIGURATOR) {
#ifdef CONFIG_DPP2
		env_data = dpp_build_enveloped_data(auth);
#endif /* CONFIG_DPP2 */
	} else {
		conf = dpp_build_conf_obj(auth, netrole, 0, cert_req);
		if (conf) {
			wpa_hexdump_ascii(MSG_DEBUG,
					  "DPP: configurationObject JSON",
					  wpabuf_head(conf), wpabuf_len(conf));
			conf2 = dpp_build_conf_obj(auth, netrole, 1, cert_req);
		}
	}

	if (conf || env_data)
		status = DPP_STATUS_OK;
	else if (!cert_req && netrole == DPP_NETROLE_STA && auth->conf_sta &&
		 auth->conf_sta->akm == DPP_AKM_DOT1X && !auth->waiting_csr)
		status = DPP_STATUS_CSR_NEEDED;
	else
		status = DPP_STATUS_CONFIGURE_FAILURE;
forced_status:
	auth->conf_resp_status = status;

	/* { E-nonce, configurationObject[, sendConnStatus]}ke */
	clear_len = 4 + e_nonce_len;
	if (conf)
		clear_len += 4 + wpabuf_len(conf);
	if (conf2)
		clear_len += 4 + wpabuf_len(conf2);
	if (env_data)
		clear_len += 4 + wpabuf_len(env_data);
	if (auth->peer_version >= 2 && auth->send_conn_status &&
	    netrole == DPP_NETROLE_STA)
		clear_len += 4;
	if (status == DPP_STATUS_CSR_NEEDED && auth->conf_sta &&
	    auth->conf_sta->csrattrs)
		clear_len += 4 + os_strlen(auth->conf_sta->csrattrs);
	clear = wpabuf_alloc(clear_len);
	attr_len = 4 + 1 + 4 + clear_len + AES_BLOCK_SIZE;
#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_AFTER_WRAPPED_DATA_CONF_RESP)
		attr_len += 5;
#endif /* CONFIG_TESTING_OPTIONS */
	msg = wpabuf_alloc(attr_len);
	if (!clear || !msg)
		goto fail;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_E_NONCE_CONF_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no E-nonce");
		goto skip_e_nonce;
	}
	if (dpp_test == DPP_TEST_E_NONCE_MISMATCH_CONF_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - E-nonce mismatch");
		wpabuf_put_le16(clear, DPP_ATTR_ENROLLEE_NONCE);
		wpabuf_put_le16(clear, e_nonce_len);
		wpabuf_put_data(clear, e_nonce, e_nonce_len - 1);
		wpabuf_put_u8(clear, e_nonce[e_nonce_len - 1] ^ 0x01);
		goto skip_e_nonce;
	}
	if (dpp_test == DPP_TEST_NO_WRAPPED_DATA_CONF_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Wrapped Data");
		goto skip_wrapped_data;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* E-nonce */
	wpabuf_put_le16(clear, DPP_ATTR_ENROLLEE_NONCE);
	wpabuf_put_le16(clear, e_nonce_len);
	wpabuf_put_data(clear, e_nonce, e_nonce_len);

#ifdef CONFIG_TESTING_OPTIONS
skip_e_nonce:
	if (dpp_test == DPP_TEST_NO_CONFIG_OBJ_CONF_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - Config Object");
		goto skip_config_obj;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	if (conf) {
		wpabuf_put_le16(clear, DPP_ATTR_CONFIG_OBJ);
		wpabuf_put_le16(clear, wpabuf_len(conf));
		wpabuf_put_buf(clear, conf);
	}
	if (auth->peer_version >= 2 && conf2) {
		wpabuf_put_le16(clear, DPP_ATTR_CONFIG_OBJ);
		wpabuf_put_le16(clear, wpabuf_len(conf2));
		wpabuf_put_buf(clear, conf2);
	} else if (conf2) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Second Config Object available, but peer does not support more than one");
	}
	if (env_data) {
		wpabuf_put_le16(clear, DPP_ATTR_ENVELOPED_DATA);
		wpabuf_put_le16(clear, wpabuf_len(env_data));
		wpabuf_put_buf(clear, env_data);
	}

	if (auth->peer_version >= 2 && auth->send_conn_status &&
	    netrole == DPP_NETROLE_STA && status == DPP_STATUS_OK) {
		wpa_printf(MSG_DEBUG, "DPP: sendConnStatus");
		wpabuf_put_le16(clear, DPP_ATTR_SEND_CONN_STATUS);
		wpabuf_put_le16(clear, 0);
	}

	if (status == DPP_STATUS_CSR_NEEDED && auth->conf_sta &&
	    auth->conf_sta->csrattrs) {
		auth->waiting_csr = true;
		wpa_printf(MSG_DEBUG, "DPP: CSR Attributes Request");
		wpabuf_put_le16(clear, DPP_ATTR_CSR_ATTR_REQ);
		wpabuf_put_le16(clear, os_strlen(auth->conf_sta->csrattrs));
		wpabuf_put_str(clear, auth->conf_sta->csrattrs);
	}

#ifdef CONFIG_TESTING_OPTIONS
skip_config_obj:
	if (dpp_test == DPP_TEST_NO_STATUS_CONF_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - Status");
		goto skip_status;
	}
	if (dpp_test == DPP_TEST_INVALID_STATUS_CONF_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - invalid Status");
		status = 255;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* DPP Status */
	dpp_build_attr_status(msg, status);

#ifdef CONFIG_TESTING_OPTIONS
skip_status:
#endif /* CONFIG_TESTING_OPTIONS */

	addr[0] = wpabuf_head(msg);
	len[0] = wpabuf_len(msg);
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD", addr[0], len[0]);

	wpabuf_put_le16(msg, DPP_ATTR_WRAPPED_DATA);
	wpabuf_put_le16(msg, wpabuf_len(clear) + AES_BLOCK_SIZE);
	wrapped = wpabuf_put(msg, wpabuf_len(clear) + AES_BLOCK_SIZE);

	wpa_hexdump_buf(MSG_DEBUG, "DPP: AES-SIV cleartext", clear);
	if (aes_siv_encrypt(auth->ke, auth->curve->hash_len,
			    wpabuf_head(clear), wpabuf_len(clear),
			    1, addr, len, wrapped) < 0)
		goto fail;
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV ciphertext",
		    wrapped, wpabuf_len(clear) + AES_BLOCK_SIZE);

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_AFTER_WRAPPED_DATA_CONF_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - attr after Wrapped Data");
		dpp_build_attr_status(msg, DPP_STATUS_OK);
	}
skip_wrapped_data:
#endif /* CONFIG_TESTING_OPTIONS */

	wpa_hexdump_buf(MSG_DEBUG,
			"DPP: Configuration Response attributes", msg);
out:
	wpabuf_clear_free(conf);
	wpabuf_clear_free(conf2);
	wpabuf_clear_free(env_data);
	wpabuf_clear_free(clear);

	return msg;
fail:
	wpabuf_free(msg);
	msg = NULL;
	goto out;
}


struct wpabuf *
dpp_conf_req_rx(struct dpp_authentication *auth, const u8 *attr_start,
		size_t attr_len)
{
	const u8 *wrapped_data, *e_nonce, *config_attr;
	u16 wrapped_data_len, e_nonce_len, config_attr_len;
	u8 *unwrapped = NULL;
	size_t unwrapped_len = 0;
	struct wpabuf *resp = NULL;
	struct json_token *root = NULL, *token;
	enum dpp_netrole netrole;
	struct wpabuf *cert_req = NULL;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_STOP_AT_CONF_REQ) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - stop at Config Request");
		return NULL;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	if (dpp_check_attrs(attr_start, attr_len) < 0) {
		dpp_auth_fail(auth, "Invalid attribute in config request");
		return NULL;
	}

	wrapped_data = dpp_get_attr(attr_start, attr_len, DPP_ATTR_WRAPPED_DATA,
				    &wrapped_data_len);
	if (!wrapped_data || wrapped_data_len < AES_BLOCK_SIZE) {
		dpp_auth_fail(auth,
			      "Missing or invalid required Wrapped Data attribute");
		return NULL;
	}

	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV ciphertext",
		    wrapped_data, wrapped_data_len);
	unwrapped_len = wrapped_data_len - AES_BLOCK_SIZE;
	unwrapped = os_malloc(unwrapped_len);
	if (!unwrapped)
		return NULL;
	if (aes_siv_decrypt(auth->ke, auth->curve->hash_len,
			    wrapped_data, wrapped_data_len,
			    0, NULL, NULL, unwrapped) < 0) {
		dpp_auth_fail(auth, "AES-SIV decryption failed");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV cleartext",
		    unwrapped, unwrapped_len);

	if (dpp_check_attrs(unwrapped, unwrapped_len) < 0) {
		dpp_auth_fail(auth, "Invalid attribute in unwrapped data");
		goto fail;
	}

	e_nonce = dpp_get_attr(unwrapped, unwrapped_len,
			       DPP_ATTR_ENROLLEE_NONCE,
			       &e_nonce_len);
	if (!e_nonce || e_nonce_len != auth->curve->nonce_len) {
		dpp_auth_fail(auth,
			      "Missing or invalid Enrollee Nonce attribute");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: Enrollee Nonce", e_nonce, e_nonce_len);
	os_memcpy(auth->e_nonce, e_nonce, e_nonce_len);

	config_attr = dpp_get_attr(unwrapped, unwrapped_len,
				   DPP_ATTR_CONFIG_ATTR_OBJ,
				   &config_attr_len);
	if (!config_attr) {
		dpp_auth_fail(auth,
			      "Missing or invalid Config Attributes attribute");
		goto fail;
	}
	wpa_hexdump_ascii(MSG_DEBUG, "DPP: Config Attributes",
			  config_attr, config_attr_len);

	root = json_parse((const char *) config_attr, config_attr_len);
	if (!root) {
		dpp_auth_fail(auth, "Could not parse Config Attributes");
		goto fail;
	}

	token = json_get_member(root, "name");
	if (!token || token->type != JSON_STRING) {
		dpp_auth_fail(auth, "No Config Attributes - name");
		goto fail;
	}
	wpa_printf(MSG_DEBUG, "DPP: Enrollee name = '%s'", token->string);

	token = json_get_member(root, "wi-fi_tech");
	if (!token || token->type != JSON_STRING) {
		dpp_auth_fail(auth, "No Config Attributes - wi-fi_tech");
		goto fail;
	}
	wpa_printf(MSG_DEBUG, "DPP: wi-fi_tech = '%s'", token->string);
	if (os_strcmp(token->string, "infra") != 0) {
		wpa_printf(MSG_DEBUG, "DPP: Unsupported wi-fi_tech '%s'",
			   token->string);
		dpp_auth_fail(auth, "Unsupported wi-fi_tech");
		goto fail;
	}

	token = json_get_member(root, "netRole");
	if (!token || token->type != JSON_STRING) {
		dpp_auth_fail(auth, "No Config Attributes - netRole");
		goto fail;
	}
	wpa_printf(MSG_DEBUG, "DPP: netRole = '%s'", token->string);
	if (os_strcmp(token->string, "sta") == 0) {
		netrole = DPP_NETROLE_STA;
	} else if (os_strcmp(token->string, "ap") == 0) {
		netrole = DPP_NETROLE_AP;
	} else if (os_strcmp(token->string, "configurator") == 0) {
		netrole = DPP_NETROLE_CONFIGURATOR;
	} else {
		wpa_printf(MSG_DEBUG, "DPP: Unsupported netRole '%s'",
			   token->string);
		dpp_auth_fail(auth, "Unsupported netRole");
		goto fail;
	}
	auth->e_netrole = netrole;

	token = json_get_member(root, "mudurl");
	if (token && token->type == JSON_STRING) {
		wpa_printf(MSG_DEBUG, "DPP: mudurl = '%s'", token->string);
		wpa_msg(auth->msg_ctx, MSG_INFO, DPP_EVENT_MUD_URL "%s",
			token->string);
	}

	token = json_get_member(root, "bandSupport");
	if (token && token->type == JSON_ARRAY) {
		int *opclass = NULL;
		char txt[200], *pos, *end;
		int i, res;

		wpa_printf(MSG_DEBUG, "DPP: bandSupport");
		token = token->child;
		while (token) {
			if (token->type != JSON_NUMBER) {
				wpa_printf(MSG_DEBUG,
					   "DPP: Invalid bandSupport array member type");
			} else {
				wpa_printf(MSG_DEBUG,
					   "DPP: Supported global operating class: %d",
					   token->number);
				int_array_add_unique(&opclass, token->number);
			}
			token = token->sibling;
		}

		txt[0] = '\0';
		pos = txt;
		end = txt + sizeof(txt);
		for (i = 0; opclass && opclass[i]; i++) {
			res = os_snprintf(pos, end - pos, "%s%d",
					  pos == txt ? "" : ",", opclass[i]);
			if (os_snprintf_error(end - pos, res)) {
				*pos = '\0';
				break;
			}
			pos += res;
		}
		os_free(opclass);
		wpa_msg(auth->msg_ctx, MSG_INFO, DPP_EVENT_BAND_SUPPORT "%s",
			txt);
	}

#ifdef CONFIG_DPP2
	cert_req = json_get_member_base64(root, "pkcs10");
	if (cert_req) {
		char *txt;
		int id;

		wpa_hexdump_buf(MSG_DEBUG, "DPP: CertificateRequest", cert_req);
		if (dpp_validate_csr(auth, cert_req) < 0) {
			wpa_printf(MSG_DEBUG, "DPP: CSR is not valid");
			auth->force_conf_resp_status = DPP_STATUS_CSR_BAD;
			goto cont;
		}

		if (auth->peer_bi) {
			id = auth->peer_bi->id;
		} else if (auth->tmp_peer_bi) {
			id = auth->tmp_peer_bi->id;
		} else {
			struct dpp_bootstrap_info *bi;

			bi = os_zalloc(sizeof(*bi));
			if (!bi)
				goto fail;
			bi->id = dpp_next_id(auth->global);
			dl_list_add(&auth->global->bootstrap, &bi->list);
			auth->tmp_peer_bi = bi;
			id = bi->id;
		}

		wpa_printf(MSG_DEBUG, "DPP: CSR is valid - forward to CA/RA");
		txt = base64_encode_no_lf(wpabuf_head(cert_req),
					  wpabuf_len(cert_req), NULL);
		if (!txt)
			goto fail;

		wpa_msg(auth->msg_ctx, MSG_INFO, DPP_EVENT_CSR "peer=%d csr=%s",
			id, txt);
		os_free(txt);
		auth->waiting_csr = false;
		auth->waiting_cert = true;
		goto fail;
	}
cont:
#endif /* CONFIG_DPP2 */

	resp = dpp_build_conf_resp(auth, e_nonce, e_nonce_len, netrole,
				   cert_req);

fail:
	wpabuf_free(cert_req);
	json_free(root);
	os_free(unwrapped);
	return resp;
}


static int dpp_parse_cred_legacy(struct dpp_config_obj *conf,
				 struct json_token *cred)
{
	struct json_token *pass, *psk_hex;

	wpa_printf(MSG_DEBUG, "DPP: Legacy akm=psk credential");

	pass = json_get_member(cred, "pass");
	psk_hex = json_get_member(cred, "psk_hex");

	if (pass && pass->type == JSON_STRING) {
		size_t len = os_strlen(pass->string);

		wpa_hexdump_ascii_key(MSG_DEBUG, "DPP: Legacy passphrase",
				      pass->string, len);
		if (len < 8 || len > 63)
			return -1;
		os_strlcpy(conf->passphrase, pass->string,
			   sizeof(conf->passphrase));
	} else if (psk_hex && psk_hex->type == JSON_STRING) {
		if (dpp_akm_sae(conf->akm) && !dpp_akm_psk(conf->akm)) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Unexpected psk_hex with akm=sae");
			return -1;
		}
		if (os_strlen(psk_hex->string) != PMK_LEN * 2 ||
		    hexstr2bin(psk_hex->string, conf->psk, PMK_LEN) < 0) {
			wpa_printf(MSG_DEBUG, "DPP: Invalid psk_hex encoding");
			return -1;
		}
		wpa_hexdump_key(MSG_DEBUG, "DPP: Legacy PSK",
				conf->psk, PMK_LEN);
		conf->psk_set = 1;
	} else {
		wpa_printf(MSG_DEBUG, "DPP: No pass or psk_hex strings found");
		return -1;
	}

	if (dpp_akm_sae(conf->akm) && !conf->passphrase[0]) {
		wpa_printf(MSG_DEBUG, "DPP: No pass for sae found");
		return -1;
	}

	return 0;
}


struct crypto_ec_key * dpp_parse_jwk(struct json_token *jwk,
				     const struct dpp_curve_params **key_curve)
{
	struct json_token *token;
	const struct dpp_curve_params *curve;
	struct wpabuf *x = NULL, *y = NULL;
	struct crypto_ec_key *key = NULL;

	token = json_get_member(jwk, "kty");
	if (!token || token->type != JSON_STRING) {
		wpa_printf(MSG_DEBUG, "DPP: No kty in JWK");
		goto fail;
	}
	if (os_strcmp(token->string, "EC") != 0) {
		wpa_printf(MSG_DEBUG, "DPP: Unexpected JWK kty '%s'",
			   token->string);
		goto fail;
	}

	token = json_get_member(jwk, "crv");
	if (!token || token->type != JSON_STRING) {
		wpa_printf(MSG_DEBUG, "DPP: No crv in JWK");
		goto fail;
	}
	curve = dpp_get_curve_jwk_crv(token->string);
	if (!curve) {
		wpa_printf(MSG_DEBUG, "DPP: Unsupported JWK crv '%s'",
			   token->string);
		goto fail;
	}

	x = json_get_member_base64url(jwk, "x");
	if (!x) {
		wpa_printf(MSG_DEBUG, "DPP: No x in JWK");
		goto fail;
	}
	wpa_hexdump_buf(MSG_DEBUG, "DPP: JWK x", x);
	if (wpabuf_len(x) != curve->prime_len) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Unexpected JWK x length %u (expected %u for curve %s)",
			   (unsigned int) wpabuf_len(x),
			   (unsigned int) curve->prime_len, curve->name);
		goto fail;
	}

	y = json_get_member_base64url(jwk, "y");
	if (!y) {
		wpa_printf(MSG_DEBUG, "DPP: No y in JWK");
		goto fail;
	}
	wpa_hexdump_buf(MSG_DEBUG, "DPP: JWK y", y);
	if (wpabuf_len(y) != curve->prime_len) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Unexpected JWK y length %u (expected %u for curve %s)",
			   (unsigned int) wpabuf_len(y),
			   (unsigned int) curve->prime_len, curve->name);
		goto fail;
	}

	key = crypto_ec_key_set_pub(curve->ike_group, wpabuf_head(x),
				    wpabuf_head(y), wpabuf_len(x));
	if (!key)
		goto fail;

	*key_curve = curve;

fail:
	wpabuf_free(x);
	wpabuf_free(y);

	return key;
}


int dpp_key_expired(const char *timestamp, os_time_t *expiry)
{
	struct os_time now;
	unsigned int year, month, day, hour, min, sec;
	os_time_t utime;
	const char *pos;

	/* ISO 8601 date and time:
	 * <date>T<time>
	 * YYYY-MM-DDTHH:MM:SSZ
	 * YYYY-MM-DDTHH:MM:SS+03:00
	 */
	if (os_strlen(timestamp) < 19) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Too short timestamp - assume expired key");
		return 1;
	}
	if (sscanf(timestamp, "%04u-%02u-%02uT%02u:%02u:%02u",
		   &year, &month, &day, &hour, &min, &sec) != 6) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Failed to parse expiration day - assume expired key");
		return 1;
	}

	if (os_mktime(year, month, day, hour, min, sec, &utime) < 0) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Invalid date/time information - assume expired key");
		return 1;
	}

	pos = timestamp + 19;
	if (*pos == 'Z' || *pos == '\0') {
		/* In UTC - no need to adjust */
	} else if (*pos == '-' || *pos == '+') {
		int items;

		/* Adjust local time to UTC */
		items = sscanf(pos + 1, "%02u:%02u", &hour, &min);
		if (items < 1) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Invalid time zone designator (%s) - assume expired key",
				   pos);
			return 1;
		}
		if (*pos == '-')
			utime += 3600 * hour;
		if (*pos == '+')
			utime -= 3600 * hour;
		if (items > 1) {
			if (*pos == '-')
				utime += 60 * min;
			if (*pos == '+')
				utime -= 60 * min;
		}
	} else {
		wpa_printf(MSG_DEBUG,
			   "DPP: Invalid time zone designator (%s) - assume expired key",
			   pos);
		return 1;
	}
	if (expiry)
		*expiry = utime;

	if (os_get_time(&now) < 0) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Cannot get current time - assume expired key");
		return 1;
	}

	if (now.sec > utime) {
		wpa_printf(MSG_DEBUG, "DPP: Key has expired (%lu < %lu)",
			   utime, now.sec);
		return 1;
	}

	return 0;
}


static int dpp_parse_connector(struct dpp_authentication *auth,
			       struct dpp_config_obj *conf,
			       const unsigned char *payload,
			       u16 payload_len)
{
	struct json_token *root, *groups, *netkey, *token;
	int ret = -1;
	struct crypto_ec_key *key = NULL;
	const struct dpp_curve_params *curve;
	unsigned int rules = 0;

	root = json_parse((const char *) payload, payload_len);
	if (!root) {
		wpa_printf(MSG_DEBUG, "DPP: JSON parsing of connector failed");
		goto fail;
	}

	groups = json_get_member(root, "groups");
	if (!groups || groups->type != JSON_ARRAY) {
		wpa_printf(MSG_DEBUG, "DPP: No groups array found");
		goto skip_groups;
	}
	for (token = groups->child; token; token = token->sibling) {
		struct json_token *id, *role;

		id = json_get_member(token, "groupId");
		if (!id || id->type != JSON_STRING) {
			wpa_printf(MSG_DEBUG, "DPP: Missing groupId string");
			goto fail;
		}

		role = json_get_member(token, "netRole");
		if (!role || role->type != JSON_STRING) {
			wpa_printf(MSG_DEBUG, "DPP: Missing netRole string");
			goto fail;
		}
		wpa_printf(MSG_DEBUG,
			   "DPP: connector group: groupId='%s' netRole='%s'",
			   id->string, role->string);
		rules++;
	}
skip_groups:

	if (!rules) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Connector includes no groups");
		goto fail;
	}

	token = json_get_member(root, "expiry");
	if (!token || token->type != JSON_STRING) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No expiry string found - connector does not expire");
	} else {
		wpa_printf(MSG_DEBUG, "DPP: expiry = %s", token->string);
		if (dpp_key_expired(token->string,
				    &auth->net_access_key_expiry)) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Connector (netAccessKey) has expired");
			goto fail;
		}
	}

	netkey = json_get_member(root, "netAccessKey");
	if (!netkey || netkey->type != JSON_OBJECT) {
		wpa_printf(MSG_DEBUG, "DPP: No netAccessKey object found");
		goto fail;
	}

	key = dpp_parse_jwk(netkey, &curve);
	if (!key)
		goto fail;
	dpp_debug_print_key("DPP: Received netAccessKey", key);

	if (crypto_ec_key_cmp(key, auth->own_protocol_key)) {
		wpa_printf(MSG_DEBUG,
			   "DPP: netAccessKey in connector does not match own protocol key");
#ifdef CONFIG_TESTING_OPTIONS
		if (auth->ignore_netaccesskey_mismatch) {
			wpa_printf(MSG_DEBUG,
				   "DPP: TESTING - skip netAccessKey mismatch");
		} else {
			goto fail;
		}
#else /* CONFIG_TESTING_OPTIONS */
		goto fail;
#endif /* CONFIG_TESTING_OPTIONS */
	}

	ret = 0;
fail:
	crypto_ec_key_deinit(key);
	json_free(root);
	return ret;
}


static void dpp_copy_csign(struct dpp_config_obj *conf,
			   struct crypto_ec_key *csign)
{
	struct wpabuf *c_sign_key;

	c_sign_key = crypto_ec_key_get_subject_public_key(csign);
	if (!c_sign_key)
		return;

	wpabuf_free(conf->c_sign_key);
	conf->c_sign_key = c_sign_key;
}


static void dpp_copy_ppkey(struct dpp_config_obj *conf,
			   struct crypto_ec_key *ppkey)
{
	struct wpabuf *pp_key;

	pp_key = crypto_ec_key_get_subject_public_key(ppkey);
	if (!pp_key)
		return;

	wpabuf_free(conf->pp_key);
	conf->pp_key = pp_key;
}


static void dpp_copy_netaccesskey(struct dpp_authentication *auth,
				  struct dpp_config_obj *conf)
{
	struct wpabuf *net_access_key;
	struct crypto_ec_key *own_key;

	own_key = auth->own_protocol_key;
#ifdef CONFIG_DPP2
	if (auth->reconfig_connector_key == DPP_CONFIG_REUSEKEY &&
	    auth->reconfig_old_protocol_key)
		own_key = auth->reconfig_old_protocol_key;
#endif /* CONFIG_DPP2 */

	net_access_key = crypto_ec_key_get_ecprivate_key(own_key, true);
	if (!net_access_key)
		return;

	wpabuf_free(auth->net_access_key);
	auth->net_access_key = net_access_key;
}


static int dpp_parse_cred_dpp(struct dpp_authentication *auth,
			      struct dpp_config_obj *conf,
			      struct json_token *cred)
{
	struct dpp_signed_connector_info info;
	struct json_token *token, *csign, *ppkey;
	int ret = -1;
	struct crypto_ec_key *csign_pub = NULL, *pp_pub = NULL;
	const struct dpp_curve_params *key_curve = NULL, *pp_curve = NULL;
	const char *signed_connector;

	os_memset(&info, 0, sizeof(info));

	if (dpp_akm_psk(conf->akm) || dpp_akm_sae(conf->akm)) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Legacy credential included in Connector credential");
		if (dpp_parse_cred_legacy(conf, cred) < 0)
			return -1;
	}

	wpa_printf(MSG_DEBUG, "DPP: Connector credential");

	csign = json_get_member(cred, "csign");
	if (!csign || csign->type != JSON_OBJECT) {
		wpa_printf(MSG_DEBUG, "DPP: No csign JWK in JSON");
		goto fail;
	}

	csign_pub = dpp_parse_jwk(csign, &key_curve);
	if (!csign_pub) {
		wpa_printf(MSG_DEBUG, "DPP: Failed to parse csign JWK");
		goto fail;
	}
	dpp_debug_print_key("DPP: Received C-sign-key", csign_pub);

	ppkey = json_get_member(cred, "ppKey");
	if (ppkey && ppkey->type == JSON_OBJECT) {
		pp_pub = dpp_parse_jwk(ppkey, &pp_curve);
		if (!pp_pub) {
			wpa_printf(MSG_DEBUG, "DPP: Failed to parse ppKey JWK");
			goto fail;
		}
		dpp_debug_print_key("DPP: Received ppKey", pp_pub);
		if (key_curve != pp_curve) {
			wpa_printf(MSG_DEBUG,
				   "DPP: C-sign-key and ppKey do not use the same curve");
			goto fail;
		}
	}

	token = json_get_member(cred, "signedConnector");
	if (!token || token->type != JSON_STRING) {
		wpa_printf(MSG_DEBUG, "DPP: No signedConnector string found");
		goto fail;
	}
	wpa_hexdump_ascii(MSG_DEBUG, "DPP: signedConnector",
			  token->string, os_strlen(token->string));
	signed_connector = token->string;

	if (os_strchr(signed_connector, '"') ||
	    os_strchr(signed_connector, '\n')) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Unexpected character in signedConnector");
		goto fail;
	}

	if (dpp_process_signed_connector(&info, csign_pub,
					 signed_connector) != DPP_STATUS_OK)
		goto fail;

	if (dpp_parse_connector(auth, conf,
				info.payload, info.payload_len) < 0) {
		wpa_printf(MSG_DEBUG, "DPP: Failed to parse connector");
		goto fail;
	}

	os_free(conf->connector);
	conf->connector = os_strdup(signed_connector);

	dpp_copy_csign(conf, csign_pub);
	if (pp_pub)
		dpp_copy_ppkey(conf, pp_pub);
	if (dpp_akm_dpp(conf->akm) || auth->peer_version >= 2)
		dpp_copy_netaccesskey(auth, conf);

	ret = 0;
fail:
	crypto_ec_key_deinit(csign_pub);
	crypto_ec_key_deinit(pp_pub);
	os_free(info.payload);
	return ret;
}


#ifdef CONFIG_DPP2
static int dpp_parse_cred_dot1x(struct dpp_authentication *auth,
				struct dpp_config_obj *conf,
				struct json_token *cred)
{
	struct json_token *ent, *name;

	ent = json_get_member(cred, "entCreds");
	if (!ent || ent->type != JSON_OBJECT) {
		dpp_auth_fail(auth, "No entCreds in JSON");
		return -1;
	}

	conf->certbag = json_get_member_base64(ent, "certBag");
	if (!conf->certbag) {
		dpp_auth_fail(auth, "No certBag in JSON");
		return -1;
	}
	wpa_hexdump_buf(MSG_MSGDUMP, "DPP: Received certBag", conf->certbag);
	conf->certs = crypto_pkcs7_get_certificates(conf->certbag);
	if (!conf->certs) {
		dpp_auth_fail(auth, "No certificates in certBag");
		return -1;
	}

	conf->cacert = json_get_member_base64(ent, "caCert");
	if (conf->cacert)
		wpa_hexdump_buf(MSG_MSGDUMP, "DPP: Received caCert",
				conf->cacert);

	name = json_get_member(ent, "trustedEapServerName");
	if (name &&
	    (name->type != JSON_STRING ||
	     has_ctrl_char((const u8 *) name->string,
			   os_strlen(name->string)))) {
		dpp_auth_fail(auth,
			      "Invalid trustedEapServerName type in JSON");
		return -1;
	}
	if (name && name->string) {
		wpa_printf(MSG_DEBUG, "DPP: Received trustedEapServerName: %s",
			   name->string);
		conf->server_name = os_strdup(name->string);
		if (!conf->server_name)
			return -1;
	}

	return 0;
}
#endif /* CONFIG_DPP2 */


const char * dpp_akm_str(enum dpp_akm akm)
{
	switch (akm) {
	case DPP_AKM_DPP:
		return "dpp";
	case DPP_AKM_PSK:
		return "psk";
	case DPP_AKM_SAE:
		return "sae";
	case DPP_AKM_PSK_SAE:
		return "psk+sae";
	case DPP_AKM_SAE_DPP:
		return "dpp+sae";
	case DPP_AKM_PSK_SAE_DPP:
		return "dpp+psk+sae";
	case DPP_AKM_DOT1X:
		return "dot1x";
	default:
		return "??";
	}
}


const char * dpp_akm_selector_str(enum dpp_akm akm)
{
	switch (akm) {
	case DPP_AKM_DPP:
		return "506F9A02";
	case DPP_AKM_PSK:
		return "000FAC02+000FAC06";
	case DPP_AKM_SAE:
		return "000FAC08";
	case DPP_AKM_PSK_SAE:
		return "000FAC02+000FAC06+000FAC08";
	case DPP_AKM_SAE_DPP:
		return "506F9A02+000FAC08";
	case DPP_AKM_PSK_SAE_DPP:
		return "506F9A02+000FAC08+000FAC02+000FAC06";
	case DPP_AKM_DOT1X:
		return "000FAC01+000FAC05";
	default:
		return "??";
	}
}


static enum dpp_akm dpp_akm_from_str(const char *akm)
{
	const char *pos;
	int dpp = 0, psk = 0, sae = 0, dot1x = 0;

	if (os_strcmp(akm, "psk") == 0)
		return DPP_AKM_PSK;
	if (os_strcmp(akm, "sae") == 0)
		return DPP_AKM_SAE;
	if (os_strcmp(akm, "psk+sae") == 0)
		return DPP_AKM_PSK_SAE;
	if (os_strcmp(akm, "dpp") == 0)
		return DPP_AKM_DPP;
	if (os_strcmp(akm, "dpp+sae") == 0)
		return DPP_AKM_SAE_DPP;
	if (os_strcmp(akm, "dpp+psk+sae") == 0)
		return DPP_AKM_PSK_SAE_DPP;
	if (os_strcmp(akm, "dot1x") == 0)
		return DPP_AKM_DOT1X;

	pos = akm;
	while (*pos) {
		if (os_strlen(pos) < 8)
			break;
		if (os_strncasecmp(pos, "506F9A02", 8) == 0)
			dpp = 1;
		else if (os_strncasecmp(pos, "000FAC02", 8) == 0)
			psk = 1;
		else if (os_strncasecmp(pos, "000FAC06", 8) == 0)
			psk = 1;
		else if (os_strncasecmp(pos, "000FAC08", 8) == 0)
			sae = 1;
		else if (os_strncasecmp(pos, "000FAC01", 8) == 0)
			dot1x = 1;
		else if (os_strncasecmp(pos, "000FAC05", 8) == 0)
			dot1x = 1;
		pos += 8;
		if (*pos != '+')
			break;
		pos++;
	}

	if (dpp && psk && sae)
		return DPP_AKM_PSK_SAE_DPP;
	if (dpp && sae)
		return DPP_AKM_SAE_DPP;
	if (dpp)
		return DPP_AKM_DPP;
	if (psk && sae)
		return DPP_AKM_PSK_SAE;
	if (sae)
		return DPP_AKM_SAE;
	if (psk)
		return DPP_AKM_PSK;
	if (dot1x)
		return DPP_AKM_DOT1X;

	return DPP_AKM_UNKNOWN;
}


static int dpp_parse_conf_obj(struct dpp_authentication *auth,
			      const u8 *conf_obj, u16 conf_obj_len)
{
	int ret = -1;
	struct json_token *root, *token, *discovery, *cred;
	struct dpp_config_obj *conf;
	struct wpabuf *ssid64 = NULL;
	int legacy;

	root = json_parse((const char *) conf_obj, conf_obj_len);
	if (!root)
		return -1;
	if (root->type != JSON_OBJECT) {
		dpp_auth_fail(auth, "JSON root is not an object");
		goto fail;
	}

	token = json_get_member(root, "wi-fi_tech");
	if (!token || token->type != JSON_STRING) {
		dpp_auth_fail(auth, "No wi-fi_tech string value found");
		goto fail;
	}
	if (os_strcmp(token->string, "infra") != 0) {
		wpa_printf(MSG_DEBUG, "DPP: Unsupported wi-fi_tech value: '%s'",
			   token->string);
		dpp_auth_fail(auth, "Unsupported wi-fi_tech value");
		goto fail;
	}

	discovery = json_get_member(root, "discovery");
	if (!discovery || discovery->type != JSON_OBJECT) {
		dpp_auth_fail(auth, "No discovery object in JSON");
		goto fail;
	}

	ssid64 = json_get_member_base64url(discovery, "ssid64");
	if (ssid64) {
		wpa_hexdump_ascii(MSG_DEBUG, "DPP: discovery::ssid64",
				  wpabuf_head(ssid64), wpabuf_len(ssid64));
		if (wpabuf_len(ssid64) > SSID_MAX_LEN) {
			dpp_auth_fail(auth, "Too long discovery::ssid64 value");
			goto fail;
		}
	} else {
		token = json_get_member(discovery, "ssid");
		if (!token || token->type != JSON_STRING) {
			dpp_auth_fail(auth,
				      "No discovery::ssid string value found");
			goto fail;
		}
		wpa_hexdump_ascii(MSG_DEBUG, "DPP: discovery::ssid",
				  token->string, os_strlen(token->string));
		if (os_strlen(token->string) > SSID_MAX_LEN) {
			dpp_auth_fail(auth,
				      "Too long discovery::ssid string value");
			goto fail;
		}
	}

	if (auth->num_conf_obj == DPP_MAX_CONF_OBJ) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No room for this many Config Objects - ignore this one");
		ret = 0;
		goto fail;
	}
	conf = &auth->conf_obj[auth->num_conf_obj++];

	if (ssid64) {
		conf->ssid_len = wpabuf_len(ssid64);
		os_memcpy(conf->ssid, wpabuf_head(ssid64), conf->ssid_len);
	} else {
		conf->ssid_len = os_strlen(token->string);
		os_memcpy(conf->ssid, token->string, conf->ssid_len);
	}

	token = json_get_member(discovery, "ssid_charset");
	if (token && token->type == JSON_NUMBER) {
		conf->ssid_charset = token->number;
		wpa_printf(MSG_DEBUG, "DPP: ssid_charset=%d",
			   conf->ssid_charset);
	}

	cred = json_get_member(root, "cred");
	if (!cred || cred->type != JSON_OBJECT) {
		dpp_auth_fail(auth, "No cred object in JSON");
		goto fail;
	}

	token = json_get_member(cred, "akm");
	if (!token || token->type != JSON_STRING) {
		dpp_auth_fail(auth, "No cred::akm string value found");
		goto fail;
	}
	conf->akm = dpp_akm_from_str(token->string);

	legacy = dpp_akm_legacy(conf->akm);
	if (legacy && auth->peer_version >= 2) {
		struct json_token *csign, *s_conn;

		csign = json_get_member(cred, "csign");
		s_conn = json_get_member(cred, "signedConnector");
		if (csign && csign->type == JSON_OBJECT &&
		    s_conn && s_conn->type == JSON_STRING)
			legacy = 0;
	}
	if (legacy) {
		if (dpp_parse_cred_legacy(conf, cred) < 0)
			goto fail;
	} else if (dpp_akm_dpp(conf->akm) ||
		   (auth->peer_version >= 2 && dpp_akm_legacy(conf->akm))) {
		if (dpp_parse_cred_dpp(auth, conf, cred) < 0)
			goto fail;
#ifdef CONFIG_DPP2
	} else if (conf->akm == DPP_AKM_DOT1X) {
		if (dpp_parse_cred_dot1x(auth, conf, cred) < 0 ||
		    dpp_parse_cred_dpp(auth, conf, cred) < 0)
			goto fail;
#endif /* CONFIG_DPP2 */
	} else {
		wpa_printf(MSG_DEBUG, "DPP: Unsupported akm: %s",
			   token->string);
		dpp_auth_fail(auth, "Unsupported akm");
		goto fail;
	}

	wpa_printf(MSG_DEBUG, "DPP: JSON parsing completed successfully");
	ret = 0;
fail:
	wpabuf_free(ssid64);
	json_free(root);
	return ret;
}


#ifdef CONFIG_DPP2
static u8 * dpp_get_csr_attrs(const u8 *attrs, size_t attrs_len, size_t *len)
{
	const u8 *b64;
	u16 b64_len;

	b64 = dpp_get_attr(attrs, attrs_len, DPP_ATTR_CSR_ATTR_REQ, &b64_len);
	if (!b64)
		return NULL;
	return base64_decode((const char *) b64, b64_len, len);
}
#endif /* CONFIG_DPP2 */


int dpp_conf_resp_rx(struct dpp_authentication *auth,
		     const struct wpabuf *resp)
{
	const u8 *wrapped_data, *e_nonce, *status, *conf_obj;
	u16 wrapped_data_len, e_nonce_len, status_len, conf_obj_len;
	const u8 *env_data;
	u16 env_data_len;
	const u8 *addr[1];
	size_t len[1];
	u8 *unwrapped = NULL;
	size_t unwrapped_len = 0;
	int ret = -1;

	auth->conf_resp_status = 255;

	if (dpp_check_attrs(wpabuf_head(resp), wpabuf_len(resp)) < 0) {
		dpp_auth_fail(auth, "Invalid attribute in config response");
		return -1;
	}

	wrapped_data = dpp_get_attr(wpabuf_head(resp), wpabuf_len(resp),
				    DPP_ATTR_WRAPPED_DATA,
				    &wrapped_data_len);
	if (!wrapped_data || wrapped_data_len < AES_BLOCK_SIZE) {
		dpp_auth_fail(auth,
			      "Missing or invalid required Wrapped Data attribute");
		return -1;
	}

	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV ciphertext",
		    wrapped_data, wrapped_data_len);
	unwrapped_len = wrapped_data_len - AES_BLOCK_SIZE;
	unwrapped = os_malloc(unwrapped_len);
	if (!unwrapped)
		return -1;

	addr[0] = wpabuf_head(resp);
	len[0] = wrapped_data - 4 - (const u8 *) wpabuf_head(resp);
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD", addr[0], len[0]);

	if (aes_siv_decrypt(auth->ke, auth->curve->hash_len,
			    wrapped_data, wrapped_data_len,
			    1, addr, len, unwrapped) < 0) {
		dpp_auth_fail(auth, "AES-SIV decryption failed");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV cleartext",
		    unwrapped, unwrapped_len);

	if (dpp_check_attrs(unwrapped, unwrapped_len) < 0) {
		dpp_auth_fail(auth, "Invalid attribute in unwrapped data");
		goto fail;
	}

	e_nonce = dpp_get_attr(unwrapped, unwrapped_len,
			       DPP_ATTR_ENROLLEE_NONCE,
			       &e_nonce_len);
	if (!e_nonce || e_nonce_len != auth->curve->nonce_len) {
		dpp_auth_fail(auth,
			      "Missing or invalid Enrollee Nonce attribute");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: Enrollee Nonce", e_nonce, e_nonce_len);
	if (os_memcmp(e_nonce, auth->e_nonce, e_nonce_len) != 0) {
		dpp_auth_fail(auth, "Enrollee Nonce mismatch");
		goto fail;
	}

	status = dpp_get_attr(wpabuf_head(resp), wpabuf_len(resp),
			      DPP_ATTR_STATUS, &status_len);
	if (!status || status_len < 1) {
		dpp_auth_fail(auth,
			      "Missing or invalid required DPP Status attribute");
		goto fail;
	}
	auth->conf_resp_status = status[0];
	wpa_printf(MSG_DEBUG, "DPP: Status %u", status[0]);
#ifdef CONFIG_DPP2
	if (status[0] == DPP_STATUS_CSR_NEEDED) {
		u8 *csrattrs;
		size_t csrattrs_len;

		wpa_printf(MSG_DEBUG, "DPP: Configurator requested CSR");

		csrattrs = dpp_get_csr_attrs(unwrapped, unwrapped_len,
					     &csrattrs_len);
		if (!csrattrs) {
			dpp_auth_fail(auth,
				      "Missing or invalid CSR Attributes Request attribute");
			goto fail;
		}
		wpa_hexdump(MSG_DEBUG, "DPP: CsrAttrs", csrattrs, csrattrs_len);
		os_free(auth->csrattrs);
		auth->csrattrs = csrattrs;
		auth->csrattrs_len = csrattrs_len;
		ret = -2;
		goto fail;
	}
#endif /* CONFIG_DPP2 */
	if (status[0] != DPP_STATUS_OK) {
		dpp_auth_fail(auth, "Configurator rejected configuration");
		goto fail;
	}

	env_data = dpp_get_attr(unwrapped, unwrapped_len,
				DPP_ATTR_ENVELOPED_DATA, &env_data_len);
#ifdef CONFIG_DPP2
	if (env_data &&
	    dpp_conf_resp_env_data(auth, env_data, env_data_len) < 0)
		goto fail;
#endif /* CONFIG_DPP2 */

	conf_obj = dpp_get_attr(unwrapped, unwrapped_len, DPP_ATTR_CONFIG_OBJ,
				&conf_obj_len);
	if (!conf_obj && !env_data) {
		dpp_auth_fail(auth,
			      "Missing required Configuration Object attribute");
		goto fail;
	}
	while (conf_obj) {
		wpa_hexdump_ascii(MSG_DEBUG, "DPP: configurationObject JSON",
				  conf_obj, conf_obj_len);
		if (dpp_parse_conf_obj(auth, conf_obj, conf_obj_len) < 0)
			goto fail;
		conf_obj = dpp_get_attr_next(conf_obj, unwrapped, unwrapped_len,
					     DPP_ATTR_CONFIG_OBJ,
					     &conf_obj_len);
	}

#ifdef CONFIG_DPP2
	status = dpp_get_attr(unwrapped, unwrapped_len,
			      DPP_ATTR_SEND_CONN_STATUS, &status_len);
	if (status) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Configurator requested connection status result");
		auth->conn_status_requested = 1;
	}
#endif /* CONFIG_DPP2 */

	ret = 0;

fail:
	os_free(unwrapped);
	return ret;
}


#ifdef CONFIG_DPP2

enum dpp_status_error dpp_conf_result_rx(struct dpp_authentication *auth,
					 const u8 *hdr,
					 const u8 *attr_start, size_t attr_len)
{
	const u8 *wrapped_data, *status, *e_nonce;
	u16 wrapped_data_len, status_len, e_nonce_len;
	const u8 *addr[2];
	size_t len[2];
	u8 *unwrapped = NULL;
	size_t unwrapped_len = 0;
	enum dpp_status_error ret = 256;

	wrapped_data = dpp_get_attr(attr_start, attr_len, DPP_ATTR_WRAPPED_DATA,
				    &wrapped_data_len);
	if (!wrapped_data || wrapped_data_len < AES_BLOCK_SIZE) {
		dpp_auth_fail(auth,
			      "Missing or invalid required Wrapped Data attribute");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: Wrapped data",
		    wrapped_data, wrapped_data_len);

	attr_len = wrapped_data - 4 - attr_start;

	addr[0] = hdr;
	len[0] = DPP_HDR_LEN;
	addr[1] = attr_start;
	len[1] = attr_len;
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[0]", addr[0], len[0]);
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[1]", addr[1], len[1]);
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV ciphertext",
		    wrapped_data, wrapped_data_len);
	unwrapped_len = wrapped_data_len - AES_BLOCK_SIZE;
	unwrapped = os_malloc(unwrapped_len);
	if (!unwrapped)
		goto fail;
	if (aes_siv_decrypt(auth->ke, auth->curve->hash_len,
			    wrapped_data, wrapped_data_len,
			    2, addr, len, unwrapped) < 0) {
		dpp_auth_fail(auth, "AES-SIV decryption failed");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV cleartext",
		    unwrapped, unwrapped_len);

	if (dpp_check_attrs(unwrapped, unwrapped_len) < 0) {
		dpp_auth_fail(auth, "Invalid attribute in unwrapped data");
		goto fail;
	}

	e_nonce = dpp_get_attr(unwrapped, unwrapped_len,
			       DPP_ATTR_ENROLLEE_NONCE,
			       &e_nonce_len);
	if (!e_nonce || e_nonce_len != auth->curve->nonce_len) {
		dpp_auth_fail(auth,
			      "Missing or invalid Enrollee Nonce attribute");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: Enrollee Nonce", e_nonce, e_nonce_len);
	if (os_memcmp(e_nonce, auth->e_nonce, e_nonce_len) != 0) {
		dpp_auth_fail(auth, "Enrollee Nonce mismatch");
		wpa_hexdump(MSG_DEBUG, "DPP: Expected Enrollee Nonce",
			    auth->e_nonce, e_nonce_len);
		goto fail;
	}

	status = dpp_get_attr(unwrapped, unwrapped_len, DPP_ATTR_STATUS,
			      &status_len);
	if (!status || status_len < 1) {
		dpp_auth_fail(auth,
			      "Missing or invalid required DPP Status attribute");
		goto fail;
	}
	wpa_printf(MSG_DEBUG, "DPP: Status %u", status[0]);
	ret = status[0];

fail:
	bin_clear_free(unwrapped, unwrapped_len);
	return ret;
}


struct wpabuf * dpp_build_conf_result(struct dpp_authentication *auth,
				      enum dpp_status_error status)
{
	struct wpabuf *msg, *clear;
	size_t nonce_len, clear_len, attr_len;
	const u8 *addr[2];
	size_t len[2];
	u8 *wrapped;

	nonce_len = auth->curve->nonce_len;
	clear_len = 5 + 4 + nonce_len;
	attr_len = 4 + clear_len + AES_BLOCK_SIZE;
	clear = wpabuf_alloc(clear_len);
	msg = dpp_alloc_msg(DPP_PA_CONFIGURATION_RESULT, attr_len);
	if (!clear || !msg)
		goto fail;

	/* DPP Status */
	dpp_build_attr_status(clear, status);

	/* E-nonce */
	wpabuf_put_le16(clear, DPP_ATTR_ENROLLEE_NONCE);
	wpabuf_put_le16(clear, nonce_len);
	wpabuf_put_data(clear, auth->e_nonce, nonce_len);

	/* OUI, OUI type, Crypto Suite, DPP frame type */
	addr[0] = wpabuf_head_u8(msg) + 2;
	len[0] = 3 + 1 + 1 + 1;
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[0]", addr[0], len[0]);

	/* Attributes before Wrapped Data (none) */
	addr[1] = wpabuf_put(msg, 0);
	len[1] = 0;
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[1]", addr[1], len[1]);

	/* Wrapped Data */
	wpabuf_put_le16(msg, DPP_ATTR_WRAPPED_DATA);
	wpabuf_put_le16(msg, wpabuf_len(clear) + AES_BLOCK_SIZE);
	wrapped = wpabuf_put(msg, wpabuf_len(clear) + AES_BLOCK_SIZE);

	wpa_hexdump_buf(MSG_DEBUG, "DPP: AES-SIV cleartext", clear);
	if (aes_siv_encrypt(auth->ke, auth->curve->hash_len,
			    wpabuf_head(clear), wpabuf_len(clear),
			    2, addr, len, wrapped) < 0)
		goto fail;

	wpa_hexdump_buf(MSG_DEBUG, "DPP: Configuration Result attributes", msg);
	wpabuf_free(clear);
	return msg;
fail:
	wpabuf_free(clear);
	wpabuf_free(msg);
	return NULL;
}


static int valid_channel_list(const char *val)
{
	while (*val) {
		if (!((*val >= '0' && *val <= '9') ||
		      *val == '/' || *val == ','))
			return 0;
		val++;
	}

	return 1;
}


enum dpp_status_error dpp_conn_status_result_rx(struct dpp_authentication *auth,
						const u8 *hdr,
						const u8 *attr_start,
						size_t attr_len,
						u8 *ssid, size_t *ssid_len,
						char **channel_list)
{
	const u8 *wrapped_data, *status, *e_nonce;
	u16 wrapped_data_len, status_len, e_nonce_len;
	const u8 *addr[2];
	size_t len[2];
	u8 *unwrapped = NULL;
	size_t unwrapped_len = 0;
	enum dpp_status_error ret = 256;
	struct json_token *root = NULL, *token;
	struct wpabuf *ssid64;

	*ssid_len = 0;
	*channel_list = NULL;

	wrapped_data = dpp_get_attr(attr_start, attr_len, DPP_ATTR_WRAPPED_DATA,
				    &wrapped_data_len);
	if (!wrapped_data || wrapped_data_len < AES_BLOCK_SIZE) {
		dpp_auth_fail(auth,
			      "Missing or invalid required Wrapped Data attribute");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: Wrapped data",
		    wrapped_data, wrapped_data_len);

	attr_len = wrapped_data - 4 - attr_start;

	addr[0] = hdr;
	len[0] = DPP_HDR_LEN;
	addr[1] = attr_start;
	len[1] = attr_len;
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[0]", addr[0], len[0]);
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[1]", addr[1], len[1]);
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV ciphertext",
		    wrapped_data, wrapped_data_len);
	unwrapped_len = wrapped_data_len - AES_BLOCK_SIZE;
	unwrapped = os_malloc(unwrapped_len);
	if (!unwrapped)
		goto fail;
	if (aes_siv_decrypt(auth->ke, auth->curve->hash_len,
			    wrapped_data, wrapped_data_len,
			    2, addr, len, unwrapped) < 0) {
		dpp_auth_fail(auth, "AES-SIV decryption failed");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV cleartext",
		    unwrapped, unwrapped_len);

	if (dpp_check_attrs(unwrapped, unwrapped_len) < 0) {
		dpp_auth_fail(auth, "Invalid attribute in unwrapped data");
		goto fail;
	}

	e_nonce = dpp_get_attr(unwrapped, unwrapped_len,
			       DPP_ATTR_ENROLLEE_NONCE,
			       &e_nonce_len);
	if (!e_nonce || e_nonce_len != auth->curve->nonce_len) {
		dpp_auth_fail(auth,
			      "Missing or invalid Enrollee Nonce attribute");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: Enrollee Nonce", e_nonce, e_nonce_len);
	if (os_memcmp(e_nonce, auth->e_nonce, e_nonce_len) != 0) {
		dpp_auth_fail(auth, "Enrollee Nonce mismatch");
		wpa_hexdump(MSG_DEBUG, "DPP: Expected Enrollee Nonce",
			    auth->e_nonce, e_nonce_len);
		goto fail;
	}

	status = dpp_get_attr(unwrapped, unwrapped_len, DPP_ATTR_CONN_STATUS,
			      &status_len);
	if (!status) {
		dpp_auth_fail(auth,
			      "Missing required DPP Connection Status attribute");
		goto fail;
	}
	wpa_hexdump_ascii(MSG_DEBUG, "DPP: connStatus JSON",
			  status, status_len);

	root = json_parse((const char *) status, status_len);
	if (!root) {
		dpp_auth_fail(auth, "Could not parse connStatus");
		goto fail;
	}

	ssid64 = json_get_member_base64url(root, "ssid64");
	if (ssid64 && wpabuf_len(ssid64) <= SSID_MAX_LEN) {
		*ssid_len = wpabuf_len(ssid64);
		os_memcpy(ssid, wpabuf_head(ssid64), *ssid_len);
	}
	wpabuf_free(ssid64);

	token = json_get_member(root, "channelList");
	if (token && token->type == JSON_STRING &&
	    valid_channel_list(token->string))
		*channel_list = os_strdup(token->string);

	token = json_get_member(root, "result");
	if (!token || token->type != JSON_NUMBER) {
		dpp_auth_fail(auth, "No connStatus - result");
		goto fail;
	}
	wpa_printf(MSG_DEBUG, "DPP: result %d", token->number);
	ret = token->number;

fail:
	json_free(root);
	bin_clear_free(unwrapped, unwrapped_len);
	return ret;
}


struct wpabuf * dpp_build_conn_status(enum dpp_status_error result,
				      const u8 *ssid, size_t ssid_len,
				      const char *channel_list)
{
	struct wpabuf *json;

	json = wpabuf_alloc(1000);
	if (!json)
		return NULL;
	json_start_object(json, NULL);
	json_add_int(json, "result", result);
	if (ssid) {
		json_value_sep(json);
		if (json_add_base64url(json, "ssid64", ssid, ssid_len) < 0) {
			wpabuf_free(json);
			return NULL;
		}
	}
	if (channel_list) {
		json_value_sep(json);
		json_add_string(json, "channelList", channel_list);
	}
	json_end_object(json);
	wpa_hexdump_ascii(MSG_DEBUG, "DPP: connStatus JSON",
			  wpabuf_head(json), wpabuf_len(json));

	return json;
}


struct wpabuf * dpp_build_conn_status_result(struct dpp_authentication *auth,
					     enum dpp_status_error result,
					     const u8 *ssid, size_t ssid_len,
					     const char *channel_list)
{
	struct wpabuf *msg = NULL, *clear = NULL, *json;
	size_t nonce_len, clear_len, attr_len;
	const u8 *addr[2];
	size_t len[2];
	u8 *wrapped;

	json = dpp_build_conn_status(result, ssid, ssid_len, channel_list);
	if (!json)
		return NULL;

	nonce_len = auth->curve->nonce_len;
	clear_len = 5 + 4 + nonce_len + 4 + wpabuf_len(json);
	attr_len = 4 + clear_len + AES_BLOCK_SIZE;
	clear = wpabuf_alloc(clear_len);
	msg = dpp_alloc_msg(DPP_PA_CONNECTION_STATUS_RESULT, attr_len);
	if (!clear || !msg)
		goto fail;

	/* E-nonce */
	wpabuf_put_le16(clear, DPP_ATTR_ENROLLEE_NONCE);
	wpabuf_put_le16(clear, nonce_len);
	wpabuf_put_data(clear, auth->e_nonce, nonce_len);

	/* DPP Connection Status */
	wpabuf_put_le16(clear, DPP_ATTR_CONN_STATUS);
	wpabuf_put_le16(clear, wpabuf_len(json));
	wpabuf_put_buf(clear, json);

	/* OUI, OUI type, Crypto Suite, DPP frame type */
	addr[0] = wpabuf_head_u8(msg) + 2;
	len[0] = 3 + 1 + 1 + 1;
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[0]", addr[0], len[0]);

	/* Attributes before Wrapped Data (none) */
	addr[1] = wpabuf_put(msg, 0);
	len[1] = 0;
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[1]", addr[1], len[1]);

	/* Wrapped Data */
	wpabuf_put_le16(msg, DPP_ATTR_WRAPPED_DATA);
	wpabuf_put_le16(msg, wpabuf_len(clear) + AES_BLOCK_SIZE);
	wrapped = wpabuf_put(msg, wpabuf_len(clear) + AES_BLOCK_SIZE);

	wpa_hexdump_buf(MSG_DEBUG, "DPP: AES-SIV cleartext", clear);
	if (aes_siv_encrypt(auth->ke, auth->curve->hash_len,
			    wpabuf_head(clear), wpabuf_len(clear),
			    2, addr, len, wrapped) < 0)
		goto fail;

	wpa_hexdump_buf(MSG_DEBUG, "DPP: Connection Status Result attributes",
			msg);
	wpabuf_free(json);
	wpabuf_free(clear);
	return msg;
fail:
	wpabuf_free(json);
	wpabuf_free(clear);
	wpabuf_free(msg);
	return NULL;
}

#endif /* CONFIG_DPP2 */


void dpp_configurator_free(struct dpp_configurator *conf)
{
	if (!conf)
		return;
	crypto_ec_key_deinit(conf->csign);
	os_free(conf->kid);
	os_free(conf->connector);
	crypto_ec_key_deinit(conf->connector_key);
	crypto_ec_key_deinit(conf->pp_key);
	os_free(conf);
}


int dpp_configurator_get_key(const struct dpp_configurator *conf, char *buf,
			     size_t buflen)
{
	struct wpabuf *key;
	int ret = -1;

	if (!conf->csign)
		return -1;

	key = crypto_ec_key_get_ecprivate_key(conf->csign, true);
	if (!key)
		return -1;

	ret = wpa_snprintf_hex(buf, buflen, wpabuf_head(key), wpabuf_len(key));

	wpabuf_clear_free(key);
	return ret;
}


static int dpp_configurator_gen_kid(struct dpp_configurator *conf)
{
	struct wpabuf *csign_pub = NULL;
	const u8 *addr[1];
	size_t len[1];
	int res;

	csign_pub = crypto_ec_key_get_pubkey_point(conf->csign, 1);
	if (!csign_pub) {
		wpa_printf(MSG_INFO, "DPP: Failed to extract C-sign-key");
		return -1;
	}

	/* kid = SHA256(ANSI X9.63 uncompressed C-sign-key) */
	addr[0] = wpabuf_head(csign_pub);
	len[0] = wpabuf_len(csign_pub);
	res = sha256_vector(1, addr, len, conf->kid_hash);
	wpabuf_free(csign_pub);
	if (res < 0) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Failed to derive kid for C-sign-key");
		return -1;
	}

	conf->kid = base64_url_encode(conf->kid_hash, sizeof(conf->kid_hash),
				      NULL);
	return conf->kid ? 0 : -1;
}


static struct dpp_configurator *
dpp_keygen_configurator(const char *curve, const u8 *privkey,
			size_t privkey_len, const u8 *pp_key, size_t pp_key_len)
{
	struct dpp_configurator *conf;

	conf = os_zalloc(sizeof(*conf));
	if (!conf)
		return NULL;

	conf->curve = dpp_get_curve_name(curve);
	if (!conf->curve) {
		wpa_printf(MSG_INFO, "DPP: Unsupported curve: %s", curve);
		os_free(conf);
		return NULL;
	}

	if (privkey)
		conf->csign = dpp_set_keypair(&conf->curve, privkey,
					      privkey_len);
	else
		conf->csign = dpp_gen_keypair(conf->curve);
	if (pp_key)
		conf->pp_key = dpp_set_keypair(&conf->curve, pp_key,
					       pp_key_len);
	else
		conf->pp_key = dpp_gen_keypair(conf->curve);
	if (!conf->csign || !conf->pp_key)
		goto fail;
	conf->own = 1;

	if (dpp_configurator_gen_kid(conf) < 0)
		goto fail;
	return conf;
fail:
	dpp_configurator_free(conf);
	return NULL;
}


int dpp_configurator_own_config(struct dpp_authentication *auth,
				const char *curve, int ap)
{
	struct wpabuf *conf_obj;
	int ret = -1;

	if (!auth->conf) {
		wpa_printf(MSG_DEBUG, "DPP: No configurator specified");
		return -1;
	}

	auth->curve = dpp_get_curve_name(curve);
	if (!auth->curve) {
		wpa_printf(MSG_INFO, "DPP: Unsupported curve: %s", curve);
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "DPP: Building own configuration/connector with curve %s",
		   auth->curve->name);

	auth->own_protocol_key = dpp_gen_keypair(auth->curve);
	if (!auth->own_protocol_key)
		return -1;
	dpp_copy_netaccesskey(auth, &auth->conf_obj[0]);
	auth->peer_protocol_key = auth->own_protocol_key;
	dpp_copy_csign(&auth->conf_obj[0], auth->conf->csign);

	conf_obj = dpp_build_conf_obj(auth, ap, 0, NULL);
	if (!conf_obj) {
		wpabuf_free(auth->conf_obj[0].c_sign_key);
		auth->conf_obj[0].c_sign_key = NULL;
		goto fail;
	}
	ret = dpp_parse_conf_obj(auth, wpabuf_head(conf_obj),
				 wpabuf_len(conf_obj));
fail:
	wpabuf_free(conf_obj);
	auth->peer_protocol_key = NULL;
	return ret;
}


static int dpp_compatible_netrole(const char *role1, const char *role2)
{
	return (os_strcmp(role1, "sta") == 0 && os_strcmp(role2, "ap") == 0) ||
		(os_strcmp(role1, "ap") == 0 && os_strcmp(role2, "sta") == 0);
}


static int dpp_connector_compatible_group(struct json_token *root,
					  const char *group_id,
					  const char *net_role,
					  bool reconfig)
{
	struct json_token *groups, *token;

	groups = json_get_member(root, "groups");
	if (!groups || groups->type != JSON_ARRAY)
		return 0;

	for (token = groups->child; token; token = token->sibling) {
		struct json_token *id, *role;

		id = json_get_member(token, "groupId");
		if (!id || id->type != JSON_STRING)
			continue;

		role = json_get_member(token, "netRole");
		if (!role || role->type != JSON_STRING)
			continue;

		if (os_strcmp(id->string, "*") != 0 &&
		    os_strcmp(group_id, "*") != 0 &&
		    os_strcmp(id->string, group_id) != 0)
			continue;

		if (reconfig && os_strcmp(net_role, "configurator") == 0)
			return 1;
		if (!reconfig && dpp_compatible_netrole(role->string, net_role))
			return 1;
	}

	return 0;
}


int dpp_connector_match_groups(struct json_token *own_root,
			       struct json_token *peer_root, bool reconfig)
{
	struct json_token *groups, *token;

	groups = json_get_member(peer_root, "groups");
	if (!groups || groups->type != JSON_ARRAY) {
		wpa_printf(MSG_DEBUG, "DPP: No peer groups array found");
		return 0;
	}

	for (token = groups->child; token; token = token->sibling) {
		struct json_token *id, *role;

		id = json_get_member(token, "groupId");
		if (!id || id->type != JSON_STRING) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Missing peer groupId string");
			continue;
		}

		role = json_get_member(token, "netRole");
		if (!role || role->type != JSON_STRING) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Missing peer groups::netRole string");
			continue;
		}
		wpa_printf(MSG_DEBUG,
			   "DPP: peer connector group: groupId='%s' netRole='%s'",
			   id->string, role->string);
		if (dpp_connector_compatible_group(own_root, id->string,
						   role->string, reconfig)) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Compatible group/netRole in own connector");
			return 1;
		}
	}

	return 0;
}


struct json_token * dpp_parse_own_connector(const char *own_connector)
{
	unsigned char *own_conn;
	size_t own_conn_len;
	const char *pos, *end;
	struct json_token *own_root;

	pos = os_strchr(own_connector, '.');
	if (!pos) {
		wpa_printf(MSG_DEBUG, "DPP: Own connector is missing the first dot (.)");
		return NULL;
	}
	pos++;
	end = os_strchr(pos, '.');
	if (!end) {
		wpa_printf(MSG_DEBUG, "DPP: Own connector is missing the second dot (.)");
		return NULL;
	}
	own_conn = base64_url_decode(pos, end - pos, &own_conn_len);
	if (!own_conn) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Failed to base64url decode own signedConnector JWS Payload");
		return NULL;
	}

	own_root = json_parse((const char *) own_conn, own_conn_len);
	os_free(own_conn);
	if (!own_root)
		wpa_printf(MSG_DEBUG, "DPP: Failed to parse local connector");

	return own_root;
}


enum dpp_status_error
dpp_peer_intro(struct dpp_introduction *intro, const char *own_connector,
	       const u8 *net_access_key, size_t net_access_key_len,
	       const u8 *csign_key, size_t csign_key_len,
	       const u8 *peer_connector, size_t peer_connector_len,
	       os_time_t *expiry)
{
	struct json_token *root = NULL, *netkey, *token;
	struct json_token *own_root = NULL;
	enum dpp_status_error ret = 255, res;
	struct crypto_ec_key *own_key = NULL, *peer_key = NULL;
	struct wpabuf *own_key_pub = NULL;
	const struct dpp_curve_params *curve, *own_curve;
	struct dpp_signed_connector_info info;
	size_t Nx_len;
	u8 Nx[DPP_MAX_SHARED_SECRET_LEN];

	os_memset(intro, 0, sizeof(*intro));
	os_memset(&info, 0, sizeof(info));
	if (expiry)
		*expiry = 0;

	own_key = dpp_set_keypair(&own_curve, net_access_key,
				  net_access_key_len);
	if (!own_key) {
		wpa_printf(MSG_ERROR, "DPP: Failed to parse own netAccessKey");
		goto fail;
	}

	own_root = dpp_parse_own_connector(own_connector);
	if (!own_root)
		goto fail;

	res = dpp_check_signed_connector(&info, csign_key, csign_key_len,
					 peer_connector, peer_connector_len);
	if (res != DPP_STATUS_OK) {
		ret = res;
		goto fail;
	}

	root = json_parse((const char *) info.payload, info.payload_len);
	if (!root) {
		wpa_printf(MSG_DEBUG, "DPP: JSON parsing of connector failed");
		ret = DPP_STATUS_INVALID_CONNECTOR;
		goto fail;
	}

	if (!dpp_connector_match_groups(own_root, root, false)) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Peer connector does not include compatible group netrole with own connector");
		ret = DPP_STATUS_NO_MATCH;
		goto fail;
	}

	token = json_get_member(root, "expiry");
	if (!token || token->type != JSON_STRING) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No expiry string found - connector does not expire");
	} else {
		wpa_printf(MSG_DEBUG, "DPP: expiry = %s", token->string);
		if (dpp_key_expired(token->string, expiry)) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Connector (netAccessKey) has expired");
			ret = DPP_STATUS_INVALID_CONNECTOR;
			goto fail;
		}
	}

#ifdef CONFIG_DPP3
	token = json_get_member(root, "version");
	if (token && token->type == JSON_NUMBER) {
		wpa_printf(MSG_DEBUG, "DPP: version = %d", token->number);
		intro->peer_version = token->number;
	}
#endif /* CONFIG_DPP3 */

	netkey = json_get_member(root, "netAccessKey");
	if (!netkey || netkey->type != JSON_OBJECT) {
		wpa_printf(MSG_DEBUG, "DPP: No netAccessKey object found");
		ret = DPP_STATUS_INVALID_CONNECTOR;
		goto fail;
	}

	peer_key = dpp_parse_jwk(netkey, &curve);
	if (!peer_key) {
		ret = DPP_STATUS_INVALID_CONNECTOR;
		goto fail;
	}
	dpp_debug_print_key("DPP: Received netAccessKey", peer_key);

	if (own_curve != curve) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Mismatching netAccessKey curves (%s != %s)",
			   own_curve->name, curve->name);
		ret = DPP_STATUS_INVALID_CONNECTOR;
		goto fail;
	}

	/* ECDH: N = nk * PK */
	if (dpp_ecdh(own_key, peer_key, Nx, &Nx_len) < 0)
		goto fail;

	wpa_hexdump_key(MSG_DEBUG, "DPP: ECDH shared secret (N.x)",
			Nx, Nx_len);

	/* PMK = HKDF(<>, "DPP PMK", N.x) */
	if (dpp_derive_pmk(Nx, Nx_len, intro->pmk, curve->hash_len) < 0) {
		wpa_printf(MSG_ERROR, "DPP: Failed to derive PMK");
		goto fail;
	}
	intro->pmk_len = curve->hash_len;

	/* PMKID = Truncate-128(H(min(NK.x, PK.x) | max(NK.x, PK.x))) */
	if (dpp_derive_pmkid(curve, own_key, peer_key, intro->pmkid) < 0) {
		wpa_printf(MSG_ERROR, "DPP: Failed to derive PMKID");
		goto fail;
	}

	ret = DPP_STATUS_OK;
fail:
	if (ret != DPP_STATUS_OK)
		os_memset(intro, 0, sizeof(*intro));
	os_memset(Nx, 0, sizeof(Nx));
	os_free(info.payload);
	crypto_ec_key_deinit(own_key);
	wpabuf_free(own_key_pub);
	crypto_ec_key_deinit(peer_key);
	json_free(root);
	json_free(own_root);
	return ret;
}


#ifdef CONFIG_DPP3
int dpp_get_connector_version(const char *connector)
{
	struct json_token *root, *token;
	int ver = -1;

	root = dpp_parse_own_connector(connector);
	if (!root)
		return -1;

	token = json_get_member(root, "version");
	if (token && token->type == JSON_NUMBER)
		ver = token->number;

	json_free(root);
	return ver;
}
#endif /* CONFIG_DPP3 */


unsigned int dpp_next_id(struct dpp_global *dpp)
{
	struct dpp_bootstrap_info *bi;
	unsigned int max_id = 0;

	dl_list_for_each(bi, &dpp->bootstrap, struct dpp_bootstrap_info, list) {
		if (bi->id > max_id)
			max_id = bi->id;
	}
	return max_id + 1;
}


static int dpp_bootstrap_del(struct dpp_global *dpp, unsigned int id)
{
	struct dpp_bootstrap_info *bi, *tmp;
	int found = 0;

	if (!dpp)
		return -1;

	dl_list_for_each_safe(bi, tmp, &dpp->bootstrap,
			      struct dpp_bootstrap_info, list) {
		if (id && bi->id != id)
			continue;
		found = 1;
#ifdef CONFIG_DPP2
		if (dpp->remove_bi)
			dpp->remove_bi(dpp->cb_ctx, bi);
#endif /* CONFIG_DPP2 */
		dl_list_del(&bi->list);
		dpp_bootstrap_info_free(bi);
	}

	if (id == 0)
		return 0; /* flush succeeds regardless of entries found */
	return found ? 0 : -1;
}


struct dpp_bootstrap_info * dpp_add_qr_code(struct dpp_global *dpp,
					    const char *uri)
{
	struct dpp_bootstrap_info *bi;

	if (!dpp)
		return NULL;

	bi = dpp_parse_uri(uri);
	if (!bi)
		return NULL;

	bi->type = DPP_BOOTSTRAP_QR_CODE;
	bi->id = dpp_next_id(dpp);
	dl_list_add(&dpp->bootstrap, &bi->list);
	return bi;
}


struct dpp_bootstrap_info * dpp_add_nfc_uri(struct dpp_global *dpp,
					    const char *uri)
{
	struct dpp_bootstrap_info *bi;

	if (!dpp)
		return NULL;

	bi = dpp_parse_uri(uri);
	if (!bi)
		return NULL;

	bi->type = DPP_BOOTSTRAP_NFC_URI;
	bi->id = dpp_next_id(dpp);
	dl_list_add(&dpp->bootstrap, &bi->list);
	return bi;
}


int dpp_bootstrap_gen(struct dpp_global *dpp, const char *cmd)
{
	char *mac = NULL, *info = NULL, *curve = NULL;
	char *key = NULL;
	u8 *privkey = NULL;
	size_t privkey_len = 0;
	int ret = -1;
	struct dpp_bootstrap_info *bi;

	if (!dpp)
		return -1;

	bi = os_zalloc(sizeof(*bi));
	if (!bi)
		goto fail;

	if (os_strstr(cmd, "type=qrcode"))
		bi->type = DPP_BOOTSTRAP_QR_CODE;
	else if (os_strstr(cmd, "type=pkex"))
		bi->type = DPP_BOOTSTRAP_PKEX;
	else if (os_strstr(cmd, "type=nfc-uri"))
		bi->type = DPP_BOOTSTRAP_NFC_URI;
	else
		goto fail;

	bi->chan = get_param(cmd, " chan=");
	mac = get_param(cmd, " mac=");
	info = get_param(cmd, " info=");
	curve = get_param(cmd, " curve=");
	key = get_param(cmd, " key=");

	if (key) {
		privkey_len = os_strlen(key) / 2;
		privkey = os_malloc(privkey_len);
		if (!privkey ||
		    hexstr2bin(key, privkey, privkey_len) < 0)
			goto fail;
	}

	if (dpp_keygen(bi, curve, privkey, privkey_len) < 0 ||
	    dpp_parse_uri_chan_list(bi, bi->chan) < 0 ||
	    dpp_parse_uri_mac(bi, mac) < 0 ||
	    dpp_parse_uri_info(bi, info) < 0 ||
	    dpp_gen_uri(bi) < 0)
		goto fail;

	bi->id = dpp_next_id(dpp);
	dl_list_add(&dpp->bootstrap, &bi->list);
	ret = bi->id;
	bi = NULL;
fail:
	os_free(curve);
	os_free(mac);
	os_free(info);
	str_clear_free(key);
	bin_clear_free(privkey, privkey_len);
	dpp_bootstrap_info_free(bi);
	return ret;
}


struct dpp_bootstrap_info *
dpp_bootstrap_get_id(struct dpp_global *dpp, unsigned int id)
{
	struct dpp_bootstrap_info *bi;

	if (!dpp)
		return NULL;

	dl_list_for_each(bi, &dpp->bootstrap, struct dpp_bootstrap_info, list) {
		if (bi->id == id)
			return bi;
	}
	return NULL;
}


int dpp_bootstrap_remove(struct dpp_global *dpp, const char *id)
{
	unsigned int id_val;

	if (os_strcmp(id, "*") == 0) {
		id_val = 0;
	} else {
		id_val = atoi(id);
		if (id_val == 0)
			return -1;
	}

	return dpp_bootstrap_del(dpp, id_val);
}


const char * dpp_bootstrap_get_uri(struct dpp_global *dpp, unsigned int id)
{
	struct dpp_bootstrap_info *bi;

	bi = dpp_bootstrap_get_id(dpp, id);
	if (!bi)
		return NULL;
	return bi->uri;
}


int dpp_bootstrap_info(struct dpp_global *dpp, int id,
		       char *reply, int reply_size)
{
	struct dpp_bootstrap_info *bi;
	char pkhash[2 * SHA256_MAC_LEN + 1];

	bi = dpp_bootstrap_get_id(dpp, id);
	if (!bi)
		return -1;
	wpa_snprintf_hex(pkhash, sizeof(pkhash), bi->pubkey_hash,
			 SHA256_MAC_LEN);
	return os_snprintf(reply, reply_size, "type=%s\n"
			   "mac_addr=" MACSTR "\n"
			   "info=%s\n"
			   "num_freq=%u\n"
			   "use_freq=%u\n"
			   "curve=%s\n"
			   "pkhash=%s\n"
			   "version=%d\n",
			   dpp_bootstrap_type_txt(bi->type),
			   MAC2STR(bi->mac_addr),
			   bi->info ? bi->info : "",
			   bi->num_freq,
			   bi->num_freq == 1 ? bi->freq[0] : 0,
			   bi->curve->name,
			   pkhash,
			   bi->version);
}


int dpp_bootstrap_set(struct dpp_global *dpp, int id, const char *params)
{
	struct dpp_bootstrap_info *bi;

	bi = dpp_bootstrap_get_id(dpp, id);
	if (!bi)
		return -1;

	str_clear_free(bi->configurator_params);

	if (params) {
		bi->configurator_params = os_strdup(params);
		return bi->configurator_params ? 0 : -1;
	}

	bi->configurator_params = NULL;
	return 0;
}


void dpp_bootstrap_find_pair(struct dpp_global *dpp, const u8 *i_bootstrap,
			     const u8 *r_bootstrap,
			     struct dpp_bootstrap_info **own_bi,
			     struct dpp_bootstrap_info **peer_bi)
{
	struct dpp_bootstrap_info *bi;

	*own_bi = NULL;
	*peer_bi = NULL;
	if (!dpp)
		return;

	dl_list_for_each(bi, &dpp->bootstrap, struct dpp_bootstrap_info, list) {
		if (!*own_bi && bi->own &&
		    os_memcmp(bi->pubkey_hash, r_bootstrap,
			      SHA256_MAC_LEN) == 0) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Found matching own bootstrapping information");
			*own_bi = bi;
		}

		if (!*peer_bi && !bi->own &&
		    os_memcmp(bi->pubkey_hash, i_bootstrap,
			      SHA256_MAC_LEN) == 0) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Found matching peer bootstrapping information");
			*peer_bi = bi;
		}

		if (*own_bi && *peer_bi)
			break;
	}
}


#ifdef CONFIG_DPP2
struct dpp_bootstrap_info * dpp_bootstrap_find_chirp(struct dpp_global *dpp,
						     const u8 *hash)
{
	struct dpp_bootstrap_info *bi;

	if (!dpp)
		return NULL;

	dl_list_for_each(bi, &dpp->bootstrap, struct dpp_bootstrap_info, list) {
		if (!bi->own && os_memcmp(bi->pubkey_hash_chirp, hash,
					  SHA256_MAC_LEN) == 0)
			return bi;
	}

	return NULL;
}
#endif /* CONFIG_DPP2 */


static int dpp_nfc_update_bi_channel(struct dpp_bootstrap_info *own_bi,
				     struct dpp_bootstrap_info *peer_bi)
{
	unsigned int i, freq = 0;
	enum hostapd_hw_mode mode;
	u8 op_class, channel;
	char chan[20];

	if (peer_bi->num_freq == 0 && !peer_bi->channels_listed)
		return 0; /* no channel preference/constraint */

	for (i = 0; i < peer_bi->num_freq; i++) {
		if ((own_bi->num_freq == 0 && !own_bi->channels_listed) ||
		    freq_included(own_bi->freq, own_bi->num_freq,
				  peer_bi->freq[i])) {
			freq = peer_bi->freq[i];
			break;
		}
	}
	if (!freq) {
		wpa_printf(MSG_DEBUG, "DPP: No common channel found");
		return -1;
	}

	mode = ieee80211_freq_to_channel_ext(freq, 0, 0, &op_class, &channel);
	if (mode == NUM_HOSTAPD_MODES) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Could not determine operating class or channel number for %u MHz",
			   freq);
	}

	wpa_printf(MSG_DEBUG,
		   "DPP: Selected %u MHz (op_class %u channel %u) as the negotiation channel based on information from NFC negotiated handover",
		   freq, op_class, channel);
	os_snprintf(chan, sizeof(chan), "%u/%u", op_class, channel);
	os_free(own_bi->chan);
	own_bi->chan = os_strdup(chan);
	own_bi->freq[0] = freq;
	own_bi->num_freq = 1;
	os_free(peer_bi->chan);
	peer_bi->chan = os_strdup(chan);
	peer_bi->freq[0] = freq;
	peer_bi->num_freq = 1;

	return dpp_gen_uri(own_bi);
}


static int dpp_nfc_update_bi_key(struct dpp_bootstrap_info *own_bi,
				 struct dpp_bootstrap_info *peer_bi)
{
	if (peer_bi->curve == own_bi->curve)
		return 0;

	wpa_printf(MSG_DEBUG,
		   "DPP: Update own bootstrapping key to match peer curve from NFC handover");

	crypto_ec_key_deinit(own_bi->pubkey);
	own_bi->pubkey = NULL;

	if (dpp_keygen(own_bi, peer_bi->curve->name, NULL, 0) < 0 ||
	    dpp_gen_uri(own_bi) < 0)
		goto fail;

	return 0;
fail:
	dl_list_del(&own_bi->list);
	dpp_bootstrap_info_free(own_bi);
	return -1;
}


int dpp_nfc_update_bi(struct dpp_bootstrap_info *own_bi,
		      struct dpp_bootstrap_info *peer_bi)
{
	if (dpp_nfc_update_bi_channel(own_bi, peer_bi) < 0 ||
	    dpp_nfc_update_bi_key(own_bi, peer_bi) < 0)
		return -1;
	return 0;
}


static unsigned int dpp_next_configurator_id(struct dpp_global *dpp)
{
	struct dpp_configurator *conf;
	unsigned int max_id = 0;

	dl_list_for_each(conf, &dpp->configurator, struct dpp_configurator,
			 list) {
		if (conf->id > max_id)
			max_id = conf->id;
	}
	return max_id + 1;
}


int dpp_configurator_add(struct dpp_global *dpp, const char *cmd)
{
	char *curve = NULL;
	char *key = NULL, *ppkey = NULL;
	u8 *privkey = NULL, *pp_key = NULL;
	size_t privkey_len = 0, pp_key_len = 0;
	int ret = -1;
	struct dpp_configurator *conf = NULL;

	curve = get_param(cmd, " curve=");
	key = get_param(cmd, " key=");
	ppkey = get_param(cmd, " ppkey=");

	if (key) {
		privkey_len = os_strlen(key) / 2;
		privkey = os_malloc(privkey_len);
		if (!privkey ||
		    hexstr2bin(key, privkey, privkey_len) < 0)
			goto fail;
	}

	if (ppkey) {
		pp_key_len = os_strlen(ppkey) / 2;
		pp_key = os_malloc(pp_key_len);
		if (!pp_key ||
		    hexstr2bin(ppkey, pp_key, pp_key_len) < 0)
			goto fail;
	}

	conf = dpp_keygen_configurator(curve, privkey, privkey_len,
				       pp_key, pp_key_len);
	if (!conf)
		goto fail;

	conf->id = dpp_next_configurator_id(dpp);
	dl_list_add(&dpp->configurator, &conf->list);
	ret = conf->id;
	conf = NULL;
fail:
	os_free(curve);
	str_clear_free(key);
	str_clear_free(ppkey);
	bin_clear_free(privkey, privkey_len);
	bin_clear_free(pp_key, pp_key_len);
	dpp_configurator_free(conf);
	return ret;
}


static int dpp_configurator_del(struct dpp_global *dpp, unsigned int id)
{
	struct dpp_configurator *conf, *tmp;
	int found = 0;

	if (!dpp)
		return -1;

	dl_list_for_each_safe(conf, tmp, &dpp->configurator,
			      struct dpp_configurator, list) {
		if (id && conf->id != id)
			continue;
		found = 1;
		dl_list_del(&conf->list);
		dpp_configurator_free(conf);
	}

	if (id == 0)
		return 0; /* flush succeeds regardless of entries found */
	return found ? 0 : -1;
}


int dpp_configurator_remove(struct dpp_global *dpp, const char *id)
{
	unsigned int id_val;

	if (os_strcmp(id, "*") == 0) {
		id_val = 0;
	} else {
		id_val = atoi(id);
		if (id_val == 0)
			return -1;
	}

	return dpp_configurator_del(dpp, id_val);
}


int dpp_configurator_get_key_id(struct dpp_global *dpp, unsigned int id,
				char *buf, size_t buflen)
{
	struct dpp_configurator *conf;

	conf = dpp_configurator_get_id(dpp, id);
	if (!conf)
		return -1;

	return dpp_configurator_get_key(conf, buf, buflen);
}


#ifdef CONFIG_DPP2

int dpp_configurator_from_backup(struct dpp_global *dpp,
				 struct dpp_asymmetric_key *key)
{
	struct dpp_configurator *conf;
	const struct dpp_curve_params *curve, *curve_pp;

	if (!key->csign || !key->pp_key)
		return -1;

	curve = dpp_get_curve_ike_group(crypto_ec_key_group(key->csign));
	if (!curve) {
		wpa_printf(MSG_INFO, "DPP: Unsupported group in c-sign-key");
		return -1;
	}

	curve_pp = dpp_get_curve_ike_group(crypto_ec_key_group(key->pp_key));
	if (!curve_pp) {
		wpa_printf(MSG_INFO, "DPP: Unsupported group in ppKey");
		return -1;
	}

	if (curve != curve_pp) {
		wpa_printf(MSG_INFO,
			   "DPP: Mismatch in c-sign-key and ppKey groups");
		return -1;
	}

	conf = os_zalloc(sizeof(*conf));
	if (!conf)
		return -1;
	conf->curve = curve;
	conf->csign = key->csign;
	key->csign = NULL;
	conf->pp_key = key->pp_key;
	key->pp_key = NULL;
	conf->own = 1;
	if (dpp_configurator_gen_kid(conf) < 0) {
		dpp_configurator_free(conf);
		return -1;
	}

	conf->id = dpp_next_configurator_id(dpp);
	dl_list_add(&dpp->configurator, &conf->list);
	return conf->id;
}


struct dpp_configurator * dpp_configurator_find_kid(struct dpp_global *dpp,
						    const u8 *kid)
{
	struct dpp_configurator *conf;

	if (!dpp)
		return NULL;

	dl_list_for_each(conf, &dpp->configurator,
			 struct dpp_configurator, list) {
		if (os_memcmp(conf->kid_hash, kid, SHA256_MAC_LEN) == 0)
			return conf;
	}
	return NULL;
}

#endif /* CONFIG_DPP2 */


struct dpp_global * dpp_global_init(struct dpp_global_config *config)
{
	struct dpp_global *dpp;

	dpp = os_zalloc(sizeof(*dpp));
	if (!dpp)
		return NULL;
#ifdef CONFIG_DPP2
	dpp->cb_ctx = config->cb_ctx;
	dpp->remove_bi = config->remove_bi;
#endif /* CONFIG_DPP2 */

	dl_list_init(&dpp->bootstrap);
	dl_list_init(&dpp->configurator);
#ifdef CONFIG_DPP2
	dl_list_init(&dpp->controllers);
	dl_list_init(&dpp->tcp_init);
#endif /* CONFIG_DPP2 */

	return dpp;
}


void dpp_global_clear(struct dpp_global *dpp)
{
	if (!dpp)
		return;

	dpp_bootstrap_del(dpp, 0);
	dpp_configurator_del(dpp, 0);
#ifdef CONFIG_DPP2
	dpp_tcp_init_flush(dpp);
	dpp_relay_flush_controllers(dpp);
	dpp_controller_stop(dpp);
#endif /* CONFIG_DPP2 */
}


void dpp_global_deinit(struct dpp_global *dpp)
{
	dpp_global_clear(dpp);
	os_free(dpp);
}


#ifdef CONFIG_DPP2

struct wpabuf * dpp_build_presence_announcement(struct dpp_bootstrap_info *bi)
{
	struct wpabuf *msg;

	wpa_printf(MSG_DEBUG, "DPP: Build Presence Announcement frame");

	msg = dpp_alloc_msg(DPP_PA_PRESENCE_ANNOUNCEMENT, 4 + SHA256_MAC_LEN);
	if (!msg)
		return NULL;

	/* Responder Bootstrapping Key Hash */
	dpp_build_attr_r_bootstrap_key_hash(msg, bi->pubkey_hash_chirp);
	wpa_hexdump_buf(MSG_DEBUG,
			"DPP: Presence Announcement frame attributes", msg);
	return msg;
}


void dpp_notify_chirp_received(void *msg_ctx, int id, const u8 *src,
				unsigned int freq, const u8 *hash)
{
	char hex[SHA256_MAC_LEN * 2 + 1];

	wpa_snprintf_hex(hex, sizeof(hex), hash, SHA256_MAC_LEN);
	wpa_msg(msg_ctx, MSG_INFO,
		DPP_EVENT_CHIRP_RX "id=%d src=" MACSTR " freq=%u hash=%s",
		id, MAC2STR(src), freq, hex);
}

#endif /* CONFIG_DPP2 */
