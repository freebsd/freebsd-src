/* mach64_state.c -- State support for mach64 (Rage Pro) driver -*- linux-c -*-
 * Created: Sun Dec 03 19:20:26 2000 by gareth@valinux.com
 */
/*-
 * Copyright 2000 Gareth Hughes
 * Copyright 2002-2003 Leif Delgass
 * All Rights Reserved.
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
 * THE COPYRIGHT OWNER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Gareth Hughes <gareth@valinux.com>
 *    Leif Delgass <ldelgass@retinalburn.net>
 *    Jos�Fonseca <j_r_fonseca@yahoo.co.uk>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/drm/mach64_state.c,v 1.2 2005/11/28 23:13:53 anholt Exp $");

#include "dev/drm/drmP.h"
#include "dev/drm/drm.h"
#include "dev/drm/mach64_drm.h"
#include "dev/drm/mach64_drv.h"

/* Interface history:
 *
 * 1.0 - Initial mach64 DRM
 *
 */
drm_ioctl_desc_t mach64_ioctls[] = {
	[DRM_IOCTL_NR(DRM_MACH64_INIT)] = {mach64_dma_init, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY},
	[DRM_IOCTL_NR(DRM_MACH64_CLEAR)] = {mach64_dma_clear, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_MACH64_SWAP)] = {mach64_dma_swap, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_MACH64_IDLE)] = {mach64_dma_idle, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_MACH64_RESET)] = {mach64_engine_reset, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_MACH64_VERTEX)] = {mach64_dma_vertex, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_MACH64_BLIT)] = {mach64_dma_blit, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_MACH64_FLUSH)] = {mach64_dma_flush, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_MACH64_GETPARAM)] = {mach64_get_param, DRM_AUTH},
};

int mach64_max_ioctl = DRM_ARRAY_SIZE(mach64_ioctls);

/* ================================================================
 * DMA hardware state programming functions
 */

static void mach64_print_dirty(const char *msg, unsigned int flags)
{
	DRM_DEBUG("%s: (0x%x) %s%s%s%s%s%s%s%s%s%s%s%s\n",
		  msg,
		  flags,
		  (flags & MACH64_UPLOAD_DST_OFF_PITCH) ? "dst_off_pitch, " :
		  "",
		  (flags & MACH64_UPLOAD_Z_ALPHA_CNTL) ? "z_alpha_cntl, " : "",
		  (flags & MACH64_UPLOAD_SCALE_3D_CNTL) ? "scale_3d_cntl, " :
		  "", (flags & MACH64_UPLOAD_DP_FOG_CLR) ? "dp_fog_clr, " : "",
		  (flags & MACH64_UPLOAD_DP_WRITE_MASK) ? "dp_write_mask, " :
		  "",
		  (flags & MACH64_UPLOAD_DP_PIX_WIDTH) ? "dp_pix_width, " : "",
		  (flags & MACH64_UPLOAD_SETUP_CNTL) ? "setup_cntl, " : "",
		  (flags & MACH64_UPLOAD_MISC) ? "misc, " : "",
		  (flags & MACH64_UPLOAD_TEXTURE) ? "texture, " : "",
		  (flags & MACH64_UPLOAD_TEX0IMAGE) ? "tex0 image, " : "",
		  (flags & MACH64_UPLOAD_TEX1IMAGE) ? "tex1 image, " : "",
		  (flags & MACH64_UPLOAD_CLIPRECTS) ? "cliprects, " : "");
}

/* Mach64 doesn't have hardware cliprects, just one hardware scissor,
 * so the GL scissor is intersected with each cliprect here
 */
/* This function returns 0 on success, 1 for no intersection, and
 * negative for an error
 */
static int mach64_emit_cliprect(DRMFILE filp, drm_mach64_private_t * dev_priv,
				drm_clip_rect_t * box)
{
	u32 sc_left_right, sc_top_bottom;
	drm_clip_rect_t scissor;
	drm_mach64_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_mach64_context_regs_t *regs = &sarea_priv->context_state;
	DMALOCALS;

	DRM_DEBUG("%s: box=%p\n", __FUNCTION__, box);

	/* Get GL scissor */
	/* FIXME: store scissor in SAREA as a cliprect instead of in
	 * hardware format, or do intersection client-side
	 */
	scissor.x1 = regs->sc_left_right & 0xffff;
	scissor.x2 = (regs->sc_left_right & 0xffff0000) >> 16;
	scissor.y1 = regs->sc_top_bottom & 0xffff;
	scissor.y2 = (regs->sc_top_bottom & 0xffff0000) >> 16;

	/* Intersect GL scissor with cliprect */
	if (box->x1 > scissor.x1)
		scissor.x1 = box->x1;
	if (box->y1 > scissor.y1)
		scissor.y1 = box->y1;
	if (box->x2 < scissor.x2)
		scissor.x2 = box->x2;
	if (box->y2 < scissor.y2)
		scissor.y2 = box->y2;
	/* positive return means skip */
	if (scissor.x1 >= scissor.x2)
		return 1;
	if (scissor.y1 >= scissor.y2)
		return 1;

	DMAGETPTR(filp, dev_priv, 2);	/* returns on failure to get buffer */

	sc_left_right = ((scissor.x1 << 0) | (scissor.x2 << 16));
	sc_top_bottom = ((scissor.y1 << 0) | (scissor.y2 << 16));

	DMAOUTREG(MACH64_SC_LEFT_RIGHT, sc_left_right);
	DMAOUTREG(MACH64_SC_TOP_BOTTOM, sc_top_bottom);

	DMAADVANCE(dev_priv, 1);

	return 0;
}

static __inline__ int mach64_emit_state(DRMFILE filp,
					drm_mach64_private_t * dev_priv)
{
	drm_mach64_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_mach64_context_regs_t *regs = &sarea_priv->context_state;
	unsigned int dirty = sarea_priv->dirty;
	u32 offset = ((regs->tex_size_pitch & 0xf0) >> 2);
	DMALOCALS;

	if (MACH64_VERBOSE) {
		mach64_print_dirty(__FUNCTION__, dirty);
	} else {
		DRM_DEBUG("%s: dirty=0x%08x\n", __FUNCTION__, dirty);
	}

	DMAGETPTR(filp, dev_priv, 17);	/* returns on failure to get buffer */

	if (dirty & MACH64_UPLOAD_MISC) {
		DMAOUTREG(MACH64_DP_MIX, regs->dp_mix);
		DMAOUTREG(MACH64_DP_SRC, regs->dp_src);
		DMAOUTREG(MACH64_CLR_CMP_CNTL, regs->clr_cmp_cntl);
		DMAOUTREG(MACH64_GUI_TRAJ_CNTL, regs->gui_traj_cntl);
		sarea_priv->dirty &= ~MACH64_UPLOAD_MISC;
	}

	if (dirty & MACH64_UPLOAD_DST_OFF_PITCH) {
		DMAOUTREG(MACH64_DST_OFF_PITCH, regs->dst_off_pitch);
		sarea_priv->dirty &= ~MACH64_UPLOAD_DST_OFF_PITCH;
	}
	if (dirty & MACH64_UPLOAD_Z_OFF_PITCH) {
		DMAOUTREG(MACH64_Z_OFF_PITCH, regs->z_off_pitch);
		sarea_priv->dirty &= ~MACH64_UPLOAD_Z_OFF_PITCH;
	}
	if (dirty & MACH64_UPLOAD_Z_ALPHA_CNTL) {
		DMAOUTREG(MACH64_Z_CNTL, regs->z_cntl);
		DMAOUTREG(MACH64_ALPHA_TST_CNTL, regs->alpha_tst_cntl);
		sarea_priv->dirty &= ~MACH64_UPLOAD_Z_ALPHA_CNTL;
	}
	if (dirty & MACH64_UPLOAD_SCALE_3D_CNTL) {
		DMAOUTREG(MACH64_SCALE_3D_CNTL, regs->scale_3d_cntl);
		sarea_priv->dirty &= ~MACH64_UPLOAD_SCALE_3D_CNTL;
	}
	if (dirty & MACH64_UPLOAD_DP_FOG_CLR) {
		DMAOUTREG(MACH64_DP_FOG_CLR, regs->dp_fog_clr);
		sarea_priv->dirty &= ~MACH64_UPLOAD_DP_FOG_CLR;
	}
	if (dirty & MACH64_UPLOAD_DP_WRITE_MASK) {
		DMAOUTREG(MACH64_DP_WRITE_MASK, regs->dp_write_mask);
		sarea_priv->dirty &= ~MACH64_UPLOAD_DP_WRITE_MASK;
	}
	if (dirty & MACH64_UPLOAD_DP_PIX_WIDTH) {
		DMAOUTREG(MACH64_DP_PIX_WIDTH, regs->dp_pix_width);
		sarea_priv->dirty &= ~MACH64_UPLOAD_DP_PIX_WIDTH;
	}
	if (dirty & MACH64_UPLOAD_SETUP_CNTL) {
		DMAOUTREG(MACH64_SETUP_CNTL, regs->setup_cntl);
		sarea_priv->dirty &= ~MACH64_UPLOAD_SETUP_CNTL;
	}

	if (dirty & MACH64_UPLOAD_TEXTURE) {
		DMAOUTREG(MACH64_TEX_SIZE_PITCH, regs->tex_size_pitch);
		DMAOUTREG(MACH64_TEX_CNTL, regs->tex_cntl);
		DMAOUTREG(MACH64_SECONDARY_TEX_OFF, regs->secondary_tex_off);
		DMAOUTREG(MACH64_TEX_0_OFF + offset, regs->tex_offset);
		sarea_priv->dirty &= ~MACH64_UPLOAD_TEXTURE;
	}

	DMAADVANCE(dev_priv, 1);

	sarea_priv->dirty &= MACH64_UPLOAD_CLIPRECTS;

	return 0;

}

/* ================================================================
 * DMA command dispatch functions
 */

static int mach64_dma_dispatch_clear(DRMFILE filp, drm_device_t * dev,
				     unsigned int flags,
				     int cx, int cy, int cw, int ch,
				     unsigned int clear_color,
				     unsigned int clear_depth)
{
	drm_mach64_private_t *dev_priv = dev->dev_private;
	drm_mach64_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_mach64_context_regs_t *ctx = &sarea_priv->context_state;
	int nbox = sarea_priv->nbox;
	drm_clip_rect_t *pbox = sarea_priv->boxes;
	u32 fb_bpp, depth_bpp;
	int i;
	DMALOCALS;

	DRM_DEBUG("%s\n", __FUNCTION__);

	switch (dev_priv->fb_bpp) {
	case 16:
		fb_bpp = MACH64_DATATYPE_RGB565;
		break;
	case 32:
		fb_bpp = MACH64_DATATYPE_ARGB8888;
		break;
	default:
		return DRM_ERR(EINVAL);
	}
	switch (dev_priv->depth_bpp) {
	case 16:
		depth_bpp = MACH64_DATATYPE_RGB565;
		break;
	case 24:
	case 32:
		depth_bpp = MACH64_DATATYPE_ARGB8888;
		break;
	default:
		return DRM_ERR(EINVAL);
	}

	if (!nbox)
		return 0;

	DMAGETPTR(filp, dev_priv, nbox * 31);	/* returns on failure to get buffer */

	for (i = 0; i < nbox; i++) {
		int x = pbox[i].x1;
		int y = pbox[i].y1;
		int w = pbox[i].x2 - x;
		int h = pbox[i].y2 - y;

		DRM_DEBUG("dispatch clear %d,%d-%d,%d flags 0x%x\n",
			  pbox[i].x1, pbox[i].y1,
			  pbox[i].x2, pbox[i].y2, flags);

		if (flags & (MACH64_FRONT | MACH64_BACK)) {
			/* Setup for color buffer clears
			 */

			DMAOUTREG(MACH64_Z_CNTL, 0);
			DMAOUTREG(MACH64_SCALE_3D_CNTL, 0);

			DMAOUTREG(MACH64_SC_LEFT_RIGHT, ctx->sc_left_right);
			DMAOUTREG(MACH64_SC_TOP_BOTTOM, ctx->sc_top_bottom);

			DMAOUTREG(MACH64_CLR_CMP_CNTL, 0);
			DMAOUTREG(MACH64_GUI_TRAJ_CNTL,
				  (MACH64_DST_X_LEFT_TO_RIGHT |
				   MACH64_DST_Y_TOP_TO_BOTTOM));

			DMAOUTREG(MACH64_DP_PIX_WIDTH, ((fb_bpp << 0) |
							(fb_bpp << 4) |
							(fb_bpp << 8) |
							(fb_bpp << 16) |
							(fb_bpp << 28)));

			DMAOUTREG(MACH64_DP_FRGD_CLR, clear_color);
			DMAOUTREG(MACH64_DP_WRITE_MASK, ctx->dp_write_mask);
			DMAOUTREG(MACH64_DP_MIX, (MACH64_BKGD_MIX_D |
						  MACH64_FRGD_MIX_S));
			DMAOUTREG(MACH64_DP_SRC, (MACH64_BKGD_SRC_FRGD_CLR |
						  MACH64_FRGD_SRC_FRGD_CLR |
						  MACH64_MONO_SRC_ONE));

		}

		if (flags & MACH64_FRONT) {

			DMAOUTREG(MACH64_DST_OFF_PITCH,
				  dev_priv->front_offset_pitch);
			DMAOUTREG(MACH64_DST_X_Y, (y << 16) | x);
			DMAOUTREG(MACH64_DST_WIDTH_HEIGHT, (h << 16) | w);

		}

		if (flags & MACH64_BACK) {

			DMAOUTREG(MACH64_DST_OFF_PITCH,
				  dev_priv->back_offset_pitch);
			DMAOUTREG(MACH64_DST_X_Y, (y << 16) | x);
			DMAOUTREG(MACH64_DST_WIDTH_HEIGHT, (h << 16) | w);

		}

		if (flags & MACH64_DEPTH) {
			/* Setup for depth buffer clear
			 */
			DMAOUTREG(MACH64_Z_CNTL, 0);
			DMAOUTREG(MACH64_SCALE_3D_CNTL, 0);

			DMAOUTREG(MACH64_SC_LEFT_RIGHT, ctx->sc_left_right);
			DMAOUTREG(MACH64_SC_TOP_BOTTOM, ctx->sc_top_bottom);

			DMAOUTREG(MACH64_CLR_CMP_CNTL, 0);
			DMAOUTREG(MACH64_GUI_TRAJ_CNTL,
				  (MACH64_DST_X_LEFT_TO_RIGHT |
				   MACH64_DST_Y_TOP_TO_BOTTOM));

			DMAOUTREG(MACH64_DP_PIX_WIDTH, ((depth_bpp << 0) |
							(depth_bpp << 4) |
							(depth_bpp << 8) |
							(depth_bpp << 16) |
							(depth_bpp << 28)));

			DMAOUTREG(MACH64_DP_FRGD_CLR, clear_depth);
			DMAOUTREG(MACH64_DP_WRITE_MASK, 0xffffffff);
			DMAOUTREG(MACH64_DP_MIX, (MACH64_BKGD_MIX_D |
						  MACH64_FRGD_MIX_S));
			DMAOUTREG(MACH64_DP_SRC, (MACH64_BKGD_SRC_FRGD_CLR |
						  MACH64_FRGD_SRC_FRGD_CLR |
						  MACH64_MONO_SRC_ONE));

			DMAOUTREG(MACH64_DST_OFF_PITCH,
				  dev_priv->depth_offset_pitch);
			DMAOUTREG(MACH64_DST_X_Y, (y << 16) | x);
			DMAOUTREG(MACH64_DST_WIDTH_HEIGHT, (h << 16) | w);
		}
	}

	DMAADVANCE(dev_priv, 1);

	return 0;
}

static int mach64_dma_dispatch_swap(DRMFILE filp, drm_device_t * dev)
{
	drm_mach64_private_t *dev_priv = dev->dev_private;
	drm_mach64_sarea_t *sarea_priv = dev_priv->sarea_priv;
	int nbox = sarea_priv->nbox;
	drm_clip_rect_t *pbox = sarea_priv->boxes;
	u32 fb_bpp;
	int i;
	DMALOCALS;

	DRM_DEBUG("%s\n", __FUNCTION__);

	switch (dev_priv->fb_bpp) {
	case 16:
		fb_bpp = MACH64_DATATYPE_RGB565;
		break;
	case 32:
	default:
		fb_bpp = MACH64_DATATYPE_ARGB8888;
		break;
	}

	if (!nbox)
		return 0;

	DMAGETPTR(filp, dev_priv, 13 + nbox * 4);	/* returns on failure to get buffer */

	DMAOUTREG(MACH64_Z_CNTL, 0);
	DMAOUTREG(MACH64_SCALE_3D_CNTL, 0);

	DMAOUTREG(MACH64_SC_LEFT_RIGHT, 0 | (8191 << 16));	/* no scissor */
	DMAOUTREG(MACH64_SC_TOP_BOTTOM, 0 | (16383 << 16));

	DMAOUTREG(MACH64_CLR_CMP_CNTL, 0);
	DMAOUTREG(MACH64_GUI_TRAJ_CNTL, (MACH64_DST_X_LEFT_TO_RIGHT |
					 MACH64_DST_Y_TOP_TO_BOTTOM));

	DMAOUTREG(MACH64_DP_PIX_WIDTH, ((fb_bpp << 0) |
					(fb_bpp << 4) |
					(fb_bpp << 8) |
					(fb_bpp << 16) | (fb_bpp << 28)));

	DMAOUTREG(MACH64_DP_WRITE_MASK, 0xffffffff);
	DMAOUTREG(MACH64_DP_MIX, (MACH64_BKGD_MIX_D | MACH64_FRGD_MIX_S));
	DMAOUTREG(MACH64_DP_SRC, (MACH64_BKGD_SRC_BKGD_CLR |
				  MACH64_FRGD_SRC_BLIT | MACH64_MONO_SRC_ONE));

	DMAOUTREG(MACH64_SRC_OFF_PITCH, dev_priv->back_offset_pitch);
	DMAOUTREG(MACH64_DST_OFF_PITCH, dev_priv->front_offset_pitch);

	for (i = 0; i < nbox; i++) {
		int x = pbox[i].x1;
		int y = pbox[i].y1;
		int w = pbox[i].x2 - x;
		int h = pbox[i].y2 - y;

		DRM_DEBUG("dispatch swap %d,%d-%d,%d\n",
			  pbox[i].x1, pbox[i].y1, pbox[i].x2, pbox[i].y2);

		DMAOUTREG(MACH64_SRC_WIDTH1, w);
		DMAOUTREG(MACH64_SRC_Y_X, (x << 16) | y);
		DMAOUTREG(MACH64_DST_Y_X, (x << 16) | y);
		DMAOUTREG(MACH64_DST_WIDTH_HEIGHT, (h << 16) | w);

	}

	DMAADVANCE(dev_priv, 1);

	if (dev_priv->driver_mode == MACH64_MODE_DMA_ASYNC) {
		for (i = 0; i < MACH64_MAX_QUEUED_FRAMES - 1; i++) {
			dev_priv->frame_ofs[i] = dev_priv->frame_ofs[i + 1];
		}
		dev_priv->frame_ofs[i] = GETRINGOFFSET();

		dev_priv->sarea_priv->frames_queued++;
	}

	return 0;
}

static int mach64_do_get_frames_queued(drm_mach64_private_t * dev_priv)
{
	drm_mach64_descriptor_ring_t *ring = &dev_priv->ring;
	drm_mach64_sarea_t *sarea_priv = dev_priv->sarea_priv;
	int i, start;
	u32 head, tail, ofs;

	DRM_DEBUG("%s\n", __FUNCTION__);

	if (sarea_priv->frames_queued == 0)
		return 0;

	tail = ring->tail;
	mach64_ring_tick(dev_priv, ring);
	head = ring->head;

	start = (MACH64_MAX_QUEUED_FRAMES -
		 DRM_MIN(MACH64_MAX_QUEUED_FRAMES, sarea_priv->frames_queued));

	if (head == tail) {
		sarea_priv->frames_queued = 0;
		for (i = start; i < MACH64_MAX_QUEUED_FRAMES; i++) {
			dev_priv->frame_ofs[i] = ~0;
		}
		return 0;
	}

	for (i = start; i < MACH64_MAX_QUEUED_FRAMES; i++) {
		ofs = dev_priv->frame_ofs[i];
		DRM_DEBUG("frame_ofs[%d] ofs: %d\n", i, ofs);
		if (ofs == ~0 ||
		    (head < tail && (ofs < head || ofs >= tail)) ||
		    (head > tail && (ofs < head && ofs >= tail))) {
			sarea_priv->frames_queued =
			    (MACH64_MAX_QUEUED_FRAMES - 1) - i;
			dev_priv->frame_ofs[i] = ~0;
		}
	}

	return sarea_priv->frames_queued;
}

/* Copy and verify a client submited buffer.
 * FIXME: Make an assembly optimized version
 */
static __inline__ int copy_and_verify_from_user(u32 *to,
						const u32 __user *ufrom,
						unsigned long bytes)
{
	unsigned long n = bytes;	/* dwords remaining in buffer */
	u32 *from, *orig_from;

	from = drm_alloc(bytes, DRM_MEM_DRIVER);
	if (from == NULL)
		return ENOMEM;

	if (DRM_COPY_FROM_USER(from, ufrom, bytes)) {
		drm_free(from, bytes, DRM_MEM_DRIVER);
		return DRM_ERR(EFAULT);
	}
	orig_from = from; /* we'll be modifying the "from" ptr, so save it */

	n >>= 2;

	while (n > 1) {
		u32 data, reg, count;

		data = *from++;

		n--;

		reg = le32_to_cpu(data);
		count = (reg >> 16) + 1;
		if (count <= n) {
			n -= count;
			reg &= 0xffff;

			/* This is an exact match of Mach64's Setup Engine registers,
			 * excluding SETUP_CNTL (1_C1).
			 */
			if ((reg >= 0x0190 && reg < 0x01c1) ||
			    (reg >= 0x01ca && reg <= 0x01cf)) {
				*to++ = data;
				memcpy(to, from, count << 2);
				from += count;
				to += count;
			} else {
				DRM_ERROR("%s: Got bad command: 0x%04x\n",
					  __FUNCTION__, reg);
				drm_free(orig_from, bytes, DRM_MEM_DRIVER);
				return DRM_ERR(EACCES);
			}
		} else {
			DRM_ERROR
			    ("%s: Got bad command count(=%u) dwords remaining=%lu\n",
			     __FUNCTION__, count, n);
			drm_free(orig_from, bytes, DRM_MEM_DRIVER);
			return DRM_ERR(EINVAL);
		}
	}

	drm_free(orig_from, bytes, DRM_MEM_DRIVER);
	if (n == 0)
		return 0;
	else {
		DRM_ERROR("%s: Bad buf->used(=%lu)\n", __FUNCTION__, bytes);
		return DRM_ERR(EINVAL);
	}
}

static int mach64_dma_dispatch_vertex(DRMFILE filp, drm_device_t * dev,
				      int prim, void *buf, unsigned long used,
				      int discard)
{
	drm_mach64_private_t *dev_priv = dev->dev_private;
	drm_mach64_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_buf_t *copy_buf;
	int done = 0;
	int verify_ret = 0;
	DMALOCALS;

	DRM_DEBUG("%s: buf=%p used=%lu nbox=%d\n",
		  __FUNCTION__, buf, used, sarea_priv->nbox);

	if (used) {
		int ret = 0;
		int i = 0;

		copy_buf = mach64_freelist_get(dev_priv);
		if (copy_buf == NULL) {
			DRM_ERROR("%s: couldn't get buffer in DMAGETPTR\n",
				  __FUNCTION__);
			return DRM_ERR(EAGAIN);
		}

		if ((verify_ret =
		     copy_and_verify_from_user(GETBUFPTR(copy_buf), buf,
					       used)) == 0) {

			copy_buf->used = used;

			DMASETPTR(copy_buf);

			if (sarea_priv->dirty & ~MACH64_UPLOAD_CLIPRECTS) {
				ret = mach64_emit_state(filp, dev_priv);
				if (ret < 0)
					return ret;
			}

			do {
				/* Emit the next cliprect */
				if (i < sarea_priv->nbox) {
					ret =
					    mach64_emit_cliprect(filp, dev_priv,
								 &sarea_priv->
								 boxes[i]);
					if (ret < 0) {
						/* failed to get buffer */
						return ret;
					} else if (ret != 0) {
						/* null intersection with scissor */
						continue;
					}
				}
				if ((i >= sarea_priv->nbox - 1))
					done = 1;

				/* Add the buffer to the DMA queue */
				DMAADVANCE(dev_priv, done);

			} while (++i < sarea_priv->nbox);
		}

		if (copy_buf->pending && !done) {
			DMADISCARDBUF();
		} else if (!done) {
			/* This buffer wasn't used (no cliprects or verify failed), so place it back
			 * on the free list
			 */
			struct list_head *ptr;
			drm_mach64_freelist_t *entry;
#if MACH64_EXTRA_CHECKING
			list_for_each(ptr, &dev_priv->pending) {
				entry =
				    list_entry(ptr, drm_mach64_freelist_t,
					       list);
				if (copy_buf == entry->buf) {
					DRM_ERROR
					    ("%s: Trying to release a pending buf\n",
					     __FUNCTION__);
					return DRM_ERR(EFAULT);
				}
			}
#endif
			ptr = dev_priv->placeholders.next;
			entry = list_entry(ptr, drm_mach64_freelist_t, list);
			copy_buf->pending = 0;
			copy_buf->used = 0;
			entry->buf = copy_buf;
			entry->discard = 1;
			list_del(ptr);
			list_add_tail(ptr, &dev_priv->free_list);
		}
	}

	sarea_priv->dirty &= ~MACH64_UPLOAD_CLIPRECTS;
	sarea_priv->nbox = 0;

	return verify_ret;
}

static int mach64_dma_dispatch_blit(DRMFILE filp, drm_device_t * dev,
				    drm_mach64_blit_t * blit)
{
	drm_mach64_private_t *dev_priv = dev->dev_private;
	drm_device_dma_t *dma = dev->dma;
	int dword_shift, dwords;
	drm_buf_t *buf;
	DMALOCALS;

	/* The compiler won't optimize away a division by a variable,
	 * even if the only legal values are powers of two.  Thus, we'll
	 * use a shift instead.
	 */
	switch (blit->format) {
	case MACH64_DATATYPE_ARGB8888:
		dword_shift = 0;
		break;
	case MACH64_DATATYPE_ARGB1555:
	case MACH64_DATATYPE_RGB565:
	case MACH64_DATATYPE_VYUY422:
	case MACH64_DATATYPE_YVYU422:
	case MACH64_DATATYPE_ARGB4444:
		dword_shift = 1;
		break;
	case MACH64_DATATYPE_CI8:
	case MACH64_DATATYPE_RGB8:
		dword_shift = 2;
		break;
	default:
		DRM_ERROR("invalid blit format %d\n", blit->format);
		return DRM_ERR(EINVAL);
	}

	/* Dispatch the blit buffer.
	 */
	buf = dma->buflist[blit->idx];

	if (buf->filp != filp) {
		DRM_ERROR("process %d (filp %p) using buffer with filp %p\n",
			  DRM_CURRENTPID, filp, buf->filp);
		return DRM_ERR(EINVAL);
	}

	if (buf->pending) {
		DRM_ERROR("sending pending buffer %d\n", blit->idx);
		return DRM_ERR(EINVAL);
	}

	/* Set buf->used to the bytes of blit data based on the blit dimensions
	 * and verify the size.  When the setup is emitted to the buffer with
	 * the DMA* macros below, buf->used is incremented to include the bytes
	 * used for setup as well as the blit data.
	 */
	dwords = (blit->width * blit->height) >> dword_shift;
	buf->used = dwords << 2;
	if (buf->used <= 0 ||
	    buf->used > MACH64_BUFFER_SIZE - MACH64_HOSTDATA_BLIT_OFFSET) {
		DRM_ERROR("Invalid blit size: %d bytes\n", buf->used);
		return DRM_ERR(EINVAL);
	}

	/* FIXME: Use a last buffer flag and reduce the state emitted for subsequent,
	 * continuation buffers?
	 */

	/* Blit via BM_HOSTDATA (gui-master) - like HOST_DATA[0-15], but doesn't require
	 * a register command every 16 dwords.  State setup is added at the start of the
	 * buffer -- the client leaves space for this based on MACH64_HOSTDATA_BLIT_OFFSET
	 */
	DMASETPTR(buf);

	DMAOUTREG(MACH64_Z_CNTL, 0);
	DMAOUTREG(MACH64_SCALE_3D_CNTL, 0);

	DMAOUTREG(MACH64_SC_LEFT_RIGHT, 0 | (8191 << 16));	/* no scissor */
	DMAOUTREG(MACH64_SC_TOP_BOTTOM, 0 | (16383 << 16));

	DMAOUTREG(MACH64_CLR_CMP_CNTL, 0);	/* disable */
	DMAOUTREG(MACH64_GUI_TRAJ_CNTL,
		  MACH64_DST_X_LEFT_TO_RIGHT | MACH64_DST_Y_TOP_TO_BOTTOM);

	DMAOUTREG(MACH64_DP_PIX_WIDTH, (blit->format << 0)	/* dst pix width */
		  |(blit->format << 4)	/* composite pix width */
		  |(blit->format << 8)	/* src pix width */
		  |(blit->format << 16)	/* host data pix width */
		  |(blit->format << 28)	/* scaler/3D pix width */
	    );

	DMAOUTREG(MACH64_DP_WRITE_MASK, 0xffffffff);	/* enable all planes */
	DMAOUTREG(MACH64_DP_MIX, MACH64_BKGD_MIX_D | MACH64_FRGD_MIX_S);
	DMAOUTREG(MACH64_DP_SRC,
		  MACH64_BKGD_SRC_BKGD_CLR
		  | MACH64_FRGD_SRC_HOST | MACH64_MONO_SRC_ONE);

	DMAOUTREG(MACH64_DST_OFF_PITCH,
		  (blit->pitch << 22) | (blit->offset >> 3));
	DMAOUTREG(MACH64_DST_X_Y, (blit->y << 16) | blit->x);
	DMAOUTREG(MACH64_DST_WIDTH_HEIGHT, (blit->height << 16) | blit->width);

	DRM_DEBUG("%s: %d bytes\n", __FUNCTION__, buf->used);

	/* Add the buffer to the queue */
	DMAADVANCEHOSTDATA(dev_priv);

	return 0;
}

/* ================================================================
 * IOCTL functions
 */

int mach64_dma_clear(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_mach64_private_t *dev_priv = dev->dev_private;
	drm_mach64_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_mach64_clear_t clear;
	int ret;

	DRM_DEBUG("%s: pid=%d\n", __FUNCTION__, DRM_CURRENTPID);

	LOCK_TEST_WITH_RETURN(dev, filp);

	DRM_COPY_FROM_USER_IOCTL(clear, (drm_mach64_clear_t *) data,
				 sizeof(clear));

	if (sarea_priv->nbox > MACH64_NR_SAREA_CLIPRECTS)
		sarea_priv->nbox = MACH64_NR_SAREA_CLIPRECTS;

	ret = mach64_dma_dispatch_clear(filp, dev, clear.flags,
					clear.x, clear.y, clear.w, clear.h,
					clear.clear_color, clear.clear_depth);

	/* Make sure we restore the 3D state next time.
	 */
	sarea_priv->dirty |= (MACH64_UPLOAD_CONTEXT | MACH64_UPLOAD_MISC);
	return ret;
}

int mach64_dma_swap(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_mach64_private_t *dev_priv = dev->dev_private;
	drm_mach64_sarea_t *sarea_priv = dev_priv->sarea_priv;
	int ret;

	DRM_DEBUG("%s: pid=%d\n", __FUNCTION__, DRM_CURRENTPID);

	LOCK_TEST_WITH_RETURN(dev, filp);

	if (sarea_priv->nbox > MACH64_NR_SAREA_CLIPRECTS)
		sarea_priv->nbox = MACH64_NR_SAREA_CLIPRECTS;

	ret = mach64_dma_dispatch_swap(filp, dev);

	/* Make sure we restore the 3D state next time.
	 */
	sarea_priv->dirty |= (MACH64_UPLOAD_CONTEXT | MACH64_UPLOAD_MISC);
	return ret;
}

int mach64_dma_vertex(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_mach64_private_t *dev_priv = dev->dev_private;
	drm_mach64_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_mach64_vertex_t vertex;

	LOCK_TEST_WITH_RETURN(dev, filp);

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __FUNCTION__);
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL(vertex, (drm_mach64_vertex_t *) data,
				 sizeof(vertex));

	DRM_DEBUG("%s: pid=%d buf=%p used=%lu discard=%d\n",
		  __FUNCTION__, DRM_CURRENTPID,
		  vertex.buf, vertex.used, vertex.discard);

	if (vertex.prim < 0 || vertex.prim > MACH64_PRIM_POLYGON) {
		DRM_ERROR("buffer prim %d\n", vertex.prim);
		return DRM_ERR(EINVAL);
	}

	if (vertex.used > MACH64_BUFFER_SIZE || (vertex.used & 3) != 0) {
		DRM_ERROR("Invalid vertex buffer size: %lu bytes\n",
			  vertex.used);
		return DRM_ERR(EINVAL);
	}

	if (sarea_priv->nbox > MACH64_NR_SAREA_CLIPRECTS)
		sarea_priv->nbox = MACH64_NR_SAREA_CLIPRECTS;

	return mach64_dma_dispatch_vertex(filp, dev, vertex.prim, vertex.buf,
					  vertex.used, vertex.discard);
}

int mach64_dma_blit(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_device_dma_t *dma = dev->dma;
	drm_mach64_private_t *dev_priv = dev->dev_private;
	drm_mach64_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_mach64_blit_t blit;
	int ret;

	LOCK_TEST_WITH_RETURN(dev, filp);

	DRM_COPY_FROM_USER_IOCTL(blit, (drm_mach64_blit_t *) data,
				 sizeof(blit));

	DRM_DEBUG("%s: pid=%d index=%d\n",
		  __FUNCTION__, DRM_CURRENTPID, blit.idx);

	if (blit.idx < 0 || blit.idx >= dma->buf_count) {
		DRM_ERROR("buffer index %d (of %d max)\n",
			  blit.idx, dma->buf_count - 1);
		return DRM_ERR(EINVAL);
	}

	ret = mach64_dma_dispatch_blit(filp, dev, &blit);

	/* Make sure we restore the 3D state next time.
	 */
	sarea_priv->dirty |= (MACH64_UPLOAD_CONTEXT |
			      MACH64_UPLOAD_MISC | MACH64_UPLOAD_CLIPRECTS);

	return ret;
}

int mach64_get_param(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_mach64_private_t *dev_priv = dev->dev_private;
	drm_mach64_getparam_t param;
	int value;

	DRM_DEBUG("%s\n", __FUNCTION__);

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __FUNCTION__);
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL(param, (drm_mach64_getparam_t *) data,
				 sizeof(param));

	switch (param.param) {
	case MACH64_PARAM_FRAMES_QUEUED:
		/* Needs lock since it calls mach64_ring_tick() */
		LOCK_TEST_WITH_RETURN(dev, filp);
		value = mach64_do_get_frames_queued(dev_priv);
		break;
	case MACH64_PARAM_IRQ_NR:
		value = dev->irq;
		break;
	default:
		return DRM_ERR(EINVAL);
	}

	if (DRM_COPY_TO_USER(param.value, &value, sizeof(int))) {
		DRM_ERROR("copy_to_user\n");
		return DRM_ERR(EFAULT);
	}

	return 0;
}
