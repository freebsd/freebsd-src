/*
 * RSN PTKSA cache implementation
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include "utils/common.h"
#include "eloop.h"
#include "common/ptksa_cache.h"

#define PTKSA_CACHE_MAX_ENTRIES 16

struct ptksa_cache {
	struct dl_list ptksa;
	unsigned int n_ptksa;
};

static void ptksa_cache_set_expiration(struct ptksa_cache *ptksa);


static void ptksa_cache_free_entry(struct ptksa_cache *ptksa,
				   struct ptksa_cache_entry *entry)
{
	ptksa->n_ptksa--;

	dl_list_del(&entry->list);
	bin_clear_free(entry, sizeof(*entry));
}


static void ptksa_cache_expire(void *eloop_ctx, void *timeout_ctx)
{
	struct ptksa_cache *ptksa = eloop_ctx;
	struct ptksa_cache_entry *e, *next;
	struct os_reltime now;

	if (!ptksa)
		return;

	os_get_reltime(&now);

	dl_list_for_each_safe(e, next, &ptksa->ptksa,
			      struct ptksa_cache_entry, list) {
		if (e->expiration > now.sec)
			continue;

		wpa_printf(MSG_DEBUG, "Expired PTKSA cache entry for " MACSTR,
			   MAC2STR(e->addr));

		ptksa_cache_free_entry(ptksa, e);
	}

	ptksa_cache_set_expiration(ptksa);
}


static void ptksa_cache_set_expiration(struct ptksa_cache *ptksa)
{
	struct ptksa_cache_entry *e;
	int sec;
	struct os_reltime now;

	eloop_cancel_timeout(ptksa_cache_expire, ptksa, NULL);

	if (!ptksa || !ptksa->n_ptksa)
		return;

	e = dl_list_first(&ptksa->ptksa, struct ptksa_cache_entry, list);
	if (!e)
		return;

	os_get_reltime(&now);
	sec = e->expiration - now.sec;
	if (sec < 0)
		sec = 0;

	eloop_register_timeout(sec + 1, 0, ptksa_cache_expire, ptksa, NULL);
}


/*
 * ptksa_cache_init - Initialize PTKSA cache
 *
 * Returns: Pointer to PTKSA cache data or %NULL on failure
 */
struct ptksa_cache * ptksa_cache_init(void)
{
	struct ptksa_cache *ptksa = os_zalloc(sizeof(struct ptksa_cache));

	wpa_printf(MSG_DEBUG, "PTKSA: Initializing");

	if (ptksa)
		dl_list_init(&ptksa->ptksa);

	return ptksa;
}


/*
 * ptksa_cache_deinit - Free all entries in PTKSA cache
 * @ptksa: Pointer to PTKSA cache data from ptksa_cache_init()
 */
void ptksa_cache_deinit(struct ptksa_cache *ptksa)
{
	struct ptksa_cache_entry *e, *next;

	if (!ptksa)
		return;

	wpa_printf(MSG_DEBUG, "PTKSA: Deinit. n_ptksa=%u", ptksa->n_ptksa);

	dl_list_for_each_safe(e, next, &ptksa->ptksa,
			      struct ptksa_cache_entry, list)
		ptksa_cache_free_entry(ptksa, e);

	eloop_cancel_timeout(ptksa_cache_expire, ptksa, NULL);
	os_free(ptksa);
}


/*
 * ptksa_cache_get - Fetch a PTKSA cache entry
 * @ptksa: Pointer to PTKSA cache data from ptksa_cache_init()
 * @addr: Peer address or %NULL to match any
 * @cipher: Specific cipher suite to search for or WPA_CIPHER_NONE for any
 * Returns: Pointer to PTKSA cache entry or %NULL if no match was found
 */
struct ptksa_cache_entry * ptksa_cache_get(struct ptksa_cache *ptksa,
					   const u8 *addr, u32 cipher)
{
	struct ptksa_cache_entry *e;

	if (!ptksa)
		return NULL;

	dl_list_for_each(e, &ptksa->ptksa, struct ptksa_cache_entry, list) {
		if ((!addr || os_memcmp(e->addr, addr, ETH_ALEN) == 0) &&
		    (cipher == WPA_CIPHER_NONE || cipher == e->cipher))
			return e;
	}

	return NULL;
}


/*
 * ptksa_cache_list - Dump text list of entries in PTKSA cache
 * @ptksa: Pointer to PTKSA cache data from ptksa_cache_init()
 * @buf: Buffer for the list
 * @len: Length of the buffer
 * Returns: Number of bytes written to buffer
 *
 * This function is used to generate a text format representation of the
 * current PTKSA cache contents for the ctrl_iface PTKSA command.
 */
int ptksa_cache_list(struct ptksa_cache *ptksa, char *buf, size_t len)
{
	struct ptksa_cache_entry *e;
	int i = 0, ret;
	char *pos = buf;
	struct os_reltime now;

	if (!ptksa)
		return 0;

	os_get_reltime(&now);

	ret = os_snprintf(pos, buf + len - pos,
			  "Index / ADDR / Cipher / expiration (secs) / TK / KDK\n");
	if (os_snprintf_error(buf + len - pos, ret))
		return pos - buf;
	pos += ret;

	dl_list_for_each(e, &ptksa->ptksa, struct ptksa_cache_entry, list) {
		ret = os_snprintf(pos, buf + len - pos, "%u " MACSTR,
				  i, MAC2STR(e->addr));
		if (os_snprintf_error(buf + len - pos, ret))
			return pos - buf;
		pos += ret;

		ret = os_snprintf(pos, buf + len - pos, " %s %lu ",
				  wpa_cipher_txt(e->cipher),
				  e->expiration - now.sec);
		if (os_snprintf_error(buf + len - pos, ret))
			return pos - buf;
		pos += ret;

		ret = wpa_snprintf_hex(pos, buf + len - pos, e->ptk.tk,
				       e->ptk.tk_len);
		if (os_snprintf_error(buf + len - pos, ret))
			return pos - buf;
		pos += ret;

		ret = os_snprintf(pos, buf + len - pos, " ");
		if (os_snprintf_error(buf + len - pos, ret))
			return pos - buf;
		pos += ret;

		ret = wpa_snprintf_hex(pos, buf + len - pos, e->ptk.kdk,
				       e->ptk.kdk_len);
		if (os_snprintf_error(buf + len - pos, ret))
			return pos - buf;
		pos += ret;

		ret = os_snprintf(pos, buf + len - pos, "\n");
		if (os_snprintf_error(buf + len - pos, ret))
			return pos - buf;
		pos += ret;

		i++;
	}

	return pos - buf;
}


/*
 * ptksa_cache_flush - Flush PTKSA cache entries
 *
 * @ptksa: Pointer to PTKSA cache data from ptksa_cache_init()
 * @addr: Peer address or %NULL to match any
 * @cipher: Specific cipher suite to search for or WPA_CIPHER_NONE for any
 */
void ptksa_cache_flush(struct ptksa_cache *ptksa, const u8 *addr, u32 cipher)
{
	struct ptksa_cache_entry *e, *next;
	bool removed = false;

	if (!ptksa)
		return;

	dl_list_for_each_safe(e, next, &ptksa->ptksa, struct ptksa_cache_entry,
			      list) {
		if ((!addr || os_memcmp(e->addr, addr, ETH_ALEN) == 0) &&
		    (cipher == WPA_CIPHER_NONE || cipher == e->cipher)) {
			wpa_printf(MSG_DEBUG,
				   "Flush PTKSA cache entry for " MACSTR,
				   MAC2STR(e->addr));

			ptksa_cache_free_entry(ptksa, e);
			removed = true;
		}
	}

	if (removed)
		ptksa_cache_set_expiration(ptksa);
}


/*
 * ptksa_cache_add - Add a PTKSA cache entry
 * @ptksa: Pointer to PTKSA cache data from ptksa_cache_init()
 * @addr: Peer address
 * @cipher: The cipher used
 * @life_time: The PTK life time in seconds
 * @ptk: The PTK
 * Returns: Pointer to the added PTKSA cache entry or %NULL on error
 *
 * This function creates a PTKSA entry and adds it to the PTKSA cache.
 * If an old entry is already in the cache for the same peer and cipher
 * this entry will be replaced with the new entry.
 */
struct ptksa_cache_entry * ptksa_cache_add(struct ptksa_cache *ptksa,
					   const u8 *addr, u32 cipher,
					   u32 life_time,
					   const struct wpa_ptk *ptk)
{
	struct ptksa_cache_entry *entry, *tmp;
	struct os_reltime now;

	if (!ptksa || !ptk || !addr || !life_time || cipher == WPA_CIPHER_NONE)
		return NULL;

	/* remove a previous entry if present */
	ptksa_cache_flush(ptksa, addr, cipher);

	/* no place to add another entry */
	if (ptksa->n_ptksa >= PTKSA_CACHE_MAX_ENTRIES)
		return NULL;

	entry = os_zalloc(sizeof(*entry));
	if (!entry)
		return NULL;

	dl_list_init(&entry->list);
	os_memcpy(entry->addr, addr, ETH_ALEN);
	entry->cipher = cipher;

	os_memcpy(&entry->ptk, ptk, sizeof(entry->ptk));

	os_get_reltime(&now);
	entry->expiration = now.sec + life_time;

	dl_list_for_each(tmp, &ptksa->ptksa, struct ptksa_cache_entry, list) {
		if (tmp->expiration > entry->expiration)
			break;
	}

	/*
	 * If the list was empty add to the head; otherwise if the expiration is
	 * later then all other entries, add it to the end of the list;
	 * otherwise add it before the relevant entry.
	 */
	if (!tmp)
		dl_list_add(&ptksa->ptksa, &entry->list);
	else if (tmp->expiration < entry->expiration)
		dl_list_add(&tmp->list, &entry->list);
	else
		dl_list_add_tail(&tmp->list, &entry->list);

	ptksa->n_ptksa++;
	wpa_printf(MSG_DEBUG,
		   "Added PTKSA cache entry addr=" MACSTR " cipher=%u",
		   MAC2STR(addr), cipher);

	return entry;
}
