/*
 * DPP module internal definitions
 * Copyright (c) 2017, Qualcomm Atheros, Inc.
 * Copyright (c) 2018-2020, The Linux Foundation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef DPP_I_H
#define DPP_I_H

#ifdef CONFIG_DPP

struct dpp_global {
	void *msg_ctx;
	struct dl_list bootstrap; /* struct dpp_bootstrap_info */
	struct dl_list configurator; /* struct dpp_configurator */
#ifdef CONFIG_DPP2
	struct dl_list controllers; /* struct dpp_relay_controller */
	struct dpp_controller *controller;
	struct dl_list tcp_init; /* struct dpp_connection */
	void *cb_ctx;
	int (*process_conf_obj)(void *ctx, struct dpp_authentication *auth);
	void (*remove_bi)(void *ctx, struct dpp_bootstrap_info *bi);
#endif /* CONFIG_DPP2 */
};

/* dpp.c */

void dpp_build_attr_status(struct wpabuf *msg, enum dpp_status_error status);
void dpp_build_attr_r_bootstrap_key_hash(struct wpabuf *msg, const u8 *hash);
unsigned int dpp_next_id(struct dpp_global *dpp);
struct wpabuf * dpp_build_conn_status(enum dpp_status_error result,
				      const u8 *ssid, size_t ssid_len,
				      const char *channel_list);
struct json_token * dpp_parse_own_connector(const char *own_connector);
int dpp_connector_match_groups(struct json_token *own_root,
			       struct json_token *peer_root, bool reconfig);
int dpp_build_jwk(struct wpabuf *buf, const char *name,
		  struct crypto_ec_key *key, const char *kid,
		  const struct dpp_curve_params *curve);
struct crypto_ec_key * dpp_parse_jwk(struct json_token *jwk,
				     const struct dpp_curve_params **key_curve);
int dpp_prepare_channel_list(struct dpp_authentication *auth,
			     unsigned int neg_freq,
			     struct hostapd_hw_modes *own_modes, u16 num_modes);
void dpp_auth_fail(struct dpp_authentication *auth, const char *txt);
int dpp_gen_uri(struct dpp_bootstrap_info *bi);
void dpp_write_adv_proto(struct wpabuf *buf);
void dpp_write_gas_query(struct wpabuf *buf, struct wpabuf *query);

/* dpp_backup.c */

void dpp_free_asymmetric_key(struct dpp_asymmetric_key *key);
struct wpabuf * dpp_build_enveloped_data(struct dpp_authentication *auth);
int dpp_conf_resp_env_data(struct dpp_authentication *auth,
			   const u8 *env_data, size_t env_data_len);

/* dpp_crypto.c */

struct dpp_signed_connector_info {
	unsigned char *payload;
	size_t payload_len;
};

enum dpp_status_error
dpp_process_signed_connector(struct dpp_signed_connector_info *info,
			     struct crypto_ec_key *csign_pub,
			     const char *connector);
enum dpp_status_error
dpp_check_signed_connector(struct dpp_signed_connector_info *info,
			   const u8 *csign_key, size_t csign_key_len,
			   const u8 *peer_connector, size_t peer_connector_len);
const struct dpp_curve_params * dpp_get_curve_name(const char *name);
const struct dpp_curve_params * dpp_get_curve_jwk_crv(const char *name);
const struct dpp_curve_params * dpp_get_curve_ike_group(u16 group);
int dpp_bi_pubkey_hash(struct dpp_bootstrap_info *bi,
		       const u8 *data, size_t data_len);
struct crypto_ec_key * dpp_set_pubkey_point(struct crypto_ec_key *group_key,
					    const u8 *buf, size_t len);
int dpp_hkdf_expand(size_t hash_len, const u8 *secret, size_t secret_len,
		    const char *label, u8 *out, size_t outlen);
int dpp_hmac_vector(size_t hash_len, const u8 *key, size_t key_len,
		    size_t num_elem, const u8 *addr[], const size_t *len,
		    u8 *mac);
int dpp_ecdh(struct crypto_ec_key *own, struct crypto_ec_key *peer,
	     u8 *secret, size_t *secret_len);
void dpp_debug_print_key(const char *title, struct crypto_ec_key *key);
int dpp_pbkdf2(size_t hash_len, const u8 *password, size_t password_len,
	       const u8 *salt, size_t salt_len, unsigned int iterations,
	       u8 *buf, size_t buflen);
int dpp_get_subject_public_key(struct dpp_bootstrap_info *bi,
			       const u8 *data, size_t data_len);
int dpp_bootstrap_key_hash(struct dpp_bootstrap_info *bi);
int dpp_keygen(struct dpp_bootstrap_info *bi, const char *curve,
	       const u8 *privkey, size_t privkey_len);
struct crypto_ec_key * dpp_set_keypair(const struct dpp_curve_params **curve,
				       const u8 *privkey, size_t privkey_len);
struct crypto_ec_key * dpp_gen_keypair(const struct dpp_curve_params *curve);
int dpp_derive_k1(const u8 *Mx, size_t Mx_len, u8 *k1, unsigned int hash_len);
int dpp_derive_k2(const u8 *Nx, size_t Nx_len, u8 *k2, unsigned int hash_len);
int dpp_derive_bk_ke(struct dpp_authentication *auth);
int dpp_gen_r_auth(struct dpp_authentication *auth, u8 *r_auth);
int dpp_gen_i_auth(struct dpp_authentication *auth, u8 *i_auth);
int dpp_auth_derive_l_responder(struct dpp_authentication *auth);
int dpp_auth_derive_l_initiator(struct dpp_authentication *auth);
int dpp_derive_pmk(const u8 *Nx, size_t Nx_len, u8 *pmk, unsigned int hash_len);
int dpp_derive_pmkid(const struct dpp_curve_params *curve,
		     struct crypto_ec_key *own_key,
		     struct crypto_ec_key *peer_key, u8 *pmkid);
struct crypto_ec_point *
dpp_pkex_derive_Qi(const struct dpp_curve_params *curve, const u8 *mac_init,
		   const char *code, const char *identifier,
		   struct crypto_ec **ret_ec);
struct crypto_ec_point *
dpp_pkex_derive_Qr(const struct dpp_curve_params *curve, const u8 *mac_resp,
		   const char *code, const char *identifier,
		   struct crypto_ec **ret_ec);
int dpp_pkex_derive_z(const u8 *mac_init, const u8 *mac_resp,
		      u8 ver_init, u8 ver_resp,
		      const u8 *Mx, size_t Mx_len,
		      const u8 *Nx, size_t Nx_len,
		      const char *code,
		      const u8 *Kx, size_t Kx_len,
		      u8 *z, unsigned int hash_len);
int dpp_reconfig_derive_ke_responder(struct dpp_authentication *auth,
				     const u8 *net_access_key,
				     size_t net_access_key_len,
				     struct json_token *peer_net_access_key);
int dpp_reconfig_derive_ke_initiator(struct dpp_authentication *auth,
				     const u8 *r_proto, u16 r_proto_len,
				     struct json_token *net_access_key);
struct crypto_ec_point * dpp_decrypt_e_id(struct crypto_ec_key *ppkey,
					  struct crypto_ec_key *a_nonce,
					  struct crypto_ec_key *e_prime_id);
char * dpp_sign_connector(struct dpp_configurator *conf,
			  const struct wpabuf *dppcon);
int dpp_test_gen_invalid_key(struct wpabuf *msg,
			     const struct dpp_curve_params *curve);

struct dpp_reconfig_id {
	struct crypto_ec *ec;
	struct crypto_ec_point *e_id; /* E-id */
	struct crypto_ec_key *csign;
	struct crypto_ec_key *a_nonce; /* A-NONCE */
	struct crypto_ec_key *e_prime_id; /* E'-id */
	struct crypto_ec_key *pp_key;
};

/* dpp_tcp.c */

void dpp_controller_conn_status_result_wait_timeout(void *eloop_ctx,
						    void *timeout_ctx);
void dpp_tcp_init_flush(struct dpp_global *dpp);
void dpp_relay_flush_controllers(struct dpp_global *dpp);

#endif /* CONFIG_DPP */
#endif /* DPP_I_H */
