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
 */

#include "sc.h"
#include "opt_vesa.h"
#include "opt_vm86.h"

#if (NSC > 0 && defined(VESA) && defined(VM86)) || defined(VESA_MODULE)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/console.h>
#include <machine/md_var.h>
#include <machine/vm86.h>
#include <machine/pc/bios.h>
#include <machine/pc/vesa.h>

#include <i386/isa/videoio.h>

#ifdef VESA_MODULE
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>

MOD_MISC(vesa);
#endif

/* VESA video adapter state buffer stub */
struct adp_state {
	int		sig;
#define V_STATE_SIG	0x61736576
	u_char		regs[1];
};
typedef struct adp_state adp_state_t;

/* VESA video adapter */
static video_adapter_t *vesa_adp = NULL;
static int vesa_state_buf_size = 0;
static void *vesa_state_buf = NULL;

/* VESA functions */
static vi_init_t		vesa_init;
static vi_adapter_t		vesa_adapter;
static vi_get_info_t		vesa_get_info;
static vi_query_mode_t		vesa_query_mode;
static vi_set_mode_t		vesa_set_mode;
static vi_save_font_t		vesa_save_font;
static vi_load_font_t		vesa_load_font;
static vi_show_font_t		vesa_show_font;
static vi_save_palette_t	vesa_save_palette;
static vi_load_palette_t	vesa_load_palette;
static vi_set_border_t		vesa_set_border;
static vi_save_state_t		vesa_save_state;
static vi_load_state_t		vesa_load_state;
static vi_set_win_org_t		vesa_set_origin;
static vi_read_hw_cursor_t	vesa_read_hw_cursor;
static vi_set_hw_cursor_t	vesa_set_hw_cursor;
static vi_diag_t		vesa_diag;

static struct vidsw vesavidsw = {
	vesa_init,	vesa_adapter,	vesa_get_info,	vesa_query_mode,
	vesa_set_mode,	vesa_save_font,	vesa_load_font,	vesa_show_font,
	vesa_save_palette,vesa_load_palette,vesa_set_border,vesa_save_state,
	vesa_load_state,vesa_set_origin,vesa_read_hw_cursor,vesa_set_hw_cursor,
	vesa_diag,
};

static struct vidsw prevvidsw;

/* VESA BIOS video modes */
#define VESA_MAXMODES	64
#define EOT		(-1)
#define NA		(-2)

static video_info_t vesa_vmode[VESA_MAXMODES + 1] = {
	{ EOT, },
};

static int vesa_init_done = FALSE;
static int has_vesa_bios = FALSE;
static struct vesa_info *vesa_adp_info = NULL;
static u_int16_t *vesa_vmodetab = NULL;

/* local macros and functions */
#define BIOS_SADDRTOLADDR(p) ((((p) & 0xffff0000) >> 12) + ((p) & 0x0000ffff))

static int vesa_bios_get_mode(int mode, struct vesa_mode *vmode);
static int vesa_bios_set_mode(int mode);
static int vesa_bios_set_dac(int bits);
static int vesa_bios_save_palette(int start, int colors, u_char *palette);
static int vesa_bios_load_palette(int start, int colors, u_char *palette);
#define STATE_SIZE	0
#define STATE_SAVE	1
#define STATE_LOAD	2
#define STATE_HW	(1<<0)
#define STATE_DATA	(1<<1)
#define STATE_DAC	(1<<2)
#define STATE_REG	(1<<3)
#define STATE_MOST	(STATE_HW | STATE_DATA | STATE_REG)
#define STATE_ALL	(STATE_HW | STATE_DATA | STATE_DAC | STATE_REG)
static int vesa_bios_state_buf_size(void);
static int vesa_bios_save_restore(int code, void *p, size_t size);
static int translate_flags(u_int16_t vflags);
static int vesa_bios_init(void);
static void clear_modes(video_info_t *info, int color);

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

/* VESA BIOS calls */
static int
vesa_bios_get_mode(int mode, struct vesa_mode *vmode)
{
	struct vm86frame vmf;
	u_char buf[256];
	int err;

	bzero(&vmf, sizeof(vmf));
	bzero(buf, sizeof(buf));  
	vmf.vmf_eax = 0x4f01; 
	vmf.vmf_ecx = mode;
	err = vm86_datacall(0x10, &vmf, (char *)buf, sizeof(buf),
			  &vmf.vmf_es, &vmf.vmf_di);
	if ((err != 0) || (vmf.vmf_eax != 0x4f))
		return 1;
	bcopy(buf, vmode, sizeof(*vmode));
	return 0;
}

static int
vesa_bios_set_mode(int mode)
{
	struct vm86frame vmf;
	int err;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f02;
	vmf.vmf_ebx = mode;
	err = vm86_intcall(0x10, &vmf);
	return ((err != 0) || (vmf.vmf_eax != 0x4f));
}

static int
vesa_bios_set_dac(int bits)
{
	struct vm86frame vmf;
	int err;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f08;
	vmf.vmf_ebx = (bits << 8);
	err = vm86_intcall(0x10, &vmf);
	return ((err != 0) || (vmf.vmf_eax != 0x4f));
}

static int
vesa_bios_save_palette(int start, int colors, u_char *palette)
{
	struct vm86frame vmf;
	u_char *p;
	int err;
	int i;

	p = malloc(colors*4, M_DEVBUF, M_WAITOK);

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f09;
	vmf.vmf_ebx = 1;	/* get primary palette data */
	vmf.vmf_ecx = colors;
	vmf.vmf_edx = start;
	err = vm86_datacall(0x10, &vmf, p, colors*4, &vmf.vmf_es, &vmf.vmf_di);
	if ((err != 0) || (vmf.vmf_eax != 0x4f)) {
		free(p, M_DEVBUF);
		return 1;
	}

	for (i = 0; i < colors; ++i) {
		palette[i*3]     = p[i*4 + 1];
		palette[i*3 + 1] = p[i*4 + 2];
		palette[i*3 + 2] = p[i*4 + 3];
	}
	free(p, M_DEVBUF);
	return 0;
}

static int
vesa_bios_load_palette(int start, int colors, u_char *palette)
{
	struct vm86frame vmf;
	u_char *p;
	int err;
	int i;

	p = malloc(colors*4, M_DEVBUF, M_WAITOK);
	for (i = 0; i < colors; ++i) {
		p[i*4]     = 0;
		p[i*4 + 1] = palette[i*3];
		p[i*4 + 2] = palette[i*3 + 1];
		p[i*4 + 3] = palette[i*3 + 2];
	}

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f09;
	vmf.vmf_ebx = 0;	/* set primary palette data */
	vmf.vmf_ecx = colors;
	vmf.vmf_edx = start;
	err = vm86_datacall(0x10, &vmf, p, colors*4, &vmf.vmf_es, &vmf.vmf_di);
	free(p, M_DEVBUF);
	return ((err != 0) || (vmf.vmf_eax != 0x4f));
}

static int
vesa_bios_state_buf_size(void)
{
	struct vm86frame vmf;
	int err;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f04; 
	vmf.vmf_ecx = STATE_MOST;
	vmf.vmf_edx = STATE_SIZE;
	err = vm86_intcall(0x10, &vmf);
	if ((err != 0) || (vmf.vmf_eax != 0x4f))
		return 0;
	return vmf.vmf_ebx*64;
}

static int
vesa_bios_save_restore(int code, void *p, size_t size)
{
	struct vm86frame vmf;
	int err;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f04; 
	vmf.vmf_ecx = STATE_MOST;
	vmf.vmf_edx = code;	/* STATE_SAVE/STATE_LOAD */
	err = vm86_datacall(0x10, &vmf, (char *)p, size,
			  &vmf.vmf_es, &vmf.vmf_bx);
	return ((err != 0) || (vmf.vmf_eax != 0x4f));
}

static int
translate_flags(u_int16_t vflags)
{
	static struct {
		u_int16_t mask;
		int set;
		int reset;
	} ftable[] = {
		{ V_MODECOLOR, V_INFO_COLOR, 0 },
		{ V_MODEGRAPHICS, V_INFO_GRAPHICS, 0 },
		{ V_MODELFB, V_INFO_LENEAR, 0 },
	};
	int flags;
	int i;

	for (flags = 0, i = 0; i < sizeof(ftable)/sizeof(ftable[0]); ++i) {
		flags |= (vflags & ftable[i].mask) ? 
			 ftable[i].set : ftable[i].reset;
	}
	return flags;
}

static int
vesa_bios_init(void)
{
	static u_char buf[512];
	struct vm86frame vmf;
	struct vesa_mode vmode;
	u_int32_t p;
	int modes;
	int err;
	int i;

	if (vesa_init_done)
		return 0;

	has_vesa_bios = FALSE;
	vesa_adp_info = NULL;
	vesa_vmode[0].vi_mode = EOT;

	bzero(&vmf, sizeof(vmf));	/* paranoia */
	bzero(buf, sizeof(buf));
	bcopy("VBE2", buf, 4);		/* try for VBE2 data */
	vmf.vmf_eax = 0x4f00;
	err = vm86_datacall(0x10, &vmf, (char *)buf, sizeof(buf), 
			  &vmf.vmf_es, &vmf.vmf_di);
	if ((err != 0) || (vmf.vmf_eax != 0x4f) || bcmp("VESA", buf, 4))
		return 1;
	vesa_adp_info = (struct vesa_info *)buf;
	if (bootverbose)
		dump_buffer(buf, 64);
	if (vesa_adp_info->v_flags & V_NONVGA)
		return 1;

	/* obtain video mode information */
	p = BIOS_SADDRTOLADDR(vesa_adp_info->v_modetable);
	vesa_vmodetab = (u_int16_t *)BIOS_PADDRTOVADDR(p);
	for (i = 0, modes = 0; vesa_vmodetab[i] != 0xffff; ++i) {
		if (modes >= VESA_MAXMODES)
			break;
		if (vesa_bios_get_mode(vesa_vmodetab[i], &vmode))
			continue;

		/* reject unsupported modes */
#if 0
		if ((vmode.v_modeattr & (V_MODESUPP | V_MODEOPTINFO 
					| V_MODENONVGA))
		    != (V_MODESUPP | V_MODEOPTINFO))
			continue;
#else
		if ((vmode.v_modeattr & (V_MODEOPTINFO | V_MODENONVGA))
		    != (V_MODEOPTINFO))
			continue;
#endif

		/* copy some fields */
		bzero(&vesa_vmode[modes], sizeof(vesa_vmode[modes]));
		vesa_vmode[modes].vi_mode = vesa_vmodetab[i];
		vesa_vmode[modes].vi_width = vmode.v_width;
		vesa_vmode[modes].vi_height = vmode.v_height;
		vesa_vmode[modes].vi_depth = vmode.v_bpp;
		vesa_vmode[modes].vi_planes = vmode.v_planes;
		vesa_vmode[modes].vi_cwidth = vmode.v_cwidth;
		vesa_vmode[modes].vi_cheight = vmode.v_cheight;
		vesa_vmode[modes].vi_window = (u_int)vmode.v_waseg << 4;
		/* XXX window B */
		vesa_vmode[modes].vi_window_size = vmode.v_wsize;
		vesa_vmode[modes].vi_window_gran = vmode.v_wgran;
		vesa_vmode[modes].vi_buffer = vmode.v_lfb;
		vesa_vmode[modes].vi_buffer_size = vmode.v_offscreen;
		/* pixel format, memory model... */
		vesa_vmode[modes].vi_flags = translate_flags(vmode.v_modeattr)
					     | V_INFO_VESA;
		++modes;
	}
	vesa_vmode[modes].vi_mode = EOT;
	if (bootverbose)
		printf("VESA: %d mode(s) found\n", modes);

	has_vesa_bios = TRUE;
	return 0;
}

static void
clear_modes(video_info_t *info, int color)
{
	while (info->vi_mode != EOT) {
		if ((info->vi_flags & V_INFO_COLOR) != color)
			info->vi_mode = NA;
		++info;
	}
}

/* exported functions */

static int
vesa_init(void)
{
	int adapters;
	int i;

	adapters = (*prevvidsw.init)();
	for (i = 0; i < adapters; ++i) {
		if ((vesa_adp = (*prevvidsw.adapter)(i)) == NULL)
			continue;
		if (vesa_adp->va_type == KD_VGA) {
			vesa_adp->va_flags |= V_ADP_VESA;
			return adapters;
		}
	}
	vesa_adp = NULL;
	return adapters;
}

static video_adapter_t
*vesa_adapter(int ad)
{
	return (*prevvidsw.adapter)(ad);
}

static int
vesa_get_info(int ad, int mode, video_info_t *info)
{
	int i;

	if ((*prevvidsw.get_info)(ad, mode, info) == 0)
		return 0;

	if (ad != vesa_adp->va_index)
		return 1;
	for (i = 0; vesa_vmode[i].vi_mode != EOT; ++i) {
		if (vesa_vmode[i].vi_mode == NA)
			continue;
		if (vesa_vmode[i].vi_mode == mode) {
			*info = vesa_vmode[i];
			return 0;
		}
	}
	return 1;
}

static int
vesa_query_mode(int ad, video_info_t *info)
{
	int i;

	if ((i = (*prevvidsw.query_mode)(ad, info)) != -1)
		return i;
	if (ad != vesa_adp->va_index)
		return -1;

	for (i = 0; vesa_vmode[i].vi_mode != EOT; ++i) {
		if ((info->vi_width != 0)
		    && (info->vi_width != vesa_vmode[i].vi_width))
			continue;
		if ((info->vi_height != 0)
		    && (info->vi_height != vesa_vmode[i].vi_height))
			continue;
		if ((info->vi_cwidth != 0)
		    && (info->vi_cwidth != vesa_vmode[i].vi_cwidth))
			continue;
		if ((info->vi_cheight != 0)
		    && (info->vi_cheight != vesa_vmode[i].vi_cheight))
			continue;
		if ((info->vi_depth != 0)
		    && (info->vi_depth != vesa_vmode[i].vi_depth))
			continue;
		if ((info->vi_planes != 0)
		    && (info->vi_planes != vesa_vmode[i].vi_planes))
			continue;
		/* pixel format, memory model */
		if ((info->vi_flags != 0)
		    && (info->vi_flags != vesa_vmode[i].vi_flags))
			continue;
		return vesa_vmode[i].vi_mode;
	}
	return -1;
}

static int
vesa_set_mode(int ad, int mode)
{
	video_info_t info;
	size_t len;

	if (ad != vesa_adp->va_index)
		return (*prevvidsw.set_mode)(ad, mode);

#ifdef SC_VIDEO_DEBUG
	printf("VESA: set_mode(): %d(%x) -> %d(%x)\n",
		vesa_adp->va_mode, vesa_adp->va_mode, mode, mode);
#endif
	/* 
	 * If the current mode is a VESA mode and the new mode is not,
	 * restore the state of the adapter first, so that non-standard, 
	 * extended SVGA registers are set to the state compatible with
	 * the standard VGA modes. Otherwise (*prevvidsw.set_mode)() 
	 * may not be able to set up the new mode correctly.
	 */
	if (VESA_MODE(vesa_adp->va_mode)) {
		if ((*prevvidsw.get_info)(ad, mode, &info) == 0) {
			/* assert(vesa_state_buf != NULL); */
		    	if ((vesa_state_buf == NULL)
			    || vesa_load_state(ad, vesa_state_buf))
				return 1;
			free(vesa_state_buf, M_DEVBUF);
			vesa_state_buf = NULL;
#ifdef SC_VIDEO_DEBUG
			printf("VESA: restored\n");
#endif
		}
		/* 
		 * once (*prevvidsw.get_info)() succeeded, 
		 * (*prevvidsw.set_mode)() below won't fail...
		 */
	}

	/* we may not need to handle this mode after all... */
	if ((*prevvidsw.set_mode)(ad, mode) == 0)
		return 0;

	/* is the new mode supported? */
	if (vesa_get_info(ad, mode, &info))
		return 1;
	/* assert(VESA_MODE(mode)); */

#ifdef SC_VIDEO_DEBUG
	printf("VESA: about to set a VESA mode...\n");
#endif
	/* 
	 * If the current mode is not a VESA mode, save the current state
	 * so that the adapter state can be restored later when a non-VESA
	 * mode is to be set up. See above.
	 */
	if (!VESA_MODE(vesa_adp->va_mode) && (vesa_state_buf == NULL)) {
		len = vesa_save_state(ad, NULL, 0);
		vesa_state_buf = malloc(len, M_DEVBUF, M_WAITOK);
		if (vesa_save_state(ad, vesa_state_buf, len)) {
#ifdef SC_VIDEO_DEBUG
			printf("VESA: state save failed! (len=%d)\n", len);
#endif
			free(vesa_state_buf, M_DEVBUF);
			vesa_state_buf = NULL;
			return 1;
		}
#ifdef SC_VIDEO_DEBUG
		printf("VESA: saved (len=%d)\n", len);
		dump_buffer(vesa_state_buf, len);
#endif
	}

	if (vesa_bios_set_mode(mode))
		return 1;

#ifdef SC_VIDEO_DEBUG
	printf("VESA: mode set!\n");
#endif
	vesa_adp->va_mode = mode;
	vesa_adp->va_flags &= ~V_ADP_COLOR;
	vesa_adp->va_flags |= 
		(info.vi_flags & V_INFO_COLOR) ? V_ADP_COLOR : 0;
	vesa_adp->va_crtc_addr =
		(vesa_adp->va_flags & V_ADP_COLOR) ? COLOR_BASE : MONO_BASE;
	vesa_adp->va_window = BIOS_PADDRTOVADDR(info.vi_window);
	vesa_adp->va_window_size = info.vi_window_size;
	vesa_adp->va_window_gran = info.vi_window_gran;
	if (info.vi_buffer_size == 0) {
		vesa_adp->va_buffer = 0;
		vesa_adp->va_buffer_size = 0;
	} else {
		vesa_adp->va_buffer = BIOS_PADDRTOVADDR(info.vi_buffer);
		vesa_adp->va_buffer_size = info.vi_buffer_size;
	}

	return 0;
}

static int
vesa_save_font(int ad, int page, int fontsize, u_char *data, int ch, int count)
{
	return (*prevvidsw.save_font)(ad, page, fontsize, data, ch, count);
}

static int
vesa_load_font(int ad, int page, int fontsize, u_char *data, int ch, int count)
{
	return (*prevvidsw.load_font)(ad, page, fontsize, data, ch, count);
}

static int
vesa_show_font(int ad, int page)
{
	return (*prevvidsw.show_font)(ad, page);
}

static int
vesa_save_palette(int ad, u_char *palette)
{
	if ((ad != vesa_adp->va_index) || !(vesa_adp_info->v_flags & V_DAC8) 
	    || vesa_bios_set_dac(8))
		return (*prevvidsw.save_palette)(ad, palette);

	return vesa_bios_save_palette(0, 256, palette);
}

static int
vesa_load_palette(int ad, u_char *palette)
{
	if ((ad != vesa_adp->va_index) || !(vesa_adp_info->v_flags & V_DAC8) 
	    || vesa_bios_set_dac(8))
		return (*prevvidsw.load_palette)(ad, palette);

	return vesa_bios_load_palette(0, 256, palette);
}

static int
vesa_set_border(int ad, int color)
{
	return (*prevvidsw.set_border)(ad, color);
}

static int
vesa_save_state(int ad, void *p, size_t size)
{
	if (ad != vesa_adp->va_index)
		return (*prevvidsw.save_state)(ad, p, size);

	if (vesa_state_buf_size == 0)
		vesa_state_buf_size = vesa_bios_state_buf_size();
	if (size == 0)
		return (sizeof(int) + vesa_state_buf_size);
	else if (size < (sizeof(int) + vesa_state_buf_size))
		return 1;

	((adp_state_t *)p)->sig = V_STATE_SIG;
	bzero(((adp_state_t *)p)->regs, vesa_state_buf_size);
	return vesa_bios_save_restore(STATE_SAVE, ((adp_state_t *)p)->regs, 
				      vesa_state_buf_size);
}

static int
vesa_load_state(int ad, void *p)
{
	if ((ad != vesa_adp->va_index) 
	    || (((adp_state_t *)p)->sig != V_STATE_SIG))
		return (*prevvidsw.load_state)(ad, p);

	return vesa_bios_save_restore(STATE_LOAD, ((adp_state_t *)p)->regs, 
				      vesa_state_buf_size);
}

static int
vesa_set_origin(int ad, off_t offset)
{
	struct vm86frame vmf;
	int err;

	/*
	 * This function should return as quickly as possible to 
	 * maintain good performance of the system. For this reason,
	 * error checking is kept minimal and let the VESA BIOS to 
	 * detect error.
	 */
	if (ad != vesa_adp->va_index) 
		return (*prevvidsw.set_win_org)(ad, offset);

	if (vesa_adp->va_window_gran == 0)
		return 1;
	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f05; 
	vmf.vmf_ebx = 0;		/* WINDOW_A, XXX */
	vmf.vmf_edx = offset/vesa_adp->va_window_gran;
	err = vm86_intcall(0x10, &vmf); 
	return ((err != 0) || (vmf.vmf_eax != 0x4f));
}

static int
vesa_read_hw_cursor(int ad, int *col, int *row)
{
	return (*prevvidsw.read_hw_cursor)(ad, col, row);
}

static int
vesa_set_hw_cursor(int ad, int col, int row)
{
	return (*prevvidsw.set_hw_cursor)(ad, col, row);
}

static int
vesa_diag(int level)
{
	struct vesa_mode vmode;
	u_int32_t p;
	int i;

	/* general adapter information */
	printf("VESA: v%d.%d, %dk memory, flags:0x%x, mode table:%p (%x)\n", 
	       ((vesa_adp_info->v_version & 0xf000) >> 12) * 10 
		   + ((vesa_adp_info->v_version & 0x0f00) >> 8),
	       ((vesa_adp_info->v_version & 0x00f0) >> 4) * 10 
		   + (vesa_adp_info->v_version & 0x000f),
	       vesa_adp_info->v_memsize * 64, vesa_adp_info->v_flags,
	       vesa_vmodetab, vesa_adp_info->v_modetable);
	/* OEM string */
	p = BIOS_SADDRTOLADDR(vesa_adp_info->v_oemstr);
	if (p != 0)
		printf("VESA: %s\n", (char *)BIOS_PADDRTOVADDR(p));

	if (level <= 0)
		return 0;

	if (vesa_adp_info->v_version >= 0x0200) {
		/* vendor name */
		p = BIOS_SADDRTOLADDR(vesa_adp_info->v_venderstr);
		if (p != 0)
			printf("VESA: %s, ", (char *)BIOS_PADDRTOVADDR(p));
		/* product name */
		p = BIOS_SADDRTOLADDR(vesa_adp_info->v_prodstr);
		if (p != 0)
			printf("%s, ", (char *)BIOS_PADDRTOVADDR(p));
		/* product revision */
		p = BIOS_SADDRTOLADDR(vesa_adp_info->v_revstr);
		if (p != 0)
			printf("%s\n", (char *)BIOS_PADDRTOVADDR(p));
	}

	/* mode information */
	for (i = 0; vesa_vmodetab[i] != 0xffff; ++i) {
		if (vesa_bios_get_mode(vesa_vmodetab[i], &vmode))
			continue;

		/* print something for diagnostic purpose */
		printf("VESA: mode:0x%03x, flags:0x%04x", 
		       vesa_vmodetab[i], vmode.v_modeattr);
		if (vmode.v_modeattr & V_MODEOPTINFO) {
			if (vmode.v_modeattr & V_MODEGRAPHICS) {
				printf(", G %dx%dx%d %d, ", 
				       vmode.v_width, vmode.v_height,
				       vmode.v_bpp, vmode.v_planes);
			} else {
				printf(", T %dx%d, ", 
				       vmode.v_width, vmode.v_height);
			}
			printf("font:%dx%d", 
			       vmode.v_cwidth, vmode.v_cheight);
		}
		if (vmode.v_modeattr & V_MODELFB) {
			printf(", mem:%d, LFB:0x%x, off:0x%x", 
			       vmode.v_memmodel, vmode.v_lfb, 
			       vmode.v_offscreen);
		}
		printf("\n");
		printf("VESA: window A:0x%x (%x), window B:0x%x (%x), ",
		       vmode.v_waseg, vmode.v_waattr,
		       vmode.v_wbseg, vmode.v_wbattr);
		printf("size:%dk, gran:%dk\n",
		       vmode.v_wsize, vmode.v_wgran);
	}

	return 0;
}

/* module loading */

#ifdef VESA_MODULE
static int
vesa_load(struct lkm_table *lkmtp, int cmd)
#else
int
vesa_load(void)
#endif
{
	int adapters;
	int error;
	int s;
	int i;

	if (vesa_init_done)
		return 0;

	/*
	 * If the VESA module is statically linked to the kernel, or
	 * it has already been loaded, abort loading this module this time.
	 */
	vesa_adp = NULL;
	adapters = (*biosvidsw.init)();
	for (i = 0; i < adapters; ++i) {
		if ((vesa_adp = (*biosvidsw.adapter)(i)) == NULL)
			continue;
		if (vesa_adp->va_flags & V_ADP_VESA)
			return ENXIO;
		if (vesa_adp->va_type == KD_VGA)
			break;
	}
	/* if a VGA adapter is not found, abort */
	if (i >= adapters)
		return ENXIO;

	if (vesa_bios_init())
		return ENXIO;
	vesa_adp->va_flags |= V_ADP_VESA;

	/* remove conflicting modes if we have more than one adapter */
	if (adapters > 1) {
		clear_modes(vesa_vmode,
			    (vesa_adp->va_flags & V_ADP_COLOR) ? 
				V_INFO_COLOR : 0);
	}

#ifdef VESA_MODULE
	s = spltty();
#endif
	if ((error = vesa_load_ioctl()) == 0) {
		bcopy(&biosvidsw, &prevvidsw, sizeof(prevvidsw));
		bcopy(&vesavidsw, &biosvidsw, sizeof(vesavidsw));
		vesa_init_done = TRUE;
	}
#ifdef VESA_MODULE
	splx(s);

	if (error == 0)
		vesa_diag(bootverbose);
#endif

	return error;
}

#ifdef VESA_MODULE

static int
vesa_unload(struct lkm_table *lkmtp, int cmd)
{
	int error;
	int s;

	/* if the adapter is currently in a VESA mode, don't unload */
	if ((vesa_adp != NULL) && VESA_MODE(vesa_adp->va_mode))
		return EBUSY;
	/* 
	 * FIXME: if there is at least one vty which is in a VESA mode,
	 * we shouldn't be unloading! XXX
	 */

	s = spltty();
	if ((error = vesa_unload_ioctl()) == 0) {
		if (vesa_adp)
			vesa_adp->va_flags &= ~V_ADP_VESA;
		bcopy(&prevvidsw, &biosvidsw, sizeof(biosvidsw));
	}
	splx(s);

	return error;
}

int
vesa_mod(struct lkm_table *lkmtp, int cmd, int ver)
{
	MOD_DISPATCH(vesa, lkmtp, cmd, ver,
		vesa_load, vesa_unload, lkm_nullcmd);
}

#endif /* VESA_MODULE */

#endif /* (NSC > 0 && VESA && VM86) || VESA_MODULE */
