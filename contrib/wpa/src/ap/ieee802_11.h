/*
 * hostapd / IEEE 802.11 Management
 * Copyright (c) 2002-2009, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef IEEE802_11_H
#define IEEE802_11_H

struct hostapd_iface;
struct hostapd_data;
struct sta_info;
struct hostapd_frame_info;
struct ieee80211_ht_capabilities;
struct ieee80211_vht_capabilities;
struct ieee80211_mgmt;

int ieee802_11_mgmt(struct hostapd_data *hapd, const u8 *buf, size_t len,
		    struct hostapd_frame_info *fi);
void ieee802_11_mgmt_cb(struct hostapd_data *hapd, const u8 *buf, size_t len,
			u16 stype, int ok);
void hostapd_2040_coex_action(struct hostapd_data *hapd,
			      const struct ieee80211_mgmt *mgmt, size_t len);
#ifdef NEED_AP_MLME
int ieee802_11_get_mib(struct hostapd_data *hapd, char *buf, size_t buflen);
int ieee802_11_get_mib_sta(struct hostapd_data *hapd, struct sta_info *sta,
			   char *buf, size_t buflen);
#else /* NEED_AP_MLME */
static inline int ieee802_11_get_mib(struct hostapd_data *hapd, char *buf,
				     size_t buflen)
{
	return 0;
}

static inline int ieee802_11_get_mib_sta(struct hostapd_data *hapd,
					 struct sta_info *sta,
					 char *buf, size_t buflen)
{
	return 0;
}
#endif /* NEED_AP_MLME */
u16 hostapd_own_capab_info(struct hostapd_data *hapd);
void ap_ht2040_timeout(void *eloop_data, void *user_data);
u8 * hostapd_eid_ext_capab(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_qos_map_set(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_supp_rates(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_ext_supp_rates(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_ht_capabilities(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_ht_operation(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_vht_capabilities(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_vht_operation(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_vendor_vht(struct hostapd_data *hapd, u8 *eid);
int hostapd_ht_operation_update(struct hostapd_iface *iface);
void ieee802_11_send_sa_query_req(struct hostapd_data *hapd,
				  const u8 *addr, const u8 *trans_id);
void hostapd_get_ht_capab(struct hostapd_data *hapd,
			  struct ieee80211_ht_capabilities *ht_cap,
			  struct ieee80211_ht_capabilities *neg_ht_cap);
void hostapd_get_vht_capab(struct hostapd_data *hapd,
			   struct ieee80211_vht_capabilities *vht_cap,
			   struct ieee80211_vht_capabilities *neg_vht_cap);
u16 copy_sta_ht_capab(struct hostapd_data *hapd, struct sta_info *sta,
		      const u8 *ht_capab);
u16 copy_sta_vendor_vht(struct hostapd_data *hapd, struct sta_info *sta,
			const u8 *ie, size_t len);

void update_ht_state(struct hostapd_data *hapd, struct sta_info *sta);
void ht40_intolerant_add(struct hostapd_iface *iface, struct sta_info *sta);
void ht40_intolerant_remove(struct hostapd_iface *iface, struct sta_info *sta);
u16 copy_sta_vht_capab(struct hostapd_data *hapd, struct sta_info *sta,
		       const u8 *vht_capab);
u16 set_sta_vht_opmode(struct hostapd_data *hapd, struct sta_info *sta,
		       const u8 *vht_opmode);
void hostapd_tx_status(struct hostapd_data *hapd, const u8 *addr,
		       const u8 *buf, size_t len, int ack);
void hostapd_eapol_tx_status(struct hostapd_data *hapd, const u8 *dst,
			     const u8 *data, size_t len, int ack);
void ieee802_11_rx_from_unknown(struct hostapd_data *hapd, const u8 *src,
				int wds);
u8 * hostapd_eid_assoc_comeback_time(struct hostapd_data *hapd,
				     struct sta_info *sta, u8 *eid);
void ieee802_11_sa_query_action(struct hostapd_data *hapd,
				const u8 *sa, const u8 action_type,
				const u8 *trans_id);
u8 * hostapd_eid_interworking(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_adv_proto(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_roaming_consortium(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_time_adv(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_time_zone(struct hostapd_data *hapd, u8 *eid);
int hostapd_update_time_adv(struct hostapd_data *hapd);
void hostapd_client_poll_ok(struct hostapd_data *hapd, const u8 *addr);
u8 * hostapd_eid_bss_max_idle_period(struct hostapd_data *hapd, u8 *eid);

int auth_sae_init_committed(struct hostapd_data *hapd, struct sta_info *sta);
#ifdef CONFIG_SAE
void sae_clear_retransmit_timer(struct hostapd_data *hapd,
				struct sta_info *sta);
#else /* CONFIG_SAE */
static inline void sae_clear_retransmit_timer(struct hostapd_data *hapd,
					      struct sta_info *sta)
{
}
#endif /* CONFIG_SAE */

#endif /* IEEE802_11_H */
