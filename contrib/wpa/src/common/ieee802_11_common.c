/*
 * IEEE 802.11 Common routines
 * Copyright (c) 2002-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "defs.h"
#include "wpa_common.h"
#include "drivers/driver.h"
#include "qca-vendor.h"
#include "ieee802_11_defs.h"
#include "ieee802_11_common.h"


static int ieee802_11_parse_vendor_specific(const u8 *pos, size_t elen,
					    struct ieee802_11_elems *elems,
					    int show_errors)
{
	unsigned int oui;

	/* first 3 bytes in vendor specific information element are the IEEE
	 * OUI of the vendor. The following byte is used a vendor specific
	 * sub-type. */
	if (elen < 4) {
		if (show_errors) {
			wpa_printf(MSG_MSGDUMP, "short vendor specific "
				   "information element ignored (len=%lu)",
				   (unsigned long) elen);
		}
		return -1;
	}

	oui = WPA_GET_BE24(pos);
	switch (oui) {
	case OUI_MICROSOFT:
		/* Microsoft/Wi-Fi information elements are further typed and
		 * subtyped */
		switch (pos[3]) {
		case 1:
			/* Microsoft OUI (00:50:F2) with OUI Type 1:
			 * real WPA information element */
			elems->wpa_ie = pos;
			elems->wpa_ie_len = elen;
			break;
		case WMM_OUI_TYPE:
			/* WMM information element */
			if (elen < 5) {
				wpa_printf(MSG_MSGDUMP, "short WMM "
					   "information element ignored "
					   "(len=%lu)",
					   (unsigned long) elen);
				return -1;
			}
			switch (pos[4]) {
			case WMM_OUI_SUBTYPE_INFORMATION_ELEMENT:
			case WMM_OUI_SUBTYPE_PARAMETER_ELEMENT:
				/*
				 * Share same pointer since only one of these
				 * is used and they start with same data.
				 * Length field can be used to distinguish the
				 * IEs.
				 */
				elems->wmm = pos;
				elems->wmm_len = elen;
				break;
			case WMM_OUI_SUBTYPE_TSPEC_ELEMENT:
				elems->wmm_tspec = pos;
				elems->wmm_tspec_len = elen;
				break;
			default:
				wpa_printf(MSG_EXCESSIVE, "unknown WMM "
					   "information element ignored "
					   "(subtype=%d len=%lu)",
					   pos[4], (unsigned long) elen);
				return -1;
			}
			break;
		case 4:
			/* Wi-Fi Protected Setup (WPS) IE */
			elems->wps_ie = pos;
			elems->wps_ie_len = elen;
			break;
		default:
			wpa_printf(MSG_EXCESSIVE, "Unknown Microsoft "
				   "information element ignored "
				   "(type=%d len=%lu)",
				   pos[3], (unsigned long) elen);
			return -1;
		}
		break;

	case OUI_WFA:
		switch (pos[3]) {
		case P2P_OUI_TYPE:
			/* Wi-Fi Alliance - P2P IE */
			elems->p2p = pos;
			elems->p2p_len = elen;
			break;
		case WFD_OUI_TYPE:
			/* Wi-Fi Alliance - WFD IE */
			elems->wfd = pos;
			elems->wfd_len = elen;
			break;
		case HS20_INDICATION_OUI_TYPE:
			/* Hotspot 2.0 */
			elems->hs20 = pos;
			elems->hs20_len = elen;
			break;
		case HS20_OSEN_OUI_TYPE:
			/* Hotspot 2.0 OSEN */
			elems->osen = pos;
			elems->osen_len = elen;
			break;
		case MBO_OUI_TYPE:
			/* MBO-OCE */
			elems->mbo = pos;
			elems->mbo_len = elen;
			break;
		case HS20_ROAMING_CONS_SEL_OUI_TYPE:
			/* Hotspot 2.0 Roaming Consortium Selection */
			elems->roaming_cons_sel = pos;
			elems->roaming_cons_sel_len = elen;
			break;
		case MULTI_AP_OUI_TYPE:
			elems->multi_ap = pos;
			elems->multi_ap_len = elen;
			break;
		case OWE_OUI_TYPE:
			/* OWE Transition Mode element */
			break;
		case DPP_CC_OUI_TYPE:
			/* DPP Configurator Connectivity element */
			break;
		case SAE_PK_OUI_TYPE:
			elems->sae_pk = pos + 4;
			elems->sae_pk_len = elen - 4;
			break;
		default:
			wpa_printf(MSG_MSGDUMP, "Unknown WFA "
				   "information element ignored "
				   "(type=%d len=%lu)",
				   pos[3], (unsigned long) elen);
			return -1;
		}
		break;

	case OUI_BROADCOM:
		switch (pos[3]) {
		case VENDOR_HT_CAPAB_OUI_TYPE:
			elems->vendor_ht_cap = pos;
			elems->vendor_ht_cap_len = elen;
			break;
		case VENDOR_VHT_TYPE:
			if (elen > 4 &&
			    (pos[4] == VENDOR_VHT_SUBTYPE ||
			     pos[4] == VENDOR_VHT_SUBTYPE2)) {
				elems->vendor_vht = pos;
				elems->vendor_vht_len = elen;
			} else
				return -1;
			break;
		default:
			wpa_printf(MSG_EXCESSIVE, "Unknown Broadcom "
				   "information element ignored "
				   "(type=%d len=%lu)",
				   pos[3], (unsigned long) elen);
			return -1;
		}
		break;

	case OUI_QCA:
		switch (pos[3]) {
		case QCA_VENDOR_ELEM_P2P_PREF_CHAN_LIST:
			elems->pref_freq_list = pos;
			elems->pref_freq_list_len = elen;
			break;
		default:
			wpa_printf(MSG_EXCESSIVE,
				   "Unknown QCA information element ignored (type=%d len=%lu)",
				   pos[3], (unsigned long) elen);
			return -1;
		}
		break;

	default:
		wpa_printf(MSG_EXCESSIVE, "unknown vendor specific "
			   "information element ignored (vendor OUI "
			   "%02x:%02x:%02x len=%lu)",
			   pos[0], pos[1], pos[2], (unsigned long) elen);
		return -1;
	}

	return 0;
}


static int ieee802_11_parse_extension(const u8 *pos, size_t elen,
				      struct ieee802_11_elems *elems,
				      int show_errors)
{
	u8 ext_id;

	if (elen < 1) {
		if (show_errors) {
			wpa_printf(MSG_MSGDUMP,
				   "short information element (Ext)");
		}
		return -1;
	}

	ext_id = *pos++;
	elen--;

	elems->frag_ies.last_eid_ext = 0;

	switch (ext_id) {
	case WLAN_EID_EXT_ASSOC_DELAY_INFO:
		if (elen != 1)
			break;
		elems->assoc_delay_info = pos;
		break;
	case WLAN_EID_EXT_FILS_REQ_PARAMS:
		if (elen < 3)
			break;
		elems->fils_req_params = pos;
		elems->fils_req_params_len = elen;
		break;
	case WLAN_EID_EXT_FILS_KEY_CONFIRM:
		elems->fils_key_confirm = pos;
		elems->fils_key_confirm_len = elen;
		break;
	case WLAN_EID_EXT_FILS_SESSION:
		if (elen != FILS_SESSION_LEN)
			break;
		elems->fils_session = pos;
		break;
	case WLAN_EID_EXT_FILS_HLP_CONTAINER:
		if (elen < 2 * ETH_ALEN)
			break;
		elems->fils_hlp = pos;
		elems->fils_hlp_len = elen;
		break;
	case WLAN_EID_EXT_FILS_IP_ADDR_ASSIGN:
		if (elen < 1)
			break;
		elems->fils_ip_addr_assign = pos;
		elems->fils_ip_addr_assign_len = elen;
		break;
	case WLAN_EID_EXT_KEY_DELIVERY:
		if (elen < WPA_KEY_RSC_LEN)
			break;
		elems->key_delivery = pos;
		elems->key_delivery_len = elen;
		break;
	case WLAN_EID_EXT_WRAPPED_DATA:
		elems->wrapped_data = pos;
		elems->wrapped_data_len = elen;
		break;
	case WLAN_EID_EXT_FILS_PUBLIC_KEY:
		if (elen < 1)
			break;
		elems->fils_pk = pos;
		elems->fils_pk_len = elen;
		break;
	case WLAN_EID_EXT_FILS_NONCE:
		if (elen != FILS_NONCE_LEN)
			break;
		elems->fils_nonce = pos;
		break;
	case WLAN_EID_EXT_OWE_DH_PARAM:
		if (elen < 2)
			break;
		elems->owe_dh = pos;
		elems->owe_dh_len = elen;
		break;
	case WLAN_EID_EXT_PASSWORD_IDENTIFIER:
		elems->password_id = pos;
		elems->password_id_len = elen;
		break;
	case WLAN_EID_EXT_HE_CAPABILITIES:
		elems->he_capabilities = pos;
		elems->he_capabilities_len = elen;
		break;
	case WLAN_EID_EXT_HE_OPERATION:
		elems->he_operation = pos;
		elems->he_operation_len = elen;
		break;
	case WLAN_EID_EXT_OCV_OCI:
		elems->oci = pos;
		elems->oci_len = elen;
		break;
	case WLAN_EID_EXT_SHORT_SSID_LIST:
		elems->short_ssid_list = pos;
		elems->short_ssid_list_len = elen;
		break;
	case WLAN_EID_EXT_HE_6GHZ_BAND_CAP:
		if (elen < sizeof(struct ieee80211_he_6ghz_band_cap))
			break;
		elems->he_6ghz_band_cap = pos;
		break;
	case WLAN_EID_EXT_PASN_PARAMS:
		elems->pasn_params = pos;
		elems->pasn_params_len = elen;
		break;
	default:
		if (show_errors) {
			wpa_printf(MSG_MSGDUMP,
				   "IEEE 802.11 element parsing ignored unknown element extension (ext_id=%u elen=%u)",
				   ext_id, (unsigned int) elen);
		}
		return -1;
	}

	if (elen == 254)
		elems->frag_ies.last_eid_ext = ext_id;

	return 0;
}


static void ieee802_11_parse_fragment(struct frag_ies_info *frag_ies,
				      const u8 *pos, u8 elen)
{
	if (frag_ies->n_frags >= MAX_NUM_FRAG_IES_SUPPORTED) {
		wpa_printf(MSG_MSGDUMP, "Too many element fragments - skip");
		return;
	}

	/*
	 * Note: while EID == 0 is a valid ID (SSID IE), it should not be
	 * fragmented.
	 */
	if (!frag_ies->last_eid) {
		wpa_printf(MSG_MSGDUMP,
			   "Fragment without a valid last element - skip");
		return;
	}

	frag_ies->frags[frag_ies->n_frags].ie = pos;
	frag_ies->frags[frag_ies->n_frags].ie_len = elen;
	frag_ies->frags[frag_ies->n_frags].eid = frag_ies->last_eid;
	frag_ies->frags[frag_ies->n_frags].eid_ext = frag_ies->last_eid_ext;
	frag_ies->n_frags++;
}


/**
 * ieee802_11_parse_elems - Parse information elements in management frames
 * @start: Pointer to the start of IEs
 * @len: Length of IE buffer in octets
 * @elems: Data structure for parsed elements
 * @show_errors: Whether to show parsing errors in debug log
 * Returns: Parsing result
 */
ParseRes ieee802_11_parse_elems(const u8 *start, size_t len,
				struct ieee802_11_elems *elems,
				int show_errors)
{
	const struct element *elem;
	int unknown = 0;

	os_memset(elems, 0, sizeof(*elems));

	if (!start)
		return ParseOK;

	for_each_element(elem, start, len) {
		u8 id = elem->id, elen = elem->datalen;
		const u8 *pos = elem->data;

		switch (id) {
		case WLAN_EID_SSID:
			if (elen > SSID_MAX_LEN) {
				wpa_printf(MSG_DEBUG,
					   "Ignored too long SSID element (elen=%u)",
					   elen);
				break;
			}
			if (elems->ssid) {
				wpa_printf(MSG_MSGDUMP,
					   "Ignored duplicated SSID element");
				break;
			}
			elems->ssid = pos;
			elems->ssid_len = elen;
			break;
		case WLAN_EID_SUPP_RATES:
			elems->supp_rates = pos;
			elems->supp_rates_len = elen;
			break;
		case WLAN_EID_DS_PARAMS:
			if (elen < 1)
				break;
			elems->ds_params = pos;
			break;
		case WLAN_EID_CF_PARAMS:
		case WLAN_EID_TIM:
			break;
		case WLAN_EID_CHALLENGE:
			elems->challenge = pos;
			elems->challenge_len = elen;
			break;
		case WLAN_EID_ERP_INFO:
			if (elen < 1)
				break;
			elems->erp_info = pos;
			break;
		case WLAN_EID_EXT_SUPP_RATES:
			elems->ext_supp_rates = pos;
			elems->ext_supp_rates_len = elen;
			break;
		case WLAN_EID_VENDOR_SPECIFIC:
			if (ieee802_11_parse_vendor_specific(pos, elen,
							     elems,
							     show_errors))
				unknown++;
			break;
		case WLAN_EID_RSN:
			elems->rsn_ie = pos;
			elems->rsn_ie_len = elen;
			break;
		case WLAN_EID_RSNX:
			elems->rsnxe = pos;
			elems->rsnxe_len = elen;
			break;
		case WLAN_EID_PWR_CAPABILITY:
			if (elen < 2)
				break;
			elems->power_capab = pos;
			elems->power_capab_len = elen;
			break;
		case WLAN_EID_SUPPORTED_CHANNELS:
			elems->supp_channels = pos;
			elems->supp_channels_len = elen;
			break;
		case WLAN_EID_MOBILITY_DOMAIN:
			if (elen < sizeof(struct rsn_mdie))
				break;
			elems->mdie = pos;
			elems->mdie_len = elen;
			break;
		case WLAN_EID_FAST_BSS_TRANSITION:
			if (elen < sizeof(struct rsn_ftie))
				break;
			elems->ftie = pos;
			elems->ftie_len = elen;
			break;
		case WLAN_EID_TIMEOUT_INTERVAL:
			if (elen != 5)
				break;
			elems->timeout_int = pos;
			break;
		case WLAN_EID_HT_CAP:
			if (elen < sizeof(struct ieee80211_ht_capabilities))
				break;
			elems->ht_capabilities = pos;
			break;
		case WLAN_EID_HT_OPERATION:
			if (elen < sizeof(struct ieee80211_ht_operation))
				break;
			elems->ht_operation = pos;
			break;
		case WLAN_EID_MESH_CONFIG:
			elems->mesh_config = pos;
			elems->mesh_config_len = elen;
			break;
		case WLAN_EID_MESH_ID:
			elems->mesh_id = pos;
			elems->mesh_id_len = elen;
			break;
		case WLAN_EID_PEER_MGMT:
			elems->peer_mgmt = pos;
			elems->peer_mgmt_len = elen;
			break;
		case WLAN_EID_VHT_CAP:
			if (elen < sizeof(struct ieee80211_vht_capabilities))
				break;
			elems->vht_capabilities = pos;
			break;
		case WLAN_EID_VHT_OPERATION:
			if (elen < sizeof(struct ieee80211_vht_operation))
				break;
			elems->vht_operation = pos;
			break;
		case WLAN_EID_VHT_OPERATING_MODE_NOTIFICATION:
			if (elen != 1)
				break;
			elems->vht_opmode_notif = pos;
			break;
		case WLAN_EID_LINK_ID:
			if (elen < 18)
				break;
			elems->link_id = pos;
			break;
		case WLAN_EID_INTERWORKING:
			elems->interworking = pos;
			elems->interworking_len = elen;
			break;
		case WLAN_EID_QOS_MAP_SET:
			if (elen < 16)
				break;
			elems->qos_map_set = pos;
			elems->qos_map_set_len = elen;
			break;
		case WLAN_EID_EXT_CAPAB:
			elems->ext_capab = pos;
			elems->ext_capab_len = elen;
			break;
		case WLAN_EID_BSS_MAX_IDLE_PERIOD:
			if (elen < 3)
				break;
			elems->bss_max_idle_period = pos;
			break;
		case WLAN_EID_SSID_LIST:
			elems->ssid_list = pos;
			elems->ssid_list_len = elen;
			break;
		case WLAN_EID_AMPE:
			elems->ampe = pos;
			elems->ampe_len = elen;
			break;
		case WLAN_EID_MIC:
			elems->mic = pos;
			elems->mic_len = elen;
			/* after mic everything is encrypted, so stop. */
			goto done;
		case WLAN_EID_MULTI_BAND:
			if (elems->mb_ies.nof_ies >= MAX_NOF_MB_IES_SUPPORTED) {
				wpa_printf(MSG_MSGDUMP,
					   "IEEE 802.11 element parse ignored MB IE (id=%d elen=%d)",
					   id, elen);
				break;
			}

			elems->mb_ies.ies[elems->mb_ies.nof_ies].ie = pos;
			elems->mb_ies.ies[elems->mb_ies.nof_ies].ie_len = elen;
			elems->mb_ies.nof_ies++;
			break;
		case WLAN_EID_SUPPORTED_OPERATING_CLASSES:
			elems->supp_op_classes = pos;
			elems->supp_op_classes_len = elen;
			break;
		case WLAN_EID_RRM_ENABLED_CAPABILITIES:
			elems->rrm_enabled = pos;
			elems->rrm_enabled_len = elen;
			break;
		case WLAN_EID_CAG_NUMBER:
			elems->cag_number = pos;
			elems->cag_number_len = elen;
			break;
		case WLAN_EID_AP_CSN:
			if (elen < 1)
				break;
			elems->ap_csn = pos;
			break;
		case WLAN_EID_FILS_INDICATION:
			if (elen < 2)
				break;
			elems->fils_indic = pos;
			elems->fils_indic_len = elen;
			break;
		case WLAN_EID_DILS:
			if (elen < 2)
				break;
			elems->dils = pos;
			elems->dils_len = elen;
			break;
		case WLAN_EID_S1G_CAPABILITIES:
			if (elen < 15)
				break;
			elems->s1g_capab = pos;
			break;
		case WLAN_EID_FRAGMENT:
			ieee802_11_parse_fragment(&elems->frag_ies, pos, elen);
			break;
		case WLAN_EID_EXTENSION:
			if (ieee802_11_parse_extension(pos, elen, elems,
						       show_errors))
				unknown++;
			break;
		default:
			unknown++;
			if (!show_errors)
				break;
			wpa_printf(MSG_MSGDUMP, "IEEE 802.11 element parse "
				   "ignored unknown element (id=%d elen=%d)",
				   id, elen);
			break;
		}

		if (id != WLAN_EID_FRAGMENT && elen == 255)
			elems->frag_ies.last_eid = id;

		if (id == WLAN_EID_EXTENSION && !elems->frag_ies.last_eid_ext)
			elems->frag_ies.last_eid = 0;
	}

	if (!for_each_element_completed(elem, start, len)) {
		if (show_errors) {
			wpa_printf(MSG_DEBUG,
				   "IEEE 802.11 element parse failed @%d",
				   (int) (start + len - (const u8 *) elem));
			wpa_hexdump(MSG_MSGDUMP, "IEs", start, len);
		}
		return ParseFailed;
	}

done:
	return unknown ? ParseUnknown : ParseOK;
}


int ieee802_11_ie_count(const u8 *ies, size_t ies_len)
{
	const struct element *elem;
	int count = 0;

	if (ies == NULL)
		return 0;

	for_each_element(elem, ies, ies_len)
		count++;

	return count;
}


struct wpabuf * ieee802_11_vendor_ie_concat(const u8 *ies, size_t ies_len,
					    u32 oui_type)
{
	struct wpabuf *buf;
	const struct element *elem, *found = NULL;

	for_each_element_id(elem, WLAN_EID_VENDOR_SPECIFIC, ies, ies_len) {
		if (elem->datalen >= 4 &&
		    WPA_GET_BE32(elem->data) == oui_type) {
			found = elem;
			break;
		}
	}

	if (!found)
		return NULL; /* No specified vendor IE found */

	buf = wpabuf_alloc(ies_len);
	if (buf == NULL)
		return NULL;

	/*
	 * There may be multiple vendor IEs in the message, so need to
	 * concatenate their data fields.
	 */
	for_each_element_id(elem, WLAN_EID_VENDOR_SPECIFIC, ies, ies_len) {
		if (elem->datalen >= 4 && WPA_GET_BE32(elem->data) == oui_type)
			wpabuf_put_data(buf, elem->data + 4, elem->datalen - 4);
	}

	return buf;
}


const u8 * get_hdr_bssid(const struct ieee80211_hdr *hdr, size_t len)
{
	u16 fc, type, stype;

	/*
	 * PS-Poll frames are 16 bytes. All other frames are
	 * 24 bytes or longer.
	 */
	if (len < 16)
		return NULL;

	fc = le_to_host16(hdr->frame_control);
	type = WLAN_FC_GET_TYPE(fc);
	stype = WLAN_FC_GET_STYPE(fc);

	switch (type) {
	case WLAN_FC_TYPE_DATA:
		if (len < 24)
			return NULL;
		switch (fc & (WLAN_FC_FROMDS | WLAN_FC_TODS)) {
		case WLAN_FC_FROMDS | WLAN_FC_TODS:
		case WLAN_FC_TODS:
			return hdr->addr1;
		case WLAN_FC_FROMDS:
			return hdr->addr2;
		default:
			return NULL;
		}
	case WLAN_FC_TYPE_CTRL:
		if (stype != WLAN_FC_STYPE_PSPOLL)
			return NULL;
		return hdr->addr1;
	case WLAN_FC_TYPE_MGMT:
		return hdr->addr3;
	default:
		return NULL;
	}
}


int hostapd_config_wmm_ac(struct hostapd_wmm_ac_params wmm_ac_params[],
			  const char *name, const char *val)
{
	int num, v;
	const char *pos;
	struct hostapd_wmm_ac_params *ac;

	/* skip 'wme_ac_' or 'wmm_ac_' prefix */
	pos = name + 7;
	if (os_strncmp(pos, "be_", 3) == 0) {
		num = 0;
		pos += 3;
	} else if (os_strncmp(pos, "bk_", 3) == 0) {
		num = 1;
		pos += 3;
	} else if (os_strncmp(pos, "vi_", 3) == 0) {
		num = 2;
		pos += 3;
	} else if (os_strncmp(pos, "vo_", 3) == 0) {
		num = 3;
		pos += 3;
	} else {
		wpa_printf(MSG_ERROR, "Unknown WMM name '%s'", pos);
		return -1;
	}

	ac = &wmm_ac_params[num];

	if (os_strcmp(pos, "aifs") == 0) {
		v = atoi(val);
		if (v < 1 || v > 255) {
			wpa_printf(MSG_ERROR, "Invalid AIFS value %d", v);
			return -1;
		}
		ac->aifs = v;
	} else if (os_strcmp(pos, "cwmin") == 0) {
		v = atoi(val);
		if (v < 0 || v > 15) {
			wpa_printf(MSG_ERROR, "Invalid cwMin value %d", v);
			return -1;
		}
		ac->cwmin = v;
	} else if (os_strcmp(pos, "cwmax") == 0) {
		v = atoi(val);
		if (v < 0 || v > 15) {
			wpa_printf(MSG_ERROR, "Invalid cwMax value %d", v);
			return -1;
		}
		ac->cwmax = v;
	} else if (os_strcmp(pos, "txop_limit") == 0) {
		v = atoi(val);
		if (v < 0 || v > 0xffff) {
			wpa_printf(MSG_ERROR, "Invalid txop value %d", v);
			return -1;
		}
		ac->txop_limit = v;
	} else if (os_strcmp(pos, "acm") == 0) {
		v = atoi(val);
		if (v < 0 || v > 1) {
			wpa_printf(MSG_ERROR, "Invalid acm value %d", v);
			return -1;
		}
		ac->admission_control_mandatory = v;
	} else {
		wpa_printf(MSG_ERROR, "Unknown wmm_ac_ field '%s'", pos);
		return -1;
	}

	return 0;
}


/* convert floats with one decimal place to value*10 int, i.e.,
 * "1.5" will return 15
 */
static int hostapd_config_read_int10(const char *value)
{
	int i, d;
	char *pos;

	i = atoi(value);
	pos = os_strchr(value, '.');
	d = 0;
	if (pos) {
		pos++;
		if (*pos >= '0' && *pos <= '9')
			d = *pos - '0';
	}

	return i * 10 + d;
}


static int valid_cw(int cw)
{
	return (cw == 1 || cw == 3 || cw == 7 || cw == 15 || cw == 31 ||
		cw == 63 || cw == 127 || cw == 255 || cw == 511 || cw == 1023 ||
		cw == 2047 || cw == 4095 || cw == 8191 || cw == 16383 ||
		cw == 32767);
}


int hostapd_config_tx_queue(struct hostapd_tx_queue_params tx_queue[],
			    const char *name, const char *val)
{
	int num;
	const char *pos;
	struct hostapd_tx_queue_params *queue;

	/* skip 'tx_queue_' prefix */
	pos = name + 9;
	if (os_strncmp(pos, "data", 4) == 0 &&
	    pos[4] >= '0' && pos[4] <= '9' && pos[5] == '_') {
		num = pos[4] - '0';
		pos += 6;
	} else if (os_strncmp(pos, "after_beacon_", 13) == 0 ||
		   os_strncmp(pos, "beacon_", 7) == 0) {
		wpa_printf(MSG_INFO, "DEPRECATED: '%s' not used", name);
		return 0;
	} else {
		wpa_printf(MSG_ERROR, "Unknown tx_queue name '%s'", pos);
		return -1;
	}

	if (num >= NUM_TX_QUEUES) {
		/* for backwards compatibility, do not trigger failure */
		wpa_printf(MSG_INFO, "DEPRECATED: '%s' not used", name);
		return 0;
	}

	queue = &tx_queue[num];

	if (os_strcmp(pos, "aifs") == 0) {
		queue->aifs = atoi(val);
		if (queue->aifs < 0 || queue->aifs > 255) {
			wpa_printf(MSG_ERROR, "Invalid AIFS value %d",
				   queue->aifs);
			return -1;
		}
	} else if (os_strcmp(pos, "cwmin") == 0) {
		queue->cwmin = atoi(val);
		if (!valid_cw(queue->cwmin)) {
			wpa_printf(MSG_ERROR, "Invalid cwMin value %d",
				   queue->cwmin);
			return -1;
		}
	} else if (os_strcmp(pos, "cwmax") == 0) {
		queue->cwmax = atoi(val);
		if (!valid_cw(queue->cwmax)) {
			wpa_printf(MSG_ERROR, "Invalid cwMax value %d",
				   queue->cwmax);
			return -1;
		}
	} else if (os_strcmp(pos, "burst") == 0) {
		queue->burst = hostapd_config_read_int10(val);
	} else {
		wpa_printf(MSG_ERROR, "Unknown queue field '%s'", pos);
		return -1;
	}

	return 0;
}


enum hostapd_hw_mode ieee80211_freq_to_chan(int freq, u8 *channel)
{
	u8 op_class;

	return ieee80211_freq_to_channel_ext(freq, 0, CHANWIDTH_USE_HT,
					     &op_class, channel);
}


/**
 * ieee80211_freq_to_channel_ext - Convert frequency into channel info
 * for HT40, VHT, and HE. DFS channels are not covered.
 * @freq: Frequency (MHz) to convert
 * @sec_channel: 0 = non-HT40, 1 = sec. channel above, -1 = sec. channel below
 * @chanwidth: VHT/EDMG channel width (CHANWIDTH_*)
 * @op_class: Buffer for returning operating class
 * @channel: Buffer for returning channel number
 * Returns: hw_mode on success, NUM_HOSTAPD_MODES on failure
 */
enum hostapd_hw_mode ieee80211_freq_to_channel_ext(unsigned int freq,
						   int sec_channel,
						   int chanwidth,
						   u8 *op_class, u8 *channel)
{
	u8 vht_opclass;

	/* TODO: more operating classes */

	if (sec_channel > 1 || sec_channel < -1)
		return NUM_HOSTAPD_MODES;

	if (freq >= 2412 && freq <= 2472) {
		if ((freq - 2407) % 5)
			return NUM_HOSTAPD_MODES;

		if (chanwidth)
			return NUM_HOSTAPD_MODES;

		/* 2.407 GHz, channels 1..13 */
		if (sec_channel == 1)
			*op_class = 83;
		else if (sec_channel == -1)
			*op_class = 84;
		else
			*op_class = 81;

		*channel = (freq - 2407) / 5;

		return HOSTAPD_MODE_IEEE80211G;
	}

	if (freq == 2484) {
		if (sec_channel || chanwidth)
			return NUM_HOSTAPD_MODES;

		*op_class = 82; /* channel 14 */
		*channel = 14;

		return HOSTAPD_MODE_IEEE80211B;
	}

	if (freq >= 4900 && freq < 5000) {
		if ((freq - 4000) % 5)
			return NUM_HOSTAPD_MODES;
		*channel = (freq - 4000) / 5;
		*op_class = 0; /* TODO */
		return HOSTAPD_MODE_IEEE80211A;
	}

	switch (chanwidth) {
	case CHANWIDTH_80MHZ:
		vht_opclass = 128;
		break;
	case CHANWIDTH_160MHZ:
		vht_opclass = 129;
		break;
	case CHANWIDTH_80P80MHZ:
		vht_opclass = 130;
		break;
	default:
		vht_opclass = 0;
		break;
	}

	/* 5 GHz, channels 36..48 */
	if (freq >= 5180 && freq <= 5240) {
		if ((freq - 5000) % 5)
			return NUM_HOSTAPD_MODES;

		if (vht_opclass)
			*op_class = vht_opclass;
		else if (sec_channel == 1)
			*op_class = 116;
		else if (sec_channel == -1)
			*op_class = 117;
		else
			*op_class = 115;

		*channel = (freq - 5000) / 5;

		return HOSTAPD_MODE_IEEE80211A;
	}

	/* 5 GHz, channels 52..64 */
	if (freq >= 5260 && freq <= 5320) {
		if ((freq - 5000) % 5)
			return NUM_HOSTAPD_MODES;

		if (vht_opclass)
			*op_class = vht_opclass;
		else if (sec_channel == 1)
			*op_class = 119;
		else if (sec_channel == -1)
			*op_class = 120;
		else
			*op_class = 118;

		*channel = (freq - 5000) / 5;

		return HOSTAPD_MODE_IEEE80211A;
	}

	/* 5 GHz, channels 149..177 */
	if (freq >= 5745 && freq <= 5885) {
		if ((freq - 5000) % 5)
			return NUM_HOSTAPD_MODES;

		if (vht_opclass)
			*op_class = vht_opclass;
		else if (sec_channel == 1)
			*op_class = 126;
		else if (sec_channel == -1)
			*op_class = 127;
		else if (freq <= 5805)
			*op_class = 124;
		else
			*op_class = 125;

		*channel = (freq - 5000) / 5;

		return HOSTAPD_MODE_IEEE80211A;
	}

	/* 5 GHz, channels 100..144 */
	if (freq >= 5500 && freq <= 5720) {
		if ((freq - 5000) % 5)
			return NUM_HOSTAPD_MODES;

		if (vht_opclass)
			*op_class = vht_opclass;
		else if (sec_channel == 1)
			*op_class = 122;
		else if (sec_channel == -1)
			*op_class = 123;
		else
			*op_class = 121;

		*channel = (freq - 5000) / 5;

		return HOSTAPD_MODE_IEEE80211A;
	}

	if (freq >= 5000 && freq < 5900) {
		if ((freq - 5000) % 5)
			return NUM_HOSTAPD_MODES;
		*channel = (freq - 5000) / 5;
		*op_class = 0; /* TODO */
		return HOSTAPD_MODE_IEEE80211A;
	}

	if (freq > 5950 && freq <= 7115) {
		if ((freq - 5950) % 5)
			return NUM_HOSTAPD_MODES;

		switch (chanwidth) {
		case CHANWIDTH_80MHZ:
			*op_class = 133;
			break;
		case CHANWIDTH_160MHZ:
			*op_class = 134;
			break;
		case CHANWIDTH_80P80MHZ:
			*op_class = 135;
			break;
		default:
			if (sec_channel)
				*op_class = 132;
			else
				*op_class = 131;
			break;
		}

		*channel = (freq - 5950) / 5;
		return HOSTAPD_MODE_IEEE80211A;
	}

	if (freq == 5935) {
		*op_class = 136;
		*channel = (freq - 5925) / 5;
		return HOSTAPD_MODE_IEEE80211A;
	}

	/* 56.16 GHz, channel 1..6 */
	if (freq >= 56160 + 2160 * 1 && freq <= 56160 + 2160 * 6) {
		if (sec_channel)
			return NUM_HOSTAPD_MODES;

		switch (chanwidth) {
		case CHANWIDTH_USE_HT:
		case CHANWIDTH_2160MHZ:
			*channel = (freq - 56160) / 2160;
			*op_class = 180;
			break;
		case CHANWIDTH_4320MHZ:
			/* EDMG channels 9 - 13 */
			if (freq > 56160 + 2160 * 5)
				return NUM_HOSTAPD_MODES;

			*channel = (freq - 56160) / 2160 + 8;
			*op_class = 181;
			break;
		case CHANWIDTH_6480MHZ:
			/* EDMG channels 17 - 20 */
			if (freq > 56160 + 2160 * 4)
				return NUM_HOSTAPD_MODES;

			*channel = (freq - 56160) / 2160 + 16;
			*op_class = 182;
			break;
		case CHANWIDTH_8640MHZ:
			/* EDMG channels 25 - 27 */
			if (freq > 56160 + 2160 * 3)
				return NUM_HOSTAPD_MODES;

			*channel = (freq - 56160) / 2160 + 24;
			*op_class = 183;
			break;
		default:
			return NUM_HOSTAPD_MODES;
		}

		return HOSTAPD_MODE_IEEE80211AD;
	}

	return NUM_HOSTAPD_MODES;
}


int ieee80211_chaninfo_to_channel(unsigned int freq, enum chan_width chanwidth,
				  int sec_channel, u8 *op_class, u8 *channel)
{
	int cw = CHAN_WIDTH_UNKNOWN;

	switch (chanwidth) {
	case CHAN_WIDTH_UNKNOWN:
	case CHAN_WIDTH_20_NOHT:
	case CHAN_WIDTH_20:
	case CHAN_WIDTH_40:
		cw = CHANWIDTH_USE_HT;
		break;
	case CHAN_WIDTH_80:
		cw = CHANWIDTH_80MHZ;
		break;
	case CHAN_WIDTH_80P80:
		cw = CHANWIDTH_80P80MHZ;
		break;
	case CHAN_WIDTH_160:
		cw = CHANWIDTH_160MHZ;
		break;
	case CHAN_WIDTH_2160:
		cw = CHANWIDTH_2160MHZ;
		break;
	case CHAN_WIDTH_4320:
		cw = CHANWIDTH_4320MHZ;
		break;
	case CHAN_WIDTH_6480:
		cw = CHANWIDTH_6480MHZ;
		break;
	case CHAN_WIDTH_8640:
		cw = CHANWIDTH_8640MHZ;
		break;
	}

	if (ieee80211_freq_to_channel_ext(freq, sec_channel, cw, op_class,
					  channel) == NUM_HOSTAPD_MODES) {
		wpa_printf(MSG_WARNING,
			   "Cannot determine operating class and channel (freq=%u chanwidth=%d sec_channel=%d)",
			   freq, chanwidth, sec_channel);
		return -1;
	}

	return 0;
}


static const char *const us_op_class_cc[] = {
	"US", "CA", NULL
};

static const char *const eu_op_class_cc[] = {
	"AL", "AM", "AT", "AZ", "BA", "BE", "BG", "BY", "CH", "CY", "CZ", "DE",
	"DK", "EE", "EL", "ES", "FI", "FR", "GE", "HR", "HU", "IE", "IS", "IT",
	"LI", "LT", "LU", "LV", "MD", "ME", "MK", "MT", "NL", "NO", "PL", "PT",
	"RO", "RS", "RU", "SE", "SI", "SK", "TR", "UA", "UK", NULL
};

static const char *const jp_op_class_cc[] = {
	"JP", NULL
};

static const char *const cn_op_class_cc[] = {
	"CN", NULL
};


static int country_match(const char *const cc[], const char *const country)
{
	int i;

	if (country == NULL)
		return 0;
	for (i = 0; cc[i]; i++) {
		if (cc[i][0] == country[0] && cc[i][1] == country[1])
			return 1;
	}

	return 0;
}


static int ieee80211_chan_to_freq_us(u8 op_class, u8 chan)
{
	switch (op_class) {
	case 12: /* channels 1..11 */
	case 32: /* channels 1..7; 40 MHz */
	case 33: /* channels 5..11; 40 MHz */
		if (chan < 1 || chan > 11)
			return -1;
		return 2407 + 5 * chan;
	case 1: /* channels 36,40,44,48 */
	case 2: /* channels 52,56,60,64; dfs */
	case 22: /* channels 36,44; 40 MHz */
	case 23: /* channels 52,60; 40 MHz */
	case 27: /* channels 40,48; 40 MHz */
	case 28: /* channels 56,64; 40 MHz */
		if (chan < 36 || chan > 64)
			return -1;
		return 5000 + 5 * chan;
	case 4: /* channels 100-144 */
	case 24: /* channels 100-140; 40 MHz */
		if (chan < 100 || chan > 144)
			return -1;
		return 5000 + 5 * chan;
	case 3: /* channels 149,153,157,161 */
	case 25: /* channels 149,157; 40 MHz */
	case 26: /* channels 149,157; 40 MHz */
	case 30: /* channels 153,161; 40 MHz */
	case 31: /* channels 153,161; 40 MHz */
		if (chan < 149 || chan > 161)
			return -1;
		return 5000 + 5 * chan;
	case 5: /* channels 149,153,157,161,165 */
		if (chan < 149 || chan > 165)
			return -1;
		return 5000 + 5 * chan;
	case 34: /* 60 GHz band, channels 1..8 */
		if (chan < 1 || chan > 8)
			return -1;
		return 56160 + 2160 * chan;
	case 37: /* 60 GHz band, EDMG CB2, channels 9..15 */
		if (chan < 9 || chan > 15)
			return -1;
		return 56160 + 2160 * (chan - 8);
	case 38: /* 60 GHz band, EDMG CB3, channels 17..22 */
		if (chan < 17 || chan > 22)
			return -1;
		return 56160 + 2160 * (chan - 16);
	case 39: /* 60 GHz band, EDMG CB4, channels 25..29 */
		if (chan < 25 || chan > 29)
			return -1;
		return 56160 + 2160 * (chan - 24);
	}
	return -1;
}


static int ieee80211_chan_to_freq_eu(u8 op_class, u8 chan)
{
	switch (op_class) {
	case 4: /* channels 1..13 */
	case 11: /* channels 1..9; 40 MHz */
	case 12: /* channels 5..13; 40 MHz */
		if (chan < 1 || chan > 13)
			return -1;
		return 2407 + 5 * chan;
	case 1: /* channels 36,40,44,48 */
	case 2: /* channels 52,56,60,64; dfs */
	case 5: /* channels 36,44; 40 MHz */
	case 6: /* channels 52,60; 40 MHz */
	case 8: /* channels 40,48; 40 MHz */
	case 9: /* channels 56,64; 40 MHz */
		if (chan < 36 || chan > 64)
			return -1;
		return 5000 + 5 * chan;
	case 3: /* channels 100-140 */
	case 7: /* channels 100-132; 40 MHz */
	case 10: /* channels 104-136; 40 MHz */
	case 16: /* channels 100-140 */
		if (chan < 100 || chan > 140)
			return -1;
		return 5000 + 5 * chan;
	case 17: /* channels 149,153,157,161,165,169 */
		if (chan < 149 || chan > 169)
			return -1;
		return 5000 + 5 * chan;
	case 18: /* 60 GHz band, channels 1..6 */
		if (chan < 1 || chan > 6)
			return -1;
		return 56160 + 2160 * chan;
	case 21: /* 60 GHz band, EDMG CB2, channels 9..11 */
		if (chan < 9 || chan > 11)
			return -1;
		return 56160 + 2160 * (chan - 8);
	case 22: /* 60 GHz band, EDMG CB3, channels 17..18 */
		if (chan < 17 || chan > 18)
			return -1;
		return 56160 + 2160 * (chan - 16);
	case 23: /* 60 GHz band, EDMG CB4, channels 25 */
		if (chan != 25)
			return -1;
		return 56160 + 2160 * (chan - 24);
	}
	return -1;
}


static int ieee80211_chan_to_freq_jp(u8 op_class, u8 chan)
{
	switch (op_class) {
	case 30: /* channels 1..13 */
	case 56: /* channels 1..9; 40 MHz */
	case 57: /* channels 5..13; 40 MHz */
		if (chan < 1 || chan > 13)
			return -1;
		return 2407 + 5 * chan;
	case 31: /* channel 14 */
		if (chan != 14)
			return -1;
		return 2414 + 5 * chan;
	case 1: /* channels 34,38,42,46(old) or 36,40,44,48 */
	case 32: /* channels 52,56,60,64 */
	case 33: /* channels 52,56,60,64 */
	case 36: /* channels 36,44; 40 MHz */
	case 37: /* channels 52,60; 40 MHz */
	case 38: /* channels 52,60; 40 MHz */
	case 41: /* channels 40,48; 40 MHz */
	case 42: /* channels 56,64; 40 MHz */
	case 43: /* channels 56,64; 40 MHz */
		if (chan < 34 || chan > 64)
			return -1;
		return 5000 + 5 * chan;
	case 34: /* channels 100-140 */
	case 35: /* channels 100-140 */
	case 39: /* channels 100-132; 40 MHz */
	case 40: /* channels 100-132; 40 MHz */
	case 44: /* channels 104-136; 40 MHz */
	case 45: /* channels 104-136; 40 MHz */
	case 58: /* channels 100-140 */
		if (chan < 100 || chan > 140)
			return -1;
		return 5000 + 5 * chan;
	case 59: /* 60 GHz band, channels 1..6 */
		if (chan < 1 || chan > 6)
			return -1;
		return 56160 + 2160 * chan;
	case 62: /* 60 GHz band, EDMG CB2, channels 9..11 */
		if (chan < 9 || chan > 11)
			return -1;
		return 56160 + 2160 * (chan - 8);
	case 63: /* 60 GHz band, EDMG CB3, channels 17..18 */
		if (chan < 17 || chan > 18)
			return -1;
		return 56160 + 2160 * (chan - 16);
	case 64: /* 60 GHz band, EDMG CB4, channel 25 */
		if (chan != 25)
			return -1;
		return 56160 + 2160 * (chan - 24);
	}
	return -1;
}


static int ieee80211_chan_to_freq_cn(u8 op_class, u8 chan)
{
	switch (op_class) {
	case 7: /* channels 1..13 */
	case 8: /* channels 1..9; 40 MHz */
	case 9: /* channels 5..13; 40 MHz */
		if (chan < 1 || chan > 13)
			return -1;
		return 2407 + 5 * chan;
	case 1: /* channels 36,40,44,48 */
	case 2: /* channels 52,56,60,64; dfs */
	case 4: /* channels 36,44; 40 MHz */
	case 5: /* channels 52,60; 40 MHz */
		if (chan < 36 || chan > 64)
			return -1;
		return 5000 + 5 * chan;
	case 3: /* channels 149,153,157,161,165 */
	case 6: /* channels 149,157; 40 MHz */
		if (chan < 149 || chan > 165)
			return -1;
		return 5000 + 5 * chan;
	}
	return -1;
}


static int ieee80211_chan_to_freq_global(u8 op_class, u8 chan)
{
	/* Table E-4 in IEEE Std 802.11-2012 - Global operating classes */
	switch (op_class) {
	case 81:
		/* channels 1..13 */
		if (chan < 1 || chan > 13)
			return -1;
		return 2407 + 5 * chan;
	case 82:
		/* channel 14 */
		if (chan != 14)
			return -1;
		return 2414 + 5 * chan;
	case 83: /* channels 1..9; 40 MHz */
	case 84: /* channels 5..13; 40 MHz */
		if (chan < 1 || chan > 13)
			return -1;
		return 2407 + 5 * chan;
	case 115: /* channels 36,40,44,48; indoor only */
	case 116: /* channels 36,44; 40 MHz; indoor only */
	case 117: /* channels 40,48; 40 MHz; indoor only */
	case 118: /* channels 52,56,60,64; dfs */
	case 119: /* channels 52,60; 40 MHz; dfs */
	case 120: /* channels 56,64; 40 MHz; dfs */
		if (chan < 36 || chan > 64)
			return -1;
		return 5000 + 5 * chan;
	case 121: /* channels 100-140 */
	case 122: /* channels 100-142; 40 MHz */
	case 123: /* channels 104-136; 40 MHz */
		if (chan < 100 || chan > 140)
			return -1;
		return 5000 + 5 * chan;
	case 124: /* channels 149,153,157,161 */
		if (chan < 149 || chan > 161)
			return -1;
		return 5000 + 5 * chan;
	case 125: /* channels 149,153,157,161,165,169,173,177 */
	case 126: /* channels 149,157,165,173; 40 MHz */
	case 127: /* channels 153,161,169,177; 40 MHz */
		if (chan < 149 || chan > 177)
			return -1;
		return 5000 + 5 * chan;
	case 128: /* center freqs 42, 58, 106, 122, 138, 155, 171; 80 MHz */
	case 130: /* center freqs 42, 58, 106, 122, 138, 155, 171; 80 MHz */
		if (chan < 36 || chan > 177)
			return -1;
		return 5000 + 5 * chan;
	case 129: /* center freqs 50, 114, 163; 160 MHz */
		if (chan < 36 || chan > 177)
			return -1;
		return 5000 + 5 * chan;
	case 131: /* UHB channels, 20 MHz: 1, 5, 9.. */
	case 132: /* UHB channels, 40 MHz: 3, 11, 19.. */
	case 133: /* UHB channels, 80 MHz: 7, 23, 39.. */
	case 134: /* UHB channels, 160 MHz: 15, 47, 79.. */
	case 135: /* UHB channels, 80+80 MHz: 7, 23, 39.. */
		if (chan < 1 || chan > 233)
			return -1;
		return 5950 + chan * 5;
	case 136: /* UHB channels, 20 MHz: 2 */
		if (chan == 2)
			return 5935;
		return -1;
	case 180: /* 60 GHz band, channels 1..8 */
		if (chan < 1 || chan > 8)
			return -1;
		return 56160 + 2160 * chan;
	case 181: /* 60 GHz band, EDMG CB2, channels 9..15 */
		if (chan < 9 || chan > 15)
			return -1;
		return 56160 + 2160 * (chan - 8);
	case 182: /* 60 GHz band, EDMG CB3, channels 17..22 */
		if (chan < 17 || chan > 22)
			return -1;
		return 56160 + 2160 * (chan - 16);
	case 183: /* 60 GHz band, EDMG CB4, channel 25..29 */
		if (chan < 25 || chan > 29)
			return -1;
		return 56160 + 2160 * (chan - 24);
	}
	return -1;
}

/**
 * ieee80211_chan_to_freq - Convert channel info to frequency
 * @country: Country code, if known; otherwise, global operating class is used
 * @op_class: Operating class
 * @chan: Channel number
 * Returns: Frequency in MHz or -1 if the specified channel is unknown
 */
int ieee80211_chan_to_freq(const char *country, u8 op_class, u8 chan)
{
	int freq;

	if (country_match(us_op_class_cc, country)) {
		freq = ieee80211_chan_to_freq_us(op_class, chan);
		if (freq > 0)
			return freq;
	}

	if (country_match(eu_op_class_cc, country)) {
		freq = ieee80211_chan_to_freq_eu(op_class, chan);
		if (freq > 0)
			return freq;
	}

	if (country_match(jp_op_class_cc, country)) {
		freq = ieee80211_chan_to_freq_jp(op_class, chan);
		if (freq > 0)
			return freq;
	}

	if (country_match(cn_op_class_cc, country)) {
		freq = ieee80211_chan_to_freq_cn(op_class, chan);
		if (freq > 0)
			return freq;
	}

	return ieee80211_chan_to_freq_global(op_class, chan);
}


int ieee80211_is_dfs(int freq, const struct hostapd_hw_modes *modes,
		     u16 num_modes)
{
	int i, j;

	if (!modes || !num_modes)
		return (freq >= 5260 && freq <= 5320) ||
			(freq >= 5500 && freq <= 5700);

	for (i = 0; i < num_modes; i++) {
		for (j = 0; j < modes[i].num_channels; j++) {
			if (modes[i].channels[j].freq == freq &&
			    (modes[i].channels[j].flag & HOSTAPD_CHAN_RADAR))
				return 1;
		}
	}

	return 0;
}


/*
 * 802.11-2020: Table E-4 - Global operating classes
 * DFS_50_100_Behavior: 118, 119, 120, 121, 122, 123
 */
int is_dfs_global_op_class(u8 op_class)
{
    return (op_class >= 118) && (op_class <= 123);
}


static int is_11b(u8 rate)
{
	return rate == 0x02 || rate == 0x04 || rate == 0x0b || rate == 0x16;
}


int supp_rates_11b_only(struct ieee802_11_elems *elems)
{
	int num_11b = 0, num_others = 0;
	int i;

	if (elems->supp_rates == NULL && elems->ext_supp_rates == NULL)
		return 0;

	for (i = 0; elems->supp_rates && i < elems->supp_rates_len; i++) {
		if (is_11b(elems->supp_rates[i]))
			num_11b++;
		else
			num_others++;
	}

	for (i = 0; elems->ext_supp_rates && i < elems->ext_supp_rates_len;
	     i++) {
		if (is_11b(elems->ext_supp_rates[i]))
			num_11b++;
		else
			num_others++;
	}

	return num_11b > 0 && num_others == 0;
}


const char * fc2str(u16 fc)
{
	u16 stype = WLAN_FC_GET_STYPE(fc);
#define C2S(x) case x: return #x;

	switch (WLAN_FC_GET_TYPE(fc)) {
	case WLAN_FC_TYPE_MGMT:
		switch (stype) {
		C2S(WLAN_FC_STYPE_ASSOC_REQ)
		C2S(WLAN_FC_STYPE_ASSOC_RESP)
		C2S(WLAN_FC_STYPE_REASSOC_REQ)
		C2S(WLAN_FC_STYPE_REASSOC_RESP)
		C2S(WLAN_FC_STYPE_PROBE_REQ)
		C2S(WLAN_FC_STYPE_PROBE_RESP)
		C2S(WLAN_FC_STYPE_BEACON)
		C2S(WLAN_FC_STYPE_ATIM)
		C2S(WLAN_FC_STYPE_DISASSOC)
		C2S(WLAN_FC_STYPE_AUTH)
		C2S(WLAN_FC_STYPE_DEAUTH)
		C2S(WLAN_FC_STYPE_ACTION)
		}
		break;
	case WLAN_FC_TYPE_CTRL:
		switch (stype) {
		C2S(WLAN_FC_STYPE_PSPOLL)
		C2S(WLAN_FC_STYPE_RTS)
		C2S(WLAN_FC_STYPE_CTS)
		C2S(WLAN_FC_STYPE_ACK)
		C2S(WLAN_FC_STYPE_CFEND)
		C2S(WLAN_FC_STYPE_CFENDACK)
		}
		break;
	case WLAN_FC_TYPE_DATA:
		switch (stype) {
		C2S(WLAN_FC_STYPE_DATA)
		C2S(WLAN_FC_STYPE_DATA_CFACK)
		C2S(WLAN_FC_STYPE_DATA_CFPOLL)
		C2S(WLAN_FC_STYPE_DATA_CFACKPOLL)
		C2S(WLAN_FC_STYPE_NULLFUNC)
		C2S(WLAN_FC_STYPE_CFACK)
		C2S(WLAN_FC_STYPE_CFPOLL)
		C2S(WLAN_FC_STYPE_CFACKPOLL)
		C2S(WLAN_FC_STYPE_QOS_DATA)
		C2S(WLAN_FC_STYPE_QOS_DATA_CFACK)
		C2S(WLAN_FC_STYPE_QOS_DATA_CFPOLL)
		C2S(WLAN_FC_STYPE_QOS_DATA_CFACKPOLL)
		C2S(WLAN_FC_STYPE_QOS_NULL)
		C2S(WLAN_FC_STYPE_QOS_CFPOLL)
		C2S(WLAN_FC_STYPE_QOS_CFACKPOLL)
		}
		break;
	}
	return "WLAN_FC_TYPE_UNKNOWN";
#undef C2S
}


const char * reason2str(u16 reason)
{
#define R2S(r) case WLAN_REASON_ ## r: return #r;
	switch (reason) {
	R2S(UNSPECIFIED)
	R2S(PREV_AUTH_NOT_VALID)
	R2S(DEAUTH_LEAVING)
	R2S(DISASSOC_DUE_TO_INACTIVITY)
	R2S(DISASSOC_AP_BUSY)
	R2S(CLASS2_FRAME_FROM_NONAUTH_STA)
	R2S(CLASS3_FRAME_FROM_NONASSOC_STA)
	R2S(DISASSOC_STA_HAS_LEFT)
	R2S(STA_REQ_ASSOC_WITHOUT_AUTH)
	R2S(PWR_CAPABILITY_NOT_VALID)
	R2S(SUPPORTED_CHANNEL_NOT_VALID)
	R2S(BSS_TRANSITION_DISASSOC)
	R2S(INVALID_IE)
	R2S(MICHAEL_MIC_FAILURE)
	R2S(4WAY_HANDSHAKE_TIMEOUT)
	R2S(GROUP_KEY_UPDATE_TIMEOUT)
	R2S(IE_IN_4WAY_DIFFERS)
	R2S(GROUP_CIPHER_NOT_VALID)
	R2S(PAIRWISE_CIPHER_NOT_VALID)
	R2S(AKMP_NOT_VALID)
	R2S(UNSUPPORTED_RSN_IE_VERSION)
	R2S(INVALID_RSN_IE_CAPAB)
	R2S(IEEE_802_1X_AUTH_FAILED)
	R2S(CIPHER_SUITE_REJECTED)
	R2S(TDLS_TEARDOWN_UNREACHABLE)
	R2S(TDLS_TEARDOWN_UNSPECIFIED)
	R2S(SSP_REQUESTED_DISASSOC)
	R2S(NO_SSP_ROAMING_AGREEMENT)
	R2S(BAD_CIPHER_OR_AKM)
	R2S(NOT_AUTHORIZED_THIS_LOCATION)
	R2S(SERVICE_CHANGE_PRECLUDES_TS)
	R2S(UNSPECIFIED_QOS_REASON)
	R2S(NOT_ENOUGH_BANDWIDTH)
	R2S(DISASSOC_LOW_ACK)
	R2S(EXCEEDED_TXOP)
	R2S(STA_LEAVING)
	R2S(END_TS_BA_DLS)
	R2S(UNKNOWN_TS_BA)
	R2S(TIMEOUT)
	R2S(PEERKEY_MISMATCH)
	R2S(AUTHORIZED_ACCESS_LIMIT_REACHED)
	R2S(EXTERNAL_SERVICE_REQUIREMENTS)
	R2S(INVALID_FT_ACTION_FRAME_COUNT)
	R2S(INVALID_PMKID)
	R2S(INVALID_MDE)
	R2S(INVALID_FTE)
	R2S(MESH_PEERING_CANCELLED)
	R2S(MESH_MAX_PEERS)
	R2S(MESH_CONFIG_POLICY_VIOLATION)
	R2S(MESH_CLOSE_RCVD)
	R2S(MESH_MAX_RETRIES)
	R2S(MESH_CONFIRM_TIMEOUT)
	R2S(MESH_INVALID_GTK)
	R2S(MESH_INCONSISTENT_PARAMS)
	R2S(MESH_INVALID_SECURITY_CAP)
	R2S(MESH_PATH_ERROR_NO_PROXY_INFO)
	R2S(MESH_PATH_ERROR_NO_FORWARDING_INFO)
	R2S(MESH_PATH_ERROR_DEST_UNREACHABLE)
	R2S(MAC_ADDRESS_ALREADY_EXISTS_IN_MBSS)
	R2S(MESH_CHANNEL_SWITCH_REGULATORY_REQ)
	R2S(MESH_CHANNEL_SWITCH_UNSPECIFIED)
	}
	return "UNKNOWN";
#undef R2S
}


const char * status2str(u16 status)
{
#define S2S(s) case WLAN_STATUS_ ## s: return #s;
	switch (status) {
	S2S(SUCCESS)
	S2S(UNSPECIFIED_FAILURE)
	S2S(TDLS_WAKEUP_ALTERNATE)
	S2S(TDLS_WAKEUP_REJECT)
	S2S(SECURITY_DISABLED)
	S2S(UNACCEPTABLE_LIFETIME)
	S2S(NOT_IN_SAME_BSS)
	S2S(CAPS_UNSUPPORTED)
	S2S(REASSOC_NO_ASSOC)
	S2S(ASSOC_DENIED_UNSPEC)
	S2S(NOT_SUPPORTED_AUTH_ALG)
	S2S(UNKNOWN_AUTH_TRANSACTION)
	S2S(CHALLENGE_FAIL)
	S2S(AUTH_TIMEOUT)
	S2S(AP_UNABLE_TO_HANDLE_NEW_STA)
	S2S(ASSOC_DENIED_RATES)
	S2S(ASSOC_DENIED_NOSHORT)
	S2S(SPEC_MGMT_REQUIRED)
	S2S(PWR_CAPABILITY_NOT_VALID)
	S2S(SUPPORTED_CHANNEL_NOT_VALID)
	S2S(ASSOC_DENIED_NO_SHORT_SLOT_TIME)
	S2S(ASSOC_DENIED_NO_HT)
	S2S(R0KH_UNREACHABLE)
	S2S(ASSOC_DENIED_NO_PCO)
	S2S(ASSOC_REJECTED_TEMPORARILY)
	S2S(ROBUST_MGMT_FRAME_POLICY_VIOLATION)
	S2S(UNSPECIFIED_QOS_FAILURE)
	S2S(DENIED_INSUFFICIENT_BANDWIDTH)
	S2S(DENIED_POOR_CHANNEL_CONDITIONS)
	S2S(DENIED_QOS_NOT_SUPPORTED)
	S2S(REQUEST_DECLINED)
	S2S(INVALID_PARAMETERS)
	S2S(REJECTED_WITH_SUGGESTED_CHANGES)
	S2S(INVALID_IE)
	S2S(GROUP_CIPHER_NOT_VALID)
	S2S(PAIRWISE_CIPHER_NOT_VALID)
	S2S(AKMP_NOT_VALID)
	S2S(UNSUPPORTED_RSN_IE_VERSION)
	S2S(INVALID_RSN_IE_CAPAB)
	S2S(CIPHER_REJECTED_PER_POLICY)
	S2S(TS_NOT_CREATED)
	S2S(DIRECT_LINK_NOT_ALLOWED)
	S2S(DEST_STA_NOT_PRESENT)
	S2S(DEST_STA_NOT_QOS_STA)
	S2S(ASSOC_DENIED_LISTEN_INT_TOO_LARGE)
	S2S(INVALID_FT_ACTION_FRAME_COUNT)
	S2S(INVALID_PMKID)
	S2S(INVALID_MDIE)
	S2S(INVALID_FTIE)
	S2S(REQUESTED_TCLAS_NOT_SUPPORTED)
	S2S(INSUFFICIENT_TCLAS_PROCESSING_RESOURCES)
	S2S(TRY_ANOTHER_BSS)
	S2S(GAS_ADV_PROTO_NOT_SUPPORTED)
	S2S(NO_OUTSTANDING_GAS_REQ)
	S2S(GAS_RESP_NOT_RECEIVED)
	S2S(STA_TIMED_OUT_WAITING_FOR_GAS_RESP)
	S2S(GAS_RESP_LARGER_THAN_LIMIT)
	S2S(REQ_REFUSED_HOME)
	S2S(ADV_SRV_UNREACHABLE)
	S2S(REQ_REFUSED_SSPN)
	S2S(REQ_REFUSED_UNAUTH_ACCESS)
	S2S(INVALID_RSNIE)
	S2S(U_APSD_COEX_NOT_SUPPORTED)
	S2S(U_APSD_COEX_MODE_NOT_SUPPORTED)
	S2S(BAD_INTERVAL_WITH_U_APSD_COEX)
	S2S(ANTI_CLOGGING_TOKEN_REQ)
	S2S(FINITE_CYCLIC_GROUP_NOT_SUPPORTED)
	S2S(CANNOT_FIND_ALT_TBTT)
	S2S(TRANSMISSION_FAILURE)
	S2S(REQ_TCLAS_NOT_SUPPORTED)
	S2S(TCLAS_RESOURCES_EXCHAUSTED)
	S2S(REJECTED_WITH_SUGGESTED_BSS_TRANSITION)
	S2S(REJECT_WITH_SCHEDULE)
	S2S(REJECT_NO_WAKEUP_SPECIFIED)
	S2S(SUCCESS_POWER_SAVE_MODE)
	S2S(PENDING_ADMITTING_FST_SESSION)
	S2S(PERFORMING_FST_NOW)
	S2S(PENDING_GAP_IN_BA_WINDOW)
	S2S(REJECT_U_PID_SETTING)
	S2S(REFUSED_EXTERNAL_REASON)
	S2S(REFUSED_AP_OUT_OF_MEMORY)
	S2S(REJECTED_EMERGENCY_SERVICE_NOT_SUPPORTED)
	S2S(QUERY_RESP_OUTSTANDING)
	S2S(REJECT_DSE_BAND)
	S2S(TCLAS_PROCESSING_TERMINATED)
	S2S(TS_SCHEDULE_CONFLICT)
	S2S(DENIED_WITH_SUGGESTED_BAND_AND_CHANNEL)
	S2S(MCCAOP_RESERVATION_CONFLICT)
	S2S(MAF_LIMIT_EXCEEDED)
	S2S(MCCA_TRACK_LIMIT_EXCEEDED)
	S2S(DENIED_DUE_TO_SPECTRUM_MANAGEMENT)
	S2S(ASSOC_DENIED_NO_VHT)
	S2S(ENABLEMENT_DENIED)
	S2S(RESTRICTION_FROM_AUTHORIZED_GDB)
	S2S(AUTHORIZATION_DEENABLED)
	S2S(FILS_AUTHENTICATION_FAILURE)
	S2S(UNKNOWN_AUTHENTICATION_SERVER)
	S2S(UNKNOWN_PASSWORD_IDENTIFIER)
	S2S(DENIED_HE_NOT_SUPPORTED)
	S2S(SAE_HASH_TO_ELEMENT)
	S2S(SAE_PK)
	}
	return "UNKNOWN";
#undef S2S
}


int mb_ies_info_by_ies(struct mb_ies_info *info, const u8 *ies_buf,
		       size_t ies_len)
{
	const struct element *elem;

	os_memset(info, 0, sizeof(*info));

	if (!ies_buf)
		return 0;

	for_each_element_id(elem, WLAN_EID_MULTI_BAND, ies_buf, ies_len) {
		if (info->nof_ies >= MAX_NOF_MB_IES_SUPPORTED)
			return 0;

		wpa_printf(MSG_DEBUG, "MB IE of %u bytes found",
			   elem->datalen + 2);
		info->ies[info->nof_ies].ie = elem->data;
		info->ies[info->nof_ies].ie_len = elem->datalen;
		info->nof_ies++;
	}

	if (!for_each_element_completed(elem, ies_buf, ies_len)) {
		wpa_hexdump(MSG_DEBUG, "Truncated IEs", ies_buf, ies_len);
		return -1;
	}

	return 0;
}


struct wpabuf * mb_ies_by_info(struct mb_ies_info *info)
{
	struct wpabuf *mb_ies = NULL;

	WPA_ASSERT(info != NULL);

	if (info->nof_ies) {
		u8 i;
		size_t mb_ies_size = 0;

		for (i = 0; i < info->nof_ies; i++)
			mb_ies_size += 2 + info->ies[i].ie_len;

		mb_ies = wpabuf_alloc(mb_ies_size);
		if (mb_ies) {
			for (i = 0; i < info->nof_ies; i++) {
				wpabuf_put_u8(mb_ies, WLAN_EID_MULTI_BAND);
				wpabuf_put_u8(mb_ies, info->ies[i].ie_len);
				wpabuf_put_data(mb_ies,
						info->ies[i].ie,
						info->ies[i].ie_len);
			}
		}
	}

	return mb_ies;
}


const struct oper_class_map global_op_class[] = {
	{ HOSTAPD_MODE_IEEE80211G, 81, 1, 13, 1, BW20, P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211G, 82, 14, 14, 1, BW20, NO_P2P_SUPP },

	/* Do not enable HT40 on 2.4 GHz for P2P use for now */
	{ HOSTAPD_MODE_IEEE80211G, 83, 1, 9, 1, BW40PLUS, NO_P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211G, 84, 5, 13, 1, BW40MINUS, NO_P2P_SUPP },

	{ HOSTAPD_MODE_IEEE80211A, 115, 36, 48, 4, BW20, P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 116, 36, 44, 8, BW40PLUS, P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 117, 40, 48, 8, BW40MINUS, P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 118, 52, 64, 4, BW20, NO_P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 119, 52, 60, 8, BW40PLUS, NO_P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 120, 56, 64, 8, BW40MINUS, NO_P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 121, 100, 140, 4, BW20, NO_P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 122, 100, 132, 8, BW40PLUS, NO_P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 123, 104, 136, 8, BW40MINUS, NO_P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 124, 149, 161, 4, BW20, P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 125, 149, 177, 4, BW20, P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 126, 149, 173, 8, BW40PLUS, P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 127, 153, 177, 8, BW40MINUS, P2P_SUPP },

	/*
	 * IEEE P802.11ax/D8.0 Table E-4 actually talks about channel center
	 * frequency index 42, 58, 106, 122, 138, 155, 171 with channel spacing
	 * of 80 MHz, but currently use the following definition for simplicity
	 * (these center frequencies are not actual channels, which makes
	 * wpas_p2p_verify_channel() fail). wpas_p2p_verify_80mhz() should take
	 * care of removing invalid channels.
	 */
	{ HOSTAPD_MODE_IEEE80211A, 128, 36, 177, 4, BW80, P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 129, 36, 177, 4, BW160, P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 131, 1, 233, 4, BW20, P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 132, 1, 233, 8, BW40, P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 133, 1, 233, 16, BW80, P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 134, 1, 233, 32, BW160, P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 135, 1, 233, 16, BW80P80, NO_P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211A, 136, 2, 2, 4, BW20, NO_P2P_SUPP },

	/*
	 * IEEE Std 802.11ad-2012 and P802.ay/D5.0 60 GHz operating classes.
	 * Class 180 has the legacy channels 1-6. Classes 181-183 include
	 * channels which implement channel bonding features.
	 */
	{ HOSTAPD_MODE_IEEE80211AD, 180, 1, 6, 1, BW2160, P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211AD, 181, 9, 13, 1, BW4320, P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211AD, 182, 17, 20, 1, BW6480, P2P_SUPP },
	{ HOSTAPD_MODE_IEEE80211AD, 183, 25, 27, 1, BW8640, P2P_SUPP },

	/* Keep the operating class 130 as the last entry as a workaround for
	 * the OneHundredAndThirty Delimiter value used in the Supported
	 * Operating Classes element to indicate the end of the Operating
	 * Classes field. */
	{ HOSTAPD_MODE_IEEE80211A, 130, 36, 177, 4, BW80P80, P2P_SUPP },
	{ -1, 0, 0, 0, 0, BW20, NO_P2P_SUPP }
};


static enum phy_type ieee80211_phy_type_by_freq(int freq)
{
	enum hostapd_hw_mode hw_mode;
	u8 channel;

	hw_mode = ieee80211_freq_to_chan(freq, &channel);

	switch (hw_mode) {
	case HOSTAPD_MODE_IEEE80211A:
		return PHY_TYPE_OFDM;
	case HOSTAPD_MODE_IEEE80211B:
		return PHY_TYPE_HRDSSS;
	case HOSTAPD_MODE_IEEE80211G:
		return PHY_TYPE_ERP;
	case HOSTAPD_MODE_IEEE80211AD:
		return PHY_TYPE_DMG;
	default:
		return PHY_TYPE_UNSPECIFIED;
	};
}


/* ieee80211_get_phy_type - Derive the phy type by freq and bandwidth */
enum phy_type ieee80211_get_phy_type(int freq, int ht, int vht)
{
	if (vht)
		return PHY_TYPE_VHT;
	if (ht)
		return PHY_TYPE_HT;

	return ieee80211_phy_type_by_freq(freq);
}


size_t global_op_class_size = ARRAY_SIZE(global_op_class);


/**
 * get_ie - Fetch a specified information element from IEs buffer
 * @ies: Information elements buffer
 * @len: Information elements buffer length
 * @eid: Information element identifier (WLAN_EID_*)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the IEs
 * buffer or %NULL in case the element is not found.
 */
const u8 * get_ie(const u8 *ies, size_t len, u8 eid)
{
	const struct element *elem;

	if (!ies)
		return NULL;

	for_each_element_id(elem, eid, ies, len)
		return &elem->id;

	return NULL;
}


/**
 * get_ie_ext - Fetch a specified extended information element from IEs buffer
 * @ies: Information elements buffer
 * @len: Information elements buffer length
 * @ext: Information element extension identifier (WLAN_EID_EXT_*)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the IEs
 * buffer or %NULL in case the element is not found.
 */
const u8 * get_ie_ext(const u8 *ies, size_t len, u8 ext)
{
	const struct element *elem;

	if (!ies)
		return NULL;

	for_each_element_extid(elem, ext, ies, len)
		return &elem->id;

	return NULL;
}


const u8 * get_vendor_ie(const u8 *ies, size_t len, u32 vendor_type)
{
	const struct element *elem;

	for_each_element_id(elem, WLAN_EID_VENDOR_SPECIFIC, ies, len) {
		if (elem->datalen >= 4 &&
		    vendor_type == WPA_GET_BE32(elem->data))
			return &elem->id;
	}

	return NULL;
}


size_t mbo_add_ie(u8 *buf, size_t len, const u8 *attr, size_t attr_len)
{
	/*
	 * MBO IE requires 6 bytes without the attributes: EID (1), length (1),
	 * OUI (3), OUI type (1).
	 */
	if (len < 6 + attr_len) {
		wpa_printf(MSG_DEBUG,
			   "MBO: Not enough room in buffer for MBO IE: buf len = %zu, attr_len = %zu",
			   len, attr_len);
		return 0;
	}

	*buf++ = WLAN_EID_VENDOR_SPECIFIC;
	*buf++ = attr_len + 4;
	WPA_PUT_BE24(buf, OUI_WFA);
	buf += 3;
	*buf++ = MBO_OUI_TYPE;
	os_memcpy(buf, attr, attr_len);

	return 6 + attr_len;
}


size_t add_multi_ap_ie(u8 *buf, size_t len, u8 value)
{
	u8 *pos = buf;

	if (len < 9)
		return 0;

	*pos++ = WLAN_EID_VENDOR_SPECIFIC;
	*pos++ = 7; /* len */
	WPA_PUT_BE24(pos, OUI_WFA);
	pos += 3;
	*pos++ = MULTI_AP_OUI_TYPE;
	*pos++ = MULTI_AP_SUB_ELEM_TYPE;
	*pos++ = 1; /* len */
	*pos++ = value;

	return pos - buf;
}


static const struct country_op_class us_op_class[] = {
	{ 1, 115 },
	{ 2, 118 },
	{ 3, 124 },
	{ 4, 121 },
	{ 5, 125 },
	{ 12, 81 },
	{ 22, 116 },
	{ 23, 119 },
	{ 24, 122 },
	{ 25, 126 },
	{ 26, 126 },
	{ 27, 117 },
	{ 28, 120 },
	{ 29, 123 },
	{ 30, 127 },
	{ 31, 127 },
	{ 32, 83 },
	{ 33, 84 },
	{ 34, 180 },
};

static const struct country_op_class eu_op_class[] = {
	{ 1, 115 },
	{ 2, 118 },
	{ 3, 121 },
	{ 4, 81 },
	{ 5, 116 },
	{ 6, 119 },
	{ 7, 122 },
	{ 8, 117 },
	{ 9, 120 },
	{ 10, 123 },
	{ 11, 83 },
	{ 12, 84 },
	{ 17, 125 },
	{ 18, 180 },
};

static const struct country_op_class jp_op_class[] = {
	{ 1, 115 },
	{ 30, 81 },
	{ 31, 82 },
	{ 32, 118 },
	{ 33, 118 },
	{ 34, 121 },
	{ 35, 121 },
	{ 36, 116 },
	{ 37, 119 },
	{ 38, 119 },
	{ 39, 122 },
	{ 40, 122 },
	{ 41, 117 },
	{ 42, 120 },
	{ 43, 120 },
	{ 44, 123 },
	{ 45, 123 },
	{ 56, 83 },
	{ 57, 84 },
	{ 58, 121 },
	{ 59, 180 },
};

static const struct country_op_class cn_op_class[] = {
	{ 1, 115 },
	{ 2, 118 },
	{ 3, 125 },
	{ 4, 116 },
	{ 5, 119 },
	{ 6, 126 },
	{ 7, 81 },
	{ 8, 83 },
	{ 9, 84 },
};

static u8
global_op_class_from_country_array(u8 op_class, size_t array_size,
				   const struct country_op_class *country_array)
{
	size_t i;

	for (i = 0; i < array_size; i++) {
		if (country_array[i].country_op_class == op_class)
			return country_array[i].global_op_class;
	}

	return 0;
}


u8 country_to_global_op_class(const char *country, u8 op_class)
{
	const struct country_op_class *country_array;
	size_t size;
	u8 g_op_class;

	if (country_match(us_op_class_cc, country)) {
		country_array = us_op_class;
		size = ARRAY_SIZE(us_op_class);
	} else if (country_match(eu_op_class_cc, country)) {
		country_array = eu_op_class;
		size = ARRAY_SIZE(eu_op_class);
	} else if (country_match(jp_op_class_cc, country)) {
		country_array = jp_op_class;
		size = ARRAY_SIZE(jp_op_class);
	} else if (country_match(cn_op_class_cc, country)) {
		country_array = cn_op_class;
		size = ARRAY_SIZE(cn_op_class);
	} else {
		/*
		 * Countries that do not match any of the above countries use
		 * global operating classes
		 */
		return op_class;
	}

	g_op_class = global_op_class_from_country_array(op_class, size,
							country_array);

	/*
	 * If the given operating class did not match any of the country's
	 * operating classes, assume that global operating class is used.
	 */
	return g_op_class ? g_op_class : op_class;
}


const struct oper_class_map * get_oper_class(const char *country, u8 op_class)
{
	const struct oper_class_map *op;

	if (country)
		op_class = country_to_global_op_class(country, op_class);

	op = &global_op_class[0];
	while (op->op_class && op->op_class != op_class)
		op++;

	if (!op->op_class)
		return NULL;

	return op;
}


int oper_class_bw_to_int(const struct oper_class_map *map)
{
	switch (map->bw) {
	case BW20:
		return 20;
	case BW40:
	case BW40PLUS:
	case BW40MINUS:
		return 40;
	case BW80:
		return 80;
	case BW80P80:
	case BW160:
		return 160;
	case BW2160:
		return 2160;
	default:
		return 0;
	}
}


int center_idx_to_bw_6ghz(u8 idx)
{
	/* Channel: 2 */
	if (idx == 2)
		return 0; /* 20 MHz */
	/* channels: 1, 5, 9, 13... */
	if ((idx & 0x3) == 0x1)
		return 0; /* 20 MHz */
	/* channels 3, 11, 19... */
	if ((idx & 0x7) == 0x3)
		return 1; /* 40 MHz */
	/* channels 7, 23, 39.. */
	if ((idx & 0xf) == 0x7)
		return 2; /* 80 MHz */
	/* channels 15, 47, 79...*/
	if ((idx & 0x1f) == 0xf)
		return 3; /* 160 MHz */

	return -1;
}


bool is_6ghz_freq(int freq)
{
	if (freq < 5935 || freq > 7115)
		return false;

	if (freq == 5935)
		return true;

	if (center_idx_to_bw_6ghz((freq - 5950) / 5) < 0)
		return false;

	return true;
}


bool is_6ghz_op_class(u8 op_class)
{
	return op_class >= 131 && op_class <= 136;
}


bool is_6ghz_psc_frequency(int freq)
{
	int i;

	if (!is_6ghz_freq(freq) || freq == 5935)
		return false;
	if ((((freq - 5950) / 5) & 0x3) != 0x1)
		return false;

	i = (freq - 5950 + 55) % 80;
	if (i == 0)
		i = (freq - 5950 + 55) / 80;

	if (i >= 1 && i <= 15)
		return true;

	return false;
}


/**
 * get_6ghz_sec_channel - Get the relative position of the secondary channel
 * to the primary channel in 6 GHz
 * @channel: Primary channel to be checked for (in global op class 131)
 * Returns: 1 = secondary channel above, -1 = secondary channel below
 */

int get_6ghz_sec_channel(int channel)
{
	/*
	 * In the 6 GHz band, primary channels are numbered as 1, 5, 9, 13.., so
	 * the 40 MHz channels are formed with the channel pairs as (1,5),
	 * (9,13), (17,21)..
	 * The secondary channel for a given primary channel is below the
	 * primary channel for the channels 5, 13, 21.. and it is above the
	 * primary channel for the channels 1, 9, 17..
	 */

	if (((channel - 1) / 4) % 2)
		return -1;
	return 1;
}


int ieee802_11_parse_candidate_list(const char *pos, u8 *nei_rep,
				    size_t nei_rep_len)
{
	u8 *nei_pos = nei_rep;
	const char *end;

	/*
	 * BSS Transition Candidate List Entries - Neighbor Report elements
	 * neighbor=<BSSID>,<BSSID Information>,<Operating Class>,
	 * <Channel Number>,<PHY Type>[,<hexdump of Optional Subelements>]
	 */
	while (pos) {
		u8 *nei_start;
		long int val;
		char *endptr, *tmp;

		pos = os_strstr(pos, " neighbor=");
		if (!pos)
			break;
		if (nei_pos + 15 > nei_rep + nei_rep_len) {
			wpa_printf(MSG_DEBUG,
				   "Not enough room for additional neighbor");
			return -1;
		}
		pos += 10;

		nei_start = nei_pos;
		*nei_pos++ = WLAN_EID_NEIGHBOR_REPORT;
		nei_pos++; /* length to be filled in */

		if (hwaddr_aton(pos, nei_pos)) {
			wpa_printf(MSG_DEBUG, "Invalid BSSID");
			return -1;
		}
		nei_pos += ETH_ALEN;
		pos += 17;
		if (*pos != ',') {
			wpa_printf(MSG_DEBUG, "Missing BSSID Information");
			return -1;
		}
		pos++;

		val = strtol(pos, &endptr, 0);
		WPA_PUT_LE32(nei_pos, val);
		nei_pos += 4;
		if (*endptr != ',') {
			wpa_printf(MSG_DEBUG, "Missing Operating Class");
			return -1;
		}
		pos = endptr + 1;

		*nei_pos++ = atoi(pos); /* Operating Class */
		pos = os_strchr(pos, ',');
		if (pos == NULL) {
			wpa_printf(MSG_DEBUG, "Missing Channel Number");
			return -1;
		}
		pos++;

		*nei_pos++ = atoi(pos); /* Channel Number */
		pos = os_strchr(pos, ',');
		if (pos == NULL) {
			wpa_printf(MSG_DEBUG, "Missing PHY Type");
			return -1;
		}
		pos++;

		*nei_pos++ = atoi(pos); /* PHY Type */
		end = os_strchr(pos, ' ');
		tmp = os_strchr(pos, ',');
		if (tmp && (!end || tmp < end)) {
			/* Optional Subelements (hexdump) */
			size_t len;

			pos = tmp + 1;
			end = os_strchr(pos, ' ');
			if (end)
				len = end - pos;
			else
				len = os_strlen(pos);
			if (nei_pos + len / 2 > nei_rep + nei_rep_len) {
				wpa_printf(MSG_DEBUG,
					   "Not enough room for neighbor subelements");
				return -1;
			}
			if (len & 0x01 ||
			    hexstr2bin(pos, nei_pos, len / 2) < 0) {
				wpa_printf(MSG_DEBUG,
					   "Invalid neighbor subelement info");
				return -1;
			}
			nei_pos += len / 2;
			pos = end;
		}

		nei_start[1] = nei_pos - nei_start - 2;
	}

	return nei_pos - nei_rep;
}


int ieee802_11_ext_capab(const u8 *ie, unsigned int capab)
{
	if (!ie || ie[1] <= capab / 8)
		return 0;
	return !!(ie[2 + capab / 8] & BIT(capab % 8));
}


bool ieee802_11_rsnx_capab_len(const u8 *rsnxe, size_t rsnxe_len,
			       unsigned int capab)
{
	const u8 *end;
	size_t flen, i;
	u32 capabs = 0;

	if (!rsnxe || rsnxe_len == 0)
		return false;
	end = rsnxe + rsnxe_len;
	flen = (rsnxe[0] & 0x0f) + 1;
	if (rsnxe + flen > end)
		return false;
	if (flen > 4)
		flen = 4;
	for (i = 0; i < flen; i++)
		capabs |= rsnxe[i] << (8 * i);

	return capabs & BIT(capab);
}


bool ieee802_11_rsnx_capab(const u8 *rsnxe, unsigned int capab)
{
	return ieee802_11_rsnx_capab_len(rsnxe ? rsnxe + 2 : NULL,
					 rsnxe ? rsnxe[1] : 0, capab);
}


void hostapd_encode_edmg_chan(int edmg_enable, u8 edmg_channel,
			      int primary_channel,
			      struct ieee80211_edmg_config *edmg)
{
	if (!edmg_enable) {
		edmg->channels = 0;
		edmg->bw_config = 0;
		return;
	}

	/* Only EDMG CB1 and EDMG CB2 contiguous channels supported for now */
	switch (edmg_channel) {
	case EDMG_CHANNEL_9:
		edmg->channels = EDMG_CHANNEL_9_SUBCHANNELS;
		edmg->bw_config = EDMG_BW_CONFIG_5;
		return;
	case EDMG_CHANNEL_10:
		edmg->channels = EDMG_CHANNEL_10_SUBCHANNELS;
		edmg->bw_config = EDMG_BW_CONFIG_5;
		return;
	case EDMG_CHANNEL_11:
		edmg->channels = EDMG_CHANNEL_11_SUBCHANNELS;
		edmg->bw_config = EDMG_BW_CONFIG_5;
		return;
	case EDMG_CHANNEL_12:
		edmg->channels = EDMG_CHANNEL_12_SUBCHANNELS;
		edmg->bw_config = EDMG_BW_CONFIG_5;
		return;
	case EDMG_CHANNEL_13:
		edmg->channels = EDMG_CHANNEL_13_SUBCHANNELS;
		edmg->bw_config = EDMG_BW_CONFIG_5;
		return;
	default:
		if (primary_channel > 0 && primary_channel < 7) {
			edmg->channels = BIT(primary_channel - 1);
			edmg->bw_config = EDMG_BW_CONFIG_4;
		} else {
			edmg->channels = 0;
			edmg->bw_config = 0;
		}
		break;
	}
}


/* Check if the requested EDMG configuration is a subset of the allowed
 * EDMG configuration. */
int ieee802_edmg_is_allowed(struct ieee80211_edmg_config allowed,
			    struct ieee80211_edmg_config requested)
{
	/*
	 * The validation check if the requested EDMG configuration
	 * is a subset of the allowed EDMG configuration:
	 * 1. Check that the requested channels are part (set) of the allowed
	 * channels.
	 * 2. P802.11ay defines the values of bw_config between 4 and 15.
	 * (bw config % 4) will give us 4 groups inside bw_config definition,
	 * inside each group we can check the subset just by comparing the
	 * bw_config value.
	 * Between this 4 groups, there is no subset relation - as a result of
	 * the P802.11ay definition.
	 * bw_config defined by IEEE P802.11ay/D4.0, 9.4.2.251, Table 13.
	 */
	if (((requested.channels & allowed.channels) != requested.channels) ||
	    ((requested.bw_config % 4) > (allowed.bw_config % 4)) ||
	    requested.bw_config > allowed.bw_config)
		return 0;

	return 1;
}


int op_class_to_bandwidth(u8 op_class)
{
	switch (op_class) {
	case 81:
	case 82:
		return 20;
	case 83: /* channels 1..9; 40 MHz */
	case 84: /* channels 5..13; 40 MHz */
		return 40;
	case 115: /* channels 36,40,44,48; indoor only */
		return 20;
	case 116: /* channels 36,44; 40 MHz; indoor only */
	case 117: /* channels 40,48; 40 MHz; indoor only */
		return 40;
	case 118: /* channels 52,56,60,64; dfs */
		return 20;
	case 119: /* channels 52,60; 40 MHz; dfs */
	case 120: /* channels 56,64; 40 MHz; dfs */
		return 40;
	case 121: /* channels 100-140 */
		return 20;
	case 122: /* channels 100-142; 40 MHz */
	case 123: /* channels 104-136; 40 MHz */
		return 40;
	case 124: /* channels 149,153,157,161 */
	case 125: /* channels 149,153,157,161,165,169,173,177 */
		return 20;
	case 126: /* channels 149,157,161,165,169,173; 40 MHz */
	case 127: /* channels 153..177; 40 MHz */
		return 40;
	case 128: /* center freqs 42, 58, 106, 122, 138, 155, 171; 80 MHz */
		return 80;
	case 129: /* center freqs 50, 114, 163; 160 MHz */
		return 160;
	case 130: /* center freqs 42, 58, 106, 122, 138, 155, 171; 80+80 MHz */
		return 80;
	case 131: /* UHB channels, 20 MHz: 1, 5, 9.. */
		return 20;
	case 132: /* UHB channels, 40 MHz: 3, 11, 19.. */
		return 40;
	case 133: /* UHB channels, 80 MHz: 7, 23, 39.. */
		return 80;
	case 134: /* UHB channels, 160 MHz: 15, 47, 79.. */
	case 135: /* UHB channels, 80+80 MHz: 7, 23, 39.. */
		return 160;
	case 136: /* UHB channels, 20 MHz: 2 */
		return 20;
	case 180: /* 60 GHz band, channels 1..8 */
		return 2160;
	case 181: /* 60 GHz band, EDMG CB2, channels 9..15 */
		return 4320;
	case 182: /* 60 GHz band, EDMG CB3, channels 17..22 */
		return 6480;
	case 183: /* 60 GHz band, EDMG CB4, channel 25..29 */
		return 8640;
	}

	return 20;
}


int op_class_to_ch_width(u8 op_class)
{
	switch (op_class) {
	case 81:
	case 82:
		return CHANWIDTH_USE_HT;
	case 83: /* channels 1..9; 40 MHz */
	case 84: /* channels 5..13; 40 MHz */
		return CHANWIDTH_USE_HT;
	case 115: /* channels 36,40,44,48; indoor only */
		return CHANWIDTH_USE_HT;
	case 116: /* channels 36,44; 40 MHz; indoor only */
	case 117: /* channels 40,48; 40 MHz; indoor only */
		return CHANWIDTH_USE_HT;
	case 118: /* channels 52,56,60,64; dfs */
		return CHANWIDTH_USE_HT;
	case 119: /* channels 52,60; 40 MHz; dfs */
	case 120: /* channels 56,64; 40 MHz; dfs */
		return CHANWIDTH_USE_HT;
	case 121: /* channels 100-140 */
		return CHANWIDTH_USE_HT;
	case 122: /* channels 100-142; 40 MHz */
	case 123: /* channels 104-136; 40 MHz */
		return CHANWIDTH_USE_HT;
	case 124: /* channels 149,153,157,161 */
	case 125: /* channels 149,153,157,161,165,169,171 */
		return CHANWIDTH_USE_HT;
	case 126: /* channels 149,157,165, 173; 40 MHz */
	case 127: /* channels 153,161,169,177; 40 MHz */
		return CHANWIDTH_USE_HT;
	case 128: /* center freqs 42, 58, 106, 122, 138, 155, 171; 80 MHz */
		return CHANWIDTH_80MHZ;
	case 129: /* center freqs 50, 114, 163; 160 MHz */
		return CHANWIDTH_160MHZ;
	case 130: /* center freqs 42, 58, 106, 122, 138, 155, 171; 80+80 MHz */
		return CHANWIDTH_80P80MHZ;
	case 131: /* UHB channels, 20 MHz: 1, 5, 9.. */
		return CHANWIDTH_USE_HT;
	case 132: /* UHB channels, 40 MHz: 3, 11, 19.. */
		return CHANWIDTH_USE_HT;
	case 133: /* UHB channels, 80 MHz: 7, 23, 39.. */
		return CHANWIDTH_80MHZ;
	case 134: /* UHB channels, 160 MHz: 15, 47, 79.. */
		return CHANWIDTH_160MHZ;
	case 135: /* UHB channels, 80+80 MHz: 7, 23, 39.. */
		return CHANWIDTH_80P80MHZ;
	case 136: /* UHB channels, 20 MHz: 2 */
		return CHANWIDTH_USE_HT;
	case 180: /* 60 GHz band, channels 1..8 */
		return CHANWIDTH_2160MHZ;
	case 181: /* 60 GHz band, EDMG CB2, channels 9..15 */
		return CHANWIDTH_4320MHZ;
	case 182: /* 60 GHz band, EDMG CB3, channels 17..22 */
		return CHANWIDTH_6480MHZ;
	case 183: /* 60 GHz band, EDMG CB4, channel 25..29 */
		return CHANWIDTH_8640MHZ;
	}
	return CHANWIDTH_USE_HT;
}


struct wpabuf * ieee802_11_defrag_data(struct ieee802_11_elems *elems,
				       u8 eid, u8 eid_ext,
				       const u8 *data, u8 len)
{
	struct frag_ies_info *frag_ies = &elems->frag_ies;
	struct wpabuf *buf;
	unsigned int i;

	if (!elems || !data || !len)
		return NULL;

	buf = wpabuf_alloc_copy(data, len);
	if (!buf)
		return NULL;

	for (i = 0; i < frag_ies->n_frags; i++) {
		int ret;

		if (frag_ies->frags[i].eid != eid ||
		    frag_ies->frags[i].eid_ext != eid_ext)
			continue;

		ret = wpabuf_resize(&buf, frag_ies->frags[i].ie_len);
		if (ret < 0) {
			wpabuf_free(buf);
			return NULL;
		}

		/* Copy only the fragment data (without the EID and length) */
		wpabuf_put_data(buf, frag_ies->frags[i].ie,
				frag_ies->frags[i].ie_len);
	}

	return buf;
}


struct wpabuf * ieee802_11_defrag(struct ieee802_11_elems *elems,
				  u8 eid, u8 eid_ext)
{
	const u8 *data;
	u8 len;

	/*
	 * TODO: Defragmentation mechanism can be supported for all IEs. For now
	 * handle only those that are used (or use ieee802_11_defrag_data()).
	 */
	switch (eid) {
	case WLAN_EID_EXTENSION:
		switch (eid_ext) {
		case WLAN_EID_EXT_FILS_HLP_CONTAINER:
			data = elems->fils_hlp;
			len = elems->fils_hlp_len;
			break;
		case WLAN_EID_EXT_WRAPPED_DATA:
			data = elems->wrapped_data;
			len = elems->wrapped_data_len;
			break;
		default:
			wpa_printf(MSG_DEBUG,
				   "Defragmentation not supported. eid_ext=%u",
				   eid_ext);
			return NULL;
		}
		break;
	default:
		wpa_printf(MSG_DEBUG,
			   "Defragmentation not supported. eid=%u", eid);
		return NULL;
	}

	return ieee802_11_defrag_data(elems, eid, eid_ext, data, len);
}
