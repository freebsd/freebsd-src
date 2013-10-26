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

#include <sys/endian.h>
#include <sys/param.h>
#include <sys/queue.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int width, wbytes, height;

struct glyph {
	TAILQ_ENTRY(glyph)	 g_list;
	uint8_t			*g_data;
	unsigned int		 g_index;
};

static TAILQ_HEAD(, glyph) glyph_list = TAILQ_HEAD_INITIALIZER(glyph_list);
static unsigned int glyph_total, glyph_normal, glyph_bold,
    glyph_unique, glyph_dupe;

struct mapping {
	TAILQ_ENTRY(mapping)	 m_list;
	unsigned int		 m_char;
	unsigned int		 m_length;
	struct glyph		*m_glyph;
};

TAILQ_HEAD(mapping_list, mapping);
static struct mapping_list mapping_list_normal =
    TAILQ_HEAD_INITIALIZER(mapping_list_normal);
static struct mapping_list mapping_list_bold =
    TAILQ_HEAD_INITIALIZER(mapping_list_bold);
static unsigned int mapping_total, mapping_normal, mapping_normal_folded,
    mapping_bold, mapping_bold_folded, mapping_unique, mapping_dupe;

static void
usage(void)
{

	fprintf(stderr,
"usage: fontcvt width height normal.bdf bold.bdf out.fnt\n");
	exit(1);
}

static int
add_mapping(struct glyph *gl, unsigned int c, int bold)
{
	struct mapping *mp;
	struct mapping_list *ml;

	mapping_total++;

	if (bold) {
		int found = 0;

		TAILQ_FOREACH(mp, &mapping_list_normal, m_list) {
			if (mp->m_char < c)
				continue;
			else if (mp->m_char > c)
				break;
			found = 1;

			/*
			 * No mapping is needed if it's equal to the
			 * normal mapping.
			 */
			if (mp->m_glyph == gl) {
				mapping_dupe++;
				return (0);
			}
		}

		if (!found) {
			fprintf(stderr,
			    "Character %u not in normal font!\n", c);
			return (1);
		}
	}

	mp = malloc(sizeof *mp);
	mp->m_char = c;
	mp->m_glyph = gl;
	mp->m_length = 0;

	ml = bold ? &mapping_list_bold : &mapping_list_normal;
	if (TAILQ_LAST(ml, mapping_list) != NULL &&
	    TAILQ_LAST(ml, mapping_list)->m_char >= c) {
		fprintf(stderr, "Bad ordering at character %u\n", c);
		return (1);
	}
	TAILQ_INSERT_TAIL(ml, mp, m_list);

	if (bold)
		mapping_bold++;
	else
		mapping_normal++;
	mapping_unique++;

	return (0);
}

static struct glyph *
add_glyph(const uint8_t *bytes, int bold, int fallback)
{
	struct glyph *gl;

	glyph_total++;
	if (bold)
		glyph_bold++;
	else
		glyph_normal++;

	TAILQ_FOREACH(gl, &glyph_list, g_list) {
		if (memcmp(gl->g_data, bytes, wbytes * height) == 0) {
			glyph_dupe++;
			return (gl);
		}
	}

	gl = malloc(sizeof *gl);
	gl->g_data = malloc(wbytes * height);
	memcpy(gl->g_data, bytes, wbytes * height);
	if (fallback)
		TAILQ_INSERT_HEAD(&glyph_list, gl, g_list);
	else
		TAILQ_INSERT_TAIL(&glyph_list, gl, g_list);

	glyph_unique++;
	return (gl);
}

static int
parse_bdf(const char *filename, int bold __unused)
{
	FILE *fp;
	char *ln;
	size_t length;
	uint8_t bytes[wbytes * height];
	unsigned int curchar = 0, i, line;
	struct glyph *gl;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		perror(filename);
		return (1);
	}

	while ((ln = fgetln(fp, &length)) != NULL) {
		ln[length - 1] = '\0';

		if (strncmp(ln, "ENCODING ", 9) == 0) {
			curchar = atoi(ln + 9);
		}

		if (strcmp(ln, "BITMAP") == 0) {
			for (i = 0; i < height; i++) {
				if ((ln = fgetln(fp, &length)) == NULL) {
					fprintf(stderr, "Unexpected EOF!\n");
					return (1);
				}
				ln[length - 1] = '\0';
				sscanf(ln, "%x", &line);
				if (wbytes == 1) {
					bytes[i] = line;
				} else if (wbytes == 2) {
					bytes[i * 2 + 0] = line >> 8;
					bytes[i * 2 + 1] = line;
				} else {
					fprintf(stderr,
					    "Unsupported wbytes!\n");
					return (1);
				}
			}

			/* Prevent adding two glyphs for 0xFFFD */
			if (curchar == 0xFFFD) {
				if (!bold)
					gl = add_glyph(bytes, bold, 1);
			} else if (curchar >= 0x20) {
				gl = add_glyph(bytes, bold, 0);
				if (add_mapping(gl, curchar, bold) != 0)
					return (1);
			}
		}
	}

	return (0);
}

static void
number_glyphs(void)
{
	struct glyph *gl;
	unsigned int idx = 0;

	TAILQ_FOREACH(gl, &glyph_list, g_list)
		gl->g_index = idx++;
}

static void
write_glyphs(FILE *fp)
{
	struct glyph *gl;

	TAILQ_FOREACH(gl, &glyph_list, g_list)
		fwrite(gl->g_data, wbytes * height, 1, fp);
}

static void
fold_mappings(int bold)
{
	struct mapping_list *ml;
	struct mapping *mn, *mp, *mbase;

	if (bold)
		ml = &mapping_list_bold;
	else
		ml = &mapping_list_normal;

	mp = mbase = TAILQ_FIRST(ml);
	for (mp = mbase = TAILQ_FIRST(ml); mp != NULL; mp = mn) {
		mn = TAILQ_NEXT(mp, m_list);
		if (mn != NULL && mn->m_char == mp->m_char + 1 &&
		    mn->m_glyph->g_index == mp->m_glyph->g_index + 1)
			continue;
		mbase->m_length = mp->m_char - mbase->m_char + 1;
		mbase = mp = mn;
		if (bold)
			mapping_bold_folded++;
		else
			mapping_normal_folded++;
	}
}

struct file_mapping {
	uint32_t	source;
	uint16_t	destination;
	uint16_t	length;
} __packed;

static void
write_mappings(FILE *fp, int bold)
{
	struct mapping_list *ml;
	struct mapping *mp;
	struct file_mapping fm;
	unsigned int i = 0, j = 0;

	if (bold)
		ml = &mapping_list_bold;
	else
		ml = &mapping_list_normal;

	TAILQ_FOREACH(mp, ml, m_list) {
		j++;
		if (mp->m_length > 0) {
			i += mp->m_length;
			fm.source = htobe32(mp->m_char);
			fm.destination = htobe16(mp->m_glyph->g_index);
			fm.length = htobe16(mp->m_length - 1);
			fwrite(&fm, sizeof fm, 1, fp);
		}
	}
	assert(i == j);
}

struct file_header {
	uint8_t		magic[8];
	uint8_t		width;
	uint8_t		height;
	uint16_t	nglyphs;
	uint16_t	nmappings_normal;
	uint16_t	nmappings_bold;
} __packed;

static int
write_fnt(const char *filename)
{
	FILE *fp;
	struct file_header fh = {
		.magic = "VFNT 1.0",
	};

	fp = fopen(filename, "wb");
	if (fp == NULL) {
		perror(filename);
		return (1);
	}

	fh.width = width;
	fh.height = height;
	fh.nglyphs = htobe16(glyph_unique);
	fh.nmappings_normal = htobe16(mapping_normal_folded);
	fh.nmappings_bold = htobe16(mapping_bold_folded);
	fwrite(&fh, sizeof fh, 1, fp);
	
	write_glyphs(fp);
	write_mappings(fp, 0);
	write_mappings(fp, 1);

	return (0);
}

int
main(int argc, char *argv[])
{

	assert(sizeof(struct file_header) == 16);
	assert(sizeof(struct file_mapping) == 8);

	if (argc != 6)
		usage();
	
	width = atoi(argv[1]);
	wbytes = howmany(width, 8);
	height = atoi(argv[2]);

	if (parse_bdf(argv[3], 0) != 0)
		return (1);
	if (parse_bdf(argv[4], 1) != 0)
		return (1);
	number_glyphs();
	fold_mappings(0);
	fold_mappings(1);
	if (write_fnt(argv[5]) != 0)
		return (1);
	
	printf(
"Statistics:\n"
"- glyph_total:           %5u\n"
"- glyph_normal:          %5u\n"
"- glyph_bold:            %5u\n"
"- glyph_unique:          %5u\n"
"- glyph_dupe:            %5u\n"
"- mapping_total:         %5u\n"
"- mapping_normal:        %5u\n"
"- mapping_normal_folded: %5u\n"
"- mapping_bold:          %5u\n"
"- mapping_bold_folded:   %5u\n"
"- mapping_unique:        %5u\n"
"- mapping_dupe:          %5u\n",
	    glyph_total,
	    glyph_normal, glyph_bold,
	    glyph_unique, glyph_dupe,
	    mapping_total,
	    mapping_normal, mapping_normal_folded,
	    mapping_bold, mapping_bold_folded,
	    mapping_unique, mapping_dupe);
	
	return (0);
}
