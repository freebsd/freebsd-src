/*
 * linux/arch/arm/mach-sa1100/itsy.c
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
fixup_itsy(struct machine_desc *desc, struct param_struct *params,
	   char **cmdline, struct meminfo *mi)
{
	SET_BANK( 0, 0xc0000000, 16*1024*1024 );
	SET_BANK( 1, 0xc8000000, 16*1024*1024 );
	SET_BANK( 2, 0xd0000000, 16*1024*1024 );
	SET_BANK( 3, 0xd8000000, 16*1024*1024 );
	mi->nr_banks = 4;
}

/* BRADFIXME The egpio addresses aren't verifiably correct. (i.e. they're most
   likely wrong. */
static struct map_desc itsy_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x00000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash bank 0 */
  { 0xf0000000, 0x49000000, 0x01000000, DOMAIN_IO, 0, 1, 0, 0 }, /* EGPIO 0 */
  LAST_DESC
};

static void __init itsy_map_io(void)
{
	sa1100_map_io();
	iotable_init(itsy_io_desc);

	sa1100_register_uart(0, 3);
	sa1100_register_uart(1, 1);
	sa1100_register_uart(2, 2);
}

MACHINE_START(ITSY, "Compaq Itsy")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	BOOT_PARAMS(0xc0000100)
	FIXUP(fixup_itsy)
	MAPIO(itsy_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
