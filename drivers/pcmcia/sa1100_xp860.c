/*
 * drivers/pcmcia/sa1100_xp860.c
 *
 * XP860 PCMCIA specific routines
 *
 */
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include "sa1100_generic.h"

#define NCR_A0VPP	(1<<16)
#define NCR_A1VPP	(1<<17)

static int xp860_pcmcia_init(struct pcmcia_init *init)
{
  /* Set GPIO_A<3:0> to be outputs for PCMCIA/CF power controller: */
  PA_DDR &= ~(GPIO_GPIO0 | GPIO_GPIO1 | GPIO_GPIO2 | GPIO_GPIO3);
  
  /* MAX1600 to standby mode: */
  PA_DWR &= ~(GPIO_GPIO0 | GPIO_GPIO1 | GPIO_GPIO2 | GPIO_GPIO3);

#error Consider the following comment
  /*
   * 1- Please move GPDR initialisation  where it is interrupt or preemption
   *    safe (like from xp860_map_io).
   * 2- The GPCR line is bogus i.e. it will simply have absolutely no effect.
   *    Please see its definition in the SA1110 manual.
   * 3- Please do not use NCR_* values!
   */
  GPDR |= (NCR_A0VPP | NCR_A1VPP);
  GPCR &= ~(NCR_A0VPP | NCR_A1VPP);

  return sa1111_pcmcia_init(init);
}

static int
xp860_pcmcia_configure_socket(const struct pcmcia_configure *conf)
{
  unsigned int gpio_mask, pa_dwr_mask;
  unsigned int gpio_set, pa_dwr_set;
  int ret;

  /* Neponset uses the Maxim MAX1600, with the following connections:
#warning ^^^ This isn't a neponset!
   *
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
    gpio_mask = NCR_A0VPP | NCR_A1VPP;

    switch (conf->vcc) {
    default:
    case 0:	pa_dwr_set = 0;			break;
    case 33:	pa_dwr_set = GPIO_GPIO1;	break;
    case 50:	pa_dwr_set = GPIO_GPIO0;	break;
    }

    switch (conf->vpp) {
    case 0:	gpio_set = 0;			break;
    case 120:	gpio_set = NCR_A1VPP;		break;

    default:
      if (conf->vpp == conf->vcc)
	gpio_set = NCR_A0VPP;
      else {
	printk(KERN_ERR "%s(): unrecognized Vpp %u\n",
	       __FUNCTION__, conf->vpp);
	return -1;
      }
    }
    break;

  case 1:
    pa_dwr_mask = GPIO_GPIO2 | GPIO_GPIO3;
    gpio_mask = 0;
    gpio_set = 0;

    switch (conf->vcc) {
    default:
    case 0:	pa_dwr_set = 0;			break;
    case 33:	pa_dwr_set = GPIO_GPIO2;	break;
    case 50:	pa_dwr_set = GPIO_GPIO3;	break;
    }

    if (conf->vpp != conf->vcc && conf->vpp != 0) {
      printk(KERN_ERR "%s(): CF slot cannot support Vpp %u\n",
	     __FUNCTION__, conf->vpp);
      return -1;
    }
    break;
  }

  ret = sa1111_pcmcia_configure_socket(conf);
  if (ret == 0) {
    unsigned long flags;

    local_irq_save(flags);
    PA_DWR = (PA_DWR & ~pa_dwr_mask) | pa_dwr_set;
    GPSR = gpio_set;
    GPCR = gpio_set ^ gpio_mask;
    local_irq_restore(flags);
  }

  return ret;
}

struct pcmcia_low_level xp860_pcmcia_ops = { 
  init:			xp860_pcmcia_init,
  shutdown:		sa1111_pcmcia_shutdown,
  socket_state:		sa1111_pcmcia_socket_state,
  get_irq_info:		sa1111_pcmcia_get_irq_info,
  configure_socket:	xp860_pcmcia_configure_socket,

  socket_init:		sa1111_pcmcia_socket_init,
  socket_suspend:	sa1111_pcmcia_socket_suspend,
};

