/*
 *
 * By Dustin McIntire (dustin@sensoria.com) (c)2001
 * 
 * Setup and IRQ handling code for the HD64465 companion chip.
 * by Greg Banks <gbanks@pocketpenguins.com>
 * Copyright (c) 2000 PocketPenguins Inc
 *
 * Derived from setup_hd64465.c which bore the message:
 * Greg Banks <gbanks@pocketpenguins.com>
 * Copyright (c) 2000 PocketPenguins Inc and
 * Copyright (C) 2000 YAEGASHI Takeshi
 * and setup_cqreek.c which bore message:
 * Copyright (C) 2000  Niibe Yutaka
 * 
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Setup and IRQ functions for a Hitachi Big Sur Evaluation Board.
 * 
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/irq.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>

#include <asm/io_keywest.h>
#include <asm/hd64465.h>
#include <asm/keywest.h>

//#define KEYWEST_DEBUG 3
#undef KEYWEST_DEBUG

//LED
#define ALPHA_LED       0xb1ffe038

#ifdef KEYWEST_DEBUG
#define DPRINTK(args...)	printk(args)
#define DIPRINTK(n, args...)	if (KEYWEST_DEBUG>(n)) printk(args)
#else
#define DPRINTK(args...)
#define DIPRINTK(n, args...)
#endif /* KEYWEST_DEBUG */

#ifdef CONFIG_HD64465
extern int hd64465_irq_demux(int irq);
#endif /* CONFIG_HD64465 */

extern void keywest_alpha(int i, char ch);

int keywest_irq_demux(int irq)
{
	int demux_irq = irq;

#ifdef CONFIG_HD64465
	demux_irq = hd64465_irq_demux(demux_irq);
#endif /* CONFIG_HD64465 */

#ifndef CONFIG_SH_GENERIC
	demux_irq = ipr_irq_demux(demux_irq);
#endif

	return demux_irq;
}


void __init init_keywest_IRQ(void)
{
	if (!MACH_KEYWEST) return;

}

void keywest_puts(const char* s)
{
	int n = 0;

	while (*s)    { keywest_alpha(n++, *s++); }
	while (n < 8) { keywest_alpha(n++, ' ');  }
}
EXPORT_SYMBOL(keywest_puts);

void keywest_printf(const char* fmt, ...)
{
	char buffer[32];
	va_list ap;

	va_start(ap, fmt);
        vsprintf(buffer, fmt, ap);
	va_end(ap);
	keywest_puts(buffer);
}
EXPORT_SYMBOL(keywest_printf);

static void tahoe_init(void)
{
#ifdef CONFIG_HD64465
	if (inw(HD64465_REG_SDID) != HD64465_SDID) {
		printk("Tahoe board is not present.\n");

		return;
	 }

	printk("Hitachi HD64465 chip support (%d.%d)\n",
		inw(HD64465_REG_SRR) >> 8, inw(HD64465_REG_SRR) & 0xFF);

	outw(~0, HD64465_REG_NIMR);
	//outw(0x03, HD64465_REG_SPLLCR);
#ifdef CONFIG_VT
	outw(inw(HD64465_REG_SMSCR) & (~HD64465_SMSCR_PS2ST), HD64465_REG_SMSCR); /* PS2   */ 
	//outw(inw(HD64465_REG_SMSCR) & (~HD64465_SMSCR_KBCST), HD64465_REG_SMSCR); /* keyboard  */ 
#endif
#ifdef CONFIG_SERIAL
	outw(inw(HD64465_REG_SMSCR) & (~HD64465_SMSCR_UARTST), HD64465_REG_SMSCR); /* UART */
#endif
#ifdef CONFIG_PARPORT
	outw(inw(HD64465_REG_SMSCR) & (~HD64465_SMSCR_PPST), HD64465_REG_SMSCR); /* PARPORT */
#endif

#ifdef CONFIG_PCMCIA
	outw(inw(HD64465_REG_SMSCR) & (~HD64465_SMSCR_PC0ST), HD64465_REG_SMSCR); /* PC0    */
	outw(inw(HD64465_REG_SMSCR) & (~HD64465_SMSCR_PC1ST), HD64465_REG_SMSCR); /* PC1    */

	/* turn on PCMCIA area 5 and 6 in BCR1 */
	* (unsigned short *) 0xffffff60 |= 0x3;

	/* must put both area 5 and 6 into 8 or 16 bit mode in BCR2 */
	* (unsigned short *) 0xffffff62 &= ~0x3c00;
	* (unsigned short *) 0xffffff62 |= 0x2800;		/* 16 bit mode */

	/* setup all the PCMCIA dual purpose pins to be PCMCIA pins */
	* (unsigned short *) 0xa4000108 = 0x0000;   /* port E, all pins */
	* (unsigned short *) 0xa400010a = 0x0000;   /* port F, all pins */
	* (unsigned short *) 0xa400010c = 0x0000;   /* port G, all pins */

	/* put the PCMCIA interrupts into edge triggered mode */
	// BAD * (unsigned short *) 0xb0005004 |= 0x6000;

#endif

	/* 0x3c00*/
	* (unsigned short *) 0xa400010e &= ~0x3f00; /* port H, pins 4, 5 & 6 */

	printk("Hitachi HD64465 configured at 0x%x on irq %d\n",
		CONFIG_HD64465_IOBASE, HD64465_IRQ_BASE);
#endif
}

static void keywest_init(void)
{
	keywest_printf("%s", "Key West");
#ifdef CONFIG_PCI
	/*
	 * make sure these pins are in PCI mode (PINT)
	 */
	* (unsigned short *) 0xa4000104 = 0xaaaa;	/* port C, all inputs */
	* (unsigned short *) 0xa4000108 = 0x0000;   /* port E, all pins */
	* (unsigned short *) 0xa4000110 = 0x0000;   /* port J, all pins */
#ifndef CONFIG_PCMCIA
	* (unsigned short *) 0xffffff62 |= 0x3c00;	/* Area 5/6 in 32 bit mode */

	/* Make sure Area 5 & 6 are in PCI mode */
	* (unsigned short *) 0xffffff60 &= ~0x3;   /* 5 & 6 as normal memory *
	* (unsigned short *) 0xffffff60 &= ~0x1e0; /* ordinary memory 5 & 6 */
#endif
#endif
}

int __init setup_keywest(void)
{
	static int done = 0; /* run this only once */

	if (!MACH_KEYWEST || done) return 0;
	done = 1;

	keywest_init();
	tahoe_init();
#if defined (CONFIG_HD64465) && defined (CONFIG_SERIAL) 
	/* remap IO ports for first ISA serial port to HD64465 UART */
	mach_port_map(0x3f8, 8, CONFIG_HD64465_IOBASE + 0x8000, 1);
#endif /* CONFIG_HD64465 && CONFIG_SERIAL */

    return 0;
}

void keywest_alpha(int i, char ch)
{
	volatile unsigned char *alpha = (unsigned char *)ALPHA_LED;
        *(alpha + i) = ch;
}

#if 0 //keywest board(rev.4) not support status LED
void keywest_led_on()
{
	volatile unsigned char *led = (unsigned char *)STATUS_LED;
        *led = 0x82;
}

void keywest_led_off()
{
        volatile unsigned char *led = (unsigned char *)STATUS_LED;
        *led = 0x02;
}
#endif

module_init(setup_keywest);
