/*
 * drivers/pcmcia/sa1100_h3600.c
 *
 * PCMCIA implementation routines for H3600
 *
 */
#include <linux/kernel.h>
#include <linux/sched.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include "sa1100_generic.h"

static struct irqs {
	int irq;
	const char *str;
} irqs[] = {
	{ IRQ_GPIO_H3600_PCMCIA_CD0, "PCMCIA CD0" },
	{ IRQ_GPIO_H3600_PCMCIA_CD1, "PCMCIA CD1" }
};

static int h3600_pcmcia_init(struct pcmcia_init *init)
{
	int i, res;

	/*
	 * Set transition detect
	 */
	set_GPIO_IRQ_edge(GPIO_H3600_PCMCIA_IRQ0 | GPIO_H3600_PCMCIA_IRQ1,
			  GPIO_FALLING_EDGE);

	/*
	 * Register interrupts
	 */
	for (i = res = 0; i < ARRAY_SIZE(irqs); i++) {
		res = request_irq(irqs[i].irq, init->handler, SA_INTERRUPT,
				  irqs[i].str, NULL);
		if (res)
			break;
	}

	if (res) {
		printk(KERN_ERR "h3600_pcmcia: request for IRQ%d failed: %d\n",
		       irqs[i].irq, res);

		while (i--)
			free_irq(irqs[i].irq, NULL);
	}

	return res ? -1 : 2;
}

static int h3600_pcmcia_shutdown(void)
{
	int i;

	/*
	 * disable IRQs
	 */
	for (i = 0; i < ARRAY_SIZE(irqs); i++)
		free_irq(irqs[i].irq, NULL);
  
	/* Disable CF bus: */
	clr_h3600_egpio(IPAQ_EGPIO_OPT_NVRAM_ON);
	clr_h3600_egpio(IPAQ_EGPIO_OPT_ON);
	set_h3600_egpio(IPAQ_EGPIO_OPT_RESET);

	return 0;
}

static int
h3600_pcmcia_socket_state(struct pcmcia_state_array *state)
{
	unsigned long levels;

	if (state->size < 2)
		return -1;

	levels = GPLR;

	state->state[0].detect = levels & GPIO_H3600_PCMCIA_CD0 ? 0 : 1;
	state->state[0].ready = levels & GPIO_H3600_PCMCIA_IRQ0 ? 1 : 0;
	state->state[0].bvd1 = 0;
	state->state[0].bvd2 = 0;
	state->state[0].wrprot = 0; /* Not available on H3600. */
	state->state[0].vs_3v = 0;
	state->state[0].vs_Xv = 0;

	state->state[1].detect = levels & GPIO_H3600_PCMCIA_CD1 ? 0 : 1;
	state->state[1].ready = levels & GPIO_H3600_PCMCIA_IRQ1 ? 1 : 0;
	state->state[1].bvd1 = 0;
	state->state[1].bvd2 = 0;
	state->state[1].wrprot = 0; /* Not available on H3600. */
	state->state[1].vs_3v = 0;
	state->state[1].vs_Xv = 0;

	return 1;
}

static int h3600_pcmcia_get_irq_info(struct pcmcia_irq_info *info)
{
	switch (info->sock) {
	case 0:
		info->irq = IRQ_GPIO_H3600_PCMCIA_IRQ0;
		break;
	case 1:
		info->irq = IRQ_GPIO_H3600_PCMCIA_IRQ1;
		break;
	default:
		return -1;
	}
	return 0;
}

static int
h3600_pcmcia_configure_socket(const struct pcmcia_configure *conf)
{
	if (conf->sock > 1)
		return -1;

	if (conf->vcc != 0 && conf->vcc != 33 && conf->vcc != 50) {
		printk(KERN_ERR "h3600_pcmcia: unrecognized Vcc %u.%uV\n",
		       conf->vcc / 10, conf->vcc % 10);
		return -1;
	}

	if (conf->reset)
		set_h3600_egpio(IPAQ_EGPIO_CARD_RESET);
	else
		clr_h3600_egpio(IPAQ_EGPIO_CARD_RESET);

	/* Silently ignore Vpp, output enable, speaker enable. */

	return 0;
}

static int h3600_pcmcia_socket_init(int sock)
{
	/* Enable CF bus: */
	set_h3600_egpio(IPAQ_EGPIO_OPT_NVRAM_ON);
	set_h3600_egpio(IPAQ_EGPIO_OPT_ON);
	clr_h3600_egpio(IPAQ_EGPIO_OPT_RESET);

	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(10*HZ / 1000);

	switch (sock) {
	case 0:
		set_GPIO_IRQ_edge(GPIO_H3600_PCMCIA_CD0, GPIO_BOTH_EDGES);
		break;
	case 1:
		set_GPIO_IRQ_edge(GPIO_H3600_PCMCIA_CD1, GPIO_BOTH_EDGES);
		break;
	}

	return 0;
}

static int h3600_pcmcia_socket_suspend(int sock)
{
	switch (sock) {
	case 0:
		set_GPIO_IRQ_edge(GPIO_H3600_PCMCIA_CD0, GPIO_NO_EDGES);
		break;
	case 1:
		set_GPIO_IRQ_edge(GPIO_H3600_PCMCIA_CD1, GPIO_NO_EDGES);
		break;
	}

	/*
	 * FIXME:  This doesn't fit well.  We don't have the mechanism in
	 * the generic PCMCIA layer to deal with the idea of two sockets
	 * on one bus.  We rely on the cs.c behaviour shutting down
	 * socket 0 then socket 1.
	 */
	if (sock == 1) {
		clr_h3600_egpio(IPAQ_EGPIO_OPT_ON);
		clr_h3600_egpio(IPAQ_EGPIO_OPT_NVRAM_ON);
		/* hmm, does this suck power? */
		set_h3600_egpio(IPAQ_EGPIO_OPT_RESET);
	}

	return 0;
}

struct pcmcia_low_level h3600_pcmcia_ops = { 
	init:			h3600_pcmcia_init,
	shutdown:		h3600_pcmcia_shutdown,
	socket_state:		h3600_pcmcia_socket_state,
	get_irq_info:		h3600_pcmcia_get_irq_info,
	configure_socket:	h3600_pcmcia_configure_socket,

	socket_init:		h3600_pcmcia_socket_init,
	socket_suspend:		h3600_pcmcia_socket_suspend,
};

