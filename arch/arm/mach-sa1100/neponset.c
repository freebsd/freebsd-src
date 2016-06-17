/*
 * linux/arch/arm/mach-sa1100/neponset.c
 *
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/tty.h>
#include <linux/ioport.h>
#include <linux/serial_core.h>
#include <linux/list.h>
#include <linux/timer.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>
#include <asm/arch/irq.h>
#include <asm/mach/serial_sa1100.h>
#include <asm/arch/assabet.h>
#include <asm/hardware/sa1111.h>

#include "sa1111.h"


/*
 * Install handler for Neponset IRQ.  Yes, yes... we are way down the IRQ
 * cascade which is not good for IRQ latency, but the hardware has been
 * designed that way...
 */

static void neponset_IRQ_demux( int irq, void *dev_id, struct pt_regs *regs )
{
	int irr;

	for(;;){
		irr = IRR & (IRR_ETHERNET | IRR_USAR | IRR_SA1111);
		/* Let's have all active IRQ bits high.
		 * Note: there is a typo in the Neponset user's guide
		 * for the SA1111 IRR level.
		 */
		irr ^= (IRR_ETHERNET | IRR_USAR);
		if (!irr) break;

		if( irr & IRR_ETHERNET )
			do_IRQ(IRQ_NEPONSET_SMC9196, regs);

		if( irr & IRR_USAR )
			do_IRQ(IRQ_NEPONSET_USAR, regs);

		if( irr & IRR_SA1111 )
			sa1111_IRQ_demux(irq, dev_id, regs);
	}
}

static struct irqaction neponset_irq = {
	.name		= "Neponset",
	.handler	= neponset_IRQ_demux,
	.flags		= SA_INTERRUPT
};

static void __init neponset_init_irq(void)
{
	sa1111_init_irq(-1);	/* SA1111 IRQ not routed to a GPIO */

	/* setup extra Neponset IRQs */
	irq_desc[IRQ_NEPONSET_SMC9196].valid	= 1;
	irq_desc[IRQ_NEPONSET_SMC9196].probe_ok	= 1;
	irq_desc[IRQ_NEPONSET_USAR].valid	= 1;
	irq_desc[IRQ_NEPONSET_USAR].probe_ok	= 1;

	set_GPIO_IRQ_edge(GPIO_GPIO25, GPIO_RISING_EDGE);
	setup_arm_irq(IRQ_GPIO25, &neponset_irq);
}

static int __init neponset_init(void)
{
	int ret;

	/*
	 * The Neponset is only present on the Assabet machine type.
	 */
	if (!machine_is_assabet())
		return -ENODEV;

	/*
	 * Ensure that the memory bus request/grant signals are setup,
	 * and the grant is held in its inactive state, whether or not
	 * we actually have a Neponset attached.
	 */
	sa1110_mb_disable();

	if (!machine_has_neponset()) {
		printk(KERN_DEBUG "Neponset expansion board not present\n");
		return -ENODEV;
	}

	if (WHOAMI != 0x11) {
		printk(KERN_WARNING "Neponset board detected, but "
			"wrong ID: %02x\n", WHOAMI);
		return -ENODEV;
	}

	/*
	 * Disable GPIO 0/1 drivers so the buttons work on the module.
	 */
	NCR_0 |= NCR_GP01_OFF;

	/*
	 * Neponset has SA1111 connected to CS4.  We know that after
	 * reset the chip will be configured for variable latency IO.
	 */
	/* FIXME: setup MSC2 */

	/*
	 * Probe for a SA1111.
	 */
	ret = sa1111_probe(NEPONSET_SA1111_BASE);
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
	 * We only need to turn on DCLK whenever we want to use the
	 * DMA.  It can otherwise be held firmly in the off position.
	 */
	SKPCR |= SKPCR_DCLKEN;

	/*
	 * Enable the SA1110 memory bus request and grant signals.
	 */
	sa1110_mb_enable();

	neponset_init_irq();

	return 0;
}

__initcall(neponset_init);

static struct map_desc neponset_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xf3000000, 0x10000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* System Registers */
  { 0xf4000000, 0x40000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* SA-1111 */
  LAST_DESC
};

static void neponset_set_mctrl(struct uart_port *port, u_int mctrl)
{
	u_int mdm_ctl0 = MDM_CTL_0;

	if (port->mapbase == _Ser1UTCR0) {
		if (mctrl & TIOCM_RTS)
			mdm_ctl0 &= ~MDM_CTL0_RTS2;
		else
			mdm_ctl0 |= MDM_CTL0_RTS2;

		if (mctrl & TIOCM_DTR)
			mdm_ctl0 &= ~MDM_CTL0_DTR2;
		else
			mdm_ctl0 |= MDM_CTL0_DTR2;
	} else if (port->mapbase == _Ser3UTCR0) {
		if (mctrl & TIOCM_RTS)
			mdm_ctl0 &= ~MDM_CTL0_RTS1;
		else
			mdm_ctl0 |= MDM_CTL0_RTS1;

		if (mctrl & TIOCM_DTR)
			mdm_ctl0 &= ~MDM_CTL0_DTR1;
		else
			mdm_ctl0 |= MDM_CTL0_DTR1;
	}

	MDM_CTL_0 = mdm_ctl0;
}

static u_int neponset_get_mctrl(struct uart_port *port)
{
	u_int ret = TIOCM_CD | TIOCM_CTS | TIOCM_DSR;
	u_int mdm_ctl1 = MDM_CTL_1;

	if (port->mapbase == _Ser1UTCR0) {
		if (mdm_ctl1 & MDM_CTL1_DCD2)
			ret &= ~TIOCM_CD;
		if (mdm_ctl1 & MDM_CTL1_CTS2)
			ret &= ~TIOCM_CTS;
		if (mdm_ctl1 & MDM_CTL1_DSR2)
			ret &= ~TIOCM_DSR;
	} else if (port->mapbase == _Ser3UTCR0) {
		if (mdm_ctl1 & MDM_CTL1_DCD1)
			ret &= ~TIOCM_CD;
		if (mdm_ctl1 & MDM_CTL1_CTS1)
			ret &= ~TIOCM_CTS;
		if (mdm_ctl1 & MDM_CTL1_DSR1)
			ret &= ~TIOCM_DSR;
	}

	return ret;
}

static struct sa1100_port_fns neponset_port_fns __initdata = {
	.set_mctrl	= neponset_set_mctrl,
	.get_mctrl	= neponset_get_mctrl,
};

void __init neponset_map_io(void)
{
	iotable_init(neponset_io_desc);
	if (machine_has_neponset())
		sa1100_register_uart_fns(&neponset_port_fns);
}
