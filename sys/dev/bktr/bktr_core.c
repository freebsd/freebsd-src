/* BT848 1.3-ALPHA  Driver for Brooktree's Bt848 based cards.
   The Brooktree  BT848 Driver driver is based upon Mark Tinguely and
   Jim Lowe's driver for the Matrox Meteor PCI card . The 
   Philips SAA 7116 and SAA 7196 are very different chipsets than
   the BT848. For starters, the BT848 is a one chipset solution and
   it incorporates a RISC engine to control the DMA transfers --
   that is it the actual dma process is control by a program which
   resides in the hosts memory also the register definitions between
   the Philips chipsets and the Bt848 are very different.

   The original copyright notice by Mark and Jim is included mostly
   to honor their fantastic work in the Matrox Meteor driver!

      Enjoy,
      Amancio

 */

/*
 * 1. Redistributions of source code must retain the 
 * Copyright (c) 1997 Amancio Hasty
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
 *	This product includes software developed by Amancio Hasty
 * 4. The name of the author may not be used to endorse or promote products 
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.	IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */




/*
 * 1. Redistributions of source code must retain the 
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
 * DISCLAIMED.	IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*		Change History:
1.0		1/24/97	   First Alpha release

1.1		2/20/97	   Added video ioctl so we can do PCI To PCI
			   data transfers. This is for capturing data
			   directly to a vga frame buffer which has
			   a linear frame buffer. Minor code clean-up.

1.3		2/23/97	   Fixed system lock-up reported by 
			   Randall Hopper <rhh@ct.picker.com>. This
			   problem seems somehow to be exhibited only
			   in his system. I changed the setting of
			   INT_MASK for CAP_CONTINUOUS to be exactly
			   the same as CAP_SINGLE apparently setting
			   bit 23 cleared the system lock up. 
			   version 1.1 of the driver has been reported
			   to work with STB's WinTv, Hauppage's Wincast/Tv
			   and last but not least with the Intel Smart
			   Video Recorder.

1.4		3/9/97	   fsmp@freefall.org
			   Merged code to support tuners on STB and WinCast
			   cards.
			   Modifications to the contrast and chroma ioctls.
			   Textual cleanup.

1.5             3/15/97    fsmp@freefall.org
                	   new bt848 specific versions of hue/bright/
                           contrast/satu/satv.
                           Amancio's patch to fix "screen freeze" problem.

1.6             3/19/97    fsmp@freefall.org
			   new table-driven frequency lookup.
			   removed disable_intr()/enable_intr() calls from i2c.
			   misc. cleanup.

1.7             3/19/97    fsmp@freefall.org
			   added audio support submitted by:
				Michael Petry <petry@netwolf.NetMasters.com>

1.8             3/20/97    fsmp@freefall.org
			   extended audio support.
			   card auto-detection.
			   major cleanup, order of routines, declarations, etc.

1.9             3/22/97    fsmp@freefall.org
			   merged in Amancio's minor unit for tuner control
			   mods.
			   misc. cleanup, especially in the _intr routine.
			   made AUDIO_SUPPORT mainline code.

1.10            3/23/97    fsmp@freefall.org
			   added polled hardware i2c routines,
			   removed all existing software i2c routines.
			   created software i2cProbe() routine.
			   Randall Hopper's fixes of BT848_GHUE & BT848_GBRIG.
			   eeprom support.

1.11            3/24/97    fsmp@freefall.org
			   Louis Mamakos's new bt848 struct.

1.12		3/25/97    fsmp@freefall.org
			   japanese freq table from Naohiro Shichijo.
			   new table structs for tuner lookups.
			   major scrub for "magic numbers".

1.13		3/28/97    fsmp@freefall.org
			   1st PAL support.
			   MAGIC_[1-4] demarcates magic #s needing PAL work.
			   AFC code submitted by Richard Tobin
			    <richard@cogsci.ed.ac.uk>.

1.14		3/29/97    richard@cogsci.ed.ac.uk
			   PAL support: magic numbers moved into
			   format_params structure.
			   Revised AFC interface.
			   fixed DMA_PROG_ALLOC size misdefinition.
*/

#include "bktr.h"

#if NBKTR > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/mman.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /* DEVFS */
#include <machine/clock.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include "pci.h"
#if NPCI > 0

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#endif

#include <machine/ioctl_meteor.h>
#include <machine/ioctl_bt848.h>	/* extensions to ioctl_meteor.h */
#include <pci/brktree_reg.h>


#define METPRI (PZERO+8)|PCATCH

static void	bktr_intr __P((void *arg));
static		bt_enable_cnt;

/*
 * memory allocated for DMA programs
 */
#define DMA_PROG_ALLOC		(8 * PAGE_SIZE)

/* 
 * Allocate enough memory for:
 *	768x576 RGB 16 or YUV (16 storage bits/pixel) = 884736 = 216 pages
 *
 * You may override this using the options "METEOR_ALLOC_PAGES=value" in your
 * kernel configuration file.
 */
#ifndef BROOKTREE_ALLOC_PAGES
#define BROOKTREE_ALLOC_PAGES	217*4
#endif
#define BROOKTREE_ALLOC		(BROOKTREE_ALLOC_PAGES * PAGE_SIZE)

static bktr_reg_t brooktree[ NBKTR ];
#define BROOKTRE_NUM(mtr)	((bktr - &brooktree[0])/sizeof(bktr_reg_t))

#define BKTRPRI (PZERO+8)|PCATCH

static char*	bktr_probe( pcici_t tag, pcidi_t type );
static void	bktr_attach( pcici_t tag, int unit );

static u_long	bktr_count;

static struct	pci_device bktr_device = {
	"bktr",
	bktr_probe,
	bktr_attach,
	&bktr_count
};

DATA_SET (pcidevice_set, bktr_device);

static	d_open_t	bktr_open;
static	d_close_t	bktr_close;
static	d_read_t	bktr_read;
static	d_write_t	bktr_write;
static	d_ioctl_t	bktr_ioctl;
static	d_mmap_t	bktr_mmap;

#define CDEV_MAJOR 79
static struct cdevsw bktr_cdevsw = 
{
	bktr_open,	bktr_close,	bktr_read,	bktr_write,
	bktr_ioctl,	nostop,		nullreset,	nodevtotty,
	seltrue,	bktr_mmap,	NULL,		"bktr",
	NULL,		-1
};


/*
 * This is for start-up convenience only, NOT mandatory.
 */
#if !defined( DEFAULT_CHNLSET )
#define DEFAULT_CHNLSET	CHNLSET_NABCST
#endif

/*
 * Parameters describing size of transmitted image.
 */

struct format_params format_params[] = {
#define FORMAT_PARAMS_NTSC525 0
  { 525, 22, 480,  910, 135, 754, (640.0 / 780.0) },
#define FORMAT_PARAMS_PAL625 1
  { 625, 32, 576, 1135, 186, 922, (768.0 / 944.0) }
};

/* experimental code for Automatic Frequency Control */
#define TUNER_AFC
#define TEST_TUNER_AFC_NOT

#if defined( TUNER_AFC )
#define AFC_DELAY		10000	/* 10 millisend delay */
#define AFC_BITS		0x07
#define AFC_FREQ_MINUS_125	0x00
#define AFC_FREQ_MINUS_62	0x01
#define AFC_FREQ_CENTERED	0x02
#define AFC_FREQ_PLUS_62	0x03
#define AFC_FREQ_PLUS_125	0x04
#define AFC_MAX_STEP		(5 * FREQFACTOR) /* no more than 5 MHz */
#endif /* TUNER_AFC */

/*
 * i2c things:
 */

#define TEST_PAL

/* PLL on a Temic NTSC tuner: 4032FY5 */
#define TEMIC_NTSC_WADDR	0xc0
#define TEMIC_NTSC_RADDR	0xc1

/* PLL on a Temic PAL I tuner: 4062FY5 */
#define TEMIC_PALI_WADDR	0xc2
#define TEMIC_PALI_RADDR	0xc3

/* PLL on a Philips tuner */
#define PHILIPS_NTSC_WADDR	0xc6
#define PHILIPS_NTSC_RADDR	0xc7

/* guaranteed address for any TSA5522/3 (PLL on all(?) tuners) */
#define TSA552x_WADDR		0xc2
#define TSA552x_RADDR		0xc3

#define TSA552x_CB_MSB		(0x80)
#define TSA552x_CB_CP		(1<<6)
#define TSA552x_CB_T2		(1<<5)
#define TSA552x_CB_T1		(1<<4)
#define TSA552x_CB_T0		(1<<3)
#define TSA552x_CB_RSA		(1<<2)
#define TSA552x_CB_RSB		(1<<1)
#define TSA552x_CB_OS		(1<<0)


/* address of BTSC/SAP decoder chip */
#define TDA9850_WADDR		0xb6
#define TDA9850_RADDR		0xb7


/* EEProm (128 * 8) on an STB card */
#define X24C01_WADDR		0xae
#define X24C01_RADDR		0xaf


/* EEProm (256 * 8) on a Hauppauge card */
#define PFC8582_WADDR		0xa0
#define PFC8582_RADDR		0xa1


/* registers in the BTSC/dbx chip */
#define CON1ADDR		0x04
#define CON2ADDR		0x05
#define CON3ADDR		0x06
#define CON4ADDR		0x07


/* raise the charge pump voltage for fast tuning */
#define TSA552x_FCONTROL	(TSA552x_CB_MSB |	\
				 TSA552x_CB_CP  |	\
				 TSA552x_CB_T0  |	\
				 TSA552x_CB_RSA |	\
				 TSA552x_CB_RSB)

/* lower the charge pump voltage for better residual oscillator FM */
#define TSA552x_SCONTROL	(TSA552x_CB_MSB |	\
				 TSA552x_CB_T0  |	\
				 TSA552x_CB_RSA |	\
				 TSA552x_CB_RSB)

/* sync detect threshold */
#if 1
#define SYNC_LEVEL		(BT848_ADC_RESERVED |	\
				 BT848_ADC_CRUSH)	/* threshold ~125 mV */
#else
#define SYNC_LEVEL		(BT848_ADC_RESERVED |	\
				 BT848_ADC_CRUSH    |	\
				 BT848_ADC_SYNC_T)	/* threshold ~75 mV */
#endif


/* the GPIO bits that control the audio MUXes */
#define GPIO_AUDIOMUX_BITS	0x07


/* debug utility for holding previous INT_STAT contents */
#define STATUS_SUM
static u_long	status_sum = 0;

/*
 * defines to make certain bit-fiddles understandable
 */
#define FIFO_ENABLED		BT848_DMA_CTL_FIFO_EN
#define RISC_ENABLED		BT848_DMA_CTL_RISC_EN
#define FIFO_RISC_ENABLED	(BT848_DMA_CTL_FIFO_EN | BT848_DMA_CTL_RISC_EN)
#define FIFO_RISC_DISABLED	0

#define ALL_INTS_DISABLED	0
#define ALL_INTS_CLEARED	0xffffffff
#define CAPTURE_OFF		0

#define BIT_SEVEN_HIGH		(1<<7)
#define BIT_EIGHT_HIGH		(1<<8)

#define I2C_BITS		(BT848_INT_RACK | BT848_INT_I2CDONE)


/*
 * misc. support routines.
 */
static const struct CARDTYPE	cards[];
static const struct TUNER	tuners[];
static int			signCard( bktr_ptr_t bktr, int offset,
					  int count, u_char* sig );
static void			probeCard( bktr_ptr_t bktr, int verbose );

static vm_offset_t	get_bktr_mem( int unit, unsigned size );


/*
 * bt848 RISC programming routines.
 */
static int	dump_bt848( bt848_ptr_t bt848 );

static void	yuvpack_prog( bktr_ptr_t bktr, char i_flag, int cols,
			      int rows,  int interlace );
static void	yuv422_prog( bktr_ptr_t bktr, char i_flag, int cols,
			     int rows, int interlace );
static void	rgb_prog( bktr_ptr_t bktr, char i_flag, int cols,
			  int rows, int pixel_width, int interlace );
static void	build_dma_prog( bktr_ptr_t bktr, char i_flag );


/*
 * video & video capture specific routines.
 */
static int	video_open( bktr_ptr_t bktr );
static int	video_close( bktr_ptr_t bktr );
static int	video_ioctl( bktr_ptr_t bktr, int unit,
			     int cmd, caddr_t arg, struct proc* pr );

static void	start_capture( bktr_ptr_t bktr, unsigned type );
static void	set_fps( bktr_ptr_t bktr, u_short fps );


/*
 * tuner specific functions.
 */
static int	tuner_open( bktr_ptr_t bktr );
static int	tuner_close( bktr_ptr_t bktr );
static int	tuner_ioctl( bktr_ptr_t bktr, int unit,
			     int cmd, caddr_t arg, struct proc* pr );

static int	tv_channel( bktr_ptr_t bktr, int channel );
static int	tv_freq( bktr_ptr_t bktr, int frequency );
#if defined( TUNER_AFC )
static int	do_afc( bktr_ptr_t bktr, int addr, int frequency );
#endif /* TUNER_AFC */


/*
 * audio specific functions.
 */
static int	set_audio( bktr_ptr_t bktr, int mode );
static void	temp_mute( bktr_ptr_t bktr, int flag );
static int	set_BTSC( bktr_ptr_t bktr, int control );


/*
 * ioctls common to both video & tuner.
 */
static int	common_ioctl( bktr_ptr_t bktr, bt848_ptr_t bt848,
			      int cmd, caddr_t arg );


/*
 * i2c primitives
 */
static int	i2cWrite( bktr_ptr_t bktr, int addr, int byte1, int byte2 );
static int	i2cRead( bktr_ptr_t bktr, int addr );
static int	writeEEProm( bktr_ptr_t bktr, int offset, int count,
			     u_char* data );
static int	readEEProm( bktr_ptr_t bktr, int offset, int count,
			    u_char* data );


/*
 * the boot time probe routine.
 */
static char*
bktr_probe( pcici_t tag, pcidi_t type )
{
	switch (type) {
	case BROOKTREE_848_ID:
		return("BrookTree 848");
	};

	return ((char *)0);
}


/*
 * what should we do here?
 */
static void
bktr_init ( bktr_ptr_t bktr )
{
	return;
}


/*
 * the attach routine.
 */
static	void
bktr_attach( pcici_t tag, int unit )
{
	bktr_ptr_t	bktr;
	bt848_ptr_t	bt848;
#ifdef BROOKTREE_IRQ
	u_long		old_irq, new_irq;
#endif 
	u_char		*test;
	vm_offset_t	buf;
	u_long		latency;
	u_long		fun;

	bt_enable_cnt = 0;
	bktr = &brooktree[unit];

	if (unit >= NBKTR) {
		printf("brooktree%d: attach: only %d units configured.\n",
		        unit, NBKTR);
		printf("brooktree%d: attach: invalid unit number.\n", unit);
		return;
	}

	bktr->tag = tag;
	pci_map_mem( tag, PCI_MAP_REG_START, (vm_offset_t *) &bktr->base,
		     &bktr->phys_base );

	fun = pci_conf_read(tag, PCI_COMMAND_STATUS_REG);

#ifdef BROOKTREE_IRQ		/* from the configuration file */
	old_irq = pci_conf_read(tag, PCI_INTERRUPT_REG);
	pci_conf_write(tag, PCI_INTERRUPT_REG, BROOKTREE_IRQ);
	new_irq = pci_conf_read(tag, PCI_INTERRUPT_REG);
	printf("bktr%d: attach: irq changed from %d to %d\n",
		unit, (old_irq & 0xff), (new_irq & 0xff));
#endif 
	/* setup the interrupt handling routine */
	pci_map_int(tag, bktr_intr, (void*) bktr, &net_imask);
	
/*
 * PCI latency timer.  32 is a good value for 4 bus mastering slots, if
 * you have more than for, then 16 would probably be a better value.
 */
#ifndef BROOKTREE_DEF_LATENCY_VALUE
#define BROOKTREE_DEF_LATENCY_VALUE	10
#endif
	latency = pci_conf_read(tag, PCI_LATENCY_TIMER);
	latency = (latency >> 8) & 0xff;
	if ( bootverbose ) {
		if (latency)
			printf("brooktree%d: PCI bus latency is", unit);
		else
			printf("brooktree%d: PCI bus latency was 0 changing to",
				unit);
	}
	if ( !latency ) {
		latency = BROOKTREE_DEF_LATENCY_VALUE;
		pci_conf_write(tag, PCI_LATENCY_TIMER,	latency<<8);
	}
	if ( bootverbose ) {
		printf(" %d.\n", latency);
	}

	/*	bktr_init(bktr);	 set up the bt848 */

	/* allocate space for dma program */
	bktr->dma_prog = get_bktr_mem(unit, DMA_PROG_ALLOC);
	bktr->odd_dma_prog = get_bktr_mem(unit, DMA_PROG_ALLOC);

	/* allocate space for pixel buffer */
	if ( BROOKTREE_ALLOC )
		buf = get_bktr_mem(unit, BROOKTREE_ALLOC);
	else
		buf = 0;

	if ( bootverbose ) {
		printf("bktr%d: buffer size %d, addr 0x%x\n",
			unit, BROOKTREE_ALLOC, vtophys(buf));
	}

	bktr->bigbuf = buf;
	bktr->alloc_pages = BROOKTREE_ALLOC_PAGES;
	if ( buf != 0 ) {
		bzero((caddr_t) buf, BROOKTREE_ALLOC);
		buf = vtophys(buf);

#ifdef amancio					/* 640x480 RGB 16 */

amancio : setup dma risc program 
		bktr->base->dma1e = buf;
		bktr->base->dma1o = buf + 0x500;
		bktr->base->dma_end_e = 
		bktr->base->dma_end_o = buf + METEOR_ALLOC;
end of setup up dma risc program
	/* 1 frame of 640x480 RGB 16 */
	bktr->flags |= METEOR_INITALIZED | METEOR_AUTOMODE | METEOR_DEV0 |
		   METEOR_RGB16;
#endif /* amancio */

		bktr->flags = METEOR_INITALIZED | METEOR_AUTOMODE |
			      METEOR_DEV0 | METEOR_RGB16;
		bktr->dma_prog_loaded = FALSE;
		bktr->cols = 640;
		bktr->rows = 480;
		bktr->depth = 2;		/* two bytes per pixel */
		bktr->frames = 1;		/* one frame */
		bt848 = bktr->base;
		bt848->int_mask = ALL_INTS_DISABLED;
		bt848->gpio_dma_ctl = FIFO_RISC_DISABLED;
	}

	/* defaults for the tuner section of the card */
	bktr->tflags = TUNER_INITALIZED;
	bktr->tuner.frequency = 0;
	bktr->tuner.channel = 0;
	bktr->tuner.chnlset = DEFAULT_CHNLSET;
	bktr->audio_mux_select = 0;
	bktr->audio_mute_state = FALSE;

	probeCard( bktr, TRUE );

#ifdef DEVFS
	bktr->devfs_token = devfs_add_devswf(&bktr_cdevsw, unit,
					     DV_CHR, 0, 0, 0644, "brooktree");
#endif /* DEVFS */
}


/*
 * interrupt handling routine complete bktr_read() if using interrupts.
 */
static void
bktr_intr( void *arg )
{ 
	bktr_ptr_t		bktr;
	bt848_ptr_t		bt848;
	u_long			bktr_status;
	u_char			dstatus;

	bktr = (bktr_ptr_t) arg;
	bt848 = bktr->base;

	/*
	 * check to see if any interrupts are unmasked on this device.  If
	 * none are, then we likely got here by way of being on a PCI shared
	 * interrupt dispatch list.
	 */
	if (bt848->int_mask == ALL_INTS_DISABLED)
	  	return;		/* bail out now, before we do something we
				   shouldn't */

	if (!(bktr->flags & METEOR_OPEN)) {
		bt848->gpio_dma_ctl = FIFO_RISC_DISABLED;
		bt848->int_mask = ALL_INTS_DISABLED;
		/* return; ?? */
	}

	/* record and clear the INTerrupt status bits */
	bktr_status = bt848->int_stat;
	bt848->int_stat = bktr_status & ~I2C_BITS;	/* don't touch i2c */

	/* record and clear the device status register */
	dstatus = bt848->dstatus;
	bt848->dstatus = 0x00;

#if defined( STATUS_SUM )
	/* add any new device status or INTerrupt status bits */
	status_sum |= (bktr_status & ~(BT848_INT_RSV0|BT848_INT_RSV1));
	status_sum |= ((dstatus & (BT848_DSTATUS_COF|BT848_DSTATUS_LOF)) << 6);
#endif /* STATUS_SUM */

/**
	printf( " STATUS %x %x %x \n",
		dstatus, bktr_status, bt848->risc_count );
 */

	/* if risc was disabled re-start process again */
	if ( !(bktr_status & BT848_INT_RISC_EN) ||
	     ((bktr_status & (BT848_INT_FBUS   |
			      BT848_INT_FTRGT  |
			      BT848_INT_FDSR   |
			      BT848_INT_PPERR  |
			      BT848_INT_RIPERR |
			      BT848_INT_PABORT |
			      BT848_INT_OCERR  |
			      BT848_INT_SCERR)) != 0) ) {

/* XXX isn't this redundant ??? */
		bt848->int_stat = bt848->int_stat & ~I2C_BITS;

		bt848->gpio_dma_ctl = FIFO_RISC_DISABLED;

		bt848->int_mask = ALL_INTS_DISABLED;

		bt848->risc_strt_add = vtophys(bktr->dma_prog);

		bt848->gpio_dma_ctl = FIFO_ENABLED;
		bt848->gpio_dma_ctl = bktr->capcontrol;

		bt848->int_mask = BT848_INT_MYSTERYBIT |
				  BT848_INT_RISCI      |
				  BT848_INT_VSYNC      |
				  BT848_INT_FMTCHG;

		bt848->cap_ctl = bktr->bktr_cap_ctl;

		return;
	}

	if (!(bktr_status & BT848_INT_RISCI))
		return;
/**
	printf( "intr status %x %x %x\n",
		bktr_status, dstatus, bt848->risc_count );
 */

	/*
	 * Disable future interrupts if a capture mode is not selected.
	 * This can happen when we are in the process of closing or 
	 * changing capture modes, otherwise it shouldn't happen.
	 */
	if (!(bktr->flags & METEOR_CAP_MASK))
		bt848->cap_ctl = CAPTURE_OFF;

	/*
	 * If we have a complete frame.
	 */
	if (!(bktr->flags & METEOR_WANT_MASK)) {
		bktr->frames_captured++;
		/*
		 * post the completion time. 
		 */
		if (bktr->flags & METEOR_WANT_TS) {
			struct timeval *ts;
			
			if ((u_int) bktr->alloc_pages * PAGE_SIZE
			   <= (bktr->frame_size + sizeof(struct timeval))) {
				ts =(struct timeval *)bktr->bigbuf +
				  bktr->frame_size;
				/* doesn't work in synch mode except
				 *  for first frame */
				/* XXX */
				microtime(ts);
			}
		}

		/*
		 * Wake up the user in single capture mode.
		 */
		if (bktr->flags & METEOR_SINGLE) {

			if (!(bktr_status & BT848_INT_FIELD))
				return;

			/* stop dma */
			bt848->int_mask = ALL_INTS_DISABLED;

			/* disable risc, leave fifo running */
			bt848->gpio_dma_ctl = FIFO_ENABLED;
			wakeup((caddr_t)bktr);
		}

		/*
		 * If the user requested to be notified via signal,
		 * let them know the frame is complete.
		 */
		if (bktr->proc && !(bktr->signal & METEOR_SIG_MODE_MASK))
			psignal( bktr->proc,
				 bktr->signal&(~METEOR_SIG_MODE_MASK) );

		/*
		 * Reset the want flags if in continuous or
		 * synchronous capture mode.
		 */
		if (bktr->flags & (METEOR_CONTIN | METEOR_SYNCAP)) {
			switch(bktr->flags & METEOR_ONLY_FIELDS_MASK) {
			case METEOR_ONLY_ODD_FIELDS:
				bktr->flags |= METEOR_WANT_ODD;
				break;
			case METEOR_ONLY_EVEN_FIELDS:
				bktr->flags |= METEOR_WANT_EVEN;
				break;
			default:
				bktr->flags |= METEOR_WANT_MASK;
				break;
			}
		}
	}

	return;
}


/*---------------------------------------------------------
**
**	BrookTree 848 character device driver routines
**
**---------------------------------------------------------
*/


#define UNIT(x)		((x) & 0x0f)
#define MINOR(x)	((x >> 4) & 0x0f)
#define VIDEO_DEV	0x00
#define TUNER_DEV	0x01

/*
 * 
 */
int
bktr_open( dev_t dev, int flags, int fmt, struct proc *p )
{
	bktr_ptr_t	bktr;
	int		unit;

	unit = UNIT( minor(dev) );
	if (unit >= NBKTR)			/* unit out of range */
		return( ENXIO );

	bktr = &(brooktree[ unit ]);

	if (!(bktr->flags & METEOR_INITALIZED)) /* device not found */
		return( ENXIO );	

	switch ( MINOR( minor(dev) ) ) {
	case VIDEO_DEV:
		return( video_open( bktr ) );

	case TUNER_DEV:
		return( tuner_open( bktr ) );
	}

	return( ENXIO );
}


/*
 * 
 */
static int
video_open( bktr_ptr_t bktr )
{
	bt848_ptr_t bt848;

	if (bktr->flags & METEOR_OPEN)		/* device is busy */
		return( EBUSY );

	bktr->flags |= METEOR_OPEN;

	bt848 = bktr->base;

/**
	dump_bt848( bt848 );
 */

	bt848->dstatus = 0x00;			/* clear device status reg. */

	bt848->adc = SYNC_LEVEL;

	bt848->iform = BT848_IFORM_M_MUX1 |
		       BT848_IFORM_X_XT0  |
		       BT848_IFORM_F_NTSCM;
	bktr->flags = (bktr->flags & ~METEOR_DEV_MASK) | METEOR_DEV0;
	bktr->format_params = FORMAT_PARAMS_NTSC525;

	bt848->color_ctl = BT848_COLOR_CTL_RGB_DED;

	bt848->e_hscale_lo = 170;
	bt848->o_hscale_lo = 170;

	bt848->e_delay_lo = 0x72;
	bt848->o_delay_lo = 0x72;
	bt848->e_scloop = 0;
	bt848->o_scloop = 0;

	bt848->vbi_pack_size = 0;
	bt848->vbi_pack_del = 0;

	bzero((u_char *) bktr->bigbuf, 640*480*4);
	bktr->fifo_errors = 0;
	bktr->dma_errors = 0;
	bktr->frames_captured = 0;
	bktr->even_fields_captured = 0;
	bktr->odd_fields_captured = 0;
	bktr->proc = (struct proc *)0;
	set_fps(bktr, 30);
	bktr->video.addr = 0;
	bktr->video.width = 0;
	bktr->video.banksize = 0;
	bktr->video.ramsize = 0;

	bt848->int_mask = BT848_INT_MYSTERYBIT;	/* what does this bit do ??? */

	return( 0 );
}


/*
 * 
 */
static int
tuner_open( bktr_ptr_t bktr )
{
	if ( !(bktr->tflags & TUNER_INITALIZED) )	/* device not found */
		return( ENXIO );	

	if ( bktr->tflags & TUNER_OPEN )		/* already open */
		return( 0 );

	bktr->tflags |= TUNER_OPEN;

	/* enable drivers on the GPIO port that control the MUXes */
	bktr->base->gpio_out_en = GPIO_AUDIOMUX_BITS;

	/* unmure the audio stream */
	set_audio( bktr, AUDIO_UNMUTE );

	/* enable stereo if appropriate */
	if ( bktr->card.dbx )
		set_BTSC( bktr, 0 );

	return( 0 );
}


/*
 * 
 */
int
bktr_close( dev_t dev, int flags, int fmt, struct proc *p )
{
	bktr_ptr_t	bktr;
	int		unit;

	unit = UNIT( minor(dev) );
	if (unit >= NBKTR)			/* unit out of range */
		return( ENXIO );

	bktr = &(brooktree[ unit ]);

	switch ( MINOR( minor(dev) ) ) {
	case VIDEO_DEV:
		return( video_close( bktr ) );

	case TUNER_DEV:
		return( tuner_close( bktr ) );
	}

	return( ENXIO );
}


/*
 * 
 */
static int
video_close( bktr_ptr_t bktr )
{
	bt848_ptr_t	bt848;

	bktr->flags &= ~(METEOR_OPEN     |
			 METEOR_SINGLE   |
			 METEOR_CAP_MASK |
			 METEOR_WANT_MASK);

	bt848 = bktr->base;
	bt848->gpio_dma_ctl = FIFO_RISC_DISABLED;
	bt848->cap_ctl = CAPTURE_OFF;

	bktr->dma_prog_loaded = FALSE;
	bt848->tdec = 0;
	bt848->int_mask = ALL_INTS_DISABLED;

/** FIXME: is 0xf magic, wouldn't 0x00 work ??? */
	bt848->sreset = 0xf;
	bt848->int_stat = ALL_INTS_CLEARED;

	return( 0 );
}


/*
 * tuner close handle,
 *  place holder for tuner specific operations on a close.
 */
static int
tuner_close( bktr_ptr_t bktr )
{
	bktr->tflags &= ~TUNER_OPEN;

	/* mute the audio by switching the mux */
	set_audio( bktr, AUDIO_MUTE );

	/* disable drivers on the GPIO port that control the MUXes */
	bktr->base->gpio_out_en = 0;

	return( 0 );
}


/*
 * 
 */
int
bktr_read( dev_t dev, struct uio *uio, int ioflag )
{
	bktr_ptr_t	bktr;
	bt848_ptr_t	bt848;
	int		unit;
	int		status;
	int		count;
	
	if (minor(dev) > 0)
		return( ENXIO );

	unit = UNIT(minor(dev));
	if (unit >= NBKTR)	/* unit out of range */
		return( ENXIO );

	bktr = &(brooktree[unit]);
	if (bktr->bigbuf == 0)	/* no frame buffer allocated (ioctl failed) */
		return( ENOMEM );

	if (bktr->flags & METEOR_CAP_MASK)
		return( EIO );	/* already capturing */

	count = bktr->rows * bktr->cols * bktr->depth;
	if ((int) uio->uio_iov->iov_len < count)
		return( EINVAL );

	bktr->flags &= ~(METEOR_CAP_MASK | METEOR_WANT_MASK);

	/* Start capture */
	bt848 = bktr->base;
	bt848->gpio_dma_ctl = FIFO_ENABLED;
	bt848->gpio_dma_ctl = FIFO_RISC_ENABLED;

	status = tsleep((caddr_t)bktr, METPRI, "captur", 0);
	if (!status)		/* successful capture */
		status = uiomove((caddr_t)bktr->bigbuf, count, uio);
	else
		printf ("meteor%d: read: tsleep error %d\n", unit, status);

	bktr->flags &= ~(METEOR_SINGLE | METEOR_WANT_MASK);

	return( status );
}


/*
 * 
 */
int
bktr_write( dev_t dev, struct uio *uio, int ioflag )
{
	return( 0 );
}


/*
 * 
 */
int
bktr_ioctl( dev_t dev, int cmd, caddr_t arg, int flag, struct proc* pr )
{
	bktr_ptr_t	bktr;
	int		unit;

	unit = UNIT(minor(dev));
	if (unit >= NBKTR)	/* unit out of range */
		return( ENXIO );

	bktr = &(brooktree[ unit ]);

	if (bktr->bigbuf == 0)	/* no frame buffer allocated (ioctl failed) */
		return( ENOMEM );

	switch ( MINOR( minor(dev) ) ) {
	case VIDEO_DEV:
		return( video_ioctl( bktr, unit, cmd, arg, pr ) );

	case TUNER_DEV:
		return( tuner_ioctl( bktr, unit, cmd, arg, pr ) );
	}

	return( ENXIO );
}

/*
 * video ioctls
 */
static int
video_ioctl( bktr_ptr_t bktr, int unit, int cmd, caddr_t arg, struct proc* pr )
{
	int			status;
	int			count;
	int			tmp_int;
	bt848_ptr_t		bt848;
	volatile u_char		c_temp;
	volatile u_short	s_temp;
	unsigned int		temp, temp1;
	unsigned int		error;
	struct meteor_geomet	*geo;
	struct meteor_counts	*cnt;
	struct meteor_video	*video;
	vm_offset_t		buf;
	struct format_params	*fp;

	bt848 =	bktr->base;

	switch ( cmd ) {

	case METEORSTATUS:	/* get Bt848 status */
		c_temp = bt848->dstatus;
		temp = 0;
		if (!(c_temp & 0x40)) temp |= METEOR_STATUS_HCLK;
		if (!(c_temp & 0x10)) temp |= METEOR_STATUS_FIDT;
		*(u_short *)arg = temp;
		break;

	case METEORSFMT:	/* set input format */
		switch(*(unsigned long *)arg & METEOR_FORM_MASK ) {
		case 0:		/* default */
		case METEOR_FMT_NTSC:
			bktr->flags = (bktr->flags & ~METEOR_FORM_MASK) |
				METEOR_NTSC;
			bt848->iform &= ~BT848_IFORM_FORMAT;
			bt848->iform |= BT848_IFORM_F_NTSCM;
			bt848->adelay = 0x68;
			bt848->bdelay = 0x5d;
			bktr->format_params = FORMAT_PARAMS_NTSC525;
			break;

		case METEOR_FMT_PAL:
			bktr->flags = (bktr->flags & ~METEOR_FORM_MASK) |
				METEOR_PAL;
			bt848->iform &= ~BT848_IFORM_FORMAT;
			bt848->iform |= BT848_IFORM_F_PALBDGHI;
			bt848->adelay = 0x7f;
			bt848->bdelay = 0x72;
			bktr->format_params = FORMAT_PARAMS_PAL625;
			break;

		case METEOR_FMT_AUTOMODE:
			bktr->flags = (bktr->flags & ~METEOR_FORM_MASK) |
				METEOR_AUTOMODE;
			bt848->iform &= ~BT848_IFORM_FORMAT;
			break;

		default:
			return( EINVAL );
		}
		break;

	case METEORGFMT:	/* get input format */
		*(u_long *)arg = bktr->flags & METEOR_FORM_MASK;
		break;

	case METEORSCOUNT:	/* (re)set error counts */
		cnt = (struct meteor_counts *) arg;
		bktr->fifo_errors = cnt->fifo_errors;
		bktr->dma_errors = cnt->dma_errors;
		bktr->frames_captured = cnt->frames_captured;
		bktr->even_fields_captured = cnt->even_fields_captured;
		bktr->odd_fields_captured = cnt->odd_fields_captured;
		break;

	case METEORGCOUNT:	/* get error counts */
		cnt = (struct meteor_counts *) arg;
		cnt->fifo_errors = bktr->fifo_errors;
		cnt->dma_errors = bktr->dma_errors;
		cnt->frames_captured = bktr->frames_captured;
		cnt->even_fields_captured = bktr->even_fields_captured;
		cnt->odd_fields_captured = bktr->odd_fields_captured;
		break;

	case METEORGVIDEO:
		video = (struct meteor_video *)arg;
		video->addr = bktr->video.addr;
		video->width = bktr->video.width;
		video->banksize = bktr->video.banksize;
		video->ramsize = bktr->video.ramsize;
		break;

	case METEORSVIDEO:
		video = (struct meteor_video *)arg;
		bktr->video.addr = video->addr;
		bktr->video.width = video->width;
		bktr->video.banksize = video->banksize;
		bktr->video.ramsize = video->ramsize;
		break;

	case METEORSFPS:
		set_fps(bktr, *(u_short *)arg);
		break;

	case METEORGFPS:
		*(u_short *)arg = bktr->fps;
		break;

	case METEORSHUE:	/* set hue */
		bt848->hue = (*(u_char *) arg) & 0xff;
		break;

	case METEORGHUE:	/* get hue */
		*(u_char *)arg = bt848->hue;
		break;

	case METEORSBRIG:	/* set brightness */
		bt848->bright = *(u_char *)arg & 0xff;
		break;

	case METEORGBRIG:	/* get brightness */
		*(u_char *)arg = bt848->bright;
		break;

	case METEORSCSAT:	/* set chroma saturation */
		temp = (int)*(u_char *)arg;

		bt848->sat_u_lo = bt848->sat_v_lo = (temp << 1) & 0xff;

		bt848->e_control &= ~(BT848_E_CONTROL_SAT_U_MSB |
				      BT848_E_CONTROL_SAT_V_MSB);
		bt848->o_control &= ~(BT848_O_CONTROL_SAT_U_MSB |
				      BT848_O_CONTROL_SAT_V_MSB);

		if ( temp & BIT_SEVEN_HIGH ) {
			bt848->e_control |= (BT848_E_CONTROL_SAT_U_MSB |
					     BT848_E_CONTROL_SAT_V_MSB);
			bt848->o_control |= (BT848_O_CONTROL_SAT_U_MSB |
					     BT848_O_CONTROL_SAT_V_MSB);
		}
		break;

	case METEORGCSAT:	/* get chroma saturation */
		temp = (bt848->sat_v_lo >> 1) & 0xff;
		if ( bt848->e_control & BT848_E_CONTROL_SAT_V_MSB )
			temp |= BIT_SEVEN_HIGH;
		*(u_char *)arg = (u_char)temp;
		break;

	case METEORSCONT:	/* set contrast */
		temp = (int)*(u_char *)arg & 0xff;
		temp <<= 1;
		bt848->contrast_lo =  temp & 0xff;
		bt848->e_control &= ~BT848_E_CONTROL_CON_MSB;
		bt848->o_control &= ~BT848_O_CONTROL_CON_MSB;
		bt848->e_control |=
			((temp & 0x100) >> 6 ) & BT848_E_CONTROL_CON_MSB;
		bt848->o_control |=
			((temp & 0x100) >> 6 ) & BT848_O_CONTROL_CON_MSB;
		break;

	case METEORGCONT:	/* get contrast */
		temp = (int)bt848->contrast_lo & 0xff;
		temp |= ((int)bt848->o_control & 0x04) << 6;
		*(u_char *)arg = (u_char)((temp >> 1) & 0xff);
		break;

	case METEORSSIGNAL:
		bktr->signal = *(int *) arg;
		bktr->proc = pr;
		break;

	case METEORGSIGNAL:
		*(int *)arg = bktr->signal;
		break;

	case METEORCAPTUR:
		temp = bktr->flags;
		switch (*(int *) arg) {
		case METEOR_CAP_SINGLE:

			if (bktr->bigbuf==0)	/* no frame buffer allocated */
				return( ENOMEM );

			/* already capturing */
/* XXX:
			if (temp & METEOR_CAP_MASK)
				return( EIO );
 */

			start_capture(bktr, METEOR_SINGLE);
			bktr->flags |= METEOR_SINGLE;
			bktr->flags &= ~METEOR_WANT_MASK;

			/* wait for capture to complete */
			bt848->int_stat = ALL_INTS_CLEARED;
			bt848->gpio_dma_ctl = FIFO_ENABLED;
			bt848->gpio_dma_ctl = bktr->capcontrol;

			bt848->int_mask = BT848_INT_MYSTERYBIT |
					  BT848_INT_RISCI      |
					  BT848_INT_VSYNC      |
					  BT848_INT_FMTCHG;

			error = tsleep((caddr_t)bktr, METPRI, "captur", hz);
			if (error) {
				printf( "bktr%d: ioctl: tsleep error %d %x\n",
					unit, error, bt848->risc_count);
			}

			bktr->flags &= ~(METEOR_SINGLE|METEOR_WANT_MASK);
			/* FIXME: should we set bt848->int_stat ??? */
			break;

		case METEOR_CAP_CONTINOUS:
			if (bktr->bigbuf==0)	/* no frame buffer allocated */
				return( ENOMEM );

			if (temp & METEOR_CAP_MASK)
				return( EIO );	/* already capturing */

			start_capture(bktr, METEOR_CONTIN);
			bt848->int_stat = bt848->int_stat;

			bt848->gpio_dma_ctl = FIFO_ENABLED;
			bt848->gpio_dma_ctl = bktr->capcontrol;

			bt848->int_mask = BT848_INT_MYSTERYBIT |
					  BT848_INT_RISCI      |
					  BT848_INT_VSYNC      |
					  BT848_INT_FMTCHG;
/**
			dump_bt848( bt848 );
 */
			break;
		
		case METEOR_CAP_STOP_CONT:
			if (bktr->flags & METEOR_CONTIN) {
				/* turn off capture */
				bt848->int_mask = ALL_INTS_DISABLED;
				bt848->gpio_dma_ctl = FIFO_RISC_DISABLED;
				bktr->flags &=
					~(METEOR_CONTIN | METEOR_WANT_MASK);
			}
		}
		break;

	case METEORSETGEO:
		geo = (struct meteor_geomet *) arg;
		fp = &format_params[bktr->format_params];

		error = 0;
		/* Either even or odd, if even & odd, then these a zero */
		if ((geo->oformat & METEOR_GEO_ODD_ONLY) &&
			(geo->oformat & METEOR_GEO_EVEN_ONLY)) {
			printf( "bktr%d: ioctl: Geometry odd or even only.\n",
				unit);
			return( EINVAL );
		}

		/* set/clear even/odd flags */
		if (geo->oformat & METEOR_GEO_ODD_ONLY)
			bktr->flags |= METEOR_ONLY_ODD_FIELDS;
		else
			bktr->flags &= ~METEOR_ONLY_ODD_FIELDS;
		if (geo->oformat & METEOR_GEO_EVEN_ONLY)
			bktr->flags |= METEOR_ONLY_EVEN_FIELDS;
		else
			bktr->flags &= ~METEOR_ONLY_EVEN_FIELDS;

		/* can't change parameters while capturing */
/* XXX:
		if (bktr->flags & METEOR_CAP_MASK)
			return( EBUSY );
 */
		if ((geo->columns & 0x3fe) != geo->columns) {
			printf(
			"bktr%d: ioctl: %d: columns too large or not even.\n",
				unit, geo->columns);
			error = EINVAL;
		}
		if (((geo->rows & 0x7fe) != geo->rows) ||
			((geo->oformat & METEOR_GEO_FIELD_MASK) &&
				((geo->rows & 0x3fe) != geo->rows)) ) {
			printf(
			"bktr%d: ioctl: %d: rows too large or not even.\n",
				unit, geo->rows);
			error = EINVAL;
		}
		if (geo->frames > 32) {
			printf("bktr%d: ioctl: too many frames.\n", unit);

			error = EINVAL;
		}

		if (error)
			return( error );

		bktr->dma_prog_loaded = FALSE;
		bt848->gpio_dma_ctl = FIFO_RISC_DISABLED;

		bt848->int_mask = ALL_INTS_DISABLED;

		if (temp=geo->rows * geo->columns * geo->frames * 2) {
			if (geo->oformat & METEOR_GEO_RGB24) temp = temp * 2;

			/* meteor_mem structure for SYNC Capture */
			if (geo->frames > 1) temp += PAGE_SIZE;

			temp = btoc(temp);
			if ((int) temp > bktr->alloc_pages
			    && bktr->video.addr == 0) {
				buf = get_bktr_mem(unit, temp*PAGE_SIZE);
				if (buf != 0) {
					kmem_free(kernel_map, bktr->bigbuf,
					  (bktr->alloc_pages * PAGE_SIZE));
					bktr->bigbuf = buf;
					bktr->alloc_pages = temp;
					if (bootverbose)
						printf(
				"meteor%d: ioctl: Allocating %d bytes\n",
							unit, temp*PAGE_SIZE);
				}
				else
					error = ENOMEM;
			}
		}

		if (error)
			return error;

		bktr->rows = geo->rows;
		bktr->cols = geo->columns;
		bktr->frames = geo->frames;

		/* horizontal scale */
		temp = ((fp->htotal*fp->hactive_frac) / bktr->cols - 1) * 4096;

		bt848->e_hscale_lo = temp & 0xff;
		bt848->o_hscale_lo = temp & 0xff;
		bt848->e_hscale_hi = (temp >> 8) & 0xff;
		bt848->o_hscale_hi = (temp >> 8) & 0xff;

		/* horizontal active */
		temp = bktr->cols;
		bt848->e_hactive_lo = temp & 0xff;
		bt848->o_hactive_lo = temp & 0xff;
		bt848->e_crop &= ~0x3;
		bt848->o_crop  &= ~0x3;
		bt848->e_crop |= (temp >> 8) & 0x3;
		bt848->o_crop  |= (temp >> 8) & 0x3;

		/* horizontal delay */
		temp = ((float)fp->hdelay / fp->hactive) * bktr->cols;
		temp = temp + 2; /* why?? - rmt */
		temp = temp & 0x3fe;
		bt848->e_delay_lo = temp & 0xff;
		bt848->o_delay_lo = temp & 0xff;
		bt848->e_crop &= ~0xc;
		bt848->o_crop &= ~0xc;
		bt848->e_crop |= (temp >> 6) & 0xc;
		bt848->o_crop |= (temp >> 6) & 0xc;

		/* vertical scale */
		if (geo->oformat & METEOR_GEO_ODD_ONLY ||
		    geo->oformat & METEOR_GEO_EVEN_ONLY)
		        tmp_int = 65536.0 -
			  ((float)fp->vactive / 2 / bktr->rows - 1) * 512.0;
		else
		        tmp_int = 65536.0 -
			  ((float)fp->vactive / bktr->rows - 1) * 512.0;

		tmp_int &= 0x1fff;

		bt848->e_vscale_lo = tmp_int & 0xff;
		bt848->o_vscale_lo = tmp_int & 0xff;
		bt848->e_vscale_hi &= ~0x1f;
		bt848->o_vscale_hi &= ~0x1f;
		bt848->e_vscale_hi |= (tmp_int >> 8) & 0x1f;
		bt848->o_vscale_hi |= (tmp_int >> 8) & 0x1f;

		/* vertical active */
		bt848->e_crop &= ~0x30;
		bt848->e_crop |= (fp->vactive >> 4) & 0x30;
		bt848->e_vactive_lo = fp->vactive & 0xff;
		bt848->o_crop &= ~0x30;
		bt848->o_crop |= (fp->vactive >> 4) & 0x30;
		bt848->o_vactive_lo = fp->vactive & 0xff;

		/* vertical delay */
		bt848->e_vdelay_lo = fp->vdelay;
		bt848->o_vdelay_lo = fp->vdelay;

		bktr->format = METEOR_GEO_YUV_422;
		switch (geo->oformat & METEOR_GEO_OUTPUT_MASK) {
		case 0:			/* default */
		case METEOR_GEO_RGB16:
			bktr->format = METEOR_GEO_RGB16;
			bktr->depth = 2;
			break;
		case METEOR_GEO_RGB24:
			bktr->format = METEOR_GEO_RGB24;
			bktr->depth = 4;
			break;
		case METEOR_GEO_YUV_422:
			bktr->format = METEOR_GEO_YUV_422;
			break;
		case METEOR_GEO_YUV_PACKED:
			bktr->format = METEOR_GEO_YUV_PACKED;
			break;
		}

/*
		if (geo->oformat & METEOR_GEO_YUV_12 )
			bktr->format |= METEOR_GEO_YUV_12;
		else if (geo->oformat & METEOR_GEO_YUV_9 )
			bktr->format |= METEOR_GEO_YUV_9;
 */

		if (bktr->flags & METEOR_CAP_MASK) {

			if (bktr->flags & (METEOR_CONTIN|METEOR_SYNCAP)) {
				switch(bktr->flags & METEOR_ONLY_FIELDS_MASK) {
				case METEOR_ONLY_ODD_FIELDS:
					bktr->flags |= METEOR_WANT_ODD;
					break;
				case METEOR_ONLY_EVEN_FIELDS:
					bktr->flags |= METEOR_WANT_EVEN;
					break;
				default:
					bktr->flags |= METEOR_WANT_MASK;
					break;
				}

				start_capture(bktr, METEOR_CONTIN);
				bt848->int_stat = bt848->int_stat;
				bt848->gpio_dma_ctl = FIFO_ENABLED;
				bt848->gpio_dma_ctl = bktr->capcontrol;
				bt848->int_mask = BT848_INT_MYSTERYBIT |
						  BT848_INT_VSYNC      |
						  BT848_INT_FMTCHG;
			}
		}
		break;
	/* end of METEORSETGEO */

	default:
		return common_ioctl( bktr, bt848, cmd, arg );
	}

	return( 0 );
}


/*
 * tuner ioctls
 */
static int
tuner_ioctl( bktr_ptr_t bktr, int unit, int cmd, caddr_t arg, struct proc* pr )
{
	bt848_ptr_t	bt848;
	int		status;
	int		tmp_int;
	unsigned int	temp, temp1;
	int		offset;
	int		count;
	u_char		*buf;

	bt848 =	bktr->base;

	switch ( cmd ) {

#if defined( TUNER_AFC )
	case TVTUNER_SETAFC:
		bktr->tuner.afc = (*(int *)arg != 0);
		break;

	case TVTUNER_GETAFC:
		*(int *)arg = bktr->tuner.afc;
		/* XXX Perhaps use another bit to indicate AFC success? */
		break;
#endif /* TUNER_AFC */

	case TVTUNER_SETCHNL:
		temp_mute( bktr, TRUE );
		temp = tv_channel( bktr, (int)*(unsigned long *)arg );
		temp_mute( bktr, FALSE );
		if ( temp < 0 )
			return( EINVAL );
		*(unsigned long *)arg = temp;
		break;

	case TVTUNER_GETCHNL:
		*(unsigned long *)arg = bktr->tuner.channel;
		break;

	case TVTUNER_SETTYPE:
		temp = *(unsigned long *)arg;
		if ( (temp < CHNLSET_MIN) || (temp > CHNLSET_MAX) )
			return( EINVAL );
		bktr->tuner.chnlset = temp;
		break;

	case TVTUNER_GETTYPE:
		*(unsigned long *)arg = bktr->tuner.chnlset;
		break;

	case TVTUNER_GETSTATUS:
		temp = i2cRead( bktr, TSA552x_RADDR );
		*(unsigned long *)arg = temp & 0xff;
		break;

	case TVTUNER_SETFREQ:
		temp_mute( bktr, TRUE );
		temp = tv_freq( bktr, (int)*(unsigned long *)arg );
		temp_mute( bktr, FALSE );
		if ( temp < 0 )
			return( EINVAL );
		*(unsigned long *)arg = temp;
		break;

	case TVTUNER_GETFREQ:
		*(unsigned long *)arg = bktr->tuner.frequency;
		break;

	case BT848_SAUDIO:	/* set audio channel */
		if ( set_audio( bktr, *(int*)arg ) < 0 )
			return( EIO );
		break;

	/* hue is a 2's compliment number, -90' to +89.3' in 0.7' steps */
	case BT848_SHUE:	/* set hue */
		bt848->hue = (u_char)(*(int*)arg & 0xff);
		break;

	case BT848_GHUE:	/* get hue */
		*(int*)arg = (signed char)(bt848->hue & 0xff);
		break;

	/* brightness is a 2's compliment #, -50 to +%49.6% in 0.39% steps */
	case BT848_SBRIG:	/* set brightness */
		bt848->bright = (u_char)(*(int *)arg & 0xff);
		break;

	case BT848_GBRIG:	/* get brightness */
		*(int *)arg = (signed char)(bt848->bright & 0xff);
		break;

	/*  */
	case BT848_SCSAT:	/* set chroma saturation */
		tmp_int = *(int*)arg;

		temp = bt848->e_control;
		temp1 = bt848->o_control;
		if ( tmp_int & BIT_EIGHT_HIGH ) {
			temp |= (BT848_E_CONTROL_SAT_U_MSB |
				 BT848_E_CONTROL_SAT_V_MSB);
			temp1 |= (BT848_O_CONTROL_SAT_U_MSB |
				  BT848_O_CONTROL_SAT_V_MSB);
		}
		else {
			temp &= ~(BT848_E_CONTROL_SAT_U_MSB |
				  BT848_E_CONTROL_SAT_V_MSB);
			temp1 &= ~(BT848_O_CONTROL_SAT_U_MSB |
				   BT848_O_CONTROL_SAT_V_MSB);
		}

		bt848->sat_u_lo = (u_char)(tmp_int & 0xff);
		bt848->sat_v_lo = (u_char)(tmp_int & 0xff);
		bt848->e_control = temp;
		bt848->o_control = temp1;
		break;

	case BT848_GCSAT:	/* get chroma saturation */
		tmp_int = (int)(bt848->sat_v_lo & 0xff);
		if ( bt848->e_control & BT848_E_CONTROL_SAT_V_MSB )
			tmp_int |= BIT_EIGHT_HIGH;
		*(int*)arg = tmp_int;
		break;

	/*  */
	case BT848_SVSAT:	/* set chroma V saturation */
		tmp_int = *(int*)arg;

		temp = bt848->e_control;
		temp1 = bt848->o_control;
		if ( tmp_int & BIT_EIGHT_HIGH) {
			temp |= BT848_E_CONTROL_SAT_V_MSB;
			temp1 |= BT848_O_CONTROL_SAT_V_MSB;
		}
		else {
			temp &= ~BT848_E_CONTROL_SAT_V_MSB;
			temp1 &= ~BT848_O_CONTROL_SAT_V_MSB;
		}

		bt848->sat_v_lo = (u_char)(tmp_int & 0xff);
		bt848->e_control = temp;
		bt848->o_control = temp1;
		break;

	case BT848_GVSAT:	/* get chroma V saturation */
		tmp_int = (int)bt848->sat_v_lo & 0xff;
		if ( bt848->e_control & BT848_E_CONTROL_SAT_V_MSB )
			tmp_int |= BIT_EIGHT_HIGH;
		*(int*)arg = tmp_int;
		break;

	/*  */
	case BT848_SUSAT:	/* set chroma U saturation */
		tmp_int = *(int*)arg;

		temp = bt848->e_control;
		temp1 = bt848->o_control;
		if ( tmp_int & BIT_EIGHT_HIGH ) {
			temp |= BT848_E_CONTROL_SAT_U_MSB;
			temp1 |= BT848_O_CONTROL_SAT_U_MSB;
		}
		else {
			temp &= ~BT848_E_CONTROL_SAT_U_MSB;
			temp1 &= ~BT848_O_CONTROL_SAT_U_MSB;
		}

		bt848->sat_u_lo = (u_char)(tmp_int & 0xff);
		bt848->e_control = temp;
		bt848->o_control = temp1;
		break;

	case BT848_GUSAT:	/* get chroma U saturation */
		tmp_int = (int)bt848->sat_u_lo & 0xff;
		if ( bt848->e_control & BT848_E_CONTROL_SAT_U_MSB )
			tmp_int |= BIT_EIGHT_HIGH;
		*(int*)arg = tmp_int;
		break;

	/*  */
	case BT848_SCONT:	/* set contrast */
		tmp_int = *(int*)arg;

		temp = bt848->e_control;
		temp1 = bt848->o_control;
		if ( tmp_int & BIT_EIGHT_HIGH ) {
			temp |= BT848_E_CONTROL_CON_MSB;
			temp1 |= BT848_O_CONTROL_CON_MSB;
		}
		else {
			temp &= ~BT848_E_CONTROL_CON_MSB;
			temp1 &= ~BT848_O_CONTROL_CON_MSB;
		}

		bt848->contrast_lo = (u_char)(tmp_int & 0xff);
		bt848->e_control = temp;
		bt848->o_control = temp1;
		break;

	case BT848_GCONT:	/* get contrast */
		tmp_int = (int)bt848->contrast_lo & 0xff;
		if ( bt848->e_control & BT848_E_CONTROL_CON_MSB )
			tmp_int |= BIT_EIGHT_HIGH;
		*(int*)arg = tmp_int;
		break;

	case BT848_SCBARS:	/* set colorbar output */
		bt848->color_ctl |= BT848_COLOR_CTL_COLOR_BARS;
		break;

	case BT848_CCBARS:	/* clear colorbar output */
		bt848->color_ctl &= ~BT848_COLOR_CTL_COLOR_BARS;
		break;

	case BT848_GAUDIO:	/* get audio channel */
		temp = bktr->audio_mux_select;
		if ( bktr->audio_mute_state == TRUE )
			temp |= AUDIO_MUTE;
		*(int*)arg = temp;
		break;

	case BT848_SBTSC:	/* set audio channel */
		if ( set_BTSC( bktr, *(int*)arg ) < 0 )
			return( EIO );
		break;

	case BT848_WEEPROM:	/* write eeprom */
		offset = (((struct eeProm *)arg)->offset);
		count = (((struct eeProm *)arg)->count);
		buf = &(((struct eeProm *)arg)->bytes[ 0 ]);
		if ( writeEEProm( bktr, offset, count, buf ) < 0 )
			return( EIO );
		break;

	case BT848_REEPROM:	/* read eeprom */
		offset = (((struct eeProm *)arg)->offset);
		count = (((struct eeProm *)arg)->count);
		buf = &(((struct eeProm *)arg)->bytes[ 0 ]);
		if ( readEEProm( bktr, offset, count, buf ) < 0 )
			return( EIO );
		break;

	case BT848_SIGNATURE:
		offset = (((struct eeProm *)arg)->offset);
		count = (((struct eeProm *)arg)->count);
		buf = &(((struct eeProm *)arg)->bytes[ 0 ]);
		if ( signCard( bktr, offset, count, buf ) < 0 )
			return( EIO );
		break;

	default:
		return common_ioctl( bktr, bt848, cmd, arg );
	}

	return( 0 );
}


/*
 * common ioctls
 */
int
common_ioctl( bktr_ptr_t bktr, bt848_ptr_t bt848, int cmd, caddr_t arg )
{
	int		status;
	unsigned int	temp;

	switch (cmd) {

	case METEORSINPUT:	/* set input device */
		switch(*(unsigned long *)arg & METEOR_DEV_MASK) {

		/* this is the RCA video input */
		case 0:		/* default */
		case METEOR_INPUT_DEV0:
			bktr->flags = (bktr->flags & ~METEOR_DEV_MASK)
				| METEOR_DEV0;
			bt848->iform &= ~BT848_IFORM_MUXSEL;
			bt848->iform |= BT848_IFORM_M_MUX1;
			bt848->e_control &= ~BT848_E_CONTROL_COMP;
			bt848->o_control &= ~BT848_O_CONTROL_COMP;
			set_audio( bktr, AUDIO_EXTERN );
			break;

		/* this is the tuner input */
		case METEOR_INPUT_DEV1:
			bktr->flags = (bktr->flags & ~METEOR_DEV_MASK)
				| METEOR_DEV1;
			bt848->iform &= ~BT848_IFORM_MUXSEL;
			bt848->iform |= BT848_IFORM_M_MUX0;
			bt848->e_control &= ~BT848_E_CONTROL_COMP;
			bt848->o_control &= ~BT848_O_CONTROL_COMP;
			set_audio( bktr, AUDIO_TUNER );
			break;

		/* this is the S-VHS input */
		case METEOR_INPUT_DEV2:
		case METEOR_INPUT_DEV_SVIDEO:
			bktr->flags = (bktr->flags & ~METEOR_DEV_MASK)
				| METEOR_DEV2;
			bt848->iform &= ~BT848_IFORM_MUXSEL;
			bt848->iform |= BT848_IFORM_M_MUX2;
			bt848->e_control |= BT848_E_CONTROL_COMP;
			bt848->o_control |= BT848_O_CONTROL_COMP;
			set_audio( bktr, AUDIO_EXTERN );
			break;

		default:
			return( EINVAL );
		}
		break;

	case METEORGINPUT:	/* get input device */
		*(u_long *)arg = bktr->flags & METEOR_DEV_MASK;
		break;

#if defined( STATUS_SUM )
	case BT848_GSTATUS:	/* reap status */
		disable_intr();
		temp = status_sum;
		status_sum = 0;
		enable_intr();
		*(u_int*)arg = temp;
		break;
#endif /* STATUS_SUM */

	default:
		return( ENODEV );
	}

	return( 0 );
}


/*
 * 
 */
int
bktr_mmap( dev_t dev, int offset, int nprot )
{
	int		unit;
	bktr_ptr_t	bktr;

	unit = UNIT(minor(dev));

	if (unit >= NBKTR || minor(dev) > 0 )	/* could this happen here? */
		return( -1 );

	bktr = &(brooktree[ unit ]);

	if (nprot & PROT_EXEC)
		return( -1 );

	if (offset >= bktr->alloc_pages * PAGE_SIZE)
		return( -1 );

	return( i386_btop(vtophys(bktr->bigbuf) + offset) );
}


/******************************************************************************
 * bt848 RISC programming routines:
 */


/*
 * 
 */
static int
dump_bt848( bt848_ptr_t bt848 )
{
	volatile u_char *bt848r = (u_char *)bt848;
	int	r[60]={
			   4,    8, 0xc, 0x8c, 0x10, 0x90, 0x14, 0x94, 
			0x18, 0x98, 0x1c, 0x9c, 0x20, 0xa0, 0x24, 0xa4,
			0x28, 0x2c, 0xac, 0x30, 0x34, 0x38, 0x3c, 0x40,
			0xc0, 0x48, 0x4c, 0xcc, 0x50, 0xd0, 0xd4, 0x60,
			0x64, 0x68, 0x6c, 0xec, 0xd8, 0xdc, 0xe0, 0xe4,
			0,	 0,    0,    0
		   };
	int	i;

	for (i = 0; i < 40; i+=4) {
		printf(" Reg:value : \t%x:%x \t%x:%x \t %x:%x \t %x:%x\n",
		       r[i], bt848r[r[i]],
		       r[i+1], bt848r[r[i+1]],
		       r[i+2], bt848r[r[i+2]],
		       r[i+3], bt848r[r[i+3]]);
	}

	printf(" INT STAT %x \n",  bt848->int_stat);
	printf(" Reg INT_MASK %x \n",  bt848->int_mask);
	printf(" Reg GPIO_DMA_CTL %x \n", bt848->gpio_dma_ctl);

	return( 0 );
}


/*
 * build write instruction
 */
#define BKTR_FM1      0x6
#define BKTR_FM3      0xe
#define BKTR_VRE      0x4
#define BKTR_VRO      0xC
#define BKTR_PXV      0x0
#define BKTR_EOL      0x1
#define BKTR_SOL      0x2

#define OP_WRITE      (0x1 << 28)
#define OP_WRITEC     (0x5 << 28)
#define OP_JUMP	      (0x7 << 28)
#define OP_SYNC	      (0x8 << 28)
#define OP_WRITE123   (0x9 << 28)
#define OP_WRITES123  (0xb << 28)
#define OP_SOL	      (1 << 27)
#define OP_EOL	      (1 << 26)

static void
rgb_prog( bktr_ptr_t bktr, char i_flag, int cols,
	  int rows, int pixel_width, int interlace )
{
	int			i;
	int  			byte_count;
	bt848_ptr_t		bt848;
	volatile unsigned int	inst;
	volatile unsigned int	inst2;
	volatile unsigned int	inst3;
	volatile u_long		target_buffer, buffer;
	volatile u_long		pitch;
	volatile  u_long	*dma_prog, *t_test;
	int			b, c;

	bt848 = bktr->base;

	/* color format : rgb32 */
	if (bktr->depth == 4)
		bt848->color_fmt = 0;
	else
		bt848->color_fmt = 0x33;

/* FIXME: why set colorbars for 1 instant ??? */
	bt848->color_ctl = BT848_COLOR_CTL_COLOR_BARS;
/* FIXME: undone 4 lines later, why bother ??? */
	bt848->color_ctl = BT848_COLOR_CTL_GAMMA;

#if 0
/* FIXME: any reason to keep these lines around ??? */
	bt848->e_vdelay_low = 0x1C;
	bt848->o_vdelay_low = 0x1C;
#endif

	bt848->vbi_pack_size = 0;	    
	bt848->vbi_pack_del = 0;

	bt848->adc = SYNC_LEVEL;
	bt848->color_ctl = BT848_COLOR_CTL_RGB_DED;

	bt848->e_vscale_hi |= 0xc0;
	bt848->o_vscale_hi |= 0xc0;

	bktr->capcontrol = 3 << 2 |  3;

	dma_prog = (u_long *) bktr->dma_prog;

	/* Construct Write */
	bt_enable_cnt = 0;

	b = (cols * pixel_width ) / 2;

	/* write, sol, eol */
	inst = OP_WRITE	 |  OP_SOL  |  bt_enable_cnt << 12 |  (b);
	inst2 = OP_WRITE |  bt_enable_cnt << 12 |  (cols * pixel_width/2); 
	/* write , sol, eol */
	inst3 = OP_WRITE |  OP_EOL  |  bt_enable_cnt << 12 |  (b);

	if (bktr->video.addr) {
		target_buffer = (u_long) bktr->video.addr;
		pitch = bktr->video.width;
	}
	else {
		target_buffer = (u_long) vtophys(bktr->bigbuf);
		pitch = cols*pixel_width;
	}

	buffer = target_buffer;

	/* contruct sync : for video packet format */
	*dma_prog++ = OP_SYNC | 0xC << 24 | 1 << 15 | BKTR_FM1;

	/* sync, mode indicator packed data */
	*dma_prog++ = 0;  /* NULL WORD */

	for (i = 0; i < (rows/interlace); i++) {
		*dma_prog++ = inst;
		*dma_prog++ = target_buffer;
		*dma_prog++ = inst3;
		*dma_prog++ = target_buffer + b;
		target_buffer += interlace*pitch;
	}

	switch (i_flag) {
	case 1:
		/* sync vre */
		*dma_prog++ = OP_SYNC | 0xC << 24 | 1 << 24 | BKTR_VRE;
		*dma_prog++ = 0;  /* NULL WORD */

		*dma_prog++ = OP_JUMP	| 0xC << 24;
		*dma_prog++ = (u_long ) vtophys(bktr->dma_prog);
		return;

	case 2:
		/* sync vre */
		*dma_prog++ = OP_SYNC | 1 << 24 | 1 << 20 | BKTR_VRO;
		*dma_prog++ = 0;  /* NULL WORD */

		*dma_prog++ = OP_JUMP;
		*dma_prog++ = (u_long ) vtophys(bktr->dma_prog);
		return;

	case 3:
		/* sync vre */
		*dma_prog++ = OP_SYNC | 0xC << 24 | 1 << 24 | 1 << 15 | BKTR_VRO;
		*dma_prog++ = 0;  /* NULL WORD */
		*dma_prog++ = OP_JUMP | 0xc << 24 ;
		*dma_prog = (u_long ) vtophys(bktr->odd_dma_prog);
		break;
	}

	if (interlace == 2) {

		target_buffer =	 (u_long) buffer + pitch;

		dma_prog = (u_long *) bktr->odd_dma_prog;

		/* sync vre IRQ bit */
		*dma_prog++ = OP_SYNC | 0xc << 24 | 1 << 15 | BKTR_FM1;
		*dma_prog++ = 0;  /* NULL WORD */

		for (i = 0; i < (rows/interlace); i++) {
			*dma_prog++ = inst;
			*dma_prog++ = target_buffer;
			*dma_prog++ = inst3;
			*dma_prog++ = target_buffer + b;
			target_buffer += interlace * pitch;
		}
	}

	/* sync vre IRQ bit */
	*dma_prog++ = OP_SYNC | 0xc << 24 | 1 << 24 | 1 << 15 | BKTR_VRE;
	*dma_prog++ = 0;  /* NULL WORD */
	*dma_prog++ = OP_JUMP | 0xc << 24;
	*dma_prog++ = (u_long ) vtophys(bktr->dma_prog) ;
	*dma_prog++ = 0;  /* NULL WORD */
}


/*
 * 
 */
static void
yuvpack_prog( bktr_ptr_t bktr, char i_flag,
	      int cols, int rows, int interlace )
{
	int			i;
	int  			byte_count;
	volatile unsigned int	inst;
	volatile unsigned int	inst2;
	volatile unsigned int	inst3;
	volatile u_long		target_buffer, buffer;
	bt848_ptr_t		bt848;
	volatile  u_long	*dma_prog;
	int			b;

	bt848 = bktr->base;

	/* color format : yuvpack */
	bt848->color_fmt = 0x44;

	bt848->e_scloop |= BT848_E_SCLOOP_CAGC; /* enable chroma comb */
	bt848->o_scloop |= BT848_O_SCLOOP_CAGC;

	bt848->color_ctl = BT848_COLOR_CTL_RGB_DED |
			   BT848_COLOR_CTL_GAMMA;
	bt848->adc = SYNC_LEVEL;

	bktr->capcontrol =   1 << 6 | 1 << 4 | 1 << 2 | 3;
	bktr->capcontrol =   1 << 5 | 1 << 4 | 1 << 2 | 3;

	dma_prog = (u_long *) bktr->dma_prog;

	/* Construct Write */
	bt_enable_cnt = 0;
    
	/* write , sol, eol */
	inst = OP_WRITE	 | OP_SOL | 0xf << 16 | bt_enable_cnt << 12 | (cols*2);
	/* write , sol, eol */
	inst3 = OP_WRITE | OP_EOL | 0xf << 16 | bt_enable_cnt << 12 | (cols);
	inst2 = OP_WRITE | bt_enable_cnt << 12 | (cols ); 

	if (bktr->video.addr)
		target_buffer = (u_long) bktr->video.addr;
	else
		target_buffer = (u_long) vtophys(bktr->bigbuf);

	buffer = target_buffer;

	/* contruct sync : for video packet format */
	/* sync, mode indicator packed data */
	*dma_prog++ = OP_SYNC | 1 << 24 | 1 << 15 | BKTR_FM1;
	*dma_prog++ = 0;  /* NULL WORD */

	b = cols;

	for (i = 0; i < (rows/interlace); i++) {
		*dma_prog++ = inst;
		*dma_prog++ = target_buffer;
		*dma_prog++ = inst3;
		*dma_prog++ = target_buffer + b; 
		target_buffer += interlace*(cols * 2);
	}

	switch (i_flag) {
	case 1:
		/* sync vre */
		*dma_prog++ = OP_SYNC  | 1 << 24  | BKTR_VRE;
		*dma_prog++ = 0;  /* NULL WORD */

		*dma_prog++ = OP_JUMP;
		*dma_prog++ = (u_long ) vtophys(bktr->dma_prog);
		return;

	case 2:
		/* sync vre */
		*dma_prog++ = OP_SYNC  | 1 << 24 | 1 << 20 | BKTR_VRO;
		*dma_prog++ = 0;  /* NULL WORD */
		*dma_prog++ = OP_JUMP;
		*dma_prog++ = (u_long ) vtophys(bktr->dma_prog);
		return;

	case 3:
		/* sync vre */
		*dma_prog++ = OP_SYNC	| 1 << 24 |  0xf << 16 | BKTR_VRE;
		*dma_prog++ = 0;  /* NULL WORD */
		*dma_prog++ = OP_JUMP  ;
		*dma_prog = (u_long ) vtophys(bktr->odd_dma_prog);
		break;
	}

	if (interlace == 2) {

		target_buffer =	 (u_long) buffer + cols*2;

		dma_prog = (u_long * ) bktr->odd_dma_prog;

		/* sync vre */
		*dma_prog++ = OP_SYNC | 1 << 24 | 0xf << 16 | 1 << 15
				| BKTR_FM1;
		*dma_prog++ = 0;  /* NULL WORD */

		for (i = 0; i < (rows/interlace) ; i++) {
			*dma_prog++ = inst;
			*dma_prog++ = target_buffer;
			*dma_prog++ = inst3;
			*dma_prog++ = target_buffer + b;
			target_buffer += interlace * ( cols*2);
		}
	}

	/* sync vre IRQ bit */
	*dma_prog++ = OP_SYNC	| 1 << 24 |  0xf << 16 |  BKTR_VRO;
	*dma_prog++ = 0;  /* NULL WORD */
	*dma_prog++ = OP_JUMP | 0xf << 16;
	*dma_prog++ = (u_long ) vtophys(bktr->dma_prog);

	*dma_prog++ = OP_JUMP;
	*dma_prog++ = (u_long ) vtophys(bktr->dma_prog);
	*dma_prog++ = 0;  /* NULL WORD */
}


/*
 * 
 */
static void
yuv422_prog( bktr_ptr_t bktr, char i_flag,
	     int cols, int rows, int interlace ){

	int			i, j;
	int			byte_count;
	volatile unsigned int	inst;
	volatile unsigned int	inst2;
	volatile unsigned int	instskip, instskip2, instskip3;
	volatile unsigned int	inst3;
	volatile u_long		target_buffer, t1, buffer;
	bt848_ptr_t		bt848;
	volatile u_long		*dma_prog;
	int			b, b1;

	bt848 = bktr->base;
	dma_prog = (u_long *) bktr->dma_prog;

	bktr->capcontrol =   1 << 6 | 1 << 4 |	3;

	bt848->adc = SYNC_LEVEL;
	bt848->oform = 0x00;

	bt848->e_control |= BT848_E_CONTROL_LDEC; /* disable luma decimation */
	bt848->o_control |= BT848_O_CONTROL_LDEC;

	bt848->e_scloop |= BT848_O_SCLOOP_CAGC;	/* chroma agc enable */
	bt848->o_scloop |= BT848_O_SCLOOP_CAGC; 

	bt848->e_vscale_hi |= 0xc0;		/* luma comb and comb enable */
	bt848->o_vscale_hi |= 0xc0;

	bt848->color_fmt = 0x88;

	/* disable gamma correction removal */
	bt848->color_ctl = BT848_COLOR_CTL_GAMMA;

	bt_enable_cnt = 0;

	/* Construct Write */
	inst  = OP_WRITE123  | OP_SOL | OP_EOL | bt_enable_cnt << 12 | (cols); 
	inst2 = OP_WRITES123 | OP_SOL | OP_EOL | bt_enable_cnt << 12 | (cols); 

	if (bktr->video.addr)
		target_buffer = (u_long) bktr->video.addr;
	else
		target_buffer = (u_long) vtophys(bktr->bigbuf);
    
	buffer = target_buffer;

	t1 = target_buffer;

	/* contruct sync : for video packet format */
	*dma_prog++ = OP_SYNC | 0xC << 24 | 1 << 15 |	BKTR_FM3; /*sync, mode indicator packed data*/
	*dma_prog++ = 0;  /* NULL WORD */

	for (i = 0; i < (rows/interlace ); i++) {
		*dma_prog++ = inst;
		*dma_prog++ = cols/2 | cols/2 << 16;
		*dma_prog++ = target_buffer;
		*dma_prog++ = t1 + (cols*rows) + i*cols/2 * interlace;
		*dma_prog++ = t1 + (cols*rows) + (cols*rows/2) + i*cols/2 * interlace;
		target_buffer += interlace*cols;
	}

	switch (i_flag) {
	case 1:
		*dma_prog++ = OP_SYNC  | 0xC << 24 | 1 << 24 |	BKTR_VRE;  /*sync vre*/
		*dma_prog++ = 0;  /* NULL WORD */

		*dma_prog++ = OP_JUMP | 0xc << 24;
		*dma_prog++ = (u_long ) vtophys(bktr->dma_prog);
		return;

	case 2:
		*dma_prog++ = OP_SYNC  | 0xC << 24 | 1 << 24 |	BKTR_VRO;  /*sync vre*/
		*dma_prog++ = 0;  /* NULL WORD */

		*dma_prog++ = OP_JUMP;
		*dma_prog++ = (u_long ) vtophys(bktr->dma_prog);
		return;

	case 3:
		*dma_prog++ = OP_SYNC	| 0xc << 24 |  1 << 15 |  BKTR_VRO;  
		*dma_prog++ = 0;  /* NULL WORD */

		*dma_prog++ = OP_JUMP  | 0xc << 24 ;
		*dma_prog = (u_long ) vtophys(bktr->odd_dma_prog);
		break;
	}

	if (interlace == 2) {

		dma_prog = (u_long * ) bktr->odd_dma_prog;

		target_buffer  = (u_long) buffer + cols;
		t1 = target_buffer + cols/2;
		*dma_prog++ = OP_SYNC	| 0xc << 24 |  1 << 24 |   1 << 15 | BKTR_FM3; 
		*dma_prog++ = 0;  /* NULL WORD */

		for (i = 0; i < (rows/interlace ) ; i++) {
			*dma_prog++ = inst;
			*dma_prog++ = cols/2 | cols/2 << 16;
			*dma_prog++ = target_buffer;
			*dma_prog++ = t1 + (cols*rows) + i*cols/2 * interlace;
			*dma_prog++ = t1 + (cols*rows) + (cols*rows/2) + i*cols/2 * interlace;
			target_buffer += interlace*cols;
		}
	}
    
	*dma_prog++ = OP_SYNC  | 0xC << 24 | 1 << 24 |	BKTR_VRE; 
	*dma_prog++ = 0;  /* NULL WORD */
	*dma_prog++ = OP_JUMP | 0xC << 24;;
	*dma_prog++ = (u_long ) vtophys(bktr->dma_prog) ;
	*dma_prog++ = 0;  /* NULL WORD */
}


/*
 * 
 */
static void
build_dma_prog( bktr_ptr_t bktr, char i_flag )
{
	int			i;
	int			pixel_width, rows, cols, byte_count, interlace;
	volatile unsigned int	inst;
	volatile unsigned int	inst2;
	volatile unsigned int	inst3;
	volatile u_long		target_buffer;
	bt848_ptr_t		bt848;
	volatile  u_long	*dma_prog;
	int			b;

	bt848 = bktr->base;
	bt848->int_mask = ALL_INTS_DISABLED;

	/* disable FIFO & RISC, leave other bits alone */
	bt848->gpio_dma_ctl &= ~FIFO_RISC_ENABLED;

	/* capture control */
	switch (i_flag) {
	case 1:
 	        bktr->bktr_cap_ctl = bt848->cap_ctl =
			(BT848_CAP_CTL_DITH_FRAME | BT848_CAP_CTL_EVEN);
		bt848->e_vscale_hi &= ~0x20;
		bt848->o_vscale_hi &= ~0x20;
		interlace = 1;
		break;
	 case 2:
 	        bktr->bktr_cap_ctl = bt848->cap_ctl =
			(BT848_CAP_CTL_DITH_FRAME | BT848_CAP_CTL_ODD);
		bt848->e_vscale_hi &= ~0x20;
		bt848->o_vscale_hi &= ~0x20;
		interlace = 1;
		break;
	 default:
 	        bktr->bktr_cap_ctl = bt848->cap_ctl =
			(BT848_CAP_CTL_DITH_FRAME |
			 BT848_CAP_CTL_EVEN | BT848_CAP_CTL_ODD);
		bt848->e_vscale_hi |= 0x20;
		bt848->o_vscale_hi |= 0x20;
		interlace = 2;
		break;
	}

	bt848->risc_strt_add = vtophys(bktr->dma_prog);

	pixel_width = bktr->depth;
	rows = bktr->rows;
	cols = bktr->cols;

	if (bktr->format  == METEOR_GEO_RGB24 ||
	    bktr->format  == METEOR_GEO_RGB16) {
		rgb_prog(bktr, i_flag, cols, rows, pixel_width, interlace);
		return;
	}

	if (bktr->format == METEOR_GEO_YUV_422 ){
		yuv422_prog(bktr, i_flag, cols, rows, interlace);
		return;
	}

	if (bktr->format == METEOR_GEO_YUV_PACKED ){
		yuvpack_prog(bktr, i_flag, cols, rows, interlace);
		return;
	}

	return;
}


/******************************************************************************
 * video & video capture specific routines:
 */


/*
 * 
 */
static void
start_capture( bktr_ptr_t bktr, unsigned type )
{
	bt848_ptr_t		bt848;
	u_char			i_flag;

	bt848 = bktr->base;

	bt848->dstatus = 0;
	bt848->int_stat = bt848->int_stat;

	bktr->flags |= type;
	switch(bktr->flags & METEOR_ONLY_FIELDS_MASK) {
	case METEOR_ONLY_EVEN_FIELDS:
		bktr->flags |= METEOR_WANT_EVEN;
		i_flag = 1;
		break;
	case METEOR_ONLY_ODD_FIELDS:
		bktr->flags |= METEOR_WANT_ODD;
		i_flag = 2;
		break;
	default:
		bktr->flags |= METEOR_WANT_MASK;
		i_flag = 3;
		break;
	}

	if (bktr->dma_prog_loaded == FALSE) {
		build_dma_prog(bktr, i_flag);
		bktr->dma_prog_loaded = TRUE;
	}
	
/* XXX:
	switch (bktr->flags & METEOR_ONLY_FIELDS_MASK) {
	default:
		*bts_reg |= 0xb;   bts_reg never been initialized!
	}
 */

	bt848->risc_strt_add = vtophys(bktr->dma_prog);

/* XXX:
	bt848->gpio_dma_ctl = FIFO_RISC_ENABLED;
 */
}


/*
 * 
 */
static void
set_fps( bktr_ptr_t bktr, u_short fps )
{
	bt848_ptr_t	bt848;

	bt848 = bktr->base;

	bt848->gpio_dma_ctl = FIFO_RISC_DISABLED;
	bt848->int_stat = ALL_INTS_CLEARED;

	bktr->fps = fps;

	if ( fps == 30 ) {
		bt848->tdec = 0;
		return;
	} else {
		bt848->tdec = (int) (((float) fps / 30.0) * 60.0) & 0x3f;
		bt848->tdec |= 0x80;
	}

	if ( bktr->flags & METEOR_CAP_MASK ) {

		bt848->int_stat = ALL_INTS_CLEARED;	  
		bt848->risc_strt_add = vtophys(bktr->dma_prog);

		bt848->gpio_dma_ctl = FIFO_ENABLED;
		bt848->gpio_dma_ctl = bktr->capcontrol;
		bt848->int_mask = BT848_INT_MYSTERYBIT |
				  BT848_INT_RISCI      |
				  BT848_INT_VSYNC      |
				  BT848_INT_FMTCHG;
	}

	return;
}


/*
 * There is also a problem with range checking on the 7116.
 * It seems to only work for 22 bits, so the max size we can allocate
 * is 22 bits long or 4194304 bytes assuming that we put the beginning
 * of the buffer on a 2^24 bit boundary.  The range registers will use
 * the top 8 bits of the dma start registers along with the bottom 22
 * bits of the range register to determine if we go out of range.
 * This makes getting memory a real kludge.
 *
 */

#define RANGE_BOUNDARY	(1<<22)
static vm_offset_t
get_bktr_mem( int unit, unsigned size )
{
	vm_offset_t	addr = 0;

	addr = vm_page_alloc_contig(size, 0x100000, 0xffffffff, 1<<24);
	if (addr == 0)
		addr = vm_page_alloc_contig(size, 0x100000, 0xffffffff,
								PAGE_SIZE);
	if (addr == 0) {
		printf("meteor%d: Unable to allocate %d bytes of memory.\n",
			unit, size);
	}

	return( addr );
}


/******************************************************************************
 * i2c primitives:
 */

/* */
#define I2CBITTIME		(0x5<<4)	/* 5 * 0.48uS */
#define I2C_READ		0x01
#define I2C_COMMAND		(I2CBITTIME |			\
				 BT848_DATA_CTL_I2CSCL |	\
				 BT848_DATA_CTL_I2CSDA)

/*
 * 
 */
static int
i2cWrite( bktr_ptr_t bktr, int addr, int byte1, int byte2 )
{
	u_long		x;
	u_long		data;
	bt848_ptr_t	bt848;

	bt848 = bktr->base;

	/* clear status bits */
	bt848->int_stat = (BT848_INT_RACK | BT848_INT_I2CDONE);

	/* build the command datum */
	data = ((addr & 0xff) << 24) | ((byte1 & 0xff) << 16) | I2C_COMMAND;
	if ( byte2 != -1 ) {
		data |= ((byte2 & 0xff) << 8);
		data |= BT848_DATA_CTL_I2CW3B;
	}

	/* write the address and data */
	bt848->i2c_data_ctl = data;

	/* wait for completion */
	for ( x = 0x7fffffff; x; --x ) {	/* safety valve */
		if ( bt848->int_stat & BT848_INT_I2CDONE )
			break;
	}

	/* check for ACK */
	if ( !x || !(bt848->int_stat & BT848_INT_RACK) )
		return( -1 );

	/* return OK */
	return( 0 );
}


/*
 * 
 */
static int
i2cRead( bktr_ptr_t bktr, int addr )
{
	u_long		x;
	bt848_ptr_t	bt848;

	bt848 = bktr->base;

	/* clear status bits */
	bt848->int_stat = (BT848_INT_RACK | BT848_INT_I2CDONE);

	/* write the READ address */
	bt848->i2c_data_ctl = ((addr & 0xff) << 24) | I2C_COMMAND;

	/* wait for completion */
	for ( x = 0x7fffffff; x; --x ) {	/* safety valve */
		if ( bt848->int_stat & BT848_INT_I2CDONE )
			break;
	}

	/* check for ACK */
	if ( !x || !(bt848->int_stat & BT848_INT_RACK) )
		return( -1 );

	/* it was a read */
	return( (bt848->i2c_data_ctl >> 8) & 0xff );
}


#if defined( I2C_SOFTWARE_PROBE )

/*
 * we are keeping this around for any parts that we need to probe
 * but that CANNOT be probed via an i2c read.
 * this is necessary because the hardware i2c mechanism
 * cannot be programmed for 1 byte writes.
 * currently there are no known i2c parts that we need to probe
 * and that cannot be safely read.
 */
static int	i2cProbe( bktr_ptr_t bktr, int addr );
#define BITD		40
#define EXTRA_START

/*
 * probe for an I2C device at addr.
 */
static int
i2cProbe( bktr_ptr_t bktr, int addr )
{
	int		x, status;
	bt848_ptr_t	bt848;

	bt848 = bktr->base;

	/* the START */
#if defined( EXTRA_START )
	bt848->i2c_data_ctl = 1; DELAY( BITD );	/* release data */
	bt848->i2c_data_ctl = 3; DELAY( BITD );	/* release clock */
#endif /* EXTRA_START */
	bt848->i2c_data_ctl = 2; DELAY( BITD );	/* lower data */
	bt848->i2c_data_ctl = 0; DELAY( BITD );	/* lower clock */

	/* write addr */
	for ( x = 7; x >= 0; --x ) {
		if ( addr & (1<<x) ) {
			bt848->i2c_data_ctl = 1;
			DELAY( BITD );		/* assert HI data */
			bt848->i2c_data_ctl = 3;
			DELAY( BITD );		/* strobe clock */
			bt848->i2c_data_ctl = 1;
			DELAY( BITD );		/* release clock */
		}
		else {
			bt848->i2c_data_ctl = 0;
			DELAY( BITD );		/* assert LO data */
			bt848->i2c_data_ctl = 2;
			DELAY( BITD );		/* strobe clock */
			bt848->i2c_data_ctl = 0;
			DELAY( BITD );		/* release clock */
		}
	}

	/* look for an ACK */
	bt848->i2c_data_ctl = 1; DELAY( BITD );	/* float data */
	bt848->i2c_data_ctl = 3; DELAY( BITD );	/* strobe clock */
	status = bt848->i2c_data_ctl & 1;	/* read the ACK bit */
	bt848->i2c_data_ctl = 1; DELAY( BITD );	/* release clock */

	/* the STOP */
	bt848->i2c_data_ctl = 0; DELAY( BITD );	/* lower clock & data */
	bt848->i2c_data_ctl = 2; DELAY( BITD );	/* release clock */
	bt848->i2c_data_ctl = 3; DELAY( BITD );	/* release data */

	return( status );
}
#undef EXTRA_START
#undef BITD

#endif /* I2C_SOFTWARE_PROBE */


/*
 * 
 */
static int
writeEEProm( bktr_ptr_t bktr, int offset, int count, u_char *data )
{
	return( -1 );
}


/*
 * 
 */
static int
readEEProm( bktr_ptr_t bktr, int offset, int count, u_char *data )
{
	int	x;
	int	addr;
	int	max;
	int	byte;

	/* get the address of the EEProm */
	addr = (int)(bktr->card.eepromAddr & 0xff);
	if ( addr == 0 )
		return( -1 );

	max = (int)(bktr->card.eepromSize * EEPROMBLOCKSIZE);
	if ( (offset + count) > max )
		return( -1 );

	/* set the start address */
	if ( i2cWrite( bktr, addr, offset, -1 ) == -1 )
		return( -1 );

	/* the read cycle */
	for ( x = 0; x < count; ++x ) {
		if ( (byte = i2cRead( bktr, (addr | 1) )) == -1 )
			return( -1 );
		data[ x ] = byte;
	}

	return( 0 );
}


/******************************************************************************
 * card probe
 */


/*
 * the recognized cards, used as indexes of several tables.
 *
 * if probeCard() fails to detect the proper card on boot you can
 * override it by setting the following define to the card you are using:
 *
#define OVERRIDE_CARD	<card type>
 *
 * where <card type> is one of the following card defines.
 */
#define	CARD_UNKNOWN		0
#define	CARD_MIRO		1
#define	CARD_HAUPPAUGE		2
#define	CARD_STB		3
#define	CARD_INTEL		4

/*
 * the data for each type of card
 *
 * Note:
 *   these entried MUST be kept in the order defined by the CARD_XXX defines!
 */
const struct CARDTYPE cards[] = {

	/* CARD_UNKNOWN */
	{ "Unknown",				/* the 'name' */
	   NULL,				/* the tuner */
	   0,					/* dbx unknown */
	   0,					/* EEProm unknown */
	   0,					/* EEProm unknown */
	   { 0, 0, 0, 0, 0 } },

	/* CARD_MIRO */
	{ "Miro TV",				/* the 'name' */
	   NULL,				/* the tuner */
	   0,					/* dbx unknown */
	   0,					/* EEProm unknown */
	   0,					/* size unknown */
	   { 0x02, 0x01, 0x00, 0x00, 1 } },	/* XXX ??? */

	/* CARD_HAUPPAUGE */
	{ "Hauppauge WinCast/TV",		/* the 'name' */
	   NULL,				/* the tuner */
	   0,					/* dbx is optional */
	   PFC8582_WADDR,			/* EEProm type */
	   (u_char)(256 / EEPROMBLOCKSIZE),	/* 256 bytes */
	   { 0x00, 0x02, 0x01, 0x01, 1 } },	/* audio MUX values */

	/* CARD_STB */
	{ "STB TV/PCI",				/* the 'name' */
	   NULL,				/* the tuner */
	   0,					/* dbx is optional */
	   X24C01_WADDR,			/* EEProm type */
	   (u_char)(128 / EEPROMBLOCKSIZE),	/* 128 bytes */
	   { 0x00, 0x01, 0x02, 0x02, 1 } },	/* audio MUX values */

	/* CARD_INTEL */
	{ "Intel Smart Video III",		/* the 'name' */
	   NULL,				/* the tuner */
	   0,
	   0,
	   0,
	   { 0, 0, 0, 0, 0 } }
};


/*
 * the data for each type of tuner
 */

/* indexes into tuners[] */
#define NO_TUNER		0
#define TEMIC_NTSC		1
#define TEMIC_PAL		2
#define TEMIC_SECAM		3
#define PHILIPS_NTSC		4
#define PHILIPS_PAL		5
#define PHILIPS_SECAM		6
/* XXX FIXME: this list is incomplete */

/* input types */
#define TTYPE_XXX		0
#define TTYPE_NTSC		1
#define TTYPE_NTSC_J		2
#define TTYPE_PAL		3
#define TTYPE_PAL_M		4
#define TTYPE_PAL_N		5
#define TTYPE_SECAM		6

/**
struct TUNER {
	char*		name;
	u_char		type;
	u_char		pllAddr;
	u_char		pllControl;
	u_char		bandLimits[ 2 ];
	u_char		bandAddrs[ 3 ];
};
 */
const struct TUNER tuners[] = {
/* XXX FIXME: fill in the band-switch crosspoints */
	/* NO_TUNER */
	{ "<none>",				/* the 'name' */
	   TTYPE_XXX,				/* input type */
	   0x00,				/* PLL write address */
	   0x00,				/* control byte for PLL */
	   { 0x00, 0x00 },			/* band-switch crosspoints */
	   { 0x00, 0x00, 0x00 } },		/* the band-switch values */

	/* TEMIC_NTSC */
	{ "Temic NTSC",				/* the 'name' */
	   TTYPE_NTSC,				/* input type */
	   TEMIC_NTSC_WADDR,			/* PLL write address */
	   TSA552x_SCONTROL,			/* control byte for PLL */
	   { 0x00, 0x00 },			/* band-switch crosspoints */
	   { 0x02, 0x04, 0x01 } },		/* the band-switch values */

	/* TEMIC_PAL */
	{ "Temic PAL",				/* the 'name' */
	   TTYPE_PAL,				/* input type */
	   TEMIC_PALI_WADDR,			/* PLL write address */
	   TSA552x_SCONTROL,			/* control byte for PLL */
	   { 0x00, 0x00 },			/* band-switch crosspoints */
	   { 0x02, 0x04, 0x01 } },		/* the band-switch values */

	/* TEMIC_SECAM */
	{ "Temic SECAM",			/* the 'name' */
	   TTYPE_SECAM,				/* input type */
	   0x00,				/* PLL write address */
	   TSA552x_SCONTROL,			/* control byte for PLL */
	   { 0x00, 0x00 },			/* band-switch crosspoints */
	   { 0x02, 0x04, 0x01 } },		/* the band-switch values */

	/* PHILIPS_NTSC */
	{ "Philips NTSC",			/* the 'name' */
	   TTYPE_NTSC,				/* input type */
	   PHILIPS_NTSC_WADDR,			/* PLL write address */
	   TSA552x_SCONTROL,			/* control byte for PLL */
	   { 0x00, 0x00 },			/* band-switch crosspoints */
	   { 0xa0, 0x90, 0x30 } },		/* the band-switch values */

	/* PHILIPS_PAL */
	{ "Philips PAL",			/* the 'name' */
	   TTYPE_PAL,				/* input type */
	   0x00,				/* PLL write address */
	   TSA552x_SCONTROL,			/* control byte for PLL */
	   { 0x00, 0x00 },			/* band-switch crosspoints */
	   { 0xa0, 0x90, 0x30 } },		/* the band-switch values */

	/* PHILIPS_SECAM */
	{ "Philips SECAM",			/* the 'name' */
	   TTYPE_SECAM,				/* input type */
	   0x00,				/* PLL write address */
	   TSA552x_SCONTROL,			/* control byte for PLL */
	   { 0x00, 0x00 },			/* band-switch crosspoints */
	   { 0xa0, 0x90, 0x30 } }		/* the band-switch values */
};


/*
 * get a signature of the card
 * read all 128 possible i2c read addresses from 0x01 thru 0xff
 * build a bit array with a 1 bit for each i2c device that responds
 *
 * XXX FIXME: use offset & count args
 */
#define ABSENT		(-1)
static int
signCard( bktr_ptr_t bktr, int offset, int count, u_char* sig )
{
	int	x;

	for ( x = 0; x < 16; ++x )
		sig[ x ] = 0;

	for ( x = 0; x < 128; ++x ) {
		if ( i2cRead( bktr, (2 * x) + 1 ) != ABSENT ) {
			sig[ x / 8 ] |= (1 << (x % 8) );
		}
	}

	return( 0 );
}
#undef ABSENT


/*
 * determine the card brand/model
 */
#define ABSENT		(-1)
static void
probeCard( bktr_ptr_t bktr, int verbose )
{
	int	card;
	int	status;

#if defined( OVERRIDE_CARD )
	bktr->card = cards[ (card = OVERRIDE_CARD) ];
	goto checkTuner;
#endif

	/* look for a tuner */
	if ( i2cRead( bktr, TSA552x_RADDR ) == ABSENT ) {
		bktr->card = cards[ (card = CARD_INTEL) ];
		bktr->card.tuner = &tuners[ NO_TUNER ];
		goto checkDBX;
	}

	/* look for a hauppauge card */
	if ( (status = i2cRead( bktr, PFC8582_RADDR )) != ABSENT ) {
		bktr->card = cards[ (card = CARD_HAUPPAUGE) ];
		goto checkTuner;
	}

	/* look for an STB card */
	if ( (status = i2cRead( bktr, X24C01_RADDR )) != ABSENT ) {
		bktr->card = cards[ (card = CARD_STB) ];
		goto checkTuner;
	}

	/* XXX FIXME: (how do I) look for a Miro card */
	bktr->card = cards[ (card = CARD_MIRO) ];

checkTuner:
	/* differentiate type of tuner */
	if ( i2cRead( bktr, TEMIC_NTSC_RADDR ) != ABSENT ) {
		bktr->card.tuner = &tuners[ TEMIC_NTSC ];
		goto checkDBX;
	}

	if ( i2cRead( bktr, PHILIPS_NTSC_RADDR ) != ABSENT ) {
		bktr->card.tuner = &tuners[ PHILIPS_NTSC ];
		goto checkDBX;
	}
#if defined( TEST_PAL )
/** WARNING: this is a test */
	if ( card == CARD_HAUPPAUGE ) {
		if ( i2cRead( bktr, TEMIC_PALI_RADDR ) != ABSENT ) {
			bktr->card.tuner = &tuners[ TEMIC_PAL ];
			goto checkDBX;
		}
	}
#endif /* TEST_PAL */

	/* no tuner found */
	bktr->card.tuner = &tuners[ NO_TUNER ];

checkDBX:
	/* probe for BTSC (dbx) chips */
	if ( i2cRead( bktr, TDA9850_RADDR ) != ABSENT )
		bktr->card.dbx = 1;

end:
	if ( verbose ) {
		printf( "%s", bktr->card.name );
		if ( bktr->card.tuner )
			printf( ", %s tuner", bktr->card.tuner->name );
		if ( bktr->card.dbx )
			printf( ", dbx stereo" );
		printf( ".\n" );
	}
}
#undef ABSENT


/******************************************************************************
 * tuner specific routines:
 */


/* scaling factor for frequencies expressed as ints */
#define FREQFACTOR		16

/*
 * Format:
 *	entry 0:         MAX legal channel
 *	entry 1:         IF frequency
 *			 expressed as fi{mHz} * 16,
 *			 eg 45.75mHz == 45.75 * 16 = 732
 *	entry 2:         [place holder/future]
 *	entry 3:         base of channel record 0
 *	entry 3 + (x*3): base of channel record 'x'
 *	entry LAST:      NULL channel entry marking end of records
 *
 * Record:
 *	int 0:		base channel
 *	int 1:		frequency of base channel,
 *			 expressed as fb{mHz} * 16,
 *	int 2:		offset frequency between channels,
 *			 expressed as fo{mHz} * 16,
 */

/*
 * North American Broadcast Channels:
 *
 *  2:  55.25 mHz -  4:  67.25 mHz
 *  5:  77.25 mHz -  6:	 83.25 mHz
 *  7: 175.25 mHz - 13:	211.25 mHz
 * 14: 471.25 mHz - 83:	885.25 mHz
 *
 * IF freq: 45.75 mHz
 */
#define OFFSET	6.00
int	nabcst[] = {
	83,	(int)( 45.75 * FREQFACTOR),	0,
	14,	(int)(471.25 * FREQFACTOR),	(int)(OFFSET * FREQFACTOR),
	 7,	(int)(175.25 * FREQFACTOR),	(int)(OFFSET * FREQFACTOR),
	 5,	(int)( 77.25 * FREQFACTOR),	(int)(OFFSET * FREQFACTOR),
	 2,	(int)( 55.25 * FREQFACTOR),	(int)(OFFSET * FREQFACTOR),
	 0
};
#undef OFFSET

/*
 * North American Cable Channels, IRC:
 *
 *  2:  55.25 mHz -  4:  67.25 mHz
 *  5:  77.25 mHz -  6:  83.25 mHz
 *  7: 175.25 mHz - 13: 211.25 mHz
 * 14: 121.25 mHz - 22: 169.25 mHz
 * 23: 217.25 mHz - 94: 643.25 mHz
 * 95:  91.25 mHz - 99: 115.25 mHz
 *
 * IF freq: 45.75 mHz
 */
#define OFFSET	6.00
int	irccable[] = {
	99,	(int)( 45.75 * FREQFACTOR),	0,
	95,	(int)( 91.25 * FREQFACTOR),	(int)(OFFSET * FREQFACTOR),
	23,	(int)(217.25 * FREQFACTOR),	(int)(OFFSET * FREQFACTOR),
	14,	(int)(121.25 * FREQFACTOR),	(int)(OFFSET * FREQFACTOR),
	 7,	(int)(175.25 * FREQFACTOR),	(int)(OFFSET * FREQFACTOR),
	 5,	(int)( 77.25 * FREQFACTOR),	(int)(OFFSET * FREQFACTOR),
	 2,	(int)( 55.25 * FREQFACTOR),	(int)(OFFSET * FREQFACTOR),
	 0
};
#undef OFFSET

/*
 * North American Cable Channels, HRC:
 */
int	hrccable[] = {
	0, 0, 0,
	0
};

/*
 * Western European broadcast channels:
 *
 * (there are others that appear to vary between countries - rmt)
 *
 * here's the table Philips provides:
 * caution, some of the offsets don't compute...
 *
 *  1	 4525	700	N21
 * 
 *  2	 4825	700	E2
 *  3	 5525	700	E3
 *  4	 6225	700	E4
 * 
 *  5	17525	700	E5
 *  6	18225	700	E6
 *  7	18925	700	E7
 *  8	19625	700	E8
 *  9	20325	700	E9
 * 10	21025	700	E10
 * 11	21725	700	E11
 * 12	22425	700	E12
 * 
 * 13	 5375	700	ITA
 * 14	 6225	700	ITB
 * 
 * 15	 8225	700	ITC
 * 
 * 16	17525	700	ITD
 * 17	18325	700	ITE
 * 
 * 18	19225	700	ITF
 * 19	20125	700	ITG
 * 20	21025	700	ITH
 * 
 * 21	47125	800	E21
 * 22	47925	800	E22
 * 23	48725	800	E23
 * 24	49525	800	E24
 * 25	50325	800	E25
 * 26	51125	800	E26
 * 27	51925	800	E27
 * 28	52725	800	E28
 * 29	53525	800	E29
 * 30	54325	800	E30
 * 31	55125	800	E31
 * 32	55925	800	E32
 * 33	56725	800	E33
 * 34	57525	800	E34
 * 35	58325	800	E35
 * 36	59125	800	E36
 * 37	59925	800	E37
 * 38	60725	800	E38
 * 39	61525	800	E39
 * 40	62325	800	E40
 * 41	63125	800	E41
 * 42	63925	800	E42
 * 43	64725	800	E43
 * 44	65525	800	E44
 * 45	66325	800	E45
 * 46	67125	800	E46
 * 47	67925	800	E47
 * 48	68725	800	E48
 * 49	69525	800	E49
 * 50	70325	800	E50
 * 51	71125	800	E51
 * 52	71925	800	E52
 * 53	72725	800	E53
 * 54	73525	800	E54
 * 55	74325	800	E55
 * 56	75125	800	E56
 * 57	75925	800	E57
 * 58	76725	800	E58
 * 59	77525	800	E59
 * 60	78325	800	E60
 * 61	79125	800	E61
 * 62	79925	800	E62
 * 63	80725	800	E63
 * 64	81525	800	E64
 * 65	82325	800	E65
 * 66	83125	800	E66
 * 67	83925	800	E67
 * 68	84725	800	E68
 * 69	85525	800	E69
 * 
 * 70	 4575	800	IA
 * 71	 5375	800	IB
 * 72	 6175	800	IC
 * 
 * 74	 6925	700	S01
 * 75	 7625	700	S02
 * 76	 8325	700	S03
 * 
 * 80	10525	700	S1
 * 81	11225	700	S2
 * 82	11925	700	S3
 * 83	12625	700	S4
 * 84	13325	700	S5
 * 85	14025	700	S6
 * 86	14725	700	S7
 * 87	15425	700	S8
 * 88	16125	700	S9
 * 89	16825	700	S10
 * 90	23125	700	S11
 * 91	23825	700	S12
 * 92	24525	700	S13
 * 93	25225	700	S14
 * 94	25925	700	S15
 * 95	26625	700	S16
 * 96	27325	700	S17
 * 97	28025	700	S18
 * 98	28725	700	S19
 * 99	29425	700	S20
 * 
 * 100	 3890	000	IFFREQ
 * 
 */
#define OFFSET	8.00
int   weurope[] = {
	69,	(int)( 38.90 * FREQFACTOR),	0,
	21,	(int)(471.25 * FREQFACTOR),	(int)(OFFSET * FREQFACTOR),
	 0
};
#undef OFFSET

/*
 * Japanese Broadcast Channels:
 *
 *  1:  91.25MHz -  3: 103.25MHz
 *  4: 171.25MHz -  7: 189.25MHz
 *  8: 193.25MHz - 12: 217.25MHz  (VHF)
 * 13: 471.25MHz - 62: 765.25MHz  (UHF)
 *
 * IF freq: 45.75 mHz
 *  OR
 * IF freq: 58.75 mHz
 */
#define OFFSET  6.00
#define IF_FREQ 45.75
int     jpnbcst[] = {
	62,     (int)(IF_FREQ * FREQFACTOR),    0,
	13,     (int)(471.25 * FREQFACTOR),     (int)(OFFSET * FREQFACTOR),
	 8,     (int)(193.25 * FREQFACTOR),     (int)(OFFSET * FREQFACTOR),
	 4,     (int)(171.25 * FREQFACTOR),     (int)(OFFSET * FREQFACTOR),
	 1,     (int)( 91.25 * FREQFACTOR),     (int)(OFFSET * FREQFACTOR),
	 0
};
#undef IF_FREQ
#undef OFFSET


int* freqTable[] = {
	NULL,
	nabcst,
	irccable,
	hrccable,
	weurope,
	jpnbcst
};


#define TBL_CHNL	freqTable[ bktr->tuner.chnlset ][ x ]
#define TBL_BASE_FREQ	freqTable[ bktr->tuner.chnlset ][ x + 1 ]
#define TBL_OFFSET	freqTable[ bktr->tuner.chnlset ][ x + 2 ]
static int
frequency_lookup( bktr_ptr_t bktr, int channel )
{
	int	x;

	/* check for "> MAX channel" */
	x = 0;
	if ( channel > TBL_CHNL )
		return( -1 );

	/* search the table for data */
	for ( x = 3; TBL_CHNL; x += 3 ) {
		if ( channel >= TBL_CHNL ) {
			return( TBL_BASE_FREQ +
				 ((channel - TBL_CHNL) * TBL_OFFSET) );
		}
	}

	/* not found, must be below the MIN channel */
	return( -1 );
}
#undef TBL_OFFSET
#undef TBL_BASE_FREQ
#undef TBL_CHNL


#define TBL_IF	freqTable[ bktr->tuner.chnlset ][ 1 ]
/*
 * set the frequency of the tuner
 */
static int
tv_freq( bktr_ptr_t bktr, int frequency )
{
	const struct TUNER*	tuner;
	u_char			addr;
	u_char			control;
	u_char			band;
	int			N;
	int			order;

	tuner = bktr->card.tuner;
	if ( tuner == NULL )
		return( -1 );

	/*
	 * select the band based on frequency
	 * XXX FIXME: get the cross-over points from the tuner struct
	 */
	if ( frequency < (160 * FREQFACTOR) )
		band = tuner->bandAddrs[ 0 ];
	else if ( frequency < (454 * FREQFACTOR) )
		band = tuner->bandAddrs[ 1 ];
	else
		band = tuner->bandAddrs[ 2 ];

	/* set the address of the PLL */
	addr = tuner->pllAddr;
	control = tuner->pllControl;

	/*
	 * N = 16 * { fRF(pc) + fIF(pc) }
	 * where:
	 *  pc is picture carrier, fRF & fIF are in mHz
	 *
	 * frequency was passed in as mHz * 16
	 */
#if defined( TEST_TUNER_AFC )
	if ( bktr->tuner.afc )
		frequency -= 4;
#endif
	N = frequency + TBL_IF;

	if ( frequency > bktr->tuner.frequency ) {
		i2cWrite( bktr, addr, (N>>8) & 0x7f, N & 0xff );
		i2cWrite( bktr, addr, control, band );
	}
	else {
		i2cWrite( bktr, addr, control, band );
		i2cWrite( bktr, addr, (N>>8) & 0x7f, N & 0xff );
	}

#if defined( TUNER_AFC )
	if ( bktr->tuner.afc == TRUE ) {
		if ( (N = do_afc( bktr, addr, N )) < 0 ) {
		    /* AFC failed, restore requested frequency */
		    N = frequency + TBL_IF;
		    i2cWrite( bktr, addr, (N>>8) & 0x7f, N & 0xff );
		}
		else
		    frequency = N - TBL_IF;
	}
#endif /* TUNER_AFC */

	/* update frequency */
	bktr->tuner.frequency = frequency;

	return( 0 );
}

#if defined( TUNER_AFC )
/*
 * 
 */
static int
do_afc( bktr_ptr_t bktr, int addr, int frequency )
{
	int step;
	int status;
	int origFrequency;

	origFrequency = frequency;

	/* wait for first setting to take effect */
	tsleep( (caddr_t)bktr, PZERO, "tuning", hz/8 );

	if ( (status = i2cRead( bktr, addr + 1 )) < 0 )
		return( -1 );

#if defined( TEST_TUNER_AFC )
 printf( "\nOriginal freq: %d, status: 0x%02x\n", frequency, status );
#endif
	for ( step = 0; step < AFC_MAX_STEP; ++step ) {
		if ( (status = i2cRead( bktr, addr + 1 )) < 0 )
			goto fubar;
		if ( !(status & 0x40) ) {
#if defined( TEST_TUNER_AFC )
 printf( "no lock!\n" );
#endif
			goto fubar;
		}

		switch( status & AFC_BITS ) {
		case AFC_FREQ_CENTERED:
#if defined( TEST_TUNER_AFC )
 printf( "Centered, freq: %d, status: 0x%02x\n", frequency, status );
#endif
			return( frequency );

		case AFC_FREQ_MINUS_125:
		case AFC_FREQ_MINUS_62:
#if defined( TEST_TUNER_AFC )
 printf( "Low, freq: %d, status: 0x%02x\n", frequency, status );
#endif
			--frequency;
			break;

		case AFC_FREQ_PLUS_62:
		case AFC_FREQ_PLUS_125:
#if defined( TEST_TUNER_AFC )
 printf( "Hi, freq: %d, status: 0x%02x\n", frequency, status );
#endif
			++frequency;
			break;
		}

		i2cWrite( bktr, addr,
			  (frequency>>8) & 0x7f, frequency & 0xff );
		DELAY( AFC_DELAY );
	}

 fubar:
	i2cWrite( bktr, addr,
		  (origFrequency>>8) & 0x7f, origFrequency & 0xff );

	return( -1 );
}
#endif /* TUNER_AFC */
#undef TBL_IF


/*
 * set the channel of the tuner
 */
static int
tv_channel( bktr_ptr_t bktr, int channel )
{
	int frequency;

	/* calculate the frequency according to tuner type */
	if ( (frequency = frequency_lookup( bktr, channel )) < 0 )
		return( -1 );

	/* set the new frequency */
	if ( tv_freq( bktr, frequency ) < 0 )
		return( -1 );

	/* OK to update records */
	return( (bktr->tuner.channel = channel) );
}


/******************************************************************************
 * audio specific routines:
 */


/*
 * 
 */
#define AUDIOMUX_DISCOVER_NOT
static int
set_audio( bktr_ptr_t bktr, int cmd )
{
	bt848_ptr_t	bt848;
	u_long		temp;
	volatile u_char	idx;

#if defined( AUDIOMUX_DISCOVER )
	if ( cmd >= 200 )
		cmd -= 200;
	else
#endif /* AUDIOMUX_DISCOVER */

	/* check for existance of audio MUXes */
	if ( !bktr->card.audiomuxs[ 4 ] )
		return( -1 );

	switch (cmd) {
	case AUDIO_TUNER:
		bktr->audio_mux_select = 0;
		break;
	case AUDIO_EXTERN:
		bktr->audio_mux_select = 1;
		break;
	case AUDIO_INTERN:
		bktr->audio_mux_select = 2;
		break;
	case AUDIO_MUTE:
		bktr->audio_mute_state = TRUE;	/* set mute */
		break;
	case AUDIO_UNMUTE:
		bktr->audio_mute_state = FALSE;	/* clear mute */
		break;
	default:
		printf("bktr: audio cmd error %02x\n", cmd);
		return( -1 );
	}

	bt848 =	bktr->base;

	/*
	 * Leave the upper bits of the GPIO port alone in case they control
	 * something like the dbx or teletext chips.  This doesn't guarantee
	 * success, but follows the rule of least astonishment.
	 */

	/* XXX FIXME: this was an 8 bit reference before new struct ??? */
	bt848->gpio_reg_inp = (~GPIO_AUDIOMUX_BITS & 0xff);

	if ( bktr->audio_mute_state == TRUE )
		idx = 3;
	else
		idx = bktr->audio_mux_select;

	temp = bt848->gpio_data & ~GPIO_AUDIOMUX_BITS;
	bt848->gpio_data =
#if defined( AUDIOMUX_DISCOVER )
		bt848->gpio_data = temp | (cmd & 0xff);
		printf("cmd: %d\n", cmd );
#else
		temp | bktr->card.audiomuxs[ idx ];
#endif /* AUDIOMUX_DISCOVER */

	return( 0 );
}


/*
 * 
 */
static void
temp_mute( bktr_ptr_t bktr, int flag )
{
	static int	muteState = FALSE;

	if ( flag == TRUE ) {
		muteState = bktr->audio_mute_state;
		set_audio( bktr, AUDIO_MUTE );		/* prevent 'click' */
	}
	else {
		tsleep( (caddr_t)bktr, PZERO, "tuning", hz/8 );
		if ( muteState == FALSE )
			set_audio( bktr, AUDIO_UNMUTE );
	}
}


/*
 * setup the dbx chip
 * XXX FIXME: alot of work to be done here, this merely unmutes it.
 */
static int
set_BTSC( bktr_ptr_t bktr, int control )
{
	return( i2cWrite( bktr, TDA9850_WADDR, CON3ADDR, control ) );
}


/******************************************************************************
 * magic:
 */


static bktr_devsw_installed = 0;

static void
bktr_drvinit( void *unused )
{
	dev_t dev;

	if ( ! bktr_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&bktr_cdevsw, NULL);
		bktr_devsw_installed = 1;
	}
}

SYSINIT(bktrdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,bktr_drvinit,NULL)

#endif /* NBKTR > 0 */

/* Local Variables: */
/* mode: C */
/* c-indent-level: 8 */
/* c-brace-offset: -8 */
/* c-argdecl-indent: 8 */
/* c-label-offset: -8 */
/* c-continued-statement-offset: 8 */
/* c-tab-always-indent: nil */
/* End: */
