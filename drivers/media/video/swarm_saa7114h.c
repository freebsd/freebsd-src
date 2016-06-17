/* 
   saa7114h - Philips SAA7114H video decoder driver

   Copyright (C) 2001,2002,2003 Broadcom Corporation

   From saa7111.c:
     Copyright (C) 1998 Dave Perks <dperks@ibm.net>
   From cpia.c:
     (C) Copyright 1999-2000 Peter Pregler
     (C) Copyright 1999-2000 Scott J. Bertin
     (C) Copyright 1999-2000 Johannes Erdfelt <johannes@erdfelt.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Important note: this driver is reasonably functional, and has been
 * tested with the "camserv" v4l application.  But it primarily a
 * proof-of-concept, and example for setting up FIFO-mode.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/ctype.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/pci.h>
#include <linux/signal.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <linux/sched.h>
#include <asm/segment.h>
#include <linux/types.h>
#include <linux/wrapper.h>
#include <linux/smp_lock.h>
#include <asm/hardirq.h>

#include <linux/i2c.h>
#include <linux/videodev.h>
#include <linux/version.h>
#include <asm/uaccess.h>

#include <linux/i2c-algo-sibyte.h>

#include <asm/sibyte/64bit.h>
#include <asm/sibyte/sb1250_regs.h>
#include <asm/sibyte/sb1250_int.h>
#include <asm/sibyte/sb1250_mac.h>
#include <asm/sibyte/sb1250_dma.h>

#define SAA_BRIGHTNESS	 0x0a
#define SAA_CONTRAST	 0x0b
#define SAA_SATURATION	 0x0c
#define SAA_HUE		 0x0d

#define DECODER_STATUS	 0x1f
#define SLICER_STATUS_0	 0x60
#define SLICER_STATUS_1	 0x61
#define SLICER_STATUS_2	 0x62
#define SCALER_STATUS	 0x8f

#define NUM_FRAME	 2
#define MAX_HORIZ	 720
#define MAX_VERT	 480
#define MIN_HORIZ	 180
#define MIN_VERT	 120
#define MAX_PER_PIXEL	 3
#define MAX_FRAME_SIZE	 (MAX_HORIZ*MAX_VERT*MAX_PER_PIXEL)
#define MAX_MMAP_SIZE	 (PAGE_ALIGN(MAX_FRAME_SIZE*NUM_FRAME))
#define RAW_PER_PIXEL	 2
#define RAW_LINE_PAD	 8
#define RAW_LINE_SIZE	 (((MAX_HORIZ*RAW_PER_PIXEL)+RAW_LINE_PAD+0x1f) & ~0x1f)
#define RAW_FRAME_SIZE	 (RAW_LINE_SIZE*MAX_VERT)

#define NUM_DESCR	 64
#define INTR_PKT_CNT	 8

/* Extensions to videodev.h IOCTL definitions */
#define VIDIOREADREG	_IOR('v', 50, int)
#define VIDIOWRITEREG	_IOW('v', 50, int)
#define VIDIOGRABFRAME	_IOR('v', 51, int)
#define VIDIOSHOWEAV	_IOR('v', 52, int)

#define IF_NAME "saa7114h"

#define MAC2_CSR(r)	   (KSEG1 + A_MAC_REGISTER(2, r))
#define MAC2_DMARX0_CSR(r) (KSEG1 + A_MAC_DMA_REGISTER(2, DMA_RX, 0, r))

/* Options */
#define DMA_DEINTERLACE	 1
#define LAZY_READ	 1
#define NULL_DMA	 0

/* Debug filters */
#define DBG_NULL	 0x0000
#define DBG_IO		 0x0001
#define DBG_DESCR	 0x0002
#define DBG_INTR	 0x0004
#define DBG_CONVERT	 0x0008
#define DBG_FRAMING	 0x0010
#define DBG_REGISTER	 0x0020
#define DBG_CALL	 0x0040
#define DBG_FRAMING_LOUD 0x0080

/* XXXKW make this settable through /proc... */
#define DEBUG_LVL	 (DBG_NULL)

#if DEBUG_LVL
#define DBG(l, p) do { if (DEBUG_LVL & l) p; } while (0)
#else
#define DBG(l, p)
#endif

/* ----------------------------------------------------------------------- */

enum {
	FRAME_READY,		/* Ready to grab into */
	FRAME_GRABBING,		/* In the process of being grabbed into */
	FRAME_DONE,		/* Finished grabbing, but not been synced yet */
	FRAME_UNUSED,		/* Unused (belongs to driver, but can't be used) */
};

struct saa_frame {
	uint8_t		 *data;
	uint8_t		 *pos;
	int		  width;
	int		  height;
	uint32_t	  size;
	volatile int	  state;
	wait_queue_head_t read_wait;
};

typedef struct fifo_descr_s {
	uint64_t descr_a;
	uint64_t descr_b;
} fifo_descr_t;

typedef unsigned long paddr_t;

typedef struct fifo_s {
	unsigned	 ringsz;
	fifo_descr_t	*descrtab;
	fifo_descr_t	*descrtab_end;
	fifo_descr_t	*next_descr;
	paddr_t		 descrtab_phys;
	void		*dma_buf;	    /* DMA buffer */
} fifo_t;

struct saa7114h {
	struct i2c_client    *client;
	struct video_device  *vd;
	struct video_window   vw;
	struct video_picture  vp;
	uint8_t		      reg[256];

	fifo_t		 ff;
	void		*frame_buf; /* hold frames for the client */
	struct saa_frame frame[NUM_FRAME]; /* point into frame_buf */
	int		 hwframe;
	int		 swframe;

	uint16_t depth;
	uint16_t palette;
	uint8_t	 bright;
	uint8_t	 contrast;
	uint8_t	 hue;
	uint8_t	 sat;

	struct proc_dir_entry *proc_entry;
	struct semaphore       param_lock;
	struct semaphore       busy_lock;

	int	dma_enable;
	int	opened;
	int	irq;
	int	interlaced;
};

static int saa7114h_probe(struct i2c_adapter *adap);
static int saa7114h_detach(struct i2c_client *device);

struct i2c_driver i2c_driver_saa7114h =
{
	name:		"saa7114h",		/* name */
	id:		I2C_DRIVERID_SAA7114H,	/* ID */
	flags:		I2C_DF_NOTIFY,		/* XXXKW do I care? */
	attach_adapter: saa7114h_probe,
	detach_client:	saa7114h_detach
};

/* -----------------------------------------------------------------------
 * VM assist for MMAPed space
 * ----------------------------------------------------------------------- */

/* Given PGD from the address space's page table, return the kernel
 * virtual mapping of the physical memory mapped at ADR.
 */
static inline unsigned long uvirt_to_kva(pgd_t *pgd, unsigned long adr)
{
	unsigned long ret = 0UL;
	pmd_t *pmd;
	pte_t *ptep, pte;

	if (!pgd_none(*pgd)) {
		pmd = pmd_offset(pgd, adr);
		if (!pmd_none(*pmd)) {
			ptep = pte_offset(pmd, adr);
			pte = *ptep;
			if (pte_present(pte)) {
				ret = (unsigned long) page_address(pte_page(pte));
				ret |= (adr & (PAGE_SIZE-1));
			}
		}
	}
	return ret;
}

/* Here we want the physical address of the memory.
 * This is used when initializing the contents of the
 * area and marking the pages as reserved.
 */
static inline unsigned long kvirt_to_pa(unsigned long adr)
{
	unsigned long va, kva, ret;

	va = VMALLOC_VMADDR(adr);
	kva = uvirt_to_kva(pgd_offset_k(va), va);
	ret = __pa(kva);
	return ret;
}

static void *rvmalloc(unsigned long size)
{
	void *mem;
	unsigned long adr, page;

	/* Round it off to PAGE_SIZE */
	size += (PAGE_SIZE - 1);
	size &= ~(PAGE_SIZE - 1);

	mem = vmalloc_32(size);
	if (!mem)
		return NULL;

	memset(mem, 0, size); /* Clear the ram out, no junk to the user */
	adr = (unsigned long) mem;
	while (size > 0) {
		page = kvirt_to_pa(adr);
		mem_map_reserve(virt_to_page(__va(page)));
		adr += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}

	return mem;
}

static void rvfree(void *mem, unsigned long size)
{
	unsigned long adr, page;

	if (!mem)
		return;

	size += (PAGE_SIZE - 1);
	size &= ~(PAGE_SIZE - 1);

	adr = (unsigned long) mem;
	while (size > 0) {
		page = kvirt_to_pa(adr);
		mem_map_unreserve(virt_to_page(__va(page)));
		adr += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}
	vfree(mem);
}

/* -----------------------------------------------------------------------
 * Control interface (i2c)
 * ----------------------------------------------------------------------- */

static int saa7114h_reg_read(struct saa7114h *dev, unsigned char subaddr)
{
	return i2c_smbus_read_byte_data(dev->client, subaddr);
}

static int saa7114h_reg_write(struct saa7114h *dev, unsigned char subaddr, int data)
{
	return i2c_smbus_write_byte_data(dev->client, subaddr, data & 0xff);
}

static int saa7114h_reg_init(struct saa7114h *dev, unsigned const char *data, unsigned int len)
{
	int rc = 0;
	int val;

	while (len && !rc) {
		dev->reg[data[0]] = data[1];
		rc = saa7114h_reg_write(dev, data[0], data[1]);
		if (!rc && (data[0] != 0)) {
			val = saa7114h_reg_read(dev, data[0]);
			if ((val < 0) || (val != data[1])) {
				printk(KERN_ERR
				       IF_NAME ": init readback mismatch reg %02x = %02x (should be %02x)\n",
				       data[0], val, data[1]);
			}
		}
		len -= 2;
		data += 2;
	}
	return rc;
}

/* -----------------------------------------------------------------------
 * /proc interface
 * ----------------------------------------------------------------------- */

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *saa7114h_proc_root=NULL;

static int decoder_read_proc(char *page, char **start, off_t off,
			     int count, int *eof, void *data)
{
	char *out = page;
	int len, status;
	struct saa7114h *decoder = data;

	out += sprintf(out, "  SWARM saa7114h\n------------------\n");
	status = saa7114h_reg_read(decoder, DECODER_STATUS);
	out += sprintf(out, "  Decoder status = %02x\n", status);
	if (status & 0x80)
		out += sprintf(out, "	 interlaced\n");
	if (status & 0x40)
		out += sprintf(out, "	 not locked\n");
	if (status & 0x02)
		out += sprintf(out, "	 Macrovision detected\n");
	if (status & 0x01)
		out += sprintf(out, "	 color\n");
	out += sprintf(out, "  Brightness = %02x\n", decoder->bright);
	out += sprintf(out, "  Contrast	  = %02x\n", decoder->contrast);
	out += sprintf(out, "  Saturation = %02x\n", decoder->sat);
	out += sprintf(out, "  Hue	  = %02x\n\n", decoder->hue);

	out += sprintf(out, "  Scaler status  = %02x\n", 
		       (int)saa7114h_reg_read(decoder, SCALER_STATUS));

	len = out - page;
	len -= off;
	if (len < count) {
		*eof = 1;
		if (len <= 0) return 0;
	} else
		len = count;

	*start = page + off;
	return len;
}

static int decoder_write_proc(struct file *file, const char *buffer,
			       unsigned long count, void *data)
{
	struct saa7114h *d = data;
	int retval;
	unsigned int cmd, reg, reg_val;
	
	if (down_interruptible(&d->param_lock))
		return -ERESTARTSYS;

#define VALUE \
	({ \
		char *_p; \
		unsigned long int _ret; \
		while (count && isspace(*buffer)) { \
			buffer++; \
			count--; \
		} \
		_ret = simple_strtoul(buffer, &_p, 16); \
		if (_p == buffer) \
			retval = -EINVAL; \
		else { \
			count -= _p - buffer; \
			buffer = _p; \
		} \
		_ret; \
	})
	
	retval = 0;
	while (count && !retval) {
		cmd = VALUE;
		if (retval)
			break;
		switch (cmd) {
		case 1:
			reg = VALUE;
			if (retval)
				break;
			reg_val = VALUE;
			if (retval)
				break;
			printk(IF_NAME ": write reg %x <- %x\n", reg, reg_val);
			if (saa7114h_reg_write(d, reg, reg_val) == -1)
				retval = -EINVAL;
			break;
		case 2:
			reg = VALUE;
			if (retval)
				break;
			reg_val = saa7114h_reg_read(d, reg);
			if (reg_val == -1)
				retval = -EINVAL;
			else
				printk(IF_NAME ": read reg %x -> %x\n", reg, reg_val);
			break;
		default:
			break;
		}
	}
	up(&d->param_lock);
	
	return retval;
}

static void create_proc_decoder(struct saa7114h *decoder)
{
	char name[8];
	struct proc_dir_entry *ent;
	
	if (!saa7114h_proc_root || !decoder)
		return;

	sprintf(name, "video%d", decoder->vd->minor);
	
	ent = create_proc_entry(name, S_IFREG|S_IRUGO|S_IWUSR, saa7114h_proc_root);
	if (!ent) {
		printk(KERN_INFO IF_NAME ": Unable to initialize /proc/saa7114h/%s\n", name);
		return;
	}

	ent->data = decoder;
	ent->read_proc = decoder_read_proc;
	ent->write_proc = decoder_write_proc;
	ent->size = 3626;	/* XXXKW ??? */
	decoder->proc_entry = ent;
}

static void destroy_proc_decoder(struct saa7114h *decoder)
{
	char name[7];
	
	if (!decoder || !decoder->proc_entry)
		return;
	
	sprintf(name, "video%d", decoder->vd->minor);
	remove_proc_entry(name, saa7114h_proc_root);
	decoder->proc_entry = NULL;
}

static void proc_saa7114h_create(void)
{
	saa7114h_proc_root = create_proc_entry("saa7114h", S_IFDIR, 0);

	if (saa7114h_proc_root)
		saa7114h_proc_root->owner = THIS_MODULE;
	else
		printk(KERN_INFO IF_NAME ": Unable to initialize /proc/saa7114h\n");
}

static void proc_saa7114h_destroy(void)
{
	remove_proc_entry("saa7114h", 0);
}
#endif /* CONFIG_PROC_FS */


/* -----------------------------------------------------------------------
 * Initialization
 * ----------------------------------------------------------------------- */

static int dma_setup(struct saa7114h *d)
{
	int i;
	void *curbuf;

	/* Reset the port */
	out64(M_MAC_PORT_RESET, MAC2_CSR(R_MAC_ENABLE));
	in64(MAC2_CSR(R_MAC_ENABLE));

	/* Zero everything out, disable filters */
	out64(0, MAC2_CSR(R_MAC_TXD_CTL));
	out64(M_MAC_ALLPKT_EN, MAC2_CSR(R_MAC_ADFILTER_CFG));
	out64(V_MAC_RX_RD_THRSH(4) | V_MAC_RX_RL_THRSH(4),
	      MAC2_CSR(R_MAC_THRSH_CFG));
	for (i=0; i<MAC_CHMAP_COUNT; i++) {
		out64(0, MAC2_CSR(R_MAC_CHLO0_BASE+(i*8)));
		out64(0, MAC2_CSR(R_MAC_CHUP0_BASE+(i*8)));
	}
	for (i=0; i<MAC_HASH_COUNT; i++) {
		out64(0, MAC2_CSR(R_MAC_HASH_BASE+(i*8)));
	}
	for (i=0; i<MAC_ADDR_COUNT; i++) {
		out64(0, MAC2_CSR(R_MAC_ADDR_BASE+(i*8)));
	}	 
	
	out64(V_MAC_MAX_FRAMESZ(16*1024) | V_MAC_MIN_FRAMESZ(0),
	      MAC2_CSR(R_MAC_FRAMECFG));

	/* Select bypass mode */
	out64((M_MAC_BYPASS_SEL | V_MAC_BYPASS_CFG(K_MAC_BYPASS_EOP) | 
	       M_MAC_FC_SEL | M_MAC_SS_EN | V_MAC_SPEED_SEL_1000MBPS),
	      MAC2_CSR(R_MAC_CFG));

	/* Set up the descriptor table */
	d->ff.descrtab = kmalloc(NUM_DESCR * sizeof(fifo_descr_t), GFP_KERNEL);
	d->ff.descrtab_phys = __pa(d->ff.descrtab);
	d->ff.descrtab_end = d->ff.descrtab + NUM_DESCR;
	d->ff.next_descr = d->ff.descrtab;
	d->ff.ringsz = NUM_DESCR;
#if 0
	/* XXXKW this won't work because the physical may not be
	   contiguous; how do I handle a bigger alloc then? */
	d->ff.dma_buf = rvmalloc(RAW_LINE_SIZE*NUM_DESCR);
	printk(KERN_DEBUG IF_NAME ": DMA buffer allocated (%p)\n",
	       d->ff.dma_buf);
#else
	d->ff.dma_buf = kmalloc(RAW_LINE_SIZE*NUM_DESCR, GFP_KERNEL);
#endif
	if (!d->ff.dma_buf) {
		printk(KERN_ERR IF_NAME ": couldn't allocate DMA buffer\n");
		return -ENOMEM;
	}
	memset(d->ff.dma_buf, 0, RAW_LINE_SIZE*NUM_DESCR);

	for (i=0, curbuf=d->ff.dma_buf; i<d->ff.ringsz; i++, curbuf+=RAW_LINE_SIZE) {
		d->ff.descrtab[i].descr_a = (__pa(curbuf) |
					     V_DMA_DSCRA_A_SIZE(RAW_LINE_SIZE >> 5));
		d->ff.descrtab[i].descr_b = 0;
	}

	out64(V_DMA_INT_PKTCNT(INTR_PKT_CNT) | M_DMA_EOP_INT_EN |
	      V_DMA_RINGSZ(d->ff.ringsz) | M_DMA_TDX_EN,
	      MAC2_DMARX0_CSR(R_MAC_DMA_CONFIG0));
	out64(M_DMA_L2CA, MAC2_DMARX0_CSR(R_MAC_DMA_CONFIG1));
	out64(d->ff.descrtab_phys, MAC2_DMARX0_CSR(R_MAC_DMA_DSCR_BASE));

	/* Enable interrupts and DMA */
	out64(M_MAC_INT_EOP_COUNT<<S_MAC_RX_CH0, MAC2_CSR(R_MAC_INT_MASK));
	out64(M_MAC_RXDMA_EN0 | M_MAC_BYP_RX_ENABLE, MAC2_CSR(R_MAC_ENABLE));

	return 0;
}

/* -----------------------------------------------------------------------
 * v4linux helpers - color conversion, etc  (taken from cpia.c)
 * ----------------------------------------------------------------------- */

#define LIMIT(x) ((((x)>0xffffff)?0xff0000:(((x)<=0xffff)?0:(x)&0xff0000))>>16)

static void yuvconvert_inplace(uint8_t *data, uint32_t in_uyvy, int out_fmt, int mmap)
{
	int y, u, v, r, g, b, y1;
	uint8_t *src, *dst;

	if (out_fmt == VIDEO_PALETTE_RGB24) {
		src = (uint8_t *)((int)data + in_uyvy);
		dst = (uint8_t *)((int)data + in_uyvy + (in_uyvy >> 1));
		DBG(DBG_CONVERT, printk(KERN_DEBUG "inplace: %p %p %p\n", data, src, dst));
		while (src > data) {
			if ((int)(src-data) < 4)
				break;
				//printk("freaky %p %p\n", src, data);
			y1 = (*(--src) - 16) * 76310;
			v = *(--src) - 128;
			y = (*(--src) - 16) * 76310;
			u = *(--src) - 128;
			r = 104635 * v;
			g = -25690 * u + -53294 * v;
			b = 132278 * u;
			/* XXXKW what on earth is up with mmap? */
			if (mmap) {
				*(--dst) = LIMIT(r+y1);
				*(--dst) = LIMIT(g+y1);
				*(--dst) = LIMIT(b+y1);
				*(--dst) = LIMIT(r+y);
				*(--dst) = LIMIT(g+y);
				*(--dst) = LIMIT(b+y);
			} else {
				*(--dst) = LIMIT(b+y1);
				*(--dst) = LIMIT(g+y1);
				*(--dst) = LIMIT(r+y1);
				*(--dst) = LIMIT(b+y);
				*(--dst) = LIMIT(g+y);
				*(--dst) = LIMIT(r+y);
			}
		}
	}
}

static int saa7114h_get_cparams(struct saa7114h *decoder)
{
	/* XXX check for error code */
	decoder->bright	    = saa7114h_reg_read(decoder, SAA_BRIGHTNESS);
	decoder->contrast   = saa7114h_reg_read(decoder, SAA_CONTRAST);
	decoder->sat	    = saa7114h_reg_read(decoder, SAA_SATURATION);
	decoder->hue	    = saa7114h_reg_read(decoder, SAA_HUE);

	decoder->vp.brightness = (uint16_t)decoder->bright << 8;
	decoder->vp.contrast   = (uint16_t)decoder->contrast << 9;
	decoder->vp.colour     = decoder->sat << 9;
	decoder->vp.hue	       = ((int16_t)decoder->hue + 128) << 8;
	return 0;
}

static int saa7114h_set_cparams(struct saa7114h *decoder)
{
	decoder->bright	  = decoder->vp.brightness >> 8;
	decoder->contrast = decoder->vp.contrast >> 9;
	decoder->sat	  = decoder->vp.colour >> 9;
	decoder->hue	  = (uint8_t)((int8_t)(decoder->vp.hue >> 8) - 128);

	return (saa7114h_reg_write(decoder, SAA_BRIGHTNESS, decoder->bright) ||
		saa7114h_reg_write(decoder, SAA_CONTRAST, decoder->contrast) ||
		saa7114h_reg_write(decoder, SAA_SATURATION, decoder->sat) ||
		saa7114h_reg_write(decoder, SAA_HUE, decoder->hue));
}

/* -----------------------------------------------------------------------
 * Custom IOCTL support
 * ----------------------------------------------------------------------- */

unsigned char eav[625][2];
static int grab_frame(struct saa7114h *d, void *user_buf, int print_eav)
{
	int cur_idx = 0;
	int to_go = 625;
	int delta;
	int i, len, eav_val, sav_val;
	int started = 0;
	uint8_t *buf;
	fifo_descr_t *cur_d;
	int swptr = d->ff.next_descr - d->ff.descrtab;
	int hwptr;

	DBG(DBG_CALL, printk(IF_NAME ": grabbing frame\n"));

	/* Check for Macrovision -- if it's on, DMA won't happen */
	if (saa7114h_reg_read(d, DECODER_STATUS) & 0x2)
		return -EACCES;

	out64(d->ff.ringsz, MAC2_DMARX0_CSR(R_MAC_DMA_DSCR_CNT));
	do {
		hwptr = (unsigned) (((in64(MAC2_DMARX0_CSR(R_MAC_DMA_CUR_DSCRADDR)) &
				      M_DMA_CURDSCR_ADDR) -
				     d->ff.descrtab_phys) /
				    sizeof(fifo_descr_t));
		delta = (hwptr + d->ff.ringsz - swptr) % d->ff.ringsz;
		
		if (delta == 0) {
#if 0
			uint64_t val = in64(MAC2_DMARX0_CSR(R_MAC_STATUS));
			printk("mac status: %08x%08x\n",
			       (u32)(val >> 32), (u32)(val&0xffffffff));
#endif
		}

		for (i=0; i<delta; i++) {
			cur_d = d->ff.next_descr;
			if (++d->ff.next_descr == d->ff.descrtab_end)
				d->ff.next_descr = d->ff.descrtab;
			
			if (!(cur_d->descr_a & M_DMA_ETHRX_SOP)) {
				printk("bogus RX\n");
				continue;
			}
			cur_d->descr_a &= ~M_DMA_ETHRX_SOP;
			len = G_DMA_DSCRB_PKT_SIZE(cur_d->descr_b);
			buf = (uint8_t *)__va(cur_d->descr_a & M_DMA_DSCRA_A_ADDR);
			if (len != (d->vw.width*RAW_PER_PIXEL)+RAW_LINE_PAD) {
				printk("funny size %d\n", len);
				continue;
			}
			eav_val = buf[1];
			sav_val = buf[5];
			if (eav_val == 0xf1) { /* end of field 2, V-blank */
				if (started) {
					started = 0;
					delta = to_go = 0;
					/* just let DMA finish in background */
				} else {
					started = 1;
				}
			}
			if (started) {
				eav[cur_idx][0] = eav_val;
				eav[cur_idx++][1] = sav_val;
				if (copy_to_user(user_buf, &buf[6], 1440))
					return -EFAULT;
				user_buf += 1440;
			}
		}
		swptr = hwptr;
		if (delta) {
			if (started)
				to_go -= delta;
			if (delta > to_go)
				delta = to_go;
			out64(delta, MAC2_DMARX0_CSR(R_MAC_DMA_DSCR_CNT));
		}
	} while (to_go);

	if (print_eav) {
		for (i=0; i<cur_idx; i++) {
			printk("%3d: %02x | %02x\n", i, eav[i][0], eav[i][1]);
		}
	}

	return cur_idx;
}

/* -----------------------------------------------------------------------
 * Interrupt handler
 * ----------------------------------------------------------------------- */

unsigned long int_count = 0;

static void saa7114h_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct saa7114h *d = dev_id;
	uint64_t status_val;
	fifo_descr_t *cur_d;
	int i, delta, len;
	uint8_t *buf, eav_val;
	int swptr = d->ff.next_descr - d->ff.descrtab;
	int hwptr;

	status_val = in64(MAC2_CSR(R_MAC_STATUS));

	/* Process finished decsriptors */
	hwptr = (unsigned) (((in64(MAC2_DMARX0_CSR(R_MAC_DMA_CUR_DSCRADDR)) &
			      M_DMA_CURDSCR_ADDR) - d->ff.descrtab_phys) /
			    sizeof(fifo_descr_t));
	delta = (hwptr + d->ff.ringsz - swptr) % d->ff.ringsz;
	if (!delta) {
		if (status_val & M_MAC_INT_EOP_SEEN<<S_MAC_RX_CH0) {
			/* Must have wrapped since the last interrupt */
			delta = d->ff.ringsz;
		} else {
			/* XXXKW why would this happen? */
			return;
		}
	}

	for (i=0; i<delta; i++) {
		cur_d = d->ff.next_descr;
		if (++d->ff.next_descr == d->ff.descrtab_end)
			d->ff.next_descr = d->ff.descrtab;
		
		if (!(cur_d->descr_a & M_DMA_ETHRX_SOP)) {
			printk(KERN_DEBUG "bogus RX\n");
			continue;
		}
		cur_d->descr_a &= ~M_DMA_ETHRX_SOP;
		if (!d->dma_enable)
			continue;
		
		len = G_DMA_DSCRB_PKT_SIZE(cur_d->descr_b);
		buf = (uint8_t *)__va(cur_d->descr_a & M_DMA_DSCRA_A_ADDR);
		if (len != (d->vw.width*RAW_PER_PIXEL)+RAW_LINE_PAD) {
			printk(KERN_DEBUG "funny size %d\n", len);
//				  continue;
		}
		len -= RAW_LINE_PAD;
		eav_val = buf[1];
		DBG(DBG_FRAMING_LOUD,
		    printk(KERN_DEBUG "eav: %02x len: %d\n", eav_val, len));
		if (eav_val == 0xf1) { /* end of field 2, V-blank: start-of-frame */
			switch (d->frame[d->hwframe].state) {
			case FRAME_UNUSED:
				DBG(DBG_FRAMING,
				    printk(KERN_ERR "capture to unused frame %d\n", 
					   d->hwframe));
				break;
			case FRAME_READY:
				DBG(DBG_FRAMING,
				    printk(KERN_DEBUG "frame started %d\n",
					   d->hwframe));
				/* start this frame (skip eav/sav) */
				memcpy(d->frame[d->hwframe].pos, &buf[6], len);
#if DMA_DEINTERLACE
				if (!d->interlaced)
					memcpy(d->frame[d->hwframe].pos-len, &buf[6], len);
				d->frame[d->hwframe].pos += len*2;
#else
				d->frame[d->hwframe].pos += len;
#endif
				d->frame[d->hwframe].state = FRAME_GRABBING;
				/* XXXKW check pos overflow */
				break;
			case FRAME_GRABBING:
				/* kick over to new frame */
				d->frame[d->hwframe].size = d->frame[d->hwframe].pos -
					d->frame[d->hwframe].data;
				d->frame[d->hwframe].state = FRAME_DONE;
				DBG(DBG_FRAMING,
				    printk(KERN_DEBUG "frame finished %d\n",
					   d->frame[d->hwframe].size));
				/* wake up a waiting reader */
				DBG(DBG_IO, printk(KERN_DEBUG "wakeup\n"));
				wake_up(&d->frame[d->hwframe].read_wait);
				d->hwframe = (d->hwframe + 1) % NUM_FRAME;
				if (d->frame[d->hwframe].state == FRAME_READY) {
					/* start this frame */
					DBG(DBG_FRAMING,
					    printk(KERN_DEBUG "frame bumped %d\n",
						   d->hwframe));
					memcpy(d->frame[d->hwframe].pos, &buf[6], len);
#if DMA_DEINTERLACE
					if (!d->interlaced)
						memcpy(d->frame[d->hwframe].pos-len, &buf[6], len);
					d->frame[d->hwframe].pos += len*2;
#else
					d->frame[d->hwframe].pos += len;
#endif
					d->frame[d->hwframe].state = FRAME_GRABBING;
				} else {
					/* drop on the floor,
					   note that we've stopped DMA'ing */
					DBG(DBG_FRAMING,
					    printk(KERN_DEBUG "frame capture halted\n"));
					d->dma_enable = 0;
				}
				break;
			case FRAME_DONE:
				/* drop on the floor (must be waiting for sw) */
				DBG(DBG_FRAMING,
				    printk(KERN_DEBUG "frame capture halted\n"));
				d->dma_enable = 0;
				break;
			}
		} else {
			switch (d->frame[d->hwframe].state) {
			case FRAME_UNUSED:
				DBG(DBG_FRAMING,
				    printk(KERN_ERR "capture to unused frame %d\n",
					   d->hwframe));
				break;
			case FRAME_READY:
				/* drop on the floor (must have dropped something) */
				DBG(DBG_FRAMING_LOUD,
				    printk(KERN_DEBUG "missed SOF\n"));
				break;
			case FRAME_DONE:
				/* drop on the floor (must be waiting for sw) */
				DBG(DBG_FRAMING,
				    printk(KERN_DEBUG "frame overflow\n"));
				d->dma_enable = 0;
				break;
			case FRAME_GRABBING:
#if DMA_DEINTERLACE
				if (eav_val == 0xb6) {
					d->frame[d->hwframe].pos = d->frame[d->hwframe].data;
				}
				memcpy(d->frame[d->hwframe].pos, &buf[6], len);
				if (!d->interlaced)
					memcpy(d->frame[d->hwframe].pos-len, &buf[6], len);
				d->frame[d->hwframe].pos += len*2;
#else
				memcpy(d->frame[d->hwframe].pos, &buf[6], len);
				d->frame[d->hwframe].pos += len;
#endif
				/* XXXKW check pos overflow */
				break;
			}
		}
	}
	
	if (d->dma_enable) {
		out64(delta, MAC2_DMARX0_CSR(R_MAC_DMA_DSCR_CNT));
		DBG(DBG_DESCR,
		    printk(KERN_DEBUG IF_NAME ": interrupt adds %d -> %d descrs\n",
			   delta, (int)in64(MAC2_DMARX0_CSR(R_MAC_DMA_DSCR_CNT))));
	}
}

/* -----------------------------------------------------------------------
 * /dev/video interface
 * ----------------------------------------------------------------------- */

static int saa7114h_open(struct video_device *vd, int nb)
{
	struct saa7114h *d = vd->priv;
	uint32_t status;

	if (!d || d->opened)
		return -EBUSY;

	d->opened = 1;
	DBG(DBG_CALL, printk(KERN_DEBUG IF_NAME ": open\n"));

	/* XXKW Should check this periodically!? */
	status = saa7114h_reg_read(d, DECODER_STATUS);
	d->interlaced = ((status & 0x80) != 0);

#if !NULL_DMA
	if (d->dma_enable) {
		printk(IF_NAME ": open found DMA on?!\n");
#if LAZY_READ
	}
#else
	} else {
		int descr;
		d->dma_enable = 1;
		DBG(DBG_DESCR, printk(IF_NAME ": open enabling DMA\n"));
		/* Force capture to start into frame buffer 0 */
		descr = in64(MAC2_DMARX0_CSR(R_MAC_DMA_DSCR_CNT));
		DBG(DBG_DESCR,
		    printk(IF_NAME ": open adds %d -> %d descrs\n",
			   d->ff.ringsz-desc, descr));
		out64(d->ff.ringsz-descr, MAC2_DMARX0_CSR(R_MAC_DMA_DSCR_CNT));
	}
#endif
#endif

	return 0;
}

static void saa7114h_release(struct video_device *vd)
{
	struct saa7114h *d = vd->priv;

	DBG(DBG_CALL, printk(KERN_DEBUG IF_NAME ": release\n"));
	d->opened = 0;
	d->dma_enable = 0;

	/* XXXKW do a clean drain of outstanding DMAs? toss leftover
	   buffer contents to avoid stale pictures? */

	return;
}

static long saa7114h_read(struct video_device *vd, char *buf,
			  unsigned long count, int noblock)
{
	struct saa7114h *d = vd->priv;
	int descr, status;

	if (!d)
		return -ENODEV;

	/* XXKW Should check this periodically!? */
	status = saa7114h_reg_read(d, DECODER_STATUS);
//	  d->interlaced = ((status & 0x80) != 0);
	
#if !NULL_DMA
#if LAZY_READ
	if (!d->dma_enable) {
		DBG(DBG_DESCR, printk(KERN_DEBUG IF_NAME ": enabling DMA\n"));
		/* Give the buffer to the DMA engine (force ptr reset) */
		d->swframe = d->hwframe;
		d->frame[d->swframe].state = FRAME_READY;
#if DMA_DEINTERLACE
		d->frame[d->swframe].pos = d->frame[d->swframe].data+d->vw.width*RAW_PER_PIXEL;
#else
		d->frame[d->swframe].pos = d->frame[d->swframe].data;
#endif
		/* Fire up the DMA engine again if it stopped */
		d->dma_enable = 1;
		descr = in64(MAC2_DMARX0_CSR(R_MAC_DMA_DSCR_CNT));
		out64(d->ff.ringsz-descr, MAC2_DMARX0_CSR(R_MAC_DMA_DSCR_CNT));
	}
#endif
#endif

	/* XXXKW mmap/read mixture could break the swframe sequence */

	if (d->frame[d->swframe].state != FRAME_DONE) {
		if (noblock)
			return -EAGAIN;
		else {
			DBG(DBG_IO,
			    printk(KERN_DEBUG IF_NAME ": sleeping for frame\n"));
			interruptible_sleep_on(&d->frame[d->swframe].read_wait);
			DBG(DBG_IO,
			    printk(KERN_DEBUG IF_NAME ": awakened\n"));
			if (signal_pending(current))
				return -ERESTARTSYS;
		}
	}

	if (count < d->frame[d->swframe].size)
		return -EFAULT;

	count = d->frame[d->swframe].size;
	yuvconvert_inplace(d->frame[d->swframe].data, d->frame[d->swframe].size, d->vp.palette, 0);
	copy_to_user(buf, d->frame[d->swframe].data, d->frame[d->swframe].size);
	d->swframe = (d->swframe + 1) % NUM_FRAME;
	/* XXXKW doesn't do format conversion!!! */
#if !NULL_DMA
#if !LAZY_READ
	/* XXXKW Fire up the DMA engine again if it stopped ??? */
	if (!d->dma_enable) {
		DBG(DBG_DESCR, printk(KERN_DEBUG IF_NAME ": enabling DMA\n"));
		/* Fire up the DMA engine again if it stopped */
		d->dma_enable = 1;
		descr = in64(MAC2_DMARX0_CSR(R_MAC_DMA_DSCR_CNT));
		out64(d->ff.ringsz-descr, MAC2_DMARX0_CSR(R_MAC_DMA_DSCR_CNT));
	}
#endif
#endif

	return count;
}

static int saa7114h_ioctl(struct video_device *vd, unsigned int cmd, void *arg)
{
	struct saa7114h *d = vd->priv;
	int val, reg, retval = 0;

	if (!d)
		return -ENODEV;

	switch (cmd) {
	case VIDIOCGCHAN:
	{
		struct video_channel v;

		if (copy_from_user(&v, arg, sizeof(v))) {
			retval = -EFAULT;
			break;
		}
		if (v.channel != 0) {
			retval = -EINVAL;
			break;
		}

		v.channel = 0;
		strcpy(v.name, "Camera");
		v.tuners = 0;
		v.flags = 0;
		v.type = VIDEO_TYPE_CAMERA;
		v.norm = 0;

		if (copy_to_user(arg, &v, sizeof(v)))
			retval = -EFAULT;
		break;
	}
	
	case VIDIOCSCHAN:
	{
		int v;

		if (copy_from_user(&v, arg, sizeof(v)))
			retval = -EFAULT;

		if (retval == 0 && v != 0)
			retval = -EINVAL;

		break;
	}

	case VIDIOCGCAP:
	{
		struct video_capability b;

		strcpy(b.name, "Philips SAA7114H Decoder");
		b.type = VID_TYPE_CAPTURE /* | VID_TYPE_TELETEXT */ | VID_TYPE_SCALES;
		b.channels = 1;
		b.audios = 0;
		b.maxwidth = MAX_HORIZ;
		b.maxheight = MAX_VERT;
		/* XXXKW find real values */
		b.minwidth = 48;
		b.minheight = 48;

		if (copy_to_user(arg, &b, sizeof(b)))
			retval = -EFAULT;

		break;
	}

	/* image properties */
	case VIDIOCGPICT:
		if (copy_to_user(arg, &d->vp, sizeof(struct video_picture)))
			retval = -EFAULT;
		break;
		
	case VIDIOCSPICT:
	{
		struct video_picture vp;

		/* copy_from_user */
		if (copy_from_user(&vp, arg, sizeof(vp))) {
			retval = -EFAULT;
			break;
		}

		down(&d->param_lock);
		/* brightness, colour, contrast need not check 0-65535 */
		memcpy( &d->vp, &vp, sizeof(vp) );
		/* update cam->params.colourParams */
		saa7114h_set_cparams(d);
		up(&d->param_lock);
		break;
	}

	/* get/set capture window */
	case VIDIOCGWIN:
		if (copy_to_user(arg, &d->vw, sizeof(struct video_window)))
			retval = -EFAULT;
		break;
	
	case VIDIOCSWIN:
	{
		/* copy_from_user, check validity, copy to internal structure */
		struct video_window vw;
		if (copy_from_user(&vw, arg, sizeof(vw))) {
			retval = -EFAULT;
			break;
		}

		if (vw.clipcount != 0) {    /* clipping not supported */
			retval = -EINVAL;
			break;
		}
		if (vw.clips != NULL) {	    /* clipping not supported */
			retval = -EINVAL;
			break;
		}
		if ((vw.width > MAX_HORIZ || vw.width < MIN_HORIZ) ||
		    (vw.height > MAX_VERT || vw.height < MIN_VERT)) {
			retval = -EINVAL;
			break;
		}

		/* we set the video window to something smaller or equal to what
		 * is requested by the user???
		 */
		down(&d->param_lock);
		if (vw.width != d->vw.width || vw.height != d->vw.height) {
			uint32_t scale_factor;
			/* XXXKW base percentage on input stream, not MAX? */

			/* Assert scaler reset */
			saa7114h_reg_write(d, 0x88, 0x98);

			/* Vertical scaling */
			scale_factor = (MAX_VERT*1024) / vw.height;
			saa7114h_reg_write(d, 0x9e, vw.height & 0xff);
			saa7114h_reg_write(d, 0x9f, (vw.height >> 8) & 0xf);
			saa7114h_reg_write(d, 0xb0, scale_factor & 0xff);
			saa7114h_reg_write(d, 0xb1, (scale_factor >> 8) & 0xff);
			saa7114h_reg_write(d, 0xb2, scale_factor & 0xff);
			saa7114h_reg_write(d, 0xb3, (scale_factor >> 8) & 0xff);
			/* Horizontal scaling */
			scale_factor = (MAX_HORIZ*1024) / vw.width;
			saa7114h_reg_write(d, 0x9c, vw.width & 0xff);
			saa7114h_reg_write(d, 0x9d, (vw.width >> 8) & 0xf);
			saa7114h_reg_write(d, 0xa8, scale_factor & 0xff);
			saa7114h_reg_write(d, 0xa9, (scale_factor >> 8) & 0xff);
			saa7114h_reg_write(d, 0xac, (scale_factor >> 1) & 0xff);
			saa7114h_reg_write(d, 0xad, (scale_factor >> 9) & 0xff);
#if 0
			/* prescaler
			saa7114h_reg_write(d, 0xa0, 2);
			saa7114h_reg_write(d, 0xa1, 1);
			saa7114h_reg_write(d, 0xa2, 1);
			*/
#endif

			/* Release scaler reset */
			saa7114h_reg_write(d, 0x88, 0xb8);
			d->vw.width = vw.width;
			d->vw.height = vw.height;
		}
		up(&d->param_lock);
		break;
	}

	/* mmap interface */
	case VIDIOCGMBUF:
	{
		struct video_mbuf vm;
		int i;

		memset(&vm, 0, sizeof(vm));
		vm.size = MAX_FRAME_SIZE*NUM_FRAME;
		vm.frames = NUM_FRAME;
		for (i = 0; i < NUM_FRAME; i++)
			vm.offsets[i] = MAX_FRAME_SIZE * i;

		if (copy_to_user((void *)arg, (void *)&vm, sizeof(vm)))
			retval = -EFAULT;

		break;
	}

	case VIDIOCMCAPTURE:
	{
		struct video_mmap vm;
		int descr, status;

		if (copy_from_user((void *)&vm, (void *)arg, sizeof(vm))) {
			retval = -EFAULT;
			break;
		}
		if (vm.frame<0||vm.frame>NUM_FRAME) {
			retval = -EINVAL;
			break;
		}

		DBG(DBG_CALL,
		    printk(KERN_DEBUG IF_NAME ":ioctl MCAPTURE %d\n", vm.frame));

		d->vp.palette = vm.format;
		/* XXXKW set depth? */
		/* XXXKW match/update for vm.width, vm.height */

		/* XXKW Should check this periodically!? */
		status = saa7114h_reg_read(d, DECODER_STATUS);
//		  d->interlaced = ((status & 0x80) != 0);

		/* Give the buffer to the DMA engine */
		/* XXXKW vm.frame vs d->swframe!!  mmap/read mismatch */
#if DMA_DEINTERLACE
		d->frame[vm.frame].pos = d->frame[vm.frame].data + d->vw.width*RAW_PER_PIXEL;
#else
		d->frame[vm.frame].pos = d->frame[vm.frame].data;
#endif
#if !NULL_DMA
		d->frame[vm.frame].state = FRAME_READY;
		/* Fire up the DMA engine again if it stopped */
		if (!d->dma_enable) {
			d->dma_enable = 1;
			d->hwframe = d->swframe = vm.frame;
			descr = in64(MAC2_DMARX0_CSR(R_MAC_DMA_DSCR_CNT));
			DBG(DBG_DESCR,
			    printk(KERN_DEBUG IF_NAME ": capture adds %d -> %d descrs\n",
				   d->ff.ringsz-descr, descr));
			out64(d->ff.ringsz-descr, MAC2_DMARX0_CSR(R_MAC_DMA_DSCR_CNT));
		}
#endif
		break;
	}

	case VIDIOCSYNC:
	{
		int frame;

		if (copy_from_user((void *)&frame, arg, sizeof(int))) {
			retval = -EFAULT;
			break;
		}

		if (frame<0 || frame >= NUM_FRAME) {
			retval = -EINVAL;
			break;
		}

		DBG(DBG_CALL, printk(KERN_DEBUG IF_NAME ":ioctl CSYNC %d\n", frame));

		switch (d->frame[frame].state) {
		case FRAME_UNUSED:
			DBG(DBG_IO,
			    printk(KERN_ERR IF_NAME ":sync to unused frame %d\n", frame));
			retval = -EINVAL;
			break;

		case FRAME_READY:
		case FRAME_GRABBING:
			DBG(DBG_IO,
			    printk(KERN_DEBUG IF_NAME ": sleeping for frame %d\n", frame));
			interruptible_sleep_on(&d->frame[frame].read_wait);
			DBG(DBG_IO,
			    printk(KERN_DEBUG IF_NAME ": awakened\n"));
			if (signal_pending(current))
				return -ERESTARTSYS;
		case FRAME_DONE:
#if !NULL_DMA
			yuvconvert_inplace(d->frame[frame].data,
					   d->frame[frame].size,
					   d->vp.palette, 1);
			d->frame[frame].state = FRAME_UNUSED;
#endif
			DBG(DBG_IO,
			    printk(KERN_DEBUG IF_NAME ": sync finished %d\n",
				   frame));
			break;
		}
		break;
	}

	case VIDIOREADREG:
		reg = *(int *)arg;
		DBG(DBG_REGISTER, printk(KERN_DEBUG IF_NAME ": read of %02x\n", reg));
		if ((reg > 0xEF) || (reg < 0))
			return -EINVAL;
		val = saa7114h_reg_read((struct saa7114h *)vd->priv, reg);
		if (val == -1)
			return -EIO;
		*(int *)arg = val;
		break;
	case VIDIOWRITEREG:
		if (copy_from_user(&reg, arg, sizeof(int)) ||
		    copy_from_user(&val, arg+sizeof(int), sizeof(int)))
			return -EFAULT;
		DBG(DBG_REGISTER, printk(KERN_DEBUG IF_NAME ": write of %02x <- %02x\n", reg, val));
		if ((reg > 0xEF) || (reg < 0))
			return -EINVAL;
		val = saa7114h_reg_write((struct saa7114h *)vd->priv, reg, val);
		if (val == -1)
			return -EIO;
		break;
	case VIDIOGRABFRAME:
		return grab_frame((struct saa7114h *)vd->priv, arg, 0);
	case VIDIOSHOWEAV:
		return grab_frame((struct saa7114h *)vd->priv, arg, 1);
	default:
		retval = -EINVAL;
		break;
	}

	return retval;
}

static int saa7114h_mmap(struct video_device *vd, const char *adr,
			 unsigned long size)
{
	struct saa7114h *d = vd->priv;
	unsigned long start = (unsigned long)adr;
	unsigned long page, pos;

	if (!d)
		return -ENODEV;

	if (size > MAX_MMAP_SIZE) {
		printk("mmap: bad size %lu > %lu\n", size, MAX_MMAP_SIZE);
		return -EINVAL;
	}

	/* make this _really_ smp-safe */
	if (down_interruptible(&d->busy_lock))
		return -EINTR;

	pos = (unsigned long)(d->frame_buf);
	while (size > 0) {
		page = kvirt_to_pa(pos);
		if (remap_page_range(start, page, PAGE_SIZE, PAGE_SHARED)) {
			up(&d->busy_lock);
			return -EAGAIN;
		}
		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}
	up(&d->busy_lock);

	return 0;
}

/* -----------------------------------------------------------------------
 * Device probing and initialization
 * ----------------------------------------------------------------------- */

/* Default values to program into SAA7114H */
static const unsigned char reg_init[] =	{
	0x00, 0x00,	/* 00 - ID byte */

	/*front end */
	0x01, 0x08,	/* 01 - Horizontal increment -> recommended delay */
	0x02, 0xC4,	/* 02 - AI Control 1 (CVBS AI23) */
	0x03, 0x10,	/* 03 - AI Control 2 */
	0x04, 0x90,	/* 04 - AI Control 3 (Gain ch 1) */
	0x05, 0x90,	/* 05 - AI Control 4 (Gain ch 2) */
	
	/* decoder */
	0x06, 0xEB,	/* 06 - Horiz sync start */
	0x07, 0xE0,	/* 07 - Horiz sync stop */
	0x08, 0x98,	/* 08 - Sync control */
	0x09, 0x40,	/* 09 - L Control */
	0x0a, 0x80,	/* 0a - L Brightness */
	0x0b, 0x44,	/* 0b - L Contrast */
	0x0c, 0x40,	/* 0c - C Saturation */
	0x0d, 0x00,	/* 0d - C Hue */
	0x0e, 0x89,	/* 0e - C Control 1 */
	0x0f, 0x0f,	/* 0f - C Gain (??? 0x2A recommended) */
	0x10, 0x0E,	/* 10 - C Control 2 */
	0x11, 0x00,	/* 11 - Mode/Delay */
	0x12, 0x00,	/* 12 - RT signal control */
	0x13, 0x00,	/* 13 - RT/X output */
	0x14, 0x00,	/* 14 - Analog, Compat */
	0x15, 0x11,	/* 15 - VGATE start */
	0x16, 0xFE,	/* 16 - VGATE stop */
	0x17, 0x40,	/* 17 - Misc VGATE (disable LLC2) */
	0x18, 0x40,	/* 18 - Raw data gain - 128 */
	0x19, 0x80,	/* 19 - Raw data offset - 0 */

	/* Global settings */
	0x88, 0x98,	/* 88 - AI1x on, AI2x off; decoder/slicer off; ACLK gen off */
	0x83, 0x00,	/* 83 - X-port output disabled */
	0x84, 0xF0,	/* 84 - I-port V/G output framing, IGP1=0=IGP0=0 */
	0x85, 0x00,	/* 85 - I-port default polarities, X-port signals */
	0x86, 0x40,	/* 86 - more IGP1/0, FIFO level, only video transmitted */
	0x87, 0x01,	/* 87 - ICK default, IDQ default, I-port output enabled */

	/* Task A: scaler input config and output format */
	0x90, 0x00,	/* 90 - Task handling */
	0x91, 0x08,	/* 91 - Scalar input and format */
	0x92, 0x10,	/* 92 - Reference signal def */
	0x93, 0x80,	/* 93 - I-port output */

	/* Task B */
	0xc0, 0x42,	/* 90 - Task handling */
	0xc1, 0x08,	/* 91 - Scalar input and format */
	0xc2, 0x10,	/* 92 - Reference signal def */
	0xc3, 0x80,	/* 93 - I-port output */

	/* Input and Output windows */
	0x94, 0x10,	/*  - */
	0x95, 0x00,	/*  - */
	0x96, 0xD0,	/*  - */
	0x97, 0x02,	/*  - */
	0x98, 0x0A,	/*  - */
	0x99, 0x00,	/*  - */
	0x9a, 0xF2,	/*  - */
	0x9b, 0x00,	/*  - */
	0x9c, 0xD0,	/*  - */
	0x9d, 0x02,	/*  - */
	0xc4, 0x10,	/*  - */
	0xc5, 0x00,	/*  - */
	0xc6, 0xD0,	/*  - */
	0xc7, 0x02,	/*  - */
	0xc8, 0x0A,	/*  - */
	0xc9, 0x00,	/*  - */
	0xca, 0xF2,	/*  - */
	0xcb, 0x00,	/*  - */
	0xcc, 0xD0,	/*  - */
	0xcd, 0x02,	/*  - */

	0x9e, 0xf0,	/*  - */
	0x9f, 0x00,	/*  - */
	0xce, 0xf0,	/*  - */
	0xcf, 0x00,	/*  - */

	/* Prefiltering and prescaling */
	0xa0, 0x01,	/*  - */
	0xa1, 0x00,	/*  - */
	0xa2, 0x00,	/*  - */
	0xa4, 0x80,	/*  - */
	0xa5, 0x40,	/*  - */
	0xa6, 0x40,	/*  - */
	0xd4, 0x80,	/*  - */
	0xd5, 0x40,	/*  - */
	0xd6, 0x40,	/*  - */

	/* Horizontal phase scaling */
	0xa8, 0x00,	/*  - */
	0xa9, 0x04,	/*  - */
	0xaa, 0x00,	/*  - */
	0xd8, 0x00,	/*  - */
	0xd9, 0x04,	/*  - */
	0xda, 0x00,	/*  - */

	0xac, 0x00,	/*  - */
	0xad, 0x02,	/*  - */
	0xae, 0x00,	/*  - */
	0xdc, 0x00,	/*  - */
	0xdd, 0x02,	/*  - */
	0xde, 0x00,	/*  - */

	/* Vertical phase scaling */
	0xb0, 0x00,	/*  - */
	0xb1, 0x04,	/*  - */
	0xb2, 0x00,	/*  - */
	0xb3, 0x04,	/*  - */
	0xe0, 0x00,	/*  - */
	0xe1, 0x04,	/*  - */
	0xe2, 0x00,	/*  - */
	0xe3, 0x04,	/*  - */
	0xb4, 0x00,	/* b4 - vscale mode control */
	0xe4, 0x00,	/* b4 - vscale mode control */

	/* Task enables */
	0x80, 0x10,	/* 80 - LLC->ICLK, dq->IDQ, scaler->F/V timing, task enables */

	/* Reset the slicer */
	0x88, 0xb8,	/* 88 - AI1x on, AI2x off; decoder/slicer on; ACLK gen off */
};

static int saa7114h_attach(struct i2c_adapter *adap, int addr, unsigned short flags, int kind)
{
	struct i2c_client *client;
	struct video_device *vd;
	struct saa7114h *decoder;
	int err;
	int val, i;

	client = kmalloc(sizeof(*client), GFP_KERNEL);
	if (client == NULL)
		return -ENOMEM;
	client->adapter = adap;
	client->addr = addr;
	client->driver = &i2c_driver_saa7114h;
	strcpy(client->name, IF_NAME);

	decoder = kmalloc(sizeof(*decoder), GFP_KERNEL);
	if (decoder == NULL) {
		kfree(client);
		return -ENOMEM;
	}
	memset(decoder, 0, sizeof(struct saa7114h));
	decoder->client = client;
	decoder->dma_enable = 0;
	decoder->palette = VIDEO_PALETTE_UYVY;
	decoder->depth = 16;
	decoder->vw.width = MAX_HORIZ;
	decoder->vw.height = MAX_VERT;
	decoder->frame_buf = rvmalloc(MAX_FRAME_SIZE*NUM_FRAME);
	if (!decoder->frame_buf) {
		kfree(decoder);
		kfree(client);
		return -ENOMEM;
	}
	/* XXXKW use clear_page? */
	memset(decoder->frame_buf, 0, MAX_FRAME_SIZE*NUM_FRAME);
	printk("saa7114h_attach: frame_buf = (fb=%8p / %08lx)\n",
	       decoder->frame_buf, kvirt_to_pa((int)decoder->frame_buf));
	for (i=0; i<NUM_FRAME; i++) {
		decoder->frame[i].data = decoder->frame_buf+i*MAX_FRAME_SIZE;
#if NULL_DMA
		decoder->frame[i].state = FRAME_DONE;
#else
		decoder->frame[i].state = FRAME_UNUSED;
#endif
		init_waitqueue_head(&decoder->frame[i].read_wait);
	}
	decoder->irq = K_INT_MAC_2;
	if (request_irq
	    (decoder->irq, saa7114h_interrupt, 0, "Philips SAA7114h", decoder)) {
		rvfree(decoder->frame_buf, MAX_FRAME_SIZE*NUM_FRAME);
		kfree(decoder);
		kfree(client);
		return -ENOMEM;
	}
	init_MUTEX(&decoder->param_lock);
	init_MUTEX(&decoder->busy_lock);

	if ((err = i2c_attach_client(client)) < 0) {
		kfree(client);
		kfree(decoder);
		return err;
	}

	if (saa7114h_reg_init(decoder, reg_init, sizeof(reg_init)) ||
	    saa7114h_get_cparams(decoder)) {
		i2c_detach_client(client);
		kfree(client);
		kfree(decoder);
		return -ENODEV;
	}

	vd = kmalloc(sizeof(*vd), GFP_KERNEL);
	memset(vd, 0, sizeof(*vd));
	if (vd == NULL) {
		i2c_detach_client(client);
		kfree(client);
		kfree(decoder);
		return -ENOMEM;
	}
	vd->priv = decoder;
	strcpy(vd->name, IF_NAME);
	vd->type = VID_TYPE_CAPTURE;
	vd->hardware = VID_HARDWARE_SAA7114H;
	vd->open =  saa7114h_open;
	vd->close = saa7114h_release;
	vd->read =  saa7114h_read;
	vd->ioctl = saa7114h_ioctl;
	vd->mmap =  saa7114h_mmap;

	if ((err = video_register_device(vd, VFL_TYPE_GRABBER, -1)) < 0) {
		i2c_detach_client(client);
		kfree(client);
		kfree(decoder);
		kfree(vd);
		return err;
	}

	client->data = vd;
	decoder->vd = vd;

	/* Turn on the ITRDY - preserve the GENO pin for syncser */
	val = in64(KSEG1 + A_MAC_REGISTER(2, R_MAC_MDIO));
	out64(M_MAC_MDIO_OUT | (val & M_MAC_GENC),
	      KSEG1 + A_MAC_REGISTER(2, R_MAC_MDIO));

	if ((err = dma_setup(decoder))) {
		i2c_detach_client(client);
		kfree(client);
		kfree(decoder);
		kfree(vd);
		return err;
	}

	printk("saa7114h_attach successful\n");

#ifdef CONFIG_PROC_FS
	proc_saa7114h_create();
	create_proc_decoder(vd->priv);
#endif

	MOD_INC_USE_COUNT;

	return 0;
}

/* Addresses to scan */
static unsigned short normal_i2c[] = {I2C_CLIENT_END};
static unsigned short normal_i2c_range[] = {0x20, 0x21, I2C_CLIENT_END};
static unsigned short probe[2]	      = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short probe_range[2]  = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short ignore[2]	      = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short ignore_range[2] = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short force[2]	      = { I2C_CLIENT_END, I2C_CLIENT_END };

static struct i2c_client_address_data addr_data = {
	normal_i2c, normal_i2c_range,
	probe, probe_range,
	ignore, ignore_range,
	force
};

static int saa7114h_probe(struct i2c_adapter *adap)
{
	/* Look for this device on the given adapter (bus) */
	if (adap->id == (I2C_ALGO_SIBYTE | I2C_HW_SIBYTE))
		return i2c_probe(adap, &addr_data, &saa7114h_attach);
	else
		return 0;
}

static int saa7114h_detach(struct i2c_client *device)
{
#if 0
	kfree(device->data);
	MOD_DEC_USE_COUNT;
#endif
#ifdef CONFIG_PROC_FS
	destroy_proc_decoder(((struct video_device *)device->data)->priv);
	proc_saa7114h_destroy();
#endif
	return 0;
}

/* ----------------------------------------------------------------------- */

static int __init swarm_7114h_init(void)
{
	return i2c_add_driver(&i2c_driver_saa7114h);
}

static void __exit swarm_7114h_cleanup(void)
{
}

MODULE_AUTHOR("Kip Walker, Broadcom Corp.");
MODULE_DESCRIPTION("Philips SAA7114H Driver for Broadcom SWARM board");

module_init(swarm_7114h_init);
module_exit(swarm_7114h_cleanup);
