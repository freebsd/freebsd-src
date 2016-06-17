/*
 * linux/include/asm-arm/arch-shark/irq.h
 *
 * by Alexander Schulz
 *
 * derived from linux/arch/ppc/kernel/i8259.c and:
 * include/asm-arm/arch-ebsa110/irq.h
 * Copyright (C) 1996-1998 Russell King
 */

#include <asm/io.h>
#define fixup_irq(x) (x)

/*
 * 8259A PIC functions to handle ISA devices:
 */

/*
 * This contains the irq mask for both 8259A irq controllers,
 * Let through the cascade-interrupt no. 2 (ff-(1<<2)==fb)
 */
static unsigned char cached_irq_mask[2] = { 0xfb, 0xff };

/*
 * These have to be protected by the irq controller spinlock
 * before being called.
 */
static void shark_disable_8259A_irq(unsigned int irq)
{
	unsigned int mask;
	if (irq<8) {
	  mask = 1 << irq;
	  cached_irq_mask[0] |= mask;
	} else {
	  mask = 1 << (irq-8);
	  cached_irq_mask[1] |= mask;
	}
	outb(cached_irq_mask[1],0xA1);
	outb(cached_irq_mask[0],0x21);
}

static void shark_enable_8259A_irq(unsigned int irq)
{
	unsigned int mask;
	if (irq<8) {
	  mask = ~(1 << irq);
	  cached_irq_mask[0] &= mask;
	} else {
	  mask = ~(1 << (irq-8));
	  cached_irq_mask[1] &= mask;
	}
	outb(cached_irq_mask[1],0xA1);
	outb(cached_irq_mask[0],0x21);
}

/*
 * Careful! The 8259A is a fragile beast, it pretty
 * much _has_ to be done exactly like this (mask it
 * first, _then_ send the EOI, and the order of EOI
 * to the two 8259s is important!
 */
static void shark_mask_and_ack_8259A_irq(unsigned int irq)
{
        if (irq & 8) {
                cached_irq_mask[1] |= 1 << (irq-8);
		inb(0xA1);              /* DUMMY */
                outb(cached_irq_mask[1],0xA1);
        } else {
                cached_irq_mask[0] |= 1 << irq;
                outb(cached_irq_mask[0],0x21);
	}
}

static void bogus_int(int irq, void *dev_id, struct pt_regs *regs)
{
	printk("Got interrupt %i!\n",irq);
}

static struct irqaction cascade;

static __inline__ void irq_init_irq(void)
{
	int irq;

	for (irq = 0; irq < NR_IRQS; irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= shark_mask_and_ack_8259A_irq;
		irq_desc[irq].mask	= shark_disable_8259A_irq;
		irq_desc[irq].unmask	= shark_enable_8259A_irq;
	}

	/* The PICs are initialized to level triggered and auto eoi!
	 * If they are set to edge triggered they lose some IRQs,
	 * if they are set to manual eoi they get locked up after
	 * a short time
	 */

	/* init master interrupt controller */
	outb(0x19, 0x20); /* Start init sequence, level triggered */
        outb(0x00, 0x21); /* Vector base */
        outb(0x04, 0x21); /* Cascade (slave) on IRQ2 */
        outb(0x03, 0x21); /* Select 8086 mode , auto eoi*/
	outb(0x0A, 0x20);
	/* init slave interrupt controller */
        outb(0x19, 0xA0); /* Start init sequence, level triggered */
        outb(0x08, 0xA1); /* Vector base */
        outb(0x02, 0xA1); /* Cascade (slave) on IRQ2 */
        outb(0x03, 0xA1); /* Select 8086 mode, auto eoi */
	outb(0x0A, 0xA0);
	outb(cached_irq_mask[1],0xA1);
	outb(cached_irq_mask[0],0x21);
	//request_region(0x20,0x2,"pic1");
	//request_region(0xA0,0x2,"pic2");

	cascade.handler = bogus_int;
	cascade.flags = 0;
	cascade.mask = 0;
	cascade.name = "cascade";
	cascade.next = NULL;
	cascade.dev_id = NULL;
	setup_arm_irq(2,&cascade);
	
}
