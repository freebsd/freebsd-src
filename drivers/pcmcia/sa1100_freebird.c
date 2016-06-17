/*
 * drivers/pcmcia/sa1100_freebird.c
 *
 * Created by Eric Peng <ericpeng@coventive.com>
 *
 */
#include <linux/kernel.h>
#include <linux/sched.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include "sa1100_generic.h"


static int freebird_pcmcia_init(struct pcmcia_init *init){
  int irq, res;

  /* Enable Linkup CF card */
  LINKUP_PRC = 0xc0;
  mdelay(100);
  LINKUP_PRC = 0xc1;
  mdelay(100);
  LINKUP_PRC = 0xd1;
  mdelay(100);
  LINKUP_PRC = 0xd1;
  mdelay(100);
  LINKUP_PRC = 0xc0;

  /* Set transition detect */
  //set_GPIO_IRQ_edge( GPIO_CF_CD|GPIO_CF_BVD2|GPIO_CF_BVD1, GPIO_BOTH_EDGES );
  //set_GPIO_IRQ_edge( GPIO_CF_IRQ, GPIO_FALLING_EDGE );
  set_GPIO_IRQ_edge(GPIO_FREEBIRD_CF_CD|GPIO_FREEBIRD_CF_BVD,GPIO_NO_EDGES);
  set_GPIO_IRQ_edge(GPIO_FREEBIRD_CF_IRQ, GPIO_FALLING_EDGE);

  /* Register interrupts */
  irq = IRQ_GPIO_FREEBIRD_CF_CD;
  res = request_irq( irq, init->handler, SA_INTERRUPT, "CF_CD", NULL );
  if( res < 0 ) goto irq_err;
  irq = IRQ_GPIO_FREEBIRD_CF_BVD;
  res = request_irq( irq, init->handler, SA_INTERRUPT, "CF_BVD1", NULL );
  if( res < 0 ) goto irq_err;

  /* There's only one slot, but it's "Slot 1": */
  return 2;

irq_err:
  printk( KERN_ERR "%s: Request for IRQ %lu failed\n", __FUNCTION__, irq );
  return -1;
}

static int freebird_pcmcia_shutdown(void)
{
  /* disable IRQs */
  free_irq( IRQ_GPIO_FREEBIRD_CF_CD, NULL );
  free_irq( IRQ_GPIO_FREEBIRD_CF_BVD, NULL );

  /* Disable CF card */
  LINKUP_PRC = 0x40;  /* SSP=1   SOE=0 */
  mdelay(100);

  return 0;
}

static int freebird_pcmcia_socket_state(struct pcmcia_state_array
				       *state_array){
  unsigned long levels;

  if(state_array->size<2) return -1;

  memset(state_array->state, 0,
	 (state_array->size)*sizeof(struct pcmcia_state));

  levels = LINKUP_PRS;
//printk("LINKUP_PRS=%x \n",levels);

  state_array->state[0].detect=
    ((levels & (LINKUP_CD1 | LINKUP_CD2))==0)?1:0;

  state_array->state[0].ready=(levels & LINKUP_RDY)?1:0;

  state_array->state[0].bvd1=(levels & LINKUP_BVD1)?1:0;

  state_array->state[0].bvd2=(levels & LINKUP_BVD2)?1:0;

  state_array->state[0].wrprot=0; /* Not available on Assabet. */

  state_array->state[0].vs_3v=1;  /* Can only apply 3.3V on Assabet. */

  state_array->state[0].vs_Xv=0;

  return 1;
}

static int freebird_pcmcia_get_irq_info(struct pcmcia_irq_info *info){

  if(info->sock>1) return -1;

  if(info->sock==0)
    info->irq=IRQ_GPIO_FREEBIRD_CF_IRQ;

  return 0;
}

static int freebird_pcmcia_configure_socket(const struct pcmcia_configure
					   *configure)
{
  unsigned long value, flags;

  if(configure->sock>1) return -1;

  if(configure->sock==1) return 0;

  save_flags_cli(flags);

  value = 0xc0;   /* SSP=1  SOE=1  CFE=1 */

  switch(configure->vcc){
  case 0:

    break;

  case 50:
    printk(KERN_WARNING "%s(): CS asked for 5V, applying 3.3V...\n",
	   __FUNCTION__);

  case 33:  /* Can only apply 3.3V to the CF slot. */
    value |= LINKUP_S1;
    break;

  default:
    printk(KERN_ERR "%s(): unrecognized Vcc %u\n", __FUNCTION__,
	   configure->vcc);
    restore_flags(flags);
    return -1;
  }

  if (configure->reset)
  value = (configure->reset) ? (value | LINKUP_RESET) : (value & ~LINKUP_RESET);

  /* Silently ignore Vpp, output enable, speaker enable. */

  LINKUP_PRC = value;
//printk("LINKUP_PRC=%x\n",value);
  restore_flags(flags);

  return 0;
}

static int freebird_pcmcia_socket_init(int sock)
{
  set_GPIO_IRQ_edge(GPIO_FREEBIRD_CF_CD|GPIO_FREEBIRD_CF_BVD, GPIO_BOTH_EDGES);
  return 0;
}

static int freebird_pcmcia_socket_suspend(int sock)
{
  set_GPIO_IRQ_edge(GPIO_FREEBIRD_CF_CD|GPIO_FREEBIRD_CF_BVD, GPIO_NO_EDGES);
  return 0;
}

struct pcmcia_low_level freebird_pcmcia_ops = {
  init:			freebird_pcmcia_init,
  shutdown:		freebird_pcmcia_shutdown,
  socket_state:		freebird_pcmcia_socket_state,
  get_irq_info:		freebird_pcmcia_get_irq_info,
  configure_socket:	freebird_pcmcia_configure_socket,

  socket_init:		freebird_pcmcia_socket_init,
  socket_suspend:	freebird_pcmcia_socket_suspend,
};

