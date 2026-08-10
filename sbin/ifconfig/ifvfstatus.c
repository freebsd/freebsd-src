/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Intel Corporation
 * Copyright (c) 2026 Kevin Bowling <kbowling@FreeBSD.org>
 */

#include <sys/types.h>

#include <net/ethernet.h>
#include <net/if.h>

#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>

#include "ifconfig.h"

static void
vf_group_begin(bool *printed, const char *name)
{

	if (!*printed) {
		printf("\t\t\t%s:", name);
		*printed = true;
	}
}

static void
vf_group_end(bool printed)
{

	if (printed)
		putchar('\n');
}

static void
vf_print_rate(const char *name, uint64_t rate, const char *zero)
{

	if (rate == 0)
		printf(" %s=%s", name, zero);
	else if (rate % IF_Mbps(1) == 0)
		printf(" %s=%" PRIu64 "Mbps", name, rate / IF_Mbps(1));
	else
		printf(" %s=%" PRIu64 "bps", name, rate);
}

static const char *
vf_link_state_name(enum ifconfig_vf_link_state state)
{

	switch (state) {
	case IFCONFIG_VF_LINK_DOWN:
		return ("down");
	case IFCONFIG_VF_LINK_UP:
		return ("up");
	case IFCONFIG_VF_LINK_AUTO:
		return ("auto");
	default:
		return ("unknown");
	}
}

static const char *
vf_vlan_mode_name(enum ifconfig_vf_vlan_mode mode)
{

	switch (mode) {
	case IFCONFIG_VF_VLAN_ACCESS:
		return ("access");
	case IFCONFIG_VF_VLAN_TRUNK:
		return ("trunk");
	default:
		return ("unknown");
	}
}

static void
vf_print_vlan_proto(uint16_t proto)
{

	switch (proto) {
	case ETHERTYPE_VLAN:
		printf("802.1q");
		break;
	case ETHERTYPE_QINQ:
		printf("802.1ad");
		break;
	default:
		printf("0x%04x", proto);
		break;
	}
}

static void
vf_driver_field(const struct ifconfig_vf_extension_field *field)
{
	const uint8_t *data;
	size_t i, length;

	printf(" %s=", field->name);
	switch (field->type) {
	case IFCONFIG_VF_EXT_BOOL:
		printf("%s", field->value.boolean ? "yes" : "no");
		break;
	case IFCONFIG_VF_EXT_NUMBER:
		printf("%" PRIu64, field->value.number);
		break;
	case IFCONFIG_VF_EXT_STRING:
		printf("%s", field->value.string);
		break;
	case IFCONFIG_VF_EXT_BINARY:
		data = field->value.binary.data;
		length = field->value.binary.length;
		printf("0x");
		for (i = 0; i < length; i++)
			printf("%02x", data[i]);
		break;
	default:
		break;
	}
}

static void
vf_driver_status(const struct ifconfig_vf_info *vf)
{
	const struct ifconfig_vf_extension *driver;
	const struct ifconfig_vf_extension_field *field;
	size_t i, j;

	for (i = 0; i < vf->num_extensions; i++) {
		driver = &vf->extensions[i];
		printf("\t\t\t%s: version=%u", driver->name,
		    driver->version);
		for (j = 0; j < driver->num_fields; j++) {
			field = &driver->fields[j];
			vf_driver_field(field);
		}
		putchar('\n');
	}
}

void
vf_status(if_ctx *ctx)
{
	struct ifconfig_vf_status *status;
	const struct ifconfig_vf_info *vf;
	const struct ether_addr *mac;
	const char *mode;
	uint64_t speed;
	bool printed;
	size_t i;
	int error;

	if (ifconfig_get_vf_status(lifh, ctx->ifname, &status) != 0) {
		error = ifconfig_err_errno(lifh);
		if (error != EOPNOTSUPP)
			warnc(error, "%s: VF status", ctx->ifname);
		return;
	}

	if (status->pf_link_state_present) {
		printf("\tVF-visible PF link: state=%s",
		    vf_link_state_name(status->pf_link_state));
	}
	speed = status->pf_link_speed;
	if (status->pf_link_speed_present) {
		if (speed % IF_Mbps(1) == 0)
			printf(" speed=%" PRIu64 "Mbps", speed / IF_Mbps(1));
		else
			printf(" speed=%" PRIu64 "bps", speed);
	}
	if (status->pf_link_state_present || status->pf_link_speed_present)
		putchar('\n');

	printf("\tvirtual functions: %zu\n", status->num_vfs);
	for (i = 0; i < status->num_vfs; i++) {
		vf = status->vfs[i];
		printf("\t\tvf %3u:\n", vf->index);

		printed = false;
		if ((vf->fields & (1ULL << IFLAF_VF_MAC)) != 0) {
			mac = (const struct ether_addr *)(const void *)vf->mac;
			vf_group_begin(&printed, "identity");
			printf(" mac=%s", ether_ntoa(mac));
		}
		vf_group_end(printed);

		printed = false;
		if ((vf->fields & (1ULL << IFLAF_VF_CONFIGURED)) != 0) {
			vf_group_begin(&printed, "state");
			printf(" configured=%s", vf->configured ? "yes" : "no");
		}
		if ((vf->fields & (1ULL << IFLAF_VF_INITIALIZED)) != 0) {
			vf_group_begin(&printed, "state");
			printf(" initialized=%s", vf->initialized ? "yes" : "no");
		}
		if ((vf->fields & (1ULL << IFLAF_VF_TRAFFIC_ALLOWED)) != 0) {
			vf_group_begin(&printed, "state");
			printf(" traffic=%s", vf->traffic_allowed ?
			    "allowed" : "denied");
		}
		if ((vf->fields & (1ULL << IFLAF_VF_FAULT_BLOCKED)) != 0) {
			vf_group_begin(&printed, "state");
			printf(" fault-blocked=%s", vf->fault_blocked ?
			    "yes" : "no");
		}
		if ((vf->fields & (1ULL << IFLAF_VF_QUARANTINED)) != 0) {
			vf_group_begin(&printed, "state");
			printf(" quarantined=%s", vf->quarantined ? "yes" : "no");
		}
		vf_group_end(printed);

		printed = false;
		if ((vf->fields & (1ULL << IFLAF_VF_NUM_TX_QUEUES)) != 0) {
			vf_group_begin(&printed, "resources");
			printf(" tx-queues=%u", vf->tx_queue_count);
		}
		if ((vf->fields & (1ULL << IFLAF_VF_NUM_RX_QUEUES)) != 0) {
			vf_group_begin(&printed, "resources");
			printf(" rx-queues=%u", vf->rx_queue_count);
		}
		if ((vf->fields & (1ULL << IFLAF_VF_MIN_TX_RATE)) != 0) {
			vf_group_begin(&printed, "resources");
			vf_print_rate("min-tx-rate", vf->min_tx_rate_bps, "none");
		}
		if ((vf->fields & (1ULL << IFLAF_VF_MAX_TX_RATE)) != 0) {
			vf_group_begin(&printed, "resources");
			vf_print_rate("max-tx-rate", vf->max_tx_rate_bps,
			    "unlimited");
		}
		vf_group_end(printed);

		printed = false;
		if ((vf->fields & (1ULL << IFLAF_VF_VLAN_MODE)) != 0) {
			mode = vf_vlan_mode_name(vf->vlan_mode);
			vf_group_begin(&printed, "vlan");
			printf(" mode=%s", mode);
			if (vf->vlan_mode == IFCONFIG_VF_VLAN_ACCESS &&
			    (vf->fields & (1ULL << IFLAF_VF_VLAN)) != 0)
				printf(" vid=%u", vf->vlan);
			if (vf->vlan_mode == IFCONFIG_VF_VLAN_ACCESS &&
			    (vf->fields & (1ULL << IFLAF_VF_VLAN_PCP)) != 0)
				printf(" pcp=%u", vf->vlan_pcp);
			if (vf->vlan_mode == IFCONFIG_VF_VLAN_ACCESS &&
			    (vf->fields & (1ULL << IFLAF_VF_VLAN_PROTO)) != 0) {
				printf(" proto=");
				vf_print_vlan_proto(vf->vlan_proto);
			}
		}
		if ((vf->fields & (1ULL << IFLAF_VF_VLAN_COUNT)) != 0) {
			vf_group_begin(&printed, "vlan");
			printf(" filters=%u", vf->vlan_count);
		}
		if ((vf->fields & (1ULL << IFLAF_VF_VLAN_LIMIT)) != 0) {
			vf_group_begin(&printed, "vlan");
			printf(" limit=%u", vf->vlan_limit);
		}
		vf_group_end(printed);

		printed = false;
		if ((vf->fields & (1ULL << IFLAF_VF_ALLOW_SET_MAC)) != 0) {
			vf_group_begin(&printed, "policy");
			printf(" set-mac=%s", vf->allow_set_mac ?
			    "allowed" : "denied");
		}
		if ((vf->fields & (1ULL << IFLAF_VF_ALLOW_SET_VLAN)) != 0) {
			vf_group_begin(&printed, "policy");
			printf(" set-vlan=%s", vf->allow_set_vlan ?
			    "allowed" : "denied");
		}
		if ((vf->fields & (1ULL << IFLAF_VF_MAC_ANTI_SPOOF)) != 0) {
			vf_group_begin(&printed, "policy");
			printf(" anti-spoof=%s", vf->mac_anti_spoof ? "on" : "off");
		}
		if ((vf->fields & (1ULL << IFLAF_VF_ALLOW_PROMISC)) != 0) {
			vf_group_begin(&printed, "policy");
			printf(" promisc=%s", vf->allow_promisc ?
			    "allowed" : "denied");
		}
		if ((vf->fields & (1ULL << IFLAF_VF_LINK_STATE_POLICY)) != 0) {
			vf_group_begin(&printed, "policy");
			printf(" link-state=%s",
			    vf_link_state_name(vf->link_state_policy));
		}
		vf_group_end(printed);

		printed = false;
		if ((vf->fields & (1ULL << IFLAF_VF_API_VERSION)) != 0) {
			vf_group_begin(&printed, "protocol");
			printf(" api=%s", vf->api_version);
		}
		vf_group_end(printed);

		vf_driver_status(vf);
	}
	ifconfig_free_vf_status(status);
}
