/*
 * drivers/pcmcia/sa1100_assabet.c
 *
 * PCMCIA implementation routines for Assabet
 *
 */
#include <linux/kernel.h>
#include <linux/sched.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/arch/assabet.h>

#include "sa1100_generic.h"

static struct irqs {
	int irq;
	unsigned int gpio;
	const char *str;
} irqs[] = {
	{ ASSABET_IRQ_GPIO_CF_CD,   ASSABET_GPIO_CF_CD,   "CF_CD"   },
	{ ASSABET_IRQ_GPIO_CF_BVD2, ASSABET_GPIO_CF_BVD2, "CF_BVD2" },
	{ ASSABET_IRQ_GPIO_CF_BVD1, ASSABET_GPIO_CF_BVD1, "CF_BVD1" },
};

static int assabet_pcmcia_init(struct pcmcia_init *init)
{
	int i, res;

	/* Set transition detect */
	set_GPIO_IRQ_edge(ASSABET_GPIO_CF_IRQ, GPIO_FALLING_EDGE);

	/* Register interrupts */
	for (i = 0; i < ARRAY_SIZE(irqs); i++) {
		set_GPIO_IRQ_edge(irqs[i].gpio, GPIO_NO_EDGES);
		res = request_irq(irqs[i].irq, init->handler, SA_INTERRUPT,
				  irqs[i].str, NULL);
		if (res)
			goto irq_err;
	}

	/* There's only one slot, but it's "Slot 1": */
	return 2;

 irq_err:
	printk(KERN_ERR "%s: request for IRQ%d failed\n",
		__FUNCTION__, irqs[i].irq);

	while (i--)
		free_irq(irqs[i].irq, NULL);

	return -1;
}

/*
 * Release all resources.
 */
static int assabet_pcmcia_shutdown(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(irqs); i++)
		free_irq(irqs[i].irq, NULL);
  
	return 0;
}

static int
assabet_pcmcia_socket_state(struct pcmcia_state_array *state_array)
{
	unsigned long levels;

	if (state_array->size < 2)
		return -1;

	levels = GPLR;

	state_array->state[1].detect = (levels & ASSABET_GPIO_CF_CD) ? 0 : 1;
	state_array->state[1].ready  = (levels & ASSABET_GPIO_CF_IRQ) ? 1 : 0;
	state_array->state[1].bvd1   = (levels & ASSABET_GPIO_CF_BVD1) ? 1 : 0;
	state_array->state[1].bvd2   = (levels & ASSABET_GPIO_CF_BVD2) ? 1 : 0;
	state_array->state[1].wrprot = 0; /* Not available on Assabet. */
	state_array->state[1].vs_3v  = 1; /* Can only apply 3.3V on Assabet. */
	state_array->state[1].vs_Xv  = 0;

	return 1;
}

static int assabet_pcmcia_get_irq_info(struct pcmcia_irq_info *info)
{
	if (info->sock > 1)
		return -1;

	if (info->sock == 1)
		info->irq = ASSABET_IRQ_GPIO_CF_IRQ;

	return 0;
}

static int
assabet_pcmcia_configure_socket(const struct pcmcia_configure *configure)
{
	unsigned int mask;

	if (configure->sock > 1)
		return -1;

	if (configure->sock == 0)
		return 0;

	switch (configure->vcc) {
	case 0:
		mask = 0;
		break;

	case 50:
		printk(KERN_WARNING "%s(): CS asked for 5V, applying 3.3V...\n",
			__FUNCTION__);

	case 33:  /* Can only apply 3.3V to the CF slot. */
		mask = ASSABET_BCR_CF_PWR;
		break;

	default:
		printk(KERN_ERR "%s(): unrecognized Vcc %u\n", __FUNCTION__,
			configure->vcc);
		return -1;
	}

	/* Silently ignore Vpp, output enable, speaker enable. */

	if (configure->reset)
		mask |= ASSABET_BCR_CF_RST;

	ASSABET_BCR_frob(ASSABET_BCR_CF_RST | ASSABET_BCR_CF_PWR, mask);

	/*
	 * Handle suspend mode properly.  This prevents a
	 * flood of IRQs from the CF device.
	 */
	if (configure->irq)
		enable_irq(ASSABET_IRQ_GPIO_CF_IRQ);
	else
		disable_irq(ASSABET_IRQ_GPIO_CF_IRQ);

	return 0;
}

/*
 * Enable card status IRQs on (re-)initialisation.  This can
 * be called at initialisation, power management event, or
 * pcmcia event.
 */
static int assabet_pcmcia_socket_init(int sock)
{
	int i;

	if (sock == 1) {
		/*
		 * Enable CF bus
		 */
		ASSABET_BCR_clear(ASSABET_BCR_CF_BUS_OFF);

		for (i = 0; i < ARRAY_SIZE(irqs); i++)
			set_GPIO_IRQ_edge(irqs[i].gpio, GPIO_BOTH_EDGES);
	}

	return 0;
}

/*
 * Disable card status IRQs on suspend.
 */
static int assabet_pcmcia_socket_suspend(int sock)
{
	int i;

	if (sock == 1) {
		for (i = 0; i < ARRAY_SIZE(irqs); i++)
			set_GPIO_IRQ_edge(irqs[i].gpio, GPIO_NO_EDGES);

		/*
		 * Tristate the CF bus signals.  Also assert CF
		 * reset as per user guide page 4-11.
		 */
		ASSABET_BCR_set(ASSABET_BCR_CF_BUS_OFF | ASSABET_BCR_CF_RST);
	}

	return 0;
}

struct pcmcia_low_level assabet_pcmcia_ops = { 
	init:			assabet_pcmcia_init,
	shutdown:		assabet_pcmcia_shutdown,
	socket_state:		assabet_pcmcia_socket_state,
	get_irq_info:		assabet_pcmcia_get_irq_info,
	configure_socket:	assabet_pcmcia_configure_socket,

	socket_init:		assabet_pcmcia_socket_init,
	socket_suspend:		assabet_pcmcia_socket_suspend,
};

