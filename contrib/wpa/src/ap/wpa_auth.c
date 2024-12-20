/*
 * IEEE 802.11 RSN / WPA Authenticator
 * Copyright (c) 2004-2022, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/state_machine.h"
#include "utils/bitfield.h"
#include "common/ieee802_11_defs.h"
#include "common/ocv.h"
#include "common/dpp.h"
#include "common/wpa_ctrl.h"
#include "crypto/aes.h"
#include "crypto/aes_wrap.h"
#include "crypto/aes_siv.h"
#include "crypto/crypto.h"
#include "crypto/sha1.h"
#include "crypto/sha256.h"
#include "crypto/sha384.h"
#include "crypto/sha512.h"
#include "crypto/random.h"
#include "eapol_auth/eapol_auth_sm.h"
#include "drivers/driver.h"
#include "ap_config.h"
#include "ieee802_11.h"
#include "sta_info.h"
#include "wpa_auth.h"
#include "pmksa_cache_auth.h"
#include "wpa_auth_i.h"
#include "wpa_auth_ie.h"

#define STATE_MACHINE_DATA struct wpa_state_machine
#define STATE_MACHINE_DEBUG_PREFIX "WPA"
#define STATE_MACHINE_ADDR wpa_auth_get_spa(sm)


static void wpa_send_eapol_timeout(void *eloop_ctx, void *timeout_ctx);
static int wpa_sm_step(struct wpa_state_machine *sm);
static int wpa_verify_key_mic(int akmp, size_t pmk_len, struct wpa_ptk *PTK,
			      u8 *data, size_t data_len);
#ifdef CONFIG_FILS
static int wpa_aead_decrypt(struct wpa_state_machine *sm, struct wpa_ptk *ptk,
			    u8 *buf, size_t buf_len, u16 *_key_data_len);
static struct wpabuf * fils_prepare_plainbuf(struct wpa_state_machine *sm,
					     const struct wpabuf *hlp);
#endif /* CONFIG_FILS */
static void wpa_sm_call_step(void *eloop_ctx, void *timeout_ctx);
static void wpa_group_sm_step(struct wpa_authenticator *wpa_auth,
			      struct wpa_group *group);
static void wpa_request_new_ptk(struct wpa_state_machine *sm);
static int wpa_gtk_update(struct wpa_authenticator *wpa_auth,
			  struct wpa_group *group);
static int wpa_group_config_group_keys(struct wpa_authenticator *wpa_auth,
				       struct wpa_group *group);
static int wpa_derive_ptk(struct wpa_state_machine *sm, const u8 *snonce,
			  const u8 *pmk, unsigned int pmk_len,
			  struct wpa_ptk *ptk, int force_sha256,
			  u8 *pmk_r0, u8 *pmk_r1, u8 *pmk_r0_name,
			  size_t *key_len, bool no_kdk);
static void wpa_group_free(struct wpa_authenticator *wpa_auth,
			   struct wpa_group *group);
static void wpa_group_get(struct wpa_authenticator *wpa_auth,
			  struct wpa_group *group);
static void wpa_group_put(struct wpa_authenticator *wpa_auth,
			  struct wpa_group *group);
static int ieee80211w_kde_len(struct wpa_state_machine *sm);
static u8 * ieee80211w_kde_add(struct wpa_state_machine *sm, u8 *pos);
static void wpa_group_update_gtk(struct wpa_authenticator *wpa_auth,
				 struct wpa_group *group);


static const u32 eapol_key_timeout_first = 100; /* ms */
static const u32 eapol_key_timeout_subseq = 1000; /* ms */
static const u32 eapol_key_timeout_first_group = 500; /* ms */
static const u32 eapol_key_timeout_no_retrans = 4000; /* ms */

/* TODO: make these configurable */
static const int dot11RSNAConfigPMKLifetime = 43200;
static const int dot11RSNAConfigPMKReauthThreshold = 70;
static const int dot11RSNAConfigSATimeout = 60;


static const u8 * wpa_auth_get_aa(const struct wpa_state_machine *sm)
{
#ifdef CONFIG_IEEE80211BE
	if (sm->mld_assoc_link_id >= 0)
		return sm->wpa_auth->mld_addr;
#endif /* CONFIG_IEEE80211BE */
	return sm->wpa_auth->addr;
}


static const u8 * wpa_auth_get_spa(const struct wpa_state_machine *sm)
{
#ifdef CONFIG_IEEE80211BE
	if (sm->mld_assoc_link_id >= 0)
		return sm->peer_mld_addr;
#endif /* CONFIG_IEEE80211BE */
	return sm->addr;
}


static void wpa_gkeydone_sta(struct wpa_state_machine *sm)
{
#ifdef CONFIG_IEEE80211BE
	int link_id;
#endif /* CONFIG_IEEE80211BE */

	if (!sm->wpa_auth)
		return;

	sm->wpa_auth->group->GKeyDoneStations--;
	sm->GUpdateStationKeys = false;

#ifdef CONFIG_IEEE80211BE
	for_each_sm_auth(sm, link_id)
		sm->mld_links[link_id].wpa_auth->group->GKeyDoneStations--;
#endif /* CONFIG_IEEE80211BE */
}


#ifdef CONFIG_IEEE80211BE

void wpa_release_link_auth_ref(struct wpa_state_machine *sm,
			       int release_link_id)
{
	int link_id;

	if (!sm || release_link_id >= MAX_NUM_MLD_LINKS)
		return;

	for_each_sm_auth(sm, link_id) {
		if (link_id == release_link_id) {
			wpa_group_put(sm->mld_links[link_id].wpa_auth,
				      sm->mld_links[link_id].wpa_auth->group);
			sm->mld_links[link_id].wpa_auth = NULL;
		}
	}
}


struct wpa_get_link_auth_ctx {
	const u8 *addr;
	const u8 *mld_addr;
	int link_id;
	struct wpa_authenticator *wpa_auth;
};

static int wpa_get_link_sta_auth(struct wpa_authenticator *wpa_auth, void *data)
{
	struct wpa_get_link_auth_ctx *ctx = data;

	if (!wpa_auth->is_ml)
		return 0;

	if (ctx->mld_addr &&
	    !ether_addr_equal(wpa_auth->mld_addr, ctx->mld_addr))
		return 0;

	if ((ctx->addr && ether_addr_equal(wpa_auth->addr, ctx->addr)) ||
	    (ctx->link_id > -1 && wpa_auth->is_ml &&
	     wpa_auth->link_id == ctx->link_id)) {
		ctx->wpa_auth = wpa_auth;
		return 1;

	}
	return 0;
}


static struct wpa_authenticator *
wpa_get_link_auth(struct wpa_authenticator *wpa_auth, int link_id)
{
	struct wpa_get_link_auth_ctx ctx;

	ctx.addr = NULL;
	ctx.mld_addr = wpa_auth->mld_addr;
	ctx.link_id = link_id;
	ctx.wpa_auth = NULL;
	wpa_auth_for_each_auth(wpa_auth, wpa_get_link_sta_auth, &ctx);
	return ctx.wpa_auth;
}


static int wpa_get_primary_auth_cb(struct wpa_authenticator *wpa_auth,
				   void *data)
{
	struct wpa_get_link_auth_ctx *ctx = data;

	if (!wpa_auth->is_ml ||
	    !ether_addr_equal(wpa_auth->mld_addr, ctx->addr) ||
	    !wpa_auth->primary_auth)
		return 0;

	ctx->wpa_auth = wpa_auth;
	return 1;
}

#endif /* CONFIG_IEEE80211BE */


static struct wpa_authenticator *
wpa_get_primary_auth(struct wpa_authenticator *wpa_auth)
{
#ifdef CONFIG_IEEE80211BE
	struct wpa_get_link_auth_ctx ctx;

	if (!wpa_auth || !wpa_auth->is_ml || wpa_auth->primary_auth)
		return wpa_auth;

	ctx.addr = wpa_auth->mld_addr;
	ctx.wpa_auth = NULL;
	wpa_auth_for_each_auth(wpa_auth, wpa_get_primary_auth_cb, &ctx);

	return ctx.wpa_auth;
#else /* CONFIG_IEEE80211BE */
	return wpa_auth;
#endif /* CONFIG_IEEE80211BE */
}


static inline int wpa_auth_mic_failure_report(
	struct wpa_authenticator *wpa_auth, const u8 *addr)
{
	if (wpa_auth->cb->mic_failure_report)
		return wpa_auth->cb->mic_failure_report(wpa_auth->cb_ctx, addr);
	return 0;
}


static inline void wpa_auth_psk_failure_report(
	struct wpa_authenticator *wpa_auth, const u8 *addr)
{
	if (wpa_auth->cb->psk_failure_report)
		wpa_auth->cb->psk_failure_report(wpa_auth->cb_ctx, addr);
}


static inline void wpa_auth_set_eapol(struct wpa_authenticator *wpa_auth,
				      const u8 *addr, wpa_eapol_variable var,
				      int value)
{
	if (wpa_auth->cb->set_eapol)
		wpa_auth->cb->set_eapol(wpa_auth->cb_ctx, addr, var, value);
}


static inline int wpa_auth_get_eapol(struct wpa_authenticator *wpa_auth,
				     const u8 *addr, wpa_eapol_variable var)
{
	if (!wpa_auth->cb->get_eapol)
		return -1;
	return wpa_auth->cb->get_eapol(wpa_auth->cb_ctx, addr, var);
}


static inline const u8 * wpa_auth_get_psk(struct wpa_authenticator *wpa_auth,
					  const u8 *addr,
					  const u8 *p2p_dev_addr,
					  const u8 *prev_psk, size_t *psk_len,
					  int *vlan_id)
{
	if (!wpa_auth->cb->get_psk)
		return NULL;
	return wpa_auth->cb->get_psk(wpa_auth->cb_ctx, addr, p2p_dev_addr,
				     prev_psk, psk_len, vlan_id);
}


static inline int wpa_auth_get_msk(struct wpa_authenticator *wpa_auth,
				   const u8 *addr, u8 *msk, size_t *len)
{
	if (!wpa_auth->cb->get_msk)
		return -1;
	return wpa_auth->cb->get_msk(wpa_auth->cb_ctx, addr, msk, len);
}


static inline int wpa_auth_set_key(struct wpa_authenticator *wpa_auth,
				   int vlan_id,
				   enum wpa_alg alg, const u8 *addr, int idx,
				   u8 *key, size_t key_len,
				   enum key_flag key_flag)
{
	if (!wpa_auth->cb->set_key)
		return -1;
	return wpa_auth->cb->set_key(wpa_auth->cb_ctx, vlan_id, alg, addr, idx,
				     key, key_len, key_flag);
}


#ifdef CONFIG_PASN
static inline int wpa_auth_set_ltf_keyseed(struct wpa_authenticator *wpa_auth,
					   const u8 *peer_addr,
					   const u8 *ltf_keyseed,
					   size_t ltf_keyseed_len)
{
	if (!wpa_auth->cb->set_ltf_keyseed)
		return -1;
	return wpa_auth->cb->set_ltf_keyseed(wpa_auth->cb_ctx, peer_addr,
					     ltf_keyseed, ltf_keyseed_len);
}
#endif /* CONFIG_PASN */


static inline int wpa_auth_get_seqnum(struct wpa_authenticator *wpa_auth,
				      const u8 *addr, int idx, u8 *seq)
{
	int res;

	if (!wpa_auth->cb->get_seqnum)
		return -1;
#ifdef CONFIG_TESTING_OPTIONS
	os_memset(seq, 0, WPA_KEY_RSC_LEN);
#endif /* CONFIG_TESTING_OPTIONS */
	res = wpa_auth->cb->get_seqnum(wpa_auth->cb_ctx, addr, idx, seq);
#ifdef CONFIG_TESTING_OPTIONS
	if (!addr && idx < 4 && wpa_auth->conf.gtk_rsc_override_set) {
		wpa_printf(MSG_DEBUG,
			   "TESTING: Override GTK RSC %016llx --> %016llx",
			   (long long unsigned) WPA_GET_LE64(seq),
			   (long long unsigned)
			   WPA_GET_LE64(wpa_auth->conf.gtk_rsc_override));
		os_memcpy(seq, wpa_auth->conf.gtk_rsc_override,
			  WPA_KEY_RSC_LEN);
	}
	if (!addr && idx >= 4 && idx <= 5 &&
	    wpa_auth->conf.igtk_rsc_override_set) {
		wpa_printf(MSG_DEBUG,
			   "TESTING: Override IGTK RSC %016llx --> %016llx",
			   (long long unsigned) WPA_GET_LE64(seq),
			   (long long unsigned)
			   WPA_GET_LE64(wpa_auth->conf.igtk_rsc_override));
		os_memcpy(seq, wpa_auth->conf.igtk_rsc_override,
			  WPA_KEY_RSC_LEN);
	}
#endif /* CONFIG_TESTING_OPTIONS */
	return res;
}


static inline int
wpa_auth_send_eapol(struct wpa_authenticator *wpa_auth, const u8 *addr,
		    const u8 *data, size_t data_len, int encrypt)
{
	if (!wpa_auth->cb->send_eapol)
		return -1;
	return wpa_auth->cb->send_eapol(wpa_auth->cb_ctx, addr, data, data_len,
					encrypt);
}


#ifdef CONFIG_MESH
static inline int wpa_auth_start_ampe(struct wpa_authenticator *wpa_auth,
				      const u8 *addr)
{
	if (!wpa_auth->cb->start_ampe)
		return -1;
	return wpa_auth->cb->start_ampe(wpa_auth->cb_ctx, addr);
}
#endif /* CONFIG_MESH */


int wpa_auth_for_each_sta(struct wpa_authenticator *wpa_auth,
			  int (*cb)(struct wpa_state_machine *sm, void *ctx),
			  void *cb_ctx)
{
	if (!wpa_auth->cb->for_each_sta)
		return 0;
	return wpa_auth->cb->for_each_sta(wpa_auth->cb_ctx, cb, cb_ctx);
}


int wpa_auth_for_each_auth(struct wpa_authenticator *wpa_auth,
			   int (*cb)(struct wpa_authenticator *a, void *ctx),
			   void *cb_ctx)
{
	if (!wpa_auth->cb->for_each_auth)
		return 0;
	return wpa_auth->cb->for_each_auth(wpa_auth->cb_ctx, cb, cb_ctx);
}


void wpa_auth_store_ptksa(struct wpa_authenticator *wpa_auth,
			  const u8 *addr, int cipher,
			  u32 life_time, const struct wpa_ptk *ptk)
{
	if (wpa_auth->cb->store_ptksa)
		wpa_auth->cb->store_ptksa(wpa_auth->cb_ctx, addr, cipher,
					  life_time, ptk);
}


static void wpa_auth_remove_ptksa(struct wpa_authenticator *wpa_auth,
				  const u8 *addr, int cipher)
{
	if (wpa_auth->cb->clear_ptksa)
		wpa_auth->cb->clear_ptksa(wpa_auth->cb_ctx, addr, cipher);
}


void wpa_auth_logger(struct wpa_authenticator *wpa_auth, const u8 *addr,
		     logger_level level, const char *txt)
{
	if (!wpa_auth->cb->logger)
		return;
	wpa_auth->cb->logger(wpa_auth->cb_ctx, addr, level, txt);
}


void wpa_auth_vlogger(struct wpa_authenticator *wpa_auth, const u8 *addr,
		      logger_level level, const char *fmt, ...)
{
	char *format;
	int maxlen;
	va_list ap;

	if (!wpa_auth->cb->logger)
		return;

	maxlen = os_strlen(fmt) + 100;
	format = os_malloc(maxlen);
	if (!format)
		return;

	va_start(ap, fmt);
	vsnprintf(format, maxlen, fmt, ap);
	va_end(ap);

	wpa_auth_logger(wpa_auth, addr, level, format);

	os_free(format);
}


static void wpa_sta_disconnect(struct wpa_authenticator *wpa_auth,
			       const u8 *addr, u16 reason)
{
	if (!wpa_auth->cb->disconnect)
		return;
	wpa_printf(MSG_DEBUG, "wpa_sta_disconnect STA " MACSTR " (reason %u)",
		   MAC2STR(addr), reason);
	wpa_auth->cb->disconnect(wpa_auth->cb_ctx, addr, reason);
}


#ifdef CONFIG_OCV
static int wpa_channel_info(struct wpa_authenticator *wpa_auth,
			    struct wpa_channel_info *ci)
{
	if (!wpa_auth->cb->channel_info)
		return -1;
	return wpa_auth->cb->channel_info(wpa_auth->cb_ctx, ci);
}
#endif /* CONFIG_OCV */


static int wpa_auth_update_vlan(struct wpa_authenticator *wpa_auth,
				const u8 *addr, int vlan_id)
{
	if (!wpa_auth->cb->update_vlan)
		return -1;
	return wpa_auth->cb->update_vlan(wpa_auth->cb_ctx, addr, vlan_id);
}


static void wpa_rekey_gmk(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_authenticator *wpa_auth = eloop_ctx;

	if (random_get_bytes(wpa_auth->group->GMK, WPA_GMK_LEN)) {
		wpa_printf(MSG_ERROR,
			   "Failed to get random data for WPA initialization.");
	} else {
		wpa_auth_logger(wpa_auth, NULL, LOGGER_DEBUG, "GMK rekeyd");
		wpa_hexdump_key(MSG_DEBUG, "GMK",
				wpa_auth->group->GMK, WPA_GMK_LEN);
	}

	if (wpa_auth->conf.wpa_gmk_rekey) {
		eloop_register_timeout(wpa_auth->conf.wpa_gmk_rekey, 0,
				       wpa_rekey_gmk, wpa_auth, NULL);
	}
}


static void wpa_rekey_all_groups(struct wpa_authenticator *wpa_auth)
{
	struct wpa_group *group, *next;

	wpa_auth_logger(wpa_auth, NULL, LOGGER_DEBUG, "rekeying GTK");
	group = wpa_auth->group;
	while (group) {
		wpa_printf(MSG_DEBUG, "GTK rekey start for authenticator ("
			   MACSTR "), group vlan %d",
			   MAC2STR(wpa_auth->addr), group->vlan_id);
		wpa_group_get(wpa_auth, group);

		group->GTKReKey = true;
		do {
			group->changed = false;
			wpa_group_sm_step(wpa_auth, group);
		} while (group->changed);

		next = group->next;
		wpa_group_put(wpa_auth, group);
		group = next;
	}
}


#ifdef CONFIG_IEEE80211BE

static void wpa_update_all_gtks(struct wpa_authenticator *wpa_auth)
{
	struct wpa_group *group, *next;

	group = wpa_auth->group;
	while (group) {
		wpa_group_get(wpa_auth, group);

		wpa_group_update_gtk(wpa_auth, group);
		next = group->next;
		wpa_group_put(wpa_auth, group);
		group = next;
	}
}


static int wpa_update_all_gtks_cb(struct wpa_authenticator *wpa_auth, void *ctx)
{
	const u8 *mld_addr = ctx;

	if (!ether_addr_equal(wpa_auth->mld_addr, mld_addr))
		return 0;

	wpa_update_all_gtks(wpa_auth);
	return 0;
}


static int wpa_rekey_all_groups_cb(struct wpa_authenticator *wpa_auth,
				   void *ctx)
{
	const u8 *mld_addr = ctx;

	if (!ether_addr_equal(wpa_auth->mld_addr, mld_addr))
		return 0;

	wpa_rekey_all_groups(wpa_auth);
	return 0;
}

#endif /* CONFIG_IEEE80211BE */


static void wpa_rekey_gtk(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_authenticator *wpa_auth = eloop_ctx;

#ifdef CONFIG_IEEE80211BE
	if (wpa_auth->is_ml) {
		/* Non-primary ML authenticator eloop timer for group rekey is
		 * never started and shouldn't fire. Check and warn just in
		 * case. */
		if (!wpa_auth->primary_auth) {
			wpa_printf(MSG_DEBUG,
				   "RSN: Cannot start GTK rekey on non-primary ML authenticator");
			return;
		}

		/* Generate all the new group keys */
		wpa_auth_for_each_auth(wpa_auth, wpa_update_all_gtks_cb,
				       wpa_auth->mld_addr);

		/* Send all the generated group keys to the respective stations
		 * with group key handshake. */
		wpa_auth_for_each_auth(wpa_auth, wpa_rekey_all_groups_cb,
				       wpa_auth->mld_addr);
	} else {
		wpa_rekey_all_groups(wpa_auth);
	}
#else /* CONFIG_IEEE80211BE */
	wpa_rekey_all_groups(wpa_auth);
#endif /* CONFIG_IEEE80211BE */

	if (wpa_auth->conf.wpa_group_rekey) {
		eloop_register_timeout(wpa_auth->conf.wpa_group_rekey,
				       0, wpa_rekey_gtk, wpa_auth, NULL);
	}
}


static void wpa_rekey_ptk(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_authenticator *wpa_auth = eloop_ctx;
	struct wpa_state_machine *sm = timeout_ctx;

	wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_DEBUG,
			"rekeying PTK");
	wpa_request_new_ptk(sm);
	wpa_sm_step(sm);
}


void wpa_auth_set_ptk_rekey_timer(struct wpa_state_machine *sm)
{
	if (sm && sm->wpa_auth->conf.wpa_ptk_rekey) {
		wpa_printf(MSG_DEBUG, "WPA: Start PTK rekeying timer for "
			   MACSTR " (%d seconds)",
			   MAC2STR(wpa_auth_get_spa(sm)),
			   sm->wpa_auth->conf.wpa_ptk_rekey);
		eloop_cancel_timeout(wpa_rekey_ptk, sm->wpa_auth, sm);
		eloop_register_timeout(sm->wpa_auth->conf.wpa_ptk_rekey, 0,
				       wpa_rekey_ptk, sm->wpa_auth, sm);
	}
}


static int wpa_auth_pmksa_clear_cb(struct wpa_state_machine *sm, void *ctx)
{
	if (sm->pmksa == ctx)
		sm->pmksa = NULL;
	return 0;
}


static void wpa_auth_pmksa_free_cb(struct rsn_pmksa_cache_entry *entry,
				   void *ctx)
{
	struct wpa_authenticator *wpa_auth = ctx;
	wpa_auth_for_each_sta(wpa_auth, wpa_auth_pmksa_clear_cb, entry);
}


static int wpa_group_init_gmk_and_counter(struct wpa_authenticator *wpa_auth,
					  struct wpa_group *group)
{
	u8 buf[ETH_ALEN + 8 + sizeof(unsigned long)];
	u8 rkey[32];
	unsigned long ptr;

	if (random_get_bytes(group->GMK, WPA_GMK_LEN) < 0)
		return -1;
	wpa_hexdump_key(MSG_DEBUG, "GMK", group->GMK, WPA_GMK_LEN);

	/*
	 * Counter = PRF-256(Random number, "Init Counter",
	 *                   Local MAC Address || Time)
	 */
	os_memcpy(buf, wpa_auth->addr, ETH_ALEN);
	wpa_get_ntp_timestamp(buf + ETH_ALEN);
	ptr = (unsigned long) group;
	os_memcpy(buf + ETH_ALEN + 8, &ptr, sizeof(ptr));
#ifdef TEST_FUZZ
	os_memset(buf + ETH_ALEN, 0xab, 8);
	os_memset(buf + ETH_ALEN + 8, 0xcd, sizeof(ptr));
#endif /* TEST_FUZZ */
	if (random_get_bytes(rkey, sizeof(rkey)) < 0)
		return -1;

	if (sha1_prf(rkey, sizeof(rkey), "Init Counter", buf, sizeof(buf),
		     group->Counter, WPA_NONCE_LEN) < 0)
		return -1;
	wpa_hexdump_key(MSG_DEBUG, "Key Counter",
			group->Counter, WPA_NONCE_LEN);

	return 0;
}


static struct wpa_group * wpa_group_init(struct wpa_authenticator *wpa_auth,
					 int vlan_id, int delay_init)
{
	struct wpa_group *group;

	group = os_zalloc(sizeof(struct wpa_group));
	if (!group)
		return NULL;

	group->GTKAuthenticator = true;
	group->vlan_id = vlan_id;
	group->GTK_len = wpa_cipher_key_len(wpa_auth->conf.wpa_group);

	if (random_pool_ready() != 1) {
		wpa_printf(MSG_INFO,
			   "WPA: Not enough entropy in random pool for secure operations - update keys later when the first station connects");
	}

	/*
	 * Set initial GMK/Counter value here. The actual values that will be
	 * used in negotiations will be set once the first station tries to
	 * connect. This allows more time for collecting additional randomness
	 * on embedded devices.
	 */
	if (wpa_group_init_gmk_and_counter(wpa_auth, group) < 0) {
		wpa_printf(MSG_ERROR,
			   "Failed to get random data for WPA initialization.");
		os_free(group);
		return NULL;
	}

	group->GInit = true;
	if (delay_init) {
		wpa_printf(MSG_DEBUG,
			   "WPA: Delay group state machine start until Beacon frames have been configured");
		/* Initialization is completed in wpa_init_keys(). */
	} else {
		wpa_group_sm_step(wpa_auth, group);
		group->GInit = false;
		wpa_group_sm_step(wpa_auth, group);
	}

	return group;
}


/**
 * wpa_init - Initialize WPA authenticator
 * @addr: Authenticator address
 * @conf: Configuration for WPA authenticator
 * @cb: Callback functions for WPA authenticator
 * Returns: Pointer to WPA authenticator data or %NULL on failure
 */
struct wpa_authenticator * wpa_init(const u8 *addr,
				    struct wpa_auth_config *conf,
				    const struct wpa_auth_callbacks *cb,
				    void *cb_ctx)
{
	struct wpa_authenticator *wpa_auth;

	wpa_auth = os_zalloc(sizeof(struct wpa_authenticator));
	if (!wpa_auth)
		return NULL;

	os_memcpy(wpa_auth->addr, addr, ETH_ALEN);
	os_memcpy(&wpa_auth->conf, conf, sizeof(*conf));

#ifdef CONFIG_IEEE80211BE
	if (conf->mld_addr) {
		wpa_auth->is_ml = true;
		wpa_auth->link_id = conf->link_id;
		wpa_auth->primary_auth = !conf->first_link_auth;
		os_memcpy(wpa_auth->mld_addr, conf->mld_addr, ETH_ALEN);
	}
#endif /* CONFIG_IEEE80211BE */

	wpa_auth->cb = cb;
	wpa_auth->cb_ctx = cb_ctx;

	if (wpa_auth_gen_wpa_ie(wpa_auth)) {
		wpa_printf(MSG_ERROR, "Could not generate WPA IE.");
		os_free(wpa_auth);
		return NULL;
	}

	wpa_auth->group = wpa_group_init(wpa_auth, 0, 1);
	if (!wpa_auth->group) {
		os_free(wpa_auth->wpa_ie);
		os_free(wpa_auth);
		return NULL;
	}

	wpa_auth->pmksa = pmksa_cache_auth_init(wpa_auth_pmksa_free_cb,
						wpa_auth);
	if (!wpa_auth->pmksa) {
		wpa_printf(MSG_ERROR, "PMKSA cache initialization failed.");
		os_free(wpa_auth->group);
		os_free(wpa_auth->wpa_ie);
		os_free(wpa_auth);
		return NULL;
	}

#ifdef CONFIG_IEEE80211R_AP
	wpa_auth->ft_pmk_cache = wpa_ft_pmk_cache_init();
	if (!wpa_auth->ft_pmk_cache) {
		wpa_printf(MSG_ERROR, "FT PMK cache initialization failed.");
		os_free(wpa_auth->group);
		os_free(wpa_auth->wpa_ie);
		pmksa_cache_auth_deinit(wpa_auth->pmksa);
		os_free(wpa_auth);
		return NULL;
	}
#endif /* CONFIG_IEEE80211R_AP */

	if (wpa_auth->conf.wpa_gmk_rekey) {
		eloop_register_timeout(wpa_auth->conf.wpa_gmk_rekey, 0,
				       wpa_rekey_gmk, wpa_auth, NULL);
	}

#ifdef CONFIG_IEEE80211BE
	/* For AP MLD, run group rekey timer only on one link (first) and
	 * whenever it fires do rekey on all associated ML links in one shot.
	 */
	if ((!wpa_auth->is_ml || !conf->first_link_auth) &&
	    wpa_auth->conf.wpa_group_rekey) {
#else /* CONFIG_IEEE80211BE */
	if (wpa_auth->conf.wpa_group_rekey) {
#endif /* CONFIG_IEEE80211BE */
		eloop_register_timeout(wpa_auth->conf.wpa_group_rekey, 0,
				       wpa_rekey_gtk, wpa_auth, NULL);
	}

#ifdef CONFIG_P2P
	if (WPA_GET_BE32(conf->ip_addr_start)) {
		int count = WPA_GET_BE32(conf->ip_addr_end) -
			WPA_GET_BE32(conf->ip_addr_start) + 1;
		if (count > 1000)
			count = 1000;
		if (count > 0)
			wpa_auth->ip_pool = bitfield_alloc(count);
	}
#endif /* CONFIG_P2P */

	if (conf->tx_bss_auth && conf->beacon_prot) {
		conf->tx_bss_auth->non_tx_beacon_prot = true;
		if (!conf->tx_bss_auth->conf.beacon_prot)
			conf->tx_bss_auth->conf.beacon_prot = true;
		if (!conf->tx_bss_auth->conf.group_mgmt_cipher)
			conf->tx_bss_auth->conf.group_mgmt_cipher =
				conf->group_mgmt_cipher;
	}

	return wpa_auth;
}


int wpa_init_keys(struct wpa_authenticator *wpa_auth)
{
	struct wpa_group *group = wpa_auth->group;

	wpa_printf(MSG_DEBUG,
		   "WPA: Start group state machine to set initial keys");
	wpa_group_sm_step(wpa_auth, group);
	group->GInit = false;
	wpa_group_sm_step(wpa_auth, group);
	if (group->wpa_group_state == WPA_GROUP_FATAL_FAILURE)
		return -1;
	return 0;
}


static void wpa_auth_free_conf(struct wpa_auth_config *conf)
{
#ifdef CONFIG_TESTING_OPTIONS
	wpabuf_free(conf->eapol_m1_elements);
	conf->eapol_m1_elements = NULL;
	wpabuf_free(conf->eapol_m3_elements);
	conf->eapol_m3_elements = NULL;
#endif /* CONFIG_TESTING_OPTIONS */
}


/**
 * wpa_deinit - Deinitialize WPA authenticator
 * @wpa_auth: Pointer to WPA authenticator data from wpa_init()
 */
void wpa_deinit(struct wpa_authenticator *wpa_auth)
{
	struct wpa_group *group, *prev;

	eloop_cancel_timeout(wpa_rekey_gmk, wpa_auth, NULL);

	/* TODO: Assign ML primary authenticator to next link authenticator and
	 * start rekey timer. */
	eloop_cancel_timeout(wpa_rekey_gtk, wpa_auth, NULL);

	pmksa_cache_auth_deinit(wpa_auth->pmksa);

#ifdef CONFIG_IEEE80211R_AP
	wpa_ft_pmk_cache_deinit(wpa_auth->ft_pmk_cache);
	wpa_auth->ft_pmk_cache = NULL;
	wpa_ft_deinit(wpa_auth);
#endif /* CONFIG_IEEE80211R_AP */

#ifdef CONFIG_P2P
	bitfield_free(wpa_auth->ip_pool);
#endif /* CONFIG_P2P */


	os_free(wpa_auth->wpa_ie);

	group = wpa_auth->group;
	while (group) {
		prev = group;
		group = group->next;
		bin_clear_free(prev, sizeof(*prev));
	}

	wpa_auth_free_conf(&wpa_auth->conf);
	os_free(wpa_auth);
}


/**
 * wpa_reconfig - Update WPA authenticator configuration
 * @wpa_auth: Pointer to WPA authenticator data from wpa_init()
 * @conf: Configuration for WPA authenticator
 */
int wpa_reconfig(struct wpa_authenticator *wpa_auth,
		 struct wpa_auth_config *conf)
{
	struct wpa_group *group;

	if (!wpa_auth)
		return 0;

	wpa_auth_free_conf(&wpa_auth->conf);
	os_memcpy(&wpa_auth->conf, conf, sizeof(*conf));
	if (wpa_auth_gen_wpa_ie(wpa_auth)) {
		wpa_printf(MSG_ERROR, "Could not generate WPA IE.");
		return -1;
	}

	/*
	 * Reinitialize GTK to make sure it is suitable for the new
	 * configuration.
	 */
	group = wpa_auth->group;
	group->GTK_len = wpa_cipher_key_len(wpa_auth->conf.wpa_group);
	group->GInit = true;
	wpa_group_sm_step(wpa_auth, group);
	group->GInit = false;
	wpa_group_sm_step(wpa_auth, group);

	return 0;
}


struct wpa_state_machine *
wpa_auth_sta_init(struct wpa_authenticator *wpa_auth, const u8 *addr,
		  const u8 *p2p_dev_addr)
{
	struct wpa_state_machine *sm;

	if (wpa_auth->group->wpa_group_state == WPA_GROUP_FATAL_FAILURE)
		return NULL;

	sm = os_zalloc(sizeof(struct wpa_state_machine));
	if (!sm)
		return NULL;
	os_memcpy(sm->addr, addr, ETH_ALEN);
	if (p2p_dev_addr)
		os_memcpy(sm->p2p_dev_addr, p2p_dev_addr, ETH_ALEN);

	sm->wpa_auth = wpa_auth;
	sm->group = wpa_auth->group;
	wpa_group_get(sm->wpa_auth, sm->group);
#ifdef CONFIG_IEEE80211BE
	sm->mld_assoc_link_id = -1;
#endif /* CONFIG_IEEE80211BE */

	return sm;
}


int wpa_auth_sta_associated(struct wpa_authenticator *wpa_auth,
			    struct wpa_state_machine *sm)
{
	if (!wpa_auth || !wpa_auth->conf.wpa || !sm)
		return -1;

#ifdef CONFIG_IEEE80211R_AP
	if (sm->ft_completed) {
		wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_DEBUG,
				"FT authentication already completed - do not start 4-way handshake");
		/* Go to PTKINITDONE state to allow GTK rekeying */
		sm->wpa_ptk_state = WPA_PTK_PTKINITDONE;
		sm->Pair = true;
		return 0;
	}
#endif /* CONFIG_IEEE80211R_AP */

#ifdef CONFIG_FILS
	if (sm->fils_completed) {
		wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_DEBUG,
				"FILS authentication already completed - do not start 4-way handshake");
		/* Go to PTKINITDONE state to allow GTK rekeying */
		sm->wpa_ptk_state = WPA_PTK_PTKINITDONE;
		sm->Pair = true;
		return 0;
	}
#endif /* CONFIG_FILS */

	if (sm->started) {
		os_memset(&sm->key_replay, 0, sizeof(sm->key_replay));
		sm->ReAuthenticationRequest = true;
		return wpa_sm_step(sm);
	}

	wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_DEBUG,
			"start authentication");
	sm->started = 1;

	sm->Init = true;
	if (wpa_sm_step(sm) == 1)
		return 1; /* should not really happen */
	sm->Init = false;
	sm->AuthenticationRequest = true;
	return wpa_sm_step(sm);
}


void wpa_auth_sta_no_wpa(struct wpa_state_machine *sm)
{
	/* WPA/RSN was not used - clear WPA state. This is needed if the STA
	 * reassociates back to the same AP while the previous entry for the
	 * STA has not yet been removed. */
	if (!sm)
		return;

	sm->wpa_key_mgmt = 0;
}


static void wpa_free_sta_sm(struct wpa_state_machine *sm)
{
#ifdef CONFIG_IEEE80211BE
	int link_id;
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_P2P
	if (WPA_GET_BE32(sm->ip_addr)) {
		wpa_printf(MSG_DEBUG,
			   "P2P: Free assigned IP address %u.%u.%u.%u from "
			   MACSTR " (bit %u)",
			   sm->ip_addr[0], sm->ip_addr[1],
			   sm->ip_addr[2], sm->ip_addr[3],
			   MAC2STR(wpa_auth_get_spa(sm)),
			   sm->ip_addr_bit);
		bitfield_clear(sm->wpa_auth->ip_pool, sm->ip_addr_bit);
	}
#endif /* CONFIG_P2P */
	if (sm->GUpdateStationKeys)
		wpa_gkeydone_sta(sm);
#ifdef CONFIG_IEEE80211R_AP
	os_free(sm->assoc_resp_ftie);
	wpabuf_free(sm->ft_pending_req_ies);
#endif /* CONFIG_IEEE80211R_AP */
	os_free(sm->last_rx_eapol_key);
	os_free(sm->wpa_ie);
	os_free(sm->rsnxe);
#ifdef CONFIG_IEEE80211BE
	for_each_sm_auth(sm, link_id) {
		wpa_group_put(sm->mld_links[link_id].wpa_auth,
			      sm->mld_links[link_id].wpa_auth->group);
		sm->mld_links[link_id].wpa_auth = NULL;
	}
#endif /* CONFIG_IEEE80211BE */
	wpa_group_put(sm->wpa_auth, sm->group);
#ifdef CONFIG_DPP2
	wpabuf_clear_free(sm->dpp_z);
#endif /* CONFIG_DPP2 */
	bin_clear_free(sm, sizeof(*sm));
}


void wpa_auth_sta_deinit(struct wpa_state_machine *sm)
{
	struct wpa_authenticator *wpa_auth;

	if (!sm)
		return;

	wpa_auth = sm->wpa_auth;
	if (wpa_auth->conf.wpa_strict_rekey && sm->has_GTK) {
		struct wpa_authenticator *primary_auth = wpa_auth;

		wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_DEBUG,
				"strict rekeying - force GTK rekey since STA is leaving");

#ifdef CONFIG_IEEE80211BE
		if (wpa_auth->is_ml && !wpa_auth->primary_auth)
			primary_auth = wpa_get_primary_auth(wpa_auth);
#endif /* CONFIG_IEEE80211BE */

		if (eloop_deplete_timeout(0, 500000, wpa_rekey_gtk,
					  primary_auth, NULL) == -1)
			eloop_register_timeout(0, 500000, wpa_rekey_gtk,
					       primary_auth, NULL);
	}

	eloop_cancel_timeout(wpa_send_eapol_timeout, wpa_auth, sm);
	sm->pending_1_of_4_timeout = 0;
	eloop_cancel_timeout(wpa_sm_call_step, sm, NULL);
	eloop_cancel_timeout(wpa_rekey_ptk, wpa_auth, sm);
#ifdef CONFIG_IEEE80211R_AP
	wpa_ft_sta_deinit(sm);
#endif /* CONFIG_IEEE80211R_AP */
	if (sm->in_step_loop) {
		/* Must not free state machine while wpa_sm_step() is running.
		 * Freeing will be completed in the end of wpa_sm_step(). */
		wpa_printf(MSG_DEBUG,
			   "WPA: Registering pending STA state machine deinit for "
			   MACSTR, MAC2STR(wpa_auth_get_spa(sm)));
		sm->pending_deinit = 1;
	} else
		wpa_free_sta_sm(sm);
}


static void wpa_request_new_ptk(struct wpa_state_machine *sm)
{
	if (!sm)
		return;

	if (!sm->use_ext_key_id && sm->wpa_auth->conf.wpa_deny_ptk0_rekey) {
		wpa_printf(MSG_INFO,
			   "WPA: PTK0 rekey not allowed, disconnect " MACSTR,
			   MAC2STR(wpa_auth_get_spa(sm)));
		sm->Disconnect = true;
		/* Try to encourage the STA to reconnect */
		sm->disconnect_reason =
			WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA;
	} else {
		if (sm->use_ext_key_id)
			sm->keyidx_active ^= 1; /* flip Key ID */
		sm->PTKRequest = true;
		sm->PTK_valid = 0;
	}
}


static int wpa_replay_counter_valid(struct wpa_key_replay_counter *ctr,
				    const u8 *replay_counter)
{
	int i;
	for (i = 0; i < RSNA_MAX_EAPOL_RETRIES; i++) {
		if (!ctr[i].valid)
			break;
		if (os_memcmp(replay_counter, ctr[i].counter,
			      WPA_REPLAY_COUNTER_LEN) == 0)
			return 1;
	}
	return 0;
}


static void wpa_replay_counter_mark_invalid(struct wpa_key_replay_counter *ctr,
					    const u8 *replay_counter)
{
	int i;
	for (i = 0; i < RSNA_MAX_EAPOL_RETRIES; i++) {
		if (ctr[i].valid &&
		    (!replay_counter ||
		     os_memcmp(replay_counter, ctr[i].counter,
			       WPA_REPLAY_COUNTER_LEN) == 0))
			ctr[i].valid = false;
	}
}


#ifdef CONFIG_IEEE80211R_AP
static int ft_check_msg_2_of_4(struct wpa_authenticator *wpa_auth,
			       struct wpa_state_machine *sm,
			       struct wpa_eapol_ie_parse *kde)
{
	struct wpa_ie_data ie, assoc_ie;
	struct rsn_mdie *mdie;
	unsigned int i, j;
	bool found = false;

	/* Verify that PMKR1Name from EAPOL-Key message 2/4 matches the value
	 * we derived. */

	if (wpa_parse_wpa_ie_rsn(kde->rsn_ie, kde->rsn_ie_len, &ie) < 0 ||
	    ie.num_pmkid < 1 || !ie.pmkid) {
		wpa_printf(MSG_DEBUG,
			   "FT: No PMKR1Name in FT 4-way handshake message 2/4");
		return -1;
	}

	if (wpa_parse_wpa_ie_rsn(sm->wpa_ie, sm->wpa_ie_len, &assoc_ie) < 0) {
		wpa_printf(MSG_DEBUG,
			   "FT: Could not parse (Re)Association Request frame RSNE");
		os_memset(&assoc_ie, 0, sizeof(assoc_ie));
		/* Continue to allow PMKR1Name matching to be done to cover the
		 * case where it is the only listed PMKID. */
	}

	for (i = 0; i < ie.num_pmkid; i++) {
		const u8 *pmkid = ie.pmkid + i * PMKID_LEN;

		if (os_memcmp_const(pmkid, sm->pmk_r1_name,
				    WPA_PMK_NAME_LEN) == 0) {
			wpa_printf(MSG_DEBUG,
				   "FT: RSNE[PMKID[%u]] from supplicant matches PMKR1Name",
				   i);
			found = true;
		} else {
			for (j = 0; j < assoc_ie.num_pmkid; j++) {
				if (os_memcmp(pmkid,
					      assoc_ie.pmkid + j * PMKID_LEN,
					      PMKID_LEN) == 0)
					break;
			}

			if (j == assoc_ie.num_pmkid) {
				wpa_printf(MSG_DEBUG,
					   "FT: RSNE[PMKID[%u]] from supplicant is neither PMKR1Name nor included in AssocReq",
					   i);
				found = false;
				break;
			}
			wpa_printf(MSG_DEBUG,
				   "FT: RSNE[PMKID[%u]] from supplicant is not PMKR1Name, but matches a PMKID in AssocReq",
				   i);
		}
	}

	if (!found) {
		wpa_auth_logger(sm->wpa_auth, wpa_auth_get_spa(sm),
				LOGGER_DEBUG,
				"PMKR1Name mismatch in FT 4-way handshake");
		wpa_hexdump(MSG_DEBUG,
			    "FT: PMKIDs/PMKR1Name from Supplicant",
			    ie.pmkid, ie.num_pmkid * PMKID_LEN);
		wpa_hexdump(MSG_DEBUG, "FT: Derived PMKR1Name",
			    sm->pmk_r1_name, WPA_PMK_NAME_LEN);
		return -1;
	}

	if (!kde->mdie || !kde->ftie) {
		wpa_printf(MSG_DEBUG,
			   "FT: No %s in FT 4-way handshake message 2/4",
			   kde->mdie ? "FTIE" : "MDIE");
		return -1;
	}

	mdie = (struct rsn_mdie *) (kde->mdie + 2);
	if (kde->mdie[1] < sizeof(struct rsn_mdie) ||
	    os_memcmp(wpa_auth->conf.mobility_domain, mdie->mobility_domain,
		      MOBILITY_DOMAIN_ID_LEN) != 0) {
		wpa_printf(MSG_DEBUG, "FT: MDIE mismatch");
		return -1;
	}

	if (sm->assoc_resp_ftie &&
	    (kde->ftie[1] != sm->assoc_resp_ftie[1] ||
	     os_memcmp(kde->ftie, sm->assoc_resp_ftie,
		       2 + sm->assoc_resp_ftie[1]) != 0)) {
		wpa_printf(MSG_DEBUG, "FT: FTIE mismatch");
		wpa_hexdump(MSG_DEBUG, "FT: FTIE in EAPOL-Key msg 2/4",
			    kde->ftie, kde->ftie_len);
		wpa_hexdump(MSG_DEBUG, "FT: FTIE in (Re)AssocResp",
			    sm->assoc_resp_ftie, 2 + sm->assoc_resp_ftie[1]);
		return -1;
	}

	return 0;
}
#endif /* CONFIG_IEEE80211R_AP */


static int wpa_receive_error_report(struct wpa_authenticator *wpa_auth,
				    struct wpa_state_machine *sm, int group)
{
	/* Supplicant reported a Michael MIC error */
	wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_INFO,
			 "received EAPOL-Key Error Request (STA detected Michael MIC failure (group=%d))",
			 group);

	if (group && wpa_auth->conf.wpa_group != WPA_CIPHER_TKIP) {
		wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_INFO,
				"ignore Michael MIC failure report since group cipher is not TKIP");
	} else if (!group && sm->pairwise != WPA_CIPHER_TKIP) {
		wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_INFO,
				"ignore Michael MIC failure report since pairwise cipher is not TKIP");
	} else {
		if (wpa_auth_mic_failure_report(wpa_auth,
						wpa_auth_get_spa(sm)) > 0)
			return 1; /* STA entry was removed */
		sm->dot11RSNAStatsTKIPRemoteMICFailures++;
		wpa_auth->dot11RSNAStatsTKIPRemoteMICFailures++;
	}

	/*
	 * Error report is not a request for a new key handshake, but since
	 * Authenticator may do it, let's change the keys now anyway.
	 */
	wpa_request_new_ptk(sm);
	return 0;
}


static int wpa_try_alt_snonce(struct wpa_state_machine *sm, u8 *data,
			      size_t data_len)
{
	struct wpa_ptk PTK;
	int ok = 0;
	const u8 *pmk = NULL;
	size_t pmk_len;
	int vlan_id = 0;
	u8 pmk_r0[PMK_LEN_MAX], pmk_r0_name[WPA_PMK_NAME_LEN];
	u8 pmk_r1[PMK_LEN_MAX];
	size_t key_len;
	int ret = -1;

	os_memset(&PTK, 0, sizeof(PTK));
	for (;;) {
		if (wpa_key_mgmt_wpa_psk(sm->wpa_key_mgmt) &&
		    !wpa_key_mgmt_sae(sm->wpa_key_mgmt)) {
			pmk = wpa_auth_get_psk(sm->wpa_auth, sm->addr,
					       sm->p2p_dev_addr, pmk, &pmk_len,
					       &vlan_id);
			if (!pmk)
				break;
#ifdef CONFIG_IEEE80211R_AP
			if (wpa_key_mgmt_ft_psk(sm->wpa_key_mgmt)) {
				os_memcpy(sm->xxkey, pmk, pmk_len);
				sm->xxkey_len = pmk_len;
			}
#endif /* CONFIG_IEEE80211R_AP */
		} else {
			pmk = sm->PMK;
			pmk_len = sm->pmk_len;
		}

		if (wpa_derive_ptk(sm, sm->alt_SNonce, pmk, pmk_len, &PTK, 0,
				   pmk_r0, pmk_r1, pmk_r0_name, &key_len,
				   false) < 0)
			break;

		if (wpa_verify_key_mic(sm->wpa_key_mgmt, pmk_len, &PTK,
				       data, data_len) == 0) {
			if (sm->PMK != pmk) {
				os_memcpy(sm->PMK, pmk, pmk_len);
				sm->pmk_len = pmk_len;
			}
			ok = 1;
			break;
		}

		if (!wpa_key_mgmt_wpa_psk(sm->wpa_key_mgmt) ||
		    wpa_key_mgmt_sae(sm->wpa_key_mgmt))
			break;
	}

	if (!ok) {
		wpa_printf(MSG_DEBUG,
			   "WPA: Earlier SNonce did not result in matching MIC");
		goto fail;
	}

	wpa_printf(MSG_DEBUG,
		   "WPA: Earlier SNonce resulted in matching MIC");
	sm->alt_snonce_valid = 0;

	if (vlan_id && wpa_key_mgmt_wpa_psk(sm->wpa_key_mgmt) &&
	    wpa_auth_update_vlan(sm->wpa_auth, sm->addr, vlan_id) < 0)
		goto fail;

#ifdef CONFIG_IEEE80211R_AP
	if (wpa_key_mgmt_ft(sm->wpa_key_mgmt) && !sm->ft_completed) {
		wpa_printf(MSG_DEBUG, "FT: Store PMK-R0/PMK-R1");
		wpa_auth_ft_store_keys(sm, pmk_r0, pmk_r1, pmk_r0_name,
				       key_len);
	}
#endif /* CONFIG_IEEE80211R_AP */

	os_memcpy(sm->SNonce, sm->alt_SNonce, WPA_NONCE_LEN);
	os_memcpy(&sm->PTK, &PTK, sizeof(PTK));
	forced_memzero(&PTK, sizeof(PTK));
	sm->PTK_valid = true;

	ret = 0;
fail:
	forced_memzero(pmk_r0, sizeof(pmk_r0));
	forced_memzero(pmk_r1, sizeof(pmk_r1));
	return ret;
}


static bool wpa_auth_gtk_rekey_in_process(struct wpa_authenticator *wpa_auth)
{
	struct wpa_group *group;

	for (group = wpa_auth->group; group; group = group->next) {
		if (group->GKeyDoneStations)
			return true;
	}
	return false;
}


enum eapol_key_msg { PAIRWISE_2, PAIRWISE_4, GROUP_2, REQUEST };

static bool wpa_auth_valid_key_desc_ver(struct wpa_authenticator *wpa_auth,
					struct wpa_state_machine *sm, u16 ver)
{
	if (ver > WPA_KEY_INFO_TYPE_AES_128_CMAC) {
		wpa_printf(MSG_INFO, "RSN: " MACSTR
			   " used undefined Key Descriptor Version %d",
			   MAC2STR(wpa_auth_get_spa(sm)), ver);
		return false;
	}

	if (!wpa_use_akm_defined(sm->wpa_key_mgmt) &&
	    wpa_use_cmac(sm->wpa_key_mgmt) &&
	    ver != WPA_KEY_INFO_TYPE_AES_128_CMAC) {
		wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm),
				LOGGER_WARNING,
				"advertised support for AES-128-CMAC, but did not use it");
		return false;
	}

	if (sm->pairwise != WPA_CIPHER_TKIP &&
	    !wpa_use_akm_defined(sm->wpa_key_mgmt) &&
	    !wpa_use_cmac(sm->wpa_key_mgmt) &&
	    ver != WPA_KEY_INFO_TYPE_HMAC_SHA1_AES) {
		wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm),
				LOGGER_WARNING,
				"did not use HMAC-SHA1-AES with CCMP/GCMP");
		return false;
	}

	if (wpa_use_akm_defined(sm->wpa_key_mgmt) &&
	    ver != WPA_KEY_INFO_TYPE_AKM_DEFINED) {
		wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm),
				LOGGER_WARNING,
				"did not use EAPOL-Key descriptor version 0 as required for AKM-defined cases");
		return false;
	}

	return true;
}


static bool wpa_auth_valid_request_counter(struct wpa_authenticator *wpa_auth,
					   struct wpa_state_machine *sm,
					   const u8 *replay_counter)
{

	if (sm->req_replay_counter_used &&
	    os_memcmp(replay_counter, sm->req_replay_counter,
		      WPA_REPLAY_COUNTER_LEN) <= 0) {
		wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm),
				LOGGER_WARNING,
				"received EAPOL-Key request with replayed counter");
		return false;
	}

	return true;
}


static bool wpa_auth_valid_counter(struct wpa_authenticator *wpa_auth,
				   struct wpa_state_machine *sm,
				   const struct wpa_eapol_key *key,
				   enum eapol_key_msg msg,
				   const char *msgtxt)
{
	int i;

	if (msg == REQUEST)
		return wpa_auth_valid_request_counter(wpa_auth, sm,
						      key->replay_counter);

	if (wpa_replay_counter_valid(sm->key_replay, key->replay_counter))
		return true;

	if (msg == PAIRWISE_2 &&
	    wpa_replay_counter_valid(sm->prev_key_replay,
				     key->replay_counter) &&
	    sm->wpa_ptk_state == WPA_PTK_PTKINITNEGOTIATING &&
	    os_memcmp(sm->SNonce, key->key_nonce, WPA_NONCE_LEN) != 0) {
		/*
		 * Some supplicant implementations (e.g., Windows XP
		 * WZC) update SNonce for each EAPOL-Key 2/4. This
		 * breaks the workaround on accepting any of the
		 * pending requests, so allow the SNonce to be updated
		 * even if we have already sent out EAPOL-Key 3/4.
		 */
		wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm),
				 LOGGER_DEBUG,
				 "Process SNonce update from STA based on retransmitted EAPOL-Key 1/4");
		sm->update_snonce = 1;
		os_memcpy(sm->alt_SNonce, sm->SNonce, WPA_NONCE_LEN);
		sm->alt_snonce_valid = true;
		os_memcpy(sm->alt_replay_counter,
			  sm->key_replay[0].counter,
			  WPA_REPLAY_COUNTER_LEN);
		return true;
	}

	if (msg == PAIRWISE_4 && sm->alt_snonce_valid &&
	    sm->wpa_ptk_state == WPA_PTK_PTKINITNEGOTIATING &&
	    os_memcmp(key->replay_counter, sm->alt_replay_counter,
		      WPA_REPLAY_COUNTER_LEN) == 0) {
		/*
		 * Supplicant may still be using the old SNonce since
		 * there was two EAPOL-Key 2/4 messages and they had
		 * different SNonce values.
		 */
		wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm),
				 LOGGER_DEBUG,
				 "Try to process received EAPOL-Key 4/4 based on old Replay Counter and SNonce from an earlier EAPOL-Key 1/4");
		return true;
	}

	if (msg == PAIRWISE_2 &&
	    wpa_replay_counter_valid(sm->prev_key_replay,
				     key->replay_counter) &&
	    sm->wpa_ptk_state == WPA_PTK_PTKINITNEGOTIATING) {
		wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm),
				 LOGGER_DEBUG,
				 "ignore retransmitted EAPOL-Key %s - SNonce did not change",
				 msgtxt);
	} else {
		wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm),
				 LOGGER_DEBUG,
				 "received EAPOL-Key %s with unexpected replay counter",
				 msgtxt);
	}
	for (i = 0; i < RSNA_MAX_EAPOL_RETRIES; i++) {
		if (!sm->key_replay[i].valid)
			break;
		wpa_hexdump(MSG_DEBUG, "pending replay counter",
			    sm->key_replay[i].counter,
			    WPA_REPLAY_COUNTER_LEN);
	}
	wpa_hexdump(MSG_DEBUG, "received replay counter",
		    key->replay_counter, WPA_REPLAY_COUNTER_LEN);
	return false;
}


void wpa_receive(struct wpa_authenticator *wpa_auth,
		 struct wpa_state_machine *sm,
		 u8 *data, size_t data_len)
{
	struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *key;
	u16 key_info, ver, key_data_length;
	enum eapol_key_msg msg;
	const char *msgtxt;
	const u8 *key_data;
	size_t keyhdrlen, mic_len;
	u8 *mic;
	u8 *key_data_buf = NULL;
	size_t key_data_buf_len = 0;

	if (!wpa_auth || !wpa_auth->conf.wpa || !sm)
		return;

	wpa_hexdump(MSG_MSGDUMP, "WPA: RX EAPOL data", data, data_len);

	mic_len = wpa_mic_len(sm->wpa_key_mgmt, sm->pmk_len);
	keyhdrlen = sizeof(*key) + mic_len + 2;

	if (data_len < sizeof(*hdr) + keyhdrlen) {
		wpa_printf(MSG_DEBUG, "WPA: Ignore too short EAPOL-Key frame");
		return;
	}

	hdr = (struct ieee802_1x_hdr *) data;
	key = (struct wpa_eapol_key *) (hdr + 1);
	mic = (u8 *) (key + 1);
	key_info = WPA_GET_BE16(key->key_info);
	key_data = mic + mic_len + 2;
	key_data_length = WPA_GET_BE16(mic + mic_len);
	wpa_printf(MSG_DEBUG, "WPA: Received EAPOL-Key from " MACSTR
		   " key_info=0x%x type=%u mic_len=%zu key_data_length=%u",
		   MAC2STR(wpa_auth_get_spa(sm)), key_info, key->type,
		   mic_len, key_data_length);
	wpa_hexdump(MSG_MSGDUMP,
		    "WPA: EAPOL-Key header (ending before Key MIC)",
		    key, sizeof(*key));
	wpa_hexdump(MSG_MSGDUMP, "WPA: EAPOL-Key Key MIC",
		    mic, mic_len);
	if (key_data_length > data_len - sizeof(*hdr) - keyhdrlen) {
		wpa_printf(MSG_INFO,
			   "WPA: Invalid EAPOL-Key frame - key_data overflow (%d > %zu)",
			   key_data_length,
			   data_len - sizeof(*hdr) - keyhdrlen);
		return;
	}

	if (sm->wpa == WPA_VERSION_WPA2) {
		if (key->type == EAPOL_KEY_TYPE_WPA) {
			/*
			 * Some deployed station implementations seem to send
			 * msg 4/4 with incorrect type value in WPA2 mode.
			 */
			wpa_printf(MSG_DEBUG,
				   "Workaround: Allow EAPOL-Key with unexpected WPA type in RSN mode");
		} else if (key->type != EAPOL_KEY_TYPE_RSN) {
			wpa_printf(MSG_DEBUG,
				   "Ignore EAPOL-Key with unexpected type %d in RSN mode",
				   key->type);
			return;
		}
	} else {
		if (key->type != EAPOL_KEY_TYPE_WPA) {
			wpa_printf(MSG_DEBUG,
				   "Ignore EAPOL-Key with unexpected type %d in WPA mode",
				   key->type);
			return;
		}
	}

	wpa_hexdump(MSG_DEBUG, "WPA: Received Key Nonce", key->key_nonce,
		    WPA_NONCE_LEN);
	wpa_hexdump(MSG_DEBUG, "WPA: Received Replay Counter",
		    key->replay_counter, WPA_REPLAY_COUNTER_LEN);

	/* FIX: verify that the EAPOL-Key frame was encrypted if pairwise keys
	 * are set */

	if (key_info & WPA_KEY_INFO_SMK_MESSAGE) {
		wpa_printf(MSG_DEBUG, "WPA: Ignore SMK message");
		return;
	}

	ver = key_info & WPA_KEY_INFO_TYPE_MASK;
	if (!wpa_auth_valid_key_desc_ver(wpa_auth, sm, ver))
		goto out;
	if (mic_len > 0 && (key_info & WPA_KEY_INFO_ENCR_KEY_DATA) &&
	    sm->PTK_valid &&
	    (ver == WPA_KEY_INFO_TYPE_HMAC_SHA1_AES ||
	     ver == WPA_KEY_INFO_TYPE_AES_128_CMAC ||
	     wpa_use_aes_key_wrap(sm->wpa_key_mgmt)) &&
	    key_data_length >= 8 && key_data_length % 8 == 0) {
		key_data_length -= 8; /* AES-WRAP adds 8 bytes */
		key_data_buf = os_malloc(key_data_length);
		if (!key_data_buf)
			goto out;
		key_data_buf_len = key_data_length;
		if (aes_unwrap(sm->PTK.kek, sm->PTK.kek_len,
			       key_data_length / 8, key_data, key_data_buf)) {
			wpa_printf(MSG_INFO,
				   "RSN: AES unwrap failed - could not decrypt EAPOL-Key key data");
			goto out;
		}
		key_data = key_data_buf;
		wpa_hexdump_key(MSG_DEBUG, "RSN: Decrypted EAPOL-Key Key Data",
				key_data, key_data_length);
	}

	if (key_info & WPA_KEY_INFO_REQUEST) {
		msg = REQUEST;
		msgtxt = "Request";
	} else if (!(key_info & WPA_KEY_INFO_KEY_TYPE)) {
		msg = GROUP_2;
		msgtxt = "2/2 Group";
	} else if (key_data_length == 0 ||
		   (sm->wpa == WPA_VERSION_WPA2 &&
		    (!(key_info & WPA_KEY_INFO_ENCR_KEY_DATA) ||
		     key_data_buf) &&
		    (key_info & WPA_KEY_INFO_SECURE) &&
		    !get_ie(key_data, key_data_length, WLAN_EID_RSN)) ||
		   (mic_len == 0 && (key_info & WPA_KEY_INFO_ENCR_KEY_DATA) &&
		    key_data_length == AES_BLOCK_SIZE)) {
		msg = PAIRWISE_4;
		msgtxt = "4/4 Pairwise";
	} else {
		msg = PAIRWISE_2;
		msgtxt = "2/4 Pairwise";
	}

	if (!wpa_auth_valid_counter(wpa_auth, sm, key, msg, msgtxt))
		goto out;

#ifdef CONFIG_FILS
	if (sm->wpa == WPA_VERSION_WPA2 && mic_len == 0 &&
	    !(key_info & WPA_KEY_INFO_ENCR_KEY_DATA)) {
		wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_DEBUG,
				 "WPA: Encr Key Data bit not set even though AEAD cipher is supposed to be used - drop frame");
		goto out;
	}
#endif /* CONFIG_FILS */

	switch (msg) {
	case PAIRWISE_2:
		if (sm->wpa_ptk_state != WPA_PTK_PTKSTART &&
		    sm->wpa_ptk_state != WPA_PTK_PTKCALCNEGOTIATING &&
		    (!sm->update_snonce ||
		     sm->wpa_ptk_state != WPA_PTK_PTKINITNEGOTIATING)) {
			wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm),
					 LOGGER_INFO,
					 "received EAPOL-Key msg 2/4 in invalid state (%d) - dropped",
					 sm->wpa_ptk_state);
			goto out;
		}
		random_add_randomness(key->key_nonce, WPA_NONCE_LEN);
		if (sm->group->reject_4way_hs_for_entropy) {
			/*
			 * The system did not have enough entropy to generate
			 * strong random numbers. Reject the first 4-way
			 * handshake(s) and collect some entropy based on the
			 * information from it. Once enough entropy is
			 * available, the next atempt will trigger GMK/Key
			 * Counter update and the station will be allowed to
			 * continue.
			 */
			wpa_printf(MSG_DEBUG,
				   "WPA: Reject 4-way handshake to collect more entropy for random number generation");
			random_mark_pool_ready();
			wpa_sta_disconnect(wpa_auth, sm->addr,
					   WLAN_REASON_PREV_AUTH_NOT_VALID);
			goto out;
		}
		break;
	case PAIRWISE_4:
		if (sm->wpa_ptk_state != WPA_PTK_PTKINITNEGOTIATING ||
		    !sm->PTK_valid) {
			wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm),
					 LOGGER_INFO,
					 "received EAPOL-Key msg 4/4 in invalid state (%d) - dropped",
					 sm->wpa_ptk_state);
			goto out;
		}
		break;
	case GROUP_2:
		if (sm->wpa_ptk_group_state != WPA_PTK_GROUP_REKEYNEGOTIATING
		    || !sm->PTK_valid) {
			wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm),
					 LOGGER_INFO,
					 "received EAPOL-Key msg 2/2 in invalid state (%d) - dropped",
					 sm->wpa_ptk_group_state);
			goto out;
		}
		break;
	case REQUEST:
		if (sm->wpa_ptk_state == WPA_PTK_PTKSTART ||
		    sm->wpa_ptk_state == WPA_PTK_PTKCALCNEGOTIATING ||
		    sm->wpa_ptk_state == WPA_PTK_PTKCALCNEGOTIATING2 ||
		    sm->wpa_ptk_state == WPA_PTK_PTKINITNEGOTIATING) {
			wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm),
					 LOGGER_INFO,
					 "received EAPOL-Key Request in invalid state (%d) - dropped",
					 sm->wpa_ptk_state);
			goto out;
		}
		break;
	}

	wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_DEBUG,
			 "received EAPOL-Key frame (%s)", msgtxt);

	if (key_info & WPA_KEY_INFO_ACK) {
		wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_INFO,
				"received invalid EAPOL-Key: Key Ack set");
		goto out;
	}

	if (!wpa_key_mgmt_fils(sm->wpa_key_mgmt) &&
	    !(key_info & WPA_KEY_INFO_MIC)) {
		wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_INFO,
				"received invalid EAPOL-Key: Key MIC not set");
		goto out;
	}

#ifdef CONFIG_FILS
	if (wpa_key_mgmt_fils(sm->wpa_key_mgmt) &&
	    (key_info & WPA_KEY_INFO_MIC)) {
		wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_INFO,
				"received invalid EAPOL-Key: Key MIC set");
		goto out;
	}
#endif /* CONFIG_FILS */

	sm->MICVerified = false;
	if (sm->PTK_valid && !sm->update_snonce) {
		if (mic_len &&
		    wpa_verify_key_mic(sm->wpa_key_mgmt, sm->pmk_len, &sm->PTK,
				       data, data_len) &&
		    (msg != PAIRWISE_4 || !sm->alt_snonce_valid ||
		     wpa_try_alt_snonce(sm, data, data_len))) {
			wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm),
					LOGGER_INFO,
					"received EAPOL-Key with invalid MIC");
#ifdef TEST_FUZZ
			wpa_printf(MSG_INFO,
				   "TEST: Ignore Key MIC failure for fuzz testing");
			goto continue_fuzz;
#endif /* TEST_FUZZ */
			goto out;
		}
#ifdef CONFIG_FILS
		if (!mic_len &&
		    wpa_aead_decrypt(sm, &sm->PTK, data, data_len,
				     &key_data_length) < 0) {
			wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm),
					LOGGER_INFO,
					"received EAPOL-Key with invalid MIC");
#ifdef TEST_FUZZ
			wpa_printf(MSG_INFO,
				   "TEST: Ignore Key MIC failure for fuzz testing");
			goto continue_fuzz;
#endif /* TEST_FUZZ */
			goto out;
		}
#endif /* CONFIG_FILS */
#ifdef TEST_FUZZ
	continue_fuzz:
#endif /* TEST_FUZZ */
		sm->MICVerified = true;
		eloop_cancel_timeout(wpa_send_eapol_timeout, wpa_auth, sm);
		sm->pending_1_of_4_timeout = 0;
	}

	if (key_info & WPA_KEY_INFO_REQUEST) {
		if (!(key_info & WPA_KEY_INFO_SECURE)) {
			wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm),
					LOGGER_INFO,
					"received EAPOL-Key request without Secure=1");
			goto out;
		}
		if (sm->MICVerified) {
			sm->req_replay_counter_used = 1;
			os_memcpy(sm->req_replay_counter, key->replay_counter,
				  WPA_REPLAY_COUNTER_LEN);
		} else {
			wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm),
					LOGGER_INFO,
					"received EAPOL-Key request with invalid MIC");
			goto out;
		}

		if (key_info & WPA_KEY_INFO_ERROR) {
			if (wpa_receive_error_report(
				    wpa_auth, sm,
				    !(key_info & WPA_KEY_INFO_KEY_TYPE)) > 0)
				goto out; /* STA entry was removed */
		} else if (key_info & WPA_KEY_INFO_KEY_TYPE) {
			wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm),
					LOGGER_INFO,
					"received EAPOL-Key Request for new 4-Way Handshake");
			wpa_request_new_ptk(sm);
		} else {
			wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm),
					LOGGER_INFO,
					"received EAPOL-Key Request for GTK rekeying");

			eloop_cancel_timeout(wpa_rekey_gtk,
					     wpa_get_primary_auth(wpa_auth),
					     NULL);
			if (wpa_auth_gtk_rekey_in_process(wpa_auth))
				wpa_auth_logger(wpa_auth, NULL, LOGGER_DEBUG,
						"skip new GTK rekey - already in process");
			else
				wpa_rekey_gtk(wpa_get_primary_auth(wpa_auth),
					      NULL);
		}
	} else {
		/* Do not allow the same key replay counter to be reused. */
		wpa_replay_counter_mark_invalid(sm->key_replay,
						key->replay_counter);

		if (msg == PAIRWISE_2) {
			/*
			 * Maintain a copy of the pending EAPOL-Key frames in
			 * case the EAPOL-Key frame was retransmitted. This is
			 * needed to allow EAPOL-Key msg 2/4 reply to another
			 * pending msg 1/4 to update the SNonce to work around
			 * unexpected supplicant behavior.
			 */
			os_memcpy(sm->prev_key_replay, sm->key_replay,
				  sizeof(sm->key_replay));
		} else {
			os_memset(sm->prev_key_replay, 0,
				  sizeof(sm->prev_key_replay));
		}

		/*
		 * Make sure old valid counters are not accepted anymore and
		 * do not get copied again.
		 */
		wpa_replay_counter_mark_invalid(sm->key_replay, NULL);
	}

	os_free(sm->last_rx_eapol_key);
	sm->last_rx_eapol_key = os_memdup(data, data_len);
	if (!sm->last_rx_eapol_key)
		goto out;
	sm->last_rx_eapol_key_len = data_len;

	sm->rx_eapol_key_secure = !!(key_info & WPA_KEY_INFO_SECURE);
	sm->EAPOLKeyReceived = true;
	sm->EAPOLKeyPairwise = !!(key_info & WPA_KEY_INFO_KEY_TYPE);
	sm->EAPOLKeyRequest = !!(key_info & WPA_KEY_INFO_REQUEST);
	os_memcpy(sm->SNonce, key->key_nonce, WPA_NONCE_LEN);
	wpa_sm_step(sm);

out:
	bin_clear_free(key_data_buf, key_data_buf_len);
}


static int wpa_gmk_to_gtk(const u8 *gmk, const char *label, const u8 *addr,
			  const u8 *gnonce, u8 *gtk, size_t gtk_len)
{
	u8 data[ETH_ALEN + WPA_NONCE_LEN + 8 + WPA_GTK_MAX_LEN];
	u8 *pos;
	int ret = 0;

	/* GTK = PRF-X(GMK, "Group key expansion",
	 *	AA || GNonce || Time || random data)
	 * The example described in the IEEE 802.11 standard uses only AA and
	 * GNonce as inputs here. Add some more entropy since this derivation
	 * is done only at the Authenticator and as such, does not need to be
	 * exactly same.
	 */
	os_memset(data, 0, sizeof(data));
	os_memcpy(data, addr, ETH_ALEN);
	os_memcpy(data + ETH_ALEN, gnonce, WPA_NONCE_LEN);
	pos = data + ETH_ALEN + WPA_NONCE_LEN;
	wpa_get_ntp_timestamp(pos);
#ifdef TEST_FUZZ
	os_memset(pos, 0xef, 8);
#endif /* TEST_FUZZ */
	pos += 8;
	if (random_get_bytes(pos, gtk_len) < 0)
		ret = -1;

#ifdef CONFIG_SHA384
	if (sha384_prf(gmk, WPA_GMK_LEN, label, data, sizeof(data),
		       gtk, gtk_len) < 0)
		ret = -1;
#else /* CONFIG_SHA384 */
#ifdef CONFIG_SHA256
	if (sha256_prf(gmk, WPA_GMK_LEN, label, data, sizeof(data),
		       gtk, gtk_len) < 0)
		ret = -1;
#else /* CONFIG_SHA256 */
	if (sha1_prf(gmk, WPA_GMK_LEN, label, data, sizeof(data),
		     gtk, gtk_len) < 0)
		ret = -1;
#endif /* CONFIG_SHA256 */
#endif /* CONFIG_SHA384 */

	forced_memzero(data, sizeof(data));

	return ret;
}


static void wpa_send_eapol_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_authenticator *wpa_auth = eloop_ctx;
	struct wpa_state_machine *sm = timeout_ctx;

	if (sm->waiting_radius_psk) {
		wpa_auth_logger(wpa_auth, sm->addr, LOGGER_DEBUG,
				"Ignore EAPOL-Key timeout while waiting for RADIUS PSK");
		return;
	}

	sm->pending_1_of_4_timeout = 0;
	wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_DEBUG,
			"EAPOL-Key timeout");
	sm->TimeoutEvt = true;
	wpa_sm_step(sm);
}


void __wpa_send_eapol(struct wpa_authenticator *wpa_auth,
		      struct wpa_state_machine *sm, int key_info,
		      const u8 *key_rsc, const u8 *nonce,
		      const u8 *kde, size_t kde_len,
		      int keyidx, int encr, int force_version)
{
	struct wpa_auth_config *conf = &wpa_auth->conf;
	struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *key;
	size_t len, mic_len, keyhdrlen;
	int alg;
	int key_data_len, pad_len = 0;
	u8 *buf, *pos;
	int version, pairwise;
	int i;
	u8 *key_mic, *key_data;

	mic_len = wpa_mic_len(sm->wpa_key_mgmt, sm->pmk_len);
	keyhdrlen = sizeof(*key) + mic_len + 2;

	len = sizeof(struct ieee802_1x_hdr) + keyhdrlen;

	if (force_version)
		version = force_version;
	else if (wpa_use_akm_defined(sm->wpa_key_mgmt))
		version = WPA_KEY_INFO_TYPE_AKM_DEFINED;
	else if (wpa_use_cmac(sm->wpa_key_mgmt))
		version = WPA_KEY_INFO_TYPE_AES_128_CMAC;
	else if (sm->pairwise != WPA_CIPHER_TKIP)
		version = WPA_KEY_INFO_TYPE_HMAC_SHA1_AES;
	else
		version = WPA_KEY_INFO_TYPE_HMAC_MD5_RC4;

	pairwise = !!(key_info & WPA_KEY_INFO_KEY_TYPE);

	wpa_printf(MSG_DEBUG,
		   "WPA: Send EAPOL(version=%d secure=%d mic=%d ack=%d install=%d pairwise=%d kde_len=%zu keyidx=%d encr=%d)",
		   version,
		   (key_info & WPA_KEY_INFO_SECURE) ? 1 : 0,
		   (key_info & WPA_KEY_INFO_MIC) ? 1 : 0,
		   (key_info & WPA_KEY_INFO_ACK) ? 1 : 0,
		   (key_info & WPA_KEY_INFO_INSTALL) ? 1 : 0,
		   pairwise, kde_len, keyidx, encr);

	key_data_len = kde_len;

	if ((version == WPA_KEY_INFO_TYPE_HMAC_SHA1_AES ||
	     wpa_use_aes_key_wrap(sm->wpa_key_mgmt) ||
	     version == WPA_KEY_INFO_TYPE_AES_128_CMAC) && encr) {
		pad_len = key_data_len % 8;
		if (pad_len)
			pad_len = 8 - pad_len;
		key_data_len += pad_len + 8;
	}

	len += key_data_len;
	if (!mic_len && encr)
		len += AES_BLOCK_SIZE;

	hdr = os_zalloc(len);
	if (!hdr)
		return;
	hdr->version = conf->eapol_version;
	hdr->type = IEEE802_1X_TYPE_EAPOL_KEY;
	hdr->length = host_to_be16(len  - sizeof(*hdr));
	key = (struct wpa_eapol_key *) (hdr + 1);
	key_mic = (u8 *) (key + 1);
	key_data = ((u8 *) (hdr + 1)) + keyhdrlen;

	key->type = sm->wpa == WPA_VERSION_WPA2 ?
		EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
	key_info |= version;
	if (encr && sm->wpa == WPA_VERSION_WPA2)
		key_info |= WPA_KEY_INFO_ENCR_KEY_DATA;
	if (sm->wpa != WPA_VERSION_WPA2)
		key_info |= keyidx << WPA_KEY_INFO_KEY_INDEX_SHIFT;
	WPA_PUT_BE16(key->key_info, key_info);

	alg = pairwise ? sm->pairwise : conf->wpa_group;
	if (sm->wpa == WPA_VERSION_WPA2 && !pairwise)
		WPA_PUT_BE16(key->key_length, 0);
	else
		WPA_PUT_BE16(key->key_length, wpa_cipher_key_len(alg));

	for (i = RSNA_MAX_EAPOL_RETRIES - 1; i > 0; i--) {
		sm->key_replay[i].valid = sm->key_replay[i - 1].valid;
		os_memcpy(sm->key_replay[i].counter,
			  sm->key_replay[i - 1].counter,
			  WPA_REPLAY_COUNTER_LEN);
	}
	inc_byte_array(sm->key_replay[0].counter, WPA_REPLAY_COUNTER_LEN);
	os_memcpy(key->replay_counter, sm->key_replay[0].counter,
		  WPA_REPLAY_COUNTER_LEN);
	wpa_hexdump(MSG_DEBUG, "WPA: Replay Counter",
		    key->replay_counter, WPA_REPLAY_COUNTER_LEN);
	sm->key_replay[0].valid = true;

	if (nonce)
		os_memcpy(key->key_nonce, nonce, WPA_NONCE_LEN);

	if (key_rsc)
		os_memcpy(key->key_rsc, key_rsc, WPA_KEY_RSC_LEN);

	if (kde && !encr) {
		os_memcpy(key_data, kde, kde_len);
		WPA_PUT_BE16(key_mic + mic_len, kde_len);
#ifdef CONFIG_FILS
	} else if (!mic_len && kde) {
		const u8 *aad[1];
		size_t aad_len[1];

		WPA_PUT_BE16(key_mic, AES_BLOCK_SIZE + kde_len);
		wpa_hexdump_key(MSG_DEBUG, "Plaintext EAPOL-Key Key Data",
				kde, kde_len);

		wpa_hexdump_key(MSG_DEBUG, "WPA: KEK",
				sm->PTK.kek, sm->PTK.kek_len);
		/* AES-SIV AAD from EAPOL protocol version field (inclusive) to
		 * to Key Data (exclusive). */
		aad[0] = (u8 *) hdr;
		aad_len[0] = key_mic + 2 - (u8 *) hdr;
		if (aes_siv_encrypt(sm->PTK.kek, sm->PTK.kek_len, kde, kde_len,
				    1, aad, aad_len, key_mic + 2) < 0) {
			wpa_printf(MSG_DEBUG, "WPA: AES-SIV encryption failed");
			return;
		}

		wpa_hexdump(MSG_DEBUG, "WPA: Encrypted Key Data from SIV",
			    key_mic + 2, AES_BLOCK_SIZE + kde_len);
#endif /* CONFIG_FILS */
	} else if (encr && kde) {
		buf = os_zalloc(key_data_len);
		if (!buf) {
			os_free(hdr);
			return;
		}
		pos = buf;
		os_memcpy(pos, kde, kde_len);
		pos += kde_len;

		if (pad_len)
			*pos++ = 0xdd;

		wpa_hexdump_key(MSG_DEBUG,
				"Plaintext EAPOL-Key Key Data (+ padding)",
				buf, key_data_len);
		if (version == WPA_KEY_INFO_TYPE_HMAC_SHA1_AES ||
		    wpa_use_aes_key_wrap(sm->wpa_key_mgmt) ||
		    version == WPA_KEY_INFO_TYPE_AES_128_CMAC) {
			wpa_hexdump_key(MSG_DEBUG, "RSN: AES-WRAP using KEK",
					sm->PTK.kek, sm->PTK.kek_len);
			if (aes_wrap(sm->PTK.kek, sm->PTK.kek_len,
				     (key_data_len - 8) / 8, buf, key_data)) {
				os_free(hdr);
				bin_clear_free(buf, key_data_len);
				return;
			}
			wpa_hexdump(MSG_DEBUG,
				    "RSN: Encrypted Key Data from AES-WRAP",
				    key_data, key_data_len);
			WPA_PUT_BE16(key_mic + mic_len, key_data_len);
#if !defined(CONFIG_NO_RC4) && !defined(CONFIG_FIPS)
		} else if (sm->PTK.kek_len == 16) {
			u8 ek[32];

			wpa_printf(MSG_DEBUG,
				   "WPA: Encrypt Key Data using RC4");
			os_memcpy(key->key_iv,
				  sm->group->Counter + WPA_NONCE_LEN - 16, 16);
			inc_byte_array(sm->group->Counter, WPA_NONCE_LEN);
			os_memcpy(ek, key->key_iv, 16);
			os_memcpy(ek + 16, sm->PTK.kek, sm->PTK.kek_len);
			os_memcpy(key_data, buf, key_data_len);
			rc4_skip(ek, 32, 256, key_data, key_data_len);
			WPA_PUT_BE16(key_mic + mic_len, key_data_len);
#endif /* !(CONFIG_NO_RC4 || CONFIG_FIPS) */
		} else {
			os_free(hdr);
			bin_clear_free(buf, key_data_len);
			return;
		}
		bin_clear_free(buf, key_data_len);
	}

	if (key_info & WPA_KEY_INFO_MIC) {
		if (!sm->PTK_valid || !mic_len) {
			wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm),
					LOGGER_DEBUG,
					"PTK not valid when sending EAPOL-Key frame");
			os_free(hdr);
			return;
		}

		if (wpa_eapol_key_mic(sm->PTK.kck, sm->PTK.kck_len,
				      sm->wpa_key_mgmt, version,
				      (u8 *) hdr, len, key_mic) < 0) {
			os_free(hdr);
			return;
		}
#ifdef CONFIG_TESTING_OPTIONS
		if (!pairwise &&
		    conf->corrupt_gtk_rekey_mic_probability > 0.0 &&
		    drand48() < conf->corrupt_gtk_rekey_mic_probability) {
			wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm),
					LOGGER_INFO,
					"Corrupting group EAPOL-Key Key MIC");
			key_mic[0]++;
		}
#endif /* CONFIG_TESTING_OPTIONS */
	}

	wpa_auth_set_eapol(wpa_auth, sm->addr, WPA_EAPOL_inc_EapolFramesTx, 1);
	wpa_hexdump(MSG_DEBUG, "Send EAPOL-Key msg", hdr, len);
	wpa_auth_send_eapol(wpa_auth, sm->addr, (u8 *) hdr, len,
			    sm->pairwise_set);
	os_free(hdr);
}


static int wpa_auth_get_sta_count(struct wpa_authenticator *wpa_auth)
{
	if (!wpa_auth->cb->get_sta_count)
		return -1;

	return wpa_auth->cb->get_sta_count(wpa_auth->cb_ctx);
}


static void wpa_send_eapol(struct wpa_authenticator *wpa_auth,
			   struct wpa_state_machine *sm, int key_info,
			   const u8 *key_rsc, const u8 *nonce,
			   const u8 *kde, size_t kde_len,
			   int keyidx, int encr)
{
	int timeout_ms;
	int pairwise = key_info & WPA_KEY_INFO_KEY_TYPE;
	u32 ctr;

	if (!sm)
		return;

	ctr = pairwise ? sm->TimeoutCtr : sm->GTimeoutCtr;

#ifdef CONFIG_TESTING_OPTIONS
	/* When delay_eapol_tx is true, delay the EAPOL-Key transmission by
	 * sending it only on the last attempt after all timeouts for the prior
	 * skipped attemps. */
	if (wpa_auth->conf.delay_eapol_tx &&
	    ctr != wpa_auth->conf.wpa_pairwise_update_count) {
		wpa_msg(sm->wpa_auth->conf.msg_ctx, MSG_INFO,
			"DELAY-EAPOL-TX-%d", ctr);
		goto skip_tx;
	}
#endif /* CONFIG_TESTING_OPTIONS */
	__wpa_send_eapol(wpa_auth, sm, key_info, key_rsc, nonce, kde, kde_len,
			 keyidx, encr, 0);
#ifdef CONFIG_TESTING_OPTIONS
skip_tx:
#endif /* CONFIG_TESTING_OPTIONS */

	if (ctr == 1 && wpa_auth->conf.tx_status) {
		if (pairwise)
			timeout_ms = eapol_key_timeout_first;
		else if (wpa_auth_get_sta_count(wpa_auth) > 100)
			timeout_ms = eapol_key_timeout_first_group * 2;
		else
			timeout_ms = eapol_key_timeout_first_group;
	} else {
		timeout_ms = eapol_key_timeout_subseq;
	}
	if (wpa_auth->conf.wpa_disable_eapol_key_retries &&
	    (!pairwise || (key_info & WPA_KEY_INFO_MIC)))
		timeout_ms = eapol_key_timeout_no_retrans;
	if (pairwise && ctr == 1 && !(key_info & WPA_KEY_INFO_MIC))
		sm->pending_1_of_4_timeout = 1;
#ifdef TEST_FUZZ
	timeout_ms = 1;
#endif /* TEST_FUZZ */
	wpa_printf(MSG_DEBUG,
		   "WPA: Use EAPOL-Key timeout of %u ms (retry counter %u)",
		   timeout_ms, ctr);
	eloop_register_timeout(timeout_ms / 1000, (timeout_ms % 1000) * 1000,
			       wpa_send_eapol_timeout, wpa_auth, sm);
}


static int wpa_verify_key_mic(int akmp, size_t pmk_len, struct wpa_ptk *PTK,
			      u8 *data, size_t data_len)
{
	struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *key;
	u16 key_info;
	int ret = 0;
	u8 mic[WPA_EAPOL_KEY_MIC_MAX_LEN], *mic_pos;
	size_t mic_len = wpa_mic_len(akmp, pmk_len);

	if (data_len < sizeof(*hdr) + sizeof(*key))
		return -1;

	hdr = (struct ieee802_1x_hdr *) data;
	key = (struct wpa_eapol_key *) (hdr + 1);
	mic_pos = (u8 *) (key + 1);
	key_info = WPA_GET_BE16(key->key_info);
	os_memcpy(mic, mic_pos, mic_len);
	os_memset(mic_pos, 0, mic_len);
	if (wpa_eapol_key_mic(PTK->kck, PTK->kck_len, akmp,
			      key_info & WPA_KEY_INFO_TYPE_MASK,
			      data, data_len, mic_pos) ||
	    os_memcmp_const(mic, mic_pos, mic_len) != 0)
		ret = -1;
	os_memcpy(mic_pos, mic, mic_len);
	return ret;
}


void wpa_remove_ptk(struct wpa_state_machine *sm)
{
	sm->PTK_valid = false;
	os_memset(&sm->PTK, 0, sizeof(sm->PTK));

	wpa_auth_remove_ptksa(sm->wpa_auth, sm->addr, sm->pairwise);

	if (wpa_auth_set_key(sm->wpa_auth, 0, WPA_ALG_NONE, sm->addr, 0, NULL,
			     0, KEY_FLAG_PAIRWISE))
		wpa_printf(MSG_DEBUG,
			   "RSN: PTK removal from the driver failed");
	if (sm->use_ext_key_id &&
	    wpa_auth_set_key(sm->wpa_auth, 0, WPA_ALG_NONE, sm->addr, 1, NULL,
			     0, KEY_FLAG_PAIRWISE))
		wpa_printf(MSG_DEBUG,
			   "RSN: PTK Key ID 1 removal from the driver failed");
	sm->pairwise_set = false;
	eloop_cancel_timeout(wpa_rekey_ptk, sm->wpa_auth, sm);
}


int wpa_auth_sm_event(struct wpa_state_machine *sm, enum wpa_event event)
{
	int remove_ptk = 1;

	if (!sm)
		return -1;

	wpa_auth_vlogger(sm->wpa_auth, wpa_auth_get_spa(sm), LOGGER_DEBUG,
			 "event %d notification", event);

	switch (event) {
	case WPA_AUTH:
#ifdef CONFIG_MESH
		/* PTKs are derived through AMPE */
		if (wpa_auth_start_ampe(sm->wpa_auth, sm->addr)) {
			/* not mesh */
			break;
		}
		return 0;
#endif /* CONFIG_MESH */
	case WPA_ASSOC:
		break;
	case WPA_DEAUTH:
	case WPA_DISASSOC:
		sm->DeauthenticationRequest = true;
		os_memset(sm->PMK, 0, sizeof(sm->PMK));
		sm->pmk_len = 0;
#ifdef CONFIG_IEEE80211R_AP
		os_memset(sm->xxkey, 0, sizeof(sm->xxkey));
		sm->xxkey_len = 0;
		os_memset(sm->pmk_r1, 0, sizeof(sm->pmk_r1));
		sm->pmk_r1_len = 0;
#endif /* CONFIG_IEEE80211R_AP */
		break;
	case WPA_REAUTH:
	case WPA_REAUTH_EAPOL:
		if (!sm->started) {
			/*
			 * When using WPS, we may end up here if the STA
			 * manages to re-associate without the previous STA
			 * entry getting removed. Consequently, we need to make
			 * sure that the WPA state machines gets initialized
			 * properly at this point.
			 */
			wpa_printf(MSG_DEBUG,
				   "WPA state machine had not been started - initialize now");
			sm->started = 1;
			sm->Init = true;
			if (wpa_sm_step(sm) == 1)
				return 1; /* should not really happen */
			sm->Init = false;
			sm->AuthenticationRequest = true;
			break;
		}

		if (sm->ptkstart_without_success > 3) {
			wpa_printf(MSG_INFO,
				   "WPA: Multiple EAP reauth attempts without 4-way handshake completion, disconnect "
				   MACSTR, MAC2STR(sm->addr));
			sm->Disconnect = true;
			break;
		}

		if (!sm->use_ext_key_id &&
		    sm->wpa_auth->conf.wpa_deny_ptk0_rekey) {
			wpa_printf(MSG_INFO,
				   "WPA: PTK0 rekey not allowed, disconnect "
				   MACSTR, MAC2STR(wpa_auth_get_spa(sm)));
			sm->Disconnect = true;
			/* Try to encourage the STA to reconnect */
			sm->disconnect_reason =
				WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA;
			break;
		}

		if (sm->use_ext_key_id)
			sm->keyidx_active ^= 1; /* flip Key ID */

		if (sm->GUpdateStationKeys) {
			/*
			 * Reauthentication cancels the pending group key
			 * update for this STA.
			 */
			wpa_gkeydone_sta(sm);
			sm->PtkGroupInit = true;
		}
		sm->ReAuthenticationRequest = true;
		break;
	case WPA_ASSOC_FT:
#ifdef CONFIG_IEEE80211R_AP
		wpa_printf(MSG_DEBUG,
			   "FT: Retry PTK configuration after association");
		wpa_ft_install_ptk(sm, 1);

		/* Using FT protocol, not WPA auth state machine */
		sm->ft_completed = 1;
		wpa_auth_set_ptk_rekey_timer(sm);
		return 0;
#else /* CONFIG_IEEE80211R_AP */
		break;
#endif /* CONFIG_IEEE80211R_AP */
	case WPA_ASSOC_FILS:
#ifdef CONFIG_FILS
		wpa_printf(MSG_DEBUG,
			   "FILS: TK configuration after association");
		fils_set_tk(sm);
		sm->fils_completed = 1;
		return 0;
#else /* CONFIG_FILS */
		break;
#endif /* CONFIG_FILS */
	case WPA_DRV_STA_REMOVED:
		sm->tk_already_set = false;
		return 0;
	}

#ifdef CONFIG_IEEE80211R_AP
	sm->ft_completed = 0;
#endif /* CONFIG_IEEE80211R_AP */

	if (sm->mgmt_frame_prot && event == WPA_AUTH)
		remove_ptk = 0;
#ifdef CONFIG_FILS
	if (wpa_key_mgmt_fils(sm->wpa_key_mgmt) &&
	    (event == WPA_AUTH || event == WPA_ASSOC))
		remove_ptk = 0;
#endif /* CONFIG_FILS */

	if (remove_ptk) {
		sm->PTK_valid = false;
		os_memset(&sm->PTK, 0, sizeof(sm->PTK));

		if (event != WPA_REAUTH_EAPOL)
			wpa_remove_ptk(sm);
	}

	if (sm->in_step_loop) {
		/*
		 * wpa_sm_step() is already running - avoid recursive call to
		 * it by making the existing loop process the new update.
		 */
		sm->changed = true;
		return 0;
	}
	return wpa_sm_step(sm);
}


SM_STATE(WPA_PTK, INITIALIZE)
{
	SM_ENTRY_MA(WPA_PTK, INITIALIZE, wpa_ptk);
	if (sm->Init) {
		/* Init flag is not cleared here, so avoid busy
		 * loop by claiming nothing changed. */
		sm->changed = false;
	}

	sm->keycount = 0;
	if (sm->GUpdateStationKeys)
		wpa_gkeydone_sta(sm);
	if (sm->wpa == WPA_VERSION_WPA)
		sm->PInitAKeys = false;
	if (1 /* Unicast cipher supported AND (ESS OR ((IBSS or WDS) and
	       * Local AA > Remote AA)) */) {
		sm->Pair = true;
	}
	wpa_auth_set_eapol(sm->wpa_auth, sm->addr, WPA_EAPOL_portEnabled, 0);
	wpa_remove_ptk(sm);
	wpa_auth_set_eapol(sm->wpa_auth, sm->addr, WPA_EAPOL_portValid, 0);
	sm->TimeoutCtr = 0;
	if (wpa_key_mgmt_wpa_psk(sm->wpa_key_mgmt) ||
	    sm->wpa_key_mgmt == WPA_KEY_MGMT_DPP ||
	    sm->wpa_key_mgmt == WPA_KEY_MGMT_OWE) {
		wpa_auth_set_eapol(sm->wpa_auth, sm->addr,
				   WPA_EAPOL_authorized, 0);
	}
}


SM_STATE(WPA_PTK, DISCONNECT)
{
	u16 reason = sm->disconnect_reason;

	SM_ENTRY_MA(WPA_PTK, DISCONNECT, wpa_ptk);
	sm->Disconnect = false;
	sm->disconnect_reason = 0;
	if (!reason)
		reason = WLAN_REASON_PREV_AUTH_NOT_VALID;
	wpa_sta_disconnect(sm->wpa_auth, sm->addr, reason);
}


SM_STATE(WPA_PTK, DISCONNECTED)
{
	SM_ENTRY_MA(WPA_PTK, DISCONNECTED, wpa_ptk);
	sm->DeauthenticationRequest = false;
}


SM_STATE(WPA_PTK, AUTHENTICATION)
{
	SM_ENTRY_MA(WPA_PTK, AUTHENTICATION, wpa_ptk);
	os_memset(&sm->PTK, 0, sizeof(sm->PTK));
	sm->PTK_valid = false;
	wpa_auth_set_eapol(sm->wpa_auth, sm->addr, WPA_EAPOL_portControl_Auto,
			   1);
	wpa_auth_set_eapol(sm->wpa_auth, sm->addr, WPA_EAPOL_portEnabled, 1);
	sm->AuthenticationRequest = false;
}


static void wpa_group_ensure_init(struct wpa_authenticator *wpa_auth,
				  struct wpa_group *group)
{
	if (group->first_sta_seen)
		return;
	/*
	 * System has run bit further than at the time hostapd was started
	 * potentially very early during boot up. This provides better chances
	 * of collecting more randomness on embedded systems. Re-initialize the
	 * GMK and Counter here to improve their strength if there was not
	 * enough entropy available immediately after system startup.
	 */
	wpa_printf(MSG_DEBUG,
		   "WPA: Re-initialize GMK/Counter on first station");
	if (random_pool_ready() != 1) {
		wpa_printf(MSG_INFO,
			   "WPA: Not enough entropy in random pool to proceed - reject first 4-way handshake");
		group->reject_4way_hs_for_entropy = true;
	} else {
		group->first_sta_seen = true;
		group->reject_4way_hs_for_entropy = false;
	}

	if (wpa_group_init_gmk_and_counter(wpa_auth, group) < 0 ||
	    wpa_gtk_update(wpa_auth, group) < 0 ||
	    wpa_group_config_group_keys(wpa_auth, group) < 0) {
		wpa_printf(MSG_INFO, "WPA: GMK/GTK setup failed");
		group->first_sta_seen = false;
		group->reject_4way_hs_for_entropy = true;
	}
}


SM_STATE(WPA_PTK, AUTHENTICATION2)
{
	SM_ENTRY_MA(WPA_PTK, AUTHENTICATION2, wpa_ptk);

	wpa_group_ensure_init(sm->wpa_auth, sm->group);
	sm->ReAuthenticationRequest = false;

	/*
	 * Definition of ANonce selection in IEEE Std 802.11i-2004 is somewhat
	 * ambiguous. The Authenticator state machine uses a counter that is
	 * incremented by one for each 4-way handshake. However, the security
	 * analysis of 4-way handshake points out that unpredictable nonces
	 * help in preventing precomputation attacks. Instead of the state
	 * machine definition, use an unpredictable nonce value here to provide
	 * stronger protection against potential precomputation attacks.
	 */
	if (random_get_bytes(sm->ANonce, WPA_NONCE_LEN)) {
		wpa_printf(MSG_ERROR,
			   "WPA: Failed to get random data for ANonce.");
		sm->Disconnect = true;
		return;
	}
	wpa_hexdump(MSG_DEBUG, "WPA: Assign ANonce", sm->ANonce,
		    WPA_NONCE_LEN);
	/* IEEE 802.11i does not clear TimeoutCtr here, but this is more
	 * logical place than INITIALIZE since AUTHENTICATION2 can be
	 * re-entered on ReAuthenticationRequest without going through
	 * INITIALIZE. */
	sm->TimeoutCtr = 0;
}


static int wpa_auth_sm_ptk_update(struct wpa_state_machine *sm)
{
	if (random_get_bytes(sm->ANonce, WPA_NONCE_LEN)) {
		wpa_printf(MSG_ERROR,
			   "WPA: Failed to get random data for ANonce");
		sm->Disconnect = true;
		return -1;
	}
	wpa_hexdump(MSG_DEBUG, "WPA: Assign new ANonce", sm->ANonce,
		    WPA_NONCE_LEN);
	sm->TimeoutCtr = 0;
	return 0;
}


SM_STATE(WPA_PTK, INITPMK)
{
	u8 msk[2 * PMK_LEN];
	size_t len = 2 * PMK_LEN;

	SM_ENTRY_MA(WPA_PTK, INITPMK, wpa_ptk);
#ifdef CONFIG_IEEE80211R_AP
	sm->xxkey_len = 0;
#endif /* CONFIG_IEEE80211R_AP */
	if (sm->pmksa) {
		wpa_printf(MSG_DEBUG, "WPA: PMK from PMKSA cache");
		os_memcpy(sm->PMK, sm->pmksa->pmk, sm->pmksa->pmk_len);
		sm->pmk_len = sm->pmksa->pmk_len;
#ifdef CONFIG_DPP
	} else if (sm->wpa_key_mgmt == WPA_KEY_MGMT_DPP) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No PMKSA cache entry for STA - reject connection");
		sm->Disconnect = true;
		sm->disconnect_reason = WLAN_REASON_INVALID_PMKID;
		return;
#endif /* CONFIG_DPP */
	} else if (wpa_auth_get_msk(sm->wpa_auth, wpa_auth_get_spa(sm),
				    msk, &len) == 0) {
		unsigned int pmk_len;

		if (wpa_key_mgmt_sha384(sm->wpa_key_mgmt))
			pmk_len = PMK_LEN_SUITE_B_192;
		else
			pmk_len = PMK_LEN;
		wpa_printf(MSG_DEBUG,
			   "WPA: PMK from EAPOL state machine (MSK len=%zu PMK len=%u)",
			   len, pmk_len);
		if (len < pmk_len) {
			wpa_printf(MSG_DEBUG,
				   "WPA: MSK not long enough (%zu) to create PMK (%u)",
				   len, pmk_len);
			sm->Disconnect = true;
			return;
		}
		os_memcpy(sm->PMK, msk, pmk_len);
		sm->pmk_len = pmk_len;
#ifdef CONFIG_IEEE80211R_AP
		if (len >= 2 * PMK_LEN) {
			if (wpa_key_mgmt_sha384(sm->wpa_key_mgmt)) {
				os_memcpy(sm->xxkey, msk, SHA384_MAC_LEN);
				sm->xxkey_len = SHA384_MAC_LEN;
			} else {
				os_memcpy(sm->xxkey, msk + PMK_LEN, PMK_LEN);
				sm->xxkey_len = PMK_LEN;
			}
		}
#endif /* CONFIG_IEEE80211R_AP */
	} else {
		wpa_printf(MSG_DEBUG, "WPA: Could not get PMK, get_msk: %p",
			   sm->wpa_auth->cb->get_msk);
		sm->Disconnect = true;
		return;
	}
	forced_memzero(msk, sizeof(msk));

	sm->req_replay_counter_used = 0;
	/* IEEE 802.11i does not set keyRun to false, but not doing this
	 * will break reauthentication since EAPOL state machines may not be
	 * get into AUTHENTICATING state that clears keyRun before WPA state
	 * machine enters AUTHENTICATION2 state and goes immediately to INITPMK
	 * state and takes PMK from the previously used AAA Key. This will
	 * eventually fail in 4-Way Handshake because Supplicant uses PMK
	 * derived from the new AAA Key. Setting keyRun = false here seems to
	 * be good workaround for this issue. */
	wpa_auth_set_eapol(sm->wpa_auth, sm->addr, WPA_EAPOL_keyRun, false);
}


SM_STATE(WPA_PTK, INITPSK)
{
	const u8 *psk;
	size_t psk_len;

	SM_ENTRY_MA(WPA_PTK, INITPSK, wpa_ptk);
	psk = wpa_auth_get_psk(sm->wpa_auth, sm->addr, sm->p2p_dev_addr, NULL,
			       &psk_len, NULL);
	if (psk) {
		os_memcpy(sm->PMK, psk, psk_len);
		sm->pmk_len = psk_len;
#ifdef CONFIG_IEEE80211R_AP
		sm->xxkey_len = PMK_LEN;
#ifdef CONFIG_SAE
		if (sm->wpa_key_mgmt == WPA_KEY_MGMT_FT_SAE_EXT_KEY &&
		    (psk_len == SHA512_MAC_LEN || psk_len == SHA384_MAC_LEN ||
		     psk_len == SHA256_MAC_LEN))
			sm->xxkey_len = psk_len;
#endif /* CONFIG_SAE */
		os_memcpy(sm->xxkey, psk, sm->xxkey_len);
#endif /* CONFIG_IEEE80211R_AP */
	}
#ifdef CONFIG_SAE
	if (wpa_auth_uses_sae(sm) && sm->pmksa) {
		wpa_printf(MSG_DEBUG, "SAE: PMK from PMKSA cache (len=%zu)",
			   sm->pmksa->pmk_len);
		os_memcpy(sm->PMK, sm->pmksa->pmk, sm->pmksa->pmk_len);
		sm->pmk_len = sm->pmksa->pmk_len;
#ifdef CONFIG_IEEE80211R_AP
		os_memcpy(sm->xxkey, sm->pmksa->pmk, sm->pmksa->pmk_len);
		sm->xxkey_len = sm->pmksa->pmk_len;
#endif /* CONFIG_IEEE80211R_AP */
	}
#endif /* CONFIG_SAE */
	sm->req_replay_counter_used = 0;
}


SM_STATE(WPA_PTK, PTKSTART)
{
	u8 *buf;
	size_t buf_len = 2 + RSN_SELECTOR_LEN + PMKID_LEN;
	u8 *pmkid = NULL;
	size_t kde_len = 0;
	u16 key_info;
#ifdef CONFIG_TESTING_OPTIONS
	struct wpa_auth_config *conf = &sm->wpa_auth->conf;
#endif /* CONFIG_TESTING_OPTIONS */

	SM_ENTRY_MA(WPA_PTK, PTKSTART, wpa_ptk);
	sm->PTKRequest = false;
	sm->TimeoutEvt = false;
	sm->alt_snonce_valid = false;
	sm->ptkstart_without_success++;

	sm->TimeoutCtr++;
	if (sm->TimeoutCtr > sm->wpa_auth->conf.wpa_pairwise_update_count) {
		/* No point in sending the EAPOL-Key - we will disconnect
		 * immediately following this. */
		return;
	}

#ifdef CONFIG_IEEE80211BE
	if (sm->mld_assoc_link_id >= 0)
		buf_len += 2 + RSN_SELECTOR_LEN + ETH_ALEN;
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_TESTING_OPTIONS
	if (conf->eapol_m1_elements)
		buf_len += wpabuf_len(conf->eapol_m1_elements);
#endif /* CONFIG_TESTING_OPTIONS */

	buf = os_zalloc(buf_len);
	if (!buf)
		return;

	wpa_auth_logger(sm->wpa_auth, wpa_auth_get_spa(sm), LOGGER_DEBUG,
			"sending 1/4 msg of 4-Way Handshake");
	/*
	 * For infrastructure BSS cases, it is better for the AP not to include
	 * the PMKID KDE in EAPOL-Key msg 1/4 since it could be used to initiate
	 * offline search for the passphrase/PSK without having to be able to
	 * capture a 4-way handshake from a STA that has access to the network.
	 *
	 * For IBSS cases, addition of PMKID KDE could be considered even with
	 * WPA2-PSK cases that use multiple PSKs, but only if there is a single
	 * possible PSK for this STA. However, this should not be done unless
	 * there is support for using that information on the supplicant side.
	 * The concern about exposing PMKID unnecessarily in infrastructure BSS
	 * cases would also apply here, but at least in the IBSS case, this
	 * would cover a potential real use case.
	 */
	if (sm->wpa == WPA_VERSION_WPA2 &&
	    (wpa_key_mgmt_wpa_ieee8021x(sm->wpa_key_mgmt) ||
	     (sm->wpa_key_mgmt == WPA_KEY_MGMT_OWE && sm->pmksa) ||
	     wpa_key_mgmt_sae(sm->wpa_key_mgmt)) &&
	    sm->wpa_key_mgmt != WPA_KEY_MGMT_OSEN) {
		pmkid = buf;
		kde_len = 2 + RSN_SELECTOR_LEN + PMKID_LEN;
		pmkid[0] = WLAN_EID_VENDOR_SPECIFIC;
		pmkid[1] = RSN_SELECTOR_LEN + PMKID_LEN;
		RSN_SELECTOR_PUT(&pmkid[2], RSN_KEY_DATA_PMKID);
		if (sm->pmksa) {
			wpa_hexdump(MSG_DEBUG,
				    "RSN: Message 1/4 PMKID from PMKSA entry",
				    sm->pmksa->pmkid, PMKID_LEN);
			os_memcpy(&pmkid[2 + RSN_SELECTOR_LEN],
				  sm->pmksa->pmkid, PMKID_LEN);
		} else if (wpa_key_mgmt_suite_b(sm->wpa_key_mgmt)) {
			/* No KCK available to derive PMKID */
			wpa_printf(MSG_DEBUG,
				   "RSN: No KCK available to derive PMKID for message 1/4");
			pmkid = NULL;
#ifdef CONFIG_FILS
		} else if (wpa_key_mgmt_fils(sm->wpa_key_mgmt)) {
			if (sm->pmkid_set) {
				wpa_hexdump(MSG_DEBUG,
					    "RSN: Message 1/4 PMKID from FILS/ERP",
					    sm->pmkid, PMKID_LEN);
				os_memcpy(&pmkid[2 + RSN_SELECTOR_LEN],
					  sm->pmkid, PMKID_LEN);
			} else {
				/* No PMKID available */
				wpa_printf(MSG_DEBUG,
					   "RSN: No FILS/ERP PMKID available for message 1/4");
				pmkid = NULL;
			}
#endif /* CONFIG_FILS */
#ifdef CONFIG_IEEE80211R_AP
		} else if (wpa_key_mgmt_ft(sm->wpa_key_mgmt) &&
			   sm->ft_completed) {
			wpa_printf(MSG_DEBUG,
				   "FT: No PMKID in message 1/4 when using FT protocol");
			pmkid = NULL;
#endif /* CONFIG_IEEE80211R_AP */
#ifdef CONFIG_SAE
		} else if (wpa_key_mgmt_sae(sm->wpa_key_mgmt)) {
			if (sm->pmkid_set) {
				wpa_hexdump(MSG_DEBUG,
					    "RSN: Message 1/4 PMKID from SAE",
					    sm->pmkid, PMKID_LEN);
				os_memcpy(&pmkid[2 + RSN_SELECTOR_LEN],
					  sm->pmkid, PMKID_LEN);
			} else {
				/* No PMKID available */
				wpa_printf(MSG_DEBUG,
					   "RSN: No SAE PMKID available for message 1/4");
				pmkid = NULL;
			}
#endif /* CONFIG_SAE */
		} else {
			/*
			 * Calculate PMKID since no PMKSA cache entry was
			 * available with pre-calculated PMKID.
			 */
			rsn_pmkid(sm->PMK, sm->pmk_len,
				  wpa_auth_get_aa(sm),
				  wpa_auth_get_spa(sm),
				  &pmkid[2 + RSN_SELECTOR_LEN],
				  sm->wpa_key_mgmt);
			wpa_hexdump(MSG_DEBUG,
				    "RSN: Message 1/4 PMKID derived from PMK",
				    &pmkid[2 + RSN_SELECTOR_LEN], PMKID_LEN);
		}
	}
	if (!pmkid)
		kde_len = 0;

#ifdef CONFIG_IEEE80211BE
	if (sm->mld_assoc_link_id >= 0) {
		wpa_printf(MSG_DEBUG,
			   "RSN: MLD: Add MAC Address KDE: kde_len=%zu",
			   kde_len);
		wpa_add_kde(buf + kde_len, RSN_KEY_DATA_MAC_ADDR,
			    sm->wpa_auth->mld_addr, ETH_ALEN, NULL, 0);
		kde_len += 2 + RSN_SELECTOR_LEN + ETH_ALEN;
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_TESTING_OPTIONS
	if (conf->eapol_m1_elements) {
		os_memcpy(buf + kde_len, wpabuf_head(conf->eapol_m1_elements),
			  wpabuf_len(conf->eapol_m1_elements));
		kde_len += wpabuf_len(conf->eapol_m1_elements);
	}
#endif /* CONFIG_TESTING_OPTIONS */

	key_info = WPA_KEY_INFO_ACK | WPA_KEY_INFO_KEY_TYPE;
	if (sm->pairwise_set && sm->wpa != WPA_VERSION_WPA)
		key_info |= WPA_KEY_INFO_SECURE;
	wpa_send_eapol(sm->wpa_auth, sm, key_info, NULL,
		       sm->ANonce, kde_len ? buf : NULL, kde_len, 0, 0);
	os_free(buf);
}


static int wpa_derive_ptk(struct wpa_state_machine *sm, const u8 *snonce,
			  const u8 *pmk, unsigned int pmk_len,
			  struct wpa_ptk *ptk, int force_sha256,
			  u8 *pmk_r0, u8 *pmk_r1, u8 *pmk_r0_name,
			  size_t *key_len, bool no_kdk)
{
	const u8 *z = NULL;
	size_t z_len = 0, kdk_len;
	int akmp;
	int ret;

	if (sm->wpa_auth->conf.force_kdk_derivation ||
	    (!no_kdk && sm->wpa_auth->conf.secure_ltf &&
	     ieee802_11_rsnx_capab(sm->rsnxe, WLAN_RSNX_CAPAB_SECURE_LTF)))
		kdk_len = WPA_KDK_MAX_LEN;
	else
		kdk_len = 0;

#ifdef CONFIG_IEEE80211R_AP
	if (wpa_key_mgmt_ft(sm->wpa_key_mgmt)) {
		if (sm->ft_completed) {
			u8 ptk_name[WPA_PMK_NAME_LEN];

			ret = wpa_pmk_r1_to_ptk(sm->pmk_r1, sm->pmk_r1_len,
						sm->SNonce, sm->ANonce,
						wpa_auth_get_spa(sm),
						wpa_auth_get_aa(sm),
						sm->pmk_r1_name, ptk,
						ptk_name, sm->wpa_key_mgmt,
						sm->pairwise, kdk_len);
		} else {
			ret = wpa_auth_derive_ptk_ft(sm, ptk, pmk_r0, pmk_r1,
						     pmk_r0_name, key_len,
						     kdk_len);
		}
		if (ret) {
			wpa_printf(MSG_ERROR, "FT: PTK derivation failed");
			return ret;
		}

#ifdef CONFIG_PASN
		if (!no_kdk && sm->wpa_auth->conf.secure_ltf &&
		    ieee802_11_rsnx_capab(sm->rsnxe,
					  WLAN_RSNX_CAPAB_SECURE_LTF)) {
			ret = wpa_ltf_keyseed(ptk, sm->wpa_key_mgmt,
					      sm->pairwise);
			if (ret) {
				wpa_printf(MSG_ERROR,
					   "FT: LTF keyseed derivation failed");
			}
		}
#endif /* CONFIG_PASN */
		return ret;
	}
#endif /* CONFIG_IEEE80211R_AP */

#ifdef CONFIG_DPP2
	if (sm->wpa_key_mgmt == WPA_KEY_MGMT_DPP && sm->dpp_z) {
		z = wpabuf_head(sm->dpp_z);
		z_len = wpabuf_len(sm->dpp_z);
	}
#endif /* CONFIG_DPP2 */

	akmp = sm->wpa_key_mgmt;
	if (force_sha256)
		akmp |= WPA_KEY_MGMT_PSK_SHA256;
	ret = wpa_pmk_to_ptk(pmk, pmk_len, "Pairwise key expansion",
			     wpa_auth_get_aa(sm), wpa_auth_get_spa(sm),
			     sm->ANonce, snonce, ptk, akmp,
			     sm->pairwise, z, z_len, kdk_len);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "WPA: PTK derivation failed");
		return ret;
	}

#ifdef CONFIG_PASN
	if (!no_kdk && sm->wpa_auth->conf.secure_ltf &&
	    ieee802_11_rsnx_capab(sm->rsnxe, WLAN_RSNX_CAPAB_SECURE_LTF)) {
		ret = wpa_ltf_keyseed(ptk, sm->wpa_key_mgmt, sm->pairwise);
		if (ret) {
			wpa_printf(MSG_DEBUG,
				   "WPA: LTF keyseed derivation failed");
		}
	}
#endif /* CONFIG_PASN */
	return ret;
}


#ifdef CONFIG_FILS

int fils_auth_pmk_to_ptk(struct wpa_state_machine *sm, const u8 *pmk,
			 size_t pmk_len, const u8 *snonce, const u8 *anonce,
			 const u8 *dhss, size_t dhss_len,
			 struct wpabuf *g_sta, struct wpabuf *g_ap)
{
	u8 ick[FILS_ICK_MAX_LEN];
	size_t ick_len;
	int res;
	u8 fils_ft[FILS_FT_MAX_LEN];
	size_t fils_ft_len = 0, kdk_len;

	if (sm->wpa_auth->conf.force_kdk_derivation ||
	    (sm->wpa_auth->conf.secure_ltf &&
	     ieee802_11_rsnx_capab(sm->rsnxe, WLAN_RSNX_CAPAB_SECURE_LTF)))
		kdk_len = WPA_KDK_MAX_LEN;
	else
		kdk_len = 0;

	res = fils_pmk_to_ptk(pmk, pmk_len, wpa_auth_get_spa(sm),
			      wpa_auth_get_aa(sm),
			      snonce, anonce, dhss, dhss_len,
			      &sm->PTK, ick, &ick_len,
			      sm->wpa_key_mgmt, sm->pairwise,
			      fils_ft, &fils_ft_len, kdk_len);
	if (res < 0)
		return res;

#ifdef CONFIG_PASN
	if (sm->wpa_auth->conf.secure_ltf &&
	    ieee802_11_rsnx_capab(sm->rsnxe, WLAN_RSNX_CAPAB_SECURE_LTF)) {
		res = wpa_ltf_keyseed(&sm->PTK, sm->wpa_key_mgmt, sm->pairwise);
		if (res) {
			wpa_printf(MSG_ERROR,
				   "FILS: LTF keyseed derivation failed");
			return res;
		}
	}
#endif /* CONFIG_PASN */

	sm->PTK_valid = true;
	sm->tk_already_set = false;

#ifdef CONFIG_IEEE80211R_AP
	if (fils_ft_len) {
		struct wpa_authenticator *wpa_auth = sm->wpa_auth;
		struct wpa_auth_config *conf = &wpa_auth->conf;
		u8 pmk_r0[PMK_LEN_MAX], pmk_r0_name[WPA_PMK_NAME_LEN];

		if (wpa_derive_pmk_r0(fils_ft, fils_ft_len,
				      conf->ssid, conf->ssid_len,
				      conf->mobility_domain,
				      conf->r0_key_holder,
				      conf->r0_key_holder_len,
				      wpa_auth_get_spa(sm), pmk_r0, pmk_r0_name,
				      sm->wpa_key_mgmt) < 0)
			return -1;

		wpa_ft_store_pmk_fils(sm, pmk_r0, pmk_r0_name);
		forced_memzero(fils_ft, sizeof(fils_ft));

		res = wpa_derive_pmk_r1_name(pmk_r0_name, conf->r1_key_holder,
					     wpa_auth_get_spa(sm),
					     sm->pmk_r1_name,
					     fils_ft_len);
		forced_memzero(pmk_r0, PMK_LEN_MAX);
		if (res < 0)
			return -1;
		wpa_hexdump(MSG_DEBUG, "FILS+FT: PMKR1Name", sm->pmk_r1_name,
			    WPA_PMK_NAME_LEN);
		sm->pmk_r1_name_valid = 1;
	}
#endif /* CONFIG_IEEE80211R_AP */

	res = fils_key_auth_sk(ick, ick_len, snonce, anonce,
			       wpa_auth_get_spa(sm),
			       wpa_auth_get_aa(sm),
			       g_sta ? wpabuf_head(g_sta) : NULL,
			       g_sta ? wpabuf_len(g_sta) : 0,
			       g_ap ? wpabuf_head(g_ap) : NULL,
			       g_ap ? wpabuf_len(g_ap) : 0,
			       sm->wpa_key_mgmt, sm->fils_key_auth_sta,
			       sm->fils_key_auth_ap,
			       &sm->fils_key_auth_len);
	forced_memzero(ick, sizeof(ick));

	/* Store nonces for (Re)Association Request/Response frame processing */
	os_memcpy(sm->SNonce, snonce, FILS_NONCE_LEN);
	os_memcpy(sm->ANonce, anonce, FILS_NONCE_LEN);

	return res;
}


static int wpa_aead_decrypt(struct wpa_state_machine *sm, struct wpa_ptk *ptk,
			    u8 *buf, size_t buf_len, u16 *_key_data_len)
{
	struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *key;
	u8 *pos;
	u16 key_data_len;
	u8 *tmp;
	const u8 *aad[1];
	size_t aad_len[1];

	hdr = (struct ieee802_1x_hdr *) buf;
	key = (struct wpa_eapol_key *) (hdr + 1);
	pos = (u8 *) (key + 1);
	key_data_len = WPA_GET_BE16(pos);
	if (key_data_len < AES_BLOCK_SIZE ||
	    key_data_len > buf_len - sizeof(*hdr) - sizeof(*key) - 2) {
		wpa_auth_logger(sm->wpa_auth, wpa_auth_get_spa(sm), LOGGER_INFO,
				"No room for AES-SIV data in the frame");
		return -1;
	}
	pos += 2; /* Pointing at the Encrypted Key Data field */

	tmp = os_malloc(key_data_len);
	if (!tmp)
		return -1;

	/* AES-SIV AAD from EAPOL protocol version field (inclusive) to
	 * to Key Data (exclusive). */
	aad[0] = buf;
	aad_len[0] = pos - buf;
	if (aes_siv_decrypt(ptk->kek, ptk->kek_len, pos, key_data_len,
			    1, aad, aad_len, tmp) < 0) {
		wpa_auth_logger(sm->wpa_auth, wpa_auth_get_spa(sm), LOGGER_INFO,
				"Invalid AES-SIV data in the frame");
		bin_clear_free(tmp, key_data_len);
		return -1;
	}

	/* AEAD decryption and validation completed successfully */
	key_data_len -= AES_BLOCK_SIZE;
	wpa_hexdump_key(MSG_DEBUG, "WPA: Decrypted Key Data",
			tmp, key_data_len);

	/* Replace Key Data field with the decrypted version */
	os_memcpy(pos, tmp, key_data_len);
	pos -= 2; /* Key Data Length field */
	WPA_PUT_BE16(pos, key_data_len);
	bin_clear_free(tmp, key_data_len);
	if (_key_data_len)
		*_key_data_len = key_data_len;
	return 0;
}


const u8 * wpa_fils_validate_fils_session(struct wpa_state_machine *sm,
					  const u8 *ies, size_t ies_len,
					  const u8 *fils_session)
{
	const u8 *ie, *end;
	const u8 *session = NULL;

	if (!wpa_key_mgmt_fils(sm->wpa_key_mgmt)) {
		wpa_printf(MSG_DEBUG,
			   "FILS: Not a FILS AKM - reject association");
		return NULL;
	}

	/* Verify Session element */
	ie = ies;
	end = ((const u8 *) ie) + ies_len;
	while (ie + 1 < end) {
		if (ie + 2 + ie[1] > end)
			break;
		if (ie[0] == WLAN_EID_EXTENSION &&
		    ie[1] >= 1 + FILS_SESSION_LEN &&
		    ie[2] == WLAN_EID_EXT_FILS_SESSION) {
			session = ie;
			break;
		}
		ie += 2 + ie[1];
	}

	if (!session) {
		wpa_printf(MSG_DEBUG,
			   "FILS: %s: Could not find FILS Session element in Assoc Req - reject",
			   __func__);
		return NULL;
	}

	if (!fils_session) {
		wpa_printf(MSG_DEBUG,
			   "FILS: %s: Could not find FILS Session element in STA entry - reject",
			   __func__);
		return NULL;
	}

	if (os_memcmp(fils_session, session + 3, FILS_SESSION_LEN) != 0) {
		wpa_printf(MSG_DEBUG, "FILS: Session mismatch");
		wpa_hexdump(MSG_DEBUG, "FILS: Expected FILS Session",
			    fils_session, FILS_SESSION_LEN);
		wpa_hexdump(MSG_DEBUG, "FILS: Received FILS Session",
			    session + 3, FILS_SESSION_LEN);
		return NULL;
	}
	return session;
}


int wpa_fils_validate_key_confirm(struct wpa_state_machine *sm, const u8 *ies,
				  size_t ies_len)
{
	struct ieee802_11_elems elems;

	if (ieee802_11_parse_elems(ies, ies_len, &elems, 1) == ParseFailed) {
		wpa_printf(MSG_DEBUG,
			   "FILS: Failed to parse decrypted elements");
		return -1;
	}

	if (!elems.fils_session) {
		wpa_printf(MSG_DEBUG, "FILS: No FILS Session element");
		return -1;
	}

	if (!elems.fils_key_confirm) {
		wpa_printf(MSG_DEBUG, "FILS: No FILS Key Confirm element");
		return -1;
	}

	if (elems.fils_key_confirm_len != sm->fils_key_auth_len) {
		wpa_printf(MSG_DEBUG,
			   "FILS: Unexpected Key-Auth length %d (expected %zu)",
			   elems.fils_key_confirm_len,
			   sm->fils_key_auth_len);
		return -1;
	}

	if (os_memcmp(elems.fils_key_confirm, sm->fils_key_auth_sta,
		      sm->fils_key_auth_len) != 0) {
		wpa_printf(MSG_DEBUG, "FILS: Key-Auth mismatch");
		wpa_hexdump(MSG_DEBUG, "FILS: Received Key-Auth",
			    elems.fils_key_confirm, elems.fils_key_confirm_len);
		wpa_hexdump(MSG_DEBUG, "FILS: Expected Key-Auth",
			    sm->fils_key_auth_sta, sm->fils_key_auth_len);
		return -1;
	}

	return 0;
}


int fils_decrypt_assoc(struct wpa_state_machine *sm, const u8 *fils_session,
		       const struct ieee80211_mgmt *mgmt, size_t frame_len,
		       u8 *pos, size_t left)
{
	u16 fc, stype;
	const u8 *end, *ie_start, *ie, *session, *crypt;
	const u8 *aad[5];
	size_t aad_len[5];

	if (!sm || !sm->PTK_valid) {
		wpa_printf(MSG_DEBUG,
			   "FILS: No KEK to decrypt Assocication Request frame");
		return -1;
	}

	if (!wpa_key_mgmt_fils(sm->wpa_key_mgmt)) {
		wpa_printf(MSG_DEBUG,
			   "FILS: Not a FILS AKM - reject association");
		return -1;
	}

	end = ((const u8 *) mgmt) + frame_len;
	fc = le_to_host16(mgmt->frame_control);
	stype = WLAN_FC_GET_STYPE(fc);
	if (stype == WLAN_FC_STYPE_REASSOC_REQ)
		ie_start = mgmt->u.reassoc_req.variable;
	else
		ie_start = mgmt->u.assoc_req.variable;
	ie = ie_start;

	/*
	 * Find FILS Session element which is the last unencrypted element in
	 * the frame.
	 */
	session = wpa_fils_validate_fils_session(sm, ie, end - ie,
						 fils_session);
	if (!session) {
		wpa_printf(MSG_DEBUG, "FILS: Session validation failed");
		return -1;
	}

	crypt = session + 2 + session[1];

	if (end - crypt < AES_BLOCK_SIZE) {
		wpa_printf(MSG_DEBUG,
			   "FILS: Too short frame to include AES-SIV data");
		return -1;
	}

	/* AES-SIV AAD vectors */

	/* The STA's MAC address */
	aad[0] = mgmt->sa;
	aad_len[0] = ETH_ALEN;
	/* The AP's BSSID */
	aad[1] = mgmt->da;
	aad_len[1] = ETH_ALEN;
	/* The STA's nonce */
	aad[2] = sm->SNonce;
	aad_len[2] = FILS_NONCE_LEN;
	/* The AP's nonce */
	aad[3] = sm->ANonce;
	aad_len[3] = FILS_NONCE_LEN;
	/*
	 * The (Re)Association Request frame from the Capability Information
	 * field to the FILS Session element (both inclusive).
	 */
	aad[4] = (const u8 *) &mgmt->u.assoc_req.capab_info;
	aad_len[4] = crypt - aad[4];

	if (aes_siv_decrypt(sm->PTK.kek, sm->PTK.kek_len, crypt, end - crypt,
			    5, aad, aad_len, pos + (crypt - ie_start)) < 0) {
		wpa_printf(MSG_DEBUG,
			   "FILS: Invalid AES-SIV data in the frame");
		return -1;
	}
	wpa_hexdump(MSG_DEBUG, "FILS: Decrypted Association Request elements",
		    pos, left - AES_BLOCK_SIZE);

	if (wpa_fils_validate_key_confirm(sm, pos, left - AES_BLOCK_SIZE) < 0) {
		wpa_printf(MSG_DEBUG, "FILS: Key Confirm validation failed");
		return -1;
	}

	return left - AES_BLOCK_SIZE;
}


int fils_encrypt_assoc(struct wpa_state_machine *sm, u8 *buf,
		       size_t current_len, size_t max_len,
		       const struct wpabuf *hlp)
{
	u8 *end = buf + max_len;
	u8 *pos = buf + current_len;
	struct ieee80211_mgmt *mgmt;
	struct wpabuf *plain;
	const u8 *aad[5];
	size_t aad_len[5];

	if (!sm || !sm->PTK_valid)
		return -1;

	wpa_hexdump(MSG_DEBUG,
		    "FILS: Association Response frame before FILS processing",
		    buf, current_len);

	mgmt = (struct ieee80211_mgmt *) buf;

	/* AES-SIV AAD vectors */

	/* The AP's BSSID */
	aad[0] = mgmt->sa;
	aad_len[0] = ETH_ALEN;
	/* The STA's MAC address */
	aad[1] = mgmt->da;
	aad_len[1] = ETH_ALEN;
	/* The AP's nonce */
	aad[2] = sm->ANonce;
	aad_len[2] = FILS_NONCE_LEN;
	/* The STA's nonce */
	aad[3] = sm->SNonce;
	aad_len[3] = FILS_NONCE_LEN;
	/*
	 * The (Re)Association Response frame from the Capability Information
	 * field (the same offset in both Association and Reassociation
	 * Response frames) to the FILS Session element (both inclusive).
	 */
	aad[4] = (const u8 *) &mgmt->u.assoc_resp.capab_info;
	aad_len[4] = pos - aad[4];

	/* The following elements will be encrypted with AES-SIV */
	plain = fils_prepare_plainbuf(sm, hlp);
	if (!plain) {
		wpa_printf(MSG_DEBUG, "FILS: Plain buffer prep failed");
		return -1;
	}

	if (pos + wpabuf_len(plain) + AES_BLOCK_SIZE > end) {
		wpa_printf(MSG_DEBUG,
			   "FILS: Not enough room for FILS elements");
		wpabuf_clear_free(plain);
		return -1;
	}

	wpa_hexdump_buf_key(MSG_DEBUG, "FILS: Association Response plaintext",
			    plain);

	if (aes_siv_encrypt(sm->PTK.kek, sm->PTK.kek_len,
			    wpabuf_head(plain), wpabuf_len(plain),
			    5, aad, aad_len, pos) < 0) {
		wpabuf_clear_free(plain);
		return -1;
	}

	wpa_hexdump(MSG_DEBUG,
		    "FILS: Encrypted Association Response elements",
		    pos, AES_BLOCK_SIZE + wpabuf_len(plain));
	current_len += wpabuf_len(plain) + AES_BLOCK_SIZE;
	wpabuf_clear_free(plain);

	sm->fils_completed = 1;

	return current_len;
}


static struct wpabuf * fils_prepare_plainbuf(struct wpa_state_machine *sm,
					     const struct wpabuf *hlp)
{
	struct wpabuf *plain;
	u8 *len, *tmp, *tmp2;
	u8 hdr[2];
	u8 *gtk, stub_gtk[32];
	size_t gtk_len;
	struct wpa_group *gsm;
	size_t plain_len;
	struct wpa_auth_config *conf = &sm->wpa_auth->conf;

	plain_len = 1000 + ieee80211w_kde_len(sm);
	if (conf->transition_disable)
		plain_len += 2 + RSN_SELECTOR_LEN + 1;
	plain = wpabuf_alloc(plain_len);
	if (!plain)
		return NULL;

	/* TODO: FILS Public Key */

	/* FILS Key Confirmation */
	wpabuf_put_u8(plain, WLAN_EID_EXTENSION); /* Element ID */
	wpabuf_put_u8(plain, 1 + sm->fils_key_auth_len); /* Length */
	/* Element ID Extension */
	wpabuf_put_u8(plain, WLAN_EID_EXT_FILS_KEY_CONFIRM);
	wpabuf_put_data(plain, sm->fils_key_auth_ap, sm->fils_key_auth_len);

	/* FILS HLP Container */
	if (hlp)
		wpabuf_put_buf(plain, hlp);

	/* TODO: FILS IP Address Assignment */

	/* Key Delivery */
	gsm = sm->group;
	wpabuf_put_u8(plain, WLAN_EID_EXTENSION); /* Element ID */
	len = wpabuf_put(plain, 1);
	wpabuf_put_u8(plain, WLAN_EID_EXT_KEY_DELIVERY);
	wpa_auth_get_seqnum(sm->wpa_auth, NULL, gsm->GN,
			    wpabuf_put(plain, WPA_KEY_RSC_LEN));
	/* GTK KDE */
	gtk = gsm->GTK[gsm->GN - 1];
	gtk_len = gsm->GTK_len;
	if (conf->disable_gtk || sm->wpa_key_mgmt == WPA_KEY_MGMT_OSEN) {
		/*
		 * Provide unique random GTK to each STA to prevent use
		 * of GTK in the BSS.
		 */
		if (random_get_bytes(stub_gtk, gtk_len) < 0) {
			wpabuf_clear_free(plain);
			return NULL;
		}
		gtk = stub_gtk;
	}
	hdr[0] = gsm->GN & 0x03;
	hdr[1] = 0;
	tmp = wpabuf_put(plain, 0);
	tmp2 = wpa_add_kde(tmp, RSN_KEY_DATA_GROUPKEY, hdr, 2,
			   gtk, gtk_len);
	wpabuf_put(plain, tmp2 - tmp);

	/* IGTK KDE and BIGTK KDE */
	tmp = wpabuf_put(plain, 0);
	tmp2 = ieee80211w_kde_add(sm, tmp);
	wpabuf_put(plain, tmp2 - tmp);

	if (conf->transition_disable) {
		tmp = wpabuf_put(plain, 0);
		tmp2 = wpa_add_kde(tmp, WFA_KEY_DATA_TRANSITION_DISABLE,
				   &conf->transition_disable, 1, NULL, 0);
		wpabuf_put(plain, tmp2 - tmp);
	}

	*len = (u8 *) wpabuf_put(plain, 0) - len - 1;

#ifdef CONFIG_OCV
	if (wpa_auth_uses_ocv(sm)) {
		struct wpa_channel_info ci;
		u8 *pos;

		if (wpa_channel_info(sm->wpa_auth, &ci) != 0) {
			wpa_printf(MSG_WARNING,
				   "FILS: Failed to get channel info for OCI element");
			wpabuf_clear_free(plain);
			return NULL;
		}
#ifdef CONFIG_TESTING_OPTIONS
		if (conf->oci_freq_override_fils_assoc) {
			wpa_printf(MSG_INFO,
				   "TEST: Override OCI frequency %d -> %u MHz",
				   ci.frequency,
				   conf->oci_freq_override_fils_assoc);
			ci.frequency = conf->oci_freq_override_fils_assoc;
		}
#endif /* CONFIG_TESTING_OPTIONS */

		pos = wpabuf_put(plain, OCV_OCI_EXTENDED_LEN);
		if (ocv_insert_extended_oci(&ci, pos) < 0) {
			wpabuf_clear_free(plain);
			return NULL;
		}
	}
#endif /* CONFIG_OCV */

	return plain;
}


int fils_set_tk(struct wpa_state_machine *sm)
{
	enum wpa_alg alg;
	int klen;

	if (!sm || !sm->PTK_valid) {
		wpa_printf(MSG_DEBUG, "FILS: No valid PTK available to set TK");
		return -1;
	}
	if (sm->tk_already_set) {
		wpa_printf(MSG_DEBUG, "FILS: TK already set to the driver");
		return -1;
	}

	alg = wpa_cipher_to_alg(sm->pairwise);
	klen = wpa_cipher_key_len(sm->pairwise);

	wpa_printf(MSG_DEBUG, "FILS: Configure TK to the driver");
	if (wpa_auth_set_key(sm->wpa_auth, 0, alg, sm->addr, 0,
			     sm->PTK.tk, klen, KEY_FLAG_PAIRWISE_RX_TX)) {
		wpa_printf(MSG_DEBUG, "FILS: Failed to set TK to the driver");
		return -1;
	}

#ifdef CONFIG_PASN
	if (sm->wpa_auth->conf.secure_ltf &&
	    ieee802_11_rsnx_capab(sm->rsnxe, WLAN_RSNX_CAPAB_SECURE_LTF) &&
	    wpa_auth_set_ltf_keyseed(sm->wpa_auth, sm->addr,
				     sm->PTK.ltf_keyseed,
				     sm->PTK.ltf_keyseed_len)) {
		wpa_printf(MSG_ERROR,
			   "FILS: Failed to set LTF keyseed to driver");
		return -1;
	}
#endif /* CONFIG_PASN */

	sm->pairwise_set = true;
	sm->tk_already_set = true;

	wpa_auth_store_ptksa(sm->wpa_auth, sm->addr, sm->pairwise,
			     dot11RSNAConfigPMKLifetime, &sm->PTK);

	return 0;
}


u8 * hostapd_eid_assoc_fils_session(struct wpa_state_machine *sm, u8 *buf,
				    const u8 *fils_session, struct wpabuf *hlp)
{
	struct wpabuf *plain;
	u8 *pos = buf;

	/* FILS Session */
	*pos++ = WLAN_EID_EXTENSION; /* Element ID */
	*pos++ = 1 + FILS_SESSION_LEN; /* Length */
	*pos++ = WLAN_EID_EXT_FILS_SESSION; /* Element ID Extension */
	os_memcpy(pos, fils_session, FILS_SESSION_LEN);
	pos += FILS_SESSION_LEN;

	plain = fils_prepare_plainbuf(sm, hlp);
	if (!plain) {
		wpa_printf(MSG_DEBUG, "FILS: Plain buffer prep failed");
		return NULL;
	}

	os_memcpy(pos, wpabuf_head(plain), wpabuf_len(plain));
	pos += wpabuf_len(plain);

	wpa_printf(MSG_DEBUG, "%s: plain buf_len: %zu", __func__,
		   wpabuf_len(plain));
	wpabuf_clear_free(plain);
	sm->fils_completed = 1;
	return pos;
}

#endif /* CONFIG_FILS */


#ifdef CONFIG_OCV
int get_sta_tx_parameters(struct wpa_state_machine *sm, int ap_max_chanwidth,
			  int ap_seg1_idx, int *bandwidth, int *seg1_idx)
{
	struct wpa_authenticator *wpa_auth = sm->wpa_auth;

	if (!wpa_auth->cb->get_sta_tx_params)
		return -1;
	return wpa_auth->cb->get_sta_tx_params(wpa_auth->cb_ctx, sm->addr,
					       ap_max_chanwidth, ap_seg1_idx,
					       bandwidth, seg1_idx);
}
#endif /* CONFIG_OCV */


static int wpa_auth_validate_ml_kdes_m2(struct wpa_state_machine *sm,
					struct wpa_eapol_ie_parse *kde)
{
#ifdef CONFIG_IEEE80211BE
	int i;
	unsigned int n_links = 0;

	if (sm->mld_assoc_link_id < 0)
		return 0;

	/* MLD MAC address must be the same */
	if (!kde->mac_addr ||
	    !ether_addr_equal(kde->mac_addr, sm->peer_mld_addr)) {
		wpa_printf(MSG_DEBUG, "RSN: MLD: Invalid MLD address");
		return -1;
	}

	/* Find matching link ID and the MAC address for each link */
	for_each_link(kde->valid_mlo_links, i) {
		/*
		 * Each entry should contain the link information and the MAC
		 * address.
		 */
		if (kde->mlo_link_len[i] != 1 + ETH_ALEN) {
			wpa_printf(MSG_DEBUG,
				   "RSN: MLD: Invalid MLO Link (ID %u) KDE len=%zu",
				   i, kde->mlo_link_len[i]);
			return -1;
		}

		if (!sm->mld_links[i].valid || i == sm->mld_assoc_link_id) {
			wpa_printf(MSG_DEBUG,
				   "RSN: MLD: Invalid link ID=%u", i);
			return -1;
		}

		if (!ether_addr_equal(sm->mld_links[i].peer_addr,
				      kde->mlo_link[i] + 1)) {
			wpa_printf(MSG_DEBUG,
				   "RSN: MLD: invalid MAC address=" MACSTR
				   " expected " MACSTR " (link ID %u)",
				   MAC2STR(kde->mlo_link[i] + 1),
				   MAC2STR(sm->mld_links[i].peer_addr), i);
			return -1;
		}

		n_links++;
	}

	/* Must have the same number of MLO links (excluding the local one) */
	if (n_links != sm->n_mld_affiliated_links) {
		wpa_printf(MSG_DEBUG,
			   "RSN: MLD: Expecting %u MLD links in msg 2, but got %u",
			   sm->n_mld_affiliated_links, n_links);
		return -1;
	}
#endif /* CONFIG_IEEE80211BE */

	return 0;
}


SM_STATE(WPA_PTK, PTKCALCNEGOTIATING)
{
	struct wpa_authenticator *wpa_auth = sm->wpa_auth;
	struct wpa_ptk PTK;
	int ok = 0, psk_found = 0;
	const u8 *pmk = NULL;
	size_t pmk_len;
	int ft;
	const u8 *eapol_key_ie, *key_data, *mic;
	u16 key_info, ver, key_data_length;
	size_t mic_len, eapol_key_ie_len;
	struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *key;
	struct wpa_eapol_ie_parse kde;
	int vlan_id = 0;
	int owe_ptk_workaround = !!wpa_auth->conf.owe_ptk_workaround;
	u8 pmk_r0[PMK_LEN_MAX], pmk_r0_name[WPA_PMK_NAME_LEN];
	u8 pmk_r1[PMK_LEN_MAX];
	size_t key_len;
	u8 *key_data_buf = NULL;
	size_t key_data_buf_len = 0;
	bool derive_kdk, no_kdk = false;

	SM_ENTRY_MA(WPA_PTK, PTKCALCNEGOTIATING, wpa_ptk);
	sm->EAPOLKeyReceived = false;
	sm->update_snonce = false;
	os_memset(&PTK, 0, sizeof(PTK));

	mic_len = wpa_mic_len(sm->wpa_key_mgmt, sm->pmk_len);

	derive_kdk = sm->wpa_auth->conf.secure_ltf &&
		ieee802_11_rsnx_capab(sm->rsnxe, WLAN_RSNX_CAPAB_SECURE_LTF);

	/* WPA with IEEE 802.1X: use the derived PMK from EAP
	 * WPA-PSK: iterate through possible PSKs and select the one matching
	 * the packet */
	for (;;) {
		if (wpa_key_mgmt_wpa_psk(sm->wpa_key_mgmt) &&
		    !wpa_key_mgmt_sae(sm->wpa_key_mgmt)) {
			pmk = wpa_auth_get_psk(sm->wpa_auth, sm->addr,
					       sm->p2p_dev_addr, pmk, &pmk_len,
					       &vlan_id);
			if (!pmk)
				break;
			psk_found = 1;
#ifdef CONFIG_IEEE80211R_AP
			if (wpa_key_mgmt_ft_psk(sm->wpa_key_mgmt)) {
				os_memcpy(sm->xxkey, pmk, pmk_len);
				sm->xxkey_len = pmk_len;
			}
#endif /* CONFIG_IEEE80211R_AP */
		} else {
			pmk = sm->PMK;
			pmk_len = sm->pmk_len;
		}

		if ((!pmk || !pmk_len) && sm->pmksa) {
			wpa_printf(MSG_DEBUG, "WPA: Use PMK from PMKSA cache");
			pmk = sm->pmksa->pmk;
			pmk_len = sm->pmksa->pmk_len;
		}

		no_kdk = false;
	try_without_kdk:
		if (wpa_derive_ptk(sm, sm->SNonce, pmk, pmk_len, &PTK,
				   owe_ptk_workaround == 2, pmk_r0, pmk_r1,
				   pmk_r0_name, &key_len, no_kdk) < 0)
			break;

		if (mic_len &&
		    wpa_verify_key_mic(sm->wpa_key_mgmt, pmk_len, &PTK,
				       sm->last_rx_eapol_key,
				       sm->last_rx_eapol_key_len) == 0) {
			if (sm->PMK != pmk) {
				os_memcpy(sm->PMK, pmk, pmk_len);
				sm->pmk_len = pmk_len;
			}
			ok = 1;
			break;
		}

#ifdef CONFIG_FILS
		if (!mic_len &&
		    wpa_aead_decrypt(sm, &PTK, sm->last_rx_eapol_key,
				     sm->last_rx_eapol_key_len, NULL) == 0) {
			ok = 1;
			break;
		}
#endif /* CONFIG_FILS */

#ifdef CONFIG_OWE
		if (sm->wpa_key_mgmt == WPA_KEY_MGMT_OWE && pmk_len > 32 &&
		    owe_ptk_workaround == 1) {
			wpa_printf(MSG_DEBUG,
				   "OWE: Try PTK derivation workaround with SHA256");
			owe_ptk_workaround = 2;
			continue;
		}
#endif /* CONFIG_OWE */

		/* Some deployed STAs that advertise SecureLTF support in the
		 * RSNXE in (Re)Association Request frames, do not derive KDK
		 * during PTK generation. Try to work around this by checking if
		 * a PTK derived without KDK would result in a matching MIC. */
		if (!sm->wpa_auth->conf.force_kdk_derivation &&
		    derive_kdk && !no_kdk) {
			wpa_printf(MSG_DEBUG,
				   "Try new PTK derivation without KDK as a workaround");
			no_kdk = true;
			goto try_without_kdk;
		}

		if (!wpa_key_mgmt_wpa_psk(sm->wpa_key_mgmt) ||
		    wpa_key_mgmt_sae(sm->wpa_key_mgmt))
			break;
	}

	if (no_kdk && ok) {
		/* The workaround worked, so allow the 4-way handshake to be
		 * completed with the PTK that was derived without the KDK. */
		wpa_printf(MSG_DEBUG,
			   "PTK without KDK worked - misbehaving STA "
			   MACSTR, MAC2STR(sm->addr));
	}

	if (!ok && wpa_key_mgmt_wpa_psk_no_sae(sm->wpa_key_mgmt) &&
	    wpa_auth->conf.radius_psk && wpa_auth->cb->request_radius_psk &&
	    !sm->waiting_radius_psk) {
		wpa_printf(MSG_DEBUG, "No PSK available - ask RADIUS server");
		wpa_auth->cb->request_radius_psk(wpa_auth->cb_ctx, sm->addr,
						 sm->wpa_key_mgmt,
						 sm->ANonce,
						 sm->last_rx_eapol_key,
						 sm->last_rx_eapol_key_len);
		sm->waiting_radius_psk = 1;
		goto out;
	}

	if (!ok) {
		wpa_auth_logger(sm->wpa_auth, wpa_auth_get_spa(sm),
				LOGGER_DEBUG,
				"invalid MIC in msg 2/4 of 4-Way Handshake");
		if (psk_found)
			wpa_auth_psk_failure_report(sm->wpa_auth, sm->addr);
		goto out;
	}

	/*
	 * Note: last_rx_eapol_key length fields have already been validated in
	 * wpa_receive().
	 */
	hdr = (struct ieee802_1x_hdr *) sm->last_rx_eapol_key;
	key = (struct wpa_eapol_key *) (hdr + 1);
	mic = (u8 *) (key + 1);
	key_info = WPA_GET_BE16(key->key_info);
	key_data = mic + mic_len + 2;
	key_data_length = WPA_GET_BE16(mic + mic_len);
	if (key_data_length > sm->last_rx_eapol_key_len - sizeof(*hdr) -
	    sizeof(*key) - mic_len - 2)
		goto out;

	ver = key_info & WPA_KEY_INFO_TYPE_MASK;
	if (mic_len && (key_info & WPA_KEY_INFO_ENCR_KEY_DATA)) {
		if (ver != WPA_KEY_INFO_TYPE_HMAC_SHA1_AES &&
		    ver != WPA_KEY_INFO_TYPE_AES_128_CMAC &&
		    !wpa_use_aes_key_wrap(sm->wpa_key_mgmt)) {
			wpa_printf(MSG_INFO,
				   "Unsupported EAPOL-Key Key Data field encryption");
			goto out;
		}

		if (key_data_length < 8 || key_data_length % 8) {
			wpa_printf(MSG_INFO,
				   "RSN: Unsupported AES-WRAP len %u",
				   key_data_length);
			goto out;
		}
		key_data_length -= 8; /* AES-WRAP adds 8 bytes */
		key_data_buf = os_malloc(key_data_length);
		if (!key_data_buf)
			goto out;
		key_data_buf_len = key_data_length;
		if (aes_unwrap(PTK.kek, PTK.kek_len, key_data_length / 8,
			       key_data, key_data_buf)) {
			bin_clear_free(key_data_buf, key_data_buf_len);
			wpa_printf(MSG_INFO,
				   "RSN: AES unwrap failed - could not decrypt EAPOL-Key key data");
			goto out;
		}
		key_data = key_data_buf;
		wpa_hexdump_key(MSG_DEBUG, "RSN: Decrypted EAPOL-Key Key Data",
				key_data, key_data_length);
	}

	if (wpa_parse_kde_ies(key_data, key_data_length, &kde) < 0) {
		wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_INFO,
				 "received EAPOL-Key msg 2/4 with invalid Key Data contents");
		goto out;
	}
	if (kde.rsn_ie) {
		eapol_key_ie = kde.rsn_ie;
		eapol_key_ie_len = kde.rsn_ie_len;
	} else if (kde.osen) {
		eapol_key_ie = kde.osen;
		eapol_key_ie_len = kde.osen_len;
	} else {
		eapol_key_ie = kde.wpa_ie;
		eapol_key_ie_len = kde.wpa_ie_len;
	}
	ft = sm->wpa == WPA_VERSION_WPA2 && wpa_key_mgmt_ft(sm->wpa_key_mgmt);
	if (!sm->wpa_ie ||
	    wpa_compare_rsn_ie(ft, sm->wpa_ie, sm->wpa_ie_len,
			       eapol_key_ie, eapol_key_ie_len)) {
		wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_INFO,
				"WPA IE from (Re)AssocReq did not match with msg 2/4");
		if (sm->wpa_ie) {
			wpa_hexdump(MSG_DEBUG, "WPA IE in AssocReq",
				    sm->wpa_ie, sm->wpa_ie_len);
		}
		wpa_hexdump(MSG_DEBUG, "WPA IE in msg 2/4",
			    eapol_key_ie, eapol_key_ie_len);
		/* MLME-DEAUTHENTICATE.request */
		wpa_sta_disconnect(wpa_auth, sm->addr,
				   WLAN_REASON_PREV_AUTH_NOT_VALID);
		goto out;
	}
	if ((!sm->rsnxe && kde.rsnxe) ||
	    (sm->rsnxe && !kde.rsnxe) ||
	    (sm->rsnxe && kde.rsnxe &&
	     (sm->rsnxe_len != kde.rsnxe_len ||
	      os_memcmp(sm->rsnxe, kde.rsnxe, sm->rsnxe_len) != 0))) {
		wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_INFO,
				"RSNXE from (Re)AssocReq did not match the one in EAPOL-Key msg 2/4");
		wpa_hexdump(MSG_DEBUG, "RSNXE in AssocReq",
			    sm->rsnxe, sm->rsnxe_len);
		wpa_hexdump(MSG_DEBUG, "RSNXE in EAPOL-Key msg 2/4",
			    kde.rsnxe, kde.rsnxe_len);
		/* MLME-DEAUTHENTICATE.request */
		wpa_sta_disconnect(wpa_auth, sm->addr,
				   WLAN_REASON_PREV_AUTH_NOT_VALID);
		goto out;
	}
#ifdef CONFIG_OCV
	if (wpa_auth_uses_ocv(sm)) {
		struct wpa_channel_info ci;
		int tx_chanwidth;
		int tx_seg1_idx;
		enum oci_verify_result res;

		if (wpa_channel_info(wpa_auth, &ci) != 0) {
			wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm),
					LOGGER_INFO,
					"Failed to get channel info to validate received OCI in EAPOL-Key 2/4");
			goto out;
		}

		if (get_sta_tx_parameters(sm,
					  channel_width_to_int(ci.chanwidth),
					  ci.seg1_idx, &tx_chanwidth,
					  &tx_seg1_idx) < 0)
			goto out;

		res = ocv_verify_tx_params(kde.oci, kde.oci_len, &ci,
					   tx_chanwidth, tx_seg1_idx);
		if (wpa_auth_uses_ocv(sm) == 2 && res == OCI_NOT_FOUND) {
			/* Work around misbehaving STAs */
			wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm),
					 LOGGER_INFO,
					 "Disable OCV with a STA that does not send OCI");
			wpa_auth_set_ocv(sm, 0);
		} else if (res != OCI_SUCCESS) {
			wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm),
					 LOGGER_INFO,
					 "OCV failed: %s", ocv_errorstr);
			if (wpa_auth->conf.msg_ctx)
				wpa_msg(wpa_auth->conf.msg_ctx, MSG_INFO,
					OCV_FAILURE "addr=" MACSTR
					" frame=eapol-key-m2 error=%s",
					MAC2STR(wpa_auth_get_spa(sm)),
					ocv_errorstr);
			goto out;
		}
	}
#endif /* CONFIG_OCV */
#ifdef CONFIG_IEEE80211R_AP
	if (ft && ft_check_msg_2_of_4(wpa_auth, sm, &kde) < 0) {
		wpa_sta_disconnect(wpa_auth, sm->addr,
				   WLAN_REASON_PREV_AUTH_NOT_VALID);
		goto out;
	}
#endif /* CONFIG_IEEE80211R_AP */
#ifdef CONFIG_P2P
	if (kde.ip_addr_req && kde.ip_addr_req[0] &&
	    wpa_auth->ip_pool && WPA_GET_BE32(sm->ip_addr) == 0) {
		int idx;
		wpa_printf(MSG_DEBUG,
			   "P2P: IP address requested in EAPOL-Key exchange");
		idx = bitfield_get_first_zero(wpa_auth->ip_pool);
		if (idx >= 0) {
			u32 start = WPA_GET_BE32(wpa_auth->conf.ip_addr_start);
			bitfield_set(wpa_auth->ip_pool, idx);
			sm->ip_addr_bit = idx;
			WPA_PUT_BE32(sm->ip_addr, start + idx);
			wpa_printf(MSG_DEBUG,
				   "P2P: Assigned IP address %u.%u.%u.%u to "
				   MACSTR " (bit %u)",
				   sm->ip_addr[0], sm->ip_addr[1],
				   sm->ip_addr[2], sm->ip_addr[3],
				   MAC2STR(wpa_auth_get_spa(sm)),
				   sm->ip_addr_bit);
		}
	}
#endif /* CONFIG_P2P */

#ifdef CONFIG_DPP2
	if (DPP_VERSION > 1 && kde.dpp_kde) {
		wpa_printf(MSG_DEBUG,
			   "DPP: peer Protocol Version %u Flags 0x%x",
			   kde.dpp_kde[0], kde.dpp_kde[1]);
		if (sm->wpa_key_mgmt == WPA_KEY_MGMT_DPP &&
		    wpa_auth->conf.dpp_pfs != 2 &&
		    (kde.dpp_kde[1] & DPP_KDE_PFS_ALLOWED) &&
		    !sm->dpp_z) {
			wpa_printf(MSG_INFO,
				   "DPP: Peer indicated it supports PFS and local configuration allows this, but PFS was not negotiated for the association");
			wpa_sta_disconnect(wpa_auth, sm->addr,
					   WLAN_REASON_PREV_AUTH_NOT_VALID);
			goto out;
		}
	}
#endif /* CONFIG_DPP2 */

	if (wpa_auth_validate_ml_kdes_m2(sm, &kde) < 0) {
		wpa_sta_disconnect(wpa_auth, sm->addr,
				   WLAN_REASON_PREV_AUTH_NOT_VALID);
		return;
	}

	if (vlan_id && wpa_key_mgmt_wpa_psk(sm->wpa_key_mgmt) &&
	    wpa_auth_update_vlan(wpa_auth, sm->addr, vlan_id) < 0) {
		wpa_sta_disconnect(wpa_auth, sm->addr,
				   WLAN_REASON_PREV_AUTH_NOT_VALID);
		goto out;
	}

	sm->pending_1_of_4_timeout = 0;
	eloop_cancel_timeout(wpa_send_eapol_timeout, sm->wpa_auth, sm);

	if (wpa_key_mgmt_wpa_psk(sm->wpa_key_mgmt) && sm->PMK != pmk) {
		/* PSK may have changed from the previous choice, so update
		 * state machine data based on whatever PSK was selected here.
		 */
		os_memcpy(sm->PMK, pmk, PMK_LEN);
		sm->pmk_len = PMK_LEN;
	}

	sm->MICVerified = true;

#ifdef CONFIG_IEEE80211R_AP
	if (wpa_key_mgmt_ft(sm->wpa_key_mgmt) && !sm->ft_completed) {
		wpa_printf(MSG_DEBUG, "FT: Store PMK-R0/PMK-R1");
		wpa_auth_ft_store_keys(sm, pmk_r0, pmk_r1, pmk_r0_name,
				       key_len);
	}
#endif /* CONFIG_IEEE80211R_AP */

	os_memcpy(&sm->PTK, &PTK, sizeof(PTK));
	forced_memzero(&PTK, sizeof(PTK));
	sm->PTK_valid = true;
out:
	forced_memzero(pmk_r0, sizeof(pmk_r0));
	forced_memzero(pmk_r1, sizeof(pmk_r1));
	bin_clear_free(key_data_buf, key_data_buf_len);
}


SM_STATE(WPA_PTK, PTKCALCNEGOTIATING2)
{
	SM_ENTRY_MA(WPA_PTK, PTKCALCNEGOTIATING2, wpa_ptk);
	sm->TimeoutCtr = 0;
}


static int ieee80211w_kde_len(struct wpa_state_machine *sm)
{
	size_t len = 0;
	struct wpa_authenticator *wpa_auth = sm->wpa_auth;

	if (sm->mgmt_frame_prot) {
		len += 2 + RSN_SELECTOR_LEN + WPA_IGTK_KDE_PREFIX_LEN;
		len += wpa_cipher_key_len(wpa_auth->conf.group_mgmt_cipher);
	}

	if (wpa_auth->conf.tx_bss_auth)
		wpa_auth = wpa_auth->conf.tx_bss_auth;
	if (sm->mgmt_frame_prot && sm->wpa_auth->conf.beacon_prot) {
		len += 2 + RSN_SELECTOR_LEN + WPA_BIGTK_KDE_PREFIX_LEN;
		len += wpa_cipher_key_len(wpa_auth->conf.group_mgmt_cipher);
	}

	return len;
}


static u8 * ieee80211w_kde_add(struct wpa_state_machine *sm, u8 *pos)
{
	struct wpa_igtk_kde igtk;
	struct wpa_bigtk_kde bigtk;
	struct wpa_group *gsm = sm->group;
	u8 rsc[WPA_KEY_RSC_LEN];
	struct wpa_authenticator *wpa_auth = sm->wpa_auth;
	struct wpa_auth_config *conf = &wpa_auth->conf;
	size_t len = wpa_cipher_key_len(conf->group_mgmt_cipher);

	if (!sm->mgmt_frame_prot)
		return pos;

#ifdef CONFIG_IEEE80211BE
	if (sm->mld_assoc_link_id >= 0)
		return pos; /* Use per-link MLO KDEs instead */
#endif /* CONFIG_IEEE80211BE */

	igtk.keyid[0] = gsm->GN_igtk;
	igtk.keyid[1] = 0;
	if (gsm->wpa_group_state != WPA_GROUP_SETKEYSDONE ||
	    wpa_auth_get_seqnum(sm->wpa_auth, NULL, gsm->GN_igtk, rsc) < 0)
		os_memset(igtk.pn, 0, sizeof(igtk.pn));
	else
		os_memcpy(igtk.pn, rsc, sizeof(igtk.pn));
	os_memcpy(igtk.igtk, gsm->IGTK[gsm->GN_igtk - 4], len);
	if (conf->disable_gtk || sm->wpa_key_mgmt == WPA_KEY_MGMT_OSEN) {
		/*
		 * Provide unique random IGTK to each STA to prevent use of
		 * IGTK in the BSS.
		 */
		if (random_get_bytes(igtk.igtk, len) < 0)
			return pos;
	}
	pos = wpa_add_kde(pos, RSN_KEY_DATA_IGTK,
			  (const u8 *) &igtk, WPA_IGTK_KDE_PREFIX_LEN + len,
			  NULL, 0);
	forced_memzero(&igtk, sizeof(igtk));

	if (wpa_auth->conf.tx_bss_auth) {
		wpa_auth = wpa_auth->conf.tx_bss_auth;
		conf = &wpa_auth->conf;
		len = wpa_cipher_key_len(conf->group_mgmt_cipher);
		gsm = wpa_auth->group;
	}

	if (!sm->wpa_auth->conf.beacon_prot)
		return pos;

	bigtk.keyid[0] = gsm->GN_bigtk;
	bigtk.keyid[1] = 0;
	if (gsm->wpa_group_state != WPA_GROUP_SETKEYSDONE ||
	    wpa_auth_get_seqnum(sm->wpa_auth, NULL, gsm->GN_bigtk, rsc) < 0)
		os_memset(bigtk.pn, 0, sizeof(bigtk.pn));
	else
		os_memcpy(bigtk.pn, rsc, sizeof(bigtk.pn));
	os_memcpy(bigtk.bigtk, gsm->BIGTK[gsm->GN_bigtk - 6], len);
	if (sm->wpa_key_mgmt == WPA_KEY_MGMT_OSEN) {
		/*
		 * Provide unique random BIGTK to each OSEN STA to prevent use
		 * of BIGTK in the BSS.
		 */
		if (random_get_bytes(bigtk.bigtk, len) < 0)
			return pos;
	}
	pos = wpa_add_kde(pos, RSN_KEY_DATA_BIGTK,
			  (const u8 *) &bigtk, WPA_BIGTK_KDE_PREFIX_LEN + len,
			  NULL, 0);
	forced_memzero(&bigtk, sizeof(bigtk));

	return pos;
}


static int ocv_oci_len(struct wpa_state_machine *sm)
{
#ifdef CONFIG_OCV
	if (wpa_auth_uses_ocv(sm))
		return OCV_OCI_KDE_LEN;
#endif /* CONFIG_OCV */
	return 0;
}


static int ocv_oci_add(struct wpa_state_machine *sm, u8 **argpos,
		       unsigned int freq)
{
#ifdef CONFIG_OCV
	struct wpa_channel_info ci;

	if (!wpa_auth_uses_ocv(sm))
		return 0;

	if (wpa_channel_info(sm->wpa_auth, &ci) != 0) {
		wpa_printf(MSG_WARNING,
			   "Failed to get channel info for OCI element");
		return -1;
	}
#ifdef CONFIG_TESTING_OPTIONS
	if (freq) {
		wpa_printf(MSG_INFO,
			   "TEST: Override OCI KDE frequency %d -> %u MHz",
			   ci.frequency, freq);
		ci.frequency = freq;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	return ocv_insert_oci_kde(&ci, argpos);
#else /* CONFIG_OCV */
	return 0;
#endif /* CONFIG_OCV */
}


#ifdef CONFIG_TESTING_OPTIONS
static u8 * replace_ie(const char *name, const u8 *old_buf, size_t *len, u8 eid,
		       const u8 *ie, size_t ie_len)
{
	const u8 *elem;
	u8 *buf;

	wpa_printf(MSG_DEBUG, "TESTING: %s EAPOL override", name);
	wpa_hexdump(MSG_DEBUG, "TESTING: wpa_ie before override",
		    old_buf, *len);
	buf = os_malloc(*len + ie_len);
	if (!buf)
		return NULL;
	os_memcpy(buf, old_buf, *len);
	elem = get_ie(buf, *len, eid);
	if (elem) {
		u8 elem_len = 2 + elem[1];

		os_memmove((void *) elem, elem + elem_len,
			   *len - (elem - buf) - elem_len);
		*len -= elem_len;
	}
	os_memcpy(buf + *len, ie, ie_len);
	*len += ie_len;
	wpa_hexdump(MSG_DEBUG, "TESTING: wpa_ie after EAPOL override",
		    buf, *len);

	return buf;
}
#endif /* CONFIG_TESTING_OPTIONS */


#ifdef CONFIG_IEEE80211BE

void wpa_auth_ml_get_key_info(struct wpa_authenticator *a,
			      struct wpa_auth_ml_link_key_info *info,
			      bool mgmt_frame_prot, bool beacon_prot)
{
	struct wpa_group *gsm = a->group;
	u8 rsc[WPA_KEY_RSC_LEN];

	wpa_printf(MSG_DEBUG,
		   "MLD: Get group key info: link_id=%u, IGTK=%u, BIGTK=%u",
		   info->link_id, mgmt_frame_prot, beacon_prot);

	info->gtkidx = gsm->GN & 0x03;
	info->gtk = gsm->GTK[gsm->GN - 1];
	info->gtk_len = gsm->GTK_len;

	if (wpa_auth_get_seqnum(a, NULL, gsm->GN, rsc) < 0)
		os_memset(info->pn, 0, sizeof(info->pn));
	else
		os_memcpy(info->pn, rsc, sizeof(info->pn));

	if (!mgmt_frame_prot)
		return;

	info->igtkidx = gsm->GN_igtk;
	info->igtk = gsm->IGTK[gsm->GN_igtk - 4];
	info->igtk_len = wpa_cipher_key_len(a->conf.group_mgmt_cipher);

	if (wpa_auth_get_seqnum(a, NULL, gsm->GN_igtk, rsc) < 0)
		os_memset(info->ipn, 0, sizeof(info->ipn));
	else
		os_memcpy(info->ipn, rsc, sizeof(info->ipn));

	if (!beacon_prot)
		return;

	if (a->conf.tx_bss_auth) {
		a = a->conf.tx_bss_auth;
		gsm = a->group;
	}

	info->bigtkidx = gsm->GN_bigtk;
	info->bigtk = gsm->BIGTK[gsm->GN_bigtk - 6];

	if (wpa_auth_get_seqnum(a, NULL, gsm->GN_bigtk, rsc) < 0)
		os_memset(info->bipn, 0, sizeof(info->bipn));
	else
		os_memcpy(info->bipn, rsc, sizeof(info->bipn));
}


static void wpa_auth_get_ml_key_info(struct wpa_authenticator *wpa_auth,
				     struct wpa_auth_ml_key_info *info)
{
	if (!wpa_auth->cb->get_ml_key_info)
		return;

	wpa_auth->cb->get_ml_key_info(wpa_auth->cb_ctx, info);
}


static size_t wpa_auth_ml_group_kdes_len(struct wpa_state_machine *sm)
{
	struct wpa_authenticator *wpa_auth;
	size_t kde_len = 0;
	int link_id;

	if (sm->mld_assoc_link_id < 0)
		return 0;

	for (link_id = 0; link_id < MAX_NUM_MLD_LINKS; link_id++) {
		if (!sm->mld_links[link_id].valid)
			continue;

		wpa_auth = sm->mld_links[link_id].wpa_auth;
		if (!wpa_auth || !wpa_auth->group)
			continue;

		/* MLO GTK KDE
		 * Header + Key ID + Tx + LinkID + PN + GTK */
		kde_len += KDE_HDR_LEN + 1 + RSN_PN_LEN;
		kde_len += wpa_auth->group->GTK_len;

		if (!sm->mgmt_frame_prot)
			continue;

		if (wpa_auth->conf.tx_bss_auth)
			wpa_auth = wpa_auth->conf.tx_bss_auth;

		/* MLO IGTK KDE
		 * Header + Key ID + IPN + LinkID + IGTK */
		kde_len += KDE_HDR_LEN + WPA_IGTK_KDE_PREFIX_LEN + 1;
		kde_len += wpa_cipher_key_len(wpa_auth->conf.group_mgmt_cipher);

		if (!wpa_auth->conf.beacon_prot)
			continue;

		/* MLO BIGTK KDE
		 * Header + Key ID + BIPN + LinkID + BIGTK */
		kde_len += KDE_HDR_LEN + WPA_BIGTK_KDE_PREFIX_LEN + 1;
		kde_len += wpa_cipher_key_len(wpa_auth->conf.group_mgmt_cipher);
	}

	wpa_printf(MSG_DEBUG, "MLO Group KDEs len = %zu", kde_len);

	return kde_len;
}


static u8 * wpa_auth_ml_group_kdes(struct wpa_state_machine *sm, u8 *pos)
{
	struct wpa_auth_ml_key_info ml_key_info;
	unsigned int i, link_id;
	u8 *start = pos;

	/* First fetch the key information from all the authenticators */
	os_memset(&ml_key_info, 0, sizeof(ml_key_info));
	ml_key_info.n_mld_links = sm->n_mld_affiliated_links + 1;

	/*
	 * Assume that management frame protection and beacon protection are the
	 * same on all links.
	 */
	ml_key_info.mgmt_frame_prot = sm->mgmt_frame_prot;
	ml_key_info.beacon_prot = sm->wpa_auth->conf.beacon_prot;

	for (i = 0, link_id = 0; link_id < MAX_NUM_MLD_LINKS; link_id++) {
		if (!sm->mld_links[link_id].valid)
			continue;

		ml_key_info.links[i++].link_id = link_id;
	}

	wpa_auth_get_ml_key_info(sm->wpa_auth, &ml_key_info);

	/* Add MLO GTK KDEs */
	for (i = 0, link_id = 0; link_id < MAX_NUM_MLD_LINKS; link_id++) {
		if (!sm->mld_links[link_id].valid ||
		    !ml_key_info.links[i].gtk_len)
			continue;

		wpa_printf(MSG_DEBUG, "RSN: MLO GTK: link=%u", link_id);
		wpa_hexdump_key(MSG_DEBUG, "RSN: MLO GTK",
				ml_key_info.links[i].gtk,
				ml_key_info.links[i].gtk_len);

		*pos++ = WLAN_EID_VENDOR_SPECIFIC;
		*pos++ = RSN_SELECTOR_LEN + 1 + 6 +
			ml_key_info.links[i].gtk_len;

		RSN_SELECTOR_PUT(pos, RSN_KEY_DATA_MLO_GTK);
		pos += RSN_SELECTOR_LEN;

		*pos++ = (ml_key_info.links[i].gtkidx & 0x3) | (link_id << 4);

		os_memcpy(pos, ml_key_info.links[i].pn, 6);
		pos += 6;

		os_memcpy(pos, ml_key_info.links[i].gtk,
			  ml_key_info.links[i].gtk_len);
		pos += ml_key_info.links[i].gtk_len;

		i++;
	}

	if (!sm->mgmt_frame_prot) {
		wpa_printf(MSG_DEBUG, "RSN: MLO Group KDE len = %ld",
			   pos - start);
		return pos;
	}

	/* Add MLO IGTK KDEs */
	for (i = 0, link_id = 0; link_id < MAX_NUM_MLD_LINKS; link_id++) {
		if (!sm->mld_links[link_id].valid ||
		    !ml_key_info.links[i].igtk_len)
			continue;

		wpa_printf(MSG_DEBUG, "RSN: MLO IGTK: link=%u", link_id);
		wpa_hexdump_key(MSG_DEBUG, "RSN: MLO IGTK",
				ml_key_info.links[i].igtk,
				ml_key_info.links[i].igtk_len);

		*pos++ = WLAN_EID_VENDOR_SPECIFIC;
		*pos++ = RSN_SELECTOR_LEN + 2 + 1 +
			sizeof(ml_key_info.links[i].ipn) +
			ml_key_info.links[i].igtk_len;

		RSN_SELECTOR_PUT(pos, RSN_KEY_DATA_MLO_IGTK);
		pos += RSN_SELECTOR_LEN;

		/* Add the Key ID */
		*pos++ = ml_key_info.links[i].igtkidx;
		*pos++ = 0;

		/* Add the IPN */
		os_memcpy(pos, ml_key_info.links[i].ipn,
			  sizeof(ml_key_info.links[i].ipn));
		pos += sizeof(ml_key_info.links[i].ipn);

		*pos++ = ml_key_info.links[i].link_id << 4;

		os_memcpy(pos, ml_key_info.links[i].igtk,
			  ml_key_info.links[i].igtk_len);
		pos += ml_key_info.links[i].igtk_len;

		i++;
	}

	if (!sm->wpa_auth->conf.beacon_prot) {
		wpa_printf(MSG_DEBUG, "RSN: MLO Group KDE len = %ld",
			   pos - start);
		return pos;
	}

	/* Add MLO BIGTK KDEs */
	for (i = 0, link_id = 0; link_id < MAX_NUM_MLD_LINKS; link_id++) {
		if (!sm->mld_links[link_id].valid ||
		    !ml_key_info.links[i].bigtk ||
		    !ml_key_info.links[i].igtk_len)
			continue;

		wpa_printf(MSG_DEBUG, "RSN: MLO BIGTK: link=%u", link_id);
		wpa_hexdump_key(MSG_DEBUG, "RSN: MLO BIGTK",
				ml_key_info.links[i].bigtk,
				ml_key_info.links[i].igtk_len);

		*pos++ = WLAN_EID_VENDOR_SPECIFIC;
		*pos++ = RSN_SELECTOR_LEN + 2 + 1 +
			sizeof(ml_key_info.links[i].bipn) +
			ml_key_info.links[i].igtk_len;

		RSN_SELECTOR_PUT(pos, RSN_KEY_DATA_MLO_BIGTK);
		pos += RSN_SELECTOR_LEN;

		/* Add the Key ID */
		*pos++ = ml_key_info.links[i].bigtkidx;
		*pos++ = 0;

		/* Add the BIPN */
		os_memcpy(pos, ml_key_info.links[i].bipn,
			  sizeof(ml_key_info.links[i].bipn));
		pos += sizeof(ml_key_info.links[i].bipn);

		*pos++ = ml_key_info.links[i].link_id << 4;

		os_memcpy(pos, ml_key_info.links[i].bigtk,
			  ml_key_info.links[i].igtk_len);
		pos += ml_key_info.links[i].igtk_len;

		i++;
	}

	wpa_printf(MSG_DEBUG, "RSN: MLO Group KDE len = %ld", pos - start);
	return pos;
}

#endif /* CONFIG_IEEE80211BE */


static size_t wpa_auth_ml_kdes_len(struct wpa_state_machine *sm)
{
	size_t kde_len = 0;

#ifdef CONFIG_IEEE80211BE
	unsigned int link_id;

	if (sm->mld_assoc_link_id < 0)
		return 0;

	/* For the MAC Address KDE */
	kde_len = 2 + RSN_SELECTOR_LEN + ETH_ALEN;

	/* MLO Link KDE for each link */
	for (link_id = 0; link_id < MAX_NUM_MLD_LINKS; link_id++) {
		struct wpa_authenticator *wpa_auth;
		const u8 *ie;

		wpa_auth = wpa_get_link_auth(sm->wpa_auth, link_id);
		if (!wpa_auth)
			continue;

		kde_len += 2 + RSN_SELECTOR_LEN + 1 + ETH_ALEN;
		ie = get_ie(wpa_auth->wpa_ie, wpa_auth->wpa_ie_len,
			    WLAN_EID_RSN);
		if (ie)
			kde_len += 2 + ie[1];
		ie = get_ie(wpa_auth->wpa_ie, wpa_auth->wpa_ie_len,
			    WLAN_EID_RSNX);
		if (ie)
			kde_len += 2 + ie[1];
	}

	kde_len += wpa_auth_ml_group_kdes_len(sm);
#endif /* CONFIG_IEEE80211BE */

	return kde_len;
}


static u8 * wpa_auth_ml_kdes(struct wpa_state_machine *sm, u8 *pos)
{
#ifdef CONFIG_IEEE80211BE
	u8 link_id;
	u8 *start = pos;

	if (sm->mld_assoc_link_id < 0)
		return pos;

	wpa_printf(MSG_DEBUG, "RSN: MLD: Adding MAC Address KDE");
	pos = wpa_add_kde(pos, RSN_KEY_DATA_MAC_ADDR,
			  sm->wpa_auth->mld_addr, ETH_ALEN, NULL, 0);

	for (link_id = 0; link_id < MAX_NUM_MLD_LINKS; link_id++) {
		struct wpa_authenticator *wpa_auth;
		const u8 *rsne, *rsnxe;
		size_t rsne_len, rsnxe_len;

		wpa_auth = wpa_get_link_auth(sm->wpa_auth, link_id);
		if (!wpa_auth)
			continue;

		rsne = get_ie(wpa_auth->wpa_ie, wpa_auth->wpa_ie_len,
			     WLAN_EID_RSN);
		rsne_len = rsne ? 2 + rsne[1] : 0;

		rsnxe = get_ie(wpa_auth->wpa_ie, wpa_auth->wpa_ie_len,
			       WLAN_EID_RSNX);
		rsnxe_len = rsnxe ? 2 + rsnxe[1] : 0;

		wpa_printf(MSG_DEBUG,
			   "RSN: MLO Link: link=%u, len=%zu", link_id,
			   RSN_SELECTOR_LEN + 1 + ETH_ALEN +
			   rsne_len + rsnxe_len);

		*pos++ = WLAN_EID_VENDOR_SPECIFIC;
		*pos++ = RSN_SELECTOR_LEN + 1 + ETH_ALEN +
			rsne_len + rsnxe_len;

		RSN_SELECTOR_PUT(pos, RSN_KEY_DATA_MLO_LINK);
		pos += RSN_SELECTOR_LEN;

		/* Add the Link Information */
		*pos = link_id;
		if (rsne_len)
			*pos |= RSN_MLO_LINK_KDE_LI_RSNE_INFO;
		if (rsnxe_len)
			*pos |= RSN_MLO_LINK_KDE_LI_RSNXE_INFO;

		pos++;
		os_memcpy(pos, wpa_auth->addr, ETH_ALEN);
		pos += ETH_ALEN;

		if (rsne_len) {
			os_memcpy(pos, rsne, rsne_len);
			pos += rsne_len;
		}

		if (rsnxe_len) {
			os_memcpy(pos, rsnxe, rsnxe_len);
			pos += rsnxe_len;
		}
	}

	wpa_printf(MSG_DEBUG, "RSN: MLO Link KDE len = %ld", pos - start);
	pos = wpa_auth_ml_group_kdes(sm, pos);
#endif /* CONFIG_IEEE80211BE */

	return pos;
}


SM_STATE(WPA_PTK, PTKINITNEGOTIATING)
{
	u8 rsc[WPA_KEY_RSC_LEN], *_rsc, *gtk, *kde = NULL, *pos, stub_gtk[32];
	size_t gtk_len, kde_len = 0, wpa_ie_len;
	struct wpa_group *gsm = sm->group;
	u8 *wpa_ie;
	int secure, gtkidx, encr = 0;
	u8 *wpa_ie_buf = NULL, *wpa_ie_buf2 = NULL;
	u8 hdr[2];
	struct wpa_auth_config *conf = &sm->wpa_auth->conf;
#ifdef CONFIG_IEEE80211BE
	bool is_mld = sm->mld_assoc_link_id >= 0;
#else /* CONFIG_IEEE80211BE */
	bool is_mld = false;
#endif /* CONFIG_IEEE80211BE */

	SM_ENTRY_MA(WPA_PTK, PTKINITNEGOTIATING, wpa_ptk);
	sm->TimeoutEvt = false;

	sm->TimeoutCtr++;
	if (conf->wpa_disable_eapol_key_retries && sm->TimeoutCtr > 1) {
		/* Do not allow retransmission of EAPOL-Key msg 3/4 */
		return;
	}
	if (sm->TimeoutCtr > conf->wpa_pairwise_update_count) {
		/* No point in sending the EAPOL-Key - we will disconnect
		 * immediately following this. */
		return;
	}

	/* Send EAPOL(1, 1, 1, Pair, P, RSC, ANonce, MIC(PTK), RSNIE, [MDIE],
	   GTK[GN], IGTK, [BIGTK], [FTIE], [TIE * 2])
	 */
	os_memset(rsc, 0, WPA_KEY_RSC_LEN);
	wpa_auth_get_seqnum(sm->wpa_auth, NULL, gsm->GN, rsc);
	/* If FT is used, wpa_auth->wpa_ie includes both RSNIE and MDIE */
	wpa_ie = sm->wpa_auth->wpa_ie;
	wpa_ie_len = sm->wpa_auth->wpa_ie_len;
	if (sm->wpa == WPA_VERSION_WPA && (conf->wpa & WPA_PROTO_RSN) &&
	    wpa_ie_len > wpa_ie[1] + 2U && wpa_ie[0] == WLAN_EID_RSN) {
		/* WPA-only STA, remove RSN IE and possible MDIE */
		wpa_ie = wpa_ie + wpa_ie[1] + 2;
		if (wpa_ie[0] == WLAN_EID_RSNX)
			wpa_ie = wpa_ie + wpa_ie[1] + 2;
		if (wpa_ie[0] == WLAN_EID_MOBILITY_DOMAIN)
			wpa_ie = wpa_ie + wpa_ie[1] + 2;
		wpa_ie_len = wpa_ie[1] + 2;
	}
#ifdef CONFIG_TESTING_OPTIONS
	if (conf->rsne_override_eapol_set) {
		wpa_ie_buf2 = replace_ie(
			"RSNE", wpa_ie, &wpa_ie_len, WLAN_EID_RSN,
			conf->rsne_override_eapol,
			conf->rsne_override_eapol_len);
		if (!wpa_ie_buf2)
			goto done;
		wpa_ie = wpa_ie_buf2;
	}
	if (conf->rsnxe_override_eapol_set) {
		wpa_ie_buf = replace_ie(
			"RSNXE", wpa_ie, &wpa_ie_len, WLAN_EID_RSNX,
			conf->rsnxe_override_eapol,
			conf->rsnxe_override_eapol_len);
		if (!wpa_ie_buf)
			goto done;
		wpa_ie = wpa_ie_buf;
	}
#endif /* CONFIG_TESTING_OPTIONS */
	wpa_auth_logger(sm->wpa_auth, wpa_auth_get_spa(sm), LOGGER_DEBUG,
			"sending 3/4 msg of 4-Way Handshake");
	if (sm->wpa == WPA_VERSION_WPA2) {
		if (sm->use_ext_key_id && sm->TimeoutCtr == 1 &&
		    wpa_auth_set_key(sm->wpa_auth, 0,
				     wpa_cipher_to_alg(sm->pairwise),
				     sm->addr,
				     sm->keyidx_active, sm->PTK.tk,
				     wpa_cipher_key_len(sm->pairwise),
				     KEY_FLAG_PAIRWISE_RX)) {
			wpa_sta_disconnect(sm->wpa_auth, sm->addr,
					   WLAN_REASON_PREV_AUTH_NOT_VALID);
			return;
		}

#ifdef CONFIG_PASN
		if (sm->wpa_auth->conf.secure_ltf &&
		    ieee802_11_rsnx_capab(sm->rsnxe,
					  WLAN_RSNX_CAPAB_SECURE_LTF) &&
		    wpa_auth_set_ltf_keyseed(sm->wpa_auth, sm->addr,
					     sm->PTK.ltf_keyseed,
					     sm->PTK.ltf_keyseed_len)) {
			wpa_printf(MSG_ERROR,
				   "WPA: Failed to set LTF keyseed to driver");
			wpa_sta_disconnect(sm->wpa_auth, sm->addr,
					   WLAN_REASON_PREV_AUTH_NOT_VALID);
			return;
		}
#endif /* CONFIG_PASN */

		/* WPA2 send GTK in the 4-way handshake */
		secure = 1;
		gtk = gsm->GTK[gsm->GN - 1];
		gtk_len = gsm->GTK_len;
		if (conf->disable_gtk ||
		    sm->wpa_key_mgmt == WPA_KEY_MGMT_OSEN) {
			/*
			 * Provide unique random GTK to each STA to prevent use
			 * of GTK in the BSS.
			 */
			if (random_get_bytes(stub_gtk, gtk_len) < 0)
				goto done;
			gtk = stub_gtk;
		}
		gtkidx = gsm->GN;
		_rsc = rsc;
		encr = 1;
	} else {
		/* WPA does not include GTK in msg 3/4 */
		secure = 0;
		gtk = NULL;
		gtk_len = 0;
		gtkidx = 0;
		_rsc = NULL;
		if (sm->rx_eapol_key_secure) {
			/*
			 * It looks like Windows 7 supplicant tries to use
			 * Secure bit in msg 2/4 after having reported Michael
			 * MIC failure and it then rejects the 4-way handshake
			 * if msg 3/4 does not set Secure bit. Work around this
			 * by setting the Secure bit here even in the case of
			 * WPA if the supplicant used it first.
			 */
			wpa_auth_logger(sm->wpa_auth, wpa_auth_get_spa(sm),
					LOGGER_DEBUG,
					"STA used Secure bit in WPA msg 2/4 - set Secure for 3/4 as workaround");
			secure = 1;
		}
	}

	kde_len = wpa_ie_len + ieee80211w_kde_len(sm) + ocv_oci_len(sm);

	if (sm->use_ext_key_id)
		kde_len += 2 + RSN_SELECTOR_LEN + 2;

	if (gtk)
		kde_len += 2 + RSN_SELECTOR_LEN + 2 + gtk_len;
#ifdef CONFIG_IEEE80211R_AP
	if (wpa_key_mgmt_ft(sm->wpa_key_mgmt)) {
		kde_len += 2 + PMKID_LEN; /* PMKR1Name into RSN IE */
		kde_len += 300; /* FTIE + 2 * TIE */
	}
#endif /* CONFIG_IEEE80211R_AP */
#ifdef CONFIG_P2P
	if (WPA_GET_BE32(sm->ip_addr) > 0)
		kde_len += 2 + RSN_SELECTOR_LEN + 3 * 4;
#endif /* CONFIG_P2P */

	if (conf->transition_disable)
		kde_len += 2 + RSN_SELECTOR_LEN + 1;

#ifdef CONFIG_DPP2
	if (sm->wpa_key_mgmt == WPA_KEY_MGMT_DPP)
		kde_len += 2 + RSN_SELECTOR_LEN + 2;
#endif /* CONFIG_DPP2 */

	kde_len += wpa_auth_ml_kdes_len(sm);

	if (sm->ssid_protection)
		kde_len += 2 + conf->ssid_len;

#ifdef CONFIG_TESTING_OPTIONS
	if (conf->eapol_m3_elements)
		kde_len += wpabuf_len(conf->eapol_m3_elements);
#endif /* CONFIG_TESTING_OPTIONS */

	kde = os_malloc(kde_len);
	if (!kde)
		goto done;

	pos = kde;
	if (!is_mld) {
		os_memcpy(pos, wpa_ie, wpa_ie_len);
		pos += wpa_ie_len;
	}
#ifdef CONFIG_IEEE80211R_AP
	if (wpa_key_mgmt_ft(sm->wpa_key_mgmt)) {
		int res;
		size_t elen;

		elen = pos - kde;
		res = wpa_insert_pmkid(kde, &elen, sm->pmk_r1_name, true);
		if (res < 0) {
			wpa_printf(MSG_ERROR,
				   "FT: Failed to insert PMKR1Name into RSN IE in EAPOL-Key data");
			goto done;
		}
		pos -= wpa_ie_len;
		pos += elen;
	}
#endif /* CONFIG_IEEE80211R_AP */
	hdr[1] = 0;

	if (sm->use_ext_key_id) {
		hdr[0] = sm->keyidx_active & 0x01;
		pos = wpa_add_kde(pos, RSN_KEY_DATA_KEYID, hdr, 2, NULL, 0);
	}

	if (gtk && !is_mld) {
		hdr[0] = gtkidx & 0x03;
		pos = wpa_add_kde(pos, RSN_KEY_DATA_GROUPKEY, hdr, 2,
				  gtk, gtk_len);
	}
	pos = ieee80211w_kde_add(sm, pos);
	if (ocv_oci_add(sm, &pos, conf->oci_freq_override_eapol_m3) < 0)
		goto done;

#ifdef CONFIG_IEEE80211R_AP
	if (wpa_key_mgmt_ft(sm->wpa_key_mgmt)) {
		int res;

		if (sm->assoc_resp_ftie &&
		    kde + kde_len - pos >= 2 + sm->assoc_resp_ftie[1]) {
			os_memcpy(pos, sm->assoc_resp_ftie,
				  2 + sm->assoc_resp_ftie[1]);
			res = 2 + sm->assoc_resp_ftie[1];
		} else {
			res = wpa_write_ftie(conf, sm->wpa_key_mgmt,
					     sm->xxkey_len,
					     conf->r0_key_holder,
					     conf->r0_key_holder_len,
					     NULL, NULL, pos,
					     kde + kde_len - pos,
					     NULL, 0, 0);
		}
		if (res < 0) {
			wpa_printf(MSG_ERROR,
				   "FT: Failed to insert FTIE into EAPOL-Key Key Data");
			goto done;
		}
		pos += res;

		/* TIE[ReassociationDeadline] (TU) */
		*pos++ = WLAN_EID_TIMEOUT_INTERVAL;
		*pos++ = 5;
		*pos++ = WLAN_TIMEOUT_REASSOC_DEADLINE;
		WPA_PUT_LE32(pos, conf->reassociation_deadline);
		pos += 4;

		/* TIE[KeyLifetime] (seconds) */
		*pos++ = WLAN_EID_TIMEOUT_INTERVAL;
		*pos++ = 5;
		*pos++ = WLAN_TIMEOUT_KEY_LIFETIME;
		WPA_PUT_LE32(pos, conf->r0_key_lifetime);
		pos += 4;
	}
#endif /* CONFIG_IEEE80211R_AP */
#ifdef CONFIG_P2P
	if (WPA_GET_BE32(sm->ip_addr) > 0) {
		u8 addr[3 * 4];
		os_memcpy(addr, sm->ip_addr, 4);
		os_memcpy(addr + 4, conf->ip_addr_mask, 4);
		os_memcpy(addr + 8, conf->ip_addr_go, 4);
		pos = wpa_add_kde(pos, WFA_KEY_DATA_IP_ADDR_ALLOC,
				  addr, sizeof(addr), NULL, 0);
	}
#endif /* CONFIG_P2P */

	if (conf->transition_disable)
		pos = wpa_add_kde(pos, WFA_KEY_DATA_TRANSITION_DISABLE,
				  &conf->transition_disable, 1, NULL, 0);

#ifdef CONFIG_DPP2
	if (DPP_VERSION > 1 && sm->wpa_key_mgmt == WPA_KEY_MGMT_DPP) {
		u8 payload[2];

		payload[0] = DPP_VERSION; /* Protocol Version */
		payload[1] = 0; /* Flags */
		if (conf->dpp_pfs == 0)
			payload[1] |= DPP_KDE_PFS_ALLOWED;
		else if (conf->dpp_pfs == 1)
			payload[1] |= DPP_KDE_PFS_ALLOWED |
				DPP_KDE_PFS_REQUIRED;
		pos = wpa_add_kde(pos, WFA_KEY_DATA_DPP,
				  payload, sizeof(payload), NULL, 0);
	}
#endif /* CONFIG_DPP2 */

	pos = wpa_auth_ml_kdes(sm, pos);

	if (sm->ssid_protection) {
		*pos++ = WLAN_EID_SSID;
		*pos++ = conf->ssid_len;
		os_memcpy(pos, conf->ssid, conf->ssid_len);
		pos += conf->ssid_len;
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (conf->eapol_m3_elements) {
		os_memcpy(pos, wpabuf_head(conf->eapol_m3_elements),
			  wpabuf_len(conf->eapol_m3_elements));
		pos += wpabuf_len(conf->eapol_m3_elements);
	}

	if (conf->eapol_m3_no_encrypt)
		encr = 0;
#endif /* CONFIG_TESTING_OPTIONS */

	wpa_send_eapol(sm->wpa_auth, sm,
		       (secure ? WPA_KEY_INFO_SECURE : 0) |
		       (wpa_mic_len(sm->wpa_key_mgmt, sm->pmk_len) ?
			WPA_KEY_INFO_MIC : 0) |
		       WPA_KEY_INFO_ACK | WPA_KEY_INFO_INSTALL |
		       WPA_KEY_INFO_KEY_TYPE,
		       _rsc, sm->ANonce, kde, pos - kde, 0, encr);
done:
	bin_clear_free(kde, kde_len);
	os_free(wpa_ie_buf);
	os_free(wpa_ie_buf2);
}


static int wpa_auth_validate_ml_kdes_m4(struct wpa_state_machine *sm)
{
#ifdef CONFIG_IEEE80211BE
	const struct ieee802_1x_hdr *hdr;
	const struct wpa_eapol_key *key;
	struct wpa_eapol_ie_parse kde;
	const u8 *key_data, *mic;
	u16 key_data_length;
	size_t mic_len;

	if (sm->mld_assoc_link_id < 0)
		return 0;

	/*
	 * Note: last_rx_eapol_key length fields have already been validated in
	 * wpa_receive().
	 */
	mic_len = wpa_mic_len(sm->wpa_key_mgmt, sm->pmk_len);

	hdr = (const struct ieee802_1x_hdr *) sm->last_rx_eapol_key;
	key = (const struct wpa_eapol_key *) (hdr + 1);
	mic = (const u8 *) (key + 1);
	key_data = mic + mic_len + 2;
	key_data_length = WPA_GET_BE16(mic + mic_len);
	if (key_data_length > sm->last_rx_eapol_key_len - sizeof(*hdr) -
	    sizeof(*key) - mic_len - 2)
		return -1;

	if (wpa_parse_kde_ies(key_data, key_data_length, &kde) < 0) {
		wpa_auth_vlogger(sm->wpa_auth, wpa_auth_get_spa(sm),
				 LOGGER_INFO,
				 "received EAPOL-Key msg 4/4 with invalid Key Data contents");
		return -1;
	}

	/* MLD MAC address must be the same */
	if (!kde.mac_addr ||
	    !ether_addr_equal(kde.mac_addr, sm->peer_mld_addr)) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Mismatching or missing MLD address in EAPOL-Key msg 4/4");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "MLD: MLD address in EAPOL-Key msg 4/4: " MACSTR,
		   MAC2STR(kde.mac_addr));
#endif /* CONFIG_IEEE80211BE */

	return 0;
}


SM_STATE(WPA_PTK, PTKINITDONE)
{
	SM_ENTRY_MA(WPA_PTK, PTKINITDONE, wpa_ptk);
	sm->EAPOLKeyReceived = false;

	if (wpa_auth_validate_ml_kdes_m4(sm) < 0) {
		wpa_sta_disconnect(sm->wpa_auth, sm->addr,
				   WLAN_REASON_PREV_AUTH_NOT_VALID);
		return;
	}

	if (sm->Pair) {
		enum wpa_alg alg = wpa_cipher_to_alg(sm->pairwise);
		int klen = wpa_cipher_key_len(sm->pairwise);
		int res;

		if (sm->use_ext_key_id)
			res = wpa_auth_set_key(sm->wpa_auth, 0, 0, sm->addr,
					       sm->keyidx_active, NULL, 0,
					       KEY_FLAG_PAIRWISE_RX_TX_MODIFY);
		else
			res = wpa_auth_set_key(sm->wpa_auth, 0, alg, sm->addr,
					       0, sm->PTK.tk, klen,
					       KEY_FLAG_PAIRWISE_RX_TX);
		if (res) {
			wpa_sta_disconnect(sm->wpa_auth, sm->addr,
					   WLAN_REASON_PREV_AUTH_NOT_VALID);
			return;
		}

#ifdef CONFIG_PASN
		if (sm->wpa_auth->conf.secure_ltf &&
		    ieee802_11_rsnx_capab(sm->rsnxe,
					  WLAN_RSNX_CAPAB_SECURE_LTF) &&
		    wpa_auth_set_ltf_keyseed(sm->wpa_auth, sm->addr,
					     sm->PTK.ltf_keyseed,
					     sm->PTK.ltf_keyseed_len)) {
			wpa_printf(MSG_ERROR,
				   "WPA: Failed to set LTF keyseed to driver");
			wpa_sta_disconnect(sm->wpa_auth, sm->addr,
					   WLAN_REASON_PREV_AUTH_NOT_VALID);
			return;
		}
#endif /* CONFIG_PASN */

		/* FIX: MLME-SetProtection.Request(TA, Tx_Rx) */
		sm->pairwise_set = true;

		wpa_auth_set_ptk_rekey_timer(sm);
		wpa_auth_store_ptksa(sm->wpa_auth, sm->addr, sm->pairwise,
				     dot11RSNAConfigPMKLifetime, &sm->PTK);

		if (wpa_key_mgmt_wpa_psk(sm->wpa_key_mgmt) ||
		    sm->wpa_key_mgmt == WPA_KEY_MGMT_DPP ||
		    sm->wpa_key_mgmt == WPA_KEY_MGMT_OWE) {
			wpa_auth_set_eapol(sm->wpa_auth, sm->addr,
					   WPA_EAPOL_authorized, 1);
		}
	}

	if (0 /* IBSS == TRUE */) {
		sm->keycount++;
		if (sm->keycount == 2) {
			wpa_auth_set_eapol(sm->wpa_auth, sm->addr,
					   WPA_EAPOL_portValid, 1);
		}
	} else {
		wpa_auth_set_eapol(sm->wpa_auth, sm->addr, WPA_EAPOL_portValid,
				   1);
	}
	wpa_auth_set_eapol(sm->wpa_auth, sm->addr, WPA_EAPOL_keyAvailable,
			   false);
	wpa_auth_set_eapol(sm->wpa_auth, sm->addr, WPA_EAPOL_keyDone, true);
	if (sm->wpa == WPA_VERSION_WPA)
		sm->PInitAKeys = true;
	else
		sm->has_GTK = true;
	wpa_auth_vlogger(sm->wpa_auth, wpa_auth_get_spa(sm), LOGGER_INFO,
			 "pairwise key handshake completed (%s)",
			 sm->wpa == WPA_VERSION_WPA ? "WPA" : "RSN");
	wpa_msg(sm->wpa_auth->conf.msg_ctx, MSG_INFO, "EAPOL-4WAY-HS-COMPLETED "
		MACSTR, MAC2STR(sm->addr));

#ifdef CONFIG_IEEE80211R_AP
	wpa_ft_push_pmk_r1(sm->wpa_auth, wpa_auth_get_spa(sm));
#endif /* CONFIG_IEEE80211R_AP */

	sm->ptkstart_without_success = 0;
}


SM_STEP(WPA_PTK)
{
	struct wpa_authenticator *wpa_auth = sm->wpa_auth;
	struct wpa_auth_config *conf = &wpa_auth->conf;

	if (sm->Init)
		SM_ENTER(WPA_PTK, INITIALIZE);
	else if (sm->Disconnect
		 /* || FIX: dot11RSNAConfigSALifetime timeout */) {
		wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_DEBUG,
				"WPA_PTK: sm->Disconnect");
		SM_ENTER(WPA_PTK, DISCONNECT);
	}
	else if (sm->DeauthenticationRequest)
		SM_ENTER(WPA_PTK, DISCONNECTED);
	else if (sm->AuthenticationRequest)
		SM_ENTER(WPA_PTK, AUTHENTICATION);
	else if (sm->ReAuthenticationRequest)
		SM_ENTER(WPA_PTK, AUTHENTICATION2);
	else if (sm->PTKRequest) {
		if (wpa_auth_sm_ptk_update(sm) < 0)
			SM_ENTER(WPA_PTK, DISCONNECTED);
		else
			SM_ENTER(WPA_PTK, PTKSTART);
	} else switch (sm->wpa_ptk_state) {
	case WPA_PTK_INITIALIZE:
		break;
	case WPA_PTK_DISCONNECT:
		SM_ENTER(WPA_PTK, DISCONNECTED);
		break;
	case WPA_PTK_DISCONNECTED:
		SM_ENTER(WPA_PTK, INITIALIZE);
		break;
	case WPA_PTK_AUTHENTICATION:
		SM_ENTER(WPA_PTK, AUTHENTICATION2);
		break;
	case WPA_PTK_AUTHENTICATION2:
		if (wpa_key_mgmt_wpa_ieee8021x(sm->wpa_key_mgmt) &&
		    wpa_auth_get_eapol(wpa_auth, sm->addr,
				       WPA_EAPOL_keyRun))
			SM_ENTER(WPA_PTK, INITPMK);
		else if (wpa_key_mgmt_wpa_psk(sm->wpa_key_mgmt) ||
			 sm->wpa_key_mgmt == WPA_KEY_MGMT_OWE
			 /* FIX: && 802.1X::keyRun */)
			SM_ENTER(WPA_PTK, INITPSK);
		else if (sm->wpa_key_mgmt == WPA_KEY_MGMT_DPP)
			SM_ENTER(WPA_PTK, INITPMK);
		break;
	case WPA_PTK_INITPMK:
		if (wpa_auth_get_eapol(wpa_auth, sm->addr,
				       WPA_EAPOL_keyAvailable)) {
			SM_ENTER(WPA_PTK, PTKSTART);
#ifdef CONFIG_DPP
		} else if (sm->wpa_key_mgmt == WPA_KEY_MGMT_DPP && sm->pmksa) {
			SM_ENTER(WPA_PTK, PTKSTART);
#endif /* CONFIG_DPP */
		} else {
			wpa_auth->dot11RSNA4WayHandshakeFailures++;
			wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm),
					LOGGER_INFO,
					"INITPMK - keyAvailable = false");
			SM_ENTER(WPA_PTK, DISCONNECT);
		}
		break;
	case WPA_PTK_INITPSK:
		if (wpa_auth_get_psk(wpa_auth, sm->addr, sm->p2p_dev_addr,
				     NULL, NULL, NULL)) {
			SM_ENTER(WPA_PTK, PTKSTART);
#ifdef CONFIG_SAE
		} else if (wpa_auth_uses_sae(sm) && sm->pmksa) {
			SM_ENTER(WPA_PTK, PTKSTART);
#endif /* CONFIG_SAE */
		} else if (wpa_key_mgmt_wpa_psk_no_sae(sm->wpa_key_mgmt) &&
			   wpa_auth->conf.radius_psk) {
			wpa_printf(MSG_DEBUG,
				   "INITPSK: No PSK yet available for STA - use RADIUS later");
			SM_ENTER(WPA_PTK, PTKSTART);
		} else {
			wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm),
					LOGGER_INFO,
					"no PSK configured for the STA");
			wpa_auth->dot11RSNA4WayHandshakeFailures++;
			SM_ENTER(WPA_PTK, DISCONNECT);
		}
		break;
	case WPA_PTK_PTKSTART:
		if (sm->EAPOLKeyReceived && !sm->EAPOLKeyRequest &&
		    sm->EAPOLKeyPairwise)
			SM_ENTER(WPA_PTK, PTKCALCNEGOTIATING);
		else if (sm->TimeoutCtr > conf->wpa_pairwise_update_count) {
			wpa_auth->dot11RSNA4WayHandshakeFailures++;
			wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm),
					 LOGGER_DEBUG,
					 "PTKSTART: Retry limit %u reached",
					 conf->wpa_pairwise_update_count);
			sm->disconnect_reason =
				WLAN_REASON_4WAY_HANDSHAKE_TIMEOUT;
			SM_ENTER(WPA_PTK, DISCONNECT);
		} else if (sm->TimeoutEvt)
			SM_ENTER(WPA_PTK, PTKSTART);
		break;
	case WPA_PTK_PTKCALCNEGOTIATING:
		if (sm->MICVerified)
			SM_ENTER(WPA_PTK, PTKCALCNEGOTIATING2);
		else if (sm->EAPOLKeyReceived && !sm->EAPOLKeyRequest &&
			 sm->EAPOLKeyPairwise)
			SM_ENTER(WPA_PTK, PTKCALCNEGOTIATING);
		else if (sm->TimeoutEvt)
			SM_ENTER(WPA_PTK, PTKSTART);
		break;
	case WPA_PTK_PTKCALCNEGOTIATING2:
		SM_ENTER(WPA_PTK, PTKINITNEGOTIATING);
		break;
	case WPA_PTK_PTKINITNEGOTIATING:
		if (sm->update_snonce)
			SM_ENTER(WPA_PTK, PTKCALCNEGOTIATING);
		else if (sm->EAPOLKeyReceived && !sm->EAPOLKeyRequest &&
			 sm->EAPOLKeyPairwise && sm->MICVerified)
			SM_ENTER(WPA_PTK, PTKINITDONE);
		else if (sm->TimeoutCtr >
			 conf->wpa_pairwise_update_count ||
			 (conf->wpa_disable_eapol_key_retries &&
			  sm->TimeoutCtr > 1)) {
			wpa_auth->dot11RSNA4WayHandshakeFailures++;
			wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm),
					 LOGGER_DEBUG,
					 "PTKINITNEGOTIATING: Retry limit %u reached",
					 conf->wpa_pairwise_update_count);
			sm->disconnect_reason =
				WLAN_REASON_4WAY_HANDSHAKE_TIMEOUT;
			SM_ENTER(WPA_PTK, DISCONNECT);
		} else if (sm->TimeoutEvt)
			SM_ENTER(WPA_PTK, PTKINITNEGOTIATING);
		break;
	case WPA_PTK_PTKINITDONE:
		break;
	}
}


SM_STATE(WPA_PTK_GROUP, IDLE)
{
	SM_ENTRY_MA(WPA_PTK_GROUP, IDLE, wpa_ptk_group);
	if (sm->Init) {
		/* Init flag is not cleared here, so avoid busy
		 * loop by claiming nothing changed. */
		sm->changed = false;
	}
	sm->GTimeoutCtr = 0;
}


SM_STATE(WPA_PTK_GROUP, REKEYNEGOTIATING)
{
	u8 rsc[WPA_KEY_RSC_LEN];
	struct wpa_group *gsm = sm->group;
	const u8 *kde = NULL;
	u8 *kde_buf = NULL, *pos, hdr[2];
	size_t kde_len = 0;
	u8 *gtk, stub_gtk[32];
	struct wpa_auth_config *conf = &sm->wpa_auth->conf;
	bool is_mld = false;

#ifdef CONFIG_IEEE80211BE
	is_mld = sm->mld_assoc_link_id >= 0;
#endif /* CONFIG_IEEE80211BE */

	SM_ENTRY_MA(WPA_PTK_GROUP, REKEYNEGOTIATING, wpa_ptk_group);

	sm->GTimeoutCtr++;
	if (conf->wpa_disable_eapol_key_retries && sm->GTimeoutCtr > 1) {
		/* Do not allow retransmission of EAPOL-Key group msg 1/2 */
		return;
	}
	if (sm->GTimeoutCtr > conf->wpa_group_update_count) {
		/* No point in sending the EAPOL-Key - we will disconnect
		 * immediately following this. */
		return;
	}

	if (sm->wpa == WPA_VERSION_WPA)
		sm->PInitAKeys = false;
	sm->TimeoutEvt = false;
	/* Send EAPOL(1, 1, 1, !Pair, G, RSC, GNonce, MIC(PTK), GTK[GN]) */
	os_memset(rsc, 0, WPA_KEY_RSC_LEN);
	if (gsm->wpa_group_state == WPA_GROUP_SETKEYSDONE)
		wpa_auth_get_seqnum(sm->wpa_auth, NULL, gsm->GN, rsc);
	wpa_auth_logger(sm->wpa_auth, wpa_auth_get_spa(sm), LOGGER_DEBUG,
			"sending 1/2 msg of Group Key Handshake");

	gtk = gsm->GTK[gsm->GN - 1];
	if (conf->disable_gtk || sm->wpa_key_mgmt == WPA_KEY_MGMT_OSEN) {
		/*
		 * Provide unique random GTK to each STA to prevent use
		 * of GTK in the BSS.
		 */
		if (random_get_bytes(stub_gtk, gsm->GTK_len) < 0)
			return;
		gtk = stub_gtk;
	}

	if (sm->wpa == WPA_VERSION_WPA2 && !is_mld) {
		kde_len = 2 + RSN_SELECTOR_LEN + 2 + gsm->GTK_len +
			ieee80211w_kde_len(sm) + ocv_oci_len(sm);
		kde_buf = os_malloc(kde_len);
		if (!kde_buf)
			return;

		kde = pos = kde_buf;
		hdr[0] = gsm->GN & 0x03;
		hdr[1] = 0;
		pos = wpa_add_kde(pos, RSN_KEY_DATA_GROUPKEY, hdr, 2,
				  gtk, gsm->GTK_len);
		pos = ieee80211w_kde_add(sm, pos);
		if (ocv_oci_add(sm, &pos,
				conf->oci_freq_override_eapol_g1) < 0) {
			os_free(kde_buf);
			return;
		}
		kde_len = pos - kde;
#ifdef CONFIG_IEEE80211BE
	} else if (sm->wpa == WPA_VERSION_WPA2 && is_mld) {
		kde_len = wpa_auth_ml_group_kdes_len(sm);
		if (kde_len) {
			kde_buf = os_malloc(kde_len);
			if (!kde_buf)
				return;

			kde = pos = kde_buf;
			pos = wpa_auth_ml_group_kdes(sm, pos);
			kde_len = pos - kde_buf;
		}
#endif /* CONFIG_IEEE80211BE */
	} else {
		kde = gtk;
		kde_len = gsm->GTK_len;
	}

	wpa_send_eapol(sm->wpa_auth, sm,
		       WPA_KEY_INFO_SECURE |
		       (wpa_mic_len(sm->wpa_key_mgmt, sm->pmk_len) ?
			WPA_KEY_INFO_MIC : 0) |
		       WPA_KEY_INFO_ACK |
		       (!sm->Pair ? WPA_KEY_INFO_INSTALL : 0),
		       rsc, NULL, kde, kde_len, gsm->GN, 1);

	bin_clear_free(kde_buf, kde_len);
}


SM_STATE(WPA_PTK_GROUP, REKEYESTABLISHED)
{
	struct wpa_authenticator *wpa_auth = sm->wpa_auth;
#ifdef CONFIG_OCV
	const u8 *key_data, *mic;
	struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *key;
	struct wpa_eapol_ie_parse kde;
	size_t mic_len;
	u16 key_data_length;
#endif /* CONFIG_OCV */

	SM_ENTRY_MA(WPA_PTK_GROUP, REKEYESTABLISHED, wpa_ptk_group);
	sm->EAPOLKeyReceived = false;

#ifdef CONFIG_OCV
	mic_len = wpa_mic_len(sm->wpa_key_mgmt, sm->pmk_len);

	/*
	 * Note: last_rx_eapol_key length fields have already been validated in
	 * wpa_receive().
	 */
	hdr = (struct ieee802_1x_hdr *) sm->last_rx_eapol_key;
	key = (struct wpa_eapol_key *) (hdr + 1);
	mic = (u8 *) (key + 1);
	key_data = mic + mic_len + 2;
	key_data_length = WPA_GET_BE16(mic + mic_len);
	if (key_data_length > sm->last_rx_eapol_key_len - sizeof(*hdr) -
	    sizeof(*key) - mic_len - 2)
		return;

	if (wpa_parse_kde_ies(key_data, key_data_length, &kde) < 0) {
		wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_INFO,
				 "received EAPOL-Key group msg 2/2 with invalid Key Data contents");
		return;
	}

	if (wpa_auth_uses_ocv(sm)) {
		struct wpa_channel_info ci;
		int tx_chanwidth;
		int tx_seg1_idx;

		if (wpa_channel_info(wpa_auth, &ci) != 0) {
			wpa_auth_logger(wpa_auth, wpa_auth_get_spa(sm),
					LOGGER_INFO,
					"Failed to get channel info to validate received OCI in EAPOL-Key group 2/2");
			return;
		}

		if (get_sta_tx_parameters(sm,
					  channel_width_to_int(ci.chanwidth),
					  ci.seg1_idx, &tx_chanwidth,
					  &tx_seg1_idx) < 0)
			return;

		if (ocv_verify_tx_params(kde.oci, kde.oci_len, &ci,
					 tx_chanwidth, tx_seg1_idx) !=
		    OCI_SUCCESS) {
			wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm),
					 LOGGER_INFO,
					 "OCV failed: %s", ocv_errorstr);
			if (wpa_auth->conf.msg_ctx)
				wpa_msg(wpa_auth->conf.msg_ctx, MSG_INFO,
					OCV_FAILURE "addr=" MACSTR
					" frame=eapol-key-g2 error=%s",
					MAC2STR(wpa_auth_get_spa(sm)),
					ocv_errorstr);
			return;
		}
	}
#endif /* CONFIG_OCV */

	if (sm->GUpdateStationKeys)
		wpa_gkeydone_sta(sm);
	sm->GTimeoutCtr = 0;
	/* FIX: MLME.SetProtection.Request(TA, Tx_Rx) */
	wpa_auth_vlogger(wpa_auth, wpa_auth_get_spa(sm), LOGGER_INFO,
			 "group key handshake completed (%s)",
			 sm->wpa == WPA_VERSION_WPA ? "WPA" : "RSN");
	sm->has_GTK = true;
}


SM_STATE(WPA_PTK_GROUP, KEYERROR)
{
	SM_ENTRY_MA(WPA_PTK_GROUP, KEYERROR, wpa_ptk_group);
	if (sm->GUpdateStationKeys)
		wpa_gkeydone_sta(sm);
	if (sm->wpa_auth->conf.no_disconnect_on_group_keyerror &&
	    sm->wpa == WPA_VERSION_WPA2) {
		wpa_auth_vlogger(sm->wpa_auth, wpa_auth_get_spa(sm),
				 LOGGER_DEBUG,
				 "group key handshake failed after %u tries - allow STA to remain connected",
				 sm->wpa_auth->conf.wpa_group_update_count);
		return;
	}
	sm->Disconnect = true;
	sm->disconnect_reason = WLAN_REASON_GROUP_KEY_UPDATE_TIMEOUT;
	wpa_auth_vlogger(sm->wpa_auth, wpa_auth_get_spa(sm), LOGGER_INFO,
			 "group key handshake failed (%s) after %u tries",
			 sm->wpa == WPA_VERSION_WPA ? "WPA" : "RSN",
			 sm->wpa_auth->conf.wpa_group_update_count);
}


SM_STEP(WPA_PTK_GROUP)
{
	if (sm->Init || sm->PtkGroupInit) {
		SM_ENTER(WPA_PTK_GROUP, IDLE);
		sm->PtkGroupInit = false;
	} else switch (sm->wpa_ptk_group_state) {
	case WPA_PTK_GROUP_IDLE:
		if (sm->GUpdateStationKeys ||
		    (sm->wpa == WPA_VERSION_WPA && sm->PInitAKeys))
			SM_ENTER(WPA_PTK_GROUP, REKEYNEGOTIATING);
		break;
	case WPA_PTK_GROUP_REKEYNEGOTIATING:
		if (sm->EAPOLKeyReceived && !sm->EAPOLKeyRequest &&
		    !sm->EAPOLKeyPairwise && sm->MICVerified)
			SM_ENTER(WPA_PTK_GROUP, REKEYESTABLISHED);
		else if (sm->GTimeoutCtr >
			 sm->wpa_auth->conf.wpa_group_update_count ||
			 (sm->wpa_auth->conf.wpa_disable_eapol_key_retries &&
			  sm->GTimeoutCtr > 1))
			SM_ENTER(WPA_PTK_GROUP, KEYERROR);
		else if (sm->TimeoutEvt)
			SM_ENTER(WPA_PTK_GROUP, REKEYNEGOTIATING);
		break;
	case WPA_PTK_GROUP_KEYERROR:
		SM_ENTER(WPA_PTK_GROUP, IDLE);
		break;
	case WPA_PTK_GROUP_REKEYESTABLISHED:
		SM_ENTER(WPA_PTK_GROUP, IDLE);
		break;
	}
}


static int wpa_gtk_update(struct wpa_authenticator *wpa_auth,
			  struct wpa_group *group)
{
	struct wpa_auth_config *conf = &wpa_auth->conf;
	int ret = 0;
	size_t len;

	os_memcpy(group->GNonce, group->Counter, WPA_NONCE_LEN);
	inc_byte_array(group->Counter, WPA_NONCE_LEN);
	if (wpa_gmk_to_gtk(group->GMK, "Group key expansion",
			   wpa_auth->addr, group->GNonce,
			   group->GTK[group->GN - 1], group->GTK_len) < 0)
		ret = -1;
	wpa_hexdump_key(MSG_DEBUG, "GTK",
			group->GTK[group->GN - 1], group->GTK_len);

	if (conf->ieee80211w != NO_MGMT_FRAME_PROTECTION) {
		len = wpa_cipher_key_len(conf->group_mgmt_cipher);
		os_memcpy(group->GNonce, group->Counter, WPA_NONCE_LEN);
		inc_byte_array(group->Counter, WPA_NONCE_LEN);
		if (wpa_gmk_to_gtk(group->GMK, "IGTK key expansion",
				   wpa_auth->addr, group->GNonce,
				   group->IGTK[group->GN_igtk - 4], len) < 0)
			ret = -1;
		wpa_hexdump_key(MSG_DEBUG, "IGTK",
				group->IGTK[group->GN_igtk - 4], len);
	}

	if (!wpa_auth->non_tx_beacon_prot &&
	    conf->ieee80211w == NO_MGMT_FRAME_PROTECTION)
		return ret;
	if (!conf->beacon_prot)
		return ret;

	if (wpa_auth->conf.tx_bss_auth) {
		group = wpa_auth->conf.tx_bss_auth->group;
		if (group->bigtk_set)
			return ret;
		wpa_printf(MSG_DEBUG, "Set up BIGTK for TX BSS");
	}

	len = wpa_cipher_key_len(conf->group_mgmt_cipher);
	os_memcpy(group->GNonce, group->Counter, WPA_NONCE_LEN);
	inc_byte_array(group->Counter, WPA_NONCE_LEN);
	if (wpa_gmk_to_gtk(group->GMK, "BIGTK key expansion",
			   wpa_auth->addr, group->GNonce,
			   group->BIGTK[group->GN_bigtk - 6], len) < 0)
		return -1;
	group->bigtk_set = true;
	wpa_hexdump_key(MSG_DEBUG, "BIGTK",
			group->BIGTK[group->GN_bigtk - 6], len);

	return ret;
}


static void wpa_group_gtk_init(struct wpa_authenticator *wpa_auth,
			       struct wpa_group *group)
{
	wpa_printf(MSG_DEBUG,
		   "WPA: group state machine entering state GTK_INIT (VLAN-ID %d)",
		   group->vlan_id);
	group->changed = false; /* GInit is not cleared here; avoid loop */
	group->wpa_group_state = WPA_GROUP_GTK_INIT;

	/* GTK[0..N] = 0 */
	os_memset(group->GTK, 0, sizeof(group->GTK));
	group->GN = 1;
	group->GM = 2;
	group->GN_igtk = 4;
	group->GM_igtk = 5;
	group->GN_bigtk = 6;
	group->GM_bigtk = 7;
	/* GTK[GN] = CalcGTK() */
	wpa_gtk_update(wpa_auth, group);
}


static int wpa_group_update_sta(struct wpa_state_machine *sm, void *ctx)
{
	if (ctx != NULL && ctx != sm->group)
		return 0;

	if (sm->wpa_ptk_state != WPA_PTK_PTKINITDONE) {
		wpa_auth_logger(sm->wpa_auth, wpa_auth_get_spa(sm),
				LOGGER_DEBUG,
				"Not in PTKINITDONE; skip Group Key update");
		sm->GUpdateStationKeys = false;
		return 0;
	}
	if (sm->GUpdateStationKeys) {
		/*
		 * This should not really happen, so add a debug log entry.
		 * Since we clear the GKeyDoneStations before the loop, the
		 * station needs to be counted here anyway.
		 */
		wpa_auth_logger(sm->wpa_auth, wpa_auth_get_spa(sm),
				LOGGER_DEBUG,
				"GUpdateStationKeys was already set when marking station for GTK rekeying");
	}

	/* Do not rekey GTK/IGTK when STA is in WNM-Sleep Mode */
	if (sm->is_wnmsleep)
		return 0;

	sm->group->GKeyDoneStations++;
	sm->GUpdateStationKeys = true;

	wpa_sm_step(sm);
	return 0;
}


#ifdef CONFIG_WNM_AP
/* update GTK when exiting WNM-Sleep Mode */
void wpa_wnmsleep_rekey_gtk(struct wpa_state_machine *sm)
{
	if (!sm || sm->is_wnmsleep)
		return;

	wpa_group_update_sta(sm, NULL);
}


void wpa_set_wnmsleep(struct wpa_state_machine *sm, int flag)
{
	if (sm)
		sm->is_wnmsleep = !!flag;
}


int wpa_wnmsleep_gtk_subelem(struct wpa_state_machine *sm, u8 *pos)
{
	struct wpa_auth_config *conf = &sm->wpa_auth->conf;
	struct wpa_group *gsm = sm->group;
	u8 *start = pos;

	/*
	 * GTK subelement:
	 * Sub-elem ID[1] | Length[1] | Key Info[2] | Key Length[1] | RSC[8] |
	 * Key[5..32]
	 */
	*pos++ = WNM_SLEEP_SUBELEM_GTK;
	*pos++ = 11 + gsm->GTK_len;
	/* Key ID in B0-B1 of Key Info */
	WPA_PUT_LE16(pos, gsm->GN & 0x03);
	pos += 2;
	*pos++ = gsm->GTK_len;
	if (wpa_auth_get_seqnum(sm->wpa_auth, NULL, gsm->GN, pos) != 0)
		return 0;
	pos += 8;
	os_memcpy(pos, gsm->GTK[gsm->GN - 1], gsm->GTK_len);
	if (conf->disable_gtk || sm->wpa_key_mgmt == WPA_KEY_MGMT_OSEN) {
		/*
		 * Provide unique random GTK to each STA to prevent use
		 * of GTK in the BSS.
		 */
		if (random_get_bytes(pos, gsm->GTK_len) < 0)
			return 0;
	}
	pos += gsm->GTK_len;

	wpa_printf(MSG_DEBUG, "WNM: GTK Key ID %u in WNM-Sleep Mode exit",
		   gsm->GN);
	wpa_hexdump_key(MSG_DEBUG, "WNM: GTK in WNM-Sleep Mode exit",
			gsm->GTK[gsm->GN - 1], gsm->GTK_len);

	return pos - start;
}


int wpa_wnmsleep_igtk_subelem(struct wpa_state_machine *sm, u8 *pos)
{
	struct wpa_auth_config *conf = &sm->wpa_auth->conf;
	struct wpa_group *gsm = sm->group;
	u8 *start = pos;
	size_t len = wpa_cipher_key_len(sm->wpa_auth->conf.group_mgmt_cipher);

	/*
	 * IGTK subelement:
	 * Sub-elem ID[1] | Length[1] | KeyID[2] | PN[6] | Key[16]
	 */
	*pos++ = WNM_SLEEP_SUBELEM_IGTK;
	*pos++ = 2 + 6 + len;
	WPA_PUT_LE16(pos, gsm->GN_igtk);
	pos += 2;
	if (wpa_auth_get_seqnum(sm->wpa_auth, NULL, gsm->GN_igtk, pos) != 0)
		return 0;
	pos += 6;

	os_memcpy(pos, gsm->IGTK[gsm->GN_igtk - 4], len);
	if (conf->disable_gtk || sm->wpa_key_mgmt == WPA_KEY_MGMT_OSEN) {
		/*
		 * Provide unique random IGTK to each STA to prevent use
		 * of IGTK in the BSS.
		 */
		if (random_get_bytes(pos, len) < 0)
			return 0;
	}
	pos += len;

	wpa_printf(MSG_DEBUG, "WNM: IGTK Key ID %u in WNM-Sleep Mode exit",
		   gsm->GN_igtk);
	wpa_hexdump_key(MSG_DEBUG, "WNM: IGTK in WNM-Sleep Mode exit",
			gsm->IGTK[gsm->GN_igtk - 4], len);

	return pos - start;
}


int wpa_wnmsleep_bigtk_subelem(struct wpa_state_machine *sm, u8 *pos)
{
	struct wpa_authenticator *wpa_auth = sm->wpa_auth;
	struct wpa_group *gsm = wpa_auth->group;
	u8 *start = pos;
	size_t len = wpa_cipher_key_len(wpa_auth->conf.group_mgmt_cipher);

	/*
	 * BIGTK subelement:
	 * Sub-elem ID[1] | Length[1] | KeyID[2] | PN[6] | Key[16]
	 */
	*pos++ = WNM_SLEEP_SUBELEM_BIGTK;
	*pos++ = 2 + 6 + len;
	WPA_PUT_LE16(pos, gsm->GN_bigtk);
	pos += 2;
	if (wpa_auth_get_seqnum(wpa_auth, NULL, gsm->GN_bigtk, pos) != 0)
		return 0;
	pos += 6;

	os_memcpy(pos, gsm->BIGTK[gsm->GN_bigtk - 6], len);
	if (sm->wpa_key_mgmt == WPA_KEY_MGMT_OSEN) {
		/*
		 * Provide unique random BIGTK to each STA to prevent use
		 * of BIGTK in the BSS.
		 */
		if (random_get_bytes(pos, len) < 0)
			return 0;
	}
	pos += len;

	wpa_printf(MSG_DEBUG, "WNM: BIGTK Key ID %u in WNM-Sleep Mode exit",
		   gsm->GN_bigtk);
	wpa_hexdump_key(MSG_DEBUG, "WNM: BIGTK in WNM-Sleep Mode exit",
			gsm->BIGTK[gsm->GN_bigtk - 6], len);

	return pos - start;
}

#endif /* CONFIG_WNM_AP */


static void wpa_group_update_gtk(struct wpa_authenticator *wpa_auth,
				 struct wpa_group *group)
{
	int tmp;

	tmp = group->GM;
	group->GM = group->GN;
	group->GN = tmp;
	tmp = group->GM_igtk;
	group->GM_igtk = group->GN_igtk;
	group->GN_igtk = tmp;
	tmp = group->GM_bigtk;
	group->GM_bigtk = group->GN_bigtk;
	group->GN_bigtk = tmp;
	/* "GKeyDoneStations = GNoStations" is done in more robust way by
	 * counting the STAs that are marked with GUpdateStationKeys instead of
	 * including all STAs that could be in not-yet-completed state. */
	wpa_gtk_update(wpa_auth, group);
}


static void wpa_group_setkeys(struct wpa_authenticator *wpa_auth,
			      struct wpa_group *group)
{
	wpa_printf(MSG_DEBUG,
		   "WPA: group state machine entering state SETKEYS (VLAN-ID %d)",
		   group->vlan_id);
	group->changed = true;
	group->wpa_group_state = WPA_GROUP_SETKEYS;
	group->GTKReKey = false;

#ifdef CONFIG_IEEE80211BE
	if (wpa_auth->is_ml)
		goto skip_update;
#endif /* CONFIG_IEEE80211BE */

	wpa_group_update_gtk(wpa_auth, group);

	if (group->GKeyDoneStations) {
		wpa_printf(MSG_DEBUG,
			   "wpa_group_setkeys: Unexpected GKeyDoneStations=%d when starting new GTK rekey",
			   group->GKeyDoneStations);
		group->GKeyDoneStations = 0;
	}

#ifdef CONFIG_IEEE80211BE
skip_update:
#endif /* CONFIG_IEEE80211BE */
	wpa_auth_for_each_sta(wpa_auth, wpa_group_update_sta, group);
	wpa_printf(MSG_DEBUG, "wpa_group_setkeys: GKeyDoneStations=%d",
		   group->GKeyDoneStations);
}


static int wpa_group_config_group_keys(struct wpa_authenticator *wpa_auth,
				       struct wpa_group *group)
{
	struct wpa_auth_config *conf = &wpa_auth->conf;
	int ret = 0;

	if (wpa_auth_set_key(wpa_auth, group->vlan_id,
			     wpa_cipher_to_alg(conf->wpa_group),
			     broadcast_ether_addr, group->GN,
			     group->GTK[group->GN - 1], group->GTK_len,
			     KEY_FLAG_GROUP_TX_DEFAULT) < 0)
		ret = -1;

	if (conf->ieee80211w != NO_MGMT_FRAME_PROTECTION) {
		enum wpa_alg alg;
		size_t len;

		alg = wpa_cipher_to_alg(conf->group_mgmt_cipher);
		len = wpa_cipher_key_len(conf->group_mgmt_cipher);

		if (ret == 0 &&
		    wpa_auth_set_key(wpa_auth, group->vlan_id, alg,
				     broadcast_ether_addr, group->GN_igtk,
				     group->IGTK[group->GN_igtk - 4], len,
				     KEY_FLAG_GROUP_TX_DEFAULT) < 0)
			ret = -1;

		if (ret || !conf->beacon_prot)
			return ret;
		if (wpa_auth->conf.tx_bss_auth) {
			wpa_auth = wpa_auth->conf.tx_bss_auth;
			group = wpa_auth->group;
			if (!group->bigtk_set || group->bigtk_configured)
				return ret;
		}
		if (wpa_auth_set_key(wpa_auth, group->vlan_id, alg,
				     broadcast_ether_addr, group->GN_bigtk,
				     group->BIGTK[group->GN_bigtk - 6], len,
				     KEY_FLAG_GROUP_TX_DEFAULT) < 0)
			ret = -1;
		else
			group->bigtk_configured = true;
	}

	return ret;
}


static int wpa_group_disconnect_cb(struct wpa_state_machine *sm, void *ctx)
{
	if (sm->group == ctx) {
		wpa_printf(MSG_DEBUG, "WPA: Mark STA " MACSTR
			   " for disconnection due to fatal failure",
			   MAC2STR(wpa_auth_get_spa(sm)));
		sm->Disconnect = true;
	}

	return 0;
}


static void wpa_group_fatal_failure(struct wpa_authenticator *wpa_auth,
				    struct wpa_group *group)
{
	wpa_printf(MSG_DEBUG,
		   "WPA: group state machine entering state FATAL_FAILURE");
	group->changed = true;
	group->wpa_group_state = WPA_GROUP_FATAL_FAILURE;
	wpa_auth_for_each_sta(wpa_auth, wpa_group_disconnect_cb, group);
}


static int wpa_group_setkeysdone(struct wpa_authenticator *wpa_auth,
				 struct wpa_group *group)
{
	wpa_printf(MSG_DEBUG,
		   "WPA: group state machine entering state SETKEYSDONE (VLAN-ID %d)",
		   group->vlan_id);
	group->changed = true;
	group->wpa_group_state = WPA_GROUP_SETKEYSDONE;

	if (wpa_group_config_group_keys(wpa_auth, group) < 0) {
		wpa_group_fatal_failure(wpa_auth, group);
		return -1;
	}

	return 0;
}


static void wpa_group_sm_step(struct wpa_authenticator *wpa_auth,
			      struct wpa_group *group)
{
	if (group->GInit) {
		wpa_group_gtk_init(wpa_auth, group);
	} else if (group->wpa_group_state == WPA_GROUP_FATAL_FAILURE) {
		/* Do not allow group operations */
	} else if (group->wpa_group_state == WPA_GROUP_GTK_INIT &&
		   group->GTKAuthenticator) {
		wpa_group_setkeysdone(wpa_auth, group);
	} else if (group->wpa_group_state == WPA_GROUP_SETKEYSDONE &&
		   group->GTKReKey) {
		wpa_group_setkeys(wpa_auth, group);
	} else if (group->wpa_group_state == WPA_GROUP_SETKEYS) {
		if (group->GKeyDoneStations == 0)
			wpa_group_setkeysdone(wpa_auth, group);
		else if (group->GTKReKey)
			wpa_group_setkeys(wpa_auth, group);
	}
}


static void wpa_clear_changed(struct wpa_state_machine *sm)
{
#ifdef CONFIG_IEEE80211BE
	int link_id;
#endif /* CONFIG_IEEE80211BE */

	sm->changed = false;
	sm->wpa_auth->group->changed = false;

#ifdef CONFIG_IEEE80211BE
	for_each_sm_auth(sm, link_id)
		sm->mld_links[link_id].wpa_auth->group->changed = false;
#endif /* CONFIG_IEEE80211BE */
}


static void wpa_group_sm_step_links(struct wpa_state_machine *sm)
{
#ifdef CONFIG_IEEE80211BE
	int link_id;
#endif /* CONFIG_IEEE80211BE */

	if (!sm || !sm->wpa_auth)
		return;
	wpa_group_sm_step(sm->wpa_auth, sm->wpa_auth->group);

#ifdef CONFIG_IEEE80211BE
	for_each_sm_auth(sm, link_id) {
		wpa_group_sm_step(sm->mld_links[link_id].wpa_auth,
				  sm->mld_links[link_id].wpa_auth->group);
	}
#endif /* CONFIG_IEEE80211BE */
}


static bool wpa_group_sm_changed(struct wpa_state_machine *sm)
{
#ifdef CONFIG_IEEE80211BE
	int link_id;
#endif /* CONFIG_IEEE80211BE */
	bool changed;

	if (!sm || !sm->wpa_auth)
		return false;
	changed = sm->wpa_auth->group->changed;

#ifdef CONFIG_IEEE80211BE
	for_each_sm_auth(sm, link_id)
		changed |= sm->mld_links[link_id].wpa_auth->group->changed;
#endif /* CONFIG_IEEE80211BE */

	return changed;
}


static int wpa_sm_step(struct wpa_state_machine *sm)
{
	if (!sm)
		return 0;

	if (sm->in_step_loop) {
		/* This should not happen, but if it does, make sure we do not
		 * end up freeing the state machine too early by exiting the
		 * recursive call. */
		wpa_printf(MSG_ERROR, "WPA: wpa_sm_step() called recursively");
		return 0;
	}

	sm->in_step_loop = 1;
	do {
		if (sm->pending_deinit)
			break;

		wpa_clear_changed(sm);

		SM_STEP_RUN(WPA_PTK);
		if (sm->pending_deinit)
			break;
		SM_STEP_RUN(WPA_PTK_GROUP);
		if (sm->pending_deinit)
			break;
		wpa_group_sm_step_links(sm);
	} while (sm->changed || wpa_group_sm_changed(sm));
	sm->in_step_loop = 0;

	if (sm->pending_deinit) {
		wpa_printf(MSG_DEBUG,
			   "WPA: Completing pending STA state machine deinit for "
			   MACSTR, MAC2STR(wpa_auth_get_spa(sm)));
		wpa_free_sta_sm(sm);
		return 1;
	}
	return 0;
}


static void wpa_sm_call_step(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_state_machine *sm = eloop_ctx;
	wpa_sm_step(sm);
}


void wpa_auth_sm_notify(struct wpa_state_machine *sm)
{
	if (!sm)
		return;
	eloop_register_timeout(0, 0, wpa_sm_call_step, sm, NULL);
}


void wpa_gtk_rekey(struct wpa_authenticator *wpa_auth)
{
	int tmp, i;
	struct wpa_group *group;

	if (!wpa_auth)
		return;

	group = wpa_auth->group;

	for (i = 0; i < 2; i++) {
		tmp = group->GM;
		group->GM = group->GN;
		group->GN = tmp;
		tmp = group->GM_igtk;
		group->GM_igtk = group->GN_igtk;
		group->GN_igtk = tmp;
		if (!wpa_auth->conf.tx_bss_auth) {
			tmp = group->GM_bigtk;
			group->GM_bigtk = group->GN_bigtk;
			group->GN_bigtk = tmp;
		}
		wpa_gtk_update(wpa_auth, group);
		wpa_group_config_group_keys(wpa_auth, group);
	}
}


static const char * wpa_bool_txt(int val)
{
	return val ? "TRUE" : "FALSE";
}


#define RSN_SUITE "%02x-%02x-%02x-%d"
#define RSN_SUITE_ARG(s) \
((s) >> 24) & 0xff, ((s) >> 16) & 0xff, ((s) >> 8) & 0xff, (s) & 0xff

int wpa_get_mib(struct wpa_authenticator *wpa_auth, char *buf, size_t buflen)
{
	struct wpa_auth_config *conf;
	int len = 0, ret;
	char pmkid_txt[PMKID_LEN * 2 + 1];
#ifdef CONFIG_RSN_PREAUTH
	const int preauth = 1;
#else /* CONFIG_RSN_PREAUTH */
	const int preauth = 0;
#endif /* CONFIG_RSN_PREAUTH */

	if (!wpa_auth)
		return len;
	conf = &wpa_auth->conf;

	ret = os_snprintf(buf + len, buflen - len,
			  "dot11RSNAOptionImplemented=TRUE\n"
			  "dot11RSNAPreauthenticationImplemented=%s\n"
			  "dot11RSNAEnabled=%s\n"
			  "dot11RSNAPreauthenticationEnabled=%s\n",
			  wpa_bool_txt(preauth),
			  wpa_bool_txt(conf->wpa & WPA_PROTO_RSN),
			  wpa_bool_txt(conf->rsn_preauth));
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	wpa_snprintf_hex(pmkid_txt, sizeof(pmkid_txt),
			 wpa_auth->dot11RSNAPMKIDUsed, PMKID_LEN);

	ret = os_snprintf(
		buf + len, buflen - len,
		"dot11RSNAConfigVersion=%u\n"
		"dot11RSNAConfigPairwiseKeysSupported=9999\n"
		/* FIX: dot11RSNAConfigGroupCipher */
		/* FIX: dot11RSNAConfigGroupRekeyMethod */
		/* FIX: dot11RSNAConfigGroupRekeyTime */
		/* FIX: dot11RSNAConfigGroupRekeyPackets */
		"dot11RSNAConfigGroupRekeyStrict=%u\n"
		"dot11RSNAConfigGroupUpdateCount=%u\n"
		"dot11RSNAConfigPairwiseUpdateCount=%u\n"
		"dot11RSNAConfigGroupCipherSize=%u\n"
		"dot11RSNAConfigPMKLifetime=%u\n"
		"dot11RSNAConfigPMKReauthThreshold=%u\n"
		"dot11RSNAConfigNumberOfPTKSAReplayCounters=0\n"
		"dot11RSNAConfigSATimeout=%u\n"
		"dot11RSNAAuthenticationSuiteSelected=" RSN_SUITE "\n"
		"dot11RSNAPairwiseCipherSelected=" RSN_SUITE "\n"
		"dot11RSNAGroupCipherSelected=" RSN_SUITE "\n"
		"dot11RSNAPMKIDUsed=%s\n"
		"dot11RSNAAuthenticationSuiteRequested=" RSN_SUITE "\n"
		"dot11RSNAPairwiseCipherRequested=" RSN_SUITE "\n"
		"dot11RSNAGroupCipherRequested=" RSN_SUITE "\n"
		"dot11RSNATKIPCounterMeasuresInvoked=%u\n"
		"dot11RSNA4WayHandshakeFailures=%u\n"
		"dot11RSNAConfigNumberOfGTKSAReplayCounters=0\n",
		RSN_VERSION,
		!!conf->wpa_strict_rekey,
		conf->wpa_group_update_count,
		conf->wpa_pairwise_update_count,
		wpa_cipher_key_len(conf->wpa_group) * 8,
		dot11RSNAConfigPMKLifetime,
		dot11RSNAConfigPMKReauthThreshold,
		dot11RSNAConfigSATimeout,
		RSN_SUITE_ARG(wpa_auth->dot11RSNAAuthenticationSuiteSelected),
		RSN_SUITE_ARG(wpa_auth->dot11RSNAPairwiseCipherSelected),
		RSN_SUITE_ARG(wpa_auth->dot11RSNAGroupCipherSelected),
		pmkid_txt,
		RSN_SUITE_ARG(wpa_auth->dot11RSNAAuthenticationSuiteRequested),
		RSN_SUITE_ARG(wpa_auth->dot11RSNAPairwiseCipherRequested),
		RSN_SUITE_ARG(wpa_auth->dot11RSNAGroupCipherRequested),
		wpa_auth->dot11RSNATKIPCounterMeasuresInvoked,
		wpa_auth->dot11RSNA4WayHandshakeFailures);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	/* TODO: dot11RSNAConfigPairwiseCiphersTable */
	/* TODO: dot11RSNAConfigAuthenticationSuitesTable */

	/* Private MIB */
	ret = os_snprintf(buf + len, buflen - len, "hostapdWPAGroupState=%d\n",
			  wpa_auth->group->wpa_group_state);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	return len;
}


int wpa_get_mib_sta(struct wpa_state_machine *sm, char *buf, size_t buflen)
{
	int len = 0, ret;
	u32 pairwise = 0;

	if (!sm)
		return 0;

	/* TODO: FF-FF-FF-FF-FF-FF entry for broadcast/multicast stats */

	/* dot11RSNAStatsEntry */

	pairwise = wpa_cipher_to_suite(sm->wpa == WPA_VERSION_WPA2 ?
				       WPA_PROTO_RSN : WPA_PROTO_WPA,
				       sm->pairwise);
	if (pairwise == 0)
		return 0;

	ret = os_snprintf(
		buf + len, buflen - len,
		/* TODO: dot11RSNAStatsIndex */
		"dot11RSNAStatsSTAAddress=" MACSTR "\n"
		"dot11RSNAStatsVersion=1\n"
		"dot11RSNAStatsSelectedPairwiseCipher=" RSN_SUITE "\n"
		/* TODO: dot11RSNAStatsTKIPICVErrors */
		"dot11RSNAStatsTKIPLocalMICFailures=%u\n"
		"dot11RSNAStatsTKIPRemoteMICFailures=%u\n"
		/* TODO: dot11RSNAStatsCCMPReplays */
		/* TODO: dot11RSNAStatsCCMPDecryptErrors */
		/* TODO: dot11RSNAStatsTKIPReplays */,
		MAC2STR(sm->addr),
		RSN_SUITE_ARG(pairwise),
		sm->dot11RSNAStatsTKIPLocalMICFailures,
		sm->dot11RSNAStatsTKIPRemoteMICFailures);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	/* Private MIB */
	ret = os_snprintf(buf + len, buflen - len,
			  "wpa=%d\n"
			  "AKMSuiteSelector=" RSN_SUITE "\n"
			  "hostapdWPAPTKState=%d\n"
			  "hostapdWPAPTKGroupState=%d\n"
			  "hostapdMFPR=%d\n",
			  sm->wpa,
			  RSN_SUITE_ARG(wpa_akm_to_suite(sm->wpa_key_mgmt)),
			  sm->wpa_ptk_state,
			  sm->wpa_ptk_group_state,
			  sm->mfpr);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	return len;
}


void wpa_auth_countermeasures_start(struct wpa_authenticator *wpa_auth)
{
	if (wpa_auth)
		wpa_auth->dot11RSNATKIPCounterMeasuresInvoked++;
}


int wpa_auth_pairwise_set(struct wpa_state_machine *sm)
{
	return sm && sm->pairwise_set;
}


int wpa_auth_get_pairwise(struct wpa_state_machine *sm)
{
	return sm->pairwise;
}


const u8 * wpa_auth_get_pmk(struct wpa_state_machine *sm, int *len)
{
	if (!sm)
		return NULL;
	*len = sm->pmk_len;
	return sm->PMK;
}


const u8 * wpa_auth_get_dpp_pkhash(struct wpa_state_machine *sm)
{
	if (!sm || !sm->pmksa)
		return NULL;
	return sm->pmksa->dpp_pkhash;
}


int wpa_auth_sta_key_mgmt(struct wpa_state_machine *sm)
{
	if (!sm)
		return -1;
	return sm->wpa_key_mgmt;
}


int wpa_auth_sta_wpa_version(struct wpa_state_machine *sm)
{
	if (!sm)
		return 0;
	return sm->wpa;
}


int wpa_auth_sta_ft_tk_already_set(struct wpa_state_machine *sm)
{
	if (!sm || !wpa_key_mgmt_ft(sm->wpa_key_mgmt))
		return 0;
	return sm->tk_already_set;
}


int wpa_auth_sta_fils_tk_already_set(struct wpa_state_machine *sm)
{
	if (!sm || !wpa_key_mgmt_fils(sm->wpa_key_mgmt))
		return 0;
	return sm->tk_already_set;
}


int wpa_auth_sta_clear_pmksa(struct wpa_state_machine *sm,
			     struct rsn_pmksa_cache_entry *entry)
{
	if (!sm || sm->pmksa != entry)
		return -1;
	sm->pmksa = NULL;
	return 0;
}


struct rsn_pmksa_cache_entry *
wpa_auth_sta_get_pmksa(struct wpa_state_machine *sm)
{
	return sm ? sm->pmksa : NULL;
}


void wpa_auth_sta_local_mic_failure_report(struct wpa_state_machine *sm)
{
	if (sm)
		sm->dot11RSNAStatsTKIPLocalMICFailures++;
}


const u8 * wpa_auth_get_wpa_ie(struct wpa_authenticator *wpa_auth, size_t *len)
{
	if (!wpa_auth)
		return NULL;
	*len = wpa_auth->wpa_ie_len;
	return wpa_auth->wpa_ie;
}


int wpa_auth_pmksa_add(struct wpa_state_machine *sm, const u8 *pmk,
		       unsigned int pmk_len,
		       int session_timeout, struct eapol_state_machine *eapol)
{
	if (!sm || sm->wpa != WPA_VERSION_WPA2 ||
	    sm->wpa_auth->conf.disable_pmksa_caching)
		return -1;

#ifdef CONFIG_IEEE80211R_AP
	if (pmk_len >= 2 * PMK_LEN && wpa_key_mgmt_ft(sm->wpa_key_mgmt) &&
	    wpa_key_mgmt_wpa_ieee8021x(sm->wpa_key_mgmt) &&
	    !wpa_key_mgmt_sha384(sm->wpa_key_mgmt)) {
		/* Cache MPMK/XXKey instead of initial part from MSK */
		pmk = pmk + PMK_LEN;
		pmk_len = PMK_LEN;
	} else
#endif /* CONFIG_IEEE80211R_AP */
	if (wpa_key_mgmt_sha384(sm->wpa_key_mgmt)) {
		if (pmk_len > PMK_LEN_SUITE_B_192)
			pmk_len = PMK_LEN_SUITE_B_192;
	} else if (pmk_len > PMK_LEN) {
		pmk_len = PMK_LEN;
	}

	wpa_hexdump_key(MSG_DEBUG, "RSN: Cache PMK", pmk, pmk_len);
	if (pmksa_cache_auth_add(sm->wpa_auth->pmksa, pmk, pmk_len, NULL,
				 sm->PTK.kck, sm->PTK.kck_len,
				 wpa_auth_get_aa(sm),
				 wpa_auth_get_spa(sm), session_timeout,
				 eapol, sm->wpa_key_mgmt))
		return 0;

	return -1;
}


int wpa_auth_pmksa_add_preauth(struct wpa_authenticator *wpa_auth,
			       const u8 *pmk, size_t len, const u8 *sta_addr,
			       int session_timeout,
			       struct eapol_state_machine *eapol)
{
	if (!wpa_auth)
		return -1;

	wpa_hexdump_key(MSG_DEBUG, "RSN: Cache PMK from preauth", pmk, len);
	if (pmksa_cache_auth_add(wpa_auth->pmksa, pmk, len, NULL,
				 NULL, 0,
				 wpa_auth->addr,
				 sta_addr, session_timeout, eapol,
				 WPA_KEY_MGMT_IEEE8021X))
		return 0;

	return -1;
}


int wpa_auth_pmksa_add_sae(struct wpa_authenticator *wpa_auth, const u8 *addr,
			   const u8 *pmk, size_t pmk_len, const u8 *pmkid,
			   int akmp)
{
	if (wpa_auth->conf.disable_pmksa_caching)
		return -1;

	wpa_hexdump_key(MSG_DEBUG, "RSN: Cache PMK from SAE", pmk, pmk_len);
	if (!akmp)
		akmp = WPA_KEY_MGMT_SAE;
	if (pmksa_cache_auth_add(wpa_auth->pmksa, pmk, pmk_len, pmkid,
				 NULL, 0, wpa_auth->addr, addr, 0, NULL, akmp))
		return 0;

	return -1;
}


void wpa_auth_add_sae_pmkid(struct wpa_state_machine *sm, const u8 *pmkid)
{
	os_memcpy(sm->pmkid, pmkid, PMKID_LEN);
	sm->pmkid_set = 1;
}


int wpa_auth_pmksa_add2(struct wpa_authenticator *wpa_auth, const u8 *addr,
			const u8 *pmk, size_t pmk_len, const u8 *pmkid,
			int session_timeout, int akmp, const u8 *dpp_pkhash)
{
	struct rsn_pmksa_cache_entry *entry;

	if (!wpa_auth || wpa_auth->conf.disable_pmksa_caching)
		return -1;

	wpa_hexdump_key(MSG_DEBUG, "RSN: Cache PMK (3)", pmk, PMK_LEN);
	entry = pmksa_cache_auth_add(wpa_auth->pmksa, pmk, pmk_len, pmkid,
				 NULL, 0, wpa_auth->addr, addr, session_timeout,
				 NULL, akmp);
	if (!entry)
		return -1;

	if (dpp_pkhash)
		entry->dpp_pkhash = os_memdup(dpp_pkhash, SHA256_MAC_LEN);

	return 0;
}


void wpa_auth_pmksa_remove(struct wpa_authenticator *wpa_auth,
			   const u8 *sta_addr)
{
	struct rsn_pmksa_cache_entry *pmksa;

	if (!wpa_auth || !wpa_auth->pmksa)
		return;
	pmksa = pmksa_cache_auth_get(wpa_auth->pmksa, sta_addr, NULL);
	if (pmksa) {
		wpa_printf(MSG_DEBUG, "WPA: Remove PMKSA cache entry for "
			   MACSTR " based on request", MAC2STR(sta_addr));
		pmksa_cache_free_entry(wpa_auth->pmksa, pmksa);
	}
}


int wpa_auth_pmksa_list(struct wpa_authenticator *wpa_auth, char *buf,
			size_t len)
{
	if (!wpa_auth || !wpa_auth->pmksa)
		return 0;
	return pmksa_cache_auth_list(wpa_auth->pmksa, buf, len);
}


void wpa_auth_pmksa_flush(struct wpa_authenticator *wpa_auth)
{
	if (wpa_auth && wpa_auth->pmksa)
		pmksa_cache_auth_flush(wpa_auth->pmksa);
}


#ifdef CONFIG_PMKSA_CACHE_EXTERNAL
#ifdef CONFIG_MESH

int wpa_auth_pmksa_list_mesh(struct wpa_authenticator *wpa_auth, const u8 *addr,
			     char *buf, size_t len)
{
	if (!wpa_auth || !wpa_auth->pmksa)
		return 0;

	return pmksa_cache_auth_list_mesh(wpa_auth->pmksa, addr, buf, len);
}


struct rsn_pmksa_cache_entry *
wpa_auth_pmksa_create_entry(const u8 *aa, const u8 *spa, const u8 *pmk,
			    size_t pmk_len, int akmp,
			    const u8 *pmkid, int expiration)
{
	struct rsn_pmksa_cache_entry *entry;
	struct os_reltime now;

	entry = pmksa_cache_auth_create_entry(pmk, pmk_len, pmkid, NULL, 0, aa,
					      spa, 0, NULL, akmp);
	if (!entry)
		return NULL;

	os_get_reltime(&now);
	entry->expiration = now.sec + expiration;
	return entry;
}


int wpa_auth_pmksa_add_entry(struct wpa_authenticator *wpa_auth,
			     struct rsn_pmksa_cache_entry *entry)
{
	int ret;

	if (!wpa_auth || !wpa_auth->pmksa)
		return -1;

	ret = pmksa_cache_auth_add_entry(wpa_auth->pmksa, entry);
	if (ret < 0)
		wpa_printf(MSG_DEBUG,
			   "RSN: Failed to store external PMKSA cache for "
			   MACSTR, MAC2STR(entry->spa));

	return ret;
}

#endif /* CONFIG_MESH */
#endif /* CONFIG_PMKSA_CACHE_EXTERNAL */


struct rsn_pmksa_cache *
wpa_auth_get_pmksa_cache(struct wpa_authenticator *wpa_auth)
{
	if (!wpa_auth || !wpa_auth->pmksa)
		return NULL;
	return wpa_auth->pmksa;
}


struct rsn_pmksa_cache_entry *
wpa_auth_pmksa_get(struct wpa_authenticator *wpa_auth, const u8 *sta_addr,
		   const u8 *pmkid)
{
	if (!wpa_auth || !wpa_auth->pmksa)
		return NULL;
	return pmksa_cache_auth_get(wpa_auth->pmksa, sta_addr, pmkid);
}


void wpa_auth_pmksa_set_to_sm(struct rsn_pmksa_cache_entry *pmksa,
			      struct wpa_state_machine *sm,
			      struct wpa_authenticator *wpa_auth,
			      u8 *pmkid, u8 *pmk, size_t *pmk_len)
{
	if (!sm)
		return;

	sm->pmksa = pmksa;
	os_memcpy(pmk, pmksa->pmk, pmksa->pmk_len);
	*pmk_len = pmksa->pmk_len;
	os_memcpy(pmkid, pmksa->pmkid, PMKID_LEN);
	os_memcpy(wpa_auth->dot11RSNAPMKIDUsed, pmksa->pmkid, PMKID_LEN);
}


/*
 * Remove and free the group from wpa_authenticator. This is triggered by a
 * callback to make sure nobody is currently iterating the group list while it
 * gets modified.
 */
static void wpa_group_free(struct wpa_authenticator *wpa_auth,
			   struct wpa_group *group)
{
	struct wpa_group *prev = wpa_auth->group;

	wpa_printf(MSG_DEBUG, "WPA: Remove group state machine for VLAN-ID %d",
		   group->vlan_id);

	while (prev) {
		if (prev->next == group) {
			/* This never frees the special first group as needed */
			prev->next = group->next;
			os_free(group);
			break;
		}
		prev = prev->next;
	}

}


/* Increase the reference counter for group */
static void wpa_group_get(struct wpa_authenticator *wpa_auth,
			  struct wpa_group *group)
{
	/* Skip the special first group */
	if (wpa_auth->group == group)
		return;

	group->references++;
}


/* Decrease the reference counter and maybe free the group */
static void wpa_group_put(struct wpa_authenticator *wpa_auth,
			  struct wpa_group *group)
{
	/* Skip the special first group */
	if (wpa_auth->group == group)
		return;

	group->references--;
	if (group->references)
		return;
	wpa_group_free(wpa_auth, group);
}


/*
 * Add a group that has its references counter set to zero. Caller needs to
 * call wpa_group_get() on the return value to mark the entry in use.
 */
static struct wpa_group *
wpa_auth_add_group(struct wpa_authenticator *wpa_auth, int vlan_id)
{
	struct wpa_group *group;

	if (!wpa_auth || !wpa_auth->group)
		return NULL;

	wpa_printf(MSG_DEBUG, "WPA: Add group state machine for VLAN-ID %d",
		   vlan_id);
	group = wpa_group_init(wpa_auth, vlan_id, 0);
	if (!group)
		return NULL;

	group->next = wpa_auth->group->next;
	wpa_auth->group->next = group;

	return group;
}


/*
 * Enforce that the group state machine for the VLAN is running, increase
 * reference counter as interface is up. References might have been increased
 * even if a negative value is returned.
 * Returns: -1 on error (group missing, group already failed); otherwise, 0
 */
int wpa_auth_ensure_group(struct wpa_authenticator *wpa_auth, int vlan_id)
{
	struct wpa_group *group;

	if (!wpa_auth)
		return 0;

	group = wpa_auth->group;
	while (group) {
		if (group->vlan_id == vlan_id)
			break;
		group = group->next;
	}

	if (!group) {
		group = wpa_auth_add_group(wpa_auth, vlan_id);
		if (!group)
			return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "WPA: Ensure group state machine running for VLAN ID %d",
		   vlan_id);

	wpa_group_get(wpa_auth, group);
	group->num_setup_iface++;

	if (group->wpa_group_state == WPA_GROUP_FATAL_FAILURE)
		return -1;

	return 0;
}


/*
 * Decrease reference counter, expected to be zero afterwards.
 * returns: -1 on error (group not found, group in fail state)
 *          -2 if wpa_group is still referenced
 *           0 else
 */
int wpa_auth_release_group(struct wpa_authenticator *wpa_auth, int vlan_id)
{
	struct wpa_group *group;
	int ret = 0;

	if (!wpa_auth)
		return 0;

	group = wpa_auth->group;
	while (group) {
		if (group->vlan_id == vlan_id)
			break;
		group = group->next;
	}

	if (!group)
		return -1;

	wpa_printf(MSG_DEBUG,
		   "WPA: Try stopping group state machine for VLAN ID %d",
		   vlan_id);

	if (group->num_setup_iface <= 0) {
		wpa_printf(MSG_ERROR,
			   "WPA: wpa_auth_release_group called more often than wpa_auth_ensure_group for VLAN ID %d, skipping.",
			   vlan_id);
		return -1;
	}
	group->num_setup_iface--;

	if (group->wpa_group_state == WPA_GROUP_FATAL_FAILURE)
		ret = -1;

	if (group->references > 1) {
		wpa_printf(MSG_DEBUG,
			   "WPA: Cannot stop group state machine for VLAN ID %d as references are still hold",
			   vlan_id);
		ret = -2;
	}

	wpa_group_put(wpa_auth, group);

	return ret;
}


int wpa_auth_sta_set_vlan(struct wpa_state_machine *sm, int vlan_id)
{
	struct wpa_group *group;

	if (!sm || !sm->wpa_auth)
		return 0;

	group = sm->wpa_auth->group;
	while (group) {
		if (group->vlan_id == vlan_id)
			break;
		group = group->next;
	}

	if (!group) {
		group = wpa_auth_add_group(sm->wpa_auth, vlan_id);
		if (!group)
			return -1;
	}

	if (sm->group == group)
		return 0;

	if (group->wpa_group_state == WPA_GROUP_FATAL_FAILURE)
		return -1;

	wpa_printf(MSG_DEBUG, "WPA: Moving STA " MACSTR
		   " to use group state machine for VLAN ID %d",
		   MAC2STR(wpa_auth_get_spa(sm)), vlan_id);

	wpa_group_get(sm->wpa_auth, group);
	wpa_group_put(sm->wpa_auth, sm->group);
	sm->group = group;

	return 0;
}


void wpa_auth_eapol_key_tx_status(struct wpa_authenticator *wpa_auth,
				  struct wpa_state_machine *sm, int ack)
{
	if (!wpa_auth || !sm)
		return;
	wpa_printf(MSG_DEBUG, "WPA: EAPOL-Key TX status for STA " MACSTR
		   " ack=%d", MAC2STR(wpa_auth_get_spa(sm)), ack);
	if (sm->pending_1_of_4_timeout && ack) {
		/*
		 * Some deployed supplicant implementations update their SNonce
		 * for each EAPOL-Key 2/4 message even within the same 4-way
		 * handshake and then fail to use the first SNonce when
		 * deriving the PTK. This results in unsuccessful 4-way
		 * handshake whenever the relatively short initial timeout is
		 * reached and EAPOL-Key 1/4 is retransmitted. Try to work
		 * around this by increasing the timeout now that we know that
		 * the station has received the frame.
		 */
		int timeout_ms = eapol_key_timeout_subseq;
		wpa_printf(MSG_DEBUG,
			   "WPA: Increase initial EAPOL-Key 1/4 timeout by %u ms because of acknowledged frame",
			   timeout_ms);
		eloop_cancel_timeout(wpa_send_eapol_timeout, wpa_auth, sm);
		eloop_register_timeout(timeout_ms / 1000,
				       (timeout_ms % 1000) * 1000,
				       wpa_send_eapol_timeout, wpa_auth, sm);
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (sm->eapol_status_cb) {
		sm->eapol_status_cb(sm->eapol_status_cb_ctx1,
				    sm->eapol_status_cb_ctx2);
		sm->eapol_status_cb = NULL;
	}
#endif /* CONFIG_TESTING_OPTIONS */
}


int wpa_auth_uses_sae(struct wpa_state_machine *sm)
{
	if (!sm)
		return 0;
	return wpa_key_mgmt_sae(sm->wpa_key_mgmt);
}


int wpa_auth_uses_ft_sae(struct wpa_state_machine *sm)
{
	if (!sm)
		return 0;
	return sm->wpa_key_mgmt == WPA_KEY_MGMT_FT_SAE ||
		sm->wpa_key_mgmt == WPA_KEY_MGMT_FT_SAE_EXT_KEY;
}


#ifdef CONFIG_P2P
int wpa_auth_get_ip_addr(struct wpa_state_machine *sm, u8 *addr)
{
	if (!sm || WPA_GET_BE32(sm->ip_addr) == 0)
		return -1;
	os_memcpy(addr, sm->ip_addr, 4);
	return 0;
}
#endif /* CONFIG_P2P */


int wpa_auth_radius_das_disconnect_pmksa(struct wpa_authenticator *wpa_auth,
					 struct radius_das_attrs *attr)
{
	return pmksa_cache_auth_radius_das_disconnect(wpa_auth->pmksa, attr);
}


void wpa_auth_reconfig_group_keys(struct wpa_authenticator *wpa_auth)
{
	struct wpa_group *group;

	if (!wpa_auth)
		return;
	for (group = wpa_auth->group; group; group = group->next)
		wpa_group_config_group_keys(wpa_auth, group);
}


#ifdef CONFIG_FILS

struct wpa_auth_fils_iter_data {
	struct wpa_authenticator *auth;
	const u8 *cache_id;
	struct rsn_pmksa_cache_entry *pmksa;
	const u8 *spa;
	const u8 *pmkid;
};


static int wpa_auth_fils_iter(struct wpa_authenticator *a, void *ctx)
{
	struct wpa_auth_fils_iter_data *data = ctx;

	if (a == data->auth || !a->conf.fils_cache_id_set ||
	    os_memcmp(a->conf.fils_cache_id, data->cache_id,
		      FILS_CACHE_ID_LEN) != 0)
		return 0;
	data->pmksa = pmksa_cache_auth_get(a->pmksa, data->spa, data->pmkid);
	return data->pmksa != NULL;
}


struct rsn_pmksa_cache_entry *
wpa_auth_pmksa_get_fils_cache_id(struct wpa_authenticator *wpa_auth,
				 const u8 *sta_addr, const u8 *pmkid)
{
	struct wpa_auth_fils_iter_data idata;

	if (!wpa_auth->conf.fils_cache_id_set)
		return NULL;
	idata.auth = wpa_auth;
	idata.cache_id = wpa_auth->conf.fils_cache_id;
	idata.pmksa = NULL;
	idata.spa = sta_addr;
	idata.pmkid = pmkid;
	wpa_auth_for_each_auth(wpa_auth, wpa_auth_fils_iter, &idata);
	return idata.pmksa;
}


#ifdef CONFIG_IEEE80211R_AP
int wpa_auth_write_fte(struct wpa_authenticator *wpa_auth,
		       struct wpa_state_machine *sm,
		       u8 *buf, size_t len)
{
	struct wpa_auth_config *conf = &wpa_auth->conf;

	return wpa_write_ftie(conf, sm->wpa_key_mgmt, sm->xxkey_len,
			      conf->r0_key_holder, conf->r0_key_holder_len,
			      NULL, NULL, buf, len, NULL, 0, 0);
}
#endif /* CONFIG_IEEE80211R_AP */


void wpa_auth_get_fils_aead_params(struct wpa_state_machine *sm,
				   u8 *fils_anonce, u8 *fils_snonce,
				   u8 *fils_kek, size_t *fils_kek_len)
{
	os_memcpy(fils_anonce, sm->ANonce, WPA_NONCE_LEN);
	os_memcpy(fils_snonce, sm->SNonce, WPA_NONCE_LEN);
	os_memcpy(fils_kek, sm->PTK.kek, WPA_KEK_MAX_LEN);
	*fils_kek_len = sm->PTK.kek_len;
}


void wpa_auth_add_fils_pmk_pmkid(struct wpa_state_machine *sm, const u8 *pmk,
				 size_t pmk_len, const u8 *pmkid)
{
	os_memcpy(sm->PMK, pmk, pmk_len);
	sm->pmk_len = pmk_len;
	os_memcpy(sm->pmkid, pmkid, PMKID_LEN);
	sm->pmkid_set = 1;
}

#endif /* CONFIG_FILS */


void wpa_auth_set_auth_alg(struct wpa_state_machine *sm, u16 auth_alg)
{
	if (sm)
		sm->auth_alg = auth_alg;
}


#ifdef CONFIG_DPP2
void wpa_auth_set_dpp_z(struct wpa_state_machine *sm, const struct wpabuf *z)
{
	if (sm) {
		wpabuf_clear_free(sm->dpp_z);
		sm->dpp_z = z ? wpabuf_dup(z) : NULL;
	}
}
#endif /* CONFIG_DPP2 */


void wpa_auth_set_ssid_protection(struct wpa_state_machine *sm, bool val)
{
	if (sm)
		sm->ssid_protection = val;
}


void wpa_auth_set_transition_disable(struct wpa_authenticator *wpa_auth,
				     u8 val)
{
	if (wpa_auth)
		wpa_auth->conf.transition_disable = val;
}


#ifdef CONFIG_TESTING_OPTIONS

int wpa_auth_resend_m1(struct wpa_state_machine *sm, int change_anonce,
		       void (*cb)(void *ctx1, void *ctx2),
		       void *ctx1, void *ctx2)
{
	const u8 *anonce = sm->ANonce;
	u8 anonce_buf[WPA_NONCE_LEN];

	if (change_anonce) {
		if (random_get_bytes(anonce_buf, WPA_NONCE_LEN))
			return -1;
		anonce = anonce_buf;
	}

	wpa_auth_logger(sm->wpa_auth, wpa_auth_get_spa(sm), LOGGER_DEBUG,
			"sending 1/4 msg of 4-Way Handshake (TESTING)");
	wpa_send_eapol(sm->wpa_auth, sm,
		       WPA_KEY_INFO_ACK | WPA_KEY_INFO_KEY_TYPE, NULL,
		       anonce, NULL, 0, 0, 0);
	return 0;
}


int wpa_auth_resend_m3(struct wpa_state_machine *sm,
		       void (*cb)(void *ctx1, void *ctx2),
		       void *ctx1, void *ctx2)
{
	u8 rsc[WPA_KEY_RSC_LEN], *_rsc, *gtk, *kde, *pos;
	u8 *opos;
	size_t gtk_len, kde_len;
	struct wpa_auth_config *conf = &sm->wpa_auth->conf;
	struct wpa_group *gsm = sm->group;
	u8 *wpa_ie;
	int wpa_ie_len, secure, gtkidx, encr = 0;
	u8 hdr[2];

	/* Send EAPOL(1, 1, 1, Pair, P, RSC, ANonce, MIC(PTK), RSNIE, [MDIE],
	   GTK[GN], IGTK, [BIGTK], [FTIE], [TIE * 2])
	 */

	/* Use 0 RSC */
	os_memset(rsc, 0, WPA_KEY_RSC_LEN);
	/* If FT is used, wpa_auth->wpa_ie includes both RSNIE and MDIE */
	wpa_ie = sm->wpa_auth->wpa_ie;
	wpa_ie_len = sm->wpa_auth->wpa_ie_len;
	if (sm->wpa == WPA_VERSION_WPA &&
	    (sm->wpa_auth->conf.wpa & WPA_PROTO_RSN) &&
	    wpa_ie_len > wpa_ie[1] + 2 && wpa_ie[0] == WLAN_EID_RSN) {
		/* WPA-only STA, remove RSN IE and possible MDIE */
		wpa_ie = wpa_ie + wpa_ie[1] + 2;
		if (wpa_ie[0] == WLAN_EID_RSNX)
			wpa_ie = wpa_ie + wpa_ie[1] + 2;
		if (wpa_ie[0] == WLAN_EID_MOBILITY_DOMAIN)
			wpa_ie = wpa_ie + wpa_ie[1] + 2;
		wpa_ie_len = wpa_ie[1] + 2;
	}
	wpa_auth_logger(sm->wpa_auth, wpa_auth_get_spa(sm), LOGGER_DEBUG,
			"sending 3/4 msg of 4-Way Handshake (TESTING)");
	if (sm->wpa == WPA_VERSION_WPA2) {
		/* WPA2 send GTK in the 4-way handshake */
		secure = 1;
		gtk = gsm->GTK[gsm->GN - 1];
		gtk_len = gsm->GTK_len;
		gtkidx = gsm->GN;
		_rsc = rsc;
		encr = 1;
	} else {
		/* WPA does not include GTK in msg 3/4 */
		secure = 0;
		gtk = NULL;
		gtk_len = 0;
		_rsc = NULL;
		if (sm->rx_eapol_key_secure) {
			/*
			 * It looks like Windows 7 supplicant tries to use
			 * Secure bit in msg 2/4 after having reported Michael
			 * MIC failure and it then rejects the 4-way handshake
			 * if msg 3/4 does not set Secure bit. Work around this
			 * by setting the Secure bit here even in the case of
			 * WPA if the supplicant used it first.
			 */
			wpa_auth_logger(sm->wpa_auth, wpa_auth_get_spa(sm),
					LOGGER_DEBUG,
					"STA used Secure bit in WPA msg 2/4 - set Secure for 3/4 as workaround");
			secure = 1;
		}
	}

	kde_len = wpa_ie_len + ieee80211w_kde_len(sm) + ocv_oci_len(sm);

	if (sm->use_ext_key_id)
		kde_len += 2 + RSN_SELECTOR_LEN + 2;

	if (gtk)
		kde_len += 2 + RSN_SELECTOR_LEN + 2 + gtk_len;
#ifdef CONFIG_IEEE80211R_AP
	if (wpa_key_mgmt_ft(sm->wpa_key_mgmt)) {
		kde_len += 2 + PMKID_LEN; /* PMKR1Name into RSN IE */
		kde_len += 300; /* FTIE + 2 * TIE */
	}
#endif /* CONFIG_IEEE80211R_AP */
	kde = os_malloc(kde_len);
	if (!kde)
		return -1;

	pos = kde;
	os_memcpy(pos, wpa_ie, wpa_ie_len);
	pos += wpa_ie_len;
#ifdef CONFIG_IEEE80211R_AP
	if (wpa_key_mgmt_ft(sm->wpa_key_mgmt)) {
		int res;
		size_t elen;

		elen = pos - kde;
		res = wpa_insert_pmkid(kde, &elen, sm->pmk_r1_name, true);
		if (res < 0) {
			wpa_printf(MSG_ERROR,
				   "FT: Failed to insert PMKR1Name into RSN IE in EAPOL-Key data");
			os_free(kde);
			return -1;
		}
		pos -= wpa_ie_len;
		pos += elen;
	}
#endif /* CONFIG_IEEE80211R_AP */
	hdr[1] = 0;

	if (sm->use_ext_key_id) {
		hdr[0] = sm->keyidx_active & 0x01;
		pos = wpa_add_kde(pos, RSN_KEY_DATA_KEYID, hdr, 2, NULL, 0);
	}

	if (gtk) {
		hdr[0] = gtkidx & 0x03;
		pos = wpa_add_kde(pos, RSN_KEY_DATA_GROUPKEY, hdr, 2,
				  gtk, gtk_len);
	}
	opos = pos;
	pos = ieee80211w_kde_add(sm, pos);
	if (pos - opos >= 2 + RSN_SELECTOR_LEN + WPA_IGTK_KDE_PREFIX_LEN) {
		/* skip KDE header and keyid */
		opos += 2 + RSN_SELECTOR_LEN + 2;
		os_memset(opos, 0, 6); /* clear PN */
	}
	if (ocv_oci_add(sm, &pos, conf->oci_freq_override_eapol_m3) < 0) {
		os_free(kde);
		return -1;
	}

#ifdef CONFIG_IEEE80211R_AP
	if (wpa_key_mgmt_ft(sm->wpa_key_mgmt)) {
		int res;

		if (sm->assoc_resp_ftie &&
		    kde + kde_len - pos >= 2 + sm->assoc_resp_ftie[1]) {
			os_memcpy(pos, sm->assoc_resp_ftie,
				  2 + sm->assoc_resp_ftie[1]);
			res = 2 + sm->assoc_resp_ftie[1];
		} else {
			res = wpa_write_ftie(conf, sm->wpa_key_mgmt,
					     sm->xxkey_len,
					     conf->r0_key_holder,
					     conf->r0_key_holder_len,
					     NULL, NULL, pos,
					     kde + kde_len - pos,
					     NULL, 0, 0);
		}
		if (res < 0) {
			wpa_printf(MSG_ERROR,
				   "FT: Failed to insert FTIE into EAPOL-Key Key Data");
			os_free(kde);
			return -1;
		}
		pos += res;

		/* TIE[ReassociationDeadline] (TU) */
		*pos++ = WLAN_EID_TIMEOUT_INTERVAL;
		*pos++ = 5;
		*pos++ = WLAN_TIMEOUT_REASSOC_DEADLINE;
		WPA_PUT_LE32(pos, conf->reassociation_deadline);
		pos += 4;

		/* TIE[KeyLifetime] (seconds) */
		*pos++ = WLAN_EID_TIMEOUT_INTERVAL;
		*pos++ = 5;
		*pos++ = WLAN_TIMEOUT_KEY_LIFETIME;
		WPA_PUT_LE32(pos, conf->r0_key_lifetime);
		pos += 4;
	}
#endif /* CONFIG_IEEE80211R_AP */

	wpa_send_eapol(sm->wpa_auth, sm,
		       (secure ? WPA_KEY_INFO_SECURE : 0) |
		       (wpa_mic_len(sm->wpa_key_mgmt, sm->pmk_len) ?
			WPA_KEY_INFO_MIC : 0) |
		       WPA_KEY_INFO_ACK | WPA_KEY_INFO_INSTALL |
		       WPA_KEY_INFO_KEY_TYPE,
		       _rsc, sm->ANonce, kde, pos - kde, 0, encr);
	bin_clear_free(kde, kde_len);
	return 0;
}


int wpa_auth_resend_group_m1(struct wpa_state_machine *sm,
			     void (*cb)(void *ctx1, void *ctx2),
			     void *ctx1, void *ctx2)
{
	u8 rsc[WPA_KEY_RSC_LEN];
	struct wpa_auth_config *conf = &sm->wpa_auth->conf;
	struct wpa_group *gsm = sm->group;
	const u8 *kde;
	u8 *kde_buf = NULL, *pos, hdr[2];
	u8 *opos;
	size_t kde_len;
	u8 *gtk;

	/* Send EAPOL(1, 1, 1, !Pair, G, RSC, GNonce, MIC(PTK), GTK[GN]) */
	os_memset(rsc, 0, WPA_KEY_RSC_LEN);
	/* Use 0 RSC */
	wpa_auth_logger(sm->wpa_auth, wpa_auth_get_spa(sm), LOGGER_DEBUG,
			"sending 1/2 msg of Group Key Handshake (TESTING)");

	gtk = gsm->GTK[gsm->GN - 1];
	if (sm->wpa == WPA_VERSION_WPA2) {
		kde_len = 2 + RSN_SELECTOR_LEN + 2 + gsm->GTK_len +
			ieee80211w_kde_len(sm) + ocv_oci_len(sm);
		kde_buf = os_malloc(kde_len);
		if (!kde_buf)
			return -1;

		kde = pos = kde_buf;
		hdr[0] = gsm->GN & 0x03;
		hdr[1] = 0;
		pos = wpa_add_kde(pos, RSN_KEY_DATA_GROUPKEY, hdr, 2,
				  gtk, gsm->GTK_len);
		opos = pos;
		pos = ieee80211w_kde_add(sm, pos);
		if (pos - opos >=
		    2 + RSN_SELECTOR_LEN + WPA_IGTK_KDE_PREFIX_LEN) {
			/* skip KDE header and keyid */
			opos += 2 + RSN_SELECTOR_LEN + 2;
			os_memset(opos, 0, 6); /* clear PN */
		}
		if (ocv_oci_add(sm, &pos,
				conf->oci_freq_override_eapol_g1) < 0) {
			os_free(kde_buf);
			return -1;
		}
		kde_len = pos - kde;
	} else {
		kde = gtk;
		kde_len = gsm->GTK_len;
	}

	sm->eapol_status_cb = cb;
	sm->eapol_status_cb_ctx1 = ctx1;
	sm->eapol_status_cb_ctx2 = ctx2;

	wpa_send_eapol(sm->wpa_auth, sm,
		       WPA_KEY_INFO_SECURE |
		       (wpa_mic_len(sm->wpa_key_mgmt, sm->pmk_len) ?
			WPA_KEY_INFO_MIC : 0) |
		       WPA_KEY_INFO_ACK |
		       (!sm->Pair ? WPA_KEY_INFO_INSTALL : 0),
		       rsc, NULL, kde, kde_len, gsm->GN, 1);

	bin_clear_free(kde_buf, kde_len);
	return 0;
}


int wpa_auth_rekey_gtk(struct wpa_authenticator *wpa_auth)
{
	if (!wpa_auth)
		return -1;
	eloop_cancel_timeout(wpa_rekey_gtk,
			     wpa_get_primary_auth(wpa_auth), NULL);
	return eloop_register_timeout(0, 0, wpa_rekey_gtk,
				      wpa_get_primary_auth(wpa_auth), NULL);
}


int wpa_auth_rekey_ptk(struct wpa_authenticator *wpa_auth,
		       struct wpa_state_machine *sm)
{
	if (!wpa_auth || !sm)
		return -1;
	wpa_auth_logger(wpa_auth, sm->addr, LOGGER_DEBUG, "rekeying PTK");
	wpa_request_new_ptk(sm);
	wpa_sm_step(sm);
	return 0;
}


void wpa_auth_set_ft_rsnxe_used(struct wpa_authenticator *wpa_auth, int val)
{
	if (wpa_auth)
		wpa_auth->conf.ft_rsnxe_used = val;
}


void wpa_auth_set_ocv_override_freq(struct wpa_authenticator *wpa_auth,
				    enum wpa_auth_ocv_override_frame frame,
				    unsigned int freq)
{
	if (!wpa_auth)
		return;
	switch (frame) {
	case WPA_AUTH_OCV_OVERRIDE_EAPOL_M3:
		wpa_auth->conf.oci_freq_override_eapol_m3 = freq;
		break;
	case WPA_AUTH_OCV_OVERRIDE_EAPOL_G1:
		wpa_auth->conf.oci_freq_override_eapol_g1 = freq;
		break;
	case WPA_AUTH_OCV_OVERRIDE_FT_ASSOC:
		wpa_auth->conf.oci_freq_override_ft_assoc = freq;
		break;
	case WPA_AUTH_OCV_OVERRIDE_FILS_ASSOC:
		wpa_auth->conf.oci_freq_override_fils_assoc = freq;
		break;
	}
}

#endif /* CONFIG_TESTING_OPTIONS */


void wpa_auth_sta_radius_psk_resp(struct wpa_state_machine *sm, bool success)
{
	if (!sm->waiting_radius_psk) {
		wpa_printf(MSG_DEBUG,
			   "Ignore RADIUS PSK response for " MACSTR
			   " that did not wait one",
			   MAC2STR(sm->addr));
		return;
	}

	wpa_printf(MSG_DEBUG, "RADIUS PSK response for " MACSTR " (%s)",
		   MAC2STR(sm->addr), success ? "success" : "fail");
	sm->waiting_radius_psk = 0;

	if (success) {
		/* Try to process the EAPOL-Key msg 2/4 again */
		sm->EAPOLKeyReceived = true;
	} else {
		sm->Disconnect = true;
	}

	eloop_register_timeout(0, 0, wpa_sm_call_step, sm, NULL);
}


void wpa_auth_set_ml_info(struct wpa_state_machine *sm,
			  u8 mld_assoc_link_id, struct mld_info *info)
{
#ifdef CONFIG_IEEE80211BE
	unsigned int link_id;

	if (!info)
		return;

	os_memset(sm->mld_links, 0, sizeof(sm->mld_links));
	sm->n_mld_affiliated_links = 0;

	wpa_auth_logger(sm->wpa_auth, wpa_auth_get_spa(sm), LOGGER_DEBUG,
			"MLD: Initialization");

	os_memcpy(sm->peer_mld_addr, info->common_info.mld_addr, ETH_ALEN);

	sm->mld_assoc_link_id = mld_assoc_link_id;

	for (link_id = 0; link_id < MAX_NUM_MLD_LINKS; link_id++) {
		struct mld_link_info *link = &info->links[link_id];
		struct mld_link *sm_link = &sm->mld_links[link_id];
		struct wpa_get_link_auth_ctx ctx;

		sm_link->valid = link->valid;
		if (!link->valid)
			continue;

		os_memcpy(sm_link->peer_addr, link->peer_addr, ETH_ALEN);

		wpa_printf(MSG_DEBUG,
			   "WPA_AUTH: MLD: id=%u, peer=" MACSTR,
			   link_id,
			   MAC2STR(sm_link->peer_addr));

		if (link_id != mld_assoc_link_id) {
			sm->n_mld_affiliated_links++;
			ctx.addr = link->local_addr;
			ctx.mld_addr = NULL;
			ctx.link_id = -1;
			ctx.wpa_auth = NULL;
			wpa_auth_for_each_auth(sm->wpa_auth,
					       wpa_get_link_sta_auth, &ctx);
			if (ctx.wpa_auth) {
				sm_link->wpa_auth = ctx.wpa_auth;
				wpa_group_get(sm_link->wpa_auth,
					      sm_link->wpa_auth->group);
			}
		} else {
			sm_link->wpa_auth = sm->wpa_auth;
		}

		if (!sm_link->wpa_auth)
			wpa_printf(MSG_ERROR,
				   "Unable to find authenticator object for ML STA "
				   MACSTR " on link id %d",
				   MAC2STR(sm->wpa_auth->mld_addr),
				   link_id);
	}
#endif /* CONFIG_IEEE80211BE */
}
