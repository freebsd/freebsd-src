/*-
 * Copyright (c) 2025 Martin Filla
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/bus.h>
#include <sys/selinfo.h>
#include <sys/fcntl.h>

#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/clk/clk.h>

#define MT_RNG_CTRL         0x00
#define MT_NG_EN            (1U << 0)
#define MT_RNG_READY        (1U << 31)
#define MT_RNG_DATA            0x08

#define USEC_POLL           2
#define TIMEOUT_POLL_US     60

static d_open_t  mtk_rng_open;
static d_read_t  mtk_rng_read;
static d_close_t mtk_rng_close;

static struct cdevsw mtk_rng_cdevsw = {
        .d_version = D_VERSION,
        .d_open    = mtk_rng_open,
        .d_read    = mtk_rng_read,
        .d_close   = mtk_rng_close,
        .d_name    = "mtk_rng",
};

struct mt7622_rng_softc {
    device_t        dev;
    struct resource *res_mem;
    int             rid_mem;
    clk_t           clk;
    struct cdev     *cdev;
    struct mtx      mtx;
    bool            enabled;
};

static inline uint32_t
mtk_rng_read4(struct mt7622_rng_softc *sc, bus_size_t off)
{
    return bus_read_4(sc->res_mem, off);
}

static inline void
mtk_rng_write4(struct mt7622_rng_softc *sc, bus_size_t off, uint32_t val)
{
    bus_write_4(sc->res_mem, off, val);
}

static int
mtk_rng_enable(struct mt7622_rng_softc *sc)
{
    uint32_t v;
    int error;

    mtx_lock(&sc->mtx);

    if (sc->enabled) {
        mtx_unlock(&sc->mtx);
        return (0);
    }

    if (sc->clk != NULL) {
        error = clk_enable(sc->clk);
        if (error != 0) {
            mtx_unlock(&sc->mtx);
            device_printf(sc->dev, "clk_enable failed: %d\n", error);
            return (error);
        }
    }

    v = mtk_rng_read4(sc, MT_RNG_CTRL);
    v |= MT_NG_EN;
    mtk_rng_write4(sc, MT_RNG_CTRL, v);

    sc->enabled = true;
    mtx_unlock(&sc->mtx);
    return (0);
}

static void
mtk_rng_disable(struct mt7622_rng_softc *sc)
{
    uint32_t v;

    mtx_lock(&sc->mtx);

    if (!sc->enabled) {
        mtx_unlock(&sc->mtx);
        return;
    }

    v = mtk_rng_read4(sc, MT_RNG_CTRL);
    v &= ~MT_NG_EN;
    mtk_rng_write4(sc, MT_RNG_CTRL, v);

    if (sc->clk != NULL)
        clk_disable(sc->clk);

    sc->enabled = false;
    mtx_unlock(&sc->mtx);
}

/* Wait for READY bit; vrací 1 = ready, 0 = timeout */
static int
mtk_rng_wait_ready(struct mt7622_rng_softc *sc, bool wait)
{
    uint32_t v;
    int waited = 0;

    v = mtk_rng_read4(sc, MT_RNG_CTRL);
    if (v & MT_RNG_READY)
        return 1;

    if (!wait)
        return 0;

    while (waited < TIMEOUT_POLL_US) {
        DELAY(USEC_POLL);
        waited += USEC_POLL;
        v = mtk_rng_read4(sc, MT_RNG_CTRL);
        if (v & MT_RNG_READY)
            return 1;
    }
    return 0;
}

/* cdev ops */
static int
mtk_rng_open(struct cdev *cdev, int oflags, int devtype, struct thread *td)
{
    struct mt7622_rng_softc *sc = cdev->si_drv1;
    return mtk_rng_enable(sc);
}

static int
mtk_rng_close(struct cdev *cdev, int fflag, int devtype, struct thread *td)
{
    struct mt7622_rng_softc *sc = cdev->si_drv1;
    mtk_rng_disable(sc);
    return (0);
}

static int
mtk_rng_read(struct cdev *cdev, struct uio *uio, int ioflag)
{
    struct mt7622_rng_softc *sc = cdev->si_drv1;
    uint32_t word;
    int error = 0;

    while (uio->uio_resid >= sizeof(uint32_t)) {
        bool wait = (ioflag & O_NONBLOCK) == 0;

        if (!mtk_rng_wait_ready(sc, wait)) {
            if (!wait)
                return (EWOULDBLOCK);

            return (EIO);
        }

        word = mtk_rng_read4(sc, MT_RNG_DATA);
        error = uiomove(&word, sizeof(word), uio);
        if (error != 0)
            break;
    }

    return (error);
}

static const struct ofw_compat_data mtk_rng_compat[] = {
        {"mediatek,mt7623-rng", 1},
        {"mediatek,mt7622-rng", 1},
        {NULL, 0}
};

static int
mt7622_rng_probe(device_t dev)
{
    if (!ofw_bus_status_okay(dev))
        return (ENXIO);
    if (ofw_bus_search_compatible(dev, mtk_rng_compat)->ocd_data == 0)
        return (ENXIO);

    device_set_desc(dev, "MediaTek Hardware RNG");
    return (BUS_PROBE_DEFAULT);
}

static int
mt7622_rng_attach(device_t dev)
{
    struct mt7622_rng_softc *sc = device_get_softc(dev);
    int error;

    sc->dev = dev;
    sc->rid_mem = 0;

    mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

    sc->res_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
                                         &sc->rid_mem, RF_ACTIVE);
    if (sc->res_mem == NULL) {
        device_printf(dev, "cannot allocate MMIO resource\n");
        error = ENXIO;
        goto fail;
    }

    sc->clk = NULL;
    (void)clk_get_by_ofw_name(dev, 0, "rng", &sc->clk);

    sc->enabled = false;
    sc->cdev = make_dev(&mtk_rng_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
                        "mtk_rng");
    if (sc->cdev == NULL) {
        device_printf(dev, "make_dev failed\n");
        error = ENXIO;
        goto fail;
    }
    sc->cdev->si_drv1 = sc;

    device_printf(dev, "registered /dev/mtk_rng\n");
    return (0);

    fail:
    if (sc->clk != NULL)
        clk_release(sc->clk);
    if (sc->res_mem != NULL)
        bus_release_resource(dev, SYS_RES_MEMORY, sc->rid_mem, sc->res_mem);
    mtx_destroy(&sc->mtx);
    return (error);
}

static int
mt7622_rng_detach(device_t dev)
{
    struct mt7622_rng_softc *sc = device_get_softc(dev);

    if (sc->cdev != NULL) {
        destroy_dev(sc->cdev);
        sc->cdev = NULL;
    }

    mtk_rng_disable(sc);

    if (sc->clk != NULL) {
        clk_release(sc->clk);
        sc->clk = NULL;
    }

    if (sc->res_mem != NULL)
        bus_release_resource(dev, SYS_RES_MEMORY, sc->rid_mem, sc->res_mem);

    mtx_destroy(&sc->mtx);
    return (0);
}

static device_method_t mt7622_rng_methods[] = {
        DEVMETHOD(device_probe,  mt7622_rng_probe),
        DEVMETHOD(device_attach, mt7622_rng_attach),
        DEVMETHOD(device_detach, mt7622_rng_detach),
        DEVMETHOD_END
};

static DEFINE_CLASS_0(mt7622_rng, mt7622_rng_driver, mt7622_rng_methods,
sizeof(struct mt7622_rng_softc));
DRIVER_MODULE(mt7622_rng, simplebus, mt7622_rng_driver, NULL, NULL);
MODULE_VERSION(mt7622_rng, 1);
MODULE_DEPEND(mt7622_rng, ofw_bus, 1, 1, 1);
MODULE_DEPEND(mt7622_rng, clk, 1, 1, 1);

