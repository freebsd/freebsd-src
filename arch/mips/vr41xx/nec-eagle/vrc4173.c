/*
 * FILE NAME
 *	arch/mips/vr41xx/nec-eagle/vrc4173.c
 *
 * BRIEF MODULE DESCRIPTION
 *	Pre-setup for NEC VRC4173.
 *
 * Author: Yoichi Yuasa
 *         yyuasa@mvista.com or source@mvista.com
 *
 * Copyright 2001,2002 MontaVista Software Inc.
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
#include <linux/config.h>

#ifdef CONFIG_PCI
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/module.h>

#include <asm/io.h>
#include <asm/vr41xx/eagle.h>
#include <asm/vr41xx/vrc4173.h>

#define PCI_CONFIG_ADDR	KSEG1ADDR(0x0f000c18)
#define PCI_CONFIG_DATA	KSEG1ADDR(0x0f000c14)
	
static inline void config_writeb(u8 reg, u8 val)
{
	u32 data;
	int shift;

	writel((1UL << 0x1e) | (reg & 0xfc), PCI_CONFIG_ADDR);
        data = readl(PCI_CONFIG_DATA);

	shift = (reg & 3) << 3;
	data &= ~(0xff << shift);
	data |= (((u32)val) << shift);

	writel(data, PCI_CONFIG_DATA);
}

static inline u16 config_readw(u8 reg)
{
	u32 data;

	writel(((1UL << 30) | (reg & 0xfc)) , PCI_CONFIG_ADDR);
	data = readl(PCI_CONFIG_DATA);

	return (u16)(data >> ((reg & 2) << 3));
}

static inline u32 config_readl(u8 reg)
{
	writel(((1UL << 30) | (reg & 0xfc)) , PCI_CONFIG_ADDR);

	return readl(PCI_CONFIG_DATA);
}

static inline void config_writel(u8 reg, u32 val)
{
	writel((1UL << 0x1e) | (reg & 0xfc), PCI_CONFIG_ADDR);
	writel(val, PCI_CONFIG_DATA);
}

void __init vrc4173_preinit(void)
{
	u32 cmdsts, base;
	u16 cmu_mask;


	if ((config_readw(PCI_VENDOR_ID) == PCI_VENDOR_ID_NEC) &&
	    (config_readw(PCI_DEVICE_ID) == PCI_DEVICE_ID_NEC_VRC4173)) {
		/*
		 * Initialized NEC VRC4173 Bus Control Unit
		 */
		cmdsts = config_readl(PCI_COMMAND);
		config_writel(PCI_COMMAND,
		              cmdsts |
		              PCI_COMMAND_IO |
		              PCI_COMMAND_MEMORY |
		              PCI_COMMAND_MASTER);

		config_writeb(PCI_LATENCY_TIMER, 0x80);

		config_writel(PCI_BASE_ADDRESS_0, VR41XX_PCI_IO_START);
		base = config_readl(PCI_BASE_ADDRESS_0);
		base &= PCI_BASE_ADDRESS_IO_MASK;
		config_writeb(0x40, 0x01);

		/* CARDU1 IDSEL = AD12, CARDU2 IDSEL = AD13 */
		config_writeb(0x41, 0);

		cmu_mask = 0x1000;
		outw(cmu_mask, base + 0x040);
		cmu_mask |= 0x0800;
		outw(cmu_mask, base + 0x040);

		outw(0x000f, base + 0x042);	/* Soft reset of CMU */
		cmu_mask |= 0x05e0;
		outw(cmu_mask, base + 0x040);
		cmu_mask = inw(base + 0x040);	/* dummy read */
		outw(0x0000, base + 0x042);
	}
}

#endif
