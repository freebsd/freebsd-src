/*
 * linux/arch/arm/mach-sa1100/sherman.c
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
fixup_sherman(struct machine_desc *desc, struct param_struct *params,
	      char **cmdline, struct meminfo *mi)
{
	SET_BANK( 0, 0xc0000000, 64*1024*1024 );
	SET_BANK( 1, 0xc8000000, 64*1024*1024 );
	mi->nr_banks = 2;

	ROOT_DEV = MKDEV( 60, 2 );
	setup_ramdisk( 1, 0, 0, 8192 );
//	setup_initrd( 0xc0400000, 8*1024*1024 );
}

static struct map_desc sherman_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x00000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash*/
  LAST_DESC
};

static void __init sherman_map_io(void)
{
	sa1100_map_io();
	iotable_init(sherman_io_desc);

	sa1100_register_uart(0, 3);
	sa1100_register_uart(1, 1);
}

MACHINE_START(SHERMAN, "Blazie Engineering Sherman")
        BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
        FIXUP(fixup_sherman)
        MAPIO(sherman_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
