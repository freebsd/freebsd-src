#ifndef EAPOL_SM_H
#define EAPOL_SM_H

#include "defs.h"

typedef enum { Unauthorized, Authorized } PortStatus;
typedef enum { Auto, ForceUnauthorized, ForceAuthorized } PortControl;

struct eapol_config {
	int accept_802_1x_keys;
#define EAPOL_REQUIRE_KEY_UNICAST BIT(0)
#define EAPOL_REQUIRE_KEY_BROADCAST BIT(1)
	int required_keys; /* which EAPOL-Key packets are required before
			    * marking connection authenticated */
	int fast_reauth; /* whether fast EAP reauthentication is enabled */
	int workaround; /* whether EAP workarounds are enabled */
};

struct eapol_sm;

struct eapol_ctx {
	void *ctx; /* pointer to arbitrary upper level context */
	int preauth; /* This EAPOL state machine is used for IEEE 802.11i/RSN
		      * pre-authentication */
	void (*cb)(struct eapol_sm *eapol, int success, void *ctx);
	void *cb_ctx, *msg_ctx, *scard_ctx;
	void (*eapol_done_cb)(void *ctx);
	int (*eapol_send)(void *ctx, int type, u8 *buf, size_t len);
	int (*set_wep_key)(void *ctx, int unicast, int keyidx,
			   u8 *key, size_t keylen);
};


struct wpa_ssid;

#ifdef IEEE8021X_EAPOL
struct eapol_sm *eapol_sm_init(struct eapol_ctx *ctx);
void eapol_sm_deinit(struct eapol_sm *sm);
void eapol_sm_step(struct eapol_sm *sm);
int eapol_sm_get_status(struct eapol_sm *sm, char *buf, size_t buflen,
			int verbose);
int eapol_sm_get_mib(struct eapol_sm *sm, char *buf, size_t buflen);
void eapol_sm_configure(struct eapol_sm *sm, int heldPeriod, int authPeriod,
			int startPeriod, int maxStart);
void eapol_sm_rx_eapol(struct eapol_sm *sm, u8 *src, u8 *buf, size_t len);
void eapol_sm_notify_tx_eapol_key(struct eapol_sm *sm);
void eapol_sm_notify_portEnabled(struct eapol_sm *sm, Boolean enabled);
void eapol_sm_notify_portValid(struct eapol_sm *sm, Boolean valid);
void eapol_sm_notify_eap_success(struct eapol_sm *sm, Boolean success);
void eapol_sm_notify_eap_fail(struct eapol_sm *sm, Boolean fail);
void eapol_sm_notify_config(struct eapol_sm *sm, struct wpa_ssid *config,
			    struct eapol_config *conf);
int eapol_sm_get_key(struct eapol_sm *sm, u8 *key, size_t len);
void eapol_sm_notify_logoff(struct eapol_sm *sm, Boolean logoff);
void eapol_sm_notify_cached(struct eapol_sm *sm);
void eapol_sm_notify_pmkid_attempt(struct eapol_sm *sm, int attempt);
void eapol_sm_register_scard_ctx(struct eapol_sm *sm, void *ctx);
void eapol_sm_notify_portControl(struct eapol_sm *sm, PortControl portControl);
void eapol_sm_notify_ctrl_attached(struct eapol_sm *sm);
void eapol_sm_notify_ctrl_response(struct eapol_sm *sm);
#else /* IEEE8021X_EAPOL */
static inline struct eapol_sm *eapol_sm_init(struct eapol_ctx *ctx)
{
	return (struct eapol_sm *) 1;
}
static inline void eapol_sm_deinit(struct eapol_sm *sm)
{
}
static inline void eapol_sm_step(struct eapol_sm *sm)
{
}
static inline int eapol_sm_get_status(struct eapol_sm *sm, char *buf,
				      size_t buflen, int verbose)
{
	return 0;
}
static inline int eapol_sm_get_mib(struct eapol_sm *sm, char *buf,
				   size_t buflen)
{
	return 0;
}
static inline void eapol_sm_configure(struct eapol_sm *sm, int heldPeriod,
				      int authPeriod, int startPeriod,
				      int maxStart)
{
}
static inline void eapol_sm_rx_eapol(struct eapol_sm *sm, u8 *src, u8 *buf,
				     size_t len)
{
}
static inline void eapol_sm_notify_tx_eapol_key(struct eapol_sm *sm)
{
}
static inline void eapol_sm_notify_portEnabled(struct eapol_sm *sm,
					       Boolean enabled)
{
}
static inline void eapol_sm_notify_portValid(struct eapol_sm *sm,
					     Boolean valid)
{
}
static inline void eapol_sm_notify_eap_success(struct eapol_sm *sm,
					       Boolean success)
{
}
static inline void eapol_sm_notify_eap_fail(struct eapol_sm *sm, Boolean fail)
{
}
static inline void eapol_sm_notify_config(struct eapol_sm *sm,
					  struct wpa_ssid *config,
					  struct eapol_config *conf)
{
}
static inline int eapol_sm_get_key(struct eapol_sm *sm, u8 *key, size_t len)
{
	return -1;
}
static inline void eapol_sm_notify_logoff(struct eapol_sm *sm, Boolean logoff)
{
}
static inline void eapol_sm_notify_cached(struct eapol_sm *sm)
{
}
#define eapol_sm_notify_pmkid_attempt(sm, attempt) do { } while (0)
#define eapol_sm_register_scard_ctx(sm, ctx) do { } while (0)
static inline void eapol_sm_notify_portControl(struct eapol_sm *sm,
					       PortControl portControl)
{
}
static inline void eapol_sm_notify_ctrl_attached(struct eapol_sm *sm)
{
}
static inline void eapol_sm_notify_ctrl_response(struct eapol_sm *sm)
{
}
#endif /* IEEE8021X_EAPOL */

#endif /* EAPOL_SM_H */
