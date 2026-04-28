/*
 * Copyright (c) 2021 Chuck Tuffli <chuck@tuffli.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stddef.h>

#include "libsmart.h"
#include "libsmart_priv.h"

/* Strings from "SMART Attribute Descriptions" (SAD) */
static const char *
desc_ata_data[] = {
	[1] = "Read Error Rate",
	[2] = "Throughput Performance",
	[3] = "Spin-Up Time",
	[4] = "Start/Stop Count",
	[5] = "Reallocated Sectors Count",
	[6] = "Read Channel Margin",
	[7] = "Seek Error Rate",
	[8] = "Seek Time Performance",
	[9] = "Power-On Hours",
	[10] = "Spin Retry Count",
	[11] = "Calibration Retry Count",
	[12] = "Power Cycle Count",
	[13] = "Soft Read Error Rate",
	[22] = "Current Helium Level",		/* HGST */
	[170] = "Available Reserved Space",	/* Intel */
	[171] = "SSD Program Fail",		/* Kingston? */
	[172] = "SSD Erase Fail Count",		/* Kingston? */
	[173] = "SSD Wear Leveling Count",	/* HPE SSD Endurance Limit */
	[174] = "Unexpected Power Loss Count",	/* Intel */
	[175] = "Power Loss Protection Failure", /* Intel */
	[176] = "Erase Fail Count (chip)",
	[177] = "Wear Range Delta",
	[179] = "Used Reserved Block Count Total",
/*	[180] = HPE, Seagate, Intel differences */
	[181] = "Non-4K Aligned Access Count",	/* Micron. Conflict Kingston */
	[182] = "Erase Fail Count",
	[183] = "Runtime Bad Block",
	[184] = "End-to-End Error",
	[185] = "Head Stability",		/* WD */
	[186] = "Induced Op-Vibration Detection", /* WD */
	[187] = "Reported Uncorrectable Errors",
	[188] = "Command Timeout",
	[189] = "High Fly Writes",
	[190] = "Airflow Temperature",		/* WDC, HPE conflict */
	[191] = "G-Sense Error Rate",
	[192] = "Power-Off Count",		/* HPE, Seagate */
	[193] = "Load/Unload Cycle Count",
	[194] = "Temperature Celsius",
	[195] = "Hardware ECC Recovered",
	[196] = "Reallocation Event Count",
	[197] = "Current Pending Sector Count",
	[198] = "Uncorrectable Sector Count",	/* Fujitsu */
	[199] = "UltraDMA CRC Error Count",
	[200] = "Write Error Rate",
	[201] = "Soft Read Error Rate",
	[202] = "Data Address Mark Errors",
	[203] = "Run Out Cancel",
	[204] = "Soft ECC Correction",
	[205] = "Thermal Asperity Rate",
	[206] = "Flying Height",
	[207] = "Spin High Current",
	[208] = "Spin Buzz",
	[209] = "Offline Seek Performnce",
	[210] = "Vibration, During Write",	/* Maxtor */
	[211] = "Vibration During Write",	/* Acronis */
	[212] = "Shock During Write",		/* Acronis */
	[220] = "Disk Shift",
	[221] = "G-Sense Error Rate",
	[222] = "Loaded Hours",
	[223] = "Load/Unload Retry Count",
	[224] = "Load Friction",
	[225] = "Load/Unload Cycle Count",
	[226] = "Load-in Time",
	[227] = "Torque Amplification Count",
	[228] = "Power-off Retract Cycle",
	[230] = "GMR Head Amplitude Drive Life Protection Status",
	[231] = "Temperature SSD Life Left",	/* Kingston */
	[232] = "Endurance Remaining",		/* Multiple conflict */
	[233] = "Power-On Hours",		/* Multiple conflict */
	[234] = "Average Erase Count",		/* Multiple conflict */
	[235] = "Good Block Count",		/* Multiple conflict */
	[240] = "Head Flying Hours",
	[241] = "Total LBAs Written",
	[242] = "Total LBAs Read",
	[243] = "Total LBAs Written Expanded",	/* Multiple conflict */
	[244] = "Total LBAs Read Expanded",	/* Multiple conflict */
	[250] = "Read Error Rate",
	[251] = "Minimum Spares Remaining",
	[252] = "Newly Added Bad Flash Block",
	[254] = "Free Fall Protection"
};

const char *
__smart_ata_desc(uint32_t page, uint32_t id)
{
	const char *desc = NULL;

	switch (page) {
	case PAGE_ID_ATA_SMART_READ_DATA:
		if (desc_ata_data[id] != NULL)
			desc = desc_ata_data[id];
		break;
	case PAGE_ID_ATA_SMART_RET_STATUS:
		desc = "SMART Status";
		break;
	default:
		;
	}

	return (desc);
}

const char *
__smart_scsi_err_desc(uint32_t id)
{
	const char *param = NULL;

	switch (id) {
	case 0:
		param = "Errors corrected without substantial delay";
		break;
	case 1:
		param = "Errors corrected with possible delays";
		break;
	case 2:
		param = "Total retries";
		break;
	case 3:
		param = "Total errors corrected";
		break;
	case 4:
		param = "Total times correction algorithm processed";
		break;
	case 5:
		param = "Total bytes processed";
		break;
	case 6:
		param = "Total uncorrected errors";
		break;
	default:
		return (NULL);
	}

	return (param);
}
