#ifndef EAP_H
#define EAP_H

#include "defs.h"
#include "eap_defs.h"

struct eap_sm;
struct wpa_ssid;


#ifdef IEEE8021X_EAPOL

enum eapol_bool_var {
	EAPOL_eapSuccess, EAPOL_eapRestart, EAPOL_eapFail, EAPOL_eapResp,
	EAPOL_eapNoResp, EAPOL_eapReq, EAPOL_portEnabled, EAPOL_altAccept,
	EAPOL_altReject
};

enum eapol_int_var {
	EAPOL_idleWhile
};

struct eapol_callbacks {
	struct wpa_ssid * (*get_config)(void *ctx);
	Boolean (*get_bool)(void *ctx, enum eapol_bool_var variable);
	void (*set_bool)(void *ctx, enum eapol_bool_var variable,
			 Boolean value);
	unsigned int (*get_int)(void *ctx, enum eapol_int_var variable);
	void (*set_int)(void *ctx, enum eapol_int_var variable,
			unsigned int value);
	u8 * (*get_eapReqData)(void *ctx, size_t *len);
};

struct eap_sm *eap_sm_init(void *eapol_ctx, struct eapol_callbacks *eapol_cb,
			   void *msg_ctx);
void eap_sm_deinit(struct eap_sm *sm);
int eap_sm_step(struct eap_sm *sm);
void eap_sm_abort(struct eap_sm *sm);
int eap_sm_get_status(struct eap_sm *sm, char *buf, size_t buflen,
		      int verbose);
u8 *eap_sm_buildIdentity(struct eap_sm *sm, int id, size_t *len,
			 int encrypted);
const struct eap_method * eap_sm_get_eap_methods(int method);
void eap_sm_request_identity(struct eap_sm *sm, struct wpa_ssid *config);
void eap_sm_request_password(struct eap_sm *sm, struct wpa_ssid *config);
void eap_sm_request_otp(struct eap_sm *sm, struct wpa_ssid *config,
			char *msg, size_t msg_len);
void eap_sm_notify_ctrl_attached(struct eap_sm *sm);
u8 eap_get_type(const char *name);
u8 eap_get_phase2_type(const char *name);
u8 *eap_get_phase2_types(struct wpa_ssid *config, size_t *count);
void eap_set_fast_reauth(struct eap_sm *sm, int enabled);
void eap_set_workaround(struct eap_sm *sm, unsigned int workaround);
struct wpa_ssid * eap_get_config(struct eap_sm *sm);
int eap_key_available(struct eap_sm *sm);
void eap_notify_success(struct eap_sm *sm);
u8 * eap_get_eapKeyData(struct eap_sm *sm, size_t *len);
u8 * eap_get_eapRespData(struct eap_sm *sm, size_t *len);
void eap_register_scard_ctx(struct eap_sm *sm, void *ctx);

#else /* IEEE8021X_EAPOL */

static inline u8 eap_get_type(const char *name)
{
	return EAP_TYPE_NONE;
}

#endif /* IEEE8021X_EAPOL */

#endif /* EAP_H */
