/*
 * Example showing how to pin down a range of virtual pages from user-space
 * to be able to do for example DMA directly into them.
 *
 * It is necessary because the pages the virtual pointers reference, might
 * not exist in memory (could be mapped to the zero-page, filemapped etc)
 * and DMA cannot trigger the MMU to force them in (and would have time
 * contraints making it impossible to wait for it anyway).
 *
 * Copyright (c) 2001, 2002, 2003  Axis Communications AB
 *
 * Author:  Bjorn Wesen
 *
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/iobuf.h>

#define KIOBUFTEST_MAJOR 124  /* in the local range, experimental */

static ssize_t
kiobuf_read(struct file *filp, char *buf, size_t len, loff_t *ppos)
{
	struct kiobuf *iobuf;
	int res, i;

	/*
	 * Make a kiobuf that maps the entire length the reader has given us.
	 */

	res = alloc_kiovec(1, &iobuf);
	if (res)
		return res;

	if ((res = map_user_kiobuf(READ, iobuf, (unsigned long)buf, len))) {
		printk("map_user_kiobuf failed, return %d\n", res);
		free_kiovec(1, &iobuf);
		return res;
	}

	/*
	 * At this point, the virtual area buf[0] -> buf[len-1] will have
	 * corresponding pages mapped in physical memory and locked until
	 * we unmap the kiobuf. They cannot be swapped out or moved around.
	 */

	printk("nr_pages == %d\noffset == %d\nlength == %d\n",
	       iobuf->nr_pages, iobuf->offset, iobuf->length);

	for (i = 0; i < iobuf->nr_pages; i++) {
		printk("page_add(maplist[%d]) == 0x%x\n", i,
		       page_address(iobuf->maplist[i]));
	}

	/*
	 * This is the place to create the necessary scatter-gather vector
	 * for the DMA using the iobuf->maplist array and page_address (don't
	 * forget __pa if the DMA needs the actual physical DRAM address)
	 * and run it.
	 */



	/* Release the mapping and exit */

	unmap_kiobuf(iobuf); /* The unlock_kiobuf is implicit here */
	free_kiovec(1, &iobuf);

	return len;
}


static struct file_operations kiobuf_fops = {
	owner:    THIS_MODULE,
	read:     kiobuf_read
};

static int __init
kiobuftest_init(void)
{
	int res;

	/* register char device */

	res = register_chrdev(KIOBUFTEST_MAJOR, "kiobuftest", &kiobuf_fops);
	if (res < 0) {
		printk(KERN_ERR "kiobuftest: couldn't get a major number.\n");
		return res;
	}

	printk("Initializing kiobuf-test device\n");
}

static void __exit
kiobuftest_exit(void)
{
	unregister_chrdev(KIOBUFTEST_MAJOR, "kiobuftest");
}

module_init(kiobuftest_init);
module_exit(kiobuftest_exit);
