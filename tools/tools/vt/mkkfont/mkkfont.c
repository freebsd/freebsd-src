/*-
 * Copyright (c) 2009 The FreeBSD Foundation
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
#include <sys/endian.h>
#include <sys/param.h>
#include <sys/font.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
print_glyphs(struct font_header *fh)
{
	unsigned int gbytes, glyph_count, j, k, total;
	uint8_t *gbuf;

	gbytes = howmany(fh->fh_width, 8) * fh->fh_height;
	glyph_count = be32toh(fh->fh_glyph_count);

	printf("\nstatic uint8_t font_bytes[%u * %u] = {", glyph_count, gbytes);
	total = glyph_count * gbytes;
	gbuf = malloc(total);

	if (fread(gbuf, total, 1, stdin) != 1) {
		perror("glyph");
		return (1);
	}

	for (j = 0; j < total; j += 12) {
		for (k = 0; k < 12 && k < total - j; k++) {
			printf(k == 0 ? "\n\t" : " ");
			printf("0x%02hhx,", gbuf[j + k]);
		}
	}

	free(gbuf);
	printf("\n};\n");

	return (0);
}

static const char *map_names[4] = {
    "normal", "normal_right", "bold", "bold_right" };

static int
print_mappings(struct font_header *fh, int map_index)
{
	vfnt_map_t fm;
	unsigned int nmappings, i, col = 0;

	
	nmappings = be32toh(fh->fh_map_count[map_index]);

	if (nmappings == 0)
		return (0);

	printf("\nstatic vfnt_map_t font_mapping_%s[%u] = {",
	    map_names[map_index], nmappings);

	for (i = 0; i < nmappings; i++) {
		if (fread(&fm, sizeof fm, 1, stdin) != 1) {
			perror("mapping");
			return (1);
		}

		printf(col == 0 ? "\n\t" : " ");
		printf("{ 0x%04x, 0x%04x, 0x%02x },",
		    be32toh(fm.vfm_src), be16toh(fm.vfm_dst),
		    be16toh(fm.vfm_len));
		col = (col + 1) % 2;
	}

	printf("\n};\n");

	return (0);
}

static int
print_info(struct font_header *fh)
{
	unsigned int i;

	printf(
	    "\nstruct vt_font vt_font_default = {\n"
	    "\t.vf_width\t\t= %u,\n"
	    "\t.vf_height\t\t= %u,\n"
	    "\t.vf_bytes\t\t= font_bytes,\n",
	    fh->fh_width, fh->fh_height);

	printf("\t.vf_map\t\t\t= {\n");
	for (i = 0; i < 4; i++) {
		if (fh->fh_map_count[i] > 0)
			printf("\t\t\t\t    font_mapping_%s,\n", map_names[i]);
		else
			printf("\t\t\t\t    NULL,\n");
	}
	printf("\t\t\t\t  },\n");
	printf("\t.vf_map_count\t\t= { %u, %u, %u, %u },\n",
	    be32toh(fh->fh_map_count[0]),
	    be32toh(fh->fh_map_count[1]),
	    be32toh(fh->fh_map_count[2]),
	    be32toh(fh->fh_map_count[3]));
	printf("\t.vf_refcount\t\t= 1,\n};\n");

	return (0);
}

int
main(int argc __unused, char *argv[] __unused)
{
	struct font_header fh;
	unsigned int i;

	if (fread(&fh, sizeof fh, 1, stdin) != 1) {
		perror("font_header");
		return (1);
	}

	if (memcmp(fh.fh_magic, "VFNT0002", 8) != 0) {
		fprintf(stderr, "Bad magic\n");
		return (1);
	}

	printf("#include <dev/vt/vt.h>\n");

	if (print_glyphs(&fh) != 0)
		return (1);
	for (i = 0; i < 4; i++)
		if (print_mappings(&fh, i) != 0)
			return (1);
	if (print_info(&fh) != 0)
		return (1);

	return (0);
}
