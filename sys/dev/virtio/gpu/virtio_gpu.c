/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013, Bryan Venteicher <bryanv@FreeBSD.org>
 * All rights reserved.
 * Copyright (c) 2023, Arm Ltd
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 */

/* Driver for VirtIO GPU device. */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/fbio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sglist.h>

#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtqueue.h>
#include <dev/virtio/gpu/virtio_gpu.h>

#include <dev/vt/vt.h>
#include <dev/vt/hw/fb/vt_fb.h>
#include <dev/vt/colors/vt_termcolors.h>

#include "fb_if.h"

#define VTGPU_FEATURES	0

/* The guest can allocate resource IDs, we only need one */
#define	VTGPU_RESOURCE_ID	1

struct vtgpu_softc {
	/* Must be first so we can cast from info -> softc */
	struct fb_info 		 vtgpu_fb_info;
	struct virtio_gpu_config vtgpu_gpucfg;

	device_t		 vtgpu_dev;
	uint64_t		 vtgpu_features;

	struct virtqueue	*vtgpu_ctrl_vq;

	uint64_t		 vtgpu_next_fence;

	bool			 vtgpu_have_fb_info;
};

static int	vtgpu_modevent(module_t, int, void *);

static int	vtgpu_probe(device_t);
static int	vtgpu_attach(device_t);
static int	vtgpu_detach(device_t);

static int	vtgpu_negotiate_features(struct vtgpu_softc *);
static int	vtgpu_setup_features(struct vtgpu_softc *);
static void	vtgpu_read_config(struct vtgpu_softc *,
		    struct virtio_gpu_config *);
static int	vtgpu_alloc_virtqueue(struct vtgpu_softc *);
static int	vtgpu_get_display_info(struct vtgpu_softc *);
static int	vtgpu_create_2d(struct vtgpu_softc *);
static int	vtgpu_attach_backing(struct vtgpu_softc *);
static int	vtgpu_set_scanout(struct vtgpu_softc *, uint32_t, uint32_t,
		    uint32_t, uint32_t);
static int	vtgpu_transfer_to_host_2d(struct vtgpu_softc *, uint32_t,
		    uint32_t, uint32_t, uint32_t);
static int	vtgpu_resource_flush(struct vtgpu_softc *, uint32_t, uint32_t,
		    uint32_t, uint32_t);

static vd_blank_t		vtgpu_fb_blank;
static vd_bitblt_text_t		vtgpu_fb_bitblt_text;
static vd_bitblt_bmp_t		vtgpu_fb_bitblt_bitmap;
static vd_drawrect_t		vtgpu_fb_drawrect;
static vd_setpixel_t		vtgpu_fb_setpixel;
static vd_bitblt_argb_t		vtgpu_fb_bitblt_argb;

static struct vt_driver vtgpu_fb_driver = {
	.vd_name = "virtio_gpu",
	.vd_init = vt_fb_init,
	.vd_fini = vt_fb_fini,
	.vd_blank = vtgpu_fb_blank,
	.vd_bitblt_text = vtgpu_fb_bitblt_text,
	.vd_invalidate_text = vt_fb_invalidate_text,
	.vd_bitblt_bmp = vtgpu_fb_bitblt_bitmap,
	.vd_bitblt_argb = vtgpu_fb_bitblt_argb,
	.vd_drawrect = vtgpu_fb_drawrect,
	.vd_setpixel = vtgpu_fb_setpixel,
	.vd_postswitch = vt_fb_postswitch,
	.vd_priority = VD_PRIORITY_GENERIC+10,
	.vd_fb_ioctl = vt_fb_ioctl,
	.vd_fb_mmap = NULL,	/* No mmap as we need to signal the host */
	.vd_suspend = vt_fb_suspend,
	.vd_resume = vt_fb_resume,
};

VT_DRIVER_DECLARE(vt_vtgpu, vtgpu_fb_driver);

static void
vtgpu_fb_blank(struct vt_device *vd, term_color_t color)
{
	struct vtgpu_softc *sc;
	struct fb_info *info;

	info = vd->vd_softc;
	sc = (struct vtgpu_softc *)info;

	vt_fb_blank(vd, color);

	vtgpu_transfer_to_host_2d(sc, 0, 0, sc->vtgpu_fb_info.fb_width,
	    sc->vtgpu_fb_info.fb_height);
	vtgpu_resource_flush(sc, 0, 0, sc->vtgpu_fb_info.fb_width,
	    sc->vtgpu_fb_info.fb_height);
}

static void
vtgpu_fb_bitblt_text(struct vt_device *vd, const struct vt_window *vw,
    const term_rect_t *area)
{
	struct vtgpu_softc *sc;
	struct fb_info *info;
	int x, y, width, height;

	info = vd->vd_softc;
	sc = (struct vtgpu_softc *)info;

	vt_fb_bitblt_text(vd, vw, area);

	x = area->tr_begin.tp_col * vw->vw_font->vf_width + vw->vw_draw_area.tr_begin.tp_col;
	y = area->tr_begin.tp_row * vw->vw_font->vf_height + vw->vw_draw_area.tr_begin.tp_row;
	width = area->tr_end.tp_col * vw->vw_font->vf_width + vw->vw_draw_area.tr_begin.tp_col - x;
	height = area->tr_end.tp_row * vw->vw_font->vf_height + vw->vw_draw_area.tr_begin.tp_row - y;

	vtgpu_transfer_to_host_2d(sc, x, y, width, height);
	vtgpu_resource_flush(sc, x, y, width, height);
}

static void
vtgpu_fb_bitblt_bitmap(struct vt_device *vd, const struct vt_window *vw,
    const uint8_t *pattern, const uint8_t *mask,
    unsigned int width, unsigned int height,
    unsigned int x, unsigned int y, term_color_t fg, term_color_t bg)
{
	struct vtgpu_softc *sc;
	struct fb_info *info;

	info = vd->vd_softc;
	sc = (struct vtgpu_softc *)info;

	vt_fb_bitblt_bitmap(vd, vw, pattern, mask, width, height, x, y, fg, bg);

	vtgpu_transfer_to_host_2d(sc, x, y, width, height);
	vtgpu_resource_flush(sc, x, y, width, height);
}

static int
vtgpu_fb_bitblt_argb(struct vt_device *vd, const struct vt_window *vw,
    const uint8_t *argb,
    unsigned int width, unsigned int height,
    unsigned int x, unsigned int y)
{

	return (EOPNOTSUPP);
}

static void
vtgpu_fb_drawrect(struct vt_device *vd, int x1, int y1, int x2, int y2,
    int fill, term_color_t color)
{
	struct vtgpu_softc *sc;
	struct fb_info *info;
	int width, height;

	info = vd->vd_softc;
	sc = (struct vtgpu_softc *)info;

	vt_fb_drawrect(vd, x1, y1, x2, y2, fill, color);

	width = x2 - x1 + 1;
	height = y2 - y1 + 1;
	vtgpu_transfer_to_host_2d(sc, x1, y1, width, height);
	vtgpu_resource_flush(sc, x1, y1, width, height);
}

static void
vtgpu_fb_setpixel(struct vt_device *vd, int x, int y, term_color_t color)
{
	struct vtgpu_softc *sc;
	struct fb_info *info;

	info = vd->vd_softc;
	sc = (struct vtgpu_softc *)info;

	vt_fb_setpixel(vd, x, y, color);

	vtgpu_transfer_to_host_2d(sc, x, y, 1, 1);
	vtgpu_resource_flush(sc, x, y, 1, 1);
}

static struct virtio_feature_desc vtgpu_feature_desc[] = {
	{ VIRTIO_GPU_F_VIRGL,		"VirGL"		},
	{ VIRTIO_GPU_F_EDID,		"EDID"		},
	{ VIRTIO_GPU_F_RESOURCE_UUID,	"ResUUID"	},
	{ VIRTIO_GPU_F_RESOURCE_BLOB,	"ResBlob"	},
	{ VIRTIO_GPU_F_CONTEXT_INIT,	"ContextInit"	},
	{ 0, NULL }
};

static device_method_t vtgpu_methods[] = {
	/* Device methods. */
	DEVMETHOD(device_probe,		vtgpu_probe),
	DEVMETHOD(device_attach,	vtgpu_attach),
	DEVMETHOD(device_detach,	vtgpu_detach),

	DEVMETHOD_END
};

static driver_t vtgpu_driver = {
	"vtgpu",
	vtgpu_methods,
	sizeof(struct vtgpu_softc)
};

VIRTIO_DRIVER_MODULE(virtio_gpu, vtgpu_driver, vtgpu_modevent, NULL);
MODULE_VERSION(virtio_gpu, 1);
MODULE_DEPEND(virtio_gpu, virtio, 1, 1, 1);

VIRTIO_SIMPLE_PNPINFO(virtio_gpu, VIRTIO_ID_GPU,
    "VirtIO GPU");

static int
vtgpu_modevent(module_t mod, int type, void *unused)
{
	int error;

	switch (type) {
	case MOD_LOAD:
	case MOD_QUIESCE:
	case MOD_UNLOAD:
	case MOD_SHUTDOWN:
		error = 0;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static int
vtgpu_probe(device_t dev)
{
	return (VIRTIO_SIMPLE_PROBE(dev, virtio_gpu));
}

static int
vtgpu_attach(device_t dev)
{
	struct vtgpu_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->vtgpu_have_fb_info = false;
	sc->vtgpu_dev = dev;
	sc->vtgpu_next_fence = 1;
	virtio_set_feature_desc(dev, vtgpu_feature_desc);

	error = vtgpu_setup_features(sc);
	if (error != 0) {
		device_printf(dev, "cannot setup features\n");
		goto fail;
	}

	vtgpu_read_config(sc, &sc->vtgpu_gpucfg);

	error = vtgpu_alloc_virtqueue(sc);
	if (error != 0) {
		device_printf(dev, "cannot allocate virtqueue\n");
		goto fail;
	}

	virtio_setup_intr(dev, INTR_TYPE_TTY);

	/* Read the device info to get the display size */
	error = vtgpu_get_display_info(sc);
	if (error != 0) {
		goto fail;
	}

	/*
	 * TODO: This doesn't need to be contigmalloc as we
	 * can use scatter-gather lists.
	 */
	sc->vtgpu_fb_info.fb_vbase = (vm_offset_t)contigmalloc(
	    sc->vtgpu_fb_info.fb_size, M_DEVBUF, M_WAITOK|M_ZERO, 0, ~0, 4, 0);
	sc->vtgpu_fb_info.fb_pbase = pmap_kextract(sc->vtgpu_fb_info.fb_vbase);

	/* Create the 2d resource */
	error = vtgpu_create_2d(sc);
	if (error != 0) {
		goto fail;
	}

	/* Attach the backing memory */
	error = vtgpu_attach_backing(sc);
	if (error != 0) {
		goto fail;
	}

	/* Set the scanout to link the framebuffer to the display scanout */
	error = vtgpu_set_scanout(sc, 0, 0, sc->vtgpu_fb_info.fb_width,
	    sc->vtgpu_fb_info.fb_height);
	if (error != 0) {
		goto fail;
	}

	vt_allocate(&vtgpu_fb_driver, &sc->vtgpu_fb_info);
	sc->vtgpu_have_fb_info = true;

	error = vtgpu_transfer_to_host_2d(sc, 0, 0, sc->vtgpu_fb_info.fb_width,
	    sc->vtgpu_fb_info.fb_height);
	if (error != 0)
		goto fail;
	error = vtgpu_resource_flush(sc, 0, 0, sc->vtgpu_fb_info.fb_width,
	    sc->vtgpu_fb_info.fb_height);

fail:
	if (error != 0)
		vtgpu_detach(dev);

	return (error);
}

static int
vtgpu_detach(device_t dev)
{
	struct vtgpu_softc *sc;

	sc = device_get_softc(dev);
	if (sc->vtgpu_have_fb_info)
		vt_deallocate(&vtgpu_fb_driver, &sc->vtgpu_fb_info);
	if (sc->vtgpu_fb_info.fb_vbase != 0) {
		MPASS(sc->vtgpu_fb_info.fb_size != 0);
		free((void *)sc->vtgpu_fb_info.fb_vbase,
		    M_DEVBUF);
	}

	/* TODO: Tell the host we are detaching */

	return (0);
}

static int
vtgpu_negotiate_features(struct vtgpu_softc *sc)
{
	device_t dev;
	uint64_t features;

	dev = sc->vtgpu_dev;
	features = VTGPU_FEATURES;

	sc->vtgpu_features = virtio_negotiate_features(dev, features);
	return (virtio_finalize_features(dev));
}

static int
vtgpu_setup_features(struct vtgpu_softc *sc)
{
	int error;

	error = vtgpu_negotiate_features(sc);
	if (error != 0)
		return (error);

	return (0);
}

static void
vtgpu_read_config(struct vtgpu_softc *sc,
    struct virtio_gpu_config *gpucfg)
{
	device_t dev;

	dev = sc->vtgpu_dev;

	bzero(gpucfg, sizeof(struct virtio_gpu_config));

#define VTGPU_GET_CONFIG(_dev, _field, _cfg)			\
	virtio_read_device_config(_dev,				\
	    offsetof(struct virtio_gpu_config, _field),	\
	    &(_cfg)->_field, sizeof((_cfg)->_field))		\

	VTGPU_GET_CONFIG(dev, events_read, gpucfg);
	VTGPU_GET_CONFIG(dev, events_clear, gpucfg);
	VTGPU_GET_CONFIG(dev, num_scanouts, gpucfg);
	VTGPU_GET_CONFIG(dev, num_capsets, gpucfg);

#undef VTGPU_GET_CONFIG
}

static int
vtgpu_alloc_virtqueue(struct vtgpu_softc *sc)
{
	device_t dev;
	struct vq_alloc_info vq_info[2];
	int nvqs;

	dev = sc->vtgpu_dev;
	nvqs = 1;

	VQ_ALLOC_INFO_INIT(&vq_info[0], 0, NULL, sc, &sc->vtgpu_ctrl_vq,
	    "%s control", device_get_nameunit(dev));

	return (virtio_alloc_virtqueues(dev, nvqs, vq_info));
}

static int
vtgpu_req_resp(struct vtgpu_softc *sc, void *req, size_t reqlen,
    void *resp, size_t resplen)
{
	struct sglist sg;
	struct sglist_seg segs[2];
	int error;

	sglist_init(&sg, 2, segs);

	error = sglist_append(&sg, req, reqlen);
	if (error != 0) {
		device_printf(sc->vtgpu_dev,
		    "Unable to append the request to the sglist: %d\n", error);
		return (error);
	}
	error = sglist_append(&sg, resp, resplen);
	if (error != 0) {
		device_printf(sc->vtgpu_dev,
		    "Unable to append the response buffer to the sglist: %d\n",
		    error);
		return (error);
	}
	error = virtqueue_enqueue(sc->vtgpu_ctrl_vq, resp, &sg, 1, 1);
	if (error != 0) {
		device_printf(sc->vtgpu_dev, "Enqueue failed: %d\n", error);
		return (error);
	}

	virtqueue_notify(sc->vtgpu_ctrl_vq);
	virtqueue_poll(sc->vtgpu_ctrl_vq, NULL);

	return (0);
}

static int
vtgpu_get_display_info(struct vtgpu_softc *sc)
{
	struct {
		struct virtio_gpu_ctrl_hdr req;
		char pad;
		struct virtio_gpu_resp_display_info resp;
	} s = { 0 };
	int error;

	s.req.type = htole32(VIRTIO_GPU_CMD_GET_DISPLAY_INFO);
	s.req.flags = htole32(VIRTIO_GPU_FLAG_FENCE);
	s.req.fence_id = htole64(atomic_fetchadd_64(&sc->vtgpu_next_fence, 1));

	error = vtgpu_req_resp(sc, &s.req, sizeof(s.req), &s.resp,
	    sizeof(s.resp));
	if (error != 0)
		return (error);

	for (int i = 0; i < sc->vtgpu_gpucfg.num_scanouts; i++) {
		if (s.resp.pmodes[i].enabled != 0)
			MPASS(i == 0);
			sc->vtgpu_fb_info.fb_name =
			    device_get_nameunit(sc->vtgpu_dev);

			sc->vtgpu_fb_info.fb_width =
			    le32toh(s.resp.pmodes[i].r.width);
			sc->vtgpu_fb_info.fb_height =
			    le32toh(s.resp.pmodes[i].r.height);
			/* 32 bits per pixel */
			sc->vtgpu_fb_info.fb_bpp = 32;
			sc->vtgpu_fb_info.fb_depth = 32;
			sc->vtgpu_fb_info.fb_size = sc->vtgpu_fb_info.fb_width *
			    sc->vtgpu_fb_info.fb_height * 4;
			sc->vtgpu_fb_info.fb_stride =
			    sc->vtgpu_fb_info.fb_width * 4;
			return (0);
	}

	return (ENXIO);
}

static int
vtgpu_create_2d(struct vtgpu_softc *sc)
{
	struct {
		struct virtio_gpu_resource_create_2d req;
		char pad;
		struct virtio_gpu_ctrl_hdr resp;
	} s = { 0 };
	int error;

	s.req.hdr.type = htole32(VIRTIO_GPU_CMD_RESOURCE_CREATE_2D);
	s.req.hdr.flags = htole32(VIRTIO_GPU_FLAG_FENCE);
	s.req.hdr.fence_id = htole64(
	    atomic_fetchadd_64(&sc->vtgpu_next_fence, 1));

	s.req.resource_id = htole32(VTGPU_RESOURCE_ID);
	s.req.format = htole32(VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM);
	s.req.width = htole32(sc->vtgpu_fb_info.fb_width);
	s.req.height = htole32(sc->vtgpu_fb_info.fb_height);

	error = vtgpu_req_resp(sc, &s.req, sizeof(s.req), &s.resp,
	    sizeof(s.resp));
	if (error != 0)
		return (error);

	if (s.resp.type != htole32(VIRTIO_GPU_RESP_OK_NODATA)) {
		device_printf(sc->vtgpu_dev, "Invalid reponse type %x\n",
		    le32toh(s.resp.type));
		return (EINVAL);
	}

	return (0);
}

static int
vtgpu_attach_backing(struct vtgpu_softc *sc)
{
	struct {
		struct {
			struct virtio_gpu_resource_attach_backing backing;
			struct virtio_gpu_mem_entry mem[1];
		} req;
		char pad;
		struct virtio_gpu_ctrl_hdr resp;
	} s = { 0 };
	int error;

	s.req.backing.hdr.type =
	    htole32(VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING);
	s.req.backing.hdr.flags = htole32(VIRTIO_GPU_FLAG_FENCE);
	s.req.backing.hdr.fence_id = htole64(
	    atomic_fetchadd_64(&sc->vtgpu_next_fence, 1));

	s.req.backing.resource_id = htole32(VTGPU_RESOURCE_ID);
	s.req.backing.nr_entries = htole32(1);

	s.req.mem[0].addr = htole64(sc->vtgpu_fb_info.fb_pbase);
	s.req.mem[0].length = htole32(sc->vtgpu_fb_info.fb_size);

	error = vtgpu_req_resp(sc, &s.req, sizeof(s.req), &s.resp,
	    sizeof(s.resp));
	if (error != 0)
		return (error);

	if (s.resp.type != htole32(VIRTIO_GPU_RESP_OK_NODATA)) {
		device_printf(sc->vtgpu_dev, "Invalid reponse type %x\n",
		    le32toh(s.resp.type));
		return (EINVAL);
	}

	return (0);
}

static int
vtgpu_set_scanout(struct vtgpu_softc *sc, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height)
{
	struct {
		struct virtio_gpu_set_scanout req;
		char pad;
		struct virtio_gpu_ctrl_hdr resp;
	} s = { 0 };
	int error;

	s.req.hdr.type = htole32(VIRTIO_GPU_CMD_SET_SCANOUT);
	s.req.hdr.flags = htole32(VIRTIO_GPU_FLAG_FENCE);
	s.req.hdr.fence_id = htole64(
	    atomic_fetchadd_64(&sc->vtgpu_next_fence, 1));

	s.req.r.x = htole32(x);
	s.req.r.y = htole32(y);
	s.req.r.width = htole32(width);
	s.req.r.height = htole32(height);

	s.req.scanout_id = 0;
	s.req.resource_id = htole32(VTGPU_RESOURCE_ID);

	error = vtgpu_req_resp(sc, &s.req, sizeof(s.req), &s.resp,
	    sizeof(s.resp));
	if (error != 0)
		return (error);

	if (s.resp.type != htole32(VIRTIO_GPU_RESP_OK_NODATA)) {
		device_printf(sc->vtgpu_dev, "Invalid reponse type %x\n",
		    le32toh(s.resp.type));
		return (EINVAL);
	}

	return (0);
}

static int
vtgpu_transfer_to_host_2d(struct vtgpu_softc *sc, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height)
{
	struct {
		struct virtio_gpu_transfer_to_host_2d req;
		char pad;
		struct virtio_gpu_ctrl_hdr resp;
	} s = { 0 };
	int error;

	s.req.hdr.type = htole32(VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D);
	s.req.hdr.flags = htole32(VIRTIO_GPU_FLAG_FENCE);
	s.req.hdr.fence_id = htole64(
	    atomic_fetchadd_64(&sc->vtgpu_next_fence, 1));

	s.req.r.x = htole32(x);
	s.req.r.y = htole32(y);
	s.req.r.width = htole32(width);
	s.req.r.height = htole32(height);

	s.req.offset = htole64((y * sc->vtgpu_fb_info.fb_width + x)
	 * (sc->vtgpu_fb_info.fb_bpp / 8));
	s.req.resource_id = htole32(VTGPU_RESOURCE_ID);

	error = vtgpu_req_resp(sc, &s.req, sizeof(s.req), &s.resp,
	    sizeof(s.resp));
	if (error != 0)
		return (error);

	if (s.resp.type != htole32(VIRTIO_GPU_RESP_OK_NODATA)) {
		device_printf(sc->vtgpu_dev, "Invalid reponse type %x\n",
		    le32toh(s.resp.type));
		return (EINVAL);
	}

	return (0);
}

static int
vtgpu_resource_flush(struct vtgpu_softc *sc, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height)
{
	struct {
		struct virtio_gpu_resource_flush req;
		char pad;
		struct virtio_gpu_ctrl_hdr resp;
	} s = { 0 };
	int error;

	s.req.hdr.type = htole32(VIRTIO_GPU_CMD_RESOURCE_FLUSH);
	s.req.hdr.flags = htole32(VIRTIO_GPU_FLAG_FENCE);
	s.req.hdr.fence_id = htole64(
	    atomic_fetchadd_64(&sc->vtgpu_next_fence, 1));

	s.req.r.x = htole32(x);
	s.req.r.y = htole32(y);
	s.req.r.width = htole32(width);
	s.req.r.height = htole32(height);

	s.req.resource_id = htole32(VTGPU_RESOURCE_ID);

	error = vtgpu_req_resp(sc, &s.req, sizeof(s.req), &s.resp,
	    sizeof(s.resp));
	if (error != 0)
		return (error);

	if (s.resp.type != htole32(VIRTIO_GPU_RESP_OK_NODATA)) {
		device_printf(sc->vtgpu_dev, "Invalid reponse type %x\n",
		    le32toh(s.resp.type));
		return (EINVAL);
	}

	return (0);
}
