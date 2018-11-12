/*
 * WPA Supplicant - Mesh RSN routines
 * Copyright (c) 2013-2014, cozybit, Inc.  All rights reserved.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "crypto/sha256.h"
#include "crypto/random.h"
#include "crypto/aes.h"
#include "crypto/aes_siv.h"
#include "rsn_supp/wpa.h"
#include "ap/hostapd.h"
#include "ap/wpa_auth.h"
#include "ap/sta_info.h"
#include "ap/ieee802_11.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "wpas_glue.h"
#include "mesh_mpm.h"
#include "mesh_rsn.h"

#define MESH_AUTH_TIMEOUT 10
#define MESH_AUTH_RETRY 3
#define MESH_AUTH_BLOCK_DURATION 3600

void mesh_auth_timer(void *eloop_ctx, void *user_data)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct sta_info *sta = user_data;

	if (sta->sae->state != SAE_ACCEPTED) {
		wpa_printf(MSG_DEBUG, "AUTH: Re-authenticate with " MACSTR
			   " (attempt %d) ",
			   MAC2STR(sta->addr), sta->sae_auth_retry);
		wpa_msg(wpa_s, MSG_INFO, MESH_SAE_AUTH_FAILURE "addr=" MACSTR,
			MAC2STR(sta->addr));
		if (sta->sae_auth_retry < MESH_AUTH_RETRY) {
			mesh_rsn_auth_sae_sta(wpa_s, sta);
		} else {
			if (sta->sae_auth_retry > MESH_AUTH_RETRY) {
				ap_free_sta(wpa_s->ifmsh->bss[0], sta);
				return;
			}

			/* block the STA if exceeded the number of attempts */
			wpa_mesh_set_plink_state(wpa_s, sta, PLINK_BLOCKED);
			sta->sae->state = SAE_NOTHING;
			if (wpa_s->mesh_auth_block_duration <
			    MESH_AUTH_BLOCK_DURATION)
				wpa_s->mesh_auth_block_duration += 60;
			eloop_register_timeout(wpa_s->mesh_auth_block_duration,
					       0, mesh_auth_timer, wpa_s, sta);
			wpa_msg(wpa_s, MSG_INFO, MESH_SAE_AUTH_BLOCKED "addr="
				MACSTR " duration=%d",
				MAC2STR(sta->addr),
				wpa_s->mesh_auth_block_duration);
		}
		sta->sae_auth_retry++;
	}
}


static void auth_logger(void *ctx, const u8 *addr, logger_level level,
			const char *txt)
{
	if (addr)
		wpa_printf(MSG_DEBUG, "AUTH: " MACSTR " - %s",
			   MAC2STR(addr), txt);
	else
		wpa_printf(MSG_DEBUG, "AUTH: %s", txt);
}


static const u8 *auth_get_psk(void *ctx, const u8 *addr,
			      const u8 *p2p_dev_addr, const u8 *prev_psk)
{
	struct mesh_rsn *mesh_rsn = ctx;
	struct hostapd_data *hapd = mesh_rsn->wpa_s->ifmsh->bss[0];
	struct sta_info *sta = ap_get_sta(hapd, addr);

	wpa_printf(MSG_DEBUG, "AUTH: %s (addr=" MACSTR " prev_psk=%p)",
		   __func__, MAC2STR(addr), prev_psk);

	if (sta && sta->auth_alg == WLAN_AUTH_SAE) {
		if (!sta->sae || prev_psk)
			return NULL;
		return sta->sae->pmk;
	}

	return NULL;
}


static int auth_set_key(void *ctx, int vlan_id, enum wpa_alg alg,
			const u8 *addr, int idx, u8 *key, size_t key_len)
{
	struct mesh_rsn *mesh_rsn = ctx;
	u8 seq[6];

	os_memset(seq, 0, sizeof(seq));

	if (addr) {
		wpa_printf(MSG_DEBUG, "AUTH: %s(alg=%d addr=" MACSTR
			   " key_idx=%d)",
			   __func__, alg, MAC2STR(addr), idx);
	} else {
		wpa_printf(MSG_DEBUG, "AUTH: %s(alg=%d key_idx=%d)",
			   __func__, alg, idx);
	}
	wpa_hexdump_key(MSG_DEBUG, "AUTH: set_key - key", key, key_len);

	return wpa_drv_set_key(mesh_rsn->wpa_s, alg, addr, idx,
			       1, seq, 6, key, key_len);
}


static int auth_start_ampe(void *ctx, const u8 *addr)
{
	struct mesh_rsn *mesh_rsn = ctx;
	struct hostapd_data *hapd;
	struct sta_info *sta;

	if (mesh_rsn->wpa_s->current_ssid->mode != WPAS_MODE_MESH)
		return -1;

	hapd = mesh_rsn->wpa_s->ifmsh->bss[0];
	sta = ap_get_sta(hapd, addr);
	if (sta)
		eloop_cancel_timeout(mesh_auth_timer, mesh_rsn->wpa_s, sta);

	mesh_mpm_auth_peer(mesh_rsn->wpa_s, addr);
	return 0;
}


static int __mesh_rsn_auth_init(struct mesh_rsn *rsn, const u8 *addr)
{
	struct wpa_auth_config conf;
	struct wpa_auth_callbacks cb;
	u8 seq[6] = {};

	wpa_printf(MSG_DEBUG, "AUTH: Initializing group state machine");

	os_memset(&conf, 0, sizeof(conf));
	conf.wpa = 2;
	conf.wpa_key_mgmt = WPA_KEY_MGMT_SAE;
	conf.wpa_pairwise = WPA_CIPHER_CCMP;
	conf.rsn_pairwise = WPA_CIPHER_CCMP;
	conf.wpa_group = WPA_CIPHER_CCMP;
	conf.eapol_version = 0;
	conf.wpa_group_rekey = -1;

	os_memset(&cb, 0, sizeof(cb));
	cb.ctx = rsn;
	cb.logger = auth_logger;
	cb.get_psk = auth_get_psk;
	cb.set_key = auth_set_key;
	cb.start_ampe = auth_start_ampe;

	rsn->auth = wpa_init(addr, &conf, &cb);
	if (rsn->auth == NULL) {
		wpa_printf(MSG_DEBUG, "AUTH: wpa_init() failed");
		return -1;
	}

	/* TODO: support rekeying */
	if (random_get_bytes(rsn->mgtk, 16) < 0) {
		wpa_deinit(rsn->auth);
		return -1;
	}

	/* group mgmt */
	wpa_drv_set_key(rsn->wpa_s, WPA_ALG_IGTK, NULL, 4, 1,
			seq, sizeof(seq), rsn->mgtk, sizeof(rsn->mgtk));

	/* group privacy / data frames */
	wpa_drv_set_key(rsn->wpa_s, WPA_ALG_CCMP, NULL, 1, 1,
			seq, sizeof(seq), rsn->mgtk, sizeof(rsn->mgtk));

	return 0;
}


static void mesh_rsn_deinit(struct mesh_rsn *rsn)
{
	os_memset(rsn->mgtk, 0, sizeof(rsn->mgtk));
	wpa_deinit(rsn->auth);
}


struct mesh_rsn *mesh_rsn_auth_init(struct wpa_supplicant *wpa_s,
				    struct mesh_conf *conf)
{
	struct mesh_rsn *mesh_rsn;
	struct hostapd_data *bss = wpa_s->ifmsh->bss[0];
	const u8 *ie;
	size_t ie_len;

	mesh_rsn = os_zalloc(sizeof(*mesh_rsn));
	if (mesh_rsn == NULL)
		return NULL;
	mesh_rsn->wpa_s = wpa_s;

	if (__mesh_rsn_auth_init(mesh_rsn, wpa_s->own_addr) < 0) {
		mesh_rsn_deinit(mesh_rsn);
		return NULL;
	}

	bss->wpa_auth = mesh_rsn->auth;

	ie = wpa_auth_get_wpa_ie(mesh_rsn->auth, &ie_len);
	conf->ies = (u8 *) ie;
	conf->ie_len = ie_len;

	wpa_supplicant_rsn_supp_set_config(wpa_s, wpa_s->current_ssid);

	return mesh_rsn;
}


static int index_within_array(const int *array, int idx)
{
	int i;

	for (i = 0; i < idx; i++) {
		if (array[i] == -1)
			return 0;
	}

	return 1;
}


static int mesh_rsn_sae_group(struct wpa_supplicant *wpa_s,
			      struct sae_data *sae)
{
	int *groups = wpa_s->ifmsh->bss[0]->conf->sae_groups;

	/* Configuration may have changed, so validate current index */
	if (!index_within_array(groups, wpa_s->mesh_rsn->sae_group_index))
		return -1;

	for (;;) {
		int group = groups[wpa_s->mesh_rsn->sae_group_index];

		if (group <= 0)
			break;
		if (sae_set_group(sae, group) == 0) {
			wpa_dbg(wpa_s, MSG_DEBUG, "SME: Selected SAE group %d",
				sae->group);
			return 0;
		}
		wpa_s->mesh_rsn->sae_group_index++;
	}

	return -1;
}


static int mesh_rsn_build_sae_commit(struct wpa_supplicant *wpa_s,
				     struct wpa_ssid *ssid,
				     struct sta_info *sta)
{
	if (ssid->passphrase == NULL) {
		wpa_msg(wpa_s, MSG_DEBUG, "SAE: No password available");
		return -1;
	}

	if (mesh_rsn_sae_group(wpa_s, sta->sae) < 0) {
		wpa_msg(wpa_s, MSG_DEBUG, "SAE: Failed to select group");
		return -1;
	}

	return sae_prepare_commit(wpa_s->own_addr, sta->addr,
				  (u8 *) ssid->passphrase,
				  os_strlen(ssid->passphrase), sta->sae);
}


/* initiate new SAE authentication with sta */
int mesh_rsn_auth_sae_sta(struct wpa_supplicant *wpa_s,
			  struct sta_info *sta)
{
	struct hostapd_data *hapd = wpa_s->ifmsh->bss[0];
	struct wpa_ssid *ssid = wpa_s->current_ssid;
	unsigned int rnd;
	int ret;

	if (!ssid) {
		wpa_msg(wpa_s, MSG_DEBUG,
			"AUTH: No current_ssid known to initiate new SAE");
		return -1;
	}

	if (!sta->sae) {
		sta->sae = os_zalloc(sizeof(*sta->sae));
		if (sta->sae == NULL)
			return -1;
	}

	if (mesh_rsn_build_sae_commit(wpa_s, ssid, sta))
		return -1;

	wpa_msg(wpa_s, MSG_DEBUG,
		"AUTH: started authentication with SAE peer: " MACSTR,
		MAC2STR(sta->addr));

	wpa_supplicant_set_state(wpa_s, WPA_AUTHENTICATING);
	ret = auth_sae_init_committed(hapd, sta);
	if (ret)
		return ret;

	eloop_cancel_timeout(mesh_auth_timer, wpa_s, sta);
	rnd = rand() % MESH_AUTH_TIMEOUT;
	eloop_register_timeout(MESH_AUTH_TIMEOUT + rnd, 0, mesh_auth_timer,
			       wpa_s, sta);
	return 0;
}


void mesh_rsn_get_pmkid(struct mesh_rsn *rsn, struct sta_info *sta, u8 *pmkid)
{
	/* don't expect wpa auth to cache the pmkid for now */
	rsn_pmkid(sta->sae->pmk, PMK_LEN, rsn->wpa_s->own_addr,
		  sta->addr, pmkid,
		  wpa_key_mgmt_sha256(wpa_auth_sta_key_mgmt(sta->wpa_sm)));
}


static void
mesh_rsn_derive_aek(struct mesh_rsn *rsn, struct sta_info *sta)
{
	u8 *myaddr = rsn->wpa_s->own_addr;
	u8 *peer = sta->addr;
	u8 *addr1 = peer, *addr2 = myaddr;
	u8 context[AES_BLOCK_SIZE];

	/* SAE */
	RSN_SELECTOR_PUT(context, wpa_cipher_to_suite(0, WPA_CIPHER_GCMP));

	if (os_memcmp(myaddr, peer, ETH_ALEN) < 0) {
		addr1 = myaddr;
		addr2 = peer;
	}
	os_memcpy(context + 4, addr1, ETH_ALEN);
	os_memcpy(context + 10, addr2, ETH_ALEN);

	sha256_prf(sta->sae->pmk, sizeof(sta->sae->pmk), "AEK Derivation",
		   context, sizeof(context), sta->aek, sizeof(sta->aek));
}


/* derive mesh temporal key from pmk */
int mesh_rsn_derive_mtk(struct wpa_supplicant *wpa_s, struct sta_info *sta)
{
	u8 *ptr;
	u8 *min, *max;
	u16 min_lid, max_lid;
	size_t nonce_len = sizeof(sta->my_nonce);
	size_t lid_len = sizeof(sta->my_lid);
	u8 *myaddr = wpa_s->own_addr;
	u8 *peer = sta->addr;
	/* 2 nonces, 2 linkids, akm suite, 2 mac addrs */
	u8 context[64 + 4 + 4 + 12];

	ptr = context;
	if (os_memcmp(sta->my_nonce, sta->peer_nonce, nonce_len) < 0) {
		min = sta->my_nonce;
		max = sta->peer_nonce;
	} else {
		min = sta->peer_nonce;
		max = sta->my_nonce;
	}
	os_memcpy(ptr, min, nonce_len);
	os_memcpy(ptr + nonce_len, max, nonce_len);
	ptr += 2 * nonce_len;

	if (sta->my_lid < sta->peer_lid) {
		min_lid = host_to_le16(sta->my_lid);
		max_lid = host_to_le16(sta->peer_lid);
	} else {
		min_lid = host_to_le16(sta->peer_lid);
		max_lid = host_to_le16(sta->my_lid);
	}
	os_memcpy(ptr, &min_lid, lid_len);
	os_memcpy(ptr + lid_len, &max_lid, lid_len);
	ptr += 2 * lid_len;

	/* SAE */
	RSN_SELECTOR_PUT(ptr, wpa_cipher_to_suite(0, WPA_CIPHER_GCMP));
	ptr += 4;

	if (os_memcmp(myaddr, peer, ETH_ALEN) < 0) {
		min = myaddr;
		max = peer;
	} else {
		min = peer;
		max = myaddr;
	}
	os_memcpy(ptr, min, ETH_ALEN);
	os_memcpy(ptr + ETH_ALEN, max, ETH_ALEN);

	sha256_prf(sta->sae->pmk, sizeof(sta->sae->pmk),
		   "Temporal Key Derivation", context, sizeof(context),
		   sta->mtk, sizeof(sta->mtk));
	return 0;
}


void mesh_rsn_init_ampe_sta(struct wpa_supplicant *wpa_s, struct sta_info *sta)
{
	if (random_get_bytes(sta->my_nonce, 32) < 0) {
		wpa_printf(MSG_INFO, "mesh: Failed to derive random nonce");
		/* TODO: How to handle this more cleanly? */
	}
	os_memset(sta->peer_nonce, 0, 32);
	mesh_rsn_derive_aek(wpa_s->mesh_rsn, sta);
}


/* insert AMPE and encrypted MIC at @ie.
 * @mesh_rsn: mesh RSN context
 * @sta: STA we're sending to
 * @cat: pointer to category code in frame header.
 * @buf: wpabuf to add encrypted AMPE and MIC to.
 * */
int mesh_rsn_protect_frame(struct mesh_rsn *rsn, struct sta_info *sta,
			   const u8 *cat, struct wpabuf *buf)
{
	struct ieee80211_ampe_ie *ampe;
	u8 const *ie = wpabuf_head_u8(buf) + wpabuf_len(buf);
	u8 *ampe_ie = NULL, *mic_ie = NULL, *mic_payload;
	const u8 *aad[] = { rsn->wpa_s->own_addr, sta->addr, cat };
	const size_t aad_len[] = { ETH_ALEN, ETH_ALEN, ie - cat };
	int ret = 0;

	if (AES_BLOCK_SIZE + 2 + sizeof(*ampe) + 2 > wpabuf_tailroom(buf)) {
		wpa_printf(MSG_ERROR, "protect frame: buffer too small");
		return -EINVAL;
	}

	ampe_ie = os_zalloc(2 + sizeof(*ampe));
	if (!ampe_ie) {
		wpa_printf(MSG_ERROR, "protect frame: out of memory");
		return -ENOMEM;
	}

	mic_ie = os_zalloc(2 + AES_BLOCK_SIZE);
	if (!mic_ie) {
		wpa_printf(MSG_ERROR, "protect frame: out of memory");
		ret = -ENOMEM;
		goto free;
	}

	/*  IE: AMPE */
	ampe_ie[0] = WLAN_EID_AMPE;
	ampe_ie[1] = sizeof(*ampe);
	ampe = (struct ieee80211_ampe_ie *) (ampe_ie + 2);

	RSN_SELECTOR_PUT(ampe->selected_pairwise_suite,
		     wpa_cipher_to_suite(WPA_PROTO_RSN, WPA_CIPHER_CCMP));
	os_memcpy(ampe->local_nonce, sta->my_nonce, 32);
	os_memcpy(ampe->peer_nonce, sta->peer_nonce, 32);
	/* incomplete: see 13.5.4 */
	/* TODO: static mgtk for now since we don't support rekeying! */
	os_memcpy(ampe->mgtk, rsn->mgtk, 16);
	/*  TODO: Populate Key RSC */
	/*  expire in 13 decades or so */
	os_memset(ampe->key_expiration, 0xff, 4);

	/* IE: MIC */
	mic_ie[0] = WLAN_EID_MIC;
	mic_ie[1] = AES_BLOCK_SIZE;
	wpabuf_put_data(buf, mic_ie, 2);
	/* MIC field is output ciphertext */

	/* encrypt after MIC */
	mic_payload = (u8 *) wpabuf_put(buf, 2 + sizeof(*ampe) +
					AES_BLOCK_SIZE);

	if (aes_siv_encrypt(sta->aek, ampe_ie, 2 + sizeof(*ampe), 3,
			    aad, aad_len, mic_payload)) {
		wpa_printf(MSG_ERROR, "protect frame: failed to encrypt");
		ret = -ENOMEM;
		goto free;
	}

free:
	os_free(ampe_ie);
	os_free(mic_ie);

	return ret;
}


int mesh_rsn_process_ampe(struct wpa_supplicant *wpa_s, struct sta_info *sta,
			  struct ieee802_11_elems *elems, const u8 *cat,
			  const u8 *start, size_t elems_len)
{
	int ret = 0;
	struct ieee80211_ampe_ie *ampe;
	u8 null_nonce[32] = {};
	u8 ampe_eid;
	u8 ampe_ie_len;
	u8 *ampe_buf, *crypt = NULL;
	size_t crypt_len;
	const u8 *aad[] = { sta->addr, wpa_s->own_addr, cat };
	const size_t aad_len[] = { ETH_ALEN, ETH_ALEN,
				   (elems->mic - 2) - cat };

	if (!elems->mic || elems->mic_len < AES_BLOCK_SIZE) {
		wpa_msg(wpa_s, MSG_DEBUG, "Mesh RSN: missing mic ie");
		return -1;
	}

	ampe_buf = (u8 *) elems->mic + elems->mic_len;
	if ((int) elems_len < ampe_buf - start)
		return -1;

	crypt_len = elems_len - (elems->mic - start);
	if (crypt_len < 2) {
		wpa_msg(wpa_s, MSG_DEBUG, "Mesh RSN: missing ampe ie");
		return -1;
	}

	/* crypt is modified by siv_decrypt */
	crypt = os_zalloc(crypt_len);
	if (!crypt) {
		wpa_printf(MSG_ERROR, "Mesh RSN: out of memory");
		ret = -ENOMEM;
		goto free;
	}

	os_memcpy(crypt, elems->mic, crypt_len);

	if (aes_siv_decrypt(sta->aek, crypt, crypt_len, 3,
			    aad, aad_len, ampe_buf)) {
		wpa_printf(MSG_ERROR, "Mesh RSN: frame verification failed!");
		ret = -1;
		goto free;
	}

	ampe_eid = *ampe_buf++;
	ampe_ie_len = *ampe_buf++;

	if (ampe_eid != WLAN_EID_AMPE ||
	    ampe_ie_len < sizeof(struct ieee80211_ampe_ie)) {
		wpa_msg(wpa_s, MSG_DEBUG, "Mesh RSN: invalid ampe ie");
		ret = -1;
		goto free;
	}

	ampe = (struct ieee80211_ampe_ie *) ampe_buf;
	if (os_memcmp(ampe->peer_nonce, null_nonce, 32) != 0 &&
	    os_memcmp(ampe->peer_nonce, sta->my_nonce, 32) != 0) {
		wpa_msg(wpa_s, MSG_DEBUG, "Mesh RSN: invalid peer nonce");
		ret = -1;
		goto free;
	}
	os_memcpy(sta->peer_nonce, ampe->local_nonce,
		  sizeof(ampe->local_nonce));
	os_memcpy(sta->mgtk, ampe->mgtk, sizeof(ampe->mgtk));

	/* todo parse mgtk expiration */
free:
	os_free(crypt);
	return ret;
}
