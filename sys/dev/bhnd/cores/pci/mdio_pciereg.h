/*-
 * Copyright (c) 2015 Landon Fuller <landon@landonf.org>
 * Copyright (c) 2010 Broadcom Corporation
 * All rights reserved.
 *
 * This file is derived from the pcie_core.h header distributed with Broadcom's
 * initial brcm80211 Linux driver release, as contributed to the Linux staging
 * repository.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $FreeBSD$
 */

#ifndef _BHND_CORES_PCI_MDIO_PCIEREG_H_
#define _BHND_CORES_PCI_MDIO_PCIEREG_H_

/* MDIO register offsets */
#define	BHND_MDIO_CTL			0x0	/**< mdio control */
#define	BHND_MDIO_DATA			0x4	/**< mdio data */

/* MDIO control */
#define	BHND_MDIOCTL_DIVISOR_MASK	0x7f	/* clock divisor mask */
#define	BHND_MDIOCTL_DIVISOR_VAL	0x2	/* default clock divisor */
#define	BHND_MDIOCTL_PREAM_EN		0x80	/* enable preamble mode */
#define	BHND_MDIOCTL_DONE		0x100	/* tranaction completed */

/* MDIO Data */
#define	BHND_MDIODATA_PHYADDR_MASK	0x0f800000	/* phy addr */
#define	BHND_MDIODATA_PHYADDR_SHIFT	23
#define	BHND_MDIODATA_REGADDR_MASK	0x007c0000	/* reg/dev addr */
#define	BHND_MDIODATA_REGADDR_SHIFT	18
#define	BHND_MDIODATA_DATA_MASK		0x0000ffff	/* data  */

#define	BHND_MDIODATA_TA		0x00020000	/* slave turnaround time */
#define	BHND_MDIODATA_START		0x40000000	/* start of transaction */
#define	BHND_MDIODATA_CMD_WRITE		0x10000000	/* write command */
#define	BHND_MDIODATA_CMD_READ		0x20000000	/* read command */

#define	BHND_MDIODATA_ADDR(_phyaddr, _regaddr)	(		\
	(((_phyaddr) << BHND_MDIODATA_PHYADDR_SHIFT) &	\
	    BHND_MDIODATA_PHYADDR_MASK) |		\
	(((_regaddr) << BHND_MDIODATA_REGADDR_SHIFT) &	\
	    BHND_MDIODATA_REGADDR_MASK)			\
)

#endif /* _BHND_CORES_PCI_MDIO_PCIEREG_H_ */
