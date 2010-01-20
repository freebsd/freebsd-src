/*-
 * Copyright (c) 2007 Bruce M. Simpson.
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

#ifndef _SIBA_SIBA_IDS_H_
#define	_SIBA_SIBA_IDS_H_

/*
 * Constants and structures for SiBa bus enumeration.
 */

struct siba_devid {
	uint16_t	 sd_vendor;
	uint16_t	 sd_device;
	uint8_t		 sd_rev;
	char		*sd_desc;
};

/*
 * Device IDs
 */
#define SIBA_DEVID_ANY		0xffff
#define SIBA_DEVID_CHIPCOMMON	0x0800
#define SIBA_DEVID_INSIDELINE	0x0801
#define SIBA_DEVID_SDRAM	0x0803
#define SIBA_DEVID_PCI		0x0804
#define SIBA_DEVID_MIPS		0x0805
#define SIBA_DEVID_ETHERNET	0x0806
#define SIBA_DEVID_MODEM	0x0807
#define SIBA_DEVID_USB		0x0808
#define SIBA_DEVID_IPSEC	0x080b
#define SIBA_DEVID_SDRAMDDR	0x080f
#define SIBA_DEVID_EXTIF	0x0811
#define SIBA_DEVID_MIPS_3302	0x0816

/*
 * Vendor IDs
 */
#define SIBA_VID_ANY		0xffff
#define SIBA_VID_BROADCOM	0x4243

/*
 * Revision IDs
 */
#define SIBA_REV_ANY		0xff

#endif /*_SIBA_SIBA_IDS_H_ */
