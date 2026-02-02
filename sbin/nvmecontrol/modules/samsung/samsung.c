/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2022 Wanpeng Qian <wanpengqian@gmail.org>
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

struct samsung_log_extended_smart
{
	uint8_t		kv[256];/* Key-Value pair */
	uint32_t	lwaf;	/* Lifetime Write Amplification */
	uint32_t	thwaf;	/* Trailing Hour Write Amplification Factor */
	uint64_t	luw[2];	/* Lifetime User Writes */
	uint64_t	lnw[2];	/* Lifetime NAND Writes */
	uint64_t	lur[2];	/* Lifetime User Reads */
	uint32_t	lrbc;	/* Lifetime Retired Block Count */
	uint16_t	ct;	/* Current Temperature */
	uint16_t	ch;	/* Capacitor Health */
	uint32_t	luurb;	/* Lifetime Unused Reserved Block */
	uint64_t	rrc;	/* Read Reclaim Count */
	uint64_t	lueccc;	/* Lifetime Uncorrectable ECC count */
	uint32_t	lurb;	/* Lifetime Used Reserved Block */
	uint64_t	poh[2];	/* Power on Hours */
	uint64_t	npoc[2];/* Normal Power Off Count */
	uint64_t	spoc[2];/* Sudden Power Off Count */
	uint32_t	pi;	/* Performance Indicator */
} __packed;

static void
print_samsung_extended_smart(const struct nvme_controller_data *cdata __unused, void *buf, uint32_t size __unused)
{
	struct samsung_log_extended_smart *temp = buf;
	char cbuf[UINT128_DIG + 1];
	uint8_t *walker = buf;
	uint8_t *end = walker + 150;
	const char *name;
	uint64_t raw;
	uint8_t normalized;

	static struct kv_name kv[] =
	{
		{ 0xab, "Lifetime Program Fail Count" },
		{ 0xac, "Lifetime Erase Fail Count" },
		{ 0xad, "Lifetime Wear Leveling Count" },
		{ 0xb8, "Lifetime End to End Error Count" },
		{ 0xc7, "Lifetime CRC Error Count" },
		{ 0xe2, "Media Wear %" },
		{ 0xe3, "Host Read %" },
		{ 0xe4, "Workload Timer" },
		{ 0xea, "Lifetime Thermal Throttle Status" },
		{ 0xf4, "Lifetime Phy Pages Written Count" },
		{ 0xf5, "Lifetime Data Units Written" },
	};

	xo_emit("{T:Extended SMART Information}\n");
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
		case 0xad:
			xo_emit("{:samsung-walker/%2X} {:samsung-walker-name/%-41s}{Lc:} {:samsung-walker-normalized/%3d} {Lc:min}{P: }{:samsung-walker-5/%u} {Lc:max}{P: }{:samsung-walker-7/%u} {Lc:ave}{P: }{:samsung-walker-9/%u}\n",
			    le16dec(walker), name, normalized,
			    le16dec(walker + 5), le16dec(walker + 7), le16dec(walker + 9));
			break;
		case 0xe2:
			xo_emit("{:samsung-walker/%2X} {:samsung-walker-name/%-41s}{Lc:} {:samsung-walker-normalized/%3d} {:samsung-walker-raw/%.3f}%\n}",
			    le16dec(walker), name, normalized,
			    raw / 1024.0);
			break;
		case 0xea:
			xo_emit("{:samsung-walker/%2X} {:samsung-walker-name/%-41s}{Lc:} {:samsung-walker-normalized/%3d} {:samsung-walker-5/%d}% {:samsung-walker-6/%d} times\n",
			    le16dec(walker), name, normalized,
			    walker[5], le32dec(walker+6));
			break;
		default:
			xo_emit("{:samsung-walker/%2X} {:samsung-walker-name/%-41s}{Lc:} {:samsung-walker-normalized/%3d} {:samsung-walker-raw/%ju}\n",
			    le16dec(walker), name, normalized,
			    (uintmax_t)raw);
			break;
		}
		walker += 12;
	}

	xo_emit("{P:   }{Lc:Lifetime Write Amplification Factor}{P:        }{:samsung-lifetime-write-amp-factor/%u}\n", le32dec(&temp->lwaf));
	xo_emit("{P:   }{Lc:Trailing Hour Write Amplification Factor}{P:   }{:samsung-hour-write-amp-factor/%u}\n", le32dec(&temp->thwaf));
	xo_emit("{P:   }{Lc:Lifetime User Writes}{P:                       }{:samsung-lifetime-user-writes/%s}\n",
	    uint128_to_str(to128(temp->luw), cbuf, sizeof(cbuf)));
	xo_emit("{P:   }{Lc:Lifetime NAND Writes}{P:                       }{:samsung-lifetime-nand-writes/%s}\n",
	    uint128_to_str(to128(temp->lnw), cbuf, sizeof(cbuf)));
	xo_emit("{P:   }{Lc:Lifetime User Reads}{P:                        }{:samsung-lifetime-user-reads/%s}\n",
	    uint128_to_str(to128(temp->lur), cbuf, sizeof(cbuf)));
	xo_emit("{P:   }{Lc:Lifetime Retired Block Count}{P:               }{:samsung-lifetime-retired-block-count/%u}\n", le32dec(&temp->lrbc));
	xo_emit("{P:   }{Lc:Current Temperature}{P:                        }");
	print_temp_K(le16dec(&temp->ct));
	xo_emit("{P:   }{Lc:Capacitor Health}{P:                           }{:samsung-capacitor-health/%u}\n", le16dec(&temp->ch));
	xo_emit("{P:   }{Lc:Reserved Erase Block Count}{P:                 }{:samsung-reserved-erase-block-count/%u}\n", le32dec(&temp->luurb));
	xo_emit("{P:   }{Lc:Read Reclaim Count}{P:                         }{:samsung-read-reclaim-count/%ju}\n", (uintmax_t) le64dec(&temp->rrc));
	xo_emit("{P:   }{Lc:Lifetime Uncorrectable ECC Count}{P:           }{:samsung-lifetime-uncorrectable-ecc-count/%ju}\n", (uintmax_t) le64dec(&temp->lueccc));
	xo_emit("{P:   }{Lc:Reallocated Block Count}{P:                    }{:samsung-reallocated-block-count/%u}\n", le32dec(&temp->lurb));
	xo_emit("{P:   }{Lc:Power on Hours}{P:                             }{:samsung-power-on-hours/%s}\n",
	    uint128_to_str(to128(temp->poh), cbuf, sizeof(cbuf)));
	xo_emit("{P:   }{Lc:Normal Power Off Count}{P:                     }{:samsung-normal-power-off-count/%s}\n",
	    uint128_to_str(to128(temp->npoc), cbuf, sizeof(cbuf)));
	xo_emit("{P:   }{Lc:Sudden Power Off Count}{P:                     }{:samsung-sudden-power-off-count/%s}\n",
	    uint128_to_str(to128(temp->spoc), cbuf, sizeof(cbuf)));
	xo_emit("{P:   }{Lc:Performance Indicator}{P:                      }{:samsung-performance-indicator/%u}\n", le32dec(&temp->pi));
}

#define SAMSUNG_LOG_EXTEND_SMART 0xca

NVME_LOGPAGE(samsung_extended_smart,
    SAMSUNG_LOG_EXTEND_SMART,		"samsung", "Extended SMART Information",
    print_samsung_extended_smart,	DEFAULT_SIZE);
