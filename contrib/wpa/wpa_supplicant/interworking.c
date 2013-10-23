/*
 * Interworking (IEEE 802.11u)
 * Copyright (c) 2011-2012, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "common/ieee802_11_defs.h"
#include "common/gas.h"
#include "common/wpa_ctrl.h"
#include "utils/pcsc_funcs.h"
#include "utils/eloop.h"
#include "drivers/driver.h"
#include "eap_common/eap_defs.h"
#include "eap_peer/eap.h"
#include "eap_peer/eap_methods.h"
#include "wpa_supplicant_i.h"
#include "config.h"
#include "config_ssid.h"
#include "bss.h"
#include "scan.h"
#include "notify.h"
#include "gas_query.h"
#include "hs20_supplicant.h"
#include "interworking.h"


#if defined(EAP_SIM) | defined(EAP_SIM_DYNAMIC)
#define INTERWORKING_3GPP
#else
#if defined(EAP_AKA) | defined(EAP_AKA_DYNAMIC)
#define INTERWORKING_3GPP
#else
#if defined(EAP_AKA_PRIME) | defined(EAP_AKA_PRIME_DYNAMIC)
#define INTERWORKING_3GPP
#endif
#endif
#endif

static void interworking_next_anqp_fetch(struct wpa_supplicant *wpa_s);


static void interworking_reconnect(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->wpa_state >= WPA_AUTHENTICATING) {
		wpa_supplicant_cancel_sched_scan(wpa_s);
		wpa_supplicant_deauthenticate(wpa_s,
					      WLAN_REASON_DEAUTH_LEAVING);
	}
	wpa_s->disconnected = 0;
	wpa_s->reassociate = 1;

	if (wpa_s->last_scan_res_used > 0) {
		struct os_time now;
		os_get_time(&now);
		if (now.sec - wpa_s->last_scan.sec <= 5) {
			wpa_printf(MSG_DEBUG, "Interworking: Old scan results "
				   "are fresh - connect without new scan");
			if (wpas_select_network_from_last_scan(wpa_s) >= 0)
				return;
		}
	}

	wpa_supplicant_req_scan(wpa_s, 0, 0);
}


static struct wpabuf * anqp_build_req(u16 info_ids[], size_t num_ids,
				      struct wpabuf *extra)
{
	struct wpabuf *buf;
	size_t i;
	u8 *len_pos;

	buf = gas_anqp_build_initial_req(0, 4 + num_ids * 2 +
					 (extra ? wpabuf_len(extra) : 0));
	if (buf == NULL)
		return NULL;

	len_pos = gas_anqp_add_element(buf, ANQP_QUERY_LIST);
	for (i = 0; i < num_ids; i++)
		wpabuf_put_le16(buf, info_ids[i]);
	gas_anqp_set_element_len(buf, len_pos);
	if (extra)
		wpabuf_put_buf(buf, extra);

	gas_anqp_set_len(buf);

	return buf;
}


static void interworking_anqp_resp_cb(void *ctx, const u8 *dst,
				      u8 dialog_token,
				      enum gas_query_result result,
				      const struct wpabuf *adv_proto,
				      const struct wpabuf *resp,
				      u16 status_code)
{
	struct wpa_supplicant *wpa_s = ctx;

	anqp_resp_cb(wpa_s, dst, dialog_token, result, adv_proto, resp,
		     status_code);
	interworking_next_anqp_fetch(wpa_s);
}


static int cred_with_roaming_consortium(struct wpa_supplicant *wpa_s)
{
	struct wpa_cred *cred;

	for (cred = wpa_s->conf->cred; cred; cred = cred->next) {
		if (cred->roaming_consortium_len)
			return 1;
	}
	return 0;
}


static int cred_with_3gpp(struct wpa_supplicant *wpa_s)
{
	struct wpa_cred *cred;

	for (cred = wpa_s->conf->cred; cred; cred = cred->next) {
		if (cred->pcsc || cred->imsi)
			return 1;
	}
	return 0;
}


static int cred_with_nai_realm(struct wpa_supplicant *wpa_s)
{
	struct wpa_cred *cred;

	for (cred = wpa_s->conf->cred; cred; cred = cred->next) {
		if (cred->pcsc || cred->imsi)
			continue;
		if (!cred->eap_method)
			return 1;
		if (cred->realm && cred->roaming_consortium_len == 0)
			return 1;
	}
	return 0;
}


static int cred_with_domain(struct wpa_supplicant *wpa_s)
{
	struct wpa_cred *cred;

	for (cred = wpa_s->conf->cred; cred; cred = cred->next) {
		if (cred->domain || cred->pcsc || cred->imsi)
			return 1;
	}
	return 0;
}


static int additional_roaming_consortiums(struct wpa_bss *bss)
{
	const u8 *ie;
	ie = wpa_bss_get_ie(bss, WLAN_EID_ROAMING_CONSORTIUM);
	if (ie == NULL || ie[1] == 0)
		return 0;
	return ie[2]; /* Number of ANQP OIs */
}


static void interworking_continue_anqp(void *eloop_ctx, void *sock_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	interworking_next_anqp_fetch(wpa_s);
}


static int interworking_anqp_send_req(struct wpa_supplicant *wpa_s,
				      struct wpa_bss *bss)
{
	struct wpabuf *buf;
	int ret = 0;
	int res;
	u16 info_ids[8];
	size_t num_info_ids = 0;
	struct wpabuf *extra = NULL;
	int all = wpa_s->fetch_all_anqp;

	wpa_printf(MSG_DEBUG, "Interworking: ANQP Query Request to " MACSTR,
		   MAC2STR(bss->bssid));

	info_ids[num_info_ids++] = ANQP_CAPABILITY_LIST;
	if (all) {
		info_ids[num_info_ids++] = ANQP_VENUE_NAME;
		info_ids[num_info_ids++] = ANQP_NETWORK_AUTH_TYPE;
	}
	if (all || (cred_with_roaming_consortium(wpa_s) &&
		    additional_roaming_consortiums(bss)))
		info_ids[num_info_ids++] = ANQP_ROAMING_CONSORTIUM;
	if (all)
		info_ids[num_info_ids++] = ANQP_IP_ADDR_TYPE_AVAILABILITY;
	if (all || cred_with_nai_realm(wpa_s))
		info_ids[num_info_ids++] = ANQP_NAI_REALM;
	if (all || cred_with_3gpp(wpa_s))
		info_ids[num_info_ids++] = ANQP_3GPP_CELLULAR_NETWORK;
	if (all || cred_with_domain(wpa_s))
		info_ids[num_info_ids++] = ANQP_DOMAIN_NAME;
	wpa_hexdump(MSG_DEBUG, "Interworking: ANQP Query info",
		    (u8 *) info_ids, num_info_ids * 2);

#ifdef CONFIG_HS20
	if (wpa_bss_get_vendor_ie(bss, HS20_IE_VENDOR_TYPE)) {
		u8 *len_pos;

		extra = wpabuf_alloc(100);
		if (!extra)
			return -1;

		len_pos = gas_anqp_add_element(extra, ANQP_VENDOR_SPECIFIC);
		wpabuf_put_be24(extra, OUI_WFA);
		wpabuf_put_u8(extra, HS20_ANQP_OUI_TYPE);
		wpabuf_put_u8(extra, HS20_STYPE_QUERY_LIST);
		wpabuf_put_u8(extra, 0); /* Reserved */
		wpabuf_put_u8(extra, HS20_STYPE_CAPABILITY_LIST);
		if (all) {
			wpabuf_put_u8(extra,
				      HS20_STYPE_OPERATOR_FRIENDLY_NAME);
			wpabuf_put_u8(extra, HS20_STYPE_WAN_METRICS);
			wpabuf_put_u8(extra, HS20_STYPE_CONNECTION_CAPABILITY);
			wpabuf_put_u8(extra, HS20_STYPE_OPERATING_CLASS);
		}
		gas_anqp_set_element_len(extra, len_pos);
	}
#endif /* CONFIG_HS20 */

	buf = anqp_build_req(info_ids, num_info_ids, extra);
	wpabuf_free(extra);
	if (buf == NULL)
		return -1;

	res = gas_query_req(wpa_s->gas, bss->bssid, bss->freq, buf,
			    interworking_anqp_resp_cb, wpa_s);
	if (res < 0) {
		wpa_printf(MSG_DEBUG, "ANQP: Failed to send Query Request");
		ret = -1;
		eloop_register_timeout(0, 0, interworking_continue_anqp, wpa_s,
				       NULL);
	} else
		wpa_printf(MSG_DEBUG, "ANQP: Query started with dialog token "
			   "%u", res);

	wpabuf_free(buf);
	return ret;
}


struct nai_realm_eap {
	u8 method;
	u8 inner_method;
	enum nai_realm_eap_auth_inner_non_eap inner_non_eap;
	u8 cred_type;
	u8 tunneled_cred_type;
};

struct nai_realm {
	u8 encoding;
	char *realm;
	u8 eap_count;
	struct nai_realm_eap *eap;
};


static void nai_realm_free(struct nai_realm *realms, u16 count)
{
	u16 i;

	if (realms == NULL)
		return;
	for (i = 0; i < count; i++) {
		os_free(realms[i].eap);
		os_free(realms[i].realm);
	}
	os_free(realms);
}


static const u8 * nai_realm_parse_eap(struct nai_realm_eap *e, const u8 *pos,
				      const u8 *end)
{
	u8 elen, auth_count, a;
	const u8 *e_end;

	if (pos + 3 > end) {
		wpa_printf(MSG_DEBUG, "No room for EAP Method fixed fields");
		return NULL;
	}

	elen = *pos++;
	if (pos + elen > end || elen < 2) {
		wpa_printf(MSG_DEBUG, "No room for EAP Method subfield");
		return NULL;
	}
	e_end = pos + elen;
	e->method = *pos++;
	auth_count = *pos++;
	wpa_printf(MSG_DEBUG, "EAP Method: len=%u method=%u auth_count=%u",
		   elen, e->method, auth_count);

	for (a = 0; a < auth_count; a++) {
		u8 id, len;

		if (pos + 2 > end || pos + 2 + pos[1] > end) {
			wpa_printf(MSG_DEBUG, "No room for Authentication "
				   "Parameter subfield");
			return NULL;
		}

		id = *pos++;
		len = *pos++;

		switch (id) {
		case NAI_REALM_EAP_AUTH_NON_EAP_INNER_AUTH:
			if (len < 1)
				break;
			e->inner_non_eap = *pos;
			if (e->method != EAP_TYPE_TTLS)
				break;
			switch (*pos) {
			case NAI_REALM_INNER_NON_EAP_PAP:
				wpa_printf(MSG_DEBUG, "EAP-TTLS/PAP");
				break;
			case NAI_REALM_INNER_NON_EAP_CHAP:
				wpa_printf(MSG_DEBUG, "EAP-TTLS/CHAP");
				break;
			case NAI_REALM_INNER_NON_EAP_MSCHAP:
				wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAP");
				break;
			case NAI_REALM_INNER_NON_EAP_MSCHAPV2:
				wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAPV2");
				break;
			}
			break;
		case NAI_REALM_EAP_AUTH_INNER_AUTH_EAP_METHOD:
			if (len < 1)
				break;
			e->inner_method = *pos;
			wpa_printf(MSG_DEBUG, "Inner EAP method: %u",
				   e->inner_method);
			break;
		case NAI_REALM_EAP_AUTH_CRED_TYPE:
			if (len < 1)
				break;
			e->cred_type = *pos;
			wpa_printf(MSG_DEBUG, "Credential Type: %u",
				   e->cred_type);
			break;
		case NAI_REALM_EAP_AUTH_TUNNELED_CRED_TYPE:
			if (len < 1)
				break;
			e->tunneled_cred_type = *pos;
			wpa_printf(MSG_DEBUG, "Tunneled EAP Method Credential "
				   "Type: %u", e->tunneled_cred_type);
			break;
		default:
			wpa_printf(MSG_DEBUG, "Unsupported Authentication "
				   "Parameter: id=%u len=%u", id, len);
			wpa_hexdump(MSG_DEBUG, "Authentication Parameter "
				    "Value", pos, len);
			break;
		}

		pos += len;
	}

	return e_end;
}


static const u8 * nai_realm_parse_realm(struct nai_realm *r, const u8 *pos,
					const u8 *end)
{
	u16 len;
	const u8 *f_end;
	u8 realm_len, e;

	if (end - pos < 4) {
		wpa_printf(MSG_DEBUG, "No room for NAI Realm Data "
			   "fixed fields");
		return NULL;
	}

	len = WPA_GET_LE16(pos); /* NAI Realm Data field Length */
	pos += 2;
	if (pos + len > end || len < 3) {
		wpa_printf(MSG_DEBUG, "No room for NAI Realm Data "
			   "(len=%u; left=%u)",
			   len, (unsigned int) (end - pos));
		return NULL;
	}
	f_end = pos + len;

	r->encoding = *pos++;
	realm_len = *pos++;
	if (pos + realm_len > f_end) {
		wpa_printf(MSG_DEBUG, "No room for NAI Realm "
			   "(len=%u; left=%u)",
			   realm_len, (unsigned int) (f_end - pos));
		return NULL;
	}
	wpa_hexdump_ascii(MSG_DEBUG, "NAI Realm", pos, realm_len);
	r->realm = os_malloc(realm_len + 1);
	if (r->realm == NULL)
		return NULL;
	os_memcpy(r->realm, pos, realm_len);
	r->realm[realm_len] = '\0';
	pos += realm_len;

	if (pos + 1 > f_end) {
		wpa_printf(MSG_DEBUG, "No room for EAP Method Count");
		return NULL;
	}
	r->eap_count = *pos++;
	wpa_printf(MSG_DEBUG, "EAP Count: %u", r->eap_count);
	if (pos + r->eap_count * 3 > f_end) {
		wpa_printf(MSG_DEBUG, "No room for EAP Methods");
		return NULL;
	}
	r->eap = os_calloc(r->eap_count, sizeof(struct nai_realm_eap));
	if (r->eap == NULL)
		return NULL;

	for (e = 0; e < r->eap_count; e++) {
		pos = nai_realm_parse_eap(&r->eap[e], pos, f_end);
		if (pos == NULL)
			return NULL;
	}

	return f_end;
}


static struct nai_realm * nai_realm_parse(struct wpabuf *anqp, u16 *count)
{
	struct nai_realm *realm;
	const u8 *pos, *end;
	u16 i, num;

	if (anqp == NULL || wpabuf_len(anqp) < 2)
		return NULL;

	pos = wpabuf_head_u8(anqp);
	end = pos + wpabuf_len(anqp);
	num = WPA_GET_LE16(pos);
	wpa_printf(MSG_DEBUG, "NAI Realm Count: %u", num);
	pos += 2;

	if (num * 5 > end - pos) {
		wpa_printf(MSG_DEBUG, "Invalid NAI Realm Count %u - not "
			   "enough data (%u octets) for that many realms",
			   num, (unsigned int) (end - pos));
		return NULL;
	}

	realm = os_calloc(num, sizeof(struct nai_realm));
	if (realm == NULL)
		return NULL;

	for (i = 0; i < num; i++) {
		pos = nai_realm_parse_realm(&realm[i], pos, end);
		if (pos == NULL) {
			nai_realm_free(realm, num);
			return NULL;
		}
	}

	*count = num;
	return realm;
}


static int nai_realm_match(struct nai_realm *realm, const char *home_realm)
{
	char *tmp, *pos, *end;
	int match = 0;

	if (realm->realm == NULL || home_realm == NULL)
		return 0;

	if (os_strchr(realm->realm, ';') == NULL)
		return os_strcasecmp(realm->realm, home_realm) == 0;

	tmp = os_strdup(realm->realm);
	if (tmp == NULL)
		return 0;

	pos = tmp;
	while (*pos) {
		end = os_strchr(pos, ';');
		if (end)
			*end = '\0';
		if (os_strcasecmp(pos, home_realm) == 0) {
			match = 1;
			break;
		}
		if (end == NULL)
			break;
		pos = end + 1;
	}

	os_free(tmp);

	return match;
}


static int nai_realm_cred_username(struct nai_realm_eap *eap)
{
	if (eap_get_name(EAP_VENDOR_IETF, eap->method) == NULL)
		return 0; /* method not supported */

	if (eap->method != EAP_TYPE_TTLS && eap->method != EAP_TYPE_PEAP) {
		/* Only tunneled methods with username/password supported */
		return 0;
	}

	if (eap->method == EAP_TYPE_PEAP) {
		if (eap->inner_method &&
		    eap_get_name(EAP_VENDOR_IETF, eap->inner_method) == NULL)
			return 0;
		if (!eap->inner_method &&
		    eap_get_name(EAP_VENDOR_IETF, EAP_TYPE_MSCHAPV2) == NULL)
			return 0;
	}

	if (eap->method == EAP_TYPE_TTLS) {
		if (eap->inner_method == 0 && eap->inner_non_eap == 0)
			return 1; /* Assume TTLS/MSCHAPv2 is used */
		if (eap->inner_method &&
		    eap_get_name(EAP_VENDOR_IETF, eap->inner_method) == NULL)
			return 0;
		if (eap->inner_non_eap &&
		    eap->inner_non_eap != NAI_REALM_INNER_NON_EAP_PAP &&
		    eap->inner_non_eap != NAI_REALM_INNER_NON_EAP_CHAP &&
		    eap->inner_non_eap != NAI_REALM_INNER_NON_EAP_MSCHAP &&
		    eap->inner_non_eap != NAI_REALM_INNER_NON_EAP_MSCHAPV2)
			return 0;
	}

	if (eap->inner_method &&
	    eap->inner_method != EAP_TYPE_GTC &&
	    eap->inner_method != EAP_TYPE_MSCHAPV2)
		return 0;

	return 1;
}


static int nai_realm_cred_cert(struct nai_realm_eap *eap)
{
	if (eap_get_name(EAP_VENDOR_IETF, eap->method) == NULL)
		return 0; /* method not supported */

	if (eap->method != EAP_TYPE_TLS) {
		/* Only EAP-TLS supported for credential authentication */
		return 0;
	}

	return 1;
}


static struct nai_realm_eap * nai_realm_find_eap(struct wpa_cred *cred,
						 struct nai_realm *realm)
{
	u8 e;

	if (cred == NULL ||
	    cred->username == NULL ||
	    cred->username[0] == '\0' ||
	    ((cred->password == NULL ||
	      cred->password[0] == '\0') &&
	     (cred->private_key == NULL ||
	      cred->private_key[0] == '\0')))
		return NULL;

	for (e = 0; e < realm->eap_count; e++) {
		struct nai_realm_eap *eap = &realm->eap[e];
		if (cred->password && cred->password[0] &&
		    nai_realm_cred_username(eap))
			return eap;
		if (cred->private_key && cred->private_key[0] &&
		    nai_realm_cred_cert(eap))
			return eap;
	}

	return NULL;
}


#ifdef INTERWORKING_3GPP

static int plmn_id_match(struct wpabuf *anqp, const char *imsi, int mnc_len)
{
	u8 plmn[3];
	const u8 *pos, *end;
	u8 udhl;

	/* See Annex A of 3GPP TS 24.234 v8.1.0 for description */
	plmn[0] = (imsi[0] - '0') | ((imsi[1] - '0') << 4);
	plmn[1] = imsi[2] - '0';
	/* default to MNC length 3 if unknown */
	if (mnc_len != 2)
		plmn[1] |= (imsi[5] - '0') << 4;
	else
		plmn[1] |= 0xf0;
	plmn[2] = (imsi[3] - '0') | ((imsi[4] - '0') << 4);

	if (anqp == NULL)
		return 0;
	pos = wpabuf_head_u8(anqp);
	end = pos + wpabuf_len(anqp);
	if (pos + 2 > end)
		return 0;
	if (*pos != 0) {
		wpa_printf(MSG_DEBUG, "Unsupported GUD version 0x%x", *pos);
		return 0;
	}
	pos++;
	udhl = *pos++;
	if (pos + udhl > end) {
		wpa_printf(MSG_DEBUG, "Invalid UDHL");
		return 0;
	}
	end = pos + udhl;

	while (pos + 2 <= end) {
		u8 iei, len;
		const u8 *l_end;
		iei = *pos++;
		len = *pos++ & 0x7f;
		if (pos + len > end)
			break;
		l_end = pos + len;

		if (iei == 0 && len > 0) {
			/* PLMN List */
			u8 num, i;
			num = *pos++;
			for (i = 0; i < num; i++) {
				if (pos + 3 > end)
					break;
				if (os_memcmp(pos, plmn, 3) == 0)
					return 1; /* Found matching PLMN */
				pos += 3;
			}
		}

		pos = l_end;
	}

	return 0;
}


static int build_root_nai(char *nai, size_t nai_len, const char *imsi,
			  size_t mnc_len, char prefix)
{
	const char *sep, *msin;
	char *end, *pos;
	size_t msin_len, plmn_len;

	/*
	 * TS 23.003, Clause 14 (3GPP to WLAN Interworking)
	 * Root NAI:
	 * <aka:0|sim:1><IMSI>@wlan.mnc<MNC>.mcc<MCC>.3gppnetwork.org
	 * <MNC> is zero-padded to three digits in case two-digit MNC is used
	 */

	if (imsi == NULL || os_strlen(imsi) > 16) {
		wpa_printf(MSG_DEBUG, "No valid IMSI available");
		return -1;
	}
	sep = os_strchr(imsi, '-');
	if (sep) {
		plmn_len = sep - imsi;
		msin = sep + 1;
	} else if (mnc_len && os_strlen(imsi) >= 3 + mnc_len) {
		plmn_len = 3 + mnc_len;
		msin = imsi + plmn_len;
	} else
		return -1;
	if (plmn_len != 5 && plmn_len != 6)
		return -1;
	msin_len = os_strlen(msin);

	pos = nai;
	end = nai + nai_len;
	if (prefix)
		*pos++ = prefix;
	os_memcpy(pos, imsi, plmn_len);
	pos += plmn_len;
	os_memcpy(pos, msin, msin_len);
	pos += msin_len;
	pos += os_snprintf(pos, end - pos, "@wlan.mnc");
	if (plmn_len == 5) {
		*pos++ = '0';
		*pos++ = imsi[3];
		*pos++ = imsi[4];
	} else {
		*pos++ = imsi[3];
		*pos++ = imsi[4];
		*pos++ = imsi[5];
	}
	pos += os_snprintf(pos, end - pos, ".mcc%c%c%c.3gppnetwork.org",
			   imsi[0], imsi[1], imsi[2]);

	return 0;
}


static int set_root_nai(struct wpa_ssid *ssid, const char *imsi, char prefix)
{
	char nai[100];
	if (build_root_nai(nai, sizeof(nai), imsi, 0, prefix) < 0)
		return -1;
	return wpa_config_set_quoted(ssid, "identity", nai);
}

#endif /* INTERWORKING_3GPP */


static int interworking_set_hs20_params(struct wpa_supplicant *wpa_s,
					struct wpa_ssid *ssid)
{
	if (wpa_config_set(ssid, "key_mgmt",
			   wpa_s->conf->pmf != NO_MGMT_FRAME_PROTECTION ?
			   "WPA-EAP WPA-EAP-SHA256" : "WPA-EAP", 0) < 0)
		return -1;
	if (wpa_config_set(ssid, "proto", "RSN", 0) < 0)
		return -1;
	if (wpa_config_set(ssid, "pairwise", "CCMP", 0) < 0)
		return -1;
	return 0;
}


static int interworking_connect_3gpp(struct wpa_supplicant *wpa_s,
				     struct wpa_bss *bss)
{
#ifdef INTERWORKING_3GPP
	struct wpa_cred *cred;
	struct wpa_ssid *ssid;
	const u8 *ie;
	int eap_type;
	int res;
	char prefix;

	if (bss->anqp == NULL || bss->anqp->anqp_3gpp == NULL)
		return -1;

	for (cred = wpa_s->conf->cred; cred; cred = cred->next) {
		char *sep;
		const char *imsi;
		int mnc_len;

#ifdef PCSC_FUNCS
		if (cred->pcsc && wpa_s->conf->pcsc_reader && wpa_s->scard &&
		    wpa_s->imsi[0]) {
			imsi = wpa_s->imsi;
			mnc_len = wpa_s->mnc_len;
			goto compare;
		}
#endif /* PCSC_FUNCS */

		if (cred->imsi == NULL || !cred->imsi[0] ||
		    cred->milenage == NULL || !cred->milenage[0])
			continue;

		sep = os_strchr(cred->imsi, '-');
		if (sep == NULL ||
		    (sep - cred->imsi != 5 && sep - cred->imsi != 6))
			continue;
		mnc_len = sep - cred->imsi - 3;
		imsi = cred->imsi;

#ifdef PCSC_FUNCS
	compare:
#endif /* PCSC_FUNCS */
		if (plmn_id_match(bss->anqp->anqp_3gpp, imsi, mnc_len))
			break;
	}
	if (cred == NULL)
		return -1;

	ie = wpa_bss_get_ie(bss, WLAN_EID_SSID);
	if (ie == NULL)
		return -1;
	wpa_printf(MSG_DEBUG, "Interworking: Connect with " MACSTR " (3GPP)",
		   MAC2STR(bss->bssid));

	ssid = wpa_config_add_network(wpa_s->conf);
	if (ssid == NULL)
		return -1;
	ssid->parent_cred = cred;

	wpas_notify_network_added(wpa_s, ssid);
	wpa_config_set_network_defaults(ssid);
	ssid->priority = cred->priority;
	ssid->temporary = 1;
	ssid->ssid = os_zalloc(ie[1] + 1);
	if (ssid->ssid == NULL)
		goto fail;
	os_memcpy(ssid->ssid, ie + 2, ie[1]);
	ssid->ssid_len = ie[1];

	if (interworking_set_hs20_params(wpa_s, ssid) < 0)
		goto fail;

	eap_type = EAP_TYPE_SIM;
	if (cred->pcsc && wpa_s->scard && scard_supports_umts(wpa_s->scard))
		eap_type = EAP_TYPE_AKA;
	if (cred->eap_method && cred->eap_method[0].vendor == EAP_VENDOR_IETF) {
		if (cred->eap_method[0].method == EAP_TYPE_SIM ||
		    cred->eap_method[0].method == EAP_TYPE_AKA ||
		    cred->eap_method[0].method == EAP_TYPE_AKA_PRIME)
			eap_type = cred->eap_method[0].method;
	}

	switch (eap_type) {
	case EAP_TYPE_SIM:
		prefix = '1';
		res = wpa_config_set(ssid, "eap", "SIM", 0);
		break;
	case EAP_TYPE_AKA:
		prefix = '0';
		res = wpa_config_set(ssid, "eap", "AKA", 0);
		break;
	case EAP_TYPE_AKA_PRIME:
		prefix = '6';
		res = wpa_config_set(ssid, "eap", "AKA'", 0);
		break;
	default:
		res = -1;
		break;
	}
	if (res < 0) {
		wpa_printf(MSG_DEBUG, "Selected EAP method (%d) not supported",
			   eap_type);
		goto fail;
	}

	if (!cred->pcsc && set_root_nai(ssid, cred->imsi, prefix) < 0) {
		wpa_printf(MSG_DEBUG, "Failed to set Root NAI");
		goto fail;
	}

	if (cred->milenage && cred->milenage[0]) {
		if (wpa_config_set_quoted(ssid, "password",
					  cred->milenage) < 0)
			goto fail;
	} else if (cred->pcsc) {
		if (wpa_config_set_quoted(ssid, "pcsc", "") < 0)
			goto fail;
		if (wpa_s->conf->pcsc_pin &&
		    wpa_config_set_quoted(ssid, "pin", wpa_s->conf->pcsc_pin)
		    < 0)
			goto fail;
	}

	if (cred->password && cred->password[0] &&
	    wpa_config_set_quoted(ssid, "password", cred->password) < 0)
		goto fail;

	wpa_config_update_prio_list(wpa_s->conf);
	interworking_reconnect(wpa_s);

	return 0;

fail:
	wpas_notify_network_removed(wpa_s, ssid);
	wpa_config_remove_network(wpa_s->conf, ssid->id);
#endif /* INTERWORKING_3GPP */
	return -1;
}


static int roaming_consortium_element_match(const u8 *ie, const u8 *rc_id,
					    size_t rc_len)
{
	const u8 *pos, *end;
	u8 lens;

	if (ie == NULL)
		return 0;

	pos = ie + 2;
	end = ie + 2 + ie[1];

	/* Roaming Consortium element:
	 * Number of ANQP OIs
	 * OI #1 and #2 lengths
	 * OI #1, [OI #2], [OI #3]
	 */

	if (pos + 2 > end)
		return 0;

	pos++; /* skip Number of ANQP OIs */
	lens = *pos++;
	if (pos + (lens & 0x0f) + (lens >> 4) > end)
		return 0;

	if ((lens & 0x0f) == rc_len && os_memcmp(pos, rc_id, rc_len) == 0)
		return 1;
	pos += lens & 0x0f;

	if ((lens >> 4) == rc_len && os_memcmp(pos, rc_id, rc_len) == 0)
		return 1;
	pos += lens >> 4;

	if (pos < end && (size_t) (end - pos) == rc_len &&
	    os_memcmp(pos, rc_id, rc_len) == 0)
		return 1;

	return 0;
}


static int roaming_consortium_anqp_match(const struct wpabuf *anqp,
					 const u8 *rc_id, size_t rc_len)
{
	const u8 *pos, *end;
	u8 len;

	if (anqp == NULL)
		return 0;

	pos = wpabuf_head(anqp);
	end = pos + wpabuf_len(anqp);

	/* Set of <OI Length, OI> duples */
	while (pos < end) {
		len = *pos++;
		if (pos + len > end)
			break;
		if (len == rc_len && os_memcmp(pos, rc_id, rc_len) == 0)
			return 1;
		pos += len;
	}

	return 0;
}


static int roaming_consortium_match(const u8 *ie, const struct wpabuf *anqp,
				    const u8 *rc_id, size_t rc_len)
{
	return roaming_consortium_element_match(ie, rc_id, rc_len) ||
		roaming_consortium_anqp_match(anqp, rc_id, rc_len);
}


static int cred_excluded_ssid(struct wpa_cred *cred, struct wpa_bss *bss)
{
	size_t i;

	if (!cred->excluded_ssid)
		return 0;

	for (i = 0; i < cred->num_excluded_ssid; i++) {
		struct excluded_ssid *e = &cred->excluded_ssid[i];
		if (bss->ssid_len == e->ssid_len &&
		    os_memcmp(bss->ssid, e->ssid, e->ssid_len) == 0)
			return 1;
	}

	return 0;
}


static struct wpa_cred * interworking_credentials_available_roaming_consortium(
	struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	struct wpa_cred *cred, *selected = NULL;
	const u8 *ie;

	ie = wpa_bss_get_ie(bss, WLAN_EID_ROAMING_CONSORTIUM);

	if (ie == NULL &&
	    (bss->anqp == NULL || bss->anqp->roaming_consortium == NULL))
		return NULL;

	if (wpa_s->conf->cred == NULL)
		return NULL;

	for (cred = wpa_s->conf->cred; cred; cred = cred->next) {
		if (cred->roaming_consortium_len == 0)
			continue;

		if (!roaming_consortium_match(ie,
					      bss->anqp ?
					      bss->anqp->roaming_consortium :
					      NULL,
					      cred->roaming_consortium,
					      cred->roaming_consortium_len))
			continue;

		if (cred_excluded_ssid(cred, bss))
			continue;

		if (selected == NULL ||
		    selected->priority < cred->priority)
			selected = cred;
	}

	return selected;
}


static int interworking_set_eap_params(struct wpa_ssid *ssid,
				       struct wpa_cred *cred, int ttls)
{
	if (cred->eap_method) {
		ttls = cred->eap_method->vendor == EAP_VENDOR_IETF &&
			cred->eap_method->method == EAP_TYPE_TTLS;

		os_free(ssid->eap.eap_methods);
		ssid->eap.eap_methods =
			os_malloc(sizeof(struct eap_method_type) * 2);
		if (ssid->eap.eap_methods == NULL)
			return -1;
		os_memcpy(ssid->eap.eap_methods, cred->eap_method,
			  sizeof(*cred->eap_method));
		ssid->eap.eap_methods[1].vendor = EAP_VENDOR_IETF;
		ssid->eap.eap_methods[1].method = EAP_TYPE_NONE;
	}

	if (ttls && cred->username && cred->username[0]) {
		const char *pos;
		char *anon;
		/* Use anonymous NAI in Phase 1 */
		pos = os_strchr(cred->username, '@');
		if (pos) {
			size_t buflen = 9 + os_strlen(pos) + 1;
			anon = os_malloc(buflen);
			if (anon == NULL)
				return -1;
			os_snprintf(anon, buflen, "anonymous%s", pos);
		} else if (cred->realm) {
			size_t buflen = 10 + os_strlen(cred->realm) + 1;
			anon = os_malloc(buflen);
			if (anon == NULL)
				return -1;
			os_snprintf(anon, buflen, "anonymous@%s", cred->realm);
		} else {
			anon = os_strdup("anonymous");
			if (anon == NULL)
				return -1;
		}
		if (wpa_config_set_quoted(ssid, "anonymous_identity", anon) <
		    0) {
			os_free(anon);
			return -1;
		}
		os_free(anon);
	}

	if (cred->username && cred->username[0] &&
	    wpa_config_set_quoted(ssid, "identity", cred->username) < 0)
		return -1;

	if (cred->password && cred->password[0]) {
		if (cred->ext_password &&
		    wpa_config_set(ssid, "password", cred->password, 0) < 0)
			return -1;
		if (!cred->ext_password &&
		    wpa_config_set_quoted(ssid, "password", cred->password) <
		    0)
			return -1;
	}

	if (cred->client_cert && cred->client_cert[0] &&
	    wpa_config_set_quoted(ssid, "client_cert", cred->client_cert) < 0)
		return -1;

#ifdef ANDROID
	if (cred->private_key &&
	    os_strncmp(cred->private_key, "keystore://", 11) == 0) {
		/* Use OpenSSL engine configuration for Android keystore */
		if (wpa_config_set_quoted(ssid, "engine_id", "keystore") < 0 ||
		    wpa_config_set_quoted(ssid, "key_id",
					  cred->private_key + 11) < 0 ||
		    wpa_config_set(ssid, "engine", "1", 0) < 0)
			return -1;
	} else
#endif /* ANDROID */
	if (cred->private_key && cred->private_key[0] &&
	    wpa_config_set_quoted(ssid, "private_key", cred->private_key) < 0)
		return -1;

	if (cred->private_key_passwd && cred->private_key_passwd[0] &&
	    wpa_config_set_quoted(ssid, "private_key_passwd",
				  cred->private_key_passwd) < 0)
		return -1;

	if (cred->phase1) {
		os_free(ssid->eap.phase1);
		ssid->eap.phase1 = os_strdup(cred->phase1);
	}
	if (cred->phase2) {
		os_free(ssid->eap.phase2);
		ssid->eap.phase2 = os_strdup(cred->phase2);
	}

	if (cred->ca_cert && cred->ca_cert[0] &&
	    wpa_config_set_quoted(ssid, "ca_cert", cred->ca_cert) < 0)
		return -1;

	return 0;
}


static int interworking_connect_roaming_consortium(
	struct wpa_supplicant *wpa_s, struct wpa_cred *cred,
	struct wpa_bss *bss, const u8 *ssid_ie)
{
	struct wpa_ssid *ssid;

	wpa_printf(MSG_DEBUG, "Interworking: Connect with " MACSTR " based on "
		   "roaming consortium match", MAC2STR(bss->bssid));

	ssid = wpa_config_add_network(wpa_s->conf);
	if (ssid == NULL)
		return -1;
	ssid->parent_cred = cred;
	wpas_notify_network_added(wpa_s, ssid);
	wpa_config_set_network_defaults(ssid);
	ssid->priority = cred->priority;
	ssid->temporary = 1;
	ssid->ssid = os_zalloc(ssid_ie[1] + 1);
	if (ssid->ssid == NULL)
		goto fail;
	os_memcpy(ssid->ssid, ssid_ie + 2, ssid_ie[1]);
	ssid->ssid_len = ssid_ie[1];

	if (interworking_set_hs20_params(wpa_s, ssid) < 0)
		goto fail;

	if (cred->eap_method == NULL) {
		wpa_printf(MSG_DEBUG, "Interworking: No EAP method set for "
			   "credential using roaming consortium");
		goto fail;
	}

	if (interworking_set_eap_params(
		    ssid, cred,
		    cred->eap_method->vendor == EAP_VENDOR_IETF &&
		    cred->eap_method->method == EAP_TYPE_TTLS) < 0)
		goto fail;

	wpa_config_update_prio_list(wpa_s->conf);
	interworking_reconnect(wpa_s);

	return 0;

fail:
	wpas_notify_network_removed(wpa_s, ssid);
	wpa_config_remove_network(wpa_s->conf, ssid->id);
	return -1;
}


int interworking_connect(struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	struct wpa_cred *cred;
	struct wpa_ssid *ssid;
	struct nai_realm *realm;
	struct nai_realm_eap *eap = NULL;
	u16 count, i;
	char buf[100];
	const u8 *ie;

	if (wpa_s->conf->cred == NULL || bss == NULL)
		return -1;
	ie = wpa_bss_get_ie(bss, WLAN_EID_SSID);
	if (ie == NULL || ie[1] == 0) {
		wpa_printf(MSG_DEBUG, "Interworking: No SSID known for "
			   MACSTR, MAC2STR(bss->bssid));
		return -1;
	}

	if (!wpa_bss_get_ie(bss, WLAN_EID_RSN)) {
		/*
		 * We currently support only HS 2.0 networks and those are
		 * required to use WPA2-Enterprise.
		 */
		wpa_printf(MSG_DEBUG, "Interworking: Network does not use "
			   "RSN");
		return -1;
	}

	cred = interworking_credentials_available_roaming_consortium(wpa_s,
								     bss);
	if (cred)
		return interworking_connect_roaming_consortium(wpa_s, cred,
							       bss, ie);

	realm = nai_realm_parse(bss->anqp ? bss->anqp->nai_realm : NULL,
				&count);
	if (realm == NULL) {
		wpa_printf(MSG_DEBUG, "Interworking: Could not parse NAI "
			   "Realm list from " MACSTR, MAC2STR(bss->bssid));
		count = 0;
	}

	for (cred = wpa_s->conf->cred; cred; cred = cred->next) {
		for (i = 0; i < count; i++) {
			if (!nai_realm_match(&realm[i], cred->realm))
				continue;
			eap = nai_realm_find_eap(cred, &realm[i]);
			if (eap)
				break;
		}
		if (eap)
			break;
	}

	if (!eap) {
		if (interworking_connect_3gpp(wpa_s, bss) == 0) {
			if (realm)
				nai_realm_free(realm, count);
			return 0;
		}

		wpa_printf(MSG_DEBUG, "Interworking: No matching credentials "
			   "and EAP method found for " MACSTR,
			   MAC2STR(bss->bssid));
		nai_realm_free(realm, count);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "Interworking: Connect with " MACSTR,
		   MAC2STR(bss->bssid));

	ssid = wpa_config_add_network(wpa_s->conf);
	if (ssid == NULL) {
		nai_realm_free(realm, count);
		return -1;
	}
	ssid->parent_cred = cred;
	wpas_notify_network_added(wpa_s, ssid);
	wpa_config_set_network_defaults(ssid);
	ssid->priority = cred->priority;
	ssid->temporary = 1;
	ssid->ssid = os_zalloc(ie[1] + 1);
	if (ssid->ssid == NULL)
		goto fail;
	os_memcpy(ssid->ssid, ie + 2, ie[1]);
	ssid->ssid_len = ie[1];

	if (interworking_set_hs20_params(wpa_s, ssid) < 0)
		goto fail;

	if (wpa_config_set(ssid, "eap", eap_get_name(EAP_VENDOR_IETF,
						     eap->method), 0) < 0)
		goto fail;

	switch (eap->method) {
	case EAP_TYPE_TTLS:
		if (eap->inner_method) {
			os_snprintf(buf, sizeof(buf), "\"autheap=%s\"",
				    eap_get_name(EAP_VENDOR_IETF,
						 eap->inner_method));
			if (wpa_config_set(ssid, "phase2", buf, 0) < 0)
				goto fail;
			break;
		}
		switch (eap->inner_non_eap) {
		case NAI_REALM_INNER_NON_EAP_PAP:
			if (wpa_config_set(ssid, "phase2", "\"auth=PAP\"", 0) <
			    0)
				goto fail;
			break;
		case NAI_REALM_INNER_NON_EAP_CHAP:
			if (wpa_config_set(ssid, "phase2", "\"auth=CHAP\"", 0)
			    < 0)
				goto fail;
			break;
		case NAI_REALM_INNER_NON_EAP_MSCHAP:
			if (wpa_config_set(ssid, "phase2", "\"auth=MSCHAP\"",
					   0) < 0)
				goto fail;
			break;
		case NAI_REALM_INNER_NON_EAP_MSCHAPV2:
			if (wpa_config_set(ssid, "phase2", "\"auth=MSCHAPV2\"",
					   0) < 0)
				goto fail;
			break;
		default:
			/* EAP params were not set - assume TTLS/MSCHAPv2 */
			if (wpa_config_set(ssid, "phase2", "\"auth=MSCHAPV2\"",
					   0) < 0)
				goto fail;
			break;
		}
		break;
	case EAP_TYPE_PEAP:
		os_snprintf(buf, sizeof(buf), "\"auth=%s\"",
			    eap_get_name(EAP_VENDOR_IETF,
					 eap->inner_method ?
					 eap->inner_method :
					 EAP_TYPE_MSCHAPV2));
		if (wpa_config_set(ssid, "phase2", buf, 0) < 0)
			goto fail;
		break;
	case EAP_TYPE_TLS:
		break;
	}

	if (interworking_set_eap_params(ssid, cred,
					eap->method == EAP_TYPE_TTLS) < 0)
		goto fail;

	nai_realm_free(realm, count);

	wpa_config_update_prio_list(wpa_s->conf);
	interworking_reconnect(wpa_s);

	return 0;

fail:
	wpas_notify_network_removed(wpa_s, ssid);
	wpa_config_remove_network(wpa_s->conf, ssid->id);
	nai_realm_free(realm, count);
	return -1;
}


static struct wpa_cred * interworking_credentials_available_3gpp(
	struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	struct wpa_cred *cred, *selected = NULL;
	int ret;

#ifdef INTERWORKING_3GPP
	if (bss->anqp == NULL || bss->anqp->anqp_3gpp == NULL)
		return NULL;

	for (cred = wpa_s->conf->cred; cred; cred = cred->next) {
		char *sep;
		const char *imsi;
		int mnc_len;

#ifdef PCSC_FUNCS
		if (cred->pcsc && wpa_s->conf->pcsc_reader && wpa_s->scard &&
		    wpa_s->imsi[0]) {
			imsi = wpa_s->imsi;
			mnc_len = wpa_s->mnc_len;
			goto compare;
		}
#endif /* PCSC_FUNCS */

		if (cred->imsi == NULL || !cred->imsi[0] ||
		    cred->milenage == NULL || !cred->milenage[0])
			continue;

		sep = os_strchr(cred->imsi, '-');
		if (sep == NULL ||
		    (sep - cred->imsi != 5 && sep - cred->imsi != 6))
			continue;
		mnc_len = sep - cred->imsi - 3;
		imsi = cred->imsi;

#ifdef PCSC_FUNCS
	compare:
#endif /* PCSC_FUNCS */
		wpa_printf(MSG_DEBUG, "Interworking: Parsing 3GPP info from "
			   MACSTR, MAC2STR(bss->bssid));
		ret = plmn_id_match(bss->anqp->anqp_3gpp, imsi, mnc_len);
		wpa_printf(MSG_DEBUG, "PLMN match %sfound", ret ? "" : "not ");
		if (ret) {
			if (cred_excluded_ssid(cred, bss))
				continue;
			if (selected == NULL ||
			    selected->priority < cred->priority)
				selected = cred;
		}
	}
#endif /* INTERWORKING_3GPP */
	return selected;
}


static struct wpa_cred * interworking_credentials_available_realm(
	struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	struct wpa_cred *cred, *selected = NULL;
	struct nai_realm *realm;
	u16 count, i;

	if (bss->anqp == NULL || bss->anqp->nai_realm == NULL)
		return NULL;

	if (wpa_s->conf->cred == NULL)
		return NULL;

	wpa_printf(MSG_DEBUG, "Interworking: Parsing NAI Realm list from "
		   MACSTR, MAC2STR(bss->bssid));
	realm = nai_realm_parse(bss->anqp->nai_realm, &count);
	if (realm == NULL) {
		wpa_printf(MSG_DEBUG, "Interworking: Could not parse NAI "
			   "Realm list from " MACSTR, MAC2STR(bss->bssid));
		return NULL;
	}

	for (cred = wpa_s->conf->cred; cred; cred = cred->next) {
		if (cred->realm == NULL)
			continue;

		for (i = 0; i < count; i++) {
			if (!nai_realm_match(&realm[i], cred->realm))
				continue;
			if (nai_realm_find_eap(cred, &realm[i])) {
				if (cred_excluded_ssid(cred, bss))
					continue;
				if (selected == NULL ||
				    selected->priority < cred->priority)
					selected = cred;
				break;
			}
		}
	}

	nai_realm_free(realm, count);

	return selected;
}


static struct wpa_cred * interworking_credentials_available(
	struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	struct wpa_cred *cred, *cred2;

	cred = interworking_credentials_available_realm(wpa_s, bss);
	cred2 = interworking_credentials_available_3gpp(wpa_s, bss);
	if (cred && cred2 && cred2->priority >= cred->priority)
		cred = cred2;
	if (!cred)
		cred = cred2;

	cred2 = interworking_credentials_available_roaming_consortium(wpa_s,
								      bss);
	if (cred && cred2 && cred2->priority >= cred->priority)
		cred = cred2;
	if (!cred)
		cred = cred2;

	return cred;
}


static int domain_name_list_contains(struct wpabuf *domain_names,
				     const char *domain)
{
	const u8 *pos, *end;
	size_t len;

	len = os_strlen(domain);
	pos = wpabuf_head(domain_names);
	end = pos + wpabuf_len(domain_names);

	while (pos + 1 < end) {
		if (pos + 1 + pos[0] > end)
			break;

		wpa_hexdump_ascii(MSG_DEBUG, "Interworking: AP domain name",
				  pos + 1, pos[0]);
		if (pos[0] == len &&
		    os_strncasecmp(domain, (const char *) (pos + 1), len) == 0)
			return 1;

		pos += 1 + pos[0];
	}

	return 0;
}


int interworking_home_sp_cred(struct wpa_supplicant *wpa_s,
			      struct wpa_cred *cred,
			      struct wpabuf *domain_names)
{
#ifdef INTERWORKING_3GPP
	char nai[100], *realm;

	char *imsi = NULL;
	int mnc_len = 0;
	if (cred->imsi)
		imsi = cred->imsi;
#ifdef CONFIG_PCSC
	else if (cred->pcsc && wpa_s->conf->pcsc_reader &&
		 wpa_s->scard && wpa_s->imsi[0]) {
		imsi = wpa_s->imsi;
		mnc_len = wpa_s->mnc_len;
	}
#endif /* CONFIG_PCSC */
	if (domain_names &&
	    imsi && build_root_nai(nai, sizeof(nai), imsi, mnc_len, 0) == 0) {
		realm = os_strchr(nai, '@');
		if (realm)
			realm++;
		wpa_printf(MSG_DEBUG, "Interworking: Search for match "
			   "with SIM/USIM domain %s", realm);
		if (realm &&
		    domain_name_list_contains(domain_names, realm))
			return 1;
	}
#endif /* INTERWORKING_3GPP */

	if (domain_names == NULL || cred->domain == NULL)
		return 0;

	wpa_printf(MSG_DEBUG, "Interworking: Search for match with "
		   "home SP FQDN %s", cred->domain);
	if (domain_name_list_contains(domain_names, cred->domain))
		return 1;

	return 0;
}


static int interworking_home_sp(struct wpa_supplicant *wpa_s,
				struct wpabuf *domain_names)
{
	struct wpa_cred *cred;

	if (domain_names == NULL || wpa_s->conf->cred == NULL)
		return -1;

	for (cred = wpa_s->conf->cred; cred; cred = cred->next) {
		int res = interworking_home_sp_cred(wpa_s, cred, domain_names);
		if (res)
			return res;
	}

	return 0;
}


static int interworking_find_network_match(struct wpa_supplicant *wpa_s)
{
	struct wpa_bss *bss;
	struct wpa_ssid *ssid;

	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
			if (wpas_network_disabled(wpa_s, ssid) ||
			    ssid->mode != WPAS_MODE_INFRA)
				continue;
			if (ssid->ssid_len != bss->ssid_len ||
			    os_memcmp(ssid->ssid, bss->ssid, ssid->ssid_len) !=
			    0)
				continue;
			/*
			 * TODO: Consider more accurate matching of security
			 * configuration similarly to what is done in events.c
			 */
			return 1;
		}
	}

	return 0;
}


static void interworking_select_network(struct wpa_supplicant *wpa_s)
{
	struct wpa_bss *bss, *selected = NULL, *selected_home = NULL;
	int selected_prio = -999999, selected_home_prio = -999999;
	unsigned int count = 0;
	const char *type;
	int res;
	struct wpa_cred *cred;

	wpa_s->network_select = 0;

	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		cred = interworking_credentials_available(wpa_s, bss);
		if (!cred)
			continue;
		if (!wpa_bss_get_ie(bss, WLAN_EID_RSN)) {
			/*
			 * We currently support only HS 2.0 networks and those
			 * are required to use WPA2-Enterprise.
			 */
			wpa_printf(MSG_DEBUG, "Interworking: Credential match "
				   "with " MACSTR " but network does not use "
				   "RSN", MAC2STR(bss->bssid));
			continue;
		}
		count++;
		res = interworking_home_sp(wpa_s, bss->anqp ?
					   bss->anqp->domain_name : NULL);
		if (res > 0)
			type = "home";
		else if (res == 0)
			type = "roaming";
		else
			type = "unknown";
		wpa_msg(wpa_s, MSG_INFO, INTERWORKING_AP MACSTR " type=%s",
			MAC2STR(bss->bssid), type);
		if (wpa_s->auto_select ||
		    (wpa_s->conf->auto_interworking &&
		     wpa_s->auto_network_select)) {
			if (selected == NULL ||
			    cred->priority > selected_prio) {
				selected = bss;
				selected_prio = cred->priority;
			}
			if (res > 0 &&
			    (selected_home == NULL ||
			     cred->priority > selected_home_prio)) {
				selected_home = bss;
				selected_home_prio = cred->priority;
			}
		}
	}

	if (selected_home && selected_home != selected &&
	    selected_home_prio >= selected_prio) {
		/* Prefer network operated by the Home SP */
		selected = selected_home;
	}

	if (count == 0) {
		/*
		 * No matching network was found based on configured
		 * credentials. Check whether any of the enabled network blocks
		 * have matching APs.
		 */
		if (interworking_find_network_match(wpa_s)) {
			wpa_printf(MSG_DEBUG, "Interworking: Possible BSS "
				   "match for enabled network configurations");
			if (wpa_s->auto_select)
				interworking_reconnect(wpa_s);
			return;
		}

		if (wpa_s->auto_network_select) {
			wpa_printf(MSG_DEBUG, "Interworking: Continue "
				   "scanning after ANQP fetch");
			wpa_supplicant_req_scan(wpa_s, wpa_s->scan_interval,
						0);
			return;
		}

		wpa_msg(wpa_s, MSG_INFO, INTERWORKING_NO_MATCH "No network "
			"with matching credentials found");
	}

	if (selected)
		interworking_connect(wpa_s, selected);
}


static struct wpa_bss_anqp *
interworking_match_anqp_info(struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	struct wpa_bss *other;

	if (is_zero_ether_addr(bss->hessid))
		return NULL; /* Cannot be in the same homegenous ESS */

	dl_list_for_each(other, &wpa_s->bss, struct wpa_bss, list) {
		if (other == bss)
			continue;
		if (other->anqp == NULL)
			continue;
		if (other->anqp->roaming_consortium == NULL &&
		    other->anqp->nai_realm == NULL &&
		    other->anqp->anqp_3gpp == NULL &&
		    other->anqp->domain_name == NULL)
			continue;
		if (!(other->flags & WPA_BSS_ANQP_FETCH_TRIED))
			continue;
		if (os_memcmp(bss->hessid, other->hessid, ETH_ALEN) != 0)
			continue;
		if (bss->ssid_len != other->ssid_len ||
		    os_memcmp(bss->ssid, other->ssid, bss->ssid_len) != 0)
			continue;

		wpa_printf(MSG_DEBUG, "Interworking: Share ANQP data with "
			   "already fetched BSSID " MACSTR " and " MACSTR,
			   MAC2STR(other->bssid), MAC2STR(bss->bssid));
		other->anqp->users++;
		return other->anqp;
	}

	return NULL;
}


static void interworking_next_anqp_fetch(struct wpa_supplicant *wpa_s)
{
	struct wpa_bss *bss;
	int found = 0;
	const u8 *ie;

	if (eloop_terminated() || !wpa_s->fetch_anqp_in_progress)
		return;

	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (!(bss->caps & IEEE80211_CAP_ESS))
			continue;
		ie = wpa_bss_get_ie(bss, WLAN_EID_EXT_CAPAB);
		if (ie == NULL || ie[1] < 4 || !(ie[5] & 0x80))
			continue; /* AP does not support Interworking */

		if (!(bss->flags & WPA_BSS_ANQP_FETCH_TRIED)) {
			if (bss->anqp == NULL) {
				bss->anqp = interworking_match_anqp_info(wpa_s,
									 bss);
				if (bss->anqp) {
					/* Shared data already fetched */
					continue;
				}
				bss->anqp = wpa_bss_anqp_alloc();
				if (bss->anqp == NULL)
					break;
			}
			found++;
			bss->flags |= WPA_BSS_ANQP_FETCH_TRIED;
			wpa_msg(wpa_s, MSG_INFO, "Starting ANQP fetch for "
				MACSTR, MAC2STR(bss->bssid));
			interworking_anqp_send_req(wpa_s, bss);
			break;
		}
	}

	if (found == 0) {
		wpa_msg(wpa_s, MSG_INFO, "ANQP fetch completed");
		wpa_s->fetch_anqp_in_progress = 0;
		if (wpa_s->network_select)
			interworking_select_network(wpa_s);
	}
}


void interworking_start_fetch_anqp(struct wpa_supplicant *wpa_s)
{
	struct wpa_bss *bss;

	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list)
		bss->flags &= ~WPA_BSS_ANQP_FETCH_TRIED;

	wpa_s->fetch_anqp_in_progress = 1;
	interworking_next_anqp_fetch(wpa_s);
}


int interworking_fetch_anqp(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->fetch_anqp_in_progress || wpa_s->network_select)
		return 0;

	wpa_s->network_select = 0;
	wpa_s->fetch_all_anqp = 1;

	interworking_start_fetch_anqp(wpa_s);

	return 0;
}


void interworking_stop_fetch_anqp(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s->fetch_anqp_in_progress)
		return;

	wpa_s->fetch_anqp_in_progress = 0;
}


int anqp_send_req(struct wpa_supplicant *wpa_s, const u8 *dst,
		  u16 info_ids[], size_t num_ids)
{
	struct wpabuf *buf;
	int ret = 0;
	int freq;
	struct wpa_bss *bss;
	int res;

	freq = wpa_s->assoc_freq;
	bss = wpa_bss_get_bssid(wpa_s, dst);
	if (bss) {
		wpa_bss_anqp_unshare_alloc(bss);
		freq = bss->freq;
	}
	if (freq <= 0)
		return -1;

	wpa_printf(MSG_DEBUG, "ANQP: Query Request to " MACSTR " for %u id(s)",
		   MAC2STR(dst), (unsigned int) num_ids);

	buf = anqp_build_req(info_ids, num_ids, NULL);
	if (buf == NULL)
		return -1;

	res = gas_query_req(wpa_s->gas, dst, freq, buf, anqp_resp_cb, wpa_s);
	if (res < 0) {
		wpa_printf(MSG_DEBUG, "ANQP: Failed to send Query Request");
		ret = -1;
	} else
		wpa_printf(MSG_DEBUG, "ANQP: Query started with dialog token "
			   "%u", res);

	wpabuf_free(buf);
	return ret;
}


static void interworking_parse_rx_anqp_resp(struct wpa_supplicant *wpa_s,
					    const u8 *sa, u16 info_id,
					    const u8 *data, size_t slen)
{
	const u8 *pos = data;
	struct wpa_bss *bss = wpa_bss_get_bssid(wpa_s, sa);
	struct wpa_bss_anqp *anqp = NULL;
#ifdef CONFIG_HS20
	u8 type;
#endif /* CONFIG_HS20 */

	if (bss)
		anqp = bss->anqp;

	switch (info_id) {
	case ANQP_CAPABILITY_LIST:
		wpa_msg(wpa_s, MSG_INFO, "RX-ANQP " MACSTR
			" ANQP Capability list", MAC2STR(sa));
		break;
	case ANQP_VENUE_NAME:
		wpa_msg(wpa_s, MSG_INFO, "RX-ANQP " MACSTR
			" Venue Name", MAC2STR(sa));
		wpa_hexdump_ascii(MSG_DEBUG, "ANQP: Venue Name", pos, slen);
		if (anqp) {
			wpabuf_free(anqp->venue_name);
			anqp->venue_name = wpabuf_alloc_copy(pos, slen);
		}
		break;
	case ANQP_NETWORK_AUTH_TYPE:
		wpa_msg(wpa_s, MSG_INFO, "RX-ANQP " MACSTR
			" Network Authentication Type information",
			MAC2STR(sa));
		wpa_hexdump_ascii(MSG_DEBUG, "ANQP: Network Authentication "
				  "Type", pos, slen);
		if (anqp) {
			wpabuf_free(anqp->network_auth_type);
			anqp->network_auth_type = wpabuf_alloc_copy(pos, slen);
		}
		break;
	case ANQP_ROAMING_CONSORTIUM:
		wpa_msg(wpa_s, MSG_INFO, "RX-ANQP " MACSTR
			" Roaming Consortium list", MAC2STR(sa));
		wpa_hexdump_ascii(MSG_DEBUG, "ANQP: Roaming Consortium",
				  pos, slen);
		if (anqp) {
			wpabuf_free(anqp->roaming_consortium);
			anqp->roaming_consortium = wpabuf_alloc_copy(pos, slen);
		}
		break;
	case ANQP_IP_ADDR_TYPE_AVAILABILITY:
		wpa_msg(wpa_s, MSG_INFO, "RX-ANQP " MACSTR
			" IP Address Type Availability information",
			MAC2STR(sa));
		wpa_hexdump(MSG_MSGDUMP, "ANQP: IP Address Availability",
			    pos, slen);
		if (anqp) {
			wpabuf_free(anqp->ip_addr_type_availability);
			anqp->ip_addr_type_availability =
				wpabuf_alloc_copy(pos, slen);
		}
		break;
	case ANQP_NAI_REALM:
		wpa_msg(wpa_s, MSG_INFO, "RX-ANQP " MACSTR
			" NAI Realm list", MAC2STR(sa));
		wpa_hexdump_ascii(MSG_DEBUG, "ANQP: NAI Realm", pos, slen);
		if (anqp) {
			wpabuf_free(anqp->nai_realm);
			anqp->nai_realm = wpabuf_alloc_copy(pos, slen);
		}
		break;
	case ANQP_3GPP_CELLULAR_NETWORK:
		wpa_msg(wpa_s, MSG_INFO, "RX-ANQP " MACSTR
			" 3GPP Cellular Network information", MAC2STR(sa));
		wpa_hexdump_ascii(MSG_DEBUG, "ANQP: 3GPP Cellular Network",
				  pos, slen);
		if (anqp) {
			wpabuf_free(anqp->anqp_3gpp);
			anqp->anqp_3gpp = wpabuf_alloc_copy(pos, slen);
		}
		break;
	case ANQP_DOMAIN_NAME:
		wpa_msg(wpa_s, MSG_INFO, "RX-ANQP " MACSTR
			" Domain Name list", MAC2STR(sa));
		wpa_hexdump_ascii(MSG_MSGDUMP, "ANQP: Domain Name", pos, slen);
		if (anqp) {
			wpabuf_free(anqp->domain_name);
			anqp->domain_name = wpabuf_alloc_copy(pos, slen);
		}
		break;
	case ANQP_VENDOR_SPECIFIC:
		if (slen < 3)
			return;

		switch (WPA_GET_BE24(pos)) {
#ifdef CONFIG_HS20
		case OUI_WFA:
			pos += 3;
			slen -= 3;

			if (slen < 1)
				return;
			type = *pos++;
			slen--;

			switch (type) {
			case HS20_ANQP_OUI_TYPE:
				hs20_parse_rx_hs20_anqp_resp(wpa_s, sa, pos,
							     slen);
				break;
			default:
				wpa_printf(MSG_DEBUG, "HS20: Unsupported ANQP "
					   "vendor type %u", type);
				break;
			}
			break;
#endif /* CONFIG_HS20 */
		default:
			wpa_printf(MSG_DEBUG, "Interworking: Unsupported "
				   "vendor-specific ANQP OUI %06x",
				   WPA_GET_BE24(pos));
			return;
		}
		break;
	default:
		wpa_printf(MSG_DEBUG, "Interworking: Unsupported ANQP Info ID "
			   "%u", info_id);
		break;
	}
}


void anqp_resp_cb(void *ctx, const u8 *dst, u8 dialog_token,
		  enum gas_query_result result,
		  const struct wpabuf *adv_proto,
		  const struct wpabuf *resp, u16 status_code)
{
	struct wpa_supplicant *wpa_s = ctx;
	const u8 *pos;
	const u8 *end;
	u16 info_id;
	u16 slen;

	if (result != GAS_QUERY_SUCCESS)
		return;

	pos = wpabuf_head(adv_proto);
	if (wpabuf_len(adv_proto) < 4 || pos[0] != WLAN_EID_ADV_PROTO ||
	    pos[1] < 2 || pos[3] != ACCESS_NETWORK_QUERY_PROTOCOL) {
		wpa_printf(MSG_DEBUG, "ANQP: Unexpected Advertisement "
			   "Protocol in response");
		return;
	}

	pos = wpabuf_head(resp);
	end = pos + wpabuf_len(resp);

	while (pos < end) {
		if (pos + 4 > end) {
			wpa_printf(MSG_DEBUG, "ANQP: Invalid element");
			break;
		}
		info_id = WPA_GET_LE16(pos);
		pos += 2;
		slen = WPA_GET_LE16(pos);
		pos += 2;
		if (pos + slen > end) {
			wpa_printf(MSG_DEBUG, "ANQP: Invalid element length "
				   "for Info ID %u", info_id);
			break;
		}
		interworking_parse_rx_anqp_resp(wpa_s, dst, info_id, pos,
						slen);
		pos += slen;
	}
}


static void interworking_scan_res_handler(struct wpa_supplicant *wpa_s,
					  struct wpa_scan_results *scan_res)
{
	wpa_printf(MSG_DEBUG, "Interworking: Scan results available - start "
		   "ANQP fetch");
	interworking_start_fetch_anqp(wpa_s);
}


int interworking_select(struct wpa_supplicant *wpa_s, int auto_select)
{
	interworking_stop_fetch_anqp(wpa_s);
	wpa_s->network_select = 1;
	wpa_s->auto_network_select = 0;
	wpa_s->auto_select = !!auto_select;
	wpa_s->fetch_all_anqp = 0;
	wpa_printf(MSG_DEBUG, "Interworking: Start scan for network "
		   "selection");
	wpa_s->scan_res_handler = interworking_scan_res_handler;
	wpa_s->scan_req = MANUAL_SCAN_REQ;
	wpa_supplicant_req_scan(wpa_s, 0, 0);

	return 0;
}


static void gas_resp_cb(void *ctx, const u8 *addr, u8 dialog_token,
			enum gas_query_result result,
			const struct wpabuf *adv_proto,
			const struct wpabuf *resp, u16 status_code)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpa_msg(wpa_s, MSG_INFO, GAS_RESPONSE_INFO "addr=" MACSTR
		" dialog_token=%d status_code=%d resp_len=%d",
		MAC2STR(addr), dialog_token, status_code,
		resp ? (int) wpabuf_len(resp) : -1);
	if (!resp)
		return;

	wpabuf_free(wpa_s->last_gas_resp);
	wpa_s->last_gas_resp = wpabuf_dup(resp);
	if (wpa_s->last_gas_resp == NULL)
		return;
	os_memcpy(wpa_s->last_gas_addr, addr, ETH_ALEN);
	wpa_s->last_gas_dialog_token = dialog_token;
}


int gas_send_request(struct wpa_supplicant *wpa_s, const u8 *dst,
		     const struct wpabuf *adv_proto,
		     const struct wpabuf *query)
{
	struct wpabuf *buf;
	int ret = 0;
	int freq;
	struct wpa_bss *bss;
	int res;
	size_t len;
	u8 query_resp_len_limit = 0, pame_bi = 0;

	freq = wpa_s->assoc_freq;
	bss = wpa_bss_get_bssid(wpa_s, dst);
	if (bss)
		freq = bss->freq;
	if (freq <= 0)
		return -1;

	wpa_printf(MSG_DEBUG, "GAS request to " MACSTR " (freq %d MHz)",
		   MAC2STR(dst), freq);
	wpa_hexdump_buf(MSG_DEBUG, "Advertisement Protocol ID", adv_proto);
	wpa_hexdump_buf(MSG_DEBUG, "GAS Query", query);

	len = 3 + wpabuf_len(adv_proto) + 2;
	if (query)
		len += wpabuf_len(query);
	buf = gas_build_initial_req(0, len);
	if (buf == NULL)
		return -1;

	/* Advertisement Protocol IE */
	wpabuf_put_u8(buf, WLAN_EID_ADV_PROTO);
	wpabuf_put_u8(buf, 1 + wpabuf_len(adv_proto)); /* Length */
	wpabuf_put_u8(buf, (query_resp_len_limit & 0x7f) |
		      (pame_bi ? 0x80 : 0));
	wpabuf_put_buf(buf, adv_proto);

	/* GAS Query */
	if (query) {
		wpabuf_put_le16(buf, wpabuf_len(query));
		wpabuf_put_buf(buf, query);
	} else
		wpabuf_put_le16(buf, 0);

	res = gas_query_req(wpa_s->gas, dst, freq, buf, gas_resp_cb, wpa_s);
	if (res < 0) {
		wpa_printf(MSG_DEBUG, "GAS: Failed to send Query Request");
		ret = -1;
	} else
		wpa_printf(MSG_DEBUG, "GAS: Query started with dialog token "
			   "%u", res);

	wpabuf_free(buf);
	return ret;
}
