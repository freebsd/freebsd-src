/*-
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
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


#include <sys/param.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <err.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <geom/geom.h>

static g_orphan_t g_dev_orphan;

static struct g_geom *
dev_taste(struct g_class *mp, struct g_provider *pp, int insist __unused)
{
	struct g_geom *gp;
	struct g_consumer *cp;

	g_trace(G_T_TOPOLOGY, "dev_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	LIST_FOREACH(cp, &pp->consumers, consumers) {
		if (cp->geom->class == mp) {
			g_topology_unlock();
			return (NULL);
		}
	}
	gp = g_new_geomf(mp, pp->name);
	gp->orphan = g_dev_orphan;
	cp = g_new_consumer(gp);
	g_attach(cp, pp);
	return (gp);
}


static void
g_dev_orphan(struct g_consumer *cp)
{
	struct g_geom *gp;

	gp = cp->geom;
	gp->flags |= G_GEOM_WITHER;
	g_trace(G_T_TOPOLOGY, "g_dev_orphan(%p(%s))", cp, gp->name);
	if (cp->biocount > 0)
		return;
	if (cp->acr  > 0 || cp->acw  > 0 || cp->ace > 0)
		g_access_rel(cp, -cp->acr, -cp->acw, -cp->ace);
	g_detach(cp);
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
}


static struct g_class dev_class	= {
	"DEV-class",
	dev_taste,
	NULL,
	G_CLASS_INITIALIZER
};

static struct g_geom *
g_dev_findg(const char *name)
{
	struct g_geom *gp;

	LIST_FOREACH(gp, &dev_class.geom, geom)
		if (!strcmp(gp->name, name))
			break;
	return (gp);
}

void
g_dev_init(void *junk __unused)
{

	g_add_class(&dev_class);
}


struct g_consumer *
g_dev_opendev(const char *name, int r, int w, int e)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	int error;

	gp = g_dev_findg(name);
	if (gp == NULL)
		return (NULL);
	g_topology_lock();
	cp = LIST_FIRST(&gp->consumer);
	error = g_access_rel(cp, r, w, e);
	g_topology_unlock();
	if (error)
		return(NULL);
	return(cp);
}

static void
g_dev_done(struct bio *bp)
{

	if (bp->bio_from->biocount > 0)
		return;
	g_topology_lock();
	g_dev_orphan(bp->bio_from);
	g_topology_unlock();
}

int
g_dev_request(const char *name, struct bio *bp)
{
	struct g_geom *gp;

	gp = g_dev_findg(name);
	if (gp == NULL)
		return (-1);
	bp->bio_done = g_dev_done;
	g_io_request(bp, LIST_FIRST(&gp->consumer));
	return (1);
}
