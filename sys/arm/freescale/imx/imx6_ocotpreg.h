/*-
 * Copyright (c) 2014 Steven Lawrance <stl@koffein.net>
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

#ifndef	IMX6_OCOTPREG_H
#define	IMX6_OCOTPREG_H

#define	IMX6_OCOTP_CTRL				0x000
#define	IMX6_OCOTP_CTRL_SET			0x004
#define	IMX6_OCOTP_CTRL_CLR			0x008
#define	IMX6_OCOTP_CTRL_TOG			0x00C
#define	IMX6_OCOTP_TIMING			0x010
#define	IMX6_OCOTP_DATA				0x020
#define	IMX6_OCOTP_READ_CTRL			0x030
#define	IMX6_OCOTP_READ_FUSE_DATA		0x040
#define	IMX6_OCOTP_SW_STICKY			0x050
#define	IMX6_OCOTP_SCS				0x060
#define	IMX6_OCOTP_SCS_SET			0x064
#define	IMX6_OCOTP_SCS_CLR			0x068
#define	IMX6_OCOTP_SCS_TOG			0x06C
#define	IMX6_OCOTP_VERSION			0x090
#define	IMX6_OCOTP_LOCK				0x400
#define	IMX6_OCOTP_CFG0				0x410
#define	IMX6_OCOTP_CFG1				0x420
#define	IMX6_OCOTP_CFG2				0x430
#define	IMX6_OCOTP_CFG3				0x440
#define	IMX6_OCOTP_CFG4				0x450
#define	IMX6_OCOTP_CFG5				0x460
#define	IMX6_OCOTP_CFG6				0x470
#define	IMX6_OCOTP_MEM0				0x480
#define	IMX6_OCOTP_MEM1				0x490
#define	IMX6_OCOTP_MEM2				0x4A0
#define	IMX6_OCOTP_MEM3				0x4B0
#define	IMX6_OCOTP_ANA0				0x4D0
#define	IMX6_OCOTP_ANA1				0x4E0
#define	IMX6_OCOTP_ANA2				0x4F0
#define	IMX6_OCOTP_SRK0				0x580
#define	IMX6_OCOTP_SRK1				0x590
#define	IMX6_OCOTP_SRK2				0x5A0
#define	IMX6_OCOTP_SRK3				0x5B0
#define	IMX6_OCOTP_SRK4				0x5C0
#define	IMX6_OCOTP_SRK5				0x5D0
#define	IMX6_OCOTP_SRK6				0x5E0
#define	IMX6_OCOTP_SRK7				0x5F0
#define	IMX6_OCOTP_HSJC_RESP0			0x600
#define	IMX6_OCOTP_HSJC_RESP1			0x610
#define	IMX6_OCOTP_MAC0				0x620
#define	IMX6_OCOTP_MAC1				0x630
#define	IMX6_OCOTP_GP1				0x660
#define	IMX6_OCOTP_GP2				0x670
#define	IMX6_OCOTP_MISC_CONF			0x6D0
#define	IMX6_OCOTP_FIELD_RETURN			0x6E0
#define	IMX6_OCOTP_SRK_REVOKE			0x6F0

#define	IMX6_OCOTP_LAST_REG			IMX6_OCOTP_SRK_REVOKE

#endif
