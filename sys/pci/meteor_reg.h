/*
 * Copyright (c) 1995 Mark Tinguely and Jim Lowe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mark Tinguely and Jim Lowe
 * 4. The name of the author may not be used to endorse or promote products 
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/pci/meteor_reg.h,v 1.5.2.1 2000/08/03 01:09:11 peter Exp $
 */
#ifndef PCI_LATENCY_TIMER
#define	PCI_LATENCY_TIMER		0x0c	/* pci timer register */
#endif

/*
 * Definitions for the Philips SAA7116 digital video to pci interface.
 */
#define	SAA7116_PHILIPS_ID			0x12238086ul
#define	SAA7116_I2C_WRITE			0x00
#define	SAA7116_I2C_READ		 	0x01
#define	SAA7116_IIC_NEW_CYCLE			0x1000000L
#define	SAA7116_IIC_DIRECT_TRANSFER_ABORTED	0x0000200L

typedef volatile u_int 	mreg_t;
struct saa7116_regs {
	mreg_t	dma1e;		/* Base address for even field dma chn 1 */
	mreg_t	dma2e;		/* Base address for even field dma chn 2 */
	mreg_t	dma3e;		/* Base address for even field dma chn 3 */
	mreg_t	dma1o;		/* Base address for odd field dma chn 1 */
	mreg_t	dma2o;		/* Base address for odd field dma chn 2 */
	mreg_t	dma3o;		/* Base address for odd field dma chn 3 */
	mreg_t	stride1e;	/* Address stride for even field dma chn 1 */
	mreg_t	stride2e;	/* Address stride for even field dma chn 2 */
	mreg_t	stride3e;	/* Address stride for even field dma chn 3 */
	mreg_t	stride1o;	/* Address stride for odd field dma chn 1 */
	mreg_t	stride2o;	/* Address stride for odd field dma chn 2 */
	mreg_t	stride3o;	/* Address stride for odd field dma chn 3 */
	mreg_t 	routee;		/* Route/mode even */
	mreg_t 	routeo;		/* Route/mode odd */
	mreg_t 	fifo_t;		/* FIFO trigger for PCI int */
	mreg_t 	field_t;	/* Field toggle */
	mreg_t 	cap_cntl;	/* Capture control */
	mreg_t 	retry_wait_cnt;	/* Clks for master to wait after disconnect */
	mreg_t 	irq_stat;	/* IRQ mask and status reg */
	mreg_t 	fme;		/* Field Mask even */
	mreg_t 	fmo;		/* Field mask odd */
	mreg_t 	fml;		/* Field mask length */
	mreg_t 	fifo_t_err;	/* FIFO almost empty/almost full ptrs */
	mreg_t	i2c_phase;	/* i2c phase register */
	mreg_t	i2c_read;	/* i2c read register */
	mreg_t	i2c_write;	/* i2c write register */
	mreg_t	i2c_auto_a_e;	/* i2c auto register a, even */
	mreg_t	i2c_auto_b_e;	/* i2c auto register b, even */
	mreg_t	i2c_auto_c_e;	/* i2c auto register c, even */
	mreg_t	i2c_auto_d_e;	/* i2c auto register d, even */
	mreg_t	i2c_auto_a_o;	/* i2c auto register a, odd */
	mreg_t	i2c_auto_b_o;	/* i2c auto register b, odd */
	mreg_t	i2c_auto_c_o;	/* i2c auto register c, odd */
	mreg_t	i2c_auto_d_o;	/* i2c auto register d, odd */
	mreg_t	i2c_auto_enable;/* enable above auto registers */
	mreg_t	dma_end_e;	/* DMA end even (range) */
	mreg_t	dma_end_o;	/* DMA end odd (range) */
};


/*
 * Definitions for the Philips SAA7196 digital video decoder,
 * scalar, and clock generator circuit (DESCpro).
 */
#define NUM_SAA7196_I2C_REGS	49
#define	SAA7196_I2C_ADDR	0x40
#define	SAA7196_WRITE(mtr, reg, data) \
	i2c_write(mtr, SAA7196_I2C_ADDR, SAA7116_I2C_WRITE, reg, data), \
	mtr->saa7196_i2c[reg] = data
#define SAA7196_REG(mtr, reg) mtr->saa7196_i2c[reg]
#define	SAA7196_READ(mtr) \
	i2c_write(mtr, SAA7196_I2C_ADDR, SAA7116_I2C_READ, 0x0, 0x0)

#define SAA7196_IDEL	0x00	/* Increment delay */
#define SAA7196_HSB5	0x01	/* H-sync begin; 50 hz */
#define SAA7196_HSS5	0x02	/* H-sync stop; 50 hz */
#define SAA7196_HCB5	0x03	/* H-clamp begin; 50 hz */
#define SAA7196_HCS5	0x04	/* H-clamp stop; 50 hz */
#define SAA7196_HSP5	0x05	/* H-sync after PHI1; 50 hz */
#define SAA7196_LUMC	0x06	/* Luminance control */
#define SAA7196_HUEC	0x07	/* Hue control */
#define SAA7196_CKTQ	0x08	/* Colour Killer Threshold QAM (PAL, NTSC) */
#define SAA7196_CKTS	0x09	/* Colour Killer Threshold SECAM */
#define SAA7196_PALS	0x0a	/* PAL switch sensitivity */
#define SAA7196_SECAMS	0x0b	/* SECAM switch sensitivity */
#define SAA7196_CGAINC	0x0c	/* Chroma gain control */
#define SAA7196_STDC	0x0d	/* Standard/Mode control */
#define SAA7196_IOCC	0x0e	/* I/O and Clock Control */
#define SAA7196_CTRL1	0x0f	/* Control #1 */
#define SAA7196_CTRL2	0x10	/* Control #2 */
#define SAA7196_CGAINR	0x11	/* Chroma Gain Reference */
#define SAA7196_CSAT	0x12	/* Chroma Saturation */
#define SAA7196_CONT	0x13	/* Luminance Contrast */
#define SAA7196_HSB6	0x14	/* H-sync begin; 60 hz */
#define SAA7196_HSS6	0x15	/* H-sync stop; 60 hz */
#define SAA7196_HCB6	0x16	/* H-clamp begin; 60 hz */
#define SAA7196_HCS6	0x17	/* H-clamp stop; 60 hz */
#define SAA7196_HSP6	0x18	/* H-sync after PHI1; 60 hz */
#define SAA7196_BRIG	0x19	/* Luminance Brightness */
#define SAA7196_FMTS	0x20	/* Formats and sequence */
#define SAA7196_OUTPIX	0x21	/* Output data pixel/line */
#define SAA7196_INPIX	0x22	/* Input data pixel/line */
#define SAA7196_HWS	0x23	/* Horiz. window start */
#define SAA7196_HFILT	0x24	/* Horiz. filter */
#define SAA7196_OUTLINE	0x25	/* Output data lines/field */
#define SAA7196_INLINE	0x26	/* Input data lines/field */
#define SAA7196_VWS	0x27	/* Vertical window start */
#define SAA7196_VYP	0x28	/* AFS/vertical Y processing */
#define SAA7196_VBS	0x29	/* Vertical Bypass start */
#define SAA7196_VBCNT	0x2a	/* Vertical Bypass count */
#define SAA7196_VBP	0x2b	/* veritcal Bypass Polarity */
#define SAA7196_VLOW	0x2c	/* Colour-keying lower V limit */
#define SAA7196_VHIGH	0x2d	/* Colour-keying upper V limit */
#define SAA7196_ULOW	0x2e	/* Colour-keying lower U limit */
#define SAA7196_UHIGH	0x2f	/* Colour-keying upper U limit */
#define SAA7196_DPATH	0x30	/* Data path setting  */

/*
 * Defines for the PCF8574.
 */
#define NUM_PCF8574_I2C_REGS	2
#define	PCF8574_CTRL_I2C_ADDR	0x70
#define PCF8574_DATA_I2C_ADDR	0x72
#define	PCF8574_CTRL_WRITE(mtr, data) \
	i2c_write(mtr,  PCF8574_CTRL_I2C_ADDR, SAA7116_I2C_WRITE, data, data), \
	mtr->pcf_i2c[0] = data
#define	PCF8574_DATA_WRITE(mtr, data) \
	i2c_write(mtr,  PCF8574_DATA_I2C_ADDR, SAA7116_I2C_WRITE, data, data), \
	mtr->pcf_i2c[1] = data
#define PCF8574_CTRL_REG(mtr) mtr->pcf_i2c[0]
#define PCF8574_DATA_REG(mtr) mtr->pcf_i2c[1]


/*
 * Defines for the BT254.
 */
#define	NUM_BT254_REGS	7

#define BT254_COMMAND	0
#define	BT254_IOUT1	1
#define	BT254_IOUT2	2
#define	BT254_IOUT3	3
#define BT254_IOUT4	4
#define	BT254_IOUT5	5
#define	BT254_IOUT6	6

/*
 * Meteor info structure, one per meteor card installed.
 */
typedef struct meteor_softc {
    struct saa7116_regs *base;	/* saa7116 register virtual address */
    vm_offset_t phys_base;	/* saa7116 register physical address */
    pcici_t	tag;		/* PCI tag, for doing PCI commands */
    vm_offset_t bigbuf;		/* buffer that holds the captured image */
    int		alloc_pages;	/* number of pages in bigbuf */
    struct proc	*proc;		/* process to receive raised signal */
    int		signal;		/* signal to send to process */
#define	METEOR_SIG_MODE_MASK	0xffff0000
#define	METEOR_SIG_FIELD_MODE	0x00010000
#define	METEOR_SIG_FRAME_MODE	0x00000000
    struct meteor_mem *mem;	/* used to control sync. multi-frame output */
    u_long	synch_wait;	/* wait for free buffer before continuing */
    short	current;	/* frame number in buffer (1-frames) */
    short	rows;		/* number of rows in a frame */
    short	cols;		/* number of columns in a frame */
    short	depth;		/* number of byte per pixel */
    short	frames;		/* number of frames allocated */
    int		frame_size;	/* number of bytes in a frame */
    u_long	fifo_errors;	/* number of fifo capture errors since open */
    u_long	dma_errors;	/* number of DMA capture errors since open */
    u_long	frames_captured;/* number of frames captured since open */
    u_long	even_fields_captured; /* number of even fields captured */
    u_long	odd_fields_captured; /* number of odd fields captured */
    u_long	range_enable;	/* enable range checking ?? */
    unsigned	flags;
#define	METEOR_INITALIZED	0x00000001
#define	METEOR_OPEN		0x00000002 
#define	METEOR_MMAP		0x00000004
#define	METEOR_INTR		0x00000008
#define	METEOR_READ		0x00000010	/* XXX never gets referenced */
#define	METEOR_SINGLE		0x00000020	/* get single frame */
#define	METEOR_CONTIN		0x00000040	/* continuously get frames */
#define	METEOR_SYNCAP		0x00000080	/* synchronously get frames */
#define	METEOR_CAP_MASK		0x000000f0
#define	METEOR_NTSC		0x00000100
#define	METEOR_PAL		0x00000200
#define	METEOR_SECAM		0x00000400
#define	METEOR_AUTOMODE		0x00000800
#define	METEOR_FORM_MASK	0x00000f00
#define	METEOR_DEV0		0x00001000
#define	METEOR_DEV1		0x00002000
#define	METEOR_DEV2		0x00004000
#define	METEOR_DEV3		0x00008000
#define METEOR_DEV_SVIDEO	0x00006000
#define METEOR_DEV_RGB		0x0000a000
#define	METEOR_DEV_MASK		0x0000f000
#define	METEOR_RGB16		0x00010000
#define	METEOR_RGB24		0x00020000
#define	METEOR_YUV_PACKED	0x00040000
#define	METEOR_YUV_PLANAR	0x00080000
#define	METEOR_WANT_EVEN	0x00100000	/* want even frame */
#define	METEOR_WANT_ODD		0x00200000	/* want odd frame */
#define	METEOR_WANT_MASK	0x00300000
#define METEOR_ONLY_EVEN_FIELDS	0x01000000
#define METEOR_ONLY_ODD_FIELDS	0x02000000
#define METEOR_ONLY_FIELDS_MASK 0x03000000
#define METEOR_YUV_422		0x04000000
#define	METEOR_OUTPUT_FMT_MASK	0x040f0000
#define	METEOR_WANT_TS		0x08000000	/* time-stamp a frame */
#define METEOR_RGB		0x20000000	/* meteor rgb unit */
#define METEOR_FIELD_MODE	0x80000000
    u_char	saa7196_i2c[NUM_SAA7196_I2C_REGS]; /* saa7196 register values */
    u_char	pcf_i2c[NUM_PCF8574_I2C_REGS];	/* PCF8574 register values */
    u_char	bt254_reg[NUM_BT254_REGS];	/* BT254 register values */
    u_short	fps;		/* frames per second */
#ifdef METEOR_TEST_VIDEO
    struct meteor_video video;
#endif
} meteor_reg_t;
