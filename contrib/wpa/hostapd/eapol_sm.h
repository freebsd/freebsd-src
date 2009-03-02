/*
 * hostapd / IEEE 802.1X-2004 Authenticator - EAPOL state machine
 * Copyright (c) 2002-2007, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#ifndef EAPOL_SM_H
#define EAPOL_SM_H

#include "defs.h"

/* IEEE Std 802.1X-2004, Ch. 8.2 */

typedef enum { ForceUnauthorized = 1, ForceAuthorized = 3, Auto = 2 }
	PortTypes;
typedef enum { Unauthorized = 2, Authorized = 1 } PortState;
typedef enum { Both = 0, In = 1 } ControlledDirection;
typedef unsigned int Counter;

struct eap_sm;

struct radius_attr_data {
	u8 *data;
	size_t len;
};

struct radius_class_data {
	struct radius_attr_data *attr;
	size_t count;
};


struct eapol_auth_config {
	int eap_reauth_period;
	int wpa;
	int individual_wep_key_len;
	int eap_server;
	void *ssl_ctx;
	void *eap_sim_db_priv;
	char *eap_req_id_text; /* a copy of this will be allocated */
	size_t eap_req_id_text_len;
	u8 *pac_opaque_encr_key;
	u8 *eap_fast_a_id;
	size_t eap_fast_a_id_len;
	char *eap_fast_a_id_info;
	int eap_fast_prov;
	int pac_key_lifetime;
	int pac_key_refresh_time;
	int eap_sim_aka_result_ind;
	int tnc;
	struct wps_context *wps;

	/*
	 * Pointer to hostapd data. This is a temporary workaround for
	 * transition phase and will be removed once IEEE 802.1X/EAPOL code is
	 * separated more cleanly from rest of hostapd.
	 */
	struct hostapd_data *hapd;
};

struct eap_user;

typedef enum {
	EAPOL_LOGGER_DEBUG, EAPOL_LOGGER_INFO, EAPOL_LOGGER_WARNING
} eapol_logger_level;

struct eapol_auth_cb {
	void (*eapol_send)(void *ctx, void *sta_ctx, u8 type, const u8 *data,
			   size_t datalen);
	void (*aaa_send)(void *ctx, void *sta_ctx, const u8 *data,
			 size_t datalen);
	void (*finished)(void *ctx, void *sta_ctx, int success, int preauth);
	int (*get_eap_user)(void *ctx, const u8 *identity, size_t identity_len,
			    int phase2, struct eap_user *user);
	int (*sta_entry_alive)(void *ctx, const u8 *addr);
	void (*logger)(void *ctx, const u8 *addr, eapol_logger_level level,
		       const char *txt);
	void (*set_port_authorized)(void *ctx, void *sta_ctx, int authorized);
	void (*abort_auth)(void *ctx, void *sta_ctx);
	void (*tx_key)(void *ctx, void *sta_ctx);
};

/**
 * struct eapol_authenticator - Global EAPOL authenticator data
 */
struct eapol_authenticator {
	struct eapol_auth_config conf;
	struct eapol_auth_cb cb;
};


/**
 * struct eapol_state_machine - Per-Supplicant Authenticator state machines
 */
struct eapol_state_machine {
	/* timers */
	int aWhile;
	int quietWhile;
	int reAuthWhen;

	/* global variables */
	Boolean authAbort;
	Boolean authFail;
	PortState authPortStatus;
	Boolean authStart;
	Boolean authTimeout;
	Boolean authSuccess;
	Boolean eapolEap;
	Boolean initialize;
	Boolean keyDone;
	Boolean keyRun;
	Boolean keyTxEnabled;
	PortTypes portControl;
	Boolean portValid;
	Boolean reAuthenticate;

	/* Port Timers state machine */
	/* 'Boolean tick' implicitly handled as registered timeout */

	/* Authenticator PAE state machine */
	enum { AUTH_PAE_INITIALIZE, AUTH_PAE_DISCONNECTED, AUTH_PAE_CONNECTING,
	       AUTH_PAE_AUTHENTICATING, AUTH_PAE_AUTHENTICATED,
	       AUTH_PAE_ABORTING, AUTH_PAE_HELD, AUTH_PAE_FORCE_AUTH,
	       AUTH_PAE_FORCE_UNAUTH, AUTH_PAE_RESTART } auth_pae_state;
	/* variables */
	Boolean eapolLogoff;
	Boolean eapolStart;
	PortTypes portMode;
	unsigned int reAuthCount;
	/* constants */
	unsigned int quietPeriod; /* default 60; 0..65535 */
#define AUTH_PAE_DEFAULT_quietPeriod 60
	unsigned int reAuthMax; /* default 2 */
#define AUTH_PAE_DEFAULT_reAuthMax 2
	/* counters */
	Counter authEntersConnecting;
	Counter authEapLogoffsWhileConnecting;
	Counter authEntersAuthenticating;
	Counter authAuthSuccessesWhileAuthenticating;
	Counter authAuthTimeoutsWhileAuthenticating;
	Counter authAuthFailWhileAuthenticating;
	Counter authAuthEapStartsWhileAuthenticating;
	Counter authAuthEapLogoffWhileAuthenticating;
	Counter authAuthReauthsWhileAuthenticated;
	Counter authAuthEapStartsWhileAuthenticated;
	Counter authAuthEapLogoffWhileAuthenticated;

	/* Backend Authentication state machine */
	enum { BE_AUTH_REQUEST, BE_AUTH_RESPONSE, BE_AUTH_SUCCESS,
	       BE_AUTH_FAIL, BE_AUTH_TIMEOUT, BE_AUTH_IDLE, BE_AUTH_INITIALIZE,
	       BE_AUTH_IGNORE
	} be_auth_state;
	/* constants */
	unsigned int serverTimeout; /* default 30; 1..X */
#define BE_AUTH_DEFAULT_serverTimeout 30
	/* counters */
	Counter backendResponses;
	Counter backendAccessChallenges;
	Counter backendOtherRequestsToSupplicant;
	Counter backendAuthSuccesses;
	Counter backendAuthFails;

	/* Reauthentication Timer state machine */
	enum { REAUTH_TIMER_INITIALIZE, REAUTH_TIMER_REAUTHENTICATE
	} reauth_timer_state;
	/* constants */
	unsigned int reAuthPeriod; /* default 3600 s */
	Boolean reAuthEnabled;

	/* Authenticator Key Transmit state machine */
	enum { AUTH_KEY_TX_NO_KEY_TRANSMIT, AUTH_KEY_TX_KEY_TRANSMIT
	} auth_key_tx_state;

	/* Key Receive state machine */
	enum { KEY_RX_NO_KEY_RECEIVE, KEY_RX_KEY_RECEIVE } key_rx_state;
	/* variables */
	Boolean rxKey;

	/* Controlled Directions state machine */
	enum { CTRL_DIR_FORCE_BOTH, CTRL_DIR_IN_OR_BOTH } ctrl_dir_state;
	/* variables */
	ControlledDirection adminControlledDirections;
	ControlledDirection operControlledDirections;
	Boolean operEdge;

	/* Authenticator Statistics Table */
	Counter dot1xAuthEapolFramesRx;
	Counter dot1xAuthEapolFramesTx;
	Counter dot1xAuthEapolStartFramesRx;
	Counter dot1xAuthEapolLogoffFramesRx;
	Counter dot1xAuthEapolRespIdFramesRx;
	Counter dot1xAuthEapolRespFramesRx;
	Counter dot1xAuthEapolReqIdFramesTx;
	Counter dot1xAuthEapolReqFramesTx;
	Counter dot1xAuthInvalidEapolFramesRx;
	Counter dot1xAuthEapLengthErrorFramesRx;
	Counter dot1xAuthLastEapolFrameVersion;

	/* Other variables - not defined in IEEE 802.1X */
	u8 addr[ETH_ALEN]; /* Supplicant address */
#define EAPOL_SM_PREAUTH BIT(0)
#define EAPOL_SM_WAIT_START BIT(1)
	int flags; /* EAPOL_SM_* */

	/* EAPOL/AAA <-> EAP full authenticator interface */
	struct eap_eapol_interface *eap_if;

	int radius_identifier;
	/* TODO: check when the last messages can be released */
	struct radius_msg *last_recv_radius;
	u8 last_eap_id; /* last used EAP Identifier */
	u8 *identity;
	size_t identity_len;
	u8 eap_type_authsrv; /* EAP type of the last EAP packet from
			      * Authentication server */
	u8 eap_type_supp; /* EAP type of the last EAP packet from Supplicant */
	struct radius_class_data radius_class;

	/* Keys for encrypting and signing EAPOL-Key frames */
	u8 *eapol_key_sign;
	size_t eapol_key_sign_len;
	u8 *eapol_key_crypt;
	size_t eapol_key_crypt_len;

	struct eap_sm *eap;

	Boolean initializing; /* in process of initializing state machines */
	Boolean changed;

	struct eapol_authenticator *eapol;

	/* Somewhat nasty pointers to global hostapd and STA data to avoid
	 * passing these to every function */
	struct hostapd_data *hapd;
	struct sta_info *sta;
};


struct eapol_authenticator * eapol_auth_init(struct eapol_auth_config *conf,
					     struct eapol_auth_cb *cb);
void eapol_auth_deinit(struct eapol_authenticator *eapol);
struct eapol_state_machine *
eapol_auth_alloc(struct eapol_authenticator *eapol, const u8 *addr,
		 int preauth, struct sta_info *sta);
void eapol_auth_free(struct eapol_state_machine *sm);
void eapol_auth_step(struct eapol_state_machine *sm);
void eapol_auth_initialize(struct eapol_state_machine *sm);
void eapol_auth_dump_state(FILE *f, const char *prefix,
			   struct eapol_state_machine *sm);
int eapol_auth_eap_pending_cb(struct eapol_state_machine *sm, void *ctx);

#endif /* EAPOL_SM_H */
