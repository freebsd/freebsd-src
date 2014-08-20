/*
 * This file is part of Pencil
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2005 James Bursa <james@semichrome.net>
 */

#ifndef PENCIL_H
#define PENCIL_H

#include <stdbool.h>
#include <stdint.h>
#include "rufl.h"


struct pencil_diagram;


typedef enum {
	pencil_OK,
	pencil_OUT_OF_MEMORY = rufl_OUT_OF_MEMORY,
	pencil_FONT_MANAGER_ERROR = rufl_FONT_MANAGER_ERROR,
	pencil_FONT_NOT_FOUND = rufl_FONT_NOT_FOUND,
	pencil_IO_ERROR = rufl_IO_ERROR,
	pencil_IO_EOF = rufl_IO_EOF,
} pencil_code;

/** A colour as 0xBBGGRR00. */
typedef uint32_t pencil_colour;

#define pencil_TRANSPARENT 0xffffffff

typedef enum {
	pencil_JOIN_MITRED,
	pencil_JOIN_ROUND,
	pencil_JOIN_BEVELLED,
} pencil_join;

typedef enum {
	pencil_CAP_BUTT,
	pencil_CAP_ROUND,
	pencil_CAP_SQUARE,
	pencil_CAP_TRIANGLE,
} pencil_cap;

typedef enum {
	pencil_SOLID,
	pencil_DOTTED,
	pencil_DASHED,
} pencil_pattern;


struct pencil_diagram *pencil_create(void);

pencil_code pencil_text(struct pencil_diagram *diagram,
		int x, int y,
		const char *font_family, rufl_style font_style,
		unsigned int font_size,
		const char *string, size_t length,
		pencil_colour colour);
pencil_code pencil_path(struct pencil_diagram *diagram,
		const int *path, unsigned int n,
		pencil_colour fill_colour, pencil_colour outline_colour,
		int thickness, pencil_join join,
		pencil_cap start_cap, pencil_cap end_cap,
		int cap_width, int cap_length,
		bool even_odd, pencil_pattern pattern);
pencil_code pencil_sprite(struct pencil_diagram *diagram,
		int x, int y, int width, int height,
		const char *sprite);

pencil_code pencil_group_start(struct pencil_diagram *diagram,
		const char *name);
pencil_code pencil_group_end(struct pencil_diagram *diagram);

pencil_code pencil_clip_start(struct pencil_diagram *diagram,
		int x0, int y0, int x1, int y1);
pencil_code pencil_clip_end(struct pencil_diagram *diagram);

pencil_code pencil_save_drawfile(struct pencil_diagram *diagram,
		const char *source,
		char **drawfile_buffer, size_t *drawfile_size);

void pencil_free(struct pencil_diagram *diagram);

void pencil_dump(struct pencil_diagram *diagram);


#endif
