/*
 * linux/arch/arm/mach-sa1100/nanoengine.c
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>

#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>

#include "generic.h"


static struct map_desc nanoengine_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x00000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash bank 0 */
  { 0xf0000000, 0x10000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* System Registers */
  { 0xf1000000, 0x18A00000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* Internal PCI Config Space */
  LAST_DESC
};

static void __init nanoengine_map_io(void)
{
	sa1100_map_io();
	iotable_init(nanoengine_io_desc);

	sa1100_register_uart(0, 1);
	sa1100_register_uart(1, 2);
	sa1100_register_uart(2, 3);
	Ser1SDCR0 |= SDCR0_UART;
	/* disable IRDA -- UART2 is used as a normal serial port */
	Ser2UTCR4=0;
	Ser2HSCR0 = 0;
}

MACHINE_START(NANOENGINE, "BSE nanoEngine")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	BOOT_PARAMS(0xc0000100)
	MAPIO(nanoengine_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
