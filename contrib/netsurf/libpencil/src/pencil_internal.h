/*
 * This file is part of Pencil
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2005 James Bursa <james@semichrome.net>
 */

#ifndef PENCIL_INTERNAL_H
#define PENCIL_INTERNAL_H

#include <stdbool.h>
#include "pencil.h"


struct pencil_item;


struct pencil_diagram {
	struct pencil_item *root;
	struct pencil_item *current_group;
};

typedef enum {
	pencil_GROUP,
	pencil_TEXT,
	pencil_PATH,
	pencil_SPRITE,
} pencil_item_type;

struct pencil_item {
	pencil_item_type type;

	pencil_colour fill_colour;
	pencil_colour outline_colour;

	char *group_name;

	int x, y;

	const char *font_family;
	rufl_style font_style;
	unsigned int font_size;
	char *text;

	int *path;
	unsigned int path_size;
	int thickness;
	pencil_join join;
	pencil_cap start_cap;
	pencil_cap end_cap;
	int cap_width;
	int cap_length;
	bool even_odd;
	pencil_pattern pattern;

	int width, height;
	const void *sprite;

	struct {
		int x0;
		int y0;
		int x1;
		int y1;
	} bbox;

	struct pencil_item *parent;
	struct pencil_item *next;
	struct pencil_item *children;
	struct pencil_item *last;
};


#endif
