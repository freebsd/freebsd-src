#ifndef HOSTAPD_H
#define HOSTAPD_H

#include "common.h"
#include "ap.h"

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif
#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif
#ifndef ETH_P_ALL
#define ETH_P_ALL 0x0003
#endif
#ifndef ETH_P_PAE
#define ETH_P_PAE 0x888E /* Port Access Entity (IEEE 802.1X) */
#endif /* ETH_P_PAE */

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#include "config.h"

struct ieee8023_hdr {
	u8 dest[6];
	u8 src[6];
	u16 ethertype;
} __attribute__ ((packed));


struct ieee80211_hdr {
	u16 frame_control;
	u16 duration_id;
	u8 addr1[6];
	u8 addr2[6];
	u8 addr3[6];
	u16 seq_ctrl;
	/* followed by 'u8 addr4[6];' if ToDS and FromDS is set in data frame
	 */
} __attribute__ ((packed));

#define IEEE80211_DA_FROMDS addr1
#define IEEE80211_BSSID_FROMDS addr2
#define IEEE80211_SA_FROMDS addr3

#define IEEE80211_HDRLEN (sizeof(struct ieee80211_hdr))

#define IEEE80211_FC(type, stype) host_to_le16((type << 2) | (stype << 4))

/* MTU to be set for the wlan#ap device; this is mainly needed for IEEE 802.1X
 * frames that might be longer than normal default MTU and they are not
 * fragmented */
#define HOSTAPD_MTU 2290

extern unsigned char rfc1042_header[6];

typedef struct hostapd_data hostapd;

struct hostap_sta_driver_data {
	unsigned long rx_packets, tx_packets, rx_bytes, tx_bytes;
};

struct driver_ops;
struct wpa_ctrl_dst;
struct radius_server_data;

struct hostapd_data {
	struct hostapd_config *conf;
	char *config_fname;

	u8 own_addr[6];

	int num_sta; /* number of entries in sta_list */
	struct sta_info *sta_list; /* STA info list head */
	struct sta_info *sta_hash[STA_HASH_SIZE];

	/* pointers to STA info; based on allocated AID or NULL if AID free
	 * AID is in the range 1-2007, so sta_aid[0] corresponders to AID 1
	 * and so on
	 */
	struct sta_info *sta_aid[MAX_AID_TABLE_SIZE];

	struct driver_ops *driver;

	u8 *default_wep_key;
	u8 default_wep_key_idx;

	struct radius_client_data *radius;
	u32 acct_session_id_hi, acct_session_id_lo;

	struct iapp_data *iapp;

	enum { DO_NOT_ASSOC = 0, WAIT_BEACON, AUTHENTICATE, ASSOCIATE,
	       ASSOCIATED } assoc_ap_state;
	char assoc_ap_ssid[33];
	int assoc_ap_ssid_len;
	u16 assoc_ap_aid;

	struct hostapd_cached_radius_acl *acl_cache;
	struct hostapd_acl_query_data *acl_queries;

	u8 *wpa_ie;
	size_t wpa_ie_len;
	struct wpa_authenticator *wpa_auth;

#define PMKID_HASH_SIZE 128
#define PMKID_HASH(pmkid) (unsigned int) ((pmkid)[0] & 0x7f)
	struct rsn_pmksa_cache *pmkid[PMKID_HASH_SIZE];
	struct rsn_pmksa_cache *pmksa;
	int pmksa_count;

	struct rsn_preauth_interface *preauth_iface;
	time_t michael_mic_failure;
	int michael_mic_failures;
	int tkip_countermeasures;

	int ctrl_sock;
	struct wpa_ctrl_dst *ctrl_dst;

	void *ssl_ctx;
	void *eap_sim_db_priv;
	struct radius_server_data *radius_srv;
};

void hostapd_new_assoc_sta(hostapd *hapd, struct sta_info *sta, int reassoc);
void hostapd_logger(struct hostapd_data *hapd, const u8 *addr,
		    unsigned int module, int level, const char *fmt,
		    ...) __attribute__ ((format (printf, 5, 6)));


#define HOSTAPD_DEBUG(level, args...) \
do { \
	if (hapd->conf == NULL || hapd->conf->debug >= (level)) \
		printf(args); \
} while (0)

#define HOSTAPD_DEBUG_COND(level) (hapd->conf->debug >= (level))

const char * hostapd_ip_txt(const struct hostapd_ip_addr *addr, char *buf,
			    size_t buflen);

#endif /* HOSTAPD_H */
