#ifndef AP_H
#define AP_H

/* STA flags */
#define WLAN_STA_AUTH BIT(0)
#define WLAN_STA_ASSOC BIT(1)
#define WLAN_STA_PS BIT(2)
#define WLAN_STA_TIM BIT(3)
#define WLAN_STA_PERM BIT(4)
#define WLAN_STA_AUTHORIZED BIT(5)
#define WLAN_STA_PENDING_POLL BIT(6) /* pending activity poll not ACKed */
#define WLAN_STA_PREAUTH BIT(7)

#define WLAN_RATE_1M BIT(0)
#define WLAN_RATE_2M BIT(1)
#define WLAN_RATE_5M5 BIT(2)
#define WLAN_RATE_11M BIT(3)
#define WLAN_RATE_COUNT 4

/* Maximum size of Supported Rates info element. IEEE 802.11 has a limit of 8,
 * but some pre-standard IEEE 802.11g products use longer elements. */
#define WLAN_SUPP_RATES_MAX 32


struct sta_info {
	struct sta_info *next; /* next entry in sta list */
	struct sta_info *hnext; /* next entry in hash table list */
	u8 addr[6];
	u16 aid; /* STA's unique AID (1 .. 2007) or 0 if not yet assigned */
	u32 flags;
	u16 capability;
	u16 listen_interval; /* or beacon_int for APs */
	u8 supported_rates[WLAN_SUPP_RATES_MAX];
	u8 tx_supp_rates;

	enum {
		STA_NULLFUNC = 0, STA_DISASSOC, STA_DEAUTH, STA_REMOVE
	} timeout_next;

	/* IEEE 802.1X related data */
	struct eapol_state_machine *eapol_sm;

	/* IEEE 802.11f (IAPP) related data */
	struct ieee80211_mgmt *last_assoc_req;

	u32 acct_session_id_hi;
	u32 acct_session_id_lo;
	time_t acct_session_start;
	int acct_session_started;
	int acct_terminate_cause; /* Acct-Terminate-Cause */
	int acct_interim_interval; /* Acct-Interim-Interval */

	unsigned long last_rx_bytes;
	unsigned long last_tx_bytes;
	u32 acct_input_gigawords; /* Acct-Input-Gigawords */
	u32 acct_output_gigawords; /* Acct-Output-Gigawords */

	u8 *challenge; /* IEEE 802.11 Shared Key Authentication Challenge */

	int pairwise; /* Pairwise cipher suite, WPA_CIPHER_* */
	u8 *wpa_ie;
	size_t wpa_ie_len;
	struct wpa_state_machine *wpa_sm;
	enum {
		WPA_VERSION_NO_WPA = 0 /* WPA not used */,
		WPA_VERSION_WPA = 1 /* WPA / IEEE 802.11i/D3.0 */,
		WPA_VERSION_WPA2 = 2 /* WPA2 / IEEE 802.11i */
	} wpa;
	int wpa_key_mgmt; /* the selected WPA_KEY_MGMT_* */
	struct rsn_pmksa_cache *pmksa;
	struct rsn_preauth_interface *preauth_iface;
	u8 req_replay_counter[8 /* WPA_REPLAY_COUNTER_LEN */];
	int req_replay_counter_used;
	u32 dot11RSNAStatsTKIPLocalMICFailures;
	u32 dot11RSNAStatsTKIPRemoteMICFailures;
};


#define MAX_STA_COUNT 1024

/* Maximum number of AIDs to use for STAs; must be 2007 or lower
 * (8802.11 limitation) */
#define MAX_AID_TABLE_SIZE 128

#define STA_HASH_SIZE 256
#define STA_HASH(sta) (sta[5])


/* Default value for maximum station inactivity. After AP_MAX_INACTIVITY has
 * passed since last received frame from the station, a nullfunc data frame is
 * sent to the station. If this frame is not acknowledged and no other frames
 * have been received, the station will be disassociated after
 * AP_DISASSOC_DELAY seconds. Similarily, the station will be deauthenticated
 * after AP_DEAUTH_DELAY seconds has passed after disassociation. */
#define AP_MAX_INACTIVITY (5 * 60)
#define AP_DISASSOC_DELAY (1)
#define AP_DEAUTH_DELAY (1)

#endif /* AP_H */
