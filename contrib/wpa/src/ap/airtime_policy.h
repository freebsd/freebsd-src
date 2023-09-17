/*
 * Airtime policy configuration
 * Copyright (c) 2018-2019, Toke Høiland-Jørgensen <toke@toke.dk>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef AIRTIME_POLICY_H
#define AIRTIME_POLICY_H

struct hostapd_iface;

#ifdef CONFIG_AIRTIME_POLICY

#define AIRTIME_DEFAULT_UPDATE_INTERVAL 200 /* ms */
#define AIRTIME_BACKLOG_EXPIRY_FACTOR 2500 /* 2.5 intervals + convert to usec */

/* scale quantum so this becomes the effective quantum after applying the max
 * weight, but never go below min or above max */
#define AIRTIME_QUANTUM_MIN 8 /* usec */
#define AIRTIME_QUANTUM_MAX 256 /* usec */
#define AIRTIME_QUANTUM_TARGET 1024 /* usec */

int airtime_policy_new_sta(struct hostapd_data *hapd, struct sta_info *sta);
int airtime_policy_update_init(struct hostapd_iface *iface);
void airtime_policy_update_deinit(struct hostapd_iface *iface);

#else /* CONFIG_AIRTIME_POLICY */

static inline int airtime_policy_new_sta(struct hostapd_data *hapd,
					 struct sta_info *sta)
{
	return -1;
}

static inline int airtime_policy_update_init(struct hostapd_iface *iface)
{
	return -1;
}

static inline void airtime_policy_update_deinit(struct hostapd_iface *iface)
{
}

#endif /* CONFIG_AIRTIME_POLICY */

#endif /* AIRTIME_POLICY_H */
