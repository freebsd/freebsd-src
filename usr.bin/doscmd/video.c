/*
 * Copyright (c) 2001 The FreeBSD Project, Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY The FreeBSD Project, Inc. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL The FreeBSD Project, Inc. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/mman.h>
#include <err.h>
#include <paths.h>
#include <unistd.h>

#include "doscmd.h"
#include "AsyncIO.h"
#include "tty.h"
#include "video.h"
#include "vparams.h"

/*
 * Global variables
 */

/* VGA registers */
u_int8_t VGA_CRTC[CRTC_Size];
u_int8_t VGA_ATC[ATC_Size];
u_int8_t VGA_TSC[TSC_Size];
u_int8_t VGA_GDC[GDC_Size];

/* VGA status information */
u_int8_t vga_status[64];

/* Table of supported video modes. */
vmode_t vmodelist[] = {
    {0x00, 0x17, TEXT, 16, 8, 2, 0xb8000, FONT8x16},
    {0x01, 0x17, TEXT, 16, 8, 2, 0xb8000, FONT8x16},
    {0x02, 0x18, TEXT, 16, 8, 2, 0xb8000, FONT8x16},
    {0x03, 0x18, TEXT, 16, 8, 2, 0xb8000, FONT8x16},
    {0x04, 0x04, GRAPHICS, 4, 1, 0, 0xb8000, FONT8x8},
    {0x05, 0x05, GRAPHICS, 4, 1, 0, 0xb8000, FONT8x8},
    {0x06, 0x06, GRAPHICS, 2, 1, 0, 0xb8000, FONT8x8},
    {0x07, 0x19, TEXT, 1, 8, 2, 0xb0000, FONT8x16},
    {0x08, 0x08, NOMODE, 0, 0, 0, 0, 0},
    {0x09, 0x09, NOMODE, 0, 0, 0, 0, 0},
    {0x0a, 0x0a, NOMODE, 0, 0, 0, 0, 0},
    {0x0b, 0x0b, NOMODE, 0, 0, 0, 0, 0},
    {0x0c, 0x0c, NOMODE, 0, 0, 0, 0, 0},
    {0x0d, 0x0d, GRAPHICS, 16, 8, 0, 0xa0000, FONT8x8},
    {0x0e, 0x0e, GRAPHICS, 16, 4, 0, 0xa0000, FONT8x8},
    {0x0f, 0x11, GRAPHICS, 1, 2, 1, 0xa0000, FONT8x14},
    {0x10, 0x12, GRAPHICS, 16, 2, 1, 0xa0000, FONT8x14},
    {0x11, 0x1a, GRAPHICS, 2, 1, 3, 0xa0000, FONT8x16},
    {0x12, 0x1b, GRAPHICS, 16, 1, 3, 0xa0000, FONT8x16},
    /*     {0x13, 0x1c, GRAPHICS, 256, 1, 0, 0xa0000, FONT8x8}, */
};

#define NUMMODES	(sizeof(vmodelist) / sizeof(vmode_t))

/*
 * Local functions
 */
static void	init_vga(void);
static u_int8_t	video_inb(int);
static void	video_outb(int, u_int8_t);

/*
 * Local types and variables
 */

/* Save Table and assorted variables */
struct VideoSaveTable {
    u_short	video_parameter_table[2];
    u_short	parameter_dynamic_save_area[2];		/* Not used */
    u_short	alphanumeric_character_set_override[2];	/* Not used */
    u_short	graphics_character_set_override[2];	/* Not used */
    u_short	secondary_save_table[2];	/* Not used */
    u_short	mbz[4];
};

struct SecondaryVideoSaveTable {
    u_short	length;
    u_short	display_combination_code_table[2];
    u_short	alphanumeric_character_set_override[2];	/* Not used */
    u_short	user_palette_profile_table[2];		/* Not used */
    u_short	mbz[6];
};

struct VideoSaveTable *vsp;
struct SecondaryVideoSaveTable *svsp;

/*
 * Read and write the VGA port
 */

/* Save the selected index register */
static u_int8_t crtc_index, atc_index, tsc_index, gdc_index;
/* Toggle between index and data on port ATC_WritePort */
static u_int8_t set_atc_index = 1;

static u_int8_t
video_inb(int port)
{
    switch(port) {
    case CRTC_DataPortColor:
	return VGA_CRTC[crtc_index];
    case CRTC_IndexPortColor:
	return crtc_index;
    case ATC_ReadPort:
	return VGA_ATC[atc_index];
    case TSC_DataPort:
	return VGA_TSC[tsc_index];
    case TSC_IndexPort:
	return tsc_index;
    case GDC_DataPort:
	return VGA_GDC[gdc_index];
    case GDC_IndexPort:
	return gdc_index;
    case VGA_InputStatus1Port:
	set_atc_index = 1;
	VGA_InputStatus1 = (VGA_InputStatus1 + 1) & 15;
	return VGA_InputStatus1;
    default:
	return 0;
    }
}

static void
video_outb(int port, u_int8_t value)
{
/* XXX */
#define row	(CursRow0)
#define col	(CursCol0)
	
    int cp;
	
    switch (port) {
    case CRTC_IndexPortColor:
	crtc_index = value;
	break;
    case CRTC_DataPortColor:
	VGA_CRTC[crtc_index] = value;
	switch (crtc_index) {
	case CRTC_CurLocHi:	/* Update cursor position in BIOS */
	    cp = row * DpyCols + col;
	    cp &= 0xff;
	    cp |= value << 8;
	    row = cp / DpyCols;
	    col = cp % DpyCols;
	    break;
	case CRTC_CurLocLo:	/* Update cursor position in BIOS */
	    cp = row * DpyCols + col;
	    cp &= 0xff00;
	    cp |= value;
	    row = cp / DpyCols;
	    col = cp % DpyCols;
	    break;
	default:
	    debug(D_VIDEO, "VGA: outb 0x%04x, 0x%02x at index 0x%02x\n",
		  port, value, crtc_index);
	    break;
	}
    case CRTC_IndexPortMono:	/* Not used */
	break;
    case CRTC_DataPortMono:	/* Not used */
	break;
    case ATC_WritePort:
	if (set_atc_index)
	    atc_index = value;
	else {
	    VGA_ATC[atc_index] = value;
	    switch (atc_index) {
	    default:
		debug(D_VIDEO, "VGA: outb 0x%04x, 0x%02x at index 0x%02x\n",
		      port, value, crtc_index);
		break;
	    }
	}
	set_atc_index = 1 - set_atc_index;
	break;
    case TSC_IndexPort:
	tsc_index = value;
	break;
    case TSC_DataPort:
	VGA_TSC[tsc_index] = value;
	switch (tsc_index) {
	default:
	    debug(D_VIDEO, "VGA: outb 0x%04x, 0x%02x at index 0x%02x\n",
		  port, value, crtc_index);
	    break;
	}
	break;
    case GDC_IndexPort:
	gdc_index = value;
	break;
    case GDC_DataPort:
	VGA_GDC[gdc_index] = value;
#if 0
	switch (gdc_index) {
	default:
	    debug(D_VIDEO, "VGA: outb 0x%04x, 0x%02x at index 0x%02x\n",
		  port, value, crtc_index);

	    break;
	}
#endif
	break;
    default:
	debug(D_ALWAYS, "VGA: Unknown port 0x%4x\n", port);
	break;
    }
	
    return;
#undef row
#undef col
}

void
video_init()
{
    /* If we are running under X, get a connection to the X server and create
       an empty window of size (1, 1). It makes a couple of init functions a
       lot easier. */
    if (xmode) {
	init_window();

	/* Set VGA emulator to a sane state */
	init_vga();
	
	/* Initialize mode 3 (text, 80x25, 16 colors) */
	init_mode(3);
    }
    
    /* Define all known I/O port handlers */
    if (!raw_kbd) {
	define_input_port_handler(CRTC_IndexPortColor, video_inb);
	define_input_port_handler(CRTC_DataPortColor, video_inb);
	define_input_port_handler(ATC_ReadPort, video_inb);
	define_input_port_handler(TSC_IndexPort, video_inb);
	define_input_port_handler(TSC_DataPort, video_inb);
	define_input_port_handler(GDC_IndexPort, video_inb);
	define_input_port_handler(GDC_DataPort, video_inb);
	define_input_port_handler(VGA_InputStatus1Port, video_inb);
		
	define_output_port_handler(CRTC_IndexPortColor, video_outb);
	define_output_port_handler(CRTC_DataPortColor, video_outb);
	define_output_port_handler(ATC_WritePort, video_outb);
	define_output_port_handler(TSC_IndexPort, video_outb);
	define_output_port_handler(TSC_DataPort, video_outb);
	define_output_port_handler(GDC_IndexPort, video_outb);
	define_output_port_handler(GDC_DataPort, video_outb);
    }
	
    redirect0 = isatty(0) == 0 || !xmode ;
    redirect1 = isatty(1) == 0 || !xmode ;
    redirect2 = isatty(2) == 0 || !xmode ;

    return;
}

void
video_bios_init()
{
    u_char *p;
    u_long vec;

    if (raw_kbd)
	return;

    /*
     * Put the Video Save Table Pointer @ C000:0000
     * Put the Secondary Video Save Table Pointer @ C000:0020
     * Put the Display Combination Code table @ C000:0040
     * Put the Video Parameter table @ C000:1000 - C000:2FFF
     */
    BIOS_SaveTablePointer = 0xC0000000;

    vsp = (struct VideoSaveTable *)0xC0000L;
    memset(vsp, 0, sizeof(struct VideoSaveTable));
    svsp = (struct SecondaryVideoSaveTable *)0xC0020L;

    vsp->video_parameter_table[0] = 0x1000;
    vsp->video_parameter_table[1] = 0xC000;

    vsp->secondary_save_table[0] = 0x0020;
    vsp->secondary_save_table[1] = 0xC000;

    svsp->display_combination_code_table[0] = 0x0040;
    svsp->display_combination_code_table[1] = 0xC000;

    p = (u_char *)0xC0040;
    *p++ = 2;		/* Only support 2 combinations currently */
    *p++ = 1;		/* Version # */
    *p++ = 8;		/* We won't use more than type 8 */
    *p++ = 0;		/* Reserved */
    *p++ = 0; *p++ = 0;	/* No Display No Display */
    *p++ = 0; *p++ = 8;	/* No Display VGA Color */

    memcpy((void *)0xC1000, videoparams, sizeof(videoparams));
    ivec[0x1d] = 0xC0001000L;	/* Video Parameter Table */

    ivec[0x42] = ivec[0x10];	/* Copy of video interrupt */

    /* Put the current font at C000:3000; the pixels are copied in
       'tty.c:load_font()'. */
    ivec[0x1f] = 0xC0003000L;
    ivec[0x43] = 0xC0003000L;

    BIOSDATA[0x8a] = 1;        /* Index into DCC table */

    vec = insert_softint_trampoline();
    ivec[0x10] = vec;
    register_callback(vec, int10, "int 10");
}

/* Initialize the VGA emulator

   XXX This is not nearly finished right now.
*/
static void
init_vga(void)
{
    int i;

    /* Zero-fill 'dac_rgb' on allocation; the default (EGA) table has only
       64 entries. */
    dac_rgb = (struct dac_colors *)calloc(256, sizeof(struct dac_colors));
    if (dac_rgb == NULL)
	err(1, "Get memory for dac_rgb");

    /* Copy the default DAC table to a working copy we can trash. */
    for (i = 0; i < 64; i++)
	dac_rgb[i] = dac_default64[i]; /* Structure copy */

    /* Point 'palette[]' to the Attribute Controller space. We will only use
       the first 16 slots. */
    palette = VGA_ATC;

    /* Get memory for the video RAM and adjust the plane pointers. */
    vram = calloc(256 * 1024, 1);	/* XXX */
    if (vram == NULL)
	warn("Could not get video memory; graphics modes not available.");

    /* XXX There is probably a more efficient memory layout... */
    vplane0 = vram;
    vplane1 = vram + 0x10000;
    vplane2 = vram + 0x20000;
    vplane3 = vram + 0x30000;

    VGA_InputStatus1 = 0;
}

/*
 * Initialize the requested video mode.
 */

/* Indices into the video parameter table. We will use that array to
   initialize the registers on startup and when the video mode changes. */
#define CRTC_Ofs	10
#define ATC_Ofs		35
#define TSC_Ofs		5
#define GDC_Ofs		55
#define MiscOutput_Ofs	9

void
init_mode(int mode)
{
    vmode_t vmode;
    int idx;			/* Index into vmode */
    int pidx;			/* Index into videoparams */
    
    debug(D_VIDEO, "VGA: Set video mode to 0x%02x\n", mode);

    idx = find_vmode(mode & 0x7f);
    if (idx == -1 || vmodelist[idx].type == NOMODE)
	err(1, "Mode 0x%02x is not supported", mode);
    vmode = vmodelist[idx];
    pidx = vmode.paramindex;
    
    /* Preset VGA registers. */
    memcpy(VGA_CRTC, (u_int8_t *)&videoparams[pidx][CRTC_Ofs],
	   sizeof(VGA_CRTC));
    memcpy(VGA_ATC, (u_int8_t *)&videoparams[pidx][ATC_Ofs],
	   sizeof(VGA_ATC));
    /* Warning: the video parameter table does not contain the Sequencer's
       Reset register. Its default value is 0x03.*/
    VGA_TSC[TSC_Reset] = 0x03;
    memcpy(VGA_TSC + 1, (u_int8_t *)&videoparams[pidx][TSC_Ofs],
	   sizeof(VGA_TSC) - 1);
    memcpy(VGA_GDC, (u_int8_t *)&videoparams[pidx][GDC_Ofs],
	   sizeof(VGA_GDC));
    VGA_MiscOutput = videoparams[pidx][MiscOutput_Ofs];

    /* Paranoia */
    if ((VGA_ATC[ATC_ModeCtrl] & 1) == 1 && vmode.type == TEXT)
	err(1, "Text mode requested, but ATC switched to graphics mode!");
    if ((VGA_ATC[ATC_ModeCtrl] & 1) == 0 && vmode.type == GRAPHICS)
	err(1, "Graphics mode requested, but ATC switched to text mode!");
    
    VideoMode = mode & 0x7f;
    DpyCols = (u_int16_t)videoparams[pidx][0];
    DpyPageSize = *(u_int16_t *)&videoparams[pidx][3];
    ActivePageOfs = 0;
    CursCol0 = 0;
    CursRow0 = 0;
    CursCol1 = 0;
    CursRow1 = 0;
    CursCol2 = 0;
    CursRow2 = 0;
    CursCol3 = 0;
    CursRow3 = 0;
    CursCol4 = 0;
    CursRow4 = 0;
    CursCol5 = 0;
    CursRow5 = 0;
    CursCol6 = 0;
    CursRow6 = 0;
    CursCol7 = 0;
    CursRow7 = 0;
    CursStart = VGA_CRTC[CRTC_CursStart];
    CursEnd = VGA_CRTC[CRTC_CursEnd];
    ActivePage = 0;
    DpyRows = videoparams[pidx][1];
    CharHeight = videoparams[pidx][2];

    CRTCPort = vmode.numcolors > 1 ? CRTC_IndexPortColor : CRTC_IndexPortMono;
    NumColors = vmode.numcolors;
    NumPages = vmode.numpages;
    VertResolution = vmode.vrescode;
    vmem = (u_int16_t *)vmode.vmemaddr;
	
    /* Copy VGA related BIOS variables from 'vga_status'. */
    memcpy(&BIOS_VideoMode, &VideoMode, 33);
    BIOS_DpyRows = DpyRows;
    BIOS_CharHeight = CharHeight;

    /* Load 'pixels[]' from default DAC values. */
    update_pixels();
	
    /* Update font. */
    xfont = vmode.fontname;
    load_font();
    
    /* Resize window if necessary. */
    resize_window();
    
    /* Mmap video memory for the graphics modes. Write access to 0xa0000 -
       0xaffff will generate a T_PAGEFAULT trap in VM86 mode (aside: why not a
       SIGSEGV?), which is handled in 'trap.c:sigbus()'. */
    if (vmode.type == GRAPHICS) {
	vmem = mmap((void *)0xa0000, 64 * 1024, PROT_NONE,
		    MAP_ANON | MAP_FIXED | MAP_SHARED, -1, 0);
	if (vmem == NULL)
	    fatal("Could not mmap() video memory");

	/* Create an XImage to display the graphics screen. */
	get_ximage();
    } else {
	int i;
	
	get_lines();
	if (mode & 0x80)
	    return;
	/* Initialize video memory with black background, white foreground */
	vattr = 0x0700;
	for (i = 0; i < DpyPageSize / 2; ++i)
	    vmem[i] = vattr;
    }

    return;
}

/* Find the requested mode in the 'vmodelist' table. This function returns the
   index into this table; we will also use the index for accessing the
   'videoparams' array. */
int find_vmode(int mode)
{
    unsigned i;

    for (i = 0; i < NUMMODES; i++)
	if (vmodelist[i].modenumber == mode)
	    return i;
	
    return -1;
}

/* Handle access to the graphics memory.

   Simply changing the protection for the memory is not enough, unfortunately.
   It would only work for the 256 color modes, where a memory byte contains
   the color value of one pixel. The 16 color modes (and 4 color modes) make
   use of four bit planes which overlay the first 64K of video memory. The
   bits are distributed into these bit planes according to the GDC state, so
   we will have to emulate the CPU instructions (see 'cpu.c:emu_instr()').

   Handling the 256 color modes will be a bit easier, once we support those at
   all. */
int
vmem_pageflt(struct sigframe *sf)
{
    regcontext_t *REGS = (regcontext_t *)(&sf->sf_uc.uc_mcontext);

    /* The ATC's Mode Control register tells us whether 4 or 8 color bits are
       used */
    if (VGA_ATC[ATC_ModeCtrl] & (1 << 6)) {
	/* 256 colors, allow writes; the protection will be set back to
           PROT_READ at the next display update */
	mprotect(vmem, 64 * 1024, PROT_READ | PROT_WRITE);
	return 0;
    }

    /* There's no need to change the protection in the 16 color modes, we will
       write to 'vram'. Just emulate the next instruction. */
    return emu_instr(REGS);
}

/* We need to keep track of the latches' contents.*/
static u_int8_t latch0, latch1, latch2, latch3;

/* Read a byte from the video memory. 'vga_read()' is called from
   'cpu.c:read_byte()' and will emulate the VGA read modes. */
u_int8_t
vga_read(u_int32_t addr)
{
    u_int32_t dst;
    
    /* 'addr' lies between 0xa0000 and 0xaffff. */
    dst = addr - 0xa0000;

    /* Fill latches. */
    latch0 = vplane0[dst];
    latch1 = vplane1[dst];
    latch2 = vplane2[dst];
    latch3 = vplane3[dst];
    
    /* Select read mode. */
    if ((VGA_GDC[GDC_Mode] & 0x80) == 0)
	/* Read Mode 0; return the byte from the selected bit plane. */
	return vram[dst + (VGA_GDC[GDC_ReadMapSelect] & 3) * 0x10000];

    /* Read Mode 1 */
    debug(D_ALWAYS, "VGA: Read Mode 1 not implemented\n");
    return 0;
}

/* Write a byte to the video memory. 'vga_write()' is called from
   'cpu.c:write_word()' and will emulate the VGA write modes. Not all four
   modes are implemented yet, nor are the addressing modes (odd/even, chain4).
   (NB: I think the latter will have to be done in 'tty_graphics_update()').
   */
void
vga_write(u_int32_t addr, u_int8_t val)
{
    u_int32_t dst;
    u_int8_t c0, c1, c2, c3;
    u_int8_t m0, m1, m2, m3;
    u_int8_t mask;

#if 0
    unsigned i;
    
    debug(D_VIDEO, "VGA: Write 0x%02x to 0x%x\n", val, addr);
    debug(D_VIDEO, "   GDC: ");
    for (i = 0; i < sizeof(VGA_GDC); i++)
	debug(D_VIDEO, "%02x ", VGA_GDC[i]);
    debug(D_VIDEO, "\n");
    debug(D_VIDEO, "   TSC: ");
    for (i = 0; i < sizeof(VGA_TSC); i++)
	debug(D_VIDEO, "%02x ", VGA_TSC[i]);
    debug(D_VIDEO, "\n");
#endif
    
    /* 'addr' lies between 0xa0000 and 0xaffff. */
    dst = addr - 0xa0000;

    c0 = latch0;
    c1 = latch1;
    c2 = latch2;
    c3 = latch3;
    
    /* Select write mode. */
    switch (VGA_GDC[GDC_Mode] & 3) {
    case 0:
	mask = VGA_GDC[GDC_BitMask];

	if (VGA_GDC[GDC_DataRotate] & 7)
	    debug(D_ALWAYS, "VGA: Data Rotate != 0\n");
	
	/* Select function.  */
	switch (VGA_GDC[GDC_DataRotate] & 0x18) {
	case 0x00:		/* replace */
	    m0 = VGA_GDC[GDC_SetReset] & 1 ? mask : 0x00;
	    m1 = VGA_GDC[GDC_SetReset] & 2 ? mask : 0x00;
	    m2 = VGA_GDC[GDC_SetReset] & 4 ? mask : 0x00;
	    m3 = VGA_GDC[GDC_SetReset] & 8 ? mask : 0x00;

	    c0 = VGA_GDC[GDC_EnableSetReset] & 1 ? c0 & ~mask : val & ~mask;
	    c1 = VGA_GDC[GDC_EnableSetReset] & 2 ? c1 & ~mask : val & ~mask;
	    c2 = VGA_GDC[GDC_EnableSetReset] & 4 ? c2 & ~mask : val & ~mask;
	    c3 = VGA_GDC[GDC_EnableSetReset] & 8 ? c3 & ~mask : val & ~mask;
    
	    c0 |= m0;
	    c1 |= m1;
	    c2 |= m2;
	    c3 |= m3;
	    break;
	case 0x08:		/* AND */
	    m0 = VGA_GDC[GDC_SetReset] & 1 ? 0xff : ~mask;
	    m1 = VGA_GDC[GDC_SetReset] & 2 ? 0xff : ~mask;
	    m2 = VGA_GDC[GDC_SetReset] & 4 ? 0xff : ~mask;
	    m3 = VGA_GDC[GDC_SetReset] & 8 ? 0xff : ~mask;

	    c0 = VGA_GDC[GDC_EnableSetReset] & 1 ? c0 & m0 : val & m0;
	    c1 = VGA_GDC[GDC_EnableSetReset] & 2 ? c1 & m1 : val & m1;
	    c2 = VGA_GDC[GDC_EnableSetReset] & 4 ? c2 & m2 : val & m2;
	    c3 = VGA_GDC[GDC_EnableSetReset] & 8 ? c3 & m3 : val & m3;
	    break;
	case 0x10:		/* OR */
	    m0 = VGA_GDC[GDC_SetReset] & 1 ? mask : 0x00;
	    m1 = VGA_GDC[GDC_SetReset] & 2 ? mask : 0x00;
	    m2 = VGA_GDC[GDC_SetReset] & 4 ? mask : 0x00;
	    m3 = VGA_GDC[GDC_SetReset] & 8 ? mask : 0x00;

	    c0 = VGA_GDC[GDC_EnableSetReset] & 1 ? c0 | m0 : val | m0;
	    c1 = VGA_GDC[GDC_EnableSetReset] & 2 ? c1 | m1 : val | m1;
	    c2 = VGA_GDC[GDC_EnableSetReset] & 4 ? c2 | m2 : val | m2;
	    c3 = VGA_GDC[GDC_EnableSetReset] & 8 ? c3 | m3 : val | m3;
	    break;
	case 0x18:		/* XOR */
	    m0 = VGA_GDC[GDC_SetReset] & 1 ? mask : 0x00;
	    m1 = VGA_GDC[GDC_SetReset] & 2 ? mask : 0x00;
	    m2 = VGA_GDC[GDC_SetReset] & 4 ? mask : 0x00;
	    m3 = VGA_GDC[GDC_SetReset] & 8 ? mask : 0x00;

	    c0 = VGA_GDC[GDC_EnableSetReset] & 1 ? c0 ^ m0 : val ^ m0;
	    c1 = VGA_GDC[GDC_EnableSetReset] & 2 ? c1 ^ m1 : val ^ m1;
	    c2 = VGA_GDC[GDC_EnableSetReset] & 4 ? c2 ^ m2 : val ^ m2;
	    c3 = VGA_GDC[GDC_EnableSetReset] & 8 ? c3 ^ m3 : val ^ m3;
	    break;
	}
	break;
    case 1:
	/* Just copy the latches' content to the desired destination
	   address. */
	break;
    case 2:
	mask = VGA_GDC[GDC_BitMask];

	/* select function */
	switch (VGA_GDC[GDC_DataRotate] & 0x18) {
	case 0x00:		/* replace */
	    m0 = (val & 1 ? 0xff : 0x00) & mask;
	    m1 = (val & 2 ? 0xff : 0x00) & mask;
	    m2 = (val & 4 ? 0xff : 0x00) & mask;
	    m3 = (val & 8 ? 0xff : 0x00) & mask;

	    c0 &= ~mask;
	    c1 &= ~mask;
	    c2 &= ~mask;
	    c3 &= ~mask;
    
	    c0 |= m0;
	    c1 |= m1;
	    c2 |= m2;
	    c3 |= m3;
	    break;
	case 0x08:		/* AND */
	    m0 = (val & 1 ? 0xff : 0x00) | ~mask;
	    m1 = (val & 2 ? 0xff : 0x00) | ~mask;
	    m2 = (val & 4 ? 0xff : 0x00) | ~mask;
	    m3 = (val & 8 ? 0xff : 0x00) | ~mask;

	    c0 &= m0;
	    c1 &= m1;
	    c2 &= m2;
	    c3 &= m3;
	    break;
	case 0x10:		/* OR */
	    m0 = (val & 1 ? 0xff : 0x00) & mask;
	    m1 = (val & 2 ? 0xff : 0x00) & mask;
	    m2 = (val & 4 ? 0xff : 0x00) & mask;
	    m3 = (val & 8 ? 0xff : 0x00) & mask;

	    c0 |= m0;
	    c1 |= m1;
	    c2 |= m2;
	    c3 |= m3;
	    break;
	case 0x18:		/* XOR */
	    m0 = (val & 1 ? 0xff : 0x00) & mask;
	    m1 = (val & 2 ? 0xff : 0x00) & mask;
	    m2 = (val & 4 ? 0xff : 0x00) & mask;
	    m3 = (val & 8 ? 0xff : 0x00) & mask;

	    c0 ^= m0;
	    c1 ^= m1;
	    c2 ^= m2;
	    c3 ^= m3;
	    break;
	}
	break;
    case 3:
	/* not yet */
	debug(D_ALWAYS, "VGA: Write Mode 3 not implemented\n");
	break;
    }

    /* Write back changed byte, depending on Map Mask register. */
    if (VGA_TSC[TSC_MapMask] & 1)
	vplane0[dst] = c0;
    if (VGA_TSC[TSC_MapMask] & 2)
	vplane1[dst] = c1;
    if (VGA_TSC[TSC_MapMask] & 4)
	vplane2[dst] = c2;
    if (VGA_TSC[TSC_MapMask] & 8)
	vplane3[dst] = c3;
    
    return;
}
