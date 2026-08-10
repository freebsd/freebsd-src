/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025, Muhammad Saheed <saheed@FreeBSD.org>
 */

#include <netlink/netlink.h>
#include <netlink/netlink_snl.h>
#include <netlink/netlink_snl_route_parsers.h>
#include <netlink/route/common.h>
#include <netlink/route/interface.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "libifconfig.h"
#include "libifconfig_internal.h"

static int ifconfig_modify_flags(ifconfig_handle_t *h, const char *ifname,
    int ifi_flags, int ifi_change);

struct ifconfig_vf_status_storage {
	/* The public object must remain first so free can recover this object. */
	struct ifconfig_vf_status public;
	struct ifconfig_vf_info *vf_info;
};

static int
ifconfig_modify_flags(ifconfig_handle_t *h, const char *ifname, int ifi_flags,
    int ifi_change)
{
	int ret = 0;
	struct snl_state ss;
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	struct ifinfomsg *ifi;
	struct snl_errmsg_data e = { 0 };

	if (!snl_init(&ss, NETLINK_ROUTE)) {
		ifconfig_error(h, NETLINK, ENOTSUP);
		return (-1);
	}

	snl_init_writer(&ss, &nw);
	hdr = snl_create_msg_request(&nw, NL_RTM_NEWLINK);
	ifi = snl_reserve_msg_object(&nw, struct ifinfomsg);
	snl_add_msg_attr_string(&nw, IFLA_IFNAME, ifname);

	ifi->ifi_flags = ifi_flags;
	ifi->ifi_change = ifi_change;

	hdr = snl_finalize_msg(&nw);
	if (hdr == NULL) {
		ifconfig_error(h, NETLINK, ENOMEM);
		ret = -1;
		goto out;
	}

	if (!snl_send_message(&ss, hdr)) {
		ifconfig_error(h, NETLINK, EIO);
		ret = -1;
		goto out;
	}

	if (!snl_read_reply_code(&ss, hdr->nlmsg_seq, &e)) {
		ifconfig_error(h, NETLINK, e.error);
		ret = -1;
		goto out;
	}

out:
	snl_free(&ss);
	return (ret);
}

int
ifconfig_set_up(ifconfig_handle_t *h, const char *ifname, bool up)
{
	int flag = up ? IFF_UP : ~IFF_UP;

	return (ifconfig_modify_flags(h, ifname, flag, IFF_UP));
}

void
ifconfig_free_vf_status(struct ifconfig_vf_status *status)
{
	struct ifconfig_vf_status_storage *storage;
	struct ifconfig_vf_extension_field *field;
	struct ifconfig_vf_extension *extension;
	struct ifconfig_vf_info *vf;
	size_t i, j, k;

	if (status == NULL)
		return;
	storage = (struct ifconfig_vf_status_storage *)(void *)status;
	for (i = 0; status->vfs != NULL && i < status->num_vfs; i++) {
		vf = status->vfs[i];
		if (vf == NULL)
			continue;
		free(vf->api_version);
		for (j = 0; j < vf->num_extensions; j++) {
			extension = &vf->extensions[j];
			free(extension->name);
			for (k = 0; k < extension->num_fields; k++) {
				field = &extension->fields[k];
				free(field->name);
				if (field->type == IFCONFIG_VF_EXT_STRING)
					free(field->value.string);
				else if (field->type == IFCONFIG_VF_EXT_BINARY)
					free(field->value.binary.data);
			}
			free(extension->fields);
		}
		free(vf->extensions);
	}
	free(storage->vf_info);
	free(status->vfs);
	free(storage);
}

static int
ifconfig_copy_vf_extension_field(struct ifconfig_vf_extension_field *dst,
    const struct snl_parsed_vf_driver_field *src)
{
	size_t length;

	dst->name = strdup(src->name);
	if (dst->name == NULL)
		return (ENOMEM);
	switch (src->type) {
	case SNL_VFDF_BOOL:
		dst->type = IFCONFIG_VF_EXT_BOOL;
		dst->value.boolean = src->boolean;
		break;
	case SNL_VFDF_NUMBER:
		dst->type = IFCONFIG_VF_EXT_NUMBER;
		dst->value.number = src->number;
		break;
	case SNL_VFDF_STRING:
		dst->type = IFCONFIG_VF_EXT_STRING;
		dst->value.string = strdup(src->string);
		if (dst->value.string == NULL)
			return (ENOMEM);
		break;
	case SNL_VFDF_BINARY:
		dst->type = IFCONFIG_VF_EXT_BINARY;
		length = NLA_DATA_LEN(src->binary);
		if (length == 0)
			return (EBADMSG);
		dst->value.binary.data = malloc(length);
		if (dst->value.binary.data == NULL)
			return (ENOMEM);
		memcpy(dst->value.binary.data, NLA_DATA(src->binary), length);
		dst->value.binary.length = length;
		break;
	default:
		return (EBADMSG);
	}
	return (0);
}

static int
ifconfig_copy_vf_extensions(struct ifconfig_vf_info *dst,
    const struct snl_parsed_vf *src)
{
	const struct snl_parsed_vf_driver_field *src_field;
	const struct snl_parsed_vf_driver *src_extension;
	struct ifconfig_vf_extension *extension;
	size_t i, j, num_extensions, num_fields;
	int error;

	num_extensions = src->drivers.count;
	if (num_extensions == 0)
		return (0);
	dst->extensions = calloc(num_extensions, sizeof(*dst->extensions));
	if (dst->extensions == NULL)
		return (ENOMEM);
	dst->num_extensions = num_extensions;
	for (i = 0; i < dst->num_extensions; i++) {
		src_extension = src->drivers.items[i];
		extension = &dst->extensions[i];
		extension->name = strdup(src_extension->name);
		if (extension->name == NULL)
			return (ENOMEM);
		extension->version = src_extension->version;
		num_fields = src_extension->fields.count;
		if (num_fields == 0)
			continue;
		extension->fields = calloc(num_fields,
		    sizeof(*extension->fields));
		if (extension->fields == NULL)
			return (ENOMEM);
		extension->num_fields = num_fields;
		for (j = 0; j < extension->num_fields; j++) {
			src_field = src_extension->fields.items[j];
			error = ifconfig_copy_vf_extension_field(
			    &extension->fields[j], src_field);
			if (error != 0)
				return (error);
		}
	}
	return (0);
}

static int
ifconfig_copy_vf(struct ifconfig_vf_info *dst,
    const struct snl_parsed_vf *src)
{

	dst->fields = src->attrs;
	dst->min_tx_rate_bps = src->min_tx_rate_bps;
	dst->max_tx_rate_bps = src->max_tx_rate_bps;
	dst->index = src->index;
	dst->vlan_count = src->vlan_count;
	dst->vlan_limit = src->vlan_limit;
	dst->tx_queue_count = src->tx_queue_count;
	dst->rx_queue_count = src->rx_queue_count;
	dst->vlan = src->vlan;
	dst->vlan_proto = src->vlan_proto;
	dst->vlan_pcp = src->vlan_pcp;
	if ((src->attrs & (1ULL << IFLAF_VF_MAC)) != 0) {
		if (NLA_DATA_LEN(src->mac) != sizeof(dst->mac))
			return (EBADMSG);
		memcpy(dst->mac, NLA_DATA(src->mac), sizeof(dst->mac));
	}
	dst->vlan_mode = (enum ifconfig_vf_vlan_mode)src->vlan_mode;
	dst->link_state_policy =
	    (enum ifconfig_vf_link_state)src->link_state_policy;
	dst->configured = src->configured;
	dst->initialized = src->initialized;
	dst->allow_set_mac = src->allow_set_mac;
	dst->allow_set_vlan = src->allow_set_vlan;
	dst->mac_anti_spoof = src->mac_anti_spoof;
	dst->allow_promisc = src->allow_promisc;
	dst->traffic_allowed = src->traffic_allowed;
	dst->fault_blocked = src->fault_blocked;
	dst->quarantined = src->quarantined;
	if ((src->attrs & (1ULL << IFLAF_VF_API_VERSION)) != 0) {
		dst->api_version = strdup(src->api_version);
		if (dst->api_version == NULL)
			return (ENOMEM);
	}
	return (ifconfig_copy_vf_extensions(dst, src));
}

int
ifconfig_get_vf_status(ifconfig_handle_t *h, const char *name,
    struct ifconfig_vf_status **statusp)
{
	struct ifconfig_vf_status_storage *storage;
	struct ifconfig_vf_status *status;
	struct snl_parsed_link link = {};
	struct snl_errmsg_data e = {};
	struct snl_state ss = {};
	struct snl_writer nw = {};
	struct nlmsghdr *hdr;
	ifconfig_errtype errtype;
	uint32_t seq;
	size_t i;
	int error;

	if (h == NULL || name == NULL || statusp == NULL) {
		if (h != NULL)
			ifconfig_error(h, OTHER, EINVAL);
		return (-1);
	}
	*statusp = NULL;
	if (strnlen(name, IFNAMSIZ) == IFNAMSIZ) {
		ifconfig_error(h, OTHER, ENAMETOOLONG);
		return (-1);
	}
	if (!snl_init(&ss, NETLINK_ROUTE)) {
		ifconfig_error(h, NETLINK, ENOTSUP);
		return (-1);
	}
	errtype = NETLINK;

	snl_init_writer(&ss, &nw);
	hdr = snl_create_msg_request(&nw, NL_RTM_GETLINK);
	if (hdr == NULL ||
	    snl_reserve_msg_object(&nw, struct ifinfomsg) == NULL ||
	    !snl_add_msg_attr_string(&nw, IFLA_IFNAME, name) ||
	    !snl_add_msg_attr_u32(&nw, IFLA_EXT_MASK, RTEXT_FILTER_VF) ||
	    (hdr = snl_finalize_msg(&nw)) == NULL) {
		error = ENOMEM;
		goto fail;
	}
	seq = hdr->nlmsg_seq;
	if (!snl_send_message(&ss, hdr)) {
		error = EIO;
		goto fail;
	}
	error = snl_grow_rxbuf_to_next_message(&ss);
	if (error != 0) {
		if (error == ENOMEM)
			errtype = OTHER;
		goto fail;
	}
	hdr = snl_read_reply(&ss, seq);
	if (hdr == NULL) {
		error = EIO;
		goto fail;
	}
	if (hdr->nlmsg_type == NLMSG_ERROR) {
		if (!snl_parse_errmsg(&ss, hdr, &e) || e.error == 0)
			error = EBADMSG;
		else
			error = e.error;
		goto fail;
	}
	if (hdr->nlmsg_type != NL_RTM_NEWLINK ||
	    !snl_parse_nlmsg(&ss, hdr, &snl_rtm_link_parser, &link)) {
		error = EBADMSG;
		goto fail;
	}
	if (!link.iflaf_vf_status.present) {
		error = EOPNOTSUPP;
		goto fail;
	}
	if (link.iflaf_vf_status.error != 0) {
		error = link.iflaf_vf_status.error;
		goto fail;
	}
	if (link.ifla_num_vf != link.iflaf_vf_status.vfs.count) {
		error = EBADMSG;
		goto fail;
	}

	storage = calloc(1, sizeof(*storage));
	if (storage == NULL) {
		errtype = OTHER;
		error = ENOMEM;
		goto fail;
	}
	status = &storage->public;
	status->pf_link_state = (enum ifconfig_vf_link_state)
	    link.iflaf_vf_status.pf_link_state;
	status->pf_link_state_present = true;
	status->pf_link_speed = link.iflaf_vf_status.pf_link_speed;
	status->pf_link_speed_present = status->pf_link_speed != 0;
	status->num_vfs = link.iflaf_vf_status.vfs.count;
	if (status->num_vfs != 0) {
		status->vfs = calloc(status->num_vfs, sizeof(*status->vfs));
		storage->vf_info = calloc(status->num_vfs,
		    sizeof(*storage->vf_info));
		if (status->vfs == NULL || storage->vf_info == NULL) {
			errtype = OTHER;
			error = ENOMEM;
			goto fail_status;
		}
		for (i = 0; i < status->num_vfs; i++)
			status->vfs[i] = &storage->vf_info[i];
	}
	for (i = 0; i < status->num_vfs; i++) {
		error = ifconfig_copy_vf(status->vfs[i],
		    link.iflaf_vf_status.vfs.items[i]);
		if (error != 0) {
			if (error == ENOMEM)
				errtype = OTHER;
			goto fail_status;
		}
	}
	ifconfig_error_clear(h);
	*statusp = status;
	snl_free(&ss);
	return (0);

fail_status:
	ifconfig_free_vf_status(status);
fail:
	ifconfig_error(h, errtype, error);
	snl_free(&ss);
	return (-1);
}
