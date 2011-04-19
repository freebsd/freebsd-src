/*-
 * Copyright (c) 2004 Texas A&M University
 * All rights reserved.
 *
 * Developer: Wm. Daryl Hawkins
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

#ifndef _ICHWD_H_
#define _ICHWD_H_

struct ichwd_device {
	uint16_t		 device;
	char			*desc;
	unsigned int		 version;
};

struct ichwd_softc {
	device_t		 device;
	device_t		 ich;
	int			 ich_version;

	int			 active;
	unsigned int		 timeout;

	int			 smi_rid;
	struct resource		*smi_res;
	bus_space_tag_t		 smi_bst;
	bus_space_handle_t	 smi_bsh;

	int			 tco_rid;
	struct resource		*tco_res;
	bus_space_tag_t		 tco_bst;
	bus_space_handle_t	 tco_bsh;

	int			 gcs_rid;
	struct resource		*gcs_res;
	bus_space_tag_t		 gcs_bst;
	bus_space_handle_t	 gcs_bsh;

	eventhandler_tag	 ev_tag;
};

#define VENDORID_INTEL		0x8086
#define DEVICEID_DH89XXCC_LPC	0x2310
#define DEVICEID_82801AA	0x2410
#define DEVICEID_82801AB	0x2420
#define DEVICEID_82801BA	0x2440
#define DEVICEID_82801BAM	0x244c
#define DEVICEID_82801CA	0x2480
#define DEVICEID_82801CAM	0x248c
#define DEVICEID_82801DB	0x24c0
#define DEVICEID_82801DBM	0x24cc
#define DEVICEID_82801E		0x2450
#define DEVICEID_82801EB	0x24dc
#define DEVICEID_82801EBR	0x24d0
#define DEVICEID_6300ESB	0x25a1
#define DEVICEID_82801FBR	0x2640
#define DEVICEID_ICH6M		0x2641
#define DEVICEID_ICH6W		0x2642
#define DEVICEID_63XXESB	0x2670
#define DEVICEID_ICH7		0x27b8
#define DEVICEID_ICH7DH		0x27b0
#define DEVICEID_ICH7M		0x27b9
#define DEVICEID_NM10		0x27bc
#define DEVICEID_ICH7MDH	0x27bd
#define DEVICEID_ICH8		0x2810
#define DEVICEID_ICH8DH		0x2812
#define DEVICEID_ICH8DO		0x2814
#define DEVICEID_ICH8M		0x2815
#define DEVICEID_ICH8ME		0x2811
#define DEVICEID_ICH9		0x2918
#define DEVICEID_ICH9DH		0x2912
#define DEVICEID_ICH9DO		0x2914
#define DEVICEID_ICH9M		0x2919
#define DEVICEID_ICH9ME		0x2917
#define DEVICEID_ICH9R		0x2916
#define DEVICEID_ICH10		0x3a18
#define DEVICEID_ICH10D		0x3a1a
#define DEVICEID_ICH10DO	0x3a14
#define DEVICEID_ICH10R		0x3a16
#define DEVICEID_H55		0x3b06

/* ICH LPC Interface Bridge Registers (ICH5 and older) */
#define ICH_GEN_STA		0xd4
#define ICH_GEN_STA_NO_REBOOT	0x02
#define ICH_PMBASE		0x40 /* ACPI base address register */
#define ICH_PMBASE_MASK		0x7f80 /* bits 7-15 */

/* ICH Chipset Configuration Registers (ICH6 and newer) */
#define ICH_RCBA		0xf0
#define ICH_GCS_OFFSET		0x3410
#define ICH_GCS_SIZE		0x4
#define ICH_GCS_NO_REBOOT	0x20

/* register names and locations (relative to PMBASE) */
#define SMI_BASE		0x30 /* base address for SMI registers */
#define SMI_LEN			0x08
#define SMI_EN			0x00 /* SMI Control and Enable Register */
#define SMI_STS			0x04 /* SMI Status Register */
#define TCO_BASE		0x60 /* base address for TCO registers */
#define TCO_LEN			0x20
#define TCO_RLD			0x00 /* TCO Reload and Current Value */
#define TCO_TMR1		0x01 /* TCO Timer Initial Value
					(ICH5 and older, 8 bits) */
#define TCO_TMR2		0x12 /* TCO Timer Initial Value
					(ICH6 and newer, 16 bits) */
#define TCO_DAT_IN		0x02 /* TCO Data In (DO NOT USE) */
#define TCO_DAT_OUT		0x03 /* TCO Data Out (DO NOT USE) */
#define TCO1_STS		0x04 /* TCO Status 1 */
#define TCO2_STS		0x06 /* TCO Status 2 */
#define TCO1_CNT		0x08 /* TCO Control 1 */
#define TCO2_CNT		0x08 /* TCO Control 2 */

/* bit definitions for SMI_EN and SMI_STS */
#define SMI_TCO_EN		0x2000
#define SMI_TCO_STS		0x2000

/* timer value mask for TCO_RLD and TCO_TMR */
#define TCO_TIMER_MASK		0x1f

/* status bits for TCO1_STS */
#define TCO_TIMEOUT		0x08 /* timed out */
#define TCO_INT_STS		0x04 /* data out (DO NOT USE) */
#define TCO_SMI_STS		0x02 /* data in (DO NOT USE) */

/* status bits for TCO2_STS */
#define TCO_BOOT_STS		0x04 /* failed to come out of reset */
#define TCO_SECOND_TO_STS	0x02 /* ran down twice */

/* control bits for TCO1_CNT */
#define TCO_TMR_HALT		0x0800 /* clear to enable WDT */
#define TCO_CNT_PRESERVE	0x0200 /* preserve these bits */

/*
 * Masks for the TCO timer value field in TCO_RLD.
 * If the datasheets are to be believed, the minimum value actually varies
 * from chipset to chipset - 4 for ICH5 and 2 for all other chipsets.
 * I suspect this is a bug in the ICH5 datasheet and that the minimum is
 * uniformly 2, but I'd rather err on the side of caution.
 */
#define TCO_RLD_TMR_MIN		0x0004
#define TCO_RLD1_TMR_MAX	0x003f
#define TCO_RLD2_TMR_MAX	0x03ff

/* approximate length in nanoseconds of one WDT tick (about 0.6 sec) */
#define ICHWD_TICK		600000000

#endif
