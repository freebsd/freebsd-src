/*
 * This file is part of Pencil
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2005 James Bursa <james@semichrome.net>
 */

/** \file
 * Saving as a DrawFile (implementation).
 *
 * Two passes over the diagram tree are made. The first pass computes the size
 * that will be required and enumerates the fonts. The second pass creates the
 * DrawFile in a buffer.
 */

#define _GNU_SOURCE  /* for strndup */
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <oslib/drawfile.h>
#include <oslib/osspriteop.h>
#include <rufl.h>
#include "pencil_internal.h"

/* Maximum grouping depth (too deep crashes Draw). */
#define MAX_DEPTH 10


struct pencil_save_context {
	pencil_code code;
	struct pencil_diagram *diagram;
	size_t size;
	char **font_list;
	unsigned int font_count;
	struct pencil_item *item;
	void *buffer;
	void *b;
	os_box bbox;
};


static void pencil_save_pass1(struct pencil_save_context *context,
		struct pencil_item *item, unsigned int depth);
static void pencil_save_pass1_text_callback(void *c,
		const char *font_name, unsigned int font_size,
		const char *s8, unsigned short *s16, unsigned int n,
		int x, int y);
static void pencil_save_pass2(struct pencil_save_context *context,
		struct pencil_item *item, unsigned int depth);
static void pencil_save_pass2_text_callback(void *c,
		const char *font_name, unsigned int font_size,
		const char *s8, unsigned short *s16, unsigned int n,
		int x, int y);


pencil_code pencil_save_drawfile(struct pencil_diagram *diagram,
		const char *source,
		char **drawfile_buffer, size_t *drawfile_size)
{
	struct pencil_save_context context =
			{ pencil_OK, diagram, 0, 0, 0, 0, 0, 0, 
			  { INT_MAX, INT_MAX, INT_MIN, INT_MIN } };
	unsigned int i;
	size_t size, font_table_size;
	void *buffer, *b;
	drawfile_diagram *header;
	drawfile_object *font_table;
	char *f;

	*drawfile_buffer = 0;
	*drawfile_size = 0;

	/* pass 1 */
	pencil_save_pass1(&context, diagram->root, 0);
	if (context.code != pencil_OK) {
		for (i = 0; i != context.font_count; i++)
			free(context.font_list[i]);
		free(context.font_list);
		return context.code;
	}

	/* find font table size */
	font_table_size = 8;
	for (i = 0; i != context.font_count; i++)
		font_table_size += 1 + strlen(context.font_list[i]) + 1;
	font_table_size = (font_table_size + 3) & ~3;

	size = 40 + font_table_size + context.size;

	/* use calloc to prevent information leakage */
	buffer = calloc(size, 1);
	if (!buffer) {
		for (i = 0; i != context.font_count; i++)
			free(context.font_list[i]);
		free(context.font_list);
		return pencil_OUT_OF_MEMORY;
	}

	/* file headers */
	header = (drawfile_diagram *) buffer;
	header->tag[0] = 'D';
	header->tag[1] = 'r';
	header->tag[2] = 'a';
	header->tag[3] = 'w';
	header->major_version = 201;
	header->minor_version = 0;
	strncpy(header->source, source, 12);
	for (i = strlen(source); i < 12; i++)
		header->source[i] = ' ';
	header->bbox = context.bbox;
	b = (char *) buffer + sizeof(drawfile_diagram_base);

	/* font table */
	font_table = (drawfile_object *) b;
	font_table->type = drawfile_TYPE_FONT_TABLE;
	font_table->size = font_table_size;
	f = (char *) b + 8;
	for (i = 0; i != context.font_count; i++) {
		*f++ = i + 1;
		strcpy(f, context.font_list[i]);
		f += strlen(context.font_list[i]) + 1;
	}
	b = (char *) b + font_table_size;

	/* pass 2 */
	context.buffer = buffer;
	context.b = b;
	pencil_save_pass2(&context, diagram->root, 0);
	
	/* free font list */
	for (i = 0; i != context.font_count; i++)
		free(context.font_list[i]);
	free(context.font_list);

	if (context.code != pencil_OK) {
		free(buffer);
		return context.code;
	}

	assert(context.b == buffer + size);

	*drawfile_buffer = buffer;
	*drawfile_size = size;
	return pencil_OK;
}


void pencil_save_pass1(struct pencil_save_context *context,
		struct pencil_item *item, unsigned int depth)
{
	rufl_code code;
	struct pencil_item *child;

	assert(item);

	/* Initialise item bounding box */
	item->bbox.x0 = INT_MAX;
	item->bbox.y0 = INT_MAX;
	item->bbox.x1 = INT_MIN;
	item->bbox.y1 = INT_MIN;

	for (child = item->children; child; child = child->next) {
		pencil_save_pass1(context, child, depth + 1);
		if (context->code != pencil_OK)
			return;

		/* Update item bounding box to include child */
		if (child->bbox.x0 < item->bbox.x0)
			item->bbox.x0 = child->bbox.x0;
		if (child->bbox.y0 < item->bbox.y0)
			item->bbox.y0 = child->bbox.y0;
		if (child->bbox.x1 > item->bbox.x1)
			item->bbox.x1 = child->bbox.x1;
		if (child->bbox.y1 > item->bbox.y1)
			item->bbox.y1 = child->bbox.y1;
	}

	switch (item->type) {
	case pencil_GROUP:
		if (!item->children || MAX_DEPTH <= depth ||
				(item->bbox.x0 == INT_MAX && 
				item->bbox.y0 == INT_MAX &&
				item->bbox.x1 == INT_MIN &&
				item->bbox.y1 == INT_MIN))
			break;

		context->size += 36;

		break;
	case pencil_TEXT:
	{
		int bbox[4];
		int width;

		code = rufl_paint_callback(item->font_family, item->font_style,
				item->font_size, item->text, strlen(item->text),
				item->x, item->y,
				pencil_save_pass1_text_callback, context);
		if (code != rufl_OK)
			context->code = code;
		if (context->code != pencil_OK)
			return;

		code = rufl_font_bbox(item->font_family, item->font_style,
				item->font_size, bbox);
		if (code != rufl_OK)
			context->code = code;
		if (context->code != pencil_OK)
			return;

		code = rufl_width(item->font_family, item->font_style,
				item->font_size, item->text, strlen(item->text),
				&width);
		if (code != rufl_OK)
			context->code = code;
		if (context->code != pencil_OK)
			return;

		item->bbox.x0 = item->x * 256;
		item->bbox.y0 = item->y * 256;
		item->bbox.x1 = (item->x + width) * 256;
		item->bbox.y1 = (item->y + (bbox[3] - bbox[1])) * 256;
	}
		break;
	case pencil_PATH:
		context->size += 24 + 16 + item->path_size * 4;
		if (item->pattern != pencil_SOLID)
			context->size += 12;

		/* Calculate bounding box */
		for (unsigned int i = 0; i != item->path_size; ) {
			switch (item->path[i]) {
			case 0:
			case 5:
				i++;
				break;
			case 2:
			case 6:
			case 8:
			{
				int points = item->path[i++] == 6 ? 3 : 1;

				for (; points > 0; points--) {
					int x = item->path[i++] * 256;
					int y = item->path[i++] * 256;

					if (x < item->bbox.x0)
						item->bbox.x0 = x;
					if (y < item->bbox.y0)
						item->bbox.y0 = y;
					if (x > item->bbox.x1)
						item->bbox.x1 = x;
					if (y > item->bbox.y1)
						item->bbox.y1 = y;
				}
			}
				break;
			default:
				assert(0);
			}
		}
		break;
	case pencil_SPRITE:
		context->size += 24 + ((const osspriteop_header *)
				item->sprite)->size;

		item->bbox.x0 = item->x * 256;
		item->bbox.y0 = item->y * 256;
		item->bbox.x1 = (item->x + item->width) * 256;
		item->bbox.y1 = (item->y + item->height) * 256;
		break;
	default:
		assert(0);
	}

	/* Update global bounding box */
	if (item->bbox.x0 < context->bbox.x0)
		context->bbox.x0 = item->bbox.x0;
	if (item->bbox.y0 < context->bbox.y0)
		context->bbox.y0 = item->bbox.y0;
	if (item->bbox.x1 > context->bbox.x1)
		context->bbox.x1 = item->bbox.x1;
	if (item->bbox.y1 > context->bbox.y1)
		context->bbox.y1 = item->bbox.y1;
}


void pencil_save_pass1_text_callback(void *c,
		const char *font_name, unsigned int font_size,
		const char *s8, unsigned short *s16, unsigned int n,
		int x, int y)
{
	struct pencil_save_context *context = c;
	unsigned int i;
	char **font_list;

	(void) font_size;  /* unused */
	(void) x;  /* unused */
	(void) y;  /* unused */

	assert(s8 || s16);

	/* check if the font name is new */
	for (i = 0; i != context->font_count &&
			strcmp(context->font_list[i], font_name) != 0; i++)
		;
	if (i == context->font_count) {
		/* add to list of fonts */
		font_list = realloc(context->font_list,
				sizeof context->font_list[0] *
				(context->font_count + 1));
		if (!font_list) {
			context->code = pencil_OUT_OF_MEMORY;
			return;
		}
		font_list[context->font_count] = strdup(font_name);
		if (!font_list[context->font_count]) {
			context->code = pencil_OUT_OF_MEMORY;
			return;
		}
		context->font_list = font_list;
		context->font_count++;
	}

	/* compute size of transformed text object */
	if (s8) {
		context->size += 24 + 56 + ((n + 4) & ~3);
	} else {
		unsigned int utf8_length = 0;
		for (i = 0; i != n; i++) {
			if (s16[i] < 0x80)
				utf8_length += 1;
			else if (s16[i] < 0x800)
				utf8_length += 2;
			else
				utf8_length += 3;
		}
		context->size += 24 + 56 + ((utf8_length + 4) & ~3);
	}
}


void pencil_save_pass2(struct pencil_save_context *context,
		struct pencil_item *item, unsigned int depth)
{
	drawfile_object *object = (drawfile_object *) context->b;
	bool group = false;
	rufl_code code;
	int *path;
	unsigned int i;
	struct pencil_item *child;

	assert(item);

	switch (item->type) {
	case pencil_GROUP:
		if (!item->children || MAX_DEPTH <= depth ||
				(item->bbox.x0 == INT_MAX && 
				item->bbox.y0 == INT_MAX &&
				item->bbox.x1 == INT_MIN &&
				item->bbox.y1 == INT_MIN))
			break;

		group = true;
		object->type = drawfile_TYPE_GROUP;
		object->size = 36;
		strncpy(object->data.group.name, item->group_name, 12);
		for (i = strlen(item->group_name); i < 12; i++)
			object->data.group.name[i] = ' ';
		object->data.group.bbox.x0 = item->bbox.x0;
		object->data.group.bbox.y0 = item->bbox.y0;
		object->data.group.bbox.x1 = item->bbox.x1;
		object->data.group.bbox.y1 = item->bbox.y1;
		context->b = (char *) context->b + object->size;
		break;
	case pencil_TEXT:
		context->item = item;
		code = rufl_paint_callback(item->font_family, item->font_style,
				item->font_size, item->text, strlen(item->text),
				item->x, item->y,
				pencil_save_pass2_text_callback, context);
		if (code != rufl_OK)
			context->code = code;
		if (context->code != pencil_OK)
			return;
		break;
	case pencil_PATH:
		object->type = drawfile_TYPE_PATH;
		object->size = 24 + 16 + item->path_size * 4;
		object->data.path.bbox.x0 = item->bbox.x0;
		object->data.path.bbox.y0 = item->bbox.y0;
		object->data.path.bbox.x1 = item->bbox.x1;
		object->data.path.bbox.y1 = item->bbox.y1;
		object->data.path.fill = item->fill_colour;
		object->data.path.outline = item->outline_colour;
		object->data.path.width = item->thickness * 256;
		object->data.path.style.flags = 0;
		object->data.path.style.cap_width = item->cap_width;
		object->data.path.style.cap_length = item->cap_length;
		if (item->pattern != pencil_SOLID) {
			object->size += 12;
			object->data.path_with_pattern.pattern.start = 0;
			object->data.path_with_pattern.pattern.
					element_count = 1;
			if (item->pattern != pencil_DOTTED)
				object->data.path_with_pattern.pattern.
					elements[0] = 512 * item->thickness;
			else if (item->pattern != pencil_DASHED)
				object->data.path_with_pattern.pattern.
					elements[0] = 1536 * item->thickness;
		}
		path = (int *) (void *) ((char *) context->b + object->size -
				item->path_size * 4);
		for (i = 0; i != item->path_size; ) {
			switch (item->path[i]) {
			case 0:
			case 5:
				path[i] = item->path[i]; i++;
				break;
			case 2:
			case 8:
				path[i] = item->path[i]; i++;
				path[i] = item->path[i] * 256; i++;
				path[i] = item->path[i] * 256; i++;
				break;
			case 6:
				path[i] = item->path[i]; i++;
				path[i] = item->path[i] * 256; i++;
				path[i] = item->path[i] * 256; i++;
				path[i] = item->path[i] * 256; i++;
				path[i] = item->path[i] * 256; i++;
				path[i] = item->path[i] * 256; i++;
				path[i] = item->path[i] * 256; i++;
				break;
			default:
				assert(0);
			}
		}
		context->b = (char *) context->b + object->size;
		break;
	case pencil_SPRITE:
		object->type = drawfile_TYPE_SPRITE;
		object->size = 24 + ((const osspriteop_header *)
				item->sprite)->size;
		object->data.sprite.bbox.x0 = item->bbox.x0;
		object->data.sprite.bbox.y0 = item->bbox.y0;
		object->data.sprite.bbox.x1 = item->bbox.x1;
		object->data.sprite.bbox.y1 = item->bbox.y1;
		memcpy(&object->data.sprite.header, item->sprite,
				object->size - 24);
		context->b = (char *) context->b + object->size;
		break;
	default:
		assert(0);
	}

	for (child = item->children; child; child = child->next) {
		pencil_save_pass2(context, child, depth + 1);
		if (context->code != pencil_OK)
			return;
	}

	if (group)
		object->size = (char *) context->b - (char *) object;
}


void pencil_save_pass2_text_callback(void *c,
		const char *font_name, unsigned int font_size,
		const char *s8, unsigned short *s16, unsigned int n,
		int x, int y)
{
	struct pencil_save_context *context = c;
	drawfile_object *object = (drawfile_object *) context->b;
	unsigned int i;

	assert(s8 || s16);

	/* find font index */
	for (i = 0; i != context->font_count &&
			strcmp(context->font_list[i], font_name) != 0; i++)
		;
	assert(i != context->font_count);

	object->type = drawfile_TYPE_TRFM_TEXT;
	object->data.trfm_text.bbox.x0 = context->item->bbox.x0;
	object->data.trfm_text.bbox.y0 = context->item->bbox.y0;
	object->data.trfm_text.bbox.x1 = context->item->bbox.x1;
	object->data.trfm_text.bbox.y1 = context->item->bbox.y1;
	object->data.trfm_text.trfm.entries[0][0] = 0x10000;
	object->data.trfm_text.trfm.entries[0][1] = 0;
	object->data.trfm_text.trfm.entries[1][0] = 0;
	object->data.trfm_text.trfm.entries[1][1] = 0x10000;
	object->data.trfm_text.trfm.entries[2][0] = 0;
	object->data.trfm_text.trfm.entries[2][1] = 0;
	object->data.trfm_text.flags = drawfile_TEXT_KERN;
	object->data.trfm_text.fill = context->item->fill_colour;
	object->data.trfm_text.bg_hint = os_COLOUR_WHITE;
	object->data.trfm_text.style.font_index = i + 1;
	object->data.trfm_text.xsize = font_size * 40;
	object->data.trfm_text.ysize = font_size * 40;
	object->data.trfm_text.base.x = x * 256;
	object->data.trfm_text.base.y = y * 256;

	if (s8) {
		strncpy(object->data.trfm_text.text, s8, n);
		object->size = 24 + 56 + ((n + 4) & ~3);
	} else {
		char *z = object->data.trfm_text.text;
		unsigned int utf8_length = 0;
		for (i = 0; i != n; i++) {
			if (s16[i] < 0x80) {
				*z++ = s16[i];
				utf8_length += 1;
			} else if (s16[i] < 0x800) {
				*z++ = 0xc0 | ((s16[i] >> 6) & 0x1f);
				*z++ = 0x80 | (s16[i] & 0x3f);
				utf8_length += 2;
			} else {
				*z++ = 0xe0 | (s16[i] >> 12);
				*z++ = 0x80 | ((s16[i] >> 6) & 0x3f);
				*z++ = 0x80 | (s16[i] & 0x3f);
				utf8_length += 3;
			}
		}
		object->size = 24 + 56 + ((utf8_length + 4) & ~3);
	}

	context->b = (char *) context->b + object->size;
}
