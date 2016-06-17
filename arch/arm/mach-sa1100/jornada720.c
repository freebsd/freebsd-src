/*
 * linux/arch/arm/mach-sa1100/jornada720.c
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/delay.h>

#include <asm/hardware.h>
#include <asm/hardware/sa1111.h>
#include <asm/irq.h>
#include <asm/setup.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>

#include "generic.h"
#include "sa1111.h"


#define JORTUCR_VAL	0x20000400
#define JORSKCR_INIT	0x00002081	/* Turn off VCO to enable PLL, set Ready En and enable nOE assertion from DC */
#define JORSKCR_RCLK	0x00002083	/* Add turning on RCLK to above */
#define JORSKCR_VAL	0x0000001B	/* sets the 1101 control register to on */

static int __init jornada720_init(void)
{
	GPDR |= GPIO_GPIO20;
	TUCR = JORTUCR_VAL;	/* set the oscillator out to the SA-1101 */

	GPSR = GPIO_GPIO20;
	udelay(1);
	GPCR = GPIO_GPIO20;
	udelay(1);
	GPSR = GPIO_GPIO20;
	udelay(20);
	SBI_SKCR = JORSKCR_INIT;/* Turn on the PLL, enable Ready and enable nOE assertion from DC */
	mdelay(100);

	SBI_SKCR = JORSKCR_RCLK;/* turn on the RCLOCK */
	SBI_SMCR = 0x35;	/* initialize the SMC (debug SA-1111 reset */
	PCCR = 0;		/* initialize the S2MC (debug SA-1111 reset) */

	/* LDD4 is speaker, LDD3 is microphone */
	PPSR &= ~(PPC_LDD3 | PPC_LDD4);
	PPDR |= PPC_LDD3 | PPC_LDD4;

	/* initialize extra IRQs */
	set_GPIO_IRQ_edge(GPIO_GPIO1, GPIO_RISING_EDGE);
	sa1111_init_irq(IRQ_GPIO1);	/* chained on GPIO 1 */

	return 0;
}

__initcall(jornada720_init);


static void __init
fixup_jornada720(struct machine_desc *desc, struct param_struct *params,
		 char **cmdline, struct meminfo *mi)
{
	SET_BANK( 0, 0xc0000000, 32*1024*1024 );
	mi->nr_banks = 1;
}

static struct map_desc jornada720_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x00000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash bank 0 */
  { 0xf0000000, 0x48000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* Epson registers */
  { 0xf1000000, 0x48200000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* Epson frame buffer */
  { 0xf4000000, 0x40000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* SA-1111 */
  LAST_DESC
};

static void __init jornada720_map_io(void)
{
	sa1100_map_io();
	iotable_init(jornada720_io_desc);
	
	sa1100_register_uart(0, 3);
	sa1100_register_uart(1, 1);
}

MACHINE_START(JORNADA720, "HP Jornada 720")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	BOOT_PARAMS(0xc0000100)
	FIXUP(fixup_jornada720)
	MAPIO(jornada720_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
