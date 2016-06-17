/*
 * linux/drivers/pcmcia/sa1100_neponset.c
 *
 * Neponset PCMCIA specific routines
 */
#include <linux/kernel.h>
#include <linux/sched.h>

#include <asm/hardware.h>
#include <asm/arch/assabet.h>
#include <asm/hardware/sa1111.h>

#include "sa1100_generic.h"
#include "sa1111_generic.h"

static int neponset_pcmcia_init(struct pcmcia_init *init)
{
	/* Set GPIO_A<3:0> to be outputs for PCMCIA/CF power controller: */
	PA_DDR &= ~(GPIO_GPIO0 | GPIO_GPIO1 | GPIO_GPIO2 | GPIO_GPIO3);

	/* MAX1600 to standby mode: */
	PA_DWR &= ~(GPIO_GPIO0 | GPIO_GPIO1 | GPIO_GPIO2 | GPIO_GPIO3);
	NCR_0 &= ~(NCR_A0VPP | NCR_A1VPP);

	return sa1111_pcmcia_init(init);
}

static int
neponset_pcmcia_configure_socket(const struct pcmcia_configure *conf)
{
	unsigned int ncr_mask, pa_dwr_mask;
	unsigned int ncr_set, pa_dwr_set;
	int ret;

	/* Neponset uses the Maxim MAX1600, with the following connections:

	 *   MAX1600      Neponset
	 *
	 *    A0VCC        SA-1111 GPIO A<1>
	 *    A1VCC        SA-1111 GPIO A<0>
	 *    A0VPP        CPLD NCR A0VPP
	 *    A1VPP        CPLD NCR A1VPP
	 *    B0VCC        SA-1111 GPIO A<2>
	 *    B1VCC        SA-1111 GPIO A<3>
	 *    B0VPP        ground (slot B is CF)
	 *    B1VPP        ground (slot B is CF)
	 *
	 *     VX          VCC (5V)
	 *     VY          VCC3_3 (3.3V)
	 *     12INA       12V
	 *     12INB       ground (slot B is CF)
	 *
	 * The MAX1600 CODE pin is tied to ground, placing the device in 
	 * "Standard Intel code" mode. Refer to the Maxim data sheet for
	 * the corresponding truth table.
	 */

	switch (conf->sock) {
	case 0:
		pa_dwr_mask = GPIO_GPIO0 | GPIO_GPIO1;
		ncr_mask = NCR_A0VPP | NCR_A1VPP;

		switch (conf->vcc) {
		default:
		case 0:		pa_dwr_set = 0;			break;
		case 33:	pa_dwr_set = GPIO_GPIO1;	break;
		case 50:	pa_dwr_set = GPIO_GPIO0;	break;
		}

		switch (conf->vpp) {
		case 0:		ncr_set = 0;			break;
		case 120:	ncr_set = NCR_A1VPP;		break;
		default:
			if (conf->vpp == conf->vcc)
				ncr_set = NCR_A0VPP;
			else {
				printk(KERN_ERR "%s(): unrecognized VPP %u\n",
				       __FUNCTION__, conf->vpp);
				return -1;
			}
		}
		break;

	case 1:
		pa_dwr_mask = GPIO_GPIO2 | GPIO_GPIO3;
		ncr_mask = 0;
		ncr_set = 0;

		switch (conf->vcc) {
		default:
		case 0:		pa_dwr_set = 0;			break;
		case 33:	pa_dwr_set = GPIO_GPIO2;	break;
		case 50:	pa_dwr_set = GPIO_GPIO3;	break;
		}

		if (conf->vpp != conf->vcc && conf->vpp != 0) {
			printk(KERN_ERR "%s(): CF slot cannot support VPP %u\n",
			       __FUNCTION__, conf->vpp);
			return -1;
		}
		break;

	default:
		return -1;
	}

	ret = sa1111_pcmcia_configure_socket(conf);
	if (ret == 0) {
		unsigned long flags;

		local_irq_save(flags);
		NCR_0 = (NCR_0 & ~ncr_mask) | ncr_set;
		PA_DWR = (PA_DWR & ~pa_dwr_mask) | pa_dwr_set;
		local_irq_restore(flags);
	}

	return 0;
}

struct pcmcia_low_level neponset_pcmcia_ops = {
	init:			neponset_pcmcia_init,
	shutdown:		sa1111_pcmcia_shutdown,
	socket_state:		sa1111_pcmcia_socket_state,
	get_irq_info:		sa1111_pcmcia_get_irq_info,
	configure_socket:	neponset_pcmcia_configure_socket,

	socket_init:		sa1111_pcmcia_socket_init,
	socket_suspend:		sa1111_pcmcia_socket_suspend,
};
