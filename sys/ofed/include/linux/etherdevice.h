/*-
 * Copyright (c) 2007 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2014 Mellanox Technologies, Ltd. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *	- Redistributions of source code must retain the above
 *	  copyright notice, this list of conditions and the following
 *	  disclaimer.
 *
 *	- Redistributions in binary form must reproduce the above
 *	  copyright notice, this list of conditions and the following
 *	  disclaimer in the documentation and/or other materials
 *	  provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef _LINUX_ETHERDEVICE
#define _LINUX_ETHERDEVICE

#include <linux/types.h>

#define	ETH_MODULE_SFF_8079		1
#define	ETH_MODULE_SFF_8079_LEN		256
#define	ETH_MODULE_SFF_8472		2
#define	ETH_MODULE_SFF_8472_LEN		512
#define	ETH_MODULE_SFF_8636		3
#define	ETH_MODULE_SFF_8636_LEN		256
#define	ETH_MODULE_SFF_8436		4
#define	ETH_MODULE_SFF_8436_LEN		256

struct ethtool_eeprom {
	u32	offset;
	u32	len;
};

struct ethtool_modinfo {
	u32	type;
	u32	eeprom_len;
};

/**
 * is_zero_ether_addr - Determine if give Ethernet address is all zeros.
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is all zeroes.
 */
static inline bool is_zero_ether_addr(const u8 *addr)
{
        return !(addr[0] | addr[1] | addr[2] | addr[3] | addr[4] | addr[5]);
}



/**
 * is_multicast_ether_addr - Determine if the Ethernet address is a multicast.
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is a multicast address.
 * By definition the broadcast address is also a multicast address.
 */
static inline bool is_multicast_ether_addr(const u8 *addr)
{
        return (0x01 & addr[0]);
}

/**
 * is_broadcast_ether_addr - Determine if the Ethernet address is broadcast
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is the broadcast address.
 */
static inline bool is_broadcast_ether_addr(const u8 *addr)
{
        return (addr[0] & addr[1] & addr[2] & addr[3] & addr[4] & addr[5]) == 0xff;
}

/**
 * is_valid_ether_addr - Determine if the given Ethernet address is valid
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Check that the Ethernet address (MAC) is not 00:00:00:00:00:00, is not
 * a multicast address, and is not FF:FF:FF:FF:FF:FF.
 *
 * Return true if the address is valid.
 **/
static inline bool is_valid_ether_addr(const u8 *addr)
{
        /* FF:FF:FF:FF:FF:FF is a multicast address so we don't need to
        ** explicitly check for it here. */
        return !is_multicast_ether_addr(addr) && !is_zero_ether_addr(addr);
}

static inline void ether_addr_copy(u8 *dst, const u8 *src)
{
	memcpy(dst, src, 6);
}

#endif /* _LINUX_ETHERDEVICE */
