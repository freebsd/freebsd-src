
/*
 * linux/arch/arm/mach-sa1100/frodo.c
 *
 * Author: Abraham van der Merwe <abraham@2d3d.co.za>
 *
 * This file contains the 2d3D, Inc. SA-1110 Development Board tweaks.
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * History:
 *
 *   2002/05/27   Setup GPIOs for all the onboard peripherals so
 *                that we don't have to do it in each driver
 *
 *   2002/01/31   Initial version
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/spinlock.h>

#include <asm/setup.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>

#include "generic.h"

static struct map_desc frodo_io_desc[] __initdata = {
	/* virtual     physical    length      domain     r  w  c  b */
	{ 0xe8000000, 0x00000000, 0x04000000, DOMAIN_IO, 0, 1, 0, 0 },	/* flash memory */
	{ 0xf0000000, 0x40000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 },	/* 16-bit on-board devices (including CPLDs) */
	{ 0xf1000000, 0x18000000, 0x04000000, DOMAIN_IO, 0, 1, 0, 0 },	/* 32-bit daughter card */
	LAST_DESC
};

static spinlock_t frodo_cpld_lock = SPIN_LOCK_UNLOCKED;
static volatile u16 *frodo_cpld_memory = (u16 *) 0xf0000000;

static void __init frodo_map_io (void)
{
	sa1100_map_io ();
	iotable_init (frodo_io_desc);

	sa1100_register_uart (0,2);		/* UART2 (serial console) */
	sa1100_register_uart (1,1);		/* UART1 (big kahuna flow control serial port) */

	/*
	 * Set SUS bit in SDCR0 so serial port 1 acts as a UART.
	 * See Intel SA-1110 Developers Manual Section 11.9.2.1 (GPCLK/UART Select)
	 */
	Ser1SDCR0 |= SDCR0_SUS;
}

static int __init frodo_init_irq(void)
{
	int i,gpio[] = {
		FRODO_IDE_GPIO,
		FRODO_ETH_GPIO,
		FRODO_USB_DC_GPIO,
		FRODO_USB_HC_GPIO,
		FRODO_RTC_GPIO,
		FRODO_UART1_GPIO,
		FRODO_UART2_GPIO,
		FRODO_PCMCIA_STATUS_GPIO,
		FRODO_PCMCIA_RDYBSY_GPIO
	};

	for (i = 0; i < sizeof (gpio) / sizeof (gpio[0]); i++)
		set_GPIO_IRQ_edge (gpio[i],GPIO_RISING_EDGE);

	return (0);
}

__initcall(frodo_init_irq);
#if 0
static int __init frodo_init_cpld(void)
{
	if ((frodo_cpld_memory = ioremap (FRODO_CPLD_BASE,FRODO_CPLD_LENGTH)) == NULL)
		panic ("Couldn't map CPLD memory to a virtual address. We're screwed!\n");

	return (0);
}

__initcall(frodo_init_cpld);
#endif
u16 frodo_cpld_read (u32 reg)
{
	unsigned long flags;
	u16 value;

	spin_lock_irqsave (&frodo_cpld_lock,flags);
	value = frodo_cpld_memory[reg / 2];
	spin_unlock_irqrestore (&frodo_cpld_lock,flags);

	return (value);
}

void frodo_cpld_write (u32 reg,u16 value)
{
	unsigned long flags;

	spin_lock_irqsave (&frodo_cpld_lock,flags);
	frodo_cpld_memory[reg / 2] = value;
	spin_unlock_irqrestore (&frodo_cpld_lock,flags);
}

void frodo_cpld_set (u32 reg,u16 mask)
{
	unsigned long flags;

	spin_lock_irqsave (&frodo_cpld_lock,flags);
	frodo_cpld_memory[reg / 2] |= mask;
	spin_unlock_irqrestore (&frodo_cpld_lock,flags);
}

void frodo_cpld_clear (u32 reg,u16 mask)
{
	unsigned long flags;

	spin_lock_irqsave (&frodo_cpld_lock,flags);
	frodo_cpld_memory[reg / 2] &= ~mask;
	spin_unlock_irqrestore (&frodo_cpld_lock,flags);
}

EXPORT_SYMBOL (frodo_cpld_read);
EXPORT_SYMBOL (frodo_cpld_write);
EXPORT_SYMBOL (frodo_cpld_set);
EXPORT_SYMBOL (frodo_cpld_clear);

MACHINE_START (FRODO,"2d3D, Inc. SA-1110 Development Board")
	BOOT_MEM (0xc0000000,0x80000000,0xf8000000)
	BOOT_PARAMS (0xc0000100)
	MAPIO (frodo_map_io)
	INITIRQ (sa1100_init_irq)
MACHINE_END

