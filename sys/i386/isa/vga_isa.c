/*-
 * Copyright (c) 1999 Kazutaka YOKOTA <yokota@zodiac.mech.utsunomiya-u.ac.jp>
 * Copyright (c) 1992-1998 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: vga_isa.c,v 1.1.2.1 1999/02/07 03:03:26 yokota Exp $
 */

#include "vga.h"
#include "opt_vga.h"
#include "opt_fb.h"
#include "opt_syscons.h"	/* should be removed in the future, XXX */

#if NVGA > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/console.h>
#include <machine/md_var.h>
#include <machine/pc/bios.h>

#include <dev/fb/fbreg.h>
#include <dev/fb/vgareg.h>

#ifndef __i386__
#include <isa/isareg.h>
#include <isa/isavar.h>
#else
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#endif

#define DRIVER_NAME		"vga"

/* cdev driver declaration */

#define ISAVGA_UNIT(dev)	minor(dev)
#define ISAVGA_MKMINOR(unit)	(unit)

typedef struct isavga_softc {
	video_adapter_t	*adp;
} isavga_softc_t;

#ifndef __i386__

#define ISAVGA_SOFTC(unit)		\
	((isavga_softc_t *)devclass_get_softc(isavga_devclass, unit))

devclass_t		isavga_devclass;

static int		isavga_probe(device_t dev);
static int		isavga_attach(device_t dev);

static device_method_t isavga_methods[] = {
	DEVMETHOD(device_probe,		isavga_probe),
	DEVMETHOD(device_attach,	isavga_attach),
	{ 0, 0 }
};

static driver_t isavga_driver = {
	DRIVER_NAME,
	isavga_methods,
	DRIVER_TYPE_TTY,
	sizeof(isavga_softc_t),
};

#else /* __i386__ */

#define ISAVGA_SOFTC(unit)	(isavga_softc[unit])

static isavga_softc_t	*isavga_softc[NVGA];

static int		isavga_probe(struct isa_device *dev);
static int		isavga_attach(struct isa_device *dev);

struct isa_driver vgadriver = {
	isavga_probe,
	isavga_attach,
	DRIVER_NAME,
	0,
};

#endif /* __i386__ */

static int		isavga_probe_unit(int unit, isavga_softc_t *sc,
					  int flags);
static int		isavga_attach_unit(int unit, isavga_softc_t *sc,
					   int flags);

#ifdef FB_INSTALL_CDEV

static d_open_t		isavgaopen;
static d_close_t	isavgaclose;
static d_read_t		isavgaread;
static d_ioctl_t	isavgaioctl;

static struct  cdevsw vga_cdevsw = {
	isavgaopen,	isavgaclose,	noread,		nowrite,	/* ?? */
	isavgaioctl,	nostop,		nullreset,	nodevtotty,
	seltrue,	nommap,		NULL,		DRIVER_NAME,
	NULL,		-1,		nodump,		nopsize,
};

#endif /* FB_INSTALL_CDEV */

#ifndef __i386__

static int
isavga_probe(device_t dev)
{
	isavga_softc_t *sc;

	sc = device_get_softc(dev);
	return isavga_probe_unit(device_get_unit(dev), sc, isa_get_flags(dev));
}

static int
isavga_attach(device_t dev)
{
	isavga_softc_t *sc;

	sc = device_get_softc(dev);
	return isavga_attach_unit(device_get_unit(dev), sc, isa_get_flags(dev));
}

#else /* __i386__ */

static int
isavga_probe(struct isa_device *dev)
{
	isavga_softc_t *sc;
	int error;

	if (dev->id_unit >= sizeof(isavga_softc)/sizeof(isavga_softc[0]))
		return 0;
	sc = isavga_softc[dev->id_unit]
	   = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT);
	if (sc == NULL)
		return 0;

	error = isavga_probe_unit(dev->id_unit, sc, dev->id_flags);
	if (error) {
		isavga_softc[dev->id_unit] = NULL;
		free(sc, M_DEVBUF);
		return 0;
	}

	dev->id_iobase = sc->adp->va_io_base;
	dev->id_maddr = (caddr_t)BIOS_PADDRTOVADDR(sc->adp->va_mem_base);
	dev->id_msize = sc->adp->va_mem_size;

	return sc->adp->va_io_size;
}

static int
isavga_attach(struct isa_device *dev)
{
	isavga_softc_t *sc;

	if (dev->id_unit >= sizeof(isavga_softc)/sizeof(isavga_softc[0]))
		return 0;
	sc = isavga_softc[dev->id_unit];
	if (sc == NULL)
		return 0;

	return ((isavga_attach_unit(dev->id_unit, sc, dev->id_flags)) ? 0 : 1);
}

#endif /* __i386__ */

static int
isavga_probe_unit(int unit, isavga_softc_t *sc, int flags)
{
	video_switch_t *sw;

	bzero(sc, sizeof(*sc));
	sw = vid_get_switch(DRIVER_NAME);
	if (sw == NULL)
		return 0;
	return (*sw->probe)(unit, &sc->adp, NULL, flags);
}

static int
isavga_attach_unit(int unit, isavga_softc_t *sc, int flags)
{
	video_switch_t *sw;
	int error;

	sw = vid_get_switch(DRIVER_NAME);
	if (sw == NULL)
		return ENXIO;

	error = (*sw->init)(unit, sc->adp, flags);
	if (error)
		return ENXIO;

#ifdef FB_INSTALL_CDEV
	/* attach a virtual frame buffer device */
	error = fb_attach(makedev(0, ISAVGA_MKMINOR(unit)), scp->adp,
			  &vga_cdevsw);
	if (error)
		return error;
#endif /* FB_INSTALL_CDEV */

	if (bootverbose)
		(*vidsw[sc->adp->va_index]->diag)(sc->adp, bootverbose);

	return 0;
}

/* LOW-LEVEL */

#include <machine/clock.h>
#include <machine/pc/vesa.h>

#define probe_done(adp)		((adp)->va_flags & V_ADP_PROBED)
#define init_done(adp)		((adp)->va_flags & V_ADP_INITIALIZED)
#define config_done(adp)	((adp)->va_flags & V_ADP_REGISTERED)

/* for compatibility with old kernel options */
#ifdef SC_ALT_SEQACCESS
#undef SC_ALT_SEQACCESS
#undef VGA_ALT_SEQACCESS
#define VGA_ALT_SEQACCESS	1
#endif

#ifdef SLOW_VGA
#undef SLOW_VGA
#undef VGA_SLOW_IOACCESS
#define VGA_SLOW_IOACCESS	1
#endif

/* architecture dependent option */
#ifdef __alpha__
#define VGA_NO_BIOS		1
#endif

/* this should really be in `rtc.h' */
#define RTC_EQUIPMENT           0x14

/* various sizes */
#define V_MODE_MAP_SIZE		(M_VGA_CG320 + 1)
#define V_MODE_PARAM_SIZE	64

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
    /* DCC_MONO */
    { 0, KD_MONO, "mda", 0, 0, 0, 	    IO_MDA, IO_MDASIZE, MONO_CRTC,
      MDA_BUF_BASE, MDA_BUF_SIZE, MDA_BUF_BASE, MDA_BUF_SIZE, MDA_BUF_SIZE, 
      0, 0, 0, 7, 0, },
    /* DCC_CGA40 */
    { 0, KD_CGA,  "cga", 0, 0, V_ADP_COLOR, IO_CGA, IO_CGASIZE, COLOR_CRTC,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 
      0, 0, 0, 3, 0, },
    /* DCC_CGA80 */
    { 0, KD_CGA,  "cga", 0, 0, V_ADP_COLOR, IO_CGA, IO_CGASIZE, COLOR_CRTC,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 
      0, 0, 0, 3, 0, },
    /* DCC_EGAMONO */
    { 0, KD_EGA,  "ega", 0, 0, 0,	    IO_MDA, 48,	  MONO_CRTC,
      EGA_BUF_BASE, EGA_BUF_SIZE, MDA_BUF_BASE, MDA_BUF_SIZE, MDA_BUF_SIZE, 
      0, 0, 0, 7, 0, },
    /* DCC_EGA40 */
    { 0, KD_EGA,  "ega", 0, 0, V_ADP_COLOR, IO_MDA, 48,	  COLOR_CRTC,
      EGA_BUF_BASE, EGA_BUF_SIZE, CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 
      0, 0, 0, 3, 0, },
    /* DCC_EGA80 */
    { 0, KD_EGA,  "ega", 0, 0, V_ADP_COLOR, IO_MDA, 48,	  COLOR_CRTC,
      EGA_BUF_BASE, EGA_BUF_SIZE, CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 
      0, 0, 0, 3, 0, },
};

static video_adapter_t	biosadapter[2];
static int		biosadapters = 0;

/* video driver declarations */
static int			vga_configure(int flags);
       int			(*vga_sub_configure)(int flags);
static int			vga_nop(void);
static vi_probe_t		vga_probe;
static vi_init_t		vga_init;
static vi_get_info_t		vga_get_info;
static vi_query_mode_t		vga_query_mode;
static vi_set_mode_t		vga_set_mode;
static vi_save_font_t		vga_save_font;
static vi_load_font_t		vga_load_font;
static vi_show_font_t		vga_show_font;
static vi_save_palette_t	vga_save_palette;
static vi_load_palette_t	vga_load_palette;
static vi_set_border_t		vga_set_border;
static vi_save_state_t		vga_save_state;
static vi_load_state_t		vga_load_state;
static vi_set_win_org_t		vga_set_origin;
static vi_read_hw_cursor_t	vga_read_hw_cursor;
static vi_set_hw_cursor_t	vga_set_hw_cursor;
static vi_set_hw_cursor_shape_t	vga_set_hw_cursor_shape;
static vi_mmap_t		vga_mmap;
static vi_diag_t		vga_diag;

static video_switch_t vgavidsw = {
	vga_probe,
	vga_init,
	vga_get_info,
	vga_query_mode,	
	vga_set_mode,
	vga_save_font,
	vga_load_font,
	vga_show_font,
	vga_save_palette,
	vga_load_palette,
	vga_set_border,
	vga_save_state,
	vga_load_state,
	vga_set_origin,
	vga_read_hw_cursor,
	vga_set_hw_cursor,
	vga_set_hw_cursor_shape,
	(vi_blank_display_t *)vga_nop,
	vga_mmap,
	vga_diag,
};

VIDEO_DRIVER(mda, vgavidsw, NULL);
VIDEO_DRIVER(cga, vgavidsw, NULL);
VIDEO_DRIVER(ega, vgavidsw, NULL);
VIDEO_DRIVER(vga, vgavidsw, vga_configure);

/* VGA BIOS standard video modes */
#define EOT		(-1)
#define NA		(-2)

static video_info_t bios_vmode[] = {
    /* CGA */
    { M_B40x25,     V_INFO_COLOR, 40, 25, 8,  8, 2, 1,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 0, 0 },
    { M_C40x25,     V_INFO_COLOR, 40, 25, 8,  8, 4, 1,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 0, 0 },
    { M_B80x25,     V_INFO_COLOR, 80, 25, 8,  8, 2, 1,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 0, 0 },
    { M_C80x25,     V_INFO_COLOR, 80, 25, 8,  8, 4, 1,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 0, 0 },
    /* EGA */
    { M_ENH_B40x25, V_INFO_COLOR, 40, 25, 8, 14, 2, 1,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 0, 0 },
    { M_ENH_C40x25, V_INFO_COLOR, 40, 25, 8, 14, 4, 1,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 0, 0 },
    { M_ENH_B80x25, V_INFO_COLOR, 80, 25, 8, 14, 2, 1,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 0, 0 },
    { M_ENH_C80x25, V_INFO_COLOR, 80, 25, 8, 14, 4, 1,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 0, 0 },
    /* VGA */
    { M_VGA_C40x25, V_INFO_COLOR, 40, 25, 8, 16, 4, 1,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 0, 0 },
    { M_VGA_M80x25, 0,            80, 25, 8, 16, 2, 1,
      MDA_BUF_BASE, MDA_BUF_SIZE, MDA_BUF_SIZE, 0, 0 },
    { M_VGA_C80x25, V_INFO_COLOR, 80, 25, 8, 16, 4, 1,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 0, 0 },
    /* MDA */
    { M_EGAMONO80x25, 0,          80, 25, 8, 14, 2, 1,
      MDA_BUF_BASE, MDA_BUF_SIZE, MDA_BUF_SIZE, 0, 0 },
    /* EGA */
    { M_ENH_B80x43, V_INFO_COLOR, 80, 43, 8,  8, 2, 1,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 0, 0 },
    { M_ENH_C80x43, V_INFO_COLOR, 80, 43, 8,  8, 4, 1,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 0, 0 },
    /* VGA */
    { M_VGA_M80x30, 0,            80, 30, 8, 16, 2, 1,
      MDA_BUF_BASE, MDA_BUF_SIZE, MDA_BUF_SIZE, 0, 0 },
    { M_VGA_C80x30, V_INFO_COLOR, 80, 30, 8, 16, 4, 1,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 0, 0 },
    { M_VGA_M80x50, 0,            80, 50, 8,  8, 2, 1,
      MDA_BUF_BASE, MDA_BUF_SIZE, MDA_BUF_SIZE, 0, 0 },
    { M_VGA_C80x50, V_INFO_COLOR, 80, 50, 8,  8, 4, 1,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 0, 0 },
    { M_VGA_M80x60, 0,            80, 60, 8,  8, 2, 1,
      MDA_BUF_BASE, MDA_BUF_SIZE, MDA_BUF_SIZE, 0, 0 },
    { M_VGA_C80x60, V_INFO_COLOR, 80, 60, 8,  8, 4, 1,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 0, 0 },
#ifndef VGA_NO_MODE_CHANGE
    /* CGA */
    { M_BG320,      V_INFO_COLOR | V_INFO_GRAPHICS, 320, 200, 8,  8, 2, 1,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 0, 0 },
    { M_CG320,      V_INFO_COLOR | V_INFO_GRAPHICS, 320, 200, 8,  8, 2, 1,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 0, 0 },
    { M_BG640,      V_INFO_COLOR | V_INFO_GRAPHICS, 640, 200, 8,  8, 1, 1,
      CGA_BUF_BASE, CGA_BUF_SIZE, CGA_BUF_SIZE, 0, 0 },
    /* EGA */
    { M_CG320_D,    V_INFO_COLOR | V_INFO_GRAPHICS, 320, 200, 8,  8, 4, 4,
      GRAPHICS_BUF_BASE, GRAPHICS_BUF_SIZE, GRAPHICS_BUF_SIZE, 0, 0 },
    { M_CG640_E,    V_INFO_COLOR | V_INFO_GRAPHICS, 640, 200, 8,  8, 4, 4,
      GRAPHICS_BUF_BASE, GRAPHICS_BUF_SIZE, GRAPHICS_BUF_SIZE, 0, 0 },
    { M_EGAMONOAPA, V_INFO_GRAPHICS,                640, 350, 8, 14, 4, 4,
      GRAPHICS_BUF_BASE, GRAPHICS_BUF_SIZE, 64*1024, 0, 0 },
    { M_ENHMONOAPA2,V_INFO_GRAPHICS,                640, 350, 8, 14, 4, 4,
      GRAPHICS_BUF_BASE, GRAPHICS_BUF_SIZE, GRAPHICS_BUF_SIZE, 0, 0 },
    { M_CG640x350,  V_INFO_COLOR | V_INFO_GRAPHICS, 640, 350, 8, 14, 2, 2,
      GRAPHICS_BUF_BASE, GRAPHICS_BUF_SIZE, GRAPHICS_BUF_SIZE, 0, 0 },
    { M_ENH_CG640,  V_INFO_COLOR | V_INFO_GRAPHICS, 640, 350, 8, 14, 4, 4,
      GRAPHICS_BUF_BASE, GRAPHICS_BUF_SIZE, GRAPHICS_BUF_SIZE, 0, 0 },
    /* VGA */
    { M_BG640x480,  V_INFO_COLOR | V_INFO_GRAPHICS, 640, 480, 8, 16, 4, 4,
      GRAPHICS_BUF_BASE, GRAPHICS_BUF_SIZE, GRAPHICS_BUF_SIZE, 0, 0 },
    { M_CG640x480,  V_INFO_COLOR | V_INFO_GRAPHICS, 640, 480, 8, 16, 4, 4,
      GRAPHICS_BUF_BASE, GRAPHICS_BUF_SIZE, GRAPHICS_BUF_SIZE, 0, 0 },
    { M_VGA_CG320,  V_INFO_COLOR | V_INFO_GRAPHICS, 320, 200, 8,  8, 8, 1,
      GRAPHICS_BUF_BASE, GRAPHICS_BUF_SIZE, GRAPHICS_BUF_SIZE, 0, 0 },
    { M_VGA_MODEX,  V_INFO_COLOR | V_INFO_GRAPHICS, 320, 240, 8,  8, 8, 1,
      GRAPHICS_BUF_BASE, GRAPHICS_BUF_SIZE, GRAPHICS_BUF_SIZE, 0, 0 },
#endif /* VGA_NO_MODE_CHANGE */

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

#if !defined(VGA_NO_BIOS) && !defined(VGA_NO_MODE_CHANGE)
static void map_mode_table(u_char *map[], u_char *table, int max);
#endif
static void clear_mode_map(video_adapter_t *adp, u_char *map[], int max,
			   int color);
#if !defined(VGA_NO_BIOS) && !defined(VGA_NO_MODE_CHANGE)
static int map_mode_num(int mode);
#endif
static int map_gen_mode_num(int type, int color, int mode);
static int map_bios_mode_num(int type, int color, int bios_mode);
static u_char *get_mode_param(int mode);
#ifndef VGA_NO_BIOS
static void fill_adapter_param(int code, video_adapter_t *adp);
#endif
static int verify_adapter(video_adapter_t *adp);
static void update_adapter_info(video_adapter_t *adp, video_info_t *info);
#if !defined(VGA_NO_BIOS) && !defined(VGA_NO_MODE_CHANGE)
#define COMP_IDENTICAL	0
#define COMP_SIMILAR	1
#define COMP_DIFFERENT	2
static int comp_adpregs(u_char *buf1, u_char *buf2);
#endif
static int probe_adapters(void);

#define PARAM_BUFSIZE	6
static void set_font_mode(video_adapter_t *adp, u_char *buf);
static void set_normal_mode(video_adapter_t *adp, u_char *buf);

static void dump_buffer(u_char *buf, size_t len);

#define	ISMAPPED(pa, width)				\
	(((pa) <= (u_long)0x1000 - (width)) 		\
	 || ((pa) >= ISA_HOLE_START && (pa) <= 0x100000 - (width)))

#define	prologue(adp, flag, err)			\
	if (!init_done || !((adp)->va_flags & (flag)))	\
	    return (err)

/* a backdoor for the console driver */
static int
vga_configure(int flags)
{
    int i;

    probe_adapters();
    for (i = 0; i < biosadapters; ++i) {
	if (!probe_done(&biosadapter[i]))
	    continue;
	biosadapter[i].va_flags |= V_ADP_INITIALIZED;
	if (!config_done(&biosadapter[i])) {
	    if (vid_register(&biosadapter[i]) < 0)
		continue;
	    biosadapter[i].va_flags |= V_ADP_REGISTERED;
	}
    }
    if (vga_sub_configure != NULL)
	(*vga_sub_configure)(flags);

    return biosadapters;
}

/* local subroutines */

#if !defined(VGA_NO_BIOS) && !defined(VGA_NO_MODE_CHANGE)
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
#endif /* !VGA_NO_BIOS && !VGA_NO_MODE_CHANGE */

static void
clear_mode_map(video_adapter_t *adp, u_char *map[], int max, int color)
{
    video_info_t info;
    int i;

    /*
     * NOTE: we don't touch `bios_vmode[]' because it is shared
     * by all adapters.
     */
    for(i = 0; i < max; ++i) {
	if (vga_get_info(adp, i, &info))
	    continue;
	if ((info.vi_flags & V_INFO_COLOR) != color)
	    map[i] = NULL;
    }
}

#if !defined(VGA_NO_BIOS) && !defined(VGA_NO_MODE_CHANGE)
/* map the non-standard video mode to a known mode number */
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
#endif /* !VGA_NO_BIOS && !VGA_NO_MODE_CHANGE */

/* map a generic video mode to a known mode number */
static int
map_gen_mode_num(int type, int color, int mode)
{
    static struct {
	int from;
	int to_color;
	int to_mono;
    } mode_map[] = {
	{ M_TEXT_80x30,	M_VGA_C80x30, M_VGA_M80x30, },
	{ M_TEXT_80x43,	M_ENH_C80x43, M_ENH_B80x43, },
	{ M_TEXT_80x50,	M_VGA_C80x50, M_VGA_M80x50, },
	{ M_TEXT_80x60,	M_VGA_C80x60, M_VGA_M80x60, },
    };
    int i;

    if (mode == M_TEXT_80x25) {
	switch (type) {

	case KD_VGA:
	    if (color)
		return M_VGA_C80x25;
	    else
		return M_VGA_M80x25;
	    break;

	case KD_EGA:
	    if (color)
		return M_ENH_C80x25;
	    else
		return M_EGAMONO80x25;
	    break;

	case KD_CGA:
	    return M_C80x25;

	case KD_MONO:
	case KD_HERCULES:
	    return M_EGAMONO80x25;	/* XXX: this name is confusing */

 	default:
	    return -1;
	}
    }

    for (i = 0; i < sizeof(mode_map)/sizeof(mode_map[0]); ++i) {
        if (mode_map[i].from == mode)
            return ((color) ? mode_map[i].to_color : mode_map[i].to_mono);
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
#if !defined(VGA_NO_BIOS) && !defined(VGA_NO_MODE_CHANGE)
    if (mode >= V_MODE_MAP_SIZE)
	mode = map_mode_num(mode);
#endif
    if ((mode >= 0) && (mode < V_MODE_MAP_SIZE))
	return mode_map[mode];
    else
	return NULL;
}

#ifndef VGA_NO_BIOS
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
#endif /* VGA_NO_BIOS */

static int
verify_adapter(video_adapter_t *adp)
{
    volatile u_int16_t *buf;
    u_int16_t v;
    u_int32_t p;

    buf = (u_int16_t *)BIOS_PADDRTOVADDR(adp->va_window);
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
	    adp->va_name = "vga";
	    adp->va_flags |= V_ADP_STATESAVE | V_ADP_PALETTE;
	}
	adp->va_flags |= V_ADP_STATELOAD | V_ADP_BORDER;
	/* the color adapter may be in the 40x25 mode... XXX */

#if !defined(VGA_NO_BIOS) && !defined(VGA_NO_MODE_CHANGE)
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
#if !defined(VGA_NO_BIOS) && !defined(VGA_NO_MODE_CHANGE)
	/* get the BIOS video mode pointer */
	p = *(u_int32_t *)BIOS_PADDRTOVADDR(0x1d*4);
	p = BIOS_SADDRTOLADDR(p);
	video_mode_ptr2 = (u_char *)BIOS_PADDRTOVADDR(p);
#endif
	break;

    case KD_MONO:
#if !defined(VGA_NO_BIOS) && !defined(VGA_NO_MODE_CHANGE)
	/* get the BIOS video mode pointer */
	p = *(u_int32_t *)BIOS_PADDRTOVADDR(0x1d*4);
	p = BIOS_SADDRTOLADDR(p);
	video_mode_ptr2 = (u_char *)BIOS_PADDRTOVADDR(p);
#endif
	break;
    }

    return 0;
}

static void
update_adapter_info(video_adapter_t *adp, video_info_t *info)
{
    adp->va_flags &= ~V_ADP_COLOR;
    adp->va_flags |= 
	(info->vi_flags & V_INFO_COLOR) ? V_ADP_COLOR : 0;
    adp->va_crtc_addr =
	(adp->va_flags & V_ADP_COLOR) ? COLOR_CRTC : MONO_CRTC;
    adp->va_window = BIOS_PADDRTOVADDR(info->vi_window);
    adp->va_window_size = info->vi_window_size;
    adp->va_window_gran = info->vi_window_gran;
    if (info->vi_buffer_size == 0) {
    	adp->va_buffer = 0;
    	adp->va_buffer_size = 0;
    } else {
    	adp->va_buffer = BIOS_PADDRTOVADDR(info->vi_buffer);
    	adp->va_buffer_size = info->vi_buffer_size;
    }
    if (info->vi_flags & V_INFO_GRAPHICS) {
	switch (info->vi_depth/info->vi_planes) {
	case 1:
	    adp->va_line_width = info->vi_width/8;
	    break;
	case 2:
	    adp->va_line_width = info->vi_width/4;
	    break;
	case 4:
	    adp->va_line_width = info->vi_width/2;
	    break;
	case 8:
	default: /* shouldn't happen */
	    adp->va_line_width = info->vi_width;
	    break;
	}
    } else {
	adp->va_line_width = info->vi_width;
    }
    bcopy(info, &adp->va_info, sizeof(adp->va_info));
}

#if !defined(VGA_NO_BIOS) && !defined(VGA_NO_MODE_CHANGE)
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
#endif /* !VGA_NO_BIOS && !VGA_NO_MODE_CHANGE */

/* probe video adapters and return the number of detected adapters */
static int
probe_adapters(void)
{
    video_adapter_t *adp;
    video_info_t info;
    u_char *mp;
    int i;

    /* do this test only once */
    if (init_done)
	return biosadapters;
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

    /* 
     * Check rtc and BIOS data area.
     * XXX: we don't use BIOSDATA_EQUIPMENT, since it is not a dead
     * copy of RTC_EQUIPMENT.  Bits 4 and 5 of ETC_EQUIPMENT are
     * zeros for EGA and VGA.  However, the EGA/VGA BIOS sets
     * these bits in BIOSDATA_EQUIPMENT according to the monitor
     * type detected.
     */
#ifndef VGA_NO_BIOS
    switch ((rtcin(RTC_EQUIPMENT) >> 4) & 3) {	/* bit 4 and 5 */
    case 0:
	/* EGA/VGA */
	fill_adapter_param(readb(BIOS_PADDRTOVADDR(0x488)) & 0x0f, 
			   biosadapter);
	break;
    case 1:
	/* CGA 40x25 */
	/* FIXME: switch to the 80x25 mode? XXX */
	biosadapter[V_ADP_PRIMARY] = adapter_init_value[DCC_CGA40];
	biosadapter[V_ADP_SECONDARY] = adapter_init_value[DCC_MONO];
	break;
    case 2:
	/* CGA 80x25 */
	biosadapter[V_ADP_PRIMARY] = adapter_init_value[DCC_CGA80];
	biosadapter[V_ADP_SECONDARY] = adapter_init_value[DCC_MONO];
	break;
    case 3:
	/* MDA */
	biosadapter[V_ADP_PRIMARY] = adapter_init_value[DCC_MONO];
	biosadapter[V_ADP_SECONDARY] = adapter_init_value[DCC_CGA80];
	break;
    }
#else
    /* assume EGA/VGA? XXX */
    biosadapter[V_ADP_PRIMARY] = adapter_init_value[DCC_EGA80];
    biosadapter[V_ADP_SECONDARY] = adapter_init_value[DCC_MONO];
#endif /* VGA_NO_BIOS */

    biosadapters = 0;
    if (verify_adapter(&biosadapter[V_ADP_SECONDARY]) == 0) {
	++biosadapters;
	biosadapter[V_ADP_SECONDARY].va_flags |= V_ADP_PROBED;
	biosadapter[V_ADP_SECONDARY].va_mode = 
	    biosadapter[V_ADP_SECONDARY].va_initial_mode =
	    map_bios_mode_num(biosadapter[V_ADP_SECONDARY].va_type, 
			      biosadapter[V_ADP_SECONDARY].va_flags
				  & V_ADP_COLOR,
			      biosadapter[V_ADP_SECONDARY].va_initial_bios_mode);
    } else {
	biosadapter[V_ADP_SECONDARY].va_type = -1;
    }
    if (verify_adapter(&biosadapter[V_ADP_PRIMARY]) == 0) {
	++biosadapters;
	biosadapter[V_ADP_PRIMARY].va_flags |= V_ADP_PROBED;
#ifndef VGA_NO_BIOS
	biosadapter[V_ADP_PRIMARY].va_initial_bios_mode = 
	    readb(BIOS_PADDRTOVADDR(0x449));
#else
	biosadapter[V_ADP_PRIMARY].va_initial_bios_mode = 3;	/* XXX */
#endif
	biosadapter[V_ADP_PRIMARY].va_mode = 
	    biosadapter[V_ADP_PRIMARY].va_initial_mode =
	    map_bios_mode_num(biosadapter[V_ADP_PRIMARY].va_type, 
			      biosadapter[V_ADP_PRIMARY].va_flags & V_ADP_COLOR,
			      biosadapter[V_ADP_PRIMARY].va_initial_bios_mode);
    } else {
	biosadapter[V_ADP_PRIMARY] = biosadapter[V_ADP_SECONDARY];
	biosadapter[V_ADP_SECONDARY].va_type = -1;
    }
    if (biosadapters == 0)
	return biosadapters;
    biosadapter[V_ADP_PRIMARY].va_unit = V_ADP_PRIMARY;
    biosadapter[V_ADP_SECONDARY].va_unit = V_ADP_SECONDARY;

#if 0 /* we don't need these... */
    fb_init_struct(&biosadapter[V_ADP_PRIMARY], ...);
    fb_init_struct(&biosadapter[V_ADP_SECONDARY], ...);
#endif

#if 0
    /*
     * We cannot have two video adapter of the same type; there must be
     * only one of color or mono adapter, or one each of them.
     */
    if (biosadapters > 1) {
	if (!((biosadapter[0].va_flags ^ biosadapter[1].va_flags)
	      & V_ADP_COLOR))
	    /* we have two mono or color adapters!! */
	    return (biosadapters = 0);
    }
#endif

    /*
     * Ensure a zero start address.  This is mainly to recover after
     * switching from pcvt using userconfig().  The registers are w/o
     * for old hardware so it's too hard to relocate the active screen
     * memory.
     * This must be done before vga_save_state() for VGA.
     */
    outb(biosadapter[V_ADP_PRIMARY].va_crtc_addr, 12);
    outb(biosadapter[V_ADP_PRIMARY].va_crtc_addr + 1, 0);
    outb(biosadapter[V_ADP_PRIMARY].va_crtc_addr, 13);
    outb(biosadapter[V_ADP_PRIMARY].va_crtc_addr + 1, 0);

    /* the video mode parameter table in EGA/VGA BIOS */
    /* NOTE: there can be only one EGA/VGA, wheather color or mono,
     * recognized by the video BIOS.
     */
    if ((biosadapter[V_ADP_PRIMARY].va_type == KD_EGA) ||
	(biosadapter[V_ADP_PRIMARY].va_type == KD_VGA)) {
	adp = &biosadapter[V_ADP_PRIMARY];
    } else if ((biosadapter[V_ADP_SECONDARY].va_type == KD_EGA) ||
	       (biosadapter[V_ADP_SECONDARY].va_type == KD_VGA)) {
	adp = &biosadapter[V_ADP_SECONDARY];
    } else {
	adp = NULL;
    }
    bzero(mode_map, sizeof(mode_map));
    if (adp != NULL) {
	if (adp->va_type == KD_VGA) {
	    vga_save_state(adp, &adpstate, sizeof(adpstate));
#if defined(VGA_NO_BIOS) || defined(VGA_NO_MODE_CHANGE)
	    mode_map[adp->va_initial_mode] = adpstate.regs;
	    rows_offset = 1;
#else /* VGA_NO_BIOS || VGA_NO_MODE_CHANGE */
	    if (video_mode_ptr == NULL) {
		mode_map[adp->va_initial_mode] = adpstate.regs;
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
		    mode_map[adp->va_initial_mode] = adpstate.regs;
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
		    mode_map[adp->va_initial_mode] = adpstate.regs;
		    rows_offset = 1;
		    break;
		}
	    }
#endif /* VGA_NO_BIOS || VGA_NO_MODE_CHANGE */

#ifndef VGA_NO_MODE_CHANGE
	    adp->va_flags |= V_ADP_MODECHANGE;
#endif
#ifndef VGA_NO_FONT_LOADING
	    adp->va_flags |= V_ADP_FONT;
#endif
	} else if (adp->va_type == KD_EGA) {
#if defined(VGA_NO_BIOS) || defined(VGA_NO_MODE_CHANGE)
	    rows_offset = 1;
#else /* VGA_NO_BIOS || VGA_NO_MODE_CHANGE */
	    if (video_mode_ptr == NULL) {
		rows_offset = 1;
	    } else {
		map_mode_table(mode_map, video_mode_ptr, M_ENH_C80x25 + 1);
		/* XXX how can one validate the EGA table... */
		mp = get_mode_param(adp->va_initial_mode);
		if (mp != NULL) {
		    adp->va_flags |= V_ADP_MODECHANGE;
#ifndef VGA_NO_FONT_LOADING
		    adp->va_flags |= V_ADP_FONT;
#endif
		    rows_offset = 1;
		} else {
		    /*
		     * This is serious. We will not be able to switch video
		     * modes at all...
		     */
		    video_mode_ptr = NULL;
		    bzero(mode_map, sizeof(mode_map));
		    rows_offset = 1;
                }
	    }
#endif /* VGA_NO_BIOS || VGA_NO_MODE_CHANGE */
	}
    }

    /* remove conflicting modes if we have more than one adapter */
    if (biosadapters > 1) {
	for (i = 0; i < biosadapters; ++i) {
	    if (!(biosadapter[i].va_flags & V_ADP_MODECHANGE))
		continue;
	    clear_mode_map(&biosadapter[i], mode_map, M_VGA_CG320 + 1,
			   (biosadapter[i].va_flags & V_ADP_COLOR) ? 
			       V_INFO_COLOR : 0);
	    if ((biosadapter[i].va_type == KD_VGA)
		|| (biosadapter[i].va_type == KD_EGA)) {
		biosadapter[i].va_io_base =
		    (biosadapter[i].va_flags & V_ADP_COLOR) ?
			IO_VGA : IO_MDA;
		biosadapter[i].va_io_size = 32;
	    }
	}
    }

    /* buffer address */
    vga_get_info(&biosadapter[V_ADP_PRIMARY],
		 biosadapter[V_ADP_PRIMARY].va_initial_mode, &info);
    update_adapter_info(&biosadapter[V_ADP_PRIMARY], &info);

    if (biosadapters > 1) {
	vga_get_info(&biosadapter[V_ADP_SECONDARY],
		     biosadapter[V_ADP_SECONDARY].va_initial_mode, &info);
	update_adapter_info(&biosadapter[V_ADP_SECONDARY], &info);
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

    return biosadapters;
}

/* entry points */

static int
vga_nop(void)
{
    return 0;
}

static int
vga_probe(int unit, video_adapter_t **adpp, void *arg, int flags)
{
    probe_adapters();
    if (unit >= biosadapters)
	return ENXIO;

    *adpp = &biosadapter[unit];

    return 0;
}

static int
vga_init(int unit, video_adapter_t *adp, int flags)
{
    if ((unit >= biosadapters) || (adp == NULL) || !probe_done(adp))
	return ENXIO;

    if (!init_done(adp)) {
	/* nothing to do really... */
	adp->va_flags |= V_ADP_INITIALIZED;
    }

    if (!config_done(adp)) {
	if (vid_register(adp) < 0)
		return ENXIO;
	adp->va_flags |= V_ADP_REGISTERED;
    }
    if (vga_sub_configure != NULL)
	(*vga_sub_configure)(0);

    return 0;
}

/*
 * get_info():
 * Return the video_info structure of the requested video mode.
 *
 * all adapters
 */
static int
vga_get_info(video_adapter_t *adp, int mode, video_info_t *info)
{
    int i;

    if (!init_done)
	return 1;

    mode = map_gen_mode_num(adp->va_type, adp->va_flags & V_ADP_COLOR, mode);
#ifndef VGA_NO_MODE_CHANGE
    if (adp->va_flags & V_ADP_MODECHANGE) {
	/*
	 * If the parameter table entry for this mode is not found, 
	 * the mode is not supported...
	 */
	if (get_mode_param(mode) == NULL)
	    return 1;
    } else
#endif /* VGA_NO_MODE_CHANGE */
    {
	/* 
	 * Even if we don't support video mode switching on this adapter,
	 * the information on the initial (thus current) video mode 
	 * should be made available.
	 */
	if (mode != adp->va_initial_mode)
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

/*
 * query_mode():
 * Find a video mode matching the requested parameters.
 * Fields filled with 0 are considered "don't care" fields and
 * match any modes.
 *
 * all adapters
 */
static int
vga_query_mode(video_adapter_t *adp, video_info_t *info)
{
    video_info_t buf;
    int i;

    if (!init_done)
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
	if (vga_get_info(adp, bios_vmode[i].vi_mode, &buf))
		continue;
	return bios_vmode[i].vi_mode;
    }
    return -1;
}

/*
 * set_mode():
 * Change the video mode.
 *
 * EGA/VGA
 */
static int
vga_set_mode(video_adapter_t *adp, int mode)
{
#ifndef VGA_NO_MODE_CHANGE
    video_info_t info;
    adp_state_t params;

    prologue(adp, V_ADP_MODECHANGE, 1);

    mode = map_gen_mode_num(adp->va_type, 
			    adp->va_flags & V_ADP_COLOR, mode);
    if (vga_get_info(adp, mode, &info))
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
	vga_load_state(adp, &params);
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
	vga_load_state(adp, &params);
	break;

    default:
	return 1;
    }

    adp->va_mode = mode;
    update_adapter_info(adp, &info);

    /* move hardware cursor out of the way */
    (*vidsw[adp->va_index]->set_hw_cursor)(adp, -1, -1);

    return 0;
#else /* VGA_NO_MODE_CHANGE */
    return 1;
#endif /* VGA_NO_MODE_CHANGE */
}

#ifndef VGA_NO_FONT_LOADING

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

#if VGA_SLOW_IOACCESS
#ifdef VGA_ALT_SEQACCESS
    outb(TSIDX, 0x00); outb(TSREG, 0x01);
#endif
    outb(TSIDX, 0x02); outb(TSREG, 0x04);
    outb(TSIDX, 0x04); outb(TSREG, 0x07);
#ifdef VGA_ALT_SEQACCESS
    outb(TSIDX, 0x00); outb(TSREG, 0x03);
#endif
    outb(GDCIDX, 0x04); outb(GDCREG, 0x02);
    outb(GDCIDX, 0x05); outb(GDCREG, 0x00);
    outb(GDCIDX, 0x06); outb(GDCREG, 0x04);
#else /* VGA_SLOW_IOACCESS */
#ifdef VGA_ALT_SEQACCESS
    outw(TSIDX, 0x0100);
#endif
    outw(TSIDX, 0x0402);
    outw(TSIDX, 0x0704);
#ifdef VGA_ALT_SEQACCESS
    outw(TSIDX, 0x0300);
#endif
    outw(GDCIDX, 0x0204);
    outw(GDCIDX, 0x0005);
    outw(GDCIDX, 0x0406);               /* addr = a0000, 64kb */
#endif /* VGA_SLOW_IOACCESS */

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

#if VGA_SLOW_IOACCESS
#ifdef VGA_ALT_SEQACCESS
    outb(TSIDX, 0x00); outb(TSREG, 0x01);
#endif
    outb(TSIDX, 0x02); outb(TSREG, buf[0]);
    outb(TSIDX, 0x04); outb(TSREG, buf[1]);
#ifdef VGA_ALT_SEQACCESS
    outb(TSIDX, 0x00); outb(TSREG, 0x03);
#endif
    outb(GDCIDX, 0x04); outb(GDCREG, buf[2]);
    outb(GDCIDX, 0x05); outb(GDCREG, buf[3]);
    if (adp->va_crtc_addr == MONO_CRTC) {
	outb(GDCIDX, 0x06); outb(GDCREG,(buf[4] & 0x03) | 0x08);
    } else {
	outb(GDCIDX, 0x06); outb(GDCREG,(buf[4] & 0x03) | 0x0c);
    }
#else /* VGA_SLOW_IOACCESS */
#ifdef VGA_ALT_SEQACCESS
    outw(TSIDX, 0x0100);
#endif
    outw(TSIDX, 0x0002 | (buf[0] << 8));
    outw(TSIDX, 0x0004 | (buf[1] << 8));
#ifdef VGA_ALT_SEQACCESS
    outw(TSIDX, 0x0300);
#endif
    outw(GDCIDX, 0x0004 | (buf[2] << 8));
    outw(GDCIDX, 0x0005 | (buf[3] << 8));
    if (adp->va_crtc_addr == MONO_CRTC)
        outw(GDCIDX, 0x0006 | (((buf[4] & 0x03) | 0x08)<<8));
    else
        outw(GDCIDX, 0x0006 | (((buf[4] & 0x03) | 0x0c)<<8));
#endif /* VGA_SLOW_IOACCESS */

    splx(s);
}

#endif /* VGA_NO_FONT_LOADING */

/*
 * save_font():
 * Read the font data in the requested font page from the video adapter.
 *
 * EGA/VGA
 */
static int
vga_save_font(video_adapter_t *adp, int page, int fontsize, u_char *data,
	      int ch, int count)
{
#ifndef VGA_NO_FONT_LOADING
    u_char buf[PARAM_BUFSIZE];
    u_int32_t segment;
    int c;
#ifdef VGA_ALT_SEQACCESS
    int s;
    u_char val = 0;
#endif

    prologue(adp, V_ADP_FONT, 1);

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
    segment = FONT_BUF + 0x4000*page;
    if (page > 3)
	segment -= 0xe000;

#ifdef VGA_ALT_SEQACCESS
    if (adp->va_type == KD_VGA) {	/* what about EGA? XXX */
	s = splhigh();
	outb(TSIDX, 0x00); outb(TSREG, 0x01);
	outb(TSIDX, 0x01); val = inb(TSREG);	/* disable screen */
	outb(TSIDX, 0x01); outb(TSREG, val | 0x20);
	outb(TSIDX, 0x00); outb(TSREG, 0x03);
	splx(s);
    }
#endif

    set_font_mode(adp, buf);
    if (fontsize == 32) {
	bcopy_fromio((void *)(segment + ch*32), data, fontsize*count);
    } else {
	for (c = ch; count > 0; ++c, --count) {
	    bcopy_fromio((void *)(segment + c*32), data, fontsize);
	    data += fontsize;
	}
    }
    set_normal_mode(adp, buf);

#ifdef VGA_ALT_SEQACCESS
    if (adp->va_type == KD_VGA) {
	s = splhigh();
	outb(TSIDX, 0x00); outb(TSREG, 0x01);
	outb(TSIDX, 0x01); outb(TSREG, val & 0xdf);	/* enable screen */
	outb(TSIDX, 0x00); outb(TSREG, 0x03);
	splx(s);
    }
#endif

    return 0;
#else /* VGA_NO_FONT_LOADING */
    return 1;
#endif /* VGA_NO_FONT_LOADING */
}

/*
 * load_font():
 * Set the font data in the requested font page.
 * NOTE: it appears that some recent video adapters do not support
 * the font page other than 0... XXX
 *
 * EGA/VGA
 */
static int
vga_load_font(video_adapter_t *adp, int page, int fontsize, u_char *data,
	      int ch, int count)
{
#ifndef VGA_NO_FONT_LOADING
    u_char buf[PARAM_BUFSIZE];
    u_int32_t segment;
    int c;
#ifdef VGA_ALT_SEQACCESS
    int s;
    u_char val = 0;
#endif

    prologue(adp, V_ADP_FONT, 1);

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
    segment = FONT_BUF + 0x4000*page;
    if (page > 3)
	segment -= 0xe000;

#ifdef VGA_ALT_SEQACCESS
    if (adp->va_type == KD_VGA) {	/* what about EGA? XXX */
	s = splhigh();
	outb(TSIDX, 0x00); outb(TSREG, 0x01);
	outb(TSIDX, 0x01); val = inb(TSREG);	/* disable screen */
	outb(TSIDX, 0x01); outb(TSREG, val | 0x20);
	outb(TSIDX, 0x00); outb(TSREG, 0x03);
	splx(s);
    }
#endif

    set_font_mode(adp, buf);
    if (fontsize == 32) {
	bcopy_toio(data, (void *)(segment + ch*32), fontsize*count);
    } else {
	for (c = ch; count > 0; ++c, --count) {
	    bcopy_toio(data, (void *)(segment + c*32), fontsize);
	    data += fontsize;
	}
    }
    set_normal_mode(adp, buf);

#ifdef VGA_ALT_SEQACCESS
    if (adp->va_type == KD_VGA) {
	s = splhigh();
	outb(TSIDX, 0x00); outb(TSREG, 0x01);
	outb(TSIDX, 0x01); outb(TSREG, val & 0xdf);	/* enable screen */
	outb(TSIDX, 0x00); outb(TSREG, 0x03);
	splx(s);
    }
#endif

    return 0;
#else /* VGA_NO_FONT_LOADING */
    return 1;
#endif /* VGA_NO_FONT_LOADING */
}

/*
 * show_font():
 * Activate the requested font page.
 * NOTE: it appears that some recent video adapters do not support
 * the font page other than 0... XXX
 *
 * EGA/VGA
 */
static int
vga_show_font(video_adapter_t *adp, int page)
{
#ifndef VGA_NO_FONT_LOADING
    static u_char cg[] = { 0x00, 0x05, 0x0a, 0x0f, 0x30, 0x35, 0x3a, 0x3f };
    int s;

    prologue(adp, V_ADP_FONT, 1);
    if (page < 0 || page >= 8)
	return 1;

    s = splhigh();
    outb(TSIDX, 0x03); outb(TSREG, cg[page]);
    splx(s);

    return 0;
#else /* VGA_NO_FONT_LOADING */
    return 1;
#endif /* VGA_NO_FONT_LOADING */
}

/*
 * save_palette():
 * Read DAC values. The values have expressed in 8 bits.
 *
 * VGA
 */
static int
vga_save_palette(video_adapter_t *adp, u_char *palette)
{
    int i;

    prologue(adp, V_ADP_PALETTE, 1);

    /* 
     * We store 8 bit values in the palette buffer, while the standard
     * VGA has 6 bit DAC .
     */
    outb(PALRADR, 0x00);
    for (i = 0; i < 256*3; ++i)
	palette[i] = inb(PALDATA) << 2; 
    inb(adp->va_crtc_addr + 6);	/* reset flip/flop */
    return 0;
}

/*
 * load_palette():
 * Set DAC values.
 *
 * VGA
 */
static int
vga_load_palette(video_adapter_t *adp, u_char *palette)
{
    int i;

    prologue(adp, V_ADP_PALETTE, 1);

    outb(PIXMASK, 0xff);		/* no pixelmask */
    outb(PALWADR, 0x00);
    for (i = 0; i < 256*3; ++i)
	outb(PALDATA, palette[i] >> 2);
    inb(adp->va_crtc_addr + 6);	/* reset flip/flop */
    outb(ATC, 0x20);			/* enable palette */
    return 0;
}

/*
 * set_border():
 * Change the border color.
 *
 * CGA/EGA/VGA
 */
static int
vga_set_border(video_adapter_t *adp, int color)
{
    prologue(adp, V_ADP_BORDER, 1);

    switch (adp->va_type) {
    case KD_EGA:
    case KD_VGA:    
	inb(adp->va_crtc_addr + 6);	/* reset flip-flop */
	outb(ATC, 0x31); outb(ATC, color & 0xff); 
	break;  
    case KD_CGA:    
	outb(adp->va_crtc_addr + 5, color & 0x0f); /* color select register */
	break;  
    case KD_MONO:   
    case KD_HERCULES:
    default:
	break;  
    }
    return 0;
}

/*
 * save_state():
 * Read video register values.
 * NOTE: this function only reads the standard EGA/VGA registers.
 * any extra/extended registers of SVGA adapters are not saved.
 *
 * VGA
 */
static int
vga_save_state(video_adapter_t *adp, void *p, size_t size)
{
    video_info_t info;
    u_char *buf;
    int crtc_addr;
    int i, j;
    int s;

    if (size == 0) {
	/* return the required buffer size */
	prologue(adp, V_ADP_STATESAVE, 0);
	return sizeof(adp_state_t);
    } else {
	prologue(adp, V_ADP_STATESAVE, 1);
	if (size < sizeof(adp_state_t))
	    return 1;
    }

    ((adp_state_t *)p)->sig = V_STATE_SIG;
    buf = ((adp_state_t *)p)->regs;
    bzero(buf, V_MODE_PARAM_SIZE);
    crtc_addr = adp->va_crtc_addr;

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
    if (vga_get_info(adp, adp->va_mode, &info) == 0) {
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
	printf("vga%d: %s: failed to obtain mode info. (vga_save_state())\n",
	       adp->va_unit, adp->va_name);
    }
#else
    buf[0] = readb(BIOS_PADDRTOVADDR(0x44a));	/* COLS */
    buf[1] = readb(BIOS_PADDRTOVADDR(0x484));	/* ROWS */
    buf[2] = readb(BIOS_PADDRTOVADDR(0x485));	/* POINTS */
    buf[3] = readb(BIOS_PADDRTOVADDR(0x44c));
    buf[4] = readb(BIOS_PADDRTOVADDR(0x44d));
#endif

    return 0;
}

/*
 * load_state():
 * Set video registers at once.
 * NOTE: this function only updates the standard EGA/VGA registers.
 * any extra/extended registers of SVGA adapters are not changed.
 *
 * EGA/VGA
 */
static int
vga_load_state(video_adapter_t *adp, void *p)
{
    u_char *buf;
    int crtc_addr;
    int s;
    int i;

    prologue(adp, V_ADP_STATELOAD, 1);
    if (((adp_state_t *)p)->sig != V_STATE_SIG)
	return 1;

    buf = ((adp_state_t *)p)->regs;
    crtc_addr = adp->va_crtc_addr;

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

#if notyet /* a temporary workaround for kernel panic, XXX */
#ifndef VGA_NO_BIOS
    if (adp->va_unit == V_ADP_PRIMARY) {
	writeb(BIOS_PADDRTOVADDR(0x44a), buf[0]);	/* COLS */
	writeb(BIOS_PADDRTOVADDR(0x484), buf[1] + rows_offset - 1); /* ROWS */
	writeb(BIOS_PADDRTOVADDR(0x485), buf[2]);	/* POINTS */
#if 0
	writeb(BIOS_PADDRTOVADDR(0x44c), buf[3]);
	writeb(BIOS_PADDRTOVADDR(0x44d), buf[4]);
#endif
    }
#endif /* VGA_NO_BIOS */
#endif /* notyet */

    splx(s);
    return 0;
}

/*
 * set_origin():
 * Change the origin (window mapping) of the banked frame buffer.
 */
static int
vga_set_origin(video_adapter_t *adp, off_t offset)
{
    /* 
     * The standard video modes do not require window mapping; 
     * always return error.
     */
    return 1;
}

/*
 * read_hw_cursor():
 * Read the position of the hardware text cursor.
 *
 * all adapters
 */
static int
vga_read_hw_cursor(video_adapter_t *adp, int *col, int *row)
{
    u_int16_t off;
    int s;

    if (!init_done)
	return 1;

    if (adp->va_info.vi_flags & V_INFO_GRAPHICS)
	return 1;

    s = spltty();
    outb(adp->va_crtc_addr, 14);
    off = inb(adp->va_crtc_addr + 1);
    outb(adp->va_crtc_addr, 15);
    off = (off << 8) | inb(adp->va_crtc_addr + 1);
    splx(s);

    *row = off / adp->va_info.vi_width;
    *col = off % adp->va_info.vi_width;

    return 0;
}

/*
 * set_hw_cursor():
 * Move the hardware text cursor.  If col and row are both -1, 
 * the cursor won't be shown.
 *
 * all adapters
 */
static int
vga_set_hw_cursor(video_adapter_t *adp, int col, int row)
{
    u_int16_t off;
    int s;

    if (!init_done)
	return 1;

    if ((col == -1) && (row == -1)) {
	off = -1;
    } else {
	if (adp->va_info.vi_flags & V_INFO_GRAPHICS)
	    return 1;
	off = row*adp->va_info.vi_width + col;
    }

    s = spltty();
    outb(adp->va_crtc_addr, 14);
    outb(adp->va_crtc_addr + 1, off >> 8);
    outb(adp->va_crtc_addr, 15);
    outb(adp->va_crtc_addr + 1, off & 0x00ff);
    splx(s);

    return 0;
}

/*
 * set_hw_cursor_shape():
 * Change the shape of the hardware text cursor. If the height is
 * zero or negative, the cursor won't be shown.
 *
 * all adapters
 */
static int
vga_set_hw_cursor_shape(video_adapter_t *adp, int base, int height,
			int celsize, int blink)
{
    int s;

    if (!init_done)
	return 1;

    s = spltty();
    switch (adp->va_type) {
    case KD_VGA:
    case KD_CGA:
    case KD_MONO:
    case KD_HERCULES:
    default:
	if (height <= 0) {
	    /* make the cursor invisible */
	    outb(adp->va_crtc_addr, 10);
	    outb(adp->va_crtc_addr + 1, 32);
	    outb(adp->va_crtc_addr, 11);
	    outb(adp->va_crtc_addr + 1, 0);
	} else {
	    outb(adp->va_crtc_addr, 10);
	    outb(adp->va_crtc_addr + 1, celsize - base - height);
	    outb(adp->va_crtc_addr, 11);
	    outb(adp->va_crtc_addr + 1, celsize - base - 1);
	}
	break;
    case KD_EGA:
	if (height <= 0) {
	    /* make the cursor invisible */
	    outb(adp->va_crtc_addr, 10);
	    outb(adp->va_crtc_addr + 1, celsize);
	    outb(adp->va_crtc_addr, 11);
	    outb(adp->va_crtc_addr + 1, 0);
	} else {
	    outb(adp->va_crtc_addr, 10);
	    outb(adp->va_crtc_addr + 1, celsize - base - height);
	    outb(adp->va_crtc_addr, 11);
	    outb(adp->va_crtc_addr + 1, celsize - base);
	}
	break;
    }
    splx(s);

    return 0;
}

/*
 * mmap():
 * Mmap frame buffer.
 *
 * all adapters
 */
static int
vga_mmap(video_adapter_t *adp, vm_offset_t offset)
{
    if (offset > 0x20000 - PAGE_SIZE)
	return -1;
#ifdef __i386__
    return i386_btop((VIDEO_BUF_BASE + offset));
#endif
#ifdef __alpha__
    return alpha_btop((VIDEO_BUF_BASE + offset));
#endif
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

/*
 * diag():
 * Print some information about the video adapter and video modes,
 * with requested level of details.
 *
 * all adapters
 */
static int
vga_diag(video_adapter_t *adp, int level)
{
#if FB_DEBUG > 1
    video_info_t info;
#endif
    u_char *mp;

    if (!init_done)
	return 1;

#if FB_DEBUG > 1
#ifndef VGA_NO_BIOS
    printf("vga: RTC equip. code:0x%02x, DCC code:0x%02x\n",
	   rtcin(RTC_EQUIPMENT), readb(BIOS_PADDRTOVADDR(0x488)));
    printf("vga: CRTC:0x%x, video option:0x%02x, ",
	   readw(BIOS_PADDRTOVADDR(0x463)),
	   readb(BIOS_PADDRTOVADDR(0x487)));
    printf("rows:%d, cols:%d, font height:%d\n",
	   readb(BIOS_PADDRTOVADDR(0x44a)),
	   readb(BIOS_PADDRTOVADDR(0x484)) + 1,
	   readb(BIOS_PADDRTOVADDR(0x485)));
#endif /* VGA_NO_BIOS */
    printf("vga: param table EGA/VGA:%p", video_mode_ptr);
    printf(", CGA/MDA:%p\n", video_mode_ptr2);
    printf("vga: rows_offset:%d\n", rows_offset);
#endif /* FB_DEBUG > 1 */

    fb_dump_adp_info(DRIVER_NAME, adp, level);

#if FB_DEBUG > 1
    if (adp->va_flags & V_ADP_MODECHANGE) {
	for (i = 0; bios_vmode[i].vi_mode != EOT; ++i) {
	    if (bios_vmode[i].vi_mode == NA)
		continue;
	    if (get_mode_param(bios_vmode[i].vi_mode) == NULL)
		continue;
	    fb_dump_mode_info(DRIVER_NAME, adp, &bios_vmode[i], level);
	}
    } else {
	vga_get_info(adp, adp->va_initial_mode, &info);	/* shouldn't fail */
	fb_dump_mode_info(DRIVER_NAME, adp, &info, level);
    }
#endif /* FB_DEBUG > 1 */

    if ((adp->va_type != KD_EGA) && (adp->va_type != KD_VGA))
	return 0;
#if !defined(VGA_NO_BIOS) && !defined(VGA_NO_MODE_CHANGE)
    if (video_mode_ptr == NULL)
	printf("vga%d: %s: WARNING: video mode switching is not "
	       "fully supported on this adapter\n",
	       adp->va_unit, adp->va_name);
#endif
    if (level <= 0)
	return 0;

    if (adp->va_type == KD_VGA) {
	printf("VGA parameters upon power-up\n");
	dump_buffer(adpstate.regs, sizeof(adpstate.regs));
	printf("VGA parameters in BIOS for mode %d\n", adp->va_initial_mode);
	dump_buffer(adpstate2.regs, sizeof(adpstate2.regs));
    }

    mp = get_mode_param(adp->va_initial_mode);
    if (mp == NULL)	/* this shouldn't be happening */
	return 0;
    printf("EGA/VGA parameters to be used for mode %d\n", adp->va_initial_mode);
    dump_buffer(mp, V_MODE_PARAM_SIZE);

    return 0;
}

#endif /* NVGA > 0 */
