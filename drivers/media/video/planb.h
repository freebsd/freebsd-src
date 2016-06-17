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

/* $Id: planb.h,v 2.9 2002/04/03 15:57:57 mlan Exp mlan $ */

#ifndef _PLANB_H_
#define _PLANB_H_

#define PLANB_DEVICE_NAME	"Apple PlanB Video-In"
#define PLANB_VBI_NAME		"Apple PlanB VBI"
#define PLANB_REV		"2.11"

#define APPLE_VENDOR_ID		0x106b
#define PLANB_DEV_ID		0x0004

#ifdef __KERNEL__
//#define PLANB_GSCANLINE	/* use this if apps have the notion of */
				/* grab buffer scanline */
/* This should be safe for both PAL and NTSC */
#define PLANB_MAXPIXELS 768
#define PLANB_MAXLINES 576
#define PLANB_NTSC_MAXLINES 480

/* Max VBI data buffer size */
#define VBI_LINESIZE 1024	/* on SAA7196, a line can be max. 1024 pixels */
#define VBI_START 7		/* VBI starts at line 7 */
#define VBI_MAXLINES 16		/* 16 lines per field */
/* We have 2 of these, but return them one at a time */
#define VBIBUF_SIZE (VBI_LINESIZE * VBI_MAXLINES)

#define LINE_OFFSET 1		/* between line 1 and SAA's first valid line */

/* Uncomment your preferred norm ;-) */
#define PLANB_DEF_NORM VIDEO_MODE_PAL
//#define PLANB_DEF_NORM VIDEO_MODE_NTSC
//#define PLANB_DEF_NORM VIDEO_MODE_SECAM

/* fields settings */
#define PLANB_SIZE8	0x1	/*  8-bit mono? */
#define PLANB_SIZE16	0x2	/* 16-bit mode */
#define PLANB_SIZE32	0x4	/* 32-bit mode */
#define PLANB_CLIPMASK	0x8	/* hardware clipmasking */

/* misc. flags for PlanB DMA operation */
#define	CH_SYNC		0x1	/* synchronize channels (set by ch1;
				   cleared by ch2) */
#define FIELD_SYNC	0x2     /* used for the start of each field
				   (0 -> 1 -> 0 for ch1; 0 -> 1 for ch2) */
#define EVEN_FIELD	0x0	/* even field is detected if unset */
#define DMA_ABORT	0x2	/* error or just out of sync if set */
#define ODD_FIELD	0x4	/* odd field is detected if set */

/* format info and correspondance */
struct fmts {
	int	bpp;		/* bytes per pixel */
	int	pb_fmt;		/* planb format (DMA engine sub 0x40/0x44 ) */
	int	saa_fmt;	/* saa format:				bit
				   SAA7196 sub 0x20: bits FS0		0
				   			  FS1		1
					   sub 0x30: bit  MCT		4
							  LLV		5 */
};

/* This is supposed to match the VIDEO_PALETTE_* defines in
 * struct video_picture in videodev.h */
static struct fmts palette2fmt[] = {
	{ 0, 0,			   0 },
	{ 1, PLANB_SIZE8,	0x33 },	/* VIDEO_PALETTE_GREY */
	{ 0, 0,			   0 },
	{ 0, 0,			   0 },
	{ 0, 0,			   0 },
	{ 4, PLANB_SIZE32,	   2 },	/* VIDEO_PALETTE_RGB32 */
	{ 2, PLANB_SIZE16,	   0 },	/* VIDEO_PALETTE_RGB555 */
	{ 2, PLANB_SIZE16,	0x21 },	/* VIDEO_PALETTE_YUV422 */
	{ 0, 0,			   0 },
	{ 0, 0,			   0 },
	{ 0, 0,			   0 },
	{ 0, 0,			   0 },
	{ 0, 0,			   0 },
	{ 0, 0,			   0 },
	{ 0, 0,			   0 },
	{ 0, 0,			   0 },
	{ 0, 0,			   0 },
};

#define PLANB_PALETTE_MAX (sizeof palette2fmt / sizeof (struct fmts))

/* for capture operations */
#define MAX_GBUFFERS	2
/* note PLANB_MAX_FBUF must be divisible by PAGE_SIZE */
#ifdef PLANB_GSCANLINE
#define PLANB_MAX_FBUF	0x240000	/* 576 * 1024 * 4 */
#define TAB_FACTOR	(1)
#else
#define PLANB_MAX_FBUF	0x1b0000	/* 576 * 768 * 4 */
#define TAB_FACTOR	(2)
#endif
#endif /* __KERNEL__ */

struct planb_saa_regs {
	unsigned char addr;
	unsigned char val;
};

struct planb_stat_regs {
	unsigned int ch1_stat;
	unsigned int ch2_stat;
	unsigned long ch1_cmdbase;
	unsigned long ch2_cmdbase;
	unsigned int ch1_cmdptr;
	unsigned int ch2_cmdptr;
	unsigned char saa_stat0;
	unsigned char saa_stat1;
};

struct planb_any_regs {
	unsigned int offset;
	unsigned int bytes;
	unsigned char data[128];
};

struct planb_buf_regs {
	unsigned int start;
	unsigned int end;
};

/* planb private ioctls */
/* Read a saa7196 reg value */
#define PLANBIOCGSAAREGS	_IOWR('v', BASE_VIDIOCPRIVATE, struct planb_saa_regs)
/* Set a saa7196 reg value */
#define PLANBIOCSSAAREGS	_IOW('v', BASE_VIDIOCPRIVATE + 1, struct planb_saa_regs)
/* Read planb status */
#define PLANBIOCGSTAT		_IOR('v', BASE_VIDIOCPRIVATE + 2, struct planb_stat_regs)
/* Get TV/VTR mode */
#define PLANB_TV_MODE		1
#define PLANB_VTR_MODE		2
#define PLANBIOCGMODE		_IOR('v', BASE_VIDIOCPRIVATE + 3, int)
/* Set TV/VTR mode */
#define PLANBIOCSMODE		_IOW('v', BASE_VIDIOCPRIVATE + 4, int)

#ifdef PLANB_GSCANLINE
/* # of bytes per scanline in grab buffer */
#define PLANBG_GRAB_BPL		_IOR('v', BASE_VIDIOCPRIVATE + 5, int)
#endif

/* This doesn't really belong here, but until someone cleans up (or defines
   in the first place ;-) the VBI API, it helps alevt... */
#define BTTV_VBISIZE		_IOR('v', BASE_VIDIOCPRIVATE + 8, int)

/* Various debugging IOCTLs */
#ifdef DEBUG
/* call wake_up_interruptible() with appropriate actions */
#define PLANB_INTR_DEBUG	_IOW('v', BASE_VIDIOCPRIVATE + 20, int)
/* investigate which reg does what */
#define PLANB_INV_REGS		_IOWR('v', BASE_VIDIOCPRIVATE + 21, struct planb_any_regs)
/* Dump DBDMA command buffer from (int) to (int) */
#define PLANBIOCGDBDMABUF	_IOW('v', BASE_VIDIOCPRIVATE + 22, struct planb_buf_regs)
#endif /* DEBUG */

#ifdef __KERNEL__

/* Potentially useful macros */
#define PLANB_SET(x)	((x) << 16 | (x))
#define PLANB_CLR(x)	((x) << 16)

typedef volatile struct dbdma_cmd dbdma_cmd_t;
typedef volatile struct dbdma_cmd *dbdma_cmd_ptr;
typedef volatile struct dbdma_regs dbdma_regs_t;
typedef volatile struct dbdma_regs *dbdma_regs_ptr;

typedef struct gbuffer gbuf_t;
typedef struct gbuffer *gbuf_ptr;

/* grab buffer status */
#define GBUFFER_UNUSED		0x00U
#define GBUFFER_GRABBING	0x01U
#define GBUFFER_DONE		0x02U

/* planb interrupt status values (0x104: irq status) */
#define PLANB_CLR_IRQ		0x00		/* clear Plan B interrupt */
#define PLANB_GEN_IRQ		0x01		/* assert Plan B interrupt */
#define PLANB_FRM_IRQ		0x0100		/* end of frame */

#define PLANB_DUMMY 40	/* # of command buf's allocated for pre-capture seq. */

/* This represents the physical register layout */
struct planb_registers {
	dbdma_regs_t		ch1;		/* 0x00: video in */
	volatile u32		even;		/* 0x40: even field setting */
	volatile u32		odd;		/* 0x44; odd field setting */
	u32			pad1[14];	/* empty? */
	dbdma_regs_t		ch2;		/* 0x80: clipmask out */
	u32			pad2[16];	/* 0xc0: empty? */
	volatile u32		reg3;		/* 0x100: ???? */
	volatile u32		intr_stat;	/* 0x104: irq status */
	u32			pad3[1];	/* empty? */
	volatile u32		reg5;		/* 0x10c: ??? */
	u32			pad4[60];	/* empty? */
	volatile u8		saa_addr;	/* 0x200: SAA subadr */
	u8			pad5[3];
	volatile u8		saa_regval;	/* SAA7196 write reg. val */
	u8			pad6[3];
	volatile u8		saa_status;	/* SAA7196 status byte */
	/* There is more unused stuff here */
};

struct planb_window {
	int	x, y;
	ushort	width, height;
	ushort	bpp, bpl, depth, pad;
	ushort	swidth, sheight;
	int	norm;
	int	interlace;
	u32	color_fmt;
	int	chromakey;
	int	mode;		/* used to switch between TV/VTR modes */
};

struct planb_suspend {
	int overlay;
	int frame;
	struct dbdma_cmd cmd;
};

/* Framebuffer info */
struct planb_fb {
	unsigned long	phys;		/* Framebuffer phys. base address */
	int		offset;		/* offset of pixel 1 */
};

/* DBDMA command buffer descriptor */
struct dbdma_cmd_buf {
	dbdma_cmd_ptr	start;
	dbdma_cmd_ptr	jumpaddr;	/* where are we called from? */
	unsigned int	size;
	unsigned long	bus;		/* start address as seen from the bus */
};

/* grab buffer descriptor */
struct gbuffer {
	dbdma_cmd_ptr		cap_cmd;
	dbdma_cmd_ptr		last_cmd;
	dbdma_cmd_ptr		pre_cmd;
	int			idx;
	int			need_pre_capture;
	int			width;
	int			height;
	unsigned int		fmt;
	int			norm_switch;
#ifndef PLANB_GSCANLINE
	int			l_fr_addr_idx;
	int			lsize;
	int			lnum;
#endif
        volatile unsigned int	*status;	/* ptr to status value */
};

struct planb {
/* the video device: */
	struct video_device	video_dev;
	struct video_picture	picture;	/* Current picture params */
	int			vid_user;	/* Users on video device */
	void			*vid_raw;	/* Org. alloc. mem for kfree */
	struct dbdma_cmd_buf	vid_cbo;	/* odd video dbdma cmd buf */
	struct dbdma_cmd_buf	vid_cbe;	/* even */
	void			*clip_raw;
	struct dbdma_cmd_buf	clip_cbo;	/* odd clip dbdma cmd buf */
	struct dbdma_cmd_buf	clip_cbe;	/* even */
	dbdma_cmd_ptr		overlay_last1;
	dbdma_cmd_ptr		overlay_last2;
  
/* the hardware: */
	volatile struct planb_registers
				*planb_base;	/* virt base of planb */
	struct planb_registers	*planb_base_bus; /* phys base of planb */
	unsigned int		tab_size;
	int     		maxlines;
	unsigned int		irq;		/* interrupt number */
	volatile unsigned int	intr_mask;
	struct planb_fb		fb;		/* Framebuffer info */

/* generic stuff: */
	void			*jump_raw;	/* jump buffer raw space */
	dbdma_cmd_ptr		jumpbuf;	/* same, DBDMA_ALIGN'ed */
	struct semaphore	lock;
	int			overlay;	/* overlay running? */
	struct planb_window	win;
	volatile unsigned char	*mask;		/* Clipmask buffer */
	int			suspend;
	wait_queue_head_t	suspendq;
	struct planb_suspend	suspended;
	int			cmd_buff_inited; /* cmd buffer inited? */

/* grabbing stuff: */ 
	int			grabbing;
	unsigned int		gcount;
	wait_queue_head_t	capq;
	int			last_fr;
	int			prev_last_fr;
	unsigned char		**rawbuf;
	int			rawbuf_nchunks;
	struct gbuffer		gbuf[MAX_GBUFFERS];

#ifdef PLANB_GSCANLINE
	int			gbytes_per_line;
#else
#define MAX_LNUM 576	/* change this if PLANB_MAXLINES or */
			/* PLANB_MAXPIXELS changes */
	unsigned char		*l_to_addr[MAX_GBUFFERS][MAX_LNUM];
	int			l_to_next_idx[MAX_GBUFFERS][MAX_LNUM];
	int			l_to_next_size[MAX_GBUFFERS][MAX_LNUM];
#endif /* PLANB_GSCANLINE */

/* VBI stuff: */
	struct video_device	vbi_dev;	/* VBI data device */
	int			vbi_user;	/* Users on vbi device */
	void			*vbi_raw;
	struct dbdma_cmd_buf	vbi_cbo;	/* odd VBI dbdma cmd buf */
	struct dbdma_cmd_buf	vbi_cbe;	/* even */
	int			vbirunning;
	int			vbip;		/* pointer into VBI buffer */
	unsigned char		*vbibuf;	/* buffer for VBI data */
	wait_queue_head_t	vbiq;
};

#endif /* __KERNEL__ */

#endif /* _PLANB_H_ */
