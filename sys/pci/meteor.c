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

/*		Change History:
	8/21/95		Release
	8/23/95		On advice from Stefan Esser, added volatile to PCI
			memory pointers to remove PCI caching .
	8/29/95		Fixes suggested by Bruce Evans.
			meteor_mmap should return -1 on error rather than 0.
			unit # > NMETEOR should be unit # >= NMETEOR.
	10/24/95	Turn 50 Hz processing for SECAM and 60 Hz processing
			off for AUTOMODE.
	11/11/95	Change UV from always begin signed to ioctl selected
			to either signed or unsigned.
*/

#include "meteor.h"

#if NMETEOR > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/devconf.h>
#include <sys/mman.h>
#include <machine/clock.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>

#include <pci.h>
#if NPCI > 0
#include <pci/pcivar.h>
#endif
#include <machine/ioctl_meteor.h>

	/* enough memory for 640x48 RGB16, or YUV (16 storage bits/pixel) or
			     450x340 RGB24 (32 storage bits/pixel)
	  options "METEOR_ALLOC_PAGES="
	*/
#ifndef METEOR_ALLOC_PAGES
#define METEOR_ALLOC_PAGES 151
#endif
#define METEOR_ALLOC (METEOR_ALLOC_PAGES * PAGE_SIZE)

#define	NUM_SAA7116_PCI_REGS	37
#define	NUM_SAA7196_I2C_REGS	49

typedef struct {
    vm_offset_t virt_baseaddr;	/* saa7116 register virtual address */
    vm_offset_t phys_baseaddr;	/* saa7116 register physical address */
    vm_offset_t capt_cntrl;	/* capture control register offset 0x40 */
    vm_offset_t stat_reg;	/* status register offset 0x60 */
    vm_offset_t iic_virt_addr;	/* ICC bus register  offset 0x64 */
    pcici_t	tag;		/* PCI tag, for doing PCI commands */
    vm_offset_t bigbuf;		/* buffer that holds the captured image */
    int		alloc_pages;	/* number of pages in bigbuf */
    struct proc	*proc;		/* process to receive raised signal */
    struct meteor_mem *mem;	/* used to control sync. multi-frame output */
    u_long	hiwat_cnt;	/* mark and count frames lost due to hiwat */
    short	ecurrent;	/* even frame number in buffer (1-frames) */
    short	ocurrent;	/* odd frame number in buffer (1-frames) */
    short	rows;		/* number of rows in a frame */
    short	cols;		/* number of columns in a frame */
    short	depth;		/* number of byte per pixel */
    short	frames;		/* number of frames allocated */
    int		frame_size;	/* number of bytes in a frame */
    u_long	fifo_errors;	/* number of fifo capture errors since open */
    u_long	dma_errors;	/* number of DMA capture errors since open */
    u_long	frames_captured;/* number of frames captured since open */
    unsigned	flags;
#define	METEOR_INITALIZED	0x00000001
#define	METEOR_OPEN		0x00000002 
#define	METEOR_MMAP		0x00000004
#define	METEOR_INTR		0x00000008
#define	METEOR_READ		0x00000010
#define	METEOR_SINGLE		0x00000020
#define	METEOR_CONTIN		0x00000040
#define	METEOR_SYNCAP		0x00000080
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
#define	METEOR_DEV_MASK		0x0000f000
#define	METEOR_RGB16		0x00010000
#define	METEOR_RGB24		0x00020000
#define	METEOR_YUV_PACKED	0x00040000
#define	METEOR_YUV_PLANER	0x00080000
#define	METEOR_PRO_MASK		0x000f0000
#define	METEOR_SINGLE_EVEN	0x00100000
#define	METEOR_SINGLE_ODD	0x00200000
#define	METEOR_SINGLE_MASK	0x00300000
    u_char	saa7196_i2c[NUM_SAA7196_I2C_REGS]; /* saa7196 register values */
} meteor_reg_t;

meteor_reg_t meteor[NMETEOR];

u_long	read_intr_wait;
#define METPRI (PZERO+8)|PCATCH

/*---------------------------------------------------------
**
**	Meteor PCI probe and initialization routines
**
**---------------------------------------------------------
*/

static	char*	met_probe (pcici_t tag, pcidi_t type);
static	void	met_attach(pcici_t tag, int unit);
static	u_long	met_count;

struct	pci_device met_device = {
	"meteor",
	met_probe,
	met_attach,
	&met_count
};

DATA_SET (pcidevice_set, met_device);

static u_long saa7116_pci_default[NUM_SAA7116_PCI_REGS] = {
				/* PCI Memory registers	    	*/
				/* BITS	  Type	Description	*/
/* 0x00 */	0x00000000,	/* 31:1   e*RW	DMA 1 (Even)
				      0   RO    0x0 		*/
/* 0x04 */	0x00000000,	/* 31:2   e*RW	DMA 2 (Even)
				    1:0   RO	0x0		*/
/* 0x08 */	0x00000000,	/* 31:2   e*RW  DMA 3 (Even)
				    1:0   RO    0x0		*/
/* 0x0c */	0x00000000,	/* 31:1   o*RW	DMA 1 (Odd)
				      0   RO	0x0		*/
/* 0x10 */	0x00000000,	/* 31:2	  o*RW	DMA 2 (Odd)
				    1:0	  RO	0x0		*/
/* 0x14 */	0x00000000,	/* 31:2   o*RW	DMA 3 (Odd)
				    1:0   RO	0x0		*/
/* 0x18 */	0x00000500,	/* 15:2   e*RW  Stride 1 (Even)
				    1:0   RO	0x0		*/
/* 0x1c */	0x00000000,	/* 15:2	  e*RW	Stride 2 (Even)
				    1:0	  RO	0x0		*/
/* 0x20 */	0x00000000,	/* 15:2	  e*RW	Stride 3 (Even)
				    1:0	  RO	0x0		*/
/* 0x24 */	0x00000500,	/* 15:2	  o*RW	Stride 1 (Odd)
				    1:0	  RO	0x0		*/
/* 0x28 */	0x00000000,	/* 15:2	  o*RW	Stride 2 (Odd)
				    1:0	  RO	0x0		*/
/* 0x2c */	0x00000000,	/* 15:2	  o*RW	Stride 3 (Odd)
				    1:0	  RO	0x0		*/
/* 0x30 */	0xeeeeee01,	/* 31:8	  *RW	Route (Even)
				    7:0	  *RW	Mode (Even)	*/
/* 0x34 */	0xeeeeee01,	/* 31:8	  *RW	Route (Odd)
				    7:0	  *RW	Mode (Odd)	*/
/* 0x38 */	0x00200020, 	/* 22:16  *RW	FIFO Trigger Planer Mode,
				    6:0	  *RW	FIFO Trigger Packed Mode */
/* 0x3c */	0x00000103,	/*  9:8   *RW	Reserved (0x0)
				      2	  *RW	Field Toggle
				      1	  *RW	Reserved (0x1)
				      0	  *RW	Reserved (0x1)		*/
/* 0x40 */	0x000000c0,	/*    15  *RW	Range Enable
				      14  *RW	Corrupt Disable
				      11  *RR	Address Error (Odd)
				      10  *RR	Address Error (Even)
				      9   *RR	Field Corrupt (Odd)
				      8   *RR	Field Corrupt (Even)
				      7	  *RW	Fifo Enable
				      6   *RW	VRSTN#
				      5	  *RR	Field Done (Odd)
				      4   *RR	Field Done (Even)
				      3	  *RS	Single Field Capture (Odd)
				      2	  *RS	Single Field Capture (Even)
				      1	  *RW	Capture (ODD) Continous
				      0	  *RW	Capture (Even) Continous */

/* 0x44 */	0x00000000,	/*  7:0	  *RW	Retry Wait Counter */
/* 0x48 */	0x00000307,	/*    10  *RW	Interrupt mask, start of field
				      9   *RW	Interrupt mask, end odd field
				      8	  *RW	Interrupt mask, end even field
				      2   *RR	Interrupt status, start of field
				      1   *RR	Interrupt status, end of odd
				      0	  *RR	Interrupt status, end of even */
/* 0x4c */	0x00000001,	/* 31:0   *RW	Field Mask (Even) continous */
/* 0x50 */	0x00000001,	/* 31:0   *RW	Field Mask (Odd) continous */
/* 0x54 */	0x00000000,	/* 20:16  *RW	Mask Length (Odd)
				    4:0	  *RW	Mask Length (Even)	*/
/* 0x58 */	0x0005007c,	/* 22:16  *RW	FIFO almost empty
				    6:0	  *RW	FIFO almost full	*/
/* 0x5c */	0x461e1e0f,	/* 31:24  *RW	I2C Phase 4
				   23:16  *RW	I2C Phase 3
				   15:8   *RW	I2C Phase 2
				    7:0	  *RW	I2C Phase 1	*/
/* 0x60 */	0x00000300,	/* 31:24  *RO	I2C Read Data
				   23:16  **RW  I2C Auto Address
				      11  RO	I2C SCL Input
				      10  RO	I2C SDA Input
				      9	  RR	I2C Direct Abort
				      8   RR	I2C Auto Abort
				      3   RW	I2C SCL Output
				      2   RW	I2C SDA Output
				      1	  RW	I2C Bypass
				      0	  RW	I2C Auto Enable	*/
/* 0x64 */	0x00000000,	/*    24  RS	I2C New Cycle
				   23:16  **RW	I2C Direct Address
				   15:8   **RW	I2C Direct Sub-address
				    7:0	  **RW	I2C Direct Write Address */
/* 0x68 */	0x00000000,	/* 31:24  **RW  I2C Auto Sub-address 1 (Even)
				   23:16  **RW  I2C Auto Data 1 (Even)
				   15:8   **RW  I2C Auto Sub-address 0 (Even)
				    7:0	  **RW	I2C Auto Data 0 (Even) */
/* 0x6c */	0x00000000,	/* 31:24  **RW  I2C Auto Sub-address 3 (Even)
				   23:16  **RW  I2C Auto Data 3 (Even)
				   15:8   **RW  I2C Auto Sub-address 2 (Even)
				    7:0	  **RW	I2C Auto Data 2 (Even) */
/* 0x70 */	0x00000000,	/* 31:24  **RW  I2C Auto Sub-address 5 (Even)
				   23:16  **RW  I2C Auto Data 5 (Even)
				   15:8   **RW  I2C Auto Sub-address 4 (Even)
				    7:0	  **RW	I2C Auto Data 4 (Even) */
/* 0x74 */	0x00000000,	/* 31:24  **RW  I2C Auto Sub-address 7 (Even)
				   23:16  **RW  I2C Auto Data 7 (Even)
				   15:8   **RW  I2C Auto Sub-address 6 (Even)
				    7:0	  **RW	I2C Auto Data 6 (Even) */
/* 0x78 */	0x00000000,	/* 31:24  **RW  I2C Auto Sub-address 1 (Odd)
				   23:16  **RW  I2C Auto Data 1 (Odd)
				   15:8   **RW  I2C Auto Sub-address 0 (Odd)
				    7:0	  **RW	I2C Auto Data 0 (Odd) */
/* 0x7c */	0x00000000,	/* 31:24  **RW  I2C Auto Sub-address 3 (Odd)
				   23:16  **RW  I2C Auto Data 3 (Odd)
				   15:8   **RW  I2C Auto Sub-address 2 (Odd)
				    7:0	  **RW	I2C Auto Data 2 (Odd) */
/* 0x80 */	0x00000000,	/* 31:24  **RW  I2C Auto Sub-address 5 (Odd)
				   23:16  **RW  I2C Auto Data 5 (Odd)
				   15:8   **RW  I2C Auto Sub-address 4 (Odd)
				    7:0	  **RW	I2C Auto Data 4 (Odd) */
/* 0x84 */	0x00000000,	/* 31:24  **RW  I2C Auto Sub-address 7 (Odd)
				   23:16  **RW  I2C Auto Data 7 (Odd)
				   15:8   **RW  I2C Auto Sub-address 6 (Odd)
				    7:0	  **RW	I2C Auto Data 6 (Odd) */
/* 0x88 */	0x00000000,	/* 23:16  **RW	I2C Register Enable (Odd)
				    7:0	  **RW	I2C Register Enable (Even) */
/* 0x8c */	0x00000000,	/* 23:2	  e*RW	DMA End (Even)
				    1:0	  RO	0x0	*/
/* 0x90 */	0x00000000	/* 23:2	  e*RW	DMA End (Odd)
				    1:0	  RO	0x0	*/
};

static u_char saa7196_i2c_default[NUM_SAA7196_I2C_REGS] = {
			/* SAA7196 I2C bus control			*/
			/* BITS	Function				*/
/* 00 */	0x50,	/* 7:0	Increment Delay				*/
/* 01 */	0x7f,	/* 7:0	Horizontal Sync Begin for 50hz		*/
/* 02 */	0x53,	/* 7:0	Horizontal Sync Stop for 50hz		*/
/* 03 */	0x43,	/* 7:0	Horizontal Sync Clamp Start for 50hz	*/
/* 04 */	0x19,	/* 7:0	Horizontal Sync Clamp Stop for 50hz 	*/
/* 05 */	0x00,	/* 7:0	Horizontal Sync Start after PH1 for 50hz */
/* 06 */	0x46,	/*   7	Input mode =0 CVBS, =1 S-Video 
			     6	Pre filter
			   5:4  Aperture Bandpass characteristics
			   3:2	Coring range for high freq
			   1:0	Aperture bandpass filter weights	*/
/* 07 */	0x00,	/* 7:0	Hue					*/
/* 08 */	0x7f,	/* 7:3	Colour-killer threshold QAM (PAL, NTSC) */
/* 09 */	0x7f,	/* 7:3	Colour-killer threshold SECAM		*/
/* 0a */	0x7f,	/* 7:0	PAL switch sensitivity			*/
/* 0b */	0x7f,	/* 7:0	SECAM switch sensitivity		*/
/* 0c */	0x40,	/*   7	Colour-on bit
			   6:5	AGC filter				*/
/* 0d */	0x84,	/*   7	VTR/TV mode bit = 1->VTR mode
			     3	Realtime output mode select bit
			     2	HREF position select
			     1	Status byte select
			     0	SECAM mode bit				*/
/* 0e */	0x38,	/*   7	Horizontal clock PLL
			     5	Select interal/external clock source
			     4	Output enable of Horizontal/Vertical sync
			     3	Data output YUV enable
			     2	S-VHS bit
			     1	GPSW2
			     0	GPSW1					*/
/* 0f */	0x50,	/*   7	Automatic Field detection
			     6	Field Select 0 = 50hz, 1=60hz
			     5	SECAM cross-colour reduction
			     4	Enable sync and clamping pulse
			   3:1	Luminance delay compensation		*/
/* 10 */	0x00,	/*   2	Select HREF Position
			   1:0  Vertical noise reduction		*/
/* 11 */	0x2c,	/* 7:0	Chrominance gain conrtol for QAM	*/
/* 12 */	0x40,	/* 7:0	Chrominance saturation control for VRAM port */
/* 13 */	0x40,	/* 7:0	Luminance contract control for VRAM port */
/* 14 */	0x34,	/* 7:0	Horizontal sync begin for 60hz		*/
/* 15 */	0x0c,	/* 7:0	Horizontal sync stop for 60hz		*/
/* 16 */	0xfb,	/* 7:0	Horizontal clamp begin for 60hz		*/
/* 17 */	0xd4,	/* 7:0	Horizontal clamp stop for 60hz		*/
/* 18 */	0xec,	/* 7:0	Horizontal sync start after PH1 for 60hz */
/* 19 */	0x80,	/* 7:0	Luminance brightness control for VRAM port */
/* 1a */	0x00,
/* 1b */	0x00,
/* 1c */	0x00,
/* 1d */	0x00,
/* 1e */	0x00,
/* 1f */	0x00,
/* 20 */	0x90,	/*   7	ROM table bypass switch
			   6:5	Set output field mode
			     4	VRAM port outputs enable
			   3:2	First pixel position in VRO data
			   1:0	FIFO output register select		*/
/* 21 */	0x80,	/* 7:0	[7:0] Pixel number per line on output	*/
/* 22 */	0x80,	/* 7:0	[7:0] Pixel number per line on input	*/
/* 23 */	0x03,	/* 7:0	[7:0] Horizontal start position of scaling win*/
/* 24 */	0x8a,	/* 7:5	Horizontal decimation filter
			     4  [8] Horizontal start position of scaling win
			   3:2	[9:8] Pixel number per line on input
			   1:0  [9:8] Pixel number per line on output 	*/
/* 25 */	0xf0,	/* 7:0	[7:0] Line number per output field	*/
/* 26 */	0xf0,	/* 7:0	[7:0] Line number per input field	*/
/* 27 */	0x0f,	/* 7:0	[7:0] Vertical start of scaling window	*/
/* 28 */	0x80,	/*   7	Adaptive filter switch
			   6:5	Vertical luminance data processing
			     4	[8] Vertical start of scaling window 
			   3:2  [9:8] Line number per input field
			   1:0	[9:8] Line number per output field	*/
/* 29 */	0x16,	/* 7:0	[7:0] Vertical bypass start		*/
/* 2a */	0x00,	/* 7:0	[7:0] Vertical bypass count		*/
/* 2b */	0x00,	/*   4  [8] Vertical bypass start
			     2  [8] Vertical bypass count
			     0	Polarity, internally detected odd even flag */
/* 2c */	0x80,	/* 7:0	Set lower limit V for colour-keying	*/
/* 2d */	0x7f,	/* 7:0	Set upper limit V for colour-keying	*/
/* 2e */	0x80,	/* 7:0	Set lower limit U for colour-keying	*/
/* 2f */	0x7f,	/* 7:0	Set upper limit U for colour-keying	*/
/* 30 */	0xbf	/*   7	VRAM bus output format
			     6	Adaptive geometrical filter
			     5	Luminance limiting value
			     4	Monochrome and two's complement output data sel
			     3	Line quailifier flag
			     2	Pixel qualifier flag
			     1	Transparent data transfer
			     0	Extended formats enable bit		*/
};

/*
 * i2c_write:
 * Returns	0	Succesful completion.
 * Returns	1	If transfer aborted or timeout occured.
 *
 */
#define	SAA7196_I2C_ADDR		0x40
#define	I2C_WRITE			0x00
#define	I2C_READ	 		0x01
#define	SAA7116_IIC_NEW_CYCLE		0x1000000L
#define	IIC_DIRECT_TRANSFER_ABORTED	0x0000200L

#define	SAA7196_WRITE(mtr, reg, data) \
	i2c_write(mtr, SAA7196_I2C_ADDR, I2C_WRITE, reg, data); \
	mtr->saa7196_i2c[reg] = data
#define SAA7196_REG(mtr, reg) mtr->saa7196_i2c[reg]
#define	SAA7196_READ(mtr) \
	i2c_write(mtr, SAA7196_I2C_ADDR, I2C_READ, 0x0, 0x0)

static int
i2c_write(meteor_reg_t * mtr, u_char slave, u_char rw, u_char reg, u_char data)
{
register unsigned long	wait_counter = 0x0001ffff;
register volatile u_long *iic_write_loc = (volatile u_long *)mtr->iic_virt_addr;
register int		err = 0;

	/* Write the data the the i2c write register */
	*iic_write_loc = SAA7116_IIC_NEW_CYCLE |
		(((u_long)slave|(u_long)rw) << 16) |
		((u_long)reg << 8) | (u_long)data;

	/* Wait until the i2c cycle is compeleted */
	while((*iic_write_loc & SAA7116_IIC_NEW_CYCLE)) {
		if(!wait_counter) break;
		wait_counter--;
	}

/*#ifdef notdef*/
	/* it seems the iic_write_loc is cached, until we can
	   figure out how to uncache the pci registers, then we
	   will just ignore the timeout.  Hopefully 1ffff will
	   be enough delay time for the i2c cycle to complete */
	if(!wait_counter) {
		printf("meteor: saa7116 i2c %s transfer timeout 0x%x",
			rw ? "read" : "write", *iic_write_loc);
			
		err=1;
	} 
/*#endif*/

	/* Check for error on direct write, clear if any */
	if((*((volatile u_long *)mtr->stat_reg)) & IIC_DIRECT_TRANSFER_ABORTED) {
		printf("meteor: saa7116 i2c %s tranfer aborted",
			rw ? "read" : "write" );
		err= 1;
	}

	if(err) {
		printf(" - reg=0x%x, value=0x%x.\n", reg, data);
	}
		
	return err;
}


static	char*
met_probe (pcici_t tag, pcidi_t type)
{
	switch (type) {

	case 0x12238086ul:	/* meteor */
		return ("Matrox Meteor");

	};
	return ((char*)0);
}

	/* interrupt handling routine 
	   complete meteor_read() if using interrupts
	*/
int
meteor_intr( void *arg)
{
	register meteor_reg_t *mtr = (meteor_reg_t *) arg;

	register volatile u_long *cap, *base, *status, cap_err;
	struct meteor_mem *mm;

	base = (volatile u_long *) mtr->virt_baseaddr;
	cap = (volatile u_long *) mtr->capt_cntrl;	/* capture control ptr */
	status = base + 18;	/*  mtr->virt_base + 0x48  save a dereference */

	/* the even field has to make the decision of whether the high water
	   has been reached. If hiwat has been reach do not advance the buffer.
	   continue to save frames on this buffer until we can advance again */
	if (*status & 0x1) {		/* even field */
		if (mtr->flags & METEOR_SYNCAP) { 
		    mm = mtr->mem;		/* shared SYNCAP struct */
		    if ((!mtr->hiwat_cnt && mm->num_active_bufs < mm->hiwat) ||
			(mm->num_active_bufs <= mm->lowat)) {
		      mtr->hiwat_cnt = 0;
		      if (++mtr->ecurrent > mtr->frames) {
			 *base = mtr->bigbuf;
			 mtr->ecurrent = 1;
		      } else {
			 *base = *base + mtr->frame_size;
		      }
		    }
		    else {
				mtr->hiwat_cnt++;
		    }
		} else if(mtr->flags & METEOR_SINGLE) {
			*cap &= 0x0ffe;
			mtr->flags &= ~METEOR_SINGLE_EVEN;
			if(!(mtr->flags & METEOR_SINGLE_MASK)) {
				mtr->frames_captured++ ;
				wakeup((caddr_t) &read_intr_wait);
			}
		}
	} else {			/* odd field */
		if (mtr->flags & METEOR_SINGLE) {
			*cap &= 0x0ffd;
			mtr->flags &= ~ METEOR_SINGLE_ODD;
			if(!(mtr->flags & METEOR_SINGLE_MASK)) {
				mtr->frames_captured++ ;
				wakeup((caddr_t) &read_intr_wait);
			}
		} else if (mtr->flags & METEOR_SYNCAP) {
			mm = mtr->mem;		/* shared SYNCAP struct */
				/* even field decided to advance or not, we
				   simply add stride to that decision */
			*(base+3) = *base + *(base + 6);
			if (mtr->ecurrent != mtr->ocurrent) {
				mm->active |= (1 << (mtr->ocurrent-1));
				mtr->ocurrent = mtr->ecurrent;
				mm->num_active_bufs++;
				if (mtr->proc && mm->signal) {
					mtr->frames_captured++ ;
					psignal(mtr->proc, mm->signal);
				}
			}
		}
	}
	if (cap_err = (*cap & 0xf00)) {
	   if (cap_err & 0x3)
		mtr->fifo_errors++ ;	/* incrememnt fifo capture errors cnt */
	   if (cap_err & 0xc)
		mtr->dma_errors++ ;	/* increment DMA capture errors cnt */
	}

	*cap |= 0xf30;			/* clear error and field done */
	*status |=  0xf;		/* clear interrupt status */

	return(1);
}

/*
 * Initialize the capture card to NTSC RGB 16 640x480
 */
static void
meteor_init ( meteor_reg_t *mtr )
{
	volatile u_long *vbase_addr;
	int i;

	*((volatile u_long *)(mtr->capt_cntrl)) = 0x00000040L;

	vbase_addr = (volatile u_long *) mtr->virt_baseaddr;
	for (i= 0 ; i < NUM_SAA7116_PCI_REGS; i++)
		*vbase_addr++ = saa7116_pci_default[i];

	for (i = 0; i < NUM_SAA7196_I2C_REGS; i++) {
		SAA7196_WRITE(mtr, i, saa7196_i2c_default[i]);
	}

}

static	void	met_attach(pcici_t tag, int unit)
{
#ifdef METEOR_IRQ		/* from the meteor.h file */
	u_long old_irq,new_irq;

#endif METEOR_IRQ		/* from the meteor.h file */
	meteor_reg_t *mtr;
	vm_offset_t buf;

	if (unit >= NMETEOR) {
		printf("meteor_attach: mx%d: invalid unit number\n");
        	return ;
	}

	mtr = &meteor[unit];
	pci_map_mem(tag, 0x10, &(mtr->virt_baseaddr),
			       &(mtr->phys_baseaddr));
 				/* IIC addres at 0x64 offset bytes */
	mtr->capt_cntrl = mtr->virt_baseaddr + 0x40;
	mtr->stat_reg = mtr->virt_baseaddr + 0x60;
	mtr->iic_virt_addr = mtr->virt_baseaddr + 0x64;

#ifdef METEOR_IRQ		/* from the meteor.h file */
	old_irq = pci_conf_read(tag, PCI_INTERRUPT_REG);
	pci_conf_write(tag, PCI_INTERRUPT_REG, METEOR_IRQ);
	new_irq = pci_conf_read(tag, PCI_INTERRUPT_REG);
	printf("meteor_attach: irq changed from %d to %d\n", (old_irq & 0xff),
							     (new_irq & 0xff));
#endif METEOR_IRQ

	meteor_init( mtr );	/* set up saa7116 and saa7196 chips */
	mtr->tag = tag;
				/* setup the interrupt handling routine */
	pci_map_int (tag, meteor_intr, (void*) mtr, &net_imask); 

				/* 640*240*3 round up to nearest pag e*/
	buf = vm_page_alloc_contig(METEOR_ALLOC, 0x100000, 0xffffffff, PAGE_SIZE);
	if (buf == NULL) {
		printf("meteor_attach: big buffer allocation failed\n");
		return;
	}
	mtr->bigbuf = buf;
	mtr->alloc_pages = METEOR_ALLOC_PAGES;
	
	bzero((caddr_t) buf, METEOR_ALLOC);

	buf = vtophys(buf);
	*((volatile u_long *) mtr->virt_baseaddr) = buf;

					/* 640x480 RGB 16 */
	*((volatile u_long *) mtr->virt_baseaddr + 3) = buf + 0x500;
			
	*((volatile u_long *) mtr->virt_baseaddr + 36) = 
	*((volatile u_long *) mtr->virt_baseaddr + 35) = buf + METEOR_ALLOC;
    	mtr->flags = METEOR_INITALIZED | METEOR_NTSC | METEOR_DEV0 |
							   METEOR_RGB16;
			/* 1 frame of 640x480 RGB 16 */
	mtr->cols = 640;
	mtr->rows = 480;
	mtr->depth = 2;		/* two bytes per pixel */
	mtr->frames = 1;	/* one frame */
}

static void
meteor_reset(meteor_reg_t * const sc)
{

}

/*---------------------------------------------------------
**
**	Meteor character device driver routines
**
**---------------------------------------------------------
*/

#define UNIT(x)	((x) & 0x07)

int
meteor_open(dev_t dev, int flag)
{
	meteor_reg_t *mtr;
	int	unit; 
	int	i;

	unit = UNIT(minor(dev));
	if (unit >= NMETEOR)	/* unit out of range */
		return(ENXIO);

	mtr = &(meteor[unit]);

	if (!(mtr->flags & METEOR_INITALIZED))	/* device not found */
		return(ENXIO);

	if (mtr->flags & METEOR_OPEN)		/* device is busy */
		return(EBUSY);

	mtr->flags |= METEOR_OPEN;
	/*
	 * Make sure that the i2c regs are set the same for each open.
	 */
	for(i=0; i< NUM_SAA7196_I2C_REGS; i++) {
		SAA7196_WRITE(mtr, i, saa7196_i2c_default[i]);
	}

	mtr->fifo_errors = 0;
	mtr->dma_errors = 0;
	mtr->frames_captured = 0;

	return(0);
}

int
meteor_close(dev_t dev, int flag)
{
	meteor_reg_t *mtr;
	int	unit; 
	int	temp;

	unit = UNIT(minor(dev));
	if (unit >= NMETEOR)	/* unit out of range */
		return(ENXIO);

	mtr = &(meteor[unit]);
	mtr->flags &= ~METEOR_OPEN;

					/* XXX stop any capture modes running */
	switch (mtr->flags & METEOR_CAP_MASK) {
	case METEOR_SINGLE:	/* this should not happen, the read capture 
				  should have completed or in the very least
				  recieved a signal before close is called. */
		mtr->flags &= ~(METEOR_SINGLE|METEOR_SINGLE_MASK);
		wakeup((caddr_t) &read_intr_wait);	/* continue read */
		break;

	case METEOR_CONTIN:	/* continous unsync-ed reading, we can
				   simply turn off the capture */
		mtr->flags &= ~METEOR_CONTIN;
		*((volatile u_long *) mtr->capt_cntrl) = 0x0ff0;	/* turn off capture */
		break;
	case METEOR_SYNCAP:
		mtr->flags &= ~METEOR_SYNCAP;
		*((volatile u_long *) mtr->capt_cntrl) = 0x0ff0;	/* turn off capture */
		mtr->proc = NULL;
		mtr->mem = NULL;
		mtr->ecurrent = mtr->ocurrent = 1;
		/* re-initalize the even/odd DMA positions to top of buffer */
		*((volatile u_long *) mtr->virt_baseaddr) = mtr->bigbuf;
		*((volatile u_long *) mtr->virt_baseaddr +3) =
				*((volatile u_long *) mtr->virt_baseaddr) +
				*((volatile u_long *) mtr->virt_baseaddr+6);
		break;
	case 0:
		break;
	default:
		printf("meteor_close: bad capture state on close %d\n",
						mtr->flags & METEOR_CAP_MASK);
	}
#ifdef METEOR_DEALLOC_PAGES
	if (mtr->bigbuf) {
		kmem_free(kernel_map,mtr->bigbuf,(mtr->alloc_pages*PAGE_SIZE));
		mtr->bigbuf = NULL;
		mtr->alloc_pages = 0;
	}
#else
#ifdef METEOR_DEALLOC_ABOVE
	if (mtr->bigbuf && mtr->alloc_pages > METEOR_DEALLOC_ABOVE) {
		temp = METEOR_DEALLOC_ABOVE - mtr->alloc_pages;
		kmem_free(kernel_map,
			  mtr->bigbuf+((mtr->alloc_pages - temp) * PAGE_SIZE),
			  (temp * PAGE_SIZE));
		mtr->alloc_pages = METEOR_DEALLOC_ABOVE;
	}
#endif
#endif
	return(0);
}

int
meteor_read(dev_t dev, struct uio *uio)
{
	meteor_reg_t *mtr;
	int	unit; 
	int	status;
	int	count;

	unit = UNIT(minor(dev));
	if (unit >= NMETEOR)	/* unit out of range */
		return(ENXIO);

	mtr = &(meteor[unit]);
	if (!mtr->bigbuf)	/* no frame buffer allocated (ioctl failed) */
		return(ENXIO);

	if (mtr->flags & METEOR_CAP_MASK)
		return(EIO);		/* already capturing */

	count = mtr->rows * mtr->cols * mtr->depth;
	if (uio->uio_iov->iov_len < count)
		return(EINVAL);

	mtr->flags |= METEOR_SINGLE | METEOR_SINGLE_MASK;

	*((volatile u_long *) mtr->capt_cntrl) = 0x0ff3;	/* capture */
	
	status = tsleep((caddr_t) &read_intr_wait, METPRI, "capturing", 0);

	if (!status) 		/* successful capture */
		status = uiomove((caddr_t)mtr->bigbuf, count, uio);

	else
		printf ("meteor_read: bad tsleep\n");
	mtr->flags &= ~(METEOR_SINGLE | METEOR_SINGLE_MASK);
	return(status);
}

int
meteor_write()
{
	return(0);
}

int
meteor_ioctl(dev_t dev, int cmd, caddr_t arg, int flag, struct proc *pr)
{
	int	error;  
	int	unit;   
	int	temp;
	meteor_reg_t *mtr;
	struct meteor_counts *cnt;
	struct meteor_geomet *geo;
	struct meteor_mem *mem;
	struct meteor_capframe *frame;
	volatile u_long *p;
	vm_offset_t buf;

	error = 0;

	if (!arg) return(EINVAL);
	unit = UNIT(minor(dev));
	if (unit >= NMETEOR)	/* unit out of range */
		return(ENXIO);

	mtr = &(meteor[unit]);

	switch (cmd) {
	case METEORSTATUS:	/* get 7196 status */
		temp = 0;
		SAA7196_WRITE(mtr, 0x0d, SAA7196_REG(mtr, 0x0d) | 0x02);
		SAA7196_READ(mtr);
		temp |= ((*((volatile u_long *)mtr->stat_reg)) & 0xff000000L) >> 24;
		SAA7196_WRITE(mtr, 0x0d, SAA7196_REG(mtr, 0x0d) & 0x02);
		SAA7196_READ(mtr);
		temp |= ((*((volatile u_long *)mtr->stat_reg)) & 0xff000000L) >> 16;
		*(u_short *)arg = temp;
		break;
	case METEORSHUE:	/* set hue */
		SAA7196_WRITE(mtr, 0x07, *(char *)arg);
		break;
	case METEORGHUE:	/* get hue */
		*(char *)arg = SAA7196_REG(mtr, 0x07);
		break;
	case METEORSCHCV:	/* set chrominance gain */
		SAA7196_WRITE(mtr, 0x11, *(char *)arg);
		break;
	case METEORGCHCV:	/* get chrominance gain */
		*(char *)arg = SAA7196_REG(mtr, 0x11);
		break;
	case METEORSINPUT:	/* set input device */
		switch(*(unsigned long *)arg & METEOR_DEV_MASK) {
		case 0:			/* default */
		case METEOR_INPUT_DEV0:
			mtr->flags = (mtr->flags & ~METEOR_DEV_MASK)
				| METEOR_DEV0;
			
			SAA7196_WRITE(mtr, 0x0e,
				(SAA7196_REG(mtr, 0x0e) & ~0x3) | 0x0);
			break;
		case METEOR_INPUT_DEV1:
			mtr->flags = (mtr->flags & ~METEOR_DEV_MASK)
					       | METEOR_DEV1;
			SAA7196_WRITE(mtr, 0x0e,
				(SAA7196_REG(mtr, 0x0e) & ~0x3) | 0x1);
			break;
		case METEOR_INPUT_DEV2:
			mtr->flags = (mtr->flags & ~METEOR_DEV_MASK)
					       | METEOR_DEV2;
			SAA7196_WRITE(mtr, 0x0e,
				(SAA7196_REG(mtr, 0x0e) & ~0x3) | 0x2);
			break;
		case METEOR_INPUT_DEV3:
			mtr->flags = (mtr->flags & ~METEOR_DEV_MASK)
					       | METEOR_DEV3;
			SAA7196_WRITE(mtr, 0x0e,
				(SAA7196_REG(mtr, 0x0e) | 0x3));
			break;
		default:
			return EINVAL;
		}
		break;
	case METEORGINPUT:	/* get input device */
		*(u_long *)arg = mtr->flags & METEOR_DEV_MASK;
		break;
	case METEORSFMT:	/* set input format */
		switch(*(unsigned long *)arg & METEOR_FORM_MASK ) {
		case 0:			/* default */
		case METEOR_FMT_NTSC:
			mtr->flags = (mtr->flags & ~METEOR_FORM_MASK) |
				METEOR_NTSC;
			SAA7196_WRITE(mtr, 0x0d, 
				(SAA7196_REG(mtr, 0x0d) & ~0x01));
			SAA7196_WRITE(mtr, 0x0f,
				(SAA7196_REG(mtr, 0x0f) & ~0xc0) | 0x40);
			SAA7196_WRITE(mtr, 0x22, 0x80);
			SAA7196_WRITE(mtr, 0x24, 
				(SAA7196_REG(mtr, 0x24) & ~0x0c) | 0x08);
			SAA7196_WRITE(mtr, 0x26, 0xf0);
			SAA7196_WRITE(mtr, 0x28, 
				(SAA7196_REG(mtr, 0x28) & ~0x0c)) ;
		break;
		case METEOR_FMT_PAL:
			mtr->flags = (mtr->flags & ~METEOR_FORM_MASK) |
				METEOR_PAL;
			SAA7196_WRITE(mtr, 0x0d, 
				(SAA7196_REG(mtr, 0x0d) & ~0x01));
			SAA7196_WRITE(mtr, 0x0f, 
				(SAA7196_REG(mtr, 0x0f) & ~0xc0));
			SAA7196_WRITE(mtr, 0x22, 0x00);
			SAA7196_WRITE(mtr, 0x24, 
				(SAA7196_REG(mtr, 0x24) | 0x0c));
			SAA7196_WRITE(mtr, 0x26, 0x20);
			SAA7196_WRITE(mtr, 0x28, 
				(SAA7196_REG(mtr, 0x28) & ~0x0c) | 0x01) ;
		break;
		case METEOR_FMT_SECAM:
			mtr->flags = (mtr->flags & ~METEOR_FORM_MASK) |
				METEOR_SECAM;
			SAA7196_WRITE(mtr, 0x0d, 
				(SAA7196_REG(mtr, 0x0d) & ~0x01) | 0x1);
			SAA7196_WRITE(mtr, 0x0f, 
				(SAA7196_REG(mtr, 0x0f) & ~0xe0) | 0x20);
			SAA7196_WRITE(mtr, 0x22, 0x00);
			SAA7196_WRITE(mtr, 0x24, 
				(SAA7196_REG(mtr, 0x24) | 0x0c));
			SAA7196_WRITE(mtr, 0x26, 0x20);
			SAA7196_WRITE(mtr, 0x28, 
				(SAA7196_REG(mtr, 0x28) & ~0x0c) | 0x01) ;
		break;
		case METEOR_FMT_AUTOMODE:
			mtr->flags = (mtr->flags & ~METEOR_FORM_MASK) |
				METEOR_AUTOMODE;
			SAA7196_WRITE(mtr, 0x0d, 
				(SAA7196_REG(mtr, 0x0d) & ~0x01));
			SAA7196_WRITE(mtr, 0x0f, 
				(SAA7196_REG(mtr, 0x0f) & ~0xc0) | 0x80);
		break;
		default:
			return EINVAL;
		}
		break;
	case METEORGFMT:	/* get input format */
		*(u_long *)arg = mtr->flags & METEOR_FORM_MASK;
		break;
	case METEORCAPTUR:
		switch (*(int *) arg) {
		case METEOR_CAP_SINGLE:
			if (!mtr->bigbuf)	/* no frame buffer allocated */
				return(ENXIO);

			if (mtr->flags & METEOR_CAP_MASK)
				return(EIO);		/* already capturing */

			mtr->flags |= METEOR_SINGLE | METEOR_SINGLE_MASK;

			*((volatile u_long *) mtr->capt_cntrl) = 0x0ff3;	/* capture */

			error = tsleep((caddr_t) &read_intr_wait, METPRI,
								"capturing", 0);
			mtr->flags &= ~(METEOR_SINGLE| METEOR_SINGLE_MASK);
			break;
		case METEOR_CAP_CONTINOUS:
			if (!mtr->bigbuf)	/* no frame buffer allocated */
				return(ENXIO);

			if (mtr->flags & METEOR_CAP_MASK)
				return(EIO);		/* already capturing */

			mtr->flags |= METEOR_CONTIN;

			*((volatile u_long *) mtr->capt_cntrl) = 0x0ff3;	/* capture */
			break;
		case METEOR_CAP_STOP_CONT:
			if (mtr->flags & METEOR_CONTIN) {
				mtr->flags &= ~METEOR_CONTIN;
							/* turn off capture */
				*((volatile u_long *) mtr->capt_cntrl) = 0x0ff0;
			}
			break;
	
		default:
			error = EINVAL;
			break;
		}
		break;
	case METEORCAPFRM:
	    frame = (struct meteor_capframe *) arg;
	    if (!frame) 
		return(EINVAL);
	    switch (frame->command) {
	    case METEOR_CAP_N_FRAMES:
		if (mtr->flags & METEOR_CAP_MASK)
			return(EIO);
		if (mtr->flags & METEOR_YUV_PLANER)
			return(EINVAL);
		if (!mtr->bigbuf)
			return(ENOMEM);
		if ((mtr->frames < 2) ||
		    (frame->lowat < 1 || frame->lowat >= mtr->frames) ||
		    (frame->hiwat < 1 || frame->hiwat >= mtr->frames))
			return(EINVAL);
		mtr->flags |= METEOR_SYNCAP;
		mtr->proc = pr;
			/* meteor_mem structure is on the page after the data */
		mem = mtr->mem = (struct meteor_mem *) (mtr->bigbuf +
				((mtr->rows*mtr->cols * mtr->depth *
				mtr->frames+PAGE_SIZE-1)/PAGE_SIZE)*PAGE_SIZE);
		mtr->ecurrent = mtr->ocurrent = 1;
		mem->signal =  frame->signal;
        	mem->num_bufs = mtr->frames;
		mem->frame_size=
			mtr->frame_size = mtr->rows * mtr->cols * mtr->depth;
                /* user and kernel change these */ 
		mem->lowat = frame->lowat;
		mem->hiwat = frame->hiwat;
        	mem->active = 0;
        	mem->num_active_bufs = 0;
		*((u_long *) mtr->capt_cntrl) = 0x0ff3;
		break;
	    case METEOR_CAP_STOP_FRAMES:
		if (mtr->flags & METEOR_SYNCAP) {
			mtr->flags &= ~METEOR_SYNCAP;
						/* turn off capture */
			*((u_long *) mtr->capt_cntrl) = 0x0ff0;
			mtr->proc = NULL;
			mtr->mem = NULL;
			mtr->ecurrent = mtr->ocurrent = 0;

		/* re-initalize the even/odd DMA positions to top of buffer*/
		/* XXX if a capture is in progress, this may be trouble */

			*((volatile u_long *) mtr->virt_baseaddr) = mtr->bigbuf;
			*((volatile u_long *) mtr->virt_baseaddr +3) =
					*((volatile u_long *) mtr->virt_baseaddr) +
					*((volatile u_long *) mtr->virt_baseaddr+6);
		}
		break;
	    default:
		error = EINVAL;
		break;
	    }
		break;
 
	case METEORSETGEO:
		geo = (struct meteor_geomet *) arg;
				/* can't change parameters while capturing */
		if (mtr->flags & METEOR_CAP_MASK)
			return(EBUSY);

		if ((geo->columns & 0x3fe) != geo->columns) {
			printf("meteor ioctl: column too large or not even\n");
			error = EINVAL;
		}
		if ((geo->rows & 0x7fe) != geo->rows) {
			printf("meteor ioctl: rows too large or not even\n");
			error = EINVAL;
		}
		if (geo->frames > 32) {
			printf("meteor ioctl: frames too large\n");
			error = EINVAL;
		}
		if (!error && (temp=geo->rows*geo->columns * geo->frames *2)) {
		   if (geo->oformat & METEOR_GEO_RGB24)
			temp = temp * 2;

		   	/* meteor_mem structure for SYNC Capture */
		   if (geo->frames > 1)
			temp += PAGE_SIZE;

		   temp = (temp + PAGE_SIZE -1)/PAGE_SIZE;
		   if (temp > mtr->alloc_pages) {
			if (mtr->bigbuf)
				kmem_free(kernel_map, mtr->bigbuf,
						(mtr->alloc_pages * PAGE_SIZE));
			mtr->bigbuf = vm_page_alloc_contig((temp*PAGE_SIZE),
					      0x100000, 0xffffffff, PAGE_SIZE);
			mtr->alloc_pages = temp;
		   }
		   if (mtr->bigbuf) {
			mtr->rows = geo->rows;
			mtr->cols = geo->columns;
			mtr->frames = geo->frames;
		   }
		   else {
			mtr->alloc_pages = 0;
			printf("meteor_ioctl: buffer allocation failed\n");
			error = ENOMEM;
		   }
		}

		p = (volatile u_long *) mtr->virt_baseaddr;
		if (mtr->bigbuf)
			buf = vtophys(mtr->bigbuf);
		else
			buf = 0;
		*p++ = buf;	/* even y or even RGB */
				/* set end of buffer location */
		*(p+36)  = *(p+35) = buf + mtr->alloc_pages * PAGE_SIZE;

		switch (geo->oformat & METEOR_PRO_MASK) {
		   case 0:			/* default */
		   case METEOR_GEO_RGB16:
		      mtr->depth = 2;
		      if (mtr->flags & METEOR_RGB16 == 0) {
			mtr->flags = (mtr->flags & ~(METEOR_RGB24 |
						   METEOR_YUV_PACKED |
						   METEOR_YUV_PLANER))
					       | METEOR_RGB16;
			}
			if (error == 0) {
			/* recal stride and odd starting point */

				*p++ = 0;
				*p++ = 0;
				*p++ = buf + mtr->cols * 2;
				*p++ = 0;
				*p++ = 0;
						/* stride */
				*p = *(p+3) = mtr->cols * 2;
				*(p+6) = *(p+7) = 0xeeeeee01;
						/* set up the saa7196 */
				SAA7196_WRITE(mtr, 0x20, 0x90);
			}
			break;
		   case METEOR_GEO_RGB24:
		      mtr->depth = 4;
		      if (mtr->flags & METEOR_RGB24 == 0) {
			mtr->flags = (mtr->flags & ~(METEOR_RGB16 |
						   METEOR_YUV_PACKED |
						   METEOR_YUV_PLANER))
					       | METEOR_RGB24;
			}
			if (error == 0 ) {
			/* recal stride and odd starting point */

				*p++ = 0;
				*p++ = 0;
				*p++ = buf + mtr->cols * 4;
						/* routes */
				*p++ = 0;
				*p++ = 0;
						/* stride */
				*p = *(p+3) = mtr->cols * 4;
						/* routes */
				*(p+6) = *(p+7) = 0x39393900;
						/* set up the saa7196 */
				SAA7196_WRITE(mtr, 0x20, 0x92);
			}
			break;
		   case METEOR_GEO_YUV_PLANER:
		      mtr->depth = 2;
		      if (mtr->flags & METEOR_YUV_PLANER == 0) {
			mtr->flags = (mtr->flags & ~(METEOR_RGB16 |
						   METEOR_RGB24 |
						   METEOR_YUV_PACKED))
					       | METEOR_YUV_PLANER;
			}
			if (error == 0 ) {
			/* recal stride and odd starting point */

				temp = mtr->rows * mtr->cols;
						/* even u */
				*p++ = buf + temp;
						/* even v */
				*p++ = buf + temp + (temp >> 2);
						/* odd y */
				*p++ = buf + mtr->cols;
						/* odd u */
				*p++ = buf + temp + (temp >> 1);
						/* odd v */
				*p++ = buf + temp + (temp >> 1)
						       + (temp >> 2);
						/* stride */
				*p = *(p+3) = mtr->cols;
						/* routes */
				*(p+6) = *(p+7) = 0xaaaaffc1;
						/* set up the saa7196 */
				SAA7196_WRITE(mtr, 0x20, 0x91);
			}
			break;
		   case METEOR_GEO_YUV_PACKED:
		      mtr->depth = 2;
		      if (mtr->flags & METEOR_YUV_PACKED == 0) {
			mtr->flags = (mtr->flags & ~(METEOR_RGB16 |
						   METEOR_RGB24 |
						   METEOR_YUV_PLANER))
					       | METEOR_YUV_PACKED;
		      }
			if (error == 0 ) {
			/* recal stride and odd starting point */

				*p++ = 0;
				*p++ = 0;
				*p++ = buf + mtr->cols * 2;
				*p++ = 0;
				*p++ = 0;
						/* stride */
				*p = *(p+3) = mtr->cols * 2;
						/* routes */
				*(p+6) = *(p+7) = 0xeeeeee41;
						/* set up the saa7196 */
				SAA7196_WRITE(mtr, 0x20, 0x91);
			}
			break;
		   default:
			error = EINVAL;	/* invalid arguement */
			printf("meteor_ioctl: invalid output format\n");
			break;
		}
		if (error == 0 ) {
					/* set cols */
			SAA7196_WRITE(mtr, 0x21, mtr->cols & 0xff);
			SAA7196_WRITE(mtr, 0x24,
				((SAA7196_REG(mtr, 0x24) & ~0x03) |
						((mtr->cols >> 8) & 0x03)));
					/* set rows */
			SAA7196_WRITE(mtr, 0x25, ((mtr->rows >> 1) & 0xff));
			SAA7196_WRITE(mtr, 0x28,
				((SAA7196_REG(mtr, 0x28) & ~0x03) |
						((mtr->rows >> 9) & 0x03)));
					/* set signed/unsigned */
			SAA7196_WRITE(mtr, 0x30, 
				(SAA7196_REG(mtr, 0x30) & ~0x10) |
				((geo->oformat&METEOR_GEO_UNSIGNED)?0:0x10));
		}
		break;
	case METEORGETGEO:
		geo = (struct meteor_geomet *) arg;
		geo->rows = mtr->rows;
		geo->columns = mtr->cols;
		geo->frames = mtr->frames;
		geo->oformat = mtr->flags & METEOR_PRO_MASK;
		break;
	case METEORSCOUNT:	/* (re)set error counts */
		cnt = (struct meteor_counts *) arg;
		mtr->fifo_errors = cnt->fifo_errors;
		mtr->dma_errors = cnt->dma_errors;
		mtr->frames_captured = cnt->frames_captured;
		break;
	case METEORGCOUNT:	/* get error counts */
		cnt = (struct meteor_counts *) arg;
		cnt->fifo_errors = mtr->fifo_errors;
		cnt->dma_errors = mtr->dma_errors;
		cnt->frames_captured = mtr->frames_captured;
		break;
	default:
		printf("meteor_ioctl: invalid ioctl request\n");
		error = ENOTTY;
		break;
	}
	return(error);
}

int
meteor_mmap(dev_t dev, int offset, int nprot)
{

	int	unit;
	meteor_reg_t *mtr;

	unit = UNIT(minor(dev));
	if (unit >= NMETEOR)		/* at this point could this happen? */
		return(-1);

	mtr = &(meteor[unit]);


	if(nprot & PROT_EXEC)
		return -1;

	if(offset >= mtr->alloc_pages * PAGE_SIZE)
		return -1;

	return i386_btop((vtophys(mtr->bigbuf) + offset));
}

#endif /* NMETEOR > 0 */
