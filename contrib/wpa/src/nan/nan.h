/*
 * Wi-Fi Aware - NAN module
 * Copyright (C) 2025 Intel Corporation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef NAN_H
#define NAN_H

#include "common/nan_defs.h"
#include "common/wpa_common.h"
#include "utils/list.h"

struct nan_cluster_config;
enum nan_reason;
struct ieee80211_mgmt;

struct nan_de_pmkid {
	struct dl_list list;
	u8 pmkid[PMKID_LEN];
};

/*
 * struct nan_device_capabilities - NAN device capabilities
 * @cdw_info: Committed DW information
 * @supported_bands: Supported bands
 * @op_mode: Operation mode
 * @n_antennas: Number of antennas
 * @channel_switch_time: Maximal channel switch time
 * @capa: Device capabilities
 */
struct nan_device_capabilities {
	u16 cdw_info;
	u8 supported_bands;
	u8 op_mode;
	u8 n_antennas;
	u16 channel_switch_time;
	u8 capa;
};

/**
 * struct nan_qos - NAN QoS requirements
 * @min_slots: Minimal number of slots
 * @max_latency: Maximum allowed NAN slots between every two non-contiguous
 *     NAN Data Link (NDL) Common Resource Blocks (CRB)
 */
struct nan_qos {
	u8 min_slots;
	u16 max_latency;
};

/**
 * enum nan_ndp_action - NDP action
 * @NAN_NDP_ACTION_REQ: Request NDP establishment
 * @NAN_NDP_ACTION_RESP: Response to NDP establishment request
 * @NAN_NDP_ACTION_CONF: Confirm NDP establishment
 * @NAN_NDP_ACTION_TERM: Request NDP termination
 */
enum nan_ndp_action {
	NAN_NDP_ACTION_REQ,
	NAN_NDP_ACTION_RESP,
	NAN_NDP_ACTION_CONF,
	NAN_NDP_ACTION_TERM,
};

/**
 * struct nan_ndp_id - Unique identifier of an NDP
 *
 * @peer_nmi: Peer NAN Management Interface (NMI)
 * @init_ndi: Initiator NAN Data Interface (NDI)
 * @id: NDP identifier
 */
struct nan_ndp_id {
	u8 peer_nmi[ETH_ALEN];
	u8 init_ndi[ETH_ALEN];
	u8 id;
};

/*
 * The maximal period of a NAN schedule is 8192 TUs. With time slots of 16 TUs,
 * need 64 octets to represent a complete schedule bitmap.
 */
#define NAN_MAX_PERIOD_TUS        8192
#define NAN_MAX_TIME_BITMAP_SLOTS (NAN_MAX_PERIOD_TUS / 16)
#define NAN_TIME_BITMAP_MAX_LEN   (NAN_MAX_TIME_BITMAP_SLOTS / 8)

/**
 * struct nan_time_bitmap - NAN time bitmap
 *
 * @duration: Slot duration represented by each bit in the bitmap. Valid values
 *     are as defined in nan_defs.h and Wi-Fi Aware spec v4.0, Table 97 (Time
 *     Bitmap Control field format for the NAN Availability attribute).
 * @period: Indicates the repeat interval of the bitmap.
 *     When set to zero, the bitmap is not repeated. Valid values are
 *     as defined in nan_defs.h and Wi-Fi Aware spec v4.0, Table 97 (Time
 *     Bitmap Control field format for the NAN Availability attribute).
 * @offset: The time period specified by the %bitmap field starts at
 *     16 * offset TUs after DW0.
 * @len: Length of the %bitmap field, in bytes. If this is zero, the NAN device
 *     is available for 512 NAN slots beginning after the immediate previous
 *     DW0.
 * @bitmap: Each bit in the time bitmap corresponds to a time duration indicated
 *     by the value of the %duration field. When a bit is set to 1, the NAN
 *     device is available (or conditionally or potentially available)
 *     for any NAN operations for the time associated with the bit.
 */
struct nan_time_bitmap {
	u8 duration;
	u16 period;
	u16 offset;
	u8 len;
	u8 bitmap[NAN_TIME_BITMAP_MAX_LEN];
};

/**
 * struct nan_sched_chan - NAN scheduled channel
 *
 * @freq: Primary channel center frequency of the 20 MHz
 * @center_freq1: Center frequency of the first segment
 * @center_freq2: Center frequency of the second segment, if any
 * @bandwidth: The channel bandwidth in MHz
 */
struct nan_sched_chan {
	int freq;
	int center_freq1;
	int center_freq2;
	int bandwidth;
};

/**
 * struct nan_chan_schedule - NAN channel schedule
 *
 * @chan: The channel associated with the schedule
 * @committed: Committed schedule time bitmap
 * @conditonal: Conditional schedule time bitmap
 * @map_id: The map_id of the availability attribute where this schedule is
 *     represented
 */
struct nan_chan_schedule {
	struct nan_sched_chan chan;
	struct nan_time_bitmap committed;
	struct nan_time_bitmap conditional;
	u8 map_id;
};

/**
 * struct nan_sched_qos - QoS requirements in units of 16 TUs per 512 TUs
 *
 * @required_slots: Number of required slots
 * @min_slots: Minimum number of CRB slots needed for this NDL. If this amount
 *     of CRB slots can't be scheduled the NDL should fail.
 * @max_gap: Maximum allowed latency (in slots) of the CRB schedule
 */
struct nan_sched_qos {
	u8 required_slots;
	u8 min_slots;
	u8 max_gap;
};

#define NAN_SCHEDULE_MAX_CHANNELS 6

/**
 * struct nan_schedule - NAN schedule
 *
 * @map_ids_bitmap: Bitmap of map IDs included in this schedule. Not all map IDs
 *    are covered by &chans. For map IDs that are not covered, when building
 *    NAFs, NAN availability attributes would be added with potential
 *    availability entries.
 * @n_chans: Number of channels for this schedule.
 * @chans:  The channels included in the schedule. The channels must be sorted
 *     such that the map IDs (in struct nan_chan_schedule) are in ascending
 *     order.
 * @ndc: NDC bitmap schedule
 * @ndc_map_id: The NDC map ID
 * @sequence_id: Schedule sequence ID
 * @elems: Additional elements to be set in an element container attribute
 */
struct nan_schedule {
	u32 map_ids_bitmap;
	u8 n_chans;
	struct nan_chan_schedule chans[NAN_SCHEDULE_MAX_CHANNELS];
	struct nan_time_bitmap ndc;
	u8 ndc_map_id;
	u8 sequence_id;
	struct wpabuf *elems;
};

/**
 * struct nan_gtk - NAN GTK information

 * @gtk: Group Temporal Key (GTK)
 * @id: GTK key ID
 * @csid: GTK Cipher suite ID. See &enum nan_cipher_suite_id
 */
struct nan_gtk {
	struct wpa_gtk gtk;
	u8 id;
	u8 csid;
};

/**
 * struct nan_ndp_sec_params - NAN NDP security parameters
 * @csid: Cipher suite ID
 * @pmk: NAN Pairwise Master Key (PMK)
 * @gtk: Group Temporal Key (GTK) information
 */
struct nan_ndp_sec_params {
	enum nan_cipher_suite_id csid;
	u8 pmk[PMK_LEN];
	struct nan_gtk gtk;
};

/**
 * struct nan_ndp_params - Holds the NDP parameters for setting up or
 * terminating an NDP.
 *
 * @type: The request type. See &enum nan_ndp_action
 * @ndp_id: The NDP identifier
 * @qos: The NDP QoS parameters. In case there is no requirement for
 *     max_latency, max_latency should be set to NAN_QOS_MAX_LATENCY_NO_PREF.
 *     Should be set only with NAN_NDP_ACTION_REQ and NAN_NDP_ACTION_RESP.
 *     Ignored for other types.
 * @sec: NDP security parameters. Should be set only with NAN_NDP_ACTION_REQ
 *     and NAN_NDP_ACTION_RESP. Ignored for other types.
 * @ssi: Service specific information. Should be set only with
 *     NAN_NDP_ACTION_REQ and NAN_NDP_ACTION_RESP. Ignored for other types.
 * @ssi_len: Service specific information length
 * @publish_inst_id: Identifier for the instance of the Publisher function
 *     associated with the data path setup request.
 * @service_id: Service identifier of the service associated with the data path
 *     setup request.
 * @resp_ndi: In case of successful response, the responder's NDI. In case of
 *     response to a counter proposal, the initiator's NDI (the one used with
 *     NAN_NDP_ACTION_REQ).
 * @status: Response status
 * @reason_code: In case of rejected response, the rejection reason.
 * @sched_valid: Indicates whether the schedule field is valid
 * @sched: The NAN schedule associated with the NDP parameters
 * @interface_id: The interface identifier to be used for the NDP. The interface
 *    identifier is used to derive the IPv6 link-local address as specified in
 *    Wi-Fi Aware specification v4.0, Table 90 (IPv6 Link Local TLV format).
 */
struct nan_ndp_params {
	enum nan_ndp_action type;

	struct nan_ndp_id ndp_id;
	struct nan_qos qos;
	struct nan_ndp_sec_params sec;
	const u8 *ssi;
	u16 ssi_len;

	union {
		struct nan_ndp_setup_req {
			u8 publish_inst_id;
			u8 service_id[NAN_SERVICE_ID_LEN];
		} req;

		/*
		 * Used with both NAN_NDP_ACTION_RESP (as a response to an NDP
		 * request) and NAN_NDP_ACTION_CONF (as a response to an NDP
		 * response with a counter).
		 */
		struct nan_ndp_setup_resp {
			u8 resp_ndi[ETH_ALEN];
			u8 status;
			u8 reason_code;
		} resp;
	} u;

	bool sched_valid;
	struct nan_schedule sched;
	const u8 *interface_id;
};

/**
 * struct nan_channel_info - Channel information for NAN channel selection
 * @op_class: Operating class
 * @channel: Control channel index
 * @pref: Channel Preference (higher is preferred). Valid values are 0-3.
 */
struct nan_channel_info {
	u8 op_class;
	u8 channel;
	u8 pref;
};

/**
 * struct nan_channels - Array of channel information entries
 *
 * @n_chans: Number of channel information entries
 * @chans: Array of channel information. Sorted by preference.
 */
struct nan_channels {
	unsigned int n_chans;
	struct nan_channel_info *chans;
};

/**
 * struct nan_ndp_connection_params - Parameters for NDP connection
 * @ndp_id: NDP identifier
 * @peer_ndi: Peer NDI MAC address
 * @local_ndi: Local NDI MAC address
 * @ssi: Service specific information
 * @ssi_len: Service specific information length
 * @install_keys: Whether the new keys should be installed
 * @first_ndp: Whether this is the first NDP with the peer
 * @new_ndi_sta: Whether a new NDI station needs to be added (peer_ndi not
 * 	already used by another NDP with this peer)
 * @interface_id: The interface identifier to be used by the peer for the NDP
 * @local_gtk: Pointer to local GTK info. NULL if local GTK is
 *	not to be installed
 * @peer_gtk: Pointer to peer GTK info. NULL if peer GTK is
 *	not to be installed
 * @peer_gtk_rsc: Pointer to the peer GTK receive sequence counter
 */
struct nan_ndp_connection_params {
	struct nan_ndp_id ndp_id;
	const u8 *peer_ndi;
	const u8 *local_ndi;
	const u8 *ssi;
	size_t ssi_len;
	bool install_keys;
	bool first_ndp;
	bool new_ndi_sta;
	const u8 *interface_id;

	const struct nan_gtk *local_gtk;
	const struct nan_gtk *peer_gtk;
	const u8 *peer_gtk_rsc;
};

/**
 * struct nan_ndp_action_notif_params - Parameters for NDP action notification
 * @ndp_id: NDP identifier
 * @is_request: Whether the data is associated with an NDP request frame (true)
 *     or with an NDP response (false).
 * @ndp_status: NDP status
 * @ndl_status: NDL status
 * @publish_inst_id: Identifier for the publish instance function
 * @ssi: Service specific information
 * @ssi_len: Service specific information length
 * @csid: NAN cipher suite identifier
 * @pmkid: NAN PMK identifier; can be NULL if security is not negotiated
 */
struct nan_ndp_action_notif_params {
	struct nan_ndp_id ndp_id;
	bool is_request;

	enum nan_ndp_status ndp_status;
	enum nan_ndl_status ndl_status;

	u8 publish_inst_id;
	const u8 *ssi;
	size_t ssi_len;
	enum nan_cipher_suite_id csid;
	const u8 *pmkid;
};

#define NAN_MAX_MAPS 8
#define NAN_MAX_CHAN_ENTRIES 16

/**
 * struct nan_peer_schedule - NAN peer schedule information
 * @n_maps: Number of maps
 * @maps: Array of maps
 * @map_id: Map ID
 * @n_chans: Number of channels in the map
 * @chans: Array of channels in the map
 * @committed: Committed schedule bitmap for the channel
 * @rx_nss: Number of spatial streams supported by the peer for RX on this
 *     channel
 * @chan: Channel information
 * @tbm: Time bitmap for the channel
 * @ndc: NDC time bitmap for the map
 * @immutable: Immutable time bitmap for the map
 * @max_idle_period: Maximal NDL idle period in seconds that the peer indicated
 */
struct nan_peer_schedule {
	u8 n_maps;
	struct nan_map {
		u8 map_id;
		u8 n_chans;
		struct nan_map_chan{
			bool committed;
			u8 rx_nss;
			struct nan_sched_chan chan;
			struct nan_time_bitmap tbm;
		} chans[NAN_MAX_CHAN_ENTRIES];

		struct nan_time_bitmap ndc;
		struct nan_time_bitmap immutable;
	} maps[NAN_MAX_MAPS];

	u16 max_idle_period;
};

/**
 * struct nan_peer_potential_avail - NAN peer potential availability
 * @n_maps: Number of maps
 * @maps: Array of maps
 * @is_band: Indicates whether the entries are bands (true) or channels (false)
 * @preference: Preference value for the availability entry
 * @utilization: Utilization value for the availability entry
 * @rx_nss: Number of spatial streams supported by the peer for RX during
 *     the time indicated by the availability entry
 * @n_band_chan: Number of band/channel entries
 * @entries: Array of band/channel entries
 */
struct nan_peer_potential_avail {
	unsigned int n_maps;
	struct pot_entry {
		bool is_band;
		u8 preference;
		u8 utilization;
		u8 rx_nss;

		u8 n_band_chan;
		union pot_band_chan{
			u8 band_id;
			struct {
				u8 op_class;
				u16 chan_bitmap;
			};
		} entries[NAN_MAX_CHAN_ENTRIES];
	} maps[NAN_MAX_MAPS];
};

#define NAN_PAIRING_PASN_128  BIT(0)
#define NAN_PAIRING_PASN_256  BIT(1)

/**
 * struct nan_pairing_cfg - NAN pairing configuration parameters
 * @pairing_setup: Whether pairing setup is enabled
 * @npk_caching: Whether NPK caching is enabled
 * @pairing_verification: Whether pairing verification is enabled
 * @cipher_suites: Bitmap of supported cipher suites (NAN_PAIRING_PASN_*)
 */
struct nan_pairing_cfg {
	bool pairing_setup;
	bool npk_caching;
	bool pairing_verification;
	u32 cipher_suites;
};

struct nan_config {
	void *cb_ctx;
	u8 nmi_addr[ETH_ALEN];

	struct nan_device_capabilities dev_capa;

	/* Wi-Fi Aware spec v4.0, Table 141 (Capability Info field) */
	u8 dev_capa_ext_reg_info; /* NAN_DEV_CAPA_EXT_INFO_0_* */

	struct nan_pairing_cfg pairing_cfg;
	u8 nik[NAN_NIK_LEN];

	/* in seconds */
	u32 nik_lifetime;

	/*
	 * The local maximal NDL idle period in seconds. This value should be
	 * set in the NDL attribute included in NAFs to indicate to the peers
	 * that the NDL (and all corresponding NDPs) may be terminated if there
	 * is no data traffic with the peer for max_ndl_idle_period seconds.
	 */
	u16 max_ndl_idle_period;

	/*
	 * Supported Pairing Bootstrapping Methods (PBM).
	 * See Wi-Fi Aware spec v4.0, Table 128 (NPBA format).
	 */
	u16 supported_bootstrap_methods;

	/* Auto-accepted bootstrapping methods.
	 * See Wi-Fi Aware spec v4.0, Table 128 (NPBA format). */
	u16 auto_accept_bootstrap_methods;

	/*
	 * Bootstrap comeback timeout in TUs. This value is used to indicate to
	 * the peer NAN device requesting bootstrapping to be performed, when
	 * to send the bootstrapping request again.
	 */
	u16 bootstrap_comeback_timeout;

	/* Security capabilities. See Wi-Fi Aware spec v4.0, Table 122 (Cipher
	 * Suite Information attribute (CSIA) field format), Capabilities field.
	 */
	u8 security_capab;

	/**
	 * start - Start NAN
	 * @ctx: Callback context from cb_ctx
	 * @config: NAN cluster configuration
	 */
	int (*start)(void *ctx, const struct nan_cluster_config *config);

	/**
	 * stop - Stop NAN
	 * @ctx: Callback context from cb_ctx
	 */
	void (*stop)(void *ctx);

	/**
	 * update_config - Update NAN configuration
	 * @ctx: Callback context from cb_ctx
	 * @config: NAN cluster configuration
	 */
	int (*update_config)(void *ctx,
			     const struct nan_cluster_config *config);

	/**
	 * ndp_action_notif - Notify NDP action is required
	 * @ctx: Callback context from cb_ctx
	 * @params: NDP action notification parameters
	 *
	 * A notification sent when an NDP establishment frame is received, and
	 * upper layer input is required to continue the flow.
	 */
	void (*ndp_action_notif)(void *ctx,
				 struct nan_ndp_action_notif_params *params);

	/**
	 * ndp_connected - Notify that NDP was successfully connected
	 * @ctx: Callback context from cb_ctx
	 * @params: NDP connection parameters
	 * Returns: 0 on success, negative on failure. Note that new NDPs
	 * may trigger security upgrade for the peer NDI station. If this fails,
	 * -2 is returned and the caller should clean up all the existing NDPs
	 * with this peer NDI.
	 */
	int (*ndp_connected)(void *ctx,
			     struct nan_ndp_connection_params *params);

	/**
	 * ndp_disconnected - Notify that NDP was disconnected
	 * @ctx: Callback context from cb_ctx
	 * @ndp_id: NDP identifier
	 * @local_ndi: Local NDI MAC address
	 * @peer_ndi: Peer NDI MAC address
	 * @reason: Disconnection reason
	 * @locally_generated: true if the disconnection was locally generated,
	 *     false if triggered by the peer
	 * @remove_sta: true if the NDI station should be removed (no other NDPs
	 *     using the same peer NDI)
	 * @failure: true if NDP setup failed (before connected), false if
	 *     graceful disconnection after NDP was established
	 * @gtk_id: GTK key ID used for the NDP; 0 if no GTK should be removed
	 *
	 * This callback notifies that an NDP has been disconnected. When
	 * @failure is true, NDP setup failed before connection was established.
	 * When @failure is false, it indicates graceful termination after NDP
	 * was successfully connected.
	 */
	void (*ndp_disconnected)(void *ctx, struct nan_ndp_id *ndp_id,
				 const u8 *local_ndi, const u8 *peer_ndi,
				 enum nan_reason reason,
				 bool locally_generated, bool remove_sta,
				 bool failure, u8 gtk_id);

	/**
	 * get_chans - Get the prioritized allowed channel information to be
	 * used for building the potential availability entries associated with
	 * the given map ID.
	 *
	 * @ctx: Callback context from cb_ctx
	 * @map_id: Map ID of the availability attribute for which the channels
	 *     are requested.
	 * @chans: Pointer to a nan_channels structure that should be filled
	 *     with the prioritized frequencies. On successful return the
	 *     channels should be sorted having the higher priority channels
	 *     first.
	 * Returns: 0 on success, -1 on failure.
	 *
	 * Note: The callback is responsible for allocating chans->chans as
	 * needed. The caller (the NAN module) is responsible for freeing the
	 * memory allocated for the chans->chans.
	 *
	 * Note: The callback should add all channels that are considered valid
	 * for use by the NAN module for the given map.
	 */
	int (*get_chans)(void *ctx, u8 map_id, struct nan_channels *chans);

	/**
	 * send_naf - Transmit a NAN Action frame
	 * @ctx: Callback context from cb_ctx
	 * @dst: Destination MAC address
	 * @src: Source MAC address. Can be NULL.
	 * @cluster_id: The cluster ID
	 * @buf: Frame body (starting from the Category field)
	 * Returns: 0 on success, -1 on failure
	 */
	int (*send_naf)(void *ctx, const u8 *dst, const u8 *src,
			const u8 *cluster_id, struct wpabuf *buf);

	/**
	 * is_valid_publish_id - Check if a publish instance ID is valid
	 * @ctx: Callback context from cb_ctx
	 * @instance_id: The instance ID to check
	 * @service_id: On return, holds the service ID if the instance ID is
	 *	valid
	 * Returns: true if there is a local publish service ID with the given
	 * instance ID; false otherse
	 */
	bool (*is_valid_publish_id)(void *ctx, u8 instance_id, u8 *service_id);

	/**
	 * set_peer_schedule - Configure peer schedule
	 * @ctx: Callback context from cb_ctx
	 * @nmi_addr: NAN Management Interface address of the peer
	 * @new_sta: Indicates whether this is a new STA (true) or an existing
	 *     STA that is being re-configured (false)
	 * @cdw: Committed DW information (from device capabilities)
	 * @sequence_id: Schedule sequence ID
	 * @max_channel_switch_time: Maximum channel switch time
	 * @sched: Peer schedule information; can be NULL
	 * @ulw_elems: ULW elements buffer; can be NULL
	 * Returns: 0 on success, -1 on failure
	 */
	int (*set_peer_schedule)(void *ctx, const u8 *nmi_addr, bool new_sta,
				 u16 cdw, u8 sequence_id,
				 u16 max_channel_switch_time,
				 const struct nan_peer_schedule *sched,
				 const struct wpabuf *ulw_elems);
	/**
	 * bootstrap_request - Notify about received bootstrap request
	 * @ctx: Callback context from cb_ctx
	 * @peer_nmi: Peer NMI address
	 * @pbm: Pairing Bootstrapping Methods from the request. As defined in
	 *     Wi-Fi Aware spec v4.0, Table 128 (NPBA format).
	 * @handle: Service handle
	 * @requestor_instance_id: Requestor instance ID
	 */
	void (*bootstrap_request)(void *ctx, const u8 *peer_nmi, u16 pbm,
				  int handle, u8 requestor_instance_id);

	/**
	 * bootstrap_completed - Notify about completed bootstrap
	 * @ctx: Callback context from cb_ctx
	 * @peer_nmi: Peer NMI address
	 * @pbm: Pairing Bootstrapping Method used. As defined in Wi-Fi Aware
	 *     spec v4.0, Table 128 (NPBA format).
	 * @success: Whether bootstrap was successful
	 * @reason_code: Reason code for failure (0 if success is true)
	 * @handle: Service handle
	 * @requestor_instance_id: Requestor instance ID
	 */
	void (*bootstrap_completed)(void *ctx, const u8 *peer_nmi, u16 pbm,
				    bool success, u8 reason_code,
				    int handle, u8 requestor_instance_id);

	/**
	 * transmit_followup - Transmit Follow-up message to the peer
	 * @ctx: Callback context from cb_ctx
	 * @peer_nmi: Peer NMI address
	 * @attrs: Attributes to include in the Follow-up message
	 * @handle: Service handle for which the follow-up is sent
	 * @req_instance_id: Peer's service instance ID
	 */
	int (*transmit_followup)(void *ctx, const u8 *peer_nmi,
				 const struct wpabuf *attrs, int handle,
				 u8 req_instance_id);

	/**
	 * get_supported_bootstrap_methods - Get supported bootstrap methods
	 * @ctx: Callback context from cb_ctx
	 * @handle: Service handle for which PBM should have been defined
	 * Returns: Supported Pairing Bootstrapping Methods (PBM) bitfield as
	 * configured for the service or 0 if service is not found.
	 */
	u16 (*get_supported_bootstrap_methods)(void *ctx, int handle);

	/**
	 * send_pasn - Transmit a PASN Authentication frame
	 * @ctx: Callback context from cb_ctx
	 * @data: Frame to transmit
	 * @data_len: Length of frame to transmit
	 * Returns: 0 on success, -1 on failure
	 */
	int (*send_pasn)(void *ctx, const u8 *data, size_t data_len);

	/**
	 * pairing_status_cb - Callback for reporting NAN pairing result
	 * @ctx: Callback context from cb_ctx
	 * @peer_addr: Peer NAN device address
	 * @akmp: AKMP used in the pairing
	 * @cipher: Cipher used in the pairing
	 * @status: Status of the pairing (WLAN_STATUS_* )
	 * @ptk: Derived PTK for the pairing (valid only if status is success)
	 * @nd_pmk: ND-PMK from the pairing (valid only if status is success)
	 * Returns: 0 if status is WLAN_STATUS_SUCCESS and the key was
	 *	installed successfully or status is
	 *	WLAN_STATUS_UNSPECIFIED_FAILURE, -1 otherwise
	 */
	int (*pairing_result_cb)(void *ctx, const u8 *peer_addr, int akmp,
				 int cipher, u16 status, struct wpa_ptk *ptk,
				 const u8 *nd_pmk);

	/**
	 * update_pairing_credentials - Report received NIK and NPK for a peer
	 * @ctx: Callback context from cb_ctx
	 * @nik: NAN Identity Key received from peer
	 * @nik_len: Length of the NIK
	 * @cipher_ver: Cipher version of the NIK
	 * @nik_lifetime: Lifetime of the NIK in seconds
	 * @akmp: AKMP suite used to establish the NPKSA
	 * @npk: The NPK associated with the received NIK
	 * @npk_len: Length of the NPK
	 * Returns: 0 on success, -1 on failure
	 */
	int (*update_pairing_credentials)(void *ctx, const u8 *nik,
					  size_t nik_len, int cipher_ver,
					  int nik_lifetime, int akmp,
					  const u8 *npk, size_t npk_len);

	/**
	 * get_npk_akmp - Get the cached NPK and AKMP for a peer
	 * @ctx: Callback context from cb_ctx
	 * @peer_nmi: Peer NMI address
	 * @nonce: Nonce from the peer's NIRA
	 * @tag: Tag from the peer's NIRA
	 * @akmp: On success, set to the AKMP suite used to establish the NPKSA
	 * Returns: The NPK on success, NULL on failure
	 */
	const struct wpabuf * (*get_npk_akmp)(void *ctx, const u8 *peer_nmi,
					      const u8 *nonce, const u8 *tag,
					      int *akmp);

	/**
	 * pairing_request - Notify about received pairing request
	 * @ctx: Callback context from cb_ctx
	 * @peer_nmi: Peer NMI address
	 * @csid: Cipher suite ID requested by the peer
	 * @instance_id: Service instance ID for which the pairing is requested
	 * @rsn_data: Parsed RSNE data from peer's Authentication frame
	 */
	void (*pairing_request)(void *ctx, const u8 *peer_nmi, u8 csid,
				u8 instance_id,
				const struct wpa_ie_data *rsn_data);

	/**
	 * set_group_key - Install a group key
	 * @ctx: Callback context from cb_ctx
	 * @alg: Encryption algorithm (WPA_ALG_* )
	 * @addr: Address of the peer STA for Rx group keys, ff:ff:ff:ff:ff:ff
	 *	for Tx keys; when clearing keys, %NULL is used to indicate that
	 *	both the broadcast-only and default key of the specified key
	 *	index is to be cleared
	 * @key_idx: Key index
	 * @seq: Packet number, the next packet number to be used for in replay
	 *	protection; %NULL if not set
	 * @key: Key buffer
	 * @key_len: Length of the key buffer in octets
	 * @key_flags: bitwise OR of KEY_FLAG_*
	 * Returns: 0 on success, -1 on failure
	 */
	int (*set_group_key)(void *ctx, enum wpa_alg alg, const u8 *addr,
			     int key_idx, const u8 *seq,
			     const u8 *key, size_t key_len,
			     enum key_flag key_flags);

	/**
	 * get_seqnum - Get the current PN for a group key
	 * @ctx: Callback context from cb_ctx
	 * @key_idx: Key index
	 * @seq: Buffer for returning the latest used PN value
	 * @ndi_addr: For NDI group keys, the NDI MAC address; %NULL for
	 *	NMI group keys
	 * Returns: 0 on success, -1 on failure
	 */
	int (*get_seqnum)(void *ctx, int key_idx, u8 *seq, const u8 *ndi_addr);

	/**
	 * get_peer_inactivity - Get the inactivity time for a peer
	 * @ctx: Callback context from cb_ctx
	 * @local_ndi: Local NDI address
	 * @peer_ndi: Peer NDI address
	 * Returns: Peer inactivity in seconds, negative value on failure
	 */
	int (*get_peer_inactivity)(void *ctx, const u8 *local_ndi,
				   const u8 *peer_ndi);

	/**
	 * schedule_changed - Notify about peer schedule change
	 * @ctx: Callback context from cb_ctx
	 * @peer_nmi: Peer NMI address
	 */
	void (*schedule_changed)(void *ctx, const u8 *peer_nmi);
};

struct nan_data * nan_init(const struct nan_config *cfg);
void nan_deinit(struct nan_data *nan);
int nan_start(struct nan_data *nan, const struct nan_cluster_config *config);
int nan_update_config(struct nan_data *nan,
		      const struct nan_cluster_config *config);
void nan_set_cdw_overwrite(struct nan_data *nan, int map_id_2g, int map_id_5g);
void nan_stop(struct nan_data *nan);
void nan_flush(struct nan_data *nan);

int nan_add_peer(struct nan_data *nan, const u8 *addr,
		 const u8 *device_attrs, size_t device_attrs_len);
bool nan_process_followup(struct nan_data *nan, const u8 *addr, const u8 *buf,
			  size_t len, u8 req_instance_id, int handle);
int nan_bootstrap_request(struct nan_data *nan, int handle,
			  const u8 *peer_addr, u8 req_instance_id, u16 pbm,
			  bool auth);
int nan_bootstrap_peer_reset(struct nan_data *nan, const u8 *peer_nmi);
int nan_bootstrap_get_supported_methods(struct nan_data *nan,
					const u8 *peer_nmi,
					u16 *supported_methods);

bool nan_publish_instance_id_valid(struct nan_data *nan, u8 instance_id,
				   u8 *service_id);
void nan_set_cluster_id(struct nan_data *nan, const u8 *cluster_id);
int nan_action_rx(struct nan_data *nan, const struct ieee80211_mgmt *mgmt,
		  size_t len);
int nan_tx_status(struct nan_data *nan, const u8 *dst, const u8 *data,
		  size_t data_len, bool acked);
int nan_handle_ndp_setup(struct nan_data *nan, struct nan_ndp_params *params);
struct nan_device_capabilities *
nan_peer_get_device_capabilities(struct nan_data *nan, const u8 *addr,
				 u8 map_id);
int nan_peer_get_tk(struct nan_data *nan, const u8 *addr,
		    const u8 *peer_ndi, const u8 *local_ndi,
		    u8 *tk, size_t *tk_len, enum nan_cipher_suite_id *csid);
int nan_peer_get_schedule_info(struct nan_data *nan, const u8 *addr,
			       struct nan_peer_schedule *sched);
int nan_peer_dump_sched_to_buf(struct nan_peer_schedule *sched,
			       char *buf, size_t buflen);
int nan_peer_get_pot_avail(struct nan_data *nan, const u8 *addr,
			   struct nan_peer_potential_avail *pot_avail);
int nan_peer_dump_pot_avail_to_buf(struct nan_peer_potential_avail *pot_avail,
				   char *buf, size_t buflen);
const struct nan_pairing_cfg * nan_peer_get_pairing_cfg(struct nan_data *nan,
							const u8 *addr,
							const u8 **nonce,
							const u8 **tag);
int nan_convert_sched_to_avail_attrs(struct nan_data *nan, u8 sequence_id,
				     u32 map_ids_bitmap,
				     size_t n_chans,
				     struct nan_chan_schedule *chans,
				     struct wpabuf *buf,
				     bool include_potential);
void nan_local_sched_update(struct nan_data *nan, struct nan_schedule *sched);
void nan_set_sched_update_pending(struct nan_data *nan, bool pending);
bool nan_peer_pairing_supported(struct nan_data *nan, const u8 *addr);
bool nan_peer_npk_nik_caching_supported(struct nan_data *nan, const u8 *addr);
int nan_get_peer_ndc_freq(struct nan_data *nan,
			  const struct nan_peer_schedule *peer_sched,
			  u8 map_idx);
int nan_crypto_derive_nd_pmk(const char *pwd, const u8 *service_id,
			     enum nan_cipher_suite_id csid,
			     const u8 *peer_nmi, u8 *nd_pmk);
int nan_crypto_pmkid_list(struct dl_list *pmkid_list, const u8 *raddr,
			  const u8 *srv_id, const int *cipher_suites_list,
			  const u8 *pmk);
void nan_crypto_clear_pmkid_list(struct dl_list *pmkid_list);
void nan_add_dev_capa_attr(struct nan_data *nan, struct wpabuf *buf);
int nan_peer_del_all_ndps(struct nan_data *nan, const u8 *addr);
int nan_get_chan_entry(struct nan_data *nan, const struct nan_sched_chan *chan,
		       struct nan_chan_entry *chan_entry);
int nan_get_peer_elems(struct nan_data *nan, const u8 *addr, u8 **elems);
int nan_set_bootstrap_configuration(struct nan_data *nan,
				    u16 supported_bootstrap_methods,
				    u16 auto_accept_bootstrap_methods,
				    u16 bootstrap_comeback_timeout);
struct wpabuf * nan_crypto_derive_nira_tag(const u8 *nik, size_t nik_len,
					   const u8 *nmi_addr,
					   const u8 *nira_nonce);
int nan_ndp_requested_gtk_csid(struct nan_data *nan,
			       const struct nan_ndp_id *ndp_id);
int nan_set_mgmt_group_cipher(struct nan_data *nan, int cipher);
int nan_set_beacon_prot(struct nan_data *nan, bool enable);
int nan_set_max_ndl_idle_period(struct nan_data *nan, u16 max_idle_period);
bool nan_has_active_ndp(struct nan_data *nan);
int nan_get_status(struct nan_data *nan, char *buf, size_t buflen);
int nan_peer_dump_ndps_to_buf(struct nan_data *nan, const u8 *addr,
			      char *buf, size_t buflen);
void nan_terminate_ndi_ndps(struct nan_data *nan, const u8 *ndi_addr);

#ifdef CONFIG_PASN
int nan_pairing_add_attrs(struct nan_data *nan_data, struct wpabuf *buf);
int nan_pairing_initiate_pasn_auth(struct nan_data *nan_data, const u8 *addr,
				   u8 auth_mode, int cipher, int handle,
				   u8 peer_instance_id, bool responder,
				   const char *password,
				   const struct nan_schedule *sched);
int nan_pairing_pasn_auth_tx_status(struct nan_data *nan, const u8 *data,
				    size_t data_len, bool acked);
int nan_pairing_auth_rx(struct nan_data *nan_data,
			const struct ieee80211_mgmt *mgmt, size_t len);
int nan_pairing_set_pairing_setup(struct nan_data *nan_data, bool value);
int nan_pairing_set_npk_caching(struct nan_data *nan_data, bool value);
int nan_pairing_set_pairing_verification(struct nan_data *nan_data, bool value);
int nan_pairing_set_cipher_suites(struct nan_data *nan_data, u32 value);
int nan_pairing_set_nik(struct nan_data *nan, const u8 *nik, size_t nik_len);
int nan_pairing_set_nik_lifetime(struct nan_data *nan, u32 lifetime);
bool nan_pairing_is_peer_paired(struct nan_data *nan_data, const u8 *peer_addr);
int nan_pairing_abort(struct nan_data *nan_data, const u8 *peer_addr);
void nan_pairing_unpair_peer(struct nan_data *nan_data, const u8 *peer_addr);
#else /* CONFIG_PASN */
static inline int nan_pairing_add_attrs(struct nan_data *nan_data,
					struct wpabuf *buf)
{
	return 0;
}

static inline
int nan_pairing_initiate_pasn_auth(struct nan_data *nan_data, const u8 *addr,
				   u8 auth_mode, int cipher, int handle,
				   u8 peer_instance_id, bool responder,
				   const char *password,
				   const struct nan_schedule *sched)
{
	return -1;
}

static inline int nan_pairing_pasn_auth_tx_status(struct nan_data *nan,
						  const u8 *data,
						  size_t data_len, bool acked)
{
	return -1;
}

static inline int nan_pairing_auth_rx(struct nan_data *nan_data,
				      const struct ieee80211_mgmt *mgmt,
				      size_t len)
{
	return -1;
}

static inline
bool nan_pairing_is_peer_paired(struct nan_data *nan_data, const u8 *peer_addr)
{
	return false;
}

static inline
void nan_pairing_unpair_peer(struct nan_data *nan_data, const u8 *peer_addr)
{
}

#endif /* CONFIG_PASN */

#endif /* NAN_H */
