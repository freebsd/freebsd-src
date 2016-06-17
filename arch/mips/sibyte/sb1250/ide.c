/*
 * Copyright (C) 2001, 2002, 2003 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*  Derived loosely from ide-pmac.c, so:
 *  
 *  Copyright (C) 1998 Paul Mackerras.
 *  Copyright (C) 1995-1998 Mark Lord
 */
#include <linux/kernel.h>
#include <linux/ide.h>

#include <asm/sibyte/board.h>

extern struct ide_ops std_ide_ops;

#ifdef CONFIG_PCMCIA_SIBYTE
extern ide_ack_intr_t sb_pcmcia_ack_intr;
#define SIBYTE_CS_REG(pcaddr)  (IO_SPACE_BASE + PCMCIA_PHYS - mips_io_port_base + pcaddr)
#define SB_PC_PORT 0xff00
#endif

/*
 * We are limiting the number of PCI-IDE devices to leave room for
 * GenBus IDE (and possibly PCMCIA/CF?)
 */
static int sibyte_ide_default_irq(ide_ioreg_t base)
{
	return 0;
}

static ide_ioreg_t sibyte_ide_default_io_base(int index)
{
	return 0;
}

static void sibyte_ide_init_hwif_ports (hw_regs_t *hw, ide_ioreg_t data_port,
				       ide_ioreg_t ctrl_port, int *irq)
{
#ifdef CONFIG_PCMCIA_SIBYTE
	if (data_port == SB_PC_PORT) {
		data_port = SIBYTE_CS_REG(0);
		ctrl_port = SIBYTE_CS_REG(6);
		hw->ack_intr = sb_pcmcia_ack_intr;
	}
#endif
	std_ide_ops.ide_init_hwif_ports(hw, data_port, ctrl_port, irq);
}

struct ide_ops sibyte_ide_ops = {
	&sibyte_ide_default_irq,
	&sibyte_ide_default_io_base,
	&sibyte_ide_init_hwif_ports
};

/*
 * I/O operations. The FPGA for SiByte generic bus IDE deals with
 * byte-swapping for us, so we can't share the I/O macros with other
 * IDE (e.g. PCI-IDE) devices.
 */

static inline void sibyte_outb(u8 val, unsigned long port) {
	*(volatile u8 *)(mips_io_port_base + (port)) = val;
}

static inline void sibyte_outw(u16 val, unsigned long port) {
	*(volatile u16 *)(mips_io_port_base + (port)) = val;
}

static inline void sibyte_outl(u32 val, unsigned long port) {
	*(volatile u32 *)(mips_io_port_base + (port)) = val;
}

static inline unsigned char sibyte_inb(unsigned long port)
{
	return (*(volatile u8 *)(mips_io_port_base + (port)));
}

static inline unsigned short sibyte_inw(unsigned long port)
{
	return (*(volatile u16 *)(mips_io_port_base + (port)));
}

static inline unsigned int sibyte_inl(unsigned long port)
{
	return (*(volatile u32 *)(mips_io_port_base + (port)));
}


static inline void sibyte_outsb(unsigned long port, void *addr, unsigned int count)
{
	while (count--) {
		sibyte_outb(*(u8 *)addr, port);
		addr++;
	}
}

static inline void sibyte_insb(unsigned long port, void *addr, unsigned int count)
{
	while (count--) {
		*(u8 *)addr = sibyte_inb(port);
		addr++;
	}
}

static inline void sibyte_outsw(unsigned long port, void *addr, unsigned int count)
{
	while (count--) {
		sibyte_outw(*(u16 *)addr, port);
		addr += 2;
	}
}

static inline void sibyte_insw(unsigned long port, void *addr, unsigned int count)
{
	while (count--) {
		*(u16 *)addr = sibyte_inw(port);
		addr += 2;
	}
}

static inline void sibyte_outsl(unsigned long port, void *addr, unsigned int count)
{
	while (count--) {
		sibyte_outl(*(u32 *)addr, port);
		addr += 4;
	}
}

static inline void sibyte_insl(unsigned long port, void *addr, unsigned int count)
{
	while (count--) {
		*(u32 *)addr = sibyte_inl(port);
		addr += 4;
	}
}

void sibyte_set_ideops(ide_hwif_t *hwif)
{
	hwif->INB = sibyte_inb;
	hwif->INW = sibyte_inw;
	hwif->INL = sibyte_inl;
	hwif->OUTB = sibyte_outb;
	hwif->OUTW = sibyte_outw;
	hwif->OUTL = sibyte_outl;
	hwif->INSW = sibyte_insw;
	hwif->INSL = sibyte_insl;
	hwif->OUTSW = sibyte_outsw;
	hwif->OUTSL = sibyte_outsl;
}
