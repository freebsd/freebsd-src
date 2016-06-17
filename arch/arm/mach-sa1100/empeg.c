/*
 * linux/arch/arm/mach-sa1100/empeg.c
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
fixup_empeg(struct machine_desc *desc, struct param_struct *params,
	    char **cmdline, struct meminfo *mi)
{
	SET_BANK( 0, 0xc0000000, 4*1024*1024 );
	SET_BANK( 1, 0xc8000000, 4*1024*1024 );
	mi->nr_banks = 2;

	ROOT_DEV = MKDEV( 3, 1 );  /* /dev/hda1 */
	setup_ramdisk( 1, 0, 0, 4096 );
	setup_initrd( 0xd0000000+((1024-320)*1024), (320*1024) );
}

static struct map_desc empeg_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { EMPEG_FLASHBASE, 0x00000000, 0x00200000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash */
  LAST_DESC
};

static void __init empeg_map_io(void)
{
	sa1100_map_io();
	iotable_init(empeg_io_desc);

	sa1100_register_uart(0, 1);
	sa1100_register_uart(1, 3);
	sa1100_register_uart(2, 2);
	Ser1SDCR0 |= SDCR0_UART;
}

MACHINE_START(EMPEG, "empeg MP3 Car Audio Player")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	FIXUP(fixup_empeg)
	MAPIO(empeg_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
