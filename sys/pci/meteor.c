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
	12/07/95	Changed 7196 startup codes for 50 Hz as recommended
			by Luigi Rizzo (luigi@iet.unipi.it)
	12/08/95	Clear SECAM bit in PAL/NTSC and set input field count
			bits for 50 Hz mode (PAL/SECAM) before I was setting the
			output count bits. by Luigi Rizzo (luigi@iet.unipi.it)
	12/18/95	Correct odd DMA field (never exceed, but good for safety
			Changed 7196 startup codes for 50 Hz as recommended
			by Luigi Rizzo (luigi@iet.unipi.it)
	12/19/95	Changed field toggle mode to enable (offset 0x3c)
			recommended by luigi@iet.unipi.it
			Added in prototyping, include file, staticizing,
			and DEVFS changes from FreeBSD team.
			Changed the default allocated pages from 151 (NTSC)
			to 217 (PAL).
			Cleaned up some old comments in iic_write().
			Added a Field (even or odd) only capture mode to 
			eliminate the high frequency problems with compression
			algorithms.  Recommended by luigi@iet.unipi.it.
			Changed geometry ioctl so if it couldn't allocated a
			large enough contiguous space, it wouldn't free the
			stuff it already had.
			Added new mode called YUV_422 which delivers the
			data in planer Y followed by U followed by V. This
			differs from the standard YUV_PACKED mode in that
			the chrominance (UV) data is in the correct (different)
			order. This is for programs like vic and mpeg_encode
			so they don't have to reorder the chrominance data.
			Added field count to stats.
			Increment frame count stat if capturing continuous on
			even frame grabs.
			Added my email address to these comments
			(james@cs.uwm.edu) suggested by (luigi@iet.unipt.it :-).
			Changed the user mode signal mechanism to allow the
			user program to be interrupted at the end of a frame
			in any one of the modes.  Added SSIGNAL ioctl.
			Added a SFPS/GFPS ioctl so one may set the frames per
			second that the card catpures.  This code needs to be
			completed.
			Changed the interrupt routine so synchronous capture
			will work on fields or frames and the starting frame
			can be either even or odd.
			Added HALT_N_FRAMES and CONT_N_FRAMES so one could
			stop and continue synchronous capture mode.
			Change the tsleep/wakeup function to wait on mtr
			rather than &read_intr_wait.
	1/22/96		Add option (METEOR_FreeBSD_210) for FreeBSD 2.1
			to compile.
			Changed intr so it only printed errors every 50 times.
			Added unit number to error messages.
			Added get_meteor_mem and enabled range checking.
	1/30/96		Added prelim test stuff for direct video dma transfers
			from Amancio Hasty (hasty@rah.star-gate.com).  Until
			we get some stuff sorted out, this will be ifdef'ed
			with METEOR_DIRECT_VIDEO.  This is very dangerous to
			use at present since we don't check the address that
			is passed by the user!!!!!
	2/26/96		Added special SVIDEO input device type.
	2/27/96		Added meteor_reg.h file and associate types Converted
			meteor.c over to using meteor.h file.  Prompted by
			Lars Jonas Olsson <ljo@po.cwru.edu>.
	2/28/96		Added meteor RGB code from Lars Jonas Olsson
			<ljo@po.cwru.edu>.  I make some mods to this code, so
			I hope it still works as I don't have an rgb card to
			test with.
	2/29/96		<ljo@po.cwru.edu> tested the meteor RGB and supplied
			me with diffs.  Thanks, we now have a working RGB
			version of the driver.  Still need to clean up this
			code and add the 4:1:1, 8:1:1 planer modes.
	3/1/96		Fixed a nasty little bug that was clearing the VTR
			mode bit when the 7116 status was requested.
*/

#include "meteor.h"

#if NMETEOR > 0

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
#include <sys/devconf.h>
#include <sys/mman.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /* DEVFS */
#include <machine/clock.h>
#include <machine/cpu.h>	/* bootverbose */

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <pci.h>
#if NPCI > 0
#include <pci/pcivar.h>
#include <pci/pcireg.h>
#endif
#include <machine/ioctl_meteor.h>
#include <pci/meteor_reg.h>


static void meteor_intr __P((void *arg));

/* 
 * Allocate enough memory for:
 *	768x576 RGB 16 or YUV (16 storage bits/pixel) = 884736 = 216 pages
 *
 * You may override this using the options "METEOR_ALLOC_PAGES=value" in your
 * kernel configuration file.
 */
#ifndef METEOR_ALLOC_PAGES
#define METEOR_ALLOC_PAGES 217
#endif
#define METEOR_ALLOC (METEOR_ALLOC_PAGES * PAGE_SIZE)

static meteor_reg_t meteor[NMETEOR];
#define METEOR_NUM(mtr)	((mtr - &meteor[0])/sizeof(meteor_reg_t))

#define METPRI (PZERO+8)|PCATCH

static	char*	met_probe (pcici_t tag, pcidi_t type);
static	void	met_attach(pcici_t tag, int unit);
static	u_long	met_count;

static struct	pci_device met_device = {
	"meteor",
	met_probe,
	met_attach,
	&met_count
};

DATA_SET (pcidevice_set, met_device);

#if defined(METEOR_FreeBSD_210)	/* XXX */
d_open_t	meteor_open;
d_close_t	meteor_close;
d_read_t	meteor_read;
d_write_t	meteor_write;
d_ioctl_t	meteor_ioctl;
d_mmap_t	meteor_mmap;
#else
static	d_open_t	meteor_open;
static	d_close_t	meteor_close;
static	d_read_t	meteor_read;
static	d_write_t	meteor_write;
static	d_ioctl_t	meteor_ioctl;
static	d_mmap_t	meteor_mmap;

#define CDEV_MAJOR 67
static struct cdevsw meteor_cdevsw = 
        { meteor_open,  meteor_close,   meteor_read,    meteor_write,   /*67*/
          meteor_ioctl, nostop,         nullreset,   nodevtotty,/* Meteor */
          seltrue,	meteor_mmap, NULL,	"meteor",	NULL,	-1 };
#endif

static mreg_t saa7116_pci_default[sizeof(struct saa7116_regs)/sizeof(mreg_t)]={
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
/* 0x3c */	0x00000107,	/*  9:8   *RW	Reserved (0x0)
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
/* 01 */	0x30,	/* 7:0	Horizontal Sync Begin for 50hz		*/
/* 02 */	0x00,	/* 7:0	Horizontal Sync Stop for 50hz		*/
/* 03 */	0xe8,	/* 7:0	Horizontal Sync Clamp Start for 50hz	*/
/* 04 */	0xb6,	/* 7:0	Horizontal Sync Clamp Stop for 50hz 	*/
/* 05 */	0xf4,	/* 7:0	Horizontal Sync Start after PH1 for 50hz */
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
#ifdef notdef
/* 15 */	0x0c,	/* 7:0	Horizontal sync stop for 60hz		*/
/* 16 */	0xfb,	/* 7:0	Horizontal clamp begin for 60hz		*/
/* 17 */	0xd4,	/* 7:0	Horizontal clamp stop for 60hz		*/
/* 18 */	0xec,	/* 7:0	Horizontal sync start after PH1 for 60hz */
#else
		0x0a, 0xf4, 0xce, 0xf4,
#endif
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

static u_char bt254_default[NUM_BT254_REGS] = {
	0x00, 	/* 24 bpp */
	0xa0,
	0xa0,
	0xa0,
	0x50,
	0x50,
	0x50,
} ;

/*
 * i2c_write:
 * Returns	0	Succesful completion.
 * Returns	1	If transfer aborted or timeout occured.
 *
 */
static i2c_print_err = 1;
static int
i2c_write(meteor_reg_t * mtr, u_char slave, u_char rw, u_char reg, u_char data)
{
register unsigned long	wait_counter = 0x0001ffff;
register mreg_t *	iic_write_loc = &mtr->base->i2c_write;
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

	/* 1ffff should be enough delay time for the i2c cycle to complete */
	if(!wait_counter) {
		if(i2c_print_err)
			printf("meteor%d: %d i2c %s transfer timeout 0x%x",
				METEOR_NUM(mtr), slave, 
				rw ? "read" : "write", *iic_write_loc);
			
		err=1;
	} 

	/* Check for error on direct write, clear if any */
	if(mtr->base->i2c_read & SAA7116_IIC_DIRECT_TRANSFER_ABORTED){
		mtr->base->i2c_read |= SAA7116_IIC_DIRECT_TRANSFER_ABORTED;
		if(i2c_print_err)
			printf("meteor%d: 0x%x i2c %s tranfer aborted",
				METEOR_NUM(mtr), slave,
				rw ? "read" : "write" );
		err= 1;
	}

	if(err) {
		if(i2c_print_err)
			printf(" - reg=0x%x, value=0x%x.\n", reg, data);
	}
		
	return err;
}
#undef i2c_print

static	char *
met_probe (pcici_t tag, pcidi_t type)
{
	
	switch (type) {
	case SAA7116_PHILIPS_ID:	/* meteor */
		return("Philips SAA 7116");
	};
	return ((char *)0);
}

	/* interrupt handling routine 
	   complete meteor_read() if using interrupts
	*/
static void
meteor_intr(void *arg)
{
	meteor_reg_t	*mtr	   = (meteor_reg_t *) arg;
	mreg_t		*cap	   = &mtr->base->cap_cntl,
			*base	   = &mtr->base->dma1e,
			*stat	   = &mtr->base->irq_stat;
	u_long		status	   = *stat,
			cap_err	   = *cap & 0x00000f00,
#ifdef METEOR_CHECK_PCI_BUS
			pci_err    = pci_conf_read(mtr->tag,
						PCI_COMMAND_STATUS_REG),
#endif
			next_base  = (u_long)(vtophys(mtr->bigbuf));

	/*
	 * Disable future interrupts if a capture mode is not selected.
	 * This can happen when we are in the process of closing or 
	 * changing capture modes, otherwise it shouldn't happen.
	 */
	if(!(mtr->flags & METEOR_CAP_MASK)) {
		*cap &= 0x8ff0;	/* disable future interrupts */
	}
#ifdef METEOR_CHECK_PCI_BUS
	/*
	 * Check for pci bus errors.
	 */
#define METEOR_MASTER_ABORT	0x20000000
#define METEOR_TARGET_ABORT	0x10000000
	if(pci_err & METEOR_MASTER_ABORT) {
		printf("meteor%d: intr: pci bus master dma abort: 0x%x 0x%x.\n",
			METEOR_NUM(mtr), *base, *(base+3));
		pci_conf_write(mtr->tag, PCI_COMMAND_STATUS_REG, pci_err);
	}
	if(pci_err & METEOR_TARGET_ABORT) {
		printf("meteor%d: intr: pci bus target dma abort: 0x%x 0x%x.\n",
			METEOR_NUM(mtr), *base, *(base+3));
		pci_conf_write(mtr->tag, PCI_COMMAND_STATUS_REG, pci_err);
	}
#endif
	/*
	 * Check for errors.
	 */
	if (cap_err) {
	   if (cap_err & 0x300) {
		if(mtr->fifo_errors % 50 == 0) {
	   		printf("meteor%d: capture error", METEOR_NUM(mtr));
			printf(": %s FIFO overflow.\n",
				cap_err&0x0100? "even" : "odd");
		}
		mtr->fifo_errors++ ;	/* increment fifo capture errors cnt */
	   }
	   if (cap_err & 0xc00) {
		if(mtr->dma_errors % 50 == 0) {
	   		printf("meteor%d: capture error", METEOR_NUM(mtr));
			printf(": %s DMA address.\n",
				cap_err&0x0400? "even" : "odd");
		}
		mtr->dma_errors++ ;	/* increment DMA capture errors cnt */
	   }
	}
	*cap |= 0x0f30;		/* clear error and field done */

	/*
	 * In synchronous capture mode we need to know what the address
	 * offset for the next field/frame will be.  next_base holds the
	 * value for the even dma buffers (for odd, one must add stride).
	 */
	if((mtr->flags & METEOR_SYNCAP) && !mtr->synch_wait &&
	   (mtr->current < mtr->frames)) { /* could be !=, but < is safer */
		/* next_base is initialized to mtr->bigbuf */
		next_base += mtr->frame_size * mtr->current;
	}

	/*
	 * Count the field and clear the field flag.
	 *
	 * In single mode capture, clear the continuous capture mode.
	 *
	 * In synchronous capture mode, if we have room for another field,
	 * adjust DMA buffer pointers.
	 * When we are above the hi water mark (hiwat), mtr->synch_wait will
	 * be set and we will not bump the DMA buffer pointers.  Thus, once
	 * we reach the hi water mark,  the driver acts like a continuous mode
	 * capture on the mtr->current frame until we hit the low water
	 * mark (lowat).  The user had the option of stopping or halting
	 * the capture if this is not the desired effect.
	 */
	if (status & 0x1) {		/* even field */
		mtr->even_fields_captured++;
		mtr->flags &= ~METEOR_WANT_EVEN;
		if((mtr->flags & METEOR_SYNCAP) && !mtr->synch_wait) {
			*base = next_base;
			/* XXX should add adjustments for YUV_422 & PLANER */
		}
	}
	if (status & 0x2) {		/* odd field */
		mtr->odd_fields_captured++;
		mtr->flags &= ~METEOR_WANT_ODD;
		if((mtr->flags & METEOR_SYNCAP) && !mtr->synch_wait) {
			*(base+3) = next_base + *(base+6);
			/* XXX should add adjustments for YUV_422 & PLANER */
		}
	}

	/*
	 * If we have a complete frame.
	 */
	if(!(mtr->flags & METEOR_WANT_MASK)) {
		mtr->frames_captured++;
		/*
		 * Wake up the user in single capture mode.
		 */
		if(mtr->flags & METEOR_SINGLE)
			wakeup((caddr_t)mtr);
		/*
		 * If the user requested to be notified via signal,
		 * let them know the frame is complete.
		 */
		if(mtr->proc && mtr->signal)
			psignal(mtr->proc, mtr->signal);
		/*
		 * Reset the want flags if in continuous or
		 * synchronous capture mode.
		 */
		if(mtr->flags & (METEOR_CONTIN|METEOR_SYNCAP)) {
			switch(mtr->flags & METEOR_ONLY_FIELDS_MASK) {
			case METEOR_ONLY_ODD_FIELDS:
				mtr->flags |= METEOR_WANT_ODD;
				break;
			case METEOR_ONLY_EVEN_FIELDS:
				mtr->flags |= METEOR_WANT_EVEN;
				break;
			default:
				mtr->flags |= METEOR_WANT_MASK;
				break;
			}
		}
		/*
		 * Special handling for synchronous capture mode.
		 */
		if(mtr->flags & METEOR_SYNCAP) {
			struct meteor_mem *mm = mtr->mem;
			/*
			 * Mark the current frame as active.  It is up to
			 * the user to clear this, but we will clear it
			 * for the user for the current frame being captured
			 * if we are within the water marks (see below).
			 */
			mm->active |= 1 << (mtr->current - 1);

			/*
			 * Since the user can muck with these values, we need
			 * to check and see if they are sane. If they don't
			 * pass the sanity check, disable the capture mode.
			 * This is rather rude, but then so was the user.
			 *
			 * Do we really need all of this or should we just
			 * eliminate the possiblity of allowing the
			 * user to change hi and lo water marks while it
			 * is running? XXX
			 */
			if(mm->num_active_bufs < 0 ||
			   mm->num_active_bufs > mtr->frames ||
		   	   mm->lowat < 1 || mm->lowat >= mtr->frames ||
			   mm->hiwat < 1 || mm->hiwat >= mtr->frames ||
			   mm->lowat > mm->hiwat ) {
				*cap &= 0x8ff0;
				mtr->flags &= ~(METEOR_SYNCAP|METEOR_WANT_MASK);
			} else {
				/*
			 	 * Ok, they are sane, now we want to
				 * check the water marks.
			 	 */
				if(mm->num_active_bufs <= mm->lowat)
					mtr->synch_wait = 0;
				if(mm->num_active_bufs >= mm->hiwat)
					mtr->synch_wait = 1;
				/*
				 * Clear the active frame bit for this frame
				 * and advance the counters if we are within
				 * the banks of the water marks. 
				 */
				if(!mtr->synch_wait) {
					mm->active &= ~(1 << mtr->current);
					mtr->current++;
					if(mtr->current > mtr->frames)
						mtr->current = 1;
					mm->num_active_bufs++;
				}
			}
		}
	}

	*stat |=  0x7;		/* clear interrupt status */
	return;
}

static void
set_fps(meteor_reg_t *mtr, u_short fps)
{
	mreg_t	*field_mask_even = &mtr->base->fme;
	mreg_t	*field_mask_odd = &mtr->base->fmo;
	mreg_t	*field_mask_length = &mtr->base->fml;
	int	is_ntsc=1;	/* assume ntsc  */
	unsigned status;

	SAA7196_WRITE(mtr, SAA7196_STDC, SAA7196_REG(mtr, SAA7196_STDC) | 0x02);
	SAA7196_READ(mtr);
	status = (mtr->base->i2c_read & 0xff000000L) >> 24;
	if((status & 0x40) == 0)
		is_ntsc = ((status & 0x20) != 0) ;

	/*
	 * A little sanity checking...
	 */
	if(fps <  1)		 fps = 1;
	if(!is_ntsc && fps > 25) fps = 25;
	if( is_ntsc && fps > 30) fps = 30;
	mtr->fps = fps;	
	/*
	 * Set the fps using the mask/length.
	 */
	/* XXX we need some code to actually do this here... */

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
get_meteor_mem(int unit, unsigned size)
{
vm_offset_t	addr = NULL;

	addr = vm_page_alloc_contig(size, 0x100000, 0xffffffff, 1<<24);
	if(addr == NULL)
		addr = vm_page_alloc_contig(size, 0x100000, 0xffffffff,
								PAGE_SIZE);
	if(addr == NULL) {
		printf("meteor%d: Unable to allocate %d bytes of memory.\n",
			unit, size);
	}

	return addr;
}

static void
bt254_write(meteor_reg_t *mtr, u_char addr, u_char data)
{
	addr &= 0x7;						/* sanity? */
	mtr->bt254_reg[addr] = data;
	PCF8574_DATA_WRITE(mtr, data);				/* set data */
	PCF8574_CTRL_WRITE(mtr, (PCF8574_CTRL_REG(mtr) & ~0x7) | addr);
	PCF8574_CTRL_WRITE(mtr, PCF8574_CTRL_REG(mtr) & ~0x10);	/* WR/ to 0 */
	PCF8574_CTRL_WRITE(mtr, PCF8574_CTRL_REG(mtr) | 0x10);	/* WR to 1 */
	PCF8574_DATA_WRITE(mtr, 0xff);				/* clr data */

}

static void
bt254_init(meteor_reg_t *mtr)
{
int	i;

	PCF8574_CTRL_WRITE(mtr, 0x7f);
	PCF8574_DATA_WRITE(mtr, 0xff);	/* data port must be 0xff */
	PCF8574_CTRL_WRITE(mtr, 0x7f);

	/* init RGB module for 24bpp, composite input */
	for(i=0; i<NUM_BT254_REGS; i++)
		bt254_write(mtr, i, bt254_default[i]);

	bt254_write(mtr, BT254_COMMAND, 0x00);	/* 24 bpp */
}

static void
select_bt254(meteor_reg_t *mtr)
{
	/* disable saa7196, saaen = 1 */
	PCF8574_CTRL_WRITE(mtr, PCF8574_CTRL_REG(mtr) | 0x80);
	/* enable Bt254, bten = 0 */
	PCF8574_CTRL_WRITE(mtr, PCF8574_CTRL_REG(mtr) & ~0x40);
}

static void
select_saa7196(meteor_reg_t *mtr)
{
	/* disable Bt254, bten = 1 */
	PCF8574_CTRL_WRITE(mtr, PCF8574_CTRL_REG(mtr) | 0x40);
	/* enable saa7196, saaen = 0 */
	PCF8574_CTRL_WRITE(mtr, PCF8574_CTRL_REG(mtr) & ~0x80);
}

/*
 * Initialize the 7116, 7196 and the RGB module.
 */
static void
meteor_init ( meteor_reg_t *mtr )
{
	mreg_t	*vbase_addr;
	int 	i;

	/*
	 * Initialize the Philips SAA7116
	 */
	mtr->base->cap_cntl = 0x00000040L;
	vbase_addr = &mtr->base->dma1e;
	for (i = 0 ; i < (sizeof(struct saa7116_regs)/sizeof(mreg_t)); i++)
		*vbase_addr++ = saa7116_pci_default[i];

	/*
	 * Check for the Philips SAA7196
	 */
	i2c_print_err = 0;
	if(i2c_write(mtr, SAA7196_I2C_ADDR, SAA7116_I2C_WRITE, 0, 0xff) == 0) {
		i2c_print_err = 1;
		/*
		 * Initialize 7196
		 */
		for (i = 0; i < NUM_SAA7196_I2C_REGS; i++) 
			SAA7196_WRITE(mtr, i, saa7196_i2c_default[i]);
		/*
		 * Get version number.
		 */
		SAA7196_WRITE(mtr, SAA7196_STDC,
			SAA7196_REG(mtr, SAA7196_STDC) & ~0x02);
		SAA7196_READ(mtr);
		printf("meteor%d: <Philips SAA 7196> rev 0x%x\n",
			METEOR_NUM(mtr), (mtr->base->i2c_read&0xff000000L)>>28);
	} else {
		i2c_print_err = 1;
		printf("meteor%d: <Philips SAA 7196 NOT FOUND>\n",
			METEOR_NUM(mtr));
	}
	/*
	 * Check for RGB module, initialized if found.
	 */
	i2c_print_err = 0;
	if(i2c_write(mtr,PCF8574_DATA_I2C_ADDR,SAA7116_I2C_WRITE,0,0xff) == 0) {
		i2c_print_err = 1;
		printf("meteor%d: <Booktree 254 (RGB module)>\n",
			METEOR_NUM(mtr));	/* does this have a rev #? */
		bt254_init(mtr);	/* Set up RGB module */
		mtr->flags = METEOR_RGB;
	} else {
		i2c_print_err = 1;
		mtr->flags = 0;
	}

	set_fps(mtr, 30);

}

static	void
met_attach(pcici_t tag, int unit)
{
#ifdef METEOR_IRQ
	u_long old_irq, new_irq;
#endif METEOR_IRQ
	meteor_reg_t *mtr;
	vm_offset_t buf;
	u_long latency;

	if (unit >= NMETEOR) {
		printf("meteor%d: attach: only %d units configured.\n",
				unit, NMETEOR);
		printf("meteor%d: attach: invalid unit number.\n", unit);
        	return ;
	}

	mtr = &meteor[unit];
	mtr->tag = tag;
	pci_map_mem(tag, PCI_MAP_REG_START, (vm_offset_t *)&mtr->base,
				&mtr->phys_base);

#ifdef METEOR_IRQ		/* from the configuration file */
	old_irq = pci_conf_read(tag, PCI_INTERRUPT_REG);
	pci_conf_write(tag, PCI_INTERRUPT_REG, METEOR_IRQ);
	new_irq = pci_conf_read(tag, PCI_INTERRUPT_REG);
	printf("meteor%d: attach: irq changed from %d to %d\n",
		unit, (old_irq & 0xff), (new_irq & 0xff));
#endif METEOR_IRQ
				/* setup the interrupt handling routine */
	pci_map_int(tag, meteor_intr, (void*) mtr, &net_imask); 

/*
 * PCI latency timer.  32 is a good value for 4 bus mastering slots, if
 * you have more than for, then 16 would probably be a better value.
 *
 */
#ifndef METEOR_DEF_LATENCY_VALUE
#define METEOR_DEF_LATENCY_VALUE	32	
#endif
	latency = pci_conf_read(tag, PCI_LATENCY_TIMER);
	latency = (latency >> 8) & 0xff;
	if(bootverbose) {
		if(latency)
			printf("meteor%d: PCI bus latency is", unit);
		else
			printf("meteor%d: PCI bus latency was 0 changing to",
				unit);
	}
	if(!latency) {
		latency = METEOR_DEF_LATENCY_VALUE;
		pci_conf_write(tag, PCI_LATENCY_TIMER,  latency<<8);
	}
	if(bootverbose) {
		printf(" %d.\n", latency);
	}

	meteor_init(mtr);	/* set up saa7116, saa7196, and rgb module */

	if(METEOR_ALLOC)
		buf = get_meteor_mem(unit, METEOR_ALLOC);
	else
		buf = NULL;
	if(bootverbose) {
		printf("meteor%d: buffer size %d, addr 0x%x\n",
			unit, METEOR_ALLOC, vtophys(buf));
	}

	mtr->bigbuf = buf;
	mtr->alloc_pages = METEOR_ALLOC_PAGES;
	if(buf != NULL) {
		bzero((caddr_t) buf, METEOR_ALLOC);
		buf = vtophys(buf);
					/* 640x480 RGB 16 */
		mtr->base->dma1e = buf;
		mtr->base->dma1o = buf + 0x500;
		mtr->base->dma_end_e = 
		mtr->base->dma_end_o = buf + METEOR_ALLOC;
	}
	/* 1 frame of 640x480 RGB 16 */
	mtr->cols = 640;
	mtr->rows = 480;
	mtr->depth = 2;		/* two bytes per pixel */
	mtr->frames = 1;	/* one frame */

    	mtr->flags |= METEOR_INITALIZED | METEOR_AUTOMODE | METEOR_DEV0 |
		   METEOR_RGB16;
#ifdef DEVFS
	mtr->devfs_token = devfs_add_devsw( "/", "meteor", &meteor_cdevsw, unit,
						DV_CHR, 0, 0, 0644);
#endif
}

#define UNIT(x)	((x) & 0x07)

static int
meteor_reset(dev_t dev)
{
int			unit = UNIT(minor(dev));
struct	saa7116_regs	*m;

	if(unit >= NMETEOR)
		return ENXIO;

	m = meteor[unit].base;

	m->cap_cntl = 0x0;
	tsleep((caddr_t)m, METPRI, "Mreset", hz/50);

	m->cap_cntl = 0x8ff0;
	m->cap_cntl = 0x80c0;
	m->cap_cntl = 0x8040;
	tsleep((caddr_t)m, METPRI, "Mreset", hz/10);
	m->cap_cntl = 0x80c0;

	return 0;

}


/*---------------------------------------------------------
**
**	Meteor character device driver routines
**
**---------------------------------------------------------
*/


int
meteor_open(dev_t dev, int flags, int fmt, struct proc *p)
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
	mtr->even_fields_captured = 0;
	mtr->odd_fields_captured = 0;
	mtr->proc = (struct proc *)0;
	set_fps(mtr, 30);
#ifdef METEOR_TEST_VIDEO
	mtr->video.addr = 0;
	mtr->video.width = 0;
	mtr->video.banksize = 0;
	mtr->video.ramsize = 0;
#endif

	return(0);
}

int
meteor_close(dev_t dev, int flags, int fmt, struct proc *p)
{
	meteor_reg_t *mtr;
	int	unit; 
	int	temp;

	unit = UNIT(minor(dev));
	if (unit >= NMETEOR)	/* unit out of range */
		return(ENXIO);

	mtr = &(meteor[unit]);
	mtr->flags &= ~METEOR_OPEN;

	if(mtr->flags & METEOR_SINGLE)
				/* this should not happen, the read capture 
				  should have completed or in the very least
				  recieved a signal before close is called. */
		wakeup((caddr_t)mtr);	/* continue read */
	/*
	 * Turn off capture mode.
	 */
	mtr->base->cap_cntl = 0x8ff0;
	mtr->flags &= ~(METEOR_CAP_MASK|METEOR_WANT_MASK);

#ifdef METEOR_DEALLOC_PAGES
	if (mtr->bigbuf != NULL) {
		kmem_free(kernel_map,mtr->bigbuf,(mtr->alloc_pages*PAGE_SIZE));
		mtr->bigbuf = NULL;
		mtr->alloc_pages = 0;
	}
#else
#ifdef METEOR_DEALLOC_ABOVE
	if (mtr->bigbuf != NULL && mtr->alloc_pages > METEOR_DEALLOC_ABOVE) {
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

static void
start_capture(meteor_reg_t *mtr, unsigned type)
{
mreg_t *cap = &mtr->base->cap_cntl;

	mtr->flags |= type;
	switch(mtr->flags & METEOR_ONLY_FIELDS_MASK) {
	case METEOR_ONLY_EVEN_FIELDS:
		mtr->flags |= METEOR_WANT_EVEN;
		if(type == METEOR_SINGLE)
			*cap = 0x0ff4 | mtr->range_enable;
		else
			*cap = 0x0ff1 | mtr->range_enable;
		break;
	case METEOR_ONLY_ODD_FIELDS:
		mtr->flags |= METEOR_WANT_ODD;
		if(type == METEOR_SINGLE)
			*cap = 0x0ff8 | mtr->range_enable;
		else
			*cap = 0x0ff2 | mtr->range_enable;
		break;
	default:
		mtr->flags |= METEOR_WANT_MASK;
		if(type == METEOR_SINGLE)
			*cap = 0x0ffc | mtr->range_enable;
		else
			*cap = 0x0ff3 | mtr->range_enable;
		break;
	}
}

int
meteor_read(dev_t dev, struct uio *uio, int ioflag)
{
	meteor_reg_t *mtr;
	int	unit; 
	int	status;
	int	count;

	unit = UNIT(minor(dev));
	if (unit >= NMETEOR)	/* unit out of range */
		return(ENXIO);

	mtr = &(meteor[unit]);
	if (mtr->bigbuf == NULL)/* no frame buffer allocated (ioctl failed) */
		return(ENOMEM);

	if (mtr->flags & METEOR_CAP_MASK)
		return(EIO);		/* already capturing */

	count = mtr->rows * mtr->cols * mtr->depth;
	if (uio->uio_iov->iov_len < count)
		return(EINVAL);

	/* Start capture */
	start_capture(mtr, METEOR_SINGLE);

	status=tsleep((caddr_t)mtr, METPRI, "capturing", 0);
	if (!status) 		/* successful capture */
		status = uiomove((caddr_t)mtr->bigbuf, count, uio);
	else
		printf ("meteor%d: read: tsleep error %d\n", unit, status);

	mtr->flags &= ~(METEOR_SINGLE | METEOR_WANT_MASK);

	return(status);
}

int
meteor_write(dev_t dev, struct uio *uio, int ioflag)
{
	return(0);
}

int
meteor_ioctl(dev_t dev, int cmd, caddr_t arg, int flag, struct proc *pr)
{
	int	error;  
	int	unit;   
	unsigned int	temp;
	meteor_reg_t *mtr;
	struct meteor_counts *cnt;
	struct meteor_geomet *geo;
	struct meteor_mem *mem;
	struct meteor_capframe *frame;
#ifdef METEOR_TEST_VIDEO
	struct meteor_video *video;
#endif
	mreg_t *p;
	vm_offset_t buf;

	error = 0;

	if (!arg) return(EINVAL);
	unit = UNIT(minor(dev));
	if (unit >= NMETEOR)	/* unit out of range */
		return(ENXIO);

	mtr = &(meteor[unit]);

	switch (cmd) {
#ifdef METEOR_TEST_VIDEO
	case METEORGVIDEO:
		video = (struct meteor_video *)arg;
		video->addr = mtr->video.addr;
		video->width = mtr->video.width;
		video->banksize = mtr->video.banksize;
		video->ramsize = mtr->video.ramsize;
		break;
	case METEORSVIDEO:
		video = (struct meteor_video *)arg;
		mtr->video.addr = video->addr;
		mtr->video.width = video->width;
		mtr->video.banksize = video->banksize;
		mtr->video.ramsize = video->ramsize;
		break;
#endif
	case METEORSFPS:
		set_fps(mtr, *(u_short *)arg);
		break;
	case METEORGFPS:
		*(u_short *)arg = mtr->fps;
		break;
	case METEORSSIGNAL:
		mtr->signal = *(int *) arg;
		mtr->proc = pr;
		break;
	case METEORGSIGNAL:
		*(int *)arg = mtr->signal;
		break;
	case METEORSTATUS:	/* get 7196 status */
		temp = 0;
		p = &mtr->base->i2c_read;
		SAA7196_WRITE(mtr, SAA7196_STDC,
			SAA7196_REG(mtr, SAA7196_STDC) | 0x02);
		SAA7196_READ(mtr);
		temp |= (*p & 0xff000000L) >> 24;
		SAA7196_WRITE(mtr, SAA7196_STDC,
			SAA7196_REG(mtr, SAA7196_STDC) & ~0x02);
		SAA7196_READ(mtr);
		temp |= (*p & 0xff000000L) >> 16;
		*(u_short *)arg = temp;
		break;
	case METEORSHUE:	/* set hue */
		SAA7196_WRITE(mtr, SAA7196_HUEC, *(char *)arg);
		break;
	case METEORGHUE:	/* get hue */
		*(char *)arg = SAA7196_REG(mtr, SAA7196_HUEC);
		break;
	case METEORSCHCV:	/* set chrominance gain */
		SAA7196_WRITE(mtr, SAA7196_CGAINR, *(char *)arg);
		break;
	case METEORGCHCV:	/* get chrominance gain */
		*(char *)arg = SAA7196_REG(mtr, SAA7196_CGAINR);
		break;
	case METEORSBRIG:	/* set brightness */
		SAA7196_WRITE(mtr, SAA7196_BRIG, *(char *)arg);
		break;
	case METEORGBRIG:	/* get brightness */
		*(char *)arg = SAA7196_REG(mtr, SAA7196_BRIG);
		break;
	case METEORSCSAT:	/* set chroma saturation */
		SAA7196_WRITE(mtr, SAA7196_CSAT, *(char *)arg);
		break;
	case METEORGCSAT:	/* get chroma saturation */
		*(char *)arg = SAA7196_REG(mtr, SAA7196_CSAT);
		break;
	case METEORSCONT:	/* set contrast */
		SAA7196_WRITE(mtr, SAA7196_CONT, *(char *)arg);
		break;
	case METEORGCONT:	/* get contrast */
		*(char *)arg = SAA7196_REG(mtr, SAA7196_CONT);
		break;
	case METEORSBT254:
		if((mtr->flags & METEOR_RGB) == 0)
			return EINVAL;
		temp = *(unsigned short *)arg;
		bt254_write(mtr, temp & 0xf, (temp & 0x0ff0) >> 4);
		break;
	case METEORGBT254:
		if((mtr->flags & METEOR_RGB) == 0)
			return EINVAL;
		temp = *(unsigned short *)arg & 0x7;
		*(unsigned short *)arg = mtr->bt254_reg[temp] << 4 | temp;
		break;
	case METEORSHWS:	/* set horizontal window start */
		SAA7196_WRITE(mtr, SAA7196_HWS, *(char *)arg);
	break;
	case METEORGHWS:	/* get horizontal window start */
		*(char *)arg = SAA7196_REG(mtr, SAA7196_HWS);
		break;
	case METEORSVWS:	/* set vertical window start */
		SAA7196_WRITE(mtr, SAA7196_VWS, *(char *)arg);
		break;
	case METEORGVWS:	/* get vertical window start */
		*(char *)arg = SAA7196_REG(mtr, SAA7196_VWS);
		break;
	case METEORSINPUT:	/* set input device */
		switch(*(unsigned long *)arg & METEOR_DEV_MASK) {
		case 0:			/* default */
		case METEOR_INPUT_DEV0:
			if(mtr->flags & METEOR_RGB)
				select_saa7196(mtr);
			mtr->flags = (mtr->flags & ~METEOR_DEV_MASK)
				| METEOR_DEV0;
			SAA7196_WRITE(mtr, 0x0e,
				(SAA7196_REG(mtr, 0x0e) & ~0x3) | 0x0);
			SAA7196_WRITE(mtr, 0x06,
				(SAA7196_REG(mtr, 0x06) & ~0x80));
			break;
		case METEOR_INPUT_DEV1:
			if(mtr->flags & METEOR_RGB)
				select_saa7196(mtr);
			mtr->flags = (mtr->flags & ~METEOR_DEV_MASK)
					       | METEOR_DEV1;
			SAA7196_WRITE(mtr, 0x0e,
				(SAA7196_REG(mtr, 0x0e) & ~0x3) | 0x1);
			SAA7196_WRITE(mtr, 0x06,
				(SAA7196_REG(mtr, 0x06) & ~0x80));
			break;
		case METEOR_INPUT_DEV2:
			if(mtr->flags & METEOR_RGB)
				select_saa7196(mtr);
			mtr->flags = (mtr->flags & ~METEOR_DEV_MASK)
					       | METEOR_DEV2;
			SAA7196_WRITE(mtr, 0x0e,
				(SAA7196_REG(mtr, 0x0e) & ~0x3) | 0x2);
			SAA7196_WRITE(mtr, 0x06,
				(SAA7196_REG(mtr, 0x06) & ~0x80));
			break;
		case METEOR_INPUT_DEV3:
			if(mtr->flags & METEOR_RGB)
				select_saa7196(mtr);
			mtr->flags = (mtr->flags & ~METEOR_DEV_MASK)
					       | METEOR_DEV3;
			SAA7196_WRITE(mtr, 0x0e,
				(SAA7196_REG(mtr, 0x0e) | 0x3));
			SAA7196_WRITE(mtr, 0x06,
				(SAA7196_REG(mtr, 0x06) & ~0x80) );
			break;
		case METEOR_INPUT_DEV_SVIDEO:
			if(mtr->flags & METEOR_RGB)
				select_saa7196(mtr);
			mtr->flags = (mtr->flags & ~METEOR_DEV_MASK)
					       | METEOR_DEV_SVIDEO;
			SAA7196_WRITE(mtr, 0x0e,
				(SAA7196_REG(mtr, 0x0e) & ~0x3) | 0x2);
			SAA7196_WRITE(mtr, 0x06,
				(SAA7196_REG(mtr, 0x06) & ~0x80) | 0x80);
			break;
		case METEOR_INPUT_DEV_RGB:
			if((mtr->flags & METEOR_RGB) == 0)
				return EINVAL;
			mtr->flags = (mtr->flags & ~METEOR_DEV_MASK)
					       | METEOR_DEV_RGB;
			SAA7196_WRITE(mtr, 0x0e,
				(SAA7196_REG(mtr, 0x0e) & ~0x3) | 0x3);
			SAA7196_WRITE(mtr, 0x06,
				(SAA7196_REG(mtr, 0x06) & ~0x80));
			select_bt254(mtr);
			SAA7196_WRITE(mtr, 0x0e,	/* chn 3 for synch */
				(SAA7196_REG(mtr, 0x0e) & ~0x3) | 0x3);
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
			SAA7196_WRITE(mtr, SAA7196_STDC, 
				(SAA7196_REG(mtr, SAA7196_STDC) & ~0x01));
			SAA7196_WRITE(mtr, 0x0f,
				(SAA7196_REG(mtr, 0x0f) & ~0xe0) | 0x40);
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
			SAA7196_WRITE(mtr, SAA7196_STDC, 
				(SAA7196_REG(mtr, SAA7196_STDC) & ~0x01));
			SAA7196_WRITE(mtr, 0x0f, 
				(SAA7196_REG(mtr, 0x0f) & ~0xe0));
			SAA7196_WRITE(mtr, 0x22, 0x00);
			SAA7196_WRITE(mtr, 0x24, 
				(SAA7196_REG(mtr, 0x24) | 0x0c));
			SAA7196_WRITE(mtr, 0x26, 0x20);
			SAA7196_WRITE(mtr, 0x28, 
				(SAA7196_REG(mtr, 0x28) & ~0x0c) | 0x04) ;
		break;
		case METEOR_FMT_SECAM:
			mtr->flags = (mtr->flags & ~METEOR_FORM_MASK) |
				METEOR_SECAM;
			SAA7196_WRITE(mtr, SAA7196_STDC, 
				(SAA7196_REG(mtr, SAA7196_STDC) & ~0x01) | 0x1);
			SAA7196_WRITE(mtr, 0x0f, 
				(SAA7196_REG(mtr, 0x0f) & ~0xe0) | 0x20);
			SAA7196_WRITE(mtr, 0x22, 0x00);
			SAA7196_WRITE(mtr, 0x24, 
				(SAA7196_REG(mtr, 0x24) | 0x0c));
			SAA7196_WRITE(mtr, 0x26, 0x20);
			SAA7196_WRITE(mtr, 0x28, 
				(SAA7196_REG(mtr, 0x28) & ~0x0c) | 0x04) ;
		break;
		case METEOR_FMT_AUTOMODE:
			mtr->flags = (mtr->flags & ~METEOR_FORM_MASK) |
				METEOR_AUTOMODE;
			SAA7196_WRITE(mtr, SAA7196_STDC, 
				(SAA7196_REG(mtr, SAA7196_STDC) & ~0x01));
			SAA7196_WRITE(mtr, 0x0f, 
				(SAA7196_REG(mtr, 0x0f) & ~0xe0) | 0x80);
		break;
		default:
			return EINVAL;
		}
		break;
	case METEORGFMT:	/* get input format */
		*(u_long *)arg = mtr->flags & METEOR_FORM_MASK;
		break;
	case METEORCAPTUR:
		temp = mtr->flags;
		switch (*(int *) arg) {
		case METEOR_CAP_SINGLE:
			if (mtr->bigbuf==NULL)	/* no frame buffer allocated */
				return(ENOMEM);

			if (temp & METEOR_CAP_MASK)
				return(EIO);		/* already capturing */

			start_capture(mtr, METEOR_SINGLE);

			/* wait for capture to complete */
			error=tsleep((caddr_t)mtr, METPRI, "capturing", 0);
			if(error)
				printf("meteor%d: ioctl: tsleep error %d\n",
					unit, error);
			mtr->flags &= ~(METEOR_SINGLE|METEOR_WANT_MASK);
			break;
		case METEOR_CAP_CONTINOUS:
			if (mtr->bigbuf==NULL)	/* no frame buffer allocated */
				return(ENOMEM);

			if (temp & METEOR_CAP_MASK)
				return(EIO);		/* already capturing */

			start_capture(mtr, METEOR_CONTIN);

			break;
		case METEOR_CAP_STOP_CONT:
			if (mtr->flags & METEOR_CONTIN) {
							/* turn off capture */
				mtr->base->cap_cntl = 0x8ff0;
				mtr->flags &= ~(METEOR_CONTIN|METEOR_WANT_MASK);
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
		if (mtr->flags & (METEOR_YUV_PLANER | METEOR_YUV_422)) /* XXX*/
			return(EINVAL); /* should fix intr so we allow these */
		if (mtr->bigbuf == NULL)
			return(ENOMEM);
		if ((mtr->frames < 2) ||
		    (frame->lowat < 1 || frame->lowat >= mtr->frames) ||
		    (frame->hiwat < 1 || frame->hiwat >= mtr->frames) ||
		    (frame->lowat > frame->hiwat)) 
			return(EINVAL);
			/* meteor_mem structure is on the page after the data */
		mem = mtr->mem = (struct meteor_mem *) (mtr->bigbuf +
				((mtr->rows*mtr->cols * mtr->depth *
				mtr->frames+PAGE_SIZE-1)/PAGE_SIZE)*PAGE_SIZE);
		mtr->current = 1;
		mtr->synch_wait = 0;
        	mem->num_bufs = mtr->frames;
		mem->frame_size=
			mtr->frame_size = mtr->rows * mtr->cols * mtr->depth;
                /* user and kernel change these */ 
		mem->lowat = frame->lowat;
		mem->hiwat = frame->hiwat;
        	mem->active = 0;
        	mem->num_active_bufs = 0;
		/* Start capture */
		start_capture(mtr, METEOR_SYNCAP);
		break;
	    case METEOR_CAP_STOP_FRAMES:
		if (mtr->flags & METEOR_SYNCAP) {
						/* turn off capture */
			mtr->base->cap_cntl = 0x8ff0;
			mtr->flags &= ~(METEOR_SYNCAP|METEOR_WANT_MASK);
		}
		break;
	    case METEOR_HALT_N_FRAMES:
		if(mtr->flags & METEOR_SYNCAP) {
			mtr->base->cap_cntl = 0x8ff0;
			mtr->flags &= ~(METEOR_WANT_MASK);
		}
		break;
	    case METEOR_CONT_N_FRAMES:
		if(!(mtr->flags & METEOR_SYNCAP)) {
			error = EINVAL;
			break;
		}
		start_capture(mtr, METEOR_SYNCAP);
		break;
	    default:
		error = EINVAL;
		break;
	    }
	    break;
 
	case METEORSETGEO:
		geo = (struct meteor_geomet *) arg;

		/* Either even or odd, if even & odd, then these a zero */
		if((geo->oformat & METEOR_GEO_ODD_ONLY) &&
			(geo->oformat & METEOR_GEO_EVEN_ONLY)) {
			printf("meteor%d: ioctl: Geometry odd or even only.\n",
				unit);
			return EINVAL;
		}
		/* set/clear even/odd flags */
		if(geo->oformat & METEOR_GEO_ODD_ONLY)
			mtr->flags |= METEOR_ONLY_ODD_FIELDS;
		else
			mtr->flags &= ~METEOR_ONLY_ODD_FIELDS;
		if(geo->oformat & METEOR_GEO_EVEN_ONLY)
			mtr->flags |= METEOR_ONLY_EVEN_FIELDS;
		else
			mtr->flags &= ~METEOR_ONLY_EVEN_FIELDS;

		/* can't change parameters while capturing */
		if (mtr->flags & METEOR_CAP_MASK)
			return(EBUSY);

		if ((geo->columns & 0x3fe) != geo->columns) {
			printf(
			"meteor%d: ioctl: %d: columns too large or not even.\n",
				unit, geo->columns);
			error = EINVAL;
		}
		if (((geo->rows & 0x7fe) != geo->rows) ||
			((geo->oformat & METEOR_GEO_FIELD_MASK) &&
				((geo->rows & 0x3fe) != geo->rows)) ) {
			printf(
			"meteor%d: ioctl: %d: rows too large or not even.\n",
				unit, geo->rows);
			error = EINVAL;
		}
		if (geo->frames > 32) {
			printf("meteor%d: ioctl: too many frames.\n", unit);
			error = EINVAL;
		}
		if(error) return error;

		if (temp=geo->rows * geo->columns * geo->frames * 2) {
			if (geo->oformat & METEOR_GEO_RGB24) temp = temp * 2;

		   	/* meteor_mem structure for SYNC Capture */
		   	if (geo->frames > 1) temp += PAGE_SIZE;

		   	temp = (temp + PAGE_SIZE -1)/PAGE_SIZE;
		   	if (temp > mtr->alloc_pages
#ifdef METEOR_TEST_VIDEO
			    && mtr->video.addr == 0
#endif
			) {
				buf = get_meteor_mem(unit, temp*PAGE_SIZE);
				if(buf != NULL) {
					kmem_free(kernel_map, mtr->bigbuf,
					  (mtr->alloc_pages * PAGE_SIZE));
					mtr->bigbuf = buf;
					mtr->alloc_pages = temp;
					if(bootverbose)
						printf(
				"meteor%d: ioctl: Allocating %d bytes\n",
							unit, temp*PAGE_SIZE);
				} else {
					error = ENOMEM;
				}
		   	}
		}
		if(error) return error;

		mtr->rows = geo->rows;
		mtr->cols = geo->columns;
		mtr->frames = geo->frames;

		p = &mtr->base->dma1e;
#ifdef METEOR_TEST_VIDEO
		if(mtr->video.addr)
			buf = vtophys(mtr->video.addr);
		else
#endif
			buf = vtophys(mtr->bigbuf);

		/* set defaults and end of buffer locations */
		*(p+0)  = buf;	/* DMA 1 even    */
		*(p+1)  = buf;	/* DMA 2 even    */
		*(p+2)  = buf;	/* DMA 3 even    */
		*(p+3)  = buf;	/* DMA 1 odd     */
		*(p+4)  = buf;	/* DMA 2 odd	 */
		*(p+5)  = buf;	/* DMA 3 odd     */
		*(p+6)  = 0;	/* Stride 1 even */
		*(p+7)  = 0;	/* Stride 2 even */
		*(p+8)  = 0;	/* Stride 3 even */
		*(p+9)  = 0;	/* Stride 1 odd  */
		*(p+10) = 0;	/* Stride 2 odd  */
		*(p+11) = 0;	/* Stride 3 odd  */
				/* set end of DMA location, even/odd */
		if(mtr->range_enable)
			*(p+35) = *(p+36) = buf + mtr->alloc_pages * PAGE_SIZE;
		else
			*(p+35) = *(p+36) = 0xffffffff;

		/*
		 * Determine if we can use the hardware range detect.
		 */
		if(mtr->alloc_pages * PAGE_SIZE < RANGE_BOUNDARY &&
		  ((buf & 0xff000000) | *(p+35)) == (buf + mtr->alloc_pages *
							PAGE_SIZE) )
                        mtr->range_enable = 0x8000;
		else
			mtr->range_enable = 0x0;


		switch (geo->oformat & METEOR_GEO_OUTPUT_MASK) {
		case 0:			/* default */
		case METEOR_GEO_RGB16:
			mtr->depth = 2;
			mtr->flags &= ~METEOR_OUTPUT_FMT_MASK;
			mtr->flags |= METEOR_RGB16;
			temp = mtr->cols * mtr->depth;
		      	/* recal stride and starting point */
			switch(mtr->flags & METEOR_ONLY_FIELDS_MASK) {
			case METEOR_ONLY_ODD_FIELDS:
				*(p+3) = buf;		/*dma 1 o */
#ifdef METEOR_TEST_VIDEO
				if(mtr->video.addr && mtr->video.width) 
					*(p+9) = mtr->video.width - temp;
#endif
				SAA7196_WRITE(mtr, 0x20, 0xd0);
				break;
			case METEOR_ONLY_EVEN_FIELDS:
				*(p+0) = buf;		/*dma 1 e */
#ifdef METEOR_TEST_VIDEO
				if(mtr->video.addr && mtr->video.width) 
					*(p+6) = mtr->video.width - temp;
#endif
				SAA7196_WRITE(mtr, 0x20, 0xf0);
				break;
			default: /* interlaced even/odd */
				*(p+0) = buf;		
				*(p+3) = buf + temp;
				*(p+6) = *(p+9) = temp;
#ifdef METEOR_TEST_VIDEO
				if(mtr->video.addr && mtr->video.width) {
					*(p+3) = buf + mtr->video.width;
					*(p+6) = *(p+9) = mtr->video.width -
						temp + mtr->video.width;
				}
#endif
				SAA7196_WRITE(mtr, 0x20, 0x90);
				break;
			}
	 		*(p+12) = *(p+13) = 0xeeeeee01;	/* routes */
			break;
		case METEOR_GEO_RGB24:
			mtr->depth = 4;
			mtr->flags &= ~METEOR_OUTPUT_FMT_MASK;
			mtr->flags |= METEOR_RGB24;
			temp = mtr->cols * mtr->depth;
			/* recal stride and starting point */
			switch(mtr->flags & METEOR_ONLY_FIELDS_MASK) {
			case METEOR_ONLY_ODD_FIELDS:
					*(p+3) = buf;		/*dma 1 o */
#ifdef METEOR_TEST_VIDEO
				if(mtr->video.addr && mtr->video.width) 
					*(p+9) = mtr->video.width - temp;
#endif
					SAA7196_WRITE(mtr, 0x20, 0xd2);
				break;
			case METEOR_ONLY_EVEN_FIELDS:
					*(p+0) = buf;		/*dma 1 e */
#ifdef METEOR_TEST_VIDEO
				if(mtr->video.addr && mtr->video.width) 
					*(p+6) = mtr->video.width - temp;
#endif
					SAA7196_WRITE(mtr, 0x20, 0xf2);
				break;
			default: /* interlaced even/odd */
				*(p+0) = buf;
				*(p+3) = buf + mtr->cols * mtr->depth;
				*(p+6) = *(p+9) = mtr->cols * mtr->depth;
#ifdef METEOR_TEST_VIDEO
				if(mtr->video.addr && mtr->video.width) {
					*(p+3) = buf + mtr->video.width;
					*(p+6) = *(p+9) = mtr->video.width -
						temp + mtr->video.width;
				}
#endif
				SAA7196_WRITE(mtr, 0x20, 0x92);
				break;
			}
			*(p+12) = *(p+13) = 0x39393900;	/* routes */
			break;
		case METEOR_GEO_YUV_PLANER:
			mtr->depth = 2;
			mtr->flags &= ~METEOR_OUTPUT_FMT_MASK;
			mtr->flags |= METEOR_YUV_PLANER;
			/* recal stride and starting point */
			temp = mtr->rows * mtr->cols;	/* compute frame size */
			switch(mtr->flags & METEOR_ONLY_FIELDS_MASK) {
			case METEOR_ONLY_ODD_FIELDS:
				*(p+3) = buf;		/* Y Odd */
				*(p+4) = buf + temp;	/* U Odd */
				temp >>= 1;
				*(p+5) = *(p+4) + temp; /* V Odd */
				SAA7196_WRITE(mtr, 0x20, 0xd1);
				break;
			case METEOR_ONLY_EVEN_FIELDS:
				*(p+0) = buf;		/* Y Even */
				*(p+1) = buf + temp;	/* U Even */
				temp >>= 1;
				*(p+2) = *(p+1) + temp;	/* V Even */
				SAA7196_WRITE(mtr, 0x20, 0xf1);
				break;
			default: /* interlaced even/odd */
				*(p+0) = buf;			/* Y Even */
				*(p+1) = buf + temp;		/* U Even */
				temp >>= 2;
				*(p+2) = *(p+1) + temp;		/* V Even */
				*(p+3) = *(p+0) + mtr->cols;	/* Y Odd */
				*(p+4) = *(p+2) + temp;		/* U Odd */
				*(p+5) = *(p+4) + temp;		/* V Odd */
				*(p+6) = *(p+9) = mtr->cols;	/* Y Stride */
				SAA7196_WRITE(mtr, 0x20, 0x91);
				break;
			}
			*(p+12) = *(p+13) = 0xaaaaffc1;	/* routes */
			break;
		case METEOR_GEO_YUV_422:/* same as planer, different uv order */
			mtr->depth = 2;
			mtr->flags &= ~METEOR_OUTPUT_FMT_MASK;
			mtr->flags |= METEOR_YUV_422;
			temp = mtr->rows * mtr->cols;	/* compute frame size */
			switch(mtr->flags & METEOR_ONLY_FIELDS_MASK) {
			case METEOR_ONLY_ODD_FIELDS:
				*(p+3) = buf;
				*(p+4) = buf + temp;
				*(p+5) = *(p+4) + (temp >> 1);
				SAA7196_WRITE(mtr, 0x20, 0xd1);
				break;
			case METEOR_ONLY_EVEN_FIELDS:
				*(p+0) = buf;
				*(p+1) = buf + temp;
				*(p+2) = *(p+1) + (temp >> 1);
				SAA7196_WRITE(mtr, 0x20, 0xf1);
				break;
			default: /* interlaced even/odd */
				*(p+0) = buf;			/* Y even */
				*(p+1) = buf + temp;		/* U even */
				*(p+2) = *(p+1) + (temp >> 1);	/* V even */
				*(p+3) = *(p+0) + mtr->cols;	/* Y odd */
				temp = mtr->cols >> 1;
				*(p+4) = *(p+1) + temp;		/* U odd */
				*(p+5) = *(p+2) + temp;		/* V odd */
				*(p+6) = *(p+9)  = mtr->cols;	/* Y stride */
				*(p+7) = *(p+10) = temp;	/* U stride */
				*(p+8) = *(p+11) = temp;	/* V stride */
				SAA7196_WRITE(mtr, 0x20, 0x91);
				break;
			}
			*(p+12) = *(p+13) = 0xaaaaffc1;	/* routes */
			break;
		case METEOR_GEO_YUV_PACKED:
			mtr->depth = 2;
			mtr->flags &= ~METEOR_OUTPUT_FMT_MASK;
			mtr->flags |= METEOR_YUV_PACKED;
			/* recal stride and odd starting point */
			switch(mtr->flags & METEOR_ONLY_FIELDS_MASK) {
			case METEOR_ONLY_ODD_FIELDS:
				*(p+3) = buf;
				SAA7196_WRITE(mtr, 0x20, 0xd1);
				break;
			case METEOR_ONLY_EVEN_FIELDS:
				*(p+0) = buf;
				SAA7196_WRITE(mtr, 0x20, 0xf1);
				break;
			default: /* interlaced even/odd */
				*(p+0) = buf;
				*(p+3) = buf + mtr->cols * mtr->depth;
				*(p+6) = *(p+9) = mtr->cols * mtr->depth;
				SAA7196_WRITE(mtr, 0x20, 0x91);
				break;
			}
			*(p+12) = *(p+13) = 0xeeeeee41;	/* routes */
			break;
		default:
			error = EINVAL;	/* invalid argument */
			printf("meteor%d: ioctl: invalid output format\n",unit);
			break;
		}
		/* set cols */
		SAA7196_WRITE(mtr, 0x21, mtr->cols & 0xff);
		SAA7196_WRITE(mtr, 0x24,
				((SAA7196_REG(mtr, 0x24) & ~0x03) |
				((mtr->cols >> 8) & 0x03)));
		/* set rows */
		if(mtr->flags & METEOR_ONLY_FIELDS_MASK) {
			SAA7196_WRITE(mtr, 0x25, ((mtr->rows) & 0xff));
			SAA7196_WRITE(mtr, 0x28,
					((SAA7196_REG(mtr, 0x28) & ~0x03) |
					((mtr->rows >> 8) & 0x03)));
		} else {	/* Interlaced */
			SAA7196_WRITE(mtr, 0x25, ((mtr->rows >> 1) & 0xff));
			SAA7196_WRITE(mtr, 0x28,
					((SAA7196_REG(mtr, 0x28) & ~0x03) |
					((mtr->rows >> 9) & 0x03)));
		}
		/* set signed/unsigned chrominance */
		SAA7196_WRITE(mtr, 0x30, (SAA7196_REG(mtr, 0x30) & ~0x10) |
				((geo->oformat&METEOR_GEO_UNSIGNED)?0:0x10));
		break;
	case METEORGETGEO:
		geo = (struct meteor_geomet *) arg;
		geo->rows = mtr->rows;
		geo->columns = mtr->cols;
		geo->frames = mtr->frames;
		geo->oformat = (mtr->flags & METEOR_OUTPUT_FMT_MASK) |
			       (mtr->flags & METEOR_ONLY_FIELDS_MASK) |
			       (SAA7196_REG(mtr, 0x30) & 0x10 ? 
				0:METEOR_GEO_UNSIGNED);
		break;
	case METEORSCOUNT:	/* (re)set error counts */
		cnt = (struct meteor_counts *) arg;
		mtr->fifo_errors = cnt->fifo_errors;
		mtr->dma_errors = cnt->dma_errors;
		mtr->frames_captured = cnt->frames_captured;
		mtr->even_fields_captured = cnt->even_fields_captured;
		mtr->odd_fields_captured = cnt->odd_fields_captured;
		break;
	case METEORGCOUNT:	/* get error counts */
		cnt = (struct meteor_counts *) arg;
		cnt->fifo_errors = mtr->fifo_errors;
		cnt->dma_errors = mtr->dma_errors;
		cnt->frames_captured = mtr->frames_captured;
		cnt->even_fields_captured = mtr->even_fields_captured;
		cnt->odd_fields_captured = mtr->odd_fields_captured;
		break;
	default:
		printf("meteor%d: ioctl: invalid ioctl request\n", unit);
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

	return i386_btop(vtophys(mtr->bigbuf) + offset);
}


#if !defined(METEOR_FreeBSD_210)	/* XXX */
static meteor_devsw_installed = 0;

static void 	meteor_drvinit(void *unused)
{
	dev_t dev;

	if( ! meteor_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&meteor_cdevsw, NULL);
		meteor_devsw_installed = 1;
    	}
}

SYSINIT(meteordev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,meteor_drvinit,NULL)
#endif

#endif /* NMETEOR > 0 */
