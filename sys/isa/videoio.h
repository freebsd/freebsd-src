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
 * from: i386/isa/videoio.h,v 1.1
 */

#ifndef _I386_ISA_VIDEOIO_H_
#define _I386_ISA_VIDEOIO_H_

#ifdef KERNEL

#define V_MAX_ADAPTERS		2

#define V_MODE_MAP_SIZE		(M_VGA_CG320 + 1)
#define V_MODE_PARAM_SIZE	64

/* physical addresses */
#define MONO_BUF		BIOS_PADDRTOVADDR(0xb0000)
#define CGA_BUF			BIOS_PADDRTOVADDR(0xb8000)
#define GRAPHICS_BUF		BIOS_PADDRTOVADDR(0xa0000)
#define VIDEOMEM		0x000A0000

/* I/O port addresses */
#define MONO_BASE	0x3B4			/* crt controller base mono */
#define COLOR_BASE	0x3D4			/* crt controller base color */
#define MISC		0x3C2			/* misc output register */
#define ATC		IO_VGA+0x00		/* attribute controller */
#define TSIDX		IO_VGA+0x04		/* timing sequencer idx */
#define TSREG		IO_VGA+0x05		/* timing sequencer data */
#define PIXMASK		IO_VGA+0x06		/* pixel write mask */
#define PALRADR		IO_VGA+0x07		/* palette read address */
#define PALWADR		IO_VGA+0x08		/* palette write address */
#define PALDATA		IO_VGA+0x09		/* palette data register */
#define GDCIDX		IO_VGA+0x0E		/* graph data controller idx */
#define GDCREG		IO_VGA+0x0F		/* graph data controller data */

/* video function table */
typedef int vi_init_t(void);
typedef video_adapter_t *vi_adapter_t(int ad);
typedef int vi_get_info_t(int ad, int mode, video_info_t *info);
typedef int vi_query_mode_t(int ad, video_info_t *info);
typedef int vi_set_mode_t(int ad, int mode);
typedef int vi_save_font_t(int ad, int page, int size, u_char *data, 
			   int c, int count);
typedef int vi_load_font_t(int ad, int page, int size, u_char *data, 
			   int c, int count);
typedef int vi_show_font_t(int ad, int page);
typedef int vi_save_palette_t(int ad, u_char *palette);
typedef int vi_load_palette_t(int ad, u_char *palette);
typedef int vi_set_border_t(int ad, int border);
typedef int vi_save_state_t(int ad, void *p, size_t size);
typedef int vi_load_state_t(int ad, void *p);
typedef int vi_set_win_org_t(int ad, off_t offset);
typedef int vi_read_hw_cursor_t(int ad, int *col, int *row);
typedef int vi_set_hw_cursor_t(int ad, int col, int row);
typedef int vi_diag_t(int level);

struct vidsw {
    vi_init_t		*init;			/* all */
    vi_adapter_t	*adapter;		/* all */
    vi_get_info_t	*get_info;		/* all */
    vi_query_mode_t	*query_mode;		/* all */
    vi_set_mode_t	*set_mode;		/* EGA/VGA */
    vi_save_font_t	*save_font;		/* EGA/VGA */
    vi_load_font_t	*load_font;		/* EGA/VGA */
    vi_show_font_t	*show_font;		/* EGA/VGA */
    vi_save_palette_t	*save_palette;		/* VGA */
    vi_load_palette_t	*load_palette;		/* VGA */
    vi_set_border_t	*set_border;		/* CGA/EGA/VGA */
    vi_save_state_t	*save_state;		/* VGA */
    vi_load_state_t	*load_state;		/* EGA/VGA */
    vi_set_win_org_t	*set_win_org;		/* all */
    vi_read_hw_cursor_t	*read_hw_cursor;	/* all */
    vi_set_hw_cursor_t	*set_hw_cursor;		/* all */
    vi_diag_t		*diag;			/* all */
};

extern struct vidsw biosvidsw;

#endif /* KERNEL */

#endif /* !_I386_ISA_VIDEOIO_H_ */
