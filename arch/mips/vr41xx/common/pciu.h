/*
 * FILE NAME
 *	arch/mips/vr41xx/common/pciu.h
 *
 * BRIEF MODULE DESCRIPTION
 *	Include file for PCI Control Unit of the NEC VR4100 series.
 *
 * Author: Yoichi Yuasa
 *         yyuasa@mvista.com or source@mvista.com
 *
 * Copyright 2002 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
 * Changes:
 *  MontaVista Software Inc. <yyuasa@mvista.com> or <source@mvista.com>
 *  - New creation, NEC VR4122 and VR4131 are supported.
 */
#ifndef __VR41XX_PCIU_H
#define __VR41XX_PCIU_H

#include <linux/config.h>
#include <asm/addrspace.h>

#define BIT(x)	(1 << (x))

#define PCIMMAW1REG			KSEG1ADDR(0x0f000c00)
#define PCIMMAW2REG			KSEG1ADDR(0x0f000c04)
#define PCITAW1REG			KSEG1ADDR(0x0f000c08)
#define PCITAW2REG			KSEG1ADDR(0x0f000c0c)
#define PCIMIOAWREG			KSEG1ADDR(0x0f000c10)
#define INTERNAL_BUS_BASE_ADDRESS	0xff000000
#define ADDRESS_MASK			0x000fe000
#define PCI_ACCESS_ENABLE		BIT(12)
#define PCI_ADDRESS_SETTING		0x000000ff

#define PCICONFDREG			KSEG1ADDR(0x0f000c14)
#define PCICONFAREG			KSEG1ADDR(0x0f000c18)
#define PCIMAILREG			KSEG1ADDR(0x0f000c1c)

#define BUSERRADREG			KSEG1ADDR(0x0f000c24)
#define ERROR_ADDRESS			0xfffffffc

#define INTCNTSTAREG			KSEG1ADDR(0x0f000c28)
#define MABTCLR				BIT(31)
#define TRDYCLR				BIT(30)
#define PARCLR				BIT(29)
#define MBCLR				BIT(28)
#define SERRCLR				BIT(27)

#define PCIEXACCREG			KSEG1ADDR(0x0f000c2c)
#define UNLOCK				BIT(1)
#define EAREQ				BIT(0)

#define PCIRECONTREG			KSEG1ADDR(0x0f000c30)
#define RTRYCNT				0x000000ff

#define PCIENREG			KSEG1ADDR(0x0f000c34)
#define CONFIG_DONE			BIT(2)

#define PCICLKSELREG			KSEG1ADDR(0x0f000c38)
#define EQUAL_VTCLOCK			0x00000002
#define HALF_VTCLOCK			0x00000000
#define QUARTER_VTCLOCK			0x00000001

#define PCITRDYVREG			KSEG1ADDR(0x0f000c3c)

#define PCICLKRUNREG			KSEG1ADDR(0x0f000c60)

#define PCIU_CONFIGREGS_BASE		KSEG1ADDR(0x0f000d00)
#define VENDORIDREG			KSEG1ADDR(0x0f000d00)
#define DEVICEIDREG			KSEG1ADDR(0x0f000d00)
#define COMMANDREG			KSEG1ADDR(0x0f000d04)
#define STATUSREG			KSEG1ADDR(0x0f000d04)
#define REVIDREG			KSEG1ADDR(0x0f000d08)
#define CLASSREG			KSEG1ADDR(0x0f000d08)
#define CACHELSREG			KSEG1ADDR(0x0f000d0c)
#define LATTIMEREG			KSEG1ADDR(0x0f000d0c)
#define MAILBAREG			KSEG1ADDR(0x0f000d10)
#define PCIMBA1REG			KSEG1ADDR(0x0f000d14)
#define PCIMBA2REG			KSEG1ADDR(0x0f000d18)
#define INTLINEREG			KSEG1ADDR(0x0f000d3c)
#define INTPINREG			KSEG1ADDR(0x0f000d3c)
#define RETVALREG			KSEG1ADDR(0x0f000d40)
#define PCIAPCNTREG			KSEG1ADDR(0x0f000d40)

#define MPCIINTREG			KSEG1ADDR(0x0f0000b2)

#define MAX_PCI_CLOCK			33333333

static inline int pciu_read_config_byte(int where, u8 *val)
{
	u32 data;

	data = readl(PCIU_CONFIGREGS_BASE + where);
	*val = (u8)(data >> ((where & 3) << 3));

	return PCIBIOS_SUCCESSFUL;
}

static inline int pciu_read_config_word(int where, u16 *val)
{
	u32 data;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	data = readl(PCIU_CONFIGREGS_BASE + where);
	*val = (u16)(data >> ((where & 2) << 3));

	return PCIBIOS_SUCCESSFUL;
}

static inline int pciu_read_config_dword(int where, u32 *val)
{
	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	*val = readl(PCIU_CONFIGREGS_BASE + where);

	return PCIBIOS_SUCCESSFUL;
}

static inline int pciu_write_config_byte(int where, u8 val)
{
	writel(val, PCIU_CONFIGREGS_BASE + where);

	return 0;
}

static inline int pciu_write_config_word(int where, u16 val)
{
	writel(val, PCIU_CONFIGREGS_BASE + where);

	return 0;
}

static inline int pciu_write_config_dword(int where, u32 val)
{
	writel(val, PCIU_CONFIGREGS_BASE + where);

	return 0;
}

#endif /* __VR41XX_PCIU_H */
