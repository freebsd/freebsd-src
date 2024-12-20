/*
 * PASN common processing
 *
 * Copyright (C) 2024, Qualcomm Innovation Center, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/wpa_common.h"
#include "common/sae.h"
#include "crypto/sha384.h"
#include "crypto/crypto.h"
#include "common/ieee802_11_defs.h"
#include "pasn_common.h"


struct pasn_data * pasn_data_init(void)
{
	struct pasn_data *pasn = os_zalloc(sizeof(struct pasn_data));

	return pasn;
}


void pasn_data_deinit(struct pasn_data *pasn)
{
	bin_clear_free(pasn, sizeof(struct pasn_data));
}


void pasn_register_callbacks(struct pasn_data *pasn, void *cb_ctx,
			     int (*send_mgmt)(void *ctx, const u8 *data,
					      size_t data_len, int noack,
					      unsigned int freq,
					      unsigned int wait),
			     int (*validate_custom_pmkid)(void *ctx,
							  const u8 *addr,
							  const u8 *pmkid))
{
	if (!pasn)
		return;

	pasn->cb_ctx = cb_ctx;
	pasn->send_mgmt = send_mgmt;
	pasn->validate_custom_pmkid = validate_custom_pmkid;
}


void pasn_enable_kdk_derivation(struct pasn_data *pasn)
{
	if (!pasn)
		return;
	pasn->derive_kdk = true;
	pasn->kdk_len = WPA_KDK_MAX_LEN;
}


void pasn_disable_kdk_derivation(struct pasn_data *pasn)
{
	if (!pasn)
		return;
	pasn->derive_kdk = false;
	pasn->kdk_len = 0;
}


void pasn_set_akmp(struct pasn_data *pasn, int akmp)
{
	if (!pasn)
		return;
	pasn->akmp = akmp;
}


void pasn_set_cipher(struct pasn_data *pasn, int cipher)
{
	if (!pasn)
		return;
	pasn->cipher = cipher;
}


void pasn_set_own_addr(struct pasn_data *pasn, const u8 *addr)
{
	if (!pasn || !addr)
		return;
	os_memcpy(pasn->own_addr, addr, ETH_ALEN);
}


void pasn_set_peer_addr(struct pasn_data *pasn, const u8 *addr)
{
	if (!pasn || !addr)
		return;
	os_memcpy(pasn->peer_addr, addr, ETH_ALEN);
}


void pasn_set_bssid(struct pasn_data *pasn, const u8 *addr)
{
	if (!pasn || !addr)
		return;
	os_memcpy(pasn->bssid, addr, ETH_ALEN);
}


int pasn_set_pt(struct pasn_data *pasn, struct sae_pt *pt)
{
	if (!pasn)
		return -1;
#ifdef CONFIG_SAE
	pasn->pt = pt;
	return 0;
#else /* CONFIG_SAE */
	return -1;
#endif /* CONFIG_SAE */
}


void pasn_set_password(struct pasn_data *pasn, const char *password)
{
	if (!pasn)
		return;
	pasn->password = password;
}


void pasn_set_wpa_key_mgmt(struct pasn_data *pasn, int key_mgmt)
{
	if (!pasn)
		return;
	pasn->wpa_key_mgmt = key_mgmt;
}


void pasn_set_rsn_pairwise(struct pasn_data *pasn, int rsn_pairwise)
{
	if (!pasn)
		return;
	pasn->rsn_pairwise = rsn_pairwise;
}


void pasn_set_rsnxe_caps(struct pasn_data *pasn, u16 rsnxe_capab)
{
	if (!pasn)
		return;
	pasn->rsnxe_capab = rsnxe_capab;
}


void pasn_set_rsnxe_ie(struct pasn_data *pasn, const u8 *rsnxe_ie)
{
	if (!pasn || !rsnxe_ie)
		return;
	pasn->rsnxe_ie = rsnxe_ie;
}


void pasn_set_custom_pmkid(struct pasn_data *pasn, const u8 *pmkid)
{
	if (!pasn || !pmkid)
		return;
	os_memcpy(pasn->custom_pmkid, pmkid, PMKID_LEN);
	pasn->custom_pmkid_valid = true;
}


int pasn_set_extra_ies(struct pasn_data *pasn, const u8 *extra_ies,
		       size_t extra_ies_len)
{
	if (!pasn || !extra_ies_len || !extra_ies)
		return -1;

	if (pasn->extra_ies) {
		os_free((u8 *) pasn->extra_ies);
		pasn->extra_ies_len = extra_ies_len;
	}

	pasn->extra_ies = os_memdup(extra_ies, extra_ies_len);
	if (!pasn->extra_ies) {
		wpa_printf(MSG_ERROR,
			   "PASN: Extra IEs memory allocation failed");
		return -1;
	}
	pasn->extra_ies_len = extra_ies_len;
	return 0;
}


int pasn_get_akmp(struct pasn_data *pasn)
{
	if (!pasn)
		return 0;
	return pasn->akmp;
}


int pasn_get_cipher(struct pasn_data *pasn)
{
	if (!pasn)
		return 0;
	return pasn->cipher;
}


size_t pasn_get_pmk_len(struct pasn_data *pasn)
{
	if (!pasn)
		return 0;
	return pasn->pmk_len;
}


u8 * pasn_get_pmk(struct pasn_data *pasn)
{
	if (!pasn)
		return NULL;
	return pasn->pmk;
}


struct wpa_ptk * pasn_get_ptk(struct pasn_data *pasn)
{
	if (!pasn)
		return NULL;
	return &pasn->ptk;
}
