/*
 * This file is part of RUfl
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2006 James Bursa <james@semichrome.net>
 */

#ifndef RUFL_H
#define RUFL_H

#include <stdbool.h>
#include <stdlib.h>
#include "oslib/os.h"


/** Return code for RUfl functions. */
typedef enum {
	/** Success. */
	rufl_OK,
	/** Failure: memory was exhausted. */
	rufl_OUT_OF_MEMORY,
	/** Failure: Font Manager error; details in rufl_fm_error. */
	rufl_FONT_MANAGER_ERROR,
	/** Failure: no font with this name exists. */
	rufl_FONT_NOT_FOUND,
	/** Failure: file input / output error: details in errno. */
	rufl_IO_ERROR,
	/** Failure: file input unexpected eof. */
	rufl_IO_EOF,
} rufl_code;

/** Font weight and slant. Normal weight is 400, 700 gives the "Bold" weight of
 * fonts. */
typedef enum {
	rufl_WEIGHT_100 = 1,
	rufl_WEIGHT_200 = 2,
	rufl_WEIGHT_300 = 3,
	rufl_WEIGHT_400 = 4,
	rufl_WEIGHT_500 = 5,
	rufl_WEIGHT_600 = 6,
	rufl_WEIGHT_700 = 7,
	rufl_WEIGHT_800 = 8,
	rufl_WEIGHT_900 = 9,
	rufl_SLANTED = 0x100,
} rufl_style;

/** rufl_paint flags */
#define rufl_BLEND_FONT 0x01

/** Last Font Manager error. */
extern os_error *rufl_fm_error;

/** List of available font families. */
extern const char **rufl_family_list;
/** Number of entries in rufl_family_list. */
extern unsigned int rufl_family_list_entries;

/** Menu of font families. */
extern void *rufl_family_menu;

/* Callbacks used by rufl_decompose_glyph */
typedef int (*rufl_move_to_func)(os_coord *to, void *user);
typedef int (*rufl_line_to_func)(os_coord *to, void *user);
typedef int (*rufl_cubic_to_func)(os_coord *control1, os_coord *control2,
		os_coord *to, void *user);

struct rufl_decomp_funcs {
	rufl_move_to_func move_to;
	rufl_line_to_func line_to;
	rufl_cubic_to_func cubic_to;
};

/**
 * Initialise RUfl.
 *
 * All available fonts are scanned. May take some time.
 */

rufl_code rufl_init(void);


/**
 * Render Unicode text.
 */

rufl_code rufl_paint(const char *font_family, rufl_style font_style,
		unsigned int font_size,
		const char *string, size_t length,
		int x, int y, unsigned int flags);


/**
 * Measure the width of Unicode text.
 */

rufl_code rufl_width(const char *font_family, rufl_style font_style,
		unsigned int font_size,
		const char *string, size_t length,
		int *width);


/**
 * Find where in a string a x coordinate falls.
 */

rufl_code rufl_x_to_offset(const char *font_family, rufl_style font_style,
		unsigned int font_size,
		const char *string, size_t length,
		int click_x,
		size_t *char_offset, int *actual_x);


/**
 * Find the prefix of a string that will fit in a specified width.
 */

rufl_code rufl_split(const char *font_family, rufl_style font_style,
		unsigned int font_size,
		const char *string, size_t length,
		int width,
		size_t *char_offset, int *actual_x);


/** Type of callback function for rufl_paint_callback(). */
typedef void (*rufl_callback_t)(void *context,
		const char *font_name, unsigned int font_size,
		const char *s8, unsigned short *s16, unsigned int n,
		int x, int y);


/**
 * Render text, but call a callback instead of each call to Font_Paint.
 */

rufl_code rufl_paint_callback(const char *font_family, rufl_style font_style,
		unsigned int font_size,
		const char *string, size_t length,
		int x, int y,
		rufl_callback_t callback, void *context);


/**
 * Decompose a glyph to a path.
 */

rufl_code rufl_decompose_glyph(const char *font_family,
		rufl_style font_style, unsigned int font_size,
		const char *string, size_t length,
		struct rufl_decomp_funcs *funcs, void *user);


/**
 * Read metrics for a font
 */

rufl_code rufl_font_metrics(const char *font_family, rufl_style font_style,
		os_box *bbox, int *xkern, int *ykern, int *italic,
		int *ascent, int *descent,
		int *xheight, int *cap_height,
		signed char *uline_position, unsigned char *uline_thickness);


/**
 * Read metrics for a glyph
 */

rufl_code rufl_glyph_metrics(const char *font_family,
		rufl_style font_style, unsigned int font_size,
		const char *string, size_t length,
		int *x_bearing, int *y_bearing,
		int *width, int *height,
		int *x_advance, int *y_advance);


/**
 * Determine the maximum bounding box of a font.
 */

rufl_code rufl_font_bbox(const char *font_family, rufl_style font_style,
		unsigned int font_size,
		int *bbox);


/**
 * Dump the internal library state to stdout.
 */

void rufl_dump_state(void);


/**
 * Clear the internal font handle cache.
 *
 * Call this function on mode changes or output redirection changes.
 */

void rufl_invalidate_cache(void);


/**
 * Free all resources used by the library.
 */

void rufl_quit(void);


#endif
