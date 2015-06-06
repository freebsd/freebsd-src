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

#include <dev/proto/proto.h>
#include <dev/proto/proto_dev.h>
#include <dev/proto/proto_busdma.h>

MALLOC_DEFINE(M_PROTO_BUSDMA, "proto_busdma", "DMA management data");

static int
proto_busdma_tag_create(struct proto_ioc_busdma *ioc,
    struct proto_tag **tag_io, bus_dma_tag_t *busdma_tag_io)
{
	struct proto_tag *tag;
	int error;

	error = bus_dma_tag_create(*busdma_tag_io, ioc->u.tag.align,
	    ioc->u.tag.bndry, ioc->u.tag.maxaddr, BUS_SPACE_MAXADDR,
	    NULL, NULL, ioc->u.tag.maxsz, ioc->u.tag.nsegs,
	    ioc->u.tag.maxsegsz, ioc->u.tag.flags, NULL, NULL,
	    busdma_tag_io);
	if (error)
		return (error);

	tag = malloc(sizeof(*tag), M_PROTO_BUSDMA, M_WAITOK | M_ZERO);
	tag->parent = *tag_io;
	tag->busdma_tag = *busdma_tag_io;
	*tag_io = tag;
	return (0);
}

static void
proto_busdma_tag_destroy(struct proto_tag *tag)
{

	bus_dma_tag_destroy(tag->busdma_tag);
	free(tag, M_PROTO_BUSDMA);
}

static struct proto_tag *
proto_busdma_tag_lookup(struct proto_busdma *busdma, u_long key)
{
	struct proto_tag *tag;

        LIST_FOREACH(tag, &busdma->tags, link) {
		if ((void *)tag == (void *)key)
			return (tag);
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
	struct proto_tag *tag, *tag1;

	LIST_FOREACH_SAFE(tag, &busdma->tags, link, tag1) {
		LIST_REMOVE(tag, link);
		proto_busdma_tag_destroy(tag);
	}
	return (0);
}

int
proto_busdma_ioctl(struct proto_softc *sc, struct proto_busdma *busdma,
    struct proto_ioc_busdma *ioc)
{
	struct proto_tag *tag;
	bus_dma_tag_t busdma_tag;
	int error;

	error = 0;
	switch (ioc->request) {
	case PROTO_IOC_BUSDMA_TAG_CREATE:
		busdma_tag = bus_get_dma_tag(sc->sc_dev);
		tag = NULL;
		error = proto_busdma_tag_create(ioc, &tag, &busdma_tag);
		if (error)
			break;
		LIST_INSERT_HEAD(&busdma->tags, tag, link);
		ioc->key = (uintptr_t)(void *)tag;
		break;
	case PROTO_IOC_BUSDMA_TAG_DERIVE:
		tag = proto_busdma_tag_lookup(busdma, ioc->key);
		if (tag == NULL) {
			error = EINVAL;
			break;
		}
		busdma_tag = tag->busdma_tag;
		error = proto_busdma_tag_create(ioc, &tag, &busdma_tag);
		if (error)
			break;
		LIST_INSERT_HEAD(&busdma->tags, tag, link);
		ioc->key = (uintptr_t)(void *)tag;
		break;
	case PROTO_IOC_BUSDMA_TAG_DESTROY:
		tag = proto_busdma_tag_lookup(busdma, ioc->key);
		if (tag == NULL) {
			error = EINVAL;
			break;
		}
		LIST_REMOVE(tag, link);
		proto_busdma_tag_destroy(tag);
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}
