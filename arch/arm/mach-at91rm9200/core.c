/*
 * linux/arch/arm/mach-at91rm9200/core.c
 *
 *  Copyright (c) 2003 SAN People
 *  Copyright (c) 2003 ATMEL
 *  Copyright (c) Rick Bronson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/mm.h>

#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/arch/hardware.h>

/*
 * System peripheral registers mapped at virtual address.
 */
AT91PS_SYS AT91_SYS = (AT91PS_SYS) AT91C_VA_BASE_SYS;

static struct map_desc at91rm9200_io_desc[] __initdata = {
	/* virtual,             physical,          length,   domain,    r, w, c, b */
	{ AT91C_VA_BASE_SYS,    AT91C_BASE_SYS,    SZ_4K,    DOMAIN_IO, 0, 1, 0, 0},
	{ AT91C_VA_BASE_SPI,    AT91C_BASE_SPI,    SZ_16K,   DOMAIN_IO, 0, 1, 0, 0},
	{ AT91C_VA_BASE_SSC2,   AT91C_BASE_SSC2,   SZ_16K,   DOMAIN_IO, 0, 1, 0, 0},
	{ AT91C_VA_BASE_SSC1,   AT91C_BASE_SSC1,   SZ_16K,   DOMAIN_IO, 0, 1, 0, 0},
	{ AT91C_VA_BASE_SSC0,   AT91C_BASE_SSC0,   SZ_16K,   DOMAIN_IO, 0, 1, 0, 0},
	{ AT91C_VA_BASE_US3,    AT91C_BASE_US3,    SZ_16K,   DOMAIN_IO, 0, 1, 0, 0},
	{ AT91C_VA_BASE_US2,    AT91C_BASE_US2,    SZ_16K,   DOMAIN_IO, 0, 1, 0, 0},
	{ AT91C_VA_BASE_US1,    AT91C_BASE_US1,    SZ_16K,   DOMAIN_IO, 0, 1, 0, 0},
	{ AT91C_VA_BASE_US0,    AT91C_BASE_US0,    SZ_16K,   DOMAIN_IO, 0, 1, 0, 0},
	{ AT91C_VA_BASE_EMAC,   AT91C_BASE_EMAC,   SZ_16K,   DOMAIN_IO, 0, 1, 0, 0},
	{ AT91C_VA_BASE_TWI,    AT91C_BASE_TWI,    SZ_16K,   DOMAIN_IO, 0, 1, 0, 0},
	{ AT91C_VA_BASE_MCI,    AT91C_BASE_MCI,    SZ_16K,   DOMAIN_IO, 0, 1, 0, 0},
	{ AT91C_VA_BASE_UDP,    AT91C_BASE_UDP,    SZ_16K,   DOMAIN_IO, 0, 1, 0, 0},
	{ AT91C_VA_BASE_TCB1,   AT91C_BASE_TCB1,   SZ_16K,   DOMAIN_IO, 0, 1, 0, 0},
	{ AT91C_VA_BASE_TCB0,   AT91C_BASE_TCB0,   SZ_16K,   DOMAIN_IO, 0, 1, 0, 0},
	LAST_DESC
};

/* Interrupt configuration */
static AT91_REG at91rm9200_irq_smr[] __initdata = {
	(AT91_SMR_FIQ),		/* FIQ */
	(AT91_SMR_SYS),		/* System Peripherals */
	(AT91_SMR_PIOA),	/* PIO A */
	(AT91_SMR_PIOB),	/* PIO B */
	(AT91_SMR_PIOC),	/* PIO C */
	(AT91_SMR_PIOD),	/* PIO D */
	(AT91_SMR_US0),		/* USART 0 */
	(AT91_SMR_US1),		/* USART 1 */
	(AT91_SMR_US2),		/* USART 2 */
	(AT91_SMR_US3),		/* USART 3 */
	(AT91_SMR_MCI),		/* Multimedia Card */
	(AT91_SMR_UDP),		/* USB Device */
	(AT91_SMR_TWI),		/* Two-wire interface */
	(AT91_SMR_SPI),		/* SPI */
	(AT91_SMR_SSC0),	/* Sync Serial 0 */
	(AT91_SMR_SSC1),	/* Sync Serial 1 */
	(AT91_SMR_SSC2),	/* Sync Serial 2 */
	(AT91_SMR_TC0),		/* TC 0 */
	(AT91_SMR_TC1),		/* TC 1 */
	(AT91_SMR_TC2),		/* TC 2 */
	(AT91_SMR_TC3),		/* TC 3 */
	(AT91_SMR_TC4),		/* TC 4 */
	(AT91_SMR_TC5),		/* TC 5 */
	(AT91_SMR_UHP),		/* USB Host */
	(AT91_SMR_EMAC),	/* Ethernet */
	(AT91_SMR_IRQ0),	/* IRQ 0 */
	(AT91_SMR_IRQ1),	/* IRQ 1 */
	(AT91_SMR_IRQ2),	/* IRQ 2 */
	(AT91_SMR_IRQ3),	/* IRQ 3 */
	(AT91_SMR_IRQ4),	/* IRQ 4 */
	(AT91_SMR_IRQ5),	/* IRQ 5 */
	(AT91_SMR_IRQ6)		/* IRQ 6 */
};

/* Architecture-specific fixups */
static void __init at91rm9200_fixup(struct machine_desc *desc, struct param_struct *unused,
		 char **cmdline, struct meminfo *mi)
{
#ifdef CONFIG_BLK_DEV_INITRD
	ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	setup_ramdisk(1, 0, 0, CONFIG_BLK_DEV_RAM_SIZE);
//	setup_initrd(0xc0100000, 3*1024*1024);
#endif
}

extern void at91_register_uart(int idx, int port);

void __init at91rm9200_map_io(void)
{
	int serial[AT91C_NR_UART] = AT91C_UART_MAP;
	int i;

	iotable_init(at91rm9200_io_desc);

	/* Register UARTs */
	for (i = 0; i < AT91C_NR_UART; i++) {
		if (serial[i] >= 0)
			at91_register_uart(i, serial[i]);
	}
}

static void at91rm9200_mask_irq(unsigned int irq)
{
	/* Disable interrupt on AIC */
	AT91_SYS->AIC_IDCR =  1 << irq;
}

static void at91rm9200_unmask_irq(unsigned int irq)
{
	/* Enable interrupt on AIC */
	AT91_SYS->AIC_IECR =  1 << irq;
}

void __init at91rm9200_init_irq(void)
{
	unsigned int i;

	/*
	 * The IVR is used by macro get_irqnr_and_base to read and verify.
	 * The irq number is NR_IRQS when a spurious interrupt has occured.
	 */
	for (i = 0; i < NR_IRQS; i++) {
		/* Put irq number in Source Vector Register: */
		AT91_SYS->AIC_SVR[i] = i;
		/* Store the Source Mode Register as defined in table above */
		AT91_SYS->AIC_SMR[i] = at91rm9200_irq_smr[i];

		irq_desc[i].valid	= 1;
		irq_desc[i].mask_ack	= at91rm9200_mask_irq;
		irq_desc[i].mask	= at91rm9200_mask_irq;
		irq_desc[i].unmask	= at91rm9200_unmask_irq;

		/* Perform 8 End Of Interrupt Command to make sure AIC will not Lock out nIRQ */
		if (i < 8)
			AT91_SYS->AIC_EOICR = AT91_SYS->AIC_EOICR;
	}

	/* Spurious Interrupt ID in Spurious Vector Register is NR_IRQS
	 * When there is no current interrupt, the IRQ Vector Register reads the value stored in AIC_SPU
	 */
	AT91_SYS->AIC_SPU = NR_IRQS;

	/* No debugging in AIC: Debug (Protect) Control Register */
	AT91_SYS->AIC_DCR = 0;

	/* Disable and clear all interrupts initially */
	AT91_SYS->AIC_IDCR = 0xFFFFFFFF;
	AT91_SYS->AIC_ICCR = 0xFFFFFFFF;
}

MACHINE_START(AT91RM9200, "ATMEL AT91RM9200")
	MAINTAINER("SAN People / ATMEL")
	BOOT_MEM(AT91_SDRAM_BASE, AT91C_BASE_SYS, AT91C_VA_BASE_SYS)
	BOOT_PARAMS(AT91_SDRAM_BASE + 0x100)
	FIXUP(at91rm9200_fixup)
	MAPIO(at91rm9200_map_io)
	INITIRQ(at91rm9200_init_irq)
MACHINE_END
