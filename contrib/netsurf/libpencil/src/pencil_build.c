/*
 * This file is part of Pencil
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2005 James Bursa <james@semichrome.net>
 */

#define _GNU_SOURCE  /* for strndup */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "pencil_internal.h"


static struct pencil_item *pencil_new_item(pencil_item_type type);
static void pencil_append_child(struct pencil_item *group,
		struct pencil_item *child);
static void pencil_free_item(struct pencil_item *item);
static void pencil_dump_item(struct pencil_item *item, unsigned int depth);


struct pencil_diagram *pencil_create(void)
{
	struct pencil_diagram *diagram;
	struct pencil_item *root_group;

	diagram = malloc(sizeof *diagram);
	root_group = pencil_new_item(pencil_GROUP);
	if (!diagram || !root_group) {
		free(root_group);
		free(diagram);
		return 0;
	}

	root_group->group_name = strdup("root group");
	if (!root_group->group_name) {
		free(root_group);
		free(diagram);
		return 0;
	}

	diagram->root = root_group;
	diagram->current_group = root_group;

	return diagram;
}


pencil_code pencil_text(struct pencil_diagram *diagram,
		int x, int y,
		const char *font_family, rufl_style font_style,
		unsigned int font_size,
		const char *string, size_t length,
		pencil_colour colour)
{
	struct pencil_item *item;

	item = pencil_new_item(pencil_TEXT);
	if (!item)
		return pencil_OUT_OF_MEMORY;

	item->x = x;
	item->y = y;
	item->fill_colour = colour;
	item->font_family = font_family;
	item->font_style = font_style;
	item->font_size = font_size;
	item->text = strndup(string, length);
	if (!item->text) {
		free(item);
		return pencil_OUT_OF_MEMORY;
	}

	pencil_append_child(diagram->current_group, item);

	return pencil_OK;
}


pencil_code pencil_path(struct pencil_diagram *diagram,
		const int *path, unsigned int n,
		pencil_colour fill_colour, pencil_colour outline_colour,
		int thickness, pencil_join join,
		pencil_cap start_cap, pencil_cap end_cap,
		int cap_width, int cap_length,
		bool even_odd, pencil_pattern pattern)

{
	struct pencil_item *item;

	item = pencil_new_item(pencil_PATH);
	if (!item)
		return pencil_OUT_OF_MEMORY;

	item->fill_colour = fill_colour;
	item->outline_colour = outline_colour;
	item->path = malloc(sizeof path[0] * n);
	if (!item->path) {
		free(item);
		return pencil_OUT_OF_MEMORY;
	}
	memcpy(item->path, path, sizeof path[0] * n);
	item->path_size = n;
	item->thickness = thickness;
	item->join = join;
	item->start_cap = start_cap;
	item->end_cap = end_cap;
	item->cap_width = cap_width;
	item->cap_length = cap_length;
	item->even_odd = even_odd;
	item->pattern = pattern;

	pencil_append_child(diagram->current_group, item);

	return pencil_OK;
}


pencil_code pencil_sprite(struct pencil_diagram *diagram,
		int x, int y, int width, int height,
		const char *sprite)
{
	struct pencil_item *item;

	item = pencil_new_item(pencil_SPRITE);
	if (!item)
		return pencil_OUT_OF_MEMORY;

	item->x = x;
	item->y = y;
	item->width = width;
	item->height = height;
	item->sprite = sprite;

	pencil_append_child(diagram->current_group, item);

	return pencil_OK;
}


pencil_code pencil_group_start(struct pencil_diagram *diagram,
		const char *name)
{
	struct pencil_item *item;

	item = pencil_new_item(pencil_GROUP);
	if (!item)
		return pencil_OUT_OF_MEMORY;

	item->group_name = strdup(name);
	if (!item->group_name) {
		free(item);
		return pencil_OUT_OF_MEMORY;
	}

	pencil_append_child(diagram->current_group, item);

	diagram->current_group = item;

	return pencil_OK;
}


pencil_code pencil_group_end(struct pencil_diagram *diagram)
{
	diagram->current_group = diagram->current_group->parent;

	return pencil_OK;
}


struct pencil_item *pencil_new_item(pencil_item_type type)
{
	struct pencil_item *item;

	item = malloc(sizeof *item);
	if (!item)
		return 0;

	item->type = type;
	item->group_name = 0;
	item->text = 0;
	item->path = 0;
	item->parent = item->next = item->children = item->last = 0;

	return item;
}


void pencil_append_child(struct pencil_item *group,
		struct pencil_item *child)
{
	child->parent = group;
	if (group->children) {
		assert(group->last);
		group->last->next = child;
	} else {
		group->children = child;
	}
	group->last = child;
}


void pencil_free(struct pencil_diagram *diagram)
{
	pencil_free_item(diagram->root);
	free(diagram);
}


void pencil_free_item(struct pencil_item *item)
{
	for (struct pencil_item *child = item->children; child;
			child = child->next)
		pencil_free_item(child);
	free(item->group_name);
	free(item->text);
	free(item->path);
	free(item);
}


void pencil_dump(struct pencil_diagram *diagram)
{
	printf("diagram %p: current group %p\n",
			(void *) diagram, (void *) diagram->current_group);
	pencil_dump_item(diagram->root, 0);
}


void pencil_dump_item(struct pencil_item *item, unsigned int depth)
{
	for (unsigned int i = 0; i != depth; i++)
		printf("  ");

	printf("%p ", (void *) item);
	switch (item->type) {
	case pencil_GROUP:
		printf("GROUP");
		break;
	case pencil_TEXT:
		printf("TEXT (%i %i) font %s %i %i, text \"%s\"",
				item->x, item->y,
				item->font_family, item->font_style,
				item->font_size, item->text);
		break;
	case pencil_PATH:
		printf("PATH (");
		for (unsigned int i = 0; i != item->path_size; i++)
			printf("%i ", item->path[i]);
		printf(") thickness %i, join %i, caps %i %i %i %i, ",
				item->thickness, item->join,
				item->start_cap, item->end_cap,
				item->cap_width, item->cap_length);
		if (item->even_odd)
			printf("even-odd, ");
		printf("pattern %i", item->pattern);
		break;
	case pencil_SPRITE:
		printf("SPRITE (%i %i) (%i x %i)\n", item->x, item->y,
				item->width, item->height);
		break;
	default:
		printf("UNKNOWN");
	}
	printf("\n");

	for (struct pencil_item *child = item->children; child;
			child = child->next)
		pencil_dump_item(child, depth + 1);
}
