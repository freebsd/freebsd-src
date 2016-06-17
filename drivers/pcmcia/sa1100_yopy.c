/*
 * drivers/pcmcia/sa1100_yopy.c
 *
 * PCMCIA implementation routines for Yopy
 *
 */
#include <linux/kernel.h>
#include <linux/sched.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include "sa1100_generic.h"


static inline void pcmcia_power(int on) {
	/* high for power up */
	yopy_gpio_set(GPIO_CF_POWER, on);
}

static inline void pcmcia_reset(int reset)
{
	/* high for reset */
	yopy_gpio_set(GPIO_CF_RESET, reset);
}

static int yopy_pcmcia_init(struct pcmcia_init *init)
{
	int irq, res;

	pcmcia_power(0);
	pcmcia_reset(1);

	/* Set transition detect */
	set_GPIO_IRQ_edge(GPIO_CF_CD|GPIO_CF_BVD2|GPIO_CF_BVD1,
			  GPIO_NO_EDGES);
	set_GPIO_IRQ_edge( GPIO_CF_IREQ, GPIO_FALLING_EDGE );

	/* Register interrupts */
	irq = IRQ_CF_CD;
	res = request_irq(irq, init->handler, SA_INTERRUPT, "CF_CD", NULL);
	if (res < 0) goto irq_err;
	irq = IRQ_CF_BVD2;
	res = request_irq(irq, init->handler, SA_INTERRUPT, "CF_BVD2", NULL);
	if (res < 0) goto irq_err;
	irq = IRQ_CF_BVD1;
	res = request_irq(irq, init->handler, SA_INTERRUPT, "CF_BVD1", NULL);
	if (res < 0) goto irq_err;

	return 1;
irq_err:
	printk(KERN_ERR "%s: Request for IRQ %d failed\n", __FUNCTION__, irq);
	return -1;
}

static int yopy_pcmcia_shutdown(void)
{
	/* disable IRQs */
	free_irq( IRQ_CF_CD, NULL );
	free_irq( IRQ_CF_BVD2, NULL );
	free_irq( IRQ_CF_BVD1, NULL );

	/* Disable CF */
	pcmcia_reset(1);
	pcmcia_power(0);

	return 0;
}

static int yopy_pcmcia_socket_state(struct pcmcia_state_array *state_array)
{
	unsigned long levels;

	if (state_array->size != 1)
		return -1;

	memset(state_array->state, 0,
	       state_array->size * sizeof(struct pcmcia_state));

	levels = GPLR;

	state_array->state[0].detect = (levels & GPIO_CF_CD)    ? 0 : 1;
	state_array->state[0].ready  = (levels & GPIO_CF_READY) ? 1 : 0;
	state_array->state[0].bvd1   = (levels & GPIO_CF_BVD1)  ? 1 : 0;
	state_array->state[0].bvd2   = (levels & GPIO_CF_BVD2)  ? 1 : 0;
	state_array->state[0].wrprot = 0; /* Not available on Yopy. */
	state_array->state[0].vs_3v  = 0; /* FIXME Can only apply 3.3V on Yopy. */
	state_array->state[0].vs_Xv  = 0;

	return 1;
}

static int yopy_pcmcia_get_irq_info(struct pcmcia_irq_info *info)
{
	if (info->sock != 0)
		return -1;

	info->irq = IRQ_CF_IREQ;

	return 0;
}

static int yopy_pcmcia_configure_socket(const struct pcmcia_configure *configure)
{
	if (configure->sock != 0)
		return -1;

	switch (configure->vcc) {
	case 0:	/* power off */;
		pcmcia_power(0);
		break;
	case 50:
		printk(KERN_WARNING __FUNCTION__"(): CS asked for 5V, applying 3.3V..\n");
	case 33:
		pcmcia_power(1);
		break;
	default:
		printk(KERN_ERR __FUNCTION__"(): unrecognized Vcc %u\n",
		       configure->vcc);
		return -1;
	}

	pcmcia_reset(configure->reset);

	/* Silently ignore Vpp, output enable, speaker enable. */

	return 0;
}

static int yopy_pcmcia_socket_init(int sock)
{
	set_GPIO_IRQ_edge(GPIO_CF_CD|GPIO_CF_BVD2|GPIO_CF_BVD1,
			  GPIO_BOTH_EDGES);
	return 0;
}

static int yopy_pcmcia_socket_suspend(int sock)
{
	set_GPIO_IRQ_edge(GPIO_CF_CD|GPIO_CF_BVD2|GPIO_CF_BVD1,
			  GPIO_NO_EDGES);
	return 0;
}

struct pcmcia_low_level yopy_pcmcia_ops = {
	init:			yopy_pcmcia_init,
	shutdown:		yopy_pcmcia_shutdown,
	socket_state:		yopy_pcmcia_socket_state,
	get_irq_info:		yopy_pcmcia_get_irq_info,
	configure_socket:	yopy_pcmcia_configure_socket,

	socket_init:		yopy_pcmcia_socket_init,
	socket_suspend:		yopy_pcmcia_socket_suspend,
};
