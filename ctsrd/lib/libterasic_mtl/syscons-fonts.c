/*-
 * Copyright (c) 1994-1996 Søren Schmidt
 * Copyright (c) 2012 SRI International
 * All rights reserved.
 *
 * Portions of this software are based in part on the work of
 * Sascha Wildner <saw@online.de> contributed to The DragonFly Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 * $DragonFly: src/usr.sbin/vidcontrol/vidcontrol.c,v 1.10 2005/03/02 06:08:29 joerg Exp $
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>
#include "path.h"
#include "decode.h"
#include "terasic_mtl.h"

#define	DATASIZE(x)	((x).w * (x).h * 256 / 8)

static char	*fontmap;
static const int font_width = 8;
static int	 font_height;

/*
 * Guess which file to open. Try to open each combination of a specified set
 * of file name components.
 */
static FILE *
openguess(const char *a[], const char *b[], const char *c[], const char *d[], char **name)
{
	FILE *f;
	int i, j, k, l;

	for (i = 0; a[i] != NULL; i++) {
		for (j = 0; b[j] != NULL; j++) {
			for (k = 0; c[k] != NULL; k++) {
				for (l = 0; d[l] != NULL; l++) {
					asprintf(name, "%s%s%s%s",
						 a[i], b[j], c[k], d[l]);

					f = fopen(*name, "r");

					if (f != NULL)
						return (f);

					free(*name);
				}
			}
		}
	}
	return (NULL);
}

/*
 * Determine a file's size.
 */
static int
fsize(FILE *file)
{
	struct stat sb;

	if (fstat(fileno(file), &sb) == 0)
		return sb.st_size;
	else
		return -1;
}


/*
 * Load a font from file and set it.
 */
void
fb_load_syscons_font(const char *type, const char *filename)
{
	FILE	*fd;
	int	h, i, size, w;
	char	*name;
	const char	*a[] = {"", FONT_PATH, NULL};
	const char	*b[] = {filename, NULL};
	const char	*c[] = {"", "16", NULL};
	const char	*d[] = {"", ".fnt", NULL};

	struct sizeinfo {
		int w;
		int h;
	} sizes[] = {{8, 16},
		     {8, 14},
		     {8,  8},
		     {0,  0}};

	fd = openguess(a, b, c, d, &name);

	if (fd == NULL)
		errx(1, "%s: can't load font file", filename);

	if (type != NULL) {
		size = 0;
		if (sscanf(type, "%dx%d", &w, &h) == 2) {
			for (i = 0; sizes[i].w != 0; i++) {
				if (sizes[i].w == w && sizes[i].h == h) {
					size = DATASIZE(sizes[i]);
					font_height = sizes[i].h;
				}
			}
		}
		if (size == 0) {
			fclose(fd);
			errx(1, "%s: bad font size specification", type);
		}
	} else {
		/* Apply heuristics */

		int j;
		int dsize[2];

		size = DATASIZE(sizes[0]);
		fontmap = (char*) malloc(size);
		dsize[0] = decode(fd, fontmap, size);
		dsize[1] = fsize(fd);
		free(fontmap);

		size = 0;
		for (j = 0; j < 2; j++) {
			for (i = 0; sizes[i].w != 0; i++) {
				if (DATASIZE(sizes[i]) == dsize[j]) {
					size = dsize[j];
					font_height = sizes[i].h;
					j = 2;	/* XXX */
					break;
				}
			}
		}

		if (size == 0) {
			fclose(fd);
			errx(1, "%s: can't guess font size", filename);
		}

		rewind(fd);
	}

	fontmap = (char*) malloc(size);

	if (decode(fd, fontmap, size) != size) {
		rewind(fd);
		if (fsize(fd) != size ||
		    fread(fontmap, 1, size, fd) != (size_t)size) {
			warnx("%s: bad font file", filename);
			fclose(fd);
			free(fontmap);
			errx(1, "%s: bad font file", filename);
		}
	}

	/*
	 * Reverse the bits so the font format so the bit offsets look like an
	 * array.  This should make a future conversion of the print code to
	 * something more complex easier.
	 */
	for (i = 0; i < size; i++) {
		fontmap[i] = (fontmap[i] & 0x0F) << 4 | (fontmap[i] & 0xF0) >> 4;
		fontmap[i] = (fontmap[i] & 0x33) << 2 | (fontmap[i] & 0xCC) >> 2;
		fontmap[i] = (fontmap[i] & 0x55) << 1 | (fontmap[i] & 0xAA) >> 1;
	}

	fclose(fd);
}

void
fb_render_text(const char *string, int expand, u_int32_t con, u_int32_t coff,
    u_int32_t *buffer, int w, int h)
{
	int col, fcol, frow, mcol, mrow, row, scol, vlen;
	char *vstr;

	vstr = malloc(strlen(string) * 4 + 1);
	if (vstr == NULL)
		err(1, "can't allocate vstr buffer");
	vlen = strvis(vstr, string, VIS_OCTAL);

	mcol = font_width * expand * vlen;
	if (font_width * expand * vlen > w)
		mcol = w;
	mrow = font_height * expand;
	if (mrow > h)
		mrow = h;

	for (row = 0; row < mrow; row++) {
		frow = row / expand;
		for (col = 0; col < mcol; col++) {
			fcol = (col % (font_width * expand)) / expand;
			scol = col / (font_width * expand);

			buffer[row * w + col] =
			    (fontmap[(u_int)vstr[scol] * font_height + frow] &
			    0x1 << fcol) ? con : coff;
		}
	}
}

int
fb_get_font_height(void)
{
	
	return (fontmap == NULL ? 0 : font_height);
}

int
fb_get_font_width(void)
{
	
	return (fontmap == NULL ? 0 : font_width);
}
