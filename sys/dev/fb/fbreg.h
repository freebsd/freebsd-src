/*-
 * Copyright (c) 1999 Kazutaka YOKOTA <yokota@zodiac.mech.utsunomiya-u.ac.jp>
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
 * $FreeBSD: src/sys/dev/fb/fbreg.h,v 1.6 1999/12/29 04:35:36 peter Exp $
 */

#ifndef _DEV_FB_FBREG_H_
#define _DEV_FB_FBREG_H_

#ifdef _KERNEL

#define V_MAX_ADAPTERS		8		/* XXX */

/* some macros */
#ifdef __i386__
#define bcopy_io(s, d, c)	generic_bcopy((void *)(s), (void *)(d), (c))
#define bcopy_toio(s, d, c)	generic_bcopy((void *)(s), (void *)(d), (c))
#define bcopy_fromio(s, d, c)	generic_bcopy((void *)(s), (void *)(d), (c))
#define bzero_io(d, c)		generic_bzero((void *)(d), (c))
#define fill_io(p, d, c)	fill((p), (void *)(d), (c))
#define fillw_io(p, d, c)	fillw((p), (void *)(d), (c))
void generic_bcopy(const void *s, void *d, size_t c);
void generic_bzero(void *d, size_t c);
#else /* !__i386__ */
#define bcopy_io(s, d, c)	memcpy_io((d), (s), (c))
#define bcopy_toio(s, d, c)	memcpy_toio((d), (void *)(s), (c))
#define bcopy_fromio(s, d, c)	memcpy_fromio((void *)(d), (s), (c))
#define bzero_io(d, c)		memset_io((d), 0, (c))
#define fill_io(p, d, c)	memset_io((d), (p), (c))
#define fillw(p, d, c)		memsetw((d), (p), (c))
#define fillw_io(p, d, c)	memsetw_io((d), (p), (c))
#endif /* !__i386__ */

/* video function table */
typedef int vi_probe_t(int unit, video_adapter_t **adpp, void *arg, int flags);
typedef int vi_init_t(int unit, video_adapter_t *adp, int flags);
typedef int vi_get_info_t(video_adapter_t *adp, int mode, video_info_t *info);
typedef int vi_query_mode_t(video_adapter_t *adp, video_info_t *info);
typedef int vi_set_mode_t(video_adapter_t *adp, int mode);
typedef int vi_save_font_t(video_adapter_t *adp, int page, int size,
			   u_char *data, int c, int count);
typedef int vi_load_font_t(video_adapter_t *adp, int page, int size,
			   u_char *data, int c, int count);
typedef int vi_show_font_t(video_adapter_t *adp, int page);
typedef int vi_save_palette_t(video_adapter_t *adp, u_char *palette);
typedef int vi_load_palette_t(video_adapter_t *adp, u_char *palette);
typedef int vi_set_border_t(video_adapter_t *adp, int border);
typedef int vi_save_state_t(video_adapter_t *adp, void *p, size_t size);
typedef int vi_load_state_t(video_adapter_t *adp, void *p);
typedef int vi_set_win_org_t(video_adapter_t *adp, off_t offset);
typedef int vi_read_hw_cursor_t(video_adapter_t *adp, int *col, int *row);
typedef int vi_set_hw_cursor_t(video_adapter_t *adp, int col, int row);
typedef int vi_set_hw_cursor_shape_t(video_adapter_t *adp, int base,
				     int height, int celsize, int blink);
typedef int vi_blank_display_t(video_adapter_t *adp, int mode);
#define V_DISPLAY_ON		0
#define V_DISPLAY_BLANK		1
#define V_DISPLAY_STAND_BY	2
#define V_DISPLAY_SUSPEND	3
typedef int vi_mmap_t(video_adapter_t *adp, vm_offset_t offset, int prot);
typedef int vi_ioctl_t(video_adapter_t *adp, u_long cmd, caddr_t data);
typedef int vi_clear_t(video_adapter_t *adp);
typedef int vi_fill_rect_t(video_adapter_t *adp, int val, int x, int y,
			   int cx, int cy);
typedef int vi_bitblt_t(video_adapter_t *adp,...);
typedef int vi_diag_t(video_adapter_t *adp, int level);

typedef struct video_switch {
    vi_probe_t		*probe;
    vi_init_t		*init;
    vi_get_info_t	*get_info;
    vi_query_mode_t	*query_mode;
    vi_set_mode_t	*set_mode;
    vi_save_font_t	*save_font;
    vi_load_font_t	*load_font;
    vi_show_font_t	*show_font;
    vi_save_palette_t	*save_palette;
    vi_load_palette_t	*load_palette;
    vi_set_border_t	*set_border;
    vi_save_state_t	*save_state;
    vi_load_state_t	*load_state;
    vi_set_win_org_t	*set_win_org;
    vi_read_hw_cursor_t	*read_hw_cursor;
    vi_set_hw_cursor_t	*set_hw_cursor;
    vi_set_hw_cursor_shape_t *set_hw_cursor_shape;
    vi_blank_display_t	*blank_display;
    vi_mmap_t		*mmap;
    vi_ioctl_t		*ioctl;
    vi_clear_t		*clear;
    vi_fill_rect_t	*fill_rect;
    vi_bitblt_t		*bitblt;
    int			(*reserved1)(void);
    int			(*reserved2)(void);
    vi_diag_t		*diag;
} video_switch_t;

#define save_palette(adp, pal)				\
	(*vidsw[(adp)->va_index]->save_palette)((adp), (pal))
#define load_palette(adp, pal)				\
	(*vidsw[(adp)->va_index]->load_palette)((adp), (pal))
#define get_mode_info(adp, mode, buf)			\
	(*vidsw[(adp)->va_index]->get_info)((adp), (mode), (buf))
#define set_video_mode(adp, mode)			\
	(*vidsw[(adp)->va_index]->set_mode)((adp), (mode))
#if 0 /* XXX conflicts with syscons' set_border() */
#define set_border(adp, border)				\
	(*vidsw[(adp)->va_index]->set_border)((adp), (border))
#endif
#define set_origin(adp, o)				\
	(*vidsw[(adp)->va_index]->set_win_org)(adp, o)

/* XXX - add more macros */

/* video driver */
typedef struct video_driver {
    char		*name;
    video_switch_t	*vidsw;
    int			(*configure)(int); /* backdoor for the console driver */
} video_driver_t;

#define VIDEO_DRIVER(name, sw, config)			\
	static struct video_driver name##_driver = {	\
		#name, &sw, config			\
	};						\
	DATA_SET(videodriver_set, name##_driver);

/* global variables */
extern struct video_switch **vidsw;
extern struct linker_set videodriver_set;

/* functions for the video card driver */
int		vid_register(video_adapter_t *adp);
int		vid_unregister(video_adapter_t *adp);
video_switch_t	*vid_get_switch(char *name);
void		vid_init_struct(video_adapter_t *adp, char *name, int type,
				int unit);

/* functions for the video card client */
int		vid_allocate(char *driver, int unit, void *id);
int		vid_release(video_adapter_t *adp, void *id);
int		vid_find_adapter(char *driver, int unit);
video_adapter_t	*vid_get_adapter(int index);

/* a backdoor for the console driver to tickle the video driver XXX */
int		vid_configure(int flags);
#define VIO_PROBE_ONLY	(1 << 0)	/* probe only, don't initialize */

#ifdef FB_INSTALL_CDEV

/* virtual frame buffer driver functions */
int		fb_attach(dev_t dev, video_adapter_t *adp,
			  struct cdevsw *cdevsw);
int		fb_detach(dev_t dev, video_adapter_t *adp,
			  struct cdevsw *cdevsw);

/* generic frame buffer cdev driver functions */

typedef struct genfb_softc {
	int		gfb_flags;	/* flag/status bits */
#define FB_OPEN		(1 << 0)
} genfb_softc_t;

int		genfbopen(genfb_softc_t *sc, video_adapter_t *adp,
			  int flag, int mode, struct proc *p);
int		genfbclose(genfb_softc_t *sc, video_adapter_t *adp,
			   int flag, int mode, struct proc *p);
int		genfbread(genfb_softc_t *sc, video_adapter_t *adp,
			  struct uio *uio, int flag);
int		genfbwrite(genfb_softc_t *sc, video_adapter_t *adp,
			   struct uio *uio, int flag);
int		genfbioctl(genfb_softc_t *sc, video_adapter_t *adp,
			   u_long cmd, caddr_t arg, int flag, struct proc *p);
int		genfbmmap(genfb_softc_t *sc, video_adapter_t *adp,
			  vm_offset_t offset, int prot);

#endif /* FB_INSTALL_CDEV */

/* generic low-level driver functions */

void		fb_dump_adp_info(char *driver, video_adapter_t *adp, int level);
void		fb_dump_mode_info(char *driver, video_adapter_t *adp,
				  video_info_t *info, int level);
int		fb_type(int adp_type);
int		fb_commonioctl(video_adapter_t *adp, u_long cmd, caddr_t arg);

#endif /* _KERNEL */

#endif /* !_DEV_FB_FBREG_H_ */
