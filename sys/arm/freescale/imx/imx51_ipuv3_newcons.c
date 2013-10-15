/*-
 * Copyright (c) 2012 Oleksandr Tymoshenko <gonzo@freebsd.org>
 * Copyright (c) 2012, 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Oleksandr Rybalko
 * under sponsorship from the FreeBSD Foundation.
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
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/fbio.h>
#include <sys/consio.h>

#include <sys/kdb.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/frame.h>
#include <machine/intr.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/vt/vt.h>
#include <dev/vt/colors/vt_termcolors.h>

#include <arm/freescale/imx/imx51_ccmvar.h>

#include <arm/freescale/imx/imx51_ipuv3reg.h>

#define	IMX51_IPU_HSP_CLOCK	665000000
#define	IPU3FB_FONT_HEIGHT	16

struct ipu3sc_softc {
	device_t		dev;

	intptr_t		sc_paddr;
	intptr_t		sc_vaddr;
	size_t			sc_fb_size;
	int			sc_depth;
	/* Storage for one pixel maybe bigger than color depth. */
	int			sc_bpp;
	int			sc_stride;
	int			sc_width;
	int			sc_height;
	uint32_t		sc_cmap[16];

	bus_space_tag_t		iot;
	bus_space_handle_t	ioh;
	bus_space_handle_t	cm_ioh;
	bus_space_handle_t	dp_ioh;
	bus_space_handle_t	di0_ioh;
	bus_space_handle_t	di1_ioh;
	bus_space_handle_t	dctmpl_ioh;
	bus_space_handle_t	dc_ioh;
	bus_space_handle_t	dmfc_ioh;
	bus_space_handle_t	idmac_ioh;
	bus_space_handle_t	cpmem_ioh;
};

static struct ipu3sc_softc *ipu3sc_softc;

static vd_init_t	vt_imx_init;
static vd_blank_t	vt_imx_blank;
static vd_bitbltchr_t	vt_imx_bitbltchr;

static struct vt_driver vt_imx_driver = {
	.vd_init = vt_imx_init,
	.vd_blank = vt_imx_blank,
	.vd_bitbltchr = vt_imx_bitbltchr,
};

#define	IPUV3_READ(ipuv3, module, reg)					\
	bus_space_read_4((ipuv3)->iot, (ipuv3)->module##_ioh, (reg))
#define	IPUV3_WRITE(ipuv3, module, reg, val)				\
	bus_space_write_4((ipuv3)->iot, (ipuv3)->module##_ioh, (reg), (val))

#define	CPMEM_CHANNEL_OFFSET(_c)	((_c) * 0x40)
#define	CPMEM_WORD_OFFSET(_w)		((_w) * 0x20)
#define	CPMEM_DP_OFFSET(_d)		((_d) * 0x10000)
#define	IMX_IPU_DP0		0
#define	IMX_IPU_DP1		1
#define	CPMEM_CHANNEL(_dp, _ch, _w)					\
	    (CPMEM_DP_OFFSET(_dp) + CPMEM_CHANNEL_OFFSET(_ch) +		\
		CPMEM_WORD_OFFSET(_w))
#define	CPMEM_OFFSET(_dp, _ch, _w, _o)					\
	    (CPMEM_CHANNEL((_dp), (_ch), (_w)) + (_o))

#define	IPUV3_DEBUG 100

#ifdef IPUV3_DEBUG
#define	SUBMOD_DUMP_REG(_sc, _m, _l)					\
	{								\
		int i;							\
		printf("*** " #_m " ***\n");				\
		for (i = 0; i <= (_l); i += 4) {			\
			if ((i % 32) == 0)				\
				printf("%04x: ", i & 0xffff);		\
			printf("0x%08x%c", IPUV3_READ((_sc), _m, i),	\
			    ((i + 4) % 32)?' ':'\n');			\
		}							\
		printf("\n");						\
	}
#endif

#ifdef IPUV3_DEBUG
int ipuv3_debug = IPUV3_DEBUG;
#define	DPRINTFN(n,x)   if (ipuv3_debug>(n)) printf x; else
#else
#define	DPRINTFN(n,x)
#endif

static int	ipu3_fb_probe(device_t);
static int	ipu3_fb_attach(device_t);

static int
ipu3_fb_malloc(struct ipu3sc_softc *sc, size_t size)
{

	sc->sc_vaddr = (intptr_t)contigmalloc(size, M_DEVBUF, M_ZERO, 0, ~0,
	    PAGE_SIZE, 0);
	sc->sc_paddr = (intptr_t)vtophys(sc->sc_vaddr);

	return (0);
}

static void
ipu3_fb_init(struct ipu3sc_softc *sc)
{
	uint64_t w0sh96;
	uint32_t w1sh96;

	/* FW W0[137:125] - 96 = [41:29] */
	/* FH W0[149:138] - 96 = [53:42] */
	w0sh96 = IPUV3_READ(sc, cpmem, CPMEM_OFFSET(IMX_IPU_DP1, 23, 0, 16));
	w0sh96 <<= 32;
	w0sh96 |= IPUV3_READ(sc, cpmem, CPMEM_OFFSET(IMX_IPU_DP1, 23, 0, 12));

	sc->sc_width = ((w0sh96 >> 29) & 0x1fff) + 1;
	sc->sc_height = ((w0sh96 >> 42) & 0x0fff) + 1;

	/* SLY W1[115:102] - 96 = [19:6] */
	w1sh96 = IPUV3_READ(sc, cpmem, CPMEM_OFFSET(IMX_IPU_DP1, 23, 1, 12));
	sc->sc_stride = ((w1sh96 >> 6) & 0x3fff) + 1;

	printf("%dx%d [%d]\n", sc->sc_width, sc->sc_height, sc->sc_stride);
	sc->sc_fb_size = sc->sc_height * sc->sc_stride;

	ipu3_fb_malloc(sc, sc->sc_fb_size);

	/* DP1 + config_ch_23 + word_2 */
	IPUV3_WRITE(sc, cpmem, CPMEM_OFFSET(IMX_IPU_DP1, 23, 1, 0),
	    (((uint32_t)sc->sc_paddr >> 3) |
	    (((uint32_t)sc->sc_paddr >> 3) << 29)) & 0xffffffff);

	IPUV3_WRITE(sc, cpmem, CPMEM_OFFSET(IMX_IPU_DP1, 23, 1, 4),
	    (((uint32_t)sc->sc_paddr >> 3) >> 3) & 0xffffffff);

	/* XXX: fetch or set it from/to IPU. */
	sc->sc_depth = sc->sc_stride / sc->sc_width * 8;
	sc->sc_bpp = sc->sc_stride / sc->sc_width;
}

static int
ipu3_fb_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "fsl,ipu3"))
		return (ENXIO);

	device_set_desc(dev, "i.MX515 Image Processing Unit (FB)");

	return (BUS_PROBE_DEFAULT);
}

static int
ipu3_fb_attach(device_t dev)
{
	struct ipu3sc_softc *sc = device_get_softc(dev);
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int err;

	ipu3sc_softc = sc;

	device_printf(dev, "\tclock gate status is %d\n",
	    imx51_get_clk_gating(IMX51CLK_IPU_HSP_CLK_ROOT));

	sc->dev = dev;

	sc = device_get_softc(dev);
	sc->iot = iot = fdtbus_bs_tag;

	device_printf(sc->dev, ": i.MX51 IPUV3 controller\n");

	/* map controller registers */
	err = bus_space_map(iot, IPU_CM_BASE, IPU_CM_SIZE, 0, &ioh);
	if (err)
		goto fail_retarn_cm;
	sc->cm_ioh = ioh;

	/* map Display Multi FIFO Controller registers */
	err = bus_space_map(iot, IPU_DMFC_BASE, IPU_DMFC_SIZE, 0, &ioh);
	if (err)
		goto fail_retarn_dmfc;
	sc->dmfc_ioh = ioh;

	/* map Display Interface 0 registers */
	err = bus_space_map(iot, IPU_DI0_BASE, IPU_DI0_SIZE, 0, &ioh);
	if (err)
		goto fail_retarn_di0;
	sc->di0_ioh = ioh;

	/* map Display Interface 1 registers */
	err = bus_space_map(iot, IPU_DI1_BASE, IPU_DI0_SIZE, 0, &ioh);
	if (err)
		goto fail_retarn_di1;
	sc->di1_ioh = ioh;

	/* map Display Processor registers */
	err = bus_space_map(iot, IPU_DP_BASE, IPU_DP_SIZE, 0, &ioh);
	if (err)
		goto fail_retarn_dp;
	sc->dp_ioh = ioh;

	/* map Display Controller registers */
	err = bus_space_map(iot, IPU_DC_BASE, IPU_DC_SIZE, 0, &ioh);
	if (err)
		goto fail_retarn_dc;
	sc->dc_ioh = ioh;

	/* map Image DMA Controller registers */
	err = bus_space_map(iot, IPU_IDMAC_BASE, IPU_IDMAC_SIZE, 0, &ioh);
	if (err)
		goto fail_retarn_idmac;
	sc->idmac_ioh = ioh;

	/* map CPMEM registers */
	err = bus_space_map(iot, IPU_CPMEM_BASE, IPU_CPMEM_SIZE, 0, &ioh);
	if (err)
		goto fail_retarn_cpmem;
	sc->cpmem_ioh = ioh;

	/* map DCTEMPL registers */
	err = bus_space_map(iot, IPU_DCTMPL_BASE, IPU_DCTMPL_SIZE, 0, &ioh);
	if (err)
		goto fail_retarn_dctmpl;
	sc->dctmpl_ioh = ioh;

#ifdef notyet
	sc->ih = imx51_ipuv3_intr_establish(IMX51_INT_IPUV3, IPL_BIO,
	    ipuv3intr, sc);
	if (sc->ih == NULL) {
		device_printf(sc->dev,
		    "unable to establish interrupt at irq %d\n",
		    IMX51_INT_IPUV3);
		return (ENXIO);
	}
#endif

	/*
	 * We have to wait until interrupts are enabled. 
	 * Mailbox relies on it to get data from VideoCore
	 */
	ipu3_fb_init(sc);
	err = vt_generate_vga_palette(sc->sc_cmap, COLOR_FORMAT_RGB, 0xff, 16,
	    0xff, 8, 0xff, 0);
	if (err)
		goto fail_retarn_dctmpl;


	vt_allocate(&vt_imx_driver, sc);

	return (0);

fail_retarn_dctmpl:
	bus_space_unmap(sc->iot, sc->cpmem_ioh, IPU_CPMEM_SIZE);
fail_retarn_cpmem:
	bus_space_unmap(sc->iot, sc->idmac_ioh, IPU_IDMAC_SIZE);
fail_retarn_idmac:
	bus_space_unmap(sc->iot, sc->dc_ioh, IPU_DC_SIZE);
fail_retarn_dp:
	bus_space_unmap(sc->iot, sc->dp_ioh, IPU_DP_SIZE);
fail_retarn_dc:
	bus_space_unmap(sc->iot, sc->di1_ioh, IPU_DI1_SIZE);
fail_retarn_di1:
	bus_space_unmap(sc->iot, sc->di0_ioh, IPU_DI0_SIZE);
fail_retarn_di0:
	bus_space_unmap(sc->iot, sc->dmfc_ioh, IPU_DMFC_SIZE);
fail_retarn_dmfc:
	bus_space_unmap(sc->iot, sc->dc_ioh, IPU_CM_SIZE);
fail_retarn_cm:
	device_printf(sc->dev,
	    "failed to map registers (errno=%d)\n", err);
	return (err);
}

static device_method_t ipu3_fb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ipu3_fb_probe),
	DEVMETHOD(device_attach,	ipu3_fb_attach),

	{ 0, 0 }
};

static devclass_t ipu3_fb_devclass;

static driver_t ipu3_fb_driver = {
	"fb",
	ipu3_fb_methods,
	sizeof(struct ipu3sc_softc),
};

DRIVER_MODULE(ipu3fb, simplebus, ipu3_fb_driver, ipu3_fb_devclass, 0, 0);

static void
vt_imx_blank(struct vt_device *vd, term_color_t color)
{
	struct ipu3sc_softc *sc = vd->vd_softc;
	u_int ofs;
	uint32_t c;

	c = sc->sc_cmap[color];
	switch (sc->sc_bpp) {
	case 1:
		for (ofs = 0; ofs < (sc->sc_stride * sc->sc_height); ofs++)
			*(uint8_t *)(sc->sc_vaddr + ofs) = c & 0xff;
		break;
	case 2:
		/* XXX must be 16bits colormap */
		for (ofs = 0; ofs < (sc->sc_stride * sc->sc_height); ofs++)
			*(uint16_t *)(sc->sc_vaddr + 2 * ofs) = c & 0xffff;
		break;
	case 3: /*  */
		/* line 0 */
		for (ofs = 0; ofs < sc->sc_stride; ofs++) {
			*(uint8_t *)(sc->sc_vaddr + ofs + 0) = c >>  0& 0xff;
			*(uint8_t *)(sc->sc_vaddr + ofs + 1) = c >>  8 & 0xff;
			*(uint8_t *)(sc->sc_vaddr + ofs + 2) = c >> 16 & 0xff;
		}
		/* Copy line0 to all other lines. */
		for (ofs = 1; ofs < sc->sc_height; ofs++) {
			memmove((void *)(sc->sc_vaddr + ofs * sc->sc_stride),
			    (void *)sc->sc_vaddr, sc->sc_stride);
		}
		break;
	case 4:
		for (ofs = 0; ofs < (sc->sc_stride * sc->sc_height); ofs++)
			*(uint32_t *)(sc->sc_vaddr + 4 * ofs) = c;
		break;
	default:
		/* panic? */
		break;
	}
}

static void
vt_imx_bitbltchr(struct vt_device *vd, const uint8_t *src,
    vt_axis_t top, vt_axis_t left, unsigned int width, unsigned int height,
    term_color_t fg, term_color_t bg)
{
	struct ipu3sc_softc *sc = vd->vd_softc;
	u_long line;
	uint32_t fgc, bgc, cc, o;
	int c, l, bpp;
	uint8_t b = 0;

	bpp = sc->sc_bpp;
	fgc = sc->sc_cmap[fg];
	bgc = sc->sc_cmap[bg];

	line = sc->sc_vaddr + (sc->sc_stride * top) + (left * bpp);
	for (l = 0; l < height; l++) {
		for (c = 0; c < width; c++) {
			if (c % 8 == 0)
				b = *src++;
			else
				b <<= 1;
			o = line + (c * bpp);
			cc = b & 0x80 ? fgc : bgc;

			switch(bpp) {
			case 1:
				*(uint8_t *)(o) = cc;
				break;
			case 2:
				*(uint16_t *)(o) = cc;
				break;
			case 3:
				/* Packed mode, so unaligned. Byte access. */
				*(uint8_t *)(o + 0) = (cc >> 16) & 0xff;
				*(uint8_t *)(o + 1) = (cc >>  8) & 0xff;
				*(uint8_t *)(o + 2) = (cc >>  0) & 0xff;
				break;
			case 4:
				/* Cover both: 32bits and aligned 24bits. */
				*(uint32_t *)(o) = cc;
				break;
			default:
				/* panic? */
				break;
			}
		}
		line += sc->sc_stride;
	}
}

static int
vt_imx_init(struct vt_device *vd)
{
	struct ipu3sc_softc *sc;

	sc = vd->vd_softc;

	vd->vd_height = sc->sc_height;
	vd->vd_width = sc->sc_width;

	/* Clear the screen. */
	vt_imx_blank(vd, TC_BLACK);

	return (CN_INTERNAL);
}
