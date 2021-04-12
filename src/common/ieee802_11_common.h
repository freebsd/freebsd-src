/*
 * IEEE 802.11 Common routines
 * Copyright (c) 2002-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef IEEE802_11_COMMON_H
#define IEEE802_11_COMMON_H

#include "defs.h"
#include "ieee802_11_defs.h"

struct element {
	u8 id;
	u8 datalen;
	u8 data[];
} STRUCT_PACKED;

struct hostapd_hw_modes;

#define MAX_NOF_MB_IES_SUPPORTED 5
#define MAX_NUM_FRAG_IES_SUPPORTED 3

struct mb_ies_info {
	struct {
		const u8 *ie;
		u8 ie_len;
	} ies[MAX_NOF_MB_IES_SUPPORTED];
	u8 nof_ies;
};

struct frag_ies_info {
	struct {
		u8 eid;
		u8 eid_ext;
		const u8 *ie;
		u8 ie_len;
	} frags[MAX_NUM_FRAG_IES_SUPPORTED];

	u8 n_frags;

	/* the last parsed element ID and element extension ID */
	u8 last_eid;
	u8 last_eid_ext;
};

/* Parsed Information Elements */
struct ieee802_11_elems {
	const u8 *ssid;
	const u8 *supp_rates;
	const u8 *ds_params;
	const u8 *challenge;
	const u8 *erp_info;
	const u8 *ext_supp_rates;
	const u8 *wpa_ie;
	const u8 *rsn_ie;
	const u8 *rsnxe;
	const u8 *wmm; /* WMM Information or Parameter Element */
	const u8 *wmm_tspec;
	const u8 *wps_ie;
	const u8 *supp_channels;
	const u8 *mdie;
	const u8 *ftie;
	const u8 *timeout_int;
	const u8 *ht_capabilities;
	const u8 *ht_operation;
	const u8 *mesh_config;
	const u8 *mesh_id;
	const u8 *peer_mgmt;
	const u8 *vht_capabilities;
	const u8 *vht_operation;
	const u8 *vht_opmode_notif;
	const u8 *vendor_ht_cap;
	const u8 *vendor_vht;
	const u8 *p2p;
	const u8 *wfd;
	const u8 *link_id;
	const u8 *interworking;
	const u8 *qos_map_set;
	const u8 *hs20;
	const u8 *ext_capab;
	const u8 *bss_max_idle_period;
	const u8 *ssid_list;
	const u8 *osen;
	const u8 *mbo;
	const u8 *ampe;
	const u8 *mic;
	const u8 *pref_freq_list;
	const u8 *supp_op_classes;
	const u8 *rrm_enabled;
	const u8 *cag_number;
	const u8 *ap_csn;
	const u8 *fils_indic;
	const u8 *dils;
	const u8 *assoc_delay_info;
	const u8 *fils_req_params;
	const u8 *fils_key_confirm;
	const u8 *fils_session;
	const u8 *fils_hlp;
	const u8 *fils_ip_addr_assign;
	const u8 *key_delivery;
	const u8 *wrapped_data;
	const u8 *fils_pk;
	const u8 *fils_nonce;
	const u8 *owe_dh;
	const u8 *power_capab;
	const u8 *roaming_cons_sel;
	const u8 *password_id;
	const u8 *oci;
	const u8 *multi_ap;
	const u8 *he_capabilities;
	const u8 *he_operation;
	const u8 *short_ssid_list;
	const u8 *he_6ghz_band_cap;
	const u8 *sae_pk;
	const u8 *s1g_capab;
	const u8 *pasn_params;

	u8 ssid_len;
	u8 supp_rates_len;
	u8 challenge_len;
	u8 ext_supp_rates_len;
	u8 wpa_ie_len;
	u8 rsn_ie_len;
	u8 rsnxe_len;
	u8 wmm_len; /* 7 = WMM Information; 24 = WMM Parameter */
	u8 wmm_tspec_len;
	u8 wps_ie_len;
	u8 supp_channels_len;
	u8 mdie_len;
	u8 ftie_len;
	u8 mesh_config_len;
	u8 mesh_id_len;
	u8 peer_mgmt_len;
	u8 vendor_ht_cap_len;
	u8 vendor_vht_len;
	u8 p2p_len;
	u8 wfd_len;
	u8 interworking_len;
	u8 qos_map_set_len;
	u8 hs20_len;
	u8 ext_capab_len;
	u8 ssid_list_len;
	u8 osen_len;
	u8 mbo_len;
	u8 ampe_len;
	u8 mic_len;
	u8 pref_freq_list_len;
	u8 supp_op_classes_len;
	u8 rrm_enabled_len;
	u8 cag_number_len;
	u8 fils_indic_len;
	u8 dils_len;
	u8 fils_req_params_len;
	u8 fils_key_confirm_len;
	u8 fils_hlp_len;
	u8 fils_ip_addr_assign_len;
	u8 key_delivery_len;
	u8 wrapped_data_len;
	u8 fils_pk_len;
	u8 owe_dh_len;
	u8 power_capab_len;
	u8 roaming_cons_sel_len;
	u8 password_id_len;
	u8 oci_len;
	u8 multi_ap_len;
	u8 he_capabilities_len;
	u8 he_operation_len;
	u8 short_ssid_list_len;
	u8 sae_pk_len;
	u8 pasn_params_len;

	struct mb_ies_info mb_ies;
	struct frag_ies_info frag_ies;
};

typedef enum { ParseOK = 0, ParseUnknown = 1, ParseFailed = -1 } ParseRes;

ParseRes ieee802_11_parse_elems(const u8 *start, size_t len,
				struct ieee802_11_elems *elems,
				int show_errors);
int ieee802_11_ie_count(const u8 *ies, size_t ies_len);
struct wpabuf * ieee802_11_vendor_ie_concat(const u8 *ies, size_t ies_len,
					    u32 oui_type);
struct ieee80211_hdr;
const u8 * get_hdr_bssid(const struct ieee80211_hdr *hdr, size_t len);

struct hostapd_wmm_ac_params {
	int cwmin;
	int cwmax;
	int aifs;
	int txop_limit; /* in units of 32us */
	int admission_control_mandatory;
};

int hostapd_config_wmm_ac(struct hostapd_wmm_ac_params wmm_ac_params[],
			  const char *name, const char *val);

struct hostapd_tx_queue_params {
	int aifs;
	int cwmin;
	int cwmax;
	int burst; /* maximum burst time in 0.1 ms, i.e., 10 = 1 ms */
};

#define NUM_TX_QUEUES 4

int hostapd_config_tx_queue(struct hostapd_tx_queue_params queue[],
			    const char *name, const char *val);
enum hostapd_hw_mode ieee80211_freq_to_chan(int freq, u8 *channel);
int ieee80211_chan_to_freq(const char *country, u8 op_class, u8 chan);
enum hostapd_hw_mode ieee80211_freq_to_channel_ext(unsigned int freq,
						   int sec_channel, int vht,
						   u8 *op_class, u8 *channel);
int ieee80211_chaninfo_to_channel(unsigned int freq, enum chan_width chanwidth,
				  int sec_channel, u8 *op_class, u8 *channel);
int ieee80211_is_dfs(int freq, const struct hostapd_hw_modes *modes,
		     u16 num_modes);
enum phy_type ieee80211_get_phy_type(int freq, int ht, int vht);

int supp_rates_11b_only(struct ieee802_11_elems *elems);
int mb_ies_info_by_ies(struct mb_ies_info *info, const u8 *ies_buf,
		       size_t ies_len);
struct wpabuf * mb_ies_by_info(struct mb_ies_info *info);

const char * fc2str(u16 fc);
const char * reason2str(u16 reason);
const char * status2str(u16 status);

struct oper_class_map {
	enum hostapd_hw_mode mode;
	u8 op_class;
	u8 min_chan;
	u8 max_chan;
	u8 inc;
	enum { BW20, BW40PLUS, BW40MINUS, BW40, BW80, BW2160, BW160, BW80P80,
	       BW4320, BW6480, BW8640} bw;
	enum { P2P_SUPP, NO_P2P_SUPP } p2p;
};

extern const struct oper_class_map global_op_class[];
extern size_t global_op_class_size;

const u8 * get_ie(const u8 *ies, size_t len, u8 eid);
const u8 * get_ie_ext(const u8 *ies, size_t len, u8 ext);
const u8 * get_vendor_ie(const u8 *ies, size_t len, u32 vendor_type);

size_t mbo_add_ie(u8 *buf, size_t len, const u8 *attr, size_t attr_len);

size_t add_multi_ap_ie(u8 *buf, size_t len, u8 value);

struct country_op_class {
	u8 country_op_class;
	u8 global_op_class;
};

u8 country_to_global_op_class(const char *country, u8 op_class);

const struct oper_class_map * get_oper_class(const char *country, u8 op_class);
int oper_class_bw_to_int(const struct oper_class_map *map);
int center_idx_to_bw_6ghz(u8 idx);
bool is_6ghz_freq(int freq);
bool is_6ghz_op_class(u8 op_class);
bool is_6ghz_psc_frequency(int freq);

int ieee802_11_parse_candidate_list(const char *pos, u8 *nei_rep,
				    size_t nei_rep_len);

int ieee802_11_ext_capab(const u8 *ie, unsigned int capab);
bool ieee802_11_rsnx_capab_len(const u8 *rsnxe, size_t rsnxe_len,
			       unsigned int capab);
bool ieee802_11_rsnx_capab(const u8 *rsnxe, unsigned int capab);
int op_class_to_bandwidth(u8 op_class);
int op_class_to_ch_width(u8 op_class);

/* element iteration helpers */
#define for_each_element(_elem, _data, _datalen)			\
	for (_elem = (const struct element *) (_data);			\
	     (const u8 *) (_data) + (_datalen) - (const u8 *) _elem >=	\
		(int) sizeof(*_elem) &&					\
	     (const u8 *) (_data) + (_datalen) - (const u8 *) _elem >=	\
		(int) sizeof(*_elem) + _elem->datalen;			\
	     _elem = (const struct element *) (_elem->data + _elem->datalen))

#define for_each_element_id(element, _id, data, datalen)		\
	for_each_element(element, data, datalen)			\
		if (element->id == (_id))

#define for_each_element_extid(element, extid, _data, _datalen)		\
	for_each_element(element, _data, _datalen)			\
		if (element->id == WLAN_EID_EXTENSION &&		\
		    element->datalen > 0 &&				\
		    element->data[0] == (extid))

#define for_each_subelement(sub, element)				\
	for_each_element(sub, (element)->data, (element)->datalen)

#define for_each_subelement_id(sub, id, element)			\
	for_each_element_id(sub, id, (element)->data, (element)->datalen)

#define for_each_subelement_extid(sub, extid, element)			\
	for_each_element_extid(sub, extid, (element)->data, (element)->datalen)

/**
 * for_each_element_completed - Determine if element parsing consumed all data
 * @element: Element pointer after for_each_element() or friends
 * @data: Same data pointer as passed to for_each_element() or friends
 * @datalen: Same data length as passed to for_each_element() or friends
 *
 * This function returns 1 if all the data was parsed or considered
 * while walking the elements. Only use this if your for_each_element()
 * loop cannot be broken out of, otherwise it always returns 0.
 *
 * If some data was malformed, this returns %false since the last parsed
 * element will not fill the whole remaining data.
 */
static inline int for_each_element_completed(const struct element *element,
					     const void *data, size_t datalen)
{
	return (const u8 *) element == (const u8 *) data + datalen;
}

struct ieee80211_edmg_config;

void hostapd_encode_edmg_chan(int edmg_enable, u8 edmg_channel,
			      int primary_channel,
			      struct ieee80211_edmg_config *edmg);

int ieee802_edmg_is_allowed(struct ieee80211_edmg_config allowed,
			    struct ieee80211_edmg_config requested);

struct wpabuf * ieee802_11_defrag_data(struct ieee802_11_elems *elems,
				       u8 eid, u8 eid_ext,
				       const u8 *data, u8 len);
struct wpabuf * ieee802_11_defrag(struct ieee802_11_elems *elems,
				  u8 eid, u8 eid_ext);

#endif /* IEEE802_11_COMMON_H */
