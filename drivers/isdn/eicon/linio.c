/*
 * Copyright (C) Eicon Technology Corporation, 2000.
 *
 * Eicon File Revision :    1.16  
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#define N_DATA

#include <asm/io.h>
#include <asm/system.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/delay.h>
#undef N_DATA

#include "uxio.h"

static
int log_on=0;

int		Divasdevflag = 0;

//spinlock_t diva_lock = SPIN_LOCK_UNLOCKED;

static
ux_diva_card_t card_pool[MAX_CARDS];

void UxPause(long int ms)
{
	int timeout = jiffies + ((ms * HZ) / 1000);

	while (time_before(jiffies, timeout));
}

int UxCardHandleGet(ux_diva_card_t **card, dia_card_t *cfg)
{
	int 		i;
	ux_diva_card_t	*c;

	if (cfg->bus_type != DIA_BUS_TYPE_PCI)
	{
		DPRINTF(("divas hw: type not PCI (%d)", cfg->bus_type));
		return -1;
	}

	for (i = 0; (i < DIM(card_pool)) && (card_pool[i].in_use); i++)
	{
		;
	}

	if (i == DIM(card_pool))
	{
		DPRINTF(("divas hw: card_pool exhausted"));
		return -1;
	}

	c = *card = &card_pool[i];

	switch (cfg->bus_type)
	{
	case DIA_BUS_TYPE_PCI:
		c->bus_num = cfg->bus_num;
		c->func_num = cfg->func_num;
		c->io_base = cfg->io_base;
		c->reset_base = cfg->reset_base;
		c->card_type    = cfg->card_type;
		c->mapped = NULL;
		c->slot 	= cfg->slot;
		c->irq 		= (int) cfg->irq;
		c->pDRAM    	= cfg->memory[DIVAS_RAM_MEMORY];
		c->pDEVICES 	= cfg->memory[DIVAS_REG_MEMORY];
		c->pCONFIG  	= cfg->memory[DIVAS_CFG_MEMORY];
		c->pSHARED  	= cfg->memory[DIVAS_SHARED_MEMORY];
		c->pCONTROL  	= cfg->memory[DIVAS_CTL_MEMORY];

	/*		c->bus_type 	= DIA_BUS_TYPE_PCI;
		c->bus_num 	= cfg->bus_num & 0x3f;
		c->slot 	= cfg->slot;
		c->irq 		= (int) cfg->irq;
		c->int_priority = (int) cfg->int_priority;
		c->card_type    = cfg->card_type;
		c->io_base      = cfg->io_base;
		c->reset_base   = cfg->reset_base;
		c->pDRAM    	= cfg->memory[DIVAS_RAM_MEMORY];
		c->pDEVICES 	= cfg->memory[DIVAS_REG_MEMORY];
		c->pCONFIG  	= cfg->memory[DIVAS_CFG_MEMORY];
		c->pSHARED  	= cfg->memory[DIVAS_SHARED_MEMORY];
		DPRINTF(("divas hw: pDRAM is 0x%x", c->pDRAM));
		DPRINTF(("divas hw: pSHARED is 0x%x", c->pSHARED));
		DPRINTF(("divas hw: pCONFIG is 0x%x", c->pCONFIG));
		c->cm_key		= cm_getbrdkey("Divas", cfg->card_id);*/
		break;
	default:
		break;
	}

	c->in_use = TRUE;

	return 0;
}

void UxCardHandleFree(ux_diva_card_t *card)
{
	card->in_use = FALSE;
}


#define PLX_IOBASE 0
#define DIVAS_IOBASE 1
void *UxCardMemAttach(ux_diva_card_t *card, int id)
{
	if (card->card_type == DIA_CARD_TYPE_DIVA_SERVER)
	{
		switch (id)
		{
		case DIVAS_SHARED_MEMORY:
			card->mapped = card->pSHARED;
			return card->pSHARED;
			break;
		case DIVAS_RAM_MEMORY:
			card->mapped = card->pDRAM;
			return card->pDRAM;
			break;
		case DIVAS_REG_MEMORY:
			card->mapped = card->pDEVICES;
			return card->pDEVICES;
			break;
		case DIVAS_CFG_MEMORY:
			card->mapped = card->pCONFIG;
			return card->pCONFIG;
			break;
		default:
			ASSERT(FALSE);
			card->mapped = NULL;
			return (void *) 0;
		}
	}
	else if (card->card_type == DIA_CARD_TYPE_DIVA_SERVER_B)
	{
		switch (id)
		{
		case PLX_IOBASE:
			return (void *) card->reset_base;
			break;
		case DIVAS_IOBASE:
			return (void *) card->io_base;
			break;
		default:
			ASSERT(FALSE);
			return 0;
		}
	}
	
	else if (card->card_type == DIA_CARD_TYPE_DIVA_SERVER_Q)
	{
		switch (id)
		{
		case DIVAS_SHARED_MEMORY:
			card->mapped = card->pSHARED;
			return card->pSHARED;
			break;
		case DIVAS_RAM_MEMORY:
			card->mapped = card->pDRAM;
			return card->pDRAM;
			break;
		case DIVAS_REG_MEMORY:
			card->mapped = (void *) card->io_base;
			return (void *) card->io_base;
			break;
		case DIVAS_CTL_MEMORY:
			card->mapped = card->pCONTROL;
			return card->pCONTROL;
			break;
		default:
			// ASSERT(FALSE);
			DPRINTF(("divas: Trying to attach to mem %d", id));
			card->mapped = NULL;
			return (void *) 0;
		}
	} else
		DPRINTF(("divas: Tried to attach to unknown card"));

	/* Unknown card type */
	return NULL;
}

void UxCardMemDetach(ux_diva_card_t *card, void *address)
{
	return; // Just a place holder. No un-mapping done.
}

void UxCardLog(int turn_on)
{
	log_on = turn_on;
}

/*
 * Control Register I/O Routines to be performed on Attached I/O ports
 */

void UxCardPortIoOut(ux_diva_card_t *card, void *AttachedBase, int offset, byte the_byte)
{
	word base = (word) (dword) AttachedBase;

	base += offset;

	outb(the_byte, base);
}

void UxCardPortIoOutW(ux_diva_card_t *card, void *AttachedBase, int offset, word the_word)
{
	word base = (word) (dword) AttachedBase;

	base += offset;

	outw(the_word, base);
}

void UxCardPortIoOutD(ux_diva_card_t *card, void *AttachedBase, int offset, dword the_dword)
{
	word base = (word) (dword) AttachedBase;

	base += offset;

	outl(the_dword, base);
}

byte UxCardPortIoIn(ux_diva_card_t *card, void *AttachedBase, int offset)
{
	word base = (word) (dword) AttachedBase;

	base += offset;

	return inb(base);
}

word UxCardPortIoInW(ux_diva_card_t *card, void *AttachedBase, int offset)
{
	word base = (word) (dword) AttachedBase;

	base += offset;

	return inw(base);
}

/*
 * Memory mapped card I/O functions
 */

byte UxCardMemIn(ux_diva_card_t *card, void *address)
{
	byte	b;
	volatile byte* t = (byte*)address;

	b = *t;

	if (log_on)
	{
		byte *a = address;
		a -= (int) card->mapped;
		DPRINTF(("divas hw: read 0x%02x from 0x%x (memory mapped)", b & 0xff, a));
    	}

    return(b); 
}

word UxCardMemInW(ux_diva_card_t *card, void *address)
{
	word	w;
	volatile word* t = (word*)address;

    w = *t;

	if (log_on)
    {
		byte *a = address;
		a -= (int) card->mapped;
		DPRINTF(("divas hw: read 0x%04x from 0x%x (memory mapped)", w & 0xffff, a));
    }

    return (w);
}

dword UxCardMemInD(ux_diva_card_t *card, void *address)
{
	dword	dw;
	volatile dword* t = (dword*)address;

    dw = *t;

	if (log_on)
    {
		byte *a = address;
		a -= (int) card->mapped;
		DPRINTF(("divas hw: read 0x%08x from 0x%x (memory mapped)", dw, a));
    }

    return (dw);
}

void UxCardMemInBuffer(ux_diva_card_t *card, void *address, void *buffer, int length)
{
	volatile byte *pSource = address;
	byte *pDest = buffer;

	while (length--)
	{
		*pDest++ = *pSource++;
	}

	if (log_on)
    {
		byte *a = address;
		a -= (int) card->mapped;
		pDest = buffer;
		DPRINTF(("divas hw: read %02x %02x %02x %02x %02x %02x %02x %02x from 0x%x (memory mapped)", 
		pDest[0] & 0xff, pDest[1] & 0xff, pDest[2] & 0xff, pDest[3] & 0xff,
		pDest[4] & 0xff, pDest[5] & 0xff, pDest[6] & 0xff, pDest[7] & 0xff,
		a));
    }

    return;
}

void UxCardMemOut(ux_diva_card_t *card, void *address, byte data)
{
	volatile byte* t = (byte*)address;

	if (log_on)
	{
		byte *a = address;
		a -= (int) card->mapped;
		DPRINTF(("divas hw: wrote 0x%02x to 0x%x (memory mapped)", data & 0xff, a));
	}

	*t = data;

    	return;
}

void UxCardMemOutW(ux_diva_card_t *card, void *address, word data)
{
	volatile word* t = (word*)address;

	if (log_on)
	{
		byte *a = address;
		a -= (int) card->mapped;
		DPRINTF(("divas hw: wrote 0x%04x to 0x%x (memory mapped)", data & 0xffff, a));
	}

	*t = data;
    return;
}

void UxCardMemOutD(ux_diva_card_t *card, void *address, dword data)
{
	volatile dword* t = (dword*)address;

	if (log_on)
	{
		byte *a = address;
		a -= (int) card->mapped;
		DPRINTF(("divas hw: wrote 0x%08x to 0x%x (memory mapped)", data, a));
	}

	*t = data;
    return;
}

void UxCardMemOutBuffer(ux_diva_card_t *card, void *address, void *buffer, int length)
{
	byte 	*pSource = buffer;
	byte	*pDest = address;

	while (length--)
	{
		*pDest++ = *pSource++;
	}

	if (log_on)
    {
		byte *a = address;
		a -= (int) card->mapped;
		pDest = buffer;
		DPRINTF(("divas hw: wrote %02x %02x %02x %02x %02x %02x %02x %02x to 0x%x (memory mapped)", 
		pDest[0] & 0xff, pDest[1] & 0xff, pDest[2] & 0xff, pDest[3] & 0xff,
		pDest[4] & 0xff, pDest[5] & 0xff, pDest[6] & 0xff, pDest[7] & 0xff,
		a));
    }

    return;
}

/*
 * Memory mapped card I/O functions
 */

byte UxCardIoIn(ux_diva_card_t *card, void *AttachedDivasIOBase, void *address)

{
	byte the_byte;

    outb(0xFF, card->io_base + 0xC);
	outw((word) (dword) address, card->io_base + 4);

	the_byte = inb(card->io_base);

	if (log_on)
    {
		DPRINTF(("divas hw: read 0x%02x from 0x%x (I/O mapped)", 
					the_byte & 0xff, address));
    }
    
	return the_byte;
}

word UxCardIoInW(ux_diva_card_t *card, void *AttachedDivasIOBase, void *address)

{
	word the_word;

	outb(0xFF, card->io_base + 0xC);
	outw((word) (dword) address, card->io_base + 4);
	the_word = inw(card->io_base);

	if (log_on)
    {
		DPRINTF(("divas hw: read 0x%04x from 0x%x (I/O mapped)", 
					the_word & 0xffff, address));
    }

	return the_word;
}

dword UxCardIoInD(ux_diva_card_t *card, void *AttachedDivasIOBase, void *address)

{
	dword the_dword;

	outb(0xFF, card->io_base + 0xC);
	outw((word) (dword) address, card->io_base + 4);
	the_dword = inl(card->io_base);

	if (log_on)
    {
		DPRINTF(("divas hw: read 0x%08x from 0x%x (I/O mapped)", 
					the_dword, address));
    }

    return the_dword;
}

void UxCardIoInBuffer(ux_diva_card_t *card, void *AttachedDivasIOBase, void *address, void *buffer, int length)

{
	byte *pSource = address;
	byte *pDest = buffer;

	if ((word) (dword) address & 0x1)
	{
		outb(0xFF, card->io_base + 0xC);
		outw((word) (dword) pSource, card->io_base + 4);
		*pDest = (byte) inb(card->io_base);
		pDest++;
		pSource++;
		length--;
		if (!length)
        {
            return;
        }
    }

	outb(0xFF, card->io_base + 0xC);
	outw((word) (dword) pSource, card->io_base + 4);
	insw(card->io_base, (word *)pDest,length%2 ? (length+1)>>1 : length>>1);

	if (log_on)
    {
		pDest = buffer;
		DPRINTF(("divas hw: read %02x %02x %02x %02x %02x %02x %02x %02x from 0x%x (I/O mapped)", 
		pDest[0] & 0xff, pDest[1] & 0xff, pDest[2] & 0xff, pDest[3] & 0xff,
		pDest[4] & 0xff, pDest[5] & 0xff, pDest[6] & 0xff, pDest[7] & 0xff,
		address));
    }

    return;
}

/* Output */

void UxCardIoOut(ux_diva_card_t *card, void *AttachedDivasIOBase, void *address, byte data)
{
	if (log_on)
    {
		DPRINTF(("divas hw: wrote 0x%02x to 0x%x (I/O mapped)", 
					data & 0xff, address));
    }

	outb(0xFF, card->io_base + 0xC);
	outw((word) (dword) address, card->io_base + 4);
	outb((byte) data & 0xFF, card->io_base);

    return;
}

void UxCardIoOutW(ux_diva_card_t *card, void *AttachedDivasIOBase, void *address, word data)
{
	if (log_on)
    {
		DPRINTF(("divas hw: wrote 0x%04x to 0x%x (I/O mapped)", 
					data & 0xffff, address));
    }

	outb(0xFF, card->io_base + 0xC);
	outw((word) (dword) address, card->io_base + 4);
	outw((word) data & 0xFFFF, card->io_base);

    return;
}

void UxCardIoOutD(ux_diva_card_t *card, void *AttachedDivasIOBase, void *address, dword data)
{
	if (log_on)
    {
		DPRINTF(("divas hw: wrote 0x%08x to 0x%x (I/O mapped)", data, address));
    }

	outb(0xFF, card->io_base + 0xC);
	outw((word) (dword) address, card->io_base + 4);
	outl((dword) data & 0xFFFFFFFF, card->io_base);

    return;
}

void UxCardIoOutBuffer(ux_diva_card_t *card, void *AttachedDivasIOBase, void *address, void *buffer, int length)

{
	byte 	*pSource = buffer;
	byte	*pDest = address;

	if ((word) (dword) address & 1)
	{
		outb(0xFF, card->io_base + 0xC);
		outw((word) (dword) pDest, card->io_base + 4);
		outb(*pSource, card->io_base);
		pSource++;
		pDest++;
		length--;
		if (!length)
        {
			return;
        }
	}

    outb(0xFF, card->io_base + 0xC);
	outw((word) (dword) pDest, card->io_base + 4);
	outsw(card->io_base, (word *)pSource, length%2 ? (length+1)>>1 : length>>1);

	if (log_on)
    {
		pDest = buffer;
		DPRINTF(("divas hw: wrote %02x %02x %02x %02x %02x %02x %02x %02x to 0x%x (I/O mapped)", 
		pDest[0] & 0xff, pDest[1] & 0xff, pDest[2] & 0xff, pDest[3] & 0xff,
		pDest[4] & 0xff, pDest[5] & 0xff, pDest[6] & 0xff, pDest[7] & 0xff,
		address));
    }

    return;
}

void 	Divasintr(int arg, void *unused, struct pt_regs *unused_regs)
{
	int i;
	card_t *card = NULL;
	ux_diva_card_t *ux_ref = NULL;

	for (i = 0; i < DivasCardNext; i++)
	{

		if (arg == DivasCards[i].cfg.irq)
		{
			card = &DivasCards[i];
			ux_ref = card->hw;
	
			if ((ux_ref) && (card->is_live))
			{
				(*ux_ref->user_isr)(ux_ref->user_isr_arg);	
			}
			else 
			{
				DPRINTF(("divas: ISR couldn't locate card"));
			}
		}
	}

	return;
}


int UxIsrInstall(ux_diva_card_t *card, isr_fn_t *isr_fn, void *isr_arg)
{
	int result;

        card->user_isr = isr_fn;
        card->user_isr_arg = isr_arg;

	result = request_irq(card->irq, Divasintr, SA_INTERRUPT | SA_SHIRQ, "Divas", (void *) isr_arg);

	return result;
}

void UxIsrRemove(ux_diva_card_t *card, void *dev_id)
{
	free_irq(card->irq, card->user_isr_arg);
}

void UxPciConfigWrite(ux_diva_card_t *card, int size, int offset, void *value)
{
	switch (size)
	{
	case sizeof(byte):
		pcibios_write_config_byte(card->bus_num, card->func_num, offset, * (byte *) value);
		break;
	case sizeof(word):
		pcibios_write_config_word(card->bus_num, card->func_num, offset, * (word *) value);
		break;
	case sizeof(dword):
		pcibios_write_config_dword(card->bus_num, card->func_num, offset, * (dword *) value);
		break;
	default:
		printk(KERN_WARNING "Divas: Invalid size in UxPciConfigWrite\n");
	}
}

void UxPciConfigRead(ux_diva_card_t *card, int size, int offset, void *value)
{
	switch (size)
	{
	case sizeof(byte):
		pcibios_read_config_byte(card->bus_num, card->func_num, offset, (byte *) value);
		break;
	case sizeof(word):
		pcibios_read_config_word(card->bus_num, card->func_num, offset, (word *) value);
		break;
	case sizeof(dword):
		pcibios_read_config_dword(card->bus_num, card->func_num, offset, (unsigned int *) value);
		break;
	default:
		printk(KERN_WARNING "Divas: Invalid size in UxPciConfigRead\n");
	}
}

void *UxAlloc(unsigned int size)
{
	void *m;

	m = kmalloc(size, GFP_ATOMIC);

	return m;
}

void UxFree(void *ptr)
{
	kfree(ptr);
}

long UxCardLock(ux_diva_card_t *card)
{
	unsigned long flags;

 	//spin_lock_irqsave(&diva_lock, flags);
	
	save_flags(flags);
	cli();
	return flags;
	
}

void UxCardUnlock(ux_diva_card_t *card, long ipl)
{
	//spin_unlock_irqrestore(&diva_lock, ipl);

	restore_flags(ipl);

}

dword UxTimeGet(void)
{
	return jiffies;
}

long UxInterlockedIncrement(ux_diva_card_t *card, long *dst)
{
	register volatile long *p;
	register long ret;
	int ipl;

	p =dst;
	
	ipl = UxCardLock(card);

	*p += 1;
	ret = *p;

	UxCardUnlock(card,ipl);

	return(ret);

}

long UxInterlockedDecrement(ux_diva_card_t *card, long *dst)
{
	register volatile long *p;
	register long ret;
	int ipl;

	p =dst;
	
	ipl = UxCardLock(card);

	*p -= 1;
	ret = *p;

	UxCardUnlock(card,ipl);

	return(ret);

}
