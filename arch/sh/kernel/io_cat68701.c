/* 
 * linux/arch/sh/kernel/io_cat68701.c
 *
 * Copyright (C) 2000  Niibe Yutaka
 *               2001  Yutaro Ebihara
 *
 * I/O routine and setup routines for A-ONE Corp CAT-68701 SH7708 Board
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#include <asm/io.h>
#include <asm/machvec.h>
#include <linux/config.h>
#include <linux/module.h>

#define SH3_PCMCIA_BUG_WORKAROUND 1
#define DUMMY_READ_AREA6	  0xba000000

#define PORT2ADDR(x) (cat68701_isa_port2addr(x))

static inline void delay(void)
{
	ctrl_inw(0xa0000000);
}

unsigned char cat68701_inb(unsigned long port)
{
	return *(volatile unsigned char*)PORT2ADDR(port);
}

unsigned short cat68701_inw(unsigned long port)
{
	return *(volatile unsigned short*)PORT2ADDR(port);
}

unsigned int cat68701_inl(unsigned long port)
{
	return *(volatile unsigned long*)PORT2ADDR(port);
}

unsigned char cat68701_inb_p(unsigned long port)
{
	unsigned long v = *(volatile unsigned char*)PORT2ADDR(port);

	delay();
	return v;
}

unsigned short cat68701_inw_p(unsigned long port)
{
	unsigned long v = *(volatile unsigned short*)PORT2ADDR(port);

	delay();
	return v;
}

unsigned int cat68701_inl_p(unsigned long port)
{
	unsigned long v = *(volatile unsigned long*)PORT2ADDR(port);

	delay();
	return v;
}

void cat68701_insb(unsigned long port, void *buffer, unsigned long count)
{
	unsigned char *buf=buffer;
	while(count--) *buf++=inb(port);
}

void cat68701_insw(unsigned long port, void *buffer, unsigned long count)
{
	unsigned short *buf=buffer;
	while(count--) *buf++=inw(port);
#ifdef SH3_PCMCIA_BUG_WORKAROUND
	ctrl_inb (DUMMY_READ_AREA6);
#endif
}

void cat68701_insl(unsigned long port, void *buffer, unsigned long count)
{
	unsigned long *buf=buffer;
	while(count--) *buf++=inl(port);
#ifdef SH3_PCMCIA_BUG_WORKAROUND
	ctrl_inb (DUMMY_READ_AREA6);
#endif
}

void cat68701_outb(unsigned char b, unsigned long port)
{
	*(volatile unsigned char*)PORT2ADDR(port) = b;
}

void cat68701_outw(unsigned short b, unsigned long port)
{
	*(volatile unsigned short*)PORT2ADDR(port) = b;
}

void cat68701_outl(unsigned int b, unsigned long port)
{
        *(volatile unsigned long*)PORT2ADDR(port) = b;
}

void cat68701_outb_p(unsigned char b, unsigned long port)
{
	*(volatile unsigned char*)PORT2ADDR(port) = b;
	delay();
}

void cat68701_outw_p(unsigned short b, unsigned long port)
{
	*(volatile unsigned short*)PORT2ADDR(port) = b;
	delay();
}

void cat68701_outl_p(unsigned int b, unsigned long port)
{
	*(volatile unsigned long*)PORT2ADDR(port) = b;
	delay();
}

void cat68701_outsb(unsigned long port, const void *buffer, unsigned long count)
{
	const unsigned char *buf=buffer;
	while(count--) outb(*buf++, port);
}

void cat68701_outsw(unsigned long port, const void *buffer, unsigned long count)
{
	const unsigned short *buf=buffer;
	while(count--) outw(*buf++, port);
#ifdef SH3_PCMCIA_BUG_WORKAROUND
	ctrl_inb (DUMMY_READ_AREA6);
#endif
}

void cat68701_outsl(unsigned long port, const void *buffer, unsigned long count)
{
	const unsigned long *buf=buffer;
	while(count--) outl(*buf++, port);
#ifdef SH3_PCMCIA_BUG_WORKAROUND
	ctrl_inb (DUMMY_READ_AREA6);
#endif
}

unsigned char cat68701_readb(unsigned long addr)
{
	return *(volatile unsigned char*)addr;
}

unsigned short cat68701_readw(unsigned long addr)
{
	return *(volatile unsigned short*)addr;
}

unsigned int cat68701_readl(unsigned long addr)
{
	return *(volatile unsigned long*)addr;
}

void cat68701_writeb(unsigned char b, unsigned long addr)
{
	*(volatile unsigned char*)addr = b;
}

void cat68701_writew(unsigned short b, unsigned long addr)
{
	*(volatile unsigned short*)addr = b;
}

void cat68701_writel(unsigned int b, unsigned long addr)
{
        *(volatile unsigned long*)addr = b;
}

void * cat68701_ioremap(unsigned long offset, unsigned long size)
{
	return (void *) P2SEGADDR(offset);
}
EXPORT_SYMBOL(cat68701_ioremap);

void cat68701_iounmap(void *addr)
{
}
EXPORT_SYMBOL(cat68701_iounmap);

unsigned long cat68701_isa_port2addr(unsigned long offset)
{
  /* CompactFlash (IDE) */
  if(((offset >= 0x1f0) && (offset <= 0x1f7)) || (offset==0x3f6))
    return 0xba000000 + offset;

  /* INPUT PORT */
  if((offset >= 0x3fc) && (offset <= 0x3fd))
    return 0xb4007000 + offset;

  /* OUTPUT PORT */
  if((offset >= 0x3fe) && (offset <= 0x3ff))
    return 0xb4007400 + offset;

  return offset + 0xb4000000; /* other I/O (EREA 5)*/
}


int cat68701_irq_demux(int irq)
{
  if(irq==13) return 14;
  if(irq==7)  return 10;
  return irq;
}



/*-------------------------------------------------------*/

void setup_cat68701(){
  /* dummy read erea5 (CS8900A) */
}

void init_cat68701_IRQ(){
  make_imask_irq(10);
  make_imask_irq(14);
}

#ifdef CONFIG_HEARTBEAT
#include <linux/sched.h>
void heartbeat_cat68701()
{
        static unsigned int cnt = 0, period = 0 , bit = 0;
        cnt += 1;
        if (cnt < period) {
                return;
        }
        cnt = 0;

        /* Go through the points (roughly!):
         * f(0)=10, f(1)=16, f(2)=20, f(5)=35,f(inf)->110
         */
        period = 110 - ( (300<<FSHIFT)/
                         ((avenrun[0]/5) + (3<<FSHIFT)) );

	if(bit){ bit=0; }else{ bit=1; }
	outw(bit<<15,0x3fe);
}
#endif /* CONFIG_HEARTBEAT */
