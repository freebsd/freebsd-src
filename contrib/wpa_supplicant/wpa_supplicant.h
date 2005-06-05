#ifndef WPA_SUPPLICANT_H
#define WPA_SUPPLICANT_H

/* Driver wrappers are not supposed to directly touch the internal data
 * structure used in wpa_supplicant, so that definition is not provided here.
 */
struct wpa_supplicant;

typedef enum {
	EVENT_ASSOC, EVENT_DISASSOC, EVENT_MICHAEL_MIC_FAILURE,
	EVENT_SCAN_RESULTS, EVENT_ASSOCINFO, EVENT_INTERFACE_STATUS,
	EVENT_PMKID_CANDIDATE
} wpa_event_type;

union wpa_event_data {
	struct {
		/* Optional request information data: IEs included in AssocReq
		 * and AssocResp. If these are not returned by the driver,
		 * WPA Supplicant will generate the WPA/RSN IE. */
		u8 *req_ies, *resp_ies;
		size_t req_ies_len, resp_ies_len;

		/* Optional Beacon/ProbeResp data: IEs included in Beacon or
		 * Probe Response frames from the current AP (i.e., the one
		 * that the client just associated with). This information is
		 * used to update WPA/RSN IE for the AP. If this field is not
		 * set, the results from previous scan will be used. If no
		 * data for the new AP is found, scan results will be requested
		 * again (without scan request). At this point, the driver is
		 * expected to provide WPA/RSN IE for the AP (if WPA/WPA2 is
		 * used). */
		u8 *beacon_ies; /* beacon or probe resp IEs */
		size_t beacon_ies_len;
	} assoc_info;
	struct {
		int unicast;
	} michael_mic_failure;
	struct {
		char ifname[20];
		enum {
			EVENT_INTERFACE_ADDED, EVENT_INTERFACE_REMOVED
		} ievent;
	} interface_status;
	struct {
		u8 bssid[ETH_ALEN];
		int index; /* smaller the index, higher the priority */
		int preauth;
	} pmkid_candidate;
};

/**
 * wpa_supplicant_event - report a driver event for wpa_supplicant
 * @wpa_s: pointer to wpa_supplicant data; this is the @ctx variable registered
 *	with wpa_driver_events_init()
 * @event: event type (defined above)
 * @data: possible extra data for the event
 *
 * Driver wrapper code should call this function whenever an event is received
 * from the driver.
 */
void wpa_supplicant_event(struct wpa_supplicant *wpa_s, wpa_event_type event,
			  union wpa_event_data *data);

/**
 * wpa_msg - conditional printf for default target and ctrl_iface monitors
 * @level: priority level (MSG_*) of the message
 * @fmt: printf format string, followed by optional arguments
 *
 * This function is used to print conditional debugging and error messages. The
 * output may be directed to stdout, stderr, and/or syslog based on
 * configuration. This function is like wpa_printf(), but it also sends the
 * same message to all attached ctrl_iface monitors.
 *
 * Note: New line '\n' is added to the end of the text when printing to stdout.
 */
void wpa_msg(struct wpa_supplicant *wpa_s, int level, char *fmt, ...)
__attribute__ ((format (printf, 3, 4)));

const char * wpa_ssid_txt(u8 *ssid, size_t ssid_len);

#endif /* WPA_SUPPLICANT_H */
