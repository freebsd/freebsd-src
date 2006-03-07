#ifndef STA_INFO_H
#define STA_INFO_H

int ap_for_each_sta(struct hostapd_data *hapd,
		    int (*cb)(struct hostapd_data *hapd, struct sta_info *sta,
			      void *ctx),
		    void *ctx);
struct sta_info * ap_get_sta(struct hostapd_data *hapd, const u8 *sta);
void ap_sta_hash_add(hostapd *hapd, struct sta_info *sta);
void ap_free_sta(hostapd *hapd, struct sta_info *sta);
void ap_free_sta(hostapd *hapd, struct sta_info *sta);
void hostapd_free_stas(hostapd *hapd);
void ap_handle_timer(void *eloop_ctx, void *timeout_ctx);
void ap_sta_session_timeout(hostapd *hapd, struct sta_info *sta,
			    u32 session_timeout);
void ap_sta_no_session_timeout(hostapd *hapd, struct sta_info *sta);
struct sta_info * ap_sta_add(struct hostapd_data *hapd, const u8 *addr);

#endif /* STA_INFO_H */
