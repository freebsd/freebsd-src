/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Scott Long
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
 * Thunderbolt PCIe bridge/switch definitions
 *
 * $FreeBSD$
 */

#ifndef _TB_PCIB_H
#define _TB_PCIB_H

DECLARE_CLASS(tb_pcib_driver);

/*
 * The order of the fields is very important.  Class inherentence replies on
 * implicitly knowing the location of the first 3 fields.
 */
struct tb_pcib_softc {
	struct pcib_softc	pcibsc;
	ACPI_HANDLE		ap_handle;
	ACPI_BUFFER		ap_prt;
	device_t		dev;
	u_int			debug;
	int			vsec;
	int			flags;
	struct sysctl_ctx_list	*sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;
};

/* Flags for tb_softc */
#define TB_GEN_UNK	0x00
#define TB_GEN_TB1	0x01
#define TB_GEN_TB2	0x02
#define TB_GEN_TB3	0x03
#define TB_GEN_USB4	0x04
#define TB_GEN_MASK	0x0f
#define TB_HWIF_UNK	0x00
#define TB_HWIF_AR	0x10
#define TB_HWIF_TR	0x20
#define TB_HWIF_ICL	0x30
#define TB_HWIF_USB4	0x40
#define TB_HWIF_MASK	0xf0
#define TB_FLAGS_ISROOT	0x100
#define TB_FLAGS_ISUFP	0x200

#define TB_IS_AR(sc)	(((sc)->flags & TB_HWIF_MASK) == TB_HWIF_AR)
#define TB_IS_TR(sc)	(((sc)->flags & TB_HWIF_MASK) == TB_HWIF_TR)
#define TB_IS_ICL(sc)	(((sc)->flags & TB_HWIF_MASK) == TB_HWIF_ICL)
#define TB_IS_USB4(sc)	(((sc)->flags & TB_HWIF_MASK) == TB_HWIF_USB4)

#define TB_IS_ROOT(sc)	(((sc)->flags & TB_FLAGS_ISROOT) != 0)
#define TB_IS_UFP(sc)	(((sc)->flags & TB_FLAGS_ISUFP) != 0)
#define TB_IS_DFP(sc)	(((sc)->flags & TB_FLAGS_ISUFP) == 0)

/* PCI IDs for the TB bridges */
#define TB_DEV_AR_2C		0x1576
#define TB_DEV_AR_LP		0x15c0
#define TB_DEV_AR_C_4C		0x15d3
#define TB_DEV_AR_C_2C		0x15da
#define TB_DEV_ICL_0		0x8a1d
#define TB_DEV_ICL_1		0x8a21

#define TB_PCIB_VSEC(dev) ((struct tb_pcib_softc *)(device_get_softc(dev)))->vsec;
#define TB_DESC_MAX	80

int tb_pcib_probe_common(device_t, char *);
int tb_pcib_attach_common(device_t dev);

#endif /* _TB_PCIB_H */
