/*-
 * Copyright (c) 1998 Kazutaka YOKOTA (yokota@zodiac.mech.utsunomiya-u.ac.jp)
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
 * 3. The name of the author may not be used to endorse or promote 
 *    products derived from this software without specific prior written 
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 * from: i386/isa videoio.c,v 1.1
 */

#include "sc.h"
#include "opt_syscons.h"

#if NSC > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/clock.h>
#include <machine/console.h>
#include <machine/md_var.h>
#include <machine/pc/bios.h>

#include <isa/isareg.h>
#include <isa/videoio.h>

/* this should really be in `rtc.h' */
#define RTC_EQUIPMENT           0x14

/* video adapter state buffer */
struct adp_state {
    int			sig;
#define V_STATE_SIG	0x736f6962
    u_char		regs[V_MODE_PARAM_SIZE];
};
typedef struct adp_state adp_state_t;

/* video adapter information */
#define DCC_MONO	0
#define DCC_CGA40	1
#define DCC_CGA80	2
#define DCC_EGAMONO	3
#define DCC_EGA40	4
#define DCC_EGA80	5

/* 
 * NOTE: `va_window' should have a virtual address, but is initialized
 * with a physical address in the following table, as verify_adapter()
 * will perform address conversion at run-time.
 */
static video_adapter_t adapter_init_value[] = {
    { 0, KD_MONO, 0, 	      MONO_BASE,  0xb0000, 32, 32, 0, 0, 0, 7, 0 },
    { 0, KD_CGA, V_ADP_COLOR, COLOR_BASE, 0xb8000, 32, 32, 0, 0, 0, 3, 0 },
    { 0, KD_CGA, V_ADP_COLOR, COLOR_BASE, 0xb8000, 32, 32, 0, 0, 0, 3, 0 },
    { 0, KD_EGA, 0,	      MONO_BASE,  0xb0000, 32, 32, 0, 0, 0, 7, 0 },
    { 0, KD_EGA, V_ADP_COLOR, COLOR_BASE, 0xb8000, 32, 32, 0, 0, 0, 3, 0 },
    { 0, KD_EGA, V_ADP_COLOR, COLOR_BASE, 0xb8000, 32, 32, 0, 0, 0, 3, 0 },
};

static video_adapter_t adapter[V_MAX_ADAPTERS];
static int adapters = 0;

/* VGA function entries */
static vi_init_t		vid_init;
static vi_adapter_t		vid_adapter;
static vi_get_info_t		vid_get_info;
static vi_query_mode_t		vid_query_mode;
static vi_set_mode_t		vid_set_mode;
static vi_save_font_t		vid_save_font;
static vi_load_font_t		vid_load_font;
static vi_show_font_t		vid_show_font;
static vi_save_palette_t	vid_save_palette;
static vi_load_palette_t	vid_load_palette;
static vi_set_border_t		vid_set_border;
static vi_save_state_t		vid_save_state;
static vi_load_state_t		vid_load_state;
static vi_set_win_org_t		vid_set_origin;
static vi_read_hw_cursor_t	vid_read_hw_cursor;
static vi_set_hw_cursor_t	vid_set_hw_cursor;
static vi_diag_t		vid_diag;

struct vidsw biosvidsw = {
	vid_init,	vid_adapter,	vid_get_info,	vid_query_mode,	
	vid_set_mode,	vid_save_font,	vid_load_font,	vid_show_font,
	vid_save_palette,vid_load_palette,vid_set_border,vid_save_state,
	vid_load_state,	vid_set_origin,	vid_read_hw_cursor, vid_set_hw_cursor,
	vid_diag,
};

/* VGA BIOS standard video modes */
#define EOT		(-1)
#define NA		(-2)

static video_info_t bios_vmode[] = {
    /* CGA */
    { M_B40x25,     V_INFO_COLOR, 40, 25, 8,  8, 2, 1, 0xb8000, 32, 32, 0, 32 },
    { M_C40x25,     V_INFO_COLOR, 40, 25, 8,  8, 4, 1, 0xb8000, 32, 32, 0, 32 },
    { M_B80x25,     V_INFO_COLOR, 80, 25, 8,  8, 2, 1, 0xb8000, 32, 32, 0, 32 },
    { M_C80x25,     V_INFO_COLOR, 80, 25, 8,  8, 4, 1, 0xb8000, 32, 32, 0, 32 },
    /* EGA */
    { M_ENH_B40x25, V_INFO_COLOR, 40, 25, 8, 14, 2, 1, 0xb8000, 32, 32, 0, 32 },
    { M_ENH_C40x25, V_INFO_COLOR, 40, 25, 8, 14, 4, 1, 0xb8000, 32, 32, 0, 32 },
    { M_ENH_B80x25, V_INFO_COLOR, 80, 25, 8, 14, 2, 1, 0xb8000, 32, 32, 0, 32 },
    { M_ENH_C80x25, V_INFO_COLOR, 80, 25, 8, 14, 4, 1, 0xb8000, 32, 32, 0, 32 },
    /* VGA */
    { M_VGA_C40x25, V_INFO_COLOR, 40, 25, 8, 16, 4, 1, 0xb8000, 32, 32, 0, 32 },
    { M_VGA_M80x25, 0,		  80, 25, 8, 16, 2, 1, 0xb0000, 32, 32, 0, 32 },
    { M_VGA_C80x25, V_INFO_COLOR, 80, 25, 8, 16, 4, 1, 0xb8000, 32, 32, 0, 32 },
    /* MDA */
    { M_EGAMONO80x25, 0,          80, 25, 8, 14, 2, 1, 0xb0000, 32, 32, 0, 32 },
    /* EGA */
    { M_ENH_B80x43, V_INFO_COLOR, 80, 43, 8,  8, 2, 1, 0xb8000, 32, 32, 0, 32 },
    { M_ENH_C80x43, V_INFO_COLOR, 80, 43, 8,  8, 4, 1, 0xb8000, 32, 32, 0, 32 },
    /* VGA */
    { M_VGA_M80x30, 0,		  80, 30, 8, 16, 2, 1, 0xb0000, 32, 32, 0, 32 },
    { M_VGA_C80x30, V_INFO_COLOR, 80, 30, 8, 16, 4, 1, 0xb8000, 32, 32, 0, 32 },
    { M_VGA_M80x50, 0,		  80, 50, 8,  8, 2, 1, 0xb0000, 32, 32, 0, 32 },
    { M_VGA_C80x50, V_INFO_COLOR, 80, 50, 8,  8, 4, 1, 0xb8000, 32, 32, 0, 32 },
    { M_VGA_M80x60, 0,		  80, 60, 8,  8, 2, 1, 0xb0000, 32, 32, 0, 32 },
    { M_VGA_C80x60, V_INFO_COLOR, 80, 60, 8,  8, 4, 1, 0xb8000, 32, 32, 0, 32 },
    /* CGA */
    { M_BG320,      V_INFO_COLOR | V_INFO_GRAPHICS,                
      320, 200, 8,  8, 2, 1, 0xb8000, 32, 32, 0, 32 },
    { M_CG320,      V_INFO_COLOR | V_INFO_GRAPHICS, 
      320, 200, 8,  8, 2, 1, 0xb8000, 32, 32, 0, 32 },
    { M_BG640,      V_INFO_COLOR | V_INFO_GRAPHICS,                
      640, 200, 8,  8, 1, 1, 0xb8000, 32, 32, 0, 32 },
    /* EGA */
    { M_CG320_D,    V_INFO_COLOR | V_INFO_GRAPHICS,
      320, 200, 8,  8, 4, 4, 0xa0000, 64, 64, 0, 64 },
    { M_CG640_E,    V_INFO_COLOR | V_INFO_GRAPHICS, 
      640, 200, 8,  8, 4, 4, 0xa0000, 64, 64, 0, 64 },
    { M_EGAMONOAPA, V_INFO_GRAPHICS, 		    
      640, 350, 8, 14, 4, 4, 0xa0000, 64, 64, 0, 64 },
    { M_ENHMONOAPA2,V_INFO_GRAPHICS, 		    
      640, 350, 8, 14, 4, 4, 0xa0000, 64, 64, 0, 64 },
    { M_CG640x350,  V_INFO_COLOR | V_INFO_GRAPHICS, 
      640, 350, 8, 14, 2, 2, 0xa0000, 64, 64, 0, 64 },
    { M_ENH_CG640,  V_INFO_COLOR | V_INFO_GRAPHICS, 
      640, 350, 8, 14, 4, 4, 0xa0000, 64, 64, 0, 64 },
    /* VGA */
    { M_BG640x480,  V_INFO_COLOR | V_INFO_GRAPHICS,                
      640, 480, 8, 16, 4, 4, 0xa0000, 64, 64, 0, 64 },
    { M_CG640x480,  V_INFO_COLOR | V_INFO_GRAPHICS, 
      640, 480, 8, 16, 4, 4, 0xa0000, 64, 64, 0, 64 },
    { M_VGA_CG320,  V_INFO_COLOR | V_INFO_GRAPHICS, 
      320, 200, 8,  8, 8, 1, 0xa0000, 64, 64, 0, 64 },
    { M_VGA_MODEX,  V_INFO_COLOR | V_INFO_GRAPHICS, 
      320, 240, 8,  8, 8, 1, 0xa0000, 64, 64, 0, 64 },

    { EOT },
};

static int		init_done = FALSE;
static u_char		*video_mode_ptr = NULL;		/* EGA/VGA */
static u_char		*video_mode_ptr2 = NULL;	/* CGA/MDA */
static u_char		*mode_map[V_MODE_MAP_SIZE];
static adp_state_t	adpstate;
static adp_state_t	adpstate2;
static int		rows_offset = 1;

/* local macros and functions */
#define BIOS_SADDRTOLADDR(p) ((((p) & 0xffff0000) >> 12) + ((p) & 0x0000ffff))

static void map_mode_table(u_char *map[], u_char *table, int max);
static void clear_mode_map(int ad, u_char *map[], int max, int color);
static int map_mode_num(int mode);
static int map_bios_mode_num(int type, int color, int bios_mode);
static u_char *get_mode_param(int mode);
static void fill_adapter_param(int code, video_adapter_t *adp);
static int verify_adapter(video_adapter_t *adp);
#define COMP_IDENTICAL	0
#define COMP_SIMILAR	1
#define COMP_DIFFERENT	2
static int comp_adpregs(u_char *buf1, u_char *buf2);

#define PARAM_BUFSIZE	6
static void set_font_mode(video_adapter_t *adp, u_char *buf);
static void set_normal_mode(video_adapter_t *adp, u_char *buf);

static char *adapter_name(int type);
static void dump_adp_info(int ad, video_adapter_t *adp, int level);
static void dump_mode_info(int ad, video_info_t *info, int level);
static void dump_buffer(u_char *buf, size_t len);

extern void generic_bcopy(const void *, void *, size_t);

#define	ISMAPPED(pa, width) \
	(((pa) <= (u_long)0x1000 - (width)) 		\
	 || ((pa) >= ISA_HOLE_START && (pa) <= 0x100000 - (width)))

#define	prologue(ad, flag, err)	\
	if (!init_done					\
	    || ((ad) < 0) || ((ad) >= adapters)		\
	    || !(adapter[(ad)].va_flags & (flag)))	\
	    return (err)

/* construct the mode parameter map */
static void
map_mode_table(u_char *map[], u_char *table, int max)
{
    int i;

    for(i = 0; i < max; ++i)
	map[i] = table + i*V_MODE_PARAM_SIZE;
    for(; i < V_MODE_MAP_SIZE; ++i)
	map[i] = NULL;
}

static void
clear_mode_map(int ad, u_char *map[], int max, int color)
{
    video_info_t info;
    int i;

    /*
     * NOTE: we don't touch `bios_vmode[]' because it is shared
     * by all adapters.
     */
    for(i = 0; i < max; ++i) {
	if (vid_get_info(ad, i, &info))
	    continue;
	if ((info.vi_flags & V_INFO_COLOR) != color)
	    map[i] = NULL;
    }
}

/* the non-standard video mode is based on a standard mode... */
static int
map_mode_num(int mode)
{
    static struct {
        int from;
        int to;
    } mode_map[] = {
        { M_ENH_B80x43, M_ENH_B80x25 },
        { M_ENH_C80x43, M_ENH_C80x25 },
        { M_VGA_M80x30, M_VGA_M80x25 },
        { M_VGA_C80x30, M_VGA_C80x25 },
        { M_VGA_M80x50, M_VGA_M80x25 },
        { M_VGA_C80x50, M_VGA_C80x25 },
        { M_VGA_M80x60, M_VGA_M80x25 },
        { M_VGA_C80x60, M_VGA_C80x25 },
        { M_VGA_MODEX,  M_VGA_CG320 },
    };
    int i;

    for (i = 0; i < sizeof(mode_map)/sizeof(mode_map[0]); ++i) {
        if (mode_map[i].from == mode)
            return mode_map[i].to;
    }
    return mode;
}

/* turn the BIOS video number into our video mode number */
static int
map_bios_mode_num(int type, int color, int bios_mode)
{
    static int cga_modes[7] = {
	M_B40x25, M_C40x25,		/* 0, 1 */
	M_B80x25, M_C80x25,		/* 2, 3 */
	M_BG320, M_CG320,
	M_BG640,
    };
    static int ega_modes[17] = {
	M_ENH_B40x25, M_ENH_C40x25,	/* 0, 1 */
	M_ENH_B80x25, M_ENH_C80x25,	/* 2, 3 */
	M_BG320, M_CG320,
	M_BG640,
	M_EGAMONO80x25,			/* 7 */
	8, 9, 10, 11, 12,
	M_CG320_D,
	M_CG640_E,
	M_ENHMONOAPA2,			/* XXX: video momery > 64K */
	M_ENH_CG640,			/* XXX: video momery > 64K */
    };
    static int vga_modes[20] = {
	M_VGA_C40x25, M_VGA_C40x25,	/* 0, 1 */
	M_VGA_C80x25, M_VGA_C80x25,	/* 2, 3 */
	M_BG320, M_CG320,
	M_BG640,
	M_VGA_M80x25,			/* 7 */
	8, 9, 10, 11, 12,
	M_CG320_D,
	M_CG640_E,
	M_ENHMONOAPA2,
	M_ENH_CG640,
	M_BG640x480, M_CG640x480, 
	M_VGA_CG320,
    };

    switch (type) {

    case KD_VGA:
	if (bios_mode < sizeof(vga_modes)/sizeof(vga_modes[0]))
	    return vga_modes[bios_mode];
	else if (color)
	    return M_VGA_C80x25;
	else
	    return M_VGA_M80x25;
	break;

    case KD_EGA:
	if (bios_mode < sizeof(ega_modes)/sizeof(ega_modes[0]))
	    return ega_modes[bios_mode];
	else if (color)
	    return M_ENH_C80x25;
	else
	    return M_EGAMONO80x25;
	break;

    case KD_CGA:
	if (bios_mode < sizeof(cga_modes)/sizeof(cga_modes[0]))
	    return cga_modes[bios_mode];
	else
	    return M_C80x25;
	break;

    case KD_MONO:
    case KD_HERCULES:
	return M_EGAMONO80x25;		/* XXX: this name is confusing */

    default:
	break;
    }
    return -1;
}

/* look up a parameter table entry */
static u_char 
*get_mode_param(int mode)
{
    if (mode >= V_MODE_MAP_SIZE)
	mode = map_mode_num(mode);
    if (mode < V_MODE_MAP_SIZE)
	return mode_map[mode];
    else
	return NULL;
}

static void
fill_adapter_param(int code, video_adapter_t *adp)
{
    static struct {
	int primary;
	int secondary;
    } dcc[] = {
	{ DCC_MONO, 			DCC_EGA40 /* CGA monitor */ },
	{ DCC_MONO, 			DCC_EGA80 /* CGA monitor */ },
	{ DCC_MONO, 			DCC_EGA80 /* CGA emulation */ },	
	{ DCC_MONO, 			DCC_EGA80 },
	{ DCC_CGA40, 			DCC_EGAMONO },
	{ DCC_CGA80, 			DCC_EGAMONO },
	{ DCC_EGA40 /* CGA monitor */, 	DCC_MONO},
	{ DCC_EGA80 /* CGA monitor */, 	DCC_MONO},
	{ DCC_EGA80 /* CGA emulation */,DCC_MONO },	
	{ DCC_EGA80, 			DCC_MONO },
	{ DCC_EGAMONO, 			DCC_CGA40 },
	{ DCC_EGAMONO, 			DCC_CGA40 },
    };

    if ((code < 0) || (code >= sizeof(dcc)/sizeof(dcc[0]))) {
	adp[V_ADP_PRIMARY] = adapter_init_value[DCC_MONO];
	adp[V_ADP_SECONDARY] = adapter_init_value[DCC_CGA80];
    } else {
	adp[V_ADP_PRIMARY] = adapter_init_value[dcc[code].primary];
	adp[V_ADP_SECONDARY] = adapter_init_value[dcc[code].secondary];
    }
}

static int
verify_adapter(video_adapter_t *adp)
{
    u_int32_t buf;
    u_short v;
    u_int32_t p;

    buf = BIOS_PADDRTOVADDR(adp->va_window);
    v = readw(buf);
    writew(buf, 0xA55A);
    if (readw(buf) != 0xA55A)
	return 1;
    writew(buf, v);

    switch (adp->va_type) {

    case KD_EGA:
	outb(adp->va_crtc_addr, 7);
	if (inb(adp->va_crtc_addr) == 7) {
	    adp->va_type = KD_VGA;
	    adp->va_flags |= V_ADP_STATESAVE | V_ADP_PALETTE;
	}
	adp->va_flags |= V_ADP_STATELOAD | V_ADP_FONT | V_ADP_BORDER;
	/* the color adapter may be in the 40x25 mode... XXX */

#ifdef __i386__
	/* get the BIOS video mode pointer */
	p = *(u_int32_t *)BIOS_PADDRTOVADDR(0x4a8);
	p = BIOS_SADDRTOLADDR(p);
	if (ISMAPPED(p, sizeof(u_int32_t))) {
	    p = *(u_int32_t *)BIOS_PADDRTOVADDR(p);
	    p = BIOS_SADDRTOLADDR(p);
	    if (ISMAPPED(p, V_MODE_PARAM_SIZE))
		video_mode_ptr = (u_char *)BIOS_PADDRTOVADDR(p);
	}
#endif
	break;

    case KD_CGA:
	adp->va_flags |= V_ADP_COLOR | V_ADP_BORDER;
	/* may be in the 40x25 mode... XXX */
	break;

    case KD_MONO:
	break;
    }

    return 0;
}

/* compare two parameter table entries */
static int 
comp_adpregs(u_char *buf1, u_char *buf2)
{
    static struct {
        u_char mask;
    } params[V_MODE_PARAM_SIZE] = {
	0xff, 0x00, 0xff, 		/* COLS, ROWS, POINTS */
	0x00, 0x00, 			/* page length */
	0xfe, 0xff, 0xff, 0xff,		/* sequencer registers */
	0xf3,				/* misc register */
	0xff, 0xff, 0xff, 0x7f, 0xff,	/* CRTC */
	0xff, 0xff, 0xff, 0x7f, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xff, 0x7f, 0xff, 0xff,
	0x7f, 0xff, 0xff, 0xef, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff,	/* attribute controller registers */
	0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xf0,
	0xff, 0xff, 0xff, 0xff, 0xff,	/* GDC register */
	0xff, 0xff, 0xff, 0xff, 
    }; 
    int identical = TRUE;
    int i;

    if ((buf1 == NULL) || (buf2 == NULL))
	return COMP_DIFFERENT;

    for (i = 0; i < sizeof(params)/sizeof(params[0]); ++i) {
	if (params[i].mask == 0)	/* don't care */
	    continue;
	if ((buf1[i] & params[i].mask) != (buf2[i] & params[i].mask))
	    return COMP_DIFFERENT;
	if (buf1[i] != buf2[i])
	    identical = FALSE;
    }
    return (identical) ? COMP_IDENTICAL : COMP_SIMILAR;
}

/* exported functions */

/* all adapters */
static int
vid_init(void)
{
    video_adapter_t *adp;
    video_info_t info;
    u_char *mp;
    int i;

    /* do this test only once */
    if (init_done)
	return adapters;
    init_done = TRUE;

    /* 
     * Locate display adapters. 
     * The AT architecture supports upto two adapters. `syscons' allows
     * the following combinations of adapters: 
     *     1) MDA + CGA
     *     2) MDA + EGA/VGA color 
     *     3) CGA + EGA/VGA mono
     * Note that `syscons' doesn't bother with MCGA as it is only
     * avaiable for low end PS/2 models which has 80286 or earlier CPUs,
     * thus, they are not running FreeBSD!
     * When there are two adapaters in the system, one becomes `primary'
     * and the other `secondary'. The EGA adapter has a set of DIP 
     * switches on board for this information and the EGA BIOS copies 
     * it in the BIOS data area BIOSDATA_VIDEOSWITCH (40:88). 
     * The VGA BIOS has more sophisticated mechanism and has this 
     * information in BIOSDATA_DCCINDEX (40:8a), but it also maintains 
     * compatibility with the EGA BIOS by updating BIOSDATA_VIDEOSWITCH.
     */

    /* check rtc and BIOS date area */
    /* 
     * XXX: don't use BIOSDATA_EQUIPMENT, it is not a dead copy
     * of RTC_EQUIPMENT. The bit 4 and 5 of the ETC_EQUIPMENT are
     * zeros for EGA and VGA. However, the EGA/VGA BIOS will set 
     * these bits in BIOSDATA_EQUIPMENT according to the monitor
     * type detected.
     */
#ifdef __i386__
    switch ((rtcin(RTC_EQUIPMENT) >> 4) & 3) {	/* bit 4 and 5 */
    case 0:
	/* EGA/VGA */
	fill_adapter_param(readb(BIOS_PADDRTOVADDR(0x488)) & 0x0f, 
			   adapter);
	break;
    case 1:
	/* CGA 40x25 */
	/* FIXME: switch to the 80x25 mode? XXX */
	adapter[V_ADP_PRIMARY] = adapter_init_value[DCC_CGA40];
	adapter[V_ADP_SECONDARY] = adapter_init_value[DCC_MONO];
	break;
    case 2:
	/* CGA 80x25 */
	adapter[V_ADP_PRIMARY] = adapter_init_value[DCC_CGA80];
	adapter[V_ADP_SECONDARY] = adapter_init_value[DCC_MONO];
	break;
    case 3:
	/* MDA */
	adapter[V_ADP_PRIMARY] = adapter_init_value[DCC_MONO];
	adapter[V_ADP_SECONDARY] = adapter_init_value[DCC_CGA80];
	break;
    }
#else
    /* EGA/VGA */
    fill_adapter_param(-1, adapter);
#endif

    adapters = 0;
    if (verify_adapter(&adapter[V_ADP_SECONDARY]) == 0) {
	++adapters;
	adapter[V_ADP_SECONDARY].va_mode = 
	    adapter[V_ADP_SECONDARY].va_initial_mode =
	    map_bios_mode_num(adapter[V_ADP_SECONDARY].va_type, 
			      adapter[V_ADP_SECONDARY].va_flags & V_ADP_COLOR,
			      adapter[V_ADP_SECONDARY].va_initial_bios_mode);
    } else {
	adapter[V_ADP_SECONDARY].va_type = -1;
    }
    if (verify_adapter(&adapter[V_ADP_PRIMARY]) == 0) {
	++adapters;
#ifdef __i386__
	adapter[V_ADP_PRIMARY].va_initial_bios_mode = 
	    *(u_int8_t *)BIOS_PADDRTOVADDR(0x449);
#else
	adapter[V_ADP_PRIMARY].va_initial_bios_mode = 3;
#endif
	adapter[V_ADP_PRIMARY].va_mode = 
	    adapter[V_ADP_PRIMARY].va_initial_mode =
	    map_bios_mode_num(adapter[V_ADP_PRIMARY].va_type, 
			      adapter[V_ADP_PRIMARY].va_flags & V_ADP_COLOR,
			      adapter[V_ADP_PRIMARY].va_initial_bios_mode);
    } else {
	adapter[V_ADP_PRIMARY] = adapter[V_ADP_SECONDARY];
	adapter[V_ADP_SECONDARY].va_type = -1;
    }
    if (adapters == 0)
	return adapters;
    adapter[V_ADP_PRIMARY].va_index = V_ADP_PRIMARY;
    adapter[V_ADP_SECONDARY].va_index = V_ADP_SECONDARY;

#if 0
    /*
     * We cannot have two video adapter of the same type; there must be
     * only one of color or mono adapter, or one each of them.
     */
    if (adapters > 1) {
	if (!((adapter[0].va_flags ^ adapter[1].va_flags) & V_ADP_COLOR))
	    /* we have two mono or color adapters!! */
	    return (adapters = 0);
    }
#endif

    /*
     * Ensure a zero start address.  This is mainly to recover after
     * switching from pcvt using userconfig().  The registers are w/o
     * for old hardware so it's too hard to relocate the active screen
     * memory.
     * This must be done before vid_save_state() for VGA.
     */
    outb(adapter[V_ADP_PRIMARY].va_crtc_addr, 12);
    outb(adapter[V_ADP_PRIMARY].va_crtc_addr + 1, 0);
    outb(adapter[V_ADP_PRIMARY].va_crtc_addr, 13);
    outb(adapter[V_ADP_PRIMARY].va_crtc_addr + 1, 0);

    /* the video mode parameter table in EGA/VGA BIOS */
    /* NOTE: there can be only one EGA/VGA, wheather color or mono,
     * recognized by the video BIOS.
     */
    if ((adapter[V_ADP_PRIMARY].va_type == KD_EGA) ||
	(adapter[V_ADP_PRIMARY].va_type == KD_VGA)) {
	adp = &adapter[V_ADP_PRIMARY];
    } else if ((adapter[V_ADP_SECONDARY].va_type == KD_EGA) ||
	       (adapter[V_ADP_SECONDARY].va_type == KD_VGA)) {
	adp = &adapter[V_ADP_SECONDARY];
    } else {
	adp = NULL;
    }
    bzero(mode_map, sizeof(mode_map));
    if (adp != NULL) {
	if (adp->va_type == KD_VGA) {
	    vid_save_state(adp - adapter, &adpstate, sizeof(adpstate));
	    if (video_mode_ptr == NULL) {
		mode_map[map_mode_num(adp->va_initial_mode)] = adpstate.regs;
		rows_offset = 1;
	    } else {
		/* discard the table if we are not familiar with it... */
		map_mode_table(mode_map, video_mode_ptr, M_VGA_CG320 + 1);
		mp = get_mode_param(adp->va_initial_mode);
		if (mp != NULL)
		    bcopy(mp, adpstate2.regs, sizeof(adpstate2.regs));
		switch (comp_adpregs(adpstate.regs, mp)) {
		case COMP_IDENTICAL:
		    /*
		     * OK, this parameter table looks reasonably familiar
		     * to us...
		     */
		    /* 
		     * This is a kludge for Toshiba DynaBook SS433 
		     * whose BIOS video mode table entry has the actual # 
		     * of rows at the offset 1; BIOSes from other 
		     * manufacturers store the # of rows - 1 there. XXX
		     */
		    rows_offset = adpstate.regs[1] + 1 - mp[1];
		    break;

		case COMP_SIMILAR:
		    /*
		     * Not exactly the same, but similar enough to be
		     * trusted. However, use the saved register values
		     * for the initial mode and other modes which are
		     * based on the initial mode.
		     */
		    mode_map[map_mode_num(adp->va_initial_mode)] = 
			adpstate.regs;
		    rows_offset = adpstate.regs[1] + 1 - mp[1];
		    adpstate.regs[1] -= rows_offset - 1;
		    break;

		case COMP_DIFFERENT:
		default:
		    /*
		     * Don't use the paramter table in BIOS. It doesn't
		     * look familiar to us. Video mode switching is allowed
		     * only if the new mode is the same as or based on
		     * the initial mode. 
		     */
		    video_mode_ptr = NULL;
		    bzero(mode_map, sizeof(mode_map));
		    mode_map[map_mode_num(adp->va_initial_mode)] = 
			adpstate.regs;
		    rows_offset = 1;
		    break;
		}
	    }
	    adp->va_flags |= V_ADP_MODECHANGE;
	} else if (adp->va_type == KD_EGA) {
	    if (video_mode_ptr == NULL) {
		adp->va_flags &= ~V_ADP_FONT;
		rows_offset = 1;
	    } else {
		map_mode_table(mode_map, video_mode_ptr, M_ENH_C80x25 + 1);
		/* XXX how can one validate the EGA table... */
		mp = get_mode_param(adp->va_initial_mode);
		if (mp != NULL) {
		    adp->va_flags |= V_ADP_MODECHANGE;
		    rows_offset = 1;
		} else {
		    /*
		     * This is serious. We will not be able to switch video
		     * modes at all...
		     */
		    adp->va_flags &= ~V_ADP_FONT;
		    video_mode_ptr = NULL;
		    bzero(mode_map, sizeof(mode_map));
		    rows_offset = 1;
                }
	    }
	}
    }

    /* remove conflicting modes if we have more than one adapter */
    if (adapters > 1) {
	for (i = 0; i < adapters; ++i) {
	    if (!(adapter[i].va_flags & V_ADP_MODECHANGE))
		continue;
	    clear_mode_map(i, mode_map, M_VGA_CG320 + 1,
			   (adapter[i].va_flags & V_ADP_COLOR) ? 
			       V_INFO_COLOR : 0);
	}
    }

    /* buffer address */
    vid_get_info(V_ADP_PRIMARY, 
		 adapter[V_ADP_PRIMARY].va_initial_mode, &info);
    adapter[V_ADP_PRIMARY].va_window = BIOS_PADDRTOVADDR(info.vi_window);
    adapter[V_ADP_PRIMARY].va_window_size = info.vi_window_size;
    adapter[V_ADP_PRIMARY].va_window_gran = info.vi_window_gran;
    adapter[V_ADP_PRIMARY].va_buffer = BIOS_PADDRTOVADDR(info.vi_buffer);
    adapter[V_ADP_PRIMARY].va_buffer_size = info.vi_buffer_size;
    if (adapters > 1) {
	vid_get_info(V_ADP_SECONDARY, 
		     adapter[V_ADP_SECONDARY].va_initial_mode, &info);
	adapter[V_ADP_SECONDARY].va_window = BIOS_PADDRTOVADDR(info.vi_window);
	adapter[V_ADP_SECONDARY].va_window_size = info.vi_window_size;
	adapter[V_ADP_SECONDARY].va_window_gran = info.vi_window_gran;
	adapter[V_ADP_SECONDARY].va_buffer = BIOS_PADDRTOVADDR(info.vi_buffer);
	adapter[V_ADP_SECONDARY].va_buffer_size = info.vi_buffer_size;
    }

    /*
     * XXX: we should verify the following values for the primary adapter...
     * crtc I/O port address: *(u_int16_t *)BIOS_PADDRTOVADDR(0x463);
     * color/mono display: (*(u_int8_t *)BIOS_PADDRTOVADDR(0x487) & 0x02) 
     *                     ? 0 : V_ADP_COLOR;
     * columns: *(u_int8_t *)BIOS_PADDRTOVADDR(0x44a);
     * rows: *(u_int8_t *)BIOS_PADDRTOVADDR(0x484);
     * font size: *(u_int8_t *)BIOS_PADDRTOVADDR(0x485);
     * buffer size: *(u_int16_t *)BIOS_PADDRTOVADDR(0x44c);
     */

    return adapters;
}

/* all adapters */
static video_adapter_t
*vid_adapter(int ad)
{
    if (!init_done)
	return NULL;
    if ((ad < 0) || (ad >= adapters))
	return NULL;
    return &adapter[ad];
}

/* all adapters */
static int
vid_get_info(int ad, int mode, video_info_t *info)
{
    int i;

    if (!init_done)
	return 1;
    if ((ad < 0) || (ad >= adapters))
	return 1;

    if (adapter[ad].va_flags & V_ADP_MODECHANGE) {
	/*
	 * If the parameter table entry for this mode is not found, 
	 * the mode is not supported...
	 */
	if (get_mode_param(mode) == NULL)
	    return 1;
    } else {
	/* 
	 * Even if we don't support video mode switching on this adapter,
	 * the information on the initial (thus current) video mode 
	 * should be made available.
	 */
	if (mode != adapter[ad].va_initial_mode)
	    return 1;
    }

    for (i = 0; bios_vmode[i].vi_mode != EOT; ++i) {
	if (bios_vmode[i].vi_mode == NA)
	    continue;
	if (mode == bios_vmode[i].vi_mode) {
	    *info = bios_vmode[i];
	    return 0;
	}
    }
    return 1;
}

/* all adapters */
static int
vid_query_mode(int ad, video_info_t *info)
{
    video_info_t buf;
    int i;

    if (!init_done)
	return -1;
    if ((ad < 0) || (ad >= adapters))
	return -1;

    for (i = 0; bios_vmode[i].vi_mode != EOT; ++i) {
	if (bios_vmode[i].vi_mode == NA)
	    continue;

	if ((info->vi_width != 0)
	    && (info->vi_width != bios_vmode[i].vi_width))
		continue;
	if ((info->vi_height != 0)
	    && (info->vi_height != bios_vmode[i].vi_height))
		continue;
	if ((info->vi_cwidth != 0)
	    && (info->vi_cwidth != bios_vmode[i].vi_cwidth))
		continue;
	if ((info->vi_cheight != 0)
	    && (info->vi_cheight != bios_vmode[i].vi_cheight))
		continue;
	if ((info->vi_depth != 0)
	    && (info->vi_depth != bios_vmode[i].vi_depth))
		continue;
	if ((info->vi_planes != 0)
	    && (info->vi_planes != bios_vmode[i].vi_planes))
		continue;
	/* XXX: should check pixel format, memory model */
	if ((info->vi_flags != 0)
	    && (info->vi_flags != bios_vmode[i].vi_flags))
		continue;

	/* verify if this mode is supported on this adapter */
	if (vid_get_info(ad, bios_vmode[i].vi_mode, &buf))
		continue;
	return bios_vmode[i].vi_mode;
    }
    return -1;
}

/* EGA/VGA */
static int
vid_set_mode(int ad, int mode)
{
    video_info_t info;
    adp_state_t params;

    prologue(ad, V_ADP_MODECHANGE, 1);

    if (vid_get_info(ad, mode, &info))
	return 1;
    params.sig = V_STATE_SIG;
    bcopy(get_mode_param(mode), params.regs, sizeof(params.regs));

    switch (mode) {
    case M_VGA_C80x60: case M_VGA_M80x60:
	params.regs[2]  = 0x08;
	params.regs[19] = 0x47;
	goto special_480l;

    case M_VGA_C80x30: case M_VGA_M80x30:
	params.regs[19] = 0x4f;
special_480l:
	params.regs[9] |= 0xc0;
	params.regs[16] = 0x08;
	params.regs[17] = 0x3e;
	params.regs[26] = 0xea;
	params.regs[28] = 0xdf;
	params.regs[31] = 0xe7;
	params.regs[32] = 0x04;
	goto setup_mode;

    case M_ENH_C80x43: case M_ENH_B80x43:
	params.regs[28] = 87;
	goto special_80x50;

    case M_VGA_C80x50: case M_VGA_M80x50:
special_80x50:
	params.regs[2] = 8;
	params.regs[19] = 7;
	goto setup_mode;

    case M_VGA_C40x25: case M_VGA_C80x25:
    case M_VGA_M80x25:
    case M_B40x25:     case M_C40x25:
    case M_B80x25:     case M_C80x25:
    case M_ENH_B40x25: case M_ENH_C40x25:
    case M_ENH_B80x25: case M_ENH_C80x25:
    case M_EGAMONO80x25:

setup_mode:
	vid_load_state(ad, &params);
	break;

    case M_VGA_MODEX:
	/* "unchain" the VGA mode */
	params.regs[5-1+0x04] &= 0xf7;
	params.regs[5-1+0x04] |= 0x04;
	/* turn off doubleword mode */
	params.regs[10+0x14] &= 0xbf;
	/* turn off word adressing */
	params.regs[10+0x17] |= 0x40;
	/* set logical screen width */
	params.regs[10+0x13] = 80;
	/* set 240 lines */
	params.regs[10+0x11] = 0x2c;
	params.regs[10+0x06] = 0x0d;
	params.regs[10+0x07] = 0x3e;
	params.regs[10+0x10] = 0xea;
	params.regs[10+0x11] = 0xac;
	params.regs[10+0x12] = 0xdf;
	params.regs[10+0x15] = 0xe7;
	params.regs[10+0x16] = 0x06;
	/* set vertical sync polarity to reflect aspect ratio */
	params.regs[9] = 0xe3;
	goto setup_grmode;

    case M_BG320:     case M_CG320:     case M_BG640:
    case M_CG320_D:   case M_CG640_E:
    case M_CG640x350: case M_ENH_CG640:
    case M_BG640x480: case M_CG640x480: case M_VGA_CG320:

setup_grmode:
	vid_load_state(ad, &params);
	break;

    default:
	return 1;
    }

    adapter[ad].va_mode = mode;
    adapter[ad].va_flags &= ~V_ADP_COLOR;
    adapter[ad].va_flags |= 
	(info.vi_flags & V_INFO_COLOR) ? V_ADP_COLOR : 0;
    adapter[ad].va_crtc_addr =
	(adapter[ad].va_flags & V_ADP_COLOR) ? COLOR_BASE : MONO_BASE;
    adapter[ad].va_window = BIOS_PADDRTOVADDR(info.vi_window);
    adapter[ad].va_window_size = info.vi_window_size;
    adapter[ad].va_window_gran = info.vi_window_gran;
    if (info.vi_buffer_size == 0) {
    	adapter[ad].va_buffer = 0;
    	adapter[ad].va_buffer_size = 0;
    } else {
    	adapter[ad].va_buffer = BIOS_PADDRTOVADDR(info.vi_buffer);
    	adapter[ad].va_buffer_size = info.vi_buffer_size;
    }

    return 0;
}

static void
set_font_mode(video_adapter_t *adp, u_char *buf)
{
    u_char *mp;
    int s;

    s = splhigh();

    /* save register values */
    if (adp->va_type == KD_VGA) {
	outb(TSIDX, 0x02); buf[0] = inb(TSREG);
	outb(TSIDX, 0x04); buf[1] = inb(TSREG);
	outb(GDCIDX, 0x04); buf[2] = inb(GDCREG);
	outb(GDCIDX, 0x05); buf[3] = inb(GDCREG);
	outb(GDCIDX, 0x06); buf[4] = inb(GDCREG);
	inb(adp->va_crtc_addr + 6);
	outb(ATC, 0x10); buf[5] = inb(ATC + 1);
    } else /* if (adp->va_type == KD_EGA) */ {
	/* 
	 * EGA cannot be read; copy parameters from the mode parameter 
	 * table. 
	 */
	mp = get_mode_param(adp->va_mode);
	buf[0] = mp[5 + 0x02 - 1];
	buf[1] = mp[5 + 0x04 - 1];
	buf[2] = mp[55 + 0x04];
	buf[3] = mp[55 + 0x05];
	buf[4] = mp[55 + 0x06];
	buf[5] = mp[35 + 0x10];
    }

    /* setup vga for loading fonts */
    inb(adp->va_crtc_addr + 6);			/* reset flip-flop */
    outb(ATC, 0x10); outb(ATC, buf[5] & ~0x01);
    inb(adp->va_crtc_addr + 6);			/* reset flip-flop */
    outb(ATC, 0x20);				/* enable palette */

#if SLOW_VGA
#ifndef SC_BAD_FLICKER
    outb(TSIDX, 0x00); outb(TSREG, 0x01);
#endif
    outb(TSIDX, 0x02); outb(TSREG, 0x04);
    outb(TSIDX, 0x04); outb(TSREG, 0x07);
#ifndef SC_BAD_FLICKER
    outb(TSIDX, 0x00); outb(TSREG, 0x03);
#endif
    outb(GDCIDX, 0x04); outb(GDCREG, 0x02);
    outb(GDCIDX, 0x05); outb(GDCREG, 0x00);
    outb(GDCIDX, 0x06); outb(GDCREG, 0x04);
#else
#ifndef SC_BAD_FLICKER
    outw(TSIDX, 0x0100);
#endif
    outw(TSIDX, 0x0402);
    outw(TSIDX, 0x0704);
#ifndef SC_BAD_FLICKER
    outw(TSIDX, 0x0300);
#endif
    outw(GDCIDX, 0x0204);
    outw(GDCIDX, 0x0005);
    outw(GDCIDX, 0x0406);               /* addr = a0000, 64kb */
#endif

    splx(s);
}

static void
set_normal_mode(video_adapter_t *adp, u_char *buf)
{
    int s;

    s = splhigh();

    /* setup vga for normal operation mode again */
    inb(adp->va_crtc_addr + 6);			/* reset flip-flop */
    outb(ATC, 0x10); outb(ATC, buf[5]);
    inb(adp->va_crtc_addr + 6);			/* reset flip-flop */
    outb(ATC, 0x20);				/* enable palette */

#if SLOW_VGA
#ifndef SC_BAD_FLICKER
    outb(TSIDX, 0x00); outb(TSREG, 0x01);
#endif
    outb(TSIDX, 0x02); outb(TSREG, buf[0]);
    outb(TSIDX, 0x04); outb(TSREG, buf[1]);
#ifndef SC_BAD_FLICKER
    outb(TSIDX, 0x00); outb(TSREG, 0x03);
#endif
    outb(GDCIDX, 0x04); outb(GDCREG, buf[2]);
    outb(GDCIDX, 0x05); outb(GDCREG, buf[3]);
    if (adp->va_crtc_addr == MONO_BASE) {
	outb(GDCIDX, 0x06); outb(GDCREG,(buf[4] & 0x03) | 0x08);
    } else {
	outb(GDCIDX, 0x06); outb(GDCREG,(buf[4] & 0x03) | 0x0c);
    }
#else
#ifndef SC_BAD_FLICKER
    outw(TSIDX, 0x0100);
#endif
    outw(TSIDX, 0x0002 | (buf[0] << 8));
    outw(TSIDX, 0x0004 | (buf[1] << 8));
#ifndef SC_BAD_FLICKER
    outw(TSIDX, 0x0300);
#endif
    outw(GDCIDX, 0x0004 | (buf[2] << 8));
    outw(GDCIDX, 0x0005 | (buf[3] << 8));
    if (adp->va_crtc_addr == MONO_BASE)
        outw(GDCIDX, 0x0006 | (((buf[4] & 0x03) | 0x08)<<8));
    else
        outw(GDCIDX, 0x0006 | (((buf[4] & 0x03) | 0x0c)<<8));
#endif

    splx(s);
}

/* EGA/VGA */
static int
vid_save_font(int ad, int page, int fontsize, u_char *data, int ch, int count)
{
    u_char buf[PARAM_BUFSIZE];
    u_char val = 0;
    u_int32_t segment;
    int c;
    int s;

    prologue(ad, V_ADP_FONT, 1);

    if (fontsize < 14) {
	/* FONT_8 */
	fontsize = 8;
    } else if (fontsize >= 32) {
	fontsize = 32;
    } else if (fontsize >= 16) {
	/* FONT_16 */
	fontsize = 16;
    } else {
	/* FONT_14 */
	fontsize = 14;
    }

    if (page < 0 || page >= 8)
	return 1;
    segment = VIDEOMEM + 0x4000*page;
    if (page > 3)
	segment -= 0xe000;

    if (adapter[ad].va_type == KD_VGA) {	/* what about EGA? XXX */
	s = splhigh();
	outb(TSIDX, 0x00); outb(TSREG, 0x01);
	outb(TSIDX, 0x01); val = inb(TSREG);	/* disable screen */
	outb(TSIDX, 0x01); outb(TSREG, val | 0x20);
	outb(TSIDX, 0x00); outb(TSREG, 0x03);
	splx(s);
    }

    set_font_mode(&adapter[ad], buf);
    if (fontsize == 32) {
	memcpy_fromio(data, BIOS_PADDRTOVADDR(segment + ch*32),
		      fontsize*count);
    } else {
	for (c = ch; count > 0; ++c, --count) {
	    memcpy_fromio(data, BIOS_PADDRTOVADDR(segment + c*32),
			  fontsize);
	    data += fontsize;
	}
    }
    set_normal_mode(&adapter[ad], buf);

    if (adapter[ad].va_type == KD_VGA) {
	s = splhigh();
	outb(TSIDX, 0x00); outb(TSREG, 0x01);
	outb(TSIDX, 0x01); outb(TSREG, val & 0xdf);	/* enable screen */
	outb(TSIDX, 0x00); outb(TSREG, 0x03);
	splx(s);
    }

    return 0;
}

/* EGA/VGA */
static int
vid_load_font(int ad, int page, int fontsize, u_char *data, int ch, int count)
{
    u_char buf[PARAM_BUFSIZE];
    u_char val = 0;
    u_int32_t segment;
    int c;
    int s;

    prologue(ad, V_ADP_FONT, 1);

    if (fontsize < 14) {
	/* FONT_8 */
	fontsize = 8;
    } else if (fontsize >= 32) {
	fontsize = 32;
    } else if (fontsize >= 16) {
	/* FONT_16 */
	fontsize = 16;
    } else {
	/* FONT_14 */
	fontsize = 14;
    }

    if (page < 0 || page >= 8)
	return 1;
    segment = VIDEOMEM + 0x4000*page;
    if (page > 3)
	segment -= 0xe000;

    if (adapter[ad].va_type == KD_VGA) {	/* what about EGA? XXX */
	s = splhigh();
	outb(TSIDX, 0x00); outb(TSREG, 0x01);
	outb(TSIDX, 0x01); val = inb(TSREG);	/* disable screen */
	outb(TSIDX, 0x01); outb(TSREG, val | 0x20);
	outb(TSIDX, 0x00); outb(TSREG, 0x03);
	splx(s);
    }

    set_font_mode(&adapter[ad], buf);
    if (fontsize == 32) {
	memcpy_toio(BIOS_PADDRTOVADDR(segment + ch*32), data,
		    fontsize*count);
    } else {
	for (c = ch; count > 0; ++c, --count) {
	    memcpy_toio(BIOS_PADDRTOVADDR(segment + c*32), data, 
			fontsize);
	    data += fontsize;
	}
    }
    set_normal_mode(&adapter[ad], buf);

    if (adapter[ad].va_type == KD_VGA) {
	s = splhigh();
	outb(TSIDX, 0x00); outb(TSREG, 0x01);
	outb(TSIDX, 0x01); outb(TSREG, val & 0xdf);	/* enable screen */
	outb(TSIDX, 0x00); outb(TSREG, 0x03);
	splx(s);
    }

    return 0;
}

/* EGA/VGA */
static int
vid_show_font(int ad, int page)
{
    static u_char cg[] = { 0x00, 0x05, 0x0a, 0x0f, 0x30, 0x35, 0x3a, 0x3f };
    int s;

    prologue(ad, V_ADP_FONT, 1);
    if (page < 0 || page >= 8)
	return 1;

    s = splhigh();
    outb(TSIDX, 0x03); outb(TSREG, cg[page]);
    splx(s);

    return 0;
}

/* VGA */
static int
vid_save_palette(int ad, u_char *palette)
{
    int i;

    prologue(ad, V_ADP_PALETTE, 1);

    /* 
     * We store 8 bit values in the palette buffer, while the standard
     * VGA has 6 bit DAC .
     */
    outb(PALRADR, 0x00);
    for (i = 0; i < 256*3; ++i)
	palette[i] = inb(PALDATA) << 2; 
    inb(adapter[ad].va_crtc_addr + 6);	/* reset flip/flop */
    return 0;
}

/* VGA */
static int
vid_load_palette(int ad, u_char *palette)
{
    int i;

    prologue(ad, V_ADP_PALETTE, 1);

    outb(PIXMASK, 0xff);		/* no pixelmask */
    outb(PALWADR, 0x00);
    for (i = 0; i < 256*3; ++i)
	outb(PALDATA, palette[i] >> 2);
    inb(adapter[ad].va_crtc_addr + 6);	/* reset flip/flop */
    outb(ATC, 0x20);			/* enable palette */
    return 0;
}

/* CGA/EGA/VGA */
static int
vid_set_border(int ad, int color)
{
    prologue(ad, V_ADP_BORDER, 1);

    switch (adapter[ad].va_type) {
    case KD_EGA:
    case KD_VGA:    
	inb(adapter[ad].va_crtc_addr + 6);	/* reset flip-flop */
	outb(ATC, 0x31); outb(ATC, color & 0xff); 
	break;  
    case KD_CGA:    
	outb(adapter[ad].va_crtc_addr + 5, color & 0x0f); /* color select register */
	break;  
    case KD_MONO:   
    case KD_HERCULES:
    default:
	break;  
    }
    return 0;
}

/* VGA */
static int
vid_save_state(int ad, void *p, size_t size)
{
    video_info_t info;
    u_char *buf;
    int crtc_addr;
    int i, j;
    int s;

    if (size == 0) {
	/* return the required buffer size */
	prologue(ad, V_ADP_STATESAVE, 0);
	return sizeof(adp_state_t);
    } else {
	prologue(ad, V_ADP_STATESAVE, 1);
	if (size < sizeof(adp_state_t))
	    return 1;
    }

    ((adp_state_t *)p)->sig = V_STATE_SIG;
    buf = ((adp_state_t *)p)->regs;
    bzero(buf, V_MODE_PARAM_SIZE);
    crtc_addr = adapter[ad].va_crtc_addr;

    s = splhigh();

    outb(TSIDX, 0x00); outb(TSREG, 0x01);	/* stop sequencer */
    for (i = 0, j = 5; i < 4; i++) {           
	outb(TSIDX, i + 1);
	buf[j++]  =  inb(TSREG);
    }
    buf[9]  =  inb(MISC + 10);			/* dot-clock */
    outb(TSIDX, 0x00); outb(TSREG, 0x03);	/* start sequencer */

    for (i = 0, j = 10; i < 25; i++) {		/* crtc */
	outb(crtc_addr, i);
	buf[j++]  =  inb(crtc_addr + 1);
    }
    for (i = 0, j = 35; i < 20; i++) {		/* attribute ctrl */
        inb(crtc_addr + 6);			/* reset flip-flop */
	outb(ATC, i);
	buf[j++]  =  inb(ATC + 1);
    }
    for (i = 0, j = 55; i < 9; i++) {		/* graph data ctrl */
	outb(GDCIDX, i);
	buf[j++]  =  inb(GDCREG);
    }
    inb(crtc_addr + 6);				/* reset flip-flop */
    outb(ATC, 0x20);				/* enable palette */

    splx(s);

#if 1
    if (vid_get_info(ad, adapter[ad].va_mode, &info) == 0) {
	if (info.vi_flags & V_INFO_GRAPHICS) {
	    buf[0] = info.vi_width/info.vi_cwidth; /* COLS */
	    buf[1] = info.vi_height/info.vi_cheight - 1; /* ROWS */
	} else {
	    buf[0] = info.vi_width;		/* COLS */
	    buf[1] = info.vi_height - 1;	/* ROWS */
	}
	buf[2] = info.vi_cheight;		/* POINTS */
    } else {
	/* XXX: shouldn't be happening... */
	printf("video#%d: failed to obtain mode info. (vid_save_state())\n",
	       ad);
    }
#else
    buf[0] = *(u_int8_t *)BIOS_PADDRTOVADDR(0x44a);	/* COLS */
    buf[1] = *(u_int8_t *)BIOS_PADDRTOVADDR(0x484);	/* ROWS */
    buf[2] = *(u_int8_t *)BIOS_PADDRTOVADDR(0x485);	/* POINTS */
    buf[3] = *(u_int8_t *)BIOS_PADDRTOVADDR(0x44c);
    buf[4] = *(u_int8_t *)BIOS_PADDRTOVADDR(0x44d);
#endif

    return 0;
}

/* EGA/VGA */
static int
vid_load_state(int ad, void *p)
{
    u_char *buf;
    int crtc_addr;
    int s;
    int i;

    prologue(ad, V_ADP_STATELOAD, 1);
    if (((adp_state_t *)p)->sig != V_STATE_SIG)
	return 1;

    buf = ((adp_state_t *)p)->regs;
    crtc_addr = adapter[ad].va_crtc_addr;

    s = splhigh();

    outb(TSIDX, 0x00); outb(TSREG, 0x01);	/* stop sequencer */
    for (i = 0; i < 4; ++i) {			/* program sequencer */
	outb(TSIDX, i + 1);
	outb(TSREG, buf[i + 5]);
    }
    outb(MISC, buf[9]);				/* set dot-clock */
    outb(TSIDX, 0x00); outb(TSREG, 0x03);	/* start sequencer */
    outb(crtc_addr, 0x11);
    outb(crtc_addr + 1, inb(crtc_addr + 1) & 0x7F);
    for (i = 0; i < 25; ++i) {			/* program crtc */
	outb(crtc_addr, i);
	outb(crtc_addr + 1, buf[i + 10]);
    }
    inb(crtc_addr+6);				/* reset flip-flop */
    for (i = 0; i < 20; ++i) {			/* program attribute ctrl */
	outb(ATC, i);
	outb(ATC, buf[i + 35]);
    }
    for (i = 0; i < 9; ++i) {			/* program graph data ctrl */
	outb(GDCIDX, i);
	outb(GDCREG, buf[i + 55]);
    }
    inb(crtc_addr + 6);				/* reset flip-flop */
    outb(ATC, 0x20);				/* enable palette */

#ifdef __i386__
    if (ad == V_ADP_PRIMARY) {
	*(u_int8_t *)BIOS_PADDRTOVADDR(0x44a) = buf[0];	/* COLS */
	*(u_int8_t *)BIOS_PADDRTOVADDR(0x484) = buf[1] + rows_offset - 1; /* ROWS */
	*(u_int8_t *)BIOS_PADDRTOVADDR(0x485) = buf[2];	/* POINTS */
#if 0
	*(u_int8_t *)BIOS_PADDRTOVADDR(0x44c) = buf[3];
	*(u_int8_t *)BIOS_PADDRTOVADDR(0x44d) = buf[4];
#endif
    }
#endif

    splx(s);
    return 0;
}

/* all */
static int
vid_set_origin(int ad, off_t offset)
{
    /* 
     * The standard video modes do not require window mapping; 
     * always return error.
     */
    return 1;
}

/* all */
static int
vid_read_hw_cursor(int ad, int *col, int *row)
{
    video_info_t info;
    u_int16_t off;

    if (!init_done)
	return 1;
    if ((ad < 0) || (ad >= adapters))
	return 1;

    (*biosvidsw.get_info)(ad, adapter[ad].va_mode, &info);
    if (info.vi_flags & V_INFO_GRAPHICS)
	return 1;

    outb(adapter[ad].va_crtc_addr, 14);
    off = inb(adapter[ad].va_crtc_addr + 1);
    outb(adapter[ad].va_crtc_addr, 15);
    off = (off << 8) | inb(adapter[ad].va_crtc_addr + 1);

    *row = off / info.vi_width;
    *col = off % info.vi_width;

    return 0;
}

/* all */
static int
vid_set_hw_cursor(int ad, int col, int row)
{
    video_info_t info;
    u_int16_t off;

    if (!init_done)
	return 1;
    if ((ad < 0) || (ad >= adapters))
	return 1;

    (*biosvidsw.get_info)(ad, adapter[ad].va_mode, &info);
    if (info.vi_flags & V_INFO_GRAPHICS)
	return 1;

    if ((col == -1) || (row == -1))
	off = 0xffff;
    else
	off = row*info.vi_width + col;
    outb(adapter[ad].va_crtc_addr, 14);
    outb(adapter[ad].va_crtc_addr + 1, off >> 8);
    outb(adapter[ad].va_crtc_addr, 15);
    outb(adapter[ad].va_crtc_addr + 1, off & 0x00ff);

    return 0;
}

static char
*adapter_name(int type)
{
    static struct {
	int type;
	char *name;
    } names[] = {
	{ KD_MONO,	"MDA" },
	{ KD_HERCULES,	"Hercules" },
	{ KD_CGA,	"CGA" },
	{ KD_EGA,	"EGA" },
	{ KD_VGA,	"VGA" },
	{ KD_PC98,	"PC-98xx" },
	{ -1,		"Unknown" },
    };
    int i;

    for (i = 0; names[i].type != -1; ++i)
	if (names[i].type == type)
	    break;
    return names[i].name;
}

static void
dump_adp_info(int ad, video_adapter_t *adp, int level)
{
    if (level <= 0)
	return;

    printf("video#%d: adapter type:%s (%d), flags:0x%x, CRTC:0x%x\n", 
	   ad, adapter_name(adp->va_type), adp->va_type, 
	   adp->va_flags, adp->va_crtc_addr);
    printf("video#%d: init mode:%d, bios mode:%d, current mode:%d\n",
	   ad, adp->va_initial_mode, adp->va_initial_bios_mode, adp->va_mode);
    printf("video#%d: window:0x%x size:%dk gran:%dk, buf:0x%x size:%dk\n",
	   ad, 
	   adp->va_window, (int)adp->va_window_size, (int)adp->va_window_gran,
	   adp->va_buffer, (int)adp->va_buffer_size);
}

static void
dump_mode_info(int ad, video_info_t *info, int level)
{
    if (level <= 0)
	return;

    printf("video#%d: mode:%d, flags:0x%x ", 
	   ad, info->vi_mode, info->vi_flags);
    if (info->vi_flags & V_INFO_GRAPHICS)
	printf("G %dx%dx%d, %d plane(s), font:%dx%d, ",
	       info->vi_width, info->vi_height, 
	       info->vi_depth, info->vi_planes, 
	       info->vi_cwidth, info->vi_cheight); 
    else
	printf("T %dx%d, font:%dx%d, ",
	       info->vi_width, info->vi_height, 
	       info->vi_cwidth, info->vi_cheight); 
    printf("win:0x%x\n", info->vi_window);
}

static void
dump_buffer(u_char *buf, size_t len)
{
    int i;

    for(i = 0; i < len;) {
	printf("%02x ", buf[i]);
	if ((++i % 16) == 0)
	    printf("\n");
    }
}

static int
vid_diag(int level)
{
    video_info_t info;
    u_char *mp;
    int ad;
    int i;

    if (!init_done)
	return 1;

#ifdef __i386__
    if (level > 0) {
	printf("video: RTC equip. code:0x%02x, DCC code:0x%02x\n",
	       rtcin(RTC_EQUIPMENT), *(u_int8_t *)BIOS_PADDRTOVADDR(0x488));
	printf("video: CRTC:0x%x, video option:0x%02x, ",
	       *(u_int16_t *)BIOS_PADDRTOVADDR(0x463),
	       *(u_int8_t *)BIOS_PADDRTOVADDR(0x487));
	printf("rows:%d, cols:%d, font height:%d\n",
	       *(u_int8_t *)BIOS_PADDRTOVADDR(0x44a),
	       *(u_int8_t *)BIOS_PADDRTOVADDR(0x484) + 1,
	       *(u_int8_t *)BIOS_PADDRTOVADDR(0x485));
	printf("video: param table EGA/VGA:%p, CGA/MDA:%p\n", 
	       video_mode_ptr, video_mode_ptr2);
	printf("video: rows_offset:%d\n", rows_offset);
    }
#endif

    for (ad = 0; ad < adapters; ++ad) {
	dump_adp_info(ad, &adapter[ad], level);

	if (!(adapter[ad].va_flags & V_ADP_MODECHANGE)) {
	    vid_get_info(ad, adapter[ad].va_initial_mode, &info);
	    dump_mode_info(ad, &info, level);
	} else {
	    for (i = 0; bios_vmode[i].vi_mode != EOT; ++i) {
		if (bios_vmode[i].vi_mode == NA)
		    continue;
		if (get_mode_param(bios_vmode[i].vi_mode) == NULL)
		    continue;
		dump_mode_info(ad, &bios_vmode[i], level);
	    }
	}
	if ((adapter[ad].va_type != KD_EGA) && (adapter[ad].va_type != KD_VGA))
	    continue;

	if (video_mode_ptr == NULL)
	    printf("video#%d: WARNING: video mode switching is not fully supported on this adapter\n", ad);

	if (level <= 0)
	    continue;

	if (adapter[ad].va_type == KD_VGA) {
	    printf("VGA parameters upon power-up\n");
	    dump_buffer(adpstate.regs, sizeof(adpstate.regs));
	    printf("VGA parameters in BIOS for mode %d\n", 
		adapter[ad].va_initial_mode);
	    dump_buffer(adpstate2.regs, sizeof(adpstate2.regs));
	}

	mp = get_mode_param(adapter[ad].va_initial_mode);
	if (mp == NULL)	/* this shouldn't be happening */
	    continue;
	printf("EGA/VGA parameters to be used for mode %d\n", 
	    adapter[ad].va_initial_mode);
	dump_buffer(mp, V_MODE_PARAM_SIZE);
    }

    return 0;
}

#endif /* NSC > 0 */
