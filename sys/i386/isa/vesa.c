/*-
 * Copyright (c) 1998 Kazutaka YOKOTA and Michael Smith
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
 * $Id: vesa.c,v 1.22 1999/03/31 15:27:00 yokota Exp $
 */

#include "vga.h"
#include "opt_vga.h"
#include "opt_vesa.h"
#include "opt_vm86.h"
#include "opt_fb.h"

#ifdef VGA_NO_MODE_CHANGE
#undef VESA
#endif

#if (NVGA > 0 && defined(VESA) && defined(VM86)) || defined(KLD_MODULE)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/console.h>
#include <machine/md_var.h>
#include <machine/vm86.h>
#include <machine/pc/bios.h>
#include <machine/pc/vesa.h>

#include <dev/fb/fbreg.h>
#include <dev/fb/vgareg.h>

#ifndef __i386__
#include <isa/isareg.h>
#else
#include <i386/isa/isa.h>
#endif

#ifndef VESA_DEBUG
#define VESA_DEBUG	0
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
#if 0
static void *vesa_state_buf = NULL;
#endif

/* VESA functions */
static int			vesa_nop(void);
static vi_probe_t		vesa_probe;
static vi_init_t		vesa_init;
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
static vi_set_hw_cursor_shape_t	vesa_set_hw_cursor_shape;
static vi_mmap_t		vesa_mmap;
static vi_diag_t		vesa_diag;
static struct vm86context	vesa_vmcontext;

static video_switch_t vesavidsw = {
	vesa_probe,
	vesa_init,
	vesa_get_info,
	vesa_query_mode,
	vesa_set_mode,
	vesa_save_font,
	vesa_load_font,
	vesa_show_font,
	vesa_save_palette,
	vesa_load_palette,
	vesa_set_border,
	vesa_save_state,
	vesa_load_state,
	vesa_set_origin,
	vesa_read_hw_cursor,
	vesa_set_hw_cursor,
	vesa_set_hw_cursor_shape,
	(vi_blank_display_t *)vesa_nop,
	vesa_mmap,
	vesa_diag,
};

static video_switch_t *prevvidsw;

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
static char *vesa_oemstr = NULL;
static char *vesa_venderstr = NULL;
static char *vesa_prodstr = NULL;
static char *vesa_revstr = NULL;

/* local macros and functions */
#define BIOS_SADDRTOLADDR(p) ((((p) & 0xffff0000) >> 12) + ((p) & 0x0000ffff))

static int int10_set_mode(int mode);
static int vesa_bios_get_mode(int mode, struct vesa_mode *vmode);
static int vesa_bios_set_mode(int mode);
static int vesa_bios_get_dac(void);
static int vesa_bios_set_dac(int bits);
static int vesa_bios_save_palette(int start, int colors, u_char *palette,
				  int bits);
static int vesa_bios_load_palette(int start, int colors, u_char *palette,
				  int bits);
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
static int vesa_bios_get_line_length(void);
static int vesa_map_gen_mode_num(int type, int color, int mode);
static int vesa_translate_flags(u_int16_t vflags);
static void *vesa_fix_ptr(u_int32_t p, u_int16_t seg, u_int16_t off, 
			  u_char *buf);
static int vesa_bios_init(void);
static void vesa_clear_modes(video_info_t *info, int color);

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

/* INT 10 BIOS calls */
static int
int10_set_mode(int mode)
{
	struct vm86frame vmf;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x0000 | mode;
	vm86_intcall(0x10, &vmf);
	return 0;
}

/* VESA BIOS calls */
static int
vesa_bios_get_mode(int mode, struct vesa_mode *vmode)
{
	struct vm86frame vmf;
	u_char *buf;
	int err;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f01; 
	vmf.vmf_ecx = mode;
	buf = (u_char *)vm86_getpage(&vesa_vmcontext, 1);
	vm86_getptr(&vesa_vmcontext, (vm_offset_t)buf, &vmf.vmf_es, &vmf.vmf_di);

	err = vm86_datacall(0x10, &vmf, &vesa_vmcontext);
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
vesa_bios_get_dac(void)
{
	struct vm86frame vmf;
	int err;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f08;
	vmf.vmf_ebx = 1;	/* get DAC width */
	err = vm86_intcall(0x10, &vmf);
	if ((err != 0) || (vmf.vmf_eax != 0x4f))
		return 6;	/* XXX */
	return ((vmf.vmf_ebx >> 8) & 0x00ff);
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
	if ((err != 0) || (vmf.vmf_eax != 0x4f))
		return 6;	/* XXX */
	return ((vmf.vmf_ebx >> 8) & 0x00ff);
}

static int
vesa_bios_save_palette(int start, int colors, u_char *palette, int bits)
{
	struct vm86frame vmf;
	u_char *p;
	int err;
	int i;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f09;
	vmf.vmf_ebx = 1;	/* get primary palette data */
	vmf.vmf_ecx = colors;
	vmf.vmf_edx = start;
	p = (u_char *)vm86_getpage(&vesa_vmcontext, 1);
	vm86_getptr(&vesa_vmcontext, (vm_offset_t)p, &vmf.vmf_es, &vmf.vmf_di);

	err = vm86_datacall(0x10, &vmf, &vesa_vmcontext);
	if ((err != 0) || (vmf.vmf_eax != 0x4f))
		return 1;

	bits = 8 - bits;
	for (i = 0; i < colors; ++i) {
		palette[i*3]     = p[i*4 + 2] << bits;
		palette[i*3 + 1] = p[i*4 + 1] << bits;
		palette[i*3 + 2] = p[i*4] << bits;
	}
	return 0;
}

static int
vesa_bios_load_palette(int start, int colors, u_char *palette, int bits)
{
	struct vm86frame vmf;
	u_char *p;
	int err;
	int i;

	p = (u_char *)vm86_getpage(&vesa_vmcontext, 1);
	bits = 8 - bits;
	for (i = 0; i < colors; ++i) {
		p[i*4]	   = palette[i*3 + 2] >> bits;
		p[i*4 + 1] = palette[i*3 + 1] >> bits;
		p[i*4 + 2] = palette[i*3] >> bits;
		p[i*4 + 3] = 0;
	}

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f09;
	vmf.vmf_ebx = 0;	/* set primary palette data */
	vmf.vmf_ecx = colors;
	vmf.vmf_edx = start;
	vm86_getptr(&vesa_vmcontext, (vm_offset_t)p, &vmf.vmf_es, &vmf.vmf_di);

	err = vm86_datacall(0x10, &vmf, &vesa_vmcontext);
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
	u_char *buf;
	int err;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f04; 
	vmf.vmf_ecx = STATE_MOST;
	vmf.vmf_edx = code;	/* STATE_SAVE/STATE_LOAD */
	buf = (u_char *)vm86_getpage(&vesa_vmcontext, 1);
	vm86_getptr(&vesa_vmcontext, (vm_offset_t)buf, &vmf.vmf_es, &vmf.vmf_di);
	bcopy(p, buf, size);

	err = vm86_datacall(0x10, &vmf, &vesa_vmcontext);
	return ((err != 0) || (vmf.vmf_eax != 0x4f));
}

static int
vesa_bios_get_line_length(void)
{
	struct vm86frame vmf;
	int err;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f06; 
	vmf.vmf_ebx = 1;	/* get scan line length */
	err = vm86_intcall(0x10, &vmf);
	if ((err != 0) || (vmf.vmf_eax != 0x4f))
		return -1;
	return vmf.vmf_bx;	/* line length in bytes */
}

/* map a generic video mode to a known mode */
static int
vesa_map_gen_mode_num(int type, int color, int mode)
{
    static struct {
	int from;
	int to;
    } mode_map[] = {
	{ M_TEXT_132x25, M_VESA_C132x25 },
	{ M_TEXT_132x43, M_VESA_C132x43 },
	{ M_TEXT_132x50, M_VESA_C132x50 },
	{ M_TEXT_132x60, M_VESA_C132x60 },
    };
    int i;

    for (i = 0; i < sizeof(mode_map)/sizeof(mode_map[0]); ++i) {
        if (mode_map[i].from == mode)
            return mode_map[i].to;
    }
    return mode;
}

static int
vesa_translate_flags(u_int16_t vflags)
{
	static struct {
		u_int16_t mask;
		int set;
		int reset;
	} ftable[] = {
		{ V_MODECOLOR, V_INFO_COLOR, 0 },
		{ V_MODEGRAPHICS, V_INFO_GRAPHICS, 0 },
		{ V_MODELFB, V_INFO_LINEAR, 0 },
	};
	int flags;
	int i;

	for (flags = 0, i = 0; i < sizeof(ftable)/sizeof(ftable[0]); ++i) {
		flags |= (vflags & ftable[i].mask) ? 
			 ftable[i].set : ftable[i].reset;
	}
	return flags;
}

static void
*vesa_fix_ptr(u_int32_t p, u_int16_t seg, u_int16_t off, u_char *buf)
{
	if (p == 0)
		return NULL;
	if (((p >> 16) == seg) && ((p & 0xffff) >= off))
		return (void *)(buf + ((p & 0xffff) - off));
	else {
		p = BIOS_SADDRTOLADDR(p);
		return (void *)BIOS_PADDRTOVADDR(p);
	}
}

static int
vesa_bios_init(void)
{
	static u_char buf[512];
	struct vm86frame vmf;
	struct vesa_mode vmode;
	u_char *vmbuf;
	int modes;
	int err;
	int i;

	if (vesa_init_done)
		return 0;

	has_vesa_bios = FALSE;
	vesa_adp_info = NULL;
	vesa_vmode[0].vi_mode = EOT;

	vmbuf = (u_char *)vm86_addpage(&vesa_vmcontext, 1, 0);
	bzero(&vmf, sizeof(vmf));	/* paranoia */
	bcopy("VBE2", vmbuf, 4);	/* try for VBE2 data */
	vmf.vmf_eax = 0x4f00;
	vm86_getptr(&vesa_vmcontext, (vm_offset_t)vmbuf, &vmf.vmf_es, &vmf.vmf_di);

	err = vm86_datacall(0x10, &vmf, &vesa_vmcontext);
	if ((err != 0) || (vmf.vmf_eax != 0x4f) || bcmp("VESA", vmbuf, 4))
		return 1;
	bcopy(vmbuf, buf, sizeof(buf));
	vesa_adp_info = (struct vesa_info *)buf;
	if (bootverbose) {
		printf("VESA: information block\n");
		dump_buffer(buf, 64);
	}
	if (vesa_adp_info->v_flags & V_NONVGA)
		return 1;

	/* fix string ptrs */
	vesa_oemstr = (char *)vesa_fix_ptr(vesa_adp_info->v_oemstr,
					   vmf.vmf_es, vmf.vmf_di, buf);
	if (vesa_adp_info->v_version >= 0x0200) {
		vesa_venderstr = 
		    (char *)vesa_fix_ptr(vesa_adp_info->v_venderstr,
					 vmf.vmf_es, vmf.vmf_di, buf);
		vesa_prodstr = 
		    (char *)vesa_fix_ptr(vesa_adp_info->v_prodstr,
					 vmf.vmf_es, vmf.vmf_di, buf);
		vesa_revstr = 
		    (char *)vesa_fix_ptr(vesa_adp_info->v_revstr,
					 vmf.vmf_es, vmf.vmf_di, buf);
	}

	/* obtain video mode information */
	vesa_vmode[0].vi_mode = EOT;
	vesa_vmodetab = (u_int16_t *)vesa_fix_ptr(vesa_adp_info->v_modetable,
						  vmf.vmf_es, vmf.vmf_di, buf);
	if (vesa_vmodetab == NULL)
		return 1;
	for (i = 0, modes = 0; 
		(i < (M_VESA_MODE_MAX - M_VESA_BASE + 1))
		&& (vesa_vmodetab[i] != 0xffff); ++i) {
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
		vesa_vmode[modes].vi_window_size = vmode.v_wsize*1024;
		vesa_vmode[modes].vi_window_gran = vmode.v_wgran*1024;
		vesa_vmode[modes].vi_buffer = vmode.v_lfb;
		/* XXX */
		if (vmode.v_offscreen > vmode.v_lfb)
			vesa_vmode[modes].vi_buffer_size
				= vmode.v_offscreen - vmode.v_lfb;
		else
			vesa_vmode[modes].vi_buffer_size = vmode.v_offscreen;
		/* pixel format, memory model... */

		vesa_vmode[modes].vi_flags 
			= vesa_translate_flags(vmode.v_modeattr) | V_INFO_VESA;
		++modes;
	}
	vesa_vmode[modes].vi_mode = EOT;
	if (bootverbose)
		printf("VESA: %d mode(s) found\n", modes);

	has_vesa_bios = (modes > 0);
	return (has_vesa_bios ? 0 : 1);
}

static void
vesa_clear_modes(video_info_t *info, int color)
{
	while (info->vi_mode != EOT) {
		if ((info->vi_flags & V_INFO_COLOR) != color)
			info->vi_mode = NA;
		++info;
	}
}

/* entry points */

static int
vesa_configure(int flags)
{
	video_adapter_t *adp;
	int adapters;
	int error;
	int i;

	if (vesa_init_done)
		return 0;
	if (flags & VIO_PROBE_ONLY)
		return 0;		/* XXX */

	/*
	 * If the VESA module has already been loaded, abort loading 
	 * the module this time.
	 */
	for (i = 0; (adp = vid_get_adapter(i)) != NULL; ++i) {
		if (adp->va_flags & V_ADP_VESA)
			return ENXIO;
		if (adp->va_type == KD_VGA)
			break;
	}
	/*
	 * The VGA adapter is not found.  This is because either 
	 * 1) the VGA driver has not been initialized, or 2) the VGA card
	 * is not present.  If 1) is the case, we shall defer
	 * initialization for now and try again later.
	 */
	if (adp == NULL) {
		vga_sub_configure = vesa_configure;
		return ENODEV;
	}

	/* count number of registered adapters */
	for (++i; vid_get_adapter(i) != NULL; ++i)
		;
	adapters = i;

	/* call VESA BIOS */
	vesa_adp = adp;
	if (vesa_bios_init()) {
		vesa_adp = NULL;
		return ENXIO;
	}
	vesa_adp->va_flags |= V_ADP_VESA;

	/* remove conflicting modes if we have more than one adapter */
	if (adapters > 1) {
		vesa_clear_modes(vesa_vmode,
				 (vesa_adp->va_flags & V_ADP_COLOR) ? 
				     V_INFO_COLOR : 0);
	}

	if ((error = vesa_load_ioctl()) == 0) {
		prevvidsw = vidsw[vesa_adp->va_index];
		vidsw[vesa_adp->va_index] = &vesavidsw;
		vesa_init_done = TRUE;
	} else {
		vesa_adp = NULL;
		return error;
	}

	return 0;
}

static int
vesa_nop(void)
{
	return 0;
}

static int
vesa_probe(int unit, video_adapter_t **adpp, void *arg, int flags)
{
	return (*prevvidsw->probe)(unit, adpp, arg, flags);
}

static int
vesa_init(int unit, video_adapter_t *adp, int flags)
{
	return (*prevvidsw->init)(unit, adp, flags);
}

static int
vesa_get_info(video_adapter_t *adp, int mode, video_info_t *info)
{
	int i;

	if ((*prevvidsw->get_info)(adp, mode, info) == 0)
		return 0;

	if (adp != vesa_adp)
		return 1;

	mode = vesa_map_gen_mode_num(vesa_adp->va_type, 
				     vesa_adp->va_flags & V_ADP_COLOR, mode);
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
vesa_query_mode(video_adapter_t *adp, video_info_t *info)
{
	int i;

	if ((i = (*prevvidsw->query_mode)(adp, info)) != -1)
		return i;
	if (adp != vesa_adp)
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
vesa_set_mode(video_adapter_t *adp, int mode)
{
	video_info_t info;
	size_t len;

	if (adp != vesa_adp)
		return (*prevvidsw->set_mode)(adp, mode);

	mode = vesa_map_gen_mode_num(vesa_adp->va_type, 
				     vesa_adp->va_flags & V_ADP_COLOR, mode);
#if VESA_DEBUG > 0
	printf("VESA: set_mode(): %d(%x) -> %d(%x)\n",
		vesa_adp->va_mode, vesa_adp->va_mode, mode, mode);
#endif
	/* 
	 * If the current mode is a VESA mode and the new mode is not,
	 * restore the state of the adapter first, so that non-standard, 
	 * extended SVGA registers are set to the state compatible with
	 * the standard VGA modes. Otherwise (*prevvidsw->set_mode)() 
	 * may not be able to set up the new mode correctly.
	 */
	if (VESA_MODE(vesa_adp->va_mode)) {
		if ((*prevvidsw->get_info)(adp, mode, &info) == 0) {
			int10_set_mode(vesa_adp->va_initial_bios_mode);
#if 0
			/* assert(vesa_state_buf != NULL); */
		    	if ((vesa_state_buf == NULL)
			    || vesa_load_state(adp, vesa_state_buf))
				return 1;
			free(vesa_state_buf, M_DEVBUF);
			vesa_state_buf = NULL;
#if VESA_DEBUG > 0
			printf("VESA: restored\n");
#endif
#endif /* 0 */
		}
		/* 
		 * once (*prevvidsw->get_info)() succeeded, 
		 * (*prevvidsw->set_mode)() below won't fail...
		 */
	}

	/* we may not need to handle this mode after all... */
	if ((*prevvidsw->set_mode)(adp, mode) == 0)
		return 0;

	/* is the new mode supported? */
	if (vesa_get_info(adp, mode, &info))
		return 1;
	/* assert(VESA_MODE(mode)); */

#if VESA_DEBUG > 0
	printf("VESA: about to set a VESA mode...\n");
#endif
	/* 
	 * If the current mode is not a VESA mode, save the current state
	 * so that the adapter state can be restored later when a non-VESA
	 * mode is to be set up. See above.
	 */
#if 0
	if (!VESA_MODE(vesa_adp->va_mode) && (vesa_state_buf == NULL)) {
		len = vesa_save_state(adp, NULL, 0);
		vesa_state_buf = malloc(len, M_DEVBUF, M_WAITOK);
		if (vesa_save_state(adp, vesa_state_buf, len)) {
#if VESA_DEBUG > 0
			printf("VESA: state save failed! (len=%d)\n", len);
#endif
			free(vesa_state_buf, M_DEVBUF);
			vesa_state_buf = NULL;
			return 1;
		}
#if VESA_DEBUG > 0
		printf("VESA: saved (len=%d)\n", len);
		dump_buffer(vesa_state_buf, len);
#endif
	}
#endif /* 0 */

	if (vesa_bios_set_mode(mode))
		return 1;

#if VESA_DEBUG > 0
	printf("VESA: mode set!\n");
#endif
	vesa_adp->va_mode = mode;
	vesa_adp->va_flags &= ~V_ADP_COLOR;
	vesa_adp->va_flags |= 
		(info.vi_flags & V_INFO_COLOR) ? V_ADP_COLOR : 0;
	vesa_adp->va_crtc_addr =
		(vesa_adp->va_flags & V_ADP_COLOR) ? COLOR_CRTC : MONO_CRTC;
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
	len = vesa_bios_get_line_length();
	if (len > 0) {
		vesa_adp->va_line_width = len;
	} else if (info.vi_flags & V_INFO_GRAPHICS) {
		switch (info.vi_depth/info.vi_planes) {
		case 1:
			vesa_adp->va_line_width = info.vi_width/8;
			break;
		case 2:
			vesa_adp->va_line_width = info.vi_width/4;
			break;
		case 4:
			vesa_adp->va_line_width = info.vi_width/2;
			break;
		case 8:
		default: /* shouldn't happen */
			vesa_adp->va_line_width = info.vi_width;
			break;
		}
	} else {
		vesa_adp->va_line_width = info.vi_width;
	}
#if VESA_DEBUG > 0
	printf("vesa_set_mode(): vi_width:%d, len:%d, line_width:%d\n",
	       info.vi_width, len, vesa_adp->va_line_width);
#endif
	bcopy(&info, &vesa_adp->va_info, sizeof(vesa_adp->va_info));

	/* move hardware cursor out of the way */
	(*vidsw[vesa_adp->va_index]->set_hw_cursor)(vesa_adp, -1, -1);

	return 0;
}

static int
vesa_save_font(video_adapter_t *adp, int page, int fontsize, u_char *data,
	       int ch, int count)
{
	return (*prevvidsw->save_font)(adp, page, fontsize, data, ch, count);
}

static int
vesa_load_font(video_adapter_t *adp, int page, int fontsize, u_char *data,
	       int ch, int count)
{
	return (*prevvidsw->load_font)(adp, page, fontsize, data, ch, count);
}

static int
vesa_show_font(video_adapter_t *adp, int page)
{
	return (*prevvidsw->show_font)(adp, page);
}

static int
vesa_save_palette(video_adapter_t *adp, u_char *palette)
{
	int bits;
	int error;

	if ((adp == vesa_adp) && (vesa_adp_info->v_flags & V_DAC8)
	    && VESA_MODE(adp->va_mode)) {
		bits = vesa_bios_get_dac();
		error = vesa_bios_save_palette(0, 256, palette, bits);
		if (error == 0)
			return 0;
		if (bits != 6)
			return error;
	}

	return (*prevvidsw->save_palette)(adp, palette);
}

static int
vesa_load_palette(video_adapter_t *adp, u_char *palette)
{
#if notyet
	int bits;
	int error;

	if ((adp == vesa_adp) && (vesa_adp_info->v_flags & V_DAC8) 
	    && VESA_MODE(adp->va_mode) && ((bits = vesa_bios_set_dac(8)) > 6)) {
		error = vesa_bios_load_palette(0, 256, palette, bits);
		if (error == 0)
			return 0;
		if (vesa_bios_set_dac(6) != 6)
			return 1;
	}
#endif /* notyet */

	return (*prevvidsw->load_palette)(adp, palette);
}

static int
vesa_set_border(video_adapter_t *adp, int color)
{
	return (*prevvidsw->set_border)(adp, color);
}

static int
vesa_save_state(video_adapter_t *adp, void *p, size_t size)
{
	if (adp != vesa_adp)
		return (*prevvidsw->save_state)(adp, p, size);

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
vesa_load_state(video_adapter_t *adp, void *p)
{
	if ((adp != vesa_adp) || (((adp_state_t *)p)->sig != V_STATE_SIG))
		return (*prevvidsw->load_state)(adp, p);

	return vesa_bios_save_restore(STATE_LOAD, ((adp_state_t *)p)->regs, 
				      vesa_state_buf_size);
}

static int
vesa_set_origin(video_adapter_t *adp, off_t offset)
{
	struct vm86frame vmf;
	int err;

	/*
	 * This function should return as quickly as possible to 
	 * maintain good performance of the system. For this reason,
	 * error checking is kept minimal and let the VESA BIOS to 
	 * detect error.
	 */
	if (adp != vesa_adp) 
		return (*prevvidsw->set_win_org)(adp, offset);

	if (vesa_adp->va_window_gran == 0)
		return 1;
	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f05; 
	vmf.vmf_ebx = 0;		/* WINDOW_A, XXX */
	vmf.vmf_edx = offset/vesa_adp->va_window_gran;
	err = vm86_intcall(0x10, &vmf); 
	if ((err != 0) || (vmf.vmf_eax != 0x4f))
		return 1;
	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f05; 
	vmf.vmf_ebx = 1;		/* WINDOW_B, XXX */
	vmf.vmf_edx = offset/vesa_adp->va_window_gran;
	err = vm86_intcall(0x10, &vmf); 
	return 0;			/* XXX */
}

static int
vesa_read_hw_cursor(video_adapter_t *adp, int *col, int *row)
{
	return (*prevvidsw->read_hw_cursor)(adp, col, row);
}

static int
vesa_set_hw_cursor(video_adapter_t *adp, int col, int row)
{
	return (*prevvidsw->set_hw_cursor)(adp, col, row);
}

static int
vesa_set_hw_cursor_shape(video_adapter_t *adp, int base, int height,
			 int celsize, int blink)
{
	return (*prevvidsw->set_hw_cursor_shape)(adp, base, height, celsize,
						 blink);
}

static int
vesa_mmap(video_adapter_t *adp, vm_offset_t offset)
{
	return (*prevvidsw->mmap)(adp, offset);
}

static int
vesa_diag(video_adapter_t *adp, int level)
{
#if VESA_DEBUG > 1
	struct vesa_mode vmode;
	int i;
#endif

	if (adp != vesa_adp)
		return 1;

#ifndef KLD_MODULE
	/* call the previous handler first */
	(*prevvidsw->diag)(adp, level);
#endif

	/* general adapter information */
	printf("VESA: v%d.%d, %dk memory, flags:0x%x, mode table:%p (%x)\n", 
	       ((vesa_adp_info->v_version & 0xf000) >> 12) * 10 
		   + ((vesa_adp_info->v_version & 0x0f00) >> 8),
	       ((vesa_adp_info->v_version & 0x00f0) >> 4) * 10 
		   + (vesa_adp_info->v_version & 0x000f),
	       vesa_adp_info->v_memsize * 64, vesa_adp_info->v_flags,
	       vesa_vmodetab, vesa_adp_info->v_modetable);
	/* OEM string */
	if (vesa_oemstr != NULL)
		printf("VESA: %s\n", vesa_oemstr);

	if (level <= 0)
		return 0;

	if (vesa_adp_info->v_version >= 0x0200) {
		/* vendor name */
		if (vesa_venderstr != NULL)
			printf("VESA: %s\n", vesa_venderstr);
		/* product name */
		if (vesa_prodstr != NULL)
			printf("VESA: %s\n", vesa_prodstr);
		/* product revision */
		if (vesa_revstr != NULL)
			printf("VESA: %s\n", vesa_revstr);
	}

#if VESA_DEBUG > 1
	/* mode information */
	for (i = 0;
		(i < (M_VESA_MODE_MAX - M_VESA_BASE + 1))
		&& (vesa_vmodetab[i] != 0xffff); ++i) {
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
#endif

	return 0;
}

/* module loading */

static int
vesa_load(void)
{
	int error;
	int s;

	if (vesa_init_done)
		return 0;

	/* locate a VGA adapter */
	s = spltty();
	vesa_adp = NULL;
	error = vesa_configure(0);
	splx(s);

#ifdef KLD_MODULE
	if (error == 0)
		vesa_diag(vesa_adp, bootverbose);
#endif

	return error;
}

#ifdef KLD_MODULE

static int
vesa_unload(void)
{
	u_char palette[256*3];
	int error;
	int bits;
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
		if (vesa_adp != NULL) {
			if (vesa_adp_info->v_flags & V_DAC8)  {
				bits = vesa_bios_get_dac();
				if (bits > 6) {
					vesa_bios_save_palette(0, 256,
							       palette, bits);
					vesa_bios_set_dac(6);
					vesa_bios_load_palette(0, 256,
							       palette, 6);
				}
			}
			vesa_adp->va_flags &= ~V_ADP_VESA;
			vidsw[vesa_adp->va_index] = prevvidsw;
		}
	}
	splx(s);

	return error;
}

static int
vesa_mod_event(module_t mod, int type, void *data)
{
	switch (type) {
	case MOD_LOAD:
		return vesa_load();
	case MOD_UNLOAD:
		return vesa_unload();
	default:
		break;
	}
	return 0;
}

static moduledata_t vesa_mod = {
	"vesa",
	vesa_mod_event,
	NULL,
};

DECLARE_MODULE(vesa, vesa_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);

#else /* KLD_MODULE */

SYSINIT(vesa, SI_SUB_DRIVERS, SI_ORDER_MIDDLE,
	(void (*)(void *))vesa_load, NULL);

#endif /* KLD_MODULE */

#endif /* (NVGA > 0 && VESA && VM86) || KLD_MODULE */
