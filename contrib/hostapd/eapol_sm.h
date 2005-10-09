#ifndef EAPOL_SM_H
#define EAPOL_SM_H

#include "defs.h"

/* IEEE Std 802.1X-REV-d11, Ch. 8.2 */

typedef enum { ForceUnauthorized = 1, ForceAuthorized = 3, Auto = 2 }
	PortTypes;
typedef enum { Unauthorized = 2, Authorized = 1 } PortState;
typedef enum { Both = 0, In = 1 } ControlledDirection;
typedef unsigned int Counter;


/* Authenticator PAE state machine */
struct eapol_auth_pae_sm {
	/* variables */
	Boolean eapolLogoff;
	Boolean eapolStart;
	Boolean eapRestart;
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

	enum { AUTH_PAE_INITIALIZE, AUTH_PAE_DISCONNECTED, AUTH_PAE_CONNECTING,
	       AUTH_PAE_AUTHENTICATING, AUTH_PAE_AUTHENTICATED,
	       AUTH_PAE_ABORTING, AUTH_PAE_HELD, AUTH_PAE_FORCE_AUTH,
	       AUTH_PAE_FORCE_UNAUTH, AUTH_PAE_RESTART } state;
};


/* Backend Authentication state machine */
struct eapol_backend_auth_sm {
	/* variables */
	Boolean eapNoReq;
	Boolean eapReq;
	Boolean eapResp;

	/* constants */
	unsigned int serverTimeout; /* default 30; 1..X */
#define BE_AUTH_DEFAULT_serverTimeout 30

	/* counters */
	Counter backendResponses;
	Counter backendAccessChallenges;
	Counter backendOtherRequestsToSupplicant;
	Counter backendAuthSuccesses;
	Counter backendAuthFails;

	enum { BE_AUTH_REQUEST, BE_AUTH_RESPONSE, BE_AUTH_SUCCESS,
	       BE_AUTH_FAIL, BE_AUTH_TIMEOUT, BE_AUTH_IDLE, BE_AUTH_INITIALIZE,
	       BE_AUTH_IGNORE
	} state;
};


/* Reauthentication Timer state machine */
struct eapol_reauth_timer_sm {
	/* constants */
	unsigned int reAuthPeriod; /* default 3600 s */
	Boolean reAuthEnabled;

	enum { REAUTH_TIMER_INITIALIZE, REAUTH_TIMER_REAUTHENTICATE } state;
};


/* Authenticator Key Transmit state machine */
struct eapol_auth_key_tx {
	enum { AUTH_KEY_TX_NO_KEY_TRANSMIT, AUTH_KEY_TX_KEY_TRANSMIT } state;
};


/* Key Receive state machine */
struct eapol_key_rx {
	/* variables */
	Boolean rxKey;

	enum { KEY_RX_NO_KEY_RECEIVE, KEY_RX_KEY_RECEIVE } state;
};


/* Controlled Directions state machine */
struct eapol_ctrl_dir {
	/* variables */
	ControlledDirection adminControlledDirections;
	ControlledDirection operControlledDirections;
	Boolean operEdge;

	enum { CTRL_DIR_FORCE_BOTH, CTRL_DIR_IN_OR_BOTH } state;
};


struct eap_sm;

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
	Boolean eapFail;
	Boolean eapolEap;
	Boolean eapSuccess;
	Boolean eapTimeout;
	Boolean initialize;
	Boolean keyAvailable;
	Boolean keyDone;
	Boolean keyRun;
	Boolean keyTxEnabled;
	PortTypes portControl;
	Boolean portEnabled;
	Boolean portValid;
	Boolean reAuthenticate;

	/* Port Timers state machine */
	/* 'Boolean tick' implicitly handled as registered timeout */

	struct eapol_auth_pae_sm auth_pae;
	struct eapol_backend_auth_sm be_auth;
	struct eapol_reauth_timer_sm reauth_timer;
	struct eapol_auth_key_tx auth_key_tx;
	struct eapol_key_rx key_rx;
	struct eapol_ctrl_dir ctrl_dir;

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
	int flags; /* EAPOL_SM_* */

	int radius_identifier;
	/* TODO: check when the last messages can be released */
	struct radius_msg *last_recv_radius;
	u8 *last_eap_supp; /* last received EAP Response from Supplicant */
	size_t last_eap_supp_len;
	u8 *last_eap_radius; /* last received EAP Response from Authentication
			      * Server */
	size_t last_eap_radius_len;
	u8 *identity;
	size_t identity_len;
	u8 *radius_class;
	size_t radius_class_len;

	/* Keys for encrypting and signing EAPOL-Key frames */
	u8 *eapol_key_sign;
	size_t eapol_key_sign_len;
	u8 *eapol_key_crypt;
	size_t eapol_key_crypt_len;

	Boolean rx_identity; /* set to TRUE on reception of
			      * EAP-Response/Identity */

	struct eap_sm *eap;

	/* currentId was removed in IEEE 802.1X-REV, but it is needed to filter
	 * out EAP-Responses to old packets (e.g., to two EAP-Request/Identity
	 * packets that are often sent in the beginning of the authentication).
	 */
	u8 currentId;

	Boolean initializing; /* in process of initializing state machines */

	/* Somewhat nasty pointers to global hostapd and STA data to avoid
	 * passing these to every function */
	struct hostapd_data *hapd;
	struct sta_info *sta;
};


struct eapol_state_machine *eapol_sm_alloc(hostapd *hapd,
					   struct sta_info *sta);
void eapol_sm_free(struct eapol_state_machine *sm);
void eapol_sm_step(struct eapol_state_machine *sm);
void eapol_sm_initialize(struct eapol_state_machine *sm);
void eapol_sm_dump_state(FILE *f, const char *prefix,
			 struct eapol_state_machine *sm);

#endif /* EAPOL_SM_H */
