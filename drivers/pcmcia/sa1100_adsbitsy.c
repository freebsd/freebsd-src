/*
 * drivers/pcmcia/sa1100_adsbitsy.c
 *
 * PCMCIA implementation routines for ADS Bitsy
 *
 * 9/18/01 Woojung
 *         Fixed wrong PCMCIA voltage setting
 *
 * 7/5/01 Woojung Huh <whuh@applieddata.net>
 *
 */
#include <linux/kernel.h>
#include <linux/sched.h>

#include <asm/hardware.h>

#include "sa1100_generic.h"
#include "sa1111_generic.h"

static int adsbitsy_pcmcia_init(struct pcmcia_init *init)
{
  /* Set GPIO_A<3:0> to be outputs for PCMCIA/CF power controller: */
  PA_DDR &= ~(GPIO_GPIO0 | GPIO_GPIO1 | GPIO_GPIO2 | GPIO_GPIO3);

  /* Disable Power 3.3V/5V for PCMCIA/CF */
  PA_DWR |= GPIO_GPIO0 | GPIO_GPIO1 | GPIO_GPIO2 | GPIO_GPIO3;

  /* Why? */			 
  MECR = 0x09430943;

  return sa1111_pcmcia_init(init);
}

static int
adsbitsy_pcmcia_configure_socket(const struct pcmcia_configure *conf)
{
  unsigned int pa_dwr_mask, pa_dwr_set;
  int ret;

  switch (conf->sock) {
  case 0:
    pa_dwr_mask = GPIO_GPIO0 | GPIO_GPIO1;

    switch (conf->vcc) {
    default:
    case 0:	pa_dwr_set = GPIO_GPIO0 | GPIO_GPIO1;	break;
    case 33:	pa_dwr_set = GPIO_GPIO1;		break;
    case 50:	pa_dwr_set = GPIO_GPIO0;		break;
    }
    break;

  case 1:
    pa_dwr_mask = GPIO_GPIO2 | GPIO_GPIO3;

    switch (conf->vcc) {
    default:
    case 0:	pa_dwr_set = 0;				break;
    case 33:	pa_dwr_set = GPIO_GPIO2;		break;
    case 50:	pa_dwr_set = GPIO_GPIO3;		break;
    }

  default:
    return -1;
  }

  if (conf->vpp != conf->vcc && conf->vpp != 0) {
    printk(KERN_ERR "%s(): CF slot cannot support VPP %u\n",
		__FUNCTION__, conf->vpp);
    return -1;
  }

  ret = sa1111_pcmcia_configure_socket(conf);
  if (ret == 0) {
    unsigned long flags;

    local_irq_save(flags);
    PA_DWR = (PA_DWR & ~pa_dwr_mask) | pa_dwr_set;
    local_irq_restore(flags);
  }

  return ret;
}

struct pcmcia_low_level adsbitsy_pcmcia_ops = {
  init:			adsbitsy_pcmcia_init,
  shutdown:		sa1111_pcmcia_shutdown,
  socket_state:		sa1111_pcmcia_socket_state,
  get_irq_info:		sa1111_pcmcia_get_irq_info,
  configure_socket:	adsbitsy_pcmcia_configure_socket,

  socket_init:		sa1111_pcmcia_socket_init,
  socket_suspend:	sa1111_pcmcia_socket_suspend,
};

