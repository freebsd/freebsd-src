/*-
 * Copyright (c) 2002 Poul-Henning Kamp
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
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

#include "opt_geom.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/stdint.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <sys/lock.h>
#include <sys/mutex.h>
#include <geom/geom.h>
#include <geom/geom_int.h>
#include <geom/geom_stats.h>

#define statsperpage (PAGE_SIZE / sizeof(struct g_stat))

struct statspage {
	TAILQ_ENTRY(statspage)	list;
	struct g_stat		*stat;
	u_int			nfree;
};

static TAILQ_HEAD(, statspage)	pagelist = TAILQ_HEAD_INITIALIZER(pagelist);

struct g_stat *
g_stat_new(void *id)
{
	struct g_stat *gsp;
	struct statspage *spp;
	u_int u;

	g_topology_assert();
	TAILQ_FOREACH(spp, &pagelist, list) {
		if (spp->nfree > 0)
			break;
	}
	if (spp == NULL) {
		spp = g_malloc(sizeof *spp, M_ZERO);
		TAILQ_INSERT_TAIL(&pagelist, spp, list);
		spp->stat = g_malloc(PAGE_SIZE, M_ZERO);
		spp->nfree = statsperpage;
	}
	gsp = spp->stat;
	for (u = 0; u < statsperpage; u++) {
		if (gsp->id == NULL)
			break;
		gsp++;
	}
	spp->nfree--;
	gsp->id = id;
	return (gsp);
}

void
g_stat_delete(struct g_stat *gsp)
{
	struct statspage *spp;

	bzero(gsp, sizeof *gsp);
	TAILQ_FOREACH(spp, &pagelist, list) {
		if (gsp >= spp->stat && gsp < (spp->stat + statsperpage)) {
			spp->nfree++;
			return;
		}
	}
}

static d_mmap_t g_stat_mmap;

static struct cdevsw geom_stats_cdevsw = {
	/* open */	nullopen,
	/* close */	nullclose,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	noioctl,
	/* poll */	nopoll,
	/* mmap */	g_stat_mmap,
	/* strtegy */	nostrategy,
	/* name */	"g_stats",
	/* maj */	GEOM_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

static int
g_stat_mmap(dev_t dev, vm_offset_t offset, int nprot)
{
	struct statspage *spp;

	if (nprot != VM_PROT_READ)
		return (-1);
	TAILQ_FOREACH(spp, &pagelist, list) {
		if (offset == 0)
			return (vtophys(spp->stat) >> PAGE_SHIFT);
		offset -= PAGE_SIZE;
	}
	return (-1);
}

void
g_stat_init(void)
{
	make_dev(&geom_stats_cdevsw, GEOM_MINOR_STATS,
	    UID_ROOT, GID_WHEEL, 0400, GEOM_STATS_DEVICE);
}
