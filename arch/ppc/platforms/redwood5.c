/*
 *
 *    Copyright 2000-2001 MontaVista Software Inc.
 *      Completed implementation.
 *      Author: Armin Kuster
 *
 *    Module name: redwood5.c
 *
 *    Description:
 *    	IBM redwood5 eval board file
 *
 *      History:  12/29/2001 - Armin
 *    	initail release
 *
 * Please read the COPYING file for all license details.
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/pagemap.h>
#include <asm/io.h>
#include <asm/machdep.h>

void __init
board_setup_arch(void)
{

	bd_t *bip = (bd_t *)__res;

#ifdef CONFIG_DEBUG_BRINGUP
	printk("\n");
	printk("machine\t: %s\n", PPC4xx_MACHINE_NAME);
	printk("\n");
	printk("bi_s_version\t %s\n",      bip->bi_s_version);
	printk("bi_r_version\t %s\n",      bip->bi_r_version);
	printk("bi_memsize\t 0x%8.8x\t %dMBytes\n", bip->bi_memsize,bip->bi_memsize/(1024*1000));
	printk("bi_enetaddr %d\t %2.2x%2.2x%2.2x-%2.2x%2.2x%2.2x\n", 0,
	bip->bi_enetaddr[0], bip->bi_enetaddr[1],
	bip->bi_enetaddr[2], bip->bi_enetaddr[3],
	bip->bi_enetaddr[4], bip->bi_enetaddr[5]);

	printk("bi_intfreq\t 0x%8.8x\t clock:\t %dMhz\n",
	       bip->bi_intfreq, bip->bi_intfreq/ 1000000);

	printk("bi_busfreq\t 0x%8.8x\t plb bus clock:\t %dMHz\n",
		bip->bi_busfreq, bip->bi_busfreq / 1000000 );
	printk("bi_tbfreq\t 0x%8.8x\t TB freq:\t %dMHz\n",
	       bip->bi_tbfreq, bip->bi_tbfreq/1000000);

	printk("\n");
#endif

}

void __init
board_io_mapping(void)
{
	int i;

	for (i = 0; i < 16; i++) {
	 unsigned long v, p;

	/* 0x400x0000 -> 0xe00x0000 */
	p = 0x40000000 | (i << 16);
	v = STB04xxx_IO_BASE | (i << 16);

	io_block_mapping(v, p, PAGE_SIZE,
		 _PAGE_NO_CACHE | pgprot_val(PAGE_KERNEL) | _PAGE_GUARDED);
	}


}

void __init
board_setup_irq(void)
{
}

void __init
board_init(void)
{
}
