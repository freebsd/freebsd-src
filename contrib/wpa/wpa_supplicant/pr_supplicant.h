/*
 * Proxmity Ranging
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef PR_SUPPLICANT_H
#define PR_SUPPLICANT_H

#include "common/proximity_ranging.h"

#ifdef CONFIG_PR

int wpas_pr_init(struct wpa_global *global, struct wpa_supplicant *wpa_s,
		 const struct wpa_driver_capa *capa);
void wpas_pr_flush(struct wpa_supplicant *wpa_s);
void wpas_pr_deinit(struct wpa_supplicant *wpa_s);
void wpas_pr_pd_stop(struct wpa_supplicant *wpa_s);
void wpas_pr_update_dev_addr(struct wpa_supplicant *wpa_s);
void wpas_pr_clear_dev_iks(struct wpa_supplicant *wpa_s);
void wpas_pr_set_dev_ik(struct wpa_supplicant *wpa_s, const u8 *dik,
			const char *password, const u8 *pmk, size_t pmk_len,
			bool own);
struct wpabuf * wpas_pr_usd_elems(struct wpa_supplicant *wpa_s,
				  const u8 *src_addr);
void wpas_pr_process_usd_elems(struct wpa_supplicant *wpa_s, const u8 *buf,
			       u16 buf_len, const u8 *peer_addr,
			       unsigned int freq);
int wpas_pr_initiate_pasn_auth(struct wpa_supplicant *wpa_s,
			       const u8 *peer_addr, int freq, u8 auth_mode,
			       u8 ranging_role, u8 ranging_type,
			       int forced_pr_freq, const u8 *src_addr,
			       enum pr_pasn_role pasn_role);
int wpas_pr_pasn_auth_tx_status(struct wpa_supplicant *wpa_s, const u8 *data,
				size_t data_len, bool acked);
int wpas_pr_pasn_auth_rx(struct wpa_supplicant *wpa_s,
			 const struct ieee80211_mgmt *mgmt, size_t len,
			 int freq);
void wpas_pr_cancel_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
					 unsigned int freq);
int wpas_pr_pasn_trigger(struct wpa_supplicant *wpa_s,
			 struct pr_pasn_ranging_params *pr_pasn_params);
void wpas_pr_abort_ranging(struct wpa_supplicant *wpa_s);
void wpas_pr_measurement_complete(struct wpa_supplicant *wpa_s,
				  struct peer_measurement_complete *complete);
void wpas_pr_measurement_result(struct wpa_supplicant *wpa_s,
				struct peer_measurement_result *result);

#else /* CONFIG_PR */

static inline int wpas_pr_init(struct wpa_global *global,
			       struct wpa_supplicant *wpa_s,
			       const struct wpa_driver_capa *capa)
{
	return 0;
}

static inline void wpas_pr_flush(struct wpa_supplicant *wpa_s)
{
}

static inline void wpas_pr_deinit(struct wpa_supplicant *wpa_s)
{
}

static inline void wpas_pr_pd_stop(struct wpa_supplicant *wpa_s)
{
}

static inline void wpas_pr_update_dev_addr(struct wpa_supplicant *wpa_s)
{
}

static inline void wpas_pr_clear_dev_iks(struct wpa_supplicant *wpa_s)
{
}

static inline void wpas_pr_set_dev_ik(struct wpa_supplicant *wpa_s,
				      const u8 *dik, const char *password,
				      const u8 *pmk, size_t pmk_len, bool own)
{
}

static inline struct wpabuf * wpas_pr_usd_elems(struct wpa_supplicant *wpa_s,
						const u8 *src_addr)
{
	return NULL;
}

static inline int wpas_pr_initiate_pasn_auth(struct wpa_supplicant *wpa_s,
					     const u8 *peer_addr, int freq,
					     u8 auth_mode, u8 ranging_role,
					     u8 ranging_type,
					     int forced_pr_freq,
					     const u8 *src_addr,
					     enum pr_pasn_role pasn_role)
{
	return 0;
}

static inline int wpas_pr_pasn_auth_tx_status(struct wpa_supplicant *wpa_s,
					      const u8 *data, size_t data_len,
					      bool acked)
{
	return 0;
}

static inline int wpas_pr_pasn_auth_rx(struct wpa_supplicant *wpa_s,
				       const struct ieee80211_mgmt *mgmt,
				       size_t len, int freq)
{
	return 0;
}

static inline void
wpas_pr_cancel_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
				    unsigned int freq)
{
}

static inline int
wpas_pr_pasn_trigger(struct wpa_supplicant *wpa_s,
		     struct pr_pasn_ranging_params *pr_pasn_params)
{
	return -1;
}

static inline void wpas_pr_abort_ranging(struct wpa_supplicant *wpa_s)
{
}

static inline void
wpas_pr_measurement_complete(struct wpa_supplicant *wpa_s,
			     struct peer_measurement_complete *complete)
{
}

static inline void
wpas_pr_measurement_result(struct wpa_supplicant *wpa_s,
			   struct peer_measurement_result *result)
{
}

#endif /* CONFIG_PR */

#endif /* PR_SUPPLICANT_H */
