/*
 * drivers/pcmcia/sa1100_graphicsclient.c
 *
 * PCMCIA implementation routines for Graphics Client Plus
 *
 * 9/12/01   Woojung
 *    Turn power OFF at startup
 * 1/31/2001 Woojung Huh
 *    Fix for GC Plus PCMCIA Reset Problem
 * 2/27/2001 Woojung Huh [whuh@applieddata.net]
 *    Fix
 *
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include "sa1100_generic.h"

#error This is broken!

#define	S0_CD_IRQ		60				// Socket 0 Card Detect IRQ
#define	S0_STS_IRQ		55				// Socket 0 PCMCIA IRQ

static volatile unsigned long *PCMCIA_Status = 
		((volatile unsigned long *) ADS_p2v(_ADS_CS_STATUS));

static volatile unsigned long *PCMCIA_Power = 
		((volatile unsigned long *) ADS_p2v(_ADS_CS_PR));

static int gcplus_pcmcia_init(struct pcmcia_init *init)
{
  int irq, res;

  // Reset PCMCIA
  // Reset Timing for CPLD(U2) version 8001E or later
  *PCMCIA_Power &= ~ ADS_CS_PR_A_RESET;
  udelay(12);			// 12 uSec

  *PCMCIA_Power |= ADS_CS_PR_A_RESET;
  mdelay(30);			// 30 mSec

  // Turn off 5V
  *PCMCIA_Power &= ~0x03;

  /* Register interrupts */
  irq = S0_CD_IRQ;
  res = request_irq(irq, init->handler, SA_INTERRUPT, "PCMCIA 0 CD", NULL);
  if (res < 0) {
	  printk(KERN_ERR "%s: Request for IRQ %lu failed\n", __FUNCTION__, irq);
	  return	-1;
  }

  return 1;			// 1 PCMCIA Slot
}

static int gcplus_pcmcia_shutdown(void)
{
  /* disable IRQs */
  free_irq( S0_CD_IRQ, NULL);
  
  /* Shutdown PCMCIA power */
  mdelay(2);						// 2msec
  *PCMCIA_Power &= ~0x03;

  return 0;
}

static int gcplus_pcmcia_socket_state(struct pcmcia_state_array
				       *state_array){
  unsigned long levels;

  if(state_array->size<1) return -1;

  memset(state_array->state, 0, 
	 (state_array->size)*sizeof(struct pcmcia_state));

  levels=*PCMCIA_Status;

  state_array->state[0].detect=(levels & ADS_CS_ST_A_CD)?1:0;
  state_array->state[0].ready=(levels & ADS_CS_ST_A_READY)?1:0;
  state_array->state[0].bvd1= 0;
  state_array->state[0].bvd2= 0;
  state_array->state[0].wrprot=0;
  state_array->state[0].vs_3v=0;
  state_array->state[0].vs_Xv=0;

  return 1;
}

static int gcplus_pcmcia_get_irq_info(struct pcmcia_irq_info *info)
{
	if (info->sock > 1)
		return -1;

	if (info->sock == 0)
		info->irq = S0_STS_IRQ;

  	return 0;
}

static int gcplus_pcmcia_configure_socket(const struct pcmcia_configure
					   *configure)
{
  unsigned long flags;

  if(configure->sock>1) return -1;

  save_flags_cli(flags);

  switch (configure->vcc) {
  case 0:
	  *PCMCIA_Power &= ~(ADS_CS_PR_A_3V_POWER | ADS_CS_PR_A_5V_POWER);
    break;

  case 50:
	  *PCMCIA_Power &= ~(ADS_CS_PR_A_3V_POWER | ADS_CS_PR_A_5V_POWER);
	  *PCMCIA_Power |= ADS_CS_PR_A_5V_POWER;
	break;

  case 33:
	  *PCMCIA_Power &= ~(ADS_CS_PR_A_3V_POWER | ADS_CS_PR_A_5V_POWER);
	  *PCMCIA_Power |= ADS_CS_PR_A_3V_POWER;
    break;

  default:
    printk(KERN_ERR "%s(): unrecognized Vcc %u\n", __FUNCTION__,
	   configure->vcc);
    restore_flags(flags);
    return -1;
  }

  /* Silently ignore Vpp, output enable, speaker enable. */

  // Reset PCMCIA
  *PCMCIA_Power &= ~ ADS_CS_PR_A_RESET;
  udelay(12);

  *PCMCIA_Power |= ADS_CS_PR_A_RESET;
  mdelay(30);

  restore_flags(flags);

  return 0;
}

static int gcplus_pcmcia_socket_init(int sock)
{
  return 0;
}

static int gcplus_pcmcia_socket_suspend(int sock)
{
  return 0;
}

struct pcmcia_low_level gcplus_pcmcia_ops = { 
  init:			gcplus_pcmcia_init,
  shutdown:		gcplus_pcmcia_shutdown,
  socket_state:		gcplus_pcmcia_socket_state,
  get_irq_info:		gcplus_pcmcia_get_irq_info,
  configure_socket:	gcplus_pcmcia_configure_socket,

  socket_init:		gcplus_pcmcia_socket_init,
  socket_suspend:	gcplus_pcmcia_socket_suspend,
};

