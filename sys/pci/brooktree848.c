/* BT848 1.30 Driver for Brooktree's Bt848 based cards.
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

1.15		4/18/97	   John-Mark Gurney <gurney_j@resnet.uoregon.edu>
                           Added [SR]RGBMASKs ioctl for byte swapping.

1.16		4/20/97	   Randall Hopper <rhh@ct.picker.com>
                           Generalized RGBMASK ioctls for general pixel
			   format setting [SG]ACTPIXFMT, and added query API
			   to return driver-supported pix fmts GSUPPIXFMT.

1.17		4/21/97	   hasty@rah.star-gate.com
                           Clipping support added.

1.18		4/23/97	   Clean up after failed CAP_SINGLEs where bt 
                           interrupt isn't delivered, and fixed fixing 
			   CAP_SINGLEs that for ODD_ONLY fields.
1.19            9/8/97     improved yuv support , cleaned up weurope
                           channel table, incorporated cleanup work from
                           Luigi, fixed pci interface bug due to a
                           change in the pci interface which disables
                           interrupts from a PCI device by default,
                           Added Luigi's, ioctl's BT848_SLNOTCH, 
                           BT848_GLNOTCH (set luma notch and get luma not)
1.20            10/5/97    Keith Sklower <sklower@CS.Berkeley.EDU> submitted
                           a patch to fix compilation of the BSDI's PCI
                           interface. 
                           Hideyuki Suzuki <hideyuki@sat.t.u-tokyo.ac.jp>
                           Submitted a patch for Japanese cable channels
                           Joao Carlos Mendes Luis jonny@gta.ufrj.br
                           Submitted general ioctl to set video broadcast
                           formats (PAL, NTSC, etc..) previously we depended
                           on the Bt848 auto video detect feature.
1.21            10/24/97   Randall Hopper <rhh@ct.picker.com>
                           Fix temporal decimation, disable it when
                           doing CAP_SINGLEs, and in dual-field capture, don't
                           capture fields for different frames
1.22            11/08/97   Randall Hopper <rhh@ct.picker.com>
                           Fixes for packed 24bpp - FIFO alignment
1.23            11/17/97   Amancio <hasty@star-gate.com>
                           Added yuv support mpeg encoding 
1.24            12/27/97   Jonathan Hanna <pangolin@rogers.wave.ca>
                           Patch to support Philips FR1236MK2 tuner
1.25            02/02/98   Takeshi Ohashi 
                           <ohashi@atohasi.mickey.ai.kyutech.ac.jp> submitted
                           code to support bktr_read .
                           Flemming Jacobsen <fj@schizo.dk.tfs.com>
                           submitted code to support  radio available with in
                           some bt848 based cards;additionally, wrote code to
                           correctly recognized his bt848 card.
                           Roger Hardiman <roger@cs.strath.ac.uk> submitted 
                           various fixes to smooth out the microcode and made 
                           all modes consistent.
1.26                       Moved Luigi's I2CWR ioctl from the video_ioctl
                           section to the tuner_ioctl section
                           Changed Major device from 79 to 92 and reserved
                           our Major device number -- hasty@star-gate.com
1.27                       Last batch of patches for radio support from
                           Flemming Jacobsen <fj@trw.nl>.
                           Added B849 PCI ID submitted by: 
                           Tomi Vainio <tomppa@fidata.fi>
1.28                       Frank Nobis <fn@Radio-do.de> added tuner support
                           for the  German Phillips PAL tuner and
                           additional channels for german cable tv.
1.29                       Roger Hardiman <roger@cs.strath.ac.uk>
                           Revised autodetection code to correctly handle both
                           old and new VideoLogic Captivator PCI cards.
                           Added tsleep of 2 seconds to initialistion code
                           for PAL users.Corrected clock selection code on
                           format change.
1.30                       Bring back Frank Nobis <fn@Radio-do.de>'s opt_bktr.h

*/

#define DDB(x) x
#define DEB(x)

#ifdef __FreeBSD__
#include "bktr.h"
#include "opt_bktr.h"
#include "opt_devfs.h"
#include "pci.h"
#endif /* __FreeBSD__ */

#if !defined(__FreeBSD__) || (NBKTR > 0 && NPCI > 0)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/mman.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#ifdef __FreeBSD__
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /* DEVFS */
#include <machine/clock.h>
#include <pci/pcivar.h>
#include <pci/pcireg.h>

#include <machine/ioctl_meteor.h>
#include <machine/ioctl_bt848.h>	/* extensions to ioctl_meteor.h */
#include <pci/brktree_reg.h>

typedef int ioctl_cmd_t;
#endif  /* __FreeBSD__ */

#ifdef __bsdi__
#include <sys/device.h>
#include <i386/isa/isa.h>
#include <i386/isa/isavar.h>
#include <i386/isa/icu.h>
#include <i386/pci/pci.h>
#include <i386/isa/dma.h>
#include <i386/eisa/eisa.h>
#include "ioctl_meteor.h"
#include "ioctl_bt848.h"
#include "bt848_reg.h"

typedef u_long ioctl_cmd_t;

#define pci_conf_read(a, r) pci_inl(a, r)
#define pci_conf_write(a, r, v) pci_outl(a, r, v)
#include <sys/reboot.h>
#define bootverbose (autoprint & (AC_VERBOSE|AC_DEBUG))
#endif /* __bsdi__ */

typedef u_char bool_t;

#define BKTRPRI (PZERO+8)|PCATCH

static void	bktr_intr __P((void *arg));


/*
 * memory allocated for DMA programs
 */
#define DMA_PROG_ALLOC		(8 * PAGE_SIZE)

/* When to split a dma transfer , the bt848 has timing as well as
   dma transfer size limitations so that we have to split dma
   transfers into two dma requests 
   */
#define DMA_BT848_SPLIT 319*2

/* 
 * Allocate enough memory for:
 *	768x576 RGB 16 or YUV (16 storage bits/pixel) = 884736 = 216 pages
 *
 * You may override this using the options "BROOKTREE_ALLOC_PAGES=value"
 * in your  kernel configuration file.
 */

#ifndef BROOKTREE_ALLOC_PAGES
#define BROOKTREE_ALLOC_PAGES	217*4
#endif
#define BROOKTREE_ALLOC		(BROOKTREE_ALLOC_PAGES * PAGE_SIZE)

/*  Defines for fields  */
#define ODD_F  0x01
#define EVEN_F 0x02

#ifdef __FreeBSD__

static bktr_reg_t brooktree[ NBKTR ];
#define BROOKTRE_NUM(mtr)	((bktr - &brooktree[0])/sizeof(bktr_reg_t))

#define UNIT(x)		((x) & 0x0f)
#define MINOR(x)	((x >> 4) & 0x0f)
#define ATTACH_ARGS	pcici_t tag, int unit

static char*	bktr_probe( pcici_t tag, pcidi_t type );
static void	bktr_attach( ATTACH_ARGS );

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

#define CDEV_MAJOR 92 
static struct cdevsw bktr_cdevsw = 
{
	bktr_open,	bktr_close,	bktr_read,	bktr_write,
	bktr_ioctl,	nostop,		nullreset,	nodevtotty,
	seltrue,	bktr_mmap,	NULL,		"bktr",
	NULL,		-1
};
#endif  /* __FreeBSD__ */

#ifdef __bsdi__
#define UNIT	dv_unit
#define MINOR	dv_subunit
#define ATTACH_ARGS \
   struct device * const parent, struct device * const self, void * const aux

#define PCI_COMMAND_STATUS_REG PCI_COMMAND

static void bktr_attach( ATTACH_ARGS );
#define NBKTR bktrcd.cd_ndevs
#define brooktree *((bktr_ptr_t *)bktrcd.cd_devs)

static int bktr_spl;
static int bktr_intr_returning_1(void *arg) { bktr_intr(arg); return (1);}
#define disable_intr() { bktr_spl = splhigh(); }
#define enable_intr() { splx(bktr_spl); }

static int
bktr_pci_match(pci_devaddr_t *pa)
{
	unsigned id;

	id = pci_inl(pa, PCI_VENDOR_ID);

	if (id == BROOKTREE_848_ID || id == BROOKTREE_849_ID   ) {
	  return 1;
	}
	aprint_debug("bktr_pci_match got %x\n", id);
	return 0;

}

pci_devres_t	bktr_res; /* XXX only remembers last one, helps debug */

static int
bktr_probe(struct device *parent, struct cfdata *cf, void *aux)
{
    pci_devaddr_t *pa;
    pci_devres_t res;
    struct isa_attach_args *ia = aux;

    if (ia->ia_bustype != BUS_PCI)
	return (0);

    if ((pa = pci_scan(bktr_pci_match)) == NULL)
	return (0);

    pci_getres(pa, &bktr_res, 1, ia);
    if (ia->ia_maddr == 0) {
	printf("bktr%d: no mem attached\n", cf->cf_unit);
	return (0);
    }
    ia->ia_aux = pa;
    return 1;
}


struct cfdriver bktrcd = 
{ 0, "bktr", bktr_probe, bktr_attach, DV_DULL, sizeof(bktr_reg_t) };

int	bktr_open __P((dev_t, int, int, struct proc *));
int	bktr_close __P((dev_t, int, int, struct proc *));
int	bktr_read __P((dev_t, struct uio *, int));
int	bktr_write __P((dev_t, struct uio *, int));
int	bktr_ioctl __P((dev_t, ioctl_cmd_t, caddr_t, int, struct proc *));
int	bktr_mmap __P((dev_t, int, int));

struct devsw bktrsw = {
	&bktrcd,
	bktr_open, bktr_close, bktr_read, bktr_write, bktr_ioctl,
	seltrue, bktr_mmap, NULL, nodump, NULL, 0, nostop
};
#endif /* __bsdi__ */

/*
 * This is for start-up convenience only, NOT mandatory.
 */
#if !defined( DEFAULT_CHNLSET )
#define DEFAULT_CHNLSET	CHNLSET_WEUROPE
#endif

/*
 * Parameters describing size of transmitted image.
 */

static struct format_params format_params[] = {
/* # define BT848_IFORM_F_AUTO             (0x0) - don't matter. */
  { 525, 26, 480,  910, 135, 754, 640,  780, 30, 0x68, 0x5d, BT848_IFORM_X_AUTO },
/* # define BT848_IFORM_F_NTSCM            (0x1) */
  { 525, 26, 480,  910, 135, 754, 640,  780, 30, 0x68, 0x5d, BT848_IFORM_X_XT0 },
/* # define BT848_IFORM_F_NTSCJ            (0x2) */
  { 525, 22, 480,  910, 135, 754, 640,  780, 30, 0x68, 0x5d, BT848_IFORM_X_XT0 },
/* # define BT848_IFORM_F_PALBDGHI         (0x3) */
  { 625, 32, 576, 1135, 186, 922, 768,  944, 25, 0x7f, 0x72, BT848_IFORM_X_XT1 },
/* # define BT848_IFORM_F_PALM             (0x4) */
  { 525, 22, 480,  910, 135, 754, 640,  780, 30, 0x68, 0x5d, BT848_IFORM_X_XT0 },
/*{ 625, 32, 576,  910, 186, 922, 640,  780, 25, 0x68, 0x5d, BT848_IFORM_X_XT0 }, */
/* # define BT848_IFORM_F_PALN             (0x5) */
  { 625, 32, 576, 1135, 186, 922, 768,  944, 25, 0x7f, 0x72, BT848_IFORM_X_XT1 },
/* # define BT848_IFORM_F_SECAM            (0x6) */
  { 625, 32, 576, 1135, 186, 922, 768,  944, 25, 0x7f, 0x00, BT848_IFORM_X_XT1 },
/* # define BT848_IFORM_F_RSVD             (0x7) - ???? */
  { 625, 32, 576, 1135, 186, 922, 768,  944, 25, 0x7f, 0x72, BT848_IFORM_X_XT0 },
};

/*
 * Table of supported Pixel Formats 
 */

static struct meteor_pixfmt_internal {
	struct meteor_pixfmt public;
	u_int                color_fmt;
} pixfmt_table[] = {

{ { 0, METEOR_PIXTYPE_RGB, 2, {   0x7c00,  0x03e0,  0x001f }, 0,0 }, 0x33 },
{ { 0, METEOR_PIXTYPE_RGB, 2, {   0x7c00,  0x03e0,  0x001f }, 1,0 }, 0x33 },

{ { 0, METEOR_PIXTYPE_RGB, 2, {   0xf800,  0x07e0,  0x001f }, 0,0 }, 0x22 },
{ { 0, METEOR_PIXTYPE_RGB, 2, {   0xf800,  0x07e0,  0x001f }, 1,0 }, 0x22 },

{ { 0, METEOR_PIXTYPE_RGB, 3, { 0xff0000,0x00ff00,0x0000ff }, 1,0 }, 0x11 },

{ { 0, METEOR_PIXTYPE_RGB, 4, { 0xff0000,0x00ff00,0x0000ff }, 0,0 }, 0x00 },
{ { 0, METEOR_PIXTYPE_RGB, 4, { 0xff0000,0x00ff00,0x0000ff }, 0,1 }, 0x00 },
{ { 0, METEOR_PIXTYPE_RGB, 4, { 0xff0000,0x00ff00,0x0000ff }, 1,0 }, 0x00 },
{ { 0, METEOR_PIXTYPE_RGB, 4, { 0xff0000,0x00ff00,0x0000ff }, 1,1 }, 0x00 },
{ { 0, METEOR_PIXTYPE_YUV, 2, { 0xff0000,0x00ff00,0x0000ff }, 1,1 }, 0x88 },
{ { 0, METEOR_PIXTYPE_YUV_PACKED, 2, { 0xff0000,0x00ff00,0x0000ff }, 0,1 }, 0x44 },
{ { 0, METEOR_PIXTYPE_YUV_12, 2, { 0xff0000,0x00ff00,0x0000ff }, 1,1 }, 0x88 },

};
#define PIXFMT_TABLE_SIZE ( sizeof(pixfmt_table) / sizeof(pixfmt_table[0]) )

/*
 * Table of Meteor-supported Pixel Formats (for SETGEO compatibility)
 */

/*  FIXME:  Also add YUV_422 and YUV_PACKED as well  */
static struct {
	u_long               meteor_format;
	struct meteor_pixfmt public;
} meteor_pixfmt_table[] = {
    { METEOR_GEO_YUV_12,
      { 0, METEOR_PIXTYPE_YUV_12, 2, { 0xff0000,0x00ff00,0x0000ff }, 1,1 }
    },

      /* FIXME: Should byte swap flag be on for this one; negative in drvr? */
    { METEOR_GEO_YUV_422,
      { 0, METEOR_PIXTYPE_YUV, 2, { 0xff0000,0x00ff00,0x0000ff }, 1,1 }
    },
    { METEOR_GEO_YUV_PACKED,
      { 0, METEOR_PIXTYPE_YUV_PACKED, 2, { 0xff0000,0x00ff00,0x0000ff }, 0,1 }
    },
    { METEOR_GEO_RGB16,
      { 0, METEOR_PIXTYPE_RGB, 2, {   0x7c00,   0x03e0,   0x001f }, 0, 0 }
    },
    { METEOR_GEO_RGB24,
      { 0, METEOR_PIXTYPE_RGB, 4, { 0xff0000, 0x00ff00, 0x0000ff }, 0, 0 }
    },

};
#define METEOR_PIXFMT_TABLE_SIZE ( sizeof(meteor_pixfmt_table) / \
				   sizeof(meteor_pixfmt_table[0]) )


#define BSWAP (BT848_COLOR_CTL_BSWAP_ODD | BT848_COLOR_CTL_BSWAP_EVEN)
#define WSWAP (BT848_COLOR_CTL_WSWAP_ODD | BT848_COLOR_CTL_WSWAP_EVEN)


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

/* PLL on a Temic NTSC tuner: 4032FY5 */
#define TEMIC_NTSC_WADDR	0xc0
#define TEMIC_NTSC_RADDR	0xc1

/* PLL on a Temic PAL I tuner: 4062FY5 */
#define TEMIC_PALI_WADDR	0xc2
#define TEMIC_PALI_RADDR	0xc3

/* PLL on a Philips tuner */
#define PHILIPS_NTSC_WADDR	0xc6
#define PHILIPS_NTSC_RADDR	0xc7

/* PLL on a the Philips FR1236MK2 tuner */
#define PHILIPS_FR1236_NTSC_WADDR      0xc2
#define PHILIPS_FR1236_NTSC_RADDR      0xc3

/* PLL on a the Philips FR1216MK2 tuner,
   yes, the european version of the tuner is 1216 */
#define PHILIPS_FR1216_PAL_WADDR	0xc2
#define PHILIPS_FR1216_PAL_RADDR	0xc3

/* guaranteed address for any TSA5522/3 (PLL on all(?) tuners) */
#define TSA552x_WADDR		0xc2
#define TSA552x_RADDR		0xc3

#define PHILIPS_PAL_WADDR       0xc2
#define PHILIPS_PAL_RADDR       0xc3


#define TSA552x_CB_MSB		(0x80)
#define TSA552x_CB_CP		(1<<6)
#define TSA552x_CB_T2		(1<<5)
#define TSA552x_CB_T1		(1<<4)
#define TSA552x_CB_T0		(1<<3)
#define TSA552x_CB_RSA		(1<<2)
#define TSA552x_CB_RSB		(1<<1)
#define TSA552x_CB_OS		(1<<0)
#define TSA552x_RADIO		(TSA552x_CB_MSB |       \
				 TSA552x_CB_T0)

/* Add RADIO_OFFSET to the "frequency" to indicate that we want to tune	*/
/* the radio (if present) not the TV tuner.				*/
/* 20000 is equivalent to 20000MHz/16 = 1.25GHz - this area is unused.	*/
#define RADIO_OFFSET		20000


/* address of BTSC/SAP decoder chip */
#define TDA9850_WADDR		0xb6
#define TDA9850_RADDR		0xb7

/* address of MSP3400C chip */
#define MSP3400C_WADDR		0x80
#define MSP3400C_RADDR		0x81


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
#if 0
#define SYNC_LEVEL		(BT848_ADC_RESERVED |	\
				 BT848_ADC_CRUSH)	/* threshold ~125 mV */
#else
#define SYNC_LEVEL		(BT848_ADC_RESERVED |	\
				 BT848_ADC_SYNC_T)	/* threshold ~75 mV */
#endif


/* the GPIO bits that control the audio MUXes */
#define GPIO_AUDIOMUX_BITS	0x0f


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
#define TDEC_BITS               (BT848_INT_FDSR | BT848_INT_FBUS)


/*
 * misc. support routines.
 */
static int			signCard( bktr_ptr_t bktr, int offset,
					  int count, u_char* sig );
static void			probeCard( bktr_ptr_t bktr, int verbose );

static vm_offset_t		get_bktr_mem( int unit, unsigned size );

static int			oformat_meteor_to_bt( u_long format );

static u_int			pixfmt_swap_flags( int pixfmt );

/*
 * bt848 RISC programming routines.
 */
#ifdef BT848_DUMP
static int	dump_bt848( bt848_ptr_t bt848 );
#endif

static void	yuvpack_prog( bktr_ptr_t bktr, char i_flag, int cols,
			      int rows,  int interlace );
static void	yuv422_prog( bktr_ptr_t bktr, char i_flag, int cols,
			     int rows, int interlace );
static void	yuv12_prog( bktr_ptr_t bktr, char i_flag, int cols,
			     int rows, int interlace );
static void	rgb_prog( bktr_ptr_t bktr, char i_flag, int cols,
			  int rows, int interlace );
static void	build_dma_prog( bktr_ptr_t bktr, char i_flag );

static bool_t   getline(bktr_reg_t *, int);
static bool_t   notclipped(bktr_reg_t * , int , int);     
static bool_t   split(bktr_reg_t *, volatile u_long **, int, u_long, int, 
		      volatile u_char ** , int  );

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


#ifdef __FreeBSD__
/*
 * the boot time probe routine.
 */
static char*
bktr_probe( pcici_t tag, pcidi_t type )
{
	switch (type) {
	case BROOKTREE_848_ID:
		return("BrookTree 848");
        case BROOKTREE_849_ID:
               return("BrookTree 849");
	};

	return ((char *)0);
}
#endif /* __FreeBSD__ */




/*
 * the attach routine.
 */
static	void
bktr_attach( ATTACH_ARGS )
{
	bktr_ptr_t	bktr;
	bt848_ptr_t	bt848;
#ifdef BROOKTREE_IRQ
	u_long		old_irq, new_irq;
#endif 
	vm_offset_t	buf;
	u_long		latency;
	u_long		fun;


#ifdef __FreeBSD__
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


#ifdef BROOKTREE_IRQ		/* from the configuration file */
	old_irq = pci_conf_read(tag, PCI_INTERRUPT_REG);
	pci_conf_write(tag, PCI_INTERRUPT_REG, BROOKTREE_IRQ);
	new_irq = pci_conf_read(tag, PCI_INTERRUPT_REG);
	printf("bktr%d: attach: irq changed from %d to %d\n",
		unit, (old_irq & 0xff), (new_irq & 0xff));
#endif 
	/* setup the interrupt handling routine */
	pci_map_int(tag, bktr_intr, (void*) bktr, &net_imask);
#endif  /* __FreeBSD__ */

#ifdef __bsdi__
	struct isa_attach_args * const ia = (struct isa_attach_args *)aux;
	pci_devaddr_t *tag = (pci_devaddr_t *) ia->ia_aux;
	int unit  = bktr->bktr_dev.dv_unit;

	bktr = (bktr_reg_t *) self;
	bktr->base = (bt848_ptr_t) bktr_res.pci_vaddr;
	isa_establish(&bktr->bktr_id, &bktr->bktr_dev);
	bktr->bktr_ih.ih_fun = bktr_intr_returning_1;
	bktr->bktr_ih.ih_arg = (void *)bktr;
	intr_establish(ia->ia_irq, &bktr->bktr_ih, DV_DULL);
#endif /* __bsdi__ */
	
/*
 * PCI latency timer.  32 is a good value for 4 bus mastering slots, if
 * you have more than four, then 16 would probably be a better value.
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
		printf(" %d.\n", (int) latency);
	}


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
		bktr->flags = METEOR_INITALIZED | METEOR_AUTOMODE |
			      METEOR_DEV0 | METEOR_RGB16;
		bktr->dma_prog_loaded = FALSE;
		bktr->cols = 640;
		bktr->rows = 480;
		bktr->frames = 1;		/* one frame */
		bktr->format = METEOR_GEO_RGB16;
		bktr->pixfmt = oformat_meteor_to_bt( bktr->format );
		bktr->pixfmt_compat = TRUE;
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
	/* XXX This just throw away the token, which should probably be fixed when
	   DEVFS is finally made really operational. */
	devfs_add_devswf(&bktr_cdevsw, unit,    DV_CHR, 0, 0, 0444, "bktr%d",  unit);
	devfs_add_devswf(&bktr_cdevsw, unit+16, DV_CHR, 0, 0, 0444, "tuner%d", unit);
#endif /* DEVFS */
#if __FreeBSD__ > 2 
	fun = pci_conf_read(tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(tag, PCI_COMMAND_STATUS_REG, fun | 4);
#endif

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
	u_long                  field;
	u_long                  w_field;
	u_long                  req_field;

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
	/*	printf( " STATUS %x %x %x \n",
		dstatus, bktr_status, bt848->risc_count );
		*/		
	/* if risc was disabled re-start process again */
	if ( !(bktr_status & BT848_INT_RISC_EN) ||
	     ((bktr_status &(BT848_INT_FBUS   |
			      BT848_INT_FTRGT  |
			      BT848_INT_FDSR   |
			      BT848_INT_PPERR  |
			      BT848_INT_RIPERR |
			      BT848_INT_PABORT |
			      BT848_INT_OCERR  |
			      BT848_INT_SCERR) ) != 0) ||
	     ((bt848->tdec == 0) && (bktr_status & TDEC_BITS)) ) {

		u_short	tdec_save = bt848->tdec;

		bt848->gpio_dma_ctl = FIFO_RISC_DISABLED;

		bt848->int_mask = ALL_INTS_DISABLED;

		/*  Reset temporal decimation ctr  */
		bt848->tdec = 0;
		bt848->tdec = tdec_save;
		
		/*  Reset to no-fields captured state  */
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
	 *  Register the completed field
	 *    (For dual-field mode, require fields from the same frame)
	 */
	field = ( bktr_status & BT848_INT_FIELD ) ? EVEN_F : ODD_F;
	switch ( bktr->flags & METEOR_WANT_MASK ) {
		case METEOR_WANT_ODD  : w_field = ODD_F         ;  break;
		case METEOR_WANT_EVEN : w_field = EVEN_F        ;  break;
		default               : w_field = (ODD_F|EVEN_F);  break;
	}
	switch ( bktr->flags & METEOR_ONLY_FIELDS_MASK ) {
		case METEOR_ONLY_ODD_FIELDS  : req_field = ODD_F  ;  break;
		case METEOR_ONLY_EVEN_FIELDS : req_field = EVEN_F ;  break;
		default                      : req_field = (ODD_F|EVEN_F);  
			                       break;
	}

	if (( field == EVEN_F ) && ( w_field == EVEN_F ))
		bktr->flags &= ~METEOR_WANT_EVEN;
	else if (( field == ODD_F ) && ( req_field == ODD_F ) &&
		 ( w_field == ODD_F ))
		bktr->flags &= ~METEOR_WANT_ODD;
	else if (( field == ODD_F ) && ( req_field == (ODD_F|EVEN_F) ) &&
		 ( w_field == (ODD_F|EVEN_F) ))
		bktr->flags &= ~METEOR_WANT_ODD;
	else if (( field == ODD_F ) && ( req_field == (ODD_F|EVEN_F) ) &&
		 ( w_field == ODD_F )) {
		bktr->flags &= ~METEOR_WANT_ODD;
		bktr->flags |=  METEOR_WANT_EVEN;
	}
	else {
		/*  We're out of sync.  Start over.  */
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
		return;
	}

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
/*
* XXX NOTE (Luigi):
* currently we only support 3 capture modes: odd only, even only,
* odd+even interlaced (odd field first). A fourth mode (non interlaced,
* either even OR odd) could provide 60 (50 for PAL) pictures per
* second, but it would require this routine to toggle the desired frame
* each time, and one more different DMA program for the Bt848.
* As a consequence, this fourth mode is currently unsupported.
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

#ifdef BT848_DUMP
	dump_bt848( bt848 );
#endif

	bt848->dstatus = 0x00;			/* clear device status reg. */

	bt848->adc = SYNC_LEVEL;

	bt848->iform = BT848_IFORM_M_MUX1 |
		       BT848_IFORM_X_XT0  |
		       BT848_IFORM_F_NTSCM;
	bktr->flags = (bktr->flags & ~METEOR_DEV_MASK) | METEOR_DEV0;
	bktr->format_params = BT848_IFORM_F_NTSCM;

	bktr->max_clip_node = 0;

	bt848->color_ctl_gamma       = 0;
	bt848->color_ctl_rgb_ded     = 1;
	bt848->color_ctl_color_bars  = 0;
	bt848->color_ctl_ext_frmrate = 0;
	bt848->color_ctl_swap        = 0;

	bt848->e_hscale_lo = 170;
	bt848->o_hscale_lo = 170;

	bt848->e_delay_lo = 0x72;
	bt848->o_delay_lo = 0x72;
	bt848->e_scloop = 0;
	bt848->o_scloop = 0;

	bt848->vbi_pack_size = 0;
	bt848->vbi_pack_del = 0;

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
	bktr->pixfmt_compat = TRUE;
	bktr->format = METEOR_GEO_RGB16;
	bktr->pixfmt = oformat_meteor_to_bt( bktr->format );


	bt848->int_mask = BT848_INT_MYSTERYBIT;	/* what does this bit do ??? */

	/* wait 2 seconds while bt848 initialises */
	tsleep( (caddr_t)bktr, PZERO, "btinit", hz*2 );

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
        bktr->tuner.radio_mode = 0;

	/* enable drivers on the GPIO port that control the MUXes */
	bktr->base->gpio_out_en = GPIO_AUDIOMUX_BITS;

	/* unmute the audio stream */
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
	
	if (MINOR(minor(dev)) > 0)
		return( ENXIO );

	unit = UNIT(minor(dev));
	if (unit >= NBKTR)	/* unit out of range */
		return( ENXIO );

	bktr = &(brooktree[unit]);
        bt848 = bktr->base;

	if (bktr->bigbuf == 0)	/* no frame buffer allocated (ioctl failed) */
		return( ENOMEM );

	if (bktr->flags & METEOR_CAP_MASK)
		return( EIO );	/* already capturing */

        bt848->cap_ctl = bktr->bktr_cap_ctl;


	count = bktr->rows * bktr->cols * 
		pixfmt_table[ bktr->pixfmt ].public.Bpp;

	if ((int) uio->uio_iov->iov_len < count)
		return( EINVAL );

	bktr->flags &= ~(METEOR_CAP_MASK | METEOR_WANT_MASK);

	/* capture one frame */
	start_capture(bktr, METEOR_SINGLE);
	/* wait for capture to complete */
	bt848->int_stat = ALL_INTS_CLEARED;
	bt848->gpio_dma_ctl = FIFO_ENABLED;
	bt848->gpio_dma_ctl = bktr->capcontrol;
	bt848->int_mask = BT848_INT_MYSTERYBIT |
                          BT848_INT_RISCI      |
                          BT848_INT_VSYNC      |
                          BT848_INT_FMTCHG;


	status = tsleep((caddr_t)bktr, BKTRPRI, "captur", 0);
	if (!status)		/* successful capture */
		status = uiomove((caddr_t)bktr->bigbuf, count, uio);
	else
		printf ("bktr%d: read: tsleep error %d\n", unit, status);

	bktr->flags &= ~(METEOR_SINGLE | METEOR_WANT_MASK);

	return( status );
}


/*
 * 
 */
int
bktr_write( dev_t dev, struct uio *uio, int ioflag )
{
	return( EINVAL ); /* XXX or ENXIO ? */
}


/*
 * 
 */
int
bktr_ioctl( dev_t dev, ioctl_cmd_t cmd, caddr_t arg, int flag, struct proc* pr )
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
	int			tmp_int;
	bt848_ptr_t		bt848;
	volatile u_char		c_temp;
	unsigned int		temp;
	unsigned int		temp_iform;
	unsigned int		error;
	struct meteor_geomet	*geo;
	struct meteor_counts	*cnt;
	struct meteor_video	*video;
	vm_offset_t		buf;
	struct format_params	*fp;
	int                     i;
 
	bt848 =	bktr->base;

	switch ( cmd ) {

	case BT848SCLIP: /* set clip region */
	    bktr->max_clip_node = 0;
	    memcpy(&bktr->clip_list, arg, sizeof(bktr->clip_list));

	    for (i = 0; i < BT848_MAX_CLIP_NODE; i++) {
		if (bktr->clip_list[i].y_min ==  0 &&
		    bktr->clip_list[i].y_max == 0)
		    break;
	    }
	    bktr->max_clip_node = i;

	    /* make sure that the list contains a valid clip secquence */
	    /* the clip rectangles should be sorted by x then by y as the
               second order sort key */

	    /* clip rectangle list is terminated by y_min and y_max set to 0 */

	    /* to disable clipping set  y_min and y_max to 0 in the first
               clip rectangle . The first clip rectangle is clip_list[0].
             */

             
                
	    if (bktr->max_clip_node == 0 && 
		(bktr->clip_list[0].y_min != 0 && 
		 bktr->clip_list[0].y_max != 0)) {
		return EINVAL;
	    }

	    for (i = 0; i < BT848_MAX_CLIP_NODE - 1 ; i++) {
		if (bktr->clip_list[i].y_min == 0 &&
		    bktr->clip_list[i].y_max == 0) {
		    break;
		}
		if ( bktr->clip_list[i+1].y_min != 0 &&
		     bktr->clip_list[i+1].y_max != 0 &&
		     bktr->clip_list[i].x_min > bktr->clip_list[i+1].x_min ) {

		    bktr->max_clip_node = 0;
		    return (EINVAL);

		 }

		if (bktr->clip_list[i].x_min >= bktr->clip_list[i].x_max ||
		    bktr->clip_list[i].y_min >= bktr->clip_list[i].y_max ||
		    bktr->clip_list[i].x_min < 0 ||
		    bktr->clip_list[i].x_max < 0 || 
		    bktr->clip_list[i].y_min < 0 ||
		    bktr->clip_list[i].y_max < 0 ) {
		    bktr->max_clip_node = 0;
		    return (EINVAL);
		}
	    }

	    bktr->dma_prog_loaded = FALSE;

	    break;

	case METEORSTATUS:	/* get Bt848 status */
		c_temp = bt848->dstatus;
		temp = 0;
		if (!(c_temp & 0x40)) temp |= METEOR_STATUS_HCLK;
		if (!(c_temp & 0x10)) temp |= METEOR_STATUS_FIDT;
		*(u_short *)arg = temp;
		break;

	case BT848SFMT:		/* set input format */
		temp = *(unsigned long*)arg & BT848_IFORM_FORMAT;
		temp_iform = bt848->iform;
		temp_iform &= ~BT848_IFORM_FORMAT;
		temp_iform &= ~BT848_IFORM_XTSEL;
		bt848->iform = (temp_iform | temp | format_params[temp].iform_xtsel);
		switch( temp ) {
		case BT848_IFORM_F_AUTO:
			bktr->flags = (bktr->flags & ~METEOR_FORM_MASK) |
			METEOR_AUTOMODE;
			break;

		case BT848_IFORM_F_NTSCM:
		case BT848_IFORM_F_NTSCJ:
			bktr->flags = (bktr->flags & ~METEOR_FORM_MASK) |
				METEOR_NTSC;
			bt848->adelay = format_params[temp].adelay;
			bt848->bdelay = format_params[temp].bdelay;
			bktr->format_params = temp;
			break;

		case BT848_IFORM_F_PALBDGHI:
		case BT848_IFORM_F_PALN:
		case BT848_IFORM_F_SECAM:
		case BT848_IFORM_F_RSVD:
		case BT848_IFORM_F_PALM:
			bktr->flags = (bktr->flags & ~METEOR_FORM_MASK) |
				METEOR_PAL;
			bt848->adelay = format_params[temp].adelay;
			bt848->bdelay = format_params[temp].bdelay;
			bktr->format_params = temp;
			break;

		}
		bktr->dma_prog_loaded = FALSE;		
		break;

	case METEORSFMT:	/* set input format */
		temp_iform = bt848->iform;
		temp_iform &= ~BT848_IFORM_FORMAT;
		temp_iform &= ~BT848_IFORM_XTSEL;
		switch(*(unsigned long *)arg & METEOR_FORM_MASK ) {
		case 0:		/* default */
		case METEOR_FMT_NTSC:
			bktr->flags = (bktr->flags & ~METEOR_FORM_MASK) |
				METEOR_NTSC;
			bt848->iform = temp_iform | BT848_IFORM_F_NTSCM | 
		                        format_params[BT848_IFORM_F_NTSCM].iform_xtsel;
			bt848->adelay = format_params[BT848_IFORM_F_NTSCM].adelay;
			bt848->bdelay = format_params[BT848_IFORM_F_NTSCM].bdelay;
			bktr->format_params = BT848_IFORM_F_NTSCM;
			break;

		case METEOR_FMT_PAL:
			bktr->flags = (bktr->flags & ~METEOR_FORM_MASK) |
				METEOR_PAL;
			bt848->iform = temp_iform | BT848_IFORM_F_PALBDGHI |
		                        format_params[BT848_IFORM_F_PALBDGHI].iform_xtsel;
			bt848->adelay = format_params[BT848_IFORM_F_PALBDGHI].adelay;
			bt848->bdelay = format_params[BT848_IFORM_F_PALBDGHI].bdelay;
			bktr->format_params = BT848_IFORM_F_PALBDGHI;
			break;

		case METEOR_FMT_AUTOMODE:
			bktr->flags = (bktr->flags & ~METEOR_FORM_MASK) |
				METEOR_AUTOMODE;
			bt848->iform = temp_iform | BT848_IFORM_F_AUTO |
		                        format_params[BT848_IFORM_F_AUTO].iform_xtsel;
			break;

		default:
			return( EINVAL );
		}
		bktr->dma_prog_loaded = FALSE;		
		break;

	case METEORGFMT:	/* get input format */
		*(u_long *)arg = bktr->flags & METEOR_FORM_MASK;
		break;


	case BT848GFMT:		/* get input format */
	        *(u_long *)arg = bt848->iform & BT848_IFORM_FORMAT;
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
		if(*(int *)arg == 0 || *(int *)arg >= NSIG) {
			return( EINVAL );
			break;
		}
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
			if (temp & METEOR_CAP_MASK)
				return( EIO );



			start_capture(bktr, METEOR_SINGLE);

			/* wait for capture to complete */
			bt848->int_stat = ALL_INTS_CLEARED;
			bt848->gpio_dma_ctl = FIFO_ENABLED;
			bt848->gpio_dma_ctl = bktr->capcontrol;

			bt848->int_mask = BT848_INT_MYSTERYBIT |
					  BT848_INT_RISCI      |
					  BT848_INT_VSYNC      |
					  BT848_INT_FMTCHG;

			bt848->cap_ctl = bktr->bktr_cap_ctl;
			error = tsleep((caddr_t)bktr, BKTRPRI, "captur", hz);
			if (error && (error != ERESTART)) {
				/*  Here if we didn't get complete frame  */
#ifdef DIAGNOSTIC
				printf( "bktr%d: ioctl: tsleep error %d %x\n",
					unit, error, bt848->risc_count);
#endif

				/* stop dma */
				bt848->int_mask = ALL_INTS_DISABLED;

				/* disable risc, leave fifo running */
				bt848->gpio_dma_ctl = FIFO_ENABLED;
			}

			bktr->flags &= ~(METEOR_SINGLE|METEOR_WANT_MASK);
			/* FIXME: should we set bt848->int_stat ??? */
			break;

		case METEOR_CAP_CONTINOUS:
			if (bktr->bigbuf==0)	/* no frame buffer allocated */
				return( ENOMEM );
			/* already capturing */
			if (temp & METEOR_CAP_MASK)
			    return( EIO );


			start_capture(bktr, METEOR_CONTIN);
			bt848->int_stat = bt848->int_stat;

			bt848->gpio_dma_ctl = FIFO_ENABLED;
			bt848->gpio_dma_ctl = bktr->capcontrol;
			bt848->cap_ctl = bktr->bktr_cap_ctl;

			bt848->int_mask = BT848_INT_MYSTERYBIT |
					  BT848_INT_RISCI      |
					  BT848_INT_VSYNC      |
					  BT848_INT_FMTCHG;
#ifdef BT848_DUMP
			dump_bt848( bt848 );
#endif
			break;
		
		case METEOR_CAP_STOP_CONT:
			if (bktr->flags & METEOR_CONTIN) {
				/* turn off capture */
				bt848->gpio_dma_ctl = FIFO_RISC_DISABLED;
				bt848->cap_ctl = CAPTURE_OFF;
				bt848->int_mask = ALL_INTS_DISABLED;
				bktr->flags &=
					~(METEOR_CONTIN | METEOR_WANT_MASK);

			}
		}
		break;

	case METEORSETGEO:
		/* can't change parameters while capturing */
		if (bktr->flags & METEOR_CAP_MASK)
			return( EBUSY );


		geo = (struct meteor_geomet *) arg;

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

		if ((temp=(geo->rows * geo->columns * geo->frames * 2))) {
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
				"bktr%d: ioctl: Allocating %d bytes\n",
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

		/*  Pixel format (if in meteor pixfmt compatibility mode)  */
		if ( bktr->pixfmt_compat ) {
			bktr->format = METEOR_GEO_YUV_422;
			switch (geo->oformat & METEOR_GEO_OUTPUT_MASK) {
			case 0:			/* default */
			case METEOR_GEO_RGB16:
				    bktr->format = METEOR_GEO_RGB16;
				    break;
			case METEOR_GEO_RGB24:
				    bktr->format = METEOR_GEO_RGB24;
				    break;
			case METEOR_GEO_YUV_422:
				    bktr->format = METEOR_GEO_YUV_422;
                                    if (geo->oformat & METEOR_GEO_YUV_12) 
					bktr->format = METEOR_GEO_YUV_12;
				    break;
			case METEOR_GEO_YUV_PACKED:
				    bktr->format = METEOR_GEO_YUV_PACKED;
				    break;
			}
			bktr->pixfmt = oformat_meteor_to_bt( bktr->format );
		}

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
	int		tmp_int;
	unsigned int	temp, temp1;
	int		offset;
	int		count;
	u_char		*buf;
	u_long          par;
	u_char          write;
	int             i2c_addr;
	int             i2c_port;
	u_long          data;

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

/* lr 970528 luma notch etc - 3 high bits of e_control/o_control */

	case BT848_SLNOTCH:	/* set luma notch */
		tmp_int = (*(int *)arg & 0x7) << 5 ;
		bt848->e_control &= ~0xe0 ;
		bt848->o_control &= ~0xe0 ;
	bt848->e_control |= tmp_int ;
		bt848->o_control |= tmp_int ;
		break;

	case BT848_GLNOTCH:	/* get luma notch */
		*(int *)arg = (int) ( (bt848->e_control & 0xe0) >> 5) ;
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

		/*  FIXME:  SCBARS and CCBARS require a valid int *        */
		/*    argument to succeed, but its not used; consider      */
		/*    using the arg to store the on/off state so           */
		/*    there's only one ioctl() needed to turn cbars on/off */
	case BT848_SCBARS:	/* set colorbar output */
		bt848->color_ctl_color_bars = 1;
		break;

	case BT848_CCBARS:	/* clear colorbar output */
		bt848->color_ctl_color_bars = 0;
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
	/* Ioctl's for running the tuner device in radio mode		*/

	case RADIO_GETMODE:
            *(unsigned char *)arg = bktr->tuner.radio_mode;
	    break;

	case RADIO_SETMODE:
            bktr->tuner.radio_mode = *(unsigned char *)arg;
            break;

 	case RADIO_GETFREQ:
            *(unsigned long *)arg = (bktr->tuner.frequency+407)*5;
            break;

	case RADIO_SETFREQ:
	    /* The argument to this ioctl is NOT freq*16. It is
	    ** freq*100.
	    */

	    /* The radio in my stereo and the linear regression function
	    ** in my HP48 have reached the conclusion that in order to
	    ** set the radio tuner of the FM1216 to f MHz, the value to
	    ** enter into the PLL is: f*20-407
	    ** If anyone has the exact values from the spec. sheet
	    ** please forward them  -- fj@login.dknet.dk
	    */
	    temp=(int)*(unsigned long *)arg/5-407  +RADIO_OFFSET;

#ifdef BKTR_RADIO_DEBUG
  printf("bktr%d: arg=%d temp=%d\n",unit,(int)*(unsigned long *)arg,temp);
#endif

#ifndef BKTR_RADIO_NOFREQCHECK
	    /* According to the spec. sheet the band: 87.5MHz-108MHz	*/
	    /* is supported.						*/
	    if(temp<1343+RADIO_OFFSET || temp>1753+RADIO_OFFSET) {
	      printf("bktr%d: Radio frequency out of range\n",unit);
	      return(EINVAL);
	      }
#endif
	    temp_mute( bktr, TRUE );
	    temp = tv_freq( bktr, temp );
	    temp_mute( bktr, FALSE );
#ifdef BKTR_RADIO_DEBUG
  if(temp)
    printf("bktr%d: tv_freq returned: %d\n",unit,temp);
#endif
	    if ( temp < 0 )
		    return( EINVAL );
	    *(unsigned long *)arg = temp;
	    break;
	    /* Luigi's I2CWR ioctl */ 
	case BT848_I2CWR:
		par = *(u_long *)arg;
		write = (par >> 24) & 0xff ;
		i2c_addr = (par >> 16) & 0xff ;
		i2c_port = (par >> 8) & 0xff ;
		data = (par) & 0xff ;
 
		if (write) { 
			i2cWrite( bktr, i2c_addr, i2c_port, data);
		} else {
			data = i2cRead( bktr, i2c_addr);
		}
		*(u_long *)arg = (par & 0xffffff00) | ( data & 0xff );
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
        int                           pixfmt;
	unsigned int	              temp;
	struct meteor_pixfmt          *pf_pub;

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

	case METEORSACTPIXFMT:
		if (( *(int *)arg < 0 ) ||
		    ( *(int *)arg >= PIXFMT_TABLE_SIZE ))
			return( EINVAL );

		bktr->pixfmt          = *(int *)arg;
		bt848->color_ctl_swap = pixfmt_swap_flags( bktr->pixfmt );
		bktr->pixfmt_compat   = FALSE;
		break;
	
	case METEORGACTPIXFMT:
		*(int *)arg = bktr->pixfmt;
		break;

	case METEORGSUPPIXFMT :
		pf_pub = (struct meteor_pixfmt *)arg;
		pixfmt = pf_pub->index;

		if (( pixfmt < 0 ) || ( pixfmt >= PIXFMT_TABLE_SIZE ))
			return( EINVAL );

		memcpy( pf_pub, &pixfmt_table[ pixfmt ].public, 
			sizeof( *pf_pub ) );

		/*  Patch in our format index  */
		pf_pub->index       = pixfmt;
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
		return( ENOTTY );
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

	if (unit >= NBKTR || MINOR(minor(dev)) > 0)/* could this happen here? */
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
#ifdef BT848_DEBUG 
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

#endif

/*
 * build write instruction
 */
#define BKTR_FM1      0x6	/* packed data to follow */
#define BKTR_FM3      0xe	/* planar data to follow */
#define BKTR_VRE      0x4	/* even field to follow */
#define BKTR_VRO      0xC	/* odd field to follow */
#define BKTR_PXV      0x0	/* valid word (never used) */
#define BKTR_EOL      0x1	/* last dword, 4 bytes */
#define BKTR_SOL      0x2	/* first dword */

#define OP_WRITE      (0x1 << 28)
#define OP_SKIP       (0x2 << 28)
#define OP_WRITEC     (0x5 << 28)
#define OP_JUMP	      (0x7 << 28)
#define OP_SYNC	      (0x8 << 28)
#define OP_WRITE123   (0x9 << 28)
#define OP_WRITES123  (0xb << 28)
#define OP_SOL	      (1 << 27)		/* first instr for scanline */
#define OP_EOL	      (1 << 26)

bool_t notclipped (bktr_reg_t * bktr, int x, int width) {
    int i;
    bktr_clip_t * clip_node;
    bktr->clip_start = -1;
    bktr->last_y = 0;
    bktr->y = 0;
    bktr->y2 = width;
    bktr->line_length = width;
    bktr->yclip = -1;
    bktr->yclip2 = -1;
    bktr->current_col = 0;
    
    if (bktr->max_clip_node == 0 ) return TRUE;
    clip_node = (bktr_clip_t *) &bktr->clip_list[0];


    for (i = 0; i < bktr->max_clip_node; i++ ) {
	clip_node = (bktr_clip_t *) &bktr->clip_list[i];
	if (x >= clip_node->x_min && x <= clip_node->x_max  ) {
	    bktr->clip_start = i;
	    return FALSE;
	}
    }	
    
    return TRUE;
}	

bool_t getline(bktr_reg_t *bktr, int x ) {
    int i, j;
    bktr_clip_t * clip_node ;
    
    if (bktr->line_length == 0 || 
	bktr->current_col >= bktr->line_length) return FALSE;

    bktr->y = min(bktr->last_y, bktr->line_length);
    bktr->y2 = bktr->line_length;

    bktr->yclip = bktr->yclip2 = -1;
    for (i = bktr->clip_start; i < bktr->max_clip_node; i++ ) {
	clip_node = (bktr_clip_t *) &bktr->clip_list[i];
	if (x >= clip_node->x_min && x <= clip_node->x_max) {
	    if (bktr->last_y <= clip_node->y_min) {
		bktr->y =      min(bktr->last_y, bktr->line_length);
		bktr->y2 =     min(clip_node->y_min, bktr->line_length);
		bktr->yclip =  min(clip_node->y_min, bktr->line_length);
		bktr->yclip2 = min(clip_node->y_max, bktr->line_length);
		bktr->last_y = bktr->yclip2;
		bktr->clip_start = i;
		
		for (j = i+1; j  < bktr->max_clip_node; j++ ) {
		    clip_node = (bktr_clip_t *) &bktr->clip_list[j];
		    if (x >= clip_node->x_min && x <= clip_node->x_max) {
			if (bktr->last_y >= clip_node->y_min) {
			    bktr->yclip2 = min(clip_node->y_max, bktr->line_length);
			    bktr->last_y = bktr->yclip2;
			    bktr->clip_start = j;
			}	
		    } else break  ;
		}	
		return TRUE;
	    }	
	}
    }

    if (bktr->current_col <= bktr->line_length) {
	bktr->current_col = bktr->line_length;
	return TRUE;
    }
    return FALSE;
}
    
static bool_t split(bktr_reg_t * bktr, volatile u_long **dma_prog, int width ,
		    u_long operation, int pixel_width,
		    volatile u_char ** target_buffer, int cols ) {

 u_long flag, flag2;
 struct meteor_pixfmt *pf = &pixfmt_table[ bktr->pixfmt ].public;
 u_int  skip, start_skip;

  /*  For RGB24, we need to align the component in FIFO Byte Lane 0         */
  /*    to the 1st byte in the mem dword containing our start addr.         */
  /*    BTW, we know this pixfmt's 1st byte is Blue; thus the start addr    */
  /*     must be Blue.                                                      */
  start_skip = 0;
  if (( pf->type == METEOR_PIXTYPE_RGB ) && ( pf->Bpp == 3 ))
	  switch ( ((u_long) *target_buffer) % 4 ) {
	  case 2 : start_skip = 4 ; break;
	  case 1 : start_skip = 8 ; break;
	  }

 if ((width * pixel_width) < DMA_BT848_SPLIT ) {
     if (  width == cols) {
	 flag = OP_SOL | OP_EOL;
       } else if (bktr->current_col == 0 ) {
	    flag  = OP_SOL;
       } else if (bktr->current_col == cols) {
	    flag = OP_EOL;
       } else flag = 0;	

     skip = 0;
     if (( flag & OP_SOL ) && ( start_skip > 0 )) {
	     *(*dma_prog)++ = OP_SKIP | OP_SOL | start_skip;
	     flag &= ~OP_SOL;
	     skip = start_skip;
     }

     *(*dma_prog)++ = operation | flag  | (width * pixel_width - skip);
     if (operation != OP_SKIP ) 
	 *(*dma_prog)++ = (u_long)  *target_buffer;

     *target_buffer += width * pixel_width;
     bktr->current_col += width;

 } else {

	if (bktr->current_col == 0 && width == cols) {
	    flag = OP_SOL ;
	    flag2 = OP_EOL;
        } else if (bktr->current_col == 0 ) {
	    flag = OP_SOL;
	    flag2 = 0;
	} else if (bktr->current_col >= cols)  {
	    flag =  0;
	    flag2 = OP_EOL;
	} else {
	    flag =  0;
	    flag2 = 0;
	}

	skip = 0;
	if (( flag & OP_SOL ) && ( start_skip > 0 )) {
		*(*dma_prog)++ = OP_SKIP | OP_SOL | start_skip;
		flag &= ~OP_SOL;
		skip = start_skip;
	}

	*(*dma_prog)++ = operation  | flag |
	      (width * pixel_width / 2 - skip);
	if (operation != OP_SKIP ) 
	      *(*dma_prog)++ = (u_long ) *target_buffer ;
	*target_buffer +=  (width * pixel_width / 2) ;

	if ( operation == OP_WRITE )
		operation = OP_WRITEC;
	*(*dma_prog)++ = operation | flag2 |
	    (width * pixel_width / 2);
	*target_buffer +=  (width * pixel_width / 2) ;
	  bktr->current_col += width;

    }
 return TRUE;
}




static void
rgb_prog( bktr_ptr_t bktr, char i_flag, int cols, int rows, int interlace )
{
	int			i;
	bt848_ptr_t		bt848;
	volatile u_long		target_buffer, buffer, target,width;
	volatile u_long		pitch;
	volatile  u_long	*dma_prog;
        struct meteor_pixfmt_internal *pf_int = &pixfmt_table[ bktr->pixfmt ];
	u_int                   Bpp = pf_int->public.Bpp;

	bt848 = bktr->base;

	bt848->color_fmt         = pf_int->color_fmt;
	bt848->vbi_pack_size     = 0;	    
	bt848->vbi_pack_del      = 0;
	bt848->adc               = SYNC_LEVEL;
	bt848->color_ctl_rgb_ded = 1;

	bt848->e_vscale_hi |= 0xc0;
	bt848->o_vscale_hi |= 0xc0;
	if (cols > 385 ) {
	    bt848->e_vtc = 0;
	    bt848->o_vtc = 0;
	} else {
	    bt848->e_vtc = 1;
	    bt848->o_vtc = 1;
	}
	bktr->capcontrol = 3 << 2 |  3;

	dma_prog = (u_long *) bktr->dma_prog;

	/* Construct Write */

	if (bktr->video.addr) {
		target_buffer = (u_long) bktr->video.addr;
		pitch = bktr->video.width;
	}
	else {
		target_buffer = (u_long) vtophys(bktr->bigbuf);
		pitch = cols*Bpp;
	}

	buffer = target_buffer;


	/* contruct sync : for video packet format */
	*dma_prog++ = OP_SYNC  | 1 << 15 | BKTR_FM1;

	/* sync, mode indicator packed data */
	*dma_prog++ = 0;  /* NULL WORD */
	width = cols;
	for (i = 0; i < (rows/interlace); i++) {
	    target = target_buffer;
	    if ( notclipped(bktr, i, width)) {
		split(bktr, (volatile u_long **) &dma_prog,
		      bktr->y2 - bktr->y, OP_WRITE,
		      Bpp, (volatile u_char **) &target,  cols);

	    } else {
		while(getline(bktr, i)) {
		    if (bktr->y != bktr->y2 ) {
			split(bktr, (volatile u_long **) &dma_prog,
			      bktr->y2 - bktr->y, OP_WRITE,
			      Bpp, (volatile u_char **) &target, cols);
		    }
		    if (bktr->yclip != bktr->yclip2 ) {
			split(bktr,(volatile u_long **) &dma_prog,
			      bktr->yclip2 - bktr->yclip,
			      OP_SKIP,
			      Bpp, (volatile u_char **) &target,  cols);
		    }
		}

	    }

	    target_buffer += interlace * pitch;

	}

	switch (i_flag) {
	case 1:
		/* sync vre */
		*dma_prog++ = OP_SYNC | 1 << 24 | BKTR_VRE;
		*dma_prog++ = 0;  /* NULL WORD */

		*dma_prog++ = OP_JUMP;
		*dma_prog++ = (u_long ) vtophys(bktr->dma_prog);
		return;

	case 2:
		/* sync vro */
		*dma_prog++ = OP_SYNC | 1 << 24 | BKTR_VRO;
		*dma_prog++ = 0;  /* NULL WORD */

		*dma_prog++ = OP_JUMP;
		*dma_prog++ = (u_long ) vtophys(bktr->dma_prog);
		return;

	case 3:
		/* sync vro */
		*dma_prog++ = OP_SYNC | 1 << 24 | 1 << 15 | BKTR_VRO;
		*dma_prog++ = 0;  /* NULL WORD */
		*dma_prog++ = OP_JUMP; ;
		*dma_prog = (u_long ) vtophys(bktr->odd_dma_prog);
		break;
	}

	if (interlace == 2) {

	        target_buffer = buffer + pitch; 

		dma_prog = (u_long *) bktr->odd_dma_prog;


		/* sync vre IRQ bit */
		*dma_prog++ = OP_SYNC |  1 << 15 | BKTR_FM1;
		*dma_prog++ = 0;  /* NULL WORD */
                width = cols;
		for (i = 0; i < (rows/interlace); i++) {
		    target = target_buffer;
		    if ( notclipped(bktr, i, width)) {
			split(bktr, (volatile u_long **) &dma_prog,
			      bktr->y2 - bktr->y, OP_WRITE,
			      Bpp, (volatile u_char **) &target,  cols);
		    } else {
			while(getline(bktr, i)) {
			    if (bktr->y != bktr->y2 ) {
				split(bktr, (volatile u_long **) &dma_prog,
				      bktr->y2 - bktr->y, OP_WRITE,
				      Bpp, (volatile u_char **) &target,
				      cols);
			    }	
			    if (bktr->yclip != bktr->yclip2 ) {
				split(bktr, (volatile u_long **) &dma_prog,
				      bktr->yclip2 - bktr->yclip, OP_SKIP,
				      Bpp, (volatile u_char **)  &target,  cols);
			    }	

			}	

		    }

		    target_buffer += interlace * pitch;

		}
	}

	/* sync vre IRQ bit */
	*dma_prog++ = OP_SYNC |  1 << 24 | 1 << 15 | BKTR_VRE;
	*dma_prog++ = 0;  /* NULL WORD */
	*dma_prog++ = OP_JUMP ;
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
	volatile unsigned int	inst;
	volatile unsigned int	inst3;
	volatile u_long		target_buffer, buffer;
	bt848_ptr_t		bt848;
	volatile  u_long	*dma_prog;
        struct meteor_pixfmt_internal *pf_int = &pixfmt_table[ bktr->pixfmt ];
	int			b;

	bt848 = bktr->base;

	bt848->color_fmt         = pf_int->color_fmt;

	bt848->e_scloop |= BT848_E_SCLOOP_CAGC; /* enable chroma comb */
	bt848->o_scloop |= BT848_O_SCLOOP_CAGC;

	bt848->color_ctl_rgb_ded = 1;
	bt848->color_ctl_gamma = 1;
	bt848->adc = SYNC_LEVEL;

	bktr->capcontrol =   1 << 6 | 1 << 4 | 1 << 2 | 3;
	bktr->capcontrol = 3 << 2 |  3;

	dma_prog = (u_long *) bktr->dma_prog;

	/* Construct Write */
    
	/* write , sol, eol */
	inst = OP_WRITE	 | OP_SOL | (cols);
	/* write , sol, eol */
	inst3 = OP_WRITE | OP_EOL | (cols);

	if (bktr->video.addr)
		target_buffer = (u_long) bktr->video.addr;
	else
		target_buffer = (u_long) vtophys(bktr->bigbuf);

	buffer = target_buffer;

	/* contruct sync : for video packet format */
	/* sync, mode indicator packed data */
	*dma_prog++ = OP_SYNC | 1 << 15 | BKTR_FM1;
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
		*dma_prog++ = OP_SYNC  | 1 << 24 | BKTR_VRE;
		*dma_prog++ = 0;  /* NULL WORD */

		*dma_prog++ = OP_JUMP;
		*dma_prog++ = (u_long ) vtophys(bktr->dma_prog);
		return;

	case 2:
		/* sync vro */
		*dma_prog++ = OP_SYNC  | 1 << 24 | BKTR_VRO;
		*dma_prog++ = 0;  /* NULL WORD */
		*dma_prog++ = OP_JUMP;
		*dma_prog++ = (u_long ) vtophys(bktr->dma_prog);
		return;

	case 3:
		/* sync vro */
		*dma_prog++ = OP_SYNC	 | 1 << 24 | 1 << 15 | BKTR_VRO;
		*dma_prog++ = 0;  /* NULL WORD */
		*dma_prog++ = OP_JUMP  ;
		*dma_prog = (u_long ) vtophys(bktr->odd_dma_prog);
		break;
	}

	if (interlace == 2) {

		target_buffer =	 (u_long) buffer + cols*2;

		dma_prog = (u_long * ) bktr->odd_dma_prog;

		/* sync vre */
		*dma_prog++ = OP_SYNC | 1 << 24 |  1 << 15 | BKTR_FM1;
		*dma_prog++ = 0;  /* NULL WORD */

		for (i = 0; i < (rows/interlace) ; i++) {
			*dma_prog++ = inst;
			*dma_prog++ = target_buffer;
			*dma_prog++ = inst3;
			*dma_prog++ = target_buffer + b;
			target_buffer += interlace * ( cols*2);
		}
	}

	/* sync vro IRQ bit */
	*dma_prog++ = OP_SYNC   |  1 << 24  | 1 << 15 |  BKTR_VRE;
	*dma_prog++ = 0;  /* NULL WORD */
	*dma_prog++ = OP_JUMP ;
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

	int			i;
	volatile unsigned int	inst;
	volatile u_long		target_buffer, t1, buffer;
	bt848_ptr_t		bt848;
	volatile u_long		*dma_prog;
        struct meteor_pixfmt_internal *pf_int = &pixfmt_table[ bktr->pixfmt ];

	bt848 = bktr->base;

	bt848->color_fmt         = pf_int->color_fmt;

	dma_prog = (u_long *) bktr->dma_prog;

	bktr->capcontrol =   1 << 6 | 1 << 4 |	3;

	bt848->adc = SYNC_LEVEL;
	bt848->oform = 0x00;

	bt848->e_control |= BT848_E_CONTROL_LDEC; /* disable luma decimation */
	bt848->o_control |= BT848_O_CONTROL_LDEC;

	bt848->e_scloop |= BT848_O_SCLOOP_CAGC;	/* chroma agc enable */
	bt848->o_scloop |= BT848_O_SCLOOP_CAGC; 

	bt848->e_vscale_hi &= ~0x80; /* clear Ycomb */
	bt848->o_vscale_hi &= ~0x80;
	bt848->e_vscale_hi |= 0x40; /* set chroma comb */
	bt848->o_vscale_hi |= 0x40;

	/* disable gamma correction removal */
	bt848->color_ctl_gamma = 1;

	/* Construct Write */
	inst  = OP_WRITE123  | OP_SOL | OP_EOL |  (cols); 
	if (bktr->video.addr)
		target_buffer = (u_long) bktr->video.addr;
	else
		target_buffer = (u_long) vtophys(bktr->bigbuf);
    
	buffer = target_buffer;

	t1 = buffer;

	/* contruct sync : for video packet format */
	*dma_prog++ = OP_SYNC  | 1 << 15 |	BKTR_FM3; /*sync, mode indicator packed data*/
	*dma_prog++ = 0;  /* NULL WORD */

	for (i = 0; i < (rows/interlace ) ; i++) {
		*dma_prog++ = inst;
		*dma_prog++ = cols/2 | cols/2 << 16;
		*dma_prog++ = target_buffer;
		*dma_prog++ = t1 + (cols*rows) + i*cols/2 * interlace;
		*dma_prog++ = t1 + (cols*rows) + (cols*rows/2) + i*cols/2 * interlace;
		target_buffer += interlace*cols;
	}

	switch (i_flag) {
	case 1:
		*dma_prog++ = OP_SYNC  | 1 << 24 | BKTR_VRE;  /*sync vre*/
		*dma_prog++ = 0;  /* NULL WORD */

		*dma_prog++ = OP_JUMP ;
		*dma_prog++ = (u_long ) vtophys(bktr->dma_prog);
		return;

	case 2:
		*dma_prog++ = OP_SYNC  | 1 << 24 | BKTR_VRO;  /*sync vre*/
		*dma_prog++ = 0;  /* NULL WORD */

		*dma_prog++ = OP_JUMP;
		*dma_prog++ = (u_long ) vtophys(bktr->dma_prog);
		return;

	case 3:
		*dma_prog++ = OP_SYNC	| 1 << 24 |  1 << 15 |   BKTR_VRO; 
		*dma_prog++ = 0;  /* NULL WORD */

		*dma_prog++ = OP_JUMP  ;
		*dma_prog = (u_long ) vtophys(bktr->odd_dma_prog);
		break;
	}

	if (interlace == 2) {

		dma_prog = (u_long * ) bktr->odd_dma_prog;

		target_buffer  = (u_long) buffer + cols;
		t1 = buffer + cols/2;
		*dma_prog++ = OP_SYNC	|   1 << 15 | BKTR_FM3; 
		*dma_prog++ = 0;  /* NULL WORD */

		for (i = 0; i < (rows/interlace )  ; i++) {
			*dma_prog++ = inst;
			*dma_prog++ = cols/2 | cols/2 << 16;
			*dma_prog++ = target_buffer;
			*dma_prog++ = t1 + (cols*rows) + i*cols/2 * interlace;
			*dma_prog++ = t1 + (cols*rows) + (cols*rows/2) + i*cols/2 * interlace;
			target_buffer += interlace*cols;
		}
	}
    
	*dma_prog++ = OP_SYNC  | 1 << 24 | 1 << 15 |   BKTR_VRE; 
	*dma_prog++ = 0;  /* NULL WORD */
	*dma_prog++ = OP_JUMP ;
	*dma_prog++ = (u_long ) vtophys(bktr->dma_prog) ;
	*dma_prog++ = 0;  /* NULL WORD */
}


/*
 * 
 */
static void
yuv12_prog( bktr_ptr_t bktr, char i_flag,
	     int cols, int rows, int interlace ){

	int			i;
	volatile unsigned int	inst;
	volatile unsigned int	inst1;
	volatile u_long		target_buffer, t1, buffer;
	bt848_ptr_t		bt848;
	volatile u_long		*dma_prog;
        struct meteor_pixfmt_internal *pf_int = &pixfmt_table[ bktr->pixfmt ];

	bt848 = bktr->base;

	bt848->color_fmt         = pf_int->color_fmt;

	dma_prog = (u_long *) bktr->dma_prog;

	bktr->capcontrol =   1 << 6 | 1 << 4 |	3;

	bt848->adc = SYNC_LEVEL;
	bt848->oform = 0x00;

	bt848->e_control |= BT848_E_CONTROL_LDEC; /* disable luma decimation */
	bt848->o_control |= BT848_O_CONTROL_LDEC;

	bt848->e_scloop |= BT848_O_SCLOOP_CAGC;	/* chroma agc enable */
	bt848->o_scloop |= BT848_O_SCLOOP_CAGC; 

        bt848->e_vscale_hi &= ~0x80; /* clear Ycomb */
 	bt848->o_vscale_hi &= ~0x80;
 	bt848->e_vscale_hi |= 0x40; /* set chroma comb */
 	bt848->o_vscale_hi |= 0x40;
 
 	/* disable gamma correction removal */
 	bt848->color_ctl_gamma = 1;
 
	/* Construct Write */
 	inst  = OP_WRITE123  | OP_SOL | OP_EOL |  (cols); 
 	inst1  = OP_WRITES123  | OP_SOL | OP_EOL |  (cols); 
 	if (bktr->video.addr)
 		target_buffer = (u_long) bktr->video.addr;
 	else
 		target_buffer = (u_long) vtophys(bktr->bigbuf);
     
	buffer = target_buffer;
 	t1 = buffer;
 
 	*dma_prog++ = OP_SYNC  | 1 << 15 |	BKTR_FM3; /*sync, mode indicator packed data*/
 	*dma_prog++ = 0;  /* NULL WORD */
 	       
 	for (i = 0; i < (rows/interlace )/2 ; i++) {
		*dma_prog++ = inst;
 		*dma_prog++ = cols/2 | (cols/2 << 16);
 		*dma_prog++ = target_buffer;
 		*dma_prog++ = t1 + (cols*rows) + i*cols/2 * interlace;
 		*dma_prog++ = t1 + (cols*rows) + (cols*rows/4) + i*cols/2 * interlace;
 		target_buffer += interlace*cols;
 		*dma_prog++ = inst1;
 		*dma_prog++ = cols/2 | (cols/2 << 16);
 		*dma_prog++ = target_buffer;
 		target_buffer += interlace*cols;
 
 	}
 
 	switch (i_flag) {
 	case 1:
 		*dma_prog++ = OP_SYNC  | 1 << 24 | BKTR_VRE;  /*sync vre*/
 		*dma_prog++ = 0;  /* NULL WORD */

		*dma_prog++ = OP_JUMP;
		*dma_prog++ = (u_long ) vtophys(bktr->dma_prog);
 		return;

 	case 2:
 		*dma_prog++ = OP_SYNC  | 1 << 24 | BKTR_VRO;  /*sync vro*/
 		*dma_prog++ = 0;  /* NULL WORD */

		*dma_prog++ = OP_JUMP;
		*dma_prog++ = (u_long ) vtophys(bktr->dma_prog);
 		return;
 
 	case 3:
 		*dma_prog++ = OP_SYNC |  1 << 24 | 1 << 15 | BKTR_VRO;
		*dma_prog++ = 0;  /* NULL WORD */
		*dma_prog++ = OP_JUMP ;
		*dma_prog = (u_long ) vtophys(bktr->odd_dma_prog);
		break;
	}

	if (interlace == 2) {

		dma_prog = (u_long * ) bktr->odd_dma_prog;

		target_buffer  = (u_long) buffer + cols;
		t1 = buffer + cols/2;
		*dma_prog++ = OP_SYNC   | 1 << 15 | BKTR_FM3; 
		*dma_prog++ = 0;  /* NULL WORD */

		for (i = 0; i < ((rows/interlace )/2 ) ; i++) {
		    *dma_prog++ = inst;
		    *dma_prog++ = cols/2 | (cols/2 << 16);
         	    *dma_prog++ = target_buffer;
		    *dma_prog++ = t1 + (cols*rows) + i*cols/2 * interlace;
		    *dma_prog++ = t1 + (cols*rows) + (cols*rows/4) + i*cols/2 * interlace;
		    target_buffer += interlace*cols;
		    *dma_prog++ = inst1;
		    *dma_prog++ = cols/2 | (cols/2 << 16);
		    *dma_prog++ = target_buffer;
		    target_buffer += interlace*cols;

		}	

	
	}
    
	*dma_prog++ = OP_SYNC |  1 << 24 | 1 << 15 | BKTR_VRE;
	*dma_prog++ = 0;  /* NULL WORD */
	*dma_prog++ = OP_JUMP;
	*dma_prog++ = (u_long ) vtophys(bktr->dma_prog);
	*dma_prog++ = 0;  /* NULL WORD */
}
  


/*
 * 
 */
static void
build_dma_prog( bktr_ptr_t bktr, char i_flag )
{
	int			rows, cols,  interlace;
	bt848_ptr_t		bt848;
	int			tmp_int;
	unsigned int		temp;	
	struct format_params	*fp;
        struct meteor_pixfmt_internal *pf_int = &pixfmt_table[ bktr->pixfmt ];
	

	fp = &format_params[bktr->format_params];
	
	bt848 = bktr->base;
	bt848->int_mask = ALL_INTS_DISABLED;

	/* disable FIFO & RISC, leave other bits alone */
	bt848->gpio_dma_ctl &= ~FIFO_RISC_ENABLED;

	/* set video parameters */
	temp = ((quad_t ) fp->htotal* (quad_t) fp->horizontal * 4096
		  / fp->vertical / bktr->cols) -  4096;
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
	temp = (fp->hdelay * bktr->cols) / fp->hactive;
	temp = temp & 0x3fe;
	bt848->e_delay_lo = temp & 0xff;
	bt848->o_delay_lo = temp & 0xff;
	bt848->e_crop &= ~0xc;
	bt848->o_crop &= ~0xc;
	bt848->e_crop |= (temp >> 6) & 0xc;
	bt848->o_crop |= (temp >> 6) & 0xc;
  
	/* vertical scale */

	if (bktr->flags  & METEOR_ONLY_ODD_FIELDS ||
	    bktr->flags & METEOR_ONLY_EVEN_FIELDS)
	  tmp_int = 65536 -
	    (((fp->vactive  * 256 + (bktr->rows/2)) / bktr->rows) - 512);
	else {
	  tmp_int = 65536  -
	    (((fp->vactive * 512 + (bktr->rows / 2)) /  bktr->rows) - 512);
	}
  
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

	/* end of video params */

	/* capture control */
	switch (i_flag) {
	case 1:
	        bktr->bktr_cap_ctl = 
		    (BT848_CAP_CTL_DITH_FRAME | BT848_CAP_CTL_EVEN);
		bt848->e_vscale_hi &= ~0x20;
		bt848->o_vscale_hi &= ~0x20;
		interlace = 1;
		break;
	 case 2:
 	        bktr->bktr_cap_ctl =
			(BT848_CAP_CTL_DITH_FRAME | BT848_CAP_CTL_ODD);
		bt848->e_vscale_hi &= ~0x20;
		bt848->o_vscale_hi &= ~0x20;
		interlace = 1;
		break;
	 default:
 	        bktr->bktr_cap_ctl = 
			(BT848_CAP_CTL_DITH_FRAME |
			 BT848_CAP_CTL_EVEN | BT848_CAP_CTL_ODD);
		bt848->e_vscale_hi |= 0x20;
		bt848->o_vscale_hi |= 0x20;
		interlace = 2;
		break;
	}

	bt848->risc_strt_add = vtophys(bktr->dma_prog);

	rows = bktr->rows;
	cols = bktr->cols;

	if ( pf_int->public.type == METEOR_PIXTYPE_RGB ) {
		rgb_prog(bktr, i_flag, cols, rows, interlace);
		return;
	}

	if ( pf_int->public.type  == METEOR_PIXTYPE_YUV ) {
		yuv422_prog(bktr, i_flag, cols, rows, interlace);
		bt848->color_ctl_swap = pixfmt_swap_flags( bktr->pixfmt );
		return;
	}

	if ( pf_int->public.type  == METEOR_PIXTYPE_YUV_PACKED ) {
		yuvpack_prog(bktr, i_flag, cols, rows, interlace);
		bt848->color_ctl_swap = pixfmt_swap_flags( bktr->pixfmt );
		return;
	}

	if ( pf_int->public.type  == METEOR_PIXTYPE_YUV_12 ) {
		yuv12_prog(bktr, i_flag, cols, rows, interlace);
		bt848->color_ctl_swap = pixfmt_swap_flags( bktr->pixfmt );
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
	struct format_params   *fp;

	fp = &format_params[bktr->format_params];

	bt848 = bktr->base;

	bt848->dstatus = 0;
	bt848->int_stat = bt848->int_stat;

	bktr->flags |= type;
	bktr->flags &= ~METEOR_WANT_MASK;
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

	/*  TDEC is only valid for continuous captures  */
	if ( type == METEOR_SINGLE ) {
		u_short	fps_save = bktr->fps;

		set_fps(bktr, fp->frame_rate);
		bktr->fps = fps_save;
	}
	else
		set_fps(bktr, bktr->fps);

	if (bktr->dma_prog_loaded == FALSE) {
		build_dma_prog(bktr, i_flag);
		bktr->dma_prog_loaded = TRUE;
	}
	

	bt848->risc_strt_add = vtophys(bktr->dma_prog);

}


/*
 * 
 */
static void
set_fps( bktr_ptr_t bktr, u_short fps )
{
	bt848_ptr_t	bt848;
	struct format_params	*fp;
	int i_flag;

	fp = &format_params[bktr->format_params];

	bt848 = bktr->base;

	switch(bktr->flags & METEOR_ONLY_FIELDS_MASK) {
	case METEOR_ONLY_EVEN_FIELDS:
		bktr->flags |= METEOR_WANT_EVEN;
		i_flag = 1;
		break;
	case METEOR_ONLY_ODD_FIELDS:
		bktr->flags |= METEOR_WANT_ODD;
		i_flag = 1;
		break;
	default:
		bktr->flags |= METEOR_WANT_MASK;
		i_flag = 2;
		break;
	}

	bt848->gpio_dma_ctl = FIFO_RISC_DISABLED;
	bt848->int_stat = ALL_INTS_CLEARED;

	bktr->fps = fps;
	bt848->tdec = 0;

	if (fps < fp->frame_rate)
		bt848->tdec = i_flag*(fp->frame_rate - fps) & 0x3f;
	else
		bt848->tdec = 0;
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
		printf("bktr%d: Unable to allocate %d bytes of memory.\n",
			unit, size);
	}

	return( addr );
}



/* 
 * Given a pixfmt index, compute the bt848 swap_flags necessary to 
 *   achieve the specified swapping.
 * Note that without bt swapping, 2Bpp and 3Bpp modes are written 
 *   byte-swapped, and 4Bpp modes are byte and word swapped (see Table 6 
 *   and read R->L).  
 * Note also that for 3Bpp, we may additionally need to do some creative 
 *   SKIPing to align the FIFO bytelines with the target buffer (see split()).
 * This is abstracted here: e.g. no swaps = RGBA; byte & short swap = ABGR
 *   as one would expect.
 */

static u_int pixfmt_swap_flags( int pixfmt )
{
	struct meteor_pixfmt *pf = &pixfmt_table[ pixfmt ].public;
	u_int		      swapf = 0;

	switch ( pf->Bpp ) {
	case 2 : swapf = ( pf->swap_bytes ? 0 : BSWAP );
		 break;

	case 3 : /* no swaps supported for 3bpp - makes no sense w/ bt848 */
		 break;
		 
	case 4 : if ( pf->swap_bytes )
			swapf = pf->swap_shorts ? 0 : WSWAP;
		 else
			swapf = pf->swap_shorts ? BSWAP : (BSWAP | WSWAP);
		 break;
	}
	return swapf;
}



/* 
 * Converts meteor-defined pixel formats (e.g. METEOR_GEO_RGB16) into
 *   our pixfmt_table indices.
 */

static int oformat_meteor_to_bt( u_long format )
{
	int    i;
        struct meteor_pixfmt *pf1, *pf2;

	/*  Find format in compatibility table  */
	for ( i = 0; i < METEOR_PIXFMT_TABLE_SIZE; i++ )
		if ( meteor_pixfmt_table[i].meteor_format == format )
			break;

	if ( i >= METEOR_PIXFMT_TABLE_SIZE )
		return -1;
	pf1 = &meteor_pixfmt_table[i].public;

	/*  Match it with an entry in master pixel format table  */
	for ( i = 0; i < PIXFMT_TABLE_SIZE; i++ ) {
		pf2 = &pixfmt_table[i].public;

		if (( pf1->type        == pf2->type        ) &&
		    ( pf1->Bpp         == pf2->Bpp         ) &&
		    !memcmp( pf1->masks, pf2->masks, sizeof( pf1->masks )) &&
		    ( pf1->swap_bytes  == pf2->swap_bytes  ) &&
		    ( pf1->swap_shorts == pf2->swap_shorts )) 
			break;
	}
	if ( i >= PIXFMT_TABLE_SIZE )
		return -1;

	return i;
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
static const struct CARDTYPE cards[] = {

	/* CARD_UNKNOWN */
	{ "Unknown",				/* the 'name' */
	   NULL,				/* the tuner */
	   0,					/* dbx unknown */
	   0,
	   0,					/* EEProm unknown */
	   0,					/* EEProm unknown */
	   { 0, 0, 0, 0, 0 } },

	/* CARD_MIRO */
	{ "Miro TV",				/* the 'name' */
	   NULL,				/* the tuner */
	   0,					/* dbx unknown */
	   0,
	   0,					/* EEProm unknown */
	   0,					/* size unknown */
	   { 0x02, 0x01, 0x00, 0x0a, 1 } },	/* XXX ??? */

	/* CARD_HAUPPAUGE */
	{ "Hauppauge WinCast/TV",		/* the 'name' */
	   NULL,				/* the tuner */
	   0,					/* dbx is optional */
	   0,
	   PFC8582_WADDR,			/* EEProm type */
	   (u_char)(256 / EEPROMBLOCKSIZE),	/* 256 bytes */
	   { 0x00, 0x02, 0x01, 0x01, 1 } },	/* audio MUX values */

	/* CARD_STB */
	{ "STB TV/PCI",				/* the 'name' */
	   NULL,				/* the tuner */
	   0,					/* dbx is optional */
	   0,
	   X24C01_WADDR,			/* EEProm type */
	   (u_char)(128 / EEPROMBLOCKSIZE),	/* 128 bytes */
	   { 0x00, 0x01, 0x02, 0x02, 1 } },	/* audio MUX values */

	/* CARD_INTEL */
	{ "Intel Smart Video III/VideoLogic Captivator PCI",		/* the 'name' */
	   NULL,				/* the tuner */
	   0,
	   0,
	   0,
	   0,
	   { 0, 0, 0, 0, 0 } }
};


/*
 * the data for each type of tuner
 *
 * if probeCard() fails to detect the proper tuner on boot you can
 * override it by setting the following define to the tuner present:
 *
#define OVERRIDE_TUNER	<tuner type>
 *
 * where <tuner type> is one of the following tuner defines.
 */

/* indexes into tuners[] */
#define NO_TUNER		0
#define TEMIC_NTSC		1
#define TEMIC_PAL		2
#define TEMIC_SECAM		3
#define PHILIPS_NTSC		4
#define PHILIPS_PAL		5
#define PHILIPS_SECAM		6
#define TEMIC_PALI		7
#define PHILIPS_PALI		8
#define PHILIPS_FR1236_NTSC     9
#define PHILIPS_FR1216_PAL	10

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
static const struct TUNER tuners[] = {
/* XXX FIXME: fill in the band-switch crosspoints */
	/* NO_TUNER */
	{ "<none>",				/* the 'name' */
	   TTYPE_XXX,				/* input type */
  	   0x00,				/* PLL write address */
 	   { 0x00,				/* control byte for PLL */
 	     0x00,
 	     0x00,
 	     0x00 },
	   { 0x00, 0x00 },			/* band-switch crosspoints */
	   { 0x00, 0x00, 0x00,0x00} },		/* the band-switch values */

	/* TEMIC_NTSC */
	{ "Temic NTSC",				/* the 'name' */
	   TTYPE_NTSC,				/* input type */
	   TEMIC_NTSC_WADDR,			/* PLL write address */
	   { TSA552x_SCONTROL,			/* control byte for PLL */
	     TSA552x_SCONTROL,
	     TSA552x_SCONTROL,
	     0x00 },
	   { 0x00, 0x00 },			/* band-switch crosspoints */
	   { 0x02, 0x04, 0x01, 0x00 } },	/* the band-switch values */

	/* TEMIC_PAL */
	{ "Temic PAL",				/* the 'name' */
	   TTYPE_PAL,				/* input type */
	   TEMIC_PALI_WADDR,			/* PLL write address */
	   { TSA552x_SCONTROL,			/* control byte for PLL */
	     TSA552x_SCONTROL,
	     TSA552x_SCONTROL,
	     0x00 },
	   { 0x00, 0x00 },			/* band-switch crosspoints */
	   { 0x02, 0x04, 0x01, 0x00 } },	/* the band-switch values */

	/* TEMIC_SECAM */
	{ "Temic SECAM",			/* the 'name' */
	   TTYPE_SECAM,				/* input type */
	   0x00,				/* PLL write address */
	   { TSA552x_SCONTROL,			/* control byte for PLL */
	     TSA552x_SCONTROL,
	     TSA552x_SCONTROL,
	     0x00 },
	   { 0x00, 0x00 },			/* band-switch crosspoints */
	   { 0x02, 0x04, 0x01,0x00 } },		/* the band-switch values */

	/* PHILIPS_NTSC */
	{ "Philips NTSC",			/* the 'name' */
	   TTYPE_NTSC,				/* input type */
	   PHILIPS_NTSC_WADDR,			/* PLL write address */
	   { TSA552x_SCONTROL,			/* control byte for PLL */
	     TSA552x_SCONTROL,
	     TSA552x_SCONTROL,
	     0x00 },
	   { 0x00, 0x00 },			/* band-switch crosspoints */
	   { 0xa0, 0x90, 0x30, 0x00 } },	/* the band-switch values */

	/* PHILIPS_PAL */
	{ "Philips PAL",			/* the 'name' */
	   TTYPE_PAL,				/* input type */
  	   PHILIPS_PAL_WADDR,			/* PLL write address */
	   { TSA552x_FCONTROL,			/* control byte for PLL */
	     TSA552x_FCONTROL,
	     TSA552x_FCONTROL,
	     TSA552x_RADIO },
	   { 0x00, 0x00 },			/* band-switch crosspoints */
	   { 0xa0, 0x90, 0x30, 0xa4 } },	/* the band-switch values */

	/* PHILIPS_SECAM */
	{ "Philips SECAM",			/* the 'name' */
	   TTYPE_SECAM,				/* input type */
	   0x00,				/* PLL write address */
	   { TSA552x_SCONTROL,			/* control byte for PLL */
	     TSA552x_SCONTROL,
	     TSA552x_SCONTROL,
	    TSA552x_RADIO },
	   { 0x00, 0x00 },			/* band-switch crosspoints */
	   { 0xa0, 0x90, 0x30,0xa4 } },		/* the band-switch values */

	/* TEMIC_PAL I */
	{ "Temic PAL I",			/* the 'name' */
	   TTYPE_PAL,				/* input type */
	   TEMIC_PALI_WADDR,			/* PLL write address */
	   { TSA552x_SCONTROL,			/* control byte for PLL */
	     TSA552x_SCONTROL,
	     TSA552x_SCONTROL,
	     0x00 },
	   { 0x00, 0x00 },			/* band-switch crosspoints */
	   { 0x02, 0x04, 0x01,0x00 } },		/* the band-switch values */

	/* PHILIPS_PAL */
	{ "Philips PAL I",			/* the 'name' */
	   TTYPE_PAL,				/* input type */
  	   TEMIC_PALI_WADDR,			/* PLL write address */
	   { TSA552x_SCONTROL,			/* control byte for PLL */
	     TSA552x_SCONTROL,
	     TSA552x_SCONTROL,
	     0x00 },
          { 0x00, 0x00 },                      /* band-switch crosspoints */
          { 0xa0, 0x90, 0x30,0x00 } },         /* the band-switch values */

       /* PHILIPS_FR1236_NTSC */
       { "Philips FR1236 NTSC FM",             /* the 'name' */
          TTYPE_NTSC,                          /* input type */
          PHILIPS_FR1236_NTSC_WADDR,           /* PLL write address */
	  { TSA552x_SCONTROL,			/* control byte for PLL */
	    TSA552x_SCONTROL,
	    TSA552x_SCONTROL,
	    0x00},
          { 0x00, 0x00 },			/* band-switch crosspoints */
	  { 0xa0, 0x90, 0x30,0x00 } },		/* the band-switch values */

	/* PHILIPS_FR1216_PAL */
	{ "Philips FR1216 PAL FM",		/* the 'name' */
	   TTYPE_PAL,				/* input type */
  	   PHILIPS_FR1216_PAL_WADDR,		/* PLL write address */
	   { TSA552x_FCONTROL,			/* control byte for PLL */
	     TSA552x_FCONTROL,
	     TSA552x_FCONTROL,
	     TSA552x_RADIO },
	   { 0x00, 0x00 },			/* band-switch crosspoints */
	   { 0xa0, 0x90, 0x30, 0xa4 } },	/* the band-switch values */
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

/*
 * any_i2c_devices.
 * Some BT848/BT848A cards have no tuner and no additional i2c devices
 * eg stereo decoder. These are used for video conferencing or capture from
 * a video camera. (VideoLogic Captivator PCI, Intel SmartCapture card).
 *
 * Determine if there are any i2c devices present. There are none present if
 *  a) reading from all 128 devices returns ABSENT (-1) for each one
 *     (eg VideoLogic Captivator PCI with BT848)
 *  b) reading from all 128 devices returns 0 for each one
 *     (eg VideoLogic Captivator PCI rev. 2F with BT848A)
 */
static int check_for_i2c_devices( bktr_ptr_t bktr ){
  int x, temp_read;
  int i2c_all_0 = 1;
  int i2c_all_absent = 1;
  for ( x = 0; x < 128; ++x ) {
	 temp_read = i2cRead( bktr, (2 * x) + 1 );
	  if (temp_read != 0)      i2c_all_0 = 0;
     if (temp_read != ABSENT) i2c_all_absent = 0;
  }

  if ((i2c_all_0) || (i2c_all_absent)) return 0;
  else return 1;
}
#undef ABSENT

/*
 * determine the card brand/model
 * OVERRIDE_CARD, OVERRIDE_TUNER, OVERRIDE_DBX and OVERRIDE_MSP
 * can be used to select a specific device, regardless of the
 * autodetection and i2c device checks.
 */
#define ABSENT		(-1)
static void
probeCard( bktr_ptr_t bktr, int verbose )
{
	int	card;
	int	status;
	bt848_ptr_t	bt848;
   int   any_i2c_devices;

   any_i2c_devices = check_for_i2c_devices( bktr );
	bt848 = bktr->base;

	bt848->gpio_out_en = 0;
	if (bootverbose)
	    printf("bktr: GPIO is 0x%08x\n", bt848->gpio_data);

#if defined( OVERRIDE_CARD )
	bktr->card = cards[ (card = OVERRIDE_CARD) ];
	goto checkTuner;
#endif

   /* Check for i2c devices */
	if (!any_i2c_devices) {
		bktr->card = cards[ (card = CARD_INTEL) ];
		goto checkTuner;
	}

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
#if defined( OVERRIDE_TUNER )
	bktr->card.tuner = &tuners[ OVERRIDE_TUNER ];
	goto checkDBX;
#endif

   /* Check for i2c devices */
	if (!any_i2c_devices) {
		bktr->card.tuner = &tuners[ NO_TUNER ];
		goto checkDBX;
	}

	/* differentiate type of tuner */
	switch (card) {
	case CARD_MIRO:
	    switch (((bt848->gpio_data >> 10)-1)&7) {
	    case 0: bktr->card.tuner = &tuners[ TEMIC_PAL ]; break;
	    case 1: bktr->card.tuner = &tuners[ PHILIPS_PAL ]; break;
	    case 2: bktr->card.tuner = &tuners[ PHILIPS_NTSC ]; break;
	    case 3: bktr->card.tuner = &tuners[ PHILIPS_SECAM ]; break;
	    case 4: bktr->card.tuner = &tuners[ NO_TUNER ]; break;
	    case 5: bktr->card.tuner = &tuners[ PHILIPS_PALI ]; break;
	    case 6: bktr->card.tuner = &tuners[ TEMIC_NTSC ]; break;
	    case 7: bktr->card.tuner = &tuners[ TEMIC_PALI ]; break;
	    }
	    break;
	default:
	    if ( i2cRead( bktr, TEMIC_NTSC_RADDR ) != ABSENT ) {
		bktr->card.tuner = &tuners[ TEMIC_NTSC ];
		goto checkDBX;
	    }

	    if ( i2cRead( bktr, PHILIPS_NTSC_RADDR ) != ABSENT ) {
		bktr->card.tuner = &tuners[ PHILIPS_NTSC ];
		goto checkDBX;
	    }

	    if ( card == CARD_HAUPPAUGE ) {
		if ( i2cRead( bktr, TEMIC_PALI_RADDR ) != ABSENT ) {
		    bktr->card.tuner = &tuners[ TEMIC_PAL ];
		    goto checkDBX;
		}
	    }
	    /* no tuner found */
	    bktr->card.tuner = &tuners[ NO_TUNER ];
	}

checkDBX:
#if defined( OVERRIDE_DBX )
	bktr->card.dbx = OVERRIDE_DBX;
	goto checkMSP;
#endif
   /* Check for i2c devices */
	if (!any_i2c_devices) {
		goto checkMSP;
	}

	/* probe for BTSC (dbx) chip */
	if ( i2cRead( bktr, TDA9850_RADDR ) != ABSENT )
		bktr->card.dbx = 1;

checkMSP:
#if defined( OVERRIDE_MSP )
	bktr->card.msp3400c = OVERRIDE_MSP;
	goto checkEnd;
#endif
   /* Check for i2c devices */
	if (!any_i2c_devices) {
		goto checkEnd;
	}

	if ( i2cRead( bktr, MSP3400C_RADDR ) != ABSENT )
		bktr->card.msp3400c = 1;

checkEnd:

	if ( verbose ) {
		printf( "%s", bktr->card.name );
		if ( bktr->card.tuner )
			printf( ", %s tuner", bktr->card.tuner->name );
		if ( bktr->card.dbx )
			printf( ", dbx stereo" );
		if ( bktr->card.msp3400c )
			printf( ", msp3400c stereo" );
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
static int nabcst[] = {
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
static int irccable[] = {
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
 *
 * 2:   54 mHz  - 4:    66 mHz
 * 5:   78 mHz  - 6:    84 mHz
 * 7:  174 mHz  - 13:  210 mHz
 * 14: 120 mHz  - 22:  168 mHz
 * 23: 216 mHz  - 94:  642 mHz
 * 95:  90 mHz  - 99:  114 mHz
 *
 * IF freq: 45.75 mHz
 */
#define OFFSET  6.00
static int hrccable[] = {
	99,	(int)( 45.75 * FREQFACTOR),     0,
	95,	(int)( 90.00 * FREQFACTOR),	(int)(OFFSET * FREQFACTOR),
	23,	(int)(216.00 * FREQFACTOR),	(int)(OFFSET * FREQFACTOR),
	14,	(int)(120.00 * FREQFACTOR),	(int)(OFFSET * FREQFACTOR),
	7,	(int)(174.00 * FREQFACTOR),	(int)(OFFSET * FREQFACTOR),
	5,	(int)( 78.00 * FREQFACTOR),	(int)(OFFSET * FREQFACTOR),
	2,	(int)( 54.00 * FREQFACTOR),	(int)(OFFSET * FREQFACTOR),
	0
};
#undef OFFSET

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
 *
 * Channels S21 - S41 are taken from
 * http://gemma.apple.com:80/dev/technotes/tn/tn1012.html
 *
 * 100	30325	800	S21
 * 101	31125	800	S22
 * 102	31925	800	S23
 * 103	32725	800	S24
 * 104	33525	800	S25
 * 105	34325	800	S26         
 * 106	35125	800	S27         
 * 107	35925	800	S28         
 * 108	36725	800	S29         
 * 109	37525	800	S30         
 * 110	38325	800	S31         
 * 111	39125	800	S32         
 * 112	39925	800	S33         
 * 113	40725	800	S34         
 * 114	41525	800	S35         
 * 115	42325	800	S36         
 * 116	43125	800	S37         
 * 117	43925	800	S38         
 * 118	44725	800	S39         
 * 119	45525	800	S40         
 * 120	46325	800	S41
 * 
 * 121	 3890	000	IFFREQ
 * 
 */
static int weurope[] = {
       121,     (int)( 38.90 * FREQFACTOR),     0, 
       100,     (int)(303.25 * FREQFACTOR),     (int)(8.00 * FREQFACTOR), 
        90,     (int)(231.25 * FREQFACTOR),     (int)(7.00 * FREQFACTOR),
        80,     (int)(105.25 * FREQFACTOR),     (int)(7.00 * FREQFACTOR),  
        74,     (int)( 69.25 * FREQFACTOR),     (int)(7.00 * FREQFACTOR),  
        21,     (int)(471.25 * FREQFACTOR),     (int)(8.00 * FREQFACTOR),
        17,     (int)(183.25 * FREQFACTOR),     (int)(9.00 * FREQFACTOR),
        16,     (int)(175.25 * FREQFACTOR),     (int)(9.00 * FREQFACTOR),
        15,     (int)(82.25 * FREQFACTOR),      (int)(8.50 * FREQFACTOR),
        13,     (int)(53.75 * FREQFACTOR),      (int)(8.50 * FREQFACTOR),
         5,     (int)(175.25 * FREQFACTOR),     (int)(7.00 * FREQFACTOR),
         2,     (int)(48.25 * FREQFACTOR),      (int)(7.00 * FREQFACTOR),
	 0
};

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
static int jpnbcst[] = {
	62,     (int)(IF_FREQ * FREQFACTOR),    0,
	13,     (int)(471.25 * FREQFACTOR),     (int)(OFFSET * FREQFACTOR),
	 8,     (int)(193.25 * FREQFACTOR),     (int)(OFFSET * FREQFACTOR),
	 4,     (int)(171.25 * FREQFACTOR),     (int)(OFFSET * FREQFACTOR),
	 1,     (int)( 91.25 * FREQFACTOR),     (int)(OFFSET * FREQFACTOR),
	 0
};
#undef IF_FREQ
#undef OFFSET

/*
 * Japanese Cable Channels:
 *
 *  1:  91.25MHz -  3: 103.25MHz
 *  4: 171.25MHz -  7: 189.25MHz
 *  8: 193.25MHz - 12: 217.25MHz
 * 13: 109.25MHz - 21: 157.25MHz
 * 22: 165.25MHz
 * 23: 223.25MHz - 63: 463.25MHz
 *
 * IF freq: 45.75 mHz
 */
#define OFFSET  6.00
#define IF_FREQ 45.75
static int jpncable[] = {
	63,     (int)(IF_FREQ * FREQFACTOR),    0,
	23,     (int)(223.25 * FREQFACTOR),     (int)(OFFSET * FREQFACTOR),
	22,     (int)(165.25 * FREQFACTOR),     (int)(OFFSET * FREQFACTOR),
	13,     (int)(109.25 * FREQFACTOR),     (int)(OFFSET * FREQFACTOR),
	 8,     (int)(193.25 * FREQFACTOR),     (int)(OFFSET * FREQFACTOR),
	 4,     (int)(171.25 * FREQFACTOR),     (int)(OFFSET * FREQFACTOR),
	 1,     (int)( 91.25 * FREQFACTOR),     (int)(OFFSET * FREQFACTOR),
	 0
};
#undef IF_FREQ
#undef OFFSET

static int* freqTable[] = {
	NULL,
	nabcst,
	irccable,
	hrccable,
	weurope,
	jpnbcst,
	jpncable
	
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

	tuner = bktr->card.tuner;
	if ( tuner == NULL )
		return( -1 );

	/*
	 * select the band based on frequency
	 * XXX FIXME: get the cross-over points from the tuner struct
	 */
	if ( frequency < (160 * FREQFACTOR) )
	  N = 0;
	else if ( frequency < (454 * FREQFACTOR) )
	  N = 1;
	else
	  N = 2;

	if(frequency > RADIO_OFFSET) {
	  N=3;
	  frequency -= RADIO_OFFSET;
	}
  
	/* set the address of the PLL */
	addr    = tuner->pllAddr;
	control = tuner->pllControl[ N ];
	band    = tuner->bandAddrs[ N ];
	if(!(band && control))			/* Don't try to set un-	*/
	  return(-1);				/* supported modes.	*/

         if(N==3) 
	  band |= bktr->tuner.radio_mode;

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
#ifdef BKTR_REVERSEMUTE
		bktr->audio_mux_select = 3;
#else
		bktr->audio_mux_select = 0;
#endif
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
#ifdef BKTR_REVERSEMUTE
		idx = 0;
#else
		idx = 3;
#endif
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


#ifdef __FreeBSD__
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

#endif  /* __FreeBSD__ */
#endif /* !defined(__FreeBSD__) || (NBKTR > 0 && NPCI > 0) */

/* Local Variables: */
/* mode: C */
/* c-indent-level: 8 */
/* c-brace-offset: -8 */
/* c-argdecl-indent: 8 */
/* c-label-offset: -8 */
/* c-continued-statement-offset: 8 */
/* c-tab-always-indent: nil */
/* tab-width: 8 */
/* End: */
