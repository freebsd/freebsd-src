/*
 * Copyright (c) 2000,2001 Jonathan Chen.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
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
 * $FreeBSD$
 */

/*
 * Cardbus CIS definitions
 */

int cardbus_do_cis(device_t dev, device_t child);

#define MAXTUPLESIZE		0x400

/* CIS TUPLES */

#define CISTPL_NULL		0x00
#define CISTPL_DEVICE		0x01
#define CISTPL_LONG_LINK_CB	0x02
#define CISTPL_INDIRECT		0x03
#define CISTPL_CONFIG_CB	0x04
#define CISTPL_CFTABLE_ENTRY_CB	0x05
#define CISTPL_LONGLINK_MFC	0x06
#define CISTPL_BAR		0x07
#define CISTPL_PWR_MGMNT	0x08
#define CISTPL_EXTDEVICE	0x09
#define CISTPL_CHECKSUM		0x10
#define CISTPL_LONGLINK_A	0x11
#define CISTPL_LONGLINK_C	0x12
#define CISTPL_LINKTARGET	0x13
#define CISTPL_NO_LINK		0x14
#define CISTPL_VERS_1		0x15
#define CISTPL_ALTSTR		0x16
#define CISTPL_DEVICE_A		0x17
#define CISTPL_JEDEC_C		0x18
#define CISTPL_JEDEC_A		0x19
#define CISTPL_CONFIG		0x1A
#define CISTPL_CFTABLE_ENTRY	0x1B
#define CISTPL_DEVICE_OC	0x1C
#define CISTPL_DEVICE_OA	0x1D
#define CISTPL_DEVICE_GEO	0x1E
#define CISTPL_DEVICE_GEO_A	0x1F
#define CISTPL_MANFID		0x20
#define CISTPL_FUNCID		0x21
#define CISTPL_FUNCE		0x22
#define CISTPL_SWIL		0x23
#define CISTPL_VERS_2		0x40
#define CISTPL_FORMAT		0x41
#define CISTPL_GEOMETRY		0x42
#define CISTPL_BYTEORDER	0x43
#define CISTPL_DATE		0x44
#define CISTPL_BATTERY		0x45
#define CISTPL_ORG		0x46
#define CISTPL_CUSTOMSTART	0x80
#define CISTPL_END		0xFF

/* BAR */
#define TPL_BAR_REG_ASI_MASK	0x07
#define TPL_BAR_REG_AS		0x08

/* CISTPL_FUNC */
#define TPL_FUNC_MF		0	/* multi function tuple */
#define TPL_FUNC_MEM		1	/* memory */
#define TPL_FUNC_SERIAL		2	/* serial, including modem and fax */
#define TPL_FUNC_PARALLEL	3	/* parallel, including printer and SCSI */
#define TPL_FUNC_DISK		4	/* Disk */
#define TPL_FUNC_VIDEO		5	/* Video Adaptor */
#define TPL_FUNC_LAN		6	/* LAN Adaptor */
#define TPL_FUNC_AIMS		7	/* Auto Inclement Mass Strages */

/* TPL_FUNC_LAN */
#define TPL_FUNCE_LAN_TECH	1	/* technology */
#define TPL_FUNCE_LAN_SPEED	2	/* speed */
#define TPL_FUNCE_LAN_MEDIA	2	/* which media do you use? */
#define TPL_FUNCE_LAN_NID	4	/* node id (address) */
#define TPL_FUNCE_LAN_CONN	5	/* connector type (shape) */
