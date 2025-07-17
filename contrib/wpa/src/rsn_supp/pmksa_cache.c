/*
 * WPA Supplicant - RSN PMKSA cache
 * Copyright (c) 2004-2009, 2011-2015, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "eloop.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "wpa.h"
#include "wpa_i.h"
#include "pmksa_cache.h"

#if defined(IEEE8021X_EAPOL) && !defined(CONFIG_NO_WPA)

static const int pmksa_cache_max_entries = 32;

struct rsn_pmksa_cache {
	struct rsn_pmksa_cache_entry *pmksa; /* PMKSA cache */
	int pmksa_count; /* number of entries in PMKSA cache */
	struct wpa_sm *sm; /* TODO: get rid of this reference(?) */

	void (*free_cb)(struct rsn_pmksa_cache_entry *entry, void *ctx,
			enum pmksa_free_reason reason);
	bool (*is_current_cb)(struct rsn_pmksa_cache_entry *entry,
			      void *ctx);
	void (*notify_cb)(struct rsn_pmksa_cache_entry *entry, void *ctx);
	void *ctx;
};


static void pmksa_cache_set_expiration(struct rsn_pmksa_cache *pmksa);


static void _pmksa_cache_free_entry(struct rsn_pmksa_cache_entry *entry)
{
	bin_clear_free(entry, sizeof(*entry));
}


static void pmksa_cache_free_entry(struct rsn_pmksa_cache *pmksa,
				   struct rsn_pmksa_cache_entry *entry,
				   enum pmksa_free_reason reason)
{
	if (pmksa->sm)
		wpa_sm_remove_pmkid(pmksa->sm, entry->network_ctx, entry->aa,
				    entry->pmkid,
				    entry->fils_cache_id_set ?
				    entry->fils_cache_id : NULL);
	pmksa->pmksa_count--;
	if (pmksa->free_cb)
		pmksa->free_cb(entry, pmksa->ctx, reason);
	_pmksa_cache_free_entry(entry);
}


void pmksa_cache_remove(struct rsn_pmksa_cache *pmksa,
			struct rsn_pmksa_cache_entry *entry)
{
	struct rsn_pmksa_cache_entry *e;

	e = pmksa->pmksa;
	while (e) {
		if (e == entry) {
			pmksa->pmksa = entry->next;
			break;
		}
		if (e->next == entry) {
			e->next = entry->next;
			break;
		}
	}

	if (!e) {
		wpa_printf(MSG_DEBUG,
			   "RSN: Could not remove PMKSA cache entry %p since it is not in the list",
			   entry);
		return;
	}

	pmksa_cache_free_entry(pmksa, entry, PMKSA_FREE);
}


static void pmksa_cache_expire(void *eloop_ctx, void *timeout_ctx)
{
	struct rsn_pmksa_cache *pmksa = eloop_ctx;
	struct os_reltime now;
	struct rsn_pmksa_cache_entry *prev = NULL, *tmp;
	struct rsn_pmksa_cache_entry *entry = pmksa->pmksa;

	os_get_reltime(&now);
	while (entry && entry->expiration <= now.sec) {
		if (wpa_key_mgmt_sae(entry->akmp) && pmksa->is_current_cb &&
		    pmksa->is_current_cb(entry, pmksa->ctx)) {
			/* Do not expire the currently used PMKSA entry for SAE
			 * since there is no convenient mechanism for
			 * reauthenticating during an association with SAE. The
			 * expired entry will be removed after this association
			 * has been lost. */
			wpa_printf(MSG_DEBUG,
				   "RSN: postpone PMKSA cache entry expiration for SAE with "
				   MACSTR, MAC2STR(entry->aa));
			prev = entry;
			entry = entry->next;
			continue;
		}

		wpa_printf(MSG_DEBUG, "RSN: expired PMKSA cache entry for "
			   MACSTR, MAC2STR(entry->aa));
		if (prev)
			prev->next = entry->next;
		else
			pmksa->pmksa = entry->next;
		tmp = entry;
		entry = entry->next;
		pmksa_cache_free_entry(pmksa, tmp, PMKSA_EXPIRE);
	}

	pmksa_cache_set_expiration(pmksa);
}


static void pmksa_cache_reauth(void *eloop_ctx, void *timeout_ctx)
{
	struct rsn_pmksa_cache *pmksa = eloop_ctx;

	if (!pmksa->sm)
		return;

	if (pmksa->sm->driver_bss_selection) {
		struct rsn_pmksa_cache_entry *entry;

		entry = pmksa->sm->cur_pmksa ?
			pmksa->sm->cur_pmksa :
			pmksa_cache_get(pmksa, pmksa->sm->bssid, NULL, NULL,
					NULL, 0);
		if (entry && wpa_key_mgmt_sae(entry->akmp)) {
			wpa_printf(MSG_DEBUG,
				   "RSN: remove reauth threshold passed PMKSA from the driver for SAE");
			entry->sae_reauth_scheduled = true;
			wpa_sm_remove_pmkid(pmksa->sm, entry->network_ctx,
					    entry->aa, entry->pmkid, NULL);
			return;
		}
	}

	pmksa->sm->cur_pmksa = NULL;
	eapol_sm_request_reauth(pmksa->sm->eapol);
}


static void pmksa_cache_set_expiration(struct rsn_pmksa_cache *pmksa)
{
	int sec;
	struct rsn_pmksa_cache_entry *entry;
	struct os_reltime now;

	eloop_cancel_timeout(pmksa_cache_expire, pmksa, NULL);
	eloop_cancel_timeout(pmksa_cache_reauth, pmksa, NULL);
	if (pmksa->pmksa == NULL)
		return;
	os_get_reltime(&now);
	sec = pmksa->pmksa->expiration - now.sec;
	if (sec < 0) {
		sec = 0;
		if (wpa_key_mgmt_sae(pmksa->pmksa->akmp) &&
		    pmksa->is_current_cb &&
		    pmksa->is_current_cb(pmksa->pmksa, pmksa->ctx)) {
			/* Do not continue polling for the current PMKSA entry
			 * from SAE to expire every second. Use the expiration
			 * time to the following entry, if any, and wait at
			 * maximum 10 minutes to check again.
			 */
			entry = pmksa->pmksa->next;
			if (entry) {
				sec = entry->expiration - now.sec;
				if (sec < 0)
					sec = 0;
				else if (sec > 600)
					sec = 600;
			} else {
				sec = 600;
			}
		}
	}
	eloop_register_timeout(sec + 1, 0, pmksa_cache_expire, pmksa, NULL);

	if (!pmksa->sm)
		return;

	entry = pmksa->sm->cur_pmksa ? pmksa->sm->cur_pmksa :
		pmksa_cache_get(pmksa, pmksa->sm->bssid, NULL, NULL, NULL, 0);
	if (entry &&
	    (!wpa_key_mgmt_sae(entry->akmp) ||
	     (pmksa->sm->driver_bss_selection &&
	      !entry->sae_reauth_scheduled))) {
		sec = pmksa->pmksa->reauth_time - now.sec;
		if (sec < 0)
			sec = 0;
		eloop_register_timeout(sec, 0, pmksa_cache_reauth, pmksa,
				       NULL);
	}
}


/**
 * pmksa_cache_add - Add a PMKSA cache entry
 * @pmksa: Pointer to PMKSA cache data from pmksa_cache_init()
 * @pmk: The new pairwise master key
 * @pmk_len: PMK length in bytes, usually PMK_LEN (32)
 * @pmkid: Calculated PMKID
 * @kck: Key confirmation key or %NULL if not yet derived
 * @kck_len: KCK length in bytes
 * @aa: Authenticator address
 * @spa: Supplicant address
 * @network_ctx: Network configuration context for this PMK
 * @akmp: WPA_KEY_MGMT_* used in key derivation
 * @cache_id: Pointer to FILS Cache Identifier or %NULL if not advertised
 * Returns: Pointer to the added PMKSA cache entry or %NULL on error
 *
 * This function create a PMKSA entry for a new PMK and adds it to the PMKSA
 * cache. If an old entry is already in the cache for the same Authenticator,
 * this entry will be replaced with the new entry. PMKID will be calculated
 * based on the PMK and the driver interface is notified of the new PMKID.
 */
struct rsn_pmksa_cache_entry *
pmksa_cache_add(struct rsn_pmksa_cache *pmksa, const u8 *pmk, size_t pmk_len,
		const u8 *pmkid, const u8 *kck, size_t kck_len,
		const u8 *aa, const u8 *spa, void *network_ctx, int akmp,
		const u8 *cache_id)
{
	struct rsn_pmksa_cache_entry *entry;
	struct os_reltime now;
	unsigned int pmk_lifetime = 43200;
	unsigned int pmk_reauth_threshold = 70;

	if (pmk_len > PMK_LEN_MAX)
		return NULL;

	if (kck_len > WPA_KCK_MAX_LEN)
		return NULL;

	if (wpa_key_mgmt_suite_b(akmp) && !kck)
		return NULL;

	entry = os_zalloc(sizeof(*entry));
	if (entry == NULL)
		return NULL;
	os_memcpy(entry->pmk, pmk, pmk_len);
	entry->pmk_len = pmk_len;
	if (kck_len > 0)
		os_memcpy(entry->kck, kck, kck_len);
	entry->kck_len = kck_len;
	if (pmkid)
		os_memcpy(entry->pmkid, pmkid, PMKID_LEN);
	else if (akmp == WPA_KEY_MGMT_IEEE8021X_SUITE_B_192)
		rsn_pmkid_suite_b_192(kck, kck_len, aa, spa, entry->pmkid);
	else if (wpa_key_mgmt_suite_b(akmp))
		rsn_pmkid_suite_b(kck, kck_len, aa, spa, entry->pmkid);
	else
		rsn_pmkid(pmk, pmk_len, aa, spa, entry->pmkid, akmp);
	os_get_reltime(&now);
	if (pmksa->sm) {
		pmk_lifetime = pmksa->sm->dot11RSNAConfigPMKLifetime;
		pmk_reauth_threshold =
			pmksa->sm->dot11RSNAConfigPMKReauthThreshold;
	}
	entry->expiration = now.sec + pmk_lifetime;
	entry->reauth_time = now.sec +
		pmk_lifetime * pmk_reauth_threshold / 100;
	entry->akmp = akmp;
	if (cache_id) {
		entry->fils_cache_id_set = 1;
		os_memcpy(entry->fils_cache_id, cache_id, FILS_CACHE_ID_LEN);
	}
	os_memcpy(entry->aa, aa, ETH_ALEN);
	os_memcpy(entry->spa, spa, ETH_ALEN);
	entry->network_ctx = network_ctx;

	return pmksa_cache_add_entry(pmksa, entry);
}


struct rsn_pmksa_cache_entry *
pmksa_cache_add_entry(struct rsn_pmksa_cache *pmksa,
		      struct rsn_pmksa_cache_entry *entry)
{
	struct rsn_pmksa_cache_entry *pos, *prev;

	/* Replace an old entry for the same Authenticator (if found) with the
	 * new entry */
	pos = pmksa->pmksa;
	prev = NULL;
	while (pos) {
		if (ether_addr_equal(entry->aa, pos->aa) &&
		    ether_addr_equal(entry->spa, pos->spa)) {
			if (pos->pmk_len == entry->pmk_len &&
			    os_memcmp_const(pos->pmk, entry->pmk,
					    entry->pmk_len) == 0 &&
			    os_memcmp_const(pos->pmkid, entry->pmkid,
					    PMKID_LEN) == 0) {
				wpa_printf(MSG_DEBUG, "WPA: reusing previous "
					   "PMKSA entry");
				os_free(entry);
				return pos;
			}
			if (prev == NULL)
				pmksa->pmksa = pos->next;
			else
				prev->next = pos->next;

			/*
			 * If OKC is used, there may be other PMKSA cache
			 * entries based on the same PMK. These needs to be
			 * flushed so that a new entry can be created based on
			 * the new PMK. Only clear other entries if they have a
			 * matching PMK and this PMK has been used successfully
			 * with the current AP, i.e., if opportunistic flag has
			 * been cleared in wpa_supplicant_key_neg_complete().
			 */
			wpa_printf(MSG_DEBUG, "RSN: Replace PMKSA entry for "
				   "the current AP and any PMKSA cache entry "
				   "that was based on the old PMK");
			if (!pos->opportunistic)
				pmksa_cache_flush(pmksa, entry->network_ctx,
						  pos->pmk, pos->pmk_len,
						  false);
			pmksa_cache_free_entry(pmksa, pos, PMKSA_REPLACE);
			break;
		}
		prev = pos;
		pos = pos->next;
	}

	if (pmksa->pmksa_count >= pmksa_cache_max_entries && pmksa->pmksa) {
		/* Remove the oldest entry to make room for the new entry */
		pos = pmksa->pmksa;

		if (pmksa->sm && pos == pmksa->sm->cur_pmksa) {
			/*
			 * Never remove the current PMKSA cache entry, since
			 * it's in use, and removing it triggers a needless
			 * deauthentication.
			 */
			pos = pos->next;
			pmksa->pmksa->next = pos ? pos->next : NULL;
		} else
			pmksa->pmksa = pos->next;

		if (pos) {
			wpa_printf(MSG_DEBUG, "RSN: removed the oldest idle "
				   "PMKSA cache entry (for " MACSTR ") to "
				   "make room for new one",
				   MAC2STR(pos->aa));
			pmksa_cache_free_entry(pmksa, pos, PMKSA_FREE);
		}
	}

	/* Add the new entry; order by expiration time */
	pos = pmksa->pmksa;
	prev = NULL;
	while (pos) {
		if (pos->expiration > entry->expiration)
			break;
		prev = pos;
		pos = pos->next;
	}
	if (prev == NULL) {
		entry->next = pmksa->pmksa;
		pmksa->pmksa = entry;
		pmksa_cache_set_expiration(pmksa);
	} else {
		entry->next = prev->next;
		prev->next = entry;
	}
	pmksa->pmksa_count++;
	wpa_printf(MSG_DEBUG, "RSN: Added PMKSA cache entry for " MACSTR
		   " spa=" MACSTR " network_ctx=%p akmp=0x%x",
		   MAC2STR(entry->aa), MAC2STR(entry->spa),
		   entry->network_ctx, entry->akmp);

	if (!pmksa->sm)
		return entry;

	if (pmksa->notify_cb)
		pmksa->notify_cb(entry, pmksa->ctx);

	wpa_sm_add_pmkid(pmksa->sm, entry->network_ctx, entry->aa, entry->pmkid,
			 entry->fils_cache_id_set ? entry->fils_cache_id : NULL,
			 entry->pmk, entry->pmk_len,
			 pmksa->sm->dot11RSNAConfigPMKLifetime,
			 pmksa->sm->dot11RSNAConfigPMKReauthThreshold,
			 entry->akmp);

	return entry;
}


/**
 * pmksa_cache_flush - Flush PMKSA cache entries for a specific network
 * @pmksa: Pointer to PMKSA cache data from pmksa_cache_init()
 * @network_ctx: Network configuration context or %NULL to flush all entries
 * @pmk: PMK to match for or %NULL to match all PMKs
 * @pmk_len: PMK length
 * @external_only: Flush only PMKSA cache entries configured by external
 * applications
 */
void pmksa_cache_flush(struct rsn_pmksa_cache *pmksa, void *network_ctx,
		       const u8 *pmk, size_t pmk_len, bool external_only)
{
	struct rsn_pmksa_cache_entry *entry, *prev = NULL, *tmp;
	int removed = 0;

	entry = pmksa->pmksa;
	while (entry) {
		if ((entry->network_ctx == network_ctx ||
		     network_ctx == NULL) &&
		    (pmk == NULL ||
		     (pmk_len == entry->pmk_len &&
		      os_memcmp(pmk, entry->pmk, pmk_len) == 0)) &&
		    (!external_only || entry->external)) {
			wpa_printf(MSG_DEBUG, "RSN: Flush PMKSA cache entry "
				   "for " MACSTR, MAC2STR(entry->aa));
			if (prev)
				prev->next = entry->next;
			else
				pmksa->pmksa = entry->next;
			tmp = entry;
			entry = entry->next;
			pmksa_cache_free_entry(pmksa, tmp, PMKSA_FREE);
			removed++;
		} else {
			prev = entry;
			entry = entry->next;
		}
	}
	if (removed)
		pmksa_cache_set_expiration(pmksa);
}


/**
 * pmksa_cache_deinit - Free all entries in PMKSA cache
 * @pmksa: Pointer to PMKSA cache data from pmksa_cache_init()
 */
void pmksa_cache_deinit(struct rsn_pmksa_cache *pmksa)
{
	struct rsn_pmksa_cache_entry *entry, *prev;

	if (pmksa == NULL)
		return;

	entry = pmksa->pmksa;
	pmksa->pmksa = NULL;
	while (entry) {
		prev = entry;
		entry = entry->next;
		os_free(prev);
	}
	pmksa_cache_set_expiration(pmksa);
	os_free(pmksa);
}


/**
 * pmksa_cache_get - Fetch a PMKSA cache entry
 * @pmksa: Pointer to PMKSA cache data from pmksa_cache_init()
 * @aa: Authenticator address or %NULL to match any
 * @pmkid: PMKID or %NULL to match any
 * @network_ctx: Network context or %NULL to match any
 * @akmp: Specific AKMP to search for or 0 for any
 * Returns: Pointer to PMKSA cache entry or %NULL if no match was found
 */
struct rsn_pmksa_cache_entry * pmksa_cache_get(struct rsn_pmksa_cache *pmksa,
					       const u8 *aa, const u8 *spa,
					       const u8 *pmkid,
					       const void *network_ctx,
					       int akmp)
{
	struct rsn_pmksa_cache_entry *entry = pmksa->pmksa;
	while (entry) {
		if ((aa == NULL || ether_addr_equal(entry->aa, aa)) &&
		    (!spa || ether_addr_equal(entry->spa, spa)) &&
		    (pmkid == NULL ||
		     os_memcmp(entry->pmkid, pmkid, PMKID_LEN) == 0) &&
		    (!akmp || akmp == entry->akmp) &&
		    (network_ctx == NULL || network_ctx == entry->network_ctx))
			return entry;
		entry = entry->next;
	}
	return NULL;
}


static struct rsn_pmksa_cache_entry *
pmksa_cache_clone_entry(struct rsn_pmksa_cache *pmksa,
			const struct rsn_pmksa_cache_entry *old_entry,
			const u8 *aa)
{
	struct rsn_pmksa_cache_entry *new_entry;
	os_time_t old_expiration = old_entry->expiration;
	os_time_t old_reauth_time = old_entry->reauth_time;
	const u8 *pmkid = NULL;

	if (!pmksa->sm)
		return NULL;

	if (wpa_key_mgmt_sae(old_entry->akmp) ||
	    wpa_key_mgmt_fils(old_entry->akmp))
		pmkid = old_entry->pmkid;
	new_entry = pmksa_cache_add(pmksa, old_entry->pmk, old_entry->pmk_len,
				    pmkid, old_entry->kck, old_entry->kck_len,
				    aa, pmksa->sm->own_addr,
				    old_entry->network_ctx, old_entry->akmp,
				    old_entry->fils_cache_id_set ?
				    old_entry->fils_cache_id : NULL);
	if (new_entry == NULL)
		return NULL;

	/* TODO: reorder entries based on expiration time? */
	new_entry->expiration = old_expiration;
	new_entry->reauth_time = old_reauth_time;
	new_entry->opportunistic = 1;

	return new_entry;
}


/**
 * pmksa_cache_get_opportunistic - Try to get an opportunistic PMKSA entry
 * @pmksa: Pointer to PMKSA cache data from pmksa_cache_init()
 * @network_ctx: Network configuration context
 * @aa: Authenticator address for the new AP
 * @akmp: Specific AKMP to search for or 0 for any
 * Returns: Pointer to a new PMKSA cache entry or %NULL if not available
 *
 * Try to create a new PMKSA cache entry opportunistically by guessing that the
 * new AP is sharing the same PMK as another AP that has the same SSID and has
 * already an entry in PMKSA cache.
 */
struct rsn_pmksa_cache_entry *
pmksa_cache_get_opportunistic(struct rsn_pmksa_cache *pmksa, void *network_ctx,
			      const u8 *aa, int akmp)
{
	struct rsn_pmksa_cache_entry *entry = pmksa->pmksa;

	wpa_printf(MSG_DEBUG, "RSN: Consider " MACSTR " for OKC", MAC2STR(aa));
	if (network_ctx == NULL)
		return NULL;
	while (entry) {
		if (entry->network_ctx == network_ctx &&
		    (!akmp || entry->akmp == akmp)) {
			struct os_reltime now;

			if (wpa_key_mgmt_sae(entry->akmp) &&
			    os_get_reltime(&now) == 0 &&
			    entry->reauth_time < now.sec) {
				wpa_printf(MSG_DEBUG,
					   "RSN: Do not clone PMKSA cache entry for "
					   MACSTR
					   " since its reauth threshold has passed",
					   MAC2STR(entry->aa));
				entry = entry->next;
				continue;
			}

			entry = pmksa_cache_clone_entry(pmksa, entry, aa);
			if (entry) {
				wpa_printf(MSG_DEBUG, "RSN: added "
					   "opportunistic PMKSA cache entry "
					   "for " MACSTR, MAC2STR(aa));
			}
			return entry;
		}
		entry = entry->next;
	}
	return NULL;
}


static struct rsn_pmksa_cache_entry *
pmksa_cache_get_fils_cache_id(struct rsn_pmksa_cache *pmksa,
			      const void *network_ctx, const u8 *cache_id)
{
	struct rsn_pmksa_cache_entry *entry;

	for (entry = pmksa->pmksa; entry; entry = entry->next) {
		if (network_ctx == entry->network_ctx &&
		    entry->fils_cache_id_set &&
		    os_memcmp(cache_id, entry->fils_cache_id,
			      FILS_CACHE_ID_LEN) == 0)
			return entry;
	}

	return NULL;
}


/**
 * pmksa_cache_get_current - Get the current used PMKSA entry
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * Returns: Pointer to the current PMKSA cache entry or %NULL if not available
 */
struct rsn_pmksa_cache_entry * pmksa_cache_get_current(struct wpa_sm *sm)
{
	if (sm == NULL)
		return NULL;
	return sm->cur_pmksa;
}


/**
 * pmksa_cache_clear_current - Clear the current PMKSA entry selection
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 */
void pmksa_cache_clear_current(struct wpa_sm *sm)
{
	if (sm == NULL)
		return;
	if (sm->cur_pmksa)
		wpa_printf(MSG_DEBUG,
			   "RSN: Clear current PMKSA entry selection");
	sm->cur_pmksa = NULL;
}


/**
 * pmksa_cache_set_current - Set the current PMKSA entry selection
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @pmkid: PMKID for selecting PMKSA or %NULL if not used
 * @bssid: BSSID for PMKSA or %NULL if not used
 * @network_ctx: Network configuration context
 * @try_opportunistic: Whether to allow opportunistic PMKSA caching
 * @fils_cache_id: Pointer to FILS Cache Identifier or %NULL if not used
 * @associated: Whether the device is associated
 * Returns: 0 if PMKSA was found or -1 if no matching entry was found
 */
int pmksa_cache_set_current(struct wpa_sm *sm, const u8 *pmkid,
			    const u8 *bssid, void *network_ctx,
			    int try_opportunistic, const u8 *fils_cache_id,
			    int akmp, bool associated)
{
	struct rsn_pmksa_cache *pmksa = sm->pmksa;
	wpa_printf(MSG_DEBUG, "RSN: PMKSA cache search - network_ctx=%p "
		   "try_opportunistic=%d akmp=0x%x",
		   network_ctx, try_opportunistic, akmp);
	if (pmkid)
		wpa_hexdump(MSG_DEBUG, "RSN: Search for PMKID",
			    pmkid, PMKID_LEN);
	if (bssid)
		wpa_printf(MSG_DEBUG, "RSN: Search for BSSID " MACSTR,
			   MAC2STR(bssid));
	if (fils_cache_id)
		wpa_printf(MSG_DEBUG,
			   "RSN: Search for FILS Cache Identifier %02x%02x",
			   fils_cache_id[0], fils_cache_id[1]);

	sm->cur_pmksa = NULL;
	if (pmkid)
		sm->cur_pmksa = pmksa_cache_get(pmksa, NULL, sm->own_addr,
						pmkid, network_ctx, akmp);
	if (sm->cur_pmksa == NULL && bssid)
		sm->cur_pmksa = pmksa_cache_get(pmksa, bssid, sm->own_addr,
						NULL, network_ctx, akmp);
	if (sm->cur_pmksa == NULL && try_opportunistic && bssid)
		sm->cur_pmksa = pmksa_cache_get_opportunistic(pmksa,
							      network_ctx,
							      bssid, akmp);
	if (sm->cur_pmksa == NULL && fils_cache_id)
		sm->cur_pmksa = pmksa_cache_get_fils_cache_id(pmksa,
							      network_ctx,
							      fils_cache_id);
	if (sm->cur_pmksa) {
		struct os_reltime now;

		if (wpa_key_mgmt_sae(sm->cur_pmksa->akmp) &&
		    os_get_reltime(&now) == 0 &&
		    sm->cur_pmksa->reauth_time < now.sec) {
			/* Driver-based roaming might have used a PMKSA entry
			 * that is already past the reauthentication threshold.
			 * Remove the related PMKID from the driver to avoid
			 * further uses for this PMKSA, but allow the
			 * association to continue since the PMKSA has not yet
			 * expired. */
			wpa_sm_remove_pmkid(sm, sm->cur_pmksa->network_ctx,
					    sm->cur_pmksa->aa,
					    sm->cur_pmksa->pmkid, NULL);
			if (associated) {
				wpa_printf(MSG_DEBUG,
					   "RSN: Associated with " MACSTR
					   " using reauth threshold passed PMKSA cache entry",
					   MAC2STR(sm->cur_pmksa->aa));
			} else {
				wpa_printf(MSG_DEBUG,
					   "RSN: Do not allow PMKSA cache entry for "
					   MACSTR
					   " to be used for SAE since its reauth threshold has passed",
					   MAC2STR(sm->cur_pmksa->aa));
				sm->cur_pmksa = NULL;
				return -1;
			}
		}

		wpa_hexdump(MSG_DEBUG, "RSN: PMKSA cache entry found - PMKID",
			    sm->cur_pmksa->pmkid, PMKID_LEN);
		return 0;
	}
	wpa_printf(MSG_DEBUG, "RSN: No PMKSA cache entry found");
	return -1;
}


/**
 * pmksa_cache_list - Dump text list of entries in PMKSA cache
 * @pmksa: Pointer to PMKSA cache data from pmksa_cache_init()
 * @buf: Buffer for the list
 * @len: Length of the buffer
 * Returns: number of bytes written to buffer
 *
 * This function is used to generate a text format representation of the
 * current PMKSA cache contents for the ctrl_iface PMKSA command.
 */
int pmksa_cache_list(struct rsn_pmksa_cache *pmksa, char *buf, size_t len)
{
	int i, ret;
	char *pos = buf;
	struct rsn_pmksa_cache_entry *entry;
	struct os_reltime now;
	int cache_id_used = 0;

	for (entry = pmksa->pmksa; entry; entry = entry->next) {
		if (entry->fils_cache_id_set) {
			cache_id_used = 1;
			break;
		}
	}

	os_get_reltime(&now);
	ret = os_snprintf(pos, buf + len - pos,
			  "Index / AA / PMKID / expiration (in seconds) / "
			  "opportunistic%s\n",
			  cache_id_used ? " / FILS Cache Identifier" : "");
	if (os_snprintf_error(buf + len - pos, ret))
		return pos - buf;
	pos += ret;
	i = 0;
	entry = pmksa->pmksa;
	while (entry) {
		i++;
		ret = os_snprintf(pos, buf + len - pos, "%d " MACSTR " ",
				  i, MAC2STR(entry->aa));
		if (os_snprintf_error(buf + len - pos, ret))
			return pos - buf;
		pos += ret;
		pos += wpa_snprintf_hex(pos, buf + len - pos, entry->pmkid,
					PMKID_LEN);
		ret = os_snprintf(pos, buf + len - pos, " %d %d",
				  (int) (entry->expiration - now.sec),
				  entry->opportunistic);
		if (os_snprintf_error(buf + len - pos, ret))
			return pos - buf;
		pos += ret;
		if (entry->fils_cache_id_set) {
			ret = os_snprintf(pos, buf + len - pos, " %02x%02x",
					  entry->fils_cache_id[0],
					  entry->fils_cache_id[1]);
			if (os_snprintf_error(buf + len - pos, ret))
				return pos - buf;
			pos += ret;
		}
		ret = os_snprintf(pos, buf + len - pos, "\n");
		if (os_snprintf_error(buf + len - pos, ret))
			return pos - buf;
		pos += ret;
		entry = entry->next;
	}
	return pos - buf;
}


struct rsn_pmksa_cache_entry * pmksa_cache_head(struct rsn_pmksa_cache *pmksa)
{
	return pmksa->pmksa;
}


/**
 * pmksa_cache_init - Initialize PMKSA cache
 * @free_cb: Callback function to be called when a PMKSA cache entry is freed
 * @ctx: Context pointer for free_cb function
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * Returns: Pointer to PMKSA cache data or %NULL on failure
 */
struct rsn_pmksa_cache *
pmksa_cache_init(void (*free_cb)(struct rsn_pmksa_cache_entry *entry,
				 void *ctx, enum pmksa_free_reason reason),
		 bool (*is_current_cb)(struct rsn_pmksa_cache_entry *entry,
				       void *ctx),
		 void (*notify_cb)(struct rsn_pmksa_cache_entry *entry,
				   void *ctx),
		 void *ctx, struct wpa_sm *sm)
{
	struct rsn_pmksa_cache *pmksa;

	pmksa = os_zalloc(sizeof(*pmksa));
	if (pmksa) {
		pmksa->free_cb = free_cb;
		pmksa->is_current_cb = is_current_cb;
		pmksa->notify_cb = notify_cb;
		pmksa->ctx = ctx;
		pmksa->sm = sm;
	}

	return pmksa;
}


void pmksa_cache_reconfig(struct rsn_pmksa_cache *pmksa)
{
	struct rsn_pmksa_cache_entry *entry;
	struct os_reltime now;

	if (!pmksa || !pmksa->pmksa)
		return;

	os_get_reltime(&now);
	for (entry = pmksa->pmksa; entry; entry = entry->next) {
		u32 life_time;
		u8 reauth_threshold;

		if (entry->expiration - now.sec < 1 ||
		    entry->reauth_time - now.sec < 1)
			continue;

		life_time = entry->expiration - now.sec;
		reauth_threshold = (entry->reauth_time - now.sec) * 100 /
			life_time;
		if (!reauth_threshold)
			continue;

		wpa_sm_add_pmkid(pmksa->sm, entry->network_ctx, entry->aa,
				 entry->pmkid,
				 entry->fils_cache_id_set ?
				 entry->fils_cache_id : NULL,
				 entry->pmk, entry->pmk_len, life_time,
				 reauth_threshold, entry->akmp);
	}
}

#else /* IEEE8021X_EAPOL */

struct rsn_pmksa_cache *
pmksa_cache_init(void (*free_cb)(struct rsn_pmksa_cache_entry *entry,
				 void *ctx, enum pmksa_free_reason reason),
		 bool (*is_current_cb)(struct rsn_pmksa_cache_entry *entry,
				       void *ctx),
		 void (*notify_cb)(struct rsn_pmksa_cache_entry *entry,
				   void *ctx),
		 void *ctx, struct wpa_sm *sm)
{
	return (void *) -1;
}


void pmksa_cache_deinit(struct rsn_pmksa_cache *pmksa)
{
}


struct rsn_pmksa_cache_entry *
pmksa_cache_get(struct rsn_pmksa_cache *pmksa, const u8 *aa, const u8 *spa,
		const u8 *pmkid, const void *network_ctx, int akmp)
{
	return NULL;
}


struct rsn_pmksa_cache_entry *
pmksa_cache_get_current(struct wpa_sm *sm)
{
	return NULL;
}


int pmksa_cache_list(struct rsn_pmksa_cache *pmksa, char *buf, size_t len)
{
	return -1;
}


struct rsn_pmksa_cache_entry *
pmksa_cache_head(struct rsn_pmksa_cache *pmksa)
{
	return NULL;
}


struct rsn_pmksa_cache_entry *
pmksa_cache_add_entry(struct rsn_pmksa_cache *pmksa,
		      struct rsn_pmksa_cache_entry *entry)
{
	return NULL;
}


struct rsn_pmksa_cache_entry *
pmksa_cache_add(struct rsn_pmksa_cache *pmksa, const u8 *pmk, size_t pmk_len,
		const u8 *pmkid, const u8 *kck, size_t kck_len,
		const u8 *aa, const u8 *spa, void *network_ctx, int akmp,
		const u8 *cache_id)
{
	return NULL;
}


void pmksa_cache_clear_current(struct wpa_sm *sm)
{
}


int pmksa_cache_set_current(struct wpa_sm *sm, const u8 *pmkid, const u8 *bssid,
			    void *network_ctx, int try_opportunistic,
			    const u8 *fils_cache_id, int akmp, bool associated)
{
	return -1;
}


void pmksa_cache_flush(struct rsn_pmksa_cache *pmksa, void *network_ctx,
		       const u8 *pmk, size_t pmk_len, bool external_only)
{
}


void pmksa_cache_remove(struct rsn_pmksa_cache *pmksa,
			struct rsn_pmksa_cache_entry *entry)
{
}


void pmksa_cache_reconfig(struct rsn_pmksa_cache *pmksa)
{
}

#endif /* IEEE8021X_EAPOL */
