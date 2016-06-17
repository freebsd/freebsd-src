/* r128_drm.h -- Public header for the r128 driver -*- linux-c -*-
 * Created: Wed Apr  5 19:24:19 2000 by kevin@precisioninsight.com
 *
 * Copyright 2000 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Kevin E. Martin <martin@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 */

#ifndef _R128_DRM_H_
#define _R128_DRM_H_

/* WARNING: If you change any of these defines, make sure to change the
 * defines in the X server file (r128_sarea.h)
 */
#ifndef __R128_SAREA_DEFINES__
#define __R128_SAREA_DEFINES__

/* What needs to be changed for the current vertex buffer?
 */
#define R128_UPLOAD_CONTEXT		0x001
#define R128_UPLOAD_SETUP		0x002
#define R128_UPLOAD_TEX0		0x004
#define R128_UPLOAD_TEX1		0x008
#define R128_UPLOAD_TEX0IMAGES		0x010
#define R128_UPLOAD_TEX1IMAGES		0x020
#define R128_UPLOAD_CORE		0x040
#define R128_UPLOAD_MASKS		0x080
#define R128_UPLOAD_WINDOW		0x100
#define R128_UPLOAD_CLIPRECTS		0x200	/* handled client-side */
#define R128_REQUIRE_QUIESCENCE		0x400
#define R128_UPLOAD_ALL			0x7ff

#define R128_FRONT			0x1
#define R128_BACK			0x2
#define R128_DEPTH			0x4

/* Primitive types
 */
#define R128_POINTS			0x1
#define R128_LINES			0x2
#define R128_LINE_STRIP			0x3
#define R128_TRIANGLES			0x4
#define R128_TRIANGLE_FAN		0x5
#define R128_TRIANGLE_STRIP		0x6

/* Vertex/indirect buffer size
 */
#if 1
#define R128_BUFFER_SIZE		16384
#else
#define R128_BUFFER_SIZE		(128 * 1024)
#endif

/* Byte offsets for indirect buffer data
 */
#define R128_INDEX_PRIM_OFFSET		20
#define R128_HOSTDATA_BLIT_OFFSET	32

/* 2048x2048 @ 32bpp texture requires this many indirect buffers
 */
#define R128_MAX_BLIT_BUFFERS		((2048 * 2048 * 4) / R128_BUFFER_SIZE)

/* Keep these small for testing.
 */
#define R128_NR_SAREA_CLIPRECTS		12

/* There are 2 heaps (local/AGP).  Each region within a heap is a
 *  minimum of 64k, and there are at most 64 of them per heap.
 */
#define R128_LOCAL_TEX_HEAP		0
#define R128_AGP_TEX_HEAP		1
#define R128_NR_TEX_HEAPS		2
#define R128_NR_TEX_REGIONS		64
#define R128_LOG_TEX_GRANULARITY	16

#define R128_NR_CONTEXT_REGS		12
#define R128_TEX_MAXLEVELS		11

#endif /* __R128_SAREA_DEFINES__ */

typedef struct {
	/* Context state - can be written in one large chunk */
	unsigned int dst_pitch_offset_c;
	unsigned int dp_gui_master_cntl_c;
	unsigned int sc_top_left_c;
	unsigned int sc_bottom_right_c;
	unsigned int z_offset_c;
	unsigned int z_pitch_c;
	unsigned int z_sten_cntl_c;
	unsigned int tex_cntl_c;
	unsigned int misc_3d_state_cntl_reg;
	unsigned int texture_clr_cmp_clr_c;
	unsigned int texture_clr_cmp_msk_c;
	unsigned int fog_color_c;

	/* Texture state */
	unsigned int tex_size_pitch_c;
	unsigned int constant_color_c;

	/* Setup state */
	unsigned int pm4_vc_fpu_setup;
	unsigned int setup_cntl;

	/* Mask state */
	unsigned int dp_write_mask;
	unsigned int sten_ref_mask_c;
	unsigned int plane_3d_mask_c;

	/* Window state */
	unsigned int window_xy_offset;

	/* Core state */
	unsigned int scale_3d_cntl;
} drm_r128_context_regs_t;

/* Setup registers for each texture unit */
typedef struct {
	unsigned int tex_cntl;
	unsigned int tex_combine_cntl;
	unsigned int tex_size_pitch;
	unsigned int tex_offset[R128_TEX_MAXLEVELS];
	unsigned int tex_border_color;
} drm_r128_texture_regs_t;


typedef struct drm_tex_region {
	unsigned char next, prev;
	unsigned char in_use;
	int age;
} drm_tex_region_t;

typedef struct drm_r128_sarea {
	/* The channel for communication of state information to the kernel
	 * on firing a vertex buffer.
	 */
	drm_r128_context_regs_t context_state;
	drm_r128_texture_regs_t tex_state[R128_NR_TEX_HEAPS];
	unsigned int dirty;
	unsigned int vertsize;
	unsigned int vc_format;

	/* The current cliprects, or a subset thereof.
	 */
	drm_clip_rect_t boxes[R128_NR_SAREA_CLIPRECTS];
	unsigned int nbox;

	/* Counters for client-side throttling of rendering clients.
	 */
	unsigned int last_frame;
	unsigned int last_dispatch;

	drm_tex_region_t tex_list[R128_NR_TEX_HEAPS][R128_NR_TEX_REGIONS+1];
	int tex_age[R128_NR_TEX_HEAPS];
	int ctx_owner;
} drm_r128_sarea_t;


/* WARNING: If you change any of these defines, make sure to change the
 * defines in the Xserver file (xf86drmR128.h)
 */
typedef struct drm_r128_init {
	enum {
		R128_INIT_CCE    = 0x01,
		R128_CLEANUP_CCE = 0x02
	} func;
	int sarea_priv_offset;
	int is_pci;
	int cce_mode;
	int cce_secure;
	int ring_size;
	int usec_timeout;

	unsigned int fb_bpp;
	unsigned int front_offset, front_pitch;
	unsigned int back_offset, back_pitch;
	unsigned int depth_bpp;
	unsigned int depth_offset, depth_pitch;
	unsigned int span_offset;

	unsigned int fb_offset;
	unsigned int mmio_offset;
	unsigned int ring_offset;
	unsigned int ring_rptr_offset;
	unsigned int buffers_offset;
	unsigned int agp_textures_offset;
} drm_r128_init_t;

typedef struct drm_r128_cce_stop {
	int flush;
	int idle;
} drm_r128_cce_stop_t;

typedef struct drm_r128_clear {
	unsigned int flags;
	int x, y, w, h;
	unsigned int clear_color;
	unsigned int clear_depth;
} drm_r128_clear_t;

typedef struct drm_r128_vertex {
	int prim;
	int idx;			/* Index of vertex buffer */
	int count;			/* Number of vertices in buffer */
	int discard;			/* Client finished with buffer? */
} drm_r128_vertex_t;

typedef struct drm_r128_indices {
	int prim;
	int idx;
	int start;
	int end;
	int discard;			/* Client finished with buffer? */
} drm_r128_indices_t;

typedef struct drm_r128_blit {
	int idx;
	int pitch;
	int offset;
	int format;
	unsigned short x, y;
	unsigned short width, height;
} drm_r128_blit_t;

typedef struct drm_r128_depth {
	enum {
		R128_WRITE_SPAN		= 0x01,
		R128_WRITE_PIXELS	= 0x02,
		R128_READ_SPAN		= 0x03,
		R128_READ_PIXELS	= 0x04
	} func;
	int n;
	int *x;
	int *y;
	unsigned int *buffer;
	unsigned char *mask;
} drm_r128_depth_t;

typedef struct drm_r128_stipple {
	unsigned int *mask;
} drm_r128_stipple_t;

typedef struct drm_r128_packet {
	unsigned int *buffer;
	int count;
	int flags;
} drm_r128_packet_t;

#endif
