/*-
 * Copyright (c) 2014 Ed Maste
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _DEV_PIXELSTREAM_H_
#define	_DEV_PIXLESTREAM_H_

struct pixelstream_softc {
	/*
	 * Bus-related fields.
	 */
	device_t	 ps_dev;
	int		 ps_unit;

	/*
	 * Control registers - mappable from userspace.
	 */
	struct cdev	*ps_reg_cdev;
	struct resource	*ps_reg_res;
	int		 ps_reg_rid;

	/*
	 * BUS_DMA(9) interface to framebuffer RAM.
	 */
	bus_dma_tag_t	 ps_fb_dmat;
	bus_dmamap_t	 ps_fb_dmamap;

	/*
	 * Framebuffer hookup for vt(4).
	 */
	struct fb_info	 ps_fb_info;
};

/*
 * Pixelstream control register offsets.
 */
#define	PIXELSTREAM_OFF_X_RESOLUTION		0x00
#define	PIXELSTREAM_OFF_HSYNC_PULSE_WIDTH	0x04
#define	PIXELSTREAM_OFF_HSYNC_BACK_PORCH	0x08
#define	PIXELSTREAM_OFF_HSYNC_FRONT_PORCH	0x0c
#define	PIXELSTREAM_OFF_Y_RESOLUTION		0x10
#define	PIXELSTREAM_OFF_VSYNC_PUSLE_WIDTH	0x14
#define	PIXELSTREAM_OFF_VSYNC_BACK_PORCH	0x18
#define	PIXELSTREAM_OFF_VSYNC_FRONT_PORCH	0x1c
#define	PIXELSTREAM_OFF_BASE_ADDR_LOWER		0x20
#define	PIXELSTREAM_OFF_BASE_ADDR_UPPER		0x24

/*
 * Driver setup routines from the bus attachment/teardown.
 */
int	pixelstream_attach(struct pixelstream_softc *sc);
void	pixelstream_detach(struct pixelstream_softc *sc);

extern devclass_t	pixelstream_devclass;

/*
 * Sub-driver setup routines.
 */
int	pixelstream_fbd_attach(struct pixelstream_softc *sc);
void	pixelstream_fbd_detach(struct pixelstream_softc *sc);
int	pixelstream_reg_attach(struct pixelstream_softc *sc);
void	pixelstream_reg_detach(struct pixelstream_softc *sc);

#endif /* _DEV_PIXELSTREAM_H_ */
