/*
 * Qualcomm Atheros OUI and vendor specific assignments
 * Copyright (c) 2014-2017, Qualcomm Atheros, Inc.
 * Copyright (c) 2018-2020, The Linux Foundation
 * Copyright (c) 2021-2023, Qualcomm Innovation Center, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef QCA_VENDOR_H
#define QCA_VENDOR_H

/*
 * This file is a registry of identifier assignments from the Qualcomm Atheros
 * OUI 00:13:74 for purposes other than MAC address assignment. New identifiers
 * can be assigned through normal review process for changes to the upstream
 * hostap.git repository.
 */

#define OUI_QCA 0x001374

#ifndef BIT
#define BIT(x) (1U << (x))
#endif

/**
 * enum qca_radiotap_vendor_ids - QCA radiotap vendor namespace IDs
 */
enum qca_radiotap_vendor_ids {
	QCA_RADIOTAP_VID_WLANTEST = 0,
};

/**
 * DOC: TX/RX NSS and chain configurations
 * This document describes all of the attributes used in the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION to configure the number of
 * spatial streams (NSS) and the number of chains used for transmitting (TX) and
 * receiving (RX) the data.
 *
 * Global NSS configuration - Applies to all bands (2.4 GHz and 5/6 GHz)
 * The following attributes are used to dynamically configure the number of
 * spatial streams to be used for transmitting or receiving the data in the
 * 2.4 GHz and 5/6 GHz bands. When configured in disconnected state, the
 * updated configuration will be considered for the immediately following
 * connection attempt. If the NSS is updated during a connection, the updated
 * NSS value is notified to the peer using operating mode notification/spatial
 * multiplexing power save frame. The updated NSS value after the connection
 * shall not be greater than the one negotiated during the connection. The
 * driver rejects any such higher value configuration with a failure.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_NSS: Only symmetric NSS configuration
 * (such as 2X2 or 1X1) can be done using this attribute.
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TX_NSS: Configure NSS for transmitting the data
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_RX_NSS: Configure NSS for receiving the data
 *
 * The QCA_WLAN_VENDOR_ATTR_CONFIG_TX_NSS and QCA_WLAN_VENDOR_ATTR_CONFIG_RX_NSS
 * attributes must be defined together or the driver will reject the command
 * with a failure. They can be used to configure either symmetric NSS
 * configuration (such as 2X2 or 1X1) or asymmetric configuration (such as 1X2).
 *
 * Per band NSS configuration - Applies to the 2.4 GHz or 5/6 GHz band
 * The following attributes are used to dynamically configure the number of
 * spatial streams to be used for transmitting or receiving the data in the
 * 2.4 GHz band or 5/6 GHz band. All these attributes must be defined together
 * to configure symmetric NSS configuration (such as 1X1 or 2X2) or asymmetric
 * NSS configuration (such as 1X2). If any of the attributes is missing, the
 * driver will reject the command with a failure. This configuration is allowed
 * only when in connected state and will be effective until disconnected. The
 * NSS value configured after the connection shall not be greater than the value
 * negotiated during the connection. Any such higher value configuration shall
 * be treated as invalid configuration by the driver.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TX_NSS_2GHZ: Configure TX_NSS in 2.4 GHz band
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_RX_NSS_2GHZ: Configure RX_NSS in 2.4 GHz band
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TX_NSS_5GHZ: Configure TX_NSS in 5 or 6 GHz band
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_RX_NSS_5GHZ: Configure RX_NSS in 5 or 6 GHz band
 *
 * Global chain configuration - Applies to all bands (2.4 GHz and 5/6 GHz)
 * The following attributes are used to dynamically configure the number of
 * chains to be used for transmitting or receiving the data in the 2.4 GHz and
 * 5/6 GHz bands. This configuration is allowed only when in connected state
 * and will be effective until disconnected. The driver rejects this
 * configuration if the number of spatial streams being used in the current
 * connection cannot be supported by this configuration.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_NUM_TX_CHAINS: The number of chains to be used
 * for transmitting the data in both the 2.4 GHz and 5/6 GHz bands.
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_NUM_RX_CHAINS: The number of chains to be used
 * for receiving the data in both the 2.4 GHz and 5/6 GHz bands.
 *
 * The attributes QCA_WLAN_VENDOR_ATTR_CONFIG_NUM_TX_CHAINS and
 * QCA_WLAN_VENDOR_ATTR_CONFIG_NUM_RX_CHAINS must be defined together or the
 * driver will reject the command with a failure.
 *
 * Per band chain configuration - Applies to the 2.4 GHz or 5/6 GHz band
 * The following band specific attributes are used to dynamically configure the
 * number of chains to be used for tranmissting or receiving the data in the
 * 2.4 GHz or 5/6 GHz band. These attributes must be defined together or the
 * driver will reject the command. This configuration is allowed only when in
 * connected state and will be effective until disconnected. The driver rejects
 * this configuration if the number of spatial streams being used in the
 * current connection cannot be supported by this configuration.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_NUM_TX_CHAINS_2GHZ: The number of chains to be
 * used for transmitting the data in the 2.4 GHz band.
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_NUM_RX_CHAINS_2GHZ: The number of chains to be
 * used for receiving the data in the 2.4 GHz band.
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_NUM_TX_CHAINS_5GHZ: The number of chains to be
 * used for transmitting the data in the 5/6 GHz band.
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_NUM_RX_CHAINS_5GHZ: The number of chains to be
 * used for receiving the data in the 5/6 GHz band.
 *
 * The following scenarios capture how the driver process the configuration when
 * different TX/RX NSS and chain config attributes are used in the command.
 *
 * Case 1: CONFIG_NSS + CONFIG_TX_NSS/RX_NSS - Only CONFIG_NSS is applied
 * since only one of the TX_NSS or RX_NSS attribute is present.
 *
 * Case 2: CONFIG_NSS + CONFIG_TX_NSS + CONFIG_RX_NSS - Same NSS values are
 * used to configure TX,RX in both the 2.4 GHz and 5/6 GHz bands.
 *
 * Case 3: Case 2 + NUM_TX_CHAINS + NUM_RX_CHAINS - The NSS and the number of
 * chains values are used to configure TX,RX in both the 2.4 GHz and 5/6 GHz
 * bands.
 *
 * Case 4: TX_NSS_2GHZ/TX_NSS_5GHZ + RX_NSS_2GHZ/RX_NSS_5GHZ - Since per band
 * TX/RX NSS attribute is missing, the driver rejects the command.
 *
 * Case 5: TX_NSS_2GHZ + TX_NSS_5GHZ + RX_NSS_2GHZ + RX_NSS_5GHZ - The 2.4 GHz
 * band is configured with the TX_NSS_2GHZ, RX_NSS_2GHZ values. The 5/6 GHz band
 * is configured with the TX_NSS_5GHZ, RX_NSS_5GHZ values.
 *
 * Case 6: TX_CHAINS_2GHZ/TX_CHAINS_5GHZ + RX_CHAINS_5GHZ/RX_CHAINS_5GHZ - Since
 * per band TX/RX chains attribute is missing, the driver rejects the command.
 *
 * Case 7: TX_CHAINS_2GHZ + TX_CHAINS_5GHZ + RX_CHAINS_5GHZ + RX_CHAINS_5GHZ -
 * The 2.4 GHz band is configured with the TX_CHAINS_2GHZ, RX_CHAINS_2GHZ
 * values. The 5/6 GHz band is configured with the TX_CHAINS_5GHZ,
 * RX_CHAINS_5GHZ values.
 *
 * Case 8: Case 5 + Case 7 - Per band TX,RX NSS and chains are configured.
 *
 * Case 9: Case 2 + Case 8 - Per band TX,RX NSS and chains are configured.
 */

/**
 * enum qca_nl80211_vendor_subcmds - QCA nl80211 vendor command identifiers
 *
 * @QCA_NL80211_VENDOR_SUBCMD_UNSPEC: Reserved value 0
 *
 * @QCA_NL80211_VENDOR_SUBCMD_TEST: Test command/event
 *
 * @QCA_NL80211_VENDOR_SUBCMD_ROAMING: Set roaming policy for drivers that use
 *	internal BSS-selection. This command uses
 *	@QCA_WLAN_VENDOR_ATTR_ROAMING_POLICY to specify the new roaming policy
 *	for the current connection (i.e., changes policy set by the nl80211
 *	Connect command). @QCA_WLAN_VENDOR_ATTR_MAC_ADDR may optionally be
 *	included to indicate which BSS to use in case roaming is disabled.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_AVOID_FREQUENCY: Recommendation of frequency
 *	ranges to avoid to reduce issues due to interference or internal
 *	co-existence information in the driver. These frequencies aim to
 *	minimize the traffic but not to totally avoid the traffic. That said
 *	for a P2P use case, these frequencies are allowed for the P2P
 *	discovery/negotiation but avoid the group to get formed on these
 *	frequencies. The event data structure is defined in
 *	struct qca_avoid_freq_list.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_DFS_CAPABILITY: Command to check driver support
 *	for DFS offloading.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_NAN: NAN command/event which is used to pass
 *	NAN Request/Response and NAN Indication messages. These messages are
 *	interpreted between the framework and the firmware component. While
 *	sending the command from userspace to the driver, payload is not
 *	encapsulated inside any attribute. Attribute QCA_WLAN_VENDOR_ATTR_NAN
 *	is used when receiving vendor events in userspace from the driver.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_TDLS_ENABLE: This command is used to enable TDLS
 *	capability or to form a session with the specified peer.
 *	If %NL80211_ATTR_VENDOR_DATA is sent as an empty nested attribute this
 *	indicates to enable TDLS capability on the interface.
 *	If %QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAC_ADDR is nested in
 *	%NL80211_ATTR_VENDOR_DATA this indicates the userspace requests to
 *	form a TDLS session with the specified peer MAC address.
 *	The attributes used with this command are defined in
 *	enum qca_wlan_vendor_attr_tdls_enable.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_TDLS_DISABLE: This command is used to disable TDLS
 *	capability or to terminate the session with the specified peer.
 *	If %NL80211_ATTR_VENDOR_DATA is sent as an empty nested attribute this
 *	indicates to disable TDLS capability on the interface.
 *	If %QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_MAC_ADDR is nested in
 *	%NL80211_ATTR_VENDOR_DATA this indicates the userspace requests to
 *	terminate TDLS session with the specified peer MAC address.
 *	The attributes used with this command are defined in
 *	enum qca_wlan_vendor_attr_tdls_disable.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_TDLS_GET_STATUS: This command is to get the TDLS
 *	status at the interface level or with the specific peer.
 *	If %NL80211_ATTR_VENDOR_DATA is sent as an empty nested attribute this
 *	indicates the TDLS status query is at interface level.
 *	If %QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAC_ADDR is nested in
 *	%NL80211_ATTR_VENDOR_DATA this indicates the userspace requests to
 *	get TDLS session status with the specified peer MAC address.
 *	The attributes used with this command are defined in
 *	enum qca_wlan_vendor_attr_tdls_get_status.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_TDLS_STATE: This event is to indicate the result
 *	of the TDLS session request with the peer sent by userspace in
 *	%QCA_NL80211_VENDOR_SUBCMD_TDLS_ENABLE.
 *	The attributes used with this command are defined in
 *	enum qca_wlan_vendor_attr_tdls_state.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_KEY_MGMT_SET_KEY: Set key operation that can be
 *	used to configure PMK to the driver even when not connected. This can
 *	be used to request offloading of key management operations. Only used
 *	if device supports QCA_WLAN_VENDOR_FEATURE_KEY_MGMT_OFFLOAD.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_KEY_MGMT_ROAM_AUTH: An extended version of
 *	NL80211_CMD_ROAM event with optional attributes including information
 *	from offloaded key management operation. Uses
 *	enum qca_wlan_vendor_attr_roam_auth attributes. Only used
 *	if device supports QCA_WLAN_VENDOR_FEATURE_KEY_MGMT_OFFLOAD.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_DO_ACS: ACS command/event which is used to
 *	invoke the ACS function in device and pass selected channels to
 *	hostapd. Uses enum qca_wlan_vendor_attr_acs_offload attributes.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GET_FEATURES: Command to get the features
 *	supported by the driver. enum qca_wlan_vendor_features defines
 *	the possible features.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_STARTED: Event used by driver,
 *	which supports DFS offloading, to indicate a channel availability check
 *	start.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_FINISHED: Event used by driver,
 *	which supports DFS offloading, to indicate a channel availability check
 *	completion.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_ABORTED: Event used by driver,
 *	which supports DFS offloading, to indicate that the channel availability
 *	check aborted, no change to the channel status.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_NOP_FINISHED: Event used by
 *	driver, which supports DFS offloading, to indicate that the
 *	Non-Occupancy Period for this channel is over, channel becomes usable.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_RADAR_DETECTED: Event used by driver,
 *	which supports DFS offloading, to indicate a radar pattern has been
 *	detected. The channel is now unusable.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_INFO: Get information from the driver.
 *	Attributes defined in enum qca_wlan_vendor_attr_get_wifi_info.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION: This command is used to
 *	configure various wiphy or interface level configurations. Attributes
 *	are defined in enum qca_wlan_vendor_attr_config. Userspace can send one
 *	or more configuration attributes with a single command. The driver
 *	accepts the command only if all the configurations are known, otherwise
 *	it rejects the command. The driver returns success only if processing of
 *	all the configurations succeeds. The driver continues to process all the
 *	configurations even if processing of some configurations failed and
 *	returns the last error occurred while processing the failed
 *	configurations.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION: This command is used to
 *	get the current values of the various wiphy or interface level
 *	configurations. Attributes are defined in enum
 *	qca_wlan_vendor_attr_config. Userspace needs to specify the
 *	configuration attributes for which it wants to get the values in the
 *	command, there is no significance for the value sent in the attribute
 *	unless explicitly specified in the corresponding configuration
 *	attribute documentation. The driver accepts the command only if all the
 *	configurations are known, otherwise it rejects the command. The driver
 *	returns success only if fetching of all configuration values succeeds
 *	and indicates the configuration values in corresponding attributes in
 *	the response. The driver continues to process all the configurations
 *	even if processing of some configurations failed and returns the last
 *	error occurred while processing the failed configurations.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GET_LOGGER_FEATURE_SET: Get the feature bitmap
 *	based on enum wifi_logger_supported_features. Attributes defined in
 *	enum qca_wlan_vendor_attr_get_logger_features.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GET_RING_DATA: Get the ring data from a particular
 *	logger ring, QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_RING_ID is passed as the
 *	attribute for this command. Attributes defined in
 *	enum qca_wlan_vendor_attr_wifi_logger_start.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_TDLS_GET_CAPABILITIES: Get the supported TDLS
 *	capabilities of the driver, parameters includes the attributes defined
 *	in enum qca_wlan_vendor_attr_tdls_get_capabilities.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_OFFLOADED_PACKETS: Vendor command used to offload
 *	sending of certain periodic IP packet to firmware, attributes defined in
 *	enum qca_wlan_vendor_attr_offloaded_packets.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_MONITOR_RSSI: Command used to configure RSSI
 *	monitoring, defines min and max RSSI which are configured for RSSI
 *	monitoring. Also used to notify the RSSI breach and provides the BSSID
 *	and RSSI value that was breached. Attributes defined in
 *	enum qca_wlan_vendor_attr_rssi_monitoring.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_NDP: Command used for performing various NAN
 *	Data Path (NDP) related operations, attributes defined in
 *	enum qca_wlan_vendor_attr_ndp_params.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_ND_OFFLOAD: Command used to enable/disable
 *	Neighbour Discovery offload, attributes defined in
 *	enum qca_wlan_vendor_attr_nd_offload.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_PACKET_FILTER: Used to set/get the various
 *	configuration parameter for BPF packet filter, attributes defined in
 *	enum qca_wlan_vendor_attr_packet_filter.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GET_BUS_SIZE: Gets the driver-firmware
 *	maximum supported size, attributes defined in
 *	enum qca_wlan_vendor_drv_info.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GET_WAKE_REASON_STATS: Command to get various
 *	data about wake reasons and datapath IP statistics, attributes defined
 *	in enum qca_wlan_vendor_attr_wake_stats.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_OCB_SET_CONFIG: Command used to set configuration
 *	for IEEE 802.11 communicating outside the context of a basic service
 *	set, called OCB command. Uses the attributes defines in
 *	enum qca_wlan_vendor_attr_ocb_set_config.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_OCB_SET_UTC_TIME: Command used to set OCB
 *	UTC time. Use the attributes defines in
 *	enum qca_wlan_vendor_attr_ocb_set_utc_time.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_OCB_START_TIMING_ADVERT: Command used to start
 *	sending OCB timing advert frames. Uses the attributes defines in
 *	enum qca_wlan_vendor_attr_ocb_start_timing_advert.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_OCB_STOP_TIMING_ADVERT: Command used to stop
 *	OCB timing advert. Uses the attributes defines in
 *	enum qca_wlan_vendor_attr_ocb_stop_timing_advert.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_OCB_GET_TSF_TIMER: Command used to get TSF
 *	timer value. Uses the attributes defines in
 *	enum qca_wlan_vendor_attr_ocb_get_tsf_resp.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_LINK_PROPERTIES: Command/event to update the
 *	link properties of the respective interface. As an event, is used
 *	to notify the connected station's status. The attributes for this
 *	command are defined in enum qca_wlan_vendor_attr_link_properties.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_SETBAND: Command to configure the enabled band(s)
 *	to the driver. This command sets the band(s) through either the
 *	attribute QCA_WLAN_VENDOR_ATTR_SETBAND_VALUE or
 *	QCA_WLAN_VENDOR_ATTR_SETBAND_MASK (or both).
 *	QCA_WLAN_VENDOR_ATTR_SETBAND_VALUE refers enum qca_set_band as unsigned
 *	integer values and QCA_WLAN_VENDOR_ATTR_SETBAND_MASK refers it as 32
 *	bit unsigned bitmask values. The allowed values for
 *	QCA_WLAN_VENDOR_ATTR_SETBAND_VALUE are limited to QCA_SETBAND_AUTO,
 *	QCA_SETBAND_5G, and QCA_SETBAND_2G. Other values/bitmasks are valid for
 *	QCA_WLAN_VENDOR_ATTR_SETBAND_MASK. The attribute
 *	QCA_WLAN_VENDOR_ATTR_SETBAND_VALUE is deprecated and the recommendation
 *	is to use the QCA_WLAN_VENDOR_ATTR_SETBAND_MASK. If the	both attributes
 *	are included for backwards compatibility, the configurations through
 *	QCA_WLAN_VENDOR_ATTR_SETBAND_MASK will take the precedence with drivers
 *	that support both attributes.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_ACS_POLICY: This command is used to configure
 *	DFS policy and channel hint for ACS operation. This command uses the
 *	attributes defined in enum qca_wlan_vendor_attr_acs_config and
 *	enum qca_acs_dfs_mode.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_P2P_LISTEN_OFFLOAD_START: Command used to
 *	start the P2P Listen offload function in device and pass the listen
 *	channel, period, interval, count, device types, and vendor specific
 *	information elements to the device driver and firmware.
 *	Uses the attributes defines in
 *	enum qca_wlan_vendor_attr_p2p_listen_offload.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_P2P_LISTEN_OFFLOAD_STOP: Command/event used to
 *	indicate stop request/response of the P2P Listen offload function in
 *	device. As an event, it indicates either the feature stopped after it
 *	was already running or feature has actually failed to start. Uses the
 *	attributes defines in enum qca_wlan_vendor_attr_p2p_listen_offload.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_SAP_CONDITIONAL_CHAN_SWITCH: After AP starts
 *	beaconing, this sub command provides the driver, the frequencies on the
 *	5 GHz band to check for any radar activity. Driver selects one channel
 *	from this priority list provided through
 *	@QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_FREQ_LIST and starts
 *	to check for radar activity on it. If no radar activity is detected
 *	during the channel availability check period, driver internally switches
 *	to the selected frequency of operation. If the frequency is zero, driver
 *	internally selects a channel. The status of this conditional switch is
 *	indicated through an event using the same sub command through
 *	@QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_STATUS. Attributes are
 *	listed in qca_wlan_vendor_attr_sap_conditional_chan_switch.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GPIO_CONFIG_COMMAND: Set GPIO pins. This uses the
 *	attributes defined in enum qca_wlan_gpio_attr.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GET_HW_CAPABILITY: Fetch hardware capabilities.
 *	This uses @QCA_WLAN_VENDOR_ATTR_GET_HW_CAPABILITY to indicate which
 *	capabilities are to be fetched and other
 *	enum qca_wlan_vendor_attr_get_hw_capability attributes to return the
 *	requested capabilities.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_LL_STATS_EXT: Link layer statistics extension.
 *	enum qca_wlan_vendor_attr_ll_stats_ext attributes are used with this
 *	command and event.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_LOC_GET_CAPA: Get capabilities for
 *	indoor location features. Capabilities are reported in
 *	QCA_WLAN_VENDOR_ATTR_LOC_CAPA.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_FTM_START_SESSION: Start an FTM
 *	(fine timing measurement) session with one or more peers.
 *	Specify Session cookie in QCA_WLAN_VENDOR_ATTR_FTM_SESSION_COOKIE and
 *	peer information in QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PEERS.
 *	On success, 0 or more QCA_NL80211_VENDOR_SUBCMD_FTM_MEAS_RESULT
 *	events will be reported, followed by
 *	QCA_NL80211_VENDOR_SUBCMD_FTM_SESSION_DONE event to indicate
 *	end of session.
 *	Refer to IEEE P802.11-REVmc/D7.0, 11.24.6
 *
 * @QCA_NL80211_VENDOR_SUBCMD_FTM_ABORT_SESSION: Abort a running session.
 *	A QCA_NL80211_VENDOR_SUBCMD_FTM_SESSION_DONE will be reported with
 *	status code indicating session was aborted.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_FTM_MEAS_RESULT: Event with measurement
 *	results for one peer. Results are reported in
 *	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PEER_RESULTS.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_FTM_SESSION_DONE: Event triggered when
 *	FTM session is finished, either successfully or aborted by
 *	request.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_FTM_CFG_RESPONDER: Configure FTM responder
 *	mode. QCA_WLAN_VENDOR_ATTR_FTM_RESPONDER_ENABLE specifies whether
 *	to enable or disable the responder. LCI/LCR reports can be
 *	configured with QCA_WLAN_VENDOR_ATTR_FTM_LCI and
 *	QCA_WLAN_VENDOR_ATTR_FTM_LCR. Can be called multiple
 *	times to update the LCI/LCR reports.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_AOA_MEAS: Perform a standalone AOA (angle of
 *	arrival) measurement with a single peer. Specify peer MAC address in
 *	QCA_WLAN_VENDOR_ATTR_MAC_ADDR and optionally frequency (MHz) in
 *	QCA_WLAN_VENDOR_ATTR_FREQ (if not specified, locate peer in kernel
 *	scan results cache and use the frequency from there).
 *	Also specify measurement type in QCA_WLAN_VENDOR_ATTR_AOA_TYPE.
 *	Measurement result is reported in
 *	QCA_NL80211_VENDOR_SUBCMD_AOA_MEAS_RESULT event.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_AOA_ABORT_MEAS: Abort an AOA measurement. Specify
 *	peer MAC address in QCA_WLAN_VENDOR_ATTR_MAC_ADDR.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_AOA_MEAS_RESULT: Event that reports
 *	the AOA measurement result.
 *	Peer MAC address reported in QCA_WLAN_VENDOR_ATTR_MAC_ADDR.
 *	success/failure status is reported in
 *	QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS.
 *	Measurement data is reported in QCA_WLAN_VENDOR_ATTR_AOA_MEAS_RESULT.
 *	The antenna array(s) used in the measurement are reported in
 *	QCA_WLAN_VENDOR_ATTR_LOC_ANTENNA_ARRAY_MASK.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_ENCRYPTION_TEST: Encrypt/decrypt the given
 *	data as per the given parameters.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GET_CHAIN_RSSI: Get antenna RSSI value for a
 *	specific chain.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_DMG_RF_GET_SECTOR_CFG: Get low level
 *	configuration for a DMG RF sector. Specify sector index in
 *	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_INDEX, sector type in
 *	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE and RF modules
 *	to return sector information for in
 *	QCA_WLAN_VENDOR_ATTR_DMG_RF_MODULE_MASK. Returns sector configuration
 *	in QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG. Also return the
 *	exact time where information was captured in
 *	QCA_WLAN_VENDOR_ATTR_TSF.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_DMG_RF_SET_SECTOR_CFG: Set low level
 *	configuration for a DMG RF sector. Specify sector index in
 *	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_INDEX, sector type in
 *	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE and sector configuration
 *	for one or more DMG RF modules in
 *	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_DMG_RF_GET_SELECTED_SECTOR: Get selected
 *	DMG RF sector for a station. This is the sector that the HW
 *	will use to communicate with the station. Specify the MAC address
 *	of associated station/AP/PCP in QCA_WLAN_VENDOR_ATTR_MAC_ADDR (not
 *	needed for unassociated	station). Specify sector type to return in
 *	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE. Returns the selected
 *	sector index in QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_INDEX.
 *	Also return the exact time where the information was captured
 *	in QCA_WLAN_VENDOR_ATTR_TSF.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_DMG_RF_SET_SELECTED_SECTOR: Set the
 *	selected DMG RF sector for a station. This is the sector that
 *	the HW will use to communicate with the station.
 *	Specify the MAC address of associated station/AP/PCP in
 *	QCA_WLAN_VENDOR_ATTR_MAC_ADDR, the sector type to select in
 *	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE and the sector index
 *	in QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_INDEX.
 *	The selected sector will be locked such that it will not be
 *	modified like it normally does (for example when station
 *	moves around). To unlock the selected sector for a station
 *	pass the special value 0xFFFF in the sector index. To unlock
 *	all connected stations also pass a broadcast MAC address.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_CONFIGURE_TDLS: Configure the TDLS behavior
 *	in the host driver. The different TDLS configurations are defined
 *	by the attributes in enum qca_wlan_vendor_attr_tdls_configuration.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GET_HE_CAPABILITIES: Query device IEEE 802.11ax HE
 *	capabilities. The response uses the attributes defined in
 *	enum qca_wlan_vendor_attr_get_he_capabilities.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_ABORT_SCAN: Abort an ongoing vendor scan that was
 *	started with QCA_NL80211_VENDOR_SUBCMD_TRIGGER_SCAN. This command
 *	carries the scan cookie of the corresponding scan request. The scan
 *	cookie is represented by QCA_WLAN_VENDOR_ATTR_SCAN_COOKIE.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS: Set the Specific
 *	Absorption Rate (SAR) power limits. A critical regulation for
 *	FCC compliance, OEMs require methods to set SAR limits on TX
 *	power of WLAN/WWAN. enum qca_vendor_attr_sar_limits
 *	attributes are used with this command.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_EXTERNAL_ACS: This command/event is used by the
 *	host driver for offloading the implementation of Auto Channel Selection
 *	(ACS) to an external user space entity. This interface is used as the
 *	event from the host driver to the user space entity and also as the
 *	request from the user space entity to the host driver. The event from
 *	the host driver is used by the user space entity as an indication to
 *	start the ACS functionality. The attributes used by this event are
 *	represented by the enum qca_wlan_vendor_attr_external_acs_event.
 *	User space entity uses the same interface to inform the host driver with
 *	selected channels after the ACS operation using the attributes defined
 *	by enum qca_wlan_vendor_attr_external_acs_channels.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_CHIP_PWRSAVE_FAILURE: Vendor event carrying the
 *	requisite information leading to a power save failure. The information
 *	carried as part of this event is represented by the
 *	enum qca_attr_chip_power_save_failure attributes.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_NUD_STATS_SET: Start/Stop the NUD statistics
 *	collection. Uses attributes defined in enum qca_attr_nud_stats_set.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_NUD_STATS_GET: Get the NUD statistics. These
 *	statistics are represented by the enum qca_attr_nud_stats_get
 *	attributes.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_FETCH_BSS_TRANSITION_STATUS: Sub-command to fetch
 *	the BSS transition status, whether accept or reject, for a list of
 *	candidate BSSIDs provided by the userspace. This uses the vendor
 *	attributes QCA_WLAN_VENDOR_ATTR_BTM_MBO_TRANSITION_REASON and
 *	QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO. The userspace shall specify
 *	the attributes QCA_WLAN_VENDOR_ATTR_BTM_MBO_TRANSITION_REASON and an
 *	array of QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_BSSID nested in
 *	QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO in the request. In the response
 *	the driver shall specify array of
 *	QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_BSSID and
 *	QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_STATUS pairs nested in
 *	QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_SET_TRACE_LEVEL: Set the trace level for a
 *	specific QCA module. The trace levels are represented by
 *	enum qca_attr_trace_level attributes.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_BRP_SET_ANT_LIMIT: Set the Beam Refinement
 *	Protocol antenna limit in different modes. See enum
 *	qca_wlan_vendor_attr_brp_ant_limit_mode.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START: Start spectral scan. The scan
 *	parameters are specified by enum qca_wlan_vendor_attr_spectral_scan.
 *	This returns a cookie (%QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COOKIE)
 *	identifying the operation in success case. In failure cases an
 *	error code (%QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_ERROR_CODE)
 *	describing the reason for the failure is returned.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_STOP: Stop spectral scan. This uses
 *	a cookie (%QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COOKIE) from
 *	@QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START to identify the scan to
 *	be stopped.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_ACTIVE_TOS: Set the active Type Of Service on the
 *	specific interface. This can be used to modify some of the low level
 *	scan parameters (off channel dwell time, home channel time) in the
 *	driver/firmware. These parameters are maintained within the host driver.
 *	This command is valid only when the interface is in the connected state.
 *	These scan parameters shall be reset by the driver/firmware once
 *	disconnected. The attributes used with this command are defined in
 *	enum qca_wlan_vendor_attr_active_tos.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_HANG: Event indicating to the user space that the
 *	driver has detected an internal failure. This event carries the
 *	information indicating the reason that triggered this detection. The
 *	attributes for this command are defined in
 *	enum qca_wlan_vendor_attr_hang.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CONFIG: Get the current values
 *	of spectral parameters used. The spectral scan parameters are specified
 *	by enum qca_wlan_vendor_attr_spectral_scan.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_DIAG_STATS: Get the debug stats
 *	for spectral scan functionality. The debug stats are specified by
 *	enum qca_wlan_vendor_attr_spectral_diag_stats.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CAP_INFO: Get spectral
 *	scan system capabilities. The capabilities are specified
 *	by enum qca_wlan_vendor_attr_spectral_cap.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_STATUS: Get the current
 *	status of spectral scan. The status values are specified
 *	by enum qca_wlan_vendor_attr_spectral_scan_status.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_PEER_FLUSH_PENDING: Sub-command to flush
 *	peer pending packets. Specify the peer MAC address in
 *	QCA_WLAN_VENDOR_ATTR_PEER_ADDR and the access category of the packets
 *	in QCA_WLAN_VENDOR_ATTR_AC. The attributes are listed
 *	in enum qca_wlan_vendor_attr_flush_pending.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GET_RROP_INFO: Get vendor specific Representative
 *	RF Operating Parameter (RROP) information. The attributes for this
 *	information are defined in enum qca_wlan_vendor_attr_rrop_info. This is
 *	intended for use by external Auto Channel Selection applications.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GET_SAR_LIMITS: Get the Specific Absorption Rate
 *	(SAR) power limits. This is a companion to the command
 *	@QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS and is used to retrieve the
 *	settings currently in use. The attributes returned by this command are
 *	defined by enum qca_vendor_attr_sar_limits.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_WLAN_MAC_INFO: Provides the current behavior of
 *	the WLAN hardware MAC. Also, provides the WLAN netdev interface
 *	information attached to the respective MAC.
 *	This works both as a query (user space asks the current mode) or event
 *	interface (driver advertising the current mode to the user space).
 *	Driver does not trigger this event for temporary hardware mode changes.
 *	Mode changes w.r.t Wi-Fi connection update (VIZ creation / deletion,
 *	channel change, etc.) are updated with this event. Attributes for this
 *	interface are defined in enum qca_wlan_vendor_attr_mac.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_SET_QDEPTH_THRESH: Set MSDU queue depth threshold
 *	per peer per TID. Attributes for this command are define in
 *	enum qca_wlan_set_qdepth_thresh_attr.
 * @QCA_NL80211_VENDOR_SUBCMD_THERMAL_CMD: Provides the thermal shutdown action
 *	guide for WLAN driver. Request to suspend of driver and FW if the
 *	temperature is higher than the suspend threshold; resume action is
 *	requested to driver if the temperature is lower than the resume
 *	threshold. In user poll mode, request temperature data by user. For test
 *	purpose, getting thermal shutdown configuration parameters is needed.
 *	Attributes for this interface are defined in
 *	enum qca_wlan_vendor_attr_thermal_cmd.
 * @QCA_NL80211_VENDOR_SUBCMD_THERMAL_EVENT: Thermal events reported from
 *	driver. Thermal temperature and indication of resume completion are
 *	reported as thermal events. The attributes for this command are defined
 *	in enum qca_wlan_vendor_attr_thermal_event.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_WIFI_TEST_CONFIGURATION: Sub command to set WiFi
 *	test configuration. Attributes for this command are defined in
 *	enum qca_wlan_vendor_attr_wifi_test_config.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_BSS_FILTER: This command is used to configure an
 *	RX filter to receive frames from stations that are active on the
 *	operating channel, but not associated with the local device (e.g., STAs
 *	associated with other APs). Filtering is done based on a list of BSSIDs
 *	and STA MAC addresses added by the user. This command is also used to
 *	fetch the statistics of unassociated stations. The attributes used with
 *	this command are defined in enum qca_wlan_vendor_attr_bss_filter.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_NAN_EXT: An extendable version of NAN vendor
 *	command. The earlier command for NAN, QCA_NL80211_VENDOR_SUBCMD_NAN,
 *	carried a payload which was a binary blob of data. The command was not
 *	extendable to send more information. The newer version carries the
 *	legacy blob encapsulated within an attribute and can be extended with
 *	additional vendor attributes that can enhance the NAN command interface.
 * @QCA_NL80211_VENDOR_SUBCMD_ROAM_SCAN_EVENT: Event to indicate scan triggered
 *	or stopped within driver/firmware in order to initiate roaming. The
 *	attributes used with this event are defined in enum
 *	qca_wlan_vendor_attr_roam_scan. Some drivers may not send these events
 *	in few cases, e.g., if the host processor is sleeping when this event
 *	is generated in firmware.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_PEER_CFR_CAPTURE_CFG: This command is used to
 *	configure parameters per peer to capture Channel Frequency Response
 *	(CFR) and enable Periodic CFR capture. The attributes for this command
 *	are defined in enum qca_wlan_vendor_peer_cfr_capture_attr. This command
 *	can also be used to send CFR data from the driver to userspace when
 *	netlink events are used to send CFR data.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_THROUGHPUT_CHANGE_EVENT: Event to indicate changes
 *	in throughput dynamically. The driver estimates the throughput based on
 *	number of packets being transmitted/received per second and indicates
 *	the changes in throughput to user space. Userspace tools can use this
 *	information to configure kernel's TCP parameters in order to achieve
 *	peak throughput. Optionally, the driver will also send guidance on
 *	modifications to kernel's TCP parameters which can be referred by
 *	userspace tools. The attributes used with this event are defined in enum
 *	qca_wlan_vendor_attr_throughput_change.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_COEX_CONFIG: This command is used to set
 *	priorities among different types of traffic during coex scenarios.
 *	Current supported prioritization is among WLAN/BT/ZIGBEE with different
 *	profiles mentioned in enum qca_coex_config_profiles. The associated
 *	attributes used with this command are defined in enum
 *	qca_vendor_attr_coex_config.
 *
 *	Based on the config provided, FW will boost the weight and prioritize
 *	the traffic for that subsystem (WLAN/BT/Zigbee).
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GET_SUPPORTED_AKMS: This command is used to query
 *	the supported AKM suite selectorss from the driver. It returns the list
 *	of supported AKMs in the attribute NL80211_ATTR_AKM_SUITES.
 * @QCA_NL80211_VENDOR_SUBCMD_GET_FW_STATE: This command is used to get firmware
 *	state from the driver. It returns the firmware state in the attribute
 *	QCA_WLAN_VENDOR_ATTR_FW_STATE.
 * @QCA_NL80211_VENDOR_SUBCMD_PEER_STATS_CACHE_FLUSH: This vendor subcommand
 *	is used by the driver to flush per-peer cached statistics to user space
 *	application. This interface is used as an event from the driver to
 *	user space application. Attributes for this event are specified in
 *	enum qca_wlan_vendor_attr_peer_stats_cache_params.
 *	QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_DATA attribute is expected to be
 *	sent in the event.
 * @QCA_NL80211_VENDOR_SUBCMD_MPTA_HELPER_CONFIG: This sub command is used to
 *	improve the success rate of Zigbee joining network.
 *	Due to PTA master limitation, Zigbee joining network success rate is
 *	low while WLAN is working. The WLAN driver needs to configure some
 *	parameters including Zigbee state and specific WLAN periods to enhance
 *	PTA master. All these parameters are delivered by the attributes
 *	defined in enum qca_mpta_helper_vendor_attr.
 * @QCA_NL80211_VENDOR_SUBCMD_BEACON_REPORTING: This sub command is used to
 *	implement Beacon frame reporting feature.
 *
 *	Userspace can request the driver/firmware to periodically report
 *	received Beacon frames whose BSSID is same as the current connected
 *	BSS's MAC address.
 *
 *	In case the STA seamlessly (without sending disconnect indication to
 *	userspace) roams to a different BSS, Beacon frame reporting will be
 *	automatically enabled for the Beacon frames whose BSSID is same as the
 *	MAC address of the new BSS. Beacon reporting will be stopped when the
 *	STA is disconnected (when the disconnect indication is sent to
 *	userspace) and need to be explicitly enabled by userspace for next
 *	connection.
 *
 *	When a Beacon frame matching configured conditions is received, and if
 *	userspace has requested to send asynchronous beacon reports, the
 *	driver/firmware will encapsulate the details of the Beacon frame in an
 *	event and send it to userspace along with updating the BSS information
 *	in cfg80211 scan cache, otherwise driver will only update the cfg80211
 *	scan cache with the information from the received Beacon frame but will
 *	not send any active report to userspace.
 *
 *	The userspace can request the driver/firmware to stop reporting Beacon
 *	frames. If the driver/firmware is not able to receive Beacon frames due
 *	to other Wi-Fi operations such as off-channel activities, etc., the
 *	driver/firmware will send a pause event to userspace and stop reporting
 *	Beacon frames. Whether the beacon reporting will be automatically
 *	resumed or not by the driver/firmware later will be reported to
 *	userspace using the QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_AUTO_RESUMES
 *	flag. The beacon reporting shall be resumed for all the cases except
 *	either when userspace sets
 *	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_DO_NOT_RESUME flag in the command
 *	which triggered the current beacon reporting or during any disconnection
 *	case as indicated by setting
 *	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_PAUSE_REASON to
 *	QCA_WLAN_VENDOR_BEACON_REPORTING_PAUSE_REASON_DISCONNECTED by the
 *	driver.
 *
 *	After QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_PAUSE event is received
 *	by userspace with QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_AUTO_RESUMES
 *	flag not set, the next first
 *	QCA_WLAN_VENDOR_BEACON_REPORTING_OP_BEACON_INFO event from the driver
 *	shall be considered as un-pause event.
 *
 *	All the attributes used with this command are defined in
 *	enum qca_wlan_vendor_attr_beacon_reporting_params.
 * @QCA_NL80211_VENDOR_SUBCMD_INTEROP_ISSUES_AP: In practice, some APs have
 *	interop issues with the DUT. This sub command is used to transfer the
 *	AP info between the driver and user space. This works both as a command
 *	and an event. As a command, it configures the stored list of APs from
 *	user space to firmware; as an event, it indicates the AP info detected
 *	by the firmware to user space for persistent storage. The attributes
 *	defined in enum qca_vendor_attr_interop_issues_ap are used to deliver
 *	the parameters.
 * @QCA_NL80211_VENDOR_SUBCMD_OEM_DATA: This command/event is used to
 *	send/receive OEM data binary blobs to/from application/service to/from
 *	firmware. The attributes defined in enum
 *	qca_wlan_vendor_attr_oem_data_params are used to deliver the
 *	parameters.
 * @QCA_NL80211_VENDOR_SUBCMD_AVOID_FREQUENCY_EXT: This command/event is used
 *	to send/receive avoid frequency data using
 *	enum qca_wlan_vendor_attr_avoid_frequency_ext.
 *	This new command is alternative to existing command
 *	QCA_NL80211_VENDOR_SUBCMD_AVOID_FREQUENCY since existing command/event
 *	is using stream of bytes instead of structured data using vendor
 *	attributes. User space sends unsafe frequency ranges to the driver using
 *	a nested attribute %QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_RANGE. On
 *	reception of this command, the driver shall check if an interface is
 *	operating on an unsafe frequency and the driver shall try to move to a
 *	safe channel when needed. If the driver is not able to find a safe
 *	channel the interface can keep operating on an unsafe channel with the
 *	TX power limit derived based on internal configurations	like
 *	regulatory/SAR rules.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_ADD_STA_NODE: This vendor subcommand is used to
 *	add the STA node details in driver/firmware. Attributes for this event
 *	are specified in enum qca_wlan_vendor_attr_add_sta_node_params.
 * @QCA_NL80211_VENDOR_SUBCMD_BTC_CHAIN_MODE: This command is used to set BT
 *	coex chain mode from application/service.
 *	The attributes defined in enum qca_vendor_attr_btc_chain_mode are used
 *	to deliver the parameters.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GET_STA_INFO: This vendor subcommand is used to
 *	get information of a station from driver to userspace. This command can
 *	be used in both STA and AP modes. For STA mode, it provides information
 *	of the current association when in connected state or the last
 *	association when in disconnected state. For AP mode, only information
 *	of the currently connected stations is available. This command uses
 *	attributes defined in enum qca_wlan_vendor_attr_get_sta_info.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_REQUEST_SAR_LIMITS_EVENT: This acts as an event.
 *	Host drivers can request the user space entity to set the SAR power
 *	limits with this event. Accordingly, the user space entity is expected
 *	to set the SAR power limits. Host drivers can retry this event to the
 *	user space for the SAR power limits configuration from user space. If
 *	the driver does not get the SAR power limits from user space for all
 *	the retried attempts, it can configure a default SAR power limit.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_UPDATE_STA_INFO: This acts as a vendor event and
 *	is used to update the information about the station from the driver to
 *	userspace. Uses attributes from enum
 *	qca_wlan_vendor_attr_update_sta_info.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_DRIVER_DISCONNECT_REASON: This acts as an event.
 *	The host driver initiates the disconnection for scenarios such as beacon
 *	miss, NUD failure, peer kick out, etc. The disconnection indication
 *	through cfg80211_disconnected() expects the reason codes from enum
 *	ieee80211_reasoncode which does not signify these various reasons why
 *	the driver has triggered the disconnection. This event will be used to
 *	send the driver specific reason codes by the host driver to userspace.
 *	Host drivers should trigger this event and pass the respective reason
 *	code immediately prior to triggering cfg80211_disconnected(). The
 *	attributes used with this event are defined in enum
 *	qca_wlan_vendor_attr_driver_disconnect_reason.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_CONFIG_TSPEC: This vendor subcommand is used to
 *	add/delete TSPEC for each AC. One command is for one specific AC only.
 *	This command can only be used in STA mode and the STA must be
 *	associated with an AP when the command is issued. Uses attributes
 *	defined in enum qca_wlan_vendor_attr_config_tspec.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT: Vendor subcommand to configure TWT.
 *	Uses attributes defined in enum qca_wlan_vendor_attr_config_twt.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GETBAND: Command to get the enabled band(s) from
 *	the driver. The band configurations obtained are referred through
 *	QCA_WLAN_VENDOR_ATTR_SETBAND_MASK.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_MEDIUM_ASSESS: Vendor subcommand/event for medium
 *	assessment.
 *	Uses attributes defined in enum qca_wlan_vendor_attr_medium_assess.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_UPDATE_SSID: This acts as a vendor event and is
 *	used to update SSID information in hostapd when it is updated in the
 *	driver. Uses the attribute NL80211_ATTR_SSID.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_WIFI_FW_STATS: This vendor subcommand is used by
 *	the driver to send opaque data from the firmware to userspace. The
 *	driver sends an event to userspace whenever such data is received from
 *	the firmware.
 *
 *	QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA is used as the attribute to
 *	send this opaque data for this event.
 *
 *	The format of the opaque data is specific to the particular firmware
 *	version and there is no guarantee of the format remaining same.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_MBSSID_TX_VDEV_STATUS: This acts as an event.
 *	The host driver selects Tx VDEV, and notifies user. The attributes
 *	used with this event are defined in enum
 *	qca_wlan_vendor_attr_mbssid_tx_vdev_status.
 *	This event contains Tx VDEV group information, other VDEVs
 *	interface index, and status information.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_CONCURRENT_POLICY: Vendor command to
 *	configure the concurrent session policies when multiple interfaces
 *	are (getting) active. The attributes used by this command are defined
 *	in enum qca_wlan_vendor_attr_concurrent_policy.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_USABLE_CHANNELS: Userspace can use this command
 *	to query usable channels for different interface types such as STA,
 *	AP, P2P GO, P2P Client, NAN, etc. The driver shall report all usable
 *	channels in the response based on country code, different static
 *	configurations, concurrency combinations, etc. The attributes used
 *	with this command are defined in
 *	enum qca_wlan_vendor_attr_usable_channels.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GET_RADAR_HISTORY: This vendor subcommand is used
 *	to get DFS radar history from the driver to userspace. The driver
 *	returns QCA_WLAN_VENDOR_ATTR_RADAR_HISTORY_ENTRIES attribute with an
 *	array of nested entries.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_MDNS_OFFLOAD: Userspace can use this command to
 *	enable/disable mDNS offload to the firmware. The attributes used with
 *	this command are defined in enum qca_wlan_vendor_attr_mdns_offload.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_SET_MONITOR_MODE: This vendor subcommand is used
 *	to set packet monitor mode that aims to send the specified set of TX and
 *	RX frames on the current client interface to an active monitor
 *	interface. If this monitor mode is set, the driver will send the
 *	configured frames, from the interface on which the command is issued, to
 *	an active monitor interface. The attributes used with this command are
 *	defined in enum qca_wlan_vendor_attr_set_monitor_mode.
 *
 *	Though the monitor mode is configured for the respective
 *	Data/Management/Control frames, it is up to the respective WLAN
 *	driver/firmware/hardware designs to consider the possibility of sending
 *	these frames over the monitor interface. For example, the Control frames
 *	are handled within the hardware and thus passing such frames over the
 *	monitor interface is left to the respective designs.
 *
 *	Also, this monitor mode is governed to behave accordingly in
 *	suspend/resume states. If the firmware handles any of such frames in
 *	suspend state without waking up the host and if the monitor mode is
 *	configured to notify all such frames, the firmware is expected to resume
 *	the host and forward the respective frames to the monitor interface.
 *	Please note that such a request to get the frames over the monitor
 *	interface will have a definite power implication.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_ROAM_EVENTS: This vendor subcommand is used both
 *	as a request to set the driver/firmware with the parameters to trigger
 *	the roaming events, and also used by the driver/firmware to pass on the
 *	various roam events to userspace.
 *	Applicable only for the STA mode. The attributes used with this command
 *	are defined in enum qca_wlan_vendor_attr_roam_events.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_RATEMASK_CONFIG: Subcommand to set or reset the
 *	rate mask config for a list of PHY types. Userspace shall provide an
 *	array of the vendor attributes defined in
 *	enum qca_wlan_vendor_attr_ratemask_params.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_MCC_QUOTA: Multi-channel Concurrency (MCC) occurs
 *	when two interfaces are active on the same band, using two different
 *	home channels, and only supported by a single radio. In this scenario
 *	the device must split the use of the radio between the two interfaces.
 *	The percentage of time allocated to a given interface is the quota.
 *	Depending on the configuration, the quota can either be fixed or
 *	dynamic.
 *
 *	When used as an event, the device will report the quota type, and for
 *	all interfaces operating in MCC it will report the current quota.
 *	When used as a command, the device can be configured for a specific
 *	quota type, and in the case of a fixed quota, the quota to apply to one
 *	of the interfaces.
 *
 *	Applications can use the event to do TX bitrate control based on the
 *	information, and can use the command to explicitly set the quota to
 *	enhance performance in specific scenarios.
 *
 *	The attributes used with this command are defined in
 *	enum qca_wlan_vendor_attr_mcc_quota.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GET_RADIO_COMBINATION_MATRIX: Vendor command to
 *	get the WLAN radio combinations matrix supported by the device which
 *	provides the device simultaneous radio configurations such as
 *	standalone, dual band simultaneous, and single band simultaneous.
 *
 *	The attributes used with this command are defined in
 *	enum qca_wlan_vendor_attr_radio_combination_matrix.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_DRIVER_READY: Event indicating to the user space
 *	that the driver is ready for operations again after recovering from
 *	internal failures. This occurs following a failure that was indicated by
 *	@QCA_NL80211_VENDOR_SUBCMD_HANG.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_PASN: Subcommand used to offload preassociation
 *	security negotiation and key generation to user space.
 *
 *	When used as an event, the driver requests userspace to trigger the PASN
 *	authentication or dropping of a PTKSA for the indicated peer devices.
 *	When used as a command response, userspace indicates a consolidated
 *	status report for all the peers that were requested for.
 *
 *	The attributes used with this command are defined in
 *	enum qca_wlan_vendor_attr_pasn.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_SECURE_RANGING_CONTEXT: Subcommand used to set
 *	secure ranging context such as TK and LTF keyseed for each peer
 *	requested by the driver with a @QCA_NL80211_VENDOR_SUBCMD_PASN event.
 *
 *	The attributes used with this command are defined in
 *	enum qca_wlan_vendor_attr_secure_ranging_ctx.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_COAP_OFFLOAD: This vendor subcommand is used to
 *	enable/disable offload processing in firmware during system/runtime
 *	suspend for CoAP messages (see RFC7252: The Constrained Application
 *	Protocol) and fetch information of the CoAP messages cached during
 *	offload processing.
 *
 *	The attributes used with this command are defined in
 *	enum qca_wlan_vendor_attr_coap_offload.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG: Subcommand to configure
 *	(add, remove, or change) a Stream Classification Service (SCS) rule.
 *
 *      The attributes used with this event are defined in
 *      enum qca_wlan_vendor_attr_scs_rule_config.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GET_SAR_CAPABILITY: Fetch SAR capabilities
 *	supported by the WLAN firmware.
 *
 *	The attributes used with this command are defined in
 *	enum qca_wlan_vendor_attr_sar_capability.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_SR: Subcommand used to implement Spatial Reuse
 *	(SR) feature. This command is used by userspace to configure SR
 *	parameters to the driver and to get the SR related parameters and
 *	statistics with synchronous responses from the driver.
 *	The driver also uses this command to send asynchronous events to
 *	userspace to indicate suspend/resume of SR feature and changes
 *	in SR parameters.
 *
 *	The attributes used with this command are defined in
 *	enum qca_wlan_vendor_attr_sr.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_MLO_PEER_PRIM_NETDEV_EVENT: Subcommand used to
 *	notify application layer about the primary netdev of an MLO connection.
 *	In some implementations, MLO has multiple netdevs out of which one
 *	netdev is designated as primary to provide a unified interface to the
 *	bridge. In those implementations this event is sent on every MLO peer
 *	connection. User applications on an AP MLD will use this event to get
 *	info for all the links from non-AP MLD that were negotiated to be used
 *	for the ML association.
 *
 *	The attributes used with this event are defined in
 *	enum qca_wlan_vendor_attr_mlo_peer_prim_netdev_event.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT: This vendor command is used by the
 *	driver to notify different AFC events to userspace. The attributes used
 *	with this command are defined in enum qca_wlan_vendor_attr_afc_event.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_AFC_RESPONSE: This vendor command is used by
 *	userspace to deliver AFC response data to driver. The attributes used
 *	with this command are defined in enum qca_wlan_vendor_attr_afc_response.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_DOZED_AP: Subcommand to configure AP interface to
 *	operate in doze mode.
 *
 *	Userspace uses this command to configure the AP interface to enter or
 *	exit from doze mode. The driver sends this event after it enters or
 *	exits the doze mode with the updated AP doze mode settings.
 *
 *	The attributes used with this subcommand are defined in
 *	enum qca_wlan_vendor_attr_dozed_ap.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_GET_MONITOR_MODE: This vendor subcommand is used
 *	to get the status of local packet capture of monitor mode. The monitor
 *	mode can be started using QCA_NL80211_VENDOR_SUBCMD_SET_MONITOR_MODE
 *	subcommand.
 *
 *	The attributes used with this command are defined in enum
 *	qca_wlan_vendor_attr_get_monitor_mode.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_ROAM_STATS: This vendor command is used to
 *	get roam information from the driver to user space. It provides the
 *	latest several instances of roam information cached in the driver.
 *	The command is only used for STA mode. The attributes used with this
 *	command are defined in enum qca_wlan_vendor_attr_roam_cached_stats.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_MLO_LINK_STATE: This vendor subcommand is used to
 *	configure and fetch the state information of the MLO links affiliated
 *	with the STA interface. The attributes used with this command are
 *	defined in enum qca_wlan_vendor_attr_mlo_link_state.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_CONNECTED_CHANNEL_STATS: Userspace can use this
 *	vendor subcommand to trigger channel utilization measurement on entire
 *	channel width of the connected channel(s). For MLO connection, connected
 *	channel utilization measurement shall be done on all the MLO links.
 *	The driver may use regular scan or wideband energy detection feature
 *	based on the hardware capability for connected channel(s) utilization
 *	measurement. The driver indicates the connected channel(s) utilization
 *	measurement completion as an asynchronous event with this command ID to
 *	userspace. Upon receiving this event, userspace can use
 *	%NL80211_CMD_GET_INTERFACE to determine the channel width of the current
 *	connected channel(s) and can derive the channel utilization percentage
 *	(CU) of each 20 MHz sub-channel of the entire connected channel using
 *	%NL80211_CMD_GET_SURVEY response.
 *	CU = %NL80211_SURVEY_INFO_TIME_BUSY * 100 / %NL80211_SURVEY_INFO_TIME.
 *	This command is only used for STA mode.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_TID_TO_LINK_MAP: This vendor subcommand is
 *	used as an event to notify the userspace of TID-to-link map changes
 *	negotiated by the driver or updated by associated AP MLD with Beacon,
 *	Probe Response, or Action frames. The attributes used with this command
 *	are defined in enum qca_wlan_vendor_attr_tid_to_link_map.
 *
 *	Note that the attribute
 *	%QCA_WLAN_VENDOR_ATTR_TID_TO_LINK_MAP_AP_MLD_ADDR may not correspond to
 *	the current connected AP MLD address.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_LINK_RECONFIG: Notify userspace about the removal
 *	of STA MLD setup links due to the AP MLD removing the corresponding
 *	affiliated APs with Multi-Link reconfiguration. If all the STA MLD setup
 *	links are removed during Multi-Link reconfiguration, the driver shall
 *	use %NL80211_CMD_DISCONNECT instead of this command since it is a
 *	connection drop. The attributes used with this command are defined in
 *	enum qca_wlan_vendor_attr_link_reconfig.
 *	Note that the attribute
 *	%QCA_WLAN_VENDOR_ATTR_LINK_RECONFIG_AP_MLD_ADDR may not correspond to
 *	the current connected AP MLD address.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_TDLS_DISC_RSP_EXT: Vendor command to configure
 *	the driver with MLO link id information on which to transmit the TDLS
 *	discovery response frame on the configured MLO BSS link when the
 *	local station is connected in MLO mode. This command is sent to the
 *	driver prior to the TDLS discovery response management transmit
 *	operation and is followed immediately by the TDLS discovery response
 *	management frame transmit command.
 *
 *	The driver saves the configured MLO link id information and uses it for
 *	the following TDLS discovery response frame transmission on the
 *	configured MLO BSS link and the link id information is cleared in the
 *	driver after the TDLS discovery response frame is successfully
 *	transmitted. This behavior is indepent of the TDLS peer STA connection
 *	mode (MLO or non-MLO).
 *
 *	Uses the attributes defined in
 *	enum qca_wlan_vendor_attr_tdls_disc_rsp_ext.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_TX_LATENCY: This vendor subcommand is used to
 *	configure, retrieve, and report per-link transmit latency statistics.
 *
 *	The attributes used with this subcommand are defined in
 *	enum qca_wlan_vendor_attr_tx_latency.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_REGULATORY_TPC_INFO: Vendor command is used to
 *	query transmit power information on STA interface from the driver for a
 *	connected AP. The attributes included in response are defined in
 *	enum qca_wlan_vendor_attr_tpc_links. In case of MLO STA, multiple links
 *	TPC info may be returned. The information includes regulatory maximum
 *	transmit power limit, AP local power constraint advertised from AP's
 *	Beacon and Probe Response frames. For PSD power mode, the information
 *	includes PSD power levels for each subchannel of operating bandwidth.
 *	The information is driver calculated power limits based on the current
 *	regulatory domain, AP local power constraint, and other IEs. The
 *	information will be set to target. Target will decide the final TX power
 *	based on this and chip specific power conformance test limits (CTL), and
 *	SAR limits.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_FW_PAGE_FAULT_REPORT: Event indication from the
 *	driver to user space which is carrying firmware page fault related
 *	summary report. The attributes for this command are defined in
 *	enum qca_wlan_vendor_attr_fw_page_fault_report.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_DISASSOC_PEER: Event indication from the driver
 *	to user space to disassociate with a peer based on the peer MAC address
 *	provided. Specify the peer MAC address in
 *	QCA_WLAN_VENDOR_ATTR_MAC_ADDR. For MLO, MLD MAC address is provided.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_ADJUST_TX_POWER: This vendor command is used to
 *	adjust transmit power. The attributes used with this subcommand are
 *	defined in enum qca_wlan_vendor_attr_adjust_tx_power.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_COMPLETE: Event indication from the
 *	driver to notify user application about the spectral scan completion.
 *	The attributes used with this subcommand are defined in
 *	enum qca_wlan_vendor_attr_spectral_scan_complete.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_ASYNC_GET_STATION: Register for receiving
 *	%NL80211_CMD_GET_STATION responses as unicast events when there are
 *	%NL80211_CMD_GET_STATION requests from any userspace module on the same
 *	interface index with which this command is sent. This command is also
 *	used as the unicast event to indicate the %NL80211_CMD_GET_STATION
 *	response. The attributes for this command are defined in
 *	enum qca_wlan_vendor_async_get_station_attr.
 *
 *	The driver will send the unicast events with same netlink port ID which
 *	is used by userspace application for sending the registration command.
 *	If multiple registration commands are received with different netlink
 *	port IDs, the driver will send unicast events with each netlink port ID
 *	separately.
 *
 *	Userspace applications can deregister the unicast event reporting with
 *	disable configuration. The registrations will be removed automatically
 *	by the driver when the corresponding netlink socket is closed.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_AP_SUSPEND: Vendor command to put an AP interface
 *	in suspend state. On enabling suspend, AP deauthenticates all associated
 *	stations and stops TX/RX operations on the interface. The driver
 *	retains the AP configuration and on resume, starts all TX/RX operations
 *	with the same AP configuration.
 *
 *	This subcommand is also used as an event to notify userspace about AP
 *	suspended/resumed state changes.
 *
 *	The attributes used with this command/event are defined in enum
 *	qca_wlan_vendor_attr_ap_suspend.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_FLOW_STATS: Event indication from the driver to
 *	the userspace which contains all the statistics collected for a flow to
 *	be classified. This event is sent if the userspace enables the
 *	flow stats reporting via the command
 *	@QCA_NL80211_VENDOR_SUBCMD_ASYNC_STATS_POLICY and when the driver has
 *	collected the required flow statistics, as specified by the attributes
 *	of this event. The attributes for this event are defined in
 *	enum qca_wlan_vendor_attr_flow_stats.
 * @QCA_NL80211_VENDOR_SUBCMD_FLOW_CLASSIFY_RESULT: This vendor command is used
 *	to indicate the flow classification result based on the flow samples
 *	received as a part of @QCA_NL80211_VENDOR_SUBCMD_FLOW_STATS. The
 *	attributes for this command are defined in the
 *	enum qca_wlan_vendor_attr_flow_classify_result.
 * @QCA_NL80211_VENDOR_SUBCMD_ASYNC_STATS_POLICY: This vendor command is used to
 *	indicate the ASYNC statistics policy from the userspace to the driver
 *	and it contains the STATS type for which the command is intended. The
 *	attributes for this command are defined in the
 *	enum qca_wlan_vendor_attr_async_stats_policy.
 * @QCA_NL80211_VENDOR_SUBCMD_CLASSIFIED_FLOW_REPORT: Event indication from the
 *	driver to the userspace containing all the samples of a classified
 *	flow along with its classification result. This event is sent by the
 *	driver to userspace when it receives classification result via the
 *	command @QCA_NL80211_VENDOR_SUBCMD_FLOW_CLASSIFY_RESULT and the
 *	collection of these statistics has been enabled by the command
 *	@QCA_NL80211_VENDOR_SUBCMD_ASYNC_STATS_POLICY. The attributes for this
 *	event are defined in enum qca_wlan_vendor_attr_flow_stats.
 */
enum qca_nl80211_vendor_subcmds {
	QCA_NL80211_VENDOR_SUBCMD_UNSPEC = 0,
	QCA_NL80211_VENDOR_SUBCMD_TEST = 1,
	/* subcmds 2..8 not yet allocated */
	QCA_NL80211_VENDOR_SUBCMD_ROAMING = 9,
	QCA_NL80211_VENDOR_SUBCMD_AVOID_FREQUENCY = 10,
	QCA_NL80211_VENDOR_SUBCMD_DFS_CAPABILITY =  11,
	QCA_NL80211_VENDOR_SUBCMD_NAN =  12,
	QCA_NL80211_VENDOR_SUBCMD_STATS_EXT = 13,
	QCA_NL80211_VENDOR_SUBCMD_LL_STATS_SET = 14,
	QCA_NL80211_VENDOR_SUBCMD_LL_STATS_GET = 15,
	QCA_NL80211_VENDOR_SUBCMD_LL_STATS_CLR = 16,
	QCA_NL80211_VENDOR_SUBCMD_LL_STATS_RADIO_RESULTS = 17,
	QCA_NL80211_VENDOR_SUBCMD_LL_STATS_IFACE_RESULTS = 18,
	QCA_NL80211_VENDOR_SUBCMD_LL_STATS_PEERS_RESULTS = 19,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_START = 20,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_STOP = 21,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_GET_VALID_CHANNELS = 22,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_GET_CAPABILITIES = 23,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_GET_CACHED_RESULTS = 24,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_SCAN_RESULTS_AVAILABLE = 25,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_FULL_SCAN_RESULT = 26,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_SCAN_EVENT = 27,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_HOTLIST_AP_FOUND = 28,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_SET_BSSID_HOTLIST = 29,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_RESET_BSSID_HOTLIST = 30,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_SIGNIFICANT_CHANGE = 31,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_SET_SIGNIFICANT_CHANGE = 32,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_RESET_SIGNIFICANT_CHANGE = 33,
	QCA_NL80211_VENDOR_SUBCMD_TDLS_ENABLE = 34,
	QCA_NL80211_VENDOR_SUBCMD_TDLS_DISABLE = 35,
	QCA_NL80211_VENDOR_SUBCMD_TDLS_GET_STATUS = 36,
	QCA_NL80211_VENDOR_SUBCMD_TDLS_STATE = 37,
	QCA_NL80211_VENDOR_SUBCMD_GET_SUPPORTED_FEATURES = 38,
	QCA_NL80211_VENDOR_SUBCMD_SCANNING_MAC_OUI = 39,
	QCA_NL80211_VENDOR_SUBCMD_NO_DFS_FLAG = 40,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_HOTLIST_AP_LOST = 41,
	QCA_NL80211_VENDOR_SUBCMD_GET_CONCURRENCY_MATRIX = 42,
	/* 43..49 - reserved for QCA */
	QCA_NL80211_VENDOR_SUBCMD_KEY_MGMT_SET_KEY = 50,
	QCA_NL80211_VENDOR_SUBCMD_KEY_MGMT_ROAM_AUTH = 51,
	QCA_NL80211_VENDOR_SUBCMD_APFIND = 52,
	/* 53 - reserved - was used by QCA, but not in use anymore */
	QCA_NL80211_VENDOR_SUBCMD_DO_ACS = 54,
	QCA_NL80211_VENDOR_SUBCMD_GET_FEATURES = 55,
	QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_STARTED = 56,
	QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_FINISHED = 57,
	QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_ABORTED = 58,
	QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_NOP_FINISHED = 59,
	QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_RADAR_DETECTED = 60,
	QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_INFO = 61,
	QCA_NL80211_VENDOR_SUBCMD_WIFI_LOGGER_START = 62,
	QCA_NL80211_VENDOR_SUBCMD_WIFI_LOGGER_MEMORY_DUMP = 63,
	QCA_NL80211_VENDOR_SUBCMD_ROAM = 64,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_SET_SSID_HOTLIST = 65,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_RESET_SSID_HOTLIST = 66,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_HOTLIST_SSID_FOUND = 67,
	QCA_NL80211_VENDOR_SUBCMD_GSCAN_HOTLIST_SSID_LOST = 68,
	QCA_NL80211_VENDOR_SUBCMD_PNO_SET_LIST = 69,
	QCA_NL80211_VENDOR_SUBCMD_PNO_SET_PASSPOINT_LIST = 70,
	QCA_NL80211_VENDOR_SUBCMD_PNO_RESET_PASSPOINT_LIST = 71,
	QCA_NL80211_VENDOR_SUBCMD_PNO_NETWORK_FOUND = 72,
	QCA_NL80211_VENDOR_SUBCMD_PNO_PASSPOINT_NETWORK_FOUND = 73,
	/* Wi-Fi configuration subcommands */
	QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION = 74,
	QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION = 75,
	QCA_NL80211_VENDOR_SUBCMD_GET_LOGGER_FEATURE_SET = 76,
	QCA_NL80211_VENDOR_SUBCMD_GET_RING_DATA = 77,
	QCA_NL80211_VENDOR_SUBCMD_TDLS_GET_CAPABILITIES = 78,
	QCA_NL80211_VENDOR_SUBCMD_OFFLOADED_PACKETS = 79,
	QCA_NL80211_VENDOR_SUBCMD_MONITOR_RSSI = 80,
	QCA_NL80211_VENDOR_SUBCMD_NDP = 81,
	QCA_NL80211_VENDOR_SUBCMD_ND_OFFLOAD = 82,
	QCA_NL80211_VENDOR_SUBCMD_PACKET_FILTER = 83,
	QCA_NL80211_VENDOR_SUBCMD_GET_BUS_SIZE = 84,
	QCA_NL80211_VENDOR_SUBCMD_GET_WAKE_REASON_STATS = 85,
	/* 86-90 - reserved for QCA */
	QCA_NL80211_VENDOR_SUBCMD_DATA_OFFLOAD = 91,
	QCA_NL80211_VENDOR_SUBCMD_OCB_SET_CONFIG = 92,
	QCA_NL80211_VENDOR_SUBCMD_OCB_SET_UTC_TIME = 93,
	QCA_NL80211_VENDOR_SUBCMD_OCB_START_TIMING_ADVERT = 94,
	QCA_NL80211_VENDOR_SUBCMD_OCB_STOP_TIMING_ADVERT = 95,
	QCA_NL80211_VENDOR_SUBCMD_OCB_GET_TSF_TIMER = 96,
	QCA_NL80211_VENDOR_SUBCMD_DCC_GET_STATS = 97,
	QCA_NL80211_VENDOR_SUBCMD_DCC_CLEAR_STATS = 98,
	QCA_NL80211_VENDOR_SUBCMD_DCC_UPDATE_NDL = 99,
	QCA_NL80211_VENDOR_SUBCMD_DCC_STATS_EVENT = 100,
	QCA_NL80211_VENDOR_SUBCMD_LINK_PROPERTIES = 101,
	QCA_NL80211_VENDOR_SUBCMD_GW_PARAM_CONFIG = 102,
	QCA_NL80211_VENDOR_SUBCMD_GET_PREFERRED_FREQ_LIST = 103,
	QCA_NL80211_VENDOR_SUBCMD_SET_PROBABLE_OPER_CHANNEL = 104,
	QCA_NL80211_VENDOR_SUBCMD_SETBAND = 105,
	QCA_NL80211_VENDOR_SUBCMD_TRIGGER_SCAN = 106,
	QCA_NL80211_VENDOR_SUBCMD_SCAN_DONE = 107,
	QCA_NL80211_VENDOR_SUBCMD_OTA_TEST = 108,
	QCA_NL80211_VENDOR_SUBCMD_SET_TXPOWER_SCALE = 109,
	/* 110..114 - reserved for QCA */
	QCA_NL80211_VENDOR_SUBCMD_SET_TXPOWER_DECR_DB = 115,
	QCA_NL80211_VENDOR_SUBCMD_ACS_POLICY = 116,
	/* 117 - reserved for QCA */
	QCA_NL80211_VENDOR_SUBCMD_SET_SAP_CONFIG = 118,
	QCA_NL80211_VENDOR_SUBCMD_TSF = 119,
	QCA_NL80211_VENDOR_SUBCMD_WISA = 120,
	/* 121 - reserved for QCA */
	QCA_NL80211_VENDOR_SUBCMD_P2P_LISTEN_OFFLOAD_START = 122,
	QCA_NL80211_VENDOR_SUBCMD_P2P_LISTEN_OFFLOAD_STOP = 123,
	QCA_NL80211_VENDOR_SUBCMD_SAP_CONDITIONAL_CHAN_SWITCH = 124,
	QCA_NL80211_VENDOR_SUBCMD_GPIO_CONFIG_COMMAND = 125,
	QCA_NL80211_VENDOR_SUBCMD_GET_HW_CAPABILITY = 126,
	QCA_NL80211_VENDOR_SUBCMD_LL_STATS_EXT = 127,
	/* FTM/indoor location subcommands */
	QCA_NL80211_VENDOR_SUBCMD_LOC_GET_CAPA = 128,
	QCA_NL80211_VENDOR_SUBCMD_FTM_START_SESSION = 129,
	QCA_NL80211_VENDOR_SUBCMD_FTM_ABORT_SESSION = 130,
	QCA_NL80211_VENDOR_SUBCMD_FTM_MEAS_RESULT = 131,
	QCA_NL80211_VENDOR_SUBCMD_FTM_SESSION_DONE = 132,
	QCA_NL80211_VENDOR_SUBCMD_FTM_CFG_RESPONDER = 133,
	QCA_NL80211_VENDOR_SUBCMD_AOA_MEAS = 134,
	QCA_NL80211_VENDOR_SUBCMD_AOA_ABORT_MEAS = 135,
	QCA_NL80211_VENDOR_SUBCMD_AOA_MEAS_RESULT = 136,
	QCA_NL80211_VENDOR_SUBCMD_ENCRYPTION_TEST = 137,
	QCA_NL80211_VENDOR_SUBCMD_GET_CHAIN_RSSI = 138,
	/* DMG low level RF sector operations */
	QCA_NL80211_VENDOR_SUBCMD_DMG_RF_GET_SECTOR_CFG = 139,
	QCA_NL80211_VENDOR_SUBCMD_DMG_RF_SET_SECTOR_CFG = 140,
	QCA_NL80211_VENDOR_SUBCMD_DMG_RF_GET_SELECTED_SECTOR = 141,
	QCA_NL80211_VENDOR_SUBCMD_DMG_RF_SET_SELECTED_SECTOR = 142,
	QCA_NL80211_VENDOR_SUBCMD_CONFIGURE_TDLS = 143,
	QCA_NL80211_VENDOR_SUBCMD_GET_HE_CAPABILITIES = 144,
	QCA_NL80211_VENDOR_SUBCMD_ABORT_SCAN = 145,
	QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS = 146,
	QCA_NL80211_VENDOR_SUBCMD_EXTERNAL_ACS = 147,
	QCA_NL80211_VENDOR_SUBCMD_CHIP_PWRSAVE_FAILURE = 148,
	QCA_NL80211_VENDOR_SUBCMD_NUD_STATS_SET = 149,
	QCA_NL80211_VENDOR_SUBCMD_NUD_STATS_GET = 150,
	QCA_NL80211_VENDOR_SUBCMD_FETCH_BSS_TRANSITION_STATUS = 151,
	QCA_NL80211_VENDOR_SUBCMD_SET_TRACE_LEVEL = 152,
	QCA_NL80211_VENDOR_SUBCMD_BRP_SET_ANT_LIMIT = 153,
	QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START = 154,
	QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_STOP = 155,
	QCA_NL80211_VENDOR_SUBCMD_ACTIVE_TOS = 156,
	QCA_NL80211_VENDOR_SUBCMD_HANG = 157,
	QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CONFIG = 158,
	QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_DIAG_STATS = 159,
	QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CAP_INFO = 160,
	QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_STATUS = 161,
	/* Flush peer pending data */
	QCA_NL80211_VENDOR_SUBCMD_PEER_FLUSH_PENDING = 162,
	QCA_NL80211_VENDOR_SUBCMD_GET_RROP_INFO = 163,
	QCA_NL80211_VENDOR_SUBCMD_GET_SAR_LIMITS = 164,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_MAC_INFO = 165,
	QCA_NL80211_VENDOR_SUBCMD_SET_QDEPTH_THRESH = 166,
	/* Thermal shutdown commands to protect wifi chip */
	QCA_NL80211_VENDOR_SUBCMD_THERMAL_CMD = 167,
	QCA_NL80211_VENDOR_SUBCMD_THERMAL_EVENT = 168,
	/* Wi-Fi test configuration subcommand */
	QCA_NL80211_VENDOR_SUBCMD_WIFI_TEST_CONFIGURATION = 169,
	/* Frame filter operations for other BSSs/unassociated STAs */
	QCA_NL80211_VENDOR_SUBCMD_BSS_FILTER = 170,
	QCA_NL80211_VENDOR_SUBCMD_NAN_EXT = 171,
	QCA_NL80211_VENDOR_SUBCMD_ROAM_SCAN_EVENT = 172,
	QCA_NL80211_VENDOR_SUBCMD_PEER_CFR_CAPTURE_CFG = 173,
	QCA_NL80211_VENDOR_SUBCMD_THROUGHPUT_CHANGE_EVENT = 174,
	QCA_NL80211_VENDOR_SUBCMD_COEX_CONFIG = 175,
	QCA_NL80211_VENDOR_SUBCMD_GET_SUPPORTED_AKMS = 176,
	QCA_NL80211_VENDOR_SUBCMD_GET_FW_STATE = 177,
	QCA_NL80211_VENDOR_SUBCMD_PEER_STATS_CACHE_FLUSH = 178,
	QCA_NL80211_VENDOR_SUBCMD_MPTA_HELPER_CONFIG = 179,
	QCA_NL80211_VENDOR_SUBCMD_BEACON_REPORTING = 180,
	QCA_NL80211_VENDOR_SUBCMD_INTEROP_ISSUES_AP = 181,
	QCA_NL80211_VENDOR_SUBCMD_OEM_DATA = 182,
	QCA_NL80211_VENDOR_SUBCMD_AVOID_FREQUENCY_EXT = 183,
	QCA_NL80211_VENDOR_SUBCMD_ADD_STA_NODE = 184,
	QCA_NL80211_VENDOR_SUBCMD_BTC_CHAIN_MODE = 185,
	QCA_NL80211_VENDOR_SUBCMD_GET_STA_INFO = 186,
	QCA_NL80211_VENDOR_SUBCMD_GET_SAR_LIMITS_EVENT = 187,
	QCA_NL80211_VENDOR_SUBCMD_UPDATE_STA_INFO = 188,
	QCA_NL80211_VENDOR_SUBCMD_DRIVER_DISCONNECT_REASON = 189,
	QCA_NL80211_VENDOR_SUBCMD_CONFIG_TSPEC = 190,
	QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT = 191,
	QCA_NL80211_VENDOR_SUBCMD_GETBAND = 192,
	QCA_NL80211_VENDOR_SUBCMD_MEDIUM_ASSESS = 193,
	QCA_NL80211_VENDOR_SUBCMD_UPDATE_SSID = 194,
	QCA_NL80211_VENDOR_SUBCMD_WIFI_FW_STATS = 195,
	QCA_NL80211_VENDOR_SUBCMD_MBSSID_TX_VDEV_STATUS = 196,
	QCA_NL80211_VENDOR_SUBCMD_CONCURRENT_POLICY = 197,
	QCA_NL80211_VENDOR_SUBCMD_USABLE_CHANNELS = 198,
	QCA_NL80211_VENDOR_SUBCMD_GET_RADAR_HISTORY = 199,
	QCA_NL80211_VENDOR_SUBCMD_MDNS_OFFLOAD = 200,
	/* 201 - reserved for QCA */
	QCA_NL80211_VENDOR_SUBCMD_SET_MONITOR_MODE = 202,
	QCA_NL80211_VENDOR_SUBCMD_ROAM_EVENTS = 203,
	QCA_NL80211_VENDOR_SUBCMD_RATEMASK_CONFIG = 204,
	QCA_NL80211_VENDOR_SUBCMD_MCC_QUOTA = 205,
	/* 206..212 - reserved for QCA */
	QCA_NL80211_VENDOR_SUBCMD_GET_RADIO_COMBINATION_MATRIX = 213,
	QCA_NL80211_VENDOR_SUBCMD_DRIVER_READY = 214,
	QCA_NL80211_VENDOR_SUBCMD_PASN = 215,
	QCA_NL80211_VENDOR_SUBCMD_SECURE_RANGING_CONTEXT = 216,
	QCA_NL80211_VENDOR_SUBCMD_COAP_OFFLOAD = 217,
	QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG = 218,
	QCA_NL80211_VENDOR_SUBCMD_GET_SAR_CAPABILITY = 219,
	QCA_NL80211_VENDOR_SUBCMD_SR = 220,
	QCA_NL80211_VENDOR_SUBCMD_MLO_PEER_PRIM_NETDEV_EVENT = 221,
	QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT = 222,
	QCA_NL80211_VENDOR_SUBCMD_AFC_RESPONSE = 223,
	QCA_NL80211_VENDOR_SUBCMD_DOZED_AP = 224,
	QCA_NL80211_VENDOR_SUBCMD_GET_MONITOR_MODE = 225,
	QCA_NL80211_VENDOR_SUBCMD_ROAM_STATS = 226,
	QCA_NL80211_VENDOR_SUBCMD_MLO_LINK_STATE = 227,
	QCA_NL80211_VENDOR_SUBCMD_CONNECTED_CHANNEL_STATS = 228,
	QCA_NL80211_VENDOR_SUBCMD_TID_TO_LINK_MAP = 229,
	QCA_NL80211_VENDOR_SUBCMD_LINK_RECONFIG = 230,
	QCA_NL80211_VENDOR_SUBCMD_TDLS_DISC_RSP_EXT = 231,
	/* 232 - reserved for QCA */
	QCA_NL80211_VENDOR_SUBCMD_TX_LATENCY = 233,
	/* 234 - reserved for QCA */
	QCA_NL80211_VENDOR_SUBCMD_SDWF_PHY_OPS = 235,
	QCA_NL80211_VENDOR_SUBCMD_SDWF_DEV_OPS = 236,
	QCA_NL80211_VENDOR_SUBCMD_REGULATORY_TPC_INFO = 237,
	QCA_NL80211_VENDOR_SUBCMD_FW_PAGE_FAULT_REPORT = 238,
	QCA_NL80211_VENDOR_SUBCMD_FLOW_POLICY = 239,
	QCA_NL80211_VENDOR_SUBCMD_DISASSOC_PEER = 240,
	QCA_NL80211_VENDOR_SUBCMD_ADJUST_TX_POWER = 241,
	QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_COMPLETE = 242,
	QCA_NL80211_VENDOR_SUBCMD_ASYNC_GET_STATION = 243,
	QCA_NL80211_VENDOR_SUBCMD_AP_SUSPEND = 244,
	QCA_NL80211_VENDOR_SUBCMD_FLOW_STATS = 245,
	QCA_NL80211_VENDOR_SUBCMD_FLOW_CLASSIFY_RESULT = 246,
	QCA_NL80211_VENDOR_SUBCMD_ASYNC_STATS_POLICY = 247,
	QCA_NL80211_VENDOR_SUBCMD_CLASSIFIED_FLOW_REPORT = 248,
};

/* Compatibility defines for previously used subcmd names.
 * These values should not be used in any new implementation.
 */
#define QCA_NL80211_VENDOR_SUBCMD_CONCURRENT_MULTI_STA_POLICY \
	QCA_NL80211_VENDOR_SUBCMD_CONCURRENT_POLICY

enum qca_wlan_vendor_attr {
	QCA_WLAN_VENDOR_ATTR_INVALID = 0,
	/* used by QCA_NL80211_VENDOR_SUBCMD_DFS_CAPABILITY */
	QCA_WLAN_VENDOR_ATTR_DFS     = 1,
	/* Used only when driver sends vendor events to the userspace under the
	 * command QCA_NL80211_VENDOR_SUBCMD_NAN. Not used when userspace sends
	 * commands to the driver.
	 */
	QCA_WLAN_VENDOR_ATTR_NAN     = 2,
	/* used by QCA_NL80211_VENDOR_SUBCMD_STATS_EXT */
	QCA_WLAN_VENDOR_ATTR_STATS_EXT     = 3,
	/* used by QCA_NL80211_VENDOR_SUBCMD_STATS_EXT */
	QCA_WLAN_VENDOR_ATTR_IFINDEX     = 4,
	/* used by QCA_NL80211_VENDOR_SUBCMD_ROAMING, u32 with values defined
	 * by enum qca_roaming_policy.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAMING_POLICY = 5,
	QCA_WLAN_VENDOR_ATTR_MAC_ADDR = 6,
	/* used by QCA_NL80211_VENDOR_SUBCMD_GET_FEATURES */
	QCA_WLAN_VENDOR_ATTR_FEATURE_FLAGS = 7,
	QCA_WLAN_VENDOR_ATTR_TEST = 8,
	/* used by QCA_NL80211_VENDOR_SUBCMD_GET_FEATURES */
	/* Unsigned 32-bit value. */
	QCA_WLAN_VENDOR_ATTR_CONCURRENCY_CAPA = 9,
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_MAX_CONCURRENT_CHANNELS_2_4_BAND = 10,
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_MAX_CONCURRENT_CHANNELS_5_0_BAND = 11,
	/* Unsigned 32-bit value from enum qca_set_band. The allowed values for
	 * this attribute are limited to QCA_SETBAND_AUTO, QCA_SETBAND_5G, and
	 * QCA_SETBAND_2G. This attribute is deprecated. Recommendation is to
	 * use QCA_WLAN_VENDOR_ATTR_SETBAND_MASK instead.
	 */
	QCA_WLAN_VENDOR_ATTR_SETBAND_VALUE = 12,
	/* Attribute used for padding for 64-bit alignment */
	QCA_WLAN_VENDOR_ATTR_PAD = 13,
	/* Unique FTM session cookie (Unsigned 64 bit). Specified in
	 * QCA_NL80211_VENDOR_SUBCMD_FTM_START_SESSION. Reported in
	 * the session in QCA_NL80211_VENDOR_SUBCMD_FTM_MEAS_RESULT and
	 * QCA_NL80211_VENDOR_SUBCMD_FTM_SESSION_DONE.
	 */
	QCA_WLAN_VENDOR_ATTR_FTM_SESSION_COOKIE = 14,
	/* Indoor location capabilities, returned by
	 * QCA_NL80211_VENDOR_SUBCMD_LOC_GET_CAPA.
	 * see enum qca_wlan_vendor_attr_loc_capa.
	 */
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA = 15,
	/* Array of nested attributes containing information about each peer
	 * in FTM measurement session. See enum qca_wlan_vendor_attr_peer_info
	 * for supported attributes for each peer.
	 */
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PEERS = 16,
	/* Array of nested attributes containing measurement results for
	 * one or more peers, reported by the
	 * QCA_NL80211_VENDOR_SUBCMD_FTM_MEAS_RESULT event.
	 * See enum qca_wlan_vendor_attr_peer_result for list of supported
	 * attributes.
	 */
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PEER_RESULTS = 17,
	/* Flag attribute for enabling or disabling responder functionality. */
	QCA_WLAN_VENDOR_ATTR_FTM_RESPONDER_ENABLE = 18,
	/* Used in the QCA_NL80211_VENDOR_SUBCMD_FTM_CFG_RESPONDER
	 * command to specify the LCI report that will be sent by
	 * the responder during a measurement exchange. The format is
	 * defined in IEEE P802.11-REVmc/D7.0, 9.4.2.22.10.
	 */
	QCA_WLAN_VENDOR_ATTR_FTM_LCI = 19,
	/* Used in the QCA_NL80211_VENDOR_SUBCMD_FTM_CFG_RESPONDER
	 * command to specify the location civic report that will
	 * be sent by the responder during a measurement exchange.
	 * The format is defined in IEEE P802.11-REVmc/D7.0, 9.4.2.22.13.
	 */
	QCA_WLAN_VENDOR_ATTR_FTM_LCR = 20,
	/* Session/measurement completion status code,
	 * reported in QCA_NL80211_VENDOR_SUBCMD_FTM_SESSION_DONE and
	 * QCA_NL80211_VENDOR_SUBCMD_AOA_MEAS_RESULT
	 * see enum qca_vendor_attr_loc_session_status.
	 */
	QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS = 21,
	/* Initial dialog token used by responder (0 if not specified),
	 * unsigned 8 bit value.
	 */
	QCA_WLAN_VENDOR_ATTR_FTM_INITIAL_TOKEN = 22,
	/* AOA measurement type. Requested in QCA_NL80211_VENDOR_SUBCMD_AOA_MEAS
	 * and optionally in QCA_NL80211_VENDOR_SUBCMD_FTM_START_SESSION if
	 * AOA measurements are needed as part of an FTM session.
	 * Reported by QCA_NL80211_VENDOR_SUBCMD_AOA_MEAS_RESULT. See
	 * enum qca_wlan_vendor_attr_aoa_type.
	 */
	QCA_WLAN_VENDOR_ATTR_AOA_TYPE = 23,
	/* A bit mask (unsigned 32 bit value) of antenna arrays used
	 * by indoor location measurements. Refers to the antenna
	 * arrays described by QCA_VENDOR_ATTR_LOC_CAPA_ANTENNA_ARRAYS.
	 */
	QCA_WLAN_VENDOR_ATTR_LOC_ANTENNA_ARRAY_MASK = 24,
	/* AOA measurement data. Its contents depends on the AOA measurement
	 * type and antenna array mask:
	 * QCA_WLAN_VENDOR_ATTR_AOA_TYPE_TOP_CIR_PHASE: array of U16 values,
	 * phase of the strongest CIR path for each antenna in the measured
	 * array(s).
	 * QCA_WLAN_VENDOR_ATTR_AOA_TYPE_TOP_CIR_PHASE_AMP: array of 2 U16
	 * values, phase and amplitude of the strongest CIR path for each
	 * antenna in the measured array(s).
	 */
	QCA_WLAN_VENDOR_ATTR_AOA_MEAS_RESULT = 25,
	/* Used in QCA_NL80211_VENDOR_SUBCMD_GET_CHAIN_RSSI command
	 * to specify the chain number (unsigned 32 bit value) to inquire
	 * the corresponding antenna RSSI value
	 */
	QCA_WLAN_VENDOR_ATTR_CHAIN_INDEX = 26,
	/* Used in QCA_NL80211_VENDOR_SUBCMD_GET_CHAIN_RSSI command
	 * to report the specific antenna RSSI value (unsigned 32 bit value)
	 */
	QCA_WLAN_VENDOR_ATTR_CHAIN_RSSI = 27,
	/* Frequency in MHz, various uses. Unsigned 32 bit value */
	QCA_WLAN_VENDOR_ATTR_FREQ = 28,
	/* TSF timer value, unsigned 64 bit value.
	 * May be returned by various commands.
	 */
	QCA_WLAN_VENDOR_ATTR_TSF = 29,
	/* DMG RF sector index, unsigned 16 bit number. Valid values are
	 * 0..127 for sector indices or 65535 as special value used to
	 * unlock sector selection in
	 * QCA_NL80211_VENDOR_SUBCMD_DMG_RF_SET_SELECTED_SECTOR.
	 */
	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_INDEX = 30,
	/* DMG RF sector type, unsigned 8 bit value. One of the values
	 * in enum qca_wlan_vendor_attr_dmg_rf_sector_type.
	 */
	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE = 31,
	/* Bitmask of DMG RF modules for which information is requested. Each
	 * bit corresponds to an RF module with the same index as the bit
	 * number. Unsigned 32 bit number but only low 8 bits can be set since
	 * all DMG chips currently have up to 8 RF modules.
	 */
	QCA_WLAN_VENDOR_ATTR_DMG_RF_MODULE_MASK = 32,
	/* Array of nested attributes where each entry is DMG RF sector
	 * configuration for a single RF module.
	 * Attributes for each entry are taken from enum
	 * qca_wlan_vendor_attr_dmg_rf_sector_cfg.
	 * Specified in QCA_NL80211_VENDOR_SUBCMD_DMG_RF_SET_SECTOR_CFG
	 * and returned by QCA_NL80211_VENDOR_SUBCMD_DMG_RF_GET_SECTOR_CFG.
	 */
	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG = 33,
	/* Used in QCA_NL80211_VENDOR_SUBCMD_STATS_EXT command
	 * to report frame aggregation statistics to userspace.
	 */
	QCA_WLAN_VENDOR_ATTR_RX_AGGREGATION_STATS_HOLES_NUM = 34,
	QCA_WLAN_VENDOR_ATTR_RX_AGGREGATION_STATS_HOLES_INFO = 35,
	/* Unsigned 8-bit value representing MBO transition reason code as
	 * provided by the AP used by subcommand
	 * QCA_NL80211_VENDOR_SUBCMD_FETCH_BSS_TRANSITION_STATUS. This is
	 * specified by the userspace in the request to the driver.
	 */
	QCA_WLAN_VENDOR_ATTR_BTM_MBO_TRANSITION_REASON = 36,
	/* Array of nested attributes, BSSID and status code, used by subcommand
	 * QCA_NL80211_VENDOR_SUBCMD_FETCH_BSS_TRANSITION_STATUS, where each
	 * entry is taken from enum qca_wlan_vendor_attr_btm_candidate_info.
	 * The userspace space specifies the list/array of candidate BSSIDs in
	 * the order of preference in the request. The driver specifies the
	 * status code, for each BSSID in the list, in the response. The
	 * acceptable candidates are listed in the order preferred by the
	 * driver.
	 */
	QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO = 37,
	/* Used in QCA_NL80211_VENDOR_SUBCMD_BRP_SET_ANT_LIMIT command
	 * See enum qca_wlan_vendor_attr_brp_ant_limit_mode.
	 */
	QCA_WLAN_VENDOR_ATTR_BRP_ANT_LIMIT_MODE = 38,
	/* Used in QCA_NL80211_VENDOR_SUBCMD_BRP_SET_ANT_LIMIT command
	 * to define the number of antennas to use for BRP.
	 * different purpose in each ANT_LIMIT_MODE:
	 * DISABLE - ignored
	 * EFFECTIVE - upper limit to number of antennas to be used
	 * FORCE - exact number of antennas to be used
	 * unsigned 8 bit value
	 */
	QCA_WLAN_VENDOR_ATTR_BRP_ANT_NUM_LIMIT = 39,
	/* Used in QCA_NL80211_VENDOR_SUBCMD_GET_CHAIN_RSSI command
	 * to report the corresponding antenna index to the chain RSSI value
	 */
	QCA_WLAN_VENDOR_ATTR_ANTENNA_INFO = 40,
	/* Used in QCA_NL80211_VENDOR_SUBCMD_GET_CHAIN_RSSI command to report
	 * the specific antenna EVM value (unsigned 32 bit value). With a
	 * determinate group of antennas, the driver specifies the EVM value
	 * for each antenna ID, and application extract them in user space.
	 */
	QCA_WLAN_VENDOR_ATTR_CHAIN_EVM = 41,
	/*
	 * Used in QCA_NL80211_VENDOR_SUBCMD_GET_FW_STATE command to report
	 * wlan firmware current state. FW state is an unsigned 8 bit value,
	 * one of the values in enum qca_wlan_vendor_attr_fw_state.
	 */
	QCA_WLAN_VENDOR_ATTR_FW_STATE = 42,

	/* Unsigned 32-bitmask value from enum qca_set_band. Substitutes the
	 * attribute QCA_WLAN_VENDOR_ATTR_SETBAND_VALUE for which only a subset
	 * of single values from enum qca_set_band are valid. This attribute
	 * uses bitmask combinations to define the respective allowed band
	 * combinations and this attributes takes precedence over
	 * QCA_WLAN_VENDOR_ATTR_SETBAND_VALUE if both attributes are included.
	 */
	QCA_WLAN_VENDOR_ATTR_SETBAND_MASK = 43,

	/* Unsigned 8-bit used by QCA_NL80211_VENDOR_SUBCMD_GET_FEATURES.
	 * This field describes the maximum number of links supported by the
	 * chip for MLO association.
	 * This is an optional attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_MLO_CAPABILITY_MAX_ASSOCIATION_COUNT = 44,

	/* Unsigned 8-bit used by QCA_NL80211_VENDOR_SUBCMD_GET_FEATURES.
	 * This field describes the maximum number of Simultaneous Transmit
	 * and Receive (STR) links used in Multi-Link Operation.
	 * The maximum number of STR links used can be different
	 * from the maximum number of radios supported by the chip.
	 * This is a static configuration of the chip.
	 * This is an optional attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_MLO_CAPABILITY_MAX_STR_LINK_COUNT = 45,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_MAX	= QCA_WLAN_VENDOR_ATTR_AFTER_LAST - 1,
};

/**
 * enum qca_roaming_policy - Represents the policies for roaming. Used by
 * QCA_WLAN_VENDOR_ATTR_ROAMING_POLICY.
 *
 * QCA_ROAMING_NOT_ALLOWED: Roaming is not allowed/disabled.
 *
 * QCA_ROAMING_ALLOWED_WITHIN_ESS: Roaming is allowed with in an ESS with
 * default RSSI thresholds.
 *
 * QCA_ROAMING_MODE_AGGRESSIVE: This mode is an extension of
 * QCA_ROAMING_ALLOWED_WITHIN_ESS. The driver/firmware roams on higher RSSI
 * thresholds when compared to QCA_ROAMING_ALLOWED_WITHIN_ESS.
 */
enum qca_roaming_policy {
	QCA_ROAMING_NOT_ALLOWED,
	QCA_ROAMING_ALLOWED_WITHIN_ESS,
	QCA_ROAMING_MODE_AGGRESSIVE,
};

/**
 * enum qca_roam_reason - Represents the reason codes for roaming. Used by
 * QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_REASON.
 *
 * @QCA_ROAM_REASON_UNKNOWN: Any reason that do not classify under the below
 * reasons.
 *
 * @QCA_ROAM_REASON_PER: Roam triggered when packet error rates (PER) breached
 * the configured threshold.
 *
 * @QCA_ROAM_REASON_BEACON_MISS: Roam triggered due to the continuous configured
 * beacon misses from the then connected AP.
 *
 * @QCA_ROAM_REASON_POOR_RSSI: Roam triggered due to the poor RSSI reported
 * by the connected AP.
 *
 * @QCA_ROAM_REASON_BETTER_RSSI: Roam triggered for finding a BSS with a better
 * RSSI than the connected BSS. Here the RSSI of the current BSS is not poor.
 *
 * @QCA_ROAM_REASON_CONGESTION: Roam triggered considering the connected channel
 * or environment being very noisy or congested.
 *
 * @QCA_ROAM_REASON_USER_TRIGGER: Roam triggered due to an explicit request
 * from the user (user space).
 *
 * @QCA_ROAM_REASON_BTM: Roam triggered due to BTM Request frame received from
 * the connected AP.
 *
 * @QCA_ROAM_REASON_BSS_LOAD: Roam triggered due to the channel utilization
 * breaching out the configured threshold.
 *
 * @QCA_ROAM_REASON_WTC: Roam triggered due to Wireless to Cellular BSS
 * transition request.
 *
 * @QCA_ROAM_REASON_IDLE: Roam triggered when device is suspended, there is no
 * data activity with the AP and the current RSSI falls below a certain
 * threshold.
 *
 * @QCA_ROAM_REASON_DISCONNECTION: Roam triggered due to Deauthentication or
 * Disassociation frames received from the connected AP.
 *
 * @QCA_ROAM_REASON_PERIODIC_TIMER: Roam triggered as part of the periodic scan
 * that happens when there is no candidate AP found during the poor RSSI scan
 * trigger.
 *
 * @QCA_ROAM_REASON_BACKGROUND_SCAN: Roam triggered based on the scan results
 * obtained from an external scan (not aimed at roaming).
 *
 * @QCA_ROAM_REASON_BT_ACTIVITY: Roam triggered due to Bluetooth connection is
 * established when the station is connected in the 2.4 GHz band.
 *
 * @QCA_ROAM_REASON_STA_KICKOUT: Roam triggered due to continuous TX Data frame
 * failures to the connected AP.
 */
enum qca_roam_reason {
	QCA_ROAM_REASON_UNKNOWN,
	QCA_ROAM_REASON_PER,
	QCA_ROAM_REASON_BEACON_MISS,
	QCA_ROAM_REASON_POOR_RSSI,
	QCA_ROAM_REASON_BETTER_RSSI,
	QCA_ROAM_REASON_CONGESTION,
	QCA_ROAM_REASON_USER_TRIGGER,
	QCA_ROAM_REASON_BTM,
	QCA_ROAM_REASON_BSS_LOAD,
	QCA_ROAM_REASON_WTC,
	QCA_ROAM_REASON_IDLE,
	QCA_ROAM_REASON_DISCONNECTION,
	QCA_ROAM_REASON_PERIODIC_TIMER,
	QCA_ROAM_REASON_BACKGROUND_SCAN,
	QCA_ROAM_REASON_BT_ACTIVITY,
	QCA_ROAM_REASON_STA_KICKOUT,
};

enum qca_wlan_vendor_attr_roam_auth {
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_INVALID = 0,
	/* Indicates BSSID of the roamed AP for non-MLO roaming and MLD address
	 * of the roamed AP for MLO roaming.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_BSSID,
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_REQ_IE,
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_RESP_IE,
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_AUTHORIZED,
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_KEY_REPLAY_CTR,
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_PTK_KCK,
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_PTK_KEK,
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_SUBNET_STATUS,
	/* Indicates the status of re-association requested by user space for
	 * the BSSID specified by QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_BSSID.
	 * Type u16.
	 * Represents the status code from AP. Use
	 * %WLAN_STATUS_UNSPECIFIED_FAILURE if the device cannot give you the
	 * real status code for failures.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_STATUS,
	/* This attribute indicates that the old association was maintained when
	 * a re-association is requested by user space and that re-association
	 * attempt fails (i.e., cannot connect to the requested BSS, but can
	 * remain associated with the BSS with which the association was in
	 * place when being requested to roam). Used along with
	 * WLAN_VENDOR_ATTR_ROAM_AUTH_STATUS to indicate the current
	 * re-association status. Type flag.
	 * This attribute is applicable only for re-association failure cases.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_RETAIN_CONNECTION,
	/* This attribute specifies the PMK if one was newly generated during
	 * FILS roaming. This is added to the PMKSA cache and is used in
	 * subsequent connections with PMKSA caching.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_PMK = 11,
	/* This attribute specifies the PMKID used/generated for the current
	 * FILS roam. This is used in subsequent connections with PMKSA caching.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_PMKID = 12,
	/* A 16-bit unsigned value specifying the next sequence number to use
	 * in ERP message in the currently associated realm. This is used in
	 * doing subsequent ERP based connections in the same realm.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_FILS_ERP_NEXT_SEQ_NUM = 13,
	/* A 16-bit unsigned value representing the reasons for the roaming.
	 * Defined by enum qca_roam_reason.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_REASON = 14,
	/* A nested attribute containing per-link information of all the links
	 * of MLO connection done while roaming. The attributes used inside this
	 * nested attribute are defined in enum qca_wlan_vendor_attr_mlo_links.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_MLO_LINKS = 15,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_MAX =
	QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_AFTER_LAST - 1
};

enum qca_wlan_vendor_attr_p2p_listen_offload {
	QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_INVALID = 0,
	/* A 32-bit unsigned value; the P2P listen frequency (MHz); must be one
	 * of the social channels.
	 */
	QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_CHANNEL,
	/* A 32-bit unsigned value; the P2P listen offload period (ms).
	 */
	QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_PERIOD,
	/* A 32-bit unsigned value; the P2P listen interval duration (ms).
	 */
	QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_INTERVAL,
	/* A 32-bit unsigned value; number of interval times the firmware needs
	 * to run the offloaded P2P listen operation before it stops.
	 */
	QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_COUNT,
	/* An array of arbitrary binary data with one or more 8-byte values.
	 * The device types include both primary and secondary device types.
	 */
	QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_DEVICE_TYPES,
	/* An array of unsigned 8-bit characters; vendor information elements.
	 */
	QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_VENDOR_IE,
	/* A 32-bit unsigned value; a control flag to indicate whether listen
	 * results need to be flushed to wpa_supplicant.
	 */
	QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_CTRL_FLAG,
	/* A 8-bit unsigned value; reason code for P2P listen offload stop
	 * event.
	 */
	QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_STOP_REASON,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_MAX =
	QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_AFTER_LAST - 1
};

/**
 * enum qca_wlan_vendor_attr_acs_offload - Defines attributes to be used with
 * vendor command/event QCA_NL80211_VENDOR_SUBCMD_DO_ACS.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_PRIMARY_CHANNEL: Required (u8).
 * Used with event to notify the primary channel number selected in ACS
 * operation.
 * Note: If both the driver and user-space application supports the 6 GHz band,
 * QCA_WLAN_VENDOR_ATTR_ACS_PRIMARY_CHANNEL is deprecated; use
 * QCA_WLAN_VENDOR_ATTR_ACS_PRIMARY_FREQUENCY instead.
 * To maintain backward compatibility, QCA_WLAN_VENDOR_ATTR_ACS_PRIMARY_CHANNEL
 * is still used if either of the driver or user space application doesn't
 * support the 6 GHz band.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_SECONDARY_CHANNEL: Required (u8).
 * Used with event to notify the secondary channel number selected in ACS
 * operation.
 * Note: If both the driver and user-space application supports the 6 GHz band,
 * QCA_WLAN_VENDOR_ATTR_ACS_SECONDARY_CHANNEL is deprecated; use
 * QCA_WLAN_VENDOR_ATTR_ACS_SECONDARY_FREQUENCY instead.
 * To maintain backward compatibility,
 * QCA_WLAN_VENDOR_ATTR_ACS_SECONDARY_CHANNEL is still used if either of
 * the driver or user space application doesn't support 6 GHz band.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_HW_MODE: Required (u8).
 * (a) Used with command to configure hw_mode from
 * enum qca_wlan_vendor_acs_hw_mode for ACS operation.
 * (b) Also used with event to notify the hw_mode of selected primary channel
 * in ACS operation.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_HT_ENABLED: Flag attribute.
 * Used with command to configure ACS operation for HT mode.
 * Disable (flag attribute not present) - HT disabled and
 * Enable (flag attribute present) - HT enabled.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_HT40_ENABLED: Flag attribute.
 * Used with command to configure ACS operation for HT40 mode.
 * Disable (flag attribute not present) - HT40 disabled and
 * Enable (flag attribute present) - HT40 enabled.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_VHT_ENABLED: Flag attribute.
 * Used with command to configure ACS operation for VHT mode.
 * Disable (flag attribute not present) - VHT disabled and
 * Enable (flag attribute present) - VHT enabled.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_CHWIDTH: Optional (u16) with command and
 * mandatory with event.
 * If specified in command path, ACS operation is configured with the given
 * channel width (in MHz).
 * In event path, specifies the channel width of the primary channel selected.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_CH_LIST: Required and type is NLA_UNSPEC.
 * Used with command to configure channel list using an array of
 * channel numbers (u8).
 * Note: If both the driver and user-space application supports the 6 GHz band,
 * the driver mandates use of QCA_WLAN_VENDOR_ATTR_ACS_FREQ_LIST whereas
 * QCA_WLAN_VENDOR_ATTR_ACS_CH_LIST is optional.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG0_CENTER_CHANNEL: Required (u8).
 * Used with event to notify the VHT segment 0 center channel number selected in
 * ACS operation. The value is the index of the channel center frequency for
 * 20 MHz, 40 MHz, and 80 MHz channels. The value is the center frequency index
 * of the primary 80 MHz segment for 160 MHz and 80+80 MHz channels.
 * Note: If both the driver and user-space application supports the 6 GHz band,
 * QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG0_CENTER_CHANNEL is deprecated; use
 * QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG0_CENTER_FREQUENCY instead.
 * To maintain backward compatibility,
 * QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG0_CENTER_CHANNEL is still used if either of
 * the driver or user space application doesn't support the 6 GHz band.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG1_CENTER_CHANNEL: Required (u8).
 * Used with event to notify the VHT segment 1 center channel number selected in
 * ACS operation. The value is zero for 20 MHz, 40 MHz, and 80 MHz channels.
 * The value is the index of the channel center frequency for 160 MHz channels
 * and the center frequency index of the secondary 80 MHz segment for 80+80 MHz
 * channels.
 * Note: If both the driver and user-space application supports the 6 GHz band,
 * QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG1_CENTER_CHANNEL is deprecated; use
 * QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG1_CENTER_FREQUENCY instead.
 * To maintain backward compatibility,
 * QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG1_CENTER_CHANNEL is still used if either of
 * the driver or user space application doesn't support the 6 GHz band.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_FREQ_LIST: Required and type is NLA_UNSPEC.
 * Used with command to configure the channel list using an array of channel
 * center frequencies in MHz (u32).
 * Note: If both the driver and user-space application supports the 6 GHz band,
 * the driver first parses the frequency list and if it fails to get a frequency
 * list, parses the channel list specified using
 * QCA_WLAN_VENDOR_ATTR_ACS_CH_LIST (considers only 2 GHz and 5 GHz channels in
 * QCA_WLAN_VENDOR_ATTR_ACS_CH_LIST).
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_PRIMARY_FREQUENCY: Required (u32).
 * Used with event to notify the primary channel center frequency (MHz) selected
 * in ACS operation.
 * Note: If the driver supports the 6 GHz band, the event sent from the driver
 * includes this attribute along with QCA_WLAN_VENDOR_ATTR_ACS_PRIMARY_CHANNEL.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_SECONDARY_FREQUENCY: Required (u32).
 * Used with event to notify the secondary channel center frequency (MHz)
 * selected in ACS operation.
 * Note: If the driver supports the 6 GHz band, the event sent from the driver
 * includes this attribute along with
 * QCA_WLAN_VENDOR_ATTR_ACS_SECONDARY_CHANNEL.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG0_CENTER_FREQUENCY: Required (u32).
 * Used with event to notify the VHT segment 0 center channel frequency (MHz)
 * selected in ACS operation.
 * Note: If the driver supports the 6 GHz band, the event sent from the driver
 * includes this attribute along with
 * QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG0_CENTER_CHANNEL.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG1_CENTER_FREQUENCY: Required (u32).
 * Used with event to notify the VHT segment 1 center channel frequency (MHz)
 * selected in ACS operation.
 * Note: If the driver supports the 6 GHz band, the event sent from the driver
 * includes this attribute along with
 * QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG1_CENTER_CHANNEL.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_EDMG_ENABLED: Flag attribute.
 * Used with command to notify the driver of EDMG request for ACS
 * operation.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_EDMG_CHANNEL: Optional (u8).
 * Used with event to notify the EDMG channel number selected in ACS
 * operation.
 * EDMG primary channel is indicated by QCA_WLAN_VENDOR_ATTR_ACS_PRIMARY_CHANNEL
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_PUNCTURE_BITMAP: Optional (u16).
 * Used with event to notify the puncture pattern selected in ACS operation.
 * Encoding for this attribute will follow the convention used in the Disabled
 * Subchannel Bitmap field of the EHT Operation IE.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_EHT_ENABLED: Flag attribute.
 * Used with command to configure ACS operation for EHT mode.
 * Disable (flag attribute not present) - EHT disabled and
 * Enable (flag attribute present) - EHT enabled.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_LAST_SCAN_AGEOUT_TIME: Optional (u32).
 * Used with command to configure how older scan can be considered for ACS
 * scoring. In case scan was performed on a partial set of channels configured
 * with this command within last QCA_WLAN_VENDOR_ATTR_ACS_LAST_SCAN_AGEOUT_TIME
 * (in ms), scan only the remaining channels.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_LINK_ID: Mandatory on AP MLD (u8).
 * Used with command to configure ACS operation for a specific link affiliated
 * to an AP MLD.
 *
 */
enum qca_wlan_vendor_attr_acs_offload {
	QCA_WLAN_VENDOR_ATTR_ACS_CHANNEL_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ACS_PRIMARY_CHANNEL = 1,
	QCA_WLAN_VENDOR_ATTR_ACS_SECONDARY_CHANNEL = 2,
	QCA_WLAN_VENDOR_ATTR_ACS_HW_MODE = 3,
	QCA_WLAN_VENDOR_ATTR_ACS_HT_ENABLED = 4,
	QCA_WLAN_VENDOR_ATTR_ACS_HT40_ENABLED = 5,
	QCA_WLAN_VENDOR_ATTR_ACS_VHT_ENABLED = 6,
	QCA_WLAN_VENDOR_ATTR_ACS_CHWIDTH = 7,
	QCA_WLAN_VENDOR_ATTR_ACS_CH_LIST = 8,
	QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG0_CENTER_CHANNEL = 9,
	QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG1_CENTER_CHANNEL = 10,
	QCA_WLAN_VENDOR_ATTR_ACS_FREQ_LIST = 11,
	QCA_WLAN_VENDOR_ATTR_ACS_PRIMARY_FREQUENCY = 12,
	QCA_WLAN_VENDOR_ATTR_ACS_SECONDARY_FREQUENCY = 13,
	QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG0_CENTER_FREQUENCY = 14,
	QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG1_CENTER_FREQUENCY = 15,
	QCA_WLAN_VENDOR_ATTR_ACS_EDMG_ENABLED = 16,
	QCA_WLAN_VENDOR_ATTR_ACS_EDMG_CHANNEL = 17,
	QCA_WLAN_VENDOR_ATTR_ACS_PUNCTURE_BITMAP = 18,
	QCA_WLAN_VENDOR_ATTR_ACS_EHT_ENABLED = 19,
	QCA_WLAN_VENDOR_ATTR_ACS_LAST_SCAN_AGEOUT_TIME = 20,
	QCA_WLAN_VENDOR_ATTR_ACS_LINK_ID = 21,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ACS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ACS_MAX =
	QCA_WLAN_VENDOR_ATTR_ACS_AFTER_LAST - 1
};

/**
 * enum qca_wlan_vendor_acs_hw_mode - Defines HW mode to be used with the
 * vendor command/event QCA_NL80211_VENDOR_SUBCMD_DO_ACS.
 *
 * @QCA_ACS_MODE_IEEE80211B: 802.11b mode
 * @QCA_ACS_MODE_IEEE80211G: 802.11g mode
 * @QCA_ACS_MODE_IEEE80211A: 802.11a mode
 * @QCA_ACS_MODE_IEEE80211AD: 802.11ad mode
 * @QCA_ACS_MODE_IEEE80211ANY: all modes
 * @QCA_ACS_MODE_IEEE80211AX: 802.11ax mode
 */
enum qca_wlan_vendor_acs_hw_mode {
	QCA_ACS_MODE_IEEE80211B,
	QCA_ACS_MODE_IEEE80211G,
	QCA_ACS_MODE_IEEE80211A,
	QCA_ACS_MODE_IEEE80211AD,
	QCA_ACS_MODE_IEEE80211ANY,
	QCA_ACS_MODE_IEEE80211AX,
};

/**
 * enum qca_wlan_vendor_features - Vendor device/driver feature flags
 *
 * @QCA_WLAN_VENDOR_FEATURE_KEY_MGMT_OFFLOAD: Device supports key
 *	management offload, a mechanism where the station's firmware
 *	does the exchange with the AP to establish the temporal keys
 *	after roaming, rather than having the user space wpa_supplicant do it.
 * @QCA_WLAN_VENDOR_FEATURE_SUPPORT_HW_MODE_ANY: Device supports automatic
 *	band selection based on channel selection results.
 * @QCA_WLAN_VENDOR_FEATURE_OFFCHANNEL_SIMULTANEOUS: Device supports
 *	simultaneous off-channel operations.
 * @QCA_WLAN_VENDOR_FEATURE_P2P_LISTEN_OFFLOAD: Device supports P2P
 *	Listen offload; a mechanism where the station's firmware takes care of
 *	responding to incoming Probe Request frames received from other P2P
 *	Devices whilst in Listen state, rather than having the user space
 *	wpa_supplicant do it. Information from received P2P requests are
 *	forwarded from firmware to host whenever the host processor wakes up.
 * @QCA_WLAN_VENDOR_FEATURE_OCE_STA: Device supports all OCE non-AP STA
 *	specific features.
 * @QCA_WLAN_VENDOR_FEATURE_OCE_AP: Device supports all OCE AP specific
 *	features.
 * @QCA_WLAN_VENDOR_FEATURE_OCE_STA_CFON: Device supports OCE STA-CFON
 *	specific features only. If a Device sets this bit but not the
 *	%QCA_WLAN_VENDOR_FEATURE_OCE_AP, the userspace shall assume that
 *	this Device may not support all OCE AP functionalities but can support
 *	only OCE STA-CFON functionalities.
 * @QCA_WLAN_VENDOR_FEATURE_SELF_MANAGED_REGULATORY: Device supports self
 *	managed regulatory.
 * @QCA_WLAN_VENDOR_FEATURE_TWT: Device supports TWT (Target Wake Time).
 * @QCA_WLAN_VENDOR_FEATURE_11AX: Device supports 802.11ax (HE)
 * @QCA_WLAN_VENDOR_FEATURE_6GHZ_SUPPORT: Device supports 6 GHz band operation
 * @QCA_WLAN_VENDOR_FEATURE_THERMAL_CONFIG: Device is capable of receiving
 *	and applying thermal configuration through
 *	%QCA_WLAN_VENDOR_ATTR_THERMAL_LEVEL and
 *	%QCA_WLAN_VENDOR_ATTR_THERMAL_COMPLETION_WINDOW attributes from
 *	userspace.
 * @QCA_WLAN_VENDOR_FEATURE_ADAPTIVE_11R: Device supports Adaptive 11r.
 *	With Adaptive 11r feature, access points advertise the vendor
 *	specific IEs and MDE but do not include FT AKM in the RSNE.
 *	The Adaptive 11r supported stations are expected to identify
 *	such vendor specific IEs and connect to the AP in FT mode though
 *	the profile is configured in non-FT mode.
 *	The driver-based SME cases also need to have this support for
 *	Adaptive 11r to handle the connection and roaming scenarios.
 *	This flag indicates the support for the same to the user space.
 * @QCA_WLAN_VENDOR_FEATURE_CONCURRENT_BAND_SESSIONS: Device supports
 *	concurrent network sessions on different Wi-Fi bands. This feature
 *	capability is attributed to the hardware's capability to support
 *	the same (e.g., DBS).
 * @QCA_WLAN_VENDOR_FEATURE_TWT_ASYNC_SUPPORT: Flag indicating whether the
 *	responses for the respective TWT operations are asynchronous (separate
 *	event message) from the driver. If not specified, the responses are
 *	synchronous (in vendor command reply) to the request. Each TWT
 *	operation is specifically mentioned (against its respective
 *	documentation) to support either of these or both modes.
 * @QCA_WLAN_VENDOR_FEATURE_USE_ADD_DEL_VIRTUAL_INTF_FOR_NDI: Flag indicates
 *	that the driver requires add/del virtual interface path using the
 *	generic nl80211 commands for NDP interface create/delete and to
 *	register/unregister the netdev instead of creating/deleting the NDP
 *	interface using the vendor commands
 *	QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_CREATE and
 *	QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_DELETE. With the latest kernel
 *	(5.12 version onward), interface creation/deletion is not allowed using
 *	vendor commands as it leads to a deadlock while acquiring the RTNL_LOCK
 *	during the register/unregister of netdev. Create and delete NDP
 *	interface using NL80211_CMD_NEW_INTERFACE and NL80211_CMD_DEL_INTERFACE
 *	commands respectively if the driver advertises this capability set.
 * @QCA_WLAN_VENDOR_FEATURE_SECURE_LTF_STA: Flag indicates that the device in
 *	station mode supports secure LTF. If NL80211_EXT_FEATURE_SECURE_LTF is
 *	set, then QCA_WLAN_VENDOR_FEATURE_SECURE_LTF_STA will be ignored.
 * @QCA_WLAN_VENDOR_FEATURE_SECURE_LTF_AP: Flag indicates that the device in AP
 *	mode supports secure LTF. If NL80211_EXT_FEATURE_SECURE_LTF is set, then
 *	QCA_WLAN_VENDOR_FEATURE_SECURE_LTF_AP will be ignored.
 * @QCA_WLAN_VENDOR_FEATURE_SECURE_RTT_STA: Flag indicates that the device in
 *	station mode supports secure RTT measurement exchange. If
 *	NL80211_EXT_FEATURE_SECURE_RTT is set,
 *	QCA_WLAN_VENDOR_FEATURE_SECURE_RTT_STA will be ignored.
 * @QCA_WLAN_VENDOR_FEATURE_SECURE_RTT_AP: Flag indicates that the device in AP
 *	mode supports secure RTT measurement exchange. If
 *	NL80211_EXT_FEATURE_SECURE_RTT is set,
 *	QCA_WLAN_VENDOR_FEATURE_SECURE_RTT_AP will be ignored.
 * @QCA_WLAN_VENDOR_FEATURE_PROT_RANGE_NEGO_AND_MEASURE_STA: Flag indicates that
 *	the device in station mode supports protection of range negotiation and
 *	measurement management frames. If
 *	NL80211_EXT_FEATURE_PROT_RANGE_NEGO_AND_MEASURE is set, then
 *	QCA_WLAN_VENDOR_FEATURE_PROT_RANGE_NEGO_AND_MEASURE_STA will be ignored.
 * @QCA_WLAN_VENDOR_FEATURE_PROT_RANGE_NEGO_AND_MEASURE_AP: Flag indicates that
 *	the device in AP mode supports protection of range negotiation and
 *	measurement management frames. If
 *	NL80211_EXT_FEATURE_PROT_RANGE_NEGO_AND_MEASURE is set, then
 *	QCA_WLAN_VENDOR_FEATURE_PROT_RANGE_NEGO_AND_MEASURE_AP will be ignored.
 * @QCA_WLAN_VENDOR_FEATURE_AP_ALLOWED_FREQ_LIST: Flag indicates that the device
 *	in AP mode supports configuring allowed frequency list for AP operation
 *	with %QCA_WLAN_VENDOR_ATTR_CONFIG_AP_ALLOWED_FREQ_LIST.
 * @QCA_WLAN_VENDOR_FEATURE_ENHANCED_AUDIO_EXPERIENCE_OVER_WLAN: Flag indicates
 *	that the device supports enhanced audio experience over WLAN feature.
 * @QCA_WLAN_VENDOR_FEATURE_HT_VHT_TWT_RESPONDER: Flag indicates that the device
 *	in AP mode supports TWT responder mode in HT and VHT modes.
 * @NUM_QCA_WLAN_VENDOR_FEATURES: Number of assigned feature bits
 */
enum qca_wlan_vendor_features {
	QCA_WLAN_VENDOR_FEATURE_KEY_MGMT_OFFLOAD	= 0,
	QCA_WLAN_VENDOR_FEATURE_SUPPORT_HW_MODE_ANY     = 1,
	QCA_WLAN_VENDOR_FEATURE_OFFCHANNEL_SIMULTANEOUS = 2,
	QCA_WLAN_VENDOR_FEATURE_P2P_LISTEN_OFFLOAD	= 3,
	QCA_WLAN_VENDOR_FEATURE_OCE_STA                 = 4,
	QCA_WLAN_VENDOR_FEATURE_OCE_AP                  = 5,
	QCA_WLAN_VENDOR_FEATURE_OCE_STA_CFON            = 6,
	QCA_WLAN_VENDOR_FEATURE_SELF_MANAGED_REGULATORY = 7,
	QCA_WLAN_VENDOR_FEATURE_TWT			= 8,
	QCA_WLAN_VENDOR_FEATURE_11AX			= 9,
	QCA_WLAN_VENDOR_FEATURE_6GHZ_SUPPORT		= 10,
	QCA_WLAN_VENDOR_FEATURE_THERMAL_CONFIG		= 11,
	QCA_WLAN_VENDOR_FEATURE_ADAPTIVE_11R		= 12,
	QCA_WLAN_VENDOR_FEATURE_CONCURRENT_BAND_SESSIONS = 13,
	QCA_WLAN_VENDOR_FEATURE_TWT_ASYNC_SUPPORT	= 14,
	QCA_WLAN_VENDOR_FEATURE_USE_ADD_DEL_VIRTUAL_INTF_FOR_NDI = 15,
	QCA_WLAN_VENDOR_FEATURE_SECURE_LTF_STA		= 16,
	QCA_WLAN_VENDOR_FEATURE_SECURE_LTF_AP		= 17,
	QCA_WLAN_VENDOR_FEATURE_SECURE_RTT_STA		= 18,
	QCA_WLAN_VENDOR_FEATURE_SECURE_RTT_AP		= 19,
	QCA_WLAN_VENDOR_FEATURE_PROT_RANGE_NEGO_AND_MEASURE_STA = 20,
	QCA_WLAN_VENDOR_FEATURE_PROT_RANGE_NEGO_AND_MEASURE_AP = 21,
	QCA_WLAN_VENDOR_FEATURE_AP_ALLOWED_FREQ_LIST = 22,
	QCA_WLAN_VENDOR_FEATURE_ENHANCED_AUDIO_EXPERIENCE_OVER_WLAN = 23,
	QCA_WLAN_VENDOR_FEATURE_HT_VHT_TWT_RESPONDER = 24,
	NUM_QCA_WLAN_VENDOR_FEATURES /* keep last */
};

/**
 * enum qca_wlan_vendor_attr_data_offload_ind - Vendor Data Offload Indication
 *
 * @QCA_WLAN_VENDOR_ATTR_DATA_OFFLOAD_IND_SESSION: Session corresponding to
 *	the offloaded data.
 * @QCA_WLAN_VENDOR_ATTR_DATA_OFFLOAD_IND_PROTOCOL: Protocol of the offloaded
 *	data.
 * @QCA_WLAN_VENDOR_ATTR_DATA_OFFLOAD_IND_EVENT: Event type for the data offload
 *	indication.
 */
enum qca_wlan_vendor_attr_data_offload_ind {
	QCA_WLAN_VENDOR_ATTR_DATA_OFFLOAD_IND_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_DATA_OFFLOAD_IND_SESSION,
	QCA_WLAN_VENDOR_ATTR_DATA_OFFLOAD_IND_PROTOCOL,
	QCA_WLAN_VENDOR_ATTR_DATA_OFFLOAD_IND_EVENT,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_DATA_OFFLOAD_IND_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_DATA_OFFLOAD_IND_MAX =
	QCA_WLAN_VENDOR_ATTR_DATA_OFFLOAD_IND_AFTER_LAST - 1
};

/**
 * enum qca_wlan_vendor_attr_ocb_set_config - Vendor subcmd attributes to set
 *	OCB config
 *
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_CHANNEL_COUNT: Number of channels in the
 *	configuration
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_SCHEDULE_SIZE: Size of the schedule
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_CHANNEL_ARRAY: Array of channels
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_SCHEDULE_ARRAY: Array of channels to be
 *	scheduled
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_NDL_CHANNEL_ARRAY: Array of NDL channel
 *	information
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_NDL_ACTIVE_STATE_ARRAY: Array of NDL
 *	active state configuration
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_FLAGS: Configuration flags such as
 *	OCB_CONFIG_FLAG_80211_FRAME_MODE
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_DEF_TX_PARAM: Default TX parameters to
 *	use in the case that a packet is sent without a TX control header
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_TA_MAX_DURATION: Max duration after the
 *	last TA received that the local time set by TA is synchronous to other
 *	communicating OCB STAs.
 */
enum qca_wlan_vendor_attr_ocb_set_config {
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_CHANNEL_COUNT = 1,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_SCHEDULE_SIZE = 2,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_CHANNEL_ARRAY = 3,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_SCHEDULE_ARRAY = 4,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_NDL_CHANNEL_ARRAY = 5,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_NDL_ACTIVE_STATE_ARRAY = 6,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_FLAGS = 7,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_DEF_TX_PARAM = 8,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_TA_MAX_DURATION = 9,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_MAX =
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_AFTER_LAST - 1
};

/**
 * enum qca_wlan_vendor_attr_ocb_set_utc_time - Vendor subcmd attributes to set
 *	UTC time
 *
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_VALUE: The UTC time as an array of
 *	10 bytes
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_ERROR: The time error as an array of
 *	5 bytes
 */
enum qca_wlan_vendor_attr_ocb_set_utc_time {
	QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_VALUE = 1,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_ERROR = 2,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_MAX =
	QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_AFTER_LAST - 1
};

/**
 * enum qca_wlan_vendor_attr_ocb_start_timing_advert - Vendor subcmd attributes
 *	to start sending timing advert frames
 *
 * @QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_CHANNEL_FREQ: Cannel frequency
 *	on which to send the frames
 * @QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_REPEAT_RATE: Number of times
 *	the frame is sent in 5 seconds
 */
enum qca_wlan_vendor_attr_ocb_start_timing_advert {
	QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_CHANNEL_FREQ = 1,
	QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_REPEAT_RATE = 2,
	QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_MAX =
	QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_AFTER_LAST - 1
};

/**
 * enum qca_wlan_vendor_attr_ocb_stop_timing_advert - Vendor subcmd attributes
 *	to stop timing advert
 *
 * @QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_CHANNEL_FREQ: The channel
 *	frequency on which to stop the timing advert
 */
enum qca_wlan_vendor_attr_ocb_stop_timing_advert {
	QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_CHANNEL_FREQ = 1,
	QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_MAX =
	QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_AFTER_LAST - 1
};

/**
 * enum qca_wlan_vendor_attr_ocb_get_tsf_response - Vendor subcmd attributes to
 *	get TSF timer value
 *
 * @QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_TIMER_HIGH: Higher 32 bits of the
 *	timer
 * @QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_TIMER_LOW: Lower 32 bits of the timer
 */
enum qca_wlan_vendor_attr_ocb_get_tsf_resp {
	QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_TIMER_HIGH = 1,
	QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_TIMER_LOW = 2,
	QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_MAX =
	QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_AFTER_LAST - 1
};

enum qca_vendor_attr_get_preferred_freq_list {
	QCA_WLAN_VENDOR_ATTR_GET_PREFERRED_FREQ_LIST_INVALID,
	/* A 32-unsigned value; the interface type/mode for which the preferred
	 * frequency list is requested (see enum qca_iface_type for possible
	 * values); used in GET_PREFERRED_FREQ_LIST command from user-space to
	 * kernel and in the kernel response back to user-space.
	 */
	QCA_WLAN_VENDOR_ATTR_GET_PREFERRED_FREQ_LIST_IFACE_TYPE,
	/* An array of 32-unsigned values; values are frequency (MHz); sent
	 * from kernel space to user space.
	 */
	QCA_WLAN_VENDOR_ATTR_GET_PREFERRED_FREQ_LIST,
	/* An array of nested values as per enum qca_wlan_vendor_attr_pcl
	 * attribute. Each element contains frequency (MHz), weight, and flag
	 * bit mask indicating how the frequency should be used in P2P
	 * negotiation; sent from kernel space to user space.
	 */
	QCA_WLAN_VENDOR_ATTR_GET_PREFERRED_FREQ_LIST_WEIGHED_PCL,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_GET_PREFERRED_FREQ_LIST_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_GET_PREFERRED_FREQ_LIST_MAX =
	QCA_WLAN_VENDOR_ATTR_GET_PREFERRED_FREQ_LIST_AFTER_LAST - 1
};

enum qca_vendor_attr_probable_oper_channel {
	QCA_WLAN_VENDOR_ATTR_PROBABLE_OPER_CHANNEL_INVALID,
	/* 32-bit unsigned value; indicates the connection/iface type likely to
	 * come on this channel (see enum qca_iface_type).
	 */
	QCA_WLAN_VENDOR_ATTR_PROBABLE_OPER_CHANNEL_IFACE_TYPE,
	/* 32-bit unsigned value; the frequency (MHz) of the probable channel */
	QCA_WLAN_VENDOR_ATTR_PROBABLE_OPER_CHANNEL_FREQ,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_PROBABLE_OPER_CHANNEL_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_PROBABLE_OPER_CHANNEL_MAX =
	QCA_WLAN_VENDOR_ATTR_PROBABLE_OPER_CHANNEL_AFTER_LAST - 1
};

enum qca_iface_type {
	QCA_IFACE_TYPE_STA,
	QCA_IFACE_TYPE_AP,
	QCA_IFACE_TYPE_P2P_CLIENT,
	QCA_IFACE_TYPE_P2P_GO,
	QCA_IFACE_TYPE_IBSS,
	QCA_IFACE_TYPE_TDLS,
};

enum qca_set_band {
	QCA_SETBAND_AUTO = 0,
	QCA_SETBAND_5G = BIT(0),
	QCA_SETBAND_2G = BIT(1),
	QCA_SETBAND_6G = BIT(2),
};

/**
 * enum qca_access_policy - Access control policy
 *
 * Access control policy is applied on the configured IE
 * (QCA_WLAN_VENDOR_ATTR_CONFIG_ACCESS_POLICY_IE).
 * To be set with QCA_WLAN_VENDOR_ATTR_CONFIG_ACCESS_POLICY.
 *
 * @QCA_ACCESS_POLICY_ACCEPT_UNLESS_LISTED: Deny Wi-Fi connections which match
 *	the specific configuration (IE) set, i.e., allow all the
 *	connections which do not match the configuration.
 * @QCA_ACCESS_POLICY_DENY_UNLESS_LISTED: Accept Wi-Fi connections which match
 *	the specific configuration (IE) set, i.e., deny all the
 *	connections which do not match the configuration.
 */
enum qca_access_policy {
	QCA_ACCESS_POLICY_ACCEPT_UNLESS_LISTED,
	QCA_ACCESS_POLICY_DENY_UNLESS_LISTED,
};

/**
 * enum qca_vendor_attr_tsf_cmd: Vendor attributes for TSF capture
 * @QCA_WLAN_VENDOR_ATTR_TSF_CMD: Required (u32)
 * Specify the TSF command. Possible values are defined in
 * &enum qca_tsf_cmd.
 * @QCA_WLAN_VENDOR_ATTR_TSF_TIMER_VALUE: Optional (u64)
 * This attribute contains TSF timer value. This attribute is only available
 * in %QCA_TSF_GET, %QCA_TSF_SYNC_GET or %QCA_TSF_SYNC_GET_CSA_TIMESTAMP
 * response.
 * @QCA_WLAN_VENDOR_ATTR_TSF_SOC_TIMER_VALUE: Optional (u64)
 * This attribute contains SOC timer value at TSF capture. This attribute is
 * only available in %QCA_TSF_GET or %QCA_TSF_SYNC_GET response.
 * @QCA_WLAN_VENDOR_ATTR_TSF_SYNC_INTERVAL: Optional (u32)
 * This attribute is used to provide TSF sync interval and only applicable when
 * TSF command is %QCA_TSF_SYNC_START. If this attribute is not provided, the
 * driver will use the default value. Time unit is in milliseconds.
 * @QCA_WLAN_VENDOR_ATTR_TSF_PAD: Attribute used for padding for 64-bit
 * alignment.
 */
enum qca_vendor_attr_tsf_cmd {
	QCA_WLAN_VENDOR_ATTR_TSF_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TSF_CMD,
	QCA_WLAN_VENDOR_ATTR_TSF_TIMER_VALUE,
	QCA_WLAN_VENDOR_ATTR_TSF_SOC_TIMER_VALUE,
	QCA_WLAN_VENDOR_ATTR_TSF_SYNC_INTERVAL,
	QCA_WLAN_VENDOR_ATTR_TSF_PAD,
	QCA_WLAN_VENDOR_ATTR_TSF_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TSF_MAX =
	QCA_WLAN_VENDOR_ATTR_TSF_AFTER_LAST - 1
};

/**
 * enum qca_tsf_cmd: TSF driver commands
 * @QCA_TSF_CAPTURE: Initiate TSF Capture
 * @QCA_TSF_GET: Get TSF capture value
 * @QCA_TSF_SYNC_GET: Initiate TSF capture and return with captured value
 * @QCA_TSF_AUTO_REPORT_ENABLE: Used in STA mode only. Once set, the target
 * will automatically send TSF report to the host. To query
 * %QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_UPLINK_DELAY or
 * %QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_UPLINK_DELAY_JITTER, this operation needs
 * to be initiated first.
 * @QCA_TSF_AUTO_REPORT_DISABLE: Used in STA mode only. Once set, the target
 * will not automatically send TSF report to the host. If
 * %QCA_TSF_AUTO_REPORT_ENABLE is initiated and
 * %QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_UPLINK_DELAY or
 * %QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_UPLINK_DELAY_JITTER is not queried
 * anymore, this operation needs to be initiated.
 * @QCA_TSF_SYNC_START: Start periodic TSF sync feature. The driver periodically
 * fetches TSF and host time mapping from the firmware with interval configured
 * through the %QCA_WLAN_VENDOR_ATTR_TSF_SYNC_INTERVAL attribute. If the
 * interval value is not provided the driver will use the default value. The
 * userspace can query the TSF and host time mapping via the %QCA_TSF_GET
 * command.
 * @QCA_TSF_SYNC_STOP: Stop periodic TSF sync feature.
 * @QCA_TSF_SYNC_GET_CSA_TIMESTAMP: Get TSF timestamp when AP will move and
 * starts beaconing on a new channel. The driver synchronously responds with the
 * TSF value using attribute %QCA_WLAN_VENDOR_ATTR_TSF_TIMER_VALUE. Userspace
 * gets the valid CSA TSF after receiving %NL80211_CMD_CH_SWITCH_STARTED_NOTIFY
 * on the AP interface. This TSF can be sent via OOB mechanism to connected
 * clients.
 */
enum qca_tsf_cmd {
	QCA_TSF_CAPTURE,
	QCA_TSF_GET,
	QCA_TSF_SYNC_GET,
	QCA_TSF_AUTO_REPORT_ENABLE,
	QCA_TSF_AUTO_REPORT_DISABLE,
	QCA_TSF_SYNC_START,
	QCA_TSF_SYNC_STOP,
	QCA_TSF_SYNC_GET_CSA_TIMESTAMP,
};

/**
 * enum qca_vendor_attr_wisa_cmd
 * @QCA_WLAN_VENDOR_ATTR_WISA_MODE: WISA mode value (u32)
 * WISA setup vendor commands
 */
enum qca_vendor_attr_wisa_cmd {
	QCA_WLAN_VENDOR_ATTR_WISA_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_WISA_MODE,
	QCA_WLAN_VENDOR_ATTR_WISA_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_WISA_MAX =
	QCA_WLAN_VENDOR_ATTR_WISA_AFTER_LAST - 1
};

/* IEEE 802.11 Vendor Specific elements */

/**
 * enum qca_vendor_element_id - QCA Vendor Specific element types
 *
 * These values are used to identify QCA Vendor Specific elements. The
 * payload of the element starts with the three octet OUI (OUI_QCA) and
 * is followed by a single octet type which is defined by this enum.
 *
 * @QCA_VENDOR_ELEM_P2P_PREF_CHAN_LIST: P2P preferred channel list.
 *	This element can be used to specify preference order for supported
 *	channels. The channels in this list are in preference order (the first
 *	one has the highest preference) and are described as a pair of
 *	(global) Operating Class and Channel Number (each one octet) fields.
 *
 *	This extends the standard P2P functionality by providing option to have
 *	more than one preferred operating channel. When this element is present,
 *	it replaces the preference indicated in the Operating Channel attribute.
 *	For supporting other implementations, the Operating Channel attribute is
 *	expected to be used with the highest preference channel. Similarly, all
 *	the channels included in this Preferred channel list element are
 *	expected to be included in the Channel List attribute.
 *
 *	This vendor element may be included in GO Negotiation Request, P2P
 *	Invitation Request, and Provision Discovery Request frames.
 *
 * @QCA_VENDOR_ELEM_HE_CAPAB: HE Capabilities element.
 *	This element can be used for pre-standard publication testing of HE
 *	before P802.11ax draft assigns the element ID. The payload of this
 *	vendor specific element is defined by the latest P802.11ax draft.
 *	Please note that the draft is still work in progress and this element
 *	payload is subject to change.
 *
 * @QCA_VENDOR_ELEM_HE_OPER: HE Operation element.
 *	This element can be used for pre-standard publication testing of HE
 *	before P802.11ax draft assigns the element ID. The payload of this
 *	vendor specific element is defined by the latest P802.11ax draft.
 *	Please note that the draft is still work in progress and this element
 *	payload is subject to change.
 *
 * @QCA_VENDOR_ELEM_RAPS: RAPS element (OFDMA-based Random Access Parameter Set
 *	element).
 *	This element can be used for pre-standard publication testing of HE
 *	before P802.11ax draft assigns the element ID extension. The payload of
 *	this vendor specific element is defined by the latest P802.11ax draft
 *	(not including the Element ID Extension field). Please note that the
 *	draft is still work in progress and this element payload is subject to
 *	change.
 *
 * @QCA_VENDOR_ELEM_MU_EDCA_PARAMS: MU EDCA Parameter Set element.
 *	This element can be used for pre-standard publication testing of HE
 *	before P802.11ax draft assigns the element ID extension. The payload of
 *	this vendor specific element is defined by the latest P802.11ax draft
 *	(not including the Element ID Extension field). Please note that the
 *	draft is still work in progress and this element payload is subject to
 *	change.
 *
 * @QCA_VENDOR_ELEM_BSS_COLOR_CHANGE: BSS Color Change Announcement element.
 *	This element can be used for pre-standard publication testing of HE
 *	before P802.11ax draft assigns the element ID extension. The payload of
 *	this vendor specific element is defined by the latest P802.11ax draft
 *	(not including the Element ID Extension field). Please note that the
 *	draft is still work in progress and this element payload is subject to
 *	change.
 *
 *  @QCA_VENDOR_ELEM_ALLPLAY: Allplay element
 */
enum qca_vendor_element_id {
	QCA_VENDOR_ELEM_P2P_PREF_CHAN_LIST = 0,
	QCA_VENDOR_ELEM_HE_CAPAB = 1,
	QCA_VENDOR_ELEM_HE_OPER = 2,
	QCA_VENDOR_ELEM_RAPS = 3,
	QCA_VENDOR_ELEM_MU_EDCA_PARAMS = 4,
	QCA_VENDOR_ELEM_BSS_COLOR_CHANGE = 5,
	QCA_VENDOR_ELEM_ALLPLAY = 6,
};

/**
 * enum qca_wlan_vendor_scan_priority - Specifies the valid values that the
 * vendor scan attribute QCA_WLAN_VENDOR_ATTR_SCAN_PRIORITY can take.
 * @QCA_WLAN_VENDOR_SCAN_PRIORITY_VERY_LOW: Very low priority
 * @QCA_WLAN_VENDOR_SCAN_PRIORITY_LOW: Low priority
 * @QCA_WLAN_VENDOR_SCAN_PRIORITY_MEDIUM: Medium priority
 * @QCA_WLAN_VENDOR_SCAN_PRIORITY_HIGH: High priority
 * @QCA_WLAN_VENDOR_SCAN_PRIORITY_VERY_HIGH: Very high priority
 */
enum qca_wlan_vendor_scan_priority {
	QCA_WLAN_VENDOR_SCAN_PRIORITY_VERY_LOW = 0,
	QCA_WLAN_VENDOR_SCAN_PRIORITY_LOW = 1,
	QCA_WLAN_VENDOR_SCAN_PRIORITY_MEDIUM = 2,
	QCA_WLAN_VENDOR_SCAN_PRIORITY_HIGH = 3,
	QCA_WLAN_VENDOR_SCAN_PRIORITY_VERY_HIGH = 4,
};

/**
 * enum qca_wlan_vendor_attr_scan - Specifies vendor scan attributes
 *
 * @QCA_WLAN_VENDOR_ATTR_SCAN_IE: IEs that should be included as part of scan
 * @QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES: Nested unsigned 32-bit attributes
 *	with frequencies to be scanned (in MHz)
 * @QCA_WLAN_VENDOR_ATTR_SCAN_SSIDS: Nested attribute with SSIDs to be scanned
 * @QCA_WLAN_VENDOR_ATTR_SCAN_SUPP_RATES: Nested array attribute of supported
 *	rates to be included
 * @QCA_WLAN_VENDOR_ATTR_SCAN_TX_NO_CCK_RATE: flag used to send probe requests
 *	at non CCK rate in 2GHz band
 * @QCA_WLAN_VENDOR_ATTR_SCAN_FLAGS: Unsigned 32-bit scan flags
 * @QCA_WLAN_VENDOR_ATTR_SCAN_COOKIE: Unsigned 64-bit cookie provided by the
 *	driver for the specific scan request
 * @QCA_WLAN_VENDOR_ATTR_SCAN_STATUS: Unsigned 8-bit status of the scan
 *	request decoded as in enum scan_status
 * @QCA_WLAN_VENDOR_ATTR_SCAN_MAC: 6-byte MAC address to use when randomisation
 *	scan flag is set
 * @QCA_WLAN_VENDOR_ATTR_SCAN_MAC_MASK: 6-byte MAC address mask to be used with
 *	randomisation
 * @QCA_WLAN_VENDOR_ATTR_SCAN_BSSID: 6-byte MAC address representing the
 *	specific BSSID to scan for.
 * @QCA_WLAN_VENDOR_ATTR_SCAN_DWELL_TIME: Unsigned 64-bit dwell time in
 *	microseconds. This is a common value which applies across all
 *	frequencies specified by QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES.
 * @QCA_WLAN_VENDOR_ATTR_SCAN_PRIORITY: Priority of vendor scan relative to
 *	other scan requests. It is a u32 attribute and takes values from enum
 *	qca_wlan_vendor_scan_priority. This is an optional attribute.
 *	If this attribute is not configured, the driver shall use
 *	QCA_WLAN_VENDOR_SCAN_PRIORITY_HIGH as the priority of vendor scan.
 * @QCA_WLAN_VENDOR_ATTR_SCAN_PAD: Attribute used for padding for 64-bit
 *	alignment.
 * @QCA_WLAN_VENDOR_ATTR_SCAN_LINK_ID: This u8 attribute is used for OBSS scan
 *	when AP is operating as MLD to specify which link is requesting the
 *	scan or which link the scan result is for. No need of this attribute
 *	in other cases.
 */
enum qca_wlan_vendor_attr_scan {
	QCA_WLAN_VENDOR_ATTR_SCAN_INVALID_PARAM = 0,
	QCA_WLAN_VENDOR_ATTR_SCAN_IE = 1,
	QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES = 2,
	QCA_WLAN_VENDOR_ATTR_SCAN_SSIDS = 3,
	QCA_WLAN_VENDOR_ATTR_SCAN_SUPP_RATES = 4,
	QCA_WLAN_VENDOR_ATTR_SCAN_TX_NO_CCK_RATE = 5,
	QCA_WLAN_VENDOR_ATTR_SCAN_FLAGS = 6,
	QCA_WLAN_VENDOR_ATTR_SCAN_COOKIE = 7,
	QCA_WLAN_VENDOR_ATTR_SCAN_STATUS = 8,
	QCA_WLAN_VENDOR_ATTR_SCAN_MAC = 9,
	QCA_WLAN_VENDOR_ATTR_SCAN_MAC_MASK = 10,
	QCA_WLAN_VENDOR_ATTR_SCAN_BSSID = 11,
	QCA_WLAN_VENDOR_ATTR_SCAN_DWELL_TIME = 12,
	QCA_WLAN_VENDOR_ATTR_SCAN_PRIORITY = 13,
	QCA_WLAN_VENDOR_ATTR_SCAN_PAD = 14,
	QCA_WLAN_VENDOR_ATTR_SCAN_LINK_ID = 15,
	QCA_WLAN_VENDOR_ATTR_SCAN_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SCAN_MAX =
	QCA_WLAN_VENDOR_ATTR_SCAN_AFTER_LAST - 1
};

/**
 * enum scan_status - Specifies the valid values the vendor scan attribute
 *	QCA_WLAN_VENDOR_ATTR_SCAN_STATUS can take
 *
 * @VENDOR_SCAN_STATUS_NEW_RESULTS: implies the vendor scan is successful with
 *	new scan results
 * @VENDOR_SCAN_STATUS_ABORTED: implies the vendor scan was aborted in-between
 */
enum scan_status {
	VENDOR_SCAN_STATUS_NEW_RESULTS,
	VENDOR_SCAN_STATUS_ABORTED,
	VENDOR_SCAN_STATUS_MAX,
};

/**
 * enum qca_vendor_attr_ota_test - Specifies the values for vendor
 *                       command QCA_NL80211_VENDOR_SUBCMD_OTA_TEST
 * @QCA_WLAN_VENDOR_ATTR_OTA_TEST_ENABLE: enable ota test
 */
enum qca_vendor_attr_ota_test {
	QCA_WLAN_VENDOR_ATTR_OTA_TEST_INVALID,
	/* 8-bit unsigned value to indicate if OTA test is enabled */
	QCA_WLAN_VENDOR_ATTR_OTA_TEST_ENABLE,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_OTA_TEST_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_OTA_TEST_MAX =
	QCA_WLAN_VENDOR_ATTR_OTA_TEST_AFTER_LAST - 1
};

/**
 * enum qca_vendor_attr_txpower_scale - vendor sub commands index
 *
 * @QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE: scaling value
 */
enum qca_vendor_attr_txpower_scale {
	QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_INVALID,
	/* 8-bit unsigned value to indicate the scaling of tx power */
	QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_MAX =
	QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_AFTER_LAST - 1
};

/**
 * enum qca_vendor_attr_txpower_decr_db - Attributes for TX power decrease
 *
 * These attributes are used with QCA_NL80211_VENDOR_SUBCMD_SET_TXPOWER_DECR_DB.
 */
enum qca_vendor_attr_txpower_decr_db {
	QCA_WLAN_VENDOR_ATTR_TXPOWER_DECR_DB_INVALID,
	/* 8-bit unsigned value to indicate the reduction of TX power in dB for
	 * a virtual interface.
	 */
	QCA_WLAN_VENDOR_ATTR_TXPOWER_DECR_DB,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TXPOWER_DECR_DB_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TXPOWER_DECR_DB_MAX =
	QCA_WLAN_VENDOR_ATTR_TXPOWER_DECR_DB_AFTER_LAST - 1
};

/* Attributes for data used by
 * QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION and
 * QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION subcommands.
 */
enum qca_wlan_vendor_attr_config {
	QCA_WLAN_VENDOR_ATTR_CONFIG_INVALID = 0,
	/* Unsigned 32-bit value to set the DTIM period.
	 * Whether the wifi chipset wakes at every dtim beacon or a multiple of
	 * the DTIM period. If DTIM is set to 3, the STA shall wake up every 3
	 * DTIM beacons.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_DYNAMIC_DTIM = 1,
	/* Unsigned 32-bit value to set the wifi_iface stats averaging factor
	 * used to calculate statistics like average the TSF offset or average
	 * number of frame leaked.
	 * For instance, upon Beacon frame reception:
	 * current_avg = ((beacon_TSF - TBTT) * factor +
	 *                previous_avg * (0x10000 - factor)) / 0x10000
	 * For instance, when evaluating leaky APs:
	 * current_avg = ((num frame received within guard time) * factor +
	 *                previous_avg * (0x10000 - factor)) / 0x10000
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_STATS_AVG_FACTOR = 2,
	/* Unsigned 32-bit value to configure guard time, i.e., when
	 * implementing IEEE power management based on frame control PM bit, how
	 * long the driver waits before shutting down the radio and after
	 * receiving an ACK frame for a Data frame with PM bit set.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_GUARD_TIME = 3,
	/* Unsigned 32-bit value to change the FTM capability dynamically */
	QCA_WLAN_VENDOR_ATTR_CONFIG_FINE_TIME_MEASUREMENT = 4,
	/* Unsigned 16-bit value to configure maximum TX rate dynamically */
	QCA_WLAN_VENDOR_ATTR_CONF_TX_RATE = 5,
	/* Unsigned 32-bit value to configure the number of continuous
	 * Beacon Miss which shall be used by the firmware to penalize
	 * the RSSI.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_PENALIZE_AFTER_NCONS_BEACON_MISS = 6,
	/* Unsigned 8-bit value to configure the channel avoidance indication
	 * behavior. Firmware to send only one indication and ignore duplicate
	 * indications when set to avoid multiple Apps wakeups.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_CHANNEL_AVOIDANCE_IND = 7,
	/* 8-bit unsigned value to configure the maximum TX MPDU for
	 * aggregation.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_TX_MPDU_AGGREGATION = 8,
	/* 8-bit unsigned value to configure the maximum RX MPDU for
	 * aggregation.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_RX_MPDU_AGGREGATION = 9,
	/* 8-bit unsigned value to configure the Non aggregrate/11g sw
	 * retry threshold (0 disable, 31 max).
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_NON_AGG_RETRY = 10,
	/* 8-bit unsigned value to configure the aggregrate sw
	 * retry threshold (0 disable, 31 max).
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_AGG_RETRY = 11,
	/* 8-bit unsigned value to configure the MGMT frame
	 * retry threshold (0 disable, 31 max).
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_MGMT_RETRY = 12,
	/* 8-bit unsigned value to configure the CTRL frame
	 * retry threshold (0 disable, 31 max).
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_CTRL_RETRY = 13,
	/* 8-bit unsigned value to configure the propagation delay for
	 * 2G/5G band (0~63, units in us)
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_PROPAGATION_DELAY = 14,
	/* Unsigned 32-bit value to configure the number of unicast TX fail
	 * packet count. The peer is disconnected once this threshold is
	 * reached.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_TX_FAIL_COUNT = 15,
	/* Attribute used to set scan default IEs to the driver.
	 *
	 * These IEs can be used by scan operations that will be initiated by
	 * the driver/firmware.
	 *
	 * For further scan requests coming to the driver, these IEs should be
	 * merged with the IEs received along with scan request coming to the
	 * driver. If a particular IE is present in the scan default IEs but not
	 * present in the scan request, then that IE should be added to the IEs
	 * sent in the Probe Request frames for that scan request.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_SCAN_DEFAULT_IES = 16,
	/* Unsigned 32-bit attribute for generic commands */
	QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND = 17,
	/* Unsigned 32-bit value attribute for generic commands */
	QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE = 18,
	/* Unsigned 32-bit data attribute for generic command response */
	QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA = 19,
	/* Unsigned 32-bit length attribute for
	 * QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_LENGTH = 20,
	/* Unsigned 32-bit flags attribute for
	 * QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_FLAGS = 21,
	/* Unsigned 32-bit, defining the access policy.
	 * See enum qca_access_policy. Used with
	 * QCA_WLAN_VENDOR_ATTR_CONFIG_ACCESS_POLICY_IE_LIST.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ACCESS_POLICY = 22,
	/* Sets the list of full set of IEs for which a specific access policy
	 * has to be applied. Used along with
	 * QCA_WLAN_VENDOR_ATTR_CONFIG_ACCESS_POLICY to control the access.
	 * Zero length payload can be used to clear this access constraint.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ACCESS_POLICY_IE_LIST = 23,
	/* Unsigned 32-bit, specifies the interface index (netdev) for which the
	 * corresponding configurations are applied. If the interface index is
	 * not specified, the configurations are attributed to the respective
	 * wiphy.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_IFINDEX = 24,
	/* 8-bit unsigned value to trigger QPower: 1-Enable, 0-Disable */
	QCA_WLAN_VENDOR_ATTR_CONFIG_QPOWER = 25,
	/* 8-bit unsigned value to configure the driver and below layers to
	 * ignore the assoc disallowed set by APs while connecting
	 * 1-Ignore, 0-Don't ignore
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_IGNORE_ASSOC_DISALLOWED = 26,
	/* 32-bit unsigned value to trigger antenna diversity features:
	 * 1-Enable, 0-Disable
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ANT_DIV_ENA = 27,
	/* 32-bit unsigned value to configure specific chain antenna */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ANT_DIV_CHAIN = 28,
	/* 32-bit unsigned value to trigger cycle selftest
	 * 1-Enable, 0-Disable
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ANT_DIV_SELFTEST = 29,
	/* 32-bit unsigned to configure the cycle time of selftest
	 * the unit is micro-second
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ANT_DIV_SELFTEST_INTVL = 30,
	/* 32-bit unsigned value to set reorder timeout for AC_VO */
	QCA_WLAN_VENDOR_ATTR_CONFIG_RX_REORDER_TIMEOUT_VOICE = 31,
	/* 32-bit unsigned value to set reorder timeout for AC_VI */
	QCA_WLAN_VENDOR_ATTR_CONFIG_RX_REORDER_TIMEOUT_VIDEO = 32,
	/* 32-bit unsigned value to set reorder timeout for AC_BE */
	QCA_WLAN_VENDOR_ATTR_CONFIG_RX_REORDER_TIMEOUT_BESTEFFORT = 33,
	/* 32-bit unsigned value to set reorder timeout for AC_BK */
	QCA_WLAN_VENDOR_ATTR_CONFIG_RX_REORDER_TIMEOUT_BACKGROUND = 34,
	/* 6-byte MAC address to point out the specific peer */
	QCA_WLAN_VENDOR_ATTR_CONFIG_PEER_MAC = 35,
	/* Backward compatibility with the original name */
	QCA_WLAN_VENDOR_ATTR_CONFIG_RX_BLOCKSIZE_PEER_MAC =
	QCA_WLAN_VENDOR_ATTR_CONFIG_PEER_MAC,
	/* 32-bit unsigned value to set window size for specific peer */
	QCA_WLAN_VENDOR_ATTR_CONFIG_RX_BLOCKSIZE_WINLIMIT = 36,
	/* 8-bit unsigned value to set the beacon miss threshold in 2.4 GHz */
	QCA_WLAN_VENDOR_ATTR_CONFIG_BEACON_MISS_THRESHOLD_24 = 37,
	/* 8-bit unsigned value to set the beacon miss threshold in 5 GHz */
	QCA_WLAN_VENDOR_ATTR_CONFIG_BEACON_MISS_THRESHOLD_5 = 38,
	/* 32-bit unsigned value to configure 5 or 10 MHz channel width for
	 * station device while in disconnect state. The attribute use the
	 * value of enum nl80211_chan_width: NL80211_CHAN_WIDTH_5 means 5 MHz,
	 * NL80211_CHAN_WIDTH_10 means 10 MHz. If set, the device work in 5 or
	 * 10 MHz channel width, the station will not connect to a BSS using 20
	 * MHz or higher bandwidth. Set to NL80211_CHAN_WIDTH_20_NOHT to
	 * clear this constraint.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_SUB20_CHAN_WIDTH = 39,
	/* 32-bit unsigned value to configure the propagation absolute delay
	 * for 2G/5G band (units in us)
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_PROPAGATION_ABS_DELAY = 40,
	/* 32-bit unsigned value to set probe period */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ANT_DIV_PROBE_PERIOD = 41,
	/* 32-bit unsigned value to set stay period */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ANT_DIV_STAY_PERIOD = 42,
	/* 32-bit unsigned value to set snr diff */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ANT_DIV_SNR_DIFF = 43,
	/* 32-bit unsigned value to set probe dwell time */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ANT_DIV_PROBE_DWELL_TIME = 44,
	/* 32-bit unsigned value to set mgmt snr weight */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ANT_DIV_MGMT_SNR_WEIGHT = 45,
	/* 32-bit unsigned value to set data snr weight */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ANT_DIV_DATA_SNR_WEIGHT = 46,
	/* 32-bit unsigned value to set ack snr weight */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ANT_DIV_ACK_SNR_WEIGHT = 47,
	/* 32-bit unsigned value to configure the listen interval.
	 * This is in units of beacon intervals. This configuration alters
	 * the negotiated listen interval with the AP during the connection.
	 * It is highly recommended to configure a value less than or equal to
	 * the one negotiated during the association. Configuring any greater
	 * value can have adverse effects (frame loss, AP disassociating STA,
	 * etc.).
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_LISTEN_INTERVAL = 48,
	/*
	 * 8 bit unsigned value that is set on an AP/GO virtual interface to
	 * disable operations that would cause the AP/GO to leave its operating
	 * channel.
	 *
	 * This will restrict the scans to the AP/GO operating channel and the
	 * channels of the other band, if DBS is supported.A STA/CLI interface
	 * brought up after this setting is enabled, will be restricted to
	 * connecting to devices only on the AP/GO interface's operating channel
	 * or on the other band in DBS case. P2P supported channel list is
	 * modified, to only include AP interface's operating-channel and the
	 * channels of the other band if DBS is supported.
	 *
	 * These restrictions are only applicable as long as the AP/GO interface
	 * is alive. If the AP/GO interface is brought down then this
	 * setting/restriction is forgotten.
	 *
	 * If this variable is set on an AP/GO interface while a multi-channel
	 * concurrent session is active, it has no effect on the operation of
	 * the current interfaces, other than restricting the scan to the AP/GO
	 * operating channel and the other band channels if DBS is supported.
	 * However, if the STA is brought down and restarted then the new STA
	 * connection will either be formed on the AP/GO channel or on the
	 * other band in a DBS case. This is because of the scan being
	 * restricted on these channels as mentioned above.
	 *
	 * 1-Restrict / 0-Don't restrict offchannel operations.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_RESTRICT_OFFCHANNEL = 49,
	/*
	 * 8 bit unsigned value to enable/disable LRO (Large Receive Offload)
	 * on an interface.
	 * 1 - Enable, 0 - Disable.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_LRO = 50,

	/*
	 * 8 bit unsigned value to globally enable/disable scan
	 * 1 - Enable, 0 - Disable.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_SCAN_ENABLE = 51,

	/* 8-bit unsigned value to set the total beacon miss count
	 * This parameter will set the total beacon miss count.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_TOTAL_BEACON_MISS_COUNT = 52,

	/* Unsigned 32-bit value to configure the number of continuous
	 * Beacon Miss which shall be used by the firmware to penalize
	 * the RSSI for BTC.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_PENALIZE_AFTER_NCONS_BEACON_MISS_BTC = 53,

	/* 8-bit unsigned value to configure the driver and below layers to
	 * enable/disable all FILS features.
	 * 0-enable, 1-disable
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_DISABLE_FILS = 54,

	/* 16-bit unsigned value to configure the level of WLAN latency
	 * module. See enum qca_wlan_vendor_attr_config_latency_level.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL = 55,

	/* 8-bit unsigned value indicating the driver to use the RSNE as-is from
	 * the connect interface. Exclusively used for the scenarios where the
	 * device is used as a test bed device with special functionality and
	 * not recommended for production. This helps driver to not validate the
	 * RSNE passed from user space and thus allow arbitrary IE data to be
	 * used for testing purposes.
	 * 1-enable, 0-disable.
	 * Applications set/reset this configuration. If not reset, this
	 * parameter remains in use until the driver is unloaded.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_RSN_IE = 56,

	/* 8-bit unsigned value to trigger green Tx power saving.
	 * 1-Enable, 0-Disable
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_GTX = 57,

	/* Attribute to configure disconnect IEs to the driver.
	 * This carries an array of unsigned 8-bit characters.
	 *
	 * If this is configured, driver shall fill the IEs in disassoc/deauth
	 * frame.
	 * These IEs are expected to be considered only for the next
	 * immediate disconnection (disassoc/deauth frame) originated by
	 * the DUT, irrespective of the entity (user space/driver/firmware)
	 * triggering the disconnection.
	 * The host drivers are not expected to use the IEs set through
	 * this interface for further disconnections after the first immediate
	 * disconnection initiated post the configuration.
	 * If the IEs are also updated through cfg80211 interface (after the
	 * enhancement to cfg80211_disconnect), host driver is expected to
	 * take the union of IEs from both of these interfaces and send in
	 * further disassoc/deauth frames.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_DISCONNECT_IES = 58,

	/* 8-bit unsigned value for ELNA bypass.
	 * 0 - Disable eLNA bypass.
	 * 1 - Enable eLNA bypass.
	 * 2 - Reset eLNA bypass configuration, the driver should
	 *	revert to the default configuration of eLNA bypass.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ELNA_BYPASS = 59,

	/* 8-bit unsigned value. This attribute enables/disables the host driver
	 * to send the Beacon Report Response with failure reason for the
	 * scenarios where STA cannot honor the Beacon Report Request from AP.
	 * 1-Enable, 0-Disable.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_BEACON_REPORT_FAIL = 60,

	/* 8-bit unsigned value. This attribute enables/disables the host driver
	 * to send roam reason information in the Reassociation Request frame to
	 * the target AP when roaming within the same ESS.
	 * 1-Enable, 0-Disable.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ROAM_REASON = 61,

	/* 32-bit unsigned value to configure different PHY modes to the
	 * driver/firmware. The possible values are defined in
	 * enum qca_wlan_vendor_phy_mode. The configuration will be reset to
	 * default value, i.e., QCA_WLAN_VENDOR_PHY_MODE_AUTO upon restarting
	 * the driver.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_PHY_MODE = 62,

	/* 8-bit unsigned value to configure the maximum supported channel width
	 * for STA mode. If this value is configured when STA is in connected
	 * state, it should not exceed the negotiated channel width. If it is
	 * configured when STA is in disconnected state, the configured value
	 * will take effect for the next immediate connection.
	 * This configuration can be sent inside
	 * %QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINKS to specify the maximum
	 * supported channel width per-MLO link.
	 *
	 * This uses values defined in enum nl80211_chan_width.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_CHANNEL_WIDTH = 63,

	/* 8-bit unsigned value to enable/disable dynamic bandwidth adjustment.
	 * This attribute is only applicable for STA mode. When dynamic
	 * bandwidth adjustment is disabled, STA will use static channel width
	 * the value of which is negotiated during connection.
	 * 1-enable (default), 0-disable
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_DYNAMIC_BW = 64,

	/* 8-bit unsigned value to configure the maximum number of subframes of
	 * TX MSDU for aggregation. Possible values are 0-31. When set to 0,
	 * it is decided by the hardware.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_TX_MSDU_AGGREGATION = 65,

	/* 8-bit unsigned value to configure the maximum number of subframes of
	 * RX MSDU for aggregation. Possible values are 0-31. When set to 0,
	 * it is decided by the hardware.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_RX_MSDU_AGGREGATION = 66,

	/* 8-bit unsigned value. This attribute is used to dynamically
	 * enable/disable the LDPC capability of the device. When configured in
	 * the disconnected state, the updated configuration will be considered
	 * for the immediately following connection attempt. If this
	 * configuration is modified while the device is in the connected state,
	 * the LDPC TX will be updated with this configuration immediately,
	 * while the LDPC RX configuration update will take place starting from
	 * the subsequent association attempt.
	 * 1-Enable, 0-Disable.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_LDPC = 67,

	/* 8-bit unsigned value. This attribute is used to dynamically
	 * enable/disable the TX STBC capability of the device. When configured
	 * in the disconnected state, the updated configuration will be
	 * considered for the immediately following connection attempt. If the
	 * connection is formed with TX STBC enabled and if this configuration
	 * is disabled during that association, the TX will be impacted
	 * immediately. Further connection attempts will disable TX STBC.
	 * However, enabling the TX STBC for a connected session with disabled
	 * capability is not allowed and will fail.
	 * 1-Enable, 0-Disable.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_TX_STBC = 68,

	/* 8-bit unsigned value. This attribute is used to dynamically
	 * enable/disable the RX STBC capability of the device. When configured
	 * in the disconnected state, the updated configuration will be
	 * considered for the immediately following connection attempt. If the
	 * configuration is modified in the connected state, there will be no
	 * impact for the current association, but further connection attempts
	 * will use the updated configuration.
	 * 1-Enable, 0-Disable.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_RX_STBC = 69,

	/* 8-bit unsigned value. Refer to TX/RX NSS and chain configurations */
	QCA_WLAN_VENDOR_ATTR_CONFIG_NSS = 70,

	/* 8-bit unsigned value to configure Optimized Power Management mode:
	 * Modes are defined by enum qca_wlan_vendor_opm_mode.
	 *
	 * This attribute shall be configured along with
	 * %QCA_WLAN_VENDOR_ATTR_CONFIG_OPM_ITO and
	 * %QCA_WLAN_VENDOR_ATTR_CONFIG_OPM_SPEC_WAKE_INTERVAL attributes
	 * when its value is set to %QCA_WLAN_VENDOR_OPM_MODE_USER_DEFINED.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_OPTIMIZED_POWER_MANAGEMENT = 71,

	/* 8-bit unsigned value. This attribute takes the QoS/access category
	 * value represented by the enum qca_wlan_ac_type and expects the driver
	 * to upgrade the UDP frames to this access category. The value of
	 * QCA_WLAN_AC_ALL is invalid for this attribute. This will override the
	 * DSCP value configured in the frame with the intention to only upgrade
	 * the access category. That said, it is not intended to downgrade the
	 * access category for the frames.
	 * Set the value to QCA_WLAN_AC_BK if the QoS upgrade needs to be
	 * disabled, as BK is of the lowest priority and an upgrade to it does
	 * not result in any changes for the frames.
	 *
	 * If only UDP frames of BE or BK access category needs to be upgraded
	 * without changing the access category of VO or VI UDP frames, refer to
	 * attribute QCA_WLAN_VENDOR_ATTR_CONFIG_UDP_QOS_UPGRADE_FOR_BE_BK.
	 *
	 * This attribute is not recommended to be used as it blindly forces all
	 * UDP packets to a higher access category which could impact the
	 * traffic pattern of all apps using UDP and can cause unknown behavior.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_UDP_QOS_UPGRADE = 72,

	/* 8-bit unsigned value. Refer to TX/RX NSS and chain configurations */
	QCA_WLAN_VENDOR_ATTR_CONFIG_NUM_TX_CHAINS = 73,

	/* 8-bit unsigned value. Refer to TX/RX NSS and chain configurations */
	QCA_WLAN_VENDOR_ATTR_CONFIG_NUM_RX_CHAINS = 74,

	/* 8-bit unsigned value to configure ANI setting type.
	 * See &enum qca_wlan_ani_setting for possible values.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ANI_SETTING = 75,
	/* 32-bit signed value to configure ANI level. This is used when
	 * ANI settings type is &QCA_WLAN_ANI_SETTING_FIXED.
	 * The set and get of ANI level with &QCA_WLAN_ANI_SETTING_AUTO
	 * is invalid, the driver will return a failure.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ANI_LEVEL = 76,

	/* 8-bit unsigned value. This attribute is used to dynamically configure
	 * the number of spatial streams used for transmitting the data. When
	 * configured in the disconnected state, the configured value will
	 * be considered for the following connection attempt.
	 * If the NSS is updated after the connection, the updated NSS value
	 * is notified to the peer using the Operating Mode Notification/Spatial
	 * Multiplexing Power Save frame.
	 * The TX NSS value configured after the connection shall not be greater
	 * than the value negotiated during the connection. Any such higher
	 * value configuration shall be treated as invalid configuration by
	 * the driver. This attribute shall be configured along with
	 * QCA_WLAN_VENDOR_ATTR_CONFIG_RX_NSS attribute to define the symmetric
	 * configuration (such as 2X2 or 1X1) or the asymmetric
	 * configuration (such as 1X2).
	 * If QCA_WLAN_VENDOR_ATTR_CONFIG_NSS attribute is also provided along
	 * with this QCA_WLAN_VENDOR_ATTR_CONFIG_TX_NSS attribute the driver
	 * will update the TX NSS based on QCA_WLAN_VENDOR_ATTR_CONFIG_TX_NSS.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_TX_NSS = 77,

	/* 8-bit unsigned value. This attribute is used to dynamically configure
	 * the number of spatial streams used for receiving the data. When
	 * configured in the disconnected state, the configured value will
	 * be considered for the following connection attempt.
	 * If the NSS is updated after the connection, the updated NSS value
	 * is notified to the peer using the Operating Mode Notification/Spatial
	 * Multiplexing Power Save frame.
	 * The RX NSS value configured after the connection shall not be greater
	 * than the value negotiated during the connection. Any such higher
	 * value configuration shall be treated as invalid configuration by
	 * the driver. This attribute shall be configured along with
	 * QCA_WLAN_VENDOR_ATTR_CONFIG_TX_NSS attribute to define the symmetric
	 * configuration (such as 2X2 or 1X1) or the asymmetric
	 * configuration (such as 1X2).
	 * If QCA_WLAN_VENDOR_ATTR_CONFIG_NSS attribute is also provided along
	 * with this QCA_WLAN_VENDOR_ATTR_CONFIG_RX_NSS attribute the driver
	 * will update the RX NSS based on QCA_WLAN_VENDOR_ATTR_CONFIG_RX_NSS.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_RX_NSS = 78,

	/*
	 * 8-bit unsigned value. This attribute, when set, indicates whether the
	 * specified interface is the primary STA interface when there are more
	 * than one STA interfaces concurrently active.
	 *
	 * This configuration helps the firmware/hardware to support certain
	 * features (e.g., roaming) on this primary interface, if the same
	 * cannot be supported on the concurrent STA interfaces simultaneously.
	 *
	 * This configuration is only applicable for a single STA interface on
	 * a device and gives the priority for it only over other concurrent STA
	 * interfaces.
	 *
	 * If the device is a multi wiphy/soc, this configuration applies to a
	 * single STA interface across the wiphys.
	 *
	 * 1-Enable (is the primary STA), 0-Disable (is not the primary STA)
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_CONCURRENT_STA_PRIMARY = 79,

	/*
	 * 8-bit unsigned value. This attribute can be used to configure the
	 * driver to enable/disable FT-over-DS feature. Possible values for
	 * this attribute are 1-Enable and 0-Disable.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_FT_OVER_DS = 80,

	/*
	 * 8-bit unsigned value. This attribute can be used to configure the
	 * firmware to enable/disable ARP/NS offload feature. Possible values
	 * for this attribute are 0-Disable and 1-Enable.
	 *
	 * This attribute is only applicable for STA/P2P-Client interface,
	 * and is optional, default behavior is ARP/NS offload enabled.
	 *
	 * This attribute can be set in disconnected and connected state, and
	 * will restore to the default behavior if the interface is closed.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ARP_NS_OFFLOAD = 81,

	/*
	 * 8-bit unsigned value. This attribute can be used to configure the
	 * data path mode to be followed for audio traffic. Possible values
	 * are defined in enum qca_wlan_audio_data_path.
	 *
	 * This attribute is used only when the driver advertises support for
	 * QCA_WLAN_VENDOR_FEATURE_ENHANCED_AUDIO_EXPERIENCE_OVER_WLAN.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_AUDIO_DATA_PATH = 82,

	/*
	 * 8-bit unsigned value. This attribute can be used to configure the
	 * Dedicated Bluetooth Antenna Mode (DBAM) feature. Possible values for
	 * this attribute are defined in the enum qca_wlan_dbam_config.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_DBAM = 83,

	/* 8-bit unsigned value. This attribute takes the QoS/access category
	 * value represented by the enum qca_wlan_ac_type and expects the driver
	 * to upgrade the UDP frames of BE or BK access category to this access
	 * category. This attribute will not modify UDP frames of VO or VI
	 * access category. The value of QCA_WLAN_AC_ALL is invalid for this
	 * attribute.
	 *
	 * This will override the DSCP value configured in the frame with the
	 * intention to only upgrade the access category. That said, it is not
	 * intended to downgrade the access category for the frames.
	 * Set the value to QCA_WLAN_AC_BK if the QoS upgrade needs to be
	 * disabled, as BK is of the lowest priority and an upgrade to it does
	 * not result in any changes for the frames.
	 *
	 * This attribute behavior is similar to
	 * QCA_WLAN_VENDOR_ATTR_CONFIG_UDP_QOS_UPGRADE with the difference that
	 * only UDP frames of BE or BK access category are upgraded and not
	 * UDP frames of VI or VO access category.
	 *
	 * This attribute is not recommended to be used as it blindly forces all
	 * UDP packets of BE or BK access category to a higher access category
	 * which could impact the traffic pattern of all apps using UDP and can
	 * cause unknown behavior.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_UDP_QOS_UPGRADE_FOR_BE_BK = 84,

	/* 8-bit unsigned value to configure the driver to enable/disable the
	 * periodic sounding for Tx beamformer functionality. The default
	 * behavior uses algorithm to do sounding based on packet stats.
	 *
	 * 0 - Default behavior.
	 * 1 - Enable the periodic sounding for Tx beamformer.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_BEAMFORMER_PERIODIC_SOUNDING = 85,

	/* 8-bit unsigned value, whenever wifi calling (wfc) begins or ends,
	 * userspace sends this information to the driver/firmware to configure
	 * wfc state. The driver/firmware uses this information to
	 * optimize power savings, rate adaption, roaming, etc.
	 *
	 * 1 - wfc is on.
	 * 0 - wfc is off.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_WFC_STATE = 86,

	/* 8-bit unsigned value to configure the driver to enable/disable the
	 * EHT EML capability in management frame EHT capabilities.
	 * 1 - Enable, 0 - Disable.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_EHT_EML_CAPABILITY = 87,

	/* 8-bit unsigned value to configure the driver with EHT MLO max
	 * simultaneous links to be used for MLO connection.
	 * The range of the value is 0 to 14.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_EHT_MLO_MAX_SIMULTANEOUS_LINKS = 88,

	/* 8-bit unsigned value to configure the driver with EHT MLO maximum
	 * number of links to be used for MLO connection. Value 0 restores the
	 * default value of the maximum MLO links capability of the device.
	 * The range of the value is 0 to 15.
	 *
	 * 0 - Restore default device limit.
	 * 1 to 15 - Set the maximum number of links to be used for an MLO
	 * connection.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_EHT_MLO_MAX_NUM_LINKS = 89,

	/* 8-bit unsigned value to configure the driver with EHT MLO mode.
	 * Uses enum qca_wlan_eht_mlo_mode values.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_EHT_MLO_MODE = 90,

	/* Nested attribute with frequencies in u32 attributes to configure a
	 * list of allowed 20 MHz channel center frequencies in MHz for AP
	 * operation. Whenever performing a channel selection operation, the
	 * driver shall generate a new list based on this provided list by
	 * filtering out channels that cannot be used at that time due to
	 * regulatory or other constraints. The resulting list is used as the
	 * list of all allowed channels, i.e., operation on any channel that is
	 * not included is not allowed, whenever performing operations like ACS
	 * and DFS.
	 *
	 * Userspace shall configure this before starting the AP and the
	 * configuration is valid only from the next BSS start and until the
	 * BSS is stopped. The driver shall clear this configuration when the
	 * AP is stopped and fall back to the default behavior for subsequent
	 * AP operation.
	 *
	 * The default behavior when this configuration is not applicable is the
	 * driver can choose any of the channels supported by the hardware
	 * except the channels that cannot be used due to regulatory or other
	 * constraints.
	 *
	 * The driver shall reject this configuration if done after the AP is
	 * started. This attribute can be used to specify user's choice of
	 * frequencies and static puncture channel list, etc.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_AP_ALLOWED_FREQ_LIST = 91,

	/* Nested attribute to indicate EHT MLO links to be forced active.
	 * It contains link MAC address attributes. These nested attributes are
	 * of the type NL80211_ATTR_MAC and are used to force enabling of the
	 * MLO links corresponding to the indicated link MAC addresses.
	 * Subsequently, the links corresponding to the link MAC addresses that
	 * are not indicated are forced inactive.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_EHT_MLO_ACTIVE_LINKS = 92,

	/* 8-bit unsigned value to configure EMLSR mode entry or exit.
	 * Uses enum qca_wlan_emlsr_mode values.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_EMLSR_MODE_SWITCH = 93,

	/* 8-bit unsigned value to configure the channel bandwidth
	 * for CTS frame transmission during the dymamic bandwidth
	 * signaling CTS procedure referred in IEEE Std 802.11-2020,
	 * 10.3.2.9 CTS and DMG CTS procedure.
	 * This configuration is used for testing purposes.
	 *
	 * This uses values defined in enum nl80211_chan_width.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_CTS_CHANNEL_WIDTH = 94,

	/* 8-bit unsigned value. This attribute is used to dynamically
	 * enable/suspend trigger based UL MU transmission.
	 * This is supported in STA mode and the device sends Operating
	 * Mode Indication to inform the change as described in
	 * IEEE Std 802.11ax-2021, 26.9.
	 *
	 * This attribute can be configured when the STA is associated
	 * to an AP and the configuration is maintained until the current
	 * association terminates.
	 *
	 * By default all UL MU transmissions are enabled.
	 *
	 * Uses enum qca_ul_mu_config values.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_UL_MU_CONFIG = 95,

	/* 8-bit unsigned value. Optionally specified along with
	 * %QCA_WLAN_VENDOR_ATTR_CONFIG_CHANNEL_WIDTH when STA is in connected
	 * state. This configuration is applicable only for the current
	 * connection. This configuration not allowed in disconnected state.
	 * This configuration can be sent inside
	 * %QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINKS to specify the maximum
	 * supported channel width update type per-MLO link.
	 *
	 * Uses enum qca_chan_width_update_type values.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_CHAN_WIDTH_UPDATE_TYPE = 96,

	/* 8-bit unsigned value to set EPCS (Emergency Preparedness
	 * Communications Service) feature capability
	 * 1 - Enable, 0 - Disable.
	 *
	 * This configuration is used for testing purposes.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_EPCS_CAPABILITY = 97,

	/* 8-bit unsigned value to enable/disable EPCS priority access
	 * 1 - Enable, 0 - Disable.
	 * The EPCS priority access shall be enabled only when EPCS feature
	 * capability is also enabled (see
	 * QCA_WLAN_VENDOR_ATTR_CONFIG_EPCS_CAPABILITY).
	 *
	 * This configuration is used for testing purposes.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_EPCS_FUNCTION = 98,

	/* 8-bit unsigned value. Used to specify the MLO link ID of a link
	 * that is being configured. This attribute must be included in each
	 * record nested inside %QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINKS, and
	 * may be included without nesting to indicate the link that is the
	 * target of other configuration attributes.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID = 99,

	/* Array of nested links each identified by
	 * %QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID. This uses values defined in
	 * enum qca_wlan_vendor_attr_config, explicit documentation shall be
	 * added for the attributes in enum qca_wlan_vendor_attr_config to
	 * support per-MLO link configuration through
	 * %QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINKS.
	 *
	 * Userspace can configure a single link or multiple links with this
	 * attribute by nesting the corresponding configuration attributes and
	 * %QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID for each link.
	 *
	 * Userspace can fetch the configuration attribute values for a single
	 * link or multiple links with this attribute by nesting the
	 * corresponding configuration attributes and
	 * %QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID for each link.
	 *
	 * For STA interface, this attribute is applicable only in connected
	 * state when the current connection is MLO capable. The valid values of
	 * %QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID are the link IDs of the
	 * connected AP MLD links.
	 *
	 * For AP interface, this configuration applicable only after adding
	 * MLO links to the AP interface with %NL80211_CMD_ADD_LINK and the
	 * valid values of %QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID are the link
	 * IDs specified in %NL80211_CMD_ADD_LINK while adding the MLO links to
	 * the AP interface.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINKS = 100,

	/* 16-bit unsigned value to configure power save inactivity timeout in
	 * milliseconds.
	 *
	 * STA enters into power save mode (PM=1) after TX/RX inactivity of time
	 * duration specified by %QCA_WLAN_VENDOR_ATTR_CONFIG_OPM_ITO.
	 *
	 * This attribute shall be configured along with
	 * %QCA_WLAN_VENDOR_ATTR_CONFIG_OPM_SPEC_WAKE_INTERVAL when
	 * %QCA_WLAN_VENDOR_ATTR_CONFIG_OPTIMIZED_POWER_MANAGEMENT
	 * is set to %QCA_WLAN_VENDOR_OPM_MODE_USER_DEFINED mode.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_OPM_ITO = 101,

	/* 16-bit unsigned value to configure speculative wake interval in
	 * milliseconds.
	 *
	 * STA speculatively wakes up to look for buffered data by AP at
	 * %QCA_WLAN_VENDOR_ATTR_CONFIG_OPM_SPEC_WAKE_INTERVAL interval after
	 * entering into power save. If configured zero, STA wakes up at
	 * upcoming DTIM beacon.
	 *
	 * This attribute shall be configured along with
	 * %QCA_WLAN_VENDOR_ATTR_CONFIG_OPM_ITO and
	 * %QCA_WLAN_VENDOR_ATTR_CONFIG_OPTIMIZED_POWER_MANAGEMENT
	 * to %QCA_WLAN_VENDOR_OPM_MODE_USER_DEFINED mode.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_OPM_SPEC_WAKE_INTERVAL = 102,

	/*
	 * 16-bit unsigned value to configure TX max A-MPDU count.
	 *
	 * For STA interface, this attribute is applicable only in connected
	 * state, peer MAC address is not required to be provided.
	 *
	 * For AP interface, this attribute is applicable only in started
	 * state and one of the associated peer STAs must be specified with
	 * QCA_WLAN_VENDOR_ATTR_CONFIG_PEER_MAC. If this is for an ML
	 * association, the peer MAC address provided is the link address of
	 * the non-AP MLD.
	 *
	 * This attribute runtime configures the TX maximum aggregation size.
	 * The value must be in range of 1 to BA window size for the specific
	 * peer.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_PEER_AMPDU_CNT = 103,

	/*
	 * 8-bit unsigned value to configure TID-to-link mapping negotiation
	 * type.
	 * Uses enum qca_wlan_ttlm_negotiation_support values.
	 *
	 * This value applies to the complete AP/non-AP MLD interface, and the
	 * MLD advertises it within the Basic Multi-Link element in the
	 * association frames. If a new value is configured during an active
	 * connection, it will take effect in the subsequent associations and
	 * is not reset during disconnection.
	 *
	 * This attribute is used for testing purposes.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_TTLM_NEGOTIATION_SUPPORT = 104,

	/* 8-bit unsigned value.
	 *
	 * This attribute configures a traffic shaping mode
	 * applied during coex scenarios.
	 * By default all coex traffic shaping modes are enabled,
	 * i.e., shape WLAN traffic based on coex traffic pattern and priority.
	 * To shape traffic, STA may enter in power save mode
	 * and AP may send CTS-to-self frame.
	 *
	 * Uses enum qca_coex_traffic_shaping_mode values.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_COEX_TRAFFIC_SHAPING_MODE = 105,

	/* 8-bit unsigned value.
	 *
	 * This attribute is used to specify whether an associated peer is a QCA
	 * device. The associated peer is specified with
	 * QCA_WLAN_VENDOR_ATTR_CONFIG_PEER_MAC. For MLO cases, the MLD MAC
	 * address of the peer is used.
	 * 1 - QCA device, 0 - non-QCA device.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_QCA_PEER = 106,

	/* 8-bit unsigned value to configure BTM support.
	 *
	 * The attribute is applicable only for STA interface. Uses enum
	 * qca_wlan_btm_support values. This configuration is not allowed in
	 * connected state.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_BTM_SUPPORT = 107,

	/* 16-bit unsigned value to configure client's keep-alive interval in
	 * seconds. The driver will reduce the keep-alive interval to this
	 * configured value if the AP advertises BSS maximum idle period and if
	 * that BSS max idle period is larger than this configured value. If the
	 * AP does not advertise a maximum value, the configured value will be
	 * used as a keep-alive period for unprotected frames.
	 *
	 * This configuration is applicable only during the STA's current
	 * association.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_KEEP_ALIVE_INTERVAL = 108,

	/* 8-bit unsigned value to configure reduced power scan mode.
	 *
	 * This attribute is used to configure the driver to optimize power
	 * during scan. For example, the driver can switch to 1x1 from 2x2 mode
	 * for additional power save.
	 *
	 * 1 - Enable reduced power scan mode.
	 * 0 - Disable reduced power scan mode.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_REDUCED_POWER_SCAN_MODE = 109,

	/* 8-bit unsigned value. Refer to TX/RX NSS and chain configurations */
	QCA_WLAN_VENDOR_ATTR_CONFIG_TX_NSS_2GHZ = 110,

	/* 8-bit unsigned value. Refer to TX/RX NSS and chain configurations */
	QCA_WLAN_VENDOR_ATTR_CONFIG_RX_NSS_2GHZ = 111,

	/* 8-bit unsigned value. Refer to TX/RX NSS and chain configurations */
	QCA_WLAN_VENDOR_ATTR_CONFIG_TX_NSS_5GHZ = 112,

	/* 8-bit unsigned value. Refer to TX/RX NSS and chain configurations */
	QCA_WLAN_VENDOR_ATTR_CONFIG_RX_NSS_5GHZ = 113,

	/* 8-bit unsigned value. Refer to TX/RX NSS and chain configurations */
	QCA_WLAN_VENDOR_ATTR_CONFIG_NUM_TX_CHAINS_2GHZ = 114,

	/* 8-bit unsigned value. Refer to TX/RX NSS and chain configurations */
	QCA_WLAN_VENDOR_ATTR_CONFIG_NUM_RX_CHAINS_2GHZ = 115,

	/* 8-bit unsigned value. Refer to TX/RX NSS and chain configurations */
	QCA_WLAN_VENDOR_ATTR_CONFIG_NUM_TX_CHAINS_5GHZ = 116,

	/* 8-bit unsigned value. Refer to TX/RX NSS and chain configurations */
	QCA_WLAN_VENDOR_ATTR_CONFIG_NUM_RX_CHAINS_5GHZ = 117,

	/* 16-bit unsigned value. This attribute is used to dynamically
	 * configure the time duration of data stall detection. Unit is
	 * milliseconds. Valid value range is 0 or 10 ms to 10000 ms. If the
	 * value is 0, the previously configured value is cleared. The driver
	 * rejects this configuration if the value is out of range. This
	 * configuration is effective for all connections on the chip. If the
	 * duration is greater than this configuration and consecutive TX no ack
	 * count is greater than
	 * QCA_WLAN_VENDOR_ATTR_CONFIG_CONSECUTIVE_TX_NO_ACK_THRESHOLD,
	 * data stall event is sent to userspace.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_CONSECUTIVE_TX_NO_ACK_DURATION = 118,

	/* 16-bit unsigned value. This attribute is used to dynamically
	 * configure the threshold of data stall detection. Valid value is 0 or
	 * greater than 10. if the value is 0, the previously configured value
	 * is cleared. The driver rejects this configuration if the value is out
	 * of range. This configuration is effective for all connections on the
	 * chip. If consecutive TX no ack count is greater than this
	 * configuration and duration is greater than
	 * QCA_WLAN_VENDOR_ATTR_CONFIG_CONSECUTIVE_TX_NO_ACK_DURATION,
	 * data stall event is sent to userspace.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_CONSECUTIVE_TX_NO_ACK_THRESHOLD = 119,

	/* 8-bit unsigned value to configure the interface offload type
	 *
	 * This attribute is used to configure the interface offload capability.
	 * User can configure software based acceleration, hardware based
	 * acceleration, or a combination of both using this option. More
	 * details on each option is described under the enum definition below.
	 * Uses enum qca_wlan_intf_offload_type for values.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_INTF_OFFLOAD_TYPE = 120,

	/* 8-bit unsigned integer to configure the driver to follow AP's
	 * preference values to select a roam candidate from BTM request.
	 *
	 * This attribute is used to configure the driver to select the roam
	 * candidate based on AP advertised preference values. If not set,
	 * the driver uses its internal scoring algorithm to do the same.
	 *
	 * 1 - STA follows AP's preference values to select a roam candidate
	 * 0 - STA uses internal scoring algorithm to select a roam candidate
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_FOLLOW_AP_PREFERENCE_FOR_CNDS_SELECT = 121,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_CONFIG_MAX =
	QCA_WLAN_VENDOR_ATTR_CONFIG_AFTER_LAST - 1,
};

/* Compatibility defines for previously used incorrect enum
 * qca_wlan_vendor_attr_config names. These values should not be used in any
 * new implementation. */
#define QCA_WLAN_VENDOR_ATTR_DISCONNECT_IES \
	QCA_WLAN_VENDOR_ATTR_CONFIG_DISCONNECT_IES
#define QCA_WLAN_VENDOR_ATTR_BEACON_REPORT_FAIL \
	QCA_WLAN_VENDOR_ATTR_CONFIG_BEACON_REPORT_FAIL

/**
 * enum qca_ul_mu_config - UL MU configuration
 * @QCA_UL_MU_SUSPEND - All trigger based UL MU transmission is suspended
 * @QCA_UL_MU_ENABLE - All trigger based UL MU transmission is enabled
 */
enum qca_ul_mu_config {
	QCA_UL_MU_SUSPEND = 0,
	QCA_UL_MU_ENABLE = 1,
};

/**
 * enum qca_dbam_config - Specifies DBAM config mode
 * @QCA_DBAM_DISABLE: Firmware disables DBAM
 * @QCA_DBAM_ENABLE: Firmware enables DBAM opportunistically when
 * internal criteria are met.
 * @QCA_DBAM_FORCE_ENABLE: Firmware enables DBAM forcefully.
 */
enum qca_dbam_config {
	QCA_DBAM_DISABLE = 0,
	QCA_DBAM_ENABLE = 1,
	QCA_DBAM_FORCE_ENABLE = 2,
};

/**
 * enum qca_wlan_ani_setting - ANI setting type
 * @QCA_WLAN_ANI_SETTING_AUTO: Automatically determine ANI level
 * @QCA_WLAN_ANI_SETTING_FIXED: Fix ANI level to the dBm parameter
 */
enum qca_wlan_ani_setting {
	QCA_WLAN_ANI_SETTING_AUTO = 0,
	QCA_WLAN_ANI_SETTING_FIXED = 1,
};

/**
 * enum qca_wlan_vendor_attr_sap_config - Parameters for AP configuration
 *
 * @QCA_WLAN_VENDOR_ATTR_SAP_CONFIG_CHANNEL: Optional (u8)
 * Channel number on which Access Point should restart.
 * Note: If both the driver and user space application supports the 6 GHz band,
 * this attribute is deprecated and QCA_WLAN_VENDOR_ATTR_SAP_CONFIG_FREQUENCY
 * should be used.
 * To maintain backward compatibility, QCA_WLAN_VENDOR_ATTR_SAP_CONFIG_CHANNEL
 * is still used if either of the driver or user space application doesn't
 * support the 6 GHz band.
 *
 * @QCA_WLAN_VENDOR_ATTR_SAP_CONFIG_FREQUENCY: Optional (u32)
 * Channel center frequency (MHz) on which the access point should restart.
 */
enum qca_wlan_vendor_attr_sap_config {
	QCA_WLAN_VENDOR_ATTR_SAP_CONFIG_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SAP_CONFIG_CHANNEL = 1,

	/* List of frequencies on which AP is expected to operate.
	 * This is irrespective of ACS configuration. This list is a priority
	 * based one and is looked for before the AP is created to ensure the
	 * best concurrency sessions (avoid MCC and use DBS/SCC) co-exist in
	 * the system.
	 */
	QCA_WLAN_VENDOR_ATTR_SAP_MANDATORY_FREQUENCY_LIST = 2,
	QCA_WLAN_VENDOR_ATTR_SAP_CONFIG_FREQUENCY = 3,

	QCA_WLAN_VENDOR_ATTR_SAP_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SAP_CONFIG_MAX =
	QCA_WLAN_VENDOR_ATTR_SAP_CONFIG_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_sap_conditional_chan_switch - Parameters for AP
 *					conditional channel switch
 */
enum qca_wlan_vendor_attr_sap_conditional_chan_switch {
	QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_INVALID = 0,
	/* Priority based frequency list (an array of u32 values in host byte
	 * order)
	 */
	QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_FREQ_LIST = 1,
	/* Status of the conditional switch (u32).
	 * 0: Success, Non-zero: Failure
	 */
	QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_STATUS = 2,

	QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_MAX =
	QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_gpio_attr - Parameters for GPIO configuration
 *
 * @QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_COMMAND: Required (u32)
 * value to specify the GPIO command. Please refer to enum qca_gpio_cmd_type
 * for the available values.
 *
 * @QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_PINNUM: Required (u32)
 * value to specify the GPIO number.
 * This is required, when %QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_COMMAND is
 * %QCA_WLAN_VENDOR_GPIO_CONFIG or %QCA_WLAN_VENDOR_GPIO_OUTPUT.
 *
 * @QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_VALUE: Required (u32)
 * value to specify the GPIO output level. Please refer to enum qca_gpio_value
 * for the available values.
 * This is required, when %QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_COMMAND is
 * %QCA_WLAN_VENDOR_GPIO_OUTPUT.
 *
 * @QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_PULL_TYPE: Optional (u32)
 * value to specify the GPIO pull type. Please refer to enum qca_gpio_pull_type
 * for the available values.
 * This is required, when %QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_COMMAND is
 * %QCA_WLAN_VENDOR_GPIO_CONFIG and
 * %QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_INTERNAL_CONFIG attribute is not present.
 * Optional when %QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_INTERNAL_CONFIG
 * attribute is present.
 *
 * @QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_INTR_MODE: Optional (u32)
 * value to specify the GPIO interrupt mode. Please refer to enum
 * qca_gpio_interrupt_mode for the available values.
 * This is required, when %QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_COMMAND is
 * %QCA_WLAN_VENDOR_GPIO_CONFIG and
 * %QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_INTERNAL_CONFIG attribute is not present.
 * Optional when %QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_INTERNAL_CONFIG
 * attribute is present.
 *
 * @QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_DIR: Optional (u32)
 * value to specify the GPIO direction. Please refer to enum qca_gpio_direction
 * for the available values.
 * This is required, when %QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_COMMAND is
 * %QCA_WLAN_VENDOR_GPIO_CONFIG and
 * %QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_INTERNAL_CONFIG attribute is not present.
 * Optional when %QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_INTERNAL_CONFIG
 * attribute is present.
 *
 * @QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_MUX_CONFIG: Optional (u32)
 * Value to specify the mux config. Meaning of a given value is dependent
 * on the target chipset and GPIO pin. Must be of the range 0-15.
 * Optional when %QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_COMMAND is
 * %QCA_WLAN_VENDOR_GPIO_CONFIG. Defaults to 0.
 *
 * @QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_DRIVE: Optional (u32)
 * Value to specify the drive, refer to enum qca_gpio_drive.
 * Optional when %QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_COMMAND is
 * %QCA_WLAN_VENDOR_GPIO_CONFIG. Defaults to QCA_WLAN_GPIO_DRIVE_2MA(0).
 *
 * @QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_INTERNAL_CONFIG: Optional (flag)
 * Optional when %QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_COMMAND is
 * %QCA_WLAN_VENDOR_GPIO_CONFIG. When present this attribute signals that all
 * other parameters for the given GPIO will be obtained from internal
 * configuration. Only %QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_PINNUM must be
 * specified to indicate the GPIO pin being configured.
 */
enum qca_wlan_gpio_attr {
	QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_INVALID = 0,
	/* Unsigned 32-bit attribute for GPIO command */
	QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_COMMAND = 1,
	/* Unsigned 32-bit attribute for GPIO PIN number to configure */
	QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_PINNUM = 2,
	/* Unsigned 32-bit attribute for GPIO value to configure */
	QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_VALUE = 3,
	/* Unsigned 32-bit attribute for GPIO pull type */
	QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_PULL_TYPE = 4,
	/* Unsigned 32-bit attribute for GPIO interrupt mode */
	QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_INTR_MODE = 5,
	/* Unsigned 32-bit attribute for GPIO direction to configure */
	QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_DIR = 6,
	/* Unsigned 32-bit attribute for GPIO mux config */
	QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_MUX_CONFIG = 7,
	/* Unsigned 32-bit attribute for GPIO drive */
	QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_DRIVE = 8,
	/* Flag attribute for using internal GPIO configuration */
	QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_INTERNAL_CONFIG = 9,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_LAST,
	QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_MAX =
	QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_LAST - 1
};

/**
 * enum gpio_cmd_type - GPIO configuration command type
 * @QCA_WLAN_VENDOR_GPIO_CONFIG: Set GPIO configuration info
 * @QCA_WLAN_VENDOR_GPIO_OUTPUT: Set GPIO output level
 */
enum qca_gpio_cmd_type {
	QCA_WLAN_VENDOR_GPIO_CONFIG = 0,
	QCA_WLAN_VENDOR_GPIO_OUTPUT = 1,
};

/**
 * enum qca_gpio_pull_type - GPIO pull type
 * @QCA_WLAN_GPIO_PULL_NONE: Set GPIO pull type to none
 * @QCA_WLAN_GPIO_PULL_UP: Set GPIO pull up
 * @QCA_WLAN_GPIO_PULL_DOWN: Set GPIO pull down
 */
enum qca_gpio_pull_type {
	QCA_WLAN_GPIO_PULL_NONE = 0,
	QCA_WLAN_GPIO_PULL_UP = 1,
	QCA_WLAN_GPIO_PULL_DOWN = 2,
	QCA_WLAN_GPIO_PULL_MAX,
};

/**
 * enum qca_gpio_direction - GPIO direction
 * @QCA_WLAN_GPIO_INPUT: Set GPIO as input mode
 * @QCA_WLAN_GPIO_OUTPUT: Set GPIO as output mode
 * @QCA_WLAN_GPIO_VALUE_MAX: Invalid value
 */
enum qca_gpio_direction {
	QCA_WLAN_GPIO_INPUT = 0,
	QCA_WLAN_GPIO_OUTPUT = 1,
	QCA_WLAN_GPIO_DIR_MAX,
};

/**
 * enum qca_gpio_value - GPIO Value
 * @QCA_WLAN_GPIO_LEVEL_LOW: set gpio output level to low
 * @QCA_WLAN_GPIO_LEVEL_HIGH: set gpio output level to high
 * @QCA_WLAN_GPIO_LEVEL_MAX: Invalid value
 */
enum qca_gpio_value {
	QCA_WLAN_GPIO_LEVEL_LOW = 0,
	QCA_WLAN_GPIO_LEVEL_HIGH = 1,
	QCA_WLAN_GPIO_LEVEL_MAX,
};

/**
 * enum gpio_interrupt_mode - GPIO interrupt mode
 * @QCA_WLAN_GPIO_INTMODE_DISABLE: Disable interrupt trigger
 * @QCA_WLAN_GPIO_INTMODE_RISING_EDGE: Interrupt with GPIO rising edge trigger
 * @QCA_WLAN_GPIO_INTMODE_FALLING_EDGE: Interrupt with GPIO falling edge trigger
 * @QCA_WLAN_GPIO_INTMODE_BOTH_EDGE: Interrupt with GPIO both edge trigger
 * @QCA_WLAN_GPIO_INTMODE_LEVEL_LOW: Interrupt with GPIO level low trigger
 * @QCA_WLAN_GPIO_INTMODE_LEVEL_HIGH: Interrupt with GPIO level high trigger
 * @QCA_WLAN_GPIO_INTMODE_MAX: Invalid value
 */
enum qca_gpio_interrupt_mode {
	QCA_WLAN_GPIO_INTMODE_DISABLE = 0,
	QCA_WLAN_GPIO_INTMODE_RISING_EDGE = 1,
	QCA_WLAN_GPIO_INTMODE_FALLING_EDGE = 2,
	QCA_WLAN_GPIO_INTMODE_BOTH_EDGE = 3,
	QCA_WLAN_GPIO_INTMODE_LEVEL_LOW = 4,
	QCA_WLAN_GPIO_INTMODE_LEVEL_HIGH = 5,
	QCA_WLAN_GPIO_INTMODE_MAX,
};

/**
 * enum qca_gpio_drive - GPIO drive
 * @QCA_WLAN_GPIO_DRIVE_2MA: drive 2MA
 * @QCA_WLAN_GPIO_DRIVE_4MA: drive 4MA
 * @QCA_WLAN_GPIO_DRIVE_6MA: drive 6MA
 * @QCA_WLAN_GPIO_DRIVE_8MA: drive 8MA
 * @QCA_WLAN_GPIO_DRIVE_10MA: drive 10MA
 * @QCA_WLAN_GPIO_DRIVE_12MA: drive 12MA
 * @QCA_WLAN_GPIO_DRIVE_14MA: drive 14MA
 * @QCA_WLAN_GPIO_DRIVE_16MA: drive 16MA
 * @QCA_WLAN_GPIO_DRIVE_MAX: invalid GPIO drive
 */
enum qca_gpio_drive {
	QCA_WLAN_GPIO_DRIVE_2MA = 0,
	QCA_WLAN_GPIO_DRIVE_4MA = 1,
	QCA_WLAN_GPIO_DRIVE_6MA = 2,
	QCA_WLAN_GPIO_DRIVE_8MA = 3,
	QCA_WLAN_GPIO_DRIVE_10MA = 4,
	QCA_WLAN_GPIO_DRIVE_12MA = 5,
	QCA_WLAN_GPIO_DRIVE_14MA = 6,
	QCA_WLAN_GPIO_DRIVE_16MA = 7,
	QCA_WLAN_GPIO_DRIVE_MAX,
};

/**
 * qca_wlan_set_qdepth_thresh_attr - Parameters for setting
 * MSDUQ depth threshold per peer per tid in the target
 *
 * Associated Vendor Command:
 * QCA_NL80211_VENDOR_SUBCMD_SET_QDEPTH_THRESH
 */
enum qca_wlan_set_qdepth_thresh_attr {
	QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_INVALID = 0,
	/* 6-byte MAC address */
	QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_MAC_ADDR,
	/* Unsigned 32-bit attribute for holding the TID */
	QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_TID,
	/* Unsigned 32-bit attribute for holding the update mask
	 * bit 0 - Update high priority msdu qdepth threshold
	 * bit 1 - Update low priority msdu qdepth threshold
	 * bit 2 - Update UDP msdu qdepth threshold
	 * bit 3 - Update Non UDP msdu qdepth threshold
	 * rest of bits are reserved
	 */
	QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_UPDATE_MASK,
	/* Unsigned 32-bit attribute for holding the threshold value */
	QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_VALUE,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_LAST,
	QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_MAX =
		QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_LAST - 1,
};

/**
 * enum qca_acs_dfs_mode - Defines different types of DFS channel
 * configurations for ACS operation.
 *
 * @QCA_ACS_DFS_MODE_NONE: Refer to invalid DFS mode
 * @QCA_ACS_DFS_MODE_ENABLE: Consider DFS channels in ACS operation
 * @QCA_ACS_DFS_MODE_DISABLE: Do not consider DFS channels in ACS operation
 * @QCA_ACS_DFS_MODE_DEPRIORITIZE: Deprioritize DFS channels in ACS operation
 */
enum qca_acs_dfs_mode {
	QCA_ACS_DFS_MODE_NONE = 0,
	QCA_ACS_DFS_MODE_ENABLE = 1,
	QCA_ACS_DFS_MODE_DISABLE = 2,
	QCA_ACS_DFS_MODE_DEPRIORITIZE = 3,
};

/**
 * enum qca_wlan_vendor_attr_acs_config - Defines Configuration attributes
 * used by the vendor command QCA_NL80211_VENDOR_SUBCMD_ACS_POLICY.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_DFS_MODE: Required (u8)
 * DFS mode for ACS operation from enum qca_acs_dfs_mode.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_CHANNEL_HINT: Required (u8)
 * channel number hint for ACS operation, if valid channel is specified then
 * ACS operation gives priority to this channel.
 * Note: If both the driver and user space application supports the 6 GHz band,
 * this attribute is deprecated and QCA_WLAN_VENDOR_ATTR_ACS_FREQUENCY_HINT
 * should be used.
 * To maintain backward compatibility, QCA_WLAN_VENDOR_ATTR_ACS_CHANNEL_HINT
 * is still used if either of the driver or user space application doesn't
 * support the 6 GHz band.
 *
 * @QCA_WLAN_VENDOR_ATTR_ACS_FREQUENCY_HINT: Required (u32).
 * Channel center frequency (MHz) hint for ACS operation, if a valid center
 * frequency is specified, ACS operation gives priority to this channel.
 */
enum qca_wlan_vendor_attr_acs_config {
	QCA_WLAN_VENDOR_ATTR_ACS_MODE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ACS_DFS_MODE = 1,
	QCA_WLAN_VENDOR_ATTR_ACS_CHANNEL_HINT = 2,
	QCA_WLAN_VENDOR_ATTR_ACS_FREQUENCY_HINT = 3,

	QCA_WLAN_VENDOR_ATTR_ACS_DFS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ACS_DFS_MAX =
		QCA_WLAN_VENDOR_ATTR_ACS_DFS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_get_hw_capability - Wi-Fi hardware capability
 */
enum qca_wlan_vendor_attr_get_hw_capability {
	QCA_WLAN_VENDOR_ATTR_HW_CAPABILITY_INVALID,
	/* Antenna isolation
	 * An attribute used in the response.
	 * The content of this attribute is encoded in a byte array. Each byte
	 * value is an antenna isolation value. The array length is the number
	 * of antennas.
	 */
	QCA_WLAN_VENDOR_ATTR_ANTENNA_ISOLATION,
	/* Request HW capability
	 * An attribute used in the request.
	 * The content of this attribute is a u32 array for one or more of
	 * hardware capabilities (attribute IDs) that are being requested. Each
	 * u32 value has a value from this
	 * enum qca_wlan_vendor_attr_get_hw_capability
	 * identifying which capabilities are requested.
	 */
	QCA_WLAN_VENDOR_ATTR_GET_HW_CAPABILITY,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_HW_CAPABILITY_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_HW_CAPABILITY_MAX =
	QCA_WLAN_VENDOR_ATTR_HW_CAPABILITY_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_ll_stats_ext - Attributes for MAC layer monitoring
 *    offload which is an extension for LL_STATS.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_CFG_PERIOD: Monitoring period. Unit in ms.
 *    If MAC counters do not exceed the threshold, FW will report monitored
 *    link layer counters periodically as this setting. The first report is
 *    always triggered by this timer.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_CFG_THRESHOLD: It is a percentage (1-99).
 *    For each MAC layer counter, FW holds two copies. One is the current value.
 *    The other is the last report. Once a current counter's increment is larger
 *    than the threshold, FW will indicate that counter to host even if the
 *    monitoring timer does not expire.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_CHG: Peer STA power state change
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TID: TID of MSDU
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_NUM_MSDU: Count of MSDU with the same
 *    failure code.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_STATUS: TX failure code
 *    1: TX packet discarded
 *    2: No ACK
 *    3: Postpone
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_MAC_ADDRESS: peer MAC address
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_STATE: Peer STA current state
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_GLOBAL: Global threshold.
 *    Threshold for all monitored parameters. If per counter dedicated threshold
 *    is not enabled, this threshold will take effect.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_EVENT_MODE: Indicate what triggers this
 *    event, PERORID_TIMEOUT == 1, THRESH_EXCEED == 0.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IFACE_ID: interface ID
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_ID: peer ID
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_BITMAP: bitmap for TX counters
 *    Bit0: TX counter unit in MSDU
 *    Bit1: TX counter unit in MPDU
 *    Bit2: TX counter unit in PPDU
 *    Bit3: TX counter unit in byte
 *    Bit4: Dropped MSDUs
 *    Bit5: Dropped Bytes
 *    Bit6: MPDU retry counter
 *    Bit7: MPDU failure counter
 *    Bit8: PPDU failure counter
 *    Bit9: MPDU aggregation counter
 *    Bit10: MCS counter for ACKed MPDUs
 *    Bit11: MCS counter for Failed MPDUs
 *    Bit12: TX Delay counter
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_BITMAP: bitmap for RX counters
 *    Bit0: MAC RX counter unit in MPDU
 *    Bit1: MAC RX counter unit in byte
 *    Bit2: PHY RX counter unit in PPDU
 *    Bit3: PHY RX counter unit in byte
 *    Bit4: Disorder counter
 *    Bit5: Retry counter
 *    Bit6: Duplication counter
 *    Bit7: Discard counter
 *    Bit8: MPDU aggregation size counter
 *    Bit9: MCS counter
 *    Bit10: Peer STA power state change (wake to sleep) counter
 *    Bit11: Peer STA power save counter, total time in PS mode
 *    Bit12: Probe request counter
 *    Bit13: Other management frames counter
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_CCA_BSS_BITMAP: bitmap for CCA
 *    Bit0: Idle time
 *    Bit1: TX time
 *    Bit2: time RX in current bss
 *    Bit3: Out of current bss time
 *    Bit4: Wireless medium busy time
 *    Bit5: RX in bad condition time
 *    Bit6: TX in bad condition time
 *    Bit7: time wlan card not available
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_SIGNAL_BITMAP: bitmap for signal
 *    Bit0: Per channel SNR counter
 *    Bit1: Per channel noise floor counter
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_NUM: number of peers
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_CHANNEL_NUM: number of channels
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_AC_RX_NUM: number of RX stats
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_CCA_BSS: per channel BSS CCA stats
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER: container for per PEER stats
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_MSDU: Number of total TX MSDUs
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_MPDU: Number of total TX MPDUs
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_PPDU: Number of total TX PPDUs
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_BYTES: bytes of TX data
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_DROP: Number of dropped TX packets
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_DROP_BYTES: Bytes dropped
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_RETRY: waiting time without an ACK
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_NO_ACK: number of MPDU not-ACKed
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_NO_BACK: number of PPDU not-ACKed
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_AGGR_NUM:
 *    aggregation stats buffer length
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_SUCC_MCS_NUM: length of mcs stats
 *    buffer for ACKed MPDUs.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_FAIL_MCS_NUM: length of mcs stats
 *    buffer for failed MPDUs.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_DELAY_ARRAY_SIZE:
 *    length of delay stats array.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_AGGR: TX aggregation stats
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_SUCC_MCS: MCS stats for ACKed MPDUs
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_FAIL_MCS: MCS stats for failed MPDUs
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_DELAY: tx delay stats
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU: MPDUs received
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_BYTES: bytes received
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_PPDU: PPDU received
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_PPDU_BYTES: PPDU bytes received
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_LOST: packets lost
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_RETRY: number of RX packets
 *    flagged as retransmissions
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_DUP: number of RX packets
 *    flagged as duplicated
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_DISCARD: number of RX
 *    packets discarded
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_AGGR_NUM: length of RX aggregation
 *    stats buffer.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MCS_NUM: length of RX mcs
 *    stats buffer.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MCS: RX mcs stats buffer
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_AGGR: aggregation stats buffer
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_TIMES: times STAs go to sleep
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_DURATION: STAs' total sleep time
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_PROBE_REQ: number of probe
 *    requests received
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MGMT: number of other mgmt
 *    frames received
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IDLE_TIME: Percentage of idle time
 *    there is no TX, nor RX, nor interference.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_TIME: percentage of time
 *    transmitting packets.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_TIME: percentage of time
 *    for receiving.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_BUSY: percentage of time
 *    interference detected.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_BAD: percentage of time
 *    receiving packets with errors.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_BAD: percentage of time
 *    TX no-ACK.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_NO_AVAIL: percentage of time
 *    the chip is unable to work in normal conditions.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IN_BSS_TIME: percentage of time
 *    receiving packets in current BSS.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_OUT_BSS_TIME: percentage of time
 *    receiving packets not in current BSS.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_ANT_NUM: number of antennas
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_SIGNAL:
 *    This is a container for per antenna signal stats.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_ANT_SNR: per antenna SNR value
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_ANT_NF: per antenna NF value
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IFACE_RSSI_BEACON: RSSI of beacon
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IFACE_SNR_BEACON: SNR of beacon
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_REPORT_TIME: u64
 *    Absolute timestamp from 1970/1/1, unit in ms. After receiving the
 *    message, user layer APP could call gettimeofday to get another
 *    timestamp and calculate transfer delay for the message.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_MEASUREMENT_TIME: u32
 *    Real period for this measurement, unit in us.
 * @QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PAD: Attribute used for padding for
 *    64-bit alignment.
 */
enum qca_wlan_vendor_attr_ll_stats_ext {
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_INVALID = 0,

	/* Attributes for configurations */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CFG_PERIOD,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CFG_THRESHOLD,

	/* Peer STA power state change */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_CHG,

	/* TX failure event */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TID,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_NUM_MSDU,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_STATUS,

	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_STATE,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_MAC_ADDRESS,

	/* MAC counters */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_GLOBAL,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_EVENT_MODE,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IFACE_ID,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_ID,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_BITMAP,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_BITMAP,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_CCA_BSS_BITMAP,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_SIGNAL_BITMAP,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_NUM,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_CHANNEL_NUM,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_CCA_BSS,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER,

	/* Sub-attributes for PEER_AC_TX */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_MSDU,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_MPDU,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_PPDU,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_BYTES,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_DROP,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_DROP_BYTES,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_RETRY,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_NO_ACK,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_NO_BACK,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_AGGR_NUM,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_SUCC_MCS_NUM,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_FAIL_MCS_NUM,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_AGGR,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_SUCC_MCS,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_FAIL_MCS,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_DELAY_ARRAY_SIZE,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_DELAY,

	/* Sub-attributes for PEER_AC_RX */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_BYTES,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_PPDU,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_PPDU_BYTES,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_LOST,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_RETRY,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_DUP,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_DISCARD,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_AGGR_NUM,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MCS_NUM,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MCS,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_AGGR,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_TIMES,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_DURATION,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_PROBE_REQ,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MGMT,

	/* Sub-attributes for CCA_BSS */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IDLE_TIME,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_TIME,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_TIME,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_BUSY,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_BAD,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_BAD,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_NO_AVAIL,

	/* sub-attribute for BSS_RX_TIME */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IN_BSS_TIME,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_OUT_BSS_TIME,

	/* Sub-attributes for PEER_SIGNAL */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_ANT_NUM,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_SIGNAL,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_ANT_SNR,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_ANT_NF,

	/* Sub-attributes for IFACE_BSS */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IFACE_RSSI_BEACON,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IFACE_SNR_BEACON,

	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_REPORT_TIME,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_MEASUREMENT_TIME,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PAD,

	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_LAST,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_MAX =
		QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_LAST - 1
};

/* Attributes for FTM commands and events */

/**
 * enum qca_wlan_vendor_attr_loc_capa - Indoor location capabilities
 *
 * @QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAGS: Various flags. See
 *	enum qca_wlan_vendor_attr_loc_capa_flags.
 * @QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_NUM_SESSIONS: Maximum number
 *	of measurement sessions that can run concurrently.
 *	Default is one session (no session concurrency).
 * @QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_NUM_PEERS: The total number of unique
 *	peers that are supported in running sessions. For example,
 *	if the value is 8 and maximum number of sessions is 2, you can
 *	have one session with 8 unique peers, or 2 sessions with 4 unique
 *	peers each, and so on.
 * @QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_NUM_BURSTS_EXP: Maximum number
 *	of bursts per peer, as an exponent (2^value). Default is 0,
 *	meaning no multi-burst support.
 * @QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_MEAS_PER_BURST: Maximum number
 *	of measurement exchanges allowed in a single burst.
 * @QCA_WLAN_VENDOR_ATTR_AOA_CAPA_SUPPORTED_TYPES: Supported AOA measurement
 *	types. A bit mask (unsigned 32 bit value), each bit corresponds
 *	to an AOA type as defined by enum qca_vendor_attr_aoa_type.
 */
enum qca_wlan_vendor_attr_loc_capa {
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_INVALID,
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAGS,
	QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_NUM_SESSIONS,
	QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_NUM_PEERS,
	QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_NUM_BURSTS_EXP,
	QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_MEAS_PER_BURST,
	QCA_WLAN_VENDOR_ATTR_AOA_CAPA_SUPPORTED_TYPES,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_MAX =
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_loc_capa_flags: Indoor location capability flags
 *
 * @QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_FTM_RESPONDER: Set if driver
 *	can be configured as an FTM responder (for example, an AP that
 *	services FTM requests). QCA_NL80211_VENDOR_SUBCMD_FTM_CFG_RESPONDER
 *	will be supported if set.
 * @QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_FTM_INITIATOR: Set if driver
 *	can run FTM sessions. QCA_NL80211_VENDOR_SUBCMD_FTM_START_SESSION
 *	will be supported if set.
 * @QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_ASAP: Set if FTM responder
 *	supports immediate (ASAP) response.
 * @QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_AOA: Set if driver supports standalone
 *	AOA measurement using QCA_NL80211_VENDOR_SUBCMD_AOA_MEAS.
 * @QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_AOA_IN_FTM: Set if driver supports
 *	requesting AOA measurements as part of an FTM session.
 */
enum qca_wlan_vendor_attr_loc_capa_flags {
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_FTM_RESPONDER = 1 << 0,
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_FTM_INITIATOR = 1 << 1,
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_ASAP = 1 << 2,
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_AOA = 1 << 3,
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_AOA_IN_FTM = 1 << 4,
};

/**
 * enum qca_wlan_vendor_attr_ftm_peer_info: Information about
 *	a single peer in a measurement session.
 *
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_MAC_ADDR: The MAC address of the peer.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAGS: Various flags related
 *	to measurement. See enum qca_wlan_vendor_attr_ftm_peer_meas_flags.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_PARAMS: Nested attribute of
 *	FTM measurement parameters, as specified by IEEE P802.11-REVmc/D7.0
 *	9.4.2.167. See enum qca_wlan_vendor_attr_ftm_meas_param for
 *	list of supported attributes.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_SECURE_TOKEN_ID: Initial token ID for
 *	secure measurement.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_AOA_BURST_PERIOD: Request AOA
 *	measurement every <value> bursts. If 0 or not specified,
 *	AOA measurements will be disabled for this peer.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_FREQ: Frequency in MHz where
 *	the measurement frames are exchanged. Optional; if not
 *	specified, try to locate the peer in the kernel scan
 *	results cache and use frequency from there.
 */
enum qca_wlan_vendor_attr_ftm_peer_info {
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_INVALID,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_MAC_ADDR,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAGS,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_PARAMS,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_SECURE_TOKEN_ID,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_AOA_BURST_PERIOD,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_FREQ,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_MAX =
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_ftm_peer_meas_flags: Measurement request flags,
 *	per-peer
 *
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_ASAP: If set, request
 *	immediate (ASAP) response from peer.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_LCI: If set, request
 *	LCI report from peer. The LCI report includes the absolute
 *	location of the peer in "official" coordinates (similar to GPS).
 *	See IEEE P802.11-REVmc/D7.0, 11.24.6.7 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_LCR: If set, request
 *	Location civic report from peer. The LCR includes the location
 *	of the peer in free-form format. See IEEE P802.11-REVmc/D7.0,
 *	11.24.6.7 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_SECURE: If set,
 *	request a secure measurement.
 *	QCA_WLAN_VENDOR_ATTR_FTM_PEER_SECURE_TOKEN_ID must also be provided.
 */
enum qca_wlan_vendor_attr_ftm_peer_meas_flags {
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_ASAP	= 1 << 0,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_LCI	= 1 << 1,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_LCR	= 1 << 2,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_SECURE	= 1 << 3,
};

/**
 * enum qca_wlan_vendor_attr_ftm_meas_param: Measurement parameters
 *
 * @QCA_WLAN_VENDOR_ATTR_FTM_PARAM_MEAS_PER_BURST: Number of measurements
 *	to perform in a single burst.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PARAM_NUM_BURSTS_EXP: Number of bursts to
 *	perform, specified as an exponent (2^value).
 * @QCA_WLAN_VENDOR_ATTR_FTM_PARAM_BURST_DURATION: Duration of burst
 *	instance, as specified in IEEE P802.11-REVmc/D7.0, 9.4.2.167.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PARAM_BURST_PERIOD: Time between bursts,
 *	as specified in IEEE P802.11-REVmc/D7.0, 9.4.2.167. Must
 *	be larger than QCA_WLAN_VENDOR_ATTR_FTM_PARAM_BURST_DURATION.
 */
enum qca_wlan_vendor_attr_ftm_meas_param {
	QCA_WLAN_VENDOR_ATTR_FTM_PARAM_INVALID,
	QCA_WLAN_VENDOR_ATTR_FTM_PARAM_MEAS_PER_BURST,
	QCA_WLAN_VENDOR_ATTR_FTM_PARAM_NUM_BURSTS_EXP,
	QCA_WLAN_VENDOR_ATTR_FTM_PARAM_BURST_DURATION,
	QCA_WLAN_VENDOR_ATTR_FTM_PARAM_BURST_PERIOD,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_FTM_PARAM_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_FTM_PARAM_MAX =
	QCA_WLAN_VENDOR_ATTR_FTM_PARAM_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_ftm_peer_result: Per-peer results
 *
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MAC_ADDR: MAC address of the reported
 *	 peer.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS: Status of measurement
 *	request for this peer.
 *	See enum qca_wlan_vendor_attr_ftm_peer_result_status.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_FLAGS: Various flags related
 *	to measurement results for this peer.
 *	See enum qca_wlan_vendor_attr_ftm_peer_result_flags.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_VALUE_SECONDS: Specified when
 *	request failed and peer requested not to send an additional request
 *	for this number of seconds.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_LCI: LCI report when received
 *	from peer. In the format specified by IEEE P802.11-REVmc/D7.0,
 *	9.4.2.22.10.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_LCR: Location civic report when
 *	received from peer. In the format specified by IEEE P802.11-REVmc/D7.0,
 *	9.4.2.22.13.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MEAS_PARAMS: Reported when peer
 *	overridden some measurement request parameters. See
 *	enum qca_wlan_vendor_attr_ftm_meas_param.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_AOA_MEAS: AOA measurement
 *	for this peer. Same contents as @QCA_WLAN_VENDOR_ATTR_AOA_MEAS_RESULT.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MEAS: Array of measurement
 *	results. Each entry is a nested attribute defined
 *	by enum qca_wlan_vendor_attr_ftm_meas.
 */
enum qca_wlan_vendor_attr_ftm_peer_result {
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_INVALID,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MAC_ADDR,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_FLAGS,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_VALUE_SECONDS,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_LCI,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_LCR,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MEAS_PARAMS,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_AOA_MEAS,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MEAS,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MAX =
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_ftm_peer_result_status
 *
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_OK: Request sent ok and results
 *	will be provided. Peer may have overridden some measurement parameters,
 *	in which case overridden parameters will be report by
 *	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MEAS_PARAM attribute.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_INCAPABLE: Peer is incapable
 *	of performing the measurement request. No more results will be sent
 *	for this peer in this session.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_FAILED: Peer reported request
 *	failed, and requested not to send an additional request for number
 *	of seconds specified by QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_VALUE_SECONDS
 *	attribute.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_INVALID: Request validation
 *	failed. Request was not sent over the air.
 */
enum qca_wlan_vendor_attr_ftm_peer_result_status {
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_OK,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_INCAPABLE,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_FAILED,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_INVALID,
};

/**
 * enum qca_wlan_vendor_attr_ftm_peer_result_flags: Various flags
 *  for measurement result, per-peer
 *
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_FLAG_DONE: If set,
 *	measurement completed for this peer. No more results will be reported
 *	for this peer in this session.
 */
enum qca_wlan_vendor_attr_ftm_peer_result_flags {
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_FLAG_DONE = 1 << 0,
};

/**
 * enum qca_vendor_attr_loc_session_status: Session completion status code
 *
 * @QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_OK: Session completed
 *	successfully.
 * @QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_ABORTED: Session aborted
 *	by request.
 * @QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_INVALID: Session request
 *	was invalid and was not started.
 * @QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_FAILED: Session had an error
 *	and did not complete normally (for example out of resources).
 */
enum qca_vendor_attr_loc_session_status {
	QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_OK,
	QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_ABORTED,
	QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_INVALID,
	QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_FAILED,
};

/**
 * enum qca_wlan_vendor_attr_ftm_meas: Single measurement data
 *
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T1: Time of departure (TOD) of FTM packet as
 *	recorded by responder, in picoseconds.
 *	See IEEE P802.11-REVmc/D7.0, 11.24.6.4 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T2: Time of arrival (TOA) of FTM packet at
 *	initiator, in picoseconds.
 *	See IEEE P802.11-REVmc/D7.0, 11.24.6.4 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T3: TOD of ACK packet as recorded by
 *	initiator, in picoseconds.
 *	See IEEE P802.11-REVmc/D7.0, 11.24.6.4 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T4: TOA of ACK packet at
 *	responder, in picoseconds.
 *	See IEEE P802.11-REVmc/D7.0, 11.24.6.4 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_RSSI: RSSI (signal level) as recorded
 *	during this measurement exchange. Optional and will be provided if
 *	the hardware can measure it.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_TOD_ERR: TOD error reported by
 *	responder. Not always provided.
 *	See IEEE P802.11-REVmc/D7.0, 9.6.8.33 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_TOA_ERR: TOA error reported by
 *	responder. Not always provided.
 *	See IEEE P802.11-REVmc/D7.0, 9.6.8.33 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_INITIATOR_TOD_ERR: TOD error measured by
 *	initiator. Not always provided.
 *	See IEEE P802.11-REVmc/D7.0, 9.6.8.33 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_INITIATOR_TOA_ERR: TOA error measured by
 *	initiator. Not always provided.
 *	See IEEE P802.11-REVmc/D7.0, 9.6.8.33 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PAD: Attribute used for padding for 64-bit
 *	alignment.
 */
enum qca_wlan_vendor_attr_ftm_meas {
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_INVALID,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T1,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T2,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T3,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T4,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_RSSI,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_TOD_ERR,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_TOA_ERR,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_INITIATOR_TOD_ERR,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_INITIATOR_TOA_ERR,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PAD,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_MAX =
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_aoa_type - AOA measurement type
 *
 * @QCA_WLAN_VENDOR_ATTR_AOA_TYPE_TOP_CIR_PHASE: Phase of the strongest
 *	CIR (channel impulse response) path for each antenna.
 * @QCA_WLAN_VENDOR_ATTR_AOA_TYPE_TOP_CIR_PHASE_AMP: Phase and amplitude
 *	of the strongest CIR path for each antenna.
 */
enum qca_wlan_vendor_attr_aoa_type {
	QCA_WLAN_VENDOR_ATTR_AOA_TYPE_TOP_CIR_PHASE,
	QCA_WLAN_VENDOR_ATTR_AOA_TYPE_TOP_CIR_PHASE_AMP,
	QCA_WLAN_VENDOR_ATTR_AOA_TYPE_MAX
};

/**
 * enum qca_wlan_vendor_attr_encryption_test - Attributes to
 * validate encryption engine
 *
 * @QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_NEEDS_DECRYPTION: Flag attribute.
 *	This will be included if the request is for decryption; if not included,
 *	the request is treated as a request for encryption by default.
 * @QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_CIPHER: Unsigned 32-bit value
 *	indicating the key cipher suite. Takes same values as
 *	NL80211_ATTR_KEY_CIPHER.
 * @QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_KEYID: Unsigned 8-bit value
 *	Key Id to be used for encryption
 * @QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_TK: Array of 8-bit values.
 *	Key (TK) to be used for encryption/decryption
 * @QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_PN: Array of 8-bit values.
 *	Packet number to be specified for encryption/decryption
 *	6 bytes for TKIP/CCMP/GCMP.
 * @QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_DATA: Array of 8-bit values
 *	representing the 802.11 packet (header + payload + FCS) that
 *	needs to be encrypted/decrypted.
 *	Encrypted/decrypted response from the driver will also be sent
 *	to userspace with the same attribute.
 */
enum qca_wlan_vendor_attr_encryption_test {
	QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_NEEDS_DECRYPTION,
	QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_CIPHER,
	QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_KEYID,
	QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_TK,
	QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_PN,
	QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_DATA,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_MAX =
	QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_AFTER_LAST - 1
};

/**
 * enum qca_wlan_vendor_attr_dmg_rf_sector_type - Type of
 * sector for DMG RF sector operations.
 *
 * @QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE_RX: RX sector
 * @QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE_TX: TX sector
 */
enum qca_wlan_vendor_attr_dmg_rf_sector_type {
	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE_RX,
	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE_TX,
	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE_MAX
};

/**
 * enum qca_wlan_vendor_attr_fw_state - State of firmware
 *
 * @QCA_WLAN_VENDOR_ATTR_FW_STATE_ERROR: FW is in bad state
 * @QCA_WLAN_VENDOR_ATTR_FW_STATE_ACTIVE: FW is active
 */
enum qca_wlan_vendor_attr_fw_state {
	QCA_WLAN_VENDOR_ATTR_FW_STATE_ERROR,
	QCA_WLAN_VENDOR_ATTR_FW_STATE_ACTIVE,
	QCA_WLAN_VENDOR_ATTR_FW_STATE_MAX
};

/**
 * BRP antenna limit mode
 *
 * @QCA_WLAN_VENDOR_ATTR_BRP_ANT_LIMIT_MODE_DISABLE: Disable BRP force
 *	antenna limit, BRP will be performed as usual.
 * @QCA_WLAN_VENDOR_ATTR_BRP_ANT_LIMIT_MODE_EFFECTIVE: Define maximal
 *	antennas limit. the hardware may use less antennas than the
 *	maximum limit.
 * @QCA_WLAN_VENDOR_ATTR_BRP_ANT_LIMIT_MODE_FORCE: The hardware will
 *	use exactly the specified number of antennas for BRP.
 */
enum qca_wlan_vendor_attr_brp_ant_limit_mode {
	QCA_WLAN_VENDOR_ATTR_BRP_ANT_LIMIT_MODE_DISABLE,
	QCA_WLAN_VENDOR_ATTR_BRP_ANT_LIMIT_MODE_EFFECTIVE,
	QCA_WLAN_VENDOR_ATTR_BRP_ANT_LIMIT_MODE_FORCE,
	QCA_WLAN_VENDOR_ATTR_BRP_ANT_LIMIT_MODE_MAX
};

/**
 * enum qca_wlan_vendor_attr_dmg_rf_sector_cfg - Attributes for
 * DMG RF sector configuration for a single RF module.
 * The values are defined in a compact way which closely matches
 * the way it is stored in HW registers.
 * The configuration provides values for 32 antennas and 8 distribution
 * amplifiers, and together describes the characteristics of the RF
 * sector - such as a beam in some direction with some gain.
 *
 * @QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_MODULE_INDEX: Index
 *	of RF module for this configuration.
 * @QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_ETYPE0: Bit 0 of edge
 *	amplifier gain index. Unsigned 32 bit number containing
 *	bits for all 32 antennas.
 * @QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_ETYPE1: Bit 1 of edge
 *	amplifier gain index. Unsigned 32 bit number containing
 *	bits for all 32 antennas.
 * @QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_ETYPE2: Bit 2 of edge
 *	amplifier gain index. Unsigned 32 bit number containing
 *	bits for all 32 antennas.
 * @QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_PSH_HI: Phase values
 *	for first 16 antennas, 2 bits per antenna.
 * @QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_PSH_LO: Phase values
 *	for last 16 antennas, 2 bits per antenna.
 * @QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_DTYPE_X16: Contains
 *	DTYPE values (3 bits) for each distribution amplifier, followed
 *	by X16 switch bits for each distribution amplifier. There are
 *	total of 8 distribution amplifiers.
 */
enum qca_wlan_vendor_attr_dmg_rf_sector_cfg {
	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_MODULE_INDEX = 1,
	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_ETYPE0 = 2,
	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_ETYPE1 = 3,
	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_ETYPE2 = 4,
	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_PSH_HI = 5,
	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_PSH_LO = 6,
	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_DTYPE_X16 = 7,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_MAX =
	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_AFTER_LAST - 1
};

enum qca_wlan_vendor_attr_ll_stats_set {
	QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_INVALID = 0,
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_MPDU_SIZE_THRESHOLD = 1,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_AGGRESSIVE_STATS_GATHERING = 2,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_MAX =
	QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_AFTER_LAST - 1,
};

/**
 * qca_wlan_ll_stats_clr_req_bitmap - Represents the bitmap to clear LL STATS
 * values for %QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_REQ_MASK.
 *
 * @QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_RADIO: Clear all radio statistics.
 *
 * @QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_RADIO_CCA: Clear cca_busy_time within
 * radio statistics.
 *
 * @QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_RADIO_CHANNEL: Clear all channel
 * statistics within radio statistics.
 *
 * @QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_SCAN: Clear all scan statistics within
 * radio statistics.
 *
 * @QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_IFACE: Clear all interface statistics.
 *
 * @QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_IFACE_TXRATE: Clear all TX rate statistics
 * within interface statistics.
 *
 * @QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_IFACE_AC: Clear all AC statistics within
 * interface statistics.
 *
 * @QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_IFACE_CONTENTION: Clear all contention
 * (min, max, avg) statistics within AC statistics.
 *
 * @QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_IFACE_ALL_PEER: Clear all peer statistics
 * on this interface.
 *
 * @QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_IFACE_PER_PEER: Clear particular peer
 * statistics depending on the peer_mac.
 */
enum qca_wlan_ll_stats_clr_req_bitmap {
	QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_RADIO =		BIT(0),
	QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_RADIO_CCA =		BIT(1),
	QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_RADIO_CHANNELS =	BIT(2),
	QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_RADIO_SCAN =		BIT(3),
	QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_IFACE =		BIT(4),
	QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_IFACE_TXRATE =		BIT(5),
	QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_IFACE_AC =		BIT(6),
	QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_IFACE_CONTENTION =	BIT(7),
	QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_IFACE_ALL_PEER =	BIT(8),
	QCA_WLAN_LL_STATS_CLR_REQ_BITMAP_IFACE_PER_PEER =	BIT(9),
};

enum qca_wlan_vendor_attr_ll_stats_clr {
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_INVALID = 0,
	/* Unsigned 32bit bitmap for clearing statistics, specified
	 * in the enum qca_wlan_ll_stats_clr_req_bitmap.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_REQ_MASK = 1,
	/* Unsigned 8 bit value: Request to stop statistics collection */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_REQ = 2,

	/* Unsigned 32 bit bitmap: Response from the driver
	 * for the cleared statistics
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_RSP_MASK = 3,
	/* Unsigned 8 bit value: Response from driver/firmware
	 * for the stop request
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_RSP = 4,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_MAX =
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_AFTER_LAST - 1,
};

/**
 * qca_wlan_ll_stats_get_req_bitmap - Represents the bitmap to request LL STATS
 * values for %QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_MASK.
 *
 * @QCA_WLAN_LL_STATS_GET_REQ_BITMAP_RADIO: Request all radio statistics.
 *
 * @QCA_WLAN_LL_STATS_GET_REQ_BITMAP_IFACE: Request interface statistics.
 *
 * @QCA_WLAN_LL_STATS_GET_REQ_BITMAP_ALL_PEER: Request all peer statistics.
 *
 * @QCA_WLAN_LL_STATS_GET_REQ_BITMAP_PER_PEER: Request per peer statistics.
 */
enum qca_wlan_ll_stats_get_req_bitmap {
	QCA_WLAN_LL_STATS_GET_REQ_BITMAP_RADIO =	BIT(0),
	QCA_WLAN_LL_STATS_GET_REQ_BITMAP_IFACE =	BIT(1),
	QCA_WLAN_LL_STATS_GET_REQ_BITMAP_ALL_PEER =	BIT(2),
	QCA_WLAN_LL_STATS_GET_REQ_BITMAP_PER_PEER =	BIT(3),
};

enum qca_wlan_vendor_attr_ll_stats_get {
	QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_INVALID = 0,
	/* Unsigned 32 bit value provided by the caller issuing the GET stats
	 * command. When reporting the stats results, the driver uses the same
	 * value to indicate which GET request the results correspond to.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_ID = 1,
	/* Unsigned 32 bit value - bit mask to identify what statistics are
	 * requested for retrieval specified in the enum
	 * qca_wlan_ll_stats_get_req_bitmap
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_MASK = 2,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_MAX =
	QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_ll_stats_results {
	QCA_WLAN_VENDOR_ATTR_LL_STATS_INVALID = 0,

	/*
	 * For Multi Link Operation (MLO) connection, per-link statistics will
	 * be sent inside of %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and
	 * cumulative statistics will be sent outside of
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK to maintain backward
	 * compatibility with legacy user space. Attributes which don't have
	 * explicit documentation for MLO will be sent only outside of
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK, such attributes values
	 * don't depend on whether the connection is MLO capable or not, e.g.,
	 * radio and channel specific attributes.
	 */

	/* Unsigned 32bit value. Used by the driver; must match the request id
	 * provided with the QCA_NL80211_VENDOR_SUBCMD_LL_STATS_GET command.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RESULTS_REQ_ID = 1,

	/* Unsigned 32 bit value. For an MLO connection, indicates the value of
	 * the link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link specific value
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_BEACON_RX = 2,
	/* Unsigned 32 bit value. For an MLO connection, indicates the value of
	 * the link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link specific value
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_RX = 3,
	/* Unsigned 32 bit value. For an MLO connection, indicates the value of
	 * the link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link specific value
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_RX = 4,
	/* Unsigned 32 bit value. For an MLO connection, indicates the value of
	 * the link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link specific value
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_TX = 5,
	/* Unsigned 32 bit value. For an MLO connection, indicates the value of
	 * the link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link specific value
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_MGMT = 6,
	/* Unsigned 32 bit value. For an MLO connection, indicates the value of
	 * the link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link specific value
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_DATA = 7,
	/* Unsigned 32 bit value. For an MLO connection, indicates the value of
	 * the link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link specific value
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_ACK = 8,

	/* Attributes of type QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_* are
	 * nested within the interface stats.
	 */

	/* Interface mode, e.g., STA, SOFTAP, IBSS, etc.
	 * Type = enum wifi_interface_mode.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_MODE = 9,
	/* Interface MAC address. An array of 6 Unsigned int8 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_MAC_ADDR = 10,
	/* Type = enum wifi_connection_state, e.g., DISCONNECTED,
	 * AUTHENTICATING, etc. valid for STA, CLI only.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_STATE = 11,
	/* Type = enum wifi_roam_state. Roaming state, e.g., IDLE or ACTIVE
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_ROAMING = 12,
	/* Unsigned 32 bit value. WIFI_CAPABILITY_XXX */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_CAPABILITIES = 13,
	/* NULL terminated SSID. An array of 33 Unsigned 8bit values */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_SSID = 14,
	/* For non-MLO connection, BSSID of the AP. For MLO connection, MLD
	 * address of the AP. An array of 6 unsigned 8 bit values */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_BSSID = 15,
	/* Country string advertised by AP. An array of 3 unsigned 8 bit
	 * values.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_AP_COUNTRY_STR = 16,
	/* Country string for this association. An array of 3 unsigned 8 bit
	 * values.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_COUNTRY_STR = 17,

	/* Attributes of type QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_* could
	 * be nested within the interface stats.
	 * For an MLO connection, all %QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_*
	 * attributes except %QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_*
	 * indicate the aggregate of all links outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_* attributes
	 * indicate value of the MLO link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 * These attributes indicate the link specific value inside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */

	/* Type = enum wifi_traffic_ac, e.g., V0, VI, BE and BK */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_AC = 18,
	/* Unsigned int 32 value corresponding to respective AC */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_MPDU = 19,
	/* Unsigned int 32 value corresponding to respective AC */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_MPDU = 20,
	/* Unsigned int 32 value corresponding to respective AC */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_MCAST = 21,
	/* Unsigned int 32 value corresponding to respective AC */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_MCAST = 22,
	/* Unsigned int 32 value corresponding to respective AC */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_AMPDU = 23,
	/* Unsigned int 32 value corresponding to respective AC */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_AMPDU = 24,
	/* Unsigned int 32 value corresponding to respective AC */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_MPDU_LOST = 25,
	/* Unsigned int 32 value corresponding to respective AC */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES = 26,
	/* Unsigned int 32 value corresponding to respective AC  */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES_SHORT = 27,
	/* Unsigned int 32 values corresponding to respective AC */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES_LONG = 28,
	/* Unsigned int 32 values corresponding to respective AC */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_MIN = 29,
	/* Unsigned int 32 values corresponding to respective AC */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_MAX = 30,
	/* Unsigned int 32 values corresponding to respective AC */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_AVG = 31,
	/* Unsigned int 32 values corresponding to respective AC */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_NUM_SAMPLES = 32,
	/* Unsigned 32 bit value. Number of peers */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_NUM_PEERS = 33,

	/* Attributes of type QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_* are
	 * nested within the interface stats.
	 */

	/* Type = enum wifi_peer_type. Peer type, e.g., STA, AP, P2P GO etc. */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_TYPE = 34,
	/* MAC addr corresponding to respective peer. An array of 6 unsigned
	 * 8 bit values.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_MAC_ADDRESS = 35,
	/* Unsigned int 32 bit value representing capabilities corresponding
	 * to respective peer.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_CAPABILITIES = 36,
	/* Unsigned 32 bit value. Number of rates */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_NUM_RATES = 37,

	/* Attributes of type QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_*
	 * are nested within the rate stat.
	 */

	/* Wi-Fi Rate - separate attributes defined for individual fields */

	/* Unsigned int 8 bit value; 0: OFDM, 1:CCK, 2:HT 3:VHT 4..7 reserved */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_PREAMBLE = 38,
	/* Unsigned int 8 bit value; 0:1x1, 1:2x2, 3:3x3, 4:4x4 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_NSS = 39,
	/* Unsigned int 8 bit value; 0:20 MHz, 1:40 MHz, 2:80 MHz, 3:160 MHz */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_BW = 40,
	/* Unsigned int 8 bit value; OFDM/CCK rate code would be as per IEEE Std
	 * in the units of 0.5 Mbps HT/VHT it would be MCS index
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_MCS_INDEX = 41,

	/* Unsigned 32 bit value. Bit rate in units of 100 kbps */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_BIT_RATE = 42,

	/* Attributes of type QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_STAT_* could be
	 * nested within the peer info stats.
	 */

	/* Unsigned int 32 bit value. Number of successfully transmitted data
	 * packets, i.e., with ACK received corresponding to the respective
	 * rate.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_TX_MPDU = 43,
	/* Unsigned int 32 bit value. Number of received data packets
	 * corresponding to the respective rate.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RX_MPDU = 44,
	/* Unsigned int 32 bit value. Number of data packet losses, i.e., no ACK
	 * received corresponding to the respective rate.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_MPDU_LOST = 45,
	/* Unsigned int 32 bit value. Total number of data packet retries for
	 * the respective rate.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES = 46,
	/* Unsigned int 32 bit value. Total number of short data packet retries
	 * for the respective rate.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES_SHORT = 47,
	/* Unsigned int 32 bit value. Total number of long data packet retries
	 * for the respective rate.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES_LONG = 48,

	/* Unsigned 32 bit value. This is used to indicate radio ID of the radio
	 * statistics when %QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE is
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE_RADIO. This is also used
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK to indicate radio ID
	 * of the MLO link.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ID = 49,
	/* Unsigned 32 bit value. Total number of msecs the radio is awake
	 * accruing over time.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME = 50,
	/* Unsigned 32 bit value. Total number of msecs the radio is
	 * transmitting accruing over time.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_TX_TIME = 51,
	/* Unsigned 32 bit value. Total number of msecs the radio is in active
	 * receive accruing over time.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_RX_TIME = 52,
	/* Unsigned 32 bit value. Total number of msecs the radio is awake due
	 * to all scan accruing over time.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_SCAN = 53,
	/* Unsigned 32 bit value. Total number of msecs the radio is awake due
	 * to NAN accruing over time.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_NBD = 54,
	/* Unsigned 32 bit value. Total number of msecs the radio is awake due
	 * to GSCAN accruing over time.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_GSCAN = 55,
	/* Unsigned 32 bit value. Total number of msecs the radio is awake due
	 * to roam scan accruing over time.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_ROAM_SCAN = 56,
	/* Unsigned 32 bit value. Total number of msecs the radio is awake due
	 * to PNO scan accruing over time.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_PNO_SCAN = 57,
	/* Unsigned 32 bit value. Total number of msecs the radio is awake due
	 * to Hotspot 2.0 scans and GAS exchange accruing over time.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_HS20 = 58,
	/* Unsigned 32 bit value. Number of channels. */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_NUM_CHANNELS = 59,

	/* Attributes of type QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_* could
	 * be nested within the channel stats.
	 */

	/* Type = enum wifi_channel_width. Channel width, e.g., 20, 40, 80 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_WIDTH = 60,
	/* Unsigned 32 bit value. Primary 20 MHz channel. This is used to
	 * indicate the primary frequency of the channel when
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE is
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE_RADIO. This is also used inside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK to indicate the frequency
	 * on which the MLO link is operating.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ = 61,
	/* Unsigned 32 bit value. Center frequency (MHz) first segment. */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ0 = 62,
	/* Unsigned 32 bit value. Center frequency (MHz) second segment. */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ1 = 63,

	/* Attributes of type QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_* could be
	 * nested within the radio stats.
	 */

	/* Unsigned int 32 bit value representing total number of msecs the
	 * radio is awake on that channel accruing over time, corresponding to
	 * the respective channel.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_ON_TIME = 64,
	/* Unsigned int 32 bit value representing total number of msecs the CCA
	 * register is busy accruing over time corresponding to the respective
	 * channel.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_CCA_BUSY_TIME = 65,

	QCA_WLAN_VENDOR_ATTR_LL_STATS_NUM_RADIOS = 66,

	/* Signifies the nested list of channel attributes
	 * QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_*
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CH_INFO = 67,

	/* Signifies the nested list of peer info attributes
	 * QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_*. For MLO connection,
	 * this also contains %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK_ID to
	 * indicate on which link the peer is connected.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO = 68,

	/* Signifies the nested list of rate info attributes
	 * QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_*
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_RATE_INFO = 69,

	/* Signifies the nested list of wmm info attributes
	 * QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_*
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_INFO = 70,

	/* Unsigned 8 bit value. Used by the driver; if set to 1, it indicates
	 * that more stats, e.g., peers or radio, are to follow in the next
	 * QCA_NL80211_VENDOR_SUBCMD_LL_STATS_*_RESULTS event.
	 * Otherwise, it is set to 0.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RESULTS_MORE_DATA = 71,

	/* Unsigned 64 bit value. For an MLO connection, indicates the value of
	 * the link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link specific value
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_AVERAGE_TSF_OFFSET = 72,

	/* Unsigned 32 bit value. For an MLO connection, indicates the value of
	 * the link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link specific value
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_DETECTED = 73,

	/* Unsigned 32 bit value. For an MLO connection, indicates the value of
	 * the link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link specific value
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_AVG_NUM_FRAMES_LEAKED = 74,

	/* Unsigned 32 bit value. For an MLO connection, indicates the value of
	 * the link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link specific value
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_GUARD_TIME = 75,

	/* Unsigned 32 bit value */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE = 76,

	/* Unsigned 32 bit value */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_NUM_TX_LEVELS = 77,

	/* Number of msecs the radio spent in transmitting for each power level
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_TX_TIME_PER_LEVEL = 78,

	/* Unsigned 32 bit value. For an MLO connection, indicates the value of
	 * the link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link specific value
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RTS_SUCC_CNT = 79,
	/* Unsigned 32 bit value. For an MLO connection, indicates the value of
	 * the link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link specific value
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RTS_FAIL_CNT = 80,
	/* Unsigned 32 bit value. For an MLO connection, indicates the value of
	 * the link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link specific value
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_PPDU_SUCC_CNT = 81,
	/* Unsigned 32 bit value. For an MLO connection, indicates the value of
	 * the link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link specific value
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_PPDU_FAIL_CNT = 82,

	/* Unsigned int 32 value.
	 * Pending MSDUs corresponding to respective AC.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_PENDING_MSDU = 83,

	/* u32 value representing total time in milliseconds for which the radio
	 * is transmitting on this channel. This attribute will be nested
	 * within QCA_WLAN_VENDOR_ATTR_LL_STATS_CH_INFO.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_TX_TIME = 84,
	/* u32 value representing total time in milliseconds for which the radio
	 * is receiving all 802.11 frames intended for this device on this
	 * channel. This attribute will be nested within
	 * QCA_WLAN_VENDOR_ATTR_LL_STATS_CH_INFO.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_RX_TIME = 85,
	/* u8 value representing the channel load percentage. Possible values
	 * are 0-100.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_LOAD_PERCENTAGE = 86,
	/* u8 value representing the time slicing duty cycle percentage.
	 * Possible values are 0-100. For an MLO connection, indicates the value
	 * of the link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link specific value
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_TS_DUTY_CYCLE = 87,
	/* Unsigned 32 bit value. The number of Beacon frames which are received
	 * from the associated AP and indicate buffered unicast frame(s) for us
	 * in the TIM element. For an MLO connection, indicates the value of the
	 * link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link specific value
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_TIM_BEACON = 88,
	/* Unsigned 32 bit value. The total number of Beacon frames received
	 * from the associated AP that have wrongly indicated buffered unicast
	 * traffic in the TIM element for us.
	 * Below scenarios will be considered as wrong TIM element beacon:
	 * 1)	The related TIM element is set in the beacon for STA but STA
	 *      doesn’t receive any unicast data after this beacon.
	 * 2)	The related TIM element is still set in the beacon for STA
	 *	after STA has indicated power save exit by QoS Null Data frame.
	 * For an MLO connection, indicates the value of the link with the best
	 * RSSI outside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link
	 * specific value inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_TIM_BEACON_ERR = 89,

	/* Signed 32 bit value. It represents the noise floor calibration value.
	 * Possible values are -120~-50 dBm. For an MLO connection, indicates
	 * the value of the link with the best RSSI outside
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK and the link specific value
	 * inside %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_NF_CAL_VAL = 90,

	/* Attribute used for padding for 64-bit alignment */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_PAD = 91,

	/* Unsigned u8 value, link ID of an MLO link. Used inside nested
	 * attribute %QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK to represent the
	 * link ID of the MLO link for which the statistics are embedded in the
	 * nested attribute. Used inside nested attribute
	 * %QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO to represent the connected
	 * link ID of the peer.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK_ID = 92,

	/* A nested array of attributes for each MLO link, each containing
	 * per-link statistics of a multi link connection. The attributes used
	 * inside this nested attribute are defined in enum
	 * qca_wlan_vendor_attr_ll_stats_results.
	 *
	 * For non-MLO connection, this attribute is not present and the
	 * statistics will be sent outside this attribute (without nesting).
	 *
	 * For MLO connection, this attribute is present and also cumulative
	 * statistics of all the links will be sent outside of this attribute
	 * to be compatible with legacy user space.
	 */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK = 93,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_LL_STATS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX =
	QCA_WLAN_VENDOR_ATTR_LL_STATS_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_ll_stats_type {
	QCA_NL80211_VENDOR_SUBCMD_LL_STATS_TYPE_INVALID = 0,
	QCA_NL80211_VENDOR_SUBCMD_LL_STATS_TYPE_RADIO = 1,
	QCA_NL80211_VENDOR_SUBCMD_LL_STATS_TYPE_IFACE = 2,
	QCA_NL80211_VENDOR_SUBCMD_LL_STATS_TYPE_PEERS = 3,

	/* keep last */
	QCA_NL80211_VENDOR_SUBCMD_LL_STATS_TYPE_AFTER_LAST,
	QCA_NL80211_VENDOR_SUBCMD_LL_STATS_TYPE_MAX =
	QCA_NL80211_VENDOR_SUBCMD_LL_STATS_TYPE_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_tdls_configuration - Attributes for
 * TDLS configuration to the host driver.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_TRIGGER_MODE: Configure the TDLS trigger
 *	mode in the host driver. enum qca_wlan_vendor_tdls_trigger_mode
 *	represents the different TDLS trigger modes.
 * @QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_TX_STATS_PERIOD: Duration (u32) within
 *      which QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_TX_THRESHOLD number
 *      of packets shall meet the criteria for implicit TDLS setup.
 * @QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_TX_THRESHOLD: Number (u32) of Tx/Rx packets
 *      within a duration QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_TX_STATS_PERIOD
 *      to initiate a TDLS setup.
 * @QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_DISCOVERY_PERIOD: Time (u32) to initiate
 *      a TDLS Discovery to the peer.
 * @QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_MAX_DISCOVERY_ATTEMPT: Max number (u32) of
 *      discovery attempts to know the TDLS capability of the peer. A peer is
 *      marked as TDLS not capable if there is no response for all the attempts.
 * @QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_IDLE_TIMEOUT: Represents a duration (u32)
 *      within which QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_IDLE_PACKET_THRESHOLD
 *      number of TX / RX frames meet the criteria for TDLS teardown.
 * @QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_IDLE_PACKET_THRESHOLD: Minimum number (u32)
 *      of Tx/Rx packets within a duration
 *      QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_IDLE_TIMEOUT to tear down a TDLS link.
 * @QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_SETUP_RSSI_THRESHOLD: Threshold
 *	corresponding to the RSSI of the peer below which a TDLS setup is
 *	triggered.
 * @QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_TEARDOWN_RSSI_THRESHOLD: Threshold
 *	corresponding to the RSSI of the peer above which a TDLS teardown is
 *	triggered.
 */
enum qca_wlan_vendor_attr_tdls_configuration {
	QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_TRIGGER_MODE = 1,

	/* Attributes configuring the TDLS Implicit Trigger */
	QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_TX_STATS_PERIOD = 2,
	QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_TX_THRESHOLD = 3,
	QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_DISCOVERY_PERIOD = 4,
	QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_MAX_DISCOVERY_ATTEMPT = 5,
	QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_IDLE_TIMEOUT = 6,
	QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_IDLE_PACKET_THRESHOLD = 7,
	QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_SETUP_RSSI_THRESHOLD = 8,
	QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_TEARDOWN_RSSI_THRESHOLD = 9,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_MAX =
	QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_AFTER_LAST - 1
};

/**
 * enum qca_wlan_vendor_tdls_trigger_mode: Represents the TDLS trigger mode in
 *	the driver
 *
 * The following are the different values for
 *	QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_TRIGGER_MODE.
 *
 * @QCA_WLAN_VENDOR_TDLS_TRIGGER_MODE_EXPLICIT: The trigger to initiate/teardown
 *	the TDLS connection to a respective peer comes from the user space.
 *	wpa_supplicant provides the commands TDLS_SETUP, TDLS_TEARDOWN,
 *	TDLS_DISCOVER to do this.
 * @QCA_WLAN_VENDOR_TDLS_TRIGGER_MODE_IMPLICIT: Host driver triggers this TDLS
 *	setup/teardown to the eligible peer once the configured criteria
 *	(such as TX/RX threshold, RSSI) is met. The attributes
 *	in QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_IMPLICIT_PARAMS correspond to
 *	the different configuration criteria for the TDLS trigger from the
 *	host driver.
 * @QCA_WLAN_VENDOR_TDLS_TRIGGER_MODE_EXTERNAL: Enables the driver to trigger
 *	the TDLS setup / teardown through the implicit mode only to the
 *	configured MAC addresses (wpa_supplicant, with tdls_external_control=1,
 *	configures the MAC address through TDLS_SETUP / TDLS_TEARDOWN commands).
 *	External mode works on top of the implicit mode. Thus the host driver
 *	is expected to configure in TDLS Implicit mode too to operate in
 *	External mode.
 *	Configuring External mode alone without	Implicit mode is invalid.
 *
 * All the above implementations work as expected only when the host driver
 * advertises the capability WPA_DRIVER_FLAGS_TDLS_EXTERNAL_SETUP - representing
 * that the TDLS message exchange is not internal to the host driver, but
 * depends on wpa_supplicant to do the message exchange.
 */
enum qca_wlan_vendor_tdls_trigger_mode {
	QCA_WLAN_VENDOR_TDLS_TRIGGER_MODE_EXPLICIT = 1 << 0,
	QCA_WLAN_VENDOR_TDLS_TRIGGER_MODE_IMPLICIT = 1 << 1,
	QCA_WLAN_VENDOR_TDLS_TRIGGER_MODE_EXTERNAL = 1 << 2,
};

/**
 * enum qca_vendor_attr_sar_limits_selections - Source of SAR power limits
 * @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT_BDF0: Select SAR profile #0
 *	that is hard-coded in the Board Data File (BDF).
 * @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT_BDF1: Select SAR profile #1
 *	that is hard-coded in the Board Data File (BDF).
 * @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT_BDF2: Select SAR profile #2
 *	that is hard-coded in the Board Data File (BDF).
 * @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT_BDF3: Select SAR profile #3
 *	that is hard-coded in the Board Data File (BDF).
 * @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT_BDF4: Select SAR profile #4
 *	that is hard-coded in the Board Data File (BDF).
 * @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT_NONE: Do not select any
 *	source of SAR power limits, thereby disabling the SAR power
 *	limit feature.
 * @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT_USER: Select the SAR power
 *	limits configured by %QCA_NL80211_VENDOR_SUBCMD_SET_SAR.
 * @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT_V2_0: Select the SAR power
 *	limits version 2.0 configured by %QCA_NL80211_VENDOR_SUBCMD_SET_SAR.
 *
 * This enumerates the valid set of values that may be supplied for
 * attribute %QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT in an instance of
 * the %QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS vendor command or in
 * the response to an instance of the
 * %QCA_NL80211_VENDOR_SUBCMD_GET_SAR_LIMITS vendor command.
 */
enum qca_vendor_attr_sar_limits_selections {
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT_BDF0 = 0,
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT_BDF1 = 1,
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT_BDF2 = 2,
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT_BDF3 = 3,
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT_BDF4 = 4,
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT_NONE = 5,
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT_USER = 6,
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT_V2_0 = 7,
};

/**
 * enum qca_vendor_attr_sar_limits_spec_modulations -
 *	SAR limits specification modulation
 * @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_MODULATION_CCK -
 *	CCK modulation
 * @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_MODULATION_OFDM -
 *	OFDM modulation
 *
 * This enumerates the valid set of values that may be supplied for
 * attribute %QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_MODULATION in an
 * instance of attribute %QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC in an
 * instance of the %QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS vendor
 * command or in the response to an instance of the
 * %QCA_NL80211_VENDOR_SUBCMD_GET_SAR_LIMITS vendor command.
 */
enum qca_vendor_attr_sar_limits_spec_modulations {
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_MODULATION_CCK = 0,
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_MODULATION_OFDM = 1,
};

/**
 * enum qca_vendor_attr_sar_limits - Attributes for SAR power limits
 *
 * @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SAR_ENABLE: Optional (u32) value to
 *	select which SAR power limit table should be used. Valid
 *	values are enumerated in enum
 *	%qca_vendor_attr_sar_limits_selections. The existing SAR
 *	power limit selection is unchanged if this attribute is not
 *	present.
 *
 * @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_NUM_SPECS: Optional (u32) value
 *	which specifies the number of SAR power limit specifications
 *	which will follow.
 *
 * @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC: Nested array of SAR power
 *	limit specifications. The number of specifications is
 *	specified by @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_NUM_SPECS. Each
 *	specification contains a set of
 *	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_* attributes. A
 *	specification is uniquely identified by the attributes
 *	%QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_BAND,
 *	%QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_CHAIN, and
 *	%QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_MODULATION and always
 *	contains as a payload the attribute
 *	%QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_POWER_LIMIT,
 *	%QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_POWER_LIMIT_INDEX.
 *	Either %QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_POWER_LIMIT or
 *	%QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_POWER_LIMIT_INDEX is
 *	needed based upon the value of
 *	%QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SAR_ENABLE.
 *
 * @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_BAND: Optional (u32) value to
 *	indicate for which band this specification applies. Valid
 *	values are enumerated in enum %nl80211_band (although not all
 *	bands may be supported by a given device). If the attribute is
 *	not supplied then the specification will be applied to all
 *	supported bands.
 *
 * @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_CHAIN: Optional (u32) value
 *	to indicate for which antenna chain this specification
 *	applies, i.e. 1 for chain 1, 2 for chain 2, etc. If the
 *	attribute is not supplied then the specification will be
 *	applied to all chains.
 *
 * @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_MODULATION: Optional (u32)
 *	value to indicate for which modulation scheme this
 *	specification applies. Valid values are enumerated in enum
 *	%qca_vendor_attr_sar_limits_spec_modulations. If the attribute
 *	is not supplied then the specification will be applied to all
 *	modulation schemes.
 *
 * @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_POWER_LIMIT: Required (u32)
 *	value to specify the actual power limit value in units of 0.5
 *	dBm (i.e., a value of 11 represents 5.5 dBm).
 *	This is required, when %QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT is
 *	%QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT_USER.
 *
 * @QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_POWER_LIMIT_INDEX: Required (u32)
 *	value to indicate SAR V2 indices (0 - 11) to select SAR V2 profiles.
 *	This is required, when %QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT is
 *	%QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SELECT_V2_0.
 *
 * These attributes are used with %QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS
 * and %QCA_NL80211_VENDOR_SUBCMD_GET_SAR_LIMITS.
 */
enum qca_vendor_attr_sar_limits {
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SAR_ENABLE = 1,
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_NUM_SPECS = 2,
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC = 3,
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_BAND = 4,
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_CHAIN = 5,
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_MODULATION = 6,
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_POWER_LIMIT = 7,
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_SPEC_POWER_LIMIT_INDEX = 8,

	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_MAX =
		QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_AFTER_LAST - 1
};

/**
 * enum qca_wlan_vendor_attr_get_wifi_info: Attributes for data used by
 * QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_INFO sub command.
 *
 * @QCA_WLAN_VENDOR_ATTR_WIFI_INFO_DRIVER_VERSION: In a request this attribute
 *	should be set to any U8 value to indicate that the driver version
 *	should be returned. When enabled in this manner, in a response this
 *	attribute will contain a string representation of the driver version.
 *
 * @QCA_WLAN_VENDOR_ATTR_WIFI_INFO_FIRMWARE_VERSION: In a request this attribute
 *	should be set to any U8 value to indicate that the firmware version
 *	should be returned. When enabled in this manner, in a response this
 *	attribute will contain a string representation of the firmware version.
 *
 * @QCA_WLAN_VENDOR_ATTR_WIFI_INFO_RADIO_INDEX: In a request this attribute
 *	should be set to any U32 value to indicate that the current radio
 *	index should be returned. When enabled in this manner, in a response
 *	this attribute will contain a U32 radio index value.
 *
 */
enum qca_wlan_vendor_attr_get_wifi_info {
	QCA_WLAN_VENDOR_ATTR_WIFI_INFO_GET_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_WIFI_INFO_DRIVER_VERSION = 1,
	QCA_WLAN_VENDOR_ATTR_WIFI_INFO_FIRMWARE_VERSION = 2,
	QCA_WLAN_VENDOR_ATTR_WIFI_INFO_RADIO_INDEX = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_WIFI_INFO_GET_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_WIFI_INFO_GET_MAX =
	QCA_WLAN_VENDOR_ATTR_WIFI_INFO_GET_AFTER_LAST - 1,
};

/*
 * enum qca_wlan_vendor_attr_wifi_logger_start: Attributes for data used by
 * QCA_NL80211_VENDOR_SUBCMD_WIFI_LOGGER_START sub command.
 */
enum qca_wlan_vendor_attr_wifi_logger_start {
	QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_START_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_RING_ID = 1,
	QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_VERBOSE_LEVEL = 2,
	QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_FLAGS = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_START_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_START_GET_MAX =
	QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_START_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_logger_results {
	QCA_WLAN_VENDOR_ATTR_LOGGER_RESULTS_INVALID = 0,

	/* Unsigned 32-bit value; must match the request Id supplied by
	 * Wi-Fi HAL in the corresponding subcmd NL msg.
	 */
	QCA_WLAN_VENDOR_ATTR_LOGGER_RESULTS_REQUEST_ID = 1,

	/* Unsigned 32-bit value; used to indicate the size of memory
	 * dump to be allocated.
	 */
	QCA_WLAN_VENDOR_ATTR_LOGGER_RESULTS_MEMDUMP_SIZE = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_LOGGER_RESULTS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_LOGGER_RESULTS_MAX =
	QCA_WLAN_VENDOR_ATTR_LOGGER_RESULTS_AFTER_LAST - 1,
};

/**
 * enum qca_scan_freq_list_type: Frequency list types
 *
 * @QCA_PREFERRED_SCAN_FREQ_LIST: The driver shall use the scan frequency list
 *	specified with attribute QCA_ATTR_ROAM_CONTROL_SCAN_FREQ_LIST as
 *	a preferred frequency list for roaming.
 *
 * @QCA_SPECIFIC_SCAN_FREQ_LIST: The driver shall use the frequency list
 *	specified with attribute QCA_ATTR_ROAM_CONTROL_SCAN_FREQ_LIST as
 *	a specific frequency list for roaming.
 */
enum qca_scan_freq_list_type {
	QCA_PREFERRED_SCAN_FREQ_LIST = 1,
	QCA_SPECIFIC_SCAN_FREQ_LIST = 2,
};

/**
 * enum qca_vendor_attr_scan_freq_list_scheme: Frequency list scheme
 *
 * @QCA_ATTR_ROAM_CONTROL_SCAN_FREQ_LIST: Nested attribute of u32 values
 *	List of frequencies in MHz to be considered for a roam scan.
 *
 * @QCA_ATTR_ROAM_CONTROL_SCAN_FREQ_LIST_TYPE: Unsigned 32-bit value.
 *	Type of frequency list scheme being configured/gotten as defined by the
 *	enum qca_scan_freq_list_type.
 */
enum qca_vendor_attr_scan_freq_list_scheme {
	QCA_ATTR_ROAM_CONTROL_SCAN_FREQ_LIST = 1,
	QCA_ATTR_ROAM_CONTROL_SCAN_FREQ_LIST_TYPE = 2,

	/* keep last */
	QCA_ATTR_ROAM_CONTROL_SCAN_FREQ_LIST_SCHEME_AFTER_LAST,
	QCA_ATTR_ROAM_CONTROL_SCAN_FREQ_LIST_SCHEME_MAX =
	QCA_ATTR_ROAM_CONTROL_SCAN_FREQ_LIST_SCHEME_AFTER_LAST - 1,
};

/**
 * enum qca_roam_scan_scheme: Scan scheme
 *
 * @QCA_ROAM_SCAN_SCHEME_NO_SCAN: No frequencies specified to scan.
 *     Indicates the driver to not scan on a Roam Trigger scenario, but
 *     disconnect. E.g., on a BTM request from the AP the driver/firmware shall
 *     disconnect from the current connected AP by notifying a failure
 *     code in the BTM response.
 *
 * @QCA_ROAM_SCAN_SCHEME_PARTIAL_SCAN: Indicates the driver/firmware to
 *     trigger partial frequency scans. These frequencies are the ones learned
 *     or maintained by the driver based on the probability of finding the
 *     BSSIDs in the ESS for which the roaming is triggered.
 *
 * @QCA_ROAM_SCAN_SCHEME_FULL_SCAN: Indicates the driver/firmware to
 *     trigger the scan on all the valid frequencies to find better
 *     candidates to roam.
 */
enum qca_roam_scan_scheme {
	QCA_ROAM_SCAN_SCHEME_NO_SCAN = 0,
	QCA_ROAM_SCAN_SCHEME_PARTIAL_SCAN = 1,
	QCA_ROAM_SCAN_SCHEME_FULL_SCAN = 2,
};

/*
 * enum qca_vendor_roam_triggers: Bitmap of roaming triggers
 *
 * @QCA_ROAM_TRIGGER_REASON_PER: Set if the roam has to be triggered based on
 *	a bad packet error rates (PER).
 * @QCA_ROAM_TRIGGER_REASON_BEACON_MISS: Set if the roam has to be triggered
 *	based on beacon misses from the connected AP.
 * @QCA_ROAM_TRIGGER_REASON_POOR_RSSI: Set if the roam has to be triggered
 *	due to poor RSSI of the connected AP.
 * @QCA_ROAM_TRIGGER_REASON_BETTER_RSSI: Set if the roam has to be triggered
 *	upon finding a BSSID with a better RSSI than the connected BSSID.
 *	Also, set if the roam has to be triggered due to the high RSSI of the
 *	current connected AP (better than
 *	QCA_ATTR_ROAM_CONTROL_CONNECTED_HIGH_RSSI_OFFSET). Here the RSSI of
 *	the current BSSID need not be poor.
 * @QCA_ROAM_TRIGGER_REASON_PERIODIC: Set if the roam has to be triggered
 *	by triggering a periodic scan to find a better AP to roam.
 * @QCA_ROAM_TRIGGER_REASON_DENSE: Set if the roam has to be triggered
 *	when the connected channel environment is too noisy/congested.
 * @QCA_ROAM_TRIGGER_REASON_BTM: Set if the roam has to be triggered
 *	when BTM Request frame is received from the connected AP.
 * @QCA_ROAM_TRIGGER_REASON_BSS_LOAD: Set if the roam has to be triggered
 *	when the channel utilization is goes above the configured threshold.
 * @QCA_ROAM_TRIGGER_REASON_USER_TRIGGER: Set if the roam has to be triggered
 *	based on the request from the user (space).
 * @QCA_ROAM_TRIGGER_REASON_DEAUTH: Set if the roam has to be triggered when
 *	device receives Deauthentication/Disassociation frame from connected AP.
 * @QCA_ROAM_TRIGGER_REASON_IDLE: Set if the roam has to be triggered when the
 *	device is in idle state (no TX/RX) and suspend mode, if the current RSSI
 *	is determined to be a poor one.
 * @QCA_ROAM_TRIGGER_REASON_TX_FAILURES: Set if the roam has to be triggered
 *	based on continuous TX Data frame failures to the connected AP.
 * @QCA_ROAM_TRIGGER_REASON_EXTERNAL_SCAN: Set if the roam has to be triggered
 *	based on the scan results obtained from an external scan (not triggered
 *	to aim roaming).
 *
 * Set the corresponding roam trigger reason bit to consider it for roam
 * trigger.
 * Userspace can set multiple bits and send to the driver. The driver shall
 * consider all of them to trigger/initiate a roam scan.
 */
enum qca_vendor_roam_triggers {
	QCA_ROAM_TRIGGER_REASON_PER		= 1 << 0,
	QCA_ROAM_TRIGGER_REASON_BEACON_MISS	= 1 << 1,
	QCA_ROAM_TRIGGER_REASON_POOR_RSSI	= 1 << 2,
	QCA_ROAM_TRIGGER_REASON_BETTER_RSSI	= 1 << 3,
	QCA_ROAM_TRIGGER_REASON_PERIODIC	= 1 << 4,
	QCA_ROAM_TRIGGER_REASON_DENSE		= 1 << 5,
	QCA_ROAM_TRIGGER_REASON_BTM		= 1 << 6,
	QCA_ROAM_TRIGGER_REASON_BSS_LOAD	= 1 << 7,
	QCA_ROAM_TRIGGER_REASON_USER_TRIGGER	= 1 << 8,
	QCA_ROAM_TRIGGER_REASON_DEAUTH          = 1 << 9,
	QCA_ROAM_TRIGGER_REASON_IDLE		= 1 << 10,
	QCA_ROAM_TRIGGER_REASON_TX_FAILURES	= 1 << 11,
	QCA_ROAM_TRIGGER_REASON_EXTERNAL_SCAN	= 1 << 12,
};

/*
 * enum qca_vendor_roam_fail_reasons: Defines the various roam
 * fail reasons. This enum value is used in
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_FAIL_REASON attribute.
 *
 * @QCA_ROAM_FAIL_REASON_SCAN_NOT_ALLOWED: Roam module in the firmware is not
 * able to trigger the scan.
 * @QCA_ROAM_FAIL_REASON_NO_AP_FOUND: No roamable APs found during roam scan.
 * @QCA_ROAM_FAIL_REASON_NO_CAND_AP_FOUND: No candidate APs found during roam
 * scan.
 * @QCA_ROAM_FAIL_REASON_HOST: Roam fail due to disconnect issued from host.
 * @QCA_ROAM_FAIL_REASON_AUTH_SEND: Unable to send Authentication frame.
 * @QCA_ROAM_FAIL_REASON_AUTH_RECV: Received Authentication frame with error
 * status code.
 * @QCA_ROAM_FAIL_REASON_NO_AUTH_RESP: Authentication frame not received.
 * @QCA_ROAM_FAIL_REASON_REASSOC_SEND: Unable to send Reassociation Request
 * frame.
 * @QCA_ROAM_FAIL_REASON_REASSOC_RECV: Received Reassociation Response frame
 * with error status code.
 * @QCA_ROAM_FAIL_REASON_NO_REASSOC_RESP: Reassociation Response frame not
 * received.
 * @QCA_ROAM_FAIL_REASON_SCAN_FAIL: Scan module not able to start scan.
 * @QCA_ROAM_FAIL_REASON_AUTH_NO_ACK: No ACK is received for Authentication
 * frame.
 * @QCA_ROAM_FAIL_REASON_AUTH_INTERNAL_DROP: Authentication frame is dropped
 * internally before transmission.
 * @QCA_ROAM_FAIL_REASON_REASSOC_NO_ACK: No ACK is received for Reassociation
 * Request frame.
 * @QCA_ROAM_FAIL_REASON_REASSOC_INTERNAL_DROP: Reassociation Request frame is
 * dropped internally.
 * @QCA_ROAM_FAIL_REASON_EAPOL_M1_TIMEOUT: EAPOL-Key M1 is not received and
 * times out.
 * @QCA_ROAM_FAIL_REASON_EAPOL_M2_SEND: Unable to send EAPOL-Key M2 frame.
 * @QCA_ROAM_FAIL_REASON_EAPOL_M2_INTERNAL_DROP: EAPOL-Key M2 frame dropped
 * internally.
 * @QCA_ROAM_FAIL_REASON_EAPOL_M2_NO_ACK: No ACK is received for EAPOL-Key
 * M2 frame.
 * @QCA_ROAM_FAIL_REASON_EAPOL_M3_TIMEOUT: EAPOL-Key M3 frame is not received.
 * @QCA_ROAM_FAIL_REASON_EAPOL_M4_SEND: Unable to send EAPOL-Key M4 frame.
 * @QCA_ROAM_FAIL_REASON_EAPOL_M4_INTERNAL_DROP: EAPOL-Key M4 frame dropped
 * internally.
 * @QCA_ROAM_FAIL_REASON_EAPOL_M4_NO_ACK: No ACK is received for EAPOL-Key M4
 * frame.
 * @QCA_ROAM_FAIL_REASON_NO_SCAN_FOR_FINAL_BEACON_MISS: Roam scan is not
 * started for final beacon miss case.
 * @QCA_ROAM_FAIL_REASON_DISCONNECT: Deauthentication or Disassociation frame
 * received from the AP during roaming handoff.
 * @QCA_ROAM_FAIL_REASON_RESUME_ABORT: Firmware roams to the AP when the Apps
 * or host is suspended and gives the indication of the last roamed AP only
 * when the Apps is resumed. If the Apps is resumed while the roaming is in
 * progress, this ongoing roaming is aborted and the last roamed AP is
 * indicated to host.
 * @QCA_ROAM_FAIL_REASON_SAE_INVALID_PMKID: WPA3-SAE invalid PMKID.
 * @QCA_ROAM_FAIL_REASON_SAE_PREAUTH_TIMEOUT: WPA3-SAE pre-authentication times
 * out.
 * @QCA_ROAM_FAIL_REASON_SAE_PREAUTH_FAIL: WPA3-SAE pre-authentication fails.
 * @QCA_ROAM_FAIL_REASON_CURR_AP_STILL_OK: Roam scan did not happen since the
 * current network conditions are fine.
 */
enum qca_vendor_roam_fail_reasons {
	QCA_ROAM_FAIL_REASON_NONE = 0,
	QCA_ROAM_FAIL_REASON_SCAN_NOT_ALLOWED = 1,
	QCA_ROAM_FAIL_REASON_NO_AP_FOUND = 2,
	QCA_ROAM_FAIL_REASON_NO_CAND_AP_FOUND = 3,
	QCA_ROAM_FAIL_REASON_HOST = 4,
	QCA_ROAM_FAIL_REASON_AUTH_SEND = 5,
	QCA_ROAM_FAIL_REASON_AUTH_RECV = 6,
	QCA_ROAM_FAIL_REASON_NO_AUTH_RESP = 7,
	QCA_ROAM_FAIL_REASON_REASSOC_SEND = 8,
	QCA_ROAM_FAIL_REASON_REASSOC_RECV = 9,
	QCA_ROAM_FAIL_REASON_NO_REASSOC_RESP = 10,
	QCA_ROAM_FAIL_REASON_SCAN_FAIL = 11,
	QCA_ROAM_FAIL_REASON_AUTH_NO_ACK = 12,
	QCA_ROAM_FAIL_REASON_AUTH_INTERNAL_DROP = 13,
	QCA_ROAM_FAIL_REASON_REASSOC_NO_ACK = 14,
	QCA_ROAM_FAIL_REASON_REASSOC_INTERNAL_DROP = 15,
	QCA_ROAM_FAIL_REASON_EAPOL_M1_TIMEOUT = 16,
	QCA_ROAM_FAIL_REASON_EAPOL_M2_SEND = 17,
	QCA_ROAM_FAIL_REASON_EAPOL_M2_INTERNAL_DROP = 18,
	QCA_ROAM_FAIL_REASON_EAPOL_M2_NO_ACK = 19,
	QCA_ROAM_FAIL_REASON_EAPOL_M3_TIMEOUT = 20,
	QCA_ROAM_FAIL_REASON_EAPOL_M4_SEND = 21,
	QCA_ROAM_FAIL_REASON_EAPOL_M4_INTERNAL_DROP = 22,
	QCA_ROAM_FAIL_REASON_EAPOL_M4_NO_ACK = 23,
	QCA_ROAM_FAIL_REASON_NO_SCAN_FOR_FINAL_BEACON_MISS = 24,
	QCA_ROAM_FAIL_REASON_DISCONNECT = 25,
	QCA_ROAM_FAIL_REASON_RESUME_ABORT = 26,
	QCA_ROAM_FAIL_REASON_SAE_INVALID_PMKID = 27,
	QCA_ROAM_FAIL_REASON_SAE_PREAUTH_TIMEOUT = 28,
	QCA_ROAM_FAIL_REASON_SAE_PREAUTH_FAIL = 29,
	QCA_ROAM_FAIL_REASON_CURR_AP_STILL_OK = 30,
};

/*
 * enum qca_vendor_roam_invoke_fail_reasons: Defines the various roam
 * invoke fail reasons. This enum value is used in
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_INVOKE_FAIL_REASON attribute.
 *
 * @QCA_ROAM_INVOKE_STATUS_IFACE_INVALID: Invalid interface ID is passed
 * in roam invoke command.
 * @QCA_ROAM_INVOKE_STATUS_OFFLOAD_DISABLE: Roam offload in firmware is not
 * enabled.
 * @QCA_ROAM_INVOKE_STATUS_AP_SSID_LENGTH_INVALID: Connected AP profile SSID
 * length is invalid.
 * @QCA_ROAM_INVOKE_STATUS_ROAM_DISALLOW: Firmware internal roaming is already
 * in progress.
 * @QCA_ROAM_INVOKE_STATUS_NON_ROAMABLE_AP: Host sends the Beacon/Probe Response
 * of the AP in the roam invoke command to firmware. This reason is sent by the
 * firmware when the given AP is configured to be ignored or SSID/security
 * does not match.
 * @QCA_ROAM_INVOKE_STATUS_ROAM_INTERNAL_FAIL: Roam handoff failed because of
 * firmware internal reasons.
 * @QCA_ROAM_INVOKE_STATUS_DISALLOW: Roam invoke trigger is not enabled.
 * @QCA_ROAM_INVOKE_STATUS_SCAN_FAIL: Scan start fail for roam invoke.
 * @QCA_ROAM_INVOKE_STATUS_START_ROAM_FAIL: Roam handoff start fail.
 * @QCA_ROAM_INVOKE_STATUS_INVALID_PARAMS: Roam invoke parameters are invalid.
 * @QCA_ROAM_INVOKE_STATUS_NO_CAND_AP: No candidate AP found to roam to.
 * @QCA_ROAM_INVOKE_STATUS_ROAM_FAIL: Roam handoff failed.
 */
enum qca_vendor_roam_invoke_fail_reasons {
	QCA_ROAM_INVOKE_STATUS_NONE = 0,
	QCA_ROAM_INVOKE_STATUS_IFACE_INVALID = 1,
	QCA_ROAM_INVOKE_STATUS_OFFLOAD_DISABLE = 2,
	QCA_ROAM_INVOKE_STATUS_AP_SSID_LENGTH_INVALID = 3,
	QCA_ROAM_INVOKE_STATUS_ROAM_DISALLOW = 4,
	QCA_ROAM_INVOKE_STATUS_NON_ROAMABLE_AP = 5,
	QCA_ROAM_INVOKE_STATUS_ROAM_INTERNAL_FAIL = 6,
	QCA_ROAM_INVOKE_STATUS_DISALLOW = 7,
	QCA_ROAM_INVOKE_STATUS_SCAN_FAIL = 8,
	QCA_ROAM_INVOKE_STATUS_START_ROAM_FAIL = 9,
	QCA_ROAM_INVOKE_STATUS_INVALID_PARAMS = 10,
	QCA_ROAM_INVOKE_STATUS_NO_CAND_AP = 11,
	QCA_ROAM_INVOKE_STATUS_ROAM_FAIL = 12,

};

/**
 * enum qca_vendor_attr_roam_candidate_selection_criteria:
 *
 * Each attribute carries a weightage in percentage (%).
 *
 * @QCA_ATTR_ROAM_CAND_SEL_CRITERIA_SCORE_RSSI: Unsigned 8-bit value.
 *	Represents the weightage to be given for the RSSI selection
 *	criteria among other parameters.
 *
 * @QCA_ATTR_ROAM_CAND_SEL_CRITERIA_RATE: Unsigned 8-bit value.
 *	Represents the weightage to be given for the rate selection
 *	criteria among other parameters.
 *
 * @QCA_ATTR_ROAM_CAND_SEL_CRITERIA_SCORE_BW: Unsigned 8-bit value.
 *	Represents the weightage to be given for the band width selection
 *	criteria among other parameters.
 *
 * @QCA_ATTR_ROAM_CAND_SEL_CRITERIA_SCORE_BAND: Unsigned 8-bit value.
 *	Represents the weightage to be given for the band selection
 *	criteria among other parameters.
 *
 * @QCA_ATTR_ROAM_CAND_SEL_CRITERIA_SCORE_NSS: Unsigned 8-bit value.
 *	Represents the weightage to be given for the NSS selection
 *	criteria among other parameters.
 *
 * @QCA_ATTR_ROAM_CAND_SEL_CRITERIA_SCORE_CHAN_CONGESTION: Unsigned 8-bit value.
 *	Represents the weightage to be given for the channel congestion
 *	selection criteria among other parameters.
 *
 * @QCA_ATTR_ROAM_CAND_SEL_CRITERIA_SCORE_BEAMFORMING: Unsigned 8-bit value.
 *	Represents the weightage to be given for the beamforming selection
 *	criteria among other parameters.
 *
 * @QCA_ATTR_ROAM_CAND_SEL_CRITERIA_SCORE_OCE_WAN: Unsigned 8-bit value.
 *	Represents the weightage to be given for the OCE selection
 *	criteria among other parameters.
 */
enum qca_vendor_attr_roam_candidate_selection_criteria {
	QCA_ATTR_ROAM_CAND_SEL_CRITERIA_SCORE_RSSI = 1,
	QCA_ATTR_ROAM_CAND_SEL_CRITERIA_RATE = 2,
	QCA_ATTR_ROAM_CAND_SEL_CRITERIA_SCORE_BW = 3,
	QCA_ATTR_ROAM_CAND_SEL_CRITERIA_SCORE_BAND = 4,
	QCA_ATTR_ROAM_CAND_SEL_CRITERIA_SCORE_NSS = 5,
	QCA_ATTR_ROAM_CAND_SEL_CRITERIA_SCORE_CHAN_CONGESTION = 6,
	QCA_ATTR_ROAM_CAND_SEL_CRITERIA_SCORE_BEAMFORMING = 7,
	QCA_ATTR_ROAM_CAND_SEL_CRITERIA_SCORE_OCE_WAN = 8,

	/* keep last */
	QCA_ATTR_ROAM_CAND_SEL_CRITERIA_RATE_AFTER_LAST,
	QCA_ATTR_ROAM_CAND_SEL_CRITERIA_RATE_MAX =
	QCA_ATTR_ROAM_CAND_SEL_CRITERIA_RATE_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_attr_roam_control - Attributes to carry roam configuration
 *	The following attributes are used to set/get/clear the respective
 *	configurations to/from the driver.
 *	For the get, the attribute for the configuration to be queried shall
 *	carry any of its acceptable values to the driver. In return, the driver
 *	shall send the configured values within the same attribute to the user
 *	space.
 *
 * @QCA_ATTR_ROAM_CONTROL_ENABLE: Unsigned 8-bit value.
 *	Signifies to enable/disable roam control in driver.
 *	1-enable, 0-disable
 *	Enable: Mandates the driver to do the further roams using the
 *	configuration parameters set through
 *	QCA_WLAN_VENDOR_ROAMING_SUBCMD_CONTROL_SET.
 *	Disable: Disables the driver/firmware roaming triggered through
 *	QCA_WLAN_VENDOR_ROAMING_SUBCMD_CONTROL_SET. Further roaming is
 *	expected to continue with the default configurations.
 *
 * @QCA_ATTR_ROAM_CONTROL_STATUS: Unsigned 8-bit value.
 *	This is used along with QCA_WLAN_VENDOR_ROAMING_SUBCMD_CONTROL_GET.
 *	Roam control status is obtained through this attribute.
 *
 * @QCA_ATTR_ROAM_CONTROL_CLEAR_ALL: Flag attribute to indicate the
 *	complete config set through QCA_WLAN_VENDOR_ROAMING_SUBCMD_CONTROL_SET
 *	is to be cleared in the driver.
 *	This is used along with QCA_WLAN_VENDOR_ROAMING_SUBCMD_CONTROL_CLEAR
 *	and shall be ignored if used with other sub commands.
 *	If this attribute is specified along with subcmd
 *	QCA_WLAN_VENDOR_ROAMING_SUBCMD_CONTROL_CLEAR, the driver shall ignore
 *	all other attributes, if there are any.
 *	If this attribute is not specified when the subcmd
 *	QCA_WLAN_VENDOR_ROAMING_SUBCMD_CONTROL_CLEAR is sent, the driver shall
 *	clear the data corresponding to the attributes specified.
 *
 * @QCA_ATTR_ROAM_CONTROL_FREQ_LIST_SCHEME: Nested attribute to carry the
 *	list of frequencies and its type, represented by
 *	enum qca_vendor_attr_scan_freq_list_scheme.
 *	Frequency list and its type are mandatory for this attribute to set
 *	the frequencies.
 *	Frequency type is mandatory for this attribute to get the frequencies
 *	and the frequency list is obtained through
 *	QCA_ATTR_ROAM_CONTROL_SCAN_FREQ_LIST.
 *	Frequency list type is mandatory for this attribute to clear the
 *	frequencies.
 *
 * @QCA_ATTR_ROAM_CONTROL_SCAN_PERIOD: Unsigned 32-bit value.
 *	Carries the value of scan period in seconds to set.
 *	The value of scan period is obtained with the same attribute for get.
 *	Clears the scan period in the driver when specified with clear command.
 *	Scan period is the idle time in seconds between each subsequent
 *	channel scans.
 *
 * @QCA_ATTR_ROAM_CONTROL_FULL_SCAN_PERIOD: Unsigned 32-bit value.
 *	Carries the value of full scan period in seconds to set.
 *	The value of full scan period is obtained with the same attribute for
 *	get.
 *	Clears the full scan period in the driver when specified with clear
 *	command. Full scan period is the idle period in seconds between two
 *	successive full channel roam scans.
 *
 * @QCA_ATTR_ROAM_CONTROL_TRIGGERS: Unsigned 32-bit value.
 *	Carries a bitmap of the roam triggers specified in
 *	enum qca_vendor_roam_triggers.
 *	The driver shall enable roaming by enabling corresponding roam triggers
 *	based on the trigger bits sent with this attribute.
 *	If this attribute is not configured, the driver shall proceed with
 *	default behavior.
 *	The bitmap configured is obtained with the same attribute for get.
 *	Clears the bitmap configured in driver when specified with clear
 *	command.
 *
 * @QCA_ATTR_ROAM_CONTROL_SELECTION_CRITERIA: Nested attribute signifying the
 *	weightage in percentage (%) to be given for each selection criteria.
 *	Different roam candidate selection criteria are represented by
 *	enum qca_vendor_attr_roam_candidate_selection_criteria.
 *	The driver shall select the roam candidate based on corresponding
 *	candidate selection scores sent.
 *
 *	An empty nested attribute is used to indicate that no specific
 *	preference score/criteria is configured (i.e., to disable this mechanism
 *	in the set case and to show that the mechanism is disabled in the get
 *	case).
 *
 *	Userspace can send multiple attributes out of this enum to the driver.
 *	Since this attribute represents the weight/percentage of preference for
 *	the respective selection criteria, it is preferred to configure 100%
 *	total weightage. The value in each attribute or cumulative weight of the
 *	values in all the nested attributes should not exceed 100%. The driver
 *	shall reject such configuration.
 *
 *	If the weights configured through this attribute are less than 100%,
 *	the driver shall honor the weights (x%) passed for the corresponding
 *	selection criteria and choose/distribute rest of the weight (100-x)%
 *	for the other selection criteria, based on its internal logic.
 *
 *	The selection criteria configured is obtained with the same
 *	attribute for get.
 *
 *	Clears the selection criteria configured in the driver when specified
 *	with clear command.
 *
 * @QCA_ATTR_ROAM_CONTROL_SCAN_SCHEME: Unsigned 32-bit value.
 *	Represents value of the scan frequency scheme from enum
 *	qca_roam_scan_scheme.
 *	It's an optional attribute. If this attribute is not configured, the
 *	driver shall proceed with default behavior.
 *
 * @QCA_ATTR_ROAM_CONTROL_CONNECTED_RSSI_THRESHOLD: Signed 32-bit value in dBm,
 *	signifying the RSSI threshold of the current connected AP, indicating
 *	the driver to trigger roam only when the current connected AP's RSSI
 *	is less than this threshold. The RSSI threshold through this attribute
 *	is only used by the STA when the connected AP asks it to roam through
 *	a BTM request. Based on this threshold, the STA can either honor or
 *	reject the AP's request to roam, and notify the status to the AP in a
 *	BTM response.
 *
 * @QCA_ATTR_ROAM_CONTROL_CANDIDATE_RSSI_THRESHOLD: Signed 32-bit value in dBm,
 *	signifying the RSSI threshold of the candidate AP, indicating
 *	the driver to trigger roam only to the candidate AP with RSSI
 *	better than this threshold. If RSSI thresholds for candidate APs found
 *	in the 2.4 GHz, 5 GHz, and 6 GHz bands are configured separately using
 *	QCA_ATTR_ROAM_CONTROL_CANDIDATE_RSSI_THRESHOLD_2P4GHZ,
 *	QCA_ATTR_ROAM_CONTROL_CANDIDATE_RSSI_THRESHOLD_5GHZ, and/or
 *	QCA_ATTR_ROAM_CONTROL_CANDIDATE_RSSI_THRESHOLD_6GHZ, those values will
 *	take precedence over the value configured using the
 *	QCA_ATTR_ROAM_CONTROL_CANDIDATE_RSSI_THRESHOLD attribute.
 *
 * @QCA_ATTR_ROAM_CONTROL_USER_REASON: Unsigned 32-bit value. Represents the
 *	user defined reason code to be sent to the AP in response to AP's
 *	request to trigger the roam if the roaming cannot be triggered.
 *	Applies to all the scenarios of AP assisted roaming (e.g., BTM).
 *
 * @QCA_ATTR_ROAM_CONTROL_SCAN_SCHEME_TRIGGERS: Unsigned 32-bit value.
 *	Carries a bitmap of the roam triggers specified in
 *	enum qca_vendor_roam_triggers.
 *	Represents the roam triggers for which the specific scan scheme from
 *	enum qca_roam_scan_scheme has to be applied.
 *	It's an optional attribute. If this attribute is not configured, but
 *	QCA_ATTR_ROAM_CONTROL_SCAN_SCHEME is specified, the scan scheme
 *	specified through QCA_ATTR_ROAM_CONTROL_SCAN_SCHEME is applicable for
 *	all the roams.
 *	If both QCA_ATTR_ROAM_CONTROL_SCAN_SCHEME and
 *	QCA_ATTR_ROAM_CONTROL_SCAN_SCHEME_TRIGGERS are not specified, the
 *	driver shall proceed with the default behavior.
 *
 * @QCA_ATTR_ROAM_CONTROL_CANDIDATE_RSSI_THRESHOLD_2P4GHZ: Signed 32-bit value
 *	in dBm, signifying the RSSI threshold of the candidate AP found in the
 *	2.4 GHz band. The driver/firmware shall trigger roaming to the candidate
 *	AP found in the 2.4 GHz band only if its RSSI value is better than this
 *	threshold. Optional attribute. If this attribute is not included, the
 *	threshold value specified by the
 *	QCA_ATTR_ROAM_CONTROL_CANDIDATE_RSSI_THRESHOLD attribute shall be used.
 *
 * @QCA_ATTR_ROAM_CONTROL_CANDIDATE_RSSI_THRESHOLD_5GHZ: Signed 32-bit value in
 *	dBm, signifying the RSSI threshold of the candidate AP found in the 5
 *	GHz band. The driver/firmware shall trigger roaming to the candidate AP
 *	found in the 5 GHz band only if its RSSI value is better than this
 *	threshold. Optional attribute. If this attribute is not included, the
 *	threshold value specified by tge
 *	QCA_ATTR_ROAM_CONTROL_CANDIDATE_RSSI_THRESHOLD attribute shall be used.
 *
 * @QCA_ATTR_ROAM_CONTROL_CANDIDATE_RSSI_THRESHOLD_6GHZ: Signed 32-bit value in
 *	dBm, signifying the RSSI threshold of the candidate AP found in the 6
 *	GHz band. The driver/firmware shall trigger roaming to the candidate AP
 *	found in the 6 GHz band only if its RSSI value is better than this
 *	threshold. Optional attribute. If this attribute is not included, the
 *	threshold value specified by the
 *	QCA_ATTR_ROAM_CONTROL_CANDIDATE_RSSI_THRESHOLD attribute shall be used.
 *
 * @QCA_ATTR_ROAM_CONTROL_BAND_MASK: Unsigned 32-bit value.
 *	Carries bitmask value of bits from &enum qca_set_band and represents
 *	all the bands in which roaming is allowed. The configuration is valid
 *	until next disconnection. If this attribute is not present, the
 *	existing configuration shall be used. By default, roaming is allowed on
 *	all bands supported by the local device. When the value is set to
 *	%QCA_SETBAND_AUTO, all supported bands shall be enabled.
 *
 * @QCA_ATTR_ROAM_CONTROL_ACTIVE_CH_DWELL_TIME: u16 value in milliseconds.
 *	Optional parameter. Scan dwell time for active channels in the 2.4/5 GHz
 *	bands. If this attribute is not configured, the driver shall proceed
 *	with default behavior.
 *
 * @QCA_ATTR_ROAM_CONTROL_PASSIVE_CH_DWELL_TIME: u16 value in milliseconds.
 *	Optional parameter. Scan dwell time for passive channels in the 5 GHz
 *	band. If this attribute is not configured, the driver shall proceed with
 *	default behavior.
 *
 * @QCA_ATTR_ROAM_CONTROL_HOME_CHANNEL_TIME: u16 value in milliseconds.
 *	Optional parameter. The minimum duration to stay on the connected AP
 *	channel during the channel scanning. If this attribute is not
 *	configured, the driver shall proceed with default behavior.
 *
 * @QCA_ATTR_ROAM_CONTROL_MAXIMUM_AWAY_TIME: u16 value in milliseconds.
 *	Optional parameter. The maximum duration for which the radio can scan
 *	foreign channels consecutively without coming back to home channel. If
 *	this attribute is not configured, the driver shall proceed with default
 *	behavior.
 *
 * @QCA_ATTR_ROAM_CONTROL_SCAN_6G_PSC_DWELL_TIME: u16 value in milliseconds.
 *	Optional parameter. Scan dwell time for 6G Preferred Scanning Channels.
 *	If this attribute is not configured, the driver shall proceed with
 *	default behavior.
 *
 * @QCA_ATTR_ROAM_CONTROL_SCAN_6G_NON_PSC_DWELL_TIME: u16 value in milliseconds.
 *	Optional parameter. Scan dwell time for 6G Non Preferred Scanning
 *	Channels. If this attribute is not configured, the driver shall proceed
 *	with default behavior.
 *
 * @QCA_ATTR_ROAM_CONTROL_RX_LINKSPEED_THRESHOLD: u16 value in Mbps.
 *	Optional parameter. RX link speed threshold to disable roaming.
 *	If the current RX link speed is above the threshold, roaming is not
 *	needed. If this attribute is not configured, or if it is set to 0, the
 *	driver will not consider the RX link speed in the roaming decision.
 *
 * @QCA_ATTR_ROAM_CONTROL_HO_DELAY_FOR_RX: u16 value in milliseconds.
 *	Optional parameter. This configuration delays hand-off by the
 *	specified duration to receive pending RX frames from the current BSS.
 *
 * @QCA_ATTR_ROAM_CONTROL_FULL_SCAN_NO_REUSE_PARTIAL_SCAN_FREQ: Unsigned 8-bit
 *	value.
 *	During the roam scan, if there are no desired APs found in the partial
 *	frequency list, an immediate full scan on all the supported frequencies
 *	is initiated as a fallback. This flag controls the frequency list
 *	creation for the full scan on the following lines.
 *	1 - Full scan to exclude the frequencies that were already scanned by
 *	    the previous partial scan.
 *	0 - Full scan to include all the supported frequencies irrespective of
 *	    the ones part of the earlier partial scan.
 *	If this flag is not specified, a full scan shall include all the
 *	supported frequencies irrespective of the ones part of an earlier
 *	partial scan.
 *
 * @QCA_ATTR_ROAM_CONTROL_FULL_SCAN_6GHZ_ONLY_ON_PRIOR_DISCOVERY: Unsigned 8-bit
 *	value.
 *	During the roam scan, if there are no desired APs found in the partial
 *	frequency list, an immediate full scan on all the supported frequencies
 *	is initiated as a fallback. This full scan would add the 2.4/5/6 GHz
 *	frequencies, including all PSC frequencies by default. This attribute
 *	controls the inclusion of the 6 GHz PSC frequencies for the full scan
 *	as following.
 *	1 - Full scan to include the supported 6 GHz PSC frequencies only on the
 *	   prior discovery of any 6 GHz frequency support in the environment.
 *	   This discovery can happen through a prior RNR, 11k neighbor
 *	request, 11v BTM request, host scan, etc.
 *	0 - Default behavior. Full scan to include all the supported 6 GHz
 *	   PSC frequencies regardless of whether 6 GHz BSSs have been
 *	   discovered.
 *	The default behavior if this flag is not specified is to include all
 *	the supported 6 GHz PSC frequencies in the roam full scan.
 *
 * @QCA_ATTR_ROAM_CONTROL_CONNECTED_LOW_RSSI_THRESHOLD: Signed 32-bit value
 *	in dBm.
 *	This attribute configures the low RSSI threshold of the connected AP,
 *	based on which the STA can start looking for the neighbor APs and
 *	trigger the roam eventually. STA keeps monitoring for the connected
 *	AP's RSSI and will start scanning for neighboring APs once the RSSI
 *	falls below this threshold. This attribute differs from
 *	QCA_ATTR_ROAM_CONTROL_CONNECTED_RSSI_THRESHOLD where the configured
 *	threshold is used only when the connected AP asks the STA to roam
 *	through a BTM request.
 *
 * @QCA_ATTR_ROAM_CONTROL_CANDIDATE_ROAM_RSSI_DIFF: Unsigned 8-bit value.
 *	This attribute signifies the RSSI difference threshold between the
 *	connected AP and the new candidate AP. This parameter influences the
 *	STA to roam to the new candidate only when its RSSI is better than
 *	the current connected one by this threshold.
 *	This parameter configures the roam behavior among the 2.4/5/6 GHz bands.
 *
 * @QCA_ATTR_ROAM_CONTROL_6GHZ_CANDIDATE_ROAM_RSSI_DIFF: Unsigned 8-bit value.
 *	This attribute signifies the RSSI difference threshold between the
 *	connected AP in the 2.4/5 GHz bands and the new candidate AP in the
 *	6 GHz band. This parameter influences the STA to roam to the new 6 GHz
 *	candidate only when its RSSI is better than the current connected one
 *	by this threshold. This threshold overrides
 *	QCA_ATTR_ROAM_CONTROL_CANDIDATE_ROAM_RSSI_DIFF for the roam from 2.4/5
 *	GHz to 6 GHz alone with the intention to have a different value to roam
 *	to the preferred 6 GHz band.
 *
 * @QCA_ATTR_ROAM_CONTROL_CONNECTED_HIGH_RSSI_OFFSET: Unsigned 8-bit value.
 *	This attribute signifies the RSSI offset that is added to low RSSI
 *	threshold (QCA_ATTR_ROAM_CONTROL_CONNECTED_LOW_RSSI_THRESHOLD) to imply
 *	high RSSI threshold. STA is expected to trigger roam if the current
 *	connected AP's RSSI gets above this high RSSI threshold. STA's roam
 *	attempt on high RSSI threshold aims to find candidates from other
 *	better Wi-Fi bands. E.g., STA would initially connect to a 2.4 GHz BSSID
 *	and would migrate to 5/6 GHz when it comes closer to the AP (high RSSI
 *	for 2.4 GHz BSS).
 */
enum qca_vendor_attr_roam_control {
	QCA_ATTR_ROAM_CONTROL_ENABLE = 1,
	QCA_ATTR_ROAM_CONTROL_STATUS = 2,
	QCA_ATTR_ROAM_CONTROL_CLEAR_ALL = 3,
	QCA_ATTR_ROAM_CONTROL_FREQ_LIST_SCHEME = 4,
	QCA_ATTR_ROAM_CONTROL_SCAN_PERIOD = 5,
	QCA_ATTR_ROAM_CONTROL_FULL_SCAN_PERIOD = 6,
	QCA_ATTR_ROAM_CONTROL_TRIGGERS = 7,
	QCA_ATTR_ROAM_CONTROL_SELECTION_CRITERIA = 8,
	QCA_ATTR_ROAM_CONTROL_SCAN_SCHEME = 9,
	QCA_ATTR_ROAM_CONTROL_CONNECTED_RSSI_THRESHOLD = 10,
	QCA_ATTR_ROAM_CONTROL_CANDIDATE_RSSI_THRESHOLD = 11,
	QCA_ATTR_ROAM_CONTROL_USER_REASON = 12,
	QCA_ATTR_ROAM_CONTROL_SCAN_SCHEME_TRIGGERS = 13,
	QCA_ATTR_ROAM_CONTROL_CANDIDATE_RSSI_THRESHOLD_2P4GHZ = 14,
	QCA_ATTR_ROAM_CONTROL_CANDIDATE_RSSI_THRESHOLD_5GHZ = 15,
	QCA_ATTR_ROAM_CONTROL_CANDIDATE_RSSI_THRESHOLD_6GHZ = 16,
	QCA_ATTR_ROAM_CONTROL_BAND_MASK = 17,
	QCA_ATTR_ROAM_CONTROL_ACTIVE_CH_DWELL_TIME = 18,
	QCA_ATTR_ROAM_CONTROL_PASSIVE_CH_DWELL_TIME = 19,
	QCA_ATTR_ROAM_CONTROL_HOME_CHANNEL_TIME = 20,
	QCA_ATTR_ROAM_CONTROL_MAXIMUM_AWAY_TIME = 21,
	QCA_ATTR_ROAM_CONTROL_SCAN_6G_PSC_DWELL_TIME = 22,
	QCA_ATTR_ROAM_CONTROL_SCAN_6G_NON_PSC_DWELL_TIME = 23,
	QCA_ATTR_ROAM_CONTROL_LINKSPEED_THRESHOLD = 24,
	QCA_ATTR_ROAM_CONTROL_HO_DELAY_FOR_RX = 25,
	QCA_ATTR_ROAM_CONTROL_FULL_SCAN_NO_REUSE_PARTIAL_SCAN_FREQ = 26,
	QCA_ATTR_ROAM_CONTROL_FULL_SCAN_6GHZ_ONLY_ON_PRIOR_DISCOVERY = 27,
	QCA_ATTR_ROAM_CONTROL_CONNECTED_LOW_RSSI_THRESHOLD = 28,
	QCA_ATTR_ROAM_CONTROL_CANDIDATE_ROAM_RSSI_DIFF = 29,
	QCA_ATTR_ROAM_CONTROL_6GHZ_CANDIDATE_ROAM_RSSI_DIFF = 30,
	QCA_ATTR_ROAM_CONTROL_CONNECTED_HIGH_RSSI_OFFSET = 31,

	/* keep last */
	QCA_ATTR_ROAM_CONTROL_AFTER_LAST,
	QCA_ATTR_ROAM_CONTROL_MAX =
	QCA_ATTR_ROAM_CONTROL_AFTER_LAST - 1,
};

/*
 * enum qca_wlan_vendor_attr_roaming_config_params: Attributes for data used by
 * QCA_NL80211_VENDOR_SUBCMD_ROAM sub command.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_SUBCMD: Unsigned 32-bit value.
 *	Represents the different roam sub commands referred by
 *	enum qca_wlan_vendor_roaming_subcmd.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_REQ_ID: Unsigned 32-bit value.
 *	Represents the Request ID for the specific set of commands.
 *	This also helps to map specific set of commands to the respective
 *	ID / client. e.g., helps to identify the user entity configuring the
 *	ignored BSSIDs and accordingly clear the respective ones with the
 *	matching ID.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_ALLOW_LIST_SSID_NUM_NETWORKS: Unsigned
 *	32-bit value. Represents the number of allowlist SSIDs configured.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_ALLOW_LIST_SSID_LIST: Nested attribute
 *	to carry the list of allowlist SSIDs.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_ALLOW_LIST_SSID: SSID (binary attribute,
 *	0..32 octets). Represents the allow list SSID. Allowlist SSIDs
 *	represent the list of SSIDs to which the firmware/driver can consider
 *	to roam to.
 *
 * The following PARAM_A_BAND_XX attributes are applied to 5GHz BSSIDs when
 * comparing with a 2.4GHz BSSID. They are not applied when comparing two
 * 5GHz BSSIDs.The following attributes are set through the Roaming SUBCMD -
 * QCA_WLAN_VENDOR_ROAMING_SUBCMD_SET_GSCAN_ROAM_PARAMS.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_BOOST_THRESHOLD: Signed 32-bit
 *	value, RSSI threshold above which 5GHz RSSI is favored.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_PENALTY_THRESHOLD: Signed 32-bit
 *	value, RSSI threshold below which 5GHz RSSI is penalized.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_BOOST_FACTOR: Unsigned 32-bit
 *	value, factor by which 5GHz RSSI is boosted.
 *	boost=(RSSI_measured-5GHz_boost_threshold)*5GHz_boost_factor
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_PENALTY_FACTOR: Unsigned 32-bit
 *	value, factor by which 5GHz RSSI is penalized.
 *	penalty=(5GHz_penalty_threshold-RSSI_measured)*5GHz_penalty_factor
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_MAX_BOOST: Unsigned 32-bit
 *	value, maximum boost that can be applied to a 5GHz RSSI.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_LAZY_ROAM_HISTERESYS: Unsigned 32-bit
 *	value, boost applied to current BSSID to ensure the currently
 *	associated BSSID is favored so as to prevent ping-pong situations.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_ALERT_ROAM_RSSI_TRIGGER: Signed 32-bit
 *	value, RSSI below which "Alert" roam is enabled.
 *	"Alert" mode roaming - firmware is "urgently" hunting for another BSSID
 *	because the RSSI is low, or because many successive beacons have been
 *	lost or other bad link conditions.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_ENABLE: Unsigned 32-bit
 *	value. 1-Enable, 0-Disable. Represents "Lazy" mode, where
 *	firmware is hunting for a better BSSID or allow listed SSID even though
 *	the RSSI of the link is good. The parameters enabling the roaming are
 *	configured through the PARAM_A_BAND_XX attrbutes.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PREFS: Nested attribute,
 *	represents the BSSIDs preferred over others while evaluating them
 *	for the roaming.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_NUM_BSSID: Unsigned
 *	32-bit value. Represents the number of preferred BSSIDs set.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_BSSID: 6-byte MAC
 *	address representing the BSSID to be preferred.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_RSSI_MODIFIER: Signed
 *	32-bit value, representing the modifier to be applied to the RSSI of
 *	the BSSID for the purpose of comparing it with other roam candidate.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS: Nested attribute,
 *	represents the BSSIDs to get ignored for roaming.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_NUM_BSSID: Unsigned
 *	32-bit value, represents the number of ignored BSSIDs.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_BSSID: 6-byte MAC
 *	address representing the ignored BSSID.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_HINT: Flag attribute,
 *	indicates this request to ignore the BSSID as a hint to the driver. The
 *	driver can select this BSSID in the worst case (when no other BSSIDs are
 *	better).
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_CONTROL: Nested attribute to
 *	set/get/clear the roam control config as
 *	defined @enum qca_vendor_attr_roam_control.
 */
enum qca_wlan_vendor_attr_roaming_config_params {
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_INVALID = 0,

	QCA_WLAN_VENDOR_ATTR_ROAMING_SUBCMD = 1,
	QCA_WLAN_VENDOR_ATTR_ROAMING_REQ_ID = 2,

	/* Attributes for wifi_set_ssid_allow_list */
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_ALLOW_LIST_SSID_NUM_NETWORKS = 3,
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_ALLOW_LIST_SSID_LIST = 4,
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_ALLOW_LIST_SSID = 5,

	/* Attributes for set_roam_params */
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_BOOST_THRESHOLD = 6,
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_PENALTY_THRESHOLD = 7,
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_BOOST_FACTOR = 8,
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_PENALTY_FACTOR = 9,
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_MAX_BOOST = 10,
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_LAZY_ROAM_HISTERESYS = 11,
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_ALERT_ROAM_RSSI_TRIGGER = 12,

	/* Attribute for set_lazy_roam */
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_ENABLE = 13,

	/* Attribute for set_lazy_roam with preferences */
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PREFS = 14,
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_NUM_BSSID = 15,
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_BSSID = 16,
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_RSSI_MODIFIER = 17,

	/* Attribute for setting ignored BSSID parameters */
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS = 18,
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_NUM_BSSID = 19,
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_BSSID = 20,
	/* Flag attribute indicates this entry as a hint */
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_HINT = 21,

	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_CONTROL = 22,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_MAX =
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_AFTER_LAST - 1,
};

/* old names for API compatibility */
#define QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_WHITE_LIST_SSID_NUM_NETWORKS \
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_ALLOW_LIST_SSID_NUM_NETWORKS
#define QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_WHITE_LIST_SSID_LIST \
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_ALLOW_LIST_SSID_LIST
#define QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_WHITE_LIST_SSID \
	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_ALLOW_LIST_SSID

/*
 * enum qca_wlan_vendor_roaming_subcmd: Referred by
 * QCA_WLAN_VENDOR_ATTR_ROAMING_SUBCMD.
 *
 * @QCA_WLAN_VENDOR_ROAMING_SUBCMD_SSID_ALLOW_LIST: Sub command to
 *	configure the allow list SSIDs. These are configured through
 *	the following attributes.
 *	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_ALLOW_LIST_SSID_NUM_NETWORKS,
 *	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_ALLOW_LIST_SSID_LIST,
 *	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_ALLOW_LIST_SSID
 *
 * @QCA_WLAN_VENDOR_ROAMING_SUBCMD_SET_GSCAN_ROAM_PARAMS: Sub command to
 *	configure the Roam params. These parameters are evaluated on the GScan
 *	results. Refers the attributes PARAM_A_BAND_XX above to configure the
 *	params.
 *
 * @QCA_WLAN_VENDOR_ROAMING_SUBCMD_SET_LAZY_ROAM: Sets the Lazy roam. Uses
 *	the attribute QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_ENABLE
 *	to enable/disable Lazy roam.
 *
 * @QCA_WLAN_VENDOR_ROAMING_SUBCMD_SET_BSSID_PREFS: Sets the BSSID
 *	preference. Contains the attribute
 *	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PREFS to set the BSSID
 *	preference.
 *
 * @QCA_WLAN_VENDOR_ROAMING_SUBCMD_SET_DENYLIST_BSSID: Sets the list of BSSIDs
 *	to ignore in roaming decision. Uses
 *	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS to set the list.
 *
 * @QCA_WLAN_VENDOR_ROAMING_SUBCMD_CONTROL_SET: Command to set the
 *	roam control config to the driver with the attribute
 *	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_CONTROL.
 *
 * @QCA_WLAN_VENDOR_ROAMING_SUBCMD_CONTROL_GET: Command to obtain the
 *	roam control config from driver with the attribute
 *	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_CONTROL.
 *	For the get, the attribute for the configuration to be queried shall
 *	carry any of its acceptable value to the driver. In return, the driver
 *	shall send the configured values within the same attribute to the user
 *	space.
 *
 * @QCA_WLAN_VENDOR_ROAMING_SUBCMD_CONTROL_CLEAR: Command to clear the
 *	roam control config in the driver with the attribute
 *	QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_CONTROL.
 *	The driver shall continue with its default roaming behavior when data
 *	corresponding to an attribute is cleared.
 */
enum qca_wlan_vendor_roaming_subcmd {
	QCA_WLAN_VENDOR_ROAMING_SUBCMD_INVALID = 0,
	QCA_WLAN_VENDOR_ROAMING_SUBCMD_SSID_ALLOW_LIST = 1,
	QCA_WLAN_VENDOR_ROAMING_SUBCMD_SET_GSCAN_ROAM_PARAMS = 2,
	QCA_WLAN_VENDOR_ROAMING_SUBCMD_SET_LAZY_ROAM = 3,
	QCA_WLAN_VENDOR_ROAMING_SUBCMD_SET_BSSID_PREFS = 4,
	QCA_WLAN_VENDOR_ROAMING_SUBCMD_SET_BSSID_PARAMS = 5,
	QCA_WLAN_VENDOR_ROAMING_SUBCMD_SET_DENYLIST_BSSID = 6,
	QCA_WLAN_VENDOR_ROAMING_SUBCMD_CONTROL_SET = 7,
	QCA_WLAN_VENDOR_ROAMING_SUBCMD_CONTROL_GET = 8,
	QCA_WLAN_VENDOR_ROAMING_SUBCMD_CONTROL_CLEAR = 9,
};

/* old names for API compatibility */
#define QCA_WLAN_VENDOR_ROAMING_SUBCMD_SSID_WHITE_LIST \
	QCA_WLAN_VENDOR_ROAMING_SUBCMD_SSID_ALLOW_LIST
#define QCA_WLAN_VENDOR_ROAMING_SUBCMD_SET_BLACKLIST_BSSID \
	QCA_WLAN_VENDOR_ROAMING_SUBCMD_SET_DENYLIST_BSSID

enum qca_wlan_vendor_attr_gscan_config_params {
	QCA_WLAN_VENDOR_ATTR_GSCAN_SUBCMD_CONFIG_PARAM_INVALID = 0,

	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID = 1,

	/* Attributes for data used by
	 * QCA_NL80211_VENDOR_SUBCMD_GSCAN_GET_VALID_CHANNELS sub command.
	 */
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_GET_VALID_CHANNELS_CONFIG_PARAM_WIFI_BAND
	= 2,
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_GET_VALID_CHANNELS_CONFIG_PARAM_MAX_CHANNELS
	= 3,

	/* Attributes for input params used by
	 * QCA_NL80211_VENDOR_SUBCMD_GSCAN_START sub command.
	 */

	/* Unsigned 32-bit value; channel frequency */
	QCA_WLAN_VENDOR_ATTR_GSCAN_CHANNEL_SPEC_CHANNEL = 4,
	/* Unsigned 32-bit value; dwell time in ms. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_CHANNEL_SPEC_DWELL_TIME = 5,
	/* Unsigned 8-bit value; 0: active; 1: passive; N/A for DFS */
	QCA_WLAN_VENDOR_ATTR_GSCAN_CHANNEL_SPEC_PASSIVE = 6,
	/* Unsigned 8-bit value; channel class */
	QCA_WLAN_VENDOR_ATTR_GSCAN_CHANNEL_SPEC_CLASS = 7,

	/* Unsigned 8-bit value; bucket index, 0 based */
	QCA_WLAN_VENDOR_ATTR_GSCAN_BUCKET_SPEC_INDEX = 8,
	/* Unsigned 8-bit value; band. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_BUCKET_SPEC_BAND = 9,
	/* Unsigned 32-bit value; desired period, in ms. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_BUCKET_SPEC_PERIOD = 10,
	/* Unsigned 8-bit value; report events semantics. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_BUCKET_SPEC_REPORT_EVENTS = 11,
	/* Unsigned 32-bit value. Followed by a nested array of
	 * GSCAN_CHANNEL_SPEC_* attributes.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_BUCKET_SPEC_NUM_CHANNEL_SPECS = 12,

	/* Array of QCA_WLAN_VENDOR_ATTR_GSCAN_CHANNEL_SPEC_* attributes.
	 * Array size: QCA_WLAN_VENDOR_ATTR_GSCAN_BUCKET_SPEC_NUM_CHANNEL_SPECS
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_CHANNEL_SPEC = 13,

	/* Unsigned 32-bit value; base timer period in ms. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_SCAN_CMD_PARAMS_BASE_PERIOD = 14,
	/* Unsigned 32-bit value; number of APs to store in each scan in the
	 * BSSID/RSSI history buffer (keep the highest RSSI APs).
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_SCAN_CMD_PARAMS_MAX_AP_PER_SCAN = 15,
	/* Unsigned 8-bit value; in %, when scan buffer is this much full, wake
	 * up AP.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_SCAN_CMD_PARAMS_REPORT_THRESHOLD_PERCENT
	= 16,

	/* Unsigned 8-bit value; number of scan bucket specs; followed by a
	 * nested array of_GSCAN_BUCKET_SPEC_* attributes and values. The size
	 * of the array is determined by NUM_BUCKETS.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_SCAN_CMD_PARAMS_NUM_BUCKETS = 17,

	/* Array of QCA_WLAN_VENDOR_ATTR_GSCAN_BUCKET_SPEC_* attributes.
	 * Array size: QCA_WLAN_VENDOR_ATTR_GSCAN_SCAN_CMD_PARAMS_NUM_BUCKETS
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_BUCKET_SPEC = 18,

	/* Unsigned 8-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_GET_CACHED_SCAN_RESULTS_CONFIG_PARAM_FLUSH
	= 19,
	/* Unsigned 32-bit value; maximum number of results to be returned. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_GET_CACHED_SCAN_RESULTS_CONFIG_PARAM_MAX
	= 20,

	/* An array of 6 x unsigned 8-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_AP_THRESHOLD_PARAM_BSSID = 21,
	/* Signed 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_AP_THRESHOLD_PARAM_RSSI_LOW = 22,
	/* Signed 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_AP_THRESHOLD_PARAM_RSSI_HIGH = 23,
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_AP_THRESHOLD_PARAM_CHANNEL = 24,

	/* Number of hotlist APs as unsigned 32-bit value, followed by a nested
	 * array of AP_THRESHOLD_PARAM attributes and values. The size of the
	 * array is determined by NUM_AP.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_BSSID_HOTLIST_PARAMS_NUM_AP = 25,

	/* Array of QCA_WLAN_VENDOR_ATTR_GSCAN_AP_THRESHOLD_PARAM_* attributes.
	 * Array size: QCA_WLAN_VENDOR_ATTR_GSCAN_BUCKET_SPEC_NUM_CHANNEL_SPECS
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_AP_THRESHOLD_PARAM = 26,

	/* Unsigned 32-bit value; number of samples for averaging RSSI. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_SIGNIFICANT_CHANGE_PARAMS_RSSI_SAMPLE_SIZE
	= 27,
	/* Unsigned 32-bit value; number of samples to confirm AP loss. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_SIGNIFICANT_CHANGE_PARAMS_LOST_AP_SAMPLE_SIZE
	= 28,
	/* Unsigned 32-bit value; number of APs breaching threshold. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_SIGNIFICANT_CHANGE_PARAMS_MIN_BREACHING = 29,
	/* Unsigned 32-bit value; number of APs. Followed by an array of
	 * AP_THRESHOLD_PARAM attributes. Size of the array is NUM_AP.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_SIGNIFICANT_CHANGE_PARAMS_NUM_AP = 30,
	/* Unsigned 32-bit value; number of samples to confirm AP loss. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_BSSID_HOTLIST_PARAMS_LOST_AP_SAMPLE_SIZE
	= 31,
	/* Unsigned 32-bit value. If max_period is non zero or different than
	 * period, then this bucket is an exponential backoff bucket.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_BUCKET_SPEC_MAX_PERIOD = 32,
	/* Unsigned 32-bit value. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_BUCKET_SPEC_BASE = 33,
	/* Unsigned 32-bit value. For exponential back off bucket, number of
	 * scans to perform for a given period.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_BUCKET_SPEC_STEP_COUNT = 34,
	/* Unsigned 8-bit value; in number of scans, wake up AP after these
	 * many scans.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_SCAN_CMD_PARAMS_REPORT_THRESHOLD_NUM_SCANS
	= 35,

	/* Attributes for data used by
	 * QCA_NL80211_VENDOR_SUBCMD_GSCAN_SET_SSID_HOTLIST sub command.
	 */
	/* Unsigned 3-2bit value; number of samples to confirm SSID loss. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_SSID_HOTLIST_PARAMS_LOST_SSID_SAMPLE_SIZE
	= 36,
	/* Number of hotlist SSIDs as unsigned 32-bit value, followed by a
	 * nested array of SSID_THRESHOLD_PARAM_* attributes and values. The
	 * size of the array is determined by NUM_SSID.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_SSID_HOTLIST_PARAMS_NUM_SSID = 37,
	/* Array of QCA_WLAN_VENDOR_ATTR_GSCAN_SSID_THRESHOLD_PARAM_*
	 * attributes.
	 * Array size: QCA_WLAN_VENDOR_ATTR_GSCAN_SSID_HOTLIST_PARAMS_NUM_SSID
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_SSID_THRESHOLD_PARAM = 38,

	/* An array of 33 x unsigned 8-bit value; NULL terminated SSID */
	QCA_WLAN_VENDOR_ATTR_GSCAN_SSID_THRESHOLD_PARAM_SSID = 39,
	/* Unsigned 8-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_SSID_THRESHOLD_PARAM_BAND = 40,
	/* Signed 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_SSID_THRESHOLD_PARAM_RSSI_LOW = 41,
	/* Signed 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_SSID_THRESHOLD_PARAM_RSSI_HIGH = 42,
	/* Unsigned 32-bit value; a bitmask with additional gscan config flag.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_CONFIGURATION_FLAGS = 43,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_GSCAN_SUBCMD_CONFIG_PARAM_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_GSCAN_SUBCMD_CONFIG_PARAM_MAX =
	QCA_WLAN_VENDOR_ATTR_GSCAN_SUBCMD_CONFIG_PARAM_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_gscan_results {
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_INVALID = 0,

	/* Unsigned 32-bit value; must match the request Id supplied by
	 * Wi-Fi HAL in the corresponding subcmd NL msg.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_REQUEST_ID = 1,

	/* Unsigned 32-bit value; used to indicate the status response from
	 * firmware/driver for the vendor sub-command.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_STATUS = 2,

	/* GSCAN Valid Channels attributes */
	/* Unsigned 32bit value; followed by a nested array of CHANNELS. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_NUM_CHANNELS = 3,
	/* An array of NUM_CHANNELS x unsigned 32-bit value integers
	 * representing channel numbers.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_CHANNELS = 4,

	/* GSCAN Capabilities attributes */
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_CAPABILITIES_MAX_SCAN_CACHE_SIZE = 5,
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_CAPABILITIES_MAX_SCAN_BUCKETS = 6,
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_CAPABILITIES_MAX_AP_CACHE_PER_SCAN
	= 7,
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_CAPABILITIES_MAX_RSSI_SAMPLE_SIZE
	= 8,
	/* Signed 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_CAPABILITIES_MAX_SCAN_REPORTING_THRESHOLD
	= 9,
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_CAPABILITIES_MAX_HOTLIST_BSSIDS = 10,
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_CAPABILITIES_MAX_SIGNIFICANT_WIFI_CHANGE_APS
	= 11,
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_CAPABILITIES_MAX_BSSID_HISTORY_ENTRIES
	= 12,

	/* GSCAN Attributes used with
	 * QCA_NL80211_VENDOR_SUBCMD_GSCAN_SCAN_RESULTS_AVAILABLE sub-command.
	 */

	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_NUM_RESULTS_AVAILABLE = 13,

	/* GSCAN attributes used with
	 * QCA_NL80211_VENDOR_SUBCMD_GSCAN_FULL_SCAN_RESULT sub-command.
	 */

	/* An array of NUM_RESULTS_AVAILABLE x
	 * QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SCAN_RESULT_*
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_LIST = 14,

	/* Unsigned 64-bit value; age of sample at the time of retrieval */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SCAN_RESULT_TIME_STAMP = 15,
	/* 33 x unsigned 8-bit value; NULL terminated SSID */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SCAN_RESULT_SSID = 16,
	/* An array of 6 x unsigned 8-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SCAN_RESULT_BSSID = 17,
	/* Unsigned 32-bit value; channel frequency in MHz */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SCAN_RESULT_CHANNEL = 18,
	/* Signed 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SCAN_RESULT_RSSI = 19,
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SCAN_RESULT_RTT = 20,
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SCAN_RESULT_RTT_SD = 21,
	/* Unsigned 16-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SCAN_RESULT_BEACON_PERIOD = 22,
	/* Unsigned 16-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SCAN_RESULT_CAPABILITY = 23,
	/* Unsigned 32-bit value; size of the IE DATA blob */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SCAN_RESULT_IE_LENGTH = 24,
	/* An array of IE_LENGTH x unsigned 8-bit value; blob of all the
	 * information elements found in the beacon; this data should be a
	 * packed list of wifi_information_element objects, one after the
	 * other.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SCAN_RESULT_IE_DATA = 25,

	/* Unsigned 8-bit value; set by driver to indicate more scan results are
	 * available.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SCAN_RESULT_MORE_DATA = 26,

	/* GSCAN attributes for
	 * QCA_NL80211_VENDOR_SUBCMD_GSCAN_SCAN_EVENT sub-command.
	 */
	/* Unsigned 8-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SCAN_EVENT_TYPE = 27,
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SCAN_EVENT_STATUS = 28,

	/* GSCAN attributes for
	 * QCA_NL80211_VENDOR_SUBCMD_GSCAN_HOTLIST_AP_FOUND sub-command.
	 */
	/* Use attr QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_NUM_RESULTS_AVAILABLE
	 * to indicate number of results.
	 * Also, use QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_LIST to indicate the
	 * list of results.
	 */

	/* GSCAN attributes for
	 * QCA_NL80211_VENDOR_SUBCMD_GSCAN_SIGNIFICANT_CHANGE sub-command.
	 */
	/* An array of 6 x unsigned 8-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SIGNIFICANT_CHANGE_RESULT_BSSID = 29,
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SIGNIFICANT_CHANGE_RESULT_CHANNEL
	= 30,
	/* Unsigned 32-bit value. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SIGNIFICANT_CHANGE_RESULT_NUM_RSSI
	= 31,
	/* A nested array of signed 32-bit RSSI values. Size of the array is
	 * determined by (NUM_RSSI of SIGNIFICANT_CHANGE_RESULT_NUM_RSSI.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_SIGNIFICANT_CHANGE_RESULT_RSSI_LIST
	= 32,

	/* GSCAN attributes used with
	 * QCA_NL80211_VENDOR_SUBCMD_GSCAN_GET_CACHED_RESULTS sub-command.
	 */
	/* Use attr QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_NUM_RESULTS_AVAILABLE
	 * to indicate number of gscan cached results returned.
	 * Also, use QCA_WLAN_VENDOR_ATTR_GSCAN_CACHED_RESULTS_LIST to indicate
	 *  the list of gscan cached results.
	 */

	/* An array of NUM_RESULTS_AVAILABLE x
	 * QCA_NL80211_VENDOR_ATTR_GSCAN_CACHED_RESULTS_*
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_CACHED_RESULTS_LIST = 33,
	/* Unsigned 32-bit value; a unique identifier for the scan unit. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_CACHED_RESULTS_SCAN_ID = 34,
	/* Unsigned 32-bit value; a bitmask w/additional information about scan.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_CACHED_RESULTS_FLAGS = 35,
	/* Use attr QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_NUM_RESULTS_AVAILABLE
	 * to indicate number of wifi scan results/bssids retrieved by the scan.
	 * Also, use QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_LIST to indicate the
	 * list of wifi scan results returned for each cached result block.
	 */

	/* GSCAN attributes for
	 * QCA_NL80211_VENDOR_SUBCMD_PNO_NETWORK_FOUND sub-command.
	 */
	/* Use QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_NUM_RESULTS_AVAILABLE for
	 * number of results.
	 * Use QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_LIST to indicate the nested
	 * list of wifi scan results returned for each
	 * wifi_passpoint_match_result block.
	 * Array size: QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_NUM_RESULTS_AVAILABLE.
	 */

	/* GSCAN attributes for
	 * QCA_NL80211_VENDOR_SUBCMD_PNO_PASSPOINT_NETWORK_FOUND sub-command.
	 */
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_PNO_RESULTS_PASSPOINT_NETWORK_FOUND_NUM_MATCHES
	= 36,
	/* A nested array of
	 * QCA_WLAN_VENDOR_ATTR_GSCAN_PNO_RESULTS_PASSPOINT_MATCH_*
	 * attributes. Array size =
	 * *_ATTR_GSCAN_PNO_RESULTS_PASSPOINT_NETWORK_FOUND_NUM_MATCHES.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_PNO_RESULTS_PASSPOINT_MATCH_RESULT_LIST = 37,

	/* Unsigned 32-bit value; network block id for the matched network */
	QCA_WLAN_VENDOR_ATTR_GSCAN_PNO_RESULTS_PASSPOINT_MATCH_ID = 38,
	/* Use QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_LIST to indicate the nested
	 * list of wifi scan results returned for each
	 * wifi_passpoint_match_result block.
	 */
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_GSCAN_PNO_RESULTS_PASSPOINT_MATCH_ANQP_LEN = 39,
	/* An array size of PASSPOINT_MATCH_ANQP_LEN of unsigned 8-bit values;
	 * ANQP data in the information_element format.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_PNO_RESULTS_PASSPOINT_MATCH_ANQP = 40,

	/* Unsigned 32-bit value; a GSCAN Capabilities attribute. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_CAPABILITIES_MAX_HOTLIST_SSIDS = 41,
	/* Unsigned 32-bit value; a GSCAN Capabilities attribute. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_CAPABILITIES_MAX_NUM_EPNO_NETS = 42,
	/* Unsigned 32-bit value; a GSCAN Capabilities attribute. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_CAPABILITIES_MAX_NUM_EPNO_NETS_BY_SSID
	= 43,
	/* Unsigned 32-bit value; a GSCAN Capabilities attribute. */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_CAPABILITIES_MAX_NUM_ALLOWLISTED_SSID
	= 44,

	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_BUCKETS_SCANNED = 45,

	/* Unsigned 32-bit value; a GSCAN Capabilities attribute.
	 * This is used to limit the maximum number of BSSIDs while sending
	 * the vendor command QCA_NL80211_VENDOR_SUBCMD_ROAM with subcmd
	 * QCA_WLAN_VENDOR_ROAMING_SUBCMD_SET_DENYLIST_BSSID and attribute
	 * QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_NUM_BSSID.
	 */
	QCA_WLAN_VENDOR_ATTR_GSCAN_MAX_NUM_DENYLISTED_BSSID = 46,

	/* Attribute used for padding for 64-bit alignment */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_PAD = 47,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_MAX =
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_AFTER_LAST - 1,
};

/* old names for API compatibility */
#define QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_CAPABILITIES_MAX_NUM_WHITELISTED_SSID \
	QCA_WLAN_VENDOR_ATTR_GSCAN_RESULTS_CAPABILITIES_MAX_NUM_ALLOWLISTED_SSID
#define QCA_WLAN_VENDOR_ATTR_GSCAN_MAX_NUM_BLACKLISTED_BSSID \
	QCA_WLAN_VENDOR_ATTR_GSCAN_MAX_NUM_DENYLISTED_BSSID

enum qca_wlan_vendor_attr_pno_config_params {
	QCA_WLAN_VENDOR_ATTR_PNO_INVALID = 0,
	/* Attributes for data used by
	 * QCA_NL80211_VENDOR_SUBCMD_PNO_SET_PASSPOINT_LIST sub command.
	 */
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_LIST_PARAM_NUM = 1,
	/* Array of nested QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_*
	 * attributes. Array size =
	 * QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_LIST_PARAM_NUM.
	 */
	QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_LIST_PARAM_NETWORK_ARRAY = 2,

	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_ID = 3,
	/* An array of 256 x unsigned 8-bit value; NULL terminated UTF-8 encoded
	 * realm, 0 if unspecified.
	 */
	QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_REALM = 4,
	/* An array of 16 x unsigned 32-bit value; roaming consortium ids to
	 * match, 0 if unspecified.
	 */
	QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_ROAM_CNSRTM_ID = 5,
	/* An array of 6 x unsigned 8-bit value; MCC/MNC combination, 0s if
	 * unspecified.
	 */
	QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_ROAM_PLMN = 6,

	/* Attributes for data used by
	 * QCA_NL80211_VENDOR_SUBCMD_PNO_SET_LIST sub command.
	 */
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_NUM_NETWORKS = 7,
	/* Array of nested
	 * QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_*
	 * attributes. Array size =
	 * QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_NUM_NETWORKS.
	 */
	QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORKS_LIST = 8,
	/* An array of 33 x unsigned 8-bit value; NULL terminated SSID */
	QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_SSID = 9,
	/* Signed 8-bit value; threshold for considering this SSID as found,
	 * required granularity for this threshold is 4 dBm to 8 dBm.
	 */
	QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_RSSI_THRESHOLD
	= 10,
	/* Unsigned 8-bit value; WIFI_PNO_FLAG_XXX */
	QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_FLAGS = 11,
	/* Unsigned 8-bit value; auth bit field for matching WPA IE */
	QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_AUTH_BIT = 12,
	/* Unsigned 8-bit to indicate ePNO type;
	 * It takes values from qca_wlan_epno_type
	 */
	QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_TYPE = 13,

	/* Nested attribute to send the channel list */
	QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_CHANNEL_LIST = 14,

	/* Unsigned 32-bit value; indicates the interval between PNO scan
	 * cycles in msec.
	 */
	QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_SCAN_INTERVAL = 15,
	QCA_WLAN_VENDOR_ATTR_EPNO_MIN5GHZ_RSSI = 16,
	QCA_WLAN_VENDOR_ATTR_EPNO_MIN24GHZ_RSSI = 17,
	QCA_WLAN_VENDOR_ATTR_EPNO_INITIAL_SCORE_MAX = 18,
	QCA_WLAN_VENDOR_ATTR_EPNO_CURRENT_CONNECTION_BONUS = 19,
	QCA_WLAN_VENDOR_ATTR_EPNO_SAME_NETWORK_BONUS = 20,
	QCA_WLAN_VENDOR_ATTR_EPNO_SECURE_BONUS = 21,
	QCA_WLAN_VENDOR_ATTR_EPNO_BAND5GHZ_BONUS = 22,
	/* Unsigned 32-bit value, representing the PNO Request ID */
	QCA_WLAN_VENDOR_ATTR_PNO_CONFIG_REQUEST_ID = 23,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_PNO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_PNO_MAX =
	QCA_WLAN_VENDOR_ATTR_PNO_AFTER_LAST - 1,
};

/**
 * qca_wlan_vendor_acs_select_reason: This represents the different reasons why
 * the ACS has to be triggered. These values are used by
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_REASON and
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_REASON
 */
enum qca_wlan_vendor_acs_select_reason {
	/* Represents the reason that the ACS triggered during the AP start */
	QCA_WLAN_VENDOR_ACS_SELECT_REASON_INIT,
	/* Represents the reason that DFS found with the current channel */
	QCA_WLAN_VENDOR_ACS_SELECT_REASON_DFS,
	/* Represents the reason that LTE co-exist in the current band. */
	QCA_WLAN_VENDOR_ACS_SELECT_REASON_LTE_COEX,
	/* Represents the reason that generic, uncategorized interference has
	 * been found in the current channel.
	 */
	QCA_WLAN_VENDOR_ACS_SELECT_REASON_GENERIC_INTERFERENCE,
	/* Represents the reason that excessive 802.11 interference has been
	 * found in the current channel.
	 */
	QCA_WLAN_VENDOR_ACS_SELECT_REASON_80211_INTERFERENCE,
	/* Represents the reason that generic Continuous Wave (CW) interference
	 * has been found in the current channel.
	 */
	QCA_WLAN_VENDOR_ACS_SELECT_REASON_CW_INTERFERENCE,
	/* Represents the reason that Microwave Oven (MWO) interference has been
	 * found in the current channel.
	 */
	QCA_WLAN_VENDOR_ACS_SELECT_REASON_MWO_INTERFERENCE,
	/* Represents the reason that generic Frequency-Hopping Spread Spectrum
	 * (FHSS) interference has been found in the current channel. This may
	 * include 802.11 waveforms.
	 */
	QCA_WLAN_VENDOR_ACS_SELECT_REASON_FHSS_INTERFERENCE,
	/* Represents the reason that non-802.11 generic Frequency-Hopping
	 * Spread Spectrum (FHSS) interference has been found in the current
	 * channel.
	 */
	QCA_WLAN_VENDOR_ACS_SELECT_REASON_NON_80211_FHSS_INTERFERENCE,
	/* Represents the reason that generic Wideband (WB) interference has
	 * been found in the current channel. This may include 802.11 waveforms.
	 */
	QCA_WLAN_VENDOR_ACS_SELECT_REASON_WB_INTERFERENCE,
	/* Represents the reason that non-802.11 generic Wideband (WB)
	 * interference has been found in the current channel.
	 */
	QCA_WLAN_VENDOR_ACS_SELECT_REASON_NON_80211_WB_INTERFERENCE,
	/* Represents the reason that Jammer interference has been found in the
	 * current channel.
	 */
	QCA_WLAN_VENDOR_ACS_SELECT_REASON_JAMMER_INTERFERENCE,
	/* Represents the reason that ACS triggered by AFC */
	QCA_WLAN_VENDOR_ACS_SELECT_REASON_AFC_TRIGGER,
};

/**
 * qca_wlan_vendor_attr_external_acs_policy: Attribute values for
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_POLICY to the vendor subcmd
 * QCA_NL80211_VENDOR_SUBCMD_EXTERNAL_ACS. This represents the
 * external ACS policies to select the channels w.r.t. the PCL weights.
 * (QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_PCL represents the channels and
 * their PCL weights.)
 * @QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_POLICY_PCL_MANDATORY: Mandatory to
 * select a channel with non-zero PCL weight.
 * @QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_POLICY_PCL_PREFERRED: Prefer a
 * channel with non-zero PCL weight.
 *
 */
enum qca_wlan_vendor_attr_external_acs_policy {
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_POLICY_PCL_PREFERRED,
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_POLICY_PCL_MANDATORY,
};

/**
 * qca_wlan_vendor_channel_prop_flags: This represent the flags for a channel.
 * This is used by QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FLAGS.
 */
enum qca_wlan_vendor_channel_prop_flags {
	/* Bits 0, 1, 2, and 3 are reserved */

	/* Turbo channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_TURBO         = 1 << 4,
	/* CCK channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_CCK           = 1 << 5,
	/* OFDM channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_OFDM          = 1 << 6,
	/* 2.4 GHz spectrum channel. */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_2GHZ          = 1 << 7,
	/* 5 GHz spectrum channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_5GHZ          = 1 << 8,
	/* Only passive scan allowed */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_PASSIVE       = 1 << 9,
	/* Dynamic CCK-OFDM channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_DYN           = 1 << 10,
	/* GFSK channel (FHSS PHY) */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_GFSK          = 1 << 11,
	/* Radar found on channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_RADAR         = 1 << 12,
	/* 11a static turbo channel only */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_STURBO        = 1 << 13,
	/* Half rate channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HALF          = 1 << 14,
	/* Quarter rate channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_QUARTER       = 1 << 15,
	/* HT 20 channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HT20          = 1 << 16,
	/* HT 40 with extension channel above */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HT40PLUS      = 1 << 17,
	/* HT 40 with extension channel below */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HT40MINUS     = 1 << 18,
	/* HT 40 intolerant */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HT40INTOL     = 1 << 19,
	/* VHT 20 channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_VHT20         = 1 << 20,
	/* VHT 40 with extension channel above */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_VHT40PLUS     = 1 << 21,
	/* VHT 40 with extension channel below */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_VHT40MINUS    = 1 << 22,
	/* VHT 80 channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_VHT80         = 1 << 23,
	/* HT 40 intolerant mark bit for ACS use */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HT40INTOLMARK = 1 << 24,
	/* Channel temporarily blocked due to noise */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_BLOCKED       = 1 << 25,
	/* VHT 160 channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_VHT160        = 1 << 26,
	/* VHT 80+80 channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_VHT80_80      = 1 << 27,
	/* HE 20 channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HE20          = 1 << 28,
	/* HE 40 with extension channel above */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HE40PLUS      = 1 << 29,
	/* HE 40 with extension channel below */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HE40MINUS     = 1 << 30,
	/* HE 40 intolerant */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HE40INTOL     = 1U << 31,
};

/**
 * qca_wlan_vendor_channel_prop_flags_2: This represents the flags for a
 * channel, and is a continuation of qca_wlan_vendor_channel_prop_flags. This is
 * used by QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FLAGS_2.
 */
enum qca_wlan_vendor_channel_prop_flags_2 {
	/* HE 40 intolerant mark bit for ACS use */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HE40INTOLMARK = 1 << 0,
	/* HE 80 channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HE80          = 1 << 1,
	/* HE 160 channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HE160         = 1 << 2,
	/* HE 80+80 channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HE80_80       = 1 << 3,
};

/**
 * qca_wlan_vendor_channel_prop_flags_ext: This represent the extended flags for
 * each channel. This is used by
 * QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FLAG_EXT.
 */
enum qca_wlan_vendor_channel_prop_flags_ext {
	/* Radar found on channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_EXT_RADAR_FOUND     = 1 << 0,
	/* DFS required on channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_EXT_DFS             = 1 << 1,
	/* DFS required on channel for 2nd band of 80+80 */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_EXT_DFS_CFREQ2      = 1 << 2,
	/* If channel has been checked for DFS */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_EXT_DFS_CLEAR       = 1 << 3,
	/* Excluded in 802.11d */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_EXT_11D_EXCLUDED    = 1 << 4,
	/* Channel Switch Announcement received on this channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_EXT_CSA_RECEIVED    = 1 << 5,
	/* Ad-hoc is not allowed */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_EXT_DISALLOW_ADHOC  = 1 << 6,
	/* Station only channel */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_EXT_DISALLOW_HOSTAP = 1 << 7,
	/* DFS radar history for client device (STA mode) */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_EXT_HISTORY_RADAR   = 1 << 8,
	/* DFS CAC valid for client device (STA mode) */
	QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_EXT_CAC_VALID       = 1 << 9,
};

/**
 * qca_wlan_vendor_external_acs_event_chan_info_attr: Represents per channel
 * information. These attributes are sent as part of
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_CHAN_INFO. Each set of the following
 * attributes correspond to a single channel.
 */
enum qca_wlan_vendor_external_acs_event_chan_info_attr {
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_INVALID = 0,

	/* A bitmask (u32) with flags specified in
	 * enum qca_wlan_vendor_channel_prop_flags.
	 */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FLAGS = 1,
	/* A bitmask (u32) with flags specified in
	 * enum qca_wlan_vendor_channel_prop_flags_ext.
	 */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FLAG_EXT = 2,
	/* frequency in MHz (u32) */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ = 3,
	/* maximum regulatory transmission power (u32) */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_MAX_REG_POWER = 4,
	/* maximum transmission power (u32) */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_MAX_POWER = 5,
	/* minimum transmission power (u32) */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_MIN_POWER = 6,
	/* regulatory class id (u8) */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_REG_CLASS_ID = 7,
	/* maximum antenna gain in (u8) */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_ANTENNA_GAIN = 8,
	/* VHT segment 0 (u8) */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_VHT_SEG_0 = 9,
	/* VHT segment 1 (u8) */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_VHT_SEG_1 = 10,
	/* A bitmask (u32) with flags specified in
	 * enum qca_wlan_vendor_channel_prop_flags_2.
	 */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FLAGS_2 = 11,

	/*
	 * Segment 0 in MHz (u32).
	 *
	 * For 20/40/80 MHz bandwidth, this indicates the channel center
	 * frequency index for the 20/40/80 MHz operating channel.
	 * For 160 MHz bandwidth, this indicates the channel center
	 * frequency of the primary 80 MHz channel.
	 * For 320 MHz bandwidth, indicates the channel center frequency
	 * of the primary 160 MHz channel.
	 *
	 * To maintain backward compatibility,
	 * QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ_VHT_SEG_0
	 * is also maintained.
	 */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ_SEG_0 = 12,
	/* Legacy alias for the Segment 0 attribute.
	 *
	 * VHT segment 0 in MHz (u32) and the attribute is mandatory.
	 * Note: Event QCA_NL80211_VENDOR_SUBCMD_EXTERNAL_ACS includes
	 * QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ_VHT_SEG_0
	 * along with
	 * QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_VHT_SEG_0.
	 *
	 * If both the driver and user-space application supports the 6 GHz
	 * band, QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_VHT_SEG_0
	 * is deprecated and
	 * QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ_VHT_SEG_0
	 * should be used.
	 *
	 * To maintain backward compatibility,
	 * QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ_VHT_SEG_0
	 * is still used if either of the driver or user space application
	 * doesn't support the 6 GHz band.
	 */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ_VHT_SEG_0 =
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ_SEG_0,

	/*
	 * Segment 1 in MHz (u32).
	 *
	 * For 20/40/80 MHz bandwidth, this is set to 0.
	 * For 160 MHz bandwidth, indicates the channel center frequency of the
	 * 160 MHz channel.
	 * For 320 MHz bandwidth, indicates the channel center frequency of the
	 * 320 MHz channel.
	 *
	 * To maintain backward compatibility,
	 * QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ_VHT_SEG_1
	 * is also maintained.
	 */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ_SEG_1 = 13,
	/* Legacy alias for the Segment 1 attribute.
	 *
	 * VHT segment 1 in MHz (u32) and the attribute is mandatory.
	 * Note: Event QCA_NL80211_VENDOR_SUBCMD_EXTERNAL_ACS includes
	 * QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ_VHT_SEG_1
	 * along with
	 * QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_VHT_SEG_1.
	 *
	 * If both the driver and user-space application supports the 6 GHz
	 * band, QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_VHT_SEG_1
	 * is deprecated and
	 * QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ_VHT_SEG_1
	 * should be considered.
	 *
	 * To maintain backward compatibility,
	 * QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ_VHT_SEG_1
	 * is still used if either of the driver or user space application
	 * doesn't support the 6 GHz band.
	 */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ_VHT_SEG_1 =
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ_SEG_1,

	/*
	 * 16-bit attribute of bits indicating the AP power modes supported by
	 * the channel (u16).
	 * Note: Currently, only 3 bits are used in the attribute and each bit
	 * corresponds to the power mode mentioned in enum
	 * qca_wlan_vendor_external_acs_chan_power_mode and a given bit is
	 * set if the associated mode is supported.
	 */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_SUPP_POWER_MODES
									= 14,
	/* Array of nested attributes for each power mode. It takes attr as
	 * defined in enum
	 * qca_wlan_vendor_external_acs_event_chan_power_info_attr.
	 */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_POWER_INFO_ATTR	= 15,
	/*
	 * This indicates the overlapping 320 MHz center frequency in MHz
	 * (u32), if the given primary channel supports more than one
	 * 320 MHz channel bonding.
	 *
	 * Example:
	 * For 6 GHz, channel frequency 6115 MHz (channel number 33) segment 0
	 * center frequency (primary 160 MHz) is 6185 MHz and there can be two
	 * possible segment 2 frequencies for this (320 MHz center
	 * frequencies):
	 *
	 * 1) Center frequency 6105 MHz (channel 31): 320 MHz channel bonding
	 *    from frequency 5945 MHz - 6265 MHz
	 * 2) Center frequency 6265 MHz (channel 63): 320 MHz channel bonding
	 *    from frequency 6105 MHz - 6425 MHz
	 *
	 * In this case,
	 * QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ_SEG_0 will
	 * return 6185 MHz.
	 * QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ_SEG_1 will
	 * return 6105 MHz.
	 * QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ_OVERLAP_SEG_1
	 * will return 6265 MHz.
	 */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ_OVERLAP_SEG_1
									= 16,

	/* keep last */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_LAST,
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_MAX =
		QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_LAST - 1,
};

/**
 * qca_wlan_vendor_external_acs_chan_power_mode - Specifies the valid
 * values that the vendor external ACS channel power attribute
 * QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_POWER_INFO_ATTR_POWER_MODE can
 * take.
 * @QCA_WLAN_VENDOR_EXTERNAL_ACS_CHAN_LOW_POWER: Low power/Indoor mode
 * @QCA_WLAN_VENDOR_EXTERNAL_ACS_CHAN_STANDARD_POWER: Standard power mode
 * @QCA_WLAN_VENDOR_EXTERNAL_ACS_CHAN_VERY_LOW_POWER: Very low power mode
 */
enum qca_wlan_vendor_external_acs_chan_power_level {
	QCA_WLAN_VENDOR_EXTERNAL_ACS_CHAN_LOW_POWER = 0,
	QCA_WLAN_VENDOR_EXTERNAL_ACS_CHAN_STANDARD_POWER = 1,
	QCA_WLAN_VENDOR_EXTERNAL_ACS_CHAN_VERY_LOW_POWER = 2,
};

/**
 * qca_wlan_vendor_external_acs_event_chan_power_info_attr: Represents nested
 * attributes for power mode type and power values corresponding to that.
 * These attributes are sent as part of
 * QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_POWER_INFO_ATTR.
 */
enum qca_wlan_vendor_external_acs_event_chan_power_info_attr {
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_POWER_INFO_ATTR_INVALID = 0,
	/*
	 * Power mode (u8) takes the values defined in enum
	 * qca_wlan_vendor_external_acs_chan_power_mode
	 */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_POWER_INFO_ATTR_POWER_MODE
									= 1,
	/*
	 * Indicates if power value is a PSD/EIRP value (flag). If flag is
	 * present, it indicates a PSD value.
	 */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_POWER_INFO_ATTR_PSD_FLAG = 2,
	/*
	 * Power value (u32) PSD/EIRP as indicated by
	 * QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_POWER_INFO_ATTR_PSD_FLAG,
	 * for power mode corresponding to the
	 * QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_POWER_INFO_ATTR_POWER_MODE.
	 * Units for PSD - dBm/MHz
	 * Units for EIRP - dBm
	 */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_POWER_INFO_ATTR_POWER_VALUE
									= 3,
	/* keep last */
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_POWER_INFO_ATTR_LAST,
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_POWER_INFO_ATTR_MAX =
	QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_POWER_INFO_ATTR_LAST - 1,
};

/**
 * qca_wlan_vendor_attr_pcl: Represents attributes for
 * preferred channel list (PCL). These attributes are sent as part of
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_PCL and
 * QCA_NL80211_VENDOR_SUBCMD_GET_PREFERRED_FREQ_LIST.
 */
enum qca_wlan_vendor_attr_pcl {
	QCA_WLAN_VENDOR_ATTR_PCL_INVALID = 0,

	/* Channel number (u8) */
	QCA_WLAN_VENDOR_ATTR_PCL_CHANNEL = 1,
	/* Channel weightage (u8) */
	QCA_WLAN_VENDOR_ATTR_PCL_WEIGHT = 2,
	/* Channel frequency (u32) in MHz */
	QCA_WLAN_VENDOR_ATTR_PCL_FREQ = 3,
	/* Channel flags (u32)
	 * bit 0 set: channel to be used for GO role,
	 * bit 1 set: channel to be used on CLI role,
	 * bit 2 set: channel must be considered for operating channel
	 *                 selection & peer chosen operating channel should be
	 *                 one of the channels with this flag set,
	 * bit 3 set: channel should be excluded in GO negotiation
	 */
	QCA_WLAN_VENDOR_ATTR_PCL_FLAG = 4,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_PCL_LAST,
	QCA_WLAN_VENDOR_ATTR_PCL_MAX = QCA_WLAN_VENDOR_ATTR_PCL_LAST - 1
};

/**
 * qca_wlan_vendor_attr_external_acs_event: Attribute to vendor sub-command
 * QCA_NL80211_VENDOR_SUBCMD_EXTERNAL_ACS. This attribute will be sent by
 * host driver.
 */
enum qca_wlan_vendor_attr_external_acs_event {
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_INVALID = 0,

	/* This reason (u8) refers to enum qca_wlan_vendor_acs_select_reason.
	 * This helps ACS module to understand why ACS needs to be started.
	 */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_REASON = 1,
	/* Flag attribute to indicate if driver supports spectral scanning */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_IS_SPECTRAL_SUPPORTED = 2,
	/* Flag attribute to indicate if 11ac is offloaded to firmware */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_IS_OFFLOAD_ENABLED = 3,
	/* Flag attribute to indicate if driver provides additional channel
	 * capability as part of scan operation
	 */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_ADD_CHAN_STATS_SUPPORT = 4,
	/* Flag attribute to indicate interface status is UP */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_AP_UP = 5,
	/* Operating mode (u8) of interface. Takes one of enum nl80211_iftype
	 * values.
	 */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_SAP_MODE = 6,
	/* Channel width (u8). It takes one of enum nl80211_chan_width values.
	 * This is the upper bound of channel width. ACS logic should try to get
	 * a channel with the specified width and if not found, look for lower
	 * values.
	 */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_CHAN_WIDTH = 7,
	/* This (u8) will hold values of one of enum nl80211_bands */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_BAND = 8,
	/* PHY/HW mode (u8). Takes one of enum qca_wlan_vendor_acs_hw_mode
	 * values
	 */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_PHY_MODE = 9,
	/* Array of (u32) supported frequency list among which ACS should choose
	 * best frequency.
	 */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_FREQ_LIST = 10,
	/* Preferred channel list by the driver which will have array of nested
	 * values as per enum qca_wlan_vendor_attr_pcl attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_PCL = 11,
	/* Array of nested attribute for each channel. It takes attr as defined
	 * in enum qca_wlan_vendor_external_acs_event_chan_info_attr.
	 */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_CHAN_INFO = 12,
	/* External ACS policy such as PCL mandatory, PCL preferred, etc.
	 * It uses values defined in enum
	 * qca_wlan_vendor_attr_external_acs_policy.
	 */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_POLICY = 13,
	/* Reference RF Operating Parameter (RROP) availability information
	 * (u16). It uses values defined in enum
	 * qca_wlan_vendor_attr_rropavail_info.
	 */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_RROPAVAIL_INFO = 14,
	/* Flag attribute to indicate if driver supports 6 GHz AFC trigger
	 * for External ACS
	 */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_AFC_CAPABILITY = 15,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_LAST,
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_MAX =
		QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_external_acs_channels: Attributes to vendor subcmd
 * QCA_NL80211_VENDOR_SUBCMD_EXTERNAL_ACS. This carries a list of channels
 * in priority order as decided after ACS operation in userspace.
 *
 * @QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_REASON: Required (u8).
 * One of reason code from enum qca_wlan_vendor_acs_select_reason.
 *
 * @QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_LIST: Required
 * Array of nested values for each channel with following attributes:
 *     QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_PRIMARY,
 *     QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_SECONDARY,
 *     QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_CENTER_SEG0,
 *     QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_CENTER_SEG1,
 *     QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_WIDTH
 * Note: If both the driver and user-space application supports the 6 GHz band,
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_LIST is deprecated and use
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_LIST.
 * To maintain backward compatibility,
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_LIST
 * is still used if either of the driver or user space application doesn't
 * support the 6 GHz band.
 *
 * @QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_PRIMARY: Required (u8).
 * Primary channel number
 * Note: If both the driver and user-space application supports the 6 GHz band,
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_PRIMARY is deprecated and use
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_PRIMARY.
 * To maintain backward compatibility,
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_PRIMARY
 * is still used if either of the driver or user space application doesn't
 * support the 6 GHz band.
 *
 * @QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_SECONDARY: Required (u8).
 * Secondary channel number, required only for 160 and 80+80 MHz bandwidths.
 * Note: If both the driver and user-space application supports the 6 GHz band,
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_SECONDARY is deprecated and use
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_SECONDARY.
 * To maintain backward compatibility,
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_SECONDARY
 * is still used if either of the driver or user space application
 * doesn't support the 6 GHz band.
 *
 * @QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_CENTER_SEG0: Required (u8).
 * VHT seg0 channel number
 * Note: If both the driver and user-space application supports the 6 GHz band,
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_CENTER_SEG0 is deprecated and use
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_CENTER_SEG0.
 * To maintain backward compatibility,
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_CENTER_SEG0
 * is still used if either of the driver or user space application
 * doesn't support the 6 GHz band.
 *
 * @QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_CENTER_SEG1: Required (u8).
 * VHT seg1 channel number
 * Note: If both the driver and user-space application supports the 6 GHz band,
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_CENTER_SEG1 is deprecated and use
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_CENTER_SEG1.
 * To maintain backward compatibility,
 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_CENTER_SEG1
 * is still used if either of the driver or user space application
 * doesn't support the 6 GHz band.
 *
 * @QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_WIDTH: Required (u8).
 * Takes one of enum nl80211_chan_width values.
 *
 * @QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_LIST: Required
 * Array of nested values for each channel with following attributes:
 *	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_PRIMARY in MHz (u32),
 *	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_SECONDARY in MHz (u32),
 *	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_CENTER_SEG0 in MHz (u32),
 *	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_CENTER_SEG1 in MHz (u32),
 *	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_WIDTH
 * Note: If user-space application has no support of the 6 GHz band, this
 * attribute is optional.
 *
 * @QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_PRIMARY: Required (u32)
 * Primary channel frequency in MHz
 * Note: If user-space application has no support of the 6 GHz band, this
 * attribute is optional.
 *
 * @QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_SECONDARY: Required (u32)
 * Secondary channel frequency in MHz used for HT 40 MHz channels.
 * Note: If user-space application has no support of the 6 GHz band, this
 * attribute is optional.
 *
 * @QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_CENTER_SEG0: Required (u32)
 * VHT seg0 channel frequency in MHz
 * Note: If user-space application has no support of the 6GHz band, this
 * attribute is optional.
 *
 * @QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_CENTER_SEG1: Required (u32)
 * VHT seg1 channel frequency in MHz
 * Note: If user-space application has no support of the 6 GHz band, this
 * attribute is optional.
 * @QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_PUNCTURE_BITMAP: Required (u16)
 * Puncture Bitmap for selected primary channel. Optional if no support
 * for EHT (IEEE 802.11be). Encoding for this attribute follows the
 * convention used in the Disabled Subchannel Bitmap field of the EHT Operation
 * element.
 * @QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_LINK_ID: Mandatory on AP MLD (u8).
 * Used with command to configure external ACS operation for a specific link
 * affiliated to an AP MLD.
 */
enum qca_wlan_vendor_attr_external_acs_channels {
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_INVALID = 0,

	/* One of reason code (u8) from enum qca_wlan_vendor_acs_select_reason
	 */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_REASON = 1,

	/* Array of nested values for each channel with following attributes:
	 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_BAND,
	 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_PRIMARY,
	 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_SECONDARY,
	 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_CENTER_SEG0,
	 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_CENTER_SEG1,
	 * QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_WIDTH
	 */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_LIST = 2,
	/* This (u8) will hold values of one of enum nl80211_bands */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_BAND = 3,
	/* Primary channel (u8) */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_PRIMARY = 4,
	/* Secondary channel (u8) used for HT 40 MHz channels */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_SECONDARY = 5,
	/* VHT seg0 channel (u8) */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_CENTER_SEG0 = 6,
	/* VHT seg1 channel (u8) */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_CENTER_SEG1 = 7,
	/* Channel width (u8). Takes one of enum nl80211_chan_width values. */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_WIDTH = 8,

	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_LIST = 9,
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_PRIMARY = 10,
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_SECONDARY = 11,
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_CENTER_SEG0 = 12,
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_FREQUENCY_CENTER_SEG1 = 13,
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_PUNCTURE_BITMAP = 14,
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_LINK_ID = 15,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_LAST,
	QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_MAX =
		QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_LAST - 1
};

enum qca_chip_power_save_failure_reason {
	/* Indicates if the reason for the failure is due to a protocol
	 * layer/module.
	 */
	QCA_CHIP_POWER_SAVE_FAILURE_REASON_PROTOCOL = 0,
	/* Indicates if the reason for the failure is due to a hardware issue.
	 */
	QCA_CHIP_POWER_SAVE_FAILURE_REASON_HARDWARE = 1,
};

/**
 * qca_attr_chip_power_save_failure: Attributes to vendor subcmd
 * QCA_NL80211_VENDOR_SUBCMD_CHIP_PWRSAVE_FAILURE. This carries the requisite
 * information leading to the power save failure.
 */
enum qca_attr_chip_power_save_failure {
	QCA_ATTR_CHIP_POWER_SAVE_FAILURE_INVALID = 0,
	/* Reason to cause the power save failure.
	 * These reasons are represented by
	 * enum qca_chip_power_save_failure_reason.
	 */
	QCA_ATTR_CHIP_POWER_SAVE_FAILURE_REASON = 1,

	/* keep last */
	QCA_ATTR_CHIP_POWER_SAVE_FAILURE_LAST,
	QCA_ATTR_CHIP_POWER_SAVE_FAILURE_MAX =
		QCA_ATTR_CHIP_POWER_SAVE_FAILURE_LAST - 1,
};

/**
 * qca_wlan_vendor_nud_stats_data_pkt_flags: Flag representing the various
 * data types for which the stats have to get collected.
 */
enum qca_wlan_vendor_nud_stats_data_pkt_flags {
	QCA_WLAN_VENDOR_NUD_STATS_DATA_ARP = 1 << 0,
	QCA_WLAN_VENDOR_NUD_STATS_DATA_DNS = 1 << 1,
	QCA_WLAN_VENDOR_NUD_STATS_DATA_TCP_HANDSHAKE = 1 << 2,
	QCA_WLAN_VENDOR_NUD_STATS_DATA_ICMPV4 = 1 << 3,
	QCA_WLAN_VENDOR_NUD_STATS_DATA_ICMPV6 = 1 << 4,
	/* Used by QCA_ATTR_NUD_STATS_PKT_TYPE only in nud stats get
	 * to represent the stats of respective data type.
	 */
	QCA_WLAN_VENDOR_NUD_STATS_DATA_TCP_SYN = 1 << 5,
	QCA_WLAN_VENDOR_NUD_STATS_DATA_TCP_SYN_ACK = 1 << 6,
	QCA_WLAN_VENDOR_NUD_STATS_DATA_TCP_ACK = 1 << 7,
};

enum qca_wlan_vendor_nud_stats_set_data_pkt_info {
	QCA_ATTR_NUD_STATS_DATA_PKT_INFO_INVALID = 0,
	/* Represents the data packet type to be monitored (u32).
	 * Host driver tracks the stats corresponding to each data frame
	 * represented by these flags.
	 * These data packets are represented by
	 * enum qca_wlan_vendor_nud_stats_data_pkt_flags
	 */
	QCA_ATTR_NUD_STATS_DATA_PKT_INFO_TYPE = 1,
	/* Name corresponding to the DNS frame for which the respective DNS
	 * stats have to get monitored (string). Max string length 255.
	 */
	QCA_ATTR_NUD_STATS_DATA_PKT_INFO_DNS_DOMAIN_NAME = 2,
	/* source port on which the respective proto stats have to get
	 * collected (u32).
	 */
	QCA_ATTR_NUD_STATS_DATA_PKT_INFO_SRC_PORT = 3,
	/* destination port on which the respective proto stats have to get
	 * collected (u32).
	 */
	QCA_ATTR_NUD_STATS_DATA_PKT_INFO_DEST_PORT = 4,
	/* IPv4 address for which the destined data packets have to be
	 * monitored. (in network byte order), u32.
	 */
	QCA_ATTR_NUD_STATS_DATA_PKT_INFO_DEST_IPV4 = 5,
	/* IPv6 address for which the destined data packets have to be
	 * monitored. (in network byte order), 16 bytes array.
	 */
	QCA_ATTR_NUD_STATS_DATA_PKT_INFO_DEST_IPV6 = 6,

	QCA_ATTR_NUD_STATS_DATA_PKT_INFO_LAST,
	QCA_ATTR_NUD_STATS_DATA_PKT_INFO_MAX =
		QCA_ATTR_NUD_STATS_DATA_PKT_INFO_LAST - 1,
};

/**
 * qca_wlan_vendor_attr_nud_stats_set: Attributes to vendor subcmd
 * QCA_NL80211_VENDOR_SUBCMD_NUD_STATS_SET. This carries the requisite
 * information to start/stop the NUD statistics collection.
 */
enum qca_attr_nud_stats_set {
	QCA_ATTR_NUD_STATS_SET_INVALID = 0,

	/* Flag to start/stop the NUD statistics collection.
	 * Start - If included, Stop - If not included
	 */
	QCA_ATTR_NUD_STATS_SET_START = 1,
	/* IPv4 address of the default gateway (in network byte order), u32 */
	QCA_ATTR_NUD_STATS_GW_IPV4 = 2,
	/* Represents the list of data packet types to be monitored.
	 * Host driver tracks the stats corresponding to each data frame
	 * represented by these flags.
	 * These data packets are represented by
	 * enum qca_wlan_vendor_nud_stats_set_data_pkt_info
	 */
	QCA_ATTR_NUD_STATS_SET_DATA_PKT_INFO = 3,

	/* keep last */
	QCA_ATTR_NUD_STATS_SET_LAST,
	QCA_ATTR_NUD_STATS_SET_MAX =
		QCA_ATTR_NUD_STATS_SET_LAST - 1,
};

enum qca_attr_nud_data_stats {
	QCA_ATTR_NUD_DATA_STATS_INVALID = 0,
	/* Data packet type for which the stats are collected (u32).
	 * Represented by enum qca_wlan_vendor_nud_stats_data_pkt_flags
	 */
	QCA_ATTR_NUD_STATS_PKT_TYPE = 1,
	/* Name corresponding to the DNS frame for which the respective DNS
	 * stats are monitored (string). Max string length 255.
	 */
	QCA_ATTR_NUD_STATS_PKT_DNS_DOMAIN_NAME = 2,
	/* source port on which the respective proto stats are collected (u32).
	 */
	QCA_ATTR_NUD_STATS_PKT_SRC_PORT = 3,
	/* destination port on which the respective proto stats are collected
	 * (u32).
	 */
	QCA_ATTR_NUD_STATS_PKT_DEST_PORT = 4,
	/* IPv4 address for which the destined data packets have to be
	 * monitored. (in network byte order), u32.
	 */
	QCA_ATTR_NUD_STATS_PKT_DEST_IPV4 = 5,
	/* IPv6 address for which the destined data packets have to be
	 * monitored. (in network byte order), 16 bytes array.
	 */
	QCA_ATTR_NUD_STATS_PKT_DEST_IPV6 = 6,
	/* Data packet Request count received from netdev (u32). */
	QCA_ATTR_NUD_STATS_PKT_REQ_COUNT_FROM_NETDEV = 7,
	/* Data packet Request count sent to lower MAC from upper MAC (u32). */
	QCA_ATTR_NUD_STATS_PKT_REQ_COUNT_TO_LOWER_MAC = 8,
	/* Data packet Request count received by lower MAC from upper MAC
	 * (u32)
	 */
	QCA_ATTR_NUD_STATS_PKT_REQ_RX_COUNT_BY_LOWER_MAC = 9,
	/* Data packet Request count successfully transmitted by the device
	 * (u32)
	 */
	QCA_ATTR_NUD_STATS_PKT_REQ_COUNT_TX_SUCCESS = 10,
	/* Data packet Response count received by lower MAC (u32) */
	QCA_ATTR_NUD_STATS_PKT_RSP_RX_COUNT_BY_LOWER_MAC = 11,
	/* Data packet Response count received by upper MAC (u32) */
	QCA_ATTR_NUD_STATS_PKT_RSP_RX_COUNT_BY_UPPER_MAC = 12,
	/* Data packet Response count delivered to netdev (u32) */
	QCA_ATTR_NUD_STATS_PKT_RSP_COUNT_TO_NETDEV = 13,
	/* Data Packet Response count that are dropped out of order (u32) */
	QCA_ATTR_NUD_STATS_PKT_RSP_COUNT_OUT_OF_ORDER_DROP = 14,

	/* keep last */
	QCA_ATTR_NUD_DATA_STATS_LAST,
	QCA_ATTR_NUD_DATA_STATS_MAX =
		QCA_ATTR_NUD_DATA_STATS_LAST - 1,
};

/**
 * qca_attr_nud_stats_get: Attributes to vendor subcmd
 * QCA_NL80211_VENDOR_SUBCMD_NUD_STATS_GET. This carries the requisite
 * NUD statistics collected when queried.
 */
enum qca_attr_nud_stats_get {
	QCA_ATTR_NUD_STATS_GET_INVALID = 0,
	/* ARP Request count from netdev (u32) */
	QCA_ATTR_NUD_STATS_ARP_REQ_COUNT_FROM_NETDEV = 1,
	/* ARP Request count sent to lower MAC from upper MAC (u32) */
	QCA_ATTR_NUD_STATS_ARP_REQ_COUNT_TO_LOWER_MAC = 2,
	/* ARP Request count received by lower MAC from upper MAC (u32) */
	QCA_ATTR_NUD_STATS_ARP_REQ_RX_COUNT_BY_LOWER_MAC = 3,
	/* ARP Request count successfully transmitted by the device (u32) */
	QCA_ATTR_NUD_STATS_ARP_REQ_COUNT_TX_SUCCESS = 4,
	/* ARP Response count received by lower MAC (u32) */
	QCA_ATTR_NUD_STATS_ARP_RSP_RX_COUNT_BY_LOWER_MAC = 5,
	/* ARP Response count received by upper MAC (u32) */
	QCA_ATTR_NUD_STATS_ARP_RSP_RX_COUNT_BY_UPPER_MAC = 6,
	/* ARP Response count delivered to netdev (u32) */
	QCA_ATTR_NUD_STATS_ARP_RSP_COUNT_TO_NETDEV = 7,
	/* ARP Response count dropped due to out of order reception (u32) */
	QCA_ATTR_NUD_STATS_ARP_RSP_COUNT_OUT_OF_ORDER_DROP = 8,
	/* Flag indicating if the station's link to the AP is active.
	 * Active Link - If included, Inactive link - If not included
	 */
	QCA_ATTR_NUD_STATS_AP_LINK_ACTIVE = 9,
	/* Flag indicating if there is any duplicate address detected (DAD).
	 * Yes - If detected, No - If not detected.
	 */
	QCA_ATTR_NUD_STATS_IS_DAD = 10,
	/* List of Data packet types for which the stats are requested.
	 * This list does not carry ARP stats as they are done by the
	 * above attributes. Represented by enum qca_attr_nud_data_stats.
	 */
	QCA_ATTR_NUD_STATS_DATA_PKT_STATS = 11,

	/* keep last */
	QCA_ATTR_NUD_STATS_GET_LAST,
	QCA_ATTR_NUD_STATS_GET_MAX =
		QCA_ATTR_NUD_STATS_GET_LAST - 1,
};

enum qca_wlan_btm_candidate_status {
	QCA_STATUS_ACCEPT = 0,
	QCA_STATUS_REJECT_EXCESSIVE_FRAME_LOSS_EXPECTED = 1,
	QCA_STATUS_REJECT_EXCESSIVE_DELAY_EXPECTED = 2,
	QCA_STATUS_REJECT_INSUFFICIENT_QOS_CAPACITY = 3,
	QCA_STATUS_REJECT_LOW_RSSI = 4,
	QCA_STATUS_REJECT_HIGH_INTERFERENCE = 5,
	QCA_STATUS_REJECT_UNKNOWN = 6,
};

enum qca_wlan_vendor_attr_btm_candidate_info {
	QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_INVALID = 0,

	/* 6-byte MAC address representing the BSSID of transition candidate */
	QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_BSSID = 1,
	/* Unsigned 32-bit value from enum qca_wlan_btm_candidate_status
	 * returned by the driver. It says whether the BSSID provided in
	 * QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_BSSID is acceptable by
	 * the driver, if not it specifies the reason for rejection.
	 * Note that the user-space can overwrite the transition reject reason
	 * codes provided by driver based on more information.
	 */
	QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_STATUS = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_AFTER_LAST - 1,
};

enum qca_attr_trace_level {
	QCA_ATTR_TRACE_LEVEL_INVALID = 0,
	/*
	 * Nested array of the following attributes:
	 * QCA_ATTR_TRACE_LEVEL_MODULE,
	 * QCA_ATTR_TRACE_LEVEL_MASK.
	 */
	QCA_ATTR_TRACE_LEVEL_PARAM = 1,
	/*
	 * Specific QCA host driver module. Please refer to the QCA host
	 * driver implementation to get the specific module ID.
	 */
	QCA_ATTR_TRACE_LEVEL_MODULE = 2,
	/* Different trace level masks represented in the QCA host driver. */
	QCA_ATTR_TRACE_LEVEL_MASK = 3,

	/* keep last */
	QCA_ATTR_TRACE_LEVEL_AFTER_LAST,
	QCA_ATTR_TRACE_LEVEL_MAX =
		QCA_ATTR_TRACE_LEVEL_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_get_he_capabilities - IEEE 802.11ax HE capabilities
 */
enum qca_wlan_vendor_attr_get_he_capabilities {
	QCA_WLAN_VENDOR_ATTR_HE_CAPABILITIES_INVALID = 0,
	/* Whether HE capabilities is supported
	 * (u8 attribute: 0 = not supported, 1 = supported)
	 */
	QCA_WLAN_VENDOR_ATTR_HE_SUPPORTED = 1,
	/* HE PHY capabilities, array of 3 u32 values  */
	QCA_WLAN_VENDOR_ATTR_PHY_CAPAB = 2,
	/* HE MAC capabilities (u32 attribute) */
	QCA_WLAN_VENDOR_ATTR_MAC_CAPAB = 3,
	/* HE MCS map (u32 attribute) */
	QCA_WLAN_VENDOR_ATTR_HE_MCS = 4,
	/* Number of SS (u32 attribute) */
	QCA_WLAN_VENDOR_ATTR_NUM_SS = 5,
	/* RU count (u32 attribute) */
	QCA_WLAN_VENDOR_ATTR_RU_IDX_MASK = 6,
	/* PPE threshold data, array of 8 u32 values */
	QCA_WLAN_VENDOR_ATTR_PPE_THRESHOLD = 7,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_HE_CAPABILITIES_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_HE_CAPABILITIES_MAX =
	QCA_WLAN_VENDOR_ATTR_HE_CAPABILITIES_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_spectral_scan - Spectral scan config parameters
 */
enum qca_wlan_vendor_attr_spectral_scan {
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_INVALID = 0,
	/* Number of times the chip enters spectral scan mode before
	 * deactivating spectral scans. When set to 0, chip will enter spectral
	 * scan mode continuously. u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_COUNT = 1,
	/* Spectral scan period. Period increment resolution is 256*Tclk,
	 * where Tclk = 1/44 MHz (Gmode), 1/40 MHz (Amode). u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_PERIOD = 2,
	/* Spectral scan priority. u32 attribute. */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PRIORITY = 3,
	/* Number of FFT data points to compute. u32 attribute. */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_SIZE = 4,
	/* Enable targeted gain change before starting the spectral scan FFT.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_GC_ENA = 5,
	/* Restart a queued spectral scan. u32 attribute. */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RESTART_ENA = 6,
	/* Noise floor reference number for the calculation of bin power.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NOISE_FLOOR_REF = 7,
	/* Disallow spectral scan triggers after TX/RX packets by setting
	 * this delay value to roughly SIFS time period or greater.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_INIT_DELAY = 8,
	/* Number of strong bins (inclusive) per sub-channel, below
	 * which a signal is declared a narrow band tone. u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NB_TONE_THR = 9,
	/* Specify the threshold over which a bin is declared strong (for
	 * scan bandwidth analysis). u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_STR_BIN_THR = 10,
	/* Spectral scan report mode. u32 attribute. */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_WB_RPT_MODE = 11,
	/* RSSI report mode, if the ADC RSSI is below
	 * QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_THR,
	 * then FFTs will not trigger, but timestamps and summaries get
	 * reported. u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_RPT_MODE = 12,
	/* ADC RSSI must be greater than or equal to this threshold (signed dB)
	 * to ensure spectral scan reporting with normal error code.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_THR = 13,
	/* Format of frequency bin magnitude for spectral scan triggered FFTs:
	 * 0: linear magnitude, 1: log magnitude (20*log10(lin_mag)).
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PWR_FORMAT = 14,
	/* Format of FFT report to software for spectral scan triggered FFTs.
	 * 0: No FFT report (only spectral scan summary report)
	 * 1: 2-dword summary of metrics for each completed FFT + spectral scan
	 * report
	 * 2: 2-dword summary of metrics for each completed FFT + 1x-oversampled
	 * bins (in-band) per FFT + spectral scan summary report
	 * 3: 2-dword summary of metrics for each completed FFT + 2x-oversampled
	 * bins (all) per FFT + spectral scan summary report
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RPT_MODE = 15,
	/* Number of LSBs to shift out in order to scale the FFT bins.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BIN_SCALE = 16,
	/* Set to 1 (with spectral_scan_pwr_format=1), to report bin magnitudes
	 * in dBm power. u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DBM_ADJ = 17,
	/* Per chain enable mask to select input ADC for search FFT.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_CHN_MASK = 18,
	/* An unsigned 64-bit integer provided by host driver to identify the
	 * spectral scan request. This attribute is included in the scan
	 * response message for @QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START
	 * and used as an attribute in
	 * @QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_STOP to identify the
	 * specific scan to be stopped.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COOKIE = 19,
	/* Skip interval for FFT reports. u32 attribute */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_PERIOD = 20,
	/* Set to report only one set of FFT results.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SHORT_REPORT = 21,
	/* Debug level for spectral module in driver.
	 * 0 : Verbosity level 0
	 * 1 : Verbosity level 1
	 * 2 : Verbosity level 2
	 * 3 : Matched filterID display
	 * 4 : One time dump of FFT report
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DEBUG_LEVEL = 22,
	/* Type of spectral scan request. u32 attribute.
	 * It uses values defined in enum
	 * qca_wlan_vendor_attr_spectral_scan_request_type.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE = 23,
	/* This specifies the frequency span over which spectral
	 * scan would be carried out. Its value depends on the
	 * value of QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_MODE and
	 * the relation is as follows.
	 * QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_NORMAL
	 *    Not applicable. Spectral scan would happen in the
	 *    operating span.
	 * QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_AGILE
	 *    Center frequency (in MHz) of the span of interest or
	 *    for convenience, center frequency (in MHz) of any channel
	 *    in the span of interest. For 80+80 MHz agile spectral scan
	 *    request it represents center frequency (in MHz) of the primary
	 *    80 MHz span or for convenience, center frequency (in MHz) of any
	 *    channel in the primary 80 MHz span. If agile spectral scan is
	 *    initiated without setting a valid frequency it returns the
	 *    error code
	 *    (QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_NOT_INITIALIZED).
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FREQUENCY = 24,
	/* Spectral scan mode. u32 attribute.
	 * It uses values defined in enum qca_wlan_vendor_spectral_scan_mode.
	 * If this attribute is not present, it is assumed to be
	 * normal mode (QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_NORMAL).
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_MODE = 25,
	/* Spectral scan error code. u32 attribute.
	 * It uses values defined in enum
	 * qca_wlan_vendor_spectral_scan_error_code.
	 * This attribute is included only in failure scenarios.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_ERROR_CODE = 26,
	/* 8-bit unsigned value to enable/disable debug of the
	 * Spectral DMA ring.
	 * 1-enable, 0-disable
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DMA_RING_DEBUG = 27,
	/* 8-bit unsigned value to enable/disable debug of the
	 * Spectral DMA buffers.
	 * 1-enable, 0-disable
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DMA_BUFFER_DEBUG = 28,
	/* This specifies the frequency span over which spectral scan would be
	 * carried out. Its value depends on the value of
	 * QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_MODE and the relation is as
	 * follows.
	 * QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_NORMAL
	 *    Not applicable. Spectral scan would happen in the operating span.
	 * QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_AGILE
	 *    This attribute is applicable only for agile spectral scan
	 *    requests in 80+80 MHz mode. It represents center frequency (in
	 *    MHz) of the secondary 80 MHz span or for convenience, center
	 *    frequency (in MHz) of any channel in the secondary 80 MHz span.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FREQUENCY_2 = 29,
	/* This attribute specifies the bandwidth to be used for spectral scan
	 * operation. This is an u8 attribute and uses the values in enum
	 * nl80211_chan_width. This is an optional attribute.
	 * If this attribute is not populated, the driver should configure the
	 * spectral scan bandwidth to the maximum value supported by the target
	 * for the current operating bandwidth.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BANDWIDTH = 30,
	/* Spectral FFT recapture flag attribute, to enable FFT recapture.
	 * Recapture can only be enabled for scan period greater than 52 us.
	 * If this attribute is enabled, re-triggers will be enabled when AGC
	 * gain changes.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_RECAPTURE = 31,
	/* Attribute used for padding for 64-bit alignment */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PAD = 32,
	/* Spectral data transport mode. u32 attribute. It uses values
	 * defined in enum qca_wlan_vendor_spectral_data_transport_mode.
	 * This is an optional attribute. If this attribute is not populated,
	 * the driver should configure the default transport mode to netlink.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_DATA_TRANSPORT_MODE = 33,
	/* Spectral scan completion timeout. u32 attribute. This
	 * attribute is used to configure a timeout value (in us). The
	 * timeout value would be from the beginning of a spectral
	 * scan. This is an optional attribute. If this attribute is
	 * not populated, the driver would internally derive the
	 * timeout value.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETION_TIMEOUT = 34,

	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX =
		QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_spectral_diag_stats - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_DIAG_STATS.
 */
enum qca_wlan_vendor_attr_spectral_diag_stats {
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_INVALID = 0,
	/* Number of spectral TLV signature mismatches.
	 * u64 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_SIG_MISMATCH = 1,
	/* Number of spectral phyerror events with insufficient length when
	 * parsing for secondary 80 search FFT report. u64 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_SEC80_SFFT_INSUFFLEN = 2,
	/* Number of spectral phyerror events without secondary 80
	 * search FFT report. u64 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_NOSEC80_SFFT = 3,
	/* Number of spectral phyerror events with vht operation segment 1 id
	 * mismatches in search fft report. u64 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_VHTSEG1ID_MISMATCH = 4,
	/* Number of spectral phyerror events with vht operation segment 2 id
	 * mismatches in search fft report. u64 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_VHTSEG2ID_MISMATCH = 5,
	/* Attribute used for padding for 64-bit alignment */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_PAD = 6,

	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_MAX =
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_spectral_cap - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CAP_INFO.
 */
enum qca_wlan_vendor_attr_spectral_cap {
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_INVALID = 0,
	/* Flag attribute to indicate phydiag capability */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_PHYDIAG = 1,
	/* Flag attribute to indicate radar detection capability */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_RADAR = 2,
	/* Flag attribute to indicate spectral capability */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_SPECTRAL = 3,
	/* Flag attribute to indicate advanced spectral capability */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_ADVANCED_SPECTRAL = 4,
	/* Spectral hardware generation. u32 attribute.
	 * It uses values defined in enum
	 * qca_wlan_vendor_spectral_scan_cap_hw_gen.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_HW_GEN = 5,
	/* Spectral bin scaling formula ID. u16 attribute.
	 * It uses values defined in enum
	 * qca_wlan_vendor_spectral_scan_cap_formula_id.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_FORMULA_ID = 6,
	/* Spectral bin scaling param - low level offset.
	 * s16 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_LOW_LEVEL_OFFSET = 7,
	/* Spectral bin scaling param - high level offset.
	 * s16 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_HIGH_LEVEL_OFFSET = 8,
	/* Spectral bin scaling param - RSSI threshold.
	 * s16 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_RSSI_THR = 9,
	/* Spectral bin scaling param - default AGC max gain.
	 * u8 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_DEFAULT_AGC_MAX_GAIN = 10,
	/* Flag attribute to indicate agile spectral scan capability
	 * for 20/40/80 MHz modes.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_AGILE_SPECTRAL = 11,
	/* Flag attribute to indicate agile spectral scan capability
	 * for 160 MHz mode.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_AGILE_SPECTRAL_160 = 12,
	/* Flag attribute to indicate agile spectral scan capability
	 * for 80+80 MHz mode.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_AGILE_SPECTRAL_80_80 = 13,
	/* Number of spectral detectors used for scan in 20 MHz.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_NUM_DETECTORS_20_MHZ = 14,
	/* Number of spectral detectors used for scan in 40 MHz.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_NUM_DETECTORS_40_MHZ = 15,
	/* Number of spectral detectors used for scan in 80 MHz.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_NUM_DETECTORS_80_MHZ = 16,
	/* Number of spectral detectors used for scan in 160 MHz.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_NUM_DETECTORS_160_MHZ = 17,
	/* Number of spectral detectors used for scan in 80+80 MHz.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_NUM_DETECTORS_80P80_MHZ = 18,
	/* Flag attribute to indicate agile spectral scan capability
	 * for 320 MHz mode.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_AGILE_SPECTRAL_320 = 19,
	/* Number of spectral detectors used for scan in 320 MHz.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_NUM_DETECTORS_320_MHZ = 20,

	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_MAX =
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_spectral_scan_status - used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_STATUS.
 */
enum qca_wlan_vendor_attr_spectral_scan_status {
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_INVALID = 0,
	/* Flag attribute to indicate whether spectral scan is enabled */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_IS_ENABLED = 1,
	/* Flag attribute to indicate whether spectral scan is in progress*/
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_IS_ACTIVE = 2,
	/* Spectral scan mode. u32 attribute.
	 * It uses values defined in enum qca_wlan_vendor_spectral_scan_mode.
	 * If this attribute is not present, normal mode
	 * (QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_NORMAL is assumed to be
	 * requested.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_MODE = 3,

	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_MAX =
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_AFTER_LAST - 1,
};

/**
 * qca_wlan_vendor_attr_spectral_scan_request_type: Attribute values for
 * QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE to the vendor subcmd
 * QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START. This represents the
 * spectral scan request types.
 * @QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_SCAN_AND_CONFIG: Request to
 * set the spectral parameters and start scan.
 * @QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_SCAN: Request to
 * only set the spectral parameters.
 * @QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_CONFIG: Request to
 * only start the spectral scan.
 */
enum qca_wlan_vendor_attr_spectral_scan_request_type {
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_SCAN_AND_CONFIG,
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_SCAN,
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_CONFIG,
};

/**
 * qca_wlan_vendor_spectral_scan_mode: Attribute values for
 * QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_MODE in the vendor subcmd
 * QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START and
 * QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_MODE in the vendor subcmd
 * QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_STATUS. This represents the
 * spectral scan modes.
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_NORMAL: Normal spectral scan:
 * spectral scan in the current operating span.
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_AGILE: Agile spectral scan:
 * spectral scan in the configured agile span.
 */
enum qca_wlan_vendor_spectral_scan_mode {
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_NORMAL = 0,
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_AGILE = 1,
};

/**
 * qca_wlan_vendor_spectral_scan_error_code: Attribute values for
 * QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_ERROR_CODE in the vendor subcmd
 * QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START.
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_UNSUPPORTED: Changing the value
 * of a parameter is not supported.
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_MODE_UNSUPPORTED: Requested spectral scan
 * mode is not supported.
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_INVALID_VALUE: A parameter
 * has invalid value.
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_NOT_INITIALIZED: A parameter
 * is not initialized.
 */
enum qca_wlan_vendor_spectral_scan_error_code {
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_UNSUPPORTED = 0,
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_MODE_UNSUPPORTED = 1,
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_INVALID_VALUE = 2,
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_NOT_INITIALIZED = 3,
};

/**
 * qca_wlan_vendor_spectral_scan_cap_hw_gen: Attribute values for
 * QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_HW_GEN to the vendor subcmd
 * QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CAP_INFO. This represents the
 * spectral hardware generation.
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_HW_GEN_1: generation 1
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_HW_GEN_2: generation 2
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_HW_GEN_3: generation 3
 */
enum qca_wlan_vendor_spectral_scan_cap_hw_gen {
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_HW_GEN_1 = 0,
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_HW_GEN_2 = 1,
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_HW_GEN_3 = 2,
};

enum qca_wlan_vendor_tos {
	QCA_WLAN_VENDOR_TOS_BK = 0,
	QCA_WLAN_VENDOR_TOS_BE = 1,
	QCA_WLAN_VENDOR_TOS_VI = 2,
	QCA_WLAN_VENDOR_TOS_VO = 3,
};

/**
 * enum qca_wlan_vendor_attr_active_tos - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_ACTIVE_TOS.
 */
enum qca_wlan_vendor_attr_active_tos {
	QCA_WLAN_VENDOR_ATTR_ACTIVE_TOS_INVALID = 0,
	/* Type Of Service - Represented by qca_wlan_vendor_tos */
	QCA_WLAN_VENDOR_ATTR_ACTIVE_TOS = 1,
	/* Flag attribute representing the start (attribute included) or stop
	 * (attribute not included) of the respective TOS.
	 */
	QCA_WLAN_VENDOR_ATTR_ACTIVE_TOS_START = 2,
};

enum qca_wlan_vendor_hang_reason {
	/* Unspecified reason */
	QCA_WLAN_HANG_REASON_UNSPECIFIED = 0,
	/* No Map for the MAC entry for the received frame */
	QCA_WLAN_HANG_RX_HASH_NO_ENTRY_FOUND = 1,
	/* Peer deletion timeout happened */
	QCA_WLAN_HANG_PEER_DELETION_TIMEDOUT = 2,
	/* Peer unmap timeout */
	QCA_WLAN_HANG_PEER_UNMAP_TIMEDOUT = 3,
	/* Scan request timed out */
	QCA_WLAN_HANG_SCAN_REQ_EXPIRED = 4,
	/* Consecutive Scan attempt failures */
	QCA_WLAN_HANG_SCAN_ATTEMPT_FAILURES = 5,
	/* Unable to get the message buffer */
	QCA_WLAN_HANG_GET_MSG_BUFF_FAILURE = 6,
	/* Current command processing is timedout */
	QCA_WLAN_HANG_ACTIVE_LIST_TIMEOUT = 7,
	/* Timeout for an ACK from FW for suspend request */
	QCA_WLAN_HANG_SUSPEND_TIMEOUT = 8,
	/* Timeout for an ACK from FW for resume request */
	QCA_WLAN_HANG_RESUME_TIMEOUT = 9,
	/* Transmission timeout for consecutive data frames */
	QCA_WLAN_HANG_TRANSMISSIONS_TIMEOUT = 10,
	/* Timeout for the TX completion status of data frame */
	QCA_WLAN_HANG_TX_COMPLETE_TIMEOUT = 11,
	/* DXE failure for TX/RX, DXE resource unavailability */
	QCA_WLAN_HANG_DXE_FAILURE = 12,
	/* WMI pending commands exceed the maximum count */
	QCA_WLAN_HANG_WMI_EXCEED_MAX_PENDING_CMDS = 13,
	/* Timeout for peer STA connection accept command's response from the
	 * FW in AP mode. This command is triggered when a STA (peer) connects
	 * to AP (DUT).
	 */
	QCA_WLAN_HANG_AP_STA_CONNECT_REQ_TIMEOUT = 14,
	/* Timeout for the AP connection accept command's response from the FW
	 * in STA mode. This command is triggered when the STA (DUT) connects
	 * to an AP (peer).
	 */
	QCA_WLAN_HANG_STA_AP_CONNECT_REQ_TIMEOUT = 15,
	/* Timeout waiting for the response to the MAC HW mode change command
	 * sent to FW as a part of MAC mode switch among DBS (Dual Band
	 * Simultaneous), SCC (Single Channel Concurrency), and MCC (Multi
	 * Channel Concurrency) mode.
	 */
	QCA_WLAN_HANG_MAC_HW_MODE_CHANGE_TIMEOUT = 16,
	/* Timeout waiting for the response from FW to configure the MAC HW's
	 * mode. This operation is to configure the single/two MACs in either
	 * SCC/MCC/DBS mode.
	 */
	QCA_WLAN_HANG_MAC_HW_MODE_CONFIG_TIMEOUT = 17,
	/* Timeout waiting for response of VDEV start command from the FW */
	QCA_WLAN_HANG_VDEV_START_RESPONSE_TIMED_OUT = 18,
	/* Timeout waiting for response of VDEV restart command from the FW */
	QCA_WLAN_HANG_VDEV_RESTART_RESPONSE_TIMED_OUT = 19,
	/* Timeout waiting for response of VDEV stop command from the FW */
	QCA_WLAN_HANG_VDEV_STOP_RESPONSE_TIMED_OUT = 20,
	/* Timeout waiting for response of VDEV delete command from the FW */
	QCA_WLAN_HANG_VDEV_DELETE_RESPONSE_TIMED_OUT = 21,
	/* Timeout waiting for response of peer all delete request command to
	 * the FW on a specific VDEV.
	 */
	QCA_WLAN_HANG_VDEV_PEER_DELETE_ALL_RESPONSE_TIMED_OUT = 22,
	/* WMI sequence mismatch between WMI command and Tx completion */
	QCA_WLAN_HANG_WMI_BUF_SEQUENCE_MISMATCH = 23,
	/* Write to Device HAL register failed */
	QCA_WLAN_HANG_REG_WRITE_FAILURE = 24,
	/* No credit left to send the wow_wakeup_from_sleep to firmware */
	QCA_WLAN_HANG_SUSPEND_NO_CREDIT = 25,
	/* Bus failure */
	QCA_WLAN_HANG_BUS_FAILURE = 26,
	/* tasklet/credit latency found */
	QCA_WLAN_HANG_TASKLET_CREDIT_LATENCY_DETECT = 27,
	/* MSDU buffers received in REO error ring, exceeding certain
	 * threshold
	 */
	QCA_WLAN_HANG_RX_MSDU_BUF_RCVD_IN_ERR_RING = 28,
	/* Vdev SM is out of sync and connect req received
	 * when already connected
	 */
	QCA_WLAN_HANG_VDEV_SM_OUT_OF_SYNC = 29,
	/* Stats request timeout */
	QCA_WLAN_HANG_STATS_REQ_TIMEOUT = 30,
	/* Leak in TX descriptor for a packet */
	QCA_WLAN_HANG_TX_DESC_LEAK = 31,
	/* Scheduler watchdog timeout */
	QCA_WLAN_HANG_SCHED_TIMEOUT = 32,
	/* Failed to send self peer deletion cmd to firmware */
	QCA_WLAN_HANG_SELF_PEER_DEL_FAIL = 33,
	/* Received del self sta without del bss */
	QCA_WLAN_HANG_DEL_SELF_STA_FAIL = 34,
	/* Recovery needed when sending flush completion to userspace */
	QCA_WLAN_HANG_FLUSH_LOGS = 35,
	/* Host wakeup because of page fault */
	QCA_WLAN_HANG_HOST_WAKEUP_REASON_PAGE_FAULT = 36,
};

/**
 * enum qca_wlan_vendor_attr_hang - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_HANG.
 */
enum qca_wlan_vendor_attr_hang {
	QCA_WLAN_VENDOR_ATTR_HANG_INVALID = 0,
	/* Reason for the hang - u32 attribute with a value from enum
	 * qca_wlan_vendor_hang_reason.
	 */
	QCA_WLAN_VENDOR_ATTR_HANG_REASON = 1,
	/* The binary blob data associated with the hang reason specified by
	 * QCA_WLAN_VENDOR_ATTR_HANG_REASON. This binary data is expected to
	 * contain the required dump to analyze the reason for the hang.
	 * NLA_BINARY attribute, the max size is 1024 bytes.
	 */
	QCA_WLAN_VENDOR_ATTR_HANG_REASON_DATA = 2,

	QCA_WLAN_VENDOR_ATTR_HANG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_HANG_MAX =
		QCA_WLAN_VENDOR_ATTR_HANG_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_flush_pending_policy: Represents values for
 * the policy to flush pending frames, configured via
 * %QCA_NL80211_VENDOR_SUBCMD_PEER_FLUSH_PENDING. This enumeration defines the
 * valid values for %QCA_WLAN_VENDOR_ATTR_FLUSH_PENDING_POLICY.
 *
 * @QCA_WLAN_VENDOR_FLUSH_PENDING_POLICY_NONE: This value clears all
 * the flush policy configured before. This command basically disables the
 * flush config set by the user.
 * @QCA_WLAN_VENDOR_FLUSH_PENDING_POLICY_IMMEDIATE: This value configures
 * the flush policy to be immediate. All pending packets for the peer/TID are
 * flushed when this command/policy is received.
 * @QCA_WLAN_VENDOR_FLUSH_PENDING_POLICY_TWT_SP_END: This value configures
 * the flush policy to the end of TWT SP. All pending packets for the peer/TID
 * are flushed when the end of TWT SP is reached.
 */
enum qca_wlan_vendor_flush_pending_policy  {
	QCA_WLAN_VENDOR_FLUSH_PENDING_POLICY_NONE = 0,
	QCA_WLAN_VENDOR_FLUSH_PENDING_POLICY_IMMEDIATE = 1,
	QCA_WLAN_VENDOR_FLUSH_PENDING_POLICY_TWT_SP_END = 2,
};

/**
 * enum qca_wlan_vendor_attr_flush_pending - Attributes for
 * flushing pending traffic in firmware.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_ADDR: Configure peer MAC address.
 * @QCA_WLAN_VENDOR_ATTR_AC: Configure access category of the pending
 * packets. It is u8 value with bit 0~3 represent AC_BE, AC_BK,
 * AC_VI, AC_VO respectively. Set the corresponding bit to 1 to
 * flush packets with access category. This is optional. See below.
 * @QCA_WLAN_VENDOR_ATTR_TID_MASK: Configure TID mask of the pending packets.
 * It is a u32 value with bit 0-7 representing TID 0-7. Set corresponding
 * bit to 1 to act upon the TID. This is optional. Either this attribute or
 * %QCA_WLAN_VENDOR_ATTR_AC must be provided. If both are provided,
 * %QCA_WLAN_VENDOR_ATTR_TID_MASK takes precedence. If neither are provided
 * it is an error.
 * @QCA_WLAN_VENDOR_ATTR_FLUSH_PENDING_POLICY: Policy of flushing the pending
 * packets corresponding to the peer/TID provided. It is a u32 value,
 * represented by %enum qca_wlan_vendor_flush_pending_policy. This
 * value is honored only when TID mask is provided. This is not honored when AC
 * mask is provided.
 */
enum qca_wlan_vendor_attr_flush_pending {
	QCA_WLAN_VENDOR_ATTR_FLUSH_PENDING_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_PEER_ADDR = 1,
	QCA_WLAN_VENDOR_ATTR_AC = 2,
	QCA_WLAN_VENDOR_ATTR_TID_MASK = 3,
	QCA_WLAN_VENDOR_ATTR_FLUSH_PENDING_POLICY = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_FLUSH_PENDING_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_FLUSH_PENDING_MAX =
	QCA_WLAN_VENDOR_ATTR_FLUSH_PENDING_AFTER_LAST - 1,
};

/**
 * qca_wlan_vendor_spectral_scan_cap_formula_id: Attribute values for
 * QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_FORMULA_ID in the vendor subcmd
 * QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CAP_INFO. This represents the
 * Spectral bin scaling formula ID.
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_NO_SCALING: No scaling
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_AGC_GAIN_RSSI_CORR_BASED: AGC gain
 * and RSSI threshold based formula.
 */
enum qca_wlan_vendor_spectral_scan_cap_formula_id {
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_NO_SCALING = 0,
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_AGC_GAIN_RSSI_CORR_BASED = 1,
};

/**
 * enum qca_wlan_vendor_attr_rropavail_info - Specifies whether Representative
 * RF Operating Parameter (RROP) information is available, and if so, at which
 * point in the application-driver interaction sequence it can be retrieved by
 * the application from the driver. This point may vary by architecture and
 * other factors. This is a u16 value.
 */
enum qca_wlan_vendor_attr_rropavail_info {
	/* RROP information is unavailable. */
	QCA_WLAN_VENDOR_ATTR_RROPAVAIL_INFO_UNAVAILABLE,
	/* RROP information is available and the application can retrieve the
	 * information after receiving an QCA_NL80211_VENDOR_SUBCMD_EXTERNAL_ACS
	 * event from the driver.
	 */
	QCA_WLAN_VENDOR_ATTR_RROPAVAIL_INFO_EXTERNAL_ACS_START,
	/* RROP information is available only after a vendor specific scan
	 * (requested using QCA_NL80211_VENDOR_SUBCMD_TRIGGER_SCAN) has
	 * successfully completed. The application can retrieve the information
	 * after receiving the QCA_NL80211_VENDOR_SUBCMD_SCAN_DONE event from
	 * the driver.
	 */
	QCA_WLAN_VENDOR_ATTR_RROPAVAIL_INFO_VSCAN_END,
};

/**
 * enum qca_wlan_vendor_attr_rrop_info - Specifies vendor specific
 * Representative RF Operating Parameter (RROP) information. It is sent for the
 * vendor command QCA_NL80211_VENDOR_SUBCMD_GET_RROP_INFO. This information is
 * intended for use by external Auto Channel Selection applications. It provides
 * guidance values for some RF parameters that are used by the system during
 * operation. These values could vary by channel, band, radio, and so on.
 */
enum qca_wlan_vendor_attr_rrop_info {
	QCA_WLAN_VENDOR_ATTR_RROP_INFO_INVALID = 0,

	/* Representative Tx Power List (RTPL) which has an array of nested
	 * values as per attributes in enum qca_wlan_vendor_attr_rtplinst.
	 */
	QCA_WLAN_VENDOR_ATTR_RROP_INFO_RTPL = 1,

	QCA_WLAN_VENDOR_ATTR_RROP_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_RROP_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_RROP_INFO_AFTER_LAST - 1
};

/**
 * enum qca_wlan_vendor_attr_rtplinst - Specifies attributes for individual list
 * entry instances in the Representative Tx Power List (RTPL). It provides
 * simplified power values intended for helping external Auto channel Selection
 * applications compare potential Tx power performance between channels, other
 * operating conditions remaining identical. These values are not necessarily
 * the actual Tx power values that will be used by the system. They are also not
 * necessarily the max or average values that will be used. Instead, they are
 * relative, summarized keys for algorithmic use computed by the driver or
 * underlying firmware considering a number of vendor specific factors.
 */
enum qca_wlan_vendor_attr_rtplinst {
	QCA_WLAN_VENDOR_ATTR_RTPLINST_INVALID = 0,

	/* Primary channel number (u8).
	 * Note: If both the driver and user space application support the
	 * 6 GHz band, this attribute is deprecated and
	 * QCA_WLAN_VENDOR_ATTR_RTPLINST_PRIMARY_FREQUENCY should be used. To
	 * maintain backward compatibility,
	 * QCA_WLAN_VENDOR_ATTR_RTPLINST_PRIMARY is still used if either the
	 * driver or user space application or both do not support the 6 GHz
	 * band.
	 */
	QCA_WLAN_VENDOR_ATTR_RTPLINST_PRIMARY = 1,
	/* Representative Tx power in dBm (s32) with emphasis on throughput. */
	QCA_WLAN_VENDOR_ATTR_RTPLINST_TXPOWER_THROUGHPUT = 2,
	/* Representative Tx power in dBm (s32) with emphasis on range. */
	QCA_WLAN_VENDOR_ATTR_RTPLINST_TXPOWER_RANGE = 3,
	/* Primary channel center frequency (u32) in MHz */
	QCA_WLAN_VENDOR_ATTR_RTPLINST_PRIMARY_FREQUENCY = 4,

	QCA_WLAN_VENDOR_ATTR_RTPLINST_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_RTPLINST_MAX =
		QCA_WLAN_VENDOR_ATTR_RTPLINST_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_config_latency_level - Level for
 * wlan latency module.
 *
 * There will be various of Wi-Fi functionality like scan/roaming/adaptive
 * power saving which would causing data exchange out of service, this
 * would be a big impact on latency. For latency sensitive applications over
 * Wi-Fi are intolerant to such operations and thus would configure them
 * to meet their respective needs. It is well understood by such applications
 * that altering the default behavior would degrade the Wi-Fi functionality
 * w.r.t the above pointed WLAN operations.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL_NORMAL:
 *	Default WLAN operation level which throughput orientated.
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL_XR:
 *	Use XR level to benefit XR (extended reality) application to achieve
 *	latency and power by via constraint scan/roaming/adaptive PS.
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL_LOW:
 *	Use low latency level to benefit application like concurrent
 *	downloading or video streaming via constraint scan/adaptive PS.
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL_ULTRALOW:
 *	Use ultra low latency level to benefit for gaming/voice
 *	application via constraint scan/roaming/adaptive PS.
 */
enum qca_wlan_vendor_attr_config_latency_level {
	QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL_NORMAL = 1,
	QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL_XR = 2,
	/* legacy name */
	QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL_MODERATE =
	QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL_XR,
	QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL_LOW = 3,
	QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL_ULTRALOW = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL_MAX =
	QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_wlan_mac - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_WLAN_MAC_INFO.
 */
enum qca_wlan_vendor_attr_mac {
	QCA_WLAN_VENDOR_ATTR_MAC_INVALID = 0,

	/* MAC mode info list which has an array of nested values as
	 * per attributes in enum qca_wlan_vendor_attr_mac_mode_info.
	 */
	QCA_WLAN_VENDOR_ATTR_MAC_INFO = 1,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_MAC_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_MAC_MAX =
	QCA_WLAN_VENDOR_ATTR_MAC_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_mac_iface_info - Information of the connected
 *	Wi-Fi netdev interface on a respective MAC.
 *	Used by the attribute QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO.
 */
enum qca_wlan_vendor_attr_mac_iface_info {
	QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO_INVALID = 0,
	/* Wi-Fi netdev's interface index (u32) */
	QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO_IFINDEX = 1,
	/* Associated frequency in MHz of the connected Wi-Fi interface (u32) */
	QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO_FREQ = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_mac_info - Points to MAC the information.
 *	Used by the attribute QCA_WLAN_VENDOR_ATTR_MAC_INFO of the
 *	vendor command QCA_NL80211_VENDOR_SUBCMD_WLAN_MAC_INFO.
 */
enum qca_wlan_vendor_attr_mac_info {
	QCA_WLAN_VENDOR_ATTR_MAC_INFO_INVALID = 0,
	/* Hardware MAC ID associated for the MAC (u32) */
	QCA_WLAN_VENDOR_ATTR_MAC_INFO_MAC_ID = 1,
	/* Band supported by the MAC at a given point.
	 * This is a u32 bitmask of BIT(NL80211_BAND_*) as described in %enum
	 * nl80211_band.
	 */
	QCA_WLAN_VENDOR_ATTR_MAC_INFO_BAND = 2,
	/* Refers to list of WLAN netdev interfaces associated with this MAC.
	 * Represented by enum qca_wlan_vendor_attr_mac_iface_info.
	 */
	QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_MAC_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_MAC_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_MAC_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_get_logger_features - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_GET_LOGGER_FEATURE_SET.
 */
enum qca_wlan_vendor_attr_get_logger_features {
	QCA_WLAN_VENDOR_ATTR_LOGGER_INVALID = 0,
	/* Unsigned 32-bit enum value of wifi_logger_supported_features */
	QCA_WLAN_VENDOR_ATTR_LOGGER_SUPPORTED = 1,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_LOGGER_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_LOGGER_MAX =
		QCA_WLAN_VENDOR_ATTR_LOGGER_AFTER_LAST - 1,
};

/**
 * enum wifi_logger_supported_features - Values for supported logger features
 */
enum wifi_logger_supported_features {
	WIFI_LOGGER_MEMORY_DUMP_FEATURE = (1 << (0)),
	WIFI_LOGGER_PER_PACKET_TX_RX_STATUS_FEATURE = (1 << (1)),
	WIFI_LOGGER_CONNECT_EVENT_FEATURE = (1 << (2)),
	WIFI_LOGGER_POWER_EVENT_FEATURE = (1 << (3)),
	WIFI_LOGGER_WAKE_LOCK_FEATURE = (1 << (4)),
	WIFI_LOGGER_VERBOSE_FEATURE = (1 << (5)),
	WIFI_LOGGER_WATCHDOG_TIMER_FEATURE = (1 << (6)),
	WIFI_LOGGER_DRIVER_DUMP_FEATURE = (1 << (7)),
	WIFI_LOGGER_PACKET_FATE_FEATURE = (1 << (8)),
};

/**
 * enum qca_wlan_tdls_caps_features_supported - Values for TDLS get
 * capabilities features
 */
enum qca_wlan_tdls_caps_features_supported {
	WIFI_TDLS_SUPPORT = (1 << (0)),
	WIFI_TDLS_EXTERNAL_CONTROL_SUPPORT = (1 << (1)),
	WIFI_TDLS_OFFCHANNEL_SUPPORT = (1 << (2)),

	/* Indicates if the TDLS session can be formed with the peer using
	 * higher bandwidth than the bandwidth of the AP path.
	 */
	WIFI_TDLS_WIDER_BW_SUPPORT = (1 << (3)),
};

/**
 * enum qca_wlan_vendor_attr_tdls_get_capabilities - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_TDLS_GET_CAPABILITIES.
 */
enum qca_wlan_vendor_attr_tdls_get_capabilities {
	QCA_WLAN_VENDOR_ATTR_TDLS_GET_CAPS_INVALID = 0,
	/* Indicates the max concurrent sessions */
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_TDLS_GET_CAPS_MAX_CONC_SESSIONS,
	/* Indicates the support for features */
	/* Unsigned 32-bit bitmap qca_wlan_tdls_caps_features_supported
	 */
	QCA_WLAN_VENDOR_ATTR_TDLS_GET_CAPS_FEATURES_SUPPORTED,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TDLS_GET_CAPS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TDLS_GET_CAPS_MAX =
		QCA_WLAN_VENDOR_ATTR_TDLS_GET_CAPS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_offloaded_packets_sending_control - Offload packets control
 * command used as value for the attribute
 * QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_SENDING_CONTROL.
 */
enum qca_wlan_offloaded_packets_sending_control {
	QCA_WLAN_OFFLOADED_PACKETS_SENDING_CONTROL_INVALID = 0,
	QCA_WLAN_OFFLOADED_PACKETS_SENDING_START,
	QCA_WLAN_OFFLOADED_PACKETS_SENDING_STOP
};

/**
 * enum qca_wlan_vendor_attr_offloaded_packets - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_OFFLOADED_PACKETS.
 */
enum qca_wlan_vendor_attr_offloaded_packets {
	QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_INVALID = 0,
	/* Takes valid value from the enum
	 * qca_wlan_offloaded_packets_sending_control
	 * Unsigned 32-bit value
	 */
	QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_SENDING_CONTROL,
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_REQUEST_ID,
	/* array of u8 len: Max packet size */
	QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_IP_PACKET_DATA,
	/* 6-byte MAC address used to represent source MAC address */
	QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_SRC_MAC_ADDR,
	/* 6-byte MAC address used to represent destination MAC address */
	QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_DST_MAC_ADDR,
	/* Unsigned 32-bit value, in milli seconds */
	QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_PERIOD,
	/* This optional unsigned 16-bit attribute is used for specifying
	 * ethernet protocol type. If not specified ethertype defaults to IPv4.
	 */
	QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_ETHER_PROTO_TYPE,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_MAX =
		QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_rssi_monitoring_control - RSSI control commands used as values
 * by the attribute QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_CONTROL.
 */
enum qca_wlan_rssi_monitoring_control {
	QCA_WLAN_RSSI_MONITORING_CONTROL_INVALID = 0,
	QCA_WLAN_RSSI_MONITORING_START,
	QCA_WLAN_RSSI_MONITORING_STOP,
};

/**
 * enum qca_wlan_vendor_attr_rssi_monitoring - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_MONITOR_RSSI.
 */
enum qca_wlan_vendor_attr_rssi_monitoring {
	QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_INVALID = 0,
	/* Takes valid value from the enum
	 * qca_wlan_rssi_monitoring_control
	 * Unsigned 32-bit value enum qca_wlan_rssi_monitoring_control
	 */
	QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_CONTROL,
	/* Unsigned 32-bit value */
	QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_REQUEST_ID,
	/* Signed 8-bit value in dBm */
	QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_MAX_RSSI,
	/* Signed 8-bit value in dBm */
	QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_MIN_RSSI,
	/* attributes to be used/received in callback */
	/* 6-byte MAC address used to represent current BSSID MAC address */
	QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_CUR_BSSID,
	/* Signed 8-bit value indicating the current RSSI */
	QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_CUR_RSSI,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_MAX =
		QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_ndp_params - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_NDP.
 */
enum qca_wlan_vendor_attr_ndp_params {
	QCA_WLAN_VENDOR_ATTR_NDP_PARAM_INVALID = 0,
	/* Unsigned 32-bit value
	 * enum of sub commands values in qca_wlan_ndp_sub_cmd
	 */
	QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD,
	/* Unsigned 16-bit value */
	QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID,
	/* NL attributes for data used NDP SUB cmds */
	/* Unsigned 32-bit value indicating a service info */
	QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_INSTANCE_ID,
	/* Unsigned 32-bit value; channel frequency in MHz */
	QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL,
	/* Interface Discovery MAC address. An array of 6 Unsigned int8 */
	QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR,
	/* Interface name on which NDP is being created */
	QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR,
	/* Unsigned 32-bit value for security */
	/* CONFIG_SECURITY is deprecated, use NCS_SK_TYPE/PMK/SCID instead */
	QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_SECURITY,
	/* Unsigned 32-bit value for QoS */
	QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_QOS,
	/* Array of u8: len = QCA_WLAN_VENDOR_ATTR_NAN_DP_APP_INFO_LEN */
	QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO,
	/* Unsigned 32-bit value for NDP instance Id */
	QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID,
	/* Array of instance Ids */
	QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY,
	/* Unsigned 32-bit value for initiator/responder NDP response code
	 * accept/reject
	 */
	QCA_WLAN_VENDOR_ATTR_NDP_RESPONSE_CODE,
	/* NDI MAC address. An array of 6 Unsigned int8 */
	QCA_WLAN_VENDOR_ATTR_NDP_NDI_MAC_ADDR,
	/* Unsigned 32-bit value errors types returned by driver
	 * The wifi_nan.h in AOSP project platform/hardware/libhardware_legacy
	 * NanStatusType includes these values.
	 */
	QCA_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE,
	/* Unsigned 32-bit value error values returned by driver
	 * The nan_i.h in AOSP project platform/hardware/qcom/wlan
	 * NanInternalStatusType includes these values.
	 */
	QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE,
	/* Unsigned 32-bit value for Channel setup configuration
	 * The wifi_nan.h in AOSP project platform/hardware/libhardware_legacy
	 * NanDataPathChannelCfg includes these values.
	 */
	QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_CONFIG,
	/* Unsigned 32-bit value for Cipher Suite Shared Key Type */
	QCA_WLAN_VENDOR_ATTR_NDP_CSID,
	/* Array of u8: len = NAN_PMK_INFO_LEN 32 bytes */
	QCA_WLAN_VENDOR_ATTR_NDP_PMK,
	/* Security Context Identifier that contains the PMKID
	 * Array of u8: len = NAN_SCID_BUF_LEN 1024 bytes
	 */
	QCA_WLAN_VENDOR_ATTR_NDP_SCID,
	/* Array of u8: len = NAN_SECURITY_MAX_PASSPHRASE_LEN 63 bytes */
	QCA_WLAN_VENDOR_ATTR_NDP_PASSPHRASE,
	/* Array of u8: len = NAN_MAX_SERVICE_NAME_LEN 255 bytes */
	QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_NAME,
	/* Unsigned 32-bit bitmap indicating schedule update
	 * BIT_0: NSS Update
	 * BIT_1: Channel list update
	 */
	QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_UPDATE_REASON,
	/* Unsigned 32-bit value for NSS */
	QCA_WLAN_VENDOR_ATTR_NDP_NSS,
	/* Unsigned 32-bit value for NUMBER NDP CHANNEL */
	QCA_WLAN_VENDOR_ATTR_NDP_NUM_CHANNELS,
	/* Unsigned 32-bit value for CHANNEL BANDWIDTH
	 * 0:20 MHz, 1:40 MHz, 2:80 MHz, 3:160 MHz
	 */
	QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_WIDTH,
	/* Array of channel/band width */
	QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_INFO,
	/* IPv6 address used by NDP (in network byte order), 16 bytes array.
	 * This attribute is used and optional for ndp request, ndp response,
	 * ndp indication, and ndp confirm.
	 */
	QCA_WLAN_VENDOR_ATTR_NDP_IPV6_ADDR = 27,
	/* Unsigned 16-bit value indicating transport port used by NDP.
	 * This attribute is used and optional for ndp response, ndp indication,
	 * and ndp confirm.
	 */
	QCA_WLAN_VENDOR_ATTR_NDP_TRANSPORT_PORT = 28,
	/* Unsigned 8-bit value indicating protocol used by NDP and assigned by
	 * the Internet Assigned Numbers Authority (IANA) as per:
	 * https://www.iana.org/assignments/protocol-numbers/protocol-numbers.xhtml
	 * This attribute is used and optional for ndp response, ndp indication,
	 * and ndp confirm.
	 */
	QCA_WLAN_VENDOR_ATTR_NDP_TRANSPORT_PROTOCOL = 29,
	/* Unsigned 8-bit value indicating if NDP remote peer supports NAN NDPE.
	 * 1:support 0:not support
	 */
	QCA_WLAN_VENDOR_ATTR_PEER_NDPE_SUPPORT = 30,
	/* As per Wi-Fi Aware Specification v3.2 Service Id is the first
	 * 48 bits of the SHA-256 hash of the Service Name.
	 * A lower-case representation of the Service Name shall be used to
	 * calculate the Service ID.
	 * Array of u8: length is 6 bytes
	 * This attribute is used and optional for ndp indication.
	 */
	QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_ID = 31,
	/* Unsigned 8-bit value for Cipher Suite capabilities.
	 * u8 attribute.
	 * This attribute is used and optional in ndp request, ndp response,
	 * ndp indication, and ndp confirm.
	 * This attribute is used to indicate the Capabilities field of Cipher
	 * Suite Information attribute (CSIA) of NDP frames as defined in
	 * Wi-Fi Aware Specification v4.0, 9.5.21.2, Table 122.
	 * Firmware can accept or ignore any of the capability bits.
	 */
	QCA_WLAN_VENDOR_ATTR_NDP_CSIA_CAPABILITIES = 32,
	/* Indicate that GTK protection is required for NDP.
	 * NLA_FLAG attribute.
	 * This attribute can be used in ndp request, ndp response, ndp
	 * indication, and ndp confirm.
	 * GTK protection required is indicated in the NDPE attribute of NAN
	 * action frame (NAF) during NDP negotiation as defined in
	 * Wi-Fi Aware Specification v4.0, 9.5.16.2.
	 * If the device and peer supports GTKSA and if GTK protection required
	 * bit is set in NDPE IE, devices will share GTK to each other in SKDA
	 * of Data Path Security Confirm and Data Path Security Install frames
	 * of NDP negotiation to send and receive protected group addressed data
	 * frames from each other.
	 */
	QCA_WLAN_VENDOR_ATTR_NDP_GTK_REQUIRED = 33,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX =
		QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_AFTER_LAST - 1,
};

enum qca_wlan_ndp_sub_cmd {
	QCA_WLAN_VENDOR_ATTR_NDP_INVALID = 0,
	/* Command to create a NAN data path interface.
	 * This command was initially designed to both create and start a NAN
	 * data path interface. However, changes to Linux 5.12 no longer allow
	 * interface creation via vendor commands. When the driver advertises
	 * QCA_WLAN_VENDOR_FEATURE_USE_ADD_DEL_VIRTUAL_INTF_FOR_NDI
	 * userspace must explicitly first create the interface using
	 * NL80211_CMD_NEW_INTERFACE before subsequently invoking this command
	 * to start the interface.
	 */
	QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_CREATE = 1,
	/* Command to delete a NAN data path interface.
	 * This command was initially designed to both stop and delete a NAN
	 * data path interface. However, changes to Linux 5.12 no longer allow
	 * interface deletion via vendor commands. When the driver advertises
	 * QCA_WLAN_VENDOR_FEATURE_USE_ADD_DEL_VIRTUAL_INTF_FOR_NDI
	 * userspace must explicitly delete the interface using
	 * NL80211_CMD_DEL_INTERFACE after calling this command.
	 */
	QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_DELETE = 2,
	/* Command to initiate a NAN data path session */
	QCA_WLAN_VENDOR_ATTR_NDP_INITIATOR_REQUEST = 3,
	/* Command to notify if the NAN data path session was sent */
	QCA_WLAN_VENDOR_ATTR_NDP_INITIATOR_RESPONSE = 4,
	/* Command to respond to NAN data path session */
	QCA_WLAN_VENDOR_ATTR_NDP_RESPONDER_REQUEST = 5,
	/* Command to notify on the responder about the response */
	QCA_WLAN_VENDOR_ATTR_NDP_RESPONDER_RESPONSE = 6,
	/* Command to initiate a NAN data path end */
	QCA_WLAN_VENDOR_ATTR_NDP_END_REQUEST = 7,
	/* Command to notify the if end request was sent */
	QCA_WLAN_VENDOR_ATTR_NDP_END_RESPONSE = 8,
	/* Command to notify the peer about the end request */
	QCA_WLAN_VENDOR_ATTR_NDP_REQUEST_IND = 9,
	/* Command to confirm the NAN data path session is complete */
	QCA_WLAN_VENDOR_ATTR_NDP_CONFIRM_IND = 10,
	/* Command to indicate the peer about the end request being received */
	QCA_WLAN_VENDOR_ATTR_NDP_END_IND = 11,
	/* Command to indicate the peer of schedule update */
	QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_UPDATE_IND = 12
};

/**
 * enum qca_wlan_vendor_attr_nd_offload - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_ND_OFFLOAD.
 */
enum qca_wlan_vendor_attr_nd_offload {
	QCA_WLAN_VENDOR_ATTR_ND_OFFLOAD_INVALID = 0,
	/* Flag to set Neighbour Discovery offload */
	QCA_WLAN_VENDOR_ATTR_ND_OFFLOAD_FLAG,
	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_ND_OFFLOAD_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ND_OFFLOAD_MAX =
		QCA_WLAN_VENDOR_ATTR_ND_OFFLOAD_AFTER_LAST - 1,
};

/**
 * enum packet_filter_sub_cmd - Packet filter sub commands
 */
enum packet_filter_sub_cmd {
	/**
	 * Write packet filter program and/or data. The driver/firmware should
	 * disable APF before writing into local buffer and re-enable APF after
	 * writing is done.
	 */
	QCA_WLAN_SET_PACKET_FILTER = 1,
	/* Get packet filter feature capabilities from driver */
	QCA_WLAN_GET_PACKET_FILTER = 2,
	/**
	 * Write packet filter program and/or data. User space will send the
	 * %QCA_WLAN_DISABLE_PACKET_FILTER command before issuing this command
	 * and will send the %QCA_WLAN_ENABLE_PACKET_FILTER afterwards. The key
	 * difference from that %QCA_WLAN_SET_PACKET_FILTER is the control over
	 * enable/disable is given to user space with this command. Also,
	 * user space sends the length of program portion in the buffer within
	 * %QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_PROG_LENGTH.
	 */
	QCA_WLAN_WRITE_PACKET_FILTER = 3,
	/* Read packet filter program and/or data */
	QCA_WLAN_READ_PACKET_FILTER = 4,
	/* Enable APF feature */
	QCA_WLAN_ENABLE_PACKET_FILTER = 5,
	/* Disable APF feature */
	QCA_WLAN_DISABLE_PACKET_FILTER = 6,
};

/**
 * enum qca_wlan_vendor_attr_packet_filter - BPF control commands used by
 * vendor QCA_NL80211_VENDOR_SUBCMD_PACKET_FILTER.
 */
enum qca_wlan_vendor_attr_packet_filter {
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_INVALID = 0,
	/* Unsigned 32-bit enum passed using packet_filter_sub_cmd */
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_SUB_CMD,
	/* Unsigned 32-bit value indicating the packet filter version */
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_VERSION,
	/* Unsigned 32-bit value indicating the packet filter id */
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_ID,
	/**
	 * Unsigned 32-bit value indicating the packet filter size including
	 * program + data.
	 */
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_SIZE,
	/* Unsigned 32-bit value indicating the packet filter current offset */
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_CURRENT_OFFSET,
	/* Program and/or data in bytes */
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_PROGRAM,
	/* Unsigned 32-bit value of the length of the program section in packet
	 * filter buffer.
	 */
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_PROG_LENGTH = 7,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_MAX =
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_drv_info - WLAN driver info used by vendor command
 * QCA_NL80211_VENDOR_SUBCMD_GET_BUS_SIZE.
 */
enum qca_wlan_vendor_drv_info {
	QCA_WLAN_VENDOR_ATTR_DRV_INFO_INVALID = 0,
	/* Maximum Message size info between firmware & HOST
	 * Unsigned 32-bit value
	 */
	QCA_WLAN_VENDOR_ATTR_DRV_INFO_BUS_SIZE,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_DRV_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_DRV_INFO_MAX =
		QCA_WLAN_VENDOR_ATTR_DRV_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_wake_stats - Wake lock stats used by vendor
 * command QCA_NL80211_VENDOR_SUBCMD_GET_WAKE_REASON_STATS.
 */
enum qca_wlan_vendor_attr_wake_stats {
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_INVALID = 0,
	/* Unsigned 32-bit value indicating the total count of wake event */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_TOTAL_CMD_EVENT_WAKE,
	/* Array of individual wake count, each index representing wake reason
	 */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_CMD_EVENT_WAKE_CNT_PTR,
	/* Unsigned 32-bit value representing wake count array */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_CMD_EVENT_WAKE_CNT_SZ,
	/* Unsigned 32-bit total wake count value of driver/fw */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_TOTAL_DRIVER_FW_LOCAL_WAKE,
	/* Array of wake stats of driver/fw */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_DRIVER_FW_LOCAL_WAKE_CNT_PTR,
	/* Unsigned 32-bit total wake count value of driver/fw */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_DRIVER_FW_LOCAL_WAKE_CNT_SZ,
	/* Unsigned 32-bit total wake count value of packets received */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_TOTAL_RX_DATA_WAKE,
	/* Unsigned 32-bit wake count value unicast packets received */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_RX_UNICAST_CNT,
	/* Unsigned 32-bit wake count value multicast packets received */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_RX_MULTICAST_CNT,
	/* Unsigned 32-bit wake count value broadcast packets received */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_RX_BROADCAST_CNT,
	/* Unsigned 32-bit wake count value of ICMP packets */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_ICMP_PKT,
	/* Unsigned 32-bit wake count value of ICMP6 packets */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_ICMP6_PKT,
	/* Unsigned 32-bit value ICMP6 router advertisement */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_ICMP6_RA,
	/* Unsigned 32-bit value ICMP6 neighbor advertisement */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_ICMP6_NA,
	/* Unsigned 32-bit value ICMP6 neighbor solicitation */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_ICMP6_NS,
	/* Unsigned 32-bit wake count value of receive side ICMP4 multicast */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_ICMP4_RX_MULTICAST_CNT,
	/* Unsigned 32-bit wake count value of receive side ICMP6 multicast */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_ICMP6_RX_MULTICAST_CNT,
	/* Unsigned 32-bit wake count value of receive side multicast */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_OTHER_RX_MULTICAST_CNT,
	/* Unsigned 32-bit wake count value of a given RSSI breach */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_RSSI_BREACH_CNT,
	/* Unsigned 32-bit wake count value of low RSSI */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_LOW_RSSI_CNT,
	/* Unsigned 32-bit value GSCAN count */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_GSCAN_CNT,
	/* Unsigned 32-bit value PNO complete count */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_PNO_COMPLETE_CNT,
	/* Unsigned 32-bit value PNO match count */
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_PNO_MATCH_CNT,
	/* keep last */
	QCA_WLAN_VENDOR_GET_WAKE_STATS_AFTER_LAST,
	QCA_WLAN_VENDOR_GET_WAKE_STATS_MAX =
		QCA_WLAN_VENDOR_GET_WAKE_STATS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_thermal_level - Defines various thermal levels
 * configured by userspace to the driver/firmware.
 * The values can be encapsulated in QCA_WLAN_VENDOR_ATTR_THERMAL_LEVEL or
 * QCA_WLAN_VENDOR_ATTR_THERMAL_EVENT_LEVEL attribute.
 * The driver/firmware takes actions requested by userspace such as throttling
 * wifi TX etc. in order to mitigate high temperature.
 *
 * @QCA_WLAN_VENDOR_THERMAL_LEVEL_NONE: Stop/clear all throttling actions.
 * @QCA_WLAN_VENDOR_THERMAL_LEVEL_LIGHT: Throttle TX lightly.
 * @QCA_WLAN_VENDOR_THERMAL_LEVEL_MODERATE: Throttle TX moderately.
 * @QCA_WLAN_VENDOR_THERMAL_LEVEL_SEVERE: Throttle TX severely.
 * @QCA_WLAN_VENDOR_THERMAL_LEVEL_CRITICAL: Critical thermal level reached.
 * @QCA_WLAN_VENDOR_THERMAL_LEVEL_EMERGENCY: Emergency thermal level reached.
 */
enum qca_wlan_vendor_thermal_level {
	QCA_WLAN_VENDOR_THERMAL_LEVEL_NONE = 0,
	QCA_WLAN_VENDOR_THERMAL_LEVEL_LIGHT = 1,
	QCA_WLAN_VENDOR_THERMAL_LEVEL_MODERATE = 2,
	QCA_WLAN_VENDOR_THERMAL_LEVEL_SEVERE = 3,
	QCA_WLAN_VENDOR_THERMAL_LEVEL_CRITICAL = 4,
	QCA_WLAN_VENDOR_THERMAL_LEVEL_EMERGENCY = 5,
};

/**
 * enum qca_wlan_vendor_attr_thermal_cmd - Vendor subcmd attributes to set
 * cmd value. Used for NL attributes for data used by
 * QCA_NL80211_VENDOR_SUBCMD_THERMAL_CMD sub command.
 */
enum qca_wlan_vendor_attr_thermal_cmd {
	QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_INVALID = 0,
	/* The value of command, driver will implement different operations
	 * according to this value. It uses values defined in
	 * enum qca_wlan_vendor_attr_thermal_cmd_type.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_VALUE = 1,
	/* Userspace uses this attribute to configure thermal level to the
	 * driver/firmware, or get thermal level from the driver/firmware.
	 * Used in request or response, u32 attribute,
	 * possible values are defined in enum qca_wlan_vendor_thermal_level.
	 */
	QCA_WLAN_VENDOR_ATTR_THERMAL_LEVEL = 2,
	/* Userspace uses this attribute to configure the time in which the
	 * driver/firmware should complete applying settings it received from
	 * userspace with QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_SET_LEVEL
	 * command type. Used in request, u32 attribute, value is in
	 * milliseconds. A value of zero indicates to apply the settings
	 * immediately. The driver/firmware can delay applying the configured
	 * thermal settings within the time specified in this attribute if
	 * there is any critical ongoing operation.
	 */
	QCA_WLAN_VENDOR_ATTR_THERMAL_COMPLETION_WINDOW = 3,
	/* Nested attribute, the driver/firmware uses this attribute to report
	 * thermal statistics of different thermal levels to userspace when
	 * requested using the
	 * QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_GET_THERMAL_STATS command
	 * type. This attribute contains a nested array of records of thermal
	 * statistics of multiple levels. The attributes used inside this nested
	 * attribute are defined in enum qca_wlan_vendor_attr_thermal_stats.
	 */
	QCA_WLAN_VENDOR_ATTR_THERMAL_STATS = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_MAX =
	QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_AFTER_LAST - 1
};

/**
 * qca_wlan_vendor_attr_thermal_cmd_type: Attribute values for
 * QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_VALUE to the vendor subcmd
 * QCA_NL80211_VENDOR_SUBCMD_THERMAL_CMD. This represents the
 * thermal command types sent to driver.
 * @QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_GET_PARAMS: Request to
 * get thermal shutdown configuration parameters for display. Parameters
 * responded from driver are defined in
 * enum qca_wlan_vendor_attr_get_thermal_params_rsp.
 * @QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_GET_TEMPERATURE: Request to
 * get temperature. Host should respond with a temperature data. It is defined
 * in enum qca_wlan_vendor_attr_thermal_get_temperature.
 * @QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_SUSPEND: Request to execute thermal
 * suspend action.
 * @QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_RESUME: Request to execute thermal
 * resume action.
 * @QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_SET_LEVEL: Configure thermal level to
 * the driver/firmware.
 * @QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_GET_LEVEL: Request to get the current
 * thermal level from the driver/firmware. The driver should respond with a
 * thermal level defined in enum qca_wlan_vendor_thermal_level.
 * @QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_GET_THERMAL_STATS: Request to get the
 * current thermal statistics from the driver/firmware. The driver should
 * respond with statistics of all thermal levels encapsulated in the attribute
 * QCA_WLAN_VENDOR_ATTR_THERMAL_STATS.
 * @QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_CLEAR_THERMAL_STATS: Request to clear
 * the current thermal statistics for all thermal levels maintained in the
 * driver/firmware and start counting from zero again.
 */
enum qca_wlan_vendor_attr_thermal_cmd_type {
	QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_GET_PARAMS,
	QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_GET_TEMPERATURE,
	QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_SUSPEND,
	QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_RESUME,
	QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_SET_LEVEL,
	QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_GET_LEVEL,
	QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_GET_THERMAL_STATS,
	QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_CLEAR_THERMAL_STATS,
};

/**
 * enum qca_wlan_vendor_attr_thermal_get_temperature - vendor subcmd attributes
 * to get chip temperature by user.
 * enum values are used for NL attributes for data used by
 * QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_GET_TEMPERATURE command for data used
 * by QCA_NL80211_VENDOR_SUBCMD_THERMAL_CMD sub command.
 */
enum qca_wlan_vendor_attr_thermal_get_temperature {
	QCA_WLAN_VENDOR_ATTR_THERMAL_GET_TEMPERATURE_INVALID = 0,
	/* Temperature value (degree Celsius) from driver.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_THERMAL_GET_TEMPERATURE_DATA,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_THERMAL_GET_TEMPERATURE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_THERMAL_GET_TEMPERATURE_MAX =
	QCA_WLAN_VENDOR_ATTR_THERMAL_GET_TEMPERATURE_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_get_thermal_params_rsp - vendor subcmd attributes
 * to get configuration parameters of thermal shutdown feature. Enum values are
 * used by QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_GET_PARAMS command for data
 * used by QCA_NL80211_VENDOR_SUBCMD_THERMAL_CMD sub command.
 */
enum qca_wlan_vendor_attr_get_thermal_params_rsp {
	QCA_WLAN_VENDOR_ATTR_GET_THERMAL_PARAMS_RSP_INVALID = 0,
	/* Indicate if the thermal shutdown feature is enabled.
	 * NLA_FLAG attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_GET_THERMAL_PARAMS_RSP_SHUTDOWN_EN,
	/* Indicate if the auto mode is enabled.
	 * Enable: Driver triggers the suspend/resume action.
	 * Disable: User space triggers the suspend/resume action.
	 * NLA_FLAG attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_GET_THERMAL_PARAMS_RSP_SHUTDOWN_AUTO_EN,
	/* Thermal resume threshold (degree Celsius). Issue the resume command
	 * if the temperature value is lower than this threshold.
	 * u16 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_GET_THERMAL_PARAMS_RSP_RESUME_THRESH,
	/* Thermal warning threshold (degree Celsius). FW reports temperature
	 * to driver if it's higher than this threshold.
	 * u16 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_GET_THERMAL_PARAMS_RSP_WARNING_THRESH,
	/* Thermal suspend threshold (degree Celsius). Issue the suspend command
	 * if the temperature value is higher than this threshold.
	 * u16 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_GET_THERMAL_PARAMS_RSP_SUSPEND_THRESH,
	/* FW reports temperature data periodically at this interval (ms).
	 * u16 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_GET_THERMAL_PARAMS_RSP_SAMPLE_RATE,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_GET_THERMAL_PARAMS_RSP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_GET_THERMAL_PARAMS_RSP_MAX =
	QCA_WLAN_VENDOR_ATTR_GET_THERMAL_PARAMS_RSP_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_thermal_event - vendor subcmd attributes to
 * report thermal events from driver to user space.
 * enum values are used for NL attributes for data used by
 * QCA_NL80211_VENDOR_SUBCMD_THERMAL_EVENT sub command.
 */
enum qca_wlan_vendor_attr_thermal_event {
	QCA_WLAN_VENDOR_ATTR_THERMAL_EVENT_INVALID = 0,
	/* Temperature value (degree Celsius) from driver.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_THERMAL_EVENT_TEMPERATURE,
	/* Indication of resume completion from power save mode.
	 * NLA_FLAG attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_THERMAL_EVENT_RESUME_COMPLETE,
	/* Thermal level from the driver.
	 * u32 attribute. Possible values are defined in
	 * enum qca_wlan_vendor_thermal_level.
	 */
	QCA_WLAN_VENDOR_ATTR_THERMAL_EVENT_LEVEL = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_THERMAL_EVENT_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_THERMAL_EVENT_MAX =
	QCA_WLAN_VENDOR_ATTR_THERMAL_EVENT_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_thermal_stats - vendor subcmd attributes
 * to get thermal status from the driver/firmware.
 * enum values are used for NL attributes encapsulated inside the
 * QCA_WLAN_VENDOR_ATTR_THERMAL_STATS nested attribute.
 *
 * QCA_WLAN_VENDOR_ATTR_THERMAL_STATS_MIN_TEMPERATURE: Minimum temperature
 * of a thermal level in Celsius. u32 size.
 * QCA_WLAN_VENDOR_ATTR_THERMAL_STATS_MAX_TEMPERATURE: Maximum temperature
 * of a thermal level in Celsius. u32 size.
 * QCA_WLAN_VENDOR_ATTR_THERMAL_STATS_DWELL_TIME: The total time spent on each
 * thermal level in milliseconds. u32 size.
 * QCA_WLAN_VENDOR_ATTR_THERMAL_STATS_TEMP_LEVEL_COUNTER: Indicates the number
 * of times the temperature crossed into the temperature range defined by the
 * thermal level from both higher and lower directions. u32 size.
 */
enum qca_wlan_vendor_attr_thermal_stats {
	QCA_WLAN_VENDOR_ATTR_THERMAL_STATS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_THERMAL_STATS_MIN_TEMPERATURE,
	QCA_WLAN_VENDOR_ATTR_THERMAL_STATS_MAX_TEMPERATURE,
	QCA_WLAN_VENDOR_ATTR_THERMAL_STATS_DWELL_TIME,
	QCA_WLAN_VENDOR_ATTR_THERMAL_STATS_TEMP_LEVEL_COUNTER,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_THERMAL_STATS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_THERMAL_STATS_MAX =
	QCA_WLAN_VENDOR_ATTR_THERMAL_STATS_AFTER_LAST - 1,
};

/**
 * enum he_fragmentation_val - HE fragmentation support values
 * Indicates level of dynamic fragmentation that is supported by
 * a STA as a recipient.
 * HE fragmentation values are defined in IEEE P802.11ax/D2.0, 9.4.2.237.2
 * (HE MAC Capabilities Information field) and are used in HE Capabilities
 * element to advertise the support. These values are validated in the driver
 * to check the device capability and advertised in the HE Capabilities
 * element. These values are used to configure testbed device to allow the
 * advertised hardware capabilities to be downgraded for testing purposes.
 *
 * @HE_FRAG_DISABLE: no support for dynamic fragmentation
 * @HE_FRAG_LEVEL1: support for dynamic fragments that are
 *	contained within an MPDU or S-MPDU, no support for dynamic fragments
 *	within an A-MPDU that is not an S-MPDU.
 * @HE_FRAG_LEVEL2: support for dynamic fragments that are
 *	contained within an MPDU or S-MPDU and support for up to one dynamic
 *	fragment for each MSDU, each A-MSDU if supported by the recipient, and
 *	each MMPDU within an A-MPDU or multi-TID A-MPDU that is not an
 *	MPDU or S-MPDU.
 * @HE_FRAG_LEVEL3: support for dynamic fragments that are
 *	contained within an MPDU or S-MPDU and support for multiple dynamic
 *	fragments for each MSDU and for each A-MSDU if supported by the
 *	recipient within an A-MPDU or multi-TID AMPDU and up to one dynamic
 *	fragment for each MMPDU in a multi-TID A-MPDU that is not an S-MPDU.
 */
enum he_fragmentation_val {
	HE_FRAG_DISABLE,
	HE_FRAG_LEVEL1,
	HE_FRAG_LEVEL2,
	HE_FRAG_LEVEL3,
};

/**
 * enum he_mcs_config - HE MCS support configuration
 *
 * Configures the HE Tx/Rx MCS map in HE capability IE for given bandwidth.
 * These values are used in driver to configure the HE MCS map to advertise
 * Tx/Rx MCS map in HE capability and these values are applied for all the
 * streams supported by the device. To configure MCS for different bandwidths,
 * vendor command needs to be sent using this attribute with appropriate value.
 * For example, to configure HE_80_MCS_0_7, send vendor command using HE MCS
 * attribute with HE_80_MCS0_7. And to configure HE MCS for HE_160_MCS0_11
 * send this command using HE MCS config attribute with value HE_160_MCS0_11.
 * These values are used to configure testbed device to allow the advertised
 * hardware capabilities to be downgraded for testing purposes. The enum values
 * are defined such that BIT[1:0] indicates the MCS map value. Values 3,7 and
 * 11 are not used as BIT[1:0] value is 3 which is used to disable MCS map.
 * These values are validated in the driver before setting the MCS map and
 * driver returns error if the input is other than these enum values.
 *
 * @HE_80_MCS0_7: support for HE 80/40/20 MHz MCS 0 to 7
 * @HE_80_MCS0_9: support for HE 80/40/20 MHz MCS 0 to 9
 * @HE_80_MCS0_11: support for HE 80/40/20 MHz MCS 0 to 11
 * @HE_160_MCS0_7: support for HE 160 MHz MCS 0 to 7
 * @HE_160_MCS0_9: support for HE 160 MHz MCS 0 to 9
 * @HE_160_MCS0_11: support for HE 160 MHz MCS 0 to 11
 * @HE_80P80_MCS0_7: support for HE 80p80 MHz MCS 0 to 7
 * @HE_80P80_MCS0_9: support for HE 80p80 MHz MCS 0 to 9
 * @HE_80P80_MCS0_11: support for HE 80p80 MHz MCS 0 to 11
 */
enum he_mcs_config {
	HE_80_MCS0_7 = 0,
	HE_80_MCS0_9 = 1,
	HE_80_MCS0_11 = 2,
	HE_160_MCS0_7 = 4,
	HE_160_MCS0_9 = 5,
	HE_160_MCS0_11 = 6,
	HE_80P80_MCS0_7 = 8,
	HE_80P80_MCS0_9 = 9,
	HE_80P80_MCS0_11 = 10,
};

/**
 * enum qca_wlan_ba_session_config - BA session configuration
 *
 * Indicates the configuration values for BA session configuration attribute.
 *
 * @QCA_WLAN_ADD_BA: Establish a new BA session with given configuration.
 * @QCA_WLAN_DELETE_BA: Delete the existing BA session for given TID.
 */
enum qca_wlan_ba_session_config {
	QCA_WLAN_ADD_BA = 1,
	QCA_WLAN_DELETE_BA = 2,
};

/**
 * enum qca_wlan_ac_type - Access category type
 *
 * Indicates the access category type value.
 *
 * @QCA_WLAN_AC_BE: BE access category
 * @QCA_WLAN_AC_BK: BK access category
 * @QCA_WLAN_AC_VI: VI access category
 * @QCA_WLAN_AC_VO: VO access category
 * @QCA_WLAN_AC_ALL: All ACs
 */
enum qca_wlan_ac_type {
	QCA_WLAN_AC_BE = 0,
	QCA_WLAN_AC_BK = 1,
	QCA_WLAN_AC_VI = 2,
	QCA_WLAN_AC_VO = 3,
	QCA_WLAN_AC_ALL = 4,
};

/**
 * enum qca_wlan_he_ltf_cfg - HE LTF configuration
 *
 * Indicates the HE LTF configuration value.
 *
 * @QCA_WLAN_HE_LTF_AUTO: HE-LTF is automatically set to the mandatory HE-LTF,
 * based on the GI setting
 * @QCA_WLAN_HE_LTF_1X: 1X HE LTF is 3.2us LTF
 * @QCA_WLAN_HE_LTF_2X: 2X HE LTF is 6.4us LTF
 * @QCA_WLAN_HE_LTF_4X: 4X HE LTF is 12.8us LTF
 */
enum qca_wlan_he_ltf_cfg {
	QCA_WLAN_HE_LTF_AUTO = 0,
	QCA_WLAN_HE_LTF_1X = 1,
	QCA_WLAN_HE_LTF_2X = 2,
	QCA_WLAN_HE_LTF_4X = 3,
};

/**
 * enum qca_wlan_he_mac_padding_dur - HE trigger frame MAC padding duration
 *
 * Indicates the HE trigger frame MAC padding duration value.
 *
 * @QCA_WLAN_HE_NO_ADDITIONAL_PROCESS_TIME: no additional time required to
 * process the trigger frame.
 * @QCA_WLAN_HE_8US_OF_PROCESS_TIME: indicates the 8us of processing time for
 * trigger frame.
 * @QCA_WLAN_HE_16US_OF_PROCESS_TIME: indicates the 16us of processing time for
 * trigger frame.
 */
enum qca_wlan_he_mac_padding_dur {
	QCA_WLAN_HE_NO_ADDITIONAL_PROCESS_TIME = 0,
	QCA_WLAN_HE_8US_OF_PROCESS_TIME = 1,
	QCA_WLAN_HE_16US_OF_PROCESS_TIME = 2,
};

/**
 * enum qca_wlan_he_om_ctrl_ch_bw - HE OM control field BW configuration
 *
 * Indicates the HE Operating mode control channel width setting value.
 *
 * @QCA_WLAN_HE_OM_CTRL_BW_20M: Primary 20 MHz
 * @QCA_WLAN_HE_OM_CTRL_BW_40M: Primary 40 MHz
 * @QCA_WLAN_HE_OM_CTRL_BW_80M: Primary 80 MHz
 * @QCA_WLAN_HE_OM_CTRL_BW_160M: 160 MHz and 80+80 MHz
 */
enum qca_wlan_he_om_ctrl_ch_bw {
	QCA_WLAN_HE_OM_CTRL_BW_20M = 0,
	QCA_WLAN_HE_OM_CTRL_BW_40M = 1,
	QCA_WLAN_HE_OM_CTRL_BW_80M = 2,
	QCA_WLAN_HE_OM_CTRL_BW_160M = 3,
};

/**
 * enum qca_wlan_keep_alive_data_type - Keep alive data type configuration
 *
 * Indicates the frame types to use for keep alive data.
 *
 * @QCA_WLAN_KEEP_ALIVE_DEFAULT: Driver default type used for keep alive.
 * @QCA_WLAN_KEEP_ALIVE_DATA: Data frame type for keep alive.
 * @QCA_WLAN_KEEP_ALIVE_MGMT: Management frame type for keep alive.
 */
enum qca_wlan_keep_alive_data_type {
	QCA_WLAN_KEEP_ALIVE_DEFAULT = 0,
	QCA_WLAN_KEEP_ALIVE_DATA = 1,
	QCA_WLAN_KEEP_ALIVE_MGMT = 2,
};

/**
 * enum eht_mcs_config - EHT MCS support configuration
 *
 * Configures the EHT Tx/Rx MCS map in EHT Capability element.
 * These values are used in the driver to configure the EHT MCS map to advertise
 * Tx/Rx MCS map in EHT capability and these values are applied for all the
 * streams supported by the device.
 * @EHT_MCS0_7: EHT MCS 0 to 7 support
 * @EHT_MCS0_9: EHT MCS 0 to 9 support
 * @EHT_MCS0_11: EHT MCS 0 to 11 support
 * @EHT_MCS0_13: EHT MCS 0 to 13 support
 */
enum eht_mcs_config {
	EHT_MCS0_7 = 0,
	EHT_MCS0_9 = 1,
	EHT_MCS0_11 = 2,
	EHT_MCS0_13 = 3,
};

/**
 * enum qca_wlan_eht_mlo_mode: EHT MLO mode configuration.
 * @QCA_WLAN_EHT_MODE_INVALID: Invalid.
 * @QCA_WLAN_EHT_MLSR: Multi-link single radio mode
 * @QCA_WLAN_EHT_EMLSR: Enhanced multi-link single radio mode.
 * @QCA_WLAN_EHT_NON_STR_MLMR: Non simultaneous transmit and receive
 * multi-link multi radio mode.
 * @QCA_WLAN_EHT_STR_MLMR: Simultaneous transmit and receive
 * multi-link multi radio mode.
 */
enum qca_wlan_eht_mlo_mode {
	QCA_WLAN_EHT_MODE_INVALID = 0,
	QCA_WLAN_EHT_MLSR = 1,
	QCA_WLAN_EHT_EMLSR = 2,
	QCA_WLAN_EHT_NON_STR_MLMR = 3,
	QCA_WLAN_EHT_STR_MLMR = 4,
};

/**
 * enum qca_wlan_emlsr_mode: Enhanced Multi-link Single Radio mode configuration
 * @QCA_WLAN_EMLSR_MODE_ENTER: Enter EMLSR mode
 * @QCA_WLAN_EMLSR_MODE_EXIT: Exit EMLSR mode
 */
enum qca_wlan_emlsr_mode {
	QCA_WLAN_EMLSR_MODE_ENTER = 0,
	QCA_WLAN_EMLSR_MODE_EXIT = 1,
};

/**
 * enum qca_wlan_ttlm_negotiation_support: TID-To-Link Mapping Negotiation
 * support
 * @QCA_WLAN_TTLM_DISABLE: TTLM disabled
 * @QCA_WLAN_TTLM_SAME_LINK_SET: Mapping of all TIDs to the same link set,
 * both DL and UL
 * @QCA_WLAN_TTLM_SAME_DIFF_LINK_SET: Mapping of each TID to the same or
 * different link set
 */
enum qca_wlan_ttlm_negotiation_support {
	QCA_WLAN_TTLM_DISABLE = 0,
	QCA_WLAN_TTLM_SAME_LINK_SET = 1,
	QCA_WLAN_TTLM_SAME_DIFF_LINK_SET = 2,
};

/**
 * enum qca_coex_traffic_shaping_mode: Coex traffic shaping mode
 * @QCA_COEX_TRAFFIC_SHAPING_MODE_DISABLE: Coex policies disabled
 * @QCA_COEX_TRAFFIC_SHAPING_MODE_ENABLE: All coex policies enabled
 */
enum qca_coex_traffic_shaping_mode {
	QCA_COEX_TRAFFIC_SHAPING_MODE_DISABLE = 0,
	QCA_COEX_TRAFFIC_SHAPING_MODE_ENABLE = 1,
};

/**
 * enum qca_wlan_vendor_attr_omi_tx: Represents attributes for HE and
 * EHT operating mode control transmit request. These attributes are
 * sent as part of QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_OMI_TX and
 * QCA_NL80211_VENDOR_SUBCMD_WIFI_TEST_CONFIGURATION.
 *
 * @QCA_WLAN_VENDOR_ATTR_HE_OMI_RX_NSS: Mandatory 8-bit unsigned value
 * indicates the maximum number of spatial streams, NSS, that the STA
 * supports in reception for PPDU bandwidths less than or equal to 80 MHz
 * and is set to NSS - 1.
 *
 * @QCA_WLAN_VENDOR_ATTR_HE_OMI_CH_BW: Mandatory 8-bit unsigned value
 * indicates the operating channel width supported by the STA for both
 * reception and transmission. Uses enum qca_wlan_he_om_ctrl_ch_bw values.
 *
 * @QCA_WLAN_VENDOR_ATTR_HE_OMI_ULMU_DISABLE: Mandatory 8-bit unsigned value
 * indicates the all trigger based UL MU operations by the STA.
 * 0 - UL MU operations are enabled by the STA.
 * 1 - All triggered UL MU transmissions are suspended by the STA.
 *
 * @QCA_WLAN_VENDOR_ATTR_HE_OMI_TX_NSTS: Mandatory 8-bit unsigned value
 * indicates the maximum number of space-time streams, NSTS, that
 * the STA supports in transmission and is set to NSTS - 1.
 *
 * @QCA_WLAN_VENDOR_ATTR_HE_OMI_ULMU_DATA_DISABLE: 8-bit unsigned value
 * combined with the UL MU Disable subfield and the recipient's setting
 * of the OM Control UL MU Data Disable RX Support subfield in the HE MAC
 * capabilities to determine which HE TB PPDUs are possible by the
 * STA to transmit.
 * 0 - UL MU data operations are enabled by the STA.
 * 1 - Determine which HE TB PPDU types are allowed by the STA if UL MU disable
 * bit is not set, else UL MU Tx is suspended.
 *
 * @QCA_WLAN_VENDOR_ATTR_EHT_OMI_RX_NSS_EXTN: 8-bit unsigned value in the EHT OM
 * Control subfield combined with the Rx NSS subfield in the OM Control subfield
 * indicates NSS - 1, where NSS is the maximum number of spatial streams that
 * STA supports in reception for PPDU bandwidths less than or equal to 80 MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_EHT_OMI_CH_BW_EXTN: 8-bit unsigned value indicates
 * 320 MHz operating channel width supported by the EHT STA for both reception
 * and transmission.
 *
 * @QCA_WLAN_VENDOR_ATTR_EHT_OMI_TX_NSS_EXTN: 8-bit unsigned value in the EHT OM
 * Control subfield combined with the Tx NSTS subfield in OM Control subfield
 * indicates NSTS - 1, where NSTS is the maximum number of space-time streams
 * that the STA supports in transmission for PPDU bandwidths less than or equal
 * to 80 MHz.
 */
enum qca_wlan_vendor_attr_omi_tx {
	QCA_WLAN_VENDOR_ATTR_HE_OMI_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_HE_OMI_RX_NSS = 1,
	QCA_WLAN_VENDOR_ATTR_HE_OMI_CH_BW = 2,
	QCA_WLAN_VENDOR_ATTR_HE_OMI_ULMU_DISABLE = 3,
	QCA_WLAN_VENDOR_ATTR_HE_OMI_TX_NSTS = 4,
	QCA_WLAN_VENDOR_ATTR_HE_OMI_ULMU_DATA_DISABLE = 5,
	QCA_WLAN_VENDOR_ATTR_EHT_OMI_RX_NSS_EXTN = 6,
	QCA_WLAN_VENDOR_ATTR_EHT_OMI_CH_BW_EXTN = 7,
	QCA_WLAN_VENDOR_ATTR_EHT_OMI_TX_NSS_EXTN = 8,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_OMI_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_OMI_MAX =
	QCA_WLAN_VENDOR_ATTR_OMI_AFTER_LAST - 1,
};

/* deprecated legacy names */
#define QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_OMI_TX \
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_OMI_TX
#define qca_wlan_vendor_attr_he_omi_tx \
	qca_wlan_vendor_attr_omi_tx
#define QCA_WLAN_VENDOR_ATTR_HE_OMI_AFTER_LAST \
	QCA_WLAN_VENDOR_ATTR_OMI_AFTER_LAST
#define QCA_WLAN_VENDOR_ATTR_HE_OMI_MAX \
	QCA_WLAN_VENDOR_ATTR_OMI_MAX

 /**
  * enum qca_wlan_vendor_phy_mode - Different PHY modes
  * These values are used with %QCA_WLAN_VENDOR_ATTR_CONFIG_PHY_MODE.
  *
  * @QCA_WLAN_VENDOR_PHY_MODE_AUTO: autoselect
  * @QCA_WLAN_VENDOR_PHY_MODE_2G_AUTO: 2.4 GHz 802.11b/g/n/ax autoselect
  * @QCA_WLAN_VENDOR_PHY_MODE_5G_AUTO: 5 GHz 802.11a/n/ac/ax autoselect
  * @QCA_WLAN_VENDOR_PHY_MODE_11A: 5 GHz, OFDM
  * @QCA_WLAN_VENDOR_PHY_MODE_11B: 2.4 GHz, CCK
  * @QCA_WLAN_VENDOR_PHY_MODE_11G: 2.4 GHz, OFDM
  * @QCA_WLAN_VENDOR_PHY_MODE_11AGN: Support 802.11n in both 2.4 GHz and 5 GHz
  * @QCA_WLAN_VENDOR_PHY_MODE_11NG_HT20: 2.4 GHz, HT20
  * @QCA_WLAN_VENDOR_PHY_MODE_11NG_HT40PLUS: 2.4 GHz, HT40 (ext ch +1)
  * @QCA_WLAN_VENDOR_PHY_MODE_11NG_HT40MINUS: 2.4 GHz, HT40 (ext ch -1)
  * @QCA_WLAN_VENDOR_PHY_MODE_11NG_HT40: 2.4 GHz, Auto HT40
  * @QCA_WLAN_VENDOR_PHY_MODE_11NA_HT20: 5 GHz, HT20
  * @QCA_WLAN_VENDOR_PHY_MODE_11NA_HT40PLUS: 5 GHz, HT40 (ext ch +1)
  * @QCA_WLAN_VENDOR_PHY_MODE_11NA_HT40MINUS: 5 GHz, HT40 (ext ch -1)
  * @QCA_WLAN_VENDOR_PHY_MODE_11NA_HT40: 5 GHz, Auto HT40
  * @QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT20: 5 GHz, VHT20
  * @QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT40PLUS: 5 GHz, VHT40 (Ext ch +1)
  * @QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT40MINUS: 5 GHz VHT40 (Ext ch -1)
  * @QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT40: 5 GHz, VHT40
  * @QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT80: 5 GHz, VHT80
  * @QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT80P80: 5 GHz, VHT80+80
  * @QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT160: 5 GHz, VHT160
  * @QCA_WLAN_VENDOR_PHY_MODE_11AX_HE20: HE20
  * @QCA_WLAN_VENDOR_PHY_MODE_11AX_HE40: HE40
  * @QCA_WLAN_VENDOR_PHY_MODE_11AX_HE40PLUS: HE40 (ext ch +1)
  * @QCA_WLAN_VENDOR_PHY_MODE_11AX_HE40MINUS: HE40 (ext ch -1)
  * @QCA_WLAN_VENDOR_PHY_MODE_11AX_HE80: HE80
  * @QCA_WLAN_VENDOR_PHY_MODE_11AX_HE80P80: HE 80P80
  * @QCA_WLAN_VENDOR_PHY_MODE_11AX_HE160: HE160
  */
enum qca_wlan_vendor_phy_mode {
	QCA_WLAN_VENDOR_PHY_MODE_AUTO = 0,
	QCA_WLAN_VENDOR_PHY_MODE_2G_AUTO = 1,
	QCA_WLAN_VENDOR_PHY_MODE_5G_AUTO = 2,
	QCA_WLAN_VENDOR_PHY_MODE_11A = 3,
	QCA_WLAN_VENDOR_PHY_MODE_11B = 4,
	QCA_WLAN_VENDOR_PHY_MODE_11G = 5,
	QCA_WLAN_VENDOR_PHY_MODE_11AGN = 6,
	QCA_WLAN_VENDOR_PHY_MODE_11NG_HT20 = 7,
	QCA_WLAN_VENDOR_PHY_MODE_11NG_HT40PLUS = 8,
	QCA_WLAN_VENDOR_PHY_MODE_11NG_HT40MINUS = 9,
	QCA_WLAN_VENDOR_PHY_MODE_11NG_HT40 = 10,
	QCA_WLAN_VENDOR_PHY_MODE_11NA_HT20 = 11,
	QCA_WLAN_VENDOR_PHY_MODE_11NA_HT40PLUS = 12,
	QCA_WLAN_VENDOR_PHY_MODE_11NA_HT40MINUS = 13,
	QCA_WLAN_VENDOR_PHY_MODE_11NA_HT40 = 14,
	QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT20 = 15,
	QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT40PLUS = 16,
	QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT40MINUS = 17,
	QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT40 = 18,
	QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT80 = 19,
	QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT80P80 = 20,
	QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT160 = 21,
	QCA_WLAN_VENDOR_PHY_MODE_11AX_HE20 = 22,
	QCA_WLAN_VENDOR_PHY_MODE_11AX_HE40 = 23,
	QCA_WLAN_VENDOR_PHY_MODE_11AX_HE40PLUS = 24,
	QCA_WLAN_VENDOR_PHY_MODE_11AX_HE40MINUS = 25,
	QCA_WLAN_VENDOR_PHY_MODE_11AX_HE80 = 26,
	QCA_WLAN_VENDOR_PHY_MODE_11AX_HE80P80 = 27,
	QCA_WLAN_VENDOR_PHY_MODE_11AX_HE160 = 28,
};

/* Attributes for data used by
 * QCA_NL80211_VENDOR_SUBCMD_WIFI_TEST_CONFIGURATION
 */
enum qca_wlan_vendor_attr_wifi_test_config {
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_INVALID = 0,
	/* 8-bit unsigned value to configure the driver to enable/disable
	 * WMM feature. This attribute is used to configure testbed device.
	 * 1-enable, 0-disable
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_WMM_ENABLE = 1,

	/* 8-bit unsigned value to configure the driver to accept/reject
	 * the addba request from peer. This attribute is used to configure
	 * the testbed device.
	 * 1-accept addba, 0-reject addba
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_ACCEPT_ADDBA_REQ = 2,

	/* 8-bit unsigned value to configure the driver to send or not to
	 * send the addba request to peer.
	 * This attribute is used to configure the testbed device.
	 * 1-send addba, 0-do not send addba
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_SEND_ADDBA_REQ = 3,

	/* 8-bit unsigned value to indicate the HE fragmentation support.
	 * Uses enum he_fragmentation_val values.
	 * This attribute is used to configure the testbed device to
	 * allow the advertised hardware capabilities to be downgraded
	 * for testing purposes.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_FRAGMENTATION = 4,

	/* 8-bit unsigned value to indicate the HE MCS support.
	 * Uses enum he_mcs_config values.
	 * This attribute is used to configure the testbed device to
	 * allow the advertised hardware capabilities to be downgraded
	 * for testing purposes.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_MCS = 5,

	/* 8-bit unsigned value to configure the driver to allow or not to
	 * allow the connection with WEP/TKIP in HT/VHT/HE modes.
	 * This attribute is used to configure the testbed device.
	 * 1-allow WEP/TKIP in HT/VHT/HE, 0-do not allow WEP/TKIP.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_WEP_TKIP_IN_HE = 6,

	/* 8-bit unsigned value to configure the driver to add a
	 * new BA session or delete the existing BA session for
	 * given TID. ADDBA command uses the buffer size and TID
	 * configuration if user specifies the values else default
	 * value for buffer size is used for all TIDs if the TID
	 * also not specified. For DEL_BA command TID value is
	 * required to process the command.
	 * Uses enum qca_wlan_ba_session_config values.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_ADD_DEL_BA_SESSION = 7,

	/* 16-bit unsigned value to configure the buffer size in addba
	 * request and response frames.
	 * This attribute is used to configure the testbed device.
	 * The range of the value is 0 to 256.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_ADDBA_BUFF_SIZE = 8,

	/* 8-bit unsigned value to configure the buffer size in addba
	 * request and response frames.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_BA_TID = 9,

	/* 8-bit unsigned value to configure the no ack policy.
	 * To configure no ack policy, access category value is
	 * required to process the command.
	 * This attribute is used to configure the testbed device.
	 * 1 - enable no ack, 0 - disable no ack.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_ENABLE_NO_ACK = 10,

	/* 8-bit unsigned value to configure the AC for no ack policy
	 * This attribute is used to configure the testbed device.
	 * Uses the enum qca_wlan_ac_type values.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_NO_ACK_AC = 11,

	/* 8-bit unsigned value to configure the HE LTF
	 * This attribute is used to configure the testbed device.
	 * Uses the enum qca_wlan_he_ltf_cfg values.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_LTF = 12,

	/* 8-bit unsigned value to configure the tx beamformee.
	 * This attribute is used to configure the testbed device.
	 * 1-enable, 0-disable.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_ENABLE_TX_BEAMFORMEE = 13,

	/* 8-bit unsigned value to configure the tx beamformee number
	 * of space-time streams.
	 * This attribute is used to configure the testbed device.
	 * The range of the value is 0 to 8.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_TX_BEAMFORMEE_NSTS = 14,

	/* 8-bit unsigned value to configure the MU EDCA params for given AC
	 * This attribute is used to configure the testbed device.
	 * Uses the enum qca_wlan_ac_type values.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_MU_EDCA_AC = 15,

	/* 8-bit unsigned value to configure the MU EDCA AIFSN for given AC
	 * To configure MU EDCA AIFSN value, MU EDCA access category value
	 * is required to process the command.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_MU_EDCA_AIFSN = 16,

	/* 8-bit unsigned value to configure the MU EDCA ECW min value for
	 * given AC.
	 * To configure MU EDCA ECW min value, MU EDCA access category value
	 * is required to process the command.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_MU_EDCA_ECWMIN = 17,

	/* 8-bit unsigned value to configure the MU EDCA ECW max value for
	 * given AC.
	 * To configure MU EDCA ECW max value, MU EDCA access category value
	 * is required to process the command.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_MU_EDCA_ECWMAX = 18,

	/* 8-bit unsigned value to configure the MU EDCA timer for given AC
	 * To configure MU EDCA timer value, MU EDCA access category value
	 * is required to process the command.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_MU_EDCA_TIMER = 19,

	/* 8-bit unsigned value to configure the HE trigger frame MAC padding
	 * duration.
	 * This attribute is used to configure the testbed device.
	 * Uses the enum qca_wlan_he_mac_padding_dur values.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_MAC_PADDING_DUR = 20,

	/* 8-bit unsigned value to override the MU EDCA params to defaults
	 * regardless of the AP beacon MU EDCA params. If it is enabled use
	 * the default values else use the MU EDCA params from AP beacon.
	 * This attribute is used to configure the testbed device.
	 * 1-enable, 0-disable.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_OVERRIDE_MU_EDCA = 21,

	/* 8-bit unsigned value to configure the support for receiving
	 * an MPDU that contains an operating mode control subfield.
	 * This attribute is used to configure the testbed device.
	 * 1-enable, 0-disable.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_OM_CTRL_SUPP = 22,

	/* Nested attribute values required to setup the TWT session.
	 * enum qca_wlan_vendor_attr_twt_setup provides the necessary
	 * information to set up the session. It contains broadcast flags,
	 * set_up flags, trigger value, flow type, flow ID, wake interval
	 * exponent, protection, target wake time, wake duration, wake interval
	 * mantissa. These nested attributes are used to setup a host triggered
	 * TWT session.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_TWT_SETUP = 23,

	/* This nested attribute is used to terminate the current TWT session.
	 * It does not currently carry any attributes.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_TWT_TERMINATE = 24,

	/* This nested attribute is used to suspend the current TWT session.
	 * It does not currently carry any attributes.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_TWT_SUSPEND = 25,

	/* Nested attribute values to indicate the request for resume.
	 * This attribute is used to resume the TWT session.
	 * enum qca_wlan_vendor_attr_twt_resume provides the necessary
	 * parameters required to resume the TWT session.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_TWT_RESUME = 26,

	/* 8-bit unsigned value to set the HE operating mode control
	 * (OM CTRL) Channel Width subfield.
	 * The Channel Width subfield indicates the operating channel width
	 * supported by the STA for both reception and transmission.
	 * Uses the enum qca_wlan_he_om_ctrl_ch_bw values.
	 * This setting is cleared with the
	 * QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_CLEAR_HE_OM_CTRL_CONFIG
	 * flag attribute to reset defaults.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_OM_CTRL_BW = 27,

	/* 8-bit unsigned value to configure the number of spatial
	 * streams in HE operating mode control field.
	 * This setting is cleared with the
	 * QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_CLEAR_HE_OM_CTRL_CONFIG
	 * flag attribute to reset defaults.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_OM_CTRL_NSS = 28,

	/* Flag attribute to configure the UL MU disable bit in
	 * HE operating mode control field.
	 * This setting is cleared with the
	 * QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_CLEAR_HE_OM_CTRL_CONFIG
	 * flag attribute to reset defaults.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_OM_CTRL_UL_MU_DISABLE = 29,

	/* Flag attribute to clear the previously set HE operating mode
	 * control field configuration.
	 * This attribute is used to configure the testbed device to reset
	 * defaults to clear any previously set HE operating mode control
	 * field configuration.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_CLEAR_HE_OM_CTRL_CONFIG = 30,

	/* 8-bit unsigned value to configure HE single user PPDU
	 * transmission. By default this setting is disabled and it
	 * is disabled in the reset defaults of the device configuration.
	 * This attribute is used to configure the testbed device.
	 * 1-enable, 0-disable
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_TX_SUPPDU = 31,

	/* 8-bit unsigned value to configure action frame transmission
	 * in HE trigger based PPDU transmission.
	 * By default this setting is disabled and it is disabled in
	 * the reset defaults of the device configuration.
	 * This attribute is used to configure the testbed device.
	 * 1-enable, 0-disable
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_ACTION_TX_TB_PPDU = 32,

	/* Nested attribute to indicate HE operating mode control field
	 * transmission. It contains operating mode control field Nss,
	 * channel bandwidth, Tx Nsts and UL MU disable attributes.
	 * These nested attributes are used to send HE operating mode control
	 * with configured values.
	 * Uses the enum qca_wlan_vendor_attr_omi_tx attributes.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_OMI_TX = 33,

	/* 8-bit unsigned value to configure +HTC_HE support to indicate the
	 * support for the reception of a frame that carries an HE variant
	 * HT Control field.
	 * This attribute is used to configure the testbed device.
	 * 1-enable, 0-disable
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_HTC_HE_SUPP = 34,

	/* 8-bit unsigned value to configure VHT support in 2.4G band.
	 * This attribute is used to configure the testbed device.
	 * 1-enable, 0-disable
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_ENABLE_2G_VHT = 35,

	/* 8-bit unsigned value to configure HE testbed defaults.
	 * This attribute is used to configure the testbed device.
	 * 1-set the device HE capabilities to testbed defaults.
	 * 0-reset the device HE capabilities to supported config.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_SET_HE_TESTBED_DEFAULTS = 36,

	/* 8-bit unsigned value to configure TWT request support.
	 * This attribute is used to configure the testbed device.
	 * 1-enable, 0-disable.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_HE_TWT_REQ_SUPPORT = 37,

	/* 8-bit unsigned value to configure protection for Management
	 * frames when PMF is enabled for the association.
	 * This attribute is used to configure the testbed device.
	 * 0-use the correct key, 1-use an incorrect key, 2-disable protection.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_PMF_PROTECTION = 38,

	/* Flag attribute to inject Disassociation frame to the connected AP.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_DISASSOC_TX = 39,

	/* 8-bit unsigned value to configure an override for the RSNXE Used
	 * subfield in the MIC control field of the FTE in FT Reassociation
	 * Request frame.
	 * 0 - Default behavior, 1 - override with 1, 2 - override with 0.
	 * This attribute is used to configure the testbed device.
	 * This attribute can be configured only when STA is in associated state
	 * and the configuration is valid until the disconnection.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_FT_REASSOCREQ_RSNXE_USED = 40,

	/* 8-bit unsigned value to configure the driver to ignore CSA (Channel
	 * Switch Announcement) when STA is in connected state.
	 * 0 - Default behavior, 1 - Ignore CSA.
	 * This attribute is used to configure the testbed device.
	 * This attribute can be configured only when STA is in associated state
	 * and the configuration is valid until the disconnection.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_IGNORE_CSA = 41,

	/* Nested attribute values required to configure OCI (Operating Channel
	 * Information). Attributes defined in enum
	 * qca_wlan_vendor_attr_oci_override are nested within this attribute.
	 * This attribute is used to configure the testbed device.
	 * This attribute can be configured only when STA is in associated state
	 * and the configuration is valid until the disconnection.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_OCI_OVERRIDE = 42,

	/* 8-bit unsigned value to configure the driver/firmware to ignore SA
	 * Query timeout. If this configuration is enabled STA shall not send
	 * Deauthentication frmae when SA Query times out (mainly, after a
	 * channel switch when OCV is enabled).
	 * 0 - Default behavior, 1 - Ignore SA Query timeout.
	 * This attribute is used to configure the testbed device.
	 * This attribute can be configured only when STA is in associated state
	 * and the configuration is valid until the disconnection.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_IGNORE_SA_QUERY_TIMEOUT = 43,

	/* 8-bit unsigned value to configure the driver/firmware to start or
	 * stop transmitting FILS discovery frames.
	 * 0 - Stop transmitting FILS discovery frames
	 * 1 - Start transmitting FILS discovery frames
	 * This attribute is used to configure the testbed device.
	 * This attribute can be configured only in AP mode and the
	 * configuration is valid until AP restart.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_FILS_DISCOVERY_FRAMES_TX = 44,

	/* 8-bit unsigned value to configure the driver/firmware to enable or
	 * disable full bandwidth UL MU-MIMO subfield in the HE PHY capabilities
	 * information field.
	 * 0 - Disable full bandwidth UL MU-MIMO subfield
	 * 1 - Enable full bandwidth UL MU-MIMO subfield
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_FULL_BW_UL_MU_MIMO = 45,

	/* 16-bit unsigned value to configure the driver with a specific BSS
	 * max idle period to advertise in the BSS Max Idle Period element
	 * (IEEE Std 802.11-2016, 9.4.2.79) in (Re)Association Request frames.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_BSS_MAX_IDLE_PERIOD = 46,

	/* 8-bit unsigned value to configure the driver to use only RU 242 tone
	 * for data transmission.
	 * 0 - Default behavior, 1 - Configure RU 242 tone for data Tx.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_RU_242_TONE_TX = 47,

	/* 8-bit unsigned value to configure the driver to disable data and
	 * management response frame transmission to test the BSS max idle
	 * feature.
	 * 0 - Default behavior, 1 - Disable data and management response Tx.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_DISABLE_DATA_MGMT_RSP_TX = 48,

	/* 8-bit unsigned value to configure the driver/firmware to enable or
	 * disable Punctured Preamble Rx subfield in the HE PHY capabilities
	 * information field.
	 * 0 - Disable Punctured Preamble Rx subfield
	 * 1 - Enable Punctured Preamble Rx subfield
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_PUNCTURED_PREAMBLE_RX = 49,

	/* 8-bit unsigned value to configure the driver to ignore the SAE H2E
	 * requirement mismatch for 6 GHz connection.
	 * 0 - Default behavior, 1 - Ignore SAE H2E requirement mismatch.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_IGNORE_H2E_RSNXE = 50,

	/* 8-bit unsigned value to configure the driver to allow 6 GHz
	 * connection with all security modes.
	 * 0 - Default behavior, 1 - Allow 6 GHz connection with all security
	 * modes.
	 * This attribute is used for testing purposes.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_6GHZ_SECURITY_TEST_MODE = 51,

	/* 8-bit unsigned value to configure the driver to transmit data with
	 * ER SU PPDU type.
	 *
	 * 0 - Default behavior, 1 - Enable ER SU PPDU type TX.
	 * This attribute is used for testing purposes.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_ER_SU_PPDU_TYPE = 52,

	/* 8-bit unsigned value to configure the driver to use Data or
	 * Management frame type for keep alive data.
	 * Uses enum qca_wlan_keep_alive_data_type values.
	 *
	 * This attribute is used for testing purposes.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_KEEP_ALIVE_FRAME_TYPE = 53,

	/* 8-bit unsigned value to configure the driver to use scan request
	 * BSSID value in Probe Request frame RA(A1) during the scan. The
	 * driver saves this configuration and applies this setting to all user
	 * space scan requests until the setting is cleared. If this
	 * configuration is set, the driver uses the BSSID value from the scan
	 * request to set the RA(A1) in the Probe Request frames during the
	 * scan.
	 *
	 * 0 - Default behavior uses the broadcast RA in Probe Request frames.
	 * 1 - Uses the scan request BSSID in RA in Probe Request frames.
	 * This attribute is used for testing purposes.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_USE_BSSID_IN_PROBE_REQ_RA = 54,

	/* 8-bit unsigned value to configure the driver to enable/disable the
	 * BSS max idle period support.
	 *
	 * 0 - Disable the BSS max idle support.
	 * 1 - Enable the BSS max idle support.
	 * This attribute is used for testing purposes.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_BSS_MAX_IDLE_PERIOD_ENABLE = 55,

	/* 8-bit unsigned value to configure the driver/firmware to enable or
	 * disable Rx control frame to MultiBSS subfield in the HE MAC
	 * capabilities information field.
	 * 0 - Disable Rx control frame to MultiBSS subfield
	 * 1 - Enable Rx control frame to MultiBSS subfield
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_RX_CTRL_FRAME_TO_MBSS = 56,

	/* 8-bit unsigned value to configure the driver/firmware to enable or
	 * disable Broadcast TWT support subfield in the HE MAC capabilities
	 * information field.
	 * 0 - Disable Broadcast TWT support subfield
	 * 1 - Enable Broadcast TWT support subfield
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_BCAST_TWT_SUPPORT = 57,

	/* 8-bit unsigned value to configure the driver/firmware to allow eMLSR
	 * mode for IEEE 802.11be MLO capable devices. If the attribute is set
	 * to 1, and if the firmware supports this capability too, the STA
	 * advertises this capability to the AP over Association Request frame.
	 * This attribute will not have any effect on legacy devices with no
	 * IEEE 802.11be support.
	 * 0 - Default behavior
	 * 1 - Enable eMLSR (Enhanced Multi-link Single-Radio) mode
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_11BE_EMLSR_MODE = 58,

	/* 8-bit unsigned value to configure the driver to enable/disable the
	 * periodic sounding for Tx beamformer functionality. The default
	 * behavior uses algorithm to do sounding based on packet stats.
	 *
	 * 0 - Default behavior.
	 * 1 - Enable the periodic sounding for Tx beamformer.
	 * This attribute is used for testing purposes.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_BEAMFORMER_PERIODIC_SOUNDING = 59,

	/* 8-bit unsigned value to configure beamformee SS EHT capability
	 * to indicate the maximum number of spatial streams that the STA
	 * can receive in an EHT sounding NDP for <= 80 MHz bandwidth.
	 * The range of the value is 3 to 7.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_EHT_BEAMFORMEE_SS_80MHZ = 60,

	/* 8-bit unsigned value to configure beamformee SS EHT capability
	 * to indicate the maximum number of spatial streams that the STA
	 * can receive in an EHT sounding NDP for 160 MHz bandwidth.
	 * The range of the value is 3 to 7.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_EHT_BEAMFORMEE_SS_160MHZ = 61,

	/* 8-bit unsigned value to configure beamformee SS EHT capability
	 * to indicate the maximum number of spatial streams that the STA
	 * can receive in an EHT sounding NDP for 320 MHz bandwidth.
	 * The range of the value is 3 to 7.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_EHT_BEAMFORMEE_SS_320MHZ = 62,

	/* 8-bit unsigned value to configure the driver to exclude station
	 * profile in Probe Request frame Multi-Link element.
	 * 0 - Default behavior, sends the Probe Request frame with station
	 * profile data included in the Multi-Link element.
	 * 1 - Exclude station profile in Probe Request frame Multi-Link
	 * element.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_EXCLUDE_STA_PROF_IN_PROBE_REQ = 63,

	/* 8-bit unsigned value to configure EHT testbed defaults.
	 * This attribute is used to configure the testbed device.
	 * 1 - Set the device EHT capabilities to testbed defaults.
	 * 0 - Reset the device EHT capabilities to supported config.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_SET_EHT_TESTBED_DEFAULTS = 64,

	/* 8-bit unsigned value to indicate the EHT MCS support.
	 * Uses enum eht_mcs_config values.
	 * This attribute is used to configure the testbed device to
	 * allow the advertised hardware capabilities to be downgraded
	 * for testing purposes.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_EHT_MCS = 65,

	/* 8-bit unsigned value to configure EHT TB Sounding Feedback
	 * Rate Limit capability.
	 * This attribute is used to configure the testbed device.
	 * 0 - Indicates no maximum supported data rate limitation.
	 * 1 - Indicates the maximum supported data rate is the lower of
	 * the 1500 Mb/s and the maximum supported data rate.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_EHT_TB_SOUNDING_FB_RL = 66,

	/* 8-bit unsigned value to configure the support for receiving an MPDU
	 * that contains an EHT operating mode control subfield.
	 * This attribute is used to configure the testbed device.
	 * 1-enable, 0-disable.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_EHT_OM_CTRL_SUPPORT = 67,

	/* 8-bit unsigned value to configure the driver with EMLSR padding delay
	 * subfield value.
	 *
	 * 0 - 0 us
	 * 1 - 32 us
	 * 2 - 64 us
	 * 3 - 128 us
	 * 4 - 256 us
	 * 5-255 - Reserved
	 *
	 * This attribute is used for testing purposes.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_EMLSR_PADDING_DELAY = 68,

	/*
	 * 8-bit unsigned value to indicate the firmware to force the active MLO
	 * links to power save mode for the configured number of beacon periods.
	 * This allows the firmware to suspend STA links for X beacon periods
	 * and remain asleep even if the AP advertises TIM as opposed to regular
	 * power save mode where STA links wake up if the AP indicates that it
	 * has buffered data to send.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_FORCE_MLO_POWER_SAVE_BCN_PERIOD = 69,

	/*
	 * 8-bit unsigned value to indicate the firmware to be in STR MLMR mode
	 * to enable simultaneous transmission of PPDUs on all active links.
	 * 0 - Default behavior
	 * 1 - Enter STR mode for simultaneous data transmission on all links
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_EHT_MLO_STR_TX = 70,

	/* Nested attribute to indicate EHT MLO links on which powersave to be
	 * enabled. It contains link ID attributes. These nested attributes are
	 * of the type u8 and are used to enable the powersave on associated
	 * MLO links corresponding to the link IDs provided in the command.
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_EHT_MLO_LINK_POWER_SAVE = 71,

	/* 8-bit unsigned value to configure the MLD ID of the BSS whose link
	 * info is requested in the ML Probe Request frame. In the MLO-MBSSID
	 * testcase, STA can request information of non-Tx BSS through Tx BSS
	 * by configuring non-Tx BSS MLD ID within the ML probe request that
	 * is transmitted via host initiated scan request.
	 *
	 * This attribute is used for testing purposes.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_MLD_ID_ML_PROBE_REQ = 72,

	/* 8-bit unsigned value to configure the SCS traffic description
	 * support in the EHT capabilities of an Association Request frame.
	 * 1-enable, 0-disable
	 * This attribute is used to configure the testbed device.
	 */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_EHT_SCS_TRAFFIC_SUPPORT = 73,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_MAX =
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_twt_operation - Operation of the config TWT request
 * Values for %QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION.
 * The response for the respective operations can be either synchronous or
 * asynchronous (wherever specified). If synchronous, the response to this
 * operation is obtained in the corresponding vendor command reply to the user
 * space. For the asynchronous case the response is obtained as an event with
 * the same operation type.
 *
 * Drivers shall support either of these modes but not both simultaneously.
 * This support for asynchronous mode is advertised through the flag
 * QCA_WLAN_VENDOR_FEATURE_TWT_ASYNC_SUPPORT. If this flag is not advertised,
 * the driver shall support synchronous mode.
 *
 * @QCA_WLAN_TWT_SET: Setup a TWT session. Required parameters are configured
 * through QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS. Refers the enum
 * qca_wlan_vendor_attr_twt_setup. Depending upon the
 * @QCA_WLAN_VENDOR_FEATURE_TWT_ASYNC_SUPPORT capability, this is either a
 * synchronous or asynchronous operation.
 *
 * @QCA_WLAN_TWT_GET: Get the configured TWT parameters. Required parameters are
 * obtained through QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS. Refers the enum
 * qca_wlan_vendor_attr_twt_setup. This is a synchronous operation.
 *
 * @QCA_WLAN_TWT_TERMINATE: Terminate the TWT session. Required parameters are
 * obtained through QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS. Refers the enum
 * qca_wlan_vendor_attr_twt_setup. Valid only after the TWT session is setup.
 * This terminate can either get triggered by the user space or can as well be
 * a notification from the firmware if it initiates a terminate.
 * Depending upon the @QCA_WLAN_VENDOR_FEATURE_TWT_ASYNC_SUPPORT capability,
 * the request from user space can either be a synchronous or asynchronous
 * operation.
 *
 * @QCA_WLAN_TWT_SUSPEND: Suspend the TWT session. Required parameters are
 * obtained through QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS. Refers the enum
 * qca_wlan_vendor_attr_twt_setup. Valid only after the TWT session is setup.
 * Depending upon the @QCA_WLAN_VENDOR_FEATURE_TWT_ASYNC_SUPPORT capability,
 * this is either a synchronous or asynchronous operation.
 *
 * @QCA_WLAN_TWT_RESUME: Resume the TWT session. Required parameters are
 * configured through QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS. Refers the enum
 * qca_wlan_vendor_attr_twt_resume. Valid only after the TWT session is setup.
 * This can as well be a notification from the firmware on a QCA_WLAN_TWT_NUDGE
 * request. Depending upon the @QCA_WLAN_VENDOR_FEATURE_TWT_ASYNC_SUPPORT
 * capability, this is either a synchronous or asynchronous operation.
 *
 * @QCA_WLAN_TWT_NUDGE: Suspend and resume the TWT session. TWT nudge is a
 * combination of suspend and resume in a single request. Required parameters
 * are configured through QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS. Refers the
 * enum qca_wlan_vendor_attr_twt_nudge. Valid only after the TWT session is
 * setup. Depending upon the @QCA_WLAN_VENDOR_FEATURE_TWT_ASYNC_SUPPORT
 * capability, this is either a synchronous or asynchronous operation.
 *
 * @QCA_WLAN_TWT_GET_STATS: Get the TWT session traffic statistics information.
 * Refers the enum qca_wlan_vendor_attr_twt_stats. Valid only after the TWT
 * session is setup. It's a synchronous operation.
 *
 * @QCA_WLAN_TWT_CLEAR_STATS: Clear TWT session traffic statistics information.
 * Valid only after the TWT session is setup. It's a synchronous operation.
 *
 * @QCA_WLAN_TWT_GET_CAPABILITIES: Get TWT capabilities of this device and its
 * peer. Refers the enum qca_wlan_vendor_attr_twt_capability. It's a synchronous
 * operation.
 *
 * @QCA_WLAN_TWT_SETUP_READY_NOTIFY: Notify userspace that the firmware is
 * ready for a new TWT session setup after it issued a TWT teardown.
 *
 * @QCA_WLAN_TWT_SET_PARAM: Configure TWT related parameters. Required
 * parameters are obtained through QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS. Refer
 * the enum qca_wlan_vendor_attr_twt_set_param.
 *
 * @QCA_WLAN_TWT_NOTIFY: Used to notify userspace about changes in TWT
 * related information for example TWT required bit in AP capabilities etc.
 * The reason for the notification is sent using
 * QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_NOTIFY_STATUS.
 */
enum qca_wlan_twt_operation {
	QCA_WLAN_TWT_SET = 0,
	QCA_WLAN_TWT_GET = 1,
	QCA_WLAN_TWT_TERMINATE = 2,
	QCA_WLAN_TWT_SUSPEND = 3,
	QCA_WLAN_TWT_RESUME = 4,
	QCA_WLAN_TWT_NUDGE = 5,
	QCA_WLAN_TWT_GET_STATS = 6,
	QCA_WLAN_TWT_CLEAR_STATS = 7,
	QCA_WLAN_TWT_GET_CAPABILITIES = 8,
	QCA_WLAN_TWT_SETUP_READY_NOTIFY = 9,
	QCA_WLAN_TWT_SET_PARAM = 10,
	QCA_WLAN_TWT_NOTIFY = 11,
};

/**
 * enum qca_wlan_vendor_attr_config_twt: Defines attributes used by
 * %QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION: u8 attribute. Specify the TWT
 * operation of this request. Possible values are defined in enum
 * qca_wlan_twt_operation. The parameters for the respective operation is
 * specified through QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS: Nested attribute representing the
 * parameters configured for TWT. These parameters are represented by
 * enum qca_wlan_vendor_attr_twt_setup, enum qca_wlan_vendor_attr_twt_resume,
 * enum qca_wlan_vendor_attr_twt_set_param, or
 * enum qca_wlan_vendor_attr_twt_stats based on the operation.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_NOTIFY_STATUS: Size is u8, mandatory when
 * QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION is set to QCA_WLAN_TWT_NOTIFY.
 * The values used by this attribute are defined in
 * enum qca_wlan_vendor_twt_status.
 */
enum qca_wlan_vendor_attr_config_twt {
	QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION = 1,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS = 2,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_NOTIFY_STATUS = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_MAX =
	QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_bss_filter - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_BSS_FILTER.
 * The user can add/delete the filter by specifying the BSSID/STA MAC address in
 * QCA_WLAN_VENDOR_ATTR_BSS_FILTER_MAC_ADDR, filter type in
 * QCA_WLAN_VENDOR_ATTR_BSS_FILTER_TYPE, add/delete action in
 * QCA_WLAN_VENDOR_ATTR_BSS_FILTER_ACTION in the request. The user can get the
 * statistics of an unassociated station by specifying the MAC address in
 * QCA_WLAN_VENDOR_ATTR_BSS_FILTER_MAC_ADDR, station type in
 * QCA_WLAN_VENDOR_ATTR_BSS_FILTER_TYPE, GET action in
 * QCA_WLAN_VENDOR_ATTR_BSS_FILTER_ACTION in the request. The user also can get
 * the statistics of all unassociated stations by specifying the Broadcast MAC
 * address (ff:ff:ff:ff:ff:ff) in QCA_WLAN_VENDOR_ATTR_BSS_FILTER_MAC_ADDR with
 * above procedure. In the response, driver shall specify statistics
 * information nested in QCA_WLAN_VENDOR_ATTR_BSS_FILTER_STA_STATS.
 */
enum qca_wlan_vendor_attr_bss_filter {
	QCA_WLAN_VENDOR_ATTR_BSS_FILTER_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_BSS_FILTER_MAC_ADDR = 1,
	/* Other BSS filter type, unsigned 8 bit value. One of the values
	 * in enum qca_wlan_vendor_bss_filter_type.
	 */
	QCA_WLAN_VENDOR_ATTR_BSS_FILTER_TYPE = 2,
	/* Other BSS filter action, unsigned 8 bit value. One of the values
	 * in enum qca_wlan_vendor_bss_filter_action.
	 */
	QCA_WLAN_VENDOR_ATTR_BSS_FILTER_ACTION = 3,
	/* Array of nested attributes where each entry is the statistics
	 * information of the specified station that belong to another BSS.
	 * Attributes for each entry are taken from enum
	 * qca_wlan_vendor_bss_filter_sta_stats.
	 * Other BSS station configured in
	 * QCA_NL80211_VENDOR_SUBCMD_BSS_FILTER with filter type
	 * QCA_WLAN_VENDOR_BSS_FILTER_TYPE_STA.
	 * Statistics returned by QCA_NL80211_VENDOR_SUBCMD_BSS_FILTER
	 * with filter action QCA_WLAN_VENDOR_BSS_FILTER_ACTION_GET.
	 */
	QCA_WLAN_VENDOR_ATTR_BSS_FILTER_STA_STATS = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_BSS_FILTER_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_BSS_FILTER_MAX =
	QCA_WLAN_VENDOR_ATTR_BSS_FILTER_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_bss_filter_type - Type of
 * filter used in other BSS filter operations. Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_BSS_FILTER.
 *
 * @QCA_WLAN_VENDOR_BSS_FILTER_TYPE_BSSID: BSSID filter
 * @QCA_WLAN_VENDOR_BSS_FILTER_TYPE_STA: Station MAC address filter
 */
enum qca_wlan_vendor_bss_filter_type {
	QCA_WLAN_VENDOR_BSS_FILTER_TYPE_BSSID,
	QCA_WLAN_VENDOR_BSS_FILTER_TYPE_STA,
};

/**
 * enum qca_wlan_vendor_bss_filter_action - Type of
 * action in other BSS filter operations. Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_BSS_FILTER.
 *
 * @QCA_WLAN_VENDOR_BSS_FILTER_ACTION_ADD: Add filter
 * @QCA_WLAN_VENDOR_BSS_FILTER_ACTION_DEL: Delete filter
 * @QCA_WLAN_VENDOR_BSS_FILTER_ACTION_GET: Get the statistics
 */
enum qca_wlan_vendor_bss_filter_action {
	QCA_WLAN_VENDOR_BSS_FILTER_ACTION_ADD,
	QCA_WLAN_VENDOR_BSS_FILTER_ACTION_DEL,
	QCA_WLAN_VENDOR_BSS_FILTER_ACTION_GET,
};

/**
 * enum qca_wlan_vendor_bss_filter_sta_stats - Attributes for
 * the statistics of a specific unassociated station belonging to another BSS.
 * The statistics provides information of the unassociated station
 * filtered by other BSS operation - such as MAC, signal value.
 * Used by the vendor command QCA_NL80211_VENDOR_SUBCMD_BSS_FILTER.
 *
 * @QCA_WLAN_VENDOR_BSS_FILTER_STA_STATS_MAC: MAC address of the station.
 * @QCA_WLAN_VENDOR_BSS_FILTER_STA_STATS_RSSI: Last received signal strength
 *	of the station. Unsigned 8 bit number containing RSSI.
 * @QCA_WLAN_VENDOR_BSS_FILTER_STA_STATS_RSSI_TS: Time stamp of the host
 *	driver for the last received RSSI. Unsigned 64 bit number containing
 *	nanoseconds from the boottime.
 * @QCA_WLAN_VENDOR_BSS_FILTER_STA_STATS_PAD: Attribute used for padding for
 *	64-bit alignment.
 */
enum qca_wlan_vendor_bss_filter_sta_stats {
	QCA_WLAN_VENDOR_BSS_FILTER_STA_STATS_INVALID = 0,
	QCA_WLAN_VENDOR_BSS_FILTER_STA_STATS_MAC = 1,
	QCA_WLAN_VENDOR_BSS_FILTER_STA_STATS_RSSI = 2,
	QCA_WLAN_VENDOR_BSS_FILTER_STA_STATS_RSSI_TS = 3,
	QCA_WLAN_VENDOR_BSS_FILTER_STA_STATS_PAD = 4,

	/* keep last */
	QCA_WLAN_VENDOR_BSS_FILTER_STA_STATS_AFTER_LAST,
	QCA_WLAN_VENDOR_BSS_FILTER_STA_STATS_MAX =
	QCA_WLAN_VENDOR_BSS_FILTER_STA_STATS_AFTER_LAST - 1
};

/* enum qca_wlan_nan_subcmd_type - Type of NAN command used by attribute
 * QCA_WLAN_VENDOR_ATTR_NAN_SUBCMD_TYPE as a part of vendor command
 * QCA_NL80211_VENDOR_SUBCMD_NAN_EXT.
 */
enum qca_wlan_nan_ext_subcmd_type {
	/* Subcmd of type NAN Enable Request */
	QCA_WLAN_NAN_EXT_SUBCMD_TYPE_ENABLE_REQ = 1,
	/* Subcmd of type NAN Disable Request */
	QCA_WLAN_NAN_EXT_SUBCMD_TYPE_DISABLE_REQ = 2,
};

/**
 * enum qca_wlan_vendor_attr_nan_params - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_NAN_EXT.
 */
enum qca_wlan_vendor_attr_nan_params {
	QCA_WLAN_VENDOR_ATTR_NAN_INVALID = 0,
	/* Carries NAN command for firmware component. Every vendor command
	 * QCA_NL80211_VENDOR_SUBCMD_NAN_EXT must contain this attribute with a
	 * payload containing the NAN command. NLA_BINARY attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_NAN_CMD_DATA = 1,
	/* Indicates the type of NAN command sent with
	 * QCA_NL80211_VENDOR_SUBCMD_NAN_EXT. enum qca_wlan_nan_ext_subcmd_type
	 * describes the possible range of values. This attribute is mandatory
	 * if the command being issued is either
	 * QCA_WLAN_NAN_EXT_SUBCMD_TYPE_ENABLE_REQ or
	 * QCA_WLAN_NAN_EXT_SUBCMD_TYPE_DISABLE_REQ. NLA_U32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_NAN_SUBCMD_TYPE = 2,
	/* Frequency (in MHz) of primary NAN discovery social channel in 2.4 GHz
	 * band. This attribute is mandatory when command type is
	 * QCA_WLAN_NAN_EXT_SUBCMD_TYPE_ENABLE_REQ. NLA_U32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_NAN_DISC_24GHZ_BAND_FREQ = 3,
	/* Frequency (in MHz) of secondary NAN discovery social channel in 5 GHz
	 * band. This attribute is optional and should be included when command
	 * type is QCA_WLAN_NAN_EXT_SUBCMD_TYPE_ENABLE_REQ and NAN discovery
	 * has to be started on 5GHz along with 2.4GHz. NLA_U32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_NAN_DISC_5GHZ_BAND_FREQ = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_NAN_PARAMS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_NAN_PARAMS_MAX =
		QCA_WLAN_VENDOR_ATTR_NAN_PARAMS_AFTER_LAST - 1
};

/**
 * qca_wlan_twt_setup_state: Represents the TWT session states.
 *
 * QCA_WLAN_TWT_SETUP_STATE_NOT_ESTABLISHED: TWT session not established.
 * QCA_WLAN_TWT_SETUP_STATE_ACTIVE: TWT session is active.
 * QCA_WLAN_TWT_SETUP_STATE_SUSPEND: TWT session is in suspended state.
 */
enum qca_wlan_twt_setup_state {
	QCA_WLAN_TWT_SETUP_STATE_NOT_ESTABLISHED = 0,
	QCA_WLAN_TWT_SETUP_STATE_ACTIVE = 1,
	QCA_WLAN_TWT_SETUP_STATE_SUSPEND = 2,
};

/**
 * enum qca_wlan_vendor_attr_twt_setup: Represents attributes for
 * TWT (Target Wake Time) setup request. These attributes are sent as part of
 * %QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_TWT_SETUP and
 * %QCA_NL80211_VENDOR_SUBCMD_WIFI_TEST_CONFIGURATION. Also used by
 * attributes through %QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST: Flag attribute.
 * Disable (flag attribute not present) - Individual TWT
 * Enable (flag attribute present) - Broadcast TWT.
 * Individual means the session is between the STA and the AP.
 * This session is established using a separate negotiation between
 * STA and AP.
 * Broadcast means the session is across multiple STAs and an AP. The
 * configuration parameters are announced in Beacon frames by the AP.
 * This is used in
 * 1. TWT SET Request and Response
 * 2. TWT GET Response
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_REQ_TYPE: Required (u8).
 * Unsigned 8-bit qca_wlan_vendor_twt_setup_req_type to
 * specify the TWT request type. This is used in TWT SET operation.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_TRIGGER: Flag attribute
 * Enable (flag attribute present) - TWT with trigger support.
 * Disable (flag attribute not present) - TWT without trigger support.
 * Trigger means the AP will send the trigger frame to allow STA to send data.
 * Without trigger, the STA will wait for the MU EDCA timer before
 * transmitting the data.
 * This is used in
 * 1. TWT SET Request and Response
 * 2. TWT GET Response
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_TYPE: Required (u8)
 * 0 - Announced TWT - In this mode, STA may skip few service periods to
 * save more power. If STA wants to wake up, it will send a PS-POLL/QoS
 * NULL frame to AP.
 * 1 - Unannounced TWT - The STA will wakeup during every SP.
 * This is a required parameter for
 * 1. TWT SET Request and Response
 * 2. TWT GET Response
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID: Optional (u8)
 * Flow ID is the unique identifier for each TWT session.
 * If not provided then dialog ID will be set to zero.
 * This is an optional parameter for
 * 1. TWT SET Request and Response
 * 2. TWT GET Request and Response
 * 3. TWT TERMINATE Request and Response
 * 4. TWT SUSPEND Request and Response
 * Flow ID values from 0 to 254 represent a single TWT session
 * Flow ID value of 255 represents all TWT sessions for the following
 * 1. TWT TERMINATE Request and Response
 * 2. TWT SUSPEND Request and Response
 * 4. TWT CLEAR STATISTICS request
 * 5. TWT GET STATISTICS request and response
 * If an invalid dialog ID is provided, status
 * QCA_WLAN_VENDOR_TWT_STATUS_SESSION_NOT_EXIST will be returned.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_EXP: Required (u8)
 * This attribute (exp) is used along with the mantissa to derive the
 * wake interval using the following formula:
 * pow(2,exp) = wake_intvl_us/wake_intvl_mantis
 * Wake interval is the interval between 2 successive SP.
 * This is a required parameter for
 * 1. TWT SET Request and Response
 * 2. TWT GET Response
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_PROTECTION: Flag attribute
 * Enable (flag attribute present) - Protection required.
 * Disable (flag attribute not present) - Protection not required.
 * If protection is enabled, then the AP will use protection
 * mechanism using RTS/CTS to self to reserve the airtime.
 * This is used in
 * 1. TWT SET Request and Response
 * 2. TWT GET Response
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_TIME: Optional (u32)
 * This attribute is used as the SP offset which is the offset from
 * TSF after which the wake happens. The units are in microseconds. If
 * this attribute is not provided, then the value will be set to zero.
 * This is an optional parameter for
 * 1. TWT SET Request and Response
 * 2. TWT GET Response
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_DURATION: Required (u32)
 * This is the duration of the service period. This is specified as
 * multiples of 256 microseconds. Valid values are 0x1 to 0xFF.
 * This is a required parameter for
 * 1. TWT SET Request and Response
 * 2. TWT GET Response
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_MANTISSA: Required (u32)
 * This attribute is used to configure wake interval mantissa.
 * The units are in TU.
 * This is a required parameter for
 * 1. TWT SET Request and Response
 * 2. TWT GET Response
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_STATUS: Required (u8)
 * This field is applicable for TWT response only.
 * This contains status values in enum qca_wlan_vendor_twt_status
 * and is passed to the userspace. This is used in TWT SET operation.
 * This is a required parameter for
 * 1. TWT SET Response
 * 2. TWT TERMINATE Response
 * 3. TWT SUSPEND Response
 * 4. TWT RESUME Response
 * 5. TWT NUDGE Response
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_RESP_TYPE: Required (u8)
 * This field is applicable for TWT response only.
 * This field contains response type from the TWT responder and is
 * passed to the userspace. The values for this field are defined in
 * enum qca_wlan_vendor_twt_setup_resp_type. This is used in TWT SET
 * response.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_TIME_TSF: Required (u64)
 * In TWT setup command this field contains absolute TSF that will
 * be used by TWT requester during setup.
 * In TWT response this field contains absolute TSF value of the
 * wake time received from the TWT responder and is passed to
 * the userspace.
 * This is an optional parameter for
 * 1. TWT SET Request
 * This is a required parameter for
 * 1. TWT SET Response
 * 2. TWT GET Response
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_TWT_INFO_ENABLED: Flag attribute.
 * Enable (flag attribute present) - Indicates that the TWT responder
 * supports reception of TWT information frame from the TWT requestor.
 * Disable (flag attribute not present) - Indicates that the responder
 * doesn't support reception of TWT information frame from requestor.
 * This is used in
 * 1. TWT SET Response
 * 2. TWT GET Response
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAC_ADDR: 6-byte MAC address
 * Represents the MAC address of the peer for which the TWT session
 * is being configured. This is used in AP mode to represent the respective
 * client.
 * In AP mode, this is a required parameter in response for
 * 1. TWT SET
 * 2. TWT GET
 * 3. TWT TERMINATE
 * 4. TWT SUSPEND
 * In STA mode, this is an optional parameter in request and response for
 * the above four TWT operations.
 * In AP mode, this is a required parameter in request for
 * 1. TWT GET
 * 2. TWT TERMINATE
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MIN_WAKE_INTVL: Optional (u32)
 * Minimum tolerance limit of wake interval parameter in microseconds.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX_WAKE_INTVL: Optional (u32)
 * Maximum tolerance limit of wake interval parameter in microseconds.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MIN_WAKE_DURATION: Optional (u32)
 * Minimum tolerance limit of wake duration parameter in microseconds.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX_WAKE_DURATION: Optional (u32)
 * Maximum tolerance limit of wake duration parameter in microseconds.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_STATE: Optional (u32)
 * TWT state for the given dialog id. The values for this are represented
 * by enum qca_wlan_twt_setup_state.
 * This is obtained through TWT GET operation.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL2_MANTISSA: Optional (u32)
 * This attribute is used to configure wake interval mantissa.
 * The unit is microseconds. This attribute, when specified, takes
 * precedence over QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_MANTISSA.
 * This parameter is used for
 * 1. TWT SET Request and Response
 * 2. TWT GET Response
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST_ID: Optional (u8)
 * This attribute is used to configure Broadcast TWT ID.
 * The Broadcast TWT ID indicates a specific Broadcast TWT for which the
 * transmitting STA is providing TWT parameters. The allowed values are 0 to 31.
 * This parameter is used for
 * 1. TWT SET Request
 * 2. TWT TERMINATE Request
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST_RECOMMENDATION: Optional (u8)
 * This attribute is used to configure Broadcast TWT recommendation.
 * The Broadcast TWT Recommendation subfield contains a value that indicates
 * recommendations on the types of frames that are transmitted by TWT
 * scheduled STAs and scheduling AP during the broadcast TWT SP.
 * The allowed values are 0 - 3.
 * This parameter is used for
 * 1. TWT SET Request
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST_PERSISTENCE: Optional (u8)
 * This attribute is used to configure Broadcast TWT Persistence.
 * The Broadcast TWT Persistence subfield indicates the number of
 * TBTTs during which the Broadcast TWT SPs corresponding to this
 * broadcast TWT Parameter set are present. The number of beacon intervals
 * during which the Broadcast TWT SPs are present is equal to the value in the
 * Broadcast TWT Persistence subfield plus 1 except that the value 255
 * indicates that the Broadcast TWT SPs are present until explicitly terminated.
 * This parameter is used for
 * 1. TWT SET Request
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_RESPONDER_PM_MODE: Optional (u8)
 * This attribute contains the value of the Responder PM Mode subfield (0 or 1)
 * from TWT response frame. During TWT setup request, this attribute is used to
 * configure the Responder PM Mode bit in the control field of the TWT element
 * for broadcast TWT schedule.
 * This parameter is used for
 * 1. TWT SET Response
 * 2. TWT GET Response
 * 3. TWT SET Request
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_ANNOUNCE_TIMEOUT: Optional (u32)
 * This attribute is used to configure the announce timeout value (in us) in
 * the firmware. This timeout value is only applicable for the announced TWT. If
 * the timeout value is non-zero the firmware waits up to the timeout value to
 * use Data frame as an announcement frame. If the timeout value is 0 the
 * firmware sends an explicit QoS NULL frame as the announcement frame on SP
 * start. The default value in the firmware is 0.
 * This parameter is used for
 * 1. TWT SET Request
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SETUP_PAD: Attribute used for padding for 64-bit
 * alignment.
 */
enum qca_wlan_vendor_attr_twt_setup {
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST = 1,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_REQ_TYPE = 2,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_TRIGGER = 3,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_TYPE = 4,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID = 5,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_EXP = 6,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_PROTECTION = 7,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_TIME = 8,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_DURATION = 9,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_MANTISSA = 10,

	/* TWT Response only attributes */
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_STATUS = 11,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_RESP_TYPE = 12,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_TIME_TSF = 13,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_TWT_INFO_ENABLED = 14,

	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAC_ADDR = 15,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MIN_WAKE_INTVL = 16,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX_WAKE_INTVL = 17,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MIN_WAKE_DURATION = 18,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX_WAKE_DURATION = 19,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_STATE = 20,

	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL2_MANTISSA = 21,

	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST_ID = 22,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST_RECOMMENDATION = 23,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST_PERSISTENCE = 24,

	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_RESPONDER_PM_MODE = 25,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_ANNOUNCE_TIMEOUT = 26,

	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_PAD = 27,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX =
	QCA_WLAN_VENDOR_ATTR_TWT_SETUP_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_twt_status - Represents the status of the requested
 * TWT operation
 *
 * @QCA_WLAN_VENDOR_TWT_STATUS_OK: TWT request successfully completed
 * @QCA_WLAN_VENDOR_TWT_STATUS_TWT_NOT_ENABLED: TWT not enabled
 * @QCA_WLAN_VENDOR_TWT_STATUS_USED_DIALOG_ID: TWT dialog ID is already used
 * @QCA_WLAN_VENDOR_TWT_STATUS_SESSION_BUSY: TWT session is busy
 * @QCA_WLAN_VENDOR_TWT_STATUS_SESSION_NOT_EXIST: TWT session does not exist
 * @QCA_WLAN_VENDOR_TWT_STATUS_NOT_SUSPENDED: TWT session not in suspend state
 * @QCA_WLAN_VENDOR_TWT_STATUS_INVALID_PARAM: Invalid parameters
 * @QCA_WLAN_VENDOR_TWT_STATUS_NOT_READY: FW not ready
 * @QCA_WLAN_VENDOR_TWT_STATUS_NO_RESOURCE: FW resource exhausted
 * @QCA_WLAN_VENDOR_TWT_STATUS_NO_ACK: Peer AP/STA did not ACK the
 * request/response frame
 * @QCA_WLAN_VENDOR_TWT_STATUS_NO_RESPONSE: Peer AP did not send the response
 * frame
 * @QCA_WLAN_VENDOR_TWT_STATUS_DENIED: AP did not accept the request
 * @QCA_WLAN_VENDOR_TWT_STATUS_UNKNOWN_ERROR: Adding TWT dialog failed due to an
 * unknown reason
 * @QCA_WLAN_VENDOR_TWT_STATUS_ALREADY_SUSPENDED: TWT session already in
 * suspend state
 * @QCA_WLAN_VENDOR_TWT_STATUS_IE_INVALID: FW has dropped the frame due to
 * invalid IE in the received TWT frame
 * @QCA_WLAN_VENDOR_TWT_STATUS_PARAMS_NOT_IN_RANGE: Parameters received from
 * the responder are not in the specified range
 * @QCA_WLAN_VENDOR_TWT_STATUS_PEER_INITIATED_TERMINATE: FW terminated the TWT
 * session due to request from the responder. Used on the TWT_TERMINATE
 * notification from the firmware.
 * @QCA_WLAN_VENDOR_TWT_STATUS_ROAM_INITIATED_TERMINATE: FW terminated the TWT
 * session due to roaming. Used on the TWT_TERMINATE notification from the
 * firmware.
 * @QCA_WLAN_VENDOR_TWT_STATUS_SCC_MCC_CONCURRENCY_TERMINATE: FW terminated the
 * TWT session due to SCC (Single Channel Concurrency) and MCC (Multi Channel
 * Concurrency). Used on the TWT_TERMINATE notification from the firmware.
 * @QCA_WLAN_VENDOR_TWT_STATUS_ROAMING_IN_PROGRESS: FW rejected the TWT setup
 * request due to roaming in progress.
 * @QCA_WLAN_VENDOR_TWT_STATUS_CHANNEL_SWITCH_IN_PROGRESS: FW rejected the TWT
 * setup request due to channel switch in progress.
 * @QCA_WLAN_VENDOR_TWT_STATUS_SCAN_IN_PROGRESS: FW rejected the TWT setup
 * request due to scan in progress.
 * QCA_WLAN_VENDOR_TWT_STATUS_POWER_SAVE_EXIT_TERMINATE: The driver requested to
 * terminate an existing TWT session on power save exit request from userspace.
 * Used on the TWT_TERMINATE notification from the driver/firmware.
 * @QCA_WLAN_VENDOR_TWT_STATUS_TWT_REQUIRED: The peer has set the TWT
 * required bit in its capabilities.
 * @QCA_WLAN_VENDOR_TWT_STATUS_TWT_NOT_REQUIRED: The peer has cleared
 * the TWT required bit(1->0) in its capabilities.
 */
enum qca_wlan_vendor_twt_status {
	QCA_WLAN_VENDOR_TWT_STATUS_OK = 0,
	QCA_WLAN_VENDOR_TWT_STATUS_TWT_NOT_ENABLED = 1,
	QCA_WLAN_VENDOR_TWT_STATUS_USED_DIALOG_ID = 2,
	QCA_WLAN_VENDOR_TWT_STATUS_SESSION_BUSY = 3,
	QCA_WLAN_VENDOR_TWT_STATUS_SESSION_NOT_EXIST = 4,
	QCA_WLAN_VENDOR_TWT_STATUS_NOT_SUSPENDED = 5,
	QCA_WLAN_VENDOR_TWT_STATUS_INVALID_PARAM = 6,
	QCA_WLAN_VENDOR_TWT_STATUS_NOT_READY = 7,
	QCA_WLAN_VENDOR_TWT_STATUS_NO_RESOURCE = 8,
	QCA_WLAN_VENDOR_TWT_STATUS_NO_ACK = 9,
	QCA_WLAN_VENDOR_TWT_STATUS_NO_RESPONSE = 10,
	QCA_WLAN_VENDOR_TWT_STATUS_DENIED = 11,
	QCA_WLAN_VENDOR_TWT_STATUS_UNKNOWN_ERROR = 12,
	QCA_WLAN_VENDOR_TWT_STATUS_ALREADY_SUSPENDED = 13,
	QCA_WLAN_VENDOR_TWT_STATUS_IE_INVALID = 14,
	QCA_WLAN_VENDOR_TWT_STATUS_PARAMS_NOT_IN_RANGE = 15,
	QCA_WLAN_VENDOR_TWT_STATUS_PEER_INITIATED_TERMINATE = 16,
	QCA_WLAN_VENDOR_TWT_STATUS_ROAM_INITIATED_TERMINATE = 17,
	QCA_WLAN_VENDOR_TWT_STATUS_SCC_MCC_CONCURRENCY_TERMINATE = 18,
	QCA_WLAN_VENDOR_TWT_STATUS_ROAMING_IN_PROGRESS = 19,
	QCA_WLAN_VENDOR_TWT_STATUS_CHANNEL_SWITCH_IN_PROGRESS = 20,
	QCA_WLAN_VENDOR_TWT_STATUS_SCAN_IN_PROGRESS = 21,
	QCA_WLAN_VENDOR_TWT_STATUS_POWER_SAVE_EXIT_TERMINATE = 22,
	QCA_WLAN_VENDOR_TWT_STATUS_TWT_REQUIRED = 23,
	QCA_WLAN_VENDOR_TWT_STATUS_TWT_NOT_REQUIRED = 24,
};

/**
 * enum qca_wlan_vendor_attr_twt_resume - Represents attributes for
 * TWT (Target Wake Time) resume request. These attributes are sent as part of
 * %QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_TWT_RESUME and
 * %QCA_NL80211_VENDOR_SUBCMD_WIFI_TEST_CONFIGURATION. Also used by
 * attributes through %QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_RESUME_NEXT_TWT: Optional (u8)
 * @QCA_WLAN_VENDOR_ATTR_TWT_RESUME_NEXT2_TWT: Optional (u32)
 * These attributes are used as the SP offset which is the offset from TSF after
 * which the wake happens. The units are in microseconds. Please note that
 * _NEXT_TWT is limited to u8 whereas _NEXT2_TWT takes the u32 data.
 * _NEXT2_TWT takes the precedence over _NEXT_TWT and thus the recommendation
 * is to use _NEXT2_TWT. If neither of these attributes is provided, the value
 * will be set to zero.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_RESUME_NEXT_TWT_SIZE: Required (u32)
 * This attribute represents the next TWT subfield size.
 * Value 0 represents 0 bits, 1 represents 32 bits, 2 for 48 bits,
 * and 4 for 64 bits.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_RESUME_FLOW_ID: Required (u8).
 * Flow ID is the unique identifier for each TWT session. This attribute
 * represents the respective TWT session to resume.
 * Flow ID values from 0 to 254 represent a single TWT session
 * Flow ID value of 255 represents all TWT sessions.
 * If an invalid dialog id is provided, status
 * QCA_WLAN_VENDOR_TWT_STATUS_SESSION_NOT_EXIST will be returned.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_RESUME_MAC_ADDR: 6-byte MAC address
 * Represents the MAC address of the peer to which TWT Resume is
 * being sent. This is used in AP mode to represent the respective
 * client and is a required parameter. In STA mode, this is an optional
 * parameter
 */
enum qca_wlan_vendor_attr_twt_resume {
	QCA_WLAN_VENDOR_ATTR_TWT_RESUME_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TWT_RESUME_NEXT_TWT = 1,
	QCA_WLAN_VENDOR_ATTR_TWT_RESUME_NEXT_TWT_SIZE = 2,
	QCA_WLAN_VENDOR_ATTR_TWT_RESUME_FLOW_ID = 3,
	QCA_WLAN_VENDOR_ATTR_TWT_RESUME_NEXT2_TWT = 4,
	QCA_WLAN_VENDOR_ATTR_TWT_RESUME_MAC_ADDR = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TWT_RESUME_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TWT_RESUME_MAX =
	QCA_WLAN_VENDOR_ATTR_TWT_RESUME_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_twt_nudge - Represents attributes for
 * TWT (Target Wake Time) nudge request. TWT nudge is a combination of suspend
 * and resume in a single request. These attributes are sent as part of
 * %QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_FLOW_ID: Required (u8)
 * Flow ID is the unique identifier for each TWT session. This attribute
 * represents the respective TWT session to suspend and resume.
 * Flow ID values from 0 to 254 represent a single TWT session
 * Flow ID value of 255 represents all TWT sessions in TWT NUDGE request
 * and response.
 * If an invalid dialog id is provided, status
 * QCA_WLAN_VENDOR_TWT_STATUS_SESSION_NOT_EXIST will be returned.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_WAKE_TIME: Required (u32)
 * This attribute is used as the SP offset which is the offset from
 * TSF after which the wake happens. The units are in microseconds.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_NEXT_TWT_SIZE: Required (u32)
 * This attribute represents the next TWT subfield size.
 * Value 0 represents 0 bits, 1 represents 32 bits, 2 for 48 bits,
 * and 4 for 64 bits.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_MAC_ADDR: 6-byte MAC address
 * Represents the MAC address of the peer to which TWT Suspend and Resume is
 * being sent. This is used in AP mode to represent the respective
 * client and is a required parameter. In STA mode, this is an optional
 * parameter.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_WAKE_TIME_TSF: Optional (u64)
 * This field contains absolute TSF value of the time at which the TWT
 * session will be resumed.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_SP_START_OFFSET: Optional (s32)
 * This field will be used when device supports Flexible TWT.
 * This field contains an offset value by which to shift the starting time
 * of the next service period. The value of offset can be negative or positive.
 * If provided, this attribute will override
 * QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_WAKE_TIME. The units are in microseconds.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_PAD: Attribute used for padding for 64-bit
 * alignment.
 */
enum qca_wlan_vendor_attr_twt_nudge {
	QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_FLOW_ID = 1,
	QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_WAKE_TIME = 2,
	QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_NEXT_TWT_SIZE = 3,
	QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_MAC_ADDR = 4,
	QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_WAKE_TIME_TSF = 5,
	QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_SP_START_OFFSET = 6,
	QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_PAD = 7,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_MAX =
	QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_twt_stats: Represents attributes for
 * TWT (Target Wake Time) get statistics and clear statistics request.
 * These attributes are sent as part of
 * %QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_STATS_FLOW_ID: Required (u8)
 * Flow ID is the unique identifier for each TWT session. This attribute
 * represents the respective TWT session for get and clear TWT statistics.
 * Flow ID values from 0 to 254 represent a single TWT session
 * Flow ID value of 255 represents all TWT sessions in
 * 1) TWT GET STATISTICS request and response
 * 2) TWT CLEAR STATISTICS request
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_STATS_MAC_ADDR: 6-byte MAC address
 * Represents the MAC address of the peer for which TWT Statistics
 * is required.
 * In AP mode this is used to represent the respective
 * client and is a required parameter for
 * 1) TWT GET STATISTICS request and response
 * 2) TWT CLEAR STATISTICS request and response
 * In STA mode, this is an optional parameter.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_STATS_SESSION_WAKE_DURATION: Required (u32)
 * This is the duration of the service period in microseconds.
 * Obtained in the QCA_WLAN_TWT_GET_STATS response from the firmware.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVG_WAKE_DURATION: Required (u32)
 * Average of the actual wake duration observed so far. Unit is microseconds.
 * Obtained in the QCA_WLAN_TWT_GET_STATS response from the firmware.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_STATS_NUM_SP_ITERATIONS: Required (u32)
 * The number of TWT service periods elapsed so far.
 * Obtained in the QCA_WLAN_TWT_GET_STATS response from the firmware.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_STATS_MIN_WAKE_DURATION: Required (u32)
 * This is the minimum value of the wake duration observed across
 * QCA_WLAN_VENDOR_ATTR_TWT_STATS_NUM_SP_ITERATIONS. Unit is
 * microseconds.
 * Obtained in the QCA_WLAN_TWT_GET_STATS response from the firmware.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_STATS_MAX_WAKE_DURATION: Required (u32)
 * This is the maximum value of wake duration observed across
 * QCA_WLAN_VENDOR_ATTR_TWT_STATS_NUM_SP_ITERATIONS. Unit is
 * microseconds.
 * Obtained in the QCA_WLAN_TWT_GET_STATS response from the firmware.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_TX_MPDU: Required (u32)
 * Average number of MPDUs transmitted successfully across
 * QCA_WLAN_VENDOR_ATTR_TWT_STATS_NUM_SP_ITERATIONS.
 * Obtained in the QCA_WLAN_TWT_GET_STATS response from the firmware.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_RX_MPDU: Required (u32)
 * Average number of MPDUs received successfully across
 * QCA_WLAN_VENDOR_ATTR_TWT_STATS_NUM_SP_ITERATIONS.
 * Obtained in the QCA_WLAN_TWT_GET_STATS response from the firmware.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_TX_PACKET_SIZE: Required (u32)
 * Average number of bytes transmitted successfully across
 * QCA_WLAN_VENDOR_ATTR_TWT_STATS_NUM_SP_ITERATIONS.
 * Obtained in the QCA_WLAN_TWT_GET_STATS response from the firmware.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_RX_PACKET_SIZE: Required (u32)
 * Average number of bytes received successfully across
 * QCA_WLAN_VENDOR_ATTR_TWT_STATS_NUM_SP_ITERATIONS.
 * Obtained in the QCA_WLAN_TWT_GET_STATS response from the firmware.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_STATS_STATUS: Required (u32)
 * Status of the TWT GET STATISTICS request.
 * This contains status values in enum qca_wlan_vendor_twt_status
 * Obtained in the QCA_WLAN_TWT_GET_STATS response from the firmware.
 */
enum qca_wlan_vendor_attr_twt_stats {
	QCA_WLAN_VENDOR_ATTR_TWT_STATS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TWT_STATS_FLOW_ID = 1,
	QCA_WLAN_VENDOR_ATTR_TWT_STATS_MAC_ADDR = 2,
	QCA_WLAN_VENDOR_ATTR_TWT_STATS_SESSION_WAKE_DURATION = 3,
	QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVG_WAKE_DURATION = 4,
	QCA_WLAN_VENDOR_ATTR_TWT_STATS_NUM_SP_ITERATIONS = 5,
	QCA_WLAN_VENDOR_ATTR_TWT_STATS_MIN_WAKE_DURATION = 6,
	QCA_WLAN_VENDOR_ATTR_TWT_STATS_MAX_WAKE_DURATION = 7,
	QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_TX_MPDU = 8,
	QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_RX_MPDU = 9,
	QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_TX_PACKET_SIZE = 10,
	QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_RX_PACKET_SIZE = 11,
	QCA_WLAN_VENDOR_ATTR_TWT_STATS_STATUS = 12,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TWT_STATS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TWT_STATS_MAX =
	QCA_WLAN_VENDOR_ATTR_TWT_STATS_AFTER_LAST - 1,
};

/**
 * qca_wlan_twt_get_capa  - Represents the bitmap of TWT capabilities
 * supported by the device and the peer.
 * Values for %QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_GET_CAPABILITIES
 *
 * @QCA_WLAN_TWT_CAPA_REQUESTOR: TWT requestor support is advertised by
 * TWT non-scheduling STA. This capability is advertised in the HE
 * Capability/Extended Capabilities information element in the
 * Association Request frame by the device.
 *
 * @QCA_WLAN_TWT_CAPA_RESPONDER: TWT responder support is advertised by
 * the TWT scheduling AP. This capability is advertised in the Extended
 * Capabilities/HE Capabilities information element.
 *
 * @QCA_WLAN_TWT_CAPA_BROADCAST: On the requestor side, this indicates support
 * for the broadcast TWT functionality. On the responder side, this indicates
 * support for the role of broadcast TWT scheduling functionality. This
 * capability is advertised in the HE Capabilities information element.
 *
 * @QCA_WLAN_TWT_CAPA_TWT_FLEXIBLE: The device supports flexible TWT schedule.
 * This capability is advertised in the HE Capabilities information element.
 *
 * @QCA_WLAN_TWT_CAPA_REQUIRED: The TWT Required is advertised by AP to indicate
 * that it mandates the associated HE STAs to support TWT. This capability is
 * advertised by AP in the HE Operation Parameters field of the HE Operation
 * information element.
 */
enum qca_wlan_twt_capa {
	QCA_WLAN_TWT_CAPA_REQUESTOR = BIT(0),
	QCA_WLAN_TWT_CAPA_RESPONDER = BIT(1),
	QCA_WLAN_TWT_CAPA_BROADCAST = BIT(2),
	QCA_WLAN_TWT_CAPA_FLEXIBLE =  BIT(3),
	QCA_WLAN_TWT_CAPA_REQUIRED =  BIT(4),
};

/**
 * enum qca_wlan_vendor_attr_twt_capability  - Represents attributes for TWT
 * get capabilities request type. Used by QCA_WLAN_TWT_GET_CAPABILITIES TWT
 * operation.
 * @QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MAC_ADDR: 6-byte MAC address
 * Represents the MAC address of the peer for which the TWT capabilities
 * are being queried. This is used in AP mode to represent the respective
 * client. In STA mode, this is an optional parameter.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_SELF: (u16).
 * Self TWT capabilities. Carries a bitmap of TWT capabilities specified in
 * enum qca_wlan_twt_capa.
 * @QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_PEER: (u16).
 * Peer TWT capabilities. Carries a bitmap of TWT capabilities specified in
 * enum qca_wlan_twt_capa.
 */
enum qca_wlan_vendor_attr_twt_capability {
	QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MAC_ADDR = 1,
	QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_SELF = 2,
	QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_PEER = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MAX =
	QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_twt_set_param: Represents attributes for
 * TWT (Target Wake Time) related parameters. It is used when
 * %QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION is set to %QCA_WLAN_TWT_SET_PARAM.
 * These attributes are sent as part of %QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SET_PARAM_AP_AC_VALUE: Optional (u8)
 * This attribute configures AC parameters to be used for all TWT
 * sessions in AP mode.
 * Uses the enum qca_wlan_ac_type values.
 *
 * @QCA_WLAN_VENDOR_ATTR_TWT_SET_PARAM_UNAVAILABILITY_MODE: Flag attribute,
 * used by TWT responder to indicate unavailability outside of the SPs.
 * Enable (flag attribute present) - Indicates that the TWT responder may be
 * unavailable outside of the SPs of its broadcast TWT schedule.
 * Disable (flag attribute not present) - Indicates that the responder will be
 * available for all TWT sessions (including individual TWT).
 */
enum qca_wlan_vendor_attr_twt_set_param {
	QCA_WLAN_VENDOR_ATTR_TWT_SET_PARAM_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TWT_SET_PARAM_AP_AC_VALUE = 1,
	QCA_WLAN_VENDOR_ATTR_TWT_SET_PARAM_UNAVAILABILITY_MODE = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TWT_SET_PARAM_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TWT_SET_PARAM_MAX =
	QCA_WLAN_VENDOR_ATTR_TWT_SET_PARAM_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_twt_setup_resp_type - Represents the response type by
 * the TWT responder
 *
 * @QCA_WLAN_VENDOR_TWT_RESP_ALTERNATE: TWT responder suggests TWT
 * parameters that are different from TWT requesting STA suggested
 * or demanded TWT parameters
 * @QCA_WLAN_VENDOR_TWT_RESP_DICTATE: TWT responder demands TWT
 * parameters that are different from TWT requesting STA TWT suggested
 * or demanded parameters
 * @QCA_WLAN_VENDOR_TWT_RESP_REJECT: TWT responder rejects TWT
 * setup
 * @QCA_WLAN_VENDOR_TWT_RESP_ACCEPT: TWT responder accepts the TWT
 * setup.
 */
enum qca_wlan_vendor_twt_setup_resp_type {
	QCA_WLAN_VENDOR_TWT_RESP_ALTERNATE = 1,
	QCA_WLAN_VENDOR_TWT_RESP_DICTATE = 2,
	QCA_WLAN_VENDOR_TWT_RESP_REJECT = 3,
	QCA_WLAN_VENDOR_TWT_RESP_ACCEPT = 4,
};

/**
 * enum qca_wlan_vendor_twt_setup_req_type - Required (u8)
 * Represents the setup type being requested for TWT.
 * @QCA_WLAN_VENDOR_TWT_SETUP_REQUEST: STA is not specifying all the TWT
 * parameters but relying on AP to fill the parameters during the negotiation.
 * @QCA_WLAN_VENDOR_TWT_SETUP_SUGGEST: STA will provide all the suggested
 * values which the AP may accept or AP may provide alternative parameters
 * which the STA may accept.
 * @QCA_WLAN_VENDOR_TWT_SETUP_DEMAND: STA is not willing to accept any
 * alternate parameters than the requested ones.
 */
enum qca_wlan_vendor_twt_setup_req_type {
	QCA_WLAN_VENDOR_TWT_SETUP_REQUEST = 1,
	QCA_WLAN_VENDOR_TWT_SETUP_SUGGEST = 2,
	QCA_WLAN_VENDOR_TWT_SETUP_DEMAND = 3,
};

/**
 * enum qca_wlan_roam_scan_event_type - Type of roam scan event
 *
 * Indicates the type of roam scan event sent by firmware/driver.
 *
 * @QCA_WLAN_ROAM_SCAN_TRIGGER_EVENT: Roam scan trigger event type.
 * @QCA_WLAN_ROAM_SCAN_STOP_EVENT: Roam scan stopped event type.
 */
enum qca_wlan_roam_scan_event_type {
	QCA_WLAN_ROAM_SCAN_TRIGGER_EVENT = 0,
	QCA_WLAN_ROAM_SCAN_STOP_EVENT = 1,
};

/**
 * enum qca_wlan_roam_scan_trigger_reason - Roam scan trigger reason
 *
 * Indicates the reason for triggering roam scan by firmware/driver.
 *
 * @QCA_WLAN_ROAM_SCAN_TRIGGER_REASON_LOW_RSSI: Due to low RSSI of current AP.
 * @QCA_WLAN_ROAM_SCAN_TRIGGER_REASON_HIGH_PER: Due to high packet error rate.
 */
enum qca_wlan_roam_scan_trigger_reason {
	QCA_WLAN_ROAM_SCAN_TRIGGER_REASON_LOW_RSSI = 0,
	QCA_WLAN_ROAM_SCAN_TRIGGER_REASON_HIGH_PER = 1,
};

/**
 * enum qca_wlan_vendor_attr_roam_scan - Vendor subcmd attributes to report
 * roam scan related details from driver/firmware to user space. enum values
 * are used for NL attributes sent with
 * %QCA_NL80211_VENDOR_SUBCMD_ROAM_SCAN_EVENT sub command.
 */
enum qca_wlan_vendor_attr_roam_scan {
	QCA_WLAN_VENDOR_ATTR_ROAM_SCAN_INVALID = 0,
	/* Encapsulates type of roam scan event being reported. enum
	 * qca_wlan_roam_scan_event_type describes the possible range of
	 * values. u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_SCAN_EVENT_TYPE = 1,
	/* Encapsulates reason for triggering roam scan. enum
	 * qca_wlan_roam_scan_trigger_reason describes the possible range of
	 * values. u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_SCAN_TRIGGER_REASON = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ROAM_SCAN_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ROAM_SCAN_MAX =
	QCA_WLAN_VENDOR_ATTR_ROAM_SCAN_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_cfr_data_transport_modes - Defines QCA vendor CFR data
 * transport modes and is used by the attribute
 * QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_TRANSPORT_MODE as a part of the vendor
 * command QCA_NL80211_VENDOR_SUBCMD_PEER_CFR_CAPTURE_CFG.
 * @QCA_WLAN_VENDOR_CFR_DATA_RELAY_FS: Use relayfs to send CFR data.
 * @QCA_WLAN_VENDOR_CFR_DATA_NETLINK_EVENTS: Use netlink events to send CFR
 * data. The data shall be encapsulated within
 * QCA_WLAN_VENDOR_ATTR_PEER_CFR_RESP_DATA along with the vendor sub command
 * QCA_NL80211_VENDOR_SUBCMD_PEER_CFR_CAPTURE_CFG as an asynchronous event.
 */
enum qca_wlan_vendor_cfr_data_transport_modes {
	QCA_WLAN_VENDOR_CFR_DATA_RELAY_FS = 0,
	QCA_WLAN_VENDOR_CFR_DATA_NETLINK_EVENTS = 1,
};

/**
 * enum qca_wlan_vendor_cfr_method - QCA vendor CFR methods used by
 * attribute QCA_WLAN_VENDOR_ATTR_PEER_CFR_METHOD as part of vendor
 * command QCA_NL80211_VENDOR_SUBCMD_PEER_CFR_CAPTURE_CFG.
 * @QCA_WLAN_VENDOR_CFR_METHOD_QOS_NULL: CFR method using QoS Null frame
 * @QCA_WLAN_VENDOR_CFR_QOS_NULL_WITH_PHASE: CFR method using QoS Null frame
 * with phase
 * @QCA_WLAN_VENDOR_CFR_PROBE_RESPONSE: CFR method using Probe Response frame
 */
enum qca_wlan_vendor_cfr_method {
	QCA_WLAN_VENDOR_CFR_METHOD_QOS_NULL = 0,
	QCA_WLAN_VENDOR_CFR_QOS_NULL_WITH_PHASE = 1,
	QCA_WLAN_VENDOR_CFR_PROBE_RESPONSE = 2,
};

/**
 * enum qca_wlan_vendor_cfr_capture_type - QCA vendor CFR capture type used by
 * attribute QCA_WLAN_VENDOR_ATTR_PEER_CFR_CAPTURE_TYPE.
 * @QCA_WLAN_VENDOR_CFR_DIRECT_FTM: Filter directed FTM ACK frames.
 * @QCA_WLAN_VENDOR_CFR_ALL_FTM_ACK: Filter all FTM ACK frames.
 * @QCA_WLAN_VENDOR_CFR_DIRECT_NDPA_NDP: Filter NDPA NDP directed frames.
 * @QCA_WLAN_VENDOR_CFR_TA_RA: Filter frames based on TA/RA/Subtype which
 * is provided by one or more of below attributes:
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA_MASK
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA_MASK
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_MGMT_FILTER
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_CTRL_FILTER
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_DATA_FILTER
 * @QCA_WLAN_CFR_ALL_PACKET: Filter all packets.
 * @QCA_WLAN_VENDOR_CFR_NDPA_NDP_ALL: Filter all NDPA NDP frames.
 */
enum qca_wlan_vendor_cfr_capture_type {
	QCA_WLAN_VENDOR_CFR_DIRECT_FTM = 0,
	QCA_WLAN_VENDOR_CFR_ALL_FTM_ACK = 1,
	QCA_WLAN_VENDOR_CFR_DIRECT_NDPA_NDP = 2,
	QCA_WLAN_VENDOR_CFR_TA_RA = 3,
	QCA_WLAN_VENDOR_CFR_ALL_PACKET = 4,
	QCA_WLAN_VENDOR_CFR_NDPA_NDP_ALL = 5,
};

/**
 * enum qca_wlan_vendor_peer_cfr_capture_attr - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_PEER_CFR_CAPTURE_CFG to configure peer
 * Channel Frequency Response capture parameters and enable periodic CFR
 * capture.
 *
 * @QCA_WLAN_VENDOR_ATTR_CFR_PEER_MAC_ADDR: Optional (6-byte MAC address)
 * MAC address of peer. This is for CFR version 1 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE: Required (flag)
 * Enable peer CFR capture. This attribute is mandatory to enable peer CFR
 * capture. If this attribute is not present, peer CFR capture is disabled.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_BANDWIDTH: Optional (u8)
 * BW of measurement, attribute uses the values in enum nl80211_chan_width
 * Supported values: 20, 40, 80, 80+80, 160.
 * Note that all targets may not support all bandwidths.
 * This attribute is mandatory for version 1 if attribute
 * QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE is used.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_PERIODICITY: Optional (u32)
 * Periodicity of CFR measurement in milliseconds.
 * Periodicity should be a multiple of Base timer.
 * Current Base timer value supported is 10 milliseconds (default).
 * 0 for one shot capture.
 * This attribute is mandatory for version 1 if attribute
 * QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE is used.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_METHOD: Optional (u8)
 * Method used to capture Channel Frequency Response.
 * Attribute uses the values defined in enum qca_wlan_vendor_cfr_method.
 * This attribute is mandatory for version 1 if attribute
 * QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE is used.
 *
 * @QCA_WLAN_VENDOR_ATTR_PERIODIC_CFR_CAPTURE_ENABLE: Optional (flag)
 * Enable periodic CFR capture.
 * This attribute is mandatory for version 1 to enable Periodic CFR capture.
 * If this attribute is not present, periodic CFR capture is disabled.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_VERSION: Optional (u8)
 * Value is 1 or 2 since there are two versions of CFR capture. Two versions
 * can't be enabled at same time. This attribute is mandatory if target
 * support both versions and use one of them.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE_GROUP_BITMAP: Optional (u32)
 * This attribute is mandatory for version 2 if
 * QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_ENTRY is used.
 * Bits 15:0 bitfield indicates which group is to be enabled.
 * Bits 31:16 Reserved for future use.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_DURATION: Optional (u32)
 * CFR capture duration in microsecond. This attribute is mandatory for
 * version 2 if attribute QCA_WLAN_VENDOR_ATTR_PEER_CFR_INTERVAL is used.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_INTERVAL: Optional (u32)
 * CFR capture interval in microsecond. This attribute is mandatory for
 * version 2 if attribute QCA_WLAN_VENDOR_ATTR_PEER_CFR_DURATION is used.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_CAPTURE_TYPE: Optional (u32)
 * CFR capture type is defined in enum qca_wlan_vendor_cfr_capture_type.
 * This attribute is mandatory for version 2.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_UL_MU_MASK: Optional (u64)
 * Bitfield indicating which user in the current UL MU transmissions are
 * enabled for CFR capture. Bits 36 to 0 indicate user indexes for 37 users in
 * a UL MU transmission. If bit 0 is set, the CFR capture will happen for user
 * index 0 in the current UL MU transmission. If bits 0 and 2 are set, CFR
 * capture for UL MU TX corresponds to user indices 0 and 2. Bits 63:37 are
 * reserved for future use. This is for CFR version 2 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_FREEZE_TLV_DELAY_COUNT: Optional (u32)
 * Indicates the number of consecutive RX frames to be skipped before CFR
 * capture is enabled again. This is for CFR version 2 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TABLE: Nested attribute containing
 * one or more %QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_ENTRY attributes.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_ENTRY: Nested attribute containing
 * the following group attributes:
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NUMBER,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA_MASK,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA_MASK,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NSS,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_BW,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_MGMT_FILTER,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_CTRL_FILTER,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_DATA_FILTER
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NUMBER: Optional (u32)
 * Target supports multiple groups for some configurations. The group number
 * can be any value between 0 and 15. This is for CFR version 2 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA: Optional (6-byte MAC address)
 * Transmitter address which is used to filter frames. This MAC address takes
 * effect with QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA_MASK. This is for CFR
 * version 2 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA: Optional (6-byte MAC address)
 * Receiver address which is used to filter frames. This MAC address takes
 * effect with QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA_MASK. This is for CFR
 * version 2 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA_MASK: Optional (6-byte MAC address)
 * Mask of transmitter address which is used to filter frames. This is for CFR
 * version 2 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA_MASK: Optional (6-byte MAC address)
 * Mask of receiver address which is used to filter frames. This is for CFR
 * version 2 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NSS: Optional (u32)
 * Indicates frames with a specific NSS will be filtered for CFR capture.
 * This is for CFR version 2 only. This is a bitmask. Bits 7:0 request CFR
 * capture to be done for frames matching the NSS specified within this bitmask.
 * Bits 31:8 are reserved for future use. Bits 7:0 map to NSS:
 *     bit 0 : NSS 1
 *     bit 1 : NSS 2
 *     ...
 *     bit 7 : NSS 8
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_BW: Optional (u32)
 * Indicates frames with a specific bandwidth will be filtered for CFR capture.
 * This is for CFR version 2 only. This is a bitmask. Bits 4:0 request CFR
 * capture to be done for frames matching the bandwidths specified within this
 * bitmask. Bits 31:5 are reserved for future use. Bits 4:0 map to bandwidth
 * numerated in enum nl80211_band (although not all bands may be supported
 * by a given device).
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_MGMT_FILTER: Optional (u32)
 * Management frames matching the subtype filter categories will be filtered in
 * by MAC for CFR capture. This is a bitmask in which each bit represents the
 * corresponding Management frame subtype value per IEEE Std 802.11-2016,
 * 9.2.4.1.3 Type and Subtype subfields. For example, Beacon frame control type
 * is 8 and its value is 1 << 8 = 0x100. This is for CFR version 2 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_CTRL_FILTER: Optional (u32)
 * Control frames matching the subtype filter categories will be filtered in by
 * MAC for CFR capture. This is a bitmask in which each bit represents the
 * corresponding Control frame subtype value per IEEE Std 802.11-2016,
 * 9.2.4.1.3 Type and Subtype subfields. This is for CFR version 2 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_DATA_FILTER: Optional (u32)
 * Data frames matching the subtype filter categories will be filtered in by
 * MAC for CFR capture. This is a bitmask in which each bit represents the
 * corresponding Data frame subtype value per IEEE Std 802.11-2016,
 * 9.2.4.1.3 Type and Subtype subfields. This is for CFR version 2 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_TRANSPORT_MODE: Optional (u8)
 * Userspace can use this attribute to specify the driver about which transport
 * mode shall be used by the driver to send CFR data to userspace. Uses values
 * from enum qca_wlan_vendor_cfr_data_transport_modes. When this attribute is
 * not present, the driver shall use the default transport mechanism which is
 * QCA_WLAN_VENDOR_CFR_DATA_RELAY_FS.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_RECEIVER_PID: Optional (u32)
 * Userspace can use this attribute to specify the nl port id of the application
 * which receives the CFR data and processes it further so that the drivers can
 * unicast the netlink events to a specific application. Optionally included
 * when QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_TRANSPORT_MODE is set to
 * QCA_WLAN_VENDOR_CFR_DATA_NETLINK_EVENTS, not required otherwise. The drivers
 * shall multicast the netlink events when this attribute is not included.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_RESP_DATA: Required (NLA_BINARY).
 * This attribute will be used by the driver to encapsulate and send CFR data
 * to userspace along with QCA_NL80211_VENDOR_SUBCMD_PEER_CFR_CAPTURE_CFG as an
 * asynchronous event when the driver is configured to send CFR data using
 * netlink events with %QCA_WLAN_VENDOR_CFR_DATA_NETLINK_EVENTS.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_PAD: Attribute used for padding for 64-bit
 * alignment.
 */
enum qca_wlan_vendor_peer_cfr_capture_attr {
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_CAPTURE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_CFR_PEER_MAC_ADDR = 1,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE = 2,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_BANDWIDTH = 3,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_PERIODICITY = 4,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_METHOD = 5,
	QCA_WLAN_VENDOR_ATTR_PERIODIC_CFR_CAPTURE_ENABLE = 6,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_VERSION = 7,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE_GROUP_BITMAP = 8,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_DURATION = 9,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_INTERVAL = 10,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_CAPTURE_TYPE = 11,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_UL_MU_MASK = 12,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_FREEZE_TLV_DELAY_COUNT = 13,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TABLE = 14,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_ENTRY = 15,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NUMBER = 16,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA = 17,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA = 18,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA_MASK = 19,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA_MASK = 20,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NSS = 21,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_BW = 22,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_MGMT_FILTER = 23,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_CTRL_FILTER = 24,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_DATA_FILTER = 25,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_TRANSPORT_MODE = 26,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_RECEIVER_PID = 27,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_RESP_DATA = 28,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_PAD = 29,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_MAX =
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_throughput_level - Current throughput level
 *
 * Indicates the current level of throughput calculated by the driver. The
 * driver may choose different thresholds to decide whether the throughput level
 * is low or medium or high based on variety of parameters like physical link
 * capacity of the current connection, the number of packets being dispatched
 * per second, etc. The throughput level events might not be consistent with the
 * actual current throughput value being observed.
 *
 * @QCA_WLAN_THROUGHPUT_LEVEL_LOW: Low level of throughput
 * @QCA_WLAN_THROUGHPUT_LEVEL_MEDIUM: Medium level of throughput
 * @QCA_WLAN_THROUGHPUT_LEVEL_HIGH: High level of throughput
 */
enum qca_wlan_throughput_level {
	QCA_WLAN_THROUGHPUT_LEVEL_LOW = 0,
	QCA_WLAN_THROUGHPUT_LEVEL_MEDIUM = 1,
	QCA_WLAN_THROUGHPUT_LEVEL_HIGH = 2,
};

/**
 * enum qca_wlan_vendor_attr_throughput_change - Vendor subcmd attributes to
 * report throughput changes from the driver to user space. enum values are used
 * for netlink attributes sent with
 * %QCA_NL80211_VENDOR_SUBCMD_THROUGHPUT_CHANGE_EVENT sub command.
 */
enum qca_wlan_vendor_attr_throughput_change {
	QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_INVALID = 0,
	/* Indicates the direction of throughput in which the change is being
	 * reported. u8 attribute. Value is 0 for TX and 1 for RX.
	 */
	QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_DIRECTION = 1,
	/* Indicates the newly observed throughput level. enum
	 * qca_wlan_throughput_level describes the possible range of values.
	 * u8 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_THROUGHPUT_LEVEL = 2,
	/* Indicates the driver's guidance on the new value to be set to
	 * kernel's TCP parameter tcp_limit_output_bytes. u32 attribute. The
	 * driver may optionally include this attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_TCP_LIMIT_OUTPUT_BYTES = 3,
	/* Indicates the driver's guidance on the new value to be set to
	 * kernel's TCP parameter tcp_adv_win_scale. s8 attribute. Possible
	 * values are from -31 to 31. The driver may optionally include this
	 * attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_TCP_ADV_WIN_SCALE = 4,
	/* Indicates the driver's guidance on the new value to be set to
	 * kernel's TCP parameter tcp_delack_seg. u32 attribute. The driver may
	 * optionally include this attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_TCP_DELACK_SEG = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_MAX =
	QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_AFTER_LAST - 1,
};

/**
 * enum qca_coex_config_profiles - This enum defines different types of
 * traffic streams that can be prioritized one over the other during coex
 * scenarios.
 * The types defined in this enum are categorized in the below manner.
 * 0 - 31 values corresponds to WLAN
 * 32 - 63 values corresponds to BT
 * 64 - 95 values corresponds to Zigbee
 * @QCA_WIFI_STA_DISCOVERY: Prioritize discovery frames for WLAN STA
 * @QCA_WIFI_STA_CONNECTION: Prioritize connection frames for WLAN STA
 * @QCA_WIFI_STA_CLASS_3_MGMT: Prioritize class 3 mgmt frames for WLAN STA
 * @QCA_WIFI_STA_DATA : Prioritize data frames for WLAN STA
 * @QCA_WIFI_STA_ALL: Priritize all frames for WLAN STA
 * @QCA_WIFI_SAP_DISCOVERY: Prioritize discovery frames for WLAN SAP
 * @QCA_WIFI_SAP_CONNECTION: Prioritize connection frames for WLAN SAP
 * @QCA_WIFI_SAP_CLASS_3_MGMT: Prioritize class 3 mgmt frames for WLAN SAP
 * @QCA_WIFI_SAP_DATA: Prioritize data frames for WLAN SAP
 * @QCA_WIFI_SAP_ALL: Prioritize all frames for WLAN SAP
 * @QCA_BT_A2DP: Prioritize BT A2DP
 * @QCA_BT_BLE: Prioritize BT BLE
 * @QCA_BT_SCO: Prioritize BT SCO
 * @QCA_ZB_LOW: Prioritize Zigbee Low
 * @QCA_ZB_HIGH: Prioritize Zigbee High
 */
enum qca_coex_config_profiles {
	/* 0 - 31 corresponds to WLAN */
	QCA_WIFI_STA_DISCOVERY = 0,
	QCA_WIFI_STA_CONNECTION = 1,
	QCA_WIFI_STA_CLASS_3_MGMT = 2,
	QCA_WIFI_STA_DATA = 3,
	QCA_WIFI_STA_ALL = 4,
	QCA_WIFI_SAP_DISCOVERY = 5,
	QCA_WIFI_SAP_CONNECTION = 6,
	QCA_WIFI_SAP_CLASS_3_MGMT = 7,
	QCA_WIFI_SAP_DATA = 8,
	QCA_WIFI_SAP_ALL = 9,
	QCA_WIFI_CASE_MAX = 31,
	/* 32 - 63 corresponds to BT */
	QCA_BT_A2DP = 32,
	QCA_BT_BLE = 33,
	QCA_BT_SCO = 34,
	QCA_BT_CASE_MAX = 63,
	/* 64 - 95 corresponds to Zigbee */
	QCA_ZB_LOW = 64,
	QCA_ZB_HIGH = 65,
	QCA_ZB_CASE_MAX = 95,
	/* 0xff is default value if the u8 profile value is not set. */
	QCA_COEX_CONFIG_PROFILE_DEFAULT_VALUE = 255
};

/**
 * enum qca_vendor_attr_coex_config_types - Coex configurations types.
 * This enum defines the valid set of values of coex configuration types. These
 * values may used by attribute
 * %QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_CONFIG_TYPE.
 *
 * @QCA_WLAN_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_COEX_RESET: Reset all the
 *	weights to default values.
 * @QCA_WLAN_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_COEX_START: Start to config
 *	weights with configurability value.
 */
enum qca_vendor_attr_coex_config_types {
	QCA_WLAN_VENDOR_ATTR_COEX_CONFIG_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_COEX_RESET = 1,
	QCA_WLAN_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_COEX_START = 2,
};

/**
 * enum qca_vendor_attr_coex_config - Specifies vendor coex config attributes
 *
 * @QCA_VENDOR_ATTR_COEX_CONFIG_PROFILES: This attribute contains variable
 * length array of 8-bit values from enum qca_coex_config_profiles.
 * FW will prioritize the profiles in the order given in the array encapsulated
 * in this attribute.
 * For example:
 * -----------------------------------------------------------------------
 * |     1       |       34       |        32         |         65       |
 * -----------------------------------------------------------------------
 * If the attribute contains the values defined in above array then it means
 * 1) Wifi STA connection has priority over BT_SCO, BT_A2DP and ZIGBEE HIGH.
 * 2) BT_SCO has priority over BT_A2DP.
 * 3) BT_A2DP has priority over ZIGBEE HIGH.
 * Profiles which are not listed in this array shall not be preferred over the
 * profiles which are listed in the array as a part of this attribute.
 */
enum qca_vendor_attr_coex_config {
	QCA_VENDOR_ATTR_COEX_CONFIG_INVALID = 0,
	QCA_VENDOR_ATTR_COEX_CONFIG_PROFILES = 1,

	/* Keep last */
	QCA_VENDOR_ATTR_COEX_CONFIG_AFTER_LAST,
	QCA_VENDOR_ATTR_COEX_CONFIG_MAX =
	QCA_VENDOR_ATTR_COEX_CONFIG_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_attr_coex_config_three_way - Specifies vendor coex config
 * attributes
 * Attributes for data used by QCA_NL80211_VENDOR_SUBCMD_COEX_CONFIG
 *
 * QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_CONFIG_TYPE: u32 attribute.
 * Indicate config type.
 * The config types are 32-bit values from qca_vendor_attr_coex_config_types
 *
 * @QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_1: u32 attribute.
 *	Indicate the Priority 1 profiles.
 *	The profiles are 8-bit values from enum qca_coex_config_profiles.
 *	In same priority level, maximum to 4 profiles can be set here.
 * @QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_2: u32 attribute.
 *	Indicate the Priority 2 profiles.
 *	The profiles are 8-bit values from enum qca_coex_config_profiles.
 *	In same priority level, maximum to 4 profiles can be set here.
 * @QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_3: u32 attribute.
 *	Indicate the Priority 3 profiles.
 *	The profiles are 8-bit values from enum qca_coex_config_profiles.
 *	In same priority level, maximum to 4 profiles can be set here.
 * @QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_4: u32 attribute.
 *	Indicate the Priority 4 profiles.
 *	The profiles are 8-bit values from enum qca_coex_config_profiles.
 *	In same priority level, maximum to 4 profiles can be set here.
 * NOTE:
 * Limitations for QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_x priority
 * arrangement:
 *	1: In the same u32 attribute (priority x), the profiles enum values own
 *	same priority level.
 *	2: 0xff is default value if the u8 profile value is not set.
 *	3: max to 4 rules/profiles in same priority level.
 *	4: max to 4 priority level (priority 1 - priority 4)
 *	5: one priority level only supports one scenario from WLAN/BT/ZB,
 *	hybrid rules not support.
 *	6: if WMI_COEX_CONFIG_THREE_WAY_COEX_RESET called, priority x will
 *	remain blank to reset all parameters.
 * For example:
 *
 *	If the attributes as follow:
 *	priority 1:
 *	------------------------------------
 *	|  0xff  |    0   |   1   |    2   |
 *	------------------------------------
 *	priority 2:
 *	-------------------------------------
 *	|  0xff  |  0xff  |  0xff  |   32   |
 *	-------------------------------------
 *	priority 3:
 *	-------------------------------------
 *	|  0xff  |  0xff  |  0xff  |   65   |
 *	-------------------------------------
 *	then it means:
 *	1: WIFI_STA_DISCOVERY, WIFI_STA_CLASS_3_MGMT and WIFI_STA_CONNECTION
 *		owns same priority level.
 *	2: WIFI_STA_DISCOVERY, WIFI_STA_CLASS_3_MGMT and WIFI_STA_CONNECTION
 *		has priority over BT_A2DP and ZB_HIGH.
 *	3: BT_A2DP has priority over ZB_HIGH.
 */
enum qca_vendor_attr_coex_config_three_way {
	QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_INVALID = 0,
	QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_CONFIG_TYPE = 1,
	QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_1 = 2,
	QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_2 = 3,
	QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_3 = 4,
	QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_4 = 5,

	/* Keep last */
	QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_AFTER_LAST,
	QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_MAX =
	QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_link_properties - Represent the link properties.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_PROPERTIES_MAC_ADDR: MAC address of the peer
 * (STA/AP) for the connected link.
 * @QCA_WLAN_VENDOR_ATTR_LINK_PROPERTIES_STA_FLAGS: Attribute containing a
 * &struct nl80211_sta_flag_update for the respective connected link. MAC
 * address of the peer represented by
 * QCA_WLAN_VENDOR_ATTR_LINK_PROPERTIES_MAC_ADDR.
 */
enum qca_wlan_vendor_attr_link_properties {
	QCA_WLAN_VENDOR_ATTR_LINK_PROPERTIES_INVALID = 0,
	/* 1 - 3 are reserved */
	QCA_WLAN_VENDOR_ATTR_LINK_PROPERTIES_MAC_ADDR = 4,
	QCA_WLAN_VENDOR_ATTR_LINK_PROPERTIES_STA_FLAGS = 5,

	/* Keep last */
	QCA_VENDOR_ATTR_LINK_PROPERTIES_AFTER_LAST,
	QCA_VENDOR_ATTR_LINK_PROPERTIES_MAX =
	QCA_VENDOR_ATTR_LINK_PROPERTIES_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_attr_peer_stats_cache_type - Represents peer stats cache type
 * This enum defines the valid set of values of peer stats cache types. These
 * values are used by attribute
 * %QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_TYPE.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_TX_RATE_STATS: Represents peer TX rate statistics
 * @QCA_WLAN_VENDOR_ATTR_PEER_RX_RATE_STATS: Represents peer RX rate statistics
 * @QCA_WLAN_VENDOR_ATTR_PEER_TX_SOJOURN_STATS: Represents peer TX sojourn
 * statistics
 */
enum qca_vendor_attr_peer_stats_cache_type {
	QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_TYPE_INVALID = 0,

	QCA_WLAN_VENDOR_ATTR_PEER_TX_RATE_STATS,
	QCA_WLAN_VENDOR_ATTR_PEER_RX_RATE_STATS,
	QCA_WLAN_VENDOR_ATTR_PEER_TX_SOJOURN_STATS,
};

/**
 * enum qca_wlan_vendor_attr_peer_stats_cache_params - This enum defines
 * attributes required for QCA_NL80211_VENDOR_SUBCMD_PEER_STATS_CACHE_FLUSH
 * Information in these attributes is used to flush peer rate statistics from
 * the driver to user application.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_TYPE: Unsigned 32-bit attribute
 * Indicate peer statistics cache type.
 * The statistics types are 32-bit values from
 * enum qca_vendor_attr_peer_stats_cache_type.
 * @QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_PEER_MAC: Unsigned 8-bit array
 * of size 6 octets, representing the peer MAC address.
 * @QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_DATA: Opaque data attribute
 * containing buffer of statistics to send to application layer entity.
 * @QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_PEER_COOKIE: Unsigned 64-bit attribute
 * representing a cookie for peer unique session.
 * @QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_PAD: Attribute used for padding for
 * 64-bit alignment.
 */
enum qca_wlan_vendor_attr_peer_stats_cache_params {
	QCA_WLAN_VENDOR_ATTR_PEER_STATS_INVALID = 0,

	QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_TYPE = 1,
	QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_PEER_MAC = 2,
	QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_DATA = 3,
	QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_PEER_COOKIE = 4,
	QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_PAD = 5,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_LAST,
	QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_MAX =
		QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_LAST - 1
};

/**
 * enum qca_mpta_helper_attr_zigbee_state - Current Zigbee state
 * This enum defines all the possible states of Zigbee, which can be
 * delivered in the QCA_MPTA_HELPER_VENDOR_ATTR_ZIGBEE_STATE attribute.
 *
 * @ZIGBEE_IDLE: Zigbee in idle state
 * @ZIGBEE_FORM_NETWORK: Zigbee forming network
 * @ZIGBEE_WAIT_JOIN: Zigbee waiting for joining network
 * @ZIGBEE_JOIN: Zigbee joining network
 * @ZIGBEE_NETWORK_UP: Zigbee network is up
 * @ZIGBEE_HMI: Zigbee in HMI mode
 */
enum qca_mpta_helper_attr_zigbee_state {
	ZIGBEE_IDLE = 0,
	ZIGBEE_FORM_NETWORK = 1,
	ZIGBEE_WAIT_JOIN = 2,
	ZIGBEE_JOIN = 3,
	ZIGBEE_NETWORK_UP = 4,
	ZIGBEE_HMI = 5,
};

/*
 * enum qca_mpta_helper_vendor_attr - Attributes used in vendor sub-command
 * QCA_NL80211_VENDOR_SUBCMD_MPTA_HELPER_CONFIG.
 */
enum qca_mpta_helper_vendor_attr {
	QCA_MPTA_HELPER_VENDOR_ATTR_INVALID = 0,
	/* Optional attribute used to update Zigbee state.
	 * enum qca_mpta_helper_attr_zigbee_state.
	 * NLA_U32 attribute.
	 */
	QCA_MPTA_HELPER_VENDOR_ATTR_ZIGBEE_STATE = 1,
	/* Optional attribute used to configure WLAN duration for Shape-OCS
	 * during interrupt.
	 * Set in pair with QCA_MPTA_HELPER_VENDOR_ATTR_INT_NON_WLAN_DURATION.
	 * Value range 0 ~ 300 (ms).
	 * NLA_U32 attribute.
	 */
	QCA_MPTA_HELPER_VENDOR_ATTR_INT_WLAN_DURATION = 2,
	/* Optional attribute used to configure non-WLAN duration for Shape-OCS
	 * during interrupt.
	 * Set in pair with QCA_MPTA_HELPER_VENDOR_ATTR_INT_WLAN_DURATION.
	 * Value range 0 ~ 300 (ms).
	 * NLA_U32 attribute.
	 */
	QCA_MPTA_HELPER_VENDOR_ATTR_INT_NON_WLAN_DURATION  = 3,
	/* Optional attribute used to configure WLAN duration for Shape-OCS
	 * monitor period.
	 * Set in pair with QCA_MPTA_HELPER_VENDOR_ATTR_MON_NON_WLAN_DURATION.
	 * Value range 0 ~ 300 (ms)
	 * NLA_U32 attribute
	 */
	QCA_MPTA_HELPER_VENDOR_ATTR_MON_WLAN_DURATION = 4,
	/* Optional attribute used to configure non-WLAN duration for Shape-OCS
	 * monitor period.
	 * Set in pair with QCA_MPTA_HELPER_VENDOR_ATTR_MON_WLAN_DURATION.
	 * Value range 0 ~ 300 (ms)
	 * NLA_U32 attribute
	 */
	QCA_MPTA_HELPER_VENDOR_ATTR_MON_NON_WLAN_DURATION  = 5,
	/* Optional attribute used to configure OCS interrupt duration.
	 * Set in pair with QCA_MPTA_HELPER_VENDOR_ATTR_MON_OCS_DURATION.
	 * Value range 1000 ~ 20000 (ms)
	 * NLA_U32 attribute
	 */
	QCA_MPTA_HELPER_VENDOR_ATTR_INT_OCS_DURATION  = 6,
	/* Optional attribute used to configure OCS monitor duration.
	 * Set in pair with QCA_MPTA_HELPER_VENDOR_ATTR_INT_OCS_DURATION.
	 * Value range 1000 ~ 20000 (ms)
	 * NLA_U32 attribute
	 */
	QCA_MPTA_HELPER_VENDOR_ATTR_MON_OCS_DURATION  = 7,
	/* Optional attribute used to notify WLAN firmware the current Zigbee
	 * channel.
	 * Value range 11 ~ 26
	 * NLA_U32 attribute
	 */
	QCA_MPTA_HELPER_VENDOR_ATTR_ZIGBEE_CHAN = 8,
	/* Optional attribute used to configure WLAN mute duration.
	 * Value range 0 ~ 400 (ms)
	 * NLA_U32 attribute
	 */
	QCA_MPTA_HELPER_VENDOR_ATTR_WLAN_MUTE_DURATION	= 9,

	/* keep last */
	QCA_MPTA_HELPER_VENDOR_ATTR_AFTER_LAST,
	QCA_MPTA_HELPER_VENDOR_ATTR_MAX =
		QCA_MPTA_HELPER_VENDOR_ATTR_AFTER_LAST - 1
};

/**
 * enum qca_wlan_vendor_beacon_reporting_op_types - Defines different types of
 * operations for which %QCA_NL80211_VENDOR_SUBCMD_BEACON_REPORTING can be used.
 * Will be used by %QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE.
 *
 * @QCA_WLAN_VENDOR_BEACON_REPORTING_OP_START: Sent by userspace to the driver
 * to request the driver to start reporting Beacon frames.
 * @QCA_WLAN_VENDOR_BEACON_REPORTING_OP_STOP: Sent by userspace to the driver to
 * request the driver to stop reporting Beacon frames.
 * @QCA_WLAN_VENDOR_BEACON_REPORTING_OP_BEACON_INFO: Sent by the driver to
 * userspace to report received Beacon frames.
 * @QCA_WLAN_VENDOR_BEACON_REPORTING_OP_PAUSE: Sent by the driver to userspace
 * to indicate that the driver is going to pause reporting Beacon frames.
 */
enum qca_wlan_vendor_beacon_reporting_op_types {
	QCA_WLAN_VENDOR_BEACON_REPORTING_OP_START = 0,
	QCA_WLAN_VENDOR_BEACON_REPORTING_OP_STOP = 1,
	QCA_WLAN_VENDOR_BEACON_REPORTING_OP_BEACON_INFO = 2,
	QCA_WLAN_VENDOR_BEACON_REPORTING_OP_PAUSE = 3,
};

/**
 * enum qca_wlan_vendor_beacon_reporting_pause_reasons - Defines different types
 * of reasons for which the driver is pausing reporting Beacon frames. Will be
 * used by %QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_PAUSE_REASON.
 *
 * @QCA_WLAN_VENDOR_BEACON_REPORTING_PAUSE_REASON_UNSPECIFIED: For unspecified
 * reasons.
 * @QCA_WLAN_VENDOR_BEACON_REPORTING_PAUSE_REASON_SCAN_STARTED: When the
 * driver/firmware is starting a scan.
 * @QCA_WLAN_VENDOR_BEACON_REPORTING_PAUSE_REASON_DISCONNECTED: When the
 * driver/firmware disconnects from the ESS and indicates the disconnection to
 * userspace (non-seamless roaming case). This reason code will be used by the
 * driver/firmware to indicate stopping of beacon report events. Userspace will
 * need to start beacon reporting again (if desired) by sending vendor command
 * QCA_NL80211_VENDOR_SUBCMD_BEACON_REPORTING with
 * QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE set to
 * QCA_WLAN_VENDOR_BEACON_REPORTING_OP_START after the next connection is
 * completed.
 */
enum qca_wlan_vendor_beacon_reporting_pause_reasons {
	QCA_WLAN_VENDOR_BEACON_REPORTING_PAUSE_REASON_UNSPECIFIED = 0,
	QCA_WLAN_VENDOR_BEACON_REPORTING_PAUSE_REASON_SCAN_STARTED = 1,
	QCA_WLAN_VENDOR_BEACON_REPORTING_PAUSE_REASON_DISCONNECTED = 2,
};

/*
 * enum qca_wlan_vendor_attr_beacon_reporting_params - List of attributes used
 * in vendor sub-command QCA_NL80211_VENDOR_SUBCMD_BEACON_REPORTING.
 */
enum qca_wlan_vendor_attr_beacon_reporting_params {
	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_INVALID = 0,
	/* Specifies the type of operation that the vendor command/event is
	 * intended for. Possible values for this attribute are defined in
	 * enum qca_wlan_vendor_beacon_reporting_op_types. u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE = 1,
	/* Optionally set by userspace to request the driver to report Beacon
	 * frames using asynchronous vendor events when the
	 * QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE is set to
	 * QCA_WLAN_VENDOR_BEACON_REPORTING_OP_START. NLA_FLAG attribute.
	 * If this flag is not set, the driver will only update Beacon frames in
	 * cfg80211 scan cache but not send any vendor events.
	 */
	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_ACTIVE_REPORTING = 2,
	/* Optionally used by userspace to request the driver/firmware to report
	 * Beacon frames periodically when the
	 * QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE is set to
	 * QCA_WLAN_VENDOR_BEACON_REPORTING_OP_START.
	 * u32 attribute, indicates the period of Beacon frames to be reported
	 * and in the units of beacon interval.
	 * If this attribute is missing in the command, then the default value
	 * of 1 will be assumed by driver, i.e., to report every Beacon frame.
	 * Zero is an invalid value.
	 * If a valid value is received for this attribute, the driver will
	 * update the cfg80211 scan cache periodically as per the value received
	 * in this attribute in addition to updating the cfg80211 scan cache
	 * when there is significant change in Beacon frame IEs.
	 */
	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_PERIOD = 3,
	/* Used by the driver to encapsulate the SSID when the
	 * QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE is set to
	 * QCA_WLAN_VENDOR_BEACON_REPORTING_OP_BEACON_INFO.
	 * u8 array with a maximum size of 32.
	 *
	 * When generating beacon report from non-MBSSID Beacon frame, the SSID
	 * will be taken from the SSID element of the received Beacon frame.
	 *
	 * When generating beacon report from Multiple BSSID Beacon frame and if
	 * the BSSID of the current connected BSS matches the BSSID of the
	 * transmitting BSS, the SSID will be taken from the SSID element of the
	 * received Beacon frame.
	 *
	 * When generating beacon report from Multiple BSSID Beacon frame and if
	 * the BSSID of the current connected BSS matches the BSSID of one of
	 * the* nontransmitting BSSs, the SSID will be taken from the SSID field
	 * included in the nontransmitted BSS profile whose derived BSSID is
	 * same as the BSSID of the current connected BSS. When there is no
	 * nontransmitted BSS profile whose derived BSSID is same as the BSSID
	 * of current connected* BSS, this attribute will not be present.
	 */
	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_SSID = 4,
	/* Used by the driver to encapsulate the BSSID of the AP to which STA is
	 * currently connected to when the
	 * QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE is set to
	 * QCA_WLAN_VENDOR_BEACON_REPORTING_OP_BEACON_INFO. u8 array with a
	 * fixed size of 6 bytes.
	 *
	 * When generating beacon report from a Multiple BSSID beacon and the
	 * current connected BSSID matches one of the nontransmitted BSSIDs in a
	 * Multiple BSSID set, this BSSID will be that particular nontransmitted
	 * BSSID and not the transmitted BSSID (i.e., the transmitting address
	 * of the Beacon frame).
	 */
	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_BSSID = 5,
	/* Used by the driver to encapsulate the frequency in MHz on which
	 * the Beacon frame was received when the
	 * QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE is
	 * set to QCA_WLAN_VENDOR_BEACON_REPORTING_OP_BEACON_INFO.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_FREQ = 6,
	/* Used by the driver to encapsulate the Beacon interval
	 * when the QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE is set to
	 * QCA_WLAN_VENDOR_BEACON_REPORTING_OP_BEACON_INFO.
	 * u16 attribute. The value will be copied from the Beacon frame and the
	 * units are TUs.
	 */
	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_BI = 7,
	/* Used by the driver to encapsulate the Timestamp field from the Beacon
	 * frame when the QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE is set
	 * to QCA_WLAN_VENDOR_BEACON_REPORTING_OP_BEACON_INFO.
	 * u64 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_TSF = 8,
	/* Used by the driver to encapsulate the CLOCK_BOOTTIME when this
	 * Beacon frame is received in the driver when the
	 * QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE is set to
	 * QCA_WLAN_VENDOR_BEACON_REPORTING_OP_BEACON_INFO. u64 attribute, in
	 * the units of nanoseconds. This value is expected to have accuracy of
	 * about 10 ms.
	 */
	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_BOOTTIME_WHEN_RECEIVED = 9,
	/* Used by the driver to encapsulate the IEs of the Beacon frame from
	 * which this event is generated when the
	 * QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE is set to
	 * QCA_WLAN_VENDOR_BEACON_REPORTING_OP_BEACON_INFO. u8 array.
	 */
	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_IES = 10,
	/* Used by the driver to specify the reason for the driver/firmware to
	 * pause sending beacons to userspace when the
	 * QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE is set to
	 * QCA_WLAN_VENDOR_BEACON_REPORTING_OP_PAUSE. Possible values are
	 * defined in enum qca_wlan_vendor_beacon_reporting_pause_reasons, u32
	 * attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_PAUSE_REASON = 11,
	/* Used by the driver to specify whether the driver will automatically
	 * resume reporting beacon events to userspace later (for example after
	 * the ongoing off-channel activity is completed etc.) when the
	 * QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE is set to
	 * QCA_WLAN_VENDOR_BEACON_REPORTING_OP_PAUSE. NLA_FLAG attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_AUTO_RESUMES = 12,
	/* Optionally set by userspace to request the driver not to resume
	 * beacon reporting after a pause is completed, when the
	 * QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE is set to
	 * QCA_WLAN_VENDOR_BEACON_REPORTING_OP_START. NLA_FLAG attribute.
	 * If this flag is set, the driver will not resume beacon reporting
	 * after any pause in beacon reporting is completed. Userspace has to
	 * send QCA_WLAN_VENDOR_BEACON_REPORTING_OP_START command again in order
	 * to initiate beacon reporting again. If this flag is set in the recent
	 * QCA_WLAN_VENDOR_BEACON_REPORTING_OP_START command, then in the
	 * subsequent QCA_WLAN_VENDOR_BEACON_REPORTING_OP_PAUSE event (if any)
	 * the QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_AUTO_RESUMES shall not be
	 * set by the driver. Setting this flag until and unless there is a
	 * specific need is not recommended as there is a chance of some beacons
	 * received after pause command and next start command being not
	 * reported.
	 */
	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_DO_NOT_RESUME = 13,
	/* Attribute used for padding for 64-bit alignment */
	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_PAD = 14,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_LAST,
	QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_MAX =
		QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_LAST - 1
};

/**
 * enum qca_vendor_interop_issues_ap_type - Interop issue types
 * This enum defines the valid set of values of interop issue types. These
 * values are used by attribute %QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_TYPE.
 *
 * @QCA_VENDOR_INTEROP_ISSUES_AP_ON_STA_PS: The AP has power save interop issue
 * when the STA's Qpower feature is enabled.
 */
enum qca_vendor_interop_issues_ap_type {
	QCA_VENDOR_INTEROP_ISSUES_AP_INVALID = 0,
	QCA_VENDOR_INTEROP_ISSUES_AP_ON_STA_PS = 1,
};

/**
 * enum qca_vendor_attr_interop_issues_ap - attribute for AP with interop issues
 * Values are used by %QCA_NL80211_VENDOR_SUBCMD_INTEROP_ISSUES_AP.
 *
 * @QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_INVALID: Invalid value
 * @QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_TYPE: Interop issue type
 * 32-bit unsigned value. The values defined in enum
 * qca_vendor_interop_issues_ap_type are used.
 * @QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_LIST: APs' BSSID container
 * array of nested QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_BSSID attributes.
 * It is present and mandatory for the command but is not used for the event
 * since only a single BSSID is reported in an event.
 * @QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_BSSID: AP's BSSID 6-byte MAC address.
 * It is used within the nested QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_LIST
 * attribute in command case and without such encapsulation in the event case.
 * @QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_AFTER_LAST: last value
 * @QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_MAX: max value
 */
enum qca_vendor_attr_interop_issues_ap {
	QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_INVALID,
	QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_TYPE,
	QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_LIST,
	QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_BSSID,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_MAX =
		QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_AFTER_LAST - 1
};

/**
 * enum qca_vendor_oem_device_type - Represents the target device in firmware.
 * It is used by QCA_WLAN_VENDOR_ATTR_OEM_DEVICE_INFO.
 *
 * @QCA_VENDOR_OEM_DEVICE_VIRTUAL: The command is intended for
 * a virtual device.
 *
 * @QCA_VENDOR_OEM_DEVICE_PHYSICAL: The command is intended for
 * a physical device.
 */
enum qca_vendor_oem_device_type {
	QCA_VENDOR_OEM_DEVICE_VIRTUAL = 0,
	QCA_VENDOR_OEM_DEVICE_PHYSICAL = 1,
};

/**
 * enum qca_wlan_vendor_attr_oem_data_params - Used by the vendor command/event
 * QCA_NL80211_VENDOR_SUBCMD_OEM_DATA.
 *
 * @QCA_WLAN_VENDOR_ATTR_OEM_DATA_CMD_DATA: This NLA_BINARY attribute is
 * used to set/query the data to/from the firmware. On query, the same
 * attribute is used to carry the respective data in the reply sent by the
 * driver to userspace. The request to set/query the data and the format of the
 * respective data from the firmware are embedded in the attribute. The
 * maximum size of the attribute payload is 1024 bytes.
 * Userspace has to set the QCA_WLAN_VENDOR_ATTR_OEM_DATA_RESPONSE_EXPECTED
 * attribute when the data is queried from the firmware.
 *
 * @QCA_WLAN_VENDOR_ATTR_OEM_DEVICE_INFO: The binary blob will be routed
 * based on this field. This optional attribute is included to specify whether
 * the device type is a virtual device or a physical device for the
 * command/event. This attribute can be omitted for a virtual device (default)
 * command/event.
 * This u8 attribute is used to carry information for the device type using
 * values defined by enum qca_vendor_oem_device_type.
 *
 * @QCA_WLAN_VENDOR_ATTR_OEM_DATA_RESPONSE_EXPECTED: This NLA_FLAG attribute
 * is set when the userspace queries data from the firmware. This attribute
 * should not be set when userspace sets the OEM data to the firmware.
 */
enum qca_wlan_vendor_attr_oem_data_params {
	QCA_WLAN_VENDOR_ATTR_OEM_DATA_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_OEM_DATA_CMD_DATA = 1,
	QCA_WLAN_VENDOR_ATTR_OEM_DEVICE_INFO = 2,
	QCA_WLAN_VENDOR_ATTR_OEM_DATA_RESPONSE_EXPECTED = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_OEM_DATA_PARAMS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_OEM_DATA_PARAMS_MAX =
		QCA_WLAN_VENDOR_ATTR_OEM_DATA_PARAMS_AFTER_LAST - 1
};

/**
 * enum qca_wlan_vendor_attr_avoid_frequency_ext - Defines attributes to be
 * used with vendor command/event QCA_NL80211_VENDOR_SUBCMD_AVOID_FREQUENCY_EXT.
 *
 * @QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_RANGE: Required
 * Nested attribute containing multiple ranges with following attributes:
 *	QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_START,
 *	QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_END, and
 *	QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_POWER_CAP_DBM.
 *
 * @QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_START: Required (u32)
 * Starting center frequency in MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_END: Required (u32)
 * Ending center frequency in MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_POWER_CAP_DBM:
 * s32 attribute, optional. It is a per frequency range attribute.
 * The maximum TX power limit from user space is to be applied on an
 * unrestricted interface for corresponding frequency range. It is also
 * possible that the actual TX power may be even lower than this cap due to
 * other considerations such as regulatory compliance, SAR, etc. In absence of
 * this attribute the driver shall follow current behavior which means
 * interface (SAP/P2P) function can keep operating on an unsafe channel with TX
 * power derived by the driver based on regulatory/SAR during interface up.
 *
 * @QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_IFACES_BITMASK:
 * u32 attribute, optional. Indicates all the interface types which are
 * restricted for all frequency ranges provided in
 * %QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_START and
 * %QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_END.
 * This attribute encapsulates bitmasks of interface types defined in
 * enum nl80211_iftype. If an interface is marked as restricted the driver must
 * move to a safe channel and if no safe channel is available the driver shall
 * terminate that interface functionality. In absence of this attribute,
 * interface (SAP/P2P) can still continue operating on an unsafe channel with
 * TX power limit derived from either
 * %QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_POWER_CAP_DBM or based on
 * regulatory/SAE limits if %QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_POWER_CAP_DBM
 * is not provided.
 *
 * @QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_IFINDEX: u32 attribute, optional.
 * This specifies the interface index (netdev) for which the corresponding
 * configurations are applied. If the interface index is not specified, the
 * configurations are applied based on
 * %QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_IFACES_BITMASK.
 * %QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_IFACES_BITMASK along with this
 * attribute shall have the matching nl80211_iftype.
 */
enum qca_wlan_vendor_attr_avoid_frequency_ext {
	QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_RANGE = 1,
	QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_START = 2,
	QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_END = 3,
	QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_POWER_CAP_DBM = 4,
	QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_IFACES_BITMASK = 5,
	QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_IFINDEX = 6,

	QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_MAX =
		QCA_WLAN_VENDOR_ATTR_AVOID_FREQUENCY_AFTER_LAST - 1
};

/*
 * enum qca_wlan_vendor_attr_add_sta_node_params - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_ADD_STA_NODE.
 */
enum qca_wlan_vendor_attr_add_sta_node_params {
	QCA_WLAN_VENDOR_ATTR_ADD_STA_NODE_INVALID = 0,
	/* 6 byte MAC address of STA */
	QCA_WLAN_VENDOR_ATTR_ADD_STA_NODE_MAC_ADDR = 1,
	/* Authentication algorithm used by the station of size u16;
	 * defined in enum nl80211_auth_type.
	 */
	QCA_WLAN_VENDOR_ATTR_ADD_STA_NODE_AUTH_ALGO = 2,

	/*
	 * This flag attribute is set if the node being added is an
	 * MLD STA node.
	 */
	QCA_WLAN_VENDOR_ATTR_ADD_STA_NODE_IS_ML = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ADD_STA_NODE_PARAM_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ADD_STA_NODE_PARAM_MAX =
		QCA_WLAN_VENDOR_ATTR_ADD_STA_NODE_PARAM_AFTER_LAST - 1
};

/**
 * enum qca_btc_chain_mode - Specifies BT coex chain mode.
 * This enum defines the valid set of values of BT coex chain mode.
 * These values are used by attribute %QCA_VENDOR_ATTR_BTC_CHAIN_MODE of
 * %QCA_NL80211_VENDOR_SUBCMD_BTC_CHAIN_MODE.
 *
 * @QCA_BTC_CHAIN_SHARED: chains of BT and WLAN 2.4 GHz are shared.
 * @QCA_BTC_CHAIN_SEPARATED_HYBRID: chains of BT and WLAN 2.4 GHz are
 * separated, hybrid mode.
 * @QCA_BTC_CHAIN_SEPARATED_FDD: chains of BT and WLAN 2.4 GHz are
 * separated, fixed FDD mode.
 */
enum qca_btc_chain_mode {
	QCA_BTC_CHAIN_SHARED = 0,
	QCA_BTC_CHAIN_SEPARATED_HYBRID = 1,
	QCA_BTC_CHAIN_SEPARATED_FDD = 2,
};

/* deprecated legacy name */
#define QCA_BTC_CHAIN_SEPARATED QCA_BTC_CHAIN_SEPARATED_HYBRID

/**
 * enum qca_vendor_attr_btc_chain_mode - Specifies attributes for BT coex
 * chain mode.
 * Attributes for data used by QCA_NL80211_VENDOR_SUBCMD_BTC_CHAIN_MODE.
 *
 * @QCA_VENDOR_ATTR_COEX_BTC_CHAIN_MODE: u32 attribute.
 * Indicates the BT coex chain mode, are 32-bit values from
 * enum qca_btc_chain_mode. This attribute is mandatory.
 *
 * @QCA_VENDOR_ATTR_COEX_BTC_CHAIN_MODE_RESTART: flag attribute.
 * If set, vdev should be restarted when BT coex chain mode is updated.
 * This attribute is optional.
 */
enum qca_vendor_attr_btc_chain_mode {
	QCA_VENDOR_ATTR_BTC_CHAIN_MODE_INVALID = 0,
	QCA_VENDOR_ATTR_BTC_CHAIN_MODE = 1,
	QCA_VENDOR_ATTR_BTC_CHAIN_MODE_RESTART = 2,

	/* Keep last */
	QCA_VENDOR_ATTR_BTC_CHAIN_MODE_LAST,
	QCA_VENDOR_ATTR_BTC_CHAIN_MODE_MAX =
	QCA_VENDOR_ATTR_BTC_CHAIN_MODE_LAST - 1,
};

/**
 * enum qca_vendor_wlan_sta_flags - Station feature flags
 * Bits will be set to 1 if the corresponding features are enabled.
 * @QCA_VENDOR_WLAN_STA_FLAG_AMPDU: AMPDU is enabled for the station
 * @QCA_VENDOR_WLAN_STA_FLAG_TX_STBC: TX Space-time block coding is enabled
    for the station
 * @QCA_VENDOR_WLAN_STA_FLAG_RX_STBC: RX Space-time block coding is enabled
    for the station
 */
enum qca_vendor_wlan_sta_flags {
	QCA_VENDOR_WLAN_STA_FLAG_AMPDU = BIT(0),
	QCA_VENDOR_WLAN_STA_FLAG_TX_STBC = BIT(1),
	QCA_VENDOR_WLAN_STA_FLAG_RX_STBC = BIT(2),
};

/**
 * enum qca_vendor_wlan_sta_guard_interval - Station guard interval
 * @QCA_VENDOR_WLAN_STA_GI_800_NS: Legacy normal guard interval
 * @QCA_VENDOR_WLAN_STA_GI_400_NS: Legacy short guard interval
 * @QCA_VENDOR_WLAN_STA_GI_1600_NS: Guard interval used by HE
 * @QCA_VENDOR_WLAN_STA_GI_3200_NS: Guard interval used by HE
 */
enum qca_vendor_wlan_sta_guard_interval {
	QCA_VENDOR_WLAN_STA_GI_800_NS = 0,
	QCA_VENDOR_WLAN_STA_GI_400_NS = 1,
	QCA_VENDOR_WLAN_STA_GI_1600_NS = 2,
	QCA_VENDOR_WLAN_STA_GI_3200_NS = 3,
};

/**
 * enum qca_wlan_vendor_attr_get_sta_info - Defines attributes
 * used by QCA_NL80211_VENDOR_SUBCMD_GET_STA_INFO vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAC:
 * Required attribute in request for AP mode only, 6-byte MAC address,
 * corresponding to the station's MAC address for which information is
 * requested. For STA mode this is not required as the info always correspond
 * to the self STA and the current/last association.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_FLAGS:
 * Optionally used in response, u32 attribute, contains a bitmap of different
 * fields defined in enum qca_vendor_wlan_sta_flags, used in AP mode only.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_GUARD_INTERVAL:
 * Optionally used in response, u32 attribute, possible values are defined in
 * enum qca_vendor_wlan_sta_guard_interval, used in AP mode only.
 * Guard interval used by the station.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_RETRY_COUNT:
 * Optionally used in response, u32 attribute, used in AP mode only.
 * Value indicates the number of data frames received from station with retry
 * bit set to 1 in FC.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_BC_MC_COUNT:
 * Optionally used in response, u32 attribute, used in AP mode only.
 * Counter for number of data frames with broadcast or multicast address in
 * the destination address received from the station.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RETRY_SUCCEED:
 * Optionally used in response, u32 attribute, used in both STA and AP modes.
 * Value indicates the number of data frames successfully transmitted only
 * after retrying the packets and for which the TX status has been updated
 * back to host from target.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RETRY_EXHAUSTED:
 * Optionally used in response, u32 attribute, used in both STA and AP mode.
 * Value indicates the number of data frames not transmitted successfully even
 * after retrying the packets for the number of times equal to the total number
 * of retries allowed for that packet and for which the TX status has been
 * updated back to host from target.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_TOTAL:
 * Optionally used in response, u32 attribute, used in AP mode only.
 * Counter in the target for the number of data frames successfully transmitted
 * to the station.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_RETRY:
 * Optionally used in response, u32 attribute, used in AP mode only.
 * Value indicates the number of data frames successfully transmitted only
 * after retrying the packets.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_RETRY_EXHAUSTED:
 * Optionally used in response, u32 attribute, used in both STA & AP mode.
 * Value indicates the number of data frames not transmitted successfully even
 * after retrying the packets for the number of times equal to the total number
 * of retries allowed for that packet.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_PROBE_REQ_BMISS_COUNT: u32, used in
 * the STA mode only. Represent the number of probe requests sent by the STA
 * while attempting to roam on missing certain number of beacons from the
 * connected AP. If queried in the disconnected state, this represents the
 * count for the last connected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_PROBE_RESP_BMISS_COUNT: u32, used in
 * the STA mode. Represent the number of probe responses received by the station
 * while attempting to roam on missing certain number of beacons from the
 * connected AP. When queried in the disconnected state, this represents the
 * count when in last connected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_ALL_COUNT: u32, used in the
 * STA mode only. Represents the total number of frames sent out by STA
 * including Data, ACK, RTS, CTS, Control Management. This data is maintained
 * only for the connect session. Represents the count of last connected session,
 * when queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RTS_COUNT: u32, used in the STA mode.
 * Total number of RTS sent out by the STA. This data is maintained per connect
 * session. Represents the count of last connected session, when queried in the
 * disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RTS_RETRY_FAIL_COUNT: u32, used in the
 * STA mode.Represent the number of RTS transmission failure that reach retry
 * limit. This data is maintained per connect session. Represents the count of
 * last connected session, when queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_DATA_NON_AGGREGATED_COUNT: u32, used in
 * the STA mode. Represent the total number of non aggregated frames transmitted
 * by the STA. This data is maintained per connect session. Represents the count
 * of last connected session, when queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_DATA_AGGREGATED_COUNT: u32, used in the
 * STA mode. Represent the total number of aggregated frames transmitted by the
 * STA. This data is maintained per connect session. Represents the count of
 * last connected session, when queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_FRAMES_GOOD_PLCP_COUNT: u32, used in
 * the STA mode. Represents the number of received frames with a good PLCP. This
 * data is maintained per connect session. Represents the count of last
 * connected session, when queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_FRAMES_INVALID_DELIMITER_COUNT: u32,
 * used in the STA mode. Represents the number of occasions that no valid
 * delimiter is detected by A-MPDU parser. This data is maintained per connect
 * session. Represents the count of last connected session, when queried in the
 * disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_FRAMES_CRC_FAIL_COUNT: u32, used in the
 * STA mode. Represents the number of frames for which CRC check failed in the
 * MAC. This data is maintained per connect session. Represents the count of
 * last connected session, when queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_ACKS_GOOD_FCS_COUNT: u32, used in the
 * STA mode. Represents the number of unicast ACKs received with good FCS. This
 * data is maintained per connect session. Represents the count of last
 * connected session, when queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_BLOCKACK_COUNT: u32, used in the STA
 * mode. Represents the number of received Block Acks. This data is maintained
 * per connect session. Represents the count of last connected session, when
 * queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_BEACON_COUNT: u32, used in the STA
 * mode. Represents the number of beacons received from the connected BSS. This
 * data is maintained per connect session. Represents the count of last
 * connected session, when queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_OTHER_BEACON_COUNT: u32, used in the
 * STA mode. Represents the number of beacons received by the other BSS when in
 * connected state (through the probes done by the STA). This data is maintained
 * per connect session. Represents the count of last connected session, when
 * queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_UCAST_DATA_GOOD_FCS_COUNT: u64, used in
 * the STA mode. Represents the number of received DATA frames with good FCS and
 * matching Receiver Address when in connected state. This data is maintained
 * per connect session. Represents the count of last connected session, when
 * queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_DATA_BC_MC_DROP_COUNT: u32, used in the
 * STA mode. Represents the number of RX Data multicast frames dropped by the HW
 * when in the connected state. This data is maintained per connect session.
 * Represents the count of last connected session, when queried in the
 * disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_24G_1MBPS: u32, used in the
 * STA mode. This represents the target power in dBm for the transmissions done
 * to the AP in 2.4 GHz at 1 Mbps (DSSS) rate. This data is maintained per
 * connect session. Represents the count of last connected session, when
 * queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_24G_6MBPS: u32, used in the
 * STA mode. This represents the Target power in dBm for transmissions done to
 * the AP in 2.4 GHz at 6 Mbps (OFDM) rate. This data is maintained per connect
 * session. Represents the count of last connected session, when queried in the
 * disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_24G_MCS0: u32, used in the
 * STA mode. This represents the Target power in dBm for transmissions done to
 * the AP in 2.4 GHz at MCS0 rate. This data is maintained per connect session.
 * Represents the count of last connected session, when queried in the
 * disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_5G_6MBPS: u32, used in the
 * STA mode. This represents the Target power in dBm for transmissions done to
 * the AP in 5 GHz at 6 Mbps (OFDM) rate. This data is maintained per connect
 * session. Represents the count of last connected session, when queried in
 * the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_5G_MCS0: u32, used in the
 * STA mode. This represents the Target power in dBm for transmissions done
 * to the AP in 5 GHz at MCS0 rate. This data is maintained per connect session.
 * Represents the count of last connected session, when queried in the
 * disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_HW_BUFFERS_OVERFLOW_COUNT: u32, used
 * in the STA mode. This represents the Nested attribute representing the
 * overflow counts of each receive buffer allocated to the hardware during the
 * STA's connection. The number of hw buffers might vary for each WLAN
 * solution and hence this attribute represents the nested array of all such
 * HW buffer count. This data is maintained per connect session. Represents
 * the count of last connected session, when queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAX_TX_POWER: u32, Max TX power (dBm)
 * allowed as per the regulatory requirements for the current or last connected
 * session. Used in the STA mode.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LATEST_TX_POWER: u32, Latest TX power
 * (dBm) used by the station in its latest unicast frame while communicating
 * to the AP in the connected state. When queried in the disconnected state,
 * this represents the TX power used by the STA with last AP communication
 * when in connected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ANI_LEVEL: u32, Adaptive noise immunity
 * level used to adjust the RX sensitivity. Represents the current ANI level
 * when queried in the connected state. When queried in the disconnected
 * state, this corresponds to the latest ANI level at the instance of
 * disconnection.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_IES: Binary attribute containing
 * the raw information elements from Beacon frames. Represents the Beacon frames
 * of the current BSS in the connected state. When queried in the disconnected
 * state, these IEs correspond to the last connected BSSID.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PROBE_RESP_IES: Binary attribute
 * containing the raw information elements from Probe Response frames.
 * Represents the Probe Response frames of the current BSS in the connected
 * state. When queried in the disconnected state, these IEs correspond to the
 * last connected BSSID.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_DRIVER_DISCONNECT_REASON: u32, Driver
 * disconnect reason for the last disconnection if the disconnection is
 * triggered from the host driver. The values are referred from
 * enum qca_disconnect_reason_codes.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BIP_MIC_ERROR_COUNT: u32, used in STA mode
 * only. This represents the number of group addressed robust management frames
 * received from this station with an invalid MIC or a missing MME when PMF is
 * enabled.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BIP_REPLAY_COUNT: u32, used in STA mode
 * only. This represents the number of group addressed robust management frames
 * received from this station with the packet number less than or equal to the
 * last received packet number when PMF is enabled.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_MIC_ERROR_COUNT: u32, used in STA
 * mode only. This represents the number of Beacon frames received from this
 * station with an invalid MIC or a missing MME when beacon protection is
 * enabled.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_REPLAY_COUNT: u32, used in STA mode
 * only. This represents number of Beacon frames received from this station with
 * the packet number less than or equal to the last received packet number when
 * beacon protection is enabled.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_CONNECT_FAIL_REASON_CODE: u32, used in
 * STA mode only. The driver uses this attribute to populate the connection
 * failure reason codes and the values are defined in
 * enum qca_sta_connect_fail_reason_codes. Userspace applications can send
 * QCA_NL80211_VENDOR_SUBCMD_GET_STA_INFO vendor command after receiving
 * a connection failure indication from the driver. The driver shall not
 * include this attribute in response to the
 * QCA_NL80211_VENDOR_SUBCMD_GET_STA_INFO command if there is no connection
 * failure observed in the last attempted connection.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LATEST_TX_RATE: u32, latest TX rate (Kbps)
 * used by the station in its last TX frame while communicating to the AP in the
 * connected state. When queried in the disconnected state, this represents the
 * rate used by the STA in the last TX frame to the AP when it was connected.
 * This attribute is used for STA mode only.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LATEST_RIX: u32, used in STA mode only.
 * This represents the rate index used by the STA for the last TX frame to the
 * AP. When queried in the disconnected state, this gives the last RIX used by
 * the STA in the last TX frame to the AP when it was connected.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TSF_OUT_OF_SYNC_COUNT: u32, used in STA
 * mode only. This represents the number of times the STA TSF goes out of sync
 * from the AP after the connection. If queried in the disconnected state, this
 * gives the count of TSF out of sync for the last connection.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_TRIGGER_REASON: u32, used in STA
 * mode only. This represents the roam trigger reason for the last roaming
 * attempted by the firmware. This can be queried either in connected state or
 * disconnected state. Each bit of this attribute represents the different
 * roam trigger reason code which are defined in enum qca_vendor_roam_triggers.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_FAIL_REASON: u32, used in STA mode
 * only. This represents the roam fail reason for the last failed roaming
 * attempt by the firmware. Different roam failure reason codes are specified
 * in enum qca_vendor_roam_fail_reasons. This can be queried either in
 * connected state or disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_INVOKE_FAIL_REASON: u32, used in
 * STA mode only. This represents the roam invoke fail reason for the last
 * failed roam invoke. Different roam invoke failure reason codes
 * are specified in enum qca_vendor_roam_invoke_fail_reasons. This can be
 * queried either in connected state or disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_UPLINK_DELAY: u32, used in STA mode only.
 * This represents the average congestion duration of uplink frames in MAC
 * queue in unit of ms. This can be queried either in connected state or
 * disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PER_MCS_TX_PACKETS: Array of u32 nested
 * values, used in AP mode. This represents the MPDU packet count per MCS
 * rate value of TX packets. Every index of this nested attribute corresponds
 * to MCS index, e.g., Index 0 represents MCS0 TX rate. This can be
 * queried in connected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PER_MCS_RX_PACKETS: Array of u32 nested
 * values, used in AP mode. This represents the MPDU packet count per MCS
 * rate value of RX packets. Every index of this nested attribute corresponds
 * to MCS index, e.g., Index 0 represents MCS0 RX rate. This can be
 * queried in connected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PAD: Attribute used for padding for
 * 64-bit alignment.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_UPLINK_DELAY_JITTER: u32, used in STA mode
 * only. This represents the average of the delta between successive uplink
 * frames congestion duration in MAC queue in unit of ms. This can be queried
 * either in connected state or disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_NSS_PKT_COUNT: Array of nested attributes,
 * used in STA mode. This represents the number of MSDU packets
 * (unicast/multicast/broadcast) transmitted/received with each NSS value. See
 * enum qca_wlan_vendor_attr_nss_pkt.
 */
enum qca_wlan_vendor_attr_get_sta_info {
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAC = 1,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_FLAGS = 2,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_GUARD_INTERVAL = 3,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_RETRY_COUNT = 4,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_BC_MC_COUNT = 5,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RETRY_SUCCEED = 6,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RETRY_EXHAUSTED = 7,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_TOTAL = 8,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_RETRY = 9,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_RETRY_EXHAUSTED = 10,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_PROBE_REQ_BMISS_COUNT = 11,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_PROBE_RESP_BMISS_COUNT = 12,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_ALL_COUNT = 13,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RTS_COUNT = 14,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RTS_RETRY_FAIL_COUNT = 15,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_DATA_NON_AGGREGATED_COUNT = 16,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_DATA_AGGREGATED_COUNT = 17,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_FRAMES_GOOD_PLCP_COUNT = 18,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_FRAMES_INVALID_DELIMITER_COUNT = 19,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_FRAMES_CRC_FAIL_COUNT = 20,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_ACKS_GOOD_FCS_COUNT = 21,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_BLOCKACK_COUNT = 22,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_BEACON_COUNT = 23,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_OTHER_BEACON_COUNT = 24,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_UCAST_DATA_GOOD_FCS_COUNT = 25,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_DATA_BC_MC_DROP_COUNT = 26,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_24G_1MBPS = 27,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_24G_6MBPS = 28,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_24G_MCS0 = 29,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_5G_6MBPS = 30,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_5G_MCS0 = 31,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_HW_BUFFERS_OVERFLOW_COUNT = 32,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAX_TX_POWER = 33,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LATEST_TX_POWER = 34,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ANI_LEVEL = 35,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_IES = 36,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PROBE_RESP_IES = 37,
	QCA_WLAN_VENDOR_ATTR_GET_STA_DRIVER_DISCONNECT_REASON = 38,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BIP_MIC_ERROR_COUNT = 39,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BIP_REPLAY_COUNT = 40,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_MIC_ERROR_COUNT = 41,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_REPLAY_COUNT = 42,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_CONNECT_FAIL_REASON_CODE = 43,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LATEST_TX_RATE = 44,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LATEST_RIX = 45,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TSF_OUT_OF_SYNC_COUNT = 46,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_TRIGGER_REASON = 47,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_FAIL_REASON = 48,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_INVOKE_FAIL_REASON = 49,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_UPLINK_DELAY = 50,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PER_MCS_TX_PACKETS = 51,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PER_MCS_RX_PACKETS = 52,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PAD = 53,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_UPLINK_DELAY_JITTER = 54,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_NSS_PKT_COUNT = 55,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_update_sta_info - Defines attributes
 * used by QCA_NL80211_VENDOR_SUBCMD_UPDATE_STA_INFO vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_UPDATE_STA_INFO_CONNECT_CHANNELS: Type is NLA_UNSPEC.
 * Used in STA mode. This attribute represents the list of channel center
 * frequencies in MHz (u32) the station has learnt during the last connection
 * or roaming attempt. This information shall not signify the channels for
 * an explicit scan request from the user space. Host drivers can update this
 * information to the user space in both connected and disconnected state.
 * In the disconnected state this information shall signify the channels
 * scanned in the last connection/roam attempt that lead to the disconnection.
 */
enum qca_wlan_vendor_attr_update_sta_info {
	QCA_WLAN_VENDOR_ATTR_UPDATE_STA_INFO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_UPDATE_STA_INFO_CONNECT_CHANNELS = 1,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_UPDATE_STA_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_UPDATE_STA_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_UPDATE_STA_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_disconnect_reason_codes - Specifies driver disconnect reason codes.
 * Used when the driver triggers the STA to disconnect from the AP.
 *
 * @QCA_DISCONNECT_REASON_UNSPECIFIED: The host driver triggered the
 * disconnection with the AP due to unspecified reasons.
 *
 * @QCA_DISCONNECT_REASON_INTERNAL_ROAM_FAILURE: The host driver triggered the
 * disconnection with the AP due to a roaming failure. This roaming is triggered
 * internally (host driver/firmware).
 *
 * @QCA_DISCONNECT_REASON_EXTERNAL_ROAM_FAILURE: The driver disconnected from
 * the AP when the user/external triggered roaming fails.
 *
 * @QCA_DISCONNECT_REASON_GATEWAY_REACHABILITY_FAILURE: This reason code is used
 * by the host driver whenever gateway reachability failure is detected and the
 * driver disconnects with AP.
 *
 * @QCA_DISCONNECT_REASON_UNSUPPORTED_CHANNEL_CSA: The driver disconnected from
 * the AP on a channel switch announcement from it with an unsupported channel.
 *
 * @QCA_DISCONNECT_REASON_OPER_CHANNEL_DISABLED_INDOOR: On a concurrent AP start
 * with indoor channels disabled and if the STA is connected on one of these
 * disabled channels, the host driver disconnected the STA with this reason
 * code.
 *
 * @QCA_DISCONNECT_REASON_OPER_CHANNEL_USER_DISABLED: Disconnection due to an
 * explicit request from the user to disable the current operating channel.
 *
 * @QCA_DISCONNECT_REASON_DEVICE_RECOVERY: STA disconnected from the AP due to
 * the internal host driver/firmware recovery.
 *
 * @QCA_DISCONNECT_REASON_KEY_TIMEOUT: The driver triggered the disconnection on
 * a timeout for the key installations from the user space.
 *
 * @QCA_DISCONNECT_REASON_OPER_CHANNEL_BAND_CHANGE: The dDriver disconnected the
 * STA on a band change request from the user space to a different band from the
 * current operation channel/band.
 *
 * @QCA_DISCONNECT_REASON_IFACE_DOWN: The STA disconnected from the AP on an
 * interface down trigger from the user space.
 *
 * @QCA_DISCONNECT_REASON_PEER_XRETRY_FAIL: The host driver disconnected the
 * STA on getting continuous transmission failures for multiple Data frames.
 *
 * @QCA_DISCONNECT_REASON_PEER_INACTIVITY: The STA does a keep alive
 * notification to the AP by transmitting NULL/G-ARP frames. This disconnection
 * represents inactivity from AP on such transmissions.

 * @QCA_DISCONNECT_REASON_SA_QUERY_TIMEOUT: This reason code is used on
 * disconnection when SA Query times out (AP does not respond to SA Query).
 *
 * @QCA_DISCONNECT_REASON_BEACON_MISS_FAILURE: The host driver disconnected the
 * STA on missing the beacons continuously from the AP.
 *
 * @QCA_DISCONNECT_REASON_CHANNEL_SWITCH_FAILURE: Disconnection due to STA not
 * able to move to the channel mentioned by the AP in CSA.
 *
 * @QCA_DISCONNECT_REASON_USER_TRIGGERED: User triggered disconnection.
 */
enum qca_disconnect_reason_codes {
	QCA_DISCONNECT_REASON_UNSPECIFIED = 0,
	QCA_DISCONNECT_REASON_INTERNAL_ROAM_FAILURE = 1,
	QCA_DISCONNECT_REASON_EXTERNAL_ROAM_FAILURE = 2,
	QCA_DISCONNECT_REASON_GATEWAY_REACHABILITY_FAILURE = 3,
	QCA_DISCONNECT_REASON_UNSUPPORTED_CHANNEL_CSA = 4,
	QCA_DISCONNECT_REASON_OPER_CHANNEL_DISABLED_INDOOR = 5,
	QCA_DISCONNECT_REASON_OPER_CHANNEL_USER_DISABLED = 6,
	QCA_DISCONNECT_REASON_DEVICE_RECOVERY = 7,
	QCA_DISCONNECT_REASON_KEY_TIMEOUT = 8,
	QCA_DISCONNECT_REASON_OPER_CHANNEL_BAND_CHANGE = 9,
	QCA_DISCONNECT_REASON_IFACE_DOWN = 10,
	QCA_DISCONNECT_REASON_PEER_XRETRY_FAIL = 11,
	QCA_DISCONNECT_REASON_PEER_INACTIVITY = 12,
	QCA_DISCONNECT_REASON_SA_QUERY_TIMEOUT = 13,
	QCA_DISCONNECT_REASON_BEACON_MISS_FAILURE = 14,
	QCA_DISCONNECT_REASON_CHANNEL_SWITCH_FAILURE = 15,
	QCA_DISCONNECT_REASON_USER_TRIGGERED = 16,
};

/**
 * enum qca_wlan_vendor_attr_driver_disconnect_reason - Defines attributes
 * used by %QCA_NL80211_VENDOR_SUBCMD_DRIVER_DISCONNECT_REASON vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_DRIVER_DISCONNECT_REASCON_CODE: u32 attribute.
 * This attribute represents the driver specific reason codes (local
 * driver/firmware initiated reasons for disconnection) defined
 * in enum qca_disconnect_reason_codes.
 */
enum qca_wlan_vendor_attr_driver_disconnect_reason {
	QCA_WLAN_VENDOR_ATTR_DRIVER_DISCONNECT_REASON_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_DRIVER_DISCONNECT_REASCON_CODE = 1,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_DRIVER_DISCONNECT_REASON_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_DRIVER_DISCONNECT_REASON_MAX =
	QCA_WLAN_VENDOR_ATTR_DRIVER_DISCONNECT_REASON_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_tspec_operation - Operation of the config TSPEC request
 *
 * Values for %QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_OPERATION.
 */
enum qca_wlan_tspec_operation {
	QCA_WLAN_TSPEC_ADD = 0,
	QCA_WLAN_TSPEC_DEL = 1,
	QCA_WLAN_TSPEC_GET = 2,
};

/**
 * enum qca_wlan_tspec_direction - Direction in TSPEC
 * As what is defined in IEEE Std 802.11-2016, Table 9-139.
 *
 * Values for %QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_DIRECTION.
 */
enum qca_wlan_tspec_direction {
	QCA_WLAN_TSPEC_DIRECTION_UPLINK = 0,
	QCA_WLAN_TSPEC_DIRECTION_DOWNLINK = 1,
	QCA_WLAN_TSPEC_DIRECTION_DIRECT = 2,
	QCA_WLAN_TSPEC_DIRECTION_BOTH = 3,
};

/**
 * enum qca_wlan_tspec_ack_policy - MAC acknowledgment policy in TSPEC
 * As what is defined in IEEE Std 802.11-2016, Table 9-141.
 *
 * Values for %QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_ACK_POLICY.
 */
enum qca_wlan_tspec_ack_policy {
	QCA_WLAN_TSPEC_NORMAL_ACK = 0,
	QCA_WLAN_TSPEC_NO_ACK = 1,
	/* Reserved */
	QCA_WLAN_TSPEC_BLOCK_ACK = 3,
};

/**
 * enum qca_wlan_vendor_attr_config_tspec - Defines attributes
 * used by %QCA_NL80211_VENDOR_SUBCMD_CONFIG_TSPEC vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_OPERATION:
 * u8 attribute. Specify the TSPEC operation of this request. Possible values
 * are defined in enum qca_wlan_tspec_operation.
 * Mandatory attribute for all kinds of config TSPEC requests.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_TSID:
 * u8 attribute. TS ID. Possible values are 0-7.
 * Applicable for operation: QCA_WLAN_TSPEC_ADD, QCA_WLAN_TSPEC_DEL,
 * QCA_WLAN_TSPEC_GET. A mandatory attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_DIRECTION:
 * u8 attribute. Direction of data carried by the TS. Possible values are
 * defined in enum qca_wlan_tspec_direction.
 * Applicable for operation: QCA_WLAN_TSPEC_ADD. A mandatory attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_APSD:
 * Flag attribute. Indicate whether APSD is enabled for the traffic associated
 * with the TS. set - enabled, not set - disabled.
 * Applicable for operation: QCA_WLAN_TSPEC_ADD. A mandatory attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_USER_PRIORITY:
 * u8 attribute. User priority to be used for the transport of MSDUs/A-MSDUs
 * belonging to this TS. Possible values are 0-7.
 * Applicable for operation: QCA_WLAN_TSPEC_ADD. An optional attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_ACK_POLICY:
 * u8 attribute. Indicate whether MAC acknowledgements are required for
 * MPDUs/A-MSDUs belonging to this TS and the form of those acknowledgements.
 * Possible values are defined in enum qca_wlan_tspec_ack_policy.
 * Applicable for operation: QCA_WLAN_TSPEC_ADD. A mandatory attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_NOMINAL_MSDU_SIZE:
 * u16 attribute. Specify the nominal size in bytes of MSDUs/A-MSDUs
 * belonging to this TS.
 * Applicable for operation: QCA_WLAN_TSPEC_ADD. A mandatory attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_MAXIMUM_MSDU_SIZE:
 * u16 attribute. Specify the maximum size in bytes of MSDUs/A-MSDUs
 * belonging to this TS.
 * Applicable for operation: QCA_WLAN_TSPEC_ADD. A mandatory attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_MIN_SERVICE_INTERVAL:
 * u32 attribute. Specify the minimum interval in microseconds between the
 * start of two successive SPs.
 * Applicable for operation: QCA_WLAN_TSPEC_ADD. A mandatory attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_MAX_SERVICE_INTERVAL:
 * u32 attribute. Specify the maximum interval in microseconds between the
 * start of two successive SPs.
 * Applicable for operation: QCA_WLAN_TSPEC_ADD. A mandatory attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_INACTIVITY_INTERVAL:
 * u32 attribute. Specify the minimum interval in microseconds that can elapse
 * without arrival or transfer of an MPDU belonging to the TS before this TS
 * is deleted by the MAC entity at the HC.
 * Applicable for operation: QCA_WLAN_TSPEC_ADD. A mandatory attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_SUSPENSION_INTERVAL:
 * u32 attribute. Specify the minimum interval in microseconds that can elapse
 * without arrival or transfer of an MSDU belonging to the TS before the
 * generation of successive QoS(+)CF-Poll is stopped for this TS. A value of
 * 0xFFFFFFFF disables the suspension interval. The value of the suspension
 * interval is always less than or equal to the inactivity interval.
 * Applicable for operation: QCA_WLAN_TSPEC_ADD. A mandatory attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_MINIMUM_DATA_RATE:
 * u32 attribute. Indicate the lowest data rate in bps specified at the MAC
 * SAP for transport of MSDUs or A-MSDUs belonging to this TS within the
 * bounds of this TSPEC.
 * Applicable for operation: QCA_WLAN_TSPEC_ADD. An optional attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_MEAN_DATA_RATE:
 * u32 attribute. Indicate the average data rate in bps specified at the MAC
 * SAP for transport of MSDUs or A-MSDUs belonging to this TS within the
 * bounds of this TSPEC.
 * Applicable for operation: QCA_WLAN_TSPEC_ADD. An optional attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_PEAK_DATA_RATE:
 * u32 attribute. Indicate the maximum allowable data rate in bps specified at
 * the MAC SAP for transport of MSDUs or A-MSDUs belonging to this TS within
 * the bounds of this TSPEC.
 * Applicable for operation: QCA_WLAN_TSPEC_ADD. An optional attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_BURST_SIZE:
 * u32 attribute. Specify the maximum burst size in bytes of the MSDUs/A-MSDUs
 * belonging to this TS that arrive at the MAC SAP at the peak data rate. A
 * value of 0 indicates that there are no bursts.
 * Applicable for operation: QCA_WLAN_TSPEC_ADD. An optional attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_MINIMUM_PHY_RATE:
 * u32 attribute. Indicate the minimum PHY rate in bps for transport of
 * MSDUs/A-MSDUs belonging to this TS within the bounds of this TSPEC.
 * Applicable for operation: QCA_WLAN_TSPEC_ADD. An optional attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_SURPLUS_BANDWIDTH_ALLOWANCE:
 * u16 attribute. Specify the excess allocation of time (and bandwidth) over
 * and above the stated application rates required to transport an MSDU/A-MSDU
 * belonging to the TS in this TSPEC.
 * Applicable for operation: QCA_WLAN_TSPEC_ADD. A mandatory attribute.
 */
enum qca_wlan_vendor_attr_config_tspec {
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_OPERATION = 1,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_TSID = 2,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_DIRECTION = 3,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_APSD = 4,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_USER_PRIORITY = 5,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_ACK_POLICY = 6,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_NOMINAL_MSDU_SIZE = 7,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_MAXIMUM_MSDU_SIZE = 8,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_MIN_SERVICE_INTERVAL = 9,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_MAX_SERVICE_INTERVAL = 10,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_INACTIVITY_INTERVAL = 11,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_SUSPENSION_INTERVAL = 12,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_MINIMUM_DATA_RATE = 13,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_MEAN_DATA_RATE = 14,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_PEAK_DATA_RATE = 15,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_BURST_SIZE = 16,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_MINIMUM_PHY_RATE = 17,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_SURPLUS_BANDWIDTH_ALLOWANCE = 18,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_MAX =
	QCA_WLAN_VENDOR_ATTR_CONFIG_TSPEC_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_oci_override_frame_type - OCI override frame type
 * @QCA_WLAN_VENDOR_OCI_OVERRIDE_FRAME_SA_QUERY_REQ: SA Query Request frame
 * @QCA_WLAN_VENDOR_OCI_OVERRIDE_FRAME_SA_QUERY_RESP: SA Query Response frame
 * @QCA_WLAN_VENDOR_OCI_OVERRIDE_FRAME_FT_REASSOC_REQ: FT Reassociation Request
 * frame
 * @QCA_WLAN_VENDOR_OCI_OVERRIDE_FRAME_FILS_REASSOC_REQ: FILS Reassociation
 * Request frame.
 */
enum qca_wlan_vendor_oci_override_frame_type {
	QCA_WLAN_VENDOR_OCI_OVERRIDE_FRAME_SA_QUERY_REQ = 1,
	QCA_WLAN_VENDOR_OCI_OVERRIDE_FRAME_SA_QUERY_RESP = 2,
	QCA_WLAN_VENDOR_OCI_OVERRIDE_FRAME_FT_REASSOC_REQ = 3,
	QCA_WLAN_VENDOR_OCI_OVERRIDE_FRAME_FILS_REASSOC_REQ = 4,
};

/**
 * enum qca_wlan_vendor_attr_oci_override: Represents attributes for
 * OCI override request. These attributes are used inside nested attribute
 * %QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_OCI_OVERRIDE in QCA vendor command
 * %QCA_NL80211_VENDOR_SUBCMD_WIFI_TEST_CONFIGURATION.
 *
 * @QCA_WLAN_VENDOR_ATTR_OCI_OVERRIDE_FRAME_TYPE: Required attribute, u8.
 * Values from enum qca_wlan_vendor_oci_override_frame_type used in this
 * attribute to specify the frame type in which the OCI is to be overridden.
 *
 * @QCA_WLAN_VENDOR_ATTR_OCI_OVERRIDE_FREQUENCY: Required (u32)
 * OCI frequency (in MHz) to override in the specified frame type.
 */
enum qca_wlan_vendor_attr_oci_override {
	QCA_WLAN_VENDOR_ATTR_OCI_OVERRIDE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_OCI_OVERRIDE_FRAME_TYPE = 1,
	QCA_WLAN_VENDOR_ATTR_OCI_OVERRIDE_FREQUENCY = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_OCI_OVERRIDE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_OCI_OVERRIDE_MAX =
	QCA_WLAN_VENDOR_ATTR_OCI_OVERRIDE_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_medium_assess_type - Type of medium assess request
 *
 * Values for %QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_TYPE.
 */
enum qca_wlan_medium_assess_type {
	QCA_WLAN_MEDIUM_ASSESS_CCA = 0,
	QCA_WLAN_MEDIUM_ASSESS_CONGESTION_REPORT = 1,
};

/**
 * enum qca_wlan_vendor_attr_medium_assess - Attributes used by
 * %QCA_NL80211_VENDOR_SUBCMD_MEDIUM_ASSESS vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_TYPE:
 * u8 attribute. Mandatory in all kinds of medium assess requests/responses.
 * Specify the type of medium assess request and indicate its type in response.
 * Possible values are defined in enum qca_wlan_medium_assess_type.
 *
 * @QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_PERIOD:
 * u32 attribute. Mandatory in CCA request.
 * Specify the assessment period in terms of seconds. Assessment result will be
 * sent as the response to the CCA request after the assessment period.
 *
 * @QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_TOTAL_CYCLE_COUNT:
 * u32 attribute. Mandatory in response to CCA request.
 * Total timer tick count of the assessment cycle.
 *
 * @QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_IDLE_COUNT:
 * u32 attribute. Mandatory in response to CCA request.
 * Timer tick count of idle time in the assessment cycle.
 *
 * @QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_IBSS_RX_COUNT:
 * u32 attribute. Mandatory in response to CCA request.
 * Timer tick count of Intra BSS traffic RX time in the assessment cycle.
 *
 * @QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_OBSS_RX_COUNT:
 * u32 attribute. Mandatory in response to CCA request.
 * Timer tick count of Overlapping BSS traffic RX time in the assessment cycle.
 *
 * @QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_MAX_IBSS_RSSI:
 * s32 attribute. Mandatory in response to CCA request.
 * Maximum RSSI of Intra BSS traffic in the assessment cycle.
 *
 * @QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_MIN_IBSS_RSSI:
 * s32 attribute. Mandatory in response to CCA request.
 * Minimum RSSI of Intra BSS traffic in the assessment cycle.
 *
 * @QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_CONGESTION_REPORT_ENABLE:
 * u8 attribute. Mandatory in congestion report request.
 * 1-enable 0-disable.
 *
 * @QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_CONGESTION_REPORT_THRESHOLD:
 * u8 attribute. Mandatory in congestion report enable request and will be
 * ignored if present in congestion report disable request. Possible values are
 * 0-100. A vendor event QCA_NL80211_VENDOR_SUBCMD_MEDIUM_ASSESS with the type
 * QCA_WLAN_MEDIUM_ASSESS_CONGESTION_REPORT will be sent to userspace if
 * congestion percentage reaches the configured threshold.
 *
 * @QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_CONGESTION_REPORT_INTERVAL:
 * u8 attribute. Optional in congestion report enable request and will be
 * ignored if present in congestion report disable request.
 * Specify the interval of congestion report event in terms of seconds. Possible
 * values are 1-255. Default value 1 will be used if this attribute is omitted
 * or using invalid values.
 *
 * @QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_CONGESTION_PERCENTAGE:
 * u8 attribute. Mandatory in congestion report event.
 * Indicate the actual congestion percentage. Possible values are 0-100.
 */
enum qca_wlan_vendor_attr_medium_assess {
	QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_TYPE = 1,

	QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_PERIOD = 2,
	QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_TOTAL_CYCLE_COUNT = 3,
	QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_IDLE_COUNT = 4,
	QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_IBSS_RX_COUNT = 5,
	QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_OBSS_RX_COUNT = 6,
	QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_MAX_IBSS_RSSI = 7,
	QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_MIN_IBSS_RSSI = 8,

	QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_CONGESTION_REPORT_ENABLE = 9,
	QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_CONGESTION_REPORT_THRESHOLD = 10,
	QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_CONGESTION_REPORT_INTERVAL = 11,
	QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_CONGESTION_PERCENTAGE = 12,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_MAX =
	QCA_WLAN_VENDOR_ATTR_MEDIUM_ASSESS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_mbssid_tx_vdev_status - Defines attributes
 * used by QCA_NL80211_VENDOR_SUBCMD_MBSSID_TX_VDEV_STATUS vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_STATUS_VAL:
 * u8 attribute. Notify the TX VDEV status. Possible values 0, 1
 * belonging to MBSSID/EMA_AP configuration. 0 means Non-Tx VDEV,
 * 1 means Tx VDEV. Mandatory attribute for all MBSSID VDEV status events.
 *
 * @QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_EVENT:
 * u8 attribute, required. 1 means Tx VDEV up event. 0 means Tx VDEV down event.
 *
 * @QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_GROUP_ID:
 * u8 attribute, required. Indicates group id of Tx VDEV.
 *
 * @QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_GROUP_INFO:
 * Nested attribute. This attribute shall be used by the driver to send
 * group information. The attributes defined in enum
 * qca_wlan_vendor_attr_mbssid_tx_vdev_group_info
 * are nested in this attribute.
 */
enum qca_wlan_vendor_attr_mbssid_tx_vdev_status {
	QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_STATUS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_STATUS_VAL = 1,
	QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_EVENT = 2,
	QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_GROUP_ID = 3,
	QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_GROUP_INFO = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_STATUS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_STATUS_MAX =
	QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_STATUS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_mbssid_tx_vdev_group_info - Attributes used
 * inside %QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_GROUP_INFO nested attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_GROUP_INFO_IF_INDEX:
 * u32 attribute, required. Contains interface index.
 *
 * @QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_GROUP_INFO_STATUS:
 * u8 attribute, required. 0 - means vdev is in down state.
 * 1 - means vdev is in up state.
 */
enum qca_wlan_vendor_attr_mbssid_tx_vdev_group_info {
	QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_GROUP_INFO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_GROUP_INFO_IF_INDEX = 1,
	QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_GROUP_INFO_STATUS = 2,

	QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_GROUP_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_GROUP_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_MBSSID_TX_VDEV_GROUP_INFO - 1,
};

/**
 * enum qca_wlan_concurrent_sta_policy_config - Concurrent STA policies
 *
 * @QCA_WLAN_CONCURRENT_STA_POLICY_PREFER_PRIMARY: Preference to the primary
 * STA interface has to be given while selecting the connection policies
 * (e.g., BSSID, band, TX/RX chains, etc.) for the subsequent STA interface.
 * An interface is set as primary through the attribute
 * QCA_WLAN_VENDOR_ATTR_CONFIG_CONCURRENT_STA_PRIMARY. This policy is not
 * applicable if the primary interface has not been set earlier.
 *
 * The intention is not to downgrade the primary STA performance, such as:
 * - Do not reduce the number of TX/RX chains of primary connection.
 * - Do not optimize DBS vs. MCC/SCC, if DBS ends up reducing the number of
 *   chains.
 * - If using MCC, should set the MCC duty cycle of the primary connection to
 *   be higher than the secondary connection.
 *
 * @QCA_WLAN_CONCURRENT_STA_POLICY_UNBIASED: The connection policies for the
 * subsequent STA connection shall be chosen to balance with the existing
 * concurrent STA's performance.
 * Such as
 * - Can choose MCC or DBS mode depending on the MCC efficiency and hardware
 *   capability.
 * - If using MCC, set the MCC duty cycle of the primary connection to be equal
 *   to the secondary.
 * - Prefer BSSID candidates which will help provide the best "overall"
 *   performance for all the STA connections.
 */
enum qca_wlan_concurrent_sta_policy_config {
	QCA_WLAN_CONCURRENT_STA_POLICY_PREFER_PRIMARY = 0,
	QCA_WLAN_CONCURRENT_STA_POLICY_UNBIASED = 1,
};

/**
 * enum qca_wlan_concurrent_ap_policy_config - Concurrent AP policies
 *
 * @QCA_WLAN_CONCURRENT_AP_POLICY_UNSPECIFIED: No specific policy for this AP
 * interface.
 *
 * @QCA_WLAN_CONCURRENT_AP_POLICY_GAMING_AUDIO: Select interface concurrencies
 * to meet gaming audio latency requirements.
 * This policy is used only when the driver advertises support for
 * QCA_WLAN_VENDOR_FEATURE_ENHANCED_AUDIO_EXPERIENCE_OVER_WLAN.
 *
 * @QCA_WLAN_CONCURRENT_AP_POLICY_LOSSLESS_AUDIO_STREAMING: Select interface
 * concurrencies to meet lossless audio streaming requirements.
 * This policy is used only when the driver advertises support for
 * QCA_WLAN_VENDOR_FEATURE_ENHANCED_AUDIO_EXPERIENCE_OVER_WLAN.
 *
 * @QCA_WLAN_CONCURRENT_AP_POLICY_XR: Select interface concurrencies to meet
 * XR (eXtended Reality) requirements.
 */
enum qca_wlan_concurrent_ap_policy_config {
	QCA_WLAN_CONCURRENT_AP_POLICY_UNSPECIFIED = 0,
	QCA_WLAN_CONCURRENT_AP_POLICY_GAMING_AUDIO = 1,
	QCA_WLAN_CONCURRENT_AP_POLICY_LOSSLESS_AUDIO_STREAMING = 2,
	QCA_WLAN_CONCURRENT_AP_POLICY_XR = 3,
};

/**
 * enum qca_wlan_vendor_attr_concurrent_policy - Defines attributes
 * used by QCA_NL80211_VENDOR_SUBCMD_CONCURRENT_POLICY vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONCURRENT_POLICY_STA_CONFIG:
 * u8 attribute. Configures the concurrent STA policy configuration.
 * Possible values are defined in enum qca_wlan_concurrent_sta_policy_config.

 * @QCA_WLAN_VENDOR_ATTR_CONCURRENT_POLICY_AP_CONFIG:
 * u8 attribute. Configures the concurrent AP policy configuration.
 * Possible values are defined in enum qca_wlan_concurrent_ap_policy_config.
 */
enum qca_wlan_vendor_attr_concurrent_policy {
	QCA_WLAN_VENDOR_ATTR_CONCURRENT_POLICY_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_CONCURRENT_POLICY_STA_CONFIG = 1,
	QCA_WLAN_VENDOR_ATTR_CONCURRENT_POLICY_AP_CONFIG = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_CONCURRENT_POLICY_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_CONCURRENT_POLICY_MAX =
	QCA_WLAN_VENDOR_ATTR_CONCURRENT_POLICY_AFTER_LAST - 1,

};

/* Compatibility defines for previously used enum
 * qca_wlan_vendor_attr_concurrent_policy names. These values should not be used
 * in any new implementation.
 */
#define QCA_WLAN_VENDOR_ATTR_CONCURRENT_STA_POLICY_CONFIG \
	QCA_WLAN_VENDOR_ATTR_CONCURRENT_POLICY_STA_CONFIG
#define QCA_WLAN_VENDOR_ATTR_CONCURRENT_STA_POLICY_MAX \
	QCA_WLAN_VENDOR_ATTR_CONCURRENT_POLICY_MAX
#define qca_wlan_vendor_attr_concurrent_sta_policy \
	qca_wlan_vendor_attr_concurrent_policy

/**
 * enum qca_sta_connect_fail_reason_codes - Defines values carried
 * by QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_CONNECT_FAIL_REASON_CODE vendor
 * attribute.
 * @QCA_STA_CONNECT_FAIL_REASON_NO_BSS_FOUND: No Probe Response frame received
 *	for unicast Probe Request frame.
 * @QCA_STA_CONNECT_FAIL_REASON_AUTH_TX_FAIL: STA failed to send auth request.
 * @QCA_STA_CONNECT_FAIL_REASON_AUTH_NO_ACK_RECEIVED: AP didn't send ACK for
 *	auth request.
 * @QCA_STA_CONNECT_FAIL_REASON_AUTH_NO_RESP_RECEIVED: Auth response is not
 *	received from AP.
 * @QCA_STA_CONNECT_FAIL_REASON_ASSOC_REQ_TX_FAIL: STA failed to send
 *	Association Request frame.
 * @QCA_STA_CONNECT_FAIL_REASON_ASSOC_NO_ACK_RECEIVED: AP didn't send ACK for
 *	Association Request frame.
 * @QCA_STA_CONNECT_FAIL_REASON_ASSOC_NO_RESP_RECEIVED: Association Response
 *	frame is not received from AP.
 */
enum qca_sta_connect_fail_reason_codes {
	QCA_STA_CONNECT_FAIL_REASON_NO_BSS_FOUND = 1,
	QCA_STA_CONNECT_FAIL_REASON_AUTH_TX_FAIL = 2,
	QCA_STA_CONNECT_FAIL_REASON_AUTH_NO_ACK_RECEIVED = 3,
	QCA_STA_CONNECT_FAIL_REASON_AUTH_NO_RESP_RECEIVED = 4,
	QCA_STA_CONNECT_FAIL_REASON_ASSOC_REQ_TX_FAIL = 5,
	QCA_STA_CONNECT_FAIL_REASON_ASSOC_NO_ACK_RECEIVED = 6,
	QCA_STA_CONNECT_FAIL_REASON_ASSOC_NO_RESP_RECEIVED = 7,
};

/**
 * enum qca_wlan_vendor_usable_channels_filter - Bitmask of different
 * filters defined in this enum are used in attribute
 * %QCA_WLAN_VENDOR_ATTR_USABLE_CHANNELS_FILTER_MASK.
 *
 * @QCA_WLAN_VENDOR_FILTER_CELLULAR_COEX: When this bit is set, the driver
 * shall filter the channels which are not usable because of coexistence with
 * cellular radio.
 * @QCA_WLAN_VENDOR_FILTER_WLAN_CONCURRENCY: When this bit is set, the driver
 * shall filter the channels which are not usable because of existing active
 * interfaces in the driver and will result in Multi Channel Concurrency, etc.
 *
 */
enum qca_wlan_vendor_usable_channels_filter {
	QCA_WLAN_VENDOR_FILTER_CELLULAR_COEX = 0,
	QCA_WLAN_VENDOR_FILTER_WLAN_CONCURRENCY = 1,
};

/**
 * enum qca_wlan_vendor_attr_chan_info - Attributes used inside
 * %QCA_WLAN_VENDOR_ATTR_USABLE_CHANNELS_CHAN_INFO nested attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_CHAN_INFO_PRIMARY_FREQ:
 * u32 attribute, required. Indicates the center frequency of the primary
 * channel in MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_CHAN_INFO_SEG0_FREQ:
 * u32 attribute. Indicates the center frequency of the primary segment of the
 * channel in MHz. This attribute is required when reporting 40 MHz, 80 MHz,
 * 160 MHz, and 320 MHz channels.
 *
 * @QCA_WLAN_VENDOR_ATTR_CHAN_INFO_SEG1_FREQ:
 * u32 attribute. Indicates the center frequency of the secondary segment of
 * 80+80 channel in MHz. This attribute is required only when
 * QCA_WLAN_VENDOR_ATTR_CHAN_INFO_BANDWIDTH is set to NL80211_CHAN_WIDTH_80P80.
 *
 * @QCA_WLAN_VENDOR_ATTR_CHAN_INFO_BANDWIDTH:
 * u32 attribute, required. Indicates the bandwidth of the channel, possible
 * values are defined in enum nl80211_chan_width.
 *
 * @QCA_WLAN_VENDOR_ATTR_CHAN_INFO_IFACE_MODE_MASK:
 * u32 attribute, required. Indicates all the interface types for which this
 * channel is usable. This attribute encapsulates bitmasks of interface types
 * defined in enum nl80211_iftype.
 *
 */
enum qca_wlan_vendor_attr_chan_info {
	QCA_WLAN_VENDOR_ATTR_CHAN_INFO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_CHAN_INFO_PRIMARY_FREQ = 1,
	QCA_WLAN_VENDOR_ATTR_CHAN_INFO_SEG0_FREQ = 2,
	QCA_WLAN_VENDOR_ATTR_CHAN_INFO_SEG1_FREQ = 3,
	QCA_WLAN_VENDOR_ATTR_CHAN_INFO_BANDWIDTH = 4,
	QCA_WLAN_VENDOR_ATTR_CHAN_INFO_IFACE_MODE_MASK = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_CHAN_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_CHAN_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_CHAN_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_usable_channels - Attributes used by
 * %QCA_NL80211_VENDOR_SUBCMD_USABLE_CHANNELS vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_USABLE_CHANNELS_BAND_MASK:
 * u32 attribute. Indicates the bands from which the channels should be reported
 * in response. This attribute encapsulates bit masks of bands defined in enum
 * nl80211_band. Optional attribute, if not present in the request the driver
 * shall return channels from all supported bands.
 *
 * @QCA_WLAN_VENDOR_ATTR_USABLE_CHANNELS_IFACE_MODE_MASK:
 * u32 attribute. Indicates all the interface types for which the usable
 * channels information is requested. This attribute encapsulates bitmasks of
 * interface types defined in enum nl80211_iftype. Optional attribute, if not
 * present in the request the driver shall send information of all supported
 * interface modes.
 *
 * @QCA_WLAN_VENDOR_ATTR_USABLE_CHANNELS_FILTER_MASK:
 * u32 attribute. This attribute carries information of all filters that shall
 * be applied while populating usable channels information by the driver. This
 * attribute carries bit masks of different filters defined in enum
 * qca_wlan_vendor_usable_channels_filter. Optional attribute, if not present
 * in the request the driver shall send information of channels without applying
 * any of the filters that can be configured through this attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_USABLE_CHANNELS_CHAN_INFO:
 * Nested attribute. This attribute shall be used by the driver to send
 * usability information of each channel. The attributes defined in enum
 * qca_wlan_vendor_attr_chan_info are used inside this attribute.
 */
enum qca_wlan_vendor_attr_usable_channels {
	QCA_WLAN_VENDOR_ATTR_USABLE_CHANNELS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_USABLE_CHANNELS_BAND_MASK = 1,
	QCA_WLAN_VENDOR_ATTR_USABLE_CHANNELS_IFACE_MODE_MASK = 2,
	QCA_WLAN_VENDOR_ATTR_USABLE_CHANNELS_FILTER_MASK = 3,
	QCA_WLAN_VENDOR_ATTR_USABLE_CHANNELS_CHAN_INFO = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_USABLE_CHANNELS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_USABLE_CHANNELS_MAX =
	QCA_WLAN_VENDOR_ATTR_USABLE_CHANNELS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_radar_history: Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_GET_RADAR_HISTORY to get DFS radar history.
 *
 * @QCA_WLAN_VENDOR_ATTR_RADAR_HISTORY_ENTRIES: Nested attribute to carry
 *	the list of radar history entries.
 *	Each entry contains freq, timestamp, and radar signal detect flag.
 *	The driver shall add an entry when CAC has finished, or radar signal
 *	has been detected post AP beaconing. The driver shall maintain at least
 *	8 entries in order to save CAC result for a 160 MHz channel.
 * @QCA_WLAN_VENDOR_ATTR_RADAR_HISTORY_FREQ: u32 attribute.
 *	Channel frequency in MHz.
 * @QCA_WLAN_VENDOR_ATTR_RADAR_HISTORY_TIMESTAMP: u64 nanoseconds.
 *	CLOCK_BOOTTIME timestamp when this entry is updated due to CAC
 *	or radar detection.
 * @QCA_WLAN_VENDOR_ATTR_RADAR_HISTORY_DETECTED: NLA_FLAG attribute.
 *	This flag indicates radar signal has been detected.
 * @QCA_WLAN_VENDOR_ATTR_RADAR_HISTORY_PAD: Attribute used for padding for
 *	64-bit alignment.
 */
enum qca_wlan_vendor_attr_radar_history {
	QCA_WLAN_VENDOR_ATTR_RADAR_HISTORY_INVALID = 0,

	QCA_WLAN_VENDOR_ATTR_RADAR_HISTORY_ENTRIES = 1,
	QCA_WLAN_VENDOR_ATTR_RADAR_HISTORY_FREQ = 2,
	QCA_WLAN_VENDOR_ATTR_RADAR_HISTORY_TIMESTAMP = 3,
	QCA_WLAN_VENDOR_ATTR_RADAR_HISTORY_DETECTED = 4,
	QCA_WLAN_VENDOR_ATTR_RADAR_HISTORY_PAD = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_RADAR_HISTORY_LAST,
	QCA_WLAN_VENDOR_ATTR_RADAR_HISTORY_MAX =
	QCA_WLAN_VENDOR_ATTR_RADAR_HISTORY_LAST - 1,
};

/**
 * enum qca_wlan_vendor_mcc_quota_type: MCC channel time quota type
 *
 * @QCA_WLAN_VENDOR_MCC_QUOTA_TYPE_CLEAR: In the event, it indicates that the
 *	target exited MCC state and cleared the quota information. In the
 *	command it clears MCC quota setting and restores adaptive scheduling.
 * @QCA_WLAN_VENDOR_MCC_QUOTA_TYPE_FIXED: Channel time quota is fixed and
 *      will not be changed.
 *      This quota type is present in command/event.
 * @QCA_WLAN_VENDOR_MCC_QUOTA_TYPE_DYNAMIC: Channel time quota is dynamic
 *      and the target may change the quota based on the data activity.
 *      This quota type is only present in event.
 * @QCA_WLAN_VENDOR_MCC_QUOTA_TYPE_LOW_LATENCY: Channel time quota is optimized
 *      by the target for low latency.
 *      This quota type is only present in command.
 */
enum qca_wlan_vendor_mcc_quota_type {
	QCA_WLAN_VENDOR_MCC_QUOTA_TYPE_CLEAR = 0,
	QCA_WLAN_VENDOR_MCC_QUOTA_TYPE_FIXED = 1,
	QCA_WLAN_VENDOR_MCC_QUOTA_TYPE_DYNAMIC = 2,
	QCA_WLAN_VENDOR_MCC_QUOTA_TYPE_LOW_LATENCY = 3,
};

/**
 * enum qca_wlan_vendor_attr_mcc_quota: Used by the vendor event
 * QCA_NL80211_VENDOR_SUBCMD_MCC_QUOTA to indicate MCC channel
 * quota information or as a command to set the required MCC quota for an
 * interface.
 *
 * @QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_TYPE: u32 attribute.
 * The type is defined in enum qca_wlan_vendor_mcc_quota_type.
 * In a command this specifies the MCC quota type to be set for the interface.
 * In an event this provides the current quota type in force.
 * This is required in a command and an event.
 *
 * @QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_ENTRIES: Nested attribute to carry
 * the list of channel quota entries.
 * In an event each entry contains the frequency and respective time quota for
 * all the MCC interfaces.
 * In a command it specifies the interface index and respective time quota.
 * In a command only one entry (ifindex, quota pair) may be specified.
 *
 * @QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_CHAN_FREQ: u32 attribute.
 * Channel frequency in MHz. This is present only in an event.
 *
 * @QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_CHAN_TIME_PERCENTAGE: u32 attribute.
 * Channel time quota expressed as percentage.
 * This is present in an event and a command.
 * In an command, the user shall specify the quota to be allocated for the
 * interface represented by %QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_IFINDEX.
 * In an event this provides the existing quota for the channel.
 *
 * @QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_IFINDEX: u32 attribute.
 * Specifies the interface index (netdev) for which the corresponding
 * configurations are applied. This is required in a command only. Only one
 * interface index may be specified. If not specified, the configuration is
 * rejected.
 *
 * @QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_LOW_LATENCY_MODE_ENABLE
 * 8-bit unsigned value to enable/disable Multi-Channel Concurrency
 * low latency mode. The firmware will do optimization for low
 * latency in Multi-Channel concurrency state if enabled. And all existing
 * user quota setting will be overwritten by the target.
 * 0 - disable(default), 1 - enable.
 * It is only present in a command with quota type of
 * QCA_WLAN_VENDOR_MCC_QUOTA_TYPE_LOW_LATENCY.
 */
enum qca_wlan_vendor_attr_mcc_quota {
	QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_TYPE = 1,
	QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_ENTRIES = 2,
	QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_CHAN_FREQ = 3,
	QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_CHAN_TIME_PERCENTAGE = 4,
	QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_IFINDEX = 5,
	QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_LOW_LATENCY_MODE_ENABLE = 6,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_LAST,
	QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_MAX =
	QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_LAST - 1,
};

/**
 * enum qca_wlan_roam_stats_invoke_reason - Roam invoke reason. These values
 * are used by the attribute
 * %QCA_WLAN_VENDOR_ATTR_ROAM_STATS_USER_TRIGGER_INVOKE_REASON.
 *
 * @QCA_WLAN_ROAM_STATS_INVOKE_REASON_UNDEFINED: Default value when target
 *  invoke roam.
 * @QCA_WLAN_ROAM_STATS_INVOKE_REASON_NUD_FAILURE: Neighbor unreachable
 *  detection failed when the roam trigger.
 * @QCA_WLAN_ROAM_STATS_INVOKE_REASON_USER_SPACE: Invoke from user space.
 */

enum qca_wlan_roam_stats_invoke_reason {
	QCA_WLAN_ROAM_STATS_INVOKE_REASON_UNDEFINED = 0,
	QCA_WLAN_ROAM_STATS_INVOKE_REASON_NUD_FAILURE = 1,
	QCA_WLAN_ROAM_STATS_INVOKE_REASON_USER_SPACE = 2,
};

/**
 * enum qca_wlan_roam_stats_tx_failures_reason - Roam TX failures reason. These
 * values are used by the attribute
 * %QCA_WLAN_VENDOR_ATTR_ROAM_STATS_TX_FAILURES_REASON.
 *
 * @QCA_WLAN_ROAM_STATS_KICKOUT_REASON_UNSPECIFIED: Default value when
 *  roam by kickout.
 * @QCA_WLAN_ROAM_STATS_KICKOUT_REASON_XRETRY: Excessive retry when roam
 *  trigger by kickout.
 * @QCA_WLAN_ROAM_STATS_KICKOUT_REASON_INACTIVITY: Station inactivity when
 *  roam trigger by kickout.
 * @QCA_WLAN_ROAM_STATS_KICKOUT_REASON_IBSS_DISCONNECT: IBSS disconnect when
 *  roam trigger by kickout.
 * @QCA_WLAN_ROAM_STATS_KICKOUT_REASON_TDLS_DISCONNECT: TDLS peer has
 *  disappeared, and all TX is failing when roam trigger by kickout.
 * @QCA_WLAN_ROAM_STATS_KICKOUT_REASON_SA_QUERY_TIMEOUT: SA query process
 *   timeout when roam trigger by kickout.
 * @QCA_WLAN_ROAM_STATS_KICKOUT_REASON_ROAMING_EVENT: Directly connected
 *  peer has roamed to a repeater.
 */
enum qca_wlan_roam_stats_tx_failures_reason {
	QCA_WLAN_ROAM_STATS_KICKOUT_REASON_UNSPECIFIED = 0,
	QCA_WLAN_ROAM_STATS_KICKOUT_REASON_XRETRY = 1,
	QCA_WLAN_ROAM_STATS_KICKOUT_REASON_INACTIVITY = 2,
	QCA_WLAN_ROAM_STATS_KICKOUT_REASON_IBSS_DISCONNECT = 3,
	QCA_WLAN_ROAM_STATS_KICKOUT_REASON_TDLS_DISCONNECT = 4,
	QCA_WLAN_ROAM_STATS_KICKOUT_REASON_SA_QUERY_TIMEOUT = 5,
	QCA_WLAN_ROAM_STATS_KICKOUT_REASON_ROAMING_EVENT = 6,
};

/**
 * enum qca_wlan_roam_stats_abort_reason - Roam abort reason. These values
 * are used by the attribute %QCA_WLAN_VENDOR_ATTR_ROAM_STATS_ABORT_REASON.
 *
 * @QCA_WLAN_ROAM_STATS_ABORT_UNSPECIFIED: Target did not specify the
 *  detailed reason for roam scan being aborted.
 * @QCA_WLAN_ROAM_STATS_ABORT_LOWRSSI_DATA_RSSI_HIGH: Roam scan is not
 *  started due to high data RSSI during LOW-RSSI roaming.
 * @QCA_WLAN_ROAM_STATS_ABORT_LOWRSSI_LINK_SPEED_GOOD: Roam scan is not
 *  started due to good link speed during LOW-RSSI roaming.
 * @QCA_WLAN_ROAM_STATS_ABORT_BG_DATA_RSSI_HIGH: Roam scan is not started
 *  due to high data RSSI during background roaming.
 * @QCA_WLAN_ROAM_STATS_ABORT_BG_RSSI_ABOVE_THRESHOLD: Roam scan is not
 *  started due to high beacon RSSI during background roaming
 */
enum qca_wlan_roam_stats_abort_reason {
	QCA_WLAN_ROAM_STATS_ABORT_UNSPECIFIED = 0,
	QCA_WLAN_ROAM_STATS_ABORT_LOWRSSI_DATA_RSSI_HIGH = 1,
	QCA_WLAN_ROAM_STATS_ABORT_LOWRSSI_LINK_SPEED_GOOD = 2,
	QCA_WLAN_ROAM_STATS_ABORT_BG_DATA_RSSI_HIGH = 3,
	QCA_WLAN_ROAM_STATS_ABORT_BG_RSSI_ABOVE_THRESHOLD = 4,
};

/**
 * enum qca_wlan_roam_stats_scan_type - Roam scan type define.
 * These values are used by the attribute
 * %QCA_WLAN_VENDOR_ATTR_ROAM_STATS_SCAN_TYPE.
 *
 * @QCA_WLAN_ROAM_STATS_SCAN_TYPE_PARTIAL: Partial channel scan
 * @QCA_WLAN_ROAM_STATS_SCAN_TYPE_FULL: Full channel scan
 * @QCA_WLAN_ROAM_STATS_SCAN_TYPE_NO_SCAN: No roam scan was triggered.
 *  This is generally used in BTM events to indicate BTM frame exchange logs.
 * @QCA_WLAN_ROAM_STATS_SCAN_TYPE_HIGHER_BAND_5GHZ_6GHZ: Higher band roam scan
 *  from 2.4 GHz to 5 GHz or 6 GHz
 * @QCA_WLAN_ROAM_STATS_SCAN_TYPE_HIGHER_BAND_6GHZ: Higher band roam scan from
 *  5 GHz to 6 GHz
 */
enum qca_wlan_roam_stats_scan_type {
	QCA_WLAN_ROAM_STATS_SCAN_TYPE_PARTIAL = 0,
	QCA_WLAN_ROAM_STATS_SCAN_TYPE_FULL = 1,
	QCA_WLAN_ROAM_STATS_SCAN_TYPE_NO_SCAN = 2,
	QCA_WLAN_ROAM_STATS_SCAN_TYPE_HIGHER_BAND_5GHZ_6GHZ = 3,
	QCA_WLAN_ROAM_STATS_SCAN_TYPE_HIGHER_BAND_6GHZ = 4,
};

/**
 * enum qca_wlan_roam_stats_scan_dwell_type - Roam scan dwell type.
 * These values are used by the attribute
 * %QCA_WLAN_VENDOR_ATTR_ROAM_STATS_SCAN_DWELL_TYPE.
 *
 * @QCA_WLAN_ROAM_STATS_DWELL_TYPE_UNSPECIFIED: Target did not specify the
 *  detailed roam scan type.
 * @QCA_WLAN_ROAM_STATS_DWELL_ACTIVE_TYPE: Active scan during roam.
 * @QCA_WLAN_ROAM_STATS_DWELL_PASSIVE_TYPE: Passive scan during roam.
 */
enum qca_wlan_roam_stats_scan_dwell_type {
	QCA_WLAN_ROAM_STATS_DWELL_TYPE_UNSPECIFIED = 0,
	QCA_WLAN_ROAM_STATS_DWELL_TYPE_ACTIVE = 1,
	QCA_WLAN_ROAM_STATS_DWELL_TYPE_PASSIVE = 2,
};

/**
 * enum qca_wlan_vendor_attr_roam_stats_scan_chan_info - Attributes used inside
 * the %QCA_WLAN_VENDOR_ATTR_ROAM_STATS_SCAN_CHAN_INFO nested attribute.
 */
enum qca_wlan_vendor_attr_roam_stats_scan_chan_info {
	/* 32-bit unsigned value to indicate center frequency of the primary
	 * channel in MHz for each roam scan channel.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_SCAN_CHANNEL_FREQ = 1,
	/* 8-bit unsigned value to indicate channel scan type for each
	 * roam scan channel, values in qca_wlan_roam_stats_scan_dwell_type.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_SCAN_DWELL_TYPE = 2,
	/* 32-bit unsigned value to indicate maximum scan time in milliseconds
	 * for each roam scan channel.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_MAX_DWELL_TIME = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_SCAN_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_SCAN_INFO_FRAME_MAX =
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_SCAN_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_roam_stats_frame_subtype - Roam frame subtypes. These values
 * are used by the attribute %QCA_WLAN_VENDOR_ATTR_ROAM_STATS_FRAME_SUBTYPE.
 *
 * @QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_AUTH_RESP: Authentication Response frame
 * @QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_REASSOC_RESP: Reassociation Response frame
 * @QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_EAPOL_M1: EAPOL-Key M1 frame
 * @QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_EAPOL_M2: EAPOL-Key M2 frame
 * @QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_EAPOL_M3: EAPOL-Key M3 frame
 * @QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_EAPOL_M4: EAPOL-Key M4 frame
 * @QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_EAPOL_GTK_M1: EAPOL-Key GTK M1 frame
 * @QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_EAPOL_GTK_M2: EAPOL-Key GTK M2 frame
 * @QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_AUTH_REQ: Authentication Request frame
 * @QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_REASSOC_REQ: Reassociation Request frame
 */
enum qca_wlan_roam_stats_frame_subtype {
	QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_AUTH_RESP = 1,
	QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_REASSOC_RESP = 2,
	QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_EAPOL_M1 = 3,
	QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_EAPOL_M2 = 4,
	QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_EAPOL_M3 = 5,
	QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_EAPOL_M4 = 6,
	QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_EAPOL_GTK_M1 = 7,
	QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_EAPOL_GTK_M2 = 8,
	QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_AUTH_REQ = 9,
	QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_REASSOC_REQ = 10,
};

/* Compatibility defines for previously used names.
 * These values should not be used in any new implementation.
 */
#define QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_PREAUTH \
	QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_AUTH_RESP
#define QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_REASSOC \
	QCA_WLAN_ROAM_STATS_FRAME_SUBTYPE_REASSOC_RESP

/**
 * enum roam_frame_status - Specifies the valid values the vendor roam frame
 * attribute QCA_WLAN_VENDOR_ATTR_ROAM_STATS_FRAME_STATUS can take.
 *
 * @QCA_WLAN_ROAM_FRAME_STATUS_SUCCESS: It indicates the roam frame was
 *  sent or received successfully.
 * @QCA_WLAN_ROAM_FRAME_STATUS_FAIL: It indicates the roam frame sending or
 *  receiving failed.
 */
enum qca_wlan_roam_stats_frame_status {
	QCA_WLAN_ROAM_STATS_FRAME_STATUS_SUCCESS = 0,
	QCA_WLAN_ROAM_STATS_FRAME_STATUS_FAIL = 1,
};

/**
 * enum qca_wlan_vendor_attr_roam_stats_frame_info - Attributes used within the
 * %QCA_WLAN_VENDOR_ATTR_ROAM_STATS_FRAME_INFO nested attribute.
 */
enum qca_wlan_vendor_attr_roam_stats_frame_info {
	/* 8-bit unsigned value to indicate the frame subtype during
	 * roaming, one of the values in qca_wlan_roam_stats_frame_subtype.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_FRAME_SUBTYPE = 1,
	/* 8-bit unsigned value to indicate the frame is successful or failed
	 * during roaming, one of the values in
	 * qca_wlan_roam_stats_frame_status.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_FRAME_STATUS = 2,
	/* 64-bit unsigned value to indicate the timestamp for frame of
	 * preauthentication/reassociation/EAPOL-M1/EAPOL-M2/EAPOL-M3/EAPOL-M4
	 * when sent or received during roaming, timestamp in milliseconds
	 * from system boot.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_FRAME_TIMESTAMP = 3,
	/* Attribute used for padding for 64-bit alignment */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_FRAME_PAD = 4,
	/* This attribute indicates a 6-byte MAC address representing
	 * the BSSID of the AP.
	 * For non-MLO scenario, it indicates the AP BSSID.
	 * For MLO scenario, it indicates the AP BSSID which may be the primary
	 * link BSSID or a nonprimary link BSSID.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_FRAME_BSSID = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_FRAME_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_FRAME_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_FRAME_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_roam_stats_info - Used by the attribute
 * QCA_WLAN_VENDOR_ATTR_ROAM_STATS_INFO.
 */
enum qca_wlan_vendor_attr_roam_stats_info {
	/* 64-bit unsigned value to indicate the timestamp when roam was
	 * triggered by the firmware, timestamp in milliseconds from system
	 * boot.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_ROAM_TRIGGER_TIMESTAMP = 1,
	/* 32-bit unsigned value to indicate the roam trigger reason for the
	 * last roaming attempted by the firmware. This can be queried either
	 * in a connected state or disconnected state. The values of this
	 * attribute represent the roam trigger reason codes, which
	 * are defined in enum qca_roam_reason.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_TRIGGER_REASON = 2,
	/* 8-bit unsigned value to indicate percentage of packets for which
	 * the RX rate is lower than the RX rate threshold in total RX packets,
	 * used for roaming trigger by per.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_PER_RXRATE_THRESHOLD_PERCENT = 3,
	/* 8-bit unsigned value to indicate percentage of packets for which
	 * the TX rate is lower than TX rate threshold in total TX packets,
	 * used for roaming trigger by per.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_PER_TXRATE_THRESHOLD_PERCENT = 4,
	/* 32-bit unsigned value to indicate final beacon miss count for
	 * trigger reason of beacon miss.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_FINAL_BMISS_CNT = 5,
	/* 32-bit unsigned value to indicate consecutive beacon miss count
	 * for trigger reason of beacon miss.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_CONSECUTIVE_BMISS_CNT = 6,
	/* 8-bit unsigned value to indicate QOS-NULL TX status for trigger
	 * reason of beacon miss, 0 - success, 1 - fail.
	 * If QOS-NULL TX status is successful, beacon miss final count and
	 * consecutive beacon miss count will be reset to zero, and roam will
	 * not be triggered. If QOS-NULL TX status is failed, beacon miss final
	 * count and consecutive beacon miss count continue to calculate until
	 * roaming trigger by beacon miss.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_BMISS_QOS_NULL_SUCCESS = 7,
	/* 8-bit signed value to indicate connected AP RSSI in dBm
	 * for trigger reason of poor RSSI.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_POOR_RSSI_CURRENT_RSSI = 8,
	/* 8-bit signed value to indicate RSSI threshold value in dBm
	 * for trigger reason of poor RSSI.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_POOR_RSSI_ROAM_RSSI_THRESHOLD = 9,
	/* 8-bit unsigned value to indicate RX link speed status
	 * for trigger reason of poor RSSI, 0 - good link speed,
	 * 1 - bad link speed.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_POOR_RSSI_RX_LINKSPEED_STATUS = 10,
	/* 8-bit signed value to indicate connected AP RSSI in dBm
	 * for trigger reason of better RSSI.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_BETTER_RSSI_CURRENT_RSSI = 11,
	/* 8-bit signed value to indicate RSSI threshold value in dBm
	 * for trigger reason of better RSSI.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_BETTER_RSSI_HIGH_RSSI_THRESHOLD = 12,
	/* 32-bit unsigned value to indicate RX throughput in bytes per second
	 * for trigger reason of congestion.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_CONGESTION_RX_TPUT = 13,
	/* 32-bit unsigned value to indicate TX throughput in bytes per second
	 * for trigger reason of congestion.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_CONGESTION_TX_TPUT = 14,
	/* 8-bit unsigned value to indicate roamable AP count
	 * for trigger reason of congestion.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_CONGESTION_ROAMABLE_CNT = 15,
	/* 8-bit unsigned value to indicate invoke reason, one of the values
	 * defined in qca_wlan_roam_stats_invoke_reason.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_USER_TRIGGER_INVOKE_REASON = 16,
	/* 8-bit unsigned value to indicate request mode for trigger reason
	 * of BTM, values are defined in IEEE Std 802.11-2020, 9.6.13.9.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_BTM_REQUEST_MODE = 17,
	/* 32-bit unsigned value to indicate disassociate time in milliseconds
	 * for trigger reason of BTM.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_BTM_DISASSOC_IMMINENT_TIME = 18,
	/* 32-bit unsigned value to indicate preferred candidate list valid
	 * interval in milliseconds for trigger reason of BTM.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_BTM_VALID_INTERNAL = 19,
	/* 8-bit unsigned value to indicate the number of preferred
	 * candidates for trigger reason of BTM.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_BTM_CANDIDATE_LIST_CNT = 20,
	/* 8-bit unsigned value to indicate response status for trigger
	 * reason of BTM, values are defined in IEEE Std 802.11-2020,
	 * Table 9-428 (BTM status code definitions).
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_BTM_RESPONSE_STATUS_CODE = 21,
	/* 32-bit unsigned value to indicate BSS termination timeout value
	 * in milliseconds for trigger reason of BTM.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_BTM_BSS_TERMINATION_TIMEOUT = 22,
	/* 32-bit unsigned value to indicate MBO associate retry timeout
	 * value in milliseconds for trigger reason of BTM.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_BTM_MBO_ASSOC_RETRY_TIMEOUT = 23,
	/* 8-bit unsigned value to indicate dialog token number
	 * for trigger reason of BTM.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_BTM_REQ_DIALOG_TOKEN = 24,
	/* 8-bit unsigned value to indicate percentage of connected AP
	 * channel congestion utilization for trigger reason of BSS load.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_BSS_CU_LOAD = 25,
	/* 8-bit unsigned value to indicate disconnection type
	 * for trigger reason of disconnection. 1 - Deauthentication,
	 * 2 - Disassociation.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_DISCONNECTION_TYPE = 26,
	/* 16-bit unsigned value to indicate deauthentication or disassociation
	 * reason for trigger reason of disconnection, values are defined
	 * in IEEE Std 802.11-2020, Table 9-49 (Reason codes).
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_DISCONNECTION_REASON = 27,
	/* 32-bit unsigned value to indicate milliseconds of roam scan
	 * periodicity when needing to roam to find a better AP for trigger
	 * reason of periodic timer.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_PERIODIC_TIMER_MS = 28,
	/* 8-bit signed value to indicate connected AP RSSI in dBm for
	 * trigger reason of background scan.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_BACKGROUND_SCAN_CURRENT_RSSI = 29,
	/* 8-bit signed value to indicate data RSSI in dBm for trigger reason
	 * of background scan.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_BACKGROUND_SCAN_DATA_RSSI = 30,
	/* 8-bit signed value to indicate data RSSI threshold in dBm
	 * for trigger reason of background scan.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_BACKGROUND_SCAN_DATA_RSSI_THRESH = 31,
	/* 32-bit unsigned value to indicate consecutive TX failure threshold
	 * for trigger reason of TX failures.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_TX_FAILURES_THRESHOLD = 32,
	/* 8-bit unsigned value to indicate TX failure reason for trigger
	 * reason of TX failures, one of the values defined in
	 *  qca_wlan_roam_stats_tx_failures_reason.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_TX_FAILURES_REASON = 33,
	/* 8-bit unsigned value to indicate detail abort reason. One of the
	 * values in enum qca_wlan_roam_stats_abort_reason.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_ABORT_REASON = 34,
	/* 8-bit signed value to indicate data RSSI in dBm when aborting the
	 * roam scan.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_DATA_RSSI = 35,
	/* 8-bit signed value to indicate data RSSI threshold in dBm when
	 * aborting the roam scan.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_DATA_RSSI_THRESHOLD = 36,
	/* 8-bit unsigned value to indicate data RSSI threshold in RX link
	 * speed status when aborting the roam scan.
	 * 0 - good link speed, 1 - bad link speed
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_DATA_RX_LINKSPEED_STATUS = 37,
	/* 8-bit unsigned value to indicate roaming scan type.
	 * One of the values in enum qca_wlan_roam_stats_scan_type.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_SCAN_TYPE = 38,
	/* 8-bit unsigned value to indicate roaming result, used in STA mode
	 * only.
	 * 0-Roaming is successful, 1-Roaming is failed
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_ROAM_STATUS = 39,
	/* 8-bit unsigned value to indicate the roam fail reason for the
	 * last failed roaming attempt by the firmware. Different roam failure
	 * reason codes are specified in enum qca_vendor_roam_fail_reasons.
	 * This can be queried either in connected state or disconnected state.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_FAIL_REASON = 40,
	/* Nested attribute. Indicate roam scan info for each channel, the
	 * attributes defined in enum
	 * qca_wlan_vendor_attr_roam_stats_scan_chan_info are used inside
	 * this attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_SCAN_CHAN_INFO = 41,
	/* 32-bit unsigned value to indicate total scan time during roam scan
	 * all channels, time in milliseconds.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_TOTAL_SCAN_TIME = 42,
	/* Nested attribute. This attribute shall be used by the driver to
	 * send roam information of each subtype. The attributes defined in
	 * enum qca_wlan_vendor_attr_roam_stats_frame_info are used inside
	 * this attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_FRAME_INFO = 43,
	/* Attribute used for padding for 64-bit alignment */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_PAD = 44,
	/* 6-byte MAC address used by the driver to send roam stats information
	 * of the original AP BSSID. The original AP is the connected AP before
	 * roam happens, regardless of the roam resulting in success or failure.
	 * This attribute is only present when
	 * QCA_WLAN_VENDOR_ATTR_ROAM_STATS_ROAM_STATUS has a value of
	 * 0 (success) or 1 (failure).
	 * For non-MLO scenario, it indicates the original connected AP BSSID.
	 * For MLO scenario, it indicates the original BSSID of the link
	 * for which the reassociation occurred during the roam.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_ORIGINAL_BSSID = 45,
	/* 6-byte MAC address used by the driver to send roam stats information
	 * of the roam candidate AP BSSID when roam failed. This is only present
	 * when QCA_WLAN_VENDOR_ATTR_ROAM_STATS_ROAM_STATUS has a value of
	 * 1 (failure). If the firmware updates more than one candidate AP BSSID
	 * to the driver, the driver only fills the last candidate AP BSSID and
	 * reports it to user space.
	 * For non-MLO scenario, it indicates the last candidate AP BSSID.
	 * For MLO scenario, it indicates the AP BSSID which may be the primary
	 * link BSSID or a nonprimary link BSSID.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_CANDIDATE_BSSID = 46,
	/* 6-byte MAC address used by the driver to send roam stats information
	 * of the roamed AP BSSID when roam succeeds. This is only present when
	 * QCA_WLAN_VENDOR_ATTR_ROAM_STATS_ROAM_STATUS has a value of
	 * 0 (success).
	 * For non-MLO scenario, it indicates the new AP BSSID to which has
	 * been successfully roamed.
	 * For MLO scenario, it indicates the new AP BSSID of the link on
	 * which the reassociation occurred during the roam.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_ROAMED_BSSID = 47,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_MAX =
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_roam_cached_stats - Vendor subcmd attributes to
 * report cached roam info from the driver to user space, enum values are used
 * for netlink attributes sent with the
 * %QCA_NL80211_VENDOR_SUBCMD_ROAM_STATS sub command.
 */
enum qca_wlan_vendor_attr_roam_cached_stats {
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_INVALID = 0,
	/* Nested attribute, this attribute contains nested array roam info
	 * statistics defined in enum qca_wlan_vendor_attr_roam_stats_info.
	 */
	QCA_WLAN_VENDOR_ATTR_ROAM_STATS_INFO = 1,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ROAM_CACHED_STATS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ROAM_CACHED_STATS_MAX =
	QCA_WLAN_VENDOR_ATTR_ROAM_CACHED_STATS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_supported_radio_cfg - Attributes for
 * radio configurations present in each radio combination.
 *
 * @QCA_WLAN_VENDOR_ATTR_SUPPORTED_RADIO_CFG_BAND: u32 attribute indicates
 * the band info in the radio configuration. Uses the enum qca_set_band values.
 *
 * @QCA_WLAN_VENDOR_ATTR_SUPPORTED_RADIO_CFG_ANTENNA: u8 attribute indicates
 * the number of antennas info in the radio configuration.
 */
enum qca_wlan_vendor_attr_supported_radio_cfg {
	QCA_WLAN_VENDOR_ATTR_SUPPORTED_RADIO_CFG_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SUPPORTED_RADIO_CFG_BAND = 1,
	QCA_WLAN_VENDOR_ATTR_SUPPORTED_RADIO_CFG_ANTENNA = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SUPPORTED_RADIO_CFG_LAST,
	QCA_WLAN_VENDOR_ATTR_SUPPORTED_RADIO_CFG_MAX =
	QCA_WLAN_VENDOR_ATTR_SUPPORTED_RADIO_CFG_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_radio_combination - Attributes for
 * radio combinations supported by the device.
 *
 * @QCA_WLAN_VENDOR_ATTR_RADIO_COMBINATIONS_CFGS: Nested attribute
 * provides the radio configurations present in the radio combination.
 * Uses the enum qca_wlan_vendor_attr_supported_radio_cfg attributes.
 * This attribute provides the values for radio combination matrix.
 * For standalone config, the number of config values is one and the config
 * carries the band and antenna information for standalone configuration. For
 * Dual Band Simultaneous (DBS)/Single Band Simultaneous (SBS) mode
 * configuration the number of config values is two and the config carries the
 * band and antenna information for each simultaneous radio.
 */
enum qca_wlan_vendor_attr_radio_combination {
	QCA_WLAN_VENDOR_ATTR_RADIO_COMBINATIONS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_RADIO_COMBINATIONS_CFGS = 1,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_RADIO_COMBINATIONS_LAST,
	QCA_WLAN_VENDOR_ATTR_RADIO_COMBINATIONS_MAX =
	QCA_WLAN_VENDOR_ATTR_RADIO_COMBINATIONS_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_radio_combination_matrix - Attributes used by
 * %QCA_NL80211_VENDOR_SUBCMD_GET_RADIO_COMBINATION_MATRIX
 *
 * @QCA_WLAN_VENDOR_ATTR_RADIO_MATRIX_SUPPORTED_CFGS: Nested attribute
 * provides the radio combinations supported by the device.
 * Uses the enum qca_wlan_vendor_attr_radio_combination attributes.
 * For example, in the radio combination matrix for a device which has two
 * radios, where one radio is capable of 2.4 GHz 2X2 only and another radio is
 * capable of either 5 GHz or 6 GHz 2X2, the possible number of radio
 * combinations is 5 and the radio combinations are
 * {{{2.4 GHz 2X2}}, //Standalone 2.4 GHz
 * {{5 GHz 2X2}}, //Standalone 5 GHz
 * {{6 GHz 2X2}}, //Standalone 6 GHz
 * {{2.4 GHz 2X2}, {5 GHz 2X2}}, //2.4 GHz + 5 GHz DBS
 * {{2.4 GHz 2X2}, {6 GHz 2X2}}} //2.4 GHz + 6 GHz DBS
 * The band and antenna info together as nested data provides one radio config.
 * Standalone configuration has one config with band and antenna nested data.
 * Dual Band Simultaneous (DBS)/Single Band Simultaneous (SBS) configuration
 * have two nested band and antenna info data.
 */
enum qca_wlan_vendor_attr_radio_combination_matrix {
	QCA_WLAN_VENDOR_ATTR_RADIO_MATRIX_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_RADIO_MATRIX_SUPPORTED_CFGS = 1,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_RADIO_MATRIX_LAST,
	QCA_WLAN_VENDOR_ATTR_RADIO_MATRIX_MAX =
	QCA_WLAN_VENDOR_ATTR_RADIO_MATRIX_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_mdns_offload - Attributes used by
 * %QCA_NL80211_VENDOR_SUBCMD_MDNS_OFFLOAD vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_ENABLE: Required (flag)
 * Enable mDNS offload. This attribute is mandatory to enable
 * mDNS offload feature. If this attribute is not present, mDNS offload
 * is disabled.
 *
 * @QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_TABLE: Nested attribute containing
 * one or more %QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_ENTRY attributes. This
 * attribute is mandatory when enabling the feature, and not required when
 * disabling the feature.
 *
 * @QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_ENTRY: Nested attribute containing
 * the following attributes:
 *	%QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_FQDN
 *	%QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_ANSWER_RESOURCE_RECORDS_COUNT
 *	%QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_ANSWER_PAYLOAD
 *
 * @QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_FQDN: Required string attribute.
 * It consists of a hostname and ".local" as the domain name. The character
 * set is limited to UTF-8 encoding. The maximum allowed size is 63 bytes.
 * It is used to compare the domain in the "QU" query. Only 1 FQDN is
 * supported per vdev.
 * For example: myphone.local
 *
 * @QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_ANSWER_RESOURCE_RECORDS_COUNT: Required
 * u16 attribute. It specifies the total number of resource records present
 * in the answer section of the answer payload. This attribute is needed by the
 * firmware to populate the mDNS response frame for mDNS queries without having
 * to parse the answer payload.
 *
 * @QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_ANSWER_PAYLOAD: Required binary blob
 * attribute sent by the mdnsResponder from userspace. It contains resource
 * records of various types (e.g., A, AAAA, PTR, TXT) and service list. This
 * payload is passed down to the firmware and is transmitted in response to
 * mDNS queries.
 * The maximum supported size of the answer payload is 512 bytes.
 */
enum qca_wlan_vendor_attr_mdns_offload {
	QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_ENABLE = 1,
	QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_TABLE = 2,
	QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_ENTRY = 3,
	QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_FQDN = 4,
	QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_ANSWER_RESOURCE_RECORDS_COUNT = 5,
	QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_ANSWER_PAYLOAD = 6,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_MAX =
	QCA_WLAN_VENDOR_ATTR_MDNS_OFFLOAD_AFTER_LAST - 1,
};

/**
 * qca_wlan_vendor_monitor_data_frame_type - Represent the various
 * Data frame types to be sent over the monitor interface.
 */
enum qca_wlan_vendor_monitor_data_frame_type {
	QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_ALL = BIT(0),
	/* valid only if QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_ALL is not set
	 */
	QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_ARP = BIT(1),
	QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_DHCPV4 = BIT(2),
	QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_DHCPV6 = BIT(3),
	QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_EAPOL = BIT(4),
	QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_DNSV4 = BIT(5),
	QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_DNSV6 = BIT(6),
	QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_TCP_SYN = BIT(7),
	QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_TCP_SYNACK = BIT(8),
	QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_TCP_FIN = BIT(9),
	QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_TCP_FINACK = BIT(10),
	QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_TCP_ACK = BIT(11),
	QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_TCP_RST = BIT(12),
	QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_ICMPV4 = BIT(13),
	QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_ICMPV6 = BIT(14),
	QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_RTP = BIT(15),
	QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_SIP = BIT(16),
	QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_QOS_NULL = BIT(17),
};

/**
 * qca_wlan_vendor_monitor_mgmt_frame_type - Represent the various
 * Management frame types to be sent over the monitor interface.
 * @QCA_WLAN_VENDOR_MONITOR_MGMT_FRAME_TYPE_ALL: All the Management Frames.
 * @QCA_WLAN_VENDOR_MONITOR_MGMT_CONNECT_NO_BEACON: All the Management frames
 * except the Beacon frame.
 * @QCA_WLAN_VENDOR_MONITOR_MGMT_CONNECT_BEACON: Only the connected
 * BSSID Beacon frames. Valid only in the connected state.
 * @QCA_WLAN_VENDOR_MONITOR_MGMT_CONNECT_SCAN_BEACON: Represents
 * the Beacon frames obtained during the scan (off channel and connected
 * channel), when in connected state.
 */
enum qca_wlan_vendor_monitor_mgmt_frame_type {
	QCA_WLAN_VENDOR_MONITOR_MGMT_FRAME_TYPE_ALL = BIT(0),
	/* valid only if QCA_WLAN_VENDOR_MONITOR_MGMT_FRAME_TYPE_ALL is not set
	 */
	QCA_WLAN_VENDOR_MONITOR_MGMT_NO_BEACON = BIT(1),
	QCA_WLAN_VENDOR_MONITOR_MGMT_CONNECT_BEACON = BIT(2),
	QCA_WLAN_VENDOR_MONITOR_MGMT_CONNECT_SCAN_BEACON = BIT(3),
};

/**
 * qca_wlan_vendor_monitor_ctrl_frame_type - Represent the various
 * Control frame types to be sent over the monitor interface.
 * @QCA_WLAN_VENDOR_MONITOR_CTRL_FRAME_TYPE_ALL: All the Control frames
 * @QCA_WLAN_VENDOR_MONITOR_CTRL_TRIGGER_FRAME: Trigger frame
 */
enum qca_wlan_vendor_monitor_ctrl_frame_type {
	QCA_WLAN_VENDOR_MONITOR_CTRL_FRAME_TYPE_ALL = BIT(0),
	/* valid only if QCA_WLAN_VENDOR_MONITOR_CTRL_FRAME_TYPE_ALL is not set
	 */
	QCA_WLAN_VENDOR_MONITOR_CTRL_TRIGGER_FRAME = BIT(1),
};

/**
 * enum qca_wlan_vendor_attr_set_monitor_mode - Used by the
 * vendor command QCA_NL80211_VENDOR_SUBCMD_SET_MONITOR_MODE to set the
 * monitor mode.
 *
 * @QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_DATA_TX_FRAME_TYPE: u32 attribute.
 * Represents the TX Data frame types to be monitored (u32). These Data frames
 * are represented by enum qca_wlan_vendor_monitor_data_frame_type.
 *
 * @QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_DATA_RX_FRAME_TYPE: u32 attribute.
 * Represents the RX Data frame types to be monitored (u32). These Data frames
 * are represented by enum qca_wlan_vendor_monitor_data_frame_type.
 *
 * @QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE: u32 attribute.
 * Represents the TX Management frame types to be monitored (u32). These
 * Management frames are represented by
 * enum qca_wlan_vendor_monitor_mgmt_frame_type.
 *
 * @QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE: u32 attribute.
 * Represents the RX Management frame types to be monitored (u32). These
 * Management frames are represented by
 * enum qca_wlan_vendor_monitor_mgmt_frame_type.
 *
 * @QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE: u32 attribute.
 * Represents the TX Control frame types to be monitored (u32). These Control
 * frames are represented by enum qca_wlan_vendor_monitor_ctrl_frame_type.
 *
 * @QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE: u32 attribute.
 * Represents the RX Control frame types to be monitored (u32). These Control
 * frames are represented by enum qca_wlan_vendor_monitor_ctrl_frame_type.
 *
 * @QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_CONNECTED_BEACON_INTERVAL: u32
 * attribute.
 * Represents the interval in milliseconds only for the connected Beacon frames,
 * expecting the connected BSS's Beacon frames to be sent on the monitor
 * interface at this specific interval.
 */
enum qca_wlan_vendor_attr_set_monitor_mode {
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_DATA_TX_FRAME_TYPE = 1,
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_DATA_RX_FRAME_TYPE = 2,
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE = 3,
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE = 4,
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE = 5,
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE = 6,
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_CONNECTED_BEACON_INTERVAL = 7,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_MAX =
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_roam_scan_state - Roam scan state flags.
 * Bits will be set to 1 if the corresponding state is enabled.
 *
 * @QCA_VENDOR_WLAN_ROAM_SCAN_STATE_START: Scan Start.
 * @QCA_VENDOR_WLAN_ROAM_SCAN_STATE_END: Scan end.
 */
enum qca_wlan_vendor_roam_scan_state {
	QCA_WLAN_VENDOR_ROAM_SCAN_STATE_START = BIT(0),
	QCA_WLAN_VENDOR_ROAM_SCAN_STATE_END = BIT(1),
};

/**
 * enum qca_wlan_vendor_roam_event_type - Roam event type flags.
 * Bits will be set to 1 if the corresponding event is notified.
 *
 * @QCA_WLAN_VENDOR_ROAM_EVENT_TRIGGER_REASON: Represents that the roam event
 * carries the trigger reason. When set, it is expected that the roam event
 * carries the respective reason via the attribute
 * QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_TRIGGER_REASON. This event also carries
 * the BSSID, RSSI, frequency info of the AP to which the roam is attempted.
 *
 * @QCA_WLAN_VENDOR_ROAM_EVENT_FAIL_REASON: Represents that the roam event
 * carries the roam fail reason. When set, it is expected that the roam event
 * carries the respective reason via the attribute
 * QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_FAIL_REASON. This event also carries the
 * BSSID, RSSI, frequency info of the AP to which the roam was attempted.
 *
 * @QCA_WLAN_VENDOR_ROAM_EVENT_INVOKE_FAIL_REASON: Represents that the roam
 * event carries the roam invoke fail reason. When set, it is expected that
 * the roam event carries the respective reason via the attribute
 * QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_INVOKE_FAIL_REASON.
 *
 * @QCA_WLAN_VENDOR_ROAM_EVENT_SCAN_STATE: Represents that the roam event
 * carries the roam scan state. When set, it is expected that the roam event
 * carries the respective scan state via the attribute
 * QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_ROAM_SCAN_STATE and the corresponding
 * frequency info via QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_ROAM_SCAN_FREQ_LIST.
 */
enum qca_wlan_vendor_roam_event_type {
	QCA_WLAN_VENDOR_ROAM_EVENT_TRIGGER_REASON = BIT(0),
	QCA_WLAN_VENDOR_ROAM_EVENT_FAIL_REASON = BIT(1),
	QCA_WLAN_VENDOR_ROAM_EVENT_INVOKE_FAIL_REASON = BIT(2),
	QCA_WLAN_VENDOR_ROAM_EVENT_ROAM_SCAN_STATE = BIT(3),
};

/**
 * enum qca_wlan_vendor_attr_roam_events_candidate_info: Roam candidate info.
 * Referred by QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_BSSID: 6-byte MAC address
 * representing the BSSID of the AP to which the roam is attempted.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_RSSI: Signed 32-bit value
 * in dBm, signifying the RSSI of the candidate BSSID to which the Roaming is
 * attempted.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_FREQ: u32, frequency in MHz
 * on which the roam is attempted.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_FAIL_REASON: u32, used in
 * STA mode only. This represents the roam fail reason for the last failed
 * roaming attempt by the firmware for the specific BSSID. Different roam
 * failure reason codes are specified in enum qca_vendor_roam_fail_reasons.
 */
enum qca_wlan_vendor_attr_roam_events_candidate_info {
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_BSSID = 1,
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_RSSI = 2,
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_FREQ = 3,
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_FAIL_REASON = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_roam_events - Used by the
 * vendor command QCA_NL80211_VENDOR_SUBCMD_ROAM_EVENTS to either configure the
 * roam events to the driver or notify these events from the driver.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CONFIGURE: u8 attribute. Configures the
 * driver/firmware to enable/disable the notification of roam events. It's a
 * mandatory attribute and used only in the request from the userspace to the
 * host driver. 1-Enable, 0-Disable.
 * If the roaming is totally offloaded to the firmware, this request when
 * enabled shall mandate the firmware to notify all the relevant roam events
 * represented by the below attributes. If the host is in the suspend mode,
 * the behavior of the firmware to notify these events is guided by
 * QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_DEVICE_STATE, and if the request is to get
 * these events in the suspend state, the firmware is expected to wake up the
 * host before the respective events are notified. Please note that such a
 * request to get the events in the suspend state will have a definite power
 * implication.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_SUSPEND_STATE: flag attribute. Represents
 * that the roam events need to be notified in the suspend state too. By
 * default, these roam events are notified in the resume state. With this flag,
 * the roam events are notified in both resume and suspend states.
 * This attribute is used in the request from the userspace to the host driver.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_TYPE: u32, used in STA mode only.
 * Represents the different roam event types, signified by the enum
 * qca_wlan_vendor_roam_event_type.
 * Each bit of this attribute represents the different roam even types reported
 * through QCA_NL80211_VENDOR_SUBCMD_ROAM_EVENTS.
 * This is sent as an event through QCA_NL80211_VENDOR_SUBCMD_ROAM_EVENTS.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_TRIGGER_REASON: u32, used in STA
 * mode only. This represents the roam trigger reason for the last roaming
 * attempted by the firmware. Each bit of this attribute represents the
 * different roam trigger reason code which are defined in enum
 * qca_vendor_roam_triggers.
 * This is sent as an event through QCA_NL80211_VENDOR_SUBCMD_ROAM_EVENTS.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_INVOKE_FAIL_REASON: u32, used in
 * STA mode only. This represents the roam invoke fail reason for the last
 * failed roam invoke. Different roam invoke failure reason codes
 * are specified in enum qca_vendor_roam_invoke_fail_reasons.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO: Array of candidates info
 * for which the roam is attempted. Each entry is a nested attribute defined
 * by enum qca_wlan_vendor_attr_roam_events_candidate_info.
 * This is sent as an event through QCA_NL80211_VENDOR_SUBCMD_ROAM_EVENTS.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_ROAM_SCAN_STATE: u8 attribute. Represents
 * the scan state on which the roam events need to be notified. The values for
 * this attribute are referred from enum qca_wlan_vendor_roam_scan_state.
 * This is sent as an event through QCA_NL80211_VENDOR_SUBCMD_ROAM_EVENTS.
 *
 * @QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_ROAM_SCAN_FREQ_LIST: Nested attribute of
 * u32 values. List of frequencies in MHz considered for a roam scan.
 * This is sent as an event through QCA_NL80211_VENDOR_SUBCMD_ROAM_EVENTS.
 */
enum qca_wlan_vendor_attr_roam_events {
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CONFIGURE = 1,
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_SUSPEND_STATE = 2,
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_TYPE = 3,
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_TRIGGER_REASON = 4,
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_INVOKE_FAIL_REASON = 5,
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO = 6,
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_ROAM_SCAN_STATE = 7,
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_ROAM_SCAN_FREQ_LIST = 8,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_MAX =
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_ratemask_params_type - Rate mask config type
 *
 * @QCA_WLAN_RATEMASK_PARAMS_TYPE_CCK_OFDM: CCK/OFDM rate mask config
 * @QCA_WLAN_RATEMASK_PARAMS_TYPE_HT: HT rate mask config
 * @QCA_WLAN_RATEMASK_PARAMS_TYPE_VHT: VHT rate mask config
 * @QCA_WLAN_RATEMASK_PARAMS_TYPE_HE: HE rate mask config
 * @QCA_WLAN_RATEMASK_PARAMS_TYPE_EHT: EHT rate mask config
 */
enum qca_wlan_ratemask_params_type {
	QCA_WLAN_RATEMASK_PARAMS_TYPE_CCK_OFDM = 0,
	QCA_WLAN_RATEMASK_PARAMS_TYPE_HT = 1,
	QCA_WLAN_RATEMASK_PARAMS_TYPE_VHT = 2,
	QCA_WLAN_RATEMASK_PARAMS_TYPE_HE = 3,
	QCA_WLAN_RATEMASK_PARAMS_TYPE_EHT = 4,
};

/**
 * enum qca_wlan_vendor_attr_ratemask_params - Used by the
 * vendor command QCA_NL80211_VENDOR_SUBCMD_RATEMASK_CONFIG.
 * This is used to set the rate mask value to be used in rate selection.
 *
 * @QCA_WLAN_VENDOR_ATTR_RATEMASK_PARAMS_LIST:
 * Array of nested containing attributes
 * QCA_WLAN_VENDOR_ATTR_RATEMASK_PARAMS_TYPE and
 * QCA_WLAN_VENDOR_ATTR_RATEMASK_PARAMS_BITMAP.
 *
 * @QCA_WLAN_VENDOR_ATTR_RATEMASK_PARAMS_TYPE: u8, represents
 * the different PHY types to which the rate mask config is to be applied.
 * The values for this attribute are referred from enum
 * qca_wlan_vendor_ratemask_params_type.
 *
 * @QCA_WLAN_VENDOR_ATTR_RATEMASK_PARAMS_BITMAP: binary, rate mask bitmap.
 * A bit value of 1 represents rate is enabled and a value of 0
 * represents rate is disabled.
 * For EHT targets,
 * b0-1  => NSS1, MCS 14-15
 * b2-15 => NSS1, MCS 0-13
 * b16-29 => NSS2, MCS 0-13
 * For HE targets, 14 bits correspond to one NSS setting.
 * b0-13  => NSS1, MCS 0-13
 * b14-27 => NSS2, MCS 0-13 and so on for other NSS.
 * For VHT targets, 10 bits correspond to one NSS setting.
 * b0-9   => NSS1, MCS 0-9
 * b10-19 => NSS2, MCS 0-9 and so on for other NSS.
 * For HT targets, 8 bits correspond to one NSS setting.
 * b0-7  => NSS1, MCS 0-7
 * b8-15 => NSS2, MCS 0-7 and so on for other NSS.
 * For OFDM/CCK targets, 8 bits correspond to one NSS setting.
 *
 * @QCA_WLAN_VENDOR_ATTR_RATEMASK_PARAMS_LINK_ID: u8, used to specify the
 * MLO link ID of a link to be configured. Optional attribute.
 * No need of this attribute in non-MLO cases. If the attribute is
 * not provided, ratemask will be applied for setup link.
 */
enum qca_wlan_vendor_attr_ratemask_params {
	QCA_WLAN_VENDOR_ATTR_RATEMASK_PARAMS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_RATEMASK_PARAMS_LIST = 1,
	QCA_WLAN_VENDOR_ATTR_RATEMASK_PARAMS_TYPE = 2,
	QCA_WLAN_VENDOR_ATTR_RATEMASK_PARAMS_BITMAP = 3,
	QCA_WLAN_VENDOR_ATTR_RATEMASK_PARAMS_LINK_ID = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_RATEMASK_PARAMS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_RATEMASK_PARAMS_MAX =
	QCA_WLAN_VENDOR_ATTR_RATEMASK_PARAMS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_audio_data_path - Defines the data path to be used for audio
 * traffic.
 *
 * @QCA_WLAN_AUDIO_DATA_PATH_VIA_HOST_PROCESSOR:
 * Send audio traffic through the host processor.
 * @QCA_WLAN_AUDIO_DATA_PATH_VIA_LOW_POWER_DSP:
 * Send audio traffic using the low power DSP to/from the encoder.
 */
enum qca_wlan_audio_data_path {
	QCA_WLAN_AUDIO_DATA_PATH_VIA_HOST_PROCESSOR = 0,
	QCA_WLAN_AUDIO_DATA_PATH_VIA_LOW_POWER_DSP = 1,
};

/**
 * enum qca_wlan_vendor_pasn_action - Action to authenticate (and generate keys
 *	for) or drop existing PASN security association for the listed the
 *	peers. Used by QCA_WLAN_VENDOR_ATTR_PASN_ACTION and sent from the driver
 *	to userspace.
 *
 * @QCA_WLAN_VENDOR_PASN_ACTION_AUTH: Initiate PASN handshake with the peer
 *	devices indicated with %QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAC_ADDR.
 * @QCA_WLAN_VENDOR_PASN_ACTION_DELETE_SECURE_RANGING_CONTEXT: Indication from
 *	the driver to userspace to inform that the existing PASN keys of the
 *	peer devices specified with %QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAC_ADDR are
 *	not valid anymore.
 */
enum qca_wlan_vendor_pasn_action {
	QCA_WLAN_VENDOR_PASN_ACTION_AUTH,
	QCA_WLAN_VENDOR_PASN_ACTION_DELETE_SECURE_RANGING_CONTEXT,
};

/**
 * enum qca_wlan_vendor_attr_pasn_peer: Defines the nested attributes used in
 *	QCA_WLAN_VENDOR_ATTR_PASN_PEERS.
 *
 * @QCA_WLAN_VENDOR_ATTR_PASN_PEER_SRC_ADDR: This attribute is optional in the
 *	event from the driver to userspace and represents the local MAC address
 *	to be used for PASN handshake. When this attribute is present, userspace
 *	shall use the source address specified in this attribute by the driver
 *	for PASN handshake with peer device.
 *	This attribute is required in a command response from userspace to the
 *	driver and represents the MAC address that was used in PASN handshake
 *	with the peer device.
 * @QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAC_ADDR: Indicates the MAC address of the
 *	peer device to which PASN handshake is requested in an event from the
 *	driver to userspace when QCA_WLAN_VENDOR_ATTR_PASN_ACTION is set to
 *	QCA_WLAN_VENDOR_PASN_ACTION_AUTH.
 *	Indicates the MAC address of the peer device for which the keys are to
 *	be invalidated in an event from the driver to userspace when
 *	QCA_WLAN_VENDOR_ATTR_PASN_ACTION is set to
 *	QCA_WLAN_VENDOR_PASN_ACTION_DELETE_SECURE_RANGING_CONTEXT.
 *	Indicates the MAC address of the peer device for which the status is
 *	being sent in a status report from userspace to the driver.
 * @QCA_WLAN_VENDOR_ATTR_PASN_PEER_LTF_KEYSEED_REQUIRED: NLA_FLAG attribute used
 *	in the event from the driver to userspace. When set, userspace is
 *	required to derive LTF key seed from KDK and set it to the driver.
 * @QCA_WLAN_VENDOR_ATTR_PASN_PEER_STATUS_SUCCESS: NLA_FLAG attribute. This
 *	attribute is used in the command response from userspace to the driver.
 *	If present, it indicates the successful PASN handshake with the peer. If
 *	this flag is not present, it indicates that the PASN handshake with the
 *	peer device failed.
 */
enum qca_wlan_vendor_attr_pasn_peer {
	QCA_WLAN_VENDOR_ATTR_PASN_PEER_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_PASN_PEER_SRC_ADDR = 1,
	QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAC_ADDR = 2,
	QCA_WLAN_VENDOR_ATTR_PASN_PEER_LTF_KEYSEED_REQUIRED = 3,
	QCA_WLAN_VENDOR_ATTR_PASN_PEER_STATUS_SUCCESS = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_PASN_PEER_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAX =
	QCA_WLAN_VENDOR_ATTR_PASN_PEER_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_pasn: Defines the attributes used in the
 *	QCA_NL80211_VENDOR_SUBCMD_PASN command.
 *
 * @QCA_WLAN_VENDOR_ATTR_PASN_ACTION: u32 attribute, possible values are
 *	defined in enum qca_wlan_vendor_pasn_action and used only in an event
 *	from the driver to userspace.
 * @QCA_WLAN_VENDOR_ATTR_PASN_PEERS: Nested attribute, used to pass PASN peer
 *	details for each peer and used in both an event and a command response.
 *	The nested attributes used inside QCA_WLAN_VENDOR_ATTR_PASN_PEERS are
 *	defined in enum qca_wlan_vendor_attr_pasn_peer.
 * @QCA_WLAN_VENDOR_ATTR_PASN_LINK_ID: u8 attribute used to identify a
 *	specific link affiliated to an MLD.
 */
enum qca_wlan_vendor_attr_pasn {
	QCA_WLAN_VENDOR_ATTR_PASN_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_PASN_ACTION = 1,
	QCA_WLAN_VENDOR_ATTR_PASN_PEERS = 2,
	QCA_WLAN_VENDOR_ATTR_PASN_LINK_ID = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_PASN_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_PASN_MAX =
	QCA_WLAN_VENDOR_ATTR_PASN_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_secure_ranging_ctx_action - Used to add or delete
 *	the ranging security context derived from PASN for each peer. Used in
 *	QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_ACTION.
 *
 * @QCA_WLAN_VENDOR_SECURE_RANGING_CTX_ACTION_ADD: Add the secure ranging
 *	context for the peer.
 * @QCA_WLAN_VENDOR_SECURE_RANGING_CTX_ACTION_DELETE: Delete the secure ranging
 *	context for the peer.
 */
enum qca_wlan_vendor_secure_ranging_ctx_action {
	QCA_WLAN_VENDOR_SECURE_RANGING_CTX_ACTION_ADD,
	QCA_WLAN_VENDOR_SECURE_RANGING_CTX_ACTION_DELETE,
};

/**
 * enum qca_wlan_vendor_sha_type - SHA types. Used to configure the SHA type
 *	used for deriving PASN keys to the driver. Used in
 *	QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_SHA_TYPE
 * @QCA_WLAN_VENDOR_SHA_256: SHA-256
 * @QCA_WLAN_VENDOR_SHA_384: SHA-384
 */
enum qca_wlan_vendor_sha_type {
	QCA_WLAN_VENDOR_SHA_256,
	QCA_WLAN_VENDOR_SHA_384,
};

/**
 * enum qca_wlan_vendor_attr_secure_ranging_ctx: Defines the attributes used
 *	to set security context for the PASN peer from userspace to the driver.
 *	Used with QCA_NL80211_VENDOR_SUBCMD_SECURE_RANGING_CONTEXT.
 *
 * @QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_ACTION: u32 attribute, possible
 *	values are defined in enum qca_wlan_vendor_secure_ranging_ctx_action
 * @QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_SRC_ADDR: The local MAC address that
 *	was used during the PASN handshake.
 * @QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_PEER_MAC_ADDR: The MAC address of
 *	the peer device for which secure ranging context is being configured.
 * @QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_SHA_TYPE: u32 attribute, defines the
 *	hash algorithm to be used, possible values are defined in enum
 *	qca_wlan_vendor_sha_type.
 * @QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_TK: Variable length attribute, holds
 *	the temporal key generated from the PASN handshake. The length of this
 *	attribute is dependent on the value of
 *	%QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_CIPHER.
 * @QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_CIPHER: cipher suite to use with the
 *	TK, u32, as defined in IEEE Std 802.11-2020, 9.4.2.24.2 (Cipher suites)
 *	(e.g., 0x000FAC04).
 * @QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_LTF_KEYSEED: Variable length
 *	attribute, holds the LTF keyseed derived from KDK of PASN handshake.
 *	The length of this attribute is dependent on the value of
 *	%QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_SHA_TYPE.
 * @QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_LINK_ID: This u8 attribute is used
 *	for secure ranging to identify a specific link affiliated to an AP MLD.
 */
enum qca_wlan_vendor_attr_secure_ranging_ctx {
	QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_ACTION = 1,
	QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_SRC_ADDR = 2,
	QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_PEER_MAC_ADDR = 3,
	QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_SHA_TYPE = 4,
	QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_TK = 5,
	QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_CIPHER = 6,
	QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_LTF_KEYSEED = 7,
	QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_LINK_ID = 8,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_MAX =
	QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_coap_offload_filter - Attributes used
 * inside %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY_FILTER
 * nested attribute. The packets that match a filter will be replied with
 * attributes configured in %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_DEST_IPV4:
 * u32 attribute. Destination IPv4 address in network byte order, the
 * IPv4 packets with different address will be filtered out.
 * This attribute is optional.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_DEST_IPV4_IS_BC:
 * Flag attribute. If it's present, indicates that
 * %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_DEST_IPV4 is a broadcast
 * address; while if not, indicates that the address is a unicast/multicast
 * address.
 * This attribute is optional.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_DEST_IPV6:
 * NLA_BINARY attribute, length is 16 bytes.
 * Destination IPv6 address in network byte order, the IPv6 packets
 * with different destination address will be filtered out.
 * This attribute is optional.
 *
 * At least one of %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_DEST_IPV4 and
 * %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_DEST_IPV6 must be configured.
 * Packets on both IPv4 and IPv6 will be processed if both are configured.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_DEST_PORT:
 * u16 attribute. Destination UDP port, the packets with different destination
 * UDP port will be filtered out.
 * This attribute is mandatory.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_MATCH_OFFSET:
 * u32 attribute. Represents the offset (in UDP payload) of the data
 * to be matched.
 * This attribute is mandatory.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_MATCH_DATA:
 * NLA_BINARY attribute, the maximum allowed size is 16 bytes.
 * Binary data that is compared bit-by-bit against the data (specified
 * by %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_MATCH_OFFSET) in UDP
 * payload, the packets don't match will be filtered out.
 * This attribute is mandatory.
 */
enum qca_wlan_vendor_attr_coap_offload_filter {
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_DEST_IPV4 = 1,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_DEST_IPV4_IS_BC = 2,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_DEST_IPV6 = 3,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_DEST_PORT = 4,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_MATCH_OFFSET = 5,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_MATCH_DATA = 6,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_MAX =
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_FILTER_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_coap_offload_reply - Attributes used
 * inside %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY nested attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY_SRC_IPV4:
 * u32 attribute. Source address (in network byte order) for replying
 * the matching broadcast/multicast IPv4 packets.
 * This attribute is optional.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY_SRC_IPV6:
 * NLA_BINARY attribute, length is 16 bytes.
 * Source address (in network byte order) for replying the matching
 * multicast IPv6 packets.
 * This attribute is optional.
 *
 * For broadcast/multicast offload reply, one of
 * %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY_SRC_IPV4 and
 * %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY_SRC_IPV6 or both must be
 * configured according to version of the IP address(es) configured in
 * %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY_FILTER;
 * while for unicast case, firmware will take the destination IP address
 * in the received matching packet as the source address for replying.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY_FILTER:
 * Nested attribute. Filter for the received UDP packets, only the matching
 * packets will be replied and cached.
 * See enum qca_wlan_vendor_attr_coap_offload_filter for list of supported
 * attributes.
 * This attribute is mandatory.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY_MSG:
 * NLA_BINARY attribute, the maximum allowed size is 1152 bytes.
 * CoAP message (UDP payload) to be sent upon receiving matching packets.
 * Firmware is responsible for adding any necessary protocol headers.
 * This attribute is mandatory.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY_CACHE_EXPTIME:
 * u32 attribute. Expiration time in milliseconds of the cached CoAP messages.
 * A cached message will be dropped by firmware if it's expired.
 * This attribute is optional. A default value of 40000 will be used in the
 * absence of it.
 */
enum qca_wlan_vendor_attr_coap_offload_reply {
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY_SRC_IPV4 = 1,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY_SRC_IPV6 = 2,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY_FILTER = 3,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY_MSG = 4,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY_CACHE_EXPTIME = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY_MAX =
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_coap_offload_tx_ipv4 - Represents parameters for
 * CoAP message (UDP) transmitting on IPv4.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV4_SRC_ADDR:
 * u32 attribute. Source address (in network byte order) for transmitting
 * packets on IPv4.
 * This attribute is mandatory.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV4_SRC_PORT:
 * u16 attribute. Source UDP port.
 * This attribute is optional, a random port is taken if it's not present.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV4_DEST_ADDR:
 * u32 attribute. Destination IPv4 address (in network byte order).
 * This attribute is mandatory.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV4_DEST_IS_BC:
 * Flag attribute. If it's present, indicates that
 * %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV4_DEST_ADDR is a broadcast
 * address; while if not, indicates that the address is unicast/multicast
 * address.
 * This attribute is optional.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV4_DEST_PORT:
 * u16 attribute. Destination UDP port.
 * This attribute is mandatory.
 */
enum qca_wlan_vendor_attr_coap_offload_tx_ipv4 {
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV4_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV4_SRC_ADDR = 1,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV4_SRC_PORT = 2,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV4_DEST_ADDR = 3,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV4_DEST_IS_BC = 4,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV4_DEST_PORT = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV4_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV4_MAX =
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV4_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_coap_offload_tx_ipv6 - Represents parameters for
 * CoAP message (UDP) transmitting on IPv6.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV6_SRC_ADDR:
 * NLA_BINARY attribute, length is 16 bytes.
 * Source address (in network byte order) for transmitting packets on IPv6.
 * This attribute is mandatory.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV6_SRC_PORT:
 * u16 attribute. Source UDP port.
 * This attribute is optional, a random port is taken if it's not present.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV6_DEST_ADDR:
 * NLA_BINARY attribute, length is 16 bytes.
 * Destination IPv6 address (in network byte order).
 * This attribute is mandatory.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV6_DEST_PORT:
 * u16 attribute. Destination UDP port.
 * This attribute is mandatory.
 */
enum qca_wlan_vendor_attr_coap_offload_tx_ipv6 {
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV6_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV6_SRC_ADDR = 1,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV6_SRC_PORT = 2,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV6_DEST_ADDR = 3,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV6_DEST_PORT = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV6_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV6_MAX =
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_TX_IPV6_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_coap_offload_periodic_tx - Attributes used
 * inside %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_PERIODIC_TX nested attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_PERIODIC_TX_IPV4:
 * Nested attribute. The IPv4 source/destination address/port for offload
 * transmitting. See enum qca_wlan_vendor_attr_coap_offload_tx_ipv4 for the list
 * of supported attributes.
 * This attribute is optional.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_PERIODIC_TX_IPV6:
 * Nested attribute. The IPv6 source/destination address/port for offload
 * transmitting. See enum qca_wlan_vendor_attr_coap_offload_tx_ipv6 for the list
 * of supported attributes.
 * This attribute is optional.
 *
 * At least one of %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_PERIODIC_TX_IPV4 and
 * %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_PERIODIC_TX_IPV6 must be configured.
 * Firmware will transmit the packets on both IPv4 and IPv6 if both are
 * configured.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_PERIODIC_TX_PERIOD:
 * u32 attribute. Period in milliseconds for the periodic transmitting.
 * This attribute is mandatory.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_PERIODIC_TX_MSG:
 * NLA_BINARY attribute, the maximum allowed size is 1152 bytes.
 * CoAP message (UDP payload) to be periodically transmitted. Firmware
 * is responsible for adding any necessary protocol headers.
 * This attribute is mandatory.
 */
enum qca_wlan_vendor_attr_coap_offload_periodic_tx {
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_PERIODIC_TX_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_PERIODIC_TX_IPV4 = 1,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_PERIODIC_TX_IPV6 = 2,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_PERIODIC_TX_PERIOD = 3,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_PERIODIC_TX_MSG = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_PERIODIC_TX_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_PERIODIC_TX_MAX =
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_PERIODIC_TX_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_coap_offload_cache_info - Attributes used
 * inside %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHES nested attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHE_INFO_TS:
 * u64 attribute. Received time (since system booted in microseconds) of
 * the cached CoAP message.
 * This attribute is mandatory.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHE_INFO_SRC_IPV4:
 * u32 attribute. Source IPv4 address (in network byte order) of the cached
 * CoAP message.
 * This attribute is optional.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHE_INFO_SRC_IPV6:
 * NLA_BINARY attribute, length is 16 bytes.
 * Source IPv6 address (in network byte order) of the cached CoAP message.
 * This attribute is optional.
 *
 * At most and at least one of
 * %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHE_INFO_SRC_IPV4 and
 * %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHE_INFO_SRC_IPV6 is given for
 * an entry.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHE_INFO_MSG:
 * NLA_BINARY attribute, the maximum allowed size is 1152 bytes.
 * The cached CoAP message (UDP payload). If the actual message size is
 * greater than the maximum size, it will be truncated and leaving only
 * the first 1152 bytes.
 * This attribute is mandatory.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHE_INFO_PAD: Attribute used for
 * padding for 64-bit alignment
 */
enum qca_wlan_vendor_attr_coap_offload_cache_info {
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHE_INFO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHE_INFO_TS = 1,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHE_INFO_SRC_IPV4 = 2,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHE_INFO_SRC_IPV6 = 3,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHE_INFO_MSG = 4,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHE_INFO_PAD = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHE_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHE_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHE_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_coap_offload_action - Actions for
 * vendor command QCA_NL80211_VENDOR_SUBCMD_COAP_OFFLOAD.
 *
 * @QCA_WLAN_VENDOR_COAP_OFFLOAD_ACTION_REPLY_ENABLE:
 * Enable CoAP offload reply.
 * If it's enabled, firmware will start offload processing on each suspend
 * and stop on each resume.
 *
 * Offload reply on match works as follows:
 * Reply the packets that match the filter with the given CoAP
 * message (with necessary protocol headers), increase the CoAP message
 * ID in the given CoAP message by one for the next use after each successful
 * transmission, and try to store the information of the received packet,
 * including the received time, source IP address, and CoAP message (UDP
 * payload).
 *
 * Firmware has a limit to the maximum stored entries, it takes the source IP
 * address as the key of an entry, and keeps at most one entry for each key.
 * A packet won't be stored if no entry for the same key is present and the
 * total number of the existing unexpired entries reaches the maximum value.
 *
 * If any configured item is changed, user space should disable offload reply
 * first and then issue a new enable request.
 *
 * @QCA_WLAN_VENDOR_COAP_OFFLOAD_ACTION_REPLY_DISABLE:
 * Disable CoAP offload reply and return information of any cached CoAP
 * messages.
 *
 * @QCA_WLAN_VENDOR_COAP_OFFLOAD_ACTION_PERIODIC_TX_ENABLE:
 * Enable CoAP offload periodic transmitting.
 * If it's enabled, firmware will start offload periodic transmitting on
 * each suspend and stop on each resume.
 *
 * Offload periodic transmitting works as follows:
 * Send the given CoAP message (with necessary protocol headers) with the given
 * source/destination IP address/UDP port periodically based on the given
 * period and increase the CoAP message ID in the given CoAP message by one
 * for the next use after each successful transmission.
 *
 * If any configured item is changed, user space should disable offload
 * periodic transmitting first and then issue a new enable request.
 *
 * @QCA_WLAN_VENDOR_COAP_OFFLOAD_ACTION_PERIODIC_TX_DISABLE:
 * Disable CoAP offload periodic transmitting.
 *
 * @QCA_WLAN_VENDOR_COAP_OFFLOAD_ACTION_CACHE_GET:
 * Get information of the CoAP messages cached during offload reply
 * processing. The cache is cleared after retrieval.
 */
enum qca_wlan_vendor_coap_offload_action {
	QCA_WLAN_VENDOR_COAP_OFFLOAD_ACTION_REPLY_ENABLE = 0,
	QCA_WLAN_VENDOR_COAP_OFFLOAD_ACTION_REPLY_DISABLE = 1,
	QCA_WLAN_VENDOR_COAP_OFFLOAD_ACTION_PERIODIC_TX_ENABLE = 2,
	QCA_WLAN_VENDOR_COAP_OFFLOAD_ACTION_PERIODIC_TX_DISABLE = 3,
	QCA_WLAN_VENDOR_COAP_OFFLOAD_ACTION_CACHE_GET = 4,
};

/**
 * enum qca_wlan_vendor_attr_coap_offload - Used by the
 * vendor command QCA_NL80211_VENDOR_SUBCMD_COAP_OFFLOAD.
 * This is used to set parameters for CoAP offload processing, or get
 * cached CoAP messages from firmware.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_ACTION:
 * u32 attribute. Action to take in this vendor command.
 * See enum qca_wlan_vendor_coap_offload_action for supported actions.
 * This attribute is mandatory.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REQ_ID:
 * u32 attribute. Represents the Request ID for the CoAP offload
 * configuration, which can help to identify the user entity starting
 * the CoAP offload processing and accordingly stop the respective
 * ones/get the cached CoAP messages with the matching ID.
 * This attribute is mandatory.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY:
 * Nested attribute. Parameters for offload reply.
 * See enum qca_wlan_vendor_attr_coap_offload_reply for the list of
 * supported attributes.
 * This attribute is mandatory if %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_ACTION
 * is QCA_WLAN_VENDOR_COAP_OFFLOAD_ACTION_REPLY_ENABLE, and is ignored
 * otherwise.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_PERIODIC_TX:
 * Nested attribute. Parameters for offload periodic transmitting.
 * See enum qca_wlan_vendor_attr_coap_offload_periodic_tx for the list of
 * supported attributes.
 * This attribute is mandatory if %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_ACTION is
 * QCA_WLAN_VENDOR_COAP_OFFLOAD_ACTION_PERIODIC_TX_ENABLE, and is ignored
 * otherwise.
 *
 * @QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHES:
 * Array of nested attributes. Information of the cached CoAP messages,
 * where each entry is taken from
 * enum qca_wlan_vendor_attr_coap_offload_cache_info.
 * This attribute is used for reporting the cached CoAP messages
 * in reply for command in which %QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_ACTION
 * is QCA_WLAN_VENDOR_COAP_OFFLOAD_ACTION_CACHE_GET or
 * QCA_WLAN_VENDOR_COAP_OFFLOAD_ACTION_REPLY_DISABLE. It means there is no
 * cached item if this attribute is not present.
 */
enum qca_wlan_vendor_attr_coap_offload {
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_ACTION = 1,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REQ_ID = 2,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_REPLY = 3,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_PERIODIC_TX = 4,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_CACHES = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_MAX =
	QCA_WLAN_VENDOR_ATTR_COAP_OFFLOAD_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_scs_rule_config - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG to configure Stream Classification
 * Service (SCS) rule.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_RULE_ID: Mandatory u32 attribute.
 * Represents the unique id of SCS rule to be configured.

 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_REQUEST_TYPE: Mandatory u8 attribute.
 * Represents the request type: add, remove, or change.
 * Values as defined in IEEE Std 802.11-2020, Table 9-246 (SCS Request
 * Type definitions).
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_OUTPUT_TID: Mandatory u8 attribute
 * in case of add/change request type.
 * Represents the output traffic identifier (TID) to be assigned to the flow
 * matching the rule.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_CLASSIFIER_TYPE: Mandatory u8
 * attribute in case of add/change request type.
 * Represents type of classifier parameters present in SCS rule.
 * Refer IEEE Std 802.11-2020 Table 9-164 (Frame classifier type).
 * Only classifier types 4 and 10 are supported for SCS.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_VERSION: Mandatory u8 attribute
 * in case of add/change request type when classifier type is TCLAS4.
 * Represents the IP version (4: IPv4, 6: IPv6) of the rule.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV4_ADDR: Optional
 * attribute in case of add/change request type when classifier type is TCLAS4
 * and version attribute is IPv4.
 * Represents the source IPv4 address in the rule which is to be compared
 * against the source IP address in the IPv4 header.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV4_ADDR: Optional
 * attribute in case of add/change request type when classifier type is TCLAS4
 * and version attribute is IPv4.
 * Represents the destination IPv4 address in the rule which is to be compared
 * against the destination IP address in the IPv4 header.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV6_ADDR: Optional
 * attribute in case of add/change request type when classifier type is TCLAS4
 * and version attribute is IPv6.
 * Represents the source IPv6 address in the rule which is to be compared
 * against the source IP address in the IPv6 header.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV6_ADDR: Optional
 * attribute in case of add/change request type when classifier type is TCLAS4
 * and version attribute is IPv6.
 * Represents the destination IPv6 address in the rule which is to be compared
 * against the destination IP address in the IPv6 header.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_PORT: Optional u16 attribute
 * in case of add/change request type when classifier type is TCLAS4.
 * Represents the source port number in the rule which is to be compared against
 * the source port number in the protocol header.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_PORT: Optional u16 attribute
 * in case of add/change request type when classifier type is TCLAS4.
 * Represents the destination port number in the rule which is to be compared
 * against the destination port number in the protocol header.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DSCP: Optional u8 attribute
 * in case of add/change request type when classifier type is TCLAS4.
 * Represents the DSCP value in the rule which is to be compared against the
 * DSCP field present in the IP header.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_NEXT_HEADER: Optional u8
 * attribute in case of add/change request type when classifier type is TCLAS4.
 * Represents the protocol/next header in the rule which is to be compared
 * against the protocol/next header field present in the IPv4/IPv6 header.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_FLOW_LABEL: Optional
 * attribute of size 3 bytes present in case of add/change request type
 * when classifier type is TCLAS4 and version is IPv6.
 * Represents the flow label value in the rule which is to be compared against
 * the flow label field present in the IPv6 header.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_PROTOCOL_INSTANCE: Optional u8
 * attribute in case of add/change request type when classifier type is TCLAS10.
 * Represents the protocol instance number in the rule.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_NEXT_HEADER: Optional u8
 * attribute in case of add/change request type when classifier type is TCLAS10.
 * Represents the protocol/next header in the rule which is to be compared
 * against the protocol/next header field present in the IPv4/IPv6 header.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_MASK: Optional
 * attribute of variable length present when request type is add/change and
 * classifier type is TCLAS10.
 * Represents the mask to be used for masking the header contents of the header
 * specified by QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_NEXT_HEADER
 * attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_VALUE: Optional
 * attribute of variable length present when request type is add/change and
 * classifier type is TCLAS10.
 * Represents the value to be compared against after masking the header contents
 * of the header specified by the
 * QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_NEXT_HEADER attribute with the
 * filter mask specified by the
 * QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_MASK attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_SERVICE_CLASS_ID: Optional u16
 * attribute.
 * Represents the service class id of the configured SCS rule.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_DST_MAC_ADDR: Optional 6 bytes
 * MAC address.
 * Represents the destination MAC address in the rule.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_NETDEV_IF_INDEX: Optional u32 attribute
 * Represents the netdevice interface index in the rule.
 */
enum qca_wlan_vendor_attr_scs_rule_config {
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_RULE_ID = 1,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_REQUEST_TYPE = 2,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_OUTPUT_TID = 3,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_CLASSIFIER_TYPE = 4,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_VERSION = 5,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV4_ADDR = 6,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV4_ADDR = 7,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV6_ADDR = 8,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV6_ADDR = 9,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_PORT = 10,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_PORT = 11,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DSCP = 12,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_NEXT_HEADER = 13,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_FLOW_LABEL = 14,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_PROTOCOL_INSTANCE = 15,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_NEXT_HEADER = 16,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_MASK = 17,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_VALUE = 18,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_SERVICE_CLASS_ID = 19,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_DST_MAC_ADDR = 20,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_NETDEV_IF_INDEX = 21,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_MAX =
	QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_mlo_links - Definition of attributes used inside
 * nested attribute QCA_WLAN_VENDOR_ATTR_ROAM_AUTH_MLO_LINKS.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_LINK_ID: u8 attribute, link ID of this MLO link.
 * @QCA_WLAN_VENDOR_ATTR_MLO_LINK_MAC_ADDR: Own MAC address of this MLO link.
 * @QCA_WLAN_VENDOR_ATTR_MLO_LINK_BSSID: AP link MAC address of this MLO link.
 */
enum qca_wlan_vendor_attr_mlo_links {
	QCA_WLAN_VENDOR_ATTR_MLO_LINK_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_MLO_LINK_ID = 1,
	QCA_WLAN_VENDOR_ATTR_MLO_LINK_MAC_ADDR = 2,
	QCA_WLAN_VENDOR_ATTR_MLO_LINK_BSSID = 3,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_MLO_LINK_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_MLO_LINK_MAX =
	QCA_WLAN_VENDOR_ATTR_MLO_LINK_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_sar_version - This describes the current SAR version
 * used in the firmware.
 *
 * @QCA_WLAN_VENDOR_SAR_VERSION_1: The firmware supports legacy SAR.
 * In legacy SAR, the firmware supports 5 static and 1 user defined SAR limits.
 *
 * @QCA_WLAN_VENDOR_SAR_VERSION_2: The firmware supports SAR version 2,
 * i.e., SAR Non DBS mode. In SAR version 2, the firmware has 6 SAR tables for
 * each CTL group. So user can select up to 6 SAR indexes from the current CTL
 * groups.
 *
 * @QCA_WLAN_VENDOR_SAR_VERSION_3: The firmware supports SAR version 3,
 * i.e., SAR DBS mode. In SAR version 3, the firmware has 6 SAR tables for each
 * CTL group but user can choose up to 3 SAR set index only, as the top half
 * of the SAR index (0 to 2) is used for non DBS purpose and the bottom half of
 * the SAR index (3 to 5) is used for DBS mode.
 *
 * @QCA_WLAN_VENDOR_SAR_VERSION_4: The firmware supports SAR version 4,
 * known as SAR Smart Transmit (STX) mode. STX is time averaging algorithmic
 * for power limit computation in collaboration with WWAN.
 * In STX mode, firmware has 41 indexes and there is no ctl grouping uses.
 *
 * @QCA_WLAN_VENDOR_SAR_VERSION_5: The firmware supports SAR version 5,
 * known as TAS (Time Averaging SAR) mode. In TAS mode, as name implies
 * instead of fixed static SAR power limit firmware uses time averaging
 * to adjust the SAR limit dynamically. It is wlan soc standalone mechanism.
 * In this mode firmware has up to 43 indexes.
 */
enum qca_wlan_vendor_sar_version {
	QCA_WLAN_VENDOR_SAR_VERSION_INVALID = 0,
	QCA_WLAN_VENDOR_SAR_VERSION_1 = 1,
	QCA_WLAN_VENDOR_SAR_VERSION_2 = 2,
	QCA_WLAN_VENDOR_SAR_VERSION_3 = 3,
	QCA_WLAN_VENDOR_SAR_VERSION_4 = 4,
	QCA_WLAN_VENDOR_SAR_VERSION_5 = 5,
};

/**
 * enum qca_wlan_vendor_sar_ctl_group_state - This describes whether
 * CTL grouping is enabled or disabled in the firmware.
 *
 * @QCA_WLAN_VENDOR_SAR_CTL_GROUP_STATE_ENABLED: CTL grouping
 * is enabled in firmware.
 *
 * @QCA_WLAN_VENDOR_SAR_CTL_GROUP_STATE_DISABLED: CTL grouping
 * is disabled in firmware.
 *
 */
enum qca_wlan_vendor_sar_ctl_group_state {
	QCA_WLAN_VENDOR_SAR_CTL_GROUP_STATE_INVALID = 0,
	QCA_WLAN_VENDOR_SAR_CTL_GROUP_STATE_ENABLED = 1,
	QCA_WLAN_VENDOR_SAR_CTL_GROUP_STATE_DISABLED = 2,
};

/**
 * enum qca_wlan_vendor_attr_sar_capability - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_GET_SAR_CAPABILITY to get SAR capabilities
 * supported by the firmware.

 * @QCA_WLAN_VENDOR_ATTR_SAR_CAPABILITY_VERSION:
 * u32 attribute. This field describes current SAR version supported by the
 * firmware.
 * See enum qca_wlan_vendor_sar_version for more information.
 * This attribute is mandatory.

 * @QCA_WLAN_VENDOR_ATTR_SAR_CAPABILITY_CTL_GROUP_STATE:
 * u32 attribute. This field describes whether CTL groups are enabled
 * or disabled in the firmware.
 * See enum qca_wlan_vendor_sar_ctl_group_state for more information.
 * This attribute is optional.
 */

enum qca_wlan_vendor_attr_sar_capability {
	QCA_WLAN_VENDOR_ATTR_SAR_CAPABILITY_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SAR_CAPABILITY_VERSION = 1,
	QCA_WLAN_VENDOR_ATTR_SAR_CAPABILITY_CTL_GROUP_STATE = 2,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_SAR_CAPABILITY_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SAR_CAPABILITY_MAX =
	QCA_WLAN_VENDOR_ATTR_SAR_CAPABILITY_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_sr_stats - Attributes for Spatial Reuse statistics.
 * These statistics are sent from the driver in a response when userspace
 * queries to get the statistics using the operation
 * %QCA_WLAN_SR_OPERATION_GET_STATS. These statistics are reset
 * by the driver when the SR feature is enabled, when the driver receives
 * %QCA_WLAN_SR_OPERATION_CLEAR_STATS operation, or when disconnected.
 *
 * @QCA_WLAN_VENDOR_ATTR_SR_STATS_NON_SRG_TX_OPPORTUNITIES_COUNT: u32 attribute.
 * Mandatory only when non-SRG is supported by the AP and optional otherwise.
 * This represents the number of non-SRG TX opportunities.
 *
 * @QCA_WLAN_VENDOR_ATTR_SR_STATS_NON_SRG_TX_PPDU_TRIED_COUNT: u32 attribute.
 * Mandatory only when non-SRG is supported by the AP and optional otherwise.
 * This represents the number of non-SRG PPDUs tried to transmit.
 *
 * @QCA_WLAN_VENDOR_ATTR_SR_STATS_NON_SRG_TX_PPDU_SUCCESS_COUNT: u32 attribute.
 * Mandatory only when non-SRG is supported by the AP and optional otherwise.
 * This represents the number of non-SRG PPDUs successfully transmitted.
 *
 * @QCA_WLAN_VENDOR_ATTR_SR_STATS_SRG_TX_OPPORTUNITIES_COUNT: u32 attribute.
 * Mandatory only when SRG is supported by the AP and optional otherwise.
 * This represents the number of SRG TX opportunities.
 *
 * @QCA_WLAN_VENDOR_ATTR_SR_STATS_SRG_TX_PPDU_TRIED_COUNT: u32 attribute.
 * Mandatory only when SRG is supported by the AP and optional otherwise.
 * This represents the number of SRG PPDUs tried to transmit.
 *
 * @QCA_WLAN_VENDOR_ATTR_SR_STATS_SRG_TX_PPDU_SUCCESS_COUNT: u32 attribute.
 * Mandatory only when SRG is supported by the AP and optional otherwise.
 * This represents the number of SRG PPDUs successfully transmitted.
 */
enum qca_wlan_vendor_attr_sr_stats {
	QCA_WLAN_VENDOR_ATTR_SR_STATS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SR_STATS_NON_SRG_TX_OPPORTUNITIES_COUNT = 1,
	QCA_WLAN_VENDOR_ATTR_SR_STATS_NON_SRG_TX_PPDU_TRIED_COUNT = 2,
	QCA_WLAN_VENDOR_ATTR_SR_STATS_NON_SRG_TX_PPDU_SUCCESS_COUNT = 3,
	QCA_WLAN_VENDOR_ATTR_SR_STATS_SRG_TX_OPPORTUNITIES_COUNT = 4,
	QCA_WLAN_VENDOR_ATTR_SR_STATS_SRG_TX_PPDU_TRIED_COUNT = 5,
	QCA_WLAN_VENDOR_ATTR_SR_STATS_SRG_TX_PPDU_SUCCESS_COUNT = 6,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_SR_STATS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SR_STATS_MAX =
	QCA_WLAN_VENDOR_ATTR_SR_STATS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_sr_reason_code - Defines the different reason codes used in
 * Spatial Reuse feature.
 *
 * @QCA_WLAN_SR_REASON_CODE_ROAMING: The SR feature is disabled/enabled due to
 * roaming to an AP that doesn't support/supports SR feature, respectively.
 *
 * @QCA_WLAN_SR_REASON_CODE_CONCURRENCY: The SR feature is disabled/enabled due
 * to change in concurrent interfaces that are supported by the driver.
 */
enum qca_wlan_sr_reason_code {
	QCA_WLAN_SR_REASON_CODE_ROAMING = 0,
	QCA_WLAN_SR_REASON_CODE_CONCURRENCY = 1,
};

/**
 * enum qca_wlan_sr_operation - Defines the different types of SR operations.
 * The values are used inside attribute %QCA_WLAN_VENDOR_ATTR_SR_OPERATION.
 *
 * @QCA_WLAN_SR_OPERATION_SR_ENABLE: Userspace sends this operation to the
 * driver to enable the Spatial Reuse feature. Attributes
 * %QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_PD_THRESHOLD and
 * %QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_PD_THRESHOLD are used with this
 * operation.
 *
 * @QCA_WLAN_SR_OPERATION_SR_DISABLE: Userspace sends this operation to the
 * driver to disable the Spatial Reuse feature.
 *
 * @QCA_WLAN_SR_OPERATION_SR_SUSPEND: The driver uses this operation in an
 * asynchronous event sent to userspace when the SR feature is disabled.
 * The disable reason is encoded in QCA_WLAN_VENDOR_ATTR_SR_PARAMS_REASON_CODE
 * and sent along with the asynchronous event.
 *
 * @QCA_WLAN_SR_OPERATION_SR_RESUME: The driver uses this operation in an
 * asynchronous event when the SR feature is enabled again after the SR feature
 * was suspended by the driver earlier. The enable reason is
 * encoded in QCA_WLAN_VENDOR_ATTR_SR_PARAMS_REASON_CODE. Attributes used are
 * %QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_PD_THRESHOLD and
 * %QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_PD_THRESHOLD.
 *
 * @QCA_WLAN_SR_OPERATION_PSR_AND_NON_SRG_OBSS_PD_PROHIBIT: This operation is
 * used to prohibit PSR-based spatial reuse and non-SRG OBSS PD-based spatial
 * reuse transmissions. Userspace sends this operation to the driver.
 * The driver/firmware upon receiving this operation shall prohibit PSR-based
 * spatial reuse and non-SRG OBSS PD-based spatial reuse transmissions.
 *
 * @QCA_WLAN_SR_OPERATION_PSR_AND_NON_SRG_OBSS_PD_ALLOW: This operation is
 * used to allow PSR-based spatial reuse and non-SRG OBSS PD-based spatial
 * reuse transmissions. Userspace sends this operation to the driver.
 * The driver/firmware upon receiving this operation shall allow PSR-based
 * spatial reuse and non-SRG OBSS PD-based spatial reuse transmissions.
 *
 * @QCA_WLAN_SR_OPERATION_GET_STATS: Userspace sends this operation to the
 * driver to get the SR statistics and the driver sends a synchronous response
 * with the attributes defined in enum qca_wlan_vendor_attr_sr_stats using the
 * nested attribute %QCA_WLAN_VENDOR_ATTR_SR_STATS.
 *
 * @QCA_WLAN_SR_OPERATION_CLEAR_STATS: Userspace sends this operation to the
 * driver to clear the SR statistics and upon receiving this operation
 * the driver/firmware shall clear the SR statistics.
 *
 * @QCA_WLAN_SR_OPERATION_GET_PARAMS: Userspace sends this operation to the
 * driver to get the SR parameters and the driver sends the synchronous response
 * with the following required attributes:
 * %QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_OBSS_PD_MIN_OFFSET,
 * %QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_OBSS_PD_MAX_OFFSET,
 * %QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_OBSS_PD_MAX_OFFSET,
 * %QCA_WLAN_VENDOR_ATTR_SR_PARAMS_HESIGA_VAL15_ENABLE,
 * %QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_OBSS_PD_DISALLOW.
 *
 * @QCA_WLAN_SR_OPERATION_UPDATE_PARAMS: The driver uses this operation in an
 * asynchronous event to userspace to update any changes in SR parameters.
 * The following attributes are used with this operation:
 * %QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_OBSS_PD_MIN_OFFSET,
 * %QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_OBSS_PD_MAX_OFFSET,
 * %QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_OBSS_PD_MAX_OFFSET,
 * %QCA_WLAN_VENDOR_ATTR_SR_PARAMS_HESIGA_VAL15_ENABLE,
 * %QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_OBSS_PD_DISALLOW.
 */
enum qca_wlan_sr_operation {
	QCA_WLAN_SR_OPERATION_SR_ENABLE = 0,
	QCA_WLAN_SR_OPERATION_SR_DISABLE = 1,
	QCA_WLAN_SR_OPERATION_SR_SUSPEND = 2,
	QCA_WLAN_SR_OPERATION_SR_RESUME = 3,
	QCA_WLAN_SR_OPERATION_PSR_AND_NON_SRG_OBSS_PD_PROHIBIT = 4,
	QCA_WLAN_SR_OPERATION_PSR_AND_NON_SRG_OBSS_PD_ALLOW = 5,
	QCA_WLAN_SR_OPERATION_GET_STATS = 6,
	QCA_WLAN_SR_OPERATION_CLEAR_STATS = 7,
	QCA_WLAN_SR_OPERATION_GET_PARAMS = 8,
	QCA_WLAN_SR_OPERATION_UPDATE_PARAMS = 9,
};

/**
 * enum qca_wlan_vendor_attr_sr_params - Defines attributes for SR configuration
 * parameters used by attribute %QCA_WLAN_VENDOR_ATTR_SR_PARAMS.
 *
 * @QCA_WLAN_VENDOR_ATTR_SR_PARAMS_HESIGA_VAL15_ENABLE: Flag attribute.
 * This attribute is optionally set in response to
 * %QCA_WLAN_SR_OPERATION_GET_PARAMS and in request when operation is set to
 * %QCA_WLAN_SR_OPERATION_PSR_AND_NON_SRG_OBSS_PD_PROHIBIT. Refer IEEE Std
 * 802.11ax-2021 Figure 9-788r-SR Control field format to understand more
 * about HESIGA_Spatial_reuse_value15_allowed.
 *
 * @QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_OBSS_PD_DISALLOW: Flag attribute.
 * This attribute is used in response to %QCA_WLAN_SR_OPERATION_GET_PARAMS
 * operation. This indicates whether non-SRG OBSS PD SR transmissions are
 * allowed or not at non-AP STAs that are associated with the AP. If present
 * non-SRG OBSS PD SR transmissions are not allowed else are allowed.
 *
 * @QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_OBSS_PD_MIN_OFFSET: Optional u8
 * attribute. This attribute is used in response to
 * %QCA_WLAN_SR_OPERATION_GET_PARAMS operation. This indicates the SRG OBSS PD
 * Min Offset field which contains an unsigned integer that is added to -82 dBm
 * to generate the value of the SRG OBSS PD Min parameter.
 *
 * @QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_OBSS_PD_MAX_OFFSET: Optional u8
 * attribute. This attribute is used in response to
 * %QCA_WLAN_SR_OPERATION_GET_PARAMS operation. This indicates the SRG OBSS PD
 * Max Offset field which contains an unsigned integer that is added to -82 dBm
 * to generate the value of the SRG OBSS PD Max parameter.
 *
 * @QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_OBSS_PD_MAX_OFFSET: Optional u8
 * attribute. This attribute is used in response to
 * %QCA_WLAN_SR_OPERATION_GET_PARAMS operation. This indicates the Non-SRG OBSS
 * PD Max Offset field which contains an unsigned integer that is added to -82
 * dBm to generate the value of the Non-SRG OBSS PD Max parameter.
 *
 * @QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_PD_THRESHOLD: s32 attribute (in dBm).
 * Userspace optionally sends this attribute with
 * %QCA_WLAN_SR_OPERATION_SR_ENABLE operation to the driver to specify the
 * preferred SRG PD threshold. The driver shall send this attribute to
 * userspace in SR resume event to indicate the PD threshold being used for SR.
 * When there is change in SRG PD threshold (for example, due to roaming, etc.)
 * the driver shall indicate the userspace the newly configured SRG PD threshold
 * using an asynchronous event.
 *
 * @QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_PD_THRESHOLD: s32 attribute (in dBm).
 * Userspace optionally sends this attribute with
 * %QCA_WLAN_SR_OPERATION_SR_ENABLE operation to the driver to specify the
 * preferred non-SRG PD threshold. The driver shall send this attribute to
 * userspace in SR resume event to indicate the PD threshold being used for SR.
 * When there is change in non-SRG PD threshold (for example, due to roaming,
 * etc.) the driver shall indicate the userspace the newly configured non-SRG PD
 * threshold using an asynchronous event.
 *
 * @QCA_WLAN_VENDOR_ATTR_SR_PARAMS_REASON_CODE: u32 attribute. The possible
 * values are defined in enum qca_wlan_sr_reason_code. This
 * attribute is used with %QCA_WLAN_SR_OPERATION_SR_RESUME and
 * %QCA_WLAN_SR_OPERATION_SR_SUSPEND operations.
 */
enum qca_wlan_vendor_attr_sr_params {
	QCA_WLAN_VENDOR_ATTR_SR_PARAMS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SR_PARAMS_HESIGA_VAL15_ENABLE = 1,
	QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_OBSS_PD_DISALLOW = 2,
	QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_OBSS_PD_MIN_OFFSET = 3,
	QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_OBSS_PD_MAX_OFFSET = 4,
	QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_OBSS_PD_MAX_OFFSET = 5,
	QCA_WLAN_VENDOR_ATTR_SR_PARAMS_SRG_PD_THRESHOLD = 6,
	QCA_WLAN_VENDOR_ATTR_SR_PARAMS_NON_SRG_PD_THRESHOLD = 7,
	QCA_WLAN_VENDOR_ATTR_SR_PARAMS_REASON_CODE = 8,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SR_PARAMS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SR_PARAMS_MAX =
	QCA_WLAN_VENDOR_ATTR_SR_PARAMS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_sr - Defines the attributes used by the vendor
 * command QCA_NL80211_VENDOR_SUBCMD_SR.
 *
 * @QCA_WLAN_VENDOR_ATTR_SR_OPERATION: Mandatory u8 attribute for all requests
 * from userspace to the driver. Possible values are defined in enum
 * qca_wlan_sr_operation.
 *
 * @QCA_WLAN_VENDOR_ATTR_SR_PARAMS: Nested attribute, contains the SR
 * configuration parameters. The possible attributes inside this attribute are
 * defined in enum qca_wlan_vendor_attr_sr_params.
 * This attribute is used when QCA_WLAN_VENDOR_ATTR_SR_OPERATION is set to
 * %QCA_WLAN_SR_OPERATION_SR_ENABLE in requests from userspace to the driver and
 * also in response from the driver to userspace when the response is sent for
 * %QCA_WLAN_SR_OPERATION_GET_PARAMS.
 * The driver uses this attribute in asynchronous events in which the operation
 * is set to %QCA_WLAN_SR_OPERATION_SR_RESUME,
 * %QCA_WLAN_SR_OPERATION_SR_SUSPEND or %QCA_WLAN_SR_OPERATION_UPDATE_PARAMS.
 *
 * @QCA_WLAN_VENDOR_ATTR_SR_STATS: Nested attribute, contains the SR
 * statistics. These attributes used inside this are defined in enum
 * qca_wlan_vendor_attr_sr_stats.
 * This attribute is used in response from the driver to a command in which
 * %QCA_WLAN_VENDOR_ATTR_SR_OPERATION is set to
 * %QCA_WLAN_SR_OPERATION_GET_STATS.
 */
enum qca_wlan_vendor_attr_sr {
	QCA_WLAN_VENDOR_ATTR_SR_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SR_OPERATION = 1,
	QCA_WLAN_VENDOR_ATTR_SR_PARAMS = 2,
	QCA_WLAN_VENDOR_ATTR_SR_STATS = 3,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_SR_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SR_MAX =
	QCA_WLAN_VENDOR_ATTR_SR_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_mlo_peer_prim_netdev_event - Defines the attributes
 * used in the QCA_NL80211_VENDOR_SUBCMD_MLO_PEER_PRIM_NETDEV_EVENT subcommand.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_PRIM_NETDEV_EVENT_MACADDR: 6 byte MAC address
 * used by the peer on the link that corresponds to the link used for sending
 * the event notification.
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_PRIM_NETDEV_EVENT_MLD_MAC_ADDR: 6 byte
 * MLD MAC address of the peer.
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_PRIM_NETDEV_EVENT_PRIM_IFINDEX: u32 attribute,
 * used to pass ifindex of the primary netdev.
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_PRIM_NETDEV_EVENT_MLD_IFINDEX: u32 attribute,
 * used to pass ifindex of the MLD netdev.
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_PRIM_NETDEV_EVENT_NUM_LINKS: u8 attribute,
 * used to indicate the number of links that the non-AP MLD negotiated to be
 * used in the ML connection.
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_PRIM_NETDEV_EVENT_LINK_INFO: Nested
 * attribute, contains information regarding links of the non-AP MLD.
 * User applications need to know all the links of a non-AP MLD that are
 * participating in the ML association. The possible attributes inside this
 * attribute are defined in enum qca_wlan_vendor_attr_mlo_link_info.
 */
enum qca_wlan_vendor_attr_mlo_peer_prim_netdev_event {
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_PRIM_NETDEV_EVENT_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_PRIM_NETDEV_EVENT_MACADDR = 1,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_PRIM_NETDEV_EVENT_MLD_MAC_ADDR = 2,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_PRIM_NETDEV_EVENT_PRIM_IFINDEX = 3,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_PRIM_NETDEV_EVENT_MLD_IFINDEX = 4,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_PRIM_NETDEV_EVENT_NUM_LINKS = 5,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_PRIM_NETDEV_EVENT_LINK_INFO = 6,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_PRIM_NETDEV_EVENT_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_PRIM_NETDEV_EVENT_MAX =
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_PRIM_NETDEV_EVENT_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_mlo_link_info - Defines attributes for
 * non-AP MLD link parameters used by the attribute
 * %QCA_WLAN_VENDOR_ATTR_MLO_PEER_PRIM_NETDEV_EVENT_LINK_INFO.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_LINK_INFO_IFINDEX: u32 attribute, used
 * to pass the netdev ifindex of the non-AP MLD link.
 * @QCA_WLAN_VENDOR_ATTR_MLO_LINK_INFO_MACADDR: 6 byte MAC address of
 * the non-AP MLD link.
 */
enum qca_wlan_vendor_attr_mlo_link_info {
	QCA_WLAN_VENDOR_ATTR_MLO_LINK_INFO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_MLO_LINK_INFO_IFINDEX = 1,
	QCA_WLAN_VENDOR_ATTR_MLO_LINK_INFO_MACADDR = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_MLO_LINK_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_MLO_LINK_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_MLO_LINK_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_afc_freq_psd_info: This enum is used with
 * nested attributes QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO and
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_FREQ_RANGE_LIST to update the frequency range
 * and PSD information.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START: Required and type is
 * u32. This attribute is used to indicate the start of the queried frequency
 * range in MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END: Required and type is u32.
 * This attribute is used to indicate the end of the queried frequency range
 * in MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_PSD: Required and type is u32.
 * This attribute will contain the PSD information for a single range as
 * specified by the QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START and
 * QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END attributes.
 *
 * The PSD power info (dBm/MHz) from user space should be multiplied
 * by a factor of 100 when sending to the driver to preserve granularity
 * up to 2 decimal places.
 * Example:
 *     PSD power value: 10.21 dBm/MHz
 *     Value to be updated in QCA_WLAN_VENDOR_ATTR_AFC_PSD_INFO: 1021.
 *
 * Note: QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_PSD attribute will be used only
 * with nested attribute QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO and with
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_FREQ_RANGE_LIST when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE.
 *
 * The following set of attributes will be used to exchange frequency and
 * corresponding PSD information for AFC between the user space and the driver.
 */
enum qca_wlan_vendor_attr_afc_freq_psd_info {
	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START = 1,
	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END = 2,
	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_PSD = 3,

	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_afc_chan_eirp_info: This enum is used with
 * nested attribute QCA_WLAN_VENDOR_ATTR_AFC_CHAN_LIST_INFO to update the
 * channel list and corresponding EIRP information.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM: Required and type is u8.
 * This attribute is used to indicate queried channel from
 * the operating class indicated in QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP: Optional and type is u32.
 * This attribute is used to configure the EIRP power info corresponding
 * to the channel number indicated in QCA_WLAN_VENDOR_ATTR_AFC_CHAN_NUM.
 * The EIRP power info(dBm) from user space should be multiplied
 * by a factor of 100 when sending to Driver to preserve granularity up to
 * 2 decimal places.
 * Example:
 *     EIRP power value: 34.23 dBm
 *     Value to be updated in QCA_WLAN_VENDOR_ATTR_AFC_EIRP_INFO: 3423.
 *
 * Note: QCA_WLAN_VENDOR_ATTR_AFC_EIRP_INFO attribute will only be used with
 * nested attribute QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO and
 * with QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_LIST when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE:
 *
 * The following set of attributes will be used to exchange Channel and
 * corresponding EIRP information for AFC between the user space and Driver.
 */
enum qca_wlan_vendor_attr_afc_chan_eirp_info {
	QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM = 1,
	QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP = 2,

	QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_afc_opclass_info: This enum is used with nested
 * attributes QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO and
 * QCA_WLAN_VENDOR_ATTR_AFC_REQ_OPCLASS_CHAN_INFO to update the operating class,
 * channel, and EIRP related information.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS: Required and type is u8.
 * This attribute is used to indicate the operating class, as listed under
 * IEEE Std 802.11-2020 Annex E Table E-4, for the queried channel list.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST: Array of nested attributes
 * for updating the channel number and EIRP power information.
 * It uses the attributes defined in
 * enum qca_wlan_vendor_attr_afc_chan_eirp_info.
 *
 * Operating class information packing format for
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_INFO when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE_EXPIRY.
 *
 * m - Total number of operating classes.
 * n, j - Number of queried channels for the corresponding operating class.
 *
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS[0]
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST[0]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[0]
 *      .....
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[n - 1]
 *  ....
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS[m]
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST[m]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[0]
 *      ....
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[j - 1]
 *
 * Operating class information packing format for
 * QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO and
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_INFO when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE.
 *
 * m - Total number of operating classes.
 * n, j - Number of channels for the corresponding operating class.
 *
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS[0]
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST[0]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[0]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP[0]
 *      .....
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[n - 1]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP[n - 1]
 *  ....
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS[m]
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST[m]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[0]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP[0]
 *      ....
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[j - 1]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP[j - 1]
 *
 * The following set of attributes will be used to exchange operating class
 * information for AFC between the user space and the driver.
 */
enum qca_wlan_vendor_attr_afc_opclass_info {
	QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS = 1,
	QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST = 2,

	QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_afc_event_type: Defines values for AFC event type.
 * Attribute used by QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE attribute.
 *
 * @QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY: AFC expiry event sent from the
 * driver to userspace in order to query the new AFC power values.
 *
 * @QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE: Power update
 * complete event will be sent from the driver to userspace to indicate
 * processing of the AFC response.
 *
 * @QCA_WLAN_VENDOR_AFC_EVENT_TYPE_PAYLOAD_RESET: AFC payload reset event
 * will be sent from the driver to userspace to indicate last received
 * AFC response data has been cleared on the AP due to invalid data
 * in the QCA_NL80211_VENDOR_SUBCMD_AFC_RESPONSE.
 *
 * The following enum defines the different event types that will be
 * used by the driver to help trigger corresponding AFC functionality in user
 * space.
 */
enum qca_wlan_vendor_afc_event_type {
	QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY = 0,
	QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE = 1,
	QCA_WLAN_VENDOR_AFC_EVENT_TYPE_PAYLOAD_RESET = 2,
};

/**
 * enum qca_wlan_vendor_afc_ap_deployment_type: Defines values for AP
 * deployment type.
 * Attribute used by QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AP_DEPLOYMENT attribute.
 *
 * @QCA_WLAN_VENDOR_AFC_AP_DEPLOYMENT_TYPE_UNKNOWN: Unknown AP deployment.
 *
 * @QCA_WLAN_VENDOR_AFC_AP_DEPLOYMENT_TYPE_INDOOR: Indoor AP deployment.
 *
 * @QCA_WLAN_VENDOR_AFC_AP_DEPLOYMENT_TYPE_OUTDOOR: Outdoor AP deployment.
 *
 * The following enum defines different deployment modes that the AP might
 * come up in. This information will be essential to retrieve deployment-type
 * specific SP power values for AFC operation.
 */
enum qca_wlan_vendor_afc_ap_deployment_type {
	QCA_WLAN_VENDOR_AFC_AP_DEPLOYMENT_TYPE_UNKNOWN = 0,
	QCA_WLAN_VENDOR_AFC_AP_DEPLOYMENT_TYPE_INDOOR = 1,
	QCA_WLAN_VENDOR_AFC_AP_DEPLOYMENT_TYPE_OUTDOOR = 2,
};

/**
 * enum qca_wlan_vendor_afc_evt_status_code: Defines values AP will use to
 * indicate AFC response status.
 * Enum used by QCA_WLAN_VENDOR_ATTR_AFC_EVENT_STATUS_CODE attribute.
 *
 * @QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_SUCCESS: Success
 *
 * @QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_TIMEOUT: Indicates AFC indication
 * command was not received within the expected time of the AFC expiry event
 * being triggered.
 *
 * @QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_PARSING_ERROR: Indicates AFC data
 * parsing error by the driver.
 *
 * @QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_LOCAL_ERROR: Indicates any other local
 * error.
 *
 * The following enum defines the status codes that the driver will use to
 * indicate whether the AFC data is valid or not.
 */
enum qca_wlan_vendor_afc_evt_status_code {
	QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_SUCCESS = 0,
	QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_TIMEOUT = 1,
	QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_PARSING_ERROR = 2,
	QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_LOCAL_ERROR = 3,
};

/**
 * enum qca_wlan_vendor_attr_afc_event: Defines attributes to be used with
 * vendor event QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT. These attributes will
 * support sending only a single request to the user space at a time.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE: Required u8 attribute.
 * Used with event to notify the type of AFC event received.
 * Valid values are defined in enum qca_wlan_vendor_afc_event_type.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AP_DEPLOYMENT: u8 attribute. Required when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY,
 * otherwise unused.
 *
 * This attribute is used to indicate the AP deployment type in the AFC request.
 * Valid values are defined in enum qca_wlan_vendor_afc_ap_deployment_type.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_REQ_ID: Required u32 attribute.
 * Unique request identifier generated by the AFC client for every
 * AFC expiry event trigger. See also QCA_WLAN_VENDOR_ATTR_AFC_RESP_REQ_ID.
 * The user space application is responsible for ensuring no duplicate values
 * are in-flight with the server, e.g., by delaying a request, should the same
 * value be received from different radios in parallel.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AFC_WFA_VERSION: u32 attribute. Optional.
 * It is used when the QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY, otherwise unused.
 *
 * This attribute indicates the AFC spec version information. This will
 * indicate the AFC version AFC client must use to query the AFC data.
 * Bits 15:0  - Minor version
 * Bits 31:16 - Major version
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_MIN_DES_POWER: u16 attribute. Required when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY,
 * otherwise unused.
 * This attribute indicates the minimum desired power (in dBm) for
 * the queried spectrum.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_STATUS_CODE: u8 attribute. Required when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE, otherwise unused.
 *
 * Valid values are defined in enum qca_wlan_vendor_afc_evt_status_code.
 * This attribute is used to indicate if there were any errors parsing the
 * AFC response.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_SERVER_RESP_CODE: s32 attribute. Required
 * when QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE, otherwise unused.
 *
 * This attribute indicates the AFC response code. The AFC response codes are
 * in the following categories:
 * -1: General Failure.
 * 0: Success.
 * 100 - 199: General errors related to protocol.
 * 300 - 399: Error events specific to message exchange
 *            for the Available Spectrum Inquiry.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_EXP_DATE: u32 attribute. Required when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE, otherwise unused.
 *
 * This attribute indicates the date until which the current response is
 * valid for in UTC format.
 * Date format: bits 7:0   - DD (Day 1-31)
 *              bits 15:8  - MM (Month 1-12)
 *              bits 31:16 - YYYY (Year)
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_EXP_TIME: u32 attribute. Required when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE, otherwise unused.
 *
 * This attribute indicates the time until which the current response is
 * valid for in UTC format.
 * Time format: bits 7:0   - SS (Seconds 0-59)
 *              bits 15:8  - MM (Minutes 0-59)
 *              bits 23:16 - HH (Hours 0-23)
 *              bits 31:24 - Reserved
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_FREQ_RANGE_LIST: Array of nested attributes
 * for updating the list of frequency ranges to be queried.
 * Required when QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY or
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE, otherwise unused.
 * It uses the attributes defined in
 * enum qca_wlan_vendor_attr_afc_freq_psd_info.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_LIST: Array of nested attributes
 * for updating the list of operating classes and corresponding channels to be
 * queried.
 * Required when QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY or
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE, otherwise unused.
 * It uses the attributes defined in enum qca_wlan_vendor_attr_afc_opclass_info.
 */
enum qca_wlan_vendor_attr_afc_event {
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE = 1,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AP_DEPLOYMENT = 2,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_REQ_ID = 3,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AFC_WFA_VERSION = 4,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_MIN_DES_POWER = 5,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_STATUS_CODE = 6,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_SERVER_RESP_CODE = 7,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_EXP_DATE = 8,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_EXP_TIME = 9,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_FREQ_RANGE_LIST = 10,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_LIST = 11,

	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_MAX =
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_afc_response: Defines attributes to be used
 * with vendor command QCA_NL80211_VENDOR_SUBCMD_AFC_RESPONSE. These attributes
 * will support sending only a single AFC response to the driver at a time.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA: Type is NLA_STRING. Required attribute.
 * This attribute will be used to send a single Spectrum Inquiry response object
 * from the 'availableSpectrumInquiryResponses' array object from the response
 * JSON.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_TIME_TO_LIVE: Required u32 attribute.
 *
 * This attribute indicates the period (in seconds) for which the response
 * data received is valid for.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_REQ_ID: Required u32 attribute.
 *
 * This attribute indicates the request ID for which the corresponding
 * response is being sent for. See also QCA_WLAN_VENDOR_ATTR_AFC_EVENT_REQ_ID.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_DATE: Required u32 attribute.
 *
 * This attribute indicates the date until which the current response is
 * valid for in UTC format.
 * Date format: bits 7:0   - DD (Day 1-31)
 *              bits 15:8  - MM (Month 1-12)
 *              bits 31:16 - YYYY (Year)
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_TIME: Required u32 attribute.
 *
 * This attribute indicates the time until which the current response is
 * valid for in UTC format.
 * Time format: bits 7:0   - SS (Seconds 0-59)
 *              bits 15:8  - MM (Minutes 0-59)
 *              bits 23:16 - HH (Hours 0-23)
 *              bits 31:24 - Reserved
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFC_SERVER_RESP_CODE: Required s32 attribute.
 *
 * This attribute indicates the AFC response code. The AFC response codes are
 * in the following categories:
 * -1: General Failure.
 * 0: Success.
 * 100 - 199: General errors related to protocol.
 * 300 - 399: Error events specific to message exchange
 *            for the Available Spectrum Inquiry.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO: Array of nested attributes
 * for PSD info of all the queried frequency ranges. It uses the attributes
 * defined in enum qca_wlan_vendor_attr_afc_freq_psd_info. Required attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO: Array of nested
 * attributes for EIRP info of all queried operating class/channels. It uses
 * the attributes defined in enum qca_wlan_vendor_attr_afc_opclass_info and
 * enum qca_wlan_vendor_attr_afc_chan_eirp_info. Required attribute.
 *
 */
enum qca_wlan_vendor_attr_afc_response {
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA = 1,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_TIME_TO_LIVE = 2,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_REQ_ID = 3,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_DATE = 4,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_TIME = 5,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFC_SERVER_RESP_CODE = 6,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO = 7,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO = 8,

	QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_MAX =
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_dozed_ap_state - Doze states for AP interface
 *
 * @QCA_WLAN_DOZED_AP_DISABLE: Disable doze state on the AP interface.
 *
 * @QCA_WLAN_DOZED_AP_ENABLE: Enable doze state on the AP interface. AP starts
 * beaconing at higher beacon interval with Rx disabled.
 */
enum qca_wlan_dozed_ap_state {
	QCA_WLAN_DOZED_AP_DISABLE = 0,
	QCA_WLAN_DOZED_AP_ENABLE = 1,
};

/**
 * enum qca_wlan_vendor_attr_dozed_ap - Used by the vendor command
 * @QCA_NL80211_VENDOR_SUBCMD_DOZED_AP to configure or receive dozed AP mode
 * configuration.
 *
 * @QCA_WLAN_VENDOR_ATTR_DOZED_AP_STATE: u8 attribute.
 * Configures the doze state for an AP interface. Possible values are defined
 * in enum qca_wlan_dozed_ap_state. @QCA_NL80211_VENDOR_SUBCMD_DOZED_AP event
 * gets triggered asynchronously to provide updated AP interface configuration.
 *
 * @QCA_WLAN_VENDOR_ATTR_DOZED_AP_COOKIE: Unsigned 64-bit cookie provided by
 * the driver in the response to specific @QCA_NL80211_VENDOR_SUBCMD_DOZED_AP
 * command, which is used later to maintain synchronization between commands
 * and asynchronous events.
 *
 * @QCA_WLAN_VENDOR_ATTR_DOZED_AP_NEXT_TSF: u64 attribute.
 * Used in event to indicate the next TBTT TSF timer value after applying the
 * doze mode configuration. Next TBTT TSF is the time at which the AP sends
 * the first beacon after entering or exiting dozed mode.
 *
 * @QCA_WLAN_VENDOR_ATTR_DOZED_AP_BI_MULTIPLIER: u16 attribute.
 * Used with event to inform the periodicity of beacon transmission that would
 * be skipped at all TBTTs in between.
 *
 * @QCA_WLAN_VENDOR_ATTR_DOZED_AP_PAD: Attribute used for padding for 64-bit
 * alignment.
 */
enum qca_wlan_vendor_attr_dozed_ap {
	QCA_WLAN_VENDOR_ATTR_DOZED_AP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_DOZED_AP_STATE = 1,
	QCA_WLAN_VENDOR_ATTR_DOZED_AP_COOKIE = 2,
	QCA_WLAN_VENDOR_ATTR_DOZED_AP_NEXT_TSF = 3,
	QCA_WLAN_VENDOR_ATTR_DOZED_AP_BI_MULTIPLIER = 4,
	QCA_WLAN_VENDOR_ATTR_DOZED_AP_PAD = 5,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_DOZED_AP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_DOZED_AP_MAX =
	QCA_WLAN_VENDOR_ATTR_DOZED_AP_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_monitor_mode_status - Represents the status codes
 * used with QCA_NL80211_VENDOR_SUBCMD_GET_MONITOR_MODE.
 * @QCA_WLAN_VENDOR_MONITOR_MODE_NO_CAPTURE_RUNNING: Used to indicate no
 * capture running status.
 * @QCA_WLAN_VENDOR_MONITOR_MODE_CAPTURE_RUNNING: Used to indicate
 * capture running status.
 **/

enum qca_wlan_vendor_monitor_mode_status {
	QCA_WLAN_VENDOR_MONITOR_MODE_NO_CAPTURE_RUNNING = 0,
	QCA_WLAN_VENDOR_MONITOR_MODE_CAPTURE_RUNNING = 1,
};

/**
 * enum qca_wlan_vendor_attr_get_monitor_mode - Used by the
 * vendor command QCA_NL80211_VENDOR_SUBCMD_GET_MONITOR_MODE to report
 * information regarding the local packet capture over the monitor mode.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_MONITOR_MODE_STATUS: u32 attribute. This attribute
 * represents the status of the start capture commands. The values used with
 * this attribute are defined in enum qca_wlan_vendor_monitor_mode_status. This
 * is returned by the driver in the response to the command.
 */

enum qca_wlan_vendor_attr_get_monitor_mode {
	QCA_WLAN_VENDOR_ATTR_GET_MONITOR_MODE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_GET_MONITOR_MODE_STATUS = 1,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_GET_MONITOR_MODE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_GET_MONITOR_MODE_MAX =
	QCA_WLAN_VENDOR_ATTR_GET_MONITOR_MODE_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_link_state_op_types - Defines different types of
 * operations for which %QCA_NL80211_VENDOR_SUBCMD_MLO_LINK_STATE can be used.
 * Will be used with %QCA_WLAN_VENDOR_ATTR_LINK_STATE_OP_TYPE attribute.
 *
 * @QCA_WLAN_VENDOR_LINK_STATE_OP_GET - Get the MLO links state information.
 * @QCA_WLAN_VENDOR_LINK_STATE_OP_SET - Set the MLO links state information.
 */
enum qca_wlan_vendor_link_state_op_types {
	QCA_WLAN_VENDOR_LINK_STATE_OP_GET = 0,
	QCA_WLAN_VENDOR_LINK_STATE_OP_SET = 1,
};

/**
 * enum qca_wlan_vendor_link_state_control_modes - Represents the types of MLO
 * links state control modes. This enum is used by
 * %QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONTROL_MODE attribute.
 *
 * @QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_DEFAULT: MLO links state controlled
 * by the driver.
 * @QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_USER: MLO links state controlled by
 * user space.
 * @QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_MIXED: User space provides the
 * desired number of MLO links to operate in active state at any given time.
 * The driver will choose which MLO links should operate in the active state.
 * See enum qca_wlan_vendor_link_state for active state definition.
 */
enum qca_wlan_vendor_link_state_control_modes {
	QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_DEFAULT = 0,
	QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_USER = 1,
	QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_MIXED = 2,
};

/**
 * enum qca_wlan_vendor_link_state_operation_modes - Represents the types of MLO
 * links state operation modes. This enum is used by
 * %QCA_WLAN_VENDOR_ATTR_LINK_STATE_OPERATION_MODE attribute.
 *
 * @QCA_WLAN_VENDOR_LINK_STATE_OPERATION_MODE_DEFAULT: In the default operation
 * mode, the driver selects the operating mode of the links, without any
 * guidance from the user space.
 * @QCA_WLAN_VENDOR_LINK_STATE_OPERATION_MODE_LOW_LATENCY: In the low latency
 * operation mode the driver should select MLO links that will achieve low
 * latency.
 * @QCA_WLAN_VENDOR_LINK_STATE_OPERATION_MODE_HIGH_THROUGHPUT: In the high
 * throughput operation mode the driver should select MLO links that will
 * achieve higher throughput.
 * @QCA_WLAN_VENDOR_LINK_STATE_OPERATION_MODE_LOW_POWER: In the low power
 * operation mode the driver should select MLO links that will achieve low
 * power.
 */
enum qca_wlan_vendor_link_state_operation_modes {
	QCA_WLAN_VENDOR_LINK_STATE_OPERATION_MODE_DEFAULT = 0,
	QCA_WLAN_VENDOR_LINK_STATE_OPERATION_MODE_LOW_LATENCY = 1,
	QCA_WLAN_VENDOR_LINK_STATE_OPERATION_MODE_HIGH_THROUGHPUT = 2,
	QCA_WLAN_VENDOR_LINK_STATE_OPERATION_MODE_LOW_POWER = 3,
};

/**
 * enum qca_wlan_vendor_link_state - Represents the possible link states of an
 * MLO link.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_STATE_INACTIVE: In this state, the link will not
 * be used for data transmission but it can have TIDs mapped to it. It will be
 * in doze state always and does not monitor the beacons.
 * @QCA_WLAN_VENDOR_ATTR_LINK_STATE_ACTIVE: In this state, the link will be
 * used for data TX/RX and monitors the beacons to check TIM bit indication.
 * It may enter doze state and comes out based on the transmit data traffic and
 * TIM bit indication in the beacon.
 */
enum qca_wlan_vendor_link_state {
	QCA_WLAN_VENDOR_LINK_STATE_INACTIVE = 0,
	QCA_WLAN_VENDOR_LINK_STATE_ACTIVE = 1,
};

/**
 * enum qca_wlan_vendor_attr_link_state_config - Definition of attributes used
 * inside nested attribute %QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_LINK_ID: u8 attribute, link ID of the
 * MLO link.
 * @QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_STATE: u32 attribute. See
 * enum qca_wlan_vendor_link_state for possible MLO link states.
 */

enum qca_wlan_vendor_attr_link_state_config {
	QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_LINK_ID = 1,
	QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_STATE = 2,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_MAX =
	QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_mlo_link_state - Attributes used by
 * %QCA_NL80211_VENDOR_SUBCMD_MLO_LINK_STATE vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_STATE_OP_TYPE: u32 attribute. Indicates the type
 * of the operation %QCA_NL80211_VENDOR_SUBCMD_MLO_LINK_STATE intended for.
 * Required only in a command. Possible values for this attribute are defined in
 * enum qca_wlan_vendor_link_state_op_types.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONTROL_MODE: u32 attribute. Indicates MLO
 * links control mode type. Optional attribute in a command when
 * %QCA_WLAN_VENDOR_ATTR_LINK_STATE_OP_TYPE is set to
 * %QCA_WLAN_VENDOR_LINK_STATE_OP_SET. Required attribute in a response when
 * %QCA_WLAN_VENDOR_ATTR_LINK_STATE_OP_TYPE is set to
 * %QCA_WLAN_VENDOR_LINK_STATE_OP_GET.
 * See enum qca_wlan_vendor_link_state_control_modes for possible control modes.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG: Array of nested attributes.
 * Indicates the state of the each MLO link affiliated with the interface.
 * Required attribute in a command when %QCA_WLAN_VENDOR_ATTR_LINK_STATE_OP_TYPE
 * is set to %QCA_WLAN_VENDOR_LINK_STATE_OP_SET and
 * %QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONTROL_MODE is set to
 * %QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_USER. Required attribute in a
 * response when %QCA_WLAN_VENDOR_ATTR_LINK_STATE_OP_TYPE is set to
 * %QCA_WLAN_VENDOR_LINK_STATE_OP_GET.
 * See enum qca_wlan_vendor_attr_link_state_config for the nested attributes.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_STATE_MIXED_MODE_ACTIVE_NUM_LINKS: u8 attribute.
 * Represents the number of active state links. See enum
 * qca_wlan_vendor_link_state for active state definition.
 * Required attribute in a command when %QCA_WLAN_VENDOR_ATTR_LINK_STATE_OP_TYPE
 * is set to %QCA_WLAN_VENDOR_LINK_STATE_OP_SET and
 * %QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONTROL_MODE is set to
 * %QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_MIXED.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_STATE_OPERATION_MODE: u32 attribute. Indicates MLO
 * links operation mode type. Optional attribute in a command when
 * %QCA_WLAN_VENDOR_ATTR_LINK_STATE_OP_TYPE is set to
 * %QCA_WLAN_VENDOR_LINK_STATE_OP_SET. Required attribute in a response when
 * %QCA_WLAN_VENDOR_ATTR_LINK_STATE_OP_TYPE is set to
 * %QCA_WLAN_VENDOR_LINK_STATE_OP_GET.
 * See enum qca_wlan_vendor_link_state_operation_modes for possible operation
 * modes.
 */
enum qca_wlan_vendor_attr_mlo_link_state {
	QCA_WLAN_VENDOR_ATTR_LINK_STATE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_LINK_STATE_OP_TYPE = 1,
	QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONTROL_MODE = 2,
	QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG = 3,
	QCA_WLAN_VENDOR_ATTR_LINK_STATE_MIXED_MODE_ACTIVE_NUM_LINKS = 4,
	QCA_WLAN_VENDOR_ATTR_LINK_STATE_OPERATION_MODE = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_LINK_STATE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_LINK_STATE_MAX =
	QCA_WLAN_VENDOR_ATTR_LINK_STATE_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_tid_link_map_status - Definition of attributes used
 * inside nested attribute %QCA_WLAN_VENDOR_ATTR_TID_TO_LINK_MAP_STATUS.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_TID_MAP_STATUS_UPLINK: Required u16 attribute
 * within nested attribute %QCA_WLAN_VENDOR_ATTR_TID_TO_LINK_MAP_STATUS.
 * Indicates the link mapping bitmap of a TID for uplink traffic. It is a
 * bitmask of the link IDs in which a bit set means that the TID is mapped with
 * that link ID in uplink traffic. Otherwise, the TID is not mapped to uplink
 * traffic for that link.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_TID_MAP_STATUS_DOWNLINK: Required u16 attribute
 * within nested attribute %QCA_WLAN_VENDOR_ATTR_TID_TO_LINK_MAP_STATUS.
 * Indicates the link mapping bitmap of a TID for downlink traffic. It is a
 * bitmask of the link IDs in which a bit set means that the TID is mapped with
 * that link ID in downlink traffic. Otherwise, the TID is not mapped to
 * downlink traffic for that link.
 */
enum qca_wlan_vendor_attr_tid_link_map_status {
	QCA_WLAN_VENDOR_ATTR_LINK_TID_MAP_STATUS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_LINK_TID_MAP_STATUS_UPLINK = 1,
	QCA_WLAN_VENDOR_ATTR_LINK_TID_MAP_STATUS_DOWNLINK = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_LINK_TID_MAP_STATUS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_LINK_TID_MAP_STATUS_MAX =
	QCA_WLAN_VENDOR_ATTR_LINK_TID_MAP_STATUS_AFTER_LAST - 1,
};

/*
 * enum qca_wlan_vendor_attr_tid_to_link_map: Definition of attributes used with
 * %QCA_NL80211_VENDOR_SUBCMD_TID_TO_LINK_MAP event.
 *
 * @QCA_WLAN_VENDOR_ATTR_TID_TO_LINK_MAP_AP_MLD_ADDR: Required attribute. 6-byte
 * AP MLD address with which this TID-to-link negotiation mapping is
 * established/updated.
 *
 * @QCA_WLAN_VENDOR_ATTR_TID_TO_LINK_MAP_STATUS: Optional attribute. Array of
 * nested attributes containing TID-to-links mapping information. This will have
 * TID-to-link mapping for TID0 to TID7, each containing the uplink and downlink
 * map information. If this attribute is not present the default TID-to-link
 * mapping is in use, i.e., all TIDs are mapped to all links for both uplink and
 * downlink traffic.
 * See enum qca_wlan_vendor_attr_tid_link_map_status for the nested attributes.
 */
enum qca_wlan_vendor_attr_tid_to_link_map {
	QCA_WLAN_VENDOR_ATTR_TID_TO_LINK_MAP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TID_TO_LINK_MAP_AP_MLD_ADDR = 1,
	QCA_WLAN_VENDOR_ATTR_TID_TO_LINK_MAP_STATUS = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TID_TO_LINK_MAP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TID_TO_LINK_MAP_MAX =
	QCA_WLAN_VENDOR_ATTR_TID_TO_LINK_MAP_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_link_reconfig: Definition of attributes used
 * with %QCA_NL80211_VENDOR_SUBCMD_LINK_RECONFIG event.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_RECONFIG_AP_MLD_ADDR: Required attribute.
 * 6-byte AP MLD address of the AP which indicated the link reconfiguration.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_RECONFIG_REMOVED_LINKS: Required u16 attribute.
 * A bitmap of the removed setup links link IDs.
 */
enum qca_wlan_vendor_attr_link_reconfig {
	QCA_WLAN_VENDOR_ATTR_LINK_RECONFIG_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_LINK_RECONFIG_AP_MLD_ADDR = 1,
	QCA_WLAN_VENDOR_ATTR_LINK_RECONFIG_REMOVED_LINKS = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_LINK_RECONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_LINK_RECONFIG_MAX =
	QCA_WLAN_VENDOR_ATTR_LINK_RECONFIG_AFTER_LAST - 1
};

/**
 * enum qca_wlan_vendor_attr_tdls_disc_rsp_ext - Attributes used by
 * %QCA_NL80211_VENDOR_SUBCMD_TDLS_DISC_RSP_EXT vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_DISC_RSP_EXT_TX_LINK: u8 attribute.
 * Indicates the MLO link id on which the TDLS discovery response
 * frame is to be transmitted.
 */
enum qca_wlan_vendor_attr_tdls_disc_rsp_ext {
	QCA_WLAN_VENDOR_ATTR_TDLS_DISC_RSP_EXT_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TDLS_DISC_RSP_EXT_TX_LINK = 1,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_TDLS_DISC_RSP_EXT_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TDLS_DISC_RSP_EXT_MAX =
	QCA_WLAN_VENDOR_ATTR_TDLS_DISC_RSP_EXT_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_tdls_state - Represents the possible TDLS states.
 *
 * @QCA_WLAN_VENDOR_TDLS_STATE_DISABLED: TDLS is not enabled, default status
 * for all stations.
 *
 * @QCA_WLAN_VENDOR_TDLS_STATE_ENABLED: TDLS is enabled, but not yet tried to
 * establish the session.
 *
 * @QCA_WLAN_VENDOR_TDLS_STATE_ESTABLISHED: Direct link TDLS session is
 * established.
 *
 * @QCA_WLAN_VENDOR_TDLS_STATE_ESTABLISHED_OFF_CHANNEL: Direct link TDLS
 * session is established using MCC.
 *
 * @QCA_WLAN_VENDOR_TDLS_STATE_DROPPED: Direct link TDLS session was
 * established, but is temporarily dropped currently.
 *
 * @QCA_WLAN_VENDOR_TDLS_STATE_FAILED: TDLS session is failed to establish.
 */
enum qca_wlan_vendor_tdls_state {
	QCA_WLAN_VENDOR_TDLS_STATE_DISABLED = 1,
	QCA_WLAN_VENDOR_TDLS_STATE_ENABLED = 2,
	QCA_WLAN_VENDOR_TDLS_STATE_ESTABLISHED = 3,
	QCA_WLAN_VENDOR_TDLS_STATE_ESTABLISHED_OFF_CHANNEL = 4,
	QCA_WLAN_VENDOR_TDLS_STATE_DROPPED = 5,
	QCA_WLAN_VENDOR_TDLS_STATE_FAILED = 6,
};

/**
 * enum qca_wlan_vendor_tdls_reason - Represents the possible TDLS reasons.
 *
 * @QCA_WLAN_TDLS_REASON_SUCCESS: TDLS session is successfully established.
 *
 * @QCA_WLAN_TDLS_REASON_UNSPECIFIED: Unspecified reason.
 *
 * @QCA_WLAN_TDLS_REASON_NOT_SUPPORTED: TDLS is not supported.
 *
 * @QCA_WLAN_TDLS_REASON_UNSUPPORTED_BAND: The specified band is not supported.
 *
 * @QCA_WLAN_TDLS_REASON_NOT_BENEFICIAL: Packets going through AP is better
 * than through direct link.
 *
 * @QCA_WLAN_TDLS_REASON_DROPPED_BY_REMOTE: Peer station doesn't want the TDLS
 * session anymore.
 */

enum qca_wlan_vendor_tdls_reason {
	QCA_WLAN_TDLS_REASON_SUCCESS = 0,
	QCA_WLAN_TDLS_REASON_UNSPECIFIED = -1,
	QCA_WLAN_TDLS_REASON_NOT_SUPPORTED = -2,
	QCA_WLAN_TDLS_REASON_UNSUPPORTED_BAND = -3,
	QCA_WLAN_TDLS_REASON_NOT_BENEFICIAL = -4,
	QCA_WLAN_TDLS_REASON_DROPPED_BY_REMOTE = -5,
};

/**
 * enum qca_wlan_vendor_attr_tdls_enable - Attributes used by
 * %QCA_NL80211_VENDOR_SUBCMD_TDLS_ENABLE vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAC_ADDR: 6-byte MAC address of the peer
 * station to enable the TDLS session. Optional attribute. The driver sends the
 * TDLS session result as an asynchronous response using the command
 * %QCA_NL80211_VENDOR_SUBCMD_TDLS_STATE when this peer MAC is provided in
 * %QCA_NL80211_VENDOR_SUBCMD_TDLS_ENABLE command.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_CHANNEL: u32 attribute. Indicates the
 * channel on which the TDLS session to be established. Required only when
 * %QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAC_ADDR is present.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_GLOBAL_OPERATING_CLASS: u32 attribute.
 * Indicates the global operating class of the TDLS session to be established.
 * Required only when %QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAC_ADDR is present.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAX_LATENCY_MS: u32 attribute. Indicates
 * the maximum latency of the WLAN packets to be transmitted/received in
 * milliseconds on TDLS session. Required only when
 * %QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAC_ADDR is present.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MIN_BANDWIDTH_KBPS: u32 attribute.
 * Indicates the minimum bandwidth to be used to establish the TDLS session
 * in kbps. Required only when %QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAC_ADDR is
 * present.
 */
enum qca_wlan_vendor_attr_tdls_enable {
	QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAC_ADDR = 1,
	QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_CHANNEL = 2,
	QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_GLOBAL_OPERATING_CLASS = 3,
	QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAX_LATENCY_MS = 4,
	QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MIN_BANDWIDTH_KBPS = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAX =
	QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_tdls_disable - Attributes used by
 * %QCA_NL80211_VENDOR_SUBCMD_TDLS_DISABLE vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_MAC_ADDR: 6-byte MAC address of the peer
 * station to disable the TDLS session. Optional attribute.
 */
enum qca_wlan_vendor_attr_tdls_disable {
	QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_MAC_ADDR = 1,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_MAX =
	QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_tdls_get_status - Attributes used by
 * %QCA_NL80211_VENDOR_SUBCMD_TDLS_GET_STATUS vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAC_ADDR: 6-byte MAC address of the
 * peer station. Optional attribute. Used in
 * %QCA_NL80211_VENDOR_SUBCMD_TDLS_GET_STATUS request and response.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_STATE: u32 attribute. Indicates the
 * TDLS session state with the peer specified in
 * %QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAC_ADDR. Uses the values from
 * enum qca_wlan_vendor_tdls_state. Used in
 * %QCA_NL80211_VENDOR_SUBCMD_TDLS_GET_STATUS response.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_REASON: s32 attribute. Indicates the
 * reason for the TDLS session state indicated in
 * %QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_STATE. Uses the values from enum
 * qca_wlan_vendor_tdls_reason. Used in
 * %QCA_NL80211_VENDOR_SUBCMD_TDLS_GET_STATUS response.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_CHANNEL: u32 attribute. Indicates the
 * channel of the TDLS session established with
 * %QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAC_ADDR. Used in
 * %QCA_NL80211_VENDOR_SUBCMD_TDLS_GET_STATUS response.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_GLOBAL_OPERATING_CLASS: u32 attribute.
 * Indicates the global operating class of the TDLS session established with
 * %QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAC_ADDR. Used in
 * %QCA_NL80211_VENDOR_SUBCMD_TDLS_GET_STATUS response.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_NUM_SESSIONS: u32 attribute. Indicates
 * the current number of active TDLS sessions. This is indicated in the response
 * when %QCA_NL80211_VENDOR_SUBCMD_TDLS_GET_STATUS is requested with
 * %NL80211_ATTR_VENDOR_DATA as an empty nested attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_AVAILABLE: Flag attribute. Indicates
 * whether the driver can initiate new TDLS session. This is indicated in the
 * response when %QCA_NL80211_VENDOR_SUBCMD_TDLS_GET_STATUS is requested with
 * %NL80211_ATTR_VENDOR_DATA as an empty nested attribute.
 */
enum qca_wlan_vendor_attr_tdls_get_status {
	QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAC_ADDR = 1,
	QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_STATE = 2,
	QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_REASON = 3,
	QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_CHANNEL = 4,
	QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_GLOBAL_OPERATING_CLASS = 5,
	QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_NUM_SESSIONS = 6,
	QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_AVAILABLE = 7,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAX =
	QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_tdls_state - Attributes used by
 * %QCA_NL80211_VENDOR_SUBCMD_TDLS_STATE vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_STATE_MAC_ADDR: 6-byte MAC address of the
 * peer station. Required attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_STATE_CURRENT_STATE: u32 attribute. Indicates
 * the current TDLS state. Required attribute. Uses the values from
 * enum qca_wlan_vendor_tdls_state.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_STATE_REASON: s32 attribute. Indicates the
 * reason of the current TDLS session state. Required attribute. Uses the values
 * from enum qca_wlan_vendor_tdls_reason.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_STATE_CHANNEL: u32 attribute. Indicates the
 * TDLS session channel. Required attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDLS_STATE_GLOBAL_OPERATING_CLASS: u32 attribute.
 * Indicates the TDLS session global operating class. Required attribute.
 */
enum qca_wlan_vendor_attr_tdls_state {
	QCA_WLAN_VENDOR_ATTR_TDLS_STATE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TDLS_STATE_MAC_ADDR = 1,
	QCA_WLAN_VENDOR_ATTR_TDLS_STATE_NEW_STATE = 2,
	QCA_WLAN_VENDOR_ATTR_TDLS_STATE_REASON = 3,
	QCA_WLAN_VENDOR_ATTR_TDLS_STATE_CHANNEL = 4,
	QCA_WLAN_VENDOR_ATTR_TDLS_STATE_GLOBAL_OPERATING_CLASS = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TDLS_STATE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TDLS_STATE_MAX =
	QCA_WLAN_VENDOR_ATTR_TDLS_STATE_AFTER_LAST - 1,
};

/*
 * enum qca_wlan_vendor_opm_mode - Modes supported by
 * %QCA_WLAN_VENDOR_ATTR_CONFIG_OPTIMIZED_POWER_MANAGEMENT vendor attribute.
 *
 * @QCA_WLAN_VENDOR_OPM_MODE_DISABLE: OPM Disabled
 * @QCA_WLAN_VENDOR_OPM_MODE_ENABLE: OPM Enabled
 * @QCA_WLAN_VENDOR_OPM_MODE_USER_DEFINED: User defined mode which allows user
 *	to configure power save inactivity timeout and speculative wake up
 *	interval through %QCA_WLAN_VENDOR_ATTR_CONFIG_OPM_ITO and
 *	%QCA_WLAN_VENDOR_ATTR_CONFIG_OPM_SPEC_WAKE_INTERVAL attributes.
 */

enum qca_wlan_vendor_opm_mode {
	QCA_WLAN_VENDOR_OPM_MODE_DISABLE = 0,
	QCA_WLAN_VENDOR_OPM_MODE_ENABLE = 1,
	QCA_WLAN_VENDOR_OPM_MODE_USER_DEFINED = 2,
};

/*
 * enum qca_wlan_vendor_tx_latency_type - Represents the possible latency
 * types.
 *
 * @QCA_WLAN_VENDOR_TX_LATENCY_TYPE_DRIVER: Per MSDU latency
 * from: An MSDU is presented to the driver
 * to: the MSDU is queued into TCL SRNG
 *
 * @QCA_WLAN_VENDOR_TX_LATENCY_TYPE_RING: Per MSDU latency
 * from: the MSDU is queued into TCL SRNG
 * to: the MSDU is released by the driver
 *
 * @QCA_WLAN_VENDOR_TX_LATENCY_TYPE_HW: Per MSDU latency
 * from: the MSDU is presented to the hardware
 * to: the MSDU is released by the hardware
 *
 * @QCA_WLAN_VENDOR_TX_LATENCY_TYPE_CCA: Per PPDU latency
 * The time spent on Clear Channel Assessment, the maximum value is 50000 (us)
 * from: A PPDU is presented to the hardware LMAC
 * to: over-the-air transmission is started for the PPDU
 */
enum qca_wlan_vendor_tx_latency_type {
	QCA_WLAN_VENDOR_TX_LATENCY_TYPE_DRIVER = 0,
	QCA_WLAN_VENDOR_TX_LATENCY_TYPE_RING = 1,
	QCA_WLAN_VENDOR_TX_LATENCY_TYPE_HW = 2,
	QCA_WLAN_VENDOR_TX_LATENCY_TYPE_CCA = 3,
};

/**
 * enum qca_wlan_vendor_attr_tx_latency_bucket - Definition of attributes
 * used inside nested attributes
 * %QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKETS and
 * %QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINK_STAT_BUCKETS.
 *
 * @QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_TYPE: u8 attribute.
 * Indicates the latency type.
 * See enum qca_wlan_vendor_tx_latency_type for the supported types.
 *
 * @QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_GRANULARITY: u32 attribute.
 * Indicates the granularity (in microseconds) of the distribution for the
 * type (specified by %QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_TYPE), the value
 * must be positive.
 * If %QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_TYPE is
 * %QCA_WLAN_VENDOR_TX_LATENCY_TYPE_CCA, the value must be an integer multiple
 * of 1000, and the maximum allowed value is 15000 (us).
 *
 * @QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_AVERAGE: u32 attribute.
 * Indicates the average of the latency (in microseconds) for the type
 * (specified by %QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_TYPE) within a cycle.
 * If there is no transmitted MSDUs/MPDUs during a cycle, this average is 0;
 * otherwise, it represents the quotient of <accumulated latency of the
 * transmitted MSDUs/MPDUs in a cycle> divided by <the number of the transmitted
 * MSDUs/MPDUs in a cycle>.
 *
 * @QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_DISTRIBUTION:
 * Array of u32, 4 elements in total, represents the latency distribution for
 * the type (specified by %QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_TYPE).
 * Each element holds the count of MSDUs/PPDUs (according to the latency type)
 * within a range:
 * element[0]: latency >= 0 && latency < granularity
 * element[1]: latency >= granularity && latency < granularity * 2
 * element[2]: latency >= granularity * 2 && latency < granularity * 3
 * element[3]: latency >= granularity * 3
 */
enum qca_wlan_vendor_attr_tx_latency_bucket {
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_TYPE = 1,
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_GRANULARITY = 2,
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_AVERAGE = 3,
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_DISTRIBUTION = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_MAX =
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_tx_latency_link - Definition of attributes
 * used inside nested attribute %QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINKS.
 *
 * @QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINK_MAC_REMOTE: 6-byte MAC address.
 * Indicates link MAC address of the remote peer. For example, when running
 * in station mode, it's the BSSID of the link; while when running in AP
 * mode, it's the link MAC address of the remote station.
 *
 * @QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINK_STAT_BUCKETS:
 * Array of nested attribute.
 * Represents the transmit latency statistics for the link specified by
 * %QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINK_MAC_REMOTE.
 * Each entry represents the statistics for one of the types defined in
 * enum qca_wlan_vendor_tx_latency_type.
 * Each defined type has and must have one entry.
 * See enum qca_wlan_vendor_attr_tx_latency_bucket for nested attributes.
 */
enum qca_wlan_vendor_attr_tx_latency_link {
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINK_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINK_MAC_REMOTE = 1,
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINK_STAT_BUCKETS = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINK_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINK_MAX =
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINK_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_tx_latency_action - Represents the possible actions
 * for %QCA_NL80211_VENDOR_SUBCMD_TX_LATENCY.
 *
 * @QCA_WLAN_VENDOR_TX_LATENCY_ACTION_DISABLE:
 * Disable transmit latency monitoring.
 *
 * @QCA_WLAN_VENDOR_TX_LATENCY_ACTION_ENABLE:
 * Enable transmit latency monitoring.
 *
 * @QCA_WLAN_VENDOR_TX_LATENCY_ACTION_GET:
 * Get transmit latency statistics of the last cycle (period is specified by
 * %QCA_WLAN_VENDOR_ATTR_TX_LATENCY_PERIOD).
 */
enum qca_wlan_vendor_tx_latency_action {
	QCA_WLAN_VENDOR_TX_LATENCY_ACTION_DISABLE = 0,
	QCA_WLAN_VENDOR_TX_LATENCY_ACTION_ENABLE = 1,
	QCA_WLAN_VENDOR_TX_LATENCY_ACTION_GET = 2,
};

/**
 * enum qca_wlan_vendor_attr_tx_latency - Definition of attributes used by
 * %QCA_NL80211_VENDOR_SUBCMD_TX_LATENCY to configure, retrieve, and report
 * per-link transmit latency statistics.
 *
 * There are 6 uses of %QCA_NL80211_VENDOR_SUBCMD_TX_LATENCY:
 * 1) used as a command to enable the feature
 * Precondition(s):
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_ACTION is
 *	%QCA_WLAN_VENDOR_TX_LATENCY_ACTION_ENABLE
 * Mandatory attribute(s):
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_ACTION,
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_PERIOD,
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKETS with nested attributes
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_TYPE,
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_GRANULARITY.
 * Notes:
 *	The driver will monitor the transmit latency for the active links
 *	and save the statistics for each cycle (period is set by
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_PERIOD) when the feature is enabled.
 *	Set flag %QCA_WLAN_VENDOR_ATTR_TX_LATENCY_PERIODIC_REPORT if periodical
 *	report is required.
 *
 * 2) used as a command to disable the feature
 * Precondition(s):
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_ACTION is
 *	%QCA_WLAN_VENDOR_TX_LATENCY_ACTION_DISABLE
 * Mandatory attribute(s):
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_ACTION
 *
 * 3) used as a command to retrieve the statistics for all the active links on
 *    the requested interface
 * Precondition(s):
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_ACTION is
 *	QCA_WLAN_VENDOR_TX_LATENCY_ACTION_GET and
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINKS is NOT present.
 * Mandatory attribute(s):
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_ACTION
 * Notes:
 *	The driver returns failure directly if the feature is not enabled or
 *	there is no active link.
 *	The driver returns the statistics of the last cycle in the case of
 *	success.
 *
 * 4) used as a command to retrieve the statistics for the specified links
 * Precondition(s):
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_ACTION is
 *	QCA_WLAN_VENDOR_TX_LATENCY_ACTION_GET and
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINKS is present.
 * Mandatory attribute(s):
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_ACTION,
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINKS, with nested attribute
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINK_MAC_REMOTE.
 * Notes:
 *	The driver returns failure directly if the feature is not enabled or
 *	any of the links (specified by %QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINKS)
 *	does not exist or is not in active state.
 *
 * 5) used as a command response for #3 or #4
 * Precondition(s):
 *	Userspace issues command #3 or #4, and the driver gets corresponding
 *	statistics successfully.
 * Mandatory attribute(s):
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINKS, with nested attributes
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINK_MAC_REMOTE,
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINK_STAT_BUCKETS with nested
 *	attributes %QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_TYPE,
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_GRANULARITY,
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_AVERAGE and
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_DISTRIBUTION.
 *
 * 6) used as an asynchronous event to report the statistics periodically
 * Precondition(s):
 *	Userspace set flag %QCA_WLAN_VENDOR_ATTR_TX_LATENCY_PERIODIC_REPORT in
 *	#1.
 *	One or more links are in active state.
 * Mandatory attribute(s):
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINKS, with nested attributes
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINK_MAC_REMOTE,
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINK_STAT_BUCKETS with nested
 *	attributes %QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_TYPE,
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_GRANULARITY,
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_AVERAGE and
 *	%QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKET_DISTRIBUTION.
 *
 * @QCA_WLAN_VENDOR_ATTR_TX_LATENCY_INVALID: Invalid attribute
 *
 * @QCA_WLAN_VENDOR_ATTR_TX_LATENCY_ACTION: u32 attribute.
 * Action to take in this vendor command.
 * See enum qca_wlan_vendor_tx_latency_action for supported actions.
 *
 * @QCA_WLAN_VENDOR_ATTR_TX_LATENCY_PERIODIC_REPORT: Flag attribute.
 * Enable (flag attribute present) - The driver needs to report transmit latency
 * statistics at the end of each statistical period.
 * Disable (flag attribute not present) - The driver doesn't need to report
 * transmit latency statistics periodically.
 *
 * @QCA_WLAN_VENDOR_ATTR_TX_LATENCY_PERIOD: u32 attribute.
 * Indicates statistical period for transmit latency in terms of milliseconds,
 * the minimal allowed value is 100 and the maximum allowed value is 60000.
 *
 * @QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKETS: Array of nested attribute.
 * Each entry represents the latency buckets configuration for one of the types
 * defined in enum qca_wlan_vendor_tx_latency_type.
 * Each defined type has and must have one entry.
 * See enum qca_wlan_vendor_attr_tx_latency_bucket for the list of
 * supported attributes.
 *
 * @QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINKS: Array of nested attribute.
 * Information of the links, each entry represents for one link.
 * See enum qca_wlan_vendor_attr_tx_latency_link for the list of
 * supported attributes for each entry.
 */
enum qca_wlan_vendor_attr_tx_latency {
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_ACTION = 1,
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_PERIODIC_REPORT = 2,
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_PERIOD = 3,
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_BUCKETS = 4,
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_LINKS = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_MAX =
	QCA_WLAN_VENDOR_ATTR_TX_LATENCY_AFTER_LAST - 1,
};

/**
 * enum qca_chan_width_update_type - Represents the possible values for
 * %QCA_WLAN_VENDOR_ATTR_CONFIG_CHAN_WIDTH_UPDATE_TYPE.
 *
 * @QCA_CHAN_WIDTH_UPDATE_TYPE_TX_RX: The maximum allowed bandwidth change is
 * applicable for both Tx and Rx paths. The driver shall conduct OMI operation
 * as defined in 26.9 (Operating mode indication) or OMN operation as defined in
 * 11.40 (Notification of operating mode changes) in IEEE P802.11-REVme/D2.0
 * with AP to indicate the change in the maximum allowed operating bandwidth.
 *
 * @QCA_CHAN_WIDTH_UPDATE_TYPE_TX_ONLY: Limit the change in maximum allowed
 * bandwidth only to Tx path. In this case the driver doesn't need to conduct
 * OMI/OMN operation since %QCA_WLAN_VENDOR_ATTR_CONFIG_CHANNEL_WIDTH is
 * expected to be less than the current connection maximum negotiated bandwidth.
 * For example: Negotiated maximum bandwidth is 160 MHz and the new maximum
 * bandwidth configured is 80 MHz, now the driver limits the maximum bandwidth
 * to 80 MHz only in the Tx path.
 *
 * @QCA_CHAN_WIDTH_UPDATE_TYPE_TX_RX_EXT: This is similar to
 * %QCA_CHAN_WIDTH_UPDATE_TYPE_TX_RX but the driver doesn't change current
 * phymode bandwidth to avoid interoperability issues with APs which don't
 * handle the maximum bandwidth change indication correctly.
 * For example: Negotiated maximum bandwidth is 40 MHz and the new maximum
 * bandwidth configured is 20 MHz, now the driver indicates the change in
 * maximum allowed bandwidth to the AP and limits the bandwidth to 20 MHz in the
 * Tx path but keeps the phymode bandwidth as 40 MHz. This will avoid
 * interoperability issues with APs which still use 40 MHz for sending the
 * frames though it received maximum allowed bandwidth indication as 20 MHz
 * from the STA.
 */
enum qca_chan_width_update_type {
	QCA_CHAN_WIDTH_UPDATE_TYPE_TX_RX = 0,
	QCA_CHAN_WIDTH_UPDATE_TYPE_TX_ONLY = 1,
	QCA_CHAN_WIDTH_UPDATE_TYPE_TX_RX_EXT = 2,
};

/**
 * enum qca_wlan_vendor_attr_tpc_pwr_level - Definition of attributes
 * used inside nested attribute %QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL.
 *
 * @QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL_FREQUENCY: u32 channel center
 * frequency (MHz): If PSD power, carries one 20 MHz sub-channel center
 * frequency. If non PSD power, carries either 20 MHz bandwidth's center
 * channel frequency or 40 MHz bandwidth's center channel frequency
 * (or 80/160 MHz bandwidth's center channel frequency).
 *
 * @QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL_TX_POWER: s8 transmit power limit (dBm).
 * If PSD power, carries PSD power value of the
 * QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL_FREQUENCY mentioned sub-channel.
 * If non PSD power, carries EIRP power value of bandwidth mentioned
 * by QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL_FREQUENCY center frequency.
 */
enum qca_wlan_vendor_attr_tpc_pwr_level {
	QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL_FREQUENCY = 1,
	QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL_TX_POWER = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL_MAX =
	QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_tpc - Definition of link attributes
 * used inside nested attribute %QCA_WLAN_VENDOR_ATTR_TPC_LINKS.
 *
 * @QCA_WLAN_VENDOR_ATTR_TPC_BSSID: 6-bytes AP BSSID.
 * For MLO STA, AP BSSID indicates the AP's link address.
 *
 * @QCA_WLAN_VENDOR_ATTR_TPC_PSD_POWER: PSD power flag
 * Indicates using PSD power mode if this flag is present.
 *
 * @QCA_WLAN_VENDOR_ATTR_TPC_EIRP_POWER: s8 Regulatory EIRP power
 * value in dBm
 *
 * @QCA_WLAN_VENDOR_ATTR_TPC_POWER_TYPE_6GHZ: u8 power type of 6 GHz
 * AP, refer to Table E-12-Regulatory Info subfield encoding in
 * IEEE P802.11-REVme/D4.0. Only present if link is connected to 6 GHz AP.
 *
 * @QCA_WLAN_VENDOR_ATTR_TPC_AP_CONSTRAINT_POWER: u8 Local Power Constraint
 * (dBm) advertised by AP in Power Constraint element, refer to
 * IEEE Std 802.11-2020, 9.4.2.13 Power Constraint element.
 *
 * @QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL: A nested attribute containing
 * attributes defined by enum qca_wlan_vendor_attr_tpc_pwr_level.
 * If PSD power, each power level describes each 20 MHz subchannel PSD
 * power value. If non PSD power, each power level describes each supported
 * bandwidth's EIRP power value (up to Max bandwidth of AP operating on),
 * each level attribute contains corresponding bandwidth's center channel
 * frequency and its EIRP power value.
 */
enum qca_wlan_vendor_attr_tpc {
	QCA_WLAN_VENDOR_ATTR_TPC_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TPC_BSSID = 1,
	QCA_WLAN_VENDOR_ATTR_TPC_PSD_POWER = 2,
	QCA_WLAN_VENDOR_ATTR_TPC_EIRP_POWER = 3,
	QCA_WLAN_VENDOR_ATTR_TPC_POWER_TYPE_6GHZ = 4,
	QCA_WLAN_VENDOR_ATTR_TPC_AP_CONSTRAINT_POWER = 5,
	QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL = 6,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TPC_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TPC_MAX =
	QCA_WLAN_VENDOR_ATTR_TPC_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_tpc_links - Definition of attributes
 * for %QCA_NL80211_VENDOR_SUBCMD_REGULATORY_TPC_INFO subcommand
 *
 * @QCA_WLAN_VENDOR_ATTR_TPC_LINKS: A nested attribute containing
 * per-link TPC information of all the active links of MLO STA.
 * For non MLO STA, only one link TPC information will be returned
 * for connected AP in this nested attribute.
 * The attributes used inside this nested attributes are defined
 * in enum qca_wlan_vendor_attr_tpc.
 */
enum qca_wlan_vendor_attr_tpc_links {
	QCA_WLAN_VENDOR_ATTR_TPC_LINKS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TPC_LINKS = 1,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TPC_LINKS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TPC_LINKS_MAX =
	QCA_WLAN_VENDOR_ATTR_TPC_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_fw_page_fault_report - Used by the vendor
 * command %QCA_NL80211_VENDOR_SUBCMD_FW_PAGE_FAULT_REPORT.
 *
 * @QCA_WLAN_VENDOR_ATTR_FW_PAGE_FAULT_REPORT_DATA: The binary blob data
 * associated with the firmware page fault that is expected to contain the
 * required dump to analyze frequent page faults.
 * NLA_BINARY attribute, the maximum size is 1024 bytes.
 */
enum qca_wlan_vendor_attr_fw_page_fault_report {
	QCA_WLAN_VENDOR_ATTR_FW_PAGE_FAULT_REPORT_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_FW_PAGE_FAULT_REPORT_DATA = 1,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_FW_PAGE_FAULT_REPORT_LAST,
	QCA_WLAN_VENDOR_ATTR_FW_PAGE_FAULT_REPORT_MAX =
	QCA_WLAN_VENDOR_ATTR_FW_PAGE_FAULT_REPORT_LAST - 1,
};

/**
 * enum qca_wlan_btm_support: BTM support configuration
 *
 * @QCA_WLAN_BTM_SUPPORT_DEFAULT: Restore default BTM support policy. The driver
 * follows the BSS Transition bit in the Extended Capabilities element from the
 * connect request IEs with the default BTM support policy.
 *
 * @QCA_WLAN_BTM_SUPPORT_DISABLE: Disable BTM support for the subsequent
 * (re)association attempts. The driver shall restore the default BTM support
 * policy during the first disconnection after successful association. When this
 * configuration is enabled, the driver shall overwrite the BSS Transition bit
 * as zero in the Extended Capabilities element while sending (Re)Association
 * Request frames. Also, the driver shall drop the BTM frames from userspace and
 * the connected AP when this configuration is enabled.
 */
enum qca_wlan_btm_support {
	QCA_WLAN_BTM_SUPPORT_DEFAULT = 0,
	QCA_WLAN_BTM_SUPPORT_DISABLE = 1,
};

/**
 * enum qca_wlan_vendor_data_rate_type - Represents the possible values for
 * attribute %QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_RATE_TYPE.
 *
 * @QCA_WLAN_VENDOR_DATA_RATE_TYPE_LEGACY: Data rate type is a legacy rate code
 * used in OFDM/CCK.
 *
 * @QCA_WLAN_VENDOR_DATA_RATE_TYPE_MCS: Data rate type is an MCS index.
 *
 */
enum qca_wlan_vendor_data_rate_type {
	QCA_WLAN_VENDOR_DATA_RATE_TYPE_LEGACY = 0,
	QCA_WLAN_VENDOR_DATA_RATE_TYPE_MCS = 1,
};

/**
 * enum qca_wlan_vendor_attr_adjust_tx_power_rate - Definition
 * of data rate related attributes which is used inside nested attribute
 * %QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_CHAIN_RATE_CONFIG.
 *
 * @QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_RATE_TYPE: u8 data rate type.
 * For this attribute, valid values are enumerated in enum
 * %qca_wlan_vendor_data_rate_type.
 *
 * @QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_RATE_VALUE: u8 value.
 * This attribute value is interpreted according to the value of attribute
 * %QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_RATE_TYPE. For legacy config
 * type, this attribute value is defined in the units of 0.5 Mbps.
 * For non legacy config type, this attribute carries the MCS index number.
 *
 * @QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_RATE_POWER_VALUE: u8 value in dBm.
 * Usually the target computes a final transmit power that is the maximum
 * power level that doesn't exceed the limits enforced by various sources
 * like chip-specific conformance test limits (CTL), Specific Absorption
 * Rate (SAR), Transmit Power Control (TPC), wiphy-specific limits, STA-specific
 * limits, channel avoidance limits, Automated Frequency Coordination (AFC),
 * and others. In some cases it may be desirable to use a power level that is
 * lower than the maximum power level allowed by all of these limits, so this
 * attribute provides an additional limit that can be used to reduce the
 * transmit power level.
 *
 */
enum qca_wlan_vendor_attr_adjust_tx_power_rate {
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_RATE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_RATE_TYPE = 1,
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_RATE_VALUE = 2,
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_RATE_POWER_VALUE = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_CONFIG_MAX =
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_CONFIG_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_adjust_tx_power_chain_config - Definition
 * of chain related attributes which is used inside nested attribute
 * %QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_BAND_CHAIN_CONFIG.
 *
 * @QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_CHAIN_INDEX: u8 value.
 * Represents a particular chain for which transmit power adjustment needed.
 *
 * @QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_CHAIN_RATE_CONFIG: A nested
 * attribute containing data rate related information to adjust transmit
 * power. The attributes used inside this nested attributes are defined in
 * enum qca_wlan_vendor_attr_adjust_tx_power_rate.
 */
enum qca_wlan_vendor_attr_adjust_tx_power_chain_config {
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_CHAIN_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_CHAIN_INDEX = 1,
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_CHAIN_RATE_CONFIG = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_CHAIN_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_CHAIN_MAX =
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_CHAIN_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_adjust_tx_power_band_config - Definition
 * of band related attributes which is used inside nested attribute
 * %QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_BAND_CONFIG.
 *
 * @QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_BAND_INDEX: u8 value to
 * indicate band for which configuration applies. Valid values are enumerated
 * in enum %nl80211_band.
 *
 * @QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_BAND_CHAIN_CONFIG: A nested
 * attribute containing per chain related information to adjust transmit
 * power. The attributes used inside this nested attribute are defined in
 * enum qca_wlan_vendor_attr_adjust_tx_power_chain_config.
 *
 */
enum qca_wlan_vendor_attr_adjust_tx_power_band_config {
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_BAND_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_BAND_INDEX = 1,
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_BAND_CHAIN_CONFIG = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_BAND_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_BAND_MAX =
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_BAND_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_adjust_tx_power - Definition of attributes
 * for %QCA_NL80211_VENDOR_SUBCMD_ADJUST_TX_POWER subcommand.
 *
 * @QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_BAND_CONFIG: A nested attribute
 * containing per band related information to adjust transmit power.
 * The attributes used inside this nested attributes are defined in
 * enum qca_wlan_vendor_attr_adjust_tx_power_band_config.
 */
enum qca_wlan_vendor_attr_adjust_tx_power {
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_BAND_CONFIG = 1,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_MAX =
	QCA_WLAN_VENDOR_ATTR_ADJUST_TX_POWER_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_spectral_data_transport_mode - Attribute
 * values for QCA_WLAN_VENDOR_ATTR_SPECTRAL_DATA_TRANSPORT_MODE.
 *
 * @QCA_WLAN_VENDOR_SPECTRAL_DATA_TRANSPORT_NETLINK: Use netlink to
 * send spectral data to userspace applications.
 * @QCA_WLAN_VENDOR_SPECTRAL_DATA_TRANSPORT_RELAY: Use relay interface
 * to send spectral data to userspace applications.
 */
enum qca_wlan_vendor_spectral_data_transport_mode {
	QCA_WLAN_VENDOR_SPECTRAL_DATA_TRANSPORT_NETLINK = 0,
	QCA_WLAN_VENDOR_SPECTRAL_DATA_TRANSPORT_RELAY = 1,
};

/* enum qca_wlan_vendor_spectral_scan_complete_status - Attribute
 * values for QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_STATUS to
 * indicate the completion status for a spectral scan.
 *
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_COMPLETE_STATUS_SUCCESSFUL:
 * Indicates a successful completion of the scan.
 *
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_COMPLETE_STATUS_TIMEOUT: Indicates
 * a timeout has occured while processing the spectral reports.
 */
enum qca_wlan_vendor_spectral_scan_complete_status {
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_COMPLETE_STATUS_SUCCESSFUL = 0,
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_COMPLETE_STATUS_TIMEOUT = 1,
};

/* enum qca_wlan_vendor_attr_spectral_scan_complete - Definition of
 * attributes for @QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_COMPLETE
 * to indicate scan status and samples received from hardware.
 *
 * @QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_INVALID: Invalid attribute
 *
 * @QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_STATUS: u32 attribute.
 * Indicates completion status, either the scan is successful or a timeout
 * is issued by the driver.
 * See enum qca_wlan_vendor_spectral_scan_complete_status.
 *
 * @QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_RECEIVED_SAMPLES: u32
 * attribute. Number of spectral samples received after the scan has started.
 */
enum qca_wlan_vendor_attr_spectral_scan_complete {
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_STATUS = 1,
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_RECEIVED_SAMPLES = 2,

	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_MAX =
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_async_get_station_attr - Attribute values for
 * %QCA_NL80211_VENDOR_SUBCMD_ASYNC_GET_STATION command.
 *
 * @QCA_WLAN_VENDOR_ATTR_ASYNC_GET_STATION_CONFIG: 8-bit unsigned value to
 * configure the driver to enable/disable reporting
 * %QCA_NL80211_VENDOR_SUBCMD_ASYNC_GET_STATION events. 1-Enable, 0-Disable.
 * This is required in a command.
 *
 * @QCA_WLAN_VENDOR_ATTR_ASYNC_GET_STATION_RESPONSE: Nested attribute. This is
 * required in %QCA_NL80211_VENDOR_SUBCMD_ASYNC_GET_STATION event.
 * This attribute is nested with the station MAC address in %NL80211_ATTR_MAC
 * and the station information in %NL80211_ATTR_STA_INFO nested attribute, see
 * enum nl80211_sta_info.
 */
enum qca_wlan_vendor_async_get_station_attr {
	QCA_WLAN_VENDOR_ATTR_ASYNC_GET_STATION_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ASYNC_GET_STATION_CONFIG = 1,
	QCA_WLAN_VENDOR_ATTR_ASYNC_GET_STATION_RESPONSE = 2,

	QCA_WLAN_VENDOR_ATTR_ASYNC_GET_STATION_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ASYNC_GET_STATION_MAX =
	QCA_WLAN_VENDOR_ATTR_ASYNC_GET_STATION_AFTER_LAST - 1,
};

/* enum qca_wlan_vendor_ap_suspend_state - Attribute values for
 * QCA_WLAN_VENDOR_ATTR_AP_SUSPEND_STATE.
 *
 * @QCA_WLAN_VENDOR_AP_SUSPEND_STATE_DISABLE: Disable suspend state. When used
 * with a command, the driver resumes AP with the same configuration that was
 * applied earlier and starts all TX/RX operations. When used in an event,
 * indicates the AP interface resumed.
 *
 * @QCA_WLAN_VENDOR_AP_SUSPEND_STATE_ENABLE: Enable suspend state. In this
 * mode, all associated STAs are disconnected and TX/RX is stopped. While an AP
 * is in this state, it allows only %QCA_WLAN_VENDOR_AP_SUSPEND_STATE_DISABLE
 * or AP stop/teardown operations. When used in an event, indicates the AP
 * interface suspended.
 */
enum qca_wlan_vendor_ap_suspend_state {
	QCA_WLAN_VENDOR_AP_SUSPEND_STATE_DISABLE = 0,
	QCA_WLAN_VENDOR_AP_SUSPEND_STATE_ENABLE = 1,
};

/* enum qca_wlan_vendor_attr_ap_suspend - Definition of attributes for
 * @QCA_NL80211_VENDOR_SUBCMD_AP_SUSPEND to configure/notify the suspend state.
 *
 * @QCA_WLAN_VENDOR_ATTR_AP_SUSPEND_STATE: u8 attribute to configure/notify
 * suspend state defined in enum qca_wlan_vendor_ap_suspend_state.
 */
enum qca_wlan_vendor_attr_ap_suspend {
	QCA_WLAN_VENDOR_ATTR_AP_SUSPEND_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_AP_SUSPEND_STATE = 1,

	QCA_WLAN_VENDOR_ATTR_AP_SUSPEND_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_AP_SUSPEND_MAX =
	QCA_WLAN_VENDOR_ATTR_AP_SUSPEND_AFTER_LAST - 1,
};

/**
 * enum qca_traffic_type - Traffic types into which the flows can be classified.
 * @QCA_TRAFFIC_TYPE_STREAMING: Traffic type is streaming
 * @QCA_TRAFFIC_TYPE_GAMING: Traffic type is gaming
 * @QCA_TRAFFIC_TYPE_VOICE_CALL: Traffic type is a voice call
 * @QCA_TRAFFIC_TYPE_VIDEO_CALL: Traffic type is a video call
 * @QCA_TRAFFIC_TYPE_SCREEN_SHARE: Traffic type is screen share
 * @QCA_TRAFFIC_TYPE_UNKNOWN: Traffic type is unknown
 * @QCA_TRAFFIC_TYPE_INVALID: Invalid traffic type
 */
enum qca_traffic_type {
	QCA_TRAFFIC_TYPE_STREAMING = 0,
	QCA_TRAFFIC_TYPE_GAMING = 1,
	QCA_TRAFFIC_TYPE_VOICE_CALL = 2,
	QCA_TRAFFIC_TYPE_VIDEO_CALL = 3,
	QCA_TRAFFIC_TYPE_SCREEN_SHARE = 4,
	QCA_TRAFFIC_TYPE_UNKNOWN = 5,
	QCA_TRAFFIC_TYPE_INVALID = 6,
};

/**
 * enum qca_wlan_vendor_flow_tuple_proto - Definition of the values to specify
 * the flow tuple protocol in QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_PROTOCOL.
 *
 * @QCA_WLAN_VENDOR_FLOW_TUPLE_PROTO_UDP: UDP flow
 *
 * @QCA_WLAN_VENDOR_FLOW_TUPLE_PROTO_TCP: TCP flow
 */
enum qca_wlan_vendor_flow_tuple_proto {
	QCA_WLAN_VENDOR_FLOW_TUPLE_PROTO_UDP = 0,
	QCA_WLAN_VENDOR_FLOW_TUPLE_PROTO_TCP = 1,
};

/**
 * enum qca_wlan_vendor_attr_flow_tuple - Definition of attributes to specify a
 * flow tuple.
 *
 * @QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_IPV4_SRC_ADDR: Optional u32 attribute
 * indicates the source IPv4 address (in network byte order).
 *
 * @QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_IPV4_DST_ADDR: Optional u32 attribute
 * indicates the destination IPv4 address (in network byte order).
 *
 * @QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_IPV6_SRC_ADDR: Optional NLA_BINARY
 * attribute of 16 bytes length that indicates the source IPv6 address
 * (in network byte order) for a flow.
 *
 * @QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_IPV6_DST_ADDR: Optional NLA_BINARY
 * attribute of 16 bytes length that indicates the destination IPv6 address
 * (in network byte order) for a flow.
 *
 * @QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_SRC_PORT: Mandatory u16 attribute indicates
 * the TCP/UDP source port.
 *
 * @QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_DST_PORT: Mandatory u16 attribute indicates
 * the TCP/UDP destination port.
 *
 * @QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_PROTOCOL: Mandatory u8 attribute indicates
 * the flow protocol. Uses the enum qca_wlan_vendor_flow_tuple_proto.
 *
 * IPv4 flows have to specify @QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_IPV4_SRC_ADDR
 * and @QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_IPV4_DST_ADDR.
 * IPv6 flows have to specify @QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_IPV6_SRC_ADDR
 * and @QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_IPV6_DST_ADDR.
 */
enum qca_wlan_vendor_attr_flow_tuple {
	QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_IPV4_SRC_ADDR = 1,
	QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_IPV4_DST_ADDR = 2,
	QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_IPV6_SRC_ADDR = 3,
	QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_IPV6_DST_ADDR = 4,
	QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_SRC_PORT = 5,
	QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_DST_PORT = 6,
	QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_PROTOCOL = 7,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_LAST,
	QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_MAX =
	QCA_WLAN_VENDOR_ATTR_FLOW_TUPLE_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_txrx_stats - Definition of attributes to specify
 * TX/RX sample for one window.
 *
 * @QCA_WLAN_VENDOR_ATTR_TXRX_STATS_NUM_BYTES: Mandatory u64 attribute indicates
 * the total number of uplink/downlink bytes within the sampling window.
 *
 * @QCA_WLAN_VENDOR_ATTR_TXRX_STATS_NUM_PKTS: Mandatory u32 attribute indicates
 * the total number of packets (uplink/downlink) within the sampling window.
 *
 * @QCA_WLAN_VENDOR_ATTR_TXRX_STATS_PKT_SIZE_MIN: Mandatory u32 attribute
 * indicates the minimum uplink/downlink packet size (in bytes) during the
 * sampling window.
 *
 * @QCA_WLAN_VENDOR_ATTR_TXRX_STATS_PKT_SIZE_MAX: Mandatory u32 attribute
 * indicates the maximum uplink/downlink packet size (in bytes) during the
 * sampling window.
 *
 * @QCA_WLAN_VENDOR_ATTR_TXRX_STATS_PKT_IAT_MIN: Mandatory u64 attribute
 * indicates the minimum uplink/downlink packet IAT (inter-arrival time)
 * in microseconds, during the sampling window.
 *
 * @QCA_WLAN_VENDOR_ATTR_TXRX_STATS_PKT_IAT_MAX: Mandatory u64 attribute
 * indicates the maximum uplink/downlink packet IAT (inter-arrival time)
 * in microseconds, during the sampling window.
 *
 * @QCA_WLAN_VENDOR_ATTR_TXRX_STATS_PKT_IAT_SUM: Mandatory u64 attribute
 * indicates the sum of all the values of uplink/downlink packet IAT
 * (inter-arrival time) in microseconds, during the sampling window.
 * This attribute is used to calculate the mean packet (inter-arrival time)
 * during the sampling window.
 */
enum qca_wlan_vendor_attr_txrx_stats {
	QCA_WLAN_VENDOR_ATTR_TXRX_STATS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TXRX_STATS_NUM_BYTES = 1,
	QCA_WLAN_VENDOR_ATTR_TXRX_STATS_NUM_PKTS = 2,
	QCA_WLAN_VENDOR_ATTR_TXRX_STATS_PKT_SIZE_MIN = 3,
	QCA_WLAN_VENDOR_ATTR_TXRX_STATS_PKT_SIZE_MAX = 4,
	QCA_WLAN_VENDOR_ATTR_TXRX_STATS_PKT_IAT_MIN = 5,
	QCA_WLAN_VENDOR_ATTR_TXRX_STATS_PKT_IAT_MAX = 6,
	QCA_WLAN_VENDOR_ATTR_TXRX_STATS_PKT_IAT_SUM = 7,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TXRX_STATS_LAST,
	QCA_WLAN_VENDOR_ATTR_TXRX_STATS_MAX =
	QCA_WLAN_VENDOR_ATTR_TXRX_STATS_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_txrx_samples_windows - Definition of attributes
 * to specify the TX/RX statistics collected in a sampling window.
 *
 * @QCA_WLAN_VENDOR_ATTR_TXRX_SAMPLES_WINDOWS_WINDOW_SIZE: Mandatory u32
 * attribute indicates window size for packet TX/RX sampling (in milliseconds).
 *
 * @QCA_WLAN_VENDOR_ATTR_TXRX_SAMPLES_WINDOWS_UL_STATS: Mandatory nested
 * attribute containing the uplink TX/RX packet statistics for a flow. Uses the
 * enum qca_wlan_vendor_attr_txrx_stats.
 *
 * @QCA_WLAN_VENDOR_ATTR_TXRX_SAMPLES_WINDOWS_DL_STATS: Mandatory nested
 * attribute containing the downlink TX/RX packet statistics for a flow. Uses
 * the enum qca_wlan_vendor_attr_txrx_stats.
 */
enum qca_wlan_vendor_attr_txrx_samples_windows {
	QCA_WLAN_VENDOR_ATTR_TXRX_SAMPLES_WINDOWS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TXRX_SAMPLES_WINDOWS_WINDOW_SIZE = 1,
	QCA_WLAN_VENDOR_ATTR_TXRX_SAMPLES_WINDOWS_UL_STATS = 2,
	QCA_WLAN_VENDOR_ATTR_TXRX_SAMPLES_WINDOWS_DL_STATS = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TXRX_SAMPLES_WINDOWS_LAST,
	QCA_WLAN_VENDOR_ATTR_TXRX_SAMPLES_WINDOWS_MAX =
	QCA_WLAN_VENDOR_ATTR_TXRX_SAMPLES_WINDOWS_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_txrx_samples - Definition of attributes to specify
 * a TX/RX sample.
 *
 * @QCA_WLAN_VENDOR_ATTR_TXRX_SAMPLES_WINDOWS: Mandatory array of nested
 * attributes that indicates the TX/RX samples in multiple overlapping windows.
 * This uses the attributes defined by
 * enum qca_wlan_vendor_attr_txrx_samples_windows.
 */
enum qca_wlan_vendor_attr_txrx_samples {
	QCA_WLAN_VENDOR_ATTR_TXRX_SAMPLES_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TXRX_SAMPLES_WINDOWS = 1,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TXRX_SAMPLES_LAST,
	QCA_WLAN_VENDOR_ATTR_TXRX_SAMPLES_MAX =
	QCA_WLAN_VENDOR_ATTR_TXRX_SAMPLES_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_burst_stats - Definition of attribute to specify
 * burst statistics.
 *
 * @QCA_WLAN_VENDOR_ATTR_BURST_STATS_BURST_DURATION_MIN: Mandatory u32 attribute
 * indicates minimum burst duration (in milliseconds) during the sampling
 * window.
 *
 * @QCA_WLAN_VENDOR_ATTR_BURST_STATS_BURST_DURATION_MAX: Mandatory u32 attribute
 * indicates maximum burst duration (in milliseconds) during the sampling
 * window.
 *
 * @QCA_WLAN_VENDOR_ATTR_BURST_STATS_BURST_DURATION_SUM: Mandatory u64 attribute
 * indicates the sum of all the values of burst duration (in milliseconds)
 * during the sampling window. This attribute is used to calculate the mean
 * burst duration (in milliseconds) during the sampling window.
 *
 * @QCA_WLAN_VENDOR_ATTR_BURST_STATS_BURST_SIZE_MIN: Mandatory u64 attribute
 * indicates minimum burst size (in bytes) during the sampling window.
 *
 * @QCA_WLAN_VENDOR_ATTR_BURST_STATS_BURST_SIZE_MAX: Mandatory u64 attribute
 * indicates maximum burst size (in bytes) during the sampling window.
 *
 * @QCA_WLAN_VENDOR_ATTR_BURST_STATS_BURST_SIZE_SUM: Mandatory u64 attribute
 * indicates the sum of all the values of burst size (in bytes) during the
 * sampling window. This attribute is used to calculate the mean burst size
 * (in bytes) during the sampling window.
 *
 * @QCA_WLAN_VENDOR_ATTR_BURST_STATS_BURST_COUNT: Mandatory u32 attribute
 * indicates the number of bursts during the sampling window.
 */
enum qca_wlan_vendor_attr_burst_stats {
	QCA_WLAN_VENDOR_ATTR_BURST_STATS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_BURST_STATS_BURST_DURATION_MIN = 1,
	QCA_WLAN_VENDOR_ATTR_BURST_STATS_BURST_DURATION_MAX = 2,
	QCA_WLAN_VENDOR_ATTR_BURST_STATS_BURST_DURATION_SUM = 3,
	QCA_WLAN_VENDOR_ATTR_BURST_STATS_BURST_SIZE_MIN = 4,
	QCA_WLAN_VENDOR_ATTR_BURST_STATS_BURST_SIZE_MAX = 5,
	QCA_WLAN_VENDOR_ATTR_BURST_STATS_BURST_SIZE_SUM = 6,
	QCA_WLAN_VENDOR_ATTR_BURST_STATS_BURST_COUNT = 7,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_BURST_STATS_LAST,
	QCA_WLAN_VENDOR_ATTR_BURST_STATS_MAX =
	QCA_WLAN_VENDOR_ATTR_BURST_STATS_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_burst_sample - Definition of attributes to specify
 * a burst sample.
 *
 * @QCA_WLAN_VENDOR_ATTR_BURST_SAMPLES_TXRX_STATS: Mandatory nested attribute
 * indicates the uplink and downlink packet statistics collected in a
 * sampling window, containing attributes defined in
 * enum qca_wlan_vendor_attr_txrx_samples_windows.
 *
 * @QCA_WLAN_VENDOR_ATTR_BURST_SAMPLES_UL_BURST_STATS: Optional nested attribute
 * indicates the uplink burst stats, containing attributes defined in
 * enum qca_wlan_vendor_attr_burst_stats.
 *
 * @QCA_WLAN_VENDOR_ATTR_BURST_SAMPLES_DL_BURST_STATS: Optional nested attribute
 * indicates the downlink burst stats, containing attributes defined in
 * enum qca_wlan_vendor_attr_burst_stats.
 */
enum qca_wlan_vendor_attr_burst_sample {
	QCA_WLAN_VENDOR_ATTR_BURST_SAMPLES_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_BURST_SAMPLES_TXRX_STATS = 1,
	QCA_WLAN_VENDOR_ATTR_BURST_SAMPLES_UL_BURST_STATS = 2,
	QCA_WLAN_VENDOR_ATTR_BURST_SAMPLES_DL_BURST_STATS = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_BURST_SAMPLES_LAST,
	QCA_WLAN_VENDOR_ATTR_BURST_SAMPLES_MAX =
	QCA_WLAN_VENDOR_ATTR_BURST_SAMPLES_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_flow_stats - Definition of attribute used by
 * %QCA_NL80211_VENDOR_SUBCMD_CLASSIFIED_FLOW_REPORT and
 * %QCA_NL80211_VENDOR_SUBCMD_FLOW_STATS.
 *
 * Presence of one of the attributes
 * @QCA_WLAN_VENDOR_ATTR_FLOW_STATS_TXRX_SAMPLES and
 * @QCA_WLAN_VENDOR_ATTR_FLOW_STATS_BURST_SAMPLES is mandatory.
 *
 * @QCA_WLAN_VENDOR_ATTR_FLOW_STATS_FLOW_TUPLE: Mandatory nested attribute
 * containing the flow tuple of the flow for which the statistics are being
 * reported.
 * Uses the attributes defined by enum qca_wlan_vendor_attr_flow_tuple.
 *
 * @QCA_WLAN_VENDOR_ATTR_FLOW_STATS_TRAFFIC_TYPE: Optional u8 attribute
 * indicates the traffic type classified for this flow tuple. Uses the
 * enum qca_traffic_type values.
 * This attribute is mandatory for the command
 * @QCA_NL80211_VENDOR_SUBCMD_CLASSIFIED_FLOW_REPORT.
 *
 * @QCA_WLAN_VENDOR_ATTR_FLOW_STATS_TXRX_SAMPLES: Optional nested attribute
 * containing nested array of TX/RX samples defined in
 * enum qca_wlan_vendor_attr_txrx_samples.
 *
 * @QCA_WLAN_VENDOR_ATTR_FLOW_STATS_BURST_SAMPLES: Optional nested attribute
 * indicates the packet burst statistics for a flow. Uses attributes defined by
 * enum qca_wlan_vendor_attr_burst_sample.
 */
enum qca_wlan_vendor_attr_flow_stats {
	QCA_WLAN_VENDOR_ATTR_FLOW_STATS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_FLOW_STATS_FLOW_TUPLE = 1,
	QCA_WLAN_VENDOR_ATTR_FLOW_STATS_TRAFFIC_TYPE = 2,
	QCA_WLAN_VENDOR_ATTR_FLOW_STATS_TXRX_SAMPLES = 3,
	QCA_WLAN_VENDOR_ATTR_FLOW_STATS_BURST_SAMPLES = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_FLOW_STATS_LAST,
	QCA_WLAN_VENDOR_ATTR_FLOW_STATS_MAX =
	QCA_WLAN_VENDOR_ATTR_FLOW_STATS_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_flow_classify_result - Definition of attributes to
 * specify the flow classification result. This enum is used by
 * @QCA_NL80211_VENDOR_SUBCMD_FLOW_CLASSIFY_RESULT.
 *
 * @QCA_WLAN_VENDOR_ATTR_FLOW_CLASSIFY_RESULT_FLOW_TUPLE: Mandatory nested
 * attribute containing attributes defined by
 * enum qca_wlan_vendor_attr_flow_tuple.
 *
 * @QCA_WLAN_VENDOR_ATTR_FLOW_CLASSIFY_RESULT_TRAFFIC_TYPE: Mandatory u8
 * attribute indicates the traffic type learned for this flow tuple. Uses the
 * enum qca_traffic_type values.
 */
enum qca_wlan_vendor_attr_flow_classify_result {
	QCA_WLAN_VENDOR_ATTR_FLOW_CLASSIFY_RESULT_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_FLOW_CLASSIFY_RESULT_FLOW_TUPLE = 1,
	QCA_WLAN_VENDOR_ATTR_FLOW_CLASSIFY_RESULT_TRAFFIC_TYPE = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_FLOW_CLASSIFY_RESULT_LAST,
	QCA_WLAN_VENDOR_ATTR_FLOW_CLASSIFY_RESULT_MAX =
	QCA_WLAN_VENDOR_ATTR_FLOW_CLASSIFY_RESULT_LAST - 1,
};

/**
 * enum qca_async_stats_sub_module - The statistics type used in async
 * statistics policy.
 * Used by @QCA_WLAN_VENDOR_ATTR_ASYNC_STATS_POLICY_STATS_TYPE.
 *
 * @QCA_ASYNC_STATS_TYPE_POWERSAVE: Wi-Fi powersave statistics
 *
 * @QCA_ASYNC_STATS_TYPE_FLOW_STATS: Flow statistics
 *
 * @QCA_ASYNC_STATS_TYPE_CLASSIFIED_FLOW_STATS: Classified flow statistics
 */
enum qca_async_stats_type {
	QCA_ASYNC_STATS_TYPE_POWERSAVE = 0,
	QCA_ASYNC_STATS_TYPE_FLOW_STATS = 1,
	QCA_ASYNC_STATS_TYPE_CLASSIFIED_FLOW_STATS = 2,
};

/**
 * enum qca_async_stats_action - ASYNC statistics action. Used by
 * @QCA_WLAN_VENDOR_ATTR_ASYNC_STATS_POLICY_ACTION.
 *
 * @QCA_ASYNC_STATS_ACTION_START: Start indication for async statistics
 * collection.
 * @QCA_ASYNC_STATS_ACTION_STOP: Stop indication for async statistics
 * collection.
 */
enum qca_async_stats_action {
	QCA_ASYNC_STATS_ACTION_START = 0,
	QCA_ASYNC_STATS_ACTION_STOP = 1,
};

/**
 * enum qca_wlan_vendor_attr_async_stats_policy - Definition of attributes to
 * specify the ASYNC statistics policy. This enum is used by
 * @QCA_NL80211_VENDOR_SUBCMD_ASYNC_STATS_POLICY.
 *
 * @QCA_WLAN_VENDOR_ATTR_ASYNC_STATS_POLICY_STATS_TYPE: Mandatory u8
 * attribute indicates the statistics type for which the async statistics policy
 * needs to be applied by the driver. Uses the enum qca_async_stats_type values.
 *
 * @QCA_WLAN_VENDOR_ATTR_ASYNC_STATS_POLICY_ACTION: Mandatory u8 attribute
 * indicates the action as part of this policy.
 * Uses the enum qca_async_stats_action values.
 *
 * @QCA_WLAN_VENDOR_ATTR_ASYNC_STATS_POLICY_STATS_PERIODICITY: Optional u32
 * attribute indicates the periodicity (in milliseconds) for the statistics to
 * be reported. This attribute is mandatory for QCA_ASYNC_STATS_TYPE_POWERSAVE.
 */
enum qca_wlan_vendor_attr_async_stats_policy {
	QCA_WLAN_VENDOR_ATTR_ASYNC_STATS_POLICY_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ASYNC_STATS_POLICY_STATS_TYPE = 1,
	QCA_WLAN_VENDOR_ATTR_ASYNC_STATS_POLICY_ACTION = 2,
	QCA_WLAN_VENDOR_ATTR_ASYNC_STATS_POLICY_STATS_PERIODICITY = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ASYNC_STATS_POLICY_LAST,
	QCA_WLAN_VENDOR_ATTR_ASYNC_STATS_POLICY_MAX =
	QCA_WLAN_VENDOR_ATTR_ASYNC_STATS_POLICY_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_nss_pkt - Attributes used by
 * %QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_NSS_PKT_COUNT.
 *
 * @QCA_WLAN_VENDOR_ATTR_NSS_PKT_NSS_VALUE: u8 attribute. This
 * represents the number of spatial streams.
 *
 * @QCA_WLAN_VENDOR_ATTR_NSS_PKT_TX_PACKET_COUNT: u64 attribute. This
 * represents the number of MSDU packets transmitted with the number of spatial
 * streams specified in %QCA_WLAN_VENDOR_ATTR_NSS_PKT_NSS_VALUE.
 *
 * @QCA_WLAN_VENDOR_ATTR_NSS_PKT_RX_PACKET_COUNT: u64 attribute. This
 * represents the number of MSDU packets received with the number of spatial
 * streams specified in %QCA_WLAN_VENDOR_ATTR_NSS_PKT_NSS_VALUE.
 */
enum qca_wlan_vendor_attr_nss_pkt {
	QCA_WLAN_VENDOR_ATTR_NSS_PKT_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_NSS_PKT_NSS_VALUE = 1,
	QCA_WLAN_VENDOR_ATTR_NSS_PKT_TX_PACKET_COUNT = 2,
	QCA_WLAN_VENDOR_ATTR_NSS_PKT_RX_PACKET_COUNT = 3,

	QCA_WLAN_VENDOR_ATTR_NSS_PKT_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_NSS_PKT_MAX =
	QCA_WLAN_VENDOR_ATTR_NSS_PKT_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_intf_offload_type - Definition of available values for
 * QCA_WLAN_VENDOR_ATTR_CONFIG_INTF_OFFLOAD_TYPE to specify the offload path for
 * packets handled through a network device.
 *
 * There are three offload paths possible for handling packet forwarding between
 * Ethernet and Wi-Fi network, and which path to use can be configured on a per
 * netdevice level based on use case. Userspace can choose different options
 * based on use cases like performance requirements, traffic control features
 * and limitations provided in each option.
 *
 * @QCA_WLAN_VENDOR_ATTR_INTF_OFFLOAD_TYPE_NONE: No acceleration configured.
 * Packets are processed through the Linux kernel networking stack.
 *
 * @QCA_WLAN_VENDOR_ATTR_INTF_OFFLOAD_TYPE_SFE: Packets are processed through
 * the shortcut forwarding engine (SFE) to bypass the Linux networking stack
 * for improved throughput performance. This option is applicable for AP, STA,
 * and Mesh mode and available for all radio designs. From the performance
 * aspect, this option consumes more CPU compared to the other two options.
 * Linux traffic control can be further applied with this option to have more
 * control on the traffic flows.
 *
 * @QCA_WLAN_VENDOR_ATTR_INTF_OFFLOAD_TYPE_ACTIVE_VP: Packets are processed
 * through both hardware and software in this case. Packet classification is
 * done by the hardware and then the packets are delivered to software along
 * with classification results as meta data. Software can choose to do more
 * classification/QoS based on use cases. This is applicable for AP, STA, and
 * Mesh modes and this is available for all radio designs. From the performance
 * aspect, this option consumes relatively less CPU compared to the SFE option
 * above. Linux traffic control rules cannot be applied with this option.
 *
 * @QCA_WLAN_VENDOR_ATTR_INTF_OFFLOAD_TYPE_PPE_DS: Packets are processed through
 * special hardware (Direct Switch) rings which can directly forward the packets
 * between Ethernet hardware and Wi-Fi hardware with very little software
 * involvement. This is applicable only for AP and STA modes; not applicable
 * for Mesh mode. From the performance aspect, this option consumes very much
 * less CPU compared to the other options. Linux traffic control rules cannot be
 * applied when this option is used. This option is applicable only for
 * specific radio designs. When this option is not available, the default option
 * (SFE) would be configured.
 */
enum qca_wlan_intf_offload_type {
	QCA_WLAN_INTF_OFFLOAD_TYPE_NONE = 0,
	QCA_WLAN_INTF_OFFLOAD_TYPE_SFE = 1,
	QCA_WLAN_INTF_OFFLOAD_TYPE_ACTIVE_VP = 2,
	QCA_WLAN_INTF_OFFLOAD_TYPE_PPE_DS = 3,
};

#endif /* QCA_VENDOR_H */
