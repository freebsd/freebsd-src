/**
 * @file
 * @section AUTHORS
 *
 * Copyright (C) 2010  Rafal Wojtczuk  <rafal@invisiblethingslab.com>
 *
 *  Authors:
 *       Rafal Wojtczuk  <rafal@invisiblethingslab.com>
 *       Daniel De Graaf <dgdegra@tycho.nsa.gov>
 *
 * @section LICENSE
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * @section DESCRIPTION
 *
 *  Originally borrowed from the Qubes OS Project, http://www.qubes-os.org,
 *  this code has been substantially rewritten to use the gntdev and gntalloc
 *  devices instead of raw MFNs and map_foreign_range.
 *
 *  This is a library for inter-domain communication.  A standard Xen ring
 *  buffer is used, with a datagram-based interface built on top.  The grant
 *  reference and event channels are shared in XenStore under a user-specified
 *  path.
 *
 *  The ring.h macros define an asymmetric interface to a shared data structure
 *  that assumes all rings reside in a single contiguous memory space. This is
 *  not suitable for vchan because the interface to the ring is symmetric except
 *  for the setup. Unlike the producer-consumer rings defined in ring.h, the
 *  size of the rings used in vchan are determined at execution time instead of
 *  compile time, so the macros in ring.h cannot be used to access the rings.
 */

#include <stdint.h>
#include <sys/types.h>

struct ring_shared {
	uint32_t cons, prod;
};

#define VCHAN_NOTIFY_WRITE 0x1
#define VCHAN_NOTIFY_READ 0x2

/**
 * vchan_interface: primary shared data structure
 */
struct vchan_interface {
	/**
	 * Standard consumer/producer interface, one pair per buffer
	 * left is client write, server read
	 * right is client read, server write
	 */
	struct ring_shared left, right;
	/**
	 * size of the rings, which determines their location
	 * 10   - at offset 1024 in ring's page
	 * 11   - at offset 2048 in ring's page
	 * 12+  - uses 2^(N-12) grants to describe the multi-page ring
	 * These should remain constant once the page is shared.
	 * Only one of the two orders can be 10 (or 11).
	 */
	uint16_t left_order, right_order;
	/**
	 * Shutdown detection:
	 *  0: client (or server) has exited
	 *  1: client (or server) is connected
	 *  2: client has not yet connected
	 */
	uint8_t cli_live, srv_live;
	/**
	 * Notification bits:
	 *  VCHAN_NOTIFY_WRITE: send notify when data is written
	 *  VCHAN_NOTIFY_READ: send notify when data is read (consumed)
	 * cli_notify is used for the client to inform the server of its action
	 */
	uint8_t cli_notify, srv_notify;
	/**
	 * Grant list: ordering is left, right. Must not extend into actual ring
	 * or grow beyond the end of the initial shared page.
	 * These should remain constant once the page is shared, to allow
	 * for possible remapping by a client that restarts.
	 */
	uint32_t grants[0];
};

