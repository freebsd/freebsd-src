#ifndef WPA_SUPPLICANT_I_H
#define WPA_SUPPLICANT_I_H

#include "driver.h"

#ifdef EAPOL_TEST
#include <netinet/in.h>

struct hostapd_radius_server {
	struct in_addr addr;
	int port;
	u8 *shared_secret;
	size_t shared_secret_len;
};
#endif /* EAPOL_TEST */

#define PMKID_LEN 16
struct rsn_pmksa_cache {
	struct rsn_pmksa_cache *next;
	u8 pmkid[PMKID_LEN];
	u8 pmk[PMK_LEN];
	size_t pmk_len;
	time_t expiration;
	int akmp; /* WPA_KEY_MGMT_* */
	u8 aa[ETH_ALEN];
};

struct rsn_pmksa_candidate {
	struct rsn_pmksa_candidate *next;
	u8 bssid[ETH_ALEN];
	int priority;
};


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


struct wpa_blacklist {
	struct wpa_blacklist *next;
	u8 bssid[ETH_ALEN];
};


struct wpa_supplicant {
	struct wpa_supplicant *head;
	struct wpa_supplicant *next;
	struct l2_packet_data *l2;
	unsigned char own_addr[ETH_ALEN];
	char ifname[100];
#ifdef CONFIG_XSUPPLICANT_IFACE
	int dot1x_s; /* socket for connection to Xsupplicant */
	int ext_pmk_received; /* 1 = PMK was received from Xsupplicant */
#endif /* CONFIG_XSUPPLICANT_IFACE */

	u8 pmk[PMK_LEN];
	size_t pmk_len;
	u8 snonce[WPA_NONCE_LEN];
	u8 anonce[WPA_NONCE_LEN]; /* ANonce from the last 1/4 msg */
	struct wpa_ptk ptk, tptk;
	int ptk_set, tptk_set;
	int renew_snonce;
	char *confname;
	struct wpa_config *conf;
	u8 request_counter[WPA_REPLAY_COUNTER_LEN];
	int countermeasures;
	time_t last_michael_mic_error;
	u8 rx_replay_counter[WPA_REPLAY_COUNTER_LEN];
	int rx_replay_counter_set;
	u8 bssid[ETH_ALEN];
	int reassociate; /* reassociation requested */
	struct wpa_ssid *current_ssid;
	u8 *ap_wpa_ie, *ap_rsn_ie;
	size_t ap_wpa_ie_len, ap_rsn_ie_len;
	u8 *assoc_wpa_ie;
	size_t assoc_wpa_ie_len;

	/* Selected configuration (based on Beacon/ProbeResp WPA IE) */
	int proto;
	int pairwise_cipher;
	int group_cipher;
	int key_mgmt;

	void *drv_priv; /* private data used by driver_ops */

	struct wpa_ssid *prev_scan_ssid; /* previously scanned SSID;
					  * NULL = not yet initialized (start
					  * with broadcast SSID)
					  * BROADCAST_SSID_SCAN = broadcast
					  * SSID was used in the previous scan
					  */
#define BROADCAST_SSID_SCAN ((struct wpa_ssid *) 1)

	struct wpa_scan_result *scan_results;
	int num_scan_results;

	struct wpa_driver_ops *driver;
	int interface_removed; /* whether the network interface has been
				* removed */
	struct eapol_sm *eapol;

	int ctrl_sock; /* UNIX domain socket for control interface or -1 if
			* not used */
	struct wpa_ctrl_dst *ctrl_dst;

	enum {
		WPA_DISCONNECTED, WPA_SCANNING, WPA_ASSOCIATING,
		WPA_ASSOCIATED, WPA_4WAY_HANDSHAKE, WPA_GROUP_HANDSHAKE,
		WPA_COMPLETED
	} wpa_state;

	struct rsn_pmksa_cache *pmksa; /* PMKSA cache */
	int pmksa_count; /* number of entries in PMKSA cache */
	struct rsn_pmksa_cache *cur_pmksa; /* current PMKSA entry */
	struct rsn_pmksa_candidate *pmksa_candidates;

	struct l2_packet_data *l2_preauth;
	u8 preauth_bssid[ETH_ALEN]; /* current RSN pre-auth peer or
				     * 00:00:00:00:00:00 if no pre-auth is
				     * in progress */
	struct eapol_sm *preauth_eapol;

	int eapol_received; /* number of EAPOL packets received after the
			     * previous association event */

	u8 *imsi;
	size_t imsi_len;
	struct scard_data *scard;

	unsigned char last_eapol_src[ETH_ALEN];

	int keys_cleared;

	struct wpa_blacklist *blacklist;

#ifdef EAPOL_TEST
	u8 radius_identifier;
	struct radius_msg *last_recv_radius;
	struct in_addr own_ip_addr;
	struct radius_client_data *radius;

	/* RADIUS Authentication and Accounting servers in priority order */
	struct hostapd_radius_server *auth_servers, *auth_server;
	int num_auth_servers;
	struct hostapd_radius_server *acct_servers, *acct_server;
	int num_acct_servers;

	int radius_retry_primary_interval;
	int radius_acct_interim_interval;

	u8 *last_eap_radius; /* last received EAP Response from Authentication
			      * Server */
	size_t last_eap_radius_len;

	u8 authenticator_pmk[PMK_LEN];
	size_t authenticator_pmk_len;
	int radius_access_accept_received;
	int radius_access_reject_received;
	int auth_timed_out;

	u8 *eap_identity;
	size_t eap_identity_len;
#endif /* EAPOL_TEST */
};


/* wpa_supplicant.c */
void wpa_supplicant_scan(void *eloop_ctx, void *timeout_ctx);

void wpa_supplicant_req_scan(struct wpa_supplicant *wpa_s, int sec, int usec);

void wpa_supplicant_cancel_scan(struct wpa_supplicant *wpa_s);

void wpa_supplicant_disassociate(struct wpa_supplicant *wpa_s,
				 int reason_code);
void wpa_supplicant_deauthenticate(struct wpa_supplicant *wpa_s,
				   int reason_code);

void wpa_supplicant_req_auth_timeout(struct wpa_supplicant *wpa_s,
				     int sec, int usec);

void wpa_supplicant_cancel_auth_timeout(struct wpa_supplicant *wpa_s);

int wpa_supplicant_reload_configuration(struct wpa_supplicant *wpa_s);

int wpa_supplicant_get_beacon_ie(struct wpa_supplicant *wpa_s);


/* wpa.c */
void wpa_supplicant_key_request(struct wpa_supplicant *wpa_s,
				int error, int pairwise);

struct wpa_ie_data {
	int proto;
	int pairwise_cipher;
	int group_cipher;
	int key_mgmt;
	int capabilities;
	int num_pmkid;
	u8 *pmkid;
};

int wpa_parse_wpa_ie(struct wpa_supplicant *wpa_s, u8 *wpa_ie,
		     size_t wpa_ie_len, struct wpa_ie_data *data);

int wpa_gen_wpa_ie(struct wpa_supplicant *wpa_s, u8 *wpa_ie);

void wpa_supplicant_rx_eapol(void *ctx, unsigned char *src_addr,
			     unsigned char *buf, size_t len);

struct wpa_ssid * wpa_supplicant_get_ssid(struct wpa_supplicant *wpa_s);

void pmksa_cache_free(struct wpa_supplicant *wpa_s);
struct rsn_pmksa_cache * pmksa_cache_get(struct wpa_supplicant *wpa_s,
					 u8 *aa, u8 *pmkid);
int pmksa_cache_list(struct wpa_supplicant *wpa_s, char *buf, size_t len);
void pmksa_candidate_free(struct wpa_supplicant *wpa_s);

int wpa_get_mib(struct wpa_supplicant *wpa_s, char *buf, size_t buflen);

struct wpa_scan_result;
#ifdef IEEE8021X_EAPOL
int rsn_preauth_init(struct wpa_supplicant *wpa_s, u8 *dst);
void rsn_preauth_deinit(struct wpa_supplicant *wpa_s);
void rsn_preauth_scan_results(struct wpa_supplicant *wpa_s,
			      struct wpa_scan_result *results, int count);
void pmksa_candidate_add(struct wpa_supplicant *wpa_s, const u8 *bssid,
			 int prio);
#else /* IEEE8021X_EAPOL */
static inline int rsn_preauth_init(struct wpa_supplicant *wpa_s, u8 *dst)
{
	return -1;
}

static inline void rsn_preauth_deinit(struct wpa_supplicant *wpa_s)
{
}
static inline void rsn_preauth_scan_results(struct wpa_supplicant *wpa_s,
					    struct wpa_scan_result *results,
					    int count)
{
}

static inline void pmksa_candidate_add(struct wpa_supplicant *wpa_s,
				       const u8 *bssid,
				       int prio)
{
}
#endif /* IEEE8021X_EAPOL */

void wpa_supplicant_notify_eapol_done(void *ctx);

/**
 * wpa_eapol_send - send IEEE 802.1X EAPOL packet to the Authenticator
 * @ctx: pointer to wpa_supplicant data
 * @type: IEEE 802.1X packet type (IEEE802_1X_TYPE_*)
 * @buf: EAPOL payload (after IEEE 802.1X header)
 * @len: EAPOL payload length
 *
 * This function adds Ethernet and IEEE 802.1X header and sends the EAPOL frame
 * to the current Authenticator or in case of pre-authentication, to the peer
 * of the authentication.
 */
int wpa_eapol_send(void *ctx, int type, u8 *buf, size_t len);
int wpa_eapol_send_preauth(void *ctx, int type, u8 *buf, size_t len);


/* driver_ops */
static inline void * wpa_drv_init(struct wpa_supplicant *wpa_s,
				  const char *ifname)
{
	if (wpa_s->driver->init) {
		return wpa_s->driver->init(wpa_s, ifname);
	}
	return NULL;
}

static inline void wpa_drv_deinit(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->deinit)
		wpa_s->driver->deinit(wpa_s->drv_priv);
}

static inline int wpa_drv_set_drop_unencrypted(struct wpa_supplicant *wpa_s,
					       int enabled)
{
	if (wpa_s->driver->set_drop_unencrypted) {
		return wpa_s->driver->set_drop_unencrypted(wpa_s->drv_priv,
							   enabled);
	}
	return -1;
}

static inline int wpa_drv_set_countermeasures(struct wpa_supplicant *wpa_s,
					      int enabled)
{
	if (wpa_s->driver->set_countermeasures) {
		return wpa_s->driver->set_countermeasures(wpa_s->drv_priv,
							  enabled);
	}
	return -1;
}

static inline int wpa_drv_set_auth_alg(struct wpa_supplicant *wpa_s,
				       int auth_alg)
{
	if (wpa_s->driver->set_auth_alg) {
		return wpa_s->driver->set_auth_alg(wpa_s->drv_priv,
						   auth_alg);
	}
	return -1;
}

static inline int wpa_drv_set_wpa(struct wpa_supplicant *wpa_s, int enabled)
{
	if (wpa_s->driver->set_wpa) {
		return wpa_s->driver->set_wpa(wpa_s->drv_priv, enabled);
	}
	return -1;
}

static inline int wpa_drv_associate(struct wpa_supplicant *wpa_s,
				    struct wpa_driver_associate_params *params)
{
	if (wpa_s->driver->associate) {
		return wpa_s->driver->associate(wpa_s->drv_priv, params);
	}
	return -1;
}

static inline int wpa_drv_scan(struct wpa_supplicant *wpa_s, const u8 *ssid,
			       size_t ssid_len)
{
	if (wpa_s->driver->scan) {
		return wpa_s->driver->scan(wpa_s->drv_priv, ssid, ssid_len);
	}
	return -1;
}

static inline int wpa_drv_get_scan_results(struct wpa_supplicant *wpa_s,
					   struct wpa_scan_result *results,
					   size_t max_size)
{
	if (wpa_s->driver->get_scan_results) {
		return wpa_s->driver->get_scan_results(wpa_s->drv_priv,
						       results, max_size);
	}
	return -1;
}

static inline int wpa_drv_get_bssid(struct wpa_supplicant *wpa_s, u8 *bssid)
{
	if (wpa_s->driver->get_bssid) {
		return wpa_s->driver->get_bssid(wpa_s->drv_priv, bssid);
	}
	return -1;
}

static inline int wpa_drv_get_ssid(struct wpa_supplicant *wpa_s, u8 *ssid)
{
	if (wpa_s->driver->get_ssid) {
		return wpa_s->driver->get_ssid(wpa_s->drv_priv, ssid);
	}
	return -1;
}

static inline int wpa_drv_set_key(struct wpa_supplicant *wpa_s, wpa_alg alg,
				   const u8 *addr, int key_idx, int set_tx,
				   const u8 *seq, size_t seq_len,
				   const u8 *key, size_t key_len)
{
	if (wpa_s->driver->set_key) {
		return wpa_s->driver->set_key(wpa_s->drv_priv, alg, addr,
					      key_idx, set_tx, seq, seq_len,
					      key, key_len);
	}
	return -1;
}

static inline int wpa_drv_deauthenticate(struct wpa_supplicant *wpa_s,
					 const u8 *addr, int reason_code)
{
	if (wpa_s->driver->deauthenticate) {
		return wpa_s->driver->deauthenticate(wpa_s->drv_priv, addr,
						     reason_code);
	}
	return -1;
}

static inline int wpa_drv_disassociate(struct wpa_supplicant *wpa_s,
				       const u8 *addr, int reason_code)
{
	if (wpa_s->driver->disassociate) {
		return wpa_s->driver->disassociate(wpa_s->drv_priv, addr,
						   reason_code);
	}
	return -1;
}

static inline int wpa_drv_add_pmkid(struct wpa_supplicant *wpa_s,
				    const u8 *bssid, const u8 *pmkid)
{
	if (wpa_s->driver->add_pmkid) {
		return wpa_s->driver->add_pmkid(wpa_s->drv_priv, bssid, pmkid);
	}
	return -1;
}

static inline int wpa_drv_remove_pmkid(struct wpa_supplicant *wpa_s,
				       const u8 *bssid, const u8 *pmkid)
{
	if (wpa_s->driver->remove_pmkid) {
		return wpa_s->driver->remove_pmkid(wpa_s->drv_priv, bssid,
						   pmkid);
	}
	return -1;
}

static inline int wpa_drv_flush_pmkid(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->flush_pmkid) {
		return wpa_s->driver->flush_pmkid(wpa_s->drv_priv);
	}
	return -1;
}

static inline int wpa_drv_get_capa(struct wpa_supplicant *wpa_s,
				   struct wpa_driver_capa *capa)
{
	if (wpa_s->driver->get_capa) {
		return wpa_s->driver->get_capa(wpa_s->drv_priv, capa);
	}
	return -1;
}

static inline void wpa_drv_poll(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->poll) {
		wpa_s->driver->poll(wpa_s->drv_priv);
	}
}

static inline const char * wpa_drv_get_ifname(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->get_ifname) {
		return wpa_s->driver->get_ifname(wpa_s->drv_priv);
	}
	return NULL;
}

static inline const u8 * wpa_drv_get_mac_addr(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->get_mac_addr) {
		return wpa_s->driver->get_mac_addr(wpa_s->drv_priv);
	}
	return NULL;
}

#endif /* WPA_SUPPLICANT_I_H */
