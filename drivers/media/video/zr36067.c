#define DEBUGLEVEL 0
#define MAX_KMALLOC_MEM (128*1024)

/*
   Miro/Pinnacle Systems Inc. DC10/DC10plus and
   Linux Media Labs LML33 video capture boards driver
   now with IOMega BUZ support!
   
   Copyright (C) 2000 Serguei Miridonov <mirsev@cicese.mx>

   Changes for BUZ by Wolfgang Scherr <scherr@net4you.net>
   
   Based on
   
    Miro DC10 driver
    Copyright (C) 1999 Wolfgang Scherr <scherr@net4you.net>
   
    Iomega Buz driver version 1.0
    Copyright (C) 1999 Rainer Johanni <Rainer@Johanni.de>

    buz.0.0.3
    Copyright (C) 1998 Dave Perks <dperks@ibm.net>

    bttv - Bt848 frame grabber driver
    Copyright (C) 1996,97,98 Ralph  Metzler (rjkm@thp.uni-koeln.de)
                           & Marcus Metzler (mocm@thp.uni-koeln.de)

    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/config.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/pci.h>
#include <linux/signal.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <linux/sched.h>
#include <asm/segment.h>
#include <linux/types.h>
#include <linux/wrapper.h>

#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/i2c-old.h>
#define     MAP_NR(x)       virt_to_page(x)
#define     ZORAN_HARDWARE  VID_HARDWARE_ZR36067

#include <linux/videodev.h>

#include <asm/uaccess.h>
#include <linux/proc_fs.h>

#include "zoran.h"
#include <linux/video_decoder.h>
#include <linux/video_encoder.h>

// RJ: Test only - want to test BUZ_USE_HIMEM even when CONFIG_BIGPHYS_AREA is defined
#if !defined(CONFIG_BIGPHYS_AREA)
//#undef CONFIG_BIGPHYS_AREA
#define BUZ_USE_HIMEM
#endif

#if defined(CONFIG_BIGPHYS_AREA)
#   include <linux/bigphysarea.h>
#endif

#define IRQ_MASK ( ZR36057_ISR_GIRQ0 | ZR36057_ISR_GIRQ1 | /* ZR36057_ISR_CodRepIRQ | */  ZR36057_ISR_JPEGRepIRQ )	// SW
//#define GPIO_MASK 0xcd
//#define GPIO_MASK 0x8d

/*
DC10:
GPIO0 = /RESET ZR 36060
GPIO1 = VIDEO BUS DIRECTION (0: CAPTURE, 1: DISPLAY)
GPIO2 = VIDEO BUS ENABLE    (0: ON, 1: OFF)
GPIO3 = /SLEEP ZR 36060
GPIO4 = ADC7175 (video out) FREQUENCY (0: LCC/SAA7110, 1: 27 MHz (quarz))
GPIO5 = ZORAN FREQUENCY  (0: LCC and LCC2 from SAA7110,
                          1: 27 and 13.5 MHz (quarz))
GPIO6 = /FRAME ZR 36060
GPIO7 = /RESET ADV7175 (video out)
(I think they lost the SAA7110 reset.....)

GIRQ0 signals that ZR36060's DATERR# line is asserted.
GIRQ1 signals a vertical sync of the video signal (VS SAA7110)

  SAA7110A:
   mode 0 - Composite
   mode 1 - 
   mode 2 - 
   mode 3 - 
   mode 4 - 
   mode 5 - internal Composite (from PCTV)
   mode 6 - 
   mode 7 - S-Video

BUZ:
GPIO0 = 1, take board out of reset
GPIO1 = 1, take JPEG codec out of sleep mode
GPIO3 = 1, deassert FRAME# to 36060

GIRQ0 signals a vertical sync of the video signal
GIRQ1 signals that ZR36060's DATERR# line is asserted.

SAA7111A

   In their infinite wisdom, the Iomega engineers decided to
   use the same input line for composite and S-Video Color,
   although there are two entries not connected at all!
   Through this ingenious strike, it is not possible to
   keep two running video sources connected at the same time
   to Composite and S-VHS input!

   mode 0 - N/C
   mode 1 - S-Video Y
   mode 2 - noise or something I don't know
   mode 3 - Composite and S-Video C
   mode 4 - N/C
   mode 5 - S-Video (gain C independently selectable of gain Y)
   mode 6 - N/C
   mode 7 - S-Video (gain C adapted to gain Y)
*/

#define MAJOR_VERSION 0		/* driver major version */
#define MINOR_VERSION 7		/* driver minor version */

#define ZORAN_NAME    "zr36067"	/* name of the device */

#define BUZ_ERR       KERN_ERR     ZORAN_NAME
#define BUZ_DEBUG     KERN_INFO    ZORAN_NAME
#define BUZ_INFO      KERN_INFO    ZORAN_NAME
#define BUZ_WARNING   KERN_WARNING ZORAN_NAME

#if(DEBUGLEVEL>0)
#define DEBUG1(x...)    x
#else
#define DEBUG1(x...)
#endif

#if(DEBUGLEVEL>1)
#define DEBUG2(x...)    x
#else
#define DEBUG2(x...)
#endif

#if(DEBUGLEVEL>2)
#define DEBUG3(x...)    x
#else
#define DEBUG3(x...)
#endif

#if(DEBUGLEVEL>3)
#define DEBUG4(x...)    x
#else
#define DEBUG4(x...)
#endif

/* The parameters for this driver */

/*
   The video mem address of the video card.
   The driver has a little database for some videocards
   to determine it from there. If your video card is not in there
   you have either to give it to the driver as a parameter
   or set in in a VIDIOCSFBUF ioctl
 */

static unsigned long vidmem = 0;	/* Video memory base address */

/* Special purposes only: */

static int triton = 0;		/* 0=no, 1=yes */
static int natoma = 0;		/* 0=no, 1=yes */

/*
   Number and size of grab buffers for Video 4 Linux
   The vast majority of applications should not need more than 2,
   the very popular BTTV driver actually does ONLY have 2.
   Time sensitive applications might need more, the maximum
   is VIDEO_MAX_FRAME (defined in <linux/videodev.h>).

   The size is set so that the maximum possible request
   can be satisfied. Decrease  it, if bigphys_area alloc'd
   memory is low. If you don't have the bigphys_area patch,
   set it to 128 KB. Will you allow only to grab small
   images with V4L, but that's better than nothing.

   v4l_bufsize has to be given in KB !

*/

static int v4l_nbufs = 2;
static int v4l_bufsize = 128;	/* Everybody should be able to work with this setting */

/*
   Default input and video norm at startup of the driver.
*/

static int default_input = 0;	/* 0=Composite, 1=S-VHS */
static int default_norm = 0;	/* 0=PAL, 1=NTSC 2=SECAM */
static int lock_norm = 0;	/* 1=Don't change TV standard (norm) */

static int pass_through = 0;	/* 1=Pass through TV signal when device is not used */
				/* 0=Show color bar when device is not used (LML33: only if lml33dpath=1) */

static int lml33dpath = 0;	/* 1 will use digital path in capture mode instead of analog.
				   It can be used for picture adjustments using tool like xawtv
				   while watching image on TV monitor connected to the output.
				   However, due to absence of 75 Ohm load on Bt819 input, there
				   will be some image imperfections */
static int video_nr = -1;

MODULE_PARM(vidmem, "i");
MODULE_PARM(triton, "i");
MODULE_PARM(natoma, "i");
MODULE_PARM(v4l_nbufs, "i");
MODULE_PARM(v4l_bufsize, "i");
MODULE_PARM(default_input, "i");
MODULE_PARM(default_norm, "i");
MODULE_PARM(lock_norm, "i");
MODULE_PARM(pass_through, "i");
MODULE_PARM(lml33dpath, "i");
MODULE_PARM(video_nr, "i");

static struct pci_device_id zr36067_pci_tbl[] = {
	{ PCI_VENDOR_ID_ZORAN, PCI_DEVICE_ID_ZORAN_36057, 
	  PCI_ANY_ID, PCI_ANY_ID,  0, 0, 0 },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, zr36067_pci_tbl);

/* Anybody who uses more than four? */
#define BUZ_MAX 4

static int zoran_num;		/* number of Buzs in use */
static struct zoran zoran[BUZ_MAX];

/* forward references */

static void v4l_fbuffer_free(struct zoran *zr);
static void jpg_fbuffer_free(struct zoran *zr);
static void zoran_feed_stat_com(struct zoran *zr);

/*
 *   Allocate the V4L grab buffers
 *
 *   These have to be pysically contiguous.
 *   If v4l_bufsize <= MAX_KMALLOC_MEM we use kmalloc
 *   else we try to allocate them with bigphysarea_alloc_pages
 *   if the bigphysarea patch is present in the kernel,
 *   else we try to use high memory (if the user has bootet
 *   Linux with the necessary memory left over).
 */

static int v4l_fbuffer_alloc(struct zoran *zr)
{
	int i, off;
	unsigned char *mem;

	for (i = 0; i < v4l_nbufs; i++) {
		if (zr->v4l_gbuf[i].fbuffer)
			printk(KERN_WARNING
			       "%s: v4l_fbuffer_alloc: buffer %d allready allocated ???\n",
			       zr->name, i);

		//udelay(20);
		if (v4l_bufsize <= MAX_KMALLOC_MEM) {
			/* Use kmalloc */

			mem =
			    (unsigned char *) kmalloc(v4l_bufsize,
						      GFP_KERNEL);
			if (mem == 0) {
				printk(KERN_ERR
				       "%s: kmalloc for V4L bufs failed\n",
				       zr->name);
				v4l_fbuffer_free(zr);
				return -ENOBUFS;
			}
			zr->v4l_gbuf[i].fbuffer = mem;
			zr->v4l_gbuf[i].fbuffer_phys = virt_to_phys(mem);
			zr->v4l_gbuf[i].fbuffer_bus = virt_to_bus(mem);
			for (off = 0; off < v4l_bufsize; off += PAGE_SIZE)
				mem_map_reserve(MAP_NR(mem + off));
			DEBUG1(printk
			       (KERN_INFO
				"%s: V4L frame %d mem 0x%lx (bus: 0x%lx)\n",
				zr->name, i, (unsigned long) mem,
				virt_to_bus(mem)));
		} else {
#if defined(CONFIG_BIGPHYS_AREA)
			/* Use bigphysarea_alloc_pages */

			int n = (v4l_bufsize + PAGE_SIZE - 1) / PAGE_SIZE;
			mem =
			    (unsigned char *) bigphysarea_alloc_pages(n, 0,
								      GFP_KERNEL);
			if (mem == 0) {
				printk(KERN_ERR
				       "%s: bigphysarea_alloc_pages for V4L bufs failed\n",
				       zr->name);
				v4l_fbuffer_free(zr);
				return -ENOBUFS;
			}
			zr->v4l_gbuf[i].fbuffer = mem;
			zr->v4l_gbuf[i].fbuffer_phys = virt_to_phys(mem);
			zr->v4l_gbuf[i].fbuffer_bus = virt_to_bus(mem);
			DEBUG1(printk
			       (KERN_INFO
				"%s: Bigphysarea frame %d mem 0x%x (bus: 0x%x)\n",
				zr->name, i, (unsigned) mem,
				(unsigned) virt_to_bus(mem)));

			/* Zero out the allocated memory */
			memset(zr->v4l_gbuf[i].fbuffer, 0, v4l_bufsize);
#else
			/* No bigphysarea present, usage of high memory disabled,
			   but user wants buffers of more than MAX_KMALLOC_MEM */
			printk(KERN_ERR
			       "%s: No bigphysarea_patch present, usage of high memory disabled,\n",
			       zr->name);
			printk(KERN_ERR
			       "%s: sorry, could not allocate V4L buffers of size %d KB.\n",
			       zr->name, v4l_bufsize >> 10);
			return -ENOBUFS;
#endif
		}
	}

	return 0;
}

/* free the V4L grab buffers */

static void v4l_fbuffer_free(struct zoran *zr)
{
	int i, off;
	unsigned char *mem;

	for (i = 0; i < v4l_nbufs; i++) {
		if (!zr->v4l_gbuf[i].fbuffer)
			continue;

		if (v4l_bufsize <= MAX_KMALLOC_MEM) {
			mem = zr->v4l_gbuf[i].fbuffer;
			for (off = 0; off < v4l_bufsize; off += PAGE_SIZE)
				mem_map_unreserve(MAP_NR(mem + off));
			kfree((void *) zr->v4l_gbuf[i].fbuffer);
		}
#if defined(CONFIG_BIGPHYS_AREA)
		else
			bigphysarea_free_pages((void *) zr->v4l_gbuf[i].
					       fbuffer);
#endif
		zr->v4l_gbuf[i].fbuffer = NULL;
	}
}

/*
 *   Allocate the MJPEG grab buffers.
 *
 *   If the requested buffer size is smaller than MAX_KMALLOC_MEM,
 *   kmalloc is used to request a physically contiguous area,
 *   else we allocate the memory in framgents with get_free_page.
 *
 *   If a Natoma chipset is present and this is a revision 1 zr36057,
 *   each MJPEG buffer needs to be physically contiguous.
 *   (RJ: This statement is from Dave Perks' original driver,
 *   I could never check it because I have a zr36067)
 *   The driver cares about this because it reduces the buffer
 *   size to MAX_KMALLOC_MEM in that case (which forces contiguous allocation).
 *
 *   RJ: The contents grab buffers needs never be accessed in the driver.
 *       Therefore there is no need to allocate them with vmalloc in order
 *       to get a contiguous virtual memory space.
 *       I don't understand why many other drivers first allocate them with
 *       vmalloc (which uses internally also get_free_page, but delivers you
 *       virtual addresses) and then again have to make a lot of efforts
 *       to get the physical address.
 *
 */

static int jpg_fbuffer_alloc(struct zoran *zr)
{
	int i, j, off;		//alloc_contig;
	unsigned long mem;

	/* Decide if we should alloc contiguous or fragmented memory */
	/* This has to be identical in jpg_fbuffer_alloc and jpg_fbuffer_free */

	//alloc_contig = (zr->jpg_bufsize <= MAX_KMALLOC_MEM);

	for (i = 0; i < zr->jpg_nbufs; i++) {
		if (zr->jpg_gbuf[i].frag_tab)
			printk(KERN_WARNING
			       "%s: jpg_fbuffer_alloc: buffer %d allready allocated ???\n",
			       zr->name, i);

		/* Allocate fragment table for this buffer */

		mem = get_free_page(GFP_KERNEL);
		if (mem == 0) {
			printk(KERN_ERR
			       "%s: jpg_fbuffer_alloc: get_free_page (frag_tab) failed for buffer %d\n",
			       zr->name, i);
			jpg_fbuffer_free(zr);
			return -ENOBUFS;
		}
		memset((void *) mem, 0, PAGE_SIZE);
		zr->jpg_gbuf[i].frag_tab = (u32 *) mem;
		zr->jpg_gbuf[i].frag_tab_bus = virt_to_bus((void *) mem);

		//if (alloc_contig) {
		if (zr->need_contiguous) {
			mem = (unsigned long) kmalloc(zr->jpg_bufsize, GFP_KERNEL);
			if (mem == 0) {
				printk(KERN_ERR "%s: jpg_fbuffer_alloc: kmalloc failed for buffer %d\n",
				       zr->name, i);
				jpg_fbuffer_free(zr);
				return -ENOBUFS;
			}
			zr->jpg_gbuf[i].frag_tab[0] = virt_to_bus((void *) mem);
			zr->jpg_gbuf[i].frag_tab[1] =
			    ((zr->jpg_bufsize / 4) << 1) | 1;
			for (off = 0; off < zr->jpg_bufsize; off += PAGE_SIZE)
				mem_map_reserve(MAP_NR(mem + off));
		} else {
			/* jpg_bufsize is allreay page aligned */
			for (j = 0; j < zr->jpg_bufsize / PAGE_SIZE; j++) 
			{
				mem = get_free_page(GFP_KERNEL);
				if (mem == 0) {
					printk(KERN_ERR
					       "%s: jpg_fbuffer_alloc: get_free_page failed for buffer %d\n",
					       zr->name, i);
					jpg_fbuffer_free(zr);
					return -ENOBUFS;
				}

				zr->jpg_gbuf[i].frag_tab[2 * j] =
				    virt_to_bus((void *) mem);
				zr->jpg_gbuf[i].frag_tab[2 * j + 1] =
				    (PAGE_SIZE / 4) << 1;
				mem_map_reserve(MAP_NR(mem));
			}

			zr->jpg_gbuf[i].frag_tab[2 * j - 1] |= 1;
		}
	}

	DEBUG1(printk
	       ("%s: jpg_fbuffer_alloc: %ld KB allocated\n", zr->name,
		(zr->jpg_nbufs * zr->jpg_bufsize) >> 10));
	zr->jpg_buffers_allocated = 1;
	return 0;
}

/* free the MJPEG grab buffers */
static void jpg_fbuffer_free(struct zoran *zr)
{
	int i, j, off;		// alloc_contig;
	unsigned char *mem;

	/* Decide if we should alloc contiguous or fragmented memory */
	/* This has to be identical in jpg_fbuffer_alloc and jpg_fbuffer_free */

	//alloc_contig = (zr->jpg_bufsize <= MAX_KMALLOC_MEM);

	for (i = 0; i < zr->jpg_nbufs; i++) {
		if (!zr->jpg_gbuf[i].frag_tab)
			continue;

		//if (alloc_contig) {
		if (zr->need_contiguous) {
			if (zr->jpg_gbuf[i].frag_tab[0]) {
				mem =
				    (unsigned char *) bus_to_virt(zr->
								  jpg_gbuf
								  [i].
								  frag_tab
								  [0]);
				for (off = 0; off < zr->jpg_bufsize;
				     off += PAGE_SIZE)
					mem_map_unreserve(MAP_NR
							  (mem + off));
				kfree((void *) mem);
				zr->jpg_gbuf[i].frag_tab[0] = 0;
				zr->jpg_gbuf[i].frag_tab[1] = 0;
			}
		} else {
			for (j = 0; j < zr->jpg_bufsize / PAGE_SIZE; j++) {
				if (!zr->jpg_gbuf[i].frag_tab[2 * j])
					break;
				mem_map_unreserve(MAP_NR
						  (bus_to_virt
						   (zr->jpg_gbuf[i].
						    frag_tab[2 * j])));
				free_page((unsigned long)
					  bus_to_virt(zr->jpg_gbuf[i].
						      frag_tab[2 * j]));
				zr->jpg_gbuf[i].frag_tab[2 * j] = 0;
				zr->jpg_gbuf[i].frag_tab[2 * j + 1] = 0;
			}
		}

		free_page((unsigned long) zr->jpg_gbuf[i].frag_tab);
		zr->jpg_gbuf[i].frag_tab = NULL;
	}
	zr->jpg_buffers_allocated = 0;
}


/* ----------------------------------------------------------------------- */

/* I2C functions                                                           */

#define I2C_DELAY   10


/* software I2C functions */

static void i2c_setlines(struct i2c_bus *bus, int ctrl, int data)
{
	struct zoran *zr = (struct zoran *) bus->data;
	btwrite((data << 1) | ctrl, ZR36057_I2CBR);
	udelay(I2C_DELAY);
}

static int i2c_getdataline(struct i2c_bus *bus)
{
	struct zoran *zr = (struct zoran *) bus->data;
	return (btread(ZR36057_I2CBR) >> 1) & 1;
}

static void attach_inform(struct i2c_bus *bus, int id)
{
	int i;
	struct zoran *zr = (struct zoran *) bus->data;

	DEBUG1(printk(KERN_DEBUG "%s: i2c attach %02x\n", zr->name, id));
	for (i = 0; i < bus->devcount; i++) {
		if (strcmp(bus->devices[i]->name, "saa7110") == 0) {
			if (zr->revision < 2) {
				zr->card = DC10;
				sprintf(zr->name, "DC10[%u]", zr->id);
			} else {
				zr->card = DC10plus;
				sprintf(zr->name, "DC10plus[%u]", zr->id);
			}
			break;
		}
		if (strcmp(bus->devices[i]->name, "bt819") == 0) {
			zr->card = LML33;
			sprintf(zr->name, "LML33[%u]", zr->id);
			break;
		}
		if (strcmp(bus->devices[i]->name, "saa7111") == 0) {
			zr->card = BUZ;
			sprintf(zr->name, "Buz[%u]", zr->id);
			break;
		}
	}
}

static void detach_inform(struct i2c_bus *bus, int id)
{
	DEBUG1(struct zoran *zr = (struct zoran *) bus->data);
	DEBUG1(printk(KERN_DEBUG "%s: i2c detach %02x\n", zr->name, id));
}

static struct i2c_bus zoran_i2c_bus_template = {
	"zr36057",
	I2C_BUSID_BT848,
	NULL,

	SPIN_LOCK_UNLOCKED,

	attach_inform,
	detach_inform,

	i2c_setlines,
	i2c_getdataline,
	NULL,
	NULL,
};

/*
 *   Set the registers for the size we have specified. Don't bother
 *   trying to understand this without the ZR36057 manual in front of
 *   you [AC].
 *
 *   PS: The manual is free for download in .pdf format from
 *   www.zoran.com - nicely done those folks.
 */

static struct tvnorm f50sqpixel = { 944, 768, 83, 880, 625, 576, 16 };
static struct tvnorm f60sqpixel = { 780, 640, 51, 716, 525, 480, 12 };

static struct tvnorm f50ccir601 = { 864, 720, 75, 804, 625, 576, 18 };
static struct tvnorm f60ccir601 = { 858, 720, 57, 788, 525, 480, 16 };

static struct tvnorm *dc10norms[] = {
	&f50sqpixel,		/* PAL-BDGHI */
	&f60sqpixel,		/* NTSC */
	&f50sqpixel,		/* SECAM */
};

static struct tvnorm *lml33norms[] = {
	&f50ccir601,		/* PAL-BDGHI */
	&f60ccir601,		/* NTSC      */
	NULL,			/* SECAM (not supported in LML33) */
};

static struct tvnorm *buznorms[] = {
	&f50ccir601,		/* PAL-BDGHI */
	&f60ccir601,		/* NTSC      */
	NULL,			/* SECAM     */
};

static struct tvnorm *unsupported[] = {
	NULL,			/* PAL-BDGHI */
	NULL,			/* NTSC      */
	NULL,			/* SECAM     */
};

static struct tvnorm **cardnorms[] = {
	unsupported,		/* UNKNOWN  */
	dc10norms,		/* DC10     */
	dc10norms,		/* DC10plus */
	lml33norms,		/* LML33    */
	buznorms,		/* Buz      */
};

static u32 cardvsync[] = {
	0,			/* UNKNOWN  */
	ZR36057_ISR_GIRQ1,	/* DC10     */
	ZR36057_ISR_GIRQ1,	/* DC10plus */
	ZR36057_ISR_GIRQ0,	/* LML33    */
	ZR36057_ISR_GIRQ0,	/* Buz      */
};

static u32 cardjpegint[] = {
	0,			/* UNKNOWN  */
	ZR36057_ISR_GIRQ0,	/* DC10     */
	ZR36057_ISR_GIRQ0,	/* DC10plus */
	ZR36057_ISR_GIRQ1,	/* LML33    */
	ZR36057_ISR_GIRQ1,	/* Buz      */
};

static int format2bpp(int format)
{
	int bpp;

	/* Determine the number of bytes per pixel for the video format requested */

	switch (format) {

	case VIDEO_PALETTE_YUV422:
		bpp = 2;
		break;

	case VIDEO_PALETTE_RGB555:
		bpp = 2;
		break;

	case VIDEO_PALETTE_RGB565:
		bpp = 2;
		break;

	case VIDEO_PALETTE_RGB24:
		bpp = 3;
		break;

	case VIDEO_PALETTE_RGB32:
		bpp = 4;
		break;

	default:
		bpp = 0;
	}

	return bpp;
}

static void zr36057_adjust_vfe(struct zoran *zr,
			       enum zoran_codec_mode mode)
{
	u32 reg;
	switch (mode) {
	case BUZ_MODE_MOTION_DECOMPRESS:
		btand(~ZR36057_VFESPFR_ExtFl, ZR36057_VFESPFR);
		reg = btread(ZR36057_VFEHCR);
		if (reg & (1 << 10)) {
			reg += ((1 << 10) | 1);
		}
		btwrite(reg, ZR36057_VFEHCR);
		break;
	case BUZ_MODE_MOTION_COMPRESS:
	case BUZ_MODE_IDLE:
	default:
		if (zr->params.norm == VIDEO_MODE_NTSC)
			btand(~ZR36057_VFESPFR_ExtFl, ZR36057_VFESPFR);
		else
			btor(ZR36057_VFESPFR_ExtFl, ZR36057_VFESPFR);
		reg = btread(ZR36057_VFEHCR);
		if (!(reg & (1 << 10))) {
			reg -= ((1 << 10) | 1);
		}
		btwrite(reg, ZR36057_VFEHCR);
		break;
	}
}

/*
 * set geometry
 */
static void zr36057_set_vfe(struct zoran *zr, int video_width,
			    int video_height, unsigned int video_format)
{
	struct tvnorm *tvn;
	unsigned HStart, HEnd, VStart, VEnd;
	unsigned DispMode;
	unsigned VidWinWid, VidWinHt;
	unsigned hcrop1, hcrop2, vcrop1, vcrop2;
	unsigned Wa, We, Ha, He;
	unsigned X, Y, HorDcm, VerDcm;
	u32 reg;
	unsigned mask_line_size;

	tvn = zr->timing;

	Wa = tvn->Wa;
	Ha = tvn->Ha;

	DEBUG1(printk (BUZ_INFO ": width = %d, height = %d\n", video_width, video_height));

	if (zr->params.norm != VIDEO_MODE_PAL
	    && zr->params.norm != VIDEO_MODE_NTSC
	    && zr->params.norm != VIDEO_MODE_SECAM) {
		printk(KERN_ERR "%s: set_vfe: video_norm = %d not valid\n",
		       zr->name, zr->params.norm);
		return;
	}
	if (video_width < BUZ_MIN_WIDTH || video_height < BUZ_MIN_HEIGHT
	    || video_width > Wa || video_height > Ha) {
		printk(KERN_ERR "%s: set_vfe: w=%d h=%d not valid\n",
		       zr->name, video_width, video_height);
		return;
	}

	/* if window has more than half of active height,
	   switch on interlacing - we want the full information */

	zr->video_interlace = (video_height > Ha / 2);

	/**** zr36057 ****/

	/* horizontal */
	VidWinWid = video_width;
	X = (VidWinWid * 64 + tvn->Wa - 1) / tvn->Wa;
	We = (VidWinWid * 64) / X;
	HorDcm = 64 - X;
	hcrop1 = 2 * ((tvn->Wa - We) / 4);
	hcrop2 = tvn->Wa - We - hcrop1;
	HStart = tvn->HStart | 1;
	if (zr->card == LML33)
		HStart += 62;
	if (zr->card == BUZ) {	//HStart += 67;
		HStart += 44;
	}
	HEnd = HStart + tvn->Wa - 1;
	HStart += hcrop1;
	HEnd -= hcrop2;
	reg = ((HStart & ZR36057_VFEHCR_Hmask) << ZR36057_VFEHCR_HStart)
	    | ((HEnd & ZR36057_VFEHCR_Hmask) << ZR36057_VFEHCR_HEnd);
	if (zr->card != BUZ)
		reg |= ZR36057_VFEHCR_HSPol;
	btwrite(reg, ZR36057_VFEHCR);

	/* Vertical */
	DispMode = !zr->video_interlace;
	VidWinHt = DispMode ? video_height : video_height / 2;
	Y = (VidWinHt * 64 * 2 + tvn->Ha - 1) / tvn->Ha;
	He = (VidWinHt * 64) / Y;
	VerDcm = 64 - Y;
	vcrop1 = (tvn->Ha / 2 - He) / 2;
	vcrop2 = tvn->Ha / 2 - He - vcrop1;
	VStart = tvn->VStart;
	VEnd = VStart + tvn->Ha / 2 - 1;
	VStart += vcrop1;
	VEnd -= vcrop2;
	reg = ((VStart & ZR36057_VFEVCR_Vmask) << ZR36057_VFEVCR_VStart)
	    | ((VEnd & ZR36057_VFEVCR_Vmask) << ZR36057_VFEVCR_VEnd);
	reg |= ZR36057_VFEVCR_VSPol;
	btwrite(reg, ZR36057_VFEVCR);

	/* scaler and pixel format */
	reg = 0;
	reg |= (HorDcm << ZR36057_VFESPFR_HorDcm);
	reg |= (VerDcm << ZR36057_VFESPFR_VerDcm);
	reg |= (DispMode << ZR36057_VFESPFR_DispMode);
	reg |= ZR36057_VFESPFR_LittleEndian;
	/* RJ: I don't know, why the following has to be the opposite
	   of the corresponding ZR36060 setting, but only this way
	   we get the correct colors when uncompressing to the screen  */
	//reg |= ZR36057_VFESPFR_VCLKPol; /**/
	/* RJ: Don't know if that is needed for NTSC also */
	if (zr->params.norm != VIDEO_MODE_NTSC)
		reg |= ZR36057_VFESPFR_ExtFl;	// NEEDED!!!!!!! Wolfgang
	reg |= ZR36057_VFESPFR_TopField;
	switch (video_format) {

	case VIDEO_PALETTE_YUV422:
		reg |= ZR36057_VFESPFR_YUV422;
		break;

	case VIDEO_PALETTE_RGB555:
		reg |= ZR36057_VFESPFR_RGB555 | ZR36057_VFESPFR_ErrDif;
		break;

	case VIDEO_PALETTE_RGB565:
		reg |= ZR36057_VFESPFR_RGB565 | ZR36057_VFESPFR_ErrDif;
		break;

	case VIDEO_PALETTE_RGB24:
		reg |= ZR36057_VFESPFR_RGB888 | ZR36057_VFESPFR_Pack24;
		break;

	case VIDEO_PALETTE_RGB32:
		reg |= ZR36057_VFESPFR_RGB888;
		break;

	default:
		printk(KERN_INFO "%s: Unknown color_fmt=%x\n", zr->name,
		       video_format);
		return;

	}
	if (HorDcm >= 48) {
		reg |= 3 << ZR36057_VFESPFR_HFilter;	/* 5 tap filter */
	} else if (HorDcm >= 32) {
		reg |= 2 << ZR36057_VFESPFR_HFilter;	/* 4 tap filter */
	} else if (HorDcm >= 16) {
		reg |= 1 << ZR36057_VFESPFR_HFilter;	/* 3 tap filter */
	}
	btwrite(reg, ZR36057_VFESPFR);

	/* display configuration */

	reg = (16 << ZR36057_VDCR_MinPix)
	    | (VidWinHt << ZR36057_VDCR_VidWinHt)
	    | (VidWinWid << ZR36057_VDCR_VidWinWid);
	if (triton || zr->revision <= 1)
		reg &= ~ZR36057_VDCR_Triton;
	else
		reg |= ZR36057_VDCR_Triton;
	btwrite(reg, ZR36057_VDCR);

	/* Write overlay clipping mask data, but don't enable overlay clipping */
	/* RJ: since this makes only sense on the screen, we use 
	   zr->window.width instead of video_width */

	mask_line_size = (BUZ_MAX_WIDTH + 31) / 32;
	reg = virt_to_bus(zr->overlay_mask);
	btwrite(reg, ZR36057_MMTR);
	reg = virt_to_bus(zr->overlay_mask + mask_line_size);
	btwrite(reg, ZR36057_MMBR);
	reg = mask_line_size - (zr->window.width + 31) / 32;
	if (DispMode == 0)
		reg += mask_line_size;
	reg <<= ZR36057_OCR_MaskStride;
	btwrite(reg, ZR36057_OCR);

	zr36057_adjust_vfe(zr, zr->codec_mode);

}

/*
 * Switch overlay on or off
 */

static void zr36057_overlay(struct zoran *zr, int on)
{
	int fmt, bpp;
	u32 reg;

	if (on) {
		/* do the necessary settings ... */

		btand(~ZR36057_VDCR_VidEn, ZR36057_VDCR);	/* switch it off first */

		switch (zr->buffer.depth) {
		case 15:
			fmt = VIDEO_PALETTE_RGB555;
			bpp = 2;
			break;
		case 16:
			fmt = VIDEO_PALETTE_RGB565;
			bpp = 2;
			break;
		case 24:
			fmt = VIDEO_PALETTE_RGB24;
			bpp = 3;
			break;
		case 32:
			fmt = VIDEO_PALETTE_RGB32;
			bpp = 4;
			break;
		default:
			fmt = 0;
			bpp = 0;
		}

		zr36057_set_vfe(zr, zr->window.width, zr->window.height,
				fmt);

		/* Start and length of each line MUST be 4-byte aligned.
		   This should be allready checked before the call to this routine.
		   All error messages are internal driver checking only! */

		/* video display top and bottom registers */

		reg =
		    (u32) zr->buffer.base + zr->window.x * bpp +
		    zr->window.y * zr->buffer.bytesperline;
		btwrite(reg, ZR36057_VDTR);
		if (reg & 3)
			printk(KERN_ERR
			       "%s: zr36057_overlay: video_address not aligned\n",
			       zr->name);
		if (zr->video_interlace)
			reg += zr->buffer.bytesperline;
		btwrite(reg, ZR36057_VDBR);

		/* video stride, status, and frame grab register */

		reg = zr->buffer.bytesperline - zr->window.width * bpp;
		if (zr->video_interlace)
			reg += zr->buffer.bytesperline;
		if (reg & 3)
			printk(KERN_ERR
			       "%s: zr36057_overlay: video_stride not aligned\n",
			       zr->name);
		reg = (reg << ZR36057_VSSFGR_DispStride);
		reg |= ZR36057_VSSFGR_VidOvf;	/* clear overflow status */
		btwrite(reg, ZR36057_VSSFGR);

		/* Set overlay clipping */

		if (zr->window.clipcount)
			btor(ZR36057_OCR_OvlEnable, ZR36057_OCR);

		/* ... and switch it on */

		btor(ZR36057_VDCR_VidEn, ZR36057_VDCR);
	} else {
		/* Switch it off */

		btand(~ZR36057_VDCR_VidEn, ZR36057_VDCR);
	}
}

/*
 * The overlay mask has one bit for each pixel on a scan line,
 *  and the maximum window size is BUZ_MAX_WIDTH * BUZ_MAX_HEIGHT pixels.
 */
static void write_overlay_mask(struct zoran *zr, struct video_clip *vp,
			       int count)
{
	unsigned mask_line_size = (BUZ_MAX_WIDTH + 31) / 32;
	u32 *mask;
	int x, y, width, height;
	unsigned i, j, k;
	u32 reg;

	/* fill mask with one bits */
	memset(zr->overlay_mask, ~0, mask_line_size * 4 * BUZ_MAX_HEIGHT);
	reg = 0;

	for (i = 0; i < count; ++i) {
		/* pick up local copy of clip */
		x = vp[i].x;
		y = vp[i].y;
		width = vp[i].width;
		height = vp[i].height;

		/* trim clips that extend beyond the window */
		if (x < 0) {
			width += x;
			x = 0;
		}
		if (y < 0) {
			height += y;
			y = 0;
		}
		if (x + width > zr->window.width) {
			width = zr->window.width - x;
		}
		if (y + height > zr->window.height) {
			height = zr->window.height - y;
		}

		/* ignore degenerate clips */
		if (height <= 0) {
			continue;
		}
		if (width <= 0) {
			continue;
		}

		/* apply clip for each scan line */
		for (j = 0; j < height; ++j) {
			/* reset bit for each pixel */
			/* this can be optimized later if need be */
			mask = zr->overlay_mask + (y + j) * mask_line_size;
			for (k = 0; k < width; ++k) {
				mask[(x + k) / 32] &=
				    ~((u32) 1 << (x + k) % 32);
			}
		}
	}
}

/* Enable/Disable uncompressed memory grabbing of the 36057 */

static void zr36057_set_memgrab(struct zoran *zr, int mode)
{
	if (mode) {
		if (btread(ZR36057_VSSFGR) &
		    (ZR36057_VSSFGR_SnapShot | ZR36057_VSSFGR_FrameGrab))
			printk(KERN_WARNING
			       "%s: zr36057_set_memgrab_on with SnapShot or FrameGrab on ???\n",
			       zr->name);

		/* switch on VSync interrupts */

		btwrite(IRQ_MASK, ZR36057_ISR);	// Clear Interrupts
		btor(cardvsync[zr->card], ZR36057_ICR);	// SW

		/* enable SnapShot */

		btor(ZR36057_VSSFGR_SnapShot, ZR36057_VSSFGR);

		/* Set zr36057 video front end  and enable video */

		zr36057_set_vfe(zr, zr->gwidth, zr->gheight, zr->gformat);

		zr->v4l_memgrab_active = 1;
	} else {
		zr->v4l_memgrab_active = 0;

		/* switch off VSync interrupts */

		//btand(~ZR36057_ICR_GIRQ1, ZR36057_ICR); // SW

		/* reenable grabbing to screen if it was running */

		if (zr->v4l_overlay_active) {
			zr36057_overlay(zr, 1);
		} else {
			btand(~ZR36057_VDCR_VidEn, ZR36057_VDCR);
			btand(~ZR36057_VSSFGR_SnapShot, ZR36057_VSSFGR);
		}
	}
}

static int wait_grab_pending(struct zoran *zr)
{
	unsigned long flags;

	/* wait until all pending grabs are finished */

	if (!zr->v4l_memgrab_active)
		return 0;

	while (zr->v4l_pend_tail != zr->v4l_pend_head) {
		interruptible_sleep_on(&zr->v4l_capq);
		if (signal_pending(current))
			return -ERESTARTSYS;
	}

	spin_lock_irqsave(&zr->lock, flags);
	zr36057_set_memgrab(zr, 0);
	spin_unlock_irqrestore(&zr->lock, flags);

	return 0;
}

/*
 *   V4L Buffer grabbing
 */

static int v4l_grab(struct zoran *zr, struct video_mmap *mp)
{
	unsigned long flags;
	int res, bpp;

	/*
	 * There is a long list of limitations to what is allowed to be grabbed
	 * We don't output error messages here, since some programs (e.g. xawtv)
	 * just try several settings to find out what is valid or not.
	 */

	/* No grabbing outside the buffer range! */

	if (mp->frame >= v4l_nbufs || mp->frame < 0) {
		DEBUG2(printk
		       (KERN_ERR "%s: Can not grab frame %d\n", zr->name,
			mp->frame));
		return -EINVAL;
	}

	/* Check size and format of the grab wanted */

	if (mp->height < BUZ_MIN_HEIGHT || mp->width < BUZ_MIN_WIDTH
	    || mp->height > BUZ_MAX_HEIGHT || mp->width > BUZ_MAX_WIDTH) {
		DEBUG2(printk
		       (KERN_ERR "%s: Wrong frame size.\n", zr->name));
		return -EINVAL;
	}

	bpp = format2bpp(mp->format);
	if (bpp == 0) {
		DEBUG2(printk
		       (KERN_ERR "%s: Wrong bytes-per-pixel format\n",
			zr->name));
		return -EINVAL;
	}

	/* Check against available buffer size */

	if (mp->height * mp->width * bpp > v4l_bufsize) {
		DEBUG2(printk
		       (KERN_ERR "%s: Video buffer size is too small\n",
			zr->name));
		return -EINVAL;
	}

	/* The video front end needs 4-byte alinged line sizes */

	if ((bpp == 2 && (mp->width & 1)) || (bpp == 3 && (mp->width & 3))) {
		DEBUG2(printk
		       (KERN_ERR "%s: Wrong frame alingment\n", zr->name));
		return -EINVAL;
	}

	/*
	 * To minimize the time spent in the IRQ routine, we avoid setting up
	 * the video front end there.
	 * If this grab has different parameters from a running streaming capture
	 * we stop the streaming capture and start it over again.
	 */

	if (zr->v4l_memgrab_active
	    && (zr->gwidth != mp->width || zr->gheight != mp->height
		|| zr->gformat != mp->format)) {
		res = wait_grab_pending(zr);
		if (res)
			return res;
	}
	zr->gwidth = mp->width;
	zr->gheight = mp->height;
	zr->gformat = mp->format;
	zr->gbpl = bpp * zr->gwidth;


	spin_lock_irqsave(&zr->lock, flags);

	/* make sure a grab isn't going on currently with this buffer */

	switch (zr->v4l_gbuf[mp->frame].state) {

	default:
	case BUZ_STATE_PEND:
		res = -EBUSY;	/* what are you doing? */
		break;

	case BUZ_STATE_USER:
	case BUZ_STATE_DONE:
		/* since there is at least one unused buffer there's room for at least one more pend[] entry */
		zr->v4l_pend[zr->v4l_pend_head++ & V4L_MASK_FRAME] =
		    mp->frame;
		zr->v4l_gbuf[mp->frame].state = BUZ_STATE_PEND;
		res = 0;
		break;

	}

	/* put the 36057 into frame grabbing mode */

	if (!res && !zr->v4l_memgrab_active)
		zr36057_set_memgrab(zr, 1);

	spin_unlock_irqrestore(&zr->lock, flags);
	//DEBUG2(printk(KERN_INFO "%s: Frame grab 3...\n", zr->name));

	return res;
}

/*
 * Sync on a V4L buffer
 */

static int v4l_sync(struct zoran *zr, int frame)
{
	unsigned long flags;

	/* check passed-in frame number */

	if (frame >= v4l_nbufs || frame < 0) {
		DEBUG1(printk(KERN_ERR "%s: v4l_sync: frame %d is invalid\n",
		       zr->name, frame));
		return -EINVAL;
	}

	/* Check if is buffer was queued at all */

	if (zr->v4l_gbuf[frame].state == BUZ_STATE_USER) {
		DEBUG1(printk(KERN_ERR
		       "%s: v4l_sync: Attempt to sync on a buffer which was not queued?\n",
		       zr->name));
		return -EPROTO;
	}

	/* wait on this buffer to get ready */

	while (zr->v4l_gbuf[frame].state == BUZ_STATE_PEND) {
		interruptible_sleep_on(&zr->v4l_capq);
		if (signal_pending(current))
			return -ERESTARTSYS;
	}

	/* buffer should now be in BUZ_STATE_DONE */

	if (zr->v4l_gbuf[frame].state != BUZ_STATE_DONE)
		printk(KERN_ERR "%s: v4l_sync - internal error\n",
		       zr->name);

	/* Check if streaming capture has finished */

	spin_lock_irqsave(&zr->lock, flags);

	if (zr->v4l_pend_tail == zr->v4l_pend_head)
		zr36057_set_memgrab(zr, 0);

	spin_unlock_irqrestore(&zr->lock, flags);

	return 0;
}

/*****************************************************************************
 *                                                                           *
 *  Set up the Buz-specific MJPEG part                                       *
 *                                                                           *
 *****************************************************************************/

/*
Wait til post office is no longer busy */
static int post_office_wait(struct zoran *zr)
{
	u32 por;

//      while (((por = btread(ZR36057_POR)) & (ZR36057_POR_POPen | ZR36057_POR_POTime)) == ZR36057_POR_POPen) {
	while ((por = btread(ZR36057_POR)) & ZR36057_POR_POPen) {
		/* wait for something to happen */
	}
	if ((por & ZR36057_POR_POTime) && zr->card != LML33
	    && zr->card != BUZ) {
		/* In LML33/BUZ \GWS line is not connected, so it has always timeout set */
		printk(KERN_WARNING "%s: pop timeout %08x\n", zr->name, por);
		return -1;
	}
	return 0;
}

static int post_office_write(struct zoran *zr, unsigned guest,
			     unsigned reg, unsigned value)
{
	u32 por;

	por =
	    ZR36057_POR_PODir | ZR36057_POR_POTime | ((guest & 7) << 20) |
	    ((reg & 7) << 16) | (value & 0xFF);
	btwrite(por, ZR36057_POR);
	return post_office_wait(zr);
}

static int post_office_read(struct zoran *zr, unsigned guest, unsigned reg)
{
	u32 por;

	por = ZR36057_POR_POTime | ((guest & 7) << 20) | ((reg & 7) << 16);
	btwrite(por, ZR36057_POR);
	if (post_office_wait(zr) < 0) {
		return -1;
	}
	return btread(ZR36057_POR) & 0xFF;
}

static int zr36060_write_8(struct zoran *zr, unsigned reg, unsigned val)
{
	if (post_office_wait(zr)
	    || post_office_write(zr, 0, 1, reg >> 8)
	    || post_office_write(zr, 0, 2, reg)) {
		return -1;
	}
	return post_office_write(zr, 0, 3, val);
}

static int zr36060_write_16(struct zoran *zr, unsigned reg, unsigned val)
{
	if (zr36060_write_8(zr, reg + 0, val >> 8)) {
		return -1;
	}
	return zr36060_write_8(zr, reg + 1, val >> 0);
}

static int zr36060_write_24(struct zoran *zr, unsigned reg, unsigned val)
{
	if (zr36060_write_8(zr, reg + 0, val >> 16)) {
		return -1;
	}
	return zr36060_write_16(zr, reg + 1, val >> 0);
}

static int zr36060_write_32(struct zoran *zr, unsigned reg, unsigned val)
{
	if (zr36060_write_16(zr, reg + 0, val >> 16)) {
		return -1;
	}
	return zr36060_write_16(zr, reg + 2, val >> 0);
}

static u32 zr36060_read_8(struct zoran *zr, unsigned reg)
{
	if (post_office_wait(zr)
	    || post_office_write(zr, 0, 1, reg >> 8)
	    || post_office_write(zr, 0, 2, reg)) {
		return -1;
	}
	return post_office_read(zr, 0, 3) & 0xFF;
}

/* ----------------------------------------------------------------------- */

static void GPIO(struct zoran *zr, unsigned bit, unsigned value)
{
	u32 reg;
	u32 mask;

	mask = 1 << (24 + bit);
	reg = btread(ZR36057_GPPGCR1) & ~mask;
	if (value) {
		reg |= mask;
	}
	btwrite(reg, ZR36057_GPPGCR1);
	udelay(1);
}


static void zr36060_sleep(struct zoran *zr, int sleep)
{
	switch (zr->card) {
	case DC10:
	case DC10plus:
		GPIO(zr, 3, !sleep);
		break;
	case BUZ:
	case LML33:
		GPIO(zr, 1, !sleep);
		break;
	default:
		break;
	}
	if (!sleep)
		udelay(500);
	else
		udelay(2);
}

static int zr36060_reset(struct zoran *zr)
{
	switch (zr->card) {
	case DC10:
	case DC10plus:
		zr36060_sleep(zr, 0);
		GPIO(zr, 0, 0);
		udelay(2);
		GPIO(zr, 0, 1);
		udelay(2);
		break;
	case LML33:
	case BUZ:
		zr36060_sleep(zr, 0);
		post_office_write(zr, 3, 0, 0);
		udelay(2);
	default:;
	}
	return 0;
}

static void set_frame(struct zoran *zr, int val)
{
	switch (zr->card) {
	case DC10:
	case DC10plus:
		GPIO(zr, 6, val);
		break;
	case LML33:
	case BUZ:
		GPIO(zr, 3, val);
		break;
	default:
		break;
	}
}

static void set_videobus_dir(struct zoran *zr, int val)
{
	switch (zr->card) {
	case DC10:
	case DC10plus:
		GPIO(zr, 1, val);
		break;
	case LML33:
		if (lml33dpath == 0)
			GPIO(zr, 5, val);
		else
			GPIO(zr, 5, 1);
		break;
	case BUZ:
	default:
		break;
	}
}

static void set_videobus_enable(struct zoran *zr, int val)
{
	switch (zr->card) {
	case LML33:
		GPIO(zr, 7, val);
		break;
	case DC10:
	case DC10plus:
	case BUZ:
	default:
		break;
	}
}

static void zr36060_set_jpg(struct zoran *zr, enum zoran_codec_mode mode)
{
	struct tvnorm *tvn;
	u32 reg;
	int size, blocks;

	reg = (1 << 0)		/* CodeMstr */
	    |(0 << 2)		/* CFIS=0 */
	    |(0 << 6)		/* Endian=0 */
	    |(0 << 7);		/* Code16=0 */
	zr36060_write_8(zr, 0x002, reg);

	switch (mode) {

	case BUZ_MODE_MOTION_DECOMPRESS:
	case BUZ_MODE_STILL_DECOMPRESS:
		reg = 0x00;	/* Codec mode = decompression */
		break;

	case BUZ_MODE_MOTION_COMPRESS:
	case BUZ_MODE_STILL_COMPRESS:
	default:
		reg = 0xa4;	/* Codec mode = compression with variable scale factor */
		break;

	}
	zr36060_write_8(zr, 0x003, reg);

	reg = 0x00;		/* reserved, mbz */
	zr36060_write_8(zr, 0x004, reg);

	/* code volume */

	/* Target field size in pixels: */
	tvn = zr->timing;
	size =
	    (tvn->Ha / 2) * (tvn->Wa) / (zr->params.HorDcm) /
	    (zr->params.VerDcm);
	blocks = size / 64;

	/* Target compressed field size in bits: */
	size = size * 16;	/* uncompressed size in bits */
	size = size * zr->params.quality / 400;	/* quality = 100 is a compression ratio 1:4 */

	/* Lower limit (arbitrary, 1 KB) */
	if (size < 8192)
		size = 8192;

	/* Upper limit: 6/8 of the code buffers */
	if (size * zr->params.field_per_buff > zr->jpg_bufsize * 6)
		size = zr->jpg_bufsize * 6 / zr->params.field_per_buff;

	reg = size * 4 / blocks;
	if (reg > 0xf0)
		reg = 0xf0;	/* 480 bits/block, does 0xff represents unlimited? */
	zr36060_write_8(zr, 0x005, reg);

	/* JPEG markers */
	reg = (zr->params.jpeg_markers) & 0x38;	/* DRI, DQT, DHT */
	if (zr->params.COM_len)
		reg |= JPEG_MARKER_COM;
	if (zr->params.APP_len)
		reg |= JPEG_MARKER_APP;
	zr36060_write_8(zr, 0x006, reg);

	if (zr->card != LML33 && zr->card != BUZ) {
		reg = (0 << 3)	/* EOAV=0 */
		    |(0 << 2)	/* EOI=0 */
		    |(0 << 1)	/* END=0 */
		    |(1 << 0);	/* DATERR=1 */
	} else {
		reg = (0 << 3)	/* EOAV=0 */
		    |(0 << 2)	/* EOI=0 */
		    |(0 << 1)	/* END=0 */
		    |(0 << 0);	/* DATERR=0 */
	}
	zr36060_write_8(zr, 0x007, reg);

	reg = size;
	zr36060_write_32(zr, 0x009, reg);

	reg = (size * 10) / 11;
	zr36060_write_32(zr, 0x00d, reg);	// Not needed for compr. with variable scale factor, just in case ...

	/* how do we set initial SF as a function of quality parameter? */
	reg = 0x0100;		/* SF=1.0 */
	zr36060_write_16(zr, 0x011, reg);

	reg = 0x00ffffff;	/* AF=max */
	zr36060_write_24(zr, 0x013, reg);

	reg = 0x0000;		/* test */
	zr36060_write_16(zr, 0x024, reg);

	//post_office_read(zr,1,0);
}

static void zr36060_set_video(struct zoran *zr, enum zoran_codec_mode mode)
{
	struct tvnorm *tvn;
	u32 reg;
	unsigned HStart;

	tvn = zr->timing;

	reg = (0 << 7)		/* Video8     */
	    |(0 << 6)		/* Range      */
	    |(0 << 3)		/* FlDet      */
	    |(1 << 2)		/* FlVedge    */
	    |(0 << 1)		/* FlExt      */
	    |(0 << 0);		/* SyncMstr   */

	if (mode != BUZ_MODE_STILL_DECOMPRESS) {
		/* limit pixels to range 16..235 as per CCIR-601 */
		reg |= (1 << 6);	/* Range=1 */
	}
	zr36060_write_8(zr, 0x030, reg);

	switch (zr->card) {
	case DC10:
	case DC10plus:
		reg = (0 << 7)	/* VCLKPol   */
		    |(0 << 6)	/* PValPol   */
		    |(0 << 5)	/* PoePol    */
		    |(0 << 4)	/* SImgPol   */
		    |(1 << 3)	/* BLPol     */
		    |(1 << 2)	/* FlPol     */
		    |(1 << 1)	/* HSPol     */
		    |(1 << 0);	/* VSPol     */
		break;
	case LML33:
		reg = (0 << 7)	/* VCLKPol=0 */
		    |(0 << 6)	/* PValPol=0 */
		    |(1 << 5)	/* PoePol=1 */
		    |(0 << 4)	/* SImgPol=0 */
		    |(0 << 3)	/* BLPol=0 */
		    |(0 << 2)	/* FlPol=0 */
		    |(0 << 1)	/* HSPol=0, sync on falling edge */
		    |(1 << 0);	/* VSPol=1 */
		break;
	case BUZ:
	default:
		reg = (0 << 7)	/* VCLKPol=0 */
		    |(0 << 6)	/* PValPol=0 */
		    |(1 << 5)	/* PoePol=1 */
		    |(0 << 4)	/* SImgPol=0 */
		    |(0 << 3)	/* BLPol=0 */
		    |(0 << 2)	/* FlPol=0 */
		    |(1 << 1)	/* HSPol=0, sync on falling edge */
		    |(1 << 0);	/* VSPol=1 */
		break;
	}
	zr36060_write_8(zr, 0x031, reg);

	switch (zr->params.HorDcm) {
	default:
	case 1:
		reg = (0 << 0);
		break;		/* HScale = 0 */

	case 2:
		reg = (1 << 0);
		break;		/* HScale = 1 */

	case 4:
		reg = (2 << 0);
		break;		/* HScale = 2 */
	}
	if (zr->params.VerDcm == 2)
		reg |= (1 << 2);
	zr36060_write_8(zr, 0x032, reg);

	reg = 0x00;		/* BackY */
	zr36060_write_8(zr, 0x033, reg);

	reg = 0x80;		/* BackU */
	zr36060_write_8(zr, 0x034, reg);

	reg = 0x80;		/* BackV */
	zr36060_write_8(zr, 0x035, reg);

	/* sync generator */

	reg = tvn->Ht - 1;	/* Vtotal */
	zr36060_write_16(zr, 0x036, reg);

	reg = tvn->Wt - 1;	/* Htotal */
	zr36060_write_16(zr, 0x038, reg);

	reg = 6 - 1;		/* VsyncSize */
	zr36060_write_8(zr, 0x03a, reg);

	//reg   = 30 - 1;               /* HsyncSize */
	reg = (zr->params.norm == 1 ? 57 : 68);
	zr36060_write_8(zr, 0x03b, reg);

	reg = tvn->VStart - 1;	/* BVstart */
	zr36060_write_8(zr, 0x03c, reg);

	reg += tvn->Ha / 2;	/* BVend */
	zr36060_write_16(zr, 0x03e, reg);

	reg = tvn->HStart + 64 - 1;	/* BHstart */
	zr36060_write_8(zr, 0x03d, reg);

	reg += tvn->Wa;		/* BHend */
	zr36060_write_16(zr, 0x040, reg);

	/* active area */
	reg = zr->params.img_y + tvn->VStart;	/* Vstart */
	zr36060_write_16(zr, 0x042, reg);

	reg += zr->params.img_height;	/* Vend */
	zr36060_write_16(zr, 0x044, reg);

	HStart = tvn->HStart;
	if (zr->card == BUZ) {
		HStart += 44;
	} else {
		HStart += 64;
	}
	reg = zr->params.img_x + HStart;	/* Hstart */
	zr36060_write_16(zr, 0x046, reg);

	reg += zr->params.img_width;	/* Hend */
	zr36060_write_16(zr, 0x048, reg);

	/* subimage area */
	reg = tvn->VStart - 4;	/* SVstart */
	zr36060_write_16(zr, 0x04a, reg);

	reg += tvn->Ha / 2 + 8;	/* SVend */
	zr36060_write_16(zr, 0x04c, reg);

	reg = tvn->HStart + 64 - 4;	/* SHstart */
	zr36060_write_16(zr, 0x04e, reg);

	reg += tvn->Wa + 8;	/* SHend */
	zr36060_write_16(zr, 0x050, reg);
}

static void zr36060_set_jpg_SOF(struct zoran *zr)
{
	u32 reg;


	reg = 0xffc0;		/* SOF marker */
	zr36060_write_16(zr, 0x060, reg);

	reg = 17;		/* SOF length */
	zr36060_write_16(zr, 0x062, reg);

	reg = 8;		/* precision 8 bits */
	zr36060_write_8(zr, 0x064, reg);

	reg = zr->params.img_height / zr->params.VerDcm;	/* image height */
	zr36060_write_16(zr, 0x065, reg);

	reg = zr->params.img_width / zr->params.HorDcm;	/* image width */
	zr36060_write_16(zr, 0x067, reg);

	reg = 3;		/* 3 color components */
	zr36060_write_8(zr, 0x069, reg);

	reg = 0x002100;		/* Y component */
	zr36060_write_24(zr, 0x06a, reg);

	reg = 0x011101;		/* U component */
	zr36060_write_24(zr, 0x06d, reg);

	reg = 0x021101;		/* V component */
	zr36060_write_24(zr, 0x070, reg);
}

static void zr36060_set_jpg_SOS(struct zoran *zr)
{
	u32 reg;


	reg = 0xffda;		/* SOS marker */
	zr36060_write_16(zr, 0x07a, reg);

	reg = 12;		/* SOS length */
	zr36060_write_16(zr, 0x07c, reg);

	reg = 3;		/* 3 color components */
	zr36060_write_8(zr, 0x07e, reg);

	reg = 0x0000;		/* Y component */
	zr36060_write_16(zr, 0x07f, reg);

	reg = 0x0111;		/* U component */
	zr36060_write_16(zr, 0x081, reg);

	reg = 0x0211;		/* V component */
	zr36060_write_16(zr, 0x083, reg);

	reg = 0x003f00;		/* Start, end spectral scans */
	zr36060_write_24(zr, 0x085, reg);
}

static void zr36060_set_jpg_DRI(struct zoran *zr)
{
	u32 reg;


	reg = 0xffdd;		/* DRI marker */
	zr36060_write_16(zr, 0x0c0, reg);

	reg = 4;		/* DRI length */
	zr36060_write_16(zr, 0x0c2, reg);

	reg = 8;		/* length in MCUs */
	zr36060_write_16(zr, 0x0c4, reg);
}

static void zr36060_set_jpg_DQT(struct zoran *zr)
{
	unsigned i;
	unsigned adr;
	static const u8 dqt[] = {
		0xff, 0xdb,	/* DHT marker */
		0x00, 0x84,	/* DHT length */
		0x00,		/* table ID 0 */
		0x10, 0x0b, 0x0c, 0x0e, 0x0c, 0x0a, 0x10, 0x0e,
		0x0d, 0x0e, 0x12, 0x11, 0x10, 0x13, 0x18, 0x28,
		0x1a, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23, 0x25,
		0x1d, 0x28, 0x3a, 0x33, 0x3d, 0x3c, 0x39, 0x33,
		0x38, 0x37, 0x40, 0x48, 0x5c, 0x4e, 0x40, 0x44,
		0x57, 0x45, 0x37, 0x38, 0x50, 0x6d, 0x51, 0x57,
		0x5f, 0x62, 0x67, 0x68, 0x67, 0x3e, 0x4d, 0x71,
		0x79, 0x70, 0x64, 0x78, 0x5c, 0x65, 0x67, 0x63,
		0x01,		/* table ID 1 */
		0x11, 0x12, 0x12, 0x18, 0x15, 0x18, 0x2f, 0x1a,
		0x1a, 0x2f, 0x63, 0x42, 0x38, 0x42, 0x63, 0x63,
		0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
		0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
		0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
		0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
		0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
		0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63
	};

	/* write fixed quantitization tables */
	adr = 0x0cc;
	for (i = 0; i < sizeof(dqt); ++i) {
		zr36060_write_8(zr, adr++, dqt[i]);
	}
}

static void zr36060_set_jpg_DHT(struct zoran *zr)
{
	unsigned i;
	unsigned adr;
	static const u8 dht[] = {
		0xff, 0xc4,	/* DHT marker */
		0x01, 0xa2,	/* DHT length */
		0x00,		/* table class 0, ID 0 */
		0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01,	/* # codes of length 1..8 */
		0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* # codes of length 8..16 */
		0x00,		/* values for codes of length 2 */
		0x01, 0x02, 0x03, 0x04, 0x05,	/* values for codes of length 3 */
		0x06,		/* values for codes of length 4 */
		0x07,		/* values for codes of length 5 */
		0x08,		/* values for codes of length 6 */
		0x09,		/* values for codes of length 7 */
		0x0a,		/* values for codes of length 8 */
		0x0b,		/* values for codes of length 9 */
		0x01,		/* table class 0, ID 1 */
		0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,	/* # codes of length 1..8 */
		0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,	/* # codes of length 9..16 */
		0x00, 0x01, 0x02,	/* values for codes of length 2 */
		0x03,		/* values for codes of length 3 */
		0x04,		/* values for codes of length 4 */
		0x05,		/* values for codes of length 5 */
		0x06,		/* values for codes of length 6 */
		0x07,		/* values for codes of length 7 */
		0x08,		/* values for codes of length 8 */
		0x09,		/* values for codes of length 9 */
		0x0a,		/* values for codes of length 10 */
		0x0b,		/* values for codes of length 11 */
		0x10,
		0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03,
		0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7d,
		0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
		0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
		0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
		0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
		0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
		0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
		0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
		0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
		0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
		0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
		0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
		0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
		0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
		0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
		0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
		0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
		0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
		0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
		0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
		0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
		0xf9, 0xfa, 0x11, 0x00, 0x02, 0x01, 0x02, 0x04,
		0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00,
		0x01, 0x02, 0x77, 0x00, 0x01, 0x02, 0x03, 0x11,
		0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51,
		0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08,
		0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23,
		0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a,
		0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18,
		0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35,
		0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45,
		0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55,
		0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65,
		0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75,
		0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84,
		0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93,
		0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2,
		0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa,
		0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9,
		0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
		0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
		0xd8, 0xd9, 0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6,
		0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5,
		0xf6, 0xf7, 0xf8, 0xf9, 0xfa
	};

	/* write fixed Huffman tables */
	adr = 0x1d4;
	for (i = 0; i < sizeof(dht); ++i) {
		zr36060_write_8(zr, adr++, dht[i]);
	}
}

static void zr36060_set_jpg_APP(struct zoran *zr)
{
	unsigned adr;
	int len, i;
	u32 reg;


	len = zr->params.APP_len;
	if (len < 0)
		len = 0;
	if (len > 60)
		len = 60;

	i = zr->params.APPn;
	if (i < 0)
		i = 0;
	if (i > 15)
		i = 15;

	reg = 0xffe0 + i;	/* APPn marker */
	zr36060_write_16(zr, 0x380, reg);

	reg = len + 2;		/* APPn len */
	zr36060_write_16(zr, 0x382, reg);

	/* write APPn data */
	adr = 0x384;
	for (i = 0; i < 60; i++) {
		zr36060_write_8(zr, adr++,
				(i < len ? zr->params.APP_data[i] : 0));
	}
}

static void zr36060_set_jpg_COM(struct zoran *zr)
{
	unsigned adr;
	int len, i;
	u32 reg;


	len = zr->params.COM_len;
	if (len < 0)
		len = 0;
	if (len > 60)
		len = 60;

	reg = 0xfffe;		/* COM marker */
	zr36060_write_16(zr, 0x3c0, reg);

	reg = len + 2;		/* COM len */
	zr36060_write_16(zr, 0x3c2, reg);

	/* write COM data */
	adr = 0x3c4;
	for (i = 0; i < 60; i++) {
		zr36060_write_8(zr, adr++,
				(i < len ? zr->params.COM_data[i] : 0));
	}
}

static void zr36060_set_cap(struct zoran *zr, enum zoran_codec_mode mode)
{
	unsigned i;
	u32 reg;

	zr36060_reset(zr);
	mdelay(10);

	reg = (0 << 7)		/* Load=0 */
	    |(1 << 0);		/* SynRst=1 */
	zr36060_write_8(zr, 0x000, reg);

	zr36060_set_jpg(zr, mode);
	zr36060_set_video(zr, mode);
	zr36060_set_jpg_SOF(zr);
	zr36060_set_jpg_SOS(zr);
	zr36060_set_jpg_DRI(zr);
	zr36060_set_jpg_DQT(zr);
	zr36060_set_jpg_DHT(zr);
	zr36060_set_jpg_APP(zr);
	zr36060_set_jpg_COM(zr);

	reg = (1 << 7)		/* Load=1 */
	    |(1 << 0);		/* SynRst=0 */
	zr36060_write_8(zr, 0x000, reg);

	/* wait for codec to unbusy */
	for (i = 0; i < 100000; ++i) {
		reg = zr36060_read_8(zr, 0x001);
		if ((reg & (1 << 7)) == 0) {
			return;
		}
		//udelay(100);
	}
	printk(KERN_ERR "%sZR36060: stuck busy, statux=%02x\n", zr->name,
	       reg);
}

static void init_jpeg_queue(struct zoran *zr)
{
	int i;
	/* re-initialize DMA ring stuff */
	zr->jpg_que_head = 0;
	zr->jpg_dma_head = 0;
	zr->jpg_dma_tail = 0;
	zr->jpg_que_tail = 0;
	zr->jpg_seq_num = 0;
	zr->JPEG_error = 0;
	zr->num_errors = 0;
	zr->jpg_err_seq = 0;
	zr->jpg_err_shift = 0;
	zr->jpg_queued_num = 0;
	for (i = 0; i < zr->jpg_nbufs; i++) {
		zr->jpg_gbuf[i].state = BUZ_STATE_USER;	/* nothing going on */
	}
	for (i = 0; i < BUZ_NUM_STAT_COM; i++) {
		zr->stat_com[i] = 1;	/* mark as unavailable to zr36057 */
	}
}

static void zr36057_set_jpg(struct zoran *zr, enum zoran_codec_mode mode)
{
	struct tvnorm *tvn;
	u32 reg;

	tvn = zr->timing;

	/* assert P_Reset */
	btwrite(0, ZR36057_JPC);

	/* MJPEG compression mode */
	switch (mode) {

	case BUZ_MODE_MOTION_COMPRESS:
	default:
		reg = ZR36057_JMC_MJPGCmpMode;
		break;

	case BUZ_MODE_MOTION_DECOMPRESS:
		reg = ZR36057_JMC_MJPGExpMode;
		reg |= ZR36057_JMC_SyncMstr;
		/* RJ: The following is experimental - improves the output to screen */
		//if(zr->params.VFIFO_FB) reg |= ZR36057_JMC_VFIFO_FB; // No, it doesn't. SM
		break;

	case BUZ_MODE_STILL_COMPRESS:
		reg = ZR36057_JMC_JPGCmpMode;
		break;

	case BUZ_MODE_STILL_DECOMPRESS:
		reg = ZR36057_JMC_JPGExpMode;
		break;

	}
	reg |= ZR36057_JMC_JPG;
	if (zr->params.field_per_buff == 1)
		reg |= ZR36057_JMC_Fld_per_buff;
	btwrite(reg, ZR36057_JMC);

	/* vertical */
	btor(ZR36057_VFEVCR_VSPol, ZR36057_VFEVCR);
	reg =
	    (6 << ZR36057_VSP_VsyncSize) | (tvn->Ht << ZR36057_VSP_FrmTot);
	btwrite(reg, ZR36057_VSP);
	reg = ((zr->params.img_y + tvn->VStart) << ZR36057_FVAP_NAY)
	    | (zr->params.img_height << ZR36057_FVAP_PAY);
	btwrite(reg, ZR36057_FVAP);

	/* horizontal */
	btor(ZR36057_VFEHCR_HSPol, ZR36057_VFEHCR);
	reg =
	    ((tvn->HSyncStart) << ZR36057_HSP_HsyncStart) | (tvn->
							     Wt <<
							     ZR36057_HSP_LineTot);
	btwrite(reg, ZR36057_HSP);
	reg = ((zr->params.img_x + tvn->HStart + 4) << ZR36057_FHAP_NAX)
	    | (zr->params.img_width << ZR36057_FHAP_PAX);
	btwrite(reg, ZR36057_FHAP);

	/* field process parameters */
	if (zr->params.odd_even)
		reg = ZR36057_FPP_Odd_Even;
	else
		reg = 0;
	if (mode == BUZ_MODE_MOTION_DECOMPRESS && zr->card != LML33
	    && zr->card != BUZ)
		reg ^= ZR36057_FPP_Odd_Even;

	btwrite(reg, ZR36057_FPP);

	/* Set proper VCLK Polarity, else colors will be wrong during playback */
	//btor(ZR36057_VFESPFR_VCLKPol, ZR36057_VFESPFR);

	/* code base address */
	reg = virt_to_bus(zr->stat_com);
	btwrite(reg, ZR36057_JCBA);

	/* FIFO threshold (FIFO is 160. double words) */
	/* NOTE: decimal values here */
	switch (mode) {

	case BUZ_MODE_STILL_COMPRESS:
	case BUZ_MODE_MOTION_COMPRESS:
		reg = 140;
		break;

	case BUZ_MODE_STILL_DECOMPRESS:
	case BUZ_MODE_MOTION_DECOMPRESS:
		reg = 20;
		break;

	default:
		reg = 80;
		break;

	}
	btwrite(reg, ZR36057_JCFT);
	zr36057_adjust_vfe(zr, mode);

}

#if (DEBUGLEVEL > 2)
static void dump_guests(struct zoran *zr)
{
	int i, guest[8];

	for (i = 1; i < 8; i++) {	// Don't read zr36060 here
		guest[i] = post_office_read(zr, i, 0);
	}

	printk(KERN_INFO "%s: Guests:", zr->name);

	for (i = 1; i < 8; i++) {
		printk(" 0x%02x", guest[i]);
	}
	printk("\n");
}

static unsigned long get_time(void)
{
	struct timeval tv;
	do_gettimeofday(&tv);
	return (1000000 * tv.tv_sec + tv.tv_usec);
}

static void detect_guest_activity(struct zoran *zr)
{
	int timeout, i, j, res, guest[8], guest0[8], change[8][3];
	unsigned long t0, t1;

	dump_guests(zr);
	printk(KERN_INFO "%s: Detecting guests activity, please wait...\n",
	       zr->name);
	for (i = 1; i < 8; i++) {	// Don't read zr36060 here
		guest0[i] = guest[i] = post_office_read(zr, i, 0);
	}

	timeout = 0;
	j = 0;
	t0 = get_time();
	while (timeout < 10000) {
		udelay(10);
		timeout++;
		for (i = 1; (i < 8) && (j < 8); i++) {
			res = post_office_read(zr, i, 0);
			if (res != guest[i]) {
				t1 = get_time();
				change[j][0] = (t1 - t0);
				t0 = t1;
				change[j][1] = i;
				change[j][2] = res;
				j++;
				guest[i] = res;
			}
		}
		if (j >= 8)
			break;
	}
	printk(KERN_INFO "%s: Guests:", zr->name);

	for (i = 1; i < 8; i++) {
		printk(" 0x%02x", guest0[i]);
	}
	printk("\n");
	if (j == 0) {
		printk(KERN_INFO "%s: No activity detected.\n", zr->name);
		return;
	}
	for (i = 0; i < j; i++) {
		printk(KERN_INFO "%s: %6d: %d => 0x%02x\n", zr->name,
		       change[i][0], change[i][1], change[i][2]);
	}
}
#endif

static void print_interrupts(struct zoran *zr)
{
	int res, noerr;
	noerr = 0;
	printk(KERN_INFO "%s: interrupts received:", zr->name);
	if ((res = zr->field_counter) < -1 || res > 1) {
		printk(" FD:%d", res);
	}
	if ((res = zr->intr_counter_GIRQ1) != 0) {
		printk(" GIRQ1:%d", res);
		noerr++;
	}
	if ((res = zr->intr_counter_GIRQ0) != 0) {
		printk(" GIRQ0:%d", res);
		noerr++;
	}
	if ((res = zr->intr_counter_CodRepIRQ) != 0) {
		printk(" CodRepIRQ:%d", res);
		noerr++;
	}
	if ((res = zr->intr_counter_JPEGRepIRQ) != 0) {
		printk(" JPEGRepIRQ:%d", res);
		noerr++;
	}
	if (zr->JPEG_max_missed) {
		printk(" JPEG delays: max=%d min=%d", zr->JPEG_max_missed,
		       zr->JPEG_min_missed);
	}
	if (zr->END_event_missed) {
		printk(" ENDs missed: %d", zr->END_event_missed);
	}
	//if (zr->jpg_queued_num) {
	printk(" queue_state=%ld/%ld/%ld/%ld", zr->jpg_que_tail,
	       zr->jpg_dma_tail, zr->jpg_dma_head, zr->jpg_que_head);
	//}
	if (!noerr) {
		printk(": no interrupts detected.");
	}
	printk("\n");
}

static void clear_interrupt_counters(struct zoran *zr)
{
	zr->intr_counter_GIRQ1 = 0;
	zr->intr_counter_GIRQ0 = 0;
	zr->intr_counter_CodRepIRQ = 0;
	zr->intr_counter_JPEGRepIRQ = 0;
	zr->field_counter = 0;
	zr->IRQ1_in = 0;
	zr->IRQ1_out = 0;
	zr->JPEG_in = 0;
	zr->JPEG_out = 0;
	zr->JPEG_0 = 0;
	zr->JPEG_1 = 0;
	zr->END_event_missed = 0;
	zr->JPEG_missed = 0;
	zr->JPEG_max_missed = 0;
	zr->JPEG_min_missed = 0x7fffffff;
}

static u32 count_reset_interrupt(struct zoran *zr)
{
	u32 isr;
	if ((isr = btread(ZR36057_ISR) & 0x78000000)) {
		if (isr & ZR36057_ISR_GIRQ1) {
			btwrite(ZR36057_ISR_GIRQ1, ZR36057_ISR);
			zr->intr_counter_GIRQ1++;
		}
		if (isr & ZR36057_ISR_GIRQ0) {
			btwrite(ZR36057_ISR_GIRQ0, ZR36057_ISR);
			zr->intr_counter_GIRQ0++;
		}
		if (isr & ZR36057_ISR_CodRepIRQ) {
			btwrite(ZR36057_ISR_CodRepIRQ, ZR36057_ISR);
			zr->intr_counter_CodRepIRQ++;
		}
		if (isr & ZR36057_ISR_JPEGRepIRQ) {
			btwrite(ZR36057_ISR_JPEGRepIRQ, ZR36057_ISR);
			zr->intr_counter_JPEGRepIRQ++;
		}
	}
	return isr;
}

static void jpeg_start(struct zoran *zr)
{
	int reg;
	zr->frame_num = 0;

	btwrite(ZR36057_JPC_P_Reset, ZR36057_JPC);	// /P_Reset
	btand(~ZR36057_MCTCR_CFlush, ZR36057_MCTCR);	// \CFlush
	btor(ZR36057_JPC_CodTrnsEn, ZR36057_JPC);	// /CodTrnsEn
	btwrite(IRQ_MASK, ZR36057_ISR);	// Clear IRQs
	btwrite(IRQ_MASK | ZR36057_ICR_IntPinEn, ZR36057_ICR);	// Enable IRQs

	set_frame(zr, 0);	// \FRAME

	/* JPEG codec guest ID */
	reg =
	    (1 << ZR36057_JCGI_JPEGuestID) | (0 <<
					      ZR36057_JCGI_JPEGuestReg);
	btwrite(reg, ZR36057_JCGI);

	btor(ZR36057_JPC_Active, ZR36057_JPC);	// /Active
	btor(ZR36057_JMC_Go_en, ZR36057_JMC);	// /Go_en
	udelay(30);
	set_frame(zr, 1);	// /FRAME
}

static void zr36057_enable_jpg(struct zoran *zr,
			       enum zoran_codec_mode mode)
{
	static int zero = 0;
	static int one = 1;

	zr->codec_mode = mode;
	switch (mode) {

	case BUZ_MODE_MOTION_COMPRESS:
		set_videobus_enable(zr, 0);
		set_videobus_dir(zr, 0);	// GPIO(zr, 1, 0);
		i2c_control_device(&zr->i2c, I2C_DRIVERID_VIDEODECODER,
				   DECODER_ENABLE_OUTPUT, &one);
		i2c_control_device(&zr->i2c, I2C_DRIVERID_VIDEOENCODER,
				   ENCODER_SET_INPUT, &zero);
		set_videobus_enable(zr, 1);
		zr36060_sleep(zr, 0);
		zr36060_set_cap(zr, mode);	// Load ZR36060
		init_jpeg_queue(zr);
		zr36057_set_jpg(zr, mode);	// \P_Reset, ... Video param, FIFO

		clear_interrupt_counters(zr);
		DEBUG1(printk
		       (KERN_INFO "%s: enable_jpg MOTION_COMPRESS\n",
			zr->name));
		break;

	case BUZ_MODE_MOTION_DECOMPRESS:
		set_videobus_enable(zr, 0);
		i2c_control_device(&zr->i2c, I2C_DRIVERID_VIDEODECODER,
				   DECODER_ENABLE_OUTPUT, &zero);
		set_videobus_dir(zr, 1);	// GPIO(zr, 1, 1);
		i2c_control_device(&zr->i2c, I2C_DRIVERID_VIDEOENCODER,
				   ENCODER_SET_INPUT, &one);
		set_videobus_enable(zr, 1);
		zr36060_sleep(zr, 0);
		zr36060_set_cap(zr, mode);	// Load ZR36060
		init_jpeg_queue(zr);
		zr36057_set_jpg(zr, mode);	// \P_Reset, ... Video param, FIFO

		clear_interrupt_counters(zr);
		DEBUG1(printk
		       (KERN_INFO "%s: enable_jpg MOTION_DECOMPRESS\n",
			zr->name));
		break;

	case BUZ_MODE_IDLE:
	default:
		/* shut down processing */
		btand(~(cardjpegint[zr->card] | ZR36057_ICR_JPEGRepIRQ),
		      ZR36057_ICR);
		btwrite(cardjpegint[zr->card] | ZR36057_ICR_JPEGRepIRQ,
			ZR36057_ISR);
		btand(~ZR36057_JMC_Go_en, ZR36057_JMC);	// \Go_en

		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(HZ/20);

		set_videobus_dir(zr, 0);	// GPIO(zr, 1, 0);
		set_frame(zr, 1);	//GPIO(zr, 6, 1);     // /FRAME
		btor(ZR36057_MCTCR_CFlush, ZR36057_MCTCR);	// /CFlush
		btwrite(0, ZR36057_JPC);	// \P_Reset,\CodTrnsEn,\Active
		btand(~ZR36057_JMC_VFIFO_FB, ZR36057_JMC);
		btand(~ZR36057_JMC_SyncMstr, ZR36057_JMC);
		zr36060_reset(zr);
		zr36060_sleep(zr, 1);
		zr36057_adjust_vfe(zr, mode);
		set_videobus_enable(zr, 0);
		i2c_control_device(&zr->i2c, I2C_DRIVERID_VIDEODECODER,
				   DECODER_ENABLE_OUTPUT, &one);
		i2c_control_device(&zr->i2c, I2C_DRIVERID_VIDEOENCODER,
				   ENCODER_SET_INPUT, &zero);
		set_videobus_enable(zr, 1);
		DEBUG1(printk
		       (KERN_INFO "%s: enable_jpg IDLE\n", zr->name));
		break;

	}
}

/*
 *   Queue a MJPEG buffer for capture/playback
 */

static int jpg_qbuf(struct zoran *zr, int frame,
		    enum zoran_codec_mode mode)
{
	unsigned long flags;
	int res;

	/* Check if buffers are allocated */

	if (!zr->jpg_buffers_allocated) {
		printk(KERN_ERR
		       "%s: jpg_qbuf: buffers not yet allocated\n",
		       zr->name);
		return -ENOMEM;
	}

	/* Does the user want to stop streaming? */

	if (frame < 0) {
		if (zr->codec_mode == mode) {
			zr36057_enable_jpg(zr, BUZ_MODE_IDLE);
			return 0;
		} else {
			printk(KERN_ERR
			       "%s: jpg_qbuf - stop streaming but not in streaming mode\n",
			       zr->name);
			return -EINVAL;
		}
	}

	/* No grabbing outside the buffer range! */

	if (frame >= zr->jpg_nbufs) {
		printk(KERN_ERR "%s: jpg_qbuf: buffer %d out of range\n",
		       zr->name, frame);
		return -EINVAL;
	}

	/* what is the codec mode right now? */

	if (zr->codec_mode == BUZ_MODE_IDLE) {
		/* Ok load up the zr36060 */
		zr36057_enable_jpg(zr, mode);
	} else if (zr->codec_mode != mode) {
		/* wrong codec mode active - invalid */
		printk(KERN_ERR "%s: jpg_qbuf - codec in wrong mode\n",
		       zr->name);
		return -EINVAL;
	}

	spin_lock_irqsave(&zr->lock, flags);

	/* make sure a grab isn't going on currently with this buffer */

	switch (zr->jpg_gbuf[frame].state) {

	case BUZ_STATE_DONE:
		DEBUG1(printk
		       (KERN_WARNING
			"%s: Warning: queing frame in BUZ_STATE_DONE state\n",
			zr->name));
	case BUZ_STATE_USER:
		/* since there is at least one unused buffer there's room for at least one more pend[] entry */
		zr->jpg_pend[zr->jpg_que_head++ & BUZ_MASK_FRAME] = frame;
		zr->jpg_gbuf[frame].state = BUZ_STATE_PEND;
		zoran_feed_stat_com(zr);
		res = 0;
		break;

	default:
	case BUZ_STATE_DMA:
	case BUZ_STATE_PEND:
		res = -EBUSY;	/* what are you doing? */
		break;

	}

	spin_unlock_irqrestore(&zr->lock, flags);

	/* Start the zr36060 when the first frame is queued  */
	if (zr->jpg_que_head == 1)
		jpeg_start(zr);

	return res;
}

/*
 *   Sync on a MJPEG buffer
 */

static int jpg_sync(struct zoran *zr, struct zoran_sync *bs)
{
	unsigned long flags;
	int frame, timeout;

	if (zr->codec_mode != BUZ_MODE_MOTION_DECOMPRESS
	    && zr->codec_mode != BUZ_MODE_MOTION_COMPRESS) {
		DEBUG1(printk(KERN_ERR
		       "%s: BUZIOCSYNC: - codec not in streaming mode\n",
		       zr->name));
		return -EINVAL;
	}
	while (zr->jpg_que_tail == zr->jpg_dma_tail) {
		if (zr->jpg_dma_tail == zr->jpg_dma_head)
			break;
		timeout =
		    interruptible_sleep_on_timeout(&zr->jpg_capq, 10 * HZ);
		if (!timeout) {
			btand(~ZR36057_JMC_Go_en, ZR36057_JMC);
			udelay(1);
			printk(KERN_ERR
			       "%s: timeout: codec isr=0x%02x, csr=0x%02x\n",
			       zr->name, zr36060_read_8(zr, 0x008),
			       zr36060_read_8(zr, 0x001));
			return -ETIME;
		} else if (signal_pending(current))
			return -ERESTARTSYS;
	}

	spin_lock_irqsave(&zr->lock, flags);

	if (zr->jpg_dma_tail != zr->jpg_dma_head)
		frame = zr->jpg_pend[zr->jpg_que_tail++ & BUZ_MASK_FRAME];
	else
		frame = zr->jpg_pend[zr->jpg_que_tail & BUZ_MASK_FRAME];
	/* buffer should now be in BUZ_STATE_DONE */

#if(DEBUGLEVEL > 0)
	if (zr->jpg_gbuf[frame].state != BUZ_STATE_DONE)
		printk(KERN_ERR "%s: jpg_sync - internal error\n",
		       zr->name);
#endif

	*bs = zr->jpg_gbuf[frame].bs;
	zr->jpg_gbuf[frame].state = BUZ_STATE_USER;

	spin_unlock_irqrestore(&zr->lock, flags);

	return 0;
}

/* when this is called the spinlock must be held */
static void zoran_feed_stat_com(struct zoran *zr)
{
	/* move frames from pending queue to DMA */

	int frame, i, max_stat_com;

	max_stat_com =
	    (zr->params.TmpDcm ==
	     1) ? BUZ_NUM_STAT_COM : (BUZ_NUM_STAT_COM >> 1);

	while ((zr->jpg_dma_head - zr->jpg_dma_tail) < max_stat_com
	       && zr->jpg_dma_head < zr->jpg_que_head) {

		frame = zr->jpg_pend[zr->jpg_dma_head & BUZ_MASK_FRAME];
		if (zr->params.TmpDcm == 1) {
			/* fill 1 stat_com entry */
			i = (zr->jpg_dma_head -
			     zr->jpg_err_shift) & BUZ_MASK_STAT_COM;
			if (!(zr->stat_com[i] & 1))
				break;
			zr->stat_com[i] = zr->jpg_gbuf[frame].frag_tab_bus;
		} else {
			/* fill 2 stat_com entries */
			i = ((zr->jpg_dma_head -
			      zr->jpg_err_shift) & 1) * 2;
			if (!(zr->stat_com[i] & 1))
				break;
			zr->stat_com[i] = zr->jpg_gbuf[frame].frag_tab_bus;
			zr->stat_com[i + 1] =
			    zr->jpg_gbuf[frame].frag_tab_bus;
		}
		zr->jpg_gbuf[frame].state = BUZ_STATE_DMA;
		zr->jpg_dma_head++;

	}
	if (zr->codec_mode == BUZ_MODE_MOTION_DECOMPRESS)
		zr->jpg_queued_num++;
}

/* when this is called the spinlock must be held */
static void zoran_reap_stat_com(struct zoran *zr)
{
	/* move frames from DMA queue to done queue */

	int i;
	u32 stat_com;
	unsigned int seq;
	unsigned int dif;
	int frame;
	struct zoran_gbuffer *gbuf;

	/* In motion decompress we don't have a hardware frame counter,
	   we just count the interrupts here */

	if (zr->codec_mode == BUZ_MODE_MOTION_DECOMPRESS) {
		zr->jpg_seq_num++;
	}
	while (zr->jpg_dma_tail < zr->jpg_dma_head) {
		if (zr->params.TmpDcm == 1)
			i = (zr->jpg_dma_tail -
			     zr->jpg_err_shift) & BUZ_MASK_STAT_COM;
		else
			i = ((zr->jpg_dma_tail -
			      zr->jpg_err_shift) & 1) * 2 + 1;

		stat_com = zr->stat_com[i];

		if ((stat_com & 1) == 0) {
			return;
		}
		frame = zr->jpg_pend[zr->jpg_dma_tail & BUZ_MASK_FRAME];
		gbuf = &zr->jpg_gbuf[frame];
		do_gettimeofday(&gbuf->bs.timestamp);

		if (zr->codec_mode == BUZ_MODE_MOTION_COMPRESS) {
			gbuf->bs.length = (stat_com & 0x7fffff) >> 1;

			/* update sequence number with the help of the counter in stat_com */

			seq = ((stat_com >> 24) + zr->jpg_err_seq) & 0xff;
			dif = (seq - zr->jpg_seq_num) & 0xff;
			zr->jpg_seq_num += dif;
		} else {
			gbuf->bs.length = 0;
		}
		gbuf->bs.seq =
		    zr->params.TmpDcm ==
		    2 ? (zr->jpg_seq_num >> 1) : zr->jpg_seq_num;
		gbuf->state = BUZ_STATE_DONE;

		zr->jpg_dma_tail++;
	}
}

static void error_handler(struct zoran *zr, u32 astat, u32 stat)
{
	/* This is JPEG error handling part */
	if ((zr->codec_mode != BUZ_MODE_MOTION_COMPRESS)
	    && (zr->codec_mode != BUZ_MODE_MOTION_DECOMPRESS)) {
		//printk(KERN_ERR "%s: Internal error: error handling request in mode %d\n", zr->name, zr->codec_mode);
		return;
	}
	if ((stat & 1) == 0
	    && zr->codec_mode == BUZ_MODE_MOTION_COMPRESS
	    && zr->jpg_dma_tail - zr->jpg_que_tail >= zr->jpg_nbufs) {
		/* No free buffers... */
		zoran_reap_stat_com(zr);
		zoran_feed_stat_com(zr);
		wake_up_interruptible(&zr->jpg_capq);
		zr->JPEG_missed = 0;
		return;
	}
	if (zr->JPEG_error != 1) {
		/*
		 * First entry: error just happened during normal operation
		 * 
		 * In BUZ_MODE_MOTION_COMPRESS:
		 * 
		 * Possible glitch in TV signal. In this case we should
		 * stop the codec and wait for good quality signal before
		 * restarting it to avoid further problems
		 * 
		 * In BUZ_MODE_MOTION_DECOMPRESS:
		 * 
		 * Bad JPEG frame: we have to mark it as processed (codec crashed
		 * and was not able to do it itself), and to remove it from queue.
		 */
		btand(~ZR36057_JMC_Go_en, ZR36057_JMC);
		udelay(1);
		stat =
		    stat | (post_office_read(zr, 7, 0) & 3) << 8 |
		    zr36060_read_8(zr, 0x008);
		btwrite(0, ZR36057_JPC);
		btor(ZR36057_MCTCR_CFlush, ZR36057_MCTCR);
		zr36060_reset(zr);
		zr36060_sleep(zr, 1);
		zr->JPEG_error = 1;
		zr->num_errors++;
		/* Report error */
#if(DEBUGLEVEL > 1)
		if (zr->num_errors <= 8) {
			long frame;
			frame =
			    zr->jpg_pend[zr->
					 jpg_dma_tail & BUZ_MASK_FRAME];
			printk(KERN_ERR
			       "%s: JPEG error stat=0x%08x(0x%08x) queue_state=%ld/%ld/%ld/%ld seq=%ld frame=%ld. Codec stopped. ",
			       zr->name, stat, zr->last_isr,
			       zr->jpg_que_tail, zr->jpg_dma_tail,
			       zr->jpg_dma_head, zr->jpg_que_head,
			       zr->jpg_seq_num, frame);
			printk("stat_com frames:");
			{
				int i, j;
				for (j = 0; j < BUZ_NUM_STAT_COM; j++) {
					for (i = 0; i < zr->jpg_nbufs; i++) {
						if (zr->stat_com[j] ==
						    zr->jpg_gbuf[i].
						    frag_tab_bus) {
							printk("% d->%d",
							       j, i);
						}
					}
				}
				printk("\n");
			}
		}
#endif
		/* Find an entry in stat_com and rotate contents */
		{
			int i;

			if (zr->params.TmpDcm == 1)
				i = (zr->jpg_dma_tail -
				     zr->
				     jpg_err_shift) & BUZ_MASK_STAT_COM;
			else
				i = ((zr->jpg_dma_tail -
				      zr->jpg_err_shift) & 1) * 2;
			if (zr->codec_mode == BUZ_MODE_MOTION_DECOMPRESS) {
				/* Mimic zr36067 operation */
				zr->stat_com[i] |= 1;
				if (zr->params.TmpDcm != 1)
					zr->stat_com[i + 1] |= 1;
				/* Refill */
				zoran_reap_stat_com(zr);
				zoran_feed_stat_com(zr);
				wake_up_interruptible(&zr->jpg_capq);
				/* Find an entry in stat_com again after refill */
				if (zr->params.TmpDcm == 1)
					i = (zr->jpg_dma_tail -
					     zr->
					     jpg_err_shift) &
					    BUZ_MASK_STAT_COM;
				else
					i = ((zr->jpg_dma_tail -
					      zr->jpg_err_shift) & 1) * 2;
			}
			if (i) {
				/* Rotate stat_comm entries to make current entry first */
				int j;
				u32 bus_addr[BUZ_NUM_STAT_COM];

				memcpy(bus_addr, zr->stat_com,
				       sizeof(bus_addr));
				for (j = 0; j < BUZ_NUM_STAT_COM; j++) {
					zr->stat_com[j] =
					    bus_addr[(i +
						      j) &
						     BUZ_MASK_STAT_COM];
				}
				zr->jpg_err_shift += i;
				zr->jpg_err_shift &= BUZ_MASK_STAT_COM;
			}
			if (zr->codec_mode == BUZ_MODE_MOTION_COMPRESS)
				zr->jpg_err_seq = zr->jpg_seq_num;	/* + 1; */
		}
	}
	/* Now the stat_comm buffer is ready for restart */
	{
		int status;

		status = 0;
		if (zr->codec_mode == BUZ_MODE_MOTION_COMPRESS)
			i2c_control_device(&zr->i2c,
					   I2C_DRIVERID_VIDEODECODER,
					   DECODER_GET_STATUS, &status);
		if (zr->codec_mode == BUZ_MODE_MOTION_DECOMPRESS
		    || (status & DECODER_STATUS_GOOD)) {
	    /********** RESTART code *************/
			zr36060_reset(zr);
			zr36060_set_cap(zr, zr->codec_mode);
			zr36057_set_jpg(zr, zr->codec_mode);
			jpeg_start(zr);
#if(DEBUGLEVEL > 1)
			if (zr->num_errors <= 8)
				printk(KERN_INFO "%s: Restart\n",
				       zr->name);
#endif
			zr->JPEG_missed = 0;
			zr->JPEG_error = 2;
	    /********** End RESTART code ***********/
		}
	}
}

static void zoran_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	u32 stat, astat;
	int count;
	struct zoran *zr;
	unsigned long flags;

	zr = (struct zoran *) dev_id;
	count = 0;

	if (zr->testing) {
		/* Testing interrupts */
		spin_lock_irqsave(&zr->lock, flags);
		while ((stat = count_reset_interrupt(zr))) {
			if (count++ > 100) {
				btand(~ZR36057_ICR_IntPinEn, ZR36057_ICR);
				printk(KERN_ERR
				       "%s: IRQ lockup while testing, isr=0x%08x, cleared int mask\n",
				       zr->name, stat);
				wake_up_interruptible(&zr->test_q);
			}
		}
		zr->last_isr = stat;
		spin_unlock_irqrestore(&zr->lock, flags);
		return;
	}

	spin_lock_irqsave(&zr->lock, flags);
	while (1) {
		/* get/clear interrupt status bits */
		stat = count_reset_interrupt(zr);
		astat = stat & IRQ_MASK;
		if (!astat) {
			break;
		}
		if (astat & cardvsync[zr->card]) {	// SW

			if (zr->codec_mode == BUZ_MODE_MOTION_DECOMPRESS
			    || zr->codec_mode ==
			    BUZ_MODE_MOTION_COMPRESS) {
				/* count missed interrupts */
				zr->JPEG_missed++;
			}
			//post_office_read(zr,1,0);
			/* Interrupts may still happen when zr->v4l_memgrab_active is switched off.
			   We simply ignore them */

			if (zr->v4l_memgrab_active) {

				/* A lot more checks should be here ... */
				if ((btread(ZR36057_VSSFGR) &
				     ZR36057_VSSFGR_SnapShot) == 0)
					printk(KERN_WARNING
					       "%s: BuzIRQ with SnapShot off ???\n",
					       zr->name);

				if (zr->v4l_grab_frame != NO_GRAB_ACTIVE) {
					/* There is a grab on a frame going on, check if it has finished */

					if ((btread(ZR36057_VSSFGR) &
					     ZR36057_VSSFGR_FrameGrab) ==
					    0) {
						/* it is finished, notify the user */

						zr->v4l_gbuf[zr->
							     v4l_grab_frame].
						    state = BUZ_STATE_DONE;
						zr->v4l_grab_frame =
						    NO_GRAB_ACTIVE;
						zr->v4l_grab_seq++;
						zr->v4l_pend_tail++;
					}
				}

				if (zr->v4l_grab_frame == NO_GRAB_ACTIVE)
					wake_up_interruptible(&zr->
							      v4l_capq);

				/* Check if there is another grab queued */

				if (zr->v4l_grab_frame == NO_GRAB_ACTIVE
				    && zr->v4l_pend_tail !=
				    zr->v4l_pend_head) {

					int frame =
					    zr->v4l_pend[zr->
							 v4l_pend_tail &
							 V4L_MASK_FRAME];
					u32 reg;

					zr->v4l_grab_frame = frame;

					/* Set zr36057 video front end and enable video */

					/* Buffer address */

					reg =
					    zr->v4l_gbuf[frame].
					    fbuffer_bus;
					btwrite(reg, ZR36057_VDTR);
					if (zr->video_interlace)
						reg += zr->gbpl;
					btwrite(reg, ZR36057_VDBR);

					/* video stride, status, and frame grab register */

					reg = 0;
					if (zr->video_interlace)
						reg += zr->gbpl;
					reg =
					    (reg <<
					     ZR36057_VSSFGR_DispStride);
					reg |= ZR36057_VSSFGR_VidOvf;
					reg |= ZR36057_VSSFGR_SnapShot;
					reg |= ZR36057_VSSFGR_FrameGrab;
					btwrite(reg, ZR36057_VSSFGR);

					btor(ZR36057_VDCR_VidEn,
					     ZR36057_VDCR);
				}
			}
		}
#if (IRQ_MASK & ZR36057_ISR_CodRepIRQ)
		if (astat & ZR36057_ISR_CodRepIRQ) {
			zr->intr_counter_CodRepIRQ++;
			IDEBUG(printk
			       (KERN_DEBUG "%s: ZR36057_ISR_CodRepIRQ\n",
				zr->name));
			btand(~ZR36057_ICR_CodRepIRQ, ZR36057_ICR);
		}
#endif				/* (IRQ_MASK & ZR36057_ISR_CodRepIRQ) */

#if (IRQ_MASK & ZR36057_ISR_JPEGRepIRQ)
		if (astat & ZR36057_ISR_JPEGRepIRQ) {

			if (zr->codec_mode == BUZ_MODE_MOTION_DECOMPRESS
			    || zr->codec_mode ==
			    BUZ_MODE_MOTION_COMPRESS) {
#if(DEBUGLEVEL > 1)
				if (!zr->frame_num || zr->JPEG_error) {
					printk(KERN_INFO
					       "%s: first frame ready: state=0x%08x odd_even=%d field_per_buff=%d delay=%d\n",
					       zr->name, stat,
					       zr->params.odd_even,
					       zr->params.field_per_buff,
					       zr->JPEG_missed);
					{
						char sc[] = "0000";
						char sv[5];
						int i;
						strcpy(sv, sc);
						for (i = 0; i < 4; i++) {
							if (zr->
							    stat_com[i] &
							    1)
								sv[i] =
								    '1';
						}
						sv[4] = 0;
						printk(KERN_INFO
						       "%s: stat_com=%s queue_state=%ld/%ld/%ld/%ld\n",
						       zr->name, sv,
						       zr->jpg_que_tail,
						       zr->jpg_dma_tail,
						       zr->jpg_dma_head,
						       zr->jpg_que_head);
					}
				} else {
					if (zr->JPEG_missed > zr->JPEG_max_missed)	// Get statistics
						zr->JPEG_max_missed =
						    zr->JPEG_missed;
					if (zr->JPEG_missed <
					    zr->JPEG_min_missed)
						zr->JPEG_min_missed =
						    zr->JPEG_missed;
				}
#endif
#if(DEBUGLEVEL > 2)
				if (zr->frame_num < 6) {
					int i;
					printk("%s: seq=%ld stat_com:",
					       zr->name, zr->jpg_seq_num);
					for (i = 0; i < 4; i++) {
						printk(" %08x",
						       zr->stat_com[i]);
					}
					printk("\n");
				}
#endif
				zr->frame_num++;
				zr->JPEG_missed = 0;
				zr->JPEG_error = 0;
				zoran_reap_stat_com(zr);
				zoran_feed_stat_com(zr);
				wake_up_interruptible(&zr->jpg_capq);
			}	//else {
			//      printk(KERN_ERR "%s: JPEG interrupt while not in motion (de)compress mode!\n", zr->name);
			//}
		}
#endif				/* (IRQ_MASK & ZR36057_ISR_JPEGRepIRQ) */

		if ((astat & cardjpegint[zr->card])	/* DATERR interrupt received                 */
		    ||zr->JPEG_missed > 25	/* Too many fields missed without processing */
		    || zr->JPEG_error == 1	/* We are already in error processing        */
		    || ((zr->codec_mode == BUZ_MODE_MOTION_DECOMPRESS)
			&& (zr->
			    frame_num & (zr->JPEG_missed >
					 zr->params.field_per_buff)))
		    /* fields missed during decompression */
		    ) {
			error_handler(zr, astat, stat);
		}

		count++;
		if (count > 10) {
			printk(KERN_WARNING "%s: irq loop %d\n", zr->name,
			       count);
			if (count > 20) {
				btand(~ZR36057_ICR_IntPinEn, ZR36057_ICR);
				printk(KERN_ERR
				       "%s: IRQ lockup, cleared int mask\n",
				       zr->name);
				break;
			}
		}
		zr->last_isr = stat;
	}
	spin_unlock_irqrestore(&zr->lock, flags);
}

/* Check a zoran_params struct for correctness, insert default params */

static int zoran_check_params(struct zoran *zr,
			      struct zoran_params *params)
{
	int err = 0, err0 = 0;

	/* insert constant params */

	params->major_version = MAJOR_VERSION;
	params->minor_version = MINOR_VERSION;

	/* Check input and norm: must be set by calling VIDIOCSCHAN only! */

	params->norm = zr->params.norm;
	params->input = zr->params.input;

	/* Check decimation, set default values for decimation = 1, 2, 4 */

	switch (params->decimation) {
	case 1:

		params->HorDcm = 1;
		params->VerDcm = 1;
		params->TmpDcm = 1;
		params->field_per_buff = 2;

		params->img_x = 0;
		params->img_y = 0;
		params->img_width = zr->timing->Wa;
		params->img_height = zr->timing->Ha / 2;
		break;

	case 2:

		params->HorDcm = 2;
		params->VerDcm = 1;
		params->TmpDcm = 2;
		params->field_per_buff = 1;

		params->img_x = 8;
		params->img_y = 0;
		params->img_width = zr->timing->Wa;
		params->img_height = zr->timing->Ha / 2;
		break;

	case 4:

		params->HorDcm = 4;
		params->VerDcm = 2;
		params->TmpDcm = 2;
		params->field_per_buff = 1;

		params->img_x = 8;
		params->img_y = 0;
		params->img_width = zr->timing->Wa;
		params->img_height = zr->timing->Ha / 2;
		break;

	case 0:

		/* We have to check the data the user has set */

		if (params->HorDcm != 1 && params->HorDcm != 2
		    && params->HorDcm != 4)
			err0++;
		if (params->VerDcm != 1 && params->VerDcm != 2)
			err0++;
		if (params->TmpDcm != 1 && params->TmpDcm != 2)
			err0++;
		if (params->field_per_buff != 1
		    && params->field_per_buff != 2)
			err0++;

		if (params->img_x < 0)
			err0++;
		if (params->img_y < 0)
			err0++;
		if (params->img_width < 0)
			err0++;
		if (params->img_height < 0)
			err0++;
		if (params->img_x + params->img_width > zr->timing->Wa)
			err0++;
		if (params->img_y + params->img_height >
		    zr->timing->Ha / 2)
			err0++;
		if (params->HorDcm) {
			if (params->img_width % (16 * params->HorDcm) != 0)
				err0++;
			if (params->img_height % (8 * params->VerDcm) != 0)
				err0++;
		}

		if (err0) {
			DEBUG1(printk(KERN_ERR
			       "%s: SET PARAMS: error in params for decimation = 0\n",
			       zr->name));
			err++;
		}
		break;

	default:
		DEBUG1(printk(KERN_ERR
		       "%s: SET PARAMS: decimation = %d, must be 0, 1, 2 or 4\n",
		       zr->name, params->decimation));
		err++;
		break;
	}

	if (params->quality > 100)
		params->quality = 100;
	if (params->quality < 5)
		params->quality = 5;

	if (params->APPn < 0)
		params->APPn = 0;
	if (params->APPn > 15)
		params->APPn = 15;
	if (params->APP_len < 0)
		params->APP_len = 0;
	if (params->APP_len > 60)
		params->APP_len = 60;
	if (params->COM_len < 0)
		params->COM_len = 0;
	if (params->COM_len > 60)
		params->COM_len = 60;

	if (err)
		return -EINVAL;

	return 0;

}
static void zoran_open_init_params(struct zoran *zr)
{
	int i;

	/* Per default, map the V4L Buffers */

	zr->map_mjpeg_buffers = 0;

	/* User must explicitly set a window */

	zr->window_set = 0;

	zr->window.x = 0;
	zr->window.y = 0;
	zr->window.width = 0;
	zr->window.height = 0;
	zr->window.chromakey = 0;
	zr->window.flags = 0;
	zr->window.clips = NULL;
	zr->window.clipcount = 0;

	zr->video_interlace = 0;

	zr->v4l_memgrab_active = 0;
	zr->v4l_overlay_active = 0;

	zr->v4l_grab_frame = NO_GRAB_ACTIVE;
	zr->v4l_grab_seq = 0;

	zr->gwidth = 0;
	zr->gheight = 0;
	zr->gformat = 0;
	zr->gbpl = 0;

	/* DMA ring stuff for V4L */

	zr->v4l_pend_tail = 0;
	zr->v4l_pend_head = 0;
	for (i = 0; i < v4l_nbufs; i++) {
		zr->v4l_gbuf[i].state = BUZ_STATE_USER;	/* nothing going on */
	}

	/* Set necessary params and call zoran_check_params to set the defaults */

	zr->params.decimation = 1;

	zr->params.quality = 50;	/* default compression factor 8 */
	if (zr->card != BUZ)
		zr->params.odd_even = 1;
	else
		zr->params.odd_even = 0;

	zr->params.APPn = 0;
	zr->params.APP_len = 0;	/* No APPn marker */
	for (i = 0; i < 60; i++)
		zr->params.APP_data[i] = 0;

	zr->params.COM_len = 0;	/* No COM marker */
	for (i = 0; i < 60; i++)
		zr->params.COM_data[i] = 0;

	zr->params.VFIFO_FB = 0;

	memset(zr->params.reserved, 0, sizeof(zr->params.reserved));

	zr->params.jpeg_markers = JPEG_MARKER_DHT | JPEG_MARKER_DQT;

	i = zoran_check_params(zr, &zr->params);
	if (i)
		printk(KERN_ERR
		       "%s: zoran_open_init_params internal error\n",
		       zr->name);

	clear_interrupt_counters(zr);
	zr->testing = 0;
}

/*
 *   Open a zoran card. Right now the flags stuff is just playing
 */

static int zoran_open(struct video_device *dev, int flags)
{
	struct zoran *zr = (struct zoran *) dev;
	//int one = 1;

	DEBUG1(printk
	       (KERN_INFO "%s: zoran_open, %s pid=[%d]\n", zr->name,
		current->comm, current->pid));

	switch (flags) {

	case 0:
		if (zr->user > 1) {
			DEBUG1(printk(KERN_WARNING
			       "%s: zoran_open: Buz is allready in use\n",
			       zr->name));
			return -EBUSY;
		}
		zr->user++;

		if (zr->user == 1 && v4l_fbuffer_alloc(zr) < 0) {
			zr->user--;
			printk(KERN_ERR
			       "%s: zoran_open: v4l_fbuffer_alloc failed\n",
			       zr->name);
			return -ENOMEM;
		}

		/* default setup */

		if (zr->user == 1) {	/* First device open */
			zoran_open_init_params(zr);

			zr36057_enable_jpg(zr, BUZ_MODE_IDLE);

			btwrite(IRQ_MASK, ZR36057_ISR);	// Clears interrupts
			btor(ZR36057_ICR_IntPinEn, ZR36057_ICR);
			/* FIXME: Don't do it this way, use the
			 * video_device->fops registration for a sane
			 * implementation of multiple opens */
			dev->users--;	/* Allow second open */
		}

		break;

	default:
		DEBUG1(printk(KERN_WARNING
		       "%s: zoran_open: flags = 0x%x not yet supported\n",
		       zr->name, flags));
		return -EBUSY;
		break;

	}
	MOD_INC_USE_COUNT;
	return 0;
}

static void zoran_close(struct video_device *dev)
{
	struct zoran *zr = (struct zoran *) dev;
	int zero = 0, two = 2;

	DEBUG1(printk
	       (KERN_INFO "%s: zoran_close, %s pid=[%d]\n", zr->name,
		current->comm, current->pid));
	/* Clean up JPEG process */

	wake_up_interruptible(&zr->jpg_capq);
	zr36057_enable_jpg(zr, BUZ_MODE_IDLE);
	jpg_fbuffer_free(zr);
	zr->jpg_nbufs = 0;

	if (zr->user == 1) {	/* Last process */
		/* disable interrupts */
		btand(~ZR36057_ICR_IntPinEn, ZR36057_ICR);

#if(DEBUGLEVEL > 1)
		print_interrupts(zr);
#endif
		/* Overlay off */
		wake_up_interruptible(&zr->v4l_capq);
		zr36057_set_memgrab(zr, 0);
		if (zr->v4l_overlay_active)
			zr36057_overlay(zr, 0);
		v4l_fbuffer_free(zr);

		if (!pass_through) {	/* Switch to color bar */
			set_videobus_enable(zr, 0);
			i2c_control_device(&zr->i2c,
					   I2C_DRIVERID_VIDEODECODER,
					   DECODER_ENABLE_OUTPUT, &zero);
			i2c_control_device(&zr->i2c,
					   I2C_DRIVERID_VIDEOENCODER,
					   ENCODER_SET_INPUT, &two);
			set_videobus_enable(zr, 1);
		}
	}

	dev->users++;
	zr->user--;

	MOD_DEC_USE_COUNT;
	DEBUG2(printk(KERN_INFO "%s: zoran_close done\n", zr->name));
}


static long zoran_read(struct video_device *dev, char *buf,
		       unsigned long count, int nonblock)
{
	return -EINVAL;
}

static long zoran_write(struct video_device *dev, const char *buf,
			unsigned long count, int nonblock)
{
	return -EINVAL;
}

/*
 *   ioctl routine
 */

static int do_zoran_ioctl(struct zoran *zr, unsigned int cmd,
		       void *arg)
{
	switch (cmd) {

	case VIDIOCGCAP:
		{
			struct video_capability b;
			DEBUG2(printk("%s: ioctl VIDIOCGCAP\n", zr->name));

			strncpy(b.name, zr->video_dev.name,
				sizeof(b.name));
			b.type =
			    VID_TYPE_CAPTURE | VID_TYPE_OVERLAY |
			    VID_TYPE_CLIPPING | VID_TYPE_FRAMERAM |
			    VID_TYPE_SCALES;
			/* theoretically we could also flag VID_TYPE_SUBCAPTURE
			   but this is not even implemented in the BTTV driver */

			if (zr->card == DC10 || zr->card == DC10plus) {
				b.channels = 3;	/* composite, svhs, internal */
			} else {
				b.channels = 2;	/* composite, svhs */
			}
			b.audios = 0;
			b.maxwidth = BUZ_MAX_WIDTH;
			b.maxheight = BUZ_MAX_HEIGHT;
			b.minwidth = BUZ_MIN_WIDTH;
			b.minheight = BUZ_MIN_HEIGHT;
			if (copy_to_user(arg, &b, sizeof(b))) {
				return -EFAULT;
			}
			return 0;
		}
		break;

	case VIDIOCGCHAN:
		{
			struct video_channel v;

			if (copy_from_user(&v, arg, sizeof(v))) {
				return -EFAULT;
			}
			DEBUG2(printk
			       ("%s: ioctl VIDIOCGCHAN for channel %d\n",
				zr->name, v.channel));
			switch (v.channel) {
			case 0:
				strcpy(v.name, "Composite");
				break;
			case 1:
				strcpy(v.name, "SVHS");
				break;
			case 2:
				if (zr->card == DC10
				    || zr->card == DC10plus) {
					strcpy(v.name, "Internal/comp");
					break;
				}
			default:
				DEBUG1(printk(KERN_ERR
				       "%s: VIDIOCGCHAN on not existing channel %d\n",
				       zr->name, v.channel));
				return -EINVAL;
			}
			v.tuners = 0;
			v.flags = 0;
			v.type = VIDEO_TYPE_CAMERA;
			v.norm = zr->params.norm;
			if (copy_to_user(arg, &v, sizeof(v))) {
				return -EFAULT;
			}
			return 0;
		}
		break;

		/* RJ: the documentation at http://roadrunner.swansea.linux.org.uk/v4lapi.shtml says:

		 * "The VIDIOCSCHAN ioctl takes an integer argument and switches the capture to this input."
		 *                                 ^^^^^^^
		 * The famos BTTV driver has it implemented with a struct video_channel argument
		 * and we follow it for compatibility reasons
		 *
		 * BTW: this is the only way the user can set the norm!
		 */

	case VIDIOCSCHAN:
		{
			struct video_channel v;
			int input;
			int on, res;
			int encoder_norm;

			if (copy_from_user(&v, arg, sizeof(v))) {
				return -EFAULT;
			}

			if (zr->codec_mode != BUZ_MODE_IDLE) {
				if (v.norm != zr->params.norm
				    || v.channel != zr->params.input) {
					DEBUG1(printk(KERN_ERR
					       "%s: VIDIOCSCHAN called while the card in capture/playback mode\n",
					       zr->name));
					return -EINVAL;
				} else {
					DEBUG1(printk(BUZ_WARNING
					       "%s: Warning: VIDIOCSCHAN called while the card in capture/playback mode\n",
					       zr->name));
				}
			}

			DEBUG2(printk
			       ("%s: ioctl VIDIOCSCHAN: channel=%d, norm=%d\n",
				zr->name, v.channel, v.norm));
			switch (v.channel) {
			case 0:
				if (zr->card == BUZ)
					input = 3;
				else
					input = 0;
				break;
			case 1:
				input = 7;
				break;
			case 2:
				if (zr->card == DC10
				    || zr->card == DC10plus) {
					input = 5;
					break;
				}
			default:
				DEBUG1(printk(KERN_ERR
				       "%s: VIDIOCSCHAN on not existing channel %d\n",
				       zr->name, v.channel));
				return -EINVAL;
				break;
			}

			if (lock_norm && v.norm != zr->params.norm) {
				if (lock_norm > 1) {
					DEBUG1(printk(KERN_WARNING
					       "%s: VIDIOCSCHAN: TV standard is locked, can not switch norm.\n",
					       zr->name));
					return -EINVAL;
				} else {
					DEBUG1(printk(KERN_WARNING
					       "%s: VIDIOCSCHAN: TV standard is locked, norm was not changed.\n",
					       zr->name));
					v.norm = zr->params.norm;
				}
			}
			
			if(v.norm >= 2)
				return -EINVAL;

			if (!cardnorms[zr->card][v.norm]) {
				DEBUG1(printk(KERN_ERR
				       "%s: VIDIOCSCHAN with not supported norm %d\n",
				       zr->name, v.norm));
				return -EOPNOTSUPP;
				break;
			}
			encoder_norm = v.norm;

			zr->params.norm = v.norm;
			zr->params.input = v.channel;
			zr->timing = cardnorms[zr->card][zr->params.norm];

			/* We switch overlay off and on since a change in the norm
			   needs different VFE settings */

			on = zr->v4l_overlay_active
			    && !zr->v4l_memgrab_active;
			if (on)
				zr36057_overlay(zr, 0);

			set_videobus_enable(zr, 0);
			i2c_control_device(&zr->i2c,
					   I2C_DRIVERID_VIDEODECODER,
					   DECODER_SET_INPUT, &input);
			i2c_control_device(&zr->i2c,
					   I2C_DRIVERID_VIDEODECODER,
					   DECODER_SET_NORM,
					   &zr->params.norm);
			i2c_control_device(&zr->i2c,
					   I2C_DRIVERID_VIDEOENCODER,
					   ENCODER_SET_NORM,
					   &encoder_norm);
			set_videobus_enable(zr, 1);

			if (on)
				zr36057_overlay(zr, 1);

			/* Make sure the changes come into effect */
			res = wait_grab_pending(zr);
			if (res)
				return res;

			return 0;
		}
		break;

	case VIDIOCGTUNER:
		{
			DEBUG1(printk(KERN_ERR
			       "%s: ioctl VIDIOCGTUNER not supported\n",
			       zr->name));
			return -EINVAL;
		}
		break;

	case VIDIOCSTUNER:
		{
			DEBUG1(printk(KERN_ERR
			       "%s: ioctl VIDIOCSTUNER not supported\n",
			       zr->name));
			return -EINVAL;
		}
		break;

	case VIDIOCGPICT:
		{
			struct video_picture p = zr->picture;

			DEBUG2(printk
			       ("%s: ioctl VIDIOCGPICT\n", zr->name));
			p.depth = zr->buffer.depth;
			switch (zr->buffer.depth) {
			case 15:
				p.palette = VIDEO_PALETTE_RGB555;
				break;

			case 16:
				p.palette = VIDEO_PALETTE_RGB565;
				break;

			case 24:
				p.palette = VIDEO_PALETTE_RGB24;
				break;

			case 32:
				p.palette = VIDEO_PALETTE_RGB32;
				break;
			}

			if (copy_to_user(arg, &p, sizeof(p))) {
				return -EFAULT;
			}
			return 0;
		}
		break;

	case VIDIOCSPICT:
		{
			struct video_picture p;

			if (copy_from_user(&p, arg, sizeof(p))) {
				return -EFAULT;
			}
			i2c_control_device(&zr->i2c,
					   I2C_DRIVERID_VIDEODECODER,
					   DECODER_SET_PICTURE, &p);
			DEBUG2(printk
			       ("%s: ioctl VIDIOCSPICT bri=%d hue=%d col=%d con=%d dep=%d pal=%d\n",
				zr->name, p.brightness, p.hue, p.colour,
				p.contrast, p.depth, p.palette));
			/* The depth and palette values have no meaning to us,
			   should we return  -EINVAL if they don't fit ? */
			zr->picture = p;
			return 0;
		}
		break;

	case VIDIOCCAPTURE:
		{
			int v, res;

			if (copy_from_user(&v, arg, sizeof(v))) {
				return -EFAULT;
			}
			DEBUG2(printk
			       ("%s: ioctl VIDIOCCAPTURE: %d\n", zr->name,
				v));

			/* If there is nothing to do, return immediatly */

			if ((v && zr->v4l_overlay_active)
			    || (!v && !zr->v4l_overlay_active))
				return 0;

			if (v == 0) {
				zr->v4l_overlay_active = 0;
				if (!zr->v4l_memgrab_active)
					zr36057_overlay(zr, 0);
				/* When a grab is running, the video simply won't be switched on any more */
			} else {
				if (!zr->buffer_set || !zr->window_set) {
					DEBUG1(printk(KERN_ERR
					       "%s: VIDIOCCAPTURE: buffer or window not set\n",
					       zr->name));
					return -EINVAL;
				}
				zr->v4l_overlay_active = 1;
				if (!zr->v4l_memgrab_active)
					zr36057_overlay(zr, 1);
				/* When a grab is running, the video will be switched on when grab is finished */
			}
			/* Make sure the changes come into effect */
			res = wait_grab_pending(zr);
			if (res)
				return res;
			return 0;
		}
		break;

	case VIDIOCGWIN:
		{
			DEBUG2(printk("%s: ioctl VIDIOCGWIN\n", zr->name));
			if (copy_to_user
			    (arg, &zr->window, sizeof(zr->window))) {
				return -EFAULT;
			}
			return 0;
		}
		break;

	case VIDIOCSWIN:
		{
			struct video_clip *vcp;
			struct video_window vw;
			struct tvnorm *tvn;
			int on, end, res, Wa, Ha;

			tvn = zr->timing;

			Wa = tvn->Wa;
			Ha = tvn->Ha;

			if (copy_from_user(&vw, arg, sizeof(vw))) {
				return -EFAULT;
			}

			DEBUG2(printk
			       ("%s: ioctl VIDIOCSWIN: x=%d y=%d w=%d h=%d clipcount=%d\n",
				zr->name, vw.x, vw.y, vw.width, vw.height,
				vw.clipcount));

			if (!zr->buffer_set) {
				DEBUG1(printk(KERN_ERR
				       "%s: VIDIOCSWIN: frame buffer has to be set first\n",
				       zr->name));
				return -EINVAL;
			}

			/*
			 * The video front end needs 4-byte alinged line sizes, we correct that
			 * silently here if necessary
			 */

			if (zr->buffer.depth == 15
			    || zr->buffer.depth == 16) {
				end = (vw.x + vw.width) & ~1;	/* round down */
				vw.x = (vw.x + 1) & ~1;	/* round up */
				vw.width = end - vw.x;
			}

			if (zr->buffer.depth == 24) {
				end = (vw.x + vw.width) & ~3;	/* round down */
				vw.x = (vw.x + 3) & ~3;	/* round up */
				vw.width = end - vw.x;
			}

			if (vw.width > Wa)
				vw.width = Wa;
			if (vw.height > Ha)
				vw.height = Ha;

			/* Check for vaild parameters */
			if (vw.width < BUZ_MIN_WIDTH
			    || vw.height < BUZ_MIN_HEIGHT
			    || vw.width > BUZ_MAX_WIDTH
			    || vw.height > BUZ_MAX_HEIGHT) {
				DEBUG1(printk(KERN_ERR
				       "%s: VIDIOCSWIN: width = %d or height = %d invalid\n",
				       zr->name, vw.width, vw.height));
				return -EINVAL;
			}

			zr->window.x = vw.x;
			zr->window.y = vw.y;
			zr->window.width = vw.width;
			zr->window.height = vw.height;
			zr->window.chromakey = 0;
			zr->window.flags = 0;	// RJ: Is this intended for interlace on/off ?
			zr->window.clips = NULL;

			/*
			 * If an overlay is running, we have to switch it off
			 * and switch it on again in order to get the new settings in effect.
			 *
			 * We also want to avoid that the overlay mask is written
			 * when an overlay is running.
			 */

			on = zr->v4l_overlay_active
			    && !zr->v4l_memgrab_active;
			if (on)
				zr36057_overlay(zr, 0);

			/*
			 *   Write the overlay mask if clips are wanted.
			 */
			 
			if (vw.clipcount < 0 || vw.clipcount > 2048)
				return -EINVAL;
			if (vw.clipcount) {
				vcp =
				    vmalloc(sizeof(struct video_clip) *
					    (vw.clipcount + 4));
				if (vcp == NULL) {
					printk(KERN_ERR
					       "%s: zoran_ioctl: Alloc of clip mask failed\n",
					       zr->name);
					return -ENOMEM;
				}
				if (copy_from_user
				    (vcp, vw.clips,
				     sizeof(struct video_clip) *
				     vw.clipcount)) {
					vfree(vcp);
					return -EFAULT;
				}
				write_overlay_mask(zr, vcp, vw.clipcount);
				vfree(vcp);
			}
			zr->window.clipcount = vw.clipcount;
	
			if (on)
				zr36057_overlay(zr, 1);
			zr->window_set = 1;

			/* Make sure the changes come into effect */
			res = wait_grab_pending(zr);
			if (res)
				return res;

			return 0;
		}
		break;

	case VIDIOCGFBUF:
		{
			DEBUG2(printk
			       ("%s: ioctl VIDIOCGFBUF\n", zr->name));
			if (copy_to_user
			    (arg, &zr->buffer, sizeof(zr->buffer))) {
				return -EFAULT;
			}
			return 0;
		}
		break;

	case VIDIOCSFBUF:
		{
			struct video_buffer v;

			/* RJ: Isn't this too restrictive? As long as the user doesn't set
			   the base address it shouldn't be too dangerous */

			if (!capable(CAP_SYS_ADMIN)) {
				DEBUG1(printk(KERN_ERR
				       "%s: Only the superuser may issue VIDIOCSFBUF ioctl\n",
				       zr->name));
				return -EPERM;
			}
			if (copy_from_user(&v, arg, sizeof(v))) {
				return -EFAULT;
			}
			DEBUG2(printk
			       ("%s: ioctl VIDIOCSFBUF: base=0x%x w=%d h=%d depth=%d bpl=%d\n",
				zr->name, (u32) v.base, v.width, v.height,
				v.depth, v.bytesperline));
			if (zr->v4l_overlay_active) {
				/* Has the user gotten crazy ... ? */
				DEBUG1(printk(KERN_ERR
				       "%s: VIDIOCSFBUF not allowed when overlay active\n",
				       zr->name));
				return -EINVAL;
			}
			if (v.depth != 15 && v.depth != 16 && v.depth != 24
			    && v.depth != 32) {
				DEBUG1(printk(KERN_ERR
				       "%s: VIDIOCSFBUF: depth=%d not supported\n",
				       zr->name, v.depth));
				return -EINVAL;
			}
			if (v.height <= 0 || v.width <= 0
			    || v.bytesperline <= 0) {
				DEBUG1(printk(KERN_ERR
				       "%s: VIDIOCSFBUF: invalid height/width/bpl value\n",
				       zr->name));
				return -EINVAL;
			}
			if (v.bytesperline & 3) {
				DEBUG1(printk(KERN_ERR
				       "%s: VIDIOCSFBUF: bytesperline must be 4-byte aligned\n",
				       zr->name));
				return -EINVAL;
			}
			if (v.base) {
				zr->buffer.base =
				    (void *) ((unsigned long) v.base & ~3);
			}
			zr->buffer.height = v.height;
			zr->buffer.width = v.width;
			zr->buffer.depth = v.depth;
			zr->buffer.bytesperline = v.bytesperline;

			if (zr->buffer.base)
				zr->buffer_set = 1;
			zr->window_set = 0;	/* The user should set new window parameters */
			return 0;
		}
		break;

		/* RJ: what is VIDIOCKEY intended to do ??? */

	case VIDIOCKEY:
		{
			/* Will be handled higher up .. */
			DEBUG2(printk("%s: ioctl VIDIOCKEY\n", zr->name));
			return 0;
		}
		break;

	case VIDIOCGFREQ:
		{
			DEBUG1(printk(KERN_ERR
			       "%s: ioctl VIDIOCGFREQ not supported\n",
			       zr->name));
			return -EINVAL;
		}
		break;

	case VIDIOCSFREQ:
		{
			DEBUG1(printk(KERN_ERR
			       "%s: ioctl VIDIOCSFREQ not supported\n",
			       zr->name));
			return -EINVAL;
		}
		break;

	case VIDIOCGAUDIO:
		{
			DEBUG1(printk(KERN_ERR
			       "%s: ioctl VIDIOCGAUDIO not supported\n",
			       zr->name));
			return -EINVAL;
		}
		break;

	case VIDIOCSAUDIO:
		{
			DEBUG1(printk(KERN_ERR
			       "%s: ioctl VIDIOCSAUDIO not supported\n",
			       zr->name));
			return -EINVAL;
		}
		break;

	case VIDIOCSYNC:
		{
			int v;

			if (copy_from_user(&v, arg, sizeof(v))) {
				return -EFAULT;
			}
			DEBUG3(printk
			       ("%s: ioctl VIDIOCSYNC %d\n", zr->name, v));
			return v4l_sync(zr, v);
		}
		break;

	case VIDIOCMCAPTURE:
		{
			struct video_mmap vm;

			if (copy_from_user
			    ((void *) &vm, (void *) arg, sizeof(vm))) {
				return -EFAULT;
			}
			DEBUG2(printk
			       ("%s: ioctl VIDIOCMCAPTURE frame=%d geom=%dx%d fmt=%d\n",
				zr->name, vm.frame, vm.width, vm.height,
				vm.format));
			return v4l_grab(zr, &vm);
		}
		break;

	case VIDIOCGMBUF:
		{
			struct video_mbuf vm;
			int i;

			DEBUG2(printk
			       ("%s: ioctl VIDIOCGMBUF\n", zr->name));
			vm.size = v4l_nbufs * v4l_bufsize;
			vm.frames = v4l_nbufs;
			for (i = 0; i < v4l_nbufs; i++) {
				vm.offsets[i] = i * v4l_bufsize;
			}

			/* The next mmap will map the V4L buffers */
			zr->map_mjpeg_buffers = 0;

			if (copy_to_user(arg, &vm, sizeof(vm))) {
				return -EFAULT;
			}
			return 0;
		}
		break;

	case VIDIOCGUNIT:
		{
			struct video_unit vu;

			DEBUG2(printk
			       ("%s: ioctl VIDIOCGUNIT\n", zr->name));
			vu.video = zr->video_dev.minor;
			vu.vbi = VIDEO_NO_UNIT;
			vu.radio = VIDEO_NO_UNIT;
			vu.audio = VIDEO_NO_UNIT;
			vu.teletext = VIDEO_NO_UNIT;
			if (copy_to_user(arg, &vu, sizeof(vu))) {
				return -EFAULT;
			}
			return 0;
		}
		break;

		/*
		 * RJ: In principal we could support subcaptures for V4L grabbing.
		 *     Not even the famous BTTV driver has them, however.
		 *     If there should be a strong demand, one could consider
		 *     to implement them.
		 */
	case VIDIOCGCAPTURE:
		{
			DEBUG1(printk(KERN_ERR
			       "%s: ioctl VIDIOCGCAPTURE not supported\n",
			       zr->name));
			return -EINVAL;
		}
		break;

	case VIDIOCSCAPTURE:
		{
			DEBUG1(printk(KERN_ERR
			       "%s: ioctl VIDIOCSCAPTURE not supported\n",
			       zr->name));
			return -EINVAL;
		}
		break;

	case BUZIOC_G_PARAMS:
		{
			DEBUG2(printk
			       ("%s: ioctl BUZIOC_G_PARAMS\n", zr->name));

			if (copy_to_user
			    (arg, &(zr->params), sizeof(zr->params))) {
				return -EFAULT;
			}
			return 0;
		}
		break;

	case BUZIOC_S_PARAMS:
		{
			struct zoran_params bp;
			/* int input, on; */

			if (zr->codec_mode != BUZ_MODE_IDLE) {
				DEBUG1(printk(KERN_ERR
				       "%s: BUZIOC_S_PARAMS called but Buz in capture/playback mode\n",
				       zr->name));
				return -EINVAL;
			}

			if (copy_from_user(&bp, arg, sizeof(bp))) {
				return -EFAULT;
			}
			DEBUG2(printk
			       ("%s: ioctl BUZIOC_S_PARAMS\n", zr->name));

			/* Check the params first before overwriting our internal values */

			if (zoran_check_params(zr, &bp))
				return -EINVAL;

			zr->params = bp;

			/* Make changes of input and norm go into effect immediatly */

			/* We switch overlay off and on since a change in the norm
			   needs different VFE settings */

			if (copy_to_user(arg, &bp, sizeof(bp))) {
				return -EFAULT;
			}
			return 0;
		}
		break;

	case BUZIOC_REQBUFS:
		{
			struct zoran_requestbuffers br;

			if (zr->jpg_buffers_allocated) {
				DEBUG1(printk(KERN_ERR
				       "%s: BUZIOC_REQBUFS: buffers allready allocated\n",
				       zr->name));
				return -EINVAL;
			}
			if (copy_from_user(&br, arg, sizeof(br))) {
				return -EFAULT;
			}
			DEBUG2(printk
			       ("%s: ioctl BUZIOC_REQBUFS count = %lu size=%lu\n",
				zr->name, br.count, br.size));

			/* Enforce reasonable lower and upper limits */
			if (br.count < 4)
				br.count = 4;	/* Could be choosen smaller */
			if (br.count > BUZ_MAX_FRAME)
				br.count = BUZ_MAX_FRAME;
			br.size = PAGE_ALIGN(br.size);
			if (br.size < 8192)
				br.size = 8192;	/* Arbitrary */
			/* br.size is limited by 1 page for the stat_com tables to a Maximum of 2 MB */
			if (br.size > (512 * 1024))
				br.size = (512 * 1024);	/* 512 K should be enough */
			if (zr->need_contiguous
			    && br.size > MAX_KMALLOC_MEM)
				br.size = MAX_KMALLOC_MEM;

			zr->jpg_nbufs = br.count;
			zr->jpg_bufsize = br.size;

			if (jpg_fbuffer_alloc(zr))
				return -ENOMEM;

			/* The next mmap will map the MJPEG buffers */
			zr->map_mjpeg_buffers = 1;

			if (copy_to_user(arg, &br, sizeof(br))) {
				return -EFAULT;
			}
			return 0;
		}
		break;

	case BUZIOC_QBUF_CAPT:
		{
			int nb;

			if (copy_from_user
			    ((void *) &nb, (void *) arg, sizeof(int))) {
				return -EFAULT;
			}
			DEBUG4(printk
			       ("%s: ioctl BUZIOC_QBUF_CAPT %d\n",
				zr->name, nb));
			return jpg_qbuf(zr, nb, BUZ_MODE_MOTION_COMPRESS);
		}
		break;

	case BUZIOC_QBUF_PLAY:
		{
			int nb;

			if (copy_from_user
			    ((void *) &nb, (void *) arg, sizeof(int))) {
				return -EFAULT;
			}
			DEBUG4(printk
			       ("%s: ioctl BUZIOC_QBUF_PLAY %d\n",
				zr->name, nb));
			return jpg_qbuf(zr, nb,
					BUZ_MODE_MOTION_DECOMPRESS);
		}
		break;

	case BUZIOC_SYNC:
		{
			struct zoran_sync bs;
			int res;

			DEBUG4(printk
			       ("%s: ioctl BUZIOC_SYNC\n", zr->name));
			res = jpg_sync(zr, &bs);
			if (copy_to_user(arg, &bs, sizeof(bs))) {
				return -EFAULT;
			}
			return res;
		}
		break;

	case BUZIOC_G_STATUS:
		{
			struct zoran_status bs;
			int norm, input, status;

			if (zr->codec_mode != BUZ_MODE_IDLE) {
				DEBUG1(printk(KERN_ERR
				       "%s: BUZIOC_G_STATUS called but Buz in capture/playback mode\n",
				       zr->name));
				return -EINVAL;
			}

			if (copy_from_user(&bs, arg, sizeof(bs))) {
				return -EFAULT;
			}
			DEBUG2(printk
			       ("%s: ioctl BUZIOC_G_STATUS\n", zr->name));

			switch (bs.input) {
			case 0:
				if (zr->card == BUZ)
					input = 3;
				else
					input = 0;
				break;
			case 1:
				input = 7;
				break;
			default:
				DEBUG1(printk(KERN_ERR
				       "%s: BUZIOC_G_STATUS on not existing input %d\n",
				       zr->name, bs.input));
				return -EINVAL;
			}

			/* Set video norm to VIDEO_MODE_AUTO */

			norm = VIDEO_MODE_AUTO;
			set_videobus_enable(zr, 0);
			i2c_control_device(&zr->i2c,
					   I2C_DRIVERID_VIDEODECODER,
					   DECODER_SET_INPUT, &input);
			i2c_control_device(&zr->i2c,
					   I2C_DRIVERID_VIDEODECODER,
					   DECODER_SET_NORM, &norm);
			set_videobus_enable(zr, 1);

			/* sleep 1 second */

			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule_timeout(HZ);

			/* Get status of video decoder */

			i2c_control_device(&zr->i2c,
					   I2C_DRIVERID_VIDEODECODER,
					   DECODER_GET_STATUS, &status);
			bs.signal = (status & DECODER_STATUS_GOOD) ? 1 : 0;

			if (status & DECODER_STATUS_NTSC)
				bs.norm = VIDEO_MODE_NTSC;
			else if (status & DECODER_STATUS_SECAM)
				bs.norm = VIDEO_MODE_SECAM;
			else
				bs.norm = VIDEO_MODE_PAL;

			bs.color = (status & DECODER_STATUS_COLOR) ? 1 : 0;

			/* restore previous input and norm */
			if (zr->card == BUZ)
				input = zr->params.input == 0 ? 3 : 7;
			else
				input = zr->params.input == 0 ? 0 : 7;
			set_videobus_enable(zr, 0);
			i2c_control_device(&zr->i2c,
					   I2C_DRIVERID_VIDEODECODER,
					   DECODER_SET_INPUT, &input);
			i2c_control_device(&zr->i2c,
					   I2C_DRIVERID_VIDEODECODER,
					   DECODER_SET_NORM,
					   &zr->params.norm);
			set_videobus_enable(zr, 1);

			if (copy_to_user(arg, &bs, sizeof(bs))) {
				return -EFAULT;
			}
			return 0;
		}
		break;

	default:
		DEBUG1(printk
		       ("%s: UNKNOWN ioctl cmd: 0x%x\n", zr->name, cmd));
		return -ENOIOCTLCMD;
	}
	return 0;
}

static int zoran_ioctl(struct video_device *dev, unsigned int cmd, void *arg)
{
	struct zoran *zr = (struct zoran *) dev;
	int err;
	
	down(&zr->sem);
	err = do_zoran_ioctl(zr, cmd, arg);
	up(&zr->sem);
	
	return err;
}

/*
 *   This maps the buffers to user space.
 *
 *   Depending on the state of zr->map_mjpeg_buffers
 *   the V4L or the MJPEG buffers are mapped
 *
 */

static int do_zoran_mmap(struct zoran *zr, const char *adr,
		      unsigned long size)
{
	unsigned long start = (unsigned long) adr;
	unsigned long page, pos, todo, fraglen;
	int i, j;

	DEBUG2(printk
	       (KERN_INFO "%s: mmap at 0x%08lx, size %lu\n", zr->name,
		start, size));
	if (zr->map_mjpeg_buffers) {
		/* Map the MJPEG buffers */

		if (!zr->jpg_buffers_allocated) {
			DEBUG1(printk(KERN_ERR
			       "%s: zoran_mmap(MJPEG): buffers not yet allocated\n",
			       zr->name));
			return -ENOMEM;
		}

		if (size > zr->jpg_nbufs * zr->jpg_bufsize) {
			DEBUG1(printk(KERN_ERR
			       "%s: zoran_mmap(MJPEG): Max size is %lu - you wanted %lu\n",
			       zr->name, zr->jpg_nbufs * zr->jpg_bufsize,
			       size));
			return -EINVAL;
		}

		if (size != zr->jpg_nbufs * zr->jpg_bufsize)
			DEBUG1(printk(KERN_WARNING
			       "%s: zoran_mmap(MJPEG): Expected %lu - you wanted %lu\n",
			       zr->name, zr->jpg_nbufs * zr->jpg_bufsize,
			       size));

		for (i = 0; i < zr->jpg_nbufs; i++) {
			for (j = 0; j < zr->jpg_bufsize / PAGE_SIZE; j++) {
				fraglen =
				    (zr->jpg_gbuf[i].
				     frag_tab[2 * j + 1] & ~1) << 1;
				todo = size;
				if (todo > fraglen)
					todo = fraglen;
				pos =
				    (unsigned long) zr->jpg_gbuf[i].
				    frag_tab[2 * j];
				page = virt_to_phys(bus_to_virt(pos));	/* should just be pos on i386 */
				if (remap_page_range
				    (start, page, todo, PAGE_SHARED)) {
					printk(KERN_ERR
					       "%s: zoran_mmap(V4L): remap_page_range failed\n",
					       zr->name);
					return -EAGAIN;
				}
				size -= todo;
				start += todo;
				if (size == 0)
					break;
				if (zr->jpg_gbuf[i].
				    frag_tab[2 * j + 1] & 1)
					break;	/* was last fragment */
			}
			if (size == 0)
				break;
		}
	} else {
		/* Map the V4L buffers */

		if (size > v4l_nbufs * v4l_bufsize) {
			DEBUG1(printk(KERN_ERR
			       "%s: zoran_mmap(V4L): Max size is %d - you wanted %ld\n",
			       zr->name, v4l_nbufs * v4l_bufsize, size));
			return -EINVAL;
		}

		if (size != v4l_nbufs * v4l_bufsize)
			DEBUG1(printk(KERN_WARNING
			       "%s: zoran_mmap(V4L): Expected %d - you wanted %ld\n",
			       zr->name, v4l_nbufs * v4l_bufsize, size));

		for (i = 0; i < v4l_nbufs; i++) {
			todo = size;
			if (todo > v4l_bufsize)
				todo = v4l_bufsize;
			page = zr->v4l_gbuf[i].fbuffer_phys;
			DEBUG2(printk
			       ("V4L remap page range %d 0x%lx %ld to 0x%lx\n",
				i, page, todo, start));
			if (remap_page_range
			    (start, page, todo, PAGE_SHARED)) {
				printk(KERN_ERR
				       "%s: zoran_mmap(V4L): remap_page_range failed\n",
				       zr->name);
				return -EAGAIN;
			}
			size -= todo;
			start += todo;
			if (size == 0)
				break;
		}
	}
	return 0;
}

static int zoran_mmap(struct video_device *dev, const char *adr,
		      unsigned long size)
{
	int err;
	struct zoran *zr = (struct zoran *) dev;
	
	down(&zr->sem);
	err = do_zoran_mmap(zr, adr, size);
	up(&zr->sem);
		
	return err;
}

static int zoran_init_done(struct video_device *dev)
{
	return 0;
}

static struct video_device zoran_template = {
	owner:		THIS_MODULE,
	name:		ZORAN_NAME,
	type:		VID_TYPE_CAPTURE | VID_TYPE_OVERLAY | VID_TYPE_CLIPPING |
			VID_TYPE_FRAMERAM | VID_TYPE_SCALES | VID_TYPE_SUBCAPTURE,
	hardware:	ZORAN_HARDWARE,
	open:		zoran_open,
	close:		zoran_close,
	read:		zoran_read,
	write:		zoran_write,
	ioctl:		zoran_ioctl,
	mmap:		zoran_mmap,
	initialize:	zoran_init_done,
};

/*
 * initialize video front end
 */
static void zr36057_init_vfe(struct zoran *zr)
{
	u32 reg;
	reg = btread(ZR36057_VFESPFR);
	reg |= ZR36057_VFESPFR_LittleEndian;
	reg &= ~ZR36057_VFESPFR_VCLKPol;
	reg |= ZR36057_VFESPFR_ExtFl;
	reg |= ZR36057_VFESPFR_TopField;
	btwrite(reg, ZR36057_VFESPFR);
	reg = btread(ZR36057_VDCR);
	if (triton || zr->revision <= 1)
		reg &= ~ZR36057_VDCR_Triton;
	else
		reg |= ZR36057_VDCR_Triton;
	btwrite(reg, ZR36057_VDCR);
}

static void test_interrupts(struct zoran *zr)
{
	int timeout, icr;

	clear_interrupt_counters(zr);
	zr->testing = 1;
	icr = btread(ZR36057_ICR);
	btwrite(0x78000000 | ZR36057_ICR_IntPinEn, ZR36057_ICR);
	timeout = interruptible_sleep_on_timeout(&zr->test_q, 1 * HZ);
	btwrite(0, ZR36057_ICR);
	btwrite(0x78000000, ZR36057_ISR);
	zr->testing = 0;
	printk(KERN_INFO "%s: Testing interrupts...\n", zr->name);
	if (timeout) {
		printk(": time spent: %d\n", 1 * HZ - timeout);
	}
	print_interrupts(zr);
	btwrite(icr, ZR36057_ICR);
}

static int zr36057_init(int i)
{
	struct zoran *zr = &zoran[i];
	unsigned long mem;
	unsigned mem_needed;
	int j;
	int two = 2;
	int zero = 0;

	printk(KERN_INFO "%s: Initializing card[%d], zr=%x\n", zr->name, i, (int) zr);

	/* default setup of all parameters which will persist beetween opens */

	zr->user = 0;

	init_waitqueue_head(&zr->v4l_capq);
	init_waitqueue_head(&zr->jpg_capq);
	init_waitqueue_head(&zr->test_q);

	zr->map_mjpeg_buffers = 0;	/* Map V4L buffers by default */

	zr->jpg_nbufs = 0;
	zr->jpg_bufsize = 0;
	zr->jpg_buffers_allocated = 0;

	zr->buffer_set = 0;	/* Flag if frame buffer has been set */
	zr->buffer.base = (void *) vidmem;
	zr->buffer.width = 0;
	zr->buffer.height = 0;
	zr->buffer.depth = 0;
	zr->buffer.bytesperline = 0;

	zr->params.norm = default_norm = (default_norm < 3 ? default_norm : VIDEO_MODE_PAL);	/* Avoid nonsense settings from user */
	if (!(zr->timing = cardnorms[zr->card][zr->params.norm])) {
		printk(KERN_WARNING
		       "%s: default TV statdard not supported by hardware. PAL will be used.\n",
		       zr->name);
		zr->params.norm = VIDEO_MODE_PAL;
		zr->timing = cardnorms[zr->card][zr->params.norm];
	}
	zr->params.input = default_input = (default_input ? 1 : 0);	/* Avoid nonsense settings from user */
	zr->video_interlace = 0;

	/* Should the following be reset at every open ? */

	zr->picture.colour = 32768;
	zr->picture.brightness = 32768;
	zr->picture.hue = 32768;
	zr->picture.contrast = 32768;
	zr->picture.whiteness = 0;
	zr->picture.depth = 0;
	zr->picture.palette = 0;

	for (j = 0; j < VIDEO_MAX_FRAME; j++) {
		zr->v4l_gbuf[i].fbuffer = 0;
		zr->v4l_gbuf[i].fbuffer_phys = 0;
		zr->v4l_gbuf[i].fbuffer_bus = 0;
	}

	zr->stat_com = 0;

	/* default setup (will be repeated at every open) */

	zoran_open_init_params(zr);

	/* allocate memory *before* doing anything to the hardware in case allocation fails */

	/* STAT_COM table and overlay mask */

	mem_needed = (BUZ_NUM_STAT_COM + ((BUZ_MAX_WIDTH + 31) / 32) * BUZ_MAX_HEIGHT) * 4;
	mem = (unsigned long) kmalloc(mem_needed, GFP_KERNEL);
	if (!mem) {
		printk(KERN_ERR "%s: zr36057_init: kmalloc (STAT_COM + ovl.mask) failed\n", zr->name);
		return -ENOMEM;
	}
	memset((void *) mem, 0, mem_needed);

	zr->stat_com = (u32 *) mem;
	for (j = 0; j < BUZ_NUM_STAT_COM; j++) {
		zr->stat_com[j] = 1;	/* mark as unavailable to zr36057 */
	}
	zr->overlay_mask = (u32 *) (mem + BUZ_NUM_STAT_COM * 4);

	/* Initialize zr->jpg_gbuf */

	for (j = 0; j < BUZ_MAX_FRAME; j++) {
		zr->jpg_gbuf[j].frag_tab = 0;
		zr->jpg_gbuf[j].frag_tab_bus = 0;
		zr->jpg_gbuf[j].state = BUZ_STATE_USER;
		zr->jpg_gbuf[j].bs.frame = j;
	}

	/*
	 *   Now add the template and register the device unit.
	 */
	memcpy(&zr->video_dev, &zoran_template, sizeof(zoran_template));
	strcpy(zr->video_dev.name, zr->name);
	if (video_register_device(&zr->video_dev, VFL_TYPE_GRABBER, video_nr) < 0) {
		i2c_unregister_bus(&zr->i2c);
		kfree((void *) zr->stat_com);
		return -1;
	}

	/* Enable bus-mastering */
	pci_set_master(zr->pci_dev);

	if (zr->card == BUZ)
		j = zr->params.input == 0 ? 3 : 7;
	else
		j = zr->params.input == 0 ? 0 : 7;
	set_videobus_enable(zr, 0);
	i2c_control_device(&zr->i2c, I2C_DRIVERID_VIDEODECODER,
			   DECODER_SET_INPUT, &j);
	i2c_control_device(&zr->i2c, I2C_DRIVERID_VIDEODECODER,
			   DECODER_SET_NORM, &zr->params.norm);
	i2c_control_device(&zr->i2c, I2C_DRIVERID_VIDEOENCODER,
			   ENCODER_SET_NORM, &zr->params.norm);
	set_videobus_enable(zr, 1);

	/* toggle JPEG codec sleep to sync PLL */
	zr36060_sleep(zr, 1);
	zr36060_sleep(zr, 0);

	/* set individual interrupt enables (without GIRQ1)
	   but don't global enable until zoran_open() */

	//btwrite(IRQ_MASK & ~ZR36057_ISR_GIRQ1, ZR36057_ICR);  // SW
	// It looks like using only JPEGRepIRQEn is not always reliable,
	// may be when JPEG codec crashes it won't generate IRQ? So,
	btwrite(IRQ_MASK, ZR36057_ICR);	// Enable Vsync interrupts too. SM

	zr36057_init_vfe(zr);

	zr->zoran_proc = NULL;
	zr->initialized = 1;

	zr36057_enable_jpg(zr, BUZ_MODE_IDLE);
#if (DEBUGLEVEL > 2)
	detect_guest_activity(zr);
#endif
	test_interrupts(zr);
	btwrite(IRQ_MASK, ZR36057_ICR);	// Enable Vsync interrupts too. SM
	if (!pass_through) {
		set_videobus_enable(zr, 0);
		i2c_control_device(&zr->i2c, I2C_DRIVERID_VIDEODECODER,
				   DECODER_ENABLE_OUTPUT, &zero);
		i2c_control_device(&zr->i2c, I2C_DRIVERID_VIDEOENCODER,
				   ENCODER_SET_INPUT, &two);
		set_videobus_enable(zr, 1);
	}
	return 0;
}

#include "zoran_procfs.c"

static void release_dc10(void)
{
	u8 command;
	int i;
	struct zoran *zr;

	for (i = 0; i < zoran_num; i++) {
		zr = &zoran[i];

		if (!zr->initialized)
			continue;

		/* unregister i2c_bus */
		i2c_unregister_bus((&zr->i2c));

		/* disable PCI bus-mastering */
		pci_read_config_byte(zr->pci_dev, PCI_COMMAND, &command);
		command &= ~PCI_COMMAND_MASTER;
		pci_write_config_byte(zr->pci_dev, PCI_COMMAND, command);

		/* put chip into reset */
		btwrite(0, ZR36057_SPGPPCR);

		free_irq(zr->pci_dev->irq, zr);

		/* unmap and free memory */

		kfree((void *) zr->stat_com);

		zoran_proc_cleanup(i);
		iounmap(zr->zr36057_mem);

		video_unregister_device(&zr->video_dev);
	}
}

/*
 *   Scan for a Buz card (actually for the PCI contoler ZR36057),
 *   request the irq and map the io memory
 */

static int find_zr36057(void)
{
	unsigned char latency, need_latency;
	struct zoran *zr;
	struct pci_dev *dev = NULL;
	int result;

	zoran_num = 0;

	while (zoran_num < BUZ_MAX
	       && (dev =
		   pci_find_device(PCI_VENDOR_ID_ZORAN,
				   PCI_DEVICE_ID_ZORAN_36057,
				   dev)) != NULL) {
		zr = &zoran[zoran_num];
		zr->pci_dev = dev;
		zr->zr36057_mem = NULL;
		zr->id = zoran_num;
		sprintf(zr->name, "MJPEG[%u]", zr->id);

		spin_lock_init(&zr->lock);
		init_MUTEX(&zr->sem);

		if (pci_enable_device(dev))
			continue;

		zr->zr36057_adr = pci_resource_start(zr->pci_dev, 0);
		pci_read_config_byte(zr->pci_dev, PCI_CLASS_REVISION,
				     &zr->revision);
		if (zr->revision < 2) {
			printk(KERN_INFO
			       "%s: Zoran ZR36057 (rev %d) irq: %d, memory: 0x%08x.\n",
			       zr->name, zr->revision, zr->pci_dev->irq,
			       zr->zr36057_adr);
		} else {
			unsigned short ss_vendor_id, ss_id;
			ss_vendor_id = zr->pci_dev->subsystem_vendor;
			ss_id = zr->pci_dev->subsystem_device;
			printk(KERN_INFO
			       "%s: Zoran ZR36067 (rev %d) irq: %d, memory: 0x%08x\n",
			       zr->name, zr->revision, zr->pci_dev->irq,
			       zr->zr36057_adr);
			printk(KERN_INFO
			       "%s: subsystem vendor=0x%04x id=0x%04x\n",
			       zr->name, ss_vendor_id, ss_id);
		}

		zr->zr36057_mem = ioremap_nocache(zr->zr36057_adr, 0x1000);
		if (!zr->zr36057_mem) {
			printk(KERN_ERR "%s: ioremap failed\n", zr->name);
			/* XXX handle error */
		}

		result = request_irq(zr->pci_dev->irq, zoran_irq, SA_SHIRQ | SA_INTERRUPT, zr->name, (void *) zr);
		if (result < 0) {
			if (result == -EINVAL) {
				printk(KERN_ERR
				       "%s: Bad irq number or handler\n",
				       zr->name);
			} else if (result == -EBUSY) {
				printk(KERN_ERR
				       "%s: IRQ %d busy, change your PnP config in BIOS\n",
				       zr->name, zr->pci_dev->irq);
			} else {
				printk(KERN_ERR
				       "%s: Can't assign irq, error code %d\n",
				       zr->name, result);
			}
			iounmap(zr->zr36057_mem);
			continue;
		}

		/* set PCI latency timer */
		pci_read_config_byte(zr->pci_dev, PCI_LATENCY_TIMER, &latency);
		need_latency = zr->revision > 1 ? 32 : 48;
		if (latency != need_latency) {
			printk(KERN_INFO "%s: Changing PCI latency from %d to %d.\n",  zr->name, latency, need_latency);
			pci_write_config_byte(zr->pci_dev, PCI_LATENCY_TIMER,  need_latency);
		}

		btwrite(0, ZR36057_SPGPPCR);
		mdelay(1);
		btwrite(ZR36057_SPGPPCR_SoftReset, ZR36057_SPGPPCR);
		mdelay(1);

		/* assert P_Reset */
		btwrite(0, ZR36057_JPC);

		/* set up GPIO direction - all output */
		btwrite(ZR36057_SPGPPCR_SoftReset | 0, ZR36057_SPGPPCR);
		btwrite((0x81 << 24) | 0x8888, ZR36057_GPPGCR1);

		/* i2c */
		memcpy(&zr->i2c, &zoran_i2c_bus_template,
		       sizeof(struct i2c_bus));
		strcpy(zr->i2c.name, zr->name);
		zr->i2c.data = zr;
		printk(KERN_INFO "%s: Initializing i2c bus...\n", zr->name);
		if (i2c_register_bus(&zr->i2c) < 0) {
			/* put chip into reset */
			btwrite(0, ZR36057_SPGPPCR);
			free_irq(zr->pci_dev->irq, zr);
			iounmap(zr->zr36057_mem);
			printk(KERN_ERR "%s: Can't initialize i2c bus\n", zr->name);
			continue;
		}

		if (zr->card != DC10 && zr->card != DC10plus
		    && zr->card != LML33 && zr->card != BUZ) {
			/* unregister i2c_bus */
			i2c_unregister_bus((&zr->i2c));
			/* put chip into reset */
			btwrite(0, ZR36057_SPGPPCR);
			free_irq(zr->pci_dev->irq, zr);
			iounmap(zr->zr36057_mem);
			printk(KERN_ERR "%s: Card not supported\n",
			       zr->name);
			continue;
		}
		printk(KERN_INFO "%s card detected\n", zr->name);
		if (zr->card == LML33) {
			GPIO(zr, 2, 1);	// Set Composite input/output
		}

		/* reset JPEG codec */
		zr36060_sleep(zr, 1);
		zr36060_reset(zr);

		/* video bus enabled */

		/* display codec revision */
		if (zr36060_read_8(zr, 0x022) == 0x33) {
			printk(KERN_INFO "%s: Zoran ZR36060 (rev %d)\n",
			       zr->name, zr36060_read_8(zr, 0x023));
		} else {
			/* unregister i2c_bus */
			i2c_unregister_bus((&zr->i2c));
			/* put chip into reset */
			btwrite(0, ZR36057_SPGPPCR);
			free_irq(zr->pci_dev->irq, zr);
			iounmap(zr->zr36057_mem);
			printk(KERN_ERR "%s: Zoran ZR36060 not found\n",
			       zr->name);
			continue;
		}

		zoran_num++;
	}
	if (zoran_num == 0) {
		printk(KERN_INFO "No known MJPEG cards found.\n");
	}
	return zoran_num;
}

static void handle_chipset(void)
{
	if(pci_pci_problems & PCIPCI_FAIL)
		printk(KERN_WARNING "Chipset may not support reliable PCI-PCI DMA.\n");

	if(pci_pci_problems & PCIPCI_TRITON)
	{
		printk(KERN_WARNING "Enabling Triton support.\n");
		triton = 1;
	}
	
	if(pci_pci_problems & PCIPCI_NATOMA)
	{
		printk(KERN_WARNING "Enabling Natoma workaround.\n");
		natoma = 1;
	}
}

static int init_dc10_cards(void)
{
	int i;

	memset(zoran, 0, sizeof(zoran));
	printk(KERN_INFO
	       "Zoran ZR36060 + ZR36057/67 MJPEG board driver version %d.%d\n",
	       MAJOR_VERSION, MINOR_VERSION);

	/* Look for cards */

	if (find_zr36057() < 0) {
		return -EIO;
	}
	if (zoran_num == 0)
		return 0;	//-ENXIO;

	printk(KERN_INFO "MJPEG: %d card(s) found\n", zoran_num);

	/* check the parameters we have been given, adjust if necessary */

	if (v4l_nbufs < 0)
		v4l_nbufs = 0;
	if (v4l_nbufs > VIDEO_MAX_FRAME)
		v4l_nbufs = VIDEO_MAX_FRAME;
	/* The user specfies the in KB, we want them in byte (and page aligned) */
	v4l_bufsize = PAGE_ALIGN(v4l_bufsize * 1024);
	if (v4l_bufsize < 32768)
		v4l_bufsize = 32768;
	/* 2 MB is arbitrary but sufficient for the maximum possible images */
	if (v4l_bufsize > 2048 * 1024)
		v4l_bufsize = 2048 * 1024;

	printk(KERN_INFO "MJPEG: using %d V4L buffers of size %d KB\n",
	       v4l_nbufs, v4l_bufsize >> 10);

	/* Use parameter for vidmem or try to find a video card */

	if (vidmem) {
		printk(KERN_INFO
		       "MJPEG: Using supplied video memory base address @ 0x%lx\n",
		       vidmem);
	}
	/* check if we have a Triton or Natome chipset */

	handle_chipset();

	/* take care of Natoma chipset and a revision 1 zr36057 */

	for (i = 0; i < zoran_num; i++) {
		if (natoma && zoran[i].revision <= 1) {
			zoran[i].need_contiguous = 1;
			printk(KERN_INFO
			       "%s: ZR36057/Natoma bug, max. buffer size is 128K\n",
			       zoran[i].name);
		} else {
			zoran[i].need_contiguous = 0;
		}
	}

	/* initialize the Buzs */

	/* We have to know which ones must be released if an error occurs */
	for (i = 0; i < zoran_num; i++)
		zoran[i].initialized = 0;

	for (i = 0; i < zoran_num; i++) {
		if (zr36057_init(i) < 0) {
			release_dc10();
			return -EIO;
		}
		zoran_proc_init(i);
	}

	return 0;
}

static void unload_dc10_cards(void)
{
	release_dc10();
}


module_init(init_dc10_cards);
module_exit(unload_dc10_cards);
MODULE_LICENSE("GPL");
