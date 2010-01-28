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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include SESINC

#include "eltsub.h"

char *
geteltnm(int type)
{
	static char rbuf[132];

	switch (type) {
	case SESTYP_UNSPECIFIED:
		sprintf(rbuf, "Unspecified");
		break;
	case SESTYP_DEVICE:
		sprintf(rbuf, "Device");
		break;
	case SESTYP_POWER:
		sprintf(rbuf, "Power supply");
		break;
	case SESTYP_FAN:
		sprintf(rbuf, "Cooling element");
		break;
	case SESTYP_THERM:
		sprintf(rbuf, "Temperature sensors");
		break;
	case SESTYP_DOORLOCK:
		sprintf(rbuf, "Door Lock");
		break;
	case SESTYP_ALARM:
		sprintf(rbuf, "Audible alarm");
		break;
	case SESTYP_ESCC:
		sprintf(rbuf, "Enclosure services controller electronics");
		break;
	case SESTYP_SCC:
		sprintf(rbuf, "SCC controller electronics");
		break;
	case SESTYP_NVRAM:
		sprintf(rbuf, "Nonvolatile cache");
		break;
	case SESTYP_UPS:
		sprintf(rbuf, "Uninterruptible power supply");
		break;
	case SESTYP_DISPLAY:
		sprintf(rbuf, "Display");
		break;
	case SESTYP_KEYPAD:
		sprintf(rbuf, "Key pad entry device");
		break;
	case SESTYP_SCSIXVR:
		sprintf(rbuf, "SCSI port/transceiver");
		break;
	case SESTYP_LANGUAGE:
		sprintf(rbuf, "Language");
		break;
	case SESTYP_COMPORT:
		sprintf(rbuf, "Communication Port");
		break;
	case SESTYP_VOM:
		sprintf(rbuf, "Voltage Sensor");
		break;
	case SESTYP_AMMETER:
		sprintf(rbuf, "Current Sensor");
		break;
	case SESTYP_SCSI_TGT:
		sprintf(rbuf, "SCSI target port");
		break;
	case SESTYP_SCSI_INI:
		sprintf(rbuf, "SCSI initiator port");
		break;
	case SESTYP_SUBENC:
		sprintf(rbuf, "Simple sub-enclosure");
		break;
	default:
		(void) sprintf(rbuf, "<Type 0x%x>", type);
		break;
	}
	return (rbuf);
}

static char *
scode2ascii(u_char code)
{
	static char rbuf[32];
	switch (code & 0xf) {
	case SES_OBJSTAT_UNSUPPORTED:
		sprintf(rbuf, "status not supported");
		break;
	case SES_OBJSTAT_OK:
		sprintf(rbuf, "ok");
		break;
	case SES_OBJSTAT_CRIT:
		sprintf(rbuf, "critical");
		break;
	case SES_OBJSTAT_NONCRIT:
		sprintf(rbuf, "non-critical");
		break;
	case SES_OBJSTAT_UNRECOV:
		sprintf(rbuf, "unrecoverable");
		break;
	case SES_OBJSTAT_NOTINSTALLED:
		sprintf(rbuf, "not installed");
		break;
	case SES_OBJSTAT_UNKNOWN:
		sprintf(rbuf, "unknown status");
		break;
	case SES_OBJSTAT_NOTAVAIL:
		sprintf(rbuf, "status not available");
		break;
	default:
		sprintf(rbuf, "unknown status code %x", code & 0xf);
		break;
	}
	return (rbuf);
}


char *
stat2ascii(int eletype __unused, u_char *cstat)
{
	static char ebuf[256], *scode;

	scode = scode2ascii(cstat[0]);
	sprintf(ebuf, "Status=%s (bytes=0x%02x 0x%02x 0x%02x 0x%02x)",
	    scode, cstat[0], cstat[1], cstat[2], cstat[3]);
	return (ebuf);
}
