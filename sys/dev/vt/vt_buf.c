/*-
 * Copyright (c) 2009 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Ed Schouten under sponsorship from the
 * FreeBSD Foundation.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include <dev/vt/vt.h>

static MALLOC_DEFINE(M_VTBUF, "vtbuf", "vt buffer");

#define	VTBUF_LOCK(vb)		mtx_lock_spin(&(vb)->vb_lock)
#define	VTBUF_UNLOCK(vb)	mtx_unlock_spin(&(vb)->vb_lock)

static inline uint64_t
vtbuf_dirty_axis(unsigned int begin, unsigned int end)
{
	uint64_t left, right, mask;

	/*
	 * Mark all bits between begin % 64 and end % 64 dirty.
	 * This code is functionally equivalent to:
	 *
	 * 	for (i = begin; i < end; i++)
	 * 		mask |= (uint64_t)1 << (i % 64);
	 */

	/* Obvious case. Mark everything dirty. */
	if (end - begin >= 64)
		return (VBM_DIRTY);

	/* 1....0; used bits on the left. */
	left = VBM_DIRTY << begin % 64;
	/* 0....1; used bits on the right. */
	right = VBM_DIRTY >> -end % 64;

	/*
	 * Only take the intersection.  If the result of that is 0, it
	 * means that the selection crossed a 64 bit boundary along the
	 * way, which means we have to take the complement.
	 */
	mask = left & right;
	if (mask == 0)
		mask = left | right;
	return (mask);
}

static inline void
vtbuf_dirty(struct vt_buf *vb, const term_rect_t *area)
{

	VTBUF_LOCK(vb);
	if (vb->vb_dirtyrect.tr_begin.tp_row > area->tr_begin.tp_row)
		vb->vb_dirtyrect.tr_begin.tp_row = area->tr_begin.tp_row;
	if (vb->vb_dirtyrect.tr_begin.tp_col > area->tr_begin.tp_col)
		vb->vb_dirtyrect.tr_begin.tp_col = area->tr_begin.tp_col;
	if (vb->vb_dirtyrect.tr_end.tp_row < area->tr_end.tp_row)
		vb->vb_dirtyrect.tr_end.tp_row = area->tr_end.tp_row;
	if (vb->vb_dirtyrect.tr_end.tp_col < area->tr_end.tp_col)
		vb->vb_dirtyrect.tr_end.tp_col = area->tr_end.tp_col;
	vb->vb_dirtymask.vbm_row |=
	    vtbuf_dirty_axis(area->tr_begin.tp_row, area->tr_end.tp_row);
	vb->vb_dirtymask.vbm_col |=
	    vtbuf_dirty_axis(area->tr_begin.tp_col, area->tr_end.tp_col);
	VTBUF_UNLOCK(vb);
}

static inline void
vtbuf_dirty_cell(struct vt_buf *vb, const term_pos_t *p)
{
	term_rect_t area;

	area.tr_begin = *p;
	area.tr_end.tp_row = p->tp_row + 1;
	area.tr_end.tp_col = p->tp_col + 1;
	vtbuf_dirty(vb, &area);
}

static void
vtbuf_make_undirty(struct vt_buf *vb)
{

	vb->vb_dirtyrect.tr_begin = vb->vb_size;
	vb->vb_dirtyrect.tr_end.tp_row = vb->vb_dirtyrect.tr_end.tp_col = 0;
	vb->vb_dirtymask.vbm_row = vb->vb_dirtymask.vbm_col = 0;
}

void
vtbuf_undirty(struct vt_buf *vb, term_rect_t *r, struct vt_bufmask *m)
{

	VTBUF_LOCK(vb);
	*r = vb->vb_dirtyrect;
	*m = vb->vb_dirtymask;
	vtbuf_make_undirty(vb);
	VTBUF_UNLOCK(vb);
}

void
vtbuf_copy(struct vt_buf *vb, const term_rect_t *r, const term_pos_t *p2)
{
	const term_pos_t *p1 = &r->tr_begin;
	term_rect_t area;
	unsigned int rows, cols;
	int pr;

	rows = r->tr_end.tp_row - r->tr_begin.tp_row;
	cols = r->tr_end.tp_col - r->tr_begin.tp_col;

	/* Handle overlapping copies. */
	if (p2->tp_row < p1->tp_row) {
		/* Move data up. */
		for (pr = 0; pr < rows; pr++)
			memmove(
			    &VTBUF_FIELD(vb, p2->tp_row + pr, p2->tp_col),
			    &VTBUF_FIELD(vb, p1->tp_row + pr, p1->tp_col),
			    cols * sizeof(term_char_t));
	} else {
		/* Move data down. */
		for (pr = rows - 1; pr >= 0; pr--)
			memmove(
			    &VTBUF_FIELD(vb, p2->tp_row + pr, p2->tp_col),
			    &VTBUF_FIELD(vb, p1->tp_row + pr, p1->tp_col),
			    cols * sizeof(term_char_t));
	}

	area.tr_begin = *p2;
	area.tr_end.tp_row = p2->tp_row + rows;
	area.tr_end.tp_col = p2->tp_col + cols;
	vtbuf_dirty(vb, &area);
}

void
vtbuf_fill(struct vt_buf *vb, const term_rect_t *r, term_char_t c)
{
	unsigned int pr, pc;

	for (pr = r->tr_begin.tp_row; pr < r->tr_end.tp_row; pr++)
		for (pc = r->tr_begin.tp_col; pc < r->tr_end.tp_col; pc++)
			VTBUF_FIELD(vb, pr, pc) = c;

	vtbuf_dirty(vb, r);
}

void
vtbuf_init_early(struct vt_buf *vb)
{

	vb->vb_flags |= VBF_CURSOR;
	vtbuf_make_undirty(vb);
	mtx_init(&vb->vb_lock, "vtbuf", NULL, MTX_SPIN);
}

void
vtbuf_init(struct vt_buf *vb, const term_pos_t *p)
{

	vb->vb_size = *p;
	vb->vb_buffer = malloc(p->tp_row * p->tp_col * sizeof(term_char_t),
	    M_VTBUF, M_WAITOK);
	vtbuf_init_early(vb);
}

void
vtbuf_grow(struct vt_buf *vb, const term_pos_t *p)
{
	term_char_t *old, *new;

	if (p->tp_row > vb->vb_size.tp_row ||
	    p->tp_col > vb->vb_size.tp_col) {
		/* Allocate new buffer. */
		new = malloc(p->tp_row * p->tp_col * sizeof(term_char_t),
		    M_VTBUF, M_WAITOK|M_ZERO);

		/* Toggle it. */
		VTBUF_LOCK(vb);
		old = vb->vb_flags & VBF_STATIC ? NULL : vb->vb_buffer;
		vb->vb_buffer = new;
		vb->vb_flags &= ~VBF_STATIC;
		vb->vb_size = *p;
		vtbuf_make_undirty(vb);
		VTBUF_UNLOCK(vb);

		/* Deallocate old buffer. */
		if (old != NULL)
			free(old, M_VTBUF);
	}
}

void
vtbuf_putchar(struct vt_buf *vb, const term_pos_t *p, term_char_t c)
{

	if (VTBUF_FIELD(vb, p->tp_row, p->tp_col) != c) {
		VTBUF_FIELD(vb, p->tp_row, p->tp_col) = c;
		vtbuf_dirty_cell(vb, p);
	}
}

void
vtbuf_cursor_position(struct vt_buf *vb, const term_pos_t *p)
{

	if (vb->vb_flags & VBF_CURSOR) {
		vtbuf_dirty_cell(vb, &vb->vb_cursor);
		vb->vb_cursor = *p;
		vtbuf_dirty_cell(vb, &vb->vb_cursor);
	} else {
		vb->vb_cursor = *p;
	}
}

void
vtbuf_cursor_visibility(struct vt_buf *vb, int yes)
{
	int oflags, nflags;

	VTBUF_LOCK(vb);
	oflags = vb->vb_flags;
	if (yes)
		vb->vb_flags |= VBF_CURSOR;
	else
		vb->vb_flags &= ~VBF_CURSOR;
	nflags = vb->vb_flags;
	VTBUF_UNLOCK(vb);

	if (oflags != nflags)
		vtbuf_dirty_cell(vb, &vb->vb_cursor);
}
