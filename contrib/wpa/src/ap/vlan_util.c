/*
 * hostapd / VLAN netlink api
 * Copyright (c) 2012, Michael Braun <michael-dev@fami-braun.de>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <linux/if_vlan.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/route/link.h>
#include <netlink/route/link/vlan.h>

#include "utils/common.h"
#include "utils/eloop.h"
#include "hostapd.h"
#include "vlan_util.h"

/*
 * Add a vlan interface with name 'vlan_if_name', VLAN ID 'vid' and
 * tagged interface 'if_name'.
 *
 * returns -1 on error
 * returns 1 if the interface already exists
 * returns 0 otherwise
*/
int vlan_add(const char *if_name, int vid, const char *vlan_if_name)
{
	int err, ret = -1;
	struct nl_sock *handle = NULL;
	struct nl_cache *cache = NULL;
	struct rtnl_link *rlink = NULL;
	int if_idx = 0;

	wpa_printf(MSG_DEBUG, "VLAN: vlan_add(if_name=%s, vid=%d, "
		   "vlan_if_name=%s)", if_name, vid, vlan_if_name);

	if ((os_strlen(if_name) + 1) > IFNAMSIZ) {
		wpa_printf(MSG_ERROR, "VLAN: Interface name too long: '%s'",
			   if_name);
		return -1;
	}

	if ((os_strlen(vlan_if_name) + 1) > IFNAMSIZ) {
		wpa_printf(MSG_ERROR, "VLAN: Interface name too long: '%s'",
			   vlan_if_name);
		return -1;
	}

	handle = nl_socket_alloc();
	if (!handle) {
		wpa_printf(MSG_ERROR, "VLAN: failed to open netlink socket");
		goto vlan_add_error;
	}

	err = nl_connect(handle, NETLINK_ROUTE);
	if (err < 0) {
		wpa_printf(MSG_ERROR, "VLAN: failed to connect to netlink: %s",
			   nl_geterror(err));
		goto vlan_add_error;
	}

	err = rtnl_link_alloc_cache(handle, AF_UNSPEC, &cache);
	if (err < 0) {
		cache = NULL;
		wpa_printf(MSG_ERROR, "VLAN: failed to alloc cache: %s",
			   nl_geterror(err));
		goto vlan_add_error;
	}

	if (!(if_idx = rtnl_link_name2i(cache, if_name))) {
		/* link does not exist */
		wpa_printf(MSG_ERROR, "VLAN: interface %s does not exist",
			   if_name);
		goto vlan_add_error;
	}

	if ((rlink = rtnl_link_get_by_name(cache, vlan_if_name))) {
		/* link does exist */
		rtnl_link_put(rlink);
		rlink = NULL;
		wpa_printf(MSG_ERROR, "VLAN: interface %s already exists",
			   vlan_if_name);
		ret = 1;
		goto vlan_add_error;
	}

	rlink = rtnl_link_alloc();
	if (!rlink) {
		wpa_printf(MSG_ERROR, "VLAN: failed to allocate new link");
		goto vlan_add_error;
	}

	err = rtnl_link_set_type(rlink, "vlan");
	if (err < 0) {
		wpa_printf(MSG_ERROR, "VLAN: failed to set link type: %s",
			   nl_geterror(err));
		goto vlan_add_error;
	}

	rtnl_link_set_link(rlink, if_idx);
	rtnl_link_set_name(rlink, vlan_if_name);

	err = rtnl_link_vlan_set_id(rlink, vid);
	if (err < 0) {
		wpa_printf(MSG_ERROR, "VLAN: failed to set link vlan id: %s",
			   nl_geterror(err));
		goto vlan_add_error;
	}

	err = rtnl_link_add(handle, rlink, NLM_F_CREATE);
	if (err < 0) {
		wpa_printf(MSG_ERROR, "VLAN: failed to create link %s for "
			   "vlan %d on %s (%d): %s",
			   vlan_if_name, vid, if_name, if_idx,
			   nl_geterror(err));
		goto vlan_add_error;
	}

	ret = 0;

vlan_add_error:
	if (rlink)
		rtnl_link_put(rlink);
	if (cache)
		nl_cache_free(cache);
	if (handle)
		nl_socket_free(handle);
	return ret;
}


int vlan_rem(const char *if_name)
{
	int err, ret = -1;
	struct nl_sock *handle = NULL;
	struct nl_cache *cache = NULL;
	struct rtnl_link *rlink = NULL;

	wpa_printf(MSG_DEBUG, "VLAN: vlan_rem(if_name=%s)", if_name);

	handle = nl_socket_alloc();
	if (!handle) {
		wpa_printf(MSG_ERROR, "VLAN: failed to open netlink socket");
		goto vlan_rem_error;
	}

	err = nl_connect(handle, NETLINK_ROUTE);
	if (err < 0) {
		wpa_printf(MSG_ERROR, "VLAN: failed to connect to netlink: %s",
			   nl_geterror(err));
		goto vlan_rem_error;
	}

	err = rtnl_link_alloc_cache(handle, AF_UNSPEC, &cache);
	if (err < 0) {
		cache = NULL;
		wpa_printf(MSG_ERROR, "VLAN: failed to alloc cache: %s",
			   nl_geterror(err));
		goto vlan_rem_error;
	}

	if (!(rlink = rtnl_link_get_by_name(cache, if_name))) {
		/* link does not exist */
		wpa_printf(MSG_ERROR, "VLAN: interface %s does not exists",
			   if_name);
		goto vlan_rem_error;
	}

	err = rtnl_link_delete(handle, rlink);
	if (err < 0) {
		wpa_printf(MSG_ERROR, "VLAN: failed to remove link %s: %s",
			   if_name, nl_geterror(err));
		goto vlan_rem_error;
	}

	ret = 0;

vlan_rem_error:
	if (rlink)
		rtnl_link_put(rlink);
	if (cache)
		nl_cache_free(cache);
	if (handle)
		nl_socket_free(handle);
	return ret;
}
