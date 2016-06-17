/*
 * drivers/pcmcia/sa1100_shannon.c
 *
 * PCMCIA implementation routines for Shannon
 *
 */
#include <linux/kernel.h>
#include <linux/sched.h>

#include <asm/hardware.h>
#include <asm/arch/shannon.h>
#include <asm/irq.h>
#include "sa1100_generic.h"

static int shannon_pcmcia_init(struct pcmcia_init *init)
{
	int irq, res;

	/* All those are inputs */
	GPDR &= ~(SHANNON_GPIO_EJECT_0 | SHANNON_GPIO_EJECT_1 | 
		  SHANNON_GPIO_RDY_0 | SHANNON_GPIO_RDY_1);
	GAFR &= ~(SHANNON_GPIO_EJECT_0 | SHANNON_GPIO_EJECT_1 | 
		  SHANNON_GPIO_RDY_0 | SHANNON_GPIO_RDY_1);

	/* Set transition detect */
	set_GPIO_IRQ_edge(SHANNON_GPIO_EJECT_0 | SHANNON_GPIO_EJECT_1, GPIO_NO_EDGES);
	set_GPIO_IRQ_edge(SHANNON_GPIO_RDY_0 | SHANNON_GPIO_RDY_1, GPIO_FALLING_EDGE);

	/* Register interrupts */
	irq = SHANNON_IRQ_GPIO_EJECT_0;
	res = request_irq(irq, init->handler, SA_INTERRUPT, "PCMCIA_CD_0", NULL);
	if (res < 0) goto irq_err;
	irq = SHANNON_IRQ_GPIO_EJECT_1;
	res = request_irq(irq, init->handler, SA_INTERRUPT, "PCMCIA_CD_1", NULL);
	if (res < 0) goto irq_err;

	return 2;
irq_err:
	printk(KERN_ERR "%s: Request for IRQ %d failed\n", __FUNCTION__, irq);
	return -1;
}

static int shannon_pcmcia_shutdown(void)
{
	/* disable IRQs */
	free_irq(SHANNON_IRQ_GPIO_EJECT_0, NULL);
	free_irq(SHANNON_IRQ_GPIO_EJECT_1, NULL);

	return 0;
}

static int shannon_pcmcia_socket_state(struct pcmcia_state_array *state_array)
{
	unsigned long levels;

	memset(state_array->state, 0,
	       state_array->size * sizeof(struct pcmcia_state));

	levels = GPLR;

	state_array->state[0].detect = (levels & SHANNON_GPIO_EJECT_0) ? 0 : 1;
	state_array->state[0].ready  = (levels & SHANNON_GPIO_RDY_0) ? 1 : 0;
	state_array->state[0].wrprot = 0; /* Not available on Shannon. */
	state_array->state[0].bvd1 = 1; 
	state_array->state[0].bvd2 = 1; 
	state_array->state[0].vs_3v  = 1; /* FIXME Can only apply 3.3V on Shannon. */
	state_array->state[0].vs_Xv  = 0;

	state_array->state[1].detect = (levels & SHANNON_GPIO_EJECT_1) ? 0 : 1;
	state_array->state[1].ready  = (levels & SHANNON_GPIO_RDY_1) ? 1 : 0;
	state_array->state[1].wrprot = 0; /* Not available on Shannon. */
	state_array->state[1].bvd1 = 1; 
	state_array->state[1].bvd2 = 1; 
	state_array->state[1].vs_3v  = 1; /* FIXME Can only apply 3.3V on Shannon. */
	state_array->state[1].vs_Xv  = 0;

	return 1;
}

static int shannon_pcmcia_get_irq_info(struct pcmcia_irq_info *info)
{
	if (info->sock == 0)
		info->irq = SHANNON_IRQ_GPIO_RDY_0;
	else if (info->sock == 1)
		info->irq = SHANNON_IRQ_GPIO_RDY_1;
	else return -1;
	
	return 0;
}

static int shannon_pcmcia_configure_socket(const struct pcmcia_configure *configure)
{

	switch (configure->vcc) {
	case 0:	/* power off */;
		printk(KERN_WARNING __FUNCTION__"(): CS asked for 0V, still applying 3.3V..\n");
		break;
	case 50:
		printk(KERN_WARNING __FUNCTION__"(): CS asked for 5V, applying 3.3V..\n");
	case 33:
		break;
	default:
		printk(KERN_ERR __FUNCTION__"(): unrecognized Vcc %u\n",
		       configure->vcc);
		return -1;
	}

	printk(KERN_WARNING __FUNCTION__"(): Warning, Can't perform reset\n");
	
	/* Silently ignore Vpp, output enable, speaker enable. */

	return 0;
}

static int shannon_pcmcia_socket_init(int sock)
{
	if (sock == 0)
		set_GPIO_IRQ_edge(SHANNON_GPIO_EJECT_0, GPIO_BOTH_EDGES);
	else if (sock == 1)
		set_GPIO_IRQ_edge(SHANNON_GPIO_EJECT_1, GPIO_BOTH_EDGES);

	return 0;
}

static int shannon_pcmcia_socket_suspend(int sock)
{
	if (sock == 0)
		set_GPIO_IRQ_edge(SHANNON_GPIO_EJECT_0, GPIO_NO_EDGES);
	else if (sock == 1)
		set_GPIO_IRQ_edge(SHANNON_GPIO_EJECT_1, GPIO_NO_EDGES);

	return 0;
}

struct pcmcia_low_level shannon_pcmcia_ops = {
	init:			shannon_pcmcia_init,
	shutdown:		shannon_pcmcia_shutdown,
	socket_state:		shannon_pcmcia_socket_state,
	get_irq_info:		shannon_pcmcia_get_irq_info,
	configure_socket:	shannon_pcmcia_configure_socket,

	socket_init:		shannon_pcmcia_socket_init,
	socket_suspend:		shannon_pcmcia_socket_suspend,
};
