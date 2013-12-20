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
#include <sys/malloc.h>
#include <sys/refcount.h>
#include <sys/systm.h>

#include <dev/vt/vt.h>

static MALLOC_DEFINE(M_VTFONT, "vtfont", "vt font");

/* Some limits to prevent abnormal fonts from being loaded. */
#define	VTFONT_MAXMAPPINGS	1024
#define	VTFONT_MAXGLYPHSIZE	262144
#define	VTFONT_MAXDIMENSION	128

static uint16_t
vtfont_bisearch(const struct vt_font_map *map, unsigned int len, uint32_t src)
{
	int min, mid, max;

	min = 0;
	max = len - 1;

	/* Empty font map. */
	if (len == 0)
		return (0);
	/* Character below minimal entry. */
	if (src < map[0].vfm_src)
		return (0);
	/* Optimization: ASCII characters occur very often. */
	if (src <= map[0].vfm_src + map[0].vfm_len)
		return (src - map[0].vfm_src + map[0].vfm_dst);
	/* Character above maximum entry. */
	if (src > map[max].vfm_src + map[max].vfm_len)
		return (0);

	/* Binary search. */
	while (max >= min) {
		mid = (min + max) / 2;
		if (src < map[mid].vfm_src)
			max = mid - 1;
		else if (src > map[mid].vfm_src + map[mid].vfm_len)
			min = mid + 1;
		else
			return (src - map[mid].vfm_src + map[mid].vfm_dst);
	}

	return (0);
}

const uint8_t *
vtfont_lookup(const struct vt_font *vf, term_char_t c)
{
	uint32_t src;
	uint16_t dst;
	size_t stride;

	/*
	 * No support for printing right hand sides for CJK fullwidth
	 * characters. Simply print a space and assume that the left
	 * hand side describes the entire character.
	 */
	src = TCHAR_CHARACTER(c);
	if (TCHAR_FORMAT(c) & TF_CJK_RIGHT)
		src = ' ';

	if (TCHAR_FORMAT(c) & TF_BOLD) {
		dst = vtfont_bisearch(vf->vf_bold, vf->vf_bold_length, src);
		if (dst != 0)
			goto found;
	}
	dst = vtfont_bisearch(vf->vf_normal, vf->vf_normal_length, src);

found:
	stride = howmany(vf->vf_width, 8) * vf->vf_height;
	return (&vf->vf_bytes[dst * stride]);
}

struct vt_font *
vtfont_ref(struct vt_font *vf)
{

	refcount_acquire(&vf->vf_refcount);
	return (vf);
}

void
vtfont_unref(struct vt_font *vf)
{

	if (refcount_release(&vf->vf_refcount)) {
		free(vf->vf_normal, M_VTFONT);
		free(vf->vf_bold, M_VTFONT);
		free(vf->vf_bytes, M_VTFONT);
		free(vf, M_VTFONT);
	}
}

static int
vtfont_validate_map(struct vt_font_map *vfm, unsigned int length,
    unsigned int nglyphs)
{
	unsigned int i, last = 0;

	for (i = 0; i < length; i++) {
		/* Not ordered. */
		if (i > 0 && vfm[i].vfm_src <= last)
			return (EINVAL);
		/*
		 * Destination extends amount of glyphs.
		 */
		if (vfm[i].vfm_dst >= nglyphs ||
		    vfm[i].vfm_dst + vfm[i].vfm_len >= nglyphs)
			return (EINVAL);
		last = vfm[i].vfm_src + vfm[i].vfm_len;
	}

	return (0);
}

int
vtfont_load(vfnt_t *f, struct vt_font **ret)
{
	size_t glyphsize;
	struct vt_font *vf;
	int error;

	/* Make sure the dimensions are valid. */
	if (f->width < 1 || f->height < 1)
		return (EINVAL);
	if (f->width > VTFONT_MAXDIMENSION || f->height > VTFONT_MAXDIMENSION)
		return (E2BIG);

	/* Not too many mappings. */
	if (f->nnormal > VTFONT_MAXMAPPINGS || f->nbold > VTFONT_MAXMAPPINGS)
		return (E2BIG);

	/* Character 0 must always be present. */
	if (f->nglyphs < 1)
		return (EINVAL);

	glyphsize = howmany(f->width, 8) * f->height * f->nglyphs;
	if (glyphsize > VTFONT_MAXGLYPHSIZE)
		return (E2BIG);

	/* Allocate new font structure. */
	vf = malloc(sizeof *vf, M_VTFONT, M_WAITOK);
	vf->vf_normal = malloc(f->nnormal * sizeof(struct vt_font_map),
	    M_VTFONT, M_WAITOK);
	vf->vf_bold = malloc(f->nbold * sizeof(struct vt_font_map),
	    M_VTFONT, M_WAITOK);
	vf->vf_bytes = malloc(glyphsize, M_VTFONT, M_WAITOK);
	vf->vf_height = f->height;
	vf->vf_width = f->width;
	vf->vf_normal_length = f->nnormal;
	vf->vf_bold_length = f->nbold;
	vf->vf_refcount = 1;

	/* Copy in data. */
	error = copyin(f->normal, vf->vf_normal,
	    vf->vf_normal_length * sizeof(struct vt_font_map));
	if (error)
		goto bad;
	error = copyin(f->bold, vf->vf_bold,
	    vf->vf_bold_length * sizeof(struct vt_font_map));
	if (error)
		goto bad;
	error = copyin(f->glyphs, vf->vf_bytes, glyphsize);
	if (error)
		goto bad;

	/* Validate mappings. */
	error = vtfont_validate_map(vf->vf_normal, vf->vf_normal_length,
	    f->nglyphs);
	if (error)
		goto bad;
	error = vtfont_validate_map(vf->vf_bold, vf->vf_bold_length,
	    f->nglyphs);
	if (error)
		goto bad;

	/* Success. */
	*ret = vf;
	return (0);

bad:	vtfont_unref(vf);
	return (error);
}
