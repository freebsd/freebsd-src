/*
 * FST module - FST group object implementation
 * Copyright (c) 2014, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "common/defs.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "drivers/driver.h"
#include "fst/fst_internal.h"
#include "fst/fst_defs.h"


struct dl_list fst_global_groups_list;

#ifndef HOSTAPD
static Boolean fst_has_fst_peer(struct fst_iface *iface, Boolean *has_peer)
{
	const u8 *bssid;

	bssid = fst_iface_get_bssid(iface);
	if (!bssid) {
		*has_peer = FALSE;
		return FALSE;
	}

	*has_peer = TRUE;
	return fst_iface_get_peer_mb_ie(iface, bssid) != NULL;
}
#endif /* HOSTAPD */


static void fst_dump_mb_ies(const char *group_id, const char *ifname,
			    struct wpabuf *mbies)
{
	const u8 *p = wpabuf_head(mbies);
	size_t s = wpabuf_len(mbies);

	while (s >= 2) {
		const struct multi_band_ie *mbie =
			(const struct multi_band_ie *) p;
		WPA_ASSERT(mbie->eid == WLAN_EID_MULTI_BAND);
		WPA_ASSERT(2 + mbie->len >= sizeof(*mbie));

		fst_printf(MSG_WARNING,
			   "%s: %s: mb_ctrl=%u band_id=%u op_class=%u chan=%u bssid="
			   MACSTR
			   " beacon_int=%u tsf_offs=[%u %u %u %u %u %u %u %u] mb_cc=0x%02x tmout=%u",
			   group_id, ifname,
			   mbie->mb_ctrl, mbie->band_id, mbie->op_class,
			   mbie->chan, MAC2STR(mbie->bssid), mbie->beacon_int,
			   mbie->tsf_offs[0], mbie->tsf_offs[1],
			   mbie->tsf_offs[2], mbie->tsf_offs[3],
			   mbie->tsf_offs[4], mbie->tsf_offs[5],
			   mbie->tsf_offs[6], mbie->tsf_offs[7],
			   mbie->mb_connection_capability,
			   mbie->fst_session_tmout);

		p += 2 + mbie->len;
		s -= 2 + mbie->len;
	}
}


static void fst_fill_mb_ie(struct wpabuf *buf, const u8 *bssid,
			   const u8 *own_addr, enum mb_band_id band, u8 channel)
{
	struct multi_band_ie *mbie;
	size_t len = sizeof(*mbie);

	if (own_addr)
		len += ETH_ALEN;

	mbie = wpabuf_put(buf, len);

	os_memset(mbie, 0, len);

	mbie->eid = WLAN_EID_MULTI_BAND;
	mbie->len = len - 2;
#ifdef HOSTAPD
	mbie->mb_ctrl = MB_STA_ROLE_AP;
	mbie->mb_connection_capability = MB_CONNECTION_CAPABILITY_AP;
#else /* HOSTAPD */
	mbie->mb_ctrl = MB_STA_ROLE_NON_PCP_NON_AP;
	mbie->mb_connection_capability = 0;
#endif /* HOSTAPD */
	if (bssid)
		os_memcpy(mbie->bssid, bssid, ETH_ALEN);
	mbie->band_id = band;
	mbie->op_class = 0;  /* means all */
	mbie->chan = channel;
	mbie->fst_session_tmout = FST_DEFAULT_SESSION_TIMEOUT_TU;

	if (own_addr) {
		mbie->mb_ctrl |= MB_CTRL_STA_MAC_PRESENT;
		os_memcpy(&mbie[1], own_addr, ETH_ALEN);
	}
}


static unsigned fst_fill_iface_mb_ies(struct fst_iface *f, struct wpabuf *buf)
{
	const  u8 *bssid;

	bssid = fst_iface_get_bssid(f);
	if (bssid) {
		enum hostapd_hw_mode hw_mode;
		u8 channel;

		if (buf) {
			fst_iface_get_channel_info(f, &hw_mode, &channel);
			fst_fill_mb_ie(buf, bssid, fst_iface_get_addr(f),
				       fst_hw_mode_to_band(hw_mode), channel);
		}
		return 1;
	} else {
		unsigned bands[MB_BAND_ID_WIFI_60GHZ + 1] = {};
		struct hostapd_hw_modes *modes;
		enum mb_band_id b;
		int num_modes = fst_iface_get_hw_modes(f, &modes);
		int ret = 0;

		while (num_modes--) {
			b = fst_hw_mode_to_band(modes->mode);
			modes++;
			if (b >= ARRAY_SIZE(bands) || bands[b]++)
				continue;
			ret++;
			if (buf)
				fst_fill_mb_ie(buf, NULL, fst_iface_get_addr(f),
					       b, MB_STA_CHANNEL_ALL);
		}
		return ret;
	}
}


static struct wpabuf * fst_group_create_mb_ie(struct fst_group *g,
					      struct fst_iface *i)
{
	struct wpabuf *buf;
	struct fst_iface *f;
	unsigned int nof_mbies = 0;
	unsigned int nof_ifaces_added = 0;
#ifndef HOSTAPD
	Boolean has_peer;
	Boolean has_fst_peer;

	foreach_fst_group_iface(g, f) {
		has_fst_peer = fst_has_fst_peer(f, &has_peer);
		if (has_peer && !has_fst_peer)
			return NULL;
	}
#endif /* HOSTAPD */

	foreach_fst_group_iface(g, f) {
		if (f == i)
			continue;
		nof_mbies += fst_fill_iface_mb_ies(f, NULL);
	}

	buf = wpabuf_alloc(nof_mbies *
			   (sizeof(struct multi_band_ie) + ETH_ALEN));
	if (!buf) {
		fst_printf_iface(i, MSG_ERROR,
				 "cannot allocate mem for %u MB IEs",
				 nof_mbies);
		return NULL;
	}

	/* The list is sorted in descending order by priorities, so MB IEs will
	 * be arranged in the same order, as required by spec (see corresponding
	 * comment in.fst_attach().
	 */
	foreach_fst_group_iface(g, f) {
		if (f == i)
			continue;

		fst_fill_iface_mb_ies(f, buf);
		++nof_ifaces_added;

		fst_printf_iface(i, MSG_DEBUG, "added to MB IE");
	}

	if (!nof_ifaces_added) {
		wpabuf_free(buf);
		buf = NULL;
		fst_printf_iface(i, MSG_INFO,
				 "cannot add MB IE: no backup ifaces");
	} else {
		fst_dump_mb_ies(fst_group_get_id(g), fst_iface_get_name(i),
				buf);
	}

	return buf;
}


static const u8 * fst_mbie_get_peer_addr(const struct multi_band_ie *mbie)
{
	const u8 *peer_addr = NULL;

	switch (MB_CTRL_ROLE(mbie->mb_ctrl)) {
	case MB_STA_ROLE_AP:
		peer_addr = mbie->bssid;
		break;
	case MB_STA_ROLE_NON_PCP_NON_AP:
		if (mbie->mb_ctrl & MB_CTRL_STA_MAC_PRESENT &&
		    (size_t) 2 + mbie->len >= sizeof(*mbie) + ETH_ALEN)
			peer_addr = (const u8 *) &mbie[1];
		break;
	default:
		break;
	}

	return peer_addr;
}


static struct fst_iface *
fst_group_get_new_iface_by_mbie_and_band_id(struct fst_group *g,
					    const u8 *mb_ies_buff,
					    size_t mb_ies_size,
					    u8 band_id,
					    u8 *iface_peer_addr)
{
	while (mb_ies_size >= 2) {
		const struct multi_band_ie *mbie =
			(const struct multi_band_ie *) mb_ies_buff;

		if (mbie->eid != WLAN_EID_MULTI_BAND ||
		    (size_t) 2 + mbie->len < sizeof(*mbie))
			break;

		if (mbie->band_id == band_id) {
			struct fst_iface *iface;

			foreach_fst_group_iface(g, iface) {
				const u8 *peer_addr =
					fst_mbie_get_peer_addr(mbie);

				if (peer_addr &&
				    fst_iface_is_connected(iface, peer_addr) &&
				    band_id == fst_iface_get_band_id(iface)) {
					os_memcpy(iface_peer_addr, peer_addr,
						  ETH_ALEN);
					return iface;
				}
			}
			break;
		}

		mb_ies_buff += 2 + mbie->len;
		mb_ies_size -= 2 + mbie->len;
	}

	return NULL;
}


struct fst_iface * fst_group_get_iface_by_name(struct fst_group *g,
					       const char *ifname)
{
	struct fst_iface *f;

	foreach_fst_group_iface(g, f) {
		const char *in = fst_iface_get_name(f);

		if (os_strncmp(in, ifname, os_strlen(in)) == 0)
			return f;
	}

	return NULL;
}


u8 fst_group_assign_dialog_token(struct fst_group *g)
{
	g->dialog_token++;
	if (g->dialog_token == 0)
		g->dialog_token++;
	return g->dialog_token;
}


u32 fst_group_assign_fsts_id(struct fst_group *g)
{
	g->fsts_id++;
	return g->fsts_id;
}


static Boolean
fst_group_does_iface_appear_in_other_mbies(struct fst_group *g,
					   struct fst_iface *iface,
					   struct fst_iface *other,
					   u8 *peer_addr)
{
	struct fst_get_peer_ctx *ctx;
	const u8 *addr;
	const u8 *iface_addr;
	enum mb_band_id  iface_band_id;

	WPA_ASSERT(g == fst_iface_get_group(iface));
	WPA_ASSERT(g == fst_iface_get_group(other));

	iface_addr = fst_iface_get_addr(iface);
	iface_band_id = fst_iface_get_band_id(iface);

	addr = fst_iface_get_peer_first(other, &ctx, TRUE);
	for (; addr; addr = fst_iface_get_peer_next(other, &ctx, TRUE)) {
		const struct wpabuf *mbies;
		u8 other_iface_peer_addr[ETH_ALEN];
		struct fst_iface *other_new_iface;

		mbies = fst_iface_get_peer_mb_ie(other, addr);
		if (!mbies)
			continue;

		other_new_iface = fst_group_get_new_iface_by_mbie_and_band_id(
			g, wpabuf_head(mbies), wpabuf_len(mbies),
			iface_band_id, other_iface_peer_addr);
		if (other_new_iface == iface &&
		    os_memcmp(iface_addr, other_iface_peer_addr,
			      ETH_ALEN) != 0) {
			os_memcpy(peer_addr, addr, ETH_ALEN);
			return TRUE;
		}
	}

	return FALSE;
}


struct fst_iface *
fst_group_find_new_iface_by_stie(struct fst_group *g,
				 struct fst_iface *iface,
				 const u8 *peer_addr,
				 const struct session_transition_ie *stie,
				 u8 *iface_peer_addr)
{
	struct fst_iface *i;

	foreach_fst_group_iface(g, i) {
		if (i == iface ||
		    stie->new_band_id != fst_iface_get_band_id(i))
			continue;
		if (fst_group_does_iface_appear_in_other_mbies(g, iface, i,
			iface_peer_addr))
			return i;
		break;
	}
	return NULL;
}


struct fst_iface *
fst_group_get_new_iface_by_stie_and_mbie(
	struct fst_group *g, const u8 *mb_ies_buff, size_t mb_ies_size,
	const struct session_transition_ie *stie, u8 *iface_peer_addr)
{
	return fst_group_get_new_iface_by_mbie_and_band_id(
		g, mb_ies_buff, mb_ies_size, stie->new_band_id,
		iface_peer_addr);
}


struct fst_group * fst_group_create(const char *group_id)
{
	struct fst_group *g;

	g = os_zalloc(sizeof(*g));
	if (g == NULL) {
		fst_printf(MSG_ERROR, "%s: Cannot alloc group", group_id);
		return NULL;
	}

	dl_list_init(&g->ifaces);
	os_strlcpy(g->group_id, group_id, sizeof(g->group_id));

	dl_list_add_tail(&fst_global_groups_list, &g->global_groups_lentry);
	fst_printf_group(g, MSG_DEBUG, "instance created");

	foreach_fst_ctrl_call(on_group_created, g);

	return g;
}


void fst_group_attach_iface(struct fst_group *g, struct fst_iface *i)
{
	struct dl_list *list = &g->ifaces;
	struct fst_iface *f;

	/*
	 * Add new interface to the list.
	 * The list is sorted in descending order by priority to allow
	 * multiple MB IEs creation according to the spec (see 10.32 Multi-band
	 * operation, 10.32.1 General), as they should be ordered according to
	 * priorities.
	 */
	foreach_fst_group_iface(g, f) {
		if (fst_iface_get_priority(f) < fst_iface_get_priority(i))
			break;
		list = &f->group_lentry;
	}
	dl_list_add(list, &i->group_lentry);
}


void fst_group_detach_iface(struct fst_group *g, struct fst_iface *i)
{
	dl_list_del(&i->group_lentry);
}


void fst_group_delete(struct fst_group *group)
{
	struct fst_session *s;

	dl_list_del(&group->global_groups_lentry);
	WPA_ASSERT(dl_list_empty(&group->ifaces));
	foreach_fst_ctrl_call(on_group_deleted, group);
	fst_printf_group(group, MSG_DEBUG, "instance deleted");
	while ((s = fst_session_global_get_first_by_group(group)) != NULL)
		fst_session_delete(s);
	os_free(group);
}


Boolean fst_group_delete_if_empty(struct fst_group *group)
{
	Boolean is_empty = !fst_group_has_ifaces(group) &&
		!fst_session_global_get_first_by_group(group);

	if (is_empty)
		fst_group_delete(group);

	return is_empty;
}


void fst_group_update_ie(struct fst_group *g)
{
	struct fst_iface *i;

	foreach_fst_group_iface(g, i) {
		struct wpabuf *mbie = fst_group_create_mb_ie(g, i);

		if (!mbie)
			fst_printf_iface(i, MSG_WARNING, "cannot create MB IE");

		fst_iface_attach_mbie(i, mbie);
		fst_iface_set_ies(i, mbie);
		fst_printf_iface(i, MSG_DEBUG, "multi-band IE set to %p", mbie);
	}
}
