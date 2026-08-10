/*
 * BSS table
 * Copyright (c) 2009-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "drivers/driver.h"
#include "eap_peer/eap.h"
#include "rsn_supp/wpa.h"
#include "wpa_supplicant_i.h"
#include "config.h"
#include "notify.h"
#include "scan.h"
#include "bssid_ignore.h"
#include "bss.h"

static void wpa_bss_set_hessid(struct wpa_bss *bss)
{
#ifdef CONFIG_INTERWORKING
	const u8 *ie = wpa_bss_get_ie(bss, WLAN_EID_INTERWORKING);
	if (ie == NULL || (ie[1] != 7 && ie[1] != 9)) {
		os_memset(bss->hessid, 0, ETH_ALEN);
		return;
	}
	if (ie[1] == 7)
		os_memcpy(bss->hessid, ie + 3, ETH_ALEN);
	else
		os_memcpy(bss->hessid, ie + 5, ETH_ALEN);
#endif /* CONFIG_INTERWORKING */
}


/**
 * wpa_bss_anqp_alloc - Allocate ANQP data structure for a BSS entry
 * Returns: Allocated ANQP data structure or %NULL on failure
 *
 * The allocated ANQP data structure has its users count set to 1. It may be
 * shared by multiple BSS entries and each shared entry is freed with
 * wpa_bss_anqp_free().
 */
struct wpa_bss_anqp * wpa_bss_anqp_alloc(void)
{
	struct wpa_bss_anqp *anqp;
	anqp = os_zalloc(sizeof(*anqp));
	if (anqp == NULL)
		return NULL;
#ifdef CONFIG_INTERWORKING
	dl_list_init(&anqp->anqp_elems);
#endif /* CONFIG_INTERWORKING */
	anqp->users = 1;
	return anqp;
}


/**
 * wpa_bss_anqp_clone - Clone an ANQP data structure
 * @anqp: ANQP data structure from wpa_bss_anqp_alloc()
 * Returns: Cloned ANQP data structure or %NULL on failure
 */
static struct wpa_bss_anqp * wpa_bss_anqp_clone(struct wpa_bss_anqp *anqp)
{
	struct wpa_bss_anqp *n;

	n = os_zalloc(sizeof(*n));
	if (n == NULL)
		return NULL;

#define ANQP_DUP(f) if (anqp->f) n->f = wpabuf_dup(anqp->f)
#ifdef CONFIG_INTERWORKING
	dl_list_init(&n->anqp_elems);
	ANQP_DUP(capability_list);
	ANQP_DUP(venue_name);
	ANQP_DUP(network_auth_type);
	ANQP_DUP(roaming_consortium);
	ANQP_DUP(ip_addr_type_availability);
	ANQP_DUP(nai_realm);
	ANQP_DUP(anqp_3gpp);
	ANQP_DUP(domain_name);
	ANQP_DUP(fils_realm_info);
#endif /* CONFIG_INTERWORKING */
#ifdef CONFIG_HS20
	ANQP_DUP(hs20_capability_list);
	ANQP_DUP(hs20_operator_friendly_name);
	ANQP_DUP(hs20_wan_metrics);
	ANQP_DUP(hs20_connection_capability);
	ANQP_DUP(hs20_operating_class);
#endif /* CONFIG_HS20 */
#undef ANQP_DUP

	return n;
}


/**
 * wpa_bss_anqp_unshare_alloc - Unshare ANQP data (if shared) in a BSS entry
 * @bss: BSS entry
 * Returns: 0 on success, -1 on failure
 *
 * This function ensures the specific BSS entry has an ANQP data structure that
 * is not shared with any other BSS entry.
 */
int wpa_bss_anqp_unshare_alloc(struct wpa_bss *bss)
{
	struct wpa_bss_anqp *anqp;

	if (bss->anqp && bss->anqp->users > 1) {
		/* allocated, but shared - clone an unshared copy */
		anqp = wpa_bss_anqp_clone(bss->anqp);
		if (anqp == NULL)
			return -1;
		anqp->users = 1;
		bss->anqp->users--;
		bss->anqp = anqp;
		return 0;
	}

	if (bss->anqp)
		return 0; /* already allocated and not shared */

	/* not allocated - allocate a new storage area */
	bss->anqp = wpa_bss_anqp_alloc();
	return bss->anqp ? 0 : -1;
}


/**
 * wpa_bss_anqp_free - Free an ANQP data structure
 * @anqp: ANQP data structure from wpa_bss_anqp_alloc() or wpa_bss_anqp_clone()
 */
static void wpa_bss_anqp_free(struct wpa_bss_anqp *anqp)
{
#ifdef CONFIG_INTERWORKING
	struct wpa_bss_anqp_elem *elem;
#endif /* CONFIG_INTERWORKING */

	if (anqp == NULL)
		return;

	anqp->users--;
	if (anqp->users > 0) {
		/* Another BSS entry holds a pointer to this ANQP info */
		return;
	}

#ifdef CONFIG_INTERWORKING
	wpabuf_free(anqp->capability_list);
	wpabuf_free(anqp->venue_name);
	wpabuf_free(anqp->network_auth_type);
	wpabuf_free(anqp->roaming_consortium);
	wpabuf_free(anqp->ip_addr_type_availability);
	wpabuf_free(anqp->nai_realm);
	wpabuf_free(anqp->anqp_3gpp);
	wpabuf_free(anqp->domain_name);
	wpabuf_free(anqp->fils_realm_info);

	while ((elem = dl_list_first(&anqp->anqp_elems,
				     struct wpa_bss_anqp_elem, list))) {
		dl_list_del(&elem->list);
		wpabuf_free(elem->payload);
		os_free(elem);
	}
#endif /* CONFIG_INTERWORKING */
#ifdef CONFIG_HS20
	wpabuf_free(anqp->hs20_capability_list);
	wpabuf_free(anqp->hs20_operator_friendly_name);
	wpabuf_free(anqp->hs20_wan_metrics);
	wpabuf_free(anqp->hs20_connection_capability);
	wpabuf_free(anqp->hs20_operating_class);
#endif /* CONFIG_HS20 */

	os_free(anqp);
}


static struct wpa_connect_work *
wpa_bss_check_pending_connect(struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	struct wpa_radio_work *work;
	struct wpa_connect_work *cwork;

	work = radio_work_pending(wpa_s, "sme-connect");
	if (!work)
		work = radio_work_pending(wpa_s, "connect");
	if (!work)
		return NULL;

	cwork = work->ctx;
	if (cwork->bss != bss)
		return NULL;

	return cwork;
}


static void wpa_bss_update_pending_connect(struct wpa_connect_work *cwork,
					   struct wpa_bss *new_bss)
{
	wpa_printf(MSG_DEBUG,
		   "Update BSS pointer for the pending connect radio work");
	cwork->bss = new_bss;
	if (!new_bss)
		cwork->bss_removed = 1;
}


void wpa_bss_remove(struct wpa_supplicant *wpa_s, struct wpa_bss *bss,
		    const char *reason)
{
	struct wpa_connect_work *cwork;
	unsigned int j;

	if (wpa_s->last_scan_res) {
		unsigned int i;
		for (i = 0; i < wpa_s->last_scan_res_used; i++) {
			if (wpa_s->last_scan_res[i] == bss) {
				os_memmove(&wpa_s->last_scan_res[i],
					   &wpa_s->last_scan_res[i + 1],
					   (wpa_s->last_scan_res_used - i - 1)
					   * sizeof(struct wpa_bss *));
				wpa_s->last_scan_res_used--;
				break;
			}
		}
	}
	cwork = wpa_bss_check_pending_connect(wpa_s, bss);
	if (cwork)
		wpa_bss_update_pending_connect(cwork, NULL);
	dl_list_del(&bss->list);
	dl_list_del(&bss->list_id);
	wpa_s->num_bss--;
	wpa_dbg(wpa_s, MSG_DEBUG, "BSS: Remove id %u BSSID " MACSTR
		" SSID '%s' due to %s", bss->id, MAC2STR(bss->bssid),
		wpa_ssid_txt(bss->ssid, bss->ssid_len), reason);
	wpas_notify_bss_removed(wpa_s, bss->bssid, bss->id);
	wpa_bss_anqp_free(bss->anqp);

	if (wpa_s->current_bss == bss) {
		wpa_printf(MSG_DEBUG,
			   "BSS: Clear current_bss due to bss removal");
		wpa_s->current_bss = NULL;
	}

#ifdef CONFIG_INTERWORKING
	if (wpa_s->interworking_gas_bss == bss) {
		wpa_printf(MSG_DEBUG,
			   "BSS: Clear interworking_gas_bss due to bss removal");
		wpa_s->interworking_gas_bss = NULL;
	}
#endif /* CONFIG_INTERWORKING */

#ifdef CONFIG_WNM
	if (wpa_s->wnm_target_bss == bss) {
		wpa_printf(MSG_DEBUG,
			   "BSS: Clear wnm_target_bss due to bss removal");
		wpa_s->wnm_target_bss = NULL;
	}
#endif /* CONFIG_WNM */

	if (wpa_s->ml_connect_probe_bss == bss) {
		wpa_printf(MSG_DEBUG,
			   "BSS: Clear ml_connect_probe_bss due to bss removal");
		wpa_s->ml_connect_probe_bss = NULL;
	}

	for (j = 0; j < MAX_NUM_MLD_LINKS; j++) {
		if (wpa_s->links[j].bss == bss) {
			wpa_printf(MSG_DEBUG,
				   "BSS: Clear links[%d].bss due to bss removal",
				   j);
			wpa_s->valid_links &= ~BIT(j);
			wpa_s->links[j].bss = NULL;
		}
	}

	os_free(bss);
}


/**
 * wpa_bss_get - Fetch a BSS table entry based on BSSID and SSID
 * @wpa_s: Pointer to wpa_supplicant data
 * @bssid: BSSID, or %NULL to match any BSSID
 * @ssid: SSID
 * @ssid_len: Length of @ssid
 * Returns: Pointer to the BSS entry or %NULL if not found
 */
struct wpa_bss * wpa_bss_get(struct wpa_supplicant *wpa_s, const u8 *bssid,
			     const u8 *ssid, size_t ssid_len)
{
	struct wpa_bss *bss;

	if (bssid && !wpa_supplicant_filter_bssid_match(wpa_s, bssid))
		return NULL;
	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		if ((!bssid || ether_addr_equal(bss->bssid, bssid)) &&
		    bss->ssid_len == ssid_len &&
		    os_memcmp(bss->ssid, ssid, ssid_len) == 0)
			return bss;
	}
	return NULL;
}

/**
 * wpa_bss_get_connection - Fetch a BSS table entry based on BSSID and SSID.
 * @wpa_s: Pointer to wpa_supplicant data
 * @bssid: BSSID, or %NULL to match any BSSID
 * @ssid: SSID
 * @ssid_len: Length of @ssid
 * Returns: Pointer to the BSS entry or %NULL if not found
 *
 * This function is similar to wpa_bss_get() but it will also return OWE
 * transition mode encrypted networks for which transition-element matches
 * @ssid.
 */
struct wpa_bss * wpa_bss_get_connection(struct wpa_supplicant *wpa_s,
					const u8 *bssid,
					const u8 *ssid, size_t ssid_len)
{
	struct wpa_bss *bss;
#ifdef CONFIG_OWE
	const u8 *owe, *owe_bssid, *owe_ssid;
	size_t owe_ssid_len;
#endif /* CONFIG_OWE */

	if (bssid && !wpa_supplicant_filter_bssid_match(wpa_s, bssid))
		return NULL;
	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (bssid && !ether_addr_equal(bss->bssid, bssid))
			continue;

		if (bss->ssid_len == ssid_len &&
		    os_memcmp(bss->ssid, ssid, ssid_len) == 0)
			return bss;

#ifdef CONFIG_OWE
		/* Check if OWE transition mode element is present and matches
		 * the SSID */
		owe = wpa_bss_get_vendor_ie(bss, OWE_IE_VENDOR_TYPE);
		if (!owe)
			continue;

		if (wpas_get_owe_trans_network(owe, &owe_bssid, &owe_ssid,
					       &owe_ssid_len))
			continue;

		if (bss->ssid_len &&
		    owe_ssid_len == ssid_len &&
		    os_memcmp(owe_ssid, ssid, ssid_len) == 0)
			return bss;
#endif /* CONFIG_OWE */
	}
	return NULL;
}


void calculate_update_time(const struct os_reltime *fetch_time,
			   unsigned int age_ms,
			   struct os_reltime *update_time)
{
	os_time_t usec;

	update_time->sec = fetch_time->sec;
	update_time->usec = fetch_time->usec;
	update_time->sec -= age_ms / 1000;
	usec = (age_ms % 1000) * 1000;
	if (update_time->usec < usec) {
		update_time->sec--;
		update_time->usec += 1000000;
	}
	update_time->usec -= usec;
}


static void wpa_bss_copy_res(struct wpa_bss *dst, struct wpa_scan_res *src,
			     struct os_reltime *fetch_time)
{
	dst->flags = src->flags;
	os_memcpy(dst->bssid, src->bssid, ETH_ALEN);
	dst->freq = src->freq;
	dst->max_cw = src->max_cw;
	dst->beacon_int = src->beacon_int;
	dst->caps = src->caps;
	dst->qual = src->qual;
	dst->noise = src->noise;
	dst->level = src->level;
	dst->tsf = src->tsf;
	dst->beacon_newer = src->beacon_newer;
	dst->est_throughput = src->est_throughput;
	dst->snr = src->snr;

	calculate_update_time(fetch_time, src->age, &dst->last_update);
}


static int wpa_bss_is_wps_candidate(struct wpa_supplicant *wpa_s,
				    struct wpa_bss *bss)
{
#ifdef CONFIG_WPS
	struct wpa_ssid *ssid;
	struct wpabuf *wps_ie;
	int pbc = 0, ret;

	wps_ie = wpa_bss_get_vendor_ie_multi(bss, WPS_IE_VENDOR_TYPE);
	if (!wps_ie)
		return 0;

	if (wps_is_selected_pbc_registrar(wps_ie)) {
		pbc = 1;
	} else if (!wps_is_addr_authorized(wps_ie, wpa_s->own_addr, 1)) {
		wpabuf_free(wps_ie);
		return 0;
	}

	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
		if (!(ssid->key_mgmt & WPA_KEY_MGMT_WPS))
			continue;
		if (ssid->ssid_len &&
		    (ssid->ssid_len != bss->ssid_len ||
		     os_memcmp(ssid->ssid, bss->ssid, ssid->ssid_len) != 0))
			continue;

		if (pbc)
			ret = eap_is_wps_pbc_enrollee(&ssid->eap);
		else
			ret = eap_is_wps_pin_enrollee(&ssid->eap);
		wpabuf_free(wps_ie);
		return ret;
	}
	wpabuf_free(wps_ie);
#endif /* CONFIG_WPS */

	return 0;
}


static bool is_p2p_pending_bss(struct wpa_supplicant *wpa_s,
			       struct wpa_bss *bss)
{
#ifdef CONFIG_P2P
	u8 addr[ETH_ALEN];

	if (ether_addr_equal(bss->bssid, wpa_s->pending_join_iface_addr))
		return true;
	if (!is_zero_ether_addr(wpa_s->pending_join_dev_addr) &&
	    p2p_parse_dev_addr(wpa_bss_ie_ptr(bss), bss->ie_len, addr) == 0 &&
	    ether_addr_equal(addr, wpa_s->pending_join_dev_addr))
		return true;
#endif /* CONFIG_P2P */
	return false;
}


#ifdef CONFIG_OWE
static int wpa_bss_owe_trans_known(struct wpa_supplicant *wpa_s,
				   struct wpa_bss *bss,
				   const u8 *entry_ssid, size_t entry_ssid_len)
{
	const u8 *owe, *owe_bssid, *owe_ssid;
	size_t owe_ssid_len;

	owe = wpa_bss_get_vendor_ie(bss, OWE_IE_VENDOR_TYPE);
	if (!owe)
		return 0;

	if (wpas_get_owe_trans_network(owe, &owe_bssid, &owe_ssid,
				       &owe_ssid_len))
		return 0;

	return entry_ssid_len == owe_ssid_len &&
		os_memcmp(owe_ssid, entry_ssid, owe_ssid_len) == 0;
}
#endif /* CONFIG_OWE */


static int wpa_bss_known(struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	struct wpa_ssid *ssid;

	if (is_p2p_pending_bss(wpa_s, bss))
		return 1;

	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
		if (ssid->ssid == NULL || ssid->ssid_len == 0)
			continue;
		if (ssid->ssid_len == bss->ssid_len &&
		    os_memcmp(ssid->ssid, bss->ssid, ssid->ssid_len) == 0)
			return 1;
#ifdef CONFIG_OWE
		if (wpa_bss_owe_trans_known(wpa_s, bss, ssid->ssid,
					    ssid->ssid_len))
			return 1;
#endif /* CONFIG_OWE */
	}

	return 0;
}


static int wpa_bss_in_use(struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	int i;

	if (bss == wpa_s->current_bss)
		return 1;

	if (bss == wpa_s->ml_connect_probe_bss)
		return 1;

#ifdef CONFIG_WNM
	if (bss == wpa_s->wnm_target_bss)
		return 1;
#endif /* CONFIG_WNM */

	if (wpa_s->current_bss &&
	    (bss->ssid_len != wpa_s->current_bss->ssid_len ||
	     os_memcmp(bss->ssid, wpa_s->current_bss->ssid,
		       bss->ssid_len) != 0))
		return 0; /* SSID has changed */

	if (!is_zero_ether_addr(bss->bssid) &&
	    (ether_addr_equal(bss->bssid, wpa_s->bssid) ||
	     ether_addr_equal(bss->bssid, wpa_s->pending_bssid)))
		return 1;

	if (!wpa_s->valid_links)
		return 0;

	for_each_link(wpa_s->valid_links, i) {
		if (ether_addr_equal(bss->bssid, wpa_s->links[i].bssid))
			return 1;
	}

	return 0;
}


static int wpa_bss_remove_oldest_unknown(struct wpa_supplicant *wpa_s)
{
	struct wpa_bss *bss;

	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (!wpa_bss_known(wpa_s, bss) &&
		    !wpa_bss_in_use(wpa_s, bss) &&
		    !wpa_bss_is_wps_candidate(wpa_s, bss)) {
			wpa_bss_remove(wpa_s, bss, __func__);
			return 0;
		}
	}

	return -1;
}


static int wpa_bss_remove_oldest(struct wpa_supplicant *wpa_s)
{
	struct wpa_bss *bss;

	/*
	 * Remove the oldest entry that does not match with any configured
	 * network.
	 */
	if (wpa_bss_remove_oldest_unknown(wpa_s) == 0)
		return 0;

	/*
	 * Remove the oldest entry that isn't currently in use.
	 */
	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (!wpa_bss_in_use(wpa_s, bss)) {
			wpa_bss_remove(wpa_s, bss, __func__);
			return 0;
		}
	}

	return -1;
}


static struct wpa_bss * wpa_bss_add(struct wpa_supplicant *wpa_s,
				    const u8 *ssid, size_t ssid_len,
				    struct wpa_scan_res *res,
				    struct os_reltime *fetch_time)
{
	struct wpa_bss *bss;
	char extra[100];
	char *pos, *end;
	int ret = 0;

	bss = os_zalloc(sizeof(*bss) + res->ie_len + res->beacon_ie_len);
	if (bss == NULL)
		return NULL;
	bss->id = wpa_s->bss_next_id++;
	bss->last_update_idx = wpa_s->bss_update_idx;
	wpa_bss_copy_res(bss, res, fetch_time);
	os_memcpy(bss->ssid, ssid, ssid_len);
	bss->ssid_len = ssid_len;
	bss->ie_len = res->ie_len;
	bss->beacon_ie_len = res->beacon_ie_len;
	os_memcpy(bss->ies, res + 1, res->ie_len + res->beacon_ie_len);
	wpa_bss_set_hessid(bss);

	wpa_bss_parse_basic_ml_element(wpa_s, bss);

	if (wpa_s->num_bss + 1 > wpa_s->conf->bss_max_count &&
	    wpa_bss_remove_oldest(wpa_s) != 0) {
		wpa_printf(MSG_ERROR, "Increasing the MAX BSS count to %d "
			   "because all BSSes are in use. We should normally "
			   "not get here!", (int) wpa_s->num_bss + 1);
		wpa_s->conf->bss_max_count = wpa_s->num_bss + 1;
	}

	dl_list_add_tail(&wpa_s->bss, &bss->list);
	dl_list_add_tail(&wpa_s->bss_id, &bss->list_id);
	wpa_s->num_bss++;

	extra[0] = '\0';
	pos = extra;
	end = pos + sizeof(extra);
	if (!is_zero_ether_addr(bss->hessid))
		ret = os_snprintf(pos, end - pos, " HESSID " MACSTR,
				  MAC2STR(bss->hessid));

	if (!is_zero_ether_addr(bss->mld_addr) &&
	    !os_snprintf_error(end - pos, ret)) {
		pos += ret;
		ret = os_snprintf(pos, end - pos, " MLD ADDR " MACSTR,
				  MAC2STR(bss->mld_addr));
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "BSS: Add new id %u BSSID " MACSTR
		" SSID '%s' freq %d%s",
		bss->id, MAC2STR(bss->bssid), wpa_ssid_txt(ssid, ssid_len),
		bss->freq, extra);
	wpas_notify_bss_added(wpa_s, bss->bssid, bss->id);
	return bss;
}


static int are_ies_equal(const struct wpa_bss *old,
			 const struct wpa_scan_res *new_res, u32 ie)
{
	const u8 *old_ie, *new_ie;
	struct wpabuf *old_ie_buff = NULL;
	struct wpabuf *new_ie_buff = NULL;
	int new_ie_len, old_ie_len, ret, is_multi;

	switch (ie) {
	case WPA_IE_VENDOR_TYPE:
		old_ie = wpa_bss_get_vendor_ie(old, ie);
		new_ie = wpa_scan_get_vendor_ie(new_res, ie);
		is_multi = 0;
		break;
	case WPS_IE_VENDOR_TYPE:
		old_ie_buff = wpa_bss_get_vendor_ie_multi(old, ie);
		new_ie_buff = wpa_scan_get_vendor_ie_multi(new_res, ie);
		is_multi = 1;
		break;
	case WLAN_EID_RSN:
	case WLAN_EID_SUPP_RATES:
	case WLAN_EID_EXT_SUPP_RATES:
		old_ie = wpa_bss_get_ie(old, ie);
		new_ie = wpa_scan_get_ie(new_res, ie);
		is_multi = 0;
		break;
	default:
		wpa_printf(MSG_DEBUG, "bss: %s: cannot compare IEs", __func__);
		return 0;
	}

	if (is_multi) {
		/* in case of multiple IEs stored in buffer */
		old_ie = old_ie_buff ? wpabuf_head_u8(old_ie_buff) : NULL;
		new_ie = new_ie_buff ? wpabuf_head_u8(new_ie_buff) : NULL;
		old_ie_len = old_ie_buff ? wpabuf_len(old_ie_buff) : 0;
		new_ie_len = new_ie_buff ? wpabuf_len(new_ie_buff) : 0;
	} else {
		/* in case of single IE */
		old_ie_len = old_ie ? old_ie[1] + 2 : 0;
		new_ie_len = new_ie ? new_ie[1] + 2 : 0;
	}

	if (!old_ie || !new_ie)
		ret = !old_ie && !new_ie;
	else
		ret = (old_ie_len == new_ie_len &&
		       os_memcmp(old_ie, new_ie, old_ie_len) == 0);

	wpabuf_free(old_ie_buff);
	wpabuf_free(new_ie_buff);

	return ret;
}


static u32 wpa_bss_compare_res(const struct wpa_bss *old,
			       const struct wpa_scan_res *new_res)
{
	u32 changes = 0;
	int caps_diff = old->caps ^ new_res->caps;

	if (old->freq != new_res->freq)
		changes |= WPA_BSS_FREQ_CHANGED_FLAG;

	if (old->level != new_res->level)
		changes |= WPA_BSS_SIGNAL_CHANGED_FLAG;

	if (caps_diff & IEEE80211_CAP_PRIVACY)
		changes |= WPA_BSS_PRIVACY_CHANGED_FLAG;

	if (caps_diff & IEEE80211_CAP_IBSS)
		changes |= WPA_BSS_MODE_CHANGED_FLAG;

	if (old->ie_len == new_res->ie_len &&
	    os_memcmp(wpa_bss_ie_ptr(old), new_res + 1, old->ie_len) == 0)
		return changes;
	changes |= WPA_BSS_IES_CHANGED_FLAG;

	if (!are_ies_equal(old, new_res, WPA_IE_VENDOR_TYPE))
		changes |= WPA_BSS_WPAIE_CHANGED_FLAG;

	if (!are_ies_equal(old, new_res, WLAN_EID_RSN))
		changes |= WPA_BSS_RSNIE_CHANGED_FLAG;

	if (!are_ies_equal(old, new_res, WPS_IE_VENDOR_TYPE))
		changes |= WPA_BSS_WPS_CHANGED_FLAG;

	if (!are_ies_equal(old, new_res, WLAN_EID_SUPP_RATES) ||
	    !are_ies_equal(old, new_res, WLAN_EID_EXT_SUPP_RATES))
		changes |= WPA_BSS_RATES_CHANGED_FLAG;

	return changes;
}


void notify_bss_changes(struct wpa_supplicant *wpa_s, u32 changes,
			const struct wpa_bss *bss)
{
	if (changes & WPA_BSS_FREQ_CHANGED_FLAG)
		wpas_notify_bss_freq_changed(wpa_s, bss->id);

	if (changes & WPA_BSS_SIGNAL_CHANGED_FLAG)
		wpas_notify_bss_signal_changed(wpa_s, bss->id);

	if (changes & WPA_BSS_PRIVACY_CHANGED_FLAG)
		wpas_notify_bss_privacy_changed(wpa_s, bss->id);

	if (changes & WPA_BSS_MODE_CHANGED_FLAG)
		wpas_notify_bss_mode_changed(wpa_s, bss->id);

	if (changes & WPA_BSS_WPAIE_CHANGED_FLAG)
		wpas_notify_bss_wpaie_changed(wpa_s, bss->id);

	if (changes & WPA_BSS_RSNIE_CHANGED_FLAG)
		wpas_notify_bss_rsnie_changed(wpa_s, bss->id);

	if (changes & WPA_BSS_WPS_CHANGED_FLAG)
		wpas_notify_bss_wps_changed(wpa_s, bss->id);

	if (changes & WPA_BSS_IES_CHANGED_FLAG)
		wpas_notify_bss_ies_changed(wpa_s, bss->id);

	if (changes & WPA_BSS_RATES_CHANGED_FLAG)
		wpas_notify_bss_rates_changed(wpa_s, bss->id);

	wpas_notify_bss_seen(wpa_s, bss->id);
}


static struct wpa_bss *
wpa_bss_update(struct wpa_supplicant *wpa_s, struct wpa_bss *bss,
	       struct wpa_scan_res *res, struct os_reltime *fetch_time)
{
	u32 changes;

	if (bss->last_update_idx == wpa_s->bss_update_idx) {
		struct os_reltime update_time;

		/*
		 * Some drivers (e.g., cfg80211) include multiple BSS entries
		 * for the same BSS if that BSS's channel changes. The BSS list
		 * implementation in wpa_supplicant does not do that and we need
		 * to filter out the obsolete results here to make sure only the
		 * most current BSS information remains in the table.
		 */
		wpa_printf(MSG_DEBUG, "BSS: " MACSTR
			   " has multiple entries in the scan results - select the most current one",
			   MAC2STR(bss->bssid));
		calculate_update_time(fetch_time, res->age, &update_time);
		wpa_printf(MSG_DEBUG,
			   "Previous last_update: %u.%06u (freq %d%s)",
			   (unsigned int) bss->last_update.sec,
			   (unsigned int) bss->last_update.usec,
			   bss->freq,
			   (bss->flags & WPA_BSS_ASSOCIATED) ? " assoc" : "");
		wpa_printf(MSG_DEBUG, "New last_update: %u.%06u (freq %d%s)",
			   (unsigned int) update_time.sec,
			   (unsigned int) update_time.usec,
			   res->freq,
			   (res->flags & WPA_SCAN_ASSOCIATED) ? " assoc" : "");
		if ((bss->flags & WPA_BSS_ASSOCIATED) ||
		    (!(res->flags & WPA_SCAN_ASSOCIATED) &&
		     !os_reltime_before(&bss->last_update, &update_time))) {
			wpa_printf(MSG_DEBUG,
				   "Ignore this BSS entry since the previous update looks more current");
			return bss;
		}
		wpa_printf(MSG_DEBUG,
			   "Accept this BSS entry since it looks more current than the previous update");
	}

	changes = wpa_bss_compare_res(bss, res);
	if (changes & WPA_BSS_FREQ_CHANGED_FLAG)
		wpa_printf(MSG_DEBUG, "BSS: " MACSTR " changed freq %d --> %d",
			   MAC2STR(bss->bssid), bss->freq, res->freq);
	bss->scan_miss_count = 0;
	bss->last_update_idx = wpa_s->bss_update_idx;
	wpa_bss_copy_res(bss, res, fetch_time);
	/* Move the entry to the end of the list */
	dl_list_del(&bss->list);
#ifdef CONFIG_P2P
	if (wpa_bss_get_vendor_ie(bss, P2P_IE_VENDOR_TYPE) &&
	    !wpa_scan_get_vendor_ie(res, P2P_IE_VENDOR_TYPE) &&
	    !(changes & WPA_BSS_FREQ_CHANGED_FLAG)) {
		/*
		 * This can happen when non-P2P station interface runs a scan
		 * without P2P IE in the Probe Request frame. P2P GO would reply
		 * to that with a Probe Response that does not include P2P IE.
		 * Do not update the IEs in this BSS entry to avoid such loss of
		 * information that may be needed for P2P operations to
		 * determine group information.
		 */
		wpa_dbg(wpa_s, MSG_DEBUG, "BSS: Do not update scan IEs for "
			MACSTR " since that would remove P2P IE information",
			MAC2STR(bss->bssid));
	} else
#endif /* CONFIG_P2P */
	if (bss->ie_len + bss->beacon_ie_len >=
	    res->ie_len + res->beacon_ie_len) {
		os_memcpy(bss->ies, res + 1, res->ie_len + res->beacon_ie_len);
		bss->ie_len = res->ie_len;
		bss->beacon_ie_len = res->beacon_ie_len;
	} else {
		struct wpa_bss *nbss;
		struct dl_list *prev = bss->list_id.prev;
		struct wpa_connect_work *cwork;
		unsigned int i, j;
		bool update_current_bss = wpa_s->current_bss == bss;
		bool update_ml_probe_bss = wpa_s->ml_connect_probe_bss == bss;
		int update_link_bss = -1;
#ifdef CONFIG_WNM
		bool update_wnm_target = wpa_s->wnm_target_bss == bss;
#endif /* CONFIG_WNM */
#ifdef CONFIG_INTERWORKING
		bool update_interworking = wpa_s->interworking_gas_bss == bss;
#endif /* CONFIG_INTERWORKING */

		for (j = 0; j < MAX_NUM_MLD_LINKS; j++) {
			if (wpa_s->links[j].bss == bss) {
				update_link_bss = j;
				break;
			}
		}

		cwork = wpa_bss_check_pending_connect(wpa_s, bss);

		for (i = 0; i < wpa_s->last_scan_res_used; i++) {
			if (wpa_s->last_scan_res[i] == bss)
				break;
		}

		dl_list_del(&bss->list_id);
		nbss = os_realloc(bss, sizeof(*bss) + res->ie_len +
				  res->beacon_ie_len);
		if (nbss) {
			if (i != wpa_s->last_scan_res_used)
				wpa_s->last_scan_res[i] = nbss;

			if (update_current_bss)
				wpa_s->current_bss = nbss;

			if (update_ml_probe_bss)
				wpa_s->ml_connect_probe_bss = nbss;

			if (update_link_bss >= 0)
				wpa_s->links[update_link_bss].bss = nbss;

#ifdef CONFIG_WNM
			if (update_wnm_target)
				wpa_s->wnm_target_bss = nbss;
#endif /* CONFIG_WNM */
#ifdef CONFIG_INTERWORKING
			if (update_interworking)
				wpa_s->interworking_gas_bss = nbss;
#endif /* CONFIG_INTERWORKING */

			if (cwork)
				wpa_bss_update_pending_connect(cwork, nbss);

			bss = nbss;
			os_memcpy(bss->ies, res + 1,
				  res->ie_len + res->beacon_ie_len);
			bss->ie_len = res->ie_len;
			bss->beacon_ie_len = res->beacon_ie_len;
		}
		dl_list_add(prev, &bss->list_id);
	}
	if (changes & WPA_BSS_IES_CHANGED_FLAG) {
		wpa_bss_set_hessid(bss);

		wpa_bss_parse_basic_ml_element(wpa_s, bss);
	}
	dl_list_add_tail(&wpa_s->bss, &bss->list);

	notify_bss_changes(wpa_s, changes, bss);

	return bss;
}


/**
 * wpa_bss_update_start - Start a BSS table update from scan results
 * @wpa_s: Pointer to wpa_supplicant data
 *
 * This function is called at the start of each BSS table update round for new
 * scan results. The actual scan result entries are indicated with calls to
 * wpa_bss_update_scan_res() and the update round is finished with a call to
 * wpa_bss_update_end().
 */
void wpa_bss_update_start(struct wpa_supplicant *wpa_s)
{
	wpa_s->bss_update_idx++;
	wpa_dbg(wpa_s, MSG_DEBUG, "BSS: Start scan result update %u",
		wpa_s->bss_update_idx);
	wpa_s->last_scan_res_used = 0;
}


/**
 * wpa_bss_update_scan_res - Update a BSS table entry based on a scan result
 * @wpa_s: Pointer to wpa_supplicant data
 * @res: Scan result
 * @fetch_time: Time when the result was fetched from the driver
 *
 * This function updates a BSS table entry (or adds one) based on a scan result.
 * This is called separately for each scan result between the calls to
 * wpa_bss_update_start() and wpa_bss_update_end().
 */
void wpa_bss_update_scan_res(struct wpa_supplicant *wpa_s,
			     struct wpa_scan_res *res,
			     struct os_reltime *fetch_time)
{
	const u8 *ssid, *p2p, *mesh;
	struct wpa_bss *bss;

	if (wpa_s->conf->ignore_old_scan_res) {
		struct os_reltime update;
		calculate_update_time(fetch_time, res->age, &update);
		if (os_reltime_before(&update, &wpa_s->scan_trigger_time)) {
			struct os_reltime age;
			os_reltime_sub(&wpa_s->scan_trigger_time, &update,
				       &age);
			wpa_dbg(wpa_s, MSG_DEBUG, "BSS: Ignore driver BSS "
				"table entry that is %u.%06u seconds older "
				"than our scan trigger",
				(unsigned int) age.sec,
				(unsigned int) age.usec);
			return;
		}
	}

	ssid = wpa_scan_get_ie(res, WLAN_EID_SSID);
	if (ssid == NULL) {
		wpa_dbg(wpa_s, MSG_DEBUG, "BSS: No SSID IE included for "
			MACSTR, MAC2STR(res->bssid));
		return;
	}
	if (ssid[1] > SSID_MAX_LEN) {
		wpa_dbg(wpa_s, MSG_DEBUG, "BSS: Too long SSID IE included for "
			MACSTR, MAC2STR(res->bssid));
		return;
	}

	p2p = wpa_scan_get_vendor_ie(res, P2P_IE_VENDOR_TYPE);
#ifdef CONFIG_P2P
	if (p2p == NULL &&
	    wpa_s->p2p_group_interface != NOT_P2P_GROUP_INTERFACE) {
		/*
		 * If it's a P2P specific interface, then don't update
		 * the scan result without a P2P IE.
		 */
		wpa_printf(MSG_DEBUG, "BSS: No P2P IE - skipping BSS " MACSTR
			   " update for P2P interface", MAC2STR(res->bssid));
		return;
	}
#endif /* CONFIG_P2P */
	if (p2p && ssid[1] == P2P_WILDCARD_SSID_LEN &&
	    os_memcmp(ssid + 2, P2P_WILDCARD_SSID, P2P_WILDCARD_SSID_LEN) == 0)
		return; /* Skip P2P listen discovery results here */

	/* TODO: add option for ignoring BSSes we are not interested in
	 * (to save memory) */

	mesh = wpa_scan_get_ie(res, WLAN_EID_MESH_ID);
	if (mesh && mesh[1] <= SSID_MAX_LEN)
		ssid = mesh;

	bss = wpa_bss_get(wpa_s, res->bssid, ssid + 2, ssid[1]);
	if (bss == NULL)
		bss = wpa_bss_add(wpa_s, ssid + 2, ssid[1], res, fetch_time);
	else {
		bss = wpa_bss_update(wpa_s, bss, res, fetch_time);
		if (wpa_s->last_scan_res) {
			unsigned int i;
			for (i = 0; i < wpa_s->last_scan_res_used; i++) {
				if (bss == wpa_s->last_scan_res[i]) {
					/* Already in the list */
					return;
				}
			}
		}
	}

	if (bss == NULL)
		return;
	if (wpa_s->last_scan_res_used >= wpa_s->last_scan_res_size) {
		struct wpa_bss **n;
		unsigned int siz;
		if (wpa_s->last_scan_res_size == 0)
			siz = 32;
		else
			siz = wpa_s->last_scan_res_size * 2;
		n = os_realloc_array(wpa_s->last_scan_res, siz,
				     sizeof(struct wpa_bss *));
		if (n == NULL)
			return;
		wpa_s->last_scan_res = n;
		wpa_s->last_scan_res_size = siz;
	}

	if (wpa_s->last_scan_res)
		wpa_s->last_scan_res[wpa_s->last_scan_res_used++] = bss;
}


static int wpa_bss_included_in_scan(const struct wpa_bss *bss,
				    const struct scan_info *info)
{
	int found;
	size_t i;

	if (info == NULL)
		return 1;

	if (info->num_freqs) {
		found = 0;
		for (i = 0; i < info->num_freqs; i++) {
			if (bss->freq == info->freqs[i]) {
				found = 1;
				break;
			}
		}
		if (!found)
			return 0;
	}

	if (info->num_ssids) {
		found = 0;
		for (i = 0; i < info->num_ssids; i++) {
			const struct wpa_driver_scan_ssid *s = &info->ssids[i];
			if ((s->ssid == NULL || s->ssid_len == 0) ||
			    (s->ssid_len == bss->ssid_len &&
			     os_memcmp(s->ssid, bss->ssid, bss->ssid_len) ==
			     0)) {
				found = 1;
				break;
			}
		}
		if (!found)
			return 0;
	}

	return 1;
}


/**
 * wpa_bss_update_end - End a BSS table update from scan results
 * @wpa_s: Pointer to wpa_supplicant data
 * @info: Information about scan parameters
 * @new_scan: Whether this update round was based on a new scan
 *
 * This function is called at the end of each BSS table update round for new
 * scan results. The start of the update was indicated with a call to
 * wpa_bss_update_start().
 */
void wpa_bss_update_end(struct wpa_supplicant *wpa_s, struct scan_info *info,
			int new_scan)
{
	struct wpa_bss *bss, *n;

	os_get_reltime(&wpa_s->last_scan);
	if ((info && info->aborted) || !new_scan)
		return; /* do not expire entries without new scan */

	dl_list_for_each_safe(bss, n, &wpa_s->bss, struct wpa_bss, list) {
		if (wpa_bss_in_use(wpa_s, bss))
			continue;
		if (!wpa_bss_included_in_scan(bss, info))
			continue; /* expire only BSSes that were scanned */
		if (bss->last_update_idx < wpa_s->bss_update_idx)
			bss->scan_miss_count++;
		if (bss->scan_miss_count >=
		    wpa_s->conf->bss_expiration_scan_count) {
			wpa_bss_remove(wpa_s, bss, "no match in scan");
		}
	}

	wpa_printf(MSG_DEBUG, "BSS: last_scan_res_used=%zu/%zu",
		   wpa_s->last_scan_res_used, wpa_s->last_scan_res_size);
}


/**
 * wpa_bss_flush_by_age - Flush old BSS entries
 * @wpa_s: Pointer to wpa_supplicant data
 * @age: Maximum entry age in seconds
 *
 * Remove BSS entries that have not been updated during the last @age seconds.
 */
void wpa_bss_flush_by_age(struct wpa_supplicant *wpa_s, int age)
{
	struct wpa_bss *bss, *n;
	struct os_reltime t;

	if (dl_list_empty(&wpa_s->bss))
		return;

	os_get_reltime(&t);

	if (t.sec < age)
		return; /* avoid underflow; there can be no older entries */

	t.sec -= age;

	dl_list_for_each_safe(bss, n, &wpa_s->bss, struct wpa_bss, list) {
		if (wpa_bss_in_use(wpa_s, bss))
			continue;

		if (wpa_s->reassoc_same_ess &&
		    wpa_s->wpa_state != WPA_COMPLETED &&
		    wpa_s->last_ssid &&
		    wpa_s->last_ssid->ssid &&
		    bss->ssid_len == wpa_s->last_ssid->ssid_len &&
		    os_memcmp(bss->ssid, wpa_s->last_ssid->ssid,
			      bss->ssid_len) == 0)
			continue;

		if (os_reltime_before(&bss->last_update, &t)) {
			wpa_bss_remove(wpa_s, bss, __func__);
		} else
			break;
	}
}


/**
 * wpa_bss_init - Initialize BSS table
 * @wpa_s: Pointer to wpa_supplicant data
 * Returns: 0 on success, -1 on failure
 *
 * This prepares BSS table lists and timer for periodic updates. The BSS table
 * is deinitialized with wpa_bss_deinit() once not needed anymore.
 */
int wpa_bss_init(struct wpa_supplicant *wpa_s)
{
	dl_list_init(&wpa_s->bss);
	dl_list_init(&wpa_s->bss_id);
	return 0;
}


/**
 * wpa_bss_flush - Flush all unused BSS entries
 * @wpa_s: Pointer to wpa_supplicant data
 */
void wpa_bss_flush(struct wpa_supplicant *wpa_s)
{
	struct wpa_bss *bss, *n;

	wpa_s->clear_driver_scan_cache = 1;

	if (wpa_s->bss.next == NULL)
		return; /* BSS table not yet initialized */

	dl_list_for_each_safe(bss, n, &wpa_s->bss, struct wpa_bss, list) {
		if (wpa_bss_in_use(wpa_s, bss))
			continue;
		wpa_bss_remove(wpa_s, bss, __func__);
	}
}


/**
 * wpa_bss_deinit - Deinitialize BSS table
 * @wpa_s: Pointer to wpa_supplicant data
 */
void wpa_bss_deinit(struct wpa_supplicant *wpa_s)
{
	wpa_bss_flush(wpa_s);
}


/**
 * wpa_bss_get_bssid - Fetch a BSS table entry based on BSSID
 * @wpa_s: Pointer to wpa_supplicant data
 * @bssid: BSSID
 * Returns: Pointer to the BSS entry or %NULL if not found
 */
struct wpa_bss * wpa_bss_get_bssid(struct wpa_supplicant *wpa_s,
				   const u8 *bssid)
{
	struct wpa_bss *bss;
	if (!wpa_supplicant_filter_bssid_match(wpa_s, bssid))
		return NULL;
	dl_list_for_each_reverse(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (ether_addr_equal(bss->bssid, bssid))
			return bss;
	}
	return NULL;
}


/**
 * wpa_bss_get_bssid_latest - Fetch the latest BSS table entry based on BSSID
 * @wpa_s: Pointer to wpa_supplicant data
 * @bssid: BSSID
 * Returns: Pointer to the BSS entry or %NULL if not found
 *
 * This function is like wpa_bss_get_bssid(), but full BSS table is iterated to
 * find the entry that has the most recent update. This can help in finding the
 * correct entry in cases where the SSID of the AP may have changed recently
 * (e.g., in WPS reconfiguration cases).
 */
struct wpa_bss * wpa_bss_get_bssid_latest(struct wpa_supplicant *wpa_s,
					  const u8 *bssid)
{
	struct wpa_bss *bss, *found = NULL;
	if (!wpa_supplicant_filter_bssid_match(wpa_s, bssid))
		return NULL;
	dl_list_for_each_reverse(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (!ether_addr_equal(bss->bssid, bssid))
			continue;
		if (found == NULL ||
		    os_reltime_before(&found->last_update, &bss->last_update))
			found = bss;
	}
	return found;
}


#ifdef CONFIG_P2P
/**
 * wpa_bss_get_p2p_dev_addr - Fetch the latest BSS table entry based on P2P Device Addr
 * @wpa_s: Pointer to wpa_supplicant data
 * @dev_addr: P2P Device Address of the GO
 * Returns: Pointer to the BSS entry or %NULL if not found
 *
 * This function tries to find the entry that has the most recent update. This
 * can help in finding the correct entry in cases where the SSID of the P2P
 * Device may have changed recently.
 */
struct wpa_bss * wpa_bss_get_p2p_dev_addr(struct wpa_supplicant *wpa_s,
					  const u8 *dev_addr)
{
	struct wpa_bss *bss, *found = NULL;
	dl_list_for_each_reverse(bss, &wpa_s->bss, struct wpa_bss, list) {
		u8 addr[ETH_ALEN];
		if (p2p_parse_dev_addr(wpa_bss_ie_ptr(bss), bss->ie_len,
				       addr) != 0 ||
		    !ether_addr_equal(addr, dev_addr))
			continue;
		if (!found ||
		    os_reltime_before(&found->last_update, &bss->last_update))
			found = bss;
	}
	return found;
}
#endif /* CONFIG_P2P */


/**
 * wpa_bss_get_id - Fetch a BSS table entry based on identifier
 * @wpa_s: Pointer to wpa_supplicant data
 * @id: Unique identifier (struct wpa_bss::id) assigned for the entry
 * Returns: Pointer to the BSS entry or %NULL if not found
 */
struct wpa_bss * wpa_bss_get_id(struct wpa_supplicant *wpa_s, unsigned int id)
{
	struct wpa_bss *bss;
	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (bss->id == id)
			return bss;
	}
	return NULL;
}


/**
 * wpa_bss_get_id_range - Fetch a BSS table entry based on identifier range
 * @wpa_s: Pointer to wpa_supplicant data
 * @idf: Smallest allowed identifier assigned for the entry
 * @idf: Largest allowed identifier assigned for the entry
 * Returns: Pointer to the BSS entry or %NULL if not found
 *
 * This function is similar to wpa_bss_get_id() but allows a BSS entry with the
 * smallest id value to be fetched within the specified range without the
 * caller having to know the exact id.
 */
struct wpa_bss * wpa_bss_get_id_range(struct wpa_supplicant *wpa_s,
				      unsigned int idf, unsigned int idl)
{
	struct wpa_bss *bss;
	dl_list_for_each(bss, &wpa_s->bss_id, struct wpa_bss, list_id) {
		if (bss->id >= idf && bss->id <= idl)
			return bss;
	}
	return NULL;
}


/**
 * wpa_bss_get_ie - Fetch a specified information element from a BSS entry
 * @bss: BSS table entry
 * @ie: Information element identitifier (WLAN_EID_*)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the BSS
 * entry.
 */
const u8 * wpa_bss_get_ie(const struct wpa_bss *bss, u8 ie)
{
	return get_ie(wpa_bss_ie_ptr(bss), bss->ie_len, ie);
}


/**
 * wpa_bss_get_ie_beacon - Fetch a specified information element from a BSS entry
 * @bss: BSS table entry
 * @ie: Information element identitifier (WLAN_EID_*)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the BSS
 * entry.
 *
 * This function is like wpa_bss_get_ie(), but uses IE buffer only from Beacon
 * frames instead of either Beacon or Probe Response frames.
 */
const u8 * wpa_bss_get_ie_beacon(const struct wpa_bss *bss, u8 ie)
{
	const u8 *ies;

	if (bss->beacon_ie_len == 0)
		return NULL;

	ies = wpa_bss_ie_ptr(bss);
	ies += bss->ie_len;
	return get_ie(ies, bss->beacon_ie_len, ie);
}


/**
 * wpa_bss_get_ie_ext - Fetch a specified extended IE from a BSS entry
 * @bss: BSS table entry
 * @ext: Information element extension identifier (WLAN_EID_EXT_*)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the BSS
 * entry.
 */
const u8 * wpa_bss_get_ie_ext(const struct wpa_bss *bss, u8 ext)
{
	return get_ie_ext(wpa_bss_ie_ptr(bss), bss->ie_len, ext);
}


/**
 * wpa_bss_get_vendor_ie - Fetch a vendor information element from a BSS entry
 * @bss: BSS table entry
 * @vendor_type: Vendor type (four octets starting the IE payload)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the BSS
 * entry.
 */
const u8 * wpa_bss_get_vendor_ie(const struct wpa_bss *bss, u32 vendor_type)
{
	const u8 *ies;
	const struct element *elem;

	ies = wpa_bss_ie_ptr(bss);

	for_each_element_id(elem, WLAN_EID_VENDOR_SPECIFIC, ies, bss->ie_len) {
		if (elem->datalen >= 4 &&
		    vendor_type == WPA_GET_BE32(elem->data))
			return &elem->id;
	}

	return NULL;
}


/**
 * wpa_bss_get_vendor_ie_beacon - Fetch a vendor information from a BSS entry
 * @bss: BSS table entry
 * @vendor_type: Vendor type (four octets starting the IE payload)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the BSS
 * entry.
 *
 * This function is like wpa_bss_get_vendor_ie(), but uses IE buffer only
 * from Beacon frames instead of either Beacon or Probe Response frames.
 */
const u8 * wpa_bss_get_vendor_ie_beacon(const struct wpa_bss *bss,
					u32 vendor_type)
{
	const u8 *ies;
	const struct element *elem;

	if (bss->beacon_ie_len == 0)
		return NULL;

	ies = wpa_bss_ie_ptr(bss);
	ies += bss->ie_len;

	for_each_element_id(elem, WLAN_EID_VENDOR_SPECIFIC, ies,
			    bss->beacon_ie_len) {
		if (elem->datalen >= 4 &&
		    vendor_type == WPA_GET_BE32(elem->data))
			return &elem->id;
	}

	return NULL;
}


/**
 * wpa_bss_get_vendor_ie_multi - Fetch vendor IE data from a BSS entry
 * @bss: BSS table entry
 * @vendor_type: Vendor type (four octets starting the IE payload)
 * Returns: Pointer to the information element payload or %NULL if not found
 *
 * This function returns concatenated payload of possibly fragmented vendor
 * specific information elements in the BSS entry. The caller is responsible for
 * freeing the returned buffer.
 */
struct wpabuf * wpa_bss_get_vendor_ie_multi(const struct wpa_bss *bss,
					    u32 vendor_type)
{
	struct wpabuf *buf;
	const u8 *end, *pos;

	buf = wpabuf_alloc(bss->ie_len);
	if (buf == NULL)
		return NULL;

	pos = wpa_bss_ie_ptr(bss);
	end = pos + bss->ie_len;

	while (end - pos > 1) {
		u8 ie, len;

		ie = pos[0];
		len = pos[1];
		if (len > end - pos - 2)
			break;
		pos += 2;
		if (ie == WLAN_EID_VENDOR_SPECIFIC && len >= 4 &&
		    vendor_type == WPA_GET_BE32(pos))
			wpabuf_put_data(buf, pos + 4, len - 4);
		pos += len;
	}

	if (wpabuf_len(buf) == 0) {
		wpabuf_free(buf);
		buf = NULL;
	}

	return buf;
}


/**
 * wpa_bss_get_vendor_ie_multi_beacon - Fetch vendor IE data from a BSS entry
 * @bss: BSS table entry
 * @vendor_type: Vendor type (four octets starting the IE payload)
 * Returns: Pointer to the information element payload or %NULL if not found
 *
 * This function returns concatenated payload of possibly fragmented vendor
 * specific information elements in the BSS entry. The caller is responsible for
 * freeing the returned buffer.
 *
 * This function is like wpa_bss_get_vendor_ie_multi(), but uses IE buffer only
 * from Beacon frames instead of either Beacon or Probe Response frames.
 */
struct wpabuf * wpa_bss_get_vendor_ie_multi_beacon(const struct wpa_bss *bss,
						   u32 vendor_type)
{
	struct wpabuf *buf;
	const u8 *end, *pos;

	buf = wpabuf_alloc(bss->beacon_ie_len);
	if (buf == NULL)
		return NULL;

	pos = wpa_bss_ie_ptr(bss);
	pos += bss->ie_len;
	end = pos + bss->beacon_ie_len;

	while (end - pos > 1) {
		u8 id, len;

		id = *pos++;
		len = *pos++;
		if (len > end - pos)
			break;
		if (id == WLAN_EID_VENDOR_SPECIFIC && len >= 4 &&
		    vendor_type == WPA_GET_BE32(pos))
			wpabuf_put_data(buf, pos + 4, len - 4);
		pos += len;
	}

	if (wpabuf_len(buf) == 0) {
		wpabuf_free(buf);
		buf = NULL;
	}

	return buf;
}


/**
 * wpa_bss_get_max_rate - Get maximum legacy TX rate supported in a BSS
 * @bss: BSS table entry
 * Returns: Maximum legacy rate in units of 500 kbps
 */
int wpa_bss_get_max_rate(const struct wpa_bss *bss)
{
	int rate = 0;
	const u8 *ie;
	int i;

	ie = wpa_bss_get_ie(bss, WLAN_EID_SUPP_RATES);
	for (i = 0; ie && i < ie[1]; i++) {
		if ((ie[i + 2] & 0x7f) > rate)
			rate = ie[i + 2] & 0x7f;
	}

	ie = wpa_bss_get_ie(bss, WLAN_EID_EXT_SUPP_RATES);
	for (i = 0; ie && i < ie[1]; i++) {
		if ((ie[i + 2] & 0x7f) > rate)
			rate = ie[i + 2] & 0x7f;
	}

	return rate;
}


/**
 * wpa_bss_get_bit_rates - Get legacy TX rates supported in a BSS
 * @bss: BSS table entry
 * @rates: Buffer for returning a pointer to the rates list (units of 500 kbps)
 * Returns: number of legacy TX rates or -1 on failure
 *
 * The caller is responsible for freeing the returned buffer with os_free() in
 * case of success.
 */
int wpa_bss_get_bit_rates(const struct wpa_bss *bss, u8 **rates)
{
	const u8 *ie, *ie2;
	int i, j;
	unsigned int len;
	u8 *r;

	ie = wpa_bss_get_ie(bss, WLAN_EID_SUPP_RATES);
	ie2 = wpa_bss_get_ie(bss, WLAN_EID_EXT_SUPP_RATES);

	len = (ie ? ie[1] : 0) + (ie2 ? ie2[1] : 0);

	r = os_malloc(len);
	if (!r)
		return -1;

	for (i = 0; ie && i < ie[1]; i++)
		r[i] = ie[i + 2] & 0x7f;

	for (j = 0; ie2 && j < ie2[1]; j++)
		r[i + j] = ie2[j + 2] & 0x7f;

	*rates = r;
	return len;
}


#ifdef CONFIG_FILS
const u8 * wpa_bss_get_fils_cache_id(const struct wpa_bss *bss)
{
	const u8 *ie;

	if (bss) {
		ie = wpa_bss_get_ie(bss, WLAN_EID_FILS_INDICATION);
		if (ie && ie[1] >= 4 && WPA_GET_LE16(ie + 2) & BIT(7))
			return ie + 4;
	}

	return NULL;
}
#endif /* CONFIG_FILS */


int wpa_bss_ext_capab(const struct wpa_bss *bss, unsigned int capab)
{
	if (!bss)
		return 0;
	return ieee802_11_ext_capab(wpa_bss_get_ie(bss, WLAN_EID_EXT_CAPAB),
				    capab);
}


static void
wpa_bss_parse_ml_rnr_ap_info(struct wpa_supplicant *wpa_s,
			     struct wpa_bss *bss, u8 ap_mld_id,
			     const struct ieee80211_neighbor_ap_info *ap_info,
			     size_t len, u16 *seen)
{
	const u8 *pos, *end;
	const u8 *mld_params;
	u8 count, mld_params_offset;
	u8 i, type, link_id;

	count = RNR_TBTT_INFO_COUNT_VAL(ap_info->tbtt_info_hdr) + 1;
	type = ap_info->tbtt_info_hdr & RNR_TBTT_INFO_HDR_TYPE_MSK;

	/* MLD information is at offset 13 or at start */
	if (type == 0 && ap_info->tbtt_info_len >= RNR_TBTT_INFO_MLD_LEN) {
		/* MLD info is appended */
		mld_params_offset = RNR_TBTT_INFO_LEN;
	} else {
		/* TODO: Support NSTR AP */
		return;
	}

	pos = (const u8 *) ap_info;
	end = pos + len;
	pos += sizeof(*ap_info);

	for (i = 0; i < count; i++, pos += ap_info->tbtt_info_len) {
		if (end - pos < ap_info->tbtt_info_len)
			break;

		mld_params = pos + mld_params_offset;

		link_id = *(mld_params + 1) & EHT_ML_LINK_ID_MSK;
		if (link_id >= MAX_NUM_MLD_LINKS)
			continue;

		if (*mld_params != ap_mld_id) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Reported link not part of MLD");
		} else if (!(BIT(link_id) & *seen)) {
			struct mld_link *l;

			*seen |= BIT(link_id);
			wpa_printf(MSG_DEBUG, "MLD: mld ID=%u, link ID=%u",
				   *mld_params, link_id);

			bss->valid_links |= BIT(link_id);
			l = &bss->mld_links[link_id];
			os_memcpy(l->bssid, pos + 1, ETH_ALEN);
			l->disabled = mld_params[2] &
				RNR_TBTT_INFO_MLD_PARAM2_LINK_DISABLED;
			l->freq = ieee80211_chan_to_freq(NULL,
							 ap_info->op_class,
							 ap_info->channel);
		}
	}
}


/**
 * wpa_bss_validate_rsne_ml - Validate RSN IEs (RSNE/RSNOE/RSNO2E) of a BSS
 * @wpa_s: Pointer to wpa_supplicant data
 * @ssid: Network config
 * @bss: BSS table entry
 * @rsne_type_p: Type of RSNE to validate. If -1 is given, choose as per the
 *	presence of RSN elements (association link); otherwise, validate
 *	against the requested type (other affiliated links).
 * @ref_rsne: Buffer for RSNE data; filled in from the main link to compare
 *	against and used internally
 * Returns: true if the BSS configuration matches local profile and the elements
 * meet MLO requirements, false otherwise
 */
static bool
wpa_bss_validate_rsne_ml(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid,
			 struct wpa_bss *bss, int *rsne_type_p,
			 struct wpa_ie_data *ref_rsne)
{
	struct ieee802_11_elems elems;
	struct wpa_ie_data wpa_ie;
	const u8 *rsne;
	size_t rsne_len;
	int rsne_type;
	const u8 *ies_pos = wpa_bss_ie_ptr(bss);
	size_t ies_len = bss->ie_len ? bss->ie_len : bss->beacon_ie_len;

	if (ieee802_11_parse_elems(ies_pos, ies_len, &elems, 0) ==
	    ParseFailed) {
		wpa_dbg(wpa_s, MSG_DEBUG, "MLD: Failed to parse elements");
		return false;
	}

	if (elems.rsne_override_2 && wpas_rsn_overriding(wpa_s, ssid)) {
		rsne = elems.rsne_override_2;
		rsne_len = elems.rsne_override_2_len;
		rsne_type = 2;
	} else if (elems.rsne_override && wpas_rsn_overriding(wpa_s, ssid)) {
		rsne = elems.rsne_override;
		rsne_len = elems.rsne_override_len;
		rsne_type = 1;
	} else {
		rsne = elems.rsn_ie;
		rsne_len = elems.rsn_ie_len;
		rsne_type = 0;
	}

	if (!rsne ||
	    wpa_parse_wpa_ie(rsne - 2, 2 + rsne_len, &wpa_ie)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "MLD: No RSN element");
		return false;
	}

	if (*rsne_type_p != -1 && *rsne_type_p != rsne_type) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"MLD: No matching RSN element (RSNO mismatch)");
		return false;
	}

	if (!(wpa_ie.capabilities & WPA_CAPABILITY_MFPC) ||
	    wpas_get_ssid_pmf(wpa_s, ssid) == NO_MGMT_FRAME_PROTECTION) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"MLD: No management frame protection");
		return false;
	}

	wpa_ie.key_mgmt &= ~(WPA_KEY_MGMT_PSK | WPA_KEY_MGMT_FT_PSK |
			     WPA_KEY_MGMT_PSK_SHA256);
	if (!(wpa_ie.key_mgmt & ssid->key_mgmt)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "MLD: No valid key management");
		return false;
	}
	wpa_dbg(wpa_s, MSG_DEBUG, "MLD: key_mgmt=0x%x", wpa_ie.key_mgmt);

	wpa_ie.pairwise_cipher &= ~(WPA_CIPHER_NONE | WPA_CIPHER_WEP40 |
				    WPA_CIPHER_WEP104 | WPA_CIPHER_TKIP);
	if (!(wpa_ie.pairwise_cipher & ssid->pairwise_cipher)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "MLD: No valid pairwise cipher");
		return false;
	}

	if (*rsne_type_p == -1) {
		os_memcpy(ref_rsne, &wpa_ie, sizeof(wpa_ie));

		*rsne_type_p = rsne_type;
	} else {
		/* Verify the neighbor given rsne_type_p and ref_rsne */
		if (!(wpa_ie.key_mgmt & ref_rsne->key_mgmt)) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"MLD: Neighbor without common AKM");
			return false;
		}

		if (!(wpa_ie.pairwise_cipher & ref_rsne->pairwise_cipher)) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"MLD: Neighbor without common pairwise cipher");
			return false;
		}
	}

	return true;
}


/*
 * SME-based association with an AP MLD requires the driver to have a
 * sufficiently recent scan entry for every requested link. With nl80211-based
 * drivers, cfg80211 rejects the association if the BSS entry for any link is
 * older than 30 seconds (IEEE80211_SCAN_RESULT_EXPIRE). Since the BSS table in
 * wpa_supplicant keeps entries considerably longer than that
 * (bss_expiration_age, 180 seconds by default) and does not refresh entries for
 * the links of the current AP MLD while associated, consider an affiliated link
 * with an older scan entry to be missing so that an ML probe request gets used
 * to refresh the information before association. Leave some margin below the
 * kernel limit to cover the time needed for authentication.
 */
#define WPA_BSS_ML_LINK_FRESH_AGE 25 /* seconds */

/**
 * wpa_bss_get_usable_links - Retrieve the usable links of the AP MLD
 * @wpa_s: Pointer to wpa_supplicant data
 * @bss: BSS table entry
 * @ssid: Target SSID (or %NULL)
 * @missing_links: Result bitmask of links that were not discovered (or %NULL)
 * Returns: Bitmap of links that are usable, or 0 for non-MLD or failure
 *
 * Validate each link of the MLD to verify that it is compatible and connection
 * to each of the links is allowed.
 */
u16 wpa_bss_get_usable_links(struct wpa_supplicant *wpa_s, struct wpa_bss *bss,
			     struct wpa_ssid *ssid, u16 *missing_links)
{
	struct wpa_ie_data rsne;
	struct os_reltime now;
	int rsne_type;
	u16 usable_links = 0;
	u8 link_id;

	if (!bss->valid_links)
		return 0;

	os_get_reltime(&now);

	rsne_type = -1;
	os_memset(&rsne, 0, sizeof(rsne));
	if (ssid &&
	    !wpa_bss_validate_rsne_ml(wpa_s, ssid, bss, &rsne_type, &rsne)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "MLD: No valid key management");
		return 0;
	}

	usable_links = BIT(bss->mld_link_id);

	for_each_link(bss->valid_links, link_id) {
		struct wpa_bss *neigh_bss;
		u16 ext_mld_capa_mask;

		if (link_id == bss->mld_link_id)
			continue;

		if (ssid && ssid->ssid_len)
			neigh_bss = wpa_bss_get(wpa_s,
						bss->mld_links[link_id].bssid,
						ssid->ssid,
						ssid->ssid_len);
		else
			neigh_bss = wpa_bss_get_bssid(wpa_s,
						      bss->mld_links[link_id].bssid);

		if (!neigh_bss) {
			if (missing_links)
				*missing_links |= BIT(link_id);
			continue;
		}

		if (os_reltime_expired(&now, &neigh_bss->last_update,
				       WPA_BSS_ML_LINK_FRESH_AGE)) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"MLD: Scan entry for neighbor " MACSTR
				" is too old for association; consider link %u missing",
				MAC2STR(neigh_bss->bssid), link_id);
			if (missing_links)
				*missing_links |= BIT(link_id);
			continue;
		}

		/* Check that the affiliated links are for the same AP MLD and
		 * the information matches */
		if (!neigh_bss->valid_links) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"MLD: Neighbor without Multi-Link support");
			continue;
		}

		if (neigh_bss->mld_link_id != link_id) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"MLD: Neighbor has unexpected link ID (%d != %d)",
				neigh_bss->mld_link_id, link_id);
			continue;
		}

		if (!ether_addr_equal(bss->mld_addr, neigh_bss->mld_addr)) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"MLD: Neighbor has a different MLD MAC address ("
				MACSTR " != " MACSTR ")",
				MAC2STR(neigh_bss->mld_addr),
				MAC2STR(bss->mld_addr));
			continue;
		}

		if ((bss->mld_capa & ~EHT_ML_MLD_CAPA_RESERVED) !=
		    (neigh_bss->mld_capa & ~EHT_ML_MLD_CAPA_RESERVED)) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"MLD: Neighbor's MLD Capabilities do not match (0x%04x != 0x%04x)",
				neigh_bss->mld_capa, bss->mld_capa);
			continue;
		}

		if ((bss->eml_capa & ~EHT_ML_EML_CAPA_RESERVED) !=
		    (neigh_bss->eml_capa & ~EHT_ML_EML_CAPA_RESERVED)) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"MLD: Neighbor's EML Capabilities do not match (0x%04x != 0x%04x",
				neigh_bss->eml_capa, bss->eml_capa);
			continue;
		}

		/*
		 * Check well-defined values in Extended MLD Capabilities.
		 * In particular the Recommended Max Simultaneous Links
		 * subfield may change over time and is reserved depending on
		 * the frame that it is carried in.
		 * See IEEE Std 802.11be-2024, Table 9-417o.
		 */
		ext_mld_capa_mask =
			EHT_ML_EXT_MLD_CAPA_OP_PARAM_UPDATE |
			EHT_ML_EXT_MLD_CAPA_NSTR_UPDATE |
			EHT_ML_EXT_MLD_CAPA_EMLSR_ENA_ONE_LINK;
		if ((bss->ext_mld_capa & ext_mld_capa_mask) !=
		    (neigh_bss->ext_mld_capa & ext_mld_capa_mask)) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"MLD: Neighbors Extended MLD Capabilities do not match (0x%04x != 0x%04x)",
				neigh_bss->ext_mld_capa, bss->ext_mld_capa);
			continue;
		}

		if (ssid) {
			/* As per IEEE Std 802.11be-2024, 12.6.2 (RSNA
			 * selection), all APs affiliated with an AP MLD shall
			 * advertise at least one common AKM suite selector in
			 * the AKM Suite List field of the RSNE. Discard links
			 * that do not have compatible configuration with the
			 * association link.
			 */
			if (!wpa_bss_validate_rsne_ml(wpa_s, ssid, neigh_bss,
						      &rsne_type, &rsne)) {
				wpa_printf(MSG_DEBUG,
					   "MLD: Discard link %u due to RSN parameter mismatch",
					   link_id);
				continue;
			}
		}

		if ((!ssid ||
		     wpa_scan_res_match(wpa_s, 0, neigh_bss, ssid, 1, 0,
					true)) &&
		    !wpa_bssid_ignore_is_listed(wpa_s, neigh_bss->bssid)) {
			usable_links |= BIT(link_id);
		}
	}

	return usable_links;
}


/**
 * wpa_bss_parse_basic_ml_element - Parse the Basic Multi-Link element
 * @wpa_s: Pointer to wpa_supplicant data
 * @bss: BSS table entry
 *
 * Parses the Basic Multi-Link element of the BSS into @link_info using the scan
 * information stored in the wpa_supplicant data to fill in information for
 * links where possible.
 */
void wpa_bss_parse_basic_ml_element(struct wpa_supplicant *wpa_s,
				    struct wpa_bss *bss)
{
	struct ieee802_11_elems elems;
	struct wpabuf *mlbuf = NULL;
	const struct element *elem;
	size_t ml_ie_len;
	const struct ieee80211_eht_ml *eht_ml;
	const struct eht_ml_basic_common_info *ml_basic_common_info;
	const u8 *mbssid_idx_elem;
	u8 i, pos, link_id, ap_mld_id;
	const u16 control_mask =
		MULTI_LINK_CONTROL_TYPE_MASK |
		BASIC_MULTI_LINK_CTRL_PRES_LINK_ID |
		BASIC_MULTI_LINK_CTRL_PRES_BSS_PARAM_CH_COUNT |
		BASIC_MULTI_LINK_CTRL_PRES_MLD_CAPA;
	const u16 control =
		MULTI_LINK_CONTROL_TYPE_BASIC |
		BASIC_MULTI_LINK_CTRL_PRES_LINK_ID |
		BASIC_MULTI_LINK_CTRL_PRES_BSS_PARAM_CH_COUNT |
		BASIC_MULTI_LINK_CTRL_PRES_MLD_CAPA;
	u16 seen;
	const u8 *ies_pos = wpa_bss_ie_ptr(bss);
	size_t ies_len = bss->ie_len ? bss->ie_len : bss->beacon_ie_len;
	struct mld_link *l;

	if (ieee802_11_parse_elems(ies_pos, ies_len, &elems, 1) ==
	    ParseFailed) {
		wpa_dbg(wpa_s, MSG_DEBUG, "MLD: Failed to parse elements");
		goto out;
	}

	mlbuf = ieee802_11_defrag(elems.basic_mle, elems.basic_mle_len, true);
	if (!mlbuf) {
		wpa_dbg(wpa_s, MSG_DEBUG, "MLD: No Multi-Link element");
		goto out;
	}

	ml_ie_len = wpabuf_len(mlbuf);

	/*
	 * for ext ID + 2 control + common info len
	 */
	if (ml_ie_len < sizeof(*eht_ml) + sizeof(*ml_basic_common_info))
		goto out;

	eht_ml = (const struct ieee80211_eht_ml *) wpabuf_head(mlbuf);
	if ((le_to_host16(eht_ml->ml_control) & control_mask) != control) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Unexpected Multi-Link element control=0x%x (mask 0x%x expected 0x%x)",
			   le_to_host16(eht_ml->ml_control), control_mask,
			   control);
		goto out;
	}

	ml_basic_common_info =
		(const struct eht_ml_basic_common_info *) eht_ml->variable;

	if (ml_ie_len < sizeof(*eht_ml) + ml_basic_common_info->len)
		goto out;

	/* Minimum Common info length to be valid */
	if (ml_basic_common_info->len <
	    sizeof(*ml_basic_common_info) + 1 + 1 + 2)
		goto out;

	/* Link ID Info, BSS Parameters Change Count (see control/control_mask)
	 */
	pos = 1 + 1;

	/* Medium Synchronization Delay Information */
	if (le_to_host16(eht_ml->ml_control) &
	    BASIC_MULTI_LINK_CTRL_PRES_MSD_INFO) {
		if (ml_basic_common_info->len <
		    sizeof(*ml_basic_common_info) + pos + 2)
			goto out;
		pos += 2;
	}

	/* EML Capabilities */
	bss->eml_capa = 0;
	if (le_to_host16(eht_ml->ml_control) &
	    BASIC_MULTI_LINK_CTRL_PRES_EML_CAPA) {
		if (ml_basic_common_info->len <
		    sizeof(*ml_basic_common_info) + pos + 2)
			goto out;
		bss->eml_capa =
			WPA_GET_LE16(&ml_basic_common_info->variable[pos]);
		pos += 2;
	}

	/* MLD Capabilities And Operations (always present, see
	 * control/control_mask) */
	if (ml_basic_common_info->len < sizeof(*ml_basic_common_info) + pos + 2)
		goto out;
	bss->mld_capa = WPA_GET_LE16(&ml_basic_common_info->variable[pos]);
	pos += 2;

	/* AP MLD ID from MLE if present (see comment below) */
	if (le_to_host16(eht_ml->ml_control) &
	    BASIC_MULTI_LINK_CTRL_PRES_AP_MLD_ID) {
		if (ml_basic_common_info->len <
		    sizeof(*ml_basic_common_info) + pos + 1)
			goto out;

		ap_mld_id = ml_basic_common_info->variable[pos];

		pos++;
	} else {
		ap_mld_id = 0;
	}

	/* Extended MLD Capabilities And Operations */
	bss->ext_mld_capa = 0;
	if (le_to_host16(eht_ml->ml_control) &
	    BASIC_MULTI_LINK_CTRL_PRES_EXT_MLD_CAP) {
		if (ml_basic_common_info->len <
		    sizeof(*ml_basic_common_info) + pos + 2)
			goto out;

		bss->ext_mld_capa =
			WPA_GET_LE16(&ml_basic_common_info->variable[pos]);

		pos += 2;
	}

	if (ml_basic_common_info->len < sizeof(*ml_basic_common_info) + pos)
		goto out;

	link_id = ml_basic_common_info->variable[0] & EHT_ML_LINK_ID_MSK;
	if (link_id >= MAX_NUM_MLD_LINKS) {
		wpa_printf(MSG_DEBUG, "MLD: Invalid link ID %u in Basic MLE",
			   link_id);
		goto out;
	}

	os_memcpy(bss->mld_addr, ml_basic_common_info->mld_addr, ETH_ALEN);

	bss->mld_link_id = link_id;
	bss->valid_links = BIT(link_id);
	seen = bss->valid_links;

	l = &bss->mld_links[link_id];
	os_memcpy(l->bssid, bss->bssid, ETH_ALEN);
	l->freq = bss->freq;

	bss->mld_bss_non_transmitted = false;

	/*
	 * We should be able to rely on the Multiple BSSID Index element
	 * to be included if the BSS is nontransmitted. Both if it was
	 * extracted from a beacon and if it came from an ML probe
	 * response (i.e. not listed in IEEE Std 802.11be-2024, 35.3.3.4).
	 *
	 * Note that the AP MLD ID and the Multiple-BSSID Index will be
	 * identical if the information was reported by the
	 * corresponding transmitting AP (IEEE Std 802.11be-2024, 9.4.2.169.2).
	 * As an AP MLD ID will not be explicitly provided we need to
	 * rely on the Multiple-BSSID Index element. This is generally the case
	 * when the BSS information was read from a Multiple-BSSID element.
	 *
	 * The alternative scenario is a BSS discovered using a
	 * Multi-Link Probe Response. In that case, we can still
	 * determine whether the BSS is nontransmitted or not using the
	 * Multiple BSSID-Index element. However, the AP MLD ID may be
	 * different inside the ML Probe Response and the driver also
	 * needs to deal with this during inheritance.
	 *
	 * We assume the driver either
	 *  - includes the appropriate AP MLD ID in the MLE it generates
	 *    (see above), or
	 *  - rewrites the RNR so that the AP MLD ID matches the
	 *    Multiple-BSSID Index element.
	 */
	mbssid_idx_elem = wpa_bss_get_ie(bss, WLAN_EID_MULTIPLE_BSSID_INDEX);
	if (mbssid_idx_elem && mbssid_idx_elem[1] >= 1) {
		if (!(le_to_host16(eht_ml->ml_control) &
		      BASIC_MULTI_LINK_CTRL_PRES_AP_MLD_ID))
			ap_mld_id = mbssid_idx_elem[2];
		bss->mld_bss_non_transmitted = !!mbssid_idx_elem[2];
	}

	for_each_element_id(elem, WLAN_EID_REDUCED_NEIGHBOR_REPORT,
			    wpa_bss_ie_ptr(bss),
			    bss->ie_len ? bss->ie_len : bss->beacon_ie_len) {
		const struct ieee80211_neighbor_ap_info *ap_info;
		const u8 *ap_info_pos = elem->data;
		size_t len = elem->datalen;

		/* RNR IE may contain more than one Neighbor AP Info */
		while (sizeof(*ap_info) <= len) {
			size_t ap_info_len = sizeof(*ap_info);
			u8 count;

			ap_info = (const struct ieee80211_neighbor_ap_info *)
				ap_info_pos;
			count = RNR_TBTT_INFO_COUNT_VAL(ap_info->tbtt_info_hdr) + 1;
			ap_info_len += count * ap_info->tbtt_info_len;

			if (ap_info_len > len)
				goto out;

			wpa_bss_parse_ml_rnr_ap_info(wpa_s, bss, ap_mld_id,
						     ap_info, len, &seen);

			ap_info_pos += ap_info_len;
			len -= ap_info_len;
		}
	}

	wpa_printf(MSG_DEBUG, "MLD: valid_links=0x%04hx",
		   bss->valid_links);

	for_each_link(bss->valid_links, i) {
		wpa_printf(MSG_DEBUG, "MLD: link=%u, bssid=" MACSTR,
			   i, MAC2STR(bss->mld_links[i].bssid));
	}

	wpabuf_free(mlbuf);
	return;

out:
	os_memset(bss->mld_addr, 0, ETH_ALEN);
	bss->valid_links = 0;
	wpabuf_free(mlbuf);
}


/*
 * wpa_bss_parse_reconf_ml_element - Parse the Reconfiguration ML element
 * @wpa_s: Pointer to wpa_supplicant data
 * @bss: BSS table entry
 * Returns: The bitmap of links that are going to be removed
 */
u16 wpa_bss_parse_reconf_ml_element(struct wpa_supplicant *wpa_s,
				    struct wpa_bss *bss)
{
	struct ieee802_11_elems elems;
	struct wpabuf *mlbuf;
	const u8 *pos = wpa_bss_ie_ptr(bss);
	size_t len = bss->ie_len ? bss->ie_len : bss->beacon_ie_len;
	const struct ieee80211_eht_ml *ml;
	const struct eht_ml_reconf_common_info *common_info;
	u16 removed_links = 0;
	u8 expected_ml_common_len;

	if (ieee802_11_parse_elems(pos, len, &elems, 1) == ParseFailed)
		return 0;

	if (!elems.reconf_mle || !elems.reconf_mle_len)
		return 0;

	mlbuf = ieee802_11_defrag(elems.reconf_mle, elems.reconf_mle_len, true);
	if (!mlbuf)
		return 0;

	ml = (const struct ieee80211_eht_ml *) wpabuf_head(mlbuf);
	len = wpabuf_len(mlbuf);

	/* There must be at least one octet for the Common Info Length subfield
	 */
	if (len < sizeof(*ml) + 1UL)
		goto out;

	expected_ml_common_len = 1;
	if (le_to_host16(ml->ml_control) &
	    RECONF_MULTI_LINK_CTRL_PRES_MLD_MAC_ADDR)
		expected_ml_common_len += ETH_ALEN;

	common_info = (const struct eht_ml_reconf_common_info *) ml->variable;
	if (len < sizeof(*ml) + common_info->len) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Unexpected Reconfiguration ML element length: (%zu < %zu)",
			   len, sizeof(*ml) + common_info->len);
		goto out;
	}

	if (common_info->len < expected_ml_common_len) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Invalid common info len=%u; min expected=%u",
			   common_info->len, expected_ml_common_len);
		goto out;
	}

	pos = ml->variable + common_info->len;
	len -= sizeof(*ml) + common_info->len;

	while (len >= 2 + sizeof(struct ieee80211_eht_per_sta_profile)) {
		size_t sub_elem_len;
		int num_frag_subelems;

		num_frag_subelems =
			ieee802_11_defrag_mle_subelem(mlbuf, pos,
						      &sub_elem_len);
		if (num_frag_subelems < 0) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Failed to parse MLE subelem");
			break;
		}

		if ((size_t) num_frag_subelems * 2 > len)
			goto out;
		len -= num_frag_subelems * 2;

		if (2 + sub_elem_len > len) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Invalid link info len: %zu %zu",
				   2 + sub_elem_len, len);
			goto out;
		}

		if  (*pos == MULTI_LINK_SUB_ELEM_ID_PER_STA_PROFILE &&
		     sub_elem_len >= 2) {
			const struct ieee80211_eht_per_sta_profile *sta_prof =
				(const struct ieee80211_eht_per_sta_profile *)
				(pos + 2);
			u16 control = le_to_host16(sta_prof->sta_control);
			u8 link_id;

			link_id = control & EHT_PER_STA_RECONF_CTRL_LINK_ID_MSK;
			if (link_id < MAX_NUM_MLD_LINKS)
				removed_links |= BIT(link_id);
		}

		pos += 2 + sub_elem_len;
		len -= 2 + sub_elem_len;
	}

	wpa_printf(MSG_DEBUG, "MLD: Reconfiguration: removed_links=0x%x",
		   removed_links);
out:
	wpabuf_free(mlbuf);
	return removed_links;
}


#ifndef CONFIG_NO_WPA

static bool wpa_bss_supported_cipher(struct wpa_supplicant *wpa_s,
				     int pairwise_cipher)
{
	if (!wpa_s->drv_enc)
		return true;

	if ((pairwise_cipher & WPA_CIPHER_CCMP) &&
	    (wpa_s->drv_enc & WPA_DRIVER_CAPA_ENC_CCMP))
		return true;

	if ((pairwise_cipher & WPA_CIPHER_GCMP) &&
	    (wpa_s->drv_enc & WPA_DRIVER_CAPA_ENC_GCMP))
		return true;

	if ((pairwise_cipher & WPA_CIPHER_CCMP_256) &&
	    (wpa_s->drv_enc & WPA_DRIVER_CAPA_ENC_CCMP_256))
		return true;

	if ((pairwise_cipher & WPA_CIPHER_GCMP_256) &&
	    (wpa_s->drv_enc & WPA_DRIVER_CAPA_ENC_GCMP_256))
		return true;

	return false;
}


static bool wpa_bss_supported_key_mgmt(struct wpa_supplicant *wpa_s,
				       int key_mgmt)
{
	if (!wpa_s->drv_key_mgmt)
		return true;

	if ((key_mgmt & WPA_KEY_MGMT_IEEE8021X) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_WPA2))
		return true;
	if ((key_mgmt & WPA_KEY_MGMT_IEEE8021X_SHA256) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_802_1X_SHA256))
		return true;
	if ((key_mgmt & WPA_KEY_MGMT_FT_IEEE8021X) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_FT))
		return true;
	if ((key_mgmt & WPA_KEY_MGMT_FT_IEEE8021X_SHA384) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_FT_802_1X_SHA384))
		return true;
	if ((key_mgmt & WPA_KEY_MGMT_IEEE8021X_SUITE_B) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_SUITE_B))
		return true;
	if ((key_mgmt & WPA_KEY_MGMT_IEEE8021X_SUITE_B_192) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_SUITE_B_192))
		return true;
	if ((key_mgmt & WPA_KEY_MGMT_PSK) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK))
		return true;
	if ((key_mgmt & WPA_KEY_MGMT_FT_PSK) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_FT_PSK))
		return true;
	if ((key_mgmt & WPA_KEY_MGMT_PSK_SHA256) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_PSK_SHA256))
		return true;
	if ((key_mgmt & WPA_KEY_MGMT_SAE) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_SAE))
		return true;
	if ((key_mgmt & WPA_KEY_MGMT_SAE_EXT_KEY) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_SAE_EXT_KEY))
		return true;
	if ((key_mgmt & WPA_KEY_MGMT_FT_SAE) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_FT_SAE))
		return true;
	if ((key_mgmt & WPA_KEY_MGMT_FT_SAE_EXT_KEY) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_FT_SAE_EXT_KEY))
		return true;
	if ((key_mgmt & WPA_KEY_MGMT_OWE) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_OWE))
		return true;
	if ((key_mgmt & WPA_KEY_MGMT_DPP) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_DPP))
		return true;
	if ((key_mgmt & WPA_KEY_MGMT_FILS_SHA256) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_FILS_SHA256))
		return true;
	if ((key_mgmt & WPA_KEY_MGMT_FILS_SHA384) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_FILS_SHA384))
		return true;
	if ((key_mgmt & WPA_KEY_MGMT_FT_FILS_SHA256) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_FT_FILS_SHA256))
		return true;
	if ((key_mgmt & WPA_KEY_MGMT_FT_FILS_SHA384) &&
	    (wpa_s->drv_key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_FT_FILS_SHA384))
		return true;

	return false;
}


static bool wpa_bss_supported_rsne(struct wpa_supplicant *wpa_s,
				   struct wpa_ssid *ssid, const u8 *ie)
{
	struct wpa_ie_data data;

	if (wpa_parse_wpa_ie_rsn(ie, 2 + ie[1], &data) < 0)
		return false;

	/* Check that there is a supported AKM and pairwise cipher based on
	 * overall capabilities */
	if (!data.pairwise_cipher || !data.key_mgmt)
		return false;

	if (wpa_s->drv_capa_known) {
		if (!wpa_bss_supported_cipher(wpa_s, data.pairwise_cipher) ||
		    !wpa_bss_supported_key_mgmt(wpa_s, data.key_mgmt))
			return false;
	}

	if (ssid) {
		/* Check that there is a supported AKM and pairwise cipher
		 * based on the specific network profile. */
		if ((ssid->pairwise_cipher & data.pairwise_cipher) == 0)
			return false;
		if ((ssid->key_mgmt & data.key_mgmt) == 0)
			return false;
	}

	return true;
}

#endif /* CONFIG_NO_WPA */


const u8 * wpa_bss_get_rsne(struct wpa_supplicant *wpa_s,
			    const struct wpa_bss *bss, struct wpa_ssid *ssid,
			    bool mlo)
{
#ifndef CONFIG_NO_WPA
	const u8 *ie;

	if (wpas_rsn_overriding(wpa_s, ssid)) {
		if (!ssid)
			ssid = wpa_s->current_ssid;

		/* MLO cases for RSN overriding are required to use RSNE
		 * Override 2 element and RSNXE Override element together. */
		ie = wpa_bss_get_vendor_ie(bss, RSNE_OVERRIDE_2_IE_VENDOR_TYPE);
		if (mlo && ie &&
		    !wpa_bss_get_vendor_ie(bss,
					   RSNXE_OVERRIDE_IE_VENDOR_TYPE)) {
			wpa_printf(MSG_DEBUG, "BSS " MACSTR
				   " advertises RSNE Override 2 element without RSNXE Override element - ignore RSNE Override 2 element for MLO",
				   MAC2STR(bss->bssid));
		} else if (ie && wpa_bss_supported_rsne(wpa_s, ssid, ie)) {
			return ie;
		}

		if (!mlo) {
			ie = wpa_bss_get_vendor_ie(
				bss, RSNE_OVERRIDE_IE_VENDOR_TYPE);
			if (ie && wpa_bss_supported_rsne(wpa_s, ssid, ie))
				return ie;
		}
	}
#endif /* CONFIG_NO_WPA */

	return wpa_bss_get_ie(bss, WLAN_EID_RSN);
}


const u8 * wpa_bss_get_rsnxe(struct wpa_supplicant *wpa_s,
			     const struct wpa_bss *bss, struct wpa_ssid *ssid,
			     bool mlo)
{
	const u8 *ie;

	if (wpas_rsn_overriding(wpa_s, ssid)) {
		ie = wpa_bss_get_vendor_ie(bss, RSNXE_OVERRIDE_IE_VENDOR_TYPE);
		if (ie) {
			const u8 *tmp;

			tmp = wpa_bss_get_rsne(wpa_s, bss, ssid, mlo);
			if (!tmp || tmp[0] == WLAN_EID_RSN) {
				/* An acceptable RSNE override element was not
				 * found, so need to ignore RSNXE overriding. */
				goto out;
			}

			return ie;
		}

		/* MLO cases for RSN overriding are required to use RSNE
		 * Override 2 element and RSNXE Override element together. */
		if (mlo && wpa_bss_get_vendor_ie(
			    bss, RSNE_OVERRIDE_2_IE_VENDOR_TYPE)) {
			wpa_printf(MSG_DEBUG, "BSS " MACSTR
				   " advertises RSNXE Override element without RSNE Override 2 element - ignore RSNXE Override element for MLO",
				   MAC2STR(bss->bssid));
			goto out;
		}
	}

out:
	return wpa_bss_get_ie(bss, WLAN_EID_RSNX);
}
