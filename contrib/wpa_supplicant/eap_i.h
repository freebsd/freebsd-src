#ifndef EAP_I_H
#define EAP_I_H

#include "eap.h"

/* draft-ietf-eap-statemachine-05.pdf - Peer state machine */

typedef enum {
	DECISION_FAIL, DECISION_COND_SUCC, DECISION_UNCOND_SUCC
} EapDecision;

typedef enum {
	METHOD_NONE, METHOD_INIT, METHOD_CONT, METHOD_MAY_CONT, METHOD_DONE
} EapMethodState;

struct eap_method_ret {
	Boolean ignore;
	EapMethodState methodState;
	EapDecision decision;
	Boolean allowNotifications;
};


struct eap_method {
	EapType method;
	const char *name;

	void * (*init)(struct eap_sm *sm);
	void (*deinit)(struct eap_sm *sm, void *priv);
	u8 * (*process)(struct eap_sm *sm, void *priv,
			struct eap_method_ret *ret,
			u8 *reqData, size_t reqDataLen,
			size_t *respDataLen);
	Boolean (*isKeyAvailable)(struct eap_sm *sm, void *priv);
	u8 * (*getKey)(struct eap_sm *sm, void *priv, size_t *len);
	int (*get_status)(struct eap_sm *sm, void *priv, char *buf,
			  size_t buflen, int verbose);

	/* Optional handlers for fast re-authentication */
	Boolean (*has_reauth_data)(struct eap_sm *sm, void *priv);
	void (*deinit_for_reauth)(struct eap_sm *sm, void *priv);
	void * (*init_for_reauth)(struct eap_sm *sm, void *priv);
	const u8 * (*get_identity)(struct eap_sm *sm, void *priv, size_t *len);
};


struct eap_sm {
	enum {
		EAP_INITIALIZE, EAP_DISABLED, EAP_IDLE, EAP_RECEIVED,
		EAP_GET_METHOD, EAP_METHOD, EAP_SEND_RESPONSE, EAP_DISCARD,
		EAP_IDENTITY, EAP_NOTIFICATION, EAP_RETRANSMIT, EAP_SUCCESS,
		EAP_FAILURE
	} EAP_state;
	/* Long-term local variables */
	EapType selectedMethod;
	EapMethodState methodState;
	int lastId;
	u8 *lastRespData;
	size_t lastRespDataLen;
	EapDecision decision;
	/* Short-term local variables */
	Boolean rxReq;
	Boolean rxSuccess;
	Boolean rxFailure;
	int reqId;
	EapType reqMethod;
	Boolean ignore;
	/* Constants */
	int ClientTimeout;

	/* Miscellaneous variables */
	Boolean allowNotifications; /* peer state machine <-> methods */
	u8 *eapRespData; /* peer to lower layer */
	size_t eapRespDataLen; /* peer to lower layer */
	Boolean eapKeyAvailable; /* peer to lower layer */
	u8 *eapKeyData; /* peer to lower layer */
	size_t eapKeyDataLen; /* peer to lower layer */
	const struct eap_method *m; /* selected EAP method */
	/* not defined in draft-ietf-eap-statemachine-02 */
	Boolean changed;
	void *eapol_ctx;
	struct eapol_callbacks *eapol_cb;
	void *eap_method_priv;
	int init_phase2;
	int fast_reauth;

	Boolean rxResp /* LEAP only */;
	Boolean leap_done;
	Boolean peap_done;
	u8 req_md5[16]; /* MD5() of the current EAP packet */
	u8 last_md5[16]; /* MD5() of the previously received EAP packet; used
			  * in duplicate request detection. */

	void *msg_ctx;
	void *scard_ctx;
	void *ssl_ctx;

	unsigned int workaround;

	/* Optional challenges generated in Phase 1 (EAP-FAST) */
	u8 *peer_challenge, *auth_challenge;

	int num_rounds;
};

#endif /* EAP_I_H */
