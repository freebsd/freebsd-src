/*
 * drivers/pcmcia/sa1100_pangolin.c
 *
 * PCMCIA implementation routines for Pangolin
 *
 */
#include <linux/kernel.h>
#include <linux/sched.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include "sa1100_generic.h"

static int pangolin_pcmcia_init(struct pcmcia_init *init){
  int irq, res;

#ifndef CONFIG_SA1100_PANGOLIN_PCMCIA_IDE
  /* Enable PCMCIA bus: */
  GPCR = GPIO_PCMCIA_BUS_ON;
#endif

  /* Set transition detect */
  set_GPIO_IRQ_edge( GPIO_PCMCIA_CD, GPIO_BOTH_EDGES );
  set_GPIO_IRQ_edge( GPIO_PCMCIA_IRQ, GPIO_FALLING_EDGE );

  /* Register interrupts */
  irq = IRQ_PCMCIA_CD;
  res = request_irq( irq, init->handler, SA_INTERRUPT, "PCMCIA_CD", NULL );
  if( res < 0 ) goto irq_err;

  /* There's only one slot, but it's "Slot 1": */
  return 2;

irq_err:
  printk( KERN_ERR "%s: Request for IRQ %lu failed\n", __FUNCTION__, irq );
  return -1;
}

static int pangolin_pcmcia_shutdown(void)
{
  /* disable IRQs */
  free_irq( IRQ_PCMCIA_CD, NULL );
#ifndef CONFIG_SA1100_PANGOLIN_PCMCIA_IDE
    /* Disable PCMCIA bus: */
    GPSR = GPIO_PCMCIA_BUS_ON;
#endif
  return 0;
}

static int pangolin_pcmcia_socket_state(struct pcmcia_state_array
				       *state_array){
  unsigned long levels;

  if(state_array->size<2) return -1;

  memset(state_array->state, 0, 
	 (state_array->size)*sizeof(struct pcmcia_state));

  levels=GPLR;
#ifndef CONFIG_SA1100_PANGOLIN_PCMCIA_IDE
  state_array->state[1].detect=((levels & GPIO_PCMCIA_CD)==0)?1:0;
  state_array->state[1].ready=(levels & GPIO_PCMCIA_IRQ)?1:0;
  state_array->state[1].bvd1=1; /* Not available on Pangolin. */
  state_array->state[1].bvd2=1; /* Not available on Pangolin. */
  state_array->state[1].wrprot=0; /* Not available on Pangolin. */
  state_array->state[1].vs_3v=1;  /* Can only apply 3.3V on Pangolin. */
  state_array->state[1].vs_Xv=0;
#else
  state_array->state[0].detect=((levels & GPIO_PCMCIA_CD)==0)?1:0;
  state_array->state[0].ready=(levels & GPIO_PCMCIA_IRQ)?1:0;
  state_array->state[0].bvd1=1; /* Not available on Pangolin. */
  state_array->state[0].bvd2=1; /* Not available on Pangolin. */
  state_array->state[0].wrprot=0; /* Not available on Pangolin. */
  state_array->state[0].vs_3v=0;  /* voltage level is determined by jumper setting */
  state_array->state[0].vs_Xv=0;
#endif
  return 1;
}

static int pangolin_pcmcia_get_irq_info(struct pcmcia_irq_info *info){

  if(info->sock>1) return -1;
#ifndef CONFIG_SA1100_PANGOLIN_PCMCIA_IDE
  if(info->sock==1)
	info->irq=IRQ_PCMCIA_IRQ;
#else
  if(info->sock==0)
        info->irq=IRQ_PCMCIA_IRQ;
#endif
  return 0;
}

static int pangolin_pcmcia_configure_socket(const struct pcmcia_configure
					   *configure)
{
  unsigned long value, flags;

  if(configure->sock>1) return -1;
#ifndef CONFIG_SA1100_PANGOLIN_PCMCIA_IDE
  if(configure->sock==0) return 0;
#endif
  save_flags_cli(flags);

  /* Murphy: BUS_ON different from POWER ? */

  switch(configure->vcc){
  case 0:
    break;
#ifndef CONFIG_SA1100_PANGOLIN_PCMCIA_IDE
  case 50:
    printk(KERN_WARNING "%s(): CS asked for 5V, applying 3.3V...\n",
	   __FUNCTION__);
  case 33:  /* Can only apply 3.3V to the CF slot. */
    break;
#else
  case 50:
    printk(KERN_WARNING "%s(): CS asked for 5V, determinded by jumper setting...\n", __FUNCTION__);
    break;
  case 33:
    printk(KERN_WARNING "%s(): CS asked for 3.3V, determined by jumper setting...\n", __FUNCTION__);
    break;
#endif
  default:
    printk(KERN_ERR "%s(): unrecognized Vcc %u\n", __FUNCTION__,
	   configure->vcc);
    restore_flags(flags);
    return -1;
  }
#ifdef CONFIG_SA1100_PANGOLIN_PCMCIA_IDE
  /* reset & unreset request */
  if(configure->sock==0) {
	if(configure->reset) {
		GPSR |= GPIO_PCMCIA_RESET;
	} else {
		GPCR |= GPIO_PCMCIA_RESET;
	}
  }
#endif
  /* Silently ignore Vpp, output enable, speaker enable. */
  restore_flags(flags);
  return 0;
}

static int pangolin_pcmcia_socket_init(int sock)
{
  /* enable card status IRQs - see sa1100_assabet.c for details  */
  return -1;
}

static int pangolin_pcmcia_socket_suspend(int sock)
{
  /* disable card status IRQs - see sa1100_assabet.c for details */
  return -1;
}

struct pcmcia_low_level pangolin_pcmcia_ops = { 
  init:			pangolin_pcmcia_init,
  shutdown:		pangolin_pcmcia_shutdown,
  socket_state:		pangolin_pcmcia_socket_state,
  get_irq_info:		pangolin_pcmcia_get_irq_info,
  configure_socket:	pangolin_pcmcia_configure_socket,

  socket_init:		pangolin_pcmcia_socket_init,
  socket_suspend,	pangolin_pcmcia_socket_suspend,
};

