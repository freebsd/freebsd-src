/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Wanpeng Qian <wanpengqian@gmail.com>
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/ioccom.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <libxo/xo.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/endian.h>

#include "nvmecontrol.h"

static void
print_micron_unique_smart(const struct nvme_controller_data *cdata __unused, void *buf, uint32_t size __unused)
{
	uint8_t *walker = buf;
	uint8_t *end = walker + 150;
	const char *name;
	uint64_t raw;
	uint8_t normalized;

	static struct kv_name kv[] =
	{
		{ 0xf9, "NAND Writes 1GiB" },
		{ 0xfa, "NAND Reads 1GiB" },
		{ 0xea, "Thermal Throttle Status" },
		{ 0xe7, "Temperature" },
		{ 0xe8, "Power Consumption" },
		{ 0xaf, "Power Loss Protection" },
	};

	xo_emit("{T:Vendor Unique SMART Information}\n");
	xo_emit("{T:=========================}\n");
	/*
	 * walker[0] = Key
	 * walker[1,2] = reserved
	 * walker[3] = Normalized Value
	 * walker[4] = reserved
	 * walker[5..10] = Little Endian Raw value
	 *	(or other represenations)
	 * walker[11] = reserved
	 */
	while (walker < end) {
		name = kv_lookup(kv, nitems(kv), *walker);
		normalized = walker[3];
		raw = le48dec(walker + 5);
		switch (*walker){
		case 0:
			break;
		case 0xf9:
			/* FALLTHOUGH */
		case 0xfa:
			xo_emit("{:micron-walker/%2X} {:micron-walker-name/%-24s}{Lc:} {:micron-walker-raw/%ju} GiB\n", *walker, name, (uintmax_t)raw);
			break;
		case 0xea:
			xo_emit("{:micron-walker/%2X} {:micron-walker-name/%-24s}{Lc:}", *walker, name);
			if (*(walker + 5) == 0)
				xo_emit(" inactive\n");
			if (*(walker + 5) == 1)
				xo_emit(" active, total throttling time {:micron-walker-6/%u} mins\n", le32dec(walker + 6));
			break;
		case 0xe7:
			xo_emit("{:micron-walker/%2X} {:micron-walker-name/%-24s}{Lc:} max ", *walker, name);
			print_temp_C(le16dec(walker + 5));
			xo_emit("                           : min ");
			print_temp_C(le16dec(walker + 7));
			xo_emit("                           : cur ");
			print_temp_C(le16dec(walker + 9));
			break;
		case 0xe8:
			xo_emit("{:micron-walker/%2X} {:micron-walker-name/%-24s}{Lc:} max {:micron-walker-5/%u} W, min {:micron-walker-7/%u} W, ave {:micron-walker-9/%u} W\n",
			    *walker, name, le16dec(walker + 5), le16dec(walker + 7), le16dec(walker + 9));
			break;
		case 0xaf:
			xo_emit("{:micron-walker/%2X} {:micron-walker-name/%-24s}{Lc:}", *walker, name);
			if (normalized == 100)
				xo_emit(" success");
			if (normalized == 0)
				xo_emit(" failed");
			xo_emit(" {:micron-walker-normalized/%3d}\n", normalized);
			break;
		default:
			xo_emit("{:micron-walker/%2X} {:micron-walker-name/%-24s}{Lc:} {:micron-walker-normalized/%3d} {:micron-walker-raw/%ju}\n",
			    *walker, name, normalized, (uintmax_t)raw);
			break;
		}
		walker += 12;
	}
}

#define MICRON_LOG_UNIQUE_SMART	0xca

NVME_LOGPAGE(micron_smart,
    MICRON_LOG_UNIQUE_SMART,		"micron", "Vendor Unique SMART Information",
    print_micron_unique_smart,		DEFAULT_SIZE);
