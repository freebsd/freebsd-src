/*
 * linux/arch/arm/mach-sa1100/shannon.c
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>

#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>

#include "generic.h"


static struct map_desc shannon_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x00000000, 0x00400000, DOMAIN_IO, 0, 1, 0, 0 }, /* main flash memory */
  LAST_DESC
};

static void __init shannon_map_io(void)
{
	sa1100_map_io();
	iotable_init(shannon_io_desc);

	sa1100_register_uart(0, 3);
	sa1100_register_uart(1, 1);
	Ser1SDCR0 |= SDCR0_SUS;
	GAFR |= (GPIO_UART_TXD | GPIO_UART_RXD);
	GPDR |= GPIO_UART_TXD;
	GPDR &= ~GPIO_UART_RXD;
	PPAR |= PPAR_UPR;

	set_GPIO_IRQ_edge(SHANNON_GPIO_IRQ_CODEC);
}

MACHINE_START(SHANNON, "Shannon (AKA: Tuxscreen)")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	BOOT_PARAMS(0xc0000100)
	MAPIO(shannon_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
