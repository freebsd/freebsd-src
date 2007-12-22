/* $FreeBSD$ */

/*
 * hostapd / EAP Standalone Authenticator state machine (RFC 4137)
 * Copyright (c) 2004-2005, Jouni Malinen <j@w1.fi>
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

#ifndef EAP_H
#define EAP_H

#include "defs.h"
#include "eap_defs.h"
#include "eap_methods.h"

struct eap_sm;

#define EAP_MAX_METHODS 8
struct eap_user {
	struct {
		int vendor;
		u32 method;
	} methods[EAP_MAX_METHODS];
	u8 *password;
	size_t password_len;
	int password_hash; /* whether password is hashed with
			    * nt_password_hash() */
	int phase2;
	int force_version;
};

enum eapol_bool_var {
	EAPOL_eapSuccess, EAPOL_eapRestart, EAPOL_eapFail, EAPOL_eapResp,
	EAPOL_eapReq, EAPOL_eapNoReq, EAPOL_portEnabled, EAPOL_eapTimeout
};

struct eapol_callbacks {
	Boolean (*get_bool)(void *ctx, enum eapol_bool_var variable);
	void (*set_bool)(void *ctx, enum eapol_bool_var variable,
			 Boolean value);
	void (*set_eapReqData)(void *ctx, const u8 *eapReqData,
			       size_t eapReqDataLen);
	void (*set_eapKeyData)(void *ctx, const u8 *eapKeyData,
			       size_t eapKeyDataLen);
	int (*get_eap_user)(void *ctx, const u8 *identity, size_t identity_len,
			    int phase2, struct eap_user *user);
	const char * (*get_eap_req_id_text)(void *ctx, size_t *len);
};

struct eap_config {
	void *ssl_ctx;
	void *eap_sim_db_priv;
	Boolean backend_auth;
};


#ifdef EAP_SERVER

struct eap_sm * eap_sm_init(void *eapol_ctx, struct eapol_callbacks *eapol_cb,
			    struct eap_config *eap_conf);
void eap_sm_deinit(struct eap_sm *sm);
int eap_sm_step(struct eap_sm *sm);
void eap_set_eapRespData(struct eap_sm *sm, const u8 *eapRespData,
			 size_t eapRespDataLen);
void eap_sm_notify_cached(struct eap_sm *sm);
void eap_sm_pending_cb(struct eap_sm *sm);
int eap_sm_method_pending(struct eap_sm *sm);

#else /* EAP_SERVER */

static inline struct eap_sm * eap_sm_init(void *eapol_ctx,
					  struct eapol_callbacks *eapol_cb,
					  struct eap_config *eap_conf)
{
	return NULL;
}

static inline void eap_sm_deinit(struct eap_sm *sm)
{
}

static inline int eap_sm_step(struct eap_sm *sm)
{
	return 0;
}


static inline void eap_set_eapRespData(struct eap_sm *sm,
				       const u8 *eapRespData,
				       size_t eapRespDataLen)
{
}

static inline void eap_sm_notify_cached(struct eap_sm *sm)
{
}

static inline void eap_sm_pending_cb(struct eap_sm *sm)
{
}

static inline int eap_sm_method_pending(struct eap_sm *sm)
{
	return 0;
}

#endif /* EAP_SERVER */

#endif /* EAP_H */
