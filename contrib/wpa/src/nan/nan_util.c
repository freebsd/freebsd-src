/*
 * Wi-Fi Aware - NAN module utils
 * Copyright (C) 2025 Intel Corporation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include "common.h"
#include "utils/bitfield.h"
#include "common/wpa_common.h"
#include "common/ieee802_11_common.h"
#include "nan_i.h"


static void nan_attrs_clear_list(struct nan_data *nan,
				 struct dl_list *list)
{
	struct nan_attrs_entry *entry, *pentry;

	dl_list_for_each_safe(entry, pentry, list, struct nan_attrs_entry,
			      list) {
		dl_list_del(&entry->list);
		os_free(entry);
	}
}


/*
 * nan_attrs_clear - Free data from NAN parsing
 * @nan: NAN module context from nan_init()
 * @attrs: Parsed nan_attrs
 */
void nan_attrs_clear(struct nan_data *nan, struct nan_attrs *attrs)
{
	nan_attrs_clear_list(nan, &attrs->serv_desc_ext);
	nan_attrs_clear_list(nan, &attrs->avail);
	nan_attrs_clear_list(nan, &attrs->ndc);
	nan_attrs_clear_list(nan, &attrs->ulw);
	nan_attrs_clear_list(nan, &attrs->dev_capa);
	nan_attrs_clear_list(nan, &attrs->element_container);

	os_memset(attrs, 0, sizeof(*attrs));
}


/*
 * nan_parse_attrs - Parse NAN attributes
 * @nan: NAN module context from nan_init()
 * @data: Buffer holding the attributes
 * @len: Length of &data
 * @attrs: On return would hold the parsed attributes
 * Returns: 0 on success; positive or negative indicate an error
 *
 * Note: In case of success, the caller must free temporary memory allocations
 * by calling nan_attrs_clear() when the parsed data is not needed anymore.
 */
int nan_parse_attrs(struct nan_data *nan, const u8 *data, size_t len,
		    struct nan_attrs *attrs)
{
	struct nan_attrs_entry *entry;
	const u8 *pos = data;
	const u8 *end = pos + len;

	os_memset(attrs, 0, sizeof(*attrs));

	dl_list_init(&attrs->serv_desc_ext);
	dl_list_init(&attrs->avail);
	dl_list_init(&attrs->ndc);
	dl_list_init(&attrs->ulw);
	dl_list_init(&attrs->dev_capa);
	dl_list_init(&attrs->element_container);

	while (end - pos > 3) {
		u8 id = *pos++;
		u16 attr_len = WPA_GET_LE16(pos);

		pos += 2;
		if (attr_len > end - pos)
			goto fail;

		switch (id) {
		case NAN_ATTR_SDEA:
			entry = os_zalloc(sizeof(*entry));
			if (!entry)
				goto fail;

			entry->ptr = pos;
			entry->len = attr_len;
			dl_list_add_tail(&attrs->serv_desc_ext, &entry->list);
			break;
		case NAN_ATTR_DEVICE_CAPABILITY:
			/* Validate Device Capability attribute length */
			if (attr_len < sizeof(struct nan_device_capa))
				break;

			entry = os_zalloc(sizeof(*entry));
			if (!entry)
				goto fail;

			entry->ptr = pos;
			entry->len = attr_len;
			dl_list_add_tail(&attrs->dev_capa, &entry->list);
			break;
		case NAN_ATTR_NDP:
			/* Validate minimal NDP attribute length */
			if (attr_len < sizeof(struct ieee80211_ndp))
				break;

			attrs->ndp = pos;
			attrs->ndp_len = attr_len;
			break;
		case NAN_ATTR_NAN_AVAILABILITY:
			/* Validate minimal Availability attribute length */
			if (attr_len < sizeof(struct nan_avail))
				break;

			entry = os_zalloc(sizeof(*entry));
			if (!entry)
				goto fail;

			entry->ptr = pos;
			entry->len = attr_len;
			dl_list_add_tail(&attrs->avail, &entry->list);
			break;
		case NAN_ATTR_NDC:
			/* Validate minimal NDC attribute length */
			if (attr_len < sizeof(struct ieee80211_ndc))
				break;

			entry = os_zalloc(sizeof(*entry));
			if (!entry)
				goto fail;

			entry->ptr = pos;
			entry->len = attr_len;
			dl_list_add_tail(&attrs->ndc, &entry->list);
			break;
		case NAN_ATTR_UNALIGNED_SCHEDULE:
			if (attr_len < sizeof(struct nan_unaligned_sched))
				break;

			entry = os_malloc(sizeof(*entry));
			if (!entry)
				goto fail;

			entry->ptr = pos;
			entry->len = attr_len;
			dl_list_add_tail(&attrs->ulw, &entry->list);
			break;
		case NAN_ATTR_NDL:
			/* Validate minimal NDL attribute length */
			if (attr_len < sizeof(struct ieee80211_ndl))
				break;

			attrs->ndl = pos;
			attrs->ndl_len = attr_len;
			break;
		case NAN_ATTR_NDL_QOS:
			/* Validate QoS attribute length */
			if (attr_len < sizeof(struct ieee80211_nan_qos))
				break;

			attrs->ndl_qos = pos;
			attrs->ndl_qos_len = attr_len;
			break;
		case NAN_ATTR_ELEM_CONTAINER:
			/* Validate minimal Element Container attribute length
			 */
			if (attr_len < 1)
				break;

			entry = os_zalloc(sizeof(*entry));
			if (!entry)
				goto fail;

			entry->ptr = pos;
			entry->len = attr_len;
			dl_list_add_tail(&attrs->element_container,
					 &entry->list);
			break;
		case NAN_ATTR_CSIA:
			if (attr_len < sizeof(struct nan_cipher_suite_info) +
			    sizeof(struct nan_cipher_suite))
				break;

			attrs->cipher_suite_info = pos;
			attrs->cipher_suite_info_len = attr_len;
			break;
		case NAN_ATTR_SCIA:
			if (attr_len < sizeof(struct nan_sec_ctxt))
				break;

			attrs->sec_ctxt_info = pos;
			attrs->sec_ctxt_info_len = attr_len;
			break;
		case NAN_ATTR_SHARED_KEY_DESCR:
			if (attr_len < sizeof(struct nan_shared_key) +
			    sizeof(struct wpa_eapol_key))
				break;

			attrs->shared_key_desc = pos;
			attrs->shared_key_desc_len = attr_len;
			break;
		case NAN_ATTR_DCEA:
			attrs->dev_capa_ext = pos;
			attrs->dev_capa_ext_len = attr_len;
			break;
		case NAN_ATTR_NPBA:
			/*
			 * Validate minimal NPBA length: Dialog Token (1) +
			 * Type and Statuss (1) + Reason Code (1) +
			 * Pairing Bootstrapping Method (2)
			 */
			if (attr_len < 5)
				break;
			attrs->npba = pos;
			attrs->npba_len = attr_len;
			break;
		case NAN_ATTR_NIRA:
			if (pos[0] != NAN_NIRA_CIPHER_VER_128)
				break;

			/* Cipher Version (1) + Nonce (8) + Tag (8) */
			if (attr_len !=
			    1 + NAN_NIRA_NONCE_LEN + NAN_NIRA_TAG_LEN)
				break;

			attrs->nira = pos;
			attrs->nira_len = attr_len;
			break;
		case NAN_ATTR_NDP_EXT:
			/*
			 * Validate minimal NDPE attribute length. NDP and NDPE
			 * attributes have the common structure and thus the
			 * same minimal length requirement based on the common
			 * fields (see struct ieee80211_ndp).
			 */
			if (attr_len < sizeof(struct ieee80211_ndp))
				break;

			attrs->ndpe = pos;
			attrs->ndpe_len = attr_len;
			break;
		case NAN_ATTR_MASTER_INDICATION:
		case NAN_ATTR_CLUSTER:
		case NAN_ATTR_NAN_ATTR_SERVICE_ID_LIST:
		case NAN_ATTR_SDA:
		case NAN_ATTR_CONN_CAPA:
		case NAN_ATTR_WLAN_INFRA:
		case NAN_ATTR_P2P_OPER:
		case NAN_ATTR_IBSS:
		case NAN_ATTR_MESH:
		case NAN_ATTR_FURTHER_NAN_SD:
		case NAN_ATTR_FURTHER_AVAIL_MAP:
		case NAN_ATTR_COUNTRY_CODE:
		case NAN_ATTR_RANGING:
		case NAN_ATTR_CLUSTER_DISCOVERY:
		case NAN_ATTR_RANGING_INFO:
		case NAN_ATTR_RANGING_SETUP:
		case NAN_ATTR_FTM_RANGING_REPORT:
		case NAN_ATTR_EXT_WLAN_INFRA:
		case NAN_ATTR_EXT_P2P_OPER:
		case NAN_ATTR_EXT_IBSS:
		case NAN_ATTR_EXT_MESH:
		case NAN_ATTR_PUBLIC_AVAILABILITY:
		case NAN_ATTR_SUBSC_SERVICE_ID_LIST:
		case NAN_ATTR_S3:
		case NAN_ATTR_TPEA:
		case NAN_ATTR_VENDOR_SPECIFIC:
			wpa_printf(MSG_DEBUG, "NAN: ignore attr=%u", id);
			break;
		default:
			wpa_printf(MSG_DEBUG, "NAN: unknown attr=%u", id);
			break;
		}

		pos += attr_len;
	}

	/* Parsing is considered success only if all attributes were consumed */
	if (pos == end)
		return 0;

fail:
	nan_attrs_clear(nan, attrs);
	return -1;
}


/*
 * nan_is_naf - Check if a given frame is a NAN Action frame
 * @mgmt: NAN Action frame
 * @len: Length of the Management frame in octets
 * Returns: true if NAF; otherwise false
 */
bool nan_is_naf(const struct ieee80211_mgmt *mgmt, size_t len)
{
	u8 subtype;

	/*
	 * 802.11 header + category + NAN Action frame minimal + subtype (1)
	 */
	if (len < IEEE80211_MIN_ACTION_LEN(naf)) {
		wpa_printf(MSG_DEBUG, "NAN: Too short NAN frame");
		return false;
	}

	if (mgmt->u.action.u.naf.action != WLAN_PA_VENDOR_SPECIFIC ||
	    WPA_GET_BE24(mgmt->u.action.u.naf.oui) != OUI_WFA ||
	    mgmt->u.action.u.naf.oui_type != NAN_NAF_OUI_TYPE)
		return false;

	subtype = mgmt->u.action.u.naf.subtype;

	if (mgmt->u.action.category != WLAN_ACTION_PUBLIC &&
	    !(subtype >= NAN_SUBTYPE_DATA_PATH_REQUEST &&
	      subtype <= NAN_SUBTYPE_DATA_PATH_TERMINATION &&
	      mgmt->u.action.category == WLAN_ACTION_PROTECTED_DUAL)) {
		wpa_printf(MSG_DEBUG, "NAN: Invalid action category for NAF");
		return false;
	}

	return true;
}


/*
 * nan_parse_naf - Parse a NAN Action frame content
 * @nan: NAN module context from nan_init()
 * @mgmt: NAN action frame
 * @len: Length of the management frame in octets
 * @msg: Buffer for returning parsed attributes
 * Returns: 0 on success; positive or negative indicate an error
 *
 * Note: in case of success, the caller must free temporary memory allocations
 * by calling nan_attrs_clear() when the parsed data is not needed anymore. In
 * addition, as the &mgmt is referenced from the returned structure, the caller
 * must ensure that the frame buffer remains valid and unmodified as long as the
 * &msg object is used.
 */
int nan_parse_naf(struct nan_data *nan, const struct ieee80211_mgmt *mgmt,
		  size_t len, struct nan_msg *msg)
{
	if (!nan_is_naf(mgmt, len))
		return -1;

	wpa_printf(MSG_DEBUG, "NAN: Parse NAF");

	msg->oui_type = mgmt->u.action.u.naf.oui_type;
	msg->oui_subtype = mgmt->u.action.u.naf.subtype;

	msg->mgmt = mgmt;
	msg->len = len;

	return nan_parse_attrs(nan,
			       mgmt->u.action.u.naf.variable,
			       len - IEEE80211_MIN_ACTION_LEN(naf),
			       &msg->attrs);
}


/*
 * nan_add_dev_capa_attr - Add Device Capability attribute
 * @nan: NAN module context from nan_init()
 * @buf: wpabuf to which the attribute would be added
 */
void nan_add_dev_capa_attr(struct nan_data *nan, struct wpabuf *buf)
{
	wpabuf_put_u8(buf, NAN_ATTR_DEVICE_CAPABILITY);
	wpabuf_put_le16(buf, sizeof(struct nan_device_capa));

	/* Device capabilities apply to the device, so set map ID = 0 */
	wpabuf_put_u8(buf, 0);
	wpabuf_put_le16(buf, nan->cfg->dev_capa.cdw_info);
	wpabuf_put_u8(buf, nan->cfg->dev_capa.supported_bands);
	wpabuf_put_u8(buf, nan->cfg->dev_capa.op_mode);
	wpabuf_put_u8(buf, nan->cfg->dev_capa.n_antennas);
	wpabuf_put_le16(buf, nan->cfg->dev_capa.channel_switch_time);
	wpabuf_put_u8(buf, nan->cfg->dev_capa.capa);
}


/*
 * nan_add_csia - Add Cipher Suite Information Attribute (CSIA) to a buffer
 * @buf: Buffer to add the attribute to
 * @capab: Capabilities field (1 octet)
 * @cs_list_len: Number of cipher suites in the list
 * @cs_list: Array of cipher suite structures
 *
 * This function constructs and appends a NAN Cipher Suite Information Attribute
 * to the provided buffer.
 *
 * Returns: 0 on success, -1 on failure (insufficient buffer space)
 */
int nan_add_csia(struct wpabuf *buf, u8 capab, size_t cs_list_len,
		 const struct nan_cipher_suite *cs_list)
{
	size_t i;
	/* Capabilities (1 octet) + Cipher Suite List */
	size_t attr_len = sizeof(capab) + cs_list_len * sizeof(*cs_list);

	if (wpabuf_tailroom(buf) <
	    (size_t) (NAN_ATTR_HDR_LEN + attr_len)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Not enough space to add CSIA attribute");
		return -1;
	}

	wpabuf_put_u8(buf, NAN_ATTR_CSIA);
	wpabuf_put_le16(buf, attr_len);
	wpabuf_put_u8(buf, capab);

	for (i = 0; i < cs_list_len; i++) {
		wpabuf_put_u8(buf, cs_list[i].csid);
		wpabuf_put_u8(buf, cs_list[i].instance_id);
	}

	return 0;
}


/**
 * nan_add_dev_capa_ext_attr - Add NAN Device Capability Extension attribute
 * @nan: NAN module context from nan_init()
 * @buf: wpabuf to which the attribute would be added
 */
void nan_add_dev_capa_ext_attr(struct nan_data *nan, struct wpabuf *buf)
{
	u8 pairing_and_npk_caching = 0;

	if (nan->cfg->pairing_cfg.pairing_setup)
		pairing_and_npk_caching |=
			NAN_DEV_CAPA_EXT_INFO_1_PAIRING_SETUP;
	if (nan->cfg->pairing_cfg.npk_caching)
		pairing_and_npk_caching |=
			NAN_DEV_CAPA_EXT_INFO_1_NPK_NIK_CACHING;

	if (!nan->cfg->dev_capa_ext_reg_info &&
	    !pairing_and_npk_caching)
		return;

	wpabuf_put_u8(buf, NAN_ATTR_DCEA);
	wpabuf_put_le16(buf, 2);
	wpabuf_put_u8(buf, nan->cfg->dev_capa_ext_reg_info);
	wpabuf_put_u8(buf, pairing_and_npk_caching);
}


/**
 * nan_add_nira - Add NIRA (NAN Identity Resolution Attribute) to a buffer
 * @buf: Buffer to which the NIRA is appended
 * @tag: Pointer to NIRA tag data (NAN_NIRA_TAG_LEN bytes)
 * @nonce: Pointer to NIRA nonce data (NAN_NIRA_NONCE_LEN bytes)
 * Returns: 0 on success, -1 if there is insufficient space in the buffer
 *
 * This function constructs and appends a NAN Identity Resolution Attribute
 * (NIRA) to the provided buffer.
 */
int nan_add_nira(struct wpabuf *buf, const u8 *tag, const u8 *nonce)
{
	u16 attr_len = 1 + NAN_NIRA_NONCE_LEN + NAN_NIRA_TAG_LEN;

	if (wpabuf_tailroom(buf) < (size_t) (NAN_ATTR_HDR_LEN + attr_len)) {
		wpa_printf(MSG_INFO, "NAN: Not enough space to add NIRA");
		return -1;
	}

	wpabuf_put_u8(buf, NAN_ATTR_NIRA);
	wpabuf_put_le16(buf, attr_len);
	wpabuf_put_u8(buf, NAN_NIRA_CIPHER_VER_128);
	wpabuf_put_data(buf, nonce, NAN_NIRA_NONCE_LEN);
	wpabuf_put_data(buf, tag, NAN_NIRA_TAG_LEN);

	return 0;
}


/**
 * nan_chan_to_chan_idx_map - Convert an op_class and chan to channel bitmap
 * @nan: NAN module context from nan_init()
 * @op_class: the operating class
 * @channel: channel number
 * @chan_idx_map: On success, would hold the channel index bitmap
 * Returns: 0 on success, otherwise a negative value
 */
int nan_chan_to_chan_idx_map(struct nan_data *nan,
			     u8 op_class, u8 channel, u16 *chan_idx_map)
{
	int ret;
	const struct oper_class_map *op_c;

	if (!chan_idx_map)
		return -1;

	op_c = get_oper_class(NULL, op_class);
	if (!op_c)
		return -1;

	ret = op_class_chan_to_idx(op_c, channel);
	if (ret < 0)
		return ret;

	if ((size_t) ret >= (sizeof(*chan_idx_map) * 8))
		return -1;

	*chan_idx_map = BIT(ret);
	return 0;
}


static u16 nan_add_avail_entry(struct nan_data *nan,
			       struct nan_time_bitmap *tbm,
			       u8 type, u8 op_class, u16 chan_bm,
			       u8 prim_chan_bm, struct wpabuf *buf)
{
	u16 ctrl;
	u8 chan_ctrl;
	u8 *len_ptr;
	u8 nss = BITS(nan->cfg->dev_capa.n_antennas, NAN_DEV_CAPA_RX_ANT_MASK,
		      NAN_DEV_CAPA_RX_ANT_POS);

	len_ptr = wpabuf_put(buf, 2);

	/* Potential availability entries are handled separately */
	if (type != NAN_AVAIL_ENTRY_CTRL_TYPE_COMMITTED &&
	    type != NAN_AVAIL_ENTRY_CTRL_TYPE_COND) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Cannot add non committed/conditional entry");
		return 0;
	}

	/*
	 * Add the entry control field
	 * - usage preference is not set for committed and conditional
	 * - utilization is max.
	 */
	ctrl = type;
	ctrl |= NAN_AVAIL_ENTRY_DEF_UTIL << NAN_AVAIL_ENTRY_CTRL_UTIL_POS;
	ctrl |= nss << NAN_AVAIL_ENTRY_CTRL_RX_NSS_POS;
	ctrl |= NAN_AVAIL_ENTRY_CTRL_TBM_PRESENT;
	wpabuf_put_le16(buf, ctrl);

	/* Add the time bitmap control field */
	ctrl = tbm->duration << NAN_TIME_BM_CTRL_BIT_DURATION_POS;
	ctrl |= tbm->period << NAN_TIME_BM_CTRL_PERIOD_POS;
	ctrl |= tbm->offset << NAN_TIME_BM_CTRL_START_OFFSET_POS;
	wpabuf_put_le16(buf, ctrl);

	wpabuf_put_u8(buf, tbm->len);
	wpabuf_put_data(buf, tbm->bitmap, tbm->len);

	/* Add the channel entry: single contiguous channel entry */
	chan_ctrl = NAN_BAND_CHAN_CTRL_TYPE;
	chan_ctrl |= 1 << NAN_BAND_CHAN_CTRL_NUM_ENTRIES_POS;
	wpabuf_put_u8(buf, chan_ctrl);
	wpabuf_put_u8(buf, op_class);
	wpabuf_put_le16(buf, chan_bm);
	wpabuf_put_u8(buf, prim_chan_bm);

	WPA_PUT_LE16(len_ptr, (u8 *) wpabuf_put(buf, 0) - len_ptr - 2);
	return (u8 *) wpabuf_put(buf, 0) - len_ptr;
}


int nan_get_chan_bm(struct nan_data *nan, const struct nan_sched_chan *chan,
		    u8 *op_class, u16 *chan_bm, u16 *pri_chan_bm)
{
	u8 channel;
	enum hostapd_hw_mode mode;
	int ret, sec_channel_offset;
	int freq_offsset = chan->freq - chan->center_freq1;
	u32 idx;
	enum oper_chan_width bandwidth;

	switch (chan->bandwidth) {
	case 20:
	case 40:
	default:
		*pri_chan_bm = 0;
		bandwidth = CONF_OPER_CHWIDTH_USE_HT;
		break;
	case 80:
		bandwidth = CONF_OPER_CHWIDTH_80MHZ;

		idx = (freq_offsset + 30) / 20;
		*pri_chan_bm = BIT(idx);
		break;
	case 160:
		if (chan->center_freq2) {
			bandwidth = CONF_OPER_CHWIDTH_80P80MHZ;

			/* TODO: Need to support auxiliary channel bitmap */
			idx = (freq_offsset + 30) / 20;
			*pri_chan_bm = BIT(idx);
		} else {
			bandwidth = CONF_OPER_CHWIDTH_160MHZ;
			idx = (freq_offsset + 70) / 20;
			*pri_chan_bm = BIT(idx);
		}
		break;
	}

	if (freq_offsset > 0)
		sec_channel_offset = 1;
	else if (freq_offsset < 0)
		sec_channel_offset = -1;
	else
		sec_channel_offset = 0;

	wpa_printf(MSG_DEBUG,
		   "NAN: Get chan bm: freq=%d, center_freq1=%d, bandwidth=%u, sec_channel_offset=%d",
		   chan->freq, chan->center_freq1, chan->bandwidth,
		   freq_offsset);

	/* For bandwidths >= 80 need to use the center frequency */
	mode = ieee80211_freq_to_channel_ext(bandwidth ==
					     CONF_OPER_CHWIDTH_USE_HT ?
					     chan->freq : chan->center_freq1,
					     sec_channel_offset,
					     bandwidth, op_class, &channel);
	if (mode == NUM_HOSTAPD_MODES) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Cannot get channel and op_class");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "NAN: Derived op_class=%u, channel=%u",
		   *op_class, channel);

	ret = nan_chan_to_chan_idx_map(nan, *op_class, channel, chan_bm);
	if (ret) {
		wpa_printf(MSG_DEBUG, "NAN: Failed to derive channel bitmap");
		return -1;
	}

	return 0;
}


static void nan_add_pot_avail_entry(struct nan_data *nan,
				    struct nan_chan_entry *entries,
				    unsigned int n_entries, u8 pref,
				    struct wpabuf *buf)
{
	u16 ctrl;
	u8 chan_ctrl;
	size_t i;
	u8 nss = BITS(nan->cfg->dev_capa.n_antennas, NAN_DEV_CAPA_RX_ANT_MASK,
		      NAN_DEV_CAPA_RX_ANT_POS);

	wpa_printf(MSG_DEBUG, "NAN: Adding potential entry: n_entries=%u",
		   n_entries);

	if (!n_entries)
		return;

	/* The number of channel entries can be too big for the buffer */
	if (wpabuf_tailroom(buf) < 2 + 2 + 1 + n_entries * 4) {
		n_entries = (wpabuf_tailroom(buf) - 5) / 4;

		wpa_printf(MSG_DEBUG,
			   "NAN: Not enough space to add potential entries, reduce to %u",
			   n_entries);
	}

	/*
	 * ctrl (2) + chan control (1) + n_entries * (nan_chan_entry without
	 * the aux bitmap).
	 */
	wpabuf_put_le16(buf, 3 + n_entries * 4);

	ctrl = NAN_AVAIL_ENTRY_CTRL_TYPE_POTENTIAL;
	ctrl |= NAN_AVAIL_ENTRY_DEF_UTIL << NAN_AVAIL_ENTRY_CTRL_UTIL_POS;
	ctrl |= nss << NAN_AVAIL_ENTRY_CTRL_RX_NSS_POS;
	ctrl |= pref << NAN_AVAIL_ENTRY_CTRL_USAGE_PREF_POS;
	wpabuf_put_le16(buf, ctrl);

	/* Add all channel entries */
	chan_ctrl = NAN_BAND_CHAN_CTRL_TYPE;
	chan_ctrl |= n_entries << NAN_BAND_CHAN_CTRL_NUM_ENTRIES_POS;
	wpabuf_put_u8(buf, chan_ctrl);

	for (i = 0; i < n_entries; i++) {
		struct nan_chan_entry *cur = &entries[i];

		wpabuf_put_u8(buf, cur->op_class);
		wpabuf_put_le16(buf, cur->chan_bitmap);
		wpabuf_put_u8(buf, 0);
	}
}


static void
nan_build_pot_avail_entry_with_chans(struct nan_data *nan,
				     const struct nan_channels *pot_chans,
				     struct wpabuf *buf, u8 map_id)
{
	struct nan_chan_entry chan_entries[global_op_class_size];
	size_t i, n_entries;

	os_memset(chan_entries, 0, sizeof(chan_entries));
	n_entries = 0;

	wpa_printf(MSG_DEBUG, "NAN: Adding potential entries: n_chans=%u",
		   pot_chans->n_chans);

	for (i = 0; i < pot_chans->n_chans; i++) {
		struct nan_channel_info *chan = &pot_chans->chans[i];
		struct nan_chan_entry *cur;
		u16 cbm = 0;
		size_t j;
		int ret;

		if (i > 0 && pot_chans->chans[i - 1].pref != chan->pref) {
			nan_add_pot_avail_entry(nan, chan_entries, n_entries,
						pot_chans->chans[i - 1].pref,
						buf);

			os_memset(chan_entries, 0, sizeof(chan_entries));
			n_entries = 0;
		}

		ret = nan_chan_to_chan_idx_map(nan, chan->op_class,
					       chan->channel, &cbm);
		if (ret)
			continue;

		/* Try to find and entry that matches the operating class */
		for (j = 0, cur = NULL; j < n_entries; j++) {
			cur = &chan_entries[j];

			if (!cur->op_class || cur->op_class == chan->op_class)
				break;
		}

		if (!n_entries)
			cur = &chan_entries[n_entries++];
		else if (j == n_entries && n_entries < global_op_class_size)
			cur = &chan_entries[n_entries++];
		else if (!cur)
			continue;

		cur->op_class = chan->op_class;
		cur->chan_bitmap |= cbm;
	}

	if (n_entries)
		nan_add_pot_avail_entry(nan, chan_entries, n_entries,
					pot_chans->chans[i - 1].pref, buf);

	wpa_printf(MSG_DEBUG, "NAN: Added potential entries: done");
}


static void nan_build_pot_avail_entry(struct nan_data *nan, struct wpabuf *buf,
				      u8 map_id)
{
	struct nan_channels pot_chans;

	os_memset(&pot_chans, 0, sizeof(pot_chans));

	if (!nan->cfg->get_chans ||
	    nan->cfg->get_chans(nan->cfg->cb_ctx, map_id, &pot_chans) < 0) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Failed to get channels. Not adding potential");
		return;
	}

	if (pot_chans.n_chans != 0)
		nan_build_pot_avail_entry_with_chans(nan, &pot_chans, buf,
						     map_id);
	else
		wpa_printf(MSG_DEBUG,
			   "NAN: No channels available. Not adding potential: map_id=%u",
			   map_id);

	os_free(pot_chans.chans);
}


/**
 * nan_add_avail_attrs - Add NAN availability attributes
 * @nan: NAN module context from nan_init()
 * @sequence_id: Sequence ID to be used in the availability attributes
 * @map_ids_bitmap: Bitmap of map IDs to be included in the availability
 *	attributes
 * @type_for_conditional: Type field to be used for conditional entries
 * @n_chans: Number of channels in chans
 * @chans: Channel schedules
 * @buf: Frame buffer to which the attribute would be added
 * @include_potential: Whether to add potential availability entries
 * Returns: 0 on success, negative on failure.
 *
 * An availability attribute is added for each map (identified by map ID) in the
 * schedule. All channels with the same map ID are added to the same
 * availability attribute. Each attribute will hold an availability entry for
 * committed slots and an availability entry for conditional slots.
 */
int nan_add_avail_attrs(struct nan_data *nan, u8 sequence_id,
			u32 map_ids_bitmap, u8 type_for_conditional,
			size_t n_chans, struct nan_chan_schedule *chans,
			struct wpabuf *buf, bool include_potential)
{
	u8 last_map_id = NAN_INVALID_MAP_ID;
	u32 handled_map_ids = 0;
	u8 *len_ptr = NULL;
	u8 i;

	wpa_printf(MSG_DEBUG, "NAN: Add availability attrs. n_chans=%zu",
		   n_chans);

	for (i = 0; i < n_chans; i++) {
		struct nan_chan_schedule *chan = &chans[i];
		u8 op_class;
		u16 chan_bm, pri_chan_bm;
		int ret;

		if (!chan->conditional.len && !chan->committed.len) {
			wpa_printf(MSG_DEBUG,
				   "NAN: committed and conditional are empty");
			continue;
		}

		ret = nan_get_chan_bm(nan, &chan->chan, &op_class,
				      &chan_bm, &pri_chan_bm);
		if (ret)
			continue;

		/*
		 * All channels with the same map ID should be added to the same
		 * availability attribute, so verify that the map IDs are
		 * sorted.
		 */
		if (last_map_id != NAN_INVALID_MAP_ID &&
		    last_map_id > chan->map_id) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Map IDs not sorted properly");
			return -1;
		}

		if (!(map_ids_bitmap & BIT(chan->map_id))) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Skip adding availability for map_id=%u",
				   chan->map_id);
			continue;
		}

		if (last_map_id != chan->map_id) {
			u16 ctrl;

			if (last_map_id != NAN_INVALID_MAP_ID) {
				wpa_printf(MSG_DEBUG,
					   "NAN: Add avail attr done: map_id=%u",
					   last_map_id);

				if (include_potential)
					nan_build_pot_avail_entry(nan, buf,
								  last_map_id);
				WPA_PUT_LE16(len_ptr,
					     (u8 *) wpabuf_put(buf, 0) -
					     len_ptr - 2);
			}

			last_map_id = chan->map_id;
			handled_map_ids |= BIT(last_map_id);

			wpa_printf(MSG_DEBUG, "NAN: Add avail attr map_id=%u",
				   last_map_id);

			wpabuf_put_u8(buf, NAN_ATTR_NAN_AVAILABILITY);
			len_ptr = wpabuf_put(buf, 2);
			wpabuf_put_u8(buf, sequence_id);

			ctrl = last_map_id << NAN_AVAIL_CTRL_MAP_ID_POS;

			/*
			 * The spec states that this bit should be set if the
			 * committed changed or if conditional is included. Set
			 * it anyway, as it is not known what information the
			 * peer has on our schedule. Similarly, always set the
			 * potential changed bit.
			 */
			ctrl |= NAN_AVAIL_CTRL_COMMITTED_CHANGED |
				NAN_AVAIL_CTRL_POTENTIAL_CHANGED;
			wpabuf_put_le16(buf, ctrl);
		}

		/* TODO: handle primary channel configuration */
		if (chan->committed.len)
			nan_add_avail_entry(nan, &chan->committed,
					    NAN_AVAIL_ENTRY_CTRL_TYPE_COMMITTED,
					    op_class, chan_bm, pri_chan_bm,
					    buf);

		if (chan->conditional.len)
			nan_add_avail_entry(nan, &chan->conditional,
					    type_for_conditional,
					    op_class, chan_bm, pri_chan_bm,
					    buf);
	}

	if (last_map_id != NAN_INVALID_MAP_ID) {
		if (include_potential)
			nan_build_pot_avail_entry(nan, buf, last_map_id);
		WPA_PUT_LE16(len_ptr, (u8 *) wpabuf_put(buf, 0) - len_ptr - 2);

		wpa_printf(MSG_DEBUG, "NAN: Add avail attr done: map_id=%u",
			   last_map_id);
	} else {
		wpa_printf(MSG_DEBUG,
			   "NAN: No committed/conditional entries were added");
	}

	if (!include_potential)
		return 0;

	/*
	 * Add NAN availability attributes with a single potential availability
	 * entry for map IDs that are not included in the schedule.
	 */
	map_ids_bitmap &= ~handled_map_ids;
	wpa_printf(MSG_DEBUG,
		   "NAN: Add avail attrs for remaining map IDs: bitmap=0x%x",
		   map_ids_bitmap);

	while (map_ids_bitmap) {
		struct nan_channels pot_chans;
		u8 map_id = ffs(map_ids_bitmap) - 1;
		u16 ctrl = map_id << NAN_AVAIL_CTRL_MAP_ID_POS |
			NAN_AVAIL_CTRL_POTENTIAL_CHANGED;

		map_ids_bitmap &= ~BIT(map_id);

		wpa_printf(MSG_DEBUG, "NAN: Add avail attr for map_id=%u",
			   map_id);

		os_memset(&pot_chans, 0, sizeof(pot_chans));

		if (!nan->cfg->get_chans ||
		    nan->cfg->get_chans(nan->cfg->cb_ctx, map_id,
					&pot_chans) < 0 ||
		    !pot_chans.chans) {
			wpa_printf(MSG_DEBUG,
				   "NAN: No channels available. Not adding potential: map_id=%u",
				   map_id);
			continue;
		}

		wpabuf_put_u8(buf, NAN_ATTR_NAN_AVAILABILITY);
		len_ptr = wpabuf_put(buf, 2);
		wpabuf_put_u8(buf, sequence_id);
		wpabuf_put_le16(buf, ctrl);

		nan_build_pot_avail_entry_with_chans(nan, &pot_chans, buf,
						     map_id);
		os_free(pot_chans.chans);

		WPA_PUT_LE16(len_ptr, (u8 *) wpabuf_put(buf, 0) - len_ptr - 2);
	}

	return 0;
}


/**
 * nan_del_avail_entry - Delete an availability entry
 * @entry: The availability entry to delete
 */
void nan_del_avail_entry(struct nan_avail_entry *entry)
{
	if (!entry)
		return;
	os_free(entry->band_chan);
	os_free(entry);
}


/**
 * nan_flush_avail_entries - Flush a list of availability entries
 * @avail_entries: List of availability entries
 */
void nan_flush_avail_entries(struct dl_list *avail_entries)
{
	struct nan_avail_entry *cur, *next;

	dl_list_for_each_safe(cur, next, avail_entries,
			      struct nan_avail_entry, list) {
		dl_list_del(&cur->list);
		nan_del_avail_entry(cur);
	}
}


/**
 * nan_sched_entries_to_avail_entries - Convert NAN schedule entries to NAN
 * availability entries
 *
 * @nan: NAN module context from nan_init()
 * @avail_entries: On successful return would hold a valid list of availability
 *     entries
 * @sched_entries: Buffer holding the schedule entries, each of type
 *     &struct nan_sched_entry
 * @sched_entries_len: Length of the sched_entries buffer
 */
int nan_sched_entries_to_avail_entries(struct nan_data *nan,
				       struct dl_list *avail_entries,
				       const u8 *sched_entries,
				       u16 sched_entries_len)
{
	dl_list_init(avail_entries);

	if (!sched_entries || !sched_entries_len)
		return 0;

	if (sched_entries_len < sizeof(struct nan_sched_entry)) {
		wpa_printf(MSG_DEBUG, "NAN: Schedule entry too short=%u",
			   sched_entries_len);
		return -1;
	}

	while (sched_entries_len > 0) {
		const struct nan_sched_entry *sched_entry =
			(const struct nan_sched_entry *) sched_entries;
		struct nan_avail_entry *avail_entry;
		u16 ctrl;
		size_t elen;

		if (sched_entries_len < sizeof(struct nan_sched_entry))
			goto fail;
		elen = sizeof(struct nan_sched_entry) + sched_entry->len;
		if (sched_entries_len < elen) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Invalid schedule entry len=%u",
				   sched_entry->len);
			goto fail;
		}

		if (sched_entry->len > NAN_TIME_BITMAP_MAX_LEN)
			goto fail;

		avail_entry = os_zalloc(sizeof(struct nan_avail_entry));
		if (!avail_entry)
			goto fail;

		avail_entry->map_id = sched_entry->map_id;
		ctrl = le_to_host16(sched_entry->control);

		avail_entry->tbm.duration =
			BITS(ctrl,
			     NAN_TIME_BM_CTRL_BIT_DURATION_MASK,
			     NAN_TIME_BM_CTRL_BIT_DURATION_POS);
		avail_entry->tbm.period =
			BITS(ctrl,
			     NAN_TIME_BM_CTRL_PERIOD_MASK,
			     NAN_TIME_BM_CTRL_PERIOD_POS);
		avail_entry->tbm.offset =
			BITS(ctrl,
			     NAN_TIME_BM_CTRL_START_OFFSET_MASK,
			     NAN_TIME_BM_CTRL_START_OFFSET_POS);

		avail_entry->tbm.len = sched_entry->len;
		os_memcpy(avail_entry->tbm.bitmap, sched_entry->bm,
			  sched_entry->len);

		dl_list_init(&avail_entry->list);
		dl_list_add(avail_entries, &avail_entry->list);

		sched_entries_len -= elen;
		sched_entries += elen;
	}

	return 0;

fail:
	nan_flush_avail_entries(avail_entries);
	return -1;
}


/**
 * nan_tbm_to_bf - Convert a time bitmap to bitfield
 * @nan: NAN module context from nan_init()
 * @tbm: Time bitmap
 * Returns: The converted bitfield on success; otherwise, NULL
 *
 * The function takes a time bitmap and converts it to a bitfield that
 * represents a time bitmap with 16 TUs slots that covers a period of 8192 TUs.
 * The conversion takes into account the duration, period, and offset fields of
 * the time bitmap.
 */
struct bitfield * nan_tbm_to_bf(struct nan_data *nan,
				const struct nan_time_bitmap *tbm)
{
	struct bitfield *bf, *base;
	u32 slot_duration, period, len;
	u32 dur_factor, i, j, iter, max_iter;

	wpa_printf(MSG_DEBUG,
		   "NAN: Convert time bitmap: len=%u, dur=%u, period=%u, offset=%u",
		   tbm->len, tbm->duration, tbm->period, tbm->offset);

	/* Calculate the length and make sure it is less than the period */
	dur_factor = 1 << tbm->duration;
	slot_duration = 16 * dur_factor;

	if (tbm->period == 0)
		period = tbm->len * 8 * slot_duration;
	else
		period = 128 * (1 << (tbm->period - 1));

	len = tbm->len;
	if (tbm->len * 8 * slot_duration > period) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Time bitmap length is bigger than duration. Chop it");
		len = period / slot_duration / 8;
	}

	/* The 'base' bitfield holds the original bitmap */
	base = bitfield_alloc_data(tbm->bitmap, tbm->len);
	if (!base) {
		wpa_printf(MSG_DEBUG, "NAN: Failed to allocate base bitmap");
		return NULL;
	}

	if (!len) {
		wpa_printf(MSG_DEBUG, "NAN: Empty time bitmap");
		return base;
	}

	/* Allocate a time bitmap to cover a 8192 TUs period */
	bf = bitfield_alloc(NAN_MAX_TIME_BITMAP_SLOTS);
	if (!bf) {
		bitfield_free(base);
		return NULL;
	}

	/*
	 * Convert the original map to a map of 16 TU slots taking into account
	 * the time bitmap offset and the period. Note that during availability
	 * attribute parsing, it was verified that offset is smaller than the
	 * period.
	 */
	max_iter = NAN_MAX_PERIOD_TUS / period;
	for (iter = 0; iter < max_iter; iter++) {
		u32 start_slot = tbm->offset + iter * (period / 16);

		for (i = 0;  i < len * 8; i++) {
			bool slot_set = bitfield_is_set(base, i);

			for (j = 0; j < dur_factor; j++) {
				u32 target_slot =
					start_slot + (i * dur_factor + j);

				if (target_slot >= NAN_MAX_TIME_BITMAP_SLOTS)
					goto done;

				if (slot_set)
					bitfield_set(bf, target_slot);
			}
		}
	}

done:
	bitfield_free(base);

	wpa_printf(MSG_DEBUG, "NAN: Done converting bitmap");

	return bf;
}


/**
 * nan_sched_to_bf - Convert schedule to bitfield
 * @nan: NAN module context from nan_init()
 * @sched: List of availability entries representing the schedule entries
 * @map_id: On return holds the map_id covered by the schedule entries
 * @reason: In case of failure contains the reason
 * Returns: A bitfield representing the schedule on success; otherwise NULL
 *
 * Note: The function only supports converting a schedule where all map IDs are
 * identical. There is no support for a schedule that uses different maps.
 */
struct bitfield * nan_sched_to_bf(struct nan_data *nan, struct dl_list *sched,
				  u8 *map_id, enum nan_reason *reason)
{
	struct bitfield *sched_bf = NULL;
	struct nan_avail_entry *cur;

	*map_id = NAN_INVALID_MAP_ID;

	/* Convert all schedule availability entries to bf */
	dl_list_for_each(cur, sched, struct nan_avail_entry, list) {
		struct bitfield *tmp;

		if (*map_id == NAN_INVALID_MAP_ID) {
			*map_id = cur->map_id;
		} else if (cur->map_id != *map_id) {
			wpa_printf(MSG_DEBUG,
				   "NAN: No support for multiple maps");
			*reason = NAN_REASON_RESOURCE_LIMITATION;
			goto fail;
		}

		tmp = nan_tbm_to_bf(nan, &cur->tbm);
		if (!tmp) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Failed to convert sched to bf");
			*reason = NAN_REASON_UNSPECIFIED_REASON;
			goto fail;
		}

		bitfield_dump(tmp, "NAN: Schedule entry bitmap");

		if (!sched_bf) {
			sched_bf = tmp;
		} else {
			int res;

			if (bitfield_intersects(sched_bf, tmp)) {
				wpa_printf(MSG_DEBUG,
					   "NAN: Invalid availability: TBMs intersect");
				*reason = NAN_REASON_INVALID_AVAILABILITY;
				bitfield_free(tmp);
				goto fail;
			}

			res = bitfield_union_in_place(sched_bf, tmp);
			bitfield_free(tmp);
			if (res) {
				wpa_printf(MSG_DEBUG,
					   "NAN: Failed to union sched bf");
				*reason = NAN_REASON_UNSPECIFIED_REASON;
				goto fail;
			}
		}
	}

	return sched_bf;

fail:
	bitfield_free(sched_bf);
	*map_id = NAN_INVALID_MAP_ID;
	return NULL;
}


/**
 * nan_sched_covered_by_avail_entry - Check if schedule is covered by the
 * availability entry
 *
 * @nan: NAN module context from nan_init()
 * @avail: Availability entry
 * @sched_bf: A bitfield representing the schedule
 * @map_id: Map ID corresponding to the schedule
 * Returns true of schedule is covered by the entry; false otherwise
 */
bool nan_sched_covered_by_avail_entry(struct nan_data *nan,
				      struct nan_avail_entry *avail,
				      struct bitfield *sched_bf, u8 map_id)
{
	struct bitfield *avail_bf = NULL;
	int ret;

	/* No schedule entries, avail_entry is good.. */
	if (!sched_bf)
		return true;

	wpa_printf(MSG_DEBUG,
		   "NAN: Check if schedule covered by availability entry");

	/* Schedule can only be covered by committed/conditional */
	if (avail->type != NAN_AVAIL_ENTRY_CTRL_TYPE_COMMITTED &&
	    avail->type != NAN_AVAIL_ENTRY_CTRL_TYPE_COND)
		return false;

	if (avail->map_id != map_id)
		return false;

	/* Convert the availability entry to bf */
	avail_bf = nan_tbm_to_bf(nan, &avail->tbm);
	if (!avail_bf)
		return false;

	bitfield_dump(avail_bf, "NAN: Availability entry bitmap");

	ret = bitfield_is_subset(avail_bf, sched_bf);
	wpa_printf(MSG_DEBUG, "NAN: Is schedule subset of entry=%d", ret);

	bitfield_free(avail_bf);

	return ret == 1;
}


static struct bitfield *
nan_sched_bf_from_avail_and_chan(struct nan_data *nan,
				 const struct dl_list *avail_entries,
				 u8 map_id, u8 op_class, u16 cbm)
{
	struct nan_avail_entry *avail;
	struct bitfield *res_bf = NULL;

	dl_list_for_each(avail, avail_entries, struct nan_avail_entry,
			 list) {
		struct bitfield *avail_bf;

		if (avail->map_id != map_id)
			continue;

		/* Schedule can only be covered by committed/conditional */
		if (avail->type != NAN_AVAIL_ENTRY_CTRL_TYPE_COMMITTED &&
		    avail->type != NAN_AVAIL_ENTRY_CTRL_TYPE_COND)
			continue;

		/* Now check channel, if it is given */
		if (op_class && cbm &&
		    (avail->n_band_chan < 1 ||
		     avail->band_chan_type != NAN_TYPE_CHANNEL ||
		     avail->band_chan[0].u.chan.op_class != op_class ||
		     !(avail->band_chan[0].u.chan.chan_bitmap & cbm)))
			continue;

		/* Convert the availability entry to bitfield */
		avail_bf = nan_tbm_to_bf(nan, &avail->tbm);
		if (!avail_bf)
			goto fail;

		bitfield_dump(avail_bf, "NAN: Availability entry bitmap");
		if (!res_bf) {
			res_bf = avail_bf;
		} else {
			struct bitfield *tmp_bf;

			tmp_bf = bitfield_union(res_bf, avail_bf);
			bitfield_free(avail_bf);

			if (!tmp_bf)
				goto fail;

			bitfield_free(res_bf);
			res_bf = tmp_bf;
		}
	}

	return res_bf;

fail:
	bitfield_free(res_bf);
	return NULL;
}


/**
 * nan_sched_covered_by_avail_entries - Check if schedule is covered by the
 * list of availability attributes
 *
 * @nan: NAN module context from nan_init()
 * @avail_entries: A list of availability entries (see &struct nan_avail_entry)
 * @sched: An array with 0 or more &struct nan_sched_entry entries
 * @sched_len: Length of the &sched array
 * Returns: true if schedule is covered by the entries; otherwise false.
 */
bool nan_sched_covered_by_avail_entries(struct nan_data *nan,
					struct dl_list *avail_entries,
					const u8 *sched, size_t sched_len)
{
	struct dl_list sched_entries;
	struct bitfield *sched_bf, *avail_bf;
	u8 map_id;
	bool ret = false;
	enum nan_reason reason;

	if (!sched || !sched_len)
		return true;

	dl_list_init(&sched_entries);
	if (nan_sched_entries_to_avail_entries(nan,
					       &sched_entries,
					       sched, sched_len))
		return false;

	sched_bf = nan_sched_to_bf(nan, &sched_entries, &map_id, &reason);
	if (!sched_bf) {
		nan_flush_avail_entries(&sched_entries);
		return false;
	}

	nan_flush_avail_entries(&sched_entries);

	avail_bf = nan_sched_bf_from_avail_and_chan(nan, avail_entries,
						    map_id, 0, 0);
	if (avail_bf)
		ret = bitfield_is_subset(avail_bf, sched_bf) ? true : false;

	wpa_printf(MSG_DEBUG, "NAN: Schedule is %sa subset of entries",
		   ret ? "" : "NOT ");

	bitfield_free(avail_bf);
	bitfield_free(sched_bf);

	return ret;
}


/**
 * nan_sched_bf_covered_by_avail_entries_and_chan - Check if schedule is covered
 * by the list of availability attributes matching the channel configurations
 *
 * @nan: NAN module context from nan_init()
 * @avail_entries: A list of availability entries. See &struct nan_avail_entry
 * @sched_bf: the bitfield representing the schedule
 * @map_id: Map ID associated with the schedule
 * @op_class: Operating class to match against
 * @cbm: Channel bitmap to match against
 * Returns: true of schedule is covered by the entries; otherwise false
 */
bool nan_sched_bf_covered_by_avail_entries_and_chan(
	struct nan_data *nan, const struct dl_list *avail_entries,
	struct bitfield *sched_bf, u8 map_id, u8 op_class, u16 cbm)
{
	struct bitfield *avail_bf;
	bool ret = false;

	/*
	 * Build a schedule bitfield from all the availability entries matching
	 * the map ID and channel configuration.
	 */
	avail_bf = nan_sched_bf_from_avail_and_chan(nan, avail_entries,
						    map_id, op_class, cbm);

	/*
	 * If there is such a schedule, verify that it is a superset of the
	 * given schedule.
	 */
	if (avail_bf && bitfield_is_subset(avail_bf, sched_bf))
		ret = true;

	wpa_printf(MSG_DEBUG,
		   "NAN: Is schedule covered by entries and chan=%u", ret);

	bitfield_free(avail_bf);
	return ret;
}


static int nan_get_control_channel(struct nan_data *nan, u8 op_class,
				   u16 cbm, u16 pri_cbm)
{
	const struct oper_class_map *op = get_oper_class(NULL, op_class);
	int freq = 0, idx;
	u8 chan_id;

	if (!op || op_class > 130)
		return -1;

	idx = ffs(cbm) - 1;
	if (idx < 0) {
		wpa_printf(MSG_DEBUG,
			   "NAN: No channel found in chan_bitmap 0x%04x for oper_class %u",
			   cbm, op_class);
		return -1;
	}

	chan_id = op_class_idx_to_chan(op, idx);
	if (!chan_id) {
		wpa_printf(MSG_DEBUG,
			   "NAN: No channel found for oper_class %u idx %u",
			   op_class, idx);
		return -1;
	}

	freq = ieee80211_chan_to_freq(NULL, op_class, chan_id);

	/*
	 * For operating classes with bandwidth < 80 MHz, the frequency is the
	 * control channel frequency. For operating classes with
	 * bandwidth >= 80 MHz, the frequency is the center frequency of the
	 * primary segment, so we need to derive the control channel frequency
	 * from the primary channel bitmap.
	 */
	if (op->bw == BW20 || op->bw == BW40 ||
	    op->bw == BW40PLUS || op->bw == BW40MINUS)
		return freq;

	if (!pri_cbm) {
		wpa_printf(MSG_DEBUG,
			   "NAN: No primary channel bitmap provided for oper_class %u",
			   op_class);
		return -1;
	}

	idx = ffs(pri_cbm) - 1;

	if (op->bw == BW80 || op->bw == BW80P80)
		return freq - 30 + idx * 20;

	if (op->bw == BW160)
		return freq - 70 + idx * 20;

	return -1;
}


/**
 * nan_avail_entries_to_bf - Convert availability entries that match the given
 * channel configuration to a bitfield.
 *
 * @nan: NAN module context from nan_init()
 * @avail_entries: A list of availability entries. See &struct nan_avail_entry
 * @op_class: Operating class to match against
 * @cbm: Channel bitmap to match against
 * @pri_cbm: Primary channel bitmap to match against
 * Returns: NULL on error or no match; otherwise returns a bitfield describing
 * all the available slots.
 */
struct bitfield * nan_avail_entries_to_bf(struct nan_data *nan,
					  const struct dl_list *avail_entries,
					  u8 op_class, u16 cbm, u16 pri_cbm)
{
	struct nan_avail_entry *avail;
	struct bitfield *res_bf = NULL;

	dl_list_for_each(avail, avail_entries, struct nan_avail_entry,
			 list) {
		struct bitfield *avail_bf;

		/* Schedule can only be covered by committed/conditional. */
		if (avail->type != NAN_AVAIL_ENTRY_CTRL_TYPE_COMMITTED &&
		    avail->type != NAN_AVAIL_ENTRY_CTRL_TYPE_COND)
			continue;

		/*
		 * Committed/conditional entries should have only a single
		 * channel entry.
		 */
		if (avail->n_band_chan != 1 ||
		    avail->band_chan_type != NAN_TYPE_CHANNEL)
			continue;

		/*
		 * Check that the availability entry channel matches. If it does
		 * not match, check if the channels are compatible, i.e., have
		 * the same control channel.
		 */
		if (avail->band_chan[0].u.chan.op_class != op_class ||
		    avail->band_chan[0].u.chan.chan_bitmap != cbm ||
		    avail->band_chan[0].u.chan.pri_chan_bitmap != pri_cbm) {
			int freq1, freq2;
			u16 chan_bitmap, pri_chan_bitmap;

			freq1 = nan_get_control_channel(nan, op_class,
							cbm, pri_cbm);

			chan_bitmap = le_to_host16(
				avail->band_chan[0].u.chan.chan_bitmap);
			pri_chan_bitmap = le_to_host16(
				avail->band_chan[0].u.chan.pri_chan_bitmap);
			freq2 = nan_get_control_channel(
				nan, avail->band_chan[0].u.chan.op_class,
				chan_bitmap, pri_chan_bitmap);

			if (freq2 == -1 || freq1 != freq2)
				continue;

			wpa_printf(MSG_DEBUG,
				   "NAN: Availability entry channel is compatible. Control channel freq=%d MHz",
				   freq1);
		}

		/* Convert the availability entry to a bitfield */
		avail_bf = nan_tbm_to_bf(nan, &avail->tbm);
		if (!avail_bf)
			goto fail;

		if (!res_bf) {
			res_bf = avail_bf;
		} else {
			struct bitfield *tmp_bf;

			tmp_bf = bitfield_union(res_bf, avail_bf);
			if (!tmp_bf)
				goto fail;

			bitfield_free(res_bf);
			bitfield_free(avail_bf);
			res_bf = tmp_bf;
		}
	}

	return res_bf;

fail:
	bitfield_free(res_bf);
	return NULL;
}


/**
 * nan_peer_dump_sched_to_buf - Dump peer schedule to a buffer
 * @sched: Peer schedule
 * @buf: Output buffer
 * @buflen: The length of &buf in bytes
 *
 * Returns: The number of characters written to the buffer, or -1 on error,
 * which indicates that the buffer was too small.
 */
int nan_peer_dump_sched_to_buf(struct nan_peer_schedule *sched,
			       char *buf, size_t buflen)
{
	int i, j, ret;
	char *pos = buf;
	char *end = buf + buflen;

	for (i = 0; i < sched->n_maps; i++) {
		struct nan_map *map = &sched->maps[i];

		ret = wpa_scnprintf(pos, end - pos,
				    "MAP [%u]\n\tmap_id=%u\n\tn_chans=%u\n",
				    i, map->map_id, map->n_chans);
		if (os_snprintf_error(end - pos, ret))
			goto err;
		pos += ret;

		for (j = 0; j < map->n_chans; j++) {
			struct nan_map_chan *chan = &map->chans[j];

			ret = wpa_scnprintf(pos, end - pos,
					    "\tchannel[%u]: committed=%u rx_nss=%u freq=%u bw=%u cfreq1=%u cfreq2=%u\n",
					    j, chan->committed, chan->rx_nss,
					    chan->chan.freq,
					    chan->chan.bandwidth,
					    chan->chan.center_freq1,
					    chan->chan.center_freq2);
			if (os_snprintf_error(end - pos, ret))
				goto err;
			pos += ret;

			ret = wpa_scnprintf(pos, end - pos,
					    "\t\tbitmap: period=%u duration=%u offset=%u ",
					    BIT(6 + chan->tbm.period),
					    BIT(4 + chan->tbm.duration),
					    16 * chan->tbm.offset);
			if (os_snprintf_error(end - pos, ret))
				goto err;
			pos += ret;

			ret = wpa_scnprintf(pos, end - pos, "bitmap=");
			if (os_snprintf_error(end - pos, ret))
				goto err;
			pos += ret;

			ret = wpa_snprintf_hex(pos, end - pos, chan->tbm.bitmap,
					       chan->tbm.len);
			if (os_snprintf_error(end - pos, ret))
				goto err;
			pos += ret;

			ret = wpa_scnprintf(pos, end - pos, "\n");
			if (os_snprintf_error(end - pos, ret))
				goto err;
			pos += ret;
		}

		ret = wpa_scnprintf(pos, end - pos,
				    "\tndc: period=%u duration=%u offset=%u bitmap=",
				    BIT(6 + map->ndc.period),
				    BIT(4 + map->ndc.duration),
				    16 * map->ndc.offset);
		if (os_snprintf_error(end - pos, ret))
			goto err;
		pos += ret;

		ret = wpa_snprintf_hex(pos, end - pos, map->ndc.bitmap,
				       map->ndc.len);
		if (os_snprintf_error(end - pos, ret))
			goto err;
		pos += ret;

		ret = wpa_scnprintf(pos, end - pos,
				    "\n\timmutable: period=%u duration=%u offset=%u bitmap=",
				    1 << (6 + map->immutable.period),
				    1 << (4 + map->immutable.duration),
				    16 * map->immutable.offset);
		if (os_snprintf_error(end - pos, ret))
			goto err;
		pos += ret;

		ret = wpa_snprintf_hex(pos, end - pos, map->immutable.bitmap,
				       map->immutable.len);
		if (os_snprintf_error(end - pos, ret))
			goto err;
		pos += ret;

		ret = wpa_scnprintf(pos, end - pos, "\n");
		if (os_snprintf_error(end - pos, ret))
			goto err;
		pos += ret;
	}

	ret = wpa_scnprintf(pos, end - pos, "max_idle_period=%u",
			    sched->max_idle_period);

	if (os_snprintf_error(end - pos, ret))
		goto err;

	pos += ret;

	return pos - buf;

err:
	wpa_printf(MSG_DEBUG, "NAN: Buffer too small to dump peer schedule");
	return -1;
}


/**
 * nan_peer_dump_pot_avail_to_buf - Dump peer potential availability to a text
 * buffer
 *
 * @pot_avail: Peer potential availability
 * @buf: Output buffer
 * @buflen: Length of &buf in bytes
 *
 * Returns: The number of characters written to the buffer, or -1 on error,
 * which indicates that the buffer was too small.
 */
int nan_peer_dump_pot_avail_to_buf(struct nan_peer_potential_avail *pot_avail,
				   char *buf, size_t buflen)
{
	unsigned int i, j;
	int ret;
	char *pos = buf;
	char *end = buf + buflen;

	for (i = 0; i < pot_avail->n_maps; i++) {
		struct pot_entry *pot = &pot_avail->maps[i];

		ret = wpa_scnprintf(pos, end - pos,
				    "entry[%u]: rx_nss=%u pref=%u util=%u\n",
				    i, pot->rx_nss, pot->preference,
				    pot->utilization);
		if (os_snprintf_error(end - pos, ret))
			goto err;
		pos += ret;

		for (j = 0; j < pot->n_band_chan; j++) {
			if (pot->is_band) {
				ret = wpa_scnprintf(pos, end - pos,
						    "\tband[%u]: band_id=%u\n",
						    j, pot->entries[j].band_id);
			} else {
				ret = wpa_scnprintf(
					pos, end - pos,
					"\tchan[%u]: op_class=%u chan_bitmap=0x%04x\n",
					j, pot->entries[j].op_class,
					pot->entries[j].chan_bitmap);
			}
			if (os_snprintf_error(end - pos, ret))
				goto err;
			pos += ret;
		}
	}

	return pos - buf;

err:
	wpa_printf(MSG_DEBUG,
		   "NAN: Buffer too small to dump peer potential availability");
	return -1;
}


/**
 * nan_get_peer_ndc_freq - Get peer NDC frequency from schedule
 * @nan: Pointer to NAN data struct
 * @peer_sched: Pointer to peer schedule struct
 * @map_idx: Index of the availability map to check
 * Returns: Frequency of the peer channel that intersects with NDC,
 *          or -1 on failure or if no intersection found
 *
 * In case NDC bitmap spans across multiple channels, only one channel is
 * returned (that corresponds to the first NDC bit).
 */
int nan_get_peer_ndc_freq(struct nan_data *nan,
			  const struct nan_peer_schedule *peer_sched,
			  u8 map_idx)
{
	struct bitfield *ndc_bf;
	int i;

	if (map_idx >= peer_sched->n_maps) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Invalid map index %u for peer schedule",
			   map_idx);
		return -1;
	}

	ndc_bf = nan_tbm_to_bf(nan, &peer_sched->maps[map_idx].ndc);
	if (!ndc_bf)
		return -1;

	for (i = 0; i < peer_sched->maps[map_idx].n_chans; i++) {
		struct bitfield *peer_chan_map;

		/*
		 * Check all peer channel entries (committed or conditional).
		 * Potential entries are not part of the peer schedule.
		 */
		peer_chan_map =
			nan_tbm_to_bf(nan,
				      &peer_sched->maps[map_idx].chans[i].tbm);
		if (!peer_chan_map) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Failed to convert peer channel TBM to bitfield");
			bitfield_free(ndc_bf);
			return -1;
		}

		if (bitfield_intersects(ndc_bf, peer_chan_map)) {
			bitfield_free(ndc_bf);
			bitfield_free(peer_chan_map);
			return peer_sched->maps[map_idx].chans[i].chan.freq;
		}

		bitfield_free(peer_chan_map);
	}

	bitfield_free(ndc_bf);
	return -1;
}


/**
 * nan_get_chan_entry - Get channel entry for a given NAN scheduled channel
 * @nan: NAN module context from nan_init()
 * @chan: NAN scheduled channel
 * @chan_entry: On successful return holds the channel entry.
 * Returns: 0 on success; otherwise -1
 */
int nan_get_chan_entry(struct nan_data *nan, const struct nan_sched_chan *chan,
		       struct nan_chan_entry *chan_entry)
{
	u8 op_class;
	u16 chan_bm, pri_chan_bm;
	int ret;

	if (!chan || !chan_entry)
		return -1;

	ret = nan_get_chan_bm(nan, chan, &op_class, &chan_bm, &pri_chan_bm);
	if (ret)
		return ret;

	os_memset(chan_entry, 0, sizeof(*chan_entry));
	chan_entry->op_class = op_class;
	chan_entry->chan_bitmap = host_to_le16(chan_bm);
	chan_entry->pri_chan_bitmap = pri_chan_bm & 0xff;

	return 0;
}


/**
 * nan_convert_chan_sched_to_bf - Convert channel schedule to bitfield
 * and get the channel information.
 *
 * @nan: NAN module context from nan_init()
 * @chan: Channel schedule to convert
 * @avail_bf: On successful return holds the availability bitmap of the given
 *     channel schedule
 * @map_id: On successful return holds the map ID for the schedule
 * @op_class: On successful return holds the operating class for the schedule
 *     with the peer
 * @cbm: On successful return holds the channel bitmap for the operating class
 * @pcbm: On successful return holds the primary channel bitmap for the
 *     channel in case of bandwidth greater than 40 MHz
 * Returns: 0 on success; -1 on failure
 */
int nan_convert_chan_sched_to_bf(struct nan_data *nan,
				 const struct nan_chan_schedule *chan,
				 struct bitfield **avail_bf, u8 *map_id,
				 u8 *op_class, u16 *cbm, u16 *pcbm)
{
	struct bitfield *committed_bf, *conditional_bf;
	int ret;

	*op_class = 0;
	*cbm = 0;
	*pcbm = 0;
	*map_id = chan->map_id;

	ret = nan_get_chan_bm(nan, &chan->chan, op_class, cbm, pcbm);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Failed to convert channel info");
		return -1;
	}

	committed_bf = nan_tbm_to_bf(nan, &chan->committed);
	if (!committed_bf) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Failed to build committed bitfield");
		return -1;
	}

	conditional_bf = nan_tbm_to_bf(nan, &chan->conditional);
	if (!conditional_bf) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Failed to build conditional bitfield");
		bitfield_free(committed_bf);
		return -1;
	}

	*avail_bf = bitfield_union(committed_bf, conditional_bf);
	bitfield_free(committed_bf);
	bitfield_free(conditional_bf);

	if (!*avail_bf) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Failed to unify committed and conditional bitfields");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "NAN: NDL: map_id=%u, op_class=%u, cbm=0x%x",
		   *map_id, *op_class, *cbm);
	return 0;
}


/**
 * nan_peer_schedule_intersection - Get local and peer schedules intersection
 * @nan: NAN module context from nan_init()
 * @peer: The peer with whom to intersect the schedule
 * @sched: Local device schedule
 * Returns: A bitfield representing the intersection of schedules, or NULL if
 *	no intersection
 *
 * The function checks if the local device schedule intersects with the peer
 * device schedule and returns a bitfield representing the intersection, or
 * NULL if no intersection.
 */
struct bitfield * nan_peer_schedule_intersection(
	struct nan_data *nan, const struct nan_peer *peer,
	const struct nan_schedule *sched)
{
	size_t i;
	struct bitfield *common_bf = NULL;
	bool intersects = false;

	/*
	 * Iterate over all the channels included in the local schedule. For
	 * each channel convert the committed and conditional slots to a
	 * bitfield object and extract the operating class and channel bitmap.
	 *
	 * Using the operating class and channel bitmap find the peer
	 * availability on that channel and check if it intersect with the
	 * local one.
	 */
	wpa_printf(MSG_DEBUG, "NAN: n_chans=%u, ndc_map_id=%u",
		   sched->n_chans, sched->ndc_map_id);

	for (i = 0; i < sched->n_chans; i++) {
		struct bitfield *own_chan_bf = NULL, *peer_chan_bf = NULL;
		u16 cbm, pri_cbm;
		u8 map_id, op_class;
		int ret;

		/* Convert the schedule for the current channel to bitfield */
		ret = nan_convert_chan_sched_to_bf(nan, &sched->chans[i],
						   &own_chan_bf, &map_id,
						   &op_class, &cbm, &pri_cbm);
		if (ret) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDL: Failed to convert chan sched to bitfield");
			return NULL;
		}

		/* Get the peer availability for the current channel */
		peer_chan_bf =
			nan_avail_entries_to_bf(nan,
						&peer->info.avail_entries,
						op_class, cbm, pri_cbm);
		if (!peer_chan_bf) {
			bitfield_free(own_chan_bf);
			continue;
		}

		intersects |= bitfield_intersects(own_chan_bf, peer_chan_bf);

		ret = bitfield_intersect_in_place(own_chan_bf, peer_chan_bf);
		if (ret < 0) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Failed to intersect own and peer chan bitfields");
			bitfield_free(own_chan_bf);
			bitfield_free(peer_chan_bf);
			bitfield_free(common_bf);
			return NULL;
		}

		bitfield_free(peer_chan_bf);

		if (common_bf) {
			ret = bitfield_union_in_place(common_bf, own_chan_bf);
			if (ret) {
				wpa_printf(MSG_DEBUG,
					   "NAN: Failed to unify own chan bitfields");

				bitfield_free(own_chan_bf);
				bitfield_free(common_bf);
				return NULL;
			}
		} else {
			common_bf = bitfield_dup(own_chan_bf);
			if (!common_bf) {
				wpa_printf(MSG_DEBUG,
					   "NAN: Failed to dup own chan bitfield");

				bitfield_free(own_chan_bf);
				bitfield_free(common_bf);
				return NULL;
			}
		}

		bitfield_free(own_chan_bf);
	}

	if (!intersects) {
		bitfield_free(common_bf);
		return NULL;
	}

	return common_bf;
}


void nan_add_kde_hdr(struct wpabuf *buf, u32 kde, size_t data_len)
{
	wpabuf_put_u8(buf, WLAN_EID_VENDOR_SPECIFIC);
	wpabuf_put_u8(buf, RSN_SELECTOR_LEN + data_len);
	RSN_SELECTOR_PUT(wpabuf_put(buf, RSN_SELECTOR_LEN), kde);
}
