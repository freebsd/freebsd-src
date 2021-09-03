/*
 * DPP functionality shared between hostapd and wpa_supplicant
 * Copyright (c) 2017, Qualcomm Atheros, Inc.
 * Copyright (c) 2018-2020, The Linux Foundation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef DPP_H
#define DPP_H

#ifdef CONFIG_DPP
#include <openssl/x509.h>

#include "utils/list.h"
#include "common/wpa_common.h"
#include "crypto/sha256.h"

struct crypto_ecdh;
struct hostapd_ip_addr;
struct dpp_global;
struct json_token;
struct dpp_reconfig_id;

#ifdef CONFIG_TESTING_OPTIONS
#define DPP_VERSION (dpp_version_override)
extern int dpp_version_override;
#else /* CONFIG_TESTING_OPTIONS */
#ifdef CONFIG_DPP2
#define DPP_VERSION 2
#else
#define DPP_VERSION 1
#endif
#endif /* CONFIG_TESTING_OPTIONS */

#define DPP_HDR_LEN (4 + 2) /* OUI, OUI Type, Crypto Suite, DPP frame type */
#define DPP_TCP_PORT 8908

enum dpp_public_action_frame_type {
	DPP_PA_AUTHENTICATION_REQ = 0,
	DPP_PA_AUTHENTICATION_RESP = 1,
	DPP_PA_AUTHENTICATION_CONF = 2,
	DPP_PA_PEER_DISCOVERY_REQ = 5,
	DPP_PA_PEER_DISCOVERY_RESP = 6,
	DPP_PA_PKEX_EXCHANGE_REQ = 7,
	DPP_PA_PKEX_EXCHANGE_RESP = 8,
	DPP_PA_PKEX_COMMIT_REVEAL_REQ = 9,
	DPP_PA_PKEX_COMMIT_REVEAL_RESP = 10,
	DPP_PA_CONFIGURATION_RESULT = 11,
	DPP_PA_CONNECTION_STATUS_RESULT = 12,
	DPP_PA_PRESENCE_ANNOUNCEMENT = 13,
	DPP_PA_RECONFIG_ANNOUNCEMENT = 14,
	DPP_PA_RECONFIG_AUTH_REQ = 15,
	DPP_PA_RECONFIG_AUTH_RESP = 16,
	DPP_PA_RECONFIG_AUTH_CONF = 17,
};

enum dpp_attribute_id {
	DPP_ATTR_STATUS = 0x1000,
	DPP_ATTR_I_BOOTSTRAP_KEY_HASH = 0x1001,
	DPP_ATTR_R_BOOTSTRAP_KEY_HASH = 0x1002,
	DPP_ATTR_I_PROTOCOL_KEY = 0x1003,
	DPP_ATTR_WRAPPED_DATA = 0x1004,
	DPP_ATTR_I_NONCE = 0x1005,
	DPP_ATTR_I_CAPABILITIES = 0x1006,
	DPP_ATTR_R_NONCE = 0x1007,
	DPP_ATTR_R_CAPABILITIES = 0x1008,
	DPP_ATTR_R_PROTOCOL_KEY = 0x1009,
	DPP_ATTR_I_AUTH_TAG = 0x100A,
	DPP_ATTR_R_AUTH_TAG = 0x100B,
	DPP_ATTR_CONFIG_OBJ = 0x100C,
	DPP_ATTR_CONNECTOR = 0x100D,
	DPP_ATTR_CONFIG_ATTR_OBJ = 0x100E,
	DPP_ATTR_BOOTSTRAP_KEY = 0x100F,
	DPP_ATTR_OWN_NET_NK_HASH = 0x1011,
	DPP_ATTR_FINITE_CYCLIC_GROUP = 0x1012,
	DPP_ATTR_ENCRYPTED_KEY = 0x1013,
	DPP_ATTR_ENROLLEE_NONCE = 0x1014,
	DPP_ATTR_CODE_IDENTIFIER = 0x1015,
	DPP_ATTR_TRANSACTION_ID = 0x1016,
	DPP_ATTR_BOOTSTRAP_INFO = 0x1017,
	DPP_ATTR_CHANNEL = 0x1018,
	DPP_ATTR_PROTOCOL_VERSION = 0x1019,
	DPP_ATTR_ENVELOPED_DATA = 0x101A,
	DPP_ATTR_SEND_CONN_STATUS = 0x101B,
	DPP_ATTR_CONN_STATUS = 0x101C,
	DPP_ATTR_RECONFIG_FLAGS = 0x101D,
	DPP_ATTR_C_SIGN_KEY_HASH = 0x101E,
	DPP_ATTR_CSR_ATTR_REQ = 0x101F,
	DPP_ATTR_A_NONCE = 0x1020,
	DPP_ATTR_E_PRIME_ID = 0x1021,
	DPP_ATTR_CONFIGURATOR_NONCE = 0x1022,
};

enum dpp_status_error {
	DPP_STATUS_OK = 0,
	DPP_STATUS_NOT_COMPATIBLE = 1,
	DPP_STATUS_AUTH_FAILURE = 2,
	DPP_STATUS_UNWRAP_FAILURE = 3,
	DPP_STATUS_BAD_GROUP = 4,
	DPP_STATUS_CONFIGURE_FAILURE = 5,
	DPP_STATUS_RESPONSE_PENDING = 6,
	DPP_STATUS_INVALID_CONNECTOR = 7,
	DPP_STATUS_NO_MATCH = 8,
	DPP_STATUS_CONFIG_REJECTED = 9,
	DPP_STATUS_NO_AP = 10,
	DPP_STATUS_CONFIGURE_PENDING = 11,
	DPP_STATUS_CSR_NEEDED = 12,
	DPP_STATUS_CSR_BAD = 13,
};

/* DPP Reconfig Flags object - connectorKey values */
enum dpp_connector_key {
	DPP_CONFIG_REUSEKEY = 0,
	DPP_CONFIG_REPLACEKEY = 1,
};

#define DPP_CAPAB_ENROLLEE BIT(0)
#define DPP_CAPAB_CONFIGURATOR BIT(1)
#define DPP_CAPAB_ROLE_MASK (BIT(0) | BIT(1))

#define DPP_BOOTSTRAP_MAX_FREQ 30
#define DPP_MAX_NONCE_LEN 32
#define DPP_MAX_HASH_LEN 64
#define DPP_MAX_SHARED_SECRET_LEN 66
#define DPP_CP_LEN 64

struct dpp_curve_params {
	const char *name;
	size_t hash_len;
	size_t aes_siv_key_len;
	size_t nonce_len;
	size_t prime_len;
	const char *jwk_crv;
	u16 ike_group;
	const char *jws_alg;
};

enum dpp_bootstrap_type {
	DPP_BOOTSTRAP_QR_CODE,
	DPP_BOOTSTRAP_PKEX,
	DPP_BOOTSTRAP_NFC_URI,
};

struct dpp_bootstrap_info {
	struct dl_list list;
	unsigned int id;
	enum dpp_bootstrap_type type;
	char *uri;
	u8 mac_addr[ETH_ALEN];
	char *chan;
	char *info;
	char *pk;
	unsigned int freq[DPP_BOOTSTRAP_MAX_FREQ];
	unsigned int num_freq;
	bool channels_listed;
	u8 version;
	int own;
	EVP_PKEY *pubkey;
	u8 pubkey_hash[SHA256_MAC_LEN];
	u8 pubkey_hash_chirp[SHA256_MAC_LEN];
	const struct dpp_curve_params *curve;
	unsigned int pkex_t; /* number of failures before dpp_pkex
			      * instantiation */
	int nfc_negotiated; /* whether this has been used in NFC negotiated
			     * connection handover */
	char *configurator_params;
};

#define PKEX_COUNTER_T_LIMIT 5

struct dpp_pkex {
	void *msg_ctx;
	unsigned int initiator:1;
	unsigned int exchange_done:1;
	unsigned int failed:1;
	struct dpp_bootstrap_info *own_bi;
	u8 own_mac[ETH_ALEN];
	u8 peer_mac[ETH_ALEN];
	char *identifier;
	char *code;
	EVP_PKEY *x;
	EVP_PKEY *y;
	u8 Mx[DPP_MAX_SHARED_SECRET_LEN];
	u8 Nx[DPP_MAX_SHARED_SECRET_LEN];
	u8 z[DPP_MAX_HASH_LEN];
	EVP_PKEY *peer_bootstrap_key;
	struct wpabuf *exchange_req;
	struct wpabuf *exchange_resp;
	unsigned int t; /* number of failures on code use */
	unsigned int exch_req_wait_time;
	unsigned int exch_req_tries;
	unsigned int freq;
};

enum dpp_akm {
	DPP_AKM_UNKNOWN,
	DPP_AKM_DPP,
	DPP_AKM_PSK,
	DPP_AKM_SAE,
	DPP_AKM_PSK_SAE,
	DPP_AKM_SAE_DPP,
	DPP_AKM_PSK_SAE_DPP,
	DPP_AKM_DOT1X,
};

enum dpp_netrole {
	DPP_NETROLE_STA,
	DPP_NETROLE_AP,
	DPP_NETROLE_CONFIGURATOR,
};

struct dpp_configuration {
	u8 ssid[32];
	size_t ssid_len;
	int ssid_charset;
	enum dpp_akm akm;
	enum dpp_netrole netrole;

	/* For DPP configuration (connector) */
	os_time_t netaccesskey_expiry;

	/* TODO: groups */
	char *group_id;

	/* For legacy configuration */
	char *passphrase;
	u8 psk[32];
	int psk_set;

	char *csrattrs;
};

struct dpp_asymmetric_key {
	struct dpp_asymmetric_key *next;
	EVP_PKEY *csign;
	EVP_PKEY *pp_key;
	char *config_template;
	char *connector_template;
};

#define DPP_MAX_CONF_OBJ 10

struct dpp_authentication {
	struct dpp_global *global;
	void *msg_ctx;
	u8 peer_version;
	const struct dpp_curve_params *curve;
	struct dpp_bootstrap_info *peer_bi;
	struct dpp_bootstrap_info *own_bi;
	struct dpp_bootstrap_info *tmp_own_bi;
	struct dpp_bootstrap_info *tmp_peer_bi;
	u8 waiting_pubkey_hash[SHA256_MAC_LEN];
	int response_pending;
	int reconfig;
	enum dpp_connector_key reconfig_connector_key;
	enum dpp_status_error auth_resp_status;
	enum dpp_status_error conf_resp_status;
	enum dpp_status_error force_conf_resp_status;
	u8 peer_mac_addr[ETH_ALEN];
	u8 i_nonce[DPP_MAX_NONCE_LEN];
	u8 r_nonce[DPP_MAX_NONCE_LEN];
	u8 e_nonce[DPP_MAX_NONCE_LEN];
	u8 c_nonce[DPP_MAX_NONCE_LEN];
	u8 i_capab;
	u8 r_capab;
	enum dpp_netrole e_netrole;
	EVP_PKEY *own_protocol_key;
	EVP_PKEY *peer_protocol_key;
	EVP_PKEY *reconfig_old_protocol_key;
	struct wpabuf *req_msg;
	struct wpabuf *resp_msg;
	struct wpabuf *reconfig_req_msg;
	struct wpabuf *reconfig_resp_msg;
	/* Intersection of possible frequencies for initiating DPP
	 * Authentication exchange */
	unsigned int freq[DPP_BOOTSTRAP_MAX_FREQ];
	unsigned int num_freq, freq_idx;
	unsigned int curr_freq;
	unsigned int neg_freq;
	unsigned int num_freq_iters;
	size_t secret_len;
	u8 Mx[DPP_MAX_SHARED_SECRET_LEN];
	size_t Mx_len;
	u8 Nx[DPP_MAX_SHARED_SECRET_LEN];
	size_t Nx_len;
	u8 Lx[DPP_MAX_SHARED_SECRET_LEN];
	size_t Lx_len;
	u8 k1[DPP_MAX_HASH_LEN];
	u8 k2[DPP_MAX_HASH_LEN];
	u8 ke[DPP_MAX_HASH_LEN];
	u8 bk[DPP_MAX_HASH_LEN];
	int initiator;
	int waiting_auth_resp;
	int waiting_auth_conf;
	int auth_req_ack;
	unsigned int auth_resp_tries;
	u8 allowed_roles;
	int configurator;
	int remove_on_tx_status;
	int connect_on_tx_status;
	int waiting_conf_result;
	int waiting_conn_status_result;
	int auth_success;
	bool reconfig_success;
	struct wpabuf *conf_req;
	const struct wpabuf *conf_resp; /* owned by GAS server */
	struct wpabuf *conf_resp_tcp;
	struct dpp_configuration *conf_ap;
	struct dpp_configuration *conf2_ap;
	struct dpp_configuration *conf_sta;
	struct dpp_configuration *conf2_sta;
	int provision_configurator;
	struct dpp_configurator *conf;
	struct dpp_config_obj {
		char *connector; /* received signedConnector */
		u8 ssid[SSID_MAX_LEN];
		u8 ssid_len;
		int ssid_charset;
		char passphrase[64];
		u8 psk[PMK_LEN];
		int psk_set;
		enum dpp_akm akm;
		struct wpabuf *c_sign_key;
		struct wpabuf *certbag;
		struct wpabuf *certs;
		struct wpabuf *cacert;
		char *server_name;
		struct wpabuf *pp_key;
	} conf_obj[DPP_MAX_CONF_OBJ];
	unsigned int num_conf_obj;
	struct dpp_asymmetric_key *conf_key_pkg;
	struct wpabuf *net_access_key;
	os_time_t net_access_key_expiry;
	int send_conn_status;
	int conn_status_requested;
	int akm_use_selector;
	int configurator_set;
	u8 transaction_id;
	u8 *csrattrs;
	size_t csrattrs_len;
	bool waiting_csr;
	struct wpabuf *csr;
	struct wpabuf *priv_key; /* DER-encoded private key used for csr */
	bool waiting_cert;
	char *trusted_eap_server_name;
	struct wpabuf *cacert;
	struct wpabuf *certbag;
	void *cert_resp_ctx;
	void *gas_server_ctx;
#ifdef CONFIG_TESTING_OPTIONS
	char *config_obj_override;
	char *discovery_override;
	char *groups_override;
	unsigned int ignore_netaccesskey_mismatch:1;
#endif /* CONFIG_TESTING_OPTIONS */
};

struct dpp_configurator {
	struct dl_list list;
	unsigned int id;
	int own;
	EVP_PKEY *csign;
	u8 kid_hash[SHA256_MAC_LEN];
	char *kid;
	const struct dpp_curve_params *curve;
	char *connector; /* own Connector for reconfiguration */
	EVP_PKEY *connector_key;
	EVP_PKEY *pp_key;
};

struct dpp_introduction {
	u8 pmkid[PMKID_LEN];
	u8 pmk[PMK_LEN_MAX];
	size_t pmk_len;
};

struct dpp_relay_config {
	const struct hostapd_ip_addr *ipaddr;
	const u8 *pkhash;

	void *msg_ctx;
	void *cb_ctx;
	void (*tx)(void *ctx, const u8 *addr, unsigned int freq, const u8 *msg,
		   size_t len);
	void (*gas_resp_tx)(void *ctx, const u8 *addr, u8 dialog_token, int prot,
			    struct wpabuf *buf);
};

struct dpp_controller_config {
	const char *configurator_params;
	int tcp_port;
	u8 allowed_roles;
	int qr_mutual;
	enum dpp_netrole netrole;
	void *msg_ctx;
	void *cb_ctx;
	int (*process_conf_obj)(void *ctx, struct dpp_authentication *auth);
};

#ifdef CONFIG_TESTING_OPTIONS
enum dpp_test_behavior {
	DPP_TEST_DISABLED = 0,
	DPP_TEST_AFTER_WRAPPED_DATA_AUTH_REQ = 1,
	DPP_TEST_AFTER_WRAPPED_DATA_AUTH_RESP = 2,
	DPP_TEST_AFTER_WRAPPED_DATA_AUTH_CONF = 3,
	DPP_TEST_AFTER_WRAPPED_DATA_PKEX_CR_REQ = 4,
	DPP_TEST_AFTER_WRAPPED_DATA_PKEX_CR_RESP = 5,
	DPP_TEST_AFTER_WRAPPED_DATA_CONF_REQ = 6,
	DPP_TEST_AFTER_WRAPPED_DATA_CONF_RESP = 7,
	DPP_TEST_ZERO_I_CAPAB = 8,
	DPP_TEST_ZERO_R_CAPAB = 9,
	DPP_TEST_NO_R_BOOTSTRAP_KEY_HASH_AUTH_REQ = 10,
	DPP_TEST_NO_I_BOOTSTRAP_KEY_HASH_AUTH_REQ = 11,
	DPP_TEST_NO_I_PROTO_KEY_AUTH_REQ = 12,
	DPP_TEST_NO_I_NONCE_AUTH_REQ = 13,
	DPP_TEST_NO_I_CAPAB_AUTH_REQ = 14,
	DPP_TEST_NO_WRAPPED_DATA_AUTH_REQ = 15,
	DPP_TEST_NO_STATUS_AUTH_RESP = 16,
	DPP_TEST_NO_R_BOOTSTRAP_KEY_HASH_AUTH_RESP = 17,
	DPP_TEST_NO_I_BOOTSTRAP_KEY_HASH_AUTH_RESP = 18,
	DPP_TEST_NO_R_PROTO_KEY_AUTH_RESP = 19,
	DPP_TEST_NO_R_NONCE_AUTH_RESP = 20,
	DPP_TEST_NO_I_NONCE_AUTH_RESP = 21,
	DPP_TEST_NO_R_CAPAB_AUTH_RESP = 22,
	DPP_TEST_NO_R_AUTH_AUTH_RESP = 23,
	DPP_TEST_NO_WRAPPED_DATA_AUTH_RESP = 24,
	DPP_TEST_NO_STATUS_AUTH_CONF = 25,
	DPP_TEST_NO_R_BOOTSTRAP_KEY_HASH_AUTH_CONF = 26,
	DPP_TEST_NO_I_BOOTSTRAP_KEY_HASH_AUTH_CONF = 27,
	DPP_TEST_NO_I_AUTH_AUTH_CONF = 28,
	DPP_TEST_NO_WRAPPED_DATA_AUTH_CONF = 29,
	DPP_TEST_I_NONCE_MISMATCH_AUTH_RESP = 30,
	DPP_TEST_INCOMPATIBLE_R_CAPAB_AUTH_RESP = 31,
	DPP_TEST_R_AUTH_MISMATCH_AUTH_RESP = 32,
	DPP_TEST_I_AUTH_MISMATCH_AUTH_CONF = 33,
	DPP_TEST_NO_FINITE_CYCLIC_GROUP_PKEX_EXCHANGE_REQ = 34,
	DPP_TEST_NO_ENCRYPTED_KEY_PKEX_EXCHANGE_REQ = 35,
	DPP_TEST_NO_STATUS_PKEX_EXCHANGE_RESP = 36,
	DPP_TEST_NO_ENCRYPTED_KEY_PKEX_EXCHANGE_RESP = 37,
	DPP_TEST_NO_BOOTSTRAP_KEY_PKEX_CR_REQ = 38,
	DPP_TEST_NO_I_AUTH_TAG_PKEX_CR_REQ = 39,
	DPP_TEST_NO_WRAPPED_DATA_PKEX_CR_REQ = 40,
	DPP_TEST_NO_BOOTSTRAP_KEY_PKEX_CR_RESP = 41,
	DPP_TEST_NO_R_AUTH_TAG_PKEX_CR_RESP = 42,
	DPP_TEST_NO_WRAPPED_DATA_PKEX_CR_RESP = 43,
	DPP_TEST_INVALID_ENCRYPTED_KEY_PKEX_EXCHANGE_REQ = 44,
	DPP_TEST_INVALID_ENCRYPTED_KEY_PKEX_EXCHANGE_RESP = 45,
	DPP_TEST_INVALID_STATUS_PKEX_EXCHANGE_RESP = 46,
	DPP_TEST_INVALID_BOOTSTRAP_KEY_PKEX_CR_REQ = 47,
	DPP_TEST_INVALID_BOOTSTRAP_KEY_PKEX_CR_RESP = 48,
	DPP_TEST_I_AUTH_TAG_MISMATCH_PKEX_CR_REQ = 49,
	DPP_TEST_R_AUTH_TAG_MISMATCH_PKEX_CR_RESP = 50,
	DPP_TEST_NO_E_NONCE_CONF_REQ = 51,
	DPP_TEST_NO_CONFIG_ATTR_OBJ_CONF_REQ = 52,
	DPP_TEST_NO_WRAPPED_DATA_CONF_REQ = 53,
	DPP_TEST_NO_E_NONCE_CONF_RESP = 54,
	DPP_TEST_NO_CONFIG_OBJ_CONF_RESP = 55,
	DPP_TEST_NO_STATUS_CONF_RESP = 56,
	DPP_TEST_NO_WRAPPED_DATA_CONF_RESP = 57,
	DPP_TEST_INVALID_STATUS_CONF_RESP = 58,
	DPP_TEST_E_NONCE_MISMATCH_CONF_RESP = 59,
	DPP_TEST_NO_TRANSACTION_ID_PEER_DISC_REQ = 60,
	DPP_TEST_NO_CONNECTOR_PEER_DISC_REQ = 61,
	DPP_TEST_NO_TRANSACTION_ID_PEER_DISC_RESP = 62,
	DPP_TEST_NO_STATUS_PEER_DISC_RESP = 63,
	DPP_TEST_NO_CONNECTOR_PEER_DISC_RESP = 64,
	DPP_TEST_AUTH_RESP_IN_PLACE_OF_CONF = 65,
	DPP_TEST_INVALID_I_PROTO_KEY_AUTH_REQ = 66,
	DPP_TEST_INVALID_R_PROTO_KEY_AUTH_RESP = 67,
	DPP_TEST_INVALID_R_BOOTSTRAP_KEY_HASH_AUTH_REQ = 68,
	DPP_TEST_INVALID_I_BOOTSTRAP_KEY_HASH_AUTH_REQ = 69,
	DPP_TEST_INVALID_R_BOOTSTRAP_KEY_HASH_AUTH_RESP = 70,
	DPP_TEST_INVALID_I_BOOTSTRAP_KEY_HASH_AUTH_RESP = 71,
	DPP_TEST_INVALID_R_BOOTSTRAP_KEY_HASH_AUTH_CONF = 72,
	DPP_TEST_INVALID_I_BOOTSTRAP_KEY_HASH_AUTH_CONF = 73,
	DPP_TEST_INVALID_STATUS_AUTH_RESP = 74,
	DPP_TEST_INVALID_STATUS_AUTH_CONF = 75,
	DPP_TEST_INVALID_CONFIG_ATTR_OBJ_CONF_REQ = 76,
	DPP_TEST_INVALID_TRANSACTION_ID_PEER_DISC_RESP = 77,
	DPP_TEST_INVALID_STATUS_PEER_DISC_RESP = 78,
	DPP_TEST_INVALID_CONNECTOR_PEER_DISC_RESP = 79,
	DPP_TEST_INVALID_CONNECTOR_PEER_DISC_REQ = 80,
	DPP_TEST_INVALID_I_NONCE_AUTH_REQ = 81,
	DPP_TEST_INVALID_TRANSACTION_ID_PEER_DISC_REQ = 82,
	DPP_TEST_INVALID_E_NONCE_CONF_REQ = 83,
	DPP_TEST_STOP_AT_PKEX_EXCHANGE_RESP = 84,
	DPP_TEST_STOP_AT_PKEX_CR_REQ = 85,
	DPP_TEST_STOP_AT_PKEX_CR_RESP = 86,
	DPP_TEST_STOP_AT_AUTH_REQ = 87,
	DPP_TEST_STOP_AT_AUTH_RESP = 88,
	DPP_TEST_STOP_AT_AUTH_CONF = 89,
	DPP_TEST_STOP_AT_CONF_REQ = 90,
	DPP_TEST_REJECT_CONFIG = 91,
};

extern enum dpp_test_behavior dpp_test;
extern u8 dpp_pkex_own_mac_override[ETH_ALEN];
extern u8 dpp_pkex_peer_mac_override[ETH_ALEN];
extern u8 dpp_pkex_ephemeral_key_override[600];
extern size_t dpp_pkex_ephemeral_key_override_len;
extern u8 dpp_protocol_key_override[600];
extern size_t dpp_protocol_key_override_len;
extern u8 dpp_nonce_override[DPP_MAX_NONCE_LEN];
extern size_t dpp_nonce_override_len;
#endif /* CONFIG_TESTING_OPTIONS */

void dpp_bootstrap_info_free(struct dpp_bootstrap_info *info);
const char * dpp_bootstrap_type_txt(enum dpp_bootstrap_type type);
int dpp_parse_uri_chan_list(struct dpp_bootstrap_info *bi,
			    const char *chan_list);
int dpp_parse_uri_mac(struct dpp_bootstrap_info *bi, const char *mac);
int dpp_parse_uri_info(struct dpp_bootstrap_info *bi, const char *info);
int dpp_nfc_update_bi(struct dpp_bootstrap_info *own_bi,
		      struct dpp_bootstrap_info *peer_bi);
struct dpp_authentication *
dpp_alloc_auth(struct dpp_global *dpp, void *msg_ctx);
struct hostapd_hw_modes;
struct dpp_authentication * dpp_auth_init(struct dpp_global *dpp, void *msg_ctx,
					  struct dpp_bootstrap_info *peer_bi,
					  struct dpp_bootstrap_info *own_bi,
					  u8 dpp_allowed_roles,
					  unsigned int neg_freq,
					  struct hostapd_hw_modes *own_modes,
					  u16 num_modes);
struct dpp_authentication *
dpp_auth_req_rx(struct dpp_global *dpp, void *msg_ctx, u8 dpp_allowed_roles,
			int qr_mutual, struct dpp_bootstrap_info *peer_bi,
		struct dpp_bootstrap_info *own_bi,
		unsigned int freq, const u8 *hdr, const u8 *attr_start,
		size_t attr_len);
struct wpabuf *
dpp_auth_resp_rx(struct dpp_authentication *auth, const u8 *hdr,
		 const u8 *attr_start, size_t attr_len);
struct wpabuf * dpp_build_conf_req(struct dpp_authentication *auth,
				   const char *json);
struct wpabuf * dpp_build_conf_req_helper(struct dpp_authentication *auth,
					  const char *name,
					  enum dpp_netrole netrole,
					  const char *mud_url, int *opclasses);
int dpp_auth_conf_rx(struct dpp_authentication *auth, const u8 *hdr,
		     const u8 *attr_start, size_t attr_len);
int dpp_notify_new_qr_code(struct dpp_authentication *auth,
			   struct dpp_bootstrap_info *peer_bi);
struct dpp_configuration * dpp_configuration_alloc(const char *type);
int dpp_akm_psk(enum dpp_akm akm);
int dpp_akm_sae(enum dpp_akm akm);
int dpp_akm_legacy(enum dpp_akm akm);
int dpp_akm_dpp(enum dpp_akm akm);
int dpp_akm_ver2(enum dpp_akm akm);
int dpp_configuration_valid(const struct dpp_configuration *conf);
void dpp_configuration_free(struct dpp_configuration *conf);
int dpp_set_configurator(struct dpp_authentication *auth, const char *cmd);
void dpp_auth_deinit(struct dpp_authentication *auth);
struct wpabuf *
dpp_build_conf_resp(struct dpp_authentication *auth, const u8 *e_nonce,
		    u16 e_nonce_len, enum dpp_netrole netrole,
		    bool cert_req);
struct wpabuf *
dpp_conf_req_rx(struct dpp_authentication *auth, const u8 *attr_start,
		size_t attr_len);
int dpp_conf_resp_rx(struct dpp_authentication *auth,
		     const struct wpabuf *resp);
enum dpp_status_error dpp_conf_result_rx(struct dpp_authentication *auth,
					 const u8 *hdr,
					 const u8 *attr_start, size_t attr_len);
struct wpabuf * dpp_build_conf_result(struct dpp_authentication *auth,
				      enum dpp_status_error status);
enum dpp_status_error dpp_conn_status_result_rx(struct dpp_authentication *auth,
						const u8 *hdr,
						const u8 *attr_start,
						size_t attr_len,
						u8 *ssid, size_t *ssid_len,
						char **channel_list);
struct wpabuf * dpp_build_conn_status_result(struct dpp_authentication *auth,
					     enum dpp_status_error result,
					     const u8 *ssid, size_t ssid_len,
					     const char *channel_list);
struct wpabuf * dpp_alloc_msg(enum dpp_public_action_frame_type type,
			      size_t len);
const u8 * dpp_get_attr(const u8 *buf, size_t len, u16 req_id, u16 *ret_len);
int dpp_check_attrs(const u8 *buf, size_t len);
int dpp_key_expired(const char *timestamp, os_time_t *expiry);
const char * dpp_akm_str(enum dpp_akm akm);
const char * dpp_akm_selector_str(enum dpp_akm akm);
int dpp_configurator_get_key(const struct dpp_configurator *conf, char *buf,
			     size_t buflen);
void dpp_configurator_free(struct dpp_configurator *conf);
int dpp_configurator_own_config(struct dpp_authentication *auth,
				const char *curve, int ap);
enum dpp_status_error
dpp_peer_intro(struct dpp_introduction *intro, const char *own_connector,
	       const u8 *net_access_key, size_t net_access_key_len,
	       const u8 *csign_key, size_t csign_key_len,
	       const u8 *peer_connector, size_t peer_connector_len,
	       os_time_t *expiry);
struct dpp_pkex * dpp_pkex_init(void *msg_ctx, struct dpp_bootstrap_info *bi,
				const u8 *own_mac,
				const char *identifier,
				const char *code);
struct dpp_pkex * dpp_pkex_rx_exchange_req(void *msg_ctx,
					   struct dpp_bootstrap_info *bi,
					   const u8 *own_mac,
					   const u8 *peer_mac,
					   const char *identifier,
					   const char *code,
					   const u8 *buf, size_t len);
struct wpabuf * dpp_pkex_rx_exchange_resp(struct dpp_pkex *pkex,
					  const u8 *peer_mac,
					  const u8 *buf, size_t len);
struct wpabuf * dpp_pkex_rx_commit_reveal_req(struct dpp_pkex *pkex,
					      const u8 *hdr,
					      const u8 *buf, size_t len);
int dpp_pkex_rx_commit_reveal_resp(struct dpp_pkex *pkex, const u8 *hdr,
				   const u8 *buf, size_t len);
void dpp_pkex_free(struct dpp_pkex *pkex);

char * dpp_corrupt_connector_signature(const char *connector);


struct dpp_pfs {
	struct crypto_ecdh *ecdh;
	const struct dpp_curve_params *curve;
	struct wpabuf *ie;
	struct wpabuf *secret;
};

struct dpp_pfs * dpp_pfs_init(const u8 *net_access_key,
			      size_t net_access_key_len);
int dpp_pfs_process(struct dpp_pfs *pfs, const u8 *peer_ie, size_t peer_ie_len);
void dpp_pfs_free(struct dpp_pfs *pfs);

struct wpabuf * dpp_build_csr(struct dpp_authentication *auth,
			      const char *name);
struct wpabuf * dpp_pkcs7_certs(const struct wpabuf *pkcs7);
int dpp_validate_csr(struct dpp_authentication *auth, const struct wpabuf *csr);

struct dpp_bootstrap_info * dpp_add_qr_code(struct dpp_global *dpp,
					    const char *uri);
struct dpp_bootstrap_info * dpp_add_nfc_uri(struct dpp_global *dpp,
					    const char *uri);
int dpp_bootstrap_gen(struct dpp_global *dpp, const char *cmd);
struct dpp_bootstrap_info *
dpp_bootstrap_get_id(struct dpp_global *dpp, unsigned int id);
int dpp_bootstrap_remove(struct dpp_global *dpp, const char *id);
struct dpp_bootstrap_info *
dpp_pkex_finish(struct dpp_global *dpp, struct dpp_pkex *pkex, const u8 *peer,
		unsigned int freq);
const char * dpp_bootstrap_get_uri(struct dpp_global *dpp, unsigned int id);
int dpp_bootstrap_info(struct dpp_global *dpp, int id,
		       char *reply, int reply_size);
int dpp_bootstrap_set(struct dpp_global *dpp, int id, const char *params);
void dpp_bootstrap_find_pair(struct dpp_global *dpp, const u8 *i_bootstrap,
			     const u8 *r_bootstrap,
			     struct dpp_bootstrap_info **own_bi,
			     struct dpp_bootstrap_info **peer_bi);
struct dpp_bootstrap_info * dpp_bootstrap_find_chirp(struct dpp_global *dpp,
						     const u8 *hash);
int dpp_configurator_add(struct dpp_global *dpp, const char *cmd);
int dpp_configurator_remove(struct dpp_global *dpp, const char *id);
int dpp_configurator_get_key_id(struct dpp_global *dpp, unsigned int id,
				char *buf, size_t buflen);
int dpp_configurator_from_backup(struct dpp_global *dpp,
				 struct dpp_asymmetric_key *key);
struct dpp_configurator * dpp_configurator_find_kid(struct dpp_global *dpp,
						    const u8 *kid);
int dpp_relay_add_controller(struct dpp_global *dpp,
			     struct dpp_relay_config *config);
int dpp_relay_rx_action(struct dpp_global *dpp, const u8 *src, const u8 *hdr,
			const u8 *buf, size_t len, unsigned int freq,
			const u8 *i_bootstrap, const u8 *r_bootstrap,
			void *cb_ctx);
int dpp_relay_rx_gas_req(struct dpp_global *dpp, const u8 *src, const u8 *data,
			 size_t data_len);
int dpp_controller_start(struct dpp_global *dpp,
			 struct dpp_controller_config *config);
void dpp_controller_stop(struct dpp_global *dpp);
struct dpp_authentication * dpp_controller_get_auth(struct dpp_global *dpp,
						    unsigned int id);
void dpp_controller_new_qr_code(struct dpp_global *dpp,
				struct dpp_bootstrap_info *bi);
int dpp_tcp_init(struct dpp_global *dpp, struct dpp_authentication *auth,
		 const struct hostapd_ip_addr *addr, int port,
		 const char *name, enum dpp_netrole netrole, void *msg_ctx,
		 void *cb_ctx,
		 int (*process_conf_obj)(void *ctx,
					 struct dpp_authentication *auth));

struct wpabuf * dpp_build_presence_announcement(struct dpp_bootstrap_info *bi);
void dpp_notify_chirp_received(void *msg_ctx, int id, const u8 *src,
				unsigned int freq, const u8 *hash);

struct dpp_global_config {
	void *cb_ctx;
	void (*remove_bi)(void *ctx, struct dpp_bootstrap_info *bi);
};

struct dpp_global * dpp_global_init(struct dpp_global_config *config);
void dpp_global_clear(struct dpp_global *dpp);
void dpp_global_deinit(struct dpp_global *dpp);

/* dpp_reconfig.c */

struct wpabuf * dpp_build_reconfig_announcement(const u8 *csign_key,
						size_t csign_key_len,
						const u8 *net_access_key,
						size_t net_access_key_len,
						struct dpp_reconfig_id *id);
struct dpp_authentication *
dpp_reconfig_init(struct dpp_global *dpp, void *msg_ctx,
		  struct dpp_configurator *conf, unsigned int freq, u16 group,
		  const u8 *a_nonce_attr, size_t a_nonce_len,
		  const u8 *e_id_attr, size_t e_id_len);
struct dpp_authentication *
dpp_reconfig_auth_req_rx(struct dpp_global *dpp, void *msg_ctx,
			 const char *own_connector,
			 const u8 *net_access_key, size_t net_access_key_len,
			 const u8 *csign_key, size_t csign_key_len,
			 unsigned int freq, const u8 *hdr,
			 const u8 *attr_start, size_t attr_len);
struct wpabuf *
dpp_reconfig_auth_resp_rx(struct dpp_authentication *auth, const u8 *hdr,
			  const u8 *attr_start, size_t attr_len);
int dpp_reconfig_auth_conf_rx(struct dpp_authentication *auth, const u8 *hdr,
			      const u8 *attr_start, size_t attr_len);

struct dpp_reconfig_id * dpp_gen_reconfig_id(const u8 *csign_key,
					     size_t csign_key_len,
					     const u8 *pp_key,
					     size_t pp_key_len);
int dpp_update_reconfig_id(struct dpp_reconfig_id *id);
void dpp_free_reconfig_id(struct dpp_reconfig_id *id);

#endif /* CONFIG_DPP */
#endif /* DPP_H */
