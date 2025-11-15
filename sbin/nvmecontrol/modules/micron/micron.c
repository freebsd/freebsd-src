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

	printf("Vendor Unique SMART Information\n");
	printf("=========================\n");
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
			printf("%2X %-24s: %ju GiB\n", *walker, name, (uintmax_t)raw);
			break;
		case 0xea:
			printf("%2X %-24s:", *walker, name);
			if (*(walker + 5) == 0)
				printf(" inactive\n");
			if (*(walker + 5) == 1)
				printf(" active, total throttling time %u mins\n", le32dec(walker + 6));
			break;
		case 0xe7:
			printf("%2X %-24s: max ", *walker, name);
			print_temp_C(le16dec(walker + 5));
			printf("                           : min ");
			print_temp_C(le16dec(walker + 7));
			printf("                           : cur ");
			print_temp_C(le16dec(walker + 9));
			break;
		case 0xe8:
			printf("%2X %-24s: max %u W, min %u W, ave %u W\n",
			    *walker, name, le16dec(walker + 5), le16dec(walker + 7), le16dec(walker + 9));
			break;
		case 0xaf:
			printf("%2X %-24s:", *walker, name);
			if (normalized == 100)
				printf(" success");
			if (normalized == 0)
				printf(" failed");
			printf(" %3d\n", normalized);
			break;
		default:
			printf("%2X %-24s: %3d %ju\n",
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
