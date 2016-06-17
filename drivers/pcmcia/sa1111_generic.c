/*
 * linux/drivers/pcmcia/sa1100_sa1111.c
 *
 * We implement the generic parts of a SA1111 PCMCIA driver.  This
 * basically means we handle everything except controlling the
 * power.  Power is machine specific...
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ioport.h>

#include <asm/hardware.h>
#include <asm/hardware/sa1111.h>
#include <asm/irq.h>

#include "sa1100_generic.h"
#include "sa1111_generic.h"

static struct irqs {
	int irq;
	const char *str;
} irqs[] = {
	{ S0_CD_VALID,    "SA1111 PCMCIA card detect" },
	{ S0_BVD1_STSCHG, "SA1111 PCMCIA BVD1"        },
	{ S1_CD_VALID,    "SA1111 CF card detect"     },
	{ S1_BVD1_STSCHG, "SA1111 CF BVD1"            },
};

int sa1111_pcmcia_init(struct pcmcia_init *init)
{
	int i, ret;

	if (!request_mem_region(_PCCR, 512, "PCMCIA"))
		return -1;

	INTPOL1 |= SA1111_IRQMASK_HI(S0_CD_VALID) |
		   SA1111_IRQMASK_HI(S1_CD_VALID) |
		   SA1111_IRQMASK_HI(S0_BVD1_STSCHG) |
		   SA1111_IRQMASK_HI(S1_BVD1_STSCHG);

	for (i = ret = 0; i < ARRAY_SIZE(irqs); i++) {
		ret = request_irq(irqs[i].irq, init->handler, SA_INTERRUPT,
				  irqs[i].str, NULL);
		if (ret)
			break;
	}

	if (i < ARRAY_SIZE(irqs)) {
		printk(KERN_ERR "sa1111_pcmcia: unable to grab IRQ%d (%d)\n",
			irqs[i].irq, ret);
		while (i--)
			free_irq(irqs[i].irq, NULL);

		release_mem_region(_PCCR, 16);
	}

	return ret ? -1 : 2;
}

int sa1111_pcmcia_shutdown(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(irqs); i++)
		free_irq(irqs[i].irq, NULL);

	INTPOL1 &= ~(SA1111_IRQMASK_HI(S0_CD_VALID) |
		     SA1111_IRQMASK_HI(S1_CD_VALID) |
		     SA1111_IRQMASK_HI(S0_BVD1_STSCHG) |
		     SA1111_IRQMASK_HI(S1_BVD1_STSCHG));

	release_mem_region(_PCCR, 512);

	return 0;
}

int sa1111_pcmcia_socket_state(struct pcmcia_state_array *state)
{
	unsigned long status;

	if (state->size < 2)
		return -1;

	status = PCSR;

	state->state[0].detect = status & PCSR_S0_DETECT ? 0 : 1;
	state->state[0].ready  = status & PCSR_S0_READY  ? 1 : 0;
	state->state[0].bvd1   = status & PCSR_S0_BVD1   ? 1 : 0;
	state->state[0].bvd2   = status & PCSR_S0_BVD2   ? 1 : 0;
	state->state[0].wrprot = status & PCSR_S0_WP     ? 1 : 0;
	state->state[0].vs_3v  = status & PCSR_S0_VS1    ? 0 : 1;
	state->state[0].vs_Xv  = status & PCSR_S0_VS2    ? 0 : 1;

	state->state[1].detect = status & PCSR_S1_DETECT ? 0 : 1;
	state->state[1].ready  = status & PCSR_S1_READY  ? 1 : 0;
	state->state[1].bvd1   = status & PCSR_S1_BVD1   ? 1 : 0;
	state->state[1].bvd2   = status & PCSR_S1_BVD2   ? 1 : 0;
	state->state[1].wrprot = status & PCSR_S1_WP     ? 1 : 0;
	state->state[1].vs_3v  = status & PCSR_S1_VS1    ? 0 : 1;
	state->state[1].vs_Xv  = status & PCSR_S1_VS2    ? 0 : 1;

	return 1;
}

int sa1111_pcmcia_get_irq_info(struct pcmcia_irq_info *info)
{
	int ret = 0;

	switch (info->sock) {
	case 0:	info->irq = S0_READY_NINT;	break;
	case 1: info->irq = S1_READY_NINT;	break;
	default: ret = 1;
	}

	return ret;
}

int sa1111_pcmcia_configure_socket(const struct pcmcia_configure *conf)
{
	unsigned int rst, flt, wait, pse, irq, pccr_mask;
	unsigned long flags;

	switch (conf->sock) {
	case 0:
		rst = PCCR_S0_RST;
		flt = PCCR_S0_FLT;
		wait = PCCR_S0_PWAITEN;
		pse = PCCR_S0_PSE;
		irq = S0_READY_NINT;
		break;

	case 1:
		rst = PCCR_S1_RST;
		flt = PCCR_S1_FLT;
		wait = PCCR_S1_PWAITEN;
		pse = PCCR_S1_PSE;
		irq = S1_READY_NINT;
		break;

	default:
		return -1;
	}

	switch (conf->vcc) {
	case 0:
		pccr_mask = 0;
		break;

	case 33:
		pccr_mask = wait;
		break;

	case 50:
		pccr_mask = pse | wait;
		break;

	default:
		printk(KERN_ERR "sa1111_pcmcia: unrecognised VCC %u\n",
			conf->vcc);
		return -1;
	}

	if (conf->reset)
		pccr_mask |= rst;

	if (conf->output)
		pccr_mask |= flt;

	local_irq_save(flags);
	PCCR = (PCCR & ~(pse | flt | wait | rst)) | pccr_mask;
	local_irq_restore(flags);

	if (conf->irq)
		enable_irq(irq);
	else
		disable_irq(irq);

	return 0;
}

int sa1111_pcmcia_socket_init(int sock)
{
	return 0;
}

int sa1111_pcmcia_socket_suspend(int sock)
{
	return 0;
}
