#ifndef EAP_I_H
#define EAP_I_H

#include "eap.h"

/* draft-ietf-eap-statemachine-05.pdf - EAP Standalone Authenticator */

struct eap_method {
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
};

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
};

const struct eap_method * eap_sm_get_eap_methods(int method);
int eap_user_get(struct eap_sm *sm, const u8 *identity, size_t identity_len,
		 int phase2);
void eap_sm_process_nak(struct eap_sm *sm, u8 *nak_list, size_t len);

#endif /* EAP_I_H */
