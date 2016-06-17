/*
 * linux/arch/arm/mach-sa1100/pleb.c
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

static void __init
fixup_pleb(struct machine_desc *desc, struct param_struct *params,
           char **cmdline, struct meminfo *mi)
{
	SET_BANK(0, 0xc0000000, 16*1024*1024);
	SET_BANK(1, 0xc8000000, 16*1024*1024);
	SET_BANK(2, 0xd0000000, 16*1024*1024);
	SET_BANK(3, 0xd8000000, 16*1024*1024);

	/* make this 4 a second memory card is used to make 64MB */
	/* make it 1 if a 16MB memory card is used */
	mi->nr_banks = 2; /* Default 32MB */

	ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	setup_ramdisk(1, 0, 0, 8192);
	setup_initrd(0xc0400000, 4*1024*1024);
}

static struct map_desc pleb_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x00000000, 0x00400000, DOMAIN_IO, 0, 1, 0, 0 }, /* main flash memory */
  { 0xe8400000, 0x08000000, 0x00400000, DOMAIN_IO, 0, 1, 0, 0 }, /* main flash, alternative location */
  LAST_DESC
};

static void __init pleb_map_io(void)
{
	sa1100_map_io();
	iotable_init(pleb_io_desc);

	sa1100_register_uart(0, 3);
        sa1100_register_uart(1, 1);
        GAFR |= (GPIO_UART_TXD | GPIO_UART_RXD);
        GPDR |= GPIO_UART_TXD;
        GPDR &= ~GPIO_UART_RXD;
        PPAR |= PPAR_UPR;
}

MACHINE_START(PLEB, "PLEB")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	FIXUP(fixup_pleb)
	MAPIO(pleb_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
