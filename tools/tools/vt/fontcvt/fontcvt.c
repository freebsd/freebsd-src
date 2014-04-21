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

#define VFNT_MAPS 4
#define VFNT_MAP_NORMAL 0
#define VFNT_MAP_BOLD 2

static unsigned int width, wbytes, height;

struct glyph {
	TAILQ_ENTRY(glyph)	 g_list;
	uint8_t			*g_data;
	unsigned int		 g_index;
};

TAILQ_HEAD(glyph_list, glyph);
static struct glyph_list glyphs[VFNT_MAPS] = {
    TAILQ_HEAD_INITIALIZER(glyphs[0]),
    TAILQ_HEAD_INITIALIZER(glyphs[1]),
    TAILQ_HEAD_INITIALIZER(glyphs[2]),
    TAILQ_HEAD_INITIALIZER(glyphs[3]),
};
static unsigned int glyph_total, glyph_count[4], glyph_unique, glyph_dupe;

struct mapping {
	TAILQ_ENTRY(mapping)	 m_list;
	unsigned int		 m_char;
	unsigned int		 m_length;
	struct glyph		*m_glyph;
};

TAILQ_HEAD(mapping_list, mapping);
static struct mapping_list maps[VFNT_MAPS] = {
    TAILQ_HEAD_INITIALIZER(maps[0]),
    TAILQ_HEAD_INITIALIZER(maps[1]),
    TAILQ_HEAD_INITIALIZER(maps[2]),
    TAILQ_HEAD_INITIALIZER(maps[3]),
};
static unsigned int mapping_total, map_count[4], map_folded_count[4],
    mapping_unique, mapping_dupe;

static void
usage(void)
{

	fprintf(stderr,
"usage: fontcvt width height normal.bdf bold.bdf out.fnt\n");
	exit(1);
}

static int
add_mapping(struct glyph *gl, unsigned int c, unsigned int map_idx)
{
	struct mapping *mp;
	struct mapping_list *ml;

	mapping_total++;

	if (map_idx >= VFNT_MAP_BOLD) {
		int found = 0;
		unsigned normal_map_idx = map_idx - VFNT_MAP_BOLD;

		TAILQ_FOREACH(mp, &maps[normal_map_idx], m_list) {
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

	ml = &maps[map_idx];
	if (TAILQ_LAST(ml, mapping_list) != NULL &&
	    TAILQ_LAST(ml, mapping_list)->m_char >= c) {
		fprintf(stderr, "Bad ordering at character %u\n", c);
		return (1);
	}
	TAILQ_INSERT_TAIL(ml, mp, m_list);

	map_count[map_idx]++;
	mapping_unique++;

	return (0);
}

static struct glyph *
add_glyph(const uint8_t *bytes, unsigned int map_idx, int fallback)
{
	struct glyph *gl;
	unsigned int i;

	glyph_total++;
	glyph_count[map_idx]++;

	for (i = 0; i < VFNT_MAPS; i++) {
		TAILQ_FOREACH(gl, &glyphs[i], g_list) {
			if (memcmp(gl->g_data, bytes, wbytes * height) == 0) {
				glyph_dupe++;
				return (gl);
			}
		}
	}

	gl = malloc(sizeof *gl);
	gl->g_data = malloc(wbytes * height);
	memcpy(gl->g_data, bytes, wbytes * height);
	if (fallback)
		TAILQ_INSERT_HEAD(&glyphs[map_idx], gl, g_list);
	else
		TAILQ_INSERT_TAIL(&glyphs[map_idx], gl, g_list);

	glyph_unique++;
	return (gl);
}

static int
parse_bitmap_line(uint8_t *left, uint8_t *right, unsigned int line,
    unsigned int dwidth)
{
	uint8_t *p;
	unsigned int i, subline;

	if (dwidth != width && dwidth != width * 2) {
		fprintf(stderr,
		    "Unsupported width %u!\n", dwidth);
		return (1);
	}

	/* Move pixel data right to simplify splitting double characters. */
	line >>= (howmany(dwidth, 8) * 8) - dwidth;

	for (i = dwidth / width; i > 0; i--) {
		p = (i == 2) ? right : left;

		subline = line & ((1 << width) - 1);
		subline <<= (howmany(width, 8) * 8) - width;

		if (wbytes == 1) {
			*p = subline;
		} else if (wbytes == 2) {
			*p++ = subline >> 8;
			*p = subline;
		} else {
			fprintf(stderr,
			    "Unsupported wbytes %u!\n", wbytes);
			return (1);
		}

		line >>= width;
	}
	
	return (0);
}

static int
parse_bdf(const char *filename, unsigned int map_idx)
{
	FILE *fp;
	char *ln;
	size_t length;
	uint8_t bytes[wbytes * height], bytes_r[wbytes * height];
	unsigned int curchar = 0, dwidth = 0, i, line;
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

		if (strncmp(ln, "DWIDTH ", 7) == 0) {
			dwidth = atoi(ln + 7);
		}

		if (strcmp(ln, "BITMAP") == 0) {
			for (i = 0; i < height; i++) {
				if ((ln = fgetln(fp, &length)) == NULL) {
					fprintf(stderr, "Unexpected EOF!\n");
					return (1);
				}
				ln[length - 1] = '\0';
				sscanf(ln, "%x", &line);
				if (parse_bitmap_line(bytes + i * wbytes,
				     bytes_r + i * wbytes, line, dwidth) != 0)
					return (1);
			}

			/* Prevent adding two glyphs for 0xFFFD */
			if (curchar == 0xFFFD) {
				if (map_idx < VFNT_MAP_BOLD)
					gl = add_glyph(bytes, 0, 1);
			} else if (curchar >= 0x20) {
				gl = add_glyph(bytes, map_idx, 0);
				if (add_mapping(gl, curchar, map_idx) != 0)
					return (1);
				if (dwidth == width * 2) {
					gl = add_glyph(bytes_r, map_idx + 1, 0);
					if (add_mapping(gl, curchar,
					    map_idx + 1) != 0)
						return (1);
				}
			}
		}
	}

	return (0);
}

static void
number_glyphs(void)
{
	struct glyph *gl;
	unsigned int i, idx = 0;

	for (i = 0; i < VFNT_MAPS; i++)
		TAILQ_FOREACH(gl, &glyphs[i], g_list)
			gl->g_index = idx++;
}

static void
write_glyphs(FILE *fp)
{
	struct glyph *gl;
	unsigned int i;

	for (i = 0; i < VFNT_MAPS; i++) {
		TAILQ_FOREACH(gl, &glyphs[i], g_list)
			fwrite(gl->g_data, wbytes * height, 1, fp);
	}
}

static void
fold_mappings(unsigned int map_idx)
{
	struct mapping_list *ml = &maps[map_idx];
	struct mapping *mn, *mp, *mbase;

	mp = mbase = TAILQ_FIRST(ml);
	for (mp = mbase = TAILQ_FIRST(ml); mp != NULL; mp = mn) {
		mn = TAILQ_NEXT(mp, m_list);
		if (mn != NULL && mn->m_char == mp->m_char + 1 &&
		    mn->m_glyph->g_index == mp->m_glyph->g_index + 1)
			continue;
		mbase->m_length = mp->m_char - mbase->m_char + 1;
		mbase = mp = mn;
		map_folded_count[map_idx]++;
	}
}

struct file_mapping {
	uint32_t	source;
	uint16_t	destination;
	uint16_t	length;
} __packed;

static void
write_mappings(FILE *fp, unsigned int map_idx)
{
	struct mapping_list *ml = &maps[map_idx];
	struct mapping *mp;
	struct file_mapping fm;
	unsigned int i = 0, j = 0;

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
	uint16_t	pad;
	uint32_t	glyph_count;
	uint32_t	map_count[4];
} __packed;

static int
write_fnt(const char *filename)
{
	FILE *fp;
	struct file_header fh = {
		.magic = "VFNT0002",
	};

	fp = fopen(filename, "wb");
	if (fp == NULL) {
		perror(filename);
		return (1);
	}

	fh.width = width;
	fh.height = height;
	fh.glyph_count = htobe32(glyph_unique);
	fh.map_count[0] = htobe32(map_folded_count[0]);
	fh.map_count[1] = htobe32(map_folded_count[1]);
	fh.map_count[2] = htobe32(map_folded_count[2]);
	fh.map_count[3] = htobe32(map_folded_count[3]);
	fwrite(&fh, sizeof fh, 1, fp);
	
	write_glyphs(fp);
	write_mappings(fp, VFNT_MAP_NORMAL);
	write_mappings(fp, 1);
	write_mappings(fp, VFNT_MAP_BOLD);
	write_mappings(fp, 3);

	return (0);
}

int
main(int argc, char *argv[])
{

	assert(sizeof(struct file_header) == 32);
	assert(sizeof(struct file_mapping) == 8);

	if (argc != 6)
		usage();
	
	width = atoi(argv[1]);
	wbytes = howmany(width, 8);
	height = atoi(argv[2]);

	if (parse_bdf(argv[3], VFNT_MAP_NORMAL) != 0)
		return (1);
	if (parse_bdf(argv[4], VFNT_MAP_BOLD) != 0)
		return (1);
	number_glyphs();
	fold_mappings(0);
	fold_mappings(1);
	fold_mappings(2);
	fold_mappings(3);
	if (write_fnt(argv[5]) != 0)
		return (1);
	
	printf(
"Statistics:\n"
"- glyph_total:                 %5u\n"
"- glyph_normal:                %5u\n"
"- glyph_normal_right:          %5u\n"
"- glyph_bold:                  %5u\n"
"- glyph_bold_right:            %5u\n"
"- glyph_unique:                %5u\n"
"- glyph_dupe:                  %5u\n"
"- mapping_total:               %5u\n"
"- mapping_normal:              %5u\n"
"- mapping_normal_folded:       %5u\n"
"- mapping_normal_right:        %5u\n"
"- mapping_normal_right_folded: %5u\n"
"- mapping_bold:                %5u\n"
"- mapping_bold_folded:         %5u\n"
"- mapping_bold_right:          %5u\n"
"- mapping_bold_right_folded:   %5u\n"
"- mapping_unique:              %5u\n"
"- mapping_dupe:                %5u\n",
	    glyph_total,
	    glyph_count[0],
	    glyph_count[1],
	    glyph_count[2],
	    glyph_count[3],
	    glyph_unique, glyph_dupe,
	    mapping_total,
	    map_count[0], map_folded_count[0],
	    map_count[1], map_folded_count[1],
	    map_count[2], map_folded_count[2],
	    map_count[3], map_folded_count[3],
	    mapping_unique, mapping_dupe);
	
	return (0);
}
