/*
 * linux/arch/arm/mach-sa1100/freebird.c
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>

#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>

#include "generic.h"


unsigned long BCR_value = BCR_DB1110;
EXPORT_SYMBOL(BCR_value);


static void __init
fixup_freebird(struct machine_desc *desc, struct param_struct *params,
	       char **cmdline, struct meminfo *mi)
{
#ifdef CONFIG_SA1100_FREEBIRD_OLD
	SET_BANK( 0, 0xc0000000, 32*1024*1024 );
	mi->nr_banks = 1;
	ROOT_DEV = MKDEV(RAMDISK_MAJOR,0);
	setup_ramdisk( 1, 0 ,0 , 8192 );
	setup_initrd( 0xc0800000, 3*1024*1024 );
#endif
}

static struct map_desc freebird_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x00000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash bank 0 */
  { 0xf0000000, 0x12000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* Board Control Register */
  { 0xf2000000, 0x19000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0},
   LAST_DESC
};

static void __init freebird_map_io(void)
{
	sa1100_map_io();
	iotable_init(freebird_io_desc);

	sa1100_register_uart(0, 3);
	sa1100_register_uart(1, 1);

	/* Set up sleep mode registers */
	PWER = 0x1;
	PGSR = 0x0;
	PCFR = PCFR_OPDE | PCFR_FP | PCFR_FS;
}

MACHINE_START(FREEBIRD, "Freebird-HPC-1.1")
	BOOT_MEM(0xc0000000,0x80000000, 0xf8000000)
#ifdef CONFIG_SA1100_FREEBIRD_NEW
	BOOT_PARAMS(0xc0000100)
#endif
	FIXUP(fixup_freebird)
	MAPIO(freebird_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
