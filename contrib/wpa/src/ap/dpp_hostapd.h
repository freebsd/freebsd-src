/*
 * hostapd / DPP integration
 * Copyright (c) 2017, Qualcomm Atheros, Inc.
 * Copyright (c) 2018-2020, The Linux Foundation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef DPP_HOSTAPD_H
#define DPP_HOSTAPD_H

struct dpp_bootstrap_info;

int hostapd_dpp_qr_code(struct hostapd_data *hapd, const char *cmd);
int hostapd_dpp_nfc_uri(struct hostapd_data *hapd, const char *cmd);
int hostapd_dpp_nfc_handover_req(struct hostapd_data *hapd, const char *cmd);
int hostapd_dpp_nfc_handover_sel(struct hostapd_data *hapd, const char *cmd);
int hostapd_dpp_auth_init(struct hostapd_data *hapd, const char *cmd);
int hostapd_dpp_listen(struct hostapd_data *hapd, const char *cmd);
void hostapd_dpp_listen_stop(struct hostapd_data *hapd);
void hostapd_dpp_rx_action(struct hostapd_data *hapd, const u8 *src,
			   const u8 *buf, size_t len, unsigned int freq);
void hostapd_dpp_tx_status(struct hostapd_data *hapd, const u8 *dst,
			   const u8 *data, size_t data_len, int ok);
struct wpabuf *
hostapd_dpp_gas_req_handler(struct hostapd_data *hapd, const u8 *sa,
			    const u8 *query, size_t query_len,
			    const u8 *data, size_t data_len);
void hostapd_dpp_gas_status_handler(struct hostapd_data *hapd, int ok);
int hostapd_dpp_configurator_add(struct hostapd_data *hapd, const char *cmd);
int hostapd_dpp_configurator_remove(struct hostapd_data *hapd, const char *id);
int hostapd_dpp_configurator_sign(struct hostapd_data *hapd, const char *cmd);
int hostapd_dpp_configurator_get_key(struct hostapd_data *hapd, unsigned int id,
				     char *buf, size_t buflen);
int hostapd_dpp_pkex_add(struct hostapd_data *hapd, const char *cmd);
int hostapd_dpp_pkex_remove(struct hostapd_data *hapd, const char *id);
void hostapd_dpp_stop(struct hostapd_data *hapd);
int hostapd_dpp_init(struct hostapd_data *hapd);
void hostapd_dpp_deinit(struct hostapd_data *hapd);
void hostapd_dpp_init_global(struct hapd_interfaces *ifaces);
void hostapd_dpp_deinit_global(struct hapd_interfaces *ifaces);

int hostapd_dpp_controller_start(struct hostapd_data *hapd, const char *cmd);
int hostapd_dpp_chirp(struct hostapd_data *hapd, const char *cmd);
void hostapd_dpp_chirp_stop(struct hostapd_data *hapd);
void hostapd_dpp_remove_bi(void *ctx, struct dpp_bootstrap_info *bi);
int hostapd_dpp_push_button(struct hostapd_data *hapd, const char *cmd);
void hostapd_dpp_push_button_stop(struct hostapd_data *hapd);
bool hostapd_dpp_configurator_connectivity(struct hostapd_data *hapd);
int hostapd_dpp_add_controller(struct hostapd_data *hapd, const char *cmd);
void hostapd_dpp_remove_controller(struct hostapd_data *hapd, const char *cmd);

#endif /* DPP_HOSTAPD_H */
