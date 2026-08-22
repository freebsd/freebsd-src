/*
 * Proxmity Ranging
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "crypto/sha256.h"
#include "pasn/pasn_common.h"
#include "proximity_ranging.h"


static bool valid_country_ch(char c)
{
	return c >= 'A' && c <= 'Z';
}


static void pr_device_free(struct pr_data *pr, struct pr_device *dev)
{
#ifdef CONFIG_PASN
	wpabuf_free(dev->ranging_wrapper);
	if (dev->pasn) {
		wpa_pasn_reset(dev->pasn);
		pasn_data_deinit(dev->pasn);
	}
#endif /* CONFIG_PASN */
	os_free(dev);
}


static struct pr_device * pr_get_device(struct pr_data *pr, const u8 *addr)
{
	struct pr_device *dev;

	dl_list_for_each(dev, &pr->devices, struct pr_device, list) {
		if (ether_addr_equal(dev->pr_device_addr, addr))
			return dev;
	}
	return NULL;
}


static struct pr_device * pr_create_device(struct pr_data *pr, const u8 *addr)
{
	struct pr_device *dev, *oldest = NULL;
	size_t count = 0;

	dev = pr_get_device(pr, addr);
	if (dev)
		return dev;

	dl_list_for_each(dev, &pr->devices, struct pr_device, list) {
		count++;
		if (!oldest ||
		    os_reltime_before(&dev->last_seen, &oldest->last_seen))
			oldest = dev;
	}
	if (count + 1 > PR_MAX_PEER && oldest) {
		wpa_printf(MSG_DEBUG,
			   "PR: Remove oldest peer entry to make room for a new peer "
			   MACSTR, MAC2STR(oldest->pr_device_addr));
		dl_list_del(&oldest->list);
		pr_device_free(pr, oldest);
	}

	dev = os_zalloc(sizeof(*dev));
	if (!dev)
		return NULL;

	dl_list_add(&pr->devices, &dev->list);
	os_memcpy(dev->pr_device_addr, addr, ETH_ALEN);
	wpa_printf(MSG_DEBUG, "PR: New Proximity Ranging device " MACSTR
		   " added to list", MAC2STR(addr));

	return dev;
}


struct pr_data * pr_init(const struct pr_config *cfg)
{
	struct pr_data *pr;

	pr = os_zalloc(sizeof(*pr) + sizeof(*cfg));
	if (!pr)
		return NULL;

	pr->cfg = (struct pr_config *) (pr + 1);
	os_memcpy(pr->cfg, cfg, sizeof(*cfg));
	if (cfg->dev_name)
		pr->cfg->dev_name = os_strdup(cfg->dev_name);
	else
		pr->cfg->dev_name = NULL;

	dl_list_init(&pr->devices);
	dl_list_init(&pr->dev_iks);

#ifdef CONFIG_PASN
	pr->initiator_pmksa = pasn_initiator_pmksa_cache_init();
	pr->responder_pmksa = pasn_responder_pmksa_cache_init();
#endif /* CONFIG_PASN */

	return pr;
}


static void pr_deinit_dev_iks(struct pr_data *pr)
{
	struct pr_dev_ik *dev_ik, *prev_dev_ik;

	dl_list_for_each_safe(dev_ik, prev_dev_ik, &pr->dev_iks,
			      struct pr_dev_ik, list) {
		dl_list_del(&dev_ik->list);
		os_free(dev_ik);
	}
}


void pr_flush(struct pr_data *pr)
{
#ifdef CONFIG_PASN
	if (pr->initiator_pmksa)
		pasn_initiator_pmksa_cache_flush(pr->initiator_pmksa);
	if (pr->responder_pmksa)
		pasn_responder_pmksa_cache_flush(pr->responder_pmksa);
#endif /* CONFIG_PASN */
}

void pr_deinit(struct pr_data *pr)
{
	struct pr_device *dev, *prev;

	if (!pr)
		return;

	os_free(pr->cfg->dev_name);

	dl_list_for_each_safe(dev, prev, &pr->devices, struct pr_device, list) {
		dl_list_del(&dev->list);
		pr_device_free(pr, dev);
	}

	pr_deinit_dev_iks(pr);

#ifdef CONFIG_PASN
	os_free(pr->pr_pasn_params);
	pr->pr_pasn_params = NULL;
	pr->ranging_final_received = false;

	pasn_initiator_pmksa_cache_deinit(pr->initiator_pmksa);
	pasn_responder_pmksa_cache_deinit(pr->responder_pmksa);
#endif /* CONFIG_PASN */

	os_free(pr);
	wpa_printf(MSG_DEBUG, "PR: Deinit done");
}


void pr_set_dev_addr(struct pr_data *pr, const u8 *addr)
{
	if (pr && addr)
		os_memcpy(pr->cfg->dev_addr, addr, ETH_ALEN);
}


void pr_clear_dev_iks(struct pr_data *pr)
{
	struct pr_device *dev;

	pr->cfg->dik_len = 0;
	os_memset(pr->cfg->dik_data, 0, DEVICE_IDENTITY_KEY_LEN);
	pr->cfg->global_password_valid = false;
	os_memset(pr->cfg->global_password, 0,
		  sizeof(pr->cfg->global_password));

	dl_list_for_each(dev, &pr->devices, struct pr_device, list) {
		dev->password_valid = false;
		os_memset(dev->password, 0, sizeof(dev->password));
		dev->dik_valid = false;
		os_memset(dev->dik, 0, DEVICE_IDENTITY_KEY_LEN);
	}

	pr_deinit_dev_iks(pr);
}


void pr_add_dev_ik(struct pr_data *pr, const u8 *dik, const char *password,
		   const u8 *pmk, size_t pmk_len, bool own)
{
	struct pr_dev_ik *dev_ik;

	if (own) {
		os_memcpy(pr->cfg->dik_data, dik, DEVICE_IDENTITY_KEY_LEN);
		pr->cfg->dik_len = DEVICE_IDENTITY_KEY_LEN;
		if (password) {
			os_strlcpy(pr->cfg->global_password, password,
				   sizeof(pr->cfg->global_password));
			pr->cfg->global_password_valid = true;
		}
		return;
	}

	if (pmk && (pmk_len != 32 && pmk_len != 48 && pmk_len != 64)) {
		wpa_printf(MSG_INFO, "PR: Unexpected PMK length %zu", pmk_len);
		return;
	}

	dl_list_for_each(dev_ik, &pr->dev_iks, struct pr_dev_ik, list) {
		if (os_memcmp(dik, dev_ik->dik, DEVICE_IDENTITY_KEY_LEN) == 0) {
			dl_list_del(&dev_ik->list);
			os_free(dev_ik);
			break;
		}
	}

	dev_ik = os_zalloc(sizeof(*dev_ik));
	if (!dev_ik)
		return;

	dl_list_add(&pr->dev_iks, &dev_ik->list);
	os_memcpy(dev_ik->dik, dik, DEVICE_IDENTITY_KEY_LEN);
	if (password) {
		os_strlcpy(dev_ik->password, password,
			   sizeof(dev_ik->password));
		dev_ik->password_valid = true;
	}
	if (pmk) {
		os_memcpy(dev_ik->pmk, pmk, pmk_len);
		dev_ik->pmk_len = pmk_len;
		dev_ik->pmk_valid = true;
	}

	wpa_printf(MSG_DEBUG, "PR: New Device Identity added to list");
}


int pr_set_peer_credentials(struct pr_data *pr, const u8 *addr,
			    const u8 *pmk, size_t pmk_len,
			    const char *password)
{
	struct pr_device *dev;

	if (!pr || !addr)
		return -1;

	dev = pr_get_device(pr, addr);
	if (!dev) {
		wpa_printf(MSG_DEBUG, "PR: set_peer_credentials: " MACSTR
			   " not found", MAC2STR(addr));
		return -1;
	}

	if (pmk && pmk_len) {
		if (pmk_len > PMK_LEN_MAX)
			return -1;
		os_memcpy(dev->pmk, pmk, pmk_len);
		dev->pmk_len = pmk_len;
		dev->pmk_valid = true;
		wpa_printf(MSG_DEBUG, "PR: PMK set for " MACSTR, MAC2STR(addr));
	}

	if (password) {
		if (os_strlen(password) >= sizeof(dev->password))
			return -1;
		os_strlcpy(dev->password, password, sizeof(dev->password));
		dev->password_valid = true;
		wpa_printf(MSG_DEBUG, "PR: password set for " MACSTR,
			   MAC2STR(addr));
	}

	return 0;
}


static struct wpabuf * pr_encaps_elem(const struct wpabuf *subelems,
				      u32 ie_type)
{
	struct wpabuf *ie = NULL;
	const u8 *pos, *end;
	size_t len = 0;

	if (!subelems)
		return NULL;

	len = wpabuf_len(subelems) + 1000;
	ie = wpabuf_alloc(len);
	if (!ie)
		return NULL;

	pos = wpabuf_head(subelems);
	end = pos + wpabuf_len(subelems);

	while (end > pos) {
		size_t frag_len = end - pos;

		if (frag_len > 251)
			frag_len = 251;
		wpabuf_put_u8(ie, WLAN_EID_VENDOR_SPECIFIC);
		wpabuf_put_u8(ie, 4 + frag_len);
		wpabuf_put_be32(ie, ie_type);
		wpabuf_put_data(ie, pos, frag_len);
		pos += frag_len;
	}
	return ie;
}


static void pr_get_ranging_capabilities(struct pr_data *pr,
					struct pr_capabilities *capab)
{
	os_memset(capab, 0, sizeof(struct pr_capabilities));

	if (pr->cfg->dev_name)
		os_strlcpy(capab->device_name, pr->cfg->dev_name,
			   sizeof(capab->device_name));

	if (pr->cfg->edca_ista_support || pr->cfg->edca_rsta_support)
		capab->edca_support = true;

	if (pr->cfg->ntb_ista_support || pr->cfg->ntb_rsta_support)
		capab->ntb_support = true;

	capab->secure_he_ltf = pr->cfg->secure_he_ltf;
	capab->pasn_type = pr->cfg->pasn_type;
	capab->support_6ghz = pr->cfg->support_6ghz;
}


static void pr_get_edca_capabilities(struct pr_data *pr,
				     struct edca_capabilities *capab)
{
	u16 edca_hw_caps = 0;

	os_memset(capab, 0, sizeof(struct edca_capabilities));
	capab->ista_support = pr->cfg->edca_ista_support;
	capab->rsta_support = pr->cfg->edca_rsta_support;
	os_memcpy(capab->country, pr->cfg->country, 3);

	edca_hw_caps |= (pr->cfg->edca_format_and_bw & EDCA_FORMAT_AND_BW_MASK)
		<< EDCA_FORMAT_AND_BW;
	edca_hw_caps |= (pr->cfg->max_tx_antenna & EDCA_MAX_TX_ANTENNA_MASK) <<
		EDCA_MAX_TX_ANTENNA;
	edca_hw_caps |= (pr->cfg->max_rx_antenna & EDCA_MAX_RX_ANTENNA_MASK) <<
		EDCA_MAX_RX_ANTENNA;

	capab->edca_hw_caps = edca_hw_caps;
	os_memcpy(&capab->channels, &pr->cfg->edca_channels,
		  sizeof(struct pr_channels));
}


static void pr_get_ntb_capabilities(struct pr_data *pr,
				    struct ntb_capabilities *capab)
{
	u32 ntb_hw_caps = 0;

	os_memset(capab, 0, sizeof(struct ntb_capabilities));
	capab->ista_support = pr->cfg->ntb_ista_support;
	capab->rsta_support = pr->cfg->ntb_rsta_support;
	os_memcpy(capab->country, pr->cfg->country, 3);
	capab->secure_he_ltf = pr->cfg->secure_he_ltf;

	ntb_hw_caps |= (pr->cfg->ntb_format_and_bw & NTB_FORMAT_AND_BW_MASK) <<
		NTB_FORMAT_AND_BW;
	ntb_hw_caps |= (pr->cfg->max_tx_ltf_repetations &
			MAX_TX_LTF_REPETATIONS_MASK) << MAX_TX_LTF_REPETATIONS;
	ntb_hw_caps |= (pr->cfg->max_rx_ltf_repetations &
			MAX_RX_LTF_REPETATIONS_MASK) << MAX_RX_LTF_REPETATIONS;

	ntb_hw_caps |= (pr->cfg->max_rx_ltf_total & MAX_RX_LTF_TOTAL_MASK) <<
		MAX_RX_LTF_TOTAL;
	ntb_hw_caps |= (pr->cfg->max_tx_ltf_total & MAX_TX_LTF_TOTAL_MASK) <<
		MAX_TX_LTF_TOTAL;

	ntb_hw_caps |= (pr->cfg->max_rx_sts_le_80 & MAX_RX_STS_LE_80_MASK) <<
		MAX_RX_STS_LE_80;
	ntb_hw_caps |= (pr->cfg->max_rx_sts_gt_80 & MAX_RX_STS_GT_80_MASK) <<
		MAX_RX_STS_GT_80;

	ntb_hw_caps |= (pr->cfg->max_tx_sts_le_80 & MAX_TX_STS_LE_80_MASK) <<
		MAX_TX_STS_LE_80;
	ntb_hw_caps |= (pr->cfg->max_tx_sts_gt_80 & MAX_TX_STS_GT_80_MASK) <<
		MAX_TX_STS_GT_80;

	capab->ntb_hw_caps = ntb_hw_caps;
	os_memcpy(&capab->channels, &pr->cfg->ntb_channels,
		  sizeof(struct pr_channels));
}


static int pr_derive_dira(struct pr_data *pr, const u8 *src_addr,
			  struct pr_dira *dira)
{
	u8 nonce[DEVICE_IDENTITY_NONCE_LEN];
	u8 tag[DEVICE_MAX_HASH_LEN];
	u8 data[DIR_STR_LEN + ETH_ALEN + DEVICE_IDENTITY_NONCE_LEN];

	if (pr->cfg->dik_cipher != DIRA_CIPHER_VERSION_128) {
		wpa_printf(MSG_INFO, "PR: Unsupported DIRA Cipher version %d",
			   pr->cfg->dik_cipher);
		return -1;
	}

	if (pr->cfg->dik_len != DEVICE_IDENTITY_KEY_LEN) {
		wpa_printf(MSG_INFO, "PR: Invalid DIK length %zu",
			   pr->cfg->dik_len);
		return -1;
	}

	os_memset(data, 0, sizeof(data));

	if (os_get_random(nonce, DEVICE_IDENTITY_NONCE_LEN) < 0) {
		wpa_printf(MSG_INFO, "PR: Failed to generate DIRA nonce");
		return -1;
	}

	/* Tag = Truncate-64(HMAC-SHA-256(DevIK, "DIR" || PR Device Address ||
	 *                                Nonce))
	 */
	os_memcpy(data, "DIR", DIR_STR_LEN);
	os_memcpy(&data[DIR_STR_LEN], src_addr, ETH_ALEN);
	os_memcpy(&data[DIR_STR_LEN + ETH_ALEN], nonce,
		  DEVICE_IDENTITY_NONCE_LEN);

	if (hmac_sha256(pr->cfg->dik_data, pr->cfg->dik_len, data, sizeof(data),
			tag) < 0) {
		wpa_printf(MSG_ERROR, "PR: Could not derive DIRA tag");
		return -1;
	}

	os_memset(dira, 0, sizeof(struct pr_dira));
	dira->cipher_version = pr->cfg->dik_cipher;
	dira->nonce_len = DEVICE_IDENTITY_NONCE_LEN;
	os_memcpy(dira->nonce, nonce, DEVICE_IDENTITY_NONCE_LEN);
	dira->tag_len = DEVICE_IDENTITY_TAG_LEN;
	os_memcpy(dira->tag, tag, DEVICE_IDENTITY_TAG_LEN);

	wpa_hexdump_key(MSG_DEBUG, "PR: DIK", pr->cfg->dik_data,
			pr->cfg->dik_len);
	wpa_hexdump(MSG_DEBUG, "PR: DIRA-NONCE", dira->nonce, dira->nonce_len);
	wpa_hexdump(MSG_DEBUG, "PR: DIRA-TAG", dira->tag, dira->tag_len);

	return 0;
}


static int pr_validate_dira(struct pr_data *pr, struct pr_device *dev,
			    const u8 *dira, u16 dira_len)
{
	int ret;
	size_t len[3];
	const u8 *addr[3];
	struct pr_dev_ik *dev_ik;
	u8 tag[DEVICE_MAX_HASH_LEN];
	const char *label = "DIR";
	const u8 *dira_nonce, *dira_tag;

	/* Reset DevIK state - set only if DIRA verification succeeds */
	os_memset(dev->dik, 0, DEVICE_IDENTITY_KEY_LEN);
	dev->dik_valid = false;

	if (dira_len < 1 + DEVICE_IDENTITY_NONCE_LEN + DEVICE_IDENTITY_TAG_LEN)
	{
		wpa_printf(MSG_DEBUG, "PR: Truncated DIRA (length %u)",
			   dira_len);
		return -1;
	}

	/* Cipher Version */
	if (dira[0] != DIRA_CIPHER_VERSION_128) {
		wpa_printf(MSG_DEBUG, "PR: Unsupported DIRA cipher version %d",
			   dira[0]);
		return -1;
	}

	/* Nonce */
	dira_nonce = &dira[1];

	/* Tag */
	dira_tag = &dira[1 + DEVICE_IDENTITY_NONCE_LEN];

	/* Tag = Truncate-64(HMAC-SHA-256(DevIK, "DIR" || Device Address ||
	 *                                Nonce)) */
	addr[0] = (const u8 *) label;
	len[0] = DIR_STR_LEN;
	addr[1] = dev->pr_device_addr;
	len[1] = ETH_ALEN;
	addr[2] = dira_nonce;
	len[2] = DEVICE_IDENTITY_NONCE_LEN;

	dl_list_for_each(dev_ik, &pr->dev_iks, struct pr_dev_ik, list) {
		ret = hmac_sha256_vector(dev_ik->dik, DEVICE_IDENTITY_KEY_LEN,
					 3, addr, len, tag);
		if (ret < 0) {
			wpa_printf(MSG_INFO,
				   "PR: Failed to derive DIRA Tag");
			return -1;
		}

		if (os_memcmp(tag, dira_tag, DEVICE_IDENTITY_TAG_LEN) == 0) {
			wpa_printf(MSG_DEBUG, "PR: DIRA Tag matched");
			if (dev_ik->password_valid) {
				os_strlcpy(dev->password, dev_ik->password,
					   sizeof(dev->password));
				dev->password_valid = true;
			}
			if (dev_ik->pmk_valid) {
				os_memcpy(dev->pmk, dev_ik->pmk,
					  dev_ik->pmk_len);
				dev->pmk_len = dev_ik->pmk_len;
				dev->pmk_valid = true;
			}
			os_memcpy(dev->dik, dev_ik->dik,
				  DEVICE_IDENTITY_KEY_LEN);
			dev->dik_valid = true;
			return 0;
		}
	}

	return -1;
}


#ifdef CONFIG_PASN

static void pr_copy_channels(struct pr_channels *dst,
			     const struct pr_channels *src, bool allow_6ghz)
{
	size_t i, j;

	if (allow_6ghz) {
		os_memcpy(dst, src, sizeof(struct pr_channels));
		return;
	}

	for (i = 0, j = 0; i < src->op_classes; i++) {
		if (is_6ghz_op_class(src->op_class[i].op_class))
			continue;
		os_memcpy(&dst->op_class[j], &src->op_class[i],
			  sizeof(struct pr_op_class));
		j++;
	}
	dst->op_classes = j;
}


static void pr_op_class_intersect(const struct pr_op_class *a,
				  const struct pr_op_class *b,
				  struct pr_op_class *res)
{
	size_t i, j;

	res->op_class = a->op_class;
	for (i = 0; i < a->channels; i++) {
		for (j = 0; j < b->channels; j++) {
			if (a->channel[i] != b->channel[j])
				continue;
			res->channel[res->channels] = a->channel[i];
			res->channels++;
			if (res->channels == PR_MAX_OP_CLASS_CHANNELS)
				return;
		}
	}
}


/**
 * pr_channels_intersect - Intersection of supported channel lists
 * @a: First set of supported channels
 * @b: Second set of supported channels
 * @res: Data structure for returning the intersection of supported channels
 *
 * This function can be used to find a common set of supported channels. Both
 * input channel sets are assumed to use the global operating classes or the
 * same country code. If different country codes are used without using the
 * global operating classes, the operating class numbers may not be matched
 * correctly and results are undefined.
 */
static void pr_channels_intersect(const struct pr_channels *a,
				  const struct pr_channels *b,
				  struct pr_channels *res)
{
	size_t i, j;
	const struct pr_op_class *a_op;
	const struct pr_op_class *b_op;

	os_memset(res, 0, sizeof(*res));

	for (i = 0; i < a->op_classes; i++) {
		a_op = &a->op_class[i];
		for (j = 0; j < b->op_classes; j++) {
			b_op = &b->op_class[j];
			if (a_op->op_class != b_op->op_class)
				continue;
			pr_op_class_intersect(a_op, b_op,
					      &res->op_class[res->op_classes]);
			if (res->op_class[res->op_classes].channels) {
				res->op_classes++;
				if (res->op_classes == PR_MAX_OP_CLASSES)
					return;
			}
		}
	}
}

#endif /* CONFIG_PASN */


static void pr_buf_add_channel_list(struct wpabuf *buf, const char *country,
				    const struct pr_channels *chan)
{
	size_t i;

	wpabuf_put_data(buf, country, 3); /* Country String */
	wpabuf_put_u8(buf, chan->op_classes); /* Number of Channel Entries */

	/* Channel Entry List */
	for (i = 0; i < chan->op_classes; i++) {
		const struct pr_op_class *c = &chan->op_class[i];

		wpabuf_put_u8(buf, c->op_class);
		wpabuf_put_u8(buf, c->channels);
		wpabuf_put_data(buf, c->channel, c->channels);
	}
}


static void pr_buf_add_ranging_capa_info(struct wpabuf *buf,
					 const struct pr_capabilities *capab)
{
	u8 *len;
	u8 capa_6g = 0;
	u8 protocol_type = 0;
	size_t _len;

	/* Proximity Ranging Capability Attribute */
	wpabuf_put_u8(buf, PR_ATTR_RANGING_CAPABILITY);
	len = wpabuf_put(buf, 2); /* Attribute length to be filled */

	/* Ranging Protocol Type */
	if (capab->edca_support)
		protocol_type |= PR_EDCA_BASED_RANGING;
	if (capab->ntb_support && capab->secure_he_ltf)
		protocol_type |= PR_NTB_SECURE_LTF_BASED_RANGING;
	if (capab->ntb_support)
		protocol_type |= PR_NTB_OPEN_BASED_RANGING;
	wpabuf_put_u8(buf, protocol_type);

	/* PASN Type */
	wpabuf_put_u8(buf, capab->pasn_type);

	/* 6GHz band */
	if (capab->support_6ghz)
		capa_6g |= BIT(0);

	wpabuf_put_u8(buf, capa_6g);

	/* Device Name */
	wpabuf_put_data(buf, capab->device_name, WPS_DEV_NAME_MAX_LEN);
	wpa_printf(MSG_DEBUG, "PR: Device name: %s", capab->device_name);

	_len = (u8 *) wpabuf_put(buf, 0) - len - 2;
	WPA_PUT_LE16(len, _len);
	wpa_hexdump(MSG_DEBUG, "PR: * Capability Attribute", len + 2, _len);
}


static void pr_buf_add_edca_capa_info(struct wpabuf *buf,
				      const struct edca_capabilities *edca_data)
{
	u8 *len;
	u8 ranging_role = 0;
	size_t _len;

	/* Proximity Ranging EDCA Capability Attribute */
	wpabuf_put_u8(buf, PR_ATTR_EDCA_CAPABILITY);
	len = wpabuf_put(buf, 2); /* Attribute length to be filled */

	/* Ranging Role */
	if (edca_data->ista_support)
		ranging_role |= PR_ISTA_SUPPORT;
	if (edca_data->rsta_support)
		ranging_role |= PR_RSTA_SUPPORT;
	wpabuf_put_u8(buf, ranging_role);

	/* Ranging Parameters */
	wpabuf_put_le16(buf, edca_data->edca_hw_caps);

	pr_buf_add_channel_list(buf, edca_data->country, &edca_data->channels);

	_len = (u8 *) wpabuf_put(buf, 0) - len - 2;
	WPA_PUT_LE16(len, _len);
	wpa_hexdump(MSG_DEBUG, "PR: * EDCA Capability Attribute",
		    len + 2, _len);
}


static void pr_buf_add_ntb_capa_info(struct wpabuf *buf,
				     const struct ntb_capabilities *ntb_data)
{
	u8 *len;
	u8 ranging_role = 0;
	size_t _len;

	/* Proximity Ranging 11az NTB Capability Attribute */
	wpabuf_put_u8(buf, PR_ATTR_NTB_CAPABILITY);
	len = wpabuf_put(buf, 2);

	/* Ranging Role */
	if (ntb_data->ista_support)
		ranging_role |= PR_ISTA_SUPPORT;
	if (ntb_data->rsta_support)
		ranging_role |= PR_RSTA_SUPPORT;
	wpabuf_put_u8(buf, ranging_role);

	/* Ranging Parameter */
	wpabuf_put_le32(buf, ntb_data->ntb_hw_caps);

	pr_buf_add_channel_list(buf, ntb_data->country, &ntb_data->channels);

	_len = (u8 *) wpabuf_put(buf, 0) - len - 2;
	WPA_PUT_LE16(len, _len);
	wpa_hexdump(MSG_DEBUG, "PR: * NTB Capability Attribute", len + 2, _len);
}


static void pr_buf_add_dira(struct wpabuf *buf, const struct pr_dira *dira)
{
	u8 *len;
	size_t _len;

	/* Proximity Ranging Device Identity Resolution attribute */
	wpabuf_put_u8(buf, PR_ATTR_DEVICE_IDENTITY_RESOLUTION);

	/* Length to be filled */
	len = wpabuf_put(buf, 2);

	wpabuf_put_u8(buf, dira->cipher_version);
	wpabuf_put_data(buf, dira->nonce, dira->nonce_len);
	wpabuf_put_data(buf, dira->tag, dira->tag_len);

	/* Update attribute length */
	_len = (u8 *) wpabuf_put(buf, 0) - len - 2;
	WPA_PUT_LE16(len, _len);

	wpa_printf(MSG_DEBUG, "PR: * DIRA");
}


struct wpabuf * pr_prepare_usd_elems(struct pr_data *pr, const u8 *src_addr)
{
	u32 ie_type;
	struct wpabuf *buf, *buf2;
	struct pr_capabilities pr_caps;
	struct pr_dira dira;

	buf = wpabuf_alloc(1000);
	if (!buf)
		return NULL;

	pr_get_ranging_capabilities(pr, &pr_caps);
	pr_buf_add_ranging_capa_info(buf, &pr_caps);

	if (pr->cfg->edca_ista_support || pr->cfg->edca_rsta_support) {
		struct edca_capabilities edca_caps;

		pr_get_edca_capabilities(pr, &edca_caps);
		pr_buf_add_edca_capa_info(buf, &edca_caps);
	}

	if (pr->cfg->ntb_ista_support || pr->cfg->ntb_rsta_support) {
		struct ntb_capabilities ntb_caps;

		pr_get_ntb_capabilities(pr, &ntb_caps);
		pr_buf_add_ntb_capa_info(buf, &ntb_caps);
	}

	if (!pr_derive_dira(pr, src_addr, &dira))
		pr_buf_add_dira(buf, &dira);

	ie_type = (OUI_WFA << 8) | PR_OUI_TYPE;
	buf2 = pr_encaps_elem(buf, ie_type);
	wpabuf_free(buf);

	return buf2;
}


static int pr_parse_attribute(u8 id, const u8 *data, u16 len,
			      struct pr_message *msg)
{
	switch (id) {
	case PR_ATTR_STATUS:
		if (len < 1) {
			wpa_printf(MSG_INFO,
				   "PR: Invalid Proximity Ranging Status Attribute (length %d)",
				   len);
			return -1;
		}
		msg->status_ie = data;
		msg->status_ie_len = len;
		wpa_printf(MSG_DEBUG, "PR: Status Code %u", data[0]);
		break;
	case PR_ATTR_RANGING_CAPABILITY:
		if (len < 35) {
			wpa_printf(MSG_INFO,
				   "PR: Too short Proximity Ranging Capability Attribute (length %d)",
				   len);
			return -1;
		}
		msg->pr_capability = data;
		msg->pr_capability_len = len;
		wpa_printf(MSG_DEBUG,
			   "PR: Ranging Protocol Type: %02x PASN Type: %02x",
			   data[0], data[1]);
		break;
	case PR_ATTR_EDCA_CAPABILITY:
		if (len < 10) {
			wpa_printf(MSG_INFO,
				   "PR: Too short Proximity Ranging EDCA Capability Attribute (length %d)",
				   len);
			return -1;
		}
		msg->edca_capability = data;
		msg->edca_capability_len = len;
		wpa_printf(MSG_DEBUG,
			   "PR: EDCA Ranging Role %02x, Ranging Parameters %04x",
			   data[0], WPA_GET_LE16(data + 1));
		break;
	case PR_ATTR_NTB_CAPABILITY:
		if (len < 12) {
			wpa_printf(MSG_INFO,
				   "PR: Too short Proximity Ranging 11az NTB Capability Attribute (length %d)",
				   len);
			return -1;
		}
		msg->ntb_capability = data;
		msg->ntb_capability_len = len;
		wpa_printf(MSG_DEBUG,
			   "PR: NTB Ranging Role %02x, Ranging Parameter %04x",
			   data[0], WPA_GET_LE32(data + 1));
		break;
	case PR_ATTR_OPERATION_MODE:
		if (len < 9) {
			wpa_printf(MSG_INFO,
				   "PR: Invalid Proximity Ranging Operation Mode Attribute (length %d)",
				   len);
			return -1;
		}
		msg->op_mode = data;
		msg->op_mode_len = len;
		break;
	case PR_ATTR_DEVICE_IDENTITY_RESOLUTION:
		if (len < 1 + DEVICE_IDENTITY_NONCE_LEN +
		    DEVICE_IDENTITY_TAG_LEN) {
			wpa_printf(MSG_INFO, "PR: Too short DIRA (length %d)",
				   len);
			return -1;
		}
		msg->dira = data;
		msg->dira_len = len;
		wpa_printf(MSG_DEBUG, "PR: DIRA cipher version %u", data[0]);
		break;
	default:
		wpa_printf(MSG_DEBUG,
			   "PR: Skipped unknown attribute %d (length %d)",
			   id, len);
		break;
	}

	return 0;
}


/**
 * pr_parse_proximity_ranging_element - Parse Proximity Ranging element
 * @buf: Concatenated PR element(s) payload
 * @msg: Buffer for returning parsed attributes
 * Returns: 0 on success, -1 on failure
 *
 * Note: The caller is responsible for clearing the msg data structure before
 * calling this function.
 */
static int pr_parse_proximity_ranging_element(const struct wpabuf *buf,
					      struct pr_message *msg)
{
	const u8 *pos = wpabuf_head_u8(buf);
	const u8 *end = pos + wpabuf_len(buf);

	wpa_printf(MSG_DEBUG, "PR: Parsing Proximity Ranging element");

	while (pos < end) {
		u16 attr_len;
		u8 id;

		if (end - pos < 3) {
			wpa_printf(MSG_DEBUG, "PR: Invalid PR attribute");
			return -1;
		}
		id = *pos++;
		attr_len = WPA_GET_LE16(pos);
		pos += 2;
		wpa_printf(MSG_DEBUG, "PR: Attribute %d length %u",
			   id, attr_len);
		if (attr_len > end - pos) {
			wpa_printf(MSG_DEBUG,
				   "PR: Attribute underflow (len=%u left=%d)",
				   attr_len, (int) (end - pos));
			wpa_hexdump(MSG_MSGDUMP, "PR: Data", pos, end - pos);
			return -1;
		}
		if (pr_parse_attribute(id, pos, attr_len, msg))
			return -1;
		pos += attr_len;
	}

	return 0;
}


static void pr_parse_free(struct pr_message *msg)
{
	wpabuf_free(msg->pr_attributes);
	msg->pr_attributes = NULL;
}


/**
 * pr_parse_elements - Parse Proximity Ranging element(s)
 * @data: Elements from the message
 * @len: Length of data buffer in octets
 * @msg: Buffer for returning parsed attributes
 * Returns: 0 on success, -1 on failure
 *
 * Note: The caller is responsible for clearing the msg data structure before
 * calling this function.
 *
 * Note: The caller must free temporary memory allocations by calling
 * pr_parse_free() when the parsed data is not needed anymore.
 */
static int pr_parse_elements(const u8 *data, size_t len, struct pr_message *msg)
{
	struct ieee802_11_elems elems;

	if (ieee802_11_parse_elems(data, len, &elems, true) == ParseFailed)
		return -1;

	msg->pr_attributes = ieee802_11_vendor_ie_concat(data, len,
							 PR_IE_VENDOR_TYPE);
	if (msg->pr_attributes &&
	    pr_parse_proximity_ranging_element(msg->pr_attributes, msg)) {
		wpa_printf(MSG_INFO,
			   "PR: Failed to parse Proximity Ranging element data");
		if (msg->pr_attributes)
			wpa_hexdump_buf(MSG_MSGDUMP,
					"PR: Proximity Ranging element payload",
					msg->pr_attributes);
		pr_parse_free(msg);
		return -1;
	}
	return 0;
}


static int pr_process_channels(const u8 *channel_list, size_t channel_list_len,
			       struct pr_channels *ch)
{
	u8 channels;
	const u8 *pos, *end;
	u8 op_class_count;

	if (channel_list_len < 1)
		return -1;

	pos = channel_list;
	end = channel_list + channel_list_len;

	/* Number of Channel Entries */
	/* Get total count of the operational classes */
	op_class_count = pos[0];
	wpa_printf(MSG_DEBUG, "PR: Total operational classes: %u",
		   op_class_count);
	pos++;

	/* Channel Entry List */
	ch->op_classes = 0;
	while (end - pos > 2 && (ch->op_classes <= op_class_count)) {
		struct pr_op_class *cl = &ch->op_class[ch->op_classes];

		cl->op_class = *pos++; /* Operating Class */
		channels = *pos++; /* Number of Channels */

		/* Channel List */
		if (channels > end - pos) {
			wpa_printf(MSG_INFO,
				   "PR: Invalid channel list channel %d, size: %ld",
				   channels, end - pos);
			return -1;
		}
		cl->channels = channels > PR_MAX_OP_CLASS_CHANNELS ?
			PR_MAX_OP_CLASS_CHANNELS : channels;
		os_memcpy(cl->channel, pos, cl->channels);
		pos += channels;
		ch->op_classes++;
		if (ch->op_classes == PR_MAX_OP_CLASSES)
			break;
	}

	if (ch->op_classes != op_class_count &&
	    ch->op_classes < PR_MAX_OP_CLASSES) {
		wpa_printf(MSG_INFO,
			   "PR: Channel list count mismatch %lu != %d",
			   ch->op_classes, op_class_count);
		return -1;
	}

	return 0;
}


static void pr_process_ranging_capabilities(const u8 *caps, size_t caps_len,
					    struct pr_capabilities *pr_caps)
{
	const u8 *pos;

	if (!caps || caps_len < 3 + WPS_DEV_NAME_MAX_LEN) {
		wpa_printf(MSG_INFO,
			   "PR: Invalid Proximity Ranging Capability Attribute");
		return;
	}

	pos = caps;

	/* Ranging Protocol Type */
	if (*pos & PR_EDCA_BASED_RANGING)
		pr_caps->edca_support = true;
	if (*pos & PR_NTB_SECURE_LTF_BASED_RANGING) {
		pr_caps->secure_he_ltf = true;
		pr_caps->ntb_support = true;
	}
	if (*pos & PR_NTB_OPEN_BASED_RANGING)
		pr_caps->ntb_support = true;

	pos++;
	/* PASN Type */
	pr_caps->pasn_type = *pos;

	pos++;
	/* 6GHz band */
	pr_caps->support_6ghz = *pos & BIT(0);

	pos++;
	/* Device Name */
	os_memset(pr_caps->device_name, 0, WPS_DEV_NAME_MAX_LEN + 1);
	os_memcpy(pr_caps->device_name, pos, WPS_DEV_NAME_MAX_LEN);

	wpa_printf(MSG_DEBUG,
		   "PR: Device name=%s, edca capability=%x, ntb capability=%x, secure LTF capability=%u, 6GHz=%u",
		   pr_caps->device_name, pr_caps->edca_support,
		   pr_caps->ntb_support, pr_caps->secure_he_ltf,
		   pr_caps->support_6ghz);
}


static void pr_process_edca_capabilities(const u8 *caps, size_t caps_len,
					 struct edca_capabilities *edca_caps)
{
	const u8 *pos, *end;

	if (caps_len < 7)
		return;

	pos = caps;
	end = caps + caps_len;

	/* Ranging Role */
	if (*pos & PR_ISTA_SUPPORT)
		edca_caps->ista_support = true;
	if (*pos & PR_RSTA_SUPPORT)
		edca_caps->rsta_support = true;
	pos++;

	/* Ranging Parameters */
	edca_caps->edca_hw_caps = WPA_GET_LE16(pos);
	pos += 2;

	/* Country String */
	os_memcpy(edca_caps->country, pos, 3);
	pos += 3;

	pr_process_channels(pos, end - pos, &edca_caps->channels);

	wpa_printf(MSG_DEBUG,
		   "PR: EDCA ISTA support=%u, EDCA RSTA support=%u, op classes count=%lu, country=%c%c",
		   edca_caps->ista_support, edca_caps->rsta_support,
		   edca_caps->channels.op_classes,
		   valid_country_ch(edca_caps->country[0]) ?
		   edca_caps->country[0] : '_',
		   valid_country_ch(edca_caps->country[1]) ?
		   edca_caps->country[1] : '_');
}


static void pr_process_ntb_capabilities(const u8 *caps, size_t caps_len,
					struct ntb_capabilities *ntb_caps,
					bool secure_ltf)
{
	const u8 *pos, *end;

	if (caps_len < 9)
		return;

	pos = caps;
	end = caps + caps_len;

	/* Ranging Role */
	if (*pos & PR_ISTA_SUPPORT)
		ntb_caps->ista_support = true;
	if (*pos & PR_RSTA_SUPPORT)
		ntb_caps->rsta_support = true;
	if (secure_ltf)
		ntb_caps->secure_he_ltf = true;
	pos++;

	/* Ranging Parameter */
	ntb_caps->ntb_hw_caps = WPA_GET_LE32(pos);
	pos += 4;

	/* Country String */
	os_memcpy(ntb_caps->country, pos, 3);
	pos += 3;

	pr_process_channels(pos, end - pos, &ntb_caps->channels);

	wpa_printf(MSG_DEBUG,
		   "PR: NTB ISTA support=%u, NTB RSTA support=%u, op classes count=%lu, secure HE-LTF=%u, country=%c%c",
		   ntb_caps->ista_support, ntb_caps->rsta_support,
		   ntb_caps->channels.op_classes,
		   ntb_caps->secure_he_ltf,
		   valid_country_ch(ntb_caps->country[0]) ?
		   ntb_caps->country[0] : '_',
		   valid_country_ch(ntb_caps->country[1]) ?
		   ntb_caps->country[1] : '_');
}


void pr_process_usd_elems(struct pr_data *pr, const u8 *ies, u16 ies_len,
			  const u8 *peer_addr, unsigned int freq)
{
	struct pr_device *dev;
	struct pr_message msg;

	os_memset(&msg, 0, sizeof(msg));
	os_memcpy(msg.pr_device_addr, peer_addr, ETH_ALEN);

	if (pr_parse_elements(ies, ies_len, &msg)) {
		wpa_printf(MSG_INFO,
			   "PR: Failed to parse Proximity Ranging element(s)");
		pr_parse_free(&msg);
		return;
	}

	if (!msg.pr_capability) {
		wpa_printf(MSG_DEBUG,
			   "PR: Ranging caps not present, ignoring proximity device "
			   MACSTR, MAC2STR(peer_addr));
		pr_parse_free(&msg);
		return;
	}

	if (!msg.edca_capability && !msg.ntb_capability) {
		wpa_printf(MSG_DEBUG,
			   "PR: Neither EDCA nor NTB capabilities are present, ignoring proximity device "
			   MACSTR, MAC2STR(peer_addr));
		pr_parse_free(&msg);
		return;
	}

	dev = pr_create_device(pr, peer_addr);
	if (!dev) {
		pr_parse_free(&msg);
		wpa_printf(MSG_INFO, "PR: Failed to create a device");
		return;
	}

	os_get_reltime(&dev->last_seen);
	dev->listen_freq = freq;
	dev->discovery_type = PR_DISCOVERY_TYPE_USD;

	pr_process_ranging_capabilities(msg.pr_capability,
					msg.pr_capability_len, &dev->pr_caps);

	if (dev->pr_caps.edca_support && msg.edca_capability)
		pr_process_edca_capabilities(msg.edca_capability,
					     msg.edca_capability_len,
					     &dev->edca_caps);

	if (dev->pr_caps.ntb_support && msg.ntb_capability)
		pr_process_ntb_capabilities(msg.ntb_capability,
					    msg.ntb_capability_len,
					    &dev->ntb_caps,
					    dev->pr_caps.secure_he_ltf);

	if (msg.dira && msg.dira_len)
		pr_validate_dira(pr, dev, msg.dira, msg.dira_len);

	if (pr->cfg->device_found)
		pr->cfg->device_found(pr->cfg->cb_ctx, dev);

	pr_parse_free(&msg);
}


/**
 * pr_ensure_oob_peer - Add a minimal OOB peer entry if not already present
 */
int pr_ensure_oob_peer(struct pr_data *pr, const u8 *addr, int freq)
{
	struct pr_device *dev;

	if (!pr || !addr)
		return -1;

	dev = pr_get_device(pr, addr);
	if (dev)
		return 0;

	dev = pr_create_device(pr, addr);
	if (!dev) {
		wpa_printf(MSG_INFO, "PR: Failed to create OOB peer " MACSTR,
			   MAC2STR(addr));
		return -1;
	}

	dev->discovery_type = PR_DISCOVERY_TYPE_OOB;
	if (freq)
		dev->listen_freq = freq;

	wpa_printf(MSG_DEBUG, "PR: OOB peer " MACSTR " created at PASN_START",
		   MAC2STR(addr));
	return 0;
}


#ifdef CONFIG_PASN

static bool pr_eq_ranging_capa_params(const struct pr_device *dev,
				     const struct pr_capabilities *caps)
{
	if (dev->discovery_type == PR_DISCOVERY_TYPE_OOB)
		return true;

	return dev->pr_caps.edca_support == caps->edca_support &&
		dev->pr_caps.ntb_support == caps->ntb_support &&
		dev->pr_caps.pasn_type == caps->pasn_type &&
		dev->pr_caps.secure_he_ltf == caps->secure_he_ltf &&
		dev->pr_caps.support_6ghz == caps->support_6ghz &&
		os_strcmp(dev->pr_caps.device_name, caps->device_name) == 0;
}


static bool pr_eq_edca_params(const struct pr_device *dev,
			      const struct edca_capabilities *edca_caps)
{
	if (dev->discovery_type == PR_DISCOVERY_TYPE_OOB)
		return true;

	return dev->edca_caps.ista_support == edca_caps->ista_support &&
		dev->edca_caps.rsta_support == edca_caps->rsta_support &&
		dev->edca_caps.edca_hw_caps == edca_caps->edca_hw_caps &&
		os_memcmp(dev->edca_caps.country, edca_caps->country, 3) == 0;
}


static bool pr_eq_ntb_params(const struct pr_device *dev,
			     const struct ntb_capabilities *ntb_caps)
{
	if (dev->discovery_type == PR_DISCOVERY_TYPE_OOB)
		return true;

	return dev->ntb_caps.ista_support == ntb_caps->ista_support &&
		dev->ntb_caps.rsta_support == ntb_caps->rsta_support &&
		dev->ntb_caps.ntb_hw_caps == ntb_caps->ntb_hw_caps &&
		os_memcmp(dev->ntb_caps.country, ntb_caps->country, 3) == 0;
}


static void pr_buf_add_operation_mode(struct wpabuf *buf,
				      struct operation_mode *mode)
{
	u8 *len;
	size_t _len;

	/* Proximity Ranging Operation Mode Attribute */
	wpabuf_put_u8(buf, PR_ATTR_OPERATION_MODE);
	/* Length to be filled */
	len = wpabuf_put(buf, 2);

	/* Ranging Protocol Type */
	wpabuf_put_u8(buf, mode->protocol_type);

	/* Ranging Role */
	wpabuf_put_u8(buf, mode->role);

	pr_buf_add_channel_list(buf, mode->country, &mode->channels);

	/* Update attribute length */
	_len = (u8 *) wpabuf_put(buf, 0) - len - 2;
	WPA_PUT_LE16(len, _len);
	wpa_hexdump(MSG_DEBUG, "PR: * Operation Mode", len + 2, _len);
}


static void pr_buf_add_ranging_neg_status(struct wpabuf *buf, u8 status)
{
	/* Proximity Ranging Status Attribute */
	wpabuf_put_u8(buf, PR_ATTR_STATUS);
	wpabuf_put_le16(buf, 1);
	wpabuf_put_u8(buf, status);
	wpa_printf(MSG_DEBUG, "PR: * Role negotiation status %u", status);
}


static void pr_process_op_mode(const u8 *caps, size_t caps_len,
			       struct operation_mode *op_mode)
{
	const u8 *pos, *end;

	if (!caps || caps_len < 6)
		return;

	pos = caps;
	end = caps + caps_len;

	/* Ranging Protocol Type */
	op_mode->protocol_type = *pos;
	pos++;

	/* Ranging Role */
	op_mode->role = *pos;
	pos++;

	/* Country String */
	os_memcpy(op_mode->country, pos, 3);
	pos += 3;

	pr_process_channels(pos, end - pos, &op_mode->channels);

	wpa_printf(MSG_DEBUG,
		   "PR: protocol type=%u, role type=%u, country=%c%c, channel size=%lu",
		   op_mode->protocol_type, op_mode->role,
		   valid_country_ch(op_mode->country[0]) ?
		   op_mode->country[0] : '_',
		   valid_country_ch(op_mode->country[1]) ?
		   op_mode->country[1] : '_',
		   op_mode->channels.op_classes);
}


static void pr_choose_best_channel(struct pr_channels *common_channel,
				   u8 *op_class, u8 *op_channel)
{
	int bw;
	int max_bw = 0;
	const struct oper_class_map *map;
	size_t i;

	if (!common_channel || !common_channel->op_classes ||
	    !common_channel->op_class[0].channels) {
		wpa_printf(MSG_INFO,
			   "PR: Empty channel list, cannot get best channel");
		*op_class = 0;
		*op_channel = 0;
		return;
	}

	for (i = 0; i < common_channel->op_classes; i++) {
		map = get_oper_class(NULL,
				     common_channel->op_class[i].op_class);
		if (!map)
			continue;
		bw = oper_class_bw_to_int(map);
		if (bw > max_bw) {
			*op_class = common_channel->op_class[i].op_class;
			*op_channel = common_channel->op_class[i].channel[0];
			max_bw = bw;
		}
	}

	wpa_printf(MSG_DEBUG,
		   "PR: Choose the operating class: %u, operating channel: %u",
		   *op_class, *op_channel);
}


static u8 pr_pasn_get_best_op_mode(struct pr_data *pr, u8 peer_supp_roles,
				   struct operation_mode *op_mode,
				   struct operation_mode *res_op_mode)
{
	u8 ranging_type = 0;
	struct pr_channels common_chan;
	struct pr_channels *own_channels = NULL;
	int status = PR_NEGOTIATION_FAIL;
	u8 op_class = 0, op_channel = 0;
	bool own_ista_support = false, own_rsta_support = false;

	if (op_mode->protocol_type &
	    (PR_NTB_SECURE_LTF_BASED_RANGING | PR_NTB_OPEN_BASED_RANGING)) {
		if (op_mode->protocol_type == PR_NTB_SECURE_LTF_BASED_RANGING &&
		    !pr->cfg->secure_he_ltf) {
			wpa_printf(MSG_INFO,
				   "PR PASN: Secure HE-LTF not supported");
			return PR_NEGOTIATION_FAIL;
		}

		if ((op_mode->protocol_type &
		     PR_NTB_SECURE_LTF_BASED_RANGING) && pr->cfg->secure_he_ltf)
			ranging_type = PR_NTB_SECURE_LTF_BASED_RANGING;
		else
			ranging_type = PR_NTB_OPEN_BASED_RANGING;

		own_ista_support = pr->cfg->ntb_ista_support;
		own_rsta_support = pr->cfg->ntb_rsta_support;
		own_channels = &pr->cfg->ntb_channels;
		wpa_printf(MSG_DEBUG, "PR PASN: Choose NTB Ranging Protocol");
	} else if (op_mode->protocol_type & PR_EDCA_BASED_RANGING) {
		ranging_type = PR_EDCA_BASED_RANGING;

		own_ista_support = pr->cfg->edca_ista_support;
		own_rsta_support = pr->cfg->edca_rsta_support;
		own_channels = &pr->cfg->edca_channels;
		wpa_printf(MSG_DEBUG, "PR PASN: Choose EDCA Ranging Protocol");
	} else {
		wpa_printf(MSG_DEBUG,
			   "PR PASN: Invalid Ranging Protocol, proposed type 0x%x",
			   op_mode->protocol_type);
		return PR_NEGOTIATION_FAIL;
	}

	if (!own_ista_support && !own_rsta_support) {
		wpa_printf(MSG_DEBUG,
			   "PR PASN: Device can't fulfill any requested Ranging Role");
		return PR_NEGOTIATION_FAIL;
	}

	if (own_channels == NULL || !own_channels->op_classes) {
		wpa_printf(MSG_DEBUG,
			   "PR PASN: Invalid or empty channel list to negotiate");
		return PR_NEGOTIATION_FAIL;
	}

	pr_channels_intersect(own_channels, &op_mode->channels, &common_chan);
	if (!common_chan.op_classes) {
		wpa_printf(MSG_DEBUG,
			   "PR PASN: No common channels to perform ranging");
		return PR_NEGOTIATION_FAIL;
	}

	if (op_mode->role == PR_ISTA_SUPPORT && !own_rsta_support &&
	    !(peer_supp_roles & PR_RSTA_SUPPORT)) {
		wpa_printf(MSG_DEBUG,
			   "PR PASN: Device cannot act as RSTA and cannot update role");
		return PR_NEGOTIATION_FAIL;
	}

	if (op_mode->role == PR_RSTA_SUPPORT && !own_ista_support &&
	    !(peer_supp_roles & PR_ISTA_SUPPORT)) {
		wpa_printf(MSG_DEBUG,
			   "PR PASN: Device cannot act as ISTA and cannot update role");
		return PR_NEGOTIATION_FAIL;
	}

	if (op_mode->role == PR_ISTA_SUPPORT && own_rsta_support) {
		res_op_mode->role = PR_RSTA_SUPPORT;
		status = PR_NEGOTIATION_SUCCESS;
	} else if (op_mode->role == PR_RSTA_SUPPORT && own_ista_support) {
		res_op_mode->role = PR_ISTA_SUPPORT;
		status = PR_NEGOTIATION_SUCCESS;
	} else if ((op_mode->role == PR_ISTA_SUPPORT && !own_rsta_support) &&
		   ((peer_supp_roles & PR_RSTA_SUPPORT) && own_ista_support)) {
		res_op_mode->role = PR_ISTA_SUPPORT;
		status = PR_NEGOTIATION_UPDATE;
	} else if ((op_mode->role == PR_RSTA_SUPPORT && !own_ista_support) &&
		   ((peer_supp_roles & PR_ISTA_SUPPORT) && own_rsta_support)) {
		res_op_mode->role = PR_RSTA_SUPPORT;
		status = PR_NEGOTIATION_UPDATE;
	} else if (op_mode->role == (PR_RSTA_SUPPORT | PR_ISTA_SUPPORT)) {
		if ((pr->cfg->preferred_ranging_role || !own_ista_support) &&
		    own_rsta_support) {
			res_op_mode->role = PR_RSTA_SUPPORT;
			status = PR_NEGOTIATION_SUCCESS;
			wpa_printf(MSG_DEBUG, "PR PASN: Choose RSTA role");
		} else if ((!pr->cfg->preferred_ranging_role ||
			    !own_rsta_support) && own_ista_support) {
			res_op_mode->role = PR_ISTA_SUPPORT;
			status = PR_NEGOTIATION_SUCCESS;
			wpa_printf(MSG_DEBUG, "PR PASN: Choose ISTA role");
		}
	} else {
		wpa_printf(MSG_DEBUG, "PR PASN: Failed to choose device role");
		return PR_NEGOTIATION_FAIL;
	}

	res_op_mode->protocol_type = ranging_type;
	os_memcpy(res_op_mode->country, pr->cfg->country, 3);

	if (res_op_mode->role == PR_RSTA_SUPPORT) {
		pr_copy_channels(&res_op_mode->channels, &common_chan,
				 pr->cfg->support_6ghz);
	} else {
		pr_choose_best_channel(&common_chan, &op_class, &op_channel);
		if (!op_class || !op_channel) {
			wpa_printf(MSG_DEBUG,
				   "PR: Couldn't choose a common channel for ranging in ISTA role");
			return PR_NEGOTIATION_FAIL;
		}
		res_op_mode->channels.op_classes = 1;
		res_op_mode->channels.op_class[0].channels = 1;
		res_op_mode->channels.op_class[0].channel[0] = op_channel;
		res_op_mode->channels.op_class[0].op_class = op_class;
		wpa_printf(MSG_DEBUG,
			   "PR: Choose operating class %u, channel %u",
			   op_class, op_channel);
	}

	wpa_printf(MSG_DEBUG,
		   "PR PASN: Ranging Protocol=%u, Role=%u, country=%c%c, status=%d, op class size=%lu",
		   res_op_mode->protocol_type, res_op_mode->role,
		   valid_country_ch(res_op_mode->country[0]) ?
		   res_op_mode->country[0] : '_',
		   valid_country_ch(res_op_mode->country[1]) ?
		   res_op_mode->country[1] : '_',
		   status, res_op_mode->channels.op_classes);

	return status;
}


static u8 pr_pasn_get_final_op_mode(struct pr_data *pr, u8 supp_roles,
				    struct operation_mode *op_mode,
				    struct operation_mode *res_op_mode)
{
	u8 ranging_type = 0;
	struct pr_channels common_chan;
	struct pr_channels *own_channels = NULL;
	int status = PR_NEGOTIATION_FAIL;
	u8 op_class = 0, op_channel = 0;
	bool own_ista_support = false, own_rsta_support = false;

	if (op_mode->protocol_type &
	    (PR_NTB_SECURE_LTF_BASED_RANGING | PR_NTB_OPEN_BASED_RANGING)) {
		if (op_mode->protocol_type == PR_NTB_SECURE_LTF_BASED_RANGING &&
		    !pr->cfg->secure_he_ltf) {
			wpa_printf(MSG_INFO,
				   "PR PASN: Secure HE-LTF not supported");
			return PR_NEGOTIATION_FAIL;
		}

		if ((op_mode->protocol_type &
		     PR_NTB_SECURE_LTF_BASED_RANGING) && pr->cfg->secure_he_ltf)
			ranging_type = PR_NTB_SECURE_LTF_BASED_RANGING;
		else
			ranging_type = PR_NTB_OPEN_BASED_RANGING;

		own_ista_support = pr->cfg->ntb_ista_support;
		own_rsta_support = pr->cfg->ntb_rsta_support;
		own_channels = &pr->cfg->ntb_channels;
		wpa_printf(MSG_DEBUG, "PR PASN: Choose NTB Ranging Protocol");
	} else if (op_mode->protocol_type & PR_EDCA_BASED_RANGING) {
		ranging_type = PR_EDCA_BASED_RANGING;

		own_ista_support = pr->cfg->edca_ista_support;
		own_rsta_support = pr->cfg->edca_rsta_support;
		own_channels = &pr->cfg->edca_channels;
		wpa_printf(MSG_DEBUG, "PR PASN: Choose EDCA Ranging Protocol");
	} else {
		wpa_printf(MSG_INFO,
			   "PR PASN: Invalid Ranging Protocol, proposed type 0x%x",
			   op_mode->protocol_type);
		return PR_NEGOTIATION_FAIL;
	}

	if (op_mode->role == PR_ISTA_SUPPORT && !own_rsta_support) {
		wpa_printf(MSG_INFO, "PR: Device cannot act as RSTA");
		return PR_NEGOTIATION_FAIL;
	}

	if (op_mode->role == PR_RSTA_SUPPORT && !own_ista_support) {
		wpa_printf(MSG_INFO, "PR: Device cannot act as ISTA");
		return PR_NEGOTIATION_FAIL;
	}

	if (op_mode->role == PR_ISTA_SUPPORT) {
		res_op_mode->role = PR_RSTA_SUPPORT;
		status = PR_NEGOTIATION_SUCCESS;
	} else if (op_mode->role == PR_RSTA_SUPPORT) {
		res_op_mode->role = PR_ISTA_SUPPORT;
		status = PR_NEGOTIATION_SUCCESS;
	} else {
		wpa_printf(MSG_INFO, "PR: Invalid Ranging Role proposed");
		return PR_NEGOTIATION_FAIL;
	}

	pr_channels_intersect(own_channels, &op_mode->channels, &common_chan);
	if (!common_chan.op_classes) {
		wpa_printf(MSG_INFO,
			   "PR: No common channels to perform ranging");
		return PR_NEGOTIATION_FAIL;
	}

	pr_choose_best_channel(&common_chan, &op_class, &op_channel);
	if (!op_class || !op_channel) {
		wpa_printf(MSG_INFO,
			   "PR: Couldn't choose a common channel for ranging");
		return PR_NEGOTIATION_FAIL;
	}

	res_op_mode->protocol_type = ranging_type;
	os_memcpy(res_op_mode->country, pr->cfg->country, 3);
	res_op_mode->channels.op_classes = 1;
	res_op_mode->channels.op_class[0].channels = 1;
	res_op_mode->channels.op_class[0].channel[0] = op_channel;
	res_op_mode->channels.op_class[0].op_class = op_class;
	wpa_printf(MSG_DEBUG, "PR: Choose operating class %u, channel %u",
		   op_class, op_channel);

	return status;
}


static int pr_prepare_pasn_pr_elem(struct pr_data *pr, struct wpabuf *extra_ies,
				   bool add_dira, u8 ranging_role,
				   u8 ranging_type, int forced_pr_freq)
{
	u32 ie_type;
	struct wpabuf *buf, *buf2;
	struct pr_dira dira;
	struct pr_capabilities pr_caps;
	struct edca_capabilities edca_caps;
	struct ntb_capabilities ntb_caps;
	struct operation_mode op_mode;
	struct pr_channels op_channels;
	u8 forced_op_class = 0, forced_op_channel = 0;
	enum hostapd_hw_mode hw_mode;

	buf = wpabuf_alloc(1000);
	if (!buf)
		return -1;

	pr_get_ranging_capabilities(pr, &pr_caps);
	pr_buf_add_ranging_capa_info(buf, &pr_caps);

	if (ranging_type & PR_EDCA_BASED_RANGING) {
		pr_get_edca_capabilities(pr, &edca_caps);
		pr_buf_add_edca_capa_info(buf, &edca_caps);
		pr_copy_channels(&op_channels, &edca_caps.channels,
				 pr->cfg->support_6ghz);
	} else if (ranging_type & PR_NTB_OPEN_BASED_RANGING ||
		   ranging_type & PR_NTB_SECURE_LTF_BASED_RANGING) {
		pr_get_ntb_capabilities(pr, &ntb_caps);
		pr_buf_add_ntb_capa_info(buf, &ntb_caps);
		pr_copy_channels(&op_channels, &ntb_caps.channels,
				 pr->cfg->support_6ghz);
	} else {
		wpa_printf(MSG_INFO, "PR: Unsupported ranging_type 0x%x",
			   ranging_type);
		wpabuf_free(buf);
		return -1;
	}

	os_memset(&op_mode, 0, sizeof(struct operation_mode));
	op_mode.role = ranging_role;
	op_mode.protocol_type = ranging_type;
	os_memcpy(op_mode.country, pr->cfg->country, 3);

	if (forced_pr_freq) {
		hw_mode = ieee80211_freq_to_channel_ext(forced_pr_freq, 0, 0,
							&forced_op_class,
							&forced_op_channel);
		if (hw_mode == NUM_HOSTAPD_MODES) {
			wpa_printf(MSG_INFO, "PR: Invalid forced_pr_freq");
			wpabuf_free(buf);
			return -1;
		}

		op_mode.channels.op_classes = 1;
		op_mode.channels.op_class[0].channels = 1;
		op_mode.channels.op_class[0].channel[0] = forced_op_channel;
		op_mode.channels.op_class[0].op_class = forced_op_class;
	} else {
		pr_copy_channels(&op_mode.channels, &op_channels,
				 pr->cfg->support_6ghz);
	}

	pr_buf_add_operation_mode(buf, &op_mode);

	/* PR Device Identity Resolution attribute */
	if (!pr_derive_dira(pr, pr->cfg->dev_addr, &dira))
		pr_buf_add_dira(buf, &dira);

	ie_type = (OUI_WFA << 8) | PR_OUI_TYPE;
	buf2 = pr_encaps_elem(buf, ie_type);
	wpabuf_free(buf);

	if (wpabuf_tailroom(extra_ies) < wpabuf_len(buf2)) {
		wpa_printf(MSG_INFO,
			   "PR: Not enough room for PR element in PASN Frame");
		wpabuf_free(buf2);
		return -1;
	}
	wpabuf_put_buf(extra_ies, buf2);
	wpabuf_free(buf2);

	return 0;
}


static struct wpabuf * pr_pasn_generate_rsnxe(struct pr_data *pr, int akmp)
{
	u32 capab;
	size_t flen = 0;
	struct wpabuf *buf;

	capab = BIT(WLAN_RSNX_CAPAB_KEK_IN_PASN);

	if (wpa_key_mgmt_sae(akmp))
		capab |= BIT(WLAN_RSNX_CAPAB_SAE_H2E);
	if (pr->cfg->secure_he_ltf)
		capab |= BIT(WLAN_RSNX_CAPAB_SECURE_LTF);

	while (capab >> flen * 8)
		flen++;

	buf = wpabuf_alloc(2 + flen);
	if (!buf)
		return NULL;

	capab |= flen - 1; /* bit 0-3 = Field length (n - 1) */

	wpa_printf(MSG_DEBUG, "PR: RSNXE capabilities: %04x", capab);
	wpabuf_put_u8(buf, WLAN_EID_RSNX);
	wpabuf_put_u8(buf, flen);
	while (flen--) {
		wpabuf_put_u8(buf, capab & 0xff);
		capab = capab >> 8;
	}

	return buf;
}


/* SSID used for deriving SAE pt for PR security */
#define PR_PASN_SSID "516F9A010000"

static void pr_pasn_set_password(struct pasn_data *pasn, u8 pasn_type,
				 const char *passphrase)
{
	int pasn_groups[4] = { 0, 0, 0, 0 };
	size_t len;

	if (!passphrase)
		return;

	len = os_strlen(passphrase);

	if ((pasn_type & (PR_PASN_DH20_UNAUTH | PR_PASN_DH20_AUTH)) &&
	    (pasn_type & (PR_PASN_DH19_UNAUTH | PR_PASN_DH19_AUTH))) {
		pasn_groups[0] = 20;
		pasn_groups[1] = 19;
	} else if (pasn_type & (PR_PASN_DH20_UNAUTH | PR_PASN_DH20_AUTH)) {
		pasn_groups[0] = 20;
	} else {
		pasn_groups[0] = 19;
	}
	pasn->pt = sae_derive_pt(pasn_groups, (const u8 *) PR_PASN_SSID,
				 os_strlen(PR_PASN_SSID),
				 (const u8 *) passphrase, len, NULL, 0);
}


static int pr_pasn_initialize(struct pr_data *pr, struct pr_device *dev,
			      const u8 *addr, u8 auth_mode, int freq,
			      u8 ranging_type, const u8 *pmkid)
{
	struct wpabuf *rsnxe;
	struct pasn_data *pasn;

	if (dev->pasn) {
		wpa_pasn_reset(dev->pasn);
	} else {
		dev->pasn = pasn_data_init();
		if (!dev->pasn)
			return -1;
	}

	pasn = dev->pasn;
	os_memcpy(pasn->own_addr, pr->cfg->dev_addr, ETH_ALEN);
	os_memcpy(pasn->peer_addr, addr, ETH_ALEN);

	if (dev->pasn_role == PR_ROLE_PASN_INITIATOR) {
		pasn->pmksa = pr->initiator_pmksa;
		os_memcpy(pasn->bssid, pasn->peer_addr, ETH_ALEN);
	} else {
		pasn->pmksa = pr->responder_pmksa;
		os_memcpy(pasn->bssid, pasn->own_addr, ETH_ALEN);
	}

	pasn->noauth = 1;

	/* As specified in Proximity Ranging Implementation Considerations for
	 * P2P Operation D1.8, unauthenticated mode PASN with DH group 19
	 * should be supported by all P2P proximity ranging devices. Skip
	 * this check for OOB peers whose capabilities are not known.
	 */
	if (dev->discovery_type != PR_DISCOVERY_TYPE_OOB &&
	    (!(pr->cfg->pasn_type & BIT(0)) ||
	     !(dev->pr_caps.pasn_type & BIT(0)))) {
		wpa_printf(MSG_DEBUG,
			   "PR PASN: Unauthenticated DH group 19 NOT supported, PASN type of self 0x%x, peer 0x%x",
			   pr->cfg->pasn_type, dev->pr_caps.pasn_type);
		return -1;
	}

	/* As specified in Proximity Ranging Implementation Considerations for
	 * P2P Operation D1.8, EDCA based ranging is only supported with
	 * unauthenticated mode PASN with DH group 19. */
	if (((pr->cfg->pasn_type & 0xc) && (dev->pr_caps.pasn_type & 0xc)) &&
	    ranging_type != PR_EDCA_BASED_RANGING) {
		pasn->group = 20;
		pasn->cipher = WPA_CIPHER_GCMP_256;
	} else {
		pasn->group = 19;
		pasn->cipher = WPA_CIPHER_CCMP;
	}

	if (pr->cfg->secure_he_ltf &&
	    ranging_type == PR_NTB_SECURE_LTF_BASED_RANGING) {
		pasn->secure_ltf = true;
		pasn_enable_kdk_derivation(pasn);
	} else {
		pasn_disable_kdk_derivation(pasn);
	}
	wpa_printf(MSG_DEBUG, "PASN: kdk_len=%zu", pasn->kdk_len);

	if (auth_mode == PR_PASN_AUTH_MODE_SAE) {
		pasn->akmp = WPA_KEY_MGMT_SAE;
		if (dev->password_valid) {
			pr_pasn_set_password(pasn, pr->cfg->pasn_type,
					     dev->password);
		} else if (pr->cfg->global_password_valid) {
			pr_pasn_set_password(pasn, pr->cfg->pasn_type,
					     pr->cfg->global_password);
		} else {
			wpa_printf(MSG_INFO, "PR PASN: Password not available");
			return -1;
		}
	} else if (auth_mode == PR_PASN_AUTH_MODE_PMK && dev->pmk_valid) {
		if (!dev->pmk_valid) {
			wpa_printf(MSG_INFO, "PR PASN: PMK not available");
			return -1;
		}
		if (dev->pasn_role == PR_ROLE_PASN_INITIATOR)
			pasn_initiator_pmksa_cache_add(pr->initiator_pmksa,
						       pasn->own_addr,
						       pasn->peer_addr,
						       dev->pmk,
						       dev->pmk_len,
						       pmkid,
						       WPA_KEY_MGMT_SAE);
		else
			pasn_responder_pmksa_cache_add(pr->responder_pmksa,
						       pasn->own_addr,
						       pasn->peer_addr,
						       dev->pmk,
						       dev->pmk_len,
						       pmkid,
						       WPA_KEY_MGMT_SAE);
		pasn->akmp = WPA_KEY_MGMT_SAE;
	} else {
		pasn->akmp = WPA_KEY_MGMT_PASN;
	}

	pasn->rsn_pairwise = pasn->cipher;
	pasn->wpa_key_mgmt = pasn->akmp;

	rsnxe = pr_pasn_generate_rsnxe(pr, pasn->akmp);
	if (rsnxe) {
		os_free(pasn->rsnxe_ie);
		pasn->rsnxe_ie = os_memdup(wpabuf_head_u8(rsnxe),
					   wpabuf_len(rsnxe));
		wpabuf_free(rsnxe);
		if (!pasn->rsnxe_ie)
			return -1;
	}

	pasn->cb_ctx = pr->cfg->cb_ctx;
	pasn->send_mgmt = pr->cfg->pasn_send_mgmt;
	pasn->freq = freq;
	return 0;
}


static int pr_validate_pasn_request(struct pr_data *pr, struct pr_device *dev,
				    u8 auth_mode, u8 ranging_role,
				    u8 ranging_type)
{
	if (!ranging_role || !ranging_type)
		return -1;

	if (auth_mode == PR_PASN_AUTH_MODE_PASN) {
		if (!(pr->cfg->pasn_type &
		      (PR_PASN_DH19_UNAUTH | PR_PASN_DH20_UNAUTH)) ||
		    !(dev->pr_caps.pasn_type &
		      (PR_PASN_DH19_UNAUTH | PR_PASN_DH20_UNAUTH))) {
			wpa_printf(MSG_INFO,
				   "PR: Dev/Peer doesn't support PASN-UNAUTH");
			return -1;
		}
	} else if (auth_mode == PR_PASN_AUTH_MODE_PMK ||
		   auth_mode == PR_PASN_AUTH_MODE_SAE) {
		if (!(pr->cfg->pasn_type &
		      (PR_PASN_DH19_AUTH | PR_PASN_DH20_AUTH)) ||
		    !(dev->pr_caps.pasn_type &
		      (PR_PASN_DH19_AUTH | PR_PASN_DH20_AUTH))) {
			wpa_printf(MSG_INFO, "PR: Dev/Peer doesn't support PASN-SAE/PMK");
			return -1;
		}
	}

	if (ranging_type == PR_NTB_SECURE_LTF_BASED_RANGING ||
	    ranging_type == PR_NTB_OPEN_BASED_RANGING) {
		if (ranging_type == PR_NTB_SECURE_LTF_BASED_RANGING &&
		    (!pr->cfg->secure_he_ltf || !dev->ntb_caps.secure_he_ltf)) {
			wpa_printf(MSG_INFO,
				   "PR: Dev/Peer doesn't support HE-LTF");
			return -1;
		}

		if (ranging_role == PR_ISTA_SUPPORT &&
		    !pr->cfg->ntb_ista_support) {
			wpa_printf(MSG_INFO,
				   "PR: Device doesn't support NTB ISTA role");
			return -1;
		}

		if (ranging_role == PR_RSTA_SUPPORT &&
		    !pr->cfg->ntb_rsta_support) {
			wpa_printf(MSG_INFO,
				   "PR: Device doesn't support NTB RSTA role");
			return -1;
		}

		if (ranging_role == PR_ISTA_SUPPORT &&
		    !dev->ntb_caps.rsta_support &&
		    !pr->cfg->ntb_rsta_support) {
			wpa_printf(MSG_INFO,
				   "PR: Device and Peer doesn't support NTB RSTA role, no possiblity for negotiation update");
			return -1;
		}

		if (ranging_role == PR_RSTA_SUPPORT &&
		    !dev->ntb_caps.ista_support &&
		    !pr->cfg->ntb_ista_support) {
			wpa_printf(MSG_INFO,
				   "PR: Device and Peer doesn't support NTB ISTA role, no possiblity for negotiation update");
			return -1;
		}
	} else if (ranging_type == PR_EDCA_BASED_RANGING) {
		if (ranging_role == PR_ISTA_SUPPORT &&
		    !pr->cfg->edca_ista_support) {
			wpa_printf(MSG_INFO,
				   "PR: Device doesn't support EDCA ISTA role");
			return -1;
		}

		if (ranging_role == PR_RSTA_SUPPORT &&
		    !pr->cfg->edca_rsta_support) {
			wpa_printf(MSG_INFO,
				   "PR: Device doesn't support EDCA RSTA role");
			return -1;
		}

		if (ranging_role == PR_ISTA_SUPPORT &&
		    !dev->edca_caps.rsta_support &&
		    !pr->cfg->edca_rsta_support) {
			wpa_printf(MSG_INFO,
				   "PR: Device and Peer doesn't support EDCA RSTA role, no possiblity for negotiation update");
			return -1;
		}

		if (ranging_role == PR_RSTA_SUPPORT &&
		    !dev->edca_caps.ista_support &&
		    !pr->cfg->edca_ista_support) {
			wpa_printf(MSG_INFO,
				   "PR: Device and Peer doesn't support EDCA ISTA role, no possiblity for negotiation update");
			return -1;
		}
	}

	return 0;
}


int pr_initiate_pasn_auth(struct pr_data *pr, const u8 *addr, int freq,
			  u8 auth_mode, u8 ranging_role, u8 ranging_type,
			  int forced_pr_freq)
{
	int ret = 0;
	struct pasn_data *pasn;
	struct pr_device *dev;
	u8 pmkid[PMKID_LEN];
	struct wpabuf *extra_ies;

	if (!addr) {
		wpa_printf(MSG_DEBUG, "PR PASN: Peer address NULL");
		return -1;
	}

	dev = pr_get_device(pr, addr);
	if (!dev) {
		wpa_printf(MSG_DEBUG, "PR PASN: Peer not known");
		return -1;
	}

	if (dev->discovery_type != PR_DISCOVERY_TYPE_OOB &&
	    pr_validate_pasn_request(pr, dev, auth_mode, ranging_role,
				     ranging_type) < 0) {
		wpa_printf(MSG_INFO,
			   "PR PASN: Invalid parameters to initiate authentication");
		return -1;
	}

	if (freq == 0)
		freq = dev->listen_freq;

	dev->pasn_role = PR_ROLE_PASN_INITIATOR;

	if (auth_mode == PR_PASN_AUTH_MODE_PMK && dev->pmk_valid &&
	    os_get_random(pmkid, PMKID_LEN) < 0)
		return -1;

	if (pr_pasn_initialize(pr, dev, addr, auth_mode, freq, ranging_type,
			       pmkid)) {
		wpa_printf(MSG_INFO, "PR PASN: Initialization failed");
		return -1;
	}
	pasn = dev->pasn;

	extra_ies = wpabuf_alloc(1500);
	if (!extra_ies)
		return -1;

	if (pr_prepare_pasn_pr_elem(pr, extra_ies, false, ranging_role,
				    ranging_type, forced_pr_freq)) {
		wpa_printf(MSG_INFO,
			   "PR PASN: Failed to prepare extra elements");
		ret = -1;
		goto out;
	}

	pasn_set_extra_ies(dev->pasn, wpabuf_head_u8(extra_ies),
			   wpabuf_len(extra_ies));

	if (auth_mode == PR_PASN_AUTH_MODE_PMK) {
		ret = wpa_pasn_verify(pasn, pasn->own_addr, pasn->peer_addr,
				      pasn->bssid, pasn->akmp, pasn->cipher,
				      pasn->group, pasn->freq, NULL, 0, NULL, 0,
				      NULL);
	} else {
		ret = wpas_pasn_start(pasn, pasn->own_addr, pasn->peer_addr,
				      pasn->bssid, pasn->akmp, pasn->cipher,
				      pasn->group, pasn->freq, NULL, 0, NULL, 0,
				      NULL);
	}
	if (ret) {
		wpa_printf(MSG_INFO, "PR PASN: Failed to start PASN");
	} else {
		/* M1 sent successfully - notify that negotiation has started */
		if (pr->cfg->negotiation_started)
			pr->cfg->negotiation_started(pr->cfg->cb_ctx, addr,
						     ranging_role,
						     ranging_type);
	}

out:
	wpabuf_free(extra_ies);
	return ret;
}


int pr_pasn_auth_tx_status(struct pr_data *pr, const u8 *data, size_t data_len,
			   bool acked)
{
	int ret = 0;
	struct pr_device *dev;
	struct pasn_data *pasn;
	const struct ieee80211_mgmt *mgmt =
		(const struct ieee80211_mgmt *) data;
	u8 self_format_bw, peer_format_bw;

	if (!pr)
		return -1;

	dev = pr_get_device(pr, mgmt->da);
	if (!dev || !dev->pasn) {
		wpa_printf(MSG_INFO, "PR PASN: Peer not found " MACSTR,
			   MAC2STR(mgmt->da));
		return -1;
	}

	pasn = dev->pasn;
	wpa_printf(MSG_DEBUG, "PR PASN: TX status from " MACSTR " ack=%d",
		   MAC2STR(mgmt->da), acked);

	ret = wpa_pasn_auth_tx_status(pasn, data, data_len, acked);

	/*
	 * Authentication frame 1 was not acked; return to caller to schedule a
	 * retransmission. Preserve pasn->frame for the retry.
	 */
	if (ret == 2)
		return ret;

	if (ret == 1 && acked && pr->cfg->pasn_result)
		pr->cfg->pasn_result(pr->cfg->cb_ctx, dev->ranging_role,
				     dev->protocol_type, dev->final_op_class,
				     dev->final_op_channel, pr->cfg->country);

	if (ret == 1 && acked && pr->cfg->get_ranging_params) {
		if (dev->protocol_type & PR_EDCA_BASED_RANGING) {
			self_format_bw = pr->cfg->edca_format_and_bw;
			peer_format_bw = dev->edca_caps.edca_hw_caps &
				EDCA_FORMAT_AND_BW_MASK;
		} else if ((dev->protocol_type &
			    PR_NTB_SECURE_LTF_BASED_RANGING) ||
			   (dev->protocol_type & PR_NTB_OPEN_BASED_RANGING)) {
			self_format_bw = pr->cfg->ntb_format_and_bw;
			peer_format_bw = dev->ntb_caps.ntb_hw_caps &
				NTB_FORMAT_AND_BW_MASK;
		} else {
			wpa_printf(MSG_INFO,
				   "PR PASN: Invalid protocol type: %u",
				   dev->protocol_type);
			goto out;
		}

		pr->cfg->get_ranging_params(pr->cfg->cb_ctx, pr->cfg->dev_addr,
					    dev->pr_device_addr,
					    dev->ranging_role,
					    dev->protocol_type,
					    dev->final_op_class,
					    dev->final_op_channel,
					    self_format_bw,
					    peer_format_bw);
		pr_flush(pr);
	}

out:
	wpabuf_free(pasn->frame);
	pasn->frame = NULL;

	return ret;
}


int pr_pasn_auth_retransmit(struct pr_data *pr, const u8 *addr)
{
	struct pr_device *dev;
	struct pasn_data *pasn;

	dev = pr_get_device(pr, addr);
	if (!dev || !dev->pasn || !dev->pasn->frame)
		return -1;

	pasn = dev->pasn;
	wpa_printf(MSG_DEBUG, "PR PASN: retransmit Authentication frame 1 to "
		   MACSTR, MAC2STR(addr));
	return pasn->send_mgmt(pasn->cb_ctx, wpabuf_head(pasn->frame),
			       wpabuf_len(pasn->frame), 0, pasn->freq, 1000);
}


static int pr_process_pasn_ranging_wrapper(struct pr_data *pr,
					   struct pr_device *dev,
					   const struct ieee80211_mgmt *mgmt,
					   size_t len, int trans_seq)
{
	u32 ie_type;
	const u8 *ies;
	size_t ies_len;
	u8 status = PR_NEGOTIATION_FAIL;
	bool success = false;
	struct wpabuf *buf, *buf2;
	struct pr_message msg;
	struct pr_capabilities caps;
	struct edca_capabilities edca;
	struct ntb_capabilities ntb;
	struct operation_mode op_mode;
	u8 supp_ranging_role = 0;
	struct operation_mode res_op_mode;
	bool ntb_caps_valid = false, edca_caps_valid = false;

	buf = wpabuf_alloc(1000);
	if (!buf) {
		wpa_printf(MSG_INFO, "PR: Memory allocation failed");
		return -1;
	}

	os_memset(&msg, 0, sizeof(msg));
	ies = mgmt->u.auth.variable;
	if (offsetof(struct ieee80211_mgmt, u.auth.variable) > len)
		return -1;
	ies_len = len - offsetof(struct ieee80211_mgmt, u.auth.variable);

	if (pr_parse_elements(ies, ies_len, &msg) || !msg.op_mode) {
		wpa_printf(MSG_INFO,
			   "PR PASN: Failed to parse PR element in Auth1");
		goto end;
	}

	if (trans_seq == WLAN_AUTH_TR_SEQ_PASN_AUTH2) {
		if (!msg.status_ie || !msg.status_ie_len) {
			wpa_printf(MSG_DEBUG, "PR INFO: * No status attribute");
			wpabuf_free(buf);
			pr_parse_free(&msg);
			return -1;
		}
		if (*msg.status_ie == PR_NEGOTIATION_FAIL) {
			wpa_printf(MSG_INFO,
				   "PR PASN: * Ranging Negotiation status fail");
			wpabuf_free(buf);
			pr_parse_free(&msg);
			return -1;
		}
	}

	if (!msg.op_mode || !msg.op_mode_len ||
	    !msg.pr_capability || !msg.pr_capability_len ||
	    ((!msg.edca_capability || !msg.edca_capability_len) &&
	     (!msg.ntb_capability || !msg.ntb_capability_len)))
		goto end;

	if (msg.dira && msg.dira_len)
		pr_validate_dira(pr, dev, msg.dira, msg.dira_len);

	pr_process_op_mode(msg.op_mode, msg.op_mode_len, &op_mode);
	if (!op_mode.channels.op_classes) {
		wpa_printf(MSG_INFO, "PR: Received empty channel list");
		goto end;
	}

	os_memset(&caps, 0, sizeof(struct pr_capabilities));
	pr_process_ranging_capabilities(msg.pr_capability,
					msg.pr_capability_len, &caps);
	if (!pr_eq_ranging_capa_params(dev, &caps)) {
		wpa_printf(MSG_INFO, "PR: Ranging capabilities not matching");
		goto end;
	}

	pr_get_ranging_capabilities(pr, &caps);

	if ((op_mode.protocol_type & PR_EDCA_BASED_RANGING) &&
	    (!msg.edca_capability || !msg.edca_capability_len))
		goto end;
	if ((op_mode.protocol_type & PR_NTB_OPEN_BASED_RANGING) &&
	    (!msg.ntb_capability || !msg.ntb_capability_len))
		goto end;
	if ((op_mode.protocol_type & PR_NTB_SECURE_LTF_BASED_RANGING) &&
	    (!msg.ntb_capability || !msg.ntb_capability_len))
		goto end;

	if (op_mode.protocol_type &
	    (PR_NTB_SECURE_LTF_BASED_RANGING | PR_NTB_OPEN_BASED_RANGING)) {
		pr_process_ntb_capabilities(msg.ntb_capability,
					    msg.ntb_capability_len, &ntb,
					    caps.secure_he_ltf);

		if (!pr_eq_ntb_params(dev, &ntb)) {
			wpa_printf(MSG_INFO,
				   "PR: NTB capabilities not matching");
			goto end;
		}

		if (dev->ntb_caps.ista_support)
			supp_ranging_role |= PR_ISTA_SUPPORT;
		if (dev->ntb_caps.rsta_support)
			supp_ranging_role |= PR_RSTA_SUPPORT;

		pr_get_ntb_capabilities(pr, &ntb);
		ntb_caps_valid = true;
	} else if (op_mode.protocol_type & PR_EDCA_BASED_RANGING) {
		pr_process_edca_capabilities(msg.edca_capability,
					     msg.edca_capability_len, &edca);
		if (!pr_eq_edca_params(dev, &edca)) {
			wpa_printf(MSG_INFO,
				   "PR: EDCA capabilities not matching");
			goto end;
		}

		if (dev->edca_caps.ista_support)
			supp_ranging_role |= PR_ISTA_SUPPORT;
		if (dev->edca_caps.rsta_support)
			supp_ranging_role |= PR_RSTA_SUPPORT;

		pr_get_edca_capabilities(pr, &edca);
		edca_caps_valid = true;
	}

	if (trans_seq == WLAN_AUTH_TR_SEQ_PASN_AUTH1)
		status = pr_pasn_get_best_op_mode(pr, supp_ranging_role,
						  &op_mode, &res_op_mode);
	else if (trans_seq == WLAN_AUTH_TR_SEQ_PASN_AUTH2)
		status = pr_pasn_get_final_op_mode(pr, supp_ranging_role,
						   &op_mode, &res_op_mode);

	if (status != PR_NEGOTIATION_SUCCESS &&
	    status != PR_NEGOTIATION_UPDATE) {
		wpa_printf(MSG_DEBUG,
			   "PR: Couldn't derive suitable operation mode");
		goto end;
	}

	if (trans_seq == WLAN_AUTH_TR_SEQ_PASN_AUTH1) {
		pr_buf_add_ranging_capa_info(buf, &caps);
		if (edca_caps_valid)
			pr_buf_add_edca_capa_info(buf, &edca);
		if (ntb_caps_valid)
			pr_buf_add_ntb_capa_info(buf, &ntb);
	}

	pr_buf_add_ranging_neg_status(buf, status);
	pr_buf_add_operation_mode(buf, &res_op_mode);

	dev->ranging_role = res_op_mode.role;
	dev->protocol_type = res_op_mode.protocol_type;

	if (trans_seq == WLAN_AUTH_TR_SEQ_PASN_AUTH2) {
		dev->final_op_channel =
			res_op_mode.channels.op_class[0].channel[0];
		dev->final_op_class = res_op_mode.channels.op_class[0].op_class;
	}

	success = true;
end:
	if (!success)
		pr_buf_add_ranging_neg_status(buf, PR_NEGOTIATION_FAIL);

	ie_type = (OUI_WFA << 8) | PR_OUI_TYPE;
	buf2 = pr_encaps_elem(buf, ie_type);
	wpabuf_free(buf);

	wpabuf_free(dev->ranging_wrapper);
	dev->ranging_wrapper = buf2;
	pr_parse_free(&msg);

	return 0;
}


static int
pr_process_pasn_ranging_wrapper_result(struct pr_data *pr,
				       struct pr_device *dev,
				       const struct ieee80211_mgmt *mgmt,
				       size_t len)
{
	int ret = -1;
	const u8 *ies;
	size_t ies_len;
	struct pr_message msg;
	struct operation_mode op_mode;
	struct pr_channels common_chan;

	os_memset(&msg, 0, sizeof(msg));
	ies = mgmt->u.auth.variable;
	ies_len = len - offsetof(struct ieee80211_mgmt, u.auth.variable);

	if (pr_parse_elements(ies, ies_len, &msg) || !msg.op_mode) {
		wpa_printf(MSG_INFO,
			   "PR PASN: Failed to parse PR element in Auth3");
		goto fail;
	}

	if (!msg.status_ie || !msg.status_ie_len) {
		wpa_printf(MSG_INFO, "PR PASN: * No status attribute");
		goto fail;
	}

	if (*msg.status_ie == PR_NEGOTIATION_FAIL) {
		wpa_printf(MSG_INFO,
			   "PR PASN: * Ranging Negotiation status fail");
		goto fail;
	}

	if (!msg.op_mode || !msg.op_mode_len)
		goto fail;

	pr_process_op_mode(msg.op_mode, msg.op_mode_len, &op_mode);
	if (op_mode.channels.op_classes != 1) {
		wpa_printf(MSG_INFO, "PR: PASN received invalid channel list");
		goto fail;
	}

	wpa_printf(MSG_DEBUG,
		   "PR PASN: Frame 3: Operating mode data: Role=%u, protocol type=%u, operating class=%u, channel= %u",
		   op_mode.role, op_mode.protocol_type,
		   op_mode.channels.op_class[0].op_class,
		   op_mode.channels.op_class[0].channel[0]);

	if (op_mode.protocol_type &
	    (PR_NTB_SECURE_LTF_BASED_RANGING | PR_NTB_OPEN_BASED_RANGING)) {
		if ((op_mode.role & PR_ISTA_SUPPORT) &&
		    !pr->cfg->ntb_rsta_support)
			goto fail;

		if ((op_mode.role & PR_RSTA_SUPPORT) &&
		    !pr->cfg->ntb_ista_support)
			goto fail;

		if ((op_mode.protocol_type & PR_NTB_SECURE_LTF_BASED_RANGING) &&
		    !pr->cfg->secure_he_ltf)
			goto fail;

		pr_channels_intersect(&pr->cfg->ntb_channels, &op_mode.channels,
				      &common_chan);
		if (common_chan.op_classes == 0) {
			wpa_printf(MSG_INFO, "PR PASN: No common channel");
			goto fail;
		}
	} else if (op_mode.protocol_type & PR_EDCA_BASED_RANGING) {
		if ((op_mode.role & PR_ISTA_SUPPORT) &&
		    !pr->cfg->edca_rsta_support)
			goto fail;

		if ((op_mode.role & PR_RSTA_SUPPORT) &&
		    !pr->cfg->edca_ista_support)
			goto fail;

		pr_channels_intersect(&pr->cfg->edca_channels,
				      &op_mode.channels, &common_chan);
		if (common_chan.op_classes == 0)
			goto fail;
	}

	if (op_mode.role & PR_RSTA_SUPPORT)
		dev->ranging_role = PR_ISTA_SUPPORT;
	else
		dev->ranging_role = PR_RSTA_SUPPORT;
	dev->protocol_type = op_mode.protocol_type;
	dev->final_op_channel = op_mode.channels.op_class[0].channel[0];
	dev->final_op_class = op_mode.channels.op_class[0].op_class;
	ret = 0;

fail:
	pr_parse_free(&msg);
	return ret;
}


static int pr_pasn_handle_auth_1(struct pr_data *pr, struct pr_device *dev,
				 const struct ieee80211_mgmt *mgmt, size_t len,
				 int freq)
{
	int ret = -1;
	u8 pasn_type;
	u8 auth_mode = 0;
	int pasn_groups[4] = { 0 };
	struct wpa_ie_data rsn_data;
	struct ieee802_11_elems elems;

	pasn_type = pr->cfg->pasn_type;
	if (pasn_type & (PR_PASN_DH20_UNAUTH | PR_PASN_DH20_AUTH) &&
	    pasn_type & (PR_PASN_DH19_UNAUTH | PR_PASN_DH19_AUTH)) {
		pasn_groups[0] = 20;
		pasn_groups[1] = 19;
	} else if (pasn_type & (PR_PASN_DH20_UNAUTH | PR_PASN_DH20_AUTH)) {
		pasn_groups[0] = 20;
	} else {
		pasn_groups[0] = 19;
	}

	if (pr_process_pasn_ranging_wrapper(pr, dev, mgmt, len, 1)) {
		wpa_printf(MSG_INFO,
			   "PR PASN: Failed to handle Auth1 action wrapper");
		return -1;
	}

	if (ieee802_11_parse_elems(mgmt->u.auth.variable,
				   len - offsetof(struct ieee80211_mgmt,
				   u.auth.variable),
				   &elems, 0) == ParseFailed) {
		wpa_printf(MSG_DEBUG,
			   "PR PASN: Failed parsing elements in Auth1 frame");
		goto fail;
	}

	if (wpa_parse_wpa_ie_rsn(elems.rsn_ie - 2, elems.rsn_ie_len + 2,
				 &rsn_data) == 0) {
		if (rsn_data.key_mgmt == WPA_KEY_MGMT_SAE && rsn_data.num_pmkid)
			auth_mode = PR_PASN_AUTH_MODE_PMK;
		else if (rsn_data.key_mgmt == WPA_KEY_MGMT_SAE)
			auth_mode = PR_PASN_AUTH_MODE_SAE;
		else
			auth_mode = PR_PASN_AUTH_MODE_PASN;
	}

	dev->pasn_role = PR_ROLE_PASN_RESPONDER;
	if (pr_pasn_initialize(pr, dev, mgmt->sa, auth_mode, freq,
			       dev->protocol_type, rsn_data.pmkid)) {
		wpa_printf(MSG_INFO, "PR PASN: Initialize failed");
		goto fail;
	}

	pasn_set_extra_ies(dev->pasn, wpabuf_head_u8(dev->ranging_wrapper),
			   wpabuf_len(dev->ranging_wrapper));
	os_free(dev->pasn->pasn_groups);
	dev->pasn->pasn_groups = int_array_dup(pasn_groups);
	if (handle_auth_pasn_1(dev->pasn, pr->cfg->dev_addr, mgmt->sa, mgmt,
			       len, false) < 0) {
		wpa_printf(MSG_INFO, "PR PASN: Handle Auth1 failed");
		goto fail;
	}

	if (!(dev->protocol_type & PR_EDCA_BASED_RANGING) &&
	    pr->cfg->set_keys &&
	    pr->cfg->set_keys(pr->cfg->cb_ctx, pr->cfg->dev_addr,
			      dev->pr_device_addr, dev->pasn->cipher,
			      dev->pasn->akmp, &dev->pasn->ptk) < 0) {
		wpa_printf(MSG_INFO, "PR PASN: Key configuration failed");
		goto fail;
	}

	/* M1 received and M2 sent - notify that negotiation has started */
	if (pr->cfg->negotiation_started)
		pr->cfg->negotiation_started(pr->cfg->cb_ctx, mgmt->sa,
					     dev->ranging_role,
					     dev->protocol_type);
	ret = 0;

fail:
	wpabuf_free(dev->ranging_wrapper);
	dev->ranging_wrapper = NULL;
	return ret;
}


static int pr_pasn_handle_auth_2(struct pr_data *pr, struct pr_device *dev,
				 const struct ieee80211_mgmt *mgmt, size_t len)
{
	int ret = -1;
	struct wpa_pasn_params_data pasn_data;

	if (dev->pasn_role != PR_ROLE_PASN_INITIATOR) {
		wpa_printf(MSG_INFO,
			   "PR PASN: Auth2 not expected on responder");
		return -1;
	}

	if (!dev->pasn)
		return -1;

	if (pr_process_pasn_ranging_wrapper(pr, dev, mgmt, len, 2)) {
		wpa_printf(MSG_INFO,
			   "PR PASN: Failed to handle Auth2 action wrapper");
		return -1;
	}
	pasn_set_extra_ies(dev->pasn, wpabuf_head_u8(dev->ranging_wrapper),
			   wpabuf_len(dev->ranging_wrapper));

	if (wpa_pasn_auth_rx(dev->pasn, (const u8 *) mgmt, len,
			     &pasn_data) < 0) {
		wpa_printf(MSG_INFO, "PR PASN: wpa_pasn_auth_rx() failed");
		dev->pasn_role = PR_ROLE_IDLE;
		goto fail;
	}

	if (!(dev->protocol_type & PR_EDCA_BASED_RANGING) &&
	    pr->cfg->set_keys &&
	    pr->cfg->set_keys(pr->cfg->cb_ctx, pr->cfg->dev_addr,
			      dev->pr_device_addr, dev->pasn->cipher,
			      dev->pasn->akmp, &dev->pasn->ptk) < 0) {
		wpa_printf(MSG_INFO, "PR PASN: Key configuration failed");
		goto fail;
	}
	ret = 0;

fail:
	wpabuf_free(dev->ranging_wrapper);
	dev->ranging_wrapper = NULL;
	return ret;
}


static int pr_pasn_handle_auth_3(struct pr_data *pr, struct pr_device *dev,
				 const struct ieee80211_mgmt *mgmt, size_t len)
{
	u8 self_format_bw, peer_format_bw;

	if (dev->pasn_role != PR_ROLE_PASN_RESPONDER) {
		wpa_printf(MSG_INFO,
			   "PR PASN: Auth3 not expected on initiator");
		return -1;
	}

	if (!dev->pasn)
		return -1;

	if (pr_process_pasn_ranging_wrapper_result(pr, dev, mgmt, len)) {
		wpa_printf(MSG_INFO,
			   "PR PASN: Failed to handle Auth3 action wrapper");
		goto fail;
	}

	if (handle_auth_pasn_3(dev->pasn, pr->cfg->dev_addr, mgmt->sa, mgmt,
			       len) < 0) {
		wpa_printf(MSG_INFO, "PR PASN: Failed to handle Auth3");
		goto fail;
	}

	if (pr->cfg->pasn_result)
		pr->cfg->pasn_result(pr->cfg->cb_ctx, dev->ranging_role,
				     dev->protocol_type, dev->final_op_class,
				     dev->final_op_channel, pr->cfg->country);

	if (dev->protocol_type & PR_EDCA_BASED_RANGING) {
		self_format_bw = pr->cfg->edca_format_and_bw;
		peer_format_bw = dev->edca_caps.edca_hw_caps &
			EDCA_FORMAT_AND_BW_MASK;

	} else if ((dev->protocol_type & PR_NTB_SECURE_LTF_BASED_RANGING) ||
		   (dev->protocol_type & PR_NTB_OPEN_BASED_RANGING)) {
		self_format_bw = pr->cfg->ntb_format_and_bw;
		peer_format_bw = dev->ntb_caps.ntb_hw_caps &
			NTB_FORMAT_AND_BW_MASK;
	} else {
		wpa_printf(MSG_INFO, "PR PASN: Invalid protocol type: %u",
			   dev->protocol_type);
		goto fail;
	}

	if (pr->cfg->get_ranging_params)
		pr->cfg->get_ranging_params(pr->cfg->cb_ctx, pr->cfg->dev_addr,
					    dev->pr_device_addr,
					    dev->ranging_role,
					    dev->protocol_type,
					    dev->final_op_class,
					    dev->final_op_channel,
					    self_format_bw,
					    peer_format_bw);
	pr_flush(pr);
	return 0;

fail:
	/* Clear the keys as M3 processing failed */
	if (pr->cfg->clear_keys)
		pr->cfg->clear_keys(pr->cfg->cb_ctx, pr->cfg->dev_addr,
				    dev->pr_device_addr);
	return -1;
}


int pr_pasn_auth_rx(struct pr_data *pr, const struct ieee80211_mgmt *mgmt,
		    size_t len, int freq)
{
	struct pr_device *dev;
	u16 auth_alg, auth_transaction;

	dev = pr_get_device(pr, mgmt->sa);
	if (!dev) {
		wpa_printf(MSG_INFO, "PR: Peer not found " MACSTR,
			   MAC2STR(mgmt->sa));
		return -1;
	}

	if (!ether_addr_equal(mgmt->da, pr->cfg->dev_addr)) {
		wpa_printf(MSG_INFO, "PR PASN: Not our frame");
		return -1;
	}

	if (len < offsetof(struct ieee80211_mgmt, u.auth.variable))
		return -1;

	auth_alg = le_to_host16(mgmt->u.auth.auth_alg);
	if (auth_alg != WLAN_AUTH_PASN) {
		wpa_printf(MSG_INFO,
			   "PR: Unexpected Authentication frame, auth_alg=%d",
			   auth_alg);
		return -1;
	}

	auth_transaction = le_to_host16(mgmt->u.auth.auth_transaction);
	if (auth_transaction == WLAN_AUTH_TR_SEQ_PASN_AUTH1)
		return pr_pasn_handle_auth_1(pr, dev, mgmt, len, freq);
	if (auth_transaction == WLAN_AUTH_TR_SEQ_PASN_AUTH2)
		return pr_pasn_handle_auth_2(pr, dev, mgmt, len);
	if (auth_transaction == WLAN_AUTH_TR_SEQ_PASN_AUTH3)
		return pr_pasn_handle_auth_3(pr, dev, mgmt, len);

	return -1;
}

#endif /* CONFIG_PASN */
