/*
 * Dummy functions to allow link_test to be linked. The need for these
 * functions should be removed to allow IEEE 802.1X/EAPOL authenticator to
 * be built outside hostapd.
 */

#include "includes.h"

#include "common.h"


struct hostapd_data;
struct sta_info;
struct rsn_pmksa_cache_entry;
struct eapol_state_machine;
struct hostapd_eap_user;
struct hostapd_bss_config;
struct hostapd_vlan;


struct sta_info * ap_get_sta(struct hostapd_data *hapd, const u8 *sta)
{
	return NULL;
}


int ap_for_each_sta(struct hostapd_data *hapd,
		    int (*cb)(struct hostapd_data *hapd, struct sta_info *sta,
			      void *ctx),
		    void *ctx)
{
	return 0;
}


void ap_sta_session_timeout(struct hostapd_data *hapd, struct sta_info *sta,
			    u32 session_timeout)
{
}


int ap_sta_bind_vlan(struct hostapd_data *hapd, struct sta_info *sta,
		     int old_vlanid)
{
	return 0;
}


void rsn_preauth_finished(struct hostapd_data *hapd, struct sta_info *sta,
			  int success)
{
}


void rsn_preauth_send(struct hostapd_data *hapd, struct sta_info *sta,
		      u8 *buf, size_t len)
{
}


void accounting_sta_start(struct hostapd_data *hapd, struct sta_info *sta)
{
}


void pmksa_cache_to_eapol_data(struct rsn_pmksa_cache_entry *entry,
			       struct eapol_state_machine *eapol)
{
}


const struct hostapd_eap_user *
hostapd_get_eap_user(const struct hostapd_bss_config *conf, const u8 *identity,
		     size_t identity_len, int phase2)
{
	return NULL;
}


const char * hostapd_get_vlan_id_ifname(struct hostapd_vlan *vlan, int vlan_id)
{
	return NULL;
}
