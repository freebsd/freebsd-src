/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ian Lepore <ian@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/condvar.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#include <dev/usb/usb_device.h>
#include <dev/usb/usb_freebsd.h>
#include <dev/usb/usb_fdt_support.h>
#include <dev/usb/net/usb_ethernet.h>

/*
 * Define a constant for allocating an array pointers to serve as a stack of
 * devices between the controller and any arbitrary device on the bus.  The
 * stack ends with the device itself, so add 1 to the max hub nesting depth.
 */
#define	MAX_UDEV_NEST	(MAX(USB_HUB_MAX_DEPTH, USB_SS_HUB_DEPTH_MAX) + 1)

static phandle_t
find_udev_in_children(phandle_t parent, struct usb_device *udev)
{
	phandle_t child;
	ssize_t proplen;
	uint32_t port;
	char compat[16]; /* big enough for "usb1234,abcd" */

	/*
	 * USB device nodes in FDT have a compatible string of "usb" followed by
	 * the vendorId,productId rendered in hex.  The port number is encoded
	 * in the standard 'reg' property; it is one-based in the FDT data, but
	 * usb_device.port_index is zero-based.  To uniquely identify a device,
	 * both the compatible string and the port number must match.
	 */
	snprintf(compat, sizeof(compat), "usb%x,%x",
	    UGETW(udev->ddesc.idVendor), UGETW(udev->ddesc.idProduct));
	for (child = OF_child(parent); child != 0; child = OF_peer(child)) {
		if (!ofw_bus_node_is_compatible(child, compat))
			continue;
		proplen = OF_getencprop(child, "reg", &port, sizeof(port));
		if (proplen != sizeof(port))
			continue;
		if (port == (udev->port_index + 1))
			return (child);
	}
	return (-1);
}

static bool
is_valid_mac_addr(uint8_t *addr)
{

	/*
	 * All-bits-zero and all-bits-one are a couple common cases of what
	 * might get read from unprogrammed eeprom or OTP data, weed them out.
	 */
	if ((addr[0] | addr[1] | addr[2] | addr[3] | addr[4] | addr[5]) == 0x00)
		return (false);
	if ((addr[0] & addr[1] & addr[2] & addr[3] & addr[4] & addr[5]) == 0xff)
		return (false);
	return (true);
}

int
usb_fdt_get_mac_addr(device_t dev, struct usb_ether* ue)
{
	phandle_t node;
	ssize_t i, proplen;
	uint8_t mac[sizeof(ue->ue_eaddr)];
	static const char *properties[] = {
	    "mac-address",
	    "local-mac-address"
	};

	if ((node = usb_fdt_get_node(ue->ue_dev, ue->ue_udev)) == -1)
		return (ENXIO);
	for (i = 0; i < nitems(properties); ++i) {
		proplen = OF_getprop(node, properties[i], mac, sizeof(mac));
		if (proplen == sizeof(mac) && is_valid_mac_addr(mac)) {
			memcpy(ue->ue_eaddr, mac, sizeof(ue->ue_eaddr));
			return (0);
		}
	}
	return (ENXIO);
}

phandle_t
usb_fdt_get_node(device_t dev, struct usb_device *udev)
{
	struct usb_device *ud;
	struct usb_device *udev_stack[MAX_UDEV_NEST];
	phandle_t controller_node, node;
	int idx;

	/*
	 * Start searching at the controller node.  The usb_device links to the
	 * bus, and its parent is the controller.  If we can't get the
	 * controller node, the requesting device cannot be in the fdt data.
	 */
	if ((controller_node = ofw_bus_get_node(udev->bus->parent)) == -1)
		return (-1);

	/*
	 * Walk up the usb hub ancestor hierarchy, building a stack of devices
	 * that begins with the requesting device and includes all the hubs
	 * between it and the controller, NOT including the root hub (the FDT
	 * bindings treat the controller and root hub as the same thing).
	 */
	for (ud = udev, idx = 0; ud->parent_hub != NULL; ud = ud->parent_hub) {
		KASSERT(idx < nitems(udev_stack), ("Too many hubs"));
		udev_stack[idx++] = ud;
	}

	/*
	 * Now walk down the stack of udevs from the controller to the
	 * requesting device, and also down the hierarchy of nested children of
	 * the controller node in the fdt data.  At each nesting level of fdt
	 * data look for a child node whose properties match the vID,pID,portIdx
	 * tuple for the udev at the corresponding layer of the udev stack.  As
	 * long as we keep matching up child nodes with udevs, loop and search
	 * within the children of the just-found child for the next-deepest hub.
	 * If at any level we fail to find a matching node, stop searching and
	 * return.  When we hit the end of the stack (the requesting device) we
	 * return whatever the result was for the search at that nesting level.
	 */
	for (node = controller_node;;) {
		node = find_udev_in_children(node, udev_stack[--idx]);
		if (idx == 0 || node == -1)
			break;
	}
	return (node);
}
