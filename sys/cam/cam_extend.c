/*
 * Written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * $FreeBSD: src/sys/cam/cam_extend.c,v 1.3 1999/08/28 00:40:39 peter Exp $
 */
/*
 * XXX XXX XXX XXX  We should get DEVFS working so that we
 * don't have to do this, possibly sparse, array based junk.
 * XXX: We can do this now with dev_t, that's even better.
 */
/*
 * Extensible arrays: Use a realloc like implementation to permit
 * the arrays to be extend.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <cam/cam_extend.h>

struct extend_array
{
	int nelem;
	void **ps;
};

static void *
cam_extend_alloc(size_t s)
{
	void *p = malloc(s, M_DEVBUF, M_NOWAIT);
	if (!p)
		panic("extend_alloc: malloc failed.");
	return p;
}

static void
cam_extend_free(void *p)
{
	free(p, M_DEVBUF);
}

/* EXTEND_CHUNK: Number of extend slots to allocate whenever we need a new
 * one.
 */
#ifndef EXTEND_CHUNK
	#define EXTEND_CHUNK 8
#endif

struct extend_array *
cam_extend_new(void)
{
	struct extend_array *p = cam_extend_alloc(sizeof(*p));
	if (p) {
		p->nelem = 0;
		p->ps = 0;
	}

	return p;
}

void *
cam_extend_set(struct extend_array *ea, int index, void *value)
{
	if (index >= ea->nelem) {
		void **space;
		space = cam_extend_alloc(sizeof(void *) * (index + EXTEND_CHUNK));
		bzero(space, sizeof(void *) * (index + EXTEND_CHUNK));

		/* Make sure we have something to copy before we copy it */
		if (ea->nelem) {
			bcopy(ea->ps, space, sizeof(void *) * ea->nelem);
			cam_extend_free(ea->ps);
		}

		ea->ps = space;
		ea->nelem = index + EXTEND_CHUNK;
	}
	if (ea->ps[index]) {
		printf("extend_set: entry %d already has storage.\n", index);
		return 0;
	}
	else
		ea->ps[index] = value;

	return value;
}

void *
cam_extend_get(struct extend_array *ea,
	       int index)
{
	if (ea == NULL || index >= ea->nelem || index < 0)
		return NULL;
	return ea->ps[index];
}

void
cam_extend_release(struct extend_array *ea, int index)
{
	void *p = cam_extend_get(ea, index);
	if (p) {
		ea->ps[index] = 0;
	}
}
