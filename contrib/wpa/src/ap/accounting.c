/*
 * hostapd / RADIUS Accounting
 * Copyright (c) 2002-2009, 2012, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "eapol_auth/eapol_auth_sm.h"
#include "eapol_auth/eapol_auth_sm_i.h"
#include "radius/radius.h"
#include "radius/radius_client.h"
#include "hostapd.h"
#include "ieee802_1x.h"
#include "ap_config.h"
#include "sta_info.h"
#include "ap_drv_ops.h"
#include "accounting.h"


/* Default interval in seconds for polling TX/RX octets from the driver if
 * STA is not using interim accounting. This detects wrap arounds for
 * input/output octets and updates Acct-{Input,Output}-Gigawords. */
#define ACCT_DEFAULT_UPDATE_INTERVAL 300

static void accounting_sta_interim(struct hostapd_data *hapd,
				   struct sta_info *sta);


static struct radius_msg * accounting_msg(struct hostapd_data *hapd,
					  struct sta_info *sta,
					  int status_type)
{
	struct radius_msg *msg;
	char buf[128];
	u8 *val;
	size_t len;
	int i;
	struct wpabuf *b;

	msg = radius_msg_new(RADIUS_CODE_ACCOUNTING_REQUEST,
			     radius_client_get_id(hapd->radius));
	if (msg == NULL) {
		wpa_printf(MSG_INFO, "Could not create new RADIUS packet");
		return NULL;
	}

	if (sta) {
		radius_msg_make_authenticator(msg, (u8 *) sta, sizeof(*sta));

		if ((hapd->conf->wpa & 2) &&
		    !hapd->conf->disable_pmksa_caching &&
		    sta->eapol_sm && sta->eapol_sm->acct_multi_session_id_hi) {
			os_snprintf(buf, sizeof(buf), "%08X+%08X",
				    sta->eapol_sm->acct_multi_session_id_hi,
				    sta->eapol_sm->acct_multi_session_id_lo);
			if (!radius_msg_add_attr(
				    msg, RADIUS_ATTR_ACCT_MULTI_SESSION_ID,
				    (u8 *) buf, os_strlen(buf))) {
				wpa_printf(MSG_INFO,
					   "Could not add Acct-Multi-Session-Id");
				goto fail;
			}
		}
	} else {
		radius_msg_make_authenticator(msg, (u8 *) hapd, sizeof(*hapd));
	}

	if (!radius_msg_add_attr_int32(msg, RADIUS_ATTR_ACCT_STATUS_TYPE,
				       status_type)) {
		wpa_printf(MSG_INFO, "Could not add Acct-Status-Type");
		goto fail;
	}

	if (!hostapd_config_get_radius_attr(hapd->conf->radius_acct_req_attr,
					    RADIUS_ATTR_ACCT_AUTHENTIC) &&
	    !radius_msg_add_attr_int32(msg, RADIUS_ATTR_ACCT_AUTHENTIC,
				       hapd->conf->ieee802_1x ?
				       RADIUS_ACCT_AUTHENTIC_RADIUS :
				       RADIUS_ACCT_AUTHENTIC_LOCAL)) {
		wpa_printf(MSG_INFO, "Could not add Acct-Authentic");
		goto fail;
	}

	if (sta) {
		/* Use 802.1X identity if available */
		val = ieee802_1x_get_identity(sta->eapol_sm, &len);

		/* Use RADIUS ACL identity if 802.1X provides no identity */
		if (!val && sta->identity) {
			val = (u8 *) sta->identity;
			len = os_strlen(sta->identity);
		}

		/* Use STA MAC if neither 802.1X nor RADIUS ACL provided
		 * identity */
		if (!val) {
			os_snprintf(buf, sizeof(buf), RADIUS_ADDR_FORMAT,
				    MAC2STR(sta->addr));
			val = (u8 *) buf;
			len = os_strlen(buf);
		}

		if (!radius_msg_add_attr(msg, RADIUS_ATTR_USER_NAME, val,
					 len)) {
			wpa_printf(MSG_INFO, "Could not add User-Name");
			goto fail;
		}
	}

	if (add_common_radius_attr(hapd, hapd->conf->radius_acct_req_attr, sta,
				   msg) < 0)
		goto fail;

	if (sta) {
		for (i = 0; ; i++) {
			val = ieee802_1x_get_radius_class(sta->eapol_sm, &len,
							  i);
			if (val == NULL)
				break;

			if (!radius_msg_add_attr(msg, RADIUS_ATTR_CLASS,
						 val, len)) {
				wpa_printf(MSG_INFO, "Could not add Class");
				goto fail;
			}
		}

		b = ieee802_1x_get_radius_cui(sta->eapol_sm);
		if (b &&
		    !radius_msg_add_attr(msg,
					 RADIUS_ATTR_CHARGEABLE_USER_IDENTITY,
					 wpabuf_head(b), wpabuf_len(b))) {
			wpa_printf(MSG_ERROR, "Could not add CUI");
			goto fail;
		}

		if (!b && sta->radius_cui &&
		    !radius_msg_add_attr(msg,
					 RADIUS_ATTR_CHARGEABLE_USER_IDENTITY,
					 (u8 *) sta->radius_cui,
					 os_strlen(sta->radius_cui))) {
			wpa_printf(MSG_ERROR, "Could not add CUI from ACL");
			goto fail;
		}
	}

	return msg;

 fail:
	radius_msg_free(msg);
	return NULL;
}


static int accounting_sta_update_stats(struct hostapd_data *hapd,
				       struct sta_info *sta,
				       struct hostap_sta_driver_data *data)
{
	if (hostapd_drv_read_sta_data(hapd, data, sta->addr))
		return -1;

	if (sta->last_rx_bytes > data->rx_bytes)
		sta->acct_input_gigawords++;
	if (sta->last_tx_bytes > data->tx_bytes)
		sta->acct_output_gigawords++;
	sta->last_rx_bytes = data->rx_bytes;
	sta->last_tx_bytes = data->tx_bytes;

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_RADIUS,
		       HOSTAPD_LEVEL_DEBUG, "updated TX/RX stats: "
		       "Acct-Input-Octets=%lu Acct-Input-Gigawords=%u "
		       "Acct-Output-Octets=%lu Acct-Output-Gigawords=%u",
		       sta->last_rx_bytes, sta->acct_input_gigawords,
		       sta->last_tx_bytes, sta->acct_output_gigawords);

	return 0;
}


static void accounting_interim_update(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = timeout_ctx;
	int interval;

	if (sta->acct_interim_interval) {
		accounting_sta_interim(hapd, sta);
		interval = sta->acct_interim_interval;
	} else {
		struct hostap_sta_driver_data data;
		accounting_sta_update_stats(hapd, sta, &data);
		interval = ACCT_DEFAULT_UPDATE_INTERVAL;
	}

	eloop_register_timeout(interval, 0, accounting_interim_update,
			       hapd, sta);
}


/**
 * accounting_sta_start - Start STA accounting
 * @hapd: hostapd BSS data
 * @sta: The station
 */
void accounting_sta_start(struct hostapd_data *hapd, struct sta_info *sta)
{
	struct radius_msg *msg;
	int interval;

	if (sta->acct_session_started)
		return;

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_RADIUS,
		       HOSTAPD_LEVEL_INFO,
		       "starting accounting session %08X-%08X",
		       sta->acct_session_id_hi, sta->acct_session_id_lo);

	os_get_reltime(&sta->acct_session_start);
	sta->last_rx_bytes = sta->last_tx_bytes = 0;
	sta->acct_input_gigawords = sta->acct_output_gigawords = 0;
	hostapd_drv_sta_clear_stats(hapd, sta->addr);

	if (!hapd->conf->radius->acct_server)
		return;

	if (sta->acct_interim_interval)
		interval = sta->acct_interim_interval;
	else
		interval = ACCT_DEFAULT_UPDATE_INTERVAL;
	eloop_register_timeout(interval, 0, accounting_interim_update,
			       hapd, sta);

	msg = accounting_msg(hapd, sta, RADIUS_ACCT_STATUS_TYPE_START);
	if (msg &&
	    radius_client_send(hapd->radius, msg, RADIUS_ACCT, sta->addr) < 0)
		radius_msg_free(msg);

	sta->acct_session_started = 1;
}


static void accounting_sta_report(struct hostapd_data *hapd,
				  struct sta_info *sta, int stop)
{
	struct radius_msg *msg;
	int cause = sta->acct_terminate_cause;
	struct hostap_sta_driver_data data;
	struct os_reltime now_r, diff;
	struct os_time now;
	u32 gigawords;

	if (!hapd->conf->radius->acct_server)
		return;

	msg = accounting_msg(hapd, sta,
			     stop ? RADIUS_ACCT_STATUS_TYPE_STOP :
			     RADIUS_ACCT_STATUS_TYPE_INTERIM_UPDATE);
	if (!msg) {
		wpa_printf(MSG_INFO, "Could not create RADIUS Accounting message");
		return;
	}

	os_get_reltime(&now_r);
	os_get_time(&now);
	os_reltime_sub(&now_r, &sta->acct_session_start, &diff);
	if (!radius_msg_add_attr_int32(msg, RADIUS_ATTR_ACCT_SESSION_TIME,
				       diff.sec)) {
		wpa_printf(MSG_INFO, "Could not add Acct-Session-Time");
		goto fail;
	}

	if (accounting_sta_update_stats(hapd, sta, &data) == 0) {
		if (!radius_msg_add_attr_int32(msg,
					       RADIUS_ATTR_ACCT_INPUT_PACKETS,
					       data.rx_packets)) {
			wpa_printf(MSG_INFO, "Could not add Acct-Input-Packets");
			goto fail;
		}
		if (!radius_msg_add_attr_int32(msg,
					       RADIUS_ATTR_ACCT_OUTPUT_PACKETS,
					       data.tx_packets)) {
			wpa_printf(MSG_INFO, "Could not add Acct-Output-Packets");
			goto fail;
		}
		if (!radius_msg_add_attr_int32(msg,
					       RADIUS_ATTR_ACCT_INPUT_OCTETS,
					       data.rx_bytes)) {
			wpa_printf(MSG_INFO, "Could not add Acct-Input-Octets");
			goto fail;
		}
		gigawords = sta->acct_input_gigawords;
#if __WORDSIZE == 64
		gigawords += data.rx_bytes >> 32;
#endif
		if (gigawords &&
		    !radius_msg_add_attr_int32(
			    msg, RADIUS_ATTR_ACCT_INPUT_GIGAWORDS,
			    gigawords)) {
			wpa_printf(MSG_INFO, "Could not add Acct-Input-Gigawords");
			goto fail;
		}
		if (!radius_msg_add_attr_int32(msg,
					       RADIUS_ATTR_ACCT_OUTPUT_OCTETS,
					       data.tx_bytes)) {
			wpa_printf(MSG_INFO, "Could not add Acct-Output-Octets");
			goto fail;
		}
		gigawords = sta->acct_output_gigawords;
#if __WORDSIZE == 64
		gigawords += data.tx_bytes >> 32;
#endif
		if (gigawords &&
		    !radius_msg_add_attr_int32(
			    msg, RADIUS_ATTR_ACCT_OUTPUT_GIGAWORDS,
			    gigawords)) {
			wpa_printf(MSG_INFO, "Could not add Acct-Output-Gigawords");
			goto fail;
		}
	}

	if (!radius_msg_add_attr_int32(msg, RADIUS_ATTR_EVENT_TIMESTAMP,
				       now.sec)) {
		wpa_printf(MSG_INFO, "Could not add Event-Timestamp");
		goto fail;
	}

	if (eloop_terminated())
		cause = RADIUS_ACCT_TERMINATE_CAUSE_ADMIN_REBOOT;

	if (stop && cause &&
	    !radius_msg_add_attr_int32(msg, RADIUS_ATTR_ACCT_TERMINATE_CAUSE,
				       cause)) {
		wpa_printf(MSG_INFO, "Could not add Acct-Terminate-Cause");
		goto fail;
	}

	if (radius_client_send(hapd->radius, msg,
			       stop ? RADIUS_ACCT : RADIUS_ACCT_INTERIM,
			       sta->addr) < 0)
		goto fail;
	return;

 fail:
	radius_msg_free(msg);
}


/**
 * accounting_sta_interim - Send a interim STA accounting report
 * @hapd: hostapd BSS data
 * @sta: The station
 */
static void accounting_sta_interim(struct hostapd_data *hapd,
				   struct sta_info *sta)
{
	if (sta->acct_session_started)
		accounting_sta_report(hapd, sta, 0);
}


/**
 * accounting_sta_stop - Stop STA accounting
 * @hapd: hostapd BSS data
 * @sta: The station
 */
void accounting_sta_stop(struct hostapd_data *hapd, struct sta_info *sta)
{
	if (sta->acct_session_started) {
		accounting_sta_report(hapd, sta, 1);
		eloop_cancel_timeout(accounting_interim_update, hapd, sta);
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_RADIUS,
			       HOSTAPD_LEVEL_INFO,
			       "stopped accounting session %08X-%08X",
			       sta->acct_session_id_hi,
			       sta->acct_session_id_lo);
		sta->acct_session_started = 0;
	}
}


void accounting_sta_get_id(struct hostapd_data *hapd,
				  struct sta_info *sta)
{
	sta->acct_session_id_lo = hapd->acct_session_id_lo++;
	if (hapd->acct_session_id_lo == 0) {
		hapd->acct_session_id_hi++;
	}
	sta->acct_session_id_hi = hapd->acct_session_id_hi;
}


/**
 * accounting_receive - Process the RADIUS frames from Accounting Server
 * @msg: RADIUS response message
 * @req: RADIUS request message
 * @shared_secret: RADIUS shared secret
 * @shared_secret_len: Length of shared_secret in octets
 * @data: Context data (struct hostapd_data *)
 * Returns: Processing status
 */
static RadiusRxResult
accounting_receive(struct radius_msg *msg, struct radius_msg *req,
		   const u8 *shared_secret, size_t shared_secret_len,
		   void *data)
{
	if (radius_msg_get_hdr(msg)->code != RADIUS_CODE_ACCOUNTING_RESPONSE) {
		wpa_printf(MSG_INFO, "Unknown RADIUS message code");
		return RADIUS_RX_UNKNOWN;
	}

	if (radius_msg_verify(msg, shared_secret, shared_secret_len, req, 0)) {
		wpa_printf(MSG_INFO, "Incoming RADIUS packet did not have correct Authenticator - dropped");
		return RADIUS_RX_INVALID_AUTHENTICATOR;
	}

	return RADIUS_RX_PROCESSED;
}


static void accounting_report_state(struct hostapd_data *hapd, int on)
{
	struct radius_msg *msg;

	if (!hapd->conf->radius->acct_server || hapd->radius == NULL)
		return;

	/* Inform RADIUS server that accounting will start/stop so that the
	 * server can close old accounting sessions. */
	msg = accounting_msg(hapd, NULL,
			     on ? RADIUS_ACCT_STATUS_TYPE_ACCOUNTING_ON :
			     RADIUS_ACCT_STATUS_TYPE_ACCOUNTING_OFF);
	if (!msg)
		return;

	if (!radius_msg_add_attr_int32(msg, RADIUS_ATTR_ACCT_TERMINATE_CAUSE,
				       RADIUS_ACCT_TERMINATE_CAUSE_NAS_REBOOT))
	{
		wpa_printf(MSG_INFO, "Could not add Acct-Terminate-Cause");
		radius_msg_free(msg);
		return;
	}

	if (radius_client_send(hapd->radius, msg, RADIUS_ACCT, NULL) < 0)
		radius_msg_free(msg);
}


/**
 * accounting_init: Initialize accounting
 * @hapd: hostapd BSS data
 * Returns: 0 on success, -1 on failure
 */
int accounting_init(struct hostapd_data *hapd)
{
	struct os_time now;

	/* Acct-Session-Id should be unique over reboots. Using a random number
	 * is preferred. If that is not available, take the current time. Mix
	 * in microseconds to make this more likely to be unique. */
	os_get_time(&now);
	if (os_get_random((u8 *) &hapd->acct_session_id_hi,
			  sizeof(hapd->acct_session_id_hi)) < 0)
		hapd->acct_session_id_hi = now.sec;
	hapd->acct_session_id_hi ^= now.usec;

	if (radius_client_register(hapd->radius, RADIUS_ACCT,
				   accounting_receive, hapd))
		return -1;

	accounting_report_state(hapd, 1);

	return 0;
}


/**
 * accounting_deinit: Deinitialize accounting
 * @hapd: hostapd BSS data
 */
void accounting_deinit(struct hostapd_data *hapd)
{
	accounting_report_state(hapd, 0);
}
