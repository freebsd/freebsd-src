#ifndef WPA_H
#define WPA_H

#define WPA_NONCE_LEN 32
#define WPA_PMK_LEN PMK_LEN
#define WPA_REPLAY_COUNTER_LEN 8
#define WPA_GMK_LEN 32
#define WPA_GTK_MAX_LEN 32
#define WPA_KEY_RSC_LEN 8
#define PMKID_LEN 16

struct rsn_pmksa_cache {
	struct rsn_pmksa_cache *next, *hnext;
	u8 pmkid[PMKID_LEN];
	u8 pmk[PMK_LEN];
	time_t expiration;
	int akmp; /* WPA_KEY_MGMT_* */
	u8 spa[ETH_ALEN];
};

struct rsn_preauth_interface {
	struct rsn_preauth_interface *next;
	struct hostapd_data *hapd;
	struct l2_packet_data *l2;
	char *ifname;
	int ifindex;
};

struct wpa_eapol_key {
	u8 type;
	u16 key_info;
	u16 key_length;
	u8 replay_counter[WPA_REPLAY_COUNTER_LEN];
	u8 key_nonce[WPA_NONCE_LEN];
	u8 key_iv[16];
	u8 key_rsc[WPA_KEY_RSC_LEN];
	u8 key_id[8]; /* Reserved */
	u8 key_mic[16];
	u16 key_data_length;
	/* followed by key_data_length bytes of key_data */
} __attribute__ ((packed));

#define WPA_KEY_INFO_TYPE_MASK (BIT(0) | BIT(1) | BIT(2))
#define WPA_KEY_INFO_TYPE_HMAC_MD5_RC4 BIT(0)
#define WPA_KEY_INFO_TYPE_HMAC_SHA1_AES BIT(1)
#define WPA_KEY_INFO_KEY_TYPE BIT(3) /* 1 = Pairwise, 0 = Group key */
/* bit4..5 is used in WPA, but is reserved in IEEE 802.11i/RSN */
#define WPA_KEY_INFO_KEY_INDEX_MASK (BIT(4) | BIT(5))
#define WPA_KEY_INFO_KEY_INDEX_SHIFT 4
#define WPA_KEY_INFO_INSTALL BIT(6) /* pairwise */
#define WPA_KEY_INFO_TXRX BIT(6) /* group */
#define WPA_KEY_INFO_ACK BIT(7)
#define WPA_KEY_INFO_MIC BIT(8)
#define WPA_KEY_INFO_SECURE BIT(9)
#define WPA_KEY_INFO_ERROR BIT(10)
#define WPA_KEY_INFO_REQUEST BIT(11)
#define WPA_KEY_INFO_ENCR_KEY_DATA BIT(12)


/* per STA state machine data */

struct wpa_ptk {
	u8 mic_key[16]; /* EAPOL-Key MIC Key (MK) */
	u8 encr_key[16]; /* EAPOL-Key Encryption Key (EK) */
	u8 tk1[16]; /* Temporal Key 1 (TK1) */
	union {
		u8 tk2[16]; /* Temporal Key 2 (TK2) */
		struct {
			u8 tx_mic_key[8];
			u8 rx_mic_key[8];
		} auth;
	} u;
} __attribute__ ((packed));

struct wpa_state_machine {
	struct hostapd_data *hapd;
	struct sta_info *sta;

	enum {
		WPA_PTK_INITIALIZE, WPA_PTK_DISCONNECT, WPA_PTK_DISCONNECTED,
		WPA_PTK_AUTHENTICATION, WPA_PTK_AUTHENTICATION2,
		WPA_PTK_INITPMK, WPA_PTK_INITPSK, WPA_PTK_PTKSTART,
		WPA_PTK_PTKCALCNEGOTIATING, WPA_PTK_PTKCALCNEGOTIATING2,
		WPA_PTK_PTKINITNEGOTIATING, WPA_PTK_PTKINITDONE
	} wpa_ptk_state;

	enum {
		WPA_PTK_GROUP_IDLE = 0,
		WPA_PTK_GROUP_REKEYNEGOTIATING,
		WPA_PTK_GROUP_REKEYESTABLISHED,
		WPA_PTK_GROUP_KEYERROR
	} wpa_ptk_group_state;

	Boolean Init;
	Boolean DeauthenticationRequest;
	Boolean AuthenticationRequest;
	Boolean ReAuthenticationRequest;
	Boolean Disconnect;
	int TimeoutCtr;
	int GTimeoutCtr;
	Boolean TimeoutEvt;
	Boolean EAPOLKeyReceived;
	Boolean EAPOLKeyPairwise;
	Boolean EAPOLKeyRequest;
	Boolean MICVerified;
	Boolean GUpdateStationKeys;
	u8 ANonce[WPA_NONCE_LEN];
	u8 SNonce[WPA_NONCE_LEN];
	u8 PMK[WPA_PMK_LEN];
	struct wpa_ptk PTK;
	Boolean PTK_valid;
	Boolean pairwise_set;
	int keycount;
	Boolean Pair;
	u8 key_replay_counter[WPA_REPLAY_COUNTER_LEN];
	Boolean key_replay_counter_valid;
	Boolean PInitAKeys; /* WPA only, not in IEEE 802.11i/D8 */
	Boolean PTKRequest; /* not in IEEE 802.11i state machine */
	Boolean has_GTK;

	u8 *last_rx_eapol_key; /* starting from IEEE 802.1X header */
	size_t last_rx_eapol_key_len;

	Boolean changed;
};

/* per authenticator data */
struct wpa_authenticator {
	Boolean GInit;
	int GNoStations;
	int GKeyDoneStations;
	Boolean GTKReKey;
	int GTK_len;
	int GN, GM;
	Boolean GTKAuthenticator;
	u8 Counter[WPA_NONCE_LEN];

	enum {
		WPA_GROUP_GTK_INIT = 0,
		WPA_GROUP_SETKEYS, WPA_GROUP_SETKEYSDONE
	} wpa_group_state;

	u8 GMK[WPA_GMK_LEN];
	u8 GTK[2][WPA_GTK_MAX_LEN];
	u8 GNonce[WPA_NONCE_LEN];
	Boolean changed;

	unsigned int dot11RSNAStatsTKIPRemoteMICFailures;
	u8 dot11RSNAAuthenticationSuiteSelected[4];
	u8 dot11RSNAPairwiseCipherSelected[4];
	u8 dot11RSNAGroupCipherSelected[4];
	u8 dot11RSNAPMKIDUsed[PMKID_LEN];
	u8 dot11RSNAAuthenticationSuiteRequested[4]; /* FIX: update */
	u8 dot11RSNAPairwiseCipherRequested[4]; /* FIX: update */
	u8 dot11RSNAGroupCipherRequested[4]; /* FIX: update */
	unsigned int dot11RSNATKIPCounterMeasuresInvoked;
	unsigned int dot11RSNA4WayHandshakeFailures;
};


int wpa_init(struct hostapd_data *hapd);
void wpa_deinit(struct hostapd_data *hapd);

enum {
	WPA_IE_OK, WPA_INVALID_IE, WPA_INVALID_GROUP, WPA_INVALID_PAIRWISE,
	WPA_INVALID_AKMP
};
	
int wpa_validate_wpa_ie(struct hostapd_data *hapd, struct sta_info *sta,
			u8 *wpa_ie, size_t wpa_ie_len, int version);
void wpa_new_station(struct hostapd_data *hapd, struct sta_info *sta);
void wpa_free_station(struct sta_info *sta);
void wpa_receive(struct hostapd_data *hapd, struct sta_info *sta,
		 u8 *data, size_t data_len);
typedef enum {
	WPA_AUTH, WPA_ASSOC, WPA_DISASSOC, WPA_DEAUTH, WPA_REAUTH,
	WPA_REAUTH_EAPOL
} wpa_event;
void wpa_sm_event(struct hostapd_data *hapd, struct sta_info *sta,
		  wpa_event event);
void wpa_sm_notify(struct hostapd_data *hapd, struct sta_info *sta);
void pmksa_cache_add(struct hostapd_data *hapd, struct sta_info *sta, u8 *pmk,
		     int session_timeout);
void rsn_preauth_finished(struct hostapd_data *hapd, struct sta_info *sta,
			  int success);
void rsn_preauth_send(struct hostapd_data *hapd, struct sta_info *sta,
		      u8 *buf, size_t len);
void wpa_gtk_rekey(struct hostapd_data *hapd);
int wpa_get_mib(struct hostapd_data *hapd, char *buf, size_t buflen);
int wpa_get_mib_sta(struct hostapd_data *hapd, struct sta_info *sta,
		    char *buf, size_t buflen);

#endif /* WPA_H */
