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

#ifndef __PCIC_H__
#define __PCIC_H__

/*****************************************************************************
 *                 pcmcia controller chip (PCIC) support                     *
 *               (eventually, move this to a separate file)                  *
 *****************************************************************************/
#include <i386/isa/ic/i82365.h>

/*
 * Each PCIC chip (82365SL or clone) can handle two card slots, and there
 * can be up to four PCICs in a system.  (On some machines, not all of the
 * address lines are decoded, so a card may appear to be in more than one
 * slot.)
 */
#define MAXSLOT 8

/*
 * To access a register on the PCIC for a particular slot, you
 * first write the correct OFFSET value for that slot in the
 * INDEX register for the PCIC controller.  You then read or write
 * the value from or to the DATA register for that controller.
 *
 * The first pair of chips shares I/O addresses for DATA and INDEX,
 * as does the second pair.   (To the programmer, it looks like each
 * pair is a single chip.)  The i/o port addresses are hard-wired
 * into the PCIC; so the following addresses should be valid for
 * any machine that uses this chip.
 */

#define PCIC_INDEX_0	0x3E0	/* index reg, chips 0 and 1 */
#define PCIC_DATA_0	0x3E1	/* data register, chips 0 and 1 */
#define PCIC_INDEX_1	0x3E2	/* index reg, chips 1 and 2 */
#define PCIC_DATA_1	0x3E3	/* data register, chips 1 and 2 */

/*
 * Given a slot number, calculate the INDEX and DATA registers
 * to talk to that slot.  OFFSET is added to the register number
 * to address the registers for a particular slot.
 */
#define INDEX(slot) ((slot) < 4 ? PCIC_INDEX_0 : PCIC_INDEX_1)
#define DATA(slot) ((slot) < 4 ? PCIC_DATA_0 : PCIC_DATA_1)
#define OFFSET(slot) ((slot) % 4 * 0x40)

/*
 * There are 5 sets (windows) of memory mapping registers on the PCIC chip
 * for each slot, numbered 0..4.
 *
 * They start at 10/50 hex within the chip's register space (not system
 * I/O space), and are eight addresses apart.  These are actually pairs of
 * 8-bit-wide registers (low byte first, then high byte) since the
 * address fields are actually 12 bits long.  The upper bits are used
 * for other things like 8/16-bit select and wait states.
 *
 * Memory mapping registers include start/stop addresses to define the
 * region to be mapped (in terms of system memory addresses), and
 * an offset register to allow for translation from system space
 * to card space.  The lower 12 bits aren't included in these, so memory is
 * mapped in 4K chunks.
 */
#define MEM_START_ADDR(window) (((window) * 0x08) + 0x10)
#define MEM_STOP_ADDR(window) (((window) * 0x08) + 0x12)
#define MEM_OFFSET(window) (((window) * 0x08) + 0x14)
/*
 * this bit gets set in the address window enable register (PCIC_ADDRWINE)
 * to enable a particular address window.
 */
#define MEM_ENABLE_BIT(window) ((1) << (window))

/*
 * There are two i/o port addressing windows.  I/O ports cannot be
 * relocated within system i/o space (unless the card doesn't decode
 * all of the address bits); unlike card memory, there is no address
 * translation offset.
 */
#define IO_START_ADDR(window) ((window) ? PCIC_IO1_STL : PCIC_IO0_STL)
#define IO_STOP_ADDR(window) ((window) ? PCIC_IO1_SPL : PCIC_IO0_SPL)
#define IO_ENABLE_BIT(window) ((window) ? PCIC_IO1_EN : PCIC_IO0_EN)
#define IO_CS16_BIT(window) ((window) ? PCIC_IO1_CS16 : PCIC_IO0_CS16)

/*
 * types of mapped memory
 */
enum memtype { COMMON, ATTRIBUTE };

/*
 * read a byte from a pcic register for a particular slot
 */
static __inline unsigned char
pcic_getb (int slot, int reg)
{
    outb (INDEX(slot), OFFSET (slot) + reg);
    return inb (DATA (slot));
}

/*
 * write a byte to a pcic register for a particular slot
 */
static __inline void
pcic_putb (int slot, int reg, unsigned char val)
{
    outb (INDEX(slot), OFFSET (slot) + reg);
    outb (DATA (slot), val);
}

/*
 * read a word from a pcic register for a particular slot
 */
static __inline unsigned short
pcic_getw (int slot, int reg)
{
    return pcic_getb (slot, reg) | (pcic_getb (slot, reg+1) << 8);
}

/*
 * write a word to a pcic register at a particular slot
 */
static __inline void
pcic_putw (int slot, int reg, unsigned short val)
{
    pcic_putb (slot, reg, val & 0xff);
    pcic_putb (slot, reg + 1, (val >> 8) & 0xff);
}


void pcic_print_regs (int slot);
void pcic_map_memory (int slot, int window, unsigned long sys_addr,
                      unsigned long card_addr, unsigned long length,
                      enum memtype type, int width);
void pcic_unmap_memory (int slot, int window);
void pcic_map_io (int slot, int window, unsigned short base,
                  unsigned short length, unsigned short width);
#ifdef TEST
void pcic_unmap_io (int slot, int window);
#endif /* TEST */
void pcic_map_irq (int slot, int irq);
void pcic_power_on (int slot);
void pcic_power_off (int slot);
void pcic_reset (int slot);


#endif /* __PCIC_H__ */
