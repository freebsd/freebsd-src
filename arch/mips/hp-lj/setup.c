/*
 * Setup pointers to hardware-dependent routines.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1997, 1998 by Ralf Baechle
 */
#include <linux/config.h>
#include <linux/hdreg.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ide.h>
#include <linux/bootmem.h>
#include <asm/mc146818rtc.h>
#include <asm/reboot.h>
#include <asm/time.h>
#include <asm/hp-lj/asic.h>
#include "utils.h"

#ifdef CONFIG_KGDB
int remote_debug = 0;
#endif

const char *get_system_type(void)
{
	return "HP LaserJet";		/* But which exactly?  */
}

static void (*timer_interrupt_service)(int irq, void *dev_id, struct pt_regs * regs) = NULL;


static void andros_timer_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
   if (!(*((volatile unsigned int*)0xbfea0010) & 0x20))  // mask = pend & en
      return;

   /* clear timer interrupt */
   {
      unsigned int tmr = *((volatile unsigned int*)0xbfe90040);   // ctl bits
      *((volatile unsigned int*)0xbfe90040) = tmr;     // write to ack
      *((volatile unsigned int*)0xbfea000c) = 0x20;    // sys int ack
   }

   /* service interrupt */
   timer_interrupt_service(irq, dev_id, regs);
}

static void harmony_timer_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
   if (!(*((volatile unsigned int*)0xbff63000) & 0x01))
      return;  // big sys     int reg, 01-timer  did it
   if (!(*((volatile unsigned int*)0xbff610a4) & 0x01))
      return;  // local small int reg, 01-timer0 did it

   *((volatile unsigned int*)0xbff610a4) = 1;   // ack local timer0 bit
   *((volatile unsigned int*)0xbff63000) = 1;   // ack global timer bit

   /* service interrupt */
   timer_interrupt_service(irq, dev_id, regs);
}


#define ASIC_IRQ_NUMBER  2


static void __init hp_time_init(struct irqaction *irq)
{
    timer_interrupt_service = irq->handler;

    if (GetAsicId() == AndrosAsic) {
       //*((volatile unsigned int*)0xbfe90000) = 0x2f;  // set by bootloader to 0x20              // prescaler
       *((volatile unsigned int*)0xbfe90040) = 0x21;    // 20-res of 1kHz,1-int ack                // control
       *((volatile unsigned int*)0xbfe90048) = 0x09;    // 09-reload val                          // reload
       *((volatile unsigned int*)0xbfe90044) = 0x09;    // 09-count val                           // count
       *((volatile unsigned int*)0xbfe90040) = 0x2f;    // 8-int enable,4-reload en,2-count down en,1-int-ack

       irq->handler = andros_timer_interrupt;
       irq->flags |= SA_INTERRUPT | SA_SHIRQ;
       printk("setting up timer in hp_time_init\n");
       setup_irq(ASIC_IRQ_NUMBER, irq);

       // enable timer interrupt
       *((volatile unsigned int*)0xbfea0000) = 0x20;

    } else if (GetAsicId() == HarmonyAsic) {

        *((volatile unsigned int*)0xbff61000) = 99;  // prescaler, 100Mz sys clk
        *((volatile unsigned int*)0xbff61028) = 0x09;  // reload reg
        *((volatile unsigned int*)0xbff61024) = 0x09;  // count reg
        *((volatile unsigned int*)0xbff61020) = 0x0b;  // 80-1khz res on timer, 2 reload en, 1 - count down en

        irq->handler = harmony_timer_interrupt;
        irq->flags |= SA_INTERRUPT | SA_SHIRQ;
        setup_irq(ASIC_IRQ_NUMBER, irq);

        *((volatile unsigned int*)0xbff610a0) |= 1;    // turn on timer0

    } else if (GetAsicId() == UnknownAsic)
        printk("Unknown asic in hp_time_init()\n");
    else
        printk("Unsupported asic in hp_time_init()\n");
}


static void hplj_restart(void)
{
   if (GetAsicId() == AndrosAsic)
       *((volatile unsigned int *) 0xbfe900c0) = 0;


    if (GetAsicId() == HarmonyAsic)
        *((volatile unsigned int *) 0xbff62030) = 0;

   printk("Restart Failed ... halting instead\n");
   while(1);
}

static void hplj_halt(void)
{
   while(1);
}


void __init hp_setup(void)
{
#ifdef CONFIG_PCI
   extern void pci_setup(void);
   pci_setup();
#endif

#ifdef CONFIG_IDE
   {
      extern struct ide_ops std_ide_ops;
      ide_ops = &std_ide_ops;
   }
#endif

   _machine_restart =(void (*)(char *)) hplj_restart;
   _machine_halt = hplj_halt;
   _machine_power_off = hplj_halt;

   board_timer_setup = hp_time_init;

#ifdef CONFIG_KGDB
   {
      extern char CommandLine[];
      remote_debug = (strstr(CommandLine, "kgdb") != NULL);
   }
#endif

   printk("HP SETUP\n");
}

int __init page_is_ram(unsigned long pagenr)
{
	return 1;
}

