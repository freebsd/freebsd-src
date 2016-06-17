/*
 * drivers/sbus/char/jsflash.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds	(drivers/char/mem.c)
 *  Copyright (C) 1997  Eddie C. Dost		(drivers/sbus/char/flash.c)
 *  Copyright (C) 1997-2000 Pavel Machek <pavel@ucw.cz>   (drivers/block/nbd.c)
 *  Copyright (C) 1999-2000 Pete Zaitcev
 *
 * This driver is used to program OS into a Flash SIMM on
 * Krups and Espresso platforms.
 *
 * TODO: do not allow erase/programming if file systems are mounted.
 * TODO: Erase/program both banks of a 8MB SIMM.
 *
 * It is anticipated that programming an OS Flash will be a routine
 * procedure. In the same time it is exeedingly dangerous because
 * a user can program its OBP flash with OS image and effectively
 * kill the machine.
 *
 * This driver uses an interface different from Eddie's flash.c
 * as a silly safeguard.
 *
 * XXX The flash.c manipulates page caching characteristics in a certain
 * dubious way; also it assumes that remap_page_range() can remap
 * PCI bus locations, which may be false. ioremap() must be used
 * instead. We should discuss this.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/smp_lock.h>

/*
 * <linux/blk.h> is controlled from the outside with these definitions.
 */
#define MAJOR_NR	JSFD_MAJOR

#define DEVICE_NAME "jsfd"
#define DEVICE_REQUEST jsfd_do_request
#define DEVICE_NR(device) (MINOR(device))
#define DEVICE_ON(device)
#define DEVICE_OFF(device)
#define DEVICE_NO_RANDOM

#include <linux/blk.h>


#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/io.h>
#include <asm/pcic.h>
#include <asm/oplib.h>

#include <asm/jsflash.h>		/* ioctl arguments. <linux/> ?? */
#define JSFIDSZ		(sizeof(struct jsflash_ident_arg))
#define JSFPRGSZ	(sizeof(struct jsflash_program_arg))

/*
 * Our device numbers have no business in system headers.
 * The only thing a user knows is the device name /dev/jsflash.
 *
 * Block devices are laid out like this:
 *   minor+0	- Bootstrap, for 8MB SIMM 0x20400000[0x800000]
 *   minor+1	- Filesystem to mount, normally 0x20400400[0x7ffc00]
 *   minor+2	- Whole flash area for any case... 0x20000000[0x01000000]
 * Total 3 minors per flash device.
 *
 * It is easier to have static size vectors, so we define
 * a total minor range JSF_MAX, which must cover all minors.
 */
/* character device */
#define JSF_MINOR	178	/* 178 is registered with hpa */
/* block device */
#define JSF_MAX		 3	/* 3 minors wasted total so far. */
#define JSF_NPART	 3	/* 3 minors per flash device */
#define JSF_PART_BITS	 2	/* 2 bits of minors to cover JSF_NPART */
#define JSF_PART_MASK	 0x3	/* 2 bits mask */

/*
 * Access functions.
 * We could ioremap(), but it's easier this way.
 */
static unsigned int jsf_inl(unsigned long addr)
{
	unsigned long retval;

	__asm__ __volatile__("lda [%1] %2, %0\n\t" :
				"=r" (retval) :
				"r" (addr), "i" (ASI_M_BYPASS));
        return retval;
}

static void jsf_outl(unsigned long addr, __u32 data)
{

	__asm__ __volatile__("sta %0, [%1] %2\n\t" : :
				"r" (data), "r" (addr), "i" (ASI_M_BYPASS) :
				"memory");
}

/*
 * soft carrier
 */

struct jsfd_part {
	unsigned long dbase;
	unsigned long dsize;
	int refcnt;
};

struct jsflash {
	unsigned long base;
	unsigned long size;
	unsigned long busy;		/* In use? */
	struct jsflash_ident_arg id;
	/* int mbase; */		/* Minor base, typically zero */
	struct jsfd_part dv[JSF_NPART];
};

/*
 * We do not map normal memory or obio as a safety precaution.
 * But offsets are real, for ease of userland programming.
 */
#define JSF_BASE_TOP	0x30000000
#define JSF_BASE_ALL	0x20000000

#define JSF_BASE_JK	0x20400000

/*
 */
static int jsfd_blksizes[JSF_MAX];
static int jsfd_sizes[JSF_MAX];
static u64 jsfd_bytesizes[JSF_MAX];

/*
 * Let's pretend we may have several of these...
 */
static struct jsflash jsf0;

/*
 * Wait for AMD to finish its embedded algorithm.
 * We use the Toggle bit DQ6 (0x40) because it does not
 * depend on the data value as /DATA bit DQ7 does.
 *
 * XXX Do we need any timeout here? So far it never hanged, beware broken hw.
 */
static void jsf_wait(unsigned long p) {
	unsigned int x1, x2;

	for (;;) {
		x1 = jsf_inl(p);
		x2 = jsf_inl(p);
		if ((x1 & 0x40404040) == (x2 & 0x40404040)) return;
	}
}

/*
 * Programming will only work if Flash is clean,
 * we leave it to the programmer application.
 *
 * AMD must be programmed one byte at a time;
 * thus, Simple Tech SIMM must be written 4 bytes at a time.
 *
 * Write waits for the chip to become ready after the write
 * was finished. This is done so that application would read
 * consistent data after the write is done.
 */
static void jsf_write4(unsigned long fa, u32 data) {

	jsf_outl(fa, 0xAAAAAAAA);		/* Unlock 1 Write 1 */
	jsf_outl(fa, 0x55555555);		/* Unlock 1 Write 2 */
	jsf_outl(fa, 0xA0A0A0A0);		/* Byte Program */
	jsf_outl(fa, data);

	jsf_wait(fa);
}

/*
 */
static void jsfd_read(char *buf, unsigned long p, size_t togo) {
	union byte4 {
		char s[4];
		unsigned int n;
	} b;

	while (togo >= 4) {
		togo -= 4;
		b.n = jsf_inl(p);
		memcpy(buf, b.s, 4);
		p += 4;
		buf += 4;
	}
}

static void jsfd_do_request(request_queue_t *q)
{
	struct request *req;
	int dev;
	struct jsfd_part *jdp;
	unsigned long offset;
	size_t len;

	for (;;) {
		INIT_REQUEST;	/* if (QUEUE_EMPTY) return; */
		req = CURRENT;

		dev = MINOR(req->rq_dev);
		if (dev >= JSF_MAX || (dev & JSF_PART_MASK) >= JSF_NPART) {
			end_request(0);
			continue;
		}
		jdp = &jsf0.dv[dev & JSF_PART_MASK];

		offset = req->sector << 9;
		len = req->current_nr_sectors << 9;
		if ((offset + len) > jdp->dsize) {
               		end_request(0);
			continue;
		}

		if (req->cmd == WRITE) {
			printk(KERN_ERR "jsfd: write\n");
			end_request(0);
			continue;
		}
		if (req->cmd != READ) {
			printk(KERN_ERR "jsfd: bad req->cmd %d\n", req->cmd);
			end_request(0);
			continue;
		}

		if ((jdp->dbase & 0xff000000) != 0x20000000) {
			printk(KERN_ERR "jsfd: bad base %x\n", (int)jdp->dbase);
			end_request(0);
			continue;
		}

/* printk("jsfd%d: read buf %p off %x len %x\n", dev, req->buffer, (int)offset, (int)len); */ /* P3 */
		jsfd_read(req->buffer, jdp->dbase + offset, len);

		end_request(1);
	}
}

/*
 * The memory devices use the full 32/64 bits of the offset, and so we cannot
 * check against negative addresses: they are ok. The return value is weird,
 * though, in that case (0).
 *
 * also note that seeking relative to the "end of file" isn't supported:
 * it has no meaning, so it returns -EINVAL.
 */
static loff_t jsf_lseek(struct file * file, loff_t offset, int orig)
{
	switch (orig) {
		case 0:
			file->f_pos = offset;
			return file->f_pos;
		case 1:
			file->f_pos += offset;
			return file->f_pos;
		default:
			return -EINVAL;
	}
}

/*
 * OS SIMM Cannot be read in other size but a 32bits word.
 */
static ssize_t jsf_read(struct file * file, char * buf, 
    size_t togo, loff_t *ppos)
{
	unsigned long p = *ppos;
	char *tmp = buf;

	union byte4 {
		char s[4];
		unsigned int n;
	} b;

	if (verify_area(VERIFY_WRITE, buf, togo))
		return -EFAULT; 

	if (p < JSF_BASE_ALL || p >= JSF_BASE_TOP) {
		return 0;
	}

	if ((p + togo) < p	/* wrap */
	   || (p + togo) >= JSF_BASE_TOP) {
		togo = JSF_BASE_TOP - p;
	}

	if (p < JSF_BASE_ALL && togo != 0) {
#if 0 /* __bzero XXX */
		size_t x = JSF_BASE_ALL - p;
		if (x > togo) x = togo;
		clear_user(tmp, x);
		tmp += x;
		p += x;
		togo -= x;
#else
		/*
		 * Implementation of clear_user() calls __bzero
		 * without regard to modversions,
		 * so we cannot build a module.
		 */
		return 0;
#endif
	}

	while (togo >= 4) {
		togo -= 4;
		b.n = jsf_inl(p);
		copy_to_user(tmp, b.s, 4);
		tmp += 4;
		p += 4;
	}

	/*
	 * XXX Small togo may remain if 1 byte is ordered.
	 * It would be nice if we did a word size read and unpacked it.
	 */

	*ppos = p;
	return tmp-buf;
}

static ssize_t jsf_write(struct file * file, const char * buf,
    size_t count, loff_t *ppos)
{
	return -ENOSPC;
}

/*
 */
static int jsf_ioctl_erase(unsigned long arg)
{
	unsigned long p;

	/* p = jsf0.base;	hits wrong bank */
	p = 0x20400000;

	jsf_outl(p, 0xAAAAAAAA);		/* Unlock 1 Write 1 */
	jsf_outl(p, 0x55555555);		/* Unlock 1 Write 2 */
	jsf_outl(p, 0x80808080);		/* Erase setup */
	jsf_outl(p, 0xAAAAAAAA);		/* Unlock 2 Write 1 */
	jsf_outl(p, 0x55555555);		/* Unlock 2 Write 2 */
	jsf_outl(p, 0x10101010);		/* Chip erase */

#if 0
	/*
	 * This code is ok, except that counter based timeout
	 * has no place in this world. Let's just drop timeouts...
	 */
	{
		int i;
		__u32 x;
		for (i = 0; i < 1000000; i++) {
			x = jsf_inl(p);
			if ((x & 0x80808080) == 0x80808080) break;
		}
		if ((x & 0x80808080) != 0x80808080) {
			printk("jsf0: erase timeout with 0x%08x\n", x);
		} else {
			printk("jsf0: erase done with 0x%08x\n", x);
		}
	}
#else
	jsf_wait(p);
#endif

	return 0;
}

/*
 * Program a block of flash.
 * Very simple because we can do it byte by byte anyway.
 */
static int jsf_ioctl_program(unsigned long arg)
{
	struct jsflash_program_arg abuf;
	char *uptr;
	unsigned long p;
	unsigned int togo;
	union {
		unsigned int n;
		char s[4];
	} b;

	if (verify_area(VERIFY_READ, (void *)arg, JSFPRGSZ))
		return -EFAULT; 
	copy_from_user(&abuf, (char *)arg, JSFPRGSZ);
	p = abuf.off;
	togo = abuf.size;
	if ((togo & 3) || (p & 3)) return -EINVAL;

	uptr = (char *) (unsigned long) abuf.data;
	if (verify_area(VERIFY_READ, uptr, togo))
		return -EFAULT;
	while (togo != 0) {
		togo -= 4;
		copy_from_user(&b.s[0], uptr, 4);
		jsf_write4(p, b.n);
		p += 4;
		uptr += 4;
	}

	return 0;
}

static int jsf_ioctl(struct inode *inode, struct file *f, unsigned int cmd,
    unsigned long arg)
{
	int error = -ENOTTY;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	switch (cmd) {
	case JSFLASH_IDENT:
		if (verify_area(VERIFY_WRITE, (void *)arg, JSFIDSZ))
			return -EFAULT; 
		copy_to_user(arg, &jsf0.id, JSFIDSZ);
		error = 0;
		break;
	case JSFLASH_ERASE:
		error = jsf_ioctl_erase(arg);
		break;
	case JSFLASH_PROGRAM:
		error = jsf_ioctl_program(arg);
		break;
	}

	return error;
}

static int jsfd_ioctl(struct inode *inode, struct file *file,
    unsigned int cmd, unsigned long arg)
{
	int dev;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (!inode)
		return -EINVAL;
	if ((dev = MINOR(inode->i_rdev)) >= JSF_MAX) return -ENODEV;

	switch (cmd) {
	case BLKGETSIZE:
		return put_user(jsfd_bytesizes[dev] >> 9, (unsigned long *) arg);
	case BLKGETSIZE64:
		return put_user(jsfd_bytesizes[dev], (u64 *) arg);

#if 0
	case BLKROSET:
	case BLKROGET:
	case BLKSSZGET:
		return blk_ioctl(inode->i_rdev, cmd, arg);
#endif

	/* case BLKFLSBUF: */	/* Program, then read, what happens? Stale? */
	default: ;
	}
	return -ENOTTY;
}

static int jsf_mmap(struct file * file, struct vm_area_struct * vma)
{
	return -ENXIO;
}

static int jsf_open(struct inode * inode, struct file * filp)
{

	if (jsf0.base == 0) return -ENXIO;
	if (test_and_set_bit(0, (void *)&jsf0.busy) != 0)
		return -EBUSY;

	return 0;	/* XXX What security? */
}

static int jsfd_open(struct inode *inode, struct file *file)
{
	struct jsfd_part *jdp;
	int dev;

	if (!inode)
		return -EINVAL;
	dev = MINOR(inode->i_rdev);
	if (dev >= JSF_MAX || (dev & JSF_PART_MASK) >= JSF_NPART) {
		printk(KERN_ALERT "jsfd_open: illegal minor %d\n", dev);
		return -ENODEV;
	}

	jdp = &jsf0.dv[dev];
	jdp->refcnt++;

	return 0;
}

static int jsf_release(struct inode *inode, struct file *file)
{
	jsf0.busy = 0;
	return 0;
}

static int jsfd_release(struct inode *inode, struct file *file)
{
	struct jsfd_part *jdp;
	int dev;

	if (!inode)
		return -ENODEV;
	dev = MINOR(inode->i_rdev);
	if (dev >= JSF_MAX || (dev & JSF_PART_MASK) >= JSF_NPART) {
		printk(KERN_ALERT "jsfd_release: illegal minor %d\n", dev);
		return -ENODEV;
	}

	jdp = &jsf0.dv[dev];
	if (jdp->refcnt <= 0) {
		printk(KERN_ALERT "jsfd_release: bad ref on minor %d\n", dev);
	} else {
		--jdp->refcnt;
	}
	/* N.B. Doesn't lo->file need an fput?? */
	return 0;
}

static struct file_operations jsf_fops = {
	owner:		THIS_MODULE,
	llseek:		jsf_lseek,
	read:		jsf_read,
	write:		jsf_write,
	ioctl:		jsf_ioctl,
	mmap:		jsf_mmap,
	open:		jsf_open,
	release:	jsf_release,
};

static struct miscdevice jsf_dev = { JSF_MINOR, "jsflash", &jsf_fops };

static struct block_device_operations jsfd_fops = {
	owner:		THIS_MODULE,
	open:		jsfd_open,
	release:	jsfd_release,
	ioctl:		jsfd_ioctl,
};

EXPORT_NO_SYMBOLS;

int jsflash_init(void)
{
	int rc;
	struct jsflash *jsf;
	int node;
	char banner[128];
	struct linux_prom_registers reg0;

	node = prom_getchild(prom_root_node);
	node = prom_searchsiblings(node, "flash-memory");
	if (node != 0 && node != -1) {
		if (prom_getproperty(node, "reg",
		    (char *)&reg0, sizeof(reg0)) == -1) {
			printk("jsflash: no \"reg\" property\n");
			return -ENXIO;
		}
		if (reg0.which_io != 0) {
			printk("jsflash: bus number nonzero: 0x%x:%x\n",
			    reg0.which_io, reg0.phys_addr);
			return -ENXIO;
		}
		/*
		 * Flash may be somewhere else, for instance on Ebus.
		 * So, don't do the following check for IIep flash space.
		 */
#if 0
		if ((reg0.phys_addr >> 24) != 0x20) {
			printk("jsflash: suspicious address: 0x%x:%x\n",
			    reg0.which_io, reg0.phys_addr);
			return -ENXIO;
		}
#endif
		if ((int)reg0.reg_size <= 0) {
			printk("jsflash: bad size 0x%x\n", (int)reg0.reg_size);
			return -ENXIO;
		}
	} else {
		/* XXX Remove this code once PROLL ID12 got widespread */
		printk("jsflash: no /flash-memory node, use PROLL >= 12\n");
		prom_getproperty(prom_root_node, "banner-name", banner, 128);
		if (strcmp (banner, "JavaStation-NC") != 0 &&
		    strcmp (banner, "JavaStation-E") != 0) {
			return -ENXIO;
		}
		reg0.which_io = 0;
		reg0.phys_addr = 0x20400000;
		reg0.reg_size  = 0x00800000;
	}

	/* Let us be really paranoid for modifications to probing code. */
	/* extern enum sparc_cpu sparc_cpu_model; */ /* in <asm/system.h> */
	if (sparc_cpu_model != sun4m) {
		/* We must be on sun4m because we use MMU Bypass ASI. */
		return -ENXIO;
	}

	if (jsf0.base == 0) {
		jsf = &jsf0;

		jsf->base = reg0.phys_addr;
		jsf->size = reg0.reg_size;

		/* XXX Redo the userland interface. */
		jsf->id.off = JSF_BASE_ALL;
		jsf->id.size = 0x01000000;	/* 16M - all segments */
		strcpy(jsf->id.name, "Krups_all");

		jsf->dv[0].dbase = jsf->base;
		jsf->dv[0].dsize = jsf->size;
		jsf->dv[1].dbase = jsf->base + 1024;
		jsf->dv[1].dsize = jsf->size - 1024;
		jsf->dv[2].dbase = JSF_BASE_ALL;
		jsf->dv[2].dsize = 0x01000000;

		printk("Espresso Flash @0x%lx [%d MB]\n", jsf->base,
		    (int) (jsf->size / (1024*1024)));
	}

	if ((rc = misc_register(&jsf_dev)) != 0) {
		printk(KERN_ERR "jsf: unable to get misc minor %d\n",
		    JSF_MINOR);
		jsf0.base = 0;
		return rc;
	}

	return 0;
}

int jsfd_init(void) {
	struct jsflash *jsf;
	struct jsfd_part *jdp;
	int i;

	if (jsf0.base == 0) {
		return -ENXIO;
	}

	if (register_blkdev(JSFD_MAJOR, "jsfd", &jsfd_fops)) {
		printk("jsfd_init: unable to get major number %d\n",
		    JSFD_MAJOR);
		return -EIO;
	}

	blksize_size[JSFD_MAJOR] = jsfd_blksizes;
	blk_size[JSFD_MAJOR] = jsfd_sizes;

	blk_init_queue(BLK_DEFAULT_QUEUE(MAJOR_NR), DEVICE_REQUEST);
	/* blk_queue_headactive(BLK_DEFAULT_QUEUE(MAJOR_NR), 0); */
	for (i = 0; i < JSF_MAX; i++) {
		if ((i & JSF_PART_MASK) >= JSF_NPART) continue;
		jsf = &jsf0;	/* actually, &jsfv[i >> JSF_PART_BITS] */
		jdp = &jsf->dv[i&JSF_PART_MASK];

		jdp->refcnt = 0;

		jsfd_blksizes[i] = 1024;
		jsfd_bytesizes[i] = jdp->dsize;
		jsfd_sizes[i] = jsfd_bytesizes[i] >> 10;
		register_disk(NULL, MKDEV(JSFD_MAJOR, i), 1, &jsfd_fops,
				jsfd_bytesizes[i] >> 9);
		set_device_ro(MKDEV(JSFD_MAJOR, i), 1);
	}
	return 0;
}

#ifdef MODULE
MODULE_LICENSE("GPL");

int init_module(void) {
	int rc;

	if ((rc = jsflash_init()) == 0) {
		jsfd_init();
		return 0;
	}
	return rc;
}

void cleanup_module(void) {

	/* for (all probed units) {  } */
	if (jsf0.busy)
		printk("jsf0: cleaning busy unit\n");
	jsf0.base = 0;
	jsf0.busy = 0;

	misc_deregister(&jsf_dev);
	if (unregister_blkdev(JSFD_MAJOR, "jsfd") != 0)
		printk("jsfd: cleanup_module failed\n");
	blk_cleanup_queue(BLK_DEFAULT_QUEUE(MAJOR_NR));
}
#endif
