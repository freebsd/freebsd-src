/*
 * Copyright 2012 Michael Drake <tlsa@netsurf-browser.org>
 *
 * This file is part of libnsfb, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 *
 * This is the *internal* interface for the cursor. 
 */

#ifndef PALETTE_H
#define PALETTE_H 1

#include <stdint.h>
#include <limits.h>

#include "libnsfb.h"
#include "libnsfb_plot.h"

enum nsfb_palette_type_e {
	NSFB_PALETTE_EMPTY,     /**< empty palette object */
	NSFB_PALETTE_NSFB_8BPP, /**< libnsfb's own 8bpp palette */
	NSFB_PALETTE_OTHER      /**< any other palette  */
};

struct nsfb_palette_s {
	enum nsfb_palette_type_e type; /**< Palette type */
	uint8_t last; /**< Last used palette index */
	nsfb_colour_t data[256]; /**< Palette for index modes */

	bool dither; /**< Whether error diffusion was requested */
	struct {
		int width; /**< Length of error value buffer ring*/
		int current; /**< Current pos in ring buffer*/
		int *data; /**< Ring buffer error values */
		int data_len; /**< Max size of ring */
	} dither_ctx;
};


/** Create an empty palette object. */
bool nsfb_palette_new(struct nsfb_palette_s **palette, int width);

/** Free a palette object. */
void nsfb_palette_free(struct nsfb_palette_s *palette);

/** Init error diffusion for a plot. */
void nsfb_palette_dither_init(struct nsfb_palette_s *palette, int width);

/** Finalise error diffusion after a plot. */
void nsfb_palette_dither_fini(struct nsfb_palette_s *palette);

/** Generate libnsfb 8bpp default palette. */
void nsfb_palette_generate_nsfb_8bpp(struct nsfb_palette_s *palette);

static inline bool nsfb_palette_dithering_on(struct nsfb_palette_s *palette)
{
	return palette->dither;
}

/** Find best palette match for given colour. */
static inline uint8_t nsfb_palette_best_match(struct nsfb_palette_s *palette,
		nsfb_colour_t c, int *r_error, int *g_error, int *b_error)
{
	uint8_t best_col = 0;

	nsfb_colour_t palent;
	int col;
	int dr, dg, db; /* delta red, green blue values */

	int cur_distance;
	int best_distance = INT_MAX;

	int r = ( c        & 0xFF);
	int g = ((c >>  8) & 0xFF);
	int b = ((c >> 16) & 0xFF);

	switch (palette->type) {
	case NSFB_PALETTE_NSFB_8BPP:
		/* Index into colour cube part */
		dr = ((r * 5) + 128) / 256;
		dg = ((g * 7) + 128) / 256;
		db = ((b * 4) + 128) / 256;
		col = 40 * dr + 5 * dg + db;

		palent = palette->data[col];
		dr = r - ( palent        & 0xFF);
		dg = g - ((palent >>  8) & 0xFF);
		db = b - ((palent >> 16) & 0xFF);
		cur_distance = (dr * dr) + (dg * dg) + (db * db);

		best_col = col;
		best_distance = cur_distance;
		*r_error = dr;
		*g_error = dg;
		*b_error = db;

		/* Index into grayscale part */
		col = (r + g + b + (45 / 2)) / (15 * 3) - 1 + 240;
		palent = palette->data[col];

		dr = r - (palent & 0xFF);
		dg = g - (palent & 0xFF);
		db = b - (palent & 0xFF);
		cur_distance = (dr * dr) + (dg * dg) + (db * db);
		if (cur_distance < best_distance) {
			best_col = col;
			*r_error = dr;
			*g_error = dg;
			*b_error = db;
		}
		break;

	case NSFB_PALETTE_OTHER:
		/* Try all colours in palette */
		for (col = 0; col <= palette->last; col++) {
			palent = palette->data[col];

			dr = r - ( palent        & 0xFF);
			dg = g - ((palent >>  8) & 0xFF);
			db = b - ((palent >> 16) & 0xFF);
			cur_distance = (dr * dr) + (dg * dg) + (db * db);
			if (cur_distance < best_distance) {
				best_distance = cur_distance;
				best_col = col;
				*r_error = dr;
				*g_error = dg;
				*b_error = db;
			}
		}
		break;

	default:
		break;
	}

        return best_col;
}

/** Find best palette match for given colour, with error diffusion. */
static inline uint8_t nsfb_palette_best_match_dither(
		struct nsfb_palette_s *palette, nsfb_colour_t c)
{
	int r, g, b;
	int current;
	int error;
	int width = palette->dither_ctx.width;
	int *data = palette->dither_ctx.data;
	uint8_t best_col_index;

	if (palette == NULL)
		return 0;

	if (palette->dither == false)
		return nsfb_palette_best_match(palette, c, &r, &g, &b);

	current = palette->dither_ctx.current;

	/* Get RGB components of colour, and apply error */
	r = ( c        & 0xFF) + data[current    ];
	g = ((c >>  8) & 0xFF) + data[current + 1];
	b = ((c >> 16) & 0xFF) + data[current + 2];

	/* Clamp new RGB components to range */
	if (r <   0) r =   0;
	if (r > 255) r = 255;
	if (g <   0) g =   0;
	if (g > 255) g = 255;
	if (b <   0) b =   0;
	if (b > 255) b = 255;

	/* Reset error diffusion slots to 0 */
	data[current    ] = 0;
	data[current + 1] = 0;
	data[current + 2] = 0;

	/* Rebuild colour from modified components */
	c = r + (g << 8) + (b << 16);

	/* Get best match for pixel, and find errors for each component */
	best_col_index = nsfb_palette_best_match(palette, c, &r, &g, &b);

	/* Advance one set of error diffusion slots */
	current += 3;
	if (current >= width)
		current = 0;
	palette->dither_ctx.current = current;

	/* Save errors
	 *
	 *       [*]-[N]
	 *      / | \
	 *   [l]-[m]-[r]
	 */
	error = current;

	/* Error for [N] (next) */
	if (error != 0) {
		/* The pixel exists */
		data[error    ] += r * 7 / 16;
		data[error + 1] += g * 7 / 16;
		data[error + 2] += b * 7 / 16;
	}

	error += width - 2 * 3;
	if (error >= width)
		error -= width;
	/* Error for [l] (below, left) */
	if (error >= 0 && error != 3) {
		/* The pixel exists */
		data[error    ] += r * 3 / 16;
		data[error + 1] += g * 3 / 16;
		data[error + 2] += b * 3 / 16;
	}

	error += 3;
	if (error >= width)
		error -= width;
	/* Error for [m] (below, middle) */
	data[error    ] += r * 5 / 16;
	data[error + 1] += g * 5 / 16;
	data[error + 2] += b * 5 / 16;

	error += 3;
	if (error >= width)
		error -= width;
	/* Error for [r] (below, right) */
	if (error != 0) {
		/* The pixel exists */
		data[error    ] += r / 16;
		data[error + 1] += g / 16;
		data[error + 2] += b / 16;
	}

	return best_col_index;
}

#endif /* PALETTE_H */
