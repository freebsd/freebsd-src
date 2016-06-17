/*
 * linux/arch/arm/mach-sa1100/xp860.c
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/tty.h>

#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>

#include "generic.h"
#include "sa1111.h"


static void xp860_power_off(void)
{
	cli();
	GPDR |= GPIO_GPIO20;
	GPSR = GPIO_GPIO20;
	mdelay(1000);
	GPCR = GPIO_GPIO20;
	while(1);
}

/*
 * Note: I replaced the sa1111_init() without the full SA1111 initialisation
 * because this machine doesn't appear to use the DMA features.  If this is
 * wrong, please look at neponset.c to fix it properly.
 */
static int __init xp860_init(void)
{
	pm_power_off = xp860_power_off;

	/*
	 * Probe for SA1111.
	 */
	ret = sa1111_probe(0x40000000);
	if (ret < 0)
		return ret;

	/*
	 * We found it.  Wake the chip up.
	 */
	sa1111_wake();

	return 0;
}

__initcall(xp860_init);


static void __init
fixup_xp860(struct machine_desc *desc, struct param_struct *params,
	    char **cmdline, struct meminfo *mi)
{
	SET_BANK( 0, 0xc0000000, 32*1024*1024 );
	mi->nr_banks = 1;
}

static struct map_desc xp860_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xf0000000, 0x10000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* SCSI */
  { 0xf1000000, 0x18000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* LAN */
  { 0xf4000000, 0x40000000, 0x00800000, DOMAIN_IO, 0, 1, 0, 0 }, /* SA-1111 */
  LAST_DESC
};

static void __init xp860_map_io(void)
{
	sa1100_map_io();
	iotable_init(xp860_io_desc);

	sa1100_register_uart(0, 3);
	sa1100_register_uart(1, 1);
}

MACHINE_START(XP860, "XP860")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	FIXUP(fixup_xp860)
	MAPIO(xp860_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
