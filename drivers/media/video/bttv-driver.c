/*
    bttv - Bt848 frame grabber driver

    Copyright (C) 1996,97,98 Ralph  Metzler (rjkm@thp.uni-koeln.de)
                           & Marcus Metzler (mocm@thp.uni-koeln.de)
    (c) 1999-2003 Gerd Knorr <kraxel@goldbach.in-berlin.de>

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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/pci.h>
#include <linux/signal.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/wrapper.h>
#include <linux/interrupt.h>
#include <linux/kmod.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/pagemap.h>

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/byteorder.h>

#include "bttvp.h"

#define DEBUG(x)	/* Debug driver */
#define MIN(a,b) (((a)>(b))?(b):(a))
#define MAX(a,b) (((a)>(b))?(a):(b))

/* fwd decl */
static void bt848_set_risc_jmps(struct bttv *btv, int state);
static void make_vbitab(struct bttv *btv);
static void bt848_set_winsize(struct bttv *btv);

unsigned int bttv_num;			/* number of Bt848s in use */
struct bttv bttvs[BTTV_MAX];

/* configuration variables */
#ifdef __BIG_ENDIAN
static unsigned int bigendian=1;
#else
static unsigned int bigendian=0;
#endif
static unsigned int radio[BTTV_MAX];
static unsigned int fieldnr = 0;
static unsigned int gpint = 1;
static unsigned int irq_debug = 0;
static unsigned int gbuffers = 4;
static unsigned int gbufsize = BTTV_MAX_FBUF;

static unsigned int combfilter = 0;
static unsigned int lumafilter = 0;
static unsigned int automute = 1;
static unsigned int chroma_agc = 0;
static unsigned int adc_crush = 1;
static int video_nr = -1;
static int radio_nr = -1;
static int vbi_nr = -1;
unsigned int bttv_debug = 0;
unsigned int bttv_verbose = 1;
unsigned int bttv_gpio = 0;

/* insmod options */
MODULE_PARM(radio,"1-" __stringify(BTTV_MAX) "i");
MODULE_PARM_DESC(radio,"The TV card supports radio, default is 0 (no)");
MODULE_PARM(bigendian,"i");
MODULE_PARM_DESC(bigendian,"byte order of the framebuffer, default is native endian");
MODULE_PARM(fieldnr,"i");
MODULE_PARM_DESC(fieldnr,"count fields, default is 0 (no)");
MODULE_PARM(bttv_verbose,"i");
MODULE_PARM_DESC(bttv_verbose,"verbose startup messages, default is 1 (yes)");
MODULE_PARM(bttv_gpio,"i");
MODULE_PARM_DESC(bttv_gpio,"log gpio changes, default is 0 (no)");
MODULE_PARM(bttv_debug,"i");
MODULE_PARM_DESC(bttv_debug,"debug messages, default is 0 (no)");
MODULE_PARM(irq_debug,"i");
MODULE_PARM_DESC(irq_debug,"irq handler debug messages, default is 0 (no)");
MODULE_PARM(gbuffers,"i");
MODULE_PARM_DESC(gbuffers,"number of capture buffers, default is 2 (64 max)");
MODULE_PARM(gbufsize,"i");
MODULE_PARM_DESC(gbufsize,"size of the capture buffers, default is 0x208000");
MODULE_PARM(gpint,"i");

MODULE_PARM(combfilter,"i");
MODULE_PARM(lumafilter,"i");
MODULE_PARM(automute,"i");
MODULE_PARM_DESC(automute,"mute audio on bad/missing video signal, default is 1 (yes)");
MODULE_PARM(chroma_agc,"i");
MODULE_PARM_DESC(chroma_agc,"enables the AGC of chroma signal, default is 0 (no)");
MODULE_PARM(adc_crush,"i");
MODULE_PARM_DESC(adc_crush,"enables the luminance ADC crush, default is 1 (yes)");

MODULE_PARM(video_nr,"i");
MODULE_PARM(radio_nr,"i");
MODULE_PARM(vbi_nr,"i");

MODULE_DESCRIPTION("bttv - v4l driver module for bt848/878 based cards");
MODULE_AUTHOR("Ralph Metzler & Marcus Metzler & Gerd Knorr");
MODULE_LICENSE("GPL");

/* kernel args */
#ifndef MODULE
static int __init p_radio(char *str) { return bttv_parse(str,BTTV_MAX,radio); }
__setup("bttv.radio=", p_radio);
#endif

#define I2C_TIMING (0x7<<4)
#define I2C_DELAY   10

#define I2C_SET(CTRL,DATA) \
    { btwrite((CTRL<<1)|(DATA), BT848_I2C); udelay(I2C_DELAY); }
#define I2C_GET()   (btread(BT848_I2C)&1)

#define BURSTOFFSET 76
#define BTTV_ERRORS 5


/*******************************/
/* Memory management functions */
/*******************************/


static inline unsigned long kvirt_to_bus(unsigned long adr) 
{
        unsigned long kva;

	kva = (unsigned long)page_address(vmalloc_to_page((void *)adr));
	kva |= adr & (PAGE_SIZE-1); /* restore the offset */   
	return virt_to_bus((void *)kva);
}

/* Here we want the physical address of the memory.
 * This is used when initializing the contents of the
 * area and marking the pages as reserved.
 */
static inline unsigned long kvirt_to_pa(unsigned long adr) 
{
        unsigned long kva;

	kva = (unsigned long)page_address(vmalloc_to_page((void *)adr));
	kva |= adr & (PAGE_SIZE-1); /* restore the offset */
	return __pa(kva);
}

static void * rvmalloc(signed long size)
{
	struct page *page;
	void * mem;
	unsigned long adr;

	mem=vmalloc_32(size);
	if (NULL == mem)
		printk(KERN_INFO "bttv: vmalloc_32(%ld) failed\n",size);
	else {
		/* Clear the ram out, no junk to the user */
		memset(mem, 0, size);
	        adr=(unsigned long) mem;
		while (size > 0) {
			page = vmalloc_to_page((void *)adr);
			mem_map_reserve(page);
			adr+=PAGE_SIZE;
			size-=PAGE_SIZE;
		}
	}
	return mem;
}

static void rvfree(void * mem, signed long size)
{
	struct page *page;
        unsigned long adr;
        
	if (mem) {
	        adr=(unsigned long) mem;
		while (size > 0) {
			page = vmalloc_to_page((void *)adr);
			mem_map_unreserve(page);
			adr+=PAGE_SIZE;
			size-=PAGE_SIZE;
		}
		vfree(mem);
	}
}



/*
 *	Create the giant waste of buffer space we need for now
 *	until we get DMA to user space sorted out (probably 2.3.x)
 *
 *	We only create this as and when someone uses mmap
 */
 
static int fbuffer_alloc(struct bttv *btv)
{
	if(!btv->fbuffer)
		btv->fbuffer=(unsigned char *) rvmalloc(gbuffers*gbufsize);
	else
		printk(KERN_ERR "bttv%d: Double alloc of fbuffer!\n",
			btv->nr);
	if(!btv->fbuffer)
		return -ENOBUFS;
	return 0;
}

/* ----------------------------------------------------------------------- */

void bttv_gpio_tracking(struct bttv *btv, char *comment)
{
	unsigned int outbits, data;
	outbits = btread(BT848_GPIO_OUT_EN);
	data    = btread(BT848_GPIO_DATA);
	printk(KERN_DEBUG "bttv%d: gpio: en=%08x, out=%08x in=%08x [%s]\n",
	       btv->nr,outbits,data & outbits, data & ~outbits, comment);
}

static char *audio_modes[] = { "audio: tuner", "audio: radio", "audio: extern",
			       "audio: intern", "audio: off" };

static void audio(struct bttv *btv, int mode)
{
	if (bttv_tvcards[btv->type].gpiomask)
		btaor(bttv_tvcards[btv->type].gpiomask,
		      ~bttv_tvcards[btv->type].gpiomask,
		      BT848_GPIO_OUT_EN);

	switch (mode)
	{
	        case AUDIO_MUTE:
                        btv->audio|=AUDIO_MUTE;
			break;
 		case AUDIO_UNMUTE:
			btv->audio&=~AUDIO_MUTE;
			mode=btv->audio;
			break;
		case AUDIO_OFF:
			mode=AUDIO_OFF;
			break;
		case AUDIO_ON:
			mode=btv->audio;
			break;
		default:
			btv->audio&=AUDIO_MUTE;
			btv->audio|=mode;
			break;
	}
        /* if audio mute or not in H-lock, turn audio off */
	if ((btv->audio&AUDIO_MUTE))
	        mode=AUDIO_OFF;
        if ((mode == AUDIO_TUNER) && (btv->radio))
		mode = AUDIO_RADIO;
	if (bttv_tvcards[btv->type].gpiomask)
		btaor(bttv_tvcards[btv->type].audiomux[mode],
		      ~bttv_tvcards[btv->type].gpiomask,
		      BT848_GPIO_DATA);
	if (bttv_gpio)
		bttv_gpio_tracking(btv,audio_modes[mode]);
	if (!in_interrupt())
		bttv_call_i2c_clients(btv,AUDC_SET_INPUT,&(mode));
}


static inline void bt848_dma(struct bttv *btv, uint state)
{
	if (state)
		btor(3, BT848_GPIO_DMA_CTL);
	else
		btand(~3, BT848_GPIO_DMA_CTL);
}


/* If Bt848a or Bt849, use PLL for PAL/SECAM and crystal for NTSC*/

/* Frequency = (F_input / PLL_X) * PLL_I.PLL_F/PLL_C 
   PLL_X = Reference pre-divider (0=1, 1=2) 
   PLL_C = Post divider (0=6, 1=4)
   PLL_I = Integer input 
   PLL_F = Fractional input 
   
   F_input = 28.636363 MHz: 
   PAL (CLKx2 = 35.46895 MHz): PLL_X = 1, PLL_I = 0x0E, PLL_F = 0xDCF9, PLL_C = 0
*/

static void set_pll_freq(struct bttv *btv, unsigned int fin, unsigned int fout)
{
        unsigned char fl, fh, fi;
        
        /* prevent overflows */
        fin/=4;
        fout/=4;

        fout*=12;
        fi=fout/fin;

        fout=(fout%fin)*256;
        fh=fout/fin;

        fout=(fout%fin)*256;
        fl=fout/fin;

        /*printk("0x%02x 0x%02x 0x%02x\n", fi, fh, fl);*/
        btwrite(fl, BT848_PLL_F_LO);
        btwrite(fh, BT848_PLL_F_HI);
        btwrite(fi|BT848_PLL_X, BT848_PLL_XCI);
}

static void set_pll(struct bttv *btv)
{
        int i;

        if (!btv->pll.pll_crystal)
                return;

	if (btv->pll.pll_ofreq == btv->pll.pll_current) {
		dprintk("bttv%d: PLL: no change required\n",btv->nr);
                return;
        }

        if (btv->pll.pll_ifreq == btv->pll.pll_ofreq) {
                /* no PLL needed */
                if (btv->pll.pll_current == 0)
                        return;
		vprintk(KERN_INFO "bttv%d: PLL can sleep, using XTAL (%d).\n",
			btv->nr,btv->pll.pll_ifreq);
                btwrite(0x00,BT848_TGCTRL);
                btwrite(0x00,BT848_PLL_XCI);
                btv->pll.pll_current = 0;
                return;
        }

	vprintk(KERN_INFO "bttv%d: PLL: %d => %d ",btv->nr,
		btv->pll.pll_ifreq, btv->pll.pll_ofreq);
	set_pll_freq(btv, btv->pll.pll_ifreq, btv->pll.pll_ofreq);

        for (i=0; i<10; i++) {
		/*  Let other people run while the PLL stabilizes */
		vprintk(".");
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(HZ/10);
		
                if (btread(BT848_DSTATUS) & BT848_DSTATUS_PLOCK) {
			btwrite(0,BT848_DSTATUS);
                } else {
                        btwrite(0x08,BT848_TGCTRL);
                        btv->pll.pll_current = btv->pll.pll_ofreq;
			vprintk(" ok\n");
                        return;
                }
        }
        btv->pll.pll_current = -1;
	vprintk("failed\n");
        return;
}

static void bt848_muxsel(struct bttv *btv, unsigned int input)
{
	dprintk("bttv%d: bt848_muxsel %d\n",btv->nr,input);

	if (bttv_tvcards[btv->type].muxsel[input] < 0) {
		dprintk("bttv%d: digital ccir muxsel\n", btv->nr);
		btv->channel = input;
		return;
	}

        /* needed by RemoteVideo MX */
	if (bttv_tvcards[btv->type].gpiomask2)
		btaor(bttv_tvcards[btv->type].gpiomask2,
		      ~bttv_tvcards[btv->type].gpiomask2,
		      BT848_GPIO_OUT_EN);

#if 0
	/* This seems to get rid of some synchronization problems */
	btand(~(3<<5), BT848_IFORM);
	mdelay(10);
#endif

	input %= bttv_tvcards[btv->type].video_inputs;
	if (input == btv->svhs) {
		btor(BT848_CONTROL_COMP, BT848_E_CONTROL);
		btor(BT848_CONTROL_COMP, BT848_O_CONTROL);
	} else {
		btand(~BT848_CONTROL_COMP, BT848_E_CONTROL);
		btand(~BT848_CONTROL_COMP, BT848_O_CONTROL);
	}

	btaor((bttv_tvcards[btv->type].muxsel[input]&3)<<5, ~(3<<5), BT848_IFORM);
	audio(btv, (input!=bttv_tvcards[btv->type].tuner) ?
              AUDIO_EXTERN : AUDIO_TUNER);

	if (bttv_tvcards[btv->type].gpiomask2)
		btaor(bttv_tvcards[btv->type].muxsel[input]>>4,
		      ~bttv_tvcards[btv->type].gpiomask2,
		      BT848_GPIO_DATA);

	/* card specific hook */
	if (bttv_tvcards[btv->type].muxsel_hook)
		bttv_tvcards[btv->type].muxsel_hook(btv, input);

	if (bttv_gpio)
		bttv_gpio_tracking(btv,"muxsel");

	btv->channel=input;
}

/* special timing tables from conexant... */
static u8 SRAM_Table[][60] =
{
	/* PAL digital input over GPIO[7:0] */
	{
		45, // 45 bytes following
		0x36,0x11,0x01,0x00,0x90,0x02,0x05,0x10,0x04,0x16,
		0x12,0x05,0x11,0x00,0x04,0x12,0xC0,0x00,0x31,0x00,
		0x06,0x51,0x08,0x03,0x89,0x08,0x07,0xC0,0x44,0x00,
		0x81,0x01,0x01,0xA9,0x0D,0x02,0x02,0x50,0x03,0x37,
		0x37,0x00,0xAF,0x21,0x00
	},
	/* NTSC digital input over GPIO[7:0] */
	{
		51, // 51 bytes following
		0x0C,0xC0,0x00,0x00,0x90,0x02,0x03,0x10,0x03,0x06,
		0x10,0x04,0x12,0x12,0x05,0x02,0x13,0x04,0x19,0x00,
		0x04,0x39,0x00,0x06,0x59,0x08,0x03,0x83,0x08,0x07,
		0x03,0x50,0x00,0xC0,0x40,0x00,0x86,0x01,0x01,0xA6,
		0x0D,0x02,0x03,0x11,0x01,0x05,0x37,0x00,0xAC,0x21,
		0x00,
	},
	// TGB_NTSC392 // quartzsight
	// This table has been modified to be used for Fusion Rev D
	{
		0x2A, // size of table = 42
		0x06, 0x08, 0x04, 0x0a, 0xc0, 0x00, 0x18, 0x08, 0x03, 0x24,
		0x08, 0x07, 0x02, 0x90, 0x02, 0x08, 0x10, 0x04, 0x0c, 0x10,
		0x05, 0x2c, 0x11, 0x04, 0x55, 0x48, 0x00, 0x05, 0x50, 0x00,
		0xbf, 0x0c, 0x02, 0x2f, 0x3d, 0x00, 0x2f, 0x3f, 0x00, 0xc3,
		0x20, 0x00
	}
};

struct tvnorm
{
        u32 Fsc;
        u16 swidth, sheight; /* scaled standard width, height */
	u16 totalwidth;
	u8 adelay, bdelay, iform;
	u32 scaledtwidth;
	u16 hdelayx1, hactivex1;
	u16 vdelay;
        u8 vbipack;
	int sram; /* index into SRAM_Table */
};

static struct tvnorm tvnorms[] = {
	/* PAL-BDGHI */
        /* max. active video is actually 922, but 924 is divisible by 4 and 3! */
 	/* actually, max active PAL with HSCALE=0 is 948, NTSC is 768 - nil */
	{
		.Fsc            = 35468950,
		.swidth         = 924,
		.sheight        = 576,
		.totalwidth     = 1135,
		.adelay         = 0x7f,
		.bdelay         = 0x72, 
		.iform          = (BT848_IFORM_PAL_BDGHI|BT848_IFORM_XT1),
		.scaledtwidth   = 1135,
		.hdelayx1       = 186,
		.hactivex1      = 924,
#ifdef VIDEODAT_HACK
		.vdelay         = VBI_MAXLINES*2,
#else
		.vdelay         = 0x20,
#endif
		.vbipack        = 255,
		.sram           = 0,
	},{
                /* NTSC */
		.Fsc            = 28636363,
		.swidth         = 768,
		.sheight        = 480,
		.totalwidth     = 910,
		.adelay         = 0x68,
		.bdelay         = 0x5d,
		.iform          = (BT848_IFORM_NTSC|BT848_IFORM_XT0),
		.scaledtwidth   = 910,
		.hdelayx1       = 128,
		.hactivex1      = 910,
		.vdelay         = 0x1a,
		.vbipack        = 144,
		.sram           = 1,
	},{
		/* SECAM L */
		.Fsc            = 35468950,
		.swidth         = 924,
		.sheight        = 576,
		.totalwidth     = 1135,
		.adelay         = 0x7f,
		.bdelay         = 0xb0,
		.iform          = (BT848_IFORM_SECAM|BT848_IFORM_XT1),
		.scaledtwidth   = 1135,
		.hdelayx1       = 186,
		.hactivex1      = 922,
		.vdelay         = 0x20,
		.vbipack        = 255,
		.sram           = 0, /* like PAL, correct? */
	},{
		/* PAL-NC */
		.Fsc            = 28636363,
		.swidth         = 640,
		.sheight        = 576,
		.totalwidth     = 910,
		.adelay         = 0x68,
		.bdelay         = 0x5d,
		.iform          = (BT848_IFORM_PAL_NC|BT848_IFORM_XT0),
		.scaledtwidth   = 780,
		.hdelayx1       = 130,
		.hactivex1      = 734,
		.vdelay         = 0x1a,
		.vbipack        = 144,
		.sram           = -1,
	},{
		/* PAL-M */
		.Fsc            = 28636363,
		.swidth         = 640,
		.sheight        = 480,
		.totalwidth     = 910,
		.adelay         = 0x68,
		.bdelay         = 0x5d,
		.iform          = (BT848_IFORM_PAL_M|BT848_IFORM_XT0),
		.scaledtwidth   = 780,
		.hdelayx1       = 135,
		.hactivex1      = 754,
		.vdelay         = 0x1a,
		.vbipack        = 144,
		.sram           = -1,
	},{
		/* PAL-N */
		.Fsc             35468950,
		.swidth         = 768,
		.sheight        = 576,
		.totalwidth     = 1135,
		.adelay         = 0x7f,
		.bdelay         = 0x72,
		.iform          = (BT848_IFORM_PAL_N|BT848_IFORM_XT1),
		.scaledtwidth   = 944,
		.hdelayx1       = 186,
		.hactivex1      = 922,
		.vdelay         = 0x20,
		.vbipack        = 144,
		.sram           = -1,
	},{
		/* NTSC-Japan */
		.Fsc            = 28636363,
		.swidth         = 640,
		.sheight        = 480,
		.totalwidth     = 910,
		.adelay         = 0x68,
		.bdelay         = 0x5d,
		.iform          = (BT848_IFORM_NTSC_J|BT848_IFORM_XT0),
		.scaledtwidth   = 780,
		.hdelayx1       = 135,
		.hactivex1      = 754,
		.vdelay         = 0x16,
		.vbipack        = 144,
		.sram           = -1,
	},{
		/* Quartzsight digital camera 
		 * From Bt832 datasheet: 393x304 pixel @30Hz,
		 * Visible: 352x288 pixel
		 */
                .Fsc            = 27000000,
                .swidth         = 352,
                .sheight        = 576, //2*288 ?
                .totalwidth     = 392,
                .adelay         = 0x68,
                .bdelay         = 0x5d,
                .iform          = (BT848_IFORM_PAL_BDGHI|BT848_IFORM_XT1),
                .scaledtwidth   = 392,
                .hdelayx1       = 0x20,
                .hactivex1      = 352,
                .vdelay         = 0x08,
                .vbipack        = 0, //255
                .sram           = 2,
        }
};
#define TVNORMS ARRAY_SIZE(tvnorms)

/* used to switch between the bt848's analog/digital video capture modes */
void bt848A_set_timing(struct bttv *btv)
{
	int i, len;
	int table_idx = tvnorms[btv->win.norm].sram;
	int fsc       = tvnorms[btv->win.norm].Fsc;

	if (bttv_tvcards[btv->type].muxsel[btv->channel] < 0) {
		dprintk("bttv%d: load digital timing table (table_idx=%d)\n",
			btv->nr,table_idx);

		/* timing change...reset timing generator address */
       		btwrite(0x00, BT848_TGCTRL);
       		btwrite(0x02, BT848_TGCTRL);
       		btwrite(0x00, BT848_TGCTRL);

		len=SRAM_Table[table_idx][0];
		for(i = 1; i <= len; i++)
			btwrite(SRAM_Table[table_idx][i],BT848_TGLB);
		btv->pll.pll_ofreq = 27000000;

		set_pll(btv);
		btwrite(0x11, BT848_TGCTRL);
		btwrite(0x41, BT848_DVSIF);
	} else {
		btv->pll.pll_ofreq = fsc;
		set_pll(btv);
		btwrite(0x0, BT848_DVSIF);
	}
}

static void bttv_set_norm(struct bttv *btv, int new_norm)
{
	unsigned long irq_flags;

	if (bttv_tvcards[btv->type].muxsel[btv->channel] < 0 &&
	    bttv_tvcards[btv->type].digital_mode == DIGITAL_MODE_CAMERA) {
		//override norm by quartzsight mode
		new_norm=7;
		dprintk("bttv%d: set_norm fix-up digital: %d\n",
			btv->nr, new_norm);
	}
	       
	if (btv->win.norm != new_norm) {
		btv->win.norm = new_norm;
		
		make_vbitab(btv);
		spin_lock_irqsave(&btv->s_lock, irq_flags);
		bt848_set_winsize(btv);
		spin_unlock_irqrestore(&btv->s_lock, irq_flags);
		
		bt848A_set_timing(btv);
		switch (btv->type) {
		case BTTV_VOODOOTV_FM:
			bttv_tda9880_setnorm(btv,new_norm);
			break;
#if 0
		case BTTV_OSPREY540:
			osprey_540_set_norm(btv,new_norm);
			break;
#endif
		}
	}
}

#define VBI_SPL 2044
/* RISC command to write one VBI data line */
#define VBI_RISC BT848_RISC_WRITE|VBI_SPL|BT848_RISC_EOL|BT848_RISC_SOL

static void make_vbitab(struct bttv *btv)
{
	int i;
	unsigned int *po=(unsigned int *) btv->vbi_odd;
	unsigned int *pe=(unsigned int *) btv->vbi_even;
  
	if (bttv_debug > 1)
		printk("bttv%d: vbi1: po=%08lx pe=%08lx\n",
		       btv->nr,virt_to_bus(po), virt_to_bus(pe));
        
	*(po++)=cpu_to_le32(BT848_RISC_SYNC|BT848_FIFO_STATUS_FM1); *(po++)=0;
	for (i=0; i<VBI_MAXLINES; i++) {
		*(po++)=cpu_to_le32(VBI_RISC);
		*(po++)=cpu_to_le32(kvirt_to_bus((unsigned long)btv->vbibuf+i*2048));
	}
	*(po++)=cpu_to_le32(BT848_RISC_JUMP);
	*(po++)=cpu_to_le32(virt_to_bus(btv->risc_jmp+4));

	*(pe++)=cpu_to_le32(BT848_RISC_SYNC|BT848_FIFO_STATUS_FM1); *(pe++)=0;
	for (i=VBI_MAXLINES; i<VBI_MAXLINES*2; i++) {
		*(pe++)=cpu_to_le32(VBI_RISC);
		*(pe++)=cpu_to_le32(kvirt_to_bus((unsigned long)btv->vbibuf+i*2048));
	}
	*(pe++)=cpu_to_le32(BT848_RISC_JUMP|BT848_RISC_IRQ|(0x01<<16));
	*(pe++)=cpu_to_le32(virt_to_bus(btv->risc_jmp+10));

	if (bttv_debug > 1)
		printk("bttv%d: vbi2: po=%08lx pe=%08lx\n",
		       btv->nr,virt_to_bus(po), virt_to_bus(pe));
}

static unsigned int fmtbppx2[16] = {
        8, 6, 4, 4, 4, 3, 2, 2, 4, 3, 0, 0, 0, 0, 2, 0 
};

static unsigned int palette2fmt[] = {
	UNSET,
	BT848_COLOR_FMT_Y8,
	BT848_COLOR_FMT_RGB8,
	BT848_COLOR_FMT_RGB16,
	BT848_COLOR_FMT_RGB24,
	BT848_COLOR_FMT_RGB32,
	BT848_COLOR_FMT_RGB15,
	BT848_COLOR_FMT_YUY2,
	BT848_COLOR_FMT_YUY2,
	UNSET,
	UNSET,
	UNSET,
	BT848_COLOR_FMT_RAW,
	BT848_COLOR_FMT_YCrCb422,
	BT848_COLOR_FMT_YCrCb411,
	BT848_COLOR_FMT_YCrCb422,
	BT848_COLOR_FMT_YCrCb411,
};
#define PALETTEFMT_MAX ARRAY_SIZE(palette2fmt)

static int make_rawrisctab(struct bttv *btv, u32 *ro, u32 *re, u32 *vbuf)
{
	u32 line;
	u32 bpl=1024;		/* bytes per line */
	unsigned long vadr=(unsigned long) vbuf;

	*(ro++)=cpu_to_le32(BT848_RISC_SYNC|BT848_FIFO_STATUS_FM1); 
        *(ro++)=cpu_to_le32(0);
	*(re++)=cpu_to_le32(BT848_RISC_SYNC|BT848_FIFO_STATUS_FM1);
        *(re++)=cpu_to_le32(0);
	
        /* In PAL 650 blocks of 256 DWORDs are sampled, but only if VDELAY
           is 2 and without separate VBI grabbing.
           We'll have to handle this inside the IRQ handler ... */

	for (line=0; line < 640; line++)
	{
                *(ro++)=cpu_to_le32(BT848_RISC_WRITE|bpl|BT848_RISC_SOL|BT848_RISC_EOL);
                *(ro++)=cpu_to_le32(kvirt_to_bus(vadr));
                *(re++)=cpu_to_le32(BT848_RISC_WRITE|bpl|BT848_RISC_SOL|BT848_RISC_EOL);
                *(re++)=cpu_to_le32(kvirt_to_bus(vadr+gbufsize/2));
                vadr+=bpl;
	}
	
	*(ro++)=cpu_to_le32(BT848_RISC_JUMP);
	*(ro++)=cpu_to_le32(btv->bus_vbi_even);
	*(re++)=cpu_to_le32(BT848_RISC_JUMP|BT848_RISC_IRQ|(2<<16));
	*(re++)=cpu_to_le32(btv->bus_vbi_odd);
	
	return 0;
}

static int  make_prisctab(struct bttv *btv, u32 *ro, u32 *re, u32 *vbuf,
			  u16 width, u16 height, u16 fmt)
{
	u16 line, lmask;
	u32 bl, blcr, blcb, rcmd;
	u32 todo;
	u32 **rp;
	int inter;
	unsigned long cbadr, cradr;
	unsigned long vadr=(unsigned long) vbuf;
	u32 shift, csize;

	if (bttv_debug > 1)
		printk("bttv%d: prisc1: ro=%08lx re=%08lx\n",
		       btv->nr,virt_to_bus(ro), virt_to_bus(re));

	switch(fmt)
	{
        case VIDEO_PALETTE_YUV422P:
                csize=(width*height)>>1;
                shift=1;
                lmask=0;
                break;
                
        case VIDEO_PALETTE_YUV411P:
                csize=(width*height)>>2;
                shift=2;
                lmask=0;
                break;
	 				
	 case VIDEO_PALETTE_YUV420P:
                csize=(width*height)>>2;
                shift=1;
                lmask=1;
                break;
                
	 case VIDEO_PALETTE_YUV410P:
                csize=(width*height)>>4;
                shift=2;
                lmask=3;
                break;
                
        default:
                return -1;
	}
	cbadr=vadr+(width*height);
	cradr=cbadr+csize;
	inter = (height>tvnorms[btv->win.norm].sheight/2) ? 1 : 0;
	
	*(ro++)=cpu_to_le32(BT848_RISC_SYNC|BT848_FIFO_STATUS_FM3);
        *(ro++)=0;
	*(re++)=cpu_to_le32(BT848_RISC_SYNC|BT848_FIFO_STATUS_FM3);
        *(re++)=0;
  
	for (line=0; line < (height<<(1^inter)); line++)
	{
		if(line==height)
		{
			vadr+=csize<<1;
			cbadr=vadr+(width*height);
			cradr=cbadr+csize;
		}
	        if (inter) 
		        rp= (line&1) ? &re : &ro;
		else 
	                rp= (line>=height) ? &ro : &re; 
	                

	        if(line&lmask)
	        	rcmd=BT848_RISC_WRITE1S23|BT848_RISC_SOL;
	        else
	        	rcmd=BT848_RISC_WRITE123|BT848_RISC_SOL;

	        todo=width;
		while(todo)
		{
                 bl=PAGE_SIZE-((PAGE_SIZE-1)&vadr);
                 blcr=(PAGE_SIZE-((PAGE_SIZE-1)&cradr))<<shift;
		 blcb=(PAGE_SIZE-((PAGE_SIZE-1)&cbadr))<<shift;
		 bl=(blcr<bl) ? blcr : bl;
		 bl=(blcb<bl) ? blcb : bl;
		 bl=(bl>todo) ? todo : bl;
		 blcr=bl>>shift;
		 blcb=blcr;
		 /* bl now containts the longest row that can be written */
		 todo-=bl;
		 if(!todo) rcmd|=BT848_RISC_EOL; /* if this is the last EOL */
		 
		 *((*rp)++)=cpu_to_le32(rcmd|bl);
		 *((*rp)++)=cpu_to_le32(blcb|(blcr<<16));
		 *((*rp)++)=cpu_to_le32(kvirt_to_bus(vadr));
		 vadr+=bl;
		 if ((rcmd&(15<<28))==BT848_RISC_WRITE123)
		 {
		 	*((*rp)++)=cpu_to_le32(kvirt_to_bus(cbadr));
		 	cbadr+=blcb;
		 	*((*rp)++)=cpu_to_le32(kvirt_to_bus(cradr));
		 	cradr+=blcr;
		 }
		 
		 rcmd&=~BT848_RISC_SOL; /* only the first has SOL */
		}
	}
	
	*(ro++)=cpu_to_le32(BT848_RISC_JUMP);
	*(ro++)=cpu_to_le32(btv->bus_vbi_even);
	*(re++)=cpu_to_le32(BT848_RISC_JUMP|BT848_RISC_IRQ|(2<<16));
	*(re++)=cpu_to_le32(btv->bus_vbi_odd);
	
	if (bttv_debug > 1)
		printk("bttv%d: prisc2: ro=%08lx re=%08lx\n",
		       btv->nr,virt_to_bus(ro), virt_to_bus(re));

	return 0;
}

static int  make_vrisctab(struct bttv *btv, u32 *ro, u32 *re, u32 *vbuf,
			  u16 width, u16 height, u16 palette)
{
	u16 line;
	u32 bpl;  /* bytes per line */
	u32 bl;
	u32 todo;
	u32 **rp;
	int inter;
	unsigned long vadr=(unsigned long)vbuf;

        if (palette==VIDEO_PALETTE_RAW) 
                return make_rawrisctab(btv, ro, re, vbuf);
        if (palette>=VIDEO_PALETTE_PLANAR)
                return make_prisctab(btv, ro, re, vbuf, width, height, palette);
	if (bttv_debug > 1)
		printk("bttv%d: vrisc1: ro=%08lx re=%08lx\n",
		       btv->nr,virt_to_bus(ro), virt_to_bus(re));
	
	inter = (height>tvnorms[btv->win.norm].sheight/2) ? 1 : 0;
	bpl=width*fmtbppx2[palette2fmt[palette]&0xf]/2;
	
	*(ro++)=cpu_to_le32(BT848_RISC_SYNC|BT848_FIFO_STATUS_FM1); 
        *(ro++)=cpu_to_le32(0);
	*(re++)=cpu_to_le32(BT848_RISC_SYNC|BT848_FIFO_STATUS_FM1);
        *(re++)=cpu_to_le32(0);
  
	for (line=0; line < (height<<(1^inter)); line++)
	{
	        if (inter) 
		        rp= (line&1) ? &re : &ro;
		else 
	                rp= (line>=height) ? &ro : &re; 

		bl=PAGE_SIZE-((PAGE_SIZE-1)&vadr);
		if (bpl<=bl)
                {
		        *((*rp)++)=cpu_to_le32(BT848_RISC_WRITE|BT848_RISC_SOL|
			        BT848_RISC_EOL|bpl); 
			*((*rp)++)=cpu_to_le32(kvirt_to_bus(vadr));
			vadr+=bpl;
		}
		else
		{
		        todo=bpl;
		        *((*rp)++)=cpu_to_le32(BT848_RISC_WRITE|BT848_RISC_SOL|bl);
			*((*rp)++)=cpu_to_le32(kvirt_to_bus(vadr));
			vadr+=bl;
			todo-=bl;
			while (todo>PAGE_SIZE)
			{
			        *((*rp)++)=cpu_to_le32(BT848_RISC_WRITE|PAGE_SIZE);
				*((*rp)++)=cpu_to_le32(kvirt_to_bus(vadr));
				vadr+=PAGE_SIZE;
				todo-=PAGE_SIZE;
			}
			*((*rp)++)=cpu_to_le32(BT848_RISC_WRITE|BT848_RISC_EOL|todo);
			*((*rp)++)=cpu_to_le32(kvirt_to_bus(vadr));
			vadr+=todo;
		}
	}
	
	*(ro++)=cpu_to_le32(BT848_RISC_JUMP);
	*(ro++)=cpu_to_le32(btv->bus_vbi_even);
	*(re++)=cpu_to_le32(BT848_RISC_JUMP|BT848_RISC_IRQ|(2<<16));
	*(re++)=cpu_to_le32(btv->bus_vbi_odd);

	if (bttv_debug > 1)
		printk("bttv%d: vrisc2: ro=%08lx re=%08lx\n",
		       btv->nr,virt_to_bus(ro), virt_to_bus(re));
	
	return 0;
}

static unsigned char lmaskt[8] = 
{ 0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80};
static unsigned char rmaskt[8] = 
{ 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};

static void clip_draw_rectangle(unsigned char *clipmap, int x, int y, int w, int h)
{
        unsigned char lmask, rmask, *p;
        int W, l, r;
	int i;

	if (bttv_debug > 1)
		printk("bttv clip: %dx%d+%d+%d\n",w,h,x,y);

	/* bitmap is fixed width, 128 bytes (1024 pixels represented) */
        if (x<0)
        {
                w+=x;
                x=0;
        }
        if (y<0)
        {
                h+=y;
                y=0;
        }
	if (w < 0 || h < 0)	/* catch bad clips */
		return;
	/* out of range data should just fall through */
        if (y+h>=625)
                h=625-y;
        if (x+w>=1024)
                w=1024-x;

        l=x>>3;
        r=(x+w-1)>>3;
        W=r-l-1;
        lmask=lmaskt[x&7];
        rmask=rmaskt[(x+w-1)&7];
        p=clipmap+128*y+l;
        
        if (W>0) 
        {
                for (i=0; i<h; i++, p+=128) 
                {
                        *p|=lmask;
                        memset(p+1, 0xff, W);
                        p[W+1]|=rmask;
                }
        } else if (!W) {
                for (i=0; i<h; i++, p+=128) 
                {
                        p[0]|=lmask;
                        p[1]|=rmask;
                }
        } else {
                for (i=0; i<h; i++, p+=128) 
                        p[0]|=lmask&rmask;
        }
               

}

static void make_clip_tab(struct bttv *btv, struct video_clip *cr, int ncr)
{
	u32 line, x, y, bpl, width, height, inter, maxw;
	u32 bpp, dx, sx, **rp, *ro, *re, flags, len;
	u32 adr;
	int i;
	unsigned char *clipmap, *clipline, cbit, lastbit, outofmem;

	/* take care: bpp != btv->win.bpp is allowed here */
	bpp = fmtbppx2[btv->win.color_fmt&0xf]/2;
	bpl=btv->win.bpl;
	adr=btv->win.vidadr + btv->win.x * btv->win.bpp + btv->win.y * bpl;
	inter=(btv->win.interlace&1)^1;
	width=btv->win.width;
	height=btv->win.height;
	if (bttv_debug > 1)
		printk("bttv%d: clip1: pal=%d size=%dx%d, bpl=%d bpp=%d\n",
		       btv->nr,btv->picture.palette,width,height,bpl,bpp);
	if(width > 1023)
		width = 1023;		/* sanity check */
	if(height > 625)
		height = 625;		/* sanity check */
	ro=btv->risc_scr_odd;
	re=btv->risc_scr_even;

	if (bttv_debug)
		printk("bttv%d: clip: ro=%08lx re=%08lx\n",
		       btv->nr,virt_to_bus(ro), virt_to_bus(re));

	if ((clipmap=vmalloc(VIDEO_CLIPMAP_SIZE))==NULL) {
		/* can't clip, don't generate any risc code */
		*(ro++)=cpu_to_le32(BT848_RISC_JUMP);
		*(ro++)=cpu_to_le32(btv->bus_vbi_even);
		*(re++)=cpu_to_le32(BT848_RISC_JUMP);
		*(re++)=cpu_to_le32(btv->bus_vbi_odd);
		return;
	}
	if (ncr < 0) {	/* bitmap was pased */
		memcpy(clipmap, (unsigned char *)cr, VIDEO_CLIPMAP_SIZE);
	} else {	/* convert rectangular clips to a bitmap */
		memset(clipmap, 0, VIDEO_CLIPMAP_SIZE); /* clear map */
		for (i=0; i<ncr; i++)
			clip_draw_rectangle(clipmap, cr[i].x, cr[i].y,
				cr[i].width, cr[i].height);
	}
	/* clip against viewing window AND screen 
	   so we do not have to rely on the user program
	 */
	maxw = (bpl - btv->win.x * btv->win.bpp) / bpp;
	clip_draw_rectangle(clipmap, (width > maxw) ? maxw : width,
			    0, 1024, 768);
	clip_draw_rectangle(clipmap,0,(btv->win.y+height>btv->win.sheight) ?
			    (btv->win.sheight-btv->win.y) : height,1024,768);
	if (btv->win.x<0)
		clip_draw_rectangle(clipmap, 0, 0, -(btv->win.x), 768);
	if (btv->win.y<0)
		clip_draw_rectangle(clipmap, 0, 0, 1024, -(btv->win.y));
	
	*(ro++)=cpu_to_le32(BT848_RISC_SYNC|BT848_FIFO_STATUS_FM1);
        *(ro++)=cpu_to_le32(0);
	*(re++)=cpu_to_le32(BT848_RISC_SYNC|BT848_FIFO_STATUS_FM1);
        *(re++)=cpu_to_le32(0);
	
	/* translate bitmap to risc code */
        for (line=outofmem=0; line < (height<<inter) && !outofmem; line++)
        {
		y = line>>inter;
		rp= (line&1) ? &re : &ro;
		clipline = clipmap + (y<<7); /* running pointers ... */
		lastbit = *clipline & 1;
		for(x=dx=0,sx=0; x<=width && !outofmem;) {
			if (0 == (x&7)) {
				/* check bytes not bits if we can ... */
				if (lastbit) {
					while (0xff==*clipline && x<width-8) {
						x  += 8;
						dx += 8;
						clipline++;
					}
				} else {
					while (0x00==*clipline && x<width-8) {
						x  += 8;
						dx += 8;
						clipline++;
					}
				}
			}
			cbit = *clipline & (1<<(x&7));
			if (x < width && !lastbit == !cbit) {
				dx++;
			} else {
				/* generate the dma controller code */
				len = dx * bpp;
				flags = ((bpp==4) ? BT848_RISC_BYTE3 : 0);
				flags |= ((!sx) ? BT848_RISC_SOL : 0);
				flags |= ((sx + dx == width) ? BT848_RISC_EOL : 0);
				if (!lastbit) {
					*((*rp)++)=cpu_to_le32(BT848_RISC_WRITE|flags|len);
					*((*rp)++)=cpu_to_le32(adr + bpp * sx);
				} else {
					*((*rp)++)=cpu_to_le32(BT848_RISC_SKIP|flags|len);
				}
				lastbit=cbit;
				sx += dx;
				dx = 1;
				if (ro - btv->risc_scr_odd>(RISCMEM_LEN>>3) - 16)
					outofmem++;
				if (re - btv->risc_scr_even>(RISCMEM_LEN>>3) - 16)
					outofmem++;
			}
			x++;
			if (0 == (x&7))
				clipline++;
		}
		if ((!inter)||(line&1))
                        adr+=bpl;
	}

	vfree(clipmap);
	/* outofmem flag relies on the following code to discard extra data */
	*(ro++)=cpu_to_le32(BT848_RISC_JUMP);
	*(ro++)=cpu_to_le32(btv->bus_vbi_even);
	*(re++)=cpu_to_le32(BT848_RISC_JUMP);
	*(re++)=cpu_to_le32(btv->bus_vbi_odd);

	if (bttv_debug > 1)
		printk("bttv%d: clip2: pal=%d size=%dx%d, bpl=%d bpp=%d\n",
		       btv->nr,btv->picture.palette,width,height,bpl,bpp);
}

/*
 *	Set the registers for the size we have specified. Don't bother
 *	trying to understand this without the BT848 manual in front of
 *	you [AC]. 
 *
 *	PS: The manual is free for download in .pdf format from 
 *	www.brooktree.com - nicely done those folks.
 */
 
static inline void bt848_set_eogeo(struct bttv *btv, struct tvnorm *tvn,
				   int odd, int width, int height)
{
        u16 vscale, hscale;
	u32 xsf, sr;
	u16 hdelay;
	u8 crop, vtc;

	int inter = (height>tvn->sheight/2) ? 0 : 1;
        int off = odd ? 0x80 : 0x00;

	int swidth       = tvn->swidth;
	int totalwidth   = tvn->totalwidth;
	int scaledtwidth = tvn->scaledtwidth;

	if (bttv_tvcards[btv->type].muxsel[btv->channel] < 0) {
		dprintk("bttv%d: DIGITAL_MODE_VIDEO override width\n",btv->nr);
		swidth       = 720;
		totalwidth   = 858;
		scaledtwidth = 858;
	}

	xsf = (width*scaledtwidth)/swidth;
	hscale = ((totalwidth*4096UL)/xsf-4096);
	hdelay =  tvn->hdelayx1;
	hdelay =  (hdelay*width)/swidth;
	hdelay &= 0x3fe;
	sr=((tvn->sheight>>inter)*512)/height-512;
	vscale=(0x10000UL-sr)&0x1fff;
	crop=((width>>8)&0x03)|((hdelay>>6)&0x0c)|
		((tvn->sheight>>4)&0x30)|((tvn->vdelay>>2)&0xc0);
	vscale |= inter ? (BT848_VSCALE_INT<<8) : 0;

	if (combfilter) {
		/* Some people say interpolation looks bad ... */
		vtc = (width < 193) ? 2 : ((width < 385) ? 1 : 0);
		if (width < 769)
			btor(BT848_VSCALE_COMB, BT848_E_VSCALE_HI+off);
		else
			btand(~BT848_VSCALE_COMB, BT848_E_VSCALE_HI+off);
	} else {
		vtc = 0;
		btand(~BT848_VSCALE_COMB, BT848_E_VSCALE_HI+off);
	}

	btwrite(vtc, BT848_E_VTC+off);
	btwrite(hscale>>8, BT848_E_HSCALE_HI+off);
	btwrite(hscale&0xff, BT848_E_HSCALE_LO+off);
	btaor((vscale>>8), 0xe0, BT848_E_VSCALE_HI+off);
	btwrite(vscale&0xff, BT848_E_VSCALE_LO+off);
	btwrite(width&0xff, BT848_E_HACTIVE_LO+off);
	btwrite(hdelay&0xff, BT848_E_HDELAY_LO+off);
	btwrite(tvn->sheight&0xff, BT848_E_VACTIVE_LO+off);
	btwrite(tvn->vdelay&0xff, BT848_E_VDELAY_LO+off);
	btwrite(crop, BT848_E_CROP+off);
}


static void bt848_set_geo(struct bttv *btv)
{
	u16 ewidth, eheight, owidth, oheight;
	u16 format, bswap;
	struct tvnorm *tvn;

	tvn=&tvnorms[btv->win.norm];
	
	btwrite(tvn->adelay, BT848_ADELAY);
	btwrite(tvn->bdelay, BT848_BDELAY);
	btaor(tvn->iform,~(BT848_IFORM_NORM|BT848_IFORM_XTBOTH), BT848_IFORM);
	btwrite(tvn->vbipack, BT848_VBI_PACK_SIZE);
	if (bttv_tvcards[btv->type].muxsel[btv->channel] < 0)
		btwrite(0x39, BT848_VBI_PACK_DEL);
	else
		btwrite(0x01, BT848_VBI_PACK_DEL);

        btv->pll.pll_ofreq = tvn->Fsc;
	if (!in_interrupt())
		set_pll(btv);

	btv->win.interlace = (btv->win.height>tvn->sheight/2) ? 1 : 0;

	if (0 == btv->risc_cap_odd &&
	    0 == btv->risc_cap_even) {
		/* overlay only */
		owidth  = btv->win.width;
		oheight = btv->win.height;
		ewidth  = btv->win.width;
		eheight = btv->win.height;
		format  = btv->win.color_fmt;
		bswap   = btv->fb_color_ctl;
	} else if (-1 != btv->gq_grab      &&
		   0  == btv->risc_cap_odd &&
		   !btv->win.interlace     &&
		   btv->scr_on) {
		/* odd field -> overlay, even field -> capture */
		owidth  = btv->win.width;
		oheight = btv->win.height;
		ewidth  = btv->gbuf[btv->gq_grab].width;
		eheight = btv->gbuf[btv->gq_grab].height;
		format  = (btv->win.color_fmt & 0xf0) |
			(btv->gbuf[btv->gq_grab].fmt & 0x0f);
		bswap   = btv->fb_color_ctl & 0x0a;
	} else {
		/* capture only */
		owidth  = btv->gbuf[btv->gq_grab].width;
		oheight = btv->gbuf[btv->gq_grab].height;
		ewidth  = btv->gbuf[btv->gq_grab].width;
		eheight = btv->gbuf[btv->gq_grab].height;
		format  = btv->gbuf[btv->gq_grab].fmt;
		bswap   = 0;
	}

	/* program odd + even fields */
	bt848_set_eogeo(btv, tvn, 1, owidth, oheight);
	bt848_set_eogeo(btv, tvn, 0, ewidth, eheight);

	btwrite(format, BT848_COLOR_FMT);
	btwrite(bswap | BT848_COLOR_CTL_GAMMA, BT848_COLOR_CTL);
}


static int bpp2fmt[4] = {
        BT848_COLOR_FMT_RGB8, BT848_COLOR_FMT_RGB16,
        BT848_COLOR_FMT_RGB24, BT848_COLOR_FMT_RGB32 
};

static void bt848_set_winsize(struct bttv *btv)
{
        unsigned short format;

	if (btv->picture.palette > 0 && btv->picture.palette <= VIDEO_PALETTE_YUV422) {
		/* format set by VIDIOCSPICT */
		format = palette2fmt[btv->picture.palette];
	} else {
		/* use default for the given color depth */
		format = (btv->win.depth==15) ? BT848_COLOR_FMT_RGB15 :
			bpp2fmt[(btv->win.bpp-1)&3];
	}
	btv->win.color_fmt = format;
	if (bigendian &&
	    format == BT848_COLOR_FMT_RGB32) {
		btv->fb_color_ctl =
			BT848_COLOR_CTL_WSWAP_ODD	|
			BT848_COLOR_CTL_WSWAP_EVEN	|
			BT848_COLOR_CTL_BSWAP_ODD	|
			BT848_COLOR_CTL_BSWAP_EVEN;
        } else if (bigendian &&
		   (format == BT848_COLOR_FMT_RGB16 ||
                    format == BT848_COLOR_FMT_RGB15)) {
		btv->fb_color_ctl =
			BT848_COLOR_CTL_BSWAP_ODD	|
			BT848_COLOR_CTL_BSWAP_EVEN;
        } else {
		btv->fb_color_ctl = 0;
	}

	/*	RGB8 seems to be a 9x5x5 GRB color cube starting at
	 *	color 16. Why the h... can't they even mention this in the
	 *	data sheet?  [AC - because it's a standard format so I guess
	 *	it never occurred to them]
	 *	Enable dithering in this mode.
	 */

	if (format==BT848_COLOR_FMT_RGB8)
                btand(~BT848_CAP_CTL_DITH_FRAME, BT848_CAP_CTL); 
	else
	        btor(BT848_CAP_CTL_DITH_FRAME, BT848_CAP_CTL);

        bt848_set_geo(btv);
}

/*
 *	Grab into virtual memory.
 */

static int vgrab(struct bttv *btv, struct video_mmap *mp)
{
	u32 *ro, *re;
	u32 *vbuf;
	unsigned long flags;
	
	if(btv->fbuffer==NULL)
	{
		if(fbuffer_alloc(btv))
			return -ENOBUFS;
	}

	if(mp->frame >= gbuffers || mp->frame < 0)
		return -EINVAL;
	if(btv->gbuf[mp->frame].stat != GBUFFER_UNUSED)
		return -EBUSY;
		
	if(mp->height < 32 || mp->width < 48)
		return -EINVAL;
	if (mp->format >= PALETTEFMT_MAX)
		return -EINVAL;

	if ((unsigned int)mp->height * mp->width *
	    fmtbppx2[palette2fmt[mp->format]&0x0f]/2 > gbufsize)
		return -EINVAL;
	if (UNSET == palette2fmt[mp->format])
		return -EINVAL;

	/*
	 *	Ok load up the BT848
	 */
	 
	vbuf=(unsigned int *)(btv->fbuffer+gbufsize*mp->frame);
	ro=btv->gbuf[mp->frame].risc;
	re=ro+2048;
        make_vrisctab(btv, ro, re, vbuf, mp->width, mp->height, mp->format);

	if (bttv_debug)
		printk("bttv%d: cap vgrab: queue %d (%d:%dx%d)\n",
		       btv->nr,mp->frame,mp->format,mp->width,mp->height);
       	spin_lock_irqsave(&btv->s_lock, flags); 
        btv->gbuf[mp->frame].stat    = GBUFFER_GRABBING;
	btv->gbuf[mp->frame].fmt     = palette2fmt[mp->format];
	btv->gbuf[mp->frame].width   = mp->width;
	btv->gbuf[mp->frame].height  = mp->height;
	btv->gbuf[mp->frame].ro      = virt_to_bus(ro);
	btv->gbuf[mp->frame].re      = virt_to_bus(re);

#if 1
	if (mp->height <= tvnorms[btv->win.norm].sheight/2 &&
	    mp->format != VIDEO_PALETTE_RAW)
		btv->gbuf[mp->frame].ro = 0;
#endif

	if (-1 == btv->gq_grab && btv->gq_in == btv->gq_out) {
		btv->gq_start = 1;
		btv->risc_jmp[12]=cpu_to_le32(BT848_RISC_JUMP|(0x8<<16)|BT848_RISC_IRQ);
        }
	btv->gqueue[btv->gq_in++] = mp->frame;
	btv->gq_in = btv->gq_in % MAX_GBUFFERS;

	btor(3, BT848_CAP_CTL);
	btor(3, BT848_GPIO_DMA_CTL);
	spin_unlock_irqrestore(&btv->s_lock, flags);
	return 0;
}

static long bttv_write(struct video_device *v, const char *buf, unsigned long count, int nonblock)
{
	return -EINVAL;
}

static long bttv_read(struct video_device *v, char *buf, unsigned long count, int nonblock)
{
	/* use bttv 0.9.x if you need capture via read() */
	return -EINVAL;
}

static inline void burst(int on)
{
	tvnorms[0].scaledtwidth = 1135 - (on?BURSTOFFSET-2:0);
	tvnorms[0].hdelayx1     = 186  - (on?BURSTOFFSET  :0);
	tvnorms[2].scaledtwidth = 1135 - (on?BURSTOFFSET-2:0);
	tvnorms[2].hdelayx1     = 186  - (on?BURSTOFFSET  :0);
}

/*
 * called from irq handler on fatal errors.  Takes the grabber chip
 * offline, flag it needs a reinitialization (which can't be done
 * from irq context) and wake up all sleeping proccesses.  They would
 * block forever else.  We also need someone who actually does the
 * reinitialization from process context...
 */
static void bt848_offline(struct bttv *btv)
{
	unsigned int i;
	spin_lock(&btv->s_lock);

	/* cancel all outstanding grab requests */
	btv->gq_in = 0;
	btv->gq_out = 0;
	btv->gq_grab = -1;
	for (i = 0; i < gbuffers; i++)
		if (btv->gbuf[i].stat == GBUFFER_GRABBING)
			btv->gbuf[i].stat = GBUFFER_ERROR;

	/* disable screen overlay and DMA */
	btv->risc_cap_odd  = 0;
	btv->risc_cap_even = 0;
	bt848_set_risc_jmps(btv,0);

	/* flag the chip needs a restart */
	btv->needs_restart = 1;
	spin_unlock(&btv->s_lock);

	wake_up_interruptible(&btv->vbiq);
	wake_up_interruptible(&btv->capq);
}

static void bt848_restart(struct bttv *btv)
{
 	unsigned long irq_flags;

	if (bttv_verbose)
		printk("bttv%d: resetting chip\n",btv->nr);
	btwrite(0xfffffUL, BT848_INT_STAT);
	btand(~15, BT848_GPIO_DMA_CTL);
	btwrite(0, BT848_SRESET);
	btwrite(virt_to_bus(btv->risc_jmp+2),
		BT848_RISC_STRT_ADD);

	/* enforce pll reprogramming */
	btv->pll.pll_current = -1;
	set_pll(btv);

	spin_lock_irqsave(&btv->s_lock, irq_flags);
	btv->errors = 0;
	btv->needs_restart = 0;
	bt848_set_geo(btv);
	bt848_set_risc_jmps(btv,-1);
	spin_unlock_irqrestore(&btv->s_lock, irq_flags);
}

/*
 *	Open a bttv card. Right now the flags stuff is just playing
 */

static int bttv_open(struct video_device *dev, int flags)
{
	struct bttv *btv = (struct bttv *)dev;
        unsigned int i;
	int ret;

	ret = -EBUSY;
	if (bttv_debug)
		printk("bttv%d: open called\n",btv->nr);

	down(&btv->lock);
	if (btv->user)
		goto out_unlock;
	
	btv->fbuffer=(unsigned char *) rvmalloc(gbuffers*gbufsize);
	ret = -ENOMEM;
	if (!btv->fbuffer)
		goto out_unlock;

        btv->gq_in = 0;
        btv->gq_out = 0;
	btv->gq_grab = -1;
        for (i = 0; i < gbuffers; i++)
                btv->gbuf[i].stat = GBUFFER_UNUSED;

	if (btv->needs_restart)
		bt848_restart(btv);
        burst(0);
	set_pll(btv);
        btv->user++;
	up(&btv->lock);
        return 0;

 out_unlock:
	up(&btv->lock);
	return ret;
}

static void bttv_close(struct video_device *dev)
{
	struct bttv *btv=(struct bttv *)dev;
 	unsigned long irq_flags;
	int need_wait;

	down(&btv->lock);
	btv->user--;
	spin_lock_irqsave(&btv->s_lock, irq_flags);
	need_wait = (-1 != btv->gq_grab);
	btv->gq_start = 0;
	btv->gq_in = 0;
	btv->gq_out = 0;
	btv->gq_grab = -1;
	btv->scr_on = 0;
	btv->risc_cap_odd = 0;
	btv->risc_cap_even = 0;
	bt848_set_risc_jmps(btv,-1);
	spin_unlock_irqrestore(&btv->s_lock, irq_flags);

	/*
	 *	A word of warning. At this point the chip
	 *	is still capturing because its FIFO hasn't emptied
	 *	and the DMA control operations are posted PCI 
	 *	operations.
	 */

	btread(BT848_I2C); 	/* This fixes the PCI posting delay */
	
	if (need_wait) {
		/*
		 *	This is sucky but right now I can't find a good way to
		 *	be sure its safe to free the buffer. We wait 5-6 fields
		 *	which is more than sufficient to be sure.
		 */
		current->state = TASK_UNINTERRUPTIBLE;
		schedule_timeout(HZ/10);	/* Wait 1/10th of a second */
	}
	
	/*
	 *	We have allowed it to drain.
	 */

	if(btv->fbuffer)
		rvfree((void *) btv->fbuffer, gbuffers*gbufsize);
	btv->fbuffer=0;
	up(&btv->lock);
}


/***********************************/
/* ioctls and supporting functions */
/***********************************/

static inline void bt848_bright(struct bttv *btv, uint bright)
{
	btwrite(bright&0xff, BT848_BRIGHT);
}

static inline void bt848_hue(struct bttv *btv, uint hue)
{
	btwrite(hue&0xff, BT848_HUE);
}

static inline void bt848_contrast(struct bttv *btv, uint cont)
{
	unsigned int conthi;

	conthi=(cont>>6)&4;
	btwrite(cont&0xff, BT848_CONTRAST_LO);
	btaor(conthi, ~4, BT848_E_CONTROL);
	btaor(conthi, ~4, BT848_O_CONTROL);
}

static inline void bt848_sat_u(struct bttv *btv, unsigned long data)
{
	u32 datahi;

	datahi=(data>>7)&2;
	btwrite(data&0xff, BT848_SAT_U_LO);
	btaor(datahi, ~2, BT848_E_CONTROL);
	btaor(datahi, ~2, BT848_O_CONTROL);
}

static inline void bt848_sat_v(struct bttv *btv, unsigned long data)
{
	u32 datahi;

	datahi=(data>>8)&1;
	btwrite(data&0xff, BT848_SAT_V_LO);
	btaor(datahi, ~1, BT848_E_CONTROL);
	btaor(datahi, ~1, BT848_O_CONTROL);
}

/*
 *	ioctl routine
 */

static int bttv_ioctl(struct video_device *dev, unsigned int cmd, void *arg)
{
	struct bttv *btv=(struct bttv *)dev;
 	unsigned long irq_flags;
 	int ret = 0;

	if (bttv_debug > 1)
		printk("bttv%d: ioctl 0x%x\n",btv->nr,cmd);

	switch (cmd) {
	case VIDIOCGCAP:
	{
		struct video_capability b;
		strcpy(b.name,btv->video_dev.name);
		b.type = VID_TYPE_CAPTURE|
			((bttv_tvcards[btv->type].tuner != UNSET) ? VID_TYPE_TUNER : 0) |
			VID_TYPE_OVERLAY|
			VID_TYPE_CLIPPING|
			VID_TYPE_FRAMERAM|
			VID_TYPE_SCALES;
		b.channels = bttv_tvcards[btv->type].video_inputs;
		b.audios = bttv_tvcards[btv->type].audio_inputs;
		b.maxwidth = tvnorms[btv->win.norm].swidth;
		b.maxheight = tvnorms[btv->win.norm].sheight;
		b.minwidth = 48;
		b.minheight = 32;
		if(copy_to_user(arg,&b,sizeof(b)))
			return -EFAULT;
		return 0;
	}
	case VIDIOCGCHAN:
	{
		struct video_channel v;
		unsigned int  channel;

		if(copy_from_user(&v, arg,sizeof(v)))
			return -EFAULT;
		channel = v.channel;
		if (channel>=bttv_tvcards[btv->type].video_inputs)
			return -EINVAL;
		v.flags=VIDEO_VC_AUDIO;
		v.tuners=0;
		v.type=VIDEO_TYPE_CAMERA;
		v.norm = btv->win.norm;
		if(channel==bttv_tvcards[btv->type].tuner) 
		{
			strcpy(v.name,"Television");
			v.flags|=VIDEO_VC_TUNER;
			v.type=VIDEO_TYPE_TV;
			v.tuners=1;
		} 
		else if (channel == btv->svhs) 
			strcpy(v.name,"S-Video");
		else if (bttv_tvcards[btv->type].muxsel[v.channel] < 0)
			strcpy(v.name,"Digital Video");
		else
			sprintf(v.name,"Composite%d",v.channel);
		
		if(copy_to_user(arg,&v,sizeof(v)))
			return -EFAULT;
		return 0;
	}
	/*
	 *	Each channel has 1 tuner
	 */
	case VIDIOCSCHAN:
	{
		struct video_channel v;
		unsigned int  channel;

		if(copy_from_user(&v, arg,sizeof(v)))
			return -EFAULT;
		channel = v.channel;
		
		if (channel>bttv_tvcards[btv->type].video_inputs)
			return -EINVAL;
		if (v.norm > TVNORMS)
			return -EOPNOTSUPP;

		bttv_call_i2c_clients(btv,cmd,&v);
		down(&btv->lock);
		bt848_muxsel(btv, channel);
		bttv_set_norm(btv, v.norm);
		up(&btv->lock);
		return 0;
	}
	case VIDIOCGTUNER:
	{
		struct video_tuner v;
		if(copy_from_user(&v,arg,sizeof(v))!=0)
			return -EFAULT;
#if 0 /* tuner.signal might be of intrest for non-tuner sources too ... */
		if(v.tuner||btv->channel)	/* Only tuner 0 */
			return -EINVAL;
#endif
		strcpy(v.name, "Television");
		v.rangelow=0;
		v.rangehigh=0xFFFFFFFF;
		v.flags=VIDEO_TUNER_PAL|VIDEO_TUNER_NTSC|VIDEO_TUNER_SECAM;
		v.mode = btv->win.norm;
		v.signal = (btread(BT848_DSTATUS)&BT848_DSTATUS_HLOC) ? 0xFFFF : 0;
		bttv_call_i2c_clients(btv,cmd,&v);
		if(copy_to_user(arg,&v,sizeof(v)))
			return -EFAULT;
		return 0;
	}
	/* We have but one tuner */
	case VIDIOCSTUNER:
	{
		struct video_tuner v;
		unsigned int tuner;
		
		if(copy_from_user(&v, arg, sizeof(v)))
			return -EFAULT;
		tuner = v.tuner;
		/* Only one channel has a tuner */
		if(tuner!=bttv_tvcards[btv->type].tuner)
			return -EINVAL;
 				
		if(v.mode!=VIDEO_MODE_PAL&&v.mode!=VIDEO_MODE_NTSC
		   &&v.mode!=VIDEO_MODE_SECAM)
			return -EOPNOTSUPP;
		bttv_call_i2c_clients(btv,cmd,&v);
		if (btv->win.norm != v.mode) {
                        down(&btv->lock);
			bttv_set_norm(btv,v.mode);
			up(&btv->lock);
		}
		return 0;
	}
	case VIDIOCGPICT:
	{
		struct video_picture p=btv->picture;
		if(copy_to_user(arg, &p, sizeof(p)))
			return -EFAULT;
		return 0;
	}
	case VIDIOCSPICT:
	{
		struct video_picture p;

		if (copy_from_user(&p, arg,sizeof(p)))
			return -EFAULT;
		if (p.palette > PALETTEFMT_MAX)
			return -EINVAL;
		if (UNSET == palette2fmt[p.palette])
			return -EINVAL;
		down(&btv->lock);
		/* We want -128 to 127 we get 0-65535 */
		bt848_bright(btv, (p.brightness>>8)-128);
		/* 0-511 for the colour */
		bt848_sat_u(btv, p.colour>>7);
		bt848_sat_v(btv, ((p.colour>>7)*201L)/237);
		/* -128 to 127 */
		bt848_hue(btv, (p.hue>>8)-128);
		/* 0-511 */
		bt848_contrast(btv, p.contrast>>7);
		btv->picture = p;
		up(&btv->lock);
		return 0;
	}
	case VIDIOCSWIN:
	{
		struct video_window vw;
		struct video_clip *vcp = NULL;
			
		if(copy_from_user(&vw,arg,sizeof(vw)))
			return -EFAULT;

		down(&btv->lock);
		if(vw.flags || vw.width < 16 || vw.height < 16) 
		{
			spin_lock_irqsave(&btv->s_lock, irq_flags);
			btv->scr_on = 0;
			bt848_set_risc_jmps(btv,-1);
			spin_unlock_irqrestore(&btv->s_lock, irq_flags);
			up(&btv->lock);
			return -EINVAL;
		}
		if (btv->win.bpp < 4) 
		{	/* adjust and align writes */
			vw.x = (vw.x + 3) & ~3;
			vw.width &= ~3;
		}
		if (btv->needs_restart)
			bt848_restart(btv);
		btv->win.x=vw.x;
		btv->win.y=vw.y;
		btv->win.width=vw.width;
		btv->win.height=vw.height;
		
		spin_lock_irqsave(&btv->s_lock, irq_flags);
		bt848_set_risc_jmps(btv,0);
		bt848_set_winsize(btv);
		spin_unlock_irqrestore(&btv->s_lock, irq_flags);

		/*
		 *	Do any clips.
		 */
		if(vw.clipcount<0) {
			if((vcp=vmalloc(VIDEO_CLIPMAP_SIZE))==NULL) {
				up(&btv->lock);
				return -ENOMEM;
			}
			if(copy_from_user(vcp, vw.clips,
					  VIDEO_CLIPMAP_SIZE)) {
				up(&btv->lock);
				vfree(vcp);
				return -EFAULT;
			}
		} else if (vw.clipcount > 2048) {
			up(&btv->lock);
			return -EINVAL;
		} else if (vw.clipcount) {
			if((vcp=vmalloc(sizeof(struct video_clip)*
					(vw.clipcount))) == NULL) {
				up(&btv->lock);
				return -ENOMEM;
			}
			if(copy_from_user(vcp,vw.clips,
					  sizeof(struct video_clip)*
					  vw.clipcount)) {
				up(&btv->lock);
				vfree(vcp);
				return -EFAULT;
			}
		}
		make_clip_tab(btv, vcp, vw.clipcount);
		if (vw.clipcount != 0)
			vfree(vcp);
		spin_lock_irqsave(&btv->s_lock, irq_flags);
		bt848_set_risc_jmps(btv,-1);
		spin_unlock_irqrestore(&btv->s_lock, irq_flags);
		up(&btv->lock);
		return 0;
	}
	case VIDIOCGWIN:
	{
		struct video_window vw;
		memset(&vw,0,sizeof(vw));
		vw.x=btv->win.x;
		vw.y=btv->win.y;
		vw.width=btv->win.width;
		vw.height=btv->win.height;
		if(btv->win.interlace)
			vw.flags|=VIDEO_WINDOW_INTERLACE;
		if(copy_to_user(arg,&vw,sizeof(vw)))
			return -EFAULT;
		return 0;
	}
	case VIDIOCCAPTURE:
	{
		int v;
		if(copy_from_user(&v, arg,sizeof(v)))
			return -EFAULT;
		if(btv->win.vidadr == 0)
			return -EINVAL;
		if (btv->win.width==0 || btv->win.height==0)
			return -EINVAL;
		if (1 == no_overlay)
			return -EIO;
		spin_lock_irqsave(&btv->s_lock, irq_flags);
		if (v == 1 && btv->win.vidadr != 0)
			btv->scr_on = 1;
		if (v == 0)
			btv->scr_on = 0;
		bt848_set_risc_jmps(btv,-1);
		spin_unlock_irqrestore(&btv->s_lock, irq_flags);
		return 0;
	}
	case VIDIOCGFBUF:
	{
		struct video_buffer v;
		v.base=(void *)btv->win.vidadr;
		v.height=btv->win.sheight;
		v.width=btv->win.swidth;
		v.depth=btv->win.depth;
		v.bytesperline=btv->win.bpl;
		if(copy_to_user(arg, &v,sizeof(v)))
			return -EFAULT;
		return 0;
			
	}
	case VIDIOCSFBUF:
	{
		struct video_buffer v;
		if(!capable(CAP_SYS_ADMIN) &&
		   !capable(CAP_SYS_RAWIO))
			return -EPERM;
		if(copy_from_user(&v, arg,sizeof(v)))
			return -EFAULT;
		if(v.depth!=8 && v.depth!=15 && v.depth!=16 && 
		   v.depth!=24 && v.depth!=32 && v.width > 16 &&
		   v.height > 16 && v.bytesperline > 16)
			return -EINVAL;
		down(&btv->lock);
		if (v.base)
			btv->win.vidadr=(unsigned long)v.base;
		btv->win.sheight=v.height;
		btv->win.swidth=v.width;
		btv->win.bpp=((v.depth+7)&0x38)/8;
		btv->win.depth=v.depth;
		btv->win.bpl=v.bytesperline;

#if 0 /* was broken for ages and nobody noticed.  Looks like we don't need
	 it any more as everybody explicitly sets the palette using VIDIOCSPICT
	 these days */
		/* set sefault color format */
		switch (v.depth) {
		case  8: btv->picture.palette = VIDEO_PALETTE_HI240;  break;
		case 15: btv->picture.palette = VIDEO_PALETTE_RGB555; break;
		case 16: btv->picture.palette = VIDEO_PALETTE_RGB565; break;
		case 24: btv->picture.palette = VIDEO_PALETTE_RGB24;  break;
		case 32: btv->picture.palette = VIDEO_PALETTE_RGB32;  break;
		}
#endif
	
		if (bttv_debug)
			printk("Display at %p is %d by %d, bytedepth %d, bpl %d\n",
			       v.base, v.width,v.height, btv->win.bpp, btv->win.bpl);
		spin_lock_irqsave(&btv->s_lock, irq_flags);
		bt848_set_winsize(btv);
		spin_unlock_irqrestore(&btv->s_lock, irq_flags);
		up(&btv->lock);
		return 0;		
	}
	case VIDIOCKEY:
	{
		/* Will be handled higher up .. */
		return 0;
	}
	case VIDIOCGFREQ:
	{
		unsigned long v=btv->win.freq;
		if(copy_to_user(arg,&v,sizeof(v)))
			return -EFAULT;
		return 0;
	}
	case VIDIOCSFREQ:
	{
		unsigned long v;
		if(copy_from_user(&v, arg, sizeof(v)))
			return -EFAULT;
		btv->win.freq=v;
		bttv_call_i2c_clients(btv,cmd,&v);
#if 1
		if (btv->radio && btv->has_matchbox)
			tea5757_set_freq(btv,v);
#endif
		return 0;
	}
	
	case VIDIOCGAUDIO:
	{
		struct video_audio v;

		v=btv->audio_dev;
		v.flags&=~(VIDEO_AUDIO_MUTE|VIDEO_AUDIO_MUTABLE);
		v.flags|=VIDEO_AUDIO_MUTABLE;
		strcpy(v.name,"TV");

		v.mode = VIDEO_SOUND_MONO;
		bttv_call_i2c_clients(btv,cmd,&v);

		/* card specific hooks */
		if (btv->audio_hook)
			btv->audio_hook(btv,&v,0);

		if(copy_to_user(arg,&v,sizeof(v)))
			return -EFAULT;
		return 0;
	}
	case VIDIOCSAUDIO:
	{
		struct video_audio v;
		unsigned int n;

		if(copy_from_user(&v,arg, sizeof(v)))
			return -EFAULT;
		n = v.audio;
		if(n >= bttv_tvcards[btv->type].audio_inputs)
			return -EINVAL;
		down(&btv->lock);
		if(v.flags&VIDEO_AUDIO_MUTE)
			audio(btv, AUDIO_MUTE);
		/* bt848_muxsel(btv,v.audio); */
		if(!(v.flags&VIDEO_AUDIO_MUTE))
			audio(btv, AUDIO_UNMUTE);

		bttv_call_i2c_clients(btv,cmd,&v);
		
		/* card specific hooks */
		if (btv->audio_hook)
			btv->audio_hook(btv,&v,1);

		btv->audio_dev=v;
		up(&btv->lock);
		return 0;
	}

	case VIDIOCSYNC:
	{
		DECLARE_WAITQUEUE(wait, current);
		unsigned int i;

		if(copy_from_user((void *)&i,arg,sizeof(int)))
			return -EFAULT;
		if (i >= gbuffers)
			return -EINVAL;
		switch (btv->gbuf[i].stat) {
		case GBUFFER_UNUSED:
			ret = -EINVAL;
			break;
		case GBUFFER_GRABBING:
			add_wait_queue(&btv->capq, &wait);
			current->state = TASK_INTERRUPTIBLE;
			while(btv->gbuf[i].stat==GBUFFER_GRABBING) {
				if (bttv_debug)
					printk("bttv%d: cap sync: sleep on %d\n",btv->nr,i);
				schedule();
				if(signal_pending(current)) {
					remove_wait_queue(&btv->capq, &wait);
					current->state = TASK_RUNNING;
					return -EINTR;
				}
			}
			remove_wait_queue(&btv->capq, &wait);
			current->state = TASK_RUNNING;
			/* fall throuth */
		case GBUFFER_DONE:
		case GBUFFER_ERROR:
			ret = (btv->gbuf[i].stat == GBUFFER_ERROR) ? -EIO : 0;
			if (bttv_debug)
				printk("bttv%d: cap sync: buffer %d, retval %d\n",btv->nr,i,ret);
			btv->gbuf[i].stat = GBUFFER_UNUSED;
		}
		if (btv->needs_restart) {
			down(&btv->lock);
			bt848_restart(btv);
			up(&btv->lock);
		}
		return ret;
	}

	case BTTV_FIELDNR: 
		if(copy_to_user((void *) arg, (void *) &btv->last_field, 
				sizeof(btv->last_field)))
			return -EFAULT;
		break;
      
	case BTTV_PLLSET: {
		struct bttv_pll_info p;
		if(!capable(CAP_SYS_ADMIN))
			return -EPERM;
		if(copy_from_user(&p , (void *) arg, sizeof(btv->pll)))
			return -EFAULT;
		down(&btv->lock);
		btv->pll.pll_ifreq = p.pll_ifreq;
		btv->pll.pll_ofreq = p.pll_ofreq;
		btv->pll.pll_crystal = p.pll_crystal;
		up(&btv->lock);
		break;
	}

	case VIDIOCMCAPTURE:
	{
		struct video_mmap vm;
		int ret;
		if(copy_from_user((void *) &vm, (void *) arg, sizeof(vm)))
			return -EFAULT;
		down(&btv->lock);
		ret = vgrab(btv, &vm);
		up(&btv->lock);
		return ret;
	}
		
	case VIDIOCGMBUF:
	{
		struct video_mbuf vm;
		unsigned int i;
		
		memset(&vm, 0 , sizeof(vm));
		vm.size=gbufsize*gbuffers;
		vm.frames=gbuffers;
		for (i = 0; i < gbuffers; i++)
			vm.offsets[i]=i*gbufsize;
		if(copy_to_user((void *)arg, (void *)&vm, sizeof(vm)))
			return -EFAULT;
		return 0;
	}
		
	case VIDIOCGUNIT:
	{
		struct video_unit vu;
		vu.video=btv->video_dev.minor;
		vu.vbi=btv->vbi_dev.minor;
		if(btv->radio_dev.minor!=-1)
			vu.radio=btv->radio_dev.minor;
		else
			vu.radio=VIDEO_NO_UNIT;
		vu.audio=VIDEO_NO_UNIT;
		vu.teletext=VIDEO_NO_UNIT;
		if(copy_to_user((void *)arg, (void *)&vu, sizeof(vu)))
			return -EFAULT;
		return 0;
	}
		
	case BTTV_BURST_ON:
	{
		burst(1);
		return 0;
	}

	case BTTV_BURST_OFF:
	{
		burst(0);
		return 0;
	}

	case BTTV_VERSION:
	{
		return BTTV_VERSION_CODE;
	}
                        
	case BTTV_PICNR:
	{
		/* return picture;*/
		return  0;
	}

	default:
		return -ENOIOCTLCMD;
	}
	return 0;
}

/*
 *	This maps the vmalloced and reserved fbuffer to user space.
 *
 *  FIXME: 
 *  - PAGE_READONLY should suffice!?
 *  - remap_page_range is kind of inefficient for page by page remapping.
 *    But e.g. pte_alloc() does not work in modules ... :-(
 */

static int do_bttv_mmap(struct bttv *btv, const char *adr, unsigned long size)
{
        unsigned long start=(unsigned long) adr;
        unsigned long page,pos;

        if (size>gbuffers*gbufsize)
                return -EINVAL;
        if (!btv->fbuffer) {
                if(fbuffer_alloc(btv))
                        return -EINVAL;
        }
        pos=(unsigned long) btv->fbuffer;
        while (size > 0) {
                page = kvirt_to_pa(pos);
                if (remap_page_range(start, page, PAGE_SIZE, PAGE_SHARED))
                        return -EAGAIN;
                start+=PAGE_SIZE;
                pos+=PAGE_SIZE;
                size-=PAGE_SIZE;
        }
        return 0;
}

static int bttv_mmap(struct video_device *dev, const char *adr, unsigned long size)
{
        struct bttv *btv=(struct bttv *)dev;
        int r;

        down(&btv->lock);
        r=do_bttv_mmap(btv, adr, size);
        up(&btv->lock);
        return r;
}


static struct video_device bttv_template=
{
	.owner		= THIS_MODULE,
	.name		= "bttv video",
	.type		= VID_TYPE_TUNER|VID_TYPE_CAPTURE|VID_TYPE_OVERLAY|VID_TYPE_TELETEXT,
	.hardware	= VID_HARDWARE_BT848,
	.open		= bttv_open,
	.close		= bttv_close,
	.read		= bttv_read,
	.write		= bttv_write,
	.ioctl		= bttv_ioctl,
	.mmap		= bttv_mmap,
	.minor		= -1,
};


static long vbi_read(struct video_device *v, char *buf, unsigned long count,
		     int nonblock)
{
	struct bttv *btv=(struct bttv *)(v-2);
	unsigned long todo;
	unsigned int q;
	DECLARE_WAITQUEUE(wait, current);

	todo=count;
	while (todo && todo>(q=VBIBUF_SIZE-btv->vbip)) 
	{
		if (btv->needs_restart) {
			down(&btv->lock);
			bt848_restart(btv);
			up(&btv->lock);
		}
		if(copy_to_user((void *) buf, (void *) btv->vbibuf+btv->vbip, q))
			return -EFAULT;
		todo-=q;
		buf+=q;

		add_wait_queue(&btv->vbiq, &wait);
		current->state = TASK_INTERRUPTIBLE;
		if (todo && q==VBIBUF_SIZE-btv->vbip) 
		{
			if(nonblock)
			{
				remove_wait_queue(&btv->vbiq, &wait);
				current->state = TASK_RUNNING;
				if(count==todo)
					return -EWOULDBLOCK;
				return count-todo;
			}
			schedule();
			if(signal_pending(current))
			{
				remove_wait_queue(&btv->vbiq, &wait);
                                current->state = TASK_RUNNING;
				if(todo==count)
					return -EINTR;
				else
					return count-todo;
			}
		}
		remove_wait_queue(&btv->vbiq, &wait);
		current->state = TASK_RUNNING;
	}
	if (todo) 
	{
		if(copy_to_user((void *) buf, (void *) btv->vbibuf+btv->vbip, todo))
			return -EFAULT;
		btv->vbip+=todo;
	}
	return count;
}

static unsigned int vbi_poll(struct video_device *dev, struct file *file,
	poll_table *wait)
{
	struct bttv *btv=(struct bttv *)(dev-2);
	unsigned int mask = 0;

	poll_wait(file, &btv->vbiq, wait);

	if (btv->vbip < VBIBUF_SIZE)
		mask |= (POLLIN | POLLRDNORM);

	return mask;
}

static int vbi_open(struct video_device *dev, int flags)
{
	struct bttv *btv=(struct bttv *)(dev-2);
 	unsigned long irq_flags;

        down(&btv->lock);
	if (btv->needs_restart)
		bt848_restart(btv);
	set_pll(btv);
	btv->vbip=VBIBUF_SIZE;
	spin_lock_irqsave(&btv->s_lock, irq_flags);
	btv->vbi_on = 1;
	bt848_set_risc_jmps(btv,-1);
	spin_unlock_irqrestore(&btv->s_lock, irq_flags);
	up(&btv->lock);

	return 0;   
}

static void vbi_close(struct video_device *dev)
{
	struct bttv *btv=(struct bttv *)(dev-2);
 	unsigned long irq_flags;

	spin_lock_irqsave(&btv->s_lock, irq_flags);
	btv->vbi_on = 0;
	bt848_set_risc_jmps(btv,-1);
	spin_unlock_irqrestore(&btv->s_lock, irq_flags);
}

static int vbi_ioctl(struct video_device *dev, unsigned int cmd, void *arg)
{
	struct bttv *btv=(struct bttv *)(dev-2);

	switch (cmd) {	
	case VIDIOCGCAP:
	{
		struct video_capability b;
		strcpy(b.name,btv->vbi_dev.name);
		b.type = ((bttv_tvcards[btv->type].tuner != UNSET) ? VID_TYPE_TUNER : 0)
			| VID_TYPE_TELETEXT;
		b.channels = 0;
		b.audios = 0;
		b.maxwidth = 0;
		b.maxheight = 0;
		b.minwidth = 0;
		b.minheight = 0;
		if(copy_to_user(arg,&b,sizeof(b)))
			return -EFAULT;
		return 0;
	}
	case VIDIOCGFREQ:
	case VIDIOCSFREQ:
	case VIDIOCGTUNER:
	case VIDIOCSTUNER:
	case VIDIOCGCHAN:
	case VIDIOCSCHAN:
	case BTTV_VERSION:
		return bttv_ioctl(dev-2,cmd,arg);
	case BTTV_VBISIZE:
		/* make alevt happy :-) */
		return VBIBUF_SIZE;
	default:
		return -EINVAL;
	}
}

static struct video_device vbi_template=
{
	.owner		= THIS_MODULE,
	.name		= "bttv vbi",
	.type		= VID_TYPE_CAPTURE|VID_TYPE_TELETEXT,
	.hardware	= VID_HARDWARE_BT848,
	.open		= vbi_open,
	.close		= vbi_close,
	.read		= vbi_read,
	.write		= bttv_write,
	.poll		= vbi_poll,
	.ioctl		= vbi_ioctl,
	.minor		= -1,
};


static int radio_open(struct video_device *dev, int flags)
{
	struct bttv *btv = (struct bttv *)(dev-1);

        down(&btv->lock);
	if (btv->user)
		goto busy_unlock;
	btv->user++;

	btv->radio = 1;
	bttv_call_i2c_clients(btv,AUDC_SET_RADIO,&btv->tuner_type);
	bt848_muxsel(btv,0);
	up(&btv->lock);

	return 0;   

 busy_unlock:
	up(&btv->lock);
	return -EBUSY;
}

static void radio_close(struct video_device *dev)
{
	struct bttv *btv=(struct bttv *)(dev-1);

	down(&btv->lock);
	btv->user--;
	btv->radio = 0;
	up(&btv->lock);
}

static long radio_read(struct video_device *v, char *buf, unsigned long count, int nonblock)
{
	return -EINVAL;
}

static int radio_ioctl(struct video_device *dev, unsigned int cmd, void *arg)
{
        struct bttv *btv=(struct bttv *)(dev-1);
	switch (cmd) {	
	case VIDIOCGCAP:
	{
		struct video_capability v;
		strcpy(v.name,btv->video_dev.name);
		v.type = VID_TYPE_TUNER;
		v.channels = 1;
		v.audios = 1;
		/* No we don't do pictures */
		v.maxwidth = 0;
		v.maxheight = 0;
		v.minwidth = 0;
		v.minheight = 0;
		if (copy_to_user(arg, &v, sizeof(v)))
			return -EFAULT;
		return 0;
		break;
	}
	case VIDIOCGTUNER:
	{
		struct video_tuner v;
		if(copy_from_user(&v,arg,sizeof(v))!=0)
			return -EFAULT;
		if(v.tuner||btv->channel)	/* Only tuner 0 */
			return -EINVAL;
		strcpy(v.name, "Radio");
		/* japan:          76.0 MHz -  89.9 MHz
		   western europe: 87.5 MHz - 108.0 MHz
		   russia:         65.0 MHz - 108.0 MHz */
		v.rangelow=(int)(65*16);
		v.rangehigh=(int)(108*16);
		v.flags= 0; /* XXX */
		v.mode = 0; /* XXX */
		bttv_call_i2c_clients(btv,cmd,&v);
		if(copy_to_user(arg,&v,sizeof(v)))
			return -EFAULT;
		return 0;
	}
	case VIDIOCSTUNER:
	{
		struct video_tuner v;
		if(copy_from_user(&v, arg, sizeof(v)))
			return -EFAULT;
		/* Only channel 0 has a tuner */
		if(v.tuner!=0 || btv->channel)
			return -EINVAL;
		/* XXX anything to do ??? */
		return 0;
	}
	case VIDIOCGFREQ:
	case VIDIOCSFREQ:
	case VIDIOCGAUDIO:
	case VIDIOCSAUDIO:
		bttv_ioctl((struct video_device *)btv,cmd,arg);
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return 0;
}

static struct video_device radio_template=
{
	.owner		= THIS_MODULE,
	.name		= "bttv radio",
	.type		= VID_TYPE_TUNER,
	.hardware	= VID_HARDWARE_BT848,
	.open		= radio_open,
	.close		= radio_close,
	.read		= radio_read,          /* just returns -EINVAL */
	.write		= bttv_write,          /* just returns -EINVAL */
	.ioctl		= radio_ioctl,
	.minor		= -1,
};


static void bt848_set_risc_jmps(struct bttv *btv, int flags)
{
	if (-1 == flags) {
		/* defaults */
		flags = 0;
		if (btv->scr_on)
			flags |= 0x03;
		if (btv->vbi_on)
			flags |= 0x0c;
		if (bttv_tvcards[btv->type].muxsel[btv->channel] < 0)
			flags |= 0x0c;
#if 0
		/* Hmm ... */
		if ((0 != btv->risc_cap_even) ||
		    (0 != btv->risc_cap_odd))
			flags |= 0x0c;
#endif
	}

	if (bttv_debug > 1)
		printk("bttv%d: set_risc_jmp %08lx:",
		       btv->nr,virt_to_bus(btv->risc_jmp));

	/* Sync to start of odd field */
	btv->risc_jmp[0]=cpu_to_le32(BT848_RISC_SYNC|BT848_RISC_RESYNC
                                |BT848_FIFO_STATUS_VRE);
	btv->risc_jmp[1]=cpu_to_le32(0);

	/* Jump to odd vbi sub */
	btv->risc_jmp[2]=cpu_to_le32(BT848_RISC_JUMP|(0xd<<20));
	if (flags&8) {
		if (bttv_debug > 1)
			printk(" ev=%08lx",virt_to_bus(btv->vbi_odd));
		btv->risc_jmp[3]=cpu_to_le32(virt_to_bus(btv->vbi_odd));
	} else {
		if (bttv_debug > 1)
			printk(" -----------");
		btv->risc_jmp[3]=cpu_to_le32(virt_to_bus(btv->risc_jmp+4));
	}

        /* Jump to odd sub */
	btv->risc_jmp[4]=cpu_to_le32(BT848_RISC_JUMP|(0xe<<20));
	if (0 != btv->risc_cap_odd) {
		if (bttv_debug > 1)
			printk(" e%d=%08x",btv->gq_grab,btv->risc_cap_odd);
		flags |= 3;
		btv->risc_jmp[5]=cpu_to_le32(btv->risc_cap_odd);
	} else if ((flags&2) &&
		   (!btv->win.interlace || 0 == btv->risc_cap_even)) {
		if (bttv_debug > 1)
			printk(" eo=%08lx",virt_to_bus(btv->risc_scr_odd));
		btv->risc_jmp[5]=cpu_to_le32(virt_to_bus(btv->risc_scr_odd));
	} else {
		if (bttv_debug > 1)
			printk(" -----------");
		btv->risc_jmp[5]=cpu_to_le32(virt_to_bus(btv->risc_jmp+6));
	}


	/* Sync to start of even field */
	btv->risc_jmp[6]=cpu_to_le32(BT848_RISC_SYNC|BT848_RISC_RESYNC
                                |BT848_FIFO_STATUS_VRO);
	btv->risc_jmp[7]=cpu_to_le32(0);

	/* Jump to even vbi sub */
	btv->risc_jmp[8]=cpu_to_le32(BT848_RISC_JUMP);
	if (flags&4) {
		if (bttv_debug > 1)
			printk(" ov=%08lx",virt_to_bus(btv->vbi_even));
		btv->risc_jmp[9]=cpu_to_le32(virt_to_bus(btv->vbi_even));
	} else {
		if (bttv_debug > 1)
			printk(" -----------");
		btv->risc_jmp[9]=cpu_to_le32(virt_to_bus(btv->risc_jmp+10));
	}

	/* Jump to even sub */
	btv->risc_jmp[10]=cpu_to_le32(BT848_RISC_JUMP|(8<<20));
	if (0 != btv->risc_cap_even) {
		if (bttv_debug > 1)
			printk(" o%d=%08x",btv->gq_grab,btv->risc_cap_even);
		flags |= 3;
		btv->risc_jmp[11]=cpu_to_le32(btv->risc_cap_even);
	} else if ((flags&1) &&
		   btv->win.interlace) {
		if (bttv_debug > 1)
			printk(" oo=%08lx",virt_to_bus(btv->risc_scr_even));
		btv->risc_jmp[11]=cpu_to_le32(virt_to_bus(btv->risc_scr_even));
	} else {
		if (bttv_debug > 1)
			printk(" -----------");
		btv->risc_jmp[11]=cpu_to_le32(virt_to_bus(btv->risc_jmp+12));
	}

	if (btv->gq_start) {
		btv->risc_jmp[12]=cpu_to_le32(BT848_RISC_JUMP|(0x8<<16)|BT848_RISC_IRQ);
	} else {
		btv->risc_jmp[12]=cpu_to_le32(BT848_RISC_JUMP);
	}
	btv->risc_jmp[13]=cpu_to_le32(virt_to_bus(btv->risc_jmp));

	/* enable cpaturing and DMA */
	if (bttv_debug > 1)
		printk(" flags=0x%x dma=%s\n",
		       flags,(flags&0x0f) ? "on" : "off");
	btaor(flags, ~0x0f, BT848_CAP_CTL);
	if (flags&0x0f)
		bt848_dma(btv, 3);
	else
		bt848_dma(btv, 0);
}

static int __devinit init_video_dev(struct bttv *btv)
{
	audio(btv, AUDIO_MUTE);
        
	if (video_register_device(&btv->video_dev,VFL_TYPE_GRABBER,video_nr)<0)
		return -1;
	printk(KERN_INFO "bttv%d: registered device video%d\n",
	       btv->nr,btv->video_dev.minor & 0x1f);
	if (video_register_device(&btv->vbi_dev,VFL_TYPE_VBI,vbi_nr)<0) {
	        video_unregister_device(&btv->video_dev);
		return -1;
	}
	printk(KERN_INFO "bttv%d: registered device vbi%d\n",
	       btv->nr,btv->vbi_dev.minor & 0x1f);
	if (btv->has_radio) {
		if(video_register_device(&btv->radio_dev, VFL_TYPE_RADIO, radio_nr)<0) {
		        video_unregister_device(&btv->vbi_dev);
		        video_unregister_device(&btv->video_dev);
			return -1;
		}
		printk(KERN_INFO "bttv%d: registered device radio%d\n",
		       btv->nr,btv->radio_dev.minor & 0x1f);
	}
        return 1;
}

static int __devinit init_bt848(struct bttv *btv)
{
	int val;
	unsigned int j;
 	unsigned long irq_flags;

	btv->user=0; 
        init_MUTEX(&btv->lock);

	/* dump current state of the gpio registers before changing them,
	 * might help to make a new card work */
	if (bttv_gpio) {
		bttv_gpio_tracking(btv,"init #1");
		bttv_gpio_tracking(btv,"init #1");
	}

	/* reset the bt848 */
	btwrite(0, BT848_SRESET);
	
	/* not registered yet */
	btv->video_dev.minor = -1;
	btv->radio_dev.minor = -1;
	btv->vbi_dev.minor = -1;

	/* default setup for max. PAL size in a 1024xXXX hicolor framebuffer */
	btv->win.norm=0; /* change this to 1 for NTSC, 2 for SECAM */
	btv->win.interlace=1;
	btv->win.x=0;
	btv->win.y=0;
	btv->win.width=320;
	btv->win.height=240;
	btv->win.bpp=2;
	btv->win.depth=16;
	btv->win.color_fmt=BT848_COLOR_FMT_RGB16;
	btv->win.bpl=1024*btv->win.bpp;
	btv->win.swidth=1024;
	btv->win.sheight=768;
	btv->win.vidadr=0;
	btv->vbi_on=0;
	btv->scr_on=0;

	btv->risc_scr_odd=0;
	btv->risc_scr_even=0;
	btv->risc_cap_odd=0;
	btv->risc_cap_even=0;
	btv->risc_jmp=0;
	btv->vbibuf=0;
        btv->field=btv->last_field=0;

	btv->errors=0;
	btv->needs_restart=0;
	btv->has_radio=radio[btv->nr];

	if (!(btv->risc_scr_odd=(unsigned int *) kmalloc(RISCMEM_LEN/2, GFP_KERNEL)))
		return -1;
	if (!(btv->risc_scr_even=(unsigned int *) kmalloc(RISCMEM_LEN/2, GFP_KERNEL)))
		return -1;
	if (!(btv->risc_jmp =(unsigned int *) kmalloc(2048, GFP_KERNEL)))
		return -1;
	btv->vbi_odd=btv->risc_jmp+16;
	btv->vbi_even=btv->vbi_odd+256;
	btv->bus_vbi_odd=virt_to_bus(btv->risc_jmp+12);
	btv->bus_vbi_even=virt_to_bus(btv->risc_jmp+6);

	btwrite(virt_to_bus(btv->risc_jmp+2), BT848_RISC_STRT_ADD);
	btv->vbibuf=(unsigned char *) vmalloc_32(VBIBUF_SIZE);
	if (!btv->vbibuf) 
		return -1;
	if (!(btv->gbuf = kmalloc(sizeof(struct bttv_gbuf)*gbuffers,GFP_KERNEL)))
		return -1;
	for (j = 0; j < gbuffers; j++) {
		if (!(btv->gbuf[j].risc = kmalloc(16384,GFP_KERNEL)))
			return -1;
	}
	
	memset(btv->vbibuf, 0, VBIBUF_SIZE); /* We don't want to return random
	                                        memory to the user */

	btv->fbuffer=NULL;

/*	btwrite(0, BT848_TDEC); */
        btwrite(0x10, BT848_COLOR_CTL);
	btwrite(0x00, BT848_CAP_CTL);
	/* set planar and packed mode trigger points and         */
	/* set rising edge of inverted GPINTR pin as irq trigger */
	btwrite(BT848_GPIO_DMA_CTL_PKTP_32|
		BT848_GPIO_DMA_CTL_PLTP1_16|
		BT848_GPIO_DMA_CTL_PLTP23_16|
		BT848_GPIO_DMA_CTL_GPINTC|
		BT848_GPIO_DMA_CTL_GPINTI, 
		BT848_GPIO_DMA_CTL);

        /* select direct input */
	btwrite(0x00, BT848_GPIO_REG_INP);
	btwrite(0x00, BT848_GPIO_OUT_EN);
	if (bttv_gpio)
		bttv_gpio_tracking(btv,"init #2");

	btwrite(BT848_IFORM_MUX1 | BT848_IFORM_XTAUTO | BT848_IFORM_AUTO,
		BT848_IFORM);

	btwrite(0xd8, BT848_CONTRAST_LO);
	bt848_bright(btv, 0x10);

	btwrite(0x20, BT848_E_VSCALE_HI);
	btwrite(0x20, BT848_O_VSCALE_HI);
	btwrite(BT848_ADC_RESERVED | (adc_crush ? BT848_ADC_CRUSH : 0),
		BT848_ADC);

	if (lumafilter) {
		btwrite(0, BT848_E_CONTROL);
		btwrite(0, BT848_O_CONTROL);
	} else {
		btwrite(BT848_CONTROL_LDEC, BT848_E_CONTROL);
		btwrite(BT848_CONTROL_LDEC, BT848_O_CONTROL);
	}

	btv->picture.colour     = 254<<7;
	btv->picture.brightness = 128<<8;
	btv->picture.hue        = 128<<8;
	btv->picture.contrast   = 0xd8<<7;
	btv->picture.palette    = VIDEO_PALETTE_RGB24;

	val = chroma_agc ? BT848_SCLOOP_CAGC : 0;
        btwrite(val, BT848_E_SCLOOP);
        btwrite(val, BT848_O_SCLOOP);

	/* clear interrupt status */
	btwrite(0xfffffUL, BT848_INT_STAT);
        
	/* set interrupt mask */
	btwrite(btv->triton1|
                /*BT848_INT_PABORT|BT848_INT_RIPERR|BT848_INT_PPERR|
                  BT848_INT_FDSR|BT848_INT_FTRGT|BT848_INT_FBUS|*/
                (fieldnr ? BT848_INT_VSYNC : 0) |
                (gpint   ? BT848_INT_GPINT : 0) |
		BT848_INT_SCERR|
		BT848_INT_RISCI|BT848_INT_OCERR|BT848_INT_VPRES|
		BT848_INT_FMTCHG|BT848_INT_HLOCK,
		BT848_INT_MASK);

	make_vbitab(btv);
	spin_lock_irqsave(&btv->s_lock, irq_flags);
	bt848_set_risc_jmps(btv,-1);
	spin_unlock_irqrestore(&btv->s_lock, irq_flags);

	/* needs to be done before i2c is registered */
	bttv_init_card1(btv);

	/* register i2c */
        btv->tuner_type  = UNSET;
        btv->pinnacle_id = UNSET;
        init_bttv_i2c(btv);

	/* some card-specific stuff (needs working i2c) */
	bttv_init_card2(btv);

	bt848_muxsel(btv, 1);
	bt848_set_winsize(btv);

	/*
	 *	Now add the template and register the device unit.
	 */
        init_video_dev(btv);

	return 0;
}

/* ----------------------------------------------------------------------- */

static char *irq_name[] = { "FMTCHG", "VSYNC", "HSYNC", "OFLOW", "HLOCK",
			    "VPRES", "6", "7", "I2CDONE", "GPINT", "10",
			    "RISCI", "FBUS", "FTRGT", "FDSR", "PPERR",
			    "RIPERR", "PABORT", "OCERR", "SCERR" };

static void bttv_irq(int irq, void *dev_id, struct pt_regs * regs)
{
	u32 stat,astat;
	u32 dstat;
	int count;
	struct bttv *btv;

	btv=(struct bttv *)dev_id;
	count=0;
	while (1) 
	{
		/* get/clear interrupt status bits */
		stat=btread(BT848_INT_STAT);
		astat=stat&btread(BT848_INT_MASK);
		if (!astat)
			return;
		btwrite(stat,BT848_INT_STAT);

		/* get device status bits */
		dstat=btread(BT848_DSTATUS);

		if (irq_debug) {
			unsigned int i;
			printk(KERN_DEBUG "bttv%d: irq loop=%d risc=%x, bits:",
			       btv->nr, count, stat>>28);
			for (i = 0; i < ARRAY_SIZE(irq_name); i++) {
				if (stat & (1 << i))
					printk(" %s",irq_name[i]);
				if (astat & (1 << i))
					printk("*");
			}
			if (stat & BT848_INT_HLOCK)
				printk("   HLOC => %s", (dstat & BT848_DSTATUS_HLOC)
				       ? "yes" : "no");
			if (stat & BT848_INT_VPRES)
				printk("   PRES => %s", (dstat & BT848_DSTATUS_PRES)
				       ? "yes" : "no");
			if (stat & BT848_INT_FMTCHG)
				printk("   NUML => %s", (dstat & BT848_DSTATUS_PRES)
				       ? "625" : "525");
			printk("\n");
		}

		if (astat&BT848_INT_GPINT)
			wake_up_interruptible(&btv->gpioq);

		if (astat&BT848_INT_VSYNC) 
                        btv->field++;

		if (astat&(BT848_INT_SCERR|BT848_INT_OCERR)) {
			if (bttv_verbose)
				printk("bttv%d: irq:%s%s risc_count=%08x\n",
				       btv->nr,
				       (astat&BT848_INT_SCERR) ? " SCERR" : "",
				       (astat&BT848_INT_OCERR) ? " OCERR" : "",
				       btread(BT848_RISC_COUNT));
			btv->errors++;
			if (btv->errors < BTTV_ERRORS) {
				spin_lock(&btv->s_lock);
				btand(~15, BT848_GPIO_DMA_CTL);
				btwrite(virt_to_bus(btv->risc_jmp+2),
					BT848_RISC_STRT_ADD);
				bt848_set_geo(btv);
				bt848_set_risc_jmps(btv,-1);
				spin_unlock(&btv->s_lock);
			} else {
				if (bttv_verbose)
					printk("bttv%d: aiee: error loops\n",btv->nr);
				bt848_offline(btv);
			}
		}
		if (astat&BT848_INT_RISCI) 
		{
			if (bttv_debug > 1)
				printk("bttv%d: IRQ_RISCI\n",btv->nr);

			/* captured VBI frame */
			if (stat&(1<<28)) 
			{
				btv->vbip=0;
				/* inc vbi frame count for detecting drops */
				(*(u32 *)&(btv->vbibuf[VBIBUF_SIZE - 4]))++;
				wake_up_interruptible(&btv->vbiq);
			}

			/* captured full frame */
			if (stat&(2<<28) && btv->gq_grab != -1) 
			{
                                btv->last_field=btv->field;
				if (bttv_debug)
					printk("bttv%d: cap irq: done %d\n",btv->nr,btv->gq_grab);
				do_gettimeofday(&btv->gbuf[btv->gq_grab].tv);
				spin_lock(&btv->s_lock);
				btv->gbuf[btv->gq_grab].stat = GBUFFER_DONE;
				btv->gq_grab = -1;
			        if (btv->gq_in != btv->gq_out)
				{
					btv->gq_grab = btv->gqueue[btv->gq_out++];
					btv->gq_out  = btv->gq_out % MAX_GBUFFERS;
					if (bttv_debug)
						printk("bttv%d: cap irq: capture %d\n",btv->nr,btv->gq_grab);
                                        btv->risc_cap_odd  = btv->gbuf[btv->gq_grab].ro;
					btv->risc_cap_even = btv->gbuf[btv->gq_grab].re;
					bt848_set_risc_jmps(btv,-1);
					bt848_set_geo(btv);
					btwrite(BT848_COLOR_CTL_GAMMA,
						BT848_COLOR_CTL);
				} else {
                                        btv->risc_cap_odd  = 0;
					btv->risc_cap_even = 0;
					bt848_set_risc_jmps(btv,-1);
                                        bt848_set_geo(btv);
					btwrite(btv->fb_color_ctl | BT848_COLOR_CTL_GAMMA,
						BT848_COLOR_CTL);
				}
				spin_unlock(&btv->s_lock);
				wake_up_interruptible(&btv->capq);
				break;
			}
			if (stat&(8<<28) && btv->gq_start) 
			{
				spin_lock(&btv->s_lock);
				btv->gq_start = 0;
				btv->gq_grab = btv->gqueue[btv->gq_out++];
				btv->gq_out  = btv->gq_out % MAX_GBUFFERS;
				if (bttv_debug)
					printk("bttv%d: cap irq: capture %d [start]\n",btv->nr,btv->gq_grab);
				btv->risc_cap_odd  = btv->gbuf[btv->gq_grab].ro;
				btv->risc_cap_even = btv->gbuf[btv->gq_grab].re;
				bt848_set_risc_jmps(btv,-1);
				bt848_set_geo(btv);
				btwrite(BT848_COLOR_CTL_GAMMA,
					BT848_COLOR_CTL);
				spin_unlock(&btv->s_lock);
			}
		}

		if (automute && (astat&BT848_INT_HLOCK)) {
			if ((dstat&BT848_DSTATUS_HLOC) || (btv->radio))
				audio(btv, AUDIO_ON);
			else
				audio(btv, AUDIO_OFF);
		}
    
		count++;
		if (count > 20) {
			btwrite(0, BT848_INT_MASK);
			printk(KERN_ERR 
			       "bttv%d: IRQ lockup, cleared int mask\n", btv->nr);
			bt848_offline(btv);
		}
	}
}



/*
 *	Scan for a Bt848 card, request the irq and map the io memory 
 */

static void bttv_remove(struct pci_dev *pci_dev)
{
        u8 command;
        unsigned int j;
        struct bttv *btv = pci_get_drvdata(pci_dev);

	if (bttv_verbose)
		printk("bttv%d: unloading\n",btv->nr);

        /* unregister i2c_bus */
	if (0 == btv->i2c_rc)
		i2c_bit_del_bus(&btv->i2c_adap);

        /* turn off all capturing, DMA and IRQs */
        btand(~15, BT848_GPIO_DMA_CTL);

        /* first disable interrupts before unmapping the memory! */
        btwrite(0, BT848_INT_MASK);
        btwrite(~(u32)0,BT848_INT_STAT);
        btwrite(0, BT848_GPIO_OUT_EN);
	if (bttv_gpio)
		bttv_gpio_tracking(btv,"cleanup");

        /* disable PCI bus-mastering */
        pci_read_config_byte(btv->dev, PCI_COMMAND, &command);
        command &= ~PCI_COMMAND_MASTER;
        pci_write_config_byte(btv->dev, PCI_COMMAND, command);

        /* unmap and free memory */
        for (j = 0; j < gbuffers; j++)
                if (btv->gbuf[j].risc)
                        kfree(btv->gbuf[j].risc);
        if (btv->gbuf)
                kfree((void *) btv->gbuf);

        if (btv->risc_scr_odd)
                kfree((void *) btv->risc_scr_odd);

        if (btv->risc_scr_even)
                kfree((void *) btv->risc_scr_even);

        DEBUG(printk(KERN_DEBUG "free: risc_jmp: 0x%p.\n", btv->risc_jmp));
        if (btv->risc_jmp)
                kfree((void *) btv->risc_jmp);

        DEBUG(printk(KERN_DEBUG "bt848_vbibuf: 0x%p.\n", btv->vbibuf));
        if (btv->vbibuf)
                vfree((void *) btv->vbibuf);

        free_irq(btv->dev->irq,btv);
        DEBUG(printk(KERN_DEBUG "bt848_mem: 0x%p.\n", btv->bt848_mem));
        if (btv->bt848_mem)
                iounmap(btv->bt848_mem);

        if (btv->video_dev.minor!=-1)
                video_unregister_device(&btv->video_dev);
        if (btv->vbi_dev.minor!=-1)
                video_unregister_device(&btv->vbi_dev);
        if (btv->radio_dev.minor != -1)
                video_unregister_device(&btv->radio_dev);

        release_mem_region(pci_resource_start(btv->dev,0),
                           pci_resource_len(btv->dev,0));
        /* wake up any waiting processes
           because shutdown flag is set, no new processes (in this queue)
           are expected
        */
        btv->shutdown=1;
        wake_up(&btv->gpioq);

	pci_set_drvdata(pci_dev, NULL);
        return;
}


static int __devinit bttv_probe(struct pci_dev *dev, const struct pci_device_id *pci_id)
{
	int result;
	unsigned char lat;
	struct bttv *btv;
#if defined(__powerpc__)
        unsigned int cmd;
#endif

	if (bttv_num == BTTV_MAX)
		return -ENOMEM;
	printk(KERN_INFO "bttv: Bt8xx card found (%d).\n", bttv_num);

        btv=&bttvs[bttv_num];
        btv->dev=dev;
        btv->nr = bttv_num;
        btv->bt848_mem=NULL;
        btv->vbibuf=NULL;
        btv->risc_jmp=NULL;
        btv->vbi_odd=NULL;
        btv->vbi_even=NULL;
        init_waitqueue_head(&btv->vbiq);
        init_waitqueue_head(&btv->capq);
        btv->vbip=VBIBUF_SIZE;
	btv->s_lock = SPIN_LOCK_UNLOCKED;
	init_waitqueue_head(&btv->gpioq);
	btv->shutdown=0;
	
	memcpy(&btv->video_dev,&bttv_template, sizeof(bttv_template));
	memcpy(&btv->vbi_dev,&vbi_template, sizeof(vbi_template));
	memcpy(&btv->radio_dev,&radio_template,sizeof(radio_template));
	
        btv->id=dev->device;
	btv->bt848_adr=pci_resource_start(dev,0);
	if (pci_enable_device(dev)) {
                printk(KERN_WARNING "bttv%d: Can't enable device.\n",
		       btv->nr);
		return -EIO;
	}
        if (pci_set_dma_mask(dev, 0xffffffff)) {
                printk(KERN_WARNING "bttv%d: No suitable DMA available.\n",
		       btv->nr);
		return -EIO;
        }
	if (!request_mem_region(pci_resource_start(dev,0),
				pci_resource_len(dev,0),
				"bttv")) {
		return -EBUSY;
	}
        if (btv->id >= 878)
                btv->i2c_command = 0x83;                   
        else
                btv->i2c_command=(I2C_TIMING | BT848_I2C_SCL | BT848_I2C_SDA);

        pci_read_config_byte(dev, PCI_CLASS_REVISION, &btv->revision);
        pci_read_config_byte(dev, PCI_LATENCY_TIMER, &lat);
        printk(KERN_INFO "bttv%d: Bt%d (rev %d) at %02x:%02x.%x, ",
               bttv_num,btv->id, btv->revision, dev->bus->number,
	       PCI_SLOT(dev->devfn),PCI_FUNC(dev->devfn));
        printk("irq: %d, latency: %d, mmio: 0x%lx\n",
	       btv->dev->irq, lat, btv->bt848_adr);
	
	bttv_idcard(btv);

#if defined(__powerpc__)
        /* on OpenFirmware machines (PowerMac at least), PCI memory cycle */
        /* response on cards with no firmware is not enabled by OF */
        pci_read_config_dword(dev, PCI_COMMAND, &cmd);
        cmd = (cmd | PCI_COMMAND_MEMORY ); 
        pci_write_config_dword(dev, PCI_COMMAND, cmd);
#endif

#ifdef __sparc__
	btv->bt848_mem=(unsigned char *)btv->bt848_adr;
#else
	btv->bt848_mem=ioremap(btv->bt848_adr, 0x1000);
#endif
        
        /* clear interrupt mask */
	btwrite(0, BT848_INT_MASK);

        result = request_irq(btv->dev->irq, bttv_irq,
                             SA_SHIRQ | SA_INTERRUPT,"bttv",(void *)btv);
        if (result==-EINVAL) 
        {
                printk(KERN_ERR "bttv%d: Bad irq number or handler\n",
                       bttv_num);
		goto fail1;
        }
        if (result==-EBUSY)
        {
                printk(KERN_ERR "bttv%d: IRQ %d busy, change your PnP config in BIOS\n",bttv_num,btv->dev->irq);
		goto fail1;
        }
        if (result < 0) 
		goto fail1;
        
	if (0 != bttv_handle_chipset(btv)) {
		result = -1;
		goto fail2;
	}
	
        pci_set_master(dev);
	pci_set_drvdata(dev,btv);

	if(init_bt848(btv) < 0) {
		bttv_remove(dev);
		return -EIO;
	}
	bttv_num++;

        return 0;

 fail2:
        free_irq(btv->dev->irq,btv);
 fail1:
	release_mem_region(pci_resource_start(btv->dev,0),
			   pci_resource_len(btv->dev,0));
	return result;
}

static struct pci_device_id bttv_pci_tbl[] = {
        {PCI_VENDOR_ID_BROOKTREE, PCI_DEVICE_ID_BT848,
         PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_BROOKTREE, PCI_DEVICE_ID_BT849,
         PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_BROOKTREE, PCI_DEVICE_ID_BT878,
         PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_BROOKTREE, PCI_DEVICE_ID_BT879,
         PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
        {0,}
};

MODULE_DEVICE_TABLE(pci, bttv_pci_tbl);

static struct pci_driver bttv_pci_driver = {
        .name     = "bttv",
        .id_table = bttv_pci_tbl,
        .probe    = bttv_probe,
        .remove   = bttv_remove,
};

static int bttv_init_module(void)
{
	int rc;
	bttv_num = 0;

	printk(KERN_INFO "bttv: driver version %d.%d.%d loaded\n",
	       (BTTV_VERSION_CODE >> 16) & 0xff,
	       (BTTV_VERSION_CODE >> 8) & 0xff,
	       BTTV_VERSION_CODE & 0xff);
	if (gbuffers < 2 || gbuffers > MAX_GBUFFERS)
		gbuffers = 2;
	if (gbufsize < 0 || gbufsize > BTTV_MAX_FBUF)
		gbufsize = BTTV_MAX_FBUF;
	if (bttv_verbose)
		printk(KERN_INFO "bttv: using %d buffers with %dk (%dk total) for capture\n",
		       gbuffers,gbufsize/1024,gbuffers*gbufsize/1024);

	bttv_check_chipset();

	rc = pci_module_init(&bttv_pci_driver);
	if (-ENODEV == rc) {
		/* plenty of people trying to use bttv for the cx2388x ... */
		if (NULL != pci_find_device(0x14f1, 0x8800, NULL))
			printk("bttv doesn't support your Conexant 2388x card.\n");
	}
	return rc;
}

static void bttv_cleanup_module(void)
{
	pci_unregister_driver(&bttv_pci_driver);
	return;
}

module_init(bttv_init_module);
module_exit(bttv_cleanup_module);

/*
 * Local variables:
 * c-basic-offset: 8
 * End:
 */
