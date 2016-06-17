/* $Id: tpam_memory.c,v 1.1.2.1 2001/11/20 14:19:37 kai Exp $
 *
 * Turbo PAM ISDN driver for Linux. (Kernel Driver - Board Memory Access)
 *
 * Copyright 2001 Stelian Pop <stelian.pop@fr.alcove.com>, Alcôve
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * For all support questions please contact: <support@auvertech.fr>
 *
 */

#include <linux/pci.h>
#include <asm/io.h>

#include "tpam.h"

/*
 * Write a DWORD into the board memory.
 *
 * 	card: the board
 * 	addr: the address (in the board memory)
 * 	val: the value to put into the memory.
 */
void copy_to_pam_dword(tpam_card *card, const void *addr, u32 val) {

	/* set the page register */
	writel(((unsigned long)addr) | TPAM_PAGE_SIZE, 
	       card->bar0 + TPAM_PAGE_REGISTER);

	/* write the value */
	writel(val, card->bar0 + (((u32)addr) & TPAM_PAGE_SIZE));
}

/*
 * Write n bytes into the board memory. The count of bytes will be rounded
 * up to a multiple of 4.
 *
 * 	card: the board
 * 	to: the destination address (in the board memory)
 * 	from: the source address (in the kernel memory)
 * 	n: number of bytes
 */
void copy_to_pam(tpam_card *card, void *to, const void *from, u32 n) {
	u32 page, offset, count;

	/* need to write in dword ! */
	while (n & 3) n++;

	while (n) {
		page = ((u32)to) | TPAM_PAGE_SIZE;
		offset = ((u32)to) & TPAM_PAGE_SIZE;
		count = n < TPAM_PAGE_SIZE - offset
				? n
				: TPAM_PAGE_SIZE - offset;

		/* set the page register */
		writel(page, card->bar0 + TPAM_PAGE_REGISTER);

		/* copy the data */
		memcpy_toio((void *)(card->bar0 + offset), from, count);
		
		from += count;
		to += count;
		n -= count;
	}
}

/*
 * Read a DWORD from the board memory.
 *
 * 	card: the board
 * 	addr: the address (in the board memory)
 *
 * Return: the value read into the memory.
 */
u32 copy_from_pam_dword(tpam_card *card, const void *addr) {

	/* set the page register */
	writel(((u32)addr) | TPAM_PAGE_SIZE, 
	       card->bar0 + TPAM_PAGE_REGISTER);

	/* read the data */
	return readl(card->bar0 + (((u32)addr) & TPAM_PAGE_SIZE));
}

/*
 * Read n bytes from the board memory.
 *
 * 	card: the board
 * 	to: the destination address (in the kernel memory)
 * 	from: the source address (in the board memory)
 * 	n: number of bytes
 */
void copy_from_pam(tpam_card *card, void *to, const void *from, u32 n) {
	u32 page, offset, count;

	while (n) {
		page = ((u32)from) | TPAM_PAGE_SIZE;
		offset = ((u32)from) & TPAM_PAGE_SIZE;
		count = n < TPAM_PAGE_SIZE - offset 
				? n 
				: TPAM_PAGE_SIZE - offset;

		/* set the page register */
		writel(page, card->bar0 + TPAM_PAGE_REGISTER);

		/* read the data */
		memcpy_fromio(to, (void *)(card->bar0 + offset), count);
		
		from += count;
		to += count;
		n -= count;
	}
}

/*
 * Read n bytes from the board memory and writes them into the user memory.
 *
 * 	card: the board
 * 	to: the destination address (in the userspace memory)
 * 	from: the source address (in the board memory)
 * 	n: number of bytes
 *
 * Return: 0 if OK, <0 if error.
 */
int copy_from_pam_to_user(tpam_card *card, void *to, const void *from, u32 n) {
	void *page;
	u32 count;

	/* allocate a free page for the data transfer */
	if (!(page = (void *)__get_free_page(GFP_KERNEL))) {
		printk(KERN_ERR "TurboPAM(copy_from_pam_to_user): "
		       "get_free_page failed\n");
		return -ENOMEM;
	}

	while (n) {
		count = n < PAGE_SIZE ? n : PAGE_SIZE;

		/* copy data from the board into the kernel memory */
		spin_lock_irq(&card->lock);
		copy_from_pam(card, page, from, count);
		spin_unlock_irq(&card->lock);

		/* copy it from the kernel memory into the user memory */
		if (copy_to_user(to, page, count)) {
			
			/* this can fail... */
			free_page((u32)page);
			return -EFAULT;
		}
		from += count;
		to += count;
		n -= count;
	}

	/* release allocated memory */
	free_page((u32)page);
	return 0;
}

/*
 * Read n bytes from the user memory and writes them into the board memory.
 *
 * 	card: the board
 * 	to: the destination address (in the board memory)
 * 	from: the source address (in the userspace memory)
 * 	n: number of bytes
 *
 * Return: 0 if OK, <0 if error.
 */
int copy_from_user_to_pam(tpam_card *card, void *to, const void *from, u32 n) {
	void *page;
	u32 count;

	/* allocate a free page for the data transfer */
	if (!(page = (void *)__get_free_page(GFP_KERNEL))) {
		printk(KERN_ERR "TurboPAM(copy_from_user_to_pam): "
		       "get_free_page failed\n");
		return -ENOMEM;
	}

	while (n) {
		count = n < PAGE_SIZE ? n : PAGE_SIZE;

		/* copy data from the user memory into the kernel memory */
		if (copy_from_user(page, from, count)) {
			/* this can fail... */
			free_page((u32)page);
			return -EFAULT;
		}

		/* copy it from the kernel memory into the board memory */
		spin_lock_irq(&card->lock);
		copy_to_pam(card, to, page, count);
		spin_unlock_irq(&card->lock);

		from += count;
		to += count;
		n -= count;
	}

	/* release allocated memory */
	free_page((u32)page);
	return 0;
}

/*
 * Verify if we have the permission to read or writes len bytes at the
 * address address from/to the board memory.
 *
 * 	address: the start address (in the board memory)
 * 	len: number of bytes
 *
 * Return: 0 if OK, <0 if error.
 */
int tpam_verify_area(u32 address, u32 len) {

	if (address < TPAM_RESERVEDAREA1_START)
		return (address + len <= TPAM_RESERVEDAREA1_START) ? 0 : -1;

	if (address <= TPAM_RESERVEDAREA1_END)
		return -1;

	if (address < TPAM_RESERVEDAREA2_START)
		return (address + len <= TPAM_RESERVEDAREA2_START) ? 0 : -1;

	if (address <= TPAM_RESERVEDAREA2_END)
		return -1;

	if (address < TPAM_RESERVEDAREA3_START)
		return (address + len <= TPAM_RESERVEDAREA3_START) ? 0 : -1;

	if (address <= TPAM_RESERVEDAREA3_END)
		return -1;

	if (address < TPAM_RESERVEDAREA4_START)
		return (address + len <= TPAM_RESERVEDAREA4_START) ? 0 : -1;

	if (address <= TPAM_RESERVEDAREA4_END)
		return -1;

	return 0;
}

