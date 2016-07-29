/* $FreeBSD$ */
/*
 * Copyright (c) 2000 by Matthew Jacob
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * the GNU Public License ("GPL").
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * Matthew Jacob
 * Feral Software
 * mjacob@feral.com
 */

#include <sys/endian.h>
#include <sys/types.h>
#include <sys/sbuf.h>

#include <err.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <cam/scsi/scsi_enc.h>

#include "eltsub.h"

/*
 * offset by +20 degrees.
 * The range of the value expresses a temperature between -19 and +235 degrees
 * Celsius. A value of 00h is reserved.
 */
#define TEMPERATURE_OFFSET 20

char *
geteltnm(int type)
{
	static char rbuf[132];

	switch (type) {
	case ELMTYP_UNSPECIFIED:
		sprintf(rbuf, "Unspecified");
		break;
	case ELMTYP_DEVICE:
		sprintf(rbuf, "Device Slot");
		break;
	case ELMTYP_POWER:
		sprintf(rbuf, "Power Supply");
		break;
	case ELMTYP_FAN:
		sprintf(rbuf, "Cooling");
		break;
	case ELMTYP_THERM:
		sprintf(rbuf, "Temperature Sensors");
		break;
	case ELMTYP_DOORLOCK:
		sprintf(rbuf, "Door Lock");
		break;
	case ELMTYP_ALARM:
		sprintf(rbuf, "Audible alarm");
		break;
	case ELMTYP_ESCC:
		sprintf(rbuf, "Enclosure Services Controller Electronics");
		break;
	case ELMTYP_SCC:
		sprintf(rbuf, "SCC Controller Electronics");
		break;
	case ELMTYP_NVRAM:
		sprintf(rbuf, "Nonvolatile Cache");
		break;
	case ELMTYP_INV_OP_REASON:
		sprintf(rbuf, "Invalid Operation Reason");
		break;
	case ELMTYP_UPS:
		sprintf(rbuf, "Uninterruptible Power Supply");
		break;
	case ELMTYP_DISPLAY:
		sprintf(rbuf, "Display");
		break;
	case ELMTYP_KEYPAD:
		sprintf(rbuf, "Key Pad Entry");
		break;
	case ELMTYP_ENCLOSURE:
		sprintf(rbuf, "Enclosure");
		break;
	case ELMTYP_SCSIXVR:
		sprintf(rbuf, "SCSI Port/Transceiver");
		break;
	case ELMTYP_LANGUAGE:
		sprintf(rbuf, "Language");
		break;
	case ELMTYP_COMPORT:
		sprintf(rbuf, "Communication Port");
		break;
	case ELMTYP_VOM:
		sprintf(rbuf, "Voltage Sensor");
		break;
	case ELMTYP_AMMETER:
		sprintf(rbuf, "Current Sensor");
		break;
	case ELMTYP_SCSI_TGT:
		sprintf(rbuf, "SCSI Target Port");
		break;
	case ELMTYP_SCSI_INI:
		sprintf(rbuf, "SCSI Initiator Port");
		break;
	case ELMTYP_SUBENC:
		sprintf(rbuf, "Simple Subenclosure");
		break;
	case ELMTYP_ARRAY_DEV:
		sprintf(rbuf, "Array Device Slot");
		break;
	case ELMTYP_SAS_EXP:
		sprintf(rbuf, "SAS Expander");
		break;
	case ELMTYP_SAS_CONN:
		sprintf(rbuf, "SAS Connector");
		break;
	default:
		(void) sprintf(rbuf, "<Type 0x%x>", type);
		break;
	}
	return (rbuf);
}

char *
scode2ascii(u_char code)
{
	static char rbuf[32];
	switch (code & 0xf) {
	case SES_OBJSTAT_UNSUPPORTED:
		sprintf(rbuf, "Unsupported");
		break;
	case SES_OBJSTAT_OK:
		sprintf(rbuf, "OK");
		break;
	case SES_OBJSTAT_CRIT:
		sprintf(rbuf, "Critical");
		break;
	case SES_OBJSTAT_NONCRIT:
		sprintf(rbuf, "Noncritical");
		break;
	case SES_OBJSTAT_UNRECOV:
		sprintf(rbuf, "Unrecoverable");
		break;
	case SES_OBJSTAT_NOTINSTALLED:
		sprintf(rbuf, "Not Installed");
		break;
	case SES_OBJSTAT_UNKNOWN:
		sprintf(rbuf, "Unknown");
		break;
	case SES_OBJSTAT_NOTAVAIL:
		sprintf(rbuf, "Not Available");
		break;
	case SES_OBJSTAT_NOACCESS:
		sprintf(rbuf, "No Access Allowed");
		break;
	default:
		sprintf(rbuf, "<Status 0x%x>", code & 0xf);
		break;
	}
	return (rbuf);
}

struct sbuf *
stat2sbuf(int eletype, u_char *cstat)
{
	struct sbuf *buf;

	buf = sbuf_new_auto();
	if (buf == NULL)
		err(EXIT_FAILURE, "sbuf_new_auto()");

	if (cstat[0] & 0x40)
		sbuf_printf(buf, "\t\t- Predicted Failure\n");
	if (cstat[0] & 0x20)
		sbuf_printf(buf, "\t\t- Disabled\n");
	if (cstat[0] & 0x10)
		sbuf_printf(buf, "\t\t- Swapped\n");
	switch (eletype) {
	case ELMTYP_DEVICE:
		if (cstat[2] & 0x02)
			sbuf_printf(buf, "\t\t- LED=locate\n");
		if (cstat[2] & 0x20)
			sbuf_printf(buf, "\t\t- LED=fault\n");
		break;
	case ELMTYP_ARRAY_DEV:
		if (cstat[2] & 0x02)
			sbuf_printf(buf, "\t\t- LED=locate\n");
		if (cstat[2] & 0x20)
			sbuf_printf(buf, "\t\t- LED=fault\n");
		break;
	case ELMTYP_FAN:
		sbuf_printf(buf, "\t\t- Speed: %d rpm\n",
		    (((0x7 & cstat[1]) << 8) + cstat[2]) * 10);
		break;
	case ELMTYP_THERM:
		if (cstat[2]) {
			sbuf_printf(buf, "\t\t- Temperature: %d C\n",
			    cstat[2] - TEMPERATURE_OFFSET);
		} else {
			sbuf_printf(buf, "\t\t- Temperature: -reserved-\n");
		}
		break;
	case ELMTYP_VOM:
		sbuf_printf(buf, "\t\t- Voltage: %.2f V\n",
		    be16dec(cstat + 2) / 100.0);
		break;
	}
	sbuf_finish(buf);
	return (buf);
}
