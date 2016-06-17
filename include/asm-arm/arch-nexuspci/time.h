/*
 * linux/include/asm-arm/arch-nexuspci/time.h
 *
 * Copyright (c) 1997, 1998, 1999, 2000 FutureTV Labs Ltd.
 *
 * The FTV PCI card has no real-time clock.  We get timer ticks from the
 * SCC chip.
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

static void timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	static int count = 25;
	unsigned char stat = __raw_readb(DUART_BASE + 0x14);
	if (!(stat & 0x10))
		return;		/* Not for us */

	/* Reset counter */
	__raw_writeb(0x90, DUART_BASE + 8);

	if (--count == 0) {
		static int state = 1;
		state ^= 1;
		__raw_writeb(0x1a + state, INTCONT_BASE);
		__raw_writeb(0x18 + state, INTCONT_BASE);
		count = 50;
	}

	/* Wait for slow rise time */
	__raw_readb(DUART_BASE + 0x14);
	__raw_readb(DUART_BASE + 0x14);
	__raw_readb(DUART_BASE + 0x14);
	__raw_readb(DUART_BASE + 0x14);
	__raw_readb(DUART_BASE + 0x14);
	__raw_readb(DUART_BASE + 0x14);

	do_timer(regs);	
}

static inline void setup_timer(void)
{
	int tick = 3686400 / 16 / 2 / 100;

	__raw_writeb(tick & 0xff, DUART_BASE + 0x1c);
	__raw_writeb(tick >> 8, DUART_BASE + 0x18);
	__raw_writeb(0x80, DUART_BASE + 8);
	__raw_writeb(0x10, DUART_BASE + 0x14);

	timer_irq.handler = timer_interrupt;
	timer_irq.flags = SA_SHIRQ;

	setup_arm_irq(IRQ_TIMER, &timer_irq);
}
