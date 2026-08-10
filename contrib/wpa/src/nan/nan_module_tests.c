/*
 * NAN NDP/NDL state machine testing
 * Copyright (C) 2025 Intel Corporation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "common/nan_defs.h"
#include "drivers/driver.h"
#include "nan_i.h"
#include "nan_module_tests.h"
#include "crypto/sha256.h"
#include "crypto/sha384.h"
#include "crypto/crypto.h"

#define NAN_TEST_MAX_NAF_LEN     1024
#define NAN_TEST_MAX_PEERS       20
#define NAN_TEST_MAX_ACTIONS     30
#define NAN_TEST_MIN_SLOTS       12
#define NAN_TEST_MAX_LATENCY     3
#define NAN_TEST_PUBLISH_INST_ID 12

static const u8 g_pub_nmi[] = { 0x00, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };
static const u8 g_pub_ndi[] = { 0x00, 0xAA, 0xAA, 0x00, 0x00, 0x00 };
static const u8 g_sub_nmi[] = { 0x00, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB };
static const u8 g_sub_ndi[] = { 0x00, 0xBB, 0xBB, 0xBB, 0x00, 0x00 };

/**
 * struct nan_test_action - NAN test action context
 * @list: Used for global actions list
 * @dev: NAN device for which the action is intended
 * @cb: Callback to be called for the action
 * @ctx: Parameter for the callback function
 *
 * The test utility uses actions to handle asynchronous events between the
 * devices and internally to the local device. When an event is triggered
 * outside of the NAN module context, the event is wrapped by an action item and
 * is processed asynchronously. Examples:
 *
 * NAF transmission: when a NAF is transmitted by a device, it is translated to
 * 2 asynchronous actions: one action to indicate Tx status to the transmitting
 * device and one to indicate an Rx frame to the peer device.
 *
 * NDP event: when an NDP event is fired by the NAN module, it is translated to
 * an asynchronous action to the local device.
 */
struct nan_test_action {
	struct dl_list list;
	struct nan_device *dev;
	int (*cb)(struct nan_device *dev, void *ctx);
	void *ctx;
};

/**
 * nan_test_tx_status_action - NAN test Tx status action context
 * @data: Copy of the original NAF
 * @acked: True iff the NAF was acked
 * @dst: Destination of the NAF
 */
struct nan_test_tx_status_action {
	const struct wpabuf *data;
	bool acked;
	u8 dst[ETH_ALEN];
};

/**
 * nan_test_ndp_notify - NAN test NDP notification handling
 * @type: Notification type
 * @ndp_id: NDP identifier
 * @ssi: Service specific information
 * @ssi_len: Length of service specific information
 */
struct nan_test_ndp_notify {
	enum nan_test_ndp_notify_type type;
	struct nan_ndp_id ndp_id;
	const u8 *ssi;
	size_t ssi_len;
};

/**
 * nan_test_global - Global context for the NAN testing
 * @devs: List of devices. See &struct nan_device.
 * @actions: tracks the NAN actions
 * @elems: Default HT/VHT/HE capabilities elements
 */
struct nan_test_global {
	struct dl_list devs;
	struct dl_list actions;
	struct wpabuf *elems;
};

#define DEV_NOT_INIT_ERR(_dev)                                        \
do {                                                                  \
	if (!(_dev) || !(_dev)->nan)  {                               \
		wpa_printf(MSG_ERROR,                                 \
			   "NAN: %s: device not initialized",         \
			   __func__);                                 \
		return -1;                                            \
	}                                                             \
} while (0)

#define DEV_NOT_INIT_ERR_VOID(_dev)                                   \
do {                                                                  \
	if (!(_dev) || !(_dev)->nan)  {                               \
		wpa_printf(MSG_ERROR,                                 \
			   "NAN: %s: device not initialized",         \
			   __func__);                                 \
		return;                                               \
	}                                                             \
} while (0)


/**
 * nan_test_global_init - Initialize NAN test global data structures
 * @global: NAN test global data structure
 */
static void nan_test_global_init(struct nan_test_global *global)
{
	u8 elems[] = {
		/* HT capabilities */
		0x2d, 0x1a, 0x7e, 0x10, 0x1b, 0xff, 0xff, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,

		/* VHT capabilities */
		0xbf, 0x0c, 0xfa, 0x04, 0x80, 0x03, 0xaa, 0xaa,
		0x00, 0x00, 0xaa, 0xaa, 0x00, 0x00,

		/* HE capabilities */
		0xff, 0x1e, 0x23, 0x01, 0x78, 0xc8, 0x1a, 0x40,
		0x00, 0x1c, 0xbf, 0xce, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0xfa, 0xff, 0xfa, 0xff,
		0xfa, 0xff, 0xfa, 0xff, 0xfa, 0xff, 0xfa, 0xff,
	};

	wpa_printf(MSG_INFO, "%s: Enter", __func__);
	os_memset(global, 0, sizeof(struct nan_test_global));
	dl_list_init(&global->devs);
	dl_list_init(&global->actions);

	global->elems = wpabuf_alloc_copy(elems, sizeof(elems));

	wpa_printf(MSG_INFO, "%s: Done\n", __func__);
}


/**
 * nan_test_dev_deinit - De-initialize NAN device
 * @dev: NAN device
 */
static void nan_test_dev_deinit(struct nan_device *dev)
{
	DEV_NOT_INIT_ERR_VOID(dev);

	os_free(dev->pot_avail);
	nan_stop(dev->nan);
	nan_deinit(dev->nan);
	os_memset(dev, 0, sizeof(struct nan_device));
}


/**
 * nan_test_global_deinit - De-initialize NAN test global data structures
 * @global: NAN test global data structure
 */
static void nan_test_global_deinit(struct nan_test_global *global)
{
	struct nan_device *dev, *next;
	struct nan_test_action *action, *action_next;

	wpa_printf(MSG_INFO, "%s: Enter", __func__);

	dl_list_for_each_safe(dev, next, &global->devs, struct nan_device,
			      list) {
		dl_list_del(&dev->list);
		nan_test_dev_deinit(dev);
		os_free(dev);
	}

	dl_list_for_each_safe(action, action_next, &global->actions,
			      struct nan_test_action, list) {
		dl_list_del(&dev->list);
		os_free(action->ctx);
		os_free(action);
	}

	wpabuf_free(global->elems);
}


/**
 * nan_test_add_action - Add a NAN test action to the list of actions
 * @global: NAN test global data structure
 * @dev: NAN device for which the action is intended
 * @cb: Callback to be called for the action
 * @ctx: Parameter for the callback function
 * Returns: A pointer to the newly added action, or NULL on failure
 */
static struct nan_test_action *
nan_test_add_action(struct nan_test_global *global,
		    struct nan_device *dev,
		    int (*cb)(struct nan_device *dev, void *ctx),
		    void *ctx)
{
	struct nan_test_action *action;

	action = os_malloc(sizeof(struct nan_test_action));
	if (!action) {
		wpa_printf(MSG_ERROR, "NAN Test: Failed to allocate action");
		return NULL;
	}

	action->dev = dev;
	action->cb = cb;
	action->ctx = ctx;

	dl_list_add_tail(&global->actions, &action->list);
	return action;
}


/**
 * nan_test_run_actions - Iterate over all NAN test actions and execute them
 * @global: NAN test global data structure
 * Returns: 0 in success, -1 on error
 *
 * Runs as longs as there are actions to run. Exists where there are no more
 * action to perform or on error.
 */
static int nan_test_run_actions(struct nan_test_global *global)
{
	u32 n_actions = 0;

	wpa_printf(MSG_INFO, "%s: Running actions", __func__);

	while (!dl_list_empty(&global->actions)) {
		struct nan_test_action *action =
			dl_list_first(&global->actions, struct nan_test_action,
				      list);
		int ret;

		dl_list_del(&action->list);

		if (++n_actions > NAN_TEST_MAX_ACTIONS) {
			wpa_printf(MSG_ERROR,
				   "NAN Test action: Too many actions executed");
			return -1;
		}

		if (!action->cb || !action->dev) {
			wpa_printf(MSG_ERROR,
				   "NAN Test action: Invalid action");
			return -1;
		}

		wpa_printf(MSG_INFO, "%s: ===> NAN Test action <===",
			   action->dev->name);
		ret = action->cb(action->dev, action->ctx);

		/*
		 * The action context should be freed by the callback function
		 * that understands the content of the context and can properly
		 * handle it.
		 */
		os_free(action);

		if (ret) {
			wpa_printf(MSG_ERROR, "NAN Test action: FAILED");
			return ret;
		}
	}

	wpa_printf(MSG_INFO, "%s: Running actions done", __func__);
	return 0;
}


static int nan_test_start_cb(void *ctx, const struct nan_cluster_config *config)
{
	struct nan_device *dev = ctx;

	DEV_NOT_INIT_ERR(dev);

	return 0;
}


/*
 * nan_test_stop_cb - Stop the NAN device.
 * @ctx: Pointer to the &struct nan_device
 */
static void nan_test_stop_cb(void *ctx)
{
	struct nan_device *dev = ctx;

	DEV_NOT_INIT_ERR_VOID(dev);

	wpa_printf(MSG_INFO, "%s: %s: Done", __func__, dev->name);
}


static int nan_test_nan_ndp_action(struct nan_device *dev, void *ctx)
{
	struct nan_ndp_params *params = ctx;
	int ret;

	wpa_printf(MSG_INFO, "%s: %s: type=%u, peer_nmi=" MACSTR,
		   dev->name, __func__, params->type,
		   MAC2STR(params->ndp_id.peer_nmi));

	ret = nan_handle_ndp_setup(dev->nan, params);

	os_free(params);

	return ret;
}


/*
 * nan_ndp_notify_action - Default NAN NDP notification handling
 * @dev: NAN device
 * @ctx: Pointer to &struct nan_test_ndp_notify
 */
static int nan_ndp_notify_action(struct nan_device *dev, void *ctx)
{
	struct nan_test_ndp_notify *notify = ctx;
	struct nan_test_action *action;
	int ret;

	wpa_printf(MSG_INFO, "%s: %s: type=%u, peer_nmi=" MACSTR,
		   dev->name, __func__, notify->type,
		   MAC2STR(notify->ndp_id.peer_nmi));

	ret = -1;
	if (notify->type == NAN_TEST_NDP_NOTIFY_REQUEST) {
		struct nan_ndp_params *params;
		struct nan_device *curd;
		bool found = false;

		dl_list_for_each(curd, &dev->global->devs, struct nan_device,
				 list) {
			if (ether_addr_equal(curd->nmi,
					     notify->ndp_id.peer_nmi)) {
				found = true;
				break;
			}
		}

		if (!found) {
			wpa_printf(MSG_ERROR, "Peer device not found");
			ret = -1;
			goto out;
		}

		params = os_zalloc(sizeof(struct nan_ndp_params));
		if (!params) {
			ret = -1;
			goto out;
		}

		params->type = NAN_NDP_ACTION_RESP;
		os_memcpy(params->ndp_id.peer_nmi, notify->ndp_id.peer_nmi,
			  ETH_ALEN);
		os_memcpy(params->ndp_id.init_ndi, notify->ndp_id.init_ndi,
			  ETH_ALEN);
		params->ndp_id.id = notify->ndp_id.id;
		params->qos.min_slots = NAN_TEST_MIN_SLOTS;
		params->qos.max_latency = NAN_TEST_MAX_LATENCY;

		if (dev->conf->ndp_confs[dev->n_ndps].accept_request) {
			wpa_printf(MSG_INFO, "%s: Accepting request",
				   dev->name);

			os_memcpy(params->u.resp.resp_ndi, dev->ndi, ETH_ALEN);
			params->u.resp.status = NAN_NDP_STATUS_ACCEPTED;
			dev->conf->schedule_cb(&params->sched);
			params->sched.elems = dev->global->elems;
			params->sched_valid = 1;

			params->sec.csid =
				dev->conf->ndp_confs[dev->n_ndps].csid;
			os_memcpy(params->sec.pmk, dev->conf->pmk, PMK_LEN);
		} else {
			wpa_printf(MSG_INFO, "%s: Rejecting request",
				   dev->name);

			params->u.resp.status = NAN_NDP_STATUS_REJECTED;
			params->u.resp.reason_code =
				dev->conf->ndp_confs[dev->n_ndps].reason;
		}

		action = nan_test_add_action(dev->global, dev,
					     nan_test_nan_ndp_action,
					     params);
		if (action)
			ret = 0;
	} else if (notify->type == NAN_TEST_NDP_NOTIFY_RESPONSE) {
		struct nan_ndp_params *params;

		params = os_zalloc(sizeof(struct nan_ndp_params));
		if (!params) {
			ret = -1;
			goto out;
		}

		params->type = NAN_NDP_ACTION_CONF;
		os_memcpy(params->ndp_id.peer_nmi, notify->ndp_id.peer_nmi,
			  ETH_ALEN);
		os_memcpy(params->ndp_id.init_ndi, notify->ndp_id.init_ndi,
			  ETH_ALEN);
		params->ndp_id.id = notify->ndp_id.id;

		params->qos.min_slots = NAN_TEST_MIN_SLOTS;
		params->qos.max_latency = NAN_TEST_MAX_LATENCY;

		if (dev->conf->ndp_confs[dev->n_ndps].accept_request) {
			wpa_printf(MSG_INFO, "%s: Accepting response",
				   dev->name);

			os_memcpy(params->u.resp.resp_ndi, dev->ndi, ETH_ALEN);
			params->u.resp.status = NAN_NDP_STATUS_ACCEPTED;

			if (!dev->conf->schedule_conf_cb) {
				wpa_printf(MSG_ERROR,
					   "%s: No schedule conf cb defined",
					   dev->name);
				os_free(params);
				ret = -1;
				goto out;
			}

			dev->conf->schedule_conf_cb(&params->sched);
			params->sched.elems = dev->global->elems;
			params->sched_valid = 1;

			params->sec.csid =
				dev->conf->ndp_confs[dev->n_ndps].csid;
			os_memcpy(params->sec.pmk, dev->conf->pmk, PMK_LEN);
		} else {
			wpa_printf(MSG_INFO, "%s: Rejecting response",
				   dev->name);

			params->u.resp.status = NAN_NDP_STATUS_REJECTED;
			params->u.resp.reason_code =
				dev->conf->ndp_confs[dev->n_ndps].reason;
		}

		action = nan_test_add_action(dev->global, dev,
					     nan_test_nan_ndp_action,
					     params);
		if (action)
			ret = 0;
	} else if (notify->type ==
		   dev->conf->ndp_confs[dev->n_ndps].expected_result) {
		ret = 0;
		if (notify->type == NAN_TEST_NDP_NOTIFY_CONNECTED) {
			if (dev->conf->ndp_confs[dev->n_ndps].
			    term_once_connected) {
				struct nan_ndp_params *params;

				wpa_printf(MSG_INFO,
					   "%s: Connected successfully. Now term",
					   dev->name);

				params = os_zalloc(sizeof(*params));
				if (!params) {
					ret = -1;
					goto out;
				}

				params->type = NAN_NDP_ACTION_TERM;
				os_memcpy(params->ndp_id.peer_nmi,
					  notify->ndp_id.peer_nmi,
					  ETH_ALEN);
				os_memcpy(params->ndp_id.init_ndi,
					  notify->ndp_id.init_ndi,
					  ETH_ALEN);
				params->ndp_id.id = notify->ndp_id.id;

				action = nan_test_add_action(
					dev->global, dev,
					nan_test_nan_ndp_action, params);
				if (action)
					ret = 0;
			}
		}
	} else if (notify->type == NAN_TEST_NDP_NOTIFY_DISCONNECTED &&
		   dev->connected_notify_received) {
		wpa_printf(MSG_INFO,
			   "%s: Disconnected after connected as expected. Test case done",
			   dev->name);
		ret = 0;
	} else {
		wpa_printf(MSG_ERROR,
			   "%s: Unexpected notify: type=%u expected=%u",
			   dev->name, notify->type,
			   dev->conf->ndp_confs[dev->n_ndps].expected_result);
		ret = -1;
	}

out:
	os_free((void *) notify->ssi);
	os_free(notify);
	return ret;
}


static void nan_test_ndp_action(struct nan_device *dev, enum
				nan_test_ndp_notify_type type,
				struct nan_ndp_id *ndp_id,
				u8 publish_inst_id,
				const u8 *ssi, size_t ssi_len,
				enum nan_cipher_suite_id csid,
				const u8 *pmkid)
{
	struct nan_test_ndp_notify *notify;

	DEV_NOT_INIT_ERR_VOID(dev);

	wpa_printf(MSG_INFO, "%s: %s: Enter: type=%u",
		   dev->name, __func__, type);

	notify = os_zalloc(sizeof(struct nan_test_ndp_notify));
	if (!notify) {
		wpa_printf(MSG_ERROR,
			   "Failed allocation: nan_test_ndp_notify");
		return;
	}

	notify->type = type;
	os_memcpy(&notify->ndp_id, ndp_id, sizeof(notify->ndp_id));

	if (ssi && ssi_len) {
		notify->ssi = os_memdup(ssi, ssi_len);
		if (!notify->ssi) {
			wpa_printf(MSG_ERROR, "Failed allocation: ssi");
			os_free(notify);
			return;
		}
		notify->ssi_len = ssi_len;
	}

	if (nan_test_add_action(dev->global, dev, nan_ndp_notify_action,
				notify))
		return;

	os_free((void *) notify->ssi);
	os_free(notify);
}


/**
 * nan_test_ndp_action_notfi_cb - Callback for NDP request
 * @ctx: Pointer to &struct nan_device
 * @params: NDP action notification parameters
 *
 * The handling of the event is done asynchronously through the NAN test actions
 * processing.
 */
static void
nan_test_ndp_action_notfi_cb(void *ctx,
			     struct nan_ndp_action_notif_params *params)
{
	struct nan_device *dev = ctx;
	enum nan_test_ndp_notify_type type;

	DEV_NOT_INIT_ERR_VOID(dev);

	if (params->is_request)
		type = NAN_TEST_NDP_NOTIFY_REQUEST;
	else
		type = NAN_TEST_NDP_NOTIFY_RESPONSE;

	nan_test_ndp_action(dev, type, &params->ndp_id,
			    params->publish_inst_id, params->ssi,
			    params->ssi_len, params->csid, params->pmkid);
}


/**
 * nan_test_ndp_connected_cb - Callback for NDP connected
 * @ctx: Pointer to &struct nan_device
 * @params: NDP action notification parameters
 * Returns: 0 on success, -1 on failure
 *
 * The handling of the event is done asynchronously through the NAN test actions
 * processing.
 */
static int nan_test_ndp_connected_cb(void *ctx,
				     struct nan_ndp_connection_params *params)
{
	struct nan_device *dev = ctx;
	struct nan_peer_schedule sched;
	struct nan_peer_potential_avail pot;

	DEV_NOT_INIT_ERR(dev);

	wpa_printf(MSG_INFO,
		   "%s: %s: Enter. local_ndi=" MACSTR " peer_ndi=" MACSTR,
		   dev->name, __func__,
		   MAC2STR(params->local_ndi), MAC2STR(params->peer_ndi));

	nan_peer_get_schedule_info(dev->nan, params->ndp_id.peer_nmi, &sched);
	nan_peer_get_pot_avail(dev->nan, params->ndp_id.peer_nmi, &pot);

	if (nan_peer_get_tk(dev->nan, params->ndp_id.peer_nmi,
			    params->peer_ndi, params->local_ndi,
			    dev->tk, &dev->tk_len, &dev->csid) == 0) {
		wpa_hexdump(MSG_DEBUG, "NAN Test: TK", dev->tk, dev->tk_len);

		if (dev->csid !=
		    dev->conf->ndp_confs[dev->n_ndps].expected_csid) {
			wpa_printf(MSG_ERROR,
				   "%s: Unexpected CSID: got %u expected %u",
				   dev->name, dev->csid,
				   dev->conf->ndp_confs[dev->n_ndps].
				   expected_csid);
			return -1;
		}
	}

	nan_test_ndp_action(dev, NAN_TEST_NDP_NOTIFY_CONNECTED, &params->ndp_id,
			    0, params->ssi, params->ssi_len, NAN_CS_NONE, NULL);

	dev->connected_notify_received = true;

	return 0;
}


/**
 * nan_test_ndp_disconnected_cb - Callback for NDP disconnected
 * @ctx: Pointer to &struct nan_device
 * @ndp_id: NDP identifier
 * @local_ndi: Local NDI address
 * @peer_ndi: Peer NDI address
 * @reason: Reason for disconnection
 * @locally_generated: true if locally generated, false if triggered by peer
 * @remove_sta: true if the NDI station should be removed
 * @failure: true if NDP setup failed, false if graceful disconnection
 * @gtk_id: GTK key ID used for the NDP; 0 if no GTK was used
 *
 * The handling of the event is done asynchronously through the NAN test actions
 * processing.
 */
static void nan_test_ndp_disconnected_cb(void *ctx, struct nan_ndp_id *ndp_id,
					 const u8 *local_ndi,
					 const u8 *peer_ndi,
					 enum nan_reason reason,
					 bool locally_generated,
					 bool remove_sta,
					 bool failure,
					 u8 gtk_id)
{
	struct nan_device *dev = ctx;

	DEV_NOT_INIT_ERR_VOID(dev);

	wpa_printf(MSG_INFO, "%s: %s: Enter", dev->name, __func__);

	nan_test_ndp_action(dev, NAN_TEST_NDP_NOTIFY_DISCONNECTED,
			    ndp_id, 0, NULL, 0, NAN_CS_NONE, NULL);

	dev->disconnected_notify_received = true;
}


/**
 * nan_test_send_naf_cb_action - NAN test action to send a NAN to a device
 * @dev: NAN device
 * @ctx: Pointer to a buffer holding the NAF to be sent
 */
static int nan_test_send_naf_cb_action(struct nan_device *dev, void *ctx)
{
	struct wpabuf *data = ctx;
	int ret;

	DEV_NOT_INIT_ERR(dev);

	wpa_printf(MSG_INFO, "%s: %s: Enter", dev->name, __func__);
	wpa_hexdump(MSG_DEBUG, "NAN Test: NAF:", wpabuf_head(data),
		    wpabuf_len(data));

	ret = nan_action_rx(dev->nan, wpabuf_head(data), wpabuf_len(data));
	wpabuf_free(data);
	return ret;
}


/**
 * nan_test_tx_status_action - NAN test action to send Tx status to a device
 * @dev: NAN device
 * @ctx: Pointer to &struct nan_test_tx_status_action
 */
static int nan_test_tx_status_action(struct nan_device *dev, void *ctx)
{
	struct nan_test_tx_status_action *tx_status = ctx;
	int ret;

	DEV_NOT_INIT_ERR(dev);

	wpa_printf(MSG_INFO, "%s: %s: enter", dev->name, __func__);

	ret = nan_tx_status(dev->nan, tx_status->dst,
			    wpabuf_head(tx_status->data),
			    wpabuf_len(tx_status->data), tx_status->acked);

	wpabuf_free((struct wpabuf *)tx_status->data);
	os_free(tx_status);
	return ret;
}


/**
 * nan_test_send_naf_cb - NAN send NAF callback function
 * @ctx: Pointer to &struct nan_device
 * @dst: Destination NAN Management Interface address
 * @src: Source NAN Management Interface address
 * @cluster_id: NAN Cluster ID
 *
 * The callback builds the management frame and creates the following NAN test
 * actions:
 * - NAN test tx status action: to be sent to the transmitting device
 * - NAN test send NAF action: to be sent to the destination device.
 */
static int nan_test_send_naf_cb(void *ctx, const u8 *dst, const u8 *src,
				const u8 *cluster_id, struct wpabuf *buf)
{
	struct nan_device *dev = ctx;
	struct nan_device *curd;
	struct ieee80211_hdr *hdr;
	struct wpabuf *data = NULL;
	struct nan_test_tx_status_action *tx_status = NULL;
	struct nan_test_action *dev_action, *cur_action;
	bool found = false;

	DEV_NOT_INIT_ERR(dev);

	if (!dst)
		return -1;

	wpa_printf(MSG_INFO, "%s: %s: Enter " MACSTR, __func__, dev->name,
		   MAC2STR(dst));

	dl_list_for_each(curd, &dev->global->devs, struct nan_device, list) {
		if (ether_addr_equal(curd->nmi, dst) ||
		    ether_addr_equal(curd->ndi, dst)) {
			found = true;
			break;
		}
	}

	if (!found) {
		wpa_printf(MSG_ERROR, "%s: Destination device not found",
			   __func__);
		return -1;
	}

	/* Prepare action to send the frame to the peer */
	data = wpabuf_alloc(sizeof(struct ieee80211_hdr) + wpabuf_len(buf));
	if (!data) {
		wpa_printf(MSG_ERROR, "%s: Failed to allocate NAF", __func__);
		return -1;
	}

	hdr = wpabuf_put(data, sizeof(struct ieee80211_hdr));
	hdr->frame_control =
		IEEE80211_FC(WLAN_FC_TYPE_MGMT, WLAN_FC_STYPE_ACTION);

	os_memcpy(hdr->addr1, dst, ETH_ALEN);
	if (!src)
		os_memcpy(hdr->addr2, dev->nmi, ETH_ALEN);
	else
		os_memcpy(hdr->addr2, src, ETH_ALEN);
	if (cluster_id)
		os_memcpy(hdr->addr3, cluster_id, ETH_ALEN);

	wpabuf_put_data(data, wpabuf_head(buf), wpabuf_len(buf));

	/* Prepare action to send Tx status */
	tx_status = os_malloc(sizeof(struct nan_test_tx_status_action));
	if (!tx_status) {
		wpa_printf(MSG_ERROR, "%s: Failed to allocate Tx status",
			   __func__);
		goto fail;
	}

	tx_status->data = wpabuf_dup(data);
	if (!tx_status->data) {
		wpa_printf(MSG_ERROR, "%s: Failed to allocate Tx status data",
			   __func__);
		goto fail;
	}

	os_memcpy(tx_status->dst, dst, ETH_ALEN);
	tx_status->acked = 1;

	/* First send the TX status */
	dev_action = nan_test_add_action(dev->global, dev,
					 nan_test_tx_status_action, tx_status);
	if (!dev_action)
		goto fail;

	/* And then deliver the frame */
	cur_action = nan_test_add_action(curd->global, curd,
					 nan_test_send_naf_cb_action, data);
	if (!cur_action)
		goto fail;

	return 0;

fail:
	wpabuf_free(data);
	if (tx_status)
		wpabuf_free((struct wpabuf *) tx_status->data);

	os_free(tx_status);

	return -1;
}


/**
 * nan_test_get_chans_cb - Get NAN supported channels callback
 * @ctx: Pointer to &struct nan_device
 * @map_id: Channel map identifier
 * @chans: Pointer to &struct nan_channels to be filled with supported channels
 */
static int nan_test_get_chans_cb(void *ctx, u8 map_id,
				 struct nan_channels *chans)
{
	struct nan_device *dev = ctx;

	DEV_NOT_INIT_ERR(dev);

	wpa_printf(MSG_INFO, "%s: %s: Enter", dev->name, __func__);

	return dev->conf->get_chans_cb(chans);
}


/**
 * nan_test_is_valid_publish_id_cb - Check if the publish instance ID is valid
 * @ctx: Pointer to &struct nan_device
 * @instance_id: Publish instance ID
 * @service_id: Buffer to be filled with the service ID
 * Returns true if the instance ID is valid, false otherwise
 */
static bool nan_test_is_valid_publish_id_cb(void *ctx, u8 instance_id,
					    u8 *service_id)
{
	if (instance_id != NAN_TEST_PUBLISH_INST_ID)
		return false;

	os_memset(service_id, 0xaa, NAN_SERVICE_ID_LEN);
	return true;
}


/*
 * nan_test_set_peer_schedule_cb - Set peer schedule callback
 *
 * @ctx: Pointer to &struct nan_device
 * @sched: Pointer to &struct nan_peer_schedule to be filled
 */
static int
nan_test_set_peer_schedule_cb(void *ctx, const u8 *nmi_addr, bool new_sta,
			      u16 cdw, u8 sequence_id,
			      u16 max_channel_switch_time,
			      const struct nan_peer_schedule *sched,
			      const struct wpabuf *ulw_elems)
{
	struct nan_device *dev = (struct nan_device *)ctx;

	DEV_NOT_INIT_ERR(dev);

	wpa_printf(MSG_INFO, "%s: %s: Enter", dev->name, __func__);
	wpa_printf(MSG_INFO, "%s: nmi_addr=" MACSTR
		   " new_sta=%d cdw=%u sequence_id=%u max_channel_switch_time=%u",
		   dev->name, MAC2STR(nmi_addr), new_sta, cdw, sequence_id,
		   max_channel_switch_time);

	return 0;
}


/**
 * nan_test_dev_init - Initialize a test device instance
 * @dev: the instance of the device to initialize
 */
static int nan_test_dev_init(struct nan_device *dev)
{
	struct nan_config nan;

	os_memset(&nan, 0, sizeof(nan));
	os_memcpy(nan.nmi_addr, dev->nmi, ETH_ALEN);
	nan.cb_ctx = dev;

	nan.start = nan_test_start_cb;
	nan.stop = nan_test_stop_cb;
	nan.ndp_action_notif = nan_test_ndp_action_notfi_cb;
	nan.ndp_connected = nan_test_ndp_connected_cb;
	nan.ndp_disconnected = nan_test_ndp_disconnected_cb;
	nan.send_naf = nan_test_send_naf_cb;
	nan.get_chans = nan_test_get_chans_cb;
	nan.is_valid_publish_id = nan_test_is_valid_publish_id_cb;
	nan.set_peer_schedule = nan_test_set_peer_schedule_cb;

	/* Awake on every DW on 2 GHz and 5 GHz */
	nan.dev_capa.cdw_info = 0x9;
	nan.dev_capa.supported_bands = NAN_DEV_CAPA_SBAND_2G |
		NAN_DEV_CAPA_SBAND_5G |
		NAN_DEV_CAPA_SBAND_6G;

	nan.dev_capa.op_mode = NAN_DEV_CAPA_OP_MODE_PHY_MODE;
	nan.dev_capa.n_antennas = 0x22;
	nan.dev_capa.channel_switch_time = 10;
	nan.dev_capa.capa = 0;

	nan.dev_capa_ext_reg_info = 0;
	nan.pairing_cfg.npk_caching = true;
	nan.pairing_cfg.pairing_setup = true;

	dev->nan = nan_init(&nan);
	if (!dev->nan) {
		wpa_printf(MSG_DEBUG, "NAN: Failed to init");
		return -1;
	}

	return 0;
}


/**
 * nan_test_start_dev - Start a NAN test device
 * @global: NAN test global data structure
 * @name: Name of the device
 * @nmi: NAN Management interface address
 * @ndi: NAN Data interface address
 * @cconf: NAN cluster configuration
 * @dconf: Test device configuration
 */
static struct nan_device *
nan_test_start_dev(struct nan_test_global *global,
		   const char *name, const u8 *nmi,
		   const u8 *ndi,
		   struct nan_cluster_config *conf,
		   const struct nan_test_dev_conf *dconf)
{
	struct nan_device *dev;
	int ret;
	size_t nlen;

	dev = os_zalloc(sizeof(struct nan_device));
	if (!dev)
		return NULL;

	nlen = os_strlen(name);
	if (nlen >= sizeof(dev->name))
		nlen = sizeof(dev->name) - 1;
	os_memcpy(dev->name, name, nlen);
	os_memcpy(dev->nmi, nmi, sizeof(dev->nmi));
	os_memcpy(dev->ndi, ndi, sizeof(dev->ndi));
	dl_list_init(&dev->list);
	dev->global = global;

	if (dconf->pot_avail_len) {
		dev->pot_avail = os_memdup(dconf->pot_avail,
					   dconf->pot_avail_len);
		if (!dev->pot_avail)
			goto fail;
		dev->pot_avail_len = dconf->pot_avail_len;
	}

	ret = nan_test_dev_init(dev);
	if (ret)
		goto fail;

	dev->conf = dconf;
	ret = nan_start(dev->nan, conf);
	if (ret)
		goto fail;

	dl_list_add(&global->devs, &dev->list);
	return dev;

fail:
	nan_test_dev_deinit(dev);
	os_free(dev);
	return NULL;
}


/**
 * nan_test_setup_devices - Setup the test devices
 * @global: NAN test global data structure
 * @pub_conf: Publisher test configuration
 * @sub_conf: Subscriber test configuration
 */
static struct nan_device *
nan_test_setup_devices(struct nan_test_global *global,
		       const struct nan_test_dev_conf *pub_conf,
		       const struct nan_test_dev_conf *sub_conf)
{
	struct nan_cluster_config cconf = {
		.master_pref = 2,
		.dual_band = 1,
	};
	/*
	 * Device attributes containing:
	 * 1. Device capability attribute (ID=0x0F, len=10):
	 *    - map_id=0
	 *    - cdw_info=0x0009 (awake on every DW on 2G and 5G)
	 *    - supported_bands=0x07 (2G, 5G, 6G)
	 *    - op_mode=0x01
	 *    - n_antennas=0x22
	 *    - channel_switch_time=0x000a (10)
	 *    - capa=0x00
	 * 2. Availability attribute (ID=0x12, len=12)
	 */
	const u8 attrs[] = {
		/* Device capability attribute */
		0x0f, 0x09, 0x00, 0x00, 0x09, 0x00, 0x07, 0x01,
		0x22, 0x0a, 0x00, 0x00,
		/* Availability attribute */
		0x12, 0x0c, 0x00, 0x01, 0x20, 0x00, 0x07, 0x00,
		0x1a, 0x00, 0x11, 0x51, 0xff, 0x07, 0x00,
	};

	struct nan_device *pub, *sub;

	wpa_printf(MSG_INFO, "%s: Enter\n", __func__);

	pub = nan_test_start_dev(global, "publisher", g_pub_nmi, g_pub_ndi,
				 &cconf, pub_conf);
	if (!pub)
		goto fail;

	sub = nan_test_start_dev(global, "subscriber", g_sub_nmi, g_sub_ndi,
				 &cconf, sub_conf);
	if (!sub)
		goto fail;

	nan_add_peer(pub->nan, g_sub_nmi, attrs, sizeof(attrs));
	nan_add_peer(sub->nan, g_pub_nmi, attrs, sizeof(attrs));

	wpa_printf(MSG_INFO, "\n%s: Done\n", __func__);
	return sub;

fail:
	wpa_printf(MSG_INFO, "\n%s: Fail\n", __func__);
	return NULL;
}


static int nan_test_ndp_request(struct nan_device *sub, const u8 *pub_nmi)
{
	struct nan_ndp_params *params;
	struct nan_test_action *action;

	DEV_NOT_INIT_ERR(sub);

	params = os_zalloc(sizeof(struct nan_ndp_params));
	if (!params) {
		wpa_printf(MSG_ERROR, "Failed allocation: nan_ndp_params");
		return -1;
	}

	params->type = NAN_NDP_ACTION_REQ;
	os_memcpy(params->ndp_id.peer_nmi, pub_nmi, ETH_ALEN);
	os_memcpy(params->ndp_id.init_ndi, sub->ndi, ETH_ALEN);
	params->ndp_id.id = ++sub->counter;
	params->qos.min_slots = NAN_TEST_MIN_SLOTS;
	params->qos.max_latency = NAN_TEST_MAX_LATENCY;
	params->sec.csid = sub->conf->ndp_confs[sub->n_ndps].csid;
	os_memcpy(params->sec.pmk, sub->conf->pmk, PMK_LEN);

	/* Use the device specific schedule callback */
	sub->conf->schedule_cb(&params->sched);
	params->sched_valid = 1;
	params->sched.elems = sub->global->elems;

	params->u.req.publish_inst_id = NAN_TEST_PUBLISH_INST_ID;
	os_memset(params->u.req.service_id, 0xaa, NAN_SERVICE_ID_LEN);

	action = nan_test_add_action(sub->global, sub, nan_test_nan_ndp_action,
				     params);
	if (action)
		return 0;

	wpa_printf(MSG_ERROR, "Failed adding NDP request action");
	os_free(params);

	return -1;
}


/**
 * nan_test_ndp_setup - test NDP setup
 * @global: NAN test global data structure
 * @pub_conf: Publisher test configuration
 * @sub_conf: Subscriber test configuration
 *
 * Create the test devices, perform the basic publish/subscribe/match to allow
 * NDP establishment and trigger an NDP request flow based on the actions
 * mechanism.
 */
static struct nan_device *
nan_test_ndp_setup(struct nan_test_global *global,
		   const struct nan_test_dev_conf *pub_conf,
		   const struct nan_test_dev_conf *sub_conf)
{
	if (pub_conf->n_ndps != sub_conf->n_ndps || !pub_conf->n_ndps ||
	    pub_conf->n_ndps >= NAN_MAX_NUM_NDPS) {
		wpa_printf(MSG_DEBUG,
			   "NAN Test: Publisher and Subscriber n_ndps mismatch or invalid");
		return NULL;
	}

	return nan_test_setup_devices(global, pub_conf, sub_conf);
}


static int nan_test_verify_expected_result(struct nan_device *dev)
{
	DEV_NOT_INIT_ERR(dev);

	if (dev->conf->ndp_confs[dev->n_ndps].expected_result ==
	    NAN_TEST_NDP_NOTIFY_CONNECTED &&
	    !dev->connected_notify_received) {
		wpa_printf(MSG_ERROR,
			   "%s: Expected connected notify not received",
			   dev->name);
		return -1;
	}

	if (dev->conf->ndp_confs[dev->n_ndps].expected_result ==
	    NAN_TEST_NDP_NOTIFY_DISCONNECTED &&
	    !dev->disconnected_notify_received) {
		wpa_printf(MSG_ERROR,
			   "%s: Expected disconnected notify not received",
			   dev->name);
		return -1;
	}

	return 0;
}


static int nan_test_iteration_done(struct nan_test_global *global)
{
	struct nan_device *dev;
	u8 tk[NAN_TK_MAX_LEN];
	size_t tk_len = 0;
	enum nan_cipher_suite_id csid;
	int ret;

	os_memset(tk, 0, sizeof(tk));

	dl_list_for_each(dev, &global->devs, struct nan_device, list) {
		ret = nan_test_verify_expected_result(dev);

		if (ret)
			return ret;

		if (tk_len == 0 && dev->tk_len > 0) {
			os_memcpy(tk, dev->tk, dev->tk_len);
			tk_len = dev->tk_len;
			csid = dev->csid;
		} else if (dev->tk_len > 0) {
			if (tk_len != dev->tk_len ||
			    csid != dev->csid ||
			    os_memcmp(tk, dev->tk, tk_len) != 0) {
				wpa_printf(MSG_ERROR,
					   "%s: TK mismatch with other device",
					   dev->name);
				return -1;
			}

			wpa_printf(MSG_INFO, "%s: TK matches with other device",
				   dev->name);
		}

		dev->connected_notify_received = false;
		dev->disconnected_notify_received = false;

		os_memset(dev->tk, 0, sizeof(dev->tk));
		dev->tk_len = 0;

		dev->n_ndps++;
	}

	return 0;
}


/**
 * nan_test_run - Run the NAN tests
 * Iterates over all tests cases and for each test case initializes the global
 * context, creates the NAN devices and perform the test case.
 */
static int nan_test_run(void)
{
	struct nan_test_global global;
	const struct nan_test_case *curr_tc;
	bool all_failed = false;

	while ((curr_tc = nan_test_case_get_next())) {
		struct nan_device *sub;
		bool failed = false;
		size_t i;

		wpa_printf(MSG_INFO,
			   "\n======> NAN TEST CASE: %s <======\n",
			   curr_tc->name);

		nan_test_global_init(&global);
		sub = nan_test_ndp_setup(&global,
					 &curr_tc->pub_conf,
					 &curr_tc->sub_conf);
		if (!sub) {
			wpa_printf(MSG_ERROR,
				   "NAN Test: Failed to setup devices");
			nan_test_global_deinit(&global);
			failed = true;
			continue;
		}

		for (i = 0; i < sub->conf->n_ndps; i++) {
			int ret = nan_test_ndp_request(sub, g_pub_nmi);

			if (!ret)
				ret = nan_test_run_actions(&global);

			if (!ret)
				ret = nan_test_iteration_done(&global);

			wpa_printf(MSG_INFO,
				   "\n======> NAN TEST CASE: %s: iter=%zu: result=%s <======\n",
				   curr_tc->name, i,
				   ret ? "FAILED" : "SUCCESS");

			if (ret) {
				failed = true;
				break;
			}
		}

		wpa_printf(MSG_INFO,
			   "\n======> NAN TEST CASE: %s: Done. Result=%s <======\n",
			   curr_tc->name, failed ? "FAILED" : "SUCCESS");

		all_failed |= failed;

		nan_test_global_deinit(&global);
	}

	return all_failed ? -1 : 0;
}


static const u8 nan_key[32] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
};

static u8 nan_addrs[][ETH_ALEN] = {
	{
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
	},
	{
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
	},
};

static u8 nan_nonce[][WPA_NONCE_LEN] = {
	{
		0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
		0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
		0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
		0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	},
	{
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	},
};

static struct nan_ptk nan_ptk_128 = {
	.kck =	{
		0x67, 0x40, 0x47, 0x6f, 0x9c, 0xb4, 0xcd, 0xee,
		0xfb, 0xc7, 0xad, 0x25, 0x78, 0xd5, 0x5e, 0xb2,
	},
	.kek = {
		0xe5, 0xc7, 0x55, 0xbb, 0x4e, 0x4f, 0xd7, 0xbb,
		0xdb, 0x19, 0xa5, 0xb3, 0x6c, 0xcc, 0x77, 0xe3,
	},
	.tk = {
		0xe2, 0x7f, 0xfc, 0x7b, 0xa2, 0xd6, 0xb2, 0x44,
		0x63, 0x4a, 0x0c, 0xf2, 0x09, 0xf8, 0x06, 0x9a,
	},
	.kck_len = 16,
	.kek_len = 16,
	.tk_len = 16,
};

static struct nan_ptk nan_ptk_256 = {
	.kck =	{
		0x72, 0x70, 0xd5, 0xdc, 0x05, 0x44, 0x8a, 0xb4,
		0x1a, 0x73, 0xe5, 0x51, 0x56, 0xa0, 0x78, 0x3a,
		0x33, 0x74, 0x62, 0x4e, 0x81, 0xe0, 0x39, 0xf8,
	},
	.kek = {
		0x55, 0x3e, 0x2c, 0x8a, 0x0b, 0xab, 0xfc, 0xe1,
		0xf9, 0x45, 0xfb, 0xa7, 0xa6, 0xc9, 0x0c, 0x77,
		0x76, 0x64, 0x6b, 0xaa, 0xc0, 0x59, 0x7c, 0xd6,
		0xa4, 0x99, 0x31, 0xf2, 0x6e, 0xbf, 0x0e, 0x49,
	},
	.tk = {
		0x44, 0x61, 0x12, 0xba, 0x4c, 0x25, 0x8b, 0xe5,
		0x29, 0x33, 0x8f, 0x77, 0x45, 0x55, 0x13, 0x87,
		0x69, 0x61, 0xf4, 0x33, 0xad, 0x87, 0x47, 0xbb,
		0xa1, 0xd6, 0x5b, 0x4d, 0xaf, 0x0a, 0x37, 0x5a
	},
	.kck_len = 24,
	.kek_len = 32,
	.tk_len = 32,
};

static const u8 nan_service_id[NAN_SERVICE_ID_LEN] = {
	0x48, 0xda, 0x63, 0xdc, 0xde, 0x19,
};

static u8 nan_pmkid_128[PMKID_LEN] = {
	0xcf, 0x4f, 0x64, 0x44, 0xd8, 0xe2, 0xc4, 0xef,
	0xa4, 0xf3, 0xa6, 0x51, 0xf3, 0x0f, 0x63, 0x4f,
};

static u8 nan_pmkid_256[PMKID_LEN] = {
	0x08, 0x11, 0x23, 0xc8, 0x57, 0x52, 0x6a, 0x09,
	0x6d, 0x46, 0x48, 0x2b, 0xa8, 0x02, 0xfc, 0x28,
};

static u8 nan_crypto_sha256_digest[SHA256_MAC_LEN] = {
	0xde, 0x7a, 0xa9, 0x34, 0x0d, 0x1d, 0xbb, 0x73,
	0x4d, 0x4d, 0x5a, 0xcb, 0x6b, 0xe4, 0xed, 0xc9,
	0x0a, 0x16, 0x37, 0xaa, 0x1e, 0x5a, 0x34, 0x2e,
	0x58, 0xdd, 0x0b, 0x30, 0x4b, 0xa4, 0x8c, 0x32,
};

static u8 nan_crypto_sha384_digest[SHA384_MAC_LEN] = {
	0xb1, 0xf9, 0x75, 0xbd, 0x80, 0x84, 0x21, 0x35,
	0xc2, 0x38, 0xf7, 0x0d, 0x03, 0x32, 0x38, 0xe7,
	0xbb, 0x55, 0x39, 0x21, 0xf5, 0xb3, 0xbd, 0x5c,
	0x93, 0x78, 0x52, 0x61, 0x6f, 0x83, 0x58, 0x2d,
	0xdc, 0x0d, 0xe2, 0x25, 0x5e, 0x72, 0x6c, 0x95,
	0xd0, 0xcb, 0x09, 0xef, 0x2a, 0x8f, 0x08, 0x17,
};

static const char sha_data[] =
	"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

static u8 sha256_digest[] = {
	0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8,
	0xe5, 0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39,
	0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff, 0x21, 0x67,
	0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1,
};

static u8 sha384_digest[] = {
	0x33, 0x91, 0xfd, 0xdd, 0xfc, 0x8d, 0xc7, 0x39,
	0x37, 0x07, 0xa6, 0x5b, 0x1b, 0x47, 0x09, 0x39,
	0x7c, 0xf8, 0xb1, 0xd1, 0x62, 0xaf, 0x05, 0xab,
	0xfe, 0x8f, 0x45, 0x0d, 0xe5, 0xf3, 0x6b, 0xc6,
	0xb0, 0x45, 0x5a, 0x85, 0x20, 0xbc, 0x4e, 0x6f,
	0x5f, 0xe9, 0x5b, 0x1f, 0xe3, 0xc8, 0x45, 0x2b,
};

#define NAN_CRYPTO_FAIL(_ret)                                       \
	if (_ret) {                                                 \
		wpa_printf(MSG_ERROR,                               \
			   "NAN Crypto test fail: %s:%d",           \
			   __func__, __LINE__);                     \
		return -1;                                          \
	}


static int nan_test_crypto_key_mic(void)
{
	u8 digest[SHA384_MAC_LEN];
	u8 *data = nan_nonce[0];
	size_t data_len = sizeof(nan_nonce[0]);

	NAN_CRYPTO_FAIL(nan_crypto_key_mic(data, data_len, nan_key,
					   sizeof(nan_key), NAN_CS_SK_CCM_128,
					   digest));

	NAN_CRYPTO_FAIL(os_memcmp(digest, nan_crypto_sha256_digest,
				  NAN_KEY_MIC_LEN));

	os_memset(digest, 0, sizeof(digest));

	NAN_CRYPTO_FAIL(nan_crypto_key_mic(data, data_len, nan_key,
					   sizeof(nan_key), NAN_CS_SK_GCM_256,
					   digest));
	NAN_CRYPTO_FAIL(os_memcmp(digest, nan_crypto_sha384_digest,
				  NAN_KEY_MIC_24_LEN));

	return 0;
}


static int nan_test_crypto_pmkid(void)
{
	u8 pmkid[PMKID_LEN];

	NAN_CRYPTO_FAIL(nan_crypto_calc_pmkid(nan_key, nan_addrs[0],
					      nan_addrs[1], nan_service_id,
					      NAN_CS_SK_CCM_128, pmkid));

	NAN_CRYPTO_FAIL(os_memcmp(pmkid, nan_pmkid_128, sizeof(pmkid)));

	NAN_CRYPTO_FAIL(nan_crypto_calc_pmkid(nan_key, nan_addrs[0],
					      nan_addrs[1], nan_service_id,
					      NAN_CS_SK_GCM_256, pmkid));

	NAN_CRYPTO_FAIL(os_memcmp(pmkid, nan_pmkid_256, sizeof(pmkid)));

	return 0;
}


static int nan_test_ptk(void)
{
	struct nan_ptk tptk;

	os_memset(&tptk, 0, sizeof(tptk));

	NAN_CRYPTO_FAIL(nan_crypto_pmk_to_ptk(nan_key,
					      nan_addrs[0], nan_addrs[1],
					      nan_nonce[0], nan_nonce[1],
					      &tptk, NAN_CS_SK_CCM_128));

	NAN_CRYPTO_FAIL(os_memcmp(&tptk, &nan_ptk_128, sizeof(tptk)));

	os_memset(&tptk, 0, sizeof(tptk));

	NAN_CRYPTO_FAIL(nan_crypto_pmk_to_ptk(nan_key,
					      nan_addrs[0], nan_addrs[1],
					      nan_nonce[0], nan_nonce[1],
					      &tptk, NAN_CS_SK_GCM_256));

	NAN_CRYPTO_FAIL(os_memcmp(&tptk, &nan_ptk_256, sizeof(tptk)));

	return 0;
}


static int nan_test_crypto_auth_token(void)
{
	u8 digest[SHA384_MAC_LEN];
	const u8 *data = (const u8 *) sha_data;
	size_t len = os_strlen(sha_data);

	NAN_CRYPTO_FAIL(nan_crypto_calc_auth_token(NAN_CS_SK_CCM_128, data,
						   len, digest));
	NAN_CRYPTO_FAIL(os_memcmp(digest, sha256_digest, NAN_AUTH_TOKEN_LEN));

	os_memset(digest, 0, sizeof(digest));

	NAN_CRYPTO_FAIL(nan_crypto_calc_auth_token(NAN_CS_SK_GCM_256, data, len,
						   digest));
	NAN_CRYPTO_FAIL(os_memcmp(digest, sha384_digest, NAN_AUTH_TOKEN_LEN));

	return 0;
}


static int nan_test_derive_nd_pmk(void)
{
	u8 pmk[PMK_LEN];
	/* Wi-Fi Aware spec v4.0, Appendix M.1 - Test Vector 1 */
	const char *pwd = "NAN";
	const u8 service_id[NAN_SERVICE_ID_LEN] = {
		0x2b, 0x9c, 0x45, 0x0f, 0x66, 0x71,
	};
	const u8 nmi[ETH_ALEN] = {
		0x02, 0x90, 0x4c, 0x12, 0xd0, 0x01,
	};
	const u8 expected_pmk_ccm128[PMK_LEN] = {
		0xee, 0x35, 0x85, 0x06, 0x30, 0x56, 0xd1, 0x64,
		0xd1, 0x54, 0x54, 0xad, 0x39, 0x01, 0x0d, 0x4e,
		0x26, 0x40, 0xb0, 0xd8, 0x2f, 0xb2, 0x4a, 0x2d,
		0x68, 0x99, 0x86, 0x2d, 0x27, 0x3c, 0x68, 0xbf,
	};

	NAN_CRYPTO_FAIL(nan_crypto_derive_nd_pmk(pwd, service_id,
						 NAN_CS_SK_CCM_128, nmi, pmk));
	NAN_CRYPTO_FAIL(os_memcmp(pmk, expected_pmk_ccm128, PMK_LEN));

	return 0;
}

int nan_test_crypto(void)
{
	NAN_CRYPTO_FAIL(nan_test_crypto_key_mic());
	NAN_CRYPTO_FAIL(nan_test_crypto_pmkid());
	NAN_CRYPTO_FAIL(nan_test_ptk());
	NAN_CRYPTO_FAIL(nan_test_crypto_auth_token());
	NAN_CRYPTO_FAIL(nan_test_derive_nd_pmk());

	return 0;
}


int nan_module_tests(void)
{
	if (nan_test_crypto())
		return -1;

	return nan_test_run();
}
