/*
 * Wi-Fi Aware - Internal definitions for NAN module
 * Copyright (C) 2025 Intel Corporation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef NAN_I_H
#define NAN_I_H

#include "list.h"
#include "common/ieee802_11_defs.h"
#include "common/nan_defs.h"
#include "common/wpa_common.h"
#include "nan.h"

struct bitfield;
struct nan_config;

#define NAN_INVALID_MAP_ID 0xff

#define NAN_KCK_MAX_LEN 24
#define NAN_KEK_MAX_LEN 32
#define NAN_TK_MAX_LEN  32
#define NAN_NPK_LEN  32

#define NAN_ELEMENT_MAX_SIZE 1024

/**
 * struct nan_ptk - NAN Pairwise Transient Key
 * @kck: Key Confirmation Key
 * @kek: Key Encryption Key
 * @tk: Transient Key
 * @kck_len: Length of &kck in octets
 * @kek_len: Length of &kek in octets
 * @tk_len: Length of &tk in octets
 */
struct nan_ptk {
	u8 kck[NAN_KCK_MAX_LEN];
	u8 kek[NAN_KEK_MAX_LEN];
	u8 tk[NAN_TK_MAX_LEN];

	size_t kck_len;
	size_t kek_len;
	size_t tk_len;
};

/**
 * struct nan_ndp_sec - NAN ndp security state
 * @present: Whether NDP setup exchange includes security
 * @valid: Whether the security configuration is valid
 * @replaycnt_ok: Whether replay count ok
 * @replaycnt: Current replay count
 * @i_nonce: Initiator nonce
 * @i_capab: Initiator capabilities
 * @i_csid: Initiator cipher suite ID
 * @i_instance_id: Initiator publish instance ID
 * @i_pmkid: Initiator PMKID
 * @r_nonce: Responder nonce
 * @r_capab: Responder capabilities
 * @r_csid: Responder cipher suite ID
 * @r_instance_id: Responder instance ID
 * @r_pmkid: Responder PMKID
 * @auth_token: Authentication token
 * @pmk: PMK used for the secure NDP establishment
 * @ptk: Derived PTK
 * @local_gtk: Group Temporal Key information of the local NDI
 * @peer_gtk: Group Temporal Key information of the peer NDI
 * @peer_gtk_rsc: Receive sequence counter of the peer NDI GTK
 */
struct nan_ndp_sec {
	bool present;
	bool valid;

	bool replaycnt_ok;
	u8 replaycnt[8];

	/* Initiator data */
	u8 i_nonce[WPA_NONCE_LEN];
	u8 i_capab;
	u8 i_csid;
	u8 i_instance_id;
	u8 i_pmkid[PMKID_LEN];

	/* Responder data */
	u8 r_nonce[WPA_NONCE_LEN];
	u8 r_capab;
	u8 r_csid;
	u8 r_instance_id;
	u8 r_pmkid[PMKID_LEN];

	u8 auth_token[NAN_AUTH_TOKEN_LEN];
	u8 pmk[PMK_LEN];

	struct nan_ptk ptk;

	struct nan_gtk local_gtk;
	struct nan_gtk peer_gtk;
	u8 peer_gtk_rsc[WPA_KEY_RSC_LEN];
};

/*
 * enum nan_ndp_state - State of NDP establishment
 * @NAN_NDP_STATE_NONE: No NDP establishment in progress
 * @NAN_NDP_STATE_START: Starting NDP establishment
 * @NAN_NDP_STATE_REQ_SENT: NDP request was sent
 * @NAN_NDP_STATE_REQ_RECV: NDP response was received and processed
 * @NAN_NDP_STATE_RES_SENT: NDP response was sent and NDP is not accepted yet
 * @NAN_NDP_STATE_RES_RECV: NDP response was received and NDP was not accepted
 *     yet (security is negotiated or confirmation is required)
 * @NAN_NDP_STATE_CON_SENT: NDP confirm was sent and NDP was not done yet, as
 *     security is negotiated
 * @NAN_NDP_STATE_CON_RECV: NDP confirm received and NDP was not done yet, as
 *     security is negotiated
 * @NAN_NDP_STATE_DONE: NDP establishment is done (either success or reject).
 *     In this state the NAN module handles actions such as notification to the
 *     encapsulating logic, etc. Once processing is done the NDP should either
 *     be cleared (rejected) or moved to the list of NDPs associated with the
 *     peer.
 */
enum nan_ndp_state {
	NAN_NDP_STATE_NONE,
	NAN_NDP_STATE_START,
	NAN_NDP_STATE_REQ_SENT,
	NAN_NDP_STATE_REQ_RECV,
	NAN_NDP_STATE_RES_SENT,
	NAN_NDP_STATE_RES_RECV,
	NAN_NDP_STATE_CON_SENT,
	NAN_NDP_STATE_CON_RECV,
	NAN_NDP_STATE_DONE,
};

/*
 * struct nan_ndp - NDP information
 *
 * Used to maintain the NDP as an object in a peer's list of NDPs.
 *
 * @list: Used for linking in the NDPs list
 * @peer: Pointer to the peer data structure
 * @initiator: True iff the local device is the initiator
 * @ndp_id: NDP ID
 * @init_ndi: Initiator NDI
 * @resp_ndi: Responder NDI. Might not always be set (as this depends on the
 *     state of NDP establishment and the status).
 * @qos: QoS requirements for this NDP
 * @gtk_id: GTK key ID used for this NDP; 0 if GTK is not used
 */
struct nan_ndp {
	/* for nan_peer ndps list */
	struct dl_list list;
	struct nan_peer *peer;
	bool initiator;
	u8 ndp_id;
	u8 init_ndi[ETH_ALEN];
	u8 resp_ndi[ETH_ALEN];

	struct nan_qos qos;
	u8 gtk_id;
};

/*
 * struct nan_ndp_setup - Holds the state of the NDP setup
 * @ndp: NDP information
 * @state: Current state
 * @status: Current status
 * @dialog_token: Setup dialog token
 * @publisher_inst_id: Publish function instance ID
 * @conf_req: True iff the NDP exchange requires confirm message
 * @reason: Reject reason. Only valid when status is rejected.
 * @ssi: Service specific information
 * @ssi_len: Service specific information length
 * @service_id: Service ID of the service used for NDP setup
 * @sec: NDP security data
 * @local_interface_id_valid: Indicates whether the &local_interface_id
 *      field is valid.
 * @local_interface_id: The local interface identifier to be used for the NDP
 * @peer_interface_id_valid: Indicates whether the &peer_interface_id
 *      field is valid.
 * @peer_interface_id: The peer interface identifier to be used for the NDP
 */
struct nan_ndp_setup {
	struct nan_ndp *ndp;
	enum nan_ndp_state state;
	enum nan_ndp_status status;
	u8 dialog_token;
	u8 publish_inst_id;
	bool conf_req;
	enum nan_reason reason;
	u8 *ssi;
	u16 ssi_len;

	u8 service_id[NAN_SERVICE_ID_LEN];
	struct nan_ndp_sec sec;

	bool local_interface_id_valid;
	u8 local_interface_id[NAN_NDPE_TLV_IPV6_LINK_LOCAL_LEN];
	bool peer_interface_id_valid;
	u8 peer_interface_id[NAN_NDPE_TLV_IPV6_LINK_LOCAL_LEN];
};

/**
 * struct nan_band_chan - NAN channel/band entry
 *
 * @band_id: Band ID as specified by enum nan_band_entry
 * @chan: Channel entry as specified by &struct nan_chan_entry
 */
struct nan_band_chan {
	union {
		u8 band_id;
		struct nan_chan_entry chan;
	} u;
};

/**
 * enum nan_band_chan_type - NAN band or channel
 *
 * @NAN_TYPE_BAND: The entry is a band entry
 * @NAN_TYPE_CHANNEL: The entry is a channel entry
 */
enum nan_band_chan_type {
	NAN_TYPE_BAND,
	NAN_TYPE_CHANNEL,
};

/* Default availability entry parameter values */
#define NAN_AVAIL_ENTRY_DEF_UTIL NAN_AVAIL_ENTRY_CTRL_UTIL_UNKNOWN
#define NAN_AVAIL_ENTRY_DEF_NSS  2
#define NAN_AVAIL_ENTRY_DEF_PREF 3

/**
 * struct nan_avail_entry - NAN availability entry
 *
 * @list: Used for linking in the availability entries list
 * @map_id: Map ID of the availability attribute that this entry belongs to
 * @type: Availability type. One of NAN_AVAIL_ENTRY_CTRL_TYPE_*.
 * @preference: Preference of being available in the NAN slots specified by
 *	the associated time bitmap. The preference is higher when the value is
 *	set larger. Valid values are 0 - 3.
 * @utilization: Indicating proportion within the NAN slots specified by the
 *	associated time bitmap that are already utilized for other purposes,
 *	quantized to 20%. Valid values are 0 - 5.
 * @rx_nss: Maximum number of special streams the NAN device can receive during
 *	the NAN slots specified by the associated time bitmap
 * @tbm: Time bitmap specifying the NAN slots in which the device will be
 *	available for NAN operations
 * @band_chan_type: Type of entries in &band_chan array, as specified by
 *	enum nan_band_chan_type
 * @n_band_chan: Number of entries in &band_chan array
 * @band_chan: Array of bands/channels on which the NAN device will be
 *	available
 */
struct nan_avail_entry {
	struct dl_list list;
	u8 map_id;
	u8 type;
	u8 preference;
	u8 utilization;
	u8 rx_nss;
	struct nan_time_bitmap tbm;
	enum nan_band_chan_type band_chan_type;
	u8 n_band_chan;
	struct nan_band_chan *band_chan;
};

/**
 * struct nan_dev_capa_entry - NAN Device Capability entry
 *
 * @list: Used for linking in the device capabilities list
 *	(in struct nan_peer_info::dev_capa)
 * @map_id: Map ID of the device capabilities
 * @capa: Device capabilities as specified by &struct nan_device_capabilities
 */
struct nan_dev_capa_entry {
	struct dl_list list;
	u8 map_id;
	struct nan_device_capabilities capa;
};

/**
 * struct nan_elem_container_entry - NAN element container entry
 *
 * @list: Used for linking in the element container entries list
 *	(in struct nan_peer_info::element_container)
 * @map_id: Map ID of the element container
 * @len: Length of data
 * @data: Pointer to the data
 */
struct nan_elem_container_entry {
	struct dl_list list;
	u8 map_id;
	u16 len;
	u8 data[];
};

/**
 * struct nan_ulw_entry - NAN Unaligned Schedule attribute entry
 * @list: Used for linking in the ULW entries list in struct nan_peer_info::ulw
 * @len: Length of the ULW attribute payload
 * @data: Pointer to the ULW attribute payload
 */
struct nan_ulw_entry {
	struct dl_list list;
	u16 len;
	u8 data[];
};

/**
 * struct nan_peer_sec_info_entry - NAN peer security information entry
 *
 * Maintains the latest security information for an NDI pair.
 *
 * @list: Used for linking in the peer security info list
 *	(struct nan_peer_info::sec)
 * @peer_ndi: Peer NDI address
 * @local_ndi: Local NDI address
 * @csid: Cipher Suite ID used for the secure NAN communication
 * @pmk: PMK shared with the peer
 * @pmkid: PMKID shared with the peer
 * @ptk: PTK shared with the peer
 * @pairing_akmp: AKMP used for the pairing (see See WPA_KEY_MGMT_*) or
 * 	zero if PASN pairing was not used for NDP establishment
 */
struct nan_peer_sec_info_entry {
	struct dl_list list;

	u8 peer_ndi[ETH_ALEN];
	u8 local_ndi[ETH_ALEN];

	enum nan_cipher_suite_id csid;
	u8 pmk[PMK_LEN];
	u8 pmkid[PMKID_LEN];
	struct nan_ptk ptk;
	int pairing_akmp;
};

/**
 * struct nan_peer_info - NAN peer information
 *
 * @last_seen: Timestamp of the last update of the peer info
 * @seq_id: Sequence id of the last availability update
 * @avail_entries: List of availability entries of the peer
 * @ulw: List of Unaligned Schedule attribute payloads of the peer
 *	(struct nan_ulw_entry::list entries)
 * @dev_capa: List of device capabilities of the peer
 *	(struct nan_dev_capa_entry::list entries)
 * @element_container: List of element container entries of the peer
 *	(struct nan_elem_container_entry::list entries)
 * @sec: List of security information entries of the peer
 *	(struct nan_peer_sec_info_entry::list entries)
 */
struct nan_peer_info {
	struct os_reltime last_seen;
	u8 seq_id;
	struct dl_list avail_entries;
	struct dl_list ulw;
	struct dl_list dev_capa;
	struct dl_list element_container;
	struct dl_list sec;
};

/**
 * enum nan_ndl_state - State of NDL establishment
 *
 * @NAN_NDL_STATE_NONE: No NDL with the peer
 * @NAN_NDL_STATE_START: NDL setup initiated by local device
 * @NAN_NDL_STATE_REQ_SENT: Sent NDL request
 * @NAN_NDL_STATE_REQ_RECV: Got NDL request
 * @NAN_NDL_STATE_RES_SENT: Sent NDL response
 * @NAN_NDL_STATE_RES_RECV: Got NDL response
 * @NAN_NDL_STATE_CON_SENT: Sent NDL confirm
 * @NAN_NDL_STATE_CON_RECV: Got NDL confirm
 * @NAN_NDL_STATE_DONE: NDL establishment is done (either success or reject).
 */
enum nan_ndl_state {
	NAN_NDL_STATE_NONE,
	NAN_NDL_STATE_START,
	NAN_NDL_STATE_REQ_SENT,
	NAN_NDL_STATE_REQ_RECV,
	NAN_NDL_STATE_RES_SENT,
	NAN_NDL_STATE_RES_RECV,
	NAN_NDL_STATE_CON_SENT,
	NAN_NDL_STATE_CON_RECV,
	NAN_NDL_STATE_DONE,
};

/**
 * enum nan_ndl_setup_reason - NAN NDL setup reason
 * @NAN_NDL_SETUP_REASON_NONE: none
 * @NAN_NDL_SETUP_REASON_NDP: NDL setup request for NDP operation
 */
enum nan_ndl_setup_reason {
	NAN_NDL_SETUP_REASON_NONE,
	NAN_NDL_SETUP_REASON_NDP,
};

/**
 * struct nan_ndl - NAN NDL data
 *
 * @state: Current state
 * @status: Current status
 * @send_naf_on_error: When set, indicates that in case that the NDL processing
 *     returned an error, a NAF still needs to be sent to the peer, i.e., the
 *     error cannot be silently ignored.
 * @reason: In case of status == NAN_NDL_STATUS_REJECTED, indicates the reason.
 * @dialog_token: The dialog token for the current NDL negotiation.
 * @max_idle_period: Indicate a period of time in units of 1024 TU during which
 *     the peer device can refrain from transmitting over the NDL without
 *     being terminated.
 * @setup_reason: The reason for the NDL setup
 * @ndc_id: NDC identifier
 * @peer_qos: Peer QoS requirements
 * @local_qos: Local QoS requirements (for the current NDP establishment)
 * @ndc_sched: The NDC schedule entries. See &struct nan_sched_entry
 * @ndc_sched_len: The length in octets of ndc_sched.
 * @immut_sched: The immutable schedule entries. See &enum nan_sched_entry
 * @immut_sched_len: The length in octets of immut_sched.
 */
struct nan_ndl {
	enum nan_ndl_state state;
	enum nan_ndl_status status;
	u8 send_naf_on_error;
	enum nan_reason reason;

	u8 dialog_token;
	u16 max_idle_period;
	enum nan_ndl_setup_reason setup_reason;

	u8 ndc_id[ETH_ALEN];

	struct nan_qos peer_qos, local_qos;

	u8 *ndc_sched;
	u16 ndc_sched_len;

	u8 *immut_sched;
	u16 immut_sched_len;
};

/**
 * struct nan_bootstrap - NAN bootstrap information
 * @supported_methods: Bitmap of supported bootstrap methods. See
 *     &enum nan_pairing_bootstrapping_method.
 * @initiator: Whether this device is the initiator
 * @requested_pbm: Bitmap of requested bootstrap methods. See
 *     &enum nan_pairing_bootstrapping_method.
 * @dialog_token: Dialog token of the bootstrap exchange
 * @status: Status of the bootstrap exchange. See &enum nan_pba_status.
 * @reason_code: Reason code for the bootstrap exchange. See &enum nan_reason.
 * @comeback_required: Whether the peer requested a comeback
 * @comeback_after: Time after which the comeback is requested
 * @cookie: Pointer to the cookie received from the peer
 * @cookie_len: Length of the cookie
 * @authorized: Authorized bootstrap method. See &enum
 *     nan_pairing_bootstrapping_method.
 * @in_progress: Whether a bootstrap exchange is in progress
 * @handle: Follow-up context handle for the ongoing bootstrap request
 * @req_instance_id: Instance ID of the bootstrap request
 * @npba: The NPBA from the last successful bootstrap
 */
struct nan_bootstrap {
	u16 supported_methods;
	bool initiator;
	u16 requested_pbm;
	u8 dialog_token;
	u8 status;
	u8 reason_code;

	bool comeback_required;
	u16 comeback_after;
	u8 *cookie;
	u8 cookie_len;

	u16 authorized;
	bool in_progress;

	int handle;
	u8 req_instance_id;

	struct wpabuf *npba;
};

/**
 * enum nan_pasn_auth_mode - NAN pairing authentication modes
 * @NAN_PASN_AUTH_MODE_PASN: Unauthenticated PASN
 * @NAN_PASN_AUTH_MODE_SAE: PASN authentication with SAE tunneling
 * @NAN_PASN_AUTH_MODE_PMK: PASN authentication with PMK caching
 */
enum nan_pasn_auth_mode {
	NAN_PASN_AUTH_MODE_PASN = 0,
	NAN_PASN_AUTH_MODE_SAE = 1,
	NAN_PASN_AUTH_MODE_PMK = 2,
};

/**
 * enum nan_pairing_role - NAN pairing role types
 * @NAN_PAIRING_ROLE_IDLE: No active pairing role
 * @NAN_PAIRING_ROLE_INITIATOR: Device acting as pairing initiator
 * @NAN_PAIRING_ROLE_RESPONDER: Device acting as pairing responder
 */
enum nan_pairing_role {
	NAN_PAIRING_ROLE_IDLE,
	NAN_PAIRING_ROLE_INITIATOR,
	NAN_PAIRING_ROLE_RESPONDER,
};


/* Current pairing uses pairing verification */
#define NAN_PAIRING_FLAG_NPK_VERIFICATION BIT(0)
/* Peer is paired */
#define NAN_PAIRING_FLAG_PAIRED BIT(1)

/**
 * struct nan_pairing_peer_data - NAN pairing peer information
 * @pairing_cfg: NAN pairing configuration parameters
 * @self_pairing_role: Role of this device in the pairing process
 * @pasn: Pointer to PASN data
 * @handle: Handle of the local service instance
 * @peer_instance_id: Instance ID of the peer service
 * @nonce_tag_valid: Indicates if the nonce and tag fields are valid
 * @nonce: Nonce from peer's NIRA
 * @tag: Tag from peer's NIRA
 * @flags: Bitmap of pairing flags. See NAN_PAIRING_FLAG_*
 * @pending_auth1: Pending PASN Authentication frame 1 to be processed
 * @pairing_csid: Cipher suite ID used for the pairing
 * @pairing_akmp: AKMP used for the pairing. See WPA_KEY_MGMT_*.
 */
struct nan_pairing_peer_data {
	struct nan_pairing_cfg pairing_cfg;
	enum nan_pairing_role self_pairing_role;
	struct pasn_data *pasn;
	int handle;
	int peer_instance_id;
	bool nonce_tag_valid;
	u8 nonce[NAN_NIRA_NONCE_LEN];
	u8 tag[NAN_NIRA_TAG_LEN];
	u32 flags;
	struct wpabuf *pending_auth1;
	enum nan_cipher_suite_id pairing_csid;
	int pairing_akmp;
};

/**
 * struct nan_peer - Represents a known NAN peer
 * @list: List node for linking peers
 * @nmi_addr: NMI of the peer
 * @configured: Indicates if the peer has been configured to the device
 * @last_seen: Timestamp of the last time this peer was seen
 * @info: Information about the peer
 * @ndps: List of NDPs associated with this peer
 * @ndp_setup: Used to hold an NDP object while NDP establishment is in
 *     progress
 * @ndl: NDL data associated with this peer
 * @bootstrap: Bootstrap information of the peer
 * @pairing: Pairing data associated with this peer
 * @igtk_id: IGTK key ID used with this peer. Zero if IGTK is not used.
 * @bigtk_id: BIGTK key ID used with this peer. Zero if BIGTK is not used.
 */
struct nan_peer {
	struct dl_list list;
	u8 nmi_addr[ETH_ALEN];
	bool configured;
	struct os_reltime last_seen;
	struct nan_peer_info info;

	struct dl_list ndps;

	struct nan_ndp_setup ndp_setup;

	struct nan_ndl *ndl;

	struct nan_bootstrap bootstrap;

	struct nan_pairing_peer_data pairing;

	u8 igtk_id;
	u8 bigtk_id;
};

/**
 * struct nan_data - Internal data structure for NAN
 * @cfg: Pointer to the NAN configuration structure
 * @nan_started: Flag indicating if NAN has been started
 * @sched_update_pending: Local schedule update is pending driver confirmation
 * @peer_list: List of known peers
 * @ndp_id_counter: NDP identifier counter. Incremented for each NDP request,
 *     and is used to set ndp_id in &struct nan_ndp.
 * @next_dialog_token: Dialog token for NDP and NDL negotiations. Incremented
 *     for each NDP and NDL request.
 * @sched: The local schedule
 * @cluster_id: Current cluster ID
 * @nira_nonce: Nonce for NAN Identity Resolution attribute (NIRA)
 * @nira_tag: Tag for NAN Identity Resolution attribute (NIRA)
 * @initiator_pmksa: PMKSA cache for PASN-PMK authentication as an initiator
 * @responder_pmksa: PMKSA cache for PASN-PMK authentication as a responder
 * @igtk: IGTK for NAN secure NDP
 * @igtk_id: Key ID of the IGTK
 * @bigtk: BIGTK for NAN secure NDP
 * @bigtk_id: Key ID of the BIGTK
 */
struct nan_data {
	struct nan_config *cfg;
	u8 nan_started:1;
	u8 sched_update_pending:1;
	struct dl_list peer_list;

	u8 ndp_id_counter;
	u8 next_dialog_token;
	struct nan_schedule sched;

	u8 cluster_id[ETH_ALEN];

	u8 nira_nonce[NAN_NIRA_NONCE_LEN];
	u8 nira_tag[NAN_NIRA_TAG_LEN];

	struct rsn_pmksa_cache *initiator_pmksa;
	struct rsn_pmksa_cache *responder_pmksa;

	struct wpa_igtk igtk;
	u8 igtk_id;

	struct wpa_bigtk bigtk;
	u8 bigtk_id;
};

struct nan_attrs_entry {
	struct dl_list list;
	const u8 *ptr;
	u16 len;
};

struct nan_attrs {
	struct dl_list serv_desc_ext;
	struct dl_list avail;
	struct dl_list ndc;
	struct dl_list ulw;
	struct dl_list dev_capa;
	struct dl_list element_container;

	const u8 *ndp;
	const u8 *ndl;
	const u8 *ndl_qos;
	const u8 *cipher_suite_info;
	const u8 *sec_ctxt_info;
	const u8 *shared_key_desc;
	const u8 *dev_capa_ext;
	const u8 *npba;
	const u8 *nira;
	const u8 *ndpe;

	u16 ndp_len;
	u16 ndl_len;
	u16 ndl_qos_len;
	u16 cipher_suite_info_len;
	u16 sec_ctxt_info_len;
	u16 shared_key_desc_len;
	u16 dev_capa_ext_len;
	u16 npba_len;
	u16 nira_len;
	u16 ndpe_len;
};

struct nan_msg {
	u8 oui_type;
	u8 oui_subtype;
	struct nan_attrs attrs;

	/* The full frame is required for the NDP security flows, that compute
	 * the NDP authentication token over the entire frame body. */
	const struct ieee80211_mgmt *mgmt;
	size_t len;
};


/**
 * nan_get_next_dialog_token - Allocate the next nonzero dialog token
 *
 * Wi-Fi Aware Specification v4.0, Tables 82, 86, 105: Dialog Token must be
 * set to a nonzero value.
 */
static inline u8 nan_get_next_dialog_token(struct nan_data *nan)
{
	if (++nan->next_dialog_token == 0)
		nan->next_dialog_token++;
	return nan->next_dialog_token;
}


/**
 * nan_get_next_ndp_id - Allocate next nonzero NDP identifier
 *
 * Wi-Fi Aware Specification v4.0, Table 82: NDP ID range is 1-255,
 * value zero is reserved.
 */
static inline u8 nan_get_next_ndp_id(struct nan_data *nan)
{
	if (++nan->ndp_id_counter == 0)
		nan->ndp_id_counter++;
	return nan->ndp_id_counter;
}


struct nan_peer * nan_get_peer(struct nan_data *nan, const u8 *addr);
bool nan_is_naf(const struct ieee80211_mgmt *mgmt, size_t len);
int nan_parse_attrs(struct nan_data *nan, const u8 *data, size_t len,
		    struct nan_attrs *attrs);
int nan_parse_naf(struct nan_data *nan, const struct ieee80211_mgmt *mgmt,
		  size_t len, struct nan_msg *msg);
void nan_attrs_clear(struct nan_data *nan, struct nan_attrs *attrs);

int nan_ndp_setup_req(struct nan_data *nan, struct nan_peer *peer,
		      struct nan_ndp_params *params);
int nan_ndp_setup_resp(struct nan_data *nan, struct nan_peer *peer,
		       struct nan_ndp_params *params);
int nan_ndp_handle_ndp_attr(struct nan_data *nan, struct nan_peer *peer,
			    struct nan_msg *msg);
int nan_ndp_add_ndp_attr(struct nan_data *nan, struct nan_peer *peer,
			 struct wpabuf *buf);
void nan_ndp_setup_reset(struct nan_data *nan, struct nan_peer *peer);
void nan_ndp_setup_failure(struct nan_data *nan, struct nan_peer *peer,
			   enum nan_reason reason, bool reset_state);
int nan_ndp_naf_sent(struct nan_data *nan, struct nan_peer *peer,
		     enum nan_subtype subtype);
int nan_parse_device_attrs(struct nan_data *nan, struct nan_peer *peer,
			   const u8 *attrs_data, size_t attrs_len);
int nan_ndp_term_req(struct nan_data *nan, struct nan_peer *peer,
		     struct nan_ndp_id *ndp_id);
int nan_ndl_setup(struct nan_data *nan, struct nan_peer *peer,
		  const struct nan_ndp_params *params, u8 dialog_token);
void nan_ndl_setup_failure(struct nan_data *nan, struct nan_peer *peer,
			   enum nan_reason reason, bool reset_state);
void nan_ndl_reset(struct nan_data *nan, struct nan_peer *peer);
int nan_ndl_handle_ndl_attr(struct nan_data *nan, struct nan_peer *peer,
			    struct nan_msg *msg);
int nan_ndl_add_ndl_attr(struct nan_data *nan, const struct nan_peer *peer,
			 struct wpabuf *buf);
int nan_ndl_add_ndc_attr(struct nan_data *nan, const struct nan_peer *peer,
			 struct wpabuf *buf);
int nan_ndl_add_qos_attr(struct nan_data *nan, const struct nan_peer *peer,
			 struct wpabuf *buf);
int nan_chan_to_chan_idx_map(struct nan_data *nan,
			     u8 op_class, u8 channel, u16 *chan_idx_map);
int nan_ndl_naf_sent(struct nan_data *nan, struct nan_peer *peer,
		     enum nan_subtype subtype);
int nan_ndl_add_avail_attrs(struct nan_data *nan, const struct nan_peer *peer,
			    struct wpabuf *buf);
void nan_ndl_add_elem_container_attr(const struct nan_data *nan,
				     const struct nan_peer *peer,
				     struct wpabuf *buf);
struct bitfield * nan_peer_schedule_intersection(
	struct nan_data *nan, const struct nan_peer *peer,
	const struct nan_schedule *sched);
bool nan_ndl_meets_qos(struct nan_data *nan, const struct nan_peer *peer,
		       const struct bitfield *common_bf);
bool nan_ndl_validate_peer_avail(struct nan_data *nan, struct nan_peer *peer);
int nan_convert_chan_sched_to_bf(struct nan_data *nan,
				 const struct nan_chan_schedule *chan,
				 struct bitfield **avail_bf, u8 *map_id,
				 u8 *op_class, u16 *cbm, u16 *pcbm);
int nan_get_chan_bm(struct nan_data *nan, const struct nan_sched_chan *chan,
		    u8 *op_class, u16 *chan_bm, u16 *pri_chan_bm);
int nan_add_avail_attrs(struct nan_data *nan, u8 sequence_id,
			u32 map_ids_bitmap, u8 type_for_conditional,
			size_t n_chans, struct nan_chan_schedule *chans,
			struct wpabuf *buf, bool include_potential);
void nan_del_avail_entry(struct nan_avail_entry *entry);
void nan_flush_avail_entries(struct dl_list *avail_entries);
int nan_sched_entries_to_avail_entries(struct nan_data *nan,
				       struct dl_list *avail_entries,
				       const u8 *sched_entries,
				       u16 sched_entries_len);
struct bitfield * nan_tbm_to_bf(struct nan_data *nan,
				const struct nan_time_bitmap *tbm);
struct bitfield * nan_sched_to_bf(struct nan_data *nan, struct dl_list *sched,
				  u8 *map_id, enum nan_reason *reason);
bool nan_sched_covered_by_avail_entry(struct nan_data *nan,
				      struct nan_avail_entry *avail,
				      struct bitfield *sched_bf, u8 map_id);
bool nan_sched_covered_by_avail_entries(struct nan_data *nan,
					struct dl_list *avail_entries,
					const u8 *sched, size_t sched_len);
bool nan_sched_bf_covered_by_avail_entries_and_chan(
	struct nan_data *nan, const struct dl_list *avail_entries,
	struct bitfield *sched_bf, u8 map_id, u8 op_class, u16 cbm);
struct bitfield * nan_avail_entries_to_bf(struct nan_data *nan,
					  const struct dl_list *avail_entries,
					  u8 op_class, u16 cbm, u16 pri_cbm);
void nan_ndp_terminated(struct nan_data *nan, struct nan_peer *peer,
			struct nan_ndp_id *ndp_id, const u8 *local_ndi,
			const u8 *peer_ndi, enum nan_reason reason, u8 gtk_id);
int nan_crypto_pmk_to_ptk(const u8 *pmk, const u8 *iaddr, const u8 *raddr,
			  const u8 *inonce, const u8 *rnonce,
			  struct nan_ptk *ptk,
			  enum nan_cipher_suite_id cipher);
int nan_crypto_calc_pmkid(const u8 *pmk, const u8 *iaddr, const u8 *raddr,
			  const u8 *serv_id,
			  enum nan_cipher_suite_id cipher, u8 *pmkid);
int nan_crypto_calc_auth_token(enum nan_cipher_suite_id cipher,
			       const u8 *buf, size_t len, u8 *token);
int nan_crypto_key_mic(const u8 *buf, size_t len, const u8 *kck,
		       size_t kck_len, u8 cipher, u8 *mic);
int nan_crypto_derive_npk(const u8 *kdk, size_t kdk_len,
			  enum nan_cipher_suite_id cipher,
			  const u8 *initiator_nmi, const u8 *responder_nmi,
			  u8 *buf, size_t buf_len);
int nan_crypto_derive_kek(const u8 *kdk, size_t kdk_len,
			  enum nan_cipher_suite_id cipher,
			  const u8 *initiator_nmi, const u8 *responder_nmi,
			  struct wpa_ptk *ptk);
int nan_crypto_derive_nd_pmk_from_kdk(const u8 *kdk, size_t kdk_len,
				      enum nan_cipher_suite_id cipher,
				      const u8 *initiator_nmi,
				      const u8 *responder_nmi, u8 *nd_pmk);
struct wpabuf * nan_crypto_encrypt_key_data(const struct wpabuf *key_data,
					    const u8 *kek, size_t kek_len);
struct wpabuf * nan_crypto_decrypt_key_data(const u8 *kek, size_t kek_len,
					    const u8 *encrypted_data,
					    size_t encrypted_len);
void nan_sec_reset(struct nan_data *nan, struct nan_ndp_sec *ndp_sec);
int nan_sec_rx(struct nan_data *nan, struct nan_peer *peer,
	       struct nan_msg *msg);
int nan_add_csia(struct wpabuf *buf, u8 capab, size_t cs_list_len,
		 const struct nan_cipher_suite *cs_list);
int nan_sec_add_attrs(struct nan_data *nan, struct nan_peer *peer,
		      enum nan_subtype subtype, struct wpabuf *buf);
int nan_sec_init_resp(struct nan_data *nan, struct nan_peer *peer);
int nan_sec_pre_tx(struct nan_data *nan, struct nan_peer *peer,
		   struct wpabuf *buf);
bool nan_sec_ndp_store_keys(struct nan_data *nan, struct nan_peer *peer,
			    const u8 *peer_ndi, const u8 *local_ndi);
int nan_sec_get_tk(struct nan_data *nan, struct nan_peer *peer,
		   const u8 *peer_ndi, const u8 *local_ndi,
		   u8 *tk, size_t *tk_len, enum nan_cipher_suite_id *csid);
void nan_add_dev_capa_ext_attr(struct nan_data *nan, struct wpabuf *buf);

void nan_bootstrap_reset(struct nan_data *nan, struct nan_peer *peer);
bool nan_bootstrap_handle_rx(struct nan_data *nan, const u8 *peer_nmi,
			     const u8 *npba, u16 npba_len,
			     const u8 *buf, size_t len,
			     int handle, u8 req_instance_id);
int nan_add_nira(struct wpabuf *buf, const u8 *tag, const u8 *nonce);
void nan_parse_peer_dev_capa_ext(struct nan_data *nan, struct nan_peer *peer,
				 struct nan_attrs *attrs);
int nan_configure_peer_schedule(struct nan_data *nan, struct nan_peer *peer,
				const struct nan_schedule *local_sched);
bool nan_is_ndpe_supported(struct nan_data *nan, const struct nan_peer *peer);
void nan_add_kde_hdr(struct wpabuf *buf, u32 kde, size_t data_len);
int nan_clear_peer_schedule(struct nan_data *nan, struct nan_peer *peer);
#ifdef CONFIG_PASN
int nan_nira_get_tag_nonce(const struct nan_config *nan, u8 *nonce, u8 *tag);
void nan_pairing_deinit_peer(struct nan_peer *peer);
bool nan_pairing_followup_rx(struct nan_data *nan_data, const u8 *peer_addr,
			     const struct nan_shared_key *shared_key_descr,
			     size_t attr_len);
#else /* CONFIG_PASN */
static inline
int nan_nira_get_tag_nonce(const struct nan_config *nan, u8 *nonce, u8 *tag)
{
	return -1;
}

static inline void nan_pairing_deinit_peer(struct nan_peer *peer)
{
}
#endif /* CONFIG_PASN */

#endif /* NAN_I_H */
