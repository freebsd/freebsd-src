/*
 * Copyright (c) 2011-2013, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef HS20_SUPPLICANT_H
#define HS20_SUPPLICANT_H

void wpas_hs20_add_indication(struct wpabuf *buf, int pps_mo_id,
			      int ap_release);
void wpas_hs20_add_roam_cons_sel(struct wpabuf *buf,
				 const struct wpa_ssid *ssid);

int hs20_anqp_send_req(struct wpa_supplicant *wpa_s, const u8 *dst, u32 stypes,
		       const u8 *payload, size_t payload_len);
void hs20_put_anqp_req(u32 stypes, const u8 *payload, size_t payload_len,
		       struct wpabuf *buf);
void hs20_parse_rx_hs20_anqp_resp(struct wpa_supplicant *wpa_s,
				  struct wpa_bss *bss, const u8 *sa,
				  const u8 *data, size_t slen, u8 dialog_token);
int get_hs20_version(struct wpa_bss *bss);
int is_hs20_network(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid,
		    struct wpa_bss *bss);
int hs20_get_pps_mo_id(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid);

void hs20_rx_deauth_imminent_notice(struct wpa_supplicant *wpa_s, u8 code,
				    u16 reauth_delay, const char *url);
void hs20_rx_t_c_acceptance(struct wpa_supplicant *wpa_s, const char *url);


#endif /* HS20_SUPPLICANT_H */
