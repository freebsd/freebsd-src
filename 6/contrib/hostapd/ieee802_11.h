#ifndef IEEE802_11_H
#define IEEE802_11_H


struct ieee80211_mgmt {
	u16 frame_control;
	u16 duration;
	u8 da[6];
	u8 sa[6];
	u8 bssid[6];
	u16 seq_ctrl;
	union {
		struct {
			u16 auth_alg;
			u16 auth_transaction;
			u16 status_code;
			/* possibly followed by Challenge text */
			u8 variable[0];
		} __attribute__ ((packed)) auth;
		struct {
			u16 reason_code;
		} __attribute__ ((packed)) deauth;
		struct {
			u16 capab_info;
			u16 listen_interval;
			/* followed by SSID and Supported rates */
			u8 variable[0];
		} __attribute__ ((packed)) assoc_req;
		struct {
			u16 capab_info;
			u16 status_code;
			u16 aid;
			/* followed by Supported rates */
			u8 variable[0];
		} __attribute__ ((packed)) assoc_resp, reassoc_resp;
		struct {
			u16 capab_info;
			u16 listen_interval;
			u8 current_ap[6];
			/* followed by SSID and Supported rates */
			u8 variable[0];
		} __attribute__ ((packed)) reassoc_req;
		struct {
			u16 reason_code;
		} __attribute__ ((packed)) disassoc;
		struct {
			u8 timestamp[8];
			u16 beacon_int;
			u16 capab_info;
			/* followed by some of SSID, Supported rates,
			 * FH Params, DS Params, CF Params, IBSS Params, TIM */
			u8 variable[0];
		} __attribute__ ((packed)) beacon;
	} u;
} __attribute__ ((packed));


/* Parsed Information Elements */
struct ieee802_11_elems {
	u8 *ssid;
	u8 ssid_len;
	u8 *supp_rates;
	u8 supp_rates_len;
	u8 *fh_params;
	u8 fh_params_len;
	u8 *ds_params;
	u8 ds_params_len;
	u8 *cf_params;
	u8 cf_params_len;
	u8 *tim;
	u8 tim_len;
	u8 *ibss_params;
	u8 ibss_params_len;
	u8 *challenge;
	u8 challenge_len;
	u8 *wpa_ie;
	u8 wpa_ie_len;
	u8 *rsn_ie;
	u8 rsn_ie_len;
};

typedef enum { ParseOK = 0, ParseUnknown = 1, ParseFailed = -1 } ParseRes;


void ieee802_11_mgmt(struct hostapd_data *hapd, u8 *buf, size_t len,
		     u16 stype);
void ieee802_11_mgmt_cb(struct hostapd_data *hapd, u8 *buf, size_t len,
			u16 stype, int ok);
ParseRes ieee802_11_parse_elems(struct hostapd_data *hapd, u8 *start,
				size_t len,
				struct ieee802_11_elems *elems,
				int show_errors);
void ieee80211_michael_mic_failure(struct hostapd_data *hapd, u8 *addr,
				   int local);
int ieee802_11_get_mib(struct hostapd_data *hapd, char *buf, size_t buflen);
int ieee802_11_get_mib_sta(struct hostapd_data *hapd, struct sta_info *sta,
			   char *buf, size_t buflen);

#endif /* IEEE802_11_H */
