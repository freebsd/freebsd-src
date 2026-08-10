/*
 * Authentication server setup
 * Copyright (c) 2002-2009, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "crypto/crypto.h"
#include "crypto/tls.h"
#include "eap_server/eap.h"
#include "eap_server/eap_sim_db.h"
#include "eapol_auth/eapol_auth_sm.h"
#include "radius/radius.h"
#include "radius/radius_server.h"
#include "hostapd.h"
#include "ap_config.h"
#include "sta_info.h"
#include "authsrv.h"


#if defined(EAP_SERVER_SIM) || defined(EAP_SERVER_AKA)
#define EAP_SIM_DB
#endif /* EAP_SERVER_SIM || EAP_SERVER_AKA */


#ifdef EAP_SIM_DB
static int hostapd_sim_db_cb_sta(struct hostapd_data *hapd,
				 struct sta_info *sta, void *ctx)
{
	if (eapol_auth_eap_pending_cb(sta->eapol_sm, ctx) == 0)
		return 1;
	return 0;
}


static void hostapd_sim_db_cb(void *ctx, void *session_ctx)
{
	struct hostapd_data *hapd = ctx;
	if (ap_for_each_sta(hapd, hostapd_sim_db_cb_sta, session_ctx) == 0) {
#ifdef RADIUS_SERVER
		radius_server_eap_pending_cb(hapd->radius_srv, session_ctx);
#endif /* RADIUS_SERVER */
	}
}
#endif /* EAP_SIM_DB */


#ifdef RADIUS_SERVER

static int hostapd_radius_get_eap_user(void *ctx, const u8 *identity,
				       size_t identity_len, int phase2,
				       struct eap_user *user)
{
	const struct hostapd_eap_user *eap_user;
	int i;
	int rv = -1;

	eap_user = hostapd_get_eap_user(ctx, identity, identity_len, phase2);
	if (eap_user == NULL)
		goto out;

	if (user == NULL)
		return 0;

	os_memset(user, 0, sizeof(*user));
	for (i = 0; i < EAP_MAX_METHODS; i++) {
		user->methods[i].vendor = eap_user->methods[i].vendor;
		user->methods[i].method = eap_user->methods[i].method;
	}

	if (eap_user->password) {
		user->password = os_memdup(eap_user->password,
					   eap_user->password_len);
		if (user->password == NULL)
			goto out;
		user->password_len = eap_user->password_len;
		user->password_hash = eap_user->password_hash;
		if (eap_user->salt && eap_user->salt_len) {
			user->salt = os_memdup(eap_user->salt,
					       eap_user->salt_len);
			if (!user->salt)
				goto out;
			user->salt_len = eap_user->salt_len;
		}
	}
	user->force_version = eap_user->force_version;
	user->macacl = eap_user->macacl;
	user->ttls_auth = eap_user->ttls_auth;
	user->accept_attr = eap_user->accept_attr;
	user->t_c_timestamp = eap_user->t_c_timestamp;
	rv = 0;

out:
	if (rv)
		wpa_printf(MSG_DEBUG, "%s: Failed to find user", __func__);

	return rv;
}


/**
 * hostapd_radius_log_acct_req - Callback for logging received RADIUS
 * accounting requests
 * @ctx: Context (struct hostapd_data)
 * @msg: Received RADIUS accounting request
 * @status_type: Status type from the message (parsed Acct-Status-Type
 * attribute)
 * Returns: 0 on success, -1 on failure
 */
static int hostapd_radius_log_acct_req(void *ctx, struct radius_msg *msg,
				       u32 status_type)
{
	char nas_id[RADIUS_MAX_ATTR_LEN + 1] = "";
	char session_id[RADIUS_MAX_ATTR_LEN + 1] = "";
	char username[RADIUS_MAX_ATTR_LEN + 1] = "";
	char calling_station_id[3 * ETH_ALEN] = "";
	u32 session_time = 0, terminate_cause = 0,
		bytes_in = 0, bytes_out = 0,
		packets_in = 0, packets_out = 0,
		gigawords_in = 0, gigawords_out = 0;
	unsigned long long total_bytes_in = 0, total_bytes_out = 0;

	/* Parse NAS identification (required by RFC 2866, section 4.1) */
	if (radius_msg_get_attr(msg, RADIUS_ATTR_NAS_IDENTIFIER, (u8 *) nas_id,
				sizeof(nas_id) - 1))
		nas_id[0] = '\0';

	/* Process Accounting-On and Accounting-Off messages separately */
	if (status_type == RADIUS_ACCT_STATUS_TYPE_ACCOUNTING_ON ||
	    status_type == RADIUS_ACCT_STATUS_TYPE_ACCOUNTING_OFF) {
		wpa_printf(MSG_INFO, "RADIUS ACCT: NAS='%s' status='%s'",
			   nas_id,
			   status_type == RADIUS_ACCT_STATUS_TYPE_ACCOUNTING_ON
			   ? "Accounting-On" : "Accounting-Off");
		return 0;
	}

	/* Parse session ID (required by RFC 2866, section 5.5) */
	if (radius_msg_get_attr(msg, RADIUS_ATTR_ACCT_SESSION_ID,
				(u8 *) session_id,
				sizeof(session_id) - 1) == 0) {
		wpa_printf(MSG_DEBUG,
			   "RADIUS ACCT: request doesn't include session ID");
		return -1;
	}

	/* Parse user name */
	radius_msg_get_attr(msg, RADIUS_ATTR_USER_NAME, (u8 *) username,
			    sizeof(username) - 1);

	/* Parse device identifier */
	radius_msg_get_attr(msg, RADIUS_ATTR_CALLING_STATION_ID,
			    (u8 *) calling_station_id,
			    sizeof(calling_station_id) - 1);

	switch (status_type) {
	case RADIUS_ACCT_STATUS_TYPE_START:
		wpa_printf(MSG_INFO,
			   "RADIUS ACCT: NAS='%s' session='%s' status='Accounting-Start' station='%s' username='%s'",
			   nas_id, session_id, calling_station_id, username);
		break;
	case RADIUS_ACCT_STATUS_TYPE_STOP:
	case RADIUS_ACCT_STATUS_TYPE_INTERIM_UPDATE:
		/* Parse counters */
		radius_msg_get_attr_int32(msg, RADIUS_ATTR_ACCT_SESSION_TIME,
					  &session_time);
		radius_msg_get_attr_int32(msg,
					  RADIUS_ATTR_ACCT_TERMINATE_CAUSE,
					  &terminate_cause);
		radius_msg_get_attr_int32(msg, RADIUS_ATTR_ACCT_INPUT_OCTETS,
					  &bytes_in);
		radius_msg_get_attr_int32(msg, RADIUS_ATTR_ACCT_OUTPUT_OCTETS,
					  &bytes_out);
		radius_msg_get_attr_int32(msg, RADIUS_ATTR_ACCT_INPUT_PACKETS,
					  &packets_in);
		radius_msg_get_attr_int32(msg, RADIUS_ATTR_ACCT_OUTPUT_PACKETS,
					  &packets_out);
		radius_msg_get_attr_int32(msg,
					  RADIUS_ATTR_ACCT_INPUT_GIGAWORDS,
					  &gigawords_in);
		radius_msg_get_attr_int32(msg,
					  RADIUS_ATTR_ACCT_OUTPUT_GIGAWORDS,
					  &gigawords_out);

		/* RFC 2869, section 5.1 and 5.2 */
		total_bytes_in = ((u64) gigawords_in << 32) + bytes_in;
		total_bytes_out = ((u64) gigawords_out << 32) + bytes_out;

		wpa_printf(MSG_INFO,
			   "RADIUS ACCT: NAS='%s' session='%s' status='%s' station='%s' username='%s' session_time=%u term_cause=%u pck_in=%u pck_out=%u bytes_in=%llu bytes_out=%llu",
			   nas_id, session_id,
			   status_type == RADIUS_ACCT_STATUS_TYPE_STOP ?
			   "Accounting-Stop" : "Accounting-Interim-Update",
			   calling_station_id, username, session_time,
			   terminate_cause, packets_in, packets_out,
			   total_bytes_in, total_bytes_out);
		break;
	default:
		wpa_printf(MSG_DEBUG,
			   "RADIUS ACCT: Unknown request status type %u",
			   status_type);
		return -1;
	}

	return 0;
}


static int hostapd_setup_radius_srv(struct hostapd_data *hapd)
{
	struct radius_server_conf srv;
	struct hostapd_bss_config *conf = hapd->conf;

#ifdef CONFIG_IEEE80211BE
	if (!hostapd_mld_is_first_bss(hapd)) {
		struct hostapd_data *first;

		wpa_printf(MSG_DEBUG,
			   "MLD: Using RADIUS server of the first BSS");

		first = hostapd_mld_get_first_bss(hapd);
		if (!first)
			return -1;
		hapd->radius_srv = first->radius_srv;
		return 0;
	}
#endif /* CONFIG_IEEE80211BE */

	os_memset(&srv, 0, sizeof(srv));
	srv.client_file = conf->radius_server_clients;
	srv.auth_port = conf->radius_server_auth_port;
	srv.acct_port = conf->radius_server_acct_port;
	srv.conf_ctx = hapd;
	srv.ipv6 = conf->radius_server_ipv6;
	srv.get_eap_user = hostapd_radius_get_eap_user;
	if (conf->radius_server_acct_log)
		srv.acct_req_cb = hostapd_radius_log_acct_req;
	srv.eap_req_id_text = conf->eap_req_id_text;
	srv.eap_req_id_text_len = conf->eap_req_id_text_len;
	srv.sqlite_file = conf->eap_user_sqlite;
#ifdef CONFIG_RADIUS_TEST
	srv.dump_msk_file = conf->dump_msk_file;
#endif /* CONFIG_RADIUS_TEST */
#ifdef CONFIG_HS20
	srv.t_c_server_url = conf->t_c_server_url;
#endif /* CONFIG_HS20 */
	srv.erp_domain = conf->erp_domain;
	srv.eap_cfg = hapd->eap_cfg;

	hapd->radius_srv = radius_server_init(&srv);
	if (hapd->radius_srv == NULL) {
		wpa_printf(MSG_ERROR, "RADIUS server initialization failed.");
		return -1;
	}

	return 0;
}

#endif /* RADIUS_SERVER */


#ifdef EAP_TLS_FUNCS
static void authsrv_tls_event(void *ctx, enum tls_event ev,
			      union tls_event_data *data)
{
	switch (ev) {
	case TLS_CERT_CHAIN_SUCCESS:
		wpa_printf(MSG_DEBUG, "authsrv: remote certificate verification success");
		break;
	case TLS_CERT_CHAIN_FAILURE:
		wpa_printf(MSG_INFO, "authsrv: certificate chain failure: reason=%d depth=%d subject='%s' err='%s'",
			   data->cert_fail.reason,
			   data->cert_fail.depth,
			   data->cert_fail.subject,
			   data->cert_fail.reason_txt);
		break;
	case TLS_PEER_CERTIFICATE:
		wpa_printf(MSG_DEBUG, "authsrv: peer certificate: depth=%d serial_num=%s subject=%s",
			   data->peer_cert.depth,
			   data->peer_cert.serial_num ? data->peer_cert.serial_num : "N/A",
			   data->peer_cert.subject);
		break;
	case TLS_ALERT:
		if (data->alert.is_local)
			wpa_printf(MSG_DEBUG, "authsrv: local TLS alert: %s",
				   data->alert.description);
		else
			wpa_printf(MSG_DEBUG, "authsrv: remote TLS alert: %s",
				   data->alert.description);
		break;
	case TLS_UNSAFE_RENEGOTIATION_DISABLED:
		/* Not applicable to TLS server */
		break;
	}
}
#endif /* EAP_TLS_FUNCS */


static struct eap_config * authsrv_eap_config(struct hostapd_data *hapd)
{
	struct eap_config *cfg;

	cfg = os_zalloc(sizeof(*cfg));
	if (!cfg)
		return NULL;

	cfg->eap_server = hapd->conf->eap_server;
	cfg->ssl_ctx = hapd->ssl_ctx;
	cfg->msg_ctx = hapd->msg_ctx;
	cfg->eap_sim_db_priv = hapd->eap_sim_db_priv;
	cfg->tls_session_lifetime = hapd->conf->tls_session_lifetime;
	cfg->tls_flags = hapd->conf->tls_flags;
	cfg->max_auth_rounds = hapd->conf->max_auth_rounds;
	cfg->max_auth_rounds_short = hapd->conf->max_auth_rounds_short;
	if (hapd->conf->pac_opaque_encr_key)
		cfg->pac_opaque_encr_key =
			os_memdup(hapd->conf->pac_opaque_encr_key, 16);
	if (hapd->conf->eap_fast_a_id) {
		cfg->eap_fast_a_id = os_memdup(hapd->conf->eap_fast_a_id,
					       hapd->conf->eap_fast_a_id_len);
		cfg->eap_fast_a_id_len = hapd->conf->eap_fast_a_id_len;
	}
	if (hapd->conf->eap_fast_a_id_info)
		cfg->eap_fast_a_id_info =
			os_strdup(hapd->conf->eap_fast_a_id_info);
	cfg->eap_fast_prov = hapd->conf->eap_fast_prov;
	cfg->pac_key_lifetime = hapd->conf->pac_key_lifetime;
	cfg->pac_key_refresh_time = hapd->conf->pac_key_refresh_time;
	cfg->eap_teap_auth = hapd->conf->eap_teap_auth;
	cfg->eap_teap_separate_result = hapd->conf->eap_teap_separate_result;
	cfg->eap_teap_id = hapd->conf->eap_teap_id;
	cfg->eap_teap_method_sequence = hapd->conf->eap_teap_method_sequence;
	cfg->eap_sim_aka_result_ind = hapd->conf->eap_sim_aka_result_ind;
	cfg->eap_sim_id = hapd->conf->eap_sim_id;
	cfg->imsi_privacy_key = hapd->imsi_privacy_key;
	cfg->eap_sim_aka_fast_reauth_limit =
		hapd->conf->eap_sim_aka_fast_reauth_limit;
	cfg->tnc = hapd->conf->tnc;
	cfg->wps = hapd->wps;
	cfg->fragment_size = hapd->conf->fragment_size;
	cfg->pwd_group = hapd->conf->pwd_group;
	cfg->pbc_in_m1 = hapd->conf->pbc_in_m1;
	if (hapd->conf->server_id) {
		cfg->server_id = (u8 *) os_strdup(hapd->conf->server_id);
		cfg->server_id_len = os_strlen(hapd->conf->server_id);
	} else {
		cfg->server_id = (u8 *) os_strdup("hostapd");
		cfg->server_id_len = 7;
	}
	cfg->erp = hapd->conf->eap_server_erp;
#ifdef CONFIG_TESTING_OPTIONS
	cfg->skip_prot_success = hapd->conf->eap_skip_prot_success;
#endif /* CONFIG_TESTING_OPTIONS */

	return cfg;
}


int authsrv_init(struct hostapd_data *hapd)
{
#ifdef CONFIG_IEEE80211BE
	if (!hostapd_mld_is_first_bss(hapd)) {
		struct hostapd_data *first;

		first = hostapd_mld_get_first_bss(hapd);
		if (!first)
			return -1;

		if (!first->eap_cfg) {
			wpa_printf(MSG_DEBUG,
				   "MLD: First BSS auth_serv does not exist. Init on its behalf");

			if (authsrv_init(first))
				return -1;
		}

		wpa_printf(MSG_DEBUG, "MLD: Using auth_serv of the first BSS");

#ifdef EAP_TLS_FUNCS
		hapd->ssl_ctx = first->ssl_ctx;
#endif /* EAP_TLS_FUNCS */
		hapd->eap_cfg = first->eap_cfg;
#ifdef EAP_SIM_DB
		hapd->eap_sim_db_priv = first->eap_sim_db_priv;
#endif /* EAP_SIM_DB */
		return 0;
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef EAP_TLS_FUNCS
	if (hapd->conf->eap_server &&
	    (hapd->conf->ca_cert || hapd->conf->server_cert ||
	     hapd->conf->private_key || hapd->conf->dh_file ||
	     hapd->conf->server_cert2 || hapd->conf->private_key2)) {
		struct tls_config conf;
		struct tls_connection_params params;

		os_memset(&conf, 0, sizeof(conf));
		conf.tls_session_lifetime = hapd->conf->tls_session_lifetime;
		if (hapd->conf->crl_reload_interval > 0 &&
		    hapd->conf->check_crl <= 0) {
			wpa_printf(MSG_INFO,
				   "Cannot enable CRL reload functionality - it depends on check_crl being set");
		} else if (hapd->conf->crl_reload_interval > 0) {
			conf.crl_reload_interval =
				hapd->conf->crl_reload_interval;
			wpa_printf(MSG_INFO,
				   "Enabled CRL reload functionality");
		}
		conf.tls_flags = hapd->conf->tls_flags;
		conf.event_cb = authsrv_tls_event;
		conf.cb_ctx = hapd;
		hapd->ssl_ctx = tls_init(&conf);
		if (hapd->ssl_ctx == NULL) {
			wpa_printf(MSG_ERROR, "Failed to initialize TLS");
			authsrv_deinit(hapd);
			return -1;
		}

		os_memset(&params, 0, sizeof(params));
		params.ca_cert = hapd->conf->ca_cert;
		params.client_cert = hapd->conf->server_cert;
		params.client_cert2 = hapd->conf->server_cert2;
		params.private_key = hapd->conf->private_key;
		params.private_key2 = hapd->conf->private_key2;
		params.private_key_passwd = hapd->conf->private_key_passwd;
		params.private_key_passwd2 = hapd->conf->private_key_passwd2;
		params.dh_file = hapd->conf->dh_file;
		params.openssl_ciphers = hapd->conf->openssl_ciphers;
		params.openssl_ecdh_curves = hapd->conf->openssl_ecdh_curves;
		params.ocsp_stapling_response =
			hapd->conf->ocsp_stapling_response;
		params.ocsp_stapling_response_multi =
			hapd->conf->ocsp_stapling_response_multi;
		params.check_cert_subject = hapd->conf->check_cert_subject;

		if (tls_global_set_params(hapd->ssl_ctx, &params)) {
			wpa_printf(MSG_ERROR, "Failed to set TLS parameters");
			authsrv_deinit(hapd);
			return -1;
		}

		if (tls_global_set_verify(hapd->ssl_ctx,
					  hapd->conf->check_crl,
					  hapd->conf->check_crl_strict)) {
			wpa_printf(MSG_ERROR, "Failed to enable check_crl");
			authsrv_deinit(hapd);
			return -1;
		}
	}
#endif /* EAP_TLS_FUNCS */

#ifdef CRYPTO_RSA_OAEP_SHA256
	crypto_rsa_key_free(hapd->imsi_privacy_key);
	hapd->imsi_privacy_key = NULL;
	if (hapd->conf->imsi_privacy_key) {
		hapd->imsi_privacy_key = crypto_rsa_key_read(
			hapd->conf->imsi_privacy_key, true);
		if (!hapd->imsi_privacy_key) {
			wpa_printf(MSG_ERROR,
				   "Failed to read/parse IMSI privacy key %s",
				   hapd->conf->imsi_privacy_key);
			authsrv_deinit(hapd);
			return -1;
		}
	}
#endif /* CRYPTO_RSA_OAEP_SHA256 */

#ifdef EAP_SIM_DB
	if (hapd->conf->eap_sim_db) {
		hapd->eap_sim_db_priv =
			eap_sim_db_init(hapd->conf->eap_sim_db,
					hapd->conf->eap_sim_db_timeout,
					hostapd_sim_db_cb, hapd);
		if (hapd->eap_sim_db_priv == NULL) {
			wpa_printf(MSG_ERROR, "Failed to initialize EAP-SIM "
				   "database interface");
			authsrv_deinit(hapd);
			return -1;
		}
	}
#endif /* EAP_SIM_DB */

	hapd->eap_cfg = authsrv_eap_config(hapd);
	if (!hapd->eap_cfg) {
		wpa_printf(MSG_ERROR,
			   "Failed to build EAP server configuration");
		authsrv_deinit(hapd);
		return -1;
	}

#ifdef RADIUS_SERVER
	if (hapd->conf->radius_server_clients &&
	    hostapd_setup_radius_srv(hapd))
		return -1;
#endif /* RADIUS_SERVER */

	return 0;
}


void authsrv_deinit(struct hostapd_data *hapd)
{
#ifdef CONFIG_IEEE80211BE
	if (!hostapd_mld_is_first_bss(hapd)) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Deinit auth_serv of a non-first BSS");

		hapd->radius_srv = NULL;
		hapd->eap_cfg = NULL;
#ifdef EAP_SIM_DB
		hapd->eap_sim_db_priv = NULL;
#endif /* EAP_SIM_DB */
#ifdef EAP_TLS_FUNCS
		hapd->ssl_ctx = NULL;
#endif /* EAP_TLS_FUNCS */
		return;
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef RADIUS_SERVER
	radius_server_deinit(hapd->radius_srv);
	hapd->radius_srv = NULL;
#endif /* RADIUS_SERVER */

#ifdef CRYPTO_RSA_OAEP_SHA256
	crypto_rsa_key_free(hapd->imsi_privacy_key);
	hapd->imsi_privacy_key = NULL;
#endif /* CRYPTO_RSA_OAEP_SHA256 */

#ifdef EAP_TLS_FUNCS
	if (hapd->ssl_ctx) {
		tls_deinit(hapd->ssl_ctx);
		hapd->ssl_ctx = NULL;
	}
#endif /* EAP_TLS_FUNCS */

#ifdef EAP_SIM_DB
	if (hapd->eap_sim_db_priv) {
		eap_sim_db_deinit(hapd->eap_sim_db_priv);
		hapd->eap_sim_db_priv = NULL;
	}
#endif /* EAP_SIM_DB */

	eap_server_config_free(hapd->eap_cfg);
	hapd->eap_cfg = NULL;
}
