/*
    bttv - Bt848 frame grabber driver

    bttv's *private* header file  --  nobody else than bttv itself
    should ever include this file.

    Copyright (C) 1996,97 Ralph Metzler (rjkm@thp.uni-koeln.de)
    (c) 1999,2000 Gerd Knorr <kraxel@goldbach.in-berlin.de>

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

#ifndef _BTTVP_H_
#define _BTTVP_H_

#include <linux/version.h>
#define BTTV_VERSION_CODE KERNEL_VERSION(0,7,108)

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#include "bt848.h"
#include "bttv.h"
#include "audiochip.h"
#include "tuner.h"
#include "i2c-compat.h"

#ifdef __KERNEL__

/* ---------------------------------------------------------- */
/* bttv-driver.c                                              */

/* insmod options / kernel args */
extern unsigned int no_overlay;
extern unsigned int bttv_verbose;
extern unsigned int bttv_debug;
extern unsigned int bttv_gpio;
extern void bttv_gpio_tracking(struct bttv *btv, char *comment);
extern int init_bttv_i2c(struct bttv *btv);
extern int pvr_boot(struct bttv *btv);

#define dprintk		if (bttv_debug)   printk
#define vprintk		if (bttv_verbose) printk

#define BTTV_MAX 16
extern unsigned int bttv_num;			/* number of Bt848s in use */
extern struct bttv bttvs[BTTV_MAX];

#define UNSET -1U

#ifdef VIDEODAT_HACK
# define VBI_MAXLINES   19
#else
# define VBI_MAXLINES   16
#endif
#define VBIBUF_SIZE     (2048*VBI_MAXLINES*2)
#define MAX_GBUFFERS	64
#define RISCMEM_LEN	(32744*2)

#define BTTV_MAX_FBUF	0x208000

struct bttv_window 
{
	int x, y;
	ushort width, height;
	ushort bpp, bpl;
	ushort swidth, sheight;
	unsigned long vidadr;
	ushort freq;
	int norm;
	int interlace;
	int color_fmt;
	ushort depth;
};

struct bttv_pll_info {
	unsigned int pll_ifreq;	   /* PLL input frequency 	 */
	unsigned int pll_ofreq;	   /* PLL output frequency       */
	unsigned int pll_crystal;  /* Crystal used for input     */
	unsigned int pll_current;  /* Currently programmed ofreq */
};

struct bttv_gbuf {
	int stat;
#define GBUFFER_UNUSED       0
#define GBUFFER_GRABBING     1
#define GBUFFER_DONE         2
#define GBUFFER_ERROR        3
	struct timeval tv;
	
	u16 width;
	u16 height;
	u16 fmt;
	
	u32 *risc;
	unsigned long ro;
	unsigned long re;
};

struct bttv {
	struct video_device video_dev;
	struct video_device radio_dev;
	struct video_device vbi_dev;
	struct video_picture picture;		/* Current picture params */
	struct video_audio audio_dev;		/* Current audio params */

	spinlock_t s_lock;
        struct semaphore lock;
	unsigned int user;

	/* i2c */
	struct i2c_adapter         i2c_adap;
	struct i2c_algo_bit_data   i2c_algo;
	struct i2c_client          i2c_client;
	int                        i2c_state, i2c_rc;

        unsigned int tuner_type;
        unsigned int pinnacle_id;
        unsigned int channel;
	unsigned int svhs;
        
        unsigned int nr;
	unsigned short id;
	struct pci_dev *dev;
	unsigned char revision;
	unsigned long bt848_adr;      /* bus address of IO mem returned by PCI BIOS */
	unsigned char *bt848_mem;   /* pointer to mapped IO memory */
	unsigned long busriscmem; 
	u32 *riscmem;
  
	unsigned char *vbibuf;
	struct bttv_window win;
	int fb_color_ctl;
	int type;            /* card type  */
	unsigned int cardid;
	int audio;           /* audio mode */
	int audio_chip;      /* set to one of the chips supported by bttv.c */
	int radio;
	int has_radio;
	int has_remote;

	/* miro/pinnacle + Aimslab VHX
	   philips matchbox (tea5757 radio tuner) support */
	int has_matchbox;
	int mbox_we;
	int mbox_data;
	int mbox_clk;
	int mbox_most;
	int mbox_mask;

	/* ISA stuff (Terratec Active Radio Upgrade) */
	int mbox_ior;
	int mbox_iow;
	int mbox_csel;

	u32 *risc_jmp;
	u32 *vbi_odd;
	u32 *vbi_even;
	u32 bus_vbi_even;
	u32 bus_vbi_odd;
        wait_queue_head_t vbiq;
	wait_queue_head_t capq;
	unsigned int vbip;

	u32 *risc_scr_odd;
	u32 *risc_scr_even;
	u32 risc_cap_odd;
	u32 risc_cap_even;
	int scr_on;
	int vbi_on;
	struct video_clip *cliprecs;

	struct bttv_gbuf *gbuf;
	int gqueue[MAX_GBUFFERS];
	int gq_in,gq_out,gq_grab,gq_start;
        char *fbuffer;

	struct bttv_pll_info pll;
	unsigned int Fsc;
	unsigned int field;
	unsigned int last_field; /* number of last grabbed field */
	int i2c_command;
	int triton1;

	int errors;
	int needs_restart;

	wait_queue_head_t gpioq;
	int shutdown;
        void (*audio_hook)(struct bttv *btv, struct video_audio *v, int set);
};
#endif

#define btwrite(dat,adr)    writel((dat), (char *) (btv->bt848_mem+(adr)))
#define btread(adr)         readl(btv->bt848_mem+(adr))

#define btand(dat,adr)      btwrite((dat) & btread(adr), adr)
#define btor(dat,adr)       btwrite((dat) | btread(adr), adr)
#define btaor(dat,mask,adr) btwrite((dat) | ((mask) & btread(adr)), adr)

/* bttv ioctls */

#define BTTV_READEE		_IOW('v',  BASE_VIDIOCPRIVATE+0, char [256])
#define BTTV_WRITEE		_IOR('v',  BASE_VIDIOCPRIVATE+1, char [256])
#define BTTV_FIELDNR		_IOR('v' , BASE_VIDIOCPRIVATE+2, unsigned int)
#define BTTV_PLLSET		_IOW('v' , BASE_VIDIOCPRIVATE+3, struct bttv_pll_info)
#define BTTV_BURST_ON      	_IOR('v' , BASE_VIDIOCPRIVATE+4, int)
#define BTTV_BURST_OFF     	_IOR('v' , BASE_VIDIOCPRIVATE+5, int)
#define BTTV_VERSION  	        _IOR('v' , BASE_VIDIOCPRIVATE+6, int)
#define BTTV_PICNR		_IOR('v' , BASE_VIDIOCPRIVATE+7, int)
#define BTTV_VBISIZE            _IOR('v' , BASE_VIDIOCPRIVATE+8, int)

#define TDA9850            0x01
#define TDA9840            0x02
#define TDA8425            0x03
#define TEA6300            0x04

#endif /* _BTTVP_H_ */

/*
 * Local variables:
 * c-basic-offset: 8
 * End:
 */
