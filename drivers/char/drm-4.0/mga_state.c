/* mga_state.c -- State support for mga g200/g400 -*- linux-c -*-
 * Created: Thu Jan 27 02:53:43 2000 by jhartmann@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
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
 * Authors: Jeff Hartmann <jhartmann@valinux.com>
 * 	    Keith Whitwell <keithw@valinux.com>
 *
 */

#define __NO_VERSION__
#include "drmP.h"
#include "mga_drv.h"
#include "drm.h"

/* If you change the functions to set state, PLEASE
 * change these values
 */

#define MGAEMITCLIP_SIZE	10
#define MGAEMITCTX_SIZE		20
#define MGAG200EMITTEX_SIZE 	20
#define MGAG400EMITTEX0_SIZE	30
#define MGAG400EMITTEX1_SIZE	25
#define MGAG400EMITPIPE_SIZE	50
#define MGAG200EMITPIPE_SIZE	15

#define MAX_STATE_SIZE ((MGAEMITCLIP_SIZE * MGA_NR_SAREA_CLIPRECTS) + \
			MGAEMITCTX_SIZE + MGAG400EMITTEX0_SIZE + \
			MGAG400EMITTEX1_SIZE + MGAG400EMITPIPE_SIZE)

static void mgaEmitClipRect(drm_mga_private_t * dev_priv,
			    drm_clip_rect_t * box)
{
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int *regs = sarea_priv->ContextState;
	PRIMLOCALS;

	/* This takes 10 dwords */
	PRIMGETPTR(dev_priv);

	/* Force reset of dwgctl on G400 (eliminates clip disable bit) */
	if (dev_priv->chipset == MGA_CARD_TYPE_G400) {
#if 0
		PRIMOUTREG(MGAREG_DMAPAD, 0);
		PRIMOUTREG(MGAREG_DWGSYNC, 0);
		PRIMOUTREG(MGAREG_DWGSYNC, 0);
		PRIMOUTREG(MGAREG_DWGCTL, regs[MGA_CTXREG_DWGCTL]);
#else
		PRIMOUTREG(MGAREG_DWGCTL, regs[MGA_CTXREG_DWGCTL]);
		PRIMOUTREG(MGAREG_LEN + MGAREG_MGA_EXEC, 0x80000000);
		PRIMOUTREG(MGAREG_DWGCTL, regs[MGA_CTXREG_DWGCTL]);
		PRIMOUTREG(MGAREG_LEN + MGAREG_MGA_EXEC, 0x80000000);
#endif
	}
	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_CXBNDRY, ((box->x2) << 16) | (box->x1));
	PRIMOUTREG(MGAREG_YTOP, box->y1 * dev_priv->stride / dev_priv->cpp);
	PRIMOUTREG(MGAREG_YBOT, box->y2 * dev_priv->stride / dev_priv->cpp);

	PRIMADVANCE(dev_priv);
}

static void mgaEmitContext(drm_mga_private_t * dev_priv)
{
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int *regs = sarea_priv->ContextState;
	PRIMLOCALS;

	/* This takes a max of 20 dwords */
	PRIMGETPTR(dev_priv);

	PRIMOUTREG(MGAREG_DSTORG, regs[MGA_CTXREG_DSTORG]);
	PRIMOUTREG(MGAREG_MACCESS, regs[MGA_CTXREG_MACCESS]);
	PRIMOUTREG(MGAREG_PLNWT, regs[MGA_CTXREG_PLNWT]);
	PRIMOUTREG(MGAREG_DWGCTL, regs[MGA_CTXREG_DWGCTL]);

	PRIMOUTREG(MGAREG_ALPHACTRL, regs[MGA_CTXREG_ALPHACTRL]);
	PRIMOUTREG(MGAREG_FOGCOL, regs[MGA_CTXREG_FOGCOLOR]);
	PRIMOUTREG(MGAREG_WFLAG, regs[MGA_CTXREG_WFLAG]);
	PRIMOUTREG(MGAREG_ZORG, dev_priv->depthOffset);	/* invarient */

	if (dev_priv->chipset == MGA_CARD_TYPE_G400) {
		PRIMOUTREG(MGAREG_WFLAG1, regs[MGA_CTXREG_WFLAG]);
		PRIMOUTREG(MGAREG_TDUALSTAGE0, regs[MGA_CTXREG_TDUAL0]);
		PRIMOUTREG(MGAREG_TDUALSTAGE1, regs[MGA_CTXREG_TDUAL1]);
		PRIMOUTREG(MGAREG_FCOL, regs[MGA_CTXREG_FCOL]);

		PRIMOUTREG(MGAREG_STENCIL, regs[MGA_CTXREG_STENCIL]);
		PRIMOUTREG(MGAREG_STENCILCTL, regs[MGA_CTXREG_STENCILCTL]);
		PRIMOUTREG(MGAREG_DMAPAD, 0);
		PRIMOUTREG(MGAREG_DMAPAD, 0);
	} else {
		PRIMOUTREG(MGAREG_FCOL, regs[MGA_CTXREG_FCOL]);
		PRIMOUTREG(MGAREG_DMAPAD, 0);
		PRIMOUTREG(MGAREG_DMAPAD, 0);
		PRIMOUTREG(MGAREG_DMAPAD, 0);
	}

	PRIMADVANCE(dev_priv);
}

static void mgaG200EmitTex(drm_mga_private_t * dev_priv)
{
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int *regs = sarea_priv->TexState[0];
	PRIMLOCALS;

	PRIMGETPTR(dev_priv);

	/* This takes 20 dwords */

	PRIMOUTREG(MGAREG_TEXCTL2, regs[MGA_TEXREG_CTL2]);
	PRIMOUTREG(MGAREG_TEXCTL, regs[MGA_TEXREG_CTL]);
	PRIMOUTREG(MGAREG_TEXFILTER, regs[MGA_TEXREG_FILTER]);
	PRIMOUTREG(MGAREG_TEXBORDERCOL, regs[MGA_TEXREG_BORDERCOL]);

	PRIMOUTREG(MGAREG_TEXORG, regs[MGA_TEXREG_ORG]);
	PRIMOUTREG(MGAREG_TEXORG1, regs[MGA_TEXREG_ORG1]);
	PRIMOUTREG(MGAREG_TEXORG2, regs[MGA_TEXREG_ORG2]);
	PRIMOUTREG(MGAREG_TEXORG3, regs[MGA_TEXREG_ORG3]);

	PRIMOUTREG(MGAREG_TEXORG4, regs[MGA_TEXREG_ORG4]);
	PRIMOUTREG(MGAREG_TEXWIDTH, regs[MGA_TEXREG_WIDTH]);
	PRIMOUTREG(MGAREG_TEXHEIGHT, regs[MGA_TEXREG_HEIGHT]);
	PRIMOUTREG(MGAREG_WR24, regs[MGA_TEXREG_WIDTH]);

	PRIMOUTREG(MGAREG_WR34, regs[MGA_TEXREG_HEIGHT]);
	PRIMOUTREG(MGAREG_TEXTRANS, 0xffff);
	PRIMOUTREG(MGAREG_TEXTRANSHIGH, 0xffff);
	PRIMOUTREG(MGAREG_DMAPAD, 0);

	PRIMADVANCE(dev_priv);
}

#define TMC_dualtex_enable 		0x80

static void mgaG400EmitTex0(drm_mga_private_t * dev_priv)
{
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int *regs = sarea_priv->TexState[0];
	PRIMLOCALS;

	PRIMGETPTR(dev_priv);

	/* This takes 30 dwords */

	PRIMOUTREG(MGAREG_TEXCTL2, regs[MGA_TEXREG_CTL2] | 0x00008000);
	PRIMOUTREG(MGAREG_TEXCTL, regs[MGA_TEXREG_CTL]);
	PRIMOUTREG(MGAREG_TEXFILTER, regs[MGA_TEXREG_FILTER]);
	PRIMOUTREG(MGAREG_TEXBORDERCOL, regs[MGA_TEXREG_BORDERCOL]);

	PRIMOUTREG(MGAREG_TEXORG, regs[MGA_TEXREG_ORG]);
	PRIMOUTREG(MGAREG_TEXORG1, regs[MGA_TEXREG_ORG1]);
	PRIMOUTREG(MGAREG_TEXORG2, regs[MGA_TEXREG_ORG2]);
	PRIMOUTREG(MGAREG_TEXORG3, regs[MGA_TEXREG_ORG3]);

	PRIMOUTREG(MGAREG_TEXORG4, regs[MGA_TEXREG_ORG4]);
	PRIMOUTREG(MGAREG_TEXWIDTH, regs[MGA_TEXREG_WIDTH]);
	PRIMOUTREG(MGAREG_TEXHEIGHT, regs[MGA_TEXREG_HEIGHT]);
	PRIMOUTREG(MGAREG_WR49, 0);

	PRIMOUTREG(MGAREG_WR57, 0);
	PRIMOUTREG(MGAREG_WR53, 0);
	PRIMOUTREG(MGAREG_WR61, 0);
	PRIMOUTREG(MGAREG_WR52, 0x40);

	PRIMOUTREG(MGAREG_WR60, 0x40);
	PRIMOUTREG(MGAREG_WR54, regs[MGA_TEXREG_WIDTH] | 0x40);
	PRIMOUTREG(MGAREG_WR62, regs[MGA_TEXREG_HEIGHT] | 0x40);
	PRIMOUTREG(MGAREG_DMAPAD, 0);

	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_TEXTRANS, 0xffff);
	PRIMOUTREG(MGAREG_TEXTRANSHIGH, 0xffff);

	PRIMADVANCE(dev_priv);
}

#define TMC_map1_enable 		0x80000000

static void mgaG400EmitTex1(drm_mga_private_t * dev_priv)
{
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int *regs = sarea_priv->TexState[1];
	PRIMLOCALS;

	PRIMGETPTR(dev_priv);

	/* This takes 25 dwords */

	PRIMOUTREG(MGAREG_TEXCTL2,
		   regs[MGA_TEXREG_CTL2] | TMC_map1_enable | 0x00008000);
	PRIMOUTREG(MGAREG_TEXCTL, regs[MGA_TEXREG_CTL]);
	PRIMOUTREG(MGAREG_TEXFILTER, regs[MGA_TEXREG_FILTER]);
	PRIMOUTREG(MGAREG_TEXBORDERCOL, regs[MGA_TEXREG_BORDERCOL]);

	PRIMOUTREG(MGAREG_TEXORG, regs[MGA_TEXREG_ORG]);
	PRIMOUTREG(MGAREG_TEXORG1, regs[MGA_TEXREG_ORG1]);
	PRIMOUTREG(MGAREG_TEXORG2, regs[MGA_TEXREG_ORG2]);
	PRIMOUTREG(MGAREG_TEXORG3, regs[MGA_TEXREG_ORG3]);

	PRIMOUTREG(MGAREG_TEXORG4, regs[MGA_TEXREG_ORG4]);
	PRIMOUTREG(MGAREG_TEXWIDTH, regs[MGA_TEXREG_WIDTH]);
	PRIMOUTREG(MGAREG_TEXHEIGHT, regs[MGA_TEXREG_HEIGHT]);
	PRIMOUTREG(MGAREG_WR49, 0);

	PRIMOUTREG(MGAREG_WR57, 0);
	PRIMOUTREG(MGAREG_WR53, 0);
	PRIMOUTREG(MGAREG_WR61, 0);
	PRIMOUTREG(MGAREG_WR52, regs[MGA_TEXREG_WIDTH] | 0x40);

	PRIMOUTREG(MGAREG_WR60, regs[MGA_TEXREG_HEIGHT] | 0x40);
	PRIMOUTREG(MGAREG_TEXTRANS, 0xffff);
	PRIMOUTREG(MGAREG_TEXTRANSHIGH, 0xffff);
	PRIMOUTREG(MGAREG_TEXCTL2, regs[MGA_TEXREG_CTL2] | 0x00008000);

	PRIMADVANCE(dev_priv);
}

#define MAGIC_FPARAM_HEX_VALUE 0x46480000
/* This is the hex value of 12800.0f which is a magic value we must
 * set in wr56.
 */

static void mgaG400EmitPipe(drm_mga_private_t * dev_priv)
{
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int pipe = sarea_priv->WarpPipe;
	PRIMLOCALS;

	PRIMGETPTR(dev_priv);

	/* This takes 50 dwords */

	/* Establish vertex size.  
	 */
	PRIMOUTREG(MGAREG_WIADDR2, WIA_wmode_suspend);
	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_DMAPAD, 0);

	if (pipe & MGA_T2) {
		PRIMOUTREG(MGAREG_WVRTXSZ, 0x00001e09);
		PRIMOUTREG(MGAREG_DMAPAD, 0);
		PRIMOUTREG(MGAREG_DMAPAD, 0);
		PRIMOUTREG(MGAREG_DMAPAD, 0);

		PRIMOUTREG(MGAREG_WACCEPTSEQ, 0);
		PRIMOUTREG(MGAREG_WACCEPTSEQ, 0);
		PRIMOUTREG(MGAREG_WACCEPTSEQ, 0);
		PRIMOUTREG(MGAREG_WACCEPTSEQ, 0x1e000000);
	} else {
		if (dev_priv->WarpPipe & MGA_T2) {
			/* Flush the WARP pipe */
			PRIMOUTREG(MGAREG_YDST, 0);
			PRIMOUTREG(MGAREG_FXLEFT, 0);
			PRIMOUTREG(MGAREG_FXRIGHT, 1);
			PRIMOUTREG(MGAREG_DWGCTL, MGA_FLUSH_CMD);

			PRIMOUTREG(MGAREG_LEN + MGAREG_MGA_EXEC, 1);
			PRIMOUTREG(MGAREG_DWGSYNC, 0x7000);
			PRIMOUTREG(MGAREG_TEXCTL2, 0x00008000);
			PRIMOUTREG(MGAREG_LEN + MGAREG_MGA_EXEC, 0);

			PRIMOUTREG(MGAREG_TEXCTL2, 0x80 | 0x00008000);
			PRIMOUTREG(MGAREG_LEN + MGAREG_MGA_EXEC, 0);
			PRIMOUTREG(MGAREG_TEXCTL2, 0x00008000);
			PRIMOUTREG(MGAREG_DMAPAD, 0);
		}

		PRIMOUTREG(MGAREG_WVRTXSZ, 0x00001807);
		PRIMOUTREG(MGAREG_DMAPAD, 0);
		PRIMOUTREG(MGAREG_DMAPAD, 0);
		PRIMOUTREG(MGAREG_DMAPAD, 0);

		PRIMOUTREG(MGAREG_WACCEPTSEQ, 0);
		PRIMOUTREG(MGAREG_WACCEPTSEQ, 0);
		PRIMOUTREG(MGAREG_WACCEPTSEQ, 0);
		PRIMOUTREG(MGAREG_WACCEPTSEQ, 0x18000000);
	}

	PRIMOUTREG(MGAREG_WFLAG, 0);
	PRIMOUTREG(MGAREG_WFLAG1, 0);
	PRIMOUTREG(MGAREG_WR56, MAGIC_FPARAM_HEX_VALUE);
	PRIMOUTREG(MGAREG_DMAPAD, 0);

	PRIMOUTREG(MGAREG_WR49, 0);	/* Tex stage 0 */
	PRIMOUTREG(MGAREG_WR57, 0);	/* Tex stage 0 */
	PRIMOUTREG(MGAREG_WR53, 0);	/* Tex stage 1 */
	PRIMOUTREG(MGAREG_WR61, 0);	/* Tex stage 1 */

	PRIMOUTREG(MGAREG_WR54, 0x40);	/* Tex stage 0 : w */
	PRIMOUTREG(MGAREG_WR62, 0x40);	/* Tex stage 0 : h */
	PRIMOUTREG(MGAREG_WR52, 0x40);	/* Tex stage 1 : w */
	PRIMOUTREG(MGAREG_WR60, 0x40);	/* Tex stage 1 : h */

	/* Dma pading required due to hw bug */
	PRIMOUTREG(MGAREG_DMAPAD, 0xffffffff);
	PRIMOUTREG(MGAREG_DMAPAD, 0xffffffff);
	PRIMOUTREG(MGAREG_DMAPAD, 0xffffffff);
	PRIMOUTREG(MGAREG_WIADDR2,
		   (u32) (dev_priv->WarpIndex[pipe].
			  phys_addr | WIA_wmode_start | WIA_wagp_agp));
	PRIMADVANCE(dev_priv);
}

static void mgaG200EmitPipe(drm_mga_private_t * dev_priv)
{
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int pipe = sarea_priv->WarpPipe;
	PRIMLOCALS;

	PRIMGETPTR(dev_priv);

	/* This takes 15 dwords */

	PRIMOUTREG(MGAREG_WIADDR, WIA_wmode_suspend);
	PRIMOUTREG(MGAREG_WVRTXSZ, 7);
	PRIMOUTREG(MGAREG_WFLAG, 0);
	PRIMOUTREG(MGAREG_WR24, 0);	/* tex w/h */

	PRIMOUTREG(MGAREG_WR25, 0x100);
	PRIMOUTREG(MGAREG_WR34, 0);	/* tex w/h */
	PRIMOUTREG(MGAREG_WR42, 0xFFFF);
	PRIMOUTREG(MGAREG_WR60, 0xFFFF);

	/* Dma pading required due to hw bug */
	PRIMOUTREG(MGAREG_DMAPAD, 0xffffffff);
	PRIMOUTREG(MGAREG_DMAPAD, 0xffffffff);
	PRIMOUTREG(MGAREG_DMAPAD, 0xffffffff);
	PRIMOUTREG(MGAREG_WIADDR,
		   (u32) (dev_priv->WarpIndex[pipe].
			  phys_addr | WIA_wmode_start | WIA_wagp_agp));

	PRIMADVANCE( dev_priv );
}

static void mgaEmitState(drm_mga_private_t * dev_priv)
{
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int dirty = sarea_priv->dirty;

	if (dev_priv->chipset == MGA_CARD_TYPE_G400) {
		int multitex = sarea_priv->WarpPipe & MGA_T2;

		if (sarea_priv->WarpPipe != dev_priv->WarpPipe) {
			mgaG400EmitPipe(dev_priv);
			dev_priv->WarpPipe = sarea_priv->WarpPipe;
		}

		if (dirty & MGA_UPLOAD_CTX) {
			mgaEmitContext(dev_priv);
			sarea_priv->dirty &= ~MGA_UPLOAD_CTX;
		}

		if (dirty & MGA_UPLOAD_TEX0) {
			mgaG400EmitTex0(dev_priv);
			sarea_priv->dirty &= ~MGA_UPLOAD_TEX0;
		}

		if ((dirty & MGA_UPLOAD_TEX1) && multitex) {
			mgaG400EmitTex1(dev_priv);
			sarea_priv->dirty &= ~MGA_UPLOAD_TEX1;
		}
	} else {
		if (sarea_priv->WarpPipe != dev_priv->WarpPipe) {
			mgaG200EmitPipe(dev_priv);
			dev_priv->WarpPipe = sarea_priv->WarpPipe;
		}

		if (dirty & MGA_UPLOAD_CTX) {
			mgaEmitContext(dev_priv);
			sarea_priv->dirty &= ~MGA_UPLOAD_CTX;
		}

		if (dirty & MGA_UPLOAD_TEX0) {
			mgaG200EmitTex(dev_priv);
			sarea_priv->dirty &= ~MGA_UPLOAD_TEX0;
		}
	}
}

/* Disallow all write destinations except the front and backbuffer.
 */
static int mgaVerifyContext(drm_mga_private_t * dev_priv)
{
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int *regs = sarea_priv->ContextState;

	if (regs[MGA_CTXREG_DSTORG] != dev_priv->frontOffset &&
	    regs[MGA_CTXREG_DSTORG] != dev_priv->backOffset) {
		DRM_DEBUG("BAD DSTORG: %x (front %x, back %x)\n\n",
			  regs[MGA_CTXREG_DSTORG], dev_priv->frontOffset,
			  dev_priv->backOffset);
		regs[MGA_CTXREG_DSTORG] = 0;
		return -1;
	}

	return 0;
}

/* Disallow texture reads from PCI space.
 */
static int mgaVerifyTex(drm_mga_private_t * dev_priv, int unit)
{
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;

	if ((sarea_priv->TexState[unit][MGA_TEXREG_ORG] & 0x3) == 0x1) {
		DRM_DEBUG("BAD TEXREG_ORG: %x, unit %d\n",
			  sarea_priv->TexState[unit][MGA_TEXREG_ORG],
			  unit);
		sarea_priv->TexState[unit][MGA_TEXREG_ORG] = 0;
		return -1;
	}

	return 0;
}

static int mgaVerifyState(drm_mga_private_t * dev_priv)
{
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int dirty = sarea_priv->dirty;
	int rv = 0;

	if (sarea_priv->nbox > MGA_NR_SAREA_CLIPRECTS)
		sarea_priv->nbox = MGA_NR_SAREA_CLIPRECTS;

	if (dirty & MGA_UPLOAD_CTX)
		rv |= mgaVerifyContext(dev_priv);

	if (dirty & MGA_UPLOAD_TEX0)
		rv |= mgaVerifyTex(dev_priv, 0);

	if (dev_priv->chipset == MGA_CARD_TYPE_G400) {
		if (dirty & MGA_UPLOAD_TEX1)
			rv |= mgaVerifyTex(dev_priv, 1);

		if (dirty & MGA_UPLOAD_PIPE)
			rv |= (sarea_priv->WarpPipe > MGA_MAX_G400_PIPES);
	} else {
		if (dirty & MGA_UPLOAD_PIPE)
			rv |= (sarea_priv->WarpPipe > MGA_MAX_G200_PIPES);
	}

	return rv == 0;
}

static int mgaVerifyIload(drm_mga_private_t * dev_priv,
			  unsigned long bus_address,
			  unsigned int dstOrg, int length)
{
	if (dstOrg < dev_priv->textureOffset ||
	    dstOrg + length >
	    (dev_priv->textureOffset + dev_priv->textureSize)) {
		return -EINVAL;
	}
	if (length % 64) {
		return -EINVAL;
	}
	return 0;
}

/* This copies a 64 byte aligned agp region to the frambuffer
 * with a standard blit, the ioctl needs to do checking */

static void mga_dma_dispatch_tex_blit(drm_device_t * dev,
				      unsigned long bus_address,
				      int length, unsigned int destOrg)
{
	drm_mga_private_t *dev_priv = dev->dev_private;
	int use_agp = PDEA_pagpxfer_enable | 0x00000001;
	u16 y2;
	PRIMLOCALS;

	y2 = length / 64;

	PRIM_OVERFLOW(dev, dev_priv, 30);

	PRIMOUTREG(MGAREG_DSTORG, destOrg);
	PRIMOUTREG(MGAREG_MACCESS, 0x00000000);
	PRIMOUTREG(MGAREG_SRCORG, (u32) bus_address | use_agp);
	PRIMOUTREG(MGAREG_AR5, 64);

	PRIMOUTREG(MGAREG_PITCH, 64);
	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_DWGCTL, MGA_COPY_CMD);

	PRIMOUTREG(MGAREG_AR0, 63);
	PRIMOUTREG(MGAREG_AR3, 0);
	PRIMOUTREG(MGAREG_FXBNDRY, (63 << 16));
	PRIMOUTREG(MGAREG_YDSTLEN + MGAREG_MGA_EXEC, y2);

	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_SRCORG, 0);
	PRIMOUTREG(MGAREG_PITCH, dev_priv->stride / dev_priv->cpp);
	PRIMOUTREG(MGAREG_DWGSYNC, 0x7000);
	PRIMADVANCE(dev_priv);
}

static void mga_dma_dispatch_vertex(drm_device_t * dev, drm_buf_t * buf)
{
	drm_mga_private_t *dev_priv = dev->dev_private;
	drm_mga_buf_priv_t *buf_priv = buf->dev_private;
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned long address = (unsigned long) buf->bus_address;
	int length = buf->used;
	int use_agp = PDEA_pagpxfer_enable;
	int i = 0;
	PRIMLOCALS;

	if (buf->used) {
		/* WARNING: if you change any of the state functions verify
		 * these numbers (Overestimating this doesn't hurt).
		 */
		buf_priv->dispatched = 1;
		PRIM_OVERFLOW(dev, dev_priv,
			      (MAX_STATE_SIZE + (5 * MGA_NR_SAREA_CLIPRECTS)));
		mgaEmitState(dev_priv);

#if 0
		length = dev_priv->vertexsize * 3 * 4;
#endif

		do {
			if (i < sarea_priv->nbox) {
				mgaEmitClipRect(dev_priv,
						&sarea_priv->boxes[i]);
			}

			PRIMGETPTR(dev_priv);
			PRIMOUTREG(MGAREG_DMAPAD, 0);
			PRIMOUTREG(MGAREG_DMAPAD, 0);
			PRIMOUTREG(MGAREG_SECADDRESS,
				   ((u32) address) | TT_VERTEX);
			PRIMOUTREG(MGAREG_SECEND,
				   (((u32) (address + length)) | use_agp));
			PRIMADVANCE(dev_priv);
		} while (++i < sarea_priv->nbox);
	}
	if (buf_priv->discard) {
		if (buf_priv->dispatched == 1)
			AGEBUF(dev_priv, buf_priv);
		buf_priv->dispatched = 0;
		mga_freelist_put(dev, buf);
	}


}


static void mga_dma_dispatch_indices(drm_device_t * dev,
				     drm_buf_t * buf,
				     unsigned int start, unsigned int end)
{
	drm_mga_private_t *dev_priv = dev->dev_private;
	drm_mga_buf_priv_t *buf_priv = buf->dev_private;
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int address = (unsigned int) buf->bus_address;
	int use_agp = PDEA_pagpxfer_enable;
	int i = 0;
	PRIMLOCALS;

	if (start != end) {
		/* WARNING: if you change any of the state functions verify
		 * these numbers (Overestimating this doesn't hurt).
		 */
		buf_priv->dispatched = 1;
		PRIM_OVERFLOW(dev, dev_priv,
			      (MAX_STATE_SIZE + (5 * MGA_NR_SAREA_CLIPRECTS)));
		mgaEmitState(dev_priv);

		do {
			if (i < sarea_priv->nbox) {
				mgaEmitClipRect(dev_priv,
						&sarea_priv->boxes[i]);
			}

			PRIMGETPTR(dev_priv);
			PRIMOUTREG(MGAREG_DMAPAD, 0);
			PRIMOUTREG(MGAREG_DMAPAD, 0);
			PRIMOUTREG(MGAREG_SETUPADDRESS,
				   ((address + start) |
				    SETADD_mode_vertlist));
			PRIMOUTREG(MGAREG_SETUPEND,
				   ((address + end) | use_agp));
/*  				   ((address + start + 12) | use_agp)); */
			PRIMADVANCE(dev_priv);
		} while (++i < sarea_priv->nbox);
	}
	if (buf_priv->discard) {
		if (buf_priv->dispatched == 1)
			AGEBUF(dev_priv, buf_priv);
		buf_priv->dispatched = 0;
		mga_freelist_put(dev, buf);
	}
}


static void mga_dma_dispatch_clear(drm_device_t * dev, int flags,
				   unsigned int clear_color,
				   unsigned int clear_zval,
				   unsigned int clear_colormask,
				   unsigned int clear_depthmask)
{
	drm_mga_private_t *dev_priv = dev->dev_private;
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int *regs = sarea_priv->ContextState;
	int nbox = sarea_priv->nbox;
	drm_clip_rect_t *pbox = sarea_priv->boxes;
	unsigned int cmd;
	int i;
	PRIMLOCALS;

	if (dev_priv->sgram)
		cmd = MGA_CLEAR_CMD | DC_atype_blk;
	else
		cmd = MGA_CLEAR_CMD | DC_atype_rstr;

	PRIM_OVERFLOW(dev, dev_priv, 35 * MGA_NR_SAREA_CLIPRECTS);

	for (i = 0; i < nbox; i++) {
		unsigned int height = pbox[i].y2 - pbox[i].y1;

		if (flags & MGA_FRONT) {
			PRIMOUTREG(MGAREG_DMAPAD, 0);
			PRIMOUTREG(MGAREG_PLNWT, clear_colormask);
			PRIMOUTREG(MGAREG_YDSTLEN,
				   (pbox[i].y1 << 16) | height);
			PRIMOUTREG(MGAREG_FXBNDRY,
				   (pbox[i].x2 << 16) | pbox[i].x1);

			PRIMOUTREG(MGAREG_DMAPAD, 0);
			PRIMOUTREG(MGAREG_FCOL, clear_color);
			PRIMOUTREG(MGAREG_DSTORG, dev_priv->frontOffset);
			PRIMOUTREG(MGAREG_DWGCTL + MGAREG_MGA_EXEC, cmd);
		}

		if (flags & MGA_BACK) {
			PRIMOUTREG(MGAREG_DMAPAD, 0);
			PRIMOUTREG(MGAREG_PLNWT, clear_colormask);
			PRIMOUTREG(MGAREG_YDSTLEN,
				   (pbox[i].y1 << 16) | height);
			PRIMOUTREG(MGAREG_FXBNDRY,
				   (pbox[i].x2 << 16) | pbox[i].x1);

			PRIMOUTREG(MGAREG_DMAPAD, 0);
			PRIMOUTREG(MGAREG_FCOL, clear_color);
			PRIMOUTREG(MGAREG_DSTORG, dev_priv->backOffset);
			PRIMOUTREG(MGAREG_DWGCTL + MGAREG_MGA_EXEC, cmd);
		}

		if (flags & MGA_DEPTH) {
			PRIMOUTREG(MGAREG_DMAPAD, 0);
			PRIMOUTREG(MGAREG_PLNWT, clear_depthmask);
			PRIMOUTREG(MGAREG_YDSTLEN,
				   (pbox[i].y1 << 16) | height);
			PRIMOUTREG(MGAREG_FXBNDRY,
				   (pbox[i].x2 << 16) | pbox[i].x1);

			PRIMOUTREG(MGAREG_DMAPAD, 0);
			PRIMOUTREG(MGAREG_FCOL, clear_zval);
			PRIMOUTREG(MGAREG_DSTORG, dev_priv->depthOffset);
			PRIMOUTREG(MGAREG_DWGCTL + MGAREG_MGA_EXEC, cmd);
		}
	}

	/* Force reset of DWGCTL */
	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_DWGCTL, regs[MGA_CTXREG_DWGCTL]);
	PRIMADVANCE(dev_priv);
}

static void mga_dma_dispatch_swap(drm_device_t * dev)
{
	drm_mga_private_t *dev_priv = dev->dev_private;
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int *regs = sarea_priv->ContextState;
	int nbox = sarea_priv->nbox;
	drm_clip_rect_t *pbox = sarea_priv->boxes;
	int i;
	int pixel_stride = dev_priv->stride / dev_priv->cpp;

	PRIMLOCALS;

	PRIM_OVERFLOW(dev, dev_priv, (MGA_NR_SAREA_CLIPRECTS * 5) + 20);

	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_DWGSYNC, 0x7100);
	PRIMOUTREG(MGAREG_DWGSYNC, 0x7000);

	PRIMOUTREG(MGAREG_DSTORG, dev_priv->frontOffset);
	PRIMOUTREG(MGAREG_MACCESS, dev_priv->mAccess);
	PRIMOUTREG(MGAREG_SRCORG, dev_priv->backOffset);
	PRIMOUTREG(MGAREG_AR5, pixel_stride);

	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_DWGCTL, MGA_COPY_CMD);

	for (i = 0; i < nbox; i++) {
		unsigned int h = pbox[i].y2 - pbox[i].y1;
		unsigned int start = pbox[i].y1 * pixel_stride;

		PRIMOUTREG(MGAREG_AR0, start + pbox[i].x2 - 1);
		PRIMOUTREG(MGAREG_AR3, start + pbox[i].x1);
		PRIMOUTREG(MGAREG_FXBNDRY,
			   pbox[i].x1 | ((pbox[i].x2 - 1) << 16));
		PRIMOUTREG(MGAREG_YDSTLEN + MGAREG_MGA_EXEC,
			   (pbox[i].y1 << 16) | h);
	}

	/* Force reset of DWGCTL */
	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_SRCORG, 0);
	PRIMOUTREG(MGAREG_DWGCTL, regs[MGA_CTXREG_DWGCTL]);

	PRIMADVANCE(dev_priv);
}

int mga_clear_bufs(struct inode *inode, struct file *filp,
		   unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_mga_private_t *dev_priv =
	    (drm_mga_private_t *) dev->dev_private;
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_mga_clear_t clear;

	if (copy_from_user(&clear, (drm_mga_clear_t *) arg, sizeof(clear)))
		return -EFAULT;

	if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("mga_clear_bufs called without lock held\n");
		return -EINVAL;
	}

	if (sarea_priv->nbox > MGA_NR_SAREA_CLIPRECTS)
		sarea_priv->nbox = MGA_NR_SAREA_CLIPRECTS;

	/* Make sure we restore the 3D state next time.
	 */
	dev_priv->sarea_priv->dirty |= MGA_UPLOAD_CTX;
	mga_dma_dispatch_clear(dev, clear.flags,
			       clear.clear_color,
			       clear.clear_depth,
			       clear.clear_color_mask,
			       clear.clear_depth_mask);
	PRIMUPDATE(dev_priv);
	mga_flush_write_combine();
	mga_dma_schedule(dev, 1);
	return 0;
}

int mga_swap_bufs(struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_mga_private_t *dev_priv =
	    (drm_mga_private_t *) dev->dev_private;
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;

	if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("mga_swap_bufs called without lock held\n");
		return -EINVAL;
	}

	if (sarea_priv->nbox > MGA_NR_SAREA_CLIPRECTS)
		sarea_priv->nbox = MGA_NR_SAREA_CLIPRECTS;

	/* Make sure we restore the 3D state next time.
	 */
	dev_priv->sarea_priv->dirty |= MGA_UPLOAD_CTX;
	mga_dma_dispatch_swap(dev);
	PRIMUPDATE(dev_priv);
	set_bit(MGA_BUF_SWAP_PENDING,
		&dev_priv->current_prim->buffer_status);
	mga_flush_write_combine();
	mga_dma_schedule(dev, 1);
	return 0;
}

int mga_iload(struct inode *inode, struct file *filp,
	      unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_device_dma_t *dma = dev->dma;
	drm_mga_private_t *dev_priv =
	    (drm_mga_private_t *) dev->dev_private;
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_buf_t *buf;
	drm_mga_buf_priv_t *buf_priv;
	drm_mga_iload_t iload;
	unsigned long bus_address;

	if (copy_from_user(&iload, (drm_mga_iload_t *) arg, sizeof(iload)))
		return -EFAULT;

	if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("mga_iload called without lock held\n");
		return -EINVAL;
	}

	if(iload.idx < 0 || iload.idx > dma->buf_count) return -EINVAL;
	buf = dma->buflist[iload.idx];
	buf_priv = buf->dev_private;
	bus_address = buf->bus_address;

	if (mgaVerifyIload(dev_priv,
			   bus_address, iload.destOrg, iload.length)) {
		mga_freelist_put(dev, buf);
		return -EINVAL;
	}

	sarea_priv->dirty |= MGA_UPLOAD_CTX;

	mga_dma_dispatch_tex_blit(dev, bus_address, iload.length,
				  iload.destOrg);
	AGEBUF(dev_priv, buf_priv);
	buf_priv->discard = 1;
	mga_freelist_put(dev, buf);
	mga_flush_write_combine();
	mga_dma_schedule(dev, 1);
	return 0;
}

int mga_vertex(struct inode *inode, struct file *filp,
	       unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_mga_private_t *dev_priv =
	    (drm_mga_private_t *) dev->dev_private;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf;
	drm_mga_buf_priv_t *buf_priv;
	drm_mga_vertex_t vertex;

	if (copy_from_user(&vertex, (drm_mga_vertex_t *) arg, sizeof(vertex)))
		return -EFAULT;

	if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("mga_vertex called without lock held\n");
		return -EINVAL;
	}

	if(vertex.idx < 0 || vertex.idx > dma->buf_count) return -EINVAL;

	buf = dma->buflist[vertex.idx];
	buf_priv = buf->dev_private;

	buf->used = vertex.used;
	buf_priv->discard = vertex.discard;

	if (!mgaVerifyState(dev_priv)) {
		if (vertex.discard) {
			if (buf_priv->dispatched == 1)
				AGEBUF(dev_priv, buf_priv);
			buf_priv->dispatched = 0;
			mga_freelist_put(dev, buf);
		}
		return -EINVAL;
	}

	mga_dma_dispatch_vertex(dev, buf);

	PRIMUPDATE(dev_priv);
	mga_flush_write_combine();
	mga_dma_schedule(dev, 1);
	return 0;
}


int mga_indices(struct inode *inode, struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_mga_private_t *dev_priv =
	    (drm_mga_private_t *) dev->dev_private;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf;
	drm_mga_buf_priv_t *buf_priv;
	drm_mga_indices_t indices;

	if (copy_from_user(&indices,
			   (drm_mga_indices_t *)arg, sizeof(indices)))
		return -EFAULT;

	if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("mga_indices called without lock held\n");
		return -EINVAL;
	}

	if(indices.idx < 0 || indices.idx > dma->buf_count) return -EINVAL;
	buf = dma->buflist[indices.idx];
	buf_priv = buf->dev_private;

	buf_priv->discard = indices.discard;

	if (!mgaVerifyState(dev_priv)) {
		if (indices.discard) {
			if (buf_priv->dispatched == 1)
				AGEBUF(dev_priv, buf_priv);
			buf_priv->dispatched = 0;
			mga_freelist_put(dev, buf);
		}
		return -EINVAL;
	}

	mga_dma_dispatch_indices(dev, buf, indices.start, indices.end);

	PRIMUPDATE(dev_priv);
	mga_flush_write_combine();
	mga_dma_schedule(dev, 1);
	return 0;
}



static int mga_dma_get_buffers(drm_device_t * dev, drm_dma_t * d)
{
	int i;
	drm_buf_t *buf;

	for (i = d->granted_count; i < d->request_count; i++) {
		buf = mga_freelist_get(dev);
		if (!buf)
			break;
		buf->pid = current->pid;
		if (copy_to_user(&d->request_indices[i],
				 &buf->idx, sizeof(buf->idx)))
			return -EFAULT;
		if (copy_to_user(&d->request_sizes[i],
				 &buf->total, sizeof(buf->total)))
			return -EFAULT;
		++d->granted_count;
	}
	return 0;
}

int mga_dma(struct inode *inode, struct file *filp, unsigned int cmd,
	    unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_device_dma_t *dma = dev->dma;
	int retcode = 0;
	drm_dma_t d;

	if (copy_from_user(&d, (drm_dma_t *) arg, sizeof(d)))
		return -EFAULT;

	if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("mga_dma called without lock held\n");
		return -EINVAL;
	}

	/* Please don't send us buffers.
	 */
	if (d.send_count != 0) {
		DRM_ERROR
		    ("Process %d trying to send %d buffers via drmDMA\n",
		     current->pid, d.send_count);
		return -EINVAL;
	}

	/* We'll send you buffers.
	 */
	if (d.request_count < 0 || d.request_count > dma->buf_count) {
		DRM_ERROR
		    ("Process %d trying to get %d buffers (of %d max)\n",
		     current->pid, d.request_count, dma->buf_count);
		return -EINVAL;
	}

	d.granted_count = 0;

	if (d.request_count) {
		retcode = mga_dma_get_buffers(dev, &d);
	}

	if (copy_to_user((drm_dma_t *) arg, &d, sizeof(d)))
		return -EFAULT;
	return retcode;
}
