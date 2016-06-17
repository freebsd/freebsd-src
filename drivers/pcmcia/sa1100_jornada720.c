/*
 * drivers/pcmcia/sa1100_jornada720.c
 *
 * Jornada720 PCMCIA specific routines
 *
 */
#include <linux/kernel.h>
#include <linux/sched.h>

#include <asm/hardware.h>

#include "sa1100_generic.h"
#include "sa1111_generic.h"

#define SOCKET0_POWER   GPIO_GPIO0
#define SOCKET0_3V      GPIO_GPIO2
#define SOCKET1_POWER   (GPIO_GPIO1 | GPIO_GPIO3)
#warning *** Does SOCKET1_3V actually do anything?
#define SOCKET1_3V	GPIO_GPIO3

static int jornada720_pcmcia_init(struct pcmcia_init *init)
{
  /*
   * What is all this crap for?
   */
  GRER |= 0x00000002;
  /* Set GPIO_A<3:1> to be outputs for PCMCIA/CF power controller: */
  PA_DDR = 0;
  PA_DWR = 0;
  PA_SDR = 0;
  PA_SSR = 0;

  PB_DDR = 0;
  PB_DWR = 0x01;
  PB_SDR = 0;
  PB_SSR = 0;

  PC_DDR = 0x88;
  PC_DWR = 0x20;
  PC_SDR = 0;
  PC_SSR = 0;

  return sa1111_pcmcia_init(init);
}

static int
jornada720_pcmcia_configure_socket(const struct pcmcia_configure *conf)
{
  unsigned int pa_dwr_mask, pa_dwr_set;
  int ret;

printk("%s(): config socket %d vcc %d vpp %d\n", __FUNCTION__,
	       conf->sock, conf->vcc, conf->vpp);

  switch (conf->sock) {
  case 0:
    pa_dwr_mask = SOCKET0_POWER | SOCKET0_3V;

    switch (conf->vcc) {
    default:
    case 0:	pa_dwr_set = 0;					break;
    case 33:	pa_dwr_set = SOCKET0_POWER | SOCKET0_3V;	break;
    case 50:	pa_dwr_set = SOCKET0_POWER;			break;
    }
    break;

  case 1:
    pa_dwr_mask = SOCKET1_POWER;

    switch (conf->vcc) {
    default:
    case 0:	pa_dwr_set = 0;					break;
    case 33:	pa_dwr_set = SOCKET1_POWER;			break;
    case 50:	pa_dwr_set = SOCKET1_POWER;			break;
    }
    break;
  }

  if (conf->vpp != conf->vcc && conf->vpp != 0) {
    printk(KERN_ERR "%s(): slot cannot support VPP %u\n",
	   __FUNCTION__, conf->vpp);
    return -1;
  }

  ret = sa1111_pcmcia_configure_socket(conf);
  if (ret == 0) {
    unsigned long flags;

    local_irq_save(flags);
    PA_DWR = (PA_DWR & ~pa_dwr_mask) | pa_dwr_set;
    locla_irq_restore(flags);
  }

  return ret;
}

struct pcmcia_low_level jornada720_pcmcia_ops = {
  init:			jornada720_pcmcia_init,
  shutdown:		sa1111_pcmcia_shutdown,
  socket_state:		sa1111_pcmcia_socket_state,
  get_irq_info:		sa1111_pcmcia_get_irq_info,
  configure_socket:	jornada720_pcmcia_configure_socket,

  socket_init:		sa1111_pcmcia_socket_init,
  socket_suspend:	sa1111_pcmcia_socket_suspend,
};

