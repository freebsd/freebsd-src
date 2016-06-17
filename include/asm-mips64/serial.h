/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1999 by Ralf Baechle
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 */
#ifndef _ASM_SERIAL_H
#define _ASM_SERIAL_H

#include <linux/config.h>

/*
 * This assumes you have a 1.8432 MHz clock for your UART.
 *
 * It'd be nice if someone built a serial card with a 24.576 MHz
 * clock, since the 16550A is capable of handling a top speed of 1.5
 * megabits/second; but this requires the faster clock.
 */
#define BASE_BAUD (1843200 / 16)

/* Standard COM flags (except for COM4, because of the 8514 problem) */
#ifdef CONFIG_SERIAL_DETECT_IRQ
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST | ASYNC_AUTO_IRQ)
#define STD_COM4_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_AUTO_IRQ)
#else
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)
#define STD_COM4_FLAGS ASYNC_BOOT_AUTOCONF
#endif

#ifdef CONFIG_HAVE_STD_PC_SERIAL_PORT

#define STD_SERIAL_PORT_DEFNS			\
	/* UART CLK   PORT IRQ     FLAGS        */			\
	{ 0, BASE_BAUD, 0x3F8, 4, STD_COM_FLAGS },	/* ttyS0 */	\
	{ 0, BASE_BAUD, 0x2F8, 3, STD_COM_FLAGS },	/* ttyS1 */	\
	{ 0, BASE_BAUD, 0x3E8, 4, STD_COM_FLAGS },	/* ttyS2 */	\
	{ 0, BASE_BAUD, 0x2E8, 3, STD_COM4_FLAGS },	/* ttyS3 */

#else /* CONFIG_HAVE_STD_PC_SERIAL_PORTS */
#define STD_SERIAL_PORT_DEFNS
#endif /* CONFIG_HAVE_STD_PC_SERIAL_PORTS */

#ifdef CONFIG_MIPS_SEAD
#include <asm/mips-boards/sead.h>
#include <asm/mips-boards/seadint.h>
#define SEAD_SERIAL_PORT_DEFNS                  \
	/* UART CLK   PORT IRQ     FLAGS        */                      \
	{ 0, SEAD_BASE_BAUD, SEAD_UART0_REGS_BASE, SEADINT_UART0, STD_COM_FLAGS },     /* ttyS0 */
#else
#define SEAD_SERIAL_PORT_DEFNS
#endif

#ifdef CONFIG_MOMENCO_OCELOT_C
/* Ordinary NS16552 duart with a 20MHz crystal.  */
#define OCELOT_C_BASE_BAUD ( 20000000 / 16 )

#define OCELOT_C_SERIAL1_IRQ	80
#define OCELOT_C_SERIAL1_BASE	0xfffffffffd000020

#define OCELOT_C_SERIAL2_IRQ	81
#define OCELOT_C_SERIAL2_BASE	0xfffffffffd000000

#define _OCELOT_C_SERIAL_INIT(int, base)				\
	{ baud_base: OCELOT_C_BASE_BAUD, irq: int, flags: (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST),\
	  iomem_base: (u8 *) base, iomem_reg_shift: 2,			\
	  io_type: SERIAL_IO_MEM }
#define MOMENCO_OCELOT_C_SERIAL_PORT_DEFNS				\
	_OCELOT_C_SERIAL_INIT(OCELOT_C_SERIAL1_IRQ, OCELOT_C_SERIAL1_BASE), \
	_OCELOT_C_SERIAL_INIT(OCELOT_C_SERIAL2_IRQ, OCELOT_C_SERIAL2_BASE)
#else
#define MOMENCO_OCELOT_C_SERIAL_PORT_DEFNS
#endif

#ifdef CONFIG_MOMENCO_JAGUAR_ATX
/* Ordinary NS16552 duart with a 20MHz crystal.  */
#define JAGUAR_ATX_BASE_BAUD ( 20000000 / 16 )

#define JAGUAR_ATX_SERIAL1_IRQ	7
#define JAGUAR_ATX_SERIAL1_BASE	0xfffffffffd000020

#define _JAGUAR_ATX_SERIAL_INIT(int, base)				 \
	{ baud_base: JAGUAR_ATX_BASE_BAUD, irq: int, flags: (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST),\
	  iomem_base: (u8 *) base, iomem_reg_shift: 2,			 \
	  io_type: SERIAL_IO_MEM }
#define MOMENCO_JAGUAR_ATX_SERIAL_PORT_DEFNS				\
	_JAGUAR_ATX_SERIAL_INIT(JAGUAR_ATX_SERIAL1_IRQ, JAGUAR_ATX_SERIAL1_BASE)
#else
#define MOMENCO_JAGUAR_ATX_SERIAL_PORT_DEFNS
#endif

#ifdef CONFIG_TITAN_SERIAL
/* 16552 20 MHz crystal */
#define TITAN_SERIAL_BASE_BAUD	( 20000000 / 16 )
#define	TITAN_SERIAL_IRQ	XXX
#define	TITAN_SERIAL_BASE	0xffffffff

#define	_TITAN_SERIAL_INIT(int, base)					\
	{ baud_base: TITAN_SERIAL_BASE_BAUD, irq: int,			\
	  flags: STD_COM_FLAGS,	iomem_base: (u8 *) base,		\
	  iomem_reg_shift: 2, io_type: SERIAL_IO_MEM			\
	}

#define TITAN_SERIAL_PORT_DEFNS						\
	_TITAN_SERIAL_INIT(TITAN_SERIAL_IRQ, TITAN_SERIAL_BASE)
#else
#define TITAN_SERIAL_PORT_DEFNS
#endif

#ifdef CONFIG_SGI_IP27

/*
 * Note about serial ports and consoles:
 * For console output, everyone uses the IOC3 UARTA (offset 0x178)
 * connected to the master node (look in ip27_setup_console() and
 * ip27prom_console_write()).
 *
 * For serial (/dev/ttyS0 etc), we can not have hardcoded serial port
 * addresses on a partitioned machine. Since we currently use the ioc3
 * serial ports, we use dynamic serial port discovery that the serial.c
 * driver uses for pci/pnp ports (there is an entry for the SGI ioc3
 * boards in pci_boards[]). Unfortunately, UARTA's pio address is greater
 * than UARTB's, although UARTA on o200s has traditionally been known as
 * port 0. So, we just use one serial port from each ioc3 (since the
 * serial driver adds addresses to get to higher ports).
 *
 * The first one to do a register_console becomes the preferred console
 * (if there is no kernel command line console= directive). /dev/console
 * (ie 5, 1) is then "aliased" into the device number returned by the
 * "device" routine referred to in this console structure
 * (ip27prom_console_dev).
 *
 * Also look in ip27-pci.c:pci_fixuop_ioc3() for some comments on working
 * around ioc3 oddities in this respect.
 *
 * The IOC3 serials use a 22MHz clock rate with an additional divider by 3.
 * (IOC3_BAUD = (22000000 / (3*16)))
 *
 * At the moment this is only a skeleton definition as we register all serials
 * at runtime.
 */

#define IP27_SERIAL_PORT_DEFNS
#else
#define IP27_SERIAL_PORT_DEFNS
#endif /* CONFIG_SGI_IP27 */

#define SERIAL_PORT_DFNS				\
	IP27_SERIAL_PORT_DEFNS				\
	MOMENCO_OCELOT_C_SERIAL_PORT_DEFNS		\
	MOMENCO_JAGUAR_ATX_SERIAL_PORT_DEFNS		\
	SEAD_SERIAL_PORT_DEFNS				\
	STD_SERIAL_PORT_DEFNS				\
	TITAN_SERIAL_PORT_DEFNS

#define RS_TABLE_SIZE	64

#endif /* _ASM_SERIAL_H */
