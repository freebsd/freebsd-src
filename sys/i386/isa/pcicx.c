/*-
 * TODO:
 * [1] integrate into current if_ed.c
 * [2] parse tuples to find out where to map the shared memory buffer,
 *     and what to write into the configuration register
 * [3] move pcic-specific code into a separate module.
 *
 * Device driver for IBM PCMCIA Credit Card Adapter for Ethernet,
 * if_ze.c
 *
 * Based on the Device driver for National Semiconductor DS8390 ethernet
 * adapters by David Greenman.  Modifications for PCMCIA by Keith Moore.
 * Adapted for FreeBSD 1.1.5 by Jordan Hubbard.
 *
 * Currently supports only the IBM Credit Card Adapter for Ethernet, but
 * could probably work with other PCMCIA cards also, if it were modified
 * to get the locations of the PCMCIA configuration option register (COR)
 * by parsing the configuration tuples, rather than by hard-coding in
 * the value expected by IBM's card.
 *
 * Sources for data on the PCMCIA/IBM CCAE specific portions of the driver:
 *
 * [1] _Local Area Network Credit Card Adapters Technical Reference_,
 *     IBM Corp., SC30-3585-00, part # 33G9243.
 * [2] "pre-alpha" PCMCIA support code for Linux by Barry Jaspan.
 * [3] Intel 82536SL PC Card Interface Controller Data Sheet, Intel
 *     Order Number 290423-002
 * [4] National Semiconductor DP83902A ST-NIC (tm) Serial Network
 *     Interface Controller for Twisted Pair data sheet.
 *
 *
 * Copyright (C) 1993, David Greenman. This software may be used, modified,
 *   copied, distributed, and sold, in both source and binary form provided
 *   that the above copyright and these terms are retained. Under no
 *   circumstances is the author responsible for the proper functioning
 *   of this software, nor does the author assume any responsibility
 *   for damages incurred with its use.
 */
#include <sys/param.h>
#if defined(__FreeBSD__)
#include <sys/systm.h>
#include <sys/kernel.h>
#include <machine/clock.h>
#endif
#include <i386/isa/isa_device.h>
#include <i386/isa/pcic.h>

/*
 * map a portion of the card's memory space into system memory
 * space.
 *
 * slot = # of the slot the card is plugged into
 * window = which pcic memory map registers to use (0..4)
 * sys_addr = base system PHYSICAL memory address where we want it.  must
 *	      be on an appropriate boundary (lower 12 bits are zero).
 * card_addr = the base address of the card's memory to correspond
 *             to sys_addr
 * length = length of the segment to map (may be rounded up as necessary)
 * type = which card memory space to map (attribute or shared)
 * width = 1 for byte-wide mapping; 2 for word (16-bit) mapping.
 */

void
pcic_map_memory (int slot, int window, unsigned long sys_addr,
		 unsigned long card_addr, unsigned long length,
		 enum memtype type, int width)
{
    unsigned short offset;
    unsigned short mem_start_addr;
    unsigned short mem_stop_addr;

    sys_addr >>= 12;
    card_addr >>= 12;
    length >>= 12;
    /*
     * compute an offset for the chip such that
     * (sys_addr + offset) = card_addr
     * but the arithmetic is done modulo 2^14
     */
    offset = (card_addr - sys_addr) & 0x3FFF;
    /*
     * now OR in the bit for "attribute memory" if necessary
     */
    if (type == ATTRIBUTE) {
	offset |= (PCIC_REG << 8);
	/* REG == "region active" pin on card */
    }
    /*
     * okay, set up the chip memory mapping registers, and turn
     * on the enable bit for this window.
     * if we are doing 16-bit wide accesses (width == 2),
     * turn on the appropriate bit.
     *
     * XXX for now, we set all of the wait state bits to zero.
     * Not really sure how they should be set.
     */
    mem_start_addr = sys_addr & 0xFFF;
    if (width == 2)
	mem_start_addr |= (PCIC_DATA16 << 8);
    mem_stop_addr = (sys_addr + length) & 0xFFF;

    pcic_putw (slot, MEM_START_ADDR(window), mem_start_addr);
    pcic_putw (slot, MEM_STOP_ADDR(window), mem_stop_addr);
    pcic_putw (slot, MEM_OFFSET(window), offset);
    /*
     * Assert the bit (PCIC_MEMCS16) that says to decode all of
     * the address lines.
     */
    pcic_putb (slot, PCIC_ADDRWINE,
	       pcic_getb (slot, PCIC_ADDRWINE) |
	       MEM_ENABLE_BIT(window) | PCIC_MEMCS16);
}

void
pcic_unmap_memory (int slot, int window)
{
    /*
     * seems like we need to turn off the enable bit first, after which
     * we can clear the registers out just to be sure.
     */
    pcic_putb (slot, PCIC_ADDRWINE,
	       pcic_getb (slot, PCIC_ADDRWINE) & ~MEM_ENABLE_BIT(window));
    pcic_putw (slot, MEM_START_ADDR(window), 0);
    pcic_putw (slot, MEM_STOP_ADDR(window), 0);
    pcic_putw (slot, MEM_OFFSET(window), 0);
}

/*
 * map a range of addresses into system i/o space
 * (no translation of i/o addresses is possible)
 *
 * 'width' is:
 * + 0 to tell the PCIC to generate the ISA IOCS16* signal from
 *   the PCMCIA IOIS16* signal.
 * + 1 to select 8-bit width
 * + 2 to select 16-bit width
 */

void
pcic_map_io (int slot, int window, unsigned short base, unsigned short length,
	     unsigned short width)
{
    unsigned char x;

    pcic_putw (slot, IO_START_ADDR(window), base);
    pcic_putw (slot, IO_STOP_ADDR(window), base+length-1);
    /*
     * select the bits that determine whether
     * an i/o operation is 8 or 16 bits wide
     */
    x = pcic_getb (slot, PCIC_IOCTL);
    switch (width) {
    case 0:			/* PCMCIA card decides */
	if (window)
	    x = (x & 0xf0) | PCIC_IO1_CS16;
	else
	    x = (x & 0x0f) | PCIC_IO0_CS16;
	break;
    case 1:			/* 8 bits wide */
	break;
    case 2:			/* 16 bits wide */
	if (window)
	    x = (x & 0xf0) | PCIC_IO1_16BIT;
	else
	    x = (x & 0x0f) | PCIC_IO0_16BIT;
	break;
    }
    pcic_putb (slot, PCIC_IOCTL, x);
    pcic_putb (slot, PCIC_ADDRWINE,
	       pcic_getb (slot, PCIC_ADDRWINE) | IO_ENABLE_BIT(window));
}

#ifdef TEST
void
pcic_unmap_io (int slot, int window)
{
    pcic_putb (slot, PCIC_ADDRWINE,
	       pcic_getb (slot, PCIC_ADDRWINE) & ~IO_ENABLE_BIT(window));
    pcic_putw (slot, IO_START_ADDR(window), 0);
    pcic_putw (slot, IO_STOP_ADDR(window), 0);
}
#endif /* TEST */

/*
 * tell the PCIC which irq we want to use.  only the following are legal:
 * 3, 4, 5, 7, 9, 10, 11, 12, 14, 15
 *
 * NB: 'irq' is an interrupt NUMBER, not a MASK as in struct isa_device.
 */

void
pcic_map_irq (int slot, int irq)
{
    if (irq < 3 || irq == 6 || irq == 8 || irq == 13 || irq > 15) {
	printf ("zp: pcic_map_irq (slot %d): illegal irq %d\n", slot, irq);
	return;
    }
    pcic_putb (slot, PCIC_INT_GEN,
	       pcic_getb (slot, PCIC_INT_GEN) | (irq & 0x0F));
}

void
pcic_power_on (int slot)
{
    pcic_putb (slot, PCIC_STATUS,
	       pcic_getb (slot, PCIC_STATUS) | PCIC_POW);
    DELAY (100000);
    pcic_putb (slot, PCIC_POWER,
	       pcic_getb (slot, PCIC_POWER) | PCIC_DISRST | PCIC_PCPWRE);
    DELAY (100000);
    pcic_putb (slot, PCIC_POWER,
	       pcic_getb (slot, PCIC_POWER) | PCIC_OUTENA);
}

void
pcic_power_off (int slot)
{
    pcic_putb (slot, PCIC_POWER,
	       pcic_getb (slot, PCIC_POWER) & ~(PCIC_OUTENA|PCIC_PCPWRE));
}

void
pcic_reset (int slot)
{
    /* assert RESET (by clearing a bit!), wait a bit, and de-assert it */
    pcic_putb (slot, PCIC_INT_GEN,
	       pcic_getb (slot, PCIC_INT_GEN) & ~PCIC_CARDRESET);
    DELAY (100000);
    pcic_putb (slot, PCIC_INT_GEN,
	       pcic_getb (slot, PCIC_INT_GEN) | PCIC_CARDRESET);
}

