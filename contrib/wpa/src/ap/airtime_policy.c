/*
 * Airtime policy configuration
 * Copyright (c) 2018-2019, Toke Høiland-Jørgensen <toke@toke.dk>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "hostapd.h"
#include "ap_drv_ops.h"
#include "sta_info.h"
#include "airtime_policy.h"

/* Idea:
 * Two modes of airtime enforcement:
 * 1. Static weights: specify weights per MAC address with a per-BSS default
 * 2. Per-BSS limits: Dynamically calculate weights of backlogged stations to
 *    enforce relative total shares between BSSes.
 *
 * - Periodic per-station callback to update queue status.
 *
 * Copy accounting_sta_update_stats() to get TXQ info and airtime weights and
 * keep them updated in sta_info.
 *
 * - Separate periodic per-bss (or per-iface?) callback to update weights.
 *
 * Just need to loop through all interfaces, count sum the active stations (or
 * should the per-STA callback just adjust that for the BSS?) and calculate new
 * weights.
 */

static int get_airtime_policy_update_timeout(struct hostapd_iface *iface,
					     unsigned int *sec,
					     unsigned int *usec)
{
	unsigned int update_int = iface->conf->airtime_update_interval;

	if (!update_int) {
		wpa_printf(MSG_ERROR,
			   "Airtime policy: Invalid airtime policy update interval %u",
			   update_int);
		return -1;
	}

	*sec = update_int / 1000;
	*usec = (update_int % 1000) * 1000;

	return 0;
}


static void set_new_backlog_time(struct hostapd_data *hapd,
				 struct sta_info *sta,
				 struct os_reltime *now)
{
	sta->backlogged_until = *now;
	sta->backlogged_until.usec += hapd->iconf->airtime_update_interval *
		AIRTIME_BACKLOG_EXPIRY_FACTOR;
	while (sta->backlogged_until.usec >= 1000000) {
		sta->backlogged_until.sec++;
		sta->backlogged_until.usec -= 1000000;
	}
}


static void count_backlogged_sta(struct hostapd_data *hapd)
{
	struct sta_info *sta;
	struct hostap_sta_driver_data data = {};
	unsigned int num_backlogged = 0;
	struct os_reltime now;

	os_get_reltime(&now);

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		if (hostapd_drv_read_sta_data(hapd, &data, sta->addr))
			continue;

		if (data.backlog_bytes > 0)
			set_new_backlog_time(hapd, sta, &now);
		if (os_reltime_before(&now, &sta->backlogged_until))
			num_backlogged++;
	}
	hapd->num_backlogged_sta = num_backlogged;
}


static int sta_set_airtime_weight(struct hostapd_data *hapd,
				  struct sta_info *sta,
				  unsigned int weight)
{
	int ret = 0;

	if (weight != sta->airtime_weight &&
	    (ret = hostapd_sta_set_airtime_weight(hapd, sta->addr, weight)))
		return ret;

	sta->airtime_weight = weight;
	return ret;
}


static void set_sta_weights(struct hostapd_data *hapd, unsigned int weight)
{
	struct sta_info *sta;

	for (sta = hapd->sta_list; sta; sta = sta->next)
		sta_set_airtime_weight(hapd, sta, weight);
}


static unsigned int get_airtime_quantum(unsigned int max_wt)
{
	unsigned int quantum = AIRTIME_QUANTUM_TARGET / max_wt;

	if (quantum < AIRTIME_QUANTUM_MIN)
		quantum = AIRTIME_QUANTUM_MIN;
	else if (quantum > AIRTIME_QUANTUM_MAX)
		quantum = AIRTIME_QUANTUM_MAX;

	return quantum;
}


static void update_airtime_weights(void *eloop_data, void *user_data)
{
	struct hostapd_iface *iface = eloop_data;
	struct hostapd_data *bss;
	unsigned int sec, usec;
	unsigned int num_sta_min = 0, num_sta_prod = 1, num_sta_sum = 0,
		wt_sum = 0;
	unsigned int quantum;
	Boolean all_div_min = TRUE;
	Boolean apply_limit = iface->conf->airtime_mode == AIRTIME_MODE_DYNAMIC;
	int wt, num_bss = 0, max_wt = 0;
	size_t i;

	for (i = 0; i < iface->num_bss; i++) {
		bss = iface->bss[i];
		if (!bss->started || !bss->conf->airtime_weight)
			continue;

		count_backlogged_sta(bss);
		if (!bss->num_backlogged_sta)
			continue;

		if (!num_sta_min || bss->num_backlogged_sta < num_sta_min)
			num_sta_min = bss->num_backlogged_sta;

		num_sta_prod *= bss->num_backlogged_sta;
		num_sta_sum += bss->num_backlogged_sta;
		wt_sum += bss->conf->airtime_weight;
		num_bss++;
	}

	if (num_sta_min) {
		for (i = 0; i < iface->num_bss; i++) {
			bss = iface->bss[i];
			if (!bss->started || !bss->conf->airtime_weight)
				continue;

			/* Check if we can divide all sta numbers by the
			 * smallest number to keep weights as small as possible.
			 * This is a lazy way to avoid having to factor
			 * integers. */
			if (bss->num_backlogged_sta &&
			    bss->num_backlogged_sta % num_sta_min > 0)
				all_div_min = FALSE;

			/* If we're in LIMIT mode, we only apply the weight
			 * scaling when the BSS(es) marked as limited would a
			 * larger share than the relative BSS weights indicates
			 * it should. */
			if (!apply_limit && bss->conf->airtime_limit) {
				if (bss->num_backlogged_sta * wt_sum >
				    bss->conf->airtime_weight * num_sta_sum)
					apply_limit = TRUE;
			}
		}
		if (all_div_min)
			num_sta_prod /= num_sta_min;
	}

	for (i = 0; i < iface->num_bss; i++) {
		bss = iface->bss[i];
		if (!bss->started || !bss->conf->airtime_weight)
			continue;

		/* We only set the calculated weight if the BSS has active
		 * stations and there are other active interfaces as well -
		 * otherwise we just set a unit weight. This ensures that
		 * the weights are set reasonably when stations transition from
		 * inactive to active. */
		if (apply_limit && bss->num_backlogged_sta && num_bss > 1)
			wt = bss->conf->airtime_weight * num_sta_prod /
				bss->num_backlogged_sta;
		else
			wt = 1;

		bss->airtime_weight = wt;
		if (wt > max_wt)
			max_wt = wt;
	}

	quantum = get_airtime_quantum(max_wt);

	for (i = 0; i < iface->num_bss; i++) {
		bss = iface->bss[i];
		if (!bss->started || !bss->conf->airtime_weight)
			continue;
		set_sta_weights(bss, bss->airtime_weight * quantum);
	}

	if (get_airtime_policy_update_timeout(iface, &sec, &usec) < 0)
		return;

	eloop_register_timeout(sec, usec, update_airtime_weights, iface,
			       NULL);
}


static int get_weight_for_sta(struct hostapd_data *hapd, const u8 *sta)
{
	struct airtime_sta_weight *wt;

	wt = hapd->conf->airtime_weight_list;
	while (wt && os_memcmp(wt->addr, sta, ETH_ALEN) != 0)
		wt = wt->next;

	return wt ? wt->weight : hapd->conf->airtime_weight;
}


int airtime_policy_new_sta(struct hostapd_data *hapd, struct sta_info *sta)
{
	unsigned int weight;

	if (hapd->iconf->airtime_mode == AIRTIME_MODE_STATIC) {
		weight = get_weight_for_sta(hapd, sta->addr);
		if (weight)
			return sta_set_airtime_weight(hapd, sta, weight);
	}
	return 0;
}


int airtime_policy_update_init(struct hostapd_iface *iface)
{
	unsigned int sec, usec;

	if (iface->conf->airtime_mode < AIRTIME_MODE_DYNAMIC)
		return 0;

	if (get_airtime_policy_update_timeout(iface, &sec, &usec) < 0)
		return -1;

	eloop_register_timeout(sec, usec, update_airtime_weights, iface, NULL);
	return 0;
}


void airtime_policy_update_deinit(struct hostapd_iface *iface)
{
	eloop_cancel_timeout(update_airtime_weights, iface, NULL);
}
