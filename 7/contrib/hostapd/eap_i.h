/*
 * hostapd / EAP Authenticator state machine internal structures (RFC 4137)
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

#ifndef EAP_I_H
#define EAP_I_H

#include "eap.h"

/* RFC 4137 - EAP Standalone Authenticator */

/**
 * struct eap_method - EAP method interface
 * This structure defines the EAP method interface. Each method will need to
 * register its own EAP type, EAP name, and set of function pointers for method
 * specific operations. This interface is based on section 5.4 of RFC 4137.
 */
struct eap_method {
	int vendor;
	EapType method;
	const char *name;

	void * (*init)(struct eap_sm *sm);
	void * (*initPickUp)(struct eap_sm *sm);
	void (*reset)(struct eap_sm *sm, void *priv);

	u8 * (*buildReq)(struct eap_sm *sm, void *priv, int id,
			 size_t *reqDataLen);
	int (*getTimeout)(struct eap_sm *sm, void *priv);
	Boolean (*check)(struct eap_sm *sm, void *priv,
			 u8 *respData, size_t respDataLen);
	void (*process)(struct eap_sm *sm, void *priv,
			u8 *respData, size_t respDataLen);
	Boolean (*isDone)(struct eap_sm *sm, void *priv);
	u8 * (*getKey)(struct eap_sm *sm, void *priv, size_t *len);
	/* isSuccess is not specified in draft-ietf-eap-statemachine-05.txt,
	 * but it is useful in implementing Policy.getDecision() */
	Boolean (*isSuccess)(struct eap_sm *sm, void *priv);

	/**
	 * free - Free EAP method data
	 * @method: Pointer to the method data registered with
	 * eap_server_method_register().
	 *
	 * This function will be called when the EAP method is being
	 * unregistered. If the EAP method allocated resources during
	 * registration (e.g., allocated struct eap_method), they should be
	 * freed in this function. No other method functions will be called
	 * after this call. If this function is not defined (i.e., function
	 * pointer is %NULL), a default handler is used to release the method
	 * data with free(method). This is suitable for most cases.
	 */
	void (*free)(struct eap_method *method);

#define EAP_SERVER_METHOD_INTERFACE_VERSION 1
	/**
	 * version - Version of the EAP server method interface
	 *
	 * The EAP server method implementation should set this variable to
	 * EAP_SERVER_METHOD_INTERFACE_VERSION. This is used to verify that the
	 * EAP method is using supported API version when using dynamically
	 * loadable EAP methods.
	 */
	int version;

	/**
	 * next - Pointer to the next EAP method
	 *
	 * This variable is used internally in the EAP method registration code
	 * to create a linked list of registered EAP methods.
	 */
	struct eap_method *next;

	/**
	 * get_emsk - Get EAP method specific keying extended material (EMSK)
	 * @sm: Pointer to EAP state machine allocated with eap_sm_init()
	 * @priv: Pointer to private EAP method data from eap_method::init()
	 * @len: Pointer to a variable to store EMSK length
	 * Returns: EMSK or %NULL if not available
	 *
	 * This function can be used to get the extended keying material from
	 * the EAP method. The key may already be stored in the method-specific
	 * private data or this function may derive the key.
	 */
	u8 * (*get_emsk)(struct eap_sm *sm, void *priv, size_t *len);
};

/**
 * struct eap_sm - EAP server state machine data
 */
struct eap_sm {
	enum {
		EAP_DISABLED, EAP_INITIALIZE, EAP_IDLE, EAP_RECEIVED,
		EAP_INTEGRITY_CHECK, EAP_METHOD_RESPONSE, EAP_METHOD_REQUEST,
		EAP_PROPOSE_METHOD, EAP_SELECT_ACTION, EAP_SEND_REQUEST,
		EAP_DISCARD, EAP_NAK, EAP_RETRANSMIT, EAP_SUCCESS, EAP_FAILURE,
		EAP_TIMEOUT_FAILURE, EAP_PICK_UP_METHOD
	} EAP_state;

	/* Constants */
	int MaxRetrans;

	/* Lower layer to standalone authenticator variables */
	/* eapResp: eapol_sm->be_auth.eapResp */
	/* portEnabled: eapol_sm->portEnabled */
	/* eapRestart: eapol_sm->auth_pae.eapRestart */
	u8 *eapRespData;
	size_t eapRespDataLen;
	int retransWhile;
	int eapSRTT;
	int eapRTTVAR;

	/* Standalone authenticator to lower layer variables */
	/* eapReq: eapol_sm->be_auth.eapReq */
	/* eapNoReq: eapol_sm->be_auth.eapNoReq */
	/* eapSuccess: eapol_sm->eapSuccess */
	/* eapFail: eapol_sm->eapFail */
	/* eapTimeout: eapol_sm->eapTimeout */
	u8 *eapReqData;
	size_t eapReqDataLen;
	u8 *eapKeyData; /* also eapKeyAvailable (boolean) */
	size_t eapKeyDataLen;

	/* Standalone authenticator state machine local variables */

	/* Long-term (maintained betwen packets) */
	EapType currentMethod;
	int currentId;
	enum {
		METHOD_PROPOSED, METHOD_CONTINUE, METHOD_END
	} methodState;
	int retransCount;
	u8 *lastReqData;
	size_t lastReqDataLen;
	int methodTimeout;

	/* Short-term (not maintained between packets) */
	Boolean rxResp;
	int respId;
	EapType respMethod;
	int respVendor;
	u32 respVendorMethod;
	Boolean ignore;
	enum {
		DECISION_SUCCESS, DECISION_FAILURE, DECISION_CONTINUE
	} decision;

	/* Miscellaneous variables */
	const struct eap_method *m; /* selected EAP method */
	/* not defined in draft-ietf-eap-statemachine-02 */
	Boolean changed;
	void *eapol_ctx, *msg_ctx;
	struct eapol_callbacks *eapol_cb;
	void *eap_method_priv;
	u8 *identity;
	size_t identity_len;
	int lastId; /* Identifier used in the last EAP-Packet */
	struct eap_user *user;
	int user_eap_method_index;
	int init_phase2;
	void *ssl_ctx;
	enum { TLV_REQ_NONE, TLV_REQ_SUCCESS, TLV_REQ_FAILURE } tlv_request;
	void *eap_sim_db_priv;
	Boolean backend_auth;
	Boolean update_user;

	int num_rounds;
	enum {
		METHOD_PENDING_NONE, METHOD_PENDING_WAIT, METHOD_PENDING_CONT
	} method_pending;
};

int eap_user_get(struct eap_sm *sm, const u8 *identity, size_t identity_len,
		 int phase2);
void eap_sm_process_nak(struct eap_sm *sm, u8 *nak_list, size_t len);
const u8 * eap_hdr_validate(int vendor, EapType eap_type,
			    const u8 *msg, size_t msglen, size_t *plen);
struct eap_hdr * eap_msg_alloc(int vendor, EapType type, size_t *len,
			       size_t payload_len, u8 code, u8 identifier,
			       u8 **payload);

#endif /* EAP_I_H */
