/*-
 * Copyright (c) 1999 FreeBSD(98) port team.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "opt_gdc.h"
#include "opt_fb.h"
#include "opt_syscons.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <sys/fbio.h>
#include <sys/fcntl.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>

#include <machine/md_var.h>
#include <machine/pc/bios.h>

#include <dev/fb/fbreg.h>

#ifdef LINE30
#include <pc98/pc98/pc98.h>
#endif
#include <pc98/pc98/pc98_machdep.h>
#include <isa/isavar.h>

#define	TEXT_GDC	0x60
#define	GRAPHIC_GDC	0xa0
#define	ROW		25
#define	COL		80

#define DRIVER_NAME	"gdc"

/* cdev driver declaration */

#define GDC_UNIT(dev)	minor(dev)
#define GDC_MKMINOR(unit) (unit)

typedef struct gdc_softc {
	video_adapter_t	*adp;
	struct resource *res_tgdc, *res_ggdc;
	struct resource *res_egc, *res_pegc, *res_grcg, *res_kcg;
	struct resource *res_tmem, *res_gmem1, *res_gmem2;
#ifdef FB_INSTALL_CDEV
	genfb_softc_t gensc;
#endif
} gdc_softc_t;

#define GDC_SOFTC(unit)	\
	((gdc_softc_t *)devclass_get_softc(gdc_devclass, unit))

static bus_addr_t	gdc_iat[] = {0, 2, 4, 6, 8, 10, 12, 14};

static devclass_t	gdc_devclass;

static int		gdc_probe_unit(int unit, gdc_softc_t *sc, int flags);
static int		gdc_attach_unit(int unit, gdc_softc_t *sc, int flags);
static int		gdc_alloc_resource(device_t dev);
static int		gdc_release_resource(device_t dev);

#if FB_INSTALL_CDEV

static d_open_t		gdcopen;
static d_close_t	gdcclose;
static d_read_t		gdcread;
static d_write_t	gdcwrite;
static d_ioctl_t	gdcioctl;
static d_mmap_t		gdcmmap;

static struct cdevsw gdc_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	gdcopen,
	.d_close =	gdcclose,
	.d_read =	gdcread,
	.d_write =	gdcwrite,
	.d_ioctl =	gdcioctl,
	.d_mmap =	gdcmmap,
	.d_name =	DRIVER_NAME,
	.d_maj =	-1,
};

#endif /* FB_INSTALL_CDEV */

static void
gdc_identify(driver_t *driver, device_t parent)
{
	BUS_ADD_CHILD(parent, ISA_ORDER_SPECULATIVE, DRIVER_NAME, 0);
}

static int
gdcprobe(device_t dev)
{
	int error;

	/* Check isapnp ids */
	if (isa_get_vendorid(dev))
		return (ENXIO);

	device_set_desc(dev, "Generic GDC");

	error = gdc_alloc_resource(dev);
	if (error)
		return (error);

	error = gdc_probe_unit(device_get_unit(dev),
			       device_get_softc(dev),
			       device_get_flags(dev));

	gdc_release_resource(dev);

	return (error);
}

static int
gdc_attach(device_t dev)
{
	gdc_softc_t *sc;
	int error;

	error = gdc_alloc_resource(dev);
	if (error)
		return (error);

	sc = device_get_softc(dev);
	error = gdc_attach_unit(device_get_unit(dev),
				sc,
				device_get_flags(dev));
	if (error) {
		gdc_release_resource(dev);
		return error;
	}

#ifdef FB_INSTALL_CDEV
	/* attach a virtual frame buffer device */
	error = fb_attach(GDC_MKMINOR(device_get_unit(dev)),
				  sc->adp, &gdc_cdevsw);
	if (error) {
		gdc_release_resource(dev);
		return error;
	}
#endif /* FB_INSTALL_CDEV */

	if (bootverbose)
		(*vidsw[sc->adp->va_index]->diag)(sc->adp, bootverbose);

	return 0;
}

static int
gdc_probe_unit(int unit, gdc_softc_t *sc, int flags)
{
	video_switch_t *sw;

	sw = vid_get_switch(DRIVER_NAME);
	if (sw == NULL)
		return ENXIO;
	return (*sw->probe)(unit, &sc->adp, NULL, flags);
}

static int
gdc_attach_unit(int unit, gdc_softc_t *sc, int flags)
{
	video_switch_t *sw;

	sw = vid_get_switch(DRIVER_NAME);
	if (sw == NULL)
		return ENXIO;
	return (*sw->init)(unit, sc->adp, flags);
}


static int
gdc_alloc_resource(device_t dev)
{
	int rid;
	gdc_softc_t *sc;

	sc = device_get_softc(dev);

	/* TEXT GDC */
	rid = 0;
	bus_set_resource(dev, SYS_RES_IOPORT, rid, TEXT_GDC, 1);
	sc->res_tgdc = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid,
					   gdc_iat, 8, RF_ACTIVE);
	if (sc->res_tgdc == NULL) {
		gdc_release_resource(dev);
		return (ENXIO);
	}
	isa_load_resourcev(sc->res_tgdc, gdc_iat, 8);

	/* GRAPHIC GDC */
	rid = 8;
	bus_set_resource(dev, SYS_RES_IOPORT, rid, GRAPHIC_GDC, 1);
	sc->res_ggdc = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid,
					   gdc_iat, 8, RF_ACTIVE);
	if (sc->res_ggdc == NULL) {
		gdc_release_resource(dev);
		return (ENXIO);
	}
	isa_load_resourcev(sc->res_ggdc, gdc_iat, 8);

	/* EGC */
	rid = 16;
	bus_set_resource(dev, SYS_RES_IOPORT, rid, 0x4a0, 1);
	sc->res_egc = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid,
					   gdc_iat, 8, RF_ACTIVE);
	if (sc->res_egc == NULL) {
		gdc_release_resource(dev);
		return (ENXIO);
	}
	isa_load_resourcev(sc->res_egc, gdc_iat, 8);

	/* PEGC */
	rid = 24;
	bus_set_resource(dev, SYS_RES_IOPORT, rid, 0x9a0, 1);
	sc->res_pegc = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid,
					   gdc_iat, 8, RF_ACTIVE);
	if (sc->res_pegc == NULL) {
		gdc_release_resource(dev);
		return (ENXIO);
	}
	isa_load_resourcev(sc->res_pegc, gdc_iat, 8);

	/* CRTC/GRCG */
	rid = 32;
	bus_set_resource(dev, SYS_RES_IOPORT, rid, 0x70, 1);
	sc->res_grcg = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid,
					   gdc_iat, 8, RF_ACTIVE);
	if (sc->res_grcg == NULL) {
		gdc_release_resource(dev);
		return (ENXIO);
	}
	isa_load_resourcev(sc->res_grcg, gdc_iat, 8);

	/* KCG */
	rid = 40;
	bus_set_resource(dev, SYS_RES_IOPORT, rid, 0xa1, 1);
	sc->res_kcg = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid,
					  gdc_iat, 8, RF_ACTIVE);
	if (sc->res_kcg == NULL) {
		gdc_release_resource(dev);
		return (ENXIO);
	}
	isa_load_resourcev(sc->res_kcg, gdc_iat, 8);


	/* TEXT Memory */
	rid = 0;
	sc->res_tmem = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
					  0xa0000, 0xa4fff, 0x5000, RF_ACTIVE);
	if (sc->res_tmem == NULL) {
		gdc_release_resource(dev);
		return (ENXIO);
	}

	/* GRAPHIC Memory */
	rid = 1;
	sc->res_gmem1 = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
					   0xa8000, 0xbffff, 0x18000,
					   RF_ACTIVE);
	if (sc->res_gmem1 == NULL) {
		gdc_release_resource(dev);
		return (ENXIO);
	}
	rid = 2;
	sc->res_gmem2 = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
					   0xe0000, 0xe7fff, 0x8000,
					   RF_ACTIVE);
	if (sc->res_gmem2 == NULL) {
		gdc_release_resource(dev);
		return (ENXIO);
	}

	return (0);
}

static int
gdc_release_resource(device_t dev)
{
	gdc_softc_t *sc;

	sc = device_get_softc(dev);

	if (sc->res_tgdc)
		bus_release_resource(dev, SYS_RES_IOPORT,  0, sc->res_tgdc);
	if (sc->res_ggdc)
		bus_release_resource(dev, SYS_RES_IOPORT,  8, sc->res_ggdc);
	if (sc->res_egc)
		bus_release_resource(dev, SYS_RES_IOPORT, 16, sc->res_egc);
	if (sc->res_pegc)
		bus_release_resource(dev, SYS_RES_IOPORT, 24, sc->res_pegc);
	if (sc->res_grcg)
		bus_release_resource(dev, SYS_RES_IOPORT, 32, sc->res_grcg);
	if (sc->res_kcg)
		bus_release_resource(dev, SYS_RES_IOPORT, 40, sc->res_kcg);

	if (sc->res_tmem)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->res_tmem);
	if (sc->res_gmem1)
		bus_release_resource(dev, SYS_RES_MEMORY, 1, sc->res_gmem1);
	if (sc->res_gmem2)
		bus_release_resource(dev, SYS_RES_MEMORY, 2, sc->res_gmem2);

	return (0);
}

/* cdev driver functions */

#ifdef FB_INSTALL_CDEV

static int
gdcopen(struct cdev *dev, int flag, int mode, struct thread *td)
{
    gdc_softc_t *sc;

    sc = GDC_SOFTC(GDC_UNIT(dev));
    if (sc == NULL)
	return ENXIO;
    if (mode & (O_CREAT | O_APPEND | O_TRUNC))
	return ENODEV;

    return genfbopen(&sc->gensc, sc->adp, flag, mode, td);
}

static int
gdcclose(struct cdev *dev, int flag, int mode, struct thread *td)
{
    gdc_softc_t *sc;

    sc = GDC_SOFTC(GDC_UNIT(dev));
    return genfbclose(&sc->gensc, sc->adp, flag, mode, td);
}

static int
gdcread(struct cdev *dev, struct uio *uio, int flag)
{
    gdc_softc_t *sc;

    sc = GDC_SOFTC(GDC_UNIT(dev));
    return genfbread(&sc->gensc, sc->adp, uio, flag);
}

static int
gdcwrite(struct cdev *dev, struct uio *uio, int flag)
{
    gdc_softc_t *sc;

    sc = GDC_SOFTC(GDC_UNIT(dev));
    return genfbread(&sc->gensc, sc->adp, uio, flag);
}

static int
gdcioctl(struct cdev *dev, u_long cmd, caddr_t arg, int flag, struct thread *td)
{
    gdc_softc_t *sc;

    sc = GDC_SOFTC(GDC_UNIT(dev));
    return genfbioctl(&sc->gensc, sc->adp, cmd, arg, flag, td);
}

static int
gdcmmap(struct cdev *dev, vm_offset_t offset, vm_paddr_t *paddr, int prot)
{
    gdc_softc_t *sc;

    sc = GDC_SOFTC(GDC_UNIT(dev));
    return genfbmmap(&sc->gensc, sc->adp, offset, paddr, prot);
}

#endif /* FB_INSTALL_CDEV */

static device_method_t gdc_methods[] = {
	DEVMETHOD(device_identify,	gdc_identify),
	DEVMETHOD(device_probe,		gdcprobe),
	DEVMETHOD(device_attach,	gdc_attach),
	{ 0, 0 }
};

static driver_t gdcdriver = {
	DRIVER_NAME,
	gdc_methods,
	sizeof(gdc_softc_t),
};

DRIVER_MODULE(gdc, isa, gdcdriver, gdc_devclass, 0, 0);

/* LOW-LEVEL */


#include <pc98/pc98/30line.h>

#define TEXT_BUF_BASE		0x000a0000
#define TEXT_BUF_SIZE		0x00008000
#define GRAPHICS_BUF_BASE	0x000a8000
#define GRAPHICS_BUF_SIZE	0x00040000
#define VIDEO_BUF_BASE		0x000a0000
#define VIDEO_BUF_SIZE		0x00048000

#define probe_done(adp)		((adp)->va_flags & V_ADP_PROBED)
#define init_done(adp)		((adp)->va_flags & V_ADP_INITIALIZED)
#define config_done(adp)	((adp)->va_flags & V_ADP_REGISTERED)

/* 
 * NOTE: `va_window' should have a virtual address, but is initialized
 * with a physical address in the following table, they will be
 * converted at run-time.
 */
static video_adapter_t adapter_init_value[] = {
    { 0,
      KD_PC98, "gdc",			/* va_type, va_name */
      0, 0, 				/* va_unit, va_minor */
      V_ADP_COLOR | V_ADP_MODECHANGE | V_ADP_BORDER, 
      TEXT_GDC, 16, TEXT_GDC,		/* va_io*, XXX */
      VIDEO_BUF_BASE, VIDEO_BUF_SIZE,	/* va_mem* */
      TEXT_BUF_BASE, TEXT_BUF_SIZE, TEXT_BUF_SIZE, 0, /* va_window* */
      0, 0, 				/* va_buffer, va_buffer_size */
      0, M_PC98_80x25, 0, 		/* va_*mode* */
    },
};

static video_adapter_t	biosadapter[1];

/* video driver declarations */
static int			gdc_configure(int flags);
static int			gdc_err(video_adapter_t *adp, ...);
static vi_probe_t		gdc_probe;
static vi_init_t		gdc_init;
static vi_get_info_t		gdc_get_info;
static vi_query_mode_t		gdc_query_mode;
static vi_set_mode_t		gdc_set_mode;
static vi_set_border_t		gdc_set_border;
static vi_save_state_t		gdc_save_state;
static vi_load_state_t		gdc_load_state;
static vi_read_hw_cursor_t	gdc_read_hw_cursor;
static vi_set_hw_cursor_t	gdc_set_hw_cursor;
static vi_set_hw_cursor_shape_t	gdc_set_hw_cursor_shape;
static vi_blank_display_t	gdc_blank_display;
static vi_mmap_t		gdc_mmap_buf;
static vi_ioctl_t		gdc_dev_ioctl;
static vi_clear_t		gdc_clear;
static vi_fill_rect_t		gdc_fill_rect;
static vi_bitblt_t		gdc_bitblt;
static vi_diag_t		gdc_diag;
static vi_save_palette_t	gdc_save_palette;
static vi_load_palette_t	gdc_load_palette;
static vi_set_win_org_t		gdc_set_origin;

static video_switch_t gdcvidsw = {
	gdc_probe,
	gdc_init,
	gdc_get_info,
	gdc_query_mode,	
	gdc_set_mode,
	(vi_save_font_t *)gdc_err,
	(vi_load_font_t *)gdc_err,
	(vi_show_font_t *)gdc_err,
	gdc_save_palette,
	gdc_load_palette,
	gdc_set_border,
	gdc_save_state,
	gdc_load_state,
	gdc_set_origin,
	gdc_read_hw_cursor,
	gdc_set_hw_cursor,
	gdc_set_hw_cursor_shape,
	gdc_blank_display,
	gdc_mmap_buf,
	gdc_dev_ioctl,
	gdc_clear,
	gdc_fill_rect,
	gdc_bitblt,
	(int (*)(void))gdc_err,
	(int (*)(void))gdc_err,
	gdc_diag,
};

VIDEO_DRIVER(gdc, gdcvidsw, gdc_configure);

/* GDC BIOS standard video modes */
#define EOT		(-1)
#define NA		(-2)

static video_info_t bios_vmode[] = {
    { M_PC98_80x25, V_INFO_COLOR, 80, 25, 8, 16, 4, 1,
      TEXT_BUF_BASE, TEXT_BUF_SIZE, TEXT_BUF_SIZE, 0, 0, V_INFO_MM_TEXT },
#ifdef LINE30
    { M_PC98_80x30, V_INFO_COLOR, 80, 30, 8, 16, 4, 1,
      TEXT_BUF_BASE, TEXT_BUF_SIZE, TEXT_BUF_SIZE, 0, 0, V_INFO_MM_TEXT },
#endif
#ifndef GDC_NOGRAPHICS
    { M_PC98_EGC640x400, V_INFO_COLOR | V_INFO_GRAPHICS,
      640, 400, 8, 16, 4, 4,
      GRAPHICS_BUF_BASE, GRAPHICS_BUF_SIZE, GRAPHICS_BUF_SIZE, 0, 0,
      V_INFO_MM_PLANAR },
    { M_PC98_PEGC640x400, V_INFO_COLOR | V_INFO_GRAPHICS | V_INFO_VESA,
      640, 400, 8, 16, 8, 1,
      GRAPHICS_BUF_BASE, 0x00008000, 0x00008000, 0, 0,
      V_INFO_MM_PACKED, 1 },
#ifdef LINE30
    { M_PC98_PEGC640x480, V_INFO_COLOR | V_INFO_GRAPHICS | V_INFO_VESA,
      640, 480, 8, 16, 8, 1,
      GRAPHICS_BUF_BASE, 0x00008000, 0x00008000, 0, 0,
      V_INFO_MM_PACKED, 1 },
#endif
#endif
    { EOT },
};

static int		gdc_init_done = FALSE;

/* local functions */
static int map_gen_mode_num(int type, int color, int mode);
static int probe_adapters(void);

#define	prologue(adp, flag, err)			\
	if (!gdc_init_done || !((adp)->va_flags & (flag)))	\
	    return (err)

/* a backdoor for the console driver */
static int
gdc_configure(int flags)
{
    probe_adapters();
    biosadapter[0].va_flags |= V_ADP_INITIALIZED;
    if (!config_done(&biosadapter[0])) {
	if (vid_register(&biosadapter[0]) < 0)
	    return 1;
	biosadapter[0].va_flags |= V_ADP_REGISTERED;
    }

    return 1;
}

/* local subroutines */

/* map a generic video mode to a known mode number */
static int
map_gen_mode_num(int type, int color, int mode)
{
    static struct {
	int from;
	int to;
    } mode_map[] = {
	{ M_TEXT_80x25,	M_PC98_80x25, },
#ifdef LINE30
	{ M_TEXT_80x30,	M_PC98_80x30, },
#endif
    };
    int i;

    for (i = 0; i < sizeof(mode_map)/sizeof(mode_map[0]); ++i) {
        if (mode_map[i].from == mode)
            return mode_map[i].to;
    }
    return mode;
}

static int
verify_adapter(video_adapter_t *adp)
{
#ifndef GDC_NOGRAPHICS
    int i;

    if (PC98_SYSTEM_PARAMETER(0x45c) & 0x40) {		/* PEGC exists */
	adp->va_flags |= V_ADP_VESA;			/* XXX */
    } else {
	for (i = 0; bios_vmode[i].vi_mode != EOT; ++i) {
	    if (bios_vmode[i].vi_flags & V_INFO_VESA)
		bios_vmode[i].vi_mode = NA;
	}
    }
#endif
    return 0;
}

/* probe video adapters and return the number of detected adapters */
static int
probe_adapters(void)
{
    video_info_t info;

    /* do this test only once */
    if (gdc_init_done)
	return 1;
    gdc_init_done = TRUE;

    biosadapter[0] = adapter_init_value[0];
    biosadapter[0].va_flags |= V_ADP_PROBED;
    biosadapter[0].va_mode = 
	biosadapter[0].va_initial_mode = biosadapter[0].va_initial_bios_mode;

    if ((PC98_SYSTEM_PARAMETER(0x597) & 0x80) ||
	(PC98_SYSTEM_PARAMETER(0x458) & 0x80)) {
	gdc_FH = (inb(0x9a8) & 1) ? _31KHZ : _24KHZ;
    } else {
	gdc_FH = _24KHZ;
    }

    gdc_get_info(&biosadapter[0], biosadapter[0].va_initial_mode, &info);
    initialize_gdc(T25_G400, info.vi_flags & V_INFO_GRAPHICS);

    biosadapter[0].va_window = BIOS_PADDRTOVADDR(info.vi_window);
    biosadapter[0].va_window_size = info.vi_window_size;
    biosadapter[0].va_window_gran = info.vi_window_gran;
    biosadapter[0].va_buffer = 0;
    biosadapter[0].va_buffer_size = 0;
    if (info.vi_flags & V_INFO_GRAPHICS) {
	switch (info.vi_depth/info.vi_planes) {
	case 1:
	    biosadapter[0].va_line_width = info.vi_width/8;
	    break;
	case 2:
	    biosadapter[0].va_line_width = info.vi_width/4;
	    break;
	case 4:
	    biosadapter[0].va_line_width = info.vi_width/2;
	    break;
	case 8:
	default: /* shouldn't happen */
	    biosadapter[0].va_line_width = info.vi_width;
	    break;
	}
    } else {
	biosadapter[0].va_line_width = info.vi_width;
    }
    bcopy(&info, &biosadapter[0].va_info, sizeof(info));

    verify_adapter(&biosadapter[0]);

    return 1;
}

static void master_gdc_cmd(unsigned int cmd)
{
    while ( (inb(TEXT_GDC) & 2) != 0);
    outb(TEXT_GDC+2, cmd);
}

static void master_gdc_prm(unsigned int pmtr)
{
    while ( (inb(TEXT_GDC) & 2) != 0);
    outb(TEXT_GDC, pmtr);
}

static void master_gdc_word_prm(unsigned int wpmtr)
{
    master_gdc_prm(wpmtr & 0x00ff);
    master_gdc_prm((wpmtr >> 8) & 0x00ff);
}	

#ifdef LINE30
static void master_gdc_fifo_empty(void)
{
    while ( (inb(TEXT_GDC) & 4) == 0);     
}
#endif

static void master_gdc_wait_vsync(void)
{
    while ( (inb(TEXT_GDC) & 0x20) != 0);          
    while ( (inb(TEXT_GDC) & 0x20) == 0);          
}

static void gdc_cmd(unsigned int cmd)
{
    while ( (inb(GRAPHIC_GDC) & 2) != 0);
    outb( GRAPHIC_GDC+2, cmd);
}

#ifdef LINE30
static void gdc_prm(unsigned int pmtr)
{
    while ( (inb(GRAPHIC_GDC) & 2) != 0);
    outb( GRAPHIC_GDC, pmtr);
}

static void gdc_word_prm(unsigned int wpmtr)
{
    gdc_prm(wpmtr & 0x00ff);
    gdc_prm((wpmtr >> 8) & 0x00ff);
}

static void gdc_fifo_empty(void)
{
    while ( (inb(GRAPHIC_GDC) & 0x04) == 0);          
}
#endif

static void gdc_wait_vsync(void)
{
    while ( (inb(GRAPHIC_GDC) & 0x20) != 0);          
    while ( (inb(GRAPHIC_GDC) & 0x20) == 0);          
}

#ifdef LINE30
static int check_gdc_clock(void)
{
    if ((inb(IO_SYSPORT) & 0x80) == 0){
       	return _5MHZ;
    } else {
       	return _2_5MHZ;
    }
}
#endif

static void initialize_gdc(unsigned int mode, int isGraph)
{
#ifdef LINE30
    /* start 30line initialize */
    int m_mode, s_mode, gdc_clock, hsync_clock;

    gdc_clock = check_gdc_clock();
    m_mode = (mode == T25_G400) ? _25L : _30L;
    s_mode = 2*mode+gdc_clock;
    gdc_INFO = m_mode;

    master_gdc_wait_vsync();

    if ((PC98_SYSTEM_PARAMETER(0x597) & 0x80) ||
	(PC98_SYSTEM_PARAMETER(0x458) & 0x80)) {
	if (PC98_SYSTEM_PARAMETER(0x481) & 0x08) {
	    hsync_clock = (m_mode == _25L) ? gdc_FH : _31KHZ;
	    outb(0x9a8, (hsync_clock == _31KHZ) ? 1 : 0);
	} else {
	    hsync_clock = gdc_FH;
	}
    } else {
	hsync_clock = _24KHZ;
    }

    if ((gdc_clock == _2_5MHZ) &&
	(slave_param[hsync_clock][s_mode][GDC_LF] > 400)) {
	outb(0x6a, 0x83);
	outb(0x6a, 0x85);
	gdc_clock = _5MHZ;
	s_mode = 2*mode+gdc_clock;
    }

    master_gdc_cmd(_GDC_RESET);
    master_gdc_cmd(_GDC_MASTER);
    gdc_cmd(_GDC_RESET);
    gdc_cmd(_GDC_SLAVE);		

    /* GDC Master */
    master_gdc_cmd(_GDC_SYNC);
    master_gdc_prm(0x00);	/* flush less */ /* text & graph */
    master_gdc_prm(master_param[hsync_clock][m_mode][GDC_CR]);
    master_gdc_word_prm(((master_param[hsync_clock][m_mode][GDC_HFP] << 10) 
		     + (master_param[hsync_clock][m_mode][GDC_VS] << 5) 
		     + master_param[hsync_clock][m_mode][GDC_HS]));
    master_gdc_prm(master_param[hsync_clock][m_mode][GDC_HBP]);
    master_gdc_prm(master_param[hsync_clock][m_mode][GDC_VFP]);
    master_gdc_word_prm(((master_param[hsync_clock][m_mode][GDC_VBP] << 10) 
       		     + (master_param[hsync_clock][m_mode][GDC_LF])));
    master_gdc_fifo_empty();
    master_gdc_cmd(_GDC_PITCH);
    master_gdc_prm(MasterPCH);
    master_gdc_fifo_empty();
	
    /* GDC slave */
    gdc_cmd(_GDC_SYNC);
    gdc_prm(0x06);
    gdc_prm(slave_param[hsync_clock][s_mode][GDC_CR]);
    gdc_word_prm((slave_param[hsync_clock][s_mode][GDC_HFP] << 10) 
		+ (slave_param[hsync_clock][s_mode][GDC_VS] << 5) 
		+ (slave_param[hsync_clock][s_mode][GDC_HS]));
    gdc_prm(slave_param[hsync_clock][s_mode][GDC_HBP]);
    gdc_prm(slave_param[hsync_clock][s_mode][GDC_VFP]);
    gdc_word_prm((slave_param[hsync_clock][s_mode][GDC_VBP] << 10) 
		+ (slave_param[hsync_clock][s_mode][GDC_LF]));
    gdc_fifo_empty();
    gdc_cmd(_GDC_PITCH);
    gdc_prm(SlavePCH[gdc_clock]);
    gdc_fifo_empty();

    /* set Master GDC scroll param */
    master_gdc_wait_vsync();
    master_gdc_wait_vsync();
    master_gdc_wait_vsync();
    master_gdc_cmd(_GDC_SCROLL);
    master_gdc_word_prm(0);
    master_gdc_word_prm((master_param[hsync_clock][m_mode][GDC_LF] << 4)
			| 0x0000);
    master_gdc_fifo_empty();

    /* set Slave GDC scroll param */
    gdc_wait_vsync();
    gdc_cmd(_GDC_SCROLL);
    gdc_word_prm(0);
    if (gdc_clock == _5MHZ) {
	gdc_word_prm((SlaveScrlLF[mode] << 4)  | 0x4000);
    } else {
	gdc_word_prm(SlaveScrlLF[mode] << 4);
    }
    gdc_fifo_empty();

    gdc_word_prm(0);
    if (gdc_clock == _5MHZ) {
	gdc_word_prm((SlaveScrlLF[mode] << 4)  | 0x4000);
    } else {
	gdc_word_prm(SlaveScrlLF[mode] << 4);
    }
    gdc_fifo_empty();

    /* sync start */
    gdc_cmd(isGraph ? _GDC_START : _GDC_STOP);

    gdc_wait_vsync();
    gdc_wait_vsync();
    gdc_wait_vsync();

    master_gdc_cmd(isGraph ? _GDC_STOP : _GDC_START);
#else
    master_gdc_wait_vsync();
    master_gdc_cmd(isGraph ? _GDC_STOP : _GDC_START);	/* text */
    gdc_wait_vsync();
    gdc_cmd(isGraph ? _GDC_START : _GDC_STOP);		/* graphics */
#endif
}

#ifndef GDC_NOGRAPHICS
static u_char b_palette[] = {
    /* R     G     B */
    0x00, 0x00, 0x00,	/* 0 */
    0x00, 0x00, 0x7f,	/* 1 */
    0x7f, 0x00, 0x00,	/* 2 */
    0x7f, 0x00, 0x7f,	/* 3 */
    0x00, 0x7f, 0x00,	/* 4 */
    0x00, 0x7f, 0x7f,	/* 5 */
    0x7f, 0x7f, 0x00,	/* 6 */
    0x7f, 0x7f, 0x7f,	/* 7 */
    0x40, 0x40, 0x40,	/* 8 */
    0x00, 0x00, 0xff,	/* 9 */
    0xff, 0x00, 0x00,	/* 10 */
    0xff, 0x00, 0xff,	/* 11 */
    0x00, 0xff, 0x00,	/* 12 */
    0x00, 0xff, 0xff,	/* 13 */
    0xff, 0xff, 0x00,	/* 14 */
    0xff, 0xff, 0xff,	/* 15 */
};
#endif

static int
gdc_load_palette(video_adapter_t *adp, u_char *palette)
{
#ifndef GDC_NOGRAPHICS
    int i;

    if (adp->va_info.vi_flags & V_INFO_VESA) {
	gdc_wait_vsync();
	for (i = 0; i < 256; ++i) {
	    outb(0xa8, i);
	    outb(0xac, *palette++);	/* R */
	    outb(0xaa, *palette++);	/* G */
	    outb(0xae, *palette++);	/* B */
	}
    } else {
	/*
	 * XXX - Even though PC-98 text color is independent of palette,
	 * we should set palette in text mode.
	 * Because the background color of text mode is palette 0's one.
	 */
	outb(0x6a, 1);		/* 16 colors mode */
	bcopy(palette, b_palette, sizeof(b_palette));

	gdc_wait_vsync();
	for (i = 0; i < 16; ++i) {
	    outb(0xa8, i);
	    outb(0xac, *palette++ >> 4);	/* R */
	    outb(0xaa, *palette++ >> 4);	/* G */
	    outb(0xae, *palette++ >> 4);	/* B */
	}
    }
#endif
    return 0;
}

static int
gdc_save_palette(video_adapter_t *adp, u_char *palette)
{
#ifndef GDC_NOGRAPHICS
    int i;

    if (adp->va_info.vi_flags & V_INFO_VESA) {
	for (i = 0; i < 256; ++i) {
	    outb(0xa8, i);
	    *palette++ = inb(0xac);	/* R */
	    *palette++ = inb(0xaa);	/* G */
	    *palette++ = inb(0xae);	/* B */
	}
    } else {
	bcopy(b_palette, palette, sizeof(b_palette));
    }
#endif
    return 0;
}

static int
gdc_set_origin(video_adapter_t *adp, off_t offset)
{
#ifndef GDC_NOGRAPHICS
    if (adp->va_info.vi_flags & V_INFO_VESA) {
	writew(BIOS_PADDRTOVADDR(0x000e0004), offset >> 15);
    }
#endif
    return 0;
}

/* entry points */

static int
gdc_err(video_adapter_t *adp, ...)
{
    return ENODEV;
}

static int
gdc_probe(int unit, video_adapter_t **adpp, void *arg, int flags)
{
    probe_adapters();
    if (unit >= 1)
	return ENXIO;

    *adpp = &biosadapter[unit];

    return 0;
}

static int
gdc_init(int unit, video_adapter_t *adp, int flags)
{
    if ((unit >= 1) || (adp == NULL) || !probe_done(adp))
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

    return 0;
}

/*
 * get_info():
 * Return the video_info structure of the requested video mode.
 */
static int
gdc_get_info(video_adapter_t *adp, int mode, video_info_t *info)
{
    int i;

    if (!gdc_init_done)
	return ENXIO;

    mode = map_gen_mode_num(adp->va_type, adp->va_flags & V_ADP_COLOR, mode);
    for (i = 0; bios_vmode[i].vi_mode != EOT; ++i) {
	if (bios_vmode[i].vi_mode == NA)
	    continue;
	if (mode == bios_vmode[i].vi_mode) {
	    *info = bios_vmode[i];
	    info->vi_buffer_size = info->vi_window_size*info->vi_planes;
	    return 0;
	}
    }
    return EINVAL;
}

/*
 * query_mode():
 * Find a video mode matching the requested parameters.
 * Fields filled with 0 are considered "don't care" fields and
 * match any modes.
 */
static int
gdc_query_mode(video_adapter_t *adp, video_info_t *info)
{
    int i;

    if (!gdc_init_done)
	return ENXIO;

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
	if (gdc_get_info(adp, bios_vmode[i].vi_mode, info))
		continue;
	return 0;
    }
    return ENODEV;
}

/*
 * set_mode():
 * Change the video mode.
 */
static int
gdc_set_mode(video_adapter_t *adp, int mode)
{
    video_info_t info;

    prologue(adp, V_ADP_MODECHANGE, ENODEV);

    mode = map_gen_mode_num(adp->va_type, 
			    adp->va_flags & V_ADP_COLOR, mode);
    if (gdc_get_info(adp, mode, &info))
	return EINVAL;

    switch (info.vi_mode) {
#ifndef GDC_NOGRAPHICS
	case M_PC98_PEGC640x480:	/* PEGC 640x480 */
	    initialize_gdc(T30_G480, info.vi_flags & V_INFO_GRAPHICS);
	    break;
	case M_PC98_PEGC640x400:	/* PEGC 640x400 */
	case M_PC98_EGC640x400:		/* EGC GRAPHICS */
#endif
	case M_PC98_80x25:		/* VGA TEXT */
	    initialize_gdc(T25_G400, info.vi_flags & V_INFO_GRAPHICS);
	    break;
	case M_PC98_80x30:		/* VGA TEXT */
	    initialize_gdc(T30_G400, info.vi_flags & V_INFO_GRAPHICS);
	    break;
	default:
	    break;
    }

#ifndef GDC_NOGRAPHICS
    if (info.vi_flags & V_INFO_VESA) {
	outb(0x6a, 0x07);		/* enable mode F/F change */
	outb(0x6a, 0x21);		/* enhanced graphics */
	if (info.vi_height > 400)
	    outb(0x6a, 0x69);		/* 800 lines */
	writeb(BIOS_PADDRTOVADDR(0x000e0100), 0);	/* packed pixel */
    } else {
	if (adp->va_flags & V_ADP_VESA) {
	    outb(0x6a, 0x07);		/* enable mode F/F change */
	    outb(0x6a, 0x20);		/* normal graphics */
	    outb(0x6a, 0x68);		/* 400 lines */
	}
	outb(0x6a, 1);			/* 16 colors */
    }
#endif

    adp->va_mode = mode;
    adp->va_flags &= ~V_ADP_COLOR;
    adp->va_flags |= 
	(info.vi_flags & V_INFO_COLOR) ? V_ADP_COLOR : 0;
#if 0
    adp->va_crtc_addr =
	(adp->va_flags & V_ADP_COLOR) ? COLOR_CRTC : MONO_CRTC;
#endif
    adp->va_window = BIOS_PADDRTOVADDR(info.vi_window);
    adp->va_window_size = info.vi_window_size;
    adp->va_window_gran = info.vi_window_gran;
    if (info.vi_buffer_size == 0) {
    	adp->va_buffer = 0;
    	adp->va_buffer_size = 0;
    } else {
    	adp->va_buffer = BIOS_PADDRTOVADDR(info.vi_buffer);
    	adp->va_buffer_size = info.vi_buffer_size;
    }
    if (info.vi_flags & V_INFO_GRAPHICS) {
	switch (info.vi_depth/info.vi_planes) {
	case 1:
	    adp->va_line_width = info.vi_width/8;
	    break;
	case 2:
	    adp->va_line_width = info.vi_width/4;
	    break;
	case 4:
	    adp->va_line_width = info.vi_width/2;
	    break;
	case 8:
	default: /* shouldn't happen */
	    adp->va_line_width = info.vi_width;
	    break;
	}
    } else {
	adp->va_line_width = info.vi_width;
    }
    bcopy(&info, &adp->va_info, sizeof(info));

    /* move hardware cursor out of the way */
    (*vidsw[adp->va_index]->set_hw_cursor)(adp, -1, -1);

    return 0;
}

/*
 * set_border():
 * Change the border color.
 */
static int
gdc_set_border(video_adapter_t *adp, int color)
{
    outb(0x6c, color << 4);                                                 
    return 0;
}

/*
 * save_state():
 * Read video card register values.
 */
static int
gdc_save_state(video_adapter_t *adp, void *p, size_t size)
{
    return ENODEV;
}

/*
 * load_state():
 * Set video card registers at once.
 */
static int
gdc_load_state(video_adapter_t *adp, void *p)
{
    return ENODEV;
}

/*
 * read_hw_cursor():
 * Read the position of the hardware text cursor.
 */
static int
gdc_read_hw_cursor(video_adapter_t *adp, int *col, int *row)
{
    u_int16_t off;
    int s;

    if (!gdc_init_done)
	return ENXIO;

    if (adp->va_info.vi_flags & V_INFO_GRAPHICS)
	return ENODEV;

    s = spltty();
    master_gdc_cmd(0xe0);	/* _GDC_CSRR */
    while((inb(TEXT_GDC + 0) & 0x1) == 0) {}	/* GDC wait */
    off = inb(TEXT_GDC + 2);			/* EADl */
    off |= (inb(TEXT_GDC + 2) << 8);		/* EADh */
    inb(TEXT_GDC + 2);				/* dummy */
    inb(TEXT_GDC + 2);				/* dummy */
    inb(TEXT_GDC + 2);				/* dummy */
    splx(s);

    if (off >= ROW*COL)
	off = 0;
    *row = off / adp->va_info.vi_width;
    *col = off % adp->va_info.vi_width;

    return 0;
}

/*
 * set_hw_cursor():
 * Move the hardware text cursor.  If col and row are both -1, 
 * the cursor won't be shown.
 */
static int
gdc_set_hw_cursor(video_adapter_t *adp, int col, int row)
{
    u_int16_t off;
    int s;

    if (!gdc_init_done)
	return ENXIO;

    if ((col == -1) && (row == -1)) {
	off = -1;
    } else {
	if (adp->va_info.vi_flags & V_INFO_GRAPHICS)
	    return ENODEV;
	off = row*adp->va_info.vi_width + col;
    }

    s = spltty();
    master_gdc_cmd(0x49);	/* _GDC_CSRW */
    master_gdc_word_prm(off);
    splx(s);

    return 0;
}

/*
 * set_hw_cursor_shape():
 * Change the shape of the hardware text cursor.  If the height is zero
 * or negative, the cursor won't be shown.
 */
static int
gdc_set_hw_cursor_shape(video_adapter_t *adp, int base, int height,
			int celsize, int blink)
{
    int start;
    int end;
    int s;

    if (!gdc_init_done)
	return ENXIO;

    start = celsize - (base + height);
    end = celsize - base - 1;

#if 0
    /*
     * muPD7220 GDC has anomaly that if end == celsize - 1 then start
     * must be 0, otherwise the cursor won't be correctly shown 
     * in the first row in the screen.  We shall set end to celsize - 2;
     * if end == celsize -1 && start > 0. XXX
     */
    if ((end == celsize - 1) && (start > 0) && (start < end))
	--end;
#endif

    s = spltty();
    master_gdc_cmd(0x4b);			/* _GDC_CSRFORM */
    master_gdc_prm(((height > 0) ? 0x80 : 0)	/* cursor on/off */
	| ((celsize - 1) & 0x1f));		/* cel size */
    master_gdc_word_prm(((end & 0x1f) << 11)	/* end line */
	| (12 << 6)				/* blink rate */
	| (blink ? 0 : 0x20)			/* blink on/off */
	| (start & 0x1f));			/* start line */
    splx(s);

    return 0;
}

/*
 * blank_display()
 * Put the display in power save/power off mode.
 */
static int
gdc_blank_display(video_adapter_t *adp, int mode)
{
    int s;
    static int standby = 0;

    if (!gdc_init_done)
	return ENXIO;

    s = splhigh();
    switch (mode) {
    case V_DISPLAY_SUSPEND:
    case V_DISPLAY_STAND_BY:
	outb(0x09a2, 0x80 | 0x40);		/* V/H-SYNC mask */
	if (inb(0x09a2) == (0x80 | 0x40))
	    standby = 1;
	/* FALLTHROUGH */

    case V_DISPLAY_BLANK:
	if (epson_machine_id == 0x20) {
	    outb(0x43f, 0x42);
	    outb(0xc17, inb(0xc17) & ~0x08);	/* turn off side light */
	    outb(0xc16, inb(0xc16) & ~0x02);	/* turn off back light */
	    outb(0x43f, 0x40);
	} else {
	    while (!(inb(TEXT_GDC) & 0x20))	/* V-SYNC wait */
		;
	    outb(TEXT_GDC + 8, 0x0e);		/* DISP off */
	}
	break;

    case V_DISPLAY_ON:
	if (epson_machine_id == 0x20) {
	    outb(0x43f, 0x42);
	    outb(0xc17, inb(0xc17) | 0x08);
	    outb(0xc16, inb(0xc16) | 0x02);
	    outb(0x43f, 0x40);
	} else {
	    while (!(inb(TEXT_GDC) & 0x20))	/* V-SYNC wait */
		;
	    outb(TEXT_GDC + 8, 0x0f);		/* DISP on */
	}
	if (standby) {
	    outb(0x09a2, 0x00);			/* V/H-SYNC unmask */
	    standby = 0;
	}
	break;
    }
    splx(s);
    return 0;
}

/*
 * mmap():
 * Mmap frame buffer.
 */
static int
gdc_mmap_buf(video_adapter_t *adp, vm_offset_t offset, vm_offset_t *paddr,
	     int prot)
{
    /* FIXME: is this correct? XXX */
    if (offset > VIDEO_BUF_SIZE - PAGE_SIZE)
	return -1;
    *paddr = adp->va_info.vi_window + offset;
    return 0;
}

#ifndef GDC_NOGRAPHICS
static void
planar_fill(video_adapter_t *adp, int val)
{

    outb(0x7c, 0x80);				/* GRCG on & TDW mode */
    outb(0x7e, 0);				/* tile B */
    outb(0x7e, 0);				/* tile R */
    outb(0x7e, 0);				/* tile G */
    outb(0x7e, 0);				/* tile I */

    fillw_io(0, adp->va_window, 0x8000 / 2);	/* XXX */

    outb(0x7c, 0);				/* GRCG off */
}

static void
packed_fill(video_adapter_t *adp, int val)
{
    int length;
    int at;			/* position in the frame buffer */
    int l;

    at = 0;
    length = adp->va_line_width*adp->va_info.vi_height;
    while (length > 0) {
	l = imin(length, adp->va_window_size);
	(*vidsw[adp->va_index]->set_win_org)(adp, at);
	bzero_io(adp->va_window, l);
	length -= l;
	at += l;
    }
}

static int
gdc_clear(video_adapter_t *adp)
{

    switch (adp->va_info.vi_mem_model) {
    case V_INFO_MM_TEXT:
	/* do nothing? XXX */
	break;
    case V_INFO_MM_PLANAR:
	planar_fill(adp, 0);
	break;
    case V_INFO_MM_PACKED:
	packed_fill(adp, 0);
	break;
    }

    return 0;
}
#else /* GDC_NOGRAPHICS */
static int
gdc_clear(video_adapter_t *adp)
{

    return 0;
}
#endif /* GDC_NOGRAPHICS */

static int
gdc_fill_rect(video_adapter_t *adp, int val, int x, int y, int cx, int cy)
{
    return ENODEV;
}

static int
gdc_bitblt(video_adapter_t *adp,...)
{
    /* FIXME */
    return ENODEV;
}

static int
gdc_dev_ioctl(video_adapter_t *adp, u_long cmd, caddr_t arg)
{
    switch (cmd) {
    case FBIO_GETWINORG:	/* get frame buffer window origin */
	*(u_int *)arg = 0;
	return 0;

    case FBIO_SETWINORG:	/* set frame buffer window origin */
    case FBIO_SETDISPSTART:	/* set display start address */
    case FBIO_SETLINEWIDTH:	/* set scan line length in pixel */
    case FBIO_GETPALETTE:	/* get color palette */
    case FBIO_SETPALETTE:	/* set color palette */
    case FBIOGETCMAP:		/* get color palette */
    case FBIOPUTCMAP:		/* set color palette */
	return ENODEV;

    case FBIOGTYPE:		/* get frame buffer type info. */
	((struct fbtype *)arg)->fb_type = fb_type(adp->va_type);
	((struct fbtype *)arg)->fb_height = adp->va_info.vi_height;
	((struct fbtype *)arg)->fb_width = adp->va_info.vi_width;
	((struct fbtype *)arg)->fb_depth = adp->va_info.vi_depth;
	if ((adp->va_info.vi_depth <= 1) || (adp->va_info.vi_depth > 8))
	    ((struct fbtype *)arg)->fb_cmsize = 0;
	else
	    ((struct fbtype *)arg)->fb_cmsize = 1 << adp->va_info.vi_depth;
	((struct fbtype *)arg)->fb_size = adp->va_buffer_size;
	return 0;

    default:
	return fb_commonioctl(adp, cmd, arg);
    }
}

/*
 * diag():
 * Print some information about the video adapter and video modes,
 * with requested level of details.
 */
static int
gdc_diag(video_adapter_t *adp, int level)
{
#if FB_DEBUG > 1
    int i;
#endif

    if (!gdc_init_done)
	return ENXIO;

    fb_dump_adp_info(DRIVER_NAME, adp, level);

#if FB_DEBUG > 1
    for (i = 0; bios_vmode[i].vi_mode != EOT; ++i) {
	 if (bios_vmode[i].vi_mode == NA)
	    continue;
	 if (get_mode_param(bios_vmode[i].vi_mode) == NULL)
	    continue;
	 fb_dump_mode_info(DRIVER_NAME, adp, &bios_vmode[i], level);
    }
#endif

    return 0;
}
