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
 */
#ifndef PCI_LATENCY_TIMER
#define	PCI_LATENCY_TIMER		0x0c	/* pci timer register */
#endif

/*
 * Definitions for the Philips SAA7116 digital video to pci interface.
 */
#define	BROOKTREE_848_ID			0x0350109E

typedef volatile u_int 	breg_t;


#define BKTR_DSTATUS                 0x000
#define BKTR_IFORM                   0x004
#define BKTR_TDEC                    0x008
#define BKTR_EVEN_CROP               0x00C
#define BKTR_ODD_CROP                0x08C
#define BKTR_E_VDELAY_LO             0x010
#define BKTR_O_VDELAY_LO             0x090
#define BKTR_E_VACTIVE_LO            0x014
#define BKTR_O_VACTIVE_LO            0x094
#define BKTR_E_DELAY_LO              0x018
#define BKTR_O_DELAY_LO              0x098
#define BKTR_E_HACTIVE_LO            0x01C
#define BKTR_O_HACTIVE_LO            0x09C
#define BKTR_E_HSCALE_HI             0x020
#define BKTR_O_HSCALE_HI             0x0A0
#define BKTR_E_HSCALE_LO             0x024
#define BKTR_O_HSCALE_LO             0x0A4
#define BKTR_BRIGHT                  0x028
#define BKTR_E_CONTROL               0x02C
#define BKTR_O_CONTROL               0x0AC
#define BKTR_CONTRAST_LO             0x030
#define BKTR_SAT_U_LO                0x034
#define BKTR_SAT_V_LO                0x038
#define BKTR_HUE                     0x03C
#define BKTR_E_SCLOOP                0x040
#define BKTR_O_SCLOOP                0x0C0
#define BKTR_OFORM                   0x048
#define BKTR_E_VSCALE_HI             0x04C
#define BKTR_O_VSCALE_HI             0x0CC
#define BKTR_E_VSCALE_LO             0x050
#define BKTR_O_VSCALE_LO             0x0D0
#define BKTR_TEST                    0x054
#define BKTR_ADELAY                  0x060
#define BKTR_BDELAY                  0x064
#define BKTR_ADC                     0x068
#define BKTR_E_VTC                   0x06C
#define BKTR_O_VTC                   0x0EC
#define BKTR_SRESET                  0x07C
#define BKTR_COLOR_FMT               0x0D4
#define BKTR_COLOR_CTL               0x0D8
#define BKTR_CAP_CTL                 0x0DC
#define BKTR_VBI_PACK_SIZE           0x0E0
#define BKTR_VBI_PACK_DEL            0x0E4
#define BKTR_INT_STAT                0x100
#define BKTR_INT_MASK                0x104
#define BKTR_RISC_COUNT              0x120
#define BKTR_RISC_STRT_ADD           0x114
#define BKTR_GPIO_DMA_CTL            0x10C
#define BKTR_GPIO_OUT_EN             0x118
#define BKTR_GPIO_REG_INP            0x11C
#define BKTR_GPIO_DATA               0x200
#define BKTR_I2C_CONTROL             0x110


/*
 * device support for onboard tv tuners
 */
struct tvtuner {
	int	frequency;
	u_char	tunertype;
	u_char	channel;
	u_char	band;
};


/*
 * BrookTree 848  info structure, one per bt848 card installed.
 */
typedef struct bktr_softc {
    char *      base;	/* saa7116 register physical address */
    vm_offset_t phys_base;	/* saa7116 register physical address */
    pcici_t	tag;		/* PCI tag, for doing PCI commands */
    vm_offset_t bigbuf;		/* buffer that holds the captured image */
    int		alloc_pages;	/* number of pages in bigbuf */
    struct proc	*proc;		/* process to receive raised signal */
    int		signal;		/* signal to send to process */
#define	METEOR_SIG_MODE_MASK	0xffff0000
#define	METEOR_SIG_FIELD_MODE	0x00010000
#define	METEOR_SIG_FRAME_MODE	0x00000000
    vm_offset_t  dma_prog;
    vm_offset_t  odd_dma_prog;
    char         dma_prog_loaded;
    struct meteor_mem *mem;	/* used to control sync. multi-frame output */
    u_long	synch_wait;	/* wait for free buffer before continuing */
    short	current;	/* frame number in buffer (1-frames) */
    short	rows;		/* number of rows in a frame */
    short	cols;		/* number of columns in a frame */
    short	depth;		/* number of byte per pixel */
    u_long	format;		/* frame format rgb, yuv, etc.. */
    short	frames;		/* number of frames allocated */
    int		frame_size;	/* number of bytes in a frame */
    u_long	fifo_errors;	/* number of fifo capture errors since open */
    u_long	dma_errors;	/* number of DMA capture errors since open */
    u_long	frames_captured;/* number of frames captured since open */
    u_long	even_fields_captured; /* number of even fields captured */
    u_long	odd_fields_captured; /* number of odd fields captured */
    u_long	range_enable;	/* enable range checking ?? */
    u_short     capcontrol;     /* reg 0xdc capture control */
    u_short     bktr_cap_ctl;
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
    u_short	fps;		/* frames per second */
#ifdef DEVFS
    void	*devfs_token;
#endif
    struct meteor_video video;
    struct tvtuner	tuner;
} bktr_reg_t;


