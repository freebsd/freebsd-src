/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2005 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * All rights reserved.
 * Copyright (c) 2025 Mateusz Piotrowski <0mp@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/bio.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/types.h>

#include <geom/geom.h>

#define	G_ZERO_CLASS_NAME	"ZERO"

static int	g_zero_byte_sysctl(SYSCTL_HANDLER_ARGS);

SYSCTL_DECL(_kern_geom);
static SYSCTL_NODE(_kern_geom, OID_AUTO, zero, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "GEOM_ZERO stuff");
static int g_zero_clear = 1;
SYSCTL_INT(_kern_geom_zero, OID_AUTO, clear,
    CTLFLAG_RWTUN, &g_zero_clear, 0,
    "Clear read data buffer");
static int g_zero_byte = 0;
static uint8_t g_zero_buffer[PAGE_SIZE];
SYSCTL_PROC(_kern_geom_zero, OID_AUTO, byte,
    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, &g_zero_byte, 0,
    g_zero_byte_sysctl, "I",
    "Byte (octet) value to clear the buffers with");

static struct g_provider *gpp;

static int
g_zero_byte_sysctl(SYSCTL_HANDLER_ARGS)
{
	int error;

	// XXX: Confirm that this is called on module load as well.
	// XXX: Shouldn't we lock here to avoid changing the byte value if the
	// driver is in the process of handling I/O?
	error = sysctl_handle_int(oidp, &g_zero_byte, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	memset(g_zero_buffer, g_zero_byte, PAGE_SIZE);
	return (0);
}

static void
g_zero_fill_pages(struct bio *bp)
{
	struct iovec aiovec;
	struct uio auio;
	size_t length;
	vm_offset_t offset;

	aiovec.iov_base = g_zero_buffer;
	aiovec.iov_len = PAGE_SIZE;
	auio.uio_iov = &aiovec;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_WRITE;
	auio.uio_td = curthread;

	/*
	 * To handle the unmapped I/O request, we need to fill the pages in the
	 * bp->bio_ma array with the g_zero_byte value. However, instead of
	 * setting every byte individually, we use uiomove_fromphys() to fill a
	 * page at a time with g_zero_buffer.
	 */
	bp->bio_resid = bp->bio_length;
	offset = bp->bio_ma_offset & PAGE_MASK;
	for (int i = 0; i < bp->bio_ma_n && bp->bio_resid > 0; i++) {
		length = MIN(PAGE_SIZE - offset, bp->bio_resid);
		auio.uio_resid = length;

		(void)uiomove_fromphys(&bp->bio_ma[i], offset, length, &auio);

		offset = 0;
		bp->bio_resid -= length;
	}
}


static void
g_zero_start(struct bio *bp)
{
	int error;

	switch (bp->bio_cmd) {
	case BIO_READ:
		if (g_zero_clear) {
			if ((bp->bio_flags & BIO_UNMAPPED) != 0)
				g_zero_fill_pages(bp);
			else
				memset(bp->bio_data, g_zero_byte,
				    bp->bio_length);
		}
		/* FALLTHROUGH */
	case BIO_DELETE:
	case BIO_WRITE:
		bp->bio_completed = bp->bio_length;
		error = 0;
		break;
	case BIO_GETATTR:
	default:
		error = EOPNOTSUPP;
		break;
	}
	g_io_deliver(bp, error);
}

static void
g_zero_init(struct g_class *mp)
{
	struct g_geom *gp;
	struct g_provider *pp;

	g_topology_assert();
	gp = g_new_geom(mp, "gzero");
	gp->start = g_zero_start;
	gp->access = g_std_access;
	gpp = pp = g_new_providerf(gp, "%s", gp->name);
	pp->flags |= G_PF_ACCEPT_UNMAPPED | G_PF_DIRECT_SEND |
	    G_PF_DIRECT_RECEIVE;
	pp->mediasize = 1152921504606846976LLU;
	pp->sectorsize = 512;
	g_error_provider(pp, 0);
}

static int
g_zero_destroy_geom(struct gctl_req *req __unused, struct g_class *mp __unused,
    struct g_geom *gp)
{
	struct g_provider *pp;

	g_topology_assert();
	if (gp == NULL)
		return (0);
	pp = LIST_FIRST(&gp->provider);
	if (pp == NULL)
		return (0);
	if (pp->acr > 0 || pp->acw > 0 || pp->ace > 0)
		return (EBUSY);
	gpp = NULL;
	g_wither_geom(gp, ENXIO);
	return (0);
}

static struct g_class g_zero_class = {
	.name = G_ZERO_CLASS_NAME,
	.version = G_VERSION,
	.init = g_zero_init,
	.destroy_geom = g_zero_destroy_geom
};

DECLARE_GEOM_CLASS(g_zero_class, g_zero);
MODULE_VERSION(geom_zero, 0);
