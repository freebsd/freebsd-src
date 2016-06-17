/*
 * linux/arch/arm/mach-sa1100/adsagc.c
 *
 * Pieces specific to the ADS Advanced Graphics Client board
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/serial_core.h>
#include <linux/delay.h>

#include <asm/hardware.h>
#include <asm/hardware/sa1111.h>
#include <asm/setup.h>
#include <asm/irq.h>

#include <asm/mach/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>

#include <asm/arch/irq.h>

#include "generic.h"
#include "sa1111.h"

static int __init adsagc_init(void)
{
	int ret;

	if (!machine_is_adsagc())
		return -ENODEV;

	/*
	 * Ensure that the memory bus request/grant signals are setup,
	 * and the grant is held in its inactive state
	 */
	sa1110_mb_disable();

	/* Adsagc uses GPIO pins for SPI interface to AVR
	 * AGC uses the standard pins instead.  AGC also has controls
	 * SA1111 and Smartio reset with GPIO.
	 */

	// Set RTS low during sleep
	PGSR |= GPIO_GPIO15 | GPIO_GPIO17 | GPIO_GPIO19;

	// Reset SA1111
	GPDR |= GPIO_GPIO12;
	GPCR = GPIO_GPIO12;
	udelay(1000);
	GPSR = GPIO_GPIO12;

	/* use the regular SSP pins */
	PPAR &= ~PPAR_SSPGPIO;
	/*
	 * Probe for SA1111.
	 */
	ret = sa1111_probe(ADS_SA1111_BASE);
	if (ret < 0)
		return ret;

	/*
	 * We found it.  Wake the chip up.
	 */
	sa1111_wake();

	/*
	 * The SDRAM configuration of the SA1110 and the SA1111 must
	 * match.  This is very important to ensure that SA1111 accesses
	 * don't corrupt the SDRAM.  Note that this ungates the SA1111's
	 * MBGNT signal, so we must have called sa1110_mb_disable()
	 * beforehand.
	 */
	sa1111_configure_smc(1,
			     FExtr(MDCNFG, MDCNFG_SA1110_DRAC0),
			     FExtr(MDCNFG, MDCNFG_SA1110_TDL0));

	/*
	 * Enable PWM control for LCD
	 */
	SKPCR |= SKPCR_PWMCLKEN;
	SACR1 &= ~SACR1_L3EN;
	ADS_CR3 |= ADS_CR3_BLON;                // Enable the backlight
	SKPWM1 = 0x7F;				// Backlight PWM
	SKPEN1 = 1;
	SKPWM0 = 0x7F;				// VEE
	SKPEN0 = 1;

	/*
	 * We only need to turn on DCLK whenever we want to use the
	 * DMA.  It can otherwise be held firmly in the off position.
	 */
	SKPCR |= SKPCR_DCLKEN;

	/*
	 * Enable the SA1110 memory bus request and grant signals.
	 */
	sa1110_mb_enable();

	set_GPIO_IRQ_edge(GPIO_GPIO0, GPIO_RISING_EDGE);
	sa1111_init_irq(IRQ_GPIO0);

	return 0;
}

__initcall(adsagc_init);

/*
 * Handlers for Adsagc's external IRQ logic
 */

#define ADSAGC_N_IRQ (IRQ_ADSAGC_END - IRQ_ADSAGC_START)

static void ADS_IRQ_demux( int irq, void *dev_id, struct pt_regs *regs )
{
	int i;

	while( (irq = ADS_INT_ST1 ) ){
		for( i = 0; i < ADSAGC_N_IRQ; i++ )
			if( irq & (1<<i) ) {
				do_IRQ( IRQ_ADSAGC_START + i, regs );
			}
	}
}

static struct irqaction ADS_ext_irq = {
	.name		= "ADS_ext_IRQ",
	.handler	= ADS_IRQ_demux,
	.flags		= SA_INTERRUPT
};

static void ADS_mask_and_ack_irq0(unsigned int irq)
{
	int mask = (1 << (irq - IRQ_ADSAGC_START));
	ADS_INT_EN1 &= ~mask;
	ADS_INT_ST1 = mask;
}

static void ADS_mask_irq0(unsigned int irq)
{
	ADS_INT_ST1 = (1 << (irq - IRQ_ADSAGC_START));
}

static void ADS_unmask_irq0(unsigned int irq)
{
	ADS_INT_EN1 |= (1 << (irq - IRQ_ADSAGC_START));
}

static void __init adsagc_init_irq(void)
{
	int irq;

	/* First the standard SA1100 IRQs */
	sa1100_init_irq();


	/* disable all IRQs */
	ADS_INT_EN1 = 0;

	/* clear all IRQs */
	ADS_INT_ST1 = 0xff;

	for (irq = IRQ_ADSAGC_START; irq < IRQ_ADSAGC_END; irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= ADS_mask_and_ack_irq0;
		irq_desc[irq].mask	= ADS_mask_irq0;
		irq_desc[irq].unmask	= ADS_unmask_irq0;
	}

	GPDR &= ~GPIO_GPIO10;
	set_GPIO_IRQ_edge(GPIO_GPIO10, GPIO_RISING_EDGE);  // this may change with final HW spin
	setup_arm_irq( IRQ_GPIO10, &ADS_ext_irq );
}


/*
 * Resume SA1111 when system wakes up
 */
void adsagc_sa1111_wake(void)
{
	// Reset SA1111
	GPDR |= GPIO_GPIO12;
	GPCR = GPIO_GPIO12;
	udelay(1000);
	GPSR = GPIO_GPIO12;
	sa1111_wake();

	SKPCR |= SKPCR_PWMCLKEN;
	SACR1 &= ~SACR1_L3EN;

	SKPWM1 = 0x7F;				// Backlight PWM
	SKPEN1 = 1;
	SKPWM0 = 0x7F;				// VEE
	SKPEN0 = 1;

	GAFR |= (GPIO_MBGNT | GPIO_MBREQ);
	GPDR |= GPIO_MBGNT;
	GPDR &= ~GPIO_MBREQ;
	TUCR |= TUCR_MR;

	sa1111_configure_smc(1,
			     FExtr(MDCNFG, MDCNFG_SA1110_DRAC0),
			     FExtr(MDCNFG, MDCNFG_SA1110_TDL0));
	SKPCR |= SKPCR_DCLKEN;

	// Reset PCMCIA
	PCCR = 0xFF;
	mdelay(100);
	PA_DDR = 0x00;
	PA_DWR = 0x05;
	PCCR = ~(PCCR_S0_RST | PCCR_S1_RST);

#ifdef CONFIG_USB_OHCI_SA1111
	// Turn ON clock
	SKPCR |= SKPCR_UCLKEN;
	udelay(100);

	// force a RESET
	USB_RESET = 0x01;
	USB_RESET |= 0x02;
	udelay(100);

	// Set Power Sense and Control Line
	USB_RESET = 0;
	USB_RESET = USB_RESET_PWRSENSELOW;
	USB_STATUS = 0;
	udelay(10);
#endif
}

static struct map_desc adsagc_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x08000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash bank 1 */
  { 0xf2000000, 0x40000000, 0x00004000, DOMAIN_IO, 0, 1, 0, 0 }, /* 91C1111 */
  { 0xf3000000, 0x48000000, 0x00340000, DOMAIN_IO, 0, 1, 0, 0 }, /* S1D13806 */
  /* note: CPLD includes CAN in Advanced Graphics Client */
  { 0xf0000000, 0x10000000, 0x00400000, DOMAIN_IO, 0, 1, 0, 0 }, /* CPLD */
  { 0xf4000000, 0x18000000, 0x00800000, DOMAIN_IO, 0, 1, 0, 0 }, /* SA-1111 */
  LAST_DESC
};

static int adsagc_uart_open(struct uart_port *port, struct uart_info *info)
{
	int	ret = 0;

	if (port->mapbase == _Ser1UTCR0) {
		Ser1SDCR0 |= SDCR0_UART;
	}
	else if (port->mapbase == _Ser2UTCR0) {
		Ser2UTCR4 = Ser2HSCR0 = 0;
	}
	return ret;
}

static u_int adsagc_get_mctrl(struct uart_port *port)
{
	u_int result = TIOCM_CD | TIOCM_DSR;

	if (port->mapbase == _Ser1UTCR0) {
		if (!(GPLR & GPIO_GPIO14))
			result |= TIOCM_CTS;
	} else if (port->mapbase == _Ser2UTCR0) {
		if (!(GPLR & GPIO_GPIO16))
			result |= TIOCM_CTS;
	} else if (port->mapbase == _Ser3UTCR0) {
		if (!(GPLR & GPIO_GPIO18))
			result |= TIOCM_CTS;
	} else {
		result = TIOCM_CTS;
	}

	return result;
}

static void adsagc_set_mctrl(struct uart_port *port, u_int mctrl)
{
	if (port->mapbase == _Ser1UTCR0) {
		if (mctrl & TIOCM_RTS)
			GPCR = GPIO_GPIO15;
		else
			GPSR = GPIO_GPIO15;
	} else if (port->mapbase == _Ser2UTCR0) {
		if (mctrl & TIOCM_RTS)
			GPCR = GPIO_GPIO17;
		else
			GPSR = GPIO_GPIO17;
	} else if (port->mapbase == _Ser3UTCR0) {
		if (mctrl & TIOCM_RTS)
			GPCR = GPIO_GPIO19;
		else
			GPSR = GPIO_GPIO19;
	}
}

static void
adsagc_uart_pm(struct uart_port *port, u_int state, u_int oldstate)
{
	// state has ACPI D0-D3
	// ACPI D0 	  : resume from suspend
	// ACPI D1-D3 : enter to a suspend state
	if (port->mapbase == _Ser1UTCR0) {
		if (state) {
                        // disable uart
                        Ser1UTCR3 = 0;
		}
	}
	else if (port->mapbase == _Ser2UTCR0) {
		if (state) {
                        // disable uart
                        Ser2UTCR3 = 0;
                        Ser2HSCR0 = 0;
		}
	}
	else if (port->mapbase == _Ser3UTCR0) {
		if (state) {
                        // disable uart
                        Ser3UTCR3 = 0;
		}
	}
}

static struct sa1100_port_fns adsagc_port_fns __initdata = {
	.open		= adsagc_uart_open,
	.get_mctrl	= adsagc_get_mctrl,
	.set_mctrl	= adsagc_set_mctrl,
	.pm		= adsagc_uart_pm,
};

static void __init adsagc_map_io(void)
{
	sa1100_map_io();
	iotable_init(adsagc_io_desc);

	sa1100_register_uart_fns(&adsagc_port_fns);
	sa1100_register_uart(0, 3);
	sa1100_register_uart(1, 1);
	// don't register if you want to use IRDA
#ifndef CONFIG_SA1100_FIR
	sa1100_register_uart(2, 2);
#endif

	/* set GPDR now */
	GPDR |= GPIO_GPIO15 | GPIO_GPIO17 | GPIO_GPIO19;
       	GPDR &= ~(GPIO_GPIO14 | GPIO_GPIO16 | GPIO_GPIO18);
}

MACHINE_START(ADSAGC, "ADS Advanced GraphicsClient")
        BOOT_PARAMS(0xc000003c)
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	MAPIO(adsagc_map_io)
	INITIRQ(adsagc_init_irq)
MACHINE_END
