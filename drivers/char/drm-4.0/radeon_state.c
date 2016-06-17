/* radeon_state.c -- State support for Radeon -*- linux-c -*-
 *
 * Copyright 2000 VA Linux Systems, Inc., Fremont, California.
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

#define __NO_VERSION__
#include "drmP.h"
#include "radeon_drv.h"
#include "drm.h"
#include <linux/delay.h>


/* ================================================================
 * CP hardware state programming functions
 */

static inline void radeon_emit_clip_rect( drm_radeon_private_t *dev_priv,
					  drm_clip_rect_t *box )
{
	RING_LOCALS;

	DRM_DEBUG( "   box:  x1=%d y1=%d  x2=%d y2=%d\n",
		   box->x1, box->y1, box->x2, box->y2 );

	BEGIN_RING( 4 );

	OUT_RING( CP_PACKET0( RADEON_RE_TOP_LEFT, 0 ) );
	OUT_RING( (box->y1 << 16) | box->x1 );

	OUT_RING( CP_PACKET0( RADEON_RE_WIDTH_HEIGHT, 0 ) );
	OUT_RING( ((box->y2 - 1) << 16) | (box->x2 - 1) );

	ADVANCE_RING();
}

static inline void radeon_emit_context( drm_radeon_private_t *dev_priv )
{
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_radeon_context_regs_t *ctx = &sarea_priv->context_state;
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __FUNCTION__ );

	BEGIN_RING( 14 );

	OUT_RING( CP_PACKET0( RADEON_PP_MISC, 6 ) );
	OUT_RING( ctx->pp_misc );
	OUT_RING( ctx->pp_fog_color );
	OUT_RING( ctx->re_solid_color );
	OUT_RING( ctx->rb3d_blendcntl );
	OUT_RING( ctx->rb3d_depthoffset );
	OUT_RING( ctx->rb3d_depthpitch );
	OUT_RING( ctx->rb3d_zstencilcntl );

	OUT_RING( CP_PACKET0( RADEON_PP_CNTL, 2 ) );
	OUT_RING( ctx->pp_cntl );
	OUT_RING( ctx->rb3d_cntl );
	OUT_RING( ctx->rb3d_coloroffset );

	OUT_RING( CP_PACKET0( RADEON_RB3D_COLORPITCH, 0 ) );
	OUT_RING( ctx->rb3d_colorpitch );

	ADVANCE_RING();
}

static inline void radeon_emit_vertfmt( drm_radeon_private_t *dev_priv )
{
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_radeon_context_regs_t *ctx = &sarea_priv->context_state;
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __FUNCTION__ );

	BEGIN_RING( 2 );

	OUT_RING( CP_PACKET0( RADEON_SE_COORD_FMT, 0 ) );
	OUT_RING( ctx->se_coord_fmt );

	ADVANCE_RING();
}

static inline void radeon_emit_line( drm_radeon_private_t *dev_priv )
{
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_radeon_context_regs_t *ctx = &sarea_priv->context_state;
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __FUNCTION__ );

	BEGIN_RING( 5 );

	OUT_RING( CP_PACKET0( RADEON_RE_LINE_PATTERN, 1 ) );
	OUT_RING( ctx->re_line_pattern );
	OUT_RING( ctx->re_line_state );

	OUT_RING( CP_PACKET0( RADEON_SE_LINE_WIDTH, 0 ) );
	OUT_RING( ctx->se_line_width );

	ADVANCE_RING();
}

static inline void radeon_emit_bumpmap( drm_radeon_private_t *dev_priv )
{
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_radeon_context_regs_t *ctx = &sarea_priv->context_state;
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __FUNCTION__ );

	BEGIN_RING( 5 );

	OUT_RING( CP_PACKET0( RADEON_PP_LUM_MATRIX, 0 ) );
	OUT_RING( ctx->pp_lum_matrix );

	OUT_RING( CP_PACKET0( RADEON_PP_ROT_MATRIX_0, 1 ) );
	OUT_RING( ctx->pp_rot_matrix_0 );
	OUT_RING( ctx->pp_rot_matrix_1 );

	ADVANCE_RING();
}

static inline void radeon_emit_masks( drm_radeon_private_t *dev_priv )
{
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_radeon_context_regs_t *ctx = &sarea_priv->context_state;
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __FUNCTION__ );

	BEGIN_RING( 4 );

	OUT_RING( CP_PACKET0( RADEON_RB3D_STENCILREFMASK, 2 ) );
	OUT_RING( ctx->rb3d_stencilrefmask );
	OUT_RING( ctx->rb3d_ropcntl );
	OUT_RING( ctx->rb3d_planemask );

	ADVANCE_RING();
}

static inline void radeon_emit_viewport( drm_radeon_private_t *dev_priv )
{
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_radeon_context_regs_t *ctx = &sarea_priv->context_state;
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __FUNCTION__ );

	BEGIN_RING( 7 );

	OUT_RING( CP_PACKET0( RADEON_SE_VPORT_XSCALE, 5 ) );
	OUT_RING( ctx->se_vport_xscale );
	OUT_RING( ctx->se_vport_xoffset );
	OUT_RING( ctx->se_vport_yscale );
	OUT_RING( ctx->se_vport_yoffset );
	OUT_RING( ctx->se_vport_zscale );
	OUT_RING( ctx->se_vport_zoffset );

	ADVANCE_RING();
}

static inline void radeon_emit_setup( drm_radeon_private_t *dev_priv )
{
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_radeon_context_regs_t *ctx = &sarea_priv->context_state;
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __FUNCTION__ );

	BEGIN_RING( 4 );

	OUT_RING( CP_PACKET0( RADEON_SE_CNTL, 0 ) );
	OUT_RING( ctx->se_cntl );
	OUT_RING( CP_PACKET0( RADEON_SE_CNTL_STATUS, 0 ) );
	OUT_RING( ctx->se_cntl_status );

	ADVANCE_RING();
}

static inline void radeon_emit_tcl( drm_radeon_private_t *dev_priv )
{
#ifdef TCL_ENABLE
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_radeon_context_regs_t *ctx = &sarea_priv->context_state;
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __FUNCTION__ );

	BEGIN_RING( 29 );

	OUT_RING( CP_PACKET0( RADEON_SE_TCL_MATERIAL_EMMISSIVE_RED, 27 ) );
	OUT_RING( ctx->se_tcl_material_emmissive.red );
	OUT_RING( ctx->se_tcl_material_emmissive.green );
	OUT_RING( ctx->se_tcl_material_emmissive.blue );
	OUT_RING( ctx->se_tcl_material_emmissive.alpha );
	OUT_RING( ctx->se_tcl_material_ambient.red );
	OUT_RING( ctx->se_tcl_material_ambient.green );
	OUT_RING( ctx->se_tcl_material_ambient.blue );
	OUT_RING( ctx->se_tcl_material_ambient.alpha );
	OUT_RING( ctx->se_tcl_material_diffuse.red );
	OUT_RING( ctx->se_tcl_material_diffuse.green );
	OUT_RING( ctx->se_tcl_material_diffuse.blue );
	OUT_RING( ctx->se_tcl_material_diffuse.alpha );
	OUT_RING( ctx->se_tcl_material_specular.red );
	OUT_RING( ctx->se_tcl_material_specular.green );
	OUT_RING( ctx->se_tcl_material_specular.blue );
	OUT_RING( ctx->se_tcl_material_specular.alpha );
	OUT_RING( ctx->se_tcl_shininess );
	OUT_RING( ctx->se_tcl_output_vtx_fmt );
	OUT_RING( ctx->se_tcl_output_vtx_sel );
	OUT_RING( ctx->se_tcl_matrix_select_0 );
	OUT_RING( ctx->se_tcl_matrix_select_1 );
	OUT_RING( ctx->se_tcl_ucp_vert_blend_ctl );
	OUT_RING( ctx->se_tcl_texture_proc_ctl );
	OUT_RING( ctx->se_tcl_light_model_ctl );
	for ( i = 0 ; i < 4 ; i++ ) {
		OUT_RING( ctx->se_tcl_per_light_ctl[i] );
	}

	ADVANCE_RING();
#else
	DRM_ERROR( "TCL not enabled!\n" );
#endif
}

static inline void radeon_emit_misc( drm_radeon_private_t *dev_priv )
{
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_radeon_context_regs_t *ctx = &sarea_priv->context_state;
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __FUNCTION__ );

	BEGIN_RING( 2 );

	OUT_RING( CP_PACKET0( RADEON_RE_MISC, 0 ) );
	OUT_RING( ctx->re_misc );

	ADVANCE_RING();
}

static inline void radeon_emit_tex0( drm_radeon_private_t *dev_priv )
{
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_radeon_texture_regs_t *tex = &sarea_priv->tex_state[0];
	RING_LOCALS;
	DRM_DEBUG( "    %s: offset=0x%x\n", __FUNCTION__, tex->pp_txoffset );

	BEGIN_RING( 9 );

	OUT_RING( CP_PACKET0( RADEON_PP_TXFILTER_0, 5 ) );
	OUT_RING( tex->pp_txfilter );
	OUT_RING( tex->pp_txformat );
	OUT_RING( tex->pp_txoffset );
	OUT_RING( tex->pp_txcblend );
	OUT_RING( tex->pp_txablend );
	OUT_RING( tex->pp_tfactor );

	OUT_RING( CP_PACKET0( RADEON_PP_BORDER_COLOR_0, 0 ) );
	OUT_RING( tex->pp_border_color );

	ADVANCE_RING();
}

static inline void radeon_emit_tex1( drm_radeon_private_t *dev_priv )
{
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_radeon_texture_regs_t *tex = &sarea_priv->tex_state[1];
	RING_LOCALS;
	DRM_DEBUG( "    %s: offset=0x%x\n", __FUNCTION__, tex->pp_txoffset );

	BEGIN_RING( 9 );

	OUT_RING( CP_PACKET0( RADEON_PP_TXFILTER_1, 5 ) );
	OUT_RING( tex->pp_txfilter );
	OUT_RING( tex->pp_txformat );
	OUT_RING( tex->pp_txoffset );
	OUT_RING( tex->pp_txcblend );
	OUT_RING( tex->pp_txablend );
	OUT_RING( tex->pp_tfactor );

	OUT_RING( CP_PACKET0( RADEON_PP_BORDER_COLOR_1, 0 ) );
	OUT_RING( tex->pp_border_color );

	ADVANCE_RING();
}

static inline void radeon_emit_tex2( drm_radeon_private_t *dev_priv )
{
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_radeon_texture_regs_t *tex = &sarea_priv->tex_state[2];
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __FUNCTION__ );

	BEGIN_RING( 9 );

	OUT_RING( CP_PACKET0( RADEON_PP_TXFILTER_2, 5 ) );
	OUT_RING( tex->pp_txfilter );
	OUT_RING( tex->pp_txformat );
	OUT_RING( tex->pp_txoffset );
	OUT_RING( tex->pp_txcblend );
	OUT_RING( tex->pp_txablend );
	OUT_RING( tex->pp_tfactor );

	OUT_RING( CP_PACKET0( RADEON_PP_BORDER_COLOR_2, 0 ) );
	OUT_RING( tex->pp_border_color );

	ADVANCE_RING();
}

static inline void radeon_emit_state( drm_radeon_private_t *dev_priv )
{
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int dirty = sarea_priv->dirty;

	DRM_DEBUG( "%s: dirty=0x%08x\n", __FUNCTION__, dirty );

	if ( dirty & RADEON_UPLOAD_CONTEXT ) {
		radeon_emit_context( dev_priv );
		sarea_priv->dirty &= ~RADEON_UPLOAD_CONTEXT;
	}

	if ( dirty & RADEON_UPLOAD_VERTFMT ) {
		radeon_emit_vertfmt( dev_priv );
		sarea_priv->dirty &= ~RADEON_UPLOAD_VERTFMT;
	}

	if ( dirty & RADEON_UPLOAD_LINE ) {
		radeon_emit_line( dev_priv );
		sarea_priv->dirty &= ~RADEON_UPLOAD_LINE;
	}

	if ( dirty & RADEON_UPLOAD_BUMPMAP ) {
		radeon_emit_bumpmap( dev_priv );
		sarea_priv->dirty &= ~RADEON_UPLOAD_BUMPMAP;
	}

	if ( dirty & RADEON_UPLOAD_MASKS ) {
		radeon_emit_masks( dev_priv );
		sarea_priv->dirty &= ~RADEON_UPLOAD_MASKS;
	}

	if ( dirty & RADEON_UPLOAD_VIEWPORT ) {
		radeon_emit_viewport( dev_priv );
		sarea_priv->dirty &= ~RADEON_UPLOAD_VIEWPORT;
	}

	if ( dirty & RADEON_UPLOAD_SETUP ) {
		radeon_emit_setup( dev_priv );
		sarea_priv->dirty &= ~RADEON_UPLOAD_SETUP;
	}

	if ( dirty & RADEON_UPLOAD_TCL ) {
#ifdef TCL_ENABLE
		radeon_emit_tcl( dev_priv );
#endif
		sarea_priv->dirty &= ~RADEON_UPLOAD_TCL;
	}

	if ( dirty & RADEON_UPLOAD_MISC ) {
		radeon_emit_misc( dev_priv );
		sarea_priv->dirty &= ~RADEON_UPLOAD_MISC;
	}

	if ( dirty & RADEON_UPLOAD_TEX0 ) {
		radeon_emit_tex0( dev_priv );
		sarea_priv->dirty &= ~RADEON_UPLOAD_TEX0;
	}

	if ( dirty & RADEON_UPLOAD_TEX1 ) {
		radeon_emit_tex1( dev_priv );
		sarea_priv->dirty &= ~RADEON_UPLOAD_TEX1;
	}

	if ( dirty & RADEON_UPLOAD_TEX2 ) {
#if 0
		radeon_emit_tex2( dev_priv );
#endif
		sarea_priv->dirty &= ~RADEON_UPLOAD_TEX2;
	}

	sarea_priv->dirty &= ~(RADEON_UPLOAD_TEX0IMAGES |
			       RADEON_UPLOAD_TEX1IMAGES |
			       RADEON_UPLOAD_TEX2IMAGES |
			       RADEON_REQUIRE_QUIESCENCE);
}


#if RADEON_PERFORMANCE_BOXES
/* ================================================================
 * Performance monitoring functions
 */

static void radeon_clear_box( drm_radeon_private_t *dev_priv,
			      int x, int y, int w, int h,
			      int r, int g, int b )
{
	u32 pitch, offset;
	u32 color;
	RING_LOCALS;

	switch ( dev_priv->color_fmt ) {
	case RADEON_COLOR_FORMAT_RGB565:
		color = (((r & 0xf8) << 8) |
			 ((g & 0xfc) << 3) |
			 ((b & 0xf8) >> 3));
		break;
	case RADEON_COLOR_FORMAT_ARGB8888:
	default:
		color = (((0xff) << 24) | (r << 16) | (g <<  8) | b);
		break;
	}

	offset = dev_priv->back_offset;
	pitch = dev_priv->back_pitch >> 3;

	BEGIN_RING( 6 );

	OUT_RING( CP_PACKET3( RADEON_CNTL_PAINT_MULTI, 4 ) );
	OUT_RING( RADEON_GMC_DST_PITCH_OFFSET_CNTL |
		  RADEON_GMC_BRUSH_SOLID_COLOR |
		  (dev_priv->color_fmt << 8) |
		  RADEON_GMC_SRC_DATATYPE_COLOR |
		  RADEON_ROP3_P |
		  RADEON_GMC_CLR_CMP_CNTL_DIS );

	OUT_RING( (pitch << 22) | (offset >> 5) );
	OUT_RING( color );

	OUT_RING( (x << 16) | y );
	OUT_RING( (w << 16) | h );

	ADVANCE_RING();
}

static void radeon_cp_performance_boxes( drm_radeon_private_t *dev_priv )
{
	if ( atomic_read( &dev_priv->idle_count ) == 0 ) {
		radeon_clear_box( dev_priv, 64, 4, 8, 8, 0, 255, 0 );
	} else {
		atomic_set( &dev_priv->idle_count, 0 );
	}
}

#endif


/* ================================================================
 * CP command dispatch functions
 */

static void radeon_print_dirty( const char *msg, unsigned int flags )
{
	DRM_DEBUG( "%s: (0x%x) %s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
		   msg,
		   flags,
		   (flags & RADEON_UPLOAD_CONTEXT)     ? "context, " : "",
		   (flags & RADEON_UPLOAD_VERTFMT)     ? "vertfmt, " : "",
		   (flags & RADEON_UPLOAD_LINE)        ? "line, " : "",
		   (flags & RADEON_UPLOAD_BUMPMAP)     ? "bumpmap, " : "",
		   (flags & RADEON_UPLOAD_MASKS)       ? "masks, " : "",
		   (flags & RADEON_UPLOAD_VIEWPORT)    ? "viewport, " : "",
		   (flags & RADEON_UPLOAD_SETUP)       ? "setup, " : "",
		   (flags & RADEON_UPLOAD_TCL)         ? "tcl, " : "",
		   (flags & RADEON_UPLOAD_MISC)        ? "misc, " : "",
		   (flags & RADEON_UPLOAD_TEX0)        ? "tex0, " : "",
		   (flags & RADEON_UPLOAD_TEX1)        ? "tex1, " : "",
		   (flags & RADEON_UPLOAD_TEX2)        ? "tex2, " : "",
		   (flags & RADEON_UPLOAD_CLIPRECTS)   ? "cliprects, " : "",
		   (flags & RADEON_REQUIRE_QUIESCENCE) ? "quiescence, " : "" );
}

static void radeon_cp_dispatch_clear( drm_device_t *dev,
				      drm_radeon_clear_t *clear )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	int nbox = sarea_priv->nbox;
	drm_clip_rect_t *pbox = sarea_priv->boxes;
	unsigned int flags = clear->flags;
	int i;
	RING_LOCALS;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	radeon_update_ring_snapshot( dev_priv );

	if ( dev_priv->page_flipping && dev_priv->current_page == 1 ) {
		unsigned int tmp = flags;

		flags &= ~(RADEON_FRONT | RADEON_BACK);
		if ( tmp & RADEON_FRONT ) flags |= RADEON_BACK;
		if ( tmp & RADEON_BACK )  flags |= RADEON_FRONT;
	}

	for ( i = 0 ; i < nbox ; i++ ) {
		int x = pbox[i].x1;
		int y = pbox[i].y1;
		int w = pbox[i].x2 - x;
		int h = pbox[i].y2 - y;

		DRM_DEBUG( "dispatch clear %d,%d-%d,%d flags 0x%x\n",
			   x, y, w, h, flags );

		if ( flags & (RADEON_FRONT | RADEON_BACK) ) {
			BEGIN_RING( 4 );

			/* Ensure the 3D stream is idle before doing a
			 * 2D fill to clear the front or back buffer.
			 */
			RADEON_WAIT_UNTIL_3D_IDLE();

			OUT_RING( CP_PACKET0( RADEON_DP_WRITE_MASK, 0 ) );
			OUT_RING( sarea_priv->context_state.rb3d_planemask );

			ADVANCE_RING();

			/* Make sure we restore the 3D state next time.
			 */
			dev_priv->sarea_priv->dirty |= (RADEON_UPLOAD_CONTEXT |
							RADEON_UPLOAD_MASKS);
		}

		if ( flags & RADEON_FRONT ) {
			BEGIN_RING( 6 );

			OUT_RING( CP_PACKET3( RADEON_CNTL_PAINT_MULTI, 4 ) );
			OUT_RING( RADEON_GMC_DST_PITCH_OFFSET_CNTL |
				  RADEON_GMC_BRUSH_SOLID_COLOR |
				  (dev_priv->color_fmt << 8) |
				  RADEON_GMC_SRC_DATATYPE_COLOR |
				  RADEON_ROP3_P |
				  RADEON_GMC_CLR_CMP_CNTL_DIS );

			OUT_RING( dev_priv->front_pitch_offset );
			OUT_RING( clear->clear_color );

			OUT_RING( (x << 16) | y );
			OUT_RING( (w << 16) | h );

			ADVANCE_RING();
		}

		if ( flags & RADEON_BACK ) {
			BEGIN_RING( 6 );

			OUT_RING( CP_PACKET3( RADEON_CNTL_PAINT_MULTI, 4 ) );
			OUT_RING( RADEON_GMC_DST_PITCH_OFFSET_CNTL |
				  RADEON_GMC_BRUSH_SOLID_COLOR |
				  (dev_priv->color_fmt << 8) |
				  RADEON_GMC_SRC_DATATYPE_COLOR |
				  RADEON_ROP3_P |
				  RADEON_GMC_CLR_CMP_CNTL_DIS );

			OUT_RING( dev_priv->back_pitch_offset );
			OUT_RING( clear->clear_color );

			OUT_RING( (x << 16) | y );
			OUT_RING( (w << 16) | h );

			ADVANCE_RING();

		}

		if ( flags & RADEON_DEPTH ) {
			drm_radeon_depth_clear_t *depth_clear =
			   &dev_priv->depth_clear;

			if ( sarea_priv->dirty & ~RADEON_UPLOAD_CLIPRECTS ) {
				radeon_emit_state( dev_priv );
			}

			/* FIXME: Render a rectangle to clear the depth
			 * buffer.  So much for those "fast Z clears"...
			 */
			BEGIN_RING( 23 );

			RADEON_WAIT_UNTIL_2D_IDLE();

			OUT_RING( CP_PACKET0( RADEON_PP_CNTL, 1 ) );
			OUT_RING( 0x00000000 );
			OUT_RING( depth_clear->rb3d_cntl );
			OUT_RING( CP_PACKET0( RADEON_RB3D_ZSTENCILCNTL, 0 ) );
			OUT_RING( depth_clear->rb3d_zstencilcntl );
			OUT_RING( CP_PACKET0( RADEON_RB3D_PLANEMASK, 0 ) );
			OUT_RING( 0x00000000 );
			OUT_RING( CP_PACKET0( RADEON_SE_CNTL, 0 ) );
			OUT_RING( depth_clear->se_cntl );

			OUT_RING( CP_PACKET3( RADEON_3D_DRAW_IMMD, 10 ) );
			OUT_RING( RADEON_VTX_Z_PRESENT );
			OUT_RING( (RADEON_PRIM_TYPE_RECT_LIST |
				   RADEON_PRIM_WALK_RING |
				   RADEON_MAOS_ENABLE |
				   RADEON_VTX_FMT_RADEON_MODE |
				   (3 << RADEON_NUM_VERTICES_SHIFT)) );

			OUT_RING( clear->rect.ui[CLEAR_X1] );
			OUT_RING( clear->rect.ui[CLEAR_Y1] );
			OUT_RING( clear->rect.ui[CLEAR_DEPTH] );

			OUT_RING( clear->rect.ui[CLEAR_X1] );
			OUT_RING( clear->rect.ui[CLEAR_Y2] );
			OUT_RING( clear->rect.ui[CLEAR_DEPTH] );

			OUT_RING( clear->rect.ui[CLEAR_X2] );
			OUT_RING( clear->rect.ui[CLEAR_Y2] );
			OUT_RING( clear->rect.ui[CLEAR_DEPTH] );

			ADVANCE_RING();

			/* Make sure we restore the 3D state next time.
			 */
			dev_priv->sarea_priv->dirty |= (RADEON_UPLOAD_CONTEXT |
							RADEON_UPLOAD_SETUP |
							RADEON_UPLOAD_MASKS);
		}
	}

	/* Increment the clear counter.  The client-side 3D driver must
	 * wait on this value before performing the clear ioctl.  We
	 * need this because the card's so damned fast...
	 */
	dev_priv->sarea_priv->last_clear++;

	BEGIN_RING( 4 );

	RADEON_CLEAR_AGE( dev_priv->sarea_priv->last_clear );
	RADEON_WAIT_UNTIL_IDLE();

	ADVANCE_RING();
}

static void radeon_cp_dispatch_swap( drm_device_t *dev )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	int nbox = sarea_priv->nbox;
	drm_clip_rect_t *pbox = sarea_priv->boxes;
	int i;
	RING_LOCALS;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	radeon_update_ring_snapshot( dev_priv );

#if RADEON_PERFORMANCE_BOXES
	/* Do some trivial performance monitoring...
	 */
	radeon_cp_performance_boxes( dev_priv );
#endif

	/* Wait for the 3D stream to idle before dispatching the bitblt.
	 * This will prevent data corruption between the two streams.
	 */
	BEGIN_RING( 2 );

	RADEON_WAIT_UNTIL_3D_IDLE();

	ADVANCE_RING();

	for ( i = 0 ; i < nbox ; i++ ) {
		int x = pbox[i].x1;
		int y = pbox[i].y1;
		int w = pbox[i].x2 - x;
		int h = pbox[i].y2 - y;

		DRM_DEBUG( "dispatch swap %d,%d-%d,%d\n",
			   x, y, w, h );

		BEGIN_RING( 7 );

		OUT_RING( CP_PACKET3( RADEON_CNTL_BITBLT_MULTI, 5 ) );
		OUT_RING( RADEON_GMC_SRC_PITCH_OFFSET_CNTL |
			  RADEON_GMC_DST_PITCH_OFFSET_CNTL |
			  RADEON_GMC_BRUSH_NONE |
			  (dev_priv->color_fmt << 8) |
			  RADEON_GMC_SRC_DATATYPE_COLOR |
			  RADEON_ROP3_S |
			  RADEON_DP_SRC_SOURCE_MEMORY |
			  RADEON_GMC_CLR_CMP_CNTL_DIS |
			  RADEON_GMC_WR_MSK_DIS );

		OUT_RING( dev_priv->back_pitch_offset );
		OUT_RING( dev_priv->front_pitch_offset );

		OUT_RING( (x << 16) | y );
		OUT_RING( (x << 16) | y );
		OUT_RING( (w << 16) | h );

		ADVANCE_RING();
	}

	/* Increment the frame counter.  The client-side 3D driver must
	 * throttle the framerate by waiting for this value before
	 * performing the swapbuffer ioctl.
	 */
	dev_priv->sarea_priv->last_frame++;

	BEGIN_RING( 4 );

	RADEON_FRAME_AGE( dev_priv->sarea_priv->last_frame );
	RADEON_WAIT_UNTIL_2D_IDLE();

	ADVANCE_RING();
}

static void radeon_cp_dispatch_flip( drm_device_t *dev )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	RING_LOCALS;
	DRM_DEBUG( "%s: page=%d\n", __FUNCTION__, dev_priv->current_page );

	radeon_update_ring_snapshot( dev_priv );

#if RADEON_PERFORMANCE_BOXES
	/* Do some trivial performance monitoring...
	 */
	radeon_cp_performance_boxes( dev_priv );
#endif

	BEGIN_RING( 6 );

	RADEON_WAIT_UNTIL_3D_IDLE();
	RADEON_WAIT_UNTIL_PAGE_FLIPPED();

	OUT_RING( CP_PACKET0( RADEON_CRTC_OFFSET, 0 ) );

	if ( dev_priv->current_page == 0 ) {
		OUT_RING( dev_priv->back_offset );
		dev_priv->current_page = 1;
	} else {
		OUT_RING( dev_priv->front_offset );
		dev_priv->current_page = 0;
	}

	ADVANCE_RING();

	/* Increment the frame counter.  The client-side 3D driver must
	 * throttle the framerate by waiting for this value before
	 * performing the swapbuffer ioctl.
	 */
	dev_priv->sarea_priv->last_frame++;

	BEGIN_RING( 2 );

	RADEON_FRAME_AGE( dev_priv->sarea_priv->last_frame );

	ADVANCE_RING();
}

static void radeon_cp_dispatch_vertex( drm_device_t *dev,
				       drm_buf_t *buf )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_buf_priv_t *buf_priv = buf->dev_private;
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	int format = sarea_priv->vc_format;
	int offset = dev_priv->agp_buffers_offset + buf->offset;
	int size = buf->used;
	int prim = buf_priv->prim;
	int i = 0;
	RING_LOCALS;
	DRM_DEBUG( "%s: nbox=%d\n", __FUNCTION__, sarea_priv->nbox );

	radeon_update_ring_snapshot( dev_priv );

	if ( 0 )
		radeon_print_dirty( "dispatch_vertex", sarea_priv->dirty );

	if ( buf->used ) {
		buf_priv->dispatched = 1;

		if ( sarea_priv->dirty & ~RADEON_UPLOAD_CLIPRECTS ) {
			radeon_emit_state( dev_priv );
		}

		do {
			/* Emit the next set of up to three cliprects */
			if ( i < sarea_priv->nbox ) {
				radeon_emit_clip_rect( dev_priv,
						       &sarea_priv->boxes[i] );
			}

			/* Emit the vertex buffer rendering commands */
			BEGIN_RING( 5 );

			OUT_RING( CP_PACKET3( RADEON_3D_RNDR_GEN_INDX_PRIM, 3 ) );
			OUT_RING( offset );
			OUT_RING( size );
			OUT_RING( format );
			OUT_RING( prim | RADEON_PRIM_WALK_LIST |
				  RADEON_COLOR_ORDER_RGBA |
				  RADEON_VTX_FMT_RADEON_MODE |
				  (size << RADEON_NUM_VERTICES_SHIFT) );

			ADVANCE_RING();

			i++;
		} while ( i < sarea_priv->nbox );
	}

	if ( buf_priv->discard ) {
		buf_priv->age = dev_priv->sarea_priv->last_dispatch;

		/* Emit the vertex buffer age */
		BEGIN_RING( 2 );
		RADEON_DISPATCH_AGE( buf_priv->age );
		ADVANCE_RING();

		buf->pending = 1;
		buf->used = 0;
		/* FIXME: Check dispatched field */
		buf_priv->dispatched = 0;
	}

	dev_priv->sarea_priv->last_dispatch++;

	sarea_priv->dirty &= ~RADEON_UPLOAD_CLIPRECTS;
	sarea_priv->nbox = 0;
}


static void radeon_cp_dispatch_indirect( drm_device_t *dev,
					 drm_buf_t *buf,
					 int start, int end )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_buf_priv_t *buf_priv = buf->dev_private;
	RING_LOCALS;
	DRM_DEBUG( "indirect: buf=%d s=0x%x e=0x%x\n",
		   buf->idx, start, end );

	radeon_update_ring_snapshot( dev_priv );

	if ( start != end ) {
		int offset = (dev_priv->agp_buffers_offset
			      + buf->offset + start);
		int dwords = (end - start + 3) / sizeof(u32);

		/* Indirect buffer data must be an even number of
		 * dwords, so if we've been given an odd number we must
		 * pad the data with a Type-2 CP packet.
		 */
		if ( dwords & 1 ) {
			u32 *data = (u32 *)
				((char *)dev_priv->buffers->handle
				 + buf->offset + start);
			data[dwords++] = RADEON_CP_PACKET2;
		}

		buf_priv->dispatched = 1;

		/* Fire off the indirect buffer */
		BEGIN_RING( 3 );

		OUT_RING( CP_PACKET0( RADEON_CP_IB_BASE, 1 ) );
		OUT_RING( offset );
		OUT_RING( dwords );

		ADVANCE_RING();
	}

	if ( buf_priv->discard ) {
		buf_priv->age = dev_priv->sarea_priv->last_dispatch;

		/* Emit the indirect buffer age */
		BEGIN_RING( 2 );
		RADEON_DISPATCH_AGE( buf_priv->age );
		ADVANCE_RING();

		buf->pending = 1;
		buf->used = 0;
		/* FIXME: Check dispatched field */
		buf_priv->dispatched = 0;
	}

	dev_priv->sarea_priv->last_dispatch++;
}

static void radeon_cp_dispatch_indices( drm_device_t *dev,
					drm_buf_t *buf,
					int start, int end,
					int count )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_buf_priv_t *buf_priv = buf->dev_private;
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	int format = sarea_priv->vc_format;
	int offset = dev_priv->agp_buffers_offset;
	int prim = buf_priv->prim;
	u32 *data;
	int dwords;
	int i = 0;
	RING_LOCALS;
	DRM_DEBUG( "indices: s=%d e=%d c=%d\n", start, end, count );

	radeon_update_ring_snapshot( dev_priv );

	if ( 0 )
		radeon_print_dirty( "dispatch_indices", sarea_priv->dirty );

	if ( start != end ) {
		buf_priv->dispatched = 1;

		if ( sarea_priv->dirty & ~RADEON_UPLOAD_CLIPRECTS ) {
			radeon_emit_state( dev_priv );
		}

		dwords = (end - start + 3) / sizeof(u32);

		data = (u32 *)((char *)dev_priv->buffers->handle
			       + buf->offset + start);

		data[0] = CP_PACKET3( RADEON_3D_RNDR_GEN_INDX_PRIM, dwords-2 );

		data[1] = offset;
		data[2] = RADEON_MAX_VB_VERTS;
		data[3] = format;
		data[4] = (prim | RADEON_PRIM_WALK_IND |
			   RADEON_COLOR_ORDER_RGBA |
			   RADEON_VTX_FMT_RADEON_MODE |
			   (count << RADEON_NUM_VERTICES_SHIFT) );

		if ( count & 0x1 ) {
			data[dwords-1] &= 0x0000ffff;
		}

		do {
			/* Emit the next set of up to three cliprects */
			if ( i < sarea_priv->nbox ) {
				radeon_emit_clip_rect( dev_priv,
						       &sarea_priv->boxes[i] );
			}

			radeon_cp_dispatch_indirect( dev, buf, start, end );

			i++;
		} while ( i < sarea_priv->nbox );
	}

	if ( buf_priv->discard ) {
		buf_priv->age = dev_priv->sarea_priv->last_dispatch;

		/* Emit the vertex buffer age */
		BEGIN_RING( 2 );
		RADEON_DISPATCH_AGE( buf_priv->age );
		ADVANCE_RING();

		buf->pending = 1;
		/* FIXME: Check dispatched field */
		buf_priv->dispatched = 0;
	}

	dev_priv->sarea_priv->last_dispatch++;

	sarea_priv->dirty &= ~RADEON_UPLOAD_CLIPRECTS;
	sarea_priv->nbox = 0;
}

static int radeon_cp_dispatch_blit( drm_device_t *dev,
				    drm_radeon_blit_t *blit )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf;
	drm_radeon_buf_priv_t *buf_priv;
	u32 format;
	u32 *data;
	int dword_shift, dwords;
	RING_LOCALS;
	DRM_DEBUG( "blit: ofs=0x%x p=%d f=%d x=%hd y=%hd w=%hd h=%hd\n",
		   blit->offset >> 10, blit->pitch, blit->format,
		   blit->x, blit->y, blit->width, blit->height );

	radeon_update_ring_snapshot( dev_priv );

	/* The compiler won't optimize away a division by a variable,
	 * even if the only legal values are powers of two.  Thus, we'll
	 * use a shift instead.
	 */
	switch ( blit->format ) {
	case RADEON_TXF_32BPP_ARGB8888:
	case RADEON_TXF_32BPP_RGBA8888:
		format = RADEON_COLOR_FORMAT_ARGB8888;
		dword_shift = 0;
		break;
	case RADEON_TXF_16BPP_AI88:
	case RADEON_TXF_16BPP_ARGB1555:
	case RADEON_TXF_16BPP_RGB565:
	case RADEON_TXF_16BPP_ARGB4444:
		format = RADEON_COLOR_FORMAT_RGB565;
		dword_shift = 1;
		break;
	case RADEON_TXF_8BPP_I:
	case RADEON_TXF_8BPP_RGB332:
		format = RADEON_COLOR_FORMAT_CI8;
		dword_shift = 2;
		break;
	default:
		DRM_ERROR( "invalid blit format %d\n", blit->format );
		return -EINVAL;
	}

	/* Flush the pixel cache.  This ensures no pixel data gets mixed
	 * up with the texture data from the host data blit, otherwise
	 * part of the texture image may be corrupted.
	 */
	BEGIN_RING( 4 );

	RADEON_FLUSH_CACHE();
	RADEON_WAIT_UNTIL_IDLE();

	ADVANCE_RING();

	/* Dispatch the indirect buffer.
	 */
	buf = dma->buflist[blit->idx];
	buf_priv = buf->dev_private;

	if ( buf->pid != current->pid ) {
		DRM_ERROR( "process %d using buffer owned by %d\n",
			   current->pid, buf->pid );
		return -EINVAL;
	}
	if ( buf->pending ) {
		DRM_ERROR( "sending pending buffer %d\n", blit->idx );
		return -EINVAL;
	}

	buf_priv->discard = 1;

	dwords = (blit->width * blit->height) >> dword_shift;
	if ( !dwords ) dwords = 1;

	data = (u32 *)((char *)dev_priv->buffers->handle + buf->offset);

	data[0] = CP_PACKET3( RADEON_CNTL_HOSTDATA_BLT, dwords + 6 );
	data[1] = (RADEON_GMC_DST_PITCH_OFFSET_CNTL |
		   RADEON_GMC_BRUSH_NONE |
		   (format << 8) |
		   RADEON_GMC_SRC_DATATYPE_COLOR |
		   RADEON_ROP3_S |
		   RADEON_DP_SRC_SOURCE_HOST_DATA |
		   RADEON_GMC_CLR_CMP_CNTL_DIS |
		   RADEON_GMC_WR_MSK_DIS);

	data[2] = (blit->pitch << 22) | (blit->offset >> 10);
	data[3] = 0xffffffff;
	data[4] = 0xffffffff;
	data[5] = (blit->y << 16) | blit->x;
	data[6] = (blit->height << 16) | blit->width;
	data[7] = dwords;

	buf->used = (dwords + 8) * sizeof(u32);

	radeon_cp_dispatch_indirect( dev, buf, 0, buf->used );

	/* Flush the pixel cache after the blit completes.  This ensures
	 * the texture data is written out to memory before rendering
	 * continues.
	 */
	BEGIN_RING( 4 );

	RADEON_FLUSH_CACHE();
	RADEON_WAIT_UNTIL_2D_IDLE();

	ADVANCE_RING();

	return 0;
}

static void radeon_cp_dispatch_stipple( drm_device_t *dev, u32 *stipple )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	int i;
	RING_LOCALS;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	radeon_update_ring_snapshot( dev_priv );

	BEGIN_RING( 35 );

	OUT_RING( CP_PACKET0( RADEON_RE_STIPPLE_ADDR, 0 ) );
	OUT_RING( 0x00000000 );

	OUT_RING( CP_PACKET0_TABLE( RADEON_RE_STIPPLE_DATA, 31 ) );
	for ( i = 0 ; i < 32 ; i++ ) {
		OUT_RING( stipple[i] );
	}

	ADVANCE_RING();
}


/* ================================================================
 * IOCTL functions
 */

int radeon_cp_clear( struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_radeon_clear_t clear;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	if ( !_DRM_LOCK_IS_HELD( dev->lock.hw_lock->lock ) ||
	     dev->lock.pid != current->pid ) {
		DRM_ERROR( "%s called without lock held\n", __FUNCTION__ );
		return -EINVAL;
	}

	if ( copy_from_user( &clear, (drm_radeon_clear_t *) arg,
			     sizeof(clear) ) )
		return -EFAULT;

	if ( sarea_priv->nbox > RADEON_NR_SAREA_CLIPRECTS )
		sarea_priv->nbox = RADEON_NR_SAREA_CLIPRECTS;

	radeon_cp_dispatch_clear( dev, &clear );

	return 0;
}

int radeon_cp_swap( struct inode *inode, struct file *filp,
		    unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	if ( !_DRM_LOCK_IS_HELD( dev->lock.hw_lock->lock ) ||
	     dev->lock.pid != current->pid ) {
		DRM_ERROR( "%s called without lock held\n", __FUNCTION__ );
		return -EINVAL;
	}

	if ( sarea_priv->nbox > RADEON_NR_SAREA_CLIPRECTS )
		sarea_priv->nbox = RADEON_NR_SAREA_CLIPRECTS;

	if ( !dev_priv->page_flipping ) {
		radeon_cp_dispatch_swap( dev );
		dev_priv->sarea_priv->dirty |= (RADEON_UPLOAD_CONTEXT |
						RADEON_UPLOAD_MASKS);
	} else {
		radeon_cp_dispatch_flip( dev );
	}

	return 0;
}

int radeon_cp_vertex( struct inode *inode, struct file *filp,
		      unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf;
	drm_radeon_buf_priv_t *buf_priv;
	drm_radeon_vertex_t vertex;

	if ( !_DRM_LOCK_IS_HELD( dev->lock.hw_lock->lock ) ||
	     dev->lock.pid != current->pid ) {
		DRM_ERROR( "%s called without lock held\n", __FUNCTION__ );
		return -EINVAL;
	}
	if ( !dev_priv || dev_priv->is_pci ) {
		DRM_ERROR( "%s called with a PCI card\n", __FUNCTION__ );
		return -EINVAL;
	}

	if ( copy_from_user( &vertex, (drm_radeon_vertex_t *)arg,
			     sizeof(vertex) ) )
		return -EFAULT;

	DRM_DEBUG( "%s: pid=%d index=%d count=%d discard=%d\n",
		   __FUNCTION__, current->pid,
		   vertex.idx, vertex.count, vertex.discard );

	if ( vertex.idx < 0 || vertex.idx >= dma->buf_count ) {
		DRM_ERROR( "buffer index %d (of %d max)\n",
			   vertex.idx, dma->buf_count - 1 );
		return -EINVAL;
	}
	if ( vertex.prim < 0 ||
	     vertex.prim > RADEON_PRIM_TYPE_3VRT_LINE_LIST ) {
		DRM_ERROR( "buffer prim %d\n", vertex.prim );
		return -EINVAL;
	}

	VB_AGE_CHECK_WITH_RET( dev_priv );

	buf = dma->buflist[vertex.idx];
	buf_priv = buf->dev_private;

	if ( buf->pid != current->pid ) {
		DRM_ERROR( "process %d using buffer owned by %d\n",
			   current->pid, buf->pid );
		return -EINVAL;
	}
	if ( buf->pending ) {
		DRM_ERROR( "sending pending buffer %d\n", vertex.idx );
		return -EINVAL;
	}

	buf->used = vertex.count;
	buf_priv->prim = vertex.prim;
	buf_priv->discard = vertex.discard;

	radeon_cp_dispatch_vertex( dev, buf );

	return 0;
}

int radeon_cp_indices( struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf;
	drm_radeon_buf_priv_t *buf_priv;
	drm_radeon_indices_t elts;
	int count;

	if ( !_DRM_LOCK_IS_HELD( dev->lock.hw_lock->lock ) ||
	     dev->lock.pid != current->pid ) {
		DRM_ERROR( "%s called without lock held\n", __FUNCTION__ );
		return -EINVAL;
	}
	if ( !dev_priv || dev_priv->is_pci ) {
		DRM_ERROR( "%s called with a PCI card\n", __FUNCTION__ );
		return -EINVAL;
	}

	if ( copy_from_user( &elts, (drm_radeon_indices_t *)arg,
			     sizeof(elts) ) )
		return -EFAULT;

	DRM_DEBUG( "%s: pid=%d index=%d start=%d end=%d discard=%d\n",
		   __FUNCTION__, current->pid,
		   elts.idx, elts.start, elts.end, elts.discard );

	if ( elts.idx < 0 || elts.idx >= dma->buf_count ) {
		DRM_ERROR( "buffer index %d (of %d max)\n",
			   elts.idx, dma->buf_count - 1 );
		return -EINVAL;
	}
	if ( elts.prim < 0 ||
	     elts.prim > RADEON_PRIM_TYPE_3VRT_LINE_LIST ) {
		DRM_ERROR( "buffer prim %d\n", elts.prim );
		return -EINVAL;
	}

	VB_AGE_CHECK_WITH_RET( dev_priv );

	buf = dma->buflist[elts.idx];
	buf_priv = buf->dev_private;

	if ( buf->pid != current->pid ) {
		DRM_ERROR( "process %d using buffer owned by %d\n",
			   current->pid, buf->pid );
		return -EINVAL;
	}
	if ( buf->pending ) {
		DRM_ERROR( "sending pending buffer %d\n", elts.idx );
		return -EINVAL;
	}

	count = (elts.end - elts.start) / sizeof(u16);
	elts.start -= RADEON_INDEX_PRIM_OFFSET;

	if ( elts.start & 0x7 ) {
		DRM_ERROR( "misaligned buffer 0x%x\n", elts.start );
		return -EINVAL;
	}
	if ( elts.start < buf->used ) {
		DRM_ERROR( "no header 0x%x - 0x%x\n", elts.start, buf->used );
		return -EINVAL;
	}

	buf->used = elts.end;
	buf_priv->prim = elts.prim;
	buf_priv->discard = elts.discard;

	radeon_cp_dispatch_indices( dev, buf, elts.start, elts.end, count );

	return 0;
}

int radeon_cp_blit( struct inode *inode, struct file *filp,
		    unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_device_dma_t *dma = dev->dma;
	drm_radeon_blit_t blit;

	if ( !_DRM_LOCK_IS_HELD( dev->lock.hw_lock->lock ) ||
	     dev->lock.pid != current->pid ) {
		DRM_ERROR( "%s called without lock held\n", __FUNCTION__ );
		return -EINVAL;
	}

	if ( copy_from_user( &blit, (drm_radeon_blit_t *)arg,
			     sizeof(blit) ) )
		return -EFAULT;

	DRM_DEBUG( "%s: pid=%d index=%d\n",
		   __FUNCTION__, current->pid, blit.idx );

	if ( blit.idx < 0 || blit.idx > dma->buf_count ) {
		DRM_ERROR( "sending %d buffers (of %d max)\n",
			   blit.idx, dma->buf_count );
		return -EINVAL;
	}

	VB_AGE_CHECK_WITH_RET( dev_priv );

	return radeon_cp_dispatch_blit( dev, &blit );
}

int radeon_cp_stipple( struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_radeon_stipple_t stipple;
	u32 mask[32];

	if ( !_DRM_LOCK_IS_HELD( dev->lock.hw_lock->lock ) ||
	     dev->lock.pid != current->pid ) {
		DRM_ERROR( "%s called without lock held\n", __FUNCTION__ );
		return -EINVAL;
	}

	if ( copy_from_user( &stipple, (drm_radeon_stipple_t *)arg,
			     sizeof(stipple) ) )
		return -EFAULT;

	if ( copy_from_user( &mask, stipple.mask,
			     32 * sizeof(u32) ) )
		return -EFAULT;

	radeon_cp_dispatch_stipple( dev, mask );

	return 0;
}

int radeon_cp_indirect( struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf;
	drm_radeon_buf_priv_t *buf_priv;
	drm_radeon_indirect_t indirect;
	RING_LOCALS;

	if ( !_DRM_LOCK_IS_HELD( dev->lock.hw_lock->lock ) ||
	     dev->lock.pid != current->pid ) {
		DRM_ERROR( "%s called without lock held\n", __FUNCTION__ );
		return -EINVAL;
	}
	if ( !dev_priv || dev_priv->is_pci ) {
		DRM_ERROR( "%s called with a PCI card\n", __FUNCTION__ );
		return -EINVAL;
	}

	if ( copy_from_user( &indirect, (drm_radeon_indirect_t *)arg,
			     sizeof(indirect) ) )
		return -EFAULT;

	DRM_DEBUG( "indirect: idx=%d s=%d e=%d d=%d\n",
		   indirect.idx, indirect.start,
		   indirect.end, indirect.discard );

	if ( indirect.idx < 0 || indirect.idx >= dma->buf_count ) {
		DRM_ERROR( "buffer index %d (of %d max)\n",
			   indirect.idx, dma->buf_count - 1 );
		return -EINVAL;
	}

	buf = dma->buflist[indirect.idx];
	buf_priv = buf->dev_private;

	if ( buf->pid != current->pid ) {
		DRM_ERROR( "process %d using buffer owned by %d\n",
			   current->pid, buf->pid );
		return -EINVAL;
	}
	if ( buf->pending ) {
		DRM_ERROR( "sending pending buffer %d\n", indirect.idx );
		return -EINVAL;
	}

	if ( indirect.start < buf->used ) {
		DRM_ERROR( "reusing indirect: start=0x%x actual=0x%x\n",
			   indirect.start, buf->used );
		return -EINVAL;
	}

	VB_AGE_CHECK_WITH_RET( dev_priv );

	buf->used = indirect.end;
	buf_priv->discard = indirect.discard;

	/* Wait for the 3D stream to idle before the indirect buffer
	 * containing 2D acceleration commands is processed.
	 */
	BEGIN_RING( 2 );

	RADEON_WAIT_UNTIL_3D_IDLE();

	ADVANCE_RING();

	/* Dispatch the indirect buffer full of commands from the
	 * X server.  This is insecure and is thus only available to
	 * privileged clients.
	 */
	radeon_cp_dispatch_indirect( dev, buf, indirect.start, indirect.end );

	return 0;
}
