/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
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
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI video.h,v 2.2 1996/04/08 19:33:12 bostic Exp
 *
 * $FreeBSD$
 */

/*
 * The VGA CRT Controller
 */
extern u_int8_t VGA_CRTC[];

/* CRTC registers
   
   We use the VGA register functions and don't care about the MDA. We also
   leave out the undocumented registers at 0x22, 0x24, 0x3?. */
#define	CRTC_HorzTotal		0x00
#define	CRTC_HorzDispEnd	0x01
#define CRTC_StartHorzBlank	0x02
#define	CRTC_EndHorzBlank	0x03
#define	CRTC_StartHorzRetrace	0x04
#define	CRTC_EndHorzRetrace	0x05
#define	CRTC_VertTotal		0x06
#define	CRTC_Overflow		0x07
#define	CRTC_ResetRowScan	0x08
#define	CRTC_MaxScanLine	0x09
#define	CRTC_CursStart		0x0a
#define	CRTC_CursEnd		0x0b
#define	CRTC_StartAddrHi	0x0c
#define	CRTC_StartAddrLo	0x0d
#define	CRTC_CurLocHi		0x0e
#define	CRTC_CurLocLo		0x0f
#define CRTC_StartVertRetrace	0x10
#define CRTC_EndVertRetrace	0x11
#define CRTC_VertDispEnd	0x12
#define CRTC_Offset		0x13
#define CRTC_UnderlineLoc	0x14
#define CRTC_StartVertBlank	0x15
#define CRTC_EndVertBlank	0x16
#define CRTC_ModeCtrl		0x17
#define CRTC_LineCompare	0x18

#define CRTC_Size		0x19

/* Port addresses for the CRTC

   The registers are read by
   	OUT index_port, reg_nr
	IN  data_port, res

   They are written by
   	OUT index_port, reg_nr
	OUT data_port, value
*/
   
#define	CRTC_IndexPortColor	0x03d4	/* CRTC Address Register (Color) */
#define	CRTC_DataPortColor	0x03d5	/* CRTC Data Register (Color) */
#define	CRTC_IndexPortMono	0x03b4	/* CRTC Address Register (Mono) */
#define	CRTC_DataPortMono	0x03b5	/* CRTC Data Register (Mono) */

/*
 * VGA Attribute Controller
 */
extern u_int8_t VGA_ATC[];

/* ATC registers

   The palette registers are here for completeness. We'll always use a
   separate array 'palette[]' to access them in our code. */
#define ATC_Palette0		0x00
#define ATC_Palette1		0x01
#define ATC_Palette2		0x02
#define ATC_Palette3		0x03
#define ATC_Palette4		0x04
#define ATC_Palette5		0x05
#define ATC_Palette6		0x06
#define ATC_Palette7		0x07
#define ATC_Palette8		0x08
#define ATC_Palette9		0x09
#define ATC_PaletteA		0x0a
#define ATC_PaletteB		0x0b
#define ATC_PaletteC		0x0c
#define ATC_PaletteD		0x0d
#define ATC_PaletteE		0x0e
#define ATC_PaletteF		0x0f
#define ATC_ModeCtrl		0x10
#define ATC_OverscanColor	0x11
#define ATC_ColorPlaneEnable	0x12
#define ATC_HorzPixelPanning	0x13
#define ATC_ColorSelect		0x14

#define ATC_Size		0x15

/* Port addresses for the ATC

   The ATC has a combined index/data port at 0x03c0. To quote from Ralf
   Brown's ports list: ``Every write access to this register will toggle an
   internal index/data selection flipflop, so that consecutive writes to index
   & data is possible through this port. To get a defined start condition,
   each read access to the input status register #1 (3BAh in mono / 3DAh in
   color) resets the flipflop to load index.'' */
#define ATC_WritePort		0x03c0
#define ATC_ReadPort		0x03c1

/*
 * VGA Sequencer Controller
 */
extern u_int8_t VGA_TSC[];

/* TSC registers

   We leave out the undocumented register at 0x07. */
#define TSC_Reset		0x00
#define TSC_ClockingMode	0x01
#define TSC_MapMask		0x02
#define TSC_CharMapSelect	0x03
#define TSC_MemoryMode		0x04

#define TSC_Size		0x05

/* Port addresses for the TSC */
#define TSC_IndexPort		0x03c4
#define TSC_DataPort		0x03c5

/*
 * VGA Graphics Controller
 */
extern u_int8_t VGA_GDC[];

/* GDC registers */
#define GDC_SetReset		0x00
#define GDC_EnableSetReset	0x01
#define GDC_ColorCompare	0x02
#define GDC_DataRotate		0x03
#define GDC_ReadMapSelect	0x04
#define GDC_Mode		0x05
#define GDC_Misc		0x06
#define GDC_ColorDontCare	0x07
#define GDC_BitMask		0x08

#define GDC_Size		0x09

/* Port addresses for the GDC */
#define GDC_IndexPort		0x03ce
#define GDC_DataPort		0x03cf

/*
 * Miscellaneous VGA registers
 */
u_int8_t VGA_InputStatus0;
u_int8_t VGA_InputStatus1;
u_int8_t VGA_MiscOutput;

u_int8_t VGA_DAC_PELData;
u_int8_t VGA_DAC_PELMask;
u_int8_t VGA_DAC_PELReadAddr;
u_int8_t VGA_DAC_PELWriteAddr;
u_int8_t VGA_DAC_State;

/* Port addresses for miscellaneous VGA registers */
#define VGA_InputStatus0Port		0x03c2	/* Read-only */
#define VGA_InputStatus1Port		0x03da	/* Read-only */
#define VGA_MiscOutputPortW		0x03c2	/* Write-only */
#define VGA_MiscOutputPortR		0x03cc	/* Read-only */

/* Port addresses for VGA DAC registers */
#define VGA_DAC_PELDataPort		0x03c9	/* Read/Write */
#define VGA_DAC_PELMaskPort		0x03c6	/* Read/Write */
#define VGA_DAC_PELReadAddrPort		0x03c7	/* Write-only */
#define VGA_DAC_PELWriteAddrPort	0x03c8	/* Read/Write */
#define VGA_DAC_StatePortOut		0x03c7	/* Read-only */

/*
 * Additional variables and type definitions
 */

/* To ease access to the palette registers, 'palette[]' will overlay the
   Attribute Controller space. */
u_int8_t *palette;

/* Entry type for the DAC table. Each value is actually 6 bits wide. */
struct dac_colors {
    u_int8_t red;
    u_int8_t green;
    u_int8_t blue;
};

/* We need a working copy of the default DAC table. This is filled from
   'dac_default{64,256}[]' in 'video.c:init_vga()'. */
struct dac_colors *dac_rgb;

/*
 * Video memory
 *
 * The video memory of a standard VGA card is 256K. For the standard modes,
 * this is divided into four planes of 64K which are accessed according to the
 * GDC state. Mode 0x13 will also fit within 64K. The higher resolution modes
 * (VESA) require a bit more sophistication; we leave that for later
 * implementation.
 */

/* Video RAM */
u_int8_t *vram;

/* Pointers to the four bit planes */
u_int8_t *vplane0;
u_int8_t *vplane1;
u_int8_t *vplane2;
u_int8_t *vplane3;

/* Pointer to the video memory. The base address varies with the video mode.
   'vmem' is used directly only in the text modes; in the graphics modes, all
   writes go to 'vram'. */
u_int16_t *vmem;

/*
 * VGA status information
 *
 * Int 10:1b returns a 64 byte block of status info for the VGA card. This
 * block also contains a couple of BIOS variables, so we will use it for
 * general housekeeping.
 */
extern u_int8_t vga_status[];

/* Access to the VGA status fields. */
#define StaticFuncTbl		*(u_int32_t *)&vga_status[0]
#define VideoMode		*(u_int8_t *)&vga_status[4]
#define DpyCols			*(u_int16_t *)&vga_status[5]
#define DpyPageSize		*(u_int16_t *)&vga_status[7]
#define ActivePageOfs		*(u_int16_t *)&vga_status[9]
#define CursCol0	 	*(u_int8_t *)&vga_status[11]
#define CursRow0		*(u_int8_t *)&vga_status[12]
#define CursCol1		*(u_int8_t *)&vga_status[13]
#define CursRow1		*(u_int8_t *)&vga_status[14]
#define CursCol2		*(u_int8_t *)&vga_status[15]
#define CursRow2		*(u_int8_t *)&vga_status[16]
#define CursCol3		*(u_int8_t *)&vga_status[17]
#define CursRow3		*(u_int8_t *)&vga_status[18]
#define CursCol4		*(u_int8_t *)&vga_status[19]
#define CursRow4		*(u_int8_t *)&vga_status[20]
#define CursCol5		*(u_int8_t *)&vga_status[21]
#define CursRow5		*(u_int8_t *)&vga_status[22]
#define CursCol6		*(u_int8_t *)&vga_status[23]
#define CursRow6		*(u_int8_t *)&vga_status[24]
#define CursCol7		*(u_int8_t *)&vga_status[25]
#define CursRow7		*(u_int8_t *)&vga_status[26]
#define CursStart		*(u_int8_t *)&vga_status[27]
#define CursEnd			*(u_int8_t *)&vga_status[28]
#define ActivePage		*(u_int8_t *)&vga_status[29]
#define CRTCPort		*(u_int16_t *)&vga_status[30]
#define CGA_ModeCtrl		*(u_int8_t *)&vga_status[32]
#define CGA_ColorSelect		*(u_int8_t *)&vga_status[33]
#define DpyRows			*(u_int8_t *)&vga_status[34]
#define CharHeight		*(u_int16_t *)&vga_status[35]
#define ActiveDCC		*(u_int8_t *)&vga_status[37]
#define SecondDCC		*(u_int8_t *)&vga_status[38]
#define NumColors		*(u_int16_t *)&vga_status[39]
#define NumPages		*(u_int8_t *)&vga_status[41]
#define VertResolution		*(u_int8_t *)&vga_status[42]
#define PrimaryCharset		*(u_int8_t *)&vga_status[43]
#define SecondaryCharset	*(u_int8_t *)&vga_status[44]
#define MiscStatus		*(u_int8_t *)&vga_status[45]
/*
#define Reserved1		*(u_int16_t *)&vga_status[46]
#define Reserved2		*(u_int8_t *)&vga_status[48]
*/
#define VMemSize		*(u_int8_t *)&vga_status[49]
#define SavePointerStatus	*(u_int8_t *)&vga_status[50]

/* VGA Static Functionality Table

   This table contains mode-independent VGA status information. It is actually
   defined in 'vparam.h'; the declaration here is just for completeness. */
extern u_int8_t static_functionality_tbl[];

/* Add some names for the VGA related BIOS variables. */
#define BIOS_VideoMode		*(u_int8_t *)&BIOSDATA[0x49]
#define BIOS_DpyCols		*(u_int16_t *)&BIOSDATA[0x4a]
#define BIOS_DpyPageSize	*(u_int16_t *)&BIOSDATA[0x4c]
#define BIOS_ActivePageOfs	*(u_int16_t *)&BIOSDATA[0x4e]
#define BIOS_CursCol0	 	*(u_int8_t *)&BIOSDATA[0x50]
#define BIOS_CursRow0		*(u_int8_t *)&BIOSDATA[0x51]
#define BIOS_CursCol1		*(u_int8_t *)&BIOSDATA[0x52]
#define BIOS_CursRow1		*(u_int8_t *)&BIOSDATA[0x53]
#define BIOS_CursCol2		*(u_int8_t *)&BIOSDATA[0x54]
#define BIOS_CursRow2		*(u_int8_t *)&BIOSDATA[0x55]
#define BIOS_CursCol3		*(u_int8_t *)&BIOSDATA[0x56]
#define BIOS_CursRow3		*(u_int8_t *)&BIOSDATA[0x57]
#define BIOS_CursCol4		*(u_int8_t *)&BIOSDATA[0x58]
#define BIOS_CursRow4		*(u_int8_t *)&BIOSDATA[0x59]
#define BIOS_CursCol5		*(u_int8_t *)&BIOSDATA[0x5a]
#define BIOS_CursRow5		*(u_int8_t *)&BIOSDATA[0x5b]
#define BIOS_CursCol6		*(u_int8_t *)&BIOSDATA[0x5c]
#define BIOS_CursRow6		*(u_int8_t *)&BIOSDATA[0x5d]
#define BIOS_CursCol7		*(u_int8_t *)&BIOSDATA[0x5e]
#define BIOS_CursRow7		*(u_int8_t *)&BIOSDATA[0x5f]
#define BIOS_CursStart		*(u_int8_t *)&BIOSDATA[0x60]
#define BIOS_CursEnd		*(u_int8_t *)&BIOSDATA[0x61]
#define BIOS_ActivePage		*(u_int8_t *)&BIOSDATA[0x62]
#define BIOS_CRTCPort		*(u_int16_t *)&BIOSDATA[0x63]
#define BIOS_CGA_ModeCtrl	*(u_int8_t *)&BIOSDATA[0x65]
#define BIOS_CGA_ColorSelect	*(u_int8_t *)&BIOSDATA[0x66]
#define BIOS_DpyRows		*(u_int8_t *)&BIOSDATA[0x84]
#define BIOS_CharHeight		*(u_int16_t *)&BIOSDATA[0x85]
#define BIOS_SaveTablePointer	*(u_int32_t *)&BIOSDATA[0xa8]

/*
 * Video modes
 *
 * This started as a big 'switch' statement in 'video.c:init_mode()' which
 * soon became too ugly and unmanagable. So, we collect all mode related
 * information in one table and define a couple of helper function to access
 * it. This will also benefit the VESA support, whenever we get to that.
 */
typedef struct {
    int modenumber;		/* Mode number */
    int paramindex;		/* Index into the parameter table */
    int type;			/* Text or Graphics */
    int numcolors;		/* Number of colors */
    int numpages;		/* Number of display pages */
    int vrescode;		/* 0 = 200, 1 = 350, 2 = 400, 3 = 480 */
    u_int32_t vmemaddr;		/* Video memory address */
    const char *fontname;	/* Font name */
} vmode_t;

/* Types. 'NOMODE' is one of the 'forbidden' internal modes. */
#define TEXT		0
#define GRAPHICS	1
#define NOMODE		-1

extern vmode_t vmodelist[];

/* Font names */
#define	FONTVGA		"vga"
#define FONT8x8		"vga8x8"
#define FONT8x14	"vga8x14"
#define FONT8x16	"vga8x16"	/* same as FONTVGA */

/* External functions in 'video.c'. */
void		init_mode(int);
int		find_vmode(int);
u_int8_t	vga_read(u_int32_t);
void		vga_write(u_int32_t, u_int8_t);
void		video_bios_init(void);
void		video_init(void);
int		vmem_pageflt(struct sigframe *);

/* Other external variables, mostly from tty.c. Needs to be cleaned up. */
extern int 	vattr;
void		write_vram(void *);
