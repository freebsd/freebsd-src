/*-
 * Copyright (c) 2014 Alexander V. Chernikov. All rights reserved.
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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/sff8436.h>
#include <net/sff8472.h>

#include <math.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libutil.h>

#include <libifconfig.h>
#include <libifconfig_sfp.h>

#include "ifconfig.h"

void
sfp_status(int s, struct ifreq *ifr, int verbose)
{
	struct ifconfig_sfp_info info;
	struct ifconfig_sfp_info_strings strings;
	struct ifconfig_sfp_vendor_info vendor_info;
	struct ifconfig_sfp_status status;
	ifconfig_handle_t *lifh;
	const char *name;
	size_t channel_count;

	lifh = ifconfig_open();
	if (lifh == NULL)
		return;

	name = ifr->ifr_name;

	if (ifconfig_sfp_get_sfp_info(lifh, name, &info) == -1)
		goto close;

	ifconfig_sfp_get_sfp_info_strings(&info, &strings);

	printf("\tplugged: %s %s (%s)\n",
	    ifconfig_sfp_id_display(info.sfp_id),
	    ifconfig_sfp_physical_spec(&info, &strings),
	    strings.sfp_conn);

	if (ifconfig_sfp_get_sfp_vendor_info(lifh, name, &vendor_info) == -1)
		goto close;

	printf("\tvendor: %s PN: %s SN: %s DATE: %s\n",
	    vendor_info.name, vendor_info.pn, vendor_info.sn, vendor_info.date);

	if (ifconfig_sfp_id_is_qsfp(info.sfp_id)) {
		if (verbose > 1)
			printf("\tcompliance level: %s\n", strings.sfp_rev);
	} else {
		if (verbose > 5) {
			printf("Class: %s\n",
			    ifconfig_sfp_physical_spec(&info, &strings));
			printf("Length: %s\n", strings.sfp_fc_len);
			printf("Tech: %s\n", strings.sfp_cab_tech);
			printf("Media: %s\n", strings.sfp_fc_media);
			printf("Speed: %s\n", strings.sfp_fc_speed);
		}
	}

	if (ifconfig_sfp_get_sfp_status(lifh, name, &status) == 0) {
		if (ifconfig_sfp_id_is_qsfp(info.sfp_id) && verbose > 1)
			printf("\tnominal bitrate: %u Mbps\n", status.bitrate);
		printf("\tmodule temperature: %.2f C voltage: %.2f Volts\n",
		    status.temp, status.voltage);
		channel_count = ifconfig_sfp_channel_count(&info);
		for (size_t chan = 0; chan < channel_count; ++chan) {
			uint16_t rx = status.channel[chan].rx;
			uint16_t tx = status.channel[chan].tx;
			printf("\tlane %zu: "
			    "RX power: %.2f mW (%.2f dBm) TX bias: %.2f mA\n",
			    chan + 1, power_mW(rx), power_dBm(rx), bias_mA(tx));
		}
		ifconfig_sfp_free_sfp_status(&status);
	}

	if (verbose > 2) {
		struct ifconfig_sfp_dump dump;

		if (ifconfig_sfp_get_sfp_dump(lifh, name, &dump) == -1)
			goto close;

		if (ifconfig_sfp_id_is_qsfp(info.sfp_id)) {
			printf("\n\tSFF8436 DUMP (0xA0 128..255 range):\n");
			hexdump(dump.data + QSFP_DUMP1_START, QSFP_DUMP1_SIZE,
			    "\t", HD_OMIT_COUNT | HD_OMIT_CHARS);
			printf("\n\tSFF8436 DUMP (0xA0 0..81 range):\n");
			hexdump(dump.data + QSFP_DUMP0_START, QSFP_DUMP0_SIZE,
			    "\t", HD_OMIT_COUNT | HD_OMIT_CHARS);
		} else {
			printf("\n\tSFF8472 DUMP (0xA0 0..127 range):\n");
			hexdump(dump.data + SFP_DUMP_START, SFP_DUMP_SIZE,
			    "\t", HD_OMIT_COUNT | HD_OMIT_CHARS);
		}
	}

close:
	ifconfig_close(lifh);
}
