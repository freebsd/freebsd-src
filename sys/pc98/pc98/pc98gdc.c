/*
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
 *	$Id: pc98gdc.c,v 1.6 1999/03/02 12:34:24 kato Exp $
 */

#include "gdc.h"
#include "opt_gdc.h"
#include "opt_fb.h"
#include "opt_syscons.h"

#if NGDC > 0

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

#include <pc98/pc98/pc98.h>
#include <pc98/pc98/pc98_machdep.h>
#include <isa/isavar.h>

#define TEXT_GDC	IO_GDC1	/* 0x60 */
#define	ROW		25
#define	COL		80

#define DRIVER_NAME	"gdc"

/* cdev driver declaration */

#define GDC_UNIT(dev)	minor(dev)
#define GDC_MKMINOR(unit) (unit)

typedef struct gdc_softc {
	video_adapter_t	*adp;
} gdc_softc_t;

#define GDC_SOFTC(unit)	\
	((gdc_softc_t *)devclass_get_softc(gdc_devclass, unit))

devclass_t		gdc_devclass;

static int		gdcprobe(device_t dev);
static int		gdc_attach(device_t dev);

static device_method_t gdc_methods[] = {
	DEVMETHOD(device_probe,		gdcprobe),
	DEVMETHOD(device_attach,	gdc_attach),
	{ 0, 0 }
};

static driver_t gdcdriver = {
	DRIVER_NAME,
	gdc_methods,
	DRIVER_TYPE_TTY,
	sizeof(gdc_softc_t),
};

DRIVER_MODULE(gdc, isa, gdcdriver, gdc_devclass, 0, 0);

static int		gdc_probe_unit(int unit, gdc_softc_t *sc, int flags);
static int		gdc_attach_unit(int unit, gdc_softc_t *sc, int flags);

#if FB_INSTALL_CDEV

static d_open_t		gdcopen;
static d_close_t	gdcclose;
static d_read_t		gdcread;
static d_ioctl_t	gdcioctl;

static struct  cdevsw vga_cdevsw = {
	gdcopen,	gdcclose,	noread,		nowrite,	/* ?? */
	gdcioctl,	nostop,		nullreset,	nodevtotty,
	seltrue,	nommap,		NULL,		DRIVER_NAME,
	NULL,		-1,		nodump,		nopsize,
};

#endif /* FB_INSTALL_CDEV */

static int
gdcprobe(device_t dev)
{
	gdc_softc_t *sc;

	device_set_desc(dev, "Generic GDC");
	sc = device_get_softc(dev);
	return gdc_probe_unit(device_get_unit(dev), sc, isa_get_flags(dev));
}

static int
gdc_attach(device_t dev)
{
	gdc_softc_t *sc;

	sc = device_get_softc(dev);
	return gdc_attach_unit(device_get_unit(dev), sc, isa_get_flags(dev));
}

static int
gdc_probe_unit(int unit, gdc_softc_t *sc, int flags)
{
	video_switch_t *sw;

	bzero(sc, sizeof(*sc));
	sw = vid_get_switch(DRIVER_NAME);
	if (sw == NULL)
		return 0;
	return (*sw->probe)(unit, &sc->adp, NULL, flags);
}

static int
gdc_attach_unit(int unit, gdc_softc_t *sc, int flags)
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
	error = fb_attach(makedev(0, GDC_MKMINOR(unit)), scp->adp,
			  &vga_cdevsw);
	if (error)
		return error;
#endif /* FB_INSTALL_CDEV */

	if (bootverbose)
		(*sw->diag)(sc->adp, bootverbose);

	return 0;
}

/* LOW-LEVEL */

#include <machine/clock.h>

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
      IO_GDC1, 16, TEXT_GDC,		/* va_io*, XXX */
      VIDEO_BUF_BASE, VIDEO_BUF_SIZE,	/* va_mem* */
      TEXT_BUF_BASE, TEXT_BUF_SIZE, TEXT_BUF_SIZE, /* va_window* */
      0, 0, 				/* va_buffer, va_buffer_size */
      0, M_PC98_80x25, 0, 		/* va_*mode* */
    },
};

static video_adapter_t	biosadapter[1];

/* video driver declarations */
static int			gdc_configure(int flags);
static int			gdc_nop(void);
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
static vi_mmap_t		gdc_mmap;
static vi_diag_t		gdc_diag;

static int			gdc_err(video_adapter_t *adp, ...);

static video_switch_t gdcvidsw = {
	gdc_probe,
	gdc_init,
	gdc_get_info,
	gdc_query_mode,	
	gdc_set_mode,
	(vi_save_font_t *)gdc_err,
	(vi_load_font_t *)gdc_err,
	(vi_show_font_t *)gdc_err,
	(vi_save_palette_t *)gdc_err,
	(vi_load_palette_t *)gdc_err,
	gdc_set_border,
	gdc_save_state,
	gdc_load_state,
	(vi_set_win_org_t *)gdc_err,
	gdc_read_hw_cursor,
	gdc_set_hw_cursor,
	gdc_set_hw_cursor_shape,
	(vi_blank_display_t *)gdc_nop,
	gdc_mmap,
	gdc_diag,
};

VIDEO_DRIVER(gdc, gdcvidsw, gdc_configure);

/* GDC BIOS standard video modes */
#define EOT		(-1)
#define NA		(-2)

static video_info_t bios_vmode[] = {
    { M_PC98_80x25, V_INFO_COLOR, 80, 25, 8, 16, 4, 1,
      TEXT_BUF_BASE, TEXT_BUF_SIZE, TEXT_BUF_SIZE, 0, 0 },
#ifdef LINE30
    { M_PC98_80x30, V_INFO_COLOR, 80, 30, 8, 16, 4, 1,
      TEXT_BUF_BASE, TEXT_BUF_SIZE, TEXT_BUF_SIZE, 0, 0 },
#endif
    { EOT },
};

static int		init_done = FALSE;

/* local functions */
static int map_gen_mode_num(int type, int color, int mode);
static int probe_adapters(void);
static void dump_buffer(u_char *buf, size_t len);

#define	prologue(adp, flag, err)			\
	if (!init_done || !((adp)->va_flags & (flag)))	\
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

/* probe video adapters and return the number of detected adapters */
static int
probe_adapters(void)
{
    video_info_t info;

    /* do this test only once */
    if (init_done)
	return 1;
    init_done = TRUE;

    biosadapter[0] = adapter_init_value[0];
    biosadapter[0].va_flags |= V_ADP_PROBED;
    biosadapter[0].va_mode = 
	biosadapter[0].va_initial_mode = biosadapter[0].va_initial_bios_mode;

    master_gdc_wait_vsync();
    master_gdc_cmd(_GDC_START);	/* text ON */
    gdc_wait_vsync();
    gdc_cmd(_GDC_STOP);		/* graphics OFF */

    gdc_get_info(&biosadapter[0], biosadapter[0].va_initial_mode, &info);
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

    return 1;
}

static void master_gdc_cmd(unsigned int cmd)
{
    while ( (inb(IO_GDC1) & 2) != 0);
    outb(IO_GDC1+2, cmd);
}

static void master_gdc_prm(unsigned int pmtr)
{
    while ( (inb(IO_GDC1) & 2) != 0);
    outb(IO_GDC1, pmtr);
}

static void master_gdc_word_prm(unsigned int wpmtr)
{
    master_gdc_prm(wpmtr & 0x00ff);
    master_gdc_prm((wpmtr >> 8) & 0x00ff);
}	

static void master_gdc_fifo_empty(void)
{
    while ( (inb(IO_GDC1) & 4) == 0);     
}

static void master_gdc_wait_vsync(void)
{
    while ( (inb(IO_GDC1) & 0x20) != 0);          
    while ( (inb(IO_GDC1) & 0x20) == 0);          
}

static void gdc_cmd(unsigned int cmd)
{
    while ( (inb(IO_GDC2) & 2) != 0);
    outb( IO_GDC2+2, cmd);
}

static void gdc_prm(unsigned int pmtr)
{
    while ( (inb(IO_GDC2) & 2) != 0);
    outb( IO_GDC2, pmtr);
}

static void gdc_word_prm(unsigned int wpmtr)
{
    gdc_prm(wpmtr & 0x00ff);
    gdc_prm((wpmtr >> 8) & 0x00ff);
}

static void gdc_fifo_empty(void)
{
    while ( (inb(IO_GDC2) & 0x04) == 0);          
}

static void gdc_wait_vsync(void)
{
    while ( (inb(IO_GDC2) & 0x20) != 0);          
    while ( (inb(IO_GDC2) & 0x20) == 0);          
}

static int check_gdc_clock(void)
{
    if ((inb(IO_SYSPORT) & 0x80) == 0){
       	return _5MHZ;
    } else {
       	return _2_5MHZ;
    }
}

static void initialize_gdc(unsigned int mode)
{
    /* start 30line initialize */
    int m_mode,s_mode,gdc_clock;
    gdc_clock = check_gdc_clock();

    if (mode == T25_G400){
	m_mode = _25L;
    }else{
	m_mode = _30L;
    }

    s_mode = 2*mode+gdc_clock;

    gdc_INFO = m_mode;

    master_gdc_cmd(_GDC_RESET);
    master_gdc_cmd(_GDC_MASTER);
    gdc_cmd(_GDC_RESET);
    gdc_cmd(_GDC_SLAVE);		

    /* GDC Master */
    master_gdc_cmd(_GDC_SYNC);
    master_gdc_prm(0x00);	/* flush less */ /* text & graph */
    master_gdc_prm(master_param[m_mode][GDC_CR]);
    master_gdc_word_prm(((master_param[m_mode][GDC_HFP] << 10) 
		     + (master_param[m_mode][GDC_VS] << 5) 
		     + master_param[m_mode][GDC_HS]));
    master_gdc_prm(master_param[m_mode][GDC_HBP]);
    master_gdc_prm(master_param[m_mode][GDC_VFP]);
    master_gdc_word_prm(((master_param[m_mode][GDC_VBP] << 10) 
       		     + (master_param[m_mode][GDC_LF])));
    master_gdc_fifo_empty();
    master_gdc_cmd(_GDC_PITCH);
    master_gdc_prm(MasterPCH);
    master_gdc_fifo_empty();
	
    /* GDC slave */
    gdc_cmd(_GDC_SYNC);
    gdc_prm(0x06);
    gdc_prm(slave_param[s_mode][GDC_CR]);
    gdc_word_prm((slave_param[s_mode][GDC_HFP] << 10) 
		+ (slave_param[s_mode][GDC_VS] << 5) 
		+ (slave_param[s_mode][GDC_HS]));
    gdc_prm(slave_param[s_mode][GDC_HBP]);
    gdc_prm(slave_param[s_mode][GDC_VFP]);
    gdc_word_prm((slave_param[s_mode][GDC_VBP] << 10) 
		+ (slave_param[s_mode][GDC_LF]));
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
    master_gdc_word_prm((master_param[m_mode][GDC_LF] << 4) | 0x0000);
    master_gdc_fifo_empty();

    /* set Slave GDC scroll param */
    gdc_wait_vsync();
    gdc_cmd(_GDC_SCROLL);
    gdc_word_prm(0);
    if (gdc_clock == _5MHZ){
	gdc_word_prm((SlaveScrlLF[mode] << 4)  | 0x4000);
    }else{
	gdc_word_prm(SlaveScrlLF[mode] << 4);
    }
    gdc_fifo_empty();

    gdc_word_prm(0);
    if (gdc_clock == _5MHZ){
	gdc_word_prm((SlaveScrlLF[mode] << 4)  | 0x4000);
    }else{
	gdc_word_prm(SlaveScrlLF[mode] << 4);
    }
    gdc_fifo_empty();

    /* sync start */
    gdc_cmd(_GDC_STOP);

    gdc_wait_vsync();
    gdc_wait_vsync();
    gdc_wait_vsync();

    master_gdc_cmd(_GDC_START);
}

/* entry points */

static int
gdc_nop(void)
{
    return 0;
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

    if (!init_done)
	return 1;

    mode = map_gen_mode_num(adp->va_type, adp->va_flags & V_ADP_COLOR, mode);
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
 */
static int
gdc_query_mode(video_adapter_t *adp, video_info_t *info)
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
	if (gdc_get_info(adp, bios_vmode[i].vi_mode, &buf))
		continue;
	return bios_vmode[i].vi_mode;
    }
    return -1;
}

/*
 * set_mode():
 * Change the video mode.
 */
static int
gdc_set_mode(video_adapter_t *adp, int mode)
{
    video_info_t info;

    prologue(adp, V_ADP_MODECHANGE, 1);

    mode = map_gen_mode_num(adp->va_type, 
			    adp->va_flags & V_ADP_COLOR, mode);
    if (gdc_get_info(adp, mode, &info))
	return 1;

#ifdef LINE30
    switch (info.vi_mode) {
       	case M_PC98_80x25:	/* VGA TEXT MODES */
		initialize_gdc(T25_G400);
		break;
	case M_PC98_80x30:
		initialize_gdc(T30_G400);
		break;
	default:
		break;
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
    return 1;
}

/*
 * load_state():
 * Set video card registers at once.
 */
static int
gdc_load_state(video_adapter_t *adp, void *p)
{
    return 1;
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

    if (!init_done)
	return 1;

    if (adp->va_info.vi_flags & V_INFO_GRAPHICS)
	return 1;

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

    start = celsize - (base + height);
    end = celsize - base - 1;
    /*
     * muPD7220 GDC has anomaly that if end == celsize - 1 then start
     * must be 0, otherwise the cursor won't be correctly shown 
     * in the first row in the screen.  We shall set end to celsize - 2;
     * if end == celsize -1 && start > 0. XXX
     */
    if ((end == celsize - 1) && (start > 0))
	--end;

    s = spltty();
    master_gdc_cmd(0x4b);			/* _GDC_CSRFORM */
    master_gdc_prm(((height > 0) ? 0x80 : 0)	/* cursor on/off */
	| (celsize - 1) & 0x1f);		/* cel size */
    master_gdc_word_prm(((end & 0x1f) << 11)	/* end line */
	| (12 << 6)				/* blink rate */
	| (blink ? 0x20 : 0)			/* blink on/off */
	| (start & 0x1f));			/* start line */
    splx(s);

    return 1;
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
 * mmap():
 * Mmap frame buffer.
 */
static int
gdc_mmap(video_adapter_t *adp, vm_offset_t offset)
{
    if (offset > 0x48000 - PAGE_SIZE)
	return -1;
    return i386_btop((VIDEO_BUF_BASE + offset));
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

    if (!init_done)
	return 1;

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

static int
gdc_err(video_adapter_t *adp, ...)
{
	return 0;
}

#endif /* NGDC > 0 */
