/*
 * xor.c : Multiple Devices driver for Linux
 *
 * Copyright (C) 1996, 1997, 1998, 1999, 2000,
 * Ingo Molnar, Matti Aarnio, Jakub Jelinek, Richard Henderson.
 *
 * Dispatch optimized RAID-5 checksumming functions.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example /usr/src/linux/COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define BH_TRACE 0
#include <linux/module.h>
#include <linux/raid/md.h>
#include <linux/raid/xor.h>
#include <asm/xor.h>

/* The xor routines to use.  */
static struct xor_block_template *active_template;

void
xor_block(unsigned int count, struct buffer_head **bh_ptr)
{
	unsigned long *p0, *p1, *p2, *p3, *p4;
	unsigned long bytes = bh_ptr[0]->b_size;

	p0 = (unsigned long *) bh_ptr[0]->b_data;
	p1 = (unsigned long *) bh_ptr[1]->b_data;
	if (count == 2) {
		active_template->do_2(bytes, p0, p1);
		return;
	}

	p2 = (unsigned long *) bh_ptr[2]->b_data;
	if (count == 3) {
		active_template->do_3(bytes, p0, p1, p2);
		return;
	}

	p3 = (unsigned long *) bh_ptr[3]->b_data;
	if (count == 4) {
		active_template->do_4(bytes, p0, p1, p2, p3);
		return;
	}

	p4 = (unsigned long *) bh_ptr[4]->b_data;
	active_template->do_5(bytes, p0, p1, p2, p3, p4);
}

/* Set of all registered templates.  */
static struct xor_block_template *template_list;

#define BENCH_SIZE (PAGE_SIZE)

static void
do_xor_speed(struct xor_block_template *tmpl, void *b1, void *b2)
{
	int speed;
	unsigned long now;
	int i, count, max;

	tmpl->next = template_list;
	template_list = tmpl;

	/*
	 * Count the number of XORs done during a whole jiffy, and use
	 * this to calculate the speed of checksumming.  We use a 2-page
	 * allocation to have guaranteed color L1-cache layout.
	 */
	max = 0;
	for (i = 0; i < 5; i++) {
		now = jiffies;
		count = 0;
		while (jiffies == now) {
			mb();
			tmpl->do_2(BENCH_SIZE, b1, b2);
			mb();
			count++;
			mb();
		}
		if (count > max)
			max = count;
	}

	speed = max * (HZ * BENCH_SIZE / 1024);
	tmpl->speed = speed;

	printk("   %-10s: %5d.%03d MB/sec\n", tmpl->name,
	       speed / 1000, speed % 1000);
}

static int
calibrate_xor_block(void)
{
	void *b1, *b2;
	struct xor_block_template *f, *fastest;

	b1 = (void *) md__get_free_pages(GFP_KERNEL, 2);
	if (! b1) {
		printk("raid5: Yikes!  No memory available.\n");
		return -ENOMEM;
	}
	b2 = b1 + 2*PAGE_SIZE + BENCH_SIZE;

	printk(KERN_INFO "raid5: measuring checksumming speed\n");
	sti();

#define xor_speed(templ)	do_xor_speed((templ), b1, b2)

	XOR_TRY_TEMPLATES;

#undef xor_speed

	free_pages((unsigned long)b1, 2);

	fastest = template_list;
	for (f = fastest; f; f = f->next)
		if (f->speed > fastest->speed)
			fastest = f;

#ifdef XOR_SELECT_TEMPLATE
	fastest = XOR_SELECT_TEMPLATE(fastest);
#endif

	active_template = fastest;
	printk("raid5: using function: %s (%d.%03d MB/sec)\n",
	       fastest->name, fastest->speed / 1000, fastest->speed % 1000);

	return 0;
}

MD_EXPORT_SYMBOL(xor_block);
MODULE_LICENSE("GPL");

module_init(calibrate_xor_block);
