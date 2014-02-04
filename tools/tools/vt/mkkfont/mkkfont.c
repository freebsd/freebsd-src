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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct file_mapping {
	uint32_t	source;
	uint16_t	destination;
	uint16_t	length;
} __packed;

struct file_header {
	uint8_t		magic[8];
	uint8_t		width;
	uint8_t		height;
	uint16_t	nglyphs;
	uint16_t	nmappings_normal;
	uint16_t	nmappings_bold;
} __packed;

static int
print_glyphs(struct file_header *fh)
{
	unsigned int gbytes, nglyphs, j, k, total;
	uint8_t *gbuf;

	gbytes = howmany(fh->width, 8) * fh->height;
	nglyphs = be16toh(fh->nglyphs);

	printf("\nstatic uint8_t font_bytes[%u * %u] = {", nglyphs, gbytes);
	total = nglyphs * gbytes;
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

static int
print_mappings(struct file_header *fh, int bold)
{
	struct file_mapping fm;
	const char *name;
	unsigned int nmappings, i, col = 0;

	if (bold) {
		nmappings = be16toh(fh->nmappings_bold);
		name = "bold";
	} else {
		nmappings = be16toh(fh->nmappings_normal);
		name = "normal";
	}

	if (nmappings == 0)
		return (0);

	printf("\nstatic struct vt_font_map font_mapping_%s[%u] = {",
	    name, nmappings);

	for (i = 0; i < nmappings; i++) {
		if (fread(&fm, sizeof fm, 1, stdin) != 1) {
			perror("mapping");
			return (1);
		}

		printf(col == 0 ? "\n\t" : " ");
		printf("{ 0x%04x, 0x%04x, 0x%02x },",
		    be32toh(fm.source), be16toh(fm.destination),
		    be16toh(fm.length));
		col = (col + 1) % 2;
	}

	printf("\n};\n");

	return (0);
}

static int
print_info(struct file_header *fh)
{
	unsigned int nnormal, nbold;

	printf(
	    "\nstruct vt_font vt_font_default = {\n"
	    "\t.vf_width\t\t= %u,\n"
	    "\t.vf_height\t\t= %u,\n"
	    "\t.vf_bytes\t\t= font_bytes,\n",
	    fh->width, fh->height);

	nnormal = be16toh(fh->nmappings_normal);
	nbold = be16toh(fh->nmappings_bold);

	if (nnormal != 0)
		printf(
		    "\t.vf_normal\t\t= font_mapping_normal,\n"
		    "\t.vf_normal_length\t= %u,\n", nnormal);
	if (nbold != 0)
		printf(
		    "\t.vf_bold\t\t= font_mapping_bold,\n"
		    "\t.vf_bold_length\t\t= %u,\n", nbold);
	printf("\t.vf_refcount\t\t= 1,\n};\n");

	return (0);
}

int
main(int argc __unused, char *argv[] __unused)
{
	struct file_header fh;

	if (fread(&fh, sizeof fh, 1, stdin) != 1) {
		perror("file_header");
		return (1);
	}

	if (memcmp(fh.magic, "VFNT 1.0", 8) != 0) {
		fprintf(stderr, "Bad magic\n");
		return (1);
	}

	printf("#include <dev/vt/vt.h>\n");

	if (print_glyphs(&fh) != 0)
		return (1);
	if (print_mappings(&fh, 0) != 0)
		return (1);
	if (print_mappings(&fh, 1) != 0)
		return (1);
	if (print_info(&fh) != 0)
		return (1);

	return (0);
}
