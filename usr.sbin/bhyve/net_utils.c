/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <net/ethernet.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bhyverun.h"
#include "config.h"
#include "debug.h"
#include "net_utils.h"

int
net_parsemac(const char *mac_str, uint8_t *mac_addr)
{
        struct ether_addr *ea;
        char zero_addr[ETHER_ADDR_LEN] = { 0, 0, 0, 0, 0, 0 };

	if (mac_str == NULL)
		return (EINVAL);

	ea = ether_aton(mac_str);

	if (ea == NULL || ETHER_IS_MULTICAST(ea->octet) ||
	    memcmp(ea->octet, zero_addr, ETHER_ADDR_LEN) == 0) {
		EPRINTLN("Invalid MAC %s", mac_str);
		return (EINVAL);
	} else
		memcpy(mac_addr, ea->octet, ETHER_ADDR_LEN);

        return (0);
}

int
net_parsemtu(const char *mtu_str, unsigned long *mtu)
{
	char *end;
	unsigned long val;

	assert(mtu_str != NULL);

	if (*mtu_str == '-')
		goto err;

	val = strtoul(mtu_str, &end, 0);

	if (*end != '\0')
		goto err;

	if (val == ULONG_MAX)
		return (ERANGE);

	if (val == 0 && errno == EINVAL)
		return (EINVAL);

	*mtu = val;

	return (0);

err:
	errno = EINVAL;
	return (EINVAL);
}

void
net_genmac(struct pci_devinst *pi, uint8_t *macaddr)
{
	/*
	 * The default MAC address is the standard NetApp OUI of 00-a0-98,
	 * followed by an MD5 of the PCI slot/func number and dev name
	 */
	MD5_CTX mdctx;
	unsigned char digest[16];
	char nstr[80];

	snprintf(nstr, sizeof(nstr), "%d-%d-%s", pi->pi_slot,
	    pi->pi_func, get_config_value("name"));

	MD5Init(&mdctx);
	MD5Update(&mdctx, nstr, (unsigned int)strlen(nstr));
	MD5Final(digest, &mdctx);

	macaddr[0] = 0x00;
	macaddr[1] = 0xa0;
	macaddr[2] = 0x98;
	macaddr[3] = digest[0];
	macaddr[4] = digest[1];
	macaddr[5] = digest[2];
}
