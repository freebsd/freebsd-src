/* $Id: eicon_isa.c,v 1.1.4.1 2001/11/20 14:19:35 kai Exp $
 *
 * ISDN low-level module for Eicon active ISDN-Cards.
 * Hardware-specific code for old ISA cards.
 *
 * Copyright 1998      by Fritz Elfert (fritz@isdn4linux.de)
 * Copyright 1998-2000 by Armin Schindler (mac@melware.de)
 * Copyright 1999,2000 Cytronics & Melware (info@melware.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/config.h>
#include "eicon.h"
#include "eicon_isa.h"

#define check_shmem   check_region
#define release_shmem release_region
#define request_shmem request_region

char *eicon_isa_revision = "$Revision: 1.1.4.1 $";

#undef EICON_MCA_DEBUG

#ifdef CONFIG_ISDN_DRV_EICON_ISA

/* Mask for detecting invalid IRQ parameter */
static int eicon_isa_valid_irq[] = {
	0x1c1c, /* 2, 3, 4, 10, 11, 12 (S)*/
	0x1c1c, /* 2, 3, 4, 10, 11, 12 (SX) */
	0x1cbc, /* 2, 3, 4, 5, 7, 10, 11, 12 (SCOM) */
	0x1cbc, /* 2, 3, 4, 5, 6, 10, 11, 12 (Quadro) */
	0x1cbc  /* 2, 3, 4, 5, 7, 10, 11, 12 (S2M) */
};

static void
eicon_isa_release_shmem(eicon_isa_card *card) {
	if (card->mvalid) {
		iounmap(card->shmem);
		release_mem_region(card->physmem, card->ramsize);
	}
	card->mvalid = 0;
}

static void
eicon_isa_release_irq(eicon_isa_card *card) {
	if (!card->master)
		return;
	if (card->ivalid)
		free_irq(card->irq, card);
	card->ivalid = 0;
}

void
eicon_isa_release(eicon_isa_card *card) {
	eicon_isa_release_irq(card);
	eicon_isa_release_shmem(card);
}

void
eicon_isa_printpar(eicon_isa_card *card) {
	switch (card->type) {
		case EICON_CTYPE_S:
		case EICON_CTYPE_SX:
		case EICON_CTYPE_SCOM:
		case EICON_CTYPE_QUADRO:
		case EICON_CTYPE_S2M:
			printk(KERN_INFO "Eicon %s at 0x%lx, irq %d.\n",
			       eicon_ctype_name[card->type],
			       card->physmem,
			       card->irq);
	}
}

int
eicon_isa_find_card(int Mem, int Irq, char * Id)
{
	int primary = 1;
	unsigned long amem;

	if (!strlen(Id))
		return -1;

	if (Mem == -1)
		return -1;

	/* Check for valid membase address */
	if ((Mem < 0x0c0000) ||
	    (Mem > 0x0fc000) ||
	    (Mem & 0xfff)) { 
		printk(KERN_WARNING "eicon_isa: illegal membase 0x%x for %s\n",
			 Mem, Id);
		return -1;
	}
	if (check_mem_region(Mem, RAMSIZE)) {
		printk(KERN_WARNING "eicon_isa_boot: memory at 0x%x already in use.\n", Mem);
		return -1;
	}

	amem = (unsigned long) ioremap(Mem, RAMSIZE);
        writew(0x55aa, amem + 0x402);
        if (readw(amem + 0x402) != 0x55aa) primary = 0;
	writew(0, amem + 0x402);
	if (readw(amem + 0x402) != 0) primary = 0;

	printk(KERN_INFO "Eicon: Driver-ID: %s\n", Id);
	if (primary) {
		printk(KERN_INFO "Eicon: assuming pri card at 0x%x\n", Mem);
		writeb(0, amem + 0x3ffe);
		iounmap((unsigned char *)amem);
		return EICON_CTYPE_ISAPRI;
	} else {
		printk(KERN_INFO "Eicon: assuming bri card at 0x%x\n", Mem);
		writeb(0, amem + 0x400);
		iounmap((unsigned char *)amem);
		return EICON_CTYPE_ISABRI;
	}
	return -1;
}

int
eicon_isa_bootload(eicon_isa_card *card, eicon_isa_codebuf *cb) {
	int	tmp;
	int               timeout;
	eicon_isa_codebuf cbuf;
	unsigned char     *code;
	eicon_isa_boot    *boot;

	if (copy_from_user(&cbuf, cb, sizeof(eicon_isa_codebuf)))
		return -EFAULT;

	/* Allocate code-buffer and copy code from userspace */
	if (cbuf.bootstrap_len > 1024) {
		printk(KERN_WARNING "eicon_isa_boot: Invalid startup-code size %ld\n",
		       cbuf.bootstrap_len);
		return -EINVAL;
	}
	if (!(code = kmalloc(cbuf.bootstrap_len, GFP_KERNEL))) {
		printk(KERN_WARNING "eicon_isa_boot: Couldn't allocate code buffer\n");
		return -ENOMEM;
	}
	if (copy_from_user(code, &cb->code, cbuf.bootstrap_len)) {
		kfree(code);
		return -EFAULT;
	}

	if (card->type == EICON_CTYPE_ISAPRI)
		card->ramsize  = RAMSIZE_P;
	else
		card->ramsize  = RAMSIZE;

	if (check_mem_region(card->physmem, card->ramsize)) {
		printk(KERN_WARNING "eicon_isa_boot: memory at 0x%lx already in use.\n",
			card->physmem);
		kfree(code);
		return -EBUSY;
	}
	request_mem_region(card->physmem, card->ramsize, "Eicon ISA ISDN");
	card->shmem = (eicon_isa_shmem *) ioremap(card->physmem, card->ramsize);
#ifdef EICON_MCA_DEBUG
	printk(KERN_INFO "eicon_isa_boot: card->ramsize = %d.\n", card->ramsize);
#endif
	card->mvalid = 1;

	switch(card->type) {
		case EICON_CTYPE_S:
		case EICON_CTYPE_SX:
		case EICON_CTYPE_SCOM:
		case EICON_CTYPE_QUADRO:
		case EICON_CTYPE_ISABRI:
			card->intack   = (__u8 *)card->shmem + INTACK;
			card->startcpu = (__u8 *)card->shmem + STARTCPU;
			card->stopcpu  = (__u8 *)card->shmem + STOPCPU;
			break;
		case EICON_CTYPE_S2M:
		case EICON_CTYPE_ISAPRI:
			card->intack   = (__u8 *)card->shmem + INTACK_P;
			card->startcpu = (__u8 *)card->shmem + STARTCPU_P;
			card->stopcpu  = (__u8 *)card->shmem + STOPCPU_P;
			break;
		default:
			printk(KERN_WARNING "eicon_isa_boot: Invalid card type %d\n", card->type);
			eicon_isa_release_shmem(card);
			kfree(code);
			return -EINVAL;
	}

	/* clear any pending irq's */
	readb(card->intack);
#ifdef CONFIG_MCA
	if (MCA_bus) {
		if (card->type == EICON_CTYPE_SCOM) {
			outb_p(0,card->io+1);
		}
		else {
			printk(KERN_WARNING "eicon_isa_boot: Card type not supported yet.\n");
			eicon_isa_release_shmem(card);
			return -EINVAL;
		};

#ifdef EICON_MCA_DEBUG
	printk(KERN_INFO "eicon_isa_boot: card->io      = %x.\n", card->io);
	printk(KERN_INFO "eicon_isa_boot: card->irq     = %d.\n", (int)card->irq);
#endif
	}
#else
	/* set reset-line active */
	writeb(0, card->stopcpu); 
#endif  /* CONFIG_MCA */
	/* clear irq-requests */
	writeb(0, card->intack);
	readb(card->intack);

	/* Copy code into card */
	memcpy_toio(&card->shmem->c, code, cbuf.bootstrap_len);

	/* Check for properly loaded code */
	if (!check_signature((unsigned long)&card->shmem->c, code, 1020)) {
		printk(KERN_WARNING "eicon_isa_boot: Could not load startup-code\n");
		eicon_isa_release_shmem(card);
		kfree(code);
		return -EIO;
	}
	/* if 16k-ramsize, duplicate the reset-jump-code */
	if (card->ramsize == RAMSIZE_P)
		memcpy_toio((__u8 *)card->shmem + 0x3ff0, &code[0x3f0], 12);

	kfree(code);
	boot = &card->shmem->boot;

	/* Delay 0.2 sec. */
	SLEEP(HZ / 5);

	/* Start CPU */
	writeb(cbuf.boot_opt, &boot->ctrl);
#ifdef CONFIG_MCA
	if (MCA_bus) {
		outb_p(0, card->io);
	}
#else 
	writeb(0, card->startcpu); 
#endif /* CONFIG_MCA */

	/* Delay 0.2 sec. */
	SLEEP(HZ / 5);

	timeout = jiffies + (HZ * 22);
	while (time_before(jiffies, timeout)) {
		if (readb(&boot->ctrl) == 0)
			break;
		SLEEP(10);
	}
	if (readb(&boot->ctrl) != 0) {
		printk(KERN_WARNING "eicon_isa_boot: CPU test failed.\n");
#ifdef EICON_MCA_DEBUG
		printk(KERN_INFO "eicon_isa_boot: &boot->ctrl = %d.\n",
			readb(&boot->ctrl));
#endif
		eicon_isa_release_shmem(card);
		return -EIO;
	}

	/* Check for memory-test errors */
	if (readw(&boot->ebit)) {
		printk(KERN_WARNING "eicon_isa_boot: memory test failed (bit 0x%04x at 0x%08x)\n",
		       readw(&boot->ebit), readl(&boot->eloc));
		eicon_isa_release_shmem(card);
		return -EIO;
	}

        /* Check card type and memory size */
        tmp = readb(&boot->card);
	if ((tmp < 0) || (tmp > 4)) {
		printk(KERN_WARNING "eicon_isa_boot: Type detect failed\n");
		eicon_isa_release_shmem(card);
		return -EIO;
	}
	card->type = tmp;
	((eicon_card *)card->card)->type = tmp;

        tmp = readb(&boot->msize);
        if (tmp != 8 && tmp != 16 && tmp != 24 &&
            tmp != 32 && tmp != 48 && tmp != 60) {
                printk(KERN_WARNING "eicon_isa_boot: invalid memsize\n");
		eicon_isa_release_shmem(card);
                return -EIO;
        }
	printk(KERN_INFO "%s: startup-code loaded\n", eicon_ctype_name[card->type]); 
	if ((card->type == EICON_CTYPE_QUADRO) && (card->master)) {
		tmp = eicon_addcard(card->type, card->physmem, card->irq, 
				((eicon_card *)card->card)->regname, 0);
		printk(KERN_INFO "Eicon: %d adapters added\n", tmp);
	}
	return 0;
}

int
eicon_isa_load(eicon_isa_card *card, eicon_isa_codebuf *cb) {
	eicon_isa_boot    *boot;
	int               tmp;
	int               timeout;
	int 		  j;
	eicon_isa_codebuf cbuf;
	unsigned char     *code;
	unsigned char     *p;

	if (copy_from_user(&cbuf, cb, sizeof(eicon_isa_codebuf)))
		return -EFAULT;

	if (!(code = kmalloc(cbuf.firmware_len, GFP_KERNEL))) {
		printk(KERN_WARNING "eicon_isa_load: Couldn't allocate code buffer\n");
		return -ENOMEM;
	}

	if (copy_from_user(code, &cb->code, cbuf.firmware_len)) {
		kfree(code);
		return -EFAULT;
	}

	boot = &card->shmem->boot;

	if ((!card->ivalid) && card->master) {
		card->irqprobe = 1;
		/* Check for valid IRQ */
		if ((card->irq < 0) || (card->irq > 15) || 
		    (!((1 << card->irq) & eicon_isa_valid_irq[card->type & 0x0f]))) {
			printk(KERN_WARNING "eicon_isa_load: illegal irq: %d\n", card->irq);
			eicon_isa_release_shmem(card);
			kfree(code);
			return -EINVAL;
		}
		/* Register irq */
		if (!request_irq(card->irq, &eicon_irq, 0, "Eicon ISA ISDN", card))
			card->ivalid = 1;
		else {
			printk(KERN_WARNING "eicon_isa_load: irq %d already in use.\n",
			       card->irq);
			eicon_isa_release_shmem(card);
			kfree(code);
			return -EBUSY;
		}
	}

        tmp = readb(&boot->msize);
        if (tmp != 8 && tmp != 16 && tmp != 24 &&
            tmp != 32 && tmp != 48 && tmp != 60) {
                printk(KERN_WARNING "eicon_isa_load: invalid memsize\n");
		eicon_isa_release_shmem(card);
                return -EIO;
        }

	eicon_isa_printpar(card);

	/* Download firmware */
	printk(KERN_INFO "%s %dkB, loading firmware ...\n", 
	       eicon_ctype_name[card->type],
	       tmp * 16);
	tmp = cbuf.firmware_len >> 8;
	p = code;
	while (tmp--) {
		memcpy_toio(&boot->b, p, 256);
		writeb(1, &boot->ctrl);
		timeout = jiffies + HZ / 10;
		while (time_before(jiffies, timeout)) {
			if (readb(&boot->ctrl) == 0)
				break;
			SLEEP(2);
		}
		if (readb(&boot->ctrl)) {
			printk(KERN_WARNING "eicon_isa_load: download timeout at 0x%x\n", p-code);
			eicon_isa_release(card);
			kfree(code);
			return -EIO;
		}
		p += 256;
	}
	kfree(code);

	/* Initialize firmware parameters */
	memcpy_toio(&card->shmem->c[8], &cbuf.tei, 14);
	memcpy_toio(&card->shmem->c[32], &cbuf.oad, 96);
	memcpy_toio(&card->shmem->c[128], &cbuf.oad, 96);
	
	/* Start firmware, wait for signature */
	writeb(2, &boot->ctrl);
	timeout = jiffies + (5*HZ);
	while (time_before(jiffies, timeout)) {
		if (readw(&boot->signature) == 0x4447)
			break;
		SLEEP(2);
	}
	if (readw(&boot->signature) != 0x4447) {
		printk(KERN_WARNING "eicon_isa_load: firmware selftest failed %04x\n",
		       readw(&boot->signature));
		eicon_isa_release(card);
		return -EIO;
	}

	card->channels = readb(&card->shmem->c[0x3f6]);

	/* clear irq-requests, reset irq-count */
	readb(card->intack);
	writeb(0, card->intack);

	if (card->master) {
		card->irqprobe = 1;
		/* Trigger an interrupt and check if it is delivered */
		tmp = readb(&card->shmem->com.ReadyInt);
		tmp ++;
		writeb(tmp, &card->shmem->com.ReadyInt);
		timeout = jiffies + HZ / 5;
		while (time_before(jiffies, timeout)) {
			if (card->irqprobe > 1)
				break;
			SLEEP(2);
		}
		if (card->irqprobe == 1) {
			printk(KERN_WARNING "eicon_isa_load: IRQ # %d test failed\n", card->irq);
			eicon_isa_release(card);
			return -EIO;
		}
	}
#ifdef EICON_MCA_DEBUG
	printk(KERN_INFO "eicon_isa_load: IRQ # %d test succeeded.\n", card->irq);
#endif

	writeb(card->irq, &card->shmem->com.Int);

	/* initializing some variables */
	((eicon_card *)card->card)->ReadyInt = 0;
	((eicon_card *)card->card)->ref_in  = 1;
	((eicon_card *)card->card)->ref_out = 1;
	for(j=0; j<256; j++) ((eicon_card *)card->card)->IdTable[j] = NULL;
	for(j=0; j< (card->channels + 1); j++) {
		((eicon_card *)card->card)->bch[j].e.busy = 0;
		((eicon_card *)card->card)->bch[j].e.D3Id = 0;
		((eicon_card *)card->card)->bch[j].e.B2Id = 0;
		((eicon_card *)card->card)->bch[j].e.ref = 0;
		((eicon_card *)card->card)->bch[j].e.Req = 0;
		((eicon_card *)card->card)->bch[j].e.complete = 1;
		((eicon_card *)card->card)->bch[j].fsm_state = EICON_STATE_NULL;
	}

	printk(KERN_INFO "Eicon: Supported channels: %d\n", card->channels); 
	printk(KERN_INFO "%s successfully started\n", eicon_ctype_name[card->type]);

	/* Enable normal IRQ processing */
	card->irqprobe = 0;
	return 0;
}

#endif /* CONFIG_ISDN_DRV_EICON_ISA */
