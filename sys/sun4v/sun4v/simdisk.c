/*-
 * Copyright (c) 2006 Kip Macy
 * Copyright (c) 2001 Benno Rice
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
 *
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/sun4v/sun4v/simdisk.c,v 1.2 2006/11/24 05:27:49 kmacy Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <geom/geom.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>


#include <machine/hv_api.h>

#define	HVD_BLOCKSIZE	512

struct hvd_softc
{
        struct bio_queue_head hvd_bio_queue;
        struct mtx	hvd_queue_mtx;
	off_t		hvd_mediasize;
        unsigned	hvd_sectorsize;
        unsigned	hvd_fwheads;
        unsigned	hvd_fwsectors;
        struct proc	*hvd_procp;
        struct g_geom	*hvd_gp;
        struct g_provider *hvd_pp;
} hvd_softc;

static g_init_t g_hvd_init;
static g_start_t g_hvd_start;
static g_access_t g_hvd_access;

struct g_class g_hvd_class = {
	.name = "HVD",
	.version = G_VERSION,
	.init = g_hvd_init,
	.start = g_hvd_start,
	.access = g_hvd_access,
};

DECLARE_GEOM_CLASS(g_hvd_class, g_hvd);



static int
hvd_startio(struct hvd_softc *sc, struct bio *bp)
{
	u_int r;
	int len, rlen, wlen;
	uint64_t page_off;

	r = H_EOK;
	len = 0;

	page_off = bp->bio_offset & PAGE_MASK;


        switch (bp->bio_cmd) {
        case BIO_READ:
		if (bp->bio_length > (PAGE_SIZE - page_off)) {
			len = rlen = (PAGE_SIZE - page_off);
			r = hv_sim_read(bp->bio_offset, vtophys((char *)bp->bio_data), rlen);
		}
		for (; len < bp->bio_length && r == H_EOK; len += PAGE_SIZE) {
			rlen = (bp->bio_length - len) > PAGE_SIZE ? PAGE_SIZE : bp->bio_length - len;
			r = hv_sim_read(bp->bio_offset + len, vtophys((char *)bp->bio_data + len), rlen);

		}
                break;
        case BIO_WRITE:
		if (bp->bio_length > (PAGE_SIZE - page_off)) {
			len = wlen = (PAGE_SIZE - page_off);
			r = hv_sim_write(bp->bio_offset, vtophys((char *)bp->bio_data), wlen);
		}
		for (; len < bp->bio_length && r == H_EOK; len += PAGE_SIZE) {
			wlen = (bp->bio_length - len) > PAGE_SIZE ? PAGE_SIZE : bp->bio_length - len;
			r = hv_sim_write(bp->bio_offset + len, vtophys((char *)bp->bio_data + len), wlen);
		}
                break;
        }
	if (r != H_EOK)
		panic("invalid I/O");

        bp->bio_resid = 0;
        return (0);
}

static void
hvd_kthread(void *arg)
{
	struct hvd_softc *sc;
	struct bio *bp;
	int error;

        sc = arg;
        curthread->td_base_pri = PRIBIO;

        for (;;) {
		mtx_lock(&sc->hvd_queue_mtx);
		bp = bioq_takefirst(&sc->hvd_bio_queue);
		if (!bp) {
			msleep(sc, &sc->hvd_queue_mtx, PRIBIO | PDROP,
			    "hvdwait", 0);
                        continue;
		}
                mtx_unlock(&sc->hvd_queue_mtx);
                if (bp->bio_cmd == BIO_GETATTR) {
			error = EOPNOTSUPP;
                } else
			error = hvd_startio(sc, bp);

		if (error != -1) {
                        bp->bio_completed = bp->bio_length;
                        g_io_deliver(bp, error);
                }
	}
}

static void
g_hvd_init(struct g_class *mp __unused)
{
	struct hvd_softc *sc;
        struct g_geom *gp;
        struct g_provider *pp;
	int	error;
	printf("calling g_hvd_init\n");

	sc = (struct hvd_softc *)malloc(sizeof *sc, M_DEVBUF,
	         M_WAITOK|M_ZERO);
	bioq_init(&sc->hvd_bio_queue);
        mtx_init(&sc->hvd_queue_mtx, "hvd bio queue", NULL, MTX_DEF);
	sc->hvd_mediasize = (off_t)0x20000000;
	sc->hvd_sectorsize = HVD_BLOCKSIZE;
	sc->hvd_fwsectors = 0;
	sc->hvd_fwheads = 0;
	error = kthread_create(hvd_kthread, sc, &sc->hvd_procp, 0, 0,
		     "hvd0");
        if (error != 0) {
		free(sc, M_DEVBUF);
                return;
	}

	gp = g_new_geomf(&g_hvd_class, "hvd0");
	gp->softc = sc;
	pp = g_new_providerf(gp, "hvd0");
	pp->mediasize = sc->hvd_mediasize;
	pp->sectorsize = sc->hvd_sectorsize;
	sc->hvd_gp = gp;
	sc->hvd_pp = pp;
	g_error_provider(pp, 0);
}

static void
g_hvd_start(struct bio *bp)
{
        struct hvd_softc *sc;
#if 0
	printf("in hvd_start\n");
#endif
        sc = bp->bio_to->geom->softc;
        mtx_lock(&sc->hvd_queue_mtx);
        bioq_disksort(&sc->hvd_bio_queue, bp);
        mtx_unlock(&sc->hvd_queue_mtx);
        wakeup(sc);
}

static int
g_hvd_access(struct g_provider *pp, int r, int w, int e)
{

	if (pp->geom->softc == NULL)
		return (ENXIO);
        return (0);
}

static int
hvd_probe(device_t dev)
{

	if (strcmp(ofw_bus_get_name(dev), "disk"))
		return (ENXIO);
	
	device_set_desc(dev, "sun4v virtual disk");	

	return (0);
}


static int
hvd_attach(device_t dev)
{
	return (0);
}

static device_method_t hvd_methods[] = {
        DEVMETHOD(device_probe, hvd_probe),
        DEVMETHOD(device_attach, hvd_attach),
        {0, 0}
};


static driver_t hvd_driver = {
        "hvd",
        hvd_methods,
	0,
};


static devclass_t hvd_devclass;

DRIVER_MODULE(hvd, vnex, hvd_driver, hvd_devclass, 0, 0);
