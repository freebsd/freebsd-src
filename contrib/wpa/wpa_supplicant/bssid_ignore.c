/*
 * wpa_supplicant - List of temporarily ignored BSSIDs
 * Copyright (c) 2003-2021, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "wpa_supplicant_i.h"
#include "bssid_ignore.h"

/**
 * wpa_bssid_ignore_get - Get the ignore list entry for a BSSID
 * @wpa_s: Pointer to wpa_supplicant data
 * @bssid: BSSID
 * Returns: Matching entry for the BSSID or %NULL if not found
 */
struct wpa_bssid_ignore * wpa_bssid_ignore_get(struct wpa_supplicant *wpa_s,
					       const u8 *bssid)
{
	struct wpa_bssid_ignore *e;

	if (wpa_s == NULL || bssid == NULL)
		return NULL;

	if (wpa_s->current_ssid &&
	    wpa_s->current_ssid->was_recently_reconfigured) {
		wpa_bssid_ignore_clear(wpa_s);
		wpa_s->current_ssid->was_recently_reconfigured = false;
		return NULL;
	}

	wpa_bssid_ignore_update(wpa_s);

	e = wpa_s->bssid_ignore;
	while (e) {
		if (ether_addr_equal(e->bssid, bssid))
			return e;
		e = e->next;
	}

	return NULL;
}


/**
 * wpa_bssid_ignore_add - Add an BSSID to the ignore list
 * @wpa_s: Pointer to wpa_supplicant data
 * @bssid: BSSID to be added to the ignore list
 * Returns: Current ignore list count on success, -1 on failure
 *
 * This function adds the specified BSSID to the ignore list or increases the
 * ignore count if the BSSID was already listed. It should be called when
 * an association attempt fails either due to the selected BSS rejecting
 * association or due to timeout.
 *
 * This ignore list is used to force %wpa_supplicant to go through all available
 * BSSes before retrying to associate with an BSS that rejected or timed out
 * association. It does not prevent the listed BSS from being used; it only
 * changes the order in which they are tried.
 */
int wpa_bssid_ignore_add(struct wpa_supplicant *wpa_s, const u8 *bssid)
{
	struct wpa_bssid_ignore *e;
	struct os_reltime now;

	if (wpa_s == NULL || bssid == NULL)
		return -1;

	e = wpa_bssid_ignore_get(wpa_s, bssid);
	os_get_reltime(&now);
	if (e) {
		e->start = now;
		e->count++;
		if (e->count > 5)
			e->timeout_secs = 1800;
		else if (e->count == 5)
			e->timeout_secs = 600;
		else if (e->count == 4)
			e->timeout_secs = 120;
		else if (e->count == 3)
			e->timeout_secs = 60;
		else
			e->timeout_secs = 10;
		wpa_msg(wpa_s, MSG_INFO, "BSSID " MACSTR
			" ignore list count incremented to %d, ignoring for %d seconds",
			MAC2STR(bssid), e->count, e->timeout_secs);
		return e->count;
	}

	e = os_zalloc(sizeof(*e));
	if (e == NULL)
		return -1;
	os_memcpy(e->bssid, bssid, ETH_ALEN);
	e->count = 1;
	e->timeout_secs = 10;
	e->start = now;
	e->next = wpa_s->bssid_ignore;
	wpa_s->bssid_ignore = e;
	wpa_msg(wpa_s, MSG_INFO, "Added BSSID " MACSTR
		" into ignore list, ignoring for %d seconds",
		MAC2STR(bssid), e->timeout_secs);

	return e->count;
}


/**
 * wpa_bssid_ignore_del - Remove an BSSID from the ignore list
 * @wpa_s: Pointer to wpa_supplicant data
 * @bssid: BSSID to be removed from the ignore list
 * Returns: 0 on success, -1 on failure
 */
int wpa_bssid_ignore_del(struct wpa_supplicant *wpa_s, const u8 *bssid)
{
	struct wpa_bssid_ignore *e, *prev = NULL;

	if (wpa_s == NULL || bssid == NULL)
		return -1;

	e = wpa_s->bssid_ignore;
	while (e) {
		if (ether_addr_equal(e->bssid, bssid)) {
			if (prev == NULL) {
				wpa_s->bssid_ignore = e->next;
			} else {
				prev->next = e->next;
			}
			wpa_msg(wpa_s, MSG_INFO, "Removed BSSID " MACSTR
				" from ignore list", MAC2STR(bssid));
			os_free(e);
			return 0;
		}
		prev = e;
		e = e->next;
	}
	return -1;
}


/**
 * wpa_bssid_ignore_is_listed - Check whether a BSSID is ignored temporarily
 * @wpa_s: Pointer to wpa_supplicant data
 * @bssid: BSSID to be checked
 * Returns: count if BSS is currently considered to be ignored, 0 otherwise
 */
int wpa_bssid_ignore_is_listed(struct wpa_supplicant *wpa_s, const u8 *bssid)
{
	struct wpa_bssid_ignore *e;
	struct os_reltime now;

	e = wpa_bssid_ignore_get(wpa_s, bssid);
	if (!e)
		return 0;
	os_get_reltime(&now);
	if (os_reltime_expired(&now, &e->start, e->timeout_secs))
		return 0;
	return e->count;
}


/**
 * wpa_bssid_ignore_clear - Clear the ignore list of all entries
 * @wpa_s: Pointer to wpa_supplicant data
 */
void wpa_bssid_ignore_clear(struct wpa_supplicant *wpa_s)
{
	struct wpa_bssid_ignore *e, *prev;

	e = wpa_s->bssid_ignore;
	wpa_s->bssid_ignore = NULL;
	while (e) {
		prev = e;
		e = e->next;
		wpa_msg(wpa_s, MSG_INFO, "Removed BSSID " MACSTR
			" from ignore list (clear)", MAC2STR(prev->bssid));
		os_free(prev);
	}
}


/**
 * wpa_bssid_ignore_update - Update the entries in the ignore list,
 * deleting entries that have been expired for over an hour.
 * @wpa_s: Pointer to wpa_supplicant data
 */
void wpa_bssid_ignore_update(struct wpa_supplicant *wpa_s)
{
	struct wpa_bssid_ignore *e, *prev = NULL;
	struct os_reltime now;

	if (!wpa_s)
		return;

	e = wpa_s->bssid_ignore;
	os_get_reltime(&now);
	while (e) {
		if (os_reltime_expired(&now, &e->start,
				       e->timeout_secs + 3600)) {
			struct wpa_bssid_ignore *to_delete = e;

			if (prev) {
				prev->next = e->next;
				e = prev->next;
			} else {
				wpa_s->bssid_ignore = e->next;
				e = wpa_s->bssid_ignore;
			}
			wpa_msg(wpa_s, MSG_INFO, "Removed BSSID " MACSTR
				" from ignore list (expired)",
				MAC2STR(to_delete->bssid));
			os_free(to_delete);
		} else {
			prev = e;
			e = e->next;
		}
	}
}
