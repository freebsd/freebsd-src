/*
 *  vrc4171.c, NEC VRC4171 base driver.
 *
 *  Copyright (C) 2003  Yoichi Yuasa <yuasa@hh.iij4u.or.jp>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/types.h>

#include <asm/io.h>
#include <asm/vr41xx/vrc4171.h>

MODULE_DESCRIPTION("NEC VRC4171 base driver");
MODULE_AUTHOR("Yoichi Yuasa <yuasa@hh.iij4u.or.jp>");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL_GPL(vrc4171_get_irq_status);
EXPORT_SYMBOL_GPL(vrc4171_set_multifunction_pin);

#define CONFIGURATION1		0x05fe
 #define SLOTB_CONFIG		0xc000
 #define SLOTB_NONE		0x0000
 #define SLOTB_PCCARD		0x4000
 #define SLOTB_CF		0x8000
 #define SLOTB_FLASHROM		0xc000

#define CONFIGURATION2		0x05fc
#define INTERRUPT_STATUS	0x05fa
#define PCS_CONTROL		0x05ee
#define GPIO_DATA		PCS_CONTROL
#define PCS0_UPPER_START	0x05ec
#define PCS0_LOWER_START	0x05ea
#define PCS0_UPPER_STOP		0x05e8
#define PCS0_LOWER_STOP		0x05e6
#define PCS1_UPPER_START	0x05e4
#define PCS1_LOWER_START	0x05e2
#define PCS1_UPPER_STOP		0x05de
#define PCS1_LOWER_STOP		0x05dc

#define VRC4171_REGS_BASE	PCS1_LOWER_STOP
#define VRC4171_REGS_SIZE	0x24

uint16_t vrc4171_get_irq_status(void)
{
	return inw(INTERRUPT_STATUS);
}

void vrc4171_set_multifunction_pin(int config)
{
	uint16_t config1;

	config1 = inw(CONFIGURATION1);
	config1 &= ~SLOTB_CONFIG;

	switch (config) {
	case SLOTB_IS_NONE:
		config1 |= SLOTB_NONE;
		break;
	case SLOTB_IS_PCCARD:
		config1 |= SLOTB_PCCARD;
		break;
	case SLOTB_IS_CF:
		config1 |= SLOTB_CF;
		break;
	case SLOTB_IS_FLASHROM:
		config1 |= SLOTB_FLASHROM;
		break;
	default:
		break;
	}

	outw(config1, CONFIGURATION1);
}

static int __devinit vrc4171_init(void)
{
	if (request_region(VRC4171_REGS_BASE, VRC4171_REGS_SIZE, "NEC VRC4171") == NULL)
		return -EBUSY;

	printk(KERN_INFO "NEC VRC4171 base driver\n");

	return 0;
}

static void __devexit vrc4171_exit(void)
{
	release_region(VRC4171_REGS_BASE, VRC4171_REGS_SIZE);
}

module_init(vrc4171_init);
module_exit(vrc4171_exit);
