/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Alstom Group.
 * Copyright (c) 2021 Semihalf.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/enetc/enetc_hw.h>
#include <dev/enetc/enetc_mdio.h>

#define ENETC_MDIO_RD4(regs, base, off) \
	bus_read_4((regs), (base) + (off))
#define ENETC_MDIO_WR4(regs, base, off, value) \
	bus_write_4((regs), (base) + (off), (value))

static int
enetc_mdio_wait(struct resource *regs, int mdio_base)
{
	int i;
	uint32_t val;

	i = 0;
	do {
		DELAY(100);
		val = ENETC_MDIO_RD4(regs, mdio_base, ENETC_MDIO_CFG);
		if ((val & MDIO_CFG_BSY) == 0)
			return (0);
	} while (i++ < ENETC_TIMEOUT);

	return (ETIMEDOUT);
}

int
enetc_mdio_read(struct resource *regs, int mdio_base, int phy, int reg)
{
	uint32_t mdio_cfg, mdio_ctl;
	uint16_t dev_addr;

	mdio_cfg = MDIO_CFG_CLKDIV(ENETC_MDC_DIV) | MDIO_CFG_NEG;
	if (reg & MII_ADDR_C45) {
		/* clause 45 */
		dev_addr = (reg >> 16) & 0x1f;
		mdio_cfg |= MDIO_CFG_ENC45;
	} else {
		/* clause 22 */
		dev_addr = reg & 0x1f;
		mdio_cfg &= ~MDIO_CFG_ENC45;
	}

	ENETC_MDIO_WR4(regs, mdio_base, ENETC_MDIO_CFG, mdio_cfg);

	if (enetc_mdio_wait(regs, mdio_base) == ETIMEDOUT)
		return (EIO);

	/* Set port and device addr. */
	mdio_ctl = MDIO_CTL_PORT_ADDR(phy) | MDIO_CTL_DEV_ADDR(dev_addr);
	ENETC_MDIO_WR4(regs, mdio_base, ENETC_MDIO_CTL, mdio_ctl);

	/* Set the register address. */
	if (reg & MII_ADDR_C45) {
		ENETC_MDIO_WR4(regs, mdio_base, ENETC_MDIO_ADDR, reg & 0xffff);

		if (enetc_mdio_wait(regs, mdio_base) == ETIMEDOUT)
			return (EIO);
	}

	/* Initiate the read. */
	ENETC_MDIO_WR4(regs, mdio_base, ENETC_MDIO_CTL, mdio_ctl | MDIO_CTL_READ);

	if (enetc_mdio_wait(regs, mdio_base) == ETIMEDOUT)
		return (EIO);

	/* Check if any error occurred while reading PHY register. */
	if (ENETC_MDIO_RD4(regs, mdio_base, ENETC_MDIO_CFG) & MDIO_CFG_RD_ER)
		return (ENXIO);

	return (MDIO_DATA(ENETC_MDIO_RD4(regs, mdio_base, ENETC_MDIO_DATA)));
}

int
enetc_mdio_write(struct resource *regs, int mdio_base, int phy, int reg,
    int data)
{
	uint32_t mdio_cfg, mdio_ctl;
	uint16_t dev_addr;

	mdio_cfg = MDIO_CFG_CLKDIV(ENETC_MDC_DIV) | MDIO_CFG_NEG;
	if (reg & MII_ADDR_C45) {
		/* clause 45 */
		dev_addr = (reg >> 16) & 0x1f;
		mdio_cfg |= MDIO_CFG_ENC45;
	} else {
		/* clause 22 */
		dev_addr = reg & 0x1f;
		mdio_cfg &= ~MDIO_CFG_ENC45;
	}

	ENETC_MDIO_WR4(regs, mdio_base, ENETC_MDIO_CFG, mdio_cfg);

	if (enetc_mdio_wait(regs, mdio_base) == ETIMEDOUT)
		return (EIO);

	/* Set port and device addr. */
	mdio_ctl = MDIO_CTL_PORT_ADDR(phy) | MDIO_CTL_DEV_ADDR(dev_addr);
	ENETC_MDIO_WR4(regs, mdio_base, ENETC_MDIO_CTL, mdio_ctl);

	/* Set the register address. */
	if (reg & MII_ADDR_C45) {
		ENETC_MDIO_WR4(regs, mdio_base, ENETC_MDIO_ADDR, reg & 0xffff);

		if (enetc_mdio_wait(regs, mdio_base) == ETIMEDOUT)
			return (EIO);
	}

	/* Write the value. */
	ENETC_MDIO_WR4(regs, mdio_base, ENETC_MDIO_DATA, MDIO_DATA(data));

	if (enetc_mdio_wait(regs, mdio_base) == ETIMEDOUT)
		return (EIO);

	return (0);
}
