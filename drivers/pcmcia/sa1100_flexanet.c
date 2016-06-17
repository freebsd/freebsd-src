/*
 * drivers/pcmcia/sa1100_flexanet.c
 *
 * PCMCIA implementation routines for Flexanet.
 * by Jordi Colomer, 09/05/2001
 *
 */

#include <linux/kernel.h>
#include <linux/sched.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include "sa1100_generic.h"

static struct {
  int irq;
  unsigned int gpio;
  const char *name;
} irqs[] = {
  { IRQ_GPIO_CF1_CD,   GPIO_CF1_NCD,  "CF1_CD"   },
  { IRQ_GPIO_CF1_BVD1, GPIO_CF1_BVD1, "CF1_BVD1" },
  { IRQ_GPIO_CF2_CD,   GPIO_CF2_NCD,  "CF2_CD"   },
  { IRQ_GPIO_CF2_BVD1, GPIO_CF2_BVD1, "CF2_BVD1" }
};

/*
 * Socket initialization.
 *
 * Called by sa1100_pcmcia_driver_init on startup.
 * Must return the number of slots.
 *
 */
static int flexanet_pcmcia_init(struct pcmcia_init *init)
{
  int i, res;

  /* Configure the GPIOs as inputs (BVD2 is not implemented) */
  GPDR &= ~(GPIO_CF1_NCD | GPIO_CF1_BVD1 | GPIO_CF1_IRQ |
            GPIO_CF2_NCD | GPIO_CF2_BVD1 | GPIO_CF2_IRQ );

  /* Set IRQ edge */
  set_GPIO_IRQ_edge( GPIO_CF1_IRQ | GPIO_CF2_IRQ, GPIO_FALLING_EDGE );

  /* Register the socket interrupts (not the card interrupts) */
  for (i = 0; i < ARRAY_SIZE(irqs); i++) {
    set_GPIO_IRQ_edge(irqs[i].gpio, GPIO_NO_EDGES);
    res = request_irq(irqs[i].irq, init->handler, SA_INTERRUPT,
		      irqs[i].name, NULL);
    if (res < 0)
      break;
  }

  /* If we failed, then free all interrupts requested thus far. */
  if (res < 0) {
    printk(KERN_ERR "%s: Request for IRQ%u failed: %d\n", __FUNCTION__,
           irqs[i].irq, res);
    while (i--)
      free_irq(irqs[i].irq, NULL);
    return -1;
  }

  return 2;
}


/*
 * Socket shutdown
 *
 */
static int flexanet_pcmcia_shutdown(void)
{
  int i;

  /* disable IRQs */
  for (i = 0; i < ARRAY_SIZE(irqs); i++)
    free_irq(irqs[i].irq, NULL);

  return 0;
}


/*
 * Get the state of the sockets.
 *
 *  Sockets in Flexanet are 3.3V only, without BVD2.
 *
 */
static int flexanet_pcmcia_socket_state(struct pcmcia_state_array
				       *state_array){
  unsigned long levels;

  if (state_array->size < 2)
    return -1;

  /* Sense the GPIOs, asynchronously */
  levels = GPLR;

  /* Socket 0 */
  state_array->state[0].detect = ((levels & GPIO_CF1_NCD)==0)?1:0;
  state_array->state[0].ready  = (levels & GPIO_CF1_IRQ)?1:0;
  state_array->state[0].bvd1   = (levels & GPIO_CF1_BVD1)?1:0;
  state_array->state[0].bvd2   = 1;
  state_array->state[0].wrprot = 0;
  state_array->state[0].vs_3v  = 1;
  state_array->state[0].vs_Xv  = 0;

  /* Socket 1 */
  state_array->state[1].detect = ((levels & GPIO_CF2_NCD)==0)?1:0;
  state_array->state[1].ready  = (levels & GPIO_CF2_IRQ)?1:0;
  state_array->state[1].bvd1   = (levels & GPIO_CF2_BVD1)?1:0;
  state_array->state[1].bvd2   = 1;
  state_array->state[1].wrprot = 0;
  state_array->state[1].vs_3v  = 1;
  state_array->state[1].vs_Xv  = 0;

  return 1;
}


/*
 * Return the IRQ information for a given socket number (the IRQ number)
 *
 */
static int flexanet_pcmcia_get_irq_info(struct pcmcia_irq_info *info){

  /* check the socket index */
  if (info->sock > 1)
    return -1;

  if (info->sock == 0)
    info->irq = IRQ_GPIO_CF1_IRQ;
  else if (info->sock == 1)
    info->irq = IRQ_GPIO_CF2_IRQ;

  return 0;
}


/*
 *
 */
static int flexanet_pcmcia_configure_socket(const struct pcmcia_configure
					   *configure)
{
  unsigned long value, flags, mask;


  if (configure->sock > 1)
    return -1;

  /* Ignore the VCC level since it is 3.3V and always on */
  switch (configure->vcc)
  {
    case 0:
      printk(KERN_WARNING "%s(): CS asked to power off.\n", __FUNCTION__);
      break;

    case 50:
      printk(KERN_WARNING "%s(): CS asked for 5V, applying 3.3V...\n",
  	   __FUNCTION__);

    case 33:
      break;

    default:
      printk(KERN_ERR "%s(): unrecognized Vcc %u\n", __FUNCTION__,
  	   configure->vcc);
      return -1;
  }

  /* Reset the slot(s) using the controls in the BCR */
  mask = 0;

  switch (configure->sock)
  {
    case 0 : mask = FHH_BCR_CF1_RST; break;
    case 1 : mask = FHH_BCR_CF2_RST; break;
  }

  save_flags_cli(flags);

  value = flexanet_BCR;
  value = (configure->reset) ? (value | mask) : (value & ~mask);
  FHH_BCR = flexanet_BCR = value;

  restore_flags(flags);

  return 0;
}

static int flexanet_pcmcia_socket_init(int sock)
{
  if (sock == 0)
    set_GPIO_IRQ_edge(GPIO_CF1_NCD | GPIO_CF1_BVD1, GPIO_BOTH_EDGES);
  else if (sock == 1)
    set_GPIO_IRQ_edge(GPIO_CF2_NCD | GPIO_CF2_BVD1, GPIO_BOTH_EDGES);

  return 0;
}

static int flexanet_pcmcia_socket_suspend(int sock)
{
  if (sock == 0)
    set_GPIO_IRQ_edge(GPIO_CF1_NCD | GPIO_CF1_BVD1, GPIO_NO_EDGES);
  else if (sock == 1)
    set_GPIO_IRQ_edge(GPIO_CF2_NCD | GPIO_CF2_BVD1, GPIO_NO_EDGES);

  return 0;
}

/*
 * The set of socket operations
 *
 */
struct pcmcia_low_level flexanet_pcmcia_ops = {
  init:			flexanet_pcmcia_init,
  shutdown:		flexanet_pcmcia_shutdown,
  socket_state:		flexanet_pcmcia_socket_state,
  get_irq_info:		flexanet_pcmcia_get_irq_info,
  configure_socket:	flexanet_pcmcia_configure_socket,

  socket_init:		flexanet_pcmcia_socket_init,
  socket_suspend:	flexanet_pcmcia_socket_suspend,
};

