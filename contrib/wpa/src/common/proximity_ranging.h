/*
 * Proxmity Ranging
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef PROXIMITY_RANGING_H
#define PROXIMITY_RANGING_H

#include "wpa_common.h"
#include "utils/list.h"
#include "wps/wps_defs.h"

struct ieee80211_mgmt;

#define DEVICE_IDENTITY_KEY_LEN 16
#define DEVICE_IDENTITY_TAG_LEN 8
#define DEVICE_IDENTITY_NONCE_LEN 8
#define DIR_STR_LEN 3
#define DEVICE_MAX_HASH_LEN 32

/* DIRA Cipher versions */
#define DIRA_CIPHER_VERSION_128 0

/**
 * PR_MAX_OP_CLASSES - Maximum number of operating classes
 */
#define PR_MAX_OP_CLASSES 30

/**
 * PR_MAX_OP_CLASS_CHANNELS - Maximum number of channels per operating class
 */
#define PR_MAX_OP_CLASS_CHANNELS 60

/**
 * PR_MAX_PEER - Maximum number of Proximity Ranging peers that device can store
 */
#define PR_MAX_PEER 100

/*
 * Proximity Ranging negotiation status
 * Proximity Ranging Implementation Considerations for P2P Operation D1.8,
 * Table 5 (Proximity Ranging Status Attribute format).
 */
#define PR_NEGOTIATION_SUCCESS 0
#define PR_NEGOTIATION_UPDATE 1
#define PR_NEGOTIATION_FAIL 2

/**
 * enum pr_session_end_reason - Reason codes for ranging session end
 */
enum pr_session_end_reason {
	PR_SESSION_END_TIMEOUT       = 0,
	PR_SESSION_END_USER_ABORT    = 1,
	PR_SESSION_END_PEER_COMPLETE = 2,
	PR_SESSION_END_NEG_FAILED    = 3,
};

enum pr_pasn_role {
	PR_ROLE_IDLE = 0,
	PR_ROLE_PASN_INITIATOR,
	PR_ROLE_PASN_RESPONDER,
};

/**
 * struct pr_channels - List of supported channels
 */
struct pr_channels {
	/**
	 * struct pr_op_class - Supported operating class
	 */
	struct pr_op_class {
		/**
		 * op_class - Operating class
		 */
		u8 op_class;

		/**
		 * channel - Supported channels
		 */
		u8 channel[PR_MAX_OP_CLASS_CHANNELS];

		/**
		 * channels - Number of channel entries in use
		 */
		size_t channels;
	} op_class[PR_MAX_OP_CLASSES];

	/**
	 * op_classes - Number of op_class entries in use
	 */
	size_t op_classes;
};

/**
 * Format and Bandwidth values for EDCA based ranging with range of 10-16
 * from IEEE Std 802.11-2024, 9.4.2.166 (FTM Parameters element), Table 9-325
 * (Format And Bandwidth subfield) as specified in Proximity Ranging
 * Implementation Considerations for P2P Operation, Draft 1.8, Table 8
 * (Proximity Ranging EDCA Capability Attribute format) for the the Ranging
 * Parameters field B0-B3.
 */
enum edca_format_and_bw_value {
	EDCA_FORMAT_AND_BW_VHT20 = 10,
	EDCA_FORMAT_AND_BW_HT40 = 11,
	EDCA_FORMAT_AND_BW_VHT40 = 12,
	EDCA_FORMAT_AND_BW_VHT80 = 13,
	EDCA_FORMAT_AND_BW_VHT80P80 = 14,
	EDCA_FORMAT_AND_BW_VHT160_DUAL_LO = 15,
	EDCA_FORMAT_AND_BW_VHT160_SINGLE_LO = 16,
	EDCA_FORMAT_AND_BW_INVALID
};

/**
 * Format and Bandwidth values for NTB based ranging as from IEEE Std
 * 802.11-2024, 9.4.2.300 (Ranging Parameters element), Table 9-412 (Format And
 * Bandwidth subfield) as specified in Proximity Ranging Implementation
 * Considerations for P2P Operation, Draft 1.8, Table 9 (Proximity Ranging 11az
 * NTB Capability Attribute format) for the Ranging Parameter field B0-B2.
 */
enum ntb_format_and_bw_value {
	NTB_FORMAT_AND_BW_HE20 = 0,
	NTB_FORMAT_AND_BW_HE40 = 1,
	NTB_FORMAT_AND_BW_HE80 = 2,
	NTB_FORMAT_AND_BW_HE80P80 = 3,
	NTB_FORMAT_AND_BW_HE160_DUAL_LO = 4,
	NTB_FORMAT_AND_BW_HE160_SINGLE_LO = 5,
	NTB_FORMAT_AND_BW_INVALID
};

struct pr_capabilities {
	u8 pasn_type;

	char device_name[WPS_DEV_NAME_MAX_LEN + 1];

	bool edca_support;

	bool ntb_support;

	bool secure_he_ltf;

	bool support_6ghz;
};

struct edca_capabilities {
	bool ista_support;

	bool rsta_support;

/**
 * Ranging Parameters field for device specific EDCA capabilities
 * Proximity Ranging Implementation Considerations for P2P Operation Draft 1.8,
 * Table 8 (Proximity Ranging EDCA Capability Attribute format).
 */
#define EDCA_FORMAT_AND_BW  0
#define EDCA_MAX_TX_ANTENNA 4
#define EDCA_MAX_RX_ANTENNA 7

#define EDCA_FORMAT_AND_BW_MASK  0x000F
#define EDCA_MAX_TX_ANTENNA_MASK 0x0007
#define EDCA_MAX_RX_ANTENNA_MASK 0x0007
	u16 edca_hw_caps;

	char country[3];

	struct pr_channels channels;
};

struct ntb_capabilities {
	bool ista_support;

	bool rsta_support;

	bool secure_he_ltf;

/**
 * Ranging Parameter field for NTB capabilities
 * Proximity Ranging Implementation Considerations for P2P Operation Draft 1.8,
 * Table 9 (Proximity Ranging 11az NTB Capability Attribute format).
 */
#define NTB_FORMAT_AND_BW	0
#define MAX_TX_LTF_REPETATIONS	3
#define MAX_RX_LTF_REPETATIONS	6
#define MAX_RX_LTF_TOTAL	9
#define MAX_TX_LTF_TOTAL	11
#define MAX_RX_STS_LE_80	13
#define MAX_RX_STS_GT_80	16
#define MAX_TX_STS_LE_80	19
#define MAX_TX_STS_GT_80	22

#define NTB_FORMAT_AND_BW_MASK  0x00000007

/* Max TX LTF repetations supported for non trigger based ranging */
#define MAX_TX_LTF_REPETATIONS_MASK	0x00000007

/* Max RX LTF repetations supported for non trigger based ranging */
#define MAX_RX_LTF_REPETATIONS_MASK	0x00000007

/* Max RX LTF total supported for non trigger based ranging */
#define MAX_RX_LTF_TOTAL_MASK		0x00000003

/* Max TX LTF total supported for non trigger based ranging */
#define MAX_TX_LTF_TOTAL_MASK		0x00000003

/* To configure max R2I STS for Bandwidth less than or equal to 80 MHz */
#define MAX_RX_STS_LE_80_MASK		0x00000007

/* To configure max R2I STS for Bandwidth greater than 80Mz */
#define MAX_RX_STS_GT_80_MASK		0x00000007

/* To configure max I2R STS for Bandwidth less than or equal to 80 MHz */
#define MAX_TX_STS_LE_80_MASK		0x00000007

/* To configure max I2R STS for Bandwidth greater than 80 MHz */
#define MAX_TX_STS_GT_80_MASK		0x00000007
	u32 ntb_hw_caps;

	char country[3];

	struct pr_channels channels;
};

/*
 * Proximity Ranging Attribute IDs
 * Proximity Ranging Implementation Considerations for P2P Operation Draft 1.8,
 * Table 4 (Proximity Ranging Attribute ID list).
 */
enum pr_attr_id {
	PR_ATTR_STATUS = 0,
	PR_ATTR_RANGING_CAPABILITY = 1,
	PR_ATTR_EDCA_CAPABILITY = 2,
	PR_ATTR_NTB_CAPABILITY = 3,
	PR_ATTR_OPERATION_MODE = 4,
	PR_ATTR_WLAN_AP_INFO = 5,
	PR_ATTR_DEVICE_IDENTITY_RESOLUTION = 6,
	PR_ATTR_VENDOR_SPECIFIC = 221,
};

/*
 * Proximity Ranging capabilities in Ranging Protocol Type field,
 * Proximity Ranging Implementation Considerations for P2P Operation Draft 1.8,
 * Table 7 (Proximity Ranging Capability Attribute format).
 */
#define PR_EDCA_BASED_RANGING BIT(0)
#define PR_NTB_SECURE_LTF_BASED_RANGING BIT(1)
#define PR_NTB_OPEN_BASED_RANGING BIT(2)

/**
 * Ranging Role field in EDCA capabilities
 * Proximity Ranging Implementation Considerations for P2P Operation Draft 1.8,
 * Table 8 (Proximity Ranging EDCA Capability Attribute format).
 */
#define PR_ISTA_SUPPORT BIT(0)
#define PR_RSTA_SUPPORT BIT(1)

/*
 * PASN capabilities in PASN Type field
 * Proximity Ranging Implementation Considerations for P2P Operation D1.8,
 * Table 7 (Proximity Ranging Capability Attribute).
 */
#define PR_PASN_DH19_UNAUTH BIT(0)
#define PR_PASN_DH19_AUTH BIT(1)
#define PR_PASN_DH20_UNAUTH BIT(2)
#define PR_PASN_DH20_AUTH BIT(3)

/* Authentication Mode */
#define PR_PASN_AUTH_MODE_PASN   0
#define PR_PASN_AUTH_MODE_SAE    1
#define PR_PASN_AUTH_MODE_PMK    2

/* Peer discovery type */
#define PR_DISCOVERY_TYPE_USD    0
#define PR_DISCOVERY_TYPE_OOB    1

/**
 * struct pr_pasn_ranging_params - Peer parameters to be used in PASN to trigger
 * ranging in case PR PASN is successful.
 */
struct pr_pasn_ranging_params {
	enum {
		PR_PASN_AND_RANGING,
		PR_STOP_RANGING,
	} action;
	u8 peer_addr[ETH_ALEN];
	u8 ranging_type;
	u8 ranging_role;
	u8 pr_pasn_status;
	u8 auth_mode;
	int freq;
	u32 ranging_timeout;
	u8 src_addr[ETH_ALEN];
	enum pr_pasn_role pasn_role;

	/**
	 * EDCA based ranging specific parameters
	 *
	 * @burst_period: Burst period in units of 100 milliseconds
	 * @num_bursts_exp: Number of bursts exponent
	 * @ftms_per_burst: Number of FTM frames per burst
	 * @ftmr_retries: Number of retries for FTM Request frame
	 * @burst_duration: Burst duration as defined in IEEE Std 802.11-2024,
	 *  Table 9-322 (Burst Duration subfield encoding).
	 */
	u16 burst_period;
	u8 num_bursts_exp;
	u8 ftms_per_burst;
	u8 ftmr_retries;
	u8 burst_duration;

	/**
	 * NTB ranging specific parameters
	 *
	 * @min_time_between_measurements: Minimum time between two consecutive
	 *  range measurements in units of 100 microseconds.
	 * @max_time_between_measurements: Maximum time between two consecutive
	 *  range measurements in units of 10 milliseconds, to avoid FTM
	 *  negotiation.
	 * @availability_window: Duration of the Availability Window (AW) in
	 *  units of 1 millisecond (0-255 ms).
	 * @nominal_time: Nominal duration between adjacent Availability Windows
	 *  in units of milliseconds.
	 */
	u32 min_time_between_measurements;
	u32 max_time_between_measurements;
	u8 availability_window;
	u32 nominal_time;

	/**
	 * @request_lci: Whether to request LCI
	 * @request_civicloc: Whether to request civic location
	 */
	bool request_lci;
	bool request_civicloc;

	/**
	 * @lmr_feedback: Negotiate LMR feedback for NTB ranging.
	 * Only valid when NTB ranging type is set. Required for RSTA
	 * to report measurement results back to the initiator.
	 */
	bool lmr_feedback;

	/**
	 * @ingress_threshold: Ingress range threshold in millimeters.
	 * The kernel reports a measurement result when the device
	 * moves into this range.
	 */
	u64 ingress_threshold;

	/**
	 * @egress_threshold: Egress range threshold in millimeters.
	 * The kernel reports a measurement result when the device
	 * moves out of this range.
	 */
	u64 egress_threshold;

	/**
	 * @pr_suppress_results: Suppress ranging results for PD requests.
	 * Cannot be used with range_report or lmr_feedback.
	 */
	bool pr_suppress_results;

	u32 continuous_ranging_session_time;

	int forced_pr_freq;
	u8 ranging_op_class;
	u16 channel_width; /* channel width in MHz (20/40/80/160/320) */
	u8 format_bw;
	u32 center_freq1;
	u32 center_freq2;
	u64 cookie;

	/* Per-session credentials - if set, used instead of stored ones */
	char password[100];
	bool password_valid;
	u8 pmk[PMK_LEN_MAX];
	size_t pmk_len;
};

struct pr_dev_ik {
	struct dl_list list;
	u8 dik[DEVICE_IDENTITY_KEY_LEN];
	char password[100];
	bool password_valid;
	u8 pmk[PMK_LEN_MAX];
	size_t pmk_len;
	bool pmk_valid;
};

/**
 * struct pr_device_info - Proximity ranging peer information
 */
struct pr_device {
	struct dl_list list;
	struct os_reltime last_seen;
	int listen_freq;

	/**
	 * pr_device_addr - PR Device Address of the peer
	 */
	u8 pr_device_addr[ETH_ALEN];

	struct pr_capabilities pr_caps;
	struct edca_capabilities edca_caps;
	struct ntb_capabilities ntb_caps;

	/* Password to be used in PASN-SAE by the Seeker.
	 * This is updated with valid password if DIRA matches for the peer.
	 */
	char password[100];
	bool password_valid;

	/* PMK to be used in PASN-PMK by the Seeker.
	 * This is updated with valid PMK if DIRA matches for the peer.
	 */
	u8 pmk[PMK_LEN_MAX];
	size_t pmk_len;
	bool pmk_valid;

	/* DevIK of the peer resolved via DIRA verification.
	 * Set to the matched dev_ik->dik when pr_validate_dira() succeeds.
	 * Cleared by pr_clear_dev_iks().
	 */
	u8 dik[DEVICE_IDENTITY_KEY_LEN];
	bool dik_valid;

#ifdef CONFIG_PASN
	/* PASN data structure */
	struct pasn_data *pasn;
	struct wpabuf *ranging_wrapper;
	enum pr_pasn_role pasn_role;
#endif /* CONFIG_PASN */

	u8 ranging_role;
	u8 protocol_type;
	u8 final_op_class;
	u8 final_op_channel;
	u8 discovery_type;
};


/**
 * struct pr_message - Proximity ranging peer information
 */
struct pr_message {
	struct wpabuf *pr_attributes;

	u8 pr_device_addr[ETH_ALEN];

	const u8 *pr_capability;
	size_t pr_capability_len;

	const u8 *edca_capability;
	size_t edca_capability_len;

	const u8 *ntb_capability;
	size_t ntb_capability_len;

	const u8 *dira;
	size_t dira_len;

	const u8 *status_ie;
	size_t status_ie_len;

	const u8 *op_mode;
	size_t op_mode_len;
};


struct pr_config {
	u8 pasn_type;

	int preferred_ranging_role;

	char country[3];

	u8 dev_addr[ETH_ALEN];

	/**
	 * dev_name - Device Name
	 */
	char *dev_name;

	bool edca_ista_support;

	bool edca_rsta_support;

	/* Best single format_bw value derived from pd_format_bw_bitmap */
	u8 edca_format_and_bw;

	u32 edca_min_ranging_interval;

	u8 max_tx_antenna;

	u8 max_rx_antenna;

	struct pr_channels edca_channels;

	bool ntb_ista_support;

	bool ntb_rsta_support;

	bool concurrent_ista_rsta;

	u32 pmsr_max_peers;

	u32 pr_max_peer_ista_role;

	u32 pr_max_peer_rsta_role;

	u8 max_ftms_per_burst;

	bool secure_he_ltf;

	u8 max_tx_ltf_repetations;

	u8 max_rx_ltf_repetations;

	u8 max_tx_ltf_total;

	u8 max_rx_ltf_total;

	u8 max_rx_sts_le_80;

	u8 max_rx_sts_gt_80;

	u8 max_tx_sts_le_80;

	u8 max_tx_sts_gt_80;

	/* PD ranging preamble and bandwidth bitmaps (shared by EDCA and NTB) */
	u32 pd_preamble_bitmap;
	u32 pd_format_bw_bitmap;

	/* Best single format_bw value derived from pd_format_bw_bitmap */
	u8 ntb_format_and_bw;

	u32 ntb_min_ranging_interval;

	struct pr_channels ntb_channels;

	bool support_6ghz;

	/* Cipher version type */
	int dik_cipher;

	/* Buffer to hold the DevIK */
	u8 dik_data[DEVICE_IDENTITY_KEY_LEN];

	/* Length of DevIK in octets */
	size_t dik_len;

	/* DevIK expiration */
	int expiration;

	/* Global password to be used in PASN-SAE for Advertiser */
	char global_password[100];

	bool global_password_valid;

	/**
	 * cb_ctx - Context to use with callback functions
	 */
	void *cb_ctx;

	/**
	 * pasn_send_mgmt - Function handler to transmit a Management frame
	 * @ctx: Callback context from cb_ctx
	 * @data: Frame to transmit
	 * @data_len: Length of frame to transmit
	 * @noack: No ack flag
	 * @freq: Frequency in MHz for the channel on which to transmit
	 * @wait: How many milliseconds to wait for a response frame
	 * Returns: 0 on success, -1 on failure
	 */
	int (*pasn_send_mgmt)(void *ctx, const u8 *data, size_t data_len,
			      int noack, unsigned int freq, unsigned int wait);

	/**
	 * negotiation_started - Called when PASN negotiation begins
	 * @ctx: Callback context from cb_ctx
	 * @peer_addr: MAC address of the peer
	 * @role: Ranging role (initiator or responder)
	 * @protocol_type: Ranging protocol type
	 *
	 * Fired on the initiator after Auth frame 1 (M1) is sent, and on the
	 * responder after Auth frame 1 (M1) is received and M2 is sent back.
	 */
	void (*negotiation_started)(void *ctx, const u8 *peer_addr, u8 role,
				    u8 protocol_type);

	void (*pasn_result)(void *ctx, u8 role, u8 protocol_type, u8 op_class,
			    u8 op_channel, const char *country);

	int (*set_keys)(void *ctx, const u8 *own_addr, const u8 *peer_addr,
			int cipher, int akmp, struct wpa_ptk *ptk);

	void (*clear_keys)(void *ctx, const u8 *own_addr, const u8 *peer_addr);

	void (*get_ranging_params)(void *ctx, const u8 *dev_addr,
				   const u8 *peer_addr, u8 ranging_role,
				   u8 protocol_type, u8 op_class, u8 op_channel,
				   u8 self_format_bw, u8 peer_format_bw);

	void (*device_found)(void *ctx, const struct pr_device *dev);
};

struct pr_data {
	/**
	 * cfg - PR module configuration
	 *
	 * This is included in the same memory allocation with the
	 * struct pr_data and as such, must not be freed separately.
	 */
	struct pr_config *cfg;

	struct dl_list devices;

	struct dl_list dev_iks;

	/* PMKSA cache for PASN-PMK authentication */
	struct rsn_pmksa_cache *initiator_pmksa;
	struct rsn_pmksa_cache *responder_pmksa;

	/* PR PASN request tracking - similar to pasn_params in wpa_supplicant
	 */
	struct pr_pasn_ranging_params *pr_pasn_params;

	/* Set when final measurement result received; blocks further results */
	bool ranging_final_received;
};

/* PR Device Identity Resolution Attribute parameters */
struct pr_dira {
	/* Cipher version type */
	int cipher_version;
	/* Nonce used in DIRA attribute */
	u8 nonce[DEVICE_IDENTITY_NONCE_LEN];
	/* Length of nonce */
	size_t nonce_len;
	/* Tag computed for nonce using NIK */
	u8 tag[DEVICE_IDENTITY_TAG_LEN];
	/* Length of tag in octets */
	size_t tag_len;
};

struct operation_mode {
	/* Bitmap for Ranging Protocol type */
	u8 protocol_type;

	/* Bitmap for Role-ISTA/RSTA */
	u8 role;

	char country[3];

	struct pr_channels channels;
};

struct pr_data * pr_init(const struct pr_config *cfg);
void pr_flush(struct pr_data *pr);
void pr_deinit(struct pr_data *pr);
void pr_set_dev_addr(struct pr_data *pr, const u8 *addr);
void pr_clear_dev_iks(struct pr_data *pr);
void pr_add_dev_ik(struct pr_data *pr, const u8 *dik, const char *password,
		   const u8 *pmk, size_t pmk_len, bool own);
int pr_set_peer_credentials(struct pr_data *pr, const u8 *addr,
			    const u8 *pmk, size_t pmk_len,
			    const char *password);
struct wpabuf * pr_prepare_usd_elems(struct pr_data *pr, const u8 *src_addr);
void pr_process_usd_elems(struct pr_data *pr, const u8 *ies, u16 ies_len,
			  const u8 *peer_addr, unsigned int freq);
int pr_ensure_oob_peer(struct pr_data *pr, const u8 *addr, int freq);
int pr_initiate_pasn_auth(struct pr_data *pr, const u8 *addr, int freq,
			  u8 auth_mode, u8 ranging_role, u8 ranging_type,
			  int forced_pr_freq);
int pr_pasn_auth_tx_status(struct pr_data *pr, const u8 *data, size_t data_len,
			   bool acked);
int pr_pasn_auth_retransmit(struct pr_data *pr, const u8 *addr);
int pr_pasn_auth_rx(struct pr_data *pr, const struct ieee80211_mgmt *mgmt,
		    size_t len, int freq);

#endif /* PROXIMITY_RANGING_H */
