/*
 * Copyright (c) 2009, Atheros Communications, Inc.
 * Copyright (c) 2011-2013, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <sys/stat.h>

#include "common.h"
#include "eloop.h"
#include "common/ieee802_11_common.h"
#include "common/ieee802_11_defs.h"
#include "common/gas.h"
#include "common/wpa_ctrl.h"
#include "rsn_supp/wpa.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "config.h"
#include "scan.h"
#include "bss.h"
#include "blacklist.h"
#include "gas_query.h"
#include "interworking.h"
#include "hs20_supplicant.h"


#define OSU_MAX_ITEMS 10

struct osu_lang_string {
	char lang[4];
	char text[253];
};

struct osu_icon {
	u16 width;
	u16 height;
	char lang[4];
	char icon_type[256];
	char filename[256];
	unsigned int id;
	unsigned int failed:1;
};

struct osu_provider {
	u8 bssid[ETH_ALEN];
	u8 osu_ssid[SSID_MAX_LEN];
	u8 osu_ssid_len;
	char server_uri[256];
	u32 osu_methods; /* bit 0 = OMA-DM, bit 1 = SOAP-XML SPP */
	char osu_nai[256];
	struct osu_lang_string friendly_name[OSU_MAX_ITEMS];
	size_t friendly_name_count;
	struct osu_lang_string serv_desc[OSU_MAX_ITEMS];
	size_t serv_desc_count;
	struct osu_icon icon[OSU_MAX_ITEMS];
	size_t icon_count;
};


void wpas_hs20_add_indication(struct wpabuf *buf, int pps_mo_id)
{
	u8 conf;

	wpabuf_put_u8(buf, WLAN_EID_VENDOR_SPECIFIC);
	wpabuf_put_u8(buf, pps_mo_id >= 0 ? 7 : 5);
	wpabuf_put_be24(buf, OUI_WFA);
	wpabuf_put_u8(buf, HS20_INDICATION_OUI_TYPE);
	conf = HS20_VERSION;
	if (pps_mo_id >= 0)
		conf |= HS20_PPS_MO_ID_PRESENT;
	wpabuf_put_u8(buf, conf);
	if (pps_mo_id >= 0)
		wpabuf_put_le16(buf, pps_mo_id);
}


int is_hs20_network(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid,
		    struct wpa_bss *bss)
{
	if (!wpa_s->conf->hs20 || !ssid)
		return 0;

	if (ssid->parent_cred)
		return 1;

	if (bss && !wpa_bss_get_vendor_ie(bss, HS20_IE_VENDOR_TYPE))
		return 0;

	/*
	 * This may catch some non-Hotspot 2.0 cases, but it is safer to do that
	 * than cause Hotspot 2.0 connections without indication element getting
	 * added. Non-Hotspot 2.0 APs should ignore the unknown vendor element.
	 */

	if (!(ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X))
		return 0;
	if (!(ssid->pairwise_cipher & WPA_CIPHER_CCMP))
		return 0;
	if (ssid->proto != WPA_PROTO_RSN)
		return 0;

	return 1;
}


int hs20_get_pps_mo_id(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid)
{
	struct wpa_cred *cred;

	if (ssid == NULL)
		return 0;

	if (ssid->update_identifier)
		return ssid->update_identifier;

	if (ssid->parent_cred == NULL)
		return 0;

	for (cred = wpa_s->conf->cred; cred; cred = cred->next) {
		if (ssid->parent_cred == cred)
			return cred->update_identifier;
	}

	return 0;
}


void hs20_put_anqp_req(u32 stypes, const u8 *payload, size_t payload_len,
		       struct wpabuf *buf)
{
	u8 *len_pos;

	if (buf == NULL)
		return;

	len_pos = gas_anqp_add_element(buf, ANQP_VENDOR_SPECIFIC);
	wpabuf_put_be24(buf, OUI_WFA);
	wpabuf_put_u8(buf, HS20_ANQP_OUI_TYPE);
	if (stypes == BIT(HS20_STYPE_NAI_HOME_REALM_QUERY)) {
		wpabuf_put_u8(buf, HS20_STYPE_NAI_HOME_REALM_QUERY);
		wpabuf_put_u8(buf, 0); /* Reserved */
		if (payload)
			wpabuf_put_data(buf, payload, payload_len);
	} else if (stypes == BIT(HS20_STYPE_ICON_REQUEST)) {
		wpabuf_put_u8(buf, HS20_STYPE_ICON_REQUEST);
		wpabuf_put_u8(buf, 0); /* Reserved */
		if (payload)
			wpabuf_put_data(buf, payload, payload_len);
	} else {
		u8 i;
		wpabuf_put_u8(buf, HS20_STYPE_QUERY_LIST);
		wpabuf_put_u8(buf, 0); /* Reserved */
		for (i = 0; i < 32; i++) {
			if (stypes & BIT(i))
				wpabuf_put_u8(buf, i);
		}
	}
	gas_anqp_set_element_len(buf, len_pos);

	gas_anqp_set_len(buf);
}


struct wpabuf * hs20_build_anqp_req(u32 stypes, const u8 *payload,
				    size_t payload_len)
{
	struct wpabuf *buf;

	buf = gas_anqp_build_initial_req(0, 100 + payload_len);
	if (buf == NULL)
		return NULL;

	hs20_put_anqp_req(stypes, payload, payload_len, buf);

	return buf;
}


int hs20_anqp_send_req(struct wpa_supplicant *wpa_s, const u8 *dst, u32 stypes,
		       const u8 *payload, size_t payload_len)
{
	struct wpabuf *buf;
	int ret = 0;
	int freq;
	struct wpa_bss *bss;
	int res;

	bss = wpa_bss_get_bssid(wpa_s, dst);
	if (!bss) {
		wpa_printf(MSG_WARNING,
			   "ANQP: Cannot send query to unknown BSS "
			   MACSTR, MAC2STR(dst));
		return -1;
	}

	wpa_bss_anqp_unshare_alloc(bss);
	freq = bss->freq;

	wpa_printf(MSG_DEBUG, "HS20: ANQP Query Request to " MACSTR " for "
		   "subtypes 0x%x", MAC2STR(dst), stypes);

	buf = hs20_build_anqp_req(stypes, payload, payload_len);
	if (buf == NULL)
		return -1;

	res = gas_query_req(wpa_s->gas, dst, freq, buf, anqp_resp_cb, wpa_s);
	if (res < 0) {
		wpa_printf(MSG_DEBUG, "ANQP: Failed to send Query Request");
		wpabuf_free(buf);
		ret = -1;
	} else
		wpa_printf(MSG_DEBUG, "ANQP: Query started with dialog token "
			   "%u", res);

	return ret;
}


static void hs20_set_osu_access_permission(const char *osu_dir,
					   const char *fname)
{
	struct stat statbuf;

	/* Get OSU directory information */
	if (stat(osu_dir, &statbuf) < 0) {
		wpa_printf(MSG_WARNING, "Cannot stat the OSU directory %s",
			   osu_dir);
		return;
	}

	if (chmod(fname, statbuf.st_mode) < 0) {
		wpa_printf(MSG_WARNING,
			   "Cannot change the permissions for %s", fname);
		return;
	}

	if (chown(fname, statbuf.st_uid, statbuf.st_gid) < 0) {
		wpa_printf(MSG_WARNING, "Cannot change the ownership for %s",
			   fname);
	}
}

static int hs20_process_icon_binary_file(struct wpa_supplicant *wpa_s,
					 const u8 *sa, const u8 *pos,
					 size_t slen)
{
	char fname[256];
	int png;
	FILE *f;
	u16 data_len;

	wpa_msg(wpa_s, MSG_INFO, "RX-HS20-ANQP " MACSTR " Icon Binary File",
		MAC2STR(sa));

	if (slen < 4) {
		wpa_dbg(wpa_s, MSG_DEBUG, "HS 2.0: Too short Icon Binary File "
			"value from " MACSTR, MAC2STR(sa));
		return -1;
	}

	wpa_printf(MSG_DEBUG, "HS 2.0: Download Status Code %u", *pos);
	if (*pos != 0)
		return -1;
	pos++;
	slen--;

	if ((size_t) 1 + pos[0] > slen) {
		wpa_dbg(wpa_s, MSG_DEBUG, "HS 2.0: Too short Icon Binary File "
			"value from " MACSTR, MAC2STR(sa));
		return -1;
	}
	wpa_hexdump_ascii(MSG_DEBUG, "Icon Type", pos + 1, pos[0]);
	png = os_strncasecmp((char *) pos + 1, "image/png", 9) == 0;
	slen -= 1 + pos[0];
	pos += 1 + pos[0];

	if (slen < 2) {
		wpa_dbg(wpa_s, MSG_DEBUG, "HS 2.0: Too short Icon Binary File "
			"value from " MACSTR, MAC2STR(sa));
		return -1;
	}
	data_len = WPA_GET_LE16(pos);
	pos += 2;
	slen -= 2;

	if (data_len > slen) {
		wpa_dbg(wpa_s, MSG_DEBUG, "HS 2.0: Too short Icon Binary File "
			"value from " MACSTR, MAC2STR(sa));
		return -1;
	}

	wpa_printf(MSG_DEBUG, "Icon Binary Data: %u bytes", data_len);
	if (wpa_s->conf->osu_dir == NULL)
		return -1;

	wpa_s->osu_icon_id++;
	if (wpa_s->osu_icon_id == 0)
		wpa_s->osu_icon_id++;
	snprintf(fname, sizeof(fname), "%s/osu-icon-%u.%s",
		 wpa_s->conf->osu_dir, wpa_s->osu_icon_id,
		 png ? "png" : "icon");
	f = fopen(fname, "wb");
	if (f == NULL)
		return -1;

	hs20_set_osu_access_permission(wpa_s->conf->osu_dir, fname);

	if (fwrite(pos, slen, 1, f) != 1) {
		fclose(f);
		unlink(fname);
		return -1;
	}
	fclose(f);

	wpa_msg(wpa_s, MSG_INFO, "RX-HS20-ANQP-ICON %s", fname);
	return 0;
}


static void hs20_continue_icon_fetch(void *eloop_ctx, void *sock_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	if (wpa_s->fetch_osu_icon_in_progress)
		hs20_next_osu_icon(wpa_s);
}


static void hs20_osu_icon_fetch_result(struct wpa_supplicant *wpa_s, int res)
{
	size_t i, j;
	struct os_reltime now, tmp;
	int dur;

	os_get_reltime(&now);
	os_reltime_sub(&now, &wpa_s->osu_icon_fetch_start, &tmp);
	dur = tmp.sec * 1000 + tmp.usec / 1000;
	wpa_printf(MSG_DEBUG, "HS 2.0: Icon fetch dur=%d ms res=%d",
		   dur, res);

	for (i = 0; i < wpa_s->osu_prov_count; i++) {
		struct osu_provider *osu = &wpa_s->osu_prov[i];
		for (j = 0; j < osu->icon_count; j++) {
			struct osu_icon *icon = &osu->icon[j];
			if (icon->id || icon->failed)
				continue;
			if (res < 0)
				icon->failed = 1;
			else
				icon->id = wpa_s->osu_icon_id;
			return;
		}
	}
}


void hs20_parse_rx_hs20_anqp_resp(struct wpa_supplicant *wpa_s,
				  struct wpa_bss *bss, const u8 *sa,
				  const u8 *data, size_t slen)
{
	const u8 *pos = data;
	u8 subtype;
	struct wpa_bss_anqp *anqp = NULL;
	int ret;

	if (slen < 2)
		return;

	if (bss)
		anqp = bss->anqp;

	subtype = *pos++;
	slen--;

	pos++; /* Reserved */
	slen--;

	switch (subtype) {
	case HS20_STYPE_CAPABILITY_LIST:
		wpa_msg(wpa_s, MSG_INFO, "RX-HS20-ANQP " MACSTR
			" HS Capability List", MAC2STR(sa));
		wpa_hexdump_ascii(MSG_DEBUG, "HS Capability List", pos, slen);
		if (anqp) {
			wpabuf_free(anqp->hs20_capability_list);
			anqp->hs20_capability_list =
				wpabuf_alloc_copy(pos, slen);
		}
		break;
	case HS20_STYPE_OPERATOR_FRIENDLY_NAME:
		wpa_msg(wpa_s, MSG_INFO, "RX-HS20-ANQP " MACSTR
			" Operator Friendly Name", MAC2STR(sa));
		wpa_hexdump_ascii(MSG_DEBUG, "oper friendly name", pos, slen);
		if (anqp) {
			wpabuf_free(anqp->hs20_operator_friendly_name);
			anqp->hs20_operator_friendly_name =
				wpabuf_alloc_copy(pos, slen);
		}
		break;
	case HS20_STYPE_WAN_METRICS:
		wpa_hexdump(MSG_DEBUG, "WAN Metrics", pos, slen);
		if (slen < 13) {
			wpa_dbg(wpa_s, MSG_DEBUG, "HS 2.0: Too short WAN "
				"Metrics value from " MACSTR, MAC2STR(sa));
			break;
		}
		wpa_msg(wpa_s, MSG_INFO, "RX-HS20-ANQP " MACSTR
			" WAN Metrics %02x:%u:%u:%u:%u:%u", MAC2STR(sa),
			pos[0], WPA_GET_LE32(pos + 1), WPA_GET_LE32(pos + 5),
			pos[9], pos[10], WPA_GET_LE16(pos + 11));
		if (anqp) {
			wpabuf_free(anqp->hs20_wan_metrics);
			anqp->hs20_wan_metrics = wpabuf_alloc_copy(pos, slen);
		}
		break;
	case HS20_STYPE_CONNECTION_CAPABILITY:
		wpa_msg(wpa_s, MSG_INFO, "RX-HS20-ANQP " MACSTR
			" Connection Capability", MAC2STR(sa));
		wpa_hexdump_ascii(MSG_DEBUG, "conn capability", pos, slen);
		if (anqp) {
			wpabuf_free(anqp->hs20_connection_capability);
			anqp->hs20_connection_capability =
				wpabuf_alloc_copy(pos, slen);
		}
		break;
	case HS20_STYPE_OPERATING_CLASS:
		wpa_msg(wpa_s, MSG_INFO, "RX-HS20-ANQP " MACSTR
			" Operating Class", MAC2STR(sa));
		wpa_hexdump_ascii(MSG_DEBUG, "Operating Class", pos, slen);
		if (anqp) {
			wpabuf_free(anqp->hs20_operating_class);
			anqp->hs20_operating_class =
				wpabuf_alloc_copy(pos, slen);
		}
		break;
	case HS20_STYPE_OSU_PROVIDERS_LIST:
		wpa_msg(wpa_s, MSG_INFO, "RX-HS20-ANQP " MACSTR
			" OSU Providers list", MAC2STR(sa));
		wpa_s->num_prov_found++;
		if (anqp) {
			wpabuf_free(anqp->hs20_osu_providers_list);
			anqp->hs20_osu_providers_list =
				wpabuf_alloc_copy(pos, slen);
		}
		break;
	case HS20_STYPE_ICON_BINARY_FILE:
		ret = hs20_process_icon_binary_file(wpa_s, sa, pos, slen);
		if (wpa_s->fetch_osu_icon_in_progress) {
			hs20_osu_icon_fetch_result(wpa_s, ret);
			eloop_cancel_timeout(hs20_continue_icon_fetch,
					     wpa_s, NULL);
			eloop_register_timeout(0, 0, hs20_continue_icon_fetch,
					       wpa_s, NULL);
		}
		break;
	default:
		wpa_printf(MSG_DEBUG, "HS20: Unsupported subtype %u", subtype);
		break;
	}
}


void hs20_notify_parse_done(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s->fetch_osu_icon_in_progress)
		return;
	if (eloop_is_timeout_registered(hs20_continue_icon_fetch, wpa_s, NULL))
		return;
	/*
	 * We are going through icon fetch, but no icon response was received.
	 * Assume this means the current AP could not provide an answer to avoid
	 * getting stuck in fetch iteration.
	 */
	hs20_icon_fetch_failed(wpa_s);
}


static void hs20_free_osu_prov_entry(struct osu_provider *prov)
{
}


void hs20_free_osu_prov(struct wpa_supplicant *wpa_s)
{
	size_t i;
	for (i = 0; i < wpa_s->osu_prov_count; i++)
		hs20_free_osu_prov_entry(&wpa_s->osu_prov[i]);
	os_free(wpa_s->osu_prov);
	wpa_s->osu_prov = NULL;
	wpa_s->osu_prov_count = 0;
}


static void hs20_osu_fetch_done(struct wpa_supplicant *wpa_s)
{
	char fname[256];
	FILE *f;
	size_t i, j;

	wpa_s->fetch_osu_info = 0;
	wpa_s->fetch_osu_icon_in_progress = 0;

	if (wpa_s->conf->osu_dir == NULL) {
		hs20_free_osu_prov(wpa_s);
		wpa_s->fetch_anqp_in_progress = 0;
		return;
	}

	snprintf(fname, sizeof(fname), "%s/osu-providers.txt",
		 wpa_s->conf->osu_dir);
	f = fopen(fname, "w");
	if (f == NULL) {
		hs20_free_osu_prov(wpa_s);
		return;
	}

	hs20_set_osu_access_permission(wpa_s->conf->osu_dir, fname);

	for (i = 0; i < wpa_s->osu_prov_count; i++) {
		struct osu_provider *osu = &wpa_s->osu_prov[i];
		if (i > 0)
			fprintf(f, "\n");
		fprintf(f, "OSU-PROVIDER " MACSTR "\n"
			"uri=%s\n"
			"methods=%08x\n",
			MAC2STR(osu->bssid), osu->server_uri, osu->osu_methods);
		if (osu->osu_ssid_len) {
			fprintf(f, "osu_ssid=%s\n",
				wpa_ssid_txt(osu->osu_ssid,
					     osu->osu_ssid_len));
		}
		if (osu->osu_nai[0])
			fprintf(f, "osu_nai=%s\n", osu->osu_nai);
		for (j = 0; j < osu->friendly_name_count; j++) {
			fprintf(f, "friendly_name=%s:%s\n",
				osu->friendly_name[j].lang,
				osu->friendly_name[j].text);
		}
		for (j = 0; j < osu->serv_desc_count; j++) {
			fprintf(f, "desc=%s:%s\n",
				osu->serv_desc[j].lang,
				osu->serv_desc[j].text);
		}
		for (j = 0; j < osu->icon_count; j++) {
			struct osu_icon *icon = &osu->icon[j];
			if (icon->failed)
				continue; /* could not fetch icon */
			fprintf(f, "icon=%u:%u:%u:%s:%s:%s\n",
				icon->id, icon->width, icon->height, icon->lang,
				icon->icon_type, icon->filename);
		}
	}
	fclose(f);
	hs20_free_osu_prov(wpa_s);

	wpa_msg(wpa_s, MSG_INFO, "OSU provider fetch completed");
	wpa_s->fetch_anqp_in_progress = 0;
}


void hs20_next_osu_icon(struct wpa_supplicant *wpa_s)
{
	size_t i, j;

	wpa_printf(MSG_DEBUG, "HS 2.0: Ready to fetch next icon");

	for (i = 0; i < wpa_s->osu_prov_count; i++) {
		struct osu_provider *osu = &wpa_s->osu_prov[i];
		for (j = 0; j < osu->icon_count; j++) {
			struct osu_icon *icon = &osu->icon[j];
			if (icon->id || icon->failed)
				continue;

			wpa_printf(MSG_DEBUG, "HS 2.0: Try to fetch icon '%s' "
				   "from " MACSTR, icon->filename,
				   MAC2STR(osu->bssid));
			os_get_reltime(&wpa_s->osu_icon_fetch_start);
			if (hs20_anqp_send_req(wpa_s, osu->bssid,
					       BIT(HS20_STYPE_ICON_REQUEST),
					       (u8 *) icon->filename,
					       os_strlen(icon->filename)) < 0) {
				icon->failed = 1;
				continue;
			}
			return;
		}
	}

	wpa_printf(MSG_DEBUG, "HS 2.0: No more icons to fetch");
	hs20_osu_fetch_done(wpa_s);
}


static void hs20_osu_add_prov(struct wpa_supplicant *wpa_s, struct wpa_bss *bss,
			      const u8 *osu_ssid, u8 osu_ssid_len,
			      const u8 *pos, size_t len)
{
	struct osu_provider *prov;
	const u8 *end = pos + len;
	u16 len2;
	const u8 *pos2;
	u8 uri_len, osu_method_len, osu_nai_len;

	wpa_hexdump(MSG_DEBUG, "HS 2.0: Parsing OSU Provider", pos, len);
	prov = os_realloc_array(wpa_s->osu_prov,
				wpa_s->osu_prov_count + 1,
				sizeof(*prov));
	if (prov == NULL)
		return;
	wpa_s->osu_prov = prov;
	prov = &prov[wpa_s->osu_prov_count];
	os_memset(prov, 0, sizeof(*prov));

	os_memcpy(prov->bssid, bss->bssid, ETH_ALEN);
	os_memcpy(prov->osu_ssid, osu_ssid, osu_ssid_len);
	prov->osu_ssid_len = osu_ssid_len;

	/* OSU Friendly Name Length */
	if (pos + 2 > end) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Not enough room for OSU "
			   "Friendly Name Length");
		return;
	}
	len2 = WPA_GET_LE16(pos);
	pos += 2;
	if (len2 > end - pos) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Not enough room for OSU "
			   "Friendly Name Duples");
		return;
	}
	pos2 = pos;
	pos += len2;

	/* OSU Friendly Name Duples */
	while (pos2 + 4 <= pos && prov->friendly_name_count < OSU_MAX_ITEMS) {
		struct osu_lang_string *f;
		if (pos2 + 1 + pos2[0] > pos || pos2[0] < 3) {
			wpa_printf(MSG_DEBUG, "Invalid OSU Friendly Name");
			break;
		}
		f = &prov->friendly_name[prov->friendly_name_count++];
		os_memcpy(f->lang, pos2 + 1, 3);
		os_memcpy(f->text, pos2 + 1 + 3, pos2[0] - 3);
		pos2 += 1 + pos2[0];
	}

	/* OSU Server URI */
	if (pos + 1 > end) {
		wpa_printf(MSG_DEBUG,
			   "HS 2.0: Not enough room for OSU Server URI length");
		return;
	}
	uri_len = *pos++;
	if (uri_len > end - pos) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Not enough room for OSU Server "
			   "URI");
		return;
	}
	os_memcpy(prov->server_uri, pos, uri_len);
	pos += uri_len;

	/* OSU Method list */
	if (pos + 1 > end) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Not enough room for OSU Method "
			   "list length");
		return;
	}
	osu_method_len = pos[0];
	if (osu_method_len > end - pos - 1) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Not enough room for OSU Method "
			   "list");
		return;
	}
	pos2 = pos + 1;
	pos += 1 + osu_method_len;
	while (pos2 < pos) {
		if (*pos2 < 32)
			prov->osu_methods |= BIT(*pos2);
		pos2++;
	}

	/* Icons Available Length */
	if (pos + 2 > end) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Not enough room for Icons "
			   "Available Length");
		return;
	}
	len2 = WPA_GET_LE16(pos);
	pos += 2;
	if (len2 > end - pos) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Not enough room for Icons "
			   "Available");
		return;
	}
	pos2 = pos;
	pos += len2;

	/* Icons Available */
	while (pos2 < pos) {
		struct osu_icon *icon = &prov->icon[prov->icon_count];
		u8 flen;

		if (pos2 + 2 + 2 + 3 + 1 + 1 > pos) {
			wpa_printf(MSG_DEBUG, "HS 2.0: Invalid Icon Metadata");
			break;
		}

		icon->width = WPA_GET_LE16(pos2);
		pos2 += 2;
		icon->height = WPA_GET_LE16(pos2);
		pos2 += 2;
		os_memcpy(icon->lang, pos2, 3);
		pos2 += 3;

		flen = pos2[0];
		if (flen > pos - pos2 - 1) {
			wpa_printf(MSG_DEBUG, "HS 2.0: Not room for Icon Type");
			break;
		}
		os_memcpy(icon->icon_type, pos2 + 1, flen);
		pos2 += 1 + flen;

		if (pos2 + 1 > pos) {
			wpa_printf(MSG_DEBUG, "HS 2.0: Not room for Icon "
				   "Filename length");
			break;
		}
		flen = pos2[0];
		if (flen > pos - pos2 - 1) {
			wpa_printf(MSG_DEBUG, "HS 2.0: Not room for Icon "
				   "Filename");
			break;
		}
		os_memcpy(icon->filename, pos2 + 1, flen);
		pos2 += 1 + flen;

		prov->icon_count++;
	}

	/* OSU_NAI */
	if (pos + 1 > end) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Not enough room for OSU_NAI");
		return;
	}
	osu_nai_len = pos[0];
	if (osu_nai_len > end - pos - 1) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Not enough room for OSU_NAI");
		return;
	}
	os_memcpy(prov->osu_nai, pos + 1, osu_nai_len);
	pos += 1 + osu_nai_len;

	/* OSU Service Description Length */
	if (pos + 2 > end) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Not enough room for OSU "
			   "Service Description Length");
		return;
	}
	len2 = WPA_GET_LE16(pos);
	pos += 2;
	if (len2 > end - pos) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Not enough room for OSU "
			   "Service Description Duples");
		return;
	}
	pos2 = pos;
	pos += len2;

	/* OSU Service Description Duples */
	while (pos2 + 4 <= pos && prov->serv_desc_count < OSU_MAX_ITEMS) {
		struct osu_lang_string *f;
		u8 descr_len;

		descr_len = pos2[0];
		if (descr_len > pos - pos2 - 1 || descr_len < 3) {
			wpa_printf(MSG_DEBUG, "Invalid OSU Service "
				   "Description");
			break;
		}
		f = &prov->serv_desc[prov->serv_desc_count++];
		os_memcpy(f->lang, pos2 + 1, 3);
		os_memcpy(f->text, pos2 + 1 + 3, descr_len - 3);
		pos2 += 1 + descr_len;
	}

	wpa_printf(MSG_DEBUG, "HS 2.0: Added OSU Provider through " MACSTR,
		   MAC2STR(bss->bssid));
	wpa_s->osu_prov_count++;
}


void hs20_osu_icon_fetch(struct wpa_supplicant *wpa_s)
{
	struct wpa_bss *bss;
	struct wpabuf *prov_anqp;
	const u8 *pos, *end;
	u16 len;
	const u8 *osu_ssid;
	u8 osu_ssid_len;
	u8 num_providers;

	hs20_free_osu_prov(wpa_s);

	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (bss->anqp == NULL)
			continue;
		prov_anqp = bss->anqp->hs20_osu_providers_list;
		if (prov_anqp == NULL)
			continue;
		wpa_printf(MSG_DEBUG, "HS 2.0: Parsing OSU Providers list from "
			   MACSTR, MAC2STR(bss->bssid));
		wpa_hexdump_buf(MSG_DEBUG, "HS 2.0: OSU Providers list",
				prov_anqp);
		pos = wpabuf_head(prov_anqp);
		end = pos + wpabuf_len(prov_anqp);

		/* OSU SSID */
		if (pos + 1 > end)
			continue;
		if (pos + 1 + pos[0] > end) {
			wpa_printf(MSG_DEBUG, "HS 2.0: Not enough room for "
				   "OSU SSID");
			continue;
		}
		osu_ssid_len = *pos++;
		if (osu_ssid_len > SSID_MAX_LEN) {
			wpa_printf(MSG_DEBUG, "HS 2.0: Invalid OSU SSID "
				   "Length %u", osu_ssid_len);
			continue;
		}
		osu_ssid = pos;
		pos += osu_ssid_len;

		if (pos + 1 > end) {
			wpa_printf(MSG_DEBUG, "HS 2.0: Not enough room for "
				   "Number of OSU Providers");
			continue;
		}
		num_providers = *pos++;
		wpa_printf(MSG_DEBUG, "HS 2.0: Number of OSU Providers: %u",
			   num_providers);

		/* OSU Providers */
		while (pos + 2 < end && num_providers > 0) {
			num_providers--;
			len = WPA_GET_LE16(pos);
			pos += 2;
			if (len > (unsigned int) (end - pos))
				break;
			hs20_osu_add_prov(wpa_s, bss, osu_ssid,
					  osu_ssid_len, pos, len);
			pos += len;
		}

		if (pos != end) {
			wpa_printf(MSG_DEBUG, "HS 2.0: Ignored %d bytes of "
				   "extra data after OSU Providers",
				   (int) (end - pos));
		}
	}

	wpa_s->fetch_osu_icon_in_progress = 1;
	hs20_next_osu_icon(wpa_s);
}


static void hs20_osu_scan_res_handler(struct wpa_supplicant *wpa_s,
				      struct wpa_scan_results *scan_res)
{
	wpa_printf(MSG_DEBUG, "OSU provisioning fetch scan completed");
	if (!wpa_s->fetch_osu_waiting_scan) {
		wpa_printf(MSG_DEBUG, "OSU fetch have been canceled");
		return;
	}
	wpa_s->network_select = 0;
	wpa_s->fetch_all_anqp = 1;
	wpa_s->fetch_osu_info = 1;
	wpa_s->fetch_osu_icon_in_progress = 0;

	interworking_start_fetch_anqp(wpa_s);
}


int hs20_fetch_osu(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->wpa_state == WPA_INTERFACE_DISABLED) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Cannot start fetch_osu - "
			   "interface disabled");
		return -1;
	}

	if (wpa_s->scanning) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Cannot start fetch_osu - "
			   "scanning");
		return -1;
	}

	if (wpa_s->conf->osu_dir == NULL) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Cannot start fetch_osu - "
			   "osu_dir not configured");
		return -1;
	}

	if (wpa_s->fetch_anqp_in_progress || wpa_s->network_select) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Cannot start fetch_osu - "
			   "fetch in progress (%d, %d)",
			   wpa_s->fetch_anqp_in_progress,
			   wpa_s->network_select);
		return -1;
	}

	wpa_msg(wpa_s, MSG_INFO, "Starting OSU provisioning information fetch");
	wpa_s->num_osu_scans = 0;
	wpa_s->num_prov_found = 0;
	hs20_start_osu_scan(wpa_s);

	return 0;
}


void hs20_start_osu_scan(struct wpa_supplicant *wpa_s)
{
	wpa_s->fetch_osu_waiting_scan = 1;
	wpa_s->num_osu_scans++;
	wpa_s->scan_req = MANUAL_SCAN_REQ;
	wpa_s->scan_res_handler = hs20_osu_scan_res_handler;
	wpa_supplicant_req_scan(wpa_s, 0, 0);
}


void hs20_cancel_fetch_osu(struct wpa_supplicant *wpa_s)
{
	wpa_printf(MSG_DEBUG, "Cancel OSU fetch");
	interworking_stop_fetch_anqp(wpa_s);
	wpa_s->fetch_osu_waiting_scan = 0;
	wpa_s->network_select = 0;
	wpa_s->fetch_osu_info = 0;
	wpa_s->fetch_osu_icon_in_progress = 0;
}


void hs20_icon_fetch_failed(struct wpa_supplicant *wpa_s)
{
	hs20_osu_icon_fetch_result(wpa_s, -1);
	eloop_cancel_timeout(hs20_continue_icon_fetch, wpa_s, NULL);
	eloop_register_timeout(0, 0, hs20_continue_icon_fetch, wpa_s, NULL);
}


void hs20_rx_subscription_remediation(struct wpa_supplicant *wpa_s,
				      const char *url, u8 osu_method)
{
	if (url)
		wpa_msg(wpa_s, MSG_INFO, HS20_SUBSCRIPTION_REMEDIATION "%u %s",
			osu_method, url);
	else
		wpa_msg(wpa_s, MSG_INFO, HS20_SUBSCRIPTION_REMEDIATION);
}


void hs20_rx_deauth_imminent_notice(struct wpa_supplicant *wpa_s, u8 code,
				    u16 reauth_delay, const char *url)
{
	if (!wpa_sm_pmf_enabled(wpa_s->wpa)) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Ignore deauthentication imminent notice since PMF was not enabled");
		return;
	}

	wpa_msg(wpa_s, MSG_INFO, HS20_DEAUTH_IMMINENT_NOTICE "%u %u %s",
		code, reauth_delay, url);

	if (code == HS20_DEAUTH_REASON_CODE_BSS) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Add BSS to blacklist");
		wpa_blacklist_add(wpa_s, wpa_s->bssid);
		/* TODO: For now, disable full ESS since some drivers may not
		 * support disabling per BSS. */
		if (wpa_s->current_ssid) {
			struct os_reltime now;
			os_get_reltime(&now);
			if (now.sec + reauth_delay <=
			    wpa_s->current_ssid->disabled_until.sec)
				return;
			wpa_printf(MSG_DEBUG, "HS 2.0: Disable network for %u seconds (BSS)",
				   reauth_delay);
			wpa_s->current_ssid->disabled_until.sec =
				now.sec + reauth_delay;
		}
	}

	if (code == HS20_DEAUTH_REASON_CODE_ESS && wpa_s->current_ssid) {
		struct os_reltime now;
		os_get_reltime(&now);
		if (now.sec + reauth_delay <=
		    wpa_s->current_ssid->disabled_until.sec)
			return;
		wpa_printf(MSG_DEBUG, "HS 2.0: Disable network for %u seconds",
			   reauth_delay);
		wpa_s->current_ssid->disabled_until.sec =
			now.sec + reauth_delay;
	}
}


void hs20_deinit(struct wpa_supplicant *wpa_s)
{
	eloop_cancel_timeout(hs20_continue_icon_fetch, wpa_s, NULL);
	hs20_free_osu_prov(wpa_s);
}
