/*-
 * Copyright (c) 2015 Marcel Moolenaar
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/bus.h>
#include <machine/bus_dma.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/rman.h>
#include <sys/sbuf.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/proto/proto.h>
#include <dev/proto/proto_dev.h>
#include <dev/proto/proto_busdma.h>

MALLOC_DEFINE(M_PROTO_BUSDMA, "proto_busdma", "DMA management data");

static int
proto_busdma_tag_create(struct proto_busdma *busdma, struct proto_tag *parent,
    struct proto_ioc_busdma *ioc)
{
	struct proto_tag *tag;

	/*
	 * If nsegs is 1, ignore maxsegsz. What this means is that if we have
	 * just 1 segment, then maxsz should be equal to maxsegsz. To keep it
	 * simple for us, limit maxsegsz to maxsz in any case.
	 */
	if (ioc->u.tag.maxsegsz > ioc->u.tag.maxsz || ioc->u.tag.nsegs == 1)
		ioc->u.tag.maxsegsz = ioc->u.tag.maxsz;

	/* A bndry of 0 really means ~0, or no boundary. */
	if (ioc->u.tag.bndry == 0)
		ioc->u.tag.bndry = ~0U;

	tag = malloc(sizeof(*tag), M_PROTO_BUSDMA, M_WAITOK | M_ZERO);
	if (parent != NULL) {
		tag->parent = parent;
		LIST_INSERT_HEAD(&parent->children, tag, peers);
		tag->align = MAX(ioc->u.tag.align, parent->align);
		tag->bndry = MIN(ioc->u.tag.bndry, parent->bndry);
		tag->maxaddr = MIN(ioc->u.tag.maxaddr, parent->maxaddr);
		tag->maxsz = MIN(ioc->u.tag.maxsz, parent->maxsz);
		tag->maxsegsz = MIN(ioc->u.tag.maxsegsz, parent->maxsegsz);
		tag->nsegs = MIN(ioc->u.tag.nsegs, parent->nsegs);
		tag->datarate = MIN(ioc->u.tag.datarate, parent->datarate);
		/* Write constraints back */
		ioc->u.tag.align = tag->align;
		ioc->u.tag.bndry = tag->bndry;
		ioc->u.tag.maxaddr = tag->maxaddr;
		ioc->u.tag.maxsz = tag->maxsz;
		ioc->u.tag.maxsegsz = tag->maxsegsz;
		ioc->u.tag.nsegs = tag->nsegs;
		ioc->u.tag.datarate = tag->datarate;
	} else {
		tag->align = ioc->u.tag.align;
		tag->bndry = ioc->u.tag.bndry;
		tag->maxaddr = ioc->u.tag.maxaddr;
		tag->maxsz = ioc->u.tag.maxsz;
		tag->maxsegsz = ioc->u.tag.maxsegsz;
		tag->nsegs = ioc->u.tag.nsegs;
		tag->datarate = ioc->u.tag.datarate;
	}
	LIST_INSERT_HEAD(&busdma->tags, tag, tags);
	ioc->result = (uintptr_t)(void *)tag;
	return (0);
}

static int
proto_busdma_tag_destroy(struct proto_busdma *busdma, struct proto_tag *tag)
{

	if (!LIST_EMPTY(&tag->mds))
		return (EBUSY);
	if (!LIST_EMPTY(&tag->children))
		return (EBUSY);

	if (tag->parent != NULL) {
		LIST_REMOVE(tag, peers);
		tag->parent = NULL;
	}
	LIST_REMOVE(tag, tags);
	free(tag, M_PROTO_BUSDMA);
	return (0);
}

static struct proto_tag *
proto_busdma_tag_lookup(struct proto_busdma *busdma, u_long key)
{
	struct proto_tag *tag;

	LIST_FOREACH(tag, &busdma->tags, tags) {
		if ((void *)tag == (void *)key)
			return (tag);
	}
	return (NULL);
}

static int
proto_busdma_mem_alloc(struct proto_busdma *busdma, struct proto_tag *tag,
    struct proto_ioc_busdma *ioc)
{
	struct proto_md *md;
	int error;

	md = malloc(sizeof(*md), M_PROTO_BUSDMA, M_WAITOK | M_ZERO);
	md->tag = tag;
 
	error = bus_dma_tag_create(busdma->bd_roottag, tag->align, tag->bndry,
	    tag->maxaddr, BUS_SPACE_MAXADDR, NULL, NULL, tag->maxsz,
	    tag->nsegs, tag->maxsegsz, 0, NULL, NULL, &md->bd_tag);
	if (error) {
		free(md, M_PROTO_BUSDMA);
		return (error);
	}
	error = bus_dmamem_alloc(md->bd_tag, &md->virtaddr, 0, &md->bd_map);
	if (error) {
		bus_dma_tag_destroy(md->bd_tag);
		free(md, M_PROTO_BUSDMA);
		return (error);
	}
	md->physaddr = pmap_kextract((uintptr_t)(md->virtaddr));
	LIST_INSERT_HEAD(&tag->mds, md, peers);
	LIST_INSERT_HEAD(&busdma->mds, md, mds);
	ioc->u.mem.nsegs = 1;
	ioc->u.mem.physaddr = md->physaddr;
	ioc->result = (uintptr_t)(void *)md;
	return (0);
}

static int
proto_busdma_mem_free(struct proto_busdma *busdma, struct proto_md *md)
{

	LIST_REMOVE(md, mds);
	LIST_REMOVE(md, peers);
	bus_dmamem_free(md->bd_tag, md->virtaddr, md->bd_map);
	bus_dma_tag_destroy(md->bd_tag);
	free(md, M_PROTO_BUSDMA);
	return (0);
}

static struct proto_md *
proto_busdma_md_lookup(struct proto_busdma *busdma, u_long key)
{
	struct proto_md *md;

	LIST_FOREACH(md, &busdma->mds, mds) {
		if ((void *)md == (void *)key)
			return (md);
	}
	return (NULL);
}

struct proto_busdma *
proto_busdma_attach(struct proto_softc *sc)
{
	struct proto_busdma *busdma;

	busdma = malloc(sizeof(*busdma), M_PROTO_BUSDMA, M_WAITOK | M_ZERO);
	return (busdma);
}

int
proto_busdma_detach(struct proto_softc *sc, struct proto_busdma *busdma)
{

	proto_busdma_cleanup(sc, busdma);
	free(busdma, M_PROTO_BUSDMA);
	return (0);
}

int
proto_busdma_cleanup(struct proto_softc *sc, struct proto_busdma *busdma)
{
	struct proto_md *md, *md1;
	struct proto_tag *tag, *tag1;

	LIST_FOREACH_SAFE(md, &busdma->mds, mds, md1)
		proto_busdma_mem_free(busdma, md);
	LIST_FOREACH_SAFE(tag, &busdma->tags, tags, tag1)
		proto_busdma_tag_destroy(busdma, tag);
	return (0);
}

int
proto_busdma_ioctl(struct proto_softc *sc, struct proto_busdma *busdma,
    struct proto_ioc_busdma *ioc)
{
	struct proto_tag *tag;
	struct proto_md *md;
	int error;

	error = 0;
	switch (ioc->request) {
	case PROTO_IOC_BUSDMA_TAG_CREATE:
		busdma->bd_roottag = bus_get_dma_tag(sc->sc_dev);
		error = proto_busdma_tag_create(busdma, NULL, ioc);
		break;
	case PROTO_IOC_BUSDMA_TAG_DERIVE:
		tag = proto_busdma_tag_lookup(busdma, ioc->key);
		if (tag == NULL) {
			error = EINVAL;
			break;
		}
		error = proto_busdma_tag_create(busdma, tag, ioc);
		break;
	case PROTO_IOC_BUSDMA_TAG_DESTROY:
		tag = proto_busdma_tag_lookup(busdma, ioc->key);
		if (tag == NULL) {
			error = EINVAL;
			break;
		}
		error = proto_busdma_tag_destroy(busdma, tag);
		break;
	case PROTO_IOC_BUSDMA_MEM_ALLOC:
		tag = proto_busdma_tag_lookup(busdma, ioc->u.mem.tag);
		if (tag == NULL) {
			error = EINVAL;
			break;
		}
		error = proto_busdma_mem_alloc(busdma, tag, ioc);
		break;
	case PROTO_IOC_BUSDMA_MEM_FREE:
		md = proto_busdma_md_lookup(busdma, ioc->key);
		if (md == NULL) {
			error = EINVAL;
			break;
		}
		error = proto_busdma_mem_free(busdma, md);
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

int
proto_busdma_mmap_allowed(struct proto_busdma *busdma, vm_paddr_t physaddr)
{
	struct proto_md *md;

	LIST_FOREACH(md, &busdma->mds, mds) {
		if (physaddr >= trunc_page(md->physaddr) &&
		    physaddr <= trunc_page(md->physaddr + md->tag->maxsz))
			return (1);
	}
	return (0);
}
