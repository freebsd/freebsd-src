/* 
    planb - v4l-compatible frame grabber driver for the PlanB hardware

    PlanB is used in the 7x00/8x00 series of PowerMacintosh
    Computers as video input DMA controller.

    Copyright (C) 1998 - 2002  Michel Lanners <mailto:mlan@cpu.lu>

    Based largely on the old bttv driver by Ralph Metzler

    Additional debugging and coding by Takashi Oe <mailto:toe@unlserve.unl.edu>

    For more information, see <http://www.cpu.lu/~mlan/planb.html>


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

/* $Id: planb.c,v 2.11 2002/04/03 15:57:57 mlan Exp mlan $ */

#include <linux/version.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/wrapper.h>
#include <linux/tqueue.h>
#include <linux/videodev.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/dbdma.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/irq.h>
#include <asm/semaphore.h>

/* Define these to get general / interrupt debugging */
#undef DEBUG
#undef IDEBUG

//#define DEBUG

#ifdef DEBUG
#define DBG(x...) printk(KERN_DEBUG ## x)
#else
#define DBG(x...)
#endif
#ifdef IDEBUG
#define IDBG(x...) printk(KERN_DEBUG ## x)
#else
#define IDBG(x...)
#endif

#include "planb.h"
#include "saa7196.h"

static struct planb planbs;
static volatile struct planb_registers *planb_regs;

static int def_norm = PLANB_DEF_NORM;	/* default norm */
static int video_nr = -1;
static int vbi_nr = -1;

MODULE_PARM(def_norm, "i");
MODULE_PARM_DESC(def_norm, "Default startup norm (0=PAL, 1=NTSC, 2=SECAM)");
MODULE_PARM(video_nr,"i");
MODULE_PARM(vbi_nr,"i");

MODULE_DESCRIPTION("planb - v4l driver module for Apple PlanB video in");
MODULE_AUTHOR("Michel Lanners & Takashi Oe - see: http://www.cpu.lu/planb.html");
MODULE_LICENSE("GPL");

/* ------------------ PlanB Exported Functions ------------------ */
static long planb_write(struct video_device *, const char *, unsigned long, int);
static long planb_read(struct video_device *, char *, unsigned long, int);
static int planb_open(struct video_device *, int);
static void planb_close(struct video_device *);
static int planb_ioctl(struct video_device *, unsigned int, void *);
static int planb_mmap(struct video_device *, const char *, unsigned long);
static void planb_irq(int, void *, struct pt_regs *);
static int planb_vbi_open(struct video_device *, int);
static void planb_vbi_close(struct video_device *);
static long planb_vbi_read(struct video_device *, char *, unsigned long, int);
static unsigned int planb_vbi_poll(struct video_device *, struct file *,
	poll_table *);
static int planb_vbi_ioctl(struct video_device *, unsigned int, void *);
static void release_planb(void);
static int __init init_planbs(void);
static void __exit exit_planbs(void);

/* ------------------ PlanB Internal Functions ------------------ */
static int planb_prepare_open(struct planb *);
static int planb_prepare_vbi(struct planb *);
static int planb_prepare_video(struct planb *);
static void planb_prepare_close(struct planb *);
static void planb_close_vbi(struct planb *);
static void planb_close_video(struct planb *);
static void saa_write_reg(unsigned char, unsigned char);
static unsigned char saa_status(int, struct planb *);
static void saa_set(unsigned char, unsigned char, struct planb *);
static void saa_init_regs(struct planb *);
static int grabbuf_alloc(struct planb *);
static int vgrab(struct planb *, struct video_mmap *);
static void add_clip(struct planb *, struct video_clip *);
static void fill_cmd_buff(struct planb *);
static void cmd_buff(struct planb *);
static dbdma_cmd_ptr setup_grab_cmd(int, struct planb *);
static void overlay_start(struct planb *);
static void overlay_stop(struct planb *);
static inline void tab_cmd_dbdma(dbdma_cmd_ptr, unsigned short, unsigned int);
static inline void tab_cmd_store(dbdma_cmd_ptr, unsigned int, unsigned int);
static inline void tab_cmd_gen(dbdma_cmd_ptr, unsigned short, unsigned short,
	unsigned int, unsigned int);
static int init_planb(struct planb *);
static int find_planb(void);
static void planb_pre_capture(int, struct planb *);
static dbdma_cmd_ptr cmd_geo_setup(dbdma_cmd_ptr, int, int, int, int, int,
	struct planb *);
static inline void planb_dbdma_stop(dbdma_regs_ptr);
static inline void planb_dbdma_restart(dbdma_regs_ptr);
static void saa_geo_setup(int, int, int, int, struct planb *);
static inline int overlay_is_active(struct planb *);

/*******************************/
/* Memory management functions */
/*******************************/

/* I know this is not the right way to allocate memory. Whoever knows
 * the right way to allocate a huge buffer for DMA that can be mapped
 * to user space, please tell me... or better, fix the code and send
 * patches.
 *
 *					Michel Lanners (mlan@cpu.lu)
 */
/* FIXME: As subsequent calls to __get_free_pages don't necessarily return
 * contiguous pages, we need to do horrible things later on when setting
 * up DMA, to make sure a single DMA transfer doesn't cross a page boundary.
 * At least, I hope it's done right later on ;-) ......
 * Anyway, there should be a way to get hold of a large buffer of contiguous
 * pages for DMA....
 */
static int grabbuf_alloc(struct planb *pb)
{
	int i, npage;

	npage = MAX_GBUFFERS * ((PLANB_MAX_FBUF / PAGE_SIZE + 1)
#ifndef PLANB_GSCANLINE
		+ MAX_LNUM
#endif /* PLANB_GSCANLINE */
		);
	if ((pb->rawbuf = (unsigned char**) kmalloc (npage
				* sizeof(unsigned long), GFP_KERNEL)) == 0)
		return -ENOMEM;
	for (i = 0; i < npage; i++) {
		pb->rawbuf[i] = (unsigned char *)__get_free_pages(GFP_KERNEL |
								GFP_DMA, 0);
		if (!pb->rawbuf[i])
			break;
		mem_map_reserve(virt_to_page(pb->rawbuf[i]));
	}
	if (i-- < npage) {
		DBG("PlanB: init_grab: grab buffer not allocated\n");
		for (; i > 0; i--) {
			mem_map_unreserve(virt_to_page(pb->rawbuf[i]));
			free_pages((unsigned long)pb->rawbuf[i], 0);
		}
		kfree(pb->rawbuf);
		return -ENOBUFS;
	}
	pb->rawbuf_nchunks = npage;
	return 0;
}

/*****************************/
/* Hardware access functions */
/*****************************/

static void saa_write_reg(unsigned char addr, unsigned char val)
{
	planb_regs->saa_addr = addr; eieio();
	planb_regs->saa_regval = val; eieio();
	return;
}

/* return  status byte 0 or 1: */
static unsigned char saa_status(int byte, struct planb *pb)
{
	saa_regs[pb->win.norm][SAA7196_STDC] =
		(saa_regs[pb->win.norm][SAA7196_STDC] & ~2) | ((byte & 1) << 1);
	saa_write_reg (SAA7196_STDC, saa_regs[pb->win.norm][SAA7196_STDC]);

	/* Let's wait 30msec for this one */
	current->state = TASK_INTERRUPTIBLE;
	schedule_timeout(30 * HZ / 1000);

	return (unsigned char)in_8 (&planb_regs->saa_status);
}

static void saa_set(unsigned char addr, unsigned char val, struct planb *pb)
{
	if(saa_regs[pb->win.norm][addr] != val) {
		saa_regs[pb->win.norm][addr] = val;
		saa_write_reg (addr, val);
	}
	return;
}

static void saa_init_regs(struct planb *pb)
{
	int i;

	for (i = 0; i < SAA7196_NUMREGS; i++)
		saa_write_reg (i, saa_regs[pb->win.norm][i]);
}

static void saa_geo_setup(int width, int height, int interlace,
	int fmt, struct planb *pb)
{
	int ht, norm = pb->win.norm;

	/* bits FS0, FS1 according to format spec */
	saa_regs[norm][SAA7196_FMTS] &= ~0x3;
	saa_regs[norm][SAA7196_FMTS] |= (palette2fmt[fmt].saa_fmt & 0x3);

	ht = (interlace ? height / 2 : height);
	saa_regs[norm][SAA7196_OUTPIX] = (unsigned char) (width & 0x00ff);
	saa_regs[norm][SAA7196_HFILT] = (saa_regs[norm][SAA7196_HFILT] & ~0x3)
						| (width >> 8 & 0x3);
	saa_regs[norm][SAA7196_OUTLINE] = (unsigned char) (ht & 0xff);
	saa_regs[norm][SAA7196_VYP] = (saa_regs[norm][SAA7196_VYP] & ~0x3)
						| (ht >> 8 & 0x3);
	/* feed both fields if interlaced, or else feed only even fields */
	saa_regs[norm][SAA7196_FMTS] = (interlace) ?
					(saa_regs[norm][SAA7196_FMTS] & ~0x60)
					: (saa_regs[norm][SAA7196_FMTS] | 0x60);
	/* transparent mode; extended format enabled */
	saa_regs[norm][SAA7196_DPATH] |= 0x3;
	/* bits LLV, MCT according to format spec */
	saa_regs[norm][SAA7196_DPATH] &= ~0x30;
	saa_regs[norm][SAA7196_DPATH] |= (palette2fmt[fmt].saa_fmt & 0x30);
}

/***************************/
/* DBDMA support functions */
/***************************/

static inline void planb_dbdma_restart(dbdma_regs_ptr ch)
{
	writel(PLANB_CLR(RUN), &ch->control);
	writel(PLANB_SET(RUN|WAKE) | PLANB_CLR(PAUSE), &ch->control);
}

static inline void planb_dbdma_stop(dbdma_regs_ptr ch)
{
	int i = 0;

	writel(PLANB_CLR(RUN) | PLANB_SET(FLUSH), &ch->control);
	while((readl(&ch->status) == (ACTIVE | FLUSH)) && (i < 999)) {
		IDBG("PlanB: waiting for DMA to stop\n");
		i++;
	}
}

static inline void tab_cmd_dbdma(dbdma_cmd_ptr ch, unsigned short command,
	unsigned int cmd_dep)
{
	st_le16(&ch->command, command);
	st_le16(&ch->req_count, 0);
	st_le32(&ch->phy_addr, 0);
	st_le32(&ch->cmd_dep, cmd_dep);
	/* really clears res_count & xfer_status */
	st_le32((unsigned int *)&ch->res_count, 0);
}

static inline void tab_cmd_store(dbdma_cmd_ptr ch, unsigned int phy_addr,
	unsigned int cmd_dep)
{
	st_le16(&ch->command, STORE_WORD | KEY_SYSTEM);
	st_le16(&ch->req_count, 4);
	st_le32(&ch->phy_addr, phy_addr);
	st_le32(&ch->cmd_dep, cmd_dep);
	st_le32((unsigned int *)&ch->res_count, 0);
}

static inline void tab_cmd_gen(dbdma_cmd_ptr ch, unsigned short command,
	unsigned short req_count, unsigned int phy_addr, unsigned int cmd_dep)
{
	st_le16(&ch->command, command);
	st_le16(&ch->req_count, req_count);
	st_le32(&ch->phy_addr, phy_addr);
	st_le32(&ch->cmd_dep, cmd_dep);
	st_le32((unsigned int *)&ch->res_count, 0);
}

static dbdma_cmd_ptr cmd_geo_setup(dbdma_cmd_ptr c1, int width, int height,
	int interlace, int fmt, int clip, struct planb *pb)
{
	int norm = pb->win.norm;

	saa_geo_setup(width, height, interlace, fmt, pb);
	/* if the number of DBDMA commands here (14) changes, lots of
	 * things need to be corrected accordingly... */
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->saa_addr),
								SAA7196_FMTS);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->saa_regval),
						saa_regs[norm][SAA7196_FMTS]);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->saa_addr),
								SAA7196_DPATH);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->saa_regval),
						saa_regs[norm][SAA7196_DPATH]);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->even),
			palette2fmt[fmt].pb_fmt | ((clip)? PLANB_CLIPMASK: 0));
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->odd),
			palette2fmt[fmt].pb_fmt | ((clip)? PLANB_CLIPMASK: 0));
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->saa_addr),
								SAA7196_OUTPIX);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->saa_regval),
						saa_regs[norm][SAA7196_OUTPIX]);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->saa_addr),
								SAA7196_HFILT);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->saa_regval),
						saa_regs[norm][SAA7196_HFILT]);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->saa_addr),
							SAA7196_OUTLINE);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->saa_regval),
					saa_regs[norm][SAA7196_OUTLINE]);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->saa_addr),
								SAA7196_VYP);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->saa_regval),
						saa_regs[norm][SAA7196_VYP]);
	return c1;
}

/******************************/
/* misc. supporting functions */
/******************************/

static inline void planb_lock(struct planb *pb)
{
	DBG("PlanB: planb_lock\n");
	down(&pb->lock);
}

static inline void planb_unlock(struct planb *pb)
{
	DBG("PlanB: planb_unlock\n");
	up(&pb->lock);
}

/***************/
/* Driver Core */
/***************/

/* number of entries in the circular DBDMA command buffer
 * initial stop, odd/even vbi, odd/even video, branch back */
#define NUMJUMPS 6
static int planb_prepare_open(struct planb *pb)
{
	dbdma_cmd_ptr	c;
	int		size, i;

	size = (NUMJUMPS + 1) * sizeof(struct dbdma_cmd);

	if((pb->jump_raw = kmalloc (size, GFP_KERNEL|GFP_DMA)) == 0)
		return -ENOMEM;
	memset(pb->jump_raw, 0, size);
	c = pb->jumpbuf = (dbdma_cmd_ptr) DBDMA_ALIGN (pb->jump_raw);

	/* circular DBDMA command buffer, to hold jumps to transfer commands */
	tab_cmd_dbdma(c++, DBDMA_STOP, 0);
	for (i=1; i<NUMJUMPS-1; i++)
		tab_cmd_dbdma(c++, DBDMA_NOP, 0);
	tab_cmd_dbdma(c, DBDMA_NOP|BR_ALWAYS,
		(unsigned int)pb->jumpbuf);

	DBG("PlanB: planb_prepare_open, jumpbuffer at 0x%08x, length %d.\n",
		(unsigned int)pb->jumpbuf, size); 
	return 0;
}

#define VBIDUMMY 40	/* must be even !! */
static int planb_prepare_vbi(struct planb *pb)
{
	int	size;

	/* allocate VBI comand buffer memory
	   (2 fields * VBI_MAXLINES + 40 handling + alignment) */
	size = (2*VBI_MAXLINES + VBIDUMMY + 1) * sizeof(struct dbdma_cmd);

	if ((pb->vbi_raw = kmalloc (size, GFP_KERNEL|GFP_DMA)) == 0)
		return -ENOMEM;
	memset (pb->vbi_raw, 0, size);
	size = (VBI_MAXLINES + VBIDUMMY/2) * sizeof(struct dbdma_cmd);
	pb->vbi_cbo.start = (dbdma_cmd_ptr) DBDMA_ALIGN (pb->vbi_raw);
	pb->vbi_cbo.size = pb->vbi_cbe.size = size;
	pb->vbi_cbe.start = pb->vbi_cbo.start + pb->vbi_cbo.size;
	pb->vbi_cbo.jumpaddr = pb->jumpbuf + 1;
	pb->vbi_cbe.jumpaddr = pb->jumpbuf + 3;

	DBG("PlanB: planb_prepare_vbi, dbdma cmd_buf at 0x%08x, length %d.\n",
		(unsigned int)pb->vbi_cbo.start, 2*size); 
	return 0;
}

static int planb_prepare_video(struct planb *pb)
{
	int	i, size;

	/* FIXME: This is stressing kmalloc to its limits...
		  We really should allocate smaller chunks. */

	/* allocate memory for two plus alpha command buffers (size: max lines,
	   plus 40 commands handling, plus 1 alignment), plus dummy command buf,
	   plus clipmask buffer, plus frame grabbing status */
	size = (pb->tab_size * (2 + MAX_GBUFFERS * TAB_FACTOR)
		    + MAX_GBUFFERS * PLANB_DUMMY + 1) * sizeof(struct dbdma_cmd)
		+ (PLANB_MAXLINES * ((PLANB_MAXPIXELS + 7) & ~7)) / 8
		+ MAX_GBUFFERS * sizeof(unsigned int);
	if ((pb->vid_raw = kmalloc (size, GFP_KERNEL|GFP_DMA)) == 0)
		return -ENOMEM;
	memset (pb->vid_raw, 0, size);
	pb->vid_cbo.start = (dbdma_cmd_ptr) DBDMA_ALIGN (pb->vid_raw);
	pb->vid_cbo.size = pb->vid_cbe.size = pb->tab_size/2;
	pb->vid_cbe.start = pb->vid_cbo.start + pb->vid_cbo.size;
	pb->vid_cbo.jumpaddr = pb->jumpbuf + 2;
	pb->vid_cbe.jumpaddr = pb->jumpbuf + 4;
	pb->overlay_last1 = pb->vid_cbo.start;
	pb->vid_cbo.bus = virt_to_bus(pb->vid_cbo.start);
	pb->vid_cbe.bus = virt_to_bus(pb->vid_cbe.start);
	pb->clip_cbo.start = pb->vid_cbe.start + pb->vid_cbe.size;
	pb->clip_cbo.size = pb->clip_cbe.size = pb->tab_size/2;
	pb->clip_cbe.start = pb->clip_cbo.start + pb->clip_cbo.size;
	pb->overlay_last2 = pb->clip_cbo.start;
	pb->clip_cbo.bus = virt_to_bus(pb->clip_cbo.start);
	pb->clip_cbe.bus = virt_to_bus(pb->clip_cbe.start);
	pb->gbuf[0].cap_cmd = pb->clip_cbe.start + pb->clip_cbe.size;
	pb->gbuf[0].pre_cmd = pb->gbuf[0].cap_cmd + pb->tab_size * TAB_FACTOR;
	for (i = 1; i < MAX_GBUFFERS; i++) {
		pb->gbuf[i].cap_cmd = pb->gbuf[i-1].pre_cmd + PLANB_DUMMY;
		pb->gbuf[i].pre_cmd = pb->gbuf[i].cap_cmd +
						pb->tab_size * TAB_FACTOR;
	}
	pb->gbuf[0].status = (volatile unsigned int *)
			(pb->gbuf[MAX_GBUFFERS-1].pre_cmd + PLANB_DUMMY);
	for (i = 1; i < MAX_GBUFFERS; i++)
		pb->gbuf[i].status = pb->gbuf[i-1].status;
	pb->mask = (unsigned char *)(pb->gbuf[MAX_GBUFFERS-1].status + 1);

	pb->rawbuf = NULL;
	pb->rawbuf_nchunks = 0;
	pb->grabbing = 0;
	for (i = 0; i < MAX_GBUFFERS; i++) {
		gbuf_ptr	gbuf = &pb->gbuf[i];

		*gbuf->status = GBUFFER_UNUSED;
		gbuf->width = 0;
		gbuf->height = 0;
		gbuf->fmt = 0;
		gbuf->norm_switch = 0;
#ifndef PLANB_GSCANLINE
		gbuf->lsize = 0;
		gbuf->lnum = 0;
#endif
	}
	pb->gcount = 0;
	pb->suspend = 0;
	pb->last_fr = -999;
	pb->prev_last_fr = -999;

	/* Reset DMA controllers */
	planb_dbdma_stop(&pb->planb_base->ch2);
	planb_dbdma_stop(&pb->planb_base->ch1);

	DBG("PlanB: planb_prepare_video, dbdma cmd_buf at 0x%08x, "
		"length %d.\n", (unsigned int)pb->vid_cbo.start, 2*size);
	return 0;
}

static void planb_prepare_close(struct planb *pb)
{
	/* make sure the dma's are idle */
	planb_dbdma_stop(&pb->planb_base->ch2);
	planb_dbdma_stop(&pb->planb_base->ch1);

	if(pb->jump_raw != 0) {
		kfree(pb->jump_raw);
		pb->jump_raw = 0;
	}
	return;
}

static void planb_close_vbi(struct planb *pb)
{
	/* FIXME: stop running DMA */

	/* Make sure the DMA controller doesn't jump here anymore */
	tab_cmd_dbdma(pb->vbi_cbo.jumpaddr, DBDMA_NOP, 0);
	tab_cmd_dbdma(pb->vbi_cbe.jumpaddr, DBDMA_NOP, 0);

	if(pb->vbi_raw != 0) {
		kfree (pb->vbi_raw);
		pb->vbi_raw = 0;
	}

	/* FIXME: deallocate VBI data buffer */

	/* FIXME: restart running DMA if app. */
	return;
}

static void planb_close_video(struct planb *pb)
{
	int i;

	/* FIXME: stop running DMA */

	/* Make sure the DMA controller doesn't jump here anymore */
	tab_cmd_dbdma(pb->vid_cbo.jumpaddr, DBDMA_NOP, 0);
	tab_cmd_dbdma(pb->vid_cbe.jumpaddr, DBDMA_NOP, 0);
/* No clipmask jumpbuffer yet  */
#if 0
	tab_cmd_dbdma(pb->clip_cbo.jumpaddr, DBDMA_NOP, 0);
	tab_cmd_dbdma(pb->clip_cbe.jumpaddr, DBDMA_NOP, 0);
#endif

	if(pb->vid_raw != 0) {
		kfree (pb->vid_raw);
		pb->vid_raw = 0;
		pb->cmd_buff_inited = 0;
	}
	if(pb->rawbuf) {
		for (i = 0; i < pb->rawbuf_nchunks; i++) {
			mem_map_unreserve(virt_to_page(pb->rawbuf[i]));
			free_pages((unsigned long)pb->rawbuf[i], 0);
		}
		kfree(pb->rawbuf);
	}
	pb->rawbuf = NULL;

	/* FIXME: restart running DMA if app. */
	return;
}

/*****************************/
/* overlay support functions */
/*****************************/

static void overlay_start(struct planb *pb)
{
	DBG("PlanB: overlay_start()\n");

	if(ACTIVE & readl(&pb->planb_base->ch1.status)) {

		DBG("PlanB: presumably, grabbing is in progress...\n");

		planb_dbdma_stop(&pb->planb_base->ch2);
		writel(pb->clip_cbo.bus, &pb->planb_base->ch2.cmdptr);
		planb_dbdma_restart(&pb->planb_base->ch2);
		st_le16 (&pb->vid_cbo.start->command, DBDMA_NOP);
		tab_cmd_dbdma(pb->gbuf[pb->last_fr].last_cmd,
			DBDMA_NOP | BR_ALWAYS, pb->vid_cbo.bus);
		eieio();
		pb->prev_last_fr = pb->last_fr;
		pb->last_fr = -2;
		if(!(ACTIVE & readl(&pb->planb_base->ch1.status))) {
			IDBG("PlanB: became inactive "
				"in the mean time... reactivating\n");
			planb_dbdma_stop(&pb->planb_base->ch1);
			writel(pb->vid_cbo.bus, &pb->planb_base->ch1.cmdptr);
			planb_dbdma_restart(&pb->planb_base->ch1);
		}
	} else {

		DBG("PlanB: currently idle, so can do whatever\n");

		planb_dbdma_stop(&pb->planb_base->ch2);
		planb_dbdma_stop(&pb->planb_base->ch1);
		st_le32(&pb->planb_base->ch2.cmdptr, pb->clip_cbo.bus);
		st_le32(&pb->planb_base->ch1.cmdptr, pb->vid_cbo.bus);
		writew(DBDMA_NOP, &pb->vid_cbo.start->command);
		planb_dbdma_restart(&pb->planb_base->ch2);
		planb_dbdma_restart(&pb->planb_base->ch1);
		pb->last_fr = -1;
	}
	return;
}

static void overlay_stop(struct planb *pb)
{
	DBG("PlanB: overlay_stop()\n");

	if(pb->last_fr == -1) {

		DBG("PlanB: no grabbing, it seems...\n");

		planb_dbdma_stop(&pb->planb_base->ch2);
		planb_dbdma_stop(&pb->planb_base->ch1);
		pb->last_fr = -999;
	} else if(pb->last_fr == -2) {
		unsigned int cmd_dep;
		tab_cmd_dbdma(pb->gbuf[pb->prev_last_fr].cap_cmd, DBDMA_STOP, 0);
		eieio();
		cmd_dep = (unsigned int)readl(&pb->overlay_last1->cmd_dep);
		if(overlay_is_active(pb)) {

			DBG("PlanB: overlay is currently active\n");

			planb_dbdma_stop(&pb->planb_base->ch2);
			planb_dbdma_stop(&pb->planb_base->ch1);
			if(cmd_dep != pb->vid_cbo.bus) {
				writel(virt_to_bus(pb->overlay_last1),
					&pb->planb_base->ch1.cmdptr);
				planb_dbdma_restart(&pb->planb_base->ch1);
			}
		}
		pb->last_fr = pb->prev_last_fr;
		pb->prev_last_fr = -999;
	}
	return;
}

static void suspend_overlay(struct planb *pb)
{
	int fr = -1;
	struct dbdma_cmd last;

	DBG("PlanB: suspend_overlay: %d\n", pb->suspend);

	if(pb->suspend++)
		return;
	if(ACTIVE & readl(&pb->planb_base->ch1.status)) {
		if(pb->last_fr == -2) {
			fr = pb->prev_last_fr;
			memcpy(&last, (void*)pb->gbuf[fr].last_cmd, sizeof(last));
			tab_cmd_dbdma(pb->gbuf[fr].last_cmd, DBDMA_STOP, 0);
		}
		if(overlay_is_active(pb)) {
			planb_dbdma_stop(&pb->planb_base->ch2);
			planb_dbdma_stop(&pb->planb_base->ch1);
			pb->suspended.overlay = 1;
			pb->suspended.frame = fr;
			memcpy(&pb->suspended.cmd, &last, sizeof(last));
			return;
		}
	}
	pb->suspended.overlay = 0;
	pb->suspended.frame = fr;
	memcpy(&pb->suspended.cmd, &last, sizeof(last));
	return;
}

static void resume_overlay(struct planb *pb)
{

	DBG("PlanB: resume_overlay: %d\n", pb->suspend);

	if(pb->suspend > 1)
		return;
	if(pb->suspended.frame != -1) {
		memcpy((void*)pb->gbuf[pb->suspended.frame].last_cmd,
			&pb->suspended.cmd, sizeof(pb->suspended.cmd));
	}
	if(ACTIVE & readl(&pb->planb_base->ch1.status)) {
		goto finish;
	}
	if(pb->suspended.overlay) {

		DBG("PlanB: overlay being resumed\n");

		st_le16 (&pb->vid_cbo.start->command, DBDMA_NOP);
		st_le16 (&pb->clip_cbo.start->command, DBDMA_NOP);
		/* Set command buffer addresses */
		writel(virt_to_bus(pb->overlay_last1),
			&pb->planb_base->ch1.cmdptr);
		writel(virt_to_bus(pb->overlay_last2),
			&pb->planb_base->ch2.cmdptr);
		/* Start the DMA controller */
		writel(PLANB_CLR(PAUSE) | PLANB_SET(RUN|WAKE),
			&pb->planb_base->ch2.control);
		writel(PLANB_CLR(PAUSE) | PLANB_SET(RUN|WAKE),
			&pb->planb_base->ch1.control);
	} else if(pb->suspended.frame != -1) {
		writel(virt_to_bus(pb->gbuf[pb->suspended.frame].last_cmd),
			&pb->planb_base->ch1.cmdptr);
		writel(PLANB_CLR(PAUSE) | PLANB_SET(RUN|WAKE),
			&pb->planb_base->ch1.control);
	}

finish:
	pb->suspend--;
	wake_up_interruptible(&pb->suspendq);
}

static void add_clip(struct planb *pb, struct video_clip *clip) 
{
	volatile unsigned char	*base;
	int	xc = clip->x, yc = clip->y;
	int	wc = clip->width, hc = clip->height;
	int	ww = pb->win.width, hw = pb->win.height;
	int	x, y, xtmp1, xtmp2;

	DBG("PlanB: clip %dx%d+%d+%d\n", wc, hc, xc, yc);

	if(xc < 0) {
		wc += xc;
		xc = 0;
	}
	if(yc < 0) {
		hc += yc;
		yc = 0;
	}
	if(xc + wc > ww)
		wc = ww - xc;
	if(wc <= 0) /* Nothing to do */
		return;
	if(yc + hc > hw)
		hc = hw - yc;

	for (y = yc; y < yc+hc; y++) {
		xtmp1=xc>>3;
		xtmp2=(xc+wc)>>3;
		base = pb->mask + y*96;
		if(xc != 0 || wc >= 8)
			*(base + xtmp1) &= (unsigned char)(0x00ff &
				(0xff00 >> (xc&7)));
		for (x = xtmp1 + 1; x < xtmp2; x++) {
			*(base + x) = 0;
		}
		if(xc < (ww & ~0x7))
			*(base + xtmp2) &= (unsigned char)(0x00ff >>
				((xc+wc) & 7));
	}

	return;
}

static void fill_cmd_buff(struct planb *pb)
{
	int		restore = 0;
	dbdma_cmd_t	last;

	DBG("PlanB: fill_cmd_buff()\n");

	if(pb->overlay_last1 != pb->vid_cbo.start) {
		restore = 1;
		last = *(pb->overlay_last1);
	}
	memset ((void *) pb->vid_cbo.start, 0, 2 * pb->tab_size
					* sizeof(struct dbdma_cmd));
	cmd_buff (pb);
	if(restore)
		*(pb->overlay_last1) = last;
	if(pb->suspended.overlay) {
		unsigned long jump_addr = readl(&pb->overlay_last1->cmd_dep);
		if(jump_addr != pb->vid_cbo.bus) {
			int i;

			DBG("PlanB: adjusting ch1's jump address\n");

			for(i = 0; i < MAX_GBUFFERS; i++) {
				if(pb->gbuf[i].need_pre_capture) {
				    if(jump_addr == virt_to_bus(pb->gbuf[i].pre_cmd))
					goto found;
				} else {
				    if(jump_addr ==
					    virt_to_bus(pb->gbuf[i].cap_cmd))
					goto found;
				}
			}

			DBG("       not found!\n");

			goto out;
found:
			if(pb->gbuf[i].need_pre_capture)
				writel(virt_to_bus(pb->overlay_last1),
					&pb->gbuf[i].pre_cmd->phy_addr);
			else
				writel(virt_to_bus(pb->overlay_last1),
					&pb->gbuf[i].cap_cmd->phy_addr);
		}
	}
out:
	pb->cmd_buff_inited = 1;

	return;
}

static void cmd_buff(struct planb *pb)
{
	int		i, bpp, count, nlines, stepsize, interlace;
	unsigned long	base, jump, addr_com, addr_dep;
	dbdma_cmd_ptr	c1 = pb->vid_cbo.start;
	dbdma_cmd_ptr	c2 = pb->clip_cbo.start;

	interlace = pb->win.interlace;
	bpp = pb->win.bpp;
	count = (bpp * ((pb->win.x + pb->win.width > pb->win.swidth) ?
		(pb->win.swidth - pb->win.x) : pb->win.width));
	nlines = ((pb->win.y + pb->win.height > pb->win.sheight) ?
		(pb->win.sheight - pb->win.y) : pb->win.height);

	/* Do video in: */

	/* Preamble commands: */
	addr_com = virt_to_bus(c1);
	addr_dep = virt_to_bus(&c1->cmd_dep);
	tab_cmd_dbdma(c1++, DBDMA_NOP, 0);
	jump = virt_to_bus(c1+16); /* 14 by cmd_geo_setup() and 2 for padding */
	c1 = cmd_geo_setup(c1, pb->win.width, pb->win.height, interlace,
						pb->win.color_fmt, 1, pb);
	tab_cmd_store(c1++, addr_com, (unsigned)(DBDMA_NOP | BR_ALWAYS) << 16);
	tab_cmd_store(c1++, addr_dep, jump);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->ch1.wait_sel),
							PLANB_SET(FIELD_SYNC));
		/* (1) wait for field sync to be set */
	tab_cmd_dbdma(c1++, DBDMA_NOP | WAIT_IFCLR, 0);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->ch1.br_sel),
							PLANB_SET(ODD_FIELD));
		/* wait for field sync to be cleared */
	tab_cmd_dbdma(c1++, DBDMA_NOP | WAIT_IFSET, 0);
		/* if not odd field, wait until field sync is set again */
	tab_cmd_dbdma(c1, DBDMA_NOP | BR_IFSET, virt_to_bus(c1-3)); c1++;
		/* assert ch_sync to ch2 */
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->ch2.control),
							PLANB_SET(CH_SYNC));
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->ch1.br_sel),
							PLANB_SET(DMA_ABORT));

	base = (pb->fb.phys + pb->fb.offset + pb->win.y * (pb->win.bpl +
					pb->win.pad) + pb->win.x * bpp);

	if (interlace) {
		stepsize = 2;
		jump = virt_to_bus(c1 + (nlines + 1) / 2);
	} else {
		stepsize = 1;
		jump = virt_to_bus(c1 + nlines);
	}

	/* even field data: */
	for (i=0; i < nlines; i += stepsize, c1++)
		tab_cmd_gen(c1, INPUT_MORE | KEY_STREAM0 | BR_IFSET,
			count, base + i * (pb->win.bpl + pb->win.pad), jump);

	/* For non-interlaced, we use even fields only */
	if (!interlace)
		goto cmd_tab_data_end;

	/* Resync to odd field */
		/* (2) wait for field sync to be set */
	tab_cmd_dbdma(c1++, DBDMA_NOP | WAIT_IFCLR, 0);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->ch1.br_sel),
							PLANB_SET(ODD_FIELD));
		/* wait for field sync to be cleared */
	tab_cmd_dbdma(c1++, DBDMA_NOP | WAIT_IFSET, 0);
		/* if not odd field, wait until field sync is set again */
	tab_cmd_dbdma(c1, DBDMA_NOP | BR_IFCLR, virt_to_bus(c1-3)); c1++;
		/* assert ch_sync to ch2 */
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->ch2.control),
							PLANB_SET(CH_SYNC));
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->ch1.br_sel),
							PLANB_SET(DMA_ABORT));
	
	/* odd field data: */
	jump = virt_to_bus(c1 + nlines / 2);
	for (i=1; i < nlines; i += stepsize, c1++)
		tab_cmd_gen(c1, INPUT_MORE | KEY_STREAM0 | BR_IFSET, count,
			base + i * (pb->win.bpl + pb->win.pad), jump);

	/* And jump back to the start */
cmd_tab_data_end:
	pb->overlay_last1 = c1;	/* keep a pointer to the last command */
	tab_cmd_dbdma(c1, DBDMA_NOP | BR_ALWAYS, pb->vid_cbo.bus);

	/* Clipmask command buffer */

	/* Preamble commands: */
	tab_cmd_dbdma(c2++, DBDMA_NOP, 0);
	tab_cmd_store(c2++, (unsigned)(&pb->planb_base_bus->ch2.wait_sel),
							PLANB_SET(CH_SYNC));
		/* wait until ch1 asserts ch_sync */
	tab_cmd_dbdma(c2++, DBDMA_NOP | WAIT_IFCLR, 0);
		/* clear ch_sync asserted by ch1 */
	tab_cmd_store(c2++, (unsigned)(&pb->planb_base_bus->ch2.control),
							PLANB_CLR(CH_SYNC));
	tab_cmd_store(c2++, (unsigned)(&pb->planb_base_bus->ch2.wait_sel),
							PLANB_SET(FIELD_SYNC));
	tab_cmd_store(c2++, (unsigned)(&pb->planb_base_bus->ch2.br_sel),
							PLANB_SET(ODD_FIELD));

	/* jump to end of even field if appropriate */
	/* this points to (interlace)? pos. C: pos. B */
	jump = (interlace) ? virt_to_bus(c2 + (nlines + 1) / 2 + 2):
						virt_to_bus(c2 + nlines + 2);
		/* if odd field, skip over to odd field clipmasking */
	tab_cmd_dbdma(c2++, DBDMA_NOP | BR_IFSET, jump);

	/* even field mask: */
	tab_cmd_store(c2++, (unsigned)(&pb->planb_base_bus->ch2.br_sel),
							PLANB_SET(DMA_ABORT));
	/* this points to pos. B */
	jump = (interlace) ? virt_to_bus(c2 + nlines + 1):
						virt_to_bus(c2 + nlines);
	base = virt_to_bus(pb->mask);
	for (i=0; i < nlines; i += stepsize, c2++)
		tab_cmd_gen(c2, OUTPUT_MORE | KEY_STREAM0 | BR_IFSET, 96,
			base + i * 96, jump);

	/* For non-interlaced, we use only even fields */
	if(!interlace)
		goto cmd_tab_mask_end;

	/* odd field mask: */
/* C */	tab_cmd_store(c2++, (unsigned)(&pb->planb_base_bus->ch2.br_sel),
							PLANB_SET(DMA_ABORT));
	/* this points to pos. B */
	jump = virt_to_bus(c2 + nlines / 2);
	base = virt_to_bus(pb->mask);
	for (i=1; i < nlines; i += 2, c2++)     /* abort if set */
		tab_cmd_gen(c2, OUTPUT_MORE | KEY_STREAM0 | BR_IFSET, 96,
			base + i * 96, jump);

	/* Inform channel 1 and jump back to start */
cmd_tab_mask_end:
	/* ok, I just realized this is kind of flawed. */
	/* this part is reached only after odd field clipmasking. */
	/* wanna clean up? */
		/* wait for field sync to be set */
		/* corresponds to fsync (1) of ch1 */
/* B */	tab_cmd_dbdma(c2++, DBDMA_NOP | WAIT_IFCLR, 0);
		/* restart ch1, meant to clear any dead bit or something */
	tab_cmd_store(c2++, (unsigned)(&pb->planb_base_bus->ch1.control),
							PLANB_CLR(RUN));
	tab_cmd_store(c2++, (unsigned)(&pb->planb_base_bus->ch1.control),
							PLANB_SET(RUN));
	pb->overlay_last2 = c2;	/* keep a pointer to the last command */
		/* start over even field clipmasking */
	tab_cmd_dbdma(c2, DBDMA_NOP | BR_ALWAYS, pb->clip_cbo.bus);

	eieio();
	return;
}

/*********************************/
/* grabdisplay support functions */
/*********************************/

static inline int overlay_is_active(struct planb *pb)
{
	unsigned int size = pb->tab_size * sizeof(struct dbdma_cmd);
	unsigned int caddr = (unsigned)readl(&pb->planb_base->ch1.cmdptr);

	return (readl(&pb->overlay_last1->cmd_dep) == pb->vid_cbo.bus)
			&& (caddr < (pb->vid_cbo.bus + size))
			&& (caddr >= (unsigned)pb->vid_cbo.bus);
}

static int vgrab(struct planb *pb, struct video_mmap *mp)
{
	unsigned int	fr = mp->frame;
	unsigned int	fmt = mp->format;
	unsigned int	bpp = palette2fmt[fmt].bpp;
	gbuf_ptr	gbuf = &pb->gbuf[fr];

	if(pb->rawbuf==NULL) {
		int err;
		if((err=grabbuf_alloc(pb)))
			return err;
	}

	DBG("PlanB: grab %d: %dx%d fmt %d (%u)\n", pb->grabbing, mp->width,
						mp->height, fmt, fr);

	if(pb->grabbing >= MAX_GBUFFERS) {
		DBG("       no buffer\n");
		return -ENOBUFS;
	}
	if(fr > (MAX_GBUFFERS - 1) || fr < 0) {
		DBG("       invalid buffer\n");
		return -EINVAL;
	}
	if(mp->height <= 0 || mp->width <= 0) {
		DBG("       negative height or width\n");
		return -EINVAL;
	}
	if(mp->format < 0 || mp->format >= PLANB_PALETTE_MAX) {
		DBG("       format out of range\n");
		return -EINVAL;
	}
	if(bpp == 0) {
		DBG("       unsupported format %d\n", mp->format);
		return -EINVAL;
	}
	if (mp->height * mp->width * bpp > PLANB_MAX_FBUF) {
		DBG("       grab bigger than buffer\n");
		return -EINVAL;
	}

	planb_lock(pb);
	if(mp->width != gbuf->width || mp->height != gbuf->height ||
			fmt != gbuf->fmt || (gbuf->norm_switch)) {
		int i;
#ifndef PLANB_GSCANLINE
		unsigned int osize = gbuf->width * gbuf->height *
					palette2fmt[gbuf->fmt].bpp;
		unsigned int nsize = mp->width * mp->height * bpp;
#endif

		DBG("PlanB: changed gwidth = %d, gheight = %d, format = %u, "
			"osize = %d, nsize = %d\n", mp->width, mp->height, fmt,
								osize, nsize);

/* Do we _really_ need to clear the grab buffers?? */
#if 0
#ifndef PLANB_GSCANLINE
		if(gbuf->norm_switch)
			nsize = 0;
		if (nsize < osize) {
			for(i = gbuf->idx; osize > 0; i++) {
				memset((void *)pb->rawbuf[i], 0, PAGE_SIZE);
				osize -= PAGE_SIZE;
			}
		}
		for(i = gbuf->l_fr_addr_idx; i <
				gbuf->l_fr_addr_idx + gbuf->lnum; i++)
			memset((void *)pb->rawbuf[i], 0, PAGE_SIZE);
#else
/* XXX TODO */
/*
		if(gbuf->norm_switch)
			memset((void *)pb->gbuffer[fr], 0,
					pb->gbytes_per_line * gbuf->height);
		else {
			if(mp->
			for(i = 0; i < gbuf->height; i++) {
				memset((void *)(pb->gbuffer[fr]
					+ pb->gbytes_per_line * i
			}
		}
*/
#endif
#endif /* if 0 */
		gbuf->width = mp->width;
		gbuf->height = mp->height;
		gbuf->fmt = fmt;
		gbuf->last_cmd = setup_grab_cmd(fr, pb);
		planb_pre_capture(fr, pb);
		gbuf->need_pre_capture = 1;
		gbuf->norm_switch = 0;
	} else
		gbuf->need_pre_capture = 0;

	*gbuf->status = GBUFFER_GRABBING;
	if(!(ACTIVE & readl(&pb->planb_base->ch1.status))) {

		IDBG("PlanB: ch1 inactive, initiating grabbing\n");

		planb_dbdma_stop(&pb->planb_base->ch1);
		if(gbuf->need_pre_capture) {

			DBG("PlanB: padding pre-capture sequence\n");

			writel(virt_to_bus(gbuf->pre_cmd),
				&pb->planb_base->ch1.cmdptr);
		} else {
			tab_cmd_dbdma(gbuf->last_cmd, DBDMA_STOP, 0);
			tab_cmd_dbdma(gbuf->cap_cmd, DBDMA_NOP, 0);
		/* let's be on the safe side. here is not timing critical. */
			tab_cmd_dbdma((gbuf->cap_cmd + 1), DBDMA_NOP, 0);
			writel(virt_to_bus(gbuf->cap_cmd),
				&pb->planb_base->ch1.cmdptr);
		}
		planb_dbdma_restart(&pb->planb_base->ch1);
		pb->last_fr = fr;
	} else {
		int i;

		DBG("PlanB: ch1 active, grabbing being queued\n");

		if((pb->last_fr == -1) || ((pb->last_fr == -2) &&
						overlay_is_active(pb))) {

			DBG("PlanB: overlay is active, grabbing defered\n");

			tab_cmd_dbdma(gbuf->last_cmd, DBDMA_NOP | BR_ALWAYS,
					pb->vid_cbo.bus);
			if(gbuf->need_pre_capture) {

				DBG("PlanB: padding pre-capture sequence\n");

				tab_cmd_store(gbuf->pre_cmd,
				    virt_to_bus(&pb->overlay_last1->cmd_dep),
				    pb->vid_cbo.bus);
				eieio();
				writel(virt_to_bus(gbuf->pre_cmd),
					&pb->overlay_last1->cmd_dep);
			} else {
				tab_cmd_store(gbuf->cap_cmd,
				    virt_to_bus(&pb->overlay_last1->cmd_dep),
				    pb->vid_cbo.bus);
				tab_cmd_dbdma((gbuf->cap_cmd + 1),
								DBDMA_NOP, 0);
				eieio();
				writel(virt_to_bus(gbuf->cap_cmd),
					&pb->overlay_last1->cmd_dep);
			}
			for(i = 0; overlay_is_active(pb) && i < 999; i++)
				DBG("PlanB: waiting for overlay done\n");
			tab_cmd_dbdma(pb->vid_cbo.start, DBDMA_NOP, 0);
			pb->prev_last_fr = fr;
			pb->last_fr = -2;
		} else if(pb->last_fr == -2) {

			DBG("PlanB: mixed mode detected, grabbing"
				" will be done before activating overlay\n");

			tab_cmd_dbdma(pb->vid_cbo.start, DBDMA_NOP, 0);
			if(gbuf->need_pre_capture) {

				DBG("PlanB: padding pre-capture sequence\n");

				tab_cmd_dbdma(pb->gbuf[pb->prev_last_fr].last_cmd,
						DBDMA_NOP | BR_ALWAYS,
						virt_to_bus(gbuf->pre_cmd));
				eieio();
			} else {
				tab_cmd_dbdma(gbuf->cap_cmd, DBDMA_NOP, 0);
				if(pb->gbuf[pb->prev_last_fr].width !=
								gbuf->width
					|| pb->gbuf[pb->prev_last_fr].height !=
								gbuf->height
					|| pb->gbuf[pb->prev_last_fr].fmt !=
								gbuf->fmt)
					tab_cmd_dbdma((gbuf->cap_cmd + 1),
								DBDMA_NOP, 0);
				else
					tab_cmd_dbdma((gbuf->cap_cmd + 1),
					    DBDMA_NOP | BR_ALWAYS,
					    virt_to_bus(gbuf->cap_cmd + 16));
				tab_cmd_dbdma(pb->gbuf[pb->prev_last_fr].last_cmd,
						DBDMA_NOP | BR_ALWAYS,
						virt_to_bus(gbuf->cap_cmd));
				eieio();
			}
			tab_cmd_dbdma(gbuf->last_cmd, DBDMA_NOP | BR_ALWAYS,
				pb->vid_cbo.bus);
			eieio();
			pb->prev_last_fr = fr;
			pb->last_fr = -2;
		} else {
			gbuf_ptr	lastgbuf = &pb->gbuf[pb->last_fr];

			DBG("PlanB: active grabbing session detected\n");

			if(gbuf->need_pre_capture) {

				DBG("PlanB: padding pre-capture sequence\n");

				tab_cmd_dbdma(lastgbuf->last_cmd,
						DBDMA_NOP | BR_ALWAYS,
						virt_to_bus(gbuf->pre_cmd));
				eieio();
			} else {
				tab_cmd_dbdma(gbuf->last_cmd, DBDMA_STOP, 0);
				tab_cmd_dbdma(gbuf->cap_cmd, DBDMA_NOP, 0);
				if(lastgbuf->width != gbuf->width
				    || lastgbuf->height != gbuf->height
				    || lastgbuf->fmt != gbuf->fmt)
					tab_cmd_dbdma((gbuf->cap_cmd + 1),
								DBDMA_NOP, 0);
				else
					tab_cmd_dbdma((gbuf->cap_cmd + 1),
					    DBDMA_NOP | BR_ALWAYS,
					    virt_to_bus(gbuf->cap_cmd + 16));
				tab_cmd_dbdma(lastgbuf->last_cmd,
						DBDMA_NOP | BR_ALWAYS,
						virt_to_bus(gbuf->cap_cmd));
				eieio();
			}
			pb->last_fr = fr;
		}
		if(!(ACTIVE & readl(&pb->planb_base->ch1.status))) {

			DBG("PlanB: became inactive in the mean time... "
				"reactivating\n");

			planb_dbdma_stop(&pb->planb_base->ch1);
			writel(virt_to_bus(gbuf->cap_cmd),
				&pb->planb_base->ch1.cmdptr);
			planb_dbdma_restart(&pb->planb_base->ch1);
		}
	}
	pb->grabbing++;
	planb_unlock(pb);

	return 0;
}

static void planb_pre_capture(int fr, struct planb *pb)
{
	gbuf_ptr	gbuf = &pb->gbuf[fr];
	dbdma_cmd_ptr	c1 = gbuf->pre_cmd;
	int		height = gbuf->height;
	int		interlace = (height > pb->maxlines/2)? 1: 0;

	tab_cmd_dbdma(c1++, DBDMA_NOP, 0);
	c1 = cmd_geo_setup(c1, gbuf->width, height, interlace, gbuf->fmt,
									0, pb);
	/* Sync to even field */
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->ch1.wait_sel),
							PLANB_SET(FIELD_SYNC));
	tab_cmd_dbdma(c1++, DBDMA_NOP | WAIT_IFCLR, 0);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->ch1.br_sel),
							PLANB_SET(ODD_FIELD));
	tab_cmd_dbdma(c1++, DBDMA_NOP | WAIT_IFSET, 0);
	tab_cmd_dbdma(c1, DBDMA_NOP | BR_IFSET, virt_to_bus(c1-3)); c1++;
	tab_cmd_dbdma(c1++, DBDMA_NOP | INTR_ALWAYS, 0);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->ch1.br_sel),
							PLANB_SET(DMA_ABORT));
	/* For non-interlaced, we use even fields only */
	if (interlace == 0)
		goto cmd_tab_data_end;
	/* Sync to odd field */
	tab_cmd_dbdma(c1++, DBDMA_NOP | WAIT_IFCLR, 0);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->ch1.br_sel),
							PLANB_SET(ODD_FIELD));
	tab_cmd_dbdma(c1++, DBDMA_NOP | WAIT_IFSET, 0);
	tab_cmd_dbdma(c1, DBDMA_NOP | BR_IFCLR, virt_to_bus(c1-3)); c1++;
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->ch1.br_sel),
							PLANB_SET(DMA_ABORT));
cmd_tab_data_end:
	tab_cmd_dbdma(c1, DBDMA_NOP | BR_ALWAYS, virt_to_bus(gbuf->cap_cmd));

	eieio();
}

/* This needs some explanation.
 * What we do here is write the DBDMA commands to fill the grab buffer.
 * Since the grab buffer is made up of physically non-contiguous chunks,
 * we need to make sure to not make the DMA engine write across a chunk
 * boundary: the DMA engine needs a physically contiguous memory chunk for
 * a single scan line.
 * So all those scan lines that cross a chunk boundary are written do spare
 * scratch buffers, and we keep track of this fact.
 * Later, in the interrupt routine, we copy those scan lines (in two pieces)
 * back to where they belong in the right sequence in the grab buffer.
 */
static dbdma_cmd_ptr setup_grab_cmd(int fr, struct planb *pb)
{
	int		i, count, nlines, stepsize, interlace;
#ifdef PLANB_GSCANLINE
	int		scanline;
#else
	int		nlpp, leftover1;
	unsigned long	base;
#endif
	unsigned long	jump;
	int		pagei;
	dbdma_cmd_ptr	c1;
	dbdma_cmd_ptr	jump_addr;
	gbuf_ptr	gbuf = &pb->gbuf[fr];
	int		fmt = gbuf->fmt;

	c1 = gbuf->cap_cmd;
	nlines = gbuf->height;
	interlace = (nlines > pb->maxlines/2) ? 1 : 0;
	count = palette2fmt[fmt].bpp * gbuf->width;
#ifdef PLANB_GSCANLINE
	scanline = pb->gbytes_per_line;
#else
	gbuf->lsize = count;
	gbuf->lnum = 0;
#endif

	/* Do video in: */

	/* Preamble commands: */
	tab_cmd_dbdma(c1++, DBDMA_NOP, 0);
	tab_cmd_dbdma(c1, DBDMA_NOP | BR_ALWAYS, virt_to_bus(c1 + 16)); c1++;
	c1 = cmd_geo_setup(c1, gbuf->width, nlines, interlace, fmt, 0, pb);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->ch1.wait_sel),
							PLANB_SET(FIELD_SYNC));
	tab_cmd_dbdma(c1++, DBDMA_NOP | WAIT_IFCLR, 0);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->ch1.br_sel),
							PLANB_SET(ODD_FIELD));
	tab_cmd_dbdma(c1++, DBDMA_NOP | WAIT_IFSET, 0);
	tab_cmd_dbdma(c1, DBDMA_NOP | BR_IFSET, virt_to_bus(c1-3)); c1++;
	tab_cmd_dbdma(c1++, DBDMA_NOP | INTR_ALWAYS, 0);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->ch1.br_sel),
							PLANB_SET(DMA_ABORT));

	if (interlace) {
		stepsize = 2;
		jump_addr = c1 + TAB_FACTOR * (nlines + 1) / 2;
	} else {
		stepsize = 1;
		jump_addr = c1 + TAB_FACTOR * nlines;
	}
	jump = virt_to_bus(jump_addr);

	/* even field data: */

	pagei = gbuf->idx;
#ifdef PLANB_GSCANLINE
	for (i = 0; i < nlines; i += stepsize) {
		tab_cmd_gen(c1++, INPUT_MORE | KEY_STREAM0 | BR_IFSET, count,
		    virt_to_bus(pb->rawbuf[pagei + i * scanline / PAGE_SIZE]),
									jump);
	}
#else
	i = 0;
	leftover1 = 0;
	do {
	    int j;

	    base = virt_to_bus(pb->rawbuf[pagei]);
	    nlpp = (PAGE_SIZE - leftover1) / count / stepsize;
	    for(j = 0; j < nlpp && i < nlines; j++, i += stepsize, c1++)
		tab_cmd_gen(c1, INPUT_MORE | KEY_STREAM0 | BR_IFSET,
			  count, base + count * j * stepsize + leftover1, jump);
	    if(i < nlines) {
		int lov0 = PAGE_SIZE - count * nlpp * stepsize - leftover1;

		if(lov0 == 0)
		    leftover1 = 0;
		else {
		    if(lov0 >= count) {
			/* can happen only when interlacing; then other field
			 * uses up leftover space (lov0 - count). */
			tab_cmd_gen(c1++, INPUT_MORE | BR_IFSET, count, base
				+ count * nlpp * stepsize + leftover1, jump);
		    } else {
			/* start of free space at end of page: */
			pb->l_to_addr[fr][gbuf->lnum] = pb->rawbuf[pagei]
					+ count * nlpp * stepsize + leftover1;
			/* index where continuation is: */
			pb->l_to_next_idx[fr][gbuf->lnum] = pagei + 1;
			/* How much is left to do in next page: */
			pb->l_to_next_size[fr][gbuf->lnum] = count - lov0;
			tab_cmd_gen(c1++, INPUT_MORE | BR_IFSET, count,
				virt_to_bus(pb->rawbuf[gbuf->l_fr_addr_idx
						+ gbuf->lnum]), jump);
			if(++gbuf->lnum > MAX_LNUM) {
				/* FIXME: error condition! */
				gbuf->lnum--;
		    	}
		    }
		    leftover1 = count * stepsize - lov0;
		    i += stepsize;
		}
	    }
	    pagei++;
	} while(i < nlines);
	tab_cmd_dbdma(c1, DBDMA_NOP | BR_ALWAYS, jump);
	c1 = jump_addr;
#endif /* PLANB_GSCANLINE */

	/* For non-interlaced, we use even fields only */
	if (!interlace)
		goto cmd_tab_data_end;

	/* Sync to odd field */
	tab_cmd_dbdma(c1++, DBDMA_NOP | WAIT_IFCLR, 0);
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->ch1.br_sel),
		PLANB_SET(ODD_FIELD));
	tab_cmd_dbdma(c1++, DBDMA_NOP | WAIT_IFSET, 0);
	tab_cmd_dbdma(c1, DBDMA_NOP | BR_IFCLR, virt_to_bus(c1-3)); c1++;
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->ch1.br_sel),
		PLANB_SET(DMA_ABORT));
	
	/* odd field data: */
	jump_addr = c1 + TAB_FACTOR * nlines / 2;
	jump = virt_to_bus(jump_addr);
#ifdef PLANB_GSCANLINE
	for (i = 1; i < nlines; i += stepsize) {
		tab_cmd_gen(c1++, INPUT_MORE | KEY_STREAM0 | BR_IFSET, count,
					virt_to_bus(pb->rawbuf[pagei
					+ i * scanline / PAGE_SIZE]), jump);
	}
#else
	i = 1;
	leftover1 = 0;
	pagei = gbuf->idx;
	if(nlines <= 1)
	    goto skip;
	do {
	    int j;

	    base = virt_to_bus(pb->rawbuf[pagei]);
	    nlpp = (PAGE_SIZE - leftover1) / count / stepsize;
	    if(leftover1 >= count) {
		tab_cmd_gen(c1++, INPUT_MORE | KEY_STREAM0 | BR_IFSET, count,
						base + leftover1 - count, jump);
		i += stepsize;
	    }
	    for(j = 0; j < nlpp && i < nlines; j++, i += stepsize, c1++)
		tab_cmd_gen(c1, INPUT_MORE | KEY_STREAM0 | BR_IFSET, count,
			base + count * (j * stepsize + 1) + leftover1, jump);
	    if(i < nlines) {
		int lov0 = PAGE_SIZE - count * nlpp * stepsize - leftover1;

		if(lov0 == 0)
		    leftover1 = 0;
		else {
		    if(lov0 > count) {
			pb->l_to_addr[fr][gbuf->lnum] = pb->rawbuf[pagei]
				+ count * (nlpp * stepsize + 1) + leftover1;
			pb->l_to_next_idx[fr][gbuf->lnum] = pagei + 1;
			pb->l_to_next_size[fr][gbuf->lnum] = count * stepsize
									- lov0;
			tab_cmd_gen(c1++, INPUT_MORE | BR_IFSET, count,
				virt_to_bus(pb->rawbuf[gbuf->l_fr_addr_idx
							+ gbuf->lnum]), jump);
			if(++gbuf->lnum > MAX_LNUM) {
				/* FIXME: error condition! */
				gbuf->lnum--;
			}
			i += stepsize;
		    }
		    leftover1 = count * stepsize - lov0;
		}
	    }
	    pagei++;
	} while(i < nlines);
skip:
	tab_cmd_dbdma(c1, DBDMA_NOP | BR_ALWAYS, jump);
	c1 = jump_addr;
#endif /* PLANB_GSCANLINE */

cmd_tab_data_end:
	tab_cmd_store(c1++, (unsigned)(&pb->planb_base_bus->intr_stat),
			(fr << 9) | PLANB_FRM_IRQ | PLANB_GEN_IRQ);
	/* stop it */
	tab_cmd_dbdma(c1, DBDMA_STOP, 0);

	eieio();
	return c1;
}

static void planb_irq(int irq, void *dev_id, struct pt_regs * regs)
{
	unsigned int stat, astat;
	struct planb *pb = (struct planb *)dev_id;

	IDBG("PlanB: planb_irq()\n");

	/* get/clear interrupt status bits */
	eieio();
	stat = readl(&pb->planb_base->intr_stat);
	astat = stat & pb->intr_mask;
	writel(PLANB_FRM_IRQ & ~astat & stat & ~PLANB_GEN_IRQ,
		&pb->planb_base->intr_stat);
	IDBG("PlanB: stat = %X, astat = %X\n", stat, astat);

	if(astat & PLANB_FRM_IRQ) {
		unsigned int	fr = stat >> 9;
		gbuf_ptr	gbuf = &pb->gbuf[fr];
#ifndef PLANB_GSCANLINE
		int		i;
#endif
		IDBG("PlanB: PLANB_FRM_IRQ\n");

		pb->gcount++;

		IDBG("PlanB: grab %d: fr = %d, gcount = %d\n",
				pb->grabbing, fr, pb->gcount);
#ifndef PLANB_GSCANLINE
		/* Now that the buffer is full, copy those lines that fell
		 * on a page boundary from the spare buffers back to where
		 * they belong. */
		IDBG("PlanB: %d * %d bytes are being copied over\n",
				gbuf->lnum, gbuf->lsize);
		for(i = 0; i < gbuf->lnum; i++) {
			int first = gbuf->lsize - pb->l_to_next_size[fr][i];

			memcpy(pb->l_to_addr[fr][i],
				pb->rawbuf[gbuf->l_fr_addr_idx + i],
				first);
			memcpy(pb->rawbuf[pb->l_to_next_idx[fr][i]],
				pb->rawbuf[gbuf->l_fr_addr_idx + i] + first,
						pb->l_to_next_size[fr][i]);
		}
#endif
		*gbuf->status = GBUFFER_DONE;
		pb->grabbing--;
		wake_up_interruptible(&pb->capq);
		return;
	}
	/* incorrect interrupts? */
	pb->intr_mask = PLANB_CLR_IRQ;
	writel(PLANB_CLR_IRQ, &pb->planb_base->intr_stat);
	printk(KERN_ERR "PlanB: IRQ lockup, cleared interrupts"
							" unconditionally\n");
}

/*******************************
 * Device Operations functions *
 *******************************/

static int planb_open(struct video_device *dev, int mode)
{
	struct planb	*pb = (struct planb *)dev->priv;
	int		err;

	/* first open on driver? */
	if(pb->vid_user + pb->vbi_user == 0) {
		if((err = planb_prepare_open(pb)) != 0)
			return err;
	}
	/* first open on video dev? */
	if(pb->vid_user == 0) {
		if((err = planb_prepare_video(pb)) != 0)
			return err;
	}
	pb->vid_user++;

	DBG("PlanB: device opened\n");

	MOD_INC_USE_COUNT;
	return 0;   
}

static void planb_close(struct video_device *dev)
{
	struct planb *pb = (struct planb *)dev->priv;

	planb_lock(pb);
	/* last close? then stop everything... */
	if(--pb->vid_user == 0) {
		if(pb->overlay) {
			planb_dbdma_stop(&pb->planb_base->ch2);
			planb_dbdma_stop(&pb->planb_base->ch1);
			pb->overlay = 0;
		}
		planb_close_video(pb);
	}
	/* last open on PlanB hardware? */
	if(pb->vid_user + pb->vbi_user == 0)
		planb_prepare_close(pb);
	planb_unlock(pb);

	DBG("PlanB: device closed\n");

	MOD_DEC_USE_COUNT;
	return;
}

static long planb_read(struct video_device *v, char *buf, unsigned long count,
				int nonblock)
{
	DBG("planb: read request\n");
	return -EINVAL;
}

static long planb_write(struct video_device *v, const char *buf,
				unsigned long count, int nonblock)
{
	DBG("planb: write request\n");
	return -EINVAL;
}

static int planb_ioctl(struct video_device *dev, unsigned int cmd, void *arg)
{
	struct planb *pb=(struct planb *)dev->priv;
  	
	switch (cmd)
	{	
		case VIDIOCGCAP:
		{
			struct video_capability b;

			DBG("PlanB: IOCTL VIDIOCGCAP\n");

			strcpy (b.name, pb->video_dev.name);
			b.type = VID_TYPE_OVERLAY | VID_TYPE_CLIPPING |
				 VID_TYPE_FRAMERAM | VID_TYPE_SCALES |
				 VID_TYPE_CAPTURE;
			b.channels = 2;	/* composite & svhs */
			b.audios = 0;
			b.maxwidth = PLANB_MAXPIXELS;
                        b.maxheight = PLANB_MAXLINES;
                        b.minwidth = 32; /* wild guess */
                        b.minheight = 32;
                        if (copy_to_user(arg,&b,sizeof(b)))
                                return -EFAULT;
			return 0;
		}
		case VIDIOCSFBUF:
		{
                        struct video_buffer v;
			unsigned int fmt;

			DBG("PlanB: IOCTL VIDIOCSFBUF\n");

                        if (!capable(CAP_SYS_ADMIN) && !capable(CAP_SYS_RAWIO))
                                return -EPERM;
                        if (copy_from_user(&v, arg, sizeof(v)))
                                return -EFAULT;
			planb_lock(pb);
			switch(v.depth) {
			    /* xawtv only asks for 8 bit in static grey, but
			     * there is no way to know what it really means.. */
			    case 8:
				fmt = VIDEO_PALETTE_GREY;
				break;
			    case 15:
				fmt = VIDEO_PALETTE_RGB555;
				break;
			    case 32:
				fmt = VIDEO_PALETTE_RGB32;
				break;
			    /* We don't deliver these two... */
			    case 16:
			    case 24:
			    default:
				planb_unlock(pb);
                                return -EINVAL;
			}
			if (palette2fmt[fmt].bpp * v.width > v.bytesperline) {
				planb_unlock(pb);
				return -EINVAL;
			}
			pb->win.bpp = palette2fmt[fmt].bpp;
			pb->win.color_fmt = fmt;
			pb->fb.phys = (unsigned long) v.base;
			pb->win.sheight = v.height;
			pb->win.swidth = v.width;
			pb->picture.depth = pb->win.depth = v.depth;
			pb->win.bpl = pb->win.bpp * pb->win.swidth;
			pb->win.pad = v.bytesperline - pb->win.bpl;

                        DBG("PlanB: Display at %p is %d by %d, bytedepth %d,"
				" bpl %d (+ %d)\n", v.base, v.width,v.height,
				pb->win.bpp, pb->win.bpl, pb->win.pad);

			pb->cmd_buff_inited = 0;
			if(pb->overlay) {
				suspend_overlay(pb);
				fill_cmd_buff(pb);
				resume_overlay(pb);
			}
			planb_unlock(pb);
			return 0;		
		}
		case VIDIOCGFBUF:
		{
                        struct video_buffer v;

			DBG("PlanB: IOCTL VIDIOCGFBUF\n");

			v.base = (void *)pb->fb.phys;
			v.height = pb->win.sheight;
			v.width = pb->win.swidth;
			v.depth = pb->win.depth;
			v.bytesperline = pb->win.bpl + pb->win.pad;
			if (copy_to_user(arg, &v, sizeof(v)))
                                return -EFAULT;
			return 0;
		}
		case VIDIOCCAPTURE:
		{
			int i;

                        if(copy_from_user(&i, arg, sizeof(i)))
                                return -EFAULT;
			if(i==0) {
				DBG("PlanB: IOCTL VIDIOCCAPTURE Stop\n");

				if (!(pb->overlay))
					return 0;
				planb_lock(pb);
				pb->overlay = 0;
				overlay_stop(pb);
				planb_unlock(pb);
			} else {
				DBG("PlanB: IOCTL VIDIOCCAPTURE Start\n");

				if (pb->fb.phys == 0 ||
					  pb->win.width == 0 ||
					  pb->win.height == 0)
					return -EINVAL;
				if (pb->overlay)
					return 0;
				planb_lock(pb);
				pb->overlay = 1;
				if(!(pb->cmd_buff_inited))
					fill_cmd_buff(pb);
				overlay_start(pb);
				planb_unlock(pb);
			}
			return 0;
		}
		case VIDIOCGCHAN:
		{
			struct video_channel v;

			DBG("PlanB: IOCTL VIDIOCGCHAN\n");

			if(copy_from_user(&v, arg,sizeof(v)))
				return -EFAULT;
			v.flags = 0;
			v.tuners = 0;
			v.type = VIDEO_TYPE_CAMERA;
			v.norm = pb->win.norm;
			switch(v.channel)
			{
			case 0:
				strcpy(v.name,"Composite");
				break;
			case 1:
				strcpy(v.name,"SVHS");
				break;
			default:
				return -EINVAL;
				break;
			}
			if(copy_to_user(arg,&v,sizeof(v)))
				return -EFAULT;

			return 0;
		}
		case VIDIOCSCHAN:
		{
			struct video_channel v;

			DBG("PlanB: IOCTL VIDIOCSCHAN\n");

			if(copy_from_user(&v, arg, sizeof(v)))
				return -EFAULT;

			if (v.norm != pb->win.norm) {
				int i, maxlines;

				switch (v.norm)
				{
				case VIDEO_MODE_PAL:
				case VIDEO_MODE_SECAM:
					maxlines = PLANB_MAXLINES;
					break;
				case VIDEO_MODE_NTSC:
					maxlines = PLANB_NTSC_MAXLINES;
					break;
				default:
					DBG("       invalid norm %d.\n", v.norm);
					return -EINVAL;
					break;
				}
				planb_lock(pb);
				/* empty the grabbing queue */
				while(pb->grabbing)
					interruptible_sleep_on(&pb->capq);
				pb->maxlines = maxlines;
				pb->win.norm = v.norm;
				/* Stop overlay if running */
				suspend_overlay(pb);
				for(i = 0; i < MAX_GBUFFERS; i++)
					pb->gbuf[i].norm_switch = 1;
				/* I know it's an overkill, but.... */
				fill_cmd_buff(pb);
				/* ok, now init it accordingly */
				saa_init_regs (pb);
				/* restart overlay if it was running */
				resume_overlay(pb);
				planb_unlock(pb);
			}

			switch(v.channel)
			{
			case 0:	/* Composite	*/
				saa_set (SAA7196_IOCC,
					((saa_regs[pb->win.norm][SAA7196_IOCC] &
					  ~7) | 3), pb);
				break;
			case 1:	/* SVHS		*/
				saa_set (SAA7196_IOCC,
					((saa_regs[pb->win.norm][SAA7196_IOCC] &
					  ~7) | 4), pb);
				break;
			default:
				DBG("       invalid channel %d.\n", v.channel);
				return -EINVAL;
				break;
			}

			return 0;
		}
		case VIDIOCGPICT:
		{
			struct video_picture vp = pb->picture;

			DBG("PlanB: IOCTL VIDIOCGPICT\n");

			vp.palette = pb->win.color_fmt;
			if(copy_to_user(arg,&vp,sizeof(vp)))
				return -EFAULT;
			return 0;
		}
		case VIDIOCSPICT:
		{
			struct video_picture vp;

			DBG("PlanB: IOCTL VIDIOCSPICT\n");

			if(copy_from_user(&vp,arg,sizeof(vp)))
				return -EFAULT;
			pb->picture = vp;
			/* Should we do sanity checks here? */
			planb_lock(pb);
			saa_set (SAA7196_BRIG, (unsigned char)
			    ((pb->picture.brightness) >> 8), pb);
			saa_set (SAA7196_HUEC, (unsigned char)
			    ((pb->picture.hue) >> 8) ^ 0x80, pb);
			saa_set (SAA7196_CSAT, (unsigned char)
			    ((pb->picture.colour) >> 9), pb);
			saa_set (SAA7196_CONT, (unsigned char)
			    ((pb->picture.contrast) >> 9), pb);
			planb_unlock(pb);

			return 0;
		}
		case VIDIOCSWIN:
		{
			struct video_window	vw;
			struct video_clip	clip;
			int 			i;
			
			DBG("PlanB: IOCTL VIDIOCSWIN\n");

			if(copy_from_user(&vw,arg,sizeof(vw)))
				return -EFAULT;

			planb_lock(pb);
			/* Stop overlay if running */
			suspend_overlay(pb);
			pb->win.interlace = (vw.height > pb->maxlines/2)? 1: 0;
			if (pb->win.x != vw.x ||
			    pb->win.y != vw.y ||
			    pb->win.width != vw.width ||
			    pb->win.height != vw.height ||
			    !pb->cmd_buff_inited) {
				pb->win.x = vw.x;
				pb->win.y = vw.y;
				pb->win.width = vw.width;
				pb->win.height = vw.height;
				fill_cmd_buff(pb);
			}
                        DBG("PlanB: Window at (%d,%d) size %dx%d\n", vw.x, vw.y, vw.width,
				vw.height);

			/* Reset clip mask */
			memset ((void *) pb->mask, 0xff, (pb->maxlines
					* ((PLANB_MAXPIXELS + 7) & ~7)) / 8);
			/* Add any clip rects */
			for (i = 0; i < vw.clipcount; i++) {
				if (copy_from_user(&clip, vw.clips + i,
						sizeof(struct video_clip)))
					return -EFAULT;
				add_clip(pb, &clip);
			}
			/* restart overlay if it was running */
			resume_overlay(pb);
			planb_unlock(pb);
			return 0;
		}
		case VIDIOCGWIN:
		{
			struct video_window vw;

			DBG("PlanB: IOCTL VIDIOCGWIN\n");

			vw.x=pb->win.x;
			vw.y=pb->win.y;
			vw.width=pb->win.width;
			vw.height=pb->win.height;
			vw.chromakey=0;
			vw.flags=0;
			if(pb->win.interlace)
				vw.flags|=VIDEO_WINDOW_INTERLACE;
			if(copy_to_user(arg,&vw,sizeof(vw)))
				return -EFAULT;
			return 0;
		}
	        case VIDIOCSYNC: {
			int		i;
			gbuf_ptr	gbuf;

			DBG("PlanB: IOCTL VIDIOCSYNC\n");

			if(copy_from_user((void *)&i,arg,sizeof(int)))
				return -EFAULT;

			DBG("PlanB: sync to frame %d\n", i);

                        if(i > (MAX_GBUFFERS - 1) || i < 0)
                                return -EINVAL;
			gbuf = &pb->gbuf[i];
chk_grab:
                        switch (*gbuf->status) {
                        case GBUFFER_UNUSED:
                                return -EINVAL;
			case GBUFFER_GRABBING:
				DBG("PlanB: waiting for grab"
							" done (%d)\n", i);
 			        interruptible_sleep_on(&pb->capq);
				if(signal_pending(current))
					return -EINTR;
				goto chk_grab;
                        case GBUFFER_DONE:
                                *gbuf->status = GBUFFER_UNUSED;
                                break;
                        }
                        return 0;
		}

	        case VIDIOCMCAPTURE:
		{
                        struct video_mmap vm;
			int		  fr;

			DBG("PlanB: IOCTL VIDIOCMCAPTURE\n");

			if(copy_from_user((void *) &vm,(void *)arg,sizeof(vm)))
				return -EFAULT;
			fr = vm.frame;
                        if(fr > (MAX_GBUFFERS - 1) || fr < 0)
                                return -EINVAL;
			if (*pb->gbuf[fr].status != GBUFFER_UNUSED)
				return -EBUSY;

			return vgrab(pb, &vm);
		}
		
		case VIDIOCGMBUF:
		{
			int i;
			struct video_mbuf vm;

			DBG("PlanB: IOCTL VIDIOCGMBUF\n");

			memset(&vm, 0 , sizeof(vm));
			vm.size = PLANB_MAX_FBUF * MAX_GBUFFERS;
			vm.frames = MAX_GBUFFERS;
			for(i = 0; i<MAX_GBUFFERS; i++)
				vm.offsets[i] = PLANB_MAX_FBUF * i;
			if(copy_to_user((void *)arg, (void *)&vm, sizeof(vm)))
				return -EFAULT;
			return 0;
		}
		
		case VIDIOCGUNIT:
		{
			struct video_unit vu;

			DBG("PlanB: IOCTL VIDIOCGUNIT\n");

			vu.video=pb->video_dev.minor;
			vu.vbi=pb->vbi_dev.minor;
			vu.radio=VIDEO_NO_UNIT;
			vu.audio=VIDEO_NO_UNIT;
			vu.teletext=VIDEO_NO_UNIT;
			if(copy_to_user((void *)arg, (void *)&vu, sizeof(vu)))
				return -EFAULT;
			return 0;
		}

		case PLANBIOCGSAAREGS:
		{
			struct planb_saa_regs preg;

			DBG("PlanB: IOCTL PLANBIOCGSAAREGS\n");

			if(copy_from_user(&preg, arg, sizeof(preg)))
				return -EFAULT;
			if(preg.addr >= SAA7196_NUMREGS)
				return -EINVAL;
			preg.val = saa_regs[pb->win.norm][preg.addr];
			if(copy_to_user((void *)arg, (void *)&preg,
								sizeof(preg)))
				return -EFAULT;
			return 0;
		}
		
		case PLANBIOCSSAAREGS:
		{
			struct planb_saa_regs preg;

			DBG("PlanB: IOCTL PLANBIOCSSAAREGS\n");

			if(copy_from_user(&preg, arg, sizeof(preg)))
				return -EFAULT;
			if(preg.addr >= SAA7196_NUMREGS)
				return -EINVAL;
			saa_set (preg.addr, preg.val, pb);
			return 0;
		}
		
		case PLANBIOCGSTAT:
		{
			struct planb_stat_regs pstat;

			DBG("PlanB: IOCTL PLANBIOCGSTAT\n");

			pstat.ch1_stat = readl(&pb->planb_base->ch1.status);
			pstat.ch2_stat = readl(&pb->planb_base->ch2.status);
			pstat.ch1_cmdbase = (unsigned long)pb->vid_cbo.start;
			pstat.ch2_cmdbase = (unsigned long)pb->clip_cbo.start;
			pstat.ch1_cmdptr = readl(&pb->planb_base->ch1.cmdptr);
			pstat.ch2_cmdptr = readl(&pb->planb_base->ch2.cmdptr);
			pstat.saa_stat0 = saa_status(0, pb);
			pstat.saa_stat1 = saa_status(1, pb);

			if(copy_to_user((void *)arg, (void *)&pstat,
							sizeof(pstat)))
				return -EFAULT;
			return 0;
		}

		case PLANBIOCSMODE: {
			int v;

			DBG("PlanB: IOCTL PLANBIOCSMODE\n");

			if(copy_from_user(&v, arg, sizeof(v)))
				return -EFAULT;

			switch(v)
			{
			case PLANB_TV_MODE:
				saa_set (SAA7196_STDC,
					(saa_regs[pb->win.norm][SAA7196_STDC] &
					  0x7f), pb);
				break;
			case PLANB_VTR_MODE:
				saa_set (SAA7196_STDC,
					(saa_regs[pb->win.norm][SAA7196_STDC] |
					  0x80), pb);
				break;
			default:
				return -EINVAL;
				break;
			}
			pb->win.mode = v;
			return 0;
		}
		case PLANBIOCGMODE: {
			int v=pb->win.mode;

			DBG("PlanB: IOCTL PLANBIOCGMODE\n");

			if(copy_to_user(arg,&v,sizeof(v)))
				return -EFAULT;
			return 0;
		}
#ifdef PLANB_GSCANLINE
		case PLANBG_GRAB_BPL: {
			int v=pb->gbytes_per_line;

			DBG("PlanB: IOCTL PLANBG_GRAB_BPL\n");

			if(copy_to_user(arg,&v,sizeof(v)))
				return -EFAULT;
			return 0;
		}
#endif /* PLANB_GSCANLINE */

/* These serve only for debugging... */
#ifdef DEBUG
		case PLANB_INTR_DEBUG: {
			int i;

			DBG("PlanB: IOCTL PLANB_INTR_DEBUG\n");

			if(copy_from_user(&i, arg, sizeof(i)))
				return -EFAULT;

			/* avoid hang ups all together */
			for (i = 0; i < MAX_GBUFFERS; i++) {
				if(*pb->gbuf[i].status == GBUFFER_GRABBING) {
					*pb->gbuf[i].status = GBUFFER_DONE;
				}
			}
			if(pb->grabbing)
				pb->grabbing--;
			wake_up_interruptible(&pb->capq);
			return 0;
		}
		case PLANB_INV_REGS: {
			int i;
			struct planb_any_regs any;

			DBG("PlanB: IOCTL PLANB_INV_REGS\n");

			if(copy_from_user(&any, arg, sizeof(any)))
				return -EFAULT;
			if(any.offset < 0 || any.offset + any.bytes > 0x400)
				return -EINVAL;
			if(any.bytes > 128)
				return -EINVAL;
			for (i = 0; i < any.bytes; i++) {
				any.data[i] =
					readb((unsigned char *)pb->planb_base
							+ any.offset + i);
			}
			if(copy_to_user(arg,&any,sizeof(any)))
				return -EFAULT;
			return 0;
		}
		case PLANBIOCGDBDMABUF:
		{
			struct planb_buf_regs buf;
			dbdma_cmd_ptr dc;
			int i;

			DBG("PlanB: IOCTL PLANBIOCGDBDMABUF\n");

			if(copy_from_user(&buf, arg, sizeof(buf)))
				return -EFAULT;
			buf.end &= ~0xf;
			if( (buf.start < 0) || (buf.end < 0x10) ||
			    (buf.end < buf.start+0x10) ||
			    (buf.end > 2*pb->tab_size) )
				return -EINVAL;

			printk ("PlanB DBDMA command buffer:\n");
			for (i=(buf.start>>4); i<=(buf.end>>4); i++) {
				printk(" 0x%04x:", i<<4);
				dc = pb->vid_cbo.start + i;
				printk (" %04x %04x %08x %08x %04x %04x\n",
				  dc->req_count, dc->command, dc->phy_addr,
				  dc->cmd_dep, dc->res_count, dc->xfer_status);
			}
			return 0;
		}
#endif /* DEBUG */

		default:
		{
			DBG("PlanB: Unimplemented IOCTL: %d (0x%x)\n", cmd, cmd);
			return -ENOIOCTLCMD;
		}
	/* Some IOCTLs are currently unsupported on PlanB */
		case VIDIOCGTUNER: {
		DBG("PlanB: IOCTL VIDIOCGTUNER\n");
			goto unimplemented; }
		case VIDIOCSTUNER: {
		DBG("PlanB: IOCTL VIDIOCSTUNER\n");
			goto unimplemented; }
		case VIDIOCSFREQ: {
		DBG("PlanB: IOCTL VIDIOCSFREQ\n");
			goto unimplemented; }
		case VIDIOCGFREQ: {
		DBG("PlanB: IOCTL VIDIOCGFREQ\n");
			goto unimplemented; }
		case VIDIOCKEY: {
		DBG("PlanB: IOCTL VIDIOCKEY\n");
			goto unimplemented; }
		case VIDIOCSAUDIO: {
		DBG("PlanB: IOCTL VIDIOCSAUDIO\n");
			goto unimplemented; }
		case VIDIOCGAUDIO: {
		DBG("PlanB: IOCTL VIDIOCGAUDIO\n");
			goto unimplemented; }
unimplemented:
		DBG("       Unimplemented\n");
			return -ENOIOCTLCMD;
	}
	return 0;
}

static int planb_mmap(struct video_device *dev, const char *adr, unsigned long size)
{
	struct planb	*pb = (struct planb *)dev->priv;
        unsigned long	start = (unsigned long)adr;
	int		i;

	if (size > MAX_GBUFFERS * PLANB_MAX_FBUF)
	        return -EINVAL;
	if (!pb->rawbuf) {
		int err;
		if((err=grabbuf_alloc(pb)))
			return err;
	}
	for (i = 0; i < pb->rawbuf_nchunks; i++) {
		if (remap_page_range(start, virt_to_phys((void *)pb->rawbuf[i]),
						PAGE_SIZE, PAGE_SHARED))
			return -EAGAIN;
		start += PAGE_SIZE;
		if (size <= PAGE_SIZE)
			break;
		size -= PAGE_SIZE;
	}
	return 0;
}

/**********************************
 * VBI device operation functions *
 **********************************/

static long planb_vbi_read(struct video_device *dev, char *buf,
	unsigned long count, int nonblock)
{
	struct planb	*pb = (struct planb *)dev->priv;
	int		q,todo;
	DECLARE_WAITQUEUE(wait, current);

/* Dummy for now */
	printk ("PlanB: VBI read %li bytes.\n", count);
	return (0);

	todo=count;
	while (todo && todo>(q=VBIBUF_SIZE-pb->vbip)) 
	{
		if(copy_to_user((void *) buf, (void *) pb->vbibuf+pb->vbip, q))
			return -EFAULT;
		todo-=q;
		buf+=q;

		add_wait_queue(&pb->vbiq, &wait);
		current->state = TASK_INTERRUPTIBLE;
		if (todo && q==VBIBUF_SIZE-pb->vbip) {
			if(nonblock) {
				remove_wait_queue(&pb->vbiq, &wait);
				current->state = TASK_RUNNING;
				if(count==todo)
					return -EWOULDBLOCK;
				return count-todo;
			}
			schedule();
			if(signal_pending(current)) {
				remove_wait_queue(&pb->vbiq, &wait);
				current->state = TASK_RUNNING;
				if(todo==count)
					return -EINTR;
				else
					return count-todo;
			}
		}
		remove_wait_queue(&pb->vbiq, &wait);
		current->state = TASK_RUNNING;
	}
	if (todo) {
		if(copy_to_user((void *) buf, (void *) pb->vbibuf+pb->vbip,
		    todo))
			return -EFAULT;
		pb->vbip+=todo;
	}
	return count;
}

static unsigned int planb_vbi_poll(struct video_device *dev,
	struct file *file, poll_table *wait)
{
	struct planb	*pb = (struct planb *)dev->priv;
	unsigned int	mask = 0;

	printk ("PlanB: VBI poll.\n");
	poll_wait(file, &pb->vbiq, wait);

	if (pb->vbip < VBIBUF_SIZE)
		mask |= (POLLIN | POLLRDNORM);

	return mask;
}

static int planb_vbi_open(struct video_device *dev, int flags)
{
	struct planb	*pb = (struct planb *)dev->priv;
	int		err;

	/* first open on the driver? */
	if(pb->vid_user + pb->vbi_user == 0) {
		if((err = planb_prepare_open(pb)) != 0)
			return err;
	}
	/* first open on the vbi device? */
	if(pb->vbi_user == 1) {
		if((err = planb_prepare_vbi(pb)) != 0)
			return err;
	}
	++pb->vbi_user;

	DBG("PlanB: VBI open\n");

	MOD_INC_USE_COUNT;
	return 0;   
}

static void planb_vbi_close(struct video_device *dev)
{
	struct planb	*pb = (struct planb *)dev->priv;

	/* last close on vbi device? */
	if(--pb->vbi_user == 0) {
		planb_close_vbi(pb);
	}
	/* last close on any planb device? */
	if(pb->vid_user + pb->vbi_user == 0) {
		planb_prepare_close(pb);
	}

	DBG("PlanB: VBI close\n");

	MOD_DEC_USE_COUNT;  
	return;
}

static int planb_vbi_ioctl(struct video_device *dev, unsigned int cmd,
	void *arg)
{
	switch (cmd) {  
		/* This is only for alevt */
		case BTTV_VBISIZE:
			DBG("PlanB: IOCTL BTTV_VBISIZE.\n");
			return VBIBUF_SIZE;
		default:
			DBG("PlanB: Unimplemented VBI IOCTL no. %i.\n", cmd);
			return -EINVAL;
	}
}

static struct video_device planb_template=
{
	owner:		THIS_MODULE,
	name:		PLANB_DEVICE_NAME,
	type:		VID_TYPE_CAPTURE|VID_TYPE_OVERLAY,
	hardware:	VID_HARDWARE_PLANB,
	open:		planb_open,
	close:		planb_close,
	read:		planb_read,
	write:		planb_write,	/* not implemented */
	ioctl:		planb_ioctl,
	mmap:		planb_mmap,	/* mmap? */
};

static struct video_device planb_vbi_template=
{
	owner:		THIS_MODULE,
	name:		PLANB_VBI_NAME,
	type:		VID_TYPE_CAPTURE|VID_TYPE_TELETEXT,
	hardware:	VID_HARDWARE_PLANB,
	open:		planb_vbi_open,
	close:		planb_vbi_close,
	read:		planb_vbi_read,
	write:		planb_write,	/* not implemented */
	poll:		planb_vbi_poll,
	ioctl:		planb_vbi_ioctl,
};

static int __devinit init_planb(struct planb *pb)
{
	unsigned char saa_rev;
	int i, result;
	unsigned long flags;

	printk(KERN_INFO "PlanB: PowerMacintosh video input driver rev. %s\n", PLANB_REV);

	pb->video_dev.minor = -1;
	pb->vid_user = 0;

	/* Simple sanity check */
	if(def_norm >= NUM_SUPPORTED_NORM || def_norm < 0) {
		printk(KERN_ERR "PlanB: Option(s) invalid\n");
		return -2;
	}
	memset ((void *) &pb->win, 0, sizeof (struct planb_window));
	pb->win.norm = def_norm;
	pb->win.mode = PLANB_TV_MODE;	/* TV mode */
	pb->win.interlace = 1;
	pb->win.x = 0;
	pb->win.y = 0;
	pb->win.width = 768; /* 640 */
	pb->win.height = 576; /* 480 */
	pb->win.pad = 0;
	pb->win.bpp = 4;
	pb->win.depth = 32;
	pb->win.color_fmt = VIDEO_PALETTE_RGB32;
	pb->win.bpl = 1024 * pb->win.bpp;
	pb->win.swidth = 1024;
	pb->win.sheight = 768;
	pb->maxlines = 576;
#ifdef PLANB_GSCANLINE
	if((pb->gbytes_per_line = PLANB_MAXPIXELS * 4) > PAGE_SIZE
				|| (pb->gbytes_per_line <= 0))
		return -3;
	else {
		/* page align pb->gbytes_per_line for DMA purpose */
		for(i = PAGE_SIZE; pb->gbytes_per_line < (i >> 1);)
			i >>= 1;
		pb->gbytes_per_line = i;
	}
#endif
	pb->tab_size = PLANB_MAXLINES + 40;
	pb->suspend = 0;
	init_MUTEX(&pb->lock);
	pb->vid_cbo.start = 0;
	pb->clip_cbo.start = 0;
	pb->mask = 0;
	pb->vid_raw = 0;
	pb->overlay = 0;
	init_waitqueue_head(&pb->suspendq);
	pb->cmd_buff_inited = 0;
	pb->fb.phys = 0;
	pb->fb.offset = 0;

	/* VBI stuff: */
	pb->vbi_dev.minor = -1;
	pb->vbi_user = 0;
	pb->vbirunning = 0;
	pb->vbip = 0;
	pb->vbibuf = 0;
	init_waitqueue_head(&pb->vbiq);

	/* Reset DMA controllers */
	planb_dbdma_stop(&pb->planb_base->ch2);
	planb_dbdma_stop(&pb->planb_base->ch1);

	saa_rev =  (saa_status(0, pb) & 0xf0) >> 4;
	printk(KERN_INFO "PlanB: SAA7196 video processor rev. %d\n", saa_rev);
	/* Initialize the SAA registers in memory and on chip */
	saa_init_regs (pb);

	/* clear interrupt mask */
	pb->intr_mask = PLANB_CLR_IRQ;

	save_flags(flags); cli();
        result = request_irq(pb->irq, planb_irq, 0, "PlanB", (void *)pb);
        if (result < 0) {
	        if (result==-EINVAL)
	                printk(KERN_ERR "PlanB: Bad irq number (%d) "
						"or handler\n", (int)pb->irq);
		else if (result==-EBUSY)
			printk(KERN_ERR "PlanB: I don't know why, "
					"but IRQ %d is busy\n", (int)pb->irq);
		restore_flags(flags);
		return result;
	}
	disable_irq(pb->irq);
	restore_flags(flags);
        
	pb->picture.brightness=0x90<<8;
	pb->picture.contrast = 0x70 << 8;
	pb->picture.colour = 0x70<<8;
	pb->picture.hue = 0x8000;
	pb->picture.whiteness = 0;
	pb->picture.depth = pb->win.depth;

	init_waitqueue_head(&pb->capq);
	for(i=0; i<MAX_GBUFFERS; i++) {
		gbuf_ptr	gbuf = &pb->gbuf[i];

		gbuf->idx = PLANB_MAX_FBUF * i / PAGE_SIZE;
		gbuf->width=0;
		gbuf->height=0;
		gbuf->fmt=0;
		gbuf->cap_cmd=NULL;
#ifndef PLANB_GSCANLINE
		gbuf->l_fr_addr_idx = MAX_GBUFFERS * (PLANB_MAX_FBUF
						/ PAGE_SIZE + 1) + MAX_LNUM * i;
		gbuf->lsize = 0;
		gbuf->lnum = 0;
#endif
	}
	pb->rawbuf=NULL;
	pb->grabbing=0;

	/* enable interrupts */
	writel(PLANB_CLR_IRQ, &pb->planb_base->intr_stat);
	pb->intr_mask = PLANB_FRM_IRQ;
	enable_irq(pb->irq);

	/* Now add the templates and register the device units. */
	memcpy(&pb->video_dev,&planb_template,sizeof(planb_template));
	pb->video_dev.priv = pb;
	memcpy(&pb->vbi_dev,&planb_vbi_template,sizeof(planb_vbi_template));
	
	if(video_register_device(&pb->video_dev, VFL_TYPE_GRABBER, video_nr)<0)
		return -1;
	if(video_register_device(&pb->vbi_dev, VFL_TYPE_VBI, vbi_nr)<0) {
		video_unregister_device(&pb->video_dev);
		return -1;
	}

	return 0;
}

/*
 *	Scan for a PlanB controller and map the io memory 
 */
static int find_planb(void)
{
	struct planb		*pb;
	struct pci_dev 		*pdev = NULL;
	unsigned long		base;
	int			planb_num = 0;

	if (_machine != _MACH_Pmac)
		return 0;

	pdev = pci_find_device(APPLE_VENDOR_ID, PLANB_DEV_ID, pdev);
	if (pdev == NULL) {
		printk(KERN_WARNING "PlanB: no device found!\n");
		return planb_num;
	}

	pb = &planbs;
	planb_num = 1;
	base = pdev->resource[0].start;

	DBG("PlanB: Found device %s, membase 0x%lx, irq %d\n",
		pdev->slot_name, base, pdev->irq);

	/* Enable response in memory space, bus mastering,
	   use memory write and invalidate */
	pci_enable_device (pdev);
	pci_set_master (pdev);
	pci_set_mwi(pdev);
	/* value copied from MacOS... */
	pci_write_config_byte (pdev, PCI_LATENCY_TIMER, 0x40);

	planb_regs = (volatile struct planb_registers *)
						ioremap (base, 0x400);
	pb->planb_base = planb_regs;
	pb->planb_base_bus = (struct planb_registers *)base;
	pb->irq	= pdev->irq;
	
	return planb_num;
}

static void release_planb(void)
{
	struct planb *pb;

	pb=&planbs;

	/* stop and flush DMAs unconditionally */
	planb_dbdma_stop(&pb->planb_base->ch2);
	planb_dbdma_stop(&pb->planb_base->ch1);

	/* clear and free interrupts */
	pb->intr_mask = PLANB_CLR_IRQ;
	writel(PLANB_CLR_IRQ, &pb->planb_base->intr_stat);
	free_irq(pb->irq, pb);

	/* make sure all allocated memory are freed */
	planb_prepare_close(pb);

	printk(KERN_INFO "PlanB: unregistering with v4l\n");
	video_unregister_device(&pb->video_dev);
	video_unregister_device(&pb->vbi_dev);

	/* note that iounmap() does nothing on the PPC right now */
	iounmap ((void *)pb->planb_base);
}

static int __init init_planbs(void)
{
	int planb_num;

	planb_num=find_planb();

	if (planb_num < 0)
		return -EIO;
	if (planb_num == 0)
		return -ENXIO;

	if (init_planb(&planbs) < 0) {
		printk(KERN_ERR "PlanB: error registering planb device"
						" with v4l\n");
		release_planb();
		return -EIO;
	} 
	return 0;
}

static void __exit exit_planbs(void)
{
	release_planb();
}

module_init(init_planbs);
module_exit(exit_planbs);
