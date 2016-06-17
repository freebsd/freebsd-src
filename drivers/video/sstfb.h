/*
 * linux/drivers/video/sstfb.h -- voodoo graphics frame buffer
 *
 *     Copyright (c) 2000,2001 Ghozlane Toumi <gtoumi@messel.emse.fr>
 *
 *     Created 28 Aug 2001 by Ghozlane Toumi
 *
 * $Id: sstfb.h,v 1.1.4.1 2001/08/29 01:30:38 ghoz Exp $
 */


#ifndef _SSTFB_H_
#define _SSTFB_H_

/*
 *
 *  Debug Stuff
 *
 */

#ifdef SST_DEBUG
#  define dprintk(X...)	printk(KERN_DEBUG "sstfb: " X)
#else
#  define dprintk(X...)
#  undef SST_DEBUG_REG
#  undef SST_DEBUG_FUNC
#  undef SST_DEBUG_VAR
#  undef SST_DEBUG_IOCTL
#endif

#if (SST_DEBUG_REG > 0)
#  define r_dprintk(X...)	dprintk(X)
#else
#  define r_dprintk(X...)
#endif
#if (SST_DEBUG_REG > 1)
#  define r_ddprintk(X...)	dprintk(" " X)
#else
#  define r_ddprintk(X...)
#endif

#if (SST_DEBUG_FUNC > 0)
#  define f_dprintk(X...)	dprintk(X)
#else
#  define f_dprintk(X...)
#endif
#if (SST_DEBUG_FUNC > 1)
#  define f_ddprintk(X...)	dprintk(" " X)
#else
#  define f_ddprintk(X...)
#endif
#if (SST_DEBUG_FUNC > 2)
#  define f_dddprintk(X...)	dprintk(" " X)
#else
#  define f_dddprintk(X...)
#endif

#if (SST_DEBUG_VAR > 0)
#  define v_dprintk(X...)	dprintk(X)
#  define print_var(V, X...)	\
   {				\
     dprintk(X);		\
     printk(" :\n");		\
     sst_dbg_print_var(V);	\
   }
#else
#  define v_dprintk(X...)
#  define print_var(X,Y...)
#endif

#define eprintk(X...)	printk(KERN_ERR "sstfb: " X)
#define iprintk(X...)	printk(KERN_INFO "sstfb: " X)
#define wprintk(X...)	printk(KERN_WARNING "sstfb: " X)

#define BIT(x)		(1ul<<(x))
#define PS2KHZ(a)	(1000000000UL/(a))	/* picoseconds to KHz */
#define KHZ2PS(a)	(1000000000UL/(a))
#define POW2(x)		(1ul<<(x))

#ifndef ABS
# define ABS(x)		(((x)<0)?-(x):(x))
#endif

//void Dump_regs(void);

/*
 *
 *  Const
 *
 */

/* pci stuff */
#define PCI_INIT_ENABLE		0x40
#  define PCI_EN_INIT_WR	  BIT(0)
#  define PCI_EN_FIFO_WR	  BIT(1)
#  define PCI_REMAP_DAC		  BIT(2)
#define PCI_VCLK_ENABLE		0xc0	/* enable video */
#define PCI_VCLK_DISABLE	0xe0

/* register offsets from memBaseAddr */
#define STATUS			0x0000
#  define STATUS_FBI_BUSY	  BIT(7)
#define FBZMODE			0x0110
#  define EN_CLIPPING		  BIT(0)	/* enable clipping */
#  define EN_RGB_WRITE		  BIT(9)	/* enable writes to rgb area */
#  define EN_ALPHA_WRITE	  BIT(10)
#  define ENGINE_INVERT_Y	  BIT(17)	/* invert Y origin (pipe) */
#define LFBMODE			0x0114
#  define LFB_565		  0		/* bits 3:0 .16 bits RGB */
#  define LFB_888		  4		/* 24 bits RGB */
#  define LFB_8888		  5		/* 32 bits ARGB */
#  define WR_BUFF_FRONT		  0		/* write buf select (front) */
#  define WR_BUFF_BACK		  (1 << 4)	/* back */
#  define RD_BUFF_FRONT		  0		/* read buff select (front) */
#  define RD_BUFF_BACK		  (1 << 6)	/* back */
#  define EN_PXL_PIPELINE	  BIT(8)	/* pixel pipeline (clip..)*/
#  define LFB_WORD_SWIZZLE_WR	  BIT(11)	/* enable write-wordswap (big-endian) */
#  define LFB_BYTE_SWIZZLE_WR	  BIT(12)	/* enable write-byteswap (big-endian) */
#  define LFB_INVERT_Y		  BIT(13)	/* invert Y origin (LFB) */
#  define LFB_WORD_SWIZZLE_RD	  BIT(15)	/* enable read-wordswap (big-endian) */
#  define LFB_BYTE_SWIZZLE_RD	  BIT(16)	/* enable read-byteswap (big-endian) */
#define CLIP_LEFT_RIGHT		0x0118
#define CLIP_LOWY_HIGHY		0x011c
#define NOPCMD			0x0120
#define FASTFILLCMD		0x0124
#define SWAPBUFFCMD		0x0128
#define FBIINIT4		0x0200		/* misc controls */
#  define FAST_PCI_READS	  0		/* 1 waitstate */
#  define SLOW_PCI_READS	  BIT(0)	/* 2 ws */
#  define LFB_READ_AHEAD	  BIT(1)
#define BACKPORCH		0x0208
#define VIDEODIMENSIONS		0x020c
#define FBIINIT0		0x0210		/* misc+fifo  controls */
#  define EN_VGA_PASSTHROUGH	  BIT(0)
#  define FBI_RESET		  BIT(1)
#  define FIFO_RESET		  BIT(2)
#define FBIINIT1		0x0214		/* PCI + video controls */
#  define VIDEO_MASK		  0x8080010f	/* masks video related bits V1+V2*/
#  define FAST_PCI_WRITES	  0		/* 0 ws */
#  define SLOW_PCI_WRITES	  BIT(1)	/* 1 ws */
#  define EN_LFB_READ		  BIT(3)
#  define TILES_IN_X_SHIFT	  4
#  define VIDEO_RESET		  BIT(8)
#  define EN_BLANKING		  BIT(12)
#  define EN_DATA_OE		  BIT(13)
#  define EN_BLANK_OE		  BIT(14)
#  define EN_HVSYNC_OE		  BIT(15)
#  define EN_DCLK_OE		  BIT(16)
#  define SEL_INPUT_VCLK_2X	  0		/* bit 17 */
#  define SEL_INPUT_VCLK_SLAVE	  BIT(17)
#  define SEL_SOURCE_VCLK_SLAVE	  0		/* bits 21:20 */
#  define SEL_SOURCE_VCLK_2X_DIV2 (0x01 << 20)
#  define SEL_SOURCE_VCLK_2X_SEL  (0x02 << 20)
#  define EN_24BPP		  BIT(22)
#  define TILES_IN_X_MSB_SHIFT	  24		/* v2 */
#  define VCLK_2X_SEL_DEL_SHIFT	  27		/* vclk out delay 0,4,6,8ns */
#  define VCLK_DEL_SHIFT	  29		/* vclk in delay */
#define FBIINIT2		0x0218		/* Dram controls */
#  define EN_FAST_RAS_READ	  BIT(5)
#  define EN_DRAM_OE		  BIT(6)
#  define EN_FAST_RD_AHEAD_WR	  BIT(7)
#  define VIDEO_OFFSET_SHIFT	  11		/* unit: #rows tile 64x16/2 */
#  define SWAP_DACVSYNC		  0
#  define SWAP_DACDATA0		  (1 << 9)
#  define SWAP_FIFO_STALL	  (2 << 9)
#  define EN_RD_AHEAD_FIFO	  BIT(21)
#  define EN_DRAM_REFRESH	  BIT(22)
#  define DRAM_REFRESH_16	  (0x30 << 23)	/* dram 16 ms */
#define DAC_READ		FBIINIT2	/* in remap mode */
#define FBIINIT3		0x021c		/* fbi controls */
#  define DISABLE_TEXTURE	  BIT(6)
#  define Y_SWAP_ORIGIN_SHIFT	  22		/* Y swap substraction value */
#define HSYNC			0x0220
#define VSYNC			0x0224
#define DAC_DATA		0x022c
#  define DAC_READ_CMD		  BIT(11)	/* set read dacreg mode */
#define FBIINIT5		0x0244		/* v2 specific */
#  define FBIINIT5_MASK		  0xfa40ffff    /* mask video bits*/
#  define HDOUBLESCAN		  BIT(20)
#  define VDOUBLESCAN		  BIT(21)
#  define HSYNC_HIGH 		  BIT(23)
#  define VSYNC_HIGH 		  BIT(24)
#  define INTERLACE		  BIT(26)
#define FBIINIT6		0x0248		/* v2 specific */
#  define TILES_IN_X_LSB_SHIFT	  30		/* v2 */
#define FBIINIT7		0x024c		/* v2 specific */

/* Dac Registers */
#define DACREG_WMA		0x0	/* pixel write mode address */
#define DACREG_LUT		0x01	/* color value */
#define DACREG_RMR		0x02	/* pixel mask */
#define DACREG_RMA		0x03	/* pixel read mode address */
/*Dac registers in indexed mode (TI, ATT dacs) */
#define DACREG_ADDR_I		DACREG_WMA
#define DACREG_DATA_I		DACREG_RMR
#define DACREG_RMR_I		0x00
#define DACREG_CR0_I		0x01
#  define DACREG_CR0_EN_INDEXED	  BIT(0)	/* enable indexec mode */
#  define DACREG_CR0_8BIT	  BIT(1)	/* set dac to 8 bits/read */
#  define DACREG_CR0_PWDOWN	  BIT(3)	/* powerdown dac */
#  define DACREG_CR0_16BPP	  0x30		/* mode 3 */
#  define DACREG_CR0_24BPP	  0x50		/* mode 5 */
#define	DACREG_CR1_I		0x05
#define DACREG_CC_I		0x06
#  define DACREG_CC_CLKA	  BIT(7)	/* clk A controled by regs */
#  define DACREG_CC_CLKA_C	  (2<<4)	/* clk A uses reg C */
#  define DACREG_CC_CLKB	  BIT(3)	/* clk B controled by regs */
#  define DACREG_CC_CLKB_D	  3		/* clkB uses reg D */
#define DACREG_AC0_I		0x48		/* clock A reg C */
#define DACREG_AC1_I		0x49
#define DACREG_BD0_I		0x6c		/* clock B reg D */
#define DACREG_BD1_I		0x6d

/* identification constants */
#define DACREG_MIR_TI		0x97
#define DACREG_DIR_TI		0x09
#define DACREG_MIR_ATT		0x84
#define DACREG_DIR_ATT		0x09
/* ics dac specific registers*/
#define DACREG_ICS_PLLWMA	0x04	/* PLL write mode address */
#define DACREG_ICS_PLLDATA	0x05	/* PLL data /parameter */
#define DACREG_ICS_CMD		0x06	/* command */
#  define DACREG_ICS_CMD_16BPP	  0x50	/* ics color mode 6 (16bpp bypass)*/
#  define DACREG_ICS_CMD_24BPP	  0x70	/* ics color mode 7 (24bpp bypass)*/
#  define DACREG_ICS_CMD_PWDOWN BIT(0)	/* powerdown dac */
#define DACREG_ICS_PLLRMA	0x07	/* PLL read mode address */
/*
 * pll parameter register:
 * indexed : write addr to PLLWMA, write data in PLLDATA.
 * for reads use PLLRMA .
 * 8 freq registers (0-7) for video clock (CLK0)
 * 2 freq registers (a-b) for graphic clock (CLK1)
 */
#define DACREG_ICS_PLL_CLK0_1_INI 0x55	/* initial pll M value for freq f1  */
#define DACREG_ICS_PLL_CLK0_7_INI 0x71	/* f7 */
#define DACREG_ICS_PLL_CLK1_B_INI 0x79	/* fb */
#define DACREG_ICS_PLL_CTRL	0x0e
#  define DACREG_ICS_CLK0	  BIT(5)
#  define DACREG_ICS_CLK0_0	  0
#  define DACREG_ICS_CLK1_A	  0	/* bit4 */

/* sst default init registers */
#define FBIINIT0_DEFAULT EN_VGA_PASSTHROUGH

#define FBIINIT1_DEFAULT 	\
	(			\
	  FAST_PCI_WRITES	\
/*	  SLOW_PCI_WRITES*/	\
	| VIDEO_RESET		\
	| 10 << TILES_IN_X_SHIFT\
	| SEL_SOURCE_VCLK_2X_SEL\
	| EN_LFB_READ		\
	)

#define FBIINIT2_DEFAULT	\
	(			\
	 SWAP_DACVSYNC		\
	| EN_DRAM_OE		\
	| DRAM_REFRESH_16	\
	| EN_DRAM_REFRESH	\
	| EN_FAST_RAS_READ	\
	| EN_RD_AHEAD_FIFO	\
	| EN_FAST_RD_AHEAD_WR	\
	)

#define FBIINIT3_DEFAULT 	\
	( DISABLE_TEXTURE )

#define FBIINIT4_DEFAULT	\
	(			\
	  FAST_PCI_READS	\
/*	  SLOW_PCI_READS*/	\
	| LFB_READ_AHEAD	\
	)
/* Careful with this one : writing back the data just read will trash the DAC
   reading some fields give logic value on pins, but setting this field will
   set the source signal driving the pin. conclusion : just use the default
   as a base before writing back .
*/
#define FBIINIT6_DEFAULT	(0x0)

/*
 *
 * Misc Const
 *
 */

/* used to know witch clock to set */
enum {
	VID_CLOCK=0,
	GFX_CLOCK=1,
};

/* freq max */
#define DAC_FREF	14318	/* DAC reference freq (Khz) */
#define VCO_MAX		260000

/*
 *  driver structs
 */

struct pll_timing {
	unsigned int m;
	unsigned int n;
	unsigned int p;
};

struct sstfb_info;

struct dac_switch {
	char * name;
	int (*detect) (struct sstfb_info *sst_info);
	int (*set_pll) (struct sstfb_info *sst_info, const struct pll_timing *t, const int clock);
	void (*set_vidmod) (struct sstfb_info *sst_info, const int bpp);
};

struct sst_spec {
	char * name;
	int default_gfx_clock;	/* 50000 for voodoo1, 75000 for voodoo2 */
	int max_gfxclk; 	/* ! in Mhz ie 60 for voodoo 1 */
};

struct sstfb_par {
	unsigned int bpp;
	unsigned int xDim;	/* xres */
	unsigned int hSyncOn;	/* hsync_len */
	unsigned int hSyncOff;	/* left_margin + xres + right_margin */
	unsigned int hBackPorch;/* left_margin */
	unsigned int yDim;
	unsigned int vSyncOn;
	unsigned int vSyncOff;
	unsigned int vBackPorch;
	unsigned int freq;	/* freq in kHz */
	struct pll_timing pll;
	unsigned int tiles_in_X;/* num of tiles in X res */
	unsigned int vmode;     /* doublescan/interlaced */
	unsigned int sync;      /* H/V sync polarity */
	unsigned int valid;	/* par is correct (fool proof) */
};

struct sstfb_info {
	struct fb_info		info;
	struct sstfb_par	current_par;
	struct pci_dev	*	dev;

	struct {
		unsigned long	base;	/* physical */
		unsigned long	vbase;	/* virtual (CPU view) */
		unsigned long	len;
	} video;			/* fb memory info */
	struct {
		unsigned long	base;
		unsigned long	vbase;
	} mmio;				/* registers memory info */

	struct dac_switch 	dac_sw;	/* dac specific functions */

	int	type;
	u8	revision;

	/* status */
/*XXX	int	configured;
	int	indexed_mode;
	int	vgapass;
	int	clipping; */
	int	gfx_clock;

	int	currcon;
	struct display  	disp; /* current display */
	struct { u_int red, green, blue, transp; } palette[16];

	union {
#ifdef FBCON_HAS_CFB16
		u16 cfb16[16];
#endif
#ifdef EN_24_32_BPP
#if defined (FBCON_HAS_CFB24) || defined(FBCON_HAS_CFB32)
		u32 cfb32[16];
#endif
#endif
	} fbcon_cmap;

};


#endif /* _SSTFB_H_ */
