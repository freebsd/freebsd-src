/*
 * Automated Frequency Coordination
 * Copyright (c) 2024, Lorenzo Bianconi <lorenzo@kernel.org>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include <sys/un.h>
#include <time.h>

#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/json.h"
#include "common/hw_features_common.h"
#include "common/wpa_ctrl.h"
#include "hostapd.h"
#include "acs.h"
#include "hw_features.h"

#define HOSTAPD_AFC_RETRY_TIMEOUT	180
#define HOSTAPD_AFC_TIMEOUT		86400 /* 24h */
#define HOSTAPD_AFC_BUFSIZE		8192

static void hostapd_afc_timeout_handler(void *eloop_ctx, void *timeout_ctx);


static int hostapd_afc_reset_channels(struct hostapd_iface *iface)
{
	int ret;

	ret = hostapd_get_hw_features(iface);
	if (ret) {
		wpa_printf(MSG_ERROR, "AFC: Failed to reset channel flags");
		return ret;
	}

	ret = hostapd_set_current_hw_info(iface, iface->freq);
	if (ret)
		wpa_printf(MSG_ERROR,
			   "AFC: Failed to set current hardware info");

	return ret;
}


static struct wpabuf *
hostapd_afc_build_location_request(struct hostapd_iface *iface)
{
	struct wpabuf *location_obj, *ellipse_obj = NULL;
	struct hostapd_config *iconf = iface->conf;
	bool is_ap_indoor = he_reg_is_indoor(iconf->he_6ghz_reg_pwr_type);
	size_t location_len = 1024, ellipse_len = 128;
	unsigned int i;

	location_obj = wpabuf_alloc(location_len);
	if (!location_obj)
		return NULL;

	json_start_object(location_obj, "location");
	if (iconf->afc.location.type != AFC_LINEAR_POLYGON) {
		struct afc_linear_polygon *lp =
			iconf->afc.location.linear_polygon_data;

		if (!lp)
			goto error;

		ellipse_obj = wpabuf_alloc(ellipse_len);
		if (!ellipse_obj)
			goto error;

		json_start_object(ellipse_obj, "center");

		json_add_double(ellipse_obj, "longitude", lp->longitude);
		json_value_sep(ellipse_obj);

		json_add_double(ellipse_obj, "latitude", lp->latitude);
		json_end_object(ellipse_obj);
		json_value_sep(ellipse_obj);
	}

	switch (iconf->afc.location.type) {
	case AFC_LINEAR_POLYGON:
		json_start_object(location_obj, "linearPolygon");
		json_start_array(location_obj, "outerBoundary");
		for (i = 0; i < iconf->afc.location.n_linear_polygon_data; i++)
		{
			struct afc_linear_polygon *lp =
				&iconf->afc.location.linear_polygon_data[i];

			json_start_object(location_obj, NULL);

			json_add_double(location_obj, "longitude",
					lp->longitude);
			json_value_sep(location_obj);

			json_add_double(location_obj, "latitude",
					lp->latitude);

			json_end_object(location_obj);
			if (i < iconf->afc.location.n_linear_polygon_data - 1)
				json_value_sep(location_obj);
		}
		json_end_array(location_obj);

		/* linearPolygon */
		json_end_object(location_obj);
		json_value_sep(location_obj);
		break;
	case AFC_RADIAL_POLYGON:
		json_start_object(location_obj, "radialPolygon");
		wpabuf_put_buf(location_obj, ellipse_obj);

		json_start_array(location_obj, "outerBoundary");
		for (i = 0; i < iconf->afc.location.n_radial_polygon_data; i++)
		{
			struct afc_radial_polygon *rp =
				&iconf->afc.location.radial_polygon_data[i];

			json_start_object(location_obj, NULL);
			json_add_double(location_obj, "length", rp->length);
			json_value_sep(location_obj);

			json_add_double(location_obj, "angle", rp->angle);
			json_end_object(location_obj);
			if (i < iconf->afc.location.n_radial_polygon_data - 1)
				json_value_sep(location_obj);
		}
		json_end_array(location_obj);

		/* radialPolygon */
		json_end_object(location_obj);
		json_value_sep(location_obj);
		break;
	case AFC_ELLIPSE:
	default:
		json_start_object(location_obj, "ellipse");
		wpabuf_put_buf(location_obj, ellipse_obj);

		json_add_int(location_obj, "majorAxis",
			     iconf->afc.location.major_axis);
		json_value_sep(location_obj);
		json_add_int(location_obj, "minorAxis",
			     iconf->afc.location.minor_axis);
		json_value_sep(location_obj);
		json_add_int(location_obj, "orientation",
			     iconf->afc.location.orientation);
		/* ellipse */
		json_end_object(location_obj);
		json_value_sep(location_obj);
		break;
	}

	json_start_object(location_obj, "elevation");

	json_add_double(location_obj, "height", iconf->afc.location.height);
	json_value_sep(location_obj);
	if (iconf->afc.location.height_type) {
		json_add_string(location_obj, "heightType",
				iconf->afc.location.height_type);
		json_value_sep(location_obj);
	}
	json_add_int(location_obj, "verticalUncertainty",
		     iconf->afc.location.vertical_tolerance);

	/* elevation */
	json_end_object(location_obj);
	json_value_sep(location_obj);

	json_add_int(location_obj, "indoorDeployment", is_ap_indoor);

	/* location */
	json_end_object(location_obj);
	json_value_sep(location_obj);

	wpabuf_free(ellipse_obj);
	return location_obj;

error:
	wpabuf_free(location_obj);
	wpabuf_free(ellipse_obj);

	return NULL;
}


static struct wpabuf * hostapd_afc_get_opclass_chan_list(u8 op_class)
{
	struct wpabuf *chan_list_obj;
	const struct oper_class_map *oper_class;
	int chan_offset, chan;
	size_t ch_len = 512;

	oper_class = get_oper_class(NULL, op_class);
	if (!oper_class)
		return NULL;

	chan_list_obj = wpabuf_alloc(ch_len);
	if (!chan_list_obj)
		return NULL;
	json_start_array(chan_list_obj, "channelCfi");

	switch (op_class) {
	case 132: /* 40 MHz */
		chan_offset = 2;
		break;
	case 133: /* 80 MHz */
		chan_offset = 6;
		break;
	case 134: /* 160 MHz */
		chan_offset = 14;
		break;
	case 137: /* 320 MHz */
		/*
		 * global_op_class already use the central channels for
		 * 320 MHz, so fallthrough and use 0 for chan_offset.
		 */
	default:
		chan_offset = 0;
		break;
	}

	for (chan = oper_class->min_chan; chan <= oper_class->max_chan;
	     chan += oper_class->inc) {
		char ch_str[32];

		if (chan + chan_offset > oper_class->max_chan)
			break;

		os_snprintf(ch_str, sizeof(ch_str), "%s%d",
			    chan != oper_class->min_chan ? ", " : "",
			    chan + chan_offset);
		wpabuf_put_str(chan_list_obj, ch_str);
	}
	json_end_array(chan_list_obj);

	return chan_list_obj;
}


static struct wpabuf *
hostapd_afc_build_req_chan_list(struct hostapd_iface *iface)
{
	struct wpabuf *op_class_list_obj;
	struct hostapd_config *iconf = iface->conf;
	unsigned int i, num;

	op_class_list_obj = wpabuf_alloc(HOSTAPD_AFC_BUFSIZE);
	if (!op_class_list_obj)
		return NULL;

	json_start_array(op_class_list_obj, "inquiredChannels");
	num = int_array_len(iconf->afc.op_class);
	for (i = 0; i < num; i++) {
		struct wpabuf *chan_list_obj = NULL;
		u8 op_class = iconf->afc.op_class[i];

		if (!is_6ghz_op_class(op_class))
			continue;

		json_start_object(op_class_list_obj, NULL);

		json_add_int(op_class_list_obj, "globalOperatingClass",
			     op_class);
		json_value_sep(op_class_list_obj);

		chan_list_obj = hostapd_afc_get_opclass_chan_list(op_class);
		if (!chan_list_obj)
			goto error;

		wpabuf_put_buf(op_class_list_obj, chan_list_obj);
		json_end_object(op_class_list_obj);

		if (i < num - 1)
			json_value_sep(op_class_list_obj);

		wpabuf_free(chan_list_obj);
		chan_list_obj = NULL;
	}
	json_end_array(op_class_list_obj);

	return op_class_list_obj;

error:
	wpabuf_free(op_class_list_obj);
	return NULL;
}


static struct wpabuf *
hostapd_afc_build_request(struct hostapd_iface *iface)
{
	struct wpabuf *req_obj, *location_obj, *op_class_list_obj = NULL;
	struct hostapd_config *iconf = iface->conf;
	unsigned int i;
	char request_id_str[16];

	req_obj = wpabuf_alloc(HOSTAPD_AFC_BUFSIZE);
	if (!req_obj)
		return NULL;

	json_start_object(req_obj, NULL);

	if (iconf->afc.request.version) {
		json_add_string(req_obj, "version", iconf->afc.request.version);
		json_value_sep(req_obj);
	}

	json_start_array(req_obj, "availableSpectrumInquiryRequests");
	json_start_object(req_obj, NULL);

	iface->afc.request_id++;
	os_snprintf(request_id_str, sizeof(request_id_str), "%u",
		    iface->afc.request_id);
	json_add_string(req_obj, "requestId", request_id_str);
	json_value_sep(req_obj);

	json_start_object(req_obj, "deviceDescriptor");
	if (iconf->afc.request.sn) {
		json_add_string(req_obj, "serialNumber", iconf->afc.request.sn);
		json_value_sep(req_obj);
	}

	json_start_array(req_obj, "certificationId");

	for (i = 0; i < iconf->afc.n_cert_ids; i++) {
		json_start_object(req_obj, NULL);
		json_add_string(req_obj, "rulesetId",
				iconf->afc.cert_ids[i].ruleset);
		json_value_sep(req_obj);

		json_add_string(req_obj, "id", iconf->afc.cert_ids[i].id);
		json_end_object(req_obj);

		if (i < iconf->afc.n_cert_ids - 1)
			json_value_sep(req_obj);
	}
	/* certificationId */
	json_end_array(req_obj);

	/* deviceDescriptor */
	json_end_object(req_obj);
	json_value_sep(req_obj);

	location_obj = hostapd_afc_build_location_request(iface);
	if (!location_obj)
		goto error;

	wpabuf_put_buf(req_obj, location_obj);

	if (iconf->afc.freqs.num) {
		json_start_array(req_obj, "inquiredFrequencyRange");
		for (i = 0; i < iconf->afc.freqs.num; i++) {
			struct wpa_freq_range *fr = &iconf->afc.freqs.range[i];

			json_start_object(req_obj, NULL);

			json_add_int(req_obj, "lowFrequency", fr->min);
			json_value_sep(req_obj);
			json_add_int(req_obj, "highFrequency", fr->max);

			json_end_object(req_obj);
			if (i < iconf->afc.freqs.num - 1)
				json_value_sep(req_obj);
		}
		json_end_array(req_obj);
		json_value_sep(req_obj);
	}

	op_class_list_obj = hostapd_afc_build_req_chan_list(iface);
	if (!op_class_list_obj)
		goto error;

	wpabuf_put_buf(req_obj, op_class_list_obj);

	if (iconf->afc.min_power) {
		json_value_sep(req_obj);
		json_add_int(req_obj, "minDesiredPower", iconf->afc.min_power);
	}

	/* availableSpectrumInquiryRequests */
	json_end_object(req_obj);
	json_end_array(req_obj);

	json_end_object(req_obj);
	wpa_hexdump_ascii_buf(MSG_MSGDUMP, "AFC: Pending request", req_obj);

	wpabuf_free(location_obj);
	wpabuf_free(op_class_list_obj);

	return req_obj;

error:
	wpabuf_free(req_obj);
	wpabuf_free(location_obj);
	wpabuf_free(op_class_list_obj);

	return NULL;
}


static int
hostad_afc_parse_available_freq_info(struct hostapd_iface *iface,
				     struct json_token *freq_info)
{
	struct afc_freq_range_elem *f = NULL, *tmp;
	struct json_token *freq_obj;
	int count = 0;

	for (freq_obj = freq_info->child; freq_obj;
	     freq_obj = freq_obj->sibling) {
		struct json_token *freq_range_obj, *token;
		int low_freq, high_freq;
		double max_psd;

		freq_range_obj = json_get_member(freq_obj, "frequencyRange");
		if (!freq_range_obj || freq_range_obj->type != JSON_OBJECT)
			continue;

		token = json_get_member(freq_range_obj, "lowFrequency");
		if (!token || token->type != JSON_NUMBER)
			continue;
		low_freq = token->number;

		token = json_get_member(freq_range_obj, "highFrequency");
		if (!token || token->type != JSON_NUMBER)
			continue;
		high_freq = token->number;

		token = json_get_member(freq_obj, "maxPsd");
		if (!token)
			token = json_get_member(freq_obj, "maxPSD");
		if (!token || (token->type != JSON_DOUBLE &&
			       token->type != JSON_NUMBER))
			continue;
		max_psd = token->type == JSON_DOUBLE ?
			token->dnumber : (double) token->number;

		tmp = os_realloc_array(f, count + 1, sizeof(*f));
		if (!tmp) {
			os_free(f);
			return -ENOMEM;
		}
		f = tmp;

		f[count].low_freq = low_freq;
		f[count].high_freq = high_freq;
		f[count++].max_psd = max_psd;
	}
	os_free(iface->afc.freq_range);
	iface->afc.freq_range = f;
	iface->afc.num_freq_range = count;

	return 0;
}


static int hostad_afc_update_chan_info(struct afc_chan_info_elem **chan_list,
				       int *chan_list_size, u8 op_class,
				       int center_chan, double power)
{
	int op_class_pwr_index, num_low_subchan, channel;
	int count = *chan_list_size;
	struct afc_chan_info_elem *c = *chan_list, *tmp;

	switch (op_class) {
	case 132: /* 40 MHz */
		op_class_pwr_index = 1;
		num_low_subchan = 2;
		break;
	case 133: /* 80 MHz */
		op_class_pwr_index = 2;
		num_low_subchan = 6;
		break;
	case 134: /* 160 MHz */
		op_class_pwr_index = 3;
		num_low_subchan = 14;
		break;
	case 137: /* 320 MHz */
		op_class_pwr_index = 4;
		num_low_subchan = 30;
		break;
	default:
		op_class_pwr_index = 0;
		num_low_subchan = 0;
		break;
	}

	for (channel = center_chan - num_low_subchan;
	     channel <= center_chan + num_low_subchan; channel += 4) {
		int i;

		for (i = 0; i < count; i++) {
			if (c[i].chan == channel)
				break;
		}

		if (i == count) {
			tmp = os_realloc_array(c, count + 1, sizeof(*c));
			if (!tmp) {
				os_free(c);
				*chan_list = NULL;
				*chan_list_size = 0;
				return -ENOMEM;
			}
			c = tmp;

			c[count].chan = channel;
			os_memset(c[count].power, 0, sizeof(c[count].power));
			count++;
		}
		c[i].power[op_class_pwr_index] = power;
	}

	*chan_list_size = count;
	*chan_list = c;

	return 0;
}


static int
hostad_afc_parse_available_chan_info(struct hostapd_iface *iface,
				     struct json_token *chan_info)
{
	struct afc_chan_info_elem *c = NULL;
	struct json_token *chan_obj;
	int count = 0;

	for (chan_obj = chan_info->child; chan_obj;
	     chan_obj = chan_obj->sibling) {
		struct json_token *chan_cfi, *max_eirp;
		struct json_token *token, *chan_arr, *power_arr;
		int op_class;

		token = json_get_member(chan_obj, "globalOperatingClass");
		if (!token || token->type != JSON_NUMBER)
			continue;
		op_class = token->number;

		chan_cfi = json_get_member(chan_obj, "channelCfi");
		if (!chan_cfi || chan_cfi->type != JSON_ARRAY)
			continue;

		max_eirp = json_get_member(chan_obj, "maxEirp");
		if (!max_eirp || max_eirp->type != JSON_ARRAY)
			continue;

		chan_arr = chan_cfi->child;
		power_arr = max_eirp->child;
		while (chan_arr && power_arr) {
			int channel, ret;
			double power;

			if (chan_arr->type != JSON_NUMBER ||
			    (power_arr->type != JSON_DOUBLE &&
			     power_arr->type != JSON_NUMBER)) {
				wpa_printf(MSG_DEBUG,
					   "AFC: Failed to parse array at opclass %d",
					   op_class);
				break;
			}

			channel = chan_arr->number;
			power = power_arr->type == JSON_DOUBLE ?
				power_arr->dnumber : (double) power_arr->number;

			ret = hostad_afc_update_chan_info(&c, &count, op_class,
							  channel, power);
			if (ret) {
				iface->afc.chan_info_list = NULL;
				iface->afc.num_chan_info = 0;
				return ret;
			}
			chan_arr = chan_arr->sibling;
			power_arr = power_arr->sibling;
		}
	}
	os_free(iface->afc.chan_info_list);
	iface->afc.chan_info_list = c;
	iface->afc.num_chan_info = count;

	return 0;
}


static int hostad_afc_get_timeout(struct json_token *obj)
{
	int year, month, day, hour, min, sec;
	os_time_t t;
	struct os_time now;

	if (sscanf(obj->string, "%d-%d-%dT%d:%d:%dZ",
		   &year, &month, &day, &hour, &min, &sec) != 6)
		return HOSTAPD_AFC_TIMEOUT;

	if (os_mktime(year, month, day, hour, min, sec, &t) < 0)
		return HOSTAPD_AFC_TIMEOUT;

	os_get_time(&now);

	return t < now.sec ?
		HOSTAPD_AFC_RETRY_TIMEOUT : (t - now.sec) * 80 / 100;
}


static int
hostapd_afc_parse_reply(struct hostapd_iface *iface, char *reply, size_t len)
{
	struct hostapd_config *iconf = iface->conf;
	int request_timeout = -1, ret = -EINVAL;
	struct json_token *root = NULL, *reply_obj, *token;

	wpa_hexdump_ascii(MSG_MSGDUMP, "AFC: Received reply", reply, len);

	iface->afc_response = os_malloc(len + 1);
	if (!iface->afc_response) {
		wpa_printf(MSG_ERROR,
			   "AFC: Failed to alloc memory for storing reply");
		return -ENOMEM;
	}
	os_memcpy(iface->afc_response, reply, len);
	iface->afc_response[len] = '\0';

	root = json_parse(reply, len);
	if (!root) {
		wpa_printf(MSG_ERROR, "AFC: Failed to parse reply payload");
		goto fail;
	}

	token = json_get_member(root, "version");
	if (!token || token->type != JSON_STRING) {
		wpa_printf(MSG_ERROR, "AFC: Missing version in reply");
		goto fail;
	}
	if (iconf->afc.request.version &&
	    os_strcmp(iconf->afc.request.version, token->string) != 0) {
		wpa_printf(MSG_ERROR, "AFC: Mismatch in reply version");
		goto fail;
	}

	reply_obj = json_get_member(root, "availableSpectrumInquiryResponses");
	if (!reply_obj || reply_obj->type != JSON_ARRAY) {
		wpa_printf(MSG_ERROR,
			   "AFC: Missing availableSpectrumInquiry in reply");
		goto fail;
	}

	for (token = reply_obj->child; token; token = token->sibling) {
		struct json_token *reply_elem, *response;
		int status = -EINVAL, timeout;
		unsigned int j;
		char request_id_str[16];

		reply_elem = json_get_member(token, "requestId");
		if (!reply_elem || reply_elem->type != JSON_STRING) {
			wpa_printf(MSG_DEBUG,
				   "AFC: Missing requestId in reply element");
			continue;
		}

		snprintf(request_id_str, sizeof(request_id_str), "%u",
			 iface->afc.request_id);
		if (os_strcmp(request_id_str, reply_elem->string) != 0) {
			wpa_printf(MSG_DEBUG,
				   "AFC: RequestId mismatch in reply element");
			continue;
		}

		reply_elem = json_get_member(token, "rulesetId");
		if (!reply_elem || reply_elem->type != JSON_STRING) {
			wpa_printf(MSG_DEBUG,
				   "AFC: Missing rulesetId in reply element");
			continue;
		}

		for (j = 0; j < iconf->afc.n_cert_ids; j++) {
			if (os_strcmp(iconf->afc.cert_ids[j].ruleset,
				      reply_elem->string) == 0)
				break;
		}

		if (j == iconf->afc.n_cert_ids) {
			wpa_printf(MSG_DEBUG,
				   "AFC: RulesetId mismatch in reply element");
			continue;
		}

		response = json_get_member(token, "response");
		if (!response || response->type != JSON_OBJECT) {
			wpa_printf(MSG_DEBUG,
				   "AFC: Missing response field in reply element");
			continue;
		}

		reply_elem = json_get_member(response, "shortDescription");
		if (reply_elem && reply_elem->type == JSON_STRING)
			wpa_printf(MSG_DEBUG, "AFC: Reply element: %s",
				   reply_elem->string);

		reply_elem = json_get_member(response, "responseCode");
		if (reply_elem && reply_elem->type == JSON_NUMBER)
			status = reply_elem->number;

		if (status < 0) {
			wpa_printf(MSG_DEBUG,
				   "AFC: Reply invalid responseCode: %d",
				   status);
			continue;
		}
		reply_elem = json_get_member(token, "availableFrequencyInfo");
		if (reply_elem && reply_elem->type == JSON_ARRAY &&
		    hostad_afc_parse_available_freq_info(iface, reply_elem)) {
			wpa_printf(MSG_DEBUG,
				   "AFC: Failed to parse availableFrequencyInfo");
			continue;
		}

		reply_elem = json_get_member(token, "availableChannelInfo");
		if (reply_elem && reply_elem->type == JSON_ARRAY &&
		    hostad_afc_parse_available_chan_info(iface, reply_elem)) {
			wpa_printf(MSG_DEBUG,
				   "AFC: Failed to parse availableChannelInfo");
			continue;
		}

		reply_elem = json_get_member(token, "availabilityExpireTime");
		if (!reply_elem || reply_elem->type != JSON_STRING) {
			wpa_printf(MSG_DEBUG,
				   "AFC: Missing expire time, use default timeout");
			continue;
		}

		timeout = hostad_afc_get_timeout(reply_elem);
		if (request_timeout < 0 || timeout < request_timeout)
			request_timeout = timeout;

		ret = status;
	}

	iface->afc.data_valid = true;
	iface->afc.timeout = request_timeout;
	if (iface->afc.timeout < 0)
		iface->afc.timeout = HOSTAPD_AFC_RETRY_TIMEOUT;

fail:
	json_free(root);
	return ret;
}


static int hostapd_afc_send_receive(struct hostapd_iface *iface)
{
	struct hostapd_config *iconf = iface->conf;
	struct wpabuf *request_obj = NULL;
	struct timeval sock_timeout = {
		.tv_sec = 10,
	};
	struct sockaddr_un addr = {
		.sun_family = AF_UNIX,
#ifdef __FreeBSD__
		.sun_len = sizeof(addr),
#endif /* __FreeBSD__ */
	};
	char *buf = NULL;
	int sockfd, ret;
	fd_set read_set;

	if (iface->afc.data_valid) {
		/* AFC data already downloaded from the server */
		return 0;
	}

	iface->afc.timeout = HOSTAPD_AFC_RETRY_TIMEOUT;
	if (!iconf->afc.socket) {
		wpa_printf(MSG_ERROR, "AFC: Missing socket string");
		return -EINVAL;
	}

	if (os_strlen(iconf->afc.socket) >= sizeof(addr.sun_path)) {
		wpa_printf(MSG_ERROR, "AFC: Malformed socket string %s",
			   iconf->afc.socket);
		return -EINVAL;
	}

	os_strlcpy(addr.sun_path, iconf->afc.socket, sizeof(addr.sun_path));
	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		wpa_printf(MSG_ERROR, "AFC: Failed creating socket");
		return sockfd;
	}

	if (connect(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		wpa_printf(MSG_ERROR, "AFC: Failed connecting socket");
		ret = -EIO;
		goto close_sock;
	}

	request_obj = hostapd_afc_build_request(iface);
	if (!request_obj) {
		ret = -EINVAL;
		goto close_sock;
	}

	ret = send(sockfd, wpabuf_head(request_obj), wpabuf_len(request_obj),
		   0);
	if (ret < 0 || (size_t) ret != wpabuf_len(request_obj)) {
		wpa_printf(MSG_ERROR, "AFC: Failed sending request");
		ret = -EIO;
		goto close_sock;
	}

	FD_ZERO(&read_set);
	FD_SET(sockfd, &read_set);
	if (select(sockfd + 1, &read_set, NULL, NULL, &sock_timeout) < 0) {
		wpa_printf(MSG_ERROR, "AFC: select() failed on socket");
		ret = -errno;
		goto close_sock;
	}

	if (!FD_ISSET(sockfd, &read_set)) {
		ret = -EIO;
		goto close_sock;
	}

	buf = os_zalloc(HOSTAPD_AFC_BUFSIZE);
	if (!buf) {
		ret = -ENOMEM;
		goto close_sock;
	}

	ret = recv(sockfd, buf, HOSTAPD_AFC_BUFSIZE - 1, 0);
	if (!ret)
		ret = -EIO;
	if (ret < 0)
		goto close_sock;

	ret = hostapd_afc_parse_reply(iface, buf, ret);
	if (ret) {
		wpa_printf(MSG_ERROR, "AFC: Failed parsing reply: %d", ret);
		goto close_sock;
	}
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, AFC_EVENT_RECEIVED);
close_sock:
	os_free(buf);
	wpabuf_free(iface->afc_request);
	iface->afc_request = request_obj;
	close(sockfd);

	return ret;
}


static bool hostapd_afc_has_usable_chans(struct hostapd_iface *iface)
{
	const struct oper_class_map *oper_class;
	int channel, chan_num;

	oper_class = get_oper_class(NULL, iface->conf->op_class);
	if (!oper_class)
		return false;

	switch (iface->conf->op_class) {
	case 132: /* 40 MHz */
		chan_num = 2;
		break;
	case 133: /* 80 MHz */
		chan_num = 4;
		break;
	case 134: /* 160 MHz */
		chan_num = 8;
		break;
	case 137: /* 320 MHz */
		chan_num = 16;
		break;
	default:
		chan_num = 1;
		break;
	}

	for (channel = oper_class->min_chan; channel <= oper_class->max_chan;
	     channel += oper_class->inc) {
		struct hostapd_hw_modes *mode = iface->current_mode;
		struct hostapd_channel_data *chan = NULL;
		int i, start_ch = channel;

		if (iface->conf->op_class == 137)
			start_ch = channel - 30;

		for (i = 0; i < chan_num; i++) {
			chan = hw_get_channel_chan(mode, start_ch + i * 4,
						   NULL);

			if (!chan || chan->flag & HOSTAPD_CHAN_DISABLED)
				break;
		}

		if (i == chan_num)
			return true;
	}

	return false;
}


int hostapd_afc_handle_request(struct hostapd_iface *iface)
{
	struct hostapd_config *iconf = iface->conf;
	int ret;

	/* AFC is required just for standard power AP */
	if (!he_reg_is_sp(iconf->he_6ghz_reg_pwr_type))
		return 1;

	if (!is_6ghz_op_class(iconf->op_class) || !is_6ghz_freq(iface->freq))
		return 1;

	if (iface->state == HAPD_IFACE_ACS)
		return 1;

	iface->afc.request_id = os_random();
	ret = hostapd_afc_send_receive(iface);
	if (ret < 0) {
		/*
		 * If the connection to the AFCD failed, reschedule for a
		 * future attempt.
		 */
		wpa_printf(MSG_ERROR, "AFC: Connection failed: %d", ret);
		if (ret == -EIO)
			ret = 0;
		goto resched;
	}

	hostap_afc_disable_channels(iface);
	if (!hostapd_afc_has_usable_chans(iface))
		goto resched;

	ret = hostapd_is_usable_chans(iface);
	if (ret != 1) {
		/* Trigger an ACS freq scan */
		iconf->channel = 0;
		iface->freq = 0;
		hostapd_set_and_check_bw320_offset(iface->conf, 0);

		if (!ret && acs_init(iface) != HOSTAPD_CHAN_ACS) {
			wpa_printf(MSG_ERROR, "AFC: Could not start ACS");
			ret = -EINVAL;
		}
	}

resched:
	eloop_cancel_timeout(hostapd_afc_timeout_handler, iface, NULL);
	eloop_register_timeout(iface->afc.timeout, 0,
			       hostapd_afc_timeout_handler, iface, NULL);

	return ret;
}


static void hostapd_afc_delete_data_from_server(struct hostapd_iface *iface)
{
	wpabuf_free(iface->afc_request);
	os_free(iface->afc_response);
	os_free(iface->afc.chan_info_list);
	os_free(iface->afc.freq_range);

	iface->afc_request = NULL;
	iface->afc_response = NULL;

	iface->afc.num_freq_range = 0;
	iface->afc.num_chan_info = 0;

	iface->afc.chan_info_list = NULL;
	iface->afc.freq_range = NULL;

	iface->afc.data_valid = false;
}


static void hostapd_afc_timeout_handler(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_iface *iface = eloop_ctx;
	bool restart_iface = true;

	hostapd_afc_delete_data_from_server(iface);
	if (iface->state != HAPD_IFACE_ENABLED) {
		/* hostapd is not fully enabled yet, toggle the interface */
		goto restart_interface;
	}

	if (hostapd_afc_send_receive(iface) < 0 ||
	    hostapd_afc_reset_channels(iface)) {
		restart_iface = false;
		goto restart_interface;
	}

	if (hostapd_is_usable_chans(iface) > 0) {
		wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, AFC_EVENT_COMPLETE);
		goto resched;
	}

	restart_iface = hostapd_afc_has_usable_chans(iface);
	if (restart_iface) {
		/* Trigger an ACS freq scan */
		iface->conf->channel = 0;
		iface->freq = 0;
		hostapd_set_and_check_bw320_offset(iface->conf, 0);
	} else {
		wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, AFC_EVENT_COMPLETE);
	}

restart_interface:
	hostapd_disable_iface(iface);
	if (restart_iface)
		hostapd_enable_iface(iface);
resched:
	eloop_register_timeout(iface->afc.timeout, 0,
			       hostapd_afc_timeout_handler, iface, NULL);
}


void hostapd_afc_send_request(struct hostapd_iface *iface)
{
	eloop_cancel_timeout(hostapd_afc_timeout_handler, iface, NULL);
	eloop_register_timeout(0, 0, hostapd_afc_timeout_handler, iface, NULL);
}


void hostapd_afc_stop(struct hostapd_iface *iface)
{
	eloop_cancel_timeout(hostapd_afc_timeout_handler, iface, NULL);
}


void hostap_afc_disable_channels(struct hostapd_iface *iface)
{
	struct hostapd_hw_modes *mode = NULL;
	int i;

	for (i = 0; i < iface->num_hw_features; i++) {
		mode = &iface->hw_features[i];
		if (mode->mode == HOSTAPD_MODE_IEEE80211A &&
		    mode->is_6ghz)
			break;
	}

	if (i == iface->num_hw_features ||
	    !he_reg_is_sp(iface->conf->he_6ghz_reg_pwr_type) ||
	    !iface->afc.data_valid)
		return;

	for (i = 0; i < mode->num_channels; i++) {
		struct hostapd_channel_data *chan = &mode->channels[i];
		unsigned int j;

		if (!is_6ghz_freq(chan->freq))
			continue;

		for (j = 0; j < iface->afc.num_freq_range; j++) {
			if (chan->freq >= iface->afc.freq_range[j].low_freq &&
			    chan->freq <= iface->afc.freq_range[j].high_freq)
				break;
		}

		if (j != iface->afc.num_freq_range)
			continue;

		for (j = 0; j < iface->afc.num_chan_info; j++) {
			if (chan->chan == iface->afc.chan_info_list[j].chan)
				break;
		}

		if (j != iface->afc.num_chan_info)
			continue;

		chan->flag |= HOSTAPD_CHAN_DISABLED;
		wpa_printf(MSG_MSGDUMP,
			   "AFC: Disabling freq=%d MHz (not allowed by AFC)",
			   chan->freq);
	}
}


int hostap_afc_get_chan_max_eirp_power(struct hostapd_iface *iface, bool psd,
				       int *power)
{
	unsigned int i;

	if (!he_reg_is_sp(iface->conf->he_6ghz_reg_pwr_type) ||
	    !iface->afc.data_valid)
		return -EINVAL;

	if (psd) {
		for (i = 0; i < iface->afc.num_freq_range; i++) {
			struct afc_freq_range_elem *f;

			f = &iface->afc.freq_range[i];
			if (iface->freq >= f->low_freq &&
			    iface->freq <= f->high_freq) {
				*power = (int) (2 * f->max_psd);
				return 0;
			}
		}
	} else {
		for (i = 0; i < iface->afc.num_chan_info; i++) {
			struct afc_chan_info_elem *c;

			c = &iface->afc.chan_info_list[i];
			if (c->chan == iface->conf->channel) {
				unsigned int j;

				*power = 0;
				for (j = 0; j < ARRAY_SIZE(c->power); j++)
					*power = MAX(*power,
						     (int) (2 * c->power[j]));
				return 0;
			}
		}
	}

	return -EINVAL;
}
