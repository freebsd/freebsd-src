/*
 *  Intel PCIC or compatible Controller driver
 *  May be built using LKM to make a loadable module.
 *-------------------------------------------------------------------------
 *
 * Copyright (c) 1995 Andrew McRae.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * pcic98 : PC9801 original PCMCIA controller code for NS/A,Ne,NX/C,NR/L.
 * by Noriyuki Hosobuchi <yj8n-hsbc@asahi-net.or.jp>
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/select.h>
#include <sys/interrupt.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/clock.h>

#include <i386/isa/icu.h>
#include <i386/isa/isa_device.h>

#include <pccard/i82365.h>
#ifdef	PC98
#include <pccard/pcic98reg.h>
#endif

#include <pccard/card.h>
#include <pccard/driver.h>
#include <pccard/slot.h>

#ifdef LKM
#define NPCI 0
#else
#include <pci.h>
#endif

#if NPCI > 0
#include <pci/pcivar.h>

static char *pcic_pci_probe(pcici_t tag, pcidi_t type);
static void  pcic_pci_attach(pcici_t tag, int unit);

static u_long pcic_pci_count;

static struct pci_device pcic_pci_device = {
	"pcic",
	pcic_pci_probe,
	pcic_pci_attach,
	&pcic_pci_count,
	NULL
};

DATA_SET(pcidevice_set, pcic_pci_device);

#define	 PCI_DEVICE_ID_PCIC_TI1130	0xAC12104Cul
#define	 PCI_DEVICE_ID_PCIC_TI1131	0xAC15104Cul
#define	 PCI_DEVICE_ID_PCIC_CLPD6729	0x11001013ul
#define	 PCI_DEVICE_ID_PCIC_O2MICRO	0x673A1217ul

static char *
pcic_pci_probe(pcici_t tag, pcidi_t type)
{
	switch (type) {
	case PCI_DEVICE_ID_PCIC_TI1130:
		return ("TI 1130 PCMCIA/CardBus Bridge");
	case PCI_DEVICE_ID_PCIC_TI1131:
		return ("TI 1131 PCI to PCMCIA/CardBus bridge");
	case PCI_DEVICE_ID_PCIC_CLPD6729:
		return ("Cirrus Logic PD6729/6730 PC-Card Controller");
	default:
		break;
	}
	return (NULL);
}

static void
pcic_pci_attach(pcici_t tag, int unit)
{
	int i, j;
	u_char *p;
	u_long *pl;

	if (!bootverbose)
		return;

	printf ("PCI Config space:\n");
	for (j = 0; j < 0x98; j += 16) {
		printf("%02x: ", j);
		for (i = 0; i < 16; i += 4) {
			printf(" %08x", pci_conf_read(tag, i+j));
		}
		printf("\n");
	}
	/* pci_conf_write(tag, 0x44, 1); */
	p = (u_char*)pmap_mapdev(pci_conf_read(tag, 0x10), 0x1000);
	pl = (u_long *)p;
	printf ("Cardbus Socket registers:\n");
	printf("00: ");
	for (i = 0; i < 4; i += 1)
		 printf(" %08x:", pl[i]);
	printf("\n10: ");
	for (i =4 ; i < 8; i += 1)
		printf(" %08x:", pl[i]);
	printf ("\nExCa registers:\n");
	for (i = 0; i < 0x40; i += 16)
		printf("%02x: %16D\n", i, p + 0x800 + i, " ");
	return;
}
#endif /* NPCI > 0 */
/*
 *	Prototypes for interrupt handler.
 */
static void		pcicintr	__P((int unit));
static int		pcic_ioctl __P((struct slot *, int, caddr_t));
static int		pcic_power __P((struct slot *));
static timeout_t 	pcic_reset;
static void		pcic_resume(struct slot *);
static void		pcic_disable __P((struct slot *));
static void		pcic_mapirq __P((struct slot *, int));
static timeout_t 	pcictimeout;
static struct callout_handle pcictimeout_ch
    = CALLOUT_HANDLE_INITIALIZER(&pcictimeout_ch);
#ifdef LKM
static int		pcic_handle __P((struct lkm_table *lkmtp, int cmd));
#endif
static int		pcic_memory(struct slot *, int);
static int		pcic_io(struct slot *, int);
static u_int		build_freelist(u_int);

/*
 *	Per-slot data table.
 */
static struct pcic_slot {
	int slot;			/* My slot number */
	int index;			/* Index register */
	int data;			/* Data register */
	int offset;			/* Offset value for index */
	char controller;		/* Device type */
	char revision;			/* Device Revision */
	struct slot *slotp;		/* Back ptr to slot */
	u_char (*getb)(struct pcic_slot *sp, int reg);
	void   (*putb)(struct pcic_slot *sp, int reg, u_char val);
	u_char	*regs;			/* Pointer to regs in mem */
} pcic_slots[PCIC_MAX_SLOTS];

static int		pcic_irq;
static unsigned		pcic_imask;
static struct slot_ctrl cinfo;


/*
 *	Internal inline functions for accessing the PCIC.
 */
/*
 * Read a register from the PCIC.
 */
static inline unsigned char
getb1(struct pcic_slot *sp, int reg)
{
	outb(sp->index, sp->offset + reg);
	return inb(sp->data);
}

static inline unsigned char
getb2(struct pcic_slot *sp, int reg)
{
	return (sp->regs[reg]);
}

/*
 * Write a register on the PCIC
 */
static inline void
putb1(struct pcic_slot *sp, int reg, unsigned char val)
{
	outb(sp->index, sp->offset + reg);
	outb(sp->data, val);
}

static inline void
putb2(struct pcic_slot *sp, int reg, unsigned char val)
{
	sp->regs[reg] = val;
}

/*
 * Clear bit(s) of a register.
 */
static inline void
clrb(struct pcic_slot *sp, int reg, unsigned char mask)
{
	sp->putb(sp, reg, sp->getb(sp, reg) & ~mask);
}

/*
 * Set bit(s) of a register
 */
static inline void
setb(struct pcic_slot *sp, int reg, unsigned char mask)
{
	sp->putb(sp, reg, sp->getb(sp, reg) | mask);
}

/*
 * Write a 16 bit value to 2 adjacent PCIC registers
 */
static inline void
putw(struct pcic_slot *sp, int reg, unsigned short word)
{
	sp->putb(sp, reg, word & 0xFF);
	sp->putb(sp, reg + 1, (word >> 8) & 0xff);
}


/*
 *	Loadable kernel module interface.
 */
#ifdef	LKM
/*
 *	This defines the lkm_misc module use by modload
 *	to define the module name.
 */
MOD_MISC(pcic);

/*
 *	Module handler that processes loads and unloads.
 *	Once the module is loaded, the probe routine
 *	is called to install the slots (if any).
 */
static int
pcic_handle(struct lkm_table *lkmtp, int cmd)
{
	int err = 0;	/* default = success*/

	switch( cmd) {
	case LKM_E_LOAD:

		/*
		 * Don't load twice! (lkmexists() is exported by kern_lkm.c)
		 */
		if( lkmexists( lkmtp))
			return( EEXIST);
		/*
		 *	Call the probe routine to find the slots. If
		 *	no slots exist, then don't bother loading the module.
		 */
		if (pcic_probe() == 0)
			return(ENODEV);
		break;		/* Success*/
	/*
	 *	Attempt to unload the slot driver.
	 */
	case LKM_E_UNLOAD:
		printf("Unloading PCIC driver\n");
		err = pcic_unload(lkmtp, cmd);
		break;		/* Success*/

	default:	/* we only understand load/unload*/
		err = EINVAL;
		break;
	}

	return( err);
}

/*
 * External entry point; should generally match name of .o file.  The
 * arguments are always the same for all loaded modules.  The "load",
 * "unload", and "stat" functions in "DISPATCH" will be called under
 * their respective circumstances unless their value is "lkm_nullcmd".
 * If called, they are called with the same arguments (cmd is included to
 * allow the use of a single function, ver is included for version
 * matching between modules and the kernel loader for the modules).
 *
 * Since we expect to link in the kernel and add external symbols to
 * the kernel symbol name space in a future version, generally all
 * functions used in the implementation of a particular module should
 * be static unless they are expected to be seen in other modules or
 * to resolve unresolved symbols alread existing in the kernel (the
 * second case is not likely to ever occur).
 *
 * The entry point should return 0 unless it is refusing load (in which
 * case it should return an errno from errno.h).
 */
int
pcic_mod(struct lkm_table *lkmtp, int cmd, int ver)
{
	MOD_DISPATCH(pcic, lkmtp, cmd, ver,
		pcic_handle, pcic_handle, lkm_nullcmd);
}

/*
 *	pcic_unload - Called when unloading a LKM.
 *	Disables interrupts and resets PCIC.
 */
static int
pcic_unload(struct lkm_table *lkmtp, int cmd)
{
	int	slot;
	struct pcic_slot *sp = pcic_slots;

	untimeout(pcictimeout,0, pcictimeout_ch);
	if (pcic_irq) {
		for (slot = 0; slot < PCIC_MAX_SLOTS; slot++, sp++) {
			if (sp->slotp)
				sp->putb(sp, PCIC_STAT_INT, 0);
		}
		unregister_intr(pcic_irq, pcicintr);
	}
	pccard_remove_controller(&cinfo);
	return(0);
}

#endif /* LKM */

#if 0
static void
pcic_dump_attributes (unsigned char *scratch, int maxlen)
{
	int i,j,k;

	i = 0;
	while (scratch[i] != 0xff && i < maxlen) {
		unsigned char link = scratch[i+2];

		/*
		 *	Dump attribute memory
		 */
		if (scratch[i]) {
			printf ("[%02x] ", i);
			for (j = 0; j < 2 * link + 4 && j < 128; j += 2)
				printf ("%02x ", scratch[j + i]);
			printf ("\n");
		}
		i += 4 + 2 * link;
	}
}
#endif

static u_int
build_freelist(u_int pcic_mask)
{ 
	inthand2_t *nullfunc; 
	int irq;
	u_int mask, freemask; 
 
	/* No free IRQs (yet). */ 
	freemask = 0; 
 
	/* Walk through all of the IRQ's and find any that aren't allocated. */ 
	for (irq = 1; irq < ICU_LEN; irq++) { 
		/* 
		 * If the PCIC controller can't generate it, don't
		 * bother checking to see if it it's free. 
		 */ 
		mask = 1 << irq; 
		if (!(mask & pcic_mask)) continue; 
 
		/* See if the IRQ is free. */
		if (register_intr(irq, 0, 0, nullfunc, NULL, irq) == 0) {
			/* Give it back, but add it to the mask */ 
			INTRMASK(freemask, mask); 
			unregister_intr(irq, nullfunc); 
		}
	} 
#ifdef PCIC_DEBUG
	printf("Freelist of IRQ's <0x%x>\n", freemask);
#endif
	return freemask; 
}

/*
 *	entry point from main code to map/unmap memory context.
 */
static int
pcic_memory(struct slot *slotp, int win)
{
	struct pcic_slot *sp = slotp->cdata;
	struct mem_desc *mp = &slotp->mem[win];
	int reg = mp->window * PCIC_MEMSIZE + PCIC_MEMBASE;

#ifdef	PC98
	if (sp->controller == PCIC_PC98){
	    if (mp->flags & MDF_ACTIVE){
		/* slot = 0, window = 0, sys_addr = 0xda000, length = 8KB */
		unsigned char x;
		
		if ((unsigned long)mp->start != 0xda000){
		    printf("sys_addr must be 0xda000. requested address = 0x%x\n",
			   mp->start);
		    return(EINVAL);
		}
		
		/* omajinai ??? */
		outb(PCIC98_REG0, 0);
		x = inb(PCIC98_REG1);
		x &= 0xfc;
		x |= 0x02;
		outb(PCIC98_REG1, x);
		
		outw(PCIC98_REG_PAGOFS, 0);
		
		if (mp->flags & MDF_ATTR){
		    outb(PCIC98_REG6, inb(PCIC98_REG6) | PCIC98_ATTRMEM);
		}else{
		    outb(PCIC98_REG6, inb(PCIC98_REG6) & (~PCIC98_ATTRMEM));
		}
		
		outb(PCIC98_REG_WINSEL, PCIC98_MAPWIN);
		
#if 0
		if (mp->flags & MDF_16BITS == 1){	/* 16bit */
		    outb(PCIC98_REG2, inb(PCIC98_REG2) & (~PCIC98_8BIT));
		}else{					/* 8bit */
		    outb(PCIC98_REG2, inb(PCIC98_REG2) | PCIC98_8BIT);
		}
#endif
	    }else{
		outb(PCIC98_REG_WINSEL, PCIC98_UNMAPWIN);
	    }
	    return 0;
	}
#endif	/* PC98 */

	if (mp->flags & MDF_ACTIVE) {
		unsigned long sys_addr = (unsigned long)mp->start >> 12;
		/*
		 * Write the addresses, card offsets and length.
		 * The values are all stored as the upper 12 bits of the
		 * 24 bit address i.e everything is allocated as 4 Kb chunks.
		 */
		putw(sp, reg, sys_addr & 0xFFF);
		putw(sp, reg+2, (sys_addr + (mp->size >> 12) - 1) & 0xFFF);
		putw(sp, reg+4, ((mp->card >> 12) - sys_addr) & 0x3FFF);
#if 0
		printf("card offs = card_adr = 0x%x 0x%x, sys_addr = 0x%x\n", 
			mp->card, ((mp->card >> 12) - sys_addr) & 0x3FFF,
			sys_addr);
#endif
		/*
		 *	Each 16 bit register has some flags in the upper bits.
		 */
		if (mp->flags & MDF_16BITS)
			setb(sp, reg+1, PCIC_DATA16);
		if (mp->flags & MDF_ZEROWS)
			setb(sp, reg+1, PCIC_ZEROWS);
		if (mp->flags & MDF_WS0)
			setb(sp, reg+3, PCIC_MW0);
		if (mp->flags & MDF_WS1)
			setb(sp, reg+3, PCIC_MW1);
		if (mp->flags & MDF_ATTR)
			setb(sp, reg+5, PCIC_REG);
		if (mp->flags & MDF_WP)
			setb(sp, reg+5, PCIC_WP);
#if 0
	printf("Slot number %d, reg 0x%x, offs 0x%x\n",
		sp->slot, reg, sp->offset);
	printf("Map window to sys addr 0x%x for %d bytes, card 0x%x\n",
		mp->start, mp->size, mp->card);
	printf("regs are: 0x%02x%02x 0x%02x%02x 0x%02x%02x flags 0x%x\n",
		sp->getb(sp, reg), sp->getb(sp, reg+1),
		sp->getb(sp, reg+2), sp->getb(sp, reg+3),
		sp->getb(sp, reg+4), sp->getb(sp, reg+5),
		mp->flags);
#endif
		/*
		 * Enable the memory window. By experiment, we need a delay.
		 */
		setb(sp, PCIC_ADDRWINE, (1<<win) | PCIC_MEMCS16);
		DELAY(50);
	} else {
#if 0
		printf("Unmapping window %d\n", win);
#endif
		clrb(sp, PCIC_ADDRWINE, 1<<win);
		putw(sp, reg, 0);
		putw(sp, reg+2, 0);
		putw(sp, reg+4, 0);
	}
	return(0);
}

/*
 *	pcic_io - map or unmap I/O context
 */
static int
pcic_io(struct slot *slotp, int win)
{
	int	mask, reg;
	struct pcic_slot *sp = slotp->cdata;
	struct io_desc *ip = &slotp->io[win];
#ifdef	PC98
	if (sp->controller == PCIC_PC98){
	    unsigned char x;

#if 0
	    if (win =! 0){
		printf("pcic98:Illegal PCIC I/O window request(%d)!", win);
		return(EINVAL);
	    }
#endif

	    if (ip->flags & IODF_ACTIVE){
		unsigned short base;

		x = inb(PCIC98_REG2) & 0x0f;
		if (! (ip->flags & IODF_16BIT))
		    x |= PCIC98_8BIT;

		if (ip->size > 16)	/* 128bytes mapping */
		    x |= PCIC98_MAP128;

		x |= PCIC98_IOMEMORY;
		outb(PCIC98_REG2, x);
    
		base = 0x80d0;
		outw(PCIC98_REG4, base);		/* 98side IO base */
		outw(PCIC98_REG5, ip->start);	/* card side IO base */

#ifdef	PCIC_DEBUG
		printf("pcic98: IO mapped 0x%04x(98) -> 0x%04x(Card) and width %d bytes\n",
		       base, ip->start, ip->size);
#endif
		ip->start = base;

	    }else{
		outb(PCIC98_REG2, inb(PCIC98_REG2) & (~PCIC98_IOMEMORY));
	    }
	    return 0;
	}
#endif
	switch (win) {
	case 0:
		mask = PCIC_IO0_EN;
		reg = PCIC_IO0;
		break;
	case 1:
		mask = PCIC_IO1_EN;
		reg = PCIC_IO1;
		break;
	default:
		panic("Illegal PCIC I/O window request!");
	}
	if (ip->flags & IODF_ACTIVE) {
		unsigned char x, ioctlv;

#ifdef	PCIC_DEBUG
printf("Map I/O 0x%x (size 0x%x) on Window %d\n", ip->start, ip->size, win);
#endif	/* PCIC_DEBUG */
		putw(sp, reg, ip->start);
		putw(sp, reg+2, ip->start+ip->size-1);
		x = 0;
		if (ip->flags & IODF_ZEROWS)
			x |= PCIC_IO_0WS;
		if (ip->flags & IODF_WS)
			x |= PCIC_IO_WS;
		if (ip->flags & IODF_CS16)
			x |= PCIC_IO_CS16;
		if (ip->flags & IODF_16BIT)
			x |= PCIC_IO_16BIT;
		/*
		 * Extract the current flags and merge with new flags.
		 * Flags for window 0 in lower nybble, and in upper nybble
		 * for window 1.
		 */
		ioctlv = sp->getb(sp, PCIC_IOCTL);
		DELAY(100);
		switch (win) {
		case 0:
			sp->putb(sp, PCIC_IOCTL, x | (ioctlv & 0xf0));
			break;
		case 1:
			sp->putb(sp, PCIC_IOCTL, (x << 4) | (ioctlv & 0xf));
			break;
		}
		DELAY(100);
		setb(sp, PCIC_ADDRWINE, mask);
		DELAY(100);
	} else {
		clrb(sp, PCIC_ADDRWINE, mask);
		DELAY(100);
		putw(sp, reg, 0);
		putw(sp, reg + 2, 0);
	}
	return(0);
}

/*
 *	Look for an Intel PCIC (or compatible).
 *	For each available slot, allocate a PC-CARD slot.
 */

/*
 *	VLSI 82C146 has incompatibilities about the I/O address 
 *	of slot 1.  Assume it's the only PCIC whose vendor ID is 0x84,
 *	contact Nate Williams <nate@FreeBSD.org> if incorrect.
 */
int
pcic_probe(void)
{
	int slot, i, validslots = 0;
	u_int free_irqs;
	struct slot *slotp;
	struct pcic_slot *sp;
	unsigned char c;
	static int maybe_vlsi = 0;

	/* Determine the list of free interrupts */
	free_irqs = build_freelist(PCIC_INT_MASK_ALLOWED);
	
	/*
	 *	Initialise controller information structure.
	 */
	cinfo.mapmem = pcic_memory;
	cinfo.mapio = pcic_io;
	cinfo.ioctl = pcic_ioctl;
	cinfo.power = pcic_power;
	cinfo.mapirq = pcic_mapirq;
	cinfo.reset = pcic_reset;
	cinfo.disable = pcic_disable;
	cinfo.resume = pcic_resume;
	cinfo.maxmem = PCIC_MEM_WIN;
	cinfo.maxio = PCIC_IO_WIN;
	cinfo.irqs = free_irqs;
	cinfo.imask = &pcic_imask;

#ifdef	LKM
	bzero(pcic_slots, sizeof(pcic_slots));
#endif
	sp = pcic_slots;
	for (slot = 0; slot < PCIC_MAX_SLOTS; slot++, sp++) {
		/*
		 *	Initialise the PCIC slot table.
		 */
		sp->getb = getb1;
		sp->putb = putb1;
		if (slot < 4) {
			sp->index = PCIC_INDEX_0;
			sp->data = PCIC_DATA_0;
			sp->offset = slot * PCIC_SLOT_SIZE;
		} else {
			sp->index = PCIC_INDEX_1;
			sp->data = PCIC_DATA_1;
			sp->offset = (slot - 4) * PCIC_SLOT_SIZE;
		}
		/* 
		 * XXX - Screwed up slot 1 on the VLSI chips.  According to
		 * the Linux PCMCIA code from David Hinds, working chipsets
		 * return 0x84 from their (correct) ID ports, while the broken
		 * ones would need to be probed at the new offset we set after
		 * we assume it's broken.
		 */
		if (slot == 1 && maybe_vlsi && sp->getb(sp, PCIC_ID_REV) != 0x84) {
			sp->index += 4;
			sp->data += 4;
			sp->offset = PCIC_SLOT_SIZE << 1;
		}
		/*
		 * see if there's a PCMCIA controller here
		 * Intel PCMCIA controllers use 0x82 and 0x83
		 * IBM clone chips use 0x88 and 0x89, apparently
		 */
		c = sp->getb(sp, PCIC_ID_REV);
		sp->revision = -1;
		switch(c) {
		/*
		 *	82365 or clones.
		 */
		case 0x82:
		case 0x83:
			sp->controller = PCIC_I82365;
			sp->revision = c & 1;
			/*
			 *	Now check for VADEM chips.
			 */
			outb(sp->index, 0x0E);
			outb(sp->index, 0x37);
			setb(sp, 0x3A, 0x40);
			c = sp->getb(sp, PCIC_ID_REV);
			if (c & 0x08) {
				sp->controller = ((sp->revision = c & 7) == 4) ?	
					PCIC_VG469 : PCIC_VG468 ;
				clrb(sp, 0x3A, 0x40);
			}

			/*
			 * Check for RICOH RF5C396 PCMCIA Controller
			 */
			c = sp->getb(sp, 0x3a);
			if (c == 0xb2) {
				sp->controller = PCIC_RF5C396;
			}

			break;
		/*
		 *	VLSI chips.
		 */
		case 0x84:
			sp->controller = PCIC_VLSI;
			maybe_vlsi = 1;
			break;
		case 0x88:
		case 0x89:
			sp->controller = PCIC_IBM;
			sp->revision = c & 1;
			break;
		case 0x8a:
			sp->controller = PCIC_IBM_KING;
			sp->revision = c & 1;
			break;
		default:
			continue;
		}
		/*
		 *	Check for Cirrus logic chips.
		 */
		sp->putb(sp, 0x1F, 0);
		c = sp->getb(sp, 0x1F);
		if ((c & 0xC0) == 0xC0) {
			c = sp->getb(sp, 0x1F);
			if ((c & 0xC0) == 0) {
				if (c & 0x20)
					sp->controller = PCIC_PD672X;
				else
					sp->controller = PCIC_PD6710;
				sp->revision = 8 - ((c & 0x1F) >> 2);
			}
		}
		switch(sp->controller) {
		case PCIC_I82365:
			cinfo.name = "Intel 82365";
			break;
		case PCIC_IBM:
			cinfo.name = "IBM PCIC";
			break;
		case PCIC_IBM_KING:
			cinfo.name = "IBM KING PCMCIA Controller";
			break;
		case PCIC_PD672X:
			cinfo.name = "Cirrus Logic PD672X";
			break;
		case PCIC_PD6710:
			cinfo.name = "Cirrus Logic PD6710";
			break;
		case PCIC_VG468:
			cinfo.name = "Vadem 468";
			break;
		case PCIC_VG469:
			cinfo.name = "Vadem 469";
			break;
		case PCIC_RF5C396:
			cinfo.name = "Ricoh RF5C396";
			break;
		case PCIC_VLSI:
			cinfo.name = "VLSI 82C146";
			break;
		default:
			cinfo.name = "Unknown!";
			break;
		}
#ifndef	PCIC_NOCLRREGS
		/*
		 *	clear out the registers.
		 */
		for (i = 2; i < 0x40; i++)
			sp->putb(sp, i, 0);
#endif	/* PCIC_NOCLRREGS */
		/*
		 *	OK it seems we have a PCIC or lookalike.
		 *	Allocate a slot and initialise the data structures.
		 */
		validslots++;
		sp->slot = slot;
		slotp = pccard_alloc_slot(&cinfo);
		if (slotp == 0)
			continue;
		slotp->cdata = sp;
		sp->slotp = slotp;
		/*
		 *	If we haven't allocated an interrupt for the controller,
		 *	then attempt to get one.
		 */
		if (pcic_irq == 0) {
			pcic_imask = soft_imask;
			pcic_irq = pccard_alloc_intr(free_irqs,
				pcicintr, 0, NULL, &pcic_imask);
			if (pcic_irq < 0)
				printf("pcic: failed to allocate IRQ\n");
			else
				printf("pcic: controller irq %d\n", pcic_irq);
		}
		/*
		 * Modem cards send the speaker audio (dialing noises)
		 * to the host's speaker.  Cirrus Logic PCIC chips must
		 * enable this.  There is also a Low Power Dynamic Mode bit
		 * that claims to reduce power consumption by 30%, so
		 * enable it and hope for the best.
		 */
		if (sp->controller == PCIC_PD672X) {
			setb(sp, PCIC_MISC1, PCIC_SPKR_EN);
			setb(sp, PCIC_MISC2, PCIC_LPDM_EN);
		}
		/*
		 *	Check for a card in this slot.
		 */
		setb(sp, PCIC_POWER, PCIC_PCPWRE| PCIC_DISRST);
		if ((sp->getb(sp, PCIC_STATUS) & PCIC_CD) != PCIC_CD) {
			slotp->laststate = slotp->state = empty;
		} else {
			slotp->laststate = slotp->state = filled;
			pccard_event(sp->slotp, card_inserted);
		}
		/*
		 *	Assign IRQ for slot changes
		 */
		if (pcic_irq > 0)
			sp->putb(sp, PCIC_STAT_INT, (pcic_irq << 4) | 0xF);
	}
#ifdef	PC98
	if (validslots == 0){
	    sp = pcic_slots; slot = 0;
	    if (inb(PCIC98_REG0) != 0xff){
		sp->controller = PCIC_PC98;
		sp->revision = 0;
		cinfo.name = "PC98 Original";
		cinfo.maxmem = 1;
		cinfo.maxio = 1;
/*		cinfo.irqs = PCIC_INT_MASK_ALLOWED;*/
		cinfo.irqs = 0x1468;
		validslots++;
		sp->slot = slot;

		slotp = pccard_alloc_slot(&cinfo);
		if (slotp == 0){
		    printf("pcic98: slotp == NULL\n");
		    goto pcic98_probe_end;
		}
		slotp->cdata = sp;
		sp->slotp = slotp;

		/* Check for a card in this slot */
		if (inb(PCIC98_REG1) & PCIC98_CARDEXIST){
		    /* PCMCIA card exist */
		    slotp->laststate = slotp->state = filled;
		    pccard_event(sp->slotp, card_inserted);
		} else {
		    slotp->laststate = slotp->state = empty;
		}
	    }
	pcic98_probe_end:
	}
#endif	/* PC98 */
	if (validslots)
		pcictimeout_ch = timeout(pcictimeout,0,hz/2);
	return(validslots);
}

/*
 *	ioctl calls - Controller specific ioctls
 */
static int
pcic_ioctl(struct slot *slotp, int cmd, caddr_t data)
{
	struct pcic_slot *sp = slotp->cdata;

	switch(cmd) {
	default:
		return(EINVAL);
	/*
	 * Get/set PCIC registers
	 */
	case PIOCGREG:
		((struct pcic_reg *)data)->value =
			sp->getb(sp, ((struct pcic_reg *)data)->reg);
		break;
	case PIOCSREG:
		sp->putb(sp, ((struct pcic_reg *)data)->reg,
			((struct pcic_reg *)data)->value);
		break;
	}
	return(0);
}

/*
 *	pcic_power - Enable the power of the slot according to
 *	the parameters in the power structure(s).
 */
static int
pcic_power(struct slot *slotp)
{
	unsigned char reg = PCIC_DISRST|PCIC_PCPWRE;
	struct pcic_slot *sp = slotp->cdata;

	switch(sp->controller) {
#ifdef	PC98
	case PCIC_PC98:
	    reg = inb(PCIC98_REG6) & (~PCIC98_VPP12V);
	    switch(slotp->pwr.vpp) {
	    default:
		return(EINVAL);
	    case 50:
		break;
	    case 120:
		reg |= PCIC98_VPP12V;
		break;
	    }
	    outb(PCIC98_REG6, reg);
	    DELAY (100*1000);

	    reg = inb(PCIC98_REG2) & (~PCIC98_VCC3P3V);
	    switch(slotp->pwr.vcc) {
	    default:
		return(EINVAL);
	    case 33:
		reg |= PCIC98_VCC3P3V;
		break;
	    case 50:
		break;
	    }
	    outb(PCIC98_REG2, reg);
	    DELAY (100*1000);
	    return (0);
#endif
	case PCIC_PD672X:
	case PCIC_PD6710:
	case PCIC_VG468:
	case PCIC_VG469:
	case PCIC_RF5C396:
	case PCIC_VLSI:
	case PCIC_IBM_KING:
		switch(slotp->pwr.vpp) {
		default:
			return(EINVAL);
		case 0:
			break;
		case 50:
		case 33:
			reg |= PCIC_VPP_5V;
			break;
		case 120:
			reg |= PCIC_VPP_12V;
			break;
		}
		switch(slotp->pwr.vcc) {
		default:
			return(EINVAL);
		case 0:
			break;
		case 33:
			if (sp->controller == PCIC_IBM_KING) {
				reg |= PCIC_VCC_5V_KING;
				break;
			}
			reg |= PCIC_VCC_3V;
			if ((sp->controller == PCIC_VG468)||
				(sp->controller == PCIC_VG469))
				setb(sp, 0x2f, 0x03) ;
			else
				setb(sp, 0x16, 0x02);
			break;
		case 50:
                        if (sp->controller == PCIC_IBM_KING) {
                                reg |= PCIC_VCC_5V_KING;
                                break;
                        }
			reg |= PCIC_VCC_5V;
			if ((sp->controller == PCIC_VG468)||
				(sp->controller == PCIC_VG469))
				clrb(sp, 0x2f, 0x03) ;
			else
				clrb(sp, 0x16, 0x02);
			break;
		}
		break;
	}
	sp->putb(sp, PCIC_POWER, reg);
	DELAY(300*1000);
	if (slotp->pwr.vcc) {
		reg |= PCIC_OUTENA;
		sp->putb(sp, PCIC_POWER, reg);
		DELAY(100*1000);
	}
	/* Some chips are smarter than us it seems, so if we weren't
	 * allowed to use 5V, try 3.3 instead
	 */
	if (!(sp->getb(sp, PCIC_STATUS) &  0x40) && slotp->pwr.vcc == 50) {
		slotp->pwr.vcc = 33;
		slotp->pwr.vpp = 0;
		return (pcic_power(slotp));
	}
	return(0);
}

/*
 * tell the PCIC which irq we want to use.  only the following are legal:
 * 3, 4, 5, 7, 9, 10, 11, 12, 14, 15
 */
static void
pcic_mapirq (struct slot *slotp, int irq)
{
	struct pcic_slot *sp = slotp->cdata;
#ifdef	PC98
	if (sp->controller == PCIC_PC98){
	    unsigned char x;
	    switch (irq){
	    case 3:
		x = PCIC98_INT0; break;
	    case 5:
		x = PCIC98_INT1; break;
	    case 6:
		x = PCIC98_INT2; break;
	    case 10:
		x = PCIC98_INT4; break;
	    case 12:
		x = PCIC98_INT5; break;
	    case 0:		/* disable */
		x = PCIC98_INTDISABLE;
		break;
	    default:
		printf("pcic98: illegal irq %d\n", irq);
		return;
	    }
#ifdef	PCIC_DEBUG
	    printf("pcic98: irq=%d mapped.\n", irq);
#endif
	    outb(PCIC98_REG3, x);

	    return;
	}	
#endif
	if (irq == 0)
		clrb(sp, PCIC_INT_GEN, 0xF);
	else
		sp->putb(sp, PCIC_INT_GEN, 
		    (sp->getb(sp, PCIC_INT_GEN) & 0xF0) | irq);
}

/*
 *	pcic_reset - Reset the card and enable initial power.
 */
static void
pcic_reset(void *chan)
{
	struct slot *slotp = chan;
	struct pcic_slot *sp = slotp->cdata;

#ifdef	PC98
	if (sp->controller == PCIC_PC98){
	    outb(PCIC98_REG0, 0);
	    outb(PCIC98_REG2, inb(PCIC98_REG2) & (~PCIC98_IOMEMORY));
	    outb(PCIC98_REG3, PCIC98_INTDISABLE);
	    outb(PCIC98_REG2, inb(PCIC98_REG2) & (~PCIC98_VCC3P3V));
	    outb(PCIC98_REG6, inb(PCIC98_REG6) & (~PCIC98_VPP12V));
	    outb(PCIC98_REG1, 0);

	    selwakeup(&slotp->selp);
	    return;
	}
#endif
	switch (slotp->insert_seq) {
	    case 0: /* Something funny happended on the way to the pub... */
		return;
	    case 1: /* Assert reset */
		clrb(sp, PCIC_INT_GEN, PCIC_CARDRESET);
		slotp->insert_seq = 2;
		timeout(pcic_reset, (void*) slotp, hz/4);
		return;
	    case 2: /* Deassert it again */
		setb(sp, PCIC_INT_GEN, PCIC_CARDRESET|PCIC_IOCARD);
		slotp->insert_seq = 3;
		timeout(pcic_reset, (void*) slotp, hz/4);
		return;
	    case 3: /* Wait if card needs more time */
		if (!sp->getb(sp, PCIC_STATUS) & PCIC_READY) {
			timeout(pcic_reset, (void*) slotp, hz/10);
			return;
		}
	}
	slotp->insert_seq = 0;
	if (sp->controller == PCIC_PD672X || sp->controller == PCIC_PD6710) {
		sp->putb(sp, PCIC_TIME_SETUP0, 0x1);
		sp->putb(sp, PCIC_TIME_CMD0, 0x6);
		sp->putb(sp, PCIC_TIME_RECOV0, 0x0);
		sp->putb(sp, PCIC_TIME_SETUP1, 1);
		sp->putb(sp, PCIC_TIME_CMD1, 0xf);
		sp->putb(sp, PCIC_TIME_RECOV1, 0);
	}
	selwakeup(&slotp->selp);
}

/*
 *	pcic_disable - Disable the slot.
 */
static void
pcic_disable(struct slot *slotp)
{
	struct pcic_slot *sp = slotp->cdata;

#ifdef	PC98
	if (sp->controller == PCIC_PC98){
	    return;
	}
#endif
	sp->putb(sp, PCIC_INT_GEN, 0);
	sp->putb(sp, PCIC_POWER, 0);
}

/*
 *	PCIC timer, it seems that we lose interrupts sometimes
 *	so poll just in case...
 */
static void
pcictimeout(void *chan)
{
	pcictimeout_ch = timeout(pcictimeout,0,hz/2);
	pcicintr(0);
}

/*
 *	PCIC Interrupt handler.
 *	Check each slot in turn, and read the card status change
 *	register. If this is non-zero, then a change has occurred
 *	on this card, so send an event to the main code.
 */
static void
pcicintr(int unit)
{
	int	slot, s;
	unsigned char chg;
	struct pcic_slot *sp = pcic_slots;

#ifdef	PC98
	if (sp->controller == PCIC_PC98){
	    slot = 0;
	    s = splhigh();
	    /* Check for a card in this slot */
	    if (inb(PCIC98_REG1) & PCIC98_CARDEXIST){
		if (sp->slotp->laststate != filled){
		    pccard_event(sp->slotp, card_inserted);
		}
	    } else {
		if (sp->slotp->laststate != empty){
		    pccard_event(sp->slotp, card_removed);
		}
	    }
	    splx(s);
	    return;
	}
#endif	/* PC98 */
	s = splhigh();
	for (slot = 0; slot < PCIC_MAX_SLOTS; slot++, sp++)
		if (sp->slotp && (chg = sp->getb(sp, PCIC_STAT_CHG)) != 0)
			if (chg & PCIC_CDTCH) {
				if ((sp->getb(sp, PCIC_STATUS) & PCIC_CD) ==
						PCIC_CD) {
					pccard_event(sp->slotp,
						card_inserted);
				} else {
					pccard_event(sp->slotp,
						card_removed);
				}
			}
	splx(s);
}

/*
 *	pcic_resume - Suspend/resume support for PCIC
 */
static void
pcic_resume(struct slot *slotp)
{
	struct pcic_slot *sp = slotp->cdata;
	if (pcic_irq > 0)
		sp->putb(sp, PCIC_STAT_INT, (pcic_irq << 4) | 0xF);
	if (sp->controller == PCIC_PD672X) {
		setb(sp, PCIC_MISC1, PCIC_SPKR_EN);
		setb(sp, PCIC_MISC2, PCIC_LPDM_EN);
	}
}
